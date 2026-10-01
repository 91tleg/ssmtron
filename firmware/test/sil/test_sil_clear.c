/* Clearing trouble codes with a held press, through the wire: the writes the
 * ECU receives, their spacing, and what the page says. */
#include "test/sil/harness.h"

/* Index of the first parameter whose bits are trouble codes. */
static size_t first_code_page( void )
{
    size_t idx = 0u;

    while ( ( idx < ecu_param_count() )
            && !ecu_param_info( idx )->trouble_codes )
    {
        idx++;
    }
    return idx;
}

static void test_a_held_press_clears_the_codes_through_the_wire( void )
{
    const ssm1_profile_t * p = profile();
    size_t code_page         = first_code_page();
    size_t at_start;
    uint8_t want;

    CHECK( code_page < ecu_param_count() );
    for ( size_t i = 0u; i < code_page; i++ )
    {
        tap_next();
    }
    run( 1500u );
    CHECK( strstr( row( 0 ), ecu_param_info( code_page )->name ) != NULL );

    /* Hold ACTION past the prompt and the 2 s bar. */
    button( &action_edges, true );
    run( 700u );
    CHECK( strstr( row( 9 ), "HOLD TO CLEAR" ) != NULL );
    run( 1800u );
    CHECK_EQ( ecu_clear_state(), ECU_CLEAR_BUSY );
    at_start = tx_count;

    run( 31000u ); /* the writes, then the wait for the ECU's flash */
    CHECK_EQ( ecu_clear_state(), ECU_CLEAR_DONE );
    CHECK_EQ( sim.writes, 4u );
    want = ( uint8_t )~mem( p->clear_addr ); /* this profile inverts */
    for ( unsigned i = 0u; i < 4u; i++ )
    {
        CHECK_EQ( sim.write_cmd[i][0], 0xAAu );
        CHECK_EQ( sim.write_cmd[i][1], ( uint8_t )( p->clear_addr >> 8 ) );
        CHECK_EQ( sim.write_cmd[i][2], ( uint8_t )p->clear_addr );
        CHECK_EQ( sim.write_cmd[i][3], want );
        if ( i > 0u )
        {
            /* At least the 384 ms the session promises, and no more than a few
             * polls late. */
            CHECK( ( sim.write_at[i] - sim.write_at[i - 1u] ) >= 384u );
            CHECK( ( sim.write_at[i] - sim.write_at[i - 1u] ) <= 390u );
        }
    }
    CHECK( strstr( row( 9 ), "CLEARED" ) != NULL );
    CHECK( at_start <= tx_count );

    button( &action_edges, false );
    run( 100u );
}

int main( void )
{
    boot( ROM_KEY );
    run( 3000u );
    test_a_held_press_clears_the_codes_through_the_wire();
    return MT_RESULT();
}
