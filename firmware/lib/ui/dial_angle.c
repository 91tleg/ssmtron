/* Where a value points on a dial: the angle for a reading, and the sine and
 * cosine of an angle. */
#include "lib/ui/dial_angle.h"

#include "lib/ui/dial_layout.h"

void dial_angle_vec(
    int16_t deg,
    int32_t * sin_milli,
    int32_t * cos_milli
)
{
    /* Sine and cosine from -135 to 135 degrees in milliunits, one per degree.
     */
    static const int16_t SIN_MILLI[DIAL_ANGLE_COUNT] = {
        -707,  -719,  -731,  -743, -755, -766, -777, -788, -799, -809, -819,
        -829,  -839,  -848,  -857, -866, -875, -883, -891, -899, -906, -914,
        -921,  -927,  -934,  -940, -946, -951, -956, -961, -966, -970, -974,
        -978,  -982,  -985,  -988, -990, -993, -995, -996, -998, -999, -999,
        -1000, -1000, -1000, -999, -999, -998, -996, -995, -993, -990, -988,
        -985,  -982,  -978,  -974, -970, -966, -961, -956, -951, -946, -940,
        -934,  -927,  -921,  -914, -906, -899, -891, -883, -875, -866, -857,
        -848,  -839,  -829,  -819, -809, -799, -788, -777, -766, -755, -743,
        -731,  -719,  -707,  -695, -682, -669, -656, -643, -629, -616, -602,
        -588,  -574,  -559,  -545, -530, -515, -500, -485, -469, -454, -438,
        -423,  -407,  -391,  -375, -358, -342, -326, -309, -292, -276, -259,
        -242,  -225,  -208,  -191, -174, -156, -139, -122, -105, -87,  -70,
        -52,   -35,   -17,   0,    17,   35,   52,   70,   87,   105,  122,
        139,   156,   174,   191,  208,  225,  242,  259,  276,  292,  309,
        326,   342,   358,   375,  391,  407,  423,  438,  454,  469,  485,
        500,   515,   530,   545,  559,  574,  588,  602,  616,  629,  643,
        656,   669,   682,   695,  707,  719,  731,  743,  755,  766,  777,
        788,   799,   809,   819,  829,  839,  848,  857,  866,  875,  883,
        891,   899,   906,   914,  921,  927,  934,  940,  946,  951,  956,
        961,   966,   970,   974,  978,  982,  985,  988,  990,  993,  995,
        996,   998,   999,   999,  1000, 1000, 1000, 999,  999,  998,  996,
        995,   993,   990,   988,  985,  982,  978,  974,  970,  966,  961,
        956,   951,   946,   940,  934,  927,  921,  914,  906,  899,  891,
        883,   875,   866,   857,  848,  839,  829,  819,  809,  799,  788,
        777,   766,   755,   743,  731,  719,  707
    };

    static const int16_t COS_MILLI[DIAL_ANGLE_COUNT] = {
        -707, -695, -682, -669, -656, -643, -629, -616, -602, -588, -574, -559,
        -545, -530, -515, -500, -485, -469, -454, -438, -423, -407, -391, -375,
        -358, -342, -326, -309, -292, -276, -259, -242, -225, -208, -191, -174,
        -156, -139, -122, -105, -87,  -70,  -52,  -35,  -17,  0,    17,   35,
        52,   70,   87,   105,  122,  139,  156,  174,  191,  208,  225,  242,
        259,  276,  292,  309,  326,  342,  358,  375,  391,  407,  423,  438,
        454,  469,  485,  500,  515,  530,  545,  559,  574,  588,  602,  616,
        629,  643,  656,  669,  682,  695,  707,  719,  731,  743,  755,  766,
        777,  788,  799,  809,  819,  829,  839,  848,  857,  866,  875,  883,
        891,  899,  906,  914,  921,  927,  934,  940,  946,  951,  956,  961,
        966,  970,  974,  978,  982,  985,  988,  990,  993,  995,  996,  998,
        999,  999,  1000, 1000, 1000, 999,  999,  998,  996,  995,  993,  990,
        988,  985,  982,  978,  974,  970,  966,  961,  956,  951,  946,  940,
        934,  927,  921,  914,  906,  899,  891,  883,  875,  866,  857,  848,
        839,  829,  819,  809,  799,  788,  777,  766,  755,  743,  731,  719,
        707,  695,  682,  669,  656,  643,  629,  616,  602,  588,  574,  559,
        545,  530,  515,  500,  485,  469,  454,  438,  423,  407,  391,  375,
        358,  342,  326,  309,  292,  276,  259,  242,  225,  208,  191,  174,
        156,  139,  122,  105,  87,   70,   52,   35,   17,   0,    -17,  -35,
        -52,  -70,  -87,  -105, -122, -139, -156, -174, -191, -208, -225, -242,
        -259, -276, -292, -309, -326, -342, -358, -375, -391, -407, -423, -438,
        -454, -469, -485, -500, -515, -530, -545, -559, -574, -588, -602, -616,
        -629, -643, -656, -669, -682, -695, -707
    };
    int32_t adjusted = deg;
    bool invert      = false;
    int32_t offset;
    size_t idx;

    while ( adjusted > DIAL_SWEEP_MAX_DEG )
    {
        adjusted -= 180;
        invert = !invert;
    }
    while ( adjusted < DIAL_SWEEP_MIN_DEG )
    {
        adjusted += 180;
        invert = !invert;
    }
    offset = adjusted - DIAL_SWEEP_MIN_DEG;
    idx    = ( size_t )offset;

    *sin_milli = invert ? -( int32_t )SIN_MILLI[idx] : SIN_MILLI[idx];
    *cos_milli = invert ? -( int32_t )COS_MILLI[idx] : COS_MILLI[idx];
}

/* Maps v from lo..hi onto deg_lo..deg_hi in a straight line. An empty range
 * has nowhere to sweep, so it gives deg_lo. */
static int32_t lerp_deg(
    int32_t v,
    int32_t lo,
    int32_t hi,
    int32_t deg_lo,
    int32_t deg_hi
)
{
    int32_t span = hi - lo;
    int32_t deg  = deg_lo;

    if ( span != 0 )
    {
        int32_t run_units = v - lo;
        int32_t turn_deg  = deg_hi - deg_lo;
        int64_t run       = ( int64_t )run_units;
        int64_t turn      = ( int64_t )turn_deg;
        int64_t moved     = ( run * turn ) / span;

        deg = deg_lo + ( int32_t )moved;
    }
    return deg;
}

int16_t dial_value_to_deg(
    const dial_cfg_t * cfg,
    int32_t value
)
{
    int32_t clamped = value;
    int32_t deg;

    if ( clamped < cfg->min_value )
    {
        clamped = cfg->min_value;
    }
    else if ( clamped > cfg->max_value )
    {
        clamped = cfg->max_value;
    }
    else
    {
        /* Keep the value unchanged. */
    }
    if ( cfg->style == DIAL_STYLE_UPPER_ARC )
    {
        deg = lerp_deg(
            clamped,
            cfg->min_value,
            cfg->max_value,
            DIAL_UPPER_MIN_DEG,
            DIAL_UPPER_MAX_DEG
        );
    }
    else if ( ( cfg->min_value < 0 ) && ( cfg->max_value > 0 ) )
    {
        /* Zero is at 9 o'clock, with its own degrees per unit each side. */
        deg = ( clamped <= 0 ) ? lerp_deg(
                                     clamped,
                                     cfg->min_value,
                                     0,
                                     DIAL_TWO_SIDED_MIN_DEG,
                                     DIAL_FULL_ZERO_DEG
                                 )
                               : lerp_deg(
                                     clamped,
                                     0,
                                     cfg->max_value,
                                     DIAL_FULL_ZERO_DEG,
                                     DIAL_TWO_SIDED_MAX_DEG
                                 );
    }
    else
    {
        deg = lerp_deg(
            clamped,
            cfg->min_value,
            cfg->max_value,
            DIAL_FULL_MIN_DEG,
            DIAL_FULL_MAX_DEG
        );
    }
    return ( int16_t )deg;
}

/* Where the needle rests: zero when the scale crosses it, else the low end. */
int16_t dial_rest_deg( const dial_cfg_t * cfg )
{
    return dial_value_to_deg(
        cfg, ( cfg->min_value < 0 ) ? 0 : cfg->min_value
    );
}
