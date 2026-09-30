"""Build the SAM3D overview PDF: Osa-AK button map (from data/procedures/osa_ak.json) and feature list.

Usage:  python tools/build_overview_pdf.py [output.pdf]
Needs:  pip install reportlab   (Windows: uses Arial/Consolas from C:\\Windows\\Fonts for Cyrillic)

The button map, subsystems and procedures are generated from the data file, so re-run this after changing it.
"""
import glob
import json
import os
import re
import subprocess
import sys
from datetime import date

from reportlab.lib import colors
from reportlab.lib.enums import TA_CENTER
from reportlab.lib.pagesizes import A4, landscape
from reportlab.lib.styles import ParagraphStyle
from reportlab.lib.units import mm
from reportlab.pdfbase import pdfmetrics
from reportlab.pdfbase.ttfonts import TTFont
from reportlab.platypus import (KeepTogether, PageBreak, Paragraph, SimpleDocTemplate, Spacer, Table,
                                TableStyle)

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
CONSOLE = os.path.join(ROOT, "data", "procedures", "osa_ak.json")
SCENARIO = os.path.join(ROOT, "data", "scenarios", "osa_practice.json")

# ---------------------------------------------------------------- fonts and styles
FONT_DIR = os.path.join(os.environ.get("WINDIR", "C:\\Windows"), "Fonts")
pdfmetrics.registerFont(TTFont("Body", os.path.join(FONT_DIR, "arial.ttf")))
pdfmetrics.registerFont(TTFont("Body-Bold", os.path.join(FONT_DIR, "arialbd.ttf")))
pdfmetrics.registerFont(TTFont("Mono", os.path.join(FONT_DIR, "consola.ttf")))
pdfmetrics.registerFontFamily("Body", normal="Body", bold="Body-Bold", italic="Body", boldItalic="Body-Bold")

INK = colors.HexColor("#1f2933")
MUTED = colors.HexColor("#616e7c")
RULE = colors.HexColor("#cbd2d9")
PANEL = colors.HexColor("#f5f7fa")
KIND_COLOURS = {
    "switch": colors.HexColor("#3b6ea8"),
    "cover": colors.HexColor("#7b8794"),
    "key": colors.HexColor("#c07a12"),
    "button": colors.HexColor("#b83232"),
    "wheel": colors.HexColor("#2f855a"),
}
CONF_COLOURS = {
    "high": colors.HexColor("#2f855a"),
    "medium": colors.HexColor("#3b6ea8"),
    "low": colors.HexColor("#c07a12"),
    "estimate": colors.HexColor("#9b2c2c"),
}

S = {
    "title": ParagraphStyle("title", fontName="Body-Bold", fontSize=26, leading=31, textColor=INK),
    "subtitle": ParagraphStyle("subtitle", fontName="Body", fontSize=13, leading=17, textColor=MUTED),
    "h1": ParagraphStyle("h1", fontName="Body-Bold", fontSize=17, leading=21, textColor=INK, spaceBefore=4,
                         spaceAfter=6),
    "h2": ParagraphStyle("h2", fontName="Body-Bold", fontSize=12, leading=15, textColor=INK, spaceBefore=8,
                         spaceAfter=4),
    "body": ParagraphStyle("body", fontName="Body", fontSize=9.5, leading=13, textColor=INK),
    "small": ParagraphStyle("small", fontName="Body", fontSize=8, leading=10, textColor=MUTED),
    "cell": ParagraphStyle("cell", fontName="Body", fontSize=8, leading=10, textColor=INK),
    "cellb": ParagraphStyle("cellb", fontName="Body-Bold", fontSize=8, leading=10, textColor=INK),
    "mono": ParagraphStyle("mono", fontName="Mono", fontSize=7.5, leading=9.5, textColor=MUTED),
    "tile_label": ParagraphStyle("tile_label", fontName="Body-Bold", fontSize=10, leading=12, textColor=INK),
    "tile_kind": ParagraphStyle("tile_kind", fontName="Body-Bold", fontSize=7, leading=8, textColor=colors.white),
    "badge": ParagraphStyle("badge", fontName="Body-Bold", fontSize=6.5, leading=8, textColor=colors.white,
                            alignment=TA_CENTER),
}


def esc(text):
    return (str(text).replace("&", "&amp;").replace("<", "&lt;").replace(">", "&gt;"))


def P(text, style="body"):
    return Paragraph(text, S[style])


# ---------------------------------------------------------------- data
with open(CONSOLE, encoding="utf-8") as f:
    console = json.load(f)
with open(SCENARIO, encoding="utf-8") as f:
    scenario = json.load(f)
controls = {c["id"]: c for c in console["controls"]}
guarded_by = {}
for c in console["controls"]:
    if c.get("guard"):
        guarded_by.setdefault(c["guard"], []).append(c["id"])


def kind_of(c):
    """Display kind: covers and locks are switches that guard something."""
    if c["kind"] == "switch" and c["id"] in guarded_by:
        return "cover"
    return c["kind"]


def position_labels(c):
    if c["kind"] == "wheel":
        unit = c.get("unit", "")
        return f"{c['min']:g} … {c['max']:g} {unit}".strip()
    if c["kind"] == "button":
        return "press (springs back)"
    labels = c.get("position_labels") or [str(i) for i in range(c.get("positions", 2))]
    return "  ·  ".join(f"{i} {esc(l)}" for i, l in enumerate(labels))


def condition_text(cond):
    if "control" in cond:
        c = controls.get(cond["control"], {})
        labels = c.get("position_labels")
        if "is" in cond:
            pos = cond["is"]
            name = f" ({labels[pos]})" if labels and 0 <= pos < len(labels) else ""
            return f"{c.get('label', cond['control'])} = {pos}{name}"
        return f"{c.get('label', cond['control'])} ≥ {cond['at_least']}"
    if "subsystem" in cond:
        return f"<i>{cond['subsystem']}</i> {cond['is']}"
    return f"input <i>{cond['input']}</i> = {'yes' if cond['is'] else 'no'}"


def count_tests():
    n = 0
    for path in glob.glob(os.path.join(ROOT, "simcore", "tests", "*.cpp")):
        with open(path, encoding="utf-8") as f:
            n += len(re.findall(r"^SC_TEST\(", f.read(), flags=re.M))
    return n


def git(*args):
    try:
        return subprocess.check_output(["git", "-C", ROOT, *args], text=True, stderr=subprocess.DEVNULL).strip()
    except (OSError, subprocess.CalledProcessError):
        return "?"


# ---------------------------------------------------------------- building blocks
def badge(text, colour, width=16 * mm):
    t = Table([[P(esc(text).upper(), "badge")]], colWidths=[width], rowHeights=[4.2 * mm])
    t.setStyle(TableStyle([("BACKGROUND", (0, 0), (-1, -1), colour), ("VALIGN", (0, 0), (-1, -1), "MIDDLE"),
                           ("LEFTPADDING", (0, 0), (-1, -1), 1), ("RIGHTPADDING", (0, 0), (-1, -1), 1),
                           ("TOPPADDING", (0, 0), (-1, -1), 0), ("BOTTOMPADDING", (0, 0), (-1, -1), 0)]))
    return t


def tile(c, width):
    k = kind_of(c)
    head = Table([[P(k.upper(), "tile_kind"), badge(c["confidence"], CONF_COLOURS[c["confidence"]])]],
                 colWidths=[width - 20 * mm, 18 * mm])
    head.setStyle(TableStyle([("BACKGROUND", (0, 0), (-1, -1), KIND_COLOURS[k]), ("VALIGN", (0, 0), (-1, -1), "MIDDLE"),
                              ("LEFTPADDING", (0, 0), (-1, -1), 4), ("RIGHTPADDING", (0, 0), (-1, -1), 2),
                              ("TOPPADDING", (0, 0), (-1, -1), 2), ("BOTTOMPADDING", (0, 0), (-1, -1), 2)]))
    rows = [[head], [P(esc(c["label"]), "tile_label")], [P(esc(c["id"]), "mono")],
            [P(position_labels(c), "cell")]]
    if c.get("guard"):
        rows.append([P(f"Behind: <b>{esc(controls[c['guard']]['label'])}</b> (open it first)", "small")])
    if c["id"] in guarded_by:
        rows.append([P("Guards: " + ", ".join(esc(controls[g]["label"]) for g in guarded_by[c["id"]]), "small")])
    if c["kind"] == "key":
        rows.append([P("Moves one position at a time", "small")])
    rows.append([P("Source: " + esc(c["source"]), "small")])
    t = Table(rows, colWidths=[width])
    t.setStyle(TableStyle([("BOX", (0, 0), (-1, -1), 0.6, RULE), ("BACKGROUND", (0, 1), (-1, -1), colors.white),
                           ("LEFTPADDING", (0, 0), (-1, 0), 0), ("RIGHTPADDING", (0, 0), (-1, 0), 0),
                           ("TOPPADDING", (0, 0), (-1, 0), 0), ("BOTTOMPADDING", (0, 0), (-1, 0), 0),
                           ("LEFTPADDING", (0, 1), (-1, -1), 5), ("RIGHTPADDING", (0, 1), (-1, -1), 5),
                           ("TOPPADDING", (0, 1), (-1, -1), 1.5), ("BOTTOMPADDING", (0, 1), (-1, -1), 1.5),
                           ("VALIGN", (0, 0), (-1, -1), "TOP")]))
    return t


def grid(cells, cols, width):
    col_w = width / cols
    rows = [cells[i:i + cols] for i in range(0, len(cells), cols)]
    rows[-1] += [""] * (cols - len(rows[-1]))
    t = Table(rows, colWidths=[col_w] * cols)
    t.setStyle(TableStyle([("VALIGN", (0, 0), (-1, -1), "TOP"), ("LEFTPADDING", (0, 0), (-1, -1), 3),
                           ("RIGHTPADDING", (0, 0), (-1, -1), 3), ("TOPPADDING", (0, 0), (-1, -1), 3),
                           ("BOTTOMPADDING", (0, 0), (-1, -1), 3)]))
    return t


def data_table(header, rows, widths):
    body = [[P(h, "cellb") for h in header]] + [[c if not isinstance(c, str) else P(c, "cell") for c in r]
                                                 for r in rows]
    t = Table(body, colWidths=widths, repeatRows=1)
    t.setStyle(TableStyle([("BACKGROUND", (0, 0), (-1, 0), PANEL), ("LINEBELOW", (0, 0), (-1, 0), 0.8, INK),
                           ("LINEBELOW", (0, 1), (-1, -1), 0.3, RULE), ("VALIGN", (0, 0), (-1, -1), "TOP"),
                           ("LEFTPADDING", (0, 0), (-1, -1), 4), ("RIGHTPADDING", (0, 0), (-1, -1), 4),
                           ("TOPPADDING", (0, 0), (-1, -1), 3), ("BOTTOMPADDING", (0, 0), (-1, -1), 3)]))
    return t


def legend():
    items = [("switch", "Toggle or selector"), ("cover", "Cover / lock (guards another control)"),
             ("key", "Key switch"), ("button", "Push-button"), ("wheel", "Hand wheel (continuous)")]
    cells = []
    for k, text in items:
        cells += [badge(k, KIND_COLOURS[k], 15 * mm), P(text, "small")]
    conf = []
    for k, text in [("high", "read in a source"), ("medium", "source, some inference"),
                    ("low", "partly guessed; check"), ("estimate", "SAM3D design / no source")]:
        conf += [badge(k, CONF_COLOURS[k], 15 * mm), P(text, "small")]
    t = Table([cells, conf + ["", ""]], colWidths=[16 * mm, 36 * mm] * 5)
    t.setStyle(TableStyle([("VALIGN", (0, 0), (-1, -1), "MIDDLE"), ("LEFTPADDING", (0, 0), (-1, -1), 1),
                           ("TOPPADDING", (0, 0), (-1, -1), 1.5), ("BOTTOMPADDING", (0, 0), (-1, -1), 1.5)]))
    return t


# ---------------------------------------------------------------- document
def on_page(canvas, doc):
    canvas.saveState()
    w, h = landscape(A4)
    canvas.setStrokeColor(RULE)
    canvas.line(15 * mm, 11 * mm, w - 15 * mm, 11 * mm)
    canvas.setFont("Body", 7.5)
    canvas.setFillColor(MUTED)
    canvas.drawString(15 * mm, 7 * mm, "SAM3D — Osa-AK button map and feature list")
    canvas.drawRightString(w - 15 * mm, 7 * mm, f"Page {doc.page}")
    canvas.restoreState()


def build(out_path):
    page_w = landscape(A4)[0] - 30 * mm
    story = []
    today = date.today().isoformat()
    n_tests = count_tests()
    branch = git("rev-parse", "--abbrev-ref", "HEAD")
    commit = git("rev-parse", "--short", "HEAD")

    # ---- cover
    story += [Spacer(1, 18 * mm), P("SAM3D", "subtitle"), P("Osa-AK button map and feature list", "title"),
              Spacer(1, 4 * mm),
              P(f"What has been built so far, generated from the repository on {today} "
                f"(branch <b>{esc(branch)}</b>, commit <b>{esc(commit)}</b>).", "subtitle"),
              Spacer(1, 10 * mm)]
    kinds = {}
    for c in console["controls"]:
        kinds[kind_of(c)] = kinds.get(kind_of(c), 0) + 1
    stats = [["Controls", str(len(console["controls"]))], ["Subsystems", str(len(console["subsystems"]))],
             ["Sim inputs", str(len(console["inputs"]))], ["Procedures", str(len(console["procedures"]))],
             ["Unit tests", str(n_tests)], ["Scenarios", "1 (practice)"]]
    st = Table([[P(f"<b>{v}</b>", "h1") for _, v in stats], [P(k, "small") for k, _ in stats]],
               colWidths=[page_w / len(stats)] * len(stats))
    st.setStyle(TableStyle([("BACKGROUND", (0, 0), (-1, -1), PANEL), ("TOPPADDING", (0, 0), (-1, -1), 6),
                            ("BOTTOMPADDING", (0, 0), (-1, -1), 6), ("LEFTPADDING", (0, 0), (-1, -1), 10)]))
    story += [st, Spacer(1, 10 * mm),
              P("<b>Contents</b>", "h2"),
              P("1. Button map by operator station (every control in <font name='Mono'>data/procedures/osa_ak.json</font>)"),
              P("2. What the controls drive: subsystems and sim inputs"),
              P("3. Procedures, step by step"),
              P("4. Feature list: what the code does today"),
              P("5. Not done yet, and decisions pending"),
              Spacer(1, 8 * mm),
              P("Sources. Control labels, stations, behaviours and the engagement procedures come from SAM Simulator "
                "v930.2 (its Osa-AK manual and control sprites), used with its creator's blessing. Power, march, deploy "
                "and stow facts come from public sources; where no source gives the switch-level detail, the item is "
                "SAM3D's own design and is marked <b>estimate</b>. No weapon-performance data is included.", "small"),
              PageBreak()]

    # ---- 1. button map
    story += [P("1. Button map by operator station", "h1"),
              P("Each tile is one control. Positions count from 0. A control 'behind' a cover or lock can't move until "
                "that cover or lock is at position 1 (open / unlocked). Push-buttons read pressed for one sim step.",
                "body"), Spacer(1, 3 * mm), legend(), Spacer(1, 4 * mm)]
    station_names = {"commander": "Commander", "driver": "Driver",
                     "range_officer": "Range Officer (SAM Simulator: Z / Y panels)",
                     "firing_officer": "Firing Officer (SAM Simulator: X panel)"}
    order = ["commander", "driver", "range_officer", "firing_officer"]
    for station in order:
        cs = [c for c in console["controls"] if c.get("station") == station]
        if not cs:
            continue
        cols = 4
        tiles = [tile(c, page_w / cols - 6) for c in cs]
        story += [KeepTogether([P(station_names[station], "h2"), grid(tiles[:cols], cols, page_w)])]
        if len(tiles) > cols:
            story.append(grid(tiles[cols:], cols, page_w))
    story.append(PageBreak())

    # ---- 2. subsystems and inputs
    story += [P("2. What the controls drive", "h1"),
              P("Subsystems are the equipment behind the panel: off → starting → ready → stopping, or fault. Listed in "
                "dependency order. <b>Latching</b> modes are engaged by a button and stay on until their release "
                "condition. Switching on with a prerequisite missing (or losing one while running) faults the subsystem.",
                "body"), Spacer(1, 3 * mm)]
    rows = []
    for s in console["subsystems"]:
        on = [condition_text(c) for c in s.get("enable", [])]
        if s.get("enable_any"):
            on.append("any of: " + " / ".join(condition_text(c) for c in s["enable_any"]))
        if s.get("latching"):
            on.append("<b>latching</b>; released by " + ", ".join(condition_text(c) for c in s["release"]))
        pre = ", ".join(condition_text(c) for c in s.get("prerequisites", [])) or "—"
        t_start = s.get("start_time_s", {}).get("value")
        t_stop = s.get("stop_time_s", {}).get("value")
        times = []
        if t_start is not None:
            times.append(f"start {t_start:g} s ({s['start_time_s']['confidence']})")
        if t_stop is not None:
            times.append(f"stop {t_stop:g} s ({s['stop_time_s']['confidence']})")
        rows.append([f"<b>{esc(s['name'])}</b><br/><font name='Mono' size='7'>{esc(s['id'])}</font>",
                     "<br/>".join(on) or "—", pre, "<br/>".join(times) or "instant",
                     badge(s["confidence"], CONF_COLOURS[s["confidence"]])])
    story.append(data_table(["Subsystem", "On when", "Faults without", "Timing", "Conf."], rows,
                            [55 * mm, 105 * mm, 45 * mm, 40 * mm, 20 * mm]))
    story += [Spacer(1, 5 * mm), P("Sim inputs (set by the simulation or a scenario, not by a control)", "h2")]
    story.append(data_table(["Input", "Meaning", "Source"],
                            [[f"<font name='Mono'>{esc(i['id'])}</font>", esc(i["description"]), esc(i["source"])]
                             for i in console["inputs"]], [45 * mm, 140 * mm, 80 * mm]))
    launch = console.get("launch")
    if launch:
        story += [Spacer(1, 4 * mm),
                  P(f"<b>Launch:</b> pressing <b>{esc(controls[launch['button']]['label'])}</b> launches when "
                    f"<i>{esc(launch['armed'])}</i> is ready. Missile flight after launch is not simulated yet.")]
    story.append(PageBreak())

    # ---- 3. procedures
    story += [P("3. Procedures", "h1"),
              P("Crew work through these one step at a time, waiting for each step's condition (a lamp) before acting. "
                "A player's actions are judged against the same steps: acting before the condition is <b>early</b>, "
                "a different control is <b>out of order</b>.", "body")]
    for p in console["procedures"]:
        rows = []
        for i, st_ in enumerate(p["steps"], 1):
            c = controls[st_["control"]]
            labels = c.get("position_labels")
            pos = st_["position"]
            target = f"{pos} {esc(labels[pos])}" if labels else ("press" if c["kind"] == "button" else str(pos))
            wait = "<br/>".join(condition_text(w) for w in st_.get("wait_for", [])) or ""
            rows.append([str(i), f"<b>{esc(c['label'])}</b><br/><font name='Mono' size='7'>{esc(c['id'])}</font>",
                         target, esc(st_["description"]), wait])
        head = [P(f"{esc(p['name'])}  <font name='Mono' size='8' color='#616e7c'>{esc(p['id'])}</font>", "h2"),
                Table([[badge(p["confidence"], CONF_COLOURS[p["confidence"]]), P("Source: " + esc(p["source"]), "small")]],
                      colWidths=[18 * mm, page_w - 18 * mm], style=[("LEFTPADDING", (0, 0), (-1, -1), 0),
                                                                     ("VALIGN", (0, 0), (-1, -1), "MIDDLE")])]
        if p.get("notes"):
            head.append(P(esc(p["notes"]), "small"))
        table = data_table(["#", "Control", "To", "What the operator does", "Waits for"], rows,
                           [8 * mm, 48 * mm, 30 * mm, 124 * mm, 55 * mm])
        story.append(KeepTogether(head + [Spacer(1, 1.5 * mm), table]))
    story.append(PageBreak())

    # ---- 4. features
    story += [P("4. Feature list", "h1"),
              P("simcore is header-only C++17 with no dependencies, no exceptions and no RTTI, so it drops into Unreal "
                "unchanged. Everything below builds with warnings as errors and is covered by tests.", "body"),
              Spacer(1, 2 * mm)]
    features = [
        ("Console model", "panel.hpp", [
            "Discrete controls: toggles, rotary selectors, covers, locks.",
            "Key switches that move one position at a time (insert, then turn).",
            "Guards: a cover or lock physically blocks the control behind it.",
            "Hand wheels with a real value that clamp at their limits or wrap like a dial, and obey their lock.",
            "Push-buttons that read pressed for exactly one sim step.",
            "Electrical logic never blocks a switch; only physical guards do."]),
        ("Equipment behind the panel", "equipment.hpp", [
            "Subsystems: off, starting, ready, stopping, fault, with warm-up and stow times.",
            "Reversing half-way only undoes what was done (a half-raised mast lowers in half the time).",
            "Wrong order has consequences: energising without prerequisites, or losing one, faults it.",
            "Latching modes engaged by a button until a release condition.",
            "Any-of enables (e.g. missile preparation starts by hand or automatically).",
            "Sim inputs: yes/no signals like 'target in zone' that lamps and sequences depend on."]),
        ("Procedures and crew", "procedures.hpp", [
            "Procedures as ordered steps, each with the lamp or condition to wait for.",
            "Procedure judge: logs early and out-of-order actions with times and the step that was due.",
            "NPC crew operate the same controls as the player: order, queue, cancel.",
            "Crew wait for lamps, react with a delay after a lamp lights, and report started / completed / waiting / "
            "equipment fault / aborted.",
            "Crew stop and report if equipment they wait on faults, or a procedure forgets a cover."]),
        ("Data files", "json.hpp, console_data.hpp", [
            "Strict JSON reader (UTF-8, Cyrillic labels, line/column errors), no exceptions.",
            "Console files: controls, subsystems, inputs, procedures, launch button.",
            "Loader requires a source and confidence on every item, rejects unknown keys and bad references, and "
            "names the exact place of any error.",
            "Osa-AK console: engagement consoles from the SAM Simulator manual; power, march, deploy and stow from "
            "public sources (SAM3D design where no source goes to switch level)."]),
        ("Scenarios", "scenario.hpp, scenario_runner.hpp", [
            "Scenario files: origin, flat terrain, own unit and its console, crew, targets with routes, triggers.",
            "Triggers on time, target range, console state or launches: set inputs, order or queue crew, end the run.",
            "Runner steps targets, triggers, equipment and crew in a fixed order and records launches.",
            "Deterministic for a seed (crew reaction variation from a portable random mapping).",
            "Ends on a time limit, an end action, or when every target is gone."]),
        ("Debrief", "debrief.hpp, acmi.hpp", [
            "One timeline of crew reports, equipment faults, player deviations, scenario events and launches.",
            "Summary counts; no points (SAM Simulator's rule: 'no points, as in reality').",
            "Tacview ACMI track of the targets and the unit, once per simulated second."]),
        ("Earlier foundation (M0)", "geo, atmosphere, terrain, radar, aircraft, missile, world", [
            "WGS-84 / ECEF / local ENU frames; ISA atmosphere; flat and grid terrain; radar horizon and terrain "
            "line of sight; radar equation with generic placeholder numbers.",
            "Waypoint aircraft; a generic point-mass missile demo (placeholder numbers, not system data)."]),
        ("Tools and demos", "examples/, tools/", [
            "procedure_demo: an Osa console session from the march to the debrief.",
            "scenario_demo: plays data/scenarios/osa_practice.json and writes a Tacview file.",
            "Both run as tests in CTest, so CI checks the data files end to end.",
            "verify.ps1 / verify.sh build and test everything; this PDF is built by tools/build_overview_pdf.py."]),
    ]
    blocks = []
    for name, where, items in features:
        body = [P(f"<b>{esc(name)}</b>  <font name='Mono' size='7.5' color='#616e7c'>{esc(where)}</font>", "body")]
        body += [P("•  " + esc(i), "cell") for i in items]
        blocks.append(Table([[b] for b in body], colWidths=[page_w / 2 - 8],
                            style=[("BACKGROUND", (0, 0), (-1, -1), PANEL), ("LEFTPADDING", (0, 0), (-1, -1), 6),
                                   ("TOPPADDING", (0, 0), (-1, -1), 1.5), ("BOTTOMPADDING", (0, 0), (-1, -1), 1.5)]))
    story.append(grid(blocks, 2, page_w))

    # ---- 5. not done
    story += [PageBreak(), P("5. Not done yet, and decisions pending", "h1")]
    pending = [
        ["Engagement outcome (phase 3)", "A launch is recorded but nothing flies, hits or misses. Needs a decision: "
         "a gameplay-level outcome resolver (recommended) or detailed missile physics."],
        ["Radar model", "Search/track radar behaviour the operators see (scan, lock, break-lock, clutter) is not modelled; "
         "scenarios stand in with triggers such as 'target captured at 25 km' (placeholder)."],
        ["Unreal", "Unreal Engine is installed but the project has never been opened or built. No cabin, vehicle or "
         "Cesium terrain yet."],
        ["Real panel labels for power / АПУ controls", "Not in any public source found; English stand-ins, marked estimate."],
        ["Behaviour to check in SAM Simulator", "How the tracking-mode buttons (ЦУ АЗ, ПА, ПОЛУАВТОМАТ) release; elevation "
         "wheel limits; starting positions of some selectors. Marked low / estimate in the data."],
        ["Other systems", "Only the Osa-AK. Shilka manual downloaded for later."],
        ["Build coverage", "Built and tested with MSVC only. GCC/Clang run in CI once the pull requests are opened."],
        ["Pull requests", "Three branches pushed (fix, feature, scenarios); PRs not opened yet."],
    ]
    story.append(data_table(["Item", "Status"], pending, [70 * mm, 195 * mm]))

    doc = SimpleDocTemplate(out_path, pagesize=landscape(A4), leftMargin=15 * mm, rightMargin=15 * mm,
                            topMargin=13 * mm, bottomMargin=15 * mm,
                            title="SAM3D — Osa-AK button map and feature list", author="SAM3D")
    doc.build(story, onFirstPage=on_page, onLaterPages=on_page)


if __name__ == "__main__":
    out = sys.argv[1] if len(sys.argv) > 1 else os.path.join(ROOT, "out", "SAM3D_button_map_and_features.pdf")
    os.makedirs(os.path.dirname(os.path.abspath(out)), exist_ok=True)
    build(out)
    print("Wrote", out)
