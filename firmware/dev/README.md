# Dev build: the ECU link on the DK's USB serial port

Lets a script on the computer play the ECU, with no extra hardware. The
gauge's ECU link normally runs on UART1 (P1.01 and P1.02, the car side). This
build moves it to UART0, the DK's USB serial port, and moves the console to
Segger RTT.

A gauge built this way cannot talk to a car. Never use it in a vehicle or in
a release.

## Build and flash

    cd /opt/nordic/ncs/v3.2.4
    nrfutil sdk-manager toolchain launch --ncs-version v3.2.4 -- \
      west build -b nrf52840dk/nrf52840 \
      /Users/maxchou/Desktop/ssmtron/firmware/app \
      -d /Users/maxchou/Desktop/ssmtron/firmware/build-dev --pristine -- \
      -DEXTRA_CONF_FILE=/Users/maxchou/Desktop/ssmtron/firmware/dev/ecu_on_usb.conf \
      -DEXTRA_DTC_OVERLAY_FILE=/Users/maxchou/Desktop/ssmtron/firmware/dev/ecu_on_usb.overlay

## Run the fake ECU

    pip install pyserial
    python3 firmware/dev/fake_ecu.py /dev/cu.usbmodemXXXX1 --baud 1953 --parity N

Use the lower of the two serial ports. The bridge carries plain 8N1, so the
dev build drops the parity bit the car uses. The baud is
`CONFIG_APP_DEV_ECU_BAUD` in `ecu_on_usb.conf` and must match `--baud`.

The script plays a 91 Turbo (ROM 3671): it answers the stop, read and ROM ID
commands, streams values that move, and records writes. `--echo` echoes the
bytes it receives, as a real K-line does. `--silence` sets how long it goes
quiet after the first write, like a real memory write. `--sweep` swings the
voltage reading over its whole range so the needle never rests.

`--chaos 1` to `--chaos 3` damage the stream: dropped, flipped and extra
bytes, plus silent hiccups every 15 to 40 seconds. `--seed` repeats a run.
Use it to check that the gauge recovers instead of hanging or resetting.

The dev build also counts received bytes and receive errors (`dev_rx_bytes`
and `dev_rx_errors` in `ecu_io_poll`). Read them with a debugger to see
whether a change drops bytes.

## Stream the values over Bluetooth

Add `-DCONFIG_APP_BLE_LOG=y` to the dev build (or any build) to open
Bluetooth at boot as "Maxtek Datum" and send one CSV line per fresh value,
about every 200 ms. The passkey is random each boot and is written to the
console (RTT in the dev build) as `ble passkey NNNNNN`. Update mode reuses
the same link and passkey, so it still works with the flag on, and the
update page shows the same passkey.

    pip install bleak
    python3 tools/ble_log.py

Never ship a build with this flag: the passkey is fixed.

## Things to know

- Start the script after the board is flashed. Flashing makes the serial
  port disappear and come back, which leaves a running script talking to
  nothing.
- A debugger leaves the CPU halted when it ends. Resume it or reset the
  board.
- The script paces its output to the baud rate. Without that, the bridge
  queues bytes faster than it sends them, and the gauge sees stale data long
  after the script "answered".
- This tests the handshake, the decoding and the pages against the same
  commands the car side uses. It cannot test parity, the 5 V level shifter
  or the real ECU's timing.
