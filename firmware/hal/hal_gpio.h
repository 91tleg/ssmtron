/* Ids index the GPIOs the board declares; the HAL does not know which pin
 * an id is. Polarity is also board-defined: reads/writes are logical. */
#ifndef HAL_GPIO_H
#define HAL_GPIO_H

#include <stdbool.h>
#include <stdint.h>

#include "hal_status.h"

typedef uint8_t hal_gpio_id_t;

typedef enum
{
    HAL_GPIO_INPUT,
    HAL_GPIO_OUTPUT
} hal_gpio_mode_t;

hal_status_t hal_gpio_open(
    hal_gpio_id_t id,
    hal_gpio_mode_t mode
);

bool hal_gpio_read( hal_gpio_id_t id );

void hal_gpio_write(
    hal_gpio_id_t id,
    bool asserted
);

#endif /* HAL_GPIO_H */
