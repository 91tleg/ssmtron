/* Looking up a character's bitmap and blending its ink over a color. */
#include "lib/ui/font.h"

#include <stddef.h>

#include "lib/ui/font_b612.h"

/* Lowercase counts as uppercase, anything unmapped as a space. */
static unsigned char code_index( char c )
{
    unsigned char up = ( unsigned char )c;

    if ( ( up >= ( unsigned char )'a' ) && ( up <= ( unsigned char )'z' ) )
    {
        up = ( unsigned char )( up - ( unsigned char )'a'
                                + ( unsigned char )'A' );
    }
    return ( ( up >= FONT_B612_FIRST ) && ( up <= FONT_B612_LAST ) )
               ? ( unsigned char )( up - FONT_B612_FIRST )
               : ( unsigned char )( ( unsigned char )' ' - FONT_B612_FIRST );
}

uint8_t font_alpha(
    char c,
    uint8_t scale,
    uint16_t x,
    uint16_t y
)
{
    uint32_t cell_w        = FONT_CELL_W * ( uint32_t )scale;
    uint32_t cell_h        = FONT_CELL_H * ( uint32_t )scale;
    const font_b612_t * bm = font_b612_find( scale );
    uint8_t alpha          = 0u;

    if ( ( scale > 0u ) && ( x < cell_w ) && ( y < cell_h ) )
    {
        if ( bm != NULL )
        {
            uint32_t n = ( ( uint32_t )y * cell_w ) + x;
            uint8_t byte =
                bm->data
                    [( ( uint32_t )code_index( c ) * bm->bytes_per_glyph )
                     + ( n / 2u )];

            alpha = ( ( n % 2u ) == 0u ) ? ( uint8_t )( byte & 0x0Fu )
                                         : ( uint8_t )( byte >> 4 );
        }
        else
        {
            /* no bitmaps at this size: blank, not a wrong glyph */
        }
    }
    return alpha;
}

/* One RGB565 channel: fg over bg by alpha out of FONT_ALPHA_MAX. */
static uint32_t
mix( uint32_t fg,
     uint32_t bg,
     uint32_t alpha )
{
    return ( ( fg * alpha ) + ( bg * ( FONT_ALPHA_MAX - alpha ) )
             + ( FONT_ALPHA_MAX / 2u ) )
           / FONT_ALPHA_MAX;
}

uint16_t font_blend(
    uint16_t fg,
    uint16_t bg,
    uint8_t alpha
)
{
    uint32_t a   = alpha;
    uint16_t out = bg;

    if ( a > FONT_ALPHA_MAX )
    {
        a = FONT_ALPHA_MAX;
    }

    if ( a == FONT_ALPHA_MAX )
    {
        out = fg;
    }
    else if ( a != 0u )
    {
        uint32_t r = mix( ( fg >> 11 ) & 0x1Fu, ( bg >> 11 ) & 0x1Fu, a );
        uint32_t g = mix( ( fg >> 5 ) & 0x3Fu, ( bg >> 5 ) & 0x3Fu, a );
        uint32_t b = mix( fg & 0x1Fu, bg & 0x1Fu, a );

        out = ( uint16_t )( ( r << 11 ) | ( g << 5 ) | b );
    }
    else
    {
        /* no ink: the background as it was */
    }
    return out;
}
