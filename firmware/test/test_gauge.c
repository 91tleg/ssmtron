/* Test the button state machines against fake ECU, HAL, and NVM calls.
 * Each test phase continues from the previous phase's state. */
#include <string.h>

#include "hal/hal_random.h"
#include "hal/hal_dfu.h"
#include "hal/hal_ble.h"
#include "app/fw_info.h"
#include "lib/ui/dial.h"
#include "lib/ecu/ecu.h"
#include "app/gauge.h"
#include "hal/hal_gpio.h"
#include "hal/hal_log.h"
#include "hal/hal_nvm.h"
#include "test/minitest.h"
#include "lib/ui/text.h"

#define NEXT_ID   ( ( hal_gpio_id_t )1u )
#define ACTION_ID ( ( hal_gpio_id_t )2u )

static bool clear_started;
static ecu_clear_t clear_result = ECU_CLEAR_BUSY;
static size_t last_selected_idx = 999u; /* sentinel: no selection observed */

size_t ecu_param_count( void )
{
    return 3u;
}

const ecu_param_info_t * ecu_param_info( size_t idx )
{
    static const ecu_param_info_t info = {
        .name       = "RPM",
        .unit       = "rpm",
        .unit_neg   = NULL,
        .decode     = ECU_DEC_LINEAR,
        .scale_num  = 1,
        .scale_den  = 1,
        .offset     = 0,
        .decimals   = 0,
        .lookup     = NULL,
        .flags      = NULL,
        .flag_count = 0,
        .trouble_codes =
            true /* the clear applies: these tests exercise the buttons */
    };

    return ( idx < 3u ) ? &info : NULL;
}

bool ecu_select_param( size_t idx )
{
    last_selected_idx = idx;
    return true;
}

bool ecu_get_value(
    size_t idx,
    int32_t * out
)
{
    ( void )idx;
    ( void )out;
    return false; /* never a fresh value: rendering is not under test */
}

bool ecu_clear_start( void )
{
    clear_started = true;
    return true;
}

ecu_clear_t ecu_clear_state( void )
{
    return clear_result;
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

uint32_t ecu_id( void )
{
    return fake_rom_id;
}

/* Dashes until identified, as the real ECU layer shows. */
void ecu_id_text(
    char * buf,
    size_t size
)
{
    if ( size > 0u )
    {
        ( void )strncpy(
            buf, ( fake_rom_id == 0u ) ? "--.--.--" : "AB.CD.EF", size - 1u
        );
        buf[size - 1u] = '\0';
    }
}

void hal_log_write( const char * text )
{
    ( void )text;
}

static bool nvm_save_called;
static uint32_t nvm_saved_value;

hal_status_t hal_nvm_save(
    const char * key,
    const void * data,
    size_t len
)
{
    ( void )key;
    nvm_save_called = true;
    if ( len == sizeof( nvm_saved_value ) )
    {
        ( void )memcpy( &nvm_saved_value, data, len );
    }
    return HAL_OK;
}

/* Tests can provide a stored selection before the first poll. */
static bool nvm_load_ok;
static uint32_t nvm_load_value;

hal_status_t hal_nvm_load(
    const char * key,
    void * data,
    size_t max,
    size_t * len
)
{
    hal_status_t status = HAL_ERR_INVALID;

    ( void )key;
    if ( nvm_load_ok && ( max >= sizeof( nvm_load_value ) ) )
    {
        ( void )memcpy( data, &nvm_load_value, sizeof( nvm_load_value ) );
        *len   = sizeof( nvm_load_value );
        status = HAL_OK;
    }
    return status;
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

/* One pending edge per button, consumed by the next poll. */
typedef struct
{
    bool level;
    bool pending;
    uint32_t timestamp_ms;
} fake_edge_t;

static fake_edge_t next_edge;
static fake_edge_t action_edge;

hal_status_t hal_gpio_watch( hal_gpio_id_t id )
{
    ( void )id;
    return HAL_OK;
}

bool hal_gpio_take_edge(
    hal_gpio_id_t id,
    bool * level,
    uint32_t * timestamp_ms
)
{
    fake_edge_t * edge = ( id == ACTION_ID ) ? &action_edge : &next_edge;
    bool had_edge      = edge->pending;

    if ( had_edge )
    {
        *level        = edge->level;
        *timestamp_ms = edge->timestamp_ms;
        edge->pending = false;
    }
    return had_edge;
}

hal_status_t display_init( const display_pins_t * pins )
{
    ( void )pins;
    return HAL_OK;
}

hal_status_t display_on( void )
{
    return HAL_OK;
}

void text_init(
    text_widget_t * widgets,
    size_t count,
    uint16_t page_bg
)
{
    ( void )widgets;
    ( void )count;
    ( void )page_bg;
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

static uint32_t now;

static void tick( uint32_t delta_ms )
{
    now += delta_ms;
    gauge_poll( now );
}

/* Latch an edge at the simulated time; tick() lets gauge.c consume it. */
static void press_next( bool level )
{
    next_edge.level        = level;
    next_edge.timestamp_ms = now;
    next_edge.pending      = true;
}

static void press_action( bool level )
{
    action_edge.level        = level;
    action_edge.timestamp_ms = now;
    action_edge.pending      = true;
}

static void test_brief_press_below_debounce_does_nothing( void )
{
    press_action( true );
    tick( 5 ); /* too brief to debounce */
    press_action( false );
    tick( 5 );
    CHECK( !clear_started );
}

static void test_release_before_threshold_cancels( void )
{
    press_action( true );
    tick( 1 );   /* press observed */
    tick( 35 );  /* past debounce: a press, not yet a hold */
    tick( 500 ); /* still short of the 2 s hold */
    press_action( false );
    tick( 1 );    /* released early: back to idle, no clear */
    tick( 2000 ); /* long after, still idle */
    CHECK( !clear_started );
}

static void test_hold_past_threshold_triggers_clear( void )
{
    press_action( true );
    tick( 1 );
    tick( 35 );   /* undecided: a tap or a hold */
    tick( 1965 ); /* past the tap window it became a hold, and 2 s is up */
    tick( 1 );    /* the poll after: the clear starts */
    CHECK( clear_started );
}

static void test_held_button_does_not_retrigger_while_clearing( void )
{
    /* Continues from the trigger above: button is still held. */
    clear_started = false;
    clear_result  = ECU_CLEAR_DONE;
    tick( 1 ); /* clear finishes: shows the result, button still held */
    tick( 1 ); /* held indefinitely: must not fire a second clear */
    CHECK( !clear_started );
}

static void test_dismissing_the_result_does_not_retrigger( void )
{
    /* Still held from the previous phase; release, then a fresh press
     * dismisses the result screen rather than starting a new clear. */
    press_action( false );
    tick( 1 );  /* release seen */
    tick( 35 ); /* and settled */
    press_action( true );
    tick( 1 );  /* a fresh press starts */
    tick( 35 ); /* past debounce: dismisses the result, not a new clear */
    CHECK( !clear_started );
    press_action( false );
    tick( 1 ); /* leave the button released for anything that follows */
    tick( 35 );
}

/* Selection restore runs only on the first poll, so this test runs first. */
static void test_restores_saved_selection_on_first_poll( void )
{
    tick( 1 ); /* triggers restore_selected_idx */
    CHECK_EQ( last_selected_idx, 2 );
}

/* One debounced tap: the parameter advances on release. */
static void tap_next( void )
{
    press_next( true );
    tick( 1 );
    tick( 35 );
    press_next( false );
    tick( 1 );
    tick( 35 );
}

static void test_next_tap_advances_on_release_without_saving( void )
{
    last_selected_idx = 999u;
    nvm_save_called   = false;
    press_next( true );
    tick( 1 );
    tick( 35 ); /* past debounce: still only a candidate press */
    CHECK_EQ( last_selected_idx, 999u );
    press_next( false );
    tick( 1 );
    tick( 35 ); /* release settles: one tap */
    /* From the last parameter the next page is the info page, which selects
     * no parameter; one more tap goes round to the first. */
    CHECK_EQ( last_selected_idx, 999u );
    tap_next();
    CHECK_EQ( last_selected_idx, 0u );
    CHECK( !nvm_save_called );
}

static void test_next_hold_saves_without_advancing( void )
{
    last_selected_idx = 999u;
    nvm_save_called   = false;
    press_next( true );
    tick( 1 );
    tick( 35 );
    tick( 1400 ); /* short of the 1.5 s hold */
    CHECK( !nvm_save_called );
    tick( 100 ); /* crosses it */
    CHECK( nvm_save_called );
    CHECK_EQ( nvm_saved_value, 0u ); /* the shown parameter, not the next */

    press_next( false );
    tick( 1 );
    tick( 35 );
    CHECK_EQ( last_selected_idx, 999u ); /* release after a save is inert */
}

static void test_next_hold_of_saved_selection_skips_the_write( void )
{
    nvm_save_called = false;
    press_next( true );
    tick( 1 );
    tick( 35 );
    tick( 1600 );
    CHECK( !nvm_save_called ); /* same index as the stored one */
    press_next( false );
    tick( 1 );
    tick( 35 );
}

static void test_next_hold_is_held_once( void )
{
    tap_next(); /* 0 -> 1, so the next hold has something new to save */
    nvm_save_called = false;
    press_next( true );
    tick( 1 );
    tick( 35 );
    tick( 1600 );
    CHECK( nvm_save_called );
    CHECK_EQ( nvm_saved_value, 1u );
    nvm_save_called = false;
    tick( 5000 ); /* still held: no second write */
    CHECK( !nvm_save_called );
    press_next( false );
    tick( 1 );
    tick( 35 );
}

static void test_next_wraps_around( void )
{
    last_selected_idx = 999u;
    tap_next(); /* 1 -> 2 */
    CHECK_EQ( last_selected_idx, 2u );
    tap_next(); /* 2 -> the info page, which selects nothing */
    CHECK_EQ( last_selected_idx, 2u );
    tap_next(); /* the info page -> 0 (wraps) */
    CHECK_EQ( last_selected_idx, 0u );
}

/* The HAL hands over a press first even when the release already
 * happened, so a tap that fits inside one stalled loop pass still counts. */
static void test_next_tap_across_a_stall_still_counts( void )
{
    last_selected_idx = 999u;
    press_next( true );
    tick( 150 ); /* one long pass: sees only the press */
    press_next( false );
    tick( 35 ); /* the release arrives on the following pass */
    CHECK_EQ( last_selected_idx, 1u ); /* 0 -> 1 */
}

static void test_next_blocked_during_clear( void )
{
    press_action( true );
    tick( 1 );
    tick( 35 ); /* HOLDING */

    last_selected_idx = 999u;
    press_next( true );
    tick( 1 );
    tick( 35 );                          /* would normally count as a press */
    CHECK_EQ( last_selected_idx, 999u ); /* blocked: clear_ui != IDLE */

    press_next( false );
    tick( 1 );
    press_action( false );
    tick( 1 ); /* released before the 2 s hold: back to IDLE */
}

/* The profile assumed at power-on came from the saved key (the fake store
 * returns 2 for every key). A different ECU answering replaces its pages,
 * and the page goes back to the first. */
static void test_a_different_ecu_returns_to_the_first_page( void )
{
    CHECK_EQ( assumed_with, 2u );
    last_selected_idx = 999u;
    fake_rom_id       = 0x013272u; /* a 92 NA, not the assumed profile */
    tick( 1 );
    CHECK_EQ( last_selected_idx, 0u );
}

int main( void )
{
    static const display_pins_t pins = { 0 };

    nvm_load_ok    = true;
    nvm_load_value = 2u; /* valid: ecu_param_count() == 3 */

    ( void )gauge_init( &pins, NEXT_ID, ACTION_ID );

    test_restores_saved_selection_on_first_poll();
    test_brief_press_below_debounce_does_nothing();
    test_release_before_threshold_cancels();
    test_hold_past_threshold_triggers_clear();
    test_held_button_does_not_retrigger_while_clearing();
    test_dismissing_the_result_does_not_retrigger();
    test_next_tap_advances_on_release_without_saving();
    test_next_hold_saves_without_advancing();
    test_next_hold_of_saved_selection_skips_the_write();
    test_next_hold_is_held_once();
    test_next_wraps_around();
    test_next_tap_across_a_stall_still_counts();
    test_next_blocked_during_clear();
    test_a_different_ecu_returns_to_the_first_page();

    return MT_RESULT();
}
