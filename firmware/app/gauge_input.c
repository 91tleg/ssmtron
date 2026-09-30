#include "gauge_input.h"

#include "hal_log.h"

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

/* The latest edge of a button. A release is stamped with the time of the
 * edge, not of the poll that noticed it, so a slow loop still measures how
 * long the button was down. */
typedef struct
{
    hal_gpio_id_t id;
    bool level;
    uint32_t stable_since;
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

static void poll_button( button_t * b )
{
    uint32_t edge_at;

    if ( hal_gpio_take_edge( b->id, &b->level, &edge_at ) )
    {
        b->stable_since = edge_at;
    }
}

static bool settled(
    const button_t * b,
    uint32_t now_ms
)
{
    return ( now_ms - b->stable_since ) >= DEBOUNCE_MS;
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
                event      = NEXT_EVENT_TAP;
                next_state = NEXT_IDLE;
            }
            else if ( next_btn.level
                      && ( ( now_ms - next_hold_start ) >= NEXT_HOLD_MS ) )
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
            if ( !action_btn.level )
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
            else if ( ( now_ms - action_hold_start ) >= ACTION_TAP_MS )
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
            if ( !action_btn.level )
            {
                action_state = ACTION_IDLE; /* let go before the bar */
                event        = ACTION_EVENT_ABORT;
            }
            else if ( ( now_ms - action_hold_start ) >= ACTION_HOLD_MS )
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
