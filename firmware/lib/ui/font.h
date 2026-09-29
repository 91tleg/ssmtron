/* 5x7 font for uppercase text and common punctuation. Unsupported bytes
 * render as blank; lowercase input is converted to uppercase. */
#ifndef FONT_H_
#define FONT_H_

#include <stdint.h>

#define FONT_W 5u
#define FONT_H 7u

/* rows[0] is the glyph's top row; within a row, bit 4 is the leftmost
 * pixel and bit 0 the rightmost (only the low 5 bits are used). */
typedef struct
{
    uint8_t rows[FONT_H];
} font_glyph_t;

/* Always returns a valid glyph: unmapped characters return the blank
 * (space) glyph rather than NULL, so callers never need a null check. */
const font_glyph_t * font_glyph( char c );

#endif /* FONT_H_ */
