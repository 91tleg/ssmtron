/* The power-on needle sweep, as on a car's instrument cluster: the first
 * dial the gauge shows swings from rest to full scale, down to the bottom of
 * the scale, and back to rest, and then the live reading takes over. It
 * happens once per power-up, and only if the first page is a dial. */
#ifndef GAUGE_SWEEP_H
#define GAUGE_SWEEP_H

#include <stdbool.h>
#include <stdint.h>

#include "lib/ui/dial.h"

/* Call every poll while a dial is shown. While it returns true the sweep
 * owns the needle and *value is the dial value to aim at. The aim moves
 * steadily, one full scale per second, from rest to the top of the scale,
 * to the bottom, and back to rest; each leg waits for the needle to arrive
 * before the next begins. After that it returns false for good and the live
 * reading drives the dial. needle_settled is whether the needle has reached
 * the value it was last given, and now_ms is the poll time. */
bool gauge_sweep_target(
    const dial_cfg_t * cfg,
    bool needle_settled,
    uint32_t now_ms,
    int32_t * value
);

/* No sweep, now or later: a page that is not a dial came first, or the user
 * moved to another page before it finished. */
void gauge_sweep_cancel( void );

#endif /* GAUGE_SWEEP_H */
