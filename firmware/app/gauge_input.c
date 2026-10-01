#include "app/gauge_input.h"

#include "hal/hal_log.h"

/* Mechanical switch settle time: a press/release edge must be stable
 * this long before it counts. */
#define DEBOUNCE_MS ( 30u )

/* ACTION released within this long is a tap; held longer, it is the start
 * of a clear. */
#define ACTION_TAP_MS ( 500u )

/* ACTION requires a sustained press to clear. */
#define ACTION_HOLD_MS ( 2000u )

/* NEXT held this long is a hold. Short presses are taps. */
#define NEXT_HOLD_MS ( 1500u )

/* The latest edge of a button. An edge is stamped with the time it happened,
 * not the time of the poll that noticed it, so a slow loop still measures how
 * long the button was down. A press whose release is already waiting is
 * kept apart: it is told to the gestures first, and its release one poll
 * later, so each is seen in order. */
typedef struct
{
    hal_gpio_id_t id;
    bool level;
    uint32_t stable_since;
    bool release_waiting;
    uint32_t release_at;
    bool aside; /* an edge read early and kept for the next poll */
    bool aside_level;
    uint32_t aside_at;
} button_t;

static button_t next_btn;
static button_t action_btn;

/* NEXT tracks one press at a time; DONE waits out the rest of a press
 * that already held or was refused, so it cannot also tap on release. */
typedef enum
{
    NEXT_IDLE,
    NEXT_PRESSED,
    NEXT_DONE
} next_state_t;

typedef enum
{
    ACTION_IDLE,
    ACTION_PRESSED, /* down, not yet a tap or a hold */
    ACTION_HOLDING,
    ACTION_QUIET,       /* the clear has started */
    ACTION_WAIT_RELEASE /* a press was handed back: it ends when released */
} action_state_t;

static action_state_t action_state = ACTION_IDLE;

/* The next edge: one that was set aside, else the HAL's. */
static bool read_edge(
    button_t * b,
    bool * level,
    uint32_t * at
)
{
    bool had = true;

    if ( b->aside )
    {
        *level   = b->aside_level;
        *at      = b->aside_at;
        b->aside = false;
    }
    else
    {
        had = hal_gpio_take_edge( b->id, level, at );
    }
    return had;
}

static void deliver_release( button_t * b )
{
    b->level           = false;
    b->stable_since    = b->release_at;
    b->release_waiting = false;
}

/* A release that was read earlier is told now, after a look for anything
 * newer, all of it, because a hold decision must never see a press that has
 * in fact ended. If the contact closed again within the debounce time, that
 * release was a bounce and never counts, and whatever release follows the
 * bounce takes its place. A press after a longer gap is a new one, set aside
 * for the next poll so the release is seen first. */
static void settle_deferred_release( button_t * b )
{
    bool done = false;

    while ( !done )
    {
        bool level;
        uint32_t at;

        if ( !hal_gpio_take_edge( b->id, &level, &at ) )
        {
            deliver_release( b );
            done = true;
        }
        else if ( !level )
        {
            b->release_at = at; /* a later release replaces it */
        }
        else if ( ( at - b->release_at ) >= DEBOUNCE_MS )
        {
            deliver_release( b );
            b->aside       = true;
            b->aside_level = true;
            b->aside_at    = at;
            done           = true;
        }
        else
        {
            /* The contact closed again: the release was a bounce. The press
             * goes on until the release that follows it, if there is one
             * already. */
            bool next_level;
            uint32_t next_at;

            if ( hal_gpio_take_edge( b->id, &next_level, &next_at )
                 && !next_level )
            {
                b->release_at = next_at;
            }
            else
            {
                b->release_waiting = false;
                done               = true;
            }
        }
    }
}

/* A press that let go before the debounce time is contact noise, however
 * late the loop gets to look at it. Both edges carry their own times, so the
 * pulse is measured by them and not by when they were noticed. */
static void poll_button( button_t * b )
{
    uint32_t edge_at;
    bool level;

    if ( b->release_waiting )
    {
        settle_deferred_release( b );
    }
    else if ( read_edge( b, &level, &edge_at ) )
    {
        uint32_t next_at;
        bool next_level;

        if ( !level )
        {
            b->level        = false;
            b->stable_since = edge_at;
        }
        else if ( hal_gpio_take_edge( b->id, &next_level, &next_at ) )
        {
            if ( next_level )
            {
                /* Another press already: the latest one counts. */
                b->level        = true;
                b->stable_since = next_at;
            }
            else if ( ( next_at - edge_at ) < DEBOUNCE_MS )
            {
                /* Pressed and let go within the debounce time: noise. */
                b->level        = false;
                b->stable_since = next_at;
            }
            else
            {
                b->level           = true;
                b->stable_since    = edge_at;
                b->release_waiting = true;
                b->release_at      = next_at;
            }
        }
        else
        {
            b->level        = true;
            b->stable_since = edge_at;
        }
    }
    else
    {
        /* No new edge. */
    }
}

static bool settled(
    const button_t * b,
    uint32_t now_ms
)
{
    return ( now_ms - b->stable_since ) >= DEBOUNCE_MS;
}

/* How long the button has been down since start_ms. When its release has
 * already been read but is told one poll later, the press ended at the
 * release, not now: a hold decision must not count the time after it. */
static uint32_t held_for(
    const button_t * b,
    uint32_t start_ms,
    uint32_t now_ms
)
{
    return ( b->release_waiting ? b->release_at : now_ms ) - start_ms;
}

void gauge_input_init(
    hal_gpio_id_t next,
    hal_gpio_id_t action
)
{
    next_btn.id   = next;
    action_btn.id = action;
    ( void )hal_gpio_open( next_btn.id, HAL_GPIO_INPUT );
    ( void )hal_gpio_open( action_btn.id, HAL_GPIO_INPUT );
    if ( hal_gpio_watch( next_btn.id ) != HAL_OK )
    {
        hal_log_write( "next btn watch failed" );
    }
    if ( hal_gpio_watch( action_btn.id ) != HAL_OK )
    {
        hal_log_write( "action btn watch failed" );
    }
}

void gauge_input_poll( void )
{
    poll_button( &next_btn );
    poll_button( &action_btn );
}

next_event_t gauge_input_next(
    uint32_t now_ms,
    bool blocked,
    bool has_params
)
{
    static next_state_t next_state = NEXT_IDLE;
    static uint32_t next_hold_start;
    next_event_t event = NEXT_EVENT_NONE;
    bool ready         = settled( &next_btn, now_ms );

    switch ( next_state )
    {
        case NEXT_IDLE:
            if ( next_btn.level && ready )
            {
                if ( !blocked && has_params )
                {
                    next_hold_start = next_btn.stable_since;
                    next_state      = NEXT_PRESSED;
                }
                else
                {
                    /* Not for us: a press that began while busy. */
                    next_state = NEXT_DONE;
                }
            }
            break;

        case NEXT_PRESSED:
            if ( blocked )
            {
                next_state = NEXT_DONE;
            }
            else if ( !next_btn.level && ready )
            {
                /* The release's own time says how long it was down, even
                 * if the loop only noticed after the hold time passed. */
                event      = ( ( next_btn.stable_since - next_hold_start )
                          >= NEXT_HOLD_MS )
                                 ? NEXT_EVENT_HOLD
                                 : NEXT_EVENT_TAP;
                next_state = NEXT_IDLE;
            }
            else if ( next_btn.level
                      && ( held_for( &next_btn, next_hold_start, now_ms )
                           >= NEXT_HOLD_MS ) )
            {
                event      = NEXT_EVENT_HOLD;
                next_state = NEXT_DONE;
            }
            else
            {
                /* Continue waiting for release or the hold threshold. */
            }
            break;

        case NEXT_DONE:
        default:
            if ( !next_btn.level && ready )
            {
                next_state = NEXT_IDLE;
            }
            break;
    }
    return event;
}

action_event_t gauge_input_action( uint32_t now_ms )
{
    /* When the current press began; only this function reads it. */
    static uint32_t action_hold_start;
    action_event_t event = ACTION_EVENT_NONE;

    switch ( action_state )
    {
        case ACTION_IDLE:
            if ( action_btn.level && settled( &action_btn, now_ms ) )
            {
                action_hold_start = action_btn.stable_since;
                action_state      = ACTION_PRESSED;
                event             = ACTION_EVENT_PRESS;
            }
            break;

        case ACTION_PRESSED:
            if ( !action_btn.level && settled( &action_btn, now_ms ) )
            {
                /* The release edge's own time says how long it was down,
                 * even if the loop was slow to notice it. */
                if ( ( action_btn.stable_since - action_hold_start )
                     < ACTION_TAP_MS )
                {
                    event = ACTION_EVENT_TAP;
                }
                action_state = ACTION_IDLE;
            }
            else if ( action_btn.level
                      && ( held_for( &action_btn, action_hold_start, now_ms )
                           >= ACTION_TAP_MS ) )
            {
                action_state = ACTION_HOLDING;
                event        = ACTION_EVENT_HOLD;
            }
            else
            {
                /* Undecided: a tap or the start of a hold. */
            }
            break;

        case ACTION_HOLDING:
            if ( !action_btn.level && settled( &action_btn, now_ms ) )
            {
                action_state = ACTION_IDLE; /* let go before the bar */
                event        = ACTION_EVENT_ABORT;
            }
            else if ( action_btn.level
                      && ( held_for( &action_btn, action_hold_start, now_ms )
                           >= ACTION_HOLD_MS ) )
            {
                action_state = ACTION_QUIET;
                event        = ACTION_EVENT_CLEAR;
            }
            else
            {
                /* Continue waiting for the hold threshold. */
            }
            break;

        case ACTION_WAIT_RELEASE:
            if ( !action_btn.level && settled( &action_btn, now_ms ) )
            {
                action_state = ACTION_IDLE;
            }
            break;

        case ACTION_QUIET:
        default:
            break;
    }
    return event;
}

void gauge_input_action_reset( void )
{
    /* A press still down belongs to whatever ended the last gesture, so it
     * is not a new tap or hold. */
    action_state = action_btn.level ? ACTION_WAIT_RELEASE : ACTION_IDLE;
}

bool gauge_input_action_down( void )
{
    return action_btn.level;
}

bool gauge_input_action_settled( uint32_t now_ms )
{
    return settled( &action_btn, now_ms );
}
