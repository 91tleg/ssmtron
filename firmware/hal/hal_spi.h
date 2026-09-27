/* Ids index the SPI buses the board declares; pin routing is the board's
 * job. Chip select is deliberately not handled here: it is an ordinary
 * GPIO the device driver toggles itself (see hal_gpio.h), so one bus can
 * be shared by several devices without the HAL knowing any of them exist.
 */
#ifndef HAL_SPI_H
#define HAL_SPI_H

#include <stddef.h>
#include <stdint.h>

#include "hal_status.h"

typedef uint8_t hal_spi_id_t;

/* Standard SPI mode numbering (clock polarity/phase). */
typedef enum
{
    HAL_SPI_MODE0,
    HAL_SPI_MODE1,
    HAL_SPI_MODE2,
    HAL_SPI_MODE3
} hal_spi_mode_t;

typedef struct
{
    uint32_t freq_hz;
    hal_spi_mode_t mode;
} hal_spi_cfg_t;

/* HAL_ERR_UNSUPPORTED if the chip cannot hit this config (e.g. frequency).
 */
hal_status_t hal_spi_open(
    hal_spi_id_t id,
    const hal_spi_cfg_t * cfg
);

/* One clock drives both directions: tx and rx cover the same len bytes.
 * Either may be NULL (write-only or read-only; the other side is ignored /
 * sent as don't-care). Blocking. */
hal_status_t hal_spi_transfer(
    hal_spi_id_t id,
    const uint8_t * tx,
    uint8_t * rx,
    size_t len
);

#endif /* HAL_SPI_H */
