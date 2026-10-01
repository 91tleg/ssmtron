/* The software-in-the-loop harness: the real gauge, button recognizer, ECU
 * glue and SSM1 session talk to a simulated ECU over a fake serial line, in
 * simulated time. Only the edges of the system are faked: the UART bytes, the
 * button pins, flash, the display and the text and dial drawing (their text
 * and values are captured, so a test reads what the screen would say).
 *
 * Include this in exactly one source file of each test program: it defines
 * the fake HAL functions the program links against. */
#ifndef SIL_HARNESS_H
#define SIL_HARNESS_H

#include <stdio.h>
#include <string.h>

#include "app/ecu_io.h"
#include "app/fw_info.h"
#include "app/gauge.h"
#include "hal/hal_ble.h"
#include "hal/hal_dfu.h"
#include "hal/hal_gpio.h"
#include "hal/hal_log.h"
#include "hal/hal_nvm.h"
#include "hal/hal_random.h"
#include "hal/hal_uart.h"
#include "lib/display/display.h"
#include "lib/ecu/ecu.h"
#include "lib/fmt/fmt.h"
#include "lib/ssm1/ssm1_decode.h"
#include "lib/ssm1/ssm1_params.h"
#include "lib/ui/dial.h"
#include "lib/ui/text.h"
#include "test/minitest.h"

#define NEXT_ID   ( ( hal_gpio_id_t )1u )
#define ACTION_ID ( ( hal_gpio_id_t )2u )
#define ROM_KEY   ( 0x3671u )
#define BYTE_MS   ( 6u ) /* about 5.6 ms per 8E1 byte at 1953 baud */
#define ID_ADDR   ( 0xFFFEu )
#define NVM_FILE  "sil_nvm.bin"

static uint32_t now;

/* ---- the simulated ECU, behind the serial line ------------------------- */

typedef struct
{
    bool alive;
    bool streaming;
    uint16_t addr;
    uint8_t cmd[4];
    uint8_t cmd_len;
    uint8_t phase; /* 0..2 frame, 3..5 ROM ID bytes */
    bool id_pending;
    uint32_t next_ms;
    unsigned writes;
    uint8_t write_cmd[8][4];
    uint32_t write_at[8];
    uint8_t rom[3]; /* the ROM ID bytes it answers with */
} sim_t;

static sim_t sim;

/* Bytes the gauge receives, in order. */
static uint8_t rx_queue[512];
static size_t rx_head;
static size_t rx_tail;

static inline void rx_push( uint8_t b )
{
    if ( ( ( rx_tail + 1u ) % sizeof( rx_queue ) ) != rx_head )
    {
        rx_queue[rx_tail] = b;
        rx_tail           = ( rx_tail + 1u ) % sizeof( rx_queue );
    }
}

/* What the ECU holds at an address. */
static inline uint8_t mem( uint16_t addr )
{
    return ( uint8_t )( ( addr * 7u + 3u ) & 0xFFu );
}

/* Everything the gauge sent, with the time of each byte. */
static uint8_t tx_log[256];
static uint32_t tx_time[256];
static size_t tx_count;

hal_status_t hal_uart_open(
    hal_uart_id_t id,
    const hal_uart_cfg_t * cfg
)
{
    ( void )id;
    ( void )cfg;
    return HAL_OK;
}

static inline void sim_rx( uint8_t b )
{
    if ( sim.alive )
    {
        sim.cmd[sim.cmd_len] = b;
        sim.cmd_len++;
        if ( sim.cmd_len == 4u )
        {
            sim.cmd_len = 0u;
            if ( sim.cmd[0] == 0x12u )
            {
                sim.streaming = false;
            }
            else if ( sim.cmd[0] == 0x78u )
            {
                sim.streaming = true;
                sim.addr    = ( uint16_t )( ( sim.cmd[1] << 8 ) | sim.cmd[2] );
                sim.phase   = 0u;
                sim.next_ms = now + BYTE_MS;
            }
            else if ( ( sim.cmd[0] == 0x00u ) && ( sim.cmd[1] == 0xFFu )
                      && ( sim.cmd[2] == 0xFFu ) )
            {
                sim.id_pending = sim.streaming;
            }
            else if ( ( sim.cmd[0] == 0xAAu ) && sim.streaming
                      && ( sim.writes < 8u ) )
            {
                ( void )memcpy( sim.write_cmd[sim.writes], sim.cmd, 4u );
                sim.write_at[sim.writes] = now;
                sim.writes++;
            }
            else
            {
                /* nothing else is understood */
            }
        }
    }
}

size_t hal_uart_write(
    hal_uart_id_t id,
    const uint8_t * buf,
    size_t len
)
{
    ( void )id;
    for ( size_t i = 0; i < len; i++ )
    {
        if ( tx_count < sizeof( tx_log ) )
        {
            tx_log[tx_count]  = buf[i];
            tx_time[tx_count] = now;
            tx_count++;
        }
        sim_rx( buf[i] );
    }
    return len;
}

hal_uart_rx_t hal_uart_read(
    hal_uart_id_t id,
    uint8_t * byte
)
{
    hal_uart_rx_t result = HAL_UART_RX_EMPTY;

    ( void )id;
    if ( rx_head != rx_tail )
    {
        *byte   = rx_queue[rx_head];
        rx_head = ( rx_head + 1u ) % sizeof( rx_queue );
        result  = HAL_UART_RX_BYTE;
    }
    return result;
}

/* One millisecond of the ECU: the next byte of its stream, when it is due. */
static inline void sim_stream( void )
{
    if ( sim.alive && sim.streaming
         && ( ( int32_t )( now - sim.next_ms ) >= 0 ) )
    {
        uint8_t b = 0u;

        if ( sim.phase == 0u )
        {
            b = ( uint8_t )( sim.addr >> 8 );
        }
        else if ( sim.phase == 1u )
        {
            b = ( uint8_t )( sim.addr & 0xFFu );
        }
        else if ( sim.phase == 2u )
        {
            b = ( sim.addr == ID_ADDR ) ? 0x55u : mem( sim.addr );
        }
        else
        {
            b = sim.rom[sim.phase - 3u];
        }
        rx_push( b );
        if ( ( sim.phase == 2u ) && ( sim.addr == ID_ADDR ) && sim.id_pending )
        {
            sim.phase = 3u;
        }
        else if ( sim.phase == 5u )
        {
            sim.id_pending = false;
            sim.phase      = 0u;
        }
        else
        {
            sim.phase =
                ( sim.phase == 2u ) ? 0u : ( uint8_t )( sim.phase + 1u );
        }
        sim.next_ms += BYTE_MS;
    }
}

/* ---- the buttons: the HAL's edge latch --------------------------------- */

typedef struct
{
    bool level;
    bool pending;
    bool press_pending;
    uint32_t at;
    uint32_t press_at;
} latch_t;

static latch_t next_edges;
static latch_t action_edges;

hal_status_t hal_gpio_open(
    hal_gpio_id_t id,
    hal_gpio_mode_t mode
)
{
    ( void )id;
    ( void )mode;
    return HAL_OK;
}

hal_status_t hal_gpio_watch( hal_gpio_id_t id )
{
    ( void )id;
    return HAL_OK;
}

bool hal_gpio_take_edge(
    hal_gpio_id_t id,
    bool * level,
    uint32_t * timestamp_ms
)
{
    latch_t * l = ( id == ACTION_ID ) ? &action_edges : &next_edges;
    bool had    = false;

    if ( l->press_pending )
    {
        *level           = true;
        *timestamp_ms    = l->press_at;
        l->press_pending = false;
        had              = true;
        if ( l->level )
        {
            l->pending = false;
        }
    }
    else if ( l->pending )
    {
        *level        = l->level;
        *timestamp_ms = l->at;
        l->pending    = false;
        had           = true;
    }
    return had;
}

static inline void button(
    latch_t * l,
    bool level
)
{
    l->level   = level;
    l->at      = now;
    l->pending = true;
    if ( level )
    {
        l->press_pending = true;
        l->press_at      = now;
    }
}

/* ---- flash: a small table, kept in a file when a run asks for it ------- */

typedef struct
{
    char key[HAL_NVM_KEY_MAX];
    uint8_t data[16];
    size_t len;
    bool used;
} nvm_item_t;

static nvm_item_t nvm_items[6];
static bool nvm_file_enabled;
static bool nvm_load_file;
static bool nvm_file_loaded; /* the file was found and read whole */
static unsigned nvm_file_writes;

/* Reads the flash file into the table. True only if it was read whole. */
static inline bool nvm_read_file( nvm_item_t * items )
{
    FILE * f = fopen( NVM_FILE, "rb" );
    bool ok  = false;

    if ( f != NULL )
    {
        ok = ( fread( items, sizeof( nvm_items ), 1u, f ) == 1u );
        ok = ( fclose( f ) == 0 ) && ok;
    }
    return ok;
}

hal_status_t hal_nvm_init( void )
{
    hal_status_t status = HAL_OK;

    if ( nvm_load_file )
    {
        nvm_file_loaded = nvm_read_file( nvm_items );
        status          = nvm_file_loaded ? HAL_OK : HAL_ERR_IO;
    }
    return status;
}

/* Writes the table to the file. True if it was written whole. */
static inline bool nvm_flush( void )
{
    bool ok = true;

    if ( nvm_file_enabled )
    {
        FILE * f = fopen( NVM_FILE, "wb" );

        ok = false;
        if ( f != NULL )
        {
            ok = ( fwrite( nvm_items, sizeof( nvm_items ), 1u, f ) == 1u );
            ok = ( fclose( f ) == 0 ) && ok;
        }
        nvm_file_writes += ok ? 1u : 0u;
    }
    return ok;
}

hal_status_t hal_nvm_save(
    const char * key,
    const void * data,
    size_t len
)
{
    nvm_item_t * slot   = NULL;
    hal_status_t status = HAL_OK;

    for ( size_t i = 0; ( slot == NULL ) && ( i < 6u ); i++ )
    {
        if ( nvm_items[i].used && ( strcmp( nvm_items[i].key, key ) == 0 ) )
        {
            slot = &nvm_items[i];
        }
    }
    for ( size_t i = 0; ( slot == NULL ) && ( i < 6u ); i++ )
    {
        if ( !nvm_items[i].used )
        {
            slot = &nvm_items[i];
        }
    }
    if ( ( slot != NULL ) && ( len <= sizeof( slot->data ) ) )
    {
        ( void )strncpy( slot->key, key, sizeof( slot->key ) - 1u );
        ( void )memcpy( slot->data, data, len );
        slot->len  = len;
        slot->used = true;
        status     = nvm_flush() ? HAL_OK : HAL_ERR_IO;
    }
    return status;
}

hal_status_t hal_nvm_load(
    const char * key,
    void * data,
    size_t max,
    size_t * len
)
{
    hal_status_t status = HAL_ERR_INVALID;

    for ( size_t i = 0; i < 6u; i++ )
    {
        if ( nvm_items[i].used && ( strcmp( nvm_items[i].key, key ) == 0 ) )
        {
            size_t n = ( nvm_items[i].len < max ) ? nvm_items[i].len : max;

            ( void )memcpy( data, nvm_items[i].data, n );
            *len   = nvm_items[i].len;
            status = HAL_OK;
        }
    }
    return status;
}

/* ---- the rest of the edges --------------------------------------------- */

const fw_info_t fw_info = { "sil" };

void hal_log_write( const char * text )
{
    ( void )text;
}

/* Bluetooth, the random source and the restart, all under the test's
 * control: what update mode asked for, and what it is told in return. */
static unsigned ble_opens;
static hal_status_t ble_open_result = HAL_OK;
static uint32_t ble_passkey;
static char ble_name[32];
static hal_ble_evt_t ble_events[4];
static size_t ble_event_count;
static uint32_t random_value = 1u;
static bool random_ok        = true;
static unsigned reboots;

hal_status_t hal_ble_open( const hal_ble_cfg_t * cfg )
{
    ble_opens++;
    ble_passkey = cfg->passkey;
    ( void )strncpy( ble_name, cfg->name, sizeof( ble_name ) - 1u );
    return ble_open_result;
}

hal_ble_evt_t hal_ble_poll_event( void )
{
    hal_ble_evt_t evt = HAL_BLE_EVT_NONE;

    if ( ble_event_count > 0u )
    {
        evt = ble_events[0];
        for ( size_t i = 1u; i < ble_event_count; i++ )
        {
            ble_events[i - 1u] = ble_events[i];
        }
        ble_event_count--;
    }
    return evt;
}

/* A phone connecting or leaving, for the next poll to find. */
static inline void ble_send( hal_ble_evt_t evt )
{
    ble_events[ble_event_count] = evt;
    ble_event_count++;
}

hal_status_t hal_random_u32( uint32_t * out )
{
    if ( random_ok )
    {
        *out = random_value;
    }
    return random_ok ? HAL_OK : HAL_ERR_IO;
}

void hal_dfu_reboot( void )
{
    reboots++;
}

static unsigned frames_drawn;

hal_status_t display_init( const display_pins_t * pins )
{
    ( void )pins;
    return HAL_OK;
}

/* The text page on screen: the widgets text_init was last given. */
static text_widget_t * page;
static size_t page_count;

/* What each widget said at the moment text_init drew a frame, before any poll
 * could put it right. */
static char first_frame[16][TEXT_MAX + 1u];
static bool last_init_was_dial;
static const dial_cfg_t * dial_cfg;

/* What the first frame held when the panel was turned on. */
static unsigned on_frames;
static bool on_was_dial;
static char on_row0[TEXT_MAX + 1u];
static const dial_cfg_t * on_dial;

hal_status_t display_on( void )
{
    on_frames   = frames_drawn;
    on_was_dial = last_init_was_dial;
    on_dial     = dial_cfg;
    ( void )memcpy( on_row0, first_frame[0], sizeof( on_row0 ) );
    return HAL_OK;
}

void text_init(
    text_widget_t * widgets,
    size_t count,
    uint16_t page_bg
)
{
    ( void )page_bg;
    page               = widgets;
    page_count         = count;
    last_init_was_dial = false;
    frames_drawn++;
    for ( size_t i = 0; ( i < count ) && ( i < 16u ); i++ )
    {
        widgets[i].text( first_frame[i] );
    }
}

void text_poll(
    text_widget_t * widgets,
    size_t count
)
{
    ( void )widgets;
    ( void )count;
}

/* The dial on screen: what it was given. Both are faked so the test reads
 * the value the needle is aimed at, not pixels. */
static bool dial_has_value;
static int32_t dial_value;

void dial_init(
    dial_widget_t * d,
    const dial_cfg_t * cfg,
    uint32_t now_ms
)
{
    ( void )d;
    ( void )now_ms;
    dial_cfg           = cfg;
    page               = NULL; /* a dial replaces any text page */
    last_init_was_dial = true;
    frames_drawn++;
}

void dial_poll(
    dial_widget_t * d,
    uint32_t now_ms,
    bool has_value,
    int32_t value
)
{
    ( void )d;
    ( void )now_ms;
    dial_has_value = has_value;
    dial_value     = value;
}

bool dial_settled( const dial_widget_t * d )
{
    ( void )d;
    return true; /* the power-on sweep ends at once */
}

/* ---- driving the whole thing ------------------------------------------- */

static inline void step( void )
{
    now++;
    sim_stream();
    ecu_io_poll( now );
    gauge_poll( now );
}

static inline void run( unsigned ms )
{
    for ( unsigned i = 0; i < ms; i++ )
    {
        step();
    }
}

static inline void tap_next( void )
{
    button( &next_edges, true );
    run( 40u );
    button( &next_edges, false );
    run( 40u );
}

static inline const char * row( size_t i )
{
    static char buf[TEXT_MAX + 1u];

    buf[0] = '\0';
    if ( ( page != NULL ) && ( i < page_count ) )
    {
        page[i].text( buf );
    }
    return buf;
}

/* The ROM ID key of the ECU being simulated. */
static uint16_t sim_key = ROM_KEY;

static inline const ssm1_profile_t * profile( void )
{
    return ssm1_profile_find( sim_key );
}

/* What the ECU memory at parameter idx's address decodes to. */
static inline int32_t expected( size_t idx )
{
    const ssm1_profile_t * p = profile();

    return ssm1_decode( p->params[idx].info, mem( p->params[idx].addr ) );
}

static inline void boot( uint16_t key )
{
    static const display_pins_t pins = { 0 };

    sim_key    = key;
    sim        = ( sim_t ){ 0 };
    sim.alive  = true;
    sim.rom[0] = ( uint8_t )( key & 0xFFu );
    sim.rom[1] = ( uint8_t )( key >> 8 );
    sim.rom[2] = 0x01u;
    ( void )hal_nvm_init();
    CHECK_EQ( ecu_io_init( 0u ), HAL_OK );
    ( void )gauge_init( &pins, NEXT_ID, ACTION_ID );
}

/* Where the message "tx" first holds this command at or after from. */
static inline size_t find_cmd(
    size_t from,
    uint8_t b0,
    uint8_t b1,
    uint8_t b2
)
{
    size_t found = tx_count;

    for ( size_t i = from; ( found == tx_count ) && ( ( i + 3u ) < tx_count );
          i++ )
    {
        if ( ( tx_log[i] == b0 ) && ( tx_log[i + 1u] == b1 )
             && ( tx_log[i + 2u] == b2 ) )
        {
            found = i;
        }
    }
    return found;
}

#endif /* SIL_HARNESS_H */
