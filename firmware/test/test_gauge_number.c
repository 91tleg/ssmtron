/* Test the number page on its own: what the name, value and unit rows say
 * with and without a value or a notice, and that it fits the panel. The fake
 * text engine only records the widgets; their own text callbacks are called
 * here to see what would be drawn. */
#include <string.h>

#include "app/gauge_number.h"
#include "lib/display/display.h"
#include "lib/ecu/ecu.h"
#include "lib/ui/text.h"
#include "test/minitest.h"

static text_widget_t * page;
static size_t page_count;

void text_init(
    text_widget_t * widgets,
    size_t count,
    uint16_t page_bg
)
{
    ( void )page_bg;
    page       = widgets;
    page_count = count;
}

void text_poll(
    text_widget_t * widgets,
    size_t count
)
{
    ( void )widgets;
    ( void )count;
}

/* What widget i would draw right now. */
static const char * row( size_t i )
{
    static char buf[TEXT_MAX + 1u];

    page[i].text( buf );
    return buf;
}

/* Engine load as whole percent, with a negative-side unit for the test. */
static const ecu_param_info_t load = {
    .name      = "Load",
    .unit      = "%",
    .unit_neg  = "neg",
    .decode    = ECU_DEC_LINEAR,
    .scale_num = 1,
    .scale_den = 1,
    .offset    = 0,
    .decimals  = 0
};

static void test_the_page_has_a_name_a_value_and_a_unit( void )
{
    gauge_number_set_colors( 0x1234u, 0x5678u );
    gauge_number_open( 0x1234u );
    CHECK_EQ( page_count, 3u );
    CHECK_EQ( page[0].bg, 0x1234u );
    CHECK_EQ( page[2].fg, 0x5678u );
}

static void test_a_value_shows_with_its_unit( void )
{
    gauge_number_poll( &load, true, 42, "" );
    CHECK( strcmp( row( 0 ), "Load" ) == 0 );
    CHECK( strcmp( row( 1 ), "42" ) == 0 );
    CHECK( strcmp( row( 2 ), "%" ) == 0 );
}

/* The unit follows the sign, as the ECU interface defines it. */
static void test_the_unit_follows_the_sign( void )
{
    gauge_number_poll( &load, true, -3, "" );
    CHECK( strcmp( row( 1 ), "-3" ) == 0 );
    CHECK( strcmp( row( 2 ), "neg" ) == 0 );
}

/* Dashes and no unit stand in for a value the ECU has not sent, and the page
 * is still named. */
static void test_no_value_shows_dashes_and_the_name( void )
{
    gauge_number_poll( &load, false, 999, "" );
    CHECK( strcmp( row( 0 ), "Load" ) == 0 );
    CHECK( strcmp( row( 1 ), "--" ) == 0 );
    CHECK( strcmp( row( 2 ), "" ) == 0 );
}

static void test_no_parameter_at_all_is_blank_with_dashes( void )
{
    gauge_number_poll( NULL, true, 5, "" );
    CHECK( strcmp( row( 0 ), "" ) == 0 );
    CHECK( strcmp( row( 1 ), "--" ) == 0 );
    CHECK( strcmp( row( 2 ), "" ) == 0 );
}

static void test_a_notice_replaces_the_name_only( void )
{
    gauge_number_poll( &load, true, 42, "SAVED" );
    CHECK( strcmp( row( 0 ), "SAVED" ) == 0 );
    CHECK( strcmp( row( 1 ), "42" ) == 0 );
    gauge_number_poll( &load, true, 42, NULL ); /* treated as nothing */
    CHECK( strcmp( row( 0 ), "Load" ) == 0 );
}

/* Every widget must lie inside the panel, at whatever size it is built for. */
static void test_widgets_fit_the_panel( void )
{
    for ( size_t i = 0; i < page_count; i++ )
    {
        CHECK( ( uint32_t )page[i].rect.x + page[i].rect.w <= DISPLAY_WIDTH );
        CHECK( ( uint32_t )page[i].rect.y + page[i].rect.h <= DISPLAY_HEIGHT );
        CHECK( page[i].rect.w > 0u );
    }
}

int main( void )
{
    test_the_page_has_a_name_a_value_and_a_unit();
    test_a_value_shows_with_its_unit();
    test_the_unit_follows_the_sign();
    test_no_value_shows_dashes_and_the_name();
    test_no_parameter_at_all_is_blank_with_dashes();
    test_a_notice_replaces_the_name_only();
    test_widgets_fit_the_panel();

    return MT_RESULT();
}
