#include "lib/textfit/textfit.h"

#include <string.h>

static size_t min_size(
    size_t a,
    size_t b
)
{
    return ( a < b ) ? a : b;
}

size_t textfit_copy(
    char * buf,
    size_t max,
    const char * s
)
{
    size_t len = 0u;

    if ( max > 0u )
    {
        len = min_size( strlen( s ), max - 1u );
        ( void )memcpy( buf, s, len );
        buf[len] = '\0';
    }
    return len;
}

/* The longest 32-bit value: a sign and ten digits. */
#define INT_CHARS_MAX ( 11u )

/* Writes the digits of u at out, and returns how many. */
static size_t put_digits(
    char * out,
    uint32_t value
)
{
    char tmp[INT_CHARS_MAX];
    size_t n      = 0u;
    uint32_t rest = value;

    do
    {
        tmp[n] = ( char )( '0' + ( rest % 10u ) );
        n++;
        rest /= 10u;
    } while ( rest != 0u );
    for ( size_t i = 0u; i < n; i++ )
    {
        out[i] = tmp[n - 1u - i];
    }
    return n;
}

size_t textfit_scaled(
    char * buf,
    size_t max,
    int32_t value,
    int32_t div
)
{
    /* A sign, ten digits, a point and the fraction's digits. */
    char tmp[( 2u * INT_CHARS_MAX ) + 2u];
    uint32_t u = ( value < 0 ) ? ( 0u - ( uint32_t )value ) : ( uint32_t )value;
    uint32_t d = ( div > 0 ) ? ( uint32_t )div : 1u;
    uint32_t frac = u % d;
    size_t n      = 0u;

    if ( value < 0 )
    {
        tmp[n] = '-';
        n++;
    }
    if ( ( ( u / d ) != 0u ) || ( frac == 0u ) )
    {
        n += put_digits( &tmp[n], u / d );
    }
    if ( frac != 0u )
    {
        tmp[n] = '.';
        n++;
        /* One digit at a time from the top of the fraction. */
        for ( uint32_t place = d / 10u; ( frac != 0u ) && ( place > 0u );
              place /= 10u )
        {
            tmp[n] = ( char )( '0' + ( frac / place ) );
            n++;
            frac %= place;
        }
    }
    tmp[n] = '\0';
    return textfit_copy( buf, max, tmp );
}

size_t textfit_row(
    char * buf,
    size_t max,
    size_t label_w,
    size_t row_w,
    const char * label,
    const char * value
)
{
    size_t total = 0u;

    if ( max > 0u )
    {
        size_t cap         = min_size( max - 1u, row_w );
        size_t label_cols  = min_size( label_w, cap );
        size_t label_len   = min_size( strlen( label ), label_cols );
        size_t value_limit = ( row_w > label_w ) ? ( row_w - label_w ) : 0u;
        size_t value_len   = min_size(
            strlen( value ), min_size( value_limit, cap - label_cols )
        );

        ( void )memcpy( buf, label, label_len );
        ( void )memset( &buf[label_len], ' ', label_cols - label_len );
        ( void )memcpy( &buf[label_cols], value, value_len );
        total      = label_cols + value_len;
        buf[total] = '\0';
    }
    return total;
}

size_t textfit_center(
    char * buf,
    size_t max,
    const char * s,
    size_t width
)
{
    size_t total = 0u;

    if ( max > 0u )
    {
        size_t cols = min_size( width, max - 1u );
        size_t len  = min_size( strlen( s ), cols );
        size_t pad  = ( cols - len ) / 2u;

        ( void )memset( buf, ' ', pad );
        ( void )memcpy( &buf[pad], s, len );
        total      = pad + len;
        buf[total] = '\0';
    }
    return total;
}
