"""Pytest fixtures for the bench HIL rig.

Env vars:
  HIL_DUT_PORT        Serial device for the DK's console UART -- the same
                       USB cable already used to flash/debug it (UART0;
                       see firmware/bsp/*/board.overlay). Required by any
                       test using the `dut` fixture.
  HIL_BLE_CONNSTRING   mcumgr connstring for the DUT, e.g.
                       "peer_name=Maxtek Datum" (the default; matches
                       BLE_NAME in firmware/app/main.c). Override if you
                       are running more than one unit on the bench.
  HIL_ACTUATOR_PORT    Serial device for the button-actuator fixture
                       (firmware/hil/fixture/button_actuator.ino). Required
                       by any test using the `button` fixture.

A test that needs hardware and finds its env var unset is skipped, not
failed: that keeps `pytest firmware/hil` runnable on a machine with no
rig attached (e.g. accidentally, or in a sandboxed agent) without every
test going red.
"""

import os
import time

import pytest
import serial

DUT_BAUD = 115200  # Zephyr's default console baud


class DutConsole:
    """Line-buffered view of the DK's console UART."""

    def __init__(self, port):
        self._ser = serial.Serial(port, DUT_BAUD, timeout=0.2)
        self._buf = ""

    def close(self):
        self._ser.close()

    def send_byte(self, b):
        self._ser.write(b)

    def drain(self):
        """Discards whatever has accumulated so far, so a later wait_for
        cannot match stale text left over from an earlier step."""
        self._buf = ""
        self._ser.read(65536)

    def wait_for(self, substr, timeout):
        """Blocks until substr appears in the console stream. Raises
        TimeoutError with whatever was actually seen, so a failed
        assertion shows real evidence instead of just "timed out"."""
        deadline = time.monotonic() + timeout
        while time.monotonic() < deadline:
            chunk = self._ser.read(4096).decode(errors="replace")
            if chunk:
                self._buf += chunk
                if substr in self._buf:
                    return self._buf
        raise TimeoutError(
            f"{substr!r} not seen within {timeout}s; console showed: {self._buf!r}"
        )

    def wait_for_any(self, substrs, timeout):
        """Like wait_for, but succeeds on the first of several possible
        substrings and reports which one matched."""
        deadline = time.monotonic() + timeout
        while time.monotonic() < deadline:
            chunk = self._ser.read(4096).decode(errors="replace")
            if chunk:
                self._buf += chunk
                for s in substrs:
                    if s in self._buf:
                        return s
        raise TimeoutError(
            f"none of {substrs!r} seen within {timeout}s; "
            f"console showed: {self._buf!r}"
        )

    def wait_for_any_output(self, timeout):
        """Blocks until any byte arrives. Used where the exact text isn't
        worth pinning down (see test_watchdog.py's comment on why "any
        output at all" is already a meaningful signal here)."""
        deadline = time.monotonic() + timeout
        while time.monotonic() < deadline:
            chunk = self._ser.read(4096)
            if chunk:
                return chunk.decode(errors="replace")
        raise TimeoutError(f"no console output within {timeout}s")


@pytest.fixture
def dut():
    port = os.environ.get("HIL_DUT_PORT")
    if not port:
        pytest.skip("HIL_DUT_PORT not set: no DUT attached")
    console = DutConsole(port)
    yield console
    console.close()


@pytest.fixture
def ble_connstring():
    return os.environ.get("HIL_BLE_CONNSTRING", "peer_name=Maxtek Datum")


class ButtonActuator:
    """Talks to firmware/hil/fixture/button_actuator.ino over USB serial."""

    def __init__(self, port):
        self._ser = serial.Serial(port, 115200, timeout=2.0)
        time.sleep(2.0)  # most USB-serial boards reset on port open
        self._ser.reset_input_buffer()

    def close(self):
        self._ser.close()

    def _command(self, line):
        self._ser.write((line + "\n").encode())
        reply = self._ser.readline().decode(errors="replace").strip()
        if not reply.startswith("OK"):
            raise RuntimeError(f"actuator rejected {line!r}: {reply!r}")

    def tap(self):
        """A brief press well under any real debounce/hold threshold."""
        self._command("TAP")

    def hold(self, ms):
        """Presses, waits exactly ms, then releases -- blocks until done."""
        self._command(f"HOLD {ms}")
        # The .ino only replies after releasing, but pad for USB latency
        # so the DUT has definitely already seen the release edge too.
        time.sleep(0.05)


@pytest.fixture
def button():
    port = os.environ.get("HIL_ACTUATOR_PORT")
    if not port:
        pytest.skip("HIL_ACTUATOR_PORT not set: no button actuator attached")
    actuator = ButtonActuator(port)
    yield actuator
    actuator.close()
