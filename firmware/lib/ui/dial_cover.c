/* The extras on a dial's face: the two slotted screws, and the arc style's
 * cover plate with its flat feet, its raised body and the body's shadow. */
#include "lib/ui/dial_cover.h"

#include "lib/ui/dial_layout.h"
#include "lib/ui/ui_math.h"
#include "lib/ui/dial_shape.h"
#include "lib/ui/font.h"

/* A recessed cross with arms that narrow toward their ends. Coordinates
 * are rotated into the screw's axes, in thousandths of a pixel. */
static int32_t slot_dist(
    int32_t across,
    int32_t along
)
{
    int32_t reach     = DIAL_SCREW_R * 650;
    int32_t width     = DIAL_SCREW_R * 130;
    int32_t long_axis = ui_abs( along );
    int32_t dist =
        width - ( ( width * long_axis ) / ( reach * 2 ) ) - ui_abs( across );
    int32_t ends = reach - long_axis;

    dist = ui_min( ends, dist );
    return dist;
}

/* Subtle metal shading, a thin rim and a recessed cross distinguish the
 * head from a printed circle. Its contact shadow is drawn separately. */
static uint16_t screw_head(
    int32_t dx,
    int32_t dy,
    int32_t dist_sq,
    int32_t cos_m,
    int32_t sin_m
)
{
    int32_t light  = 5 - ( ( ( dx + dy ) * 3 ) / DIAL_SCREW_R );
    int32_t across = ( dx * cos_m ) + ( dy * sin_m );
    int32_t along  = ( dy * cos_m ) - ( dx * sin_m );
    int32_t slot_a = slot_dist( across, along );
    int32_t slot_b = slot_dist( along, across );
    int32_t slot   = ui_max( slot_a, slot_b );
    uint16_t rim;
    uint16_t metal;
    uint16_t out;

    light = ui_max( light, 0 );
    light = ui_min( light, 10 );
    rim   = font_blend( 0xD6BAu, 0x738Eu, ( uint8_t )light );
    metal = font_blend( 0xEF5Du, DIAL_SCREW_FACE_COLOR, ( uint8_t )light );
    out   = font_blend(
        metal, rim, dial_disc_alpha( dist_sq, DIAL_SCREW_R - UI_LEN( 9 ) )
    );
    /* A narrow light lip surrounds the dark screwdriver recess. */
    out = font_blend( 0xCE59u, out, dial_edge_alpha( slot + 300 ) );
    return font_blend( DIAL_SCREW_SLOT_COLOR, out, dial_edge_alpha( slot ) );
}

/* Both screws share the same lighting but have different slot rotations. */
uint16_t dial_screws(
    uint16_t color,
    int32_t dx,
    int32_t dy,
    bool arc
)
{
    /* Where the screws sit: x then y, thousandths of the radius from
     * center. */
    static const int16_t SCREW_PERMILLE[DIAL_SCREW_COUNT][2] = {
        { 280, -190 }, { 120, 260 }
    };
    /* Cosine and sine in thousandths: 15 degrees and -35 degrees. */
    static const int16_t SLOT_AXES[DIAL_SCREW_COUNT][2] = {
        { 966, 259 }, { 819, -574 }
    };
    uint16_t out = color;

    for ( size_t i = 0; i < DIAL_SCREW_COUNT; i++ )
    {
        /* On the arc style they sit on the cover's two feet. */
        int32_t at_x = arc ? ( ( i == 0u ) ? -( int32_t )DIAL_ARC_SCREW_X
                                           : ( int32_t )DIAL_ARC_SCREW_X )
                           : UI_LEN( SCREW_PERMILLE[i][0] );
        int32_t at_y =
            arc ? ( int32_t )DIAL_ARC_SCREW_Y : UI_LEN( SCREW_PERMILLE[i][1] );
        int32_t sdx   = dx - at_x;
        int32_t sdy   = dy - at_y;
        int32_t reach = DIAL_SCREW_R + ( 2 * UI_LEN( 9 ) ) + 1;

        /* Only pixels close to a screw, its head and its shadow, are shaded. */
        if ( ( ui_abs( sdx ) <= reach ) && ( ui_abs( sdy ) <= reach ) )
        {
            int32_t dist_sq   = ( sdx * sdx ) + ( sdy * sdy );
            uint8_t ink       = dial_disc_alpha( dist_sq, DIAL_SCREW_R );
            int32_t shadow_y  = sdy - UI_LEN( 9 );
            int32_t shadow_sq = ( sdx * sdx ) + ( shadow_y * shadow_y );
            uint8_t shadow_ink =
                dial_disc_alpha( shadow_sq, DIAL_SCREW_R + UI_LEN( 9 ) );

            /* A small contact shadow below the head gives it height without
             * changing the mounting ear's outline. */
            shadow_ink = ( uint8_t )( ( ( uint32_t )shadow_ink * 2u )
                                      / ( uint32_t )FONT_ALPHA_MAX );
            out        = font_blend( 0x0000u, out, shadow_ink );

            if ( ink != 0u )
            {
                uint16_t head = screw_head(
                    sdx, sdy, dist_sq, SLOT_AXES[i][0], SLOT_AXES[i][1]
                );

                out = font_blend( head, out, ink );
            }
        }
    }
    return out;
}

/* The integer square root of a value that fits 32 bits. Most distances here
 * do, and 32-bit steps are much cheaper than 64-bit ones on this chip. */
static uint32_t isqrt_u32( uint32_t v )
{
    uint32_t bit  = ( uint32_t )1 << 30;
    uint32_t res  = 0u;
    uint32_t rest = v;

    while ( bit > rest )
    {
        bit >>= 2;
    }
    while ( bit != 0u )
    {
        if ( rest >= ( res + bit ) )
        {
            rest -= res + bit;
            res = ( res >> 1 ) + bit;
        }
        else
        {
            res >>= 1;
        }
        bit >>= 2;
    }
    return res;
}

/* The integer square root. */
static int32_t isqrt_u64( uint64_t v )
{
    int32_t root = 0;

    if ( v <= ( uint64_t )UINT32_MAX )
    {
        root = ( int32_t )isqrt_u32( ( uint32_t )v );
    }
    else
    {
        uint64_t bit     = 0u;
        uint64_t res     = 0u;
        uint64_t rest    = v;
        uint64_t bit_pos = 0u;
        uint64_t even_pos;

        /* Start at the highest power of four that fits. Halving the shift
         * each step finds it in six tests instead of up to 31 loop passes. */
        uint64_t shift = 32u;

        while ( shift != 0u )
        {
            if ( ( v >> ( bit_pos + shift ) ) != 0u )
            {
                bit_pos += shift;
            }
            shift >>= 1;
        }
        even_pos = bit_pos - ( bit_pos % 2u );
        bit      = ( uint64_t )1 << even_pos;

        while ( bit != 0u )
        {
            if ( rest >= ( res + bit ) )
            {
                rest -= res + bit;
                res = ( res >> 1 ) + bit;
            }
            else
            {
                res >>= 1;
            }
            bit >>= 2;
        }
        root = ( int32_t )res;
    }
    return root;
}

/* The length of the vector (x_m, y_m), both in thousandths of a pixel. */
static int32_t hypot_m(
    int64_t x_m,
    int64_t y_m
)
{
    int64_t sum_sq   = ( x_m * x_m ) + ( y_m * y_m );
    uint64_t root_of = ( uint64_t )sum_sq;

    return isqrt_u64( root_of );
}

/* How far inside the cover's raised body a point is, in thousandths of a
 * pixel, negative outside. The body is a trapezoid: its top edge is at the
 * middle of the panel, its sides spread toward the bottom, and its top
 * corners are rounded. dx and dy are from the middle of the panel. up_m
 * comes back too: how much the nearest edge faces up, from 0 (a side) to
 * 1000 (the top), turning smoothly around a corner, so the shading has no
 * seam where the flat top meets the rounded shoulder. */
static int32_t body_dist(
    int32_t dx,
    int32_t dy,
    int32_t * up_m
)
{
    int32_t across_m = ui_abs( dx ) * 1000;
    int32_t spread =
        ( int32_t )DIAL_COVER_BOTTOM_HALF_W - ( int32_t )DIAL_COVER_TOP_HALF_W;
    int32_t half_m = ( ( int32_t )DIAL_COVER_TOP_HALF_W * 1000 )
                     + ( ( dy * spread * 1000 ) / ( int32_t )UI_RADIUS );
    int32_t round_m  = ( int32_t )DIAL_COVER_CORNER_R * 1000;
    int32_t corner_x = ( ( ( int32_t )DIAL_COVER_TOP_HALF_W * 1000 )
                         + ( ( ( int32_t )DIAL_COVER_CORNER_R * spread * 1000 )
                             / ( int32_t )UI_RADIUS )
                         + 500 )
                       - round_m;
    int32_t corner_y = round_m - 500;
    int32_t top_m    = ( dy * 1000 ) + 500;
    int32_t side_m   = ( half_m + 500 ) - across_m;
    int32_t dist     = ui_min( top_m, side_m );

    *up_m = ( top_m < side_m ) ? 1000 : 0;
    if ( ( across_m > corner_x ) && ( ( dy * 1000 ) < corner_y ) )
    {
        int64_t ex   = ( int64_t )across_m - ( int64_t )corner_x;
        int32_t dy_m = dy * 1000;
        int64_t ey   = ( int64_t )dy_m - ( int64_t )corner_y;
        int32_t root = hypot_m( ex, ey );

        dist = round_m - root;
        /* The edge's normal points from the corner's center to the pixel. */
        *up_m = ( root > 0 ) ? ( ( ( corner_y - dy_m ) * 1000 ) / root ) : 0;
        *up_m = ( *up_m > 1000 ) ? 1000 : *up_m;
    }
    return dist;
}

/* Whether the face is a light one, by its green. */
static bool is_light_face( uint16_t face_bg )
{
    return ( ( face_bg >> 5 ) & 0x3Fu ) >= 0x20u;
}

/* The body's color at a point inside it, from the face's own color. Near
 * its edge a rim catches the light where the edge faces up, and falls into
 * shade where it faces the side: up_m says which, from 0 to 1000. */
static uint16_t body_color(
    const dial_cfg_t * cfg,
    int32_t rim_m,
    int32_t up_m
)
{
    bool light      = is_light_face( cfg->face_bg );
    uint16_t high   = light ? 0xFFFFu : cfg->tick_fg;
    uint16_t color  = font_blend( high, cfg->face_bg, light ? 7u : 3u );
    int32_t bevel_m = ( int32_t )DIAL_COVER_BEVEL * 1000;
    int32_t shade_m = ( int32_t )DIAL_COVER_SIDE_SHADE * 1000;
    int32_t near    = ui_max( rim_m, 0 );

    if ( near < bevel_m )
    {
        int32_t lift = ( ( 8 * ( bevel_m - near ) ) / bevel_m * up_m ) / 1000;

        color = font_blend( high, color, ( uint8_t )lift );
    }
    if ( near < shade_m )
    {
        /* The raised shell has dark walls on either theme. The ink is light
         * on a dark face, so it cannot serve as the wall's shade. */
        int32_t shade =
            ( ( 2 * ( shade_m - near ) ) / shade_m * ( 1000 - up_m ) ) / 1000;

        color = font_blend( 0x2104u, color, ( uint8_t )shade );
    }
    if ( near < bevel_m )
    {
        /* A thin light lip along the walls. */
        int32_t lip =
            ( ( 2 * ( bevel_m - near ) ) / bevel_m * ( 1000 - up_m ) ) / 1000;

        color = font_blend( high, color, ( uint8_t )lip );
    }
    return color;
}

/* The ear's lower edge is a broad circular arc following the gauge rim. A small
 * outer corner joins that curve tangentially to the gently sloping top.
 * Distances and the corner center use thousandths of a pixel, so the two
 * curves meet smoothly at either panel size. */
/* The radius of the ear's lower arc, from the middle of the panel's lower
 * half. It is the same for every pixel, so it is worked out once. */
static int32_t foot_gauge_r_m(
    int64_t cx_m,
    int64_t panel_cy_m,
    int32_t round_m
)
{
    static int32_t gauge_r_m;

    if ( gauge_r_m == 0 )
    {
        gauge_r_m = hypot_m( cx_m, panel_cy_m ) + round_m;
    }
    return gauge_r_m;
}

/* How far inside the lower arc a pixel is, in thousandths of a pixel. The
 * edge only shows within two pixels of the arc, so a pixel further than that
 * is given a distance just past it without taking a square root. */
static int32_t foot_curve(
    int32_t gauge_r_m,
    int64_t x_m,
    int64_t panel_y_m
)
{
    int64_t pixel_sq = ( x_m * x_m ) + ( panel_y_m * panel_y_m );
    int64_t inner_m  = ( int64_t )gauge_r_m - DIAL_COVER_FOOT_EDGE_FAR_M;
    int64_t outer_m  = ( int64_t )gauge_r_m + DIAL_COVER_FOOT_EDGE_FAR_M;
    int32_t curve;

    if ( pixel_sq < ( inner_m * inner_m ) )
    {
        curve = DIAL_COVER_FOOT_EDGE_FAR_M;
    }
    else if ( pixel_sq > ( outer_m * outer_m ) )
    {
        curve = -DIAL_COVER_FOOT_EDGE_FAR_M;
    }
    else
    {
        curve = gauge_r_m - hypot_m( x_m, panel_y_m );
    }
    return curve;
}

static int32_t foot_dist(
    int32_t across,
    int32_t dy
)
{
    int32_t cx         = DIAL_COVER_FOOT_CX;
    int32_t cy         = DIAL_COVER_FOOT_CY;
    int32_t down       = DIAL_COVER_FOOT_DOWN;
    int64_t cx_m       = ( int64_t )cx * 1000;
    int64_t cy_m       = ( int64_t )cy * 1000;
    int64_t x_m        = ( int64_t )across * 1000;
    int64_t y_m        = ( int64_t )dy * 1000;
    int64_t ex_m       = x_m - cx_m;
    int64_t ey_m       = y_m - cy_m;
    int64_t panel_cy_m = cy_m + ( ( int64_t )down * 1000 );
    int64_t panel_y_m  = y_m + ( ( int64_t )down * 1000 );
    int32_t round_m    = DIAL_COVER_FOOT_R * 1000;
    /* Use panel coordinates for the lower arc, despite the ear's offset.
     * Its radius is tangent to the small corner circle, giving a constant
     * rim gap and a smooth join to the rounded tip. */
    int32_t gauge_r_m = foot_gauge_r_m( cx_m, panel_cy_m, round_m );
    int32_t curve     = foot_curve( gauge_r_m, x_m, panel_y_m );
    /* Both fit 32 bits (a few hundred thousand at most), so the division
     * does not need the slow 64-bit routine. */
    int32_t ex32 = ( int32_t )ex_m;
    int32_t ey32 = ( int32_t )ey_m;
    int32_t top  = ( ey32 - ( ( ex32 * DIAL_COVER_FOOT_SLOPE_MILLI ) / 1000 ) )
                  + ( DIAL_COVER_FOOT_R * DIAL_COVER_FOOT_LEN_MILLI );
    int32_t dist;

    top  = ( top * DIAL_COVER_FOOT_NORM_MILLI ) / 1000;
    dist = ui_min( top, curve );
    /* Between the top's outward normal and the circular edge's normal,
     * the small corner circle replaces their sharp intersection. */
    if ( ( ( ( ex_m * 1000 ) + ( ey_m * DIAL_COVER_FOOT_SLOPE_MILLI ) ) > 0 )
         && ( ( ey_m * cx_m ) < ( ex_m * panel_cy_m ) ) )
    {
        dist = round_m - hypot_m( ex_m, ey_m );
    }
    return dist;
}

/* Flat mounting ears, tucked under the raised center, with a thin lip. */
uint16_t dial_cover_feet(
    const dial_cfg_t * cfg,
    uint16_t color,
    int32_t dx,
    int32_t dy
)
{
    int32_t across = ui_abs( dx );
    int32_t foot_y = dy - DIAL_COVER_FOOT_DOWN;
    uint16_t out   = color;

    /* Most pixels cannot touch an ear. Avoid a square root for those. */
    if ( ( across >= UI_LEN( 450 ) )
         && ( across <= ( DIAL_COVER_FOOT_CX + DIAL_COVER_FOOT_R + 1 ) )
         && ( foot_y >= ( DIAL_COVER_FOOT_TOP - 1 ) )
         && ( foot_y
              <= ( DIAL_COVER_FOOT_BOTTOM + DIAL_COVER_FOOT_SHADOW + 1 ) ) )
    {
        int32_t foot   = foot_dist( across, foot_y );
        uint8_t ink    = dial_edge_alpha( foot );
        uint8_t shadow = dial_edge_alpha(
            foot_dist( across, foot_y - DIAL_COVER_FOOT_SHADOW )
        );
        uint32_t darkness = ( ( uint32_t )shadow * 2u ) / FONT_ALPHA_MAX;

        /* The downward offset reveals just a narrow shadow at the lower
         * return. The opaque ear covers the shadow everywhere else. */
        out = font_blend( 0x0000u, out, ( uint8_t )darkness );

        if ( ink != 0u )
        {
            bool light    = is_light_face( cfg->face_bg );
            uint16_t high = light ? 0xFFFFu : cfg->tick_fg;
            uint16_t flat = font_blend( high, cfg->face_bg, light ? 2u : 1u );

            if ( foot < 1500 )
            {
                flat = font_blend( high, flat, 4u );
            }
            out = font_blend( flat, out, ink );
        }
    }
    return out;
}

/* The body's soft shadow falls straight down so the outline stays symmetric. */
static uint16_t shadow_layer(
    uint16_t color,
    int32_t dx,
    int32_t dy
)
{
    int32_t up_m;
    int32_t spread_m = ( int32_t )DIAL_COVER_SHADOW_SPREAD * 1000;
    int32_t away =
        body_dist( dx, dy - ( int32_t )DIAL_COVER_SHADOW_OFFSET, &up_m );
    uint16_t out = color;

    if ( ( away < 0 ) && ( away > -spread_m ) )
    {
        int32_t fade     = ( 4 * ( spread_m + away ) ) / spread_m;
        int32_t darkness = ( fade * fade ) / 4;

        out = font_blend( 0x0000u, out, ( uint8_t )darkness );
    }
    return out;
}

/* The raised body, over the needle's base. */
static uint16_t body_layer(
    const dial_cfg_t * cfg,
    uint16_t color,
    int32_t dx,
    int32_t dy
)
{
    int32_t up_m;
    int32_t inside = body_dist( dx, dy, &up_m );
    uint8_t ink    = dial_edge_alpha( inside );
    uint16_t out   = color;

    if ( ink != 0u )
    {
        out = font_blend( body_color( cfg, inside, up_m ), out, ink );
    }
    return out;
}

uint16_t dial_cover_body(
    const dial_cfg_t * cfg,
    uint16_t color,
    int32_t dx,
    int32_t dy
)
{
    return body_layer( cfg, shadow_layer( color, dx, dy ), dx, dy );
}

bool dial_cover_interior(
    int32_t dx,
    int32_t dy
)
{
    int32_t half_w = ( int32_t )DIAL_COVER_TOP_HALF_W
                     + ( ( dy
                           * ( ( int32_t )DIAL_COVER_BOTTOM_HALF_W
                               - ( int32_t )DIAL_COVER_TOP_HALF_W ) )
                         / ( int32_t )UI_RADIUS );
    int32_t margin = DIAL_COVER_INTERIOR_MARGIN;
    /* Its rounded top corners cut in as far as the corner radius. */
    int32_t side_margin = ( dy < ( ( int32_t )DIAL_COVER_CORNER_R + margin ) )
                              ? ( ( int32_t )DIAL_COVER_CORNER_R + margin )
                              : margin;

    return ( dy >= margin ) && ( ui_abs( dx ) <= ( half_w - side_margin ) );
}

uint16_t dial_cover_base_color( const dial_cfg_t * cfg )
{
    return body_color( cfg, ( int32_t )DIAL_COVER_SIDE_SHADE * 2000, 0 );
}
