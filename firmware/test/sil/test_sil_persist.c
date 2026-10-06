/* What survives a restart. The firmware can only be restarted by starting the
 * program again, so "save" and "restore" are two runs that share a file
 * standing in for flash. */
#include "test/sil/harness.h"

/* The page saved by "save" belongs to a profile other than the first one, so
 * the page only comes back if the profile does too. */
#define SAVED_KEY  ( 0x3170u )
#define SAVED_PAGE ( 14u )

/* "save" boots against another ECU, goes to a page of its own and saves it
 * with a long NEXT press. */
static void scenario_save( void )
{
    nvm_item_t items[6];
    uint32_t value     = 0u;
    bool found_page    = false;
    bool found_profile = false;

    /* Fresh flash: whatever an earlier run left must not be able to make the
     * restore pass. */
    ( void )remove( NVM_FILE ); /* fails harmlessly if there is none */
    {
        FILE * leftover = fopen( NVM_FILE, "rb" );

        CHECK( leftover == NULL ); /* so it really is gone */
        if ( leftover != NULL )
        {
            ( void )fclose( leftover );
        }
    }
    nvm_file_enabled = true;
    boot( SAVED_KEY );
    run( 3000u );
    CHECK_EQ( ecu_id(), 0x013170u );
    for ( size_t i = 0u; i < SAVED_PAGE; i++ )
    {
        tap_next();
    }
    run( 1500u );
    CHECK( strcmp( row( 0 ), ecu_param_info( SAVED_PAGE )->name ) == 0 );
    button( &next_edges, true );
    run( 1700u );
    button( &next_edges, false );
    run( 200u );

    /* The save reached the file, whole, with the page and the profile. */
    CHECK( nvm_file_writes >= 1u );
    ( void )memset( items, 0, sizeof( items ) );
    CHECK( nvm_read_file( items ) );
    for ( size_t i = 0u; i < 6u; i++ )
    {
        if ( items[i].used && ( strcmp( items[i].key, "param_idx" ) == 0 ) )
        {
            ( void )memcpy( &value, items[i].data, sizeof( value ) );
            CHECK_EQ( value, SAVED_PAGE );
            found_page = true;
        }
        if ( items[i].used && ( strcmp( items[i].key, "ecu_profile" ) == 0 ) )
        {
            ( void )memcpy( &value, items[i].data, sizeof( value ) );
            CHECK_EQ( value, SAVED_KEY );
            found_profile = true;
        }
    }
    CHECK( found_page );
    CHECK( found_profile );
}

/* "restore" starts again from the file: that page is drawn first, before the
 * ECU has sent a byte, and it stays when the same ECU answers. */
static void scenario_restore( void )
{
    const char * saved_name =
        ssm1_profile_find( SAVED_KEY )->params[SAVED_PAGE].info->name;
    const char * first_name =
        ssm1_profile_find( ROM_KEY )->params[SAVED_PAGE].info->name;

    nvm_load_file = true;
    boot( SAVED_KEY );
    CHECK( nvm_file_loaded ); /* the file from the save was really there */
    CHECK( strcmp( saved_name, first_name ) != 0 ); /* the test can tell */
    /* The saved page was the frame on screen when the panel turned on. */
    CHECK_EQ( on_frames, 1u );
    CHECK( !on_was_dial );
    CHECK( strcmp( on_row0, saved_name ) == 0 );
    CHECK_EQ( ecu_id(), 0u ); /* nobody answered yet */
    CHECK_EQ( tx_count, 0u );
    run( 4000u );
    CHECK_EQ( ecu_id(), 0x013170u );
    CHECK( strcmp( row( 0 ), saved_name ) == 0 ); /* still there */
}

int main(
    int argc,
    char ** argv
)
{
    const char * which = ( argc > 1 ) ? argv[1] : "";

    if ( strcmp( which, "save" ) == 0 )
    {
        scenario_save();
    }
    else if ( strcmp( which, "restore" ) == 0 )
    {
        scenario_restore();
    }
    else
    {
        ( void )fputs( "usage: test_sil_persist save|restore\n", stderr );
        mt_failures++;
    }
    return MT_RESULT();
}
