#include "minitest.h"
#include "ssm1_cmd.h"

static void test_stop( void )
{
    uint8_t b[SSM1_CMD_LEN];

    CHECK_EQ( ssm1_cmd_stop( b ), 4 );
    CHECK(
        ( b[0] == 0x12 ) && ( b[1] == 0 ) && ( b[2] == 0 ) && ( b[3] == 0 )
    );
}

static void test_read( void )
{
    uint8_t b[SSM1_CMD_LEN];

    CHECK_EQ( ssm1_cmd_read_ecu( 0x1234, b ), 4 );
    CHECK(
        ( b[0] == 0x78 ) && ( b[1] == 0x12 ) && ( b[2] == 0x34 )
        && ( b[3] == 0 )
    );

    ssm1_cmd_read_ecu( 0x00FF, b );
    CHECK( ( b[1] == 0x00 ) && ( b[2] == 0xFF ) );
    ssm1_cmd_read_ecu( 0xFF00, b );
    CHECK( ( b[1] == 0xFF ) && ( b[2] == 0x00 ) );
}

static void test_romid_and_write( void )
{
    uint8_t b[SSM1_CMD_LEN];

    CHECK_EQ( ssm1_cmd_romid( b ), 4 );
    CHECK(
        ( b[0] == 0x00 ) && ( b[1] == 0xFF ) && ( b[2] == 0xFF )
        && ( b[3] == 0x00 )
    );
    CHECK_EQ( ssm1_cmd_write( 0x135E, 0xA5, b ), 4 );
    CHECK(
        ( b[0] == 0xAA ) && ( b[1] == 0x13 ) && ( b[2] == 0x5E )
        && ( b[3] == 0xA5 )
    );
}

int main( void )
{
    test_stop();
    test_read();
    test_romid_and_write();
    return MT_RESULT();
}
