"""Regressions for needle measurements through the real host renderer.

Run with .venv/bin/python -m unittest discover -s tools -p test_animate_ui.py.
"""

import math
from pathlib import Path
import subprocess
import tempfile
import unittest

import numpy as np

from animate_ui import needle_angle, read_csv, signed_gap
from preview_ui import build_renderer


class AnimationMeasurementTests(unittest.TestCase):
    def test_shaded_tip_uses_the_supplied_pivot(self):
        frame = np.zeros((240, 240), dtype=np.uint16)
        # A horizontal needle from an arc's lowered pivot, with no pixels
        # equal to the old exact-red detector's color.
        frame[172:175, 140:210] = 0xD102
        self.assertAlmostEqual(needle_angle(frame, 120, 173), 90.0)

    def test_missing_needle_is_nan(self):
        self.assertTrue(math.isnan(needle_angle(np.zeros((240, 240), dtype=np.uint16), 120, 120)))

    def test_rendered_arc_and_startup_targets(self):
        with tempfile.TemporaryDirectory() as folder:
            for size in (240, 360):
                _, renderer = build_renderer(size)
                for scene in ("anim_voltage_ramp", "anim_power_on"):
                    with self.subTest(size=size, scene=scene):
                        path = Path(folder) / f"{size}_{scene}.frames"
                        subprocess.run([str(renderer), scene, str(path), "3500"], check=True)
                        frames = np.fromfile(path, dtype="<u2").reshape(-1, size, size)
                        rows = read_csv(Path(str(path) + ".csv"))
                        self.assertEqual(len(frames), len(rows))
                        for frame, row in zip(frames, rows):
                            angle = needle_angle(frame, row["pivot_x"], row["pivot_y"])
                            self.assertLess(abs(signed_gap(angle, row["displayed_deg"])), 0.6)
                        last = rows[-1]
                        self.assertEqual(last["target_deg"], last["displayed_deg"])
                        self.assertLess(abs(signed_gap(
                            needle_angle(frames[-1], last["pivot_x"], last["pivot_y"]),
                            last["target_deg"],
                        )), 0.6)
                        if scene == "anim_voltage_ramp":
                            self.assertEqual(rows[0]["target_deg"], -45)
                            self.assertEqual(max(row["target_deg"] for row in rows), 45)
                            self.assertEqual(last["target_deg"], -45)
                        else:
                            self.assertEqual(rows[0]["target_deg"], 270)
                            self.assertEqual(max(row["target_deg"] for row in rows), 450)
                            self.assertEqual(min(row["target_deg"] for row in rows), 180)
                            self.assertEqual(last["target_deg"], 270)
                            self.assertTrue(any(row["aim_value"] != row["value"] for row in rows))


if __name__ == "__main__":
    unittest.main()
