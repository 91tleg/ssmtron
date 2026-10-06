/* Pixel coverage of a dial's shapes: how much of a pixel is inside, from 0
 * (none) to FONT_ALPHA_MAX (all), measured from the pixel's center. Distances
 * and offsets are whole pixels, directions are sines and cosines in
 * thousandths. */
#ifndef DIAL_SHAPE_H
#define DIAL_SHAPE_H

#include <stdint.h>

#include "lib/ui/font.h"
#include "lib/ui/ui_math.h"

/* A broad, tapered pointer of this length. A nonpositive length is empty. */
uint8_t dial_needle_alpha(
    int32_t dx,
    int32_t dy,
    int32_t sin_milli,
    int32_t cos_milli,
    int32_t len
);

/* How much of a pixel a shape covers, from its edge's distance: positive
 * inside, in thousandths of a pixel, measured from the pixel's center. A
 * center on the edge is half covered, and one half a pixel in or out is all
 * or none, so a slanted edge fades over one pixel instead of stair-stepping.
 * The distance is straight across the edge, which the rows of the shape's own
 * axis give exactly and a slanted edge to within a few percent. */
static inline uint8_t dial_edge_alpha( int32_t dist_milli )
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
static inline uint8_t dial_radial_line_alpha(
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

/* A disc of this radius at the center: coverage from how far the pixel's
 * distance is inside the edge, taking (r*r - d*d) / 2r for r - d. */
static inline uint8_t dial_disc_alpha(
    int32_t dist_sq,
    int32_t radius
)
{
    return dial_edge_alpha(
        ( ( ( radius * radius ) - dist_sq ) * 500 ) / radius
    );
}

#endif /* DIAL_SHAPE_H */
