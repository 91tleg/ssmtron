/* Flags page: one byte of ECU flags (switches or trouble codes) as a list
 * of every flag name with its state. gauge.c decides when the page is
 * shown; this file owns what it looks like. */
#ifndef GAUGE_FLAGS_H
#define GAUGE_FLAGS_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "lib/ecu/ecu.h"

/* What the line under the flags says. */
typedef enum
{
    FLAGS_STATUS_NONE,
    FLAGS_STATUS_HOLD_TO_CLEAR,
    FLAGS_STATUS_CLEARING,
    FLAGS_STATUS_CLEARED,
    FLAGS_STATUS_CLEAR_FAILED,
    FLAGS_STATUS_SAVED,
    FLAGS_STATUS_SAVE_FAILED
} flags_status_t;

/* True if this parameter is drawn as a flags page. */
bool gauge_flags_handles( const ecu_param_info_t * info );

/* Sets the page colors; takes effect at the next gauge_flags_open. */
void gauge_flags_set_colors(
    uint16_t face,
    uint16_t ink
);

/* Clears the panel to face and draws the whole page. The other arguments are
 * those of gauge_flags_poll, so the first draw already shows the selected
 * parameter and not the one an earlier visit showed. */
void gauge_flags_open(
    uint16_t face,
    size_t idx,
    flags_status_t status,
    uint32_t now_ms
);

/* Redraws only what changed. idx is the selected parameter, status the
 * line under the flags, now_ms the poll time (for the clearing dots). */
void gauge_flags_poll(
    size_t idx,
    flags_status_t status,
    uint32_t now_ms
);

#endif /* GAUGE_FLAGS_H */
