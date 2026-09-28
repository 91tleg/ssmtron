/* gauge.c's ACTION-button state machine (hold-to-clear), tested against
 * fake ECU/HAL/NVM. ui_init/ui_poll are no-ops here: only the button and
 * clear-trigger logic is under test, not rendering.
 *
 * gauge_init runs once, as in production; it does not reset the
 * debounce/hold state (there is nothing to reset it for at boot), so this
 * is one continuous timeline rather than independently-reset test cases,
 * each phase picking up in the state the previous one actually left. */
#include "ecu.h"
#include "gauge.h"
#include "hal_gpio.h"
#include "hal_log.h"
#include "hal_nvm.h"
#include "minitest.h"
#include "ui.h"

#define NEXT_ID   ( ( hal_gpio_id_t )1u )
#define ACTION_ID ( ( hal_gpio_id_t )2u )

static bool next_pressed;
static bool action_pressed;
static bool clear_started;
static ecu_clear_t clear_result = ECU_CLEAR_BUSY;

size_t ecu_param_count( void )
{
    return 1u;
}

const ecu_param_info_t * ecu_param_info( size_t idx )
{
    static const ecu_param_info_t info = { "RPM", "rpm", NULL, ECU_DEC_LINEAR,
                                            1,     1,     0,    0,
                                            NULL,  NULL,  0,    false,
                                            0,     0 };

    return ( idx == 0u ) ? &info : NULL;
}

bool ecu_select_param( size_t idx )
{
    ( void )idx;
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

void hal_log_write( const char * text )
{
    ( void )text;
}

hal_status_t hal_nvm_save(
    const char * key,
    const void * data,
    size_t len
)
{
    ( void )key;
    ( void )data;
    ( void )len;
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
    return HAL_ERR_INVALID; /* nothing stored: parameter 0 stays selected */
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

bool hal_gpio_read( hal_gpio_id_t id )
{
    return ( id == ACTION_ID ) ? action_pressed : next_pressed;
}

hal_status_t display_init( const display_pins_t * pins )
{
    ( void )pins;
    return HAL_OK;
}

void ui_init(
    ui_widget_t * widgets,
    size_t count,
    uint16_t page_bg
)
{
    ( void )widgets;
    ( void )count;
    ( void )page_bg;
}

void ui_poll(
    ui_widget_t * widgets,
    size_t count
)
{
    ( void )widgets;
    ( void )count;
}

static uint32_t now;

static void tick( uint32_t delta_ms )
{
    now += delta_ms;
    gauge_poll( now );
}

static void test_brief_press_below_debounce_does_nothing( void )
{
    action_pressed = true;
    tick( 5 ); /* raw goes true, but not stable long enough yet */
    action_pressed = false;
    tick( 5 );
    CHECK( !clear_started );
}

static void test_release_before_threshold_cancels( void )
{
    action_pressed = true;
    tick( 1 );  /* raw settles, debounce window starts */
    tick( 35 ); /* past debounce: enters HOLDING */
    tick( 500 ); /* still short of the 2 s hold */
    action_pressed = false;
    tick( 1 );    /* released early: back to idle, no clear */
    tick( 2000 ); /* long after, still idle */
    CHECK( !clear_started );
}

static void test_hold_past_threshold_triggers_clear( void )
{
    action_pressed = true;
    tick( 1 );
    tick( 35 );   /* HOLDING starts */
    tick( 1965 ); /* crosses the 2 s hold */
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
    action_pressed = false;
    tick( 1 ); /* release seen */
    action_pressed = true;
    tick( 1 );  /* a fresh press starts */
    tick( 35 ); /* past debounce: dismisses the result, not a new clear */
    CHECK( !clear_started );
    action_pressed = false;
    tick( 1 ); /* leave the button released for anything that follows */
}

int main( void )
{
    static const display_pins_t pins = { 0 };

    ( void )gauge_init( &pins, NEXT_ID, ACTION_ID );

    test_brief_press_below_debounce_does_nothing();
    test_release_before_threshold_cancels();
    test_hold_past_threshold_triggers_clear();
    test_held_button_does_not_retrigger_while_clearing();
    test_dismissing_the_result_does_not_retrigger();

    return MT_RESULT();
}
