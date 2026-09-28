/* SSM1 ECU profiles: which addresses to read and how to decode them, one
 * profile per ROM ID. The ECU is picked at runtime by its ROM ID, so this
 * is const data, not code. */
#ifndef SSM1_PARAMS_H
#define SSM1_PARAMS_H

#include <stddef.h>
#include <stdint.h>

#include "ecu.h"

/* Largest parameter count of any profile (compile-time checked). */
#define SSM1_PARAM_MAX ( 32u )

typedef struct
{
    const ecu_param_info_t * info;
    uint16_t addr;
} ssm1_param_t;

typedef enum
{
    SSM1_CLEAR_ZERO,  /* write 0 */
    SSM1_CLEAR_INVERT /* write the bitwise NOT of the current value */
} ssm1_clear_mode_t;

typedef struct
{
    uint16_t rom_id; /* r0 | r1 << 8 */
    const char * name;
    const ssm1_param_t * params;
    size_t count;
    uint16_t clear_addr;
    ssm1_clear_mode_t clear_mode;
} ssm1_profile_t;

/* NULL if the ROM ID is not one we know. */
const ssm1_profile_t * ssm1_profile_find( uint16_t rom_id );
size_t ssm1_profile_count( void );
const ssm1_profile_t * ssm1_profile( size_t idx );

#endif /* SSM1_PARAMS_H */
