"""ACTION button hold-to-clear (see gauge.c's poll_action). The SIL suite
(firmware/test/test_gauge.c) already covers the state machine's logic
against a faked GPIO; this is the physical half it can't reach: a real
press on the real pin, through the real debounce, on real hardware.

Driven by firmware/hil/fixture/fixture.ino's button actuator (see
conftest.py's `button` fixture) rather than a human: a script can hit
exact hold durations a person can't reliably reproduce (e.g. just under
vs. just over the 2 s threshold), which is exactly what this needs to
prove.

The bench's fake ECU (fixture.ino's ecu_sim, ROM ID 0x3672) means a
clear request now genuinely identifies and runs to completion: ssm1.c's
clear sequence (clear_tick/clear_write) is purely time/write-count based
-- 4 writes 384 ms apart -- once s.profile is non-NULL, with no ECU-side
acknowledgment required. So "ecu clear: done" is the correct outcome
here, unlike on a bench with no simulated ECU at all.
"""

ACTION_HOLD_MS = 2000  # must match ACTION_HOLD_MS in firmware/app/gauge.c
CLEAR_START_TIMEOUT_S = 1.5
# 4 writes * 384 ms gap (see ssm1.c CLEAR_WRITES/CLEAR_GAP_MS) plus margin.
CLEAR_RESULT_TIMEOUT_S = 4.0


def test_tap_does_nothing(dut, button):
    dut.drain()
    button.tap()
    try:
        dut.wait_for("ecu clear:", timeout=2.0)
    except TimeoutError:
        return  # expected: a brief tap prints nothing
    raise AssertionError("a brief tap must not start a clear")


def test_hold_under_threshold_does_nothing(dut, button):
    dut.drain()
    button.hold(ACTION_HOLD_MS - 200)  # comfortably short of the threshold
    try:
        dut.wait_for("ecu clear:", timeout=1.0)
    except TimeoutError:
        return  # expected: released before the hold completed
    raise AssertionError("releasing before the 2 s hold must not start a clear")


def test_hold_past_threshold_triggers_clear(dut, button):
    dut.drain()
    button.hold(ACTION_HOLD_MS + 200)  # comfortably past the threshold
    dut.wait_for("ecu clear: started", timeout=CLEAR_START_TIMEOUT_S)
    dut.wait_for("ecu clear: done", timeout=CLEAR_RESULT_TIMEOUT_S)
