"""The effects' art tools (tools/fx_sounds.py, tools/fx_textures.py, tools/ue_make_fx_assets.py): their DSP and texture
helpers against hand calculations, short loops made and measured, and the committed art checked by the same rules,
without remaking any of it.

    python -m pytest tools/test_fx_tools.py -q
"""
import json
import os
import sys

import numpy as np
import pytest

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import fx_sounds as S  # noqa: E402
import fx_textures as T  # noqa: E402
import ue_make_fx_assets as U  # noqa: E402

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SOUNDS = os.path.join(REPO, 'art', 'fx', 'sounds')
TEXTURES = os.path.join(REPO, 'art', 'fx', 'textures')
SR = S.SR


# ---------------------------------------------------------------- sound: building blocks
def test_friedlander_has_its_hand_calculated_shape():
    # p(t) = (1 - t/T) exp(-b t/T): 1 at the front, 0 at T, the negative phase's least value at t = T (1 + b) / b,
    # -(1/b) exp(-(1 + b)). T = 10 ms, b = 1.6: zero at 480 samples, minimum -0.0464 at 16.25 ms (780 samples).
    T_s, b = 0.010, 1.6
    p = S.friedlander(SR, T_s, b=b)
    assert p[0] == pytest.approx(1.0)
    assert p[480] == pytest.approx(0.0, abs=1e-12)
    i_min = int(np.argmin(p))
    assert i_min == pytest.approx(T_s * (1 + b) / b * SR, abs=1)
    assert p[i_min] == pytest.approx(-(1 / b) * np.exp(-(1 + b)), rel=1e-4)
    # a later start: silence before it, the same pulse after
    q = S.friedlander(SR, T_s, b=b, start=100)
    assert np.all(q[:100] == 0.0)
    assert q[100:600] == pytest.approx(p[:500])


def test_envelope_reaches_minus_60_db_after_its_decay():
    env = S.envelope(SR, attack_s=0.01, decay_s=0.5, hold_s=0.1)
    assert env[S.seconds(0.005)] == pytest.approx(0.5, abs=1e-3)          # half way up the linear attack
    assert env[S.seconds(0.05)] == pytest.approx(1.0)                      # held
    assert env[S.seconds(0.61)] == pytest.approx(10 ** (-60 / 20), rel=0.01)


def test_lowpass_is_3_db_down_at_its_corner_and_12_db_an_octave_up():
    # A whole number of cycles, filtered as a loop: the gain is exactly the Butterworth magnitude 1/sqrt(1 + (f/fc)^2n).
    t = np.arange(SR) / SR
    for f, want in ((1000.0, 1 / np.sqrt(2)), (2000.0, 1 / np.sqrt(17)), (250.0, 1 / np.sqrt(1 + 0.25 ** 4))):
        y = S.lowpass(np.sin(2 * np.pi * f * t), 1000.0, order=2, circular=True)
        assert S.rms(y) / S.rms(np.sin(2 * np.pi * f * t)) == pytest.approx(want, rel=1e-6)


def test_lowpass_dulls_noise_more_the_lower_its_corner():
    # the far sounds' recipe: a lower corner (more air absorption with distance) always lowers the centroid
    x = np.random.default_rng(1).normal(size=SR)
    centroids = [S.spectral_centroid_hz(S.lowpass(x, fc)) for fc in (8000, 4000, 2000, 1000, 500, 250)]
    assert all(a > b for a, b in zip(centroids, centroids[1:]))


def test_octave_bands_sum_back_to_the_signal():
    f = np.fft.rfftfreq(1 << 14, 1.0 / SR)
    assert np.sum(S.octave_weights(f), axis=0) == pytest.approx(np.ones_like(f))
    x = np.random.default_rng(2).normal(size=4800)
    assert np.sum(S.octave_bands(x), axis=0) == pytest.approx(x, abs=1e-9)


def test_causal_smooth_moves_nothing_before_a_jump_and_rises_like_an_rc():
    x = np.zeros(4800)
    x[1000:] = 1.0
    tau = 0.001
    y = S.causal_smooth(x, tau)
    assert np.all(np.abs(y[:1000]) < 1e-12)
    assert y[1000 + S.seconds(tau)] == pytest.approx(1 - np.exp(-1), abs=0.01)   # 63 % after one tau
    assert y[-1] == pytest.approx(1.0, abs=1e-3)


def test_dc_block_lets_an_offset_bleed_away():
    x = np.zeros(SR)
    x[100:] = 0.5                                   # a step: a DC offset from sample 100 on
    y = S.dc_block(x, fc=5.0)
    tau = 1 / (2 * np.pi * 5.0)
    assert np.all(np.abs(y[:100]) < 1e-12)
    assert y[100] == pytest.approx(0.5, rel=0.01)    # the jump passes
    assert y[100 + S.seconds(tau)] == pytest.approx(0.5 * np.exp(-1), abs=0.01)
    assert abs(y[-1]) < 1e-3                        # a second later the offset is gone


def test_wrap_round_folds_the_overhang_onto_the_start():
    assert list(S.wrap_round(np.arange(1.0, 8.0), 3)) == [1 + 4 + 7, 2 + 5, 3 + 6]


def test_circular_filtering_is_the_steady_state_of_the_repeated_loop():
    # a loop filtered circularly equals the middle period of the loop repeated and filtered: no seam
    x = np.random.default_rng(3).normal(size=4800)
    loop = S.bandpass(x, 300.0, 3000.0, circular=True)
    long = S.bandpass(np.tile(x, 5), 300.0, 3000.0)
    assert loop == pytest.approx(long[2 * 4800:3 * 4800], abs=1e-6)


def test_loop_tones_sit_on_whole_cycles():
    n = SR * 3
    for f in S.loop_tones(n, [95.1, 4200.37, 19.0]):
        assert f * n / SR == pytest.approx(round(f * n / SR), abs=1e-9)


def test_n_wave_is_an_n():
    x = S.n_wave(SR // 2, 0.1, 0.002, 0.003)
    fit = S.n_wave_fit(x)
    assert 0.002 <= fit['front_s'] <= 0.002 + 0.003      # the steepest rise, within the 3 ms front
    assert fit['length_s'] == pytest.approx(0.1, abs=0.002)
    assert fit['front_rise'] > 0.9 and fit['tail_rise'] > 0.9
    assert fit['slope'] < 0 and fit['r2'] > 0.999
    assert np.sum(x) == pytest.approx(0.0, abs=1.0)    # the N has no net impulse


def test_rate_from_autocorrelation_finds_a_click_train():
    x = np.zeros(SR)
    for i in range(40):
        x[int(i * SR / 40)] = 1.0
    rate, height = S.rate_from_autocorrelation(S.lowpass(x, 4000.0, circular=True), 40.0)
    assert rate == pytest.approx(40.0, rel=0.01) and height > 0.5


def test_the_seam_measure_tells_a_bad_loop_from_a_good_one():
    n = SR
    t = np.arange(n) / SR
    good = np.sin(2 * np.pi * 440.0 * t)               # 440 whole cycles: the end runs into the start
    bad = np.sin(2 * np.pi * 440.25 * t)               # a quarter cycle over: from the crest back to zero
    assert S.seam_ok(S.seam(good))
    assert not S.seam_ok(S.seam(bad))


def test_air_absorption_matches_iso_9613_tables():
    # ISO 9613-2 table 2, 10 degC and 70 % RH: 3.7 dB/km at 1 kHz, 32.8 dB/km at 4 kHz (as simcore::fx's)
    assert S.air_absorption_db_per_m(1000.0, t_k=283.15) * 1000.0 == pytest.approx(3.7, abs=0.2)
    assert S.air_absorption_db_per_m(4000.0, t_k=283.15) * 1000.0 == pytest.approx(32.8, abs=1.0)
    # the cut-off is where the budget is spent, and it falls with distance
    fc = S.absorption_cutoff_hz(1500.0)
    assert S.air_absorption_db_per_m(fc) * 1500.0 == pytest.approx(6.0, rel=1e-3)
    assert S.absorption_cutoff_hz(5000.0) < fc < S.absorption_cutoff_hz(500.0)


def test_pitched_plays_faster_and_higher():
    x = np.sin(2 * np.pi * 100.0 * np.arange(SR) / SR)
    y = S.pitched(x, 2.0)
    assert len(y) == pytest.approx(SR / 2, abs=2)
    assert S.spectral_centroid_hz(y) == pytest.approx(200.0, rel=0.02)


def test_flatten_level_takes_out_a_slow_swell_and_keeps_the_loop():
    rng = np.random.default_rng(3)
    n = 4 * SR
    t = np.arange(n) / SR
    noise = S.lowpass(rng.normal(size=n), 2000.0, circular=True)
    swell = noise * (1.0 + 0.6 * np.sin(2 * np.pi * t / 4.0))      # 8 dB up and down once a loop
    flat = S.flatten_level(swell)
    win = S.window_rms_db(flat, 0.5)
    assert win.max() - win.min() < 2.0
    assert S.seam_ok(S.seam(S.finish_loop(flat)))


def test_a_recording_is_matched_to_what_it_stands_for():
    # the same sound, quieter by 6 dB, matches at +6 dB; a recording made 5 km off is not dulled again under 5 km
    rng = np.random.default_rng(5)
    x = rng.normal(size=SR)
    assert S.match_db(0.5 * x, 1.0, 0.0, x, 1.0, 1000.0) == pytest.approx(6.02, abs=0.05)
    dull = S.heard_rms_db(x, 1.0, 3000.0)
    assert S.heard_rms_db(x, 1.0, 3000.0, recorded_at_m=5000.0) > dull + 3.0


def test_wav_round_trip_and_refusal_of_nan(tmp_path):
    x = 0.5 * np.sin(2 * np.pi * 100 * np.arange(4800) / SR)
    path = str(tmp_path / 'a.wav')
    S.write_wav(path, x)
    y, ch, width, rate = S.read_wav(path)
    assert (ch, width, rate) == (1, 2, SR)
    assert y == pytest.approx(x, abs=1 / 32767)
    with pytest.raises(ValueError):
        S.write_wav(path, np.array([0.0, np.nan]))


# ---------------------------------------------------------------- sound: short versions of the real makers
@pytest.mark.parametrize('make', [
    lambda: S.jet_roar('t_roar', 320.0, 5.0, length_s=2.0),
    lambda: S.jet_whine('t_whine', 190.0, 4200.0, length_s=2.0),
    lambda: S.afterburner('t_ab', length_s=2.0),
    lambda: S.rocket_motor('t_rocket', 900.0, length_s=2.0),
    lambda: S.propeller('t_rotor', 19.0, harmonics=40, length_s=2.0),
    lambda: S.fire('t_fire', length_s=2.0),
])
def test_short_loops_are_seamless(make):
    x = make()
    m, bad = S.analyse('t', x, S.Sound(None, 'loop', None))
    assert not bad, bad
    # and the wrap is no different from any other join: rolled half way, the old wrap sits inside and the seam
    # measure of the rolled loop (now joining two inner points) is as quiet
    assert S.seam_ok(S.seam(np.roll(x, len(x) // 2)))


def test_a_gun_loop_has_its_rate_of_fire_and_a_whole_number_of_rounds():
    x = S.gun_burst_loop('t_gun', 23.0, 58.0, length_s=2.0)
    assert len(x) == round(116 * SR / 58.0)
    m, bad = S.analyse('t', x, S.Sound(None, 'loop', 58.0))
    assert not bad, bad
    assert m['rate_hz'] == pytest.approx(58.0, rel=0.03)


def test_the_sonic_boom_passes_its_own_checks():
    x = S.sonic_boom('t_boom', 0.1)
    m, bad = S.analyse('S_FX_Sonic_Boom_T', x, S.Sound(None, 'one_shot', None))
    assert not bad, bad
    assert m['n_length_s'] == pytest.approx(0.1, abs=0.003)


# ---------------------------------------------------------------- sound: the committed files
def test_every_sound_is_committed_with_its_manifest():
    with open(os.path.join(SOUNDS, S.MANIFEST), encoding='utf-8') as f:
        manifest = json.load(f)
    assert set(manifest) == set(S.SOUNDS)
    assert {k for k, v in manifest.items() if v['loop']} == S.LOOPS
    for name, entry in manifest.items():
        x, ch, width, rate = S.read_wav(os.path.join(SOUNDS, name + '.wav'))
        assert (ch, width, rate) == (1, 2, SR), name
        assert len(x) / SR == pytest.approx(entry['seconds'], abs=1e-3), name


def test_the_committed_sounds_keep_every_rule():
    # format, -1 dBFS peaks, no clipping, no DC, lengths, seamless loops, onsets, decays, the boom's N, the guns'
    # rates, far duller than near: what `python tools/fx_sounds.py --check` prints
    assert S.check_dir(SOUNDS) == []


def test_the_engine_script_loops_the_same_sounds():
    loops, from_manifest = U.loop_names(SOUNDS, list(S.SOUNDS))
    assert from_manifest and loops == S.LOOPS
    # its fallback (no manifest) picks the same by name
    fallback = {n for n in S.SOUNDS if any(p in n for p in U.LOOP_NAME_PARTS)}
    assert fallback == S.LOOPS


# ---------------------------------------------------------------- textures
def test_atlas_lays_tiles_row_major_from_the_top_left():
    tiles = [np.full((8, 8, 4), i, dtype=np.uint8) for i in range(7)]
    a = T.atlas(tiles, 3)
    assert a.shape == (24, 24, 4)                     # 7 tiles in 3 columns: 3 rows
    assert a[0, 0, 0] == 0 and a[0, 8, 0] == 1 and a[8, 0, 0] == 3 and a[16, 0, 0] == 6
    assert a[16, 8, 3] == 0                           # the unused cells are empty


def test_spectral_noise_is_in_range_and_tiles():
    n = T.spectral_noise((64, 64), 2.0, 5)
    assert n.min() == pytest.approx(0.0) and n.max() == pytest.approx(1.0)
    inner = np.abs(np.diff(n, axis=1)).mean()
    wrap = np.abs(n[:, 0] - n[:, -1]).mean()
    assert wrap < 3 * inner                           # the right edge runs into the left like any column into the next


def test_heat_ramp_runs_from_soot_to_a_hot_black_body():
    ramp = T.heat_ramp(256, 4)
    assert ramp.shape == (4, 256, 4) and ramp.dtype == np.uint8
    assert np.all(ramp == ramp[0])                     # every row the same
    rgb = ramp[0, :, :3].astype(float) / 255.0
    a = ramp[0, :, 3].astype(float) / 255.0
    luminance = rgb @ np.array([0.2126, 0.7152, 0.0722])
    assert np.all(np.diff(luminance) >= -1.0 / 255)     # brighter as it heats (to within 8-bit rounding)
    assert np.all(np.diff(luminance[::16]) > 0)
    r, g, b = rgb[0]
    assert r > g > b and r < 0.25 and a[0] < 0.25        # cold: dark red-brown, mostly see-through
    assert rgb[-1][0] > 0.95 and rgb[-1][1] > 0.7 and rgb[-1][2] > 0.45 and a[-1] == 1.0   # hot: #FFC88C (3500 K), opaque
    mid = rgb[int(0.6 * 255)]
    assert mid[0] > 0.9 and 0.3 < mid[1] < 0.7 and mid[2] < 0.2   # orange on the way


def test_the_texture_table_layout_and_the_engine_script_agree():
    with open(os.path.join(TEXTURES, 'layout.json'), encoding='utf-8') as f:
        layout = json.load(f)
    assert set(layout) == set(T.TEXTURES)
    from PIL import Image
    for name, (_, cols, rows) in T.TEXTURES.items():
        with Image.open(os.path.join(TEXTURES, name + '.png')) as im:
            assert im.size == (layout[name]['width'], layout[name]['height']), name
            assert im.mode == 'RGBA', name
        assert (layout[name]['cols'], layout[name]['rows']) == (cols, rows), name
        assert layout[name]['width'] % cols == 0 and layout[name]['height'] % rows == 0, name
    # what the materials sample: the data textures with sRGB off, the colours with it on, the same names
    assert set(U.DATA_TEXTURES) | set(U.COLOUR_TEXTURES) == set(T.TEXTURES)
    assert set(U.COLOUR_TEXTURES) == set(T.SRGB)
    assert not set(U.DATA_TEXTURES) & set(U.COLOUR_TEXTURES)
    assert layout['T_FX_SmokeAtlas']['cols'] == 4 and layout['T_FX_Fireball']['cols'] == 8   # M_FX_Smoke's kinds 0 and 1


def test_the_engine_script_names_the_contracts_materials_and_custom_data():
    assert U.MATERIALS == ('M_FX_Smoke', 'M_FX_Additive', 'M_FX_Ribbon', 'M_FX_Munition', 'M_FX_Scorch')
    assert [(i, n) for i, n, _ in U.CUSTOM_DATA] == [(0, 'frame'), (1, 'opacity'), (2, 'heat'), (3, 'tint_r'),
                                                    (4, 'tint_g'), (5, 'tint_b'), (6, 'erosion'), (7, 'lit'),
                                                    (8, 'kind'), (9, 'local_r'), (10, 'local_g'), (11, 'local_b'),
                                                    (12, 'fade_cm')]
    assert (U.KIND_ATLAS, U.KIND_HEAT_FLIPBOOK, U.KIND_COLOUR_FLIPBOOK) == (0, 1, 2)
    assert U.TEX == '/Game/SAM3D/FX/Textures' and U.SND == '/Game/SAM3D/FX/Sounds' and U.MAT == '/Game/SAM3D/FX/Materials'
