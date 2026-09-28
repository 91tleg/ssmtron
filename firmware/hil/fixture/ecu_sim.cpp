/* Fake SSM1 ECU, so the DUT's ecu_io.c/lib/ssm1 stack can be exercised
 * against a real UART peer at the protocol's real 1953 baud, without a
 * car. Protocol details (command bytes, reply framing, ROM ID format)
 * are taken directly from firmware/lib/ssm1 (ssm1_cmd.h, ssm1_rx.h,
 * ssm1_romid.h), not guessed.
 *
 * Simulates ROM ID 0x3672 ("91-94 Turbo" in ssm1_params.c): RPM at
 * 0x140B, clear address 0x1650, SSM1_CLEAR_ZERO. ssm1.c's clear sequence
 * (see clear_tick/clear_write) is purely time/write-count based -- 4
 * writes 384 ms apart -- so this simulator does not need to do anything
 * special for a clear to succeed once identification has completed; it
 * only needs to not corrupt the stream while writes are interleaved in.
 *
 * Baud: the real protocol is 1953.125 baud (see firmware/hal/nrf52's own
 * direct BAUDRATE register write, needed because Zephyr's UART API
 * cannot request a fractional rate either). The Arduino API here only
 * accepts an integer baud, so this asks for 1953: a ~0.0064% deviation
 * from 1953.125, well inside standard UART tolerance, not a claim of
 * bit-exact timing.
 */
#include "ecu_sim.h"

#include <Arduino.h>

#define ECU_BAUD ( 1953 )

#define CMD_LEN ( 4u )
#define CMD_STOP_0 ( 0x12u )
#define CMD_ROMID_0 ( 0x00u )
#define CMD_ROMID_1 ( 0xFFu )
#define CMD_READ_ECU ( 0x78u )
#define CMD_WRITE ( 0xAAu )

/* ROM ID 0x3672 -> r0 | r1<<8 == 0x3672, so r0=0x72, r1=0x36; r2 is not
 * used for profile lookup (ssm1_profile_t.rom_id is 16-bit). */
static const uint8_t ROMID_REPLY[6] = { 0xFFu, 0xFEu, 0x00u, 0x72u, 0x36u, 0x00u };

#define RPM_ADDR ( 0x140Bu )

typedef enum
{
    ECU_QUIET,
    ECU_STREAM_ROMID,
    ECU_STREAM_ADDR
} ecu_state_t;

static ecu_state_t state = ECU_QUIET;
static uint16_t stream_addr;
static uint8_t stream_data = 0x40u;
static unsigned long last_frame_ms;
static unsigned long last_data_bump_ms;

#define FRAME_PERIOD_MS ( 10ul ) /* arbitrary; just needs to be steady */
#define DATA_BUMP_MS ( 1000ul )  /* cosmetic: RPM "moves" once a second */

static uint8_t cmd[CMD_LEN];

static void handle_command()
{
    if ( ( cmd[0] == CMD_STOP_0 ) && ( cmd[1] == 0x00u ) && ( cmd[2] == 0x00u )
         && ( cmd[3] == 0x00u ) )
    {
        state = ECU_QUIET;
    }
    else if ( ( cmd[0] == CMD_ROMID_0 ) && ( cmd[1] == CMD_ROMID_1 )
              && ( cmd[2] == CMD_ROMID_1 ) && ( cmd[3] == 0x00u ) )
    {
        state = ECU_STREAM_ROMID;
    }
    else if ( cmd[0] == CMD_READ_ECU )
    {
        state = ECU_STREAM_ADDR;
        stream_addr = ( ( uint16_t )cmd[1] << 8 ) | cmd[2];
    }
    else if ( cmd[0] == CMD_WRITE )
    {
        /* "Only takes effect while streaming": no reply, and the
         * session keeps streaming whatever it was already reading (see
         * ssm1.c's clear_write -- it interleaves writes, it does not
         * re-request a read), so state/stream_addr stay unchanged. */
    }
}

static void poll_commands()
{
    static size_t have = 0;

    while ( Serial1.available() > 0 )
    {
        if ( have == CMD_LEN )
        {
            /* Slide the window by one byte, like ssm1_rx.c does on the
             * DUT side, since commands carry no start marker either. */
            for ( size_t i = 1; i < CMD_LEN; i++ )
            {
                cmd[i - 1] = cmd[i];
            }
            have = CMD_LEN - 1u;
        }
        cmd[have] = ( uint8_t )Serial1.read();
        have++;
        if ( have == CMD_LEN )
        {
            handle_command();
        }
    }
}

static void send_frame()
{
    unsigned long now = millis();

    if ( ( now - last_frame_ms ) < FRAME_PERIOD_MS )
    {
        return;
    }
    last_frame_ms = now;

    if ( ( now - last_data_bump_ms ) >= DATA_BUMP_MS )
    {
        last_data_bump_ms = now;
        stream_data++;
    }

    if ( state == ECU_STREAM_ROMID )
    {
        Serial1.write( ROMID_REPLY, sizeof( ROMID_REPLY ) );
    }
    else if ( state == ECU_STREAM_ADDR )
    {
        uint8_t frame[3] = { ( uint8_t )( stream_addr >> 8 ),
                              ( uint8_t )( stream_addr & 0xFFu ), stream_data };
        Serial1.write( frame, sizeof( frame ) );
    }
    else
    {
        /* QUIET: nothing to send. */
    }
}

void ecu_sim_init(
    int rx_pin,
    int tx_pin
)
{
    Serial1.begin( ECU_BAUD, SERIAL_8E1, rx_pin, tx_pin );
}

void ecu_sim_poll()
{
    poll_commands();
    send_frame();
}
