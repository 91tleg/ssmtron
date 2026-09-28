#include "fmt.h"

#define DIGITS_MAX ( 11u ) /* 10 digits of a 32-bit value, plus a fill zero */

/* Small writer that tracks overflow instead of writing past the end. */
typedef struct
{
    char * buf;
    size_t max;
    size_t len;
    bool ok;
} out_t;

static void
put( out_t * o,
     char c )
{
    if ( o->ok && ( ( o->len + 1u ) < o->max ) )
    {
        o->buf[o->len] = c;
        o->len++;
    }
    else
    {
        o->ok = false;
    }
}

static void put_str(
    out_t * o,
    const char * s
)
{
    const char * p = s;

    for ( ; ( p != NULL ) && ( *p != '\0' ); p++ )
    {
        put( o, *p );
    }
}

/* Digits of v, at least min_digits of them, with a '.' before the last
 * `decimals` digits when decimals > 0. */
static void put_number(
    out_t * o,
    uint32_t v,
    uint8_t decimals
)
{
    char rev[DIGITS_MAX];
    size_t n           = 0;
    uint32_t remaining = v;

    do
    {
        rev[n] = ( char )( '0' + ( remaining % 10u ) );
        n++;
        remaining /= 10u;
    } while ( ( remaining != 0u ) || ( n <= decimals ) );

    while ( n > 0u )
    {
        n--;
        put( o, rev[n] );
        if ( ( decimals > 0u ) && ( n == decimals ) )
        {
            put( o, '.' );
        }
    }
}

static void put_hex_byte(
    out_t * o,
    uint8_t v
)
{
    static const char hex[] = "0123456789ABCDEF";

    put_str( o, "0x" );
    put( o, hex[v >> 4] );
    put( o, hex[v & 0x0Fu] );
}

static void put_value(
    out_t * o,
    const ecu_param_info_t * info,
    int32_t value
)
{
    if ( info->decode == ECU_DEC_FLAGS )
    {
        put_hex_byte( o, ( uint8_t )value );
    }
    else
    {
        uint32_t mag =
            ( value < 0 ) ? ( 0u - ( uint32_t )value ) : ( uint32_t )value;

        if ( value < 0 )
        {
            put( o, '-' );
        }
        put_number(
            o,
            mag,
            ( info->decimals < ( DIGITS_MAX - 1u ) )
                ? info->decimals
                : ( uint8_t )( DIGITS_MAX - 2u )
        );
    }
}

static size_t finish( out_t * o )
{
    size_t n = 0;

    if ( o->max > 0u )
    {
        if ( o->ok )
        {
            n = o->len;
        }
        else
        {
            o->len = 0;
        }
        o->buf[o->len] = '\0';
    }
    return n;
}

size_t fmt_value(
    char * buf,
    size_t max,
    const ecu_param_info_t * info,
    int32_t value
)
{
    out_t o = { buf, max, 0, true };

    put_value( &o, info, value );
    return finish( &o );
}

size_t fmt_log_line(
    char * buf,
    size_t max,
    uint32_t now_ms,
    const ecu_param_info_t * info,
    int32_t value
)
{
    out_t o = { buf, max, 0, true };

    put_number( &o, now_ms, 0 );
    put( &o, ',' );
    put_str( &o, info->name );
    put( &o, ',' );
    put_value( &o, info, value );
    put( &o, ',' );
    put_str( &o, ecu_unit( info, value ) );
    put( &o, '\n' );
    return finish( &o );
}
