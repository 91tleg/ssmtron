/* Check redraw pacing, idle behavior, and needle movement. */
#include "dial.h"
#include "display.h"
#include "minitest.h"

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

#define PANEL_H 240u /* one blit per row: see dial.c's draw_frame */

/* 0..100 over the full 135..435 sweep, with no face label or units. 300
 * degrees / 10 per frame = exactly 30 frames end to end. */
static const dial_cfg_t test_cfg = {
    0, 100, 25, 0u, NULL, NULL, NULL, 1u, 2u, 3u, 10u, DIAL_STYLE_DEFAULT
};

static void reset( void )
{
    blit_calls  = 0;
    clear_calls = 0;
}

static void test_init_draws_one_frame_at_rest( void )
{
    dial_widget_t d;

    reset();
    dial_init( &d, &test_cfg, 0u );
    CHECK_EQ( clear_calls, 1u );
    CHECK_EQ( blit_calls, PANEL_H );
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

static void test_big_jump_sweeps_over_multiple_frames( void )
{
    dial_widget_t d;
    uint32_t now     = 0u;
    unsigned redraws = 0u;

    dial_init( &d, &test_cfg, now );
    reset();

    /* Full-scale jump, one frame interval apart each time: must take
     * more than one redraw to get there (a real sweep, not a teleport),
     * and exactly 30 (300 degrees / 10 per frame). */
    for ( int i = 0; i < 40; i++ )
    {
        now += 33u;
        dial_poll( &d, now, true, 100 );
        if ( blit_calls != ( redraws * PANEL_H ) )
        {
            redraws = blit_calls / PANEL_H;
        }
    }
    CHECK_EQ( redraws, 30u );
    CHECK_EQ( d.displayed_deg, d.target_deg );

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
    for ( int i = 0; i < 30; i++ )
    {
        now += 33u;
        dial_poll( &d, now, true, 100 );
    }
    CHECK_EQ( d.displayed_deg, d.target_deg );

    /* ECU value disappears: must sweep back to min_value's angle, not
     * freeze at the last reading. */
    reset();
    for ( int i = 0; i < 30; i++ )
    {
        now += 33u;
        dial_poll( &d, now, false, 0 );
    }
    CHECK( blit_calls > PANEL_H ); /* took more than one redraw */
    CHECK_EQ( d.displayed_deg, d.target_deg );
    CHECK_EQ( d.target_deg, 135 );
}

static void test_default_scale_and_rest( void )
{
    static const dial_cfg_t cfg = {
        -30,
        20,
        10,
        4u,
        "TURBO",
        "PSI",
        "INHG",
        1u,
        2u,
        3u,
        30u,
        DIAL_STYLE_DEFAULT
    };
    dial_widget_t d;

    dial_init( &d, &cfg, 0u );
    CHECK_EQ( d.displayed_deg, 270 );

    dial_poll( &d, 0u, true, -30 );
    CHECK_EQ( d.target_deg, 135 );
    dial_poll( &d, 0u, true, 0 );
    CHECK_EQ( d.target_deg, 270 );
    dial_poll( &d, 0u, true, 20 );
    CHECK_EQ( d.target_deg, 435 );
    dial_poll( &d, 0u, false, 0 );
    CHECK_EQ( d.target_deg, 270 );
}

static void test_rpm_face_scale( void )
{
    static const dial_cfg_t cfg = {
        0, 8, 1, 0u, "RPM", "x1000", NULL, 1u, 2u, 3u, 30u, DIAL_STYLE_DEFAULT
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

int main( void )
{
    test_init_draws_one_frame_at_rest();
    test_idle_poll_blits_nothing();
    test_value_change_within_frame_interval_waits();
    test_big_jump_sweeps_over_multiple_frames();
    test_value_loss_sweeps_back_to_rest();
    test_default_scale_and_rest();
    test_rpm_face_scale();

    return MT_RESULT();
}
