/* Analog dial drawn one row at a time, with no stored bitmap. */
#ifndef DIAL_H
#define DIAL_H

#include <stdbool.h>
#include <stdint.h>

#include "lib/ui/dial_motion.h"

/* Maximum visible characters in a dial label. */
#define DIAL_TEXT_MAX ( 8u )

/* Scale layout. All layouts share the same needle and motion.
 * FULL_SWEEP: most of the circle, 135 to 435 degrees. A scale that crosses
 * zero is drawn two-sided, with zero at 270 degrees and the same angle per
 * unit on both sides; any other scale runs in a straight line.
 * UPPER_ARC: a short arc across the top, -75 to 75 degrees. */
typedef enum
{
    DIAL_STYLE_FULL_SWEEP,
    DIAL_STYLE_UPPER_ARC
} dial_style_t;

typedef struct
{
    int32_t min_value;       /* value at the sweep's counter-clockwise end,
                              * in the units dial_poll is given */
    int32_t max_value;       /* value at the sweep's clockwise end */
    int32_t major_step;      /* value spacing between labeled ticks; must
                              * evenly divide (max_value - min_value) */
    uint8_t minor_per_major; /* unlabeled ticks between each pair of
                              * labeled ones; major_step must divide
                              * evenly by (minor_per_major + 1) */
    int32_t label_div;       /* the scale's units per labeled unit, above 0:
                              * tick numbers show value / label_div, so a
                              * scale in thousandths keeps its precision
                              * while its labels stay whole */
    const char * label;      /* short word above center, e.g. "BOOST" */
    const char * unit_pos;   /* unit text for the positive (>= 0) side */
    const char * unit_neg;   /* unit text for the negative side; NULL if
                              * the scale does not change sign */
    uint16_t face_bg;
    uint16_t tick_fg; /* also used for tick numbers, label, and units */
    uint16_t needle_fg;
    uint16_t max_deg_per_s; /* needle's fastest speed, degrees per second;
                             * must be > 0 or the needle can never move */
    dial_style_t style;
} dial_cfg_t;

typedef struct
{
    const dial_cfg_t * cfg;
    int16_t displayed_deg; /* the angle actually drawn, in whole degrees */
    int16_t target_deg;    /* the angle the latest value maps to */
    dial_motion_t motion;  /* its motion, see dial_motion.h */
} dial_widget_t;

/* True once the needle has reached the value it was last given. */
bool dial_settled( const dial_widget_t * d );

/* Draw the face with the needle at min_value. */
void dial_init(
    dial_widget_t * d,
    const dial_cfg_t * cfg,
    uint32_t now_ms
);

/* Move toward value (in the units of the cfg's scale) like a damped needle: it
 * closes on the value smoothly and is never faster than max_deg_per_s. Motion
 * follows the time that has passed, so a late frame moves it further instead of
 * slowing it. Frames come at a steady interval and are drawn only when the
 * needle's whole degree changes. If has_value is false, return to min_value. */
void dial_poll(
    dial_widget_t * d,
    uint32_t now_ms,
    bool has_value,
    int32_t value
);

#endif /* DIAL_H */
