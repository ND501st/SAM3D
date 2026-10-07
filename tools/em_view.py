"""Turn an electromagnetic-layer run into an interactive HTML page: the debug overlays in a browser.

Usage:  python tools/em_view.py [run.json] [--out FILE]

    run.json  written by simcore's em_demo (default out/em_demo.json)
    --out     output page, default out/em_view.html (out/ is git-ignored)

The page draws the same primitives Unreal draws with DrawDebug* (simcore/em/debug_draw.hpp): every
layer can be switched on and off, a time slider steps through the run, a selector shows one node's
knowledge (or every node's), and the event log and MVP checks sit beside the map. Terrain is
shaded from the run's 2 km height grid. Standard library only; the page has no external files.
"""
import argparse
import json
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent

# Layers drawn when the page opens (the rest are one click away).
DEFAULT_LAYERS = {"truth", "emitters", "beams", "coverage", "bearings", "fixes", "uncertainty", "tracks",
                  "c2_links", "comms_traffic", "jamming", "sensor_status"}

PAGE = """<!doctype html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>EM Debug Viewer</title>
<style>
:root { --bg:#f4f5f7; --panel:#ffffff; --ink:#1d2330; --muted:#5d6675; --line:#d5d9e0; --accent:#2f6fde; }
@media (prefers-color-scheme: dark) { :root:not([data-theme="light"]) {
  --bg:#0f1318; --panel:#171c23; --ink:#e3e7ee; --muted:#9aa3b2; --line:#2a313b; --accent:#6aa0ff; } }
:root[data-theme="dark"] { --bg:#0f1318; --panel:#171c23; --ink:#e3e7ee; --muted:#9aa3b2; --line:#2a313b; --accent:#6aa0ff; }
* { box-sizing: border-box; }
body { margin:0; background:var(--bg); color:var(--ink); font:13px/1.4 system-ui, -apple-system, "Segoe UI", sans-serif; }
header { padding:10px 16px; border-bottom:1px solid var(--line); display:flex; gap:16px; align-items:center; flex-wrap:wrap; }
header h1 { font-size:15px; margin:0; font-weight:600; }
header .sub { color:var(--muted); }
main { display:grid; grid-template-columns: 1fr 330px; gap:0; height: calc(100vh - 92px); }
#mapwrap { position:relative; min-height:300px; }
canvas { display:block; width:100%; height:100%; background:#0b0f14; cursor:grab; }
aside { border-left:1px solid var(--line); background:var(--panel); overflow:auto; padding:10px 12px; }
aside h2 { font-size:12px; text-transform:uppercase; letter-spacing:.06em; color:var(--muted); margin:12px 0 6px; }
.layers label { display:inline-flex; gap:4px; align-items:center; width:48%; padding:1px 0; }
.controls { padding:8px 16px; border-bottom:1px solid var(--line); display:flex; gap:12px; align-items:center; flex-wrap:wrap; }
.controls input[type=range] { flex:1; min-width:200px; }
button, select { background:var(--panel); color:var(--ink); border:1px solid var(--line); border-radius:6px; padding:4px 10px; font:inherit; }
button:hover { border-color:var(--accent); }
#events { font:12px/1.35 ui-monospace, Consolas, monospace; white-space:pre-wrap; }
#events .now { color:var(--accent); }
.check { display:flex; gap:6px; } .ok { color:#2fae5b; } .bad { color:#e5483b; }
#time { font-variant-numeric: tabular-nums; min-width:70px; display:inline-block; }
#hover { position:absolute; left:8px; bottom:8px; color:#cfd6e2; font:12px ui-monospace, Consolas, monospace;
  background:rgba(0,0,0,.45); padding:3px 6px; border-radius:4px; }
@media (max-width: 800px) { main { grid-template-columns: 1fr; height:auto; } #mapwrap { height:70vh; }
  aside { border-left:0; border-top:1px solid var(--line); } }
</style>
</head>
<body>
<header><h1 id="title"></h1><span class="sub">Same overlay primitives as Unreal's <code>sam3d.em.Debug</code>. Drag to pan, wheel to zoom.</span></header>
<div class="controls">
  <button id="play">Play</button>
  <span id="time"></span>
  <input id="slider" type="range" min="0" value="0" step="1">
  <label>speed <select id="speed"><option>1</option><option selected>4</option><option>10</option><option>30</option></select>x</label>
  <label>knowledge of <select id="owner"></select></label>
  <button id="fit">Fit</button>
</div>
<main>
  <div id="mapwrap"><canvas id="map"></canvas><div id="hover"></div></div>
  <aside>
    <h2>Layers</h2><div class="layers" id="layers"></div>
    <h2>MVP checks</h2><div id="checks"></div>
    <h2>Events</h2><div id="events"></div>
  </aside>
</main>
<script>
const DATA = __DATA__;
const DEFAULTS = new Set(__DEFAULTS__);
const meta = DATA.meta, frames = DATA.frames;
document.getElementById('title').textContent = meta.title;
const canvas = document.getElementById('map'), ctx = canvas.getContext('2d');
const slider = document.getElementById('slider');
slider.max = frames.length - 1;
let frame = 0, playing = false, view = {cx: 10000, cy: 15000, scale: 0.005}, layersOn = new Set();
const layerBox = document.getElementById('layers');
for (const [bit, name] of Object.entries(meta.layers)) {
  const id = 'L' + bit, on = DEFAULTS.has(name);
  if (on) layersOn.add(Number(bit));
  const lab = document.createElement('label');
  lab.innerHTML = `<input type="checkbox" id="${id}" ${on ? 'checked' : ''}> ${name.replace(/_/g, ' ')}`;
  layerBox.appendChild(lab);
  lab.querySelector('input').addEventListener('change', e => { e.target.checked ? layersOn.add(Number(bit)) : layersOn.delete(Number(bit)); draw(); });
}
const ownerSel = document.getElementById('owner');
ownerSel.innerHTML = '<option value="all">every node</option>' +
  Object.entries(meta.nodes).map(([id, n]) => `<option value="${id}">${n}</option>`).join('');
if (meta.default_owner !== undefined) ownerSel.value = String(meta.default_owner);
ownerSel.addEventListener('change', draw);
document.getElementById('checks').innerHTML = meta.checks.map(c =>
  `<div class="check"><span class="${c.ok ? 'ok' : 'bad'}">${c.ok ? 'PASS' : 'FAIL'}</span><span>${c.name}</span></div>`).join('');

function rgba(c) { c >>>= 0; return `rgba(${c >>> 24},${(c >>> 16) & 255},${(c >>> 8) & 255},${((c & 255) / 255).toFixed(3)})`; }
function sx(e) { return canvas.width / 2 + (e - view.cx) * view.scale; }
function sy(n) { return canvas.height / 2 - (n - view.cy) * view.scale; }

// Terrain: shaded relief from the 2 km grid, drawn once per view change into an offscreen canvas.
const T = meta.terrain;
let hmin = Infinity, hmax = -Infinity;
for (const h of T.h) { hmin = Math.min(hmin, h); hmax = Math.max(hmax, h); }
function terrainColor(h, shade) {
  const t = (h - hmin) / Math.max(1, hmax - hmin);
  const r = 30 + 60 * t, g = 46 + 60 * t, b = 38 + 30 * t;
  const k = 0.75 + 0.5 * shade;
  return `rgb(${Math.round(r * k)},${Math.round(g * k)},${Math.round(b * k)})`;
}
function drawTerrain() {
  const cell = T.step * view.scale + 1;
  for (let r = 0; r < T.rows; r++) for (let c = 0; c < T.cols; c++) {
    const h = T.h[r * T.cols + c];
    const hx = T.h[r * T.cols + Math.min(c + 1, T.cols - 1)] - T.h[r * T.cols + Math.max(c - 1, 0)];
    const hy = T.h[Math.min(r + 1, T.rows - 1) * T.cols + c] - T.h[Math.max(r - 1, 0) * T.cols + c];
    const shade = Math.max(-1, Math.min(1, (-hx + hy) / 300));
    ctx.fillStyle = terrainColor(h, shade);
    const e = T.x0 + c * T.step, n = T.y0 + r * T.step;
    ctx.fillRect(sx(e) - cell / 2, sy(n) - cell / 2, cell, cell);
  }
}
function drawScaleBar() {
  const km = [1, 2, 5, 10, 20, 50][[1, 2, 5, 10, 20, 50].findIndex(k => k * 1000 * view.scale > 80)] || 50;
  const px = km * 1000 * view.scale;
  ctx.strokeStyle = '#cfd6e2'; ctx.lineWidth = 2; ctx.beginPath();
  ctx.moveTo(16, canvas.height - 40); ctx.lineTo(16 + px, canvas.height - 40); ctx.stroke();
  ctx.fillStyle = '#cfd6e2'; ctx.font = '12px system-ui'; ctx.fillText(km + ' km', 16, canvas.height - 46);
  ctx.fillText('N', canvas.width - 24, 22); ctx.beginPath(); ctx.moveTo(canvas.width - 20, 28); ctx.lineTo(canvas.width - 20, 50); ctx.stroke();
}
function draw() {
  const dpr = window.devicePixelRatio || 1, w = canvas.clientWidth, h = canvas.clientHeight;
  if (canvas.width !== Math.round(w * dpr)) { canvas.width = Math.round(w * dpr); canvas.height = Math.round(h * dpr); }
  ctx.setTransform(1, 0, 0, 1, 0, 0);
  ctx.fillStyle = '#0b0f14'; ctx.fillRect(0, 0, canvas.width, canvas.height);
  drawTerrain();
  const f = frames[frame], owner = ownerSel.value;
  for (const p of f.prims) {
    if (!layersOn.has(p.l)) continue;
    if (owner !== 'all' && p.o !== undefined && String(p.o) !== owner) continue;
    ctx.strokeStyle = ctx.fillStyle = rgba(p.c);
    ctx.lineWidth = (p.w || 1) * (window.devicePixelRatio || 1);
    ctx.setLineDash(p.d ? [6, 5] : []);
    const ax = sx(p.a[0]), ay = sy(p.a[1]);
    switch (p.s) {
      case 0: ctx.beginPath(); ctx.moveTo(ax, ay); ctx.lineTo(sx(p.b[0]), sy(p.b[1])); ctx.stroke(); break;
      case 1: ctx.beginPath(); ctx.arc(ax, ay, Math.max(1, p.r[0] * view.scale), 0, 2 * Math.PI); ctx.stroke(); break;
      case 2: { const g = p.g ? p.g[0] : 0; ctx.beginPath();
        ctx.ellipse(ax, ay, Math.max(1, p.r[0] * view.scale), Math.max(1, p.r[1] * view.scale), g - Math.PI / 2, 0, 2 * Math.PI); ctx.stroke(); break; }
      case 3: { const g = p.g || [0, 0]; ctx.beginPath(); ctx.moveTo(ax, ay);
        ctx.arc(ax, ay, p.r[0] * view.scale, g[0] - g[1] - Math.PI / 2, g[0] + g[1] - Math.PI / 2); ctx.closePath(); ctx.fill(); break; }
      case 4: ctx.setLineDash([]); ctx.font = `${11 * (window.devicePixelRatio || 1)}px system-ui`; ctx.fillText(p.t || '', ax + 4, ay); break;
      case 5: ctx.beginPath(); ctx.arc(ax, ay, (p.w || 3) * (window.devicePixelRatio || 1), 0, 2 * Math.PI); ctx.fill();
        if (p.t) { ctx.font = `${11 * (window.devicePixelRatio || 1)}px system-ui`; ctx.fillText(p.t, ax + 6, ay - 6); } break;
      case 6: if (!p.p) break; ctx.beginPath(); p.p.forEach((q, i) => i ? ctx.lineTo(sx(q[0]), sy(q[1])) : ctx.moveTo(sx(q[0]), sy(q[1])));
        ctx.closePath(); ctx.stroke(); break;
    }
  }
  ctx.setLineDash([]);
  drawScaleBar();
  document.getElementById('time').textContent = 't = ' + f.t.toFixed(1) + ' s';
  slider.value = frame;
  const evs = meta.events.filter(e => e.t <= f.t).slice(-40);
  document.getElementById('events').innerHTML = evs.map((e, i) =>
    `<div class="${i === evs.length - 1 ? 'now' : ''}">${e.t.toFixed(1).padStart(6)} ${e.k}: ${e.x.replace(/</g, '&lt;')}</div>`).join('');
}
slider.addEventListener('input', () => { frame = Number(slider.value); draw(); });
const playBtn = document.getElementById('play');
playBtn.addEventListener('click', () => { playing = !playing; playBtn.textContent = playing ? 'Pause' : 'Play'; });
let last = performance.now(), acc = 0;
function tick(now) {
  const dt = (now - last) / 1000; last = now;
  if (playing) {
    acc += dt * Number(document.getElementById('speed').value);
    const step = frames.length > 1 ? frames[1].t - frames[0].t : 1;
    while (acc >= step) { acc -= step; frame = Math.min(frame + 1, frames.length - 1); if (frame === frames.length - 1) { playing = false; playBtn.textContent = 'Play'; } }
    draw();
  }
  requestAnimationFrame(tick);
}
requestAnimationFrame(tick);
function fit() {
  const w = canvas.clientWidth * (window.devicePixelRatio || 1), h = canvas.clientHeight * (window.devicePixelRatio || 1);
  const ex = T.cols * T.step, ny = T.rows * T.step;
  view.cx = T.x0 + ex / 2; view.cy = T.y0 + ny / 2; view.scale = Math.min(w / ex, h / ny) * 0.98; draw();
}
document.getElementById('fit').addEventListener('click', fit);
let drag = null;
canvas.addEventListener('mousedown', e => { drag = {x: e.clientX, y: e.clientY, cx: view.cx, cy: view.cy}; canvas.style.cursor = 'grabbing'; });
window.addEventListener('mouseup', () => { drag = null; canvas.style.cursor = 'grab'; });
window.addEventListener('mousemove', e => {
  const dpr = window.devicePixelRatio || 1, rect = canvas.getBoundingClientRect();
  const east = view.cx + ((e.clientX - rect.left) * dpr - canvas.width / 2) / view.scale;
  const north = view.cy - ((e.clientY - rect.top) * dpr - canvas.height / 2) / view.scale;
  document.getElementById('hover').textContent = `E ${(east / 1000).toFixed(1)} km  N ${(north / 1000).toFixed(1)} km`;
  if (!drag) return;
  view.cx = drag.cx - (e.clientX - drag.x) * dpr / view.scale; view.cy = drag.cy + (e.clientY - drag.y) * dpr / view.scale; draw();
});
canvas.addEventListener('wheel', e => { e.preventDefault(); view.scale *= e.deltaY < 0 ? 1.15 : 1 / 1.15; draw(); }, {passive: false});
window.addEventListener('resize', draw);
fit();
</script>
</body>
</html>
"""


def build_page(run: dict) -> str:
    """The viewer page for one run (the run's JSON is embedded)."""
    for key in ("meta", "frames"):
        if key not in run:
            raise ValueError(f"not an em_demo run file: missing '{key}'")
    data = json.dumps(run, separators=(",", ":")).replace("</", "<\\/")
    return PAGE.replace("__DATA__", data).replace("__DEFAULTS__", json.dumps(sorted(DEFAULT_LAYERS)))


def main(argv=None) -> int:
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("run", nargs="?", default=str(ROOT / "out" / "em_demo.json"))
    ap.add_argument("--out", default=str(ROOT / "out" / "em_view.html"))
    args = ap.parse_args(argv)
    run_path = Path(args.run)
    if not run_path.exists():
        print(f"em_view: {run_path} not found (run simcore's em_demo first)", file=sys.stderr)
        return 2
    page = build_page(json.loads(run_path.read_text(encoding="utf-8")))
    out = Path(args.out)
    out.parent.mkdir(parents=True, exist_ok=True)
    out.write_text(page, encoding="utf-8")
    print(f"wrote {out} ({len(page) / 1e6:.1f} MB)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
