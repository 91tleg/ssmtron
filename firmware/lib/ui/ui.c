/* Render one pixel row at a time to keep the scratch buffer small. */
#include "ui.h"

#include <stdbool.h>
#include <string.h>

#include "display.h"
#include "ui_font.h"

/* The widest panel this engine has been sized for; gc9a01 (240) is the
 * only display driver today. Bump this if a wider panel is ever added. */
#define UI_MAX_ROW_PX 240u

static void draw_widget(
    const ui_widget_t * w,
    const char * text
)
{
    static uint16_t row_buf[UI_MAX_ROW_PX];
    size_t text_len    = strlen( text );
    uint16_t cell_w    = ( uint16_t )( ( UI_FONT_W + 1u ) * w->scale );
    uint16_t glyph_px  = ( uint16_t )( UI_FONT_W * w->scale );
    uint16_t font_rows = ( uint16_t )( UI_FONT_H * w->scale );
    uint16_t row_pixels =
        ( w->rect.w <= UI_MAX_ROW_PX ) ? w->rect.w : UI_MAX_ROW_PX;

    for ( uint16_t row = 0; row < w->rect.h; row++ )
    {
        bool in_font      = ( row < font_rows ) && ( cell_w > 0u );
        uint8_t glyph_row = ( uint8_t )( row / w->scale );

        for ( uint16_t col = 0; col < row_pixels; col++ )
        {
            uint16_t px = w->bg;

            if ( in_font )
            {
                uint16_t cell   = ( uint16_t )( col / cell_w );
                uint16_t within = ( uint16_t )( col % cell_w );

                if ( ( cell < ( uint16_t )text_len ) && ( within < glyph_px ) )
                {
                    const ui_glyph_t * gl = ui_font_glyph( text[cell] );
                    uint8_t font_col      = ( uint8_t )( within / w->scale );
                    uint8_t bit =
                        ( uint8_t )( ( gl->rows[glyph_row]
                                       >> ( UI_FONT_W - 1u - font_col ) )
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
    ui_widget_t * w,
    const char * text
)
{
    ( void )strncpy( w->cache, text, UI_TEXT_MAX );
    w->cache[UI_TEXT_MAX] = '\0';
}

void ui_init(
    ui_widget_t * widgets,
    size_t count,
    uint16_t page_bg
)
{
    ( void )display_clear( page_bg );
    for ( size_t i = 0; i < count; i++ )
    {
        char text[UI_TEXT_MAX + 1u];

        widgets[i].text( text );
        draw_widget( &widgets[i], text );
        latch( &widgets[i], text );
    }
}

void ui_poll(
    ui_widget_t * widgets,
    size_t count
)
{
    for ( size_t i = 0; i < count; i++ )
    {
        char text[UI_TEXT_MAX + 1u];

        widgets[i].text( text );
        if ( strcmp( text, widgets[i].cache ) != 0 )
        {
            draw_widget( &widgets[i], text );
            latch( &widgets[i], text );
        }
    }
}
