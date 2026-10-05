/* Small integer helpers for the drawing code: absolute value, smaller,
 * larger and clamp. They take and return int32_t, so a value of another
 * width is converted by the caller. */
#ifndef UI_MATH_H
#define UI_MATH_H

#include <stdint.h>

static inline int32_t ui_abs( int32_t v )
{
    return ( v < 0 ) ? -v : v;
}

static inline int32_t ui_min(
    int32_t a,
    int32_t b
)
{
    return ( a < b ) ? a : b;
}

static inline int32_t ui_max(
    int32_t a,
    int32_t b
)
{
    return ( a > b ) ? a : b;
}

/* v held between lo and hi. */
static inline int32_t ui_clamp(
    int32_t v,
    int32_t lo,
    int32_t hi
)
{
    return ui_min( ui_max( v, lo ), hi );
}

#endif /* UI_MATH_H */
