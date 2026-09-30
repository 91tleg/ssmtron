/* A random value from a cryptographically secure source. */
#ifndef HAL_RANDOM_H
#define HAL_RANDOM_H

#include <stdint.h>

#include "hal_status.h"

/* Stores an unpredictable 32-bit value in *out and returns HAL_OK. Used for
 * one-off secrets such as a pairing passkey. HAL_ERR_IO if the source fails,
 * and then *out is untouched: the caller must not fall back to a fixed or
 * guessable value. */
hal_status_t hal_random_u32( uint32_t * out );

#endif /* HAL_RANDOM_H */
