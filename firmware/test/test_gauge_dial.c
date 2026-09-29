/* Check switching between the boost/rpm dials and the digital screen. */
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

/* idx 0: boost/vacuum (dial). idx 1: rpm (dial). idx 2: an ordinary
 * linear parameter (digital only). */
#define BOOST_IDX 0u
#define RPM_IDX   1u
#define LOAD_IDX  2u

static size_t selected         = BOOST_IDX;
static int32_t boost_raw_value = 15000; /* milli-psi: 15.000 psi */
static int32_t rpm_raw_value   = 3000;
static bool value_available    = true;

size_t ecu_param_count( void )
{
    return 3u;
}

const ecu_param_info_t * ecu_param_info( size_t idx )
{
    static const ecu_param_info_t boost_info = {
        "Boost/Vacuum",
        "psi",
        "inHg",
        ECU_DEC_BOOST,
        1,
        1,
        0,
        3,
        NULL,
        NULL,
        0,
        false,
        0,
        0
    };
    static const ecu_param_info_t rpm_info = {
        "Engine Speed",
        "rpm",
        NULL,
        ECU_DEC_LINEAR,
        1,
        1,
        0,
        0,
        NULL,
        NULL,
        0,
        false,
        0,
        0
    };
    static const ecu_param_info_t load_info = {
        "Load", "", NULL, ECU_DEC_LINEAR, 1, 1, 0, 0, NULL, NULL, 0, false, 0, 0
    };
    const ecu_param_info_t * info = NULL;

    if ( idx == BOOST_IDX )
    {
        info = &boost_info;
    }
    else if ( idx == RPM_IDX )
    {
        info = &rpm_info;
    }
    else if ( idx == LOAD_IDX )
    {
        info = &load_info;
    }
    else
    {
        /* out of range: NULL */
    }
    return info;
}

bool ecu_select_param( size_t idx )
{
    selected = idx;
    return true;
}

bool ecu_get_value(
    size_t idx,
    int32_t * out
)
{
    bool found = value_available && ( idx == selected );

    if ( found )
    {
        *out = ( idx == BOOST_IDX ) ? boost_raw_value : rpm_raw_value;
    }
    return found;
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

void hal_log_write( const char * text )
{
    ( void )text;
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
    return HAL_ERR_INVALID; /* selected starts at BOOST_IDX, as set above */
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

hal_status_t hal_gpio_watch( hal_gpio_id_t id )
{
    ( void )id;
    return HAL_OK;
}

typedef struct
{
    bool level;
    bool pending;
    uint32_t timestamp_ms;
} fake_edge_t;

static fake_edge_t next_edge;
static fake_edge_t action_edge;

bool hal_gpio_take_edge(
    hal_gpio_id_t id,
    bool * level,
    uint32_t * timestamp_ms
)
{
    fake_edge_t * edge = ( id == ACTION_ID ) ? &action_edge : &next_edge;
    bool had_edge      = edge->pending;

    if ( had_edge )
    {
        *level        = edge->level;
        *timestamp_ms = edge->timestamp_ms;
        edge->pending = false;
    }
    return had_edge;
}

hal_status_t display_init( const display_pins_t * pins )
{
    ( void )pins;
    return HAL_OK;
}

static unsigned text_init_count;
static unsigned dial_init_count;
static bool last_dial_has_value;
static int32_t last_dial_value;

void text_init(
    text_widget_t * widgets,
    size_t count,
    uint16_t page_bg
)
{
    ( void )widgets;
    ( void )count;
    ( void )page_bg;
    text_init_count++;
}

void text_poll(
    text_widget_t * widgets,
    size_t count
)
{
    ( void )widgets;
    ( void )count;
}

static const dial_cfg_t * last_dial_cfg;

void dial_init(
    dial_widget_t * d,
    const dial_cfg_t * cfg,
    uint32_t now_ms
)
{
    ( void )d;
    ( void )now_ms;
    dial_init_count++;
    last_dial_cfg = cfg;
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
    last_dial_has_value = has_value;
    last_dial_value     = value;
}

static uint32_t now;

static void tick( uint32_t delta_ms )
{
    now += delta_ms;
    gauge_poll( now );
}

static void press_next( bool level )
{
    next_edge.level        = level;
    next_edge.timestamp_ms = now;
    next_edge.pending      = true;
}

/* One debounced tap: the selection advances when the button is released. */
static void tap_next( void )
{
    press_next( true );
    tick( 1 );
    tick( 35 );
    press_next( false );
    tick( 1 );
    tick( 35 );
}

static void press_action( bool level )
{
    action_edge.level        = level;
    action_edge.timestamp_ms = now;
    action_edge.pending      = true;
}

int main( void )
{
    static const display_pins_t pins = { 0 };
    const dial_cfg_t * boost_cfg;
    const dial_cfg_t * rpm_cfg;

    ( void )gauge_init( &pins, NEXT_ID, ACTION_ID );

    /* Initialization starts on the digital screen. */
    CHECK_EQ( text_init_count, 1u );

    /* Boost selection enters the dial on the first poll. */
    tick( 1 );
    CHECK_EQ( dial_init_count, 1u );
    CHECK_EQ( text_init_count, 1u );
    CHECK( last_dial_has_value );
    CHECK_EQ( last_dial_value, 15 ); /* 15000 milli-psi -> 15 psi */
    boost_cfg = last_dial_cfg;

    /* Staying on the same parameter must not re-enter the dial. */
    tick( 1 );
    CHECK_EQ( dial_init_count, 1u );

    /* NEXT selects rpm: also a dial, but a different one. */
    tap_next(); /* idx 0 -> 1 */
    CHECK_EQ( selected, RPM_IDX );
    CHECK_EQ( text_init_count, 1u ); /* still no digital screen */
    CHECK_EQ( dial_init_count, 2u );
    CHECK_EQ( last_dial_value, 3 ); /* thousands of rpm */
    rpm_cfg = last_dial_cfg;
    CHECK( rpm_cfg != boost_cfg );

    /* NEXT selects load: not a dial, back to digital. */
    tap_next();
    CHECK_EQ( selected, LOAD_IDX );
    CHECK_EQ( text_init_count, 2u );
    CHECK_EQ( dial_init_count, 2u );

    /* NEXT wraps back to the boost dial. */
    tap_next();
    CHECK_EQ( selected, BOOST_IDX );
    CHECK_EQ( dial_init_count, 3u );
    CHECK( last_dial_cfg == boost_cfg );

    /* Clear status uses the digital screen, even on a dial param. */
    press_action( true );
    tick( 1 );
    tick( 35 );   /* HOLDING */
    tick( 1965 ); /* crosses the 2 s hold: clear starts */
    CHECK_EQ( text_init_count, 3u );

    return MT_RESULT();
}
