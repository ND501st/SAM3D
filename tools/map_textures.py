"""Draw the map's ground and ribbon textures (roadmap S9, docs/scenery/README.md "The ground's and the ribbons' materials").

    python tools/map_textures.py [--out art/map/textures] [--preview out/map_textures_preview.png]

Every pixel is made here from seeded noise (numpy only), so the project owns them (no third-party art: nothing for the
asset register) and a rerun gives the same files. tools/ue_make_map_materials.py imports them into
/Game/SAM3D/Map/Textures and builds the materials that read them, M_SimMapGround and M_SimMapRibbon.

Every texture is data (imported with sRGB off), each channel a brightness about 0.5 the material multiplies the
ground's or the ribbon's own colour by, so the land cover's colours (data/scenery/map_dressing.json) stay what they are
on the average and the texture only adds grain:

  T_MapGroundDetail  512 x 512, tiles both ways; the ground material samples it every DetailTileM (4 m) and again at
                     3.7 times that, and its A every MacroTileM (900 m).
                     R a field: clods of tilled soil and the crop's rows (0.25 m apart along U)
                     G grass and leaf litter (woods, marsh, the river's edge): blades and clumps
                     B hard ground (yards, military ground, verges): gravel grain on packed earth
                     A the macro variation: soft blotches, a few hundred metres across where it is sampled
  T_MapGroundNormal  512 x 512, tiles both ways: a tangent-space normal of a soil-and-grass height field (RGB, A 1)
  T_MapRibbon        1024 along (U, one tile = 10 m: the ribbons' UVs) x 128 across (V 0 one edge, 1 the other); tiles
                     along U only.
                     R asphalt: fine aggregate, darker patches, hairline cracks, the edges worn and broken
                     G a dirt track, a shoulder or a bank: two wheel ruts at V 0.3 and 0.7, the strip between them lighter
                     B a railway: ballast, a sleeper every 0.6 m across V 0.12-0.88, the rails' heads at V 0.38 and 0.62
                     A water: ripples stretched along the flow
"""
import argparse
import os

import numpy as np
from PIL import Image

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SIZE = 512
RIBBON_U, RIBBON_V = 1024, 128


def spectral_noise(shape, beta, seed, stretch=(1.0, 1.0)):
    """Tileable fractal noise in [0, 1]: random phases, amplitude 1/|k|^beta; `stretch` lengthens features per axis."""
    rng = np.random.default_rng(seed)
    fy = np.fft.fftfreq(shape[0]) * shape[0] * stretch[0]
    fx = np.fft.fftfreq(shape[1]) * shape[1] * stretch[1]
    ky, kx = np.meshgrid(fy, fx, indexing='ij')
    k = np.sqrt(kx * kx + ky * ky)
    k[0, 0] = 1.0
    amp = 1.0 / k ** beta
    amp[0, 0] = 0.0
    spec = amp * (rng.normal(size=shape) + 1j * rng.normal(size=shape))
    field = np.real(np.fft.ifft2(spec))
    field -= field.min()
    return field / max(field.max(), 1e-9)


def centred(field, mean=0.5, spread=0.16):
    """Shifted and scaled to the given mean and standard deviation, clipped to [0, 1]."""
    sd = max(float(field.std()), 1e-9)
    return np.clip((field - field.mean()) / sd * spread + mean, 0.0, 1.0)


def ground_detail(seed=1985):
    n = SIZE
    v = np.arange(n) / n  # V (rows) and U (columns), 0..1 over the tile
    # R: a field. Clods (grainy) under the crop's rows: 16 rows along U across the 4 m tile, each wavering a little.
    clods = spectral_noise((n, n), 1.1, seed + 1)
    waver = spectral_noise((n, n), 2.2, seed + 2) * 0.08
    rows = 0.5 + 0.5 * np.cos(2.0 * np.pi * (16.0 * v[:, None] + waver))
    field = 0.8 * clods + 0.2 * rows * (0.6 + 0.4 * spectral_noise((n, n), 1.6, seed + 3))  # rows faint: 3.7x reads them as stripes
    # G: grass and litter. Blades (fine, stretched along V) in clumps.
    blades = spectral_noise((n, n), 0.7, seed + 4, stretch=(0.35, 1.0))
    clumps = spectral_noise((n, n), 2.0, seed + 5)
    grass = 0.55 * blades + 0.45 * clumps
    # B: hard ground. Packed earth with gravel specks, light and dark.
    earth = spectral_noise((n, n), 1.8, seed + 6)
    grain = spectral_noise((n, n), 0.4, seed + 7)
    specks = np.where(grain > 0.72, 0.35, 0.0) - np.where(grain < 0.28, 0.25, 0.0)
    hard = 0.7 * earth + 0.3 * grain + specks
    # A: the macro variation, soft.
    macro = spectral_noise((n, n), 2.6, seed + 8)
    return np.stack([centred(field), centred(grass), centred(hard, spread=0.14), centred(macro, spread=0.2)], axis=-1)


def ground_normal(seed=1985, strength=2.0):
    n = SIZE
    height = 0.6 * spectral_noise((n, n), 1.3, seed + 11) + 0.4 * spectral_noise((n, n), 0.8, seed + 12)
    gy = (np.roll(height, -1, axis=0) - np.roll(height, 1, axis=0)) * 0.5 * n / 64.0
    gx = (np.roll(height, -1, axis=1) - np.roll(height, 1, axis=1)) * 0.5 * n / 64.0
    nx, ny, nz = -gx * strength, -gy * strength, np.ones_like(height)
    length = np.sqrt(nx * nx + ny * ny + nz * nz)
    rgb = np.stack([nx / length, ny / length, nz / length], axis=-1) * 0.5 + 0.5
    return np.concatenate([rgb, np.ones((n, n, 1))], axis=-1)


def ribbon(seed=1985):
    nu, nv = RIBBON_U, RIBBON_V
    u = np.arange(nu) / nu  # along: one tile is 10 m
    v = (np.arange(nv) + 0.5) / nv  # across: 0 one edge, 1 the other
    vv = np.broadcast_to(v[:, None], (nv, nu))
    uu = np.broadcast_to(u[None, :], (nv, nu))
    # The noise is made square and tileable along U, then cut to the ribbon's rows (no tiling across is needed).
    def noise(beta, s, stretch=(1.0, 1.0)):
        return spectral_noise((nu, nu), beta, s, stretch)[:nv, :]

    # R: asphalt.
    aggregate = noise(0.5, seed + 21)
    patches = noise(2.2, seed + 22)
    crack_field = noise(1.6, seed + 23)
    cracks = np.where(np.abs(crack_field - 0.5) < 0.012, -0.5, 0.0)
    edge = np.minimum(vv, 1.0 - vv)
    broken = np.where(edge < 0.04 + 0.05 * noise(1.4, seed + 24), -0.35, 0.0)
    asphalt = 0.45 * aggregate + 0.55 * patches + cracks + broken
    # G: a dirt track. Two ruts, the strip between and the verges lighter.
    rut = np.exp(-((vv - 0.3) / 0.07) ** 2) + np.exp(-((vv - 0.7) / 0.07) ** 2)
    dirt = 0.6 * noise(1.2, seed + 25) + 0.4 * noise(0.6, seed + 26) - 0.45 * rut
    # B: a railway. Ballast, sleepers every 0.6 m (10 m / 0.6 m = 16.67 along a tile: 17 so it tiles), the rails' heads.
    ballast = noise(0.35, seed + 27)
    sleeper = (np.abs(((uu * 17.0) % 1.0) - 0.5) < 0.2) & (vv > 0.12) & (vv < 0.88)
    rails = (np.abs(vv - 0.38) < 0.012) | (np.abs(vv - 0.62) < 0.012)
    rail = 0.6 * ballast + np.where(sleeper, -0.45, 0.0) + np.where(rails, 0.8, 0.0)
    # A: water, rippled along the flow.
    water = noise(1.5, seed + 28, stretch=(1.0, 0.25))
    return np.stack([centred(asphalt), centred(dirt), centred(rail, spread=0.2), centred(water, spread=0.12)], axis=-1)


def save(rgba, path):
    img = Image.fromarray(np.round(np.clip(rgba, 0.0, 1.0) * 255.0).astype(np.uint8), 'RGBA')
    img.save(path)
    return path


def build(out_dir):
    os.makedirs(out_dir, exist_ok=True)
    made = {
        'T_MapGroundDetail': ground_detail(),
        'T_MapGroundNormal': ground_normal(),
        'T_MapRibbon': ribbon(),
    }
    paths = {}
    for name, rgba in made.items():
        paths[name] = save(rgba, os.path.join(out_dir, name + '.png'))
    return made, paths


def preview(made, path):
    """The three side by side, each channel as a grey strip, for a look by eye."""
    tiles = []
    for name, rgba in made.items():
        h = 256
        for c in range(4):
            ch = rgba[..., c]
            img = Image.fromarray(np.round(ch * 255).astype(np.uint8), 'L').resize((h * ch.shape[1] // ch.shape[0], h))
            tiles.append(img)
    width = sum(t.width for t in tiles)
    sheet = Image.new('L', (width, 256))
    x = 0
    for t in tiles:
        sheet.paste(t, (x, 0))
        x += t.width
    os.makedirs(os.path.dirname(path), exist_ok=True)
    sheet.save(path)


def main():
    ap = argparse.ArgumentParser(description=__doc__.split('\n')[0])
    ap.add_argument('--out', default=os.path.join(REPO, 'art', 'map', 'textures'))
    ap.add_argument('--preview', default=None)
    args = ap.parse_args()
    made, paths = build(args.out)
    for name, p in paths.items():
        print('wrote', os.path.relpath(p, REPO))
    if args.preview:
        preview(made, args.preview)
        print('preview', args.preview)


if __name__ == '__main__':
    main()
