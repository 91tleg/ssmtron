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
        size_t cap         = max - 1u;
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
