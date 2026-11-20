"""tools/map_textures.py draws the map's ground and ribbon textures (roadmap S9, docs/scenery/README.md)."""
import sys
from pathlib import Path

import numpy as np

sys.path.insert(0, str(Path(__file__).resolve().parent.parent))
import map_textures  # noqa: E402


def test_every_channel_is_a_brightness_about_half():
    # The material multiplies the land cover's colour by them: on the average they must leave it as it is.
    for rgba in (map_textures.ground_detail(), map_textures.ribbon()):
        assert rgba.min() >= 0.0 and rgba.max() <= 1.0
        for c in range(4):
            assert abs(float(rgba[..., c].mean()) - 0.5) < 0.05, c
            assert float(rgba[..., c].std()) > 0.05, c  # grain, not flat


def test_the_same_seed_gives_the_same_pixels():
    assert np.array_equal(map_textures.ground_detail(7), map_textures.ground_detail(7))
    assert not np.array_equal(map_textures.ground_detail(7), map_textures.ground_detail(8))


def test_the_ground_tiles_both_ways_and_the_ribbon_along():
    # A tile's last row (column) must run on into its first as smoothly as any two rows inside it.
    def seam(a, axis):
        inner = np.abs(np.diff(a, axis=axis)).mean()
        wrap = np.abs(np.take(a, 0, axis=axis) - np.take(a, -1, axis=axis)).mean()
        return wrap / inner

    detail = map_textures.ground_detail()
    for c in range(4):
        assert seam(detail[..., c], 0) < 1.5 and seam(detail[..., c], 1) < 1.5, c
    rib = map_textures.ribbon()
    assert rib.shape == (map_textures.RIBBON_V, map_textures.RIBBON_U, 4)
    for c in range(4):
        assert seam(rib[..., c], 1) < 1.5, c


def test_the_normal_map_is_unit_length_and_faces_up():
    n = map_textures.ground_normal()[..., :3] * 2.0 - 1.0
    assert np.allclose(np.linalg.norm(n, axis=-1), 1.0, atol=1e-6)
    assert float(n[..., 2].min()) > 0.3


def test_build_writes_the_three_files(tmp_path):
    _, paths = map_textures.build(str(tmp_path))
    assert sorted(paths) == ['T_MapGroundDetail', 'T_MapGroundNormal', 'T_MapRibbon']
    for p in paths.values():
        assert Path(p).stat().st_size > 1000
