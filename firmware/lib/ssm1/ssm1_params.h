/* Read addresses and decoders for each ECU ROM ID. */
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
    SSM1_CLEAR_ZERO,  /* write 0xFF, not literal zero */
    SSM1_CLEAR_INVERT /* write the bitwise NOT of the current value */
} ssm1_clear_mode_t;

typedef struct
{
    const char * name;
    const ssm1_param_t * params;
    size_t count;
    ssm1_clear_mode_t clear_mode;
    uint16_t rom_id; /* r0 | r1 << 8 */
    uint16_t clear_addr;
} ssm1_profile_t;

/* NULL if the ROM ID is not one we know. */
const ssm1_profile_t * ssm1_profile_find( uint16_t rom_id );
size_t ssm1_profile_count( void );
const ssm1_profile_t * ssm1_profile( size_t idx );

#endif /* SSM1_PARAMS_H */
