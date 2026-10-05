/* Where a value points on a dial. Angles are in degrees, clockwise from
 * straight up, and unwrapped so a needle on the lower arc keeps a steady
 * direction. */
#ifndef DIAL_ANGLE_H
#define DIAL_ANGLE_H

#include <stdint.h>

#include "lib/ui/dial.h"

/* The sine and cosine of deg in thousandths, for any angle. */
void dial_angle_vec(
    int16_t deg,
    int32_t * sin_milli,
    int32_t * cos_milli
);

/* The angle a reading points at, clamped to the scale's ends. */
int16_t dial_value_to_deg(
    const dial_cfg_t * cfg,
    int32_t value
);

/* Where the needle rests: zero when the scale crosses it, else the low end. */
int16_t dial_rest_deg( const dial_cfg_t * cfg );

#endif /* DIAL_ANGLE_H */
