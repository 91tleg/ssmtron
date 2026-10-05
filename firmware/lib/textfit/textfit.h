/* Bounded text builders for widget and page text. Pure: no IO, and they never
 * write past the buffer. max is the buffer size including the terminating
 * NUL, so at most max - 1 characters are written and text that does not fit
 * is cut. Each function returns the text length without the NUL. With max of
 * 0 nothing is written and 0 is returned. */
#ifndef TEXTFIT_H
#define TEXTFIT_H

#include <stddef.h>
#include <stdint.h>

/* Copies s. */
size_t textfit_copy(
    char * buf,
    size_t max,
    const char * s
);

/* Writes v in decimal, with a leading '-' when negative. Cut like the others
 * if it does not fit. */
size_t textfit_int(
    char * buf,
    size_t max,
    int32_t v
);

/* Writes value / div as a decimal, the way a gauge face prints it: a whole
 * number has no point, otherwise the fraction follows with its trailing
 * zeros dropped, and below one there is no zero before the point. 20 over 100
 * is ".2", 105 over 100 is "1.05", -50 over 100 is "-.5". div is a power of
 * ten, 1 or more; anything smaller counts as 1. Cut like the others if it
 * does not fit. */
size_t textfit_scaled(
    char * buf,
    size_t max,
    int32_t value,
    int32_t div
);

/* Writes label, spaces up to label_w columns, then value. The whole row is
 * cut to row_w columns, so the value gets row_w - label_w of them. */
size_t textfit_row(
    char * buf,
    size_t max,
    size_t label_w,
    size_t row_w,
    const char * label,
    const char * value
);

/* Writes s with leading spaces that center it in width columns. Text longer
 * than width is cut to width. There are no trailing spaces. */
size_t textfit_center(
    char * buf,
    size_t max,
    const char * s,
    size_t width
);

#endif /* TEXTFIT_H */
