/* The UARTE baud register math. The expected registers are the ones in the
 * nRF52 data sheet (and 1953 baud, which is exactly 0x80000 of them), not the
 * helper's own formula. */
#include <stdint.h>

#include "hal/nrf52/uarte_baud.h"
#include "test/minitest.h"

#define SENTINEL ( 0xDEADBEEFu )

static bool accepts(
    uint32_t baud,
    uint32_t * reg
)
{
    *reg = SENTINEL;
    return uarte_baud_to_reg( baud, reg );
}

/* The rate the hardware runs at for a register value. */
static double actual_baud( uint32_t reg )
{
    return ( ( double )reg * 16000000.0 ) / 4294967296.0;
}

static void test_known_registers( void )
{
    uint32_t reg;

    CHECK( accepts( 1200, &reg ) );
    CHECK_EQ( reg, 0x0004F000u );
    CHECK( accepts( 9600, &reg ) ); /* the rate hal_uart starts from */
    CHECK_EQ( reg, 0x00275000u );
    CHECK( accepts( 250000, &reg ) );
    CHECK_EQ( reg, 0x04000000u );
    CHECK( accepts( 1000000, &reg ) );
    CHECK_EQ( reg, 0x10000000u );
}

/* The ECU's rate: 1953 baud is the 1953.125 of the SSM1 line, and it is a
 * register value the hardware holds exactly. */
static void test_the_ecu_rate_is_exact( void )
{
    uint32_t reg;

    CHECK( accepts( 1953, &reg ) );
    CHECK_EQ( reg, 0x00080000u );
    CHECK( actual_baud( reg ) > 1953.12 );
    CHECK( actual_baud( reg ) < 1953.13 );
}

static void test_bad_requests_are_refused_and_leave_the_output_alone( void )
{
    uint32_t reg = SENTINEL;

    CHECK( !uarte_baud_to_reg( 9600, NULL ) );
    CHECK( !accepts( 0, &reg ) );
    CHECK_EQ( reg, SENTINEL );
    CHECK( !accepts( 1000001u, &reg ) );
    CHECK_EQ( reg, SENTINEL );
    CHECK( !accepts( UINT32_MAX, &reg ) );
    CHECK_EQ( reg, SENTINEL );
    /* Too slow for the register to hold: it rounds to nothing, or lands
     * further from the request than the hardware could tolerate. */
    CHECK( !accepts( 1, &reg ) );
    CHECK_EQ( reg, SENTINEL );
    CHECK( !accepts( 10, &reg ) );
    CHECK_EQ( reg, SENTINEL );
    CHECK( !accepts( 300, &reg ) ); /* the nearest step is 1.7 percent off */
    CHECK_EQ( reg, SENTINEL );
    CHECK( accepts( 500, &reg ) ); /* and this one is 0.7 percent off */
}

/* Whatever it accepts is within 1 percent of the request and sits on the
 * hardware's step, and a faster request never gets a smaller register. */
static void test_every_accepted_rate_is_close_and_in_order( void )
{
    uint32_t previous = 0;
    unsigned accepted = 0;

    for ( uint32_t baud = 1; baud <= 1000000u;
          baud += ( baud < 20000u ) ? 1u : 97u )
    {
        uint32_t reg;

        if ( accepts( baud, &reg ) )
        {
            double err = actual_baud( reg ) - ( double )baud;

            accepted++;
            CHECK( ( reg & 0xFFFu ) == 0u );
            CHECK( err <= ( double )baud * 0.01 );
            CHECK( -err <= ( double )baud * 0.01 );
            CHECK( reg >= previous );
            previous = reg;
        }
    }
    CHECK( accepted > 10000u ); /* it is not refusing everything */
}

int main( void )
{
    test_known_registers();
    test_the_ecu_rate_is_exact();
    test_bad_requests_are_refused_and_leave_the_output_alone();
    test_every_accepted_rate_is_close_and_in_order();
    return MT_RESULT();
}
