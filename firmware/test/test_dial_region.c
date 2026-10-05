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

/* A pixel is part of a shape when its center is inside the edge, which is
 * at least half covered. */
#define HALF_COVERED ( 8u )

static bool on_needle(
    int32_t dx,
    int32_t dy,
    int32_t sin_milli,
    int32_t cos_milli,
    int32_t len
)
{
    return dial_needle_alpha( dx, dy, sin_milli, cos_milli, len )
           >= HALF_COVERED;
}

static bool on_radial_line(
    int32_t dx,
    int32_t dy,
    int32_t sin_milli,
    int32_t cos_milli,
    int32_t inner_r,
    int32_t outer_r,
    int32_t half_w
)
{
    return dial_radial_line_alpha(
               dx, dy, sin_milli, cos_milli, inner_r, outer_r, half_w
           )
           >= HALF_COVERED;
}

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
    .min_value       = 0,
    .max_value       = 300,
    .major_step      = 100,
    .minor_per_major = 1u,
    .label_div       = 1,
    .label           = "TEST",
    .unit_pos        = "UNIT",
    .face_bg         = 0xEF5Bu,
    .tick_fg         = 0x632Cu,
    .needle_fg       = 0xF922u,
    .max_deg_per_s   = 900u,
    .style           = DIAL_STYLE_FULL_SWEEP
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

/* The face's labels are the numbers first, then the fixed words. */
static size_t number_count(
    const dial_cfg_t * c,
    const dial_face_t * f
)
{
    size_t fixed = 0u;

    fixed += ( c->label != NULL ) ? 1u : 0u;
    fixed += ( c->unit_pos != NULL ) ? 1u : 0u;
    fixed += ( c->unit_neg != NULL ) ? 1u : 0u;
    return f->label_count - fixed;
}

/* A scale in thousandths still labels its ticks in whole units. */
static void test_tick_numbers_use_label_div( void )
{
    static const dial_cfg_t fine = {
        .min_value       = 0,
        .max_value       = 8000,
        .major_step      = 1000,
        .minor_per_major = 0u,
        .label_div       = 1000,
        .label           = "RPM",
        .face_bg         = 1u,
        .tick_fg         = 2u,
        .needle_fg       = 3u,
        .max_deg_per_s   = 900u,
        .style           = DIAL_STYLE_FULL_SWEEP
    };
    dial_face_t face;

    dial_make_face( &fine, &face );
    CHECK_EQ(
        number_count( &fine, &face ), 9u
    ); /* 0 to 8: the zero is labeled too */
    CHECK( strcmp( face.labels[0].text, "0" ) == 0 );
    CHECK( strcmp( face.labels[1].text, "1" ) == 0 );
    CHECK( strcmp( face.labels[8].text, "8" ) == 0 );
}

/* The needle at the lowest and the highest value points at the middle of the
 * first and last long ticks: its tip lies on the tick's own line, inside it.
 * Checked for each kind of scale. */
static void test_needle_ends_line_up_with_the_scale_ends( void )
{
    static const dial_cfg_t cfgs[] = {
        { .min_value       = -30000,
          .max_value       = 30000,
          .major_step      = 10000,
          .minor_per_major = 4u,
          .label_div       = 1000,
          .label           = "TURBO",
          .unit_pos        = "PSI",
          .unit_neg        = "INHG",
          .face_bg         = 1u,
          .tick_fg         = 2u,
          .needle_fg       = 3u,
          .max_deg_per_s   = 900u,
          .style           = DIAL_STYLE_FULL_SWEEP },
        { .min_value       = 0,
          .max_value       = 8000,
          .major_step      = 1000,
          .minor_per_major = 4u,
          .label_div       = 1000,
          .label           = "RPM",
          .unit_pos        = "x1000",
          .face_bg         = 1u,
          .tick_fg         = 2u,
          .needle_fg       = 3u,
          .max_deg_per_s   = 900u,
          .style           = DIAL_STYLE_FULL_SWEEP },
        { .min_value       = 1000,
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
          .style           = DIAL_STYLE_UPPER_ARC },
    };

    for ( size_t c = 0; c < ( sizeof( cfgs ) / sizeof( cfgs[0] ) ); c++ )
    {
        dial_face_t face;
        int32_t ends[2] = { cfgs[c].min_value, cfgs[c].max_value };

        dial_make_face( &cfgs[c], &face );
        CHECK_EQ( face.ticks[0].deg, dial_value_to_deg( &cfgs[c], ends[0] ) );
        CHECK_EQ(
            face.ticks[face.tick_count - 1u].deg,
            dial_value_to_deg( &cfgs[c], ends[1] )
        );
        for ( size_t e = 0; e < 2u; e++ )
        {
            int16_t deg = dial_value_to_deg( &cfgs[c], ends[e] );
            int32_t sin_m;
            int32_t cos_m;
            int32_t len = dial_needle_length( &cfgs[c] );
            int32_t tx;
            int32_t ty;

            dial_angle_vec( deg, &sin_m, &cos_m );
            tx = ( len * sin_m ) / 1000;
            ty = -( len * cos_m ) / 1000;
            CHECK( on_needle( tx, ty, sin_m, cos_m, len ) );
            CHECK( on_radial_line(
                tx,
                ty,
                sin_m,
                cos_m,
                dial_tick_radius( &cfgs[c] ) - DIAL_TICK_MAJOR_LEN,
                dial_tick_radius( &cfgs[c] ),
                DIAL_TICK_MAJOR_HALF_W
            ) );
        }
    }
}

/* A scale that changes sign without its own unit keeps its minus signs. */
static void test_a_scale_without_a_vacuum_unit_keeps_minus_signs( void )
{
    static const dial_cfg_t signed_scale = {
        .min_value       = -30,
        .max_value       = 20,
        .major_step      = 10,
        .minor_per_major = 4u,
        .label_div       = 1,
        .label           = "T",
        .unit_pos        = "U",
        .face_bg         = 1u,
        .tick_fg         = 2u,
        .needle_fg       = 3u,
        .max_deg_per_s   = 900u,
        .style           = DIAL_STYLE_FULL_SWEEP
    };
    dial_face_t face;

    dial_make_face( &signed_scale, &face );
    CHECK( strcmp( face.labels[0].text, "-30" ) == 0 );
}

/* A tachometer scale with four small ticks between the numbers, 200 rpm
 * apart: 41 ticks, all of which fit. */
static void test_a_scale_with_small_ticks_fits_every_tick( void )
{
    static const dial_cfg_t tach = {
        .min_value       = 0,
        .max_value       = 8000,
        .major_step      = 1000,
        .minor_per_major = 4u,
        .label_div       = 1000,
        .label           = "RPM",
        .unit_pos        = "x1000",
        .face_bg         = 1u,
        .tick_fg         = 2u,
        .needle_fg       = 3u,
        .max_deg_per_s   = 900u,
        .style           = DIAL_STYLE_FULL_SWEEP
    };
    dial_face_t face;
    size_t majors = 0;

    dial_make_face( &tach, &face );
    CHECK_EQ( face.tick_count, 41u );
    CHECK_EQ( number_count( &tach, &face ), 9u );
    for ( size_t i = 0; i < face.tick_count; i++ )
    {
        majors += face.ticks[i].major ? 1u : 0u;
    }
    CHECK_EQ( majors, 9u );
    CHECK(
        face.ticks[face.tick_count - 1u].deg == 435
    ); /* the last tick is 8000 */
}

/* Upper scales share the needle, and every partial redraw must agree with
 * a fresh frame, including when it crosses a centered title or unit. */
static void test_upper_arc( void )
{
    static const dial_cfg_t upper = {
        .min_value       = 1000,
        .max_value       = 2600,
        .major_step      = 400,
        .minor_per_major = 1u,
        .label_div       = 10,
        .label           = "COOLANT",
        .unit_pos        = "F",
        .face_bg         = 0xEF5Bu,
        .tick_fg         = 0x632Cu,
        .needle_fg       = 0xF922u,
        .max_deg_per_s   = 900u,
        .style           = DIAL_STYLE_UPPER_ARC
    };
    dial_face_t face;
    dial_widget_t d;

    CHECK_EQ( dial_value_to_deg( &upper, 1000 ), -45 );
    CHECK_EQ( dial_value_to_deg( &upper, 1800 ), 0 );
    CHECK_EQ( dial_value_to_deg( &upper, 2600 ), 45 );
    CHECK_EQ( dial_value_to_deg( &upper, 0 ), -45 );
    CHECK_EQ( dial_value_to_deg( &upper, 3000 ), 45 );
    dial_make_face( &upper, &face );
    CHECK_EQ( face.tick_count, 9u );
    CHECK_EQ( number_count( &upper, &face ), 5u );
    CHECK( strcmp( face.labels[0].text, "100" ) == 0 );
    CHECK( strcmp( face.labels[4].text, "260" ) == 0 );
    for ( size_t i = 0; i < face.tick_count; i++ )
    {
        CHECK( ( face.ticks[i].deg >= -45 ) && ( face.ticks[i].deg <= 45 ) );
    }
    dial_init( &d, &upper, now );
    for ( int16_t deg = -45; deg < 45; deg++ )
    {
        int16_t old     = d.displayed_deg;
        d.displayed_deg = deg;
        redraw_needle( &d, old );
        ( void )memcpy( partial, shadow, sizeof( shadow ) );
        draw_frame( &d );
        CHECK_EQ( differences(), 0u );
    }
}

/* A coolant-like arc dial, three ticks between the numbers. */
static const dial_cfg_t coolant_like = {
    .min_value       = 1000,
    .max_value       = 2600,
    .major_step      = 400,
    .minor_per_major = 3u,
    .label_div       = 10,
    .label           = "COOLANT",
    .unit_pos        = "F",
    .face_bg         = 0xEF5Bu,
    .tick_fg         = 0x632Cu,
    .needle_fg       = 0xF922u,
    .max_deg_per_s   = 900u,
    .style           = DIAL_STYLE_UPPER_ARC
};

/* The ticks are only worked out for pixels in the tick ring, so no tick may
 * touch a pixel outside it. Checked against every tick of every pixel, for a
 * full sweep dial with small ticks, one with a negative side, and an arc. */
static void test_no_tick_reaches_outside_the_tick_ring( void )
{
    static const dial_cfg_t tach = {
        .min_value       = 0,
        .max_value       = 8000,
        .major_step      = 1000,
        .minor_per_major = 4u,
        .label_div       = 1000,
        .label           = "RPM",
        .unit_pos        = "x1000",
        .face_bg         = 1u,
        .tick_fg         = 2u,
        .needle_fg       = 3u,
        .max_deg_per_s   = 900u,
        .style           = DIAL_STYLE_FULL_SWEEP
    };
    static const dial_cfg_t boost = {
        .min_value       = -30000,
        .max_value       = 30000,
        .major_step      = 10000,
        .minor_per_major = 4u,
        .label_div       = 1000,
        .label           = "TURBO",
        .unit_pos        = "PSI",
        .unit_neg        = "INHG",
        .face_bg         = 1u,
        .tick_fg         = 2u,
        .needle_fg       = 3u,
        .max_deg_per_s   = 900u,
        .style           = DIAL_STYLE_FULL_SWEEP
    };
    const dial_cfg_t * cfgs[] = { &tach, &boost, &coolant_like };

    for ( size_t c = 0; c < ( sizeof( cfgs ) / sizeof( cfgs[0] ) ); c++ )
    {
        dial_face_t face;
        unsigned strays = 0u;

        dial_make_face( cfgs[c], &face );
        for ( int32_t dy = -DIAL_CY; dy < DIAL_CY; dy++ )
        {
            for ( int32_t dx = -DIAL_CX; dx < DIAL_CX; dx++ )
            {
                int32_t py  = dy - dial_pivot_down( cfgs[c] );
                int32_t d2  = ( dx * dx ) + ( py * py );
                bool inside = ( d2
                                >= ( dial_tick_ring_in( cfgs[c] )
                                     * dial_tick_ring_in( cfgs[c] ) ) )
                              && ( d2
                                   <= ( dial_tick_ring_out( cfgs[c] )
                                        * dial_tick_ring_out( cfgs[c] ) ) );

                for ( size_t t = 0; ( !inside ) && ( t < face.tick_count );
                      t++ )
                {
                    const dial_tick_t * tick = &face.ticks[t];

                    if ( dial_radial_line_alpha(
                             dx,
                             py,
                             tick->sin_m,
                             tick->cos_m,
                             dial_tick_radius( cfgs[c] )
                                 - ( tick->major ? DIAL_TICK_MAJOR_LEN
                                                 : DIAL_TICK_MINOR_LEN ),
                             dial_tick_radius( cfgs[c] ),
                             tick->major ? DIAL_TICK_MAJOR_HALF_W
                                         : DIAL_TICK_HALF_W
                         )
                         != 0u )
                    {
                        strays++;
                    }
                }
            }
        }
        CHECK_EQ( strays, 0u );
    }
}

/* A dial's face is worked out when it opens. Changing the dial's settings
 * afterwards changes nothing already drawn: the ticks and numbers are the
 * ones from the start. */
static void test_a_dial_face_is_made_when_the_dial_opens( void )
{
    dial_cfg_t mine = coolant_like;
    dial_face_t made;
    dial_widget_t d;

    dial_init( &d, &mine, now );
    dial_make_face( &mine, &made );
    CHECK_EQ( d.face.tick_count, made.tick_count );
    CHECK_EQ( d.face.label_count, made.label_count );
    for ( size_t t = 0; t < made.tick_count; t++ )
    {
        int32_t sin_m;
        int32_t cos_m;

        dial_angle_vec( made.ticks[t].deg, &sin_m, &cos_m );
        CHECK_EQ( made.ticks[t].sin_m, sin_m );
        CHECK_EQ( made.ticks[t].cos_m, cos_m );
        CHECK_EQ( d.face.ticks[t].deg, made.ticks[t].deg );
    }

    draw_frame( &d );
    ( void )memcpy( partial, shadow, sizeof( shadow ) );
    mine.max_value  = 2200;
    mine.major_step = 200;
    mine.label_div  = 100;
    draw_frame( &d );
    CHECK_EQ( differences(), 0u );
}

/* Check every moving frame through the motion module. The opaque cap must
 * remain identical to the first frame, regardless of the needle's angle, and
 * a needle of no length draws nothing. */
static void test_round_hub_stays_fixed_above_the_moving_needle( void )
{
    static const int32_t values[] = { 135, 300, 0, 150 };
    dial_widget_t d;
    int32_t fixed_r = DIAL_HUB_R - 1;

    CHECK_EQ( dial_needle_alpha( 0, 0, 0, 1000, 0 ), 0u );
    dial_init( &d, &cfg, now );
    ( void )memcpy( partial, shadow, sizeof( shadow ) );
    for ( size_t i = 0u; i < ( sizeof( values ) / sizeof( values[0] ) ); i++ )
    {
        unsigned polls = 0u;

        do
        {
            now += DIAL_MOTION_FRAME_MS;
            dial_poll( &d, now, true, values[i] );
            for ( int32_t y = -fixed_r; y <= fixed_r; y++ )
            {
                for ( int32_t x = -fixed_r; x <= fixed_r; x++ )
                {
                    if ( ( ( x * x ) + ( y * y ) ) <= ( fixed_r * fixed_r ) )
                    {
                        CHECK_EQ(
                            shadow[DIAL_CY + y][DIAL_CX + x],
                            partial[DIAL_CY + y][DIAL_CX + x]
                        );
                    }
                }
            }
            polls++;
        } while ( !dial_settled( &d ) && ( polls < 200u ) );
        CHECK( dial_settled( &d ) );
    }
}

int main( void )
{
    test_round_hub_stays_fixed_above_the_moving_needle();
    test_partial_redraw_matches_a_fresh_frame();
    test_tick_numbers_use_label_div();
    test_needle_ends_line_up_with_the_scale_ends();
    test_a_scale_without_a_vacuum_unit_keeps_minus_signs();
    test_a_scale_with_small_ticks_fits_every_tick();
    test_no_tick_reaches_outside_the_tick_ring();
    test_upper_arc();
    test_a_dial_face_is_made_when_the_dial_opens();
    return MT_RESULT();
}
