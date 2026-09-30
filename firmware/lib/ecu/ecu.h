/* Single-instance ECU protocol interface. One protocol is linked per build;
 * the app supplies bytes and time. */
#ifndef ECU_H
#define ECU_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef enum
{
    ECU_LINK_DOWN,
    ECU_LINK_UP
} ecu_link_t;

typedef enum
{
    ECU_PARITY_NONE,
    ECU_PARITY_EVEN,
    ECU_PARITY_ODD
} ecu_parity_t;

/* Serial settings the protocol needs; the app applies them to the HAL. */
typedef struct
{
    uint32_t baud;
    uint8_t data_bits;
    ecu_parity_t parity;
    uint8_t stop_bits;
} ecu_link_cfg_t;

/* How a raw byte becomes a shown value. */
typedef enum
{
    ECU_DEC_LINEAR, /* raw * num / den + offset */
    ECU_DEC_SIGNED, /* (int8)raw * num / den + offset */
    ECU_DEC_LOOKUP, /* lookup[raw] */
    ECU_DEC_BOOST,  /* manifold pressure: psi >= 0, inHg below */
    ECU_DEC_FLAGS   /* the raw byte itself; see flags[] */
} ecu_decode_t;

/* One labelled bit of a flags byte. Set when the masked bits are non-zero
 * (active_high) or zero (!active_high). */
typedef struct
{
    const char * name;
    uint8_t mask;
    bool active_high;
} ecu_flag_t;

/* Values are integers in units of 10^-decimals. */
typedef struct
{
    const char * name;
    const char * unit;
    const char * unit_neg; /* unit while the value is negative; NULL = unit */
    ecu_decode_t decode;
    int32_t scale_num;
    int32_t scale_den;
    int32_t offset;
    uint8_t decimals;
    const int16_t * lookup;   /* 256 entries, ECU_DEC_LOOKUP */
    const ecu_flag_t * flags; /* ECU_DEC_FLAGS */
    uint8_t flag_count;
    /* Thresholds use ecu_get_value units. Without verified thresholds,
     * the display must not color-code a value. */
    bool has_thresholds;
    int32_t warn_value;
    int32_t redline_value;
    /* A flags byte whose bits are trouble codes: the only pages the clear
     * command applies to. Switch bytes and values are false. */
    bool trouble_codes;
} ecu_param_info_t;

static inline const char * ecu_unit(
    const ecu_param_info_t * info,
    int32_t value
)
{
    return ( ( value < 0 ) && ( info->unit_neg != NULL ) ) ? info->unit_neg
                                                           : info->unit;
}

static inline bool ecu_flag_set(
    const ecu_flag_t * flag,
    uint8_t raw
)
{
    bool masked = ( ( raw & flag->mask ) != 0u );

    return masked == flag->active_high;
}

/* Higher is worse (redline is always an upper bound: rpm, temp, boost),
 * matching what "redline" means everywhere else. A parameter with no
 * verified thresholds (has_thresholds false) is always ECU_LEVEL_NORMAL.
 */
typedef enum
{
    ECU_LEVEL_NORMAL,
    ECU_LEVEL_WARN,
    ECU_LEVEL_REDLINE
} ecu_level_t;

static inline ecu_level_t ecu_level(
    const ecu_param_info_t * info,
    int32_t value
)
{
    ecu_level_t level = ECU_LEVEL_NORMAL;

    if ( info->has_thresholds && ( value >= info->warn_value ) )
    {
        level = ( value >= info->redline_value ) ? ECU_LEVEL_REDLINE
                                                 : ECU_LEVEL_WARN;
    }
    return level;
}

typedef enum
{
    ECU_CLEAR_IDLE,
    ECU_CLEAR_BUSY,
    ECU_CLEAR_DONE, /* turn the ignition off, then on */
    ECU_CLEAR_FAILED
} ecu_clear_t;

const ecu_link_cfg_t * ecu_link_config( void );

void ecu_init( void );
/* Call every loop, before feeding bytes: rx timing uses the last tick time.
 */
void ecu_tick( uint32_t now_ms );
void ecu_feed( uint8_t byte );
/* A received byte had a parity/framing error (its value is untrusted). */
void ecu_feed_error( void );
/* Copies at most max bytes of pending output; 0 = nothing (or max too
 * small, in which case the output stays queued). */
size_t ecu_next_tx(
    uint8_t * buf,
    size_t max
);

ecu_link_t ecu_link( void );

/* The ECU is identified on its own after ecu_init. Until then the name is
 * NULL and there are no parameters. An ECU that answers with an ID we do
 * not know is named "Unknown" and has no parameters. */
const char * ecu_name( void );
/* ROM ID bytes as r0 | r1 << 8 | r2 << 16; 0 until identified. */
uint32_t ecu_rom_id( void );

/* Shows a remembered ECU's parameters before it has identified itself, so
 * a gauge can draw its pages at power-on. profile_key is the low 16 bits of
 * a ROM ID from an earlier run; an unknown key uses the first profile.
 * Returns the key of the profile actually used. Call it after ecu_init and
 * before the ECU answers.
 * The ECU is still not identified: ecu_name and ecu_rom_id report nothing,
 * and ecu_clear_start refuses, because nothing may be written to an address
 * taken from a guess. When the ECU identifies itself, the same profile keeps
 * the selection made meanwhile; a different one replaces it and the
 * selection goes back to the first parameter. */
uint16_t ecu_assume_profile( uint16_t profile_key );

size_t ecu_param_count( void );
const ecu_param_info_t * ecu_param_info( size_t idx );
/* Start monitoring a parameter. False if idx is out of range (so false
 * until a profile is known, identified or assumed). */
bool ecu_select_param( size_t idx );
/* Latest value; false if never read or stale. For ECU_DEC_FLAGS the value
 * is the raw byte. */
bool ecu_get_value(
    size_t idx,
    int32_t * out
);

/* Clear the ECU's stored trouble codes. Interrupts monitoring, then goes
 * back to the selected parameter. False if not identified, unsupported or
 * already running. */
bool ecu_clear_start( void );
ecu_clear_t ecu_clear_state( void );

#endif /* ECU_H */
