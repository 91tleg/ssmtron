/* A minimal 5x7 monospace bitmap font: digits, uppercase letters (input is
 * upshifted; there is no separate lowercase glyph shape), space, and the
 * handful of punctuation marks fmt.h and the SSM1 profiles actually use
 * (- . / # % _). Anything else -- including every byte below 0x20 or
 * above 0x5F -- renders as blank.
 *
 * Unlike gc9a01.c's init table, this one was not copied from a datasheet
 * or another project: it was built and proofread as ASCII art before
 * being encoded, so there is no unverified-vendor-table risk here. */
#ifndef UI_FONT_H_
#define UI_FONT_H_

#include <stdint.h>

#define UI_FONT_W 5u
#define UI_FONT_H 7u

/* rows[0] is the glyph's top row; within a row, bit 4 is the leftmost
 * pixel and bit 0 the rightmost (only the low 5 bits are used). */
typedef struct
{
    uint8_t rows[UI_FONT_H];
} ui_glyph_t;

/* Always returns a valid glyph: unmapped characters return the blank
 * (space) glyph rather than NULL, so callers never need a null check. */
const ui_glyph_t * ui_font_glyph( char c );

#endif /* UI_FONT_H_ */
