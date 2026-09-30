/* Test the button gestures on their own: only the GPIO edges are faked,
 * so there is no display, ECU or NVM here. */
#include "gauge_input.h"
#include "hal_gpio.h"
#include "hal_log.h"
#include "minitest.h"

#define NEXT_ID   ( ( hal_gpio_id_t )1u )
#define ACTION_ID ( ( hal_gpio_id_t )2u )

/* One pending edge per button, taken by the next gauge_input_poll. */
typedef struct
{
    bool level;
    bool pending;
    uint32_t at;
} edge_t;

static edge_t next_edge;
static edge_t action_edge;
static uint32_t now;

void hal_log_write( const char * text )
{
    ( void )text;
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

bool hal_gpio_take_edge(
    hal_gpio_id_t id,
    bool * level,
    uint32_t * timestamp_ms
)
{
    edge_t * e = ( id == ACTION_ID ) ? &action_edge : &next_edge;
    bool had   = e->pending;

    if ( had )
    {
        *level        = e->level;
        *timestamp_ms = e->at;
        e->pending    = false;
    }
    return had;
}

static void set_next( bool level )
{
    next_edge = ( edge_t ){ level, true, now };
}

static void set_action( bool level )
{
    action_edge = ( edge_t ){ level, true, now };
}

/* One loop pass: read the edges, then ask NEXT and ACTION what happened. */
static next_event_t next_ev;
static action_event_t action_ev;

static void pass(
    uint32_t delta_ms,
    bool blocked
)
{
    now += delta_ms;
    gauge_input_poll();
    next_ev   = gauge_input_next( now, blocked, true );
    action_ev = gauge_input_action( now );
}

static void test_next_tap_fires_on_release( void )
{
    set_next( true );
    pass( 40, false );
    CHECK_EQ( next_ev, NEXT_EVENT_NONE ); /* down: nothing yet */
    set_next( false );
    pass( 1, false );
    CHECK_EQ( next_ev, NEXT_EVENT_NONE ); /* release not settled */
    pass( 40, false );
    CHECK_EQ( next_ev, NEXT_EVENT_TAP );
}

static void test_next_hold_fires_once( void )
{
    set_next( true );
    pass( 40, false );
    pass( 1400, false );
    CHECK_EQ( next_ev, NEXT_EVENT_NONE );
    pass( 100, false );
    CHECK_EQ( next_ev, NEXT_EVENT_HOLD );
    pass( 3000, false );
    CHECK_EQ( next_ev, NEXT_EVENT_NONE ); /* still held: not again */
    set_next( false );
    pass( 1, false );
    pass( 40, false );
    CHECK_EQ( next_ev, NEXT_EVENT_NONE ); /* release after a hold is inert */
}

static void test_next_is_dropped_when_blocked( void )
{
    set_next( true );
    pass( 40, false ); /* armed */
    pass( 1, true );   /* the UI became busy: the press is dropped */
    set_next( false );
    pass( 1, false );
    pass( 40, false );
    CHECK_EQ( next_ev, NEXT_EVENT_NONE );
}

static void test_action_tap( void )
{
    set_action( true );
    pass( 40, false );
    CHECK_EQ( action_ev, ACTION_EVENT_PRESS );
    set_action( false );
    pass( 1, false );
    CHECK_EQ( action_ev, ACTION_EVENT_TAP );
}

static void test_action_hold_then_abort( void )
{
    set_action( true );
    pass( 40, false ); /* press */
    pass( 500, false );
    CHECK_EQ( action_ev, ACTION_EVENT_HOLD );
    set_action( false );
    pass( 1, false );
    CHECK_EQ( action_ev, ACTION_EVENT_ABORT );
}

static void test_action_hold_to_clear_then_quiet( void )
{
    set_action( true );
    pass( 40, false );
    pass( 500, false ); /* HOLD */
    pass( 1500, false );
    CHECK_EQ( action_ev, ACTION_EVENT_CLEAR );
    pass( 100, false );
    CHECK_EQ( action_ev, ACTION_EVENT_NONE ); /* quiet until reset */
    CHECK( gauge_input_action_down() );
    CHECK( gauge_input_action_settled( now ) );

    set_action( false );
    pass( 1, false );
    CHECK( !gauge_input_action_down() );
    gauge_input_action_reset();
    set_action( true );
    pass( 40, false );
    CHECK_EQ( action_ev, ACTION_EVENT_PRESS ); /* listening again */
    set_action( false );
    pass( 1, false );
}

static void test_reset_while_held_swallows_that_press( void )
{
    /* Held to the clear, then reset while still down, as dismissing the
     * result does. The rest of that press must do nothing. */
    set_action( true );
    pass( 40, false );
    pass( 500, false );
    pass( 1500, false );
    CHECK_EQ( action_ev, ACTION_EVENT_CLEAR );
    gauge_input_action_reset();
    pass( 100, false );
    CHECK_EQ( action_ev, ACTION_EVENT_NONE ); /* not a new PRESS */

    set_action( false );
    pass( 1, false );
    pass( 40, false );
    CHECK_EQ( action_ev, ACTION_EVENT_NONE ); /* and no TAP on release */

    set_action( true );
    pass( 40, false );
    CHECK_EQ( action_ev, ACTION_EVENT_PRESS ); /* a fresh press works */
    set_action( false );
    pass( 1, false );
    CHECK_EQ( action_ev, ACTION_EVENT_TAP );
}

int main( void )
{
    gauge_input_init( NEXT_ID, ACTION_ID );

    test_next_tap_fires_on_release();
    test_next_hold_fires_once();
    test_next_is_dropped_when_blocked();
    test_action_tap();
    test_action_hold_then_abort();
    test_action_hold_to_clear_then_quiet();
    test_reset_while_held_swallows_that_press();

    return MT_RESULT();
}
