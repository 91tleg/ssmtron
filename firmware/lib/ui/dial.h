/* Analog dial drawn one row at a time, with no stored bitmap. */
#ifndef DIAL_H
#define DIAL_H

#include <stdbool.h>
#include <stdint.h>

/* Maximum visible characters in a dial label. */
#define DIAL_TEXT_MAX ( 8u )

/* Face and needle look. Only the default exists today; the enum is kept so
 * user-defined dials can be added later without changing dial_cfg_t. */
typedef enum
{
    DIAL_STYLE_DEFAULT
} dial_style_t;

typedef struct
{
    int32_t min_value;       /* value at the sweep's counter-clockwise end */
    int32_t max_value;       /* value at the sweep's clockwise end */
    int32_t major_step;      /* value spacing between labeled ticks; must
                              * evenly divide (max_value - min_value) */
    uint8_t minor_per_major; /* unlabeled ticks between each pair of
                              * labeled ones; major_step must divide
                              * evenly by (minor_per_major + 1) */
    const char * label;      /* short word above center, e.g. "BOOST" */
    const char * unit_pos;   /* unit text for the positive (>= 0) side */
    const char * unit_neg;   /* unit text for the negative side; NULL if
                              * the scale does not change sign */
    uint16_t face_bg;
    uint16_t tick_fg; /* also used for tick numbers, label, and units */
    uint16_t needle_fg;
    uint16_t max_step_deg; /* needle's sweep rate limit, degrees/redraw;
                            * must be > 0 or the needle can never move */
    dial_style_t style;
} dial_cfg_t;

typedef struct
{
    const dial_cfg_t * cfg;
    int16_t displayed_deg; /* the angle actually drawn */
    int16_t target_deg;    /* the angle the latest value maps to */
    uint32_t last_frame_ms;
} dial_widget_t;

/* True once the needle has reached the value it was last given. */
bool dial_settled( const dial_widget_t * d );

/* Draw the face with the needle at min_value. */
void dial_init(
    dial_widget_t * d,
    const dial_cfg_t * cfg,
    uint32_t now_ms
);

/* Move toward value by at most max_step_deg per frame. Redraw only while
 * moving. If has_value is false, return to min_value. */
void dial_poll(
    dial_widget_t * d,
    uint32_t now_ms,
    bool has_value,
    int32_t value
);

#endif /* DIAL_H */
