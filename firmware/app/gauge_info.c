#include "app/gauge_info.h"

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "app/fw_info.h"
#include "lib/ecu/ecu.h"
#include "lib/textfit/textfit.h"
#include "lib/ui/text.h"
#include "lib/ui/ui_geom.h"

/* Each row is a label, padded so the values line up, and then its value.
 * A row holds at most 20 glyphs, so a long value is cut. */
#define INFO_LABEL_W ( 8u )
#define INFO_ROW_W   ( 20u )

/* The text the notice line shows. The callbacks take no context, so
 * gauge_info_poll sets this before every redraw. */
static const char * shown_notice = "";

/* The page's rows, cut to INFO_ROW_W glyphs. */
static void put_row(
    char * buf,
    const char * label,
    const char * value
)
{
    ( void )textfit_row(
        buf, INFO_ROW_W + 1u, INFO_LABEL_W, INFO_ROW_W, label, value
    );
}

/* Text cut to INFO_ROW_W glyphs. Its widget centers it. */
static void put_text(
    char * buf,
    const char * s
)
{
    ( void )textfit_copy( buf, INFO_ROW_W + 1u, s );
}

static void info_title_text( char * buf )
{
    put_text( buf, "INFO" );
}

static void info_version_text( char * buf )
{
    put_row( buf, "FW", fw_info.version );
}

/* The three ROM ID bytes in the order the ECU sent them, as xx.xx.xx, or
 * dashes until the ECU has identified itself. Only the first two bytes pick
 * the profile; the third is shown for reference. */
static void info_rom_id_text( char * buf )
{
    static const char hex[] = "0123456789ABCDEF";
    char id_text[9];
    uint32_t id = ecu_rom_id();

    if ( id == 0u )
    {
        ( void )memcpy( id_text, "--.--.--", sizeof( id_text ) );
    }
    else
    {
        uint32_t bits = id;

        for ( size_t byte = 0; byte < 3u; byte++ )
        {
            uint32_t value = bits & 0xFFu;

            bits >>= 8u;
            id_text[byte * 3u]          = hex[value >> 4];
            id_text[( byte * 3u ) + 1u] = hex[value & 0x0Fu];
            id_text[( byte * 3u ) + 2u] = ( byte < 2u ) ? '.' : '\0';
        }
    }
    put_row( buf, "ROM ID", id_text );
}

static void info_link_text( char * buf )
{
    put_row( buf, "ECU", ( ecu_link() == ECU_LINK_UP ) ? "LINKED" : "NO LINK" );
}

static void info_notice_text( char * buf )
{
    put_text( buf, shown_notice );
}

/* Text sizes, chosen for the 240 px panel. */
#define INFO_SCALE UI_TEXT_SCALE( 1 )

/* Rows sit as a block centered on the panel, 134 thousandths of the radius
 * apart, like the flags page. The notice line is below them. */
#define INFO_ROW_Y( n ) ( UI_CY - UI_LEN( 200 ) + ( ( n ) * UI_LEN( 134 ) ) )

static text_widget_t info_page[] = {
    { .rect  = UI_CENTERED_RECT( 20, INFO_SCALE, UI_CY - UI_LEN( 800 ) ),
      .scale = INFO_SCALE,
      .fg    = 0u,
      .bg    = 0u,
      .text  = info_title_text,
      .cache = { 0 },
      .align = TEXT_ALIGN_CENTER },
    { .rect  = UI_CENTERED_RECT( 20, INFO_SCALE, INFO_ROW_Y( 0 ) ),
      .scale = INFO_SCALE,
      .fg    = 0u,
      .bg    = 0u,
      .text  = info_version_text,
      .cache = { 0 },
      .align = TEXT_ALIGN_LEFT },
    { .rect  = UI_CENTERED_RECT( 20, INFO_SCALE, INFO_ROW_Y( 1 ) ),
      .scale = INFO_SCALE,
      .fg    = 0u,
      .bg    = 0u,
      .text  = info_rom_id_text,
      .cache = { 0 },
      .align = TEXT_ALIGN_LEFT },
    { .rect  = UI_CENTERED_RECT( 20, INFO_SCALE, INFO_ROW_Y( 2 ) ),
      .scale = INFO_SCALE,
      .fg    = 0u,
      .bg    = 0u,
      .text  = info_link_text,
      .cache = { 0 },
      .align = TEXT_ALIGN_LEFT },
    { .rect  = UI_CENTERED_RECT( 20, INFO_SCALE, UI_CY + UI_LEN( 550 ) ),
      .scale = INFO_SCALE,
      .fg    = 0u,
      .bg    = 0u,
      .text  = info_notice_text,
      .cache = { 0 },
      .align = TEXT_ALIGN_CENTER },
};
#define INFO_PAGE_COUNT ( sizeof( info_page ) / sizeof( info_page[0] ) )

void gauge_info_set_colors(
    uint16_t face,
    uint16_t ink
)
{
    for ( size_t i = 0; i < INFO_PAGE_COUNT; i++ )
    {
        info_page[i].bg = face;
        info_page[i].fg = ink;
    }
}

/* The callbacks draw from this, so it is set before anything is drawn. */
static void info_show( const char * notice )
{
    shown_notice = ( notice != NULL ) ? notice : "";
}

void gauge_info_open(
    uint16_t face,
    const char * notice
)
{
    info_show( notice );
    text_init( info_page, INFO_PAGE_COUNT, face );
}

void gauge_info_poll( const char * notice )
{
    info_show( notice );
    text_poll( info_page, INFO_PAGE_COUNT );
}
