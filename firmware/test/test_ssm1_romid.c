#include <string.h>

#include "minitest.h"
#include "ssm1_romid.h"

/* Feeds bytes; returns how many IDs were reported, last one in *id. */
static unsigned feed_all(
    ssm1_romid_t * p,
    const uint8_t * b,
    size_t n,
    uint32_t * id
)
{
    unsigned got = 0;

    for ( size_t i = 0; i < n; i++ )
    {
        if ( ssm1_romid_feed( p, b[i], id ) )
        {
            got++;
        }
    }
    return got;
}

static void test_clean_needs_two_matches( void )
{
    ssm1_romid_t p;
    uint32_t id           = 0;
    const uint8_t frame[] = { 0xFF, 0xFE, 0x55, 0x71, 0x36, 0x01 };

    ssm1_romid_init( &p );
    CHECK_EQ( feed_all( &p, frame, 6, &id ), 0 );
    CHECK_EQ( feed_all( &p, frame, 6, &id ), 1 );
    CHECK_EQ( id, 0x013671 );
}

static void test_hunts_through_garbage_and_header_stream( void )
{
    ssm1_romid_t p;
    uint32_t id           = 0;
    const uint8_t noise[] = {
        0x78,
        0xFF,
        0xFE,
        0x00,
        0x00,
        0xFF,
        0xFF,
        0x00,
        0x12,
        0xFF,
        0xFE,
        0x55,
        0xFF,
        0xFE,
        0x55
    };
    const uint8_t frame[] = { 0xFF, 0xFE, 0x55, 0x72, 0x36, 0x02 };
    unsigned got          = 0;

    ssm1_romid_init( &p );
    got += feed_all( &p, noise, sizeof( noise ), &id );
    for ( unsigned i = 0; i < 4; i++ )
    {
        got += feed_all( &p, frame, 6, &id );
    }
    CHECK( got >= 1 );
    CHECK_EQ( id, 0x023672 );
}

static void test_mismatched_candidates_not_reported( void )
{
    ssm1_romid_t p;
    uint32_t id       = 0;
    const uint8_t a[] = { 0xFF, 0xFE, 0x55, 0x71, 0x36, 0x01 };
    const uint8_t b[] = { 0xFF, 0xFE, 0x55, 0x72, 0x36, 0x01 };

    ssm1_romid_init( &p );
    CHECK_EQ( feed_all( &p, a, 6, &id ), 0 );
    CHECK_EQ( feed_all( &p, b, 6, &id ), 0 ); /* a different one between */
    CHECK_EQ( feed_all( &p, a, 6, &id ), 1 ); /* a is now seen twice */
    CHECK_EQ( id, 0x013671 );
}

static void test_echoed_command_is_not_an_id( void )
{
    ssm1_romid_t p;
    uint32_t id          = 0;
    const uint8_t echo[] = { 0xFF, 0xFE, 0x55, 0x00, 0xFF, 0xFF };

    ssm1_romid_init( &p );
    for ( unsigned i = 0; i < 5; i++ )
    {
        CHECK_EQ( feed_all( &p, echo, 6, &id ), 0 );
    }
}

static void test_error_drops_state( void )
{
    ssm1_romid_t p;
    uint32_t id           = 0;
    const uint8_t frame[] = { 0xFF, 0xFE, 0x55, 0x71, 0x36, 0x01 };

    ssm1_romid_init( &p );
    feed_all( &p, frame, 6, &id );
    ssm1_romid_error( &p );
    CHECK_EQ( feed_all( &p, frame, 6, &id ), 0 ); /* candidate forgotten */
    CHECK_EQ( feed_all( &p, frame, 6, &id ), 1 );
}

static void test_r0_ff_rejected( void )
{
    ssm1_romid_t p;
    uint32_t id           = 0;
    const uint8_t frame[] = { 0xFF, 0xFE, 0x55, 0xFF, 0x36, 0x01 };

    ssm1_romid_init( &p );
    CHECK_EQ( feed_all( &p, frame, 6, &id ), 0 );
    CHECK_EQ( feed_all( &p, frame, 6, &id ), 0 );
}

static void test_random_noise_never_reports( void )
{
    ssm1_romid_t p;
    uint32_t id  = 0;
    unsigned x   = 12345;
    unsigned got = 0;

    ssm1_romid_init( &p );
    for ( unsigned i = 0; i < 100000; i++ )
    {
        x = x * 1103515245u + 12345u;
        /* never 0xFF: no header can form */
        got += ssm1_romid_feed( &p, ( uint8_t )( ( x >> 16 ) % 255u ), &id );
    }
    CHECK_EQ( got, 0 );
}

/* The stream of ff fe 55 frames the ECU sends while it answers. One frame in
 * each of two places gets the same defect: a byte dropped, or a junk byte
 * added, at the same spot in the frame. The same false ID then comes round
 * twice, which is what a flaky line does and what must never be confirmed. */
#define FRAMES ( 14u )

static size_t damaged_stream(
    uint8_t * out,
    bool insert,
    size_t pos,
    uint8_t junk
)
{
    const uint8_t frame[3] = { 0xFF, 0xFE, 0x55 };
    size_t n               = 0u;

    for ( size_t f = 0u; f < FRAMES; f++ )
    {
        bool hit = ( f == 3u ) || ( f == 9u );

        for ( size_t i = 0u; i < 3u; i++ )
        {
            if ( hit && insert && ( i == pos ) )
            {
                out[n++] = junk;
            }
            if ( !( hit && !insert && ( i == pos ) ) )
            {
                out[n++] = frame[i];
            }
        }
        if ( hit && insert && ( pos == 3u ) )
        {
            out[n++] = junk;
        }
    }
    return n;
}

static void test_a_stream_with_a_byte_lost_or_added_is_not_an_id( void )
{
    uint8_t buf[FRAMES * 4u];
    const uint8_t junks[] = { 0x00, 0x55, 0xFE, 0x12, 0xA7 };

    for ( size_t pos = 0u; pos < 3u; pos++ )
    {
        ssm1_romid_t p;
        uint32_t id = 0u;
        size_t n    = damaged_stream( buf, false, pos, 0u );

        ssm1_romid_init( &p );
        CHECK_EQ( feed_all( &p, buf, n, &id ), 0 );
    }
    for ( size_t pos = 0u; pos < 4u; pos++ )
    {
        for ( size_t j = 0u; j < sizeof( junks ); j++ )
        {
            ssm1_romid_t p;
            uint32_t id = 0u;
            size_t n    = damaged_stream( buf, true, pos, junks[j] );

            ssm1_romid_init( &p );
            CHECK_EQ( feed_all( &p, buf, n, &id ), 0 );
        }
    }
}

/* The real reply is still found after such damage. */
static void test_a_real_id_after_a_damaged_stream_is_found( void )
{
    uint8_t buf[FRAMES * 4u];
    const uint8_t frame[] = { 0xFF, 0xFE, 0x55, 0x71, 0x36, 0x01 };

    for ( size_t pos = 0u; pos < 3u; pos++ )
    {
        ssm1_romid_t p;
        uint32_t id = 0u;
        size_t n    = damaged_stream( buf, false, pos, 0u );
        unsigned got;

        ssm1_romid_init( &p );
        got = feed_all( &p, buf, n, &id );
        got += feed_all( &p, frame, sizeof( frame ), &id );
        got += feed_all( &p, frame, sizeof( frame ), &id );
        CHECK_EQ( got, 1 );
        CHECK_EQ( id, 0x013671 );
    }
}

int main( void )
{
    test_clean_needs_two_matches();
    test_hunts_through_garbage_and_header_stream();
    test_mismatched_candidates_not_reported();
    test_echoed_command_is_not_an_id();
    test_error_drops_state();
    test_r0_ff_rejected();
    test_random_noise_never_reports();
    test_a_stream_with_a_byte_lost_or_added_is_not_an_id();
    test_a_real_id_after_a_damaged_stream_is_found();
    return MT_RESULT();
}
