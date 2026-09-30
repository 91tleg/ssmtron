#include "hal_dfu.h"

#include <zephyr/kernel.h>
#include <zephyr/sys/reboot.h>

#if defined( CONFIG_BOOTLOADER_MCUBOOT )
#include <zephyr/dfu/mcuboot.h>
#endif

hal_status_t hal_dfu_confirm( void )
{
    hal_status_t status = HAL_ERR_UNSUPPORTED;

#if defined( CONFIG_BOOTLOADER_MCUBOOT )
    status = ( boot_write_img_confirmed() == 0 ) ? HAL_OK : HAL_ERR_IO;
#endif
    return status;
}

void hal_dfu_reboot( void )
{
    sys_reboot( SYS_REBOOT_COLD );
}
