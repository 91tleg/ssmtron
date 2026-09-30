/* Single-instance display interface. One driver is linked per build;
 * callers provide RGB565 colors. */
#ifndef DISPLAY_H
#define DISPLAY_H

#include <stdbool.h>
#include <stdint.h>

#include "hal_gpio.h"
#include "hal_spi.h"
#include "hal_status.h"

/* Which bus and control lines the panel is wired to. The board (BSP) knows
 * these ids; the driver does not. backlight is only touched when
 * has_backlight is true (some modules tie it permanently on). */
typedef struct
{
    hal_spi_id_t spi;
    hal_gpio_id_t cs;
    hal_gpio_id_t dc;
    hal_gpio_id_t reset;
    hal_gpio_id_t backlight;
    bool has_backlight;
} display_pins_t;

hal_status_t display_init( const display_pins_t * pins );

/* Writes a w x h block of RGB565 pixels, row-major, top-left at (x, y).
 * Blocking. HAL_ERR_INVALID if the rectangle falls outside the panel. */
hal_status_t display_blit(
    uint16_t x,
    uint16_t y,
    uint16_t w,
    uint16_t h,
    const uint16_t * pixels
);

/* Fills the whole panel with one color; cheaper than blitting a filled
 * buffer of it. */
hal_status_t display_clear( uint16_t color_rgb565 );

#endif /* DISPLAY_H */
