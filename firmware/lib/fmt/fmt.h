/* Text for ECU values, shared by the log stream and the display. Pure: no
 * IO, no libc formatting, never writes past the buffer. All functions
 * return the text length (without the terminating NUL), or 0 with an empty
 * string in buf if it does not fit. */
#ifndef FMT_H
#define FMT_H

#include <stddef.h>
#include <stdint.h>

#include "ecu.h"

/* "-12.34" for a value with decimals = 2; "0xA5" for a flags byte. No
 * unit. */
size_t fmt_value(
    char * buf,
    size_t max,
    const ecu_param_info_t * info,
    int32_t value
);

/* One CSV log line: "<now_ms>,<name>,<value>,<unit>\n" */
size_t fmt_log_line(
    char * buf,
    size_t max,
    uint32_t now_ms,
    const ecu_param_info_t * info,
    int32_t value
);

#endif /* FMT_H */
