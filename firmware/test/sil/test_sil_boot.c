/* A cold start against a simulated ECU: the first frame comes before the ECU
 * says anything, then it identifies and the dial gets its reading. After that
 * the serial link fails and recovers. */
#include "test/sil/harness.h"

static void test_the_first_frame_comes_before_the_ecu_answers( void )
{
    /* Nothing was saved: the first profile is assumed and its first page is
     * drawn, though no ECU has said a word. */
    CHECK_EQ( ecu_id(), 0u );
    CHECK_EQ( tx_count, 0u );
    /* Exactly one frame existed when the panel turned on, and it was the
     * voltage dial. */
    CHECK_EQ( on_frames, 1u );
    CHECK( on_was_dial );
    CHECK(
        ( on_dial != NULL ) && ( strcmp( on_dial->label, "VOLTAGE" ) == 0 )
    );
}

static void test_the_ecu_identifies_and_the_dial_gets_its_reading( void )
{
    run( 3000u );
    CHECK_EQ( ecu_id(), 0x013671u );
    CHECK_EQ( ecu_link(), ECU_LINK_UP );
    CHECK( dial_has_value );
    CHECK_EQ( dial_value, expected( 0 ) ); /* system voltage, hundredths */
}

static void test_the_link_drops_and_comes_back( void )
{
    sim.alive = false;
    run( 3000u );
    CHECK_EQ( ecu_link(), ECU_LINK_DOWN );
    sim.alive = true;
    run( 12000u );
    CHECK_EQ( ecu_link(), ECU_LINK_UP );
}

int main( void )
{
    boot( ROM_KEY );
    test_the_first_frame_comes_before_the_ecu_answers();
    test_the_ecu_identifies_and_the_dial_gets_its_reading();
    test_the_link_drops_and_comes_back();
    return MT_RESULT();
}
