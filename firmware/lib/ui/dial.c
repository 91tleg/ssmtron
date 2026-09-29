/* Render dial geometry into one row buffer; no bitmap is stored. */
#include "dial.h"

#include <string.h>

#include "display.h"
#include "font.h"

#define DIAL_PANEL_W ( 240 )
#define DIAL_PANEL_H ( 240 )
#define DIAL_CX      ( 120 )
#define DIAL_CY      ( 120 )

/* 270 degree sweep, with zero straight up and positive clockwise. */
#define DIAL_SWEEP_MIN_DEG  ( -135 )
#define DIAL_SWEEP_MAX_DEG  ( 135 )
#define DIAL_ANGLE_COUNT    ( 271u )

/* Unwrapped angles keep the needle on the lower arc. */
#define DIAL_DEFAULT_MIN_DEG  ( 135 )
#define DIAL_DEFAULT_ZERO_DEG ( 270 )
#define DIAL_DEFAULT_MAX_DEG  ( 435 )
#define DIAL_DEFAULT_TICK_R   ( 99 )

#define DIAL_TICK_MAJOR_LEN ( 16 )
#define DIAL_TICK_MINOR_LEN ( 8 )
#define DIAL_TICK_HALF_W    ( 1 )

#define DIAL_NEEDLE_LEN ( 78 )

#define DIAL_FRAME_INTERVAL_MS ( 33u ) /* ~30 fps */

#define DIAL_MAX_TICKS  ( 32u )
#define DIAL_MAX_LABELS ( 16u )

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
    char text[DIAL_TEXT_MAX + 1u];
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
    if ( ( cfg->min_value < 0 ) && ( cfg->max_value > 0 ) )
    {
        if ( clamped <= 0 )
        {
            offset = ( int32_t )( ( ( int64_t )( clamped - cfg->min_value )
                                    * ( DIAL_DEFAULT_ZERO_DEG - DIAL_DEFAULT_MIN_DEG ) )
                                  / -cfg->min_value );
            deg    = ( int16_t )( DIAL_DEFAULT_MIN_DEG + offset );
        }
        else
        {
            offset = ( int32_t )( ( ( int64_t )clamped
                                    * ( DIAL_DEFAULT_MAX_DEG - DIAL_DEFAULT_ZERO_DEG ) )
                                  / cfg->max_value );
            deg    = ( int16_t )( DIAL_DEFAULT_ZERO_DEG + offset );
        }
    }
    else
    {
        int32_t span = cfg->max_value - cfg->min_value;

        offset = ( int32_t )( ( ( int64_t )( clamped - cfg->min_value )
                                * ( DIAL_DEFAULT_MAX_DEG - DIAL_DEFAULT_MIN_DEG ) )
                              / span );
        deg    = ( int16_t )( DIAL_DEFAULT_MIN_DEG + offset );
    }
    return deg;
}

/* Where the needle rests: zero when the scale crosses it, else the low end. */
static int16_t rest_deg( const dial_cfg_t * cfg )
{
    return value_to_deg( cfg, ( cfg->min_value < 0 ) ? 0 : cfg->min_value );
}

static int16_t step_toward(
    int16_t current,
    int16_t target,
    uint16_t max_step
)
{
    int16_t diff = ( int16_t )( target - current );
    int16_t result;

    if ( diff > ( int16_t )max_step )
    {
        result = ( int16_t )( current + ( int16_t )max_step );
    }
    else if ( diff < -( int16_t )max_step )
    {
        result = ( int16_t )( current - ( int16_t )max_step );
    }
    else
    {
        result = target;
    }
    return result;
}

/* Truncate labels to fit the fixed buffer. */
static void copy_bounded(
    char * dst,
    const char * src
)
{
    size_t n = strlen( src );

    if ( n > DIAL_TEXT_MAX )
    {
        n = DIAL_TEXT_MAX;
    }
    ( void )memcpy( dst, src, n );
    dst[n] = '\0';
}

static int32_t label_width(
    size_t length,
    uint8_t scale
)
{
    int32_t characters  = ( int32_t )length;
    int32_t glyph_width = ( int32_t )FONT_W + 1;

    return characters * glyph_width * ( int32_t )scale;
}

static size_t format_int(
    int32_t v,
    char * buf
)
{
    char tmp[12];
    size_t n   = 0;
    bool neg   = ( v < 0 );
    uint32_t u = neg ? ( uint32_t )( -v ) : ( uint32_t )v;

    do
    {
        tmp[n] = ( char )( '0' + ( u % 10u ) );
        u /= 10u;
        n++;
    } while ( u != 0u );
    if ( neg )
    {
        tmp[n] = '-';
        n++;
    }
    for ( size_t i = 0; i < n; i++ )
    {
        buf[i] = tmp[n - 1u - i];
    }
    buf[n] = '\0';
    return n;
}

/* Test a radial segment without square root or angle calculations. */
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
    int32_t proj = ( dx * sin_milli ) - ( dy * cos_milli );
    int32_t perp = ( dx * cos_milli ) + ( dy * sin_milli );

    return ( proj >= ( inner_r * 1000 ) ) && ( proj <= ( outer_r * 1000 ) )
           && ( perp <= ( half_w * 1000 ) ) && ( perp >= -( half_w * 1000 ) );
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

        if ( is_major && ( nl < DIAL_MAX_LABELS ) && ( value != 0 ) )
        {
            int32_t sin_m;
            int32_t cos_m;
            char text[DIAL_TEXT_MAX + 1u];
            size_t len;
            int32_t x;
            int32_t y;
            uint8_t label_scale = 2u;
            int32_t label_h = ( int32_t )FONT_H * ( int32_t )label_scale;

            angle_vec( deg, &sin_m, &cos_m );
            len = format_int( value, text );
            int32_t label_r = 70;

            x = DIAL_CX + ( ( label_r * sin_m ) / 1000 )
                - ( label_width( len, label_scale ) / 2 );
            y = DIAL_CY - ( ( label_r * cos_m ) / 1000 ) - ( label_h / 2 );
            labels[nl].x     = ( int16_t )x;
            labels[nl].y     = ( int16_t )y;
            labels[nl].scale = label_scale;
            copy_bounded( labels[nl].text, text );
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
                         - ( label_width( strlen( cfg->label ), 1u ) / 2 ) );
        labels[n].y     = 89;
        labels[n].scale = 1u;
        copy_bounded( labels[n].text, cfg->label );
        n++;
    }

    if ( ( cfg->unit_pos != NULL ) && ( n < DIAL_MAX_LABELS ) )
    {
        labels[n].x     = 159;
        labels[n].y     = 125;
        labels[n].scale = 1u;
        copy_bounded( labels[n].text, cfg->unit_pos );
        n++;
    }

    if ( ( cfg->unit_neg != NULL ) && ( n < DIAL_MAX_LABELS ) )
    {
        labels[n].x     = 54;
        labels[n].y     = 143;
        labels[n].scale = 1u;
        copy_bounded( labels[n].text, cfg->unit_neg );
        n++;
    }
    *label_count = n;
}

static bool label_pixel(
    const dial_label_t * lb,
    int16_t row,
    int16_t col,
    uint16_t fg,
    uint16_t * out_color
)
{
    size_t len         = strlen( lb->text );
    uint16_t cell_w    = ( uint16_t )( ( FONT_W + 1u ) * lb->scale );
    uint16_t glyph_px  = ( uint16_t )( FONT_W * lb->scale );
    uint16_t font_rows = ( uint16_t )( FONT_H * lb->scale );
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

        if ( within < glyph_px )
        {
            const font_glyph_t * gl = font_glyph( lb->text[glyph_index] );
            uint8_t glyph_row       = ( uint8_t )( rel_row / lb->scale );
            uint8_t font_col        = ( uint8_t )( within / lb->scale );
            uint8_t bit             = ( uint8_t )( ( gl->rows[glyph_row]
                                         >> ( FONT_W - 1u - font_col ) )
                                       & 1u );

            if ( bit != 0u )
            {
                *out_color = fg;
                hit        = true;
            }
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
    bool set        = false;

    if ( dist_sq <= ( 12 * 12 ) )
    {
        color = ( dist_sq <= ( 9 * 9 ) ) ? 0xE73Cu : 0xA514u;
        set   = true;
    }

    if ( on_radial_line( dx, dy, -needle_sin, -needle_cos, 0, 31, 5 ) )
    {
        color = 0xCE79u;
        set   = true;
    }
    if ( !set
         && on_radial_line(
             dx,
             dy,
             needle_sin,
             needle_cos,
             0,
             DIAL_NEEDLE_LEN,
             4
         ) )
    {
        color = cfg->needle_fg;
        set   = true;
    }
    for ( size_t i = 0; ( !set ) && ( i < label_count ); i++ )
    {
        uint16_t lc = 0u;

        if ( label_pixel( &labels[i], row, col, cfg->tick_fg, &lc ) )
        {
            color = lc;
            set   = true;
        }
    }
    for ( size_t i = 0; ( !set ) && ( i < tick_count ); i++ )
    {
        int32_t sin_m;
        int32_t cos_m;
        int32_t len =
            ticks[i].major ? DIAL_TICK_MAJOR_LEN : DIAL_TICK_MINOR_LEN;
        int32_t outer_r = DIAL_DEFAULT_TICK_R;

        angle_vec( ticks[i].deg, &sin_m, &cos_m );
        if ( on_radial_line(
                 dx,
                 dy,
                 sin_m,
                 cos_m,
                 outer_r - len,
                 outer_r,
                 ticks[i].major ? 2 : DIAL_TICK_HALF_W
             ) )
        {
            color = cfg->tick_fg;
            set   = true;
        }
    }
    return color;
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

void dial_init(
    dial_widget_t * d,
    const dial_cfg_t * cfg,
    uint32_t now_ms
)
{
    d->cfg = cfg;
    d->displayed_deg = rest_deg( cfg );
    d->target_deg    = d->displayed_deg;
    d->last_frame_ms = now_ms;
    ( void )display_clear( cfg->face_bg );
    draw_frame( d );
}

void dial_poll(
    dial_widget_t * d,
    uint32_t now_ms,
    bool has_value,
    int32_t value
)
{
    d->target_deg =
        has_value ? value_to_deg( d->cfg, value ) : rest_deg( d->cfg );

    if ( ( d->displayed_deg != d->target_deg )
         && ( ( now_ms - d->last_frame_ms ) >= DIAL_FRAME_INTERVAL_MS ) )
    {
        d->displayed_deg = step_toward(
            d->displayed_deg, d->target_deg, d->cfg->max_step_deg
        );
        draw_frame( d );
        d->last_frame_ms = now_ms;
    }
}
