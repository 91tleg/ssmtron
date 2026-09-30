/* SSM1 session. The ECU streams one address until stopped. Each target
 * follows this cycle:
 *
 *   STOP_PENDING -> QUIET -> READ_PENDING -> WAIT_DATA -> STREAM
 *
 * Identification reads 0xFFFE to select a profile. Failures restart the
 * cycle; repeated failures pause before retrying. Time arithmetic handles
 * counter wraparound. */
#include "lib/ecu/ecu.h"
#include "lib/ssm1/ssm1_cmd.h"
#include "lib/ssm1/ssm1_decode.h"
#include "lib/ssm1/ssm1_params.h"
#include "lib/ssm1/ssm1_romid.h"
#include "lib/ssm1/ssm1_rx.h"

#define QUIET_MS       ( 80u )   /* line silence required after a stop */
#define STOP_SETTLE_MS ( 1000u ) /* give up waiting for silence after this */
#define FIRST_DATA_MS  ( 300u )  /* read sent -> first confirmed sample */
#define STALL_MS       ( 500u )  /* no confirmed sample while streaming */
#define BACKOFF_MS     ( 1000u )
#define FRESH_MS       ( 500u )  /* value older than this is not reported */
#define LINK_MS        ( 1000u ) /* link is up while samples are this recent */
#define MAX_TRIES      ( 3u )
#define ID_RESEND_MS   ( 300u )  /* ROM ID command repeat while unanswered */
#define ID_TIMEOUT_MS  ( 1500u ) /* read sent -> confirmed ROM ID */
#define CLEAR_GAP_MS   ( 384u )  /* between clear writes */
/* After the last write, before resuming reads: real EEPROM/flash write
 * cycle time, not just line settling. Resuming sooner risks reading
 * mid-write garbage or interrupting the clear. */
#define CLEAR_DONE_WAIT_MS ( 22407u )
#define CLEAR_WRITES       ( 4u )
/* Must clear (CLEAR_WRITES - 1) write gaps plus CLEAR_DONE_WAIT_MS,
 * with margin for command transmission time and jitter. */
#define CLEAR_LIMIT_MS  ( 30000u )
#define SSM1_ADDR_ROMID ( 0xFFFEu )
#define SLOTS           ( SSM1_PARAM_MAX + 1u ) /* + the clear address */

typedef enum
{
    PH_IDLE,         /* nothing selected yet */
    PH_STOP_PENDING, /* stop command waiting to be sent */
    PH_QUIET,        /* stop sent, waiting for the line to go silent */
    PH_READ_PENDING, /* read command waiting to be sent */
    PH_WAIT_DATA,    /* read sent, waiting for the first confirmed sample */
    PH_ID_WAIT,      /* read of 0xFFFE sent, waiting for the ROM ID */
    PH_STREAM,       /* samples arriving */
    PH_BACKOFF       /* too many failed cycles, waiting to retry */
} phase_t;

typedef struct
{
    phase_t phase;
    uint32_t now;
    uint32_t phase_start;
    uint32_t last_activity; /* last rx byte, or stop tx, while QUIET */
    uint32_t last_sample;
    size_t want;   /* slot being read; == profile count for the clear addr */
    size_t resume; /* parameter to return to after a clear */
    const ssm1_profile_t * profile;
    uint32_t rom_id;
    uint32_t id_cmd_at;
    ecu_clear_t clear;
    uint32_t clear_start;
    uint32_t clear_last;
    ssm1_rx_t rx;
    ssm1_romid_t romid;
    uint32_t stamp[SLOTS];
    uint8_t fails;
    bool ident; /* this cycle identifies the ECU */
    bool any_sample;
    bool skip_first; /* first sample after a read may be our own echo */
    bool identified;
    bool id_cmd_due;
    uint8_t clear_writes;
    uint8_t clear_data;
    uint8_t raw[SLOTS];
    bool have[SLOTS];
} session_t;

static session_t s;

static uint32_t since( uint32_t t )
{
    return s.now - t;
}

static void begin_cycle( void )
{
    s.phase       = PH_STOP_PENDING;
    s.phase_start = s.now;
}

static void fail_cycle( void )
{
    s.fails++;
    if ( s.fails >= MAX_TRIES )
    {
        s.fails       = 0;
        s.phase       = PH_BACKOFF;
        s.phase_start = s.now;
    }
    else
    {
        begin_cycle();
    }
}

static size_t slot_count( void )
{
    return ( s.profile != NULL ) ? s.profile->count : 0u;
}

static uint16_t target_addr( void )
{
    return ( s.want < slot_count() ) ? s.profile->params[s.want].addr
                                     : s.profile->clear_addr;
}

/* Point the cycle at another slot. Before the read command is built, the
 * newest choice simply wins. Once it has been sent the ECU is streaming the
 * old address and needs a stop first. */
static void set_target( size_t slot )
{
    bool changed = ( slot != s.want ) || ( s.phase == PH_IDLE );

    s.want = slot;
    if ( changed
         && ( ( s.phase == PH_IDLE ) || ( s.phase == PH_WAIT_DATA )
              || ( s.phase == PH_STREAM ) || ( s.phase == PH_BACKOFF ) ) )
    {
        s.fails = 0;
        begin_cycle();
    }
}

static void finish_clear( ecu_clear_t result )
{
    s.clear = result;
    set_target( s.resume );
}

static void on_sample( uint8_t data )
{
    /* If the line echoes our command, "78 msb lsb 00" hides a false frame
     * "msb lsb 00" right before the real stream. It looks valid, so drop
     * the first sample after every read (and every write). */
    if ( s.skip_first )
    {
        s.skip_first = false;
    }
    else
    {
        s.raw[s.want]   = data;
        s.stamp[s.want] = s.now;
        s.have[s.want]  = true;
        s.last_sample   = s.now;
        s.any_sample    = true;
        s.fails         = 0;
        s.phase         = PH_STREAM;
    }
}

static void on_identified( uint32_t id )
{
    const ssm1_profile_t * found =
        ssm1_profile_find( ( uint16_t )( id & 0xFFFFu ) );
    /* An assumed profile that turns out to be right keeps the selection made
     * while waiting. Anything else starts at the first parameter. */
    bool same = ( found != NULL ) && ( found == s.profile );

    s.rom_id      = id;
    s.profile     = found;
    s.identified  = true;
    s.ident       = false;
    s.last_sample = s.now;
    s.any_sample  = true;
    s.fails       = 0;
    if ( !same )
    {
        s.want   = 0;
        s.resume = 0;
    }
    if ( s.profile != NULL )
    {
        begin_cycle(); /* stop the 0xFFFE stream, then read parameter 0 */
    }
    else
    {
        s.phase = PH_IDLE; /* nothing we can read */
    }
}

static void clear_tick( void )
{
    if ( s.clear == ECU_CLEAR_BUSY )
    {
        if ( since( s.clear_start ) >= CLEAR_LIMIT_MS )
        {
            finish_clear( ECU_CLEAR_FAILED );
        }
        else if ( ( s.clear_writes >= CLEAR_WRITES )
                  && ( since( s.clear_last ) >= CLEAR_DONE_WAIT_MS ) )
        {
            finish_clear( ECU_CLEAR_DONE );
        }
        else
        {
            /* still within CLEAR_LIMIT_MS, not yet enough writes */
        }
    }
}

const ecu_link_cfg_t * ecu_link_config( void )
{
    static const ecu_link_cfg_t link_cfg = {
        .baud      = 1953u,
        .data_bits = 8u,
        .parity    = ECU_PARITY_EVEN,
        .stop_bits = 1u
    };

    return &link_cfg;
}

void ecu_init( void )
{
    s = ( session_t ){ 0 };
    ssm1_rx_init( &s.rx );
    ssm1_romid_init( &s.romid );
    s.ident = true;
    begin_cycle();
}

void ecu_tick( uint32_t now_ms )
{
    s.now = now_ms;

    switch ( s.phase )
    {
        case PH_QUIET:
            if ( since( s.last_activity ) >= QUIET_MS )
            {
                s.phase       = PH_READ_PENDING;
                s.phase_start = s.now;
            }
            else if ( since( s.phase_start ) >= STOP_SETTLE_MS )
            {
                fail_cycle();
            }
            else
            {
                /* still quiet, still within STOP_SETTLE_MS */
            }
            break;
        case PH_WAIT_DATA:
            if ( since( s.phase_start ) >= FIRST_DATA_MS )
            {
                fail_cycle();
            }
            break;
        case PH_ID_WAIT:
            if ( since( s.phase_start ) >= ID_TIMEOUT_MS )
            {
                fail_cycle();
            }
            else if ( since( s.id_cmd_at ) >= ID_RESEND_MS )
            {
                s.id_cmd_due = true;
            }
            else
            {
                /* still waiting, not yet due for a resend */
            }
            break;
        case PH_STREAM:
            if ( since( s.last_sample ) >= STALL_MS )
            {
                fail_cycle();
            }
            break;
        case PH_BACKOFF:
            if ( since( s.phase_start ) >= BACKOFF_MS )
            {
                begin_cycle();
            }
            break;
        case PH_IDLE:
        case PH_STOP_PENDING:
        case PH_READ_PENDING:
            break;
        default:
            break;
    }
    clear_tick();
}

void ecu_feed( uint8_t byte )
{
    uint8_t data = 0;
    uint32_t id  = 0;

    switch ( s.phase )
    {
        case PH_QUIET:
            s.last_activity = s.now; /* still talking: not quiet yet */
            break;
        case PH_ID_WAIT:
            if ( ssm1_romid_feed( &s.romid, byte, &id ) )
            {
                on_identified( id );
            }
            break;
        case PH_WAIT_DATA:
        case PH_STREAM:
            if ( ssm1_rx_feed( &s.rx, byte, &data ) )
            {
                on_sample( data );
            }
            break;
        case PH_IDLE:
        case PH_STOP_PENDING:
        case PH_READ_PENDING:
        case PH_BACKOFF:
            break; /* stray bytes before/between cycles */
        default:
            break;
    }
}

void ecu_feed_error( void )
{
    switch ( s.phase )
    {
        case PH_QUIET:
            s.last_activity = s.now;
            break;
        case PH_ID_WAIT:
            ssm1_romid_error( &s.romid );
            break;
        case PH_WAIT_DATA:
        case PH_STREAM:
            ssm1_rx_error( &s.rx );
            break;
        case PH_IDLE:
        case PH_STOP_PENDING:
        case PH_READ_PENDING:
        case PH_BACKOFF:
            break;
        default:
            break;
    }
}

/* The next clear write, if one is due. Needs a fresh reading of the clear
 * address first, since the value to write can depend on it. */
static size_t clear_write( uint8_t * buf )
{
    size_t n = 0;

    if ( ( s.clear == ECU_CLEAR_BUSY ) && ( s.want == slot_count() )
         && ( s.clear_writes < CLEAR_WRITES ) )
    {
        bool first = ( s.clear_writes == 0u );
        bool due;

        if ( first )
        {
            due = s.have[s.want] && ( since( s.stamp[s.want] ) < FRESH_MS );
        }
        else
        {
            due = ( since( s.clear_last ) >= CLEAR_GAP_MS );
        }

        if ( due )
        {
            if ( first )
            {
                /* SSM1_CLEAR_ZERO sends 0xFF, not literal 0. */
                s.clear_data = ( s.profile->clear_mode == SSM1_CLEAR_ZERO )
                                   ? 0xFFu
                                   : ( uint8_t )~s.raw[s.want];
            }
            n = ssm1_cmd_write( s.profile->clear_addr, s.clear_data, buf );
            s.clear_writes++;
            s.clear_last = s.now;
            s.skip_first = true;
        }
    }
    return n;
}

size_t ecu_next_tx(
    uint8_t * buf,
    size_t max
)
{
    size_t n = 0;

    if ( max >= SSM1_CMD_LEN )
    {
        switch ( s.phase )
        {
            case PH_STOP_PENDING:
                n               = ssm1_cmd_stop( buf );
                s.phase         = PH_QUIET;
                s.phase_start   = s.now;
                s.last_activity = s.now;
                break;
            case PH_READ_PENDING:
                if ( s.ident )
                {
                    ssm1_romid_init( &s.romid );
                    n            = ssm1_cmd_read_ecu( SSM1_ADDR_ROMID, buf );
                    s.phase      = PH_ID_WAIT;
                    s.id_cmd_due = true;
                    s.id_cmd_at  = s.now;
                }
                else
                {
                    ssm1_rx_expect( &s.rx, target_addr() );
                    n            = ssm1_cmd_read_ecu( target_addr(), buf );
                    s.phase      = PH_WAIT_DATA;
                    s.skip_first = true;
                }
                s.phase_start = s.now;
                break;
            case PH_ID_WAIT:
                if ( s.id_cmd_due )
                {
                    n            = ssm1_cmd_romid( buf );
                    s.id_cmd_due = false;
                    s.id_cmd_at  = s.now;
                }
                break;
            case PH_STREAM:
                n = clear_write( buf );
                break;
            case PH_IDLE:
            case PH_QUIET:
            case PH_WAIT_DATA:
            case PH_BACKOFF:
                break;
            default:
                break;
        }
    }
    return n;
}

ecu_link_t ecu_link( void )
{
    bool up = s.any_sample && ( since( s.last_sample ) < LINK_MS );

    return up ? ECU_LINK_UP : ECU_LINK_DOWN;
}

/* cppcheck-suppress misra-c2012-8.7 ; public API (ecu.h), called from
 * test_ssm1.c, which this scoped MISRA scan does not cover. */
const char * ecu_name( void )
{
    const char * name = NULL;

    if ( s.identified )
    {
        name = ( s.profile != NULL ) ? s.profile->name : "Unknown";
    }
    return name;
}

uint32_t ecu_rom_id( void )
{
    return s.identified ? s.rom_id : 0u;
}

uint16_t ecu_assume_profile( uint16_t profile_key )
{
    if ( !s.identified )
    {
        const ssm1_profile_t * found = ssm1_profile_find( profile_key );

        s.profile = ( found != NULL ) ? found : ssm1_profile( 0u );
        s.want    = 0;
        s.resume  = 0;
    }
    return ( s.profile != NULL ) ? s.profile->rom_id : 0u;
}

size_t ecu_param_count( void )
{
    return slot_count();
}

const ecu_param_info_t * ecu_param_info( size_t idx )
{
    return ( idx < slot_count() ) ? s.profile->params[idx].info : NULL;
}

bool ecu_select_param( size_t idx )
{
    bool ok = idx < slot_count();

    if ( ok )
    {
        if ( s.clear == ECU_CLEAR_BUSY )
        {
            s.resume = idx; /* applied when the clear finishes */
        }
        else
        {
            set_target( idx );
        }
    }
    return ok;
}

bool ecu_get_value(
    size_t idx,
    int32_t * out
)
{
    bool ok = ( idx < slot_count() ) && s.have[idx]
              && ( since( s.stamp[idx] ) < FRESH_MS );

    if ( ok )
    {
        *out = ssm1_decode( s.profile->params[idx].info, s.raw[idx] );
    }
    return ok;
}

bool ecu_clear_start( void )
{
    /* Only a profile the ECU itself identified: an assumed one is a guess,
     * and the clear writes to an address taken from the profile. */
    bool ok =
        s.identified && ( s.profile != NULL ) && ( s.clear != ECU_CLEAR_BUSY );

    if ( ok )
    {
        s.clear              = ECU_CLEAR_BUSY;
        s.clear_start        = s.now;
        s.clear_writes       = 0;
        s.resume             = s.want;
        s.have[slot_count()] = false;
        set_target( slot_count() );
    }
    return ok;
}

ecu_clear_t ecu_clear_state( void )
{
    return s.clear;
}
