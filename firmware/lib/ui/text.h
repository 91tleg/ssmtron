/* Widget engine over display.h. Each widget has a rectangle and text
 * callback. Only changed text triggers a redraw of that rectangle. */
#ifndef TEXT_H
#define TEXT_H

#include <stddef.h>
#include <stdint.h>

#include "lib/ui/font.h"

/* Longest string any widget may produce, not counting the NUL. A widget
 * needing more should be given its own wider rect, not a longer cache. */
#define TEXT_MAX 24u

typedef struct
{
    uint16_t x;
    uint16_t y;
    uint16_t w;
    uint16_t h; /* must be FONT_CELL_H * scale; see font.h */
} text_rect_t;

/* The size of a rect that holds this many glyphs at this scale, so a
 * widget's rect follows its text size instead of a hand-counted number.
 * chars and scale are plain int constants. */
#define TEXT_RECT_W( chars, scale )                                            \
    ( ( chars ) * ( int )FONT_CELL_W * ( scale ) )
#define TEXT_RECT_H( scale ) ( ( int )FONT_CELL_H * ( scale ) )

typedef enum
{
    TEXT_ALIGN_LEFT = 0, /* the default, so widgets may leave it out */
    TEXT_ALIGN_CENTER    /* centered in the rect, to the pixel */
} text_align_t;

typedef struct
{
    /* Writes this widget's current text into buf (size TEXT_MAX + 1),
     * NUL-terminated. Called every text_poll: keep it cheap and pure. */
    void ( *text )( char * buf );
    text_align_t align; /* where the text sits in the rect */
    text_rect_t rect;
    uint16_t fg;   /* RGB565 */
    uint16_t bg;   /* RGB565; also what erases now-shorter stale text */
    uint8_t scale; /* 1 = 6x7 px per character, 2 = 12x14, ... */
    char cache[TEXT_MAX + 1u]; /* last text actually drawn */
} text_widget_t;

/* Clears the whole panel to page_bg, then draws every widget once,
 * regardless of its cache (there is nothing to compare against yet). */
void text_init(
    text_widget_t * widgets,
    size_t count,
    uint16_t page_bg
);

/* Re-reads every widget's text and redraws (blits) only the ones whose
 * text changed since the last call. */
void text_poll(
    text_widget_t * widgets,
    size_t count
);

#endif /* TEXT_H */
