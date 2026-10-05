/* The dial's measurements, shared by the files that draw it. Lengths are in
 * thousandths of the panel's radius (see ui_geom.h), so they follow the
 * panel's size. */
#ifndef DIAL_LAYOUT_H
#define DIAL_LAYOUT_H

#include <stdbool.h>
#include <stdint.h>

#include "lib/display/display.h"
#include "lib/ui/dial.h"
#include "lib/ui/ui_geom.h"

#define DIAL_PANEL_W ( ( int )DISPLAY_WIDTH )
#define DIAL_PANEL_H ( ( int )DISPLAY_HEIGHT )
#define DIAL_CX      UI_CX
#define DIAL_CY      UI_CY

/* 270 degree sweep, with zero straight up and positive clockwise. */
#define DIAL_SWEEP_MIN_DEG ( -135 )
#define DIAL_SWEEP_MAX_DEG ( 135 )
#define DIAL_ANGLE_COUNT   ( 271u )

/* Unwrapped angles keep the needle on the lower arc. */
#define DIAL_FULL_MIN_DEG  ( 135 )
#define DIAL_FULL_ZERO_DEG ( 270 )
#define DIAL_FULL_MAX_DEG  ( 435 )

/* A scale on both sides of zero (boost and vacuum) has its lowest value at 6
 * o'clock and its highest at 3 o'clock, with zero at 9 o'clock between them.
 * Each side has its own degrees per unit. */
#define DIAL_TWO_SIDED_MIN_DEG ( 180 )
#define DIAL_TWO_SIDED_MAX_DEG ( 450 )
#define DIAL_UPPER_MIN_DEG     ( -45 )
#define DIAL_UPPER_MAX_DEG     ( 45 )

#define DIAL_TICK_R UI_LEN( 825 )

#define DIAL_TICK_MAJOR_LEN UI_LEN( 134 )
#define DIAL_TICK_MINOR_LEN UI_LEN( 67 )

/* The arc style (coolant, voltage, O2) is a flatter arc than the round
 * dials. Its needle pivots below the middle of the panel, under a cover
 * plate whose top edge is at the middle: the needle comes out from behind it.
 * The ticks are on a circle around the pivot that is wider than the panel's
 * own, so only the top of it shows. The cover is a trapezoid, wider toward
 * the bottom, with a foot on each side where a screw holds it. */
#define DIAL_ARC_PIVOT_DOWN      UI_LEN( 446 )
#define DIAL_ARC_TICK_R          UI_LEN( 1165 )
#define DIAL_ARC_LABEL_GAP       UI_LEN( 25 )
#define DIAL_COVER_TOP_HALF_W    UI_LEN( 458 )
#define DIAL_COVER_BOTTOM_HALF_W UI_LEN( 583 )
#define DIAL_COVER_FOOT_X        UI_LEN( 860 )
#define DIAL_COVER_FOOT_TOP      UI_LEN( 330 )
#define DIAL_COVER_FOOT_BOTTOM   UI_LEN( 550 )
#define DIAL_ARC_SCREW_X         UI_LEN( 710 )
#define DIAL_ARC_SCREW_Y         UI_LEN( 437 )
/* Below this, under the cover, the needle can no longer be seen. */
#define DIAL_ARC_NEEDLE_SHOWS_TO UI_LEN( 100 )
/* The cover's body stands out from the face: its top corners are rounded, it
 * casts a soft shadow down and to the right, its top edge catches the light
 * and its sides fall into shade. The feet are flat on the face. */
#define DIAL_COVER_CORNER_R      UI_LEN( 100 )
#define DIAL_COVER_SHADOW_OFFSET UI_LEN( 25 )
#define DIAL_COVER_SHADOW_SPREAD UI_LEN( 50 )
#define DIAL_COVER_BEVEL         UI_LEN( 25 )
#define DIAL_COVER_SIDE_SHADE    UI_LEN( 67 )

/* Line thicknesses are half-widths, so 1 is a line 3 px wide. They follow
 * the panel size, rounded down. */
#define DIAL_TICK_HALF_W       UI_LEN( 9 )
#define DIAL_TICK_MAJOR_HALF_W UI_LEN( 26 )
#define DIAL_NEEDLE_HALF_W     UI_LEN( 34 )
#define DIAL_TAIL_HALF_W       UI_LEN( 42 )

/* The hub: a ring with a lighter disc inside it. */
#define DIAL_HUB_R       UI_LEN( 100 )
#define DIAL_HUB_INNER_R UI_LEN( 75 )

/* How much of the needle's end is the triangle. */
#define DIAL_NEEDLE_TIP_LEN UI_LEN( 100 )

/* Two slotted screws on the face, as on a gauge's bezel plate. Centers are
 * in thousandths of the radius from the dial's center, clear of the label,
 * the units and the tick numbers. The needle passes over them. */
#define DIAL_SCREW_R            UI_LEN( 66 )
#define DIAL_SCREW_SLOT_HALF_W  ( 650 )
#define DIAL_SCREW_COUNT        ( 2u )
#define DIAL_SCREW_FACE_COLOR   ( 0xA514u )
#define DIAL_SCREW_SLOT_COLOR   ( 0x4208u )
#define DIAL_SLOT_SIN_COS_MILLI ( 707 )

/* The title of a full sweep face (TURBO) sits higher than the units, from
 * the center to its top edge. */
#define DIAL_TITLE_Y UI_LEN( 384 )

/* Where the positive unit sits on a full sweep face, from the center. A dial
 * with one unit has it right of center, a little above the middle. A dial
 * with a unit for each side (boost: psi and inHg) has it lower, 67 px right
 * and 20 px below center at 240 px. The negative unit sits beside the lowest
 * number, this far from it. */
#define DIAL_UNIT_X       UI_LEN( 435 )
#define DIAL_UNIT_UP      UI_LEN( 42 )
#define DIAL_PAIR_X       UI_LEN( 560 )
#define DIAL_PAIR_DOWN    UI_LEN( 168 )
#define DIAL_NEG_UNIT_GAP UI_LEN( 33 )

/* The label and units around the center use this text size. */
#define DIAL_FIXED_SCALE UI_TEXT_SCALE( 1 )

/* Tick numbers are drawn at this text size, chosen for the 240 px panel. */
#define DIAL_LABEL_SCALE UI_TEXT_SCALE( 2 )

/* Where the tick numbers sit on the round dials, from the center: all on one
 * circle. On the arc style each number sits as close inside its ticks as it
 * can without touching, which depends on how wide it is and which way it
 * faces (see add_number). */
#define DIAL_LABEL_R UI_LEN( 584 )

/* Per-frame redraw covers only the needle's old and new position, not the
 * whole face (ticks/labels/face don't change between frames): one blit
 * instead of one per row, and only the pixels near the needle. This bounds
 * the biggest side that box can be; a jump that would exceed it (should
 * never happen at any sane top speed) falls back to a full draw_frame.
 * DIAL_TAIL_R is the counterweight's reach behind center. */
#define DIAL_REGION_PERMILLE ( 1150 )
#define DIAL_REGION_MAX      UI_LEN( DIAL_REGION_PERMILLE )
#define DIAL_REGION_PIXELS                                                     \
    ( UI_LEN_U( DIAL_REGION_PERMILLE ) * UI_LEN_U( DIAL_REGION_PERMILLE ) )
/* The box must cover the widest line the needle shape draws, plus one
 * pixel of rounding slack. A smaller margin leaves stale pixels of the old
 * needle behind (test_dial_region checks this). */
#define DIAL_HALF_W_MAX                                                        \
    ( ( DIAL_NEEDLE_HALF_W > DIAL_TAIL_HALF_W ) ? DIAL_NEEDLE_HALF_W           \
                                                : DIAL_TAIL_HALF_W )
#define DIAL_REGION_MARGIN ( DIAL_HALF_W_MAX + 1 )
#define DIAL_TAIL_R        UI_LEN( 259 )

static inline bool dial_is_arc( const dial_cfg_t * cfg )
{
    return cfg->style == DIAL_STYLE_UPPER_ARC;
}

/* How far below the middle of the panel the needle pivots and the ticks are
 * centered: 0 on the round dials. */
static inline int32_t dial_pivot_down( const dial_cfg_t * cfg )
{
    return dial_is_arc( cfg ) ? ( int32_t )DIAL_ARC_PIVOT_DOWN : 0;
}

/* The radius the ticks' outer ends are on, from the pivot. */
static inline int32_t dial_tick_radius( const dial_cfg_t * cfg )
{
    return dial_is_arc( cfg ) ? ( int32_t )DIAL_ARC_TICK_R
                              : ( int32_t )DIAL_TICK_R;
}

/* The needle's reach from the pivot: to the middle of a long tick. */
static inline int32_t dial_needle_length( const dial_cfg_t * cfg )
{
    return dial_tick_radius( cfg ) - ( DIAL_TICK_MAJOR_LEN / 2 );
}

/* A pixel a tick touches is in this ring around the pivot: no nearer than a
 * long tick's inner end (less a pixel for its soft edge) and no further than
 * the outer end plus a long tick's half-width (and a pixel). Pixels inside or
 * outside it can skip every tick. */
static inline int32_t dial_tick_ring_in( const dial_cfg_t * cfg )
{
    return dial_tick_radius( cfg ) - DIAL_TICK_MAJOR_LEN - 1;
}

static inline int32_t dial_tick_ring_out( const dial_cfg_t * cfg )
{
    return dial_tick_radius( cfg ) + DIAL_TICK_MAJOR_HALF_W + 1;
}

#endif /* DIAL_LAYOUT_H */
