# SSMTron

SSMTron is an nRF52840-based gauge for Subaru ECUs that use the SSM1 protocol. It reads ECU data over a serial link, identifies the ECU by ROM ID, decodes supported parameters, and shows the selected value on a round GC9A01 display. The firmware also supports Bluetooth firmware updates over the air (FUOTA) using MCUboot and mcumgr; Bluetooth stays off until you start update mode.

## Controls

- **NEXT (Button 1):** Tap to cycle through the available ECU parameters, then an Info page that shows the firmware version, the ECU's ROM ID (dashes until the ECU answers) and whether the ECU is connected. Hold for 1.5 seconds to save the shown parameter and the theme as the power-on defaults; the display shows SAVED, or SAVE FAILED if the write fails. Tapping only browses and does not write flash.
- **ACTION (Button 2):** Tap to switch between the light and dark theme. The theme is kept until the next restart unless you save it with a NEXT hold. On an Active Codes or Stored Codes page, hold for two seconds to clear trouble codes; the clear prompt appears after half a second. On the Info page, hold for two seconds to start update mode (see below). Holding on any other page does nothing. After a clear result appears, release and press again to return to the gauge.

At power-on, the needle of the first dial sweeps to full scale, down to the bottom of the scale and back to rest, and then settles on the reading. The display shows the parameter name, decoded value and unit; dashes stand in for a value the ECU has not sent. Switch and trouble-code bytes get a page of their own that lists every flag name with a 1 (asserted) or 0, or - while the byte is stale. Available parameters depend on the detected ECU profile. The current profiles target 1990-94 Legacy ECUs; their addresses and conversions still need verification on a real car.

## Hardware and firmware

The target is the nRF52840 DK. The ECU link uses UART1 through a suitable level shifter; do not connect the car-side signal directly to a 3.3 V pin. The display uses SPI2, and the two controls use the DK's onboard buttons. Pin assignments are in [`firmware/bsp/nrf52840dk/board.overlay`](firmware/bsp/nrf52840dk/board.overlay).

Bluetooth is used only for authenticated firmware updates and is off until update mode starts; the gauge does not stream ECU values over Bluetooth. Update mode uses a new random passkey each time and keeps no pairing.

## Repository layout

Start at `firmware/app/main.c` (startup and the main loop), then `firmware/app/gauge.c` (which page shows and what the buttons do), then `firmware/lib/ecu/ecu.h` (what the gauge asks of an ECU). Each layer uses only the ones below it: `app` uses `lib` and `hal`, `lib` uses nothing but the `hal` headers it is given, and `hal` is the only code that touches the chip. Most `.c` files have a `.h` beside them that says what they offer, and each folder has a `CMakeLists.txt` that lists what it builds.

```
.github/workflows/ci.yml        CI: clang-format, host tests, device build and the MISRA scan
AGENTS.md                       how to work on this code: reviews, tests, commits
README.md                       this file

firmware/
  .clang-format                 the code style CI checks

  app/                          what the gauge does: pages, buttons, themes, saved settings, update mode
    main.c                      startup order and the main loop
    gauge.c                     picks which page or dial shows, turns button events into actions, applies the theme
    gauge_input.c               debounces the two buttons and recognizes taps and holds
    gauge_number.c              the plain number page
    gauge_flags.c               the page for a switch or trouble-code byte: every flag name with 1 or 0
    gauge_info.c                the Info page: version, ROM ID and whether the ECU is connected
    gauge_update.c              the update-mode page: passkey, status and time left
    gauge_sweep.c               the needle sweep at power-on
    gauge_settings.c            what is saved: page, theme and ECU profile
    ecu_io.c                    connects the UART to the ECU session
    fw_info.c                   the firmware version
    ble_log.c                   streams values over Bluetooth (kept for now, main.c does not call it)
    hil_test.c                  a hang hook for the hardware test rig, built only with hil/prj_hil.conf
    prj.conf, sysbuild.conf     Zephyr settings (Bluetooth, saved settings, MCUboot) and the MCUboot build
    Kconfig, VERSION            the hardware-test option, and the firmware version

  lib/                          code with no hardware or app knowledge, one job per folder
    ecu/ecu.h                   the ECU interface: pages, values, clearing trouble codes
    ssm1/                       the Subaru SSM1 implementation of that interface
      ssm1.c                    the session: handshake, one read at a time, stall recovery, clear
      ssm1_cmd.c                encodes the 4-byte commands
      ssm1_rx.c                 finds frames in the reply bytes with a sliding window
      ssm1_romid.c              parses the ROM ID reply
      ssm1_params.c             the ECU profiles and the address and decoder of each parameter
      ssm1_decode.c             raw byte to shown value
    fmt/fmt.c                   turns ECU values and log lines into text
    textfit/textfit.c           cuts, pads and centers text to a width, and prints numbers the way a gauge face does (".2")
    display/
      display.h                 the panel interface
      gc9a01/gc9a01.c           the GC9A01 round display driver
    ui/                         what gets drawn
      ui_geom.h                 where things sit on a panel of any size, as parts of its radius
      font.c, font_b612.c       the font: B612 Mono bitmaps made by tools/gen_font.py
      text.c                    text widgets that redraw only what changed
      dial.c                    the analog dial: draws it a pixel at a time, redraws the needle, opens and polls it
      dial_layout.h             the dial's measurements, shared by the files below
      dial_angle.c              where a reading points: the angle, and its sine and cosine
      dial_face.c               works out the ticks, numbers, title and units once, when the dial opens
      dial_shape.c              how much of a pixel a bar, a disc or the needle covers
      dial_cover.c              the screws, and the arc style's cover plate with its shadow
      dial_motion.c             how the needle moves: damped, speed-limited, in frames

  hal/                          the chip interface
    hal_*.h                     one header per service: gpio, uart, spi, time, nvm, ble, dfu, wdt, random, log
    nrf52/                      the nRF52840 drivers behind those headers
      uarte_baud.h              baud register math, so 1953 baud can be set

  bsp/                          the board's pins
    nrf52840dk/board.overlay    devicetree: UART1 for the ECU, SPI2 for the display, the two buttons
    nrf52840dk/bsp.h            the pin and device ids the app uses

  test/                         host tests, run with ctest
    minitest.h                  a tiny test harness
    test_*.c                    one test file per module, with fakes for what is below it
    sil/                        whole-gauge scenarios: the real gauge and ECU session against a simulated ECU

  hil/                          tests on real hardware, which need a bench fixture
    test_*.py, conftest.py      pytest tests: watchdog recovery, button hold, Bluetooth update
    fixture/                    the ESP32 sketch that plays the ECU and presses the button
    prj_hil.conf                the settings that add the hang hook (never merged into prj.conf)

tools/                          helpers that run on a computer, not on the gauge
  gen_font.py                   makes lib/ui/font_b612.c from the font in fonts/
  gen_logo.py                   makes a logo table from logos/ (the logo is not drawn at the moment)
  preview_ui.c, preview_ui.py   draw the real UI code to PNG files, with no device
  animate_ui.py                 animates the needle's motion to GIFs and CSVs
  update_editor_commands.py     merges the build databases for VS Code IntelliSense
  CMakeLists.txt, requirements.txt   build the preview program; the Python packages the tools need
```

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

**Clearing trouble codes.** A clear writes to the code address with `aa <addr high> <addr low> <data>`, which only takes effect while the ECU is streaming. The gauge first reads the byte, then writes it four times, 384 ms apart, and waits about 22 seconds after the last write for the ECU's memory write to finish. The gauge sends nothing during that wait, even if the ECU goes quiet. Reads then resume, the clear counts as done when the ECU answers again, and the gauge gives up if that takes more than 30 seconds from the start. The data written is either `0xFF`, or the inverse of the byte it read, depending on the profile.

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
