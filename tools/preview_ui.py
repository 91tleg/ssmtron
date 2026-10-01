#!/usr/bin/env python3
"""Render the firmware UI to PNG files without flashing a device.

Run from anywhere with: python3 tools/preview_ui.py
The PNGs and index.html are written to firmware/build-ui-preview/ui/.
"""

from __future__ import annotations

import argparse
from pathlib import Path
import struct
import subprocess
import zlib


ROOT = Path(__file__).resolve().parents[1]
DEFAULT_SIZE = 240
SCENES = {
    "boost_vacuum": "Boost: -18 inHg",
    "boost_zero": "Boost: zero",
    "boost_positive": "Boost: 15 psi",
    "rpm_3500": "RPM: 3500",
    "rpm_7000": "RPM: 7000",
    "coolant_180": "Coolant: 180 F",
    "coolant_240": "Coolant: 240 F",
    "voltage_14": "Voltage: 14 V",
    "coolant_180_dark": "Dark: coolant 180 F",
    "voltage_14_dark": "Dark: voltage 14 V",
    "digital_load": "Digital: load 42",
    "flags_codes": "Flags: stored codes 2/3 (0x22)",
    "flags_no_codes": "Flags: no codes stored (0x00)",
    "flags_switches": "Flags: input switches (0xD5)",
    "flags_stale": "Flags: stale byte",
    "flags_hold_to_clear": "Flags: hold to clear",
    "flags_clearing": "Flags: clearing",
    "flags_cleared": "Flags: cleared",
    "boost_sweep_peak": "Boost: power-on sweep at full scale",
    "no_ecu": "No ECU: dashes only",
    "info_no_ecu": "Info page: no ECU",
    "info_connected": "Info page: connected",
    "update_mode": "Update mode: waiting for a phone",
    "update_mode_dark": "Dark: update mode",
    "info_connected_dark": "Dark: info page, connected",
    "boost_vacuum_dark": "Dark: boost -18 inHg",
    "boost_positive_dark": "Dark: boost 15 psi",
    "rpm_3500_dark": "Dark: RPM 3500",
    "digital_load_dark": "Dark: digital load 42",
    "flags_codes_dark": "Dark: stored codes 2/3",
    "flags_switches_dark": "Dark: input switches",
    "flags_clearing_dark": "Dark: clearing",
}


def png_chunk(kind: bytes, payload: bytes) -> bytes:
    body = kind + payload
    return struct.pack(">I", len(payload)) + body + struct.pack(">I", zlib.crc32(body))


def write_png(raw_path: Path, png_path: Path, size: int) -> None:
    width = height = size
    raw = raw_path.read_bytes()
    expected = width * height * 2
    if len(raw) != expected:
        raise ValueError(f"{raw_path}: expected {expected} bytes, got {len(raw)}")

    pixels = memoryview(raw)
    rows = bytearray()
    for y in range(height):
        rows.append(0)  # PNG filter type 0
        for x in range(width):
            offset = (y * width + x) * 2
            color = pixels[offset] | (pixels[offset + 1] << 8)
            r = ((color >> 11) & 0x1F) * 255 // 31
            g = ((color >> 5) & 0x3F) * 255 // 63
            b = (color & 0x1F) * 255 // 31
            inside = (2 * x - (width - 1)) ** 2 + (2 * y - (height - 1)) ** 2 <= size**2
            rows.extend((r, g, b, 255 if inside else 0))

    header = struct.pack(">IIBBBBB", width, height, 8, 6, 0, 0, 0)
    png = b"\x89PNG\r\n\x1a\n"
    png += png_chunk(b"IHDR", header)
    png += png_chunk(b"IDAT", zlib.compress(rows, level=9))
    png += png_chunk(b"IEND", b"")
    png_path.write_bytes(png)


def write_gallery(output: Path, size: int) -> None:
    cards = "\n".join(
        f'<figure><img src="{name}.png" alt="{label}">'
        f"<figcaption>{label}</figcaption></figure>"
        for name, label in SCENES.items()
    )
    (output / "index.html").write_text(
        "<!doctype html><html lang=\"en\"><meta charset=\"utf-8\">"
        "<title>SSMtron UI preview</title>"
        "<style>body{background:#20242b;color:#f5f5f5;font:16px system-ui;"
        "margin:2rem}main{display:flex;flex-wrap:wrap;gap:2rem}"
        f"figure{{margin:0;text-align:center}}img{{width:{size}px;height:{size}px}}"
        "figcaption{margin-top:.5rem}</style>"
        "<h1>SSMtron UI preview</h1><main>"
        + cards
        + "</main></html>\n",
        encoding="utf-8",
    )


def build_renderer(size: int) -> tuple[Path, Path]:
    """Builds the renderer for this panel size. Returns its build folder and
    the renderer program."""
    # Each size gets its own build, since the size is compiled in.
    build = ROOT / "firmware" / (
        "build-ui-preview"
        if size == DEFAULT_SIZE
        else f"build-ui-preview-{size}"
    )
    subprocess.run(
        [
            "cmake",
            "-S",
            str(ROOT / "tools"),
            "-B",
            str(build),
            f"-DCMAKE_C_FLAGS=-DDISPLAY_WIDTH={size}u -DDISPLAY_HEIGHT={size}u",
        ],
        check=True,
    )
    subprocess.run(
        ["cmake", "--build", str(build), "--target", "preview_ui"], check=True
    )
    return build, build / "preview_ui"


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--size",
        type=int,
        default=DEFAULT_SIZE,
        help="panel width and height in pixels (default %(default)s)",
    )
    parser.add_argument(
        "--output", type=Path, default=None, help="preview directory"
    )
    args = parser.parse_args()
    size = args.size
    build, renderer = build_renderer(size)
    output = (args.output or build / "ui").resolve()
    output.mkdir(parents=True, exist_ok=True)

    for scene in SCENES:
        raw_path = output / f"{scene}.rgb565"
        subprocess.run([str(renderer), scene, str(raw_path)], check=True)
        write_png(raw_path, output / f"{scene}.png", size)
        raw_path.unlink()

    write_gallery(output, size)
    print(f"UI previews: {output / 'index.html'}")


if __name__ == "__main__":
    main()
