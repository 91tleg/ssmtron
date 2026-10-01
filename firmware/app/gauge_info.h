/* Info page: the firmware version, the ECU's ROM ID and whether the ECU is
 * connected. It is the last page of the cycle and shows no ECU value.
 * gauge.c decides when it is shown; this file owns what it looks like. */
#ifndef GAUGE_INFO_H
#define GAUGE_INFO_H

#include <stdint.h>

/* Sets the page colors; takes effect at the next gauge_info_open. */
void gauge_info_set_colors(
    uint16_t face,
    uint16_t ink
);

/* Clears the panel to face and draws the whole page. notice is that of
 * gauge_info_poll, so the first draw already shows it and not what an earlier
 * visit left behind. */
void gauge_info_open(
    uint16_t face,
    const char * notice
);

/* Redraws only what changed. notice is the line under the page, such as
 * "SAVED", or "" for nothing; it must stay valid until the next call. */
void gauge_info_poll( const char * notice );

#endif /* GAUGE_INFO_H */
