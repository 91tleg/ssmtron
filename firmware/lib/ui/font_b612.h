/* B612 Mono bitmaps made by tools/gen_font.py: for each text size, 4 bit
 * alpha for the characters FONT_B612_FIRST to FONT_B612_LAST, two pixels a byte
 * (the first in the low half), row by row in a cell of 6*scale by 7*scale
 * pixels. */
#ifndef FONT_B612_H_
#define FONT_B612_H_

#include <stdint.h>

/* The characters the bitmaps cover. */
#define FONT_B612_FIRST ( 0x20u )
#define FONT_B612_LAST  ( 0x5Fu )

typedef struct
{
    uint8_t scale;
    const uint8_t * data;     /* the first glyph's bytes */
    uint16_t bytes_per_glyph; /* where the next glyph starts */
} font_b612_t;

/* The bitmaps for text at this scale, or NULL when there are none. */
const font_b612_t * font_b612_find( uint8_t scale );

#endif /* FONT_B612_H_ */
