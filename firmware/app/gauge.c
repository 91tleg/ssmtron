#include "gauge.h"

#include <string.h>

#include "ecu.h"
#include "fmt.h"
#include "ui.h"

#define FG ( 0xFFFFu ) /* white */
#define BG ( 0x0000u ) /* black */

/* The first parameter with a fresh value: the ECU only streams one at a
 * time (see ble_log.c, which does the same lookup for the same reason --
 * this is app-level glue, not protocol logic, so it is not shared). */
static bool current_value(
    const ecu_param_info_t ** info,
    int32_t * value
)
{
    bool found = false;

    for ( size_t i = 0; !found && ( i < ecu_param_count() ); i++ )
    {
        if ( ecu_get_value( i, value ) )
        {
            *info = ecu_param_info( i );
            found = true;
        }
    }
    return found;
}

/* Copies at most UI_TEXT_MAX bytes of s into buf and NUL-terminates;
 * silently truncates a name too long for its widget's rect rather than
 * overrunning it (the round panel's edge would clip it anyway). */
static void copy_capped(
    char * buf,
    const char * s
)
{
    size_t n = strlen( s );

    if ( n > UI_TEXT_MAX )
    {
        n = UI_TEXT_MAX;
    }
    memcpy( buf, s, n );
    buf[n] = '\0';
}

static void name_text( char * buf )
{
    const ecu_param_info_t * info = NULL;
    int32_t value                 = 0;

    copy_capped( buf, current_value( &info, &value ) ? info->name : "NO ECU" );
}

static void value_text( char * buf )
{
    const ecu_param_info_t * info = NULL;
    int32_t value                 = 0;

    if ( current_value( &info, &value ) )
    {
        ( void )fmt_value( buf, UI_TEXT_MAX + 1u, info, value );
    }
    else
    {
        copy_capped( buf, "--" );
    }
}

static void unit_text( char * buf )
{
    const ecu_param_info_t * info = NULL;
    int32_t value                 = 0;

    copy_capped(
        buf, current_value( &info, &value ) ? ecu_unit( info, value ) : ""
    );
}

static void link_text( char * buf )
{
    copy_capped( buf, ( ecu_link() == ECU_LINK_UP ) ? "LINKED" : "NO LINK" );
}

/* The screen: rects are kept a safe margin inside the round panel's 120
 * px radius (see the corner-distance check in the commit that added this
 * file), not tiled edge-to-edge like a square display would allow.
 * Moving, resizing or adding a field is a change to this table alone. */
static ui_widget_t screen[] = {
    { { 40, 55, 160, 7 }, 1, FG, BG, name_text, { 0 } },
    { { 32, 95, 176, 28 }, 4, FG, BG, value_text, { 0 } },
    { { 60, 135, 120, 14 }, 2, FG, BG, unit_text, { 0 } },
    { { 70, 175, 100, 7 }, 1, FG, BG, link_text, { 0 } },
};
#define SCREEN_COUNT ( sizeof( screen ) / sizeof( screen[0] ) )

hal_status_t gauge_init( const display_pins_t * pins )
{
    hal_status_t status = display_init( pins );

    if ( status == HAL_OK )
    {
        ui_init( screen, SCREEN_COUNT, BG );
    }
    return status;
}

void gauge_poll( void )
{
    ui_poll( screen, SCREEN_COUNT );
}
