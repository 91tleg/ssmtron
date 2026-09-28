/* The gauge screen: the one place that knows both the ui widget engine
 * and what an ECU value actually is (matches ecu_io.c, the one place
 * that knows both hal_uart and the protocol). To change what the panel
 * shows, edit the widget table in gauge.c -- this header never changes
 * for that. */
#ifndef GAUGE_H
#define GAUGE_H

#include "display.h"

/* Starts the panel on the given pins and draws the initial screen. */
hal_status_t gauge_init( const display_pins_t * pins );

/* Call every loop: re-checks the ECU value and link state and redraws
 * only the widgets whose text actually changed (see ui.h). */
void gauge_poll( void );

#endif /* GAUGE_H */
