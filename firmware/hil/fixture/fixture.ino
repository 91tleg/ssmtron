/* Bench fixture firmware (ESP32): one sketch, flashed once, serving two
 * roles at once on independent peripherals so neither test suite needs
 * to reflash the fixture to run the other:
 *
 *   - Serial  (UART0, the USB/programming link to the test host) carries
 *     the button-actuator command protocol; see button_actuator.h.
 *   - Serial1 (a second hardware UART, routed via the pin matrix to the
 *     DK's ECU UART1 pins) runs the fake-ECU SSM1 responder continuously
 *     in the background; see ecu_sim.h. It needs no commands: it just
 *     answers whatever the DUT sends, the way a real ECU would.
 *
 * Wiring:
 *   ECU_TX_PIN -> DK P1.01 (UART1 RX)
 *   ECU_RX_PIN -> DK P1.02 (UART1 TX)
 *   ACTUATOR_PIN -> MOSFET gate -> DK P0.12 (ACTION), MOSFET source -> GND
 *   Common ground between the ESP32 and the DK.
 */
#include "button_actuator.h"
#include "ecu_sim.h"

#define ECU_RX_PIN ( 16 )
#define ECU_TX_PIN ( 17 )

void setup()
{
    Serial.begin( 115200 );
    button_actuator_init();
    ecu_sim_init( ECU_RX_PIN, ECU_TX_PIN );
}

void loop()
{
    button_actuator_poll();
    ecu_sim_poll();
}
