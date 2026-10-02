/* The firmware's only font: B612 Mono, as anti-aliased bitmaps made by
 * tools/gen_font.py (see font_b612.h). It covers the characters 0x20 to
 * 0x5F: uppercase letters, digits and common punctuation. Lowercase input is
 * drawn as uppercase and anything else as a space. */
#ifndef FONT_H_
#define FONT_H_

#include <stdint.h>

/* One character's cell at text size 1. At size n the cell is n times this:
 * every character is the same width, so a text of k characters is exactly
 * k * FONT_CELL_W * n wide. */
#define FONT_CELL_W 6u
#define FONT_CELL_H 7u

/* How much of the pixel (x, y) of a character's cell is ink, from 0 (none)
 * to FONT_ALPHA_MAX (all), for text drawn at this scale. Outside the cell it
 * is 0. So is every pixel at a scale the generated font has no bitmaps for:
 * the UI's sizes are checked against it by test_font. */
#define FONT_ALPHA_MAX ( 15u )
uint8_t font_alpha(
    char c,
    uint8_t scale,
    uint16_t x,
    uint16_t y
);

/* fg over bg by alpha (0 to FONT_ALPHA_MAX, larger is taken as the most),
 * both RGB565. */
uint16_t font_blend(
    uint16_t fg,
    uint16_t bg,
    uint8_t alpha
);

#endif /* FONT_H_ */
