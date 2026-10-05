/* How much of a pixel a shape covers, for the shapes a dial is drawn from:
 * a straight bar, a disc and the needle. */
#include "lib/ui/dial_shape.h"

#include "lib/ui/dial_layout.h"
#include "lib/ui/ui_math.h"

/* How much of a pixel a shape covers, from its edge's distance: positive
 * inside, in thousandths of a pixel, measured from the pixel's center. A
 * center on the edge is half covered, and one half a pixel in or out is all
 * or none, so a slanted edge fades over one pixel instead of stair-stepping.
 * The distance is straight across the edge, which the rows of the shape's own
 * axis give exactly and a slanted edge to within a few percent. */
uint8_t dial_edge_alpha( int32_t dist_milli )
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
uint8_t dial_radial_line_alpha(
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
    int32_t across = ( half_w * 1000 ) - ui_abs( perp );
    int32_t before = proj - ( inner_r * 1000 );
    int32_t beyond = ( outer_r * 1000 ) - proj;
    int32_t dist   = across;

    dist = ui_min( before, dist );
    dist = ui_min( beyond, dist );
    return dial_edge_alpha( dist );
}

/* A broad pointer that tapers gently along its blade, then ends in a short
 * point. The tip keeps half a pixel of width so it remains visible. */
uint8_t dial_needle_alpha(
    int32_t dx,
    int32_t dy,
    int32_t sin_milli,
    int32_t cos_milli,
    int32_t len
)
{
    int32_t proj   = ( dx * sin_milli ) - ( dy * cos_milli );
    uint8_t alpha  = 0u;
    int32_t beyond = ( len * 1000 ) - proj;

    if ( ( len > 0 ) && ( proj >= -500 ) && ( beyond >= -500 ) )
    {
        int32_t perp    = ( dx * cos_milli ) + ( dy * sin_milli );
        int32_t across  = ui_abs( perp );
        int32_t tip_run = ui_max( beyond, 0 );
        int32_t room =
            ( ( DIAL_NEEDLE_HALF_W * tip_run ) / DIAL_NEEDLE_TIP_LEN ) + 500;
        int32_t blade_room = ( DIAL_NEEDLE_HALF_W * 1000 )
                             - ( ( proj * ( DIAL_NEEDLE_HALF_W / 3 ) ) / len );
        int32_t dist;

        if ( room > blade_room )
        {
            room = blade_room;
        }
        dist  = room - across;
        dist  = ui_min( beyond, dist );
        dist  = ui_min( proj, dist );
        alpha = dial_edge_alpha( dist );
    }
    return alpha;
}

/* A disc of this radius at the center: coverage from how far the pixel's
 * distance is inside the edge, taking (r*r - d*d) / 2r for r - d. */
uint8_t dial_disc_alpha(
    int32_t dist_sq,
    int32_t radius
)
{
    return dial_edge_alpha(
        ( ( ( radius * radius ) - dist_sq ) * 500 ) / radius
    );
}
