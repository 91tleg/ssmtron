/* Test what the gauge remembers, against a fake key-value store. */
#include <stdint.h>
#include <string.h>

#include "gauge_settings.h"
#include "hal_nvm.h"
#include "minitest.h"

/* A store of two known keys, so a load returns what was last saved. */
typedef struct
{
    const char * key;
    uint32_t value;
    bool present;
} slot_t;

static slot_t slots[] = { { "param_idx", 0, false }, { "theme", 0, false } };
static unsigned writes;
static bool fail_writes;

static slot_t * find( const char * key )
{
    slot_t * found = NULL;

    for ( size_t i = 0; i < ( sizeof( slots ) / sizeof( slots[0] ) ); i++ )
    {
        if ( strcmp( slots[i].key, key ) == 0 )
        {
            found = &slots[i];
        }
    }
    return found;
}

hal_status_t hal_nvm_save(
    const char * key,
    const void * data,
    size_t len
)
{
    slot_t * s = find( key );

    if ( ( s != NULL ) && !fail_writes && ( len == sizeof( s->value ) ) )
    {
        ( void )memcpy( &s->value, data, len );
        s->present = true;
        writes++;
    }
    return ( ( s != NULL ) && !fail_writes ) ? HAL_OK : HAL_ERR_IO;
}

hal_status_t hal_nvm_load(
    const char * key,
    void * data,
    size_t max,
    size_t * len
)
{
    slot_t * s          = find( key );
    hal_status_t status = HAL_ERR_INVALID;

    if ( ( s != NULL ) && s->present && ( max >= sizeof( s->value ) ) )
    {
        ( void )memcpy( data, &s->value, sizeof( s->value ) );
        *len   = sizeof( s->value );
        status = HAL_OK;
    }
    return status;
}

static void test_nothing_stored_loads_nothing( void )
{
    size_t out = 99u;

    CHECK( !gauge_settings_load_param( 5u, &out ) );
    CHECK( !gauge_settings_load_theme( 2u, &out ) );
    CHECK_EQ( out, 99u ); /* left alone */
}

static void test_first_save_writes_both( void )
{
    writes = 0;
    CHECK( gauge_settings_save( 3u, 1u ) );
    CHECK_EQ( writes, 2u );
}

static void test_repeat_save_writes_nothing( void )
{
    writes = 0;
    CHECK( gauge_settings_save( 3u, 1u ) );
    CHECK_EQ( writes, 0u );
}

static void test_only_the_changed_value_is_written( void )
{
    writes = 0;
    CHECK( gauge_settings_save( 4u, 1u ) ); /* theme unchanged */
    CHECK_EQ( writes, 1u );
    CHECK( gauge_settings_save( 4u, 0u ) ); /* param unchanged */
    CHECK_EQ( writes, 2u );
}

static void test_saved_values_load_back( void )
{
    size_t param = 0;
    size_t theme = 0;

    CHECK( gauge_settings_load_param( 5u, &param ) );
    CHECK( gauge_settings_load_theme( 2u, &theme ) );
    CHECK_EQ( param, 4u );
    CHECK_EQ( theme, 0u );
}

static void test_a_stored_value_out_of_range_is_refused( void )
{
    size_t out = 99u;

    /* 4 is stored, but the ECU now reports only 3 parameters. */
    CHECK( !gauge_settings_load_param( 3u, &out ) );
    CHECK_EQ( out, 99u );
}

static void test_a_failed_write_is_reported_and_retried( void )
{
    fail_writes = true;
    writes      = 0;
    CHECK( !gauge_settings_save( 2u, 0u ) ); /* param differs: needs a write */
    CHECK_EQ( writes, 0u );

    fail_writes = false;
    CHECK( gauge_settings_save( 2u, 0u ) ); /* not remembered as stored */
    CHECK_EQ( writes, 1u );
}

int main( void )
{
    test_nothing_stored_loads_nothing();
    test_first_save_writes_both();
    test_repeat_save_writes_nothing();
    test_only_the_changed_value_is_written();
    test_saved_values_load_back();
    test_a_stored_value_out_of_range_is_refused();
    test_a_failed_write_is_reported_and_retried();

    return MT_RESULT();
}
