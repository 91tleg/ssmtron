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
    int32_t cos_milli
)
{
    return needle_alpha( dx, dy, sin_milli, cos_milli ) >= HALF_COVERED;
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
    return radial_line_alpha(
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
    CHECK_EQ( label_count, 9u ); /* 0 to 8: the zero is labeled too */
    CHECK( strcmp( labels[0].text, "0" ) == 0 );
    CHECK( strcmp( labels[1].text, "1" ) == 0 );
    CHECK( strcmp( labels[8].text, "8" ) == 0 );
}

/* The needle is a straight bar with a triangle at its end. The point reaches
 * the middle of a long tick, and the long ticks are one pixel wider on each
 * side than they were. */
static int32_t needle_width_at( int32_t distance )
{
    int32_t count = 0;

    /* Pointing straight up (sin 0, cos 1), distance from the hub. */
    for ( int32_t dx = -20; dx <= 20; dx++ )
    {
        count += on_needle( dx, -distance, 0, 1000 ) ? 1 : 0;
    }
    return count;
}

static void test_needle_is_a_bar_with_a_pointed_end( void )
{
    int32_t len = DIAL_NEEDLE_LEN;
    int32_t bar = ( 2 * DIAL_NEEDLE_HALF_W ) + 1;

    CHECK_EQ( len, DIAL_TICK_R - ( DIAL_TICK_MAJOR_LEN / 2 ) );
    CHECK( on_needle( 0, -len, 0, 1000 ) ); /* the tip is the end */
    CHECK( !on_needle( 0, -( len + 1 ), 0, 1000 ) );

    /* The bar keeps its full width right up to where the triangle starts. */
    CHECK_EQ( needle_width_at( 8 ), bar );
    CHECK_EQ( needle_width_at( len - DIAL_NEEDLE_TIP_LEN - 1 ), bar );
    /* Then it narrows steadily to a point. */
    CHECK( needle_width_at( len - ( DIAL_NEEDLE_TIP_LEN / 2 ) ) < bar );
    CHECK(
        needle_width_at( len - ( DIAL_NEEDLE_TIP_LEN / 2 ) )
        > needle_width_at( len - 2 )
    );
    CHECK( needle_width_at( len - 1 ) <= 3 );
    CHECK( needle_width_at( len ) >= 1 );
    CHECK( DIAL_TICK_MAJOR_HALF_W >= ( UI_LEN( 17 ) + 1 ) );
}

/* The needle at the lowest and the highest value points at the middle of the
 * first and last long ticks: its tip lies on the tick's own line, inside it.
 * Checked for each kind of scale. */
static void test_needle_ends_line_up_with_the_scale_ends( void )
{
    static const dial_cfg_t cfgs[] = {
        { -30000,
          30000,
          10000,
          4u,
          1000,
          "TURBO",
          "PSI",
          "INHG",
          1u,
          2u,
          3u,
          900u,
          DIAL_STYLE_FULL_SWEEP },
        { 0,
          8000,
          1000,
          4u,
          1000,
          "RPM",
          "x1000",
          NULL,
          1u,
          2u,
          3u,
          900u,
          DIAL_STYLE_FULL_SWEEP },
        { 1000,
          2600,
          400,
          1u,
          10,
          "COOLANT",
          "F",
          NULL,
          1u,
          2u,
          3u,
          900u,
          DIAL_STYLE_UPPER_ARC },
    };

    for ( size_t c = 0; c < ( sizeof( cfgs ) / sizeof( cfgs[0] ) ); c++ )
    {
        dial_tick_t ticks[DIAL_MAX_TICKS];
        dial_label_t labels[DIAL_MAX_LABELS];
        size_t tick_count  = 0;
        size_t label_count = 0;
        int32_t ends[2]    = { cfgs[c].min_value, cfgs[c].max_value };

        build_ticks( &cfgs[c], ticks, &tick_count, labels, &label_count );
        CHECK_EQ( ticks[0].deg, value_to_deg( &cfgs[c], ends[0] ) );
        CHECK_EQ(
            ticks[tick_count - 1u].deg, value_to_deg( &cfgs[c], ends[1] )
        );
        for ( size_t e = 0; e < 2u; e++ )
        {
            int16_t deg = value_to_deg( &cfgs[c], ends[e] );
            int32_t sin_m;
            int32_t cos_m;
            int32_t len = DIAL_NEEDLE_LEN;
            int32_t tx;
            int32_t ty;

            angle_vec( deg, &sin_m, &cos_m );
            tx = ( len * sin_m ) / 1000;
            ty = -( len * cos_m ) / 1000;
            CHECK( on_needle( tx, ty, sin_m, cos_m ) );
            CHECK( on_radial_line(
                tx,
                ty,
                sin_m,
                cos_m,
                DIAL_TICK_R - DIAL_TICK_MAJOR_LEN,
                DIAL_TICK_R,
                DIAL_TICK_MAJOR_HALF_W
            ) );
        }
    }
}

/* A scale that crosses zero has the same gap between ticks on both sides: a
 * step of 10 vacuum units is as wide as a step of 10 boost units. */
static void test_ticks_are_evenly_spaced_across_zero( void )
{
    static const dial_cfg_t boost = {
        -30000,
        20000,
        10000,
        4u,
        1000,
        "TURBO",
        "PSI",
        "INHG",
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

    build_ticks( &boost, ticks, &tick_count, labels, &label_count );
    CHECK_EQ( tick_count, 26u );
    CHECK_EQ( ticks[0].deg, 135 );
    CHECK_EQ( ticks[25].deg, 360 );
    for ( size_t i = 1; i < tick_count; i++ )
    {
        CHECK_EQ( ticks[i].deg - ticks[i - 1u].deg, 9 ); /* 2 units each */
    }
    CHECK_EQ( label_count, 6u );
    CHECK( strcmp( labels[0].text, "-30" ) == 0 );
    CHECK( strcmp( labels[3].text, "0" ) == 0 );
    CHECK( strcmp( labels[5].text, "20" ) == 0 );
}

/* A tachometer scale with four small ticks between the numbers, 200 rpm
 * apart: 41 ticks, all of which fit. */
static void test_a_scale_with_small_ticks_fits_every_tick( void )
{
    static const dial_cfg_t tach = {
        0,
        8000,
        1000,
        4u,
        1000,
        "RPM",
        "x1000",
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
    size_t majors      = 0;

    build_ticks( &tach, ticks, &tick_count, labels, &label_count );
    CHECK_EQ( tick_count, 41u );
    CHECK_EQ( label_count, 9u );
    for ( size_t i = 0; i < tick_count; i++ )
    {
        majors += ticks[i].major ? 1u : 0u;
    }
    CHECK_EQ( majors, 9u );
    CHECK( ticks[tick_count - 1u].deg == 435 ); /* the last tick is 8000 */
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

/* The coolant and voltage dials as gauge.c sets them up: three ticks between
 * the numbers. test_gauge_dial checks that gauge.c still has these values. */
static const dial_cfg_t coolant_like = {
    1000,
    2600,
    400,
    3u,
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
static const dial_cfg_t voltage_like = {
    800,
    1600,
    200,
    3u,
    100,
    "VOLTAGE",
    "V",
    NULL,
    0xEF5Bu,
    0x632Cu,
    0xF922u,
    900u,
    DIAL_STYLE_UPPER_ARC
};

/* Whether any ink of this label lands on a visible part of a tick that is
 * grown by grow pixels: longer toward the center and wider on both sides.
 * Growing by 1 reaches the pixels right next to a tick, so a label that is
 * first hit at grow k has k - 1 empty pixels between its ink and the tick. */
static bool label_hits_a_tick(
    const dial_label_t * lb,
    const dial_tick_t * ticks,
    size_t nt,
    int32_t grow
)
{
    size_t len = strlen( lb->text );
    bool hit   = false;

    for ( int16_t row = lb->y;
          !hit && ( row < ( lb->y + ( int16_t )( FONT_CELL_H * lb->scale ) ) );
          row++ )
    {
        for ( int16_t col = lb->x;
              !hit
              && ( col
                   < ( lb->x + ( int16_t )( len * FONT_CELL_W * lb->scale ) ) );
              col++ )
        {
            uint8_t ink = 0u;

            if ( label_pixel( lb, row, col, &ink ) && ( ink >= 4u ) )
            {
                for ( size_t t = 0; !hit && ( t < nt ); t++ )
                {
                    int32_t sin_m;
                    int32_t cos_m;
                    int32_t tick_len = ticks[t].major ? DIAL_TICK_MAJOR_LEN
                                                      : DIAL_TICK_MINOR_LEN;
                    int32_t half     = ticks[t].major ? DIAL_TICK_MAJOR_HALF_W
                                                      : DIAL_TICK_HALF_W;

                    angle_vec( ticks[t].deg, &sin_m, &cos_m );
                    hit = radial_line_alpha(
                              col - DIAL_CX,
                              row - DIAL_CY,
                              sin_m,
                              cos_m,
                              ( DIAL_TICK_R - tick_len ) - grow,
                              DIAL_TICK_R,
                              half + grow
                          )
                          >= 4u;
                }
            }
        }
    }
    return hit;
}

/* Empty pixels between a label's ink and the nearest tick: 0 when they touch.
 * Capped at 8, which is far. */
static int32_t gap_to_ticks(
    const dial_label_t * lb,
    const dial_tick_t * ticks,
    size_t nt
)
{
    int32_t grow = 0;

    while ( ( grow < 8 ) && !label_hits_a_tick( lb, ticks, nt, grow ) )
    {
        grow++;
    }
    return ( grow == 0 ) ? 0 : ( grow - 1 );
}

/* Three ticks between each pair of numbers, and each number sits one or two
 * empty pixels from the tick it belongs to: close, never touching. */
static void
test_arc_dials_have_three_ticks_between_numbers_and_snug_labels( void )
{
    const dial_cfg_t * arcs[] = { &coolant_like, &voltage_like };

    for ( size_t i = 0; i < 2u; i++ )
    {
        dial_tick_t ticks[DIAL_MAX_TICKS];
        dial_label_t labels[DIAL_MAX_LABELS];
        size_t nt;
        size_t nl;
        size_t minors_since_major = 0u;

        build_ticks( arcs[i], ticks, &nt, labels, &nl );
        CHECK_EQ( nt, 17u );
        CHECK_EQ( nl, 5u );
        for ( size_t t = 0; t < nt; t++ )
        {
            if ( ticks[t].major )
            {
                CHECK( ( t == 0u ) || ( minors_since_major == 3u ) );
                minors_since_major = 0u;
            }
            else
            {
                minors_since_major++;
            }
        }
        for ( size_t l = 0; l < nl; l++ )
        {
            int32_t gap = gap_to_ticks( &labels[l], ticks, nt );

            if ( ( gap < 1 ) || ( gap > 2 ) )
            {
                printf(
                    "  dial %zu, number %s: %d empty pixels to the ticks\n",
                    i,
                    labels[l].text,
                    ( int )gap
                );
            }
            CHECK( gap >= 1 ); /* never touching */
            CHECK( gap <= 2 ); /* and close */
        }
    }
}

/* Whether build_ticks puts every number of this dial where a search made now,
 * with nothing kept from before, puts it. */
static bool labels_match_a_fresh_search( const dial_cfg_t * c )
{
    dial_tick_t ticks[DIAL_MAX_TICKS];
    dial_label_t labels[DIAL_MAX_LABELS];
    size_t nt;
    size_t nl;
    size_t major = 0u;
    bool match   = true;

    build_ticks( c, ticks, &nt, labels, &nl );
    for ( size_t t = 0; t < nt; t++ )
    {
        if ( ticks[t].major )
        {
            int32_t sin_m;
            int32_t cos_m;
            int32_t r;
            int32_t x;
            int32_t y;

            angle_vec( ticks[t].deg, &sin_m, &cos_m );
            r = arc_label_radius_search(
                labels[major].text, DIAL_LABEL_SCALE, sin_m, cos_m
            );
            place_label(
                strlen( labels[major].text ),
                DIAL_LABEL_SCALE,
                r,
                sin_m,
                cos_m,
                &x,
                &y
            );
            match =
                match && ( labels[major].x == x ) && ( labels[major].y == y );
            major++;
        }
    }
    return match && ( major == nl ) && ( nl > 0u );
}

/* Arc number positions are worked out once per dial and kept, so another dial
 * in between, or a dial changed where it stands, in any one of the values that
 * decide its numbers, must not leave old positions behind. */
static void test_arc_number_positions_are_not_stale( void )
{
    dial_cfg_t moved = coolant_like;

    CHECK( labels_match_a_fresh_search( &coolant_like ) );
    CHECK( labels_match_a_fresh_search( &coolant_like ) ); /* from the cache */
    CHECK( labels_match_a_fresh_search( &voltage_like ) );
    CHECK( labels_match_a_fresh_search( &coolant_like ) ); /* after another */

    /* The same dial, at the same address, changed one value at a time: each
     * starts from the cache of the dial before it. */
    moved = coolant_like;
    CHECK( labels_match_a_fresh_search( &moved ) );
    moved.max_value = 2200;
    CHECK( labels_match_a_fresh_search( &moved ) );

    moved = coolant_like;
    CHECK( labels_match_a_fresh_search( &moved ) );
    moved.min_value = 1200;
    CHECK( labels_match_a_fresh_search( &moved ) );

    moved = coolant_like;
    CHECK( labels_match_a_fresh_search( &moved ) );
    moved.major_step = 200;
    CHECK( labels_match_a_fresh_search( &moved ) );

    moved = coolant_like;
    CHECK( labels_match_a_fresh_search( &moved ) );
    moved.label_div = 100;
    CHECK( labels_match_a_fresh_search( &moved ) );
}

int main( void )
{
    test_fixed_label_is_centered_at_its_drawn_size();
    test_partial_redraw_matches_a_fresh_frame();
    test_tick_numbers_use_label_div();
    test_needle_is_a_bar_with_a_pointed_end();
    test_needle_ends_line_up_with_the_scale_ends();
    test_ticks_are_evenly_spaced_across_zero();
    test_a_scale_with_small_ticks_fits_every_tick();
    test_upper_arc();
    test_arc_dials_have_three_ticks_between_numbers_and_snug_labels();
    test_arc_number_positions_are_not_stale();
    return MT_RESULT();
}
