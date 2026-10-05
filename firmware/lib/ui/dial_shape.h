/* Pixel coverage of a dial's shapes: how much of a pixel is inside, from 0
 * (none) to FONT_ALPHA_MAX (all), measured from the pixel's center. Distances
 * and offsets are whole pixels, directions are sines and cosines in
 * thousandths. */
#ifndef DIAL_SHAPE_H
#define DIAL_SHAPE_H

#include <stdint.h>

/* Coverage from a distance to the shape's edge: positive inside, in
 * thousandths of a pixel. */
uint8_t dial_edge_alpha( int32_t dist_milli );

/* A straight bar of constant width along a direction from the origin, from
 * inner_r to outer_r, half_w to each side. dx and dy are from the origin. */
uint8_t dial_radial_line_alpha(
    int32_t dx,
    int32_t dy,
    int32_t sin_milli,
    int32_t cos_milli,
    int32_t inner_r,
    int32_t outer_r,
    int32_t half_w
);

/* The needle of this length: a bar that ends in a triangle. */
uint8_t dial_needle_alpha(
    int32_t dx,
    int32_t dy,
    int32_t sin_milli,
    int32_t cos_milli,
    int32_t len
);

/* A disc of this radius, from the squared distance to its center. */
uint8_t dial_disc_alpha(
    int32_t dist_sq,
    int32_t radius
);

#endif /* DIAL_SHAPE_H */
