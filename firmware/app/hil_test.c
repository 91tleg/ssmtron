/* Only ever built when CONFIG_APP_HIL_TEST is on (see CMakeLists.txt);
 * never part of a production image. */
#include "app/hil_test.h"

#if defined( CONFIG_APP_HIL_TEST )
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/uart.h>

void hil_test_poll( void )
{
    /* Zephyr defines both devicetree macros in the headers above. The
     * standalone CI scan has no generated Zephyr headers to expand them. */
    const struct device * const console_dev =
        /* cppcheck-suppress misra-c2012-17.3 */
        DEVICE_DT_GET( DT_CHOSEN( zephyr_console ) );
    unsigned char c;

    /* Declared in zephyr/drivers/uart.h, unavailable to the standalone scan. */
    /* cppcheck-suppress misra-c2012-17.3 */
    if ( uart_poll_in( console_dev, &c ) == 0 )
    {
        if ( c == ( unsigned char )'H' )
        {
            /* The watchdog, not this code, must recover the unit. */
            for ( ;; )
            {
            }
        }
    }
}
#endif /* CONFIG_APP_HIL_TEST */
