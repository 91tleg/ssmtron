/* Render one pixel row at a time to keep the scratch buffer small. */
#include "text.h"

#include <stdbool.h>
#include <string.h>

#include "display.h"
#include "font.h"

/* The widest panel this engine has been sized for; gc9a01 (240) is the
 * only display driver today. Bump this if a wider panel is ever added. */
#define TEXT_MAX_ROW_PX 240u

static void draw_widget(
    const text_widget_t * w,
    const char * text
)
{
    static uint16_t row_buf[TEXT_MAX_ROW_PX];
    size_t text_len    = strlen( text );
    uint16_t cell_w    = ( uint16_t )( ( FONT_W + 1u ) * w->scale );
    uint16_t glyph_px  = ( uint16_t )( FONT_W * w->scale );
    uint16_t font_rows = ( uint16_t )( FONT_H * w->scale );
    uint16_t row_pixels =
        ( w->rect.w <= TEXT_MAX_ROW_PX ) ? w->rect.w : TEXT_MAX_ROW_PX;
    /* The last glyph has no trailing gap, so it does not skew the center. */
    uint16_t gap_px = ( uint16_t )( cell_w - glyph_px );
    uint16_t text_px =
        ( text_len > 0u ) ? ( uint16_t )( ( text_len * cell_w ) - gap_px ) : 0u;
    uint16_t x_off =
        ( ( w->align == TEXT_ALIGN_CENTER ) && ( row_pixels > text_px ) )
            ? ( uint16_t )( ( row_pixels - text_px ) / 2u )
            : 0u;

    for ( uint16_t row = 0; row < w->rect.h; row++ )
    {
        bool in_font      = ( row < font_rows ) && ( cell_w > 0u );
        uint8_t glyph_row = ( uint8_t )( row / w->scale );

        for ( uint16_t col = 0; col < row_pixels; col++ )
        {
            uint16_t px = w->bg;

            if ( in_font && ( col >= x_off ) )
            {
                uint16_t text_col    = ( uint16_t )( col - x_off );
                uint16_t glyph_index = ( uint16_t )( text_col / cell_w );
                uint16_t within      = ( uint16_t )( text_col % cell_w );

                if ( ( glyph_index < ( uint16_t )text_len )
                     && ( within < glyph_px ) )
                {
                    const font_glyph_t * gl = font_glyph( text[glyph_index] );
                    uint8_t font_col        = ( uint8_t )( within / w->scale );
                    uint8_t bit             = ( uint8_t )( ( gl->rows[glyph_row]
                                                 >> ( FONT_W - 1u - font_col ) )
                                               & 1u );

                    px = ( bit != 0u ) ? w->fg : w->bg;
                }
            }
            row_buf[col] = px;
        }
        ( void )display_blit(
            w->rect.x, ( uint16_t )( w->rect.y + row ), row_pixels, 1u, row_buf
        );
    }
}

static void latch(
    text_widget_t * w,
    const char * text
)
{
    ( void )strncpy( w->cache, text, TEXT_MAX );
    w->cache[TEXT_MAX] = '\0';
}

void text_init(
    text_widget_t * widgets,
    size_t count,
    uint16_t page_bg
)
{
    ( void )display_clear( page_bg );
    for ( size_t i = 0; i < count; i++ )
    {
        char text[TEXT_MAX + 1u];

        widgets[i].text( text );
        draw_widget( &widgets[i], text );
        latch( &widgets[i], text );
    }
}

void text_poll(
    text_widget_t * widgets,
    size_t count
)
{
    for ( size_t i = 0; i < count; i++ )
    {
        char text[TEXT_MAX + 1u];

        widgets[i].text( text );
        if ( strcmp( text, widgets[i].cache ) != 0 )
        {
            draw_widget( &widgets[i], text );
            latch( &widgets[i], text );
        }
    }
}
