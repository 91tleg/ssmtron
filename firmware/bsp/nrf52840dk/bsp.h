/* Board support: what is wired where on this board. It names the HAL ids
 * (which index the lists in board.overlay, /zephyr,user) so that nothing
 * above it needs to know a pin or an instance number. One directory per
 * board, chosen by CMake (BSP_BOARD); every board provides a bsp.h with
 * the same names. */
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

#endif /* BSP_H */
