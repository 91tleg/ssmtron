/* 5x7 font for uppercase text and common punctuation. Unsupported bytes
 * render as blank; lowercase input is converted to uppercase. */
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
