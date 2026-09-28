/* The gauge screen: the one place that knows the ui widget engine, the
 * ECU, and the two physical buttons (matches ecu_io.c, the one place
 * that knows both hal_uart and the protocol). To change what the panel
 * shows, edit the widget table in gauge.c -- this header never changes
 * for that. */
#ifndef GAUGE_H
#define GAUGE_H

#include "display.h"
#include "hal_gpio.h"

/* Starts the panel on the given pins and draws the initial screen.
 * next_btn cycles the shown parameter; action_btn, held, clears trouble
 * codes (see gauge.c for the exact behavior of each). */
hal_status_t gauge_init(
    const display_pins_t * pins,
    hal_gpio_id_t next_btn,
    hal_gpio_id_t action_btn
);

/* Call every loop: polls both buttons and re-checks the ECU value and
 * link state, redrawing only the widgets whose text actually changed
 * (see ui.h). */
void gauge_poll( uint32_t now_ms );

#endif /* GAUGE_H */
