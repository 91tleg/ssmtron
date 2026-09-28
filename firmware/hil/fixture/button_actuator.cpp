/* Drives the DK's ACTION button (P0.12, GPIO_ACTIVE_LOW | GPIO_PULL_UP --
 * see board.overlay) through a MOSFET to GND, replicating the mechanical
 * switch instead of driving a voltage into the DK's rail. Talks a
 * one-line text protocol over USB serial so firmware/hil/conftest.py can
 * trigger presses without a human.
 *
 * Commands (newline-terminated, case-insensitive), each replying "OK" or
 * "ERR <reason>":
 *   PRESS          drive the pin low (button held down)
 *   RELEASE        let the pin float back high (button released)
 *   TAP            press then release after TAP_MS
 *   HOLD <ms>      press, wait <ms>, release
 */
#include "button_actuator.h"

#include <Arduino.h>

static const int ACTUATOR_PIN = 5; // any free GPIO wired to the MOSFET gate
static const unsigned long TAP_MS = 50;

static void press()
{
    digitalWrite( ACTUATOR_PIN, HIGH );
}

static void release()
{
    digitalWrite( ACTUATOR_PIN, LOW );
}

void button_actuator_init()
{
    pinMode( ACTUATOR_PIN, OUTPUT );
    release();
}

void button_actuator_poll()
{
    if ( !Serial.available() )
    {
        return;
    }

    String line = Serial.readStringUntil( '\n' );
    line.trim();
    line.toUpperCase();

    if ( line == "PRESS" )
    {
        press();
        Serial.println( "OK" );
    }
    else if ( line == "RELEASE" )
    {
        release();
        Serial.println( "OK" );
    }
    else if ( line == "TAP" )
    {
        press();
        delay( TAP_MS );
        release();
        Serial.println( "OK" );
    }
    else if ( line.startsWith( "HOLD " ) )
    {
        long ms = line.substring( 5 ).toInt();

        if ( ms <= 0 )
        {
            Serial.println( "ERR bad duration" );
        }
        else
        {
            press();
            delay( ( unsigned long )ms );
            release();
            Serial.println( "OK" );
        }
    }
    else
    {
        Serial.println( "ERR unknown command" );
    }
}
