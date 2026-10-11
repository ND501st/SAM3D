"""Check the citations in data/: every "source" names a source registered in docs/SOURCES.md (or says it is a SAM3D
estimate, a placeholder, or a derivation in docs/), and none cites a tier-5 source (roadmap 6.8).

    python tools/check_citations.py          # lists the problems; exit code 1 if there are any

A source is "named" when its id appears as a word in the string (e.g. "voenizdat_osa_1991 p165"). Tier 5 is the
Western "facts and stats" tier: kept in docs/SOURCES.md for history, not for new decisions.
"""
import glob
import json
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
# Words that make a citation acceptable without a registered source: SAM3D's own estimates and placeholders, and
# derivations written up in docs/.
OWN = ("sam3d", "estimate", "assumption", "placeholder", "docs/", "derived", "game value")


def registered_sources(text=None):
    """{id: tier string} from docs/SOURCES.md's tables."""
    text = text if text is not None else (ROOT / "docs" / "SOURCES.md").read_text(encoding="utf-8")
    return {m.group(1): m.group(2).strip() for m in re.finditer(r"^\| `([a-z0-9_]+)` \| ([^|]+)\|", text, re.M)}


def citations(obj, path=""):
    """Every (json path, source string) in a data file."""
    if isinstance(obj, dict):
        for k, v in obj.items():
            p = f"{path}.{k}" if path else k
            if k == "source" and isinstance(v, str):
                yield path or "(top)", v
            else:
                yield from citations(v, p)
    elif isinstance(obj, list):
        for i, v in enumerate(obj):
            yield from citations(v, f"{path}[{i}]")


def problems(data_files, sources):
    out = []
    for f in data_files:
        rel = Path(f).resolve().relative_to(ROOT).as_posix()
        for path, s in citations(json.loads(Path(f).read_text(encoding="utf-8"))):
            words = set(re.findall(r"[a-z][a-z0-9_]*", s))
            named = [w for w in words if w in sources]
            tier5 = [w for w in named if sources[w].startswith("5")]
            if tier5:
                out.append(f"{rel}: {path}: cites tier-5 {', '.join(sorted(tier5))}")
            elif not named and not any(o in s.lower() for o in OWN):
                out.append(f"{rel}: {path}: names no registered source: {s[:90]!r}")
    return out


def main(argv=None):
    files = sorted(glob.glob(str(ROOT / "data" / "**" / "*.json"), recursive=True))
    files = [f for f in files if not Path(f).name.startswith("_")]  # templates
    found = problems(files, registered_sources())
    for p in found:
        print(p)
    print(f"{len(files)} data files, {len(found)} problem(s)")
    return 1 if found else 0


if __name__ == "__main__":
    sys.exit(main())
