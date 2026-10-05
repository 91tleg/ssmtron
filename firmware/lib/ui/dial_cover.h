/* What is drawn on a dial's face besides its ticks, numbers and needle. Each
 * function blends its part over color and returns the result. dx and dy are
 * from the middle of the panel. */
#ifndef DIAL_COVER_H
#define DIAL_COVER_H

#include <stdbool.h>
#include <stdint.h>

#include "lib/ui/dial.h"

/* The two slotted screws: on the face of a round dial, on the cover's feet
 * on the arc style. */
uint16_t dial_screws(
    uint16_t color,
    int32_t dx,
    int32_t dy,
    bool arc
);

/* The cover's two rounded, tapered mounting ears, flat on the face. */
uint16_t dial_cover_feet(
    const dial_cfg_t * cfg,
    uint16_t color,
    int32_t dx,
    int32_t dy
);

/* The cover's raised body and its shadow, over the needle's base. */
uint16_t dial_cover_body(
    const dial_cfg_t * cfg,
    uint16_t color,
    int32_t dx,
    int32_t dy
);

#endif /* DIAL_COVER_H */
