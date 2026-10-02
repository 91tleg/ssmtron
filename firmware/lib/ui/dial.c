/* Render dial geometry into one row buffer; no bitmap is stored. */
#include "lib/ui/dial.h"

#include <string.h>

#include "lib/display/display.h"
#include "lib/textfit/textfit.h"
#include "lib/ui/font.h"
#include "lib/ui/dial_motion.h"
#include "lib/ui/ui_geom.h"

/* A label's buffer: DIAL_TEXT_MAX characters and the NUL. */
#define DIAL_LABEL_BUF ( DIAL_TEXT_MAX + 1u )

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

/* The label and units around the center use this text size. */
#define DIAL_FIXED_SCALE UI_TEXT_SCALE( 1 )

/* Tick numbers are drawn at this text size, chosen for the 240 px panel. */
#define DIAL_LABEL_SCALE UI_TEXT_SCALE( 2 )

/* Where the tick numbers sit, from the center. Numbers on the full sweep sit
 * on one circle. Numbers on the upper arc sit as close to their tick as the
 * gap below allows, so wide numbers at the steep ends are not pushed over a
 * long tick and short ones are not left far from theirs. */
#define DIAL_LABEL_R UI_LEN( 584 )

/* How far an arc number's ink starts out from the end of its tick, in
 * thousandths of a pixel, before the search below closes in: one empty pixel
 * from pixel center to pixel center. The box it is measured from is only a
 * bound, so the real number can be placed further out than this. */
#define DIAL_ARC_LABEL_GAP ( 2000 )

/* The empty pixels left between an arc number's ink and its tick. */
#define DIAL_ARC_LABEL_EMPTY_PX ( 1 )

/* Ink at least this strong counts as visible, a quarter of full. */
#define DIAL_INK_VISIBLE ( 4u )

#define DIAL_MAX_TICKS  ( 48u )
#define DIAL_MAX_LABELS ( 16u )

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

typedef struct
{
    int16_t deg;
    bool major;
} dial_tick_t;

typedef struct
{
    int16_t x;
    int16_t y;
    uint8_t scale;
    char text[DIAL_LABEL_BUF];
} dial_label_t;

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

/* The ink at pixel (x, y) from the top left corner of a label, whose
 * characters sit side by side in cells. Both are inside the label. */
static uint8_t label_ink_at(
    const char * text,
    uint8_t scale,
    int32_t x,
    int32_t y
)
{
    int32_t cell_w = ( int32_t )FONT_CELL_W * ( int32_t )scale;
    int32_t glyph  = x / cell_w;
    int32_t within = x % cell_w;

    return font_alpha( text[glyph], scale, ( uint16_t )within, ( uint16_t )y );
}

/* The smallest box, in pixels from the label's top left corner, that holds
 * the label's visible ink: x0 and y0 are the first pixel with ink and x1 and
 * y1 the last. A label with no ink gets its whole cell. */
static void label_ink_box(
    const char * text,
    uint8_t scale,
    int32_t * x0,
    int32_t * y0,
    int32_t * x1,
    int32_t * y1
)
{
    int32_t width  = label_width( strlen( text ), scale );
    int32_t height = ( int32_t )FONT_CELL_H * ( int32_t )scale;
    bool any       = false;

    *x0 = width;
    *y0 = height;
    *x1 = 0;
    *y1 = 0;
    for ( int32_t y = 0; y < height; y++ )
    {
        for ( int32_t x = 0; x < width; x++ )
        {
            uint8_t ink = label_ink_at( text, scale, x, y );

            if ( ink >= DIAL_INK_VISIBLE )
            {
                any = true;
                *x0 = ( x < *x0 ) ? x : *x0;
                *y0 = ( y < *y0 ) ? y : *y0;
                *x1 = ( x > *x1 ) ? x : *x1;
                *y1 = ( y > *y1 ) ? y : *y1;
            }
        }
    }
    if ( !any )
    {
        *x0 = 0;
        *y0 = 0;
        *x1 = width - 1;
        *y1 = height - 1;
    }
}

/* The top left corner of a label whose middle is r pixels from the dial's
 * center along this direction. */
static void place_label(
    size_t length,
    uint8_t scale,
    int32_t r,
    int32_t sin_m,
    int32_t cos_m,
    int32_t * x,
    int32_t * y
)
{
    int32_t label_h = ( int32_t )FONT_CELL_H * ( int32_t )scale;

    *x = DIAL_CX + ( ( r * sin_m ) / 1000 )
         - ( label_width( length, scale ) / 2 );
    *y = DIAL_CY - ( ( r * cos_m ) / 1000 ) - ( label_h / 2 );
}

/* Whether any ink of the label, with its middle r pixels out, falls on its own
 * long tick once that tick is grown by grow pixels: longer toward the center
 * and wider on both sides. Growing by 1 reaches the pixels right beside the
 * tick, so a label that is clear at grow 1 has an empty pixel between. */
static bool arc_label_touches_tick(
    const char * text,
    uint8_t scale,
    int32_t r,
    int32_t sin_m,
    int32_t cos_m,
    int32_t grow
)
{
    size_t length   = strlen( text );
    int32_t width   = label_width( length, scale );
    int32_t height  = ( int32_t )FONT_CELL_H * ( int32_t )scale;
    int32_t inner_r = DIAL_TICK_R - DIAL_TICK_MAJOR_LEN;
    int32_t x0;
    int32_t y0;
    bool touches = false;

    place_label( length, scale, r, sin_m, cos_m, &x0, &y0 );
    for ( int32_t y = 0; !touches && ( y < height ); y++ )
    {
        for ( int32_t x = 0; !touches && ( x < width ); x++ )
        {
            uint8_t ink = label_ink_at( text, scale, x, y );

            if ( ink >= DIAL_INK_VISIBLE )
            {
                touches = radial_line_alpha(
                              ( x0 + x ) - DIAL_CX,
                              ( y0 + y ) - DIAL_CY,
                              sin_m,
                              cos_m,
                              inner_r - grow,
                              DIAL_TICK_R,
                              DIAL_TICK_MAJOR_HALF_W + grow
                          )
                          >= DIAL_INK_VISIBLE;
            }
        }
    }
    return touches;
}

/* How far from the center the middle of an arc number goes, in pixels, for
 * its ink to sit DIAL_ARC_LABEL_EMPTY_PX from the end of its long tick: as
 * close as it can without touching. The ink's bounding box gives a first
 * guess that is never too far out (it is axis aligned and the tick is not),
 * and whole pixels then close the rest of the way against the real ink. */
static int32_t arc_label_radius_search(
    const char * text,
    uint8_t scale,
    int32_t sin_m,
    int32_t cos_m
)
{
    int32_t x0;
    int32_t y0;
    int32_t x1;
    int32_t y1;
    int32_t width   = label_width( strlen( text ), scale );
    int32_t height  = ( int32_t )FONT_CELL_H * ( int32_t )scale;
    int32_t reach   = INT32_MIN;
    int32_t inner_r = DIAL_TICK_R - DIAL_TICK_MAJOR_LEN;
    int32_t r;

    label_ink_box( text, scale, &x0, &y0, &x1, &y1 );
    for ( int32_t i = 0; i < 4; i++ )
    {
        /* Corner pixel centers, from the middle of the label, in thousandths
         * of a pixel. */
        int32_t cx =
            ( ( 2 * ( ( ( i % 2 ) == 0 ) ? x0 : x1 ) ) - ( width - 1 ) ) * 500;
        int32_t cy = ( ( 2 * ( ( i < 2 ) ? y0 : y1 ) ) - ( height - 1 ) ) * 500;
        int32_t along = ( ( cx * sin_m ) - ( cy * cos_m ) ) / 1000;

        reach = ( along > reach ) ? along : reach;
    }
    r = ( ( ( inner_r * 1000 ) - DIAL_ARC_LABEL_GAP - reach ) + 500 ) / 1000;

    /* Back in if the guess touches, then out while one more step is clear. */
    for ( int32_t guard = 0;
          ( guard < inner_r )
          && arc_label_touches_tick(
              text, scale, r, sin_m, cos_m, DIAL_ARC_LABEL_EMPTY_PX
          );
          guard++ )
    {
        r--;
    }
    for ( int32_t guard = 0;
          ( guard < inner_r )
          && !arc_label_touches_tick(
              text, scale, r + 1, sin_m, cos_m, DIAL_ARC_LABEL_EMPTY_PX
          );
          guard++ )
    {
        r++;
    }
    return r;
}

/* The search above reads the font pixel by pixel, which is too slow to repeat
 * for every needle frame, and a dial's numbers never move. The radius of each
 * arc number is kept for as long as the scale that decides its text and angle
 * stays the same, so another dial, or this one changed, starts afresh. */
typedef struct
{
    int32_t min_value;
    int32_t max_value;
    int32_t major_step;
    int32_t label_div;
    bool known[DIAL_MAX_LABELS];
    int16_t radius[DIAL_MAX_LABELS];
} arc_radius_cache_t;

static int32_t arc_label_radius(
    const dial_cfg_t * cfg,
    size_t index,
    const char * text,
    int32_t sin_m,
    int32_t cos_m
)
{
    static arc_radius_cache_t cache;
    int32_t r;

    if ( ( cache.min_value != cfg->min_value )
         || ( cache.max_value != cfg->max_value )
         || ( cache.major_step != cfg->major_step )
         || ( cache.label_div != cfg->label_div ) )
    {
        cache            = ( arc_radius_cache_t ){ 0 };
        cache.min_value  = cfg->min_value;
        cache.max_value  = cfg->max_value;
        cache.major_step = cfg->major_step;
        cache.label_div  = cfg->label_div;
    }
    if ( !cache.known[index] )
    {
        cache.radius[index] = ( int16_t )arc_label_radius_search(
            text, DIAL_LABEL_SCALE, sin_m, cos_m
        );
        cache.known[index] = true;
    }
    r = cache.radius[index];
    return r;
}

static void build_ticks(
    const dial_cfg_t * cfg,
    dial_tick_t * ticks,
    size_t * tick_count,
    dial_label_t * labels,
    size_t * label_count
)
{
    uint32_t subdivisions = ( uint32_t )cfg->minor_per_major + 1u;
    int32_t minor_step    = cfg->major_step / ( int32_t )subdivisions;
    int32_t value         = cfg->min_value;
    size_t nt             = 0;
    size_t nl             = 0;

    while ( ( value <= cfg->max_value ) && ( nt < DIAL_MAX_TICKS ) )
    {
        int16_t deg = value_to_deg( cfg, value );
        bool is_major =
            ( ( ( value - cfg->min_value ) % cfg->major_step ) == 0 );

        ticks[nt].deg   = deg;
        ticks[nt].major = is_major;
        nt++;

        if ( is_major && ( nl < DIAL_MAX_LABELS ) )
        {
            int32_t sin_m;
            int32_t cos_m;
            char text[DIAL_LABEL_BUF];
            size_t len;
            int32_t x;
            int32_t y;

            angle_vec( deg, &sin_m, &cos_m );
            len = textfit_int( text, DIAL_LABEL_BUF, value / cfg->label_div );
            int32_t label_r =
                ( cfg->style == DIAL_STYLE_UPPER_ARC )
                    ? arc_label_radius( cfg, nl, text, sin_m, cos_m )
                    : DIAL_LABEL_R;

            place_label( len, DIAL_LABEL_SCALE, label_r, sin_m, cos_m, &x, &y );
            labels[nl].x     = ( int16_t )x;
            labels[nl].y     = ( int16_t )y;
            labels[nl].scale = DIAL_LABEL_SCALE;
            ( void )textfit_copy( labels[nl].text, DIAL_LABEL_BUF, text );
            nl++;
        }
        value += minor_step;
    }
    *tick_count  = nt;
    *label_count = nl;
}

static void add_fixed_labels(
    const dial_cfg_t * cfg,
    dial_label_t * labels,
    size_t * label_count
)
{
    size_t n = *label_count;

    if ( ( cfg->label != NULL ) && ( n < DIAL_MAX_LABELS ) )
    {
        labels[n].x =
            ( int16_t )( DIAL_CX
                         - ( label_width(
                                 strlen( cfg->label ), DIAL_FIXED_SCALE
                             )
                             / 2 ) );
        labels[n].y     = ( int16_t )( DIAL_CY
                                   - ( ( cfg->style == DIAL_STYLE_UPPER_ARC )
                                               ? UI_LEN( 300 )
                                               : UI_LEN( 259 ) ) );
        labels[n].scale = DIAL_FIXED_SCALE;
        ( void )textfit_copy( labels[n].text, DIAL_LABEL_BUF, cfg->label );
        n++;
    }

    if ( ( cfg->unit_pos != NULL ) && ( n < DIAL_MAX_LABELS ) )
    {
        labels[n].x     = ( int16_t )( ( cfg->style == DIAL_STYLE_UPPER_ARC )
                                           ? DIAL_CX
                                             - ( label_width(
                                                     strlen( cfg->unit_pos ),
                                                     DIAL_FIXED_SCALE
                                                 )
                                                 / 2 )
                                           : DIAL_CX + UI_LEN( 325 ) );
        labels[n].y     = ( int16_t )( ( cfg->style == DIAL_STYLE_UPPER_ARC )
                                           ? DIAL_CY - UI_LEN( 192 )
                                           : DIAL_CY + UI_LEN( 42 ) );
        labels[n].scale = DIAL_FIXED_SCALE;
        ( void )textfit_copy( labels[n].text, DIAL_LABEL_BUF, cfg->unit_pos );
        n++;
    }

    if ( ( cfg->unit_neg != NULL ) && ( n < DIAL_MAX_LABELS ) )
    {
        labels[n].x     = ( int16_t )( DIAL_CX - UI_LEN( 550 ) );
        labels[n].y     = ( int16_t )( DIAL_CY + UI_LEN( 192 ) );
        labels[n].scale = DIAL_FIXED_SCALE;
        ( void )textfit_copy( labels[n].text, DIAL_LABEL_BUF, cfg->unit_neg );
        n++;
    }
    *label_count = n;
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

/* Draw order: hub, needle, labels, ticks, face. */
static uint16_t pixel_color(
    const dial_cfg_t * cfg,
    const dial_tick_t * ticks,
    size_t tick_count,
    const dial_label_t * labels,
    size_t label_count,
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
    for ( size_t i = 0; i < tick_count; i++ )
    {
        int32_t sin_m;
        int32_t cos_m;
        int32_t len =
            ticks[i].major ? DIAL_TICK_MAJOR_LEN : DIAL_TICK_MINOR_LEN;
        int32_t outer_r = DIAL_TICK_R;

        angle_vec( ticks[i].deg, &sin_m, &cos_m );
        ink = radial_line_alpha(
            dx,
            dy,
            sin_m,
            cos_m,
            outer_r - len,
            outer_r,
            ticks[i].major ? DIAL_TICK_MAJOR_HALF_W : DIAL_TICK_HALF_W
        );
        if ( ink != 0u )
        {
            color = font_blend( cfg->tick_fg, color, ink );
        }
    }
    for ( size_t i = 0; i < label_count; i++ )
    {
        ink = 0u;
        if ( label_pixel( &labels[i], row, col, &ink ) )
        {
            color = font_blend( cfg->tick_fg, color, ink );
        }
    }
    ink = needle_alpha( dx, dy, needle_sin, needle_cos );
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
static void draw_region(
    const dial_widget_t * d,
    int16_t x0,
    int16_t y0,
    int16_t x1,
    int16_t y1
)
{
    static uint16_t region_buf[DIAL_REGION_PIXELS];
    dial_tick_t ticks[DIAL_MAX_TICKS];
    dial_label_t labels[DIAL_MAX_LABELS];
    size_t tick_count;
    size_t label_count;

    int32_t needle_sin;
    int32_t needle_cos;

    int32_t width_delta  = ( int32_t )x1 - ( int32_t )x0;
    int32_t height_delta = ( int32_t )y1 - ( int32_t )y0;
    uint16_t width       = ( uint16_t )width_delta;
    int16_t row;
    size_t row_index = 0u;

    build_ticks( d->cfg, ticks, &tick_count, labels, &label_count );
    add_fixed_labels( d->cfg, labels, &label_count );
    angle_vec( d->displayed_deg, &needle_sin, &needle_cos );

    for ( row = y0; row < y1; row++ )
    {
        uint16_t * dst = &region_buf[row_index * ( size_t )width];
        int16_t col;

        for ( col = x0; col < x1; col++ )
        {
            dst[col - x0] = pixel_color(
                d->cfg,
                ticks,
                tick_count,
                labels,
                label_count,
                needle_sin,
                needle_cos,
                row,
                col
            );
        }
        row_index++;
    }
    ( void )display_blit(
        ( uint16_t )x0,
        ( uint16_t )y0,
        width,
        ( uint16_t )height_delta,
        region_buf
    );
}

static void draw_frame( const dial_widget_t * d )
{
    static uint16_t row_buf[DIAL_PANEL_W];
    dial_tick_t ticks[DIAL_MAX_TICKS];
    dial_label_t labels[DIAL_MAX_LABELS];
    size_t tick_count;
    size_t label_count;
    int32_t needle_sin;
    int32_t needle_cos;
    int16_t row;

    build_ticks( d->cfg, ticks, &tick_count, labels, &label_count );
    add_fixed_labels( d->cfg, labels, &label_count );
    angle_vec( d->displayed_deg, &needle_sin, &needle_cos );

    for ( row = 0; row < ( int16_t )DIAL_PANEL_H; row++ )
    {
        int16_t col;

        for ( col = 0; col < ( int16_t )DIAL_PANEL_W; col++ )
        {
            row_buf[col] = pixel_color(
                d->cfg,
                ticks,
                tick_count,
                labels,
                label_count,
                needle_sin,
                needle_cos,
                row,
                col
            );
        }
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
    d->cfg           = cfg;
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
