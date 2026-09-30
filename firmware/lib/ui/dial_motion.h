/* A damped needle's motion, with no drawing. It closes on its target
 * smoothly, is never faster than a top speed, and moves on a steady frame
 * rhythm. Motion follows the time that has passed, so a late frame moves the
 * needle further instead of slowing it. */
#ifndef DIAL_MOTION_H
#define DIAL_MOTION_H

#include <stdbool.h>
#include <stdint.h>

/* Time between frames (about 30 fps). Frames are scheduled from the previous
 * due time, not from when the last one ran, so the rhythm stays even. */
#define DIAL_MOTION_FRAME_MS ( 33u )

typedef struct
{
    int32_t pos_q;    /* the true position, 1/64 degree */
    uint32_t last_ms; /* when pos_q last moved */
    uint32_t next_ms; /* when the next move is due */
} dial_motion_t;

/* Rest at deg, with the first frame due one interval from now. */
void dial_motion_init(
    dial_motion_t * n,
    int16_t deg,
    uint32_t now_ms
);

/* True once the needle has reached target_deg. */
bool dial_motion_reached(
    const dial_motion_t * n,
    int16_t target_deg
);

/* Moves toward target_deg when a frame is due. The whole degree is written
 * to *deg. Returns true if it changed, so the caller redraws only then.
 * While at rest the clock for the next move restarts on every call, so a
 * move after a long rest does not count the rest as time spent moving. */
bool dial_motion_poll(
    dial_motion_t * n,
    uint32_t now_ms,
    int16_t target_deg,
    uint16_t max_deg_per_s,
    int16_t * deg
);

#endif /* DIAL_MOTION_H */
