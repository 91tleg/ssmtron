/* What the gauge opens on, and what it remembers, when it meets an ECU for the
 * first time or a different one. Each case is its own start, because the
 * gauge's state lasts for the whole run. */
#include "test/sil/harness.h"

/* A profile other than the simulated ECU's, with a page of its own. */
#define OTHER_KEY  ( 0x3170u )
#define OTHER_PAGE ( 14u )
#define NOT_STORED ( 0xFFFFFFFFu )

/* A ROM ID no profile has. */
#define UNKNOWN_KEY ( 0x9999u )

/* Page 0 of the simulated ECU is the voltage dial. */
static bool voltage_dial_shown( void )
{
    return ( page == NULL ) && ( dial_cfg != NULL )
           && ( strcmp( dial_cfg->label, "VOLTAGE" ) == 0 );
}

static uint32_t stored( const char * key )
{
    uint32_t value = NOT_STORED;
    size_t len     = 0u;

    ( void )hal_nvm_load( key, &value, sizeof( value ), &len );
    return value;
}

/* Nothing is stored: no profile is assumed, the first frame is the info page,
 * and the ECU that answers becomes the remembered one without any page
 * being saved with it. */
static void scenario_fresh( void )
{
    boot_fresh( ROM_KEY );

    CHECK_EQ( on_frames, 1u );
    CHECK( !on_was_dial );
    CHECK( strcmp( on_row0, "INFO" ) == 0 );
    CHECK_EQ( ecu_param_count(), 0u ); /* no pages guessed */
    CHECK_EQ( tx_count, 0u );

    run( 3000u );
    CHECK_EQ( ecu_id(), 0x013671u );
    CHECK( strcmp( row( 0 ), "INFO" ) == 0 ); /* the page does not jump */
    CHECK_EQ( stored( "ecu_profile" ), ROM_KEY );
    CHECK_EQ( stored( "param_idx" ), NOT_STORED );
    CHECK_EQ( nvm_saves, 1u );

    /* The info page leads to the first page of the ECU that was found. */
    tap_next();
    run( 1500u );
    CHECK( voltage_dial_shown() );
}

/* The remembered ECU is not the one that answers: its pages are replaced,
 * the page goes back to the first, and the new profile and page are stored
 * so the next start does not mix the old page with the new profile. */
static void scenario_different( void )
{
    const char * old_name =
        ssm1_profile_find( OTHER_KEY )->params[OTHER_PAGE].info->name;
    const char * new_name = ssm1_profile_find( ROM_KEY )->params[0].info->name;

    boot_remembered( OTHER_KEY, OTHER_PAGE, ROM_KEY );
    CHECK( strcmp( on_row0, old_name ) == 0 ); /* assumed until it answers */
    CHECK( strcmp( old_name, new_name ) != 0 );

    run( 4000u );
    CHECK_EQ( ecu_id(), 0x013671u );
    CHECK( voltage_dial_shown() );
    CHECK_EQ( stored( "ecu_profile" ), ROM_KEY );
    CHECK_EQ( stored( "param_idx" ), 0u );
}

/* A profile is remembered but no page was ever saved: the pages are ready,
 * and the gauge still opens on the info page. */
static void scenario_profile_only( void )
{
    uint32_t profile_value = ROM_KEY;

    ( void )memset( nvm_items, 0, sizeof( nvm_items ) );
    ( void )hal_nvm_save(
        "ecu_profile", &profile_value, sizeof( profile_value )
    );
    nvm_saves = 0u;
    boot_fresh( ROM_KEY );

    CHECK( strcmp( on_row0, "INFO" ) == 0 );
    CHECK( ecu_param_count() > 0u ); /* the remembered pages are there */

    tap_next();
    run( 100u );
    CHECK( voltage_dial_shown() );
    run( 3000u );
    CHECK_EQ( ecu_id(), 0x013671u );
    CHECK_EQ( nvm_saves, 0u ); /* same ECU: nothing to store */
}

/* An ECU the firmware does not know has no pages and is not remembered: an
 * unknown key would bring the first profile's pages back at the next start.
 * Whatever was remembered stays, and the info page is shown. */
static void scenario_unknown( void )
{
    boot_remembered( ROM_KEY, 3u, UNKNOWN_KEY );
    run( 3000u );
    CHECK_EQ( ecu_id(), 0x019999u );
    CHECK_EQ( ecu_param_count(), 0u );
    CHECK( strcmp( row( 0 ), "INFO" ) == 0 );
    CHECK_EQ( stored( "ecu_profile" ), ROM_KEY );
    CHECK_EQ( stored( "param_idx" ), 3u );
    CHECK_EQ( nvm_saves, 0u );

    /* A long NEXT press has no page worth keeping: the theme alone. */
    button( &next_edges, true );
    run( 1700u );
    button( &next_edges, false );
    run( 200u );
    CHECK_EQ( stored( "ecu_profile" ), ROM_KEY );
    CHECK_EQ( stored( "param_idx" ), 3u );
}

/* The same on a first run: nothing is stored at all. */
static void scenario_unknown_first_run( void )
{
    boot_fresh( UNKNOWN_KEY );
    run( 3000u );
    CHECK_EQ( ecu_id(), 0x019999u );
    CHECK( strcmp( row( 0 ), "INFO" ) == 0 );
    CHECK_EQ( stored( "ecu_profile" ), NOT_STORED );
    CHECK_EQ( stored( "param_idx" ), NOT_STORED );
}

/* The remembered ECU answers: nothing is rewritten. */
static void scenario_same( void )
{
    boot( ROM_KEY );
    run( 3000u );
    CHECK_EQ( ecu_id(), 0x013671u );
    CHECK_EQ( nvm_saves, 0u );
}

int main(
    int argc,
    char ** argv
)
{
    const char * which = ( argc > 1 ) ? argv[1] : "";

    if ( strcmp( which, "fresh" ) == 0 )
    {
        scenario_fresh();
    }
    else if ( strcmp( which, "different" ) == 0 )
    {
        scenario_different();
    }
    else if ( strcmp( which, "profileonly" ) == 0 )
    {
        scenario_profile_only();
    }
    else if ( strcmp( which, "unknown" ) == 0 )
    {
        scenario_unknown();
    }
    else if ( strcmp( which, "unknownfirst" ) == 0 )
    {
        scenario_unknown_first_run();
    }
    else if ( strcmp( which, "same" ) == 0 )
    {
        scenario_same();
    }
    else
    {
        ( void )fputs(
            "usage: test_sil_firstrun "
            "fresh|different|profileonly|unknown|unknownfirst|same\n",
            stderr
        );
        mt_failures++;
    }
    return MT_RESULT();
}
