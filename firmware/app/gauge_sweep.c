#include "app/gauge_sweep.h"

typedef enum
{
    SWEEP_ARMED,   /* waiting for the first dial */
    SWEEP_RUNNING, /* heading for full scale */
    SWEEP_DONE
} sweep_phase_t;

static sweep_phase_t phase = SWEEP_ARMED;

bool gauge_sweep_target(
    const dial_cfg_t * cfg,
    bool needle_settled,
    int32_t * value
)
{
    bool active = false;

    switch ( phase )
    {
        case SWEEP_ARMED:
            /* A new dial starts at rest and counts as settled, so this
             * first poll must not look at needle_settled. */
            phase  = SWEEP_RUNNING;
            *value = cfg->max_value;
            active = true;
            break;

        case SWEEP_RUNNING:
            if ( needle_settled )
            {
                phase = SWEEP_DONE; /* at full scale: the reading takes over */
            }
            else
            {
                *value = cfg->max_value;
                active = true;
            }
            break;

        case SWEEP_DONE:
        default:
            break;
    }
    return active;
}

void gauge_sweep_cancel( void )
{
    phase = SWEEP_DONE;
}
