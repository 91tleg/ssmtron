# Bench HIL rig setup

Covers the four hardware-in-the-loop areas host tests can't reach:
watchdog recovery, BLE DFU end-to-end, ACTION-button hold-to-clear, and
exercising `lib/ssm1`/`ecu_io.c` against a real UART peer at real SSM1
baud — all without a car. You already have the Raspberry Pi (test host)
and the Arduino Mega 2560 (fixture controller); this covers everything else and how
it all fits together.

## Parts to add

| Part | Spec | Qty | Why |
| --- | --- | --- | --- |
| N-channel logic-level MOSFET | 2N7000 or BS170 (TO-92) | 1 | Pulls the DK's P0.12 ACTION pin to GND, replicating the mechanical switch instead of driving voltage into the DK's rail (see `fixture/button_actuator.cpp`) |
| Gate resistor | 220 Ω, 1/4 W | 1 | Limits the Mega GPIO current into the MOSFET gate |
| Gate pull-down resistor | 10 kΩ, 1/4 W | 1 | Holds the MOSFET off (button released) if the Mega GPIO floats during its own power-up/reset |
| Jumper wires, male-to-male | 0.1" (2.54 mm) pitch | ~6 | Mega `Serial1` ↔ level shifter ↔ DK P1.01/P1.02, common ground, MOSFET-to-P0.12 |
| 4-channel BSS138 level shifter | LV, HV, A1-A4, B1-B4, 2x GND | 1 | 3.3V to 5V shift on the two ECU lines (channels 1 and 2) |
| Small breadboard | Half-size, 400-point | 1 | Mounts the MOSFET + resistors cleanly |
| Jumper wire kit | M-M / M-F / F-F assortment | 1 kit | General breadboard-to-header flexibility |
| USB cable for the Mega | USB-B (match your board) | 1 | Power/program the Mega; also carries the button + ECU-sim serial link to the Pi |

A 3.3V to 5V level shifter is needed on the two ECU lines: the Mega is
5V logic and the nRF52840 DK is 3.3V. Never wire Mega TX1 straight to
the DK. The button MOSFET gate can be driven by the Mega directly.

Optional / contingency:

| Part | Spec | Qty | Why |
| --- | --- | --- | --- |
| USB Bluetooth LE dongle | BLE 4.0+ | 1 | Fallback if the Pi's onboard Bluetooth proves unreliable for the DFU test |
| Basic multimeter | Any | 1 | Verify wiring (pull-up levels, continuity) before connecting boards |

## Wiring

```
Mega 2560          level shifter                nRF52840 DK
---------          -------------                -----------
5V          -----> HV                           (3.3V -----> LV)
Pin 18 TX1  -----> B2 ... A2  ----------------> P1.01 (UART1 RX)
Pin 19 RX1  <----- B1 ... A1  <---------------- P1.02 (UART1 TX)
Pin 5 --[220R]--[MOSFET gate]
                 MOSFET drain  --  P0.12 (ACTION)
                 MOSFET source --  GND
Pin 5 --[10k pulldown]-- GND
GND  -----------  shifter GND (both pins)  ---  GND (common)
```

The shifter's LV pin goes to the DK's 3.3V pin, not to the Mega. Check
A1 and A2 read about 3.3V and B1 and B2 about 5V before connecting the
signal wires.

## One-time flashing (not per test run)

1. **DK**: build with the HIL overlay and flash once — it's safe to
   leave enabled for every suite, not just the watchdog one, since the
   hang hook only fires on the literal byte `'H'` over the console:
   ```sh
   cd /opt/nordic/ncs/v3.2.4
   nrfutil sdk-manager toolchain launch --ncs-version v3.2.4 -- \
     west build -b nrf52840dk/nrf52840 firmware/app -d firmware/build --pristine \
     -- -DEXTRA_CONF_FILE=../hil/prj_hil.conf
   west flash -d firmware/build
   ```
2. **Mega**: flash (board: Arduino Mega or Mega 2560) `firmware/hil/fixture/fixture.ino` (Arduino IDE or
   `arduino-cli`) once. Both the button actuator and the fake ECU run
   concurrently in its `loop()` — no per-suite reflashing.
3. Bond the Pi to the DK over BLE once (interactive passkey pairing;
   `mcumgr`'s authenticated pairing needs this the first time). This is
   outside any script — pair normally via `bluetoothctl` or your BLE
   stack of choice.

## Running the suites

```sh
pip install pytest pyserial

export HIL_DUT_PORT=/dev/ttyACM0          # DK's console UART
export HIL_ACTUATOR_PORT=/dev/ttyUSB0     # Mega
export HIL_BLE_CONNSTRING="peer_name=Maxtek Datum"   # default if unset
export HIL_DFU_IMAGE=/path/to/app.signed.bin       # for test_dfu.py only

pytest firmware/hil
```

Tests skip (not fail) when their required env var is unset, so this is
safe to run on a machine with no rig attached.

Each of the three env vars maps to one thing already flashed above:
`HIL_DUT_PORT` and `HIL_ACTUATOR_PORT` never change between runs once
wired up; only `HIL_DFU_IMAGE` changes, per firmware build you want to
test.

## What's simulated vs. real

- **Fake ECU** (`fixture/ecu_sim.cpp`): SSM1 protocol taken directly
  from `lib/ssm1` (`ssm1_cmd.h`, `ssm1_rx.h`, `ssm1_romid.h`), simulating
  ROM ID `0x3672` ("91-94 Turbo" in `ssm1_params.c`). Runs at 1953 baud
  (the Arduino API only accepts an integer; the real protocol is
  1953.125 — a ~0.0064% deviation, well inside UART tolerance, not a
  claim of bit-exact timing).
- **Trouble-code clear**: `lib/ssm1/ssm1.c`'s clear sequence is purely
  time/write-count based (4 writes, 384 ms apart) once identification
  succeeds — the simulator doesn't need to specially acknowledge a
  clear for it to reach `ECU_CLEAR_DONE`.
- **Matches the real interface**: SSM1 is a plain 5V TTL 2-wire
  full-duplex connection (separate TX/RX; see `docs/PARTS.md`) — the
  same topology as this rig's Mega↔DK cross-connect, including the
  3.3V↔5V level shift. `ssm1.c`
  keeps defensive echo-handling (`skip_first`, "if the line echoes our
  command") for interface hardware that might cause one, but a clean
  2-wire link — real or simulated — shouldn't trigger it, so this rig
  isn't skipping a real electrical property of the protocol.
- **Not simulated**: the exact analog ECU response timing/jitter a real
  engine would produce, and the true 1953.125 baud (this fixture asks for
  integer 1953; see above) — protocol framing and round-trip logic are
  what's actually under test here.
