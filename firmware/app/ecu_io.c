#include "app/ecu_io.h"

#include "lib/ecu/ecu.h"

#define RX_BURST ( 64u ) /* per poll, so a flood cannot starve the loop */
#define TX_MAX   ( 8u )

static hal_uart_id_t uart_id;

static hal_uart_parity_t to_hal_parity( ecu_parity_t p )
{
    hal_uart_parity_t hp = HAL_UART_PARITY_NONE;

    switch ( p )
    {
        case ECU_PARITY_NONE:
            hp = HAL_UART_PARITY_NONE;
            break;
        case ECU_PARITY_EVEN:
            hp = HAL_UART_PARITY_EVEN;
            break;
        case ECU_PARITY_ODD:
            hp = HAL_UART_PARITY_ODD;
            break;
        default:
            break;
    }
    return hp;
}

/* The settings the protocol asks for. */
static hal_uart_cfg_t uart_config( void )
{
    const ecu_link_cfg_t * link = ecu_link_config();
    hal_uart_cfg_t cfg          = {
                 .baud      = link->baud,
                 .data_bits = link->data_bits,
                 .parity    = to_hal_parity( link->parity ),
                 .stop_bits = link->stop_bits,
    };

#if defined( CONFIG_APP_DEV_ECU_ON_USB )
    /* See firmware/dev/: the USB serial bridge carries plain 8N1. */
    cfg.baud   = CONFIG_APP_DEV_ECU_BAUD;
    cfg.parity = HAL_UART_PARITY_NONE;
#endif
    return cfg;
}

hal_status_t ecu_io_init( hal_uart_id_t uart )
{
    const hal_uart_cfg_t cfg = uart_config();
    hal_status_t status      = hal_uart_open( uart, &cfg );

    if ( status == HAL_OK )
    {
        uart_id = uart;
        ecu_init();
    }
    return status;
}

void ecu_io_poll( uint32_t now_ms )
{
    uint8_t byte = 0;
    uint8_t tx[TX_MAX];
    hal_uart_rx_t rx = HAL_UART_RX_BYTE;
    size_t n;
#if defined( CONFIG_APP_DEV_ECU_ON_USB )
    /* Bench counters, read with a debugger. */
    static volatile uint32_t dev_rx_bytes;
    static volatile uint32_t dev_rx_errors;
#endif

    ecu_tick( now_ms );
    for ( unsigned i = 0; ( i < RX_BURST ) && ( rx != HAL_UART_RX_EMPTY ); i++ )
    {
        rx = hal_uart_read( uart_id, &byte );
        if ( rx == HAL_UART_RX_BYTE )
        {
#if defined( CONFIG_APP_DEV_ECU_ON_USB )
            dev_rx_bytes++;
#endif
            ecu_feed( byte );
        }
        else if ( rx == HAL_UART_RX_ERROR )
        {
#if defined( CONFIG_APP_DEV_ECU_ON_USB )
            dev_rx_errors++;
#endif
            ecu_feed_error();
        }
        else
        {
            /* HAL_UART_RX_EMPTY: nothing to do, the loop condition ends
             * it. */
        }
    }

    n = ecu_next_tx( tx, sizeof( tx ) );
    if ( n > 0u )
    {
        /* Nothing to retry on: the next poll's ecu_next_tx call is the
         * retry, same as every other tx path in this file. */
        ( void )hal_uart_write( uart_id, tx, n );
    }
}
