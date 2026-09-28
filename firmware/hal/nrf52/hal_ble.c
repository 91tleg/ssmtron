/* Nordic UART Service transport. Stack callbacks share ring buffers and
 * atomics with the app thread. Built when CONFIG_BT_NUS is enabled. */
#include "hal_ble.h"

#include <string.h>
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/conn.h>
#include <bluetooth/services/nus.h>
#include <zephyr/kernel.h>
#include <zephyr/settings/settings.h>
#include <zephyr/sys/atomic.h>
#include <zephyr/sys/ring_buffer.h>

#if !defined( CONFIG_BT_APP_PASSKEY ) || !defined( CONFIG_BT_NUS_AUTHEN )
#error                                                                         \
    "hal_ble needs CONFIG_BT_SMP, CONFIG_BT_APP_PASSKEY and CONFIG_BT_NUS_AUTHEN"
#endif

#define TX_SIZE  ( 512 )
#define RX_SIZE  ( 256 )
#define EVT_SIZE ( 8 )

RING_BUF_DECLARE(
    tx_ring,
    TX_SIZE
);
RING_BUF_DECLARE(
    rx_ring,
    RX_SIZE
);
RING_BUF_DECLARE(
    evt_ring,
    EVT_SIZE
);

static struct k_work tx_work;
static struct k_work_delayable adv_retry_work;
static struct bt_conn * conn;
static atomic_t linked;     /* a peer is connected */
static atomic_t subscribed; /* and has enabled notifications */
static bool opened;
static uint32_t passkey;

static struct bt_data adv[2];
static const struct bt_data scan_rsp[] = {
    BT_DATA_BYTES( BT_DATA_UUID128_ALL, BT_UUID_NUS_VAL ),
};

static void push_event( hal_ble_evt_t evt )
{
    const uint8_t e = ( uint8_t )evt;

    /* Event loss means the app has not polled for several connection cycles. */
    ( void )ring_buf_put( &evt_ring, &e, 1 );
}

#define ADV_RETRY_MS 500u

static void advertise( void )
{
    /* Retry when the controller temporarily lacks resources. */
    if ( bt_le_adv_start(
             BT_LE_ADV_CONN_FAST_2,
             adv,
             ARRAY_SIZE( adv ),
             scan_rsp,
             ARRAY_SIZE( scan_rsp )
         )
         != 0 )
    {
        ( void )k_work_schedule( &adv_retry_work, K_MSEC( ADV_RETRY_MS ) );
    }
}

static void adv_retry_handler( struct k_work * work )
{
    ARG_UNUSED( work );
    advertise();
}

/* Runs on the system work queue, the only consumer of tx_ring. */
static void tx_handler( struct k_work * work )
{
    bool go = true;

    ARG_UNUSED( work );
    while ( go )
    {
        uint8_t * data;
        uint32_t claimed = ring_buf_get_claim( &tx_ring, &data, TX_SIZE );

        if ( claimed == 0u )
        {
            go = false;
        }
        else if ( !atomic_get( &linked ) || !atomic_get( &subscribed ) )
        {
            ( void )ring_buf_get_finish(
                &tx_ring, claimed
            ); /* nobody to send to */
        }
        else
        {
            struct bt_conn * c = conn;
            /* bt_nus_get_mtu is gatt_mtu - 3; clamp instead of trusting
             * that gatt_mtu is always >= 3, so a bad read never turns into
             * a huge unsigned value here. */
            uint32_t mtu = bt_nus_get_mtu( c );
            uint32_t len;

            if ( mtu > TX_SIZE )
            {
                mtu = TX_SIZE;
            }
            len = ( claimed < mtu ) ? claimed : mtu;

            if ( bt_nus_send( c, data, ( uint16_t )len ) == 0 )
            {
                ( void )ring_buf_get_finish( &tx_ring, len );
            }
            else
            {
                /* Stack buffers are full: keep the data. The sent callback
                 * calls us again when there is room. */
                ( void )ring_buf_get_finish( &tx_ring, 0 );
                go = false;
            }
        }
    }
}

static void on_connected(
    struct bt_conn * c,
    uint8_t err
)
{
    struct bt_conn * ref = ( err == 0u ) ? bt_conn_ref( c ) : NULL;

    if ( ref != NULL )
    {
        conn = ref;
        ( void )atomic_set( &linked, 1 );
        push_event( HAL_BLE_EVT_CONNECTED );
    }
    else
    {
        /* Either the stack reported an error, or bt_conn_ref itself
         * failed; either way there is no usable connection to keep. */
        advertise();
    }
}

static void on_disconnected(
    struct bt_conn * c,
    uint8_t reason
)
{
    ARG_UNUSED( reason );
    ( void )atomic_set( &linked, 0 );
    ( void )atomic_set( &subscribed, 0 );
    bt_conn_unref( c );
    conn = NULL;
    push_event( HAL_BLE_EVT_DISCONNECTED );
    ( void )k_work_submit( &tx_work ); /* discards anything still queued */
}

BT_CONN_CB_DEFINE( conn_callbacks ) = {
    .connected    = on_connected,
    .disconnected = on_disconnected,
    .recycled     = advertise,
};

static void on_received(
    struct bt_conn * c,
    const uint8_t * const data,
    uint16_t len
)
{
    ARG_UNUSED( c );
    if ( data != NULL )
    {
        /* drops what does not fit */
        ( void )ring_buf_put( &rx_ring, data, len );
    }
}

static void on_sent( struct bt_conn * c )
{
    ARG_UNUSED( c );
    ( void )k_work_submit( &tx_work );
}

static void on_send_enabled( enum bt_nus_send_status status )
{
    ( void )atomic_set(
        &subscribed, ( status == BT_NUS_SEND_STATUS_ENABLED ) ? 1 : 0
    );
    ( void )k_work_submit( &tx_work );
}

/* Pairing: we show a fixed passkey (nothing to display yet), the peer types
 * it. Having app_passkey makes us display-capable, which is what turns
 * pairing into the authenticated (MITM-protected) kind. */
static uint32_t on_app_passkey( struct bt_conn * c )
{
    ARG_UNUSED( c );
    return passkey;
}

static void on_passkey_display(
    struct bt_conn * c,
    unsigned int key
)
{
    ARG_UNUSED( c );
    ARG_UNUSED( key );
}

static void on_pairing_cancel( struct bt_conn * c )
{
    ARG_UNUSED( c );
}

static const struct bt_conn_auth_cb auth_callbacks = {
    .app_passkey     = on_app_passkey,
    .passkey_display = on_passkey_display,
    .cancel          = on_pairing_cancel,
};

static struct bt_nus_cb nus_callbacks = {
    .received     = on_received,
    .sent         = on_sent,
    .send_enabled = on_send_enabled,
};

hal_status_t hal_ble_open( const hal_ble_cfg_t * cfg )
{
    hal_status_t status = HAL_ERR_INVALID;
    size_t name_len =
        ( cfg != NULL && cfg->name != NULL ) ? strlen( cfg->name ) : 0u;

    if ( !opened && ( name_len > 0u ) && ( name_len <= HAL_BLE_NAME_MAX ) )
    {
        k_work_init( &tx_work, tx_handler );
        k_work_init_delayable( &adv_retry_work, adv_retry_handler );
        adv[0] = ( struct bt_data )BT_DATA_BYTES(
            BT_DATA_FLAGS, ( BT_LE_AD_GENERAL | BT_LE_AD_NO_BREDR )
        );
        adv[1] = ( struct bt_data )BT_DATA(
            BT_DATA_NAME_COMPLETE, cfg->name, name_len
        );

        if ( ( bt_conn_auth_cb_register( &auth_callbacks ) != 0 )
             || ( bt_enable( NULL ) != 0 )
             || ( IS_ENABLED( CONFIG_BT_SETTINGS ) && ( settings_load() != 0 ) )
             || ( bt_set_name( cfg->name ) != 0 )
             || ( bt_nus_init( &nus_callbacks ) != 0 ) )
        {
            status = HAL_ERR_IO;
        }
        else if ( bt_le_adv_start(
                      BT_LE_ADV_CONN_FAST_2,
                      adv,
                      ARRAY_SIZE( adv ),
                      scan_rsp,
                      ARRAY_SIZE( scan_rsp )
                  )
                  != 0 )
        {
            status = HAL_ERR_IO;
        }
        else
        {
            opened = true;
            status = HAL_OK;
        }
    }
    return status;
}

hal_ble_evt_t hal_ble_poll_event( void )
{
    uint8_t e = ( uint8_t )HAL_BLE_EVT_NONE;

    ( void )ring_buf_get( &evt_ring, &e, 1 );
    return ( hal_ble_evt_t )e;
}

bool hal_ble_ready( void )
{
    return atomic_get( &linked ) && atomic_get( &subscribed );
}

size_t hal_ble_write(
    const uint8_t * buf,
    size_t len
)
{
    size_t accepted = 0;

    if ( hal_ble_ready() && ( buf != NULL ) && ( len > 0u ) )
    {
        accepted = ring_buf_put( &tx_ring, buf, ( uint32_t )len );
        if ( accepted > 0u )
        {
            ( void )k_work_submit( &tx_work );
        }
    }
    return accepted;
}

size_t hal_ble_read(
    uint8_t * buf,
    size_t max
)
{
    return ( buf != NULL ) ? ring_buf_get( &rx_ring, buf, ( uint32_t )max )
                           : 0u;
}
