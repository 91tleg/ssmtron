#!/usr/bin/env python3
"""A fake 91 Turbo SSM1 ECU for bench work.

Run it on the computer against a gauge built with ecu_on_usb.conf, where the
ECU link is the DK's USB serial port:

    python3 firmware/dev/fake_ecu.py /dev/cu.usbmodemXXXX1

It plays the ECU side of the same session the firmware's own tests simulate:

  stop       12 00 00 00     stops the stream
  read       78 hi lo 00     streams frames "hi lo data" for that address
  ROM ID     00 ff ff 00     while streaming address 0xFFFE, the next frame is
                             "ff fe 55 r0 r1 r2"
  write      aa hi lo data   recorded and printed; the first one can make the
                             ECU go silent, like a real memory write

Values change over time so a dial moves. Needs pyserial.
"""

import argparse
import math
import random
import sys
import time

import serial

BAUD = 1953
ID_ADDR = 0xFFFE

# Quiet time after which a partly received command is dropped.
RESYNC_S = 0.1
# The ROM ID reported is r0, r1 and r2. The profile key is r0 | r1 << 8, so
# --rom 3671 (the 91 Turbo, the default) sends 71 36 01. r2 is shown on the
# info page only.
CLEAR_ADDR = 0x15F0

# Raw bytes by address for the 91 Turbo profile (lib/ssm1/ssm1_params.c).
# Each entry is a function of seconds since start. Everything else reads a
# fixed value derived from its address.
RAW = {
    0x1404: lambda t: 175 + round(2 * math.sin(t / 5)),       # 14.0 V
    0x154B: lambda t: 40 + round(30 * math.sin(t / 7)),        # speed
    0x140B: lambda t: 32 + round(30 * (1 + math.sin(t / 4))),  # 800 to 2300 rpm
    0x1405: lambda t: 110 + round(8 * math.sin(t / 11)),       # coolant
    0x1489: lambda t: 20 + round(6 * math.sin(t / 2)),         # timing
    0x1403: lambda t: 50 + round(40 * math.sin(t * 2)),        # O2 0.1 to 0.9 V
    0x1516: lambda t: 200 + round(2 * math.sin(t / 9)),        # baro
    0x1400: lambda t: 60 + round(40 * math.sin(t / 3)),        # airflow
    0x1414: lambda t: 80 + round(40 * math.sin(t / 4)),        # load
    0x1487: lambda t: 50 + round(30 * math.sin(t / 3)),        # throttle volts
    0x1453: lambda t: 40 + round(30 * math.sin(t / 3)),        # throttle angle
    0x15F0: lambda t: 90 + round(40 * math.sin(t / 2)),        # pulse width
    0x142A: lambda t: 90 + round(40 * math.sin(t / 2)),        # pulse width
    0x158A: lambda t: 60 + round(30 * math.sin(t / 5)),        # idle air duty
    0x1488: lambda t: 128 + round(20 * math.sin(t / 6)),       # fuel trim
    0x144D: lambda t: 100 + round(60 * math.sin(t / 4)),       # boost duty
    0x00BE: lambda t: 136 + round(30 * math.sin(t / 3)),       # boost, vacuum
}


# Set by --sweep: the first page's reading (system voltage) swings across its
# whole range, so its needle never rests.
SWEEP = False


def raw_value(addr, t):
    if SWEEP and addr == 0x1404:
        return 128 + round(127 * math.sin(t * 1.5))
    if addr in RAW:
        return max(0, min(255, RAW[addr](t)))
    return (addr * 7 + 3) & 0xFF


class FakeEcu:
    def __init__(self, port, echo, silence_s, baud, parity, chaos=0, seed=None,
                 rom=0x3671, glitch=False):
        self.ser = serial.Serial(
            port,
            baudrate=baud,
            bytesize=serial.EIGHTBITS,
            parity=parity,
            stopbits=serial.STOPBITS_ONE,
            timeout=0.002,
        )
        self.echo = echo
        self.silence_s = silence_s
        self.t0 = time.monotonic()
        self.cmd = bytearray()
        self.streaming = False
        self.addr = 0
        self.id_pending = False
        self.muted_until = 0.0
        self.writes = 0
        # The bridge shifts bytes out at the line rate but accepts them from
        # the computer much faster, so writing without pacing builds a backlog
        # that arrives long after it was sent. Send the next frame only when
        # the line has had time to carry the last one.
        self.baud = baud
        self.next_send = 0.0
        # --chaos: damage the stream the way a poor connection could, at a
        # level from 1 (rare) to 3 (a lot). Counts are printed every minute.
        self.rom_bytes = bytes([rom & 0xFF, rom >> 8, 0x01])
        self.glitch = glitch
        self.last_rx = time.monotonic()
        self.next_glitch = time.monotonic() + 20
        self.chaos = chaos
        self.rng = random.Random(seed)
        self.stats = {"dropped": 0, "flipped": 0, "junk": 0, "gaps": 0}
        self.next_gap = time.monotonic() + self.rng.uniform(15, 40)
        self.next_stats = time.monotonic() + 60

    def log(self, text):
        print("%7.2f  %s" % (time.monotonic() - self.t0, text), flush=True)

    def handle(self, cmd):
        op = cmd[0]
        if op == 0x12:
            self.streaming = False
            self.log("stop")
        elif op == 0x78:
            self.addr = (cmd[1] << 8) | cmd[2]
            self.streaming = True
            self.log("read %04X" % self.addr)
        elif op == 0x00 and cmd[1] == 0xFF and cmd[2] == 0xFF:
            self.id_pending = self.streaming
            self.log("ROM ID request (%s)" % ("answering" if self.streaming else "ignored"))
        elif op == 0xAA:
            self.writes += 1
            self.log("write %04X <- %02X" % ((cmd[1] << 8) | cmd[2], cmd[3]))
            if self.writes == 1 and self.silence_s > 0:
                self.muted_until = time.monotonic() + self.silence_s
                self.log("busy for %.1f s" % self.silence_s)
        else:
            self.log("unknown command %s" % cmd.hex())

    def take_input(self):
        data = self.ser.read(16)
        now = time.monotonic()
        # A command is four bytes sent back to back. A gap means the rest of
        # a partial one is not coming, for example after a stray byte at
        # power-up; without this a lost byte shifts every later command.
        if self.cmd and (now - self.last_rx) > RESYNC_S:
            self.cmd.clear()
        if data:
            self.last_rx = now
        for b in data:
            if self.echo:
                self.ser.write(bytes([b]))
            if not self.cmd and b not in (0x12, 0x78, 0x00, 0xAA):
                continue  # not the start of a command: resync
            self.cmd.append(b)
            if len(self.cmd) == 4:
                self.handle(bytes(self.cmd))
                self.cmd.clear()

    def damage(self, frame):
        """Drops, corrupts and adds bytes, and now and then goes silent."""
        if not self.chaos:
            return frame
        rate = (0.0, 0.002, 0.01, 0.04)[self.chaos]
        now = time.monotonic()
        out = bytearray()
        for b in frame:
            if self.rng.random() < rate:
                self.stats["dropped"] += 1
                continue
            if self.rng.random() < rate:
                b ^= 1 << self.rng.randrange(8)
                self.stats["flipped"] += 1
            if self.rng.random() < rate:
                out.append(self.rng.randrange(256))
                self.stats["junk"] += 1
            out.append(b)
        if now >= self.next_gap:
            gap = self.rng.uniform(0.5, 1.5 * self.chaos)
            self.muted_until = max(self.muted_until, now + gap)
            self.next_gap = now + self.rng.uniform(15, 40)
            self.stats["gaps"] += 1
            self.log("hiccup: silent for %.1f s" % gap)
        if now >= self.next_stats:
            self.next_stats = now + 60
            self.log("chaos so far: %s" % self.stats)
        return bytes(out)

    def line_glitch(self):
        """A burst at the wrong rate: the gauge sees framing errors."""
        now = time.monotonic()
        if self.glitch and now >= self.next_glitch:
            self.next_glitch = now + self.rng.uniform(20, 60)
            self.ser.baudrate = 9600
            self.ser.write(bytes([0xA5]) * 6)
            self.ser.flush()
            self.ser.baudrate = self.baud
            self.next_send = time.monotonic() + 0.05
            self.log("line glitch")

    def send_frame(self):
        t = time.monotonic() - self.t0
        data = 0x55 if self.addr == ID_ADDR else raw_value(self.addr, t)
        frame = bytes([self.addr >> 8, self.addr & 0xFF, data])
        if self.addr == ID_ADDR and self.id_pending:
            frame += self.rom_bytes
            self.id_pending = False
            self.log("sent ROM ID %s" % self.rom_bytes.hex())
        frame = self.damage(frame)
        self.ser.write(frame)
        now = time.monotonic()
        self.next_send = max(now, self.next_send) + len(frame) * 10.0 / self.baud

    def run(self):
        self.log("listening on %s at %d baud, parity %s" % (self.ser.port, self.ser.baudrate, self.ser.parity))
        while True:
            self.take_input()
            now = time.monotonic()
            if self.streaming and now >= self.muted_until and now >= self.next_send:
                self.line_glitch()
                self.send_frame()


def main():
    parser = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    parser.add_argument("port", help="the DK's USB serial port")
    parser.add_argument("--echo", action="store_true",
                        help="echo every received byte, like a real K-line")
    parser.add_argument("--silence", type=float, default=22.0,
                        help="seconds the ECU stays silent after its first write")
    parser.add_argument("--baud", type=int, default=BAUD,
                        help="line rate; the car side is 1953")
    parser.add_argument("--parity", choices=["E", "N"], default="E",
                        help="E for the car side; N for the dev build on the USB port")
    parser.add_argument("--chaos", type=int, choices=[0, 1, 2, 3], default=0,
                        help="damage the stream: 1 rare, 3 a lot")
    parser.add_argument("--seed", type=int, default=None,
                        help="seed for --chaos, to repeat a run")
    parser.add_argument("--rom", type=lambda v: int(v, 16), default=0x3671,
                        help="the ROM ID to report, as four hex digits (3671)")
    parser.add_argument("--glitch", action="store_true",
                        help="now and then send a burst at the wrong rate, to "
                             "cause framing errors")
    parser.add_argument("--sweep", action="store_true",
                        help="swing the voltage reading over its whole range")
    args = parser.parse_args()
    global SWEEP
    SWEEP = args.sweep
    try:
        FakeEcu(args.port, args.echo, args.silence, args.baud, args.parity,
                args.chaos, args.seed, args.rom, args.glitch).run()
    except KeyboardInterrupt:
        sys.exit(0)


if __name__ == "__main__":
    main()
