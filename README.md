# SSMTron

SSMTron is an nRF52840-based gauge for Subaru ECUs that use the SSM1 protocol. It reads ECU data over a serial link, identifies the ECU by ROM ID, decodes supported parameters, and shows the selected value on a round GC9A01 display. The firmware also supports Bluetooth firmware updates over the air (FUOTA) using MCUboot and mcumgr; Bluetooth stays off until you start update mode.

## Controls

- **NEXT (Button 1):** Tap to cycle through the available ECU parameters, then an Info page that shows the firmware version, the ECU's ROM ID (dashes until the ECU answers) and whether the ECU is connected. Hold for 1.5 seconds to save the shown parameter and the theme as the power-on defaults; the display shows SAVED, or SAVE FAILED if the write fails. Tapping only browses and does not write flash.
- **ACTION (Button 2):** Tap to switch between the light and dark theme. The theme is kept until the next restart unless you save it with a NEXT hold. On an Active Codes or Stored Codes page, hold for two seconds to clear trouble codes; the clear prompt appears after half a second. On the Info page, hold for two seconds to start update mode (see below). Holding on any other page does nothing. After a clear result appears, release and press again to return to the gauge.

At power-on, the needle of the first dial sweeps to full scale and settles on the reading. The display shows the parameter name, decoded value and unit; dashes stand in for a value the ECU has not sent. Switch and trouble-code bytes get a page of their own that lists every flag name with a 1 (asserted) or 0, or - while the byte is stale. Available parameters depend on the detected ECU profile. The current profiles target 1990-94 Legacy ECUs; their addresses and conversions still need verification on a real car.

## Hardware and firmware

The target is the nRF52840 DK. The ECU link uses UART1 through a suitable level shifter; do not connect the car-side signal directly to a 3.3 V pin. The display uses SPI2, and the two controls use the DK's onboard buttons. Pin assignments are in [`firmware/bsp/nrf52840dk/board.overlay`](firmware/bsp/nrf52840dk/board.overlay).

The SSM1 session, ECU profiles, and decoders live in `firmware/lib/ssm1/`. `firmware/app/` connects them to the display and buttons, while `firmware/hal/nrf52/` provides the board-facing drivers. Bluetooth is used only for authenticated firmware updates and is off until update mode starts; the gauge does not stream ECU values over Bluetooth. Update mode uses a new random passkey each time and keeps no pairing, so `firmware/provision/`, which stored a fixed per-unit passkey, is no longer needed.

## Updating the firmware

Bluetooth is off at power-on. To update a unit:

1. Tap NEXT until the Info page shows, then hold ACTION for two seconds. The gauge shows UPDATE MODE with a six-digit passkey.
2. On a phone, open Nordic's nRF Connect Device Manager, connect to "SSM1 Gauge", and enter the passkey when it asks.
3. Choose the signed update image from the build (usually `app_update.bin` in the build output) and upload it. The gauge restarts into the new firmware, and the Info page shows the new version.

The wait for a phone lasts five minutes and is paused while a phone is connected. Tap NEXT, or let it run out, to leave update mode; either restarts the gauge and switches Bluetooth off again.

## Build and test

Build with the Nordic Connect SDK v3.2.4 toolchain from its workspace:

```sh
west build -b nrf52840dk/nrf52840 /path/to/ssmtron/firmware/app \
  -d /path/to/ssmtron/firmware/build --pristine
```

The application builds MCUboot and a signed image through Zephyr sysbuild. Host tests need CMake and a C compiler:

```sh
cmake -S firmware/test -B firmware/build-test
cmake --build firmware/build-test
ctest --test-dir firmware/build-test --output-on-failure
```
