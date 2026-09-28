"""Watchdog recovery: nothing but real hardware can prove this.

Requires the DUT flashed with the HIL image (build with
firmware/hil/prj_hil.conf added, see that file), which compiles in the
console hang command (hil_test.c). A production image ignores the same
byte, since hil_test_poll() is a no-op without CONFIG_APP_HIL_TEST.
"""

# Must exceed WDT_TIMEOUT_MS in firmware/app/main.c (currently 4000 ms)
# by enough margin to also cover the reboot itself (bootloader hop, BLE
# stack bring-up) before anything can print again.
RECOVERY_TIMEOUT_S = 10.0


def test_recovers_from_a_hung_loop(dut):
    # The only things that ever write to the console in this firmware are
    # the boot banner, an occasional "wdt not armed", and gauge.c's rare
    # ECU-clear log lines (see gauge.c) -- nothing prints in ordinary idle
    # operation. So while the button is left alone, any console output
    # appearing after we deliberately hang the loop can only mean one
    # thing: the unit rebooted. That makes "any output at all" a valid,
    # sufficient recovery signal here, without pinning this test to the
    # exact boot-banner text (which changes with firmware/app/VERSION).
    dut.drain()
    dut.send_byte(b"H")  # see hil_test.c: HANG_CMD
    dut.wait_for_any_output(timeout=RECOVERY_TIMEOUT_S)
