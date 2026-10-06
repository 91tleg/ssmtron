/* Session tests: the real ssm1 session against a simulated ECU. */
#include <string.h>

#include "ecu.h"
#include "minitest.h"
#include "ssm1_decode.h"
#include "ssm1_params.h"

#define BYTE_MS ( 6u ) /* ~5.6 ms per 8E1 byte at 1953 baud */
#define ID_ADDR ( 0xFFFEu )

typedef struct
{
    bool alive;
    bool streaming;
    bool echo;
    bool id_continuous; /* ROM ID follows every header, not just on request
                         */
    bool id_pending;
    unsigned ignore_stops; /* stop commands to ignore before obeying */
    unsigned drop_bytes;   /* stream bytes to swallow */
    uint16_t addr;
    uint8_t rom[3];
    uint8_t cmd[4];
    uint8_t cmd_len;
    uint8_t phase; /* 0..2 frame, 3..5 ROM ID bytes */
    uint32_t next_ms;
    bool busy_after_write; /* goes silent for a while after the first write */
    bool muted;            /* hears commands, sends nothing */
    uint32_t mute_end;
    uint32_t busy_ms; /* how long it is busy; 0 is a memory write, 22 s */
    unsigned writes;
    uint8_t write_cmd[8][4];
    uint16_t write_stream_addr[8]; /* what was streaming at the time */
} sim_t;

static sim_t sim;
static uint32_t now;
static unsigned tx_bytes;
static unsigned wrong_values;
static size_t selected;
static uint8_t bias;
static const ssm1_profile_t * prof;

static uint8_t mem( uint16_t addr )
{
    return ( uint8_t )( ( addr * 7u + 3u + bias ) & 0xFFu );
}

static int32_t expected( size_t idx )
{
    return ssm1_decode( prof->params[idx].info, mem( prof->params[idx].addr ) );
}

static void sim_rx( uint8_t b )
{
    if ( sim.alive )
    {
        if ( sim.echo )
        {
            ecu_feed( b );
        }
        sim.cmd[sim.cmd_len++] = b;
        if ( sim.cmd_len == 4 )
        {
            sim.cmd_len = 0;
            if ( sim.cmd[0] == 0x12 )
            {
                if ( sim.ignore_stops > 0 )
                {
                    sim.ignore_stops--;
                }
                else
                {
                    sim.streaming = false;
                }
            }
            else if ( sim.cmd[0] == 0x78 )
            {
                sim.streaming = true;
                sim.addr    = ( uint16_t )( ( sim.cmd[1] << 8 ) | sim.cmd[2] );
                sim.phase   = 0;
                sim.next_ms = now + BYTE_MS;
            }
            else if ( ( sim.cmd[0] == 0x00 ) && ( sim.cmd[1] == 0xFF )
                      && ( sim.cmd[2] == 0xFF ) )
            {
                sim.id_pending = sim.streaming; /* only while streaming */
            }
            else if ( sim.cmd[0] == 0xAA )
            {
                if ( ( sim.streaming || sim.muted ) && ( sim.writes < 8 ) )
                {
                    memcpy( sim.write_cmd[sim.writes], sim.cmd, 4 );
                    sim.write_stream_addr[sim.writes] = sim.addr;
                    sim.writes++;
                    if ( sim.busy_after_write && ( sim.writes == 1u ) )
                    {
                        sim.muted = true;
                        sim.mute_end =
                            now
                            + ( ( sim.busy_ms != 0u ) ? sim.busy_ms : 22000u );
                    }
                }
            }
        }
    }
}

static void sim_stream( void )
{
    if ( sim.muted && ( ( int32_t )( now - sim.mute_end ) >= 0 ) )
    {
        sim.muted = false;
    }
    if ( sim.alive && sim.streaming && !sim.muted
         && ( ( int32_t )( now - sim.next_ms ) >= 0 ) )
    {
        uint8_t b = 0;

        if ( sim.phase == 0 )
        {
            b = ( uint8_t )( sim.addr >> 8 );
        }
        else if ( sim.phase == 1 )
        {
            b = ( uint8_t )( sim.addr & 0xFF );
        }
        else if ( sim.phase == 2 )
        {
            b = ( sim.addr == ID_ADDR ) ? 0x55u : mem( sim.addr );
        }
        else
        {
            b = sim.rom[sim.phase - 3];
        }

        if ( sim.drop_bytes > 0 )
        {
            sim.drop_bytes--;
        }
        else
        {
            ecu_feed( b );
        }
        if ( ( sim.phase == 2 ) && ( sim.addr == ID_ADDR )
             && ( sim.id_pending || sim.id_continuous ) )
        {
            sim.phase = 3;
        }
        else if ( sim.phase == 5 )
        {
            sim.id_pending = false;
            sim.phase      = 0;
        }
        else
        {
            sim.phase = ( sim.phase < 2 )    ? ( uint8_t )( sim.phase + 1 )
                        : ( sim.phase == 2 ) ? 0u
                                             : ( uint8_t )( sim.phase + 1 );
        }
        sim.next_ms += BYTE_MS;
    }
}

/* One millisecond of the app loop. Also checks that a reported value is
 * never wrong. */
static void step( void )
{
    uint8_t buf[8];
    size_t n;
    int32_t v;

    ecu_tick( now );
    sim_stream();
    n = ecu_next_tx( buf, sizeof( buf ) );
    for ( size_t i = 0; i < n; i++ )
    {
        sim_rx( buf[i] );
    }
    tx_bytes += ( unsigned )n;

    if ( ( prof != NULL ) && ( selected < prof->count )
         && ecu_get_value( selected, &v ) && ( v != expected( selected ) ) )
    {
        wrong_values++;
    }
    now++;
}

static void run( unsigned ms )
{
    for ( unsigned i = 0; i < ms; i++ )
    {
        step();
    }
}

/* A fresh session against an ECU with this ROM ID. */
static void start_id(
    uint32_t t0,
    uint16_t rom_id
)
{
    sim          = ( sim_t ){ 0 };
    sim.alive    = true;
    sim.rom[0]   = ( uint8_t )( rom_id & 0xFF );
    sim.rom[1]   = ( uint8_t )( rom_id >> 8 );
    sim.rom[2]   = 0x01;
    bias         = 0;
    tx_bytes     = 0;
    wrong_values = 0;
    selected     = 0;
    prof         = ssm1_profile_find( rom_id );
    now          = t0;
    ecu_init();
    ecu_tick( now );
}

/* The 92 NA is the default test car. */
static void start( uint32_t t0 )
{
    start_id( t0, 0x3272 );
}

/* Identified and reading parameter 0. */
static void boot( void )
{
    start( 0 );
    run( 2500 );
}

static bool value_ok( size_t idx )
{
    int32_t v;

    return ecu_get_value( idx, &v ) && ( v == expected( idx ) );
}

static void test_initial_state( void )
{
    int32_t v;
    uint8_t b[8];

    start( 0 );
    CHECK( ecu_link() == ECU_LINK_DOWN );
    CHECK( ecu_name() == NULL );
    CHECK_EQ( ecu_id(), 0 );
    {
        char text[16];

        ecu_id_text( text, sizeof( text ) );
        CHECK( strcmp( text, "--.--.--" ) == 0 );
    }
    CHECK_EQ( ecu_param_count(), 0 );
    CHECK( ecu_param_info( 0 ) == NULL );
    CHECK( !ecu_select_param( 0 ) );
    CHECK( !ecu_get_value( 0, &v ) );
    CHECK( !ecu_clear_start() );
    CHECK( ecu_clear_state() == ECU_CLEAR_IDLE );
    CHECK_EQ( ecu_link_config()->baud, 1953 );
    CHECK( ecu_link_config()->parity == ECU_PARITY_EVEN );
    CHECK_EQ( ecu_link_config()->data_bits, 8 );
    CHECK_EQ( ecu_link_config()->stop_bits, 1 );
    CHECK_EQ( ecu_next_tx( b, 8 ), 4 ); /* identification starts by itself */
}

static void test_command_sequence( void )
{
    uint8_t b[8];

    start( 0 );
    CHECK_EQ( ecu_next_tx( b, 2 ), 0 ); /* too small: stays queued */
    CHECK_EQ( ecu_next_tx( b, 8 ), 4 );
    CHECK(
        ( b[0] == 0x12 ) && ( b[1] == 0 ) && ( b[2] == 0 ) && ( b[3] == 0 )
    );
    CHECK_EQ( ecu_next_tx( b, 8 ), 0 ); /* nothing until the line is quiet */

    ecu_tick( 40 );
    CHECK_EQ( ecu_next_tx( b, 8 ), 0 );
    ecu_tick( 80 );
    CHECK_EQ( ecu_next_tx( b, 8 ), 4 ); /* read the ROM ID address */
    CHECK(
        ( b[0] == 0x78 ) && ( b[1] == 0xFF ) && ( b[2] == 0xFE )
        && ( b[3] == 0 )
    );
    CHECK_EQ( ecu_next_tx( b, 8 ), 4 ); /* then ask for the ROM ID */
    CHECK(
        ( b[0] == 0x00 ) && ( b[1] == 0xFF ) && ( b[2] == 0xFF )
        && ( b[3] == 0 )
    );
    CHECK_EQ( ecu_next_tx( b, 8 ), 0 );
    ecu_tick( 80 + 299 );
    CHECK_EQ( ecu_next_tx( b, 8 ), 0 );
    ecu_tick( 80 + 300 ); /* unanswered: asks again */
    CHECK_EQ( ecu_next_tx( b, 8 ), 4 );
    CHECK( b[0] == 0x00 );
}

static void test_identifies_all_six( void )
{
    const uint16_t ids[] = { 0x3671, 0x3672, 0x3170, 0x3270, 0x3272, 0x3273 };

    for ( size_t k = 0; k < 6; k++ )
    {
        start_id( 0, ids[k] );
        run( 2500 );
        CHECK( ecu_name() != NULL );
        CHECK( strcmp( ecu_name(), prof->name ) == 0 );
        CHECK_EQ( ecu_id(), 0x010000u | ids[k] );
        CHECK_EQ( ecu_param_count(), prof->count );
        CHECK( ecu_param_info( 0 ) == prof->params[0].info );
        CHECK( ecu_param_info( prof->count ) == NULL );
        CHECK( ecu_link() == ECU_LINK_UP );
        CHECK( value_ok( 0 ) ); /* parameter 0 is read on its own */
        CHECK_EQ( wrong_values, 0 );
    }
}

static void test_every_parameter_of_every_profile_reads( void )
{
    const uint16_t ids[] = { 0x3671, 0x3672, 0x3170, 0x3270, 0x3272, 0x3273 };

    for ( size_t k = 0; k < 6; k++ )
    {
        start_id( 0, ids[k] );
        run( 2500 );
        for ( size_t i = 0; i < prof->count; i++ )
        {
            selected = i;
            CHECK( ecu_select_param( i ) );
            run( 700 );
            CHECK( value_ok( i ) );
        }
        CHECK_EQ( wrong_values, 0 );
        CHECK( !ecu_select_param( prof->count ) );
    }
}

/* A remembered profile shows its parameters before the ECU has answered,
 * but it is only a guess: nothing is reported as identified and the clear,
 * which writes to an address from the profile, refuses. */
static void test_assumed_profile_shows_pages_but_is_not_identified( void )
{
    start_id( 0, 0x3672 );
    CHECK_EQ( ecu_assume_profile( 0x3672 ), 0x3672u );
    CHECK_EQ( ecu_param_count(), prof->count );
    CHECK( ecu_param_info( 0 ) == prof->params[0].info );
    CHECK( ecu_select_param( 3 ) );
    CHECK( ecu_name() == NULL );
    CHECK_EQ( ecu_id(), 0 );
    CHECK( !ecu_clear_start() );
}

/* The same ECU answers: the page chosen meanwhile is kept, and the clear
 * is allowed now that the ECU has identified itself. */
static void test_assumed_profile_confirmed_keeps_the_selection( void )
{
    start_id( 0, 0x3672 );
    ( void )ecu_assume_profile( 0x3672 );
    selected = 3;
    CHECK( ecu_select_param( 3 ) );
    run( 2500 );
    CHECK( ecu_name() != NULL );
    CHECK_EQ( ecu_id(), 0x013672u );
    {
        char text[16];
        char tiny[1] = { 'x' };
        char small[5];

        /* r0.r1.r2: the bytes in the order the ECU sent them. */
        ecu_id_text( text, sizeof( text ) );
        CHECK( strcmp( text, "72.36.01" ) == 0 );
        /* A short buffer is cut and still ended; size 0 is left alone. */
        ecu_id_text( small, sizeof( small ) );
        CHECK( strcmp( small, "72.3" ) == 0 );
        ecu_id_text( tiny, sizeof( tiny ) );
        CHECK_EQ( tiny[0], '\0' );
        tiny[0] = 'x';
        ecu_id_text( tiny, 0u );
        CHECK_EQ( tiny[0], 'x' );
    }
    CHECK( value_ok( 3 ) ); /* parameter 3 is being read, not parameter 0 */
    CHECK_EQ( wrong_values, 0 );
    CHECK( ecu_clear_start() );
}

/* A different ECU answers: its own pages replace the assumed ones and the
 * selection returns to the first parameter. */
static void test_assumed_profile_replaced_by_the_real_one( void )
{
    start_id( 0, 0x3272 ); /* the real ECU is a 92 NA */
    CHECK_EQ( ecu_assume_profile( 0x3672 ), 0x3672u ); /* a turbo was assumed */
    CHECK( ecu_select_param( 3 ) );
    selected = 0;
    run( 2500 );
    CHECK( strcmp( ecu_name(), prof->name ) == 0 );
    CHECK_EQ( ecu_param_count(), prof->count );
    CHECK( ecu_param_info( 0 ) == prof->params[0].info );
    CHECK( value_ok( 0 ) );
    CHECK_EQ( wrong_values, 0 );
}

static void test_assuming_an_unknown_key_uses_the_first_profile( void )
{
    start_id( 0, 0x3272 );
    CHECK_EQ( ecu_assume_profile( 0x9999 ), ssm1_profile( 0 )->rom_id );
    CHECK_EQ( ecu_param_count(), ssm1_profile( 0 )->count );
}

/* No ECU ever answers: the assumed pages stay, with no data and no link. */
static void test_assumed_profile_stays_when_the_ecu_is_silent( void )
{
    int32_t v;

    start_id( 0, 0x3672 );
    sim.alive = false;
    ( void )ecu_assume_profile( 0x3672 );
    run( 5000 );
    CHECK_EQ( ecu_param_count(), prof->count );
    CHECK( !ecu_get_value( 0, &v ) );
    CHECK( ecu_link() == ECU_LINK_DOWN );
    CHECK( !ecu_clear_start() );
}

static void test_unknown_rom_id( void )
{
    int32_t v;

    start_id( 0, 0x9999 );
    run( 2500 );
    CHECK( ecu_name() != NULL );
    CHECK( strcmp( ecu_name(), "Unknown" ) == 0 );
    CHECK_EQ( ecu_id(), 0x019999u );
    CHECK_EQ( ecu_param_count(), 0 );
    CHECK( !ecu_select_param( 0 ) );
    CHECK( !ecu_get_value( 0, &v ) );
    CHECK( !ecu_clear_start() );
}

/* The identity text spells every hex digit in capitals, even for an ECU the
 * tables do not know. */
static void test_id_text_uses_upper_case_hex( void )
{
    char text[16];

    start_id( 0, 0xABCD );
    run( 2500 );
    CHECK_EQ( ecu_id(), 0x01ABCDu );
    ecu_id_text( text, sizeof( text ) );
    CHECK( strcmp( text, "CD.AB.01" ) == 0 );
}

static void test_id_streamed_continuously( void )
{
    start( 0 );
    sim.id_continuous = true;
    run( 2500 );
    CHECK( ecu_name() != NULL );
    CHECK( value_ok( 0 ) );
}

static void test_id_survives_lost_bytes_and_noise( void )
{
    start( 0 );
    sim.drop_bytes = 5;
    run( 150 );
    for ( uint8_t i = 0; i < 20; i++ )
    {
        ecu_feed( ( uint8_t )( 0x01 + i ) );
    }
    ecu_feed_error();
    run( 3500 );
    CHECK( ecu_name() != NULL );
    CHECK( strcmp( ecu_name(), prof->name ) == 0 );
    CHECK( value_ok( 0 ) );
    CHECK_EQ( wrong_values, 0 );
}

static void test_id_with_echo( void )
{
    start( 0 );
    sim.echo = true;
    run( 3000 );
    CHECK( ecu_name() != NULL );
    CHECK( strcmp( ecu_name(), prof->name ) == 0 );
    CHECK( value_ok( 0 ) );
    CHECK_EQ( wrong_values, 0 );
}

static void test_ecu_powers_up_late( void )
{
    start( 0 );
    sim.alive = false;
    run( 6000 );
    CHECK( ecu_name() == NULL );
    CHECK( ecu_link() == ECU_LINK_DOWN );
    sim.alive = true;
    run( 4000 );
    CHECK( ecu_name() != NULL );
    CHECK( value_ok( 0 ) );
}

static void test_switch_parameter( void )
{
    int32_t v;

    boot();
    CHECK( value_ok( 0 ) );

    selected = 1;
    CHECK( ecu_select_param( 1 ) );
    run( 600 );
    CHECK( value_ok( 1 ) );
    CHECK( !ecu_get_value( 0, &v ) ); /* old parameter goes stale */
    CHECK_EQ( wrong_values, 0 );

    for ( unsigned i = 0; i < 10; i++ ) /* rapid button mashing */
    {
        selected = i % ecu_param_count();
        ecu_select_param( selected );
        run( 30 );
    }
    run( 800 );
    CHECK( value_ok( selected ) );
    CHECK_EQ( wrong_values, 0 );
}

static void test_reselect_same_is_noop( void )
{
    boot();
    {
        unsigned before = tx_bytes;
        ecu_select_param( 0 );
        run( 200 );
        CHECK_EQ( tx_bytes, before ); /* no restart */
        CHECK( value_ok( 0 ) );
    }
}

static void test_value_tracks_changes( void )
{
    boot();
    bias = 10;
    run( 100 ); /* the last old-bias value is legitimately shown briefly */
    CHECK( value_ok( 0 ) );
}

static void test_reads_value( void )
{
    boot();
    CHECK( ecu_link() == ECU_LINK_UP );
    CHECK( value_ok( 0 ) );
    CHECK_EQ( wrong_values, 0 );
}

static void test_lost_byte_recovers( void )
{
    boot();
    sim.drop_bytes = 1;
    run( 300 );
    CHECK( ecu_link() == ECU_LINK_UP );
    CHECK( value_ok( 0 ) );
    CHECK_EQ( wrong_values, 0 );
}

static void test_noise_burst_recovers( void )
{
    boot();
    for ( uint8_t i = 0; i < 20; i++ )
    {
        /* never the address's msb (0x13) */
        ecu_feed( ( uint8_t )( 0x81 + i ) );
    }
    run( 300 );
    CHECK( value_ok( 0 ) );
    CHECK_EQ( wrong_values, 0 );
}

static void test_parity_errors_recover( void )
{
    boot();
    for ( unsigned i = 0; i < 10; i++ )
    {
        ecu_feed_error();
        run( 7 );
    }
    run( 300 );
    CHECK( ecu_link() == ECU_LINK_UP );
    CHECK( value_ok( 0 ) );
    CHECK_EQ( wrong_values, 0 );
}

static void test_command_echo( void )
{
    start( 0 );
    sim.echo = true;
    run( 3000 );
    selected = 2;
    ecu_select_param( 2 );
    run( 800 );
    CHECK( ecu_link() == ECU_LINK_UP );
    CHECK( value_ok( 2 ) );
    CHECK_EQ( wrong_values, 0 );
}

/* ECU is already streaming from an earlier session and ignores the first
 * stop. */
static void test_ecu_already_streaming_and_ignores_stop( void )
{
    start( 0 );
    sim.streaming    = true;
    sim.addr         = prof->params[1].addr;
    sim.ignore_stops = 1;
    sim.next_ms      = 0;
    run( 6000 );
    CHECK( ecu_name() != NULL );
    CHECK( ecu_link() == ECU_LINK_UP );
    CHECK( value_ok( 0 ) );
    CHECK_EQ( wrong_values, 0 );
}

static void test_absent_ecu_backs_off( void )
{
    start( 0 );
    sim.alive = false;
    run( 10000 );
    CHECK( ecu_link() == ECU_LINK_DOWN );
    CHECK( tx_bytes > 0 );   /* it did try */
    CHECK( tx_bytes < 200 ); /* but not a flood */
}

static void test_ecu_dies_and_returns( void )
{
    int32_t v;

    boot();
    CHECK( ecu_link() == ECU_LINK_UP );

    sim.alive = false;
    run( 2500 );
    CHECK( ecu_link() == ECU_LINK_DOWN );
    CHECK( !ecu_get_value( 0, &v ) ); /* stale, not shown as current */

    sim.alive     = true;
    sim.streaming = false;
    run( 4000 );
    CHECK( ecu_link() == ECU_LINK_UP );
    CHECK( value_ok( 0 ) );
    CHECK_EQ( wrong_values, 0 );
}

static void test_millis_wraparound( void )
{
    start( 0xFFFFFF00u ); /* wraps 256 ms in */
    run( 3000 );
    CHECK( ecu_name() != NULL );
    CHECK( ecu_link() == ECU_LINK_UP );
    CHECK( value_ok( 0 ) );
    CHECK_EQ( wrong_values, 0 );

    sim.alive = false;
    run( 2500 );
    CHECK( ecu_link() == ECU_LINK_DOWN );
}

static void test_stray_bytes_before_start( void )
{
    start( 0 );
    for ( unsigned i = 0; i < 50; i++ )
    {
        ecu_feed( ( uint8_t )i );
    }
    ecu_feed_error();
    run( 3000 );
    CHECK( ecu_name() != NULL );
    CHECK( value_ok( 0 ) );
    CHECK_EQ( wrong_values, 0 );
}

/* ---- clearing trouble codes -------------------------------------------
 */

static void check_clear_writes( void )
{
    uint8_t want = ( prof->clear_mode == SSM1_CLEAR_ZERO )
                       ? 0xFFu
                       : ( uint8_t )~mem( prof->clear_addr );

    CHECK_EQ( sim.writes, 4 );
    for ( unsigned i = 0; ( i < sim.writes ) && ( i < 8 ); i++ )
    {
        CHECK( sim.write_cmd[i][0] == 0xAA );
        CHECK( sim.write_cmd[i][1] == ( prof->clear_addr >> 8 ) );
        CHECK( sim.write_cmd[i][2] == ( prof->clear_addr & 0xFF ) );
        CHECK( sim.write_cmd[i][3] == want );
        CHECK( sim.write_stream_addr[i] == prof->clear_addr );
    }
}

static void test_clear_every_profile( void )
{
    const uint16_t ids[] = { 0x3671, 0x3672, 0x3170, 0x3270, 0x3272, 0x3273 };

    for ( size_t k = 0; k < 6; k++ )
    {
        start_id( 0, ids[k] );
        run( 2500 );
        selected = 2;
        ecu_select_param( 2 );
        run( 600 );

        CHECK( ecu_clear_start() );
        CHECK( ecu_clear_state() == ECU_CLEAR_BUSY );
        CHECK( !ecu_clear_start() ); /* already running */
        run( 26000 ); /* > write gaps + CLEAR_DONE_WAIT_MS + resume */
        CHECK( ecu_clear_state() == ECU_CLEAR_DONE );
        check_clear_writes();

        /* and monitoring resumes on the parameter that was selected */
        run( 1000 );
        CHECK( value_ok( 2 ) );
        CHECK_EQ( wrong_values, 0 );
    }
}

static void test_clear_write_spacing( void )
{
    uint8_t b[8];
    unsigned writes = 0;
    uint32_t last   = 0;

    boot();
    ecu_clear_start();
    for ( unsigned i = 0; i < 6000; i++ )
    {
        size_t n;

        ecu_tick( now );
        sim_stream();
        n = ecu_next_tx( b, sizeof( b ) );
        for ( size_t j = 0; j < n; j++ )
        {
            sim_rx( b[j] );
        }
        if ( ( n == 4 ) && ( b[0] == 0xAA ) )
        {
            CHECK( ( writes == 0 ) || ( ( now - last ) >= 384 ) );
            last = now;
            writes++;
        }
        now++;
    }
    CHECK_EQ( writes, 4 );
}

static void test_clear_selection_during_clear_applies_after( void )
{
    boot();
    ecu_clear_start();
    run( 300 );
    selected = 3;
    CHECK( ecu_select_param( 3 ) );
    run( 26000 ); /* > write gaps + CLEAR_DONE_WAIT_MS + resume */
    CHECK( ecu_clear_state() == ECU_CLEAR_DONE );
    run( 1000 );
    CHECK( value_ok( 3 ) );
    CHECK_EQ( wrong_values, 0 );
}

static void test_clear_survives_lost_bytes( void )
{
    boot();
    ecu_clear_start();
    run( 250 );
    sim.drop_bytes = 3;
    ecu_feed_error();
    run( 26000 ); /* > write gaps + CLEAR_DONE_WAIT_MS + resume */
    CHECK( ecu_clear_state() == ECU_CLEAR_DONE );
    CHECK_EQ( sim.writes, 4 );
}

static void test_clear_fails_when_ecu_dies( void )
{
    boot();
    ecu_clear_start();
    run( 200 );
    sim.alive = false;
    run( 31000 ); /* > CLEAR_LIMIT_MS */
    CHECK( ecu_clear_state() == ECU_CLEAR_FAILED );
    CHECK( sim.writes < 4 );

    sim.alive     = true;
    sim.streaming = false;
    sim.writes    = 0;
    CHECK( ecu_clear_start() ); /* can try again */
    run( 26000 );               /* > write gaps + CLEAR_DONE_WAIT_MS + resume */
    CHECK( ecu_clear_state() == ECU_CLEAR_DONE );
    CHECK_EQ( sim.writes, 4 );
}

/* The ECU writes its memory after the clear and may say nothing for a long
 * while. The reference tool sends nothing until the wait is over, so the
 * gauge must not restart its read cycle on the silence and talk to a busy
 * ECU. */
static void test_clear_wait_sends_nothing_while_the_ecu_is_busy( void )
{
    uint8_t b[8];
    unsigned extra      = 0;
    uint32_t last_write = 0;
    bool wrote          = false;

    boot();
    sim.busy_after_write = true;
    ecu_clear_start();
    for ( unsigned i = 0; i < 26000u; i++ )
    {
        size_t n;

        ecu_tick( now );
        sim_stream();
        n = ecu_next_tx( b, sizeof( b ) );
        for ( size_t j = 0; j < n; j++ )
        {
            sim_rx( b[j] );
        }
        if ( n == 4u )
        {
            if ( b[0] == 0xAAu )
            {
                wrote      = true;
                last_write = now;
            }
            else if ( wrote && ( ( now - last_write ) < 22400u ) )
            {
                extra++; /* a stop or a read while the ECU writes memory */
            }
        }
        now++;
    }
    CHECK_EQ( extra, 0 );
    CHECK_EQ( sim.writes, 4 );
    CHECK( ecu_clear_state() == ECU_CLEAR_DONE );

    /* and the gauge is reading again once the wait is over */
    run( 1500 );
    CHECK( value_ok( 0 ) );
    CHECK_EQ( wrong_values, 0 );
}

/* Steps until the clear is no longer running, at most limit ms. Returns the
 * time it took. */
static unsigned run_while_clearing( unsigned limit )
{
    unsigned t = 0;

    while ( ( t < limit ) && ( ecu_clear_state() == ECU_CLEAR_BUSY ) )
    {
        step();
        t++;
    }
    return t;
}

/* A second clear in the same session: nothing is left over from the first,
 * so it writes four more times and is not ended by the first answer. */
static void test_a_second_clear_in_the_same_session( void )
{
    boot();
    ecu_clear_start();
    CHECK( run_while_clearing( 30000 ) < 30000u );
    CHECK( ecu_clear_state() == ECU_CLEAR_DONE );
    CHECK_EQ( sim.writes, 4 );

    run( 1000 );
    CHECK( ecu_clear_start() );
    CHECK( ecu_clear_state() == ECU_CLEAR_BUSY );
    run( 5000 ); /* long enough for a stale flag to end it early */
    CHECK( ecu_clear_state() == ECU_CLEAR_BUSY );
    CHECK( run_while_clearing( 30000 ) < 30000u );
    CHECK( ecu_clear_state() == ECU_CLEAR_DONE );
    CHECK_EQ( sim.writes, 8 );
    for ( unsigned i = 4; i < 8u; i++ )
    {
        CHECK( sim.write_cmd[i][0] == 0xAA );
        CHECK( sim.write_cmd[i][3] == sim.write_cmd[0][3] );
    }
}

/* The ECU stays busy past the wait and answers before the limit: the clear
 * is done when it does, not when the wait ends. */
static void test_an_ecu_that_answers_late_still_finishes_the_clear( void )
{
    unsigned took;

    boot();
    sim.busy_after_write = true;
    sim.busy_ms          = 27000u;
    ecu_clear_start();
    took = run_while_clearing( 31000 );
    CHECK( ecu_clear_state() == ECU_CLEAR_DONE );
    CHECK( took > 27000u ); /* not before it spoke */
    CHECK( took < 30000u );
    CHECK_EQ( sim.writes, 4 );
    run( 1500 );
    CHECK( value_ok( 0 ) );
}

/* The ECU is still silent at the limit: the clear fails, and the gauge reads
 * again when the ECU finally returns. */
static void test_an_ecu_silent_past_the_limit_fails_the_clear( void )
{
    unsigned took;

    boot();
    sim.busy_after_write = true;
    sim.busy_ms          = 40000u;
    ecu_clear_start();
    took = run_while_clearing( 31000 );
    CHECK( ecu_clear_state() == ECU_CLEAR_FAILED );
    CHECK( took >= 29900u );
    CHECK( took <= 30100u );
    run( 20000 ); /* the ECU comes back */
    CHECK( ecu_clear_state() == ECU_CLEAR_FAILED );
    CHECK( value_ok( 0 ) );
    CHECK_EQ( wrong_values, 0 );
}

/* The 22 s wait crosses the 32 bit millisecond counter's wrap. */
static void test_a_clear_across_the_timer_wrap( void )
{
    unsigned took;

    start( 0xFFFFFFFFu - 12000u ); /* wraps 9.5 s into the clear */
    run( 2500 );
    ecu_clear_start();
    took = run_while_clearing( 30000 );
    CHECK( took < 30000u );
    CHECK( took >= 22500u ); /* the wait is not cut short by the wrap */
    CHECK( ecu_clear_state() == ECU_CLEAR_DONE );
    CHECK_EQ( sim.writes, 4 );
    run( 1000 );
    CHECK( value_ok( 0 ) );
    CHECK_EQ( wrong_values, 0 );
}

/* An ECU that goes quiet before the first write: the gauge keeps trying to
 * read the clear address, writes nothing, and gives up at the limit. */
static void test_an_ecu_that_dies_before_the_first_write( void )
{
    boot();
    ecu_clear_start();
    run( 40 ); /* too early for a first sample of the clear address */
    CHECK_EQ( sim.writes, 0 );
    sim.alive = false;
    tx_bytes  = 0;
    CHECK( run_while_clearing( 31000 ) <= 30100u );
    CHECK( tx_bytes >= 40u ); /* still stopping and reading again, 10+ times */
    CHECK( ecu_clear_state() == ECU_CLEAR_FAILED );
    CHECK_EQ( sim.writes, 0 );
}

/* An ECU whose ROM ID is unknown leaves the session with no profile. It must
 * stay idle however long it runs: a read is built from a profile, so one
 * sent from none would be a NULL dereference. */
static void test_an_unknown_ecu_stays_idle_for_good( void )
{
    unsigned sent;

    start_id( 0, 0x9999 );
    run( 2500 );
    CHECK_EQ( ecu_param_count(), 0 );
    sent = tx_bytes;
    run( 30000 );
    CHECK_EQ( tx_bytes, sent );
    CHECK( !ecu_clear_start() );
}

int main( void )
{
    test_initial_state();
    test_command_sequence();
    test_identifies_all_six();
    test_every_parameter_of_every_profile_reads();
    test_assumed_profile_shows_pages_but_is_not_identified();
    test_assumed_profile_confirmed_keeps_the_selection();
    test_assumed_profile_replaced_by_the_real_one();
    test_assuming_an_unknown_key_uses_the_first_profile();
    test_assumed_profile_stays_when_the_ecu_is_silent();
    test_unknown_rom_id();
    test_id_text_uses_upper_case_hex();
    test_id_streamed_continuously();
    test_id_survives_lost_bytes_and_noise();
    test_id_with_echo();
    test_ecu_powers_up_late();
    test_reads_value();
    test_value_tracks_changes();
    test_switch_parameter();
    test_reselect_same_is_noop();
    test_lost_byte_recovers();
    test_noise_burst_recovers();
    test_parity_errors_recover();
    test_command_echo();
    test_ecu_already_streaming_and_ignores_stop();
    test_absent_ecu_backs_off();
    test_ecu_dies_and_returns();
    test_millis_wraparound();
    test_stray_bytes_before_start();
    test_clear_every_profile();
    test_clear_write_spacing();
    test_clear_selection_during_clear_applies_after();
    test_clear_survives_lost_bytes();
    test_clear_fails_when_ecu_dies();
    test_clear_wait_sends_nothing_while_the_ecu_is_busy();
    test_a_second_clear_in_the_same_session();
    test_an_ecu_that_answers_late_still_finishes_the_clear();
    test_an_ecu_silent_past_the_limit_fails_the_clear();
    test_a_clear_across_the_timer_wrap();
    test_an_ecu_that_dies_before_the_first_write();
    test_an_unknown_ecu_stays_idle_for_good();
    return MT_RESULT();
}
