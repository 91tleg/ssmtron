/* The real main.c: its startup order and its loop, run against the simulated
 * ECU. main.c is compiled under another name, and the fake delay in its loop
 * jumps out after a set time, so the test can look at what it did.
 *
 * Each mode is its own start of the program: "run" is the normal start, the
 * others break one thing at startup or in the loop. */
#include <setjmp.h>

#include "bsp.h"
#define NEXT_ID   BSP_GPIO_NEXT
#define ACTION_ID BSP_GPIO_ACTION
#include "hal/hal_time.h"
#include "hal/hal_wdt.h"
#include "test/sil/harness.h"

int app_main( void ); /* main.c, renamed by the build */

#define RUN_MS ( 12000u ) /* past the 10 s the image is confirmed after */

static jmp_buf loop_exit;
static unsigned passes;
static void ( *script )( uint32_t at_ms );

/* The watchdog. */
static unsigned wdt_starts;
static uint32_t wdt_timeout;
static unsigned wdt_feeds;
static hal_status_t wdt_result = HAL_OK;

hal_status_t hal_wdt_start( uint32_t timeout_ms )
{
    trace( "wdt start" );
    wdt_starts++;
    wdt_timeout = timeout_ms;
    return wdt_result;
}

void hal_wdt_feed( void )
{
    wdt_feeds++;
}

/* The image confirm. */
static unsigned confirm_calls;
static uint32_t confirm_first_ms;
static unsigned confirm_failures; /* calls that fail before one succeeds */
static hal_status_t confirm_result = HAL_OK;

hal_status_t hal_dfu_confirm( void )
{
    hal_status_t status = confirm_result;

    if ( confirm_calls == 0u )
    {
        confirm_first_ms = now;
    }
    if ( ( confirm_calls < confirm_failures ) && ( status == HAL_OK ) )
    {
        status = HAL_ERR_IO;
    }
    confirm_calls++;
    return status;
}

/* Time: the loop's sleep is one millisecond, so this is the loop's clock. */
uint32_t hal_millis( void )
{
    return now;
}

void hal_delay_ms( uint32_t ms )
{
    now += ms;
    sim_stream();
    passes++;
    if ( script != NULL )
    {
        script( now );
    }
    if ( now >= RUN_MS )
    {
        longjmp( loop_exit, 1 );
    }
}

static void run_main( void )
{
    sim_reset( ROM_KEY );
    if ( setjmp( loop_exit ) == 0 )
    {
        ( void )app_main();
    }
}

/* Where an event appears in the trace, or the count if it never does. */
static size_t trace_at( const char * what )
{
    size_t found = trace_count;

    for ( size_t i = 0u; ( found == trace_count ) && ( i < trace_count ); i++ )
    {
        if ( strcmp( trace_log[i], what ) == 0 )
        {
            found = i;
        }
    }
    return found;
}

static void test_a_normal_start( void )
{
    run_main();

    /* The watchdog is armed first, before the hardware, with the timeout the
     * startup was sized for. */
    CHECK_EQ( trace_at( "wdt start" ), 0u );
    CHECK_EQ( wdt_starts, 1u );
    CHECK_EQ( wdt_timeout, 4000u );
    CHECK( trace_at( "wdt start" ) < trace_at( "nvm init" ) );
    CHECK( trace_at( "nvm init" ) < trace_at( "uart open" ) );
    CHECK( trace_at( "uart open" ) < trace_at( "display init" ) );
    CHECK( trace_at( "display init" ) < trace_at( "display on" ) );

    /* The firmware version is logged first, and nothing went wrong. */
    CHECK( strcmp( log_lines[0], "sil" ) == 0 );
    CHECK( !logged( "wdt not armed" ) );
    CHECK( !logged( "display not ready" ) );

    /* The first frame existed when the panel turned on. */
    CHECK_EQ( on_frames, 1u );

    /* The loop runs: the watchdog is fed on every pass, the ECU is polled
     * and identifies, and the image is confirmed once, no sooner than 10 s. */
    CHECK( passes >= ( RUN_MS - 100u ) );
    CHECK( wdt_feeds >= passes );
    CHECK( tx_count > 0u );
    CHECK_EQ( ecu_rom_id(), 0x013671u );
    CHECK_EQ( confirm_calls, 1u );
    CHECK( confirm_first_ms >= 10000u );
    CHECK( confirm_first_ms <= 10010u );
}

static void test_no_display_means_no_confirm( void )
{
    display_init_result = HAL_ERR_IO;
    run_main();

    CHECK( logged( "display not ready" ) );
    CHECK_EQ( trace_at( "display on" ), trace_count ); /* never turned on */
    /* The unit still runs and talks to the ECU, but a unit that cannot show
     * anything is not a good image: it is left for the bootloader to
     * revert. */
    CHECK_EQ( ecu_rom_id(), 0x013671u );
    CHECK( wdt_feeds >= passes );
    CHECK_EQ( confirm_calls, 0u );
}

/* A press and release of NEXT at 1 s, to see whether the gauge still runs. */
static void tap_at_one_second( uint32_t at_ms )
{
    if ( at_ms == 1000u )
    {
        button( &next_edges, true );
    }
    if ( at_ms == 1100u )
    {
        button( &next_edges, false );
    }
}

static void test_an_ecu_uart_that_will_not_open_leaves_the_gauge_running( void )
{
    unsigned frames_before;

    uart_open_result = HAL_ERR_IO;
    script           = tap_at_one_second;
    run_main();
    frames_before = frames_drawn;

    CHECK_EQ( tx_count, 0u ); /* nothing was ever sent */
    CHECK_EQ( ecu_rom_id(), 0u );
    CHECK( on_frames >= 1u );     /* the first frame was drawn */
    CHECK( frames_before >= 2u ); /* and the tap changed the page */
    CHECK( wdt_feeds >= passes );
    CHECK_EQ( confirm_calls, 1u ); /* a car that is off is not a bad image */
}

static void test_a_confirm_that_fails_is_tried_again( void )
{
    confirm_failures = 3u;
    run_main();
    CHECK_EQ( confirm_calls, 4u ); /* three failures, then it took */
}

static void test_no_bootloader_means_nothing_to_confirm( void )
{
    confirm_result = HAL_ERR_UNSUPPORTED;
    run_main();
    CHECK_EQ( confirm_calls, 1u ); /* not asked again */
}

int main(
    int argc,
    char ** argv
)
{
    const char * which = ( argc > 1 ) ? argv[1] : "run";

    if ( strcmp( which, "nodisplay" ) == 0 )
    {
        test_no_display_means_no_confirm();
    }
    else if ( strcmp( which, "nouart" ) == 0 )
    {
        test_an_ecu_uart_that_will_not_open_leaves_the_gauge_running();
    }
    else if ( strcmp( which, "retry" ) == 0 )
    {
        test_a_confirm_that_fails_is_tried_again();
    }
    else if ( strcmp( which, "nobootloader" ) == 0 )
    {
        test_no_bootloader_means_nothing_to_confirm();
    }
    else
    {
        test_a_normal_start();
    }
    return MT_RESULT();
}
