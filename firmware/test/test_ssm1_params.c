/* Sanity of the profile tables. */
#include <string.h>

#include "minitest.h"
#include "ssm1_params.h"

static void test_six_profiles_found_by_rom_id( void )
{
    const uint16_t ids[] = { 0x3671, 0x3672, 0x3170, 0x3270, 0x3272, 0x3273 };

    CHECK_EQ( ssm1_profile_count(), 6 );
    for ( size_t i = 0; i < 6; i++ )
    {
        const ssm1_profile_t * p = ssm1_profile_find( ids[i] );

        CHECK( p != NULL );
        CHECK( p->rom_id == ids[i] );
    }
    CHECK( ssm1_profile_find( 0x0000 ) == NULL );
    CHECK( ssm1_profile_find( 0xFFFF ) == NULL );
    CHECK( ssm1_profile( 6 ) == NULL );
}

/* The four readings the gauge gives a dial carry their role in every profile,
 * and no other reading claims one. */
static void test_dial_roles_are_set_on_the_right_readings( void )
{
    for ( size_t i = 0; i < ssm1_profile_count(); i++ )
    {
        const ssm1_profile_t * p = ssm1_profile( i );

        for ( size_t a = 0; a < p->count; a++ )
        {
            const ecu_param_info_t * info = p->params[a].info;
            ecu_role_t want               = ECU_ROLE_NONE;

            if ( strcmp( info->name, "Coolant Temp" ) == 0 )
            {
                want = ECU_ROLE_COOLANT;
            }
            else if ( strcmp( info->name, "System Voltage" ) == 0 )
            {
                want = ECU_ROLE_VOLTAGE;
            }
            else if ( strcmp( info->name, "Oxygen Sensor" ) == 0 )
            {
                want = ECU_ROLE_O2;
            }
            else if ( info->decode == ECU_DEC_BOOST )
            {
                want = ECU_ROLE_BOOST;
            }
            CHECK_EQ( info->role, want );
        }
    }
}

static void test_tables_are_well_formed( void )
{
    for ( size_t i = 0; i < ssm1_profile_count(); i++ )
    {
        const ssm1_profile_t * p = ssm1_profile( i );

        CHECK( p->name != NULL );
        CHECK( ( p->count > 0 ) && ( p->count <= SSM1_PARAM_MAX ) );
        CHECK( p->clear_addr != 0 );
        for ( size_t a = 0; a < p->count; a++ )
        {
            const ecu_param_info_t * ia = p->params[a].info;

            CHECK( ia != NULL );
            CHECK( ia->name != NULL );
            CHECK( ia->unit != NULL );
            CHECK( ia->scale_den != 0 );
            CHECK( p->params[a].addr != 0xFFFE );
            if ( ia->decode == ECU_DEC_LOOKUP )
            {
                CHECK( ia->lookup != NULL );
            }
            if ( ia->decode == ECU_DEC_FLAGS )
            {
                CHECK( ( ia->flags != NULL ) && ( ia->flag_count > 0 ) );
            }
            /* addresses must be unique: one slot per address */
            CHECK( p->params[a].addr != p->clear_addr );
            for ( size_t b = a + 1; b < p->count; b++ )
            {
                CHECK( p->params[a].addr != p->params[b].addr );
            }
        }
    }
}

/* The clear applies to the Active and Stored Codes pages and to nothing
 * else, in every profile. A code page that missed the marker could not be
 * cleared; a switch page that had it would offer a clear that does nothing
 * useful. */
static void test_only_the_code_pages_carry_the_marker( void )
{
    for ( size_t i = 0; i < ssm1_profile_count(); i++ )
    {
        const ssm1_profile_t * p = ssm1_profile( i );
        size_t code_pages        = 0;

        for ( size_t k = 0; k < p->count; k++ )
        {
            const ecu_param_info_t * info = p->params[k].info;
            bool named_codes =
                ( strncmp( info->name, "Active Codes", 12 ) == 0 )
                || ( strncmp( info->name, "Stored Codes", 12 ) == 0 );

            CHECK( info->trouble_codes == named_codes );
            code_pages += info->trouble_codes ? 1u : 0u;
        }
        CHECK_EQ( code_pages, 6u ); /* three active and three stored bytes */
    }
}

int main( void )
{
    test_six_profiles_found_by_rom_id();
    test_dial_roles_are_set_on_the_right_readings();
    test_tables_are_well_formed();
    test_only_the_code_pages_carry_the_marker();

    return MT_RESULT();
}
