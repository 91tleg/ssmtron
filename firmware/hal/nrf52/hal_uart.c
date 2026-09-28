/* UARTs come from the board's /zephyr,user hal-uarts property.
 * Zephyr rejects nonstandard rates such as 1953 baud, so configure a
 * supported rate first, then set BAUDRATE directly. */
#include "hal_uart.h"

#include <hal/nrf_uarte.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/sys/atomic.h>
#include <zephyr/sys/ring_buffer.h>

#include "uarte_baud.h"

#if !defined( CONFIG_UART_INTERRUPT_DRIVEN )                                   \
    || !defined( CONFIG_UART_USE_RUNTIME_CONFIGURE )
#error                                                                         \
    "hal_uart needs CONFIG_UART_INTERRUPT_DRIVEN and CONFIG_UART_USE_RUNTIME_CONFIGURE"
#endif

#define USER_NODE        DT_PATH( zephyr_user )
#define HAL_UART_COUNT   DT_PROP_LEN( USER_NODE, hal_uarts )
#define UART_NODE( idx ) DT_PHANDLE_BY_IDX( USER_NODE, hal_uarts, idx )
#define UART_INIT( idx, _ )                                                    \
    { .dev  = DEVICE_DT_GET( UART_NODE( idx ) ),                               \
      .regs = ( NRF_UARTE_Type * )DT_REG_ADDR( UART_NODE( idx ) ) }

/* The ring holds 2-byte entries: { byte, is_error }. */
#define RX_ENTRIES  ( 64u )
#define RX_BUF_SIZE ( RX_ENTRIES * 2u )
#define STOCK_BAUD                                                             \
    ( 9600u ) /* any rate Zephyr accepts; overwritten below                    \
               */

struct uart_state
{
    const struct device * dev;
    NRF_UARTE_Type * regs;
    struct ring_buf rx;
    uint8_t rx_storage[RX_BUF_SIZE];
    atomic_t lost; /* set by the ISR when the ring was full */
};

static struct uart_state uarts[] = {
    LISTIFY( HAL_UART_COUNT, UART_INIT, (, ) )
};

static void rx_push(
    struct uart_state * u,
    uint8_t byte,
    bool is_error
)
{
    const uint8_t entry[2] = { byte, is_error ? 1u : 0u };

    if ( ring_buf_space_get( &u->rx ) >= sizeof( entry ) )
    {
        ( void )ring_buf_put( &u->rx, entry, sizeof( entry ) );
    }
    else
    {
        /* reported as an error, in hal_uart_read */
        ( void )atomic_set( &u->lost, 1 );
    }
}

static void uart_isr(
    const struct device * dev,
    void * user
)
{
    struct uart_state * u = user;
    uint8_t chunk[16];

    while ( uart_irq_update( dev ) && uart_irq_is_pending( dev ) )
    {
        if ( uart_irq_rx_ready( dev ) )
        {
            int n = uart_fifo_read( dev, chunk, sizeof( chunk ) );

            for ( int i = 0; i < n; i++ )
            {
                rx_push( u, chunk[i], false );
            }
        }
        /* The error flags are not tied to one byte, so the marker goes
         * after the bytes read in this pass. */
        if ( uart_err_check( dev ) > 0 )
        {
            rx_push( u, 0, true );
        }
    }
}

static int parity_to_zephyr( hal_uart_parity_t p )
{
    int z = -1;

    switch ( p )
    {
        case HAL_UART_PARITY_NONE:
            z = UART_CFG_PARITY_NONE;
            break;
        case HAL_UART_PARITY_EVEN:
            z = UART_CFG_PARITY_EVEN;
            break;
        case HAL_UART_PARITY_ODD:
            z = UART_CFG_PARITY_ODD;
            break;
    }
    return z;
}

static hal_status_t start_uart(
    struct uart_state * u,
    uint8_t parity,
    uint32_t baud_reg
)
{
    /* Every "unsupported" case (baud, parity, framing) was already ruled
     * out by the caller before this runs, so a failure here is the
     * hardware refusing, not a bad config. */
    hal_status_t status           = HAL_ERR_IO;
    const struct uart_config zcfg = {
        .baudrate  = STOCK_BAUD,
        .parity    = parity,
        .stop_bits = UART_CFG_STOP_BITS_1,
        .data_bits = UART_CFG_DATA_BITS_8,
        .flow_ctrl = UART_CFG_FLOW_CTRL_NONE,
    };

    if ( uart_configure( u->dev, &zcfg ) == 0 )
    {
        /* RX is not started yet, so the peripheral is idle. */
        nrf_uarte_disable( u->regs );
        nrf_uarte_baudrate_set( u->regs, ( nrf_uarte_baudrate_t )baud_reg );
        nrf_uarte_enable( u->regs );

        ring_buf_init( &u->rx, sizeof( u->rx_storage ), u->rx_storage );

        /* -ENOSYS/-ENOTSUP here means this driver instance never calls
         * uart_isr, so RX and error detection would silently do nothing;
         * that must not come back as HAL_OK. */
        if ( uart_irq_callback_user_data_set( u->dev, uart_isr, u ) == 0 )
        {
            ( void )atomic_set( &u->lost, 0 );
            uart_irq_err_enable( u->dev );
            uart_irq_rx_enable( u->dev );
            status = HAL_OK;
        }
        else
        {
            nrf_uarte_disable( u->regs ); /* leave it as we found it */
        }
    }
    return status;
}

hal_status_t hal_uart_open(
    hal_uart_id_t id,
    const hal_uart_cfg_t * cfg
)
{
    hal_status_t status = HAL_ERR_INVALID;

    if ( ( id < HAL_UART_COUNT ) && ( cfg != NULL ) )
    {
        struct uart_state * u = &uarts[id];
        int parity            = parity_to_zephyr( cfg->parity );
        uint32_t baud_reg     = 0;
        bool supported        = ( parity >= 0 ) && ( cfg->data_bits == 8u )
                         && ( cfg->stop_bits == 1u )
                         && uarte_baud_to_reg( cfg->baud, &baud_reg );

        if ( !supported )
        {
            status = HAL_ERR_UNSUPPORTED;
        }
        else if ( !device_is_ready( u->dev ) )
        {
            status = HAL_ERR_IO;
        }
        else
        {
            status = start_uart( u, ( uint8_t )parity, baud_reg );
        }
    }
    return status;
}

size_t hal_uart_write(
    hal_uart_id_t id,
    const uint8_t * buf,
    size_t len
)
{
    size_t written = 0;

    if ( ( id < HAL_UART_COUNT ) && ( buf != NULL ) )
    {
        for ( ; written < len; written++ )
        {
            /* blocks ~5 ms/byte at 1953 baud; the driver returns void, so
             * there is nothing to check per byte. */
            uart_poll_out( uarts[id].dev, buf[written] );
        }
    }
    return written;
}

hal_uart_rx_t hal_uart_read(
    hal_uart_id_t id,
    uint8_t * byte
)
{
    hal_uart_rx_t result = HAL_UART_RX_EMPTY;
    uint8_t entry[2];

    if ( ( id < HAL_UART_COUNT ) && ( byte != NULL ) )
    {
        struct uart_state * u = &uarts[id];

        if ( atomic_cas( &u->lost, 1, 0 ) )
        {
            result = HAL_UART_RX_ERROR;
        }
        else if ( ring_buf_get( &u->rx, entry, sizeof( entry ) )
                  == sizeof( entry ) )
        {
            if ( entry[1] != 0u )
            {
                result = HAL_UART_RX_ERROR;
            }
            else
            {
                *byte  = entry[0];
                result = HAL_UART_RX_BYTE;
            }
        }
        else
        {
            /* Nothing pending. */
            result = HAL_UART_RX_EMPTY;
        }
    }
    return result;
}
