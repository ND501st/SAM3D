"""The haze reading (tools/haze_reading.py): the rows' distances over a curved ground, and the reading's recovering a known
transmittance from a hazed and a clear picture."""
import math
import sys
from pathlib import Path

import numpy as np
from PIL import Image

sys.path.insert(0, str(Path(__file__).resolve().parent.parent))
import haze_reading  # noqa: E402

W, H = 640, 360


def test_the_horizon_row_and_the_distances_down_from_it():
    d, hr = haze_reading.row_distances(H, W, 300.0, 0.0, 60.0)
    assert abs(hr - H / 2) < 1e-9
    below = d[int(hr) + 1:]
    assert np.all(np.diff(below) < 0)            # nearer the lower the row
    assert np.all(np.isinf(d[:int(hr)]))         # sky over the horizon
    # A ray 1 degree down from 300 m meets the ground near 300 / tan(1 degree) = 17.2 km, less where the earth curves away.
    d2, _ = haze_reading.row_distances(H, W, 300.0, -1.0, 60.0)  # looking 1 degree down: the centre row is 1 degree down
    assert 14000.0 < d2[H // 2] < 17500.0
    # A pitch down lifts the horizon above the middle of the picture.
    _, hr_down = haze_reading.row_distances(H, W, 300.0, -10.0, 60.0)
    assert hr_down < H / 2


def test_the_reading_recovers_a_transmittance(tmp_path):
    eye, pitch, fov = 300.0, -4.0, 60.0
    d, hr = haze_reading.row_distances(H, W, eye, pitch, fov)
    sky_lum = 0.40
    visibility_km = 20.0
    sigma = 3.912 / visibility_km
    clear = np.zeros((H, W, 3))
    hazed = np.zeros((H, W, 3))
    ground_lum = 0.15 + 0.05 * (np.arange(H) % 7) / 7.0  # the ground's own, varying row to row
    for y in range(H):
        if np.isfinite(d[y]):
            t = math.exp(-sigma * d[y] / 1000.0)
            clear[y, :, :] = ground_lum[y]
            hazed[y, :, :] = ground_lum[y] * t + sky_lum * (1.0 - t)
        else:
            clear[y, :, :] = 0.0
            hazed[y, :, :] = sky_lum
    # The pictures are sRGB 8 bit: store the values as luminance in sRGB.
    def encode(a):
        c = np.clip(a, 0.0, 1.0)
        s = np.where(c <= 0.0031308, 12.92 * c, 1.055 * c ** (1 / 2.4) - 0.055)
        return (s * 255 + 0.5).astype(np.uint8)
    Image.fromarray(encode(hazed)).save(tmp_path / "h.png")
    Image.fromarray(encode(clear)).save(tmp_path / "c.png")
    reading = haze_reading.read_view(tmp_path / "h.png", tmp_path / "c.png", eye, pitch, fov, visibility_km)
    assert reading, "nothing to read"
    for _a, _b, t, law in reading:
        assert abs(t - law) < 0.2 * law + 0.05, (t, law)
    assert haze_reading.verdict(reading) <= 1.5


def test_the_weather_of_the_views_scenario_has_a_visibility():
    assert haze_reading.weather_visibility_km() > 1.0
