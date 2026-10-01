# SSMTron

SSMTron is an nRF52840-based gauge for Subaru ECUs that use the SSM1 protocol. It reads ECU data over a serial link, identifies the ECU by ROM ID, decodes supported parameters, and shows the selected value on a round GC9A01 display. The firmware also supports Bluetooth firmware updates over the air (FUOTA) using MCUboot and mcumgr; Bluetooth stays off until you start update mode.

## Controls

- **NEXT (Button 1):** Tap to cycle through the available ECU parameters, then an Info page that shows the firmware version, the ECU's ROM ID (dashes until the ECU answers) and whether the ECU is connected. Hold for 1.5 seconds to save the shown parameter and the theme as the power-on defaults; the display shows SAVED, or SAVE FAILED if the write fails. Tapping only browses and does not write flash.
- **ACTION (Button 2):** Tap to switch between the light and dark theme. The theme is kept until the next restart unless you save it with a NEXT hold. On an Active Codes or Stored Codes page, hold for two seconds to clear trouble codes; the clear prompt appears after half a second. On the Info page, hold for two seconds to start update mode (see below). Holding on any other page does nothing. After a clear result appears, release and press again to return to the gauge.

At power-on, the needle of the first dial sweeps to full scale, down to the bottom of the scale and back to rest, and then settles on the reading. The display shows the parameter name, decoded value and unit; dashes stand in for a value the ECU has not sent. Switch and trouble-code bytes get a page of their own that lists every flag name with a 1 (asserted) or 0, or - while the byte is stale. Available parameters depend on the detected ECU profile. The current profiles target 1990-94 Legacy ECUs; their addresses and conversions still need verification on a real car.

## Hardware and firmware

The target is the nRF52840 DK. The ECU link uses UART1 through a suitable level shifter; do not connect the car-side signal directly to a 3.3 V pin. The display uses SPI2, and the two controls use the DK's onboard buttons. Pin assignments are in [`firmware/bsp/nrf52840dk/board.overlay`](firmware/bsp/nrf52840dk/board.overlay).

Bluetooth is used only for authenticated firmware updates and is off until update mode starts; the gauge does not stream ECU values over Bluetooth. Update mode uses a new random passkey each time and keeps no pairing.

## Code map

Start at `firmware/app/main.c` (startup and the main loop), then `firmware/app/gauge.c` (which page shows and what the buttons do), then `firmware/lib/ecu/ecu.h` (what the gauge asks of an ECU). Each layer uses only the ones below it.

```
app/    what the gauge does: pages, buttons, themes, saved settings, update mode
lib/    code with no hardware or app knowledge, one job per folder
hal/    the chip interface (headers) and its nrf52/ drivers
bsp/    the board's pins (devicetree overlay and bsp.h)
test/   host tests, run with ctest
hil/    tests on real hardware, which need a fixture
```

| Folder | What it does |
| --- | --- |
| `lib/ecu` | The ECU interface: what the rest of the firmware can ask for: pages, values, clearing trouble codes. |
| `lib/ssm1` | The Subaru SSM1 implementation of that interface: the session, the ECU profiles and their parameters, and the decoders. |
| `lib/fmt` | Turns ECU values and log lines into text. |
| `lib/textfit` | Cuts, pads and centers text to a width. No dependencies. |
| `lib/display` | The panel interface, with the GC9A01 driver in `gc9a01/`. |
| `lib/ui` | What gets drawn: text widgets, the font, the analog dial (`dial`) and how its needle moves (`dial_motion`). |

| App file | What it does |
| --- | --- |
| `main.c` | Startup order and the loop. |
| `gauge.c` | Picks which page or dial shows, turns button events into actions, applies the theme. |
| `gauge_input.c` | Debounces the two buttons and recognizes taps and holds. |
| `gauge_number.c`, `gauge_flags.c`, `gauge_info.c`, `gauge_update.c` | The text pages: the number page, the flags page, the Info page and the update page. Each owns its own text and layout. |
| `gauge_sweep.c` | The needle sweep at power-on. |
| `gauge_settings.c` | What is saved: page, theme and ECU profile. |
| `ecu_io.c` | Connects the UART to the ECU session. |
| `fw_info.c` | The firmware version. |
| `ble_log.c` | Streams values over Bluetooth. Kept for now, but `main.c` does not call it. |
| `hil_test.c` | A hang hook that only hardware tests build in. |

## How the ECU link works

The link is 1953 baud, 8 data bits, even parity. Every command the gauge sends is 4 bytes. The ECU answers in frames of 3 bytes, `[address high][address low][data]`, with no start marker or checksum.

**One request streams.** The ECU has no request-per-reading mode. Once the gauge sends `78 <addr high> <addr low> 00` (read), the ECU keeps sending that one address's frames, about 60 a second at this baud rate, until it is told to stop. The gauge reads one address at a time, so it does not poll.

**Changing what is read.** To switch to another parameter, the gauge sends `12 00 00 00` (stop), waits for 80 ms of silence on the line, then sends a new read. The stop and the wait are a cautious choice, so the old stream's bytes cannot be mistaken for the new one. Whether the ECU would accept a new read without them has not been tested on a car. Every page change (and every retry after a failure) goes through this cycle:

```
stop -> wait for silence -> read -> first confirmed sample -> streaming
```

**Frames are found by sliding.** Replies carry no marker, so the gauge slides a 3 byte window over the incoming bytes looking for the address it asked for. A sample is reported only after the next frame confirms the alignment, which adds one frame (about 17 ms) of delay. The first sample after a command is dropped, because the line can echo the command and the echo can look like a frame. A parity or framing error drops the partial frame and needs a new confirmation.

**Identifying the ECU.** After power-up the gauge reads address `0xFFFE` and sends `00 FF FF 00`. The ECU answers `ff fe xx r0 r1 r2`, and the ID is accepted only after it is seen twice. The profile comes from `r0 | r1 << 8` (the low 16 bits), and the first page of that profile is then read. Until the ECU answers, the gauge shows the profile it remembers, so a dial appears at once.

**Failures and link state.** A cycle that gets no confirmed sample within 300 ms, or a stream that goes quiet for 500 ms, restarts from the stop. Three failures in a row pause for one second before trying again. A value older than 500 ms is shown as dashes, and the link counts as up while samples are under one second old.

**Clearing trouble codes.** A clear writes to the code address with `aa <addr high> <addr low> <data>`, which only takes effect while the ECU is streaming. The gauge first reads the byte, then writes it four times, 384 ms apart, and waits about 22 seconds after the last write for the ECU's memory write to finish. Reads resume after that, and the gauge gives up if it takes more than 30 seconds. The data written is either `0xFF`, or the inverse of the byte it read, depending on the profile.

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
