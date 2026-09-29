/* Check redraw pacing, idle behavior, and needle movement. */
#include "dial.h"
#include "display.h"
#include "minitest.h"

static unsigned blit_calls;
static unsigned clear_calls;
static uint16_t last_blit_w;
static uint16_t last_blit_h;

/* Mirrors every blit into absolute panel coordinates, so a test can ask
 * "what color is actually on screen at (row, col) right now" regardless
 * of whether it got there via a full draw_frame or a small draw_region. */
#define SHADOW_DIM 240u
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
    240u /* one blit per row, only for the full-panel draw_frame               \
          * path (dial_init, and a box-too-big fallback) */

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
    uint32_t now = 0u;

    dial_init( &d, &test_cfg, now );
    reset();

    /* Full-scale jump, one frame interval apart each time: must take
     * more than one redraw to get there (a real sweep, not a teleport),
     * and exactly 30 (300 degrees / 10 per frame). Each redraw is one
     * dirty-rect blit now (see draw_region), not one per row. */
    for ( int i = 0; i < 40; i++ )
    {
        now += 33u;
        dial_poll( &d, now, true, 100 );
    }
    CHECK_EQ( blit_calls, 30u );
    CHECK_EQ( d.displayed_deg, d.target_deg );

    /* The dirty rect actually is a small fraction of the panel, not the
     * whole face. */
    CHECK( last_blit_w < ( PANEL_H / 2u ) );
    CHECK( last_blit_h < ( PANEL_H / 2u ) );

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
    CHECK( blit_calls > 1u ); /* took more than one redraw */
    CHECK_EQ( d.displayed_deg, d.target_deg );
    CHECK_EQ( d.target_deg, 135 );
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
        0,
        100,
        25,
        0u,
        "TURBO",
        NULL,
        NULL,
        0x1111u,
        0x2222u,
        0x3333u,
        5u,
        DIAL_STYLE_DEFAULT
    };
    const uint16_t label_y0 = 89u;
    const uint16_t label_y1 = 96u;
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
    CHECK( shadow[92][120] == 0x3333u ); /* needle drawn over the label */

    /* Keep sweeping past it: the label must reappear, with no needle
     * pixel left behind at the spot it just vacated. */
    do
    {
        now += 33u;
        dial_poll( &d, now, true, 100 );
    } while ( d.displayed_deg != d.target_deg );
    CHECK_EQ( d.target_deg, 435 );
    CHECK( shadow[92][120] != 0x3333u );
    CHECK(
        shadow_has_color( label_y0, label_y1, label_x0, label_x1, 0x2222u )
    );
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
    test_needle_draws_over_label_and_uncovers_it();
    test_default_scale_and_rest();
    test_rpm_face_scale();

    return MT_RESULT();
}
