/* Sanity of the profile tables. */
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

int main( void )
{
    test_six_profiles_found_by_rom_id();
    test_tables_are_well_formed();
    return MT_RESULT();
}
