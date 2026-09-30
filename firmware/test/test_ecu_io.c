/* The app's HAL <-> ECU glue, tested in isolation: the HAL UART and the
 * ECU library are both replaced by fakes defined here. */
#include <string.h>

#include "lib/ecu/ecu.h"
#include "app/ecu_io.h"
#include "test/minitest.h"

static char log_[256];
static size_t log_len;
static uint8_t fed[64];
static size_t fed_len;
static uint32_t last_tick;
static bool inited;
static ecu_link_cfg_t link_cfg = {
    .baud = 1953u, .data_bits = 8u, .parity = ECU_PARITY_EVEN, .stop_bits = 1u
};
static uint8_t next_tx[8];

static void note( char c )
{
    if ( log_len < sizeof( log_ ) - 1 )
    {
        log_[log_len++] = c;
        log_[log_len]   = '\0';
    }
}

const ecu_link_cfg_t * ecu_link_config( void )
{
    return &link_cfg;
}

void ecu_init( void )
{
    inited = true;
    note( 'I' );
}

void ecu_tick( uint32_t now_ms )
{
    last_tick = now_ms;
    note( 'T' );
}

void ecu_feed( uint8_t byte )
{
    if ( fed_len < sizeof( fed ) )
    {
        fed[fed_len++] = byte;
    }
    note( 'B' );
}

void ecu_feed_error( void )
{
    note( 'E' );
}

static size_t tx_len;

size_t ecu_next_tx(
    uint8_t * buf,
    size_t max
)
{
    size_t n = ( tx_len <= max ) ? tx_len : 0u;

    memcpy( buf, next_tx, n );
    note( 'N' );
    return n;
}

typedef struct
{
    hal_uart_rx_t kind;
    uint8_t byte;
} rx_item_t;

static rx_item_t rx_q[512];
static size_t rx_head;
static size_t rx_tail;
static hal_status_t open_result;
static hal_uart_id_t opened_id;
static hal_uart_cfg_t opened_cfg;
static unsigned open_calls;
static uint8_t written[64];
static size_t written_len;
static hal_uart_id_t read_id;
static hal_uart_id_t write_id;

hal_status_t hal_uart_open(
    hal_uart_id_t id,
    const hal_uart_cfg_t * cfg
)
{
    open_calls++;
    opened_id  = id;
    opened_cfg = *cfg;
    return open_result;
}

size_t hal_uart_write(
    hal_uart_id_t id,
    const uint8_t * buf,
    size_t len
)
{
    write_id = id;
    memcpy( written + written_len, buf, len );
    written_len += len;
    note( 'W' );
    return len;
}

hal_uart_rx_t hal_uart_read(
    hal_uart_id_t id,
    uint8_t * byte
)
{
    hal_uart_rx_t r = HAL_UART_RX_EMPTY;

    read_id = id;
    if ( rx_head != rx_tail )
    {
        r = rx_q[rx_tail].kind;
        if ( r == HAL_UART_RX_BYTE )
        {
            *byte = rx_q[rx_tail].byte;
        }
        rx_tail++;
    }
    return r;
}

static void reset( void )
{
    log_len     = 0;
    log_[0]     = '\0';
    fed_len     = 0;
    inited      = false;
    rx_head     = 0;
    rx_tail     = 0;
    open_result = HAL_OK;
    open_calls  = 0;
    written_len = 0;
    tx_len      = 0;
    last_tick   = 0;
    link_cfg    = ( ecu_link_cfg_t ){ .baud      = 1953u,
                                      .data_bits = 8u,
                                      .parity    = ECU_PARITY_EVEN,
                                      .stop_bits = 1u };
}

static void queue_byte( uint8_t b )
{
    rx_q[rx_head++] = ( rx_item_t ){ HAL_UART_RX_BYTE, b };
}

static void queue_error( void )
{
    rx_q[rx_head++] = ( rx_item_t ){ HAL_UART_RX_ERROR, 0 };
}

static void test_init_applies_the_protocols_link_config( void )
{
    reset();
    CHECK_EQ( ecu_io_init( 3 ), HAL_OK );
    CHECK_EQ( opened_id, 3 );
    CHECK_EQ( opened_cfg.baud, 1953 );
    CHECK_EQ( opened_cfg.data_bits, 8 );
    CHECK( opened_cfg.parity == HAL_UART_PARITY_EVEN );
    CHECK_EQ( opened_cfg.stop_bits, 1 );
    CHECK( inited );
}

static void test_parity_mapping( void )
{
    reset();
    link_cfg.parity = ECU_PARITY_NONE;
    ecu_io_init( 0 );
    CHECK( opened_cfg.parity == HAL_UART_PARITY_NONE );
    link_cfg.parity = ECU_PARITY_ODD;
    ecu_io_init( 0 );
    CHECK( opened_cfg.parity == HAL_UART_PARITY_ODD );
}

static void test_failed_open_does_not_start_the_protocol( void )
{
    reset();
    open_result = HAL_ERR_UNSUPPORTED;
    CHECK_EQ( ecu_io_init( 0 ), HAL_ERR_UNSUPPORTED );
    CHECK( !inited );
}

static void test_bytes_and_errors_reach_the_ecu_in_order( void )
{
    reset();
    ecu_io_init( 2 );
    log_len = 0;
    queue_byte( 0x11 );
    queue_byte( 0x22 );
    queue_error();
    queue_byte( 0x33 );
    queue_error();
    queue_error();
    ecu_io_poll( 1234 );

    CHECK_EQ( last_tick, 1234 );
    /* tick first, then the stream exactly as the HAL reported it, then
     * the transmit poll */
    CHECK( strcmp( log_, "TBBEBEEN" ) == 0 );
    CHECK_EQ( fed_len, 3 );
    CHECK( ( fed[0] == 0x11 ) && ( fed[1] == 0x22 ) && ( fed[2] == 0x33 ) );
    CHECK_EQ( read_id, 2 );
}

static void test_poll_with_nothing_pending( void )
{
    reset();
    ecu_io_init( 0 );
    log_len = 0;
    ecu_io_poll( 5 );
    CHECK( strcmp( log_, "TN" ) == 0 );
    CHECK_EQ( written_len, 0 );
}

static void test_transmit_goes_to_the_same_uart( void )
{
    reset();
    ecu_io_init( 4 );
    next_tx[0] = 0x12;
    next_tx[1] = 0x00;
    next_tx[2] = 0x00;
    next_tx[3] = 0x00;
    tx_len     = 4;
    ecu_io_poll( 0 );
    CHECK_EQ( written_len, 4 );
    CHECK( written[0] == 0x12 );
    CHECK_EQ( write_id, 4 );
}

static void test_receive_burst_is_bounded( void )
{
    reset();
    ecu_io_init( 0 );
    for ( unsigned i = 0; i < 300; i++ )
    {
        queue_byte( ( uint8_t )i );
    }
    ecu_io_poll( 0 );
    CHECK_EQ( fed_len, 64 ); /* the rest waits for the next poll */
    fed_len = 0;
    ecu_io_poll( 1 );
    CHECK_EQ( fed_len, 64 );
}

int main( void )
{
    test_init_applies_the_protocols_link_config();
    test_parity_mapping();
    test_failed_open_does_not_start_the_protocol();
    test_bytes_and_errors_reach_the_ecu_in_order();
    test_poll_with_nothing_pending();
    test_transmit_goes_to_the_same_uart();
    test_receive_burst_is_bounded();

    return MT_RESULT();
}
