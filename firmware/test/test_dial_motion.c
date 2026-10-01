/* The needle's motion: how it eases, its top speed, its frame rhythm and how
 * it copes with late frames and a stall. No drawing is involved. */
#include <stdbool.h>

#include "lib/ui/dial_motion.h"
#include "test/minitest.h"

/* The sweep's two ends, as angles on the dial. */
#define LOW_DEG  ( 135 )
#define HIGH_DEG ( 435 )

#define FAST_DPS ( 900u ) /* well above what a small jump needs */
#define SLOW_DPS ( 300u ) /* a full sweep takes a second */

typedef struct
{
    dial_motion_t motion;
    int16_t deg;
} rig_t;

static void rig_init(
    rig_t * r,
    uint32_t now_ms
)
{
    r->deg = LOW_DEG;
    dial_motion_init( &r->motion, r->deg, now_ms );
}

static bool rig_poll(
    rig_t * r,
    uint32_t now_ms,
    int16_t target,
    uint16_t dps
)
{
    return dial_motion_poll( &r->motion, now_ms, target, dps, &r->deg );
}

static void test_a_needle_at_rest_stays_put( void )
{
    rig_t r;

    rig_init( &r, 0u );
    CHECK( dial_motion_reached( &r.motion, LOW_DEG ) );
    CHECK_EQ( r.deg, LOW_DEG );
    for ( uint32_t t = 1u; t < 2000u; t += 7u )
    {
        CHECK( !rig_poll( &r, t, LOW_DEG, SLOW_DPS ) );
    }
    CHECK_EQ( r.deg, LOW_DEG );
}

static void test_a_new_target_waits_for_the_next_frame( void )
{
    rig_t r;

    rig_init( &r, 0u );
    CHECK( !rig_poll( &r, 0u, HIGH_DEG, SLOW_DPS ) );
    CHECK( !dial_motion_reached( &r.motion, HIGH_DEG ) );
    CHECK( !rig_poll( &r, DIAL_MOTION_FRAME_MS - 1u, HIGH_DEG, SLOW_DPS ) );
    CHECK( rig_poll( &r, DIAL_MOTION_FRAME_MS, HIGH_DEG, SLOW_DPS ) );
}

static void test_it_eases_in_without_overshoot( void )
{
    rig_t r;
    uint32_t now = 0u;
    int16_t last_deg;
    int16_t last_step    = 0;
    bool shrinking       = true;
    const int16_t target = LOW_DEG + 30; /* well inside the speed limit */

    rig_init( &r, now );
    last_deg = r.deg;
    for ( int i = 0; i < 12; i++ )
    {
        int16_t step;

        now += DIAL_MOTION_FRAME_MS;
        ( void )rig_poll( &r, now, target, FAST_DPS );
        step = ( int16_t )( r.deg - last_deg );
        CHECK( step >= 0 );       /* never backs up */
        CHECK( r.deg <= target ); /* never overshoots */
        if ( ( last_step > 0 ) && ( step > last_step ) )
        {
            shrinking = false;
        }
        if ( step > 0 )
        {
            last_step = step;
        }
        last_deg = r.deg;
    }
    CHECK( shrinking ); /* each step is smaller than the one before */
    CHECK_EQ( r.deg, target );
    CHECK( dial_motion_reached( &r.motion, target ) );
}

static void test_it_lands_exactly_on_the_target( void )
{
    rig_t r;
    uint32_t now = 0u;

    rig_init( &r, now );
    for ( int i = 0; i < 40; i++ )
    {
        now += DIAL_MOTION_FRAME_MS;
        ( void )rig_poll( &r, now, HIGH_DEG, SLOW_DPS );
    }
    CHECK( dial_motion_reached( &r.motion, HIGH_DEG ) );
    CHECK_EQ( r.deg, HIGH_DEG );
    CHECK_EQ( r.motion.pos_q, HIGH_DEG * 64 );
}

static void test_it_can_move_down_too( void )
{
    rig_t r;
    uint32_t now = 0u;

    rig_init( &r, now );
    for ( int i = 0; i < 40; i++ )
    {
        now += DIAL_MOTION_FRAME_MS;
        ( void )rig_poll( &r, now, HIGH_DEG, SLOW_DPS );
    }
    for ( int i = 0; i < 40; i++ )
    {
        int16_t before = r.deg;

        now += DIAL_MOTION_FRAME_MS;
        ( void )rig_poll( &r, now, LOW_DEG, SLOW_DPS );
        CHECK( r.deg <= before );
    }
    CHECK( dial_motion_reached( &r.motion, LOW_DEG ) );
}

static void test_motion_follows_elapsed_time_not_frame_count( void )
{
    rig_t a;
    rig_t b;
    int32_t diff;
    const int16_t target = LOW_DEG + 30;

    rig_init( &a, 0u );
    rig_init( &b, 0u );

    /* The same 66 ms: two frames for a, one late frame for b. */
    ( void )rig_poll( &a, 33u, target, FAST_DPS );
    ( void )rig_poll( &a, 66u, target, FAST_DPS );
    ( void )rig_poll( &b, 66u, target, FAST_DPS );
    CHECK( a.motion.pos_q > ( LOW_DEG * 64 ) );
    diff = a.motion.pos_q - b.motion.pos_q;
    /* Within a degree of each other. */
    CHECK( ( diff > -64 ) && ( diff < 64 ) );
}

static void test_top_speed_holds_in_every_frame( void )
{
    rig_t r;
    uint32_t now = 0u;

    rig_init( &r, now );
    for ( int i = 0; i < 50; i++ )
    {
        int16_t before = r.deg;

        now += DIAL_MOTION_FRAME_MS;
        ( void )rig_poll( &r, now, HIGH_DEG, SLOW_DPS );
        /* 300 degrees per second is 9.9 degrees in 33 ms, and rounding to
         * whole degrees can add one. */
        CHECK( ( r.deg - before ) <= 11 );
    }
    CHECK_EQ( r.deg, HIGH_DEG );
}

static void test_a_late_frame_moves_further_not_slower( void )
{
    rig_t r;

    rig_init( &r, 0u );
    /* 90 ms late: the limit allows 27 degrees now, not the 9.9 of a normal
     * frame. */
    ( void )rig_poll( &r, 90u, HIGH_DEG, SLOW_DPS );
    CHECK( r.deg > ( LOW_DEG + 20 ) );
    CHECK( r.deg <= ( LOW_DEG + 28 ) );
}

static void test_the_first_move_after_a_long_rest_does_not_lurch( void )
{
    rig_t r;

    rig_init( &r, 0u );
    /* Rest for five seconds, polled on the way. */
    for ( uint32_t t = 0u; t <= 5000u; t += 100u )
    {
        ( void )rig_poll( &r, t, LOW_DEG, SLOW_DPS );
    }
    ( void )rig_poll( &r, 5001u, HIGH_DEG, SLOW_DPS ); /* the value arrives */
    ( void )rig_poll( &r, 5033u, HIGH_DEG, SLOW_DPS );
    /* One normal frame of movement, not a clamp's worth from the rest. */
    CHECK( r.deg > LOW_DEG );
    CHECK( r.deg <= ( LOW_DEG + 11 ) );
}

static void test_a_stall_is_capped_to_one_limited_step( void )
{
    rig_t r;

    rig_init( &r, 0u );
    /* A minute passes without a poll: no more than 100 ms of motion. */
    ( void )rig_poll( &r, 60000u, HIGH_DEG, SLOW_DPS );
    CHECK( r.deg > LOW_DEG );
    CHECK( r.deg <= ( LOW_DEG + 31 ) );
}

static void test_frames_come_at_a_steady_interval( void )
{
    rig_t r;
    uint32_t stamps[20];
    unsigned found = 0u;

    rig_init( &r, 0u );
    /* Polled every millisecond, as the main loop does. */
    for ( uint32_t t = 1u; ( t < 1500u ) && ( found < 20u ); t++ )
    {
        if ( rig_poll( &r, t, HIGH_DEG, SLOW_DPS ) )
        {
            stamps[found] = t;
            found++;
        }
    }
    CHECK_EQ( found, 20u );
    for ( unsigned i = 1u; i < found; i++ )
    {
        CHECK_EQ( stamps[i] - stamps[i - 1u], DIAL_MOTION_FRAME_MS );
    }
}

static void test_the_rhythm_does_not_drift_when_polls_are_late( void )
{
    rig_t r;
    unsigned frames = 0u;
    uint32_t last   = 0u;

    rig_init( &r, 0u );
    /* Polls every 5 ms never land on a due time, so each frame is a little
     * late. Lateness must not add up: frame n is due at 33 n. */
    for ( uint32_t t = 5u; ( t < 1500u ) && ( frames < 20u ); t += 5u )
    {
        if ( rig_poll( &r, t, HIGH_DEG, SLOW_DPS ) )
        {
            frames++;
            last = t;
        }
    }
    CHECK_EQ( frames, 20u );
    CHECK( last <= ( ( 20u * DIAL_MOTION_FRAME_MS ) + 5u ) );
}

static void test_a_stall_drops_frames_instead_of_bursting( void )
{
    rig_t r;
    unsigned frames = 0u;

    rig_init( &r, 0u );
    ( void )rig_poll( &r, 33u, HIGH_DEG, SLOW_DPS );
    /* The loop stalled for half a second: one frame, then the rhythm starts
     * over from there. */
    frames += rig_poll( &r, 533u, HIGH_DEG, SLOW_DPS ) ? 1u : 0u;
    frames += rig_poll( &r, 534u, HIGH_DEG, SLOW_DPS ) ? 1u : 0u;
    frames += rig_poll( &r, 560u, HIGH_DEG, SLOW_DPS ) ? 1u : 0u;
    CHECK_EQ( frames, 1u );
    CHECK( rig_poll( &r, 566u, HIGH_DEG, SLOW_DPS ) );
}

static void test_it_keeps_its_rhythm_across_the_timer_wrap( void )
{
    rig_t r;
    uint32_t start  = 0xFFFFFF00u; /* 256 ms before the 32-bit timer wraps */
    unsigned frames = 0u;

    rig_init( &r, start );
    for ( uint32_t i = 1u; i <= 600u; i++ )
    {
        if ( rig_poll( &r, start + i, HIGH_DEG, SLOW_DPS ) )
        {
            frames++;
        }
    }
    /* 600 ms at one frame per 33 ms, across the wrap. */
    CHECK( frames >= 17u );
    CHECK( frames <= 19u );
}

/* Angles below zero, as on an arc that swings either side of the top: the
 * needle must settle on the exact target, not one degree toward zero. */
static void test_it_settles_exactly_on_a_negative_target( void )
{
    static const int16_t targets[] = { -75, -74, -30, -1 };

    for ( size_t i = 0; i < ( sizeof( targets ) / sizeof( targets[0] ) ); i++ )
    {
        rig_t r;
        uint32_t now = 0u;

        r.deg = 0;
        dial_motion_init( &r.motion, r.deg, now );
        for ( int k = 0; k < 60; k++ )
        {
            now += DIAL_MOTION_FRAME_MS;
            ( void )rig_poll( &r, now, targets[i], SLOW_DPS );
        }
        CHECK( dial_motion_reached( &r.motion, targets[i] ) );
        CHECK_EQ( r.deg, targets[i] );
        CHECK_EQ( r.motion.pos_q, targets[i] * 64 );
    }
}

/* From one end of an arc to the other, through zero, and back. */
static void test_it_crosses_zero_and_lands_on_both_ends( void )
{
    rig_t r;
    uint32_t now = 0u;

    r.deg = -75;
    dial_motion_init( &r.motion, r.deg, now );
    for ( int k = 0; k < 60; k++ )
    {
        now += DIAL_MOTION_FRAME_MS;
        ( void )rig_poll( &r, now, 75, SLOW_DPS );
    }
    CHECK_EQ( r.deg, 75 );
    for ( int k = 0; k < 60; k++ )
    {
        int16_t before = r.deg;

        now += DIAL_MOTION_FRAME_MS;
        ( void )rig_poll( &r, now, -75, SLOW_DPS );
        CHECK( r.deg <= before ); /* straight down, never back up */
    }
    CHECK_EQ( r.deg, -75 );
    CHECK( dial_motion_reached( &r.motion, -75 ) );
}

int main( void )
{
    test_a_needle_at_rest_stays_put();
    test_a_new_target_waits_for_the_next_frame();
    test_it_eases_in_without_overshoot();
    test_it_lands_exactly_on_the_target();
    test_it_can_move_down_too();
    test_motion_follows_elapsed_time_not_frame_count();
    test_top_speed_holds_in_every_frame();
    test_a_late_frame_moves_further_not_slower();
    test_the_first_move_after_a_long_rest_does_not_lurch();
    test_a_stall_is_capped_to_one_limited_step();
    test_frames_come_at_a_steady_interval();
    test_the_rhythm_does_not_drift_when_polls_are_late();
    test_a_stall_drops_frames_instead_of_bursting();
    test_it_keeps_its_rhythm_across_the_timer_wrap();
    test_it_settles_exactly_on_a_negative_target();
    test_it_crosses_zero_and_lands_on_both_ends();
    return MT_RESULT();
}
