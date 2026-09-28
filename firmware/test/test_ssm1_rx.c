#include <string.h>

#include "minitest.h"
#include "ssm1_rx.h"

typedef struct
{
    uint8_t data[512];
    unsigned n;
} out_t;

static void feed(
    ssm1_rx_t * rx,
    const uint8_t * b,
    size_t n,
    out_t * out
)
{
    for ( size_t i = 0; i < n; i++ )
    {
        uint8_t d;
        if ( ssm1_rx_feed( rx, b[i], &d )
             && ( out->n < sizeof( out->data ) ) )
        {
            out->data[out->n++] = d;
        }
    }
}

static void feed_frame(
    ssm1_rx_t * rx,
    uint8_t hi,
    uint8_t lo,
    uint8_t d,
    out_t * out
)
{
    const uint8_t f[3] = { hi, lo, d };
    feed( rx, f, 3, out );
}

static bool all_equal(
    const out_t * o,
    uint8_t v
)
{
    bool ok = true;
    for ( unsigned i = 0; i < o->n; i++ )
    {
        ok = ok && ( o->data[i] == v );
    }
    return ok;
}

static void test_clean_stream( void )
{
    ssm1_rx_t rx;
    out_t o = { { 0 }, 0 };

    ssm1_rx_init( &rx );
    ssm1_rx_expect( &rx, 0x1234 );
    feed_frame( &rx, 0x12, 0x34, 0x55, &o );
    CHECK_EQ( o.n, 0 ); /* held until the next frame confirms it */
    feed_frame( &rx, 0x12, 0x34, 0x56, &o );
    CHECK_EQ( o.n, 1 );
    CHECK_EQ( o.data[0], 0x55 );
    feed_frame( &rx, 0x12, 0x34, 0x57, &o );
    CHECK_EQ( o.n, 2 );
    CHECK_EQ( o.data[1], 0x56 );
}

/* For many address shapes, garbage prefixes, and data values: whatever is
 * reported must be right, and lock must be reached quickly. */
static void test_garbage_prefix_exhaustive( void )
{
    const uint8_t his[] = { 0x00, 0x12, 0xAB, 0xFF, 0x78 };
    const uint8_t los[] = { 0x00, 0x12, 0xAB, 0xFF, 0x08, 0x78 };
    unsigned bad = 0, unlocked = 0;

    for ( unsigned h = 0; h < sizeof( his ); h++ )
    {
        for ( unsigned l = 0; l < sizeof( los ); l++ )
        {
            for ( unsigned prefix = 0; prefix <= 5; prefix++ )
            {
                for ( unsigned d = 0; d < 256; d++ )
                {
                    ssm1_rx_t rx;
                    out_t o   = { { 0 }, 0 };
                    uint8_t g = ( his[h] == 0x55 ) ? 0x56 : 0x55;

                    ssm1_rx_init( &rx );
                    ssm1_rx_expect(
                        &rx, ( uint16_t )( ( his[h] << 8 ) | los[l] )
                    );
                    for ( unsigned i = 0; i < prefix; i++ )
                    {
                        feed( &rx, &g, 1, &o );
                    }
                    for ( unsigned f = 0; f < 6; f++ )
                    {
                        feed_frame( &rx, his[h], los[l], ( uint8_t )d, &o );
                    }
                    bad += !all_equal( &o, ( uint8_t )d );
                    unlocked += ( o.n < 3 );
                }
            }
        }
    }
    CHECK_EQ( bad, 0 );
    CHECK_EQ( unlocked, 0 );
}

static void test_lost_byte_recovers( void )
{
    for ( unsigned pos = 0; pos < 3; pos++ )
    {
        ssm1_rx_t rx;
        out_t o = { { 0 }, 0 };
        uint8_t stream[3 * 8];
        unsigned n = 0;

        for ( unsigned f = 0; f < 8; f++ )
        {
            stream[n++] = 0x12;
            stream[n++] = 0x34;
            stream[n++] = ( uint8_t )( 0x40 + f );
        }
        /* drop one byte from frame 3 */
        memmove(
            &stream[9 + pos], &stream[9 + pos + 1], n - ( 9 + pos + 1 )
        );
        n--;

        ssm1_rx_init( &rx );
        ssm1_rx_expect( &rx, 0x1234 );
        feed( &rx, stream, n, &o );

        CHECK( o.n >= 2 ); /* frames before the loss, and recovery after */
        CHECK(
            o.data[o.n - 1] == 0x46
        ); /* recovered (0x47 awaits a successor) */
    }
}

/* Regression: losing a data byte slides the next msb into the data slot,
 * and inserted junk can land there too. Whatever the position, no value
 * outside the real stream may ever be reported. */
static void test_never_reports_a_wrong_value( void )
{
    const uint8_t junks[] = { 0x12, 0x34, 0xEE, 0x00 };
    unsigned wrong = 0, runs = 0, starved = 0;

    for ( unsigned mode = 0; mode < 1 + sizeof( junks ); mode++ )
    {
        for ( unsigned pos = 0; pos < 30; pos++ )
        {
            uint8_t stream[3 * 10 + 1];
            unsigned n = 0;
            ssm1_rx_t rx;
            out_t o = { { 0 }, 0 };

            for ( unsigned f = 0; f < 10; f++ )
            {
                stream[n++] = 0x12;
                stream[n++] = 0x34;
                stream[n++] = ( uint8_t )( 0x40 + f );
            }
            if ( mode == 0 ) /* drop the byte at pos */
            {
                memmove( &stream[pos], &stream[pos + 1], n - pos - 1 );
                n--;
            }
            else /* insert a junk byte at pos */
            {
                memmove( &stream[pos + 1], &stream[pos], n - pos );
                stream[pos] = junks[mode - 1];
                n++;
            }

            ssm1_rx_init( &rx );
            ssm1_rx_expect( &rx, 0x1234 );
            feed( &rx, stream, n, &o );

            for ( unsigned i = 0; i < o.n; i++ )
            {
                wrong += ( o.data[i] < 0x40 ) || ( o.data[i] > 0x49 );
            }
            starved +=
                ( o.n < 5 ); /* must still deliver most of the stream */
            runs++;
        }
    }
    CHECK( runs > 0 );
    CHECK_EQ( wrong, 0 );
    CHECK_EQ( starved, 0 );
}

static void test_inserted_byte_recovers( void )
{
    ssm1_rx_t rx;
    out_t o            = { { 0 }, 0 };
    const uint8_t junk = 0xEE;

    ssm1_rx_init( &rx );
    ssm1_rx_expect( &rx, 0x1234 );
    for ( unsigned f = 0; f < 4; f++ )
    {
        feed_frame( &rx, 0x12, 0x34, ( uint8_t )( 0x40 + f ), &o );
    }
    feed( &rx, &junk, 1, &o );
    for ( unsigned f = 4; f < 9; f++ )
    {
        feed_frame( &rx, 0x12, 0x34, ( uint8_t )( 0x40 + f ), &o );
    }
    CHECK( o.data[o.n - 1] == 0x47 );
}

static void test_parity_error_drops_and_recovers( void )
{
    ssm1_rx_t rx;
    out_t o              = { { 0 }, 0 };
    const uint8_t two[2] = { 0x12, 0x34 };

    ssm1_rx_init( &rx );
    ssm1_rx_expect( &rx, 0x1234 );
    feed_frame( &rx, 0x12, 0x34, 0x40, &o );
    feed_frame( &rx, 0x12, 0x34, 0x41, &o );
    CHECK_EQ( o.n, 1 );
    CHECK_EQ( o.data[0], 0x40 );

    feed( &rx, two, 2, &o );
    ssm1_rx_error( &rx ); /* third byte arrived with bad parity */
    for ( unsigned f = 0; f < 4; f++ )
    {
        feed_frame( &rx, 0x12, 0x34, ( uint8_t )( 0x50 + f ), &o );
    }
    CHECK( o.data[o.n - 1] == 0x52 );
    /* nothing from the corrupted frame was reported */
    for ( unsigned i = 0; i < o.n; i++ )
    {
        CHECK( ( o.data[i] == 0x40 ) || ( o.data[i] >= 0x50 ) );
    }
}

static void test_command_echo_prefix( void )
{
    ssm1_rx_t rx;
    out_t o               = { { 0 }, 0 };
    const uint8_t echo[4] = { 0x78, 0x12, 0x34, 0x00 };

    ssm1_rx_init( &rx );
    ssm1_rx_expect( &rx, 0x1234 );
    feed( &rx, echo, 4, &o );
    for ( unsigned f = 0; f < 4; f++ )
    {
        feed_frame( &rx, 0x12, 0x34, ( uint8_t )( 0x60 + f ), &o );
    }
    CHECK( o.n >= 2 );
    CHECK( o.data[o.n - 1] == 0x62 );
}

static void test_other_address_ignored( void )
{
    ssm1_rx_t rx;
    out_t o = { { 0 }, 0 };

    ssm1_rx_init( &rx );
    ssm1_rx_expect( &rx, 0x1234 );
    for ( unsigned f = 0; f < 20; f++ )
    {
        feed_frame(
            &rx, 0x00, 0x08, ( uint8_t )f, &o
        ); /* stale old stream */
    }
    CHECK_EQ( o.n, 0 );
    /* and it locks onto the right one once that arrives */
    for ( unsigned f = 0; f < 3; f++ )
    {
        feed_frame( &rx, 0x12, 0x34, 0x77, &o );
    }
    CHECK( o.n >= 1 );
    CHECK( all_equal( &o, 0x77 ) );
}

static void test_expect_resets_lock( void )
{
    ssm1_rx_t rx;
    out_t o = { { 0 }, 0 };

    ssm1_rx_init( &rx );
    ssm1_rx_expect( &rx, 0x1234 );
    feed_frame( &rx, 0x12, 0x34, 1, &o );
    feed_frame( &rx, 0x12, 0x34, 2, &o );
    CHECK_EQ( o.n, 1 );

    ssm1_rx_expect( &rx, 0x0008 ); /* switch parameter */
    o.n = 0;
    feed_frame( &rx, 0x12, 0x34, 3, &o ); /* leftovers of the old stream */
    feed_frame( &rx, 0x00, 0x08, 9, &o );
    CHECK_EQ( o.n, 0 ); /* needs a fresh confirmation */
    feed_frame( &rx, 0x00, 0x08, 9, &o );
    CHECK_EQ( o.n, 1 );
}

/* Noise that cannot contain the address's first byte can never produce a
 * sample. */
static void test_noise_never_reports( void )
{
    ssm1_rx_t rx;
    out_t o    = { { 0 }, 0 };
    uint32_t x = 12345;

    ssm1_rx_init( &rx );
    ssm1_rx_expect( &rx, 0x1234 );
    for ( unsigned i = 0; i < 100000; i++ )
    {
        uint8_t b;
        x = x * 1664525u + 1013904223u;
        b = ( uint8_t )( x >> 24 );
        if ( b == 0x12 )
        {
            b = 0x13;
        }
        feed( &rx, &b, 1, &o );
    }
    CHECK_EQ( o.n, 0 );
}

static void test_no_overflow_of_window( void )
{
    ssm1_rx_t rx;
    out_t o    = { { 0 }, 0 };
    uint32_t x = 99;

    ssm1_rx_init( &rx );
    ssm1_rx_expect( &rx, 0x0000 );
    for ( unsigned i = 0; i < 100000; i++ )
    {
        uint8_t b;
        x = x * 1664525u + 1013904223u;
        b = ( uint8_t )( x >> 24 );
        feed( &rx, &b, 1, &o );
        CHECK( rx.len < SSM1_FRAME_LEN );
    }
}

int main( void )
{
    test_clean_stream();
    test_garbage_prefix_exhaustive();
    test_lost_byte_recovers();
    test_never_reports_a_wrong_value();
    test_inserted_byte_recovers();
    test_parity_error_drops_and_recovers();
    test_command_echo_prefix();
    test_other_address_ignored();
    test_expect_resets_lock();
    test_noise_never_reports();
    test_no_overflow_of_window();
    return MT_RESULT();
}
