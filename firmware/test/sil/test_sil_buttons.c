/* Button timelines through the whole stack: real edge times into the input
 * recognizer, the gauge, the saved settings and the ECU session, judged by
 * what reaches the flash, the screen and the serial line. The scenarios run
 * one after another from one power-on, so each starts on the page the last
 * one left, and the page numbers below follow that order. */
#include "test/sil/harness.h"

/* A page change: stop, silence, read, first data. */
#define SETTLE_MS ( 1500u )

/* The page the gauge reads from the ECU, after a change has settled. */
static size_t page_now( void )
{
    return reading_page();
}

static void press_next_for( unsigned ms )
{
    button( &next_edges, true );
    run( ms );
    button( &next_edges, false );
    run( 40u );
}

static void press_action_for( unsigned ms )
{
    button( &action_edges, true );
    run( ms );
    button( &action_edges, false );
    run( 40u );
}

/* The loop did not run for this long: edges keep the time they happened, but
 * nothing reads them until it comes back. */
static void stall( unsigned ms )
{
    now += ms;
}

/* The page background, which follows the theme. */
static uint16_t page_bg( void )
{
    return ( page != NULL ) ? page[0].bg : 0u;
}

static uint32_t stored( const char * key )
{
    uint32_t value = 0xFFFFFFFFu;
    size_t len     = 0u;

    ( void )hal_nvm_load( key, &value, sizeof( value ), &len );
    return value;
}

/* A short press moves one page, and a press with contact bounce on both
 * edges still moves exactly one. */
static void test_a_tap_and_a_bouncy_tap_each_move_one_page( void )
{
    CHECK_EQ( page_now(), 0u );
    press_next_for( 100u );
    run( SETTLE_MS );
    CHECK_EQ( page_now(), 1u );

    button( &next_edges, true );
    run( 3u );
    button( &next_edges, false ); /* a bounce */
    run( 3u );
    button( &next_edges, true );
    run( 100u );
    button( &next_edges, false );
    run( 3u );
    button( &next_edges, true ); /* the release chatters */
    run( 2u );
    button( &next_edges, false );
    run( SETTLE_MS );
    CHECK_EQ( page_now(), 2u );
    CHECK_EQ( nvm_saves, 0u ); /* browsing writes no flash */
}

/* A hold saves the page and theme, shows SAVED for one second from the hold,
 * and does not also move the page. It fires once however long it lasts. */
static void test_a_hold_saves_once_and_does_not_move_the_page( void )
{
    /* Page 2 is the engine speed, a number page. */
    button( &next_edges, true );
    run( 1600u );
    CHECK_EQ( strcmp( row( 0 ), "SAVED" ), 0 ); /* the page gives way */
    CHECK_EQ(
        nvm_saves, 2u
    ); /* the page and the theme: the ECU profile is stored already */
    button( &next_edges, false );
    run( 1500u );
    CHECK( page != NULL ); /* the page is back */
    CHECK( strcmp( row( 0 ), ecu_param_info( 2 )->name ) == 0 );
    CHECK_EQ( page_now(), 2u );
    CHECK_EQ( stored( "param_idx" ), 2u );

    /* The same page again writes nothing, but still says SAVED. */
    button( &next_edges, true );
    run( 1600u );
    CHECK_EQ( strcmp( row( 0 ), "SAVED" ), 0 );
    CHECK_EQ( nvm_saves, 2u );
    button( &next_edges, false );
    run( 1500u );

    /* A long hold on a new page: one save, and the notice ends after its
     * second although the button is still down. */
    press_next_for( 100u );
    run( SETTLE_MS );
    CHECK_EQ( page_now(), 3u );
    button( &next_edges, true );
    run( 1600u );
    CHECK_EQ( strcmp( row( 0 ), "SAVED" ), 0 );
    run( 1100u ); /* 2.7 s into the hold */
    CHECK( page == NULL );
    CHECK( strcmp( dial_cfg->label, "COOLANT" ) == 0 );
    run( 2300u ); /* 5 s */
    CHECK( page == NULL );
    button( &next_edges, false );
    run( SETTLE_MS );
    CHECK_EQ( page_now(), 3u );
    CHECK_EQ( nvm_saves, 3u ); /* only the page changed */
    CHECK_EQ( stored( "param_idx" ), 3u );
}

/* Just under the hold time is a tap, just over is a hold. */
static void test_release_just_either_side_of_the_hold_time( void )
{
    press_next_for( 1450u );
    run( SETTLE_MS );
    CHECK_EQ( page_now(), 4u ); /* a tap */
    CHECK_EQ( nvm_saves, 3u );

    press_next_for( 1550u );
    run( SETTLE_MS );
    CHECK_EQ( page_now(), 4u ); /* a hold: no move */
    CHECK_EQ( nvm_saves, 4u );
    CHECK_EQ( stored( "param_idx" ), 4u );
}

/* The loop is late, but the edges are not: what counts is when the button
 * went up, not when it was noticed. */
static void test_a_late_poll_does_not_change_a_tap_into_a_hold( void )
{
    button( &next_edges, true );
    run( 1450u );
    button( &next_edges, false ); /* up at 1450 ms ... */
    stall( 250u );                /* ... but read at 1700 ms */
    run( SETTLE_MS );
    CHECK_EQ( page_now(), 5u ); /* still a tap */
    CHECK_EQ( nvm_saves, 4u );

    /* Held through a stall: the hold is found when the loop returns, once,
     * and the release after it is not a tap. */
    button( &next_edges, true );
    stall( 1700u );
    run( 100u );
    CHECK_EQ( nvm_saves, 5u );
    button( &next_edges, false );
    run( SETTLE_MS );
    CHECK_EQ( page_now(), 5u );
    CHECK_EQ( nvm_saves, 5u );
}

/* ACTION taps switch the theme and are never written without a NEXT hold. */
static void test_action_taps_switch_the_theme_in_ram_only( void )
{
    uint16_t light;
    uint16_t dark;

    run( 1200u );          /* the SAVED notice is over */
    CHECK( page != NULL ); /* page 5 is a number page */
    light = page_bg();
    press_action_for( 100u );
    run( 200u );
    dark = page_bg();
    CHECK( dark != light );
    press_action_for( 100u );
    run( 200u );
    CHECK_EQ( page_bg(), light );
    CHECK_EQ( nvm_saves, 5u );
}

/* Holding ACTION where there is nothing to clear or start does nothing: no
 * clear, no update mode, nothing sent, and the release is not a tap. */
static void test_an_action_hold_on_a_plain_page_does_nothing( void )
{
    uint16_t before = page_bg();
    size_t sent     = tx_count;

    button( &action_edges, true );
    run( 2600u );
    button( &action_edges, false );
    run( 300u );
    CHECK_EQ( ecu_clear_state(), ECU_CLEAR_IDLE );
    CHECK_EQ( ble_opens, 0u );
    CHECK_EQ( tx_count, sent );
    CHECK_EQ( page_bg(), before );
}

/* On a trouble code page, a hold that ends before the two seconds shows the
 * prompt and then goes quietly: nothing is cleared or written. */
static void test_an_early_release_abandons_the_clear( void )
{
    uint16_t before;

    for ( unsigned i = 5u; i < 19u; i++ )
    {
        press_next_for( 100u ); /* each a tap, one page further */
        run( 100u );
    }
    run( SETTLE_MS );
    CHECK_EQ( page_now(), 19u );
    before = page_bg();

    button( &action_edges, true );
    run( 900u );
    CHECK( strstr( row( 9 ), "HOLD TO CLEAR" ) != NULL );
    button( &action_edges, false );
    run( 500u );
    CHECK( strstr( row( 9 ), "HOLD" ) == NULL );
    CHECK_EQ( ecu_clear_state(), ECU_CLEAR_IDLE );
    CHECK_EQ( sim.writes, 0u );
    CHECK_EQ( page_bg(), before );
}

/* NEXT does nothing while a clear runs, and works again once the result has
 * been dismissed. Nothing but the clear address and this page is read. */
static void test_next_is_ignored_during_a_clear( void )
{
    const ssm1_profile_t * p = profile();
    uint16_t page_addr       = p->params[19].addr;
    size_t at_tap;

    button( &action_edges, true );
    run( 2500u );
    CHECK_EQ( ecu_clear_state(), ECU_CLEAR_BUSY );
    at_tap = tx_count;
    press_next_for( 100u );
    run( 300u );

    for ( unsigned i = 0u; i < 30000u; i++ )
    {
        if ( ecu_clear_state() != ECU_CLEAR_BUSY )
        {
            break;
        }
        step();
    }
    CHECK_EQ( ecu_clear_state(), ECU_CLEAR_DONE );
    CHECK_EQ( sim.writes, 4u );
    CHECK_EQ( page_now(), 19u );
    for ( size_t i = at_tap - ( at_tap % 4u ); ( i + 4u ) <= tx_count; i += 4u )
    {
        if ( tx_log[i] == 0x78u )
        {
            uint16_t addr =
                ( uint16_t )( ( tx_log[i + 1u] << 8 ) | tx_log[i + 2u] );

            CHECK( ( addr == p->clear_addr ) || ( addr == page_addr ) );
        }
    }

    /* Release, dismiss with a new press, and NEXT moves on again. */
    button( &action_edges, false );
    run( 200u );
    press_action_for( 100u );
    run( 300u );
    press_next_for( 100u );
    run( SETTLE_MS );
    CHECK_EQ( page_now(), 20u );
}

int main( void )
{
    boot( ROM_KEY );
    run( 3000u ); /* identified, on page 0 */
    test_a_tap_and_a_bouncy_tap_each_move_one_page();
    test_a_hold_saves_once_and_does_not_move_the_page();
    test_release_just_either_side_of_the_hold_time();
    test_a_late_poll_does_not_change_a_tap_into_a_hold();
    test_action_taps_switch_the_theme_in_ram_only();
    test_an_action_hold_on_a_plain_page_does_nothing();
    test_an_early_release_abandons_the_clear();
    test_next_is_ignored_during_a_clear();
    return MT_RESULT();
}
