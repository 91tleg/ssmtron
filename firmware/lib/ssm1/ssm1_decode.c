#include "ssm1_decode.h"

/* Manifold pressure: t = 100 * (raw - 135.68), which is 12.8 * torr
 * relative to atmosphere. Boost is shown in milli-psi, vacuum in
 * milli-inHg (negative). */
#define BOOST_ZERO     ( 13568 )
#define BOOST_PSI_NUM  ( 15107 )
#define BOOST_INHG_NUM ( 30757 )
#define BOOST_DEN      ( 10000 )

static int32_t boost( uint8_t raw )
{
    int32_t t = ( int32_t )raw * 100 - BOOST_ZERO;
    int32_t v = 0;

    if ( t >= 0 )
    {
        v = ( t * BOOST_PSI_NUM ) / BOOST_DEN;
    }
    else
    {
        v = -( ( -t * BOOST_INHG_NUM ) / BOOST_DEN );
    }
    return v;
}

int32_t ssm1_decode(
    const ecu_param_info_t * info,
    uint8_t raw
)
{
    int32_t v = 0;

    switch ( info->decode )
    {
        case ECU_DEC_LINEAR:
            v = ( ( int32_t )raw * info->scale_num ) / info->scale_den
                + info->offset;
            break;
        case ECU_DEC_SIGNED:
        {
            int32_t x =
                ( raw < 128u ) ? ( int32_t )raw : ( int32_t )raw - 256;

            v = ( x * info->scale_num ) / info->scale_den + info->offset;
            break;
        }
        case ECU_DEC_LOOKUP:
            v = info->lookup[raw];
            break;
        case ECU_DEC_BOOST:
            v = boost( raw );
            break;
        case ECU_DEC_FLAGS:
            v = raw;
            break;
    }
    return v;
}
