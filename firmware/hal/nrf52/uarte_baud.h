/* nRF52 UARTE BAUDRATE register math. Pure, host-testable.
 * reg = baud * 2^32 / 16 MHz; the low 12 bits are ignored by the hardware.
 * Zephyr's driver only knows a fixed baud table, so we compute this
 * ourselves. */
#ifndef UARTE_BAUD_H
#define UARTE_BAUD_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define UARTE_BASE_HZ ( 16000000ull )
#define UARTE_BAUD_MAX_ERR_PCT                                                 \
    ( 1u ) /* async tolerance is a few %; keep well inside */

static inline bool uarte_baud_to_reg(
    uint32_t baud,
    uint32_t * reg
)
{
    bool ok    = ( reg != NULL ) && ( baud != 0 ) && ( baud <= 1000000u );
    uint64_t v = 0;

    if ( ok )
    {
        v  = ( ( ( uint64_t )baud << 32 ) + UARTE_BASE_HZ / 2 ) / UARTE_BASE_HZ;
        v  = ( v + 0x800u ) & ~0xFFFull; /* hardware granularity */
        ok = v != 0 && v <= 0xFFFFFFFFull;
    }
    if ( ok )
    {
        uint64_t actual = ( v * UARTE_BASE_HZ ) >> 32;
        uint64_t err    = actual > baud ? actual - baud : baud - actual;
        ok = err * 100u <= ( uint64_t )baud * UARTE_BAUD_MAX_ERR_PCT;
    }
    if ( ok )
    {
        *reg = ( uint32_t )v;
    }
    return ok;
}

#endif /* UARTE_BAUD_H */
