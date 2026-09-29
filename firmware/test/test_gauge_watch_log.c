/* Check that gauge_init logs only the button whose watch setup fails. */
#include <string.h>

#include "dial.h"
#include "ecu.h"
#include "gauge.h"
#include "hal_gpio.h"
#include "hal_log.h"
#include "hal_nvm.h"
#include "minitest.h"
#include "text.h"

#define NEXT_ID   ( ( hal_gpio_id_t )1u )
#define ACTION_ID ( ( hal_gpio_id_t )2u )

size_t ecu_param_count( void )
{
    return 0u;
}

const ecu_param_info_t * ecu_param_info( size_t idx )
{
    ( void )idx;
    return NULL;
}

bool ecu_select_param( size_t idx )
{
    ( void )idx;
    return true;
}

bool ecu_get_value(
    size_t idx,
    int32_t * out
)
{
    ( void )idx;
    ( void )out;
    return false;
}

bool ecu_clear_start( void )
{
    return true;
}

ecu_clear_t ecu_clear_state( void )
{
    return ECU_CLEAR_BUSY;
}

ecu_link_t ecu_link( void )
{
    return ECU_LINK_DOWN;
}

static bool logged_next_failed;
static bool logged_action_failed;
static unsigned log_count;

void hal_log_write( const char * text )
{
    log_count++;
    if ( strcmp( text, "next btn watch failed" ) == 0 )
    {
        logged_next_failed = true;
    }
    else if ( strcmp( text, "action btn watch failed" ) == 0 )
    {
        logged_action_failed = true;
    }
}

hal_status_t hal_nvm_save(
    const char * key,
    const void * data,
    size_t len
)
{
    ( void )key;
    ( void )data;
    ( void )len;
    return HAL_OK;
}

hal_status_t hal_nvm_load(
    const char * key,
    void * data,
    size_t max,
    size_t * len
)
{
    ( void )key;
    ( void )data;
    ( void )max;
    ( void )len;
    return HAL_ERR_INVALID;
}

hal_status_t hal_gpio_open(
    hal_gpio_id_t id,
    hal_gpio_mode_t mode
)
{
    ( void )id;
    ( void )mode;
    return HAL_OK;
}

/* NEXT's interrupt line is unavailable; ACTION's is fine. */
hal_status_t hal_gpio_watch( hal_gpio_id_t id )
{
    return ( id == NEXT_ID ) ? HAL_ERR_IO : HAL_OK;
}

bool hal_gpio_take_edge(
    hal_gpio_id_t id,
    bool * level,
    uint32_t * timestamp_ms
)
{
    ( void )id;
    ( void )level;
    ( void )timestamp_ms;
    return false; /* never fed an edge: not what this test checks */
}

hal_status_t display_init( const display_pins_t * pins )
{
    ( void )pins;
    return HAL_OK;
}

void text_init(
    text_widget_t * widgets,
    size_t count,
    uint16_t page_bg
)
{
    ( void )widgets;
    ( void )count;
    ( void )page_bg;
}

void text_poll(
    text_widget_t * widgets,
    size_t count
)
{
    ( void )widgets;
    ( void )count;
}

void dial_init(
    dial_widget_t * d,
    const dial_cfg_t * cfg,
    uint32_t now_ms
)
{
    ( void )d;
    ( void )cfg;
    ( void )now_ms;
}

void dial_poll(
    dial_widget_t * d,
    uint32_t now_ms,
    bool has_value,
    int32_t value
)
{
    ( void )d;
    ( void )now_ms;
    ( void )has_value;
    ( void )value;
}

int main( void )
{
    static const display_pins_t pins = { 0 };

    ( void )gauge_init( &pins, NEXT_ID, ACTION_ID );

    CHECK( logged_next_failed );
    CHECK( !logged_action_failed );
    CHECK_EQ( log_count, 1u );

    return MT_RESULT();
}
