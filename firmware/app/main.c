#include "bsp.h"
#include "ecu_io.h"
#include "fw_info.h"
#include "gauge.h"
#include "hal_ble.h"
#include "hal_dfu.h"
#include "hal_log.h"
#include "hal_nvm.h"
#include "hal_time.h"
#include "hal_wdt.h"

#define BLE_NAME "SSM1 Gauge"

/* Bench-only fallback for a unit nothing has provisioned yet (see
 * firmware/provision/). The same passkey in every unit is only good
 * enough for the bench; a real unit's passkey lives in NVM instead,
 * under this key, written once at manufacture time. */
#define BLE_PASSKEY_DEFAULT ( 123456u )
#define BLE_PASSKEY_KEY     "ble_passkey"

/* HAL_ERR_UNSUPPORTED/HAL_ERR_IO (no storage, or never provisioned) both
 * fall back to the bench default; only a short, valid stored value is
 * trusted. */
static uint32_t ble_passkey( void )
{
    uint32_t passkey = BLE_PASSKEY_DEFAULT;
    size_t len;

    if ( ( hal_nvm_init() == HAL_OK )
         && ( hal_nvm_load( BLE_PASSKEY_KEY, &passkey, sizeof( passkey ), &len )
              == HAL_OK )
         && ( len != sizeof( passkey ) ) )
    {
        passkey = BLE_PASSKEY_DEFAULT;
    }
    return passkey;
}

/* A new firmware image is confirmed once it has run this long; if it
 * crashes sooner the bootloader goes back to the old one. */
#define CONFIRM_AFTER_MS ( 10000u )

/* Generous margin over today's worst-case boot path (ECU UART open, BT
 * stack bring-up, one display_init attempt): comfortably covers that
 * with room to grow if display bring-up later gets bounded retries,
 * without this needing to change. Not sized to steady-state loop
 * timing -- once past boot, hal_wdt_feed runs every ~1 ms iteration. */
#define WDT_TIMEOUT_MS ( 4000u )

/* The loop has nothing blocking in it, so without a yield it would spin at
 * 100% CPU. This is well under one byte time (~5.6 ms at 1953 baud), so
 * it costs no responsiveness. */
#define LOOP_DELAY_MS ( 1u )

int main( void )
{
    const hal_ble_cfg_t ble           = { BLE_NAME, ble_passkey() };
    const display_pins_t display_pins = {
        BSP_SPI_DISPLAY,
        BSP_GPIO_DISPLAY_CS,
        BSP_GPIO_DISPLAY_DC,
        BSP_GPIO_DISPLAY_RESET,
        0u,
        false
    };
    hal_status_t status;
    bool confirmed = false;

    /* Armed before anything that could hang, including the boot log line
     * itself. A field unit has no other way back from a wedged HAL call.
     */
    if ( hal_wdt_start( WDT_TIMEOUT_MS ) != HAL_OK )
    {
        hal_log_write( "wdt not armed" );
    }

    /* First thing printed, always: whatever else does or doesn't work
     * below, whoever is looking at the console knows what is running. */
    hal_log_write( fw_info.version );

    status = ecu_io_init( BSP_UART_ECU );
    if ( status == HAL_OK )
    {
        /* v1: BLE is for firmware updates only, never app data -- see
         * hal_dfu_confirm below and mcumgr's own BT transport, which
         * registers itself independently of anything here. hal_ble_open
         * still brings up pairing/advertising (DFU needs a live,
         * connectable link), but nothing ever calls hal_ble_write; a
         * failure here is not fatal either way. */
        status = hal_ble_open( &ble );
    }
    /* The panel showing nothing is not fatal either: the ECU link still
     * works without it. */
    ( void )gauge_init( &display_pins );
    hal_wdt_feed();

    for ( ;; )
    {
        uint32_t now = hal_millis();

        ecu_io_poll( now );
        gauge_poll();
        if ( !confirmed && ( now >= CONFIRM_AFTER_MS ) )
        {
            status    = hal_dfu_confirm();
            confirmed = true;
        }

        hal_wdt_feed();
        hal_delay_ms( LOOP_DELAY_MS );
    }

    return ( int )status;
}
