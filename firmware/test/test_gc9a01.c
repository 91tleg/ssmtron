/* The GC9A01 driver against fake HAL calls: the panel and the backlight stay
 * off until display_on, a board with no backlight line never has it touched,
 * the init commands start in the order the controller needs, and drawing
 * puts the right bytes on the wire with chip select held for the whole
 * window and the pixels in a few long transfers. */
#include <stdbool.h>
#include <stddef.h>

#include "lib/display/display.h"
#include "test/minitest.h"

#define BACKLIGHT_ID ( 9u )

#define CS_ID       ( 1u )
#define DC_ID       ( 2u )
#define CMD_LOG_MAX ( 64u )
#define XFER_MAX    ( 512u )

/* Every transfer: with DC high or low, its length and its first bytes. */
typedef struct
{
    bool data;
    size_t len;
    uint8_t first[8];
    uint8_t last[2];
} xfer_t;

static xfer_t xfers[XFER_MAX];
static unsigned xfer_count;
static unsigned cs_selects;
static uint32_t opened_hz;
static bool dc_level;
static uint8_t cmd_log[CMD_LOG_MAX];
static unsigned cmd_count;
static unsigned backlight_opens;
static unsigned backlight_writes;
static bool backlight_level;

hal_status_t hal_gpio_open(
    hal_gpio_id_t id,
    hal_gpio_mode_t mode
)
{
    ( void )mode;
    if ( id == BACKLIGHT_ID )
    {
        backlight_opens++;
    }
    return HAL_OK;
}

void hal_gpio_write(
    hal_gpio_id_t id,
    bool asserted
)
{
    if ( id == DC_ID )
    {
        dc_level = asserted;
    }
    if ( ( id == CS_ID ) && !asserted )
    {
        cs_selects++; /* active low: written low means selected */
    }
    if ( id == BACKLIGHT_ID )
    {
        backlight_writes++;
        backlight_level = asserted;
    }
}

hal_status_t hal_spi_open(
    hal_spi_id_t id,
    const hal_spi_cfg_t * cfg
)
{
    ( void )id;
    opened_hz = cfg->freq_hz;
    return HAL_OK;
}

hal_status_t hal_spi_transfer(
    hal_spi_id_t id,
    const uint8_t * tx,
    uint8_t * rx,
    size_t len
)
{
    ( void )id;
    ( void )rx;
    if ( xfer_count < XFER_MAX )
    {
        xfer_t * x = &xfers[xfer_count];

        x->data = dc_level;
        x->len  = len;
        for ( size_t i = 0u; ( i < 8u ) && ( i < len ); i++ )
        {
            x->first[i] = tx[i];
        }
        if ( len >= 2u )
        {
            x->last[0] = tx[len - 2u];
            x->last[1] = tx[len - 1u];
        }
        xfer_count++;
    }
    /* A command is a transfer with DC low. */
    if ( !dc_level && ( len > 0u ) && ( cmd_count < CMD_LOG_MAX ) )
    {
        cmd_log[cmd_count] = tx[0];
        cmd_count++;
    }
    return HAL_OK;
}

void hal_delay_ms( uint32_t ms )
{
    ( void )ms;
}

static display_pins_t pins_with( bool has_backlight )
{
    display_pins_t p = {
        .spi           = 0u,
        .cs            = 1u,
        .dc            = 2u,
        .reset         = 3u,
        .backlight     = BACKLIGHT_ID,
        .has_backlight = has_backlight
    };

    return p;
}

static void test_the_backlight_is_off_until_display_on( void )
{
    const display_pins_t pins = pins_with( true );

    CHECK_EQ( display_init( &pins ), HAL_OK );
    CHECK_EQ( backlight_opens, 1u );
    /* Off after init: written low, not left to whatever the pin held. */
    CHECK( backlight_writes >= 1u );
    CHECK( !backlight_level );

    CHECK_EQ( display_on(), HAL_OK );
    CHECK( backlight_level ); /* now it is lit */
}

static void test_a_board_without_a_backlight_line_leaves_it_alone( void )
{
    const display_pins_t pins = pins_with( false );
    unsigned opens_before     = backlight_opens;
    unsigned writes_before    = backlight_writes;

    CHECK_EQ( display_init( &pins ), HAL_OK );
    CHECK_EQ( display_on(), HAL_OK );
    CHECK_EQ( backlight_opens, opens_before );
    CHECK_EQ( backlight_writes, writes_before );
}

/* The extended registers only take values once FEh and EFh have unlocked
 * them, so those come right after the software reset and before the first
 * register. Zephyr's gc9x01x driver sends them first for the same reason. */
static void test_init_unlocks_the_extended_registers_first( void )
{
    const display_pins_t pins = pins_with( false );

    cmd_count = 0u;
    CHECK_EQ( display_init( &pins ), HAL_OK );
    CHECK( cmd_count > 4u );
    CHECK_EQ( cmd_log[0], 0x01u ); /* software reset */
    CHECK_EQ( cmd_log[1], 0xFEu );
    CHECK_EQ( cmd_log[2], 0xEFu );
    CHECK_EQ( cmd_log[3], 0xEBu ); /* the first extended register */
}

static void forget_wire( void )
{
    xfer_count = 0u;
    cs_selects = 0u;
}

/* A window: three commands with their address bytes, then the pixels, high
 * byte first. Chip select goes low once for all of it. */
static void test_a_blit_holds_chip_select_and_sends_big_endian_pixels( void )
{
    const display_pins_t pins   = pins_with( false );
    static const uint16_t px[6] = { 0x1234u, 0xABCDu, 0, 0, 0, 0x00FFu };

    CHECK_EQ( display_init( &pins ), HAL_OK );
    forget_wire();
    CHECK_EQ( display_blit( 10u, 20u, 3u, 2u, px ), HAL_OK );
    CHECK_EQ( cs_selects, 1u );
    CHECK_EQ( xfer_count, 6u ); /* 3 commands, 2 address pairs, the pixels */
    CHECK( !xfers[0].data );
    CHECK_EQ( xfers[0].first[0], 0x2Au );
    CHECK( xfers[1].data );
    CHECK_EQ( xfers[1].len, 4u );
    CHECK_EQ( xfers[1].first[1], 10u ); /* first column */
    CHECK_EQ( xfers[1].first[3], 12u ); /* last column */
    CHECK_EQ( xfers[2].first[0], 0x2Bu );
    CHECK_EQ( xfers[3].first[1], 20u ); /* first row */
    CHECK_EQ( xfers[3].first[3], 21u ); /* last row */
    CHECK_EQ( xfers[4].first[0], 0x2Cu );
    CHECK( xfers[5].data );
    CHECK_EQ( xfers[5].len, 12u ); /* 6 pixels, one transfer */
    CHECK_EQ( xfers[5].first[0], 0x12u );
    CHECK_EQ( xfers[5].first[1], 0x34u );
    CHECK_EQ( xfers[5].first[2], 0xABu );
    CHECK_EQ( xfers[5].first[3], 0xCDu );
    CHECK_EQ( xfers[5].last[0], 0x00u );
    CHECK_EQ( xfers[5].last[1], 0xFFu );
}

/* The whole panel in one color: one window, and the pixels in long transfers
 * whose lengths add up to every pixel twice. */
static void test_clearing_sends_every_pixel_in_a_few_long_transfers( void )
{
    const display_pins_t pins = pins_with( false );
    size_t bytes              = 0u;
    size_t longest            = 0u;

    CHECK_EQ( display_init( &pins ), HAL_OK );
    forget_wire();
    CHECK_EQ( display_clear( 0xF800u ), HAL_OK );
    CHECK_EQ( cs_selects, 1u );
    for ( unsigned i = 5u; i < xfer_count; i++ )
    {
        CHECK( xfers[i].data );
        CHECK_EQ( xfers[i].first[0], 0xF8u );
        CHECK_EQ( xfers[i].first[1], 0x00u );
        bytes += xfers[i].len;
        longest = ( xfers[i].len > longest ) ? xfers[i].len : longest;
    }
    CHECK_EQ( bytes, ( size_t )DISPLAY_WIDTH * DISPLAY_HEIGHT * 2u );
    CHECK( longest >= 1024u ); /* long transfers, not one per row */
    CHECK( ( xfer_count - 5u ) <= 120u );
}

/* The board sets the bus clock; 0 means the driver's own default. */
static void test_the_bus_clock_comes_from_the_board( void )
{
    display_pins_t pins = pins_with( false );

    CHECK_EQ( display_init( &pins ), HAL_OK );
    CHECK_EQ( opened_hz, 8000000u );
    pins.spi_hz = 16000000u;
    CHECK_EQ( display_init( &pins ), HAL_OK );
    CHECK_EQ( opened_hz, 16000000u );
}

int main( void )
{
    test_the_backlight_is_off_until_display_on();
    test_a_board_without_a_backlight_line_leaves_it_alone();
    test_init_unlocks_the_extended_registers_first();
    test_a_blit_holds_chip_select_and_sends_big_endian_pixels();
    test_clearing_sends_every_pixel_in_a_few_long_transfers();
    test_the_bus_clock_comes_from_the_board();
    return MT_RESULT();
}
