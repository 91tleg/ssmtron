/* Update mode, end to end: a long ACTION press on the Info page starts it, the
 * page shows a fresh passkey and a countdown, a phone connecting pauses the
 * countdown, and the timeout or a NEXT tap leaves by restarting. The fake
 * restart returns, so one boot can go through several sessions. */
#include "test/sil/harness.h"

/* NEXT through the pages until the Info page shows. */
static void go_to_info( void )
{
    for ( size_t i = 0u; ( i < ( ecu_param_count() + 2u ) )
                         && ( strcmp( row( 0 ), "INFO" ) != 0 );
          i++ )
    {
        tap_next();
    }
    CHECK( strcmp( row( 0 ), "INFO" ) == 0 );
}

/* Holds ACTION on the Info page until update mode has started, and leaves
 * the button down. */
static void hold_to_start( void )
{
    button( &action_edges, true );
    run( 700u );
    CHECK( strstr( row( 4 ), "HOLD TO UPDATE" ) != NULL ); /* the prompt */
    run( 1500u );
}

static void release_action( void )
{
    button( &action_edges, false );
    run( 100u );
}

static void test_a_session_from_start_to_timeout( void )
{
    random_value = 123456u;
    go_to_info();
    hold_to_start();

    /* Bluetooth was started with a name and the random passkey, and the page
     * was drawn already showing both, not filled in a moment later. */
    CHECK_EQ( ble_opens, 1u );
    CHECK_EQ( ble_passkey, 123456u );
    CHECK( strcmp( ble_name, "SSM1 Gauge" ) == 0 );
    CHECK( strstr( first_frame[0], "UPDATE MODE" ) != NULL );
    CHECK( strstr( first_frame[1], "123456" ) != NULL );
    CHECK( strstr( first_frame[3], "5:00" ) != NULL );
    CHECK( strstr( row( 2 ), "WAITING FOR PHONE" ) != NULL );

    /* Letting go of the button that started it does nothing else. */
    release_action();
    CHECK( strstr( row( 0 ), "UPDATE MODE" ) != NULL );
    CHECK_EQ( reboots, 0u );

    /* The countdown runs while no phone is connected. */
    run( 60000u );
    CHECK( strcmp( row( 3 ) + 10, "5:00" ) != 0 );
    CHECK( ( row( 3 )[10] == '3' ) || ( row( 3 )[10] == '4' ) );

    /* A phone connects: the wait stops, however long the upload takes. */
    ble_send( HAL_BLE_EVT_CONNECTED );
    run( 100u );
    CHECK( strstr( row( 2 ), "PHONE CONNECTED" ) != NULL );
    CHECK( strcmp( row( 3 ) + 10, "" ) == 0 );
    run( 400000u );
    CHECK_EQ( reboots, 0u );
    CHECK( strstr( row( 0 ), "UPDATE MODE" ) != NULL );

    /* It leaves: a whole new wait starts. */
    ble_send( HAL_BLE_EVT_DISCONNECTED );
    run( 100u );
    CHECK( strstr( row( 2 ), "WAITING FOR PHONE" ) != NULL );
    CHECK( strcmp( row( 3 ) + 10, "5:00" ) == 0 );

    /* Nobody comes: the wait runs out and the gauge restarts. */
    run( 301000u );
    CHECK_EQ( reboots, 1u );
}

static void test_a_tap_on_next_leaves( void )
{
    random_value = 7u;
    go_to_info(); /* the fake restart returned: still on the Info page */
    hold_to_start();
    CHECK_EQ( ble_opens, 2u );
    CHECK( strstr( first_frame[1], "000007" ) != NULL ); /* leading zeros */
    release_action();
    CHECK_EQ( reboots, 1u );
    tap_next();
    CHECK_EQ( reboots, 2u );
}

static void
test_bluetooth_that_will_not_start_is_shown_and_still_times_out( void )
{
    ble_open_result = HAL_ERR_IO;
    go_to_info();
    hold_to_start();
    CHECK_EQ( ble_opens, 3u );
    CHECK( strstr( row( 2 ), "BLUETOOTH FAILED" ) != NULL );
    CHECK( strstr( row( 1 ), "------" ) != NULL ); /* no passkey to show */
    release_action();
    run( 301000u );
    CHECK_EQ( reboots, 3u );
    ble_open_result = HAL_OK;
}

static void test_no_random_source_means_no_bluetooth( void )
{
    unsigned opens = ble_opens;

    random_ok = false;
    go_to_info();
    hold_to_start();
    CHECK_EQ( ble_opens, opens ); /* it is not started on a guessable key */
    CHECK( strstr( row( 2 ), "BLUETOOTH FAILED" ) != NULL );
    release_action();
    tap_next();
    CHECK_EQ( reboots, 4u );
    random_ok = true;
}

int main( void )
{
    boot( ROM_KEY );
    run( 3000u );
    test_a_session_from_start_to_timeout();
    test_a_tap_on_next_leaves();
    test_bluetooth_that_will_not_start_is_shown_and_still_times_out();
    test_no_random_source_means_no_bluetooth();
    return MT_RESULT();
}
