#include "lib/ui/dial_motion.h"

/* The position is kept in 1/64 degree. Only whole degrees are drawn, but a
 * slow approach would otherwise stall a fraction short of its target. */
#define DIAL_MOTION_SUBDEG ( 64 )

/* The needle closes on its target with this time constant: about 63 percent
 * of the gap in one time constant, 95 percent in three. */
#define DIAL_MOTION_RESPONSE_MS ( 40u )

/* A frame never covers more than this much time, so a stall (or a needle
 * made long before its first poll) does not make it lurch. */
#define DIAL_MOTION_MAX_DT_MS ( 100u )

/* Within this of the target the needle lands on it. */
#define DIAL_MOTION_SNAP ( 4 )

/* Half of the 32-bit millisecond timer's range. */
#define DIAL_MOTION_HALF_RANGE ( 0x80000000u )

/* Moves pos toward target over dt_ms. The gap shrinks by the same fraction
 * every millisecond, so one long frame and two short ones end in nearly the
 * same place. The distance covered is then limited to the needle's top
 * speed. */
static int32_t advance(
    int32_t pos,
    int32_t target,
    uint32_t dt_ms,
    uint16_t max_deg_per_s
)
{
    int32_t gap = target - pos;
    int32_t moved;
    uint32_t limit_q =
        ( ( uint32_t )max_deg_per_s * ( uint32_t )DIAL_MOTION_SUBDEG * dt_ms )
        / 1000u;
    int32_t limit = ( int32_t )limit_q;

    for ( uint32_t i = 0u; i < dt_ms; i++ )
    {
        gap = ( gap * ( ( int32_t )DIAL_MOTION_RESPONSE_MS - 1 ) )
              / ( int32_t )DIAL_MOTION_RESPONSE_MS;
    }
    moved = ( target - pos ) - gap;
    if ( moved > limit )
    {
        moved = limit;
    }
    else if ( moved < -limit )
    {
        moved = -limit;
    }
    else
    {
        /* within the top speed */
    }
    return pos + moved;
}

/* True once now has reached due, counted across the timer's wraparound. */
static bool time_reached(
    uint32_t now_ms,
    uint32_t due_ms
)
{
    uint32_t past = now_ms - due_ms;

    return past < DIAL_MOTION_HALF_RANGE;
}

/* The whole degree nearest pos. Angles on the dial are never negative. */
static int16_t round_deg( int32_t pos )
{
    return ( int16_t )( ( pos + ( DIAL_MOTION_SUBDEG / 2 ) )
                        / DIAL_MOTION_SUBDEG );
}

void dial_motion_init(
    dial_motion_t * n,
    int16_t deg,
    uint32_t now_ms
)
{
    n->pos_q   = ( int32_t )deg * DIAL_MOTION_SUBDEG;
    n->last_ms = now_ms;
    n->next_ms = now_ms + DIAL_MOTION_FRAME_MS;
}

bool dial_motion_reached(
    const dial_motion_t * n,
    int16_t target_deg
)
{
    return n->pos_q == ( ( int32_t )target_deg * DIAL_MOTION_SUBDEG );
}

/* One frame of motion. The next due time moves on from the previous one, so
 * the rhythm stays even; a frame that ran a whole interval late drops the
 * frames it missed instead of running them back to back. */
static void step_frame(
    dial_motion_t * n,
    uint32_t now_ms,
    int32_t target_q,
    uint16_t max_deg_per_s
)
{
    uint32_t dt_ms = now_ms - n->last_ms;
    int32_t gap;

    if ( dt_ms > DIAL_MOTION_MAX_DT_MS )
    {
        dt_ms = DIAL_MOTION_MAX_DT_MS;
    }
    n->pos_q = advance( n->pos_q, target_q, dt_ms, max_deg_per_s );
    gap      = target_q - n->pos_q;
    if ( ( gap <= DIAL_MOTION_SNAP ) && ( gap >= -DIAL_MOTION_SNAP ) )
    {
        n->pos_q = target_q;
    }
    n->last_ms = now_ms;
    n->next_ms += DIAL_MOTION_FRAME_MS;
    if ( time_reached( now_ms, n->next_ms ) )
    {
        n->next_ms = now_ms + DIAL_MOTION_FRAME_MS;
    }
}

bool dial_motion_poll(
    dial_motion_t * n,
    uint32_t now_ms,
    int16_t target_deg,
    uint16_t max_deg_per_s,
    int16_t * deg
)
{
    int32_t target_q = ( int32_t )target_deg * DIAL_MOTION_SUBDEG;
    int16_t before   = *deg;

    if ( n->pos_q == target_q )
    {
        /* At rest: the clock for the next move starts now. */
        n->last_ms = now_ms;
        n->next_ms = now_ms + DIAL_MOTION_FRAME_MS;
    }
    else if ( time_reached( now_ms, n->next_ms ) )
    {
        step_frame( n, now_ms, target_q, max_deg_per_s );
        *deg = round_deg( n->pos_q );
    }
    else
    {
        /* Not due yet. */
    }
    return *deg != before;
}
