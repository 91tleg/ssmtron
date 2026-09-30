#include "app/ble_log.h"

#include "hal/hal_ble.h"
#include "lib/ecu/ecu.h"
#include "lib/fmt/fmt.h"

#define LINE_MAX ( 64u )

/* cppcheck-suppress misra-c2012-8.7 ; public API (ble_log.h), called
 * from main.c when BLE app data is enabled and from test_ble_log.c,
 * which this scoped MISRA scan does not cover. */
void ble_log_poll( uint32_t now_ms )
{
    const ecu_param_info_t * info = NULL;
    int32_t value                 = 0;
    static uint32_t last_ms;
    static bool sent_any;

    if ( hal_ble_ready()
         && ( !sent_any || ( ( now_ms - last_ms ) >= BLE_LOG_PERIOD_MS ) )
         && ecu_fresh_value( &info, &value ) )
    {
        char line[LINE_MAX];
        size_t n = fmt_log_line( line, sizeof( line ), now_ms, info, value );

        if ( n > 0u )
        {
            /* If the queue is full the line is dropped: the next one is
             * only a period away. */
            ( void )hal_ble_write( ( const uint8_t * )line, n );
            last_ms  = now_ms;
            sent_any = true;
        }
    }
}
