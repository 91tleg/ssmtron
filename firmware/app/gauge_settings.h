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

/* The ECU profile the saved parameter belongs to: the low 16 bits of its
 * ROM ID. A saved parameter index only means something for that profile. */
bool gauge_settings_load_profile( size_t * out );

/* Each save writes only a value that differs from what is already stored,
 * and returns false if a write was needed and failed; it is tried again on
 * the next save. */

/* The theme. */
bool gauge_settings_save_theme( size_t theme_idx );

/* The page and the profile it belongs to, together: a page index only means
 * something for its profile. */
bool gauge_settings_save_page(
    size_t param_idx,
    size_t profile_key
);

/* The profile alone, so the next start knows which pages to show. The saved
 * page, if any, is left as it is. */
bool gauge_settings_save_profile( size_t profile_key );

#endif /* GAUGE_SETTINGS_H */
