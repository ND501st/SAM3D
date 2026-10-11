"""Tests for tools/check_citations.py: the data cites registered sources, and no tier-5 one (roadmap 6.8)."""
import json
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))

import check_citations  # noqa: E402

SOURCES = """
| Id | Tier | Source | Notes |
|---|---|---|---|
| `manual_a` | 1 | A manual | |
| `veteran_b` | 2 | A veteran | |
| `facts_c` | 5 | A Western facts page | To be replaced |
"""


def _write(obj):
    # Under the repository (problems() reports paths relative to it), in the git-ignored out/ folder.
    f = check_citations.ROOT / "out" / "test_check_citations" / "data.json"
    f.parent.mkdir(parents=True, exist_ok=True)
    f.write_text(json.dumps(obj), encoding="utf-8")
    return f


def test_registered_sources_reads_the_tiers():
    s = check_citations.registered_sources(SOURCES)
    assert s == {"manual_a": "1", "veteran_b": "2", "facts_c": "5"}


def test_problems_flag_tier_5_and_unnamed_sources():
    sources = check_citations.registered_sources(SOURCES)
    f = _write({
        "a": {"value": 1, "source": "manual_a p12", "confidence": "high"},
        "b": {"value": 2, "source": "facts_c", "confidence": "low"},
        "c": [{"source": "somebody's blog"}],
        "d": {"source": "sam3d estimate"},
        "e": {"source": "derived in docs/research/X.md"},
    })
    found = check_citations.problems([str(f)], sources)
    assert len(found) == 2
    assert any("b: cites tier-5 facts_c" in p for p in found)
    assert any("c[0]: names no registered source" in p for p in found)


def test_the_repository_data_is_clean():
    assert check_citations.main([]) == 0
