#include "app/gauge_number.h"

#include <stddef.h>

#include "lib/fmt/fmt.h"
#include "lib/textfit/textfit.h"
#include "lib/ui/text.h"
#include "lib/ui/ui_geom.h"

/* A text widget's buffer: TEXT_MAX characters and the NUL. */
#define TEXT_BUF ( TEXT_MAX + 1u )

/* What the callbacks show. They take no context, so gauge_number_poll sets
 * these before every redraw. */
static const ecu_param_info_t * number_info;
static bool number_has_value;
static int32_t number_value;
static const char * number_notice = "";

static void name_text( char * buf )
{
    if ( number_notice[0] != '\0' )
    {
        ( void )textfit_copy( buf, TEXT_BUF, number_notice );
    }
    else
    {
        /* The page is named even before the ECU has sent a value. */
        ( void )textfit_copy(
            buf, TEXT_BUF, ( number_info != NULL ) ? number_info->name : ""
        );
    }
}

/* Dashes stand in for a value the ECU has not sent, as a dial rests. */
static void value_text( char * buf )
{
    if ( ( number_info != NULL ) && number_has_value )
    {
        ( void )fmt_value( buf, TEXT_BUF, number_info, number_value );
    }
    else
    {
        ( void )textfit_copy( buf, TEXT_BUF, "--" );
    }
}

static void unit_text( char * buf )
{
    ( void )textfit_copy(
        buf,
        TEXT_BUF,
        ( ( number_info != NULL ) && number_has_value )
            ? ecu_unit( number_info, number_value )
            : ""
    );
}

/* Rects are kept a safe margin inside the round panel's 120 px radius, not
 * tiled edge-to-edge like a square display would allow. Text sizes are
 * chosen for the 240 px panel. */
#define NAME_SCALE  UI_TEXT_SCALE( 1 )
#define VALUE_SCALE UI_TEXT_SCALE( 4 )
#define UNIT_SCALE  UI_TEXT_SCALE( 2 )

static text_widget_t number_page[] = {
    /* The name sits where the dials put their label: centered, above the
     * center. */
    { .rect  = UI_CENTERED_RECT( 24, NAME_SCALE, UI_CY - UI_LEN( 259 ) ),
      .scale = NAME_SCALE,
      .fg    = 0u,
      .bg    = 0u,
      .text  = name_text,
      .cache = { 0 },
      .align = TEXT_ALIGN_CENTER },
    { .rect  = UI_CENTERED_RECT( 8, VALUE_SCALE, UI_CY - UI_LEN( 134 ) ),
      .scale = VALUE_SCALE,
      .fg    = 0u,
      .bg    = 0u,
      .text  = value_text,
      .cache = { 0 },
      .align = TEXT_ALIGN_CENTER },
    { .rect  = UI_CENTERED_RECT( 12, UNIT_SCALE, UI_CY + UI_LEN( 167 ) ),
      .scale = UNIT_SCALE,
      .fg    = 0u,
      .bg    = 0u,
      .text  = unit_text,
      .cache = { 0 },
      .align = TEXT_ALIGN_CENTER },
};
#define NUMBER_PAGE_COUNT ( sizeof( number_page ) / sizeof( number_page[0] ) )

void gauge_number_set_colors(
    uint16_t face,
    uint16_t ink
)
{
    for ( size_t i = 0; i < NUMBER_PAGE_COUNT; i++ )
    {
        number_page[i].bg = face;
        number_page[i].fg = ink;
    }
}

/* The callbacks draw from these, so they are set before anything is drawn. */
static void number_show(
    const ecu_param_info_t * info,
    bool has_value,
    int32_t value,
    const char * notice
)
{
    number_info      = info;
    number_has_value = has_value;
    number_value     = value;
    number_notice    = ( notice != NULL ) ? notice : "";
}

void gauge_number_open(
    uint16_t face,
    const ecu_param_info_t * info,
    bool has_value,
    int32_t value,
    const char * notice
)
{
    number_show( info, has_value, value, notice );
    text_init( number_page, NUMBER_PAGE_COUNT, face );
}

void gauge_number_poll(
    const ecu_param_info_t * info,
    bool has_value,
    int32_t value,
    const char * notice
)
{
    number_show( info, has_value, value, notice );
    text_poll( number_page, NUMBER_PAGE_COUNT );
}
