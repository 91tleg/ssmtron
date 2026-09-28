/* Zephyr Settings backend. Requires CONFIG_SETTINGS and persistent storage
 * such as CONFIG_SETTINGS_NVS. */
#include "hal_nvm.h"

#include <stdbool.h>
#include <string.h>

#if defined( CONFIG_SETTINGS )
#include <zephyr/settings/settings.h>
#endif

static bool valid_key( const char * key )
{
    bool ok = ( key != NULL ) && ( key[0] != '\0' );

    if ( ok )
    {
        ok = strnlen( key, HAL_NVM_KEY_MAX ) < HAL_NVM_KEY_MAX;
    }
    return ok;
}

hal_status_t hal_nvm_init( void )
{
    hal_status_t status = HAL_ERR_UNSUPPORTED;

#if defined( CONFIG_SETTINGS )
    status = ( settings_subsys_init() == 0 ) ? HAL_OK : HAL_ERR_IO;
#endif
    return status;
}

hal_status_t hal_nvm_save(
    const char * key,
    const void * data,
    size_t len
)
{
    hal_status_t status = HAL_ERR_INVALID;

    if ( valid_key( key ) && ( data != NULL ) && ( len > 0u ) )
    {
#if defined( CONFIG_SETTINGS )
        status =
            ( settings_save_one( key, data, len ) == 0 ) ? HAL_OK : HAL_ERR_IO;
#else
        status = HAL_ERR_UNSUPPORTED;
#endif
    }
    return status;
}

hal_status_t hal_nvm_load(
    const char * key,
    void * data,
    size_t max,
    size_t * len
)
{
    hal_status_t status = HAL_ERR_INVALID;

    if ( valid_key( key ) && ( data != NULL ) && ( max > 0u )
         && ( len != NULL ) )
    {
#if defined( CONFIG_SETTINGS )
        ssize_t n = settings_load_one( key, data, max );

        if ( n >= 0 )
        {
            *len   = ( size_t )n;
            status = HAL_OK;
        }
        else
        {
            /* Nothing has ever been saved under this key. */
            status = HAL_ERR_INVALID;
        }
#else
        status = HAL_ERR_UNSUPPORTED;
#endif
    }
    return status;
}
