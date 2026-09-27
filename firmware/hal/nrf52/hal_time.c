#include "hal_time.h"

#include <zephyr/kernel.h>

uint32_t hal_millis( void )
{
    return k_uptime_get_32();
}

void hal_delay_ms( uint32_t ms )
{
    /* k_msleep returns the time left if woken early; hal_delay_ms has no
     * caller that acts on that, so it is explicitly discarded. */
    ( void )k_msleep( ( int32_t )ms );
}
