/* One line of text for a human at a terminal (e.g. a factory bench tool
 * reading the console). Not for anything the app decides from -- that is
 * hal_uart/hal_ble's job. Deliberately not printf-style: a formatting API
 * would pull in <stdarg.h>, which the other hal_* headers avoid, and
 * every caller today only ever has a fixed, known message anyway. */
#ifndef HAL_LOG_H
#define HAL_LOG_H

/* Blocking, best-effort: nothing to report if there is no console. A
 * trailing newline is added; text should not include one. */
void hal_log_write( const char * text );

#endif /* HAL_LOG_H */
