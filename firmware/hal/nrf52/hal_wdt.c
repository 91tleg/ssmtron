/* Single-channel watchdog resets the SoC on expiry. */
#include "hal_wdt.h"

#include <zephyr/device.h>
#include <zephyr/drivers/watchdog.h>

#define WDT_NODE DT_ALIAS( watchdog0 )

static const struct device * wdt_dev = DEVICE_DT_GET( WDT_NODE );
static int wdt_channel               = -1;

hal_status_t hal_wdt_start( uint32_t timeout_ms )
{
    hal_status_t status = HAL_ERR_UNSUPPORTED;

    if ( device_is_ready( wdt_dev ) )
    {
        struct wdt_timeout_cfg cfg = {
            .window   = { .min = 0u, .max = timeout_ms },
            .callback = NULL,
            .flags    = WDT_FLAG_RESET_SOC,
        };

        wdt_channel = wdt_install_timeout( wdt_dev, &cfg );
        if ( wdt_channel >= 0 )
        {
            /* Pause for breakpoints, but keep running during sleep. */
            status = ( wdt_setup( wdt_dev, WDT_OPT_PAUSE_HALTED_BY_DBG ) == 0 )
                         ? HAL_OK
                         : HAL_ERR_IO;
        }
        else
        {
            status = HAL_ERR_IO;
        }
    }
    return status;
}

void hal_wdt_feed( void )
{
    if ( wdt_channel >= 0 )
    {
        ( void )wdt_feed( wdt_dev, wdt_channel );
    }
}
