/* Test the flags page: every flag name is printed with its state, and the
 * page gives way to the clear screen and back. The fakes stand in for the
 * ECU, HAL and display; the widgets' own text callbacks are called here to
 * see what would be drawn. */
#include <string.h>

#include "app/gauge.h"
#include "hal/hal_gpio.h"
#include "hal/hal_log.h"
#include "hal/hal_nvm.h"
#include "lib/display/display.h"
#include "lib/ecu/ecu.h"
#include "lib/ui/dial.h"
#include "lib/ui/text.h"
#include "test/minitest.h"

#define NEXT_ID   ( ( hal_gpio_id_t )1u )
#define ACTION_ID ( ( hal_gpio_id_t )2u )

/* An active-high bit and an active-low bit, so both polarities are seen. */
static const ecu_flag_t test_flags[] = {
    { "22 Knock", 0x02u, true },
    { "Test Mode", 0x20u, false },
};

static uint8_t raw_byte;
static bool value_fresh;

size_t ecu_param_count( void )
{
    return 1u;
}

const ecu_param_info_t * ecu_param_info( size_t idx )
{
    static const ecu_param_info_t info = {
        "Stored Codes 2/3",
        "",
        NULL,
        ECU_DEC_FLAGS,
        1,
        1,
        0,
        0,
        NULL,
        test_flags,
        2,
        false,
        0,
        0
    };

    return ( idx == 0u ) ? &info : NULL;
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
    if ( ( idx == 0u ) && value_fresh )
    {
        *out = ( int32_t )raw_byte;
    }
    return ( idx == 0u ) && value_fresh;
}

bool ecu_clear_start( void )
{
    return true;
}

static ecu_clear_t clear_state = ECU_CLEAR_BUSY;

ecu_clear_t ecu_clear_state( void )
{
    return clear_state;
}

ecu_link_t ecu_link( void )
{
    return ECU_LINK_DOWN;
}

void hal_log_write( const char * text )
{
    ( void )text;
}

static unsigned theme_saves;
static uint32_t saved_theme_value;

hal_status_t hal_nvm_save(
    const char * key,
    const void * data,
    size_t len
)
{
    if ( ( strcmp( key, "theme" ) == 0 ) && ( len == sizeof( uint32_t ) ) )
    {
        theme_saves++;
        ( void )memcpy( &saved_theme_value, data, len );
    }
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

hal_status_t hal_gpio_watch( hal_gpio_id_t id )
{
    ( void )id;
    return HAL_OK;
}

static uint32_t now;
static bool action_level;
static bool action_pending;
static uint32_t action_at;
static bool next_level;
static bool next_pending;
static uint32_t next_at;

bool hal_gpio_take_edge(
    hal_gpio_id_t id,
    bool * level,
    uint32_t * timestamp_ms
)
{
    bool is_action = ( id == ACTION_ID );
    bool had       = is_action ? action_pending : next_pending;

    if ( had )
    {
        *level         = is_action ? action_level : next_level;
        *timestamp_ms  = is_action ? action_at : next_at;
        action_pending = is_action ? false : action_pending;
        next_pending   = is_action ? next_pending : false;
    }
    return had;
}

hal_status_t display_init( const display_pins_t * pins )
{
    ( void )pins;
    return HAL_OK;
}

/* The page currently on screen: the last widget set text_init was given. */
static uint16_t last_page_bg;
static text_widget_t * page;
static size_t page_count;

void text_init(
    text_widget_t * widgets,
    size_t count,
    uint16_t page_bg
)
{
    last_page_bg = page_bg;
    page         = widgets;
    page_count   = count;
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

static void tick( uint32_t delta_ms )
{
    now += delta_ms;
    gauge_poll( now );
}

static void press_action( bool level )
{
    action_level   = level;
    action_at      = now;
    action_pending = true;
}

static void press_next( bool level )
{
    next_level   = level;
    next_at      = now;
    next_pending = true;
}

/* What widget i would draw right now. */
static const char * row( size_t i )
{
    static char buf[TEXT_MAX + 1u];

    page[i].text( buf );
    return buf;
}

/* Every widget of the page on screen must lie inside the panel. A rect
 * that hangs off an edge wraps when it is stored as an unsigned position
 * and then draws nothing. */
static void check_widgets_fit_the_panel( void )
{
    for ( size_t i = 0; i < page_count; i++ )
    {
        CHECK( ( uint32_t )page[i].rect.x + page[i].rect.w <= DISPLAY_WIDTH );
        CHECK( ( uint32_t )page[i].rect.y + page[i].rect.h <= DISPLAY_HEIGHT );
        CHECK( page[i].rect.w > 0u );
    }
}

static void test_number_screen_fits_the_panel( void )
{
    /* gauge_init drew the number screen, the first page. */
    CHECK_EQ( page_count, 4u );
    check_widgets_fit_the_panel();
}

static void test_flags_page_is_shown_for_a_flags_byte( void )
{
    raw_byte    = 0x02u; /* Knock set, Test Mode bit clear */
    value_fresh = true;
    tick( 1 );
    CHECK_EQ( page_count, 10u ); /* title, eight rows and a status line */
    check_widgets_fit_the_panel();
}

static void test_title_is_centered( void )
{
    CHECK( strcmp( row( 0 ), "  Stored Codes 2/3" ) == 0 );
}

static void test_status_line_is_blank_when_idle( void )
{
    CHECK( strcmp( row( 9 ), "" ) != 0 ); /* padded blanks, no words */
    CHECK( strspn( row( 9 ), " " ) == strlen( row( 9 ) ) );
}

static void test_every_name_is_printed_with_its_state( void )
{
    /* Names pad to 11 columns, then a space, then the state. Test Mode is
     * active-low: its bit is clear, so it reads 1. */
    CHECK( strcmp( row( 1 ), "22 Knock    1" ) == 0 );
    CHECK( strcmp( row( 2 ), "Test Mode   1" ) == 0 );

    raw_byte = 0x20u; /* Knock clear, Test Mode bit set */
    CHECK( strcmp( row( 1 ), "22 Knock    0" ) == 0 );
    CHECK( strcmp( row( 2 ), "Test Mode   0" ) == 0 );
}

static void test_rows_past_the_last_flag_are_blank( void )
{
    for ( size_t i = 3u; i < 9u; i++ )
    {
        CHECK( strcmp( row( i ), "" ) == 0 );
    }
}

static void test_names_stay_when_the_value_is_stale( void )
{
    value_fresh = false;
    CHECK( strcmp( row( 1 ), "22 Knock    -" ) == 0 );
    CHECK( strcmp( row( 2 ), "Test Mode   -" ) == 0 );
    value_fresh = true;
}

static void test_clear_hold_prompts_on_the_flags_page( void )
{
    press_action( true );
    tick( 1 );
    tick( 35 ); /* debounced, but still a possible tap: no prompt yet */
    CHECK( strspn( row( 9 ), " " ) == strlen( row( 9 ) ) );
    tick( 500 );                 /* past the tap window: it is a hold */
    CHECK_EQ( page_count, 10u ); /* the page stays put */
    CHECK( strcmp( row( 9 ), "   HOLD TO CLEAR" ) == 0 );
    CHECK( strcmp( row( 1 ), "22 Knock    0" ) == 0 ); /* flags still drawn */

    press_action( false );
    tick( 1 ); /* released early: back to idle */
    CHECK( strspn( row( 9 ), " " ) == strlen( row( 9 ) ) );
}

static void test_clearing_shows_dots_that_advance( void )
{
    char first[TEXT_MAX + 1u];

    press_action( true );
    tick( 1 );
    tick( 35 );
    tick( 2000 ); /* past the tap window and the 2 s hold */
    tick( 1 );    /* the poll after: the clear starts */
    CHECK_EQ( page_count, 10u );
    CHECK( strncmp( row( 9 ), "    CLEARING", 12 ) == 0 );

    ( void )strcpy( first, row( 9 ) );
    tick( 400 ); /* one step later */
    CHECK( strcmp( row( 9 ), first ) != 0 );
    CHECK( strncmp( row( 9 ), "    CLEARING", 12 ) == 0 );
}

static void test_clear_result_is_shown_on_the_page( void )
{
    uint16_t face_before;

    clear_state = ECU_CLEAR_DONE;
    tick( 1 );
    CHECK( strcmp( row( 9 ), "      CLEARED" ) == 0 );
    CHECK_EQ( page_count, 10u );
    face_before = page[1].bg;

    /* Dismiss with a fresh press. */
    press_action( false );
    tick( 1 );
    press_action( true );
    tick( 1 );
    tick( 35 ); /* settled: this dismisses the result */
    tick( 1 );  /* the next loop pass, button still down */
    press_action( false );
    tick( 1 );
    tick( 35 );
    CHECK( strspn( row( 9 ), " " ) == strlen( row( 9 ) ) );
    /* The press that dismissed the result is not also a theme tap. */
    CHECK_EQ( page[1].bg, face_before );
}

/* A tap in the fake is a press and a release inside the tap window. */
static void tap_action( void )
{
    press_action( true );
    tick( 1 );
    tick( 35 );
    press_action( false );
    tick( 1 );
}

static void test_action_tap_switches_to_dark_and_back( void )
{
    tap_action();
    CHECK_EQ( page_count, 10u );       /* the same page, drawn again */
    CHECK_EQ( last_page_bg, 0x0000u ); /* black face */
    CHECK_EQ( page[1].bg, 0x0000u );
    CHECK_EQ( page[1].fg, 0x9CD3u );                        /* inverted ink */
    CHECK( strspn( row( 9 ), " " ) == strlen( row( 9 ) ) ); /* no prompt */

    tap_action();
    CHECK_EQ( last_page_bg, 0xEF5Bu );
    CHECK_EQ( page[1].bg, 0xEF5Bu );
    CHECK_EQ( page[1].fg, 0x632Cu );
}

static void test_theme_is_saved_only_by_a_next_hold( void )
{
    tap_action();                /* dark */
    CHECK_EQ( theme_saves, 0u ); /* a tap writes nothing */

    press_next( true );
    tick( 1 );
    tick( 35 );
    tick( 1500 ); /* held for the save */
    CHECK_EQ( theme_saves, 1u );
    CHECK_EQ( saved_theme_value, 1u );
    CHECK( strspn( row( 9 ), " " ) != strlen( row( 9 ) ) ); /* SAVED */

    press_next( false );
    tick( 1 );
    tick( 35 );
    tap_action(); /* back to light so nothing after is affected */
}

int main( void )
{
    static const display_pins_t pins = { 0 };

    ( void )gauge_init( &pins, NEXT_ID, ACTION_ID );

    test_number_screen_fits_the_panel();
    test_flags_page_is_shown_for_a_flags_byte();
    test_title_is_centered();
    test_status_line_is_blank_when_idle();
    test_every_name_is_printed_with_its_state();
    test_rows_past_the_last_flag_are_blank();
    test_names_stay_when_the_value_is_stale();
    test_clear_hold_prompts_on_the_flags_page();
    test_clearing_shows_dots_that_advance();
    test_clear_result_is_shown_on_the_page();
    test_action_tap_switches_to_dark_and_back();
    test_theme_is_saved_only_by_a_next_hold();

    return MT_RESULT();
}
