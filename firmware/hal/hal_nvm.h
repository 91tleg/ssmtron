/* A small persistent key/value store in on-chip flash, for the handful of
 * settings the app wants to survive a power cycle (e.g. which parameter is
 * selected). Not a filesystem: no directories, no large blobs, and no
 * guarantee beyond what the backend gives about write-cycle limits, so the
 * app should not write on every button press. HAL_ERR_UNSUPPORTED if no
 * storage is configured for this build. */
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

/* Reads the value stored under key into data (up to max bytes) and reports
 * its real length in *len. HAL_ERR_INVALID for a bad key/pointer, or if
 * nothing has ever been saved under it. If the stored value is larger than
 * max, *len still reports the real size but data holds only the first max
 * bytes, so the caller can tell it was truncated. */
hal_status_t hal_nvm_load(
    const char * key,
    void * data,
    size_t max,
    size_t * len
);

#endif /* HAL_NVM_H */
