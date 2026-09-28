/* Neutral display interface. Exactly one concrete driver (lib/<name>) is
 * linked per build, selected by CMake, the same way lib/ecu selects a
 * protocol. Single instance. Colors are RGB565, the common native format
 * for small SPI TFTs, so callers do the conversion once instead of every
 * driver doing it differently. */
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

uint16_t display_width( void );
uint16_t display_height( void );

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

/* No-op if has_backlight was false at display_init. */
void display_backlight( bool on );

#endif /* DISPLAY_H */
