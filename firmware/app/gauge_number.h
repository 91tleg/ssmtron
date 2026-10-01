/* Number page: the name, value and unit of one ECU parameter as big text. It
 * shows the parameters that have no dial, and a dial's parameter while a
 * notice such as SAVED replaces the dial.
 * gauge.c decides when it is shown; this file owns what it looks like. */
#ifndef GAUGE_NUMBER_H
#define GAUGE_NUMBER_H

#include <stdbool.h>
#include <stdint.h>

#include "lib/ecu/ecu.h"

/* Sets the page colors; takes effect at the next gauge_number_open. */
void gauge_number_set_colors(
    uint16_t face,
    uint16_t ink
);

/* Clears the panel to face and draws the whole page. The content arguments
 * are those of gauge_number_poll, so the first draw already shows them and
 * not what an earlier page left behind. */
void gauge_number_open(
    uint16_t face,
    const ecu_param_info_t * info,
    bool has_value,
    int32_t value,
    const char * notice
);

/* Redraws only what changed. info is the shown parameter, or NULL before the
 * ECU has any; it names the page even while there is no value. has_value says
 * whether value is fresh: without one the page shows dashes. notice replaces
 * the name when it is not empty, such as "SAVED". info and notice must stay
 * valid until the next call. */
void gauge_number_poll(
    const ecu_param_info_t * info,
    bool has_value,
    int32_t value,
    const char * notice
);

#endif /* GAUGE_NUMBER_H */
