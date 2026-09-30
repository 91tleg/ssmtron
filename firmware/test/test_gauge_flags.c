/* Test the flags page: every flag name is printed with its state, and the
 * page gives way to the clear screen and back. The fakes stand in for the
 * ECU, HAL and display; the widgets' own text callbacks are called here to
 * see what would be drawn. */
#include <string.h>

#include "app/fw_info.h"
#include "app/gauge.h"
#include "hal/hal_ble.h"
#include "hal/hal_dfu.h"
#include "hal/hal_random.h"
#include "hal/hal_gpio.h"
#include "hal/hal_log.h"
#include "hal/hal_nvm.h"
#include "lib/display/display.h"
#include "lib/ecu/ecu.h"
#include "lib/ui/dial.h"
#include "lib/ui/text.h"
#include "test/minitest.h"

#define NEXT_ID   ( ( hal_gpio_id_t )1u )
#define ACTION_ID ( ( hal_gpio_id_t )2u )

/* An active-high bit and an active-low bit, so both polarities are seen. */
static const ecu_flag_t test_flags[] = {
    { "22 Knock", 0x02u, true },
    { "Test Mode", 0x20u, false },
};

static uint8_t raw_byte;
static bool value_fresh;

size_t ecu_param_count( void )
{
    return 3u;
}

const ecu_param_info_t * ecu_param_info( size_t idx )
{
    static const ecu_param_info_t info = {
        .name          = "Stored Codes 2/3",
        .unit          = "",
        .unit_neg      = NULL,
        .decode        = ECU_DEC_FLAGS,
        .scale_num     = 1,
        .scale_den     = 1,
        .offset        = 0,
        .decimals      = 0,
        .lookup        = NULL,
        .flags         = test_flags,
        .flag_count    = 2,
        .trouble_codes = true /* trouble codes: the clear applies */
    };
    static const ecu_param_info_t switches = {
        .name          = "Input Switches",
        .unit          = "",
        .unit_neg      = NULL,
        .decode        = ECU_DEC_FLAGS,
        .scale_num     = 1,
        .scale_den     = 1,
        .offset        = 0,
        .decimals      = 0,
        .lookup        = NULL,
        .flags         = test_flags,
        .flag_count    = 2,
        .trouble_codes = false /* switches: nothing to clear */
    };

    static const ecu_param_info_t load = {
        .name          = "Load",
        .unit          = "",
        .unit_neg      = NULL,
        .decode        = ECU_DEC_LINEAR,
        .scale_num     = 1,
        .scale_den     = 1,
        .offset        = 0,
        .decimals      = 0,
        .lookup        = NULL,
        .flags         = NULL,
        .flag_count    = 0,
        .trouble_codes = false /* a plain value: a number page */
    };

    if ( idx == 1u )
    {
        return &switches;
    }
    return ( idx == 0u ) ? &info : ( ( idx == 2u ) ? &load : NULL );
}

static unsigned select_calls;

bool ecu_select_param( size_t idx )
{
    ( void )idx;
    select_calls++;
    return true;
}

bool ecu_get_value(
    size_t idx,
    int32_t * out
)
{
    if ( ( idx == 0u ) && value_fresh )
    {
        *out = ( int32_t )raw_byte;
    }
    return ( idx == 0u ) && value_fresh;
}

static unsigned clear_starts;
static bool clear_start_ok = true; /* false: the ECU layer refuses */

bool ecu_clear_start( void )
{
    clear_starts++;
    return clear_start_ok;
}

static ecu_clear_t clear_state = ECU_CLEAR_BUSY;

ecu_clear_t ecu_clear_state( void )
{
    return clear_state;
}

ecu_link_t ecu_link( void )
{
    return ECU_LINK_DOWN;
}

/* The page that shows the firmware version needs one. */
const fw_info_t fw_info = { "9.9.9-test" };

/* Bluetooth, the random source and the restart that ends update mode. */
static unsigned ble_opens;
static uint32_t ble_opened_passkey;
static hal_status_t ble_open_result = HAL_OK;
static hal_ble_evt_t ble_events[4];
static size_t ble_event_count;
static uint32_t random_value = 123456u;
static unsigned reboots;

hal_status_t hal_ble_open( const hal_ble_cfg_t * cfg )
{
    ble_opens++;
    ble_opened_passkey = cfg->passkey;
    return ble_open_result;
}

hal_ble_evt_t hal_ble_poll_event( void )
{
    hal_ble_evt_t evt = HAL_BLE_EVT_NONE;

    if ( ble_event_count > 0u )
    {
        evt = ble_events[0];
        for ( size_t i = 1; i < ble_event_count; i++ )
        {
            ble_events[i - 1u] = ble_events[i];
        }
        ble_event_count--;
    }
    return evt;
}

hal_status_t hal_random_u32( uint32_t * out )
{
    *out = random_value;
    return HAL_OK;
}

void hal_dfu_reboot( void )
{
    reboots++;
}

/* 0 until the ECU has identified itself, as the real one reports. */
static uint32_t fake_rom_id;
static uint16_t assumed_with = 0xFFFFu; /* the key the gauge assumed */

uint16_t ecu_assume_profile( uint16_t profile_key )
{
    assumed_with = profile_key;
    return profile_key;
}

uint32_t ecu_rom_id( void )
{
    return fake_rom_id;
}

static const char * last_log = "";

void hal_log_write( const char * text )
{
    last_log = text;
}

static unsigned theme_saves;
static uint32_t saved_theme_value;

hal_status_t hal_nvm_save(
    const char * key,
    const void * data,
    size_t len
)
{
    if ( ( strcmp( key, "theme" ) == 0 ) && ( len == sizeof( uint32_t ) ) )
    {
        theme_saves++;
        ( void )memcpy( &saved_theme_value, data, len );
    }
    return HAL_OK;
}

hal_status_t hal_nvm_load(
    const char * key,
    void * data,
    size_t max,
    size_t * len
)
{
    ( void )key;
    ( void )data;
    ( void )max;
    ( void )len;
    return HAL_ERR_INVALID;
}

hal_status_t hal_gpio_open(
    hal_gpio_id_t id,
    hal_gpio_mode_t mode
)
{
    ( void )id;
    ( void )mode;
    return HAL_OK;
}

hal_status_t hal_gpio_watch( hal_gpio_id_t id )
{
    ( void )id;
    return HAL_OK;
}

static uint32_t now;
static bool action_level;
static bool action_pending;
static uint32_t action_at;
static bool next_level;
static bool next_pending;
static uint32_t next_at;

bool hal_gpio_take_edge(
    hal_gpio_id_t id,
    bool * level,
    uint32_t * timestamp_ms
)
{
    bool is_action = ( id == ACTION_ID );
    bool had       = is_action ? action_pending : next_pending;

    if ( had )
    {
        *level         = is_action ? action_level : next_level;
        *timestamp_ms  = is_action ? action_at : next_at;
        action_pending = is_action ? false : action_pending;
        next_pending   = is_action ? next_pending : false;
    }
    return had;
}

hal_status_t display_init( const display_pins_t * pins )
{
    ( void )pins;
    return HAL_OK;
}

/* Whether the first frame was already drawn when the panel was turned on. */
static unsigned frames_drawn;
static bool frame_was_ready_at_display_on;
static unsigned frames_at_display_on;

hal_status_t display_on( void )
{
    frame_was_ready_at_display_on = ( frames_drawn > 0u );
    frames_at_display_on          = frames_drawn;
    return HAL_OK;
}

/* The page currently on screen: the last widget set text_init was given. */
static uint16_t last_page_bg;
static text_widget_t * page;
static size_t page_count;

void text_init(
    text_widget_t * widgets,
    size_t count,
    uint16_t page_bg
)
{
    last_page_bg = page_bg;
    page         = widgets;
    page_count   = count;
    frames_drawn++;
}

void text_poll(
    text_widget_t * widgets,
    size_t count
)
{
    ( void )widgets;
    ( void )count;
}

void dial_init(
    dial_widget_t * d,
    const dial_cfg_t * cfg,
    uint32_t now_ms
)
{
    ( void )d;
    ( void )cfg;
    ( void )now_ms;
}

void dial_poll(
    dial_widget_t * d,
    uint32_t now_ms,
    bool has_value,
    int32_t value
)
{
    ( void )d;
    ( void )now_ms;
    ( void )has_value;
    ( void )value;
}

/* Whether the needle has reached the value it was last given. */
static bool fake_dial_settled;

bool dial_settled( const dial_widget_t * d )
{
    ( void )d;
    return fake_dial_settled;
}

static void tick( uint32_t delta_ms )
{
    now += delta_ms;
    gauge_poll( now );
}

static void press_action( bool level )
{
    action_level   = level;
    action_at      = now;
    action_pending = true;
}

static void press_next( bool level )
{
    next_level   = level;
    next_at      = now;
    next_pending = true;
}

/* What widget i would draw right now. */
static const char * row( size_t i )
{
    static char buf[TEXT_MAX + 1u];

    page[i].text( buf );
    return buf;
}

/* Every widget of the page on screen must lie inside the panel. A rect
 * that hangs off an edge wraps when it is stored as an unsigned position
 * and then draws nothing. */
static void check_widgets_fit_the_panel( void )
{
    for ( size_t i = 0; i < page_count; i++ )
    {
        CHECK( ( uint32_t )page[i].rect.x + page[i].rect.w <= DISPLAY_WIDTH );
        CHECK( ( uint32_t )page[i].rect.y + page[i].rect.h <= DISPLAY_HEIGHT );
        CHECK( page[i].rect.w > 0u );
    }
}

/* The first frame is the saved page itself: here the code page, drawn once
 * before the panel turns on, not a number screen replaced a moment later. */
static void test_first_frame_is_the_saved_page( void )
{
    CHECK_EQ( page_count, 10u ); /* title, eight rows and a status line */
    check_widgets_fit_the_panel();
    CHECK( strcmp( row( 0 ), "  Stored Codes 2/3" ) == 0 );
}

static void test_panel_turns_on_after_exactly_the_first_frame( void )
{
    CHECK( frame_was_ready_at_display_on );
    CHECK_EQ( frames_at_display_on, 1u );
}

/* Nothing is stored on a first run: the first profile (key 0) is assumed,
 * and the ECU answering as that same profile leaves the page alone. */
static void test_the_same_ecu_leaves_the_page_alone( void )
{
    unsigned before;

    CHECK_EQ( assumed_with, 0u );
    before      = select_calls;
    fake_rom_id = 0x010000u; /* profile key 0, as assumed */
    tick( 1 );
    CHECK_EQ( select_calls, before );
    fake_rom_id = 0u; /* the later tests do not involve identification */
}

static void test_flags_page_is_shown_for_a_flags_byte( void )
{
    raw_byte    = 0x02u; /* Knock set, Test Mode bit clear */
    value_fresh = true;
    tick( 1 );
    CHECK_EQ( page_count, 10u ); /* title, eight rows and a status line */
    check_widgets_fit_the_panel();
}

static void test_title_is_centered( void )
{
    CHECK( strcmp( row( 0 ), "  Stored Codes 2/3" ) == 0 );
}

static void test_status_line_is_blank_when_idle( void )
{
    CHECK( strcmp( row( 9 ), "" ) != 0 ); /* padded blanks, no words */
    CHECK( strspn( row( 9 ), " " ) == strlen( row( 9 ) ) );
}

static void test_every_name_is_printed_with_its_state( void )
{
    /* Names pad to 11 columns, then a space, then the state. Test Mode is
     * active-low: its bit is clear, so it reads 1. */
    CHECK( strcmp( row( 1 ), "22 Knock    1" ) == 0 );
    CHECK( strcmp( row( 2 ), "Test Mode   1" ) == 0 );

    raw_byte = 0x20u; /* Knock clear, Test Mode bit set */
    CHECK( strcmp( row( 1 ), "22 Knock    0" ) == 0 );
    CHECK( strcmp( row( 2 ), "Test Mode   0" ) == 0 );
}

static void test_rows_past_the_last_flag_are_blank( void )
{
    for ( size_t i = 3u; i < 9u; i++ )
    {
        CHECK( strcmp( row( i ), "" ) == 0 );
    }
}

static void test_names_stay_when_the_value_is_stale( void )
{
    value_fresh = false;
    CHECK( strcmp( row( 1 ), "22 Knock    -" ) == 0 );
    CHECK( strcmp( row( 2 ), "Test Mode   -" ) == 0 );
    value_fresh = true;
}

static void test_clear_hold_prompts_on_the_flags_page( void )
{
    press_action( true );
    tick( 1 );
    tick( 35 ); /* debounced, but still a possible tap: no prompt yet */
    CHECK( strspn( row( 9 ), " " ) == strlen( row( 9 ) ) );
    tick( 500 );                 /* past the tap window: it is a hold */
    CHECK_EQ( page_count, 10u ); /* the page stays put */
    CHECK( strcmp( row( 9 ), "   HOLD TO CLEAR" ) == 0 );
    CHECK( strcmp( row( 1 ), "22 Knock    0" ) == 0 ); /* flags still drawn */

    press_action( false );
    tick( 1 ); /* released early: back to idle */
    CHECK( strspn( row( 9 ), " " ) == strlen( row( 9 ) ) );
}

static void test_clearing_shows_dots_that_advance( void )
{
    char first[TEXT_MAX + 1u];

    press_action( true );
    tick( 1 );
    tick( 35 );
    tick( 2000 ); /* past the tap window and the 2 s hold */
    tick( 1 );    /* the poll after: the clear starts */
    CHECK_EQ( page_count, 10u );
    CHECK( strncmp( row( 9 ), "    CLEARING", 12 ) == 0 );

    ( void )strcpy( first, row( 9 ) );
    tick( 400 ); /* one step later */
    CHECK( strcmp( row( 9 ), first ) != 0 );
    CHECK( strncmp( row( 9 ), "    CLEARING", 12 ) == 0 );
}

static void test_clear_result_is_shown_on_the_page( void )
{
    uint16_t face_before;

    clear_state = ECU_CLEAR_DONE;
    tick( 1 );
    CHECK( strcmp( row( 9 ), "      CLEARED" ) == 0 );
    CHECK_EQ( page_count, 10u );
    face_before = page[1].bg;

    /* Dismiss with a fresh press. */
    press_action( false );
    tick( 1 );
    press_action( true );
    tick( 1 );
    tick( 35 ); /* settled: this dismisses the result */
    tick( 1 );  /* the next loop pass, button still down */
    press_action( false );
    tick( 1 );
    tick( 35 );
    CHECK( strspn( row( 9 ), " " ) == strlen( row( 9 ) ) );
    /* The press that dismissed the result is not also a theme tap. */
    CHECK_EQ( page[1].bg, face_before );
}

/* A tap in the fake is a press and a release inside the tap window. */
static void tap_action( void )
{
    press_action( true );
    tick( 1 );
    tick( 35 );
    press_action( false );
    tick( 1 );
}

/* One debounced NEXT tap: on to the next page on release. */
static void tap_next( void )
{
    press_next( true );
    tick( 1 );
    tick( 35 );
    press_next( false );
    tick( 1 );
    tick( 35 );
}

/* A switch page has no trouble codes to clear: holding ACTION there does
 * nothing, not even a prompt, and the press is dropped without turning into
 * a theme tap when it is released. */
static void test_hold_on_a_switch_page_does_nothing( void )
{
    uint16_t face_before;
    unsigned starts_before = clear_starts;

    press_next( true );
    tick( 1 );
    tick( 35 );
    press_next( false );
    tick( 1 );
    tick( 35 ); /* a NEXT tap: on to the switch page */
    CHECK( strcmp( row( 0 ), "   Input Switches" ) == 0 );
    face_before = page[1].bg;

    press_action( true );
    tick( 1 );
    tick( 35 );
    tick( 500 ); /* past the tap window: a code page would prompt here */
    CHECK( strspn( row( 9 ), " " ) == strlen( row( 9 ) ) );
    tick( 2500 ); /* well past the 2 s hold */
    CHECK_EQ( clear_starts, starts_before );
    CHECK( strspn( row( 9 ), " " ) == strlen( row( 9 ) ) );

    press_action( false );
    tick( 1 );
    tick( 35 );
    CHECK_EQ( page[1].bg, face_before ); /* no theme change either */

    /* A plain value page is a number screen: the page is named even before
     * the ECU has sent a value, and dashes stand in for the value. */
    tap_next();
    CHECK_EQ( page_count, 3u ); /* name, value and unit */
    check_widgets_fit_the_panel();
    {
        char buf[TEXT_MAX + 1u];

        page[0].text( buf );
        CHECK( strcmp( buf, "Load" ) == 0 );
        page[1].text( buf );
        CHECK( strcmp( buf, "--" ) == 0 );
        page[2].text( buf );
        CHECK( strcmp( buf, "" ) == 0 );
    }

    /* The info page follows the last parameter. It shows the firmware
     * version, the ROM ID as dashes and no link. */
    tap_next();
    CHECK_EQ( page_count, 5u );
    CHECK( strcmp( row( 0 ), "INFO" ) == 0 );
    CHECK( strcmp( row( 1 ), "FW      9.9.9-test" ) == 0 );
    CHECK( strcmp( row( 2 ), "ROM ID  --.--.--" ) == 0 );
    CHECK( strcmp( row( 3 ), "ECU     NO LINK" ) == 0 );
    CHECK_EQ( ble_opens, 0u ); /* Bluetooth stays off until asked */

    /* Holding ACTION here does not clear anything: it starts update mode,
     * with a prompt while the button is held. */
    press_action( true );
    tick( 1 );
    tick( 35 );
    tick( 500 ); /* past the tap window */
    CHECK( strcmp( row( 4 ), "HOLD TO UPDATE" ) == 0 );
    CHECK_EQ( ble_opens, 0u );
    tick( 2000 ); /* the 2 s hold is done */
    CHECK_EQ( clear_starts, starts_before );
    CHECK_EQ( ble_opens, 1u );
    CHECK_EQ( ble_opened_passkey, 123456u ); /* the fake random value */
    CHECK( strcmp( row( 0 ), "UPDATE MODE" ) == 0 );
    CHECK( strcmp( row( 1 ), "PASSKEY   123456" ) == 0 );

    /* The rest of that press is dropped: releasing it is not a theme tap. */
    face_before = page[1].bg;
    press_action( false );
    tick( 1 );
    tick( 35 );
    CHECK_EQ( page[1].bg, face_before );
    CHECK( strcmp( row( 0 ), "UPDATE MODE" ) == 0 );

    /* Holding ACTION again while update mode is on does nothing more. */
    press_action( true );
    tick( 1 );
    tick( 35 );
    tick( 2600 );
    CHECK_EQ( ble_opens, 1u );
    press_action( false );
    tick( 1 );
    tick( 35 );

    /* A NEXT tap leaves update mode by restarting. The fake restart returns,
     * so the info page shows again. */
    CHECK_EQ( reboots, 0u );
    tap_next();
    CHECK_EQ( reboots, 1u );
    CHECK( strcmp( row( 0 ), "INFO" ) == 0 );

    /* Round to the first page for the tests after this one. */
    tap_next();
    CHECK( strcmp( row( 0 ), "  Stored Codes 2/3" ) == 0 );
}

/* If the ECU layer refuses the clear (nothing has identified itself yet),
 * the page says the clear failed at once and the log does not claim it
 * started. */
static void test_a_refused_clear_is_reported_as_refused( void )
{
    clear_start_ok = false;
    press_action( true );
    tick( 1 );
    tick( 35 );
    tick( 2600 ); /* past the prompt and the 2 s hold */
    tick( 1 );    /* the poll after: the clear is attempted */
    CHECK( strcmp( row( 9 ), "    CLEAR FAILED" ) == 0 );
    CHECK( strcmp( last_log, "ecu clear: refused" ) == 0 );

    /* Release, then a fresh press dismisses the result as usual. */
    press_action( false );
    tick( 1 );
    press_action( true );
    tick( 1 );
    tick( 35 );
    press_action( false );
    tick( 1 );
    tick( 35 );
    CHECK( strspn( row( 9 ), " " ) == strlen( row( 9 ) ) );
    clear_start_ok = true;
}

static void test_action_tap_switches_to_dark_and_back( void )
{
    tap_action();
    CHECK_EQ( page_count, 10u );       /* the same page, drawn again */
    CHECK_EQ( last_page_bg, 0x0000u ); /* black face */
    CHECK_EQ( page[1].bg, 0x0000u );
    CHECK_EQ( page[1].fg, 0x9CD3u );                        /* inverted ink */
    CHECK( strspn( row( 9 ), " " ) == strlen( row( 9 ) ) ); /* no prompt */

    tap_action();
    CHECK_EQ( last_page_bg, 0xEF5Bu );
    CHECK_EQ( page[1].bg, 0xEF5Bu );
    CHECK_EQ( page[1].fg, 0x632Cu );
}

static void test_theme_is_saved_only_by_a_next_hold( void )
{
    tap_action();                /* dark */
    CHECK_EQ( theme_saves, 0u ); /* a tap writes nothing */

    press_next( true );
    tick( 1 );
    tick( 35 );
    tick( 1500 ); /* held for the save */
    CHECK_EQ( theme_saves, 1u );
    CHECK_EQ( saved_theme_value, 1u );
    CHECK( strspn( row( 9 ), " " ) != strlen( row( 9 ) ) ); /* SAVED */

    press_next( false );
    tick( 1 );
    tick( 35 );
    tap_action(); /* back to light so nothing after is affected */
}

int main( void )
{
    static const display_pins_t pins = { 0 };

    ( void )gauge_init( &pins, NEXT_ID, ACTION_ID );

    test_first_frame_is_the_saved_page();
    test_panel_turns_on_after_exactly_the_first_frame();
    test_the_same_ecu_leaves_the_page_alone();
    test_flags_page_is_shown_for_a_flags_byte();
    test_title_is_centered();
    test_status_line_is_blank_when_idle();
    test_every_name_is_printed_with_its_state();
    test_rows_past_the_last_flag_are_blank();
    test_names_stay_when_the_value_is_stale();
    test_clear_hold_prompts_on_the_flags_page();
    test_clearing_shows_dots_that_advance();
    test_clear_result_is_shown_on_the_page();
    test_a_refused_clear_is_reported_as_refused();
    test_hold_on_a_switch_page_does_nothing();
    test_action_tap_switches_to_dark_and_back();
    test_theme_is_saved_only_by_a_next_hold();

    return MT_RESULT();
}
