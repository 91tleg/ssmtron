#include "gauge.h"

#include <string.h>

#include "ecu.h"
#include "fmt.h"
#include "hal_log.h"
#include "hal_nvm.h"
#include "ui.h"

#define FG ( 0xFFFFu ) /* white */
#define BG ( 0x0000u ) /* black */

/* Mechanical switch settle time: a press/release edge must be stable
 * this long before it counts. */
#define DEBOUNCE_MS ( 30u )
/* ACTION requires a sustained press; taps do nothing. */
#define ACTION_HOLD_MS ( 2000u )

#define PARAM_IDX_KEY "param_idx"

/* UI state includes HOLDING before any ECU clear request starts. */
typedef enum
{
    CLEAR_UI_IDLE,
    CLEAR_UI_HOLDING,
    CLEAR_UI_ACTIVE,
    CLEAR_UI_RESULT
} clear_ui_t;

static hal_gpio_id_t next_btn_id;
static hal_gpio_id_t action_btn_id;

static bool next_raw_prev;
static uint32_t next_stable_since;
static bool next_latched; /* true while held, so one press = one advance */

static bool action_raw_prev;
static uint32_t action_stable_since;
static uint32_t action_hold_start;
static bool action_released_since_result;
static clear_ui_t clear_ui = CLEAR_UI_IDLE;

/* Mirror the selected parameter; gauge.c alone calls ecu_select_param. */
static size_t selected_idx;
static bool selection_restored;

/* Find the fresh value from the ECU's single active stream. */
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

/* Copy and terminate up to UI_TEXT_MAX bytes. */
static void copy_capped(
    char * buf,
    const char * s
)
{
    size_t n = strlen( s );

    if ( n > UI_TEXT_MAX )
    {
        n = UI_TEXT_MAX;
    }
    memcpy( buf, s, n );
    buf[n] = '\0';
}

static void save_selected_idx( size_t idx )
{
    uint32_t v = ( uint32_t )idx;

    ( void )hal_nvm_save( PARAM_IDX_KEY, &v, sizeof( v ) );
}

/* Restore selection once ECU identification provides the parameter count. */
static void restore_selected_idx( void )
{
    uint32_t v;
    size_t len;

    if ( ( hal_nvm_load( PARAM_IDX_KEY, &v, sizeof( v ), &len ) == HAL_OK )
         && ( len == sizeof( v ) ) && ( ( size_t )v < ecu_param_count() ) )
    {
        selected_idx = ( size_t )v;
    }
    ( void )ecu_select_param( selected_idx );
}

static void poll_next( uint32_t now )
{
    bool raw = hal_gpio_read( next_btn_id );

    if ( raw != next_raw_prev )
    {
        next_stable_since = now;
        next_raw_prev     = raw;
    }
    if ( ( now - next_stable_since ) >= DEBOUNCE_MS )
    {
        if ( raw && !next_latched && ( clear_ui == CLEAR_UI_IDLE )
             && ( ecu_param_count() > 0u ) )
        {
            selected_idx = ( selected_idx + 1u ) % ecu_param_count();
            ( void )ecu_select_param( selected_idx );
            save_selected_idx( selected_idx );
            next_latched = true;
        }
        else if ( !raw )
        {
            next_latched = false;
        }
    }
}

static void poll_action( uint32_t now )
{
    bool raw = hal_gpio_read( action_btn_id );

    if ( raw != action_raw_prev )
    {
        action_stable_since = now;
        action_raw_prev     = raw;
    }

    switch ( clear_ui )
    {
        case CLEAR_UI_IDLE:
            if ( raw && ( ( now - action_stable_since ) >= DEBOUNCE_MS ) )
            {
                action_hold_start = action_stable_since;
                clear_ui          = CLEAR_UI_HOLDING;
            }
            break;

        case CLEAR_UI_HOLDING:
            if ( !raw )
            {
                clear_ui = CLEAR_UI_IDLE; /* let go before the bar */
            }
            else if ( ( now - action_hold_start ) >= ACTION_HOLD_MS )
            {
                ( void )ecu_clear_start();
                clear_ui = CLEAR_UI_ACTIVE;
                hal_log_write( "ecu clear: started" );
            }
            break;

        case CLEAR_UI_ACTIVE:
        {
            ecu_clear_t result = ecu_clear_state();

            if ( result != ECU_CLEAR_BUSY )
            {
                clear_ui                     = CLEAR_UI_RESULT;
                action_released_since_result = false;
                hal_log_write(
                    ( result == ECU_CLEAR_DONE ) ? "ecu clear: done"
                                                 : "ecu clear: failed"
                );
            }
            break;
        }

        case CLEAR_UI_RESULT:
        default:
            /* A fresh tap dismisses back to the normal screen; it must
             * not re-trigger a clear, so a press only counts once the
             * button has been seen released since the result appeared
             * (it may still be held from the original 2 s press). */
            if ( !raw )
            {
                action_released_since_result = true;
            }
            else if ( action_released_since_result
                      && ( ( now - action_stable_since ) >= DEBOUNCE_MS ) )
            {
                clear_ui = CLEAR_UI_IDLE;
            }
            break;
    }
}

static void name_text( char * buf )
{
    switch ( clear_ui )
    {
        case CLEAR_UI_HOLDING:
            copy_capped( buf, "HOLD TO CLEAR" );
            break;

        case CLEAR_UI_ACTIVE:
            copy_capped( buf, "CLEARING..." );
            break;

        case CLEAR_UI_RESULT:
            copy_capped(
                buf,
                ( ecu_clear_state() == ECU_CLEAR_DONE ) ? "CLEARED - IGNITION"
                                                        : "CLEAR FAILED"
            );
            break;

        case CLEAR_UI_IDLE:
        default:
        {
            const ecu_param_info_t * info = NULL;
            int32_t value                 = 0;

            copy_capped(
                buf, current_value( &info, &value ) ? info->name : "NO ECU"
            );
            break;
        }
    }
}

static void value_text( char * buf )
{
    const ecu_param_info_t * info = NULL;
    int32_t value                 = 0;

    if ( current_value( &info, &value ) )
    {
        ( void )fmt_value( buf, UI_TEXT_MAX + 1u, info, value );
    }
    else
    {
        copy_capped( buf, "--" );
    }
}

static void unit_text( char * buf )
{
    const ecu_param_info_t * info = NULL;
    int32_t value                 = 0;

    copy_capped(
        buf, current_value( &info, &value ) ? ecu_unit( info, value ) : ""
    );
}

static void link_text( char * buf )
{
    copy_capped( buf, ( ecu_link() == ECU_LINK_UP ) ? "LINKED" : "NO LINK" );
}

/* The screen: rects are kept a safe margin inside the round panel's 120
 * px radius (see the corner-distance check in the commit that added this
 * file), not tiled edge-to-edge like a square display would allow.
 * Moving, resizing or adding a field is a change to this table alone. */
static ui_widget_t screen[] = {
    { { 40, 55, 160, 7 }, 1, FG, BG, name_text, { 0 } },
    { { 32, 95, 176, 28 }, 4, FG, BG, value_text, { 0 } },
    { { 60, 135, 120, 14 }, 2, FG, BG, unit_text, { 0 } },
    { { 70, 175, 100, 7 }, 1, FG, BG, link_text, { 0 } },
};
#define SCREEN_COUNT ( sizeof( screen ) / sizeof( screen[0] ) )

hal_status_t gauge_init(
    const display_pins_t * pins,
    hal_gpio_id_t next_btn,
    hal_gpio_id_t action_btn
)
{
    hal_status_t status = display_init( pins );

    next_btn_id   = next_btn;
    action_btn_id = action_btn;
    /* Not fatal if either fails to open: the buttons just would not
     * respond, same tolerance main.c already gives the display/BLE. */
    ( void )hal_gpio_open( next_btn_id, HAL_GPIO_INPUT );
    ( void )hal_gpio_open( action_btn_id, HAL_GPIO_INPUT );

    if ( status == HAL_OK )
    {
        ui_init( screen, SCREEN_COUNT, BG );
    }
    return status;
}

void gauge_poll( uint32_t now_ms )
{
    if ( !selection_restored && ( ecu_param_count() > 0u ) )
    {
        restore_selected_idx();
        selection_restored = true;
    }
    poll_next( now_ms );
    poll_action( now_ms );
    ui_poll( screen, SCREEN_COUNT );
}
