import json
import subprocess
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent.parent))
import plot_lab  # noqa: E402


def load_look():
    return json.loads(plot_lab.LOOK.read_text(encoding="utf-8"))


def test_the_look_target_names_its_traits_and_pictures():
    look = load_look()
    assert [t["id"] for t in look["traits"]] == [f"K{i}" for i in range(1, 15)]
    for t in look["traits"]:
        assert t["name"] and t["target"], t["id"]
    assert [p["id"] for p in look["pictures"]] == ["K-a", "K-b", "K-c", "K-d", "K-e", "K-f"]
    assert look["folder"].startswith("reference/")


def test_the_look_targets_pictures_are_never_tracked_by_git():
    # The screenshots are the publisher's: git must ignore every one of them.
    look = load_look()
    paths = [f'{look["folder"]}/{p["file"]}' for p in look["pictures"]]
    out = subprocess.run(["git", "check-ignore", *paths], cwd=plot_lab.ROOT, capture_output=True, text=True)
    assert sorted(out.stdout.split()) == sorted(paths)


def test_the_page_links_the_pictures_this_pc_has_and_lists_the_traits(tmp_path):
    look = load_look()
    pics = tmp_path / look["folder"]
    pics.mkdir(parents=True)
    (pics / look["pictures"][0]["file"]).write_bytes(b"x")  # only the first picture on this "PC"
    folder = tmp_path / "out" / "plots" / "t1"
    folder.mkdir(parents=True)
    strip = plot_lab.look_strip(look, folder, root=tmp_path)
    assert f'src="../../../{look["folder"]}/{look["pictures"][0]["file"]}"' in strip
    assert look["pictures"][1]["file"] not in strip
    assert "K14 Every distance" in strip


def test_the_page_says_when_the_pictures_are_missing(tmp_path):
    strip = plot_lab.look_strip(load_look(), tmp_path, root=tmp_path)
    assert "not on this PC" in strip
    assert plot_lab.look_strip(None, tmp_path) == ""
