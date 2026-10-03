/* Render dial geometry into one row buffer; no bitmap is stored. */
#include "lib/ui/dial.h"

#include <string.h>

#include "lib/display/display.h"
#include "lib/textfit/textfit.h"
#include "lib/ui/font.h"
#include "lib/ui/logo_maxtek.h"
#include "lib/ui/dial_motion.h"
#include "lib/ui/ui_geom.h"

#define DIAL_PANEL_W ( ( int )DISPLAY_WIDTH )
#define DIAL_PANEL_H ( ( int )DISPLAY_HEIGHT )
#define DIAL_CX      UI_CX
#define DIAL_CY      UI_CY

/* 270 degree sweep, with zero straight up and positive clockwise. */
#define DIAL_SWEEP_MIN_DEG ( -135 )
#define DIAL_SWEEP_MAX_DEG ( 135 )
#define DIAL_ANGLE_COUNT   ( 271u )

/* Unwrapped angles keep the needle on the lower arc. */
#define DIAL_FULL_MIN_DEG  ( 135 )
#define DIAL_FULL_ZERO_DEG ( 270 )
#define DIAL_FULL_MAX_DEG  ( 435 )
#define DIAL_UPPER_MIN_DEG ( -75 )
#define DIAL_UPPER_MAX_DEG ( 75 )

#define DIAL_TICK_R UI_LEN( 825 )

#define DIAL_TICK_MAJOR_LEN UI_LEN( 134 )
#define DIAL_TICK_MINOR_LEN UI_LEN( 67 )

/* A pixel a tick touches is in this ring around the center: no nearer than a
 * long tick's inner end (less a pixel for its soft edge) and no further than
 * the outer end plus a long tick's half-width (and a pixel). Everything
 * inside or outside it can skip every tick. */
#define DIAL_TICK_RING_IN  ( DIAL_TICK_R - DIAL_TICK_MAJOR_LEN - 1 )
#define DIAL_TICK_RING_OUT ( DIAL_TICK_R + DIAL_TICK_MAJOR_HALF_W + 1 )

/* Line thicknesses are half-widths, so 1 is a line 3 px wide. They follow
 * the panel size, rounded down. */
#define DIAL_TICK_HALF_W       UI_LEN( 9 )
#define DIAL_TICK_MAJOR_HALF_W UI_LEN( 26 )
#define DIAL_NEEDLE_HALF_W     UI_LEN( 34 )
#define DIAL_TAIL_HALF_W       UI_LEN( 42 )

/* The hub: a ring with a lighter disc inside it. */
#define DIAL_HUB_R       UI_LEN( 100 )
#define DIAL_HUB_INNER_R UI_LEN( 75 )

/* The needle's tip reaches the middle of a long tick, so it points at the
 * mark and not at the gap beside it. */
#define DIAL_NEEDLE_LEN ( DIAL_TICK_R - ( DIAL_TICK_MAJOR_LEN / 2 ) )

/* How much of the needle's end is the triangle. */
#define DIAL_NEEDLE_TIP_LEN UI_LEN( 100 )

/* Two slotted screws on the face, as on a gauge's bezel plate. Centers are
 * in thousandths of the radius from the dial's center, clear of the label,
 * the units and the tick numbers. The needle passes over them. */
#define DIAL_SCREW_R            UI_LEN( 66 )
#define DIAL_SCREW_SLOT_HALF_W  ( 650 )
#define DIAL_SCREW_COUNT        ( 2u )
#define DIAL_SCREW_FACE_COLOR   ( 0xA514u )
#define DIAL_SCREW_SLOT_COLOR   ( 0x4208u )
#define DIAL_SLOT_SIN_COS_MILLI ( 707 )

/* The Maxtek logo on the full sweep faces, under the positive unit: the
 * middle of its width, and its top edge, from the center. It is drawn black
 * on a light face and white on a dark one. */
#define DIAL_LOGO_X UI_LEN( 510 )
#define DIAL_LOGO_Y UI_LEN( 60 )

/* The title of a full sweep face (TURBO) sits higher than the units, from
 * the center to its top edge. */
#define DIAL_TITLE_Y UI_LEN( 384 )

/* The label and units around the center use this text size. */
#define DIAL_FIXED_SCALE UI_TEXT_SCALE( 1 )

/* Tick numbers are drawn at this text size, chosen for the 240 px panel. */
#define DIAL_LABEL_SCALE UI_TEXT_SCALE( 2 )

/* Where the tick numbers sit, from the center. On the full sweep they sit on
 * one circle. On the upper arc they sit on a smaller one, and the same one
 * for every number: it is set by the three-digit numbers, which are widest
 * and so reach furthest toward the long ticks at the steep ends of the arc.
 * They are as close to their ticks as they can be without touching, and the
 * shorter numbers, on the same circle, only sit a little further away. The
 * spacing test in test_dial_region checks this for both arc dials. */
#define DIAL_LABEL_R     UI_LEN( 584 )
#define DIAL_ARC_LABEL_R UI_LEN( 549 )

/* Per-frame redraw covers only the needle's old and new position, not the
 * whole face (ticks/labels/face don't change between frames): one blit
 * instead of one per row, and only the pixels near the needle. This bounds
 * the biggest side that box can be; a jump that would exceed it (should
 * never happen at any sane top speed) falls back to a full draw_frame.
 * DIAL_TAIL_R is the counterweight's reach behind center. */
#define DIAL_REGION_PERMILLE ( 1150 )
#define DIAL_REGION_MAX      UI_LEN( DIAL_REGION_PERMILLE )
#define DIAL_REGION_PIXELS                                                     \
    ( UI_LEN_U( DIAL_REGION_PERMILLE ) * UI_LEN_U( DIAL_REGION_PERMILLE ) )
/* The box must cover the widest line the needle shape draws, plus one
 * pixel of rounding slack. A smaller margin leaves stale pixels of the old
 * needle behind (test_dial_region checks this). */
#define DIAL_HALF_W_MAX                                                        \
    ( ( DIAL_NEEDLE_HALF_W > DIAL_TAIL_HALF_W ) ? DIAL_NEEDLE_HALF_W           \
                                                : DIAL_TAIL_HALF_W )
#define DIAL_REGION_MARGIN ( DIAL_HALF_W_MAX + 1 )
#define DIAL_TAIL_R        UI_LEN( 259 )

static void angle_vec(
    int16_t deg,
    int32_t * sin_milli,
    int32_t * cos_milli
)
{
    /* Sine and cosine from -135 to 135 degrees in milliunits, one per degree.
     */
    static const int16_t SIN_MILLI[DIAL_ANGLE_COUNT] = {
        -707,  -719,  -731,  -743, -755, -766, -777, -788, -799, -809, -819,
        -829,  -839,  -848,  -857, -866, -875, -883, -891, -899, -906, -914,
        -921,  -927,  -934,  -940, -946, -951, -956, -961, -966, -970, -974,
        -978,  -982,  -985,  -988, -990, -993, -995, -996, -998, -999, -999,
        -1000, -1000, -1000, -999, -999, -998, -996, -995, -993, -990, -988,
        -985,  -982,  -978,  -974, -970, -966, -961, -956, -951, -946, -940,
        -934,  -927,  -921,  -914, -906, -899, -891, -883, -875, -866, -857,
        -848,  -839,  -829,  -819, -809, -799, -788, -777, -766, -755, -743,
        -731,  -719,  -707,  -695, -682, -669, -656, -643, -629, -616, -602,
        -588,  -574,  -559,  -545, -530, -515, -500, -485, -469, -454, -438,
        -423,  -407,  -391,  -375, -358, -342, -326, -309, -292, -276, -259,
        -242,  -225,  -208,  -191, -174, -156, -139, -122, -105, -87,  -70,
        -52,   -35,   -17,   0,    17,   35,   52,   70,   87,   105,  122,
        139,   156,   174,   191,  208,  225,  242,  259,  276,  292,  309,
        326,   342,   358,   375,  391,  407,  423,  438,  454,  469,  485,
        500,   515,   530,   545,  559,  574,  588,  602,  616,  629,  643,
        656,   669,   682,   695,  707,  719,  731,  743,  755,  766,  777,
        788,   799,   809,   819,  829,  839,  848,  857,  866,  875,  883,
        891,   899,   906,   914,  921,  927,  934,  940,  946,  951,  956,
        961,   966,   970,   974,  978,  982,  985,  988,  990,  993,  995,
        996,   998,   999,   999,  1000, 1000, 1000, 999,  999,  998,  996,
        995,   993,   990,   988,  985,  982,  978,  974,  970,  966,  961,
        956,   951,   946,   940,  934,  927,  921,  914,  906,  899,  891,
        883,   875,   866,   857,  848,  839,  829,  819,  809,  799,  788,
        777,   766,   755,   743,  731,  719,  707
    };

    static const int16_t COS_MILLI[DIAL_ANGLE_COUNT] = {
        -707, -695, -682, -669, -656, -643, -629, -616, -602, -588, -574, -559,
        -545, -530, -515, -500, -485, -469, -454, -438, -423, -407, -391, -375,
        -358, -342, -326, -309, -292, -276, -259, -242, -225, -208, -191, -174,
        -156, -139, -122, -105, -87,  -70,  -52,  -35,  -17,  0,    17,   35,
        52,   70,   87,   105,  122,  139,  156,  174,  191,  208,  225,  242,
        259,  276,  292,  309,  326,  342,  358,  375,  391,  407,  423,  438,
        454,  469,  485,  500,  515,  530,  545,  559,  574,  588,  602,  616,
        629,  643,  656,  669,  682,  695,  707,  719,  731,  743,  755,  766,
        777,  788,  799,  809,  819,  829,  839,  848,  857,  866,  875,  883,
        891,  899,  906,  914,  921,  927,  934,  940,  946,  951,  956,  961,
        966,  970,  974,  978,  982,  985,  988,  990,  993,  995,  996,  998,
        999,  999,  1000, 1000, 1000, 999,  999,  998,  996,  995,  993,  990,
        988,  985,  982,  978,  974,  970,  966,  961,  956,  951,  946,  940,
        934,  927,  921,  914,  906,  899,  891,  883,  875,  866,  857,  848,
        839,  829,  819,  809,  799,  788,  777,  766,  755,  743,  731,  719,
        707,  695,  682,  669,  656,  643,  629,  616,  602,  588,  574,  559,
        545,  530,  515,  500,  485,  469,  454,  438,  423,  407,  391,  375,
        358,  342,  326,  309,  292,  276,  259,  242,  225,  208,  191,  174,
        156,  139,  122,  105,  87,   70,   52,   35,   17,   0,    -17,  -35,
        -52,  -70,  -87,  -105, -122, -139, -156, -174, -191, -208, -225, -242,
        -259, -276, -292, -309, -326, -342, -358, -375, -391, -407, -423, -438,
        -454, -469, -485, -500, -515, -530, -545, -559, -574, -588, -602, -616,
        -629, -643, -656, -669, -682, -695, -707
    };
    int32_t adjusted = deg;
    bool invert      = false;
    int32_t offset;
    size_t idx;

    while ( adjusted > DIAL_SWEEP_MAX_DEG )
    {
        adjusted -= 180;
        invert = !invert;
    }
    while ( adjusted < DIAL_SWEEP_MIN_DEG )
    {
        adjusted += 180;
        invert = !invert;
    }
    offset = adjusted - DIAL_SWEEP_MIN_DEG;
    idx    = ( size_t )offset;

    *sin_milli = invert ? -( int32_t )SIN_MILLI[idx] : SIN_MILLI[idx];
    *cos_milli = invert ? -( int32_t )COS_MILLI[idx] : COS_MILLI[idx];
}

static int16_t value_to_deg(
    const dial_cfg_t * cfg,
    int32_t value
)
{
    int32_t clamped = value;
    int32_t offset;
    int16_t deg;

    if ( clamped < cfg->min_value )
    {
        clamped = cfg->min_value;
    }
    else if ( clamped > cfg->max_value )
    {
        clamped = cfg->max_value;
    }
    else
    {
        /* Keep the value unchanged. */
    }
    if ( cfg->style == DIAL_STYLE_UPPER_ARC )
    {
        int32_t span = cfg->max_value - cfg->min_value;

        offset =
            ( span != 0 )
                ? ( int32_t )( ( ( int64_t )( clamped - cfg->min_value )
                                 * ( DIAL_UPPER_MAX_DEG - DIAL_UPPER_MIN_DEG ) )
                               / span )
                : 0;
        deg = ( int16_t )( DIAL_UPPER_MIN_DEG + offset );
    }
    else if ( ( cfg->min_value < 0 ) && ( cfg->max_value > 0 ) )
    {
        if ( clamped <= 0 )
        {
            offset =
                ( int32_t )( ( ( int64_t )( clamped - cfg->min_value )
                               * ( DIAL_FULL_ZERO_DEG - DIAL_FULL_MIN_DEG ) )
                             / -cfg->min_value );
            deg = ( int16_t )( DIAL_FULL_MIN_DEG + offset );
        }
        else
        {
            /* The same degrees per unit as below zero, so the ticks are as
             * far apart on both sides. The positive side ends where its
             * scale does, not at the end of the sweep. */
            offset =
                ( int32_t )( ( ( int64_t )clamped
                               * ( DIAL_FULL_ZERO_DEG - DIAL_FULL_MIN_DEG ) )
                             / -cfg->min_value );
            deg = ( int16_t )( DIAL_FULL_ZERO_DEG + offset );
            if ( deg > DIAL_FULL_MAX_DEG )
            {
                deg = ( int16_t )DIAL_FULL_MAX_DEG;
            }
        }
    }
    else
    {
        int32_t span = cfg->max_value - cfg->min_value;

        /* An empty scale has nowhere to sweep: rest at the low end. */
        offset =
            ( span != 0 )
                ? ( int32_t )( ( ( int64_t )( clamped - cfg->min_value )
                                 * ( DIAL_FULL_MAX_DEG - DIAL_FULL_MIN_DEG ) )
                               / span )
                : 0;
        deg = ( int16_t )( DIAL_FULL_MIN_DEG + offset );
    }
    return deg;
}

/* Where the needle rests: zero when the scale crosses it, else the low end. */
static int16_t rest_deg( const dial_cfg_t * cfg )
{
    return value_to_deg( cfg, ( cfg->min_value < 0 ) ? 0 : cfg->min_value );
}

static int32_t label_width(
    size_t length,
    uint8_t scale
)
{
    int32_t characters  = ( int32_t )length;
    int32_t glyph_width = ( int32_t )FONT_CELL_W;

    return characters * glyph_width * ( int32_t )scale;
}

/* Test a radial segment without square root or angle calculations. */
/* How much of a pixel a shape covers, from its edge's distance: positive
 * inside, in thousandths of a pixel, measured from the pixel's center. A
 * center on the edge is half covered, and one half a pixel in or out is all
 * or none, so a slanted edge fades over one pixel instead of stair-stepping.
 * The distance is straight across the edge, which the rows of the shape's own
 * axis give exactly and a slanted edge to within a few percent. */
static uint8_t edge_alpha( int32_t dist_milli )
{
    uint8_t alpha = 0u;

    if ( dist_milli >= 500 )
    {
        alpha = FONT_ALPHA_MAX;
    }
    else if ( dist_milli > -500 )
    {
        int32_t levels = ( int32_t )FONT_ALPHA_MAX;
        int32_t scaled = ( ( ( dist_milli + 500 ) * levels ) + 500 ) / 1000;

        alpha = ( uint8_t )scaled;
    }
    else
    {
        /* outside the shape */
    }
    return alpha;
}

/* The pixel's coverage by a straight bar of constant width. */
static uint8_t radial_line_alpha(
    int32_t dx,
    int32_t dy,
    int32_t sin_milli,
    int32_t cos_milli,
    int32_t inner_r,
    int32_t outer_r,
    int32_t half_w
)
{
    int32_t proj   = ( dx * sin_milli ) - ( dy * cos_milli );
    int32_t perp   = ( dx * cos_milli ) + ( dy * sin_milli );
    int32_t across = ( half_w * 1000 ) - ( ( perp < 0 ) ? -perp : perp );
    int32_t before = proj - ( inner_r * 1000 );
    int32_t beyond = ( outer_r * 1000 ) - proj;
    int32_t dist   = across;

    dist = ( before < dist ) ? before : dist;
    dist = ( beyond < dist ) ? beyond : dist;
    return edge_alpha( dist );
}

/* The needle: a straight bar of constant width from the hub, ending in a
 * triangle that narrows to a point. The point keeps half a pixel of width on
 * each side, so it never thins out to nothing. */
static uint8_t needle_alpha(
    int32_t dx,
    int32_t dy,
    int32_t sin_milli,
    int32_t cos_milli
)
{
    int32_t proj   = ( dx * sin_milli ) - ( dy * cos_milli );
    int32_t len    = DIAL_NEEDLE_LEN;
    uint8_t alpha  = 0u;
    int32_t beyond = ( len * 1000 ) - proj;

    if ( ( proj >= -500 ) && ( beyond >= -500 ) )
    {
        int32_t perp    = ( dx * cos_milli ) + ( dy * sin_milli );
        int32_t across  = ( perp < 0 ) ? -perp : perp;
        int32_t tip_run = ( beyond > 0 ) ? beyond : 0;
        int32_t room =
            ( ( DIAL_NEEDLE_HALF_W * tip_run ) / DIAL_NEEDLE_TIP_LEN ) + 500;
        int32_t dist;

        if ( room > ( DIAL_NEEDLE_HALF_W * 1000 ) )
        {
            room = DIAL_NEEDLE_HALF_W * 1000; /* the straight part */
        }
        dist  = room - across;
        dist  = ( beyond < dist ) ? beyond : dist;
        dist  = ( proj < dist ) ? proj : dist;
        alpha = edge_alpha( dist );
    }
    return alpha;
}

/* A disc of this radius at the center: coverage from how far the pixel's
 * distance is inside the edge, taking (r*r - d*d) / 2r for r - d. */
static uint8_t disc_alpha(
    int32_t dist_sq,
    int32_t radius
)
{
    return edge_alpha( ( ( ( radius * radius ) - dist_sq ) * 500 ) / radius );
}

/* The top left corner of a label whose middle is r pixels from the dial's
 * center along this direction. */
/* Adds a piece of text to the face, with its top left corner at (x, y). */
static void add_label(
    dial_face_t * face,
    const char * text,
    uint8_t scale,
    int32_t x,
    int32_t y
)
{
    if ( face->label_count < DIAL_MAX_LABELS )
    {
        dial_label_t * lb = &face->labels[face->label_count];

        lb->x     = ( int16_t )x;
        lb->y     = ( int16_t )y;
        lb->scale = scale;
        ( void )textfit_copy( lb->text, DIAL_LABEL_BUF, text );
        face->label_count++;
    }
}

/* The ticks, and a number at each long one. */
static void add_ticks(
    const dial_cfg_t * cfg,
    dial_face_t * face
)
{
    int32_t minor_step =
        cfg->major_step / ( ( int32_t )cfg->minor_per_major + 1 );
    int32_t r      = ( cfg->style == DIAL_STYLE_UPPER_ARC ) ? DIAL_ARC_LABEL_R
                                                            : DIAL_LABEL_R;
    int32_t height = ( int32_t )FONT_CELL_H * ( int32_t )DIAL_LABEL_SCALE;

    for ( int32_t value = cfg->min_value;
          ( value <= cfg->max_value ) && ( face->tick_count < DIAL_MAX_TICKS );
          value += minor_step )
    {
        dial_tick_t * tick = &face->ticks[face->tick_count];
        int32_t sin_m;
        int32_t cos_m;

        tick->deg   = value_to_deg( cfg, value );
        tick->major = ( ( ( value - cfg->min_value ) % cfg->major_step ) == 0 );
        angle_vec( tick->deg, &sin_m, &cos_m );
        tick->sin_m = ( int16_t )sin_m;
        tick->cos_m = ( int16_t )cos_m;
        face->tick_count++;

        if ( tick->major )
        {
            char text[DIAL_LABEL_BUF];
            int32_t shown = value / cfg->label_div;
            size_t len;

            /* A scale with its own unit below zero reads as a plain
             * magnitude there: the unit says which side it is. */
            if ( ( cfg->unit_neg != NULL ) && ( shown < 0 ) )
            {
                shown = -shown;
            }
            len = textfit_int( text, DIAL_LABEL_BUF, shown );

            /* The number's middle is r from the center along the tick. */
            add_label(
                face,
                text,
                DIAL_LABEL_SCALE,
                ( DIAL_CX + ( ( r * sin_m ) / 1000 ) )
                    - ( label_width( len, DIAL_LABEL_SCALE ) / 2 ),
                ( DIAL_CY - ( ( r * cos_m ) / 1000 ) ) - ( height / 2 )
            );
        }
    }
}

/* The label and the units around the center. */
static void add_fixed_labels(
    const dial_cfg_t * cfg,
    dial_face_t * face
)
{
    bool arc = ( cfg->style == DIAL_STYLE_UPPER_ARC );

    if ( cfg->label != NULL )
    {
        add_label(
            face,
            cfg->label,
            DIAL_FIXED_SCALE,
            DIAL_CX
                - ( label_width( strlen( cfg->label ), DIAL_FIXED_SCALE ) / 2 ),
            DIAL_CY - ( arc ? UI_LEN( 300 ) : DIAL_TITLE_Y )
        );
    }
    if ( cfg->unit_pos != NULL )
    {
        add_label(
            face,
            cfg->unit_pos,
            DIAL_FIXED_SCALE,
            arc ? ( DIAL_CX
                    - ( label_width( strlen( cfg->unit_pos ), DIAL_FIXED_SCALE )
                        / 2 ) )
                : ( DIAL_CX + UI_LEN( 435 ) ),
            arc ? ( DIAL_CY - UI_LEN( 192 ) ) : ( DIAL_CY - UI_LEN( 42 ) )
        );
    }
    if ( cfg->unit_neg != NULL )
    {
        add_label(
            face,
            cfg->unit_neg,
            DIAL_FIXED_SCALE,
            DIAL_CX - UI_LEN( 550 ),
            DIAL_CY + UI_LEN( 192 )
        );
    }
}

/* Works out the whole face of a dial, once, when it is opened. */
static void make_face(
    const dial_cfg_t * cfg,
    dial_face_t * face
)
{
    face->tick_count  = 0u;
    face->label_count = 0u;
    add_ticks( cfg, face );
    add_fixed_labels( cfg, face );
}

/* Whether a label covers this pixel, and how much ink it puts there. */
static bool label_pixel(
    const dial_label_t * lb,
    int16_t row,
    int16_t col,
    uint8_t * out_alpha
)
{
    size_t len         = strlen( lb->text );
    uint16_t cell_w    = ( uint16_t )( FONT_CELL_W * lb->scale );
    uint16_t font_rows = ( uint16_t )( FONT_CELL_H * lb->scale );
    uint16_t width     = ( uint16_t )( cell_w * ( uint16_t )len );
    bool hit           = false;

    if ( ( row >= lb->y )
         && ( row < ( int16_t )( lb->y + ( int16_t )font_rows ) )
         && ( col >= lb->x )
         && ( col < ( int16_t )( lb->x + ( int16_t )width ) ) )
    {
        int16_t row_offset   = ( int16_t )( row - lb->y );
        int16_t col_offset   = ( int16_t )( col - lb->x );
        uint16_t rel_row     = ( uint16_t )row_offset;
        uint16_t rel_col     = ( uint16_t )col_offset;
        uint16_t glyph_index = ( uint16_t )( rel_col / cell_w );
        uint16_t within      = ( uint16_t )( rel_col % cell_w );
        uint8_t alpha =
            font_alpha( lb->text[glyph_index], lb->scale, within, rel_row );

        if ( alpha != 0u )
        {
            *out_alpha = alpha;
            hit        = true;
        }
    }
    return hit;
}

static int32_t abs_i32( int32_t v )
{
    return ( v < 0 ) ? -v : v;
}

/* One slot of the X: a thin bar along a diagonal through the screw's
 * center, reaching half way to the screw's edge. diag is +1 or -1. */
static uint8_t slot_alpha(
    int32_t sdx,
    int32_t sdy,
    int32_t diag
)
{
    int32_t perp = ( ( sdx * diag ) + sdy ) * DIAL_SLOT_SIN_COS_MILLI;
    int32_t proj = ( ( sdx * diag ) - sdy ) * DIAL_SLOT_SIN_COS_MILLI;
    int32_t dist = DIAL_SCREW_SLOT_HALF_W - abs_i32( perp );
    int32_t ends = ( DIAL_SCREW_R * 500 ) - abs_i32( proj );

    dist = ( ends < dist ) ? ends : dist;
    return edge_alpha( dist );
}

/* Both screws blended over color, each a disc with an X cut in it. */
static uint16_t screw_layer(
    uint16_t color,
    int32_t dx,
    int32_t dy
)
{
    /* Where the screws sit: x then y, thousandths of the radius from
     * center. */
    static const int16_t SCREW_PERMILLE[DIAL_SCREW_COUNT][2] = {
        { 280, -190 }, { 120, 260 }
    };
    uint16_t out = color;

    for ( size_t i = 0; i < DIAL_SCREW_COUNT; i++ )
    {
        int32_t sdx = dx - UI_LEN( SCREW_PERMILLE[i][0] );
        int32_t sdy = dy - UI_LEN( SCREW_PERMILLE[i][1] );
        uint8_t ink = disc_alpha( ( sdx * sdx ) + ( sdy * sdy ), DIAL_SCREW_R );

        if ( ink != 0u )
        {
            uint8_t slot_a = slot_alpha( sdx, sdy, 1 );
            uint8_t slot_b = slot_alpha( sdx, sdy, -1 );
            uint8_t slot   = ( slot_a > slot_b ) ? slot_a : slot_b;

            out = font_blend( DIAL_SCREW_FACE_COLOR, out, ink );
            if ( slot != 0u )
            {
                out = font_blend( DIAL_SCREW_SLOT_COLOR, out, slot );
            }
        }
    }
    return out;
}

/* Black on a light face, white on a dark one, by the face's green. */
static uint16_t logo_color( uint16_t face_bg )
{
    return ( ( ( face_bg >> 5 ) & 0x3Fu ) >= 0x20u ) ? 0x0000u : 0xFFFFu;
}

/* Ink of the logo at this pixel, relative to the dial's center. */
static uint8_t logo_ink(
    int32_t dx,
    int32_t dy
)
{
    int32_t x   = dx - ( DIAL_LOGO_X - ( ( int32_t )LOGO_MAXTEK_W / 2 ) );
    int32_t y   = dy - DIAL_LOGO_Y;
    uint8_t ink = 0u;

    if ( ( x >= 0 ) && ( y >= 0 ) )
    {
        ink = logo_maxtek_alpha( ( uint16_t )x, ( uint16_t )y );
    }
    return ink;
}

/* Draw order: hub, needle, screws, logo, labels, ticks, face. */
static uint16_t pixel_color(
    const dial_cfg_t * cfg,
    const dial_face_t * face,
    int32_t needle_sin,
    int32_t needle_cos,
    int16_t row,
    int16_t col
)
{
    int32_t dx      = ( int32_t )col - DIAL_CX;
    int32_t dy      = ( int32_t )row - DIAL_CY;
    int32_t dist_sq = ( dx * dx ) + ( dy * dy );
    uint16_t color  = cfg->face_bg;
    uint8_t ink;

    /* Bottom up, each layer blended over what is under it, so every edge
     * softens against the right color: ticks, tick numbers, needle, hub,
     * counterweight. */
    if ( ( dist_sq >= ( DIAL_TICK_RING_IN * DIAL_TICK_RING_IN ) )
         && ( dist_sq <= ( DIAL_TICK_RING_OUT * DIAL_TICK_RING_OUT ) ) )
    {
        for ( size_t i = 0; i < face->tick_count; i++ )
        {
            const dial_tick_t * tick = &face->ticks[i];

            ink = radial_line_alpha(
                dx,
                dy,
                tick->sin_m,
                tick->cos_m,
                DIAL_TICK_R
                    - ( tick->major ? DIAL_TICK_MAJOR_LEN
                                    : DIAL_TICK_MINOR_LEN ),
                DIAL_TICK_R,
                tick->major ? DIAL_TICK_MAJOR_HALF_W : DIAL_TICK_HALF_W
            );
            if ( ink != 0u )
            {
                color = font_blend( cfg->tick_fg, color, ink );
            }
        }
    }
    for ( size_t i = 0; i < face->label_count; i++ )
    {
        ink = 0u;
        if ( label_pixel( &face->labels[i], row, col, &ink ) )
        {
            color = font_blend( cfg->tick_fg, color, ink );
        }
    }
    if ( cfg->style == DIAL_STYLE_FULL_SWEEP )
    {
        ink = logo_ink( dx, dy );
        if ( ink != 0u )
        {
            color = font_blend( logo_color( cfg->face_bg ), color, ink );
        }
    }
    color = screw_layer( color, dx, dy );
    ink   = needle_alpha( dx, dy, needle_sin, needle_cos );
    if ( ink != 0u )
    {
        color = font_blend( cfg->needle_fg, color, ink );
    }
    ink = disc_alpha( dist_sq, DIAL_HUB_R );
    if ( ink != 0u )
    {
        color = font_blend( 0xA514u, color, ink );
        ink   = disc_alpha( dist_sq, DIAL_HUB_INNER_R );
        if ( ink != 0u )
        {
            color = font_blend( 0xE73Cu, color, ink );
        }
    }
    ink = radial_line_alpha(
        dx, dy, -needle_sin, -needle_cos, 0, DIAL_TAIL_R, DIAL_TAIL_HALF_W
    );
    if ( ink != 0u )
    {
        color = font_blend( 0xCE79u, color, ink );
    }
    return color;
}

/* Bounding box (x0 <= x < x1) of the needle shape at deg: the forward tip
 * at DIAL_NEEDLE_LEN, and the counterweight tail reaching
 * DIAL_TAIL_R behind center. Both lie on the same line through the
 * center, so covering both endpoints covers the whole shape. */
static void needle_extent(
    int16_t deg,
    int16_t * x0,
    int16_t * y0,
    int16_t * x1,
    int16_t * y1
)
{
    int32_t sin_m;
    int32_t cos_m;
    int32_t back_r = DIAL_TAIL_R;
    int32_t fwd_x;
    int32_t fwd_y;
    int32_t back_x;
    int32_t back_y;
    int32_t min_x;
    int32_t max_x;
    int32_t min_y;
    int32_t max_y;

    angle_vec( deg, &sin_m, &cos_m );
    fwd_x  = ( DIAL_NEEDLE_LEN * sin_m ) / 1000;
    fwd_y  = -( DIAL_NEEDLE_LEN * cos_m ) / 1000;
    back_x = -( back_r * sin_m ) / 1000;
    back_y = ( back_r * cos_m ) / 1000;

    min_x = ( fwd_x < back_x ) ? fwd_x : back_x;
    max_x = ( fwd_x > back_x ) ? fwd_x : back_x;
    min_y = ( fwd_y < back_y ) ? fwd_y : back_y;
    max_y = ( fwd_y > back_y ) ? fwd_y : back_y;

    *x0 = ( int16_t )( ( DIAL_CX + min_x ) - DIAL_REGION_MARGIN );
    *y0 = ( int16_t )( ( DIAL_CY + min_y ) - DIAL_REGION_MARGIN );
    *x1 = ( int16_t )( ( DIAL_CX + max_x ) + DIAL_REGION_MARGIN + 1 );
    *y1 = ( int16_t )( ( DIAL_CY + max_y ) + DIAL_REGION_MARGIN + 1 );
}

/* Clips a box to the panel; x0/y0 may still exceed x1/y1 afterward if the
 * box fell entirely outside it (the caller checks that). */
static void clamp_box(
    int16_t * x0,
    int16_t * y0,
    int16_t * x1,
    int16_t * y1
)
{
    if ( *x0 < 0 )
    {
        *x0 = 0;
    }
    if ( *y0 < 0 )
    {
        *y0 = 0;
    }
    if ( *x1 > ( int16_t )DIAL_PANEL_W )
    {
        *x1 = ( int16_t )DIAL_PANEL_W;
    }
    if ( *y1 > ( int16_t )DIAL_PANEL_H )
    {
        *y1 = ( int16_t )DIAL_PANEL_H;
    }
}

/* Same pixel_color geometry as draw_frame, restricted to one rectangle and
 * blitted in a single call instead of one blit per row. Ticks/labels are
 * still built fresh (cheap: a couple dozen entries) so a tick or label
 * inside the box is drawn correctly; pixels outside the box are untouched
 * because nothing but the needle moved since the last redraw. */
/* Renders the dial into dst, row by row, for x0 <= x < x1 and y0 <= y < y1. */
static void fill_rect(
    const dial_widget_t * d,
    int16_t x0,
    int16_t y0,
    int16_t x1,
    int16_t y1,
    uint16_t * dst
)
{
    int32_t needle_sin;
    int32_t needle_cos;
    size_t n = 0u;

    angle_vec( d->displayed_deg, &needle_sin, &needle_cos );
    for ( int16_t row = y0; row < y1; row++ )
    {
        for ( int16_t col = x0; col < x1; col++ )
        {
            dst[n] = pixel_color(
                d->cfg, &d->face, needle_sin, needle_cos, row, col
            );
            n++;
        }
    }
}

/* One blit for the box around the needle's old and new place. */
static void draw_region(
    const dial_widget_t * d,
    int16_t x0,
    int16_t y0,
    int16_t x1,
    int16_t y1
)
{
    static uint16_t region_buf[DIAL_REGION_PIXELS];
    int32_t width  = ( int32_t )x1 - ( int32_t )x0;
    int32_t height = ( int32_t )y1 - ( int32_t )y0;

    fill_rect( d, x0, y0, x1, y1, region_buf );
    ( void )display_blit(
        ( uint16_t )x0,
        ( uint16_t )y0,
        ( uint16_t )width,
        ( uint16_t )height,
        region_buf
    );
}

/* The whole face, one row at a time. */
static void draw_frame( const dial_widget_t * d )
{
    static uint16_t row_buf[DIAL_PANEL_W];

    for ( int16_t row = 0; row < ( int16_t )DIAL_PANEL_H; row++ )
    {
        fill_rect( d, 0, row, ( int16_t )DIAL_PANEL_W, row + 1, row_buf );
        ( void )display_blit(
            0u, ( uint16_t )row, ( uint16_t )DIAL_PANEL_W, 1u, row_buf
        );
    }
}

bool dial_settled( const dial_widget_t * d )
{
    return dial_motion_reached( &d->motion, d->target_deg );
}

void dial_init(
    dial_widget_t * d,
    const dial_cfg_t * cfg,
    uint32_t now_ms
)
{
    d->cfg = cfg;
    make_face( cfg, &d->face );
    d->displayed_deg = rest_deg( cfg );
    d->target_deg    = d->displayed_deg;
    dial_motion_init( &d->motion, d->displayed_deg, now_ms );
    ( void )display_clear( cfg->face_bg );
    draw_frame( d );
}

/* Only the needle moved: redraw the union of where it was and where it is
 * now, not the whole face (see draw_region). A box bigger than the scratch
 * buffer can hold (never expected at any sane speed) falls back to a full
 * redraw instead of clipping it. */
static void redraw_needle(
    const dial_widget_t * d,
    int16_t old_deg
)
{
    int16_t ox0;
    int16_t oy0;
    int16_t ox1;
    int16_t oy1;
    int16_t nx0;
    int16_t ny0;
    int16_t nx1;
    int16_t ny1;
    int16_t x0;
    int16_t y0;
    int16_t x1;
    int16_t y1;

    needle_extent( old_deg, &ox0, &oy0, &ox1, &oy1 );
    needle_extent( d->displayed_deg, &nx0, &ny0, &nx1, &ny1 );
    x0 = ( ox0 < nx0 ) ? ox0 : nx0;
    y0 = ( oy0 < ny0 ) ? oy0 : ny0;
    x1 = ( ox1 > nx1 ) ? ox1 : nx1;
    y1 = ( oy1 > ny1 ) ? oy1 : ny1;
    clamp_box( &x0, &y0, &x1, &y1 );

    if ( ( x0 < x1 ) && ( y0 < y1 )
         && ( ( x1 - x0 ) <= ( int16_t )DIAL_REGION_MAX )
         && ( ( y1 - y0 ) <= ( int16_t )DIAL_REGION_MAX ) )
    {
        draw_region( d, x0, y0, x1, y1 );
    }
    else
    {
        draw_frame( d );
    }
}

void dial_poll(
    dial_widget_t * d,
    uint32_t now_ms,
    bool has_value,
    int32_t value
)
{
    int16_t old_deg = d->displayed_deg;

    d->target_deg =
        has_value ? value_to_deg( d->cfg, value ) : rest_deg( d->cfg );
    if ( dial_motion_poll(
             &d->motion,
             now_ms,
             d->target_deg,
             d->cfg->max_deg_per_s,
             &d->displayed_deg
         ) )
    {
        redraw_needle( d, old_deg );
    }
}
