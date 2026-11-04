"""The haze check (roadmap T3.4, docs/scenery/T3_T4_PLAN.md): how much of the far ground's own luminance a view of the game
keeps, against what Koschmieder's law keeps for the weather preset's visibility.

    python tools/haze_reading.py out/views/t32            # every view of a run, against the scenario's weather
    python tools/haze_reading.py out/views/t32 --visibility-km 20

Each view is rendered twice, with the atmosphere and the fog (the run) and without them (`run_clear`, the same camera and
the same ground). A ray `theta` below the horizontal from an eye `h` over a ground that falls away with the earth's
curvature meets it at d with h = d tan(theta) + d^2 / (2 R), which gives each picture row its distance. The row's median
over the picture's width is L(d) hazed and L0(d) clear, the sky's luminance Ls the rows just over the horizon, so the share
of the ground's light that reaches the eye is T(d) = (L(d) - Ls) / (L0(d) - Ls), the same pixels either way; Koschmieder's
law says exp(-3.912 d / V). Low eyes see a few rows of distant ground (at 1.7 m a kilometre is a pixel's width), so the
check reads the views with the eye high enough for the distance to spread: V7 (4 m), V8 (5.5 m), V10 and V11. By eye, not by
measurement: a view is called good when T is within a factor of 1.5 of the law's.
"""
import argparse
import json
import math
import re
import sys
from pathlib import Path

import numpy as np
from PIL import Image

ROOT = Path(__file__).resolve().parent.parent
EARTH_R_M = 6371000.0
BINS_KM = ((0.5, 1.0), (1.0, 2.0), (2.0, 4.0), (4.0, 8.0))  # the distances a reading is made over


def luminance(rgb):
    """Relative luminance of an sRGB picture (floats 0..1 in, linear Y out)."""
    c = np.where(rgb <= 0.04045, rgb / 12.92, ((rgb + 0.055) / 1.055) ** 2.4)
    return 0.2126 * c[..., 0] + 0.7152 * c[..., 1] + 0.0722 * c[..., 2]


def row_distances(height_px, width_px, eye_m, pitch_deg, fov_deg):
    """The ground's distance (m) along each picture row, inf at and over the horizon; the horizon's row too."""
    f = 0.5 * width_px / math.tan(math.radians(0.5 * fov_deg))
    down = -math.radians(pitch_deg)  # the camera looks this far below the horizontal (a negative pitch looks down)
    rows = np.arange(height_px) + 0.5
    # A ray through the row falls (atan of its offset below the axis) plus the camera's own fall below the horizontal.
    depression = np.arctan((rows - 0.5 * height_px) / f) + down
    horizon_row = 0.5 * height_px - f * math.tan(down)
    t = np.tan(np.maximum(depression, 1e-9))
    d = EARTH_R_M * (-t + np.sqrt(t * t + 2.0 * eye_m / EARTH_R_M))
    d = np.where(depression > 1e-9, d, np.inf)
    return d, horizon_row


def horizon_sky(lum, horizon_row):
    """The sky's luminance just over the horizon (the haze's own colour), None when the horizon is off the picture."""
    h, w = lum.shape
    hr = int(round(horizon_row))
    if hr < 20 or hr >= h - 4:
        return None
    return float(np.median(lum[max(0, hr - 14):hr - 3, int(0.1 * w):int(0.9 * w)]))


def read_view(png, clear_png, eye_m, pitch_deg, fov_deg, visibility_km, sky=None):
    """The reading of one view: a list of (from_km, to_km, measured T, the law's T), or None where it cannot be read.
    `png` is the hazed picture, `clear_png` the same without the atmosphere and the fog, `sky` the haze's own luminance (the
    sky over the horizon) where the picture's horizon is off it."""
    img = np.asarray(Image.open(png).convert("RGB"), dtype=np.float64) / 255.0
    clear = np.asarray(Image.open(clear_png).convert("RGB"), dtype=np.float64) / 255.0
    h, w = img.shape[:2]
    lum, lum0 = luminance(img), luminance(clear)
    d, horizon_row = row_distances(h, w, eye_m, pitch_deg, fov_deg)
    x0, x1 = int(0.1 * w), int(0.9 * w)
    ground = np.median(lum[:, x0:x1], axis=1)  # each row's median across the middle of the picture
    ground0 = np.median(lum0[:, x0:x1], axis=1)
    if sky is None:
        sky = horizon_sky(lum, horizon_row)
    if sky is None:
        return None
    sigma = 3.912 / visibility_km
    out = []
    for a, b in BINS_KM:
        sel = (d >= a * 1000.0) & (d < b * 1000.0)
        if sel.sum() < 3:
            continue
        l, l0 = float(np.median(ground[sel])), float(np.median(ground0[sel]))
        if abs(l0 - sky) < 0.05:
            continue  # little contrast to read it by: the ratio is noise
        mid_km = float(np.median(d[sel])) / 1000.0
        out.append((a, b, (l - sky) / (l0 - sky), math.exp(-sigma * mid_km)))
    return out


def weather_visibility_km(scenario="scenarios/kesmark_range_test.json"):
    """The visibility of the scenario's weather preset (data/weather/presets.json)."""
    sc = json.loads((ROOT / "data" / scenario).read_text(encoding="utf-8"))
    presets = json.loads((ROOT / "data" / "weather" / "presets.json").read_text(encoding="utf-8"))["presets"]
    for p in presets:
        if p["id"] == sc.get("weather"):
            return float(p["surface"]["visibility_km"])
    raise SystemExit("no weather preset for the scenario")


def view_pitch_deg(v):
    """The view's pitch, None where the game aims it (a view that looks at its anchor without a pitch of its own looks down
    at the anchor's ground by an angle this tool does not know)."""
    if "pitch_deg" in v:
        return float(v["pitch_deg"])
    return None if v.get("look") == "at_anchor" else 0.0


def verdict(reading):
    """'ok' when every bin's T is within a factor of 1.5 of the law's (T under 0.03 is below what a picture shows)."""
    worst = 1.0
    for _a, _b, t, law in reading:
        if law < 0.03:
            continue
        ratio = max(t, 0.01) / law
        worst = max(worst, ratio, 1.0 / ratio)
    return worst


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("folder", help="a run's folder, out/views/<label>")
    ap.add_argument("--clear", default="", help="the same views without the atmosphere and the fog (default: <folder>_clear)")
    ap.add_argument("--visibility-km", type=float, default=0.0, help="the weather's visibility (default: the scenario's preset)")
    args = ap.parse_args(argv)
    folder = Path(args.folder)
    clear = Path(args.clear) if args.clear else folder.with_name(folder.name + "_clear")
    visibility = args.visibility_km or weather_visibility_km()
    views = json.loads((ROOT / "data" / "scenery" / "views.json").read_text(encoding="utf-8"))["views"]
    print(f"visibility {visibility:g} km: Koschmieder's extinction {3.912 / visibility:.3f} per km")
    # The haze's own luminance: the sky over the horizon in the views that show it (one for the run: the same weather and hour).
    skies = []
    for v in views:
        if view_pitch_deg(v) is None:
            continue
        for png in folder.glob(v["id"] + "*.png"):
            img = np.asarray(Image.open(png).convert("RGB"), dtype=np.float64) / 255.0
            _d, hrow = row_distances(img.shape[0], img.shape[1], float(v.get("eye_m", 1.7)), view_pitch_deg(v), float(v.get("fov_deg", 60.0)))
            sky = horizon_sky(luminance(img), hrow)
            if sky is not None:
                skies.append(sky)
    run_sky = float(np.median(skies)) if skies else None
    print(f"the sky over the horizon, {len(skies)} views: {run_sky:.3f}" if run_sky is not None else "no view shows the horizon")
    for v in views:
        if view_pitch_deg(v) is None:
            print(f"{v['id']:24} not read: the game aims it at its anchor, at a pitch this tool does not know")
            continue
        for png in sorted(folder.glob(v["id"] + "*.png")):
            if not re.fullmatch(re.escape(v["id"]) + r"(_h\d{3})?\.png", png.name):
                continue
            if not (clear / png.name).exists():
                print(f"{png.stem:24} no clear picture in {clear}")
                continue
            reading = read_view(png, clear / png.name, float(v.get("eye_m", 1.7)), view_pitch_deg(v), float(v.get("fov_deg", 60.0)),
                                visibility, run_sky)
            if not reading:
                print(f"{png.stem:24} nothing to read (too little distance across the rows, or no contrast)")
                continue
            cells = "  ".join(f"{a:g}-{b:g} km {t:5.2f} vs {law:4.2f}" for a, b, t, law in reading)
            worst = verdict(reading)
            print(f"{png.stem:24} {cells}  | worst factor {worst:3.1f} {'ok' if worst <= 1.5 else 'OFF'}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
