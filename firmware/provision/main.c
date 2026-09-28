/* Factory app: store this unit's BLE passkey in NVM, then halt. */
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

    /* Report whether the passkey was stored. */
    hal_log_write(
        ( status == HAL_OK ) ? "Provisioned OK" : "Provisioning FAILED"
    );

    for ( ;; )
    {
        hal_delay_ms( 1000u );
    }
    return ( int )status;
}
