#include "app/gauge_update.h"

#include <stddef.h>
#include <string.h>

#include "hal/hal_ble.h"
#include "hal/hal_dfu.h"
#include "hal/hal_random.h"
#include "lib/textfit/textfit.h"
#include "lib/ui/text.h"
#include "lib/ui/ui_geom.h"

#define UPDATE_BLE_NAME "Maxtek Datum"

/* How long to wait for a phone before giving up. */
#define UPDATE_WAIT_MS ( 300000u )

/* The passkey is six digits: 000000 to 999999. */
#define UPDATE_PASSKEY_LIMIT  ( 1000000u )
#define UPDATE_PASSKEY_DIGITS ( 6u )

/* Each row is a label, padded so the values line up, then its value. A row
 * holds at most 20 glyphs. */
#define UPDATE_LABEL_W ( 10u )
#define UPDATE_ROW_W   ( 20u )

typedef enum
{
    UPDATE_OFF,
    UPDATE_WAITING,   /* advertising, no phone yet */
    UPDATE_CONNECTED, /* a phone is connected */
    UPDATE_FAILED     /* Bluetooth would not start */
} update_state_t;

static update_state_t update_state = UPDATE_OFF;
static uint32_t update_passkey;
static uint32_t update_wait_start; /* when the countdown last started */

/* What the callbacks read. They take no context, so gauge_update_poll sets
 * the time before every redraw. */
static uint32_t update_now_ms;

bool gauge_update_active( void )
{
    return update_state != UPDATE_OFF;
}

void gauge_update_start( uint32_t now_ms )
{
    if ( update_state == UPDATE_OFF )
    {
        hal_ble_cfg_t cfg;
        uint32_t random_value = 0u;

        update_wait_start = now_ms;
        update_state      = UPDATE_FAILED;
        /* Without a secure random passkey there is no pairing to offer, so
         * Bluetooth is not started at all. */
        if ( hal_random_u32( &random_value ) == HAL_OK )
        {
            update_passkey = random_value % UPDATE_PASSKEY_LIMIT;
            cfg.name       = UPDATE_BLE_NAME;
            cfg.passkey    = update_passkey;
            if ( hal_ble_open( &cfg ) == HAL_OK )
            {
                update_state = UPDATE_WAITING;
            }
        }
    }
}

void gauge_update_exit( void )
{
    hal_dfu_reboot();
    update_state = UPDATE_OFF; /* only reached where a restart is faked */
}

/* The page's rows, cut to UPDATE_ROW_W glyphs. */
static void update_put_row(
    char * buf,
    const char * label,
    const char * value
)
{
    ( void )textfit_row(
        buf, UPDATE_ROW_W + 1u, UPDATE_LABEL_W, UPDATE_ROW_W, label, value
    );
}

/* Text cut to UPDATE_ROW_W glyphs. Its widget centers it. */
static void update_put_text(
    char * buf,
    const char * s
)
{
    ( void )textfit_copy( buf, UPDATE_ROW_W + 1u, s );
}

static void update_title_text( char * buf )
{
    update_put_text( buf, "UPDATE MODE" );
}

/* The passkey with its leading zeros, or dashes when Bluetooth is not up. */
static void update_passkey_text( char * buf )
{
    char digits[UPDATE_PASSKEY_DIGITS + 1u];
    uint32_t rest = update_passkey;

    if ( update_state == UPDATE_FAILED )
    {
        ( void )memcpy( digits, "------", sizeof( digits ) );
    }
    else
    {
        for ( size_t i = UPDATE_PASSKEY_DIGITS; i > 0u; i-- )
        {
            digits[i - 1u] = ( char )( '0' + ( rest % 10u ) );
            rest /= 10u;
        }
        digits[UPDATE_PASSKEY_DIGITS] = '\0';
    }
    update_put_row( buf, "PASSKEY", digits );
}

static void update_status_text( char * buf )
{
    const char * text = "";

    switch ( update_state )
    {
        case UPDATE_WAITING:
            text = "WAITING FOR PHONE";
            break;

        case UPDATE_CONNECTED:
            text = "PHONE CONNECTED";
            break;

        case UPDATE_FAILED:
            text = "BLUETOOTH FAILED";
            break;

        case UPDATE_OFF:
        default:
            break;
    }
    update_put_text( buf, text );
}

/* The wait that is left, as m:ss. Nothing while a phone is connected, since
 * the wait is paused then. */
static void update_time_text( char * buf )
{
    char clock[8];

    if ( update_state == UPDATE_CONNECTED )
    {
        clock[0] = '\0';
    }
    else
    {
        uint32_t used_ms = update_now_ms - update_wait_start;
        uint32_t left_s  = 0u;

        if ( used_ms < UPDATE_WAIT_MS )
        {
            left_s = ( UPDATE_WAIT_MS - used_ms + 999u ) / 1000u;
        }
        clock[0] = ( char )( '0' + ( left_s / 60u ) );
        clock[1] = ':';
        clock[2] = ( char )( '0' + ( ( left_s % 60u ) / 10u ) );
        clock[3] = ( char )( '0' + ( left_s % 10u ) );
        clock[4] = '\0';
    }
    update_put_row( buf, "TIME LEFT", clock );
}

static void update_hint_text( char * buf )
{
    update_put_text( buf, "NEXT TO EXIT" );
}

/* Text sizes, chosen for the 240 px panel. */
#define UPDATE_SCALE UI_TEXT_SCALE( 1 )

/* Rows sit as a block centered on the panel, 134 thousandths of the radius
 * apart, like the info page. The hint is below them. */
#define UPDATE_ROW_Y( n ) ( UI_CY - UI_LEN( 200 ) + ( ( n ) * UI_LEN( 134 ) ) )

static text_widget_t update_page[] = {
    UI_CENTERED_WIDGET(
        20,
        UPDATE_SCALE,
        UI_CY - UI_LEN( 800 ),
        update_title_text,
        TEXT_ALIGN_CENTER
    ),
    UI_CENTERED_WIDGET(
        20,
        UPDATE_SCALE,
        UPDATE_ROW_Y( 0 ),
        update_passkey_text,
        TEXT_ALIGN_LEFT
    ),
    UI_CENTERED_WIDGET(
        20, UPDATE_SCALE, UPDATE_ROW_Y( 1 ), update_status_text, TEXT_ALIGN_LEFT
    ),
    UI_CENTERED_WIDGET(
        20, UPDATE_SCALE, UPDATE_ROW_Y( 2 ), update_time_text, TEXT_ALIGN_LEFT
    ),
    UI_CENTERED_WIDGET(
        20,
        UPDATE_SCALE,
        UI_CY + UI_LEN( 550 ),
        update_hint_text,
        TEXT_ALIGN_CENTER
    ),
};
#define UPDATE_PAGE_COUNT ( sizeof( update_page ) / sizeof( update_page[0] ) )

void gauge_update_set_colors(
    uint16_t face,
    uint16_t ink
)
{
    text_set_colors( update_page, UPDATE_PAGE_COUNT, face, ink );
}

void gauge_update_open(
    uint16_t face,
    uint32_t now_ms
)
{
    update_now_ms = now_ms;
    text_init( update_page, UPDATE_PAGE_COUNT, face );
}

void gauge_update_poll( uint32_t now_ms )
{
    update_now_ms = now_ms;
    if ( update_state != UPDATE_OFF )
    {
        hal_ble_evt_t evt = hal_ble_poll_event();

        for ( ; evt != HAL_BLE_EVT_NONE; evt = hal_ble_poll_event() )
        {
            if ( ( evt == HAL_BLE_EVT_CONNECTED )
                 && ( update_state != UPDATE_FAILED ) )
            {
                update_state = UPDATE_CONNECTED;
            }
            else if ( ( evt == HAL_BLE_EVT_DISCONNECTED )
                      && ( update_state == UPDATE_CONNECTED ) )
            {
                /* The phone left: a full wait starts again. */
                update_state      = UPDATE_WAITING;
                update_wait_start = now_ms;
            }
            else
            {
                /* Nothing to do for this event. */
            }
        }
        if ( ( update_state != UPDATE_CONNECTED )
             && ( ( now_ms - update_wait_start ) >= UPDATE_WAIT_MS ) )
        {
            gauge_update_exit();
        }
    }
    text_poll( update_page, UPDATE_PAGE_COUNT );
}
