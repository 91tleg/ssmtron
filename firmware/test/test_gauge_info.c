/* Test the info page on its own: what each row says, before and after the
 * ECU has identified itself, and that it fits the panel. The fakes stand in
 * for the ECU, the firmware identity and the text engine; the widgets' own
 * text callbacks are called here to see what would be drawn. */
#include <string.h>

#include "app/fw_info.h"
#include "app/gauge_info.h"
#include "lib/display/display.h"
#include "lib/ecu/ecu.h"
#include "lib/ui/text.h"
#include "test/minitest.h"

const fw_info_t fw_info = { "9.9.9-test" };

static uint32_t rom_id;
static ecu_link_t link_state = ECU_LINK_DOWN;

uint32_t ecu_rom_id( void )
{
    return rom_id;
}

ecu_link_t ecu_link( void )
{
    return link_state;
}

static text_widget_t * page;
static size_t page_count;

/* What each widget said at the moment text_init drew the first frame. */
static char first_frame[8][TEXT_MAX + 1u];

void text_init(
    text_widget_t * widgets,
    size_t count,
    uint16_t page_bg
)
{
    ( void )page_bg;
    page       = widgets;
    page_count = count;
    for ( size_t i = 0; ( i < count ) && ( i < 8u ); i++ )
    {
        widgets[i].text( first_frame[i] );
    }
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

static void test_the_page_has_a_title_three_rows_and_a_notice( void )
{
    gauge_info_set_colors( 0x1234u, 0x5678u );
    gauge_info_open( 0x1234u, "" );
    CHECK_EQ( page_count, 5u );
    CHECK( strcmp( row( 0 ), "INFO" ) == 0 );
    CHECK_EQ( page[1].bg, 0x1234u );
    CHECK_EQ( page[1].fg, 0x5678u );
}

static void test_firmware_version_row( void )
{
    CHECK( strcmp( row( 1 ), "FW      9.9.9-test" ) == 0 );
}

/* Until the ECU has identified itself there is no ROM ID: dashes. */
static void test_rom_id_is_dashes_before_identification( void )
{
    CHECK( strcmp( row( 2 ), "ROM ID  --.--.--" ) == 0 );
    CHECK( strcmp( row( 3 ), "ECU     NO LINK" ) == 0 );
}

/* The bytes come out in the order the ECU sent them: r0.r1.r2. */
static void test_rom_id_shows_the_three_bytes_in_order( void )
{
    rom_id     = 0x013672u; /* r0 = 0x72, r1 = 0x36, r2 = 0x01 */
    link_state = ECU_LINK_UP;
    CHECK( strcmp( row( 2 ), "ROM ID  72.36.01" ) == 0 );
    CHECK( strcmp( row( 3 ), "ECU     LINKED" ) == 0 );

    rom_id = 0x0AB3CDEFu & 0xFFFFFFu; /* upper-case hex, two digits each */
    CHECK( strcmp( row( 2 ), "ROM ID  EF.CD.B3" ) == 0 );
}

static void test_notice_line_follows_what_the_caller_says( void )
{
    gauge_info_poll( "SAVED" );
    CHECK( strcmp( row( 4 ), "SAVED" ) == 0 );
    gauge_info_poll( "" );
    CHECK( strcmp( row( 4 ), "" ) == 0 );
    gauge_info_poll( NULL ); /* treated as nothing */
    CHECK( strcmp( row( 4 ), "" ) == 0 );
}

/* The first draw shows the notice it was given, not the one an earlier visit
 * left behind. */
static void test_the_first_frame_shows_the_notice_it_was_given( void )
{
    gauge_info_poll( "OLD" );
    gauge_info_open( 0x1234u, "SAVED" );
    CHECK( strcmp( first_frame[4], "SAVED" ) == 0 );
    gauge_info_open( 0x1234u, "" );
    CHECK( strcmp( first_frame[4], "" ) == 0 );
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
    test_the_page_has_a_title_three_rows_and_a_notice();
    test_firmware_version_row();
    test_rom_id_is_dashes_before_identification();
    test_rom_id_shows_the_three_bytes_in_order();
    test_notice_line_follows_what_the_caller_says();
    test_the_first_frame_shows_the_notice_it_was_given();
    test_widgets_fit_the_panel();

    return MT_RESULT();
}
