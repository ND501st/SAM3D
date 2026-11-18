"""Draw the effects' textures (docs/fx/README.md): smoke puffs, the explosion's fireball flipbook and its heat ramp,
glows, tracer streaks, sparks, the contrail ribbon, the rocket plume, the shock ring, debris and the scorch decal.

    python tools/fx_textures.py [--out art/fx/textures] [--preview out/fx_textures_preview.png]

Every pixel is made here from seeded noise (numpy only), so the project owns them and a rerun gives the same files.
tools/ue_make_fx_assets.py imports them into /Game/SAM3D/FX/Textures and builds the materials that read them.

Channel packing (the materials in ue_make_fx_assets.py read these):
  smoke atlas, contrail, debris   R lit from above   G thinness (light through)   B erosion noise   A density
  fireball flipbook               R temperature      G lit from above             B erosion noise   A density
  glow, streak, spark, ring, plume, afterburner  grey in RGB, the same value in A (the materials tint and scale it)
  heat ramp                       RGB a burning body's colour by temperature (U 0 cold .. 1 white hot), A its opacity
  scorch                          RGB soot colour, A mask
All but the heat ramp and the scorch are data (imported with sRGB off); those two are colours (sRGB on).
"""
import argparse
import json
import os

import numpy as np
from PIL import Image

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))


# ---------------------------------------------------------------- noise
def spectral_noise(shape, beta, seed):
    """Tileable fractal noise in [0, 1]: random phases, amplitude 1/|k|^beta (beta 1 grainy .. 2.5 soft)."""
    rng = np.random.default_rng(seed)
    freqs = np.meshgrid(*[np.fft.fftfreq(n) * n for n in shape], indexing='ij')
    k = np.sqrt(sum(f * f for f in freqs))
    k[(0,) * len(shape)] = 1.0
    amp = 1.0 / k ** beta
    amp[(0,) * len(shape)] = 0.0
    spec = amp * (rng.normal(size=shape) + 1j * rng.normal(size=shape))
    field = np.real(np.fft.ifftn(spec))
    field -= field.min()
    return field / max(field.max(), 1e-9)


def billow(shape, seed):
    """Cauliflower noise: folded octaves (|2n-1|), the look of billowing smoke."""
    out = np.zeros(shape)
    weight = 0.0
    for octave, (beta, w) in enumerate(((2.2, 1.0), (1.8, 0.55), (1.5, 0.3))):
        n = spectral_noise(shape, beta, seed + 17 * octave)
        out += w * (1.0 - np.abs(2.0 * n - 1.0))
        weight += w
    return out / weight


def smoothstep(e0, e1, x):
    t = np.clip((x - e0) / (e1 - e0), 0.0, 1.0)
    return t * t * (3.0 - 2.0 * t)


def grid(n):
    """Pixel centres in [-0.5, 0.5]: x right, y up."""
    c = (np.arange(n) + 0.5) / n - 0.5
    x, y = np.meshgrid(c, -c)
    return x, y


def lit_from_above(density, absorb):
    """Light falling from the top of the image through the density: bright tops, shadowed bottoms."""
    depth = np.cumsum(density, axis=0) / density.shape[0]
    return np.exp(-absorb * depth)


def to_u8(a):
    return (np.clip(a, 0.0, 1.0) * 255.0 + 0.5).astype(np.uint8)


def rgba(r, g, b, a):
    return np.dstack([to_u8(r), to_u8(g), to_u8(b), to_u8(a)])


def grey(v):
    return rgba(v, v, v, v)


def atlas(tiles, cols):
    rows = (len(tiles) + cols - 1) // cols
    h, w = tiles[0].shape[:2]
    out = np.zeros((rows * h, cols * w, 4), dtype=np.uint8)
    for i, t in enumerate(tiles):
        r, c = divmod(i, cols)
        out[r * h:(r + 1) * h, c * w:(c + 1) * w] = t
    return out


# ---------------------------------------------------------------- textures
def lobes(n, count_lo, count_hi, seed):
    """A closed outline's radius factor round the circle: a few big lobes and smaller ones on them (cauliflower)."""
    rng = np.random.default_rng(seed)
    x, y = grid(n)
    a = np.arctan2(y, x)
    f = np.zeros_like(a)
    for k in range(count_lo, count_hi + 1):
        f += rng.uniform(0.3, 1.0) / k * np.cos(k * a + rng.uniform(0, 2 * np.pi))
    return f / (np.abs(f).max() + 1e-9)


def smoke_puff(n, seed):
    """One puff (docs/fx/LOOK.md T1-T3, the art review's D3): fractal density inside a soft sphere whose outline is
    pushed in and out by the same fractal noise (no two or three big lobes: those read as hearts and cloud icons),
    filling the tile out to a radius of about 0.42 so a sprite shows at the size it is drawn, a soft silhouette, and
    gentle light from above (absorption 1.75 through the smooth body: the bottom keeps about a third of the top's
    light, so a cluster reads as one cloud, not as separate balls), modulated by the billows."""
    x, y = grid(n)
    r = np.sqrt(x * x + y * y)
    warp = spectral_noise((n, n), 2.0, seed * 5 + 1) - 0.5    # big soft folds of the outline
    fbm = spectral_noise((n, n), 1.6, seed * 7 + 3)
    b = billow((n, n), seed * 7 + 11)
    rr = r / 0.50 + 0.36 * warp + 0.25 * (fbm - 0.5)          # about 1 on the wandering outline
    body = smoothstep(1.15, 0.0, rr)                          # a sphere's thickness, for the light
    # The billows' soft, ragged edge, two fifths of the radius wide and broken by both noises (the second art review's
    # item 6: a fifth wide and sharpened again, the outlines were hard cut-outs, flattened where they met the tile's edge).
    rim = smoothstep(1.06, 0.64, rr + 0.36 * (b - 0.5) + 0.2 * (fbm - 0.5))
    density = rim * (0.55 + 0.30 * fbm + 0.40 * b)
    density = smoothstep(0.0, 0.8, density)
    density *= smoothstep(0.5, 0.40, r)                       # nothing reaches the tile's edge
    lit = lit_from_above(body, 1.75)
    lit = 0.35 + 0.65 * lit * (0.65 + 0.35 * b)
    thin = np.exp(-2.5 * density)
    erosion = spectral_noise((n, n), 1.3, seed * 13 + 5)
    return rgba(lit, thin, erosion, density)


def fireball_flipbook(n, cols, rows, seed):
    """An explosion seen from one side, frame by frame: a white-hot lobed ball swells, its hot pockets cool into dark,
    billowing smoke that rises and thins. R carries the temperature (the material colours it as a glowing body), A the
    density; the turbulence is one 3D field sliced in time, so the frames flow into each other."""
    frames = cols * rows
    shape = (frames, n, n)
    turb = spectral_noise(shape, 2.0, seed) - 0.5             # big slow folds
    bill = np.zeros(shape)
    for octave, (beta, w) in enumerate(((2.1, 1.0), (1.7, 0.5), (1.4, 0.25))):
        bill += w * (1.0 - np.abs(2.0 * spectral_noise(shape, beta, seed + 10 + octave) - 1.0))
    bill /= 1.75
    x, y = grid(n)
    tiles = []
    for f in range(frames):
        t = f / (frames - 1)
        radius = 0.09 + 0.35 * (1.0 - (1.0 - t) ** 2.2)
        rise = 0.07 * t * t
        # A gently wandering outline of many small lobes (the art review's D2: big lobes made polygons).
        edge = 1.0 + 0.06 * lobes(n, 6, 16, seed + 100 + f // 16)
        dist = np.sqrt(x * x + ((y - rise) * (1.0 + 0.15 * t)) ** 2)
        rho = dist / (radius * edge) + (0.16 + 0.10 * t) * turb[f]
        b = bill[f]
        density = smoothstep(1.0, 0.72, rho) * (0.45 + 0.75 * b)
        density = np.clip(density * (1.15 - 0.55 * t ** 1.4), 0.0, 1.0)
        density *= smoothstep(0.5, 0.44, np.sqrt(x * x + y * y))
        core = smoothstep(0.95, 0.0, rho)
        pockets = smoothstep(0.55, 0.95, b) * smoothstep(1.0, 0.4, rho)
        temp = (1.0 - t) ** 1.4 * core * (0.55 + 0.7 * b) + 0.7 * (1.0 - t) ** 2.4 * pockets
        temp = np.clip(temp, 0.0, 1.0) * (density > 0.02)
        lit = 0.15 + 0.85 * lit_from_above(density, 3.0) * (0.5 + 0.5 * b)
        tiles.append(rgba(temp, lit, b, density))
    return atlas(tiles, cols)


def glow(n):
    x, y = grid(n)
    r = np.sqrt(x * x + y * y) * 2.0
    v = np.exp(-(r / 0.22) ** 2) + 0.35 * np.exp(-r / 0.18) * smoothstep(1.0, 0.6, r)
    return grey(np.clip(v, 0.0, 1.0) * smoothstep(1.0, 0.85, r))


def flash(n, seed):
    """The detonation's flash for its first frame or two (LOOK 2.1, 3): a hot core and 6-12 uneven rays, not a soft
    dome (the art review's section 2)."""
    x, y = grid(n)
    r = np.sqrt(x * x + y * y) * 2.0
    a = np.arctan2(y, x)
    rng = np.random.default_rng(seed)
    rays = np.zeros_like(r)
    for _ in range(int(rng.integers(6, 13))):
        ang = rng.uniform(-np.pi, np.pi)
        length = rng.uniform(0.45, 0.95)
        width = rng.uniform(0.025, 0.06)
        d = np.angle(np.exp(1j * (a - ang)))                     # angle off the ray, -pi..pi
        across = np.abs(np.sin(d)) * r
        rays = np.maximum(rays, np.exp(-(across / width) ** 2) * (np.cos(d) > 0) * smoothstep(length, 0.0, r))
    core = np.exp(-(r / 0.16) ** 2) + 0.4 * np.exp(-(r / 0.32) ** 2)
    v = np.clip(core + 0.85 * rays, 0.0, 1.0) * smoothstep(1.0, 0.9, r)
    return grey(v)


def streak(w, h):
    """A tracer as the eye sees it: a hot head (right) and a fading tail, soft across."""
    u = (np.arange(w) + 0.5) / w
    v = (np.arange(h) + 0.5) / h - 0.5
    U, V = np.meshgrid(u, v)
    along = smoothstep(0.0, 0.85, U) * smoothstep(1.0, 0.93, U)
    head = np.exp(-((U - 0.9) / 0.05) ** 2)
    across = np.exp(-(V / 0.16) ** 2)
    core = np.exp(-(V / 0.05) ** 2)
    val = (0.55 * along + 0.6 * head) * across + 0.5 * along * core
    return grey(np.clip(val, 0.0, 1.0))


def spark(n):
    x, y = grid(n)
    r = np.sqrt(x * x + y * y) * 2.0
    star = np.exp(-np.abs(x * 2.0) / 0.02) * np.exp(-np.abs(y * 2.0) / 0.35) + \
        np.exp(-np.abs(y * 2.0) / 0.02) * np.exp(-np.abs(x * 2.0) / 0.35)
    v = np.exp(-(r / 0.12) ** 2) + 0.35 * star
    return grey(np.clip(v, 0.0, 1.0) * smoothstep(1.0, 0.8, r))


def ring(n, seed):
    """The shock ring and the dust ring a blast throws out along the ground."""
    x, y = grid(n)
    r = np.sqrt(x * x + y * y) * 2.0
    a = np.arctan2(y, x)
    noise = spectral_noise((n, n), 1.6, seed)
    wobble = 0.04 * np.sin(7 * a) + 0.06 * (noise - 0.5)
    # A thin ring only: no filled inside (the art review's section 2: the fill read as a white disc).
    v = np.exp(-((r - 0.78 - wobble) / 0.07) ** 2) * (0.7 + 0.5 * noise)
    return grey(np.clip(v, 0.0, 1.0) * smoothstep(1.0, 0.92, r))


def contrail(w, h, seed):
    """The ribbon's texture: U runs along the trail (tiles), V across it. A dense core filling about 70 % of the
    width with soft billowy edges (the first critic's R2: a trail is a body of smoke, not a glowing line); R the
    billows lit from above (M_FX_Ribbon shades by lerp(0.55, 1, R)), G thinness, B a blobby erosion field that is
    lowest at the thin edges (the trail breaks into puffs from its edges in, LOOK 9.3), A density."""
    v = (np.arange(h) + 0.5) / h - 0.5
    V = np.repeat(v[:, None], w, axis=1)
    bil = billow((h, w), seed)
    noise = spectral_noise((h, w), 1.8, seed + 3)
    edge = 0.35 + 0.08 * (spectral_noise((1, w), 2.0, seed + 9)[0] - 0.5)
    prof = smoothstep(edge[None, :] + 0.12, edge[None, :] - 0.12, np.abs(V) + 0.12 * (noise - 0.5))
    density = smoothstep(0.0, 0.9, prof * (0.55 + 0.45 * bil))
    density *= smoothstep(0.5, 0.44, np.abs(V))
    emboss = bil - np.roll(bil, 2, axis=0)
    lit = np.clip(0.30 + 0.70 * lit_from_above(density, 1.5) + 1.5 * emboss, 0.0, 1.0)
    thin = np.exp(-2.0 * density)
    blob = spectral_noise((h, w), 2.4, seed + 5)
    erode = np.clip(0.55 * blob + 0.45 * density, 0.0, 1.0)
    return rgba(lit, thin, erode, density)


def plume(w, h, seed):
    """A rocket motor's flame seen side on: the nozzle at the left, a bright core and faint shock diamonds."""
    u = (np.arange(w) + 0.5) / w
    v = (np.arange(h) + 0.5) / h - 0.5
    U, V = np.meshgrid(u, v)
    noise = spectral_noise((h, w), 1.5, seed)
    radius = 0.12 + 0.30 * U ** 0.7
    body = np.exp(-(V / radius) ** 2 * 2.0) * (1.0 - U) ** 1.4
    core = np.exp(-(V / (0.06 + 0.08 * U)) ** 2) * np.exp(-U / 0.25)
    diamonds = 0.25 * (0.5 + 0.5 * np.cos(U * 2 * np.pi * 6.0)) * np.exp(-U / 0.35) * np.exp(-(V / 0.07) ** 2)
    val = 0.6 * body * (0.7 + 0.6 * noise) + 0.9 * core + diamonds
    val *= smoothstep(0.0, 0.03, U)
    return grey(np.clip(val, 0.0, 1.0))


def afterburner(w, h, seed):
    """An afterburner's flame seen side on (LOOK 13), the nozzle at the left: a column about 0.8 of the sprite's width
    across at the nozzle (the nozzle's own width, not the rocket plume's flaring cone), holding its width for half its
    length and rounding off to nothing at the end, brightest in a core near the nozzle, with faint lengthwise flicker.
    The shock diamonds are drawn as their own knots (SimFxEffects.cpp), so none here."""
    u = (np.arange(w) + 0.5) / w
    v = (np.arange(h) + 0.5) / h - 0.5
    U, V = np.meshgrid(u, v)
    noise = spectral_noise((h, w), 1.6, seed)
    streaks = np.repeat(spectral_noise((h, 4), 1.4, seed + 1)[:, :1], w, axis=1)   # along its length: a function of V
    radius = 0.40 * np.sqrt(np.clip(1.0 - U ** 2.2, 0.0, 1.0)) + 0.01
    body = smoothstep(radius, radius * 0.55, np.abs(V) + 0.03 * (noise - 0.5))
    along = 0.45 * (1.0 - U) ** 0.7 + 0.55 * np.exp(-U / 0.25)
    core = np.exp(-(V / (0.03 + 0.14 * (1.0 - U))) ** 2) * np.exp(-U / 0.45)
    val = body * along * (0.8 + 0.25 * noise + 0.15 * (streaks - 0.5)) + 0.45 * core * body
    val *= smoothstep(0.0, 0.08, U)   # out of the nozzle, not a flat cut (a hard edge read as a cylinder's end)
    return grey(np.clip(val, 0.0, 1.0))


def scorch(n, seed):
    x, y = grid(n)
    r = np.sqrt(x * x + y * y) * 2.0
    a = np.arctan2(y, x)
    noise = spectral_noise((n, n), 1.5, seed)
    rays = 0.5 + 0.5 * spectral_noise((1, 256), 1.2, seed + 3)[0][((a + np.pi) / (2 * np.pi) * 255).astype(int)]
    mask = smoothstep(0.55 + 0.4 * rays, 0.1, r + 0.25 * (noise - 0.5))
    soot = 0.05 + 0.10 * noise
    return rgba(soot * 1.1, soot, soot * 0.85, mask)


def debris_atlas(n, cols, seed):
    """Chunks of metal, earth and wreckage: irregular polygons, lit from above."""
    rng = np.random.default_rng(seed)
    tiles = []
    for i in range(cols * cols):
        x, y = grid(n)
        a = np.arctan2(y, x)
        k = rng.integers(5, 9)
        radii = rng.uniform(0.18, 0.42, size=k)
        angles = np.sort(rng.uniform(0, 2 * np.pi, size=k))
        edge = np.interp(a % (2 * np.pi), np.concatenate([angles, [angles[0] + 2 * np.pi]]),
                         np.concatenate([radii, [radii[0]]]), period=2 * np.pi)
        r = np.sqrt(x * x + y * y)
        mask = smoothstep(edge, edge - 0.03, r)
        noise = spectral_noise((n, n), 1.3, seed + i)
        lit = 0.35 + 0.65 * smoothstep(-0.4, 0.4, y + 0.3 * (noise - 0.5))
        tiles.append(rgba(lit, np.full_like(lit, 0.2), noise, mask))
    return atlas(tiles, cols)


HEAT_RAMP = np.array([  # temperature 0..1 -> a burning fireball's colour (sRGB) and opacity: t, R, G, B, A
    # The hue of a black body (docs/fx/LOOK.md 0.1's table, sRGB: 1300 K #FF5800, 1800 K #FF8100, 2500 K #FFA64A,
    # 3000 K #FFB96E, 3500 K #FFC88C) with its low end going to soot (the art review's D2: a cooling fireball goes
    # dark, not bright red). The brightness is not here: M_FX_Smoke multiplies the ramp by temp^4, so the low end is
    # dark anyway; the ramp only colours. The fireball's R channel is a look, not a temperature in kelvin.
    [0.00, 0.08, 0.06, 0.04, 0.15],   # soot
    [0.15, 0.14, 0.08, 0.05, 0.45],   # soot, the faintest glow
    [0.32, 0.55, 0.12, 0.03, 0.80],   # dark red (~1100 K)
    [0.50, 1.00, 0.35, 0.00, 1.00],   # #FF5800, 1300 K
    [0.66, 1.00, 0.51, 0.00, 1.00],   # #FF8100, 1800 K
    [0.82, 1.00, 0.65, 0.29, 1.00],   # #FFA64A, 2500 K
    [1.00, 1.00, 0.78, 0.55, 1.00]])  # #FFC88C, 3500 K


def heat_colour(t):
    """The ramp's colour (sRGB, 0..1) at temperature t (an array): an (..., 3) stack."""
    return np.dstack([np.interp(t, HEAT_RAMP[:, 0], HEAT_RAMP[:, i]) for i in (1, 2, 3)])


def heat_ramp(w, h):
    """The fire material's heat ramp: U is the temperature 0..1 (pixel centres), every row the same. sRGB colour,
    A the ramp's opacity (the material reads the colour; the opacity is there for a material that wants it)."""
    u = (np.arange(w) + 0.5) / w
    rgb = heat_colour(u)[0]                                   # (w, 3)
    a = np.interp(u, HEAT_RAMP[:, 0], HEAT_RAMP[:, 4])
    row = rgba(rgb[None, :, 0], rgb[None, :, 1], rgb[None, :, 2], a[None, :])   # (1, w, 4)
    return np.repeat(row, h, axis=0)


TEXTURES = {
    # name: (builder, flipbook columns, rows) — the materials' SubUV layout
    'T_FX_SmokeAtlas': (lambda: atlas([smoke_puff(256, 100 + i) for i in range(16)], 4), 4, 4),
    'T_FX_Fireball': (lambda: fireball_flipbook(128, 8, 8, 400), 8, 8),
    'T_FX_HeatRamp': (lambda: heat_ramp(256, 4), 1, 1),
    'T_FX_Glow': (lambda: glow(128), 1, 1),
    'T_FX_Flash': (lambda: flash(256, 1300), 1, 1),
    'T_FX_Streak': (lambda: streak(256, 32), 1, 1),
    'T_FX_Spark': (lambda: spark(64), 1, 1),
    'T_FX_Ring': (lambda: ring(256, 700), 1, 1),
    'T_FX_Contrail': (lambda: contrail(512, 128, 800), 1, 1),
    'T_FX_Plume': (lambda: plume(256, 64, 900), 1, 1),
    'T_FX_Afterburner': (lambda: afterburner(256, 64, 950), 1, 1),
    'T_FX_Scorch': (lambda: scorch(256, 1000), 1, 1),
    'T_FX_DebrisAtlas': (lambda: debris_atlas(64, 4, 1100), 4, 4),
}

# Colour textures (sRGB on when imported); every other texture is packed data (sRGB off).
SRGB = ('T_FX_HeatRamp', 'T_FX_Scorch')


def decoded(name, img):
    """The texture as its material draws it (sunlit smoke, a glowing fireball), for the preview."""
    f = img.astype(np.float64) / 255.0
    r, g, a = f[..., 0], f[..., 1], f[..., 3]
    if name in ('T_FX_SmokeAtlas', 'T_FX_Contrail', 'T_FX_DebrisAtlas'):
        shade = 0.35 + 0.65 * r if name != 'T_FX_DebrisAtlas' else 0.12 + 0.3 * r
        rgb = np.dstack([shade, shade, shade * 1.02])
    elif name == 'T_FX_Fireball':
        smoke = np.dstack([0.08 + 0.3 * g] * 3)
        heat = heat_colour(r)
        glow_w = np.clip(r * 2.5, 0.0, 1.0)[..., None]
        rgb = smoke * (1.0 - glow_w) + heat * glow_w * 1.4
    elif name in ('T_FX_Scorch', 'T_FX_HeatRamp'):
        rgb = f[..., :3]
    else:
        tint = {'T_FX_Streak': (1.0, 0.55, 0.25), 'T_FX_Plume': (1.0, 0.75, 0.45)}.get(name, (1.0, 0.95, 0.85))
        rgb = np.dstack([r * tint[0], r * tint[1], r * tint[2]]) * 1.3
    return np.dstack([to_u8(rgb[..., 0]), to_u8(rgb[..., 1]), to_u8(rgb[..., 2]), to_u8(a)])


def preview(images, path):
    """Every texture as its material draws it, over a sky blue, for a quick look."""
    tiles = []
    for name, img in images.items():
        im = Image.fromarray(decoded(name, img), 'RGBA')
        if im.height < 32:   # a ramp a few pixels high: stretch it to be seen
            im = im.resize((im.width, 48), Image.NEAREST)
        im.thumbnail((256, 256))
        bg = Image.new('RGBA', (256, 256), (90, 100, 120, 255))
        bg.alpha_composite(im, ((256 - im.width) // 2, (256 - im.height) // 2))
        tiles.append(np.asarray(bg))
    while len(tiles) % 5:   # the last row's empty cells in the background colour too
        tiles.append(np.asarray(Image.new('RGBA', (256, 256), (90, 100, 120, 255))))
    sheet = atlas(tiles, 5)
    os.makedirs(os.path.dirname(path), exist_ok=True)
    Image.fromarray(sheet, 'RGBA').save(path)


def main():
    ap = argparse.ArgumentParser(description=' '.join(__doc__.split('\n\n')[0].split()))
    ap.add_argument('--out', default=os.path.join(REPO, 'art', 'fx', 'textures'))
    ap.add_argument('--preview', default=os.path.join(REPO, 'out', 'fx_textures_preview.png'))
    args = ap.parse_args()
    os.makedirs(args.out, exist_ok=True)
    images, layout = {}, {}
    for name, (build, cols, rows) in TEXTURES.items():
        img = build()
        Image.fromarray(img, 'RGBA').save(os.path.join(args.out, name + '.png'))
        images[name] = img
        layout[name] = {'cols': cols, 'rows': rows, 'width': img.shape[1], 'height': img.shape[0]}
        print('%-18s %4d x %-4d  %d x %d frames' % (name, img.shape[1], img.shape[0], cols, rows))
    with open(os.path.join(args.out, 'layout.json'), 'w', encoding='utf-8') as f:
        json.dump(layout, f, indent=2)
        f.write('\n')
    if args.preview:
        preview(images, args.preview)
        print('preview', args.preview)


if __name__ == '__main__':
    main()
