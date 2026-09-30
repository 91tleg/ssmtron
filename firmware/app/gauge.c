#include "gauge.h"

#include <stdint.h>
#include <string.h>

#include "dial.h"
#include "ecu.h"
#include "fmt.h"
#include "gauge_flags.h"
#include "gauge_input.h"
#include "gauge_settings.h"
#include "hal_log.h"
#include "text.h"

/* Two looks. Dark is a black face with the light theme's numbers inverted;
 * the needle stays red in both. The page colors match the dial's face and
 * ink, so every screen follows the theme. */
typedef struct
{
    uint16_t face; /* dial face and page background */
    uint16_t ink;  /* dial ticks, numbers and text */
} theme_t;

#define THEME_COUNT ( 2u )
#define NEEDLE_RED  ( 0xF922u )

static const theme_t themes[THEME_COUNT] = {
    { 0xEF5Bu, 0x632Cu }, /* light */
    { 0x0000u, 0x9CD3u }, /* dark: black face, inverted ink */
};

/* Widgets start in the light colors; apply_theme() sets the real ones. */
#define FG ( 0x632Cu )
#define BG ( 0xEF5Bu )

/* How long the SAVED or SAVE FAILED notice replaces the gauge face. */
#define SAVE_NOTICE_MS ( 1000u )

/* UI state includes HOLDING before any ECU clear request starts. */
typedef enum
{
    CLEAR_UI_IDLE,
    CLEAR_UI_PRESSED, /* down, not yet a tap or a hold */
    CLEAR_UI_HOLDING,
    CLEAR_UI_ACTIVE,
    CLEAR_UI_RESULT
} clear_ui_t;

static clear_ui_t clear_ui = CLEAR_UI_IDLE;

static bool save_notice_active;
static bool save_notice_ok;
static uint32_t save_notice_start;

static size_t theme_idx;
static bool redraw_face; /* the theme changed: draw the current face again */

/* Recolors every screen and dial from themes[theme_idx]. */
static void apply_theme( void );

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

/* Restore selection once ECU identification provides the parameter count. */
static void restore_selected_idx( void )
{
    size_t idx;

    if ( gauge_settings_load_param( ecu_param_count(), &idx ) )
    {
        selected_idx = idx;
    }
    ( void )ecu_select_param( selected_idx );
}

/* The clear prompt, progress or result owns the screen. A press that is
 * still only a possible tap does not, so a tap causes no flicker. */
static bool clear_prompt_shown( void )
{
    return ( clear_ui != CLEAR_UI_IDLE ) && ( clear_ui != CLEAR_UI_PRESSED );
}

static void poll_next( uint32_t now )
{
    switch ( gauge_input_next(
        now, clear_ui != CLEAR_UI_IDLE, ecu_param_count() > 0u
    ) )
    {
        case NEXT_EVENT_TAP:
            /* A short press browses in RAM only. */
            selected_idx = ( selected_idx + 1u ) % ecu_param_count();
            ( void )ecu_select_param( selected_idx );
            break;

        case NEXT_EVENT_HOLD:
            save_notice_ok     = gauge_settings_save( selected_idx, theme_idx );
            save_notice_active = true;
            save_notice_start  = now;
            break;

        case NEXT_EVENT_NONE:
        default:
            break;
    }
}

/* Which press-tracking step the clear UI is in: the gesture events move
 * it, and the two later steps below are driven by the ECU instead. */
static void poll_action_gesture( uint32_t now )
{
    switch ( gauge_input_action( now ) )
    {
        case ACTION_EVENT_PRESS:
            clear_ui = CLEAR_UI_PRESSED;
            break;

        case ACTION_EVENT_TAP:
            theme_idx = ( theme_idx + 1u ) % THEME_COUNT;
            apply_theme();
            redraw_face = true;
            clear_ui    = CLEAR_UI_IDLE;
            break;

        case ACTION_EVENT_HOLD:
            clear_ui = CLEAR_UI_HOLDING;
            break;

        case ACTION_EVENT_ABORT:
            clear_ui = CLEAR_UI_IDLE;
            break;

        case ACTION_EVENT_CLEAR:
            ( void )ecu_clear_start();
            clear_ui = CLEAR_UI_ACTIVE;
            hal_log_write( "ecu clear: started" );
            break;

        case ACTION_EVENT_NONE:
        default:
            break;
    }
}

static void poll_action( uint32_t now )
{
    static bool action_released_since_result;

    switch ( clear_ui )
    {
        case CLEAR_UI_IDLE:
        case CLEAR_UI_PRESSED:
        case CLEAR_UI_HOLDING:
            poll_action_gesture( now );
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
            if ( !gauge_input_action_down() )
            {
                action_released_since_result = true;
            }
            else if ( action_released_since_result
                      && gauge_input_action_settled( now ) )
            {
                clear_ui = CLEAR_UI_IDLE;
                gauge_input_action_reset();
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
    /* The name sits where the dials put their label (centered, y = 89). */
    { { 48, 89, 144, 7 }, 1, FG, BG, name_text, { 0 }, TEXT_ALIGN_CENTER },
    { { 24, 104, 192, 28 }, 4, FG, BG, value_text, { 0 }, TEXT_ALIGN_CENTER },
    { { 48, 140, 144, 14 }, 2, FG, BG, unit_text, { 0 }, TEXT_ALIGN_CENTER },
    { { 48, 175, 144, 7 }, 1, FG, BG, link_text, { 0 }, TEXT_ALIGN_CENTER },
};
#define SCREEN_COUNT ( sizeof( screen ) / sizeof( screen[0] ) )

/* What the flags page shows under the flags. */
static flags_status_t flags_status( void )
{
    flags_status_t status = FLAGS_STATUS_NONE;

    switch ( clear_ui )
    {
        case CLEAR_UI_HOLDING:
            status = FLAGS_STATUS_HOLD_TO_CLEAR;
            break;

        case CLEAR_UI_ACTIVE:
            status = FLAGS_STATUS_CLEARING;
            break;

        case CLEAR_UI_RESULT:
            status = ( ecu_clear_state() == ECU_CLEAR_DONE )
                         ? FLAGS_STATUS_CLEARED
                         : FLAGS_STATUS_CLEAR_FAILED;
            break;

        case CLEAR_UI_IDLE:
        case CLEAR_UI_PRESSED:
        default:
            if ( save_notice_active )
            {
                status = save_notice_ok ? FLAGS_STATUS_SAVED
                                        : FLAGS_STATUS_SAVE_FAILED;
            }
            break;
    }
    return status;
}

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

/* Keep the existing pressure units with the default-style face. Colors
 * are set by apply_theme(), so these are not const. */
static dial_cfg_t boost_dial_cfg = {
    -30,
    20,
    10,
    4u,
    "TURBO",
    "PSI",
    "INHG",
    0xEF5Bu,
    0x632Cu,
    NEEDLE_RED,
    30u,
    DIAL_STYLE_DEFAULT
};
static dial_widget_t boost_dial;

/* The tachometer displays thousands of rpm on the same face. */
static dial_cfg_t rpm_dial_cfg = {
    0,
    8,
    1,
    0u,
    "RPM",
    "x1000",
    NULL,
    0xEF5Bu,
    0x632Cu,
    NEEDLE_RED,
    40u,
    DIAL_STYLE_DEFAULT
};
static dial_widget_t rpm_dial;

static const dial_entry_t dial_table[] = {
    { is_boost, &boost_dial_cfg, &boost_dial, 1000 }, /* milli-psi/inHg */
    { is_rpm, &rpm_dial_cfg, &rpm_dial, 1000 },
};

static void apply_theme( void )
{
    const theme_t * t = &themes[theme_idx];

    boost_dial_cfg.face_bg = t->face;
    boost_dial_cfg.tick_fg = t->ink;
    rpm_dial_cfg.face_bg   = t->face;
    rpm_dial_cfg.tick_fg   = t->ink;
    for ( size_t i = 0; i < SCREEN_COUNT; i++ )
    {
        screen[i].bg = t->face;
        screen[i].fg = t->ink;
    }
    gauge_flags_set_colors( t->face, t->ink );
}

hal_status_t gauge_init(
    const display_pins_t * pins,
    hal_gpio_id_t next_btn,
    hal_gpio_id_t action_btn
)
{
    hal_status_t status = display_init( pins );

    gauge_input_init( next_btn, action_btn );

    ( void )gauge_settings_load_theme( THEME_COUNT, &theme_idx );
    apply_theme();
    if ( status == HAL_OK )
    {
        text_init( screen, SCREEN_COUNT, themes[theme_idx].face );
    }
    return status;
}

void gauge_poll( uint32_t now_ms )
{
    static bool selection_restored;
    static const dial_entry_t * active_dial;
    static bool flags_active;
    bool face_idle;
    bool want_flags;
    const ecu_param_info_t * sel_info = NULL;
    const dial_entry_t * next_dial    = NULL;

    if ( !selection_restored && ( ecu_param_count() > 0u ) )
    {
        restore_selected_idx();
        selection_restored = true;
    }
    gauge_input_poll();
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
    /* The flags page shows clear and save status itself. A dial gives way
     * to the digital screen for them. */
    want_flags = gauge_flags_handles( sel_info );
    face_idle  = !clear_prompt_shown() && !save_notice_active
                && ( sel_info != NULL ) && !want_flags;
    if ( face_idle )
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

    if ( ( next_dial != active_dial ) || ( want_flags != flags_active )
         || redraw_face )
    {
        if ( next_dial != NULL )
        {
            dial_init( next_dial->widget, next_dial->cfg, now_ms );
        }
        else if ( want_flags )
        {
            gauge_flags_open( themes[theme_idx].face );
        }
        else
        {
            text_init( screen, SCREEN_COUNT, themes[theme_idx].face );
        }
        active_dial  = next_dial;
        flags_active = want_flags;
        redraw_face  = false;
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
    else if ( flags_active )
    {
        gauge_flags_poll( selected_idx, flags_status(), now_ms );
    }
    else
    {
        text_poll( screen, SCREEN_COUNT );
    }
}
