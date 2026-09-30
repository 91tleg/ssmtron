/* The power-on needle sweep, as on a car's instrument cluster: the first
 * dial the gauge shows swings from rest to full scale, then on to the live
 * reading, or back to rest if there is none. It happens once per power-up,
 * and only if the first page is a dial. */
#ifndef GAUGE_SWEEP_H
#define GAUGE_SWEEP_H

#include <stdbool.h>
#include <stdint.h>

#include "lib/ui/dial.h"

/* Call every poll while a dial is shown. While it returns true the sweep
 * owns the needle and *value is the dial value to aim at. After that it
 * returns false for good and the live reading drives the dial, so the
 * needle comes back from full scale to the reading at the dial's own pace.
 * needle_settled is whether the needle has reached the value it was last
 * given. */
bool gauge_sweep_target(
    const dial_cfg_t * cfg,
    bool needle_settled,
    int32_t * value
);

/* No sweep, now or later: a page that is not a dial came first, or the user
 * moved to another page before it finished. */
void gauge_sweep_cancel( void );

#endif /* GAUGE_SWEEP_H */
