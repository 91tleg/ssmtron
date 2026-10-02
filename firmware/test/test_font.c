/* The font: every text size the UI asks for has bitmaps on every supported
 * panel, every character in range has ink, and blending behaves. Built for
 * each panel width, since the sizes a build carries follow DISPLAY_WIDTH. */
#include <stdio.h>
#include <string.h>

#include "lib/ui/font.h"
#include "lib/ui/font_b612.h"
#include "lib/ui/ui_geom.h"
#include "test/minitest.h"

/* The text sizes the UI uses, as given to UI_TEXT_SCALE for the 240 px panel
 * (tools/gen_font.py --sizes). If the UI gains a size, add it here and there.
 */
static const int ui_sizes[] = { 1, 2, 4 };
#define UI_SIZE_COUNT ( sizeof( ui_sizes ) / sizeof( ui_sizes[0] ) )

static uint8_t scale_of( int n )
{
    return ( uint8_t )UI_TEXT_SCALE( n );
}

/* The sum of a character's ink over its whole cell. */
static unsigned
ink( char c,
     uint8_t scale )
{
    unsigned total = 0u;

    for ( uint16_t y = 0; y < ( FONT_CELL_H * scale ); y++ )
    {
        for ( uint16_t x = 0; x < ( FONT_CELL_W * scale ); x++ )
        {
            total += font_alpha( c, scale, x, y );
        }
    }
    return total;
}

static bool same_pixels(
    char a,
    char b,
    uint8_t scale
)
{
    bool same = true;

    for ( uint16_t y = 0; y < ( FONT_CELL_H * scale ); y++ )
    {
        for ( uint16_t x = 0; x < ( FONT_CELL_W * scale ); x++ )
        {
            same = same
                   && ( font_alpha( a, scale, x, y )
                        == font_alpha( b, scale, x, y ) );
        }
    }
    return same;
}

/* The panel's sizes all have bitmaps, so no text is ever blank by accident. */
static void test_every_size_the_ui_asks_for_has_bitmaps( void )
{
    for ( size_t i = 0; i < UI_SIZE_COUNT; i++ )
    {
        uint8_t scale = scale_of( ui_sizes[i] );

        CHECK( scale > 0u );
        CHECK( font_b612_find( scale ) != NULL );
        CHECK( ink( '8', scale ) > 0u );
    }
}

/* Every character from the first to the last is drawn, except the ones that
 * sit wholly below the baseline or above the cell, at every size in use. */
static void test_every_character_has_ink( void )
{
    for ( size_t i = 0; i < UI_SIZE_COUNT; i++ )
    {
        uint8_t scale = scale_of( ui_sizes[i] );

        CHECK_EQ( ink( ' ', scale ), 0u );
        for ( char c = '!'; c <= '_'; c++ )
        {
            if ( ink( c, scale ) == 0u )
            {
                printf(
                    "  no ink for '%c' at scale %u\n", c, ( unsigned )scale
                );
            }
            CHECK( ink( c, scale ) > 0u );
        }
    }
}

static void test_unknown_and_lowercase( void )
{
    uint8_t scale = scale_of( 1 );

    CHECK( same_pixels( '~', ' ', scale ) ); /* beyond the range: a space */
    CHECK( same_pixels( '\x7f', ' ', scale ) );
    CHECK( same_pixels( '\xe9', ' ', scale ) ); /* a high byte, not a crash */
    CHECK( same_pixels( 'a', 'A', scale ) );
    CHECK( same_pixels( 'z', 'Z', scale ) );
    CHECK( !same_pixels( '0', '1', scale ) );
    CHECK( !same_pixels( 'A', 'B', scale ) );
}

/* A zero and a capital O are different at every size in use, so a reading
 * such as 1O0 can be told from 100. */
static void test_zero_and_capital_o_differ( void )
{
    for ( size_t i = 0; i < UI_SIZE_COUNT; i++ )
    {
        CHECK( !same_pixels( '0', 'O', scale_of( ui_sizes[i] ) ) );
    }
}

/* Outside the cell, and at a size with no bitmaps, there is no ink. */
static void test_no_ink_outside_the_cell_or_without_bitmaps( void )
{
    uint8_t scale       = scale_of( 1 );
    uint8_t unsupported = 0u;

    CHECK_EQ( font_alpha( '8', scale, FONT_CELL_W * scale, 0 ), 0u );
    CHECK_EQ( font_alpha( '8', scale, 0, FONT_CELL_H * scale ), 0u );
    CHECK_EQ( font_alpha( '8', scale, 65535u, 65535u ), 0u );
    CHECK_EQ( font_alpha( '8', 0u, 0, 0 ), 0u );

    for ( uint8_t s = 1u; ( unsupported == 0u ) && ( s < 40u ); s++ )
    {
        unsupported = ( font_b612_find( s ) == NULL ) ? s : 0u;
    }
    CHECK( unsupported != 0u ); /* some size is not carried */
    CHECK_EQ( ink( '8', unsupported ), 0u );
}

/* The bitmaps are in the cell: no ink on the last column's far side that
 * would be cut, and a capital reaches the top and sits on the bottom. */
static void test_capitals_fill_the_cell_height( void )
{
    for ( size_t i = 0; i < UI_SIZE_COUNT; i++ )
    {
        uint8_t scale     = scale_of( ui_sizes[i] );
        uint16_t last_row = ( uint16_t )( ( FONT_CELL_H * scale ) - 1u );
        unsigned top      = 0u;
        unsigned bottom   = 0u;

        for ( uint16_t x = 0; x < ( FONT_CELL_W * scale ); x++ )
        {
            top +=
                font_alpha( 'H', scale, x, 0 ) + font_alpha( 'H', scale, x, 1 );
            bottom += font_alpha( 'H', scale, x, last_row );
        }
        CHECK( top > 0u );
        CHECK( bottom > 0u );
    }
}

static void test_blend_ends_and_clamp( void )
{
    CHECK_EQ( font_blend( 0xF800u, 0x001Fu, 0u ), 0x001Fu );
    CHECK_EQ( font_blend( 0xF800u, 0x001Fu, FONT_ALPHA_MAX ), 0xF800u );
    CHECK_EQ( font_blend( 0xF800u, 0x001Fu, 200u ), 0xF800u ); /* the most */
}

/* The same color over itself stays that color, whatever the alpha. */
static void test_blend_of_one_color_is_that_color( void )
{
    const uint16_t colors[] = {
        0x0000u, 0xFFFFu, 0xEF5Bu, 0x632Cu, 0xF922u, 0x9CD3u, 0x0821u
    };

    for ( size_t i = 0; i < ( sizeof( colors ) / sizeof( colors[0] ) ); i++ )
    {
        for ( uint8_t a = 0; a <= FONT_ALPHA_MAX; a++ )
        {
            CHECK_EQ( font_blend( colors[i], colors[i], a ), colors[i] );
        }
    }
}

/* Black to white: each channel is its share of the full value, rounded. */
static void test_blend_midpoints( void )
{
    for ( uint8_t a = 0; a <= FONT_ALPHA_MAX; a++ )
    {
        uint16_t got = font_blend( 0xFFFFu, 0x0000u, a );
        unsigned r   = ( got >> 11 ) & 0x1Fu;
        unsigned g   = ( got >> 5 ) & 0x3Fu;
        unsigned b   = got & 0x1Fu;

        CHECK_EQ( r, ( 31u * a + 7u ) / 15u );
        CHECK_EQ( g, ( 63u * a + 7u ) / 15u );
        CHECK_EQ( b, ( 31u * a + 7u ) / 15u );
    }
}

/* More ink never moves a channel away from the foreground. */
static void test_blend_is_monotonic( void )
{
    unsigned last_r = 0u;
    unsigned last_g = 0u;
    unsigned last_b = 0u;

    for ( uint8_t a = 0; a <= FONT_ALPHA_MAX; a++ )
    {
        uint16_t got = font_blend( 0xFFFFu, 0x0000u, a );

        CHECK( ( ( got >> 11 ) & 0x1Fu ) >= last_r );
        CHECK( ( ( got >> 5 ) & 0x3Fu ) >= last_g );
        CHECK( ( got & 0x1Fu ) >= last_b );
        last_r = ( got >> 11 ) & 0x1Fu;
        last_g = ( got >> 5 ) & 0x3Fu;
        last_b = got & 0x1Fu;
    }
}

int main( void )
{
    test_every_size_the_ui_asks_for_has_bitmaps();
    test_every_character_has_ink();
    test_unknown_and_lowercase();
    test_zero_and_capital_o_differ();
    test_no_ink_outside_the_cell_or_without_bitmaps();
    test_capitals_fill_the_cell_height();
    test_blend_ends_and_clamp();
    test_blend_of_one_color_is_that_color();
    test_blend_midpoints();
    test_blend_is_monotonic();
    return MT_RESULT();
}
