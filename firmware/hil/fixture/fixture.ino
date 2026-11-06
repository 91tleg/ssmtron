/* Bench fixture firmware (Arduino Mega 2560): one sketch, flashed once,
 * serving two roles at once on independent peripherals so neither test
 * suite needs to reflash the fixture to run the other:
 *
 *   - Serial  (UART0, the USB/programming link to the test host) carries
 *     the button-actuator command protocol; see button_actuator.h.
 *   - Serial1 (the Mega's second hardware UART, pins 18 and 19, wired to
 *     the DK's ECU UART1 pins through a level shifter) runs the fake-ECU
 *     SSM1 responder continuously in the background; see ecu_sim.h. It
 *     needs no commands: it just answers whatever the DUT sends, the
 *     way a real ECU would.
 *
 * Wiring:
 *   Mega pin 18 (TX1) -> shifter B2 -> A2 -> DK P1.01 (UART1 RX)
 *   Mega pin 19 (RX1) <- shifter B1 <- A1 <- DK P1.02 (UART1 TX)
 *   ACTUATOR_PIN -> MOSFET gate -> DK P0.12 (ACTION), MOSFET source -> GND
 *   Common ground between the Mega, the shifter and the DK.
 */
#include "button_actuator.h"
#include "ecu_sim.h"

void setup()
{
    Serial.begin( 115200 );
    button_actuator_init();
    ecu_sim_init();
}

void loop()
{
    button_actuator_poll();
    ecu_sim_poll();
}
