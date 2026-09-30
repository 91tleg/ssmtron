#include "app/gauge_settings.h"

#include <stdint.h>

#include "hal/hal_nvm.h"

#define PARAM_IDX_KEY "param_idx"
#define THEME_KEY     "theme"

/* No stored value is known yet. */
#define STORED_NONE ( SIZE_MAX )

/* Last values known to be in NVM. */
static size_t stored_param = STORED_NONE;
static size_t stored_theme = STORED_NONE;

/* Read one stored index that must be below count. */
static bool load_index(
    const char * key,
    size_t count,
    size_t * out
)
{
    uint32_t v;
    size_t len;
    bool ok = ( hal_nvm_load( key, &v, sizeof( v ), &len ) == HAL_OK )
              && ( len == sizeof( v ) ) && ( ( size_t )v < count );

    if ( ok )
    {
        *out = ( size_t )v;
    }
    return ok;
}

/* Write one index unless it is already what is stored. */
static bool save_index(
    const char * key,
    size_t value,
    size_t * stored
)
{
    bool ok = ( value == *stored );

    if ( !ok )
    {
        uint32_t v = ( uint32_t )value;

        ok = ( hal_nvm_save( key, &v, sizeof( v ) ) == HAL_OK );
        if ( ok )
        {
            *stored = value;
        }
    }
    return ok;
}

bool gauge_settings_load_param(
    size_t count,
    size_t * out
)
{
    bool ok = load_index( PARAM_IDX_KEY, count, out );

    if ( ok )
    {
        stored_param = *out;
    }
    return ok;
}

bool gauge_settings_load_theme(
    size_t count,
    size_t * out
)
{
    bool ok = load_index( THEME_KEY, count, out );

    if ( ok )
    {
        stored_theme = *out;
    }
    return ok;
}

bool gauge_settings_save(
    size_t param_idx,
    size_t theme_idx
)
{
    /* Both are attempted, so one failure does not stop the other. */
    bool param_ok = save_index( PARAM_IDX_KEY, param_idx, &stored_param );
    bool theme_ok = save_index( THEME_KEY, theme_idx, &stored_theme );

    return param_ok && theme_ok;
}
