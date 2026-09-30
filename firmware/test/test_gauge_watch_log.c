/* Check that gauge_init logs only the button whose watch setup fails. */
#include <string.h>

#include "hal/hal_random.h"
#include "hal/hal_dfu.h"
#include "hal/hal_ble.h"
#include "app/fw_info.h"
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

/* The page that shows the firmware version needs one. */
const fw_info_t fw_info = { "9.9.9-test" };

/* Bluetooth, the random source and the restart that ends update mode. */
static unsigned ble_opens;
static uint32_t ble_opened_passkey;
static hal_status_t ble_open_result = HAL_OK;
static hal_ble_evt_t ble_events[4];
static size_t ble_event_count;
static uint32_t random_value = 123456u;
static unsigned reboots;

hal_status_t hal_ble_open( const hal_ble_cfg_t * cfg )
{
    ble_opens++;
    ble_opened_passkey = cfg->passkey;
    return ble_open_result;
}

hal_ble_evt_t hal_ble_poll_event( void )
{
    hal_ble_evt_t evt = HAL_BLE_EVT_NONE;

    if ( ble_event_count > 0u )
    {
        evt = ble_events[0];
        for ( size_t i = 1; i < ble_event_count; i++ )
        {
            ble_events[i - 1u] = ble_events[i];
        }
        ble_event_count--;
    }
    return evt;
}

hal_status_t hal_random_u32( uint32_t * out )
{
    *out = random_value;
    return HAL_OK;
}

void hal_dfu_reboot( void )
{
    reboots++;
}

/* 0 until the ECU has identified itself, as the real one reports. */
static uint32_t fake_rom_id;
static uint16_t assumed_with = 0xFFFFu; /* the key the gauge assumed */

uint16_t ecu_assume_profile( uint16_t profile_key )
{
    assumed_with = profile_key;
    return profile_key;
}

uint32_t ecu_rom_id( void )
{
    return fake_rom_id;
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

hal_status_t display_on( void )
{
    return HAL_OK;
}

/* A full draw clears the panel to a color first. */
static unsigned text_inits;
static size_t text_init_widgets;

void text_init(
    text_widget_t * widgets,
    size_t count,
    uint16_t page_bg
)
{
    ( void )widgets;
    ( void )page_bg;
    text_inits++;
    text_init_widgets = count;
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

/* Whether the needle has reached the value it was last given. */
static bool fake_dial_settled;

bool dial_settled( const dial_widget_t * d )
{
    ( void )d;
    return fake_dial_settled;
}

int main( void )
{
    static const display_pins_t pins = { 0 };

    ( void )gauge_init( &pins, NEXT_ID, ACTION_ID );

    CHECK( logged_next_failed );
    CHECK( !logged_action_failed );
    CHECK_EQ( log_count, 1u );

    /* With no ECU pages yet the first frame is the number screen. It must
     * be drawn in full, which clears the panel, before the panel turns on;
     * otherwise its widgets land on whatever the panel last showed. */
    CHECK_EQ( text_inits, 1u );
    CHECK_EQ( text_init_widgets, 3u );

    return MT_RESULT();
}
