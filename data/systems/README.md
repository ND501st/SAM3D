# System data files

Each air-defence system gets one JSON file of parameters used by `simcore`.

Rules:
1. **Every number needs a source.** Fill in `source` with document, page and table, and set `confidence` (`high` / `medium` / `low` / `estimate`).
2. **Public and declassified material only.** See `docs/SOURCES.md`.
3. When sources disagree, record the range in `notes` and say which value you picked and why.
4. Values are SI (m, s, kg, N, W, Hz, dB, radians unless the key ends in `_deg`).

`_template.json` shows the schema. Copy it to e.g. `osa.json` and fill it in as you research.
None of the demo or test numbers in the code come from any real system; they are generic placeholders.
