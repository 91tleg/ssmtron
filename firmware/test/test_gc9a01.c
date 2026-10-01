/* The GC9A01 driver's power-up behavior against fake HAL calls: the panel and
 * the backlight stay off until display_on, and a board with no backlight
 * line never has it touched. The bytes sent to the panel are not checked. */
#include <stdbool.h>

#include "lib/display/display.h"
#include "test/minitest.h"

#define BACKLIGHT_ID ( 9u )

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
    ( void )tx;
    ( void )rx;
    ( void )len;
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

int main( void )
{
    test_the_backlight_is_off_until_display_on();
    test_a_board_without_a_backlight_line_leaves_it_alone();
    return MT_RESULT();
}
