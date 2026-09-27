/* SPI buses come from the board's devicetree: /zephyr,user { hal-spis =
 * <&spi2>; }. Zephyr's driver model covers both the SPI and SPIM
 * peripherals transparently, so this just uses the generic API; unlike
 * hal_uart there is no odd baud rate to fight, so no register-level work
 * is needed here. */
#include "hal_spi.h"

#include <zephyr/devicetree.h>
#include <zephyr/drivers/spi.h>

#define USER_NODE          DT_PATH( zephyr_user )
#define HAL_SPI_COUNT      DT_PROP_LEN( USER_NODE, hal_spis )
#define SPI_NODE( idx )    DT_PHANDLE_BY_IDX( USER_NODE, hal_spis, idx )
#define SPI_INIT( idx, _ ) { .dev = DEVICE_DT_GET( SPI_NODE( idx ) ) }

struct spi_state
{
    const struct device * dev;
    struct spi_config cfg;
};

static struct spi_state spis[] = {
    LISTIFY( HAL_SPI_COUNT, SPI_INIT, (, ) )
};

static spi_operation_t mode_to_operation( hal_spi_mode_t mode )
{
    spi_operation_t op =
        SPI_OP_MODE_MASTER | SPI_WORD_SET( 8 ) | SPI_TRANSFER_MSB;

    switch ( mode )
    {
        case HAL_SPI_MODE0:
            break;
        case HAL_SPI_MODE1:
            op |= SPI_MODE_CPHA;
            break;
        case HAL_SPI_MODE2:
            op |= SPI_MODE_CPOL;
            break;
        case HAL_SPI_MODE3:
            op |= SPI_MODE_CPOL | SPI_MODE_CPHA;
            break;
    }
    return op;
}

hal_status_t hal_spi_open(
    hal_spi_id_t id,
    const hal_spi_cfg_t * cfg
)
{
    hal_status_t status = HAL_ERR_INVALID;

    if ( ( id < HAL_SPI_COUNT ) && ( cfg != NULL )
         && ( cfg->freq_hz > 0u ) )
    {
        struct spi_state * s = &spis[id];

        if ( !device_is_ready( s->dev ) )
        {
            status = HAL_ERR_IO;
        }
        else
        {
            /* cs left zero-initialized: no automatic chip select. The
             * config is otherwise built fresh per transfer, so there is
             * nothing else to validate against the hardware up front;
             * an unsupportable frequency/mode would only show up as a
             * failed transfer, same as hal_uart_write's blocking calls
             * that cannot report per-call failure either. */
            s->cfg = ( struct spi_config ){
                .frequency = cfg->freq_hz,
                .operation = mode_to_operation( cfg->mode ),
                .slave     = 0u,
            };
            status = HAL_OK;
        }
    }
    return status;
}

hal_status_t hal_spi_transfer(
    hal_spi_id_t id,
    const uint8_t * tx,
    uint8_t * rx,
    size_t len
)
{
    hal_status_t status = HAL_ERR_INVALID;

    if ( ( id < HAL_SPI_COUNT ) && ( len > 0u )
         && ( ( tx != NULL ) || ( rx != NULL ) ) )
    {
        struct spi_state * s        = &spis[id];
        const struct spi_buf tx_buf = { .buf = ( void * )tx, .len = len };
        const struct spi_buf_set tx_set = {
            .buffers = &tx_buf, .count = 1
        };
        const struct spi_buf rx_buf     = { .buf = rx, .len = len };
        const struct spi_buf_set rx_set = {
            .buffers = &rx_buf, .count = 1
        };

        int rc = spi_transceive(
            s->dev,
            &s->cfg,
            ( tx != NULL ) ? &tx_set : NULL,
            ( rx != NULL ) ? &rx_set : NULL
        );

        status = ( rc == 0 ) ? HAL_OK : HAL_ERR_IO;
    }
    return status;
}
