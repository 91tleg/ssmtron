/* Verify that unchanged text skips blits and changed text redraws fully. */
#include <string.h>

#include "display.h"
#include "minitest.h"
#include "text.h"
#include "font.h"

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
        { { 0, 0, 40, FONT_H },
          1,
          0xFFFFu,
          0x0000u,
          one_text,
          { 0 },
          TEXT_ALIGN_LEFT },
    };

    reset();
    text_init( w, 1, 0x0000u );
    CHECK_EQ( clear_calls, 1 );
    CHECK_EQ( blit_calls, FONT_H ); /* one row-blit per pixel row */
    CHECK( strcmp( w[0].cache, "RPM" ) == 0 );
}

static void test_poll_skips_unchanged_text( void )
{
    text_widget_t w[1] = {
        { { 0, 0, 40, FONT_H },
          1,
          0xFFFFu,
          0x0000u,
          one_text,
          { 0 },
          TEXT_ALIGN_LEFT },
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
        { { 0, 0, 40, FONT_H },
          1,
          0xFFFFu,
          0x0000u,
          one_text,
          { 0 },
          TEXT_ALIGN_LEFT },
    };

    reset();
    text_init( w, 1, 0x0000u );
    blit_calls = 0;

    source_text = "800";
    text_poll( w, 1 );
    CHECK_EQ( blit_calls, FONT_H );
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
    /* 'T' has a full top row, 5 px wide: in a 41 px rect it belongs at
     * columns 18 to 22, the same distance from both edges. */
    text_widget_t w[2] = {
        { { 0, 0, 41, FONT_H },
          1,
          0xFFFFu,
          0x0000u,
          one_text,
          { 0 },
          TEXT_ALIGN_CENTER },
        { { 0, 0, 41, FONT_H },
          1,
          0xFFFFu,
          0x0000u,
          one_text,
          { 0 },
          TEXT_ALIGN_LEFT },
    };
    int first;
    int last;

    reset();
    source_text = "T";
    text_init( w, 1, 0x0000u );
    lit_span( &first, &last );
    CHECK_EQ( first, 18 );
    CHECK_EQ( last, 22 );

    text_init( &w[1], 1, 0x0000u ); /* left is unchanged */
    lit_span( &first, &last );
    CHECK_EQ( first, 0 );
    CHECK_EQ( last, 4 );
}

/* The update page's clock is m:ss, so the colon must be drawn. */
static void test_font_colon_is_drawn( void )
{
    const font_glyph_t * g = font_glyph( ':' );
    unsigned lit           = 0;

    for ( size_t row = 0; row < FONT_H; row++ )
    {
        lit += ( g->rows[row] != 0u ) ? 1u : 0u;
    }
    CHECK( lit > 0u );
}

static void test_font_unknown_is_blank( void )
{
    const font_glyph_t * unknown = font_glyph( '~' );
    const font_glyph_t * space   = font_glyph( ' ' );

    CHECK( memcmp( unknown->rows, space->rows, FONT_H ) == 0 );
}

static void test_font_lowercase_matches_uppercase( void )
{
    const font_glyph_t * lower = font_glyph( 'a' );
    const font_glyph_t * upper = font_glyph( 'A' );

    CHECK( memcmp( lower->rows, upper->rows, FONT_H ) == 0 );
}

static void test_font_digits_differ( void )
{
    const font_glyph_t * zero = font_glyph( '0' );
    const font_glyph_t * one  = font_glyph( '1' );

    CHECK( memcmp( zero->rows, one->rows, FONT_H ) != 0 );
}

int main( void )
{
    test_init_draws_and_clears();
    test_poll_skips_unchanged_text();
    test_poll_redraws_on_change();
    test_centered_text_is_centered_to_the_pixel();
    test_font_colon_is_drawn();
    test_font_unknown_is_blank();
    test_font_lowercase_matches_uppercase();
    test_font_digits_differ();

    return MT_RESULT();
}
