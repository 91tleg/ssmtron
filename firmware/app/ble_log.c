#include "ble_log.h"

#include "ecu.h"
#include "fmt.h"
#include "hal_ble.h"

#define LINE_MAX ( 64u )

static uint32_t last_ms;
static bool sent_any;

/* The first parameter with a fresh value: the ECU only streams one at a
 * time, so this is the selected one. */
static bool current_value(
    const ecu_param_info_t ** info,
    int32_t * value
)
{
    bool found = false;

    for ( size_t i = 0; !found && ( i < ecu_param_count() ); i++ )
    {
        if ( ecu_get_value( i, value ) )
        {
            *info = ecu_param_info( i );
            found = true;
        }
    }
    return found;
}

void ble_log_poll( uint32_t now_ms )
{
    const ecu_param_info_t * info = NULL;
    int32_t value                 = 0;

    if ( hal_ble_ready()
         && ( !sent_any || ( ( now_ms - last_ms ) >= BLE_LOG_PERIOD_MS ) )
         && current_value( &info, &value ) )
    {
        char line[LINE_MAX];
        size_t n = fmt_log_line( line, sizeof( line ), now_ms, info, value );

        if ( n > 0u )
        {
            /* If the queue is full the line is dropped: the next one is
             * only a period away. */
            hal_ble_write( ( const uint8_t * )line, n );
            last_ms  = now_ms;
            sent_any = true;
        }
    }
}
