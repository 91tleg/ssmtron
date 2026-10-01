/* Test the power-on sweep: the aim moves steadily from rest to full scale, to
 * the bottom and back to rest, at one full scale per second, then the live
 * reading takes over, once per power-up. */
#include <string.h>

#include "app/gauge_sweep.h"
#include "test/minitest.h"

/* A scale of -30 to 20, like the boost dial, in plain units: 50 across. */
static const dial_cfg_t cfg = {
    -30,
    20,
    10,
    4u,
    1,
    "TURBO",
    "PSI",
    "INHG",
    0u,
    0u,
    0u,
    900u,
    DIAL_STYLE_FULL_SWEEP
};

/* Each leg ends one poll after its time is up, since the dial is given the
 * end first, so times below allow for a poll or two. */
#define NEAR( value, want, slack )                                             \
    ( ( ( value ) >= ( ( want ) - ( slack ) ) )                                \
      && ( ( value ) <= ( ( want ) + ( slack ) ) ) )

static uint32_t clock_ms;
static int32_t aim;
static bool active;

/* Polls every 10 ms until the clock reaches t. The needle is taken to have
 * arrived whenever settled says so. */
static void run_to(
    const dial_cfg_t * c,
    uint32_t t,
    bool settled
)
{
    while ( clock_ms < t )
    {
        clock_ms += 10u;
        active = gauge_sweep_target( c, settled, clock_ms, &aim );
    }
}

static void test_it_moves_steadily_through_top_bottom_and_rest( void )
{
    /* The first poll starts at rest, which is zero on a scale that crosses
     * it. */
    active = gauge_sweep_target( &cfg, true, clock_ms, &aim );
    CHECK( active );
    CHECK_EQ( aim, 0 );

    /* Rest to the top is 20 of 50: 400 ms, a steady climb. */
    run_to( &cfg, 200u, true );
    CHECK( NEAR( aim, 10, 1 ) );
    run_to( &cfg, 400u, true );
    CHECK_EQ( aim, 20 );

    /* The top to the bottom is 50 of 50: a whole second, steadily down. */
    run_to( &cfg, 910u, true );
    CHECK( NEAR( aim, -5, 2 ) );
    run_to( &cfg, 1410u, true );
    CHECK_EQ( aim, -30 );

    /* The bottom back to rest is 30 of 50: 600 ms. */
    run_to( &cfg, 1720u, true );
    CHECK( NEAR( aim, -15, 2 ) );
    run_to( &cfg, 2020u, true );
    CHECK_EQ( aim, 0 ); /* the dial is told it, then it lets go */
    CHECK( active );
    run_to( &cfg, 2040u, true );
    CHECK( !active );
}

static void test_it_waits_for_the_needle_at_each_end( void )
{
    /* The needle is not keeping up: the aim stops at the top and holds. */
    run_to( &cfg, 700u, false );
    CHECK( active );
    CHECK_EQ( aim, 20 );
    run_to( &cfg, 1500u, false );
    CHECK_EQ( aim, 20 ); /* still there: the next leg has not started */

    /* Once it arrives the descent begins from the top. */
    run_to( &cfg, 1520u, true );
    run_to( &cfg, 1760u, true );
    CHECK( aim < 20 );
    CHECK( aim > -10 );
}

static void test_it_happens_only_once( void )
{
    run_to( &cfg, 20000u, true );
    CHECK( !active );
    run_to( &cfg, 20100u, false );
    CHECK( !active );
}

/* A loop that stalls does not skip the aim ahead: one poll counts for at most
 * 100 ms. */
static void test_a_stalled_loop_does_not_skip_ahead( void )
{
    active   = gauge_sweep_target( &cfg, true, 0u, &aim );
    clock_ms = 0u;
    active   = gauge_sweep_target( &cfg, true, 60000u, &aim ); /* a minute */
    CHECK( active );
    CHECK( aim >= 0 );
    CHECK( aim <= 5 ); /* 100 ms of the climb is 2.5 */
}

/* A page that is not a dial came first: there is no sweep, now or later. */
static void test_cancelled_before_it_starts_never_sweeps( void )
{
    gauge_sweep_cancel();
    CHECK( !gauge_sweep_target( &cfg, true, 10u, &aim ) );
    CHECK( !gauge_sweep_target( &cfg, false, 20u, &aim ) );
}

/* A scale from zero up, like the tachometer: the bottom is also rest, so the
 * way back is the same. A second a way, two seconds in all. */
static void test_a_scale_from_zero_goes_up_and_down( void )
{
    static const dial_cfg_t tach = {
        0,
        8000,
        1000,
        4u,
        1000,
        "RPM",
        "x1000",
        NULL,
        0u,
        0u,
        0u,
        1200u,
        DIAL_STYLE_FULL_SWEEP
    };

    active = gauge_sweep_target( &tach, true, clock_ms, &aim );
    CHECK_EQ( aim, 0 );
    run_to( &tach, 500u, true );
    CHECK( NEAR( aim, 4000, 100 ) );
    run_to( &tach, 1000u, true );
    CHECK_EQ( aim, 8000 );
    run_to( &tach, 1510u, true );
    CHECK( NEAR( aim, 4000, 160 ) );
    run_to( &tach, 2020u, true );
    CHECK_EQ( aim, 0 );
    run_to( &tach, 2060u, true );
    CHECK( !active );
}

/* A scale that starts above zero, like the coolant arc, rests at its bottom:
 * up a second, down a second. */
static void test_a_raised_scale_rests_at_its_bottom( void )
{
    static const dial_cfg_t arc = {
        1000,
        2600,
        400,
        1u,
        10,
        "COOLANT",
        "F",
        NULL,
        0u,
        0u,
        0u,
        900u,
        DIAL_STYLE_UPPER_ARC
    };

    active = gauge_sweep_target( &arc, true, clock_ms, &aim );
    CHECK_EQ( aim, 1000 );
    run_to( &arc, 1000u, true );
    CHECK_EQ( aim, 2600 );
    run_to( &arc, 2010u, true );
    CHECK_EQ( aim, 1000 );
    run_to( &arc, 2050u, true );
    CHECK( !active );
}

/* The sweep state lasts for the whole run, like a power-up, so each case is
 * its own run: "sweep" (the default), "wait", "stall", "zero",
 * "raised" or "cancel". */
int main(
    int argc,
    char ** argv
)
{
    const char * which = ( argc > 1 ) ? argv[1] : "sweep";

    if ( strcmp( which, "cancel" ) == 0 )
    {
        test_cancelled_before_it_starts_never_sweeps();
    }
    else if ( strcmp( which, "zero" ) == 0 )
    {
        test_a_scale_from_zero_goes_up_and_down();
    }
    else if ( strcmp( which, "raised" ) == 0 )
    {
        test_a_raised_scale_rests_at_its_bottom();
    }
    else if ( strcmp( which, "wait" ) == 0 )
    {
        active = gauge_sweep_target( &cfg, true, clock_ms, &aim );
        test_it_waits_for_the_needle_at_each_end();
    }
    else if ( strcmp( which, "stall" ) == 0 )
    {
        test_a_stalled_loop_does_not_skip_ahead();
    }
    else
    {
        test_it_moves_steadily_through_top_bottom_and_rest();
        test_it_happens_only_once();
    }
    return MT_RESULT();
}
