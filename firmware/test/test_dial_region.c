/* The needle is redrawn in a small box around where it was and where it is
 * going. If the box is too small, pixels of the old needle stay on the
 * panel. This includes dial.c so it can draw a full frame at any angle, then
 * checks that moving the needle by a partial redraw gives exactly the
 * same pixels as drawing the frame fresh at the new angle. The risk is
 * highest when the needle is near vertical or horizontal, where the box is
 * thinnest, so the angles below include every one of those. */
#include <stdio.h>
#include <string.h>

#include "lib/ui/dial.c"
#include "test/minitest.h"

static uint16_t shadow[DISPLAY_HEIGHT][DISPLAY_WIDTH];
static uint16_t partial[DISPLAY_HEIGHT][DISPLAY_WIDTH];

hal_status_t display_blit(
    uint16_t x,
    uint16_t y,
    uint16_t w,
    uint16_t h,
    const uint16_t * pixels
)
{
    for ( uint16_t row = 0; row < h; row++ )
    {
        for ( uint16_t col = 0; col < w; col++ )
        {
            shadow[y + row][x + col] = pixels[( ( size_t )row * w ) + col];
        }
    }
    return HAL_OK;
}

hal_status_t display_clear( uint16_t color_rgb565 )
{
    for ( uint16_t row = 0; row < DISPLAY_HEIGHT; row++ )
    {
        for ( uint16_t col = 0; col < DISPLAY_WIDTH; col++ )
        {
            shadow[row][col] = color_rgb565;
        }
    }
    return HAL_OK;
}

/* One value per degree of sweep, so value v is exactly angle 135 + v. */
static const dial_cfg_t cfg = {
    0,
    300,
    100,
    1u,
    1,
    "TEST",
    "UNIT",
    NULL,
    0xEF5Bu,
    0x632Cu,
    0xF922u,
    900u,
    DIAL_STYLE_FULL_SWEEP
};

static uint32_t now = 1000u;

static void full_frame_at(
    dial_widget_t * d,
    int16_t deg
)
{
    d->displayed_deg = deg;
    d->target_deg    = deg;
    dial_motion_init( &d->motion, deg, now );
    draw_frame( d );
}

/* Pixels where the saved partial redraw differs from the fresh frame now
 * on the panel. */
static unsigned differences( void )
{
    unsigned bad = 0;

    for ( uint16_t row = 0; row < DISPLAY_HEIGHT; row++ )
    {
        for ( uint16_t col = 0; col < DISPLAY_WIDTH; col++ )
        {
            if ( shadow[row][col] != partial[row][col] )
            {
                bad++;
            }
        }
    }
    return bad;
}

/* Draws at from, moves to to over as many frames as it takes, and compares with
 * a fresh frame at to. Returns the number of stale or wrong pixels. */
static unsigned move_and_compare(
    dial_widget_t * d,
    int16_t from,
    int16_t to
)
{
    unsigned polls = 0u;

    full_frame_at( d, from );
    /* The move takes several frames; every partial redraw on the way must
     * leave the panel right, not just the last. */

    do
    {
        now += 33u;
        dial_poll( d, now, true, to - 135 );
        polls++;
    } while ( !dial_settled( d ) && ( polls < 200u ) );
    ( void )memcpy( partial, shadow, sizeof( shadow ) );
    full_frame_at( d, to );
    return differences();
}

static void test_partial_redraw_matches_a_fresh_frame( void )
{
    /* Around the vertical (270), the horizontals (180 and 360) and the
     * diagonals, where the box is thinnest or the needle crosses ticks. */
    static const int16_t starts[] = {
        135, 165, 180, 195, 225, 255, 270, 285, 315, 345, 360, 375, 405
    };
    static const int16_t offsets[] = { -30, -6, -1, 1, 6, 30 };
    dial_widget_t d;
    unsigned total     = 0;
    int16_t worst_from = 0;
    unsigned worst     = 0;

    dial_init( &d, &cfg, now );
    for ( size_t s = 0; s < ( sizeof( starts ) / sizeof( starts[0] ) ); s++ )
    {
        for ( size_t i = 0; i < ( sizeof( offsets ) / sizeof( offsets[0] ) );
              i++ )
        {
            int16_t to = ( int16_t )( starts[s] + offsets[i] );

            if ( ( to >= 135 ) && ( to <= 435 ) )
            {
                unsigned bad = move_and_compare( &d, starts[s], to );

                total += bad;
                if ( bad > worst )
                {
                    worst      = bad;
                    worst_from = starts[s];
                }
            }
        }
    }
    if ( total != 0u )
    {
        ( void )fprintf(
            stderr,
            "%u stale pixels in total, worst %u moving from %d degrees\n",
            total,
            worst,
            ( int )worst_from
        );
    }
    CHECK_EQ( total, 0u );
}

/* The label above the center is centered using the width it is drawn at.
 * If the width is worked out at one text size and drawn at another, the
 * label sits off center. */
static void test_fixed_label_is_centered_at_its_drawn_size( void )
{
    dial_label_t labels[DIAL_MAX_LABELS];
    size_t count = 0;
    int32_t drawn_width;
    int32_t middle;

    add_fixed_labels( &cfg, labels, &count );
    CHECK( count > 0u );
    drawn_width = label_width( strlen( labels[0].text ), labels[0].scale );
    middle      = labels[0].x + ( drawn_width / 2 );
    CHECK( ( middle >= ( DIAL_CX - 1 ) ) && ( middle <= ( DIAL_CX + 1 ) ) );
}

/* A scale in thousandths still labels its ticks in whole units. */
static void test_tick_numbers_use_label_div( void )
{
    static const dial_cfg_t fine = {
        0,
        8000,
        1000,
        0u,
        1000,
        "RPM",
        NULL,
        NULL,
        1u,
        2u,
        3u,
        900u,
        DIAL_STYLE_FULL_SWEEP
    };
    dial_tick_t ticks[DIAL_MAX_TICKS];
    dial_label_t labels[DIAL_MAX_LABELS];
    size_t tick_count  = 0;
    size_t label_count = 0;

    build_ticks( &fine, ticks, &tick_count, labels, &label_count );
    CHECK_EQ( label_count, 8u ); /* 1 to 8, with none for the zero */
    CHECK( strcmp( labels[0].text, "1" ) == 0 );
    CHECK( strcmp( labels[7].text, "8" ) == 0 );
}

/* Upper scales share the needle, and every partial redraw must agree with
 * a fresh frame, including when it crosses a centered title or unit. */
static void test_upper_arc( void )
{
    static const dial_cfg_t upper = {
        1000,
        2600,
        400,
        1u,
        10,
        "COOLANT",
        "F",
        NULL,
        0xEF5Bu,
        0x632Cu,
        0xF922u,
        900u,
        DIAL_STYLE_UPPER_ARC
    };
    dial_tick_t ticks[DIAL_MAX_TICKS];
    dial_label_t labels[DIAL_MAX_LABELS];
    size_t nt;
    size_t nl;
    dial_widget_t d;

    CHECK_EQ( value_to_deg( &upper, 1000 ), -75 );
    CHECK_EQ( value_to_deg( &upper, 1800 ), 0 );
    CHECK_EQ( value_to_deg( &upper, 2600 ), 75 );
    CHECK_EQ( value_to_deg( &upper, 0 ), -75 );
    CHECK_EQ( value_to_deg( &upper, 3000 ), 75 );
    build_ticks( &upper, ticks, &nt, labels, &nl );
    CHECK_EQ( nt, 9u );
    CHECK_EQ( nl, 5u );
    CHECK( strcmp( labels[0].text, "100" ) == 0 );
    CHECK( strcmp( labels[4].text, "260" ) == 0 );
    for ( size_t i = 0; i < nt; i++ )
    {
        CHECK( ( ticks[i].deg >= -75 ) && ( ticks[i].deg <= 75 ) );
    }
    dial_init( &d, &upper, now );
    for ( int16_t deg = -75; deg < 75; deg++ )
    {
        int16_t old     = d.displayed_deg;
        d.displayed_deg = deg;
        redraw_needle( &d, old );
        ( void )memcpy( partial, shadow, sizeof( shadow ) );
        draw_frame( &d );
        CHECK_EQ( differences(), 0u );
    }
}

int main( void )
{
    test_fixed_label_is_centered_at_its_drawn_size();
    test_partial_redraw_matches_a_fresh_frame();
    test_tick_numbers_use_label_div();
    test_upper_arc();
    return MT_RESULT();
}
