#include "app/gauge_flags.h"

#include <string.h>

#include "lib/textfit/textfit.h"
#include "lib/ui/text.h"
#include "lib/ui/ui_geom.h"

/* The selected byte is 8 flags, one row per flag. Every name is always
 * printed, followed by that flag's state as 1 or 0, or - while the byte is
 * stale. A flag reads 1 when ecu_flag_set() says it is asserted, so an
 * active-low bit already comes out the right way round. The longest flag
 * name is 11 characters. */
#define FLAG_NAME_W   ( 11u )
#define FLAG_TITLE_W  ( 20u )
#define FLAG_STATUS_W ( 20u )

/* The same widths as plain ints, for sizing the widgets' rects. A row is
 * the name, a space and the state. */
#define FLAG_TITLE_CHARS  ( ( int )FLAG_TITLE_W )
#define FLAG_STATUS_CHARS ( ( int )FLAG_STATUS_W )
#define FLAG_ROW_CHARS    ( ( int )FLAG_NAME_W + 2 )

/* One dot per step, so the wait shows the unit has not hung. The clear
 * takes tens of seconds. The font has no | or \, so a spinning bar is not
 * possible. */
#define CLEAR_DOTS_MS ( 400u )
#define CLEAR_DOTS_N  ( 4u )

/* What the widget callbacks read. They take no context, so gauge_flags_poll
 * sets this before every redraw. */
static size_t shown_idx;
static flags_status_t shown_status = FLAGS_STATUS_NONE;
static uint32_t shown_now_ms;

bool gauge_flags_handles( const ecu_param_info_t * info )
{
    return ( info != NULL ) && ( info->decode == ECU_DEC_FLAGS );
}

/* The shown parameter if it is a flags byte, else NULL. */
static const ecu_param_info_t * flags_info( void )
{
    const ecu_param_info_t * info = NULL;

    if ( shown_idx < ecu_param_count() )
    {
        info = ecu_param_info( shown_idx );
        if ( !gauge_flags_handles( info ) )
        {
            info = NULL;
        }
    }
    return info;
}

static void title_text( char * buf )
{
    const ecu_param_info_t * info = flags_info();

    buf[0] = '\0';
    if ( info != NULL )
    {
        ( void )textfit_center( buf, TEXT_MAX + 1u, info->name, FLAG_TITLE_W );
    }
}

/* Clear and save progress sits under the flags, so the page stays put. */
static void status_text( char * buf )
{
    const char * text = "";

    switch ( shown_status )
    {
        case FLAGS_STATUS_HOLD_TO_CLEAR:
            text = "HOLD TO CLEAR";
            break;

        case FLAGS_STATUS_CLEARING:
        {
            /* Fixed width, so the text does not shift as dots come and go. */
            static const char frames[CLEAR_DOTS_N][12] = {
                "CLEARING   ", "CLEARING.  ", "CLEARING.. ", "CLEARING..."
            };

            text = frames[( shown_now_ms / CLEAR_DOTS_MS ) % CLEAR_DOTS_N];
            break;
        }

        case FLAGS_STATUS_CLEARED:
            text = "CLEARED";
            break;

        case FLAGS_STATUS_CLEAR_FAILED:
            text = "CLEAR FAILED";
            break;

        case FLAGS_STATUS_SAVED:
            text = "SAVED";
            break;

        case FLAGS_STATUS_SAVE_FAILED:
            text = "SAVE FAILED";
            break;

        case FLAGS_STATUS_NONE:
        default:
            break;
    }
    ( void )textfit_center( buf, TEXT_MAX + 1u, text, FLAG_STATUS_W );
}

/* Blank for rows past the last flag of this byte. */
static void row_text(
    char * buf,
    size_t row
)
{
    const ecu_param_info_t * info = flags_info();

    buf[0] = '\0';
    if ( ( info != NULL ) && ( row < info->flag_count ) )
    {
        const ecu_flag_t * flag = &info->flags[row];
        int32_t raw             = 0;
        char state              = '-';
        size_t n                = 0;

        if ( ecu_get_value( shown_idx, &raw ) )
        {
            state = ecu_flag_set( flag, ( uint8_t )raw ) ? '1' : '0';
        }
        /* Pad the name so the states line up in one column. */
        for ( ; n < FLAG_NAME_W; n++ )
        {
            buf[n] = ( n < strlen( flag->name ) ) ? flag->name[n] : ' ';
        }
        buf[n]      = ' ';
        buf[n + 1u] = state;
        buf[n + 2u] = '\0';
    }
}

static void row0( char * buf )
{
    row_text( buf, 0u );
}
static void row1( char * buf )
{
    row_text( buf, 1u );
}
static void row2( char * buf )
{
    row_text( buf, 2u );
}
static void row3( char * buf )
{
    row_text( buf, 3u );
}
static void row4( char * buf )
{
    row_text( buf, 4u );
}
static void row5( char * buf )
{
    row_text( buf, 5u );
}
static void row6( char * buf )
{
    row_text( buf, 6u );
}
static void row7( char * buf )
{
    row_text( buf, 7u );
}

/* Colors are set by gauge_flags_set_colors before the page is drawn.
 * Rows use the title's size and sit 134 thousandths of the radius apart,
 * with the first 600 above the center. The status line sits below the last
 * row. All of it fits inside the round panel. */
#define ROW_Y( n ) ( UI_CY - UI_LEN( 600 ) + ( ( n ) * UI_LEN( 134 ) ) )

/* The flags page is all one text size, chosen for the 240 px panel. */
#define FLAGS_SCALE UI_TEXT_SCALE( 1 )

static text_widget_t page[] = {
    { .rect = UI_CENTERED_RECT(
          FLAG_TITLE_CHARS, FLAGS_SCALE, UI_CY - UI_LEN( 800 )
      ),
      .scale = FLAGS_SCALE,
      .fg    = 0u,
      .bg    = 0u,
      .text  = title_text,
      .cache = { 0 },
      .align = TEXT_ALIGN_LEFT },
    { .rect  = UI_CENTERED_RECT( FLAG_ROW_CHARS, FLAGS_SCALE, ROW_Y( 0 ) ),
      .scale = FLAGS_SCALE,
      .fg    = 0u,
      .bg    = 0u,
      .text  = row0,
      .cache = { 0 },
      .align = TEXT_ALIGN_LEFT },
    { .rect  = UI_CENTERED_RECT( FLAG_ROW_CHARS, FLAGS_SCALE, ROW_Y( 1 ) ),
      .scale = FLAGS_SCALE,
      .fg    = 0u,
      .bg    = 0u,
      .text  = row1,
      .cache = { 0 },
      .align = TEXT_ALIGN_LEFT },
    { .rect  = UI_CENTERED_RECT( FLAG_ROW_CHARS, FLAGS_SCALE, ROW_Y( 2 ) ),
      .scale = FLAGS_SCALE,
      .fg    = 0u,
      .bg    = 0u,
      .text  = row2,
      .cache = { 0 },
      .align = TEXT_ALIGN_LEFT },
    { .rect  = UI_CENTERED_RECT( FLAG_ROW_CHARS, FLAGS_SCALE, ROW_Y( 3 ) ),
      .scale = FLAGS_SCALE,
      .fg    = 0u,
      .bg    = 0u,
      .text  = row3,
      .cache = { 0 },
      .align = TEXT_ALIGN_LEFT },
    { .rect  = UI_CENTERED_RECT( FLAG_ROW_CHARS, FLAGS_SCALE, ROW_Y( 4 ) ),
      .scale = FLAGS_SCALE,
      .fg    = 0u,
      .bg    = 0u,
      .text  = row4,
      .cache = { 0 },
      .align = TEXT_ALIGN_LEFT },
    { .rect  = UI_CENTERED_RECT( FLAG_ROW_CHARS, FLAGS_SCALE, ROW_Y( 5 ) ),
      .scale = FLAGS_SCALE,
      .fg    = 0u,
      .bg    = 0u,
      .text  = row5,
      .cache = { 0 },
      .align = TEXT_ALIGN_LEFT },
    { .rect  = UI_CENTERED_RECT( FLAG_ROW_CHARS, FLAGS_SCALE, ROW_Y( 6 ) ),
      .scale = FLAGS_SCALE,
      .fg    = 0u,
      .bg    = 0u,
      .text  = row6,
      .cache = { 0 },
      .align = TEXT_ALIGN_LEFT },
    { .rect  = UI_CENTERED_RECT( FLAG_ROW_CHARS, FLAGS_SCALE, ROW_Y( 7 ) ),
      .scale = FLAGS_SCALE,
      .fg    = 0u,
      .bg    = 0u,
      .text  = row7,
      .cache = { 0 },
      .align = TEXT_ALIGN_LEFT },
    { .rect = UI_CENTERED_RECT(
          FLAG_STATUS_CHARS, FLAGS_SCALE, UI_CY + UI_LEN( 550 )
      ),
      .scale = FLAGS_SCALE,
      .fg    = 0u,
      .bg    = 0u,
      .text  = status_text,
      .cache = { 0 },
      .align = TEXT_ALIGN_LEFT },
};
#define PAGE_COUNT ( sizeof( page ) / sizeof( page[0] ) )

void gauge_flags_set_colors(
    uint16_t face,
    uint16_t ink
)
{
    text_set_colors( page, PAGE_COUNT, face, ink );
}

/* The callbacks draw from these, so they are set before anything is drawn. */
static void flags_show(
    size_t idx,
    flags_status_t status,
    uint32_t now_ms
)
{
    shown_idx    = idx;
    shown_status = status;
    shown_now_ms = now_ms;
}

void gauge_flags_open(
    uint16_t face,
    size_t idx,
    flags_status_t status,
    uint32_t now_ms
)
{
    flags_show( idx, status, now_ms );
    text_init( page, PAGE_COUNT, face );
}

void gauge_flags_poll(
    size_t idx,
    flags_status_t status,
    uint32_t now_ms
)
{
    flags_show( idx, status, now_ms );
    text_poll( page, PAGE_COUNT );
}
