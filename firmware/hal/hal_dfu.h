/* Firmware update support: confirming a new Bluetooth-delivered image so the
 * bootloader keeps it, and restarting the device. */
#ifndef HAL_DFU_H
#define HAL_DFU_H

#include "hal_status.h"

/* Marks the running image as good. HAL_ERR_UNSUPPORTED when the build has
 * no bootloader (nothing to confirm). */
hal_status_t hal_dfu_confirm( void );

/* Restarts the device. The bootloader runs first, so a newly uploaded image
 * starts; with none waiting it is an ordinary restart. It does not return. */
void hal_dfu_reboot( void );

#endif /* HAL_DFU_H */
