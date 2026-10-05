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
    return dial_edge_alpha( dist );
}

/* Both screws blended over color, each a disc with an X cut in it. */
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
    uint16_t out = color;

    for ( size_t i = 0; i < DIAL_SCREW_COUNT; i++ )
    {
        /* On the arc style they sit on the cover's two feet. */
        int32_t at_x = arc ? ( ( i == 0u ) ? -( int32_t )DIAL_ARC_SCREW_X
                                           : ( int32_t )DIAL_ARC_SCREW_X )
                           : UI_LEN( SCREW_PERMILLE[i][0] );
        int32_t at_y =
            arc ? ( int32_t )DIAL_ARC_SCREW_Y : UI_LEN( SCREW_PERMILLE[i][1] );
        int32_t sdx = dx - at_x;
        int32_t sdy = dy - at_y;
        uint8_t ink =
            dial_disc_alpha( ( sdx * sdx ) + ( sdy * sdy ), DIAL_SCREW_R );

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
 * corners are rounded. dx and dy are from the middle of the panel. The
 * distances to the top and to the side come back too, for shading. */
static int32_t body_dist(
    int32_t dx,
    int32_t dy,
    int32_t * top_m,
    int32_t * side_m
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
    int32_t dist;

    *top_m  = ( dy * 1000 ) + 500;
    *side_m = ( half_m + 500 ) - across_m;
    dist    = ( *top_m < *side_m ) ? *top_m : *side_m;
    if ( ( across_m > corner_x ) && ( ( dy * 1000 ) < corner_y ) )
    {
        int64_t ex       = ( int64_t )across_m - ( int64_t )corner_x;
        int32_t dy_m     = dy * 1000;
        int64_t ey       = ( int64_t )dy_m - ( int64_t )corner_y;
        int64_t sum_sq   = ( ex * ex ) + ( ey * ey );
        uint64_t root_of = ( uint64_t )sum_sq;

        dist = round_m - isqrt_u64( root_of );
    }
    return dist;
}

/* Whether the face is a light one, by its green. */
static bool is_light_face( uint16_t face_bg )
{
    return ( ( face_bg >> 5 ) & 0x3Fu ) >= 0x20u;
}

/* The body's color at a point inside it, from the face's own color: lifted
 * toward white (or toward the ink on a dark face), a bright line along the
 * top edge and shade down the sides (more on the right, away from the
 * light). */
static uint16_t body_color(
    const dial_cfg_t * cfg,
    int32_t dx,
    int32_t top_m,
    int32_t side_m
)
{
    bool light      = is_light_face( cfg->face_bg );
    uint16_t high   = light ? 0xFFFFu : cfg->tick_fg;
    uint16_t color  = font_blend( high, cfg->face_bg, light ? 9u : 3u );
    int32_t bevel_m = ( int32_t )DIAL_COVER_BEVEL * 1000;
    int32_t shade_m = ( int32_t )DIAL_COVER_SIDE_SHADE * 1000;
    int32_t shade   = 0;

    if ( top_m < bevel_m )
    {
        int32_t lift = ( 6 * ( bevel_m - top_m ) ) / bevel_m;

        color = font_blend( high, color, ( uint8_t )lift );
    }
    if ( side_m < shade_m )
    {
        int32_t most = ( dx < 0 ) ? 2 : 5;
        int32_t near = ( side_m > 0 ) ? side_m : 0;

        shade = ( most * ( shade_m - near ) ) / shade_m;
    }
    return font_blend( cfg->tick_fg, color, ( uint8_t )shade );
}

/* The two flat feet either side of the body, with a thin edge line. */
uint16_t dial_cover_feet(
    const dial_cfg_t * cfg,
    uint16_t color,
    int32_t dx,
    int32_t dy
)
{
    int32_t across = ( dx < 0 ) ? -dx : dx;
    int32_t foot_a = ( ( dy - ( int32_t )DIAL_COVER_FOOT_TOP ) * 1000 ) + 500;
    int32_t foot_b =
        ( ( ( int32_t )DIAL_COVER_FOOT_BOTTOM - dy ) * 1000 ) + 500;
    int32_t foot = ( foot_a < foot_b ) ? foot_a : foot_b;
    int32_t foot_x;
    uint8_t ink;
    uint16_t out = color;

    foot_x = ( ( ( int32_t )DIAL_COVER_FOOT_X - across ) * 1000 ) + 500;
    foot   = ( foot_x < foot ) ? foot_x : foot;
    ink    = dial_edge_alpha( foot );
    if ( ink != 0u )
    {
        uint16_t flat = font_blend( cfg->tick_fg, cfg->face_bg, 1u );

        out = font_blend( flat, out, ink );
        if ( foot < 1000 )
        {
            out = font_blend( cfg->tick_fg, out, 3u );
        }
    }
    return out;
}

/* The body's soft shadow on the face and the feet, down and to the right. */
static uint16_t shadow_layer(
    const dial_cfg_t * cfg,
    uint16_t color,
    int32_t dx,
    int32_t dy
)
{
    int32_t top_m;
    int32_t side_m;
    int32_t spread_m = ( int32_t )DIAL_COVER_SHADOW_SPREAD * 1000;
    int32_t away     = body_dist(
        dx - ( int32_t )DIAL_COVER_SHADOW_OFFSET,
        dy - ( int32_t )DIAL_COVER_SHADOW_OFFSET,
        &top_m,
        &side_m
    );
    uint16_t out = color;

    if ( ( away < 0 ) && ( away > -spread_m ) )
    {
        int32_t darkness = ( 3 * ( spread_m + away ) ) / spread_m;

        out = font_blend( cfg->tick_fg, out, ( uint8_t )darkness );
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
    int32_t top_m;
    int32_t side_m;
    int32_t inside = body_dist( dx, dy, &top_m, &side_m );
    uint8_t ink    = dial_edge_alpha( inside );
    uint16_t out   = color;

    if ( ink != 0u )
    {
        out = font_blend( body_color( cfg, dx, top_m, side_m ), out, ink );
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
    return body_layer( cfg, shadow_layer( cfg, color, dx, dy ), dx, dy );
}
