/* The widget engine, with a fake display.h: the property under test is
 * that a widget whose text has not changed since the last poll is never
 * blitted (the "dirty bit"), and one whose text changed is redrawn in
 * full so stale, now-shorter text is erased. */
#include <string.h>

#include "display.h"
#include "minitest.h"
#include "ui.h"
#include "ui_font.h"

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
    ( void )strncpy( buf, source_text, UI_TEXT_MAX );
    buf[UI_TEXT_MAX] = '\0';
}

static void reset( void )
{
    blit_calls  = 0;
    clear_calls = 0;
    source_text = "RPM";
}

static void test_init_draws_and_clears( void )
{
    ui_widget_t w[1] = {
        { { 0, 0, 40, UI_FONT_H }, 1, 0xFFFFu, 0x0000u, one_text, { 0 } },
    };

    reset();
    ui_init( w, 1, 0x0000u );
    CHECK_EQ( clear_calls, 1 );
    CHECK_EQ( blit_calls, UI_FONT_H ); /* one row-blit per pixel row */
    CHECK( strcmp( w[0].cache, "RPM" ) == 0 );
}

static void test_poll_skips_unchanged_text( void )
{
    ui_widget_t w[1] = {
        { { 0, 0, 40, UI_FONT_H }, 1, 0xFFFFu, 0x0000u, one_text, { 0 } },
    };

    reset();
    ui_init( w, 1, 0x0000u );
    blit_calls = 0;

    ui_poll( w, 1 );
    ui_poll( w, 1 );
    CHECK_EQ( blit_calls, 0 ); /* text never changed: no SPI traffic */
}

static void test_poll_redraws_on_change( void )
{
    ui_widget_t w[1] = {
        { { 0, 0, 40, UI_FONT_H }, 1, 0xFFFFu, 0x0000u, one_text, { 0 } },
    };

    reset();
    ui_init( w, 1, 0x0000u );
    blit_calls = 0;

    source_text = "800";
    ui_poll( w, 1 );
    CHECK_EQ( blit_calls, UI_FONT_H );
    CHECK( strcmp( w[0].cache, "800" ) == 0 );

    blit_calls = 0;
    ui_poll( w, 1 ); /* same text again: back to quiet */
    CHECK_EQ( blit_calls, 0 );
}

static void test_font_unknown_is_blank( void )
{
    const ui_glyph_t * unknown = ui_font_glyph( '~' );
    const ui_glyph_t * space   = ui_font_glyph( ' ' );

    CHECK( memcmp( unknown->rows, space->rows, UI_FONT_H ) == 0 );
}

static void test_font_lowercase_matches_uppercase( void )
{
    const ui_glyph_t * lower = ui_font_glyph( 'a' );
    const ui_glyph_t * upper = ui_font_glyph( 'A' );

    CHECK( memcmp( lower->rows, upper->rows, UI_FONT_H ) == 0 );
}

static void test_font_digits_differ( void )
{
    const ui_glyph_t * zero = ui_font_glyph( '0' );
    const ui_glyph_t * one  = ui_font_glyph( '1' );

    CHECK( memcmp( zero->rows, one->rows, UI_FONT_H ) != 0 );
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
