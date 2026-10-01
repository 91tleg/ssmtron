/* Moving between pages: what goes over the wire when NEXT is pressed, and the
 * scales the dials and number pages end up with. */
#include "test/sil/harness.h"

static void test_next_stops_the_stream_and_reads_the_new_parameter( void )
{
    size_t before = tx_count;
    size_t stop;
    size_t read;
    const ssm1_param_t * speed = &profile()->params[1];
    char text[TEXT_MAX + 1u];

    tap_next();
    run( 1500u );
    stop = find_cmd( before, 0x12u, 0x00u, 0x00u );
    read = find_cmd(
        before, 0x78u, ( uint8_t )( speed->addr >> 8 ), ( uint8_t )speed->addr
    );
    CHECK( stop < tx_count );
    CHECK( read < tx_count );
    CHECK( stop < read ); /* stop first, then the new read */
    CHECK( ( tx_time[read] - tx_time[stop] ) >= 80u ); /* after the silence */

    /* The speed has no dial, so it is a number page: name, value, unit. */
    CHECK( strcmp( row( 0 ), ecu_param_info( 1 )->name ) == 0 );
    ( void )fmt_value(
        text, sizeof( text ), ecu_param_info( 1 ), expected( 1 )
    );
    CHECK( strcmp( row( 1 ), text ) == 0 );
}

static void test_rpm_and_coolant_dials_get_the_right_scale( void )
{
    tap_next(); /* engine speed */
    run( 1500u );
    CHECK( strcmp( dial_cfg->label, "RPM" ) == 0 );
    CHECK( dial_has_value );
    CHECK_EQ( dial_value, expected( 2 ) ); /* rpm itself */

    tap_next(); /* coolant, whole F on this ECU: the dial wants tenths */
    run( 1500u );
    CHECK( strcmp( dial_cfg->label, "COOLANT" ) == 0 );
    CHECK( dial_has_value );
    CHECK_EQ( dial_value, expected( 3 ) * 10 );
}

int main( void )
{
    boot( ROM_KEY );
    run( 3000u ); /* identified, on the first page */
    test_next_stops_the_stream_and_reads_the_new_parameter();
    test_rpm_and_coolant_dials_get_the_right_scale();
    return MT_RESULT();
}
