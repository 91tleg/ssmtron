/* The extras on a dial's face: the two slotted screws, and the arc style's
 * cover plate with its flat feet, its raised body and the body's shadow. */
#include "lib/ui/dial_cover.h"

#include "lib/ui/dial_layout.h"
#include "lib/ui/dial_shape.h"
#include "lib/ui/font.h"

static int32_t abs_i32( int32_t v )
{
    return ( v < 0 ) ? -v : v;
}

/* A recessed cross with arms that narrow toward their ends. Coordinates
 * are rotated into the screw's axes, in thousandths of a pixel. */
static int32_t slot_dist(
    int32_t across,
    int32_t along
)
{
    int32_t reach     = DIAL_SCREW_R * 650;
    int32_t width     = DIAL_SCREW_R * 130;
    int32_t long_axis = abs_i32( along );
    int32_t dist =
        width - ( ( width * long_axis ) / ( reach * 2 ) ) - abs_i32( across );
    int32_t ends = reach - long_axis;

    dist = ( ends < dist ) ? ends : dist;
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
    int32_t slot   = ( slot_a > slot_b ) ? slot_a : slot_b;
    uint16_t rim;
    uint16_t metal;
    uint16_t out;

    light = ( light < 0 ) ? 0 : light;
    light = ( light > 10 ) ? 10 : light;
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
        int32_t sdx       = dx - at_x;
        int32_t sdy       = dy - at_y;
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
    return out;
}

/* The integer square root. */
static int32_t isqrt_u64( uint64_t v )
{
    uint64_t bit  = ( uint64_t )1 << 62;
    uint64_t res  = 0u;
    uint64_t rest = v;

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
    return ( int32_t )res;
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
    int32_t across_m = ( ( dx < 0 ) ? -dx : dx ) * 1000;
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
    int32_t dist     = ( top_m < side_m ) ? top_m : side_m;

    *up_m = ( top_m < side_m ) ? 1000 : 0;
    if ( ( across_m > corner_x ) && ( ( dy * 1000 ) < corner_y ) )
    {
        int64_t ex       = ( int64_t )across_m - ( int64_t )corner_x;
        int32_t dy_m     = dy * 1000;
        int64_t ey       = ( int64_t )dy_m - ( int64_t )corner_y;
        int64_t sum_sq   = ( ex * ex ) + ( ey * ey );
        uint64_t root_of = ( uint64_t )sum_sq;
        int32_t root     = isqrt_u64( root_of );

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
    int32_t near    = ( rim_m > 0 ) ? rim_m : 0;

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
    int64_t center_sq       = ( cx_m * cx_m ) + ( panel_cy_m * panel_cy_m );
    int64_t pixel_sq        = ( x_m * x_m ) + ( panel_y_m * panel_y_m );
    uint64_t center_root_of = ( uint64_t )center_sq;
    uint64_t pixel_root_of  = ( uint64_t )pixel_sq;
    int32_t gauge_r_m       = isqrt_u64( center_root_of ) + round_m;
    int32_t curve           = gauge_r_m - isqrt_u64( pixel_root_of );
    /* A 0.12 slope is about seven degrees. 1007 and 993 normalize the
     * sloped top's offset and distance, respectively. */
    int32_t top = ( int32_t )( ey_m - ( ( ex_m * 120 ) / 1000 ) )
                  + ( DIAL_COVER_FOOT_R * 1007 );
    int32_t dist;

    top  = ( top * 993 ) / 1000;
    dist = ( top < curve ) ? top : curve;
    /* Between the top's outward normal and the circular edge's normal,
     * the small corner circle replaces their sharp intersection. */
    if ( ( ( ( ex_m * 1000 ) + ( ey_m * 120 ) ) > 0 )
         && ( ( ey_m * cx_m ) < ( ex_m * panel_cy_m ) ) )
    {
        int64_t sum_sq   = ( ex_m * ex_m ) + ( ey_m * ey_m );
        uint64_t root_of = ( uint64_t )sum_sq;

        dist = round_m - isqrt_u64( root_of );
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
    int32_t across = ( dx < 0 ) ? -dx : dx;
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
