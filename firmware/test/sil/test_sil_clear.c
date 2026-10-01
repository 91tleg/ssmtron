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

/* ---- the ECU goes quiet while it writes its memory --------------------- */

/* Taps to the first trouble code page and holds ACTION until the clear is
 * running. Returns that page's index. */
static size_t start_a_clear( void )
{
    size_t code_page = first_code_page();

    for ( size_t i = 0u; i < code_page; i++ )
    {
        tap_next();
    }
    run( 1500u );
    button( &action_edges, true );
    run( 2500u );
    CHECK_EQ( ecu_clear_state(), ECU_CLEAR_BUSY );
    return code_page;
}

/* Runs until the ECU has had all four writes. They are 384 ms apart. */
static void wait_for_the_writes( void )
{
    for ( unsigned i = 0u; ( i < 5000u ) && ( sim.writes < 4u ); i++ )
    {
        step();
    }
}

/* Commands the gauge sent that are not clear writes, between two times. */
static unsigned other_commands_between(
    uint32_t from,
    uint32_t to
)
{
    unsigned count = 0u;

    for ( size_t i = 0u; ( i + 4u ) <= tx_count; i += 4u )
    {
        if ( ( tx_time[i] > from ) && ( tx_time[i] < to )
             && ( tx_log[i] != 0xAAu ) )
        {
            count++;
        }
    }
    return count;
}

/* While the ECU writes its memory the gauge says nothing to it, whatever it
 * hears back. The page keeps saying CLEARING, and the clear ends when the ECU
 * speaks again, not when the wait is over. */
static void test_a_quiet_ecu_does_not_end_or_disturb_the_clear( void )
{
    size_t code_page;
    uint32_t first_write;
    uint32_t last_write;
    uint32_t wait_over;
    uint32_t done_at = 0u;
    int32_t value    = 0;

    sim.busy_after_write = true;
    sim.busy_ms          = 25000u; /* past the 22 s wait */
    code_page            = start_a_clear();
    wait_for_the_writes();
    CHECK_EQ( sim.writes, 4u );
    first_write = sim.write_at[0];
    last_write  = sim.write_at[3];
    wait_over   = last_write + 22400u;

    /* Every few seconds of the silence: still clearing, page intact. */
    while ( ( now - first_write ) < 29000u )
    {
        run( 1000u );
        if ( ecu_clear_state() == ECU_CLEAR_BUSY )
        {
            CHECK( strstr( row( 9 ), "CLEARING" ) != NULL );
            CHECK(
                strstr( row( 0 ), ecu_param_info( code_page )->name ) != NULL
            );
        }
        else if ( done_at == 0u )
        {
            done_at = now;
        }
    }
    CHECK_EQ( ecu_clear_state(), ECU_CLEAR_DONE );
    CHECK( strstr( row( 9 ), "CLEARED" ) != NULL );
    CHECK_EQ( sim.writes, 4u );

    /* Nothing but the four writes went out until the wait was over. */
    CHECK_EQ( other_commands_between( first_write, wait_over ), 0u );
    /* It was not done before the ECU spoke again, or before the wait. */
    CHECK( done_at >= ( first_write + 25000u ) );
    CHECK( done_at <= ( first_write + 30000u ) );

    /* Released and dismissed, the page reads the ECU again. */
    button( &action_edges, false );
    run( 200u );
    run( 1500u );
    CHECK( ecu_link() == ECU_LINK_UP );
    CHECK( ecu_get_value( code_page, &value ) );
    CHECK_EQ( value, expected( code_page ) );
}

/* The ECU never comes back: the clear fails at its limit, the page says so,
 * and the gauge keeps trying to read. */
static void test_an_ecu_that_never_returns_fails_the_clear( void )
{
    uint32_t first_write;
    uint32_t last_write;
    size_t tx_at_failure;

    sim.busy_after_write = true;
    sim.busy_ms          = 1000000u;
    ( void )start_a_clear();
    wait_for_the_writes();
    CHECK_EQ( sim.writes, 4u );
    first_write = sim.write_at[0];
    last_write  = sim.write_at[3];

    while ( ( ecu_clear_state() == ECU_CLEAR_BUSY )
            && ( ( now - first_write ) < 40000u ) )
    {
        run( 100u );
    }
    CHECK_EQ( ecu_clear_state(), ECU_CLEAR_FAILED );
    /* The limit is 30 s from the start of the clear, which is a fraction of a
     * second before the first write. */
    CHECK( ( now - first_write ) >= 29000u );
    CHECK( ( now - first_write ) <= 30500u );
    CHECK( strstr( row( 9 ), "CLEAR FAILED" ) != NULL );
    CHECK_EQ( sim.writes, 4u );
    CHECK_EQ( other_commands_between( first_write, last_write + 22400u ), 0u );
    CHECK( ecu_link() == ECU_LINK_DOWN );

    /* It still tries: more reads go out after the failure. */
    tx_at_failure = tx_count;
    run( 5000u );
    CHECK( tx_count > tx_at_failure );
    button( &action_edges, false );
    run( 100u );
}

int main(
    int argc,
    char ** argv
)
{
    const char * which = ( argc > 1 ) ? argv[1] : "run";

    boot( ROM_KEY );
    run( 3000u );
    if ( strcmp( which, "busy" ) == 0 )
    {
        test_a_quiet_ecu_does_not_end_or_disturb_the_clear();
    }
    else if ( strcmp( which, "gone" ) == 0 )
    {
        test_an_ecu_that_never_returns_fails_the_clear();
    }
    else
    {
        test_a_held_press_clears_the_codes_through_the_wire();
    }
    return MT_RESULT();
}
