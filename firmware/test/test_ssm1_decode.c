#include "minitest.h"
#include "ssm1_decode.h"
#include "ssm1_params.h"

static const ecu_param_info_t * find(
    const ssm1_profile_t * pr,
    const char * name
)
{
    const ecu_param_info_t * r = NULL;

    for ( size_t i = 0; i < pr->count; i++ )
    {
        const char * a = pr->params[i].info->name;
        const char * b = name;

        while ( ( *a != '\0' ) && ( *a == *b ) )
        {
            a++;
            b++;
        }
        if ( ( *a == '\0' ) && ( *b == '\0' ) )
        {
            r = pr->params[i].info;
        }
    }
    return r;
}

static void test_linear( void )
{
    const ssm1_profile_t * t = ssm1_profile_find( 0x3672 );

    CHECK_EQ( ssm1_decode( find( t, "Engine Speed" ), 40 ), 1000 );
    CHECK_EQ(
        ssm1_decode( find( t, "System Voltage" ), 175 ), 1400
    ); /* 14.00 V */
    CHECK_EQ(
        ssm1_decode( find( t, "Inj Pulse Width" ), 10 ), 1280
    ); /* 1.280 ms */
    CHECK_EQ( ssm1_decode( find( t, "Barometric Pressure" ), 0 ), 50000 );
}

static void test_signed( void )
{
    const ecu_param_info_t * i =
        find( ssm1_profile_find( 0x3672 ), "Timing Correction" );

    CHECK_EQ( ssm1_decode( i, 0 ), 0 );
    CHECK_EQ( ssm1_decode( i, 5 ), 5 );
    CHECK_EQ( ssm1_decode( i, 127 ), 127 );
    CHECK_EQ( ssm1_decode( i, 128 ), -128 );
    CHECK_EQ( ssm1_decode( i, 255 ), -1 );
}

static void test_fuel_trim( void )
{
    const ecu_param_info_t * i =
        find( ssm1_profile_find( 0x3272 ), "Fuel Trim" );

    CHECK_EQ( ssm1_decode( i, 128 ), 0 );
    CHECK_EQ( ssm1_decode( i, 0 ), -10000 ); /* -100.00 % */
    CHECK( ssm1_decode( i, 192 ) == 5000 );
}

static void test_jecs_coolant( void )
{
    const ecu_param_info_t * i =
        find( ssm1_profile_find( 0x3272 ), "Coolant Temp" );

    CHECK_EQ( ssm1_decode( i, 0 ), -580 ); /* -58.0 F */
    CHECK_EQ( ssm1_decode( i, 100 ), 1220 );
}

static void test_hitachi_coolant_table( void )
{
    const ecu_param_info_t * i =
        find( ssm1_profile_find( 0x3672 ), "Coolant Temp" );

    CHECK_EQ( ssm1_decode( i, 0 ), 401 ); /* first 14 entries + 255 */
    CHECK_EQ( ssm1_decode( i, 13 ), 257 );
    CHECK_EQ( ssm1_decode( i, 14 ), 250 );
    CHECK_EQ( ssm1_decode( i, 226 ), 0 );  /* last non-negative entry */
    CHECK_EQ( ssm1_decode( i, 227 ), -2 ); /* the last 29 are negative */
    CHECK_EQ( ssm1_decode( i, 255 ), -47 );
    /* colder raw -> ... never wildly out of order around the sign flip */
    CHECK( ssm1_decode( i, 225 ) > 0 );
}

static void test_boost( void )
{
    const ecu_param_info_t * i =
        find( ssm1_profile_find( 0x3672 ), "Boost/Vacuum" );

    CHECK_EQ( ssm1_decode( i, 136 ), 48 ); /* just above atmosphere */
    CHECK( ssm1_decode( i, 135 ) < 0 );    /* just below */
    CHECK_EQ(
        ssm1_decode( i, 255 ), ( ( 25500 - 13568 ) * 15107 ) / 10000
    );
    CHECK_EQ( ssm1_decode( i, 0 ), -( ( 13568 * 30757 ) / 10000 ) );
    CHECK( ecu_unit( i, 1 ) == i->unit );
    CHECK( ecu_unit( i, -1 ) == i->unit_neg );
    CHECK(
        ecu_unit( find( ssm1_profile_find( 0x3672 ), "Engine Speed" ), -1 )
        != NULL
    );
}

static void test_flags( void )
{
    const ecu_param_info_t * i =
        find( ssm1_profile_find( 0x3272 ), "Input Switches" );

    CHECK_EQ( ssm1_decode( i, 0xA5 ), 0xA5 );
    CHECK_EQ( i->flag_count, 7 );
    /* "California" is inverted: set when the bit is clear */
    CHECK( ecu_flag_set( &i->flags[6], 0x00 ) );
    CHECK( !ecu_flag_set( &i->flags[6], 0x01 ) );
    CHECK( ecu_flag_set( &i->flags[0], 0x80 ) );
    CHECK( !ecu_flag_set( &i->flags[0], 0x7F ) );
}

int main( void )
{
    test_linear();
    test_signed();
    test_fuel_trim();
    test_jecs_coolant();
    test_hitachi_coolant_table();
    test_boost();
    test_flags();
    return MT_RESULT();
}
