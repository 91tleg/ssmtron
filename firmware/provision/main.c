/* Factory app: store this unit's BLE passkey in NVM, then halt. */
#include "hal_log.h"
#include "hal_nvm.h"
#include "hal_time.h"

#define BLE_PASSKEY_KEY "ble_passkey"

/* A bad provisioning value must fail the build, not ship to a device. */
#if ( PROVISION_PASSKEY < 0 ) || ( PROVISION_PASSKEY > 999999 )
#error "PROVISION_PASSKEY must be between 0 and 999999"
#endif

/* cppcheck-suppress misra-c2012-8.6 ; separate provisioning firmware */
int main( void )
{
    uint32_t passkey    = ( uint32_t )PROVISION_PASSKEY;
    hal_status_t status = hal_nvm_init();

    if ( status == HAL_OK )
    {
        status = hal_nvm_save( BLE_PASSKEY_KEY, &passkey, sizeof( passkey ) );
    }

    /* Report whether the passkey was stored. */
    hal_log_write(
        ( status == HAL_OK ) ? "Provisioned OK" : "Provisioning FAILED"
    );

    for ( ;; )
    {
        hal_delay_ms( 1000u );
    }
}
