#ifndef HAL_TIME_H
#define HAL_TIME_H

#include <stdint.h>

/* Monotonic milliseconds since boot; wraps at 2^32. */
uint32_t hal_millis( void );

/* Frees the CPU for at least this long, letting other threads (Bluetooth,
 * the system workqueue) run instead of the caller spinning. Not for timing
 * protocol delays: those belong to the protocol library and are driven by
 * hal_millis, not by blocking here. */
void hal_delay_ms( uint32_t ms );

#endif /* HAL_TIME_H */
