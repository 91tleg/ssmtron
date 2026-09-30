/* What the gauge remembers across restarts: the shown parameter and the
 * theme. It also remembers what it last stored, so a save that changes
 * nothing costs no flash write. */
#ifndef GAUGE_SETTINGS_H
#define GAUGE_SETTINGS_H

#include <stdbool.h>
#include <stddef.h>

/* Each load returns true and sets *out if a valid value was stored, meaning
 * it is below count. Otherwise *out is left alone. */
bool gauge_settings_load_param(
    size_t count,
    size_t * out
);
bool gauge_settings_load_theme(
    size_t count,
    size_t * out
);

/* Stores both values, writing only the one that differs from what is
 * already stored. False if any write was needed and failed; a failed value
 * is tried again on the next save. */
bool gauge_settings_save(
    size_t param_idx,
    size_t theme_idx
);

#endif /* GAUGE_SETTINGS_H */
