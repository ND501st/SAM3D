"""Write the crew action map: every action of a system's console and how it connects (docs/crew/, pillar 3).

Usage:  python tools/crew_map.py [system] [--out FILE] [--check]

    system   default osa_ak: reads data/procedures/<system>.json, data/cabins/<system>.json and
             data/crew/<system>_crew.json
    --out    default docs/crew/generated/<system>_action_map.md (checked in)
    --check  exit 1 if the checked-in map differs from what the data gives now (tools/tests/test_crew_map.py runs it)

Standard library only. Everything in the map comes from the data; nothing is written by hand here. Sections:

    1. Seats: duties, displays, lamps, and every control with what it feeds, the drill steps that move it and the
       orders that lead to it
    2. Subsystems: what switches each on (with the seat owning every control in the chain), its warm-up, what
       depends on it, the report it triggers
    3. Inputs (world facts): who or what sets each, the seats and conditions it needs, what reads it
    4. Drills (procedures): each step, and the other seats each step waits for (through subsystems and facts)
    5. Who waits for whom: the seat-to-seat matrix from the drills' waits
    6. Orders: what each order starts or asks for
    7. Tasks: the conditions, and every seat action in their dependency chains
    8. Callouts: the catalogue, with what triggers the automatic reports
"""
import argparse
import collections
import json
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
SEATS = ["commander", "deputy_commander", "senior_operator", "operator", "driver"]  # the Osa's, when a console lists none


def load(system):
    def read(rel):
        return json.loads((ROOT / rel).read_text(encoding="utf-8"))
    con = read(f"data/procedures/{system}.json")
    cab_path = ROOT / f"data/cabins/{system}.json"
    cab = read(f"data/cabins/{system}.json") if cab_path.exists() else {"panels": [], "lamps": [], "displays": []}
    crew = read(f"data/crew/{system}_crew.json")
    return con, cab, crew


def pos_of(c):
    """A control condition's position: "is" (equals) or "at_least" (written ">=n")."""
    return c["is"] if "is" in c else f">={c.get('at_least')}"


def cond_text(c, owner=None):
    """One condition as text, with the owning seat for a control."""
    if "control" in c:
        o = f" [{owner.get(c['control'], '?')}]" if owner else ""
        return f"`{c['control']}`={pos_of(c)}{o}"
    if "subsystem" in c:
        return f"`{c['subsystem']}` {c['is']}"
    return f"input `{c['input']}`={str(c['is']).lower()}"


class Graph:
    def __init__(self, con, crew):
        self.con, self.crew = con, crew
        self.owner = {c["id"]: c.get("station", "?") for c in con["controls"]}
        self.subs = {s["id"]: s for s in con.get("subsystems", [])}
        self.facts = {f["input"]: f for f in crew.get("facts", [])}

    def seats_for(self, cond, seen=None):
        """The seats whose actions a condition depends on, through subsystems (all alternatives) and facts."""
        seen = seen if seen is not None else set()
        out = set()
        if "control" in cond:
            out.add(self.owner.get(cond["control"], "?"))
            return out
        key = ("s", cond.get("subsystem")) if "subsystem" in cond else ("i", cond.get("input"))
        if key in seen:
            return out
        seen.add(key)
        if "subsystem" in cond:
            s = self.subs.get(cond["subsystem"], {})
            for c in s.get("enable", []) + s.get("enable_any", []) + s.get("prerequisites", []):
                out |= self.seats_for(c, seen)
        else:
            f = self.facts.get(cond["input"], {})
            out |= set(f.get("seats", []))
            for c in f.get("needs", []):
                out |= self.seats_for(c, seen)
        return out

    def actions_for(self, cond, seen=None):
        """Every (seat, control, position) in a condition's chain (alternatives included, marked)."""
        seen = seen if seen is not None else set()
        out = []
        if "control" in cond:
            out.append((self.owner.get(cond["control"], "?"), cond["control"], pos_of(cond)))
            return out
        key = ("s", cond.get("subsystem")) if "subsystem" in cond else ("i", cond.get("input"))
        if key in seen:
            return out
        seen.add(key)
        if "subsystem" in cond:
            s = self.subs.get(cond["subsystem"], {})
            for c in s.get("enable", []) + s.get("prerequisites", []) + s.get("enable_any", []):
                out += self.actions_for(c, seen)
        else:
            for c in self.facts.get(cond["input"], {}).get("needs", []):
                out += self.actions_for(c, seen)
        return out


def build(system):
    con, cab, crew = load(system)
    seat_ids = [st["id"] for st in con.get("stations", [])] or SEATS
    g = Graph(con, crew)
    controls = con["controls"]
    procs = con.get("procedures", [])
    st_label = {s["id"]: s["label"] for s in con.get("stations", [])}
    seat_info = {s["station"]: s for s in crew.get("seats", [])}
    callouts = {c["id"]: c for c in crew.get("callouts", [])}
    panel_station = {p["id"]: p.get("station") for p in cab.get("panels", [])}

    feeds = collections.defaultdict(list)       # control -> [(subsystem, how)]
    reverse = collections.defaultdict(list)     # subsystem/input -> [subsystem that needs it]
    for s in con.get("subsystems", []):
        for how, lst in (("enable", s.get("enable", [])), ("any of", s.get("enable_any", [])),
                         ("prerequisite", s.get("prerequisites", [])), ("release", s.get("release", []))):
            for c in lst:
                if "control" in c:
                    feeds[c["control"]].append((s["id"], how, pos_of(c)))
                elif "subsystem" in c:
                    reverse[("s", c["subsystem"])].append(s["id"])
                else:
                    reverse[("i", c["input"])].append(s["id"])
    steps_of = collections.defaultdict(list)    # control -> [(procedure, step no, position)]
    proc_waits_input = collections.defaultdict(list)
    for p in procs:
        for i, st in enumerate(p["steps"]):
            steps_of[st["control"]].append((p["id"], i + 1, st["position"]))
            for w in st.get("wait_for", []):
                if "input" in w:
                    proc_waits_input[w["input"]].append(f"`{p['id']}` step {i + 1}")
    orders_of_proc = collections.defaultdict(list)
    for o in crew.get("orders", []):
        for pr in o.get("procedures", []):
            orders_of_proc[pr["procedure"]].append(o["callout"])
    reports_of = collections.defaultdict(list)
    for r in crew.get("reports", []):
        w = r["when"]
        key = ("s", w["subsystem"]) if "subsystem" in w else ("i", w["input"])
        reports_of[key].append(r["callout"])
    sensor_inputs = set()
    for blk in con.get("sensors", {}).values():
        if isinstance(blk, dict):
            for k, v in blk.items():
                if k.endswith("_input") and isinstance(v, str):
                    sensor_inputs.add(v)

    L = []
    w = L.append
    w(f"# {con.get('display_name', system)}: crew action map")
    w("")
    w(f"Generated by `python tools/crew_map.py {system}` from `data/procedures/{system}.json`, "
      f"`data/cabins/{system}.json` and `data/crew/{system}_crew.json`. Do not edit by hand: change the data and run "
      "the tool (`tools/tests/test_crew_map.py` fails while this file is out of date). What the sections mean: "
      "`docs/crew/02_INTERDEPENDENCE.md`.")
    w("")
    n_ctrl, n_sub, n_in, n_proc = len(controls), len(g.subs), len(con.get("inputs", [])), len(procs)
    w(f"{n_ctrl} controls, {n_sub} subsystems, {n_in} inputs, {n_proc} drills, {len(crew.get('orders', []))} orders, "
      f"{len(callouts)} callouts, {len(crew.get('tasks', []))} tasks.")
    w("")

    w("## 1. Seats")
    for seat in seat_ids:
        info = seat_info.get(seat, {})
        w("")
        w(f"### {st_label.get(seat, seat)} (`{seat}`)" + (f": {info['title_ru']}" if info.get("title_ru") else ""))
        w("")
        if info.get("duties"):
            w("Duties: " + "; ".join(info["duties"]) + ".")
            w("")
        disp = info.get("displays", [])
        w("Displays: " + (", ".join(f"`{d}`" for d in disp) if disp else "none") + ".")
        lamps = [l.get("label") or l["id"] for l in cab.get("lamps", []) if panel_station.get(l.get("panel")) == seat]
        w("")
        w("Lamps: " + (", ".join(lamps) if lamps else "none") + ".")
        w("")
        w("| Control | Label | Positions | Feeds | Drill steps | Orders |")
        w("|---|---|---|---|---|---|")
        for c in controls:
            if c.get("station") != seat:
                continue
            pos = c.get("position_labels")
            positions = " / ".join(pos) if pos else c.get("kind", "")
            fd = "; ".join(f"`{s}` ({how} ={v})" for s, how, v in feeds.get(c["id"], [])) or "–"
            sp = "; ".join(f"`{p}` {i}→{v}" for p, i, v in steps_of.get(c["id"], [])) or "–"
            ords = sorted({o for p, _, _ in steps_of.get(c["id"], []) for o in orders_of_proc.get(p, [])})
            label = c.get("label", "").replace("|", "/")
            w(f"| `{c['id']}` | {label} | {positions} | {fd} | {sp} | {', '.join(f'`{o}`' for o in ords) or '–'} |")

    w("")
    w("## 2. Subsystems")
    w("")
    w("| Subsystem | Name | Switched on by (seat owning each control) | Warm-up s | Seats in its chain | Needed by | Report |")
    w("|---|---|---|---|---|---|---|")
    for s in con.get("subsystems", []):
        parts = []
        if s.get("enable"):
            parts.append("all of " + ", ".join(cond_text(c, g.owner) for c in s["enable"]))
        if s.get("enable_any"):
            parts.append("any of " + ", ".join(cond_text(c, g.owner) for c in s["enable_any"]))
        if s.get("prerequisites"):
            parts.append("while " + ", ".join(cond_text(c, g.owner) for c in s["prerequisites"]))
        if s.get("latching"):
            parts.append("latching; released by " + ", ".join(cond_text(c, g.owner) for c in s.get("release", [])))
        t = s.get("start_time_s")
        t = t.get("value") if isinstance(t, dict) else t
        seats = sorted(g.seats_for({"subsystem": s["id"], "is": "ready"}))
        need = sorted(set(reverse.get(("s", s["id"]), [])))
        rep = reports_of.get(("s", s["id"]), [])
        w(f"| `{s['id']}` | {s.get('name', '')} | {'; '.join(parts) or '–'} | {t if t else '–'} | {', '.join(seats) or '–'} | "
          f"{', '.join(f'`{n}`' for n in need) or '–'} | {', '.join(f'`{r}`' for r in rep) or '–'} |")

    w("")
    w("## 3. Inputs (world facts)")
    w("")
    w("| Input | Set by | Seats it needs | Conditions it needs | Read by subsystems | Waited for by drills | Report |")
    w("|---|---|---|---|---|---|---|")
    for i in con.get("inputs", []):
        f = g.facts.get(i["id"], {})
        set_by = f.get("set_by") or ("engagement model (sensors)" if i["id"] in sensor_inputs else "launcher / sim")
        need = ", ".join(cond_text(c, g.owner) for c in f.get("needs", [])) or "–"
        reads = sorted(set(reverse.get(("i", i["id"]), [])))
        rep = reports_of.get(("i", i["id"]), [])
        w(f"| `{i['id']}` | {set_by} | {', '.join(f.get('seats', [])) or '–'} | {need} | {', '.join(f'`{r}`' for r in reads) or '–'} | "
          f"{', '.join(proc_waits_input.get(i['id'], [])) or '–'} | {', '.join(f'`{r}`' for r in rep) or '–'} |")

    w("")
    w("## 4. Drills (procedures)")
    wait_matrix = collections.defaultdict(set)
    for p in procs:
        seat = p["steps"][0] and g.owner.get(p["steps"][0]["control"], "?")
        w("")
        orders = orders_of_proc.get(p["id"], [])
        w(f"### `{p['id']}` ({seat}): {p.get('name', '')}" + (f" — ordered by {', '.join(f'`{o}`' for o in orders)}" if orders else ""))
        w("")
        w("| Step | Control | Position | Waits for | Other seats it waits on |")
        w("|---|---|---|---|---|")
        for i, st in enumerate(p["steps"]):
            waits = st.get("wait_for", [])
            others = set()
            for c in waits:
                others |= g.seats_for(c)
            others.discard(seat)
            others.discard("?")
            for o in others:
                wait_matrix[(seat, o)].add(p["id"])
            w(f"| {i + 1} | `{st['control']}` | {st['position']} | {', '.join(cond_text(c, g.owner) for c in waits) or '–'} | "
              f"{', '.join(sorted(others)) or '–'} |")

    w("")
    w("## 5. Who waits for whom")
    w("")
    w("Rows wait for columns: the drills of the row's seat that wait on something the column's seat does.")
    w("")
    w("| waits → for | " + " | ".join(seat_ids) + " |")
    w("|---|" + "---|" * len(seat_ids))
    for a in seat_ids:
        cells = []
        for b in seat_ids:
            ps = sorted(wait_matrix.get((a, b), []))
            cells.append("–" if a == b or not ps else ", ".join(f"`{p}`" for p in ps))
        w(f"| **{a}** | " + " | ".join(cells) + " |")

    w("")
    w("## 6. Orders")
    w("")
    w("| Order | ru | en | Starts | Task |")
    w("|---|---|---|---|---|")
    for o in crew.get("orders", []):
        c = callouts.get(o["callout"], {})
        starts = "; ".join(f"{p['station']}: `{p['procedure']}`" + (" (queued)" if p.get("queue") else "") for p in o.get("procedures", []))
        w(f"| `{o['callout']}` | {c.get('ru', '')} | {c.get('en', '')} | {starts or '–'} | {('`' + o['task'] + '`') if o.get('task') else '–'} |")

    w("")
    w("## 7. Tasks")
    for t in crew.get("tasks", []):
        w("")
        w(f"### `{t['id']}`: {t['name']}")
        w("")
        w("Conditions: " + ", ".join(cond_text(c, g.owner) for c in t["conditions"]) + ".")
        w("")
        acts = collections.OrderedDict()
        for c in t["conditions"]:
            for seat, ctrl, pos in g.actions_for(c):
                acts.setdefault(seat, [])
                if (ctrl, pos) not in acts[seat]:
                    acts[seat].append((ctrl, pos))
        w("| Seat | Controls in the chain (alternatives included) |")
        w("|---|---|")
        for seat in seat_ids:
            if seat in acts:
                w(f"| {seat} | {', '.join(f'`{c}`={p}' for c, p in acts[seat])} |")

    w("")
    w("## 8. Callouts")
    w("")
    auto = collections.defaultdict(list)
    for r in crew.get("reports", []):
        auto[r["callout"]].append(cond_text(r["when"]))
    w("| Callout | Kind | From → to | ru | en | Spoken automatically when |")
    w("|---|---|---|---|---|---|")
    for c in crew.get("callouts", []):
        w(f"| `{c['id']}` | {c['kind']} | {c['from']} → {c['to']} | {c['ru']} | {c['en']} | {'; '.join(auto.get(c['id'], [])) or '–'} |")
    return "\n".join(L) + "\n"


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("system", nargs="?", default="osa_ak")
    ap.add_argument("--out")
    ap.add_argument("--check", action="store_true")
    a = ap.parse_args(argv)
    out = Path(a.out) if a.out else ROOT / "docs" / "crew" / "generated" / f"{a.system}_action_map.md"
    text = build(a.system)
    if a.check:
        old = out.read_text(encoding="utf-8") if out.exists() else ""
        if old.replace("\r\n", "\n") != text:
            print(f"{out} is out of date: run python tools/crew_map.py {a.system}")
            return 1
        print(f"{out} is up to date")
        return 0
    out.parent.mkdir(parents=True, exist_ok=True)
    out.write_text(text, encoding="utf-8", newline="\n")
    print(f"wrote {out} ({len(text.splitlines())} lines)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
