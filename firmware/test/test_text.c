/* Verify that unchanged text skips blits and changed text redraws fully. */
#include <string.h>

#include "display.h"
#include "minitest.h"
#include "text.h"
#include "font.h"

static unsigned blit_calls;
static unsigned clear_calls;

hal_status_t display_blit(
    uint16_t x,
    uint16_t y,
    uint16_t w,
    uint16_t h,
    const uint16_t * pixels
)
{
    ( void )x;
    ( void )y;
    ( void )w;
    ( void )h;
    ( void )pixels;
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
        { { 0, 0, 40, FONT_H }, 1, 0xFFFFu, 0x0000u, one_text, { 0 } },
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
        { { 0, 0, 40, FONT_H }, 1, 0xFFFFu, 0x0000u, one_text, { 0 } },
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
        { { 0, 0, 40, FONT_H }, 1, 0xFFFFu, 0x0000u, one_text, { 0 } },
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
    test_font_unknown_is_blank();
    test_font_lowercase_matches_uppercase();
    test_font_digits_differ();

    return MT_RESULT();
}
