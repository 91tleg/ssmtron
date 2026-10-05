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
    DIAL_STYLE_FULL_SWEEP,
    0,
    0u
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
        DIAL_STYLE_FULL_SWEEP,
        0,
        0u
    };
    dial_face_t face;

    make_face( &fine, &face );
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
          DIAL_STYLE_FULL_SWEEP,
          0,
          0u },
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
          DIAL_STYLE_FULL_SWEEP,
          0,
          0u },
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
          DIAL_STYLE_UPPER_ARC,
          0,
          0u },
    };

    for ( size_t c = 0; c < ( sizeof( cfgs ) / sizeof( cfgs[0] ) ); c++ )
    {
        dial_face_t face;
        int32_t ends[2] = { cfgs[c].min_value, cfgs[c].max_value };

        make_face( &cfgs[c], &face );
        CHECK_EQ( face.ticks[0].deg, value_to_deg( &cfgs[c], ends[0] ) );
        CHECK_EQ(
            face.ticks[face.tick_count - 1u].deg,
            value_to_deg( &cfgs[c], ends[1] )
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

/* The boost scale: vacuum fills the quarter turn from 9 o'clock down to 6,
 * boost the half turn from 9 up over the top to 3. Vacuum has a long,
 * numbered tick every 10 and a short one between (every 5), 15 degrees
 * apart. Boost has a long tick every 5 units and four short ones between,
 * 6 degrees apart. */
static void test_boost_ticks_are_a_quarter_and_a_half_turn( void )
{
    static const dial_cfg_t boost = {
        -30000,
        30000,
        5000,
        4u,
        1000,
        "TURBO",
        "PSI",
        "INHG",
        1u,
        2u,
        3u,
        900u,
        DIAL_STYLE_FULL_SWEEP,
        10000,
        1u
    };
    static const char * const numbers[] = {
        "30", "20", "10", "0", "5", "10", "15", "20", "25", "30"
    };
    dial_face_t face;

    make_face( &boost, &face );
    CHECK_EQ( face.tick_count, 37u );
    CHECK_EQ( face.ticks[0].deg, 180 );  /* 30 inHg, 6 o'clock */
    CHECK_EQ( face.ticks[6].deg, 270 );  /* zero, 9 o'clock */
    CHECK_EQ( face.ticks[36].deg, 450 ); /* 30 psi, 3 o'clock */
    for ( size_t i = 1; i < face.tick_count; i++ )
    {
        CHECK_EQ(
            face.ticks[i].deg - face.ticks[i - 1u].deg, ( i <= 6u ) ? 15 : 6
        );
    }
    for ( size_t i = 0; i < face.tick_count; i++ )
    {
        bool long_tick =
            ( i <= 6u ) ? ( ( i % 2u ) == 0u ) : ( ( ( i - 6u ) % 5u ) == 0u );

        CHECK_EQ( face.ticks[i].major, long_tick );
    }
    CHECK_EQ( number_count( &boost, &face ), 10u );
    for ( size_t i = 0; i < 10u; i++ )
    {
        CHECK( strcmp( face.labels[i].text, numbers[i] ) == 0 );
    }
}

/* A scale that changes sign without its own unit keeps its minus signs. */
static void test_a_scale_without_a_vacuum_unit_keeps_minus_signs( void )
{
    static const dial_cfg_t signed_scale = {
        -30,
        20,
        10,
        4u,
        1,
        "T",
        "U",
        NULL,
        1u,
        2u,
        3u,
        900u,
        DIAL_STYLE_FULL_SWEEP,
        0,
        0u
    };
    dial_face_t face;

    make_face( &signed_scale, &face );
    CHECK( strcmp( face.labels[0].text, "-30" ) == 0 );
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
        DIAL_STYLE_FULL_SWEEP,
        0,
        0u
    };
    dial_face_t face;
    size_t majors = 0;

    make_face( &tach, &face );
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
        DIAL_STYLE_UPPER_ARC,
        0,
        0u
    };
    dial_face_t face;
    dial_widget_t d;

    CHECK_EQ( value_to_deg( &upper, 1000 ), -75 );
    CHECK_EQ( value_to_deg( &upper, 1800 ), 0 );
    CHECK_EQ( value_to_deg( &upper, 2600 ), 75 );
    CHECK_EQ( value_to_deg( &upper, 0 ), -75 );
    CHECK_EQ( value_to_deg( &upper, 3000 ), 75 );
    make_face( &upper, &face );
    CHECK_EQ( face.tick_count, 9u );
    CHECK_EQ( number_count( &upper, &face ), 5u );
    CHECK( strcmp( face.labels[0].text, "100" ) == 0 );
    CHECK( strcmp( face.labels[4].text, "260" ) == 0 );
    for ( size_t i = 0; i < face.tick_count; i++ )
    {
        CHECK( ( face.ticks[i].deg >= -75 ) && ( face.ticks[i].deg <= 75 ) );
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
    DIAL_STYLE_UPPER_ARC,
    0,
    0u
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
    DIAL_STYLE_UPPER_ARC,
    0,
    0u
};

/* Whether any ink of this label lands on a visible part of a tick that is
 * grown by grow pixels: longer toward the center and wider on both sides.
 * Growing by 1 reaches the pixels right next to a tick, so a label that is
 * first hit at grow k has k - 1 empty pixels between its ink and the tick. */
static bool label_hits_a_tick(
    const dial_label_t * lb,
    const dial_face_t * face,
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
                for ( size_t t = 0; !hit && ( t < face->tick_count ); t++ )
                {
                    const dial_tick_t * tick = &face->ticks[t];
                    int32_t tick_len =
                        tick->major ? DIAL_TICK_MAJOR_LEN : DIAL_TICK_MINOR_LEN;
                    int32_t half =
                        tick->major ? DIAL_TICK_MAJOR_HALF_W : DIAL_TICK_HALF_W;

                    hit = radial_line_alpha(
                              col - DIAL_CX,
                              row - DIAL_CY,
                              tick->sin_m,
                              tick->cos_m,
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
    const dial_face_t * face
)
{
    int32_t grow = 0;

    while ( ( grow < 8 ) && !label_hits_a_tick( lb, face, grow ) )
    {
        grow++;
    }
    return ( grow == 0 ) ? 0 : ( grow - 1 );
}

static size_t major_ticks( const dial_face_t * face )
{
    size_t count = 0u;

    for ( size_t t = 0; t < face->tick_count; t++ )
    {
        count += face->ticks[t].major ? 1u : 0u;
    }
    return count;
}

/* No number of either arc dial touches a tick. */
static void test_arc_numbers_clear_the_ticks( void )
{
    const dial_cfg_t * arcs[] = { &coolant_like, &voltage_like };

    for ( size_t i = 0; i < 2u; i++ )
    {
        dial_face_t face;

        make_face( arcs[i], &face );
        for ( size_t l = 0; l < major_ticks( &face ); l++ )
        {
            int32_t gap = gap_to_ticks( &face.labels[l], &face );

            if ( gap < 1 )
            {
                printf(
                    "  dial %zu, number %s touches a tick\n",
                    i,
                    face.labels[l].text
                );
            }
            CHECK( gap >= 1 );
        }
    }
}

/* The ticks are only worked out for pixels in the tick ring, so no tick may
 * touch a pixel outside it. Checked against every tick of every pixel, for a
 * full sweep dial with small ticks, one with a negative side, and an arc. */
static void test_no_tick_reaches_outside_the_tick_ring( void )
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
        DIAL_STYLE_FULL_SWEEP,
        0,
        0u
    };
    static const dial_cfg_t boost = {
        -30000,
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
        DIAL_STYLE_FULL_SWEEP,
        0,
        0u
    };
    const dial_cfg_t * cfgs[] = { &tach, &boost, &coolant_like };

    for ( size_t c = 0; c < ( sizeof( cfgs ) / sizeof( cfgs[0] ) ); c++ )
    {
        dial_face_t face;
        unsigned strays = 0u;

        make_face( cfgs[c], &face );
        for ( int32_t dy = -DIAL_CY; dy < DIAL_CY; dy++ )
        {
            for ( int32_t dx = -DIAL_CX; dx < DIAL_CX; dx++ )
            {
                int32_t d2 = ( dx * dx ) + ( dy * dy );
                bool inside =
                    ( d2 >= ( DIAL_TICK_RING_IN * DIAL_TICK_RING_IN ) )
                    && ( d2 <= ( DIAL_TICK_RING_OUT * DIAL_TICK_RING_OUT ) );

                for ( size_t t = 0; ( !inside ) && ( t < face.tick_count );
                      t++ )
                {
                    const dial_tick_t * tick = &face.ticks[t];

                    if ( radial_line_alpha(
                             dx,
                             dy,
                             tick->sin_m,
                             tick->cos_m,
                             DIAL_TICK_R
                                 - ( tick->major ? DIAL_TICK_MAJOR_LEN
                                                 : DIAL_TICK_MINOR_LEN ),
                             DIAL_TICK_R,
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

/* The face's own words (title and units) must not run into a tick number or
 * into each other, and must stay on the round panel. Checked on the faces
 * that carry units: boost with its pair of units, and rpm with one. */
static bool boxes_overlap(
    const dial_label_t * a,
    const dial_label_t * b
)
{
    int32_t aw = label_width( strlen( a->text ), a->scale );
    int32_t ah = ( int32_t )FONT_CELL_H * a->scale;
    int32_t bw = label_width( strlen( b->text ), b->scale );
    int32_t bh = ( int32_t )FONT_CELL_H * b->scale;

    return ( a->x < ( b->x + bw ) ) && ( b->x < ( a->x + aw ) )
           && ( a->y < ( b->y + bh ) ) && ( b->y < ( a->y + ah ) );
}

static bool on_the_panel( const dial_label_t * lb )
{
    int32_t w     = label_width( strlen( lb->text ), lb->scale );
    int32_t h     = ( int32_t )FONT_CELL_H * lb->scale;
    int32_t limit = ( int32_t )UI_RADIUS - 2;
    bool ok       = true;

    for ( int32_t corner = 0; corner < 4; corner++ )
    {
        int32_t dx = ( lb->x + ( ( ( corner % 2 ) != 0 ) ? w : 0 ) ) - DIAL_CX;
        int32_t dy = ( lb->y + ( ( corner >= 2 ) ? h : 0 ) ) - DIAL_CY;

        ok = ok && ( ( ( dx * dx ) + ( dy * dy ) ) <= ( limit * limit ) );
    }
    return ok;
}

static void test_fixed_words_keep_clear_of_the_numbers_and_each_other( void )
{
    static const dial_cfg_t boost = {
        -30000,
        30000,
        5000,
        4u,
        1000,
        "TURBO",
        "PSI",
        "INHG",
        1u,
        2u,
        3u,
        900u,
        DIAL_STYLE_FULL_SWEEP,
        10000,
        1u
    };
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
        DIAL_STYLE_FULL_SWEEP,
        0,
        0u
    };
    const dial_cfg_t * cfgs[] = { &boost, &tach };

    for ( size_t c = 0; c < 2u; c++ )
    {
        dial_face_t face;
        size_t numbers;

        make_face( cfgs[c], &face );
        numbers = number_count( cfgs[c], &face );
        CHECK( face.label_count > numbers );
        for ( size_t n = 0; n < numbers; n++ )
        {
            /* The numbers themselves keep clear of one another and of the
             * ticks. */
            CHECK( gap_to_ticks( &face.labels[n], &face ) >= 1 );
            for ( size_t o = n + 1u; o < numbers; o++ )
            {
                CHECK( !boxes_overlap( &face.labels[n], &face.labels[o] ) );
            }
        }
        for ( size_t f = numbers; f < face.label_count; f++ )
        {
            CHECK( on_the_panel( &face.labels[f] ) );
            for ( size_t o = 0; o < face.label_count; o++ )
            {
                if ( o != f )
                {
                    CHECK( !boxes_overlap( &face.labels[f], &face.labels[o] ) );
                }
            }
        }
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
    make_face( &mine, &made );
    CHECK_EQ( d.face.tick_count, made.tick_count );
    CHECK_EQ( d.face.label_count, made.label_count );
    for ( size_t t = 0; t < made.tick_count; t++ )
    {
        int32_t sin_m;
        int32_t cos_m;

        angle_vec( made.ticks[t].deg, &sin_m, &cos_m );
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

int main( void )
{
    test_partial_redraw_matches_a_fresh_frame();
    test_tick_numbers_use_label_div();
    test_needle_ends_line_up_with_the_scale_ends();
    test_boost_ticks_are_a_quarter_and_a_half_turn();
    test_a_scale_without_a_vacuum_unit_keeps_minus_signs();
    test_a_scale_with_small_ticks_fits_every_tick();
    test_no_tick_reaches_outside_the_tick_ring();
    test_upper_arc();
    test_arc_numbers_clear_the_ticks();
    test_fixed_words_keep_clear_of_the_numbers_and_each_other();
    test_a_dial_face_is_made_when_the_dial_opens();
    return MT_RESULT();
}
