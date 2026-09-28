/* Backed by Zephyr's printk: no format string of our own to keep MISRA's
 * <stdarg.h> restriction (Rule 17.1) out of the app-facing API; see
 * hal_log.h. */
#include "hal_log.h"

#include <stddef.h>

#include <zephyr/sys/printk.h>

void hal_log_write( const char * text )
{
    if ( text != NULL )
    {
        printk( "%s\n", text );
    }
}
