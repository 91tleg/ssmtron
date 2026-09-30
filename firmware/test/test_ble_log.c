/* App glue: ECU values -> BLE lines, with fakes for both sides. */
#include <string.h>

#include "app/ble_log.h"
#include "lib/ecu/ecu.h"
#include "hal/hal_ble.h"
#include "test/minitest.h"

static const ecu_param_info_t infos[2] = {
    { .name           = "Engine Speed",
      .unit           = "rpm",
      .unit_neg       = NULL,
      .decode         = ECU_DEC_LINEAR,
      .scale_num      = 25,
      .scale_den      = 1,
      .offset         = 0,
      .decimals       = 0,
      .lookup         = NULL,
      .flags          = NULL,
      .flag_count     = 0,
      .has_thresholds = false,
      .warn_value     = 0,
      .redline_value  = 0,
      .trouble_codes  = false },
    { .name           = "System Voltage",
      .unit           = "V",
      .unit_neg       = NULL,
      .decode         = ECU_DEC_LINEAR,
      .scale_num      = 8,
      .scale_den      = 1,
      .offset         = 0,
      .decimals       = 2,
      .lookup         = NULL,
      .flags          = NULL,
      .flag_count     = 0,
      .has_thresholds = false,
      .warn_value     = 0,
      .redline_value  = 0,
      .trouble_codes  = false },
};
static bool fresh[2];
static int32_t vals[2];
static size_t count = 2;

size_t ecu_param_count( void )
{
    return count;
}

const ecu_param_info_t * ecu_param_info( size_t idx )
{
    return ( idx < 2 ) ? &infos[idx] : NULL;
}

bool ecu_get_value(
    size_t idx,
    int32_t * out
)
{
    bool ok = ( idx < 2 ) && fresh[idx];

    if ( ok )
    {
        *out = vals[idx];
    }
    return ok;
}

static bool ready;
static char sent[512];
static size_t sent_len;
static unsigned writes;
static size_t accept_limit;

bool hal_ble_ready( void )
{
    return ready;
}

size_t hal_ble_write(
    const uint8_t * buf,
    size_t len
)
{
    size_t n = ( len < accept_limit ) ? len : accept_limit;

    memcpy( sent + sent_len, buf, n );
    sent_len += n;
    sent[sent_len] = '\0';
    writes++;
    return n;
}

/* The module keeps state between calls, so tests use disjoint time ranges.
 */
static uint32_t t0;

static void reset( void )
{
    ready        = true;
    sent_len     = 0;
    sent[0]      = '\0';
    writes       = 0;
    accept_limit = 1000;
    fresh[0]     = false;
    fresh[1]     = false;
    t0 += 100000;
}

static void test_sends_a_csv_line( void )
{
    reset();
    fresh[0] = true;
    vals[0]  = 1000;
    ble_log_poll( t0 );
    CHECK_EQ( writes, 1 );
    CHECK( strcmp( sent, "" ) != 0 );
    CHECK( strstr( sent, ",Engine Speed,1000,rpm\n" ) != NULL );
}

static void test_picks_the_fresh_parameter( void )
{
    reset();
    fresh[1] = true;
    vals[1]  = 1400;
    ble_log_poll( t0 );
    CHECK( strstr( sent, ",System Voltage,14.00,V\n" ) != NULL );
}

static void test_rate_limited( void )
{
    reset();
    fresh[0] = true;
    ble_log_poll( t0 );
    ble_log_poll( t0 + 1 );
    ble_log_poll( t0 + BLE_LOG_PERIOD_MS - 1 );
    CHECK_EQ( writes, 1 );
    ble_log_poll( t0 + BLE_LOG_PERIOD_MS );
    CHECK_EQ( writes, 2 );
}

static void test_quiet_when_nobody_listens( void )
{
    reset();
    fresh[0] = true;
    ready    = false;
    ble_log_poll( t0 );
    ble_log_poll( t0 + 1000 );
    CHECK_EQ( writes, 0 );
    ready = true; /* and nothing was owed for the missed time */
    ble_log_poll( t0 + 1001 );
    CHECK_EQ( writes, 1 );
}

static void test_quiet_without_a_fresh_value( void )
{
    reset();
    ble_log_poll( t0 );
    CHECK_EQ( writes, 0 );
    count    = 0;
    fresh[0] = true;
    ble_log_poll( t0 + 1000 );
    CHECK_EQ( writes, 0 ); /* no parameters known yet */
    count = 2;
}

static void test_full_queue_drops_the_line( void )
{
    reset();
    fresh[0]     = true;
    accept_limit = 0;
    ble_log_poll( t0 );
    CHECK_EQ( writes, 1 );
    accept_limit = 1000; /* not retried in a loop: next line is a period on */
    ble_log_poll( t0 + 1 );
    CHECK_EQ( writes, 1 );
    ble_log_poll( t0 + BLE_LOG_PERIOD_MS );
    CHECK_EQ( writes, 2 );
    CHECK( strstr( sent, ",Engine Speed," ) != NULL );
}

static void test_wraparound( void )
{
    reset();
    fresh[0] = true;
    ble_log_poll( 0xFFFFFF00u );
    ble_log_poll( 0xFFFFFF00u + BLE_LOG_PERIOD_MS ); /* wraps */
    CHECK_EQ( writes, 2 );
}

int main( void )
{
    test_sends_a_csv_line();
    test_picks_the_fresh_parameter();
    test_rate_limited();
    test_quiet_when_nobody_listens();
    test_quiet_without_a_fresh_value();
    test_full_queue_drops_the_line();
    test_wraparound();

    return MT_RESULT();
}
