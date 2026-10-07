/* HAL ids index the devices declared in board.overlay. */
#ifndef BSP_H
#define BSP_H

#include "hal_gpio.h"
#include "hal_spi.h"
#include "hal_uart.h"

/* hal-uarts = < ... > */
#define BSP_UART_ECU                                                           \
    ( ( hal_uart_id_t )0u ) /* K-line via the level shifter                    \
                             */

/* hal-gpios = < ... > */
#define BSP_GPIO_NEXT ( ( hal_gpio_id_t )0u ) /* cycles the shown parameter */

/* No panel is on the bench yet: pin numbers only, nothing to verify
 * against real hardware until then. */
#define BSP_GPIO_DISPLAY_CS    ( ( hal_gpio_id_t )1u )
#define BSP_GPIO_DISPLAY_DC    ( ( hal_gpio_id_t )2u )
#define BSP_GPIO_DISPLAY_RESET ( ( hal_gpio_id_t )3u )

/* Hold to clear trouble codes; a tap does nothing (see gauge.c). */
#define BSP_GPIO_ACTION ( ( hal_gpio_id_t )4u )

/* hal-spis = < ... > */
#define BSP_SPI_DISPLAY ( ( hal_spi_id_t )0u )

/* The display's bus clock. SPIM3, the instance used, runs up to 32 MHz. That
 * needs short wires; lower this if the picture shows noise. */
#define BSP_DISPLAY_SPI_HZ ( 32000000u )

#endif /* BSP_H */
