/* Test the button gestures on their own: only the GPIO edges are faked,
 * so there is no display, ECU or NVM here. */
#include "gauge_input.h"
#include "hal_gpio.h"
#include "hal_log.h"
#include "minitest.h"

#define NEXT_ID   ( ( hal_gpio_id_t )1u )
#define ACTION_ID ( ( hal_gpio_id_t )2u )

/* The same latch as the real HAL (hal_gpio.c): the latest edge, and the
 * latest press kept apart, so the press is reported first and then the
 * release after it. Repeated presses and releases between two polls
 * collapse to the latest press and the latest edge. */
typedef struct
{
    bool level;
    bool pending;
    bool press_pending;
    uint32_t at;
    uint32_t press_at;
} latch_t;

static latch_t next_edges;
static latch_t action_edges;
static uint32_t now;

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

void hal_log_write( const char * text )
{
    ( void )text;
}

bool hal_gpio_take_edge(
    hal_gpio_id_t id,
    bool * level,
    uint32_t * timestamp_ms
)
{
    latch_t * l = ( id == ACTION_ID ) ? &action_edges : &next_edges;
    bool had    = false;

    if ( l->press_pending )
    {
        *level           = true;
        *timestamp_ms    = l->press_at;
        l->press_pending = false;
        had              = true;
        if ( l->level )
        {
            l->pending = false;
        }
    }
    else if ( l->pending )
    {
        *level        = l->level;
        *timestamp_ms = l->at;
        l->pending    = false;
        had           = true;
    }
    return had;
}

/* An edge as the interrupt handler latches it. */
static void push_edge(
    latch_t * l,
    bool level,
    uint32_t at
)
{
    l->level   = level;
    l->at      = at;
    l->pending = true;
    if ( level )
    {
        l->press_pending = true;
        l->press_at      = at;
    }
}

static void set_next( bool level )
{
    push_edge( &next_edges, level, now );
}

static void set_action( bool level )
{
    push_edge( &action_edges, level, now );
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
    CHECK_EQ( action_ev, ACTION_EVENT_NONE ); /* release not settled */
    pass( 40, false );
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
    CHECK_EQ( action_ev, ACTION_EVENT_NONE ); /* release not settled */
    pass( 40, false );
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
    pass( 40, false );
    CHECK_EQ( action_ev, ACTION_EVENT_TAP );
}

/* Contact bounce on the release of a held ACTION must not end the hold. */
static void test_action_release_bounce_does_not_abort_a_hold( void )
{
    set_action( true );
    pass( 40, false );
    pass( 500, false );
    CHECK_EQ( action_ev, ACTION_EVENT_HOLD );
    set_action( false ); /* the contact opens for a moment... */
    pass( 2, false );
    CHECK_EQ( action_ev, ACTION_EVENT_NONE );
    set_action( true ); /* ...and closes again */
    pass( 2, false );
    CHECK_EQ( action_ev, ACTION_EVENT_NONE );
    pass( 1500, false );
    CHECK_EQ( action_ev, ACTION_EVENT_CLEAR ); /* the press carried on */
    gauge_input_action_reset();
    set_action( false );
    pass( 1, false );
    pass( 40, false );
}

/* The same bounce during a short press must not turn it into a tap. */
static void test_action_release_bounce_does_not_tap_early( void )
{
    set_action( true );
    pass( 40, false );
    CHECK_EQ( action_ev, ACTION_EVENT_PRESS );
    set_action( false );
    pass( 2, false );
    set_action( true );
    pass( 2, false );
    CHECK_EQ( action_ev, ACTION_EVENT_NONE ); /* still one press */
    pass( 100, false );
    CHECK_EQ( action_ev, ACTION_EVENT_NONE );
    set_action( false );
    pass( 1, false );
    pass( 40, false );
    CHECK_EQ( action_ev, ACTION_EVENT_TAP ); /* the real release */
}

/* A 2 ms pulse that the loop only gets to after 100 ms is still a 2 ms
 * pulse: the edges carry their own times. */
static void test_a_late_glitch_is_not_a_press( void )
{
    uint32_t t = now;

    push_edge( &action_edges, true, t );
    push_edge( &action_edges, false, t + 2u );
    push_edge( &next_edges, true, t );
    push_edge( &next_edges, false, t + 2u );
    pass( 100, false );
    CHECK_EQ( action_ev, ACTION_EVENT_NONE );
    CHECK_EQ( next_ev, NEXT_EVENT_NONE );
    pass( 100, false );
    CHECK_EQ( action_ev, ACTION_EVENT_NONE );
    CHECK_EQ( next_ev, NEXT_EVENT_NONE );
    CHECK( !gauge_input_action_down() );
}

/* A real short press, 50 ms, also reaching a slow loop late, still counts. */
static void test_a_late_real_tap_still_counts( void )
{
    uint32_t t = now;

    push_edge( &action_edges, true, t );
    push_edge( &action_edges, false, t + 50u );
    push_edge( &next_edges, true, t );
    push_edge( &next_edges, false, t + 50u );
    pass( 200, false );
    CHECK_EQ( action_ev, ACTION_EVENT_PRESS );
    pass( 1, false );
    CHECK_EQ( action_ev, ACTION_EVENT_TAP );
    CHECK_EQ( next_ev, NEXT_EVENT_TAP );
}

/* If the loop stalls past the hold time and the button was let go after it,
 * the release is a hold, not a tap that would turn the page. */
static void test_a_stalled_loop_still_sees_a_next_hold( void )
{
    uint32_t t = now;

    set_next( true );
    pass( 40, false ); /* the press is armed */
    CHECK_EQ( next_ev, NEXT_EVENT_NONE );
    push_edge( &next_edges, false, t + 1600u ); /* let go after 1.6 s */
    now += 2000u;                               /* the loop was stalled */
    pass( 0, false );
    CHECK_EQ( next_ev, NEXT_EVENT_HOLD );
    pass( 40, false );
    CHECK_EQ( next_ev, NEXT_EVENT_NONE ); /* and nothing more */
}

/* Let go just before the hold time, even if noticed late, is a tap. */
static void test_a_late_release_under_the_hold_time_is_a_tap( void )
{
    uint32_t t = now;

    set_next( true );
    pass( 40, false );
    push_edge( &next_edges, false, t + 1400u );
    now += 2000u;
    pass( 0, false );
    CHECK_EQ( next_ev, NEXT_EVENT_TAP );
}

/* A release that was already read, and is only told one poll later, ends the
 * press where it happened. A slow loop must not count the time after it as
 * still held: that would start a hold the user never made. */
static void test_a_deferred_release_does_not_start_an_action_hold( void )
{
    uint32_t t = now;

    set_action( true );
    pass( 40, false );
    CHECK_EQ( action_ev, ACTION_EVENT_PRESS );
    /* A bounce, then the real release 200 ms in, all before the next poll. */
    push_edge( &action_edges, false, t + 100u );
    push_edge( &action_edges, true, t + 110u );
    push_edge( &action_edges, false, t + 200u );
    now = t + 700u; /* the loop stalled past the 500 ms tap time */
    pass( 0, false );
    CHECK_EQ( action_ev, ACTION_EVENT_NONE ); /* not a hold */
    pass( 1, false );
    CHECK_EQ( action_ev, ACTION_EVENT_TAP ); /* it was a 200 ms tap */
}

static void test_a_deferred_release_does_not_start_a_next_hold( void )
{
    uint32_t t = now;

    set_next( true );
    pass( 40, false );
    CHECK_EQ( next_ev, NEXT_EVENT_NONE );
    push_edge( &next_edges, false, t + 100u );
    push_edge( &next_edges, true, t + 110u );
    push_edge( &next_edges, false, t + 200u );
    now = t + 1700u; /* past the 1.5 s hold time */
    pass( 0, false );
    CHECK_EQ( next_ev, NEXT_EVENT_NONE ); /* not a save */
    pass( 1, false );
    CHECK_EQ( next_ev, NEXT_EVENT_TAP );
}

static void test_a_deferred_release_does_not_start_a_clear( void )
{
    uint32_t t = now;

    set_action( true );
    pass( 40, false );
    pass( 500, false );
    CHECK_EQ( action_ev, ACTION_EVENT_HOLD );
    push_edge( &action_edges, false, t + 700u );
    push_edge( &action_edges, true, t + 710u );
    push_edge( &action_edges, false, t + 1000u );
    now = t + 2500u; /* past the 2 s clear time */
    pass( 0, false );
    CHECK_EQ( action_ev, ACTION_EVENT_NONE ); /* not a clear */
    pass( 1, false );
    CHECK_EQ( action_ev, ACTION_EVENT_ABORT );
}

/* Repeated edges between two polls collapse as in the real HAL: a press, a
 * bounce and another press read as one press, timed from the latest. */
static void test_repeated_presses_collapse_to_one( void )
{
    uint32_t t = now;

    push_edge( &action_edges, true, t );
    push_edge( &action_edges, false, t + 5u );
    push_edge( &action_edges, true, t + 10u );
    pass( 40, false );
    CHECK_EQ( action_ev, ACTION_EVENT_PRESS );
    pass( 1, false );
    CHECK_EQ( action_ev, ACTION_EVENT_NONE ); /* one press, not two */
    set_action( false );
    pass( 1, false );
    pass( 40, false );
    CHECK_EQ( action_ev, ACTION_EVENT_TAP );
}

/* A release that is only told one poll later must still be checked against
 * what came after it: here the contact closes again 11 ms after it opened,
 * between the poll that read the release and the poll that tells it. The
 * release never was, and the press carries on. */
static void test_a_bounce_after_a_deferred_release_is_not_a_release( void )
{
    uint32_t t = now;

    /* ACTION: down at 0, up at 50, both read by the poll at 60. */
    push_edge( &action_edges, true, t );
    push_edge( &action_edges, false, t + 50u );
    push_edge( &next_edges, true, t );
    push_edge( &next_edges, false, t + 50u );
    now = t + 60u;
    pass( 0, false );
    CHECK_EQ( action_ev, ACTION_EVENT_PRESS );
    /* The contact bounces closed at 61 ms, before the next poll. */
    push_edge( &action_edges, true, t + 61u );
    push_edge( &next_edges, true, t + 61u );
    now = t + 100u;
    pass( 0, false );
    CHECK_EQ( action_ev, ACTION_EVENT_NONE ); /* no early tap */
    CHECK_EQ( next_ev, NEXT_EVENT_NONE );     /* no early page change */
    CHECK( gauge_input_action_down() );
    pass( 60, false );
    CHECK_EQ( action_ev, ACTION_EVENT_NONE );
    CHECK_EQ( next_ev, NEXT_EVENT_NONE );

    /* The real release comes later and is a tap as usual. */
    set_action( false );
    set_next( false );
    pass( 1, false );
    pass( 40, false );
    CHECK_EQ( action_ev, ACTION_EVENT_TAP );
    CHECK_EQ( next_ev, NEXT_EVENT_TAP );
}

/* Pressed again after a gap longer than the debounce time, the release
 * before it was real: it is told first, and the new press after it. */
static void test_a_real_press_after_a_deferred_release_comes_second( void )
{
    uint32_t t = now;

    push_edge( &action_edges, true, t );
    push_edge( &action_edges, false, t + 50u );
    now = t + 60u;
    pass( 0, false );
    CHECK_EQ( action_ev, ACTION_EVENT_PRESS );
    push_edge( &action_edges, true, t + 120u ); /* 70 ms after the release */
    now = t + 130u;
    pass( 0, false );
    CHECK_EQ( action_ev, ACTION_EVENT_TAP ); /* the first press ended */
    pass( 40, false );
    CHECK_EQ( action_ev, ACTION_EVENT_PRESS ); /* then the new one began */
    set_action( false );
    pass( 1, false );
    pass( 40, false );
    CHECK_EQ( action_ev, ACTION_EVENT_TAP );
}

/* Both newer edges can already be waiting when a deferred release is told:
 * the contact closes again and then really opens. The release after the
 * bounce ends the press, so the time since must not count as held. */
static void test_a_bounce_and_its_release_do_not_start_a_clear( void )
{
    uint32_t t = now;

    set_action( true );
    pass( 40, false );
    pass( 500, false );
    CHECK_EQ( action_ev, ACTION_EVENT_HOLD );
    /* A bounce closed at 1900 and the real release at 1950, read at 1960. */
    push_edge( &action_edges, true, t + 1900u );
    push_edge( &action_edges, false, t + 1950u );
    now = t + 1960u;
    pass( 0, false );
    CHECK_EQ( action_ev, ACTION_EVENT_NONE );
    /* The contact closes again at 1961 and opens at 1980, before the poll
     * at 2010 that would tell the release at 1950. */
    push_edge( &action_edges, true, t + 1961u );
    push_edge( &action_edges, false, t + 1980u );
    now = t + 2010u;
    pass( 0, false );
    CHECK( action_ev != ACTION_EVENT_CLEAR ); /* released before 2 s */
    CHECK_EQ( action_ev, ACTION_EVENT_ABORT );
}

static void test_a_bounce_and_its_release_do_not_start_a_next_hold( void )
{
    uint32_t t = now;

    set_next( true );
    pass( 40, false );
    push_edge( &next_edges, true, t + 1400u );
    push_edge( &next_edges, false, t + 1450u );
    now = t + 1460u;
    pass( 0, false );
    CHECK_EQ( next_ev, NEXT_EVENT_NONE );
    push_edge( &next_edges, true, t + 1461u );
    push_edge( &next_edges, false, t + 1480u );
    now = t + 1510u;
    pass( 0, false );
    CHECK( next_ev != NEXT_EVENT_HOLD ); /* released before 1.5 s */
    CHECK_EQ( next_ev, NEXT_EVENT_TAP );
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
    test_action_release_bounce_does_not_abort_a_hold();
    test_action_release_bounce_does_not_tap_early();
    test_a_late_glitch_is_not_a_press();
    test_a_late_real_tap_still_counts();
    test_a_stalled_loop_still_sees_a_next_hold();
    test_a_late_release_under_the_hold_time_is_a_tap();
    test_a_deferred_release_does_not_start_an_action_hold();
    test_a_deferred_release_does_not_start_a_next_hold();
    test_a_deferred_release_does_not_start_a_clear();
    test_repeated_presses_collapse_to_one();
    test_a_bounce_after_a_deferred_release_is_not_a_release();
    test_a_real_press_after_a_deferred_release_comes_second();
    test_a_bounce_and_its_release_do_not_start_a_clear();
    test_a_bounce_and_its_release_do_not_start_a_next_hold();

    return MT_RESULT();
}
