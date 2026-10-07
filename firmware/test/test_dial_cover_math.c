/* The cover's integer square root: exact for every kind of input, including
 * both sides of the 32-bit boundary where it switches code paths. The
 * reference is a plain search, which shares nothing with the real thing. */
#include <stdint.h>

#include "lib/ui/dial_cover.c"
#include "test/minitest.h"

/* The largest r with r * r <= v, found by counting up. Slow, so it is used
 * on a spread of values and a few thousand neighbors, not on all of them. */
static int32_t reference_root( uint64_t v )
{
    uint64_t r = 0u;

    while ( ( ( r + 1u ) * ( r + 1u ) ) <= v )
    {
        r++;
    }
    return ( int32_t )r;
}

static void check_value( uint64_t v )
{
    CHECK_EQ( isqrt_u64( v ), reference_root( v ) );
}

/* Perfect squares and the numbers just before and after them. */
static void test_squares_and_their_neighbors( void )
{
    for ( uint64_t r = 0u; r < 2000u; r++ )
    {
        uint64_t sq = r * r;

        check_value( sq );
        check_value( sq + 1u );
        if ( sq > 0u )
        {
            check_value( sq - 1u );
        }
    }
}

/* Around 2^32, where the code changes path. The reference counts up from
 * zero, so it is checked against roots of known squares instead. */
static void test_the_32_bit_boundary( void )
{
    const uint64_t edge = ( uint64_t )UINT32_MAX;

    CHECK_EQ( isqrt_u64( edge - 1u ), 65535 );
    CHECK_EQ( isqrt_u64( edge ), 65535 );
    CHECK_EQ( isqrt_u64( edge + 1u ), 65536 );
    CHECK_EQ( isqrt_u64( edge + 2u ), 65536 );
    /* 65536^2 is 2^32; 65537^2 is the next square. */
    CHECK_EQ( isqrt_u64( ( 65537ull * 65537ull ) - 1u ), 65536 );
    CHECK_EQ( isqrt_u64( 65537ull * 65537ull ), 65537 );
    CHECK_EQ( isqrt_u64( ( 65535ull * 65535ull ) + 1u ), 65535 );
}

/* The largest values the cover can produce: a few hundred thousand squared. */
static void test_large_values_against_known_squares( void )
{
    for ( uint64_t r = 200000u; r < 600000u; r += 7919u )
    {
        uint64_t sq = r * r;

        CHECK_EQ( isqrt_u64( sq ), ( int32_t )r );
        CHECK_EQ( isqrt_u64( sq + ( r * 2u ) ), ( int32_t )r );
        CHECK_EQ( isqrt_u64( sq + ( r * 2u ) + 1u ), ( int32_t )( r + 1u ) );
        CHECK_EQ( isqrt_u64( sq - 1u ), ( int32_t )( r - 1u ) );
    }
}

/* A spread of values across the whole range the 32-bit path covers. */
static void test_a_spread_of_small_values( void )
{
    for ( uint64_t v = 0u; v < 3000000u; v += 997u )
    {
        check_value( v );
    }
}

int main( void )
{
    test_squares_and_their_neighbors();
    test_the_32_bit_boundary();
    test_large_values_against_known_squares();
    test_a_spread_of_small_values();
    return MT_RESULT();
}
