/* One-time factory step: writes this unit's BLE pairing passkey into NVM
 * under the same key firmware/app/main.c reads at boot, then halts. Not
 * part of the gauge firmware and never updated over the air -- see
 * CMakeLists.txt for how to build it and its partition-manager caveat. */
#include "hal_ble.h" /* HAL_BLE_PASSKEY_MAX only */
#include "hal_log.h"
#include "hal_nvm.h"
#include "hal_time.h"

#define BLE_PASSKEY_KEY "ble_passkey"

int main( void )
{
    uint32_t passkey    = ( uint32_t )PROVISION_PASSKEY;
    hal_status_t status = HAL_ERR_INVALID;

    if ( passkey <= HAL_BLE_PASSKEY_MAX )
    {
        status = hal_nvm_init();
        if ( status == HAL_OK )
        {
            status =
                hal_nvm_save( BLE_PASSKEY_KEY, &passkey, sizeof( passkey ) );
        }
    }

    /* The passkey itself is already known to whoever ran this build (it
     * was their -DPROVISION_PASSKEY); all the operator needs back is
     * whether the write actually took. */
    hal_log_write(
        ( status == HAL_OK ) ? "Provisioned OK" : "Provisioning FAILED"
    );

    for ( ;; )
    {
        hal_delay_ms( 1000u );
    }
    return ( int )status;
}
