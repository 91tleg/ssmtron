/* Blocking, single-instance GC9A01 SPI display driver.
 * Verify init_steps against the panel vendor's values before use on
 * hardware; gamma and voltage settings can vary between panels. */
#include <stddef.h>

#include "hal_time.h"
#include "display.h"

/* The GC9A01 is a 240 x 240 chip; a different size needs a different
 * driver, so a mismatch stops the build instead of drawing wrongly. */
#if ( DISPLAY_WIDTH != 240u ) || ( DISPLAY_HEIGHT != 240u )
#error                                                                         \
    "gc9a01 drives a 240 x 240 panel: set DISPLAY_WIDTH and DISPLAY_HEIGHT to 240"
#endif

#define PANEL_W     ( DISPLAY_WIDTH )
#define PANEL_H     ( DISPLAY_HEIGHT )
#define PANEL_MAX_X ( PANEL_W - 1u )
#define PANEL_MAX_Y ( PANEL_H - 1u )

/* Conservative speed until verified with the panel and wiring. */
#define SPI_FREQ_HZ ( 8000000u )

#define CMD_SWRESET ( 0x01u )
#define CMD_SLPOUT  ( 0x11u )
#define CMD_INVON   ( 0x21u )
#define CMD_DISPON  ( 0x29u )
#define CMD_CASET   ( 0x2Au )
#define CMD_RASET   ( 0x2Bu )
#define CMD_RAMWR   ( 0x2Cu )
#define CMD_MADCTL  ( 0x36u )
#define CMD_COLMOD  ( 0x3Au )

typedef struct
{
    uint8_t cmd;
    uint8_t len; /* data bytes that follow */
    uint8_t delay_ms;
    const uint8_t * data;
} init_step_t;

static display_pins_t pins;
static bool ready;

static void cs_select( bool selected )
{
    /* Active low. */
    hal_gpio_write( pins.cs, !selected );
}

static void raw_write(
    bool is_data,
    const uint8_t * buf,
    size_t len
)
{
    hal_gpio_write( pins.dc, is_data );
    cs_select( true );
    ( void )hal_spi_transfer( pins.spi, buf, NULL, len );
    cs_select( false );
}

static void write_cmd( uint8_t cmd )
{
    raw_write( false, &cmd, 1u );
}

static void write_data(
    const uint8_t * data,
    size_t len
)
{
    if ( len > 0u )
    {
        raw_write( true, data, len );
    }
}

static void run_init_steps( void )
{
    /* clang-format off */
    static const uint8_t d_eb[]     = { 0x14u };
    static const uint8_t d_84[]     = { 0x40u };
    static const uint8_t d_85[]     = { 0xFFu };
    static const uint8_t d_86[]     = { 0xFFu };
    static const uint8_t d_87[]     = { 0xFFu };
    static const uint8_t d_88[]     = { 0x0Au };
    static const uint8_t d_89[]     = { 0x21u };
    static const uint8_t d_8a[]     = { 0x00u };
    static const uint8_t d_8b[]     = { 0x80u };
    static const uint8_t d_8c[]     = { 0x01u };
    static const uint8_t d_8d[]     = { 0x01u };
    static const uint8_t d_8e[]     = { 0xFFu };
    static const uint8_t d_8f[]     = { 0xFFu };
    static const uint8_t d_b6[]     = { 0x00u, 0x00u };
    static const uint8_t d_madctl[] = { 0x48u };
    static const uint8_t d_colmod[] = { 0x05u }; /* 16 bpp / RGB565 */
    static const uint8_t d_90[]     = { 0x08u, 0x08u, 0x08u, 0x08u };
    static const uint8_t d_bd[]     = { 0x06u };
    static const uint8_t d_bc[]     = { 0x00u };
    static const uint8_t d_ff[]     = { 0x60u, 0x01u, 0x04u };
    static const uint8_t d_c3[]     = { 0x13u };
    static const uint8_t d_c4[]     = { 0x13u };
    static const uint8_t d_c9[]     = { 0x22u };
    static const uint8_t d_be[]     = { 0x11u };
    static const uint8_t d_e1[]     = { 0x10u, 0x0Eu };
    static const uint8_t d_df[]     = { 0x21u, 0x0Cu, 0x02u };
    static const uint8_t d_f0[]     = { 0x45u, 0x09u, 0x08u, 0x08u, 0x26u, 0x2Au };
    static const uint8_t d_f1[]     = { 0x43u, 0x70u, 0x72u, 0x36u, 0x37u, 0x6Fu };
    static const uint8_t d_f2[]     = { 0x45u, 0x09u, 0x08u, 0x08u, 0x26u, 0x2Au };
    static const uint8_t d_f3[]     = { 0x43u, 0x70u, 0x72u, 0x36u, 0x37u, 0x6Fu };
    static const uint8_t d_ed[]     = { 0x1Bu, 0x0Bu };
    static const uint8_t d_ae[]     = { 0x77u };
    static const uint8_t d_cd[]     = { 0x63u };
    static const uint8_t d_70[]     = { 0x07u, 0x07u, 0x04u, 0x0Eu, 0x0Fu, 0x09u,
                                         0x07u, 0x08u, 0x03u };
    static const uint8_t d_e8[]     = { 0x34u };
    static const uint8_t d_62[]     = { 0x18u, 0x0Du, 0x71u, 0xEDu, 0x70u, 0x70u,
                                         0x18u, 0x0Fu, 0x71u, 0xEFu, 0x70u, 0x70u };
    static const uint8_t d_63[]     = { 0x18u, 0x11u, 0x71u, 0xF1u, 0x70u, 0x70u,
                                         0x18u, 0x13u, 0x71u, 0xF3u, 0x70u, 0x70u };
    static const uint8_t d_64[]     = { 0x28u, 0x29u, 0xF1u, 0x01u, 0xF1u, 0x00u,
                                         0x07u };
    static const uint8_t d_66[]     = { 0x3Cu, 0x00u, 0xCDu, 0x67u, 0x45u, 0x45u,
                                         0x10u, 0x00u, 0x00u, 0x00u };
    static const uint8_t d_67[]     = { 0x00u, 0x3Cu, 0x00u, 0x00u, 0x00u, 0x01u,
                                         0x54u, 0x10u, 0x32u, 0x98u };
    static const uint8_t d_74[]     = { 0x10u, 0x85u, 0x80u, 0x00u, 0x00u, 0x4Eu,
                                         0x00u };
    static const uint8_t d_98[]     = { 0x3Eu, 0x07u };
    /* {command, data length, delay after (ms), data} */
    static const init_step_t init_steps[] = {
        { 0xEFu, 0, 0, NULL },
        { 0xEBu, 1, 0, d_eb },
        { 0x84u, 1, 0, d_84 },
        { 0x85u, 1, 0, d_85 },
        { 0x86u, 1, 0, d_86 },
        { 0x87u, 1, 0, d_87 },
        { 0x88u, 1, 0, d_88 },
        { 0x89u, 1, 0, d_89 },
        { 0x8Au, 1, 0, d_8a },
        { 0x8Bu, 1, 0, d_8b },
        { 0x8Cu, 1, 0, d_8c },
        { 0x8Du, 1, 0, d_8d },
        { 0x8Eu, 1, 0, d_8e },
        { 0x8Fu, 1, 0, d_8f },
        { 0xB6u, 2, 0, d_b6 },
        { CMD_MADCTL, 1, 0, d_madctl },
        { CMD_COLMOD, 1, 0, d_colmod },
        { 0x90u, 4, 0, d_90 },
        { 0xBDu, 1, 0, d_bd },
        { 0xBCu, 1, 0, d_bc },
        { 0xFFu, 3, 0, d_ff },
        { 0xC3u, 1, 0, d_c3 },
        { 0xC4u, 1, 0, d_c4 },
        { 0xC9u, 1, 0, d_c9 },
        { 0xBEu, 1, 0, d_be },
        { 0xE1u, 2, 0, d_e1 },
        { 0xDFu, 3, 0, d_df },
        { 0xF0u, 6, 0, d_f0 },
        { 0xF1u, 6, 0, d_f1 },
        { 0xF2u, 6, 0, d_f2 },
        { 0xF3u, 6, 0, d_f3 },
        { 0xEDu, 2, 0, d_ed },
        { 0xAEu, 1, 0, d_ae },
        { 0xCDu, 1, 0, d_cd },
        { 0x70u, 9, 0, d_70 },
        { 0xE8u, 1, 0, d_e8 },
        { 0x62u, 12, 0, d_62 },
        { 0x63u, 12, 0, d_63 },
        { 0x64u, 7, 0, d_64 },
        { 0x66u, 10, 0, d_66 },
        { 0x67u, 10, 0, d_67 },
        { 0x74u, 7, 0, d_74 },
        { 0x98u, 2, 0, d_98 },
        { CMD_INVON, 0, 0, NULL },
        { CMD_SLPOUT, 0, 120, NULL },
        { CMD_DISPON, 0, 20, NULL },
    };
    /* clang-format on */
    const size_t step_count = sizeof( init_steps ) / sizeof( init_steps[0] );

    for ( size_t i = 0; i < step_count; i++ )
    {
        const init_step_t * s = &init_steps[i];

        write_cmd( s->cmd );
        write_data( s->data, s->len );
        if ( s->delay_ms > 0u )
        {
            hal_delay_ms( s->delay_ms );
        }
    }
}

static void hw_reset( void )
{
    /* Active-low reset pulse, then the controller's own boot time. */
    hal_gpio_write( pins.reset, false );
    hal_delay_ms( 10u );
    hal_gpio_write( pins.reset, true );
    hal_delay_ms( 120u );
}

static void set_window(
    uint16_t x0,
    uint16_t y0,
    uint16_t x1,
    uint16_t y1
)
{
    const uint8_t col[4] = {
        ( uint8_t )( x0 >> 8u ),
        ( uint8_t )x0,
        ( uint8_t )( x1 >> 8u ),
        ( uint8_t )x1
    };
    const uint8_t row[4] = {
        ( uint8_t )( y0 >> 8u ),
        ( uint8_t )y0,
        ( uint8_t )( y1 >> 8u ),
        ( uint8_t )y1
    };

    write_cmd( CMD_CASET );
    write_data( col, sizeof( col ) );
    write_cmd( CMD_RASET );
    write_data( row, sizeof( row ) );
    write_cmd( CMD_RAMWR );
}

hal_status_t display_init( const display_pins_t * p )
{
    hal_status_t status = HAL_ERR_INVALID;

    if ( p != NULL )
    {
        const hal_spi_cfg_t spi_cfg = { SPI_FREQ_HZ, HAL_SPI_MODE0 };

        pins = *p;
        if ( hal_gpio_open( pins.cs, HAL_GPIO_OUTPUT ) != HAL_OK )
        {
            status = HAL_ERR_IO;
        }
        else if ( hal_gpio_open( pins.dc, HAL_GPIO_OUTPUT ) != HAL_OK )
        {
            status = HAL_ERR_IO;
        }
        else if ( hal_gpio_open( pins.reset, HAL_GPIO_OUTPUT ) != HAL_OK )
        {
            status = HAL_ERR_IO;
        }
        else if ( pins.has_backlight
                  && ( hal_gpio_open( pins.backlight, HAL_GPIO_OUTPUT )
                       != HAL_OK ) )
        {
            status = HAL_ERR_IO;
        }
        else if ( hal_spi_open( pins.spi, &spi_cfg ) != HAL_OK )
        {
            status = HAL_ERR_IO;
        }
        else
        {
            cs_select( false );
            hw_reset();
            write_cmd( CMD_SWRESET );
            hal_delay_ms( 120u );
            run_init_steps();
            ready  = true;
            status = HAL_OK;
        }
    }
    return status;
}

/* Convert RGB565 to wire byte order one row at a time. */
#define ROW_BUF_PIXELS PANEL_W

static void write_pixel_row(
    const uint16_t * src,
    uint16_t count
)
{
    /* Guard against sending an unwritten row. */
    if ( count > 0u )
    {
        uint8_t row[ROW_BUF_PIXELS * 2u];

        for ( uint16_t i = 0; i < count; i++ )
        {
            row[( 2u * i )]      = ( uint8_t )( src[i] >> 8 );
            row[( 2u * i ) + 1u] = ( uint8_t )src[i];
        }
        ( void )hal_spi_transfer( pins.spi, row, NULL, ( size_t )count * 2u );
    }
}

hal_status_t display_blit(
    uint16_t x,
    uint16_t y,
    uint16_t w,
    uint16_t h,
    const uint16_t * pixels
)
{
    hal_status_t status = HAL_ERR_INVALID;
    bool fits = ready && ( pixels != NULL ) && ( w > 0u ) && ( h > 0u )
                && ( w <= PANEL_W )
                && ( ( ( uint32_t )x + ( uint32_t )w ) <= PANEL_W )
                && ( ( ( uint32_t )y + ( uint32_t )h ) <= PANEL_H );

    if ( fits )
    {
        set_window(
            x, y, ( uint16_t )( x + w - 1u ), ( uint16_t )( y + h - 1u )
        );
        hal_gpio_write( pins.dc, true );
        cs_select( true );
        for ( uint16_t row = 0; row < h; row++ )
        {
            write_pixel_row( &pixels[( size_t )row * w], w );
        }
        cs_select( false );
        status = HAL_OK;
    }
    return status;
}

hal_status_t display_clear( uint16_t color_rgb565 )
{
    hal_status_t status = HAL_ERR_INVALID;

    if ( ready )
    {
        uint16_t fill[ROW_BUF_PIXELS];

        for ( uint16_t i = 0; i < PANEL_W; i++ )
        {
            fill[i] = color_rgb565;
        }
        set_window( 0u, 0u, PANEL_MAX_X, PANEL_MAX_Y );
        hal_gpio_write( pins.dc, true );
        cs_select( true );
        for ( uint16_t row = 0; row < PANEL_H; row++ )
        {
            write_pixel_row( fill, PANEL_W );
        }
        cs_select( false );
        status = HAL_OK;
    }
    return status;
}
