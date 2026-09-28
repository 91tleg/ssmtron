#include "bsp.h"
#include "ecu_io.h"
#include "fw_info.h"
#include "gauge.h"
#include "hal_ble.h"
#include "hal_dfu.h"
#include "hal_log.h"
#include "hal_nvm.h"
#include "hal_time.h"
#include "hal_wdt.h"
#include "hil_test.h"

#define BLE_NAME "SSM1 Gauge"

/* Bench fallback. Production passkeys are provisioned in NVM. */
#define BLE_PASSKEY_DEFAULT ( 123456u )
#define BLE_PASSKEY_KEY     "ble_passkey"

/* Use the bench passkey when no valid stored passkey is available. */
static uint32_t ble_passkey( void )
{
    uint32_t passkey = BLE_PASSKEY_DEFAULT;
    size_t len;

    if ( ( hal_nvm_init() == HAL_OK )
         && ( hal_nvm_load( BLE_PASSKEY_KEY, &passkey, sizeof( passkey ), &len )
              == HAL_OK )
         && ( len != sizeof( passkey ) ) )
    {
        passkey = BLE_PASSKEY_DEFAULT;
    }
    return passkey;
}

/* A new firmware image is confirmed once it has run this long; if it
 * crashes sooner the bootloader goes back to the old one. */
#define CONFIRM_AFTER_MS ( 10000u )

/* Allow for ECU, Bluetooth, and display startup before the first feed. */
#define WDT_TIMEOUT_MS ( 4000u )

/* Yield without delaying a 1953 baud byte. */
#define LOOP_DELAY_MS ( 1u )

int main( void )
{
    const hal_ble_cfg_t ble = { BLE_NAME, ble_passkey() };

    /* No backlight line wired yet (see bsp.h); backlight is left at its
     * default id and unused because has_backlight is false. */
    const display_pins_t display_pins = {
        .spi           = BSP_SPI_DISPLAY,
        .cs            = BSP_GPIO_DISPLAY_CS,
        .dc            = BSP_GPIO_DISPLAY_DC,
        .reset         = BSP_GPIO_DISPLAY_RESET,
        .backlight     = ( hal_gpio_id_t )0u,
        .has_backlight = false
    };
    hal_status_t status;
    hal_status_t ecu_status;
    bool confirmed = false;

    /* Arm before any HAL call that could hang. */
    if ( hal_wdt_start( WDT_TIMEOUT_MS ) != HAL_OK )
    {
        hal_log_write( "wdt not armed" );
    }

    /* Print the version before initializing peripherals. */
    hal_log_write( fw_info.version );

    /* ECU failure must not block firmware updates over BLE. */
    ecu_status = ecu_io_init( BSP_UART_ECU );

    /* BLE provides a connectable link for firmware updates. */
    ( void )hal_ble_open( &ble );

    /* The ECU link can run without a display. */
    ( void )gauge_init( &display_pins, BSP_GPIO_NEXT, BSP_GPIO_ACTION );

    hal_wdt_feed();

    for ( ;; )
    {
        uint32_t now = hal_millis();

        if ( ecu_status == HAL_OK )
        {
            ecu_io_poll( now );
        }

        gauge_poll( now );
        hil_test_poll();

        if ( !confirmed && ( now >= CONFIRM_AFTER_MS ) )
        {
            status = hal_dfu_confirm();

            /* Retry failures; builds without a bootloader need no confirmation.
             */
            confirmed =
                ( status == HAL_OK ) || ( status == HAL_ERR_UNSUPPORTED );
        }

        hal_wdt_feed();
        hal_delay_ms( LOOP_DELAY_MS );
    }

    return ( int )status;
}
