/* Check redraw pacing, idle behavior, and needle movement. */
#include "lib/ui/dial.h"
#include "lib/display/display.h"
#include "test/minitest.h"

static unsigned blit_calls;
static unsigned blit_rows;
static unsigned clear_calls;
static uint16_t last_blit_w;
static uint16_t last_blit_h;

/* Mirrors every blit into absolute panel coordinates, so a test can ask
 * "what color is actually on screen at (row, col) right now" regardless
 * of whether it got there via a full draw_frame or a small draw_region. */
#define SHADOW_DIM ( 240u )
static uint16_t shadow[SHADOW_DIM][SHADOW_DIM];

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
    last_blit_w = w;
    last_blit_h = h;
    blit_calls++;
    blit_rows += h;
    return HAL_OK;
}

hal_status_t display_clear( uint16_t color_rgb565 )
{
    for ( uint16_t row = 0; row < SHADOW_DIM; row++ )
    {
        for ( uint16_t col = 0; col < SHADOW_DIM; col++ )
        {
            shadow[row][col] = color_rgb565;
        }
    }
    clear_calls++;
    return HAL_OK;
}

/* True if any pixel in [y0, y1) x [x0, x1) currently shows color. */
static bool shadow_has_color(
    uint16_t y0,
    uint16_t y1,
    uint16_t x0,
    uint16_t x1,
    uint16_t color
)
{
    bool found = false;

    for ( uint16_t row = y0; ( !found ) && ( row < y1 ); row++ )
    {
        for ( uint16_t col = x0; ( !found ) && ( col < x1 ); col++ )
        {
            found = ( shadow[row][col] == color );
        }
    }
    return found;
}

#define PANEL_H                                                                \
    ( 240u ) /* one blit per row, only for the full-panel draw_frame           \
              * path (dial_init, and a box-too-big fallback) */

/* 0..100 over the full 135..435 sweep, with no face label or units. Its top
 * speed is 300 degrees per second, a full sweep in one second. */
static const dial_cfg_t test_cfg = {
    .min_value       = 0,
    .max_value       = 100,
    .major_step      = 25,
    .minor_per_major = 0u,
    .label_div       = 1,
    .face_bg         = 1u,
    .tick_fg         = 2u,
    .needle_fg       = 3u,
    .max_deg_per_s   = 300u,
    .style           = DIAL_STYLE_FULL_SWEEP
};

static void reset( void )
{
    blit_calls  = 0;
    blit_rows   = 0;
    clear_calls = 0;
}

static void test_init_draws_one_frame_at_rest( void )
{
    dial_widget_t d;

    reset();
    dial_init( &d, &test_cfg, 0u );
    CHECK_EQ( clear_calls, 1u );
    /* Every row of the face is sent once, in a few big blits and not one per
     * row, so the panel's window is set a few times. */
    CHECK_EQ( blit_rows, PANEL_H );
    CHECK( ( blit_calls >= 1u ) && ( blit_calls <= 5u ) );
}

static void test_idle_poll_blits_nothing( void )
{
    dial_widget_t d;

    dial_init( &d, &test_cfg, 0u );
    reset();

    /* Same value, no time elapsed: nothing to move, nothing to draw. */
    dial_poll( &d, 0u, true, 0 );
    CHECK_EQ( blit_calls, 0u );

    /* Time elapsing alone, with no value change, still shouldn't redraw:
     * the needle has nowhere to go. */
    dial_poll( &d, 1000u, true, 0 );
    CHECK_EQ( blit_calls, 0u );
}

static void test_value_change_within_frame_interval_waits( void )
{
    dial_widget_t d;

    dial_init( &d, &test_cfg, 0u );
    reset();

    /* Target jumps to max, but no time has passed yet: must not redraw
     * before its own frame interval, so the sweep stays paced. */
    dial_poll( &d, 0u, true, 100 );
    CHECK_EQ( blit_calls, 0u );
}

/* Polls every frame interval until the needle settles. Returns the number
 * of polls it took, or 0 if it never settled within limit polls. */
static unsigned settle(
    dial_widget_t * d,
    uint32_t * now,
    bool has_value,
    int32_t value,
    unsigned limit
)
{
    unsigned polls = 0u;

    do
    {
        *now += 33u;
        dial_poll( d, *now, has_value, value );
        polls++;
    } while ( !dial_settled( d ) && ( polls < limit ) );
    return dial_settled( d ) ? polls : 0u;
}

static void test_big_jump_sweeps_over_multiple_frames( void )
{
    dial_widget_t d;
    uint32_t now = 0u;
    unsigned polls;

    dial_init( &d, &test_cfg, now );
    reset();

    /* Full-scale jump: a real sweep, not a teleport. At 300 degrees per
     * second the 300 degrees take about a second, plus a short tail while
     * the needle closes on the value. Each redraw is one dirty-rect blit
     * (see draw_region), not one per row. */
    polls = settle( &d, &now, true, 100, 100u );
    CHECK( polls >= 30u );
    CHECK( polls <= 45u );
    CHECK( blit_calls > 1u );
    CHECK( blit_calls <= polls );
    CHECK_EQ( d.displayed_deg, d.target_deg );

    /* The dirty rect actually is a small fraction of the panel, not the
     * whole face. */
    CHECK( last_blit_w < ( ( PANEL_H * 3u ) / 4u ) );
    CHECK( last_blit_h < ( ( PANEL_H * 3u ) / 4u ) );

    /* Settled: further polls at the same value blit nothing more. */
    blit_calls = 0u;
    dial_poll( &d, now + 1000u, true, 100 );
    CHECK_EQ( blit_calls, 0u );
}

static void test_value_loss_sweeps_back_to_rest( void )
{
    dial_widget_t d;
    uint32_t now = 0u;

    dial_init( &d, &test_cfg, now );
    /* Drive it to max first. */
    CHECK( settle( &d, &now, true, 100, 100u ) > 0u );
    CHECK_EQ( d.displayed_deg, d.target_deg );

    /* ECU value disappears: must sweep back to min_value's angle, not
     * freeze at the last reading. */
    reset();
    CHECK( settle( &d, &now, false, 0, 100u ) > 0u );
    CHECK( blit_calls > 1u ); /* took more than one redraw */
    CHECK_EQ( d.displayed_deg, d.target_deg );
    CHECK_EQ( d.target_deg, 135 );
}

static void test_frames_come_at_a_steady_interval( void )
{
    dial_widget_t d;
    uint32_t stamps[20];
    unsigned found = 0u;
    unsigned seen;

    dial_init( &d, &test_cfg, 0u );
    reset();
    seen = blit_calls;
    /* Polled every millisecond, as the main loop does. */
    for ( uint32_t t = 1u; ( t < 1500u ) && ( found < 20u ); t++ )
    {
        dial_poll( &d, t, true, 100 );
        if ( blit_calls != seen )
        {
            seen            = blit_calls;
            stamps[found++] = t;
        }
    }
    CHECK_EQ( found, 20u );
    for ( unsigned i = 1u; i < found; i++ )
    {
        CHECK_EQ( stamps[i] - stamps[i - 1u], 33u );
    }
}

static void test_needle_draws_over_label_and_uncovers_it( void )
{
    /* TURBO at scale 1, centered above center: the face label sits at
     * rows [89, 96), cols [105, 135) (5 chars * 6 px each). Value 75 on
     * a 0..100 scale is deg 360, straight up, so the needle is a vertical
     * stripe through col 120 and (row 92, col 120) sits inside both the
     * label's box and the needle. No font-bitmap knowledge is needed,
     * since the needle covers every pixel along its stripe regardless of
     * what glyph pixel is under it. */
    static const dial_cfg_t cfg = {
        .min_value       = 0,
        .max_value       = 100,
        .major_step      = 25,
        .minor_per_major = 0u,
        .label_div       = 1,
        .label           = "TURBO",
        .face_bg         = 0x1111u,
        .tick_fg         = 0x2222u,
        .needle_fg       = 0x3333u,
        .max_deg_per_s   = 150u,
        .style           = DIAL_STYLE_FULL_SWEEP
    };
    const uint16_t label_y0 = 74u;
    const uint16_t label_y1 = 82u;
    const uint16_t label_x0 = 105u;
    const uint16_t label_x1 = 135u;
    dial_widget_t d;
    uint32_t now = 0u;

    dial_init( &d, &cfg, now );
    CHECK(
        shadow_has_color( label_y0, label_y1, label_x0, label_x1, 0x2222u )
    );
    CHECK(
        !shadow_has_color( label_y0, label_y1, label_x0, label_x1, 0x3333u )
    );

    /* Sweep straight up at value 75, one step at a time, until the needle
     * actually reaches it. do-while: target_deg only updates inside
     * dial_poll, so it must run at least once before the target is
     * known. */
    do
    {
        now += 33u;
        dial_poll( &d, now, true, 75 );
    } while ( d.displayed_deg != d.target_deg );
    CHECK_EQ( d.target_deg, 360 );
    CHECK( shadow[80][120] == 0x3333u ); /* needle drawn over the label */

    /* Keep sweeping past it: the label must reappear, with no needle
     * pixel left behind at the spot it just vacated. */
    do
    {
        now += 33u;
        dial_poll( &d, now, true, 100 );
    } while ( d.displayed_deg != d.target_deg );
    CHECK_EQ( d.target_deg, 435 );
    CHECK( shadow[80][120] != 0x3333u );
    CHECK(
        shadow_has_color( label_y0, label_y1, label_x0, label_x1, 0x2222u )
    );
}

/* A scale in thousandths keeps the digits a whole-unit one would drop: 3000
 * and 3975 rpm are different needle positions, not both the 3 mark. */
static void test_a_fine_scale_keeps_its_precision( void )
{
    static const dial_cfg_t cfg = {
        .min_value       = 0,
        .max_value       = 8000,
        .major_step      = 1000,
        .minor_per_major = 0u,
        .label_div       = 1000,
        .label           = "RPM",
        .unit_pos        = "x1000",
        .face_bg         = 1u,
        .tick_fg         = 2u,
        .needle_fg       = 3u,
        .max_deg_per_s   = 900u,
        .style           = DIAL_STYLE_FULL_SWEEP
    };
    dial_widget_t d;
    int16_t at_3000;

    dial_init( &d, &cfg, 0u );
    dial_poll( &d, 0u, true, 3000 );
    at_3000 = d.target_deg;
    dial_poll( &d, 0u, true, 3975 );
    CHECK( d.target_deg > at_3000 );
    CHECK_EQ( at_3000, 135 + ( 3000 * 300 / 8000 ) );
    CHECK_EQ( d.target_deg, 135 + ( 3975 * 300 / 8000 ) );
}

/* On the upper arc the lowest reading is a negative angle. Driven through the
 * real motion, the needle must stop exactly on it, and on the highest too. */
static void test_upper_arc_settles_on_both_ends( void )
{
    static const dial_cfg_t arc = {
        .min_value       = 1000,
        .max_value       = 2600,
        .major_step      = 400,
        .minor_per_major = 1u,
        .label_div       = 10,
        .label           = "COOLANT",
        .unit_pos        = "F",
        .face_bg         = 1u,
        .tick_fg         = 2u,
        .needle_fg       = 3u,
        .max_deg_per_s   = 900u,
        .style           = DIAL_STYLE_UPPER_ARC
    };
    dial_widget_t d;
    uint32_t now = 0u;

    dial_init( &d, &arc, now );
    CHECK_EQ( d.displayed_deg, -45 ); /* at rest: the lowest value */
    CHECK( settle( &d, &now, true, 2600, 200u ) > 0u );
    CHECK_EQ( d.displayed_deg, 45 );
    CHECK( settle( &d, &now, true, 1000, 200u ) > 0u );
    CHECK_EQ( d.target_deg, -45 );
    CHECK_EQ( d.displayed_deg, -45 ); /* exactly, not one degree short */
    /* A value part way up the arc, still below zero: 400 of 1600 is 37 of
     * 90 degrees, so -45 + 22. */
    CHECK( settle( &d, &now, true, 1400, 200u ) > 0u );
    CHECK_EQ( d.displayed_deg, -23 );
}

static void test_two_sided_scale_and_rest( void )
{
    static const dial_cfg_t cfg = {
        .min_value       = -30,
        .max_value       = 20,
        .major_step      = 10,
        .minor_per_major = 4u,
        .label_div       = 1,
        .label           = "TURBO",
        .unit_pos        = "PSI",
        .unit_neg        = "INHG",
        .face_bg         = 1u,
        .tick_fg         = 2u,
        .needle_fg       = 3u,
        .max_deg_per_s   = 900u,
        .style           = DIAL_STYLE_FULL_SWEEP
    };
    dial_widget_t d;

    dial_init( &d, &cfg, 0u );
    CHECK_EQ( d.displayed_deg, 270 );

    /* Below zero 30 units take the quarter turn from 9 o'clock down to 6
     * (3 degrees each). Above zero 20 take the half turn up to 3 o'clock
     * (9 degrees each). */
    dial_poll( &d, 0u, true, -30 );
    CHECK_EQ( d.target_deg, 180 );
    dial_poll( &d, 0u, true, 0 );
    CHECK_EQ( d.target_deg, 270 );
    dial_poll( &d, 0u, true, 20 );
    CHECK_EQ( d.target_deg, 450 );
    dial_poll( &d, 0u, true, -10 );
    CHECK_EQ( d.target_deg, 240 );
    dial_poll( &d, 0u, true, 10 );
    CHECK_EQ( d.target_deg, 360 );
    dial_poll( &d, 0u, false, 0 );
    CHECK_EQ( d.target_deg, 270 );
}

static void test_rpm_face_scale( void )
{
    static const dial_cfg_t cfg = {
        .min_value       = 0,
        .max_value       = 8,
        .major_step      = 1,
        .minor_per_major = 0u,
        .label_div       = 1,
        .label           = "RPM",
        .unit_pos        = "x1000",
        .face_bg         = 1u,
        .tick_fg         = 2u,
        .needle_fg       = 3u,
        .max_deg_per_s   = 900u,
        .style           = DIAL_STYLE_FULL_SWEEP
    };
    dial_widget_t d;

    dial_init( &d, &cfg, 0u );
    CHECK_EQ( d.displayed_deg, 135 );
    dial_poll( &d, 0u, true, 4 );
    CHECK_EQ( d.target_deg, 285 );
    dial_poll( &d, 0u, true, 8 );
    CHECK_EQ( d.target_deg, 435 );
    dial_poll( &d, 0u, false, 0 );
    CHECK_EQ( d.target_deg, 135 );
}

/* A reading past either end of the scale puts the needle on that end, as a
 * real gauge's stop pin would, and the needle never travels past it on the
 * way, whatever the style and however wild the number. */
static void check_beyond_the_ends(
    const dial_cfg_t * cfg,
    int16_t low_deg,
    int16_t high_deg
)
{
    const int32_t above[] = {
        cfg->max_value + 1, cfg->max_value + 1000000, INT32_MAX
    };
    const int32_t below[] = {
        cfg->min_value - 1, cfg->min_value - 1000000, INT32_MIN
    };

    for ( size_t i = 0; i < 3u; i++ )
    {
        dial_widget_t d;
        uint32_t now  = 0u;
        int16_t most  = low_deg;
        int16_t least = high_deg;

        dial_init( &d, cfg, now );
        dial_poll( &d, now, true, above[i] );
        CHECK_EQ( d.target_deg, high_deg );
        for ( unsigned f = 0; ( f < 200u ) && !dial_settled( &d ); f++ )
        {
            now += 33u;
            dial_poll( &d, now, true, above[i] );
            most = ( d.displayed_deg > most ) ? d.displayed_deg : most;
        }
        CHECK( dial_settled( &d ) );
        CHECK_EQ( d.displayed_deg, high_deg );
        CHECK( most <= high_deg );

        dial_poll( &d, now, true, below[i] );
        CHECK_EQ( d.target_deg, low_deg );
        for ( unsigned f = 0; ( f < 200u ) && !dial_settled( &d ); f++ )
        {
            now += 33u;
            dial_poll( &d, now, true, below[i] );
            least = ( d.displayed_deg < least ) ? d.displayed_deg : least;
        }
        CHECK( dial_settled( &d ) );
        CHECK_EQ( d.displayed_deg, low_deg );
        CHECK( least >= low_deg );
    }
}

static void test_a_reading_past_the_scale_stops_at_its_end( void )
{
    /* One sided, over the full sweep. */
    check_beyond_the_ends( &test_cfg, 135, 435 );
    {
        /* Two sided: lowest at 6 o'clock, highest at 3 o'clock. */
        static const dial_cfg_t two = {
            .min_value       = -30,
            .max_value       = 20,
            .major_step      = 10,
            .minor_per_major = 4u,
            .label_div       = 1,
            .label           = "T",
            .unit_pos        = "P",
            .unit_neg        = "I",
            .face_bg         = 1u,
            .tick_fg         = 2u,
            .needle_fg       = 3u,
            .max_deg_per_s   = 900u,
            .style           = DIAL_STYLE_FULL_SWEEP
        };

        check_beyond_the_ends( &two, 180, 450 );
    }
    {
        /* Two sided with a longer positive side: the same two end angles. */
        static const dial_cfg_t wide = {
            .min_value       = -10,
            .max_value       = 50,
            .major_step      = 10,
            .minor_per_major = 0u,
            .label_div       = 1,
            .label           = "T",
            .unit_pos        = "P",
            .unit_neg        = "I",
            .face_bg         = 1u,
            .tick_fg         = 2u,
            .needle_fg       = 3u,
            .max_deg_per_s   = 900u,
            .style           = DIAL_STYLE_FULL_SWEEP
        };

        check_beyond_the_ends( &wide, 180, 450 );
    }
    {
        static const dial_cfg_t arc = {
            .min_value       = 1000,
            .max_value       = 2600,
            .major_step      = 400,
            .minor_per_major = 1u,
            .label_div       = 10,
            .label           = "C",
            .unit_pos        = "F",
            .face_bg         = 1u,
            .tick_fg         = 2u,
            .needle_fg       = 3u,
            .max_deg_per_s   = 900u,
            .style           = DIAL_STYLE_UPPER_ARC
        };

        check_beyond_the_ends( &arc, -45, 45 );
    }
}

/* An empty scale has nowhere to sweep, so every reading rests at its start. */
static void test_an_empty_scale_rests_at_its_start( void )
{
    static const dial_cfg_t empty = {
        .min_value       = 50,
        .max_value       = 50,
        .major_step      = 25,
        .minor_per_major = 0u,
        .label_div       = 1,
        .label           = "E",
        .unit_pos        = "U",
        .face_bg         = 1u,
        .tick_fg         = 2u,
        .needle_fg       = 3u,
        .max_deg_per_s   = 900u,
        .style           = DIAL_STYLE_FULL_SWEEP
    };
    const int32_t values[] = { INT32_MIN, 0, 49, 50, 51, INT32_MAX };
    dial_widget_t d;

    dial_init( &d, &empty, 0u );
    for ( size_t i = 0; i < 6u; i++ )
    {
        dial_poll( &d, 33u * ( uint32_t )( i + 1u ), true, values[i] );
        CHECK_EQ( d.target_deg, 135 );
    }
}

int main( void )
{
    test_init_draws_one_frame_at_rest();
    test_idle_poll_blits_nothing();
    test_value_change_within_frame_interval_waits();
    test_big_jump_sweeps_over_multiple_frames();
    test_value_loss_sweeps_back_to_rest();
    test_frames_come_at_a_steady_interval();
    test_needle_draws_over_label_and_uncovers_it();
    test_a_fine_scale_keeps_its_precision();
    test_upper_arc_settles_on_both_ends();
    test_two_sided_scale_and_rest();
    test_rpm_face_scale();
    test_a_reading_past_the_scale_stops_at_its_end();
    test_an_empty_scale_rests_at_its_start();

    return MT_RESULT();
}
