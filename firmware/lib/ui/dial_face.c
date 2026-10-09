/* A dial's face worked out once when the dial opens: where its ticks point,
 * and where its numbers, title and units go. */
#include "lib/ui/dial_face.h"

#include <string.h>

#include "lib/textfit/textfit.h"
#include "lib/ui/dial_angle.h"
#include "lib/ui/dial_layout.h"
#include "lib/ui/ui_math.h"
#include "lib/ui/font.h"

/* The top left corner of a label whose middle is r pixels from the dial's
 * center along this direction. */
/* Adds a piece of text to the face, with its top left corner at (x, y). */
static void add_label(
    dial_face_t * face,
    const char * text,
    uint8_t scale,
    int32_t x,
    int32_t y
)
{
    if ( face->label_count < DIAL_MAX_LABELS )
    {
        dial_label_t * lb = &face->labels[face->label_count];

        size_t len;

        lb->x     = ( int16_t )x;
        lb->y     = ( int16_t )y;
        lb->scale = scale;
        len       = textfit_copy( lb->text, DIAL_LABEL_BUF, text );
        lb->w     = ( uint16_t )font_text_width( len, scale );
        lb->h     = ( uint16_t )( ( uint32_t )FONT_CELL_H * ( uint32_t )scale );
        face->label_count++;
    }
}

/* The number at a long tick. On the round dials its middle is on one circle
 * around the center. On the arc style it is as far out as it can go without
 * touching the ticks: a number reaches along the tick by half its width
 * across the tick's direction plus half its height along it, so a wide
 * number turned toward the side sits nearer the pivot. */
static void add_number(
    const dial_cfg_t * cfg,
    dial_face_t * face,
    int32_t value,
    const dial_tick_t * tick
)
{
    char text[DIAL_LABEL_BUF];
    int32_t shown  = value;
    int32_t height = ( int32_t )FONT_CELL_H * ( int32_t )DIAL_LABEL_SCALE;
    int32_t r      = DIAL_LABEL_R;
    size_t len;
    int32_t width;

    /* A scale with its own unit below zero reads as a plain magnitude
     * there: the unit says which side it is. */
    if ( ( cfg->unit_neg != NULL ) && ( shown < 0 ) )
    {
        shown = -shown;
    }
    len   = textfit_scaled( text, sizeof( text ), shown, cfg->label_div );
    width = font_text_width( len, DIAL_LABEL_SCALE );
    if ( dial_is_arc( cfg ) )
    {
        int32_t sin_abs = ui_abs( tick->sin_m );
        int32_t cos_abs = ui_abs( tick->cos_m );
        int32_t reach   = ( ( width * sin_abs ) + ( height * cos_abs ) ) / 2000;

        r = ( dial_tick_radius( cfg ) - DIAL_TICK_MAJOR_LEN )
            - ( int32_t )DIAL_ARC_LABEL_GAP - reach;
    }
    add_label(
        face,
        text,
        DIAL_LABEL_SCALE,
        ( DIAL_CX + ( ( r * tick->sin_m ) / 1000 ) ) - ( width / 2 ),
        ( ( DIAL_CY + dial_pivot_down( cfg ) )
          - ( ( r * tick->cos_m ) / 1000 ) )
            - ( height / 2 )
    );
}

/* The box around a tick: both ends of its line, and its half width and two
 * pixels more on every side for the soft edge and the rounding. */
static void set_tick_box(
    const dial_cfg_t * cfg,
    dial_tick_t * tick,
    int32_t sin_m,
    int32_t cos_m
)
{
    int32_t r_out = dial_tick_radius( cfg );
    int32_t r_in =
        r_out - ( tick->major ? DIAL_TICK_MAJOR_LEN : DIAL_TICK_MINOR_LEN );
    int32_t half =
        ( tick->major ? DIAL_TICK_MAJOR_HALF_W : DIAL_TICK_HALF_W ) + 2;
    int32_t xa = ( r_in * sin_m ) / 1000;
    int32_t ya = -( r_in * cos_m ) / 1000;
    int32_t xb = ( r_out * sin_m ) / 1000;
    int32_t yb = -( r_out * cos_m ) / 1000;

    tick->box_x0 = ( int16_t )( ui_min( xa, xb ) - half );
    tick->box_y0 = ( int16_t )( ui_min( ya, yb ) - half );
    tick->box_x1 = ( int16_t )( ui_max( xa, xb ) + half );
    tick->box_y1 = ( int16_t )( ui_max( ya, yb ) + half );
}

/* One run of ticks, from one value to another in equal steps. A tick is long
 * (and numbered) when it is a whole number of major steps from anchor. */
static void add_run(
    const dial_cfg_t * cfg,
    dial_face_t * face,
    int32_t from,
    int32_t to,
    int32_t step,
    int32_t major_step,
    int32_t anchor
)
{
    for ( int32_t value = from;
          ( value <= to ) && ( face->tick_count < DIAL_MAX_TICKS );
          value += step )
    {
        dial_tick_t * tick = &face->ticks[face->tick_count];
        int32_t sin_m;
        int32_t cos_m;

        tick->deg   = dial_value_to_deg( cfg, value );
        tick->major = ( ( ( value - anchor ) % major_step ) == 0 );
        dial_angle_vec( tick->deg, &sin_m, &cos_m );
        tick->sin_m = ( int16_t )sin_m;
        tick->cos_m = ( int16_t )cos_m;
        set_tick_box( cfg, tick, sin_m, cos_m );
        face->tick_count++;

        if ( tick->major )
        {
            add_number( cfg, face, value, tick );
        }
    }
}

/* The ticks, and a number at each long one. A scale on both sides of zero
 * may space them differently on each side. */
static void add_ticks(
    const dial_cfg_t * cfg,
    dial_face_t * face
)
{
    int32_t minor_step =
        cfg->major_step / ( ( int32_t )cfg->minor_per_major + 1 );

    if ( ( cfg->min_value < 0 ) && ( cfg->max_value > 0 )
         && ( cfg->neg_major_step > 0 ) )
    {
        int32_t neg_step =
            cfg->neg_major_step / ( ( int32_t )cfg->neg_minor_per_major + 1 );

        add_run(
            cfg,
            face,
            cfg->min_value,
            0,
            neg_step,
            cfg->neg_major_step,
            cfg->min_value
        );
        add_run(
            cfg,
            face,
            minor_step,
            cfg->max_value,
            minor_step,
            cfg->major_step,
            0
        );
    }
    else
    {
        add_run(
            cfg,
            face,
            cfg->min_value,
            cfg->max_value,
            minor_step,
            cfg->major_step,
            cfg->min_value
        );
    }
}

/* The label and the units around the center. */
static void add_fixed_labels(
    const dial_cfg_t * cfg,
    dial_face_t * face
)
{
    bool arc    = ( cfg->style == DIAL_STYLE_UPPER_ARC );
    bool paired = ( cfg->unit_neg != NULL );

    if ( cfg->label != NULL )
    {
        add_label(
            face,
            cfg->label,
            DIAL_FIXED_SCALE,
            DIAL_CX
                - ( font_text_width( strlen( cfg->label ), DIAL_FIXED_SCALE )
                    / 2 ),
            DIAL_CY - ( arc ? UI_LEN( 300 ) : DIAL_TITLE_Y )
        );
    }
    if ( cfg->unit_pos != NULL )
    {
        add_label(
            face,
            cfg->unit_pos,
            DIAL_FIXED_SCALE,
            arc ? ( DIAL_CX
                    - ( font_text_width(
                            strlen( cfg->unit_pos ), DIAL_FIXED_SCALE
                        )
                        / 2 ) )
                : ( DIAL_CX + ( paired ? DIAL_PAIR_X : DIAL_UNIT_X ) ),
            arc ? ( DIAL_CY - UI_LEN( 192 ) )
                : ( paired ? ( DIAL_CY + DIAL_PAIR_DOWN )
                           : ( DIAL_CY - DIAL_UNIT_UP ) )
        );
    }
    if ( ( cfg->unit_neg != NULL ) && ( face->label_count > 0u ) )
    {
        /* Beside the lowest number, which add_ticks made first. */
        const dial_label_t lowest = face->labels[0];

        add_label(
            face,
            cfg->unit_neg,
            DIAL_FIXED_SCALE,
            lowest.x
                + font_text_width( strlen( lowest.text ), DIAL_LABEL_SCALE )
                + DIAL_NEG_UNIT_GAP,
            lowest.y
                + ( ( ( ( int32_t )FONT_CELL_H * ( int32_t )DIAL_LABEL_SCALE )
                      - ( ( int32_t )FONT_CELL_H
                          * ( int32_t )DIAL_FIXED_SCALE ) )
                    / 2 )
        );
    }
}

/* Works out the whole face of a dial, once, when it is opened. */
void dial_make_face(
    const dial_cfg_t * cfg,
    dial_face_t * face
)
{
    face->tick_count  = 0u;
    face->label_count = 0u;
    add_ticks( cfg, face );
    add_fixed_labels( cfg, face );
}

/* Whether a label covers this pixel, and how much ink it puts there. */
bool dial_label_pixel(
    const dial_label_t * lb,
    int16_t row,
    int16_t col,
    uint8_t * out_alpha
)
{
    bool hit = false;

    if ( ( row >= lb->y ) && ( row < ( int16_t )( lb->y + ( int16_t )lb->h ) )
         && ( col >= lb->x )
         && ( col < ( int16_t )( lb->x + ( int16_t )lb->w ) ) )
    {
        uint16_t cell_w      = ( uint16_t )( FONT_CELL_W * lb->scale );
        int16_t row_offset   = ( int16_t )( row - lb->y );
        int16_t col_offset   = ( int16_t )( col - lb->x );
        uint16_t rel_row     = ( uint16_t )row_offset;
        uint16_t rel_col     = ( uint16_t )col_offset;
        uint16_t glyph_index = ( uint16_t )( rel_col / cell_w );
        uint16_t within      = ( uint16_t )( rel_col % cell_w );
        uint8_t alpha =
            font_alpha( lb->text[glyph_index], lb->scale, within, rel_row );

        if ( alpha != 0u )
        {
            *out_alpha = alpha;
            hit        = true;
        }
    }
    return hit;
}
