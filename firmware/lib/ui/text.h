/* Widget engine over display.h. Each widget has a rectangle and text
 * callback. Only changed text triggers a redraw of that rectangle. */
#ifndef TEXT_H
#define TEXT_H

#include <stddef.h>
#include <stdint.h>

/* Longest string any widget may produce, not counting the NUL. A widget
 * needing more should be given its own wider rect, not a longer cache. */
#define TEXT_MAX 24u

typedef struct
{
    uint16_t x;
    uint16_t y;
    uint16_t w;
    uint16_t h; /* must be FONT_H * scale; see font.h */
} text_rect_t;

typedef struct
{
    text_rect_t rect;
    uint8_t scale; /* 1 = 5x7 px per glyph, 2 = 10x14, ... */
    uint16_t fg;   /* RGB565 */
    uint16_t bg;   /* RGB565; also what erases now-shorter stale text */
    /* Writes this widget's current text into buf (size TEXT_MAX + 1),
     * NUL-terminated. Called every text_poll: keep it cheap and pure. */
    void ( *text )( char * buf );
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
