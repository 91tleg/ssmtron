/* Button gestures for the gauge: debounce, tap versus hold, and nothing
 * else. It reports what the user did; gauge.c decides what that does. */
#ifndef GAUGE_INPUT_H
#define GAUGE_INPUT_H

#include <stdbool.h>
#include <stdint.h>

#include "hal_gpio.h"

typedef enum
{
    NEXT_EVENT_NONE,
    NEXT_EVENT_TAP, /* released before the hold time */
    NEXT_EVENT_HOLD /* held for the hold time, once per press */
} next_event_t;

typedef enum
{
    ACTION_EVENT_NONE,
    ACTION_EVENT_PRESS, /* debounced down: a tap or a hold may follow */
    ACTION_EVENT_TAP,   /* released within the tap window */
    ACTION_EVENT_HOLD,  /* still down past the tap window */
    ACTION_EVENT_ABORT, /* released after HOLD, before CLEAR */
    ACTION_EVENT_CLEAR  /* held long enough to clear; then it goes quiet */
} action_event_t;

/* Opens both buttons as inputs and watches their edges. Logs a line if a
 * watch cannot be set up, since a button without a watch cannot respond. */
void gauge_input_init(
    hal_gpio_id_t next_btn,
    hal_gpio_id_t action_btn
);

/* Call once per loop, before the two event calls: reads the latest edges. */
void gauge_input_poll( void );

/* blocked: the UI is busy, so a press must not act (it is ignored if it
 * starts then, and dropped if it is already down). has_params: there is
 * something for a tap to cycle. Both apply only to NEXT. */
next_event_t gauge_input_next(
    uint32_t now_ms,
    bool blocked,
    bool has_params
);

/* Call only while a press is being recognized. After CLEAR it returns
 * nothing until gauge_input_action_reset(). */
action_event_t gauge_input_action( uint32_t now_ms );

/* Back to waiting for a fresh press. If ACTION is still down, that press is
 * ignored until it is released, so it cannot also be a tap or a hold. */
void gauge_input_action_reset( void );

/* ACTION as it stands now, for a caller that has stopped calling
 * gauge_input_action(): still down, and down for the debounce time. */
bool gauge_input_action_down( void );
bool gauge_input_action_settled( uint32_t now_ms );

#endif /* GAUGE_INPUT_H */
