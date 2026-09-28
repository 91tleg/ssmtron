/* Confirm a new Bluetooth-delivered image so the bootloader keeps it. */
#ifndef HAL_DFU_H
#define HAL_DFU_H

#include "hal_status.h"

/* Marks the running image as good. HAL_ERR_UNSUPPORTED when the build has
 * no bootloader (nothing to confirm). */
hal_status_t hal_dfu_confirm( void );

#endif /* HAL_DFU_H */
