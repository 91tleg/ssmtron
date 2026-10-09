#!/usr/bin/env python3
"""Print the gauge's Bluetooth ECU log on the computer.

Build the firmware with CONFIG_APP_BLE_LOG=y (see firmware/dev/README.md),
then run:  python3 tools/ble_log.py

Each line is "<ms>,<name>,<value>,<unit>". macOS asks for the passkey (random each
boot, written to the console as "ble passkey NNNNNN") when the first read of the
link fails for lack of encryption.
"""

import argparse
import asyncio

from bleak import BleakClient, BleakScanner
from bleak.exc import BleakError

NUS_TX = "6e400003-b5a3-f393-e0a9-e50e24dcca9e"


async def run(name: str) -> None:
    device = await BleakScanner.find_device_by_name(name, timeout=15.0)
    if device is None:
        raise SystemExit(f"no device named {name!r} found")
    buf = b""

    def on_data(_char, data: bytearray) -> None:
        nonlocal buf
        buf += bytes(data)
        while b"\n" in buf:
            line, buf = buf.split(b"\n", 1)
            print(line.decode(errors="replace").rstrip("\r"), flush=True)

    async with BleakClient(device) as client:
        # The first try fails for lack of encryption, which makes macOS ask
        # for the passkey. Retry on the same link until pairing is done.
        while True:
            try:
                await client.start_notify(NUS_TX, on_data)
                break
            except (BleakError, ValueError) as err:
                if not isinstance(err, ValueError) and (
                    "ncryption" not in str(err) and "uthenticat" not in str(err)
                ):
                    raise
                # CoreBluetooth keeps the failed subscription; clear it.
                try:
                    await client.stop_notify(NUS_TX)
                except (BleakError, ValueError):
                    pass
                print("waiting for pairing, enter the passkey", flush=True)
                await asyncio.sleep(2.0)
        while client.is_connected:
            await asyncio.sleep(1.0)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--name", default="Maxtek Datum")
    args = parser.parse_args()
    asyncio.run(run(args.name))
