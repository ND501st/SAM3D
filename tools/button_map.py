"""Draw a system's button map: every control traced to the subsystems, models, lamps and displays it drives.

Usage:  python tools/button_map.py [system] [--out FILE] [--check]

    system   console and cabin file name, default osa_ak
             (data/procedures/<system>.json and data/cabins/<system>.json)
    --out    output page, default out/button_map/<system>.html (out/ is git-ignored)
    --check  exit with status 1 if the map has problems (references to unknown nodes, cycles)

Standard library only. The page is a left-to-right flow chart: controls by crew seat, then subsystems in the order
they depend on each other, the engagement model's inputs and outputs (from the console's "sensors" block), the sim
signals, lamps, displays and the launch. Click a node to trace it up and down. Below the chart: the crew drill
(procedures), the work not built yet, and an index of every control with its state:

    wired      drives a subsystem or the launch (a guard over a wired control counts as wired)
    model      read by the engagement model (radars, IFF, TV, wheels, orientation)
    indicator  only lights a lamp or turns on a display
    inert      placed in the cabin, nothing reads it

Most of the map comes from the data files. Two things are written here by hand and need updating as the sim grows:
DISPLAY_SOURCES (what each display draws) and PLANNED (work not built yet, with its roadmap phase).
"""
import argparse
import collections
import json
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
TEMPLATE = Path(__file__).resolve().parent / "button_map.html"

STATION_ORDER = ["driver", "operator", "senior_operator", "deputy_commander", "commander"]
NODE_H = {"control": 36, "signal": 42, "subsystem": 46, "model": 46, "event": 40, "lamp": 30, "display": 36, "planned": 46}
GAP = 8
COL_W = 214
COL_GAP = 58

# What each display draws (its on/off conditions come from the cabin file). Node ids: c: control, s: subsystem,
# m: model, i: signal.
DISPLAY_SOURCES = {
    "osa_ak": {
        "soc_ppi": ["m:search", "m:iff"], "ssc_range": ["m:track"], "beta_c": ["c:ssc.az_wheel", "c:ssc.az_light"],
        "soc_counters": ["m:track"], "tov_monitor": ["m:track", "c:tov.fov"],
        "epsilon_c": ["c:ssc.el_wheel", "c:ssc.el_light"], "dts_voltmeter": ["m:track"], "qc_setter": ["c:orient.qc"],
        "elev_search": ["m:elev"],
    },
}

# Work not built yet: (id, label, roadmap phase, notes, what it hooks into, "missing" or "unknown").
# Phases follow docs/DEVELOPMENT_ROADMAP.md; notes follow the controls' notes and docs/ARCHITECTURE.md.
PLANNED = {
    "osa_ak": [
        ("p:containers", "Missile containers 1–3 I, 1–3 II", "6.3", "Six containers with ammunition lamps 1/2/3-I and -II (flash if a gyro overheats).", [], "missing"),
        ("p:select", "Automatic missile selection", "6.3", "Two missiles chosen at ВНИМАНИЕ, outer ones first; ≥ 4 s between launches.", ["p:containers"], "missing"),
        ("p:channels", "Two guidance channels I / II", "6.3", "КАНАЛЫ picks channel I, II or both (СОВМ.). Today one shot is resolved by the outcome model.", ["c:msl.channels", "p:select"], "missing"),
        ("p:cmdtx", "Command transmitter (СПК) and letter change", "6.3", "Transmitter on automatically at ВНИМАНИЕ; СМЕНА ЛИТ. changes its frequency letter.", ["c:spk.letter"], "missing"),
        ("p:chanlamps", "Per-channel lamps СХОД → ЗАХВАТ → СР.Л → УЗКИЙ ЛУЧ → К3", "6.3", "Missile leaves the container, captured in wide beam, medium beam, narrow beam, fuse armed ~120 m out. ЗАХВАТ / ВКЛ ВЫС are mapped to preparation today.", ["p:channels", "p:cmdtx"], "missing"),
        ("p:rangech", "Range indicator: missile sweeps Р I / Р II", "6.3", "Р I — ЦЕЛЬ — Р II should switch the range scope to a missile channel's sweep; it always shows the target today.", ["c:ssc.target_select", "p:channels"], "missing"),
        ("p:emerg", "Emergency launch (unguided throw-off)", "6.3", "ИЗДЕЛИЯ picks the container, АВАРИЙНЫЙ ПУСК throws it off when the launcher has lost power.", ["c:msl.emergency_select", "c:msl.emergency", "p:containers"], "missing"),
        ("p:fuse", "Jamming-target fuse arming (ПОМЕХА)", "6.3 / 6.7", "ПОМЕХА should arm the proximity fuse right after launch; the switch is placed but no model reads it.", ["c:fo.pomekha"], "missing"),
        ("p:rr", "РР mode and СВР beam width", "later", "РОД РАБОТЫ РР and ШИР. — СРЕДН. are not modelled (the game has only БР and ТР).", ["c:svr.beam"], "missing"),
        ("p:fk", "Function check ФК and real cold start (≈ 6.8 min)", "6.2", "Switch-on cycle, computer 9S456 warm-up (100 s), function check; readiness states.", [], "missing"),
        ("p:mc", "…МЦ toggle", "unknown", "Toggle at the launch console's top left. Function not known from any source yet.", ["c:fo.mc"], "unknown"),
        ("p:sscmti", "SSC moving-target filter", "6.4", "СДЦ / ШТ. on the range indicator is placed but the tracking model doesn't read it.", ["c:ssc.mti"], "missing"),
        ("p:beams", "Search beams decide what the SOC sees", "6.4", "Beams I/II/III (4°, 4°, 19° high), 33 rpm, low-target detection ranges. Today the beam only steers the ЦУ elevation search.", ["c:soc.beam", "m:search"], "missing"),
        ("p:qcscale", "OR-31M Qc scale, gyro drift, compass work", "6.5", "Agreement is read from the log today, not from the OR-31M scale.", ["m:orient"], "missing"),
        ("p:manualtv", "Manual TV tracking with the wheels", "6.6", "The TV channel locks and follows by itself today; the real one is tracked by hand (3-mil reticle).", ["s:tv_angle_track"], "missing"),
        ("p:zones", "Zones and guidance methods T/T, Горка, Ф, Н", "6.7", "One zone box today; no pursuit or optical zones, no method choice.", ["e:launch"], "missing"),
        ("p:pu12", "PU-12 battery designations to the commander", "phase 7", "Designations by voice: azimuth and range. Targets are found by the vehicle's own radar only today.", [], "missing"),
        ("p:extpower", "External power supply", "later", "The external supply isn't modelled; power comes from the ГТА or the take-off generator.", [], "missing"),
    ],
}


class MapBuilder:
    def __init__(self, console, cabin):
        self.con, self.cab = console, cabin
        self.ctrl = {c["id"]: c for c in console["controls"]}
        self.subs = {s["id"]: s for s in console.get("subsystems", [])}
        self.inputs = {i["id"]: i for i in console.get("inputs", [])}
        self.panels = {p["id"]: p for p in cabin.get("panels", [])}
        self.st_label = {s["id"]: s["label"] for s in console.get("stations", [])}
        self.nodes = {}
        self.edges = []
        self.problems = []

    # ------------------------------------------------------------ helpers
    def add_node(self, nid, **kw):
        self.nodes[nid] = dict(id=nid, **kw)

    def add_edge(self, src, dst, kind, text="", val=None):
        e = dict(s=src, t=dst, k=kind, x=text)
        if val is not None:
            e["v"] = val
        self.edges.append(e)

    def pos_label(self, cid, n):
        c = self.ctrl[cid]
        labels = c.get("position_labels")
        if labels and 0 <= n < len(labels):
            return labels[n]
        if c["kind"] == "button":
            return "pressed" if n else "released"
        return str(n)

    def cond_text(self, cond):
        if "control" in cond:
            cid = cond["control"]
            if "at_least" in cond:
                return f"{self.ctrl[cid]['label']} ≥ {self.pos_label(cid, cond['at_least'])}"
            return f"{self.ctrl[cid]['label']} = {self.pos_label(cid, cond['is'])}"
        if "subsystem" in cond:
            return f"{self.subs[cond['subsystem']]['name']} is {cond['is']}"
        return f"{self.inputs[cond['input']]['description']}: {'yes' if cond['is'] else 'no'}"

    def cond_val(self, cond):
        if "control" in cond:
            if "at_least" in cond:
                return "≥ " + self.pos_label(cond["control"], cond["at_least"])
            return self.pos_label(cond["control"], cond["is"])
        if "subsystem" in cond:
            return cond["is"]
        return "yes" if cond["is"] else "no"

    @staticmethod
    def cond_src(cond):
        if "control" in cond:
            return "c:" + cond["control"]
        if "subsystem" in cond:
            return "s:" + cond["subsystem"]
        return "i:" + cond["input"]

    def add_cond(self, cond, dst, kind):
        try:
            self.add_edge(self.cond_src(cond), dst, kind, self.cond_text(cond), self.cond_val(cond))
        except KeyError as err:
            self.problems.append(f"{dst}: condition refers to unknown {err}")

    # ------------------------------------------------------------ nodes and edges
    def build(self):
        con, cab = self.con, self.cab
        placed = {c["control"]: c["panel"] for c in cab.get("controls", [])}
        for c in con["controls"]:
            pnl = placed.get(c["id"])
            if pnl is None:  # a guard without its own entry is drawn over the control it guards (cabin.hpp)
                guarded = [x["id"] for x in con["controls"] if x.get("guard") == c["id"]]
                pnl = placed.get(guarded[0]) if guarded else None
            self.add_node("c:" + c["id"], t="control", label=c["label"], cid=c["id"], kind=c["kind"],
                          station=c.get("station"), panel=pnl,
                          panel_label=self.panels[pnl]["label"] if pnl in self.panels else None,
                          cover_auto=c["id"] not in placed, positions=c.get("position_labels"),
                          initial=c.get("initial"), minv=c.get("min"), maxv=c.get("max"), unit=c.get("unit"),
                          guard=c.get("guard"), source=c.get("source"), conf=c.get("confidence"), notes=c.get("notes"))
            if c.get("guard"):
                self.add_edge("c:" + c["guard"], "c:" + c["id"], "guard", f"guard must be open before {c['label']} can move")

        for i in con.get("inputs", []):
            self.add_node("i:" + i["id"], t="signal", label=i["description"], cid=i["id"],
                          source=i.get("source"), conf=i.get("confidence"), notes=i.get("notes"))

        for s in con.get("subsystems", []):
            self.add_node("s:" + s["id"], t="subsystem", label=s["name"], cid=s["id"],
                          latching=s.get("latching", False), start_ready=s.get("start_ready", False),
                          start_s=(s.get("start_time_s") or {}).get("value"), stop_s=(s.get("stop_time_s") or {}).get("value"),
                          source=s.get("source"), conf=s.get("confidence"), notes=s.get("notes"))
            for key, kind in (("enable", "enable"), ("enable_any", "any"), ("prerequisites", "prereq"), ("release", "release")):
                for cond in s.get(key, []):
                    self.add_cond(cond, "s:" + s["id"], kind)

        self.add_models()

        launch = con.get("launch")
        if launch:
            self.add_node("e:launch", t="event", label="ПУСК: missile away", notes=launch.get("notes"),
                          source=launch.get("source"), conf=launch.get("confidence"))
            self.add_edge("c:" + launch["button"], "e:launch", "enable", "launch button pressed", "pressed")
            if launch.get("armed"):
                self.add_edge("s:" + launch["armed"], "e:launch", "enable", "launch circuit armed", "ready")
            if launch.get("restarts"):
                self.add_edge("e:launch", "s:" + launch["restarts"], "restart", "each launch restarts this")
            self.add_node("m:outcome", t="model", label="Hit / miss outcome (gameplay model)",
                          notes=f"data/gameplay/{con['system_id']}.json: hit chance by target class × TV-track factor × "
                                "in-zone factor × manoeuvre and crew-mistake factors; flight time from range.",
                          source=f"data/gameplay/{con['system_id']}.json", conf="game balance")
            self.add_edge("e:launch", "m:outcome", "feed", "resolves the shot")
            if "m:track" in self.nodes:
                self.add_edge("m:track", "m:outcome", "feed", "track quality, TV or radar")

        for lamp in cab.get("lamps", []):
            pnl = self.panels.get(lamp["panel"], {})
            self.add_node("l:" + lamp["id"], t="lamp", label=lamp["label"], color=lamp.get("color"), panel=lamp["panel"],
                          panel_label=pnl.get("label"), station=pnl.get("station"),
                          source=lamp.get("source"), conf=lamp.get("confidence"), notes=lamp.get("notes"))
            for cond in lamp.get("lit_when") or []:
                self.add_cond(cond, "l:" + lamp["id"], "lit")
            for cond in lamp.get("lit_when_any") or []:
                self.add_cond(cond, "l:" + lamp["id"], "litany")
        for d in cab.get("displays", []):
            pnl = self.panels.get(d["panel"], {})
            self.add_node("d:" + d["id"], t="display", label=d["label"], dkind=d["kind"], panel=d["panel"],
                          panel_label=pnl.get("label"), station=pnl.get("station"),
                          source=d.get("source"), conf=d.get("confidence"), notes=d.get("notes"))
            for cond in d.get("on_when") or []:
                self.add_cond(cond, "d:" + d["id"], "lit")
        for did, srcs in DISPLAY_SOURCES.get(con["system_id"], {}).items():
            for s in srcs:
                self.add_edge(s, "d:" + did, "draws", "picture or reading comes from here")

        for pid, label, phase, notes, srcs, status in PLANNED.get(con["system_id"], []):
            self.add_node(pid, t="planned", label=label, phase=phase, notes=notes, status=status)
            for s in srcs:
                self.add_edge(s, pid, "planned", "would drive this once built")

        self.clean_edges()
        self.set_status()
        self.add_procedures()
        self.layout()
        return self

    def add_models(self):
        """The engagement model's view of the console (the console's "sensors" block, simcore/engagement.hpp)."""
        sen = self.con.get("sensors") or {}
        search, track, orient = sen.get("search"), sen.get("track"), sen.get("orientation")

        def feeds(mid, pairs):
            for prefix, key, block, why in pairs:
                if block and block.get(key):
                    self.add_edge(prefix + block[key], mid, "feed", why)

        if search:
            self.add_node("m:search", t="model", label="Search radar picture",
                          notes="Detects targets on the PPI with the selected scale and MTI while the antenna turns and the transmitter radiates.",
                          source=sen.get("source"), conf=sen.get("confidence"))
            feeds("m:search", [("s:", "antenna", search, "antenna"), ("s:", "transmitter", search, "transmitter"),
                               ("c:", "range_control", search, "range scale"), ("c:", "mti_control", search, "MTI")])
            iff = search.get("iff")
            if iff:
                self.add_node("m:iff", t="model", label="IFF interrogator", notes=iff.get("notes"),
                              source=iff.get("source"), conf=iff.get("confidence"))
                feeds("m:iff", [("c:", "query", iff, "query"), ("c:", "control_query", iff, "control query"),
                                ("c:", "gate_control", iff, "gated mode"), ("s:", "ready", iff, "needs radiating")])
                if iff.get("interrogating_input"):
                    self.add_edge("m:iff", "i:" + iff["interrogating_input"], "signal", "sets this signal")
        if track:
            es = track.get("elevation_search")
            if es:
                self.add_node("m:elev", t="model", label="Elevation search in the beam (ЦУ)", notes=es.get("notes"),
                              source=es.get("source"), conf=es.get("confidence"))
                feeds("m:elev", [("s:", "mode", es, "mode"), ("c:", "beam_control", es, "beam")])
            self.add_node("m:track", t="model", label="Tracking radar and TV channel", notes=track.get("notes"),
                          source=sen.get("source"), conf=sen.get("confidence"))
            feeds("m:track", [("s:", k, track, k) for k in ("transmitter", "slew", "radar_mode", "tv_camera", "tv_mode", "range_only_mode")]
                  + [("c:", k, track, k) for k in ("az_wheel", "el_wheel", "range_wheel", "lock_control", "fov_control")])
            if es:
                self.add_edge("m:elev", "m:track", "feed", "elevation search")
            for k in ("captured_input", "too_low_input", "near_zone_input", "in_zone_input"):
                if track.get(k):
                    self.add_edge("m:track", "i:" + track[k], "signal", "sets this signal")
        if orient:
            self.add_node("m:orient", t="model", label="Heading: gyro half-compass", notes=orient.get("notes"),
                          source=orient.get("source"), conf=orient.get("confidence"))
            feeds("m:orient", [("s:", "gyro", orient, "gyro power"), ("c:", "setter", orient, "course setter"),
                               ("c:", "mode_control", orient, "mode switch"), ("c:", "relay_control", orient, "relay switch"),
                               ("c:", "sync_button", orient, "fast sync")])
            if orient.get("synced_input"):
                self.add_edge("m:orient", "i:" + orient["synced_input"], "signal", "sets this signal")

    def clean_edges(self):
        seen, kept = set(), []
        for e in self.edges:
            missing = [n for n in (e["s"], e["t"]) if n not in self.nodes]
            if missing:
                self.problems.append(f"edge {e['s']} -> {e['t']} ({e['k']}): unknown node {', '.join(missing)}")
                continue
            key = (e["s"], e["t"], e["k"])
            if key not in seen:
                seen.add(key)
                kept.append(e)
        self.edges = kept

    def set_status(self):
        kinds = collections.defaultdict(set)
        for e in self.edges:
            kinds[e["s"]].add(e["k"])
        launch_button = "c:" + self.con["launch"]["button"] if self.con.get("launch") else None
        controls = [n for n in self.nodes.values() if n["t"] == "control"]
        for n in controls:
            ks = kinds[n["id"]]
            if n["id"] == launch_button or ks & {"enable", "any", "prereq", "release"}:
                n["status"] = "wired"
            elif ks & {"feed", "draws"}:
                n["status"] = "model"
            elif "guard" in ks:
                n["status"] = "wired"  # settled below
            elif ks & {"lit", "litany"}:
                n["status"] = "indicator"
            else:
                n["status"] = "inert"
        for n in controls:  # a guard is only as live as what it guards
            if n["status"] == "wired" and kinds[n["id"]] <= {"guard", "lit", "litany"}:
                guarded = [e["t"] for e in self.edges if e["s"] == n["id"] and e["k"] == "guard"]
                if guarded and all(self.nodes[g]["status"] == "inert" for g in guarded):
                    n["status"] = "indicator" if kinds[n["id"]] & {"lit", "litany"} else "inert"

    def add_procedures(self):
        self.procs = []
        for p in self.con.get("procedures", []):
            steps = []
            for s in p["steps"]:
                cid = s["control"]
                steps.append(dict(c=cid, label=self.ctrl[cid]["label"], pos=self.pos_label(cid, s.get("position", 1)),
                                  d=s.get("description", ""), w=[self.cond_text(w) for w in s.get("wait_for", [])]))
                used = self.nodes["c:" + cid].setdefault("procs", [])
                if p["id"] not in used:
                    used.append(p["id"])
            station = self.ctrl[p["steps"][0]["control"]].get("station") if p["steps"] else None
            self.procs.append(dict(id=p["id"], name=p["name"], station=station, steps=steps,
                                   source=p.get("source"), notes=p.get("notes")))

    # ------------------------------------------------------------ layout
    def layout(self):
        """Columns by longest path from the controls; rows by seat in column 0, by barycentre elsewhere."""
        preds, succs = collections.defaultdict(list), collections.defaultdict(list)
        for e in self.edges:
            if e["k"] != "restart":
                preds[e["t"]].append(e["s"])
                succs[e["s"]].append(e["t"])
        depth, visiting = {}, set()

        def dep(nid):
            if nid in depth:
                return depth[nid]
            n = self.nodes[nid]
            if n["t"] == "control" or not preds[nid]:
                depth[nid] = 0 if n["t"] in ("control", "signal") else 1
                return depth[nid]
            if nid in visiting:
                self.problems.append(f"cycle through {nid}")
                return 0
            visiting.add(nid)
            depth[nid] = 1 + max(dep(p) for p in preds[nid])
            visiting.discard(nid)
            return depth[nid]

        for nid in self.nodes:
            dep(nid)
        cols = collections.defaultdict(list)
        for nid, d in depth.items():
            cols[d].append(nid)
        ncols = max(depth.values()) + 1

        ypos, groups, y = {}, [], 0.0
        stations = [s for s in STATION_ORDER if s in self.st_label] + [s for s in self.st_label if s not in STATION_ORDER]
        no_seat = [c["id"] for c in self.con["controls"] if c.get("station") not in self.st_label]
        bands = [(st, self.st_label[st], ["c:" + c["id"] for c in self.con["controls"] if c.get("station") == st]) for st in stations]
        if no_seat:
            bands.append((None, "No seat", ["c:" + c for c in no_seat]))
        bands.append(("world", "World", [i for i in cols[0] if self.nodes[i]["t"] == "signal"]))
        for st, label, ids in bands:
            if not ids:
                continue
            groups.append(dict(station=st, label=label, y0=y))
            y += 26
            for i in ids:
                h = NODE_H[self.nodes[i]["t"]]
                ypos[i] = y + h / 2
                y += h + GAP
            groups[-1]["y1"] = y - GAP
            y += 18
        col0_h = y

        def place(ids, want):
            ids = sorted(ids, key=lambda i: want[i])
            hs = [NODE_H[self.nodes[i]["t"]] for i in ids]
            tops = [want[i] - h / 2 for i, h in zip(ids, hs)]
            fwd = list(tops)
            for k in range(1, len(ids)):
                fwd[k] = max(fwd[k], fwd[k - 1] + hs[k - 1] + GAP)
            back = list(tops)
            for k in range(len(ids) - 2, -1, -1):
                back[k] = min(back[k], back[k + 1] - hs[k] - GAP)
            res = [(a + b) / 2 for a, b in zip(fwd, back)]
            for k in range(1, len(ids)):
                res[k] = max(res[k], res[k - 1] + hs[k - 1] + GAP)
            shift = -res[0] if res and res[0] < 0 else 0
            for i, r, h in zip(ids, res, hs):
                ypos[i] = r + shift + h / 2

        for _ in range(4):
            for d in range(1, ncols):
                want = {}
                for i in cols[d]:
                    ps = [ypos[p] for p in preds[i] if p in ypos and depth[p] < d]
                    want[i] = sum(ps) / len(ps) if ps else ypos.get(i, col0_h / 2)
                place(cols[d], want)
            for d in range(ncols - 2, 0, -1):
                want = {}
                for i in cols[d]:
                    ps = [ypos[p] for p in preds[i] if p in ypos]
                    ss = [ypos[s] for s in succs[i] if s in ypos]
                    if ps and ss:
                        want[i] = 0.65 * sum(ps) / len(ps) + 0.35 * sum(ss) / len(ss)
                    elif ps:
                        want[i] = sum(ps) / len(ps)
                    else:
                        want[i] = ypos[i]
                place(cols[d], want)

        for nid, n in self.nodes.items():
            n["col"] = depth[nid]
            n["x"] = depth[nid] * (COL_W + COL_GAP)
            n["h"] = NODE_H[n["t"]]
            n["y"] = round(ypos[nid] - n["h"] / 2, 1)
            n["w"] = COL_W
        self.groups, self.ncols = groups, ncols

    # ------------------------------------------------------------ output
    def data(self):
        nodes = list(self.nodes.values())
        ctrls = [n for n in nodes if n["t"] == "control"]
        counts = dict(
            controls=len(ctrls),
            **{s: sum(1 for n in ctrls if n["status"] == s) for s in ("wired", "model", "indicator", "inert")},
            subsystems=len(self.subs), lamps=len(self.cab.get("lamps", [])), displays=len(self.cab.get("displays", [])),
            procedures=len(self.procs), planned=sum(1 for n in nodes if n["t"] == "planned"),
            signals=len(self.inputs), models=sum(1 for n in nodes if n["t"] == "model"),
        )
        return dict(
            system=self.con["system_id"], display_name=self.con.get("display_name", self.con["system_id"]),
            nodes=nodes, edges=self.edges, groups=self.groups,
            W=self.ncols * (COL_W + COL_GAP) - COL_GAP, H=max(n["y"] + n["h"] for n in nodes),
            col_w=COL_W, col_gap=COL_GAP, ncols=self.ncols,
            stations=[dict(id=s, label=self.st_label[s]) for s in STATION_ORDER if s in self.st_label]
                     + [dict(id=s, label=l) for s, l in self.st_label.items() if s not in STATION_ORDER],
            procs=self.procs, counts=counts, status=self.con.get("status", ""),
        )


def build(console, cabin):
    return MapBuilder(console, cabin).build()


def render(data):
    payload = json.dumps(data, ensure_ascii=False).replace("</", "<\\/")
    return TEMPLATE.read_text(encoding="utf-8").replace("__DATA__", payload)


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("system", nargs="?", default="osa_ak")
    ap.add_argument("--out")
    ap.add_argument("--check", action="store_true")
    args = ap.parse_args(argv)

    console = json.loads((ROOT / "data" / "procedures" / f"{args.system}.json").read_text(encoding="utf-8"))
    cabin = json.loads((ROOT / "data" / "cabins" / f"{args.system}.json").read_text(encoding="utf-8"))
    mb = build(console, cabin)
    data = mb.data()
    out = Path(args.out) if args.out else ROOT / "out" / "button_map" / f"{args.system}.html"
    out.parent.mkdir(parents=True, exist_ok=True)
    out.write_text(render(data), encoding="utf-8")

    c = data["counts"]
    print(f"{out}: {c['controls']} controls ({c['wired']} wired, {c['model']} model, {c['indicator']} indicator, "
          f"{c['inert']} inert), {c['subsystems']} subsystems, {c['lamps']} lamps, {c['displays']} displays, "
          f"{c['procedures']} procedures, {c['planned']} not built")
    for p in mb.problems:
        print("problem:", p, file=sys.stderr)
    return 1 if args.check and mb.problems else 0


if __name__ == "__main__":
    sys.exit(main())
