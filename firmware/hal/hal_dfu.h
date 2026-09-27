/* Firmware update support. The image itself arrives over Bluetooth through
 * the bootloader's own protocol; the application's part is to confirm a new
 * image once it has proven itself, otherwise the bootloader reverts to the
 * old one at the next reset. */
#ifndef HAL_DFU_H
#define HAL_DFU_H

#include "hal_status.h"

/* Marks the running image as good. HAL_ERR_UNSUPPORTED when the build has
 * no bootloader (nothing to confirm). */
hal_status_t hal_dfu_confirm( void );

#endif /* HAL_DFU_H */
