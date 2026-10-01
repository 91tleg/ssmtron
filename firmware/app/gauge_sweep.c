#include "app/gauge_sweep.h"

typedef enum
{
    SWEEP_ARMED, /* waiting for the first dial */
    SWEEP_MAX,   /* heading for full scale */
    SWEEP_MIN,   /* heading for the bottom of the scale */
    SWEEP_REST,  /* heading back to rest */
    SWEEP_DONE
} sweep_phase_t;

/* Time to cross the whole scale. Every leg is a share of it, so a half-scale
 * leg takes half as long. */
#define SWEEP_SCALE_MS ( 1000u )

/* One poll counts for at most this much time, so a stalled loop does not
 * skip the needle ahead. */
#define SWEEP_STEP_MAX_MS ( 100u )

static sweep_phase_t phase = SWEEP_ARMED;
static uint32_t elapsed_ms; /* how far into the leg */
static int32_t leg_from;
static int32_t leg_to;
static bool leg_reached; /* the aim has been given the end of the leg */

/* Where the needle rests: zero when the scale crosses it, else the bottom. */
static int32_t rest_value( const dial_cfg_t * cfg )
{
    return ( cfg->min_value < 0 ) ? 0 : cfg->min_value;
}

static uint32_t leg_duration_ms( const dial_cfg_t * cfg )
{
    int64_t span     = ( int64_t )cfg->max_value - ( int64_t )cfg->min_value;
    int64_t distance = ( int64_t )leg_to - ( int64_t )leg_from;
    int64_t scale_ms = ( int64_t )SWEEP_SCALE_MS;
    int64_t ms       = 0;

    if ( distance < 0 )
    {
        distance = -distance;
    }
    if ( span > 0 )
    {
        ms = ( distance * scale_ms ) / span;
    }
    return ( uint32_t )ms;
}

/* The value the aim has reached along the leg. */
static int32_t leg_aim( const dial_cfg_t * cfg )
{
    uint32_t duration = leg_duration_ms( cfg );
    int32_t aim       = leg_to;

    if ( elapsed_ms < duration )
    {
        int64_t delta = ( int64_t )leg_to - ( int64_t )leg_from;
        int64_t moved = ( delta * ( int64_t )elapsed_ms ) / ( int64_t )duration;

        aim = leg_from + ( int32_t )moved;
    }
    return aim;
}

static void start_leg(
    sweep_phase_t next,
    int32_t from,
    int32_t to
)
{
    phase       = next;
    leg_from    = from;
    leg_to      = to;
    elapsed_ms  = 0u;
    leg_reached = false;
}

bool gauge_sweep_target(
    const dial_cfg_t * cfg,
    bool needle_settled,
    uint32_t now_ms,
    int32_t * value
)
{
    static uint32_t last_ms;             /* the time of the previous poll */
    static const dial_cfg_t * sweep_cfg; /* the dial being swept */
    uint32_t step = now_ms - last_ms;
    bool active   = ( phase != SWEEP_DONE );

    if ( ( phase != SWEEP_ARMED ) && ( phase != SWEEP_DONE )
         && ( cfg != sweep_cfg ) )
    {
        /* Another dial is showing: the sweep's legs belong to the first
         * dial's scale, so it ends rather than aim at the wrong one. */
        phase  = SWEEP_DONE;
        active = false;
    }
    if ( phase == SWEEP_ARMED )
    {
        /* A new dial starts at rest. */
        sweep_cfg = cfg;
        start_leg( SWEEP_MAX, rest_value( cfg ), cfg->max_value );
        step = 0u;
    }
    last_ms = now_ms;
    if ( step > SWEEP_STEP_MAX_MS )
    {
        step = SWEEP_STEP_MAX_MS;
    }
    if ( active )
    {
        elapsed_ms += step;
        /* The needle has to arrive, as well as the aim, before the next leg
         * starts. The end of a leg is given to the dial first. */
        if ( leg_reached && needle_settled && ( step > 0u ) )
        {
            if ( phase == SWEEP_MAX )
            {
                start_leg( SWEEP_MIN, cfg->max_value, cfg->min_value );
            }
            else if ( phase == SWEEP_MIN )
            {
                start_leg( SWEEP_REST, cfg->min_value, rest_value( cfg ) );
            }
            else
            {
                phase  = SWEEP_DONE; /* back at rest: the reading takes over */
                active = false;
            }
        }
        if ( active )
        {
            *value      = leg_aim( cfg );
            leg_reached = ( elapsed_ms >= leg_duration_ms( cfg ) );
        }
    }
    return active;
}

void gauge_sweep_cancel( void )
{
    phase = SWEEP_DONE;
}
