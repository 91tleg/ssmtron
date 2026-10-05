#include "app/gauge.h"

#include <stdint.h>
#include <string.h>

#include "app/gauge_flags.h"
#include "app/gauge_info.h"
#include "app/gauge_input.h"
#include "app/gauge_number.h"
#include "app/gauge_settings.h"
#include "app/gauge_sweep.h"
#include "app/gauge_update.h"
#include "hal/hal_log.h"
#include "lib/ecu/ecu.h"
#include "lib/ui/dial.h"

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

/* The info page follows the last parameter in the cycle. While it is shown,
 * selected_idx keeps the last parameter, which the ECU keeps streaming. */
static bool info_shown;

/* Restore the saved page. */
static void restore_selected_idx( void )
{
    size_t idx;

    if ( gauge_settings_load_param( ecu_param_count(), &idx ) )
    {
        selected_idx = idx;
    }
    ( void )ecu_select_param( selected_idx );
}

/* Restores the saved page as soon as the ECU has pages to choose from. */
static void restore_selection_once( void )
{
    static bool selection_restored;

    if ( !selection_restored && ( ecu_param_count() > 0u ) )
    {
        restore_selected_idx();
        selection_restored = true;
    }
}

/* The ECU profile whose pages are on show, as its 16-bit key. It is what a
 * save stores next to the page number. */
static uint16_t profile_key;

/* Shows the pages of the ECU remembered from last time, or of the first
 * profile on a first run, so the first frame already has its pages. The ECU
 * still has to identify itself before anything is written to it. */
static void assume_remembered_profile( void )
{
    size_t key = 0u;

    ( void )gauge_settings_load_profile( &key );
    profile_key = ecu_assume_profile( ( uint16_t )key );
    restore_selection_once();
}

/* Once the ECU answers, a different ECU than the one assumed replaces its
 * pages, and the page goes back to the first. */
static void check_identification( void )
{
    static bool identification_checked;
    uint32_t id = ecu_rom_id();

    if ( ( id != 0u ) && !identification_checked )
    {
        uint16_t key = ( uint16_t )( id & 0xFFFFu );

        identification_checked = true;
        if ( key != profile_key )
        {
            profile_key  = key;
            selected_idx = 0u;
            ( void )ecu_select_param( 0u );
        }
    }
}

/* The selected page's parameter, or NULL while the ECU has none. */
static const ecu_param_info_t * selected_info( void )
{
    const ecu_param_info_t * info = NULL;

    if ( selected_idx < ecu_param_count() )
    {
        info = ecu_param_info( selected_idx );
    }
    return info;
}

/* The selected parameter's fresh value, if it has one. */
static bool selected_value( int32_t * value )
{
    return ( selected_idx < ecu_param_count() )
           && ecu_get_value( selected_idx, value );
}

/* The clear command applies to the trouble-code pages only. */
static bool clear_allowed( void )
{
    const ecu_param_info_t * info = selected_info();

    return !info_shown && ( info != NULL ) && info->trouble_codes;
}

/* The pages are the ECU's parameters in order, then the info page, then
 * round to the first again. The info page exists without an ECU, so NEXT
 * always has somewhere to go. */
static void next_page( void )
{
    if ( info_shown )
    {
        info_shown   = false;
        selected_idx = 0u;
        ( void )ecu_select_param( selected_idx );
    }
    else if ( ( selected_idx + 1u ) >= ecu_param_count() )
    {
        info_shown = true;
    }
    else
    {
        selected_idx++;
        ( void )ecu_select_param( selected_idx );
    }
}

static void poll_next( uint32_t now )
{
    switch ( gauge_input_next( now, clear_ui != CLEAR_UI_IDLE, true ) )
    {
        case NEXT_EVENT_TAP:
            if ( gauge_update_active() )
            {
                gauge_update_exit(); /* a tap leaves update mode */
            }
            else
            {
                /* Moving to another page ends the power-on sweep. */
                gauge_sweep_cancel();
                /* A short press browses in RAM only. */
                next_page();
            }
            break;

        case NEXT_EVENT_HOLD:
            if ( !gauge_update_active() )
            {
                save_notice_ok =
                    gauge_settings_save( selected_idx, theme_idx, profile_key );
                save_notice_active = true;
                save_notice_start  = now;
            }
            break;

        case NEXT_EVENT_NONE:
        default:
            break;
    }
}

/* Which press-tracking step the clear UI is in: the gesture events move
 * it, and the two later steps below are driven by the ECU instead. */
/* Holding ACTION means something on the trouble-code pages (clear the codes)
 * and on the info page (start update mode), and nothing while update mode is
 * already on. */
static bool hold_allowed( void )
{
    return !gauge_update_active() && ( clear_allowed() || info_shown );
}

/* After a clear result shows, the button must be seen released before a new
 * press dismisses it. */
static bool clear_result_released;

/* The last clear attempt was refused, so the ECU's own result, which may
 * still be that of an earlier clear, does not describe it. */
static bool clear_refused;

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
            if ( hold_allowed() )
            {
                clear_ui = CLEAR_UI_HOLDING;
            }
            else
            {
                /* Holding does nothing on a page with no trouble codes to
                 * clear. The press is dropped, so it cannot turn into a tap
                 * when it is released. */
                clear_ui = CLEAR_UI_IDLE;
                gauge_input_action_reset();
            }
            break;

        case ACTION_EVENT_ABORT:
            clear_ui = CLEAR_UI_IDLE;
            break;

        case ACTION_EVENT_CLEAR:
            if ( info_shown )
            {
                /* On the info page the hold starts update mode. The rest of
                 * the press is dropped, so it cannot also be a tap. */
                gauge_update_start( now );
                clear_ui = CLEAR_UI_IDLE;
                gauge_input_action_reset();
            }
            else if ( ecu_clear_start() )
            {
                clear_refused = false;
                clear_ui      = CLEAR_UI_ACTIVE;
                hal_log_write( "ecu clear: started" );
            }
            else
            {
                /* The ECU layer refused: nothing has identified itself yet.
                 * Say so at once, instead of showing a clear that is not
                 * happening. */
                clear_refused         = true;
                clear_ui              = CLEAR_UI_RESULT;
                clear_result_released = false;
                hal_log_write( "ecu clear: refused" );
            }
            break;

        case ACTION_EVENT_NONE:
        default:
            break;
    }
}

static void poll_action( uint32_t now )
{
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
                clear_ui              = CLEAR_UI_RESULT;
                clear_result_released = false;
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
                /* A release that has settled: contact bounce while the
                 * button is still held does not count. */
                if ( gauge_input_action_settled( now ) )
                {
                    clear_result_released = true;
                }
            }
            else if ( clear_result_released
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

/* The save result as a notice line, or nothing while there is none. */
static const char * save_notice_text( void )
{
    const char * text = "";

    if ( save_notice_active )
    {
        text = save_notice_ok ? "SAVED" : "SAVE FAILED";
    }
    return text;
}

/* The number page shows the selected parameter. The value is fetched before
 * the call, since an argument list does not say what is evaluated first. */
static void number_page_open( void )
{
    int32_t value  = 0;
    bool has_value = selected_value( &value );

    gauge_number_open(
        themes[theme_idx].face,
        selected_info(),
        has_value,
        value,
        save_notice_text()
    );
}

static void number_page_poll( void )
{
    int32_t value  = 0;
    bool has_value = selected_value( &value );

    gauge_number_poll( selected_info(), has_value, value, save_notice_text() );
}

/* What the info page shows under its rows. Holding ACTION there starts
 * update mode, so it says so while the button is held. */
static const char * current_info_notice( void )
{
    const char * text;

    if ( clear_ui == CLEAR_UI_HOLDING )
    {
        text = "HOLD TO UPDATE";
    }
    else
    {
        text = save_notice_text();
    }
    return text;
}

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
            status =
                ( !clear_refused && ( ecu_clear_state() == ECU_CLEAR_DONE ) )
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

/* Boost is unique by decode. Tried in order; first match wins. */
static bool is_boost( const ecu_param_info_t * info )
{
    return info->decode == ECU_DEC_BOOST;
}

/* Match the parameter identity, so other temperature and voltage readings
 * keep their own pages. Coolant profiles report either whole or tenths F. */
static bool is_coolant( const ecu_param_info_t * info )
{
    return ( strcmp( info->name, "Coolant Temp" ) == 0 )
           && ( strcmp( info->unit, "F" ) == 0 ) && ( info->decimals <= 1u );
}

static bool is_voltage( const ecu_param_info_t * info )
{
    return ( strcmp( info->name, "System Voltage" ) == 0 )
           && ( strcmp( info->unit, "V" ) == 0 ) && ( info->decimals == 2u );
}

/* The oxygen sensor's voltage, in hundredths of a volt. */
static bool is_o2( const ecu_param_info_t * info )
{
    return ( strcmp( info->name, "Oxygen Sensor" ) == 0 )
           && ( strcmp( info->unit, "V" ) == 0 ) && ( info->decimals == 2u );
}

typedef struct
{
    bool ( *matches )( const ecu_param_info_t * info );
    const dial_cfg_t * cfg;
} dial_entry_t;

/* Dials are picked by index into the table. Their widgets live in a
 * separate array, so no widget pointer is read through the const table. */
#define DIAL_COUNT ( 4u )
#define NO_DIAL    ( SIZE_MAX )

/* The scales are in the ECU's own units, so the needle keeps every digit of
 * the reading: thousandths of a psi or inHg here. label_div sets the units
 * the tick numbers are in. Colors are set by apply_theme(), so these are
 * not const. */
static dial_cfg_t boost_dial_cfg = {
    -30000,
    30000,
    5000,
    4u,
    1000,
    "TURBO",
    "PSI",
    "INHG",
    0xEF5Bu,
    0x632Cu,
    NEEDLE_RED,
    900u,
    DIAL_STYLE_FULL_SWEEP,
    10000,
    1u
};

/* Upper scales use tenths F and hundredths V to keep ECU precision. */
static dial_cfg_t coolant_dial_cfg = {
    1000,
    2600,
    400,
    3u,
    10,
    "COOLANT",
    "F",
    NULL,
    0xEF5Bu,
    0x632Cu,
    NEEDLE_RED,
    900u,
    DIAL_STYLE_UPPER_ARC,
    0,
    0u
};

static dial_cfg_t voltage_dial_cfg = {
    800,
    1600,
    200,
    3u,
    100,
    "VOLTAGE",
    "V",
    NULL,
    0xEF5Bu,
    0x632Cu,
    NEEDLE_RED,
    900u,
    DIAL_STYLE_UPPER_ARC,
    0,
    0u
};

/* The oxygen sensor swings between about 0.1 V (lean) and 0.9 V (rich), in
 * hundredths of a volt. Its numbers are in volts, 0.2 apart, with three
 * ticks between them, like the coolant dial. */
static dial_cfg_t o2_dial_cfg = {
    0,
    100,
    20,
    3u,
    100,
    "O2",
    "V",
    NULL,
    0xEF5Bu,
    0x632Cu,
    NEEDLE_RED,
    900u,
    DIAL_STYLE_UPPER_ARC,
    0,
    0u
};

static void apply_theme( void )
{
    const theme_t * t = &themes[theme_idx];

    boost_dial_cfg.face_bg   = t->face;
    boost_dial_cfg.tick_fg   = t->ink;
    coolant_dial_cfg.face_bg = t->face;
    coolant_dial_cfg.tick_fg = t->ink;
    voltage_dial_cfg.face_bg = t->face;
    voltage_dial_cfg.tick_fg = t->ink;
    o2_dial_cfg.face_bg      = t->face;
    o2_dial_cfg.tick_fg      = t->ink;
    gauge_info_set_colors( t->face, t->ink );
    gauge_update_set_colors( t->face, t->ink );
    gauge_number_set_colors( t->face, t->ink );
    gauge_flags_set_colors( t->face, t->ink );
}

/* Picks the face for the page on show, draws it afresh when it changes, and
 * polls it. gauge_init calls it once before the panel is turned on, so the
 * first frame is already the saved page. */
static void update_face( uint32_t now_ms )
{
    static dial_widget_t dial_widgets[DIAL_COUNT];
    static const dial_entry_t dial_table[DIAL_COUNT] = {
        { is_boost, &boost_dial_cfg },
        { is_coolant, &coolant_dial_cfg },
        { is_voltage, &voltage_dial_cfg },
        { is_o2, &o2_dial_cfg },
    };
    static size_t active_dial = NO_DIAL;

    static bool flags_active;
    static bool info_active;
    static bool update_active;
    static bool face_drawn; /* a first frame exists: nothing is stale before */
    bool face_idle;
    bool want_flags;
    bool want_info;
    bool want_update;
    const ecu_param_info_t * sel_info = NULL;
    size_t next_dial                  = NO_DIAL;

    if ( ecu_param_count() > 0u )
    {
        sel_info = ecu_param_info( selected_idx );
    }
    /* The flags page shows clear and save status itself. A dial gives way
     * to the digital screen for them. */
    want_update = gauge_update_active();
    want_info   = info_shown && !want_update;
    want_flags  = !want_info && !want_update && gauge_flags_handles( sel_info );
    face_idle   = !want_update && !want_info && !save_notice_active
                && ( sel_info != NULL ) && !want_flags;
    if ( face_idle )
    {
        for ( size_t i = 0; ( next_dial == NO_DIAL ) && ( i < DIAL_COUNT );
              i++ )
        {
            if ( dial_table[i].matches( sel_info ) )
            {
                next_dial = i;
            }
        }
    }

    /* A page that is not a dial came first: no sweep. A blank face while the
     * ECU has no pages yet does not count, so the sweep waits for the dial. */
    if ( want_update || want_info || want_flags
         || ( face_idle && ( next_dial == NO_DIAL ) ) )
    {
        gauge_sweep_cancel();
    }

    if ( ( next_dial != active_dial ) || ( want_flags != flags_active )
         || ( want_info != info_active ) || ( want_update != update_active )
         || redraw_face || !face_drawn )
    {
        if ( next_dial != NO_DIAL )
        {
            /* Drawing a dial again, after the first frame, puts its needle
             * back at rest. A sweep that is under way would think the needle
             * had arrived, and would chase a target from the wrong place, so
             * it ends here. */
            if ( face_drawn )
            {
                gauge_sweep_cancel();
            }
            dial_init(
                &dial_widgets[next_dial], dial_table[next_dial].cfg, now_ms
            );
        }
        else if ( want_update )
        {
            gauge_update_open( themes[theme_idx].face, now_ms );
        }
        else if ( want_info )
        {
            gauge_info_open( themes[theme_idx].face, current_info_notice() );
        }
        else if ( want_flags )
        {
            gauge_flags_open(
                themes[theme_idx].face, selected_idx, flags_status(), now_ms
            );
        }
        else
        {
            number_page_open();
        }
        active_dial   = next_dial;
        flags_active  = want_flags;
        info_active   = want_info;
        update_active = want_update;
        redraw_face   = false;
        face_drawn    = true;
    }

    if ( active_dial != NO_DIAL )
    {
        /* The selected parameter's own reading, not another's that has not
         * gone stale yet. The dial takes it in the ECU's units. */
        int32_t dial_value  = 0;
        bool has_value      = selected_value( &dial_value );
        int32_t sweep_value = 0;

        /* Hitachi coolant is whole F; JECS coolant already has tenths. */
        if ( has_value && is_coolant( sel_info )
             && ( sel_info->decimals == 0u ) )
        {
            dial_value *= 10;
        }

        /* At power-on the needle sweeps from rest to full scale, to the
         * bottom and back to rest; the live reading, or rest if there is
         * none, takes over after that. */
        if ( gauge_sweep_target(
                 dial_table[active_dial].cfg,
                 dial_settled( &dial_widgets[active_dial] ),
                 now_ms,
                 &sweep_value
             ) )
        {
            has_value  = true;
            dial_value = sweep_value;
        }
        dial_poll( &dial_widgets[active_dial], now_ms, has_value, dial_value );
    }
    else if ( update_active )
    {
        gauge_update_poll( now_ms );
    }
    else if ( info_active )
    {
        gauge_info_poll( current_info_notice() );
    }
    else if ( flags_active )
    {
        gauge_flags_poll( selected_idx, flags_status(), now_ms );
    }
    else
    {
        number_page_poll();
    }
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
    assume_remembered_profile();
    if ( status == HAL_OK )
    {
        /* Draw the saved page itself, not a stand-in for it. The panel was
         * left off by display_init, so it lights up already showing it. */
        update_face( 0u );
        ( void )display_on();
    }
    return status;
}

void gauge_poll( uint32_t now_ms )
{
    restore_selection_once();
    check_identification();
    gauge_input_poll();
    poll_next( now_ms );
    poll_action( now_ms );
    if ( save_notice_active
         && ( ( now_ms - save_notice_start ) >= SAVE_NOTICE_MS ) )
    {
        save_notice_active = false;
    }

    update_face( now_ms );
}
