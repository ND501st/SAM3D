"""tools/em_view.py builds a self-contained viewer page from an em_demo run file."""
import json
import sys
from pathlib import Path

import pytest

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))

import em_view  # noqa: E402


def tiny_run():
    return {
        "meta": {
            "title": "test run", "origin": [49.2, 16.6],
            "layers": {"1": "truth", "512": "tracks"}, "nodes": {"3": "CP"},
            "terrain": {"x0": 0, "y0": 0, "step": 2000, "cols": 2, "rows": 2, "h": [0, 10, 20, 30]},
            "events": [{"t": 1.0, "k": "info", "x": "a </script> b"}],
            "checks": [{"name": "1 something", "ok": True}],
        },
        "frames": [{"t": 2.0, "prims": [{"s": 5, "l": 1, "a": [0, 0], "c": 4294967295, "t": "SAM"}]}],
    }


def test_page_embeds_the_run_and_cannot_be_broken_out_of():
    page = em_view.build_page(tiny_run())
    assert "<title>EM Debug Viewer</title>" in page
    assert '"title":"test run"' in page
    assert "</script> b" not in page  # escaped so the embedded JSON can't close the script tag
    assert '"truth"' in page and "c2_links" in page  # default layers listed


def test_rejects_other_json():
    with pytest.raises(ValueError):
        em_view.build_page({"something": 1})


def test_cli_writes_the_page(tmp_path):
    run = tmp_path / "run.json"
    run.write_text(json.dumps(tiny_run()), encoding="utf-8")
    out = tmp_path / "view.html"
    assert em_view.main([str(run), "--out", str(out)]) == 0
    assert out.read_text(encoding="utf-8").startswith("<!doctype html>")
    assert em_view.main([str(tmp_path / "missing.json")]) == 2
