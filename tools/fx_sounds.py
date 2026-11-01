"""Make the weapons' and aircraft's sounds (docs/fx/README.md, "Sound"): explosions near and far, jet roar, whine and
afterburner, rocket motors, gun bursts, sonic booms, cluster ripples, fire and the rest.

    python tools/fx_sounds.py [--out art/fx/sounds] [--only NAME ...] [--spectrograms [DIR]] [--check]

Every sample is synthesised here from seeded noise (numpy only), so the project owns them and a rerun gives the same
files: 48 kHz, 16-bit, mono (the game places each sound in the world itself). A sound's loudness in the world is not in
the file: every file peaks at -1 dBFS, and the level at its reference distance is in data/fx/effects.json; the game
delays, Doppler-shifts, filters and mixes it (simcore/fx/acoustics.hpp). The list of files, which of them loop and
their lengths is written beside them (sounds.json; tools/ue_make_fx_assets.py sets looping from it).

Loops are made as one period of a periodic signal, so the last sample runs into the first with nothing to hide: noise is
filtered circularly, tones sit on whole cycles of the loop, and every event (a round, a blade's slap, a crackle) that
runs past the end comes in again at the start. No crossfade, so no dip, no doubled round and no beating at the seam.

How they are made (the techniques of procedural audio, A. Farnell, "Designing Sound", MIT Press 2010):
  multiband envelopes  noise is split into octave bands once (zero-phase, so no envelope is smeared) and each band gets
                       its own envelope: an explosion's crack dies in milliseconds, its boom in seconds
  Friedlander pulse    the blast wave's own shape, p(t) = (1 - t/T) exp(-b t/T): a gun's report, a bomb's shock
  N-wave               a sonic boom: a jump up, a straight fall through zero, a jump back (both shocks compressions)
  crackle              rocket and afterburner exhaust is skewed: sharp positive spikes on the roar
  outdoor echoes       a few late, low-passed copies (a treeline, a hillside): the rolling of a far explosion

--check measures the files in --out against the rules they keep (format, level, DC, length, seamless loops, onsets
and decays, far duller than near, the boom's N, the guns' rates) and prints a table; it exits 1 if any fails.
"""
import argparse
import json
import os
import sys
import wave
from collections import namedtuple

import numpy as np

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SR = 48000


# ---------------------------------------------------------------- building blocks
def rng_for(name, salt=0):
    return np.random.default_rng(abs(hash_name(name)) + salt)


def hash_name(name):
    """FNV-1a of the name: a seed that is the same on every run and machine (Python's hash() is not)."""
    h = 2166136261
    for ch in name.encode('utf-8'):
        h = ((h ^ ch) * 16777619) & 0xFFFFFFFF
    return h


def seconds(n):
    return int(round(n * SR))


def time_axis(n):
    return np.arange(n) / SR


def rms(x):
    return float(np.sqrt(np.mean(np.square(x)))) if len(x) else 0.0


def fit(x, n):
    """x cut or padded with silence to n samples."""
    return x[:n] if len(x) >= n else np.concatenate([x, np.zeros(n - len(x))])


def delay(x, s):
    """x later by s seconds (silence before it), the same length."""
    k = seconds(s)
    return np.concatenate([np.zeros(k), x[:len(x) - k]]) if k > 0 else x


def wrap_round(x, n):
    """Fold a signal onto n samples: what runs past the end comes in again at the start (one period of a loop)."""
    y = np.zeros(n)
    for i in range(0, len(x), n):
        seg = x[i:i + n]
        y[:len(seg)] += seg
    return y


def spectrum_filter(x, gain_fn, circular=False):
    """Filter by a zero-phase magnitude response gain_fn(f) (linear), on the whole signal at once. circular: x is one
    period of a loop and the result is one too; otherwise x is padded with silence before and after."""
    n = len(x)
    size = n if circular else 1 << int(np.ceil(np.log2(max(2 * n, 2))))
    spec = np.fft.rfft(x, size)
    f = np.fft.rfftfreq(size, 1.0 / SR)
    spec *= gain_fn(f)
    return np.fft.irfft(spec, size)[:n]


def lowpass(x, fc, order=2, circular=False):
    return spectrum_filter(x, lambda f: 1.0 / np.sqrt(1.0 + (f / fc) ** (2 * order)), circular)


def highpass(x, fc, order=2, circular=False):
    return spectrum_filter(x, lambda f: 1.0 / np.sqrt(1.0 + (fc / np.maximum(f, 1e-3)) ** (2 * order)), circular)


def bandpass(x, lo, hi, order=2, circular=False):
    return highpass(lowpass(x, hi, order, circular), lo, order, circular)


def causal_smooth(x, tau_s):
    """A first-order (RC) low-pass that only looks back: a jump rises over ~2.2 tau (10-90 %) and nothing moves before
    it, as a shock's front does (a zero-phase filter would start the rise early). The kernel is cut at 10 tau."""
    n = len(x)
    k = max(int(10 * tau_s * SR), 1)
    h = np.exp(-np.arange(k) / max(tau_s * SR, 1e-9))
    h /= h.sum()
    size = 1 << int(np.ceil(np.log2(max(n + k, 2))))
    return np.fft.irfft(np.fft.rfft(x, size) * np.fft.rfft(h, size), size)[:n]


def dc_block(x, fc=5.0):
    """A causal first-order high-pass at fc (x less its RC-smoothed self): a one-shot's DC (a blast's net impulse, a
    clipped peak's asymmetry) bleeds away within ~1/(2 pi fc) of where it arises, so the silence after a sound is
    silence and not an offset (taking the mean out of the whole file would leave one in its tail)."""
    return x - causal_smooth(x, 1.0 / (2.0 * np.pi * fc))


def shaped_noise(n, rng, shape_db, circular=False):
    """Gaussian noise with a spectral envelope shape_db(f) (dB)."""
    return spectrum_filter(rng.normal(size=n), lambda f: 10.0 ** (shape_db(np.maximum(f, 1.0)) / 20.0), circular)


OCTAVES = np.array([31.5, 63, 125, 250, 500, 1000, 2000, 4000, 8000, 16000])


def octave_weights(f):
    """The octave bands' gains at frequencies f: raised-cosine crossovers in log frequency that sum to one."""
    lf = np.log2(np.maximum(f, 1.0))
    centres = np.log2(OCTAVES)
    weights = []
    for i, c in enumerate(centres):
        w = np.zeros_like(lf)
        lo = centres[i - 1] if i > 0 else -np.inf
        hi = centres[i + 1] if i + 1 < len(centres) else np.inf
        rising = (lf >= lo) & (lf <= c)
        falling = (lf > c) & (lf <= hi)
        w[rising] = 1.0 if i == 0 else 0.5 - 0.5 * np.cos(np.pi * (lf[rising] - lo) / (c - lo))
        w[falling] = 1.0 if i + 1 == len(centres) else 0.5 + 0.5 * np.cos(np.pi * (lf[falling] - c) / (hi - c))
        weights.append(w)
    return weights


def octave_bands(x):
    """Split into octave bands whose responses sum to one (raised-cosine crossovers in log frequency)."""
    n = len(x)
    size = 1 << int(np.ceil(np.log2(max(2 * n, 2))))
    spec = np.fft.rfft(x, size)
    return [np.fft.irfft(spec * w, size)[:n] for w in octave_weights(np.fft.rfftfreq(size, 1.0 / SR))]


def envelope(n, attack_s, decay_s, hold_s=0.0, curve=1.0):
    """Linear attack, hold, then an exponential decay to -60 dB in decay_s (curve > 1 bends it: slower first)."""
    t = time_axis(n)
    env = np.ones(n)
    if attack_s > 0:
        env = np.minimum(env, t / attack_s)
    after = np.maximum(t - attack_s - hold_s, 0.0)
    env *= np.exp(-6.91 * (after / max(decay_s, 1e-4)) ** curve)
    return env


def friedlander(n, positive_s, b=1.6, start=0):
    """The blast wave's pressure: an instant rise, (1 - t/T) exp(-b t/T) through a shallow negative phase."""
    t = np.maximum(time_axis(n) - start / SR, 0.0)
    p = (1.0 - t / positive_s) * np.exp(-b * t / positive_s)
    p[time_axis(n) < start / SR] = 0.0
    return p


def slow_wobble(n, rng, rate_hz, depth_db, circular=False):
    """A slow random level change (turbulence, a fire's breathing): +-depth_db at about rate_hz."""
    w = lowpass(rng.normal(size=n), rate_hz, order=2, circular=circular)
    w -= w.mean()
    w /= max(np.abs(w).max(), 1e-9)
    return 10.0 ** (depth_db * w / 20.0)


def crackle(n, rng, rate_hz, decay_ms, skew=1.0, circular=False, first=None):
    """Sharp positive spikes at random (Poisson) times with random sizes: the crackle of a rocket or afterburner.
    first: a spike of that size at the very start as well."""
    out = np.zeros(n)
    count = rng.poisson(rate_hz * n / SR)
    at = rng.integers(0, n, size=count)
    size = rng.pareto(2.5, size=count) + 0.2
    np.add.at(out, at, size)
    if first is not None:
        out[0] += first
    k = seconds(decay_ms * 6 / 1000.0)
    kernel = np.exp(-np.arange(k) / (decay_ms / 1000.0 * SR)) - skew * 0.3 * np.exp(-np.arange(k) / (decay_ms / 300.0 * SR))
    full = np.convolve(out, kernel)
    return wrap_round(full, n) if circular else full[:n]


def echoes(x, rng, count, first_s, last_s, gain, fc):
    """Late outdoor reflections (a hillside, a treeline, a town): low-passed, quieter, spread in time. Each copy is
    delayed before it is filtered, so its front is rounded too (filtering it cut off at its start leaves a step: a
    click at every echo)."""
    n = len(x)
    out = x.copy()
    for i in range(count):
        d = seconds(rng.uniform(first_s, last_s))
        if d >= n:
            continue
        g = gain * rng.uniform(0.5, 1.0) * (1.0 - 0.5 * i / max(count, 1))
        out += g * lowpass(np.concatenate([np.zeros(d), x[:n - d]]), fc * rng.uniform(0.7, 1.3))
    return out


def outdoor_tail(n, rng, decay_s, fc):
    """A diffuse outdoor tail: noise decaying through a low-pass (the ground's and trees' late scatter)."""
    return lowpass(rng.normal(size=n), fc) * envelope(n, 0.02, decay_s, curve=0.8)


def soft_clip(x, drive):
    return np.tanh(drive * x) / np.tanh(drive)


def normalise(x, peak_db=-1.0):
    """Scaled so its peak is at peak_db (dB re full scale)."""
    peak = np.abs(x).max()
    return x * (10 ** (peak_db / 20.0) / peak) if peak > 0 else x


def fade(x, in_s=0.0, out_s=0.0):
    n = len(x)
    if in_s > 0:
        k = min(seconds(in_s), n)
        x[:k] *= np.linspace(0.0, 1.0, k)
    if out_s > 0:
        k = min(seconds(out_s), n)
        x[n - k:] *= np.linspace(1.0, 0.0, k) ** 2
    return x


def finish_one_shot(x, fade_out_s, dc_hz=5.0):
    """A one-shot's last step: its DC blocked (dc_block; None for a sound whose shape is its point, the boom's N, which
    has none), faded to exact silence over fade_out_s, then its peak put at -1 dBFS."""
    if dc_hz:
        x = dc_block(x, dc_hz)
    return normalise(fade(x, 0.0, fade_out_s))


def finish_loop(x):
    """A loop's last step: its mean taken out (exact for one period of a periodic signal), its peak at -1 dBFS."""
    return normalise(x - np.mean(x))


def write_wav(path, x):
    if not np.all(np.isfinite(x)):
        raise ValueError('%s: not finite' % path)
    pcm = np.clip(np.round(x * 32767.0), -32768, 32767).astype('<i2')
    with wave.open(path, 'wb') as w:
        w.setnchannels(1)
        w.setsampwidth(2)
        w.setframerate(SR)
        w.writeframes(pcm.tobytes())


def read_wav(path):
    """(samples as floats in -1..1, channels, sample width in bytes, rate)."""
    with wave.open(path, 'rb') as w:
        ch, width, rate, frames = w.getnchannels(), w.getsampwidth(), w.getframerate(), w.getnframes()
        raw = w.readframes(frames)
    x = np.frombuffer(raw, dtype='<i2').astype(np.float64) / 32767.0 if width == 2 else np.zeros(0)
    return x, ch, width, rate


# ---------------------------------------------------------------- explosions
def explosion(name, charge_kg, ground=True, distant=False):
    """A detonation of `charge_kg` of explosive, heard close (with its crack, its boom and the earth coming down) or far
    (no crack: a dull thud, then the boom low and rolling back off the country round about). Sizes scale by the cube
    root of the charge (Hopkinson-Cranz): the blast's positive phase, the boom's pitch and its length."""
    rng = rng_for(name)
    scale = charge_kg ** (1.0 / 3.0)               # 15 kg: 2.5, 250 kg: 6.3, 900 kg: 9.7
    length = (3.0 + 0.35 * scale) * (1.6 if distant else 1.0)
    n = seconds(length)
    out = np.zeros(n)
    # The boom: octave bands of noise, each with its own attack and decay; low bands longer, high bands shorter.
    peak_hz = 140.0 / scale * (0.7 if distant else 1.0)  # the spectrum's peak falls as the charge grows
    bands = octave_bands(rng.normal(size=n))
    for fc, band in zip(OCTAVES, bands):
        level_db = -9.0 * np.log2(fc / peak_hz) ** 2 / 2.2
        if fc > peak_hz:
            level_db = -5.5 * np.log2(fc / peak_hz)
        if distant:
            level_db -= 3.5 * max(np.log2(fc / 250.0), 0.0) ** 1.5
        decay = (0.25 + 0.16 * scale) * (250.0 / fc) ** 0.45 * (1.6 if distant else 1.0)
        attack = 0.0015 if fc > 1000 else 0.004 + 0.002 * scale
        if distant:
            attack *= 3.0     # the front rounded by the way, still an instant to the ear
        out += band * 10.0 ** (level_db / 20.0) * envelope(n, attack, decay, hold_s=0.01 * scale, curve=0.85)
    out /= max(np.abs(out).max(), 1e-9)
    t = time_axis(n)
    if not distant:
        # The shock itself: a Friedlander pulse, its positive phase a few milliseconds long, gently rounded.
        shock = friedlander(n, 0.0012 * scale, b=1.4)
        shock = lowpass(shock, 7000.0 / np.sqrt(scale), order=1)
        out += 1.6 * shock / max(np.abs(shock).max(), 1e-9)
        # The sub-bass thump: a short falling sine.
        f0 = 55.0 / np.sqrt(scale / 2.5)
        thump = np.sin(2 * np.pi * np.cumsum(f0 * (1.0 + 0.8 * np.exp(-t / 0.04))) / SR) * envelope(n, 0.003, 0.35 + 0.05 * scale)
        out += 0.9 * thump
    else:
        # Far off the shock arrives as a dull thud: the same pulse, longer and low-passed by the air and the ground.
        shock = lowpass(friedlander(n, 0.004 * scale, b=1.4), 400.0 / np.sqrt(scale), order=2)
        out += 0.8 * shock / max(np.abs(shock).max(), 1e-9)
    # The rumble: the air and the ground ringing on, low and long.
    out += (0.35 if not distant else 0.6) * outdoor_tail(n, rng, length * 0.6, 180.0 if not distant else 120.0) \
        * slow_wobble(n, rng, 3.0, 4.0)
    if ground and not distant:
        # The earth and the fragments coming down: sparse little impacts from 0.3 s, thinning out.
        t0 = seconds(0.3)
        debris = crackle(n - t0, rng, 60.0 + 10.0 * scale, 2.0, skew=0.0)
        debris = bandpass(debris, 300.0, 5000.0) * envelope(n - t0, 0.2, 1.2 + 0.25 * scale)
        out[t0:] += 0.25 * debris / max(np.abs(debris).max(), 1e-9)
    if distant:
        out = echoes(out, rng, 5, 0.4, length * 0.45, 0.45, 220.0)
    else:
        out = echoes(out, rng, 3, 0.25, 1.5, 0.25, 900.0)
    out = soft_clip(normalise(out, 0.0), 1.8 if not distant else 1.2)
    return finish_one_shot(out, 0.4)


def airburst(name, charge_kg):
    """A SAM's warhead going off in the air: a hard crack and a short boom, no earth thrown up, the fragments' whine."""
    rng = rng_for(name)
    scale = charge_kg ** (1.0 / 3.0)
    n = seconds(2.5 + 0.3 * scale)
    out = np.zeros(n)
    bands = octave_bands(rng.normal(size=n))
    peak_hz = 220.0 / scale
    for fc, band in zip(OCTAVES, bands):
        level_db = -6.0 * abs(np.log2(fc / peak_hz)) ** 1.3
        decay = (0.15 + 0.1 * scale) * (250.0 / fc) ** 0.5
        out += band * 10.0 ** (level_db / 20.0) * envelope(n, 0.001, decay, curve=0.8)
    out /= max(np.abs(out).max(), 1e-9)
    shock = lowpass(friedlander(n, 0.0009 * scale, b=1.2), 9000.0, order=1)
    out += 2.0 * shock / max(np.abs(shock).max(), 1e-9)
    out += 0.3 * outdoor_tail(n, rng, 1.8, 250.0)
    out = echoes(out, rng, 3, 0.3, 1.6, 0.3, 700.0)
    return finish_one_shot(soft_clip(normalise(out, 0.0), 2.2), 0.3)


def cluster_ripple(name, bomblets=200, spread_s=1.5):
    """A cluster bomb's load landing: a ripple of small detonations over a second or two ("popcorn"), spread evenly
    over spread_s as the picture lays its carpet (effects.json bombs.cluster_spread_s 1.5 s, uniform)."""
    rng = rng_for(name)
    n = seconds(spread_s + 2.5)
    out = np.zeros(n)
    pop = fade(airburst(name + '_pop', 0.3)[:seconds(1.0)], 0.0, 0.3)    # cut short, faded: no click at its end
    times = np.sort(rng.uniform(0.0, spread_s, size=bomblets))
    times -= times[0]                                # the file starts with the first bomblet
    for i_pop, t0 in enumerate(times):
        i = seconds(t0)
        if i + len(pop) >= n:
            continue
        g = 0.6 if i_pop == 0 else rng.uniform(0.15, 0.6)
        k = rng.uniform(0.85, 1.2)
        idx = np.clip((np.arange(len(pop)) * k).astype(int), 0, len(pop) - 1)
        out[i:i + len(pop)] += g * pop[idx]
    out += 0.3 * outdoor_tail(n, rng, n / SR * 0.8, 200.0) * np.clip(np.cumsum(np.abs(out)) / max(np.abs(out).sum(), 1e-9) * 3, 0, 1)
    return finish_one_shot(soft_clip(normalise(out, 0.0), 1.5), 0.5)


# ---------------------------------------------------------------- the sound barrier
def n_wave(n, duration_s, start_s, rise_s):
    """The boom's pressure: from zero a jump to +1 at start_s, a straight fall to -1 over duration_s, a jump back to
    zero; both jumps rise over rise_s (10-90 %), the front's thickness in turbulent air."""
    u = (time_axis(n) - start_s) / duration_s
    p = np.where((u >= 0) & (u <= 1), 1.0 - 2.0 * u, 0.0)
    return causal_smooth(p, rise_s / 2.2)


def sonic_boom(name, duration_s):
    """An N-wave (n_wave): a fighter's lasts ~0.1-0.2 s, its shocks rising in a few milliseconds; then the ground's and
    buildings' rattle and the rumble rolling away."""
    rng = rng_for(name)
    n = seconds(duration_s + 2.5)
    start = 0.001
    p = n_wave(n, duration_s, start, 0.002 + 0.008 * duration_s)
    # the rattle and the rumble are noise: their chance DC is blocked so the N, which has none, keeps its shape
    rattle = delay(dc_block(bandpass(rng.normal(size=n), 120.0, 2500.0) * envelope(n, 0.01, 0.5, curve=0.8)), start)
    rumble = delay(dc_block(outdoor_tail(n, rng, 2.0, 90.0) * slow_wobble(n, rng, 2.0, 5.0)), start + duration_s)
    out = 1.0 * p / np.abs(p).max() + 0.12 * rattle / np.abs(rattle).max() + 0.35 * rumble / np.abs(rumble).max()
    out = echoes(out, rng, 3, 0.25 + duration_s, 1.2, 0.35, 400.0)
    return finish_one_shot(out, 0.5, dc_hz=None)


# ---------------------------------------------------------------- engines (loops)
def loop_tones(n, freqs_hz):
    """Frequencies moved to the nearest whole number of cycles in n samples, so each tone loops (a shift of at most
    half a cycle per loop: 0.1 Hz in a 5 s loop)."""
    length_s = n / SR
    return [max(round(f * length_s), 1) / length_s for f in freqs_hz]


def jet_roar(name, peak_hz, harsh_db, length_s=6.0):
    """The exhaust's mixing noise, heard from behind: a broad hump of noise (peak_hz 150-400 Hz), with the shock
    noise of a hot, fast jet riding on it (harsh_db) and slow turbulent swells. Loops."""
    rng = rng_for(name)
    n = seconds(length_s)
    lp = np.log2(peak_hz)

    def shape(f):
        lf = np.log2(f)
        hump = np.where(lf < lp, -10.0 * np.abs(lp - lf) ** 1.2, -6.5 * (lf - lp))
        shock = harsh_db * np.exp(-((lf - np.log2(1600.0)) / 1.1) ** 2)
        return hump + shock
    x = shaped_noise(n, rng, shape, circular=True)
    x *= slow_wobble(n, rng, 1.5, 1.5, circular=True) * slow_wobble(n, rng, 7.0, 0.8, circular=True)
    return finish_loop(x)


def jet_whine(name, shaft_hz, blade_pass_hz, length_s=4.0):
    """The front of a jet engine: the fan's blade-pass tone and its harmonics, and the buzz-saw tones (the shaft's
    harmonics, from supersonic fan tips), wavering a little together, over a thin hiss. Loops."""
    rng = rng_for(name)
    n = seconds(length_s)
    # The engine's speed wanders by about 0.2 % (zero mean over the loop, so every tone still ends on a whole cycle).
    w = lowpass(rng.normal(size=n), 0.7, circular=True)
    w = (w - w.mean()) / max(np.std(w), 1e-12)
    wander = 1.0 + 0.002 * w
    phase_base = 2 * np.pi * (np.cumsum(wander) - wander[0]) / SR
    x = np.zeros(n)
    blade = loop_tones(n, [blade_pass_hz * k for k in (1, 2, 3)])
    for f, amp in zip(blade, (1.0, 0.35, 0.12)):
        x += amp * np.sin(f * phase_base + rng.uniform(0, 2 * np.pi))
    for f in loop_tones(n, [shaft_hz * h for h in range(6, 28)]):
        x += rng.uniform(0.02, 0.12) * np.sin(f * phase_base + rng.uniform(0, 2 * np.pi))
    hiss = bandpass(rng.normal(size=n), blade_pass_hz * 0.7, blade_pass_hz * 1.5, circular=True)
    x = x / np.abs(x).max() + 0.25 * hiss / max(np.abs(hiss).max(), 1e-9)
    x *= slow_wobble(n, rng, 2.0, 1.0, circular=True)
    return finish_loop(x)


def afterburner(name, length_s=4.0):
    """An afterburner's roar: lower and rougher than military power, crackling and popping. Loops."""
    rng = rng_for(name)
    n = seconds(length_s)
    roar = shaped_noise(n, rng, lambda f: np.where(f < 120, -8 * np.log2(120 / f), -5.5 * np.log2(f / 120)), circular=True)
    crack = highpass(crackle(n, rng, 160.0, 0.6, circular=True), 400.0, circular=True)
    x = roar / np.abs(roar).max() + 0.45 * crack / np.abs(crack).max()
    x *= slow_wobble(n, rng, 4.0, 2.0, circular=True)
    return finish_loop(soft_clip(x / np.abs(x).max(), 1.5))


def rocket_motor(name, peak_hz, length_s=3.0):
    """A solid rocket motor's exhaust: a harsh broadband roar peaking high (a small, very fast jet) with heavy crackle.
    Loops."""
    rng = rng_for(name)
    n = seconds(length_s)
    lp = np.log2(peak_hz)
    roar = shaped_noise(n, rng, lambda f: np.where(np.log2(f) < lp, -9.0 * (lp - np.log2(f)), -4.5 * (np.log2(f) - lp)),
                        circular=True)
    crack = highpass(crackle(n, rng, 400.0, 0.35, circular=True), 600.0, circular=True)
    x = roar / np.abs(roar).max() + 0.6 * crack / np.abs(crack).max()
    x *= slow_wobble(n, rng, 9.0, 1.5, circular=True)
    return finish_loop(soft_clip(x / np.abs(x).max(), 2.0))


def propeller(name, blade_hz, harmonics=12, length_s=4.0):
    """A propeller or rotor: the blade-pass fundamental and its harmonics, each blade's slap a little different, over
    the engine's roar. Loops: a whole number of blade passes."""
    rng = rng_for(name)
    count = max(int(round(length_s * blade_hz)), 1)
    n = int(round(count * SR / blade_hz))
    period = n / count
    pulses = np.zeros(n)
    for i in range(count):
        at = int(round(i * period + rng.normal(0.0, 0.002) * period)) % n
        pulses[at] += rng.uniform(0.85, 1.15)
    k = seconds(0.02)
    kernel = np.exp(-np.arange(k) / (0.0025 * SR)) * np.sin(2 * np.pi * 180.0 * np.arange(k) / SR)
    slap = wrap_round(np.convolve(pulses, kernel), n)
    slap = lowpass(slap, blade_hz * harmonics, circular=True)
    engine = shaped_noise(n, rng, lambda f: -6.0 * np.abs(np.log2(f / 300.0)), circular=True)
    x = slap / np.abs(slap).max() + 0.3 * engine / np.abs(engine).max()
    return finish_loop(x)


def fire(name, length_s=6.0):
    """A burning wreck: a low breathing roar and the crackle of things burning. Loops."""
    rng = rng_for(name)
    n = seconds(length_s)
    roar = lowpass(rng.normal(size=n), 250.0, circular=True) * slow_wobble(n, rng, 0.8, 4.0, circular=True)
    crack = bandpass(crackle(n, rng, 25.0, 1.2, circular=True), 500.0, 6000.0, circular=True)
    x = roar / np.abs(roar).max() + 0.5 * crack / np.abs(crack).max()
    return finish_loop(x)


# ---------------------------------------------------------------- guns
def gun_shot(rng, calibre_mm, n):
    """One round's report: the muzzle blast (a Friedlander pulse, its positive phase growing with the calibre), the
    gun's body ringing briefly, and the mechanism's clank."""
    scale = calibre_mm / 23.0
    blast = lowpass(friedlander(n, 0.0011 * scale, b=1.3), 9000.0 / scale, order=1)
    body = bandpass(rng.normal(size=n), 120.0 / scale, 3500.0) * envelope(n, 0.0008, 0.05 * scale ** 0.7)
    clank = bandpass(rng.normal(size=n), 1500.0, 6000.0) * envelope(n, 0.0005, 0.012)
    return blast / np.abs(blast).max() + 0.45 * body / np.abs(body).max() + 0.08 * clank / np.abs(clank).max()


def gun_burst_loop(name, calibre_mm, rounds_per_s, length_s=3.0):
    """A gun or a mount's barrels firing on: one report per round at the cyclic rate (each round a little early or
    late, the barrels never quite even), the reports overlapping into the buzz of an automatic cannon, over the steady
    rumble of their echoes. Loops: a whole number of rounds, the last ones' reports running on into the first."""
    rng = rng_for(name)
    count = max(int(round(length_s * rounds_per_s)), 1)
    n = int(round(count * SR / rounds_per_s))
    period = n / count
    shot_n = seconds(0.25)
    shots = [gun_shot(rng, calibre_mm, shot_n) for _ in range(6)]
    out = np.zeros(n + 2 * shot_n)
    for i in range(count):
        at = int(round(i * period + rng.uniform(-0.04, 0.04) * period)) % n
        out[at:at + shot_n] += rng.uniform(0.75, 1.0) * shots[rng.integers(len(shots))]
    out = wrap_round(out, n)
    bed = lowpass(rng.normal(size=n), 600.0, circular=True) * slow_wobble(n, rng, 1.0, 2.0, circular=True)
    out += 0.3 * rms(out) * bed / max(rms(bed), 1e-12)
    return finish_loop(soft_clip(out / np.abs(out).max(), 1.6))


def gun_burst_tail(name, calibre_mm):
    """What follows the last round: the echo of the burst off the country, rolling away."""
    rng = rng_for(name)
    n = seconds(2.5)
    x = gun_shot(rng, calibre_mm, n) * 0.4
    x += 0.8 * outdoor_tail(n, rng, 1.8, 700.0)
    x = echoes(x, rng, 4, 0.15, 1.2, 0.5, 600.0)
    return finish_one_shot(x, 0.3)


def gun_single(name, calibre_mm):
    """A heavy gun's single report (57 mm): the blast, the body and the outdoor echo."""
    rng = rng_for(name)
    n = seconds(2.0)
    x = gun_shot(rng, calibre_mm, n)
    x += 0.2 * outdoor_tail(n, rng, 1.4, 500.0)
    x = echoes(x, rng, 3, 0.2, 1.0, 0.3, 700.0)
    return finish_one_shot(soft_clip(x / np.abs(x).max(), 1.4), 0.3)


def bullet_crack(name):
    """A supersonic round passing close: its own little N-wave (a sharp snap, 0.4 ms), no body."""
    rng = rng_for(name)
    n = seconds(0.4)
    p = n_wave(n, 0.0004, 0.0005, 0.00005)
    tail = bandpass(rng.normal(size=n), 800.0, 6000.0) * envelope(n, 0.0005, 0.08)
    return finish_one_shot(p / np.abs(p).max() + 0.1 * tail / np.abs(tail).max(), 0.05)


# ---------------------------------------------------------------- one-shots
def missile_launch(name):
    """A SAM leaving its launcher: the igniter's crack, the motor's roar coming up in a quarter second, a few seconds of
    the boost going away (the game then plays the motor's loop, Doppler-shifted, where the missile is)."""
    rng = rng_for(name)
    n = seconds(3.5)
    t = time_axis(n)
    roar = rocket_motor(name + '_roar', 700.0)
    roar = np.tile(roar, int(np.ceil(n / len(roar))))[:n]
    # going away fast: 500 m off within 2 s, 1 km within 3 s (spreading alone takes 30 dB off the roar)
    env = np.minimum(t / 0.25, 1.0) * np.exp(-np.maximum(t - 0.5, 0.0) / 0.65)
    ignite = fit(airburst(name + '_ignite', 0.4), n) * 0.9
    thump = lowpass(rng.normal(size=n), 150.0) * envelope(n, 0.01, 0.8)
    x = roar * env + ignite + 0.6 * thump / np.abs(thump).max()
    return finish_one_shot(soft_clip(x / np.abs(x).max(), 1.5), 0.5)


def bomb_rush(name):
    """A bomb's fall heard in its last seconds: a rushing, tearing air noise rising in pitch and loudness; it ends
    loud, on the impact (the explosion covers its end). Short overlapping frames of band-passed noise, their centre
    rising from 300 Hz to 1.8 kHz (Hann windows at half overlap sum to one: no flutter)."""
    rng = rng_for(name)
    n = seconds(3.0)
    t = time_axis(n)
    frame = seconds(0.2)
    hop = frame // 2
    window = 0.5 - 0.5 * np.cos(2 * np.pi * np.arange(frame) / frame)     # periodic Hann
    noise = rng.normal(size=n + 2 * frame)
    acc = np.zeros(n + 2 * frame)
    for a in range(0, n + frame, hop):
        u = min(max((a - hop) / n, 0.0), 1.0)
        f = 300.0 + 1500.0 * u ** 2
        seg = bandpass(noise[a:a + frame], f * 0.6, f * 1.6)
        acc[a:a + frame] += seg / max(rms(seg), 1e-12) * window          # every frame as loud: the envelope rules
    x = acc[frame:frame + n]                                             # the first frames only ramp the sum up
    x = lowpass(x, 6000.0) * (0.15 + (t / t[-1]) ** 3)
    return normalise(fade(x, 0.002, 0.02))


def flare_pop(name):
    """A decoy flare's cartridge firing and the flare catching: a pop and a short fierce hiss."""
    rng = rng_for(name)
    n = seconds(1.2)
    pop = lowpass(friedlander(n, 0.0006, b=1.2), 6000.0, order=1)
    hiss = bandpass(rng.normal(size=n), 2000.0, 9000.0) * envelope(n, 0.05, 0.9, hold_s=0.2)
    return finish_one_shot(pop / np.abs(pop).max() + 0.4 * hiss / np.abs(hiss).max(), 0.2)


def fireball_roar(name, burn_s=3.6):
    """An aircraft's fuel going up (LOOK 6, the picture's rolling fireball of 0.45 M^1/3 s: 3.6 s for 500 kg): no
    detonation's crack but a deep 'whoomph' as the cloud catches (a deflagration rises over a tenth of a second), a
    low roar of burning while the ball rolls, a little crackle, then it dies with the flame."""
    rng = rng_for(name)
    n = seconds(burn_s + 2.0)
    t = time_axis(n)
    roar = shaped_noise(n, rng, lambda f: np.where(f < 70, -10 * np.log2(70 / f), -7.0 * np.log2(f / 70)))
    roar *= slow_wobble(n, rng, 2.5, 4.0)
    whoomph = lowpass(friedlander(n, 0.06, b=1.0), 120.0, order=2)
    crack = bandpass(crackle(n, rng, 30.0, 1.5), 400.0, 4000.0)
    rise = 1.0 - np.exp(-t / 0.08)
    burn = np.where(t < burn_s, 1.0, np.exp(-(t - burn_s) / 0.5)) * np.exp(-t / (1.5 * burn_s))
    x = (roar / np.abs(roar).max() + 0.25 * crack / np.abs(crack).max()) * rise * burn
    x += 1.2 * whoomph / np.abs(whoomph).max()
    return finish_one_shot(soft_clip(x / np.abs(x).max(), 1.3), 0.6)


def debris_rain(name):
    """Wreckage and earth coming down after a hit: thuds and clatter, thinning out over a few seconds (the first
    thud starts the file)."""
    rng = rng_for(name)
    n = seconds(4.0)
    thuds = lowpass(crackle(n, rng, 18.0, 6.0, skew=0.0, first=1.0), 400.0)
    clatter = bandpass(crackle(n, rng, 40.0, 1.0, skew=0.0), 700.0, 7000.0)
    env = envelope(n, 0.0, 3.0, curve=1.2)
    return finish_one_shot((thuds / np.abs(thuds).max() + 0.5 * clatter / np.abs(clatter).max()) * env, 0.5)


# ---------------------------------------------------------------- recordings (art/fx/recorded, PROVENANCE.md 2)
# Public-domain and CC0 recordings beat our noise where a real engine's tones and a real distance's colour matter: a jet
# far off, a heavy jet, a turboprop, a helicopter, a rocket's roar far off, the rolling of a far blast (LOOK 16.2).
# They carry their own distance (the air's dulling, the ground's echo, a passing aircraft's Doppler): only steady
# stretches are used, as loops for the engines (the game adds the Doppler of the source's own motion) and the far half of
# a crossfade (data/fx/sounds.json "band": "far"; "recordings": how far off each was made, so the game dulls it only
# by the rest of the way). Whole fly-bys are not used: no event says when a far aircraft passes, and the loops carry
# its motion already. Near blasts, booms, the bullet's crack, fire and the guns stay synthesised.
RECORDED = os.path.join(REPO, 'art', 'fx', 'recorded', 'sounds')


def recorded(file):
    """A fetched recording (mono 16-bit 48 kHz, PROVENANCE.md 2), as floats."""
    x, ch, width, rate = read_wav(os.path.join(RECORDED, file + '.wav'))
    if (ch, width, rate) != (1, 2, SR):
        raise ValueError('%s: %s, not mono 16-bit 48 kHz' % (file, (ch, width, rate)))
    return x


def flatten_level(x, win_s=1.0):
    """One period of a loop with its slow swells taken out (an aircraft coming a little nearer while it was recorded):
    divided by its RMS smoothed circularly over win_s, so the loop holds one level and its seam none; what moves faster
    (a rotor's slap, crackle) is kept."""
    k = seconds(win_s)
    w = np.hanning(k)
    w /= w.sum()
    kernel = np.zeros(len(x))
    kernel[:k] = w
    kernel = np.roll(kernel, -k // 2)
    power = np.fft.irfft(np.fft.rfft(np.square(x)) * np.fft.rfft(kernel), len(x))
    env = np.sqrt(np.maximum(power, 1e-12))
    return x * (np.sqrt(np.mean(np.square(x))) / env)


def recorded_loop(file, flatten=False, lowpass_hz=None):
    """A fetched loop (its end already crossfaded into its start), filtered circularly so it stays one."""
    x = recorded(file)
    if lowpass_hz:
        x = lowpass(x, lowpass_hz, order=2, circular=True)
    if flatten:
        x = flatten_level(x)
    return finish_loop(x)


def blast_tail(file, start_s=3.0, length_s=11.0, decay_db_per_s=3.0, lowpass_hz=700.0):
    """The rolling of a far blast from the NPS thunder (after its first crack and peal): the rain's hiss under it
    filtered off (above 2 kHz it is all rain, 50 dB down), a gentle fade in to slip under the synthesised far blast's
    own thud, an even decay of decay_db_per_s with the roll's own surges on it."""
    x = recorded(file)
    x = lowpass(x, lowpass_hz, order=4)
    seg = x[seconds(start_s):seconds(start_s + length_s)].copy()
    seg *= 10.0 ** (-decay_db_per_s * time_axis(len(seg)) / 20.0)
    seg = fade(seg, 0.5, 0.0)
    return finish_one_shot(seg, 1.5)


# How the recordings were matched to the synthesised sounds they stand in for (fx_sounds.py --check prints and checks
# it; data/fx/sounds.json's "recordings" carry the numbers): the level of each heard through the air's low-pass at the
# distance where it takes over, a recording dulled only by what is left after its own distance.
def air_absorption_db_per_m(f, t_k=288.15, rh=70.0, p_pa=101325.0):
    """ISO 9613-1, as simcore::fx::AirAbsorptionDbPerM."""
    pa = p_pa / 101325.0
    tr = t_k / 293.15
    c_sat = -6.8346 * (273.16 / t_k) ** 1.261 + 4.6151
    h = rh * 10.0 ** c_sat / pa
    fr_o = pa * (24.0 + 4.04e4 * h * (0.02 + h) / (0.391 + h))
    fr_n = pa / np.sqrt(tr) * (9.0 + 280.0 * h * np.exp(-4.170 * (tr ** (-1.0 / 3.0) - 1.0)))
    f2 = np.asarray(f, dtype=float) ** 2
    return 8.686 * f2 * (1.84e-11 / pa * np.sqrt(tr) + tr ** -2.5 * (
        0.01275 * np.exp(-2239.1 / t_k) / (fr_o + f2 / fr_o) + 0.1068 * np.exp(-3352.0 / t_k) / (fr_n + f2 / fr_n)))


def absorption_cutoff_hz(distance_m, budget_db=6.0):
    """The frequency above which the air takes budget_db off over distance_m (simcore::fx::AbsorptionCutoffHz)."""
    top, bottom = 20000.0, 60.0
    if distance_m <= 0 or air_absorption_db_per_m(top) * distance_m <= budget_db:
        return top
    if air_absorption_db_per_m(bottom) * distance_m >= budget_db:
        return bottom
    lo, hi = np.log(bottom), np.log(top)
    for _ in range(40):
        mid = 0.5 * (lo + hi)
        if air_absorption_db_per_m(np.exp(mid)) * distance_m > budget_db:
            hi = mid
        else:
            lo = mid
    return float(np.exp(0.5 * (lo + hi)))


def pitched(x, pitch):
    """Played pitch times faster (the engine's pitch multiplier): resampled by linear interpolation."""
    if abs(pitch - 1.0) < 1e-9:
        return x
    return np.interp(np.arange(0.0, len(x) - 1, pitch), np.arange(len(x)), x)


def heard_rms_db(x, pitch, distance_m, recorded_at_m=0.0):
    """A sample's RMS (dBFS) as the game plays it distance_m away: at its pitch, through the one-pole low-pass at the
    air's cut-off for the distance less the recording's own."""
    fc = absorption_cutoff_hz(max(distance_m - recorded_at_m, 1.0))
    y = spectrum_filter(pitched(x, pitch), lambda f: 1.0 / np.sqrt(1.0 + (f / fc) ** 2))
    return 20.0 * np.log10(max(rms(y), 1e-12))


def match_db(rec, rec_pitch, recorded_at_m, synth, synth_pitch, at_m):
    """How much louder the recording's layer is set than the synthesised sound it stands for, so the two are heard
    alike at at_m (where the recording takes over): the synthesised one's heard RMS less the recording's."""
    return heard_rms_db(synth, synth_pitch, at_m) - heard_rms_db(rec, rec_pitch, at_m, recorded_at_m)


# The sounds and their files. The data (data/fx/effects.json) says which plays for what, at what level and distance.
#   kind  one_shot: starts at once, decays to silence     loop: one period of a seamless loop
#         riser: starts at once, ends loud on the event that cuts it (a bomb's rush ends in its explosion)
#         tail: comes in softly under another sound and decays to silence (a far blast's recorded roll)
#   recorded: made from a fetched recording (a loop may then be up to 10 s: the steady stretch it was cut from)
Sound = namedtuple('Sound', 'make kind rate_hz recorded', defaults=(False,))


def one(make):
    return Sound(make, 'one_shot', None)


def loop(make, rate_hz=None):
    return Sound(make, 'loop', rate_hz)


SOUNDS = {
    # explosions: (charge in kg of TNT equivalent)
    'S_FX_Explosion_Small_Near': one(lambda n: airburst(n, 15.0)),           # a SAM's warhead (9M33: 15 kg)
    'S_FX_Explosion_Small_Far': one(lambda n: explosion(n, 15.0, distant=True)),
    'S_FX_Explosion_Ground_Small_Near': one(lambda n: explosion(n, 5.0)),     # a rocket, a 57 mm shell, a bomblet's bigger kin
    'S_FX_Explosion_Medium_Near': one(lambda n: explosion(n, 120.0)),        # a 250 kg bomb
    'S_FX_Explosion_Medium_Far': one(lambda n: explosion(n, 120.0, distant=True)),
    'S_FX_Explosion_Large_Near': one(lambda n: explosion(n, 430.0)),         # a 1000 kg bomb, a Kh-22's warhead
    'S_FX_Explosion_Large_Far': one(lambda n: explosion(n, 430.0, distant=True)),
    'S_FX_Cluster_Ripple': one(lambda n: cluster_ripple(n)),
    'S_FX_Sonic_Boom_Short': one(lambda n: sonic_boom(n, 0.10)),
    'S_FX_Sonic_Boom_Long': one(lambda n: sonic_boom(n, 0.20)),
    # engines (loops)
    'S_FX_Jet_Roar_Turbojet': loop(lambda n: jet_roar(n, 320.0, 5.0)),        # MiG-21, F-4: hot, harsh
    'S_FX_Jet_Roar_Turbofan': loop(lambda n: jet_roar(n, 220.0, 2.0)),        # F-15, F-16, Tornado
    'S_FX_Jet_Whine_Turbojet': loop(lambda n: jet_whine(n, 190.0, 4200.0)),
    'S_FX_Jet_Whine_Turbofan': loop(lambda n: jet_whine(n, 140.0, 2600.0)),
    'S_FX_Afterburner': loop(lambda n: afterburner(n)),
    'S_FX_Rocket_Motor_Small': loop(lambda n: rocket_motor(n, 900.0)),        # SAMs, air-to-air, air-to-ground missiles
    'S_FX_Rocket_Motor_Large': loop(lambda n: rocket_motor(n, 450.0)),        # Kh-22, large boosters
    'S_FX_Prop_Turboprop': loop(lambda n: propeller(n, 95.0)),
    'S_FX_Rotor_Helicopter': loop(lambda n: propeller(n, 19.0, harmonics=40)),
    'S_FX_Fire_Loop': loop(lambda n: fire(n)),
    # guns (loops at the cyclic rate, a tail for the end of a burst, single rounds)
    'S_FX_Gun_23mm_Quad': loop(lambda n: gun_burst_loop(n, 23.0, 58.0), 58.0),      # ZSU-23-4: four barrels, ~3400 rounds/min
    'S_FX_Gun_23mm_Twin': loop(lambda n: gun_burst_loop(n, 23.0, 30.0), 30.0),      # ZU-23-2
    'S_FX_Gun_20mm_Gatling': loop(lambda n: gun_burst_loop(n, 20.0, 100.0), 100.0),  # M61: 6000 rounds/min
    'S_FX_Gun_30mm_Gatling': loop(lambda n: gun_burst_loop(n, 30.0, 65.0), 65.0),   # GAU-8, GSh-6-30
    'S_FX_Gun_Burst_Tail': one(lambda n: gun_burst_tail(n, 23.0)),
    'S_FX_Gun_57mm_Single': one(lambda n: gun_single(n, 57.0)),             # S-60
    'S_FX_Bullet_Crack': one(lambda n: bullet_crack(n)),
    # one-shots
    'S_FX_Missile_Launch': one(lambda n: missile_launch(n)),
    'S_FX_Bomb_Rush': Sound(lambda n: bomb_rush(n), 'riser', None),
    'S_FX_Flare_Pop': one(lambda n: flare_pop(n)),
    'S_FX_Debris_Rain': one(lambda n: debris_rain(n)),
    'S_FX_Fireball_Roar': one(lambda n: fireball_roar(n)),
    # from recordings (PROVENANCE.md 2; data/fx/sounds.json "recordings" says how far off each was made)
    'S_FX_Rec_Jet_Far': Sound(lambda n: recorded_loop('S_FXR_Jet_Military_Far_NPS_Loop'), 'loop', None, True),
    'S_FX_Rec_Jet_Heavy': Sound(lambda n: recorded_loop('S_FXR_Jet_Heavy_SOFIA_Takeoff_Loop'), 'loop', None, True),
    'S_FX_Rec_Prop_Turboprop': Sound(lambda n: recorded_loop('S_FXR_Prop_Turboprop_An24_Loop'), 'loop', None, True),
    'S_FX_Rec_Rotor_Near': Sound(lambda n: recorded_loop('S_FXR_Rotor_Chinook_Loop', flatten=True), 'loop', None, True),
    'S_FX_Rec_Rotor_Far': Sound(lambda n: recorded_loop('S_FXR_Rotor_Heli_Far_Lake_Loop', flatten=True), 'loop', None, True),
    'S_FX_Rec_Rocket_Motor_Far': Sound(lambda n: recorded_loop('S_FXR_Rocket_AtlasV_Far_SDO_Loop'), 'loop', None, True),
    'S_FX_Rec_Rocket_Motor_Large': Sound(lambda n: recorded_loop('S_FXR_Rocket_StaticFire_SLS_Loop'), 'loop', None, True),
    'S_FX_Rec_Rocket_Motor_Far_Large': Sound(lambda n: recorded_loop('S_FXR_Rocket_Shuttle_Far_STS131_Loop'), 'loop', None,
                                             True),
    'S_FX_Rec_Blast_Tail': Sound(lambda n: blast_tail('S_FXR_Thunder_Roll_NPS'), 'tail', None, True),
}

LOOPS = {k for k, s in SOUNDS.items() if s.kind == 'loop'}

# Near and far versions of one explosion: the far one must be the duller.
NEAR_FAR = (('S_FX_Explosion_Small_Near', 'S_FX_Explosion_Small_Far'),
            ('S_FX_Explosion_Medium_Near', 'S_FX_Explosion_Medium_Far'),
            ('S_FX_Explosion_Large_Near', 'S_FX_Explosion_Large_Far'))

MANIFEST = 'sounds.json'


# ---------------------------------------------------------------- measuring a sound
def peak_dbfs(x):
    return 20.0 * np.log10(max(np.abs(x).max(), 1e-12))


def window_rms_db(x, win_s):
    """The RMS level (dB re full scale) of each whole window of win_s."""
    k = max(seconds(win_s), 1)
    m = len(x) // k
    if m == 0:
        return np.zeros(0)
    return 10.0 * np.log10(np.mean(np.square(x[:m * k].reshape(m, k)), axis=1) + 1e-20)


def seam(x, win_s=0.02):
    """How a loop joins its end to its start: the RMS step from its last win_s to its first (dB), the largest step
    between neighbouring windows inside it (99th percentile), and the sample jump across the wrap against the median
    and the 99.9th percentile of the jumps inside it."""
    k = seconds(win_s)
    step = abs(10.0 * np.log10((np.mean(np.square(x[:k])) + 1e-20) / (np.mean(np.square(x[-k:])) + 1e-20)))
    inner = np.abs(np.diff(window_rms_db(x, win_s)))
    jumps = np.abs(np.diff(x))
    med = max(float(np.median(jumps)), 1e-12)
    return {'wrap_rms_step_db': float(step), 'inner_rms_step_db_p99': float(np.percentile(inner, 99)),
            'wrap_jump_x_median': abs(float(x[0] - x[-1])) / med,
            'jump_p999_x_median': float(np.percentile(jumps, 99.9)) / med}


def seam_ok(s):
    """The wrap is no worse than the joins inside: its level step within 1.5 dB or half as much again as the 99th
    percentile inside (a perfect loop's wrap is one more draw from that spread), its sample jump within six median
    jumps or the 99.9th percentile (a click would stand out of both)."""
    return s['wrap_rms_step_db'] <= max(1.5, 1.5 * s['inner_rms_step_db_p99']) and \
        s['wrap_jump_x_median'] <= max(6.0, s['jump_p999_x_median'])


def onset_s(x, below_peak_db=30.0):
    """When the sound starts: its first sample within below_peak_db of its peak."""
    thr = np.abs(x).max() * 10 ** (-below_peak_db / 20.0)
    return int(np.argmax(np.abs(x) >= thr)) / SR


def spectral_centroid_hz(x):
    """The power-weighted mean frequency of the whole sound: lower is duller."""
    p = np.abs(np.fft.rfft(x)) ** 2
    f = np.fft.rfftfreq(len(x), 1.0 / SR)
    return float(np.sum(f * p) / max(np.sum(p), 1e-20))


def rate_from_autocorrelation(x, expect_hz):
    """The repetition rate of a train of reports: the strongest peak of the level envelope's autocorrelation between
    0.6 and 1.6 periods of expect_hz. Returns (rate in Hz, the peak's height as a fraction of zero lag). The loop
    repeats, so the autocorrelation is circular."""
    e = np.abs(x)
    e = lowpass(e, 2000.0, circular=True)                # the reports' envelope, its carrier gone
    e -= e.mean()
    spec = np.fft.rfft(e)
    ac = np.fft.irfft(spec * np.conj(spec), len(e))
    lo, hi = int(0.6 * SR / expect_hz), int(1.6 * SR / expect_hz)
    lag = lo + int(np.argmax(ac[lo:hi]))
    return SR / lag, float(ac[lag] / max(ac[0], 1e-20))


def n_wave_fit(x, search_s=0.6, rise_s=0.003):
    """The sonic boom's N, from the samples: the front (the largest rise over 2 rise_s in the first 10 ms), the tail
    shock (the largest such rise after it, within search_s; both shocks are compressions, so both are rises) and the
    straightness of the fall between (R^2 of a line through its middle 70 %). Returns a dict: front_s, length_s,
    front_rise and tail_rise (fractions of the peak), the fall's slope sign, r2, and the level just before the tail
    shock (fraction of the peak; an N's is near -1)."""
    peak = np.abs(x).max()
    k = seconds(rise_s)
    pad = np.concatenate([np.zeros(k), x, np.zeros(k)])
    rise = (pad[2 * k:] - pad[:-2 * k]) / peak                 # rise[i] = x[i + k] - x[i - k]
    i0 = int(np.argmax(rise[:seconds(0.01)]))
    lo = i0 + seconds(0.02)
    i1 = lo + int(np.argmax(rise[lo:seconds(search_s)]))
    a, b = i0 + k + int(0.15 * (i1 - i0 - 2 * k)), i0 + k + int(0.85 * (i1 - i0 - 2 * k))
    t = np.arange(a, b)
    coef = np.polyfit(t, x[a:b], 1)
    resid = x[a:b] - np.polyval(coef, t)
    r2 = 1.0 - np.sum(resid ** 2) / max(np.sum((x[a:b] - x[a:b].mean()) ** 2), 1e-20)
    return {'front_s': i0 / SR, 'length_s': (i1 - i0) / SR, 'front_rise': float(rise[i0]),
            'tail_rise': float(rise[i1]), 'slope': float(np.sign(coef[0])), 'r2': float(r2),
            'before_tail': float(x[max(i1 - k, 0)] / peak)}


def analyse(name, x, sound, fmt=(1, 2, SR)):
    """Measure one sound against the rules every file keeps. Returns (metrics, failures as text)."""
    m, bad = {}, []
    m['seconds'] = len(x) / SR
    if tuple(fmt) != (1, 2, SR):
        bad.append('format %s, not mono 16-bit 48 kHz' % (fmt,))
    if not np.all(np.isfinite(x)):
        bad.append('not finite')
        return m, bad
    m['peak_dbfs'] = peak_dbfs(x)
    m['clipped'] = int(np.sum(np.abs(np.round(x * 32767.0)) >= 32767))
    m['dc'] = float(np.mean(x))
    if not -1.3 <= m['peak_dbfs'] <= -0.7:
        bad.append('peak %.2f dBFS' % m['peak_dbfs'])
    if m['clipped']:
        bad.append('%d samples at full scale' % m['clipped'])
    if abs(m['dc']) > 1e-3:
        bad.append('DC %.4f' % m['dc'])
    if sound.kind != 'loop':
        # where a one-shot has died away it must be silence, not an offset (the whole file's mean can be 0 with one)
        m['dc_tail'] = float(np.mean(x[int(0.75 * len(x)):int(0.95 * len(x))]))
        if abs(m['dc_tail']) > 3e-4:
            bad.append('DC %.5f in the tail' % m['dc_tail'])
    if sound.kind == 'loop':
        longest = 10.0 if sound.recorded else 6.0       # a recording's loop is the steady stretch it was cut from
        if not 2.0 - 1e-6 <= m['seconds'] <= longest + 1e-6:
            bad.append('loop %.2f s (2-%g s)' % (m['seconds'], longest))
        s = seam(x)
        m.update(s)
        if not seam_ok(s):
            bad.append('seam: step %.2f dB (inner p99 %.2f), jump %.1fx median (inner p99.9 %.1fx)' % (
                s['wrap_rms_step_db'], s['inner_rms_step_db_p99'], s['wrap_jump_x_median'], s['jump_p999_x_median']))
        if sound.rate_hz:
            rate, height = rate_from_autocorrelation(x, sound.rate_hz)
            m['rate_hz'], m['rate_peak'] = rate, height
            if abs(rate / sound.rate_hz - 1.0) > 0.05 or height < 0.1:
                bad.append('rate %.1f /s (peak %.2f), wanted %.1f' % (rate, height, sound.rate_hz))
    else:
        if not 0.2 <= m['seconds'] <= 12.0:
            bad.append('one-shot %.2f s (0.2-12 s)' % m['seconds'])
        m['onset_ms'] = 1000.0 * onset_s(x)
        if m['onset_ms'] > 5.0 and sound.kind != 'tail':    # a tail comes in softly under the sound it follows
            bad.append('starts at %.1f ms' % m['onset_ms'])
        m['last_sample'] = float(abs(x[-1]))
        if m['last_sample'] > 1e-3:
            bad.append('ends on %.4f, not silence' % m['last_sample'])
        if sound.kind in ('one_shot', 'tail'):
            # it decays on its own: the last tenth (before the fade-out) lies far below the loudest 50 ms
            win = 0.05 if m['seconds'] > 1.0 else 0.01
            levels = window_rms_db(x, win)
            tail = window_rms_db(x[int(0.85 * len(x)):int(0.95 * len(x))], win)
            m['tail_below_loudest_db'] = float(levels.max() - (tail.max() if len(tail) else -200.0))
            if m['tail_below_loudest_db'] < 30.0:
                bad.append('tail only %.1f dB below the loudest' % m['tail_below_loudest_db'])
        m['centroid_hz'] = spectral_centroid_hz(x)
    if name.startswith('S_FX_Sonic_Boom'):
        nw = n_wave_fit(x)
        m.update({'n_' + k: v for k, v in nw.items()})
        if not (nw['front_s'] < 0.005 and nw['front_rise'] > 0.6 and nw['tail_rise'] > 0.6 and nw['slope'] < 0
                and nw['r2'] > 0.95 and nw['before_tail'] < -0.5):
            bad.append('not an N-wave: %s' % ', '.join('%s %.3f' % kv for kv in nw.items()))
    return m, bad


def check_dir(out):
    """Measure every sound in `out`; print a line per file and the near/far comparisons. Returns the failures."""
    failures = []
    centroids = {}
    for name, sound in SOUNDS.items():
        path = os.path.join(out, name + '.wav')
        if not os.path.exists(path):
            failures.append('%s: missing' % name)
            print('%-34s MISSING' % name)
            continue
        x, ch, width, rate = read_wav(path)
        m, bad = analyse(name, x, sound, (ch, width, rate))
        centroids[name] = spectral_centroid_hz(x)
        extra = []
        if sound.kind == 'loop':
            extra.append('seam %.2f dB %.1fx' % (m.get('wrap_rms_step_db', 0), m.get('wrap_jump_x_median', 0)))
            if 'rate_hz' in m:
                extra.append('rate %.1f/s' % m['rate_hz'])
        else:
            extra.append('onset %.1f ms' % m.get('onset_ms', 0))
            if 'tail_below_loudest_db' in m:
                extra.append('tail -%.0f dB' % m['tail_below_loudest_db'])
        extra.append('centroid %.0f Hz' % centroids[name])
        print('%-34s %5.2f s %6.2f dBFS dc %+.5f  %-6s %s  %s' % (
            name, m['seconds'], m.get('peak_dbfs', 0), m.get('dc', 0), sound.kind, '  '.join(extra),
            'OK' if not bad else 'FAIL: ' + '; '.join(bad)))
        failures += ['%s: %s' % (name, b) for b in bad]
    for near, far in NEAR_FAR:
        if near in centroids and far in centroids:
            ok = centroids[far] < 0.8 * centroids[near]
            print('%s centroid %.0f Hz < %s %.0f Hz: %s' % (far, centroids[far], near, centroids[near], 'OK' if ok else 'FAIL'))
            if not ok:
                failures.append('%s is not duller than %s' % (far, near))
    return failures + check_recordings(out)


BANK = os.path.join(REPO, 'data', 'fx', 'sounds.json')


def check_recordings(out, bank_path=BANK):
    """The recordings' levels in data/fx/sounds.json: each match_db recomputed from the files in `out` (within 1 dB),
    and each layer that plays a recording at its stands_for_level_db_1m + match_db (within 0.15 dB). Returns failures."""
    failures = []
    if not os.path.exists(bank_path):
        return failures
    with open(bank_path, encoding='utf-8') as f:
        bank = json.load(f)
    recordings = {k: v for k, v in bank.get('recordings', {}).items() if isinstance(v, dict)}
    for name, entry in recordings.items():
        if name not in SOUNDS or not SOUNDS[name].recorded:
            failures.append('%s: in sounds.json recordings but not a recorded sound here' % name)
            continue
        if not os.path.exists(os.path.join(RECORDED, entry.get('file', '') + '.wav')):
            failures.append('%s: its recording %s is missing' % (name, entry.get('file')))
        for m in entry.get('matches', []):
            layers = [la for la in bank['profiles'].get(m['profile'], {}).get('layers', []) if la['sample'] == name]
            if not layers:
                failures.append('%s: no layer of %s plays it' % (name, m['profile']))
                continue
            layer = layers[0]
            paths = [os.path.join(out, n + '.wav') for n in (name, m['stands_for'])]
            if not all(os.path.exists(p) for p in paths):
                failures.append('%s: %s or %s missing' % (name, *paths))
                continue
            got = match_db(read_wav(paths[0])[0], layer.get('pitch', 1.0), entry.get('recorded_at_m', 0.0),
                           read_wav(paths[1])[0], m.get('stands_for_pitch', 1.0), m['at_m'])
            want_level = m['stands_for_level_db_1m'] + m['match_db']
            ok = abs(got - m['match_db']) <= 1.0 and abs(layer['level_db_1m'] - want_level) <= 0.15
            print('%-34s in %-16s at %5.0f m: match %+5.1f dB (sounds.json %+5.1f), level %.1f dB (%.1f + %+.1f): %s' % (
                name, m['profile'], m['at_m'], got, m['match_db'], layer['level_db_1m'], m['stands_for_level_db_1m'],
                m['match_db'], 'OK' if ok else 'FAIL'))
            if not ok:
                failures.append('%s in %s: match %.1f dB, sounds.json %.1f dB; level %.1f, wanted %.1f' % (
                    name, m['profile'], got, m['match_db'], layer['level_db_1m'], want_level))
    return failures


def spectrogram_png(x, path, width=600, height=200):
    """A log-frequency spectrogram (30 Hz at the bottom, 24 kHz at the top, 80 dB of range), for a look at a sound
    without hearing it."""
    from PIL import Image
    hop = max(len(x) // width, 64)
    win = 2048
    pad = np.concatenate([x, np.zeros(win)])
    cols = []
    for i in range(0, len(x), hop):
        seg = pad[i:i + win] * np.hanning(win)
        cols.append(20 * np.log10(np.abs(np.fft.rfft(seg)) + 1e-6))
    s = np.array(cols).T
    f = np.fft.rfftfreq(win, 1.0 / SR)
    rows = np.geomspace(30, SR / 2, height)
    img = np.array([np.interp(rows, f, c) for c in s.T]).T[::-1]
    img = np.clip((img - img.max() + 80) / 80, 0, 1)
    Image.fromarray((img * 255).astype(np.uint8)).resize((width, height)).save(path)


def write_manifest(out, made):
    """sounds.json beside the files: each sound's kind, whether it loops, its length and (guns) its rate. Kept for the
    sounds not made this run (--only)."""
    path = os.path.join(out, MANIFEST)
    manifest = {}
    if os.path.exists(path):
        with open(path, encoding='utf-8') as f:
            manifest = json.load(f)
    manifest.update(made)
    manifest = {k: manifest[k] for k in SOUNDS if k in manifest}
    with open(path, 'w', encoding='utf-8') as f:
        json.dump(manifest, f, indent=2)
        f.write('\n')


def main(argv=None):
    ap = argparse.ArgumentParser(description=' '.join(__doc__.split('\n\n')[0].split()))
    ap.add_argument('--out', default=os.path.join(REPO, 'art', 'fx', 'sounds'))
    ap.add_argument('--only', nargs='*', help='make only these sounds (names)')
    ap.add_argument('--spectrograms', nargs='?', const=os.path.join(REPO, 'out', 'fx_sounds'),
                    help='also draw a spectrogram of each into this folder (default out/fx_sounds)')
    ap.add_argument('--check', action='store_true', help='measure the files in --out (makes nothing unless --only)')
    args = ap.parse_args(argv)
    os.makedirs(args.out, exist_ok=True)
    if args.spectrograms:
        os.makedirs(args.spectrograms, exist_ok=True)
    unknown = [k for k in (args.only or []) if k not in SOUNDS]
    if unknown:
        print('unknown sounds: %s' % ', '.join(unknown))
        return 2
    made = {}
    if not args.check or args.only:
        for name, sound in SOUNDS.items():
            if args.only and name not in args.only:
                continue
            x = sound.make(name)
            write_wav(os.path.join(args.out, name + '.wav'), x)
            if args.spectrograms:
                spectrogram_png(x, os.path.join(args.spectrograms, name + '.png'))
            made[name] = {'kind': sound.kind, 'loop': sound.kind == 'loop', 'seconds': round(len(x) / SR, 4)}
            if sound.rate_hz:
                made[name]['rate_hz'] = sound.rate_hz
            print('%-34s %6.2f s %s' % (name, len(x) / SR, sound.kind))
        write_manifest(args.out, made)
    if args.check:
        failures = check_dir(args.out)
        print('%d failure(s)' % len(failures))
        for f in failures:
            print('  ' + f)
        return 1 if failures else 0
    return 0


if __name__ == '__main__':
    sys.exit(main())
