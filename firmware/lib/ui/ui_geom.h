/* Where things sit on the panel, as fractions of its size, so a panel of
 * another size only changes DISPLAY_WIDTH and DISPLAY_HEIGHT (see display.h).
 * Lengths are given in thousandths of the radius. Text and line
 * thicknesses are whole pixels and do not scale with these. */
#ifndef UI_GEOM_H
#define UI_GEOM_H

#include "lib/display/display.h"
#include "lib/ui/text.h"

/* The panel is round: the center and radius follow from its width. */
#define UI_CX     ( ( ( int )DISPLAY_WIDTH ) / 2 )
#define UI_CY     ( ( ( int )DISPLAY_HEIGHT ) / 2 )
#define UI_RADIUS ( ( ( int )DISPLAY_WIDTH ) / 2 )

/* Pixels for a length given in thousandths of the radius, rounded down.
 * permille is a plain int constant. UI_LEN_U is the same as an unsigned
 * value, for array sizes. */
#define UI_LEN( permille ) ( ( UI_RADIUS * ( permille ) ) / 1000 )
#define UI_LEN_U( permille )                                                   \
    ( ( ( DISPLAY_WIDTH / 2u ) * ( unsigned )( permille ) ) / 1000u )

/* Text is drawn at whole-number scales of the 5x7 font, so it follows the
 * panel in steps. UI_TEXT_SCALE(n) takes a text size chosen for the 240 px
 * panel and scales it in proportion to the panel width, rounded to the
 * nearest whole number. Each size is scaled on its own, so a text that
 * filled most of the panel still fits after scaling. n is a plain int
 * constant. */
#define UI_TEXT_SCALE( n ) ( ( ( ( n ) * ( int )DISPLAY_WIDTH ) + 120 ) / 240 )

/* A text widget's rect: room for this many glyphs at this scale, centered
 * across the panel, with its top edge at y. chars and scale are plain int
 * constants. The values are non-negative and fit the rect's fields. */
#define UI_CENTERED_RECT( chars, scale, y )                                    \
    { UI_CX - ( TEXT_RECT_W( chars, scale ) / 2 ),                             \
      ( y ),                                                                   \
      TEXT_RECT_W( chars, scale ),                                             \
      TEXT_RECT_H( scale ) }

/* A text widget centered across the panel like UI_CENTERED_RECT: its rect,
 * text size, text callback and alignment, with no colors yet (the page sets
 * them for its theme) and nothing drawn. Every widget of a page is written
 * with this, so a new field of text_widget_t is added in one place. */
#define UI_CENTERED_WIDGET( chars_, scale_, y_, text_fn_, align_ )             \
    { .rect  = UI_CENTERED_RECT( chars_, scale_, y_ ),                         \
      .scale = ( scale_ ),                                                     \
      .fg    = 0u,                                                             \
      .bg    = 0u,                                                             \
      .text  = ( text_fn_ ),                                                   \
      .cache = { 0 },                                                          \
      .align = ( align_ ) }

#endif /* UI_GEOM_H */
