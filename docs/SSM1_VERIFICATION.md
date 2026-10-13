# SSM1: what to verify against the real car

None of `firmware/lib/ssm1/ssm1.c`'s clear-sequence, identification, or
parameter-switch timing has been run against an actual ECU yet. Two
issues below have already been fixed in code; the rest are open
questions that need a real car to answer, not more guessing.

## Checked against a fake ECU (not a car)

`firmware/dev/` plays the ECU from a computer through the DK's USB serial
port (see its README). That covers the real firmware and UART at the car's
1953 baud, but without parity and without a real ECU's timing:

- Identification of a 91 Turbo (ROM 3671) completes in one attempt, about
  0.8 s after the first stop.
- The trouble-code clear sends a stop, a read of 0x15F0 and four writes of
  the inverse of the current value 384 ms apart, waits 22.4 s after the
  last write, then resumes reads and ends in DONE.
- A full redraw of the face (up to about 0.8 s) no longer drops received
  bytes: the receive ring is 256 entries, up from 64.

## Fixed — re-confirm these work as intended

- **Clear value for ZERO-mode profiles** (`0x3672`, `0x3272`, `0x3273`):
  `ssm1.c`'s `clear_write` now sends `0xFF` instead of `0x00`. **Verify**:
  with a real stored trouble code on one of these ROM IDs, confirm a
  clear actually clears it. This direction has never been tested against
  real hardware, before or after the fix.
- **Post-clear resume wait**: now ~22.4s after the last write
  (`CLEAR_DONE_WAIT_MS`) before resuming normal reads, up from ~384ms.
  **Verify**: after a clear completes, confirm reads resume cleanly with
  no framing/parity error burst (which would mean we resumed while the
  ECU was still busy), and that the unit doesn't hang or stall longer
  than expected.

## Open questions — needs a real decision, not a guess

- **Inter-write gap** (`CLEAR_GAP_MS`, 384ms): is this too slow for the
  ECU to accept all 4 clear writes as one sequence? **Verify**: if the
  ZERO-mode clear test above fails, this is a suspect to tighten.
- **ROM ID identification timeout** (`ID_RESEND_MS`=300ms,
  `ID_TIMEOUT_MS`=1.5s): does identification ever fail or need to retry
  on a real connection (including a marginal one, e.g. a slightly dirty
  connector) where a more persistent approach would have succeeded? If
  so, loosen the timeout or resend more aggressively.
- **Parameter-switch sequence** (`QUIET_MS`=80ms line-silence wait after
  a STOP before reading the next address): does pressing NEXT on real
  hardware need this settling time, or does switching work fine without
  it? If the latter, removing the wait would make the UI feel snappier.
- **Stale-value blanking** (`FRESH_MS`=500ms): lowest priority — a UX
  tuning choice, not a correctness question. Only worth revisiting if it
  causes visible flicker during normal streaming jitter on real
  hardware.

## How to check these

A logic analyzer during each scenario (clear, identification, parameter
switch) is the most direct way to confirm actual ECU behavior and
timing, rather than inferring it from how the gauge's own UI responds.

A cheap 8-channel/24MHz USB logic analyzer (sigrok/PulseView-compatible,
~$10-20) is plenty for this: 1953 baud is far below its sample-rate
ceiling, and PulseView's built-in UART decoder reads the bytes directly.
One caveat: input protection on the cheap clones is often minimal, and
SSM1 is 5V TTL, so probe on the 3.3V side (the DK's UART1 TX/RX pins,
after the level shifter) rather than the raw 5V car-side lines — same
bit pattern and timing either way, without the voltage-tolerance risk.
