# System data files

Each air-defence system gets one JSON file of parameters used by `simcore`.

Rules:
1. **Every number needs a source.** Fill in `source` with the source id from `docs/SOURCES.md`, page and table, and
   set `confidence` (`high` / `medium` / `low` / `estimate`).
2. **Public and declassified material only, best tier first.** Tiers are in `docs/SOURCES.md`: documents from
   countries that operated the system, then operator accounts, then SAM Simulator, then operator-country reference
   pages. Western "facts and stats" pages (tier 5) are not used for new values; old citations to them are being
   replaced (roadmap 6.8).
3. When sources disagree, record the range in `notes` and say which value you picked and why
   (`docs/research/OSA_RESEARCH_2026-10.md` §10 lists the known conflicts for the Osa).
4. Values are SI (m, s, kg, N, W, Hz, dB, radians unless the key ends in `_deg`).

`_template.json` shows the schema. Copy it to e.g. `osa.json` and fill it in as you research.
None of the demo or test numbers in the code come from any real system; they are generic placeholders.
