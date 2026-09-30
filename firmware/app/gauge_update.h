/* Update mode: Bluetooth is off until the user asks for it. Then the gauge
 * starts it with a fresh random passkey shown on screen, waits for a phone
 * to pair and upload a firmware image, and gives up after a while. Leaving
 * update mode restarts the device, which is also how the radio goes off
 * again. gauge.c decides when it starts; this file owns its state and its
 * page. */
#ifndef GAUGE_UPDATE_H
#define GAUGE_UPDATE_H

#include <stdbool.h>
#include <stdint.h>

/* True from gauge_update_start until the device restarts. */
bool gauge_update_active( void );

/* Starts Bluetooth with a new random passkey and begins the countdown. Does
 * nothing if update mode is already on. If Bluetooth cannot start, update
 * mode still opens and its page says so. */
void gauge_update_start( uint32_t now_ms );

/* Leaves update mode by restarting the device. */
void gauge_update_exit( void );

/* Sets the page colors; takes effect at the next gauge_update_open. */
void gauge_update_set_colors(
    uint16_t face,
    uint16_t ink
);

/* Clears the panel to face and draws the whole page. */
void gauge_update_open( uint16_t face );

/* Call every poll while active: follows the phone connecting and leaving,
 * restarts the device when the wait runs out, and redraws what changed. The
 * wait only counts down while no phone is connected, so an upload that is
 * under way is never cut off. */
void gauge_update_poll( uint32_t now_ms );

#endif /* GAUGE_UPDATE_H */
