/* The parts of a dial's face that do not change while its needle moves. */
#ifndef DIAL_FACE_H
#define DIAL_FACE_H

#include <stdbool.h>
#include <stdint.h>

#include "lib/ui/dial.h"

/* Works out the whole face of a dial: its ticks, numbers, title and units. */
void dial_make_face(
    const dial_cfg_t * cfg,
    dial_face_t * face
);

/* Whether a label covers this pixel, and how much ink it puts there. */
bool dial_label_pixel(
    const dial_label_t * lb,
    int16_t row,
    int16_t col,
    uint8_t * out_alpha
);

#endif /* DIAL_FACE_H */
