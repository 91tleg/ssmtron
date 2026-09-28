/* Only ever built when CONFIG_APP_HIL_TEST is on (see CMakeLists.txt);
 * never part of a production image. */
#include "hil_test.h"

#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/uart.h>

#define HANG_CMD ( 'H' )

static const struct device * console_dev =
    DEVICE_DT_GET( DT_CHOSEN( zephyr_console ) );

void hil_test_poll( void )
{
    unsigned char c;

    if ( uart_poll_in( console_dev, &c ) == 0 )
    {
        if ( c == HANG_CMD )
        {
            /* The watchdog, not this code, must recover the unit. */
            for ( ;; )
            {
            }
        }
    }
}
