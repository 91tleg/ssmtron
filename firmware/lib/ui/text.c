/* Render one pixel row at a time to keep the scratch buffer small. */
#include "lib/ui/text.h"

#include <stdbool.h>
#include <string.h>

#include "lib/display/display.h"
#include "lib/ui/font.h"

/* The widest row it ever draws is the panel's width. */
#define TEXT_MAX_ROW_PX ( DISPLAY_WIDTH )

static void draw_widget(
    const text_widget_t * w,
    const char * text
)
{
    static uint16_t row_buf[TEXT_MAX_ROW_PX];
    size_t text_len    = strlen( text );
    uint16_t cell_w    = ( uint16_t )( FONT_CELL_W * w->scale );
    uint16_t font_rows = ( uint16_t )( FONT_CELL_H * w->scale );
    uint16_t row_pixels =
        ( w->rect.w <= TEXT_MAX_ROW_PX ) ? w->rect.w : TEXT_MAX_ROW_PX;
    /* Every character fills its whole cell, spacing included, so the text is
     * centered by its cells, as the dial centers its labels. */
    uint16_t text_px = ( uint16_t )( text_len * cell_w );
    uint16_t x_off =
        ( ( w->align == TEXT_ALIGN_CENTER ) && ( row_pixels > text_px ) )
            ? ( uint16_t )( ( row_pixels - text_px ) / 2u )
            : 0u;

    for ( uint16_t row = 0; row < w->rect.h; row++ )
    {
        bool in_font = ( row < font_rows ) && ( cell_w > 0u );

        for ( uint16_t col = 0; col < row_pixels; col++ )
        {
            uint16_t px = w->bg;

            if ( in_font && ( col >= x_off ) )
            {
                uint16_t text_col    = ( uint16_t )( col - x_off );
                uint16_t glyph_index = ( uint16_t )( text_col / cell_w );
                uint16_t within      = ( uint16_t )( text_col % cell_w );

                if ( glyph_index < ( uint16_t )text_len )
                {
                    px = font_blend(
                        w->fg,
                        w->bg,
                        font_alpha( text[glyph_index], w->scale, within, row )
                    );
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
    size_t n = 0u;

    /* Text longer than the cache is cut. Only strcmp reads the cache, so the
     * rest of it needs no padding. */
    while ( ( n < TEXT_MAX ) && ( text[n] != '\0' ) )
    {
        w->cache[n] = text[n];
        n++;
    }
    w->cache[n] = '\0';
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
