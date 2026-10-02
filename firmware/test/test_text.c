/* Verify that unchanged text skips blits and changed text redraws fully. */
#include <string.h>

#include "lib/display/display.h"
#include "test/minitest.h"
#include "lib/ui/text.h"
#include "lib/ui/font.h"

static unsigned blit_calls;
static unsigned clear_calls;

/* The top pixel row of the widget at y = 0, kept to see where text sits. */
static uint16_t top_row[64];
static uint16_t top_row_w;

hal_status_t display_blit(
    uint16_t x,
    uint16_t y,
    uint16_t w,
    uint16_t h,
    const uint16_t * pixels
)
{
    ( void )x;
    ( void )h;
    if ( ( y == 0u ) && ( w <= 64u ) )
    {
        ( void )memcpy( top_row, pixels, w * sizeof( pixels[0] ) );
        top_row_w = w;
    }
    blit_calls++;
    return HAL_OK;
}

hal_status_t display_clear( uint16_t color_rgb565 )
{
    ( void )color_rgb565;
    clear_calls++;
    return HAL_OK;
}

static const char * source_text = "RPM";

static void one_text( char * buf )
{
    ( void )strncpy( buf, source_text, TEXT_MAX );
    buf[TEXT_MAX] = '\0';
}

static void reset( void )
{
    blit_calls  = 0;
    clear_calls = 0;
    source_text = "RPM";
}

static void test_init_draws_and_clears( void )
{
    text_widget_t w[1] = {
        { .rect  = { 0, 0, 40, FONT_CELL_H },
          .scale = 1,
          .fg    = 0xFFFFu,
          .bg    = 0x0000u,
          .text  = one_text,
          .cache = { 0 },
          .align = TEXT_ALIGN_LEFT },
    };

    reset();
    text_init( w, 1, 0x0000u );
    CHECK_EQ( clear_calls, 1 );
    CHECK_EQ( blit_calls, FONT_CELL_H ); /* one row-blit per pixel row */
    CHECK( strcmp( w[0].cache, "RPM" ) == 0 );
}

static void test_poll_skips_unchanged_text( void )
{
    text_widget_t w[1] = {
        { .rect  = { 0, 0, 40, FONT_CELL_H },
          .scale = 1,
          .fg    = 0xFFFFu,
          .bg    = 0x0000u,
          .text  = one_text,
          .cache = { 0 },
          .align = TEXT_ALIGN_LEFT },
    };

    reset();
    text_init( w, 1, 0x0000u );
    blit_calls = 0;

    text_poll( w, 1 );
    text_poll( w, 1 );
    CHECK_EQ( blit_calls, 0 ); /* text never changed: no SPI traffic */
}

static void test_poll_redraws_on_change( void )
{
    text_widget_t w[1] = {
        { .rect  = { 0, 0, 40, FONT_CELL_H },
          .scale = 1,
          .fg    = 0xFFFFu,
          .bg    = 0x0000u,
          .text  = one_text,
          .cache = { 0 },
          .align = TEXT_ALIGN_LEFT },
    };

    reset();
    text_init( w, 1, 0x0000u );
    blit_calls = 0;

    source_text = "800";
    text_poll( w, 1 );
    CHECK_EQ( blit_calls, FONT_CELL_H );
    CHECK( strcmp( w[0].cache, "800" ) == 0 );

    blit_calls = 0;
    text_poll( w, 1 ); /* same text again: back to quiet */
    CHECK_EQ( blit_calls, 0 );
}

/* First and last lit columns of the top row, or -1 if none. */
static void lit_span(
    int * first,
    int * last
)
{
    *first = -1;
    *last  = -1;
    for ( uint16_t i = 0; i < top_row_w; i++ )
    {
        if ( top_row[i] == 0xFFFFu )
        {
            *first = ( *first < 0 ) ? ( int )i : *first;
            *last  = ( int )i;
        }
    }
}

static void test_centered_text_is_centered_to_the_pixel( void )
{
    /* One character is one 6 px cell, so in a 41 px rect the cell starts
     * (41 - 6) / 2 = 17 px further right than the same text aligned left:
     * 17 px of rect on one side and 18 on the other, the nearest to even.
     * The glyph's own ink columns do not matter, only that the shift is
     * exactly 17. */
    text_widget_t w[2] = {
        { .rect  = { 0, 0, 41, FONT_CELL_H },
          .scale = 1,
          .fg    = 0xFFFFu,
          .bg    = 0x0000u,
          .text  = one_text,
          .cache = { 0 },
          .align = TEXT_ALIGN_CENTER },
        { .rect  = { 0, 0, 41, FONT_CELL_H },
          .scale = 1,
          .fg    = 0xFFFFu,
          .bg    = 0x0000u,
          .text  = one_text,
          .cache = { 0 },
          .align = TEXT_ALIGN_LEFT },
    };
    int first;
    int last;
    int left_first;
    int left_last;

    reset();
    source_text = "T";
    text_init( &w[1], 1, 0x0000u );
    lit_span( &left_first, &left_last );
    CHECK( left_first >= 0 );
    CHECK( left_last > left_first );
    CHECK( left_last < 6 ); /* inside its own cell */

    text_init( w, 1, 0x0000u );
    lit_span( &first, &last );
    CHECK_EQ( first, left_first + 17 );
    CHECK_EQ( last, left_last + 17 );
}

int main( void )
{
    test_init_draws_and_clears();
    test_poll_skips_unchanged_text();
    test_poll_redraws_on_change();
    test_centered_text_is_centered_to_the_pixel();

    return MT_RESULT();
}
