/* Test update mode on its own: what it asks of Bluetooth, what the page says
 * and how the wait runs. The fakes stand in for Bluetooth, the random source,
 * the restart and the text engine; the widgets' own text callbacks are called
 * here to see what would be drawn. The state lasts for the whole run, so each
 * scenario is its own run, chosen by an argument. */
#include <string.h>

#include "app/gauge_update.h"
#include "hal/hal_ble.h"
#include "hal/hal_dfu.h"
#include "hal/hal_random.h"
#include "lib/display/display.h"
#include "lib/ui/text.h"
#include "test/minitest.h"

static unsigned ble_opens;
static char opened_name[32];
static uint32_t opened_passkey;
static hal_status_t ble_open_result = HAL_OK;
static hal_ble_evt_t events[4];
static size_t event_count;
static uint32_t random_value = 1234567u;
static bool random_ok        = true;
static unsigned reboots;

hal_status_t hal_ble_open( const hal_ble_cfg_t * cfg )
{
    ble_opens++;
    ( void )strncpy( opened_name, cfg->name, sizeof( opened_name ) - 1u );
    opened_passkey = cfg->passkey;
    return ble_open_result;
}

hal_ble_evt_t hal_ble_poll_event( void )
{
    hal_ble_evt_t evt = HAL_BLE_EVT_NONE;

    if ( event_count > 0u )
    {
        evt = events[0];
        for ( size_t i = 1; i < event_count; i++ )
        {
            events[i - 1u] = events[i];
        }
        event_count--;
    }
    return evt;
}

hal_status_t hal_random_u32( uint32_t * out )
{
    if ( random_ok )
    {
        *out = random_value;
    }
    return random_ok ? HAL_OK : HAL_ERR_IO;
}

void hal_dfu_reboot( void )
{
    reboots++;
}

static text_widget_t * page;
static size_t page_count;

void text_init(
    text_widget_t * widgets,
    size_t count,
    uint16_t page_bg
)
{
    ( void )page_bg;
    page       = widgets;
    page_count = count;
}

void text_poll(
    text_widget_t * widgets,
    size_t count
)
{
    ( void )widgets;
    ( void )count;
}

/* What widget i would draw right now. */
static const char * row( size_t i )
{
    static char buf[TEXT_MAX + 1u];

    page[i].text( buf );
    return buf;
}

static void send( hal_ble_evt_t evt )
{
    events[event_count] = evt;
    event_count++;
}

static void begin( uint32_t now )
{
    gauge_update_set_colors( 0x1111u, 0x2222u );
    gauge_update_open( 0x1111u );
    gauge_update_start( now );
    gauge_update_poll( now );
}

/* The usual session: start, wait, a phone comes and goes, the wait runs out. */
static void test_a_session( void )
{
    CHECK( !gauge_update_active() );
    begin( 1000u );

    /* Bluetooth starts with a passkey from the random source, reduced to six
     * digits, under the gauge's name. */
    CHECK( gauge_update_active() );
    CHECK_EQ( ble_opens, 1u );
    CHECK( strcmp( opened_name, "SSM1 Gauge" ) == 0 );
    CHECK_EQ( opened_passkey, 234567u );
    CHECK_EQ( page[1].bg, 0x1111u );
    CHECK_EQ( page[1].fg, 0x2222u );

    CHECK( strcmp( row( 0 ), "UPDATE MODE" ) == 0 );
    CHECK( strcmp( row( 1 ), "PASSKEY   234567" ) == 0 );
    CHECK( strcmp( row( 2 ), "WAITING FOR PHONE" ) == 0 );
    CHECK( strcmp( row( 3 ), "TIME LEFT 5:00" ) == 0 );
    CHECK( strcmp( row( 4 ), "NEXT TO EXIT" ) == 0 );

    /* Starting again does nothing: same passkey, no second open. */
    random_value = 7u;
    gauge_update_start( 2000u );
    CHECK_EQ( ble_opens, 1u );
    CHECK_EQ( opened_passkey, 234567u );

    /* The countdown, rounded up to whole seconds. */
    gauge_update_poll( 1000u + 65000u );
    CHECK( strcmp( row( 3 ), "TIME LEFT 3:55" ) == 0 );
    gauge_update_poll( 1000u + 299001u );
    CHECK( strcmp( row( 3 ), "TIME LEFT 0:01" ) == 0 );
    CHECK_EQ( reboots, 0u );

    /* A phone connects: the wait stops, so a long upload is never cut off. */
    send( HAL_BLE_EVT_CONNECTED );
    gauge_update_poll( 1000u + 100000u );
    CHECK( strcmp( row( 2 ), "PHONE CONNECTED" ) == 0 );
    CHECK( strcmp( row( 3 ), "TIME LEFT " ) == 0 );
    gauge_update_poll( 1000u + 100000u + 3600000u ); /* an hour later */
    CHECK_EQ( reboots, 0u );
    CHECK( gauge_update_active() );

    /* The phone leaves: a full wait starts again from that moment. */
    send( HAL_BLE_EVT_DISCONNECTED );
    gauge_update_poll( 5000000u );
    CHECK( strcmp( row( 2 ), "WAITING FOR PHONE" ) == 0 );
    CHECK( strcmp( row( 3 ), "TIME LEFT 5:00" ) == 0 );
    gauge_update_poll( 5000000u + 299000u );
    CHECK_EQ( reboots, 0u );

    /* The wait runs out: the device restarts, which also switches the radio
     * off. */
    gauge_update_poll( 5000000u + 300000u );
    CHECK_EQ( reboots, 1u );
    CHECK( !gauge_update_active() );
}

/* Six digits always, with leading zeros. */
static void test_passkey_keeps_its_leading_zeros( void )
{
    random_value = 42u;
    begin( 0u );
    CHECK_EQ( opened_passkey, 42u );
    CHECK( strcmp( row( 1 ), "PASSKEY   000042" ) == 0 );
}

/* Bluetooth would not start: the page says so, shows no passkey, and the
 * wait still ends by restarting. */
static void test_bluetooth_failure_is_shown_and_still_times_out( void )
{
    ble_open_result = HAL_ERR_IO;
    begin( 0u );
    CHECK( gauge_update_active() );
    CHECK( strcmp( row( 1 ), "PASSKEY   ------" ) == 0 );
    CHECK( strcmp( row( 2 ), "BLUETOOTH FAILED" ) == 0 );

    send( HAL_BLE_EVT_CONNECTED ); /* cannot happen, but must not matter */
    gauge_update_poll( 1000u );
    CHECK( strcmp( row( 2 ), "BLUETOOTH FAILED" ) == 0 );

    gauge_update_poll( 300000u );
    CHECK_EQ( reboots, 1u );
    CHECK( !gauge_update_active() );
}

/* No secure random passkey means no pairing to offer: Bluetooth is not
 * started, the page says so, and the wait still ends by restarting. */
static void test_no_random_source_means_no_bluetooth( void )
{
    random_ok = false;
    begin( 0u );
    CHECK( gauge_update_active() );
    CHECK_EQ( ble_opens, 0u );
    CHECK( strcmp( row( 1 ), "PASSKEY   ------" ) == 0 );
    CHECK( strcmp( row( 2 ), "BLUETOOTH FAILED" ) == 0 );
    gauge_update_poll( 300000u );
    CHECK_EQ( reboots, 1u );
}

/* Leaving on purpose restarts at once. */
static void test_exit_restarts_the_device( void )
{
    begin( 0u );
    gauge_update_exit();
    CHECK_EQ( reboots, 1u );
    CHECK( !gauge_update_active() );
}

/* Every widget must lie inside the panel, at whatever size it is built for. */
static void test_widgets_fit_the_panel( void )
{
    begin( 0u );
    CHECK_EQ( page_count, 5u );
    for ( size_t i = 0; i < page_count; i++ )
    {
        CHECK( ( uint32_t )page[i].rect.x + page[i].rect.w <= DISPLAY_WIDTH );
        CHECK( ( uint32_t )page[i].rect.y + page[i].rect.h <= DISPLAY_HEIGHT );
        CHECK( page[i].rect.w > 0u );
    }
}

int main(
    int argc,
    char ** argv
)
{
    const char * which = ( argc > 1 ) ? argv[1] : "session";

    if ( strcmp( which, "zeros" ) == 0 )
    {
        test_passkey_keeps_its_leading_zeros();
    }
    else if ( strcmp( which, "failed" ) == 0 )
    {
        test_bluetooth_failure_is_shown_and_still_times_out();
    }
    else if ( strcmp( which, "norandom" ) == 0 )
    {
        test_no_random_source_means_no_bluetooth();
    }
    else if ( strcmp( which, "exit" ) == 0 )
    {
        test_exit_restarts_the_device();
    }
    else if ( strcmp( which, "fit" ) == 0 )
    {
        test_widgets_fit_the_panel();
    }
    else
    {
        test_a_session();
    }
    return MT_RESULT();
}
