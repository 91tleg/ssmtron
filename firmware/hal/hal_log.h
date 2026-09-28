/* Human-readable console output. Callers provide complete text. */
#ifndef HAL_LOG_H
#define HAL_LOG_H

/* Blocking, best-effort: nothing to report if there is no console. A
 * trailing newline is added; text should not include one. */
void hal_log_write( const char * text );

#endif /* HAL_LOG_H */
