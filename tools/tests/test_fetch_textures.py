"""Tests for tools/fetch_textures.py: the texture register's rules, and the maps found in Poly Haven's and ambientCG's
answers, without the network."""
import json
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))

import fetch_textures  # noqa: E402

GOOD = {"id": "ploughed_soil", "site": "polyhaven", "asset": "brown_mud_02", "licence": "CC0-1.0",
        "origin": "https://polyhaven.com/a/brown_mud_02", "resolution": "2k"}


def test_the_register_in_the_data_passes():
    register = json.loads(fetch_textures.REGISTER.read_text(encoding="utf-8"))
    assert fetch_textures.check_register(register) == []


def test_quixels_zip_members_and_its_normal_flipped_to_directx():
    import io

    from PIL import Image

    names = ["Gravel_Ground_x_2K_AO.jpg", "Gravel_Ground_x_2K_BaseColor.jpg", "Gravel_Ground_x_2K_Bump.jpg",
             "Gravel_Ground_x_2K_Gloss.jpg", "Gravel_Ground_x_2K_Normal.jpg", "Gravel_Ground_x_2K_Roughness.jpg", "x.json"]
    assert fetch_textures.quixel_members(names) == {
        "albedo": "Gravel_Ground_x_2K_BaseColor.jpg", "normal": "Gravel_Ground_x_2K_Normal.jpg",
        "roughness": "Gravel_Ground_x_2K_Roughness.jpg", "ao": "Gravel_Ground_x_2K_AO.jpg"}
    try:
        fetch_textures.quixel_members(names[:2])
        assert False, "a zip without a normal and a roughness is refused"
    except ValueError:
        pass
    src = io.BytesIO()
    Image.new("RGB", (2, 2), (10, 200, 250)).save(src, format="PNG")
    out = Image.open(io.BytesIO(fetch_textures.quixel_normal_dx(src.getvalue()))).convert("RGB")
    assert out.getpixel((0, 0)) == (10, 55, 250)  # green inverted, red and blue kept
    assert fetch_textures.check_register({"textures": [dict(GOOD, site="quixel", licence="Fab-Standard")]}) == []


def test_the_register_rules():
    assert fetch_textures.check_register({"textures": [GOOD]}) == []
    assert any("twice" in p for p in fetch_textures.check_register({"textures": [GOOD, GOOD]}))
    assert any("site" in p for p in fetch_textures.check_register({"textures": [dict(GOOD, site="fab")]}))
    assert any("licence" in p for p in fetch_textures.check_register({"textures": [dict(GOOD, licence="CC-BY-NC-4.0")]}))
    assert any("author" in p for p in fetch_textures.check_register({"textures": [dict(GOOD, licence="CC-BY-4.0")]}))
    assert any("lower-case" in p for p in fetch_textures.check_register({"textures": [dict(GOOD, id="Ploughed")]}))
    assert any("origin" in p for p in fetch_textures.check_register({"textures": [dict(GOOD, origin="")]}))
    assert any("resolution" in p for p in fetch_textures.check_register({"textures": [dict(GOOD, resolution="8k")]}))
    assert any("scan_m" in p for p in fetch_textures.check_register({"textures": [dict(GOOD, scan_m=0)]}))
    assert fetch_textures.check_register({"textures": [dict(GOOD, scan_m=2.1)]}) == []
    assert fetch_textures.check_register({"textures": [dict(GOOD, crop_u=[0.17, 0.8])]}) == []
    for bad in ([0.5], [0.6, 0.4], [0.5, 0.6], [-0.1, 0.8], [0.2, 1.2], "0.2-0.8"):
        assert any("crop_u" in p for p in fetch_textures.check_register({"textures": [dict(GOOD, crop_u=bad)]})), bad
    assert fetch_textures.check_register({"textures": [dict(GOOD, grade=[0.84, 1.29, 1.1])]}) == []
    for bad in ([1.0, 1.0], [0.1, 1.0, 1.0], [1.0, 4.0, 1.0], [1.0, True, 1.0], "greener"):
        assert any("grade" in p for p in fetch_textures.check_register({"textures": [dict(GOOD, grade=bad)]})), bad


def test_a_grade_scales_each_channel_in_linear_light_and_keeps_the_fetched_albedo(tmp_path):
    """A.3.40: sRGB 128 is 0.216 linear; times 2 it is 0.432, sRGB 175; times 0.5, 0.108, sRGB 92; a gain past white clips."""
    pytest = __import__("pytest")
    Image = pytest.importorskip("PIL.Image")
    import io

    src = io.BytesIO()
    Image.new("RGB", (4, 4), (128, 128, 250)).save(src, format="PNG")
    out = Image.open(io.BytesIO(fetch_textures.grade_albedo(src.getvalue(), [2.0, 0.5, 2.0])))
    r, g, b = out.getpixel((0, 0))
    assert abs(r - 175) <= 1 and abs(g - 92) <= 1 and b == 255
    folder = tmp_path / "meadow"
    folder.mkdir()
    (folder / "albedo.png").write_bytes(src.getvalue())
    assert fetch_textures.write_graded(dict(GOOD, grade=[2.0, 0.5, 2.0]), folder)
    assert (folder / "albedo_graded.png").exists()
    assert Image.open(folder / "albedo.png").getpixel((0, 0)) == (128, 128, 250)
    assert not fetch_textures.write_graded(GOOD, folder)  # graded no more: the old file goes
    assert not (folder / "albedo_graded.png").exists()


def test_crop_u_keeps_the_middle_across_the_width():
    pytest = __import__("pytest")
    Image = pytest.importorskip("PIL.Image")
    import io

    # Black at each side (a trench), white between, 100 wide: [0.2, 0.8] keeps only the white, at the full size.
    img = Image.new("L", (100, 40), 0)
    img.paste(255, (20, 0, 80, 40))
    src = io.BytesIO()
    img.save(src, format="PNG")
    out = Image.open(io.BytesIO(fetch_textures.crop_u(src.getvalue(), 0.2, 0.8)))
    assert out.size == (100, 40)
    assert min(out.getdata()) == 255


def test_poly_havens_maps_prefer_jpeg_and_the_directx_normal():
    files = {
        "Diffuse": {"2k": {"png": {"url": "d.png", "size": 20}, "jpg": {"url": "d.jpg", "size": 5}}},
        "nor_gl": {"2k": {"jpg": {"url": "gl.jpg", "size": 6}}},
        "nor_dx": {"2k": {"png": {"url": "dx.png", "size": 24}}},
        "Rough": {"2k": {"jpg": {"url": "r.jpg", "size": 2}}},
        "AO": {"1k": {"jpg": {"url": "ao1k.jpg", "size": 1}}},
    }
    assert fetch_textures.polyhaven_urls(files, "2k") == {"albedo": "d.jpg", "normal": "dx.png", "roughness": "r.jpg"}
    assert sum(f["size"] for f in fetch_textures.polyhaven_files(files, "2k").values()) == 31
    try:
        fetch_textures.polyhaven_urls({"Diffuse": files["Diffuse"]}, "2k")
        assert False, "a texture without its normal must be refused"
    except ValueError as e:
        assert "normal" in str(e)


def test_ambientcgs_zip_and_its_members():
    assert fetch_textures.ambientcg_zip_url("Ground054", "2k") == "https://ambientcg.com/get?file=Ground054_2K-JPG.zip"
    names = ["Ground054_2K-JPG_Color.jpg", "Ground054_2K-JPG_NormalGL.jpg", "Ground054_2K-JPG_NormalDX.jpg",
             "Ground054_2K-JPG_Roughness.jpg", "Ground054_2K-JPG_AmbientOcclusion.jpg", "Ground054.png", "Ground054_2K-JPG.usdc"]
    assert fetch_textures.ambientcg_members(names) == {
        "albedo": "Ground054_2K-JPG_Color.jpg", "normal": "Ground054_2K-JPG_NormalDX.jpg",
        "roughness": "Ground054_2K-JPG_Roughness.jpg", "ao": "Ground054_2K-JPG_AmbientOcclusion.jpg"}
    try:
        fetch_textures.ambientcg_members(["Ground054_2K-JPG_Color.jpg", "Ground054_2K-JPG_NormalGL.jpg", "Ground054_2K-JPG_Roughness.jpg"])
        assert False, "a zip with only the OpenGL normal must be refused"
    except ValueError as e:
        assert "normal" in str(e)


def test_orm_packs_the_occlusion_and_the_roughness():
    pytest = __import__("pytest")
    Image = pytest.importorskip("PIL.Image")
    import io

    def png(value, size):
        out = io.BytesIO()
        Image.new("L", (size, size), value).save(out, format="PNG")
        return out.getvalue()

    orm = Image.open(io.BytesIO(fetch_textures.pack_orm(png(200, 64), png(90, 64), size=32)))
    assert orm.size == (32, 32) and orm.mode == "RGB"
    assert orm.getpixel((5, 5)) == (90, 200, 0)
    # Without an occlusion map: none (white).
    orm = Image.open(io.BytesIO(fetch_textures.pack_orm(png(40, 32), None, size=32)))
    assert orm.getpixel((0, 0)) == (255, 40, 0)


def test_a_maps_extension_by_its_bytes():
    assert fetch_textures.extension(b"\x89PNG\r\n\x1a\n....") == ".png"
    assert fetch_textures.extension(b"\xff\xd8\xff\xe0....") == ".jpg"
    try:
        fetch_textures.extension(b"<html>")
        assert False, "an HTML error page is not a map"
    except ValueError:
        pass


def test_every_ground_layers_texture_is_in_the_register():
    register = json.loads(fetch_textures.REGISTER.read_text(encoding="utf-8"))
    ids = {t["id"] for t in register["textures"]}
    layers = json.loads((fetch_textures.ROOT / "data" / "scenery" / "ground_layers.json").read_text(encoding="utf-8"))["layers"]
    for layer in layers:
        assert not layer.get("texture") or layer["texture"] in ids, f"{layer['id']}: {layer['texture']} is not in textures.json"
