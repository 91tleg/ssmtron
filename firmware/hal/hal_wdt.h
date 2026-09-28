/* Single-instance watchdog that resets the MCU if feeding stops. */
#ifndef HAL_WDT_H
#define HAL_WDT_H

#include <stdint.h>

#include "hal_status.h"

/* Arm early. Size timeout_ms for startup as well as the main loop.
 * Returns HAL_ERR_UNSUPPORTED if the build has no watchdog. */
hal_status_t hal_wdt_start( uint32_t timeout_ms );

/* Feed from the main loop and long startup retries. No-op if not armed. */
void hal_wdt_feed( void );

#endif /* HAL_WDT_H */
