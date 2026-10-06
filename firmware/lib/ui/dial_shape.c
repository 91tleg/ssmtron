/* How much of a pixel a shape covers, for the shapes a dial is drawn from:
 * a straight bar, a disc and the needle. */
#include "lib/ui/dial_shape.h"

#include "lib/ui/dial_layout.h"
#include "lib/ui/ui_math.h"

/* A broad pointer that tapers gently along its blade, then ends in a short
 * point. The tip keeps half a pixel of width so it remains visible. */
uint8_t dial_needle_alpha(
    int32_t dx,
    int32_t dy,
    int32_t sin_milli,
    int32_t cos_milli,
    int32_t len
)
{
    int32_t proj   = ( dx * sin_milli ) - ( dy * cos_milli );
    uint8_t alpha  = 0u;
    int32_t beyond = ( len * 1000 ) - proj;

    if ( ( len > 0 ) && ( proj >= -500 ) && ( beyond >= -500 ) )
    {
        int32_t perp    = ( dx * cos_milli ) + ( dy * sin_milli );
        int32_t across  = ui_abs( perp );
        int32_t tip_run = ui_max( beyond, 0 );
        int32_t room =
            ( ( DIAL_NEEDLE_HALF_W * tip_run ) / DIAL_NEEDLE_TIP_LEN ) + 500;
        int32_t blade_room = ( DIAL_NEEDLE_HALF_W * 1000 )
                             - ( ( proj * ( DIAL_NEEDLE_HALF_W / 3 ) ) / len );
        int32_t dist;

        if ( room > blade_room )
        {
            room = blade_room;
        }
        dist  = room - across;
        dist  = ui_min( beyond, dist );
        dist  = ui_min( proj, dist );
        alpha = dial_edge_alpha( dist );
    }
    return alpha;
}
