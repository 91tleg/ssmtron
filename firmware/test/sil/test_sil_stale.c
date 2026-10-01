/* Readings that go stale: an ECU that stops talking, and one page it never
 * answers. A value is shown only while it is fresh and only on its own page,
 * the gauge keeps trying, and a page change is never held up by the retries.
 * The scenarios run one after another from one power-on. */
#include "test/sil/harness.h"

#define FRESH_MS  ( 500u )  /* the session's limit on a shown value */
#define LINK_MS   ( 1000u ) /* the link is up while samples are this recent */
#define SETTLE_MS ( 1500u )

static void press_next_for( unsigned ms )
{
    button( &next_edges, true );
    run( ms );
    button( &next_edges, false );
    run( 40u );
}

/* Every millisecond, a dial shows its own reading or nothing: never another
 * value. */
static void run_dial_checked(
    unsigned ms,
    int32_t want
)
{
    for ( unsigned i = 0u; i < ms; i++ )
    {
        step();
        CHECK( !dial_has_value || ( dial_value == want ) );
    }
}

/* The ECU stops answering on the page in view. Its last value is shown while
 * it is fresh, then the dial rests, then the link drops. Everything it shows
 * on the way is right, and it comes back by itself. */
static void
test_a_silent_ecu_blanks_the_value_then_the_link_and_recovers( void )
{
    uint32_t silent_at;
    unsigned back_after = 0u;

    run( 5000u ); /* past the power-on sweep */
    CHECK( dial_has_value );
    CHECK_EQ( dial_value, expected( 0 ) );
    CHECK_EQ( ecu_link(), ECU_LINK_UP );

    sim.alive = false;
    silent_at = now;
    run_dial_checked( 300u, expected( 0 ) );
    CHECK( dial_has_value ); /* still fresh */
    run_dial_checked( 400u, expected( 0 ) );
    CHECK( ( now - silent_at ) > FRESH_MS );
    CHECK( !dial_has_value );            /* stale: the dial goes back to rest */
    CHECK_EQ( ecu_link(), ECU_LINK_UP ); /* samples under a second old */
    run_dial_checked( 600u, expected( 0 ) );
    CHECK( ( now - silent_at ) > LINK_MS );
    CHECK_EQ( ecu_link(), ECU_LINK_DOWN );
    run_dial_checked( 2000u, expected( 0 ) );
    CHECK( !dial_has_value );

    sim.alive = true;
    while ( ( !dial_has_value || ( ecu_link() != ECU_LINK_UP ) )
            && ( back_after < 6000u ) )
    {
        run_dial_checked( 1u, expected( 0 ) );
        back_after++;
    }
    CHECK( dial_has_value );
    CHECK_EQ( dial_value, expected( 0 ) );
    CHECK_EQ( ecu_link(), ECU_LINK_UP );
    /* One backoff, one stop and silence, then a read and its first frames. */
    CHECK( back_after <= 3000u );
}

/* One page the ECU never answers. Moving onto it must not show the fresh
 * reading of the page just left, it keeps asking, and moving on again is not
 * held up by the pause between its attempts. */
static void test_a_dead_page_shows_nothing_and_does_not_block_the_next( void )
{
    const ssm1_profile_t * p = profile();
    const uint16_t dead      = p->params[3].addr;
    uint32_t last_tx;
    unsigned tries = 0u;
    bool saw_dial  = false;
    unsigned waited;
    size_t asked_from;
    char text[TEXT_MAX + 1u];

    /* To page 2, the rpm dial, with a fresh reading. */
    press_next_for( 100u );
    run( 200u );
    press_next_for( 100u );
    run( SETTLE_MS );
    CHECK( strcmp( dial_cfg->label, "RPM" ) == 0 );
    CHECK( dial_has_value );
    CHECK_EQ( dial_value, expected( 2 ) );

    /* Page 3 is the coolant dial, and its address gets no answer. */
    sim.dead_addr_set = true;
    sim.dead_addr     = dead;
    button( &next_edges, true );
    run( 100u );
    button( &next_edges, false );
    for ( unsigned i = 0u; i < 6000u; i++ )
    {
        step();
        if ( ( dial_cfg != NULL )
             && ( strcmp( dial_cfg->label, "COOLANT" ) == 0 ) )
        {
            saw_dial = true;
            CHECK( !dial_has_value ); /* not the rpm that is still fresh */
        }
    }
    CHECK( saw_dial );
    for ( size_t i = 0u; ( i + 4u ) <= tx_count; i += 4u )
    {
        if ( ( tx_log[i] == 0x78u )
             && ( ( uint16_t )( ( tx_log[i + 1u] << 8 ) | tx_log[i + 2u] )
                  == dead ) )
        {
            tries++;
        }
    }
    CHECK( tries >= 4u ); /* it keeps asking */

    /* Wait for the pause between rounds of attempts. First a command goes
     * out, then nothing for longer than any single wait for data lasts: the
     * pause has just begun, with most of it still to run. */
    waited = 0u;
    while ( ( ( now - tx_time[tx_count - 1u] ) > 20u ) && ( waited < 5000u ) )
    {
        step();
        waited++;
    }
    while ( ( ( now - tx_time[tx_count - 1u] ) < 400u ) && ( waited < 9000u ) )
    {
        step();
        waited++;
    }
    last_tx = tx_time[tx_count - 1u];
    CHECK( ( now - last_tx ) >= 400u );
    CHECK( ( now - last_tx ) < 450u );

    /* Page 4 is a number page. It is asked for at once, not after the pause,
     * and shows its value well inside what is left of it. */
    asked_from = tx_count;
    press_next_for( 100u );
    run( 500u );
    CHECK( find_cmd( asked_from, 0x12u, 0x00u, 0x00u ) < tx_count );
    CHECK(
        ( tx_time[find_cmd( asked_from, 0x12u, 0x00u, 0x00u )]
          - ( now - 640u ) )
        <= 150u
    );
    CHECK( strcmp( row( 0 ), ecu_param_info( 4 )->name ) == 0 );
    ( void )fmt_value(
        text, sizeof( text ), ecu_param_info( 4 ), expected( 4 )
    );
    CHECK( strcmp( row( 1 ), text ) == 0 );
}

/* The state of flag row r on the switches page, as shown: the last character
 * of its text. */
static char shown_state( size_t r )
{
    const char * text = row( 1u + r );

    return ( text[0] == '\0' ) ? '?' : text[strlen( text ) - 1u];
}

/* A flags byte that goes stale shows dashes with the names still in place,
 * never a wrong state, and shows its states again when the ECU returns. */
static void test_a_stale_flags_byte_shows_dashes_and_the_names_stay( void )
{
    const ssm1_profile_t * p = profile();
    const ecu_param_info_t * info;
    char names[8][TEXT_MAX + 1u];

    /* Page 17 is the first switches byte. */
    for ( unsigned i = 4u; i < 17u; i++ )
    {
        press_next_for( 100u );
        run( 100u );
    }
    run( SETTLE_MS );
    CHECK_EQ( reading_page(), 17u );
    info = ecu_param_info( 17u );
    CHECK( info->flag_count <= 8u );
    for ( size_t r = 0u; r < info->flag_count; r++ )
    {
        bool want = ecu_flag_set( &info->flags[r], mem( p->params[17].addr ) );

        CHECK_EQ( shown_state( r ), want ? '1' : '0' );
        ( void )strncpy( names[r], row( 1u + r ), TEXT_MAX );
        names[r][TEXT_MAX] = '\0';
    }

    sim.alive = false;
    run( 700u );
    for ( size_t r = 0u; r < info->flag_count; r++ )
    {
        CHECK_EQ( shown_state( r ), '-' );
        /* The same name, in the same place. */
        CHECK_EQ(
            strncmp( row( 1u + r ), names[r], strlen( names[r] ) - 1u ), 0
        );
    }
    CHECK( strstr( row( 0 ), info->name ) != NULL );

    sim.alive = true;
    run( 5000u );
    for ( size_t r = 0u; r < info->flag_count; r++ )
    {
        bool want = ecu_flag_set( &info->flags[r], mem( p->params[17].addr ) );

        CHECK_EQ( shown_state( r ), want ? '1' : '0' );
    }
}

int main( void )
{
    boot( ROM_KEY );
    run( 3000u ); /* identified, on page 0 */
    test_a_silent_ecu_blanks_the_value_then_the_link_and_recovers();
    test_a_dead_page_shows_nothing_and_does_not_block_the_next();
    test_a_stale_flags_byte_shows_dashes_and_the_names_stay();
    return MT_RESULT();
}
