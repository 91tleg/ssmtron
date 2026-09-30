#include "app/ecu_io.h"
#include "app/fw_info.h"
#include "app/gauge.h"
#include "app/hil_test.h"
#include "bsp.h"
#include "hal/hal_dfu.h"
#include "hal/hal_log.h"
#include "hal/hal_nvm.h"
#include "hal/hal_time.h"
#include "hal/hal_wdt.h"

/* A new firmware image is confirmed once it has run this long; if it
 * crashes sooner the bootloader goes back to the old one. */
#define CONFIRM_AFTER_MS ( 10000u )

/* Allow for ECU and display startup before the first feed. */
#define WDT_TIMEOUT_MS ( 4000u )

/* Yield without delaying a 1953 baud byte. */
#define LOOP_DELAY_MS ( 1u )

/* cppcheck-suppress misra-c2012-8.6 ; separate application firmware */
int main( void )
{
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

    hal_status_t ecu_status;
    bool confirmed = false;

    /* Arm before any HAL call that could hang. */
    if ( hal_wdt_start( WDT_TIMEOUT_MS ) != HAL_OK )
    {
        hal_log_write( "wdt not armed" );
    }

    /* Print the version before initializing peripherals. */
    hal_log_write( fw_info.version );

    /* The saved page and theme are read while the gauge starts. */
    ( void )hal_nvm_init();

    /* Open the ECU link first: it is quick. An ECU failure must not block
     * the display or firmware updates. */
    ecu_status = ecu_io_init( BSP_UART_ECU );

    /* Draw the first frame before anything slow, so the gauge is showing
     * something almost as soon as it powers on. It runs without an ECU. */
    ( void )gauge_init( &display_pins, BSP_GPIO_NEXT, BSP_GPIO_ACTION );

    /* Bluetooth stays off until the user starts update mode from the info
     * page (see gauge_update.c), so it does not slow the start or sit open
     * while driving. */

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
            hal_status_t status = hal_dfu_confirm();

            /* Retry failures; builds without a bootloader need no confirmation.
             */
            confirmed =
                ( status == HAL_OK ) || ( status == HAL_ERR_UNSUPPORTED );
        }

        hal_wdt_feed();
        hal_delay_ms( LOOP_DELAY_MS );
    }
}
