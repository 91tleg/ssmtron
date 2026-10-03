/* The GC9A01 driver's power-up behavior against fake HAL calls: the panel and
 * the backlight stay off until display_on, a board with no backlight line
 * never has it touched, and the init commands start in the order the
 * controller needs. The rest of the bytes sent to the panel are not checked. */
#include <stdbool.h>

#include "lib/display/display.h"
#include "test/minitest.h"

#define BACKLIGHT_ID ( 9u )

#define DC_ID       ( 2u )
#define CMD_LOG_MAX ( 64u )

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
    ( void )cfg;
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

int main( void )
{
    test_the_backlight_is_off_until_display_on();
    test_a_board_without_a_backlight_line_leaves_it_alone();
    test_init_unlocks_the_extended_registers_first();
    return MT_RESULT();
}
