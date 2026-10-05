#!/usr/bin/env python3
"""Animate the gauge dials so the needle's motion can be watched and measured.

The real firmware code draws every frame, at its real 33 ms rhythm, while the
reading follows a script (a step, a ramp, the power-on sweep).
Each scene gives an animated GIF, a CSV of the reading and the needle angle
at every frame, a plot of the angle against its target, and a line of numbers:
how long it took, how far it overshot and how fast it moved.

Needs NumPy, Pillow and Matplotlib:
    python3 -m venv .venv && .venv/bin/pip install -r tools/requirements.txt
Run from anywhere with: .venv/bin/python tools/animate_ui.py
The files and index.html are written to firmware/build-ui-preview/anim/.
"""

from __future__ import annotations

import argparse
import csv
import math
from pathlib import Path
import subprocess
import sys

import numpy as np
from PIL import Image
import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt  # noqa: E402

sys.path.insert(0, str(Path(__file__).resolve().parent))
from preview_ui import DEFAULT_SIZE, build_renderer  # noqa: E402

FRAME_MS = 33
BACKGROUND = (0x20, 0x24, 0x2B)


def top_cw(deg: float) -> float:
    """The dial's angle as it is measured: degrees clockwise from the top."""
    return deg % 360.0


# Targets and pivots come from the real firmware poll, including startup sweep
# overrides. The needle angle is measured independently from rendered pixels.
SCENES = {
    "anim_power_on": ("Power-on sweep, boost: rest, full scale, bottom, rest", 3500),
    "anim_boost_step": ("Boost: step to 15 psi, then to -20 inHg", 4000),
    "anim_boost_ramp": ("Boost: ramp to 30 psi and down to -30 inHg", 3500),
    "anim_coolant_sweep": ("Coolant: 100 to 260 F and back on the upper arc", 3500),
    "anim_voltage_ramp": ("Voltage: 8 to 16 V and back on the upper arc", 3500),
}


def rgb565_to_rgb(frames: np.ndarray) -> np.ndarray:
    """RGB565 frames or a single frame to 8-bit RGB."""
    r = ((frames >> 11) & 0x1F).astype(np.uint16) * 255 // 31
    g = ((frames >> 5) & 0x3F).astype(np.uint16) * 255 // 63
    b = (frames & 0x1F).astype(np.uint16) * 255 // 31
    return np.stack([r, g, b], axis=-1).astype(np.uint8)


def circle_mask(size: int) -> np.ndarray:
    y, x = np.mgrid[0:size, 0:size]
    return (2 * x - (size - 1)) ** 2 + (2 * y - (size - 1)) ** 2 <= size**2


def needle_angle(frame: np.ndarray, pivot_x: int, pivot_y: int) -> float:
    """Measure the shaded red tip around the actual firmware needle pivot."""
    rgb = rgb565_to_rgb(frame).astype(np.int16)
    red = (
        (rgb[..., 0] > 120)
        & (rgb[..., 0] > 2 * rgb[..., 1])
        & (rgb[..., 0] > 2 * rgb[..., 2])
    )
    ys, xs = np.nonzero(red)
    if len(xs) == 0:
        return float("nan")
    dx = xs - pivot_x
    dy = ys - pivot_y
    radius = np.hypot(dx, dy)
    # Average both edges of the tip so shading and rasterization do not bias
    # the result toward one side of the blade.
    tip = radius >= radius.max() - 3.0
    return top_cw(math.degrees(math.atan2(
        float(dx[tip].mean()), -float(dy[tip].mean())
    )))


def signed_gap(a: float, b: float) -> float:
    """a - b as the shortest turn, in -180..180 degrees."""
    return (a - b + 180.0) % 360.0 - 180.0


def read_csv(path: Path) -> list[dict[str, int]]:
    with path.open(newline="") as stream:
        return [
            {key: int(value) for key, value in row.items()}
            for row in csv.DictReader(stream)
        ]


def unwrap(angles: list[float]) -> list[float]:
    """The angles as one continuous curve, with no jump at 0 and 360."""
    out: list[float] = []
    previous = None
    offset = 0.0
    for a in angles:
        if math.isnan(a):
            out.append(a)
            continue
        if previous is not None:
            step = a - previous
            if step > 180.0:
                offset -= 360.0
            elif step < -180.0:
                offset += 360.0
        previous = a
        out.append(a + offset)
    return out


def analyze(times, angles, targets):
    """Numbers about the motion: the fastest the needle moved,
    how far it went past its target, how far it fell behind
    while the reading was moving, and how long it took to settle after the
    reading stopped changing."""
    speeds = []
    for i in range(1, len(angles)):
        if not (math.isnan(angles[i]) or math.isnan(angles[i - 1])):
            speeds.append(
                abs(angles[i] - angles[i - 1]) * 1000.0 / (times[i] - times[i - 1])
            )
    overshoot = 0.0
    lag = 0.0
    last_change = None
    for i in range(1, len(times)):
        if math.isnan(angles[i]):
            continue
        gap = angles[i] - targets[i]
        lag = max(lag, abs(gap))
        moving = targets[i] - targets[i - 1]
        if abs(moving) < 0.01 and last_change is not None:
            # the reading is still: a needle past it, in the direction it came
            # from, has overshot
            overshoot = max(overshoot, gap * (1.0 if targets[i] > targets[last_change - 1] else -1.0))
        if abs(moving) > 0.01:
            last_change = i
    settle = None
    if last_change is not None:
        for j in range(last_change, len(times)):
            if not math.isnan(angles[j]) and abs(angles[j] - targets[j]) <= 1.0:
                settle = times[j] - times[last_change]
                break
    peak_i = max(
        (i for i in range(len(angles)) if not math.isnan(angles[i])),
        key=lambda i: abs(angles[i] - angles[0]),
    )
    return {
        "max_speed_dps": max(speeds) if speeds else 0.0,
        "overshoot_deg": overshoot,
        "lag_deg": lag,
        "settle_ms": settle,
        "peak_deg": angles[peak_i],
        "peak_ms": times[peak_i],
    }


def render_scene(renderer: Path, scene: str, size: int, output: Path) -> dict:
    title, duration = SCENES[scene]
    frames_path = output / f"{scene}.frames"
    subprocess.run([str(renderer), scene, str(frames_path), str(duration)], check=True)
    raw = np.fromfile(frames_path, dtype="<u2")
    count = raw.size // (size * size)
    frames = raw.reshape(count, size, size)
    csv_path = Path(str(frames_path) + ".csv")
    rows = read_csv(csv_path)
    frames_path.unlink()
    csv_path.unlink()

    if len(rows) != count:
        raise ValueError(f"{scene}: {count} frames but {len(rows)} metadata rows")
    times = [row["t_ms"] for row in rows]
    angles = unwrap([
        needle_angle(frame, row["pivot_x"], row["pivot_y"])
        for frame, row in zip(frames, rows)
    ])
    if any(math.isnan(angle) for angle in angles):
        raise ValueError(f"{scene}: could not measure the needle in every frame")
    # Firmware targets already use an unwrapped scale. Preserve its intended
    # direction even when a reading jumps by more than half a turn.
    targets = [row["target_deg"] for row in rows]
    shift = 360.0 * round((targets[0] - angles[0]) / 360.0)
    angles = [angle + shift for angle in angles]

    # The animated GIF, with the panel's round edge.
    mask = circle_mask(size)
    rgb_frames = rgb565_to_rgb(frames)
    for i in range(count):
        rgb_frames[i][~mask] = BACKGROUND
    images = [Image.fromarray(rgb_frames[i]) for i in range(count)]
    images[0].save(
        output / f"{scene}.gif",
        save_all=True,
        append_images=images[1:],
        duration=FRAME_MS,
        loop=0,
        optimize=False,
    )

    # The numbers, one line per frame.
    with open(output / f"{scene}.csv", "w") as f:
        f.write("t_ms,value,aim_value,target_deg,displayed_deg,needle_deg\n")
        for row, tg, a in zip(rows, targets, angles):
            f.write(
                f"{row['t_ms']},{row['value']},{row['aim_value']},"
                f"{tg:.2f},{row['displayed_deg']},{a:.2f}\n"
            )

    # The plot: where the needle was against where it should be.
    fig, ax = plt.subplots(figsize=(7, 3), dpi=110)
    ax.plot(times, targets, "--", color="#888", label="target")
    ax.plot(times, angles, "-", color="#d3342a", label="needle (drawn)")
    ax.set_xlabel("time (ms)")
    ax.set_ylabel("needle angle (degrees, unwrapped)")
    ax.set_title(title)
    ax.grid(alpha=0.3)
    ax.legend()
    fig.tight_layout()
    fig.savefig(output / f"{scene}.png")
    plt.close(fig)

    stats = analyze(times, angles, targets)
    stats["title"] = title
    stats["frames"] = count
    return stats


def write_index(output: Path, results: dict[str, dict], size: int) -> None:
    cards = []
    for scene, st in results.items():
        settle = "n/a" if st["settle_ms"] is None else f"{st['settle_ms']:.0f} ms"
        cards.append(
            f"<section><h2>{st['title']}</h2>"
            f'<div class="row"><img class="gif" src="{scene}.gif" alt="">'
            f'<img class="plot" src="{scene}.png" alt=""></div>'
            f"<p>{st['frames']} frames. Fastest {st['max_speed_dps']:.0f} deg/s. "
            f"Overshoot {st['overshoot_deg']:.1f} deg, "
            f"furthest behind the target {st['lag_deg']:.1f} deg, within 1 degree "
            f"of it {settle} after the last change. "
            f"<a href=\"{scene}.csv\">CSV</a></p></section>"
        )
    (output / "index.html").write_text(
        '<!doctype html><html lang="en"><meta charset="utf-8">'
        "<title>SSMtron needle animation</title>"
        "<style>body{background:#20242b;color:#f5f5f5;font:16px system-ui;margin:2rem}"
        ".row{display:flex;flex-wrap:wrap;gap:1.5rem;align-items:center}"
        f".gif{{width:{size}px;height:{size}px}}.plot{{background:#fff;max-width:100%}}"
        "section{margin-bottom:2.5rem}a{color:#8ab4f8}</style>"
        "<h1>SSMtron needle animation</h1>" + "\n".join(cards) + "</html>\n",
        encoding="utf-8",
    )


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--size", type=int, default=DEFAULT_SIZE,
                        help="panel width and height in pixels (default %(default)s)")
    parser.add_argument("--output", type=Path, default=None, help="output folder")
    parser.add_argument("scenes", nargs="*",
                        metavar="SCENE", help="scenes to render (default: all)")
    args = parser.parse_args()
    for scene in args.scenes:
        if scene not in SCENES:
            parser.error(f"unknown scene {scene!r}; choose from {', '.join(SCENES)}")
    build, renderer = build_renderer(args.size)
    output = (args.output or build / "anim").resolve()
    output.mkdir(parents=True, exist_ok=True)

    results = {}
    for scene in args.scenes or SCENES:
        print(f"rendering {scene}")
        results[scene] = render_scene(renderer, scene, args.size, output)
    write_index(output, results, args.size)
    print(f"Needle animation: {output / 'index.html'}")


if __name__ == "__main__":
    main()
