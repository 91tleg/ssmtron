#include "gauge.h"

#include <stdint.h>
#include <string.h>

#include "dial.h"
#include "ecu.h"
#include "fmt.h"
#include "hal_log.h"
#include "hal_nvm.h"
#include "text.h"

#define FG ( 0xFFFFu ) /* white */
#define BG ( 0x0000u ) /* black */

/* Mechanical switch settle time: a press/release edge must be stable
 * this long before it counts. */
#define DEBOUNCE_MS ( 30u )

/* ACTION requires a sustained press; taps do nothing. */
#define ACTION_HOLD_MS ( 2000u )

/* NEXT held this long saves the shown parameter as the power-on default.
 * Short presses only browse, so flash is written only on request. */
#define NEXT_HOLD_MS ( 1500u )

/* How long the SAVED or SAVE FAILED notice replaces the gauge face. */
#define SAVE_NOTICE_MS ( 1000u )

#define PARAM_IDX_KEY "param_idx"

/* No stored selection is known yet. */
#define SAVED_IDX_NONE ( SIZE_MAX )

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

static clear_ui_t clear_ui = CLEAR_UI_IDLE;

/* NEXT tracks one press at a time; DONE waits out the rest of a press
 * that already saved or was refused, so it cannot also cycle on release. */
typedef enum
{
    NEXT_IDLE,
    NEXT_PRESSED,
    NEXT_DONE
} next_state_t;

static bool save_notice_active;
static bool save_notice_ok;
static uint32_t save_notice_start;

/* Last value known to be in NVM, so a repeat save costs no flash write. */
static size_t saved_idx = SAVED_IDX_NONE;

/* Mirror the selected parameter; gauge.c alone calls ecu_select_param. */
static size_t selected_idx;

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

/* Copy and terminate up to TEXT_MAX bytes. */
static void copy_capped(
    char * buf,
    const char * s
)
{
    size_t n = strlen( s );

    if ( n > TEXT_MAX )
    {
        n = TEXT_MAX;
    }
    ( void )memcpy( buf, s, n );
    buf[n] = '\0';
}

static bool save_selected_idx( size_t idx )
{
    bool ok = ( idx == saved_idx );

    if ( !ok )
    {
        uint32_t v = ( uint32_t )idx;

        ok = ( hal_nvm_save( PARAM_IDX_KEY, &v, sizeof( v ) ) == HAL_OK );
        if ( ok )
        {
            saved_idx = idx;
        }
    }
    return ok;
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
        saved_idx    = selected_idx;
    }
    ( void )ecu_select_param( selected_idx );
}

static void poll_level(
    hal_gpio_id_t id,
    bool * level,
    uint32_t * stable_since
)
{
    uint32_t edge_at;

    if ( hal_gpio_take_edge( id, level, &edge_at ) )
    {
        *stable_since = edge_at;
    }
}

static void poll_next( uint32_t now )
{
    static next_state_t next_state = NEXT_IDLE;
    static bool next_level;
    static uint32_t next_stable_since;
    static uint32_t next_hold_start;
    bool settled;

    poll_level( next_btn_id, &next_level, &next_stable_since );
    settled = ( now - next_stable_since ) >= DEBOUNCE_MS;

    switch ( next_state )
    {
        case NEXT_IDLE:
            if ( next_level && settled )
            {
                if ( ( clear_ui == CLEAR_UI_IDLE )
                     && ( ecu_param_count() > 0u ) )
                {
                    next_hold_start = next_stable_since;
                    next_state      = NEXT_PRESSED;
                }
                else
                {
                    /* Not for us: a press that began during a clear. */
                    next_state = NEXT_DONE;
                }
            }
            break;

        case NEXT_PRESSED:
            if ( clear_ui != CLEAR_UI_IDLE )
            {
                next_state = NEXT_DONE;
            }
            else if ( !next_level && settled )
            {
                /* A short press browses in RAM only. */
                selected_idx = ( selected_idx + 1u ) % ecu_param_count();
                ( void )ecu_select_param( selected_idx );
                next_state = NEXT_IDLE;
            }
            else if ( next_level
                      && ( ( now - next_hold_start ) >= NEXT_HOLD_MS ) )
            {
                save_notice_ok     = save_selected_idx( selected_idx );
                save_notice_active = true;
                save_notice_start  = now;
                next_state         = NEXT_DONE;
            }
            else
            {
                /* Continue waiting for release or the hold threshold. */
            }
            break;

        case NEXT_DONE:
        default:
            if ( !next_level && settled )
            {
                next_state = NEXT_IDLE;
            }
            break;
    }
}

static void poll_action( uint32_t now )
{
    static bool action_level;
    static uint32_t action_stable_since;
    static uint32_t action_hold_start;
    static bool action_released_since_result;

    poll_level( action_btn_id, &action_level, &action_stable_since );

    switch ( clear_ui )
    {
        case CLEAR_UI_IDLE:
            if ( action_level
                 && ( ( now - action_stable_since ) >= DEBOUNCE_MS ) )
            {
                action_hold_start = action_stable_since;
                clear_ui          = CLEAR_UI_HOLDING;
            }
            break;

        case CLEAR_UI_HOLDING:
            if ( !action_level )
            {
                clear_ui = CLEAR_UI_IDLE; /* let go before the bar */
            }
            else if ( ( now - action_hold_start ) >= ACTION_HOLD_MS )
            {
                ( void )ecu_clear_start();
                clear_ui = CLEAR_UI_ACTIVE;
                hal_log_write( "ecu clear: started" );
            }
            else
            {
                /* Continue waiting for the hold threshold. */
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
            if ( !action_level )
            {
                action_released_since_result = true;
            }
            else if ( action_released_since_result
                      && ( ( now - action_stable_since ) >= DEBOUNCE_MS ) )
            {
                clear_ui = CLEAR_UI_IDLE;
            }
            else
            {
                /* Wait for a new debounced press. */
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

            if ( save_notice_active )
            {
                copy_capped( buf, save_notice_ok ? "SAVED" : "SAVE FAILED" );
            }
            else
            {
                copy_capped(
                    buf, current_value( &info, &value ) ? info->name : "NO ECU"
                );
            }
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
        ( void )fmt_value( buf, TEXT_MAX + 1u, info, value );
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
 * px radius, not tiled edge-to-edge like a square display would allow. */
static text_widget_t screen[] = {
    { { 40, 55, 160, 7 }, 1, FG, BG, name_text, { 0 } },
    { { 32, 95, 176, 28 }, 4, FG, BG, value_text, { 0 } },
    { { 60, 135, 120, 14 }, 2, FG, BG, unit_text, { 0 } },
    { { 70, 175, 100, 7 }, 1, FG, BG, link_text, { 0 } },
};
#define SCREEN_COUNT ( sizeof( screen ) / sizeof( screen[0] ) )

/* Boost is unique by decode; rpm shares ECU_DEC_LINEAR, so match by unit
 * instead. Tried in order; first match wins. */
static bool is_boost( const ecu_param_info_t * info )
{
    return info->decode == ECU_DEC_BOOST;
}

static bool is_rpm( const ecu_param_info_t * info )
{
    return strcmp( info->unit, "rpm" ) == 0;
}

typedef struct
{
    bool ( *matches )( const ecu_param_info_t * info );
    const dial_cfg_t * cfg;
    dial_widget_t * widget;
    int32_t value_div; /* ecu_get_value units per dial unit */
} dial_entry_t;

hal_status_t gauge_init(
    const display_pins_t * pins,
    hal_gpio_id_t next_btn,
    hal_gpio_id_t action_btn
)
{
    hal_status_t status = display_init( pins );

    next_btn_id   = next_btn;
    action_btn_id = action_btn;
    /* Log watch setup failures; a button without a watch cannot respond. */
    ( void )hal_gpio_open( next_btn_id, HAL_GPIO_INPUT );
    ( void )hal_gpio_open( action_btn_id, HAL_GPIO_INPUT );
    if ( hal_gpio_watch( next_btn_id ) != HAL_OK )
    {
        hal_log_write( "next btn watch failed" );
    }
    if ( hal_gpio_watch( action_btn_id ) != HAL_OK )
    {
        hal_log_write( "action btn watch failed" );
    }

    if ( status == HAL_OK )
    {
        text_init( screen, SCREEN_COUNT, BG );
    }
    return status;
}

void gauge_poll( uint32_t now_ms )
{
    /* Keep the existing pressure units with the default-style face. */
    static const dial_cfg_t boost_dial_cfg = {
        -30,
        20,
        10,
        4u,
        "TURBO",
        "PSI",
        "INHG",
        0xEF5Bu,
        0x632Cu,
        0xF922u,
        30u,
        DIAL_STYLE_DEFAULT
    };
    static dial_widget_t boost_dial;

    /* The tachometer displays thousands of rpm on the same pale face. */
    static const dial_cfg_t rpm_dial_cfg = {
        0,
        8,
        1,
        0u,
        "RPM",
        "x1000",
        NULL,
        0xEF5Bu,
        0x632Cu,
        0xF922u,
        40u,
        DIAL_STYLE_DEFAULT
    };
    static dial_widget_t rpm_dial;
    static bool selection_restored;
    static const dial_entry_t dial_table[] = {
        { is_boost, &boost_dial_cfg, &boost_dial, 1000 }, /* milli-psi/inHg */
        { is_rpm, &rpm_dial_cfg, &rpm_dial, 1000 },
    };
    static const dial_entry_t * active_dial;
    const ecu_param_info_t * sel_info = NULL;
    const dial_entry_t * next_dial    = NULL;

    if ( !selection_restored && ( ecu_param_count() > 0u ) )
    {
        restore_selected_idx();
        selection_restored = true;
    }
    poll_next( now_ms );
    poll_action( now_ms );
    if ( save_notice_active
         && ( ( now_ms - save_notice_start ) >= SAVE_NOTICE_MS ) )
    {
        save_notice_active = false;
    }

    if ( ecu_param_count() > 0u )
    {
        sel_info = ecu_param_info( selected_idx );
    }
    /* Show clear and save status on the digital screen, even for a dial
     * param. */
    if ( ( clear_ui == CLEAR_UI_IDLE ) && !save_notice_active
         && ( sel_info != NULL ) )
    {
        for ( size_t i = 0;
              ( next_dial == NULL )
              && ( i < ( sizeof( dial_table ) / sizeof( dial_table[0] ) ) );
              i++ )
        {
            if ( dial_table[i].matches( sel_info ) )
            {
                next_dial = &dial_table[i];
            }
        }
    }

    if ( next_dial != active_dial )
    {
        if ( next_dial != NULL )
        {
            dial_init( next_dial->widget, next_dial->cfg, now_ms );
        }
        else
        {
            text_init( screen, SCREEN_COUNT, BG );
        }
        active_dial = next_dial;
    }

    if ( active_dial != NULL )
    {
        const ecu_param_info_t * info = NULL;
        int32_t value                 = 0;
        bool has_value                = current_value( &info, &value );

        dial_poll(
            active_dial->widget,
            now_ms,
            has_value,
            value / active_dial->value_div
        );
    }
    else
    {
        text_poll( screen, SCREEN_COUNT );
    }
}
