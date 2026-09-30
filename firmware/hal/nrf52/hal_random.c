/* Backed by Zephyr's cryptographically secure generator, which draws on the
 * hardware entropy source. Unlike sys_rand32_get(), it reports failure
 * instead of quietly falling back to something predictable. */
#include "hal/hal_random.h"

#include <zephyr/random/random.h>

hal_status_t hal_random_u32( uint32_t * out )
{
    uint32_t value;
    hal_status_t status = HAL_ERR_IO;

    if ( ( out != NULL ) && ( sys_csrand_get( &value, sizeof( value ) ) == 0 ) )
    {
        *out   = value;
        status = HAL_OK;
    }
    return status;
}
