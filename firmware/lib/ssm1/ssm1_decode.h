/* Raw byte -> shown value, per the parameter's decode kind. Pure. */
#ifndef SSM1_DECODE_H
#define SSM1_DECODE_H

#include <stdint.h>

#include "ecu.h"

int32_t ssm1_decode(
    const ecu_param_info_t * info,
    uint8_t raw
);

#endif /* SSM1_DECODE_H */
