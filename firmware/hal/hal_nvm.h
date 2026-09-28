/* Persistent key/value storage in flash. Avoid frequent writes.
 * Returns HAL_ERR_UNSUPPORTED when storage is unavailable. */
#ifndef HAL_NVM_H
#define HAL_NVM_H

#include <stddef.h>

#include "hal_status.h"

#define HAL_NVM_KEY_MAX 32u /* characters, including the terminator */

/* Starts the store; safe to call more than once. Must be called before
 * hal_nvm_save/hal_nvm_load. */
hal_status_t hal_nvm_init( void );

/* Writes data under key, replacing any previous value. HAL_ERR_INVALID for
 * a bad key or a NULL/zero-length value. */
hal_status_t hal_nvm_save(
    const char * key,
    const void * data,
    size_t len
);

/* Reads up to max bytes. *len reports the stored length even if truncated.
 * Returns HAL_ERR_INVALID for invalid input or a missing key. */
hal_status_t hal_nvm_load(
    const char * key,
    void * data,
    size_t max,
    size_t * len
);

#endif /* HAL_NVM_H */
