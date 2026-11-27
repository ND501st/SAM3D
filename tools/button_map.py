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
    model      read by a simulation model (radars, IFF, TV, wheels, orientation, radios and voices, the post)
    display    sets a screen's picture or what a meter reads (brightness, focus, gain, a meter's selector)
    indicator  only lights a lamp or turns on a display
    code       nothing in the console reads it, but simcore's source or a system file names it
    drill      worked in a drill, a lesson or a readiness state, but nothing reads it: it changes nothing
    inert      placed in the cabin, nothing names it

The same states, control by control with what each reaches, are written as a list to
docs/generated/<system>_controls.md (the map's list; regenerate it with this tool).

Most of the map comes from the data files. Two things are written here by hand and need updating as the sim grows:
DISPLAY_SOURCES (what each display draws) and PLANNED (work not built yet, with its roadmap phase).
"""
import argparse
import collections
import json
import re
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
        "soc_ppi": ["m:search", "m:iff"], "ssc_range": ["m:track", "m:launcher"], "beta_c": ["c:ssc.az_wheel", "c:ssc.az_light"],
        "ssc_az_coarse": ["m:track"], "ssc_az_fine": ["m:track"], "ssc_el_coarse": ["m:track"], "ssc_el_fine": ["m:track"], "tov_monitor": ["m:track", "c:tov.fov"],
        "epsilon_c": ["c:ssc.el_wheel", "c:ssc.el_light"], "dts_voltmeter": ["m:track"], "qc_setter": ["c:orient.qc"],
        "elev_search": ["m:elev"], "or31_qc": ["m:orient"], "lat_scale": ["c:orient.latitude"], "fuel_gauge": ["m:fuel"],
    },
}

# Work not built yet: (id, label, roadmap phase, notes, what it hooks into, "missing" or "unknown").
# Phases follow docs/DEVELOPMENT_ROADMAP.md; notes follow the controls' notes and docs/ARCHITECTURE.md.
PLANNED = {
    "osa_ak": [
        # The audit of 12 Nov 2026 (docs/OSA_INTERIOR_HOLISTIC.md section 47): drawn but not wired, seen in a
        # photo, or in the sources only.
        ('p:0062', '0062-6M1 switch КАНАЛЫ', 'on the model, not wired', 'The noise check is built (КОНТРОЛЬ, ПОДЖИГ ГШ, РЕЖИМЫ, the РРУ КШ knobs: simcore/checks.hpp); КАНАЛЫ, likeliest the large pointer knob, is not a control: РЕЖИМЫ picks the channel (voenizdat_osa_1991 p98, p161).', ['c:ssc.modes'], 'missing'),
        ('p:pu303', 'PU-303: ХОЛ. ПРОКР., the hour counters', 'on the model, not wired', 'The lamps, the ГТА and ТВГ gauges, the load buttons (ОТКЛ. НАГР. takes the loads off until ГТА or ИВП puts them back: subsystem loads_off) and СТОП (gta_stopped, until ПУСК is put off) work; the cold cranking and the hour counters wait for a model of the turbine start.', ['c:gta.cold_crank'], 'missing'),
        ('p:bort_off', 'БОРТ СПК ОТКЛ. as its own push-button', 'on the model, not wired', 'Drawn beside the guarded БОРТ СПК ВКЛ.; one switch fo.bort_spk stands for both (voenizdat_osa_1991 p119-121).', ['c:fo.bort_spk'], 'missing'),
        ('p:faults', 'Fault lamp НЕИСПРАВНОСТЬ', 'in the sources only: silent', "ОТКАЗ СРП, ОТКАЗ Qc and ОБРЫВ ФАЗЫ light with their faults (roadmap C4); НЕИСПРАВНОСТЬ is silent in the sources (voenizdat_osa_1991 p121); the PU-303's fault lamps light on its lamp test.", ['s:power'], 'missing'),
        # The ПАНЕЛЬ ИНДИКАЦИИ's lamps (12 Nov 2026, docs/OSA_INTERIOR_HOLISTIC.md section 50): ДВЕРЬ ЗАКРЫТА, ЛЮКИ РТА ЗАКРЫТЫ and
        # СЭП АВАР. ОТКЛ. are built; these wait for their systems or their legends.
        ('p:ind_caps', 'ПАНЕЛЬ ИНДИКАЦИИ: five legend lamps not read, two blank spares', 'on the model, not wired', 'Drawn dark (wwp_gecko g095-b): four three-line and one two-line legends not legible; the left column\'s bottom two blank, under a cool and a reddish lens (spares, SAM3D\'s reading).', ['s:power'], 'unknown'),
        ('p:deenergised', 'De-energisation indicator box', 'seen in a photo: owner to decide', 'Stencilled box aft of the spares box (wwp_gecko g090-a, g092-a; rwd_mb3 layout).', ['s:power'], 'missing'),
        ('p:flags', 'March signals: flags and lamps', 'seen in a photo: owner to decide', 'Two signal flags and a bag by the commander (wwp_gecko g089-d); the march signals (voenizdat_osa_1991 p232-239).', ['c:veh.hatches'], 'missing'),
        ('p:gauges', "Air pressure gauges over the commander's screen and by the right radio", 'seen in a photo: owner to decide', 'With pipe and valve (wwp_gecko g087-d, g089-b, g090-a); what they serve is not sourced.', ['c:press.standard'], 'unknown'),
        ('p:0091_17', '0091-17M: КОНТРОЛЬ НАПР. and its meter', 'on the model, not wired', "The НАКАЛ lamps now light; the SOC's voltage check (voenizdat_osa_1991 p49) and ПЕРЕКЛ. ИСТОЧНИКОВ are drawn in the mesh, not wired.", ['s:soc_transmitter'], 'missing'),
        ('p:gyro_sw', '0005-8M КОРРЕКЦИЯ ОСН. — КОНТР.', 'in the sources only: owner to decide', "The latitude correction to the ГА-3 (voenizdat_osa_1991 p51); МК — ГПК — АК (gyro.compass_mode) and ПОТРЕБИТЕЛИ ОСН. — КОНТР. (gyro.consumers, on the block's last free toggle) are built; no toggle is left on g091-a for this one. Also 0009-5M's ОШИБКА ψ sector narrowing when the antenna rises (p52): its lamp only glows.", ['s:gyro_erected'], 'missing'),
        ('p:0051', '0051-7M repetition frequency АВТ., РУЧН. F1-F3', 'in the sources only: owner to decide', "On the operator's bottom block (voenizdat_osa_1991 p96-101); its nameplate is drawn.", ['s:ssc_transmitter'], 'missing'),
        ('p:0065', '0065-2M АРУ — РРУ and gains', 'in the sources only: owner to decide', 'The MTI canceller on SP-1M, under flaps modelled closed (voenizdat_osa_1991 p49-53).', ['s:soc_transmitter'], 'missing'),
        ('p:vision', "Driver's vision: PNV-57E, TNPO-170A", 'in the sources only: owner to decide', 'rwd_mb3; g089-a/b show only periscope blanks. The wave shield is built: veh.bilge_pump, ВОДООТРАЖАТЕЛЬНЫЙ ЩИТ (voenizdat_osa_1991 p224).', ['c:veh.waterjet_left'], 'missing'),
        # The panel review of 12 Nov 2026 (docs/research/OSA_PANEL_REVIEW_2026-11.md section 4): their systems are not built.
        ('p:fuse_lamps', "Fuse lamps in 0096-9M1, on SP-1M, 0094-11M; OR-95-1M ПЕРЕГРУЗКА", 'in the sources only: owner to decide', "0096-10M's four (СОЦ, ССЦ, СВР, СПК) light with their faults (fuse_*, docs/FAULTS.md); 0096-9M1's twins and the racks' own lamps are not drawn: their legends and places are not in the sources (voenizdat_osa_1991 p166).", ['s:power'], 'missing'),
        ('p:fuel_lever', 'Hand fuel-pump lever on its ratchet quadrant', 'seen in a photo: owner to decide', 'Ball-knobbed, low left of the column (wwp_gecko g086-d, g083-a).', ['c:pwr.pto'], 'missing'),
        ('p:diff_fire', 'Differential lock; the fire system on/off', 'in the sources only: owner to decide', 'voenizdat_osa_1991 p175, p221. The fire bottles, the automat and the driver\'s disc are built (roadmap C8).', ['i:vehicle_stopped'], 'missing'),
        ('p:iff_1l24', 'IFF 1L24-2 blocks', 'in the sources only: owner to decide', 'On the BM3, not the AK, which carries the РЗМ set (voenizdat_osa_1991 p53-54).', ['s:soc_antenna'], 'missing'),
        ('p:or14', 'OR-14M: timers, ДЕЛИТЕЛИ НАПРЯЖЕНИЯ knobs and toggles', 'in the sources only: owner to decide', 'Zeroed for ПУСК ГОТОВ (voenizdat_osa_1991 p154).', ['s:fire_control_computer'], 'missing'),
        ('p:pb107_fan', 'ПБ-107: its own fan on the engine bulkhead', 'in the sources only: owner to decide', 'voenizdat_osa_1991 p168.', ['s:power'], 'missing'),
    ],
}


STATES = ["wired", "model", "display", "indicator", "code", "drill", "inert"]
STATE_TEXT = {
    "wired": "drives a subsystem or the launch",
    "model": "read by a simulation model",
    "display": "sets a screen's picture or what a meter reads",
    "indicator": "only lights a lamp or turns on a display",
    "code": "named by simcore's source or a system file, not by the console",
    "drill": "worked in a drill, lesson or readiness state; changes nothing",
    "inert": "nothing names it",
}
# Console blocks read by simcore whose controls the map links by name (any control id inside them feeds the model).
BLOCK_MODELS = {
    "voice": ("m:voice", "Voices: the intercom boxes, headsets, sets and telephone",
              "simcore/voice_comms.hpp, radio_nets.hpp: a seat hears the intercom, a radio or a line only through its headset, "
              "its box's selector and a set that is on, warm and on the net's frequency."),
    "post": ("m:post", "The command post: plotting board, telecode, nets", "simcore/pu12.hpp: the post's picture, telecode and nets."),
    "radios": ("m:radio_sets", "The radio sets worked as sets", "simcore/radio_sets.hpp: the R-111 and R-407 as sets."),
    "checks": ("m:checks", "The BM's checks: the function-check meters, the noise check",
               "simcore/checks.hpp: ИП1, ИП2 and 0004-14M's meters in ФК, КОНТРОЛЬ of 0062-6M1 with ПОДЖИГ ГШ (roadmap C4)."),
}
# The vehicle's own systems (data/vehicles/<system>_systems.json, simcore/vehicle_systems.hpp, roadmap C1-C9): each block
# a model its controls feed and whose *_input signals it sets.
VEHICLE_MODELS = {
    "engine": ("m:veh_engine", "Engine: starter, stop, hand throttle, warm-up and oil"),
    "drive": ("m:veh_drive", "Drive: gears, handbrake, the water-jets and wave shield"),
    "electrics": ("m:veh_electrics", "Electrics: batteries, the network, the 220 V supply and УКЧ"),
    "lights": ("m:veh_lights", "Lights: headlamps, sidelights, blackout, the cabin"),
    "climate": ("m:veh_climate", "Climate: the cabin, racks and transmitters' heat"),
    "fire": ("m:veh_fire", "Fire: the compartments, the bottles, the automat"),
}
# Where else a control can be named: simcore reads these (code); the crew, the lessons and the scenarios only drill them.
CODE_GLOBS = ["simcore/include/simcore/**/*.hpp", "unreal/SAM3D/Source/**/*.cpp", "unreal/SAM3D/Source/**/*.h", "data/systems/*.json"]
DRILL_GLOBS = ["data/crew/*.json", "data/tutorials/*.json", "data/scenarios/**/*.json", "data/command/*.json"]


def find_refs(ids):
    """Each control id named outside its console and cabin: {id: {"code": [files], "drill": [files]}}."""
    refs = collections.defaultdict(lambda: collections.defaultdict(list))
    want = set(ids)
    for kind, globs in (("code", CODE_GLOBS), ("drill", DRILL_GLOBS)):
        for g in globs:
            for f in sorted(ROOT.glob(g)):
                text = f.read_text(encoding="utf-8", errors="ignore")
                for cid in set(re.findall(r'"([a-z0-9_]+(?:\.[a-z0-9_]+)+)"', text)) & want:
                    refs[cid][kind].append(f.relative_to(ROOT).as_posix())
    return refs


def control_ids_in(value, ids):
    """Every control id in a nested block of data."""
    if isinstance(value, str):
        return [value] if value in ids else []
    if isinstance(value, dict):
        return [c for v in value.values() for c in control_ids_in(v, ids)]
    if isinstance(value, list):
        return [c for v in value for c in control_ids_in(v, ids)]
    return []


class MapBuilder:
    def __init__(self, console, cabin, refs=None, vehicle=None):
        self.con, self.cab = console, cabin
        self.veh = vehicle or {}
        self.refs = refs or {}
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
            for key, kind in (("enable", "enable"), ("enable_any", "any"), ("prerequisites", "prereq"), ("release", "release"),
                              ("release_any", "release")):
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
            for cond in (d.get("on_when") or []) + (d.get("lit_when") or []):   # powered; its own lamps
                self.add_cond(cond, "d:" + d["id"], "lit")
            for key, val in (d.get("picture") or {}).items():   # the picture's potentiometers (CabinDisplay::picture)
                if isinstance(val, str) and val in self.ctrl:
                    self.add_edge("c:" + val, "d:" + d["id"], "picture", f"sets the picture's {key.replace('_', ' ')}")
            for cond in (d.get("picture") or {}).get("gain_when") or []:   # the gain by hand only at РРУ
                self.add_cond(cond, "d:" + d["id"], "picture")
            cs = d.get("course_scale") or {}
            if cs.get("control") in self.ctrl:
                self.add_edge("c:" + cs["control"], "d:" + d["id"], "picture", "lights the engraved course scale")
            for lv in d.get("levels") or []:                    # a meter's reading chosen by a selector
                for cond in lv.get("when", []):
                    if "control" in cond:
                        self.add_cond(cond, "d:" + d["id"], "reading")
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
        self.settle_unread()
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

        def receiver_feeds(block, mid):
            """The receiver's knobs (roadmap C5): MTI source, wind compensation, AFC and its trim, СА."""
            rec = block.get("receiver") or {}
            feeds(mid, [("c:", k, rec, why) for k, why in (
                ("mti_external_control", "MTI internal/external"), ("wind_comp_control", "wind compensation on"),
                ("wind_coarse_control", "wind compensation, coarse"), ("wind_fine_control", "wind compensation, fine"),
                ("afc_control", "AFC automatic/manual"), ("afc_trim_control", "manual tuning"), ("sa_control", "СА blanking"))])

        if search:
            self.add_node("m:search", t="model", label="Search radar picture",
                          notes="Detects targets on the PPI with the selected scale and MTI while the antenna turns and the transmitter radiates.",
                          source=sen.get("source"), conf=sen.get("confidence"))
            feeds("m:search", [("s:", "antenna", search, "antenna"), ("s:", "transmitter", search, "transmitter"),
                               ("c:", "range_control", search, "range scale"), ("c:", "mti_control", search, "MTI")])
            receiver_feeds(search, "m:search")
            beams = search.get("beams")
            if beams:  # the beam selector's programme, and the beam lamps it lights (roadmap 6.4)
                feeds("m:search", [("c:", "control", beams, "beam programme")])
                for inp in beams.get("inputs", []):
                    self.add_edge("m:search", "i:" + inp, "signal", "sets this signal")
            iff = search.get("iff")
            if iff:
                self.add_node("m:iff", t="model", label="IFF interrogator", notes=iff.get("notes"),
                              source=iff.get("source"), conf=iff.get("confidence"))
                feeds("m:iff", [("c:", "query", iff, "query"), ("c:", "control_query", iff, "control query"),
                                ("c:", "gate_control", iff, "gated mode"), ("s:", "ready", iff, "needs radiating"),
                                ("c:", "check_control", iff, "indicator check marks")])
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
                  + [("c:", k, track, k) for k in ("az_wheel", "el_wheel", "range_wheel", "lock_control", "slew_control", "fov_control", "mti_control",
                                                         "rgpo_control", "filter_control", "range_balance_control")]
                  + [("c:", "small_target_control", track, "МЦ: a small target")]
                  + [("c:", k, track, "ПА balance") for k in ("pa_balance_az", "pa_balance_el")])
            receiver_feeds(track, "m:track")
            if es:
                self.add_edge("m:elev", "m:track", "feed", "elevation search")
            for k in ("captured_input", "too_low_input", "near_zone_input", "in_zone_input", "crosshair_input", "range_pulled_input"):
                if track.get(k):
                    self.add_edge("m:track", "i:" + track[k], "signal", "sets this signal")
        if orient:
            self.add_node("m:orient", t="model", label="Heading: gyro half-compass", notes=orient.get("notes"),
                          source=orient.get("source"), conf=orient.get("confidence"))
            feeds("m:orient", [("s:", "gyro", orient, "gyro power"), ("c:", "setter", orient, "course setter"),
                               ("c:", "mode_control", orient, "mode switch"), ("c:", "relay_control", orient, "relay switch"),
                               ("c:", "sync_button", orient, "fast sync"),
                               ("c:", "latitude_control", orient, "latitude correction (drift)"),
                               ("c:", "compass_control", orient, "compass mode: МК — ГПК — АК"),
                               ("c:", "consumers_control", orient, "ПОТРЕБИТЕЛИ: the setter connected to the gyro"),
                               ("c:", "follow_up_control", orient, "follow-up: the frame slaved to the gyro"),
                               ("c:", "latitude_switch", orient, "latitude switch φ")])
            if orient.get("qc_fault_input"):
                self.add_edge("i:" + orient["qc_fault_input"], "m:orient", "feed", "ОТКАЗ Qc stops the follow-up")
            for key in ("synced_input", "qc_known_input", "oriented_input"):
                if orient.get(key):
                    self.add_edge("m:orient", "i:" + orient[key], "signal", "sets this signal")

        fuel = self.con.get("fuel")
        if fuel:  # the tank the power sources burn from (roadmap 7.4)
            self.add_node("m:fuel", t="model", label="Fuel: the tank, what burns it, the cut at half", notes=fuel.get("notes"),
                          source=fuel.get("source"), conf=fuel.get("confidence"))
            for src in fuel.get("sources", []):
                if "s:" + src["subsystem"] in self.nodes:
                    self.add_edge("s:" + src["subsystem"], "m:fuel", "feed", "burns fuel")
            # Its power cut at the reserve (power_cut_input) stops the same sources: not drawn, it would be a cycle.

        radios = self.con.get("radios")
        if radios:  # the two R-123M sets: the commander's source selection (roadmap 7.8)
            self.add_node("m:radios", t="model", label="R-123M sets and the command network", notes=radios.get("notes"),
                          source=radios.get("source"), conf=radios.get("confidence"))
            feeds("m:radios", [("c:", "listen", radios, "set listened on")])
            for cid in radios.get("sets", []):
                self.add_edge("c:" + cid, "m:radios", "feed", "preset")

        comms = self.con.get("comms")
        if comms:  # the R-123M sets and the R-124 intercom (roadmap 7.8, simcore/crew_comms.hpp)
            self.add_node("m:c2net", t="model", label="Command network: sources, presets, headset",
                          notes="simcore/c2_sources.hpp: which source is on which of the four presets, the set the headset is on, "
                                "what the crew has been told. The two sets' preset selectors and the headset switch move it, and it moves them.",
                          source="mon_2112_80 p26; r123_refs; docs/research/EM_C2_SOURCES_2026-10.md", conf="medium")
            for cid in ("radio.left.preset", "radio.right.preset", "radio.headset"):
                if cid in self.ctrl:
                    self.add_edge("c:" + cid, "m:c2net", "feed", "tunes / listens")
            self.add_node("m:comms", t="model", label="R-123M sets and R-124 intercom",
                          notes=comms.get("notes"), source=comms.get("source"), conf=comms.get("confidence"))
            for r in comms.get("radios", []):
                for key, why in (("power", "switched on"), ("mode", "СИМПЛЕКС / Д. ПРИЕМ"), ("squelch", "noise suppressor: shortens the range"),
                                 ("volume", "turned up"), ("antenna", "whip, 1 m or emergency wire: shortens the range")):
                    self.add_edge("c:" + r[key], "m:comms", "feed", why)
                for key in ("warm", "transmit"):
                    self.add_edge("s:" + r[key], "m:comms", "feed", "state of the set")
                if r.get("tone"):
                    self.add_edge("c:" + r["tone"], "m:comms", "feed", "ТОН. ВЫЗОВ: a call tone while transmitting")
                # the preset set-up (radio_setup.hpp): what the fixators hold, and the knobs
                for key, why in (("fixator_cover", "the fixators' cover"), ("frequency", "УСТАНОВКА ЧАСТОТЫ: the frequency"),
                                 ("subband", "the sub-band"), ("antenna_lock", "the antenna knob's lock"),
                                 ("antenna_tune", "НАСТРОЙКА АНТЕННЫ: the antenna's match")):
                    if r.get(key):
                        self.add_edge("c:" + r[key], "m:comms", "feed", why)
                for fx in r.get("fixators", []):
                    self.add_edge("c:" + fx, "m:comms", "feed", "a fixator: locked, it keeps the frequency set")
                for key in ("on_frequency_input", "tuned_input"):
                    if r.get(key):
                        self.add_edge("m:comms", "i:" + r[key], "signal", "sets this signal")
            ic = comms.get("intercom", {})
            for key, why in (("commander", "A-1: on the radio, on the intercom or off"), ("deputy", "A-2: on the radio"), ("driver", "ПВ: on the radio"),
                             ("chest", "chest switch: ПРД transmits on the commander's set"),
                             ("driver_chest", "the driver's chest switch: ПРД keys the left set (ПВ on РС)")):
                if ic.get(key):
                    self.add_edge("c:" + ic[key], "m:comms", "feed", why)
            if ic.get("subsystem"):
                self.add_edge("s:" + ic["subsystem"], "m:comms", "feed", "intercom up")
            self.add_edge("m:comms", "m:c2net", "feed", "which set hears, the loss on the link, who is on the radio")

        for key, (mid, label, notes) in BLOCK_MODELS.items():
            block = self.con.get(key)
            if not isinstance(block, dict):
                continue
            ids = control_ids_in(block, self.ctrl)
            if not ids:
                continue
            if mid not in self.nodes:
                self.add_node(mid, t="model", label=label, notes=block.get("notes") or notes,
                              source=block.get("source"), conf=block.get("confidence"))
            for cid in ids:
                self.add_edge("c:" + cid, mid, "feed", label.split(":")[0].lower())

        launcher = self.con.get("launcher")
        if launcher:  # the missile load and the guidance channels (roadmap 6.3, simcore/launcher.hpp)
            self.add_node("m:launcher", t="model", label="Missiles and guidance channels I / II", notes=launcher.get("notes"),
                          source=launcher.get("source"), conf=launcher.get("confidence"))
            feeds("m:launcher", [("c:", "channel_control", launcher, "channels"), ("c:", "jam_control", launcher, "jamming target"),
                                 ("i:", "attention_input", launcher, "selects missiles"),
                                 ("s:", "pair_preparation", launcher, "prepared")])
            letter, emergency = launcher.get("letter") or {}, launcher.get("emergency") or {}
            sweep = launcher.get("range_sweep") or {}
            feeds("m:launcher", [("c:", "control", letter, "letter change"), ("c:", "select_control", emergency, "emergency: container"),
                                 ("c:", "fire_control", emergency, "emergency launch"),
                                 ("c:", "control", sweep, "range sweep: channel")])
            outs = [launcher.get("hold_input"), emergency.get("ready_input")] + list(letter.get("lock_inputs", []))
            outs += [c.get("input") for c in launcher.get("containers", [])]
            outs += [c.get("hot_input") for c in launcher.get("containers", [])]
            for ch in launcher.get("channels", []):
                outs += [v for k, v in ch.items() if k != "return"]
                if ch.get("return"):  # ВОЗВРАТ: the button resets the channel's range finder, losing its missile
                    self.add_edge("c:" + ch["return"], "m:launcher", "feed", "ВОЗВРАТ: the missile in flight is lost")
            refuse = launcher.get("refuse") or {}
            feeds("m:launcher", [("c:", "select_control", refuse, "ОТКАЗ: the missile chosen"),
                                 ("c:", "button", refuse, "ОТКАЗ: the missile on preparation refused")])
            outs.append(refuse.get("refused_input"))
            for inp in outs:
                if inp:
                    self.add_edge("m:launcher", "i:" + inp, "signal", "sets this signal")

        for key, (mid, label) in VEHICLE_MODELS.items():
            block = self.veh.get(key)
            if not isinstance(block, dict):
                continue
            self.add_node(mid, t="model", label=label, notes=block.get("notes") or self.veh.get("notes"),
                          source=block.get("rule_source") or self.veh.get("source"), conf=self.veh.get("confidence"))
            for cid in dict.fromkeys(control_ids_in(block, self.ctrl)):
                self.add_edge("c:" + cid, mid, "feed", label.split(":")[0].lower())
            for k, v in block.items():
                if k.endswith("_input") and isinstance(v, str) and v in self.inputs:
                    self.add_edge(mid, "i:" + v, "signal", "sets this signal")
            # The subsystems it needs are not drawn: its signals enable them back (the network, the engine), a cycle.

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
            elif ks & {"picture", "reading"}:
                n["status"] = "display"
            elif ks & {"lit", "litany"}:
                n["status"] = "indicator"
            else:
                n["status"] = "inert"
        for n in controls:  # a guard is only as live as what it guards
            if n["status"] == "wired" and kinds[n["id"]] <= {"guard", "lit", "litany"}:
                guarded = [e["t"] for e in self.edges if e["s"] == n["id"] and e["k"] == "guard"]
                if guarded and all(self.nodes[g]["status"] == "inert" for g in guarded):
                    n["status"] = "indicator" if kinds[n["id"]] & {"lit", "litany"} else "inert"

    def settle_unread(self):
        """An inert control named elsewhere: by simcore (code) or only by drills, lessons and readiness states (drill)."""
        drilled = set(control_ids_in(self.con.get("procedures", []), self.ctrl)) | set(control_ids_in(self.con.get("readiness", {}), self.ctrl))
        for n in self.nodes.values():
            if n["t"] != "control":
                continue
            r = self.refs.get(n["cid"], {})
            n["refs"] = {k: list(v) for k, v in r.items()}
            if n["status"] != "inert":
                continue
            if r.get("code"):
                n["status"] = "code"
            elif r.get("drill") or n["cid"] in drilled:
                n["status"] = "drill"

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
            **{s: sum(1 for n in ctrls if n["status"] == s) for s in STATES},
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


def build(console, cabin, refs=None, vehicle=None):
    return MapBuilder(console, cabin, refs, vehicle).build()


def write_list(mb, path):
    """The map's list: every control by state with what it reaches, then the work not built."""
    data = mb.data()
    nodes = {n["id"]: n for n in data["nodes"]}
    outs = collections.defaultdict(list)
    for e in data["edges"]:
        if e["k"] not in ("planned", "guard"):
            outs[e["s"]].append(nodes[e["t"]]["label"])
    st_label = {s["id"]: s["label"] for s in data["stations"]}
    c = data["counts"]
    lines = [f"# {data['display_name']}: every control and what it does", "",
             f"Written by `tools/button_map.py {data['system']}` from the console (`data/procedures/{data['system']}.json`), the cabin "
             f"(`data/cabins/{data['system']}.json`) and where else simcore names each control; the same states colour "
             f"`out/button_map/{data['system']}.html`. Do not edit by hand.", "",
             "| State | Meaning | Controls |", "|---|---|---|"]
    for s in STATES:
        lines.append(f"| {s} | {STATE_TEXT[s]} | {c[s]} |")
    lines.append(f"| **all** | | **{c['controls']}** |")
    ctrls = [n for n in data["nodes"] if n["t"] == "control"]
    for s in STATES:
        group = sorted((n for n in ctrls if n["status"] == s), key=lambda n: (n.get("panel") or "", n["cid"]))
        if not group:
            continue
        lines += ["", f"## {s} ({len(group)}): {STATE_TEXT[s]}", "", "| Panel | Control | Id | Seat | Reaches |", "|---|---|---|---|---|"]
        for n in group:
            reach = sorted(set(outs.get(n["id"], [])))
            if s == "code":
                reach = n["refs"].get("code", [])
            elif s == "drill":
                reach = sorted(set(n["refs"].get("drill", []))) or ["the console's procedures or readiness"]
            text = "; ".join(reach) if reach else "nothing"
            label = n["label"].replace("|", "/")
            lines.append(f"| {n.get('panel') or '-'} | {label} | `{n['cid']}` | {st_label.get(n.get('station'), n.get('station') or '-')} | {text.replace('|', '/')} |")
    planned = [n for n in data["nodes"] if n["t"] == "planned"]
    if planned:
        lines += ["", f"## Not built ({len(planned)})", "", "| Item | Where it stands | Notes |", "|---|---|---|"]
        for n in planned:
            lines.append(f"| {n['label'].replace('|', '/')} | {n['phase']} | {(n.get('notes') or '').replace('|', '/')} |")
    dark = [l for l in mb.cab.get("lamps", []) if not l.get("lit_when") and not l.get("lit_when_any")]
    if dark:
        lines += ["", f"## Lamps with no rule to light ({len(dark)})", "", "| Panel | Lamp | Id |", "|---|---|---|"]
        for l in dark:
            lines.append(f"| {l['panel']} | {l['label']} | `{l['id']}` |")
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text("\n".join(lines) + "\n", encoding="utf-8")


def render(data):
    payload = json.dumps(data, ensure_ascii=False).replace("</", "<\\/")
    return TEMPLATE.read_text(encoding="utf-8").replace("__DATA__", payload)


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("system", nargs="?", default="osa_ak")
    ap.add_argument("--out")
    ap.add_argument("--list", help="where to write the list (default docs/generated/<system>_controls.md)")
    ap.add_argument("--check", action="store_true")
    args = ap.parse_args(argv)

    console = json.loads((ROOT / "data" / "procedures" / f"{args.system}.json").read_text(encoding="utf-8"))
    cabin_path = ROOT / "data" / "cabins" / f"{args.system}.json"  # a system without a cabin yet: no lamps or displays
    cabin = json.loads(cabin_path.read_text(encoding="utf-8")) if cabin_path.exists() else {"panels": []}
    refs = find_refs([c["id"] for c in console["controls"]])
    veh_path = ROOT / "data" / "vehicles" / f"{args.system}_systems.json"  # the vehicle's own systems, where built
    vehicle = json.loads(veh_path.read_text(encoding="utf-8")) if veh_path.exists() else None
    mb = build(console, cabin, refs, vehicle)
    data = mb.data()
    write_list(mb, Path(args.list) if args.list else ROOT / "docs" / "generated" / f"{args.system}_controls.md")
    out = Path(args.out) if args.out else ROOT / "out" / "button_map" / f"{args.system}.html"
    out.parent.mkdir(parents=True, exist_ok=True)
    out.write_text(render(data), encoding="utf-8")

    c = data["counts"]
    print(f"{out}: {c['controls']} controls (" + ", ".join(f"{c[s]} {s}" for s in STATES) + "), "
          f"{c['subsystems']} subsystems, {c['lamps']} lamps, {c['displays']} displays, "
          f"{c['procedures']} procedures, {c['planned']} not built")
    for p in mb.problems:
        print("problem:", p, file=sys.stderr)
    return 1 if args.check and mb.problems else 0


if __name__ == "__main__":
    sys.exit(main())
