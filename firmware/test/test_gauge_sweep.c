/* Test the power-on sweep's sequence: full scale first, then the live
 * reading takes over, once per power-up. */
#include <string.h>

#include "app/gauge_sweep.h"
#include "test/minitest.h"

/* A scale of -30 to 20, like the boost dial. */
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

static void test_it_aims_at_full_scale_then_hands_over( void )
{
    int32_t value = 0;

    /* The first poll starts the sweep even though a new dial looks settled. */
    CHECK( gauge_sweep_target( &cfg, true, &value ) );
    CHECK_EQ( value, 20 );

    /* Still travelling: it keeps aiming at full scale. */
    value = 0;
    CHECK( gauge_sweep_target( &cfg, false, &value ) );
    CHECK_EQ( value, 20 );

    /* At full scale: it lets go, and the needle returns to the reading. */
    CHECK( !gauge_sweep_target( &cfg, true, &value ) );
}

static void test_it_happens_only_once( void )
{
    int32_t value = 0;

    CHECK( !gauge_sweep_target( &cfg, false, &value ) );
    CHECK( !gauge_sweep_target( &cfg, true, &value ) );
}

/* A page that is not a dial came first: there is no sweep, now or later. */
static void test_cancelled_before_it_starts_never_sweeps( void )
{
    int32_t value = 0;

    gauge_sweep_cancel();
    CHECK( !gauge_sweep_target( &cfg, true, &value ) );
    CHECK( !gauge_sweep_target( &cfg, false, &value ) );
}

/* The sweep state lasts for the whole run, like a power-up, so each case is
 * its own run: "sweep" (the default) or "cancel". */
int main(
    int argc,
    char ** argv
)
{
    if ( ( argc > 1 ) && ( strcmp( argv[1], "cancel" ) == 0 ) )
    {
        test_cancelled_before_it_starts_never_sweeps();
    }
    else
    {
        test_it_aims_at_full_scale_then_hands_over();
        test_it_happens_only_once();
    }
    return MT_RESULT();
}
