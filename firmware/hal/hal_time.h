#ifndef HAL_TIME_H
#define HAL_TIME_H

#include <stdint.h>

/* Monotonic milliseconds since boot; wraps at 2^32. */
uint32_t hal_millis( void );

/* Sleep for at least this long, letting other threads run. Protocol
 * timing uses hal_millis rather than blocking delays. */
void hal_delay_ms( uint32_t ms );

#endif /* HAL_TIME_H */
