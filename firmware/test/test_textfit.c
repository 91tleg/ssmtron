/* Bounded text builders: what they write, how they cut, and that they never
 * write past the buffer. */
#include <stdbool.h>
#include <string.h>

#include "lib/textfit/textfit.h"
#include "test/minitest.h"

/* A buffer with guard bytes after the size the call is given, to catch a
 * write past it. */
#define GUARD 0x5Au
static char storage[32];

static char * fresh( void )
{
    ( void )memset( storage, ( int )GUARD, sizeof( storage ) );
    return storage;
}

static bool guard_intact( size_t max )
{
    bool ok = true;

    for ( size_t i = max; i < sizeof( storage ); i++ )
    {
        ok = ok && ( ( unsigned char )storage[i] == GUARD );
    }
    return ok;
}

static void test_copy( void )
{
    char * buf = fresh();

    CHECK_EQ( textfit_copy( buf, 8u, "abc" ), 3u );
    CHECK( strcmp( buf, "abc" ) == 0 );
    CHECK( guard_intact( 8u ) );

    /* Exactly full: 7 characters and the NUL. */
    buf = fresh();
    CHECK_EQ( textfit_copy( buf, 8u, "1234567" ), 7u );
    CHECK( strcmp( buf, "1234567" ) == 0 );
    CHECK( guard_intact( 8u ) );

    /* One too long is cut, and still ends in a NUL. */
    buf = fresh();
    CHECK_EQ( textfit_copy( buf, 8u, "12345678" ), 7u );
    CHECK( strcmp( buf, "1234567" ) == 0 );
    CHECK( guard_intact( 8u ) );

    /* Empty text, and a zero-size buffer writes nothing. */
    buf = fresh();
    CHECK_EQ( textfit_copy( buf, 8u, "" ), 0u );
    CHECK_EQ( buf[0], 0 );
    buf = fresh();
    CHECK_EQ( textfit_copy( buf, 0u, "abc" ), 0u );
    CHECK( guard_intact( 0u ) );
}

static void test_int( void )
{
    char * buf = fresh();

    CHECK_EQ( textfit_int( buf, 16u, 0 ), 1u );
    CHECK( strcmp( buf, "0" ) == 0 );
    CHECK_EQ( textfit_int( buf, 16u, 7 ), 1u );
    CHECK( strcmp( buf, "7" ) == 0 );
    CHECK_EQ( textfit_int( buf, 16u, 10 ), 2u );
    CHECK( strcmp( buf, "10" ) == 0 );
    CHECK_EQ( textfit_int( buf, 16u, -30 ), 3u );
    CHECK( strcmp( buf, "-30" ) == 0 );
    CHECK_EQ( textfit_int( buf, 16u, 1234567890 ), 10u );
    CHECK( strcmp( buf, "1234567890" ) == 0 );

    /* The extremes of a 32-bit value, the most negative one included. */
    CHECK_EQ( textfit_int( buf, 16u, 2147483647 ), 10u );
    CHECK( strcmp( buf, "2147483647" ) == 0 );
    CHECK_EQ( textfit_int( buf, 16u, ( -2147483647 - 1 ) ), 11u );
    CHECK( strcmp( buf, "-2147483648" ) == 0 );

    /* Exactly full, then one too long: cut, with a NUL, and nothing written
     * past the size given. */
    buf = fresh();
    CHECK_EQ( textfit_int( buf, 4u, -30 ), 3u );
    CHECK( strcmp( buf, "-30" ) == 0 );
    CHECK( guard_intact( 4u ) );
    buf = fresh();
    CHECK_EQ( textfit_int( buf, 4u, 12345 ), 3u );
    CHECK( strcmp( buf, "123" ) == 0 );
    CHECK( guard_intact( 4u ) );
    buf = fresh();
    CHECK_EQ(
        textfit_int( buf, 9u, ( -2147483647 - 1 ) ), 8u
    ); /* a dial label */
    CHECK( guard_intact( 9u ) );

    buf = fresh();
    CHECK_EQ( textfit_int( buf, 0u, 5 ), 0u );
    CHECK( guard_intact( 0u ) );
}

static void test_row( void )
{
    char * buf = fresh();

    /* The label is padded to 8 columns and the value follows. */
    CHECK_EQ( textfit_row( buf, 21u, 8u, 20u, "FW", "1.2.3" ), 13u );
    CHECK( strcmp( buf, "FW      1.2.3" ) == 0 );
    CHECK( guard_intact( 21u ) );

    /* A long label is cut to the label columns. */
    buf = fresh();
    CHECK_EQ( textfit_row( buf, 21u, 4u, 20u, "PASSKEY", "12" ), 6u );
    CHECK( strcmp( buf, "PASS12" ) == 0 );

    /* A long value is cut so the row is row_w columns at most. */
    buf = fresh();
    CHECK_EQ(
        textfit_row( buf, 21u, 8u, 20u, "ROM", "0123456789ABCDEF" ), 20u
    );
    CHECK( strcmp( buf, "ROM     0123456789AB" ) == 0 );
    CHECK( guard_intact( 21u ) );

    /* The buffer is smaller than the row: the buffer wins. */
    buf = fresh();
    CHECK_EQ( textfit_row( buf, 12u, 8u, 20u, "ROM", "0123456789AB" ), 11u );
    CHECK( strcmp( buf, "ROM     012" ) == 0 );
    CHECK( guard_intact( 12u ) );

    /* A row narrower than the label column cuts the label to the row. */
    buf = fresh();
    CHECK_EQ( textfit_row( buf, 21u, 8u, 4u, "ABCDEFGH", "xy" ), 4u );
    CHECK( strcmp( buf, "ABCD" ) == 0 );
    CHECK( guard_intact( 21u ) );

    /* A label column wider than the buffer cannot overrun it. */
    buf = fresh();
    CHECK_EQ( textfit_row( buf, 5u, 8u, 20u, "AB", "xyz" ), 4u );
    CHECK( strcmp( buf, "AB  " ) == 0 );
    CHECK( guard_intact( 5u ) );

    /* No room for a value at all. */
    buf = fresh();
    CHECK_EQ( textfit_row( buf, 21u, 8u, 8u, "AB", "xyz" ), 8u );
    CHECK( strcmp( buf, "AB      " ) == 0 );

    buf = fresh();
    CHECK_EQ( textfit_row( buf, 0u, 8u, 20u, "AB", "xyz" ), 0u );
    CHECK( guard_intact( 0u ) );
}

static void test_center( void )
{
    char * buf = fresh();

    /* Leading spaces only: 20 columns, 5 characters, 7 spaces before. */
    CHECK_EQ( textfit_center( buf, 21u, "HELLO", 20u ), 12u );
    CHECK( strcmp( buf, "       HELLO" ) == 0 );
    CHECK( guard_intact( 21u ) );

    /* An odd margin leaves the extra column on the right. */
    buf = fresh();
    CHECK_EQ( textfit_center( buf, 21u, "ABCD", 9u ), 6u );
    CHECK( strcmp( buf, "  ABCD" ) == 0 );

    /* Text longer than the width is cut to it. */
    buf = fresh();
    CHECK_EQ( textfit_center( buf, 21u, "ABCDEFGH", 4u ), 4u );
    CHECK( strcmp( buf, "ABCD" ) == 0 );

    /* Empty text gives pad spaces only. */
    buf = fresh();
    CHECK_EQ( textfit_center( buf, 21u, "", 6u ), 3u );
    CHECK( strcmp( buf, "   " ) == 0 );

    /* A width wider than the buffer is cut to the buffer. */
    buf = fresh();
    CHECK_EQ( textfit_center( buf, 6u, "AB", 20u ), 3u );
    CHECK( strcmp( buf, " AB" ) == 0 );
    CHECK( guard_intact( 6u ) );

    buf = fresh();
    CHECK_EQ( textfit_center( buf, 0u, "AB", 20u ), 0u );
    CHECK( guard_intact( 0u ) );
}

int main( void )
{
    test_copy();
    test_int();
    test_row();
    test_center();
    return MT_RESULT();
}
