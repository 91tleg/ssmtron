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
/* The widest identity text a protocol may give, with its NUL. */
#define INFO_ID_MAX ( INFO_ROW_W + 1u )

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

/* The ECU's identity as the protocol words it, or dashes until the ECU has
 * identified itself. */
static void info_ecu_id_text( char * buf )
{
    char id_text[INFO_ID_MAX];

    ecu_id_text( id_text, sizeof( id_text ) );
    put_row( buf, "ECU ID", id_text );
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
    UI_CENTERED_WIDGET(
        20,
        INFO_SCALE,
        UI_CY - UI_LEN( 800 ),
        info_title_text,
        TEXT_ALIGN_CENTER
    ),
    UI_CENTERED_WIDGET(
        20, INFO_SCALE, INFO_ROW_Y( 0 ), info_version_text, TEXT_ALIGN_LEFT
    ),
    UI_CENTERED_WIDGET(
        20, INFO_SCALE, INFO_ROW_Y( 1 ), info_ecu_id_text, TEXT_ALIGN_LEFT
    ),
    UI_CENTERED_WIDGET(
        20, INFO_SCALE, INFO_ROW_Y( 2 ), info_link_text, TEXT_ALIGN_LEFT
    ),
    UI_CENTERED_WIDGET(
        20,
        INFO_SCALE,
        UI_CY + UI_LEN( 550 ),
        info_notice_text,
        TEXT_ALIGN_CENTER
    ),
};
#define INFO_PAGE_COUNT ( sizeof( info_page ) / sizeof( info_page[0] ) )

void gauge_info_set_colors(
    uint16_t face,
    uint16_t ink
)
{
    text_set_colors( info_page, INFO_PAGE_COUNT, face, ink );
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
