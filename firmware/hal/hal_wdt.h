/* Hardware watchdog: resets the MCU if the app stops feeding it. There is
 * no user-facing recovery action for a hang in the field (no debugger, no
 * console), so this is the only thing that gets a wedged unit running
 * again on its own. Single instance. */
#ifndef HAL_WDT_H
#define HAL_WDT_H

#include <stdint.h>

#include "hal_status.h"

/* Arms the watchdog: the MCU resets if hal_wdt_feed is not called again
 * within timeout_ms. Call once, early, before anything that could hang
 * -- size timeout_ms to the worst case startup path (display retries,
 * BT bring-up, ...), not steady-state loop timing, or a slow-but-healthy
 * boot resets itself. HAL_ERR_UNSUPPORTED if the build has no watchdog.
 */
hal_status_t hal_wdt_start( uint32_t timeout_ms );

/* Resets the countdown. Call from the main loop, and from inside any
 * bounded retry/delay sequence during startup that could otherwise look
 * like a hang. No-op if hal_wdt_start was never called or failed. */
void hal_wdt_feed( void );

#endif /* HAL_WDT_H */
