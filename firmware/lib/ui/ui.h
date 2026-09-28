/* A tiny, hardware-neutral widget engine over display.h. Each widget owns
 * a fixed rectangle and a callback that produces its current text; a
 * widget is only re-rendered -- and only its own rectangle blitted --
 * when that text has actually changed since the last poll (the "dirty
 * bit"), so a screen that is not changing costs no SPI traffic at all.
 *
 * The widget table is the only thing that changes to change what is on
 * screen: adding, moving, resizing or restyling a field is one line in
 * that table, never a change to this engine. See firmware/app/gauge.c
 * for the actual screen. */
#ifndef UI_H
#define UI_H

#include <stddef.h>
#include <stdint.h>

/* Longest string any widget may produce, not counting the NUL. A widget
 * needing more should be given its own wider rect, not a longer cache. */
#define UI_TEXT_MAX 24u

typedef struct
{
    uint16_t x;
    uint16_t y;
    uint16_t w;
    uint16_t h; /* must be UI_FONT_H * scale; see ui_font.h */
} ui_rect_t;

typedef struct
{
    ui_rect_t rect;
    uint8_t scale; /* 1 = 5x7 px per glyph, 2 = 10x14, ... */
    uint16_t fg;   /* RGB565 */
    uint16_t bg;   /* RGB565; also what erases now-shorter stale text */
    /* Writes this widget's current text into buf (size UI_TEXT_MAX + 1),
     * NUL-terminated. Called every ui_poll: keep it cheap and pure. */
    void ( *text )( char * buf );
    char cache[UI_TEXT_MAX + 1u]; /* last text actually drawn */
} ui_widget_t;

/* Clears the whole panel to page_bg, then draws every widget once,
 * regardless of its cache (there is nothing to compare against yet). */
void ui_init(
    ui_widget_t * widgets,
    size_t count,
    uint16_t page_bg
);

/* Re-reads every widget's text and redraws (blits) only the ones whose
 * text changed since the last call. */
void ui_poll(
    ui_widget_t * widgets,
    size_t count
);

#endif /* UI_H */
