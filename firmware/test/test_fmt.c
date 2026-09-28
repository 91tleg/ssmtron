#include <string.h>

#include "fmt.h"
#include "minitest.h"

static const ecu_param_info_t rpm = {
    "Engine Speed",
    "rpm",
    NULL,
    ECU_DEC_LINEAR,
    25,
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
static const ecu_param_info_t volt = {
    "System Voltage",
    "V",
    NULL,
    ECU_DEC_LINEAR,
    8,
    1,
    0,
    2,
    NULL,
    NULL,
    0,
    false,
    0,
    0
};
static const ecu_param_info_t boost = {
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
static const ecu_param_info_t flags = {
    "Input Switches",
    "",
    NULL,
    ECU_DEC_FLAGS,
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

static bool
is( const ecu_param_info_t * info,
    int32_t v,
    const char * want )
{
    char b[32];
    size_t n = fmt_value( b, sizeof( b ), info, v );

    return ( strcmp( b, want ) == 0 ) && ( n == strlen( want ) );
}

static void test_values( void )
{
    CHECK( is( &rpm, 0, "0" ) );
    CHECK( is( &rpm, 1000, "1000" ) );
    CHECK( is( &rpm, -7, "-7" ) );
    CHECK( is( &volt, 1400, "14.00" ) );
    CHECK( is( &volt, 5, "0.05" ) ); /* leading zeros after the point */
    CHECK( is( &volt, 0, "0.00" ) );
    CHECK( is( &volt, -5, "-0.05" ) );
    CHECK( is( &volt, -1234, "-12.34" ) );
    CHECK( is( &boost, 48, "0.048" ) );
    CHECK( is( &rpm, 2147483647, "2147483647" ) );
    CHECK( is( &rpm, ( int32_t )0x80000000, "-2147483648" ) );
    CHECK( is( &flags, 0xA5, "0xA5" ) );
    CHECK( is( &flags, 0x00, "0x00" ) );
}

static void test_log_line( void )
{
    char b[64];

    CHECK_EQ( fmt_log_line( b, sizeof( b ), 12345, &rpm, 1000 ), 28 );
    CHECK( strcmp( b, "12345,Engine Speed,1000,rpm\n" ) == 0 );
    fmt_log_line( b, sizeof( b ), 0, &volt, 1400 );
    CHECK( strcmp( b, "0,System Voltage,14.00,V\n" ) == 0 );
    /* the unit follows the sign of the value */
    fmt_log_line( b, sizeof( b ), 1, &boost, -1500 );
    CHECK( strcmp( b, "1,Boost/Vacuum,-1.500,inHg\n" ) == 0 );
    fmt_log_line( b, sizeof( b ), 1, &boost, 1500 );
    CHECK( strcmp( b, "1,Boost/Vacuum,1.500,psi\n" ) == 0 );
    fmt_log_line( b, sizeof( b ), 4294967295u, &flags, 0x81 );
    CHECK( strcmp( b, "4294967295,Input Switches,0x81,\n" ) == 0 );
}

/* Every size from 0 up: never overruns, and it is all or nothing. */
static void test_never_overruns( void )
{
    const char * want = "12345,Engine Speed,1000,rpm\n";
    char guard[80];

    for ( size_t max = 0; max < 40; max++ )
    {
        size_t n;

        memset( guard, 0x5A, sizeof( guard ) );
        n = fmt_log_line( guard, max, 12345, &rpm, 1000 );
        if ( max > strlen( want ) )
        {
            CHECK_EQ( n, strlen( want ) );
            CHECK( strcmp( guard, want ) == 0 );
        }
        else
        {
            CHECK_EQ( n, 0 );
            if ( max > 0 )
            {
                CHECK( guard[0] == '\0' );
            }
        }
        for ( size_t i = max; i < sizeof( guard ); i++ )
        {
            CHECK( ( unsigned char )guard[i] == 0x5A );
        }
    }
}

static void test_odd_inputs( void )
{
    char b[16];
    ecu_param_info_t noname = rpm;

    noname.name = NULL;
    noname.unit = NULL;
    CHECK( fmt_log_line( b, sizeof( b ), 1, &noname, 5 ) > 0 );
    CHECK( strcmp( b, "1,,5,\n" ) == 0 );
    CHECK_EQ( fmt_value( b, 0, &rpm, 1 ), 0 ); /* no room, not even a NUL */
}

int main( void )
{
    test_values();
    test_log_line();
    test_never_overruns();
    test_odd_inputs();
    return MT_RESULT();
}
