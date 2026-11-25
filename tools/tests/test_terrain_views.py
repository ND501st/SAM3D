import json
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent.parent))
import terrain_views  # noqa: E402


def test_every_photograph_of_the_views_is_openly_licensed_and_linked():
    data = json.loads(terrain_views.VIEWS.read_text(encoding="utf-8"))
    for pid, p in data["photos"].items():
        assert terrain_views.licence_ok(p["licence"]), pid
        assert p["page_url"].startswith("https://"), pid
        assert p["author"], pid
    for v in data["views"]:
        assert v["photos"], v["id"]
        for pid in v["photos"]:
            assert pid in data["photos"], (v["id"], pid)


def test_licence_rule():
    assert terrain_views.licence_ok("CC BY-SA 3.0")
    assert terrain_views.licence_ok("CC BY 4.0")
    assert terrain_views.licence_ok("CC0 1.0")
    assert terrain_views.licence_ok("Public domain")
    assert not terrain_views.licence_ok("CC BY-NC 4.0")
    assert not terrain_views.licence_ok("All rights reserved")


def test_thumbnail_urls():
    c = terrain_views.thumbnail_url("https://commons.wikimedia.org/wiki/File:Road_-_panoramio_(26).jpg")
    assert c == "https://commons.wikimedia.org/wiki/Special:FilePath/Road_-_panoramio_(26).jpg?width=320"
    assert terrain_views.thumbnail_url("https://fortepan.hu/hu/photos/?id=8920") == \
        "https://fortepan.download/file/fortepan-eu/1600/fortepan_8920.jpg"
    assert terrain_views.thumbnail_url("https://example.com/x") is None


def test_the_page_from_a_run(tmp_path):
    views = tmp_path / "views.json"
    views.write_text(json.dumps({
        "views": [{"id": "v1", "photos": ["P1"], "must_see": "furrows"},
                  {"id": "v7", "photos": [], "must_see": "tree lines"},
                  {"id": "v5", "photos": [], "must_see": "sheds"}],
        "photos": {"P1": {"title": "A <field>", "page_url": "https://fortepan.hu/hu/photos/?id=1", "licence": "CC BY-SA 3.0",
                          "author": "Someone"}}}), encoding="utf-8")
    run, old = tmp_path / "new", tmp_path / "old"
    run.mkdir()
    old.mkdir()
    (run / "v1.png").write_bytes(b"x")
    (run / "v1.cost.json").write_text(json.dumps({"gpu_ms": 4.5, "game_ms": 2.0, "render_ms": 3.0, "samples": 40}), encoding="utf-8")
    (old / "v1.cost.json").write_text(json.dumps({"gpu_ms": 4.0, "game_ms": 2.0, "render_ms": 3.0, "samples": 40}), encoding="utf-8")
    for h in ("000", "090"):
        (run / f"v7_h{h}.png").write_bytes(b"x")
    (run / "v5.missing.txt").write_text("the map has no farmyard site kind yet", encoding="utf-8")
    page = terrain_views.build_page(views, run, old, "t1")
    assert "GPU 4.50 ms (was 4.00)" in page
    assert "v7_h000.png" in page and "v7_h090.png" in page
    assert "Not rendered: the map has no farmyard site kind yet" in page
    assert "CC BY-SA 3.0" in page and "Someone" in page
    assert "A &lt;field&gt;" in page  # escaped
    assert page.index("v1") < page.index("v7") < page.index("v5")


def test_shots_and_costs_of_a_folder(tmp_path):
    (tmp_path / "v1.png").write_bytes(b"x")
    (tmp_path / "v10.png").write_bytes(b"x")  # another view whose id starts the same
    assert terrain_views.shots_of(tmp_path, "v1") == ["v1.png"]
    (tmp_path / "v1.cost.json").write_text("{bad", encoding="utf-8")
    assert terrain_views.read_costs(tmp_path) == {}
    assert terrain_views.read_costs(tmp_path / "nothing") == {}
