#include "ecu_io.h"

#include "ecu.h"

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
    }
    return hp;
}

hal_status_t ecu_io_init( hal_uart_id_t uart )
{
    const ecu_link_cfg_t * link = ecu_link_config();
    const hal_uart_cfg_t cfg    = {
           .baud      = link->baud,
           .data_bits = link->data_bits,
           .parity    = to_hal_parity( link->parity ),
           .stop_bits = link->stop_bits,
    };
    hal_status_t status = hal_uart_open( uart, &cfg );

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

    ecu_tick( now_ms );
    for ( unsigned i = 0; ( i < RX_BURST ) && ( rx != HAL_UART_RX_EMPTY ); i++ )
    {
        rx = hal_uart_read( uart_id, &byte );
        if ( rx == HAL_UART_RX_BYTE )
        {
            ecu_feed( byte );
        }
        else if ( rx == HAL_UART_RX_ERROR )
        {
            ecu_feed_error();
        }
    }

    n = ecu_next_tx( tx, sizeof( tx ) );
    if ( n > 0u )
    {
        hal_uart_write( uart_id, tx, n );
    }
}
