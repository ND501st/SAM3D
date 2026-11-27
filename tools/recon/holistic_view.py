"""The holistic layer's look-alike check (docs/OSA_INTERIOR_HOLISTIC.md sections 11-13): every reference photo posed on
the cabin, the built cabin rendered from that photo's camera, and the two compared, component by component, edge by edge
and detail by detail, so that what the model lacks or has wrong is seen, listed and placed.

    python tools/recon/holistic_view.py check              # views and detail inventories well formed
    python tools/recon/holistic_view.py pose g092-a        # solve the photo's camera from its view file (anchors or a face camera)
    python tools/recon/holistic_view.py render g092-a      # the built cabin from that camera (Blender, background): model and id passes
    python tools/recon/holistic_view.py compare g092-a     # the contact sheet, components, edges, details: out/recon/holistic/views/g092-a/
    python tools/recon/holistic_view.py cast g092-a        # the photo's details cast onto their parents: 3-D places for the builders
    python tools/recon/holistic_view.py report             # docs/research/OSA_INTERIOR_LOOKALIKE_2026-11.md
    python tools/recon/holistic_view.py all g092-a         # pose, render, compare, cast

It reads the cabin data and the register, never writes them: what the comparison finds goes back through the register's
tables (holistic.py), the observations and the builders, and is checked again here. NumPy and OpenCV; Blender for render.
"""
import argparse
import glob
import json
import math
import os
import subprocess
import sys

import numpy as np

REPO = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import holistic as H  # noqa: E402
import interior as I  # noqa: E402  (the pipeline's lens model, read only)

VIEWS = os.path.join(REPO, 'tools', 'traces', 'osa_interior', 'views')
DETAILS = os.path.join(REPO, 'tools', 'traces', 'osa_interior', 'details')
OUT = os.path.join(REPO, 'out', 'recon', 'holistic', 'views')
REPORT = os.path.join(REPO, 'docs', 'research', 'OSA_INTERIOR_LOOKALIKE_2026-11.md')
SCENE = os.path.join(REPO, 'tools', 'blender', 'holistic_scene.py')
BLENDER = os.environ.get('SAM3D_BLENDER', r'C:\Program Files\Blender Foundation\Blender 5.2\blender.exe')
# The book's photos: the full run's if it exists, else the dry run's (tools/recon/osa_9a33bm2.json, S0-S3)
PHOTO_RUNS = [os.path.join(REPO, 'out', 'recon', 'osa_9a33bm2', 'photos'), os.path.join(REPO, 'out', 'recon', 'dryrun', 'photos')]

# ------------------------------------------------------------------------------------------------ the detail inventory
# What a photo shows below the register's components: every small thing that makes the part look like itself. Each kind
# has the way it is built by default (section 12's rule: relief that changes the silhouette or casts a shadow at arm's
# length is geometry; relief below that is a normal map baked from geometry; flat things are decals or texture).
DETAIL_KINDS = {
    'fastener': ('screw, bolt, nut, rivet or stud head on a surface', 'mesh'),
    'seam': ('a joint between plates or panels, a weld bead, a gap or overlap', 'normal'),
    'edge': ('a chamfer, fillet, rolled lip, flange or step along an edge', 'mesh'),
    'rib': ('a stiffener, swaged bead, corrugation or frame member', 'mesh'),
    'opening': ('a hole, vent, louvre, slot, grille or cut-out', 'mesh'),
    'hinge': ('a hinge, piano hinge or pivot', 'mesh'),
    'latch': ('a catch, clasp, toggle latch or lock', 'mesh'),
    'handle': ('a grip, pull, ring or lifting eye', 'mesh'),
    'label': ('a plate, stencil, sticker or printed table', 'decal'),
    'clip': ('a cable clip, P-clip, tie, saddle or strap fitting', 'mesh'),
    'grommet': ('a grommet, gland, cable entry or plug', 'mesh'),
    'gasket': ('a rubber seal, edge strip or bumper', 'mesh'),
    'pattern': ('perforation, tread plate, mesh, quilting, knurling: a repeated relief', 'normal'),
    'profile': ('the outline of a structural surface: a wall break, a sponson edge, a roof curve, a slope', 'mesh'),
    'surface': ('paint edge, two-tone, wear, dent, rust, stain, padding', 'texture'),
    'part': ('a small component of its own the register does not list (a lamp, a knob, a connector)', 'mesh'),
}
BUILD = ['mesh', 'normal', 'decal', 'texture']
RELIEF = ['raised', 'recessed', 'flush', 'through']
DETAIL_STATUS = ['absent', 'blocked', 'modelled', 'matched', 'not_built']   # not_built: a later fit's, with the reason
GEOM_KEYS = ['point', 'points', 'line', 'bbox']


def build_rule(kind, relief_mm=None, silhouette=False):
    """Section 12: geometry for anything that changes the silhouette or stands 2 mm or more proud of (or sunk into) its
    surface; a baked normal map for relief between 0.3 and 2 mm; a decal or texture for flat things."""
    if silhouette or kind in ('profile', 'part', 'handle', 'hinge'):
        return 'mesh'
    if relief_mm is None:
        return DETAIL_KINDS[kind][1]
    if relief_mm >= 2.0:
        return 'mesh'
    if relief_mm >= 0.3:
        return 'normal'
    return 'decal' if kind == 'label' else 'texture'


# ------------------------------------------------------------------------------------------------ files
def load(path):
    with open(path, encoding='utf-8') as f:
        return json.load(f)


def save(path, doc):
    os.makedirs(os.path.dirname(path), exist_ok=True)
    with open(path, 'w', encoding='utf-8', newline='\n') as f:
        json.dump(doc, f, indent=1, ensure_ascii=False)
        f.write('\n')


def view_path(photo):
    return os.path.join(VIEWS, photo + '.json')


def details_path(photo):
    return os.path.join(DETAILS, photo + '.json')


def photo_file(photo, kind='clean'):
    for run in PHOTO_RUNS:
        for ext in ('.png', '.jpg'):
            p = os.path.join(run, kind, photo + ext)
            if os.path.exists(p):
                return p
    return None


def views():
    return [load(p) for p in sorted(glob.glob(os.path.join(VIEWS, '*.json')))]


def detail_files():
    return [load(p) for p in sorted(glob.glob(os.path.join(DETAILS, '*.json')))]


# ------------------------------------------------------------------------------------------------ the cabin's points
def panel_axes(p):
    """simcore::CabinPanel's Normal, Right, Up (cabin.hpp)."""
    f, t = math.radians(p['facing_deg']), math.radians(p.get('tilt_deg', 0.0))
    n = np.array([math.sin(f) * math.cos(t), math.cos(f) * math.cos(t), math.sin(t)])
    r = np.array([-math.cos(f), math.sin(f), 0.0])
    return n, r, np.cross(n, r)


def panel_point(p, u, v):
    _, r, up = panel_axes(p)
    return np.array(p['centre_m'], float) + r * u + up * v


CORNERS = {'tl': (-0.5, 0.5), 'tr': (0.5, 0.5), 'bl': (-0.5, -0.5), 'br': (0.5, -0.5), 'c': (0.0, 0.0),
           'tc': (0.0, 0.5), 'bc': (0.0, -0.5), 'lc': (-0.5, 0.0), 'rc': (0.5, 0.0)}


def yaw_matrix(yaw_deg):
    """Clockwise from forward, seen from above (cabin.hpp's shell parts): a part's own +y (forward) turns to the right."""
    a = math.radians(yaw_deg or 0.0)
    return np.array([[math.cos(a), math.sin(a), 0.0], [-math.sin(a), math.cos(a), 0.0], [0.0, 0.0, 1.0]])


def box_corner(centre, size, signs, yaw_deg=0.0):
    off = np.array([s * 0.5 * d for s, d in zip(signs, size)])
    return np.array(centre, float) + yaw_matrix(yaw_deg) @ off


def box_corners(centre, size, yaw_deg=0.0):
    return np.array([box_corner(centre, size, (sx, sy, sz), yaw_deg)
                     for sx in (-1, 1) for sy in (-1, 1) for sz in (-1, 1)])


def _signs(spec):
    s = spec.replace(' ', '')
    if len(s) != 3 or any(ch not in '+-0' for ch in s):
        raise ValueError('a box corner is three of + - 0 (x y z): ' + spec)
    return tuple({'+': 1.0, '-': -1.0, '0': 0.0}[ch] for ch in s)


def register_box(entry):
    b = entry.get('box_m') or entry.get('estimate')
    if not b or not b.get('centre'):
        return None
    return b['centre'], b.get('size') or [0.1, 0.1, 0.1], b.get('sigma_m', 0.4)


def resolve(at, cab, reg):
    """A named point in the body frame: 'panel:<id>@tl' (tl tr bl br c tc bc lc rc, or u,v metres), 'shell:<id>@+-+' (a
    corner, edge middle or face centre of a shell part's box: x y z signs, 0 for the middle), 'reg:<id>@+-+' (the same
    on a register component's box), 'body:x,y,z'."""
    kind, _, rest = at.partition(':')
    if kind == 'body':
        return np.array([float(v) for v in rest.split(',')])
    ident, _, where = rest.partition('@')
    if kind == 'panel':
        p = cab['panels'][ident]
        if where in CORNERS:
            fu, fv = CORNERS[where]
            return panel_point(p, fu * p['size_m'][0], fv * p['size_m'][1])
        u, v = (float(x) for x in where.split(','))
        return panel_point(p, u, v)
    if kind == 'shell':
        s = cab['shell'][ident]
        return box_corner(s['centre_m'], s['size_m'], _signs(where), s.get('yaw_deg', 0.0))
    if kind == 'reg':
        b = register_box(reg[ident])
        if b is None:
            raise KeyError('register component without a place: ' + ident)
        return box_corner(b[0], b[1], _signs(where))
    raise KeyError('unknown point kind: ' + at)


def canon(ident, reg):
    """An observed id as the register keeps it: H3's aliases, and an accepted proposal without its `new.` prefix."""
    ident = H.ALIASES.get(ident, ident)
    if ident not in reg and ident.startswith('new.') and ident[4:] in reg:
        return ident[4:]
    return ident


def load_cabin():
    c = load(H.CABIN)
    return {'panels': {p['id']: p for p in c['panels']}, 'shell': {s['id']: s for s in c['shell']}, 'raw': c}


def load_register():
    return {e['id']: e for e in load(H.REGISTER)['components']}


# ------------------------------------------------------------------------------------------------ the camera
class Camera:
    """A pinhole in the body frame, OpenCV's convention: x_cam = R x_body + t, x right, y down, z ahead; pixels of an
    image w x h with the focal length f and principal point (cx, cy); k1 the radial term (OpenCV's), 0 if none."""

    def __init__(self, f, cx, cy, R, t, w, h, k1=0.0):
        self.f, self.cx, self.cy, self.w, self.h, self.k1 = float(f), float(cx), float(cy), int(w), int(h), float(k1)
        self.R, self.t = np.array(R, float), np.array(t, float)

    def to_dict(self):
        return {'f_px': self.f, 'pp_px': [self.cx, self.cy], 'R': self.R.tolist(), 't': self.t.tolist(),
                'size_px': [self.w, self.h], 'k1': self.k1}

    @classmethod
    def from_dict(cls, d):
        return cls(d['f_px'], d['pp_px'][0], d['pp_px'][1], d['R'], d['t'], d['size_px'][0], d['size_px'][1],
                   d.get('k1', 0.0))

    def scaled(self, w, h):
        """The same camera on a resampled copy of the photo (the descreened crop is smaller than the raw scan)."""
        sx, sy = w / self.w, h / self.h
        if abs(sx - sy) > 0.01 * sx:
            raise ValueError('not the same framing: %s vs %s' % ((self.w, self.h), (w, h)))
        return Camera(self.f * sx, self.cx * sx, self.cy * sy, self.R, self.t, w, h, self.k1)

    def centre(self):
        return -self.R.T @ self.t

    def cam(self, X):
        return (self.R @ np.asarray(X, float).T).T + self.t

    def project(self, X):
        """Pixels of body points, without the lens term (compare on the undistorted photo); NaN behind the camera."""
        Xc = self.cam(np.atleast_2d(X))
        with np.errstate(divide='ignore', invalid='ignore'):
            u = self.f * Xc[:, 0] / Xc[:, 2] + self.cx
            v = self.f * Xc[:, 1] / Xc[:, 2] + self.cy
        bad = Xc[:, 2] <= 1e-3
        u[bad] = np.nan
        v[bad] = np.nan
        return np.stack([u, v], axis=1)

    def undistort_px(self, px):
        """A pixel of the photo as taken (with the lens's barrel) to the pinhole's pixel."""
        if not self.k1:
            return np.asarray(px, float)
        import cv2
        K = np.array([[self.f, 0, self.cx], [0, self.f, self.cy], [0, 0, 1]], float)
        q = cv2.undistortPoints(np.array([[px]], float), K, np.array([self.k1, 0, 0, 0, 0]), P=K)
        return q.reshape(2)

    def ray(self, px):
        d = np.array([(px[0] - self.cx) / self.f, (px[1] - self.cy) / self.f, 1.0])
        d = self.R.T @ d
        return self.centre(), d / np.linalg.norm(d)

    def blender(self):
        """For tools/blender/holistic_scene.py: the camera's place, its axes in the body frame (Blender's camera looks down
        its -z with +y up) and the lens on a 36 mm sensor across the width, shifted to the principal point."""
        m = np.stack([self.R[0], -self.R[1], -self.R[2]], axis=1)
        big = max(self.w, self.h)
        return {'location': self.centre().tolist(), 'matrix': m.tolist(), 'sensor_mm': 36.0,
                'lens_mm': self.f * 36.0 / self.w, 'shift': [(0.5 * self.w - self.cx) / big, (self.cy - 0.5 * self.h) / big],
                'size': [self.w, self.h]}


def face_camera(cam_json, panel, face_w_m, face_h_m):
    """A camera solved on a traced face (tools/photo_trace.py camera, or the measuring run's: R, t in the face's frame, x
    right, y down, z into the face, origin at its top left, in mm or m) moved to the body frame by the panel it lies in."""
    c = cam_json if isinstance(cam_json, dict) else load(cam_json)
    t = np.array(c['t'], float)
    scale = 0.001 if 'K' in c else 1.0          # the measuring run's camera.json is in mm, photo_trace's _tc in m
    _, r, up = panel_axes(panel)
    n, _, _ = panel_axes(panel)
    A = np.stack([r, -up, -n], axis=1)          # columns: the face's x, y, z in the body frame
    tl = panel_point(panel, -0.5 * face_w_m, 0.5 * face_h_m)
    Rf = np.array(c['R'], float)
    R = Rf @ A.T
    tt = t * scale - R @ tl
    if 'K' in c:
        K = c['K']
        f, cx, cy = K[0][0], K[0][2], K[1][2]
        w, h = c['size']
        k1 = (c.get('dist') or [0.0])[0]
    else:
        f, w, h = c['f_px'], c['w'], c['h']
        cx, cy, k1 = 0.5 * w, 0.5 * h, 0.0
    return Camera(f, cx, cy, R, tt, w, h, k1)


def _pnp(P3, p2, f, cx, cy):
    import cv2
    K = np.array([[f, 0, cx], [0, f, cy], [0, 0, 1]], float)
    best = None
    for flag in (cv2.SOLVEPNP_SQPNP, cv2.SOLVEPNP_EPNP):
        try:
            ok, rv, tv = cv2.solvePnP(P3, p2, K, None, flags=flag)
        except cv2.error:
            continue
        if not ok:
            continue
        rv, tv = cv2.solvePnPRefineLM(P3, p2, K, None, rv, tv)
        proj, _ = cv2.projectPoints(P3, rv, tv, K, None)
        err = float(np.sqrt(np.mean(np.sum((proj.reshape(-1, 2) - p2) ** 2, axis=1))))
        if best is None or err < best[0]:
            best = (err, rv, tv)
    return best


# Where a photographer's camera can be in the compartment (body frame, m): across, between the main console's face
# (x -0.73) and the right racks' faces (x 0.59-0.61) less a body's room either side; along, the bulkhead to the cab; up,
# crouched over the floor (z 0.50) to just under the roof lining (z 2.26). A view gives its own `eye_box` where it needs
# another (the cab, a hatch).
EYE_BOX = [[-0.55, 0.45], [-0.95, 4.3], [0.8, 2.2]]


def _outside(eye, box):
    """How far (m) an eye is outside a box [[x0, x1], [y0, y1], [z0, z1]]; 0 inside."""
    return float(np.linalg.norm([max(lo - e, 0.0, e - hi) for e, (lo, hi) in zip(eye, box)]))


def solve_anchors(P3, p2, w, h, f_range=(0.35, 6.0), f_fixed=None, eye_box=None):
    """The camera that best puts the anchors' body points on their pixels: f searched over a range of frame widths (the
    prints are cropped, so the principal point is held at the centre), each f by PnP and Levenberg-Marquardt, the best f
    then refined by golden section. Returns the camera and each anchor's residual in px.

    Anchors on one plane (a console's face) leave f and the distance trading off against each other at nearly the same
    residual. `eye_box` then says where the photographer can have stood (the aisle, inside the compartment): the best f
    whose eye is inside it is taken, or, if none is, the one whose eye is nearest it."""
    import cv2
    P3, p2 = np.asarray(P3, float), np.asarray(p2, float)
    cx, cy = 0.5 * w, 0.5 * h
    if f_fixed:
        cand = [f_fixed]
    else:
        cand = list(np.geomspace(f_range[0] * w, f_range[1] * w, 48))

    def score(f):
        r = _pnp(P3, p2, f, cx, cy)
        if r is None:
            return None
        if eye_box is None:
            return r[0]
        R, _ = cv2.Rodrigues(r[1])
        out = _outside(-R.T @ r[2].ravel(), eye_box)
        return r[0] + (1e4 * (1.0 + out) if out > 0 else 0.0)

    scored = [(s, f) for f in cand for s in [score(f)] if s is not None]
    if not scored:
        raise RuntimeError('no pose fits the anchors')
    err, f = min(scored)
    if not f_fixed:
        i = cand.index(f)
        a, b = cand[max(i - 1, 0)], cand[min(i + 1, len(cand) - 1)]
        g = (math.sqrt(5) - 1) / 2
        for _ in range(40):
            c1, c2 = b - g * (b - a), a + g * (b - a)
            e1, e2 = score(c1), score(c2)
            if e1 is None or e2 is None:
                break
            if e1 < e2:
                b = c2
            else:
                a = c1
        f = 0.5 * (a + b)
    err, rv, tv = _pnp(P3, p2, f, cx, cy)
    R, _ = cv2.Rodrigues(rv)
    cam = Camera(f, cx, cy, R, tv.ravel(), w, h)
    res = np.linalg.norm(cam.project(P3) - p2, axis=1)
    return cam, res


def ray_box(origin, d, centre, size, yaw_deg=0.0):
    """Where a ray meets a box (slab method in the box's own frame): the entry point from outside, the exit point from
    inside (a wall, the roof or the floor seen from within the compartment); None if it misses."""
    M = yaw_matrix(yaw_deg)
    o = M.T @ (np.asarray(origin, float) - np.asarray(centre, float))
    dd = M.T @ np.asarray(d, float)
    lo, hi = -np.asarray(size, float) / 2, np.asarray(size, float) / 2
    t0, t1 = -np.inf, np.inf
    for i in range(3):
        if abs(dd[i]) < 1e-12:
            if o[i] < lo[i] or o[i] > hi[i]:
                return None
            continue
        a, b = (lo[i] - o[i]) / dd[i], (hi[i] - o[i]) / dd[i]
        t0, t1 = max(t0, min(a, b)), min(t1, max(a, b))
    if t1 < max(t0, 0.0):
        return None
    t = t0 if t0 > 0 else t1
    return np.asarray(origin, float) + t * np.asarray(d, float)


# ------------------------------------------------------------------------------------------------ check
SFM_CAMERAS = os.path.join(REPO, 'out', 'recon', 'holistic', 'sfm', 'cameras.json')
_REFINED = os.path.join(REPO, 'out', 'recon', 'holistic', 'sfm', 'refined', 'cameras.json')
if os.path.exists(_REFINED):      # the solve adjusted with focal-length and plane priors (tools/recon/holistic_refine.py) replaces the first solve wherever it exists
    SFM_CAMERAS = _REFINED


def measured_camera(m, w, h):
    """A camera read from the photo itself where the solve has none: the y axis's vanishing point (its height is the horizon), the focal length and the eye."""
    yaw, pitch = -math.atan((m['vanish_x_px'] - w / 2.0) / m['f_px']), math.atan((m['horizon_px'] - h / 2.0) / m['f_px'])
    fwd = np.array([math.sin(yaw) * math.cos(pitch), math.cos(yaw) * math.cos(pitch), math.sin(pitch)])   # a horizon above the image's middle: the camera looks down
    right = np.cross(fwd, [0, 0, 1.0])
    right /= np.linalg.norm(right)
    R = np.vstack([right, np.cross(fwd, right), fwd])
    return Camera(m['f_px'], w / 2.0, h / 2.0, R, -R @ np.array(m['eye_m'], float), w, h)


def check_view(v, cab, reg):
    errs = []
    for k in ('photo', 'image', 'camera_from'):
        if k not in v:
            errs.append('missing ' + k)
    if v.get('image') not in ('clean', 'raw'):
        errs.append('image is clean or raw')
    src = v.get('camera_from')
    if src == 'anchors':
        if len([a for a in v.get('anchors', []) if a.get('use', True)]) < 6:
            errs.append('at least six anchors used')
        for a in v.get('anchors', []):
            if not a.get('use', True) and not a.get('why'):
                errs.append('check %s needs its why' % a.get('at'))
        for a in v.get('anchors', []):
            try:
                resolve(a['at'], cab, reg)
            except (KeyError, ValueError) as e:
                errs.append('anchor %s: %s' % (a.get('at'), e))
            if len(a.get('px', [])) != 2:
                errs.append('anchor %s: px is [x, y]' % a.get('at'))
    elif src == 'face':
        f = v.get('face', {})
        for k in ('camera', 'panel', 'size_m'):
            if k not in f:
                errs.append('face needs ' + k)
        if f.get('panel') not in cab['panels']:
            errs.append('face panel not in the cabin: %s' % f.get('panel'))
    elif src == 'sfm':
        # the camera from the joint solve of the book's photos (tools/recon/holistic_sfm.py): no model point goes into it
        if not v.get('why'):
            errs.append('an sfm camera needs its why')
    elif src == 'measured':
        m = v.get('measured', {})
        for k in ('eye_m', 'f_px', 'vanish_x_px', 'horizon_px', 'how'):
            if k not in m:
                errs.append('measured camera needs %s' % k)
    elif src == 'pending':
        # no camera yet: the cabin has too little of what the photo shows, or what it has disagrees with itself as the
        # photo sees it; the anchors clicked so far are kept for the pose once the zone is fixed or room/ba lands
        if not v.get('why'):
            errs.append('a pending camera needs its why')
        for a in v.get('anchors', []):
            try:
                resolve(a['at'], cab, reg)
            except (KeyError, ValueError) as e:
                errs.append('anchor %s: %s' % (a.get('at'), e))
    else:
        errs.append('camera_from is anchors, face, sfm, measured or pending')
    st = v.get('state', {})                  # the vehicle as photographed: parts not there as built, each with its why
    if st.get('hide'):
        if not st.get('why'):
            errs.append('state.hide needs its why')
        for pid in st['hide']:
            if pid not in cab['shell'] and pid not in cab['panels']:
                errs.append('state.hide: %s is not a cabin part' % pid)
    box = v.get('eye_box')
    if box is not None and (len(box) != 3 or any(len(r) != 2 or r[0] >= r[1] for r in box)):
        errs.append('eye_box is [[x0, x1], [y0, y1], [z0, z1]]')
    k1 = v.get('k1')
    if k1 is not None and not (isinstance(k1.get('half_diagonal'), (int, float)) and k1.get('from')):
        errs.append('k1 is {"half_diagonal": <the pipeline\'s calib k1>, "from": <where>}')
    return ['%s: %s' % (v.get('photo'), e) for e in errs]


def check_details(d, reg):
    errs = []
    seen = set()
    for i, x in enumerate(d.get('details', [])):
        tag = '%s %s' % (d.get('photo'), x.get('id', i))
        if x.get('id') in seen:
            errs.append(tag + ': id twice')
        seen.add(x.get('id'))
        if x.get('kind') not in DETAIL_KINDS:
            errs.append(tag + ': kind %s' % x.get('kind'))
        on = canon(x.get('on', ''), reg)
        if not (on in reg or on.startswith('new.')):
            errs.append(tag + ': on %s is neither registered nor new.*' % on)
        geo = [k for k in GEOM_KEYS if k in x]
        if len(geo) != 1:
            errs.append(tag + ': one of point, points, line, bbox')
        else:
            pts = _detail_points(x)
            if any(not (0.0 <= p[0] <= 1.0 and 0.0 <= p[1] <= 1.0) for p in pts):
                errs.append(tag + ': image fractions 0..1')
        if x.get('build') not in BUILD:
            errs.append(tag + ': build %s' % x.get('build'))
        if x.get('relief') not in RELIEF:
            errs.append(tag + ': relief %s' % x.get('relief'))
        if x.get('status') not in DETAIL_STATUS:
            errs.append(tag + ': status %s' % x.get('status'))
        if x.get('status') == 'not_built' and not x.get('why'):
            errs.append(tag + ': not_built needs its why')
        if x.get('confidence') not in H.CONFIDENCE:
            errs.append(tag + ': confidence')
    return errs


def _detail_points(x):
    if 'point' in x:
        return [x['point']]
    if 'points' in x:
        return x['points']
    if 'line' in x:
        return x['line']
    b = x['bbox']
    return [[b[0], b[1]], [b[2], b[3]]]


def cmd_check(args):
    cab, reg = load_cabin(), load_register()
    errs = []
    for v in views():
        errs += check_view(v, cab, reg)
    for d in detail_files():
        errs += check_details(d, reg)
    for e in errs:
        print(e)
    print('%d views, %d detail files, %d problems' % (len(views()), len(detail_files()), len(errs)))
    return 1 if errs else 0


# ------------------------------------------------------------------------------------------------ pose
def pose(photo):
    v = load(view_path(photo))
    cab, reg = load_cabin(), load_register()
    img = photo_file(photo, v['image'])
    if img is None:
        raise FileNotFoundError('no %s photo %s (run tools/recon/osa_recon.py first)' % (v['image'], photo))
    import cv2
    h, w = cv2.imread(img).shape[:2]
    out = {'photo': photo, 'image': v['image'], 'size_px': [w, h], 'from': v['camera_from']}
    if v['camera_from'] == 'face':
        f = v['face']
        src = f['camera'] if isinstance(f['camera'], dict) else os.path.join(REPO, f['camera'])
        cam = face_camera(src, cab['panels'][f['panel']], f['size_m'][0], f['size_m'][1])
        cam = cam.scaled(w, h) if (cam.w, cam.h) != (w, h) else cam
        out['note'] = f.get('note', '')
        rows = []
        for a in v.get('anchors', []):          # optional checks: other points of the cabin against the photo
            X = resolve(a['at'], cab, reg)
            p = cam.project(X)[0]
            rows.append({'at': a['at'], 'px': a['px'], 'model_px': p.round(1).tolist(),
                         'residual_px': round(float(np.linalg.norm(p - a['px'])), 1)})
        out['anchors'] = rows
    elif v['camera_from'] == 'measured':       # a camera from the photo's own yardsticks, where no solve is to scale: eye, focal length, level and yaw
        cam = measured_camera(v['measured'], w, h)
        out['note'] = v.get('why', '')
    elif v['camera_from'] == 'sfm':
        solved = load(SFM_CAMERAS)['cameras']
        if photo not in solved:
            raise KeyError('%s is not in the joint solve' % photo)
        cam = Camera.from_dict(solved[photo])
        cam = cam.scaled(w, h) if (cam.w, cam.h) != (w, h) else cam
        out['note'] = v.get('why', '')
    else:
        # an anchor with "use": false is a check: a model point thought to be off, reported against the camera the
        # others give, not fitted to
        used = [a for a in v['anchors'] if a.get('use', True)]
        P3 = [resolve(a['at'], cab, reg) for a in used]
        # the lens's barrel from the pipeline's calib (its k1 per half-diagonal, from plumb lines): the clicks are
        # undistorted first, and the camera carries the same term in OpenCV's form for the comparison
        kd = v.get('k1', {}).get('half_diagonal', 0.0)
        undist = (lambda q: I.undistort(np.array([q], float), kd, w, h)[0].tolist()) if kd else (lambda q: q)
        p2 = [undist(a['px']) for a in used]
        box = v.get('eye_box', EYE_BOX)
        cam, res = solve_anchors(P3, p2, w, h, f_fixed=v.get('f_px'), eye_box=box)
        if kd:
            cam.k1 = kd * (cam.f / (0.5 * math.hypot(w, h))) ** 2
            out['k1'] = v['k1']
        out['anchors'] = [{'at': a['at'], 'px': a['px'], 'residual_px': round(float(r), 1)} for a, r in zip(used, res)]
        out['rms_px'] = round(float(np.sqrt(np.mean(res ** 2))), 2)
        out['checks'] = []
        for a in v['anchors']:
            if not a.get('use', True):
                p = cam.project(resolve(a['at'], cab, reg))[0]
                out['checks'].append({'at': a['at'], 'px': a['px'], 'model_px': p.round(1).tolist(),
                                      'residual_px': round(float(np.linalg.norm(p - np.array(undist(a['px'])))), 1),
                                      'why': a.get('why', '')})
        out['eye_box'] = box
        out['eye_outside_m'] = round(_outside(cam.centre(), box), 3)
        # each anchor left out in turn: an anchor whose own point moves the camera is a model point in the wrong place
        loo = []
        if len(P3) >= 7:
            for i in range(len(P3)):
                keep = [j for j in range(len(P3)) if j != i]
                c2, _ = solve_anchors([P3[j] for j in keep], [p2[j] for j in keep], w, h, f_fixed=cam.f)
                loo.append(round(float(np.linalg.norm(c2.project(P3[i])[0] - p2[i])), 1))
            for row, e in zip(out['anchors'], loo):
                row['left_out_px'] = e
    out['camera'] = cam.to_dict()
    out['eye_m'] = cam.centre().round(3).tolist()
    out['hfov_deg'] = round(math.degrees(2 * math.atan(0.5 * w / cam.f)), 1)
    save(os.path.join(OUT, photo, 'camera.json'), out)
    return out


def cmd_pose(args):
    for photo in args.photos:
        v = load(view_path(photo))
        if v['camera_from'] == 'pending':
            print('%s: camera pending: %s' % (photo, v['why']))
            continue
        out = pose(photo)
        print('%s: eye %s, hfov %.1f deg, %s%s' % (photo, out['eye_m'], out['hfov_deg'], out['from'],
                                                   ', rms %.1f px' % out['rms_px'] if 'rms_px' in out else ''))
        for a in out.get('anchors', []):
            print('   %-34s %6.1f px%s' % (a['at'], a['residual_px'],
                                          '  (left out %.1f)' % a['left_out_px'] if 'left_out_px' in a else ''))
        for a in out.get('checks', []):
            print('   check %-28s %6.1f px  model at %s' % (a['at'], a['residual_px'], a['model_px']))
    return 0


# ------------------------------------------------------------------------------------------------ render
def _posed(photo):
    if os.path.exists(os.path.join(OUT, photo, 'camera.json')):
        return True
    print('%s: no camera (pose it first, or its view is pending)' % photo)
    return False


def cmd_render(args):
    for photo in args.photos:
        if not _posed(photo):
            continue
        cam = Camera.from_dict(load(os.path.join(OUT, photo, 'camera.json'))['camera'])
        scale = args.width / cam.w if args.width else 1.0
        cam = cam.scaled(int(round(cam.w * scale)), int(round(cam.h * scale)))
        rays = []
        if os.path.exists(details_path(photo)):
            for x in load(details_path(photo))['details']:
                for i, p in enumerate(_detail_points(x)):
                    o, d = cam.ray(cam.undistort_px((p[0] * cam.w, p[1] * cam.h)))
                    rays.append(['%s:%d' % (x['id'], i), o.tolist(), d.tolist()])
        state = load(view_path(photo)).get('state', {})
        job = {'camera': cam.blender(), 'out': os.path.join(OUT, photo, 'render_'), 'passes': args.passes.split(','),
               'repo': REPO, 'rays': rays, 'hide': state.get('hide', [])}
        jp = os.path.join(OUT, photo, 'render_job.json')
        save(jp, job)
        cmd = [BLENDER, '--background', '--factory-startup', '--python', SCENE, '--', jp]
        r = subprocess.run(cmd, capture_output=True, text=True, encoding='utf-8', errors='replace')
        tail = [ln for ln in r.stdout.splitlines() if ln.startswith(('holistic_scene', 'Traceback', 'Error'))]
        print('\n'.join(tail[-12:]))
        if r.returncode != 0 or not os.path.exists(job['out'] + 'model.png'):
            print(r.stderr[-2000:])
            return 1
    return 0


# ------------------------------------------------------------------------------------------------ compare
TOL = 0.005          # an edge agrees when the other's edge is within 0.5 % of the width (about 10 px on a 2000 px crop)
WORK_W = 1600        # the comparison's working width
GRID = (8, 6)        # tiles across and down for the "what the model lacks" map


def _edges(gray, lo_q=0.5, k=1.0):
    import cv2
    g = cv2.bilateralFilter(gray, 7, 40, 7)
    v = float(np.median(g))
    lo, hi = int(max(0, (1.0 - 0.33 * k) * v * lo_q)), int(min(255, (1.0 + 0.33 * k) * v))
    return cv2.Canny(g, max(lo, 20), max(hi, 60)) > 0


def _id_edges(idimg):
    d = np.zeros(idimg.shape[:2], bool)
    a = idimg.astype(np.int32)
    key = a[..., 0] * 65536 + a[..., 1] * 256 + a[..., 2]
    d[:, 1:] |= key[:, 1:] != key[:, :-1]
    d[1:, :] |= key[1:, :] != key[:-1, :]
    return d


def _mask_bbox(mask):
    ys, xs = np.nonzero(mask)
    if len(xs) < 20:
        return None
    return [float(np.percentile(xs, 1)), float(np.percentile(ys, 1)), float(np.percentile(xs, 99)), float(np.percentile(ys, 99))]


def _iou(a, b):
    ix = max(0.0, min(a[2], b[2]) - max(a[0], b[0]))
    iy = max(0.0, min(a[3], b[3]) - max(a[1], b[1]))
    inter = ix * iy
    u = (a[2] - a[0]) * (a[3] - a[1]) + (b[2] - b[0]) * (b[3] - b[1]) - inter
    return inter / u if u > 0 else 0.0


def _proj_bbox(cam, centre, size, yaw=0.0):
    p = cam.project(box_corners(centre, size, yaw))
    p = p[~np.isnan(p[:, 0])]
    if len(p) < 2:
        return None
    b = [float(p[:, 0].min()), float(p[:, 1].min()), float(p[:, 0].max()), float(p[:, 1].max())]
    b = [max(b[0], 0.0), max(b[1], 0.0), min(b[2], cam.w), min(b[3], cam.h)]
    return b if b[2] > b[0] and b[3] > b[1] else None


def compare(photo):
    import cv2
    cdoc = load(os.path.join(OUT, photo, 'camera.json'))
    cam0 = Camera.from_dict(cdoc['camera'])
    img = cv2.imread(photo_file(photo, cdoc['image']))
    valid = None
    if cam0.k1:
        K = np.array([[cam0.f, 0, cam0.cx], [0, cam0.f, cam0.cy], [0, 0, 1]])
        img = cv2.undistort(img, K, np.array([cam0.k1, 0, 0, 0, 0]))
        # what the undistorted photo still covers: its empty borders are not the photo's structure
        valid = cv2.undistort(np.full(img.shape[:2], 255, np.uint8), K, np.array([cam0.k1, 0, 0, 0, 0]))
    s = WORK_W / img.shape[1]
    W, Hh = WORK_W, int(round(img.shape[0] * s))
    cam = cam0.scaled(W, Hh)
    photo_im = cv2.resize(img, (W, Hh), interpolation=cv2.INTER_AREA)
    model = cv2.resize(cv2.imread(os.path.join(OUT, photo, 'render_model.png')), (W, Hh), interpolation=cv2.INTER_AREA)
    idimg = cv2.resize(cv2.imread(os.path.join(OUT, photo, 'render_id.png')), (W, Hh), interpolation=cv2.INTER_NEAREST)
    idmap = load(os.path.join(OUT, photo, 'render_id.json'))
    reg = load_register()
    obs_p = os.path.join(H.OBS_DIR, photo + '.json')
    obs = load(obs_p) if os.path.exists(obs_p) else {'components': []}
    det_p = details_path(photo)
    dets = load(det_p) if os.path.exists(det_p) else {'details': []}

    # -- edges: the photo's structure against the model's
    pe = _edges(cv2.cvtColor(photo_im, cv2.COLOR_BGR2GRAY))
    me = _edges(cv2.cvtColor(model, cv2.COLOR_BGR2GRAY), k=1.4) | _id_edges(idimg)
    if valid is not None:
        inside = cv2.erode(cv2.resize(valid, (W, Hh), interpolation=cv2.INTER_NEAREST), np.ones((15, 15), np.uint8)) > 0
        pe &= inside
        me &= inside
    bg = np.all(idimg < 8, axis=2)
    tol = TOL * W
    dt_m = cv2.distanceTransform((~me).astype(np.uint8), cv2.DIST_L2, 5)
    dt_p = cv2.distanceTransform((~pe).astype(np.uint8), cv2.DIST_L2, 5)
    supported = float(np.mean(dt_p[me] <= tol)) if me.any() else 0.0
    explained = float(np.mean(dt_m[pe] <= tol)) if pe.any() else 0.0
    lacking = pe & (dt_m > tol)
    gx, gy = GRID
    tiles = []
    for j in range(gy):
        for i in range(gx):
            sl = (slice(j * Hh // gy, (j + 1) * Hh // gy), slice(i * W // gx, (i + 1) * W // gx))
            n_p = int(pe[sl].sum())
            tiles.append({'tile': [i, j], 'photo_edges': n_p, 'lacking': int(lacking[sl].sum()),
                          'share': round(float(lacking[sl].sum()) / n_p, 2) if n_p else 0.0})

    # -- components: where the photo's observer saw each one against where the model has it
    rgb_key = {tuple(v['rgb']): k for k, v in idmap['ids'].items()}
    key_img = idimg[..., ::-1]
    masks = {}

    def mask_of(cabin_id):
        if cabin_id not in masks:
            rgb = idmap['by_part'].get(cabin_id)
            masks[cabin_id] = np.zeros(idimg.shape[:2], bool) if rgb is None else \
                np.all(np.abs(key_img.astype(int) - rgb) <= 6, axis=2)
        return masks[cabin_id]

    cab = load_cabin()
    blocks = {b['id']: b for b in cab['raw']['blocks']}

    def block_bbox(bid):
        b = blocks[bid]
        p = cab['panels'][b['panel']]
        u, v = b['at_m']
        w, h = b['size_m']
        pts = [panel_point(p, u + a * w, v + c * h) for a in (-0.5, 0.5) for c in (-0.5, 0.5)]
        q = cam.project(np.array(pts))
        if np.isnan(q).any():
            return None
        return [float(q[:, 0].min()), float(q[:, 1].min()), float(q[:, 0].max()), float(q[:, 1].max())]

    rows = []
    for c in obs['components']:
        cid = canon(c['id'], reg)
        e = reg.get(cid)
        ob = [c['bbox'][0] * W, c['bbox'][1] * Hh, c['bbox'][2] * W, c['bbox'][3] * Hh]
        row = {'id': cid, 'class': c.get('class'), 'confidence': c.get('confidence'), 'observed_px': [round(x) for x in ob]}
        mb, how, verdict = None, None, None
        if e is not None:
            row['status'] = e.get('status')
            cabin_ids = e.get('cabin', [])
            structure = H.CLASS_GROUP.get(e.get('class')) == 'structure'
            parts = [p for p in cabin_ids if p in idmap['by_part']]
            if 'hull' in cabin_ids and structure:
                # one mesh for floor, walls, roof and bulkheads: its outline says nothing of one surface; the edges do
                b = register_box(e)
                mb = _proj_bbox(cam, b[0], b[1]) if b and all(v is not None for v in b[0]) else None
                how, verdict = 'hull mesh', 'structure: judge by the edges'
            elif any(p in blocks for p in cabin_ids):
                mb, how = block_bbox([p for p in cabin_ids if p in blocks][0]), 'cabin:block'
            elif parts:
                m = np.zeros(idimg.shape[:2], bool)
                for p in parts:
                    m |= mask_of(p)
                mb, how = _mask_bbox(m), 'cabin:' + '+'.join(parts)
                if mb is None:
                    # the cabin has it, but not here: out of this view, or behind something
                    shells = [cab['shell'][p] for p in parts if p in cab['shell']]
                    pb = [_proj_bbox(cam, s['centre_m'], s['size_m'], s.get('yaw_deg', 0.0)) for s in shells]
                    pb = [b for b in pb if b]
                    verdict = 'the model has it elsewhere' if not pb else 'hidden in the model'
                    how = 'cabin:' + '+'.join(parts)
                    mb = pb[0] if pb else None
            if mb is None and verdict is None:
                b = register_box(e)
                if b is not None and all(v is not None for v in b[0]):
                    mb, how = _proj_bbox(cam, b[0], b[1]), ('register box' if e.get('box_m') else 'estimate ±%.2f m' % b[2])
        else:
            row['status'] = 'unregistered'
        if mb is None:
            row['verdict'] = verdict or ('not in the model' if not how else 'the model has it elsewhere')
            if how:
                row['model_from'] = how
        else:
            iou = _iou(ob, mb)
            dc = math.hypot((ob[0] + ob[2] - mb[0] - mb[2]) / 2, (ob[1] + ob[3] - mb[1] - mb[3]) / 2) / W
            row.update({'model_px': [round(x) for x in mb], 'model_from': how, 'iou': round(iou, 2),
                        'centre_off': round(dc, 3)})
            in_cabin = how.startswith('cabin')
            row['verdict'] = verdict or (('agrees' if iou >= 0.5 else 'off' if iou >= 0.2 else 'wrong place') if in_cabin else
                                         ('only a box in the register' if iou >= 0.2 else 'register box off'))
        rows.append(row)
    # the model's parts the photo's observer did not name (drawn, maybe hidden, maybe not there)
    owns = ownership(reg, cab)
    named = {p for r in rows for p in (r.get('model_from') or '').replace('cabin:', '').split('+')}
    for r in rows:
        named |= owns.get(r['id'], set())
    unnamed = []
    for part, rgb in idmap['by_part'].items():
        m = mask_of(part)
        share = float(m.mean())
        if share > 0.004 and part not in named:
            unnamed.append({'part': part, 'share': round(share, 3), 'bbox': [round(x) for x in _mask_bbox(m) or [0, 0, 0, 0]]})

    # -- details: the inventory's tally, and each detail drawn where the photo shows it
    tally = {}
    for x in dets['details']:
        tally.setdefault(x['status'], 0)
        tally[x['status']] += 1

    report = {'photo': photo, 'camera': {k: cdoc[k] for k in ('from', 'eye_m', 'hfov_deg') if k in cdoc},
              'edges': {'tolerance_px': round(tol, 1), 'model_supported': round(supported, 3),
                        'photo_explained': round(explained, 3), 'photo_edge_px': int(pe.sum()),
                        'model_edge_px': int(me.sum())},
              'lacking_tiles': sorted(tiles, key=lambda t: -t['lacking'])[:12], 'components': rows,
              'model_unnamed': sorted(unnamed, key=lambda u: -u['share']), 'details': tally,
              'model_cover': round(float(1.0 - bg.mean()), 3)}
    save(os.path.join(OUT, photo, 'compare.json'), report)
    _sheet(photo, photo_im, model, idimg, pe, me, lacking, rows, dets, cam, reg, W, Hh)
    return report


def _put(img, text, org, color, scale=0.45):
    import cv2
    cv2.putText(img, text, org, cv2.FONT_HERSHEY_SIMPLEX, scale, (0, 0, 0), 3, cv2.LINE_AA)
    cv2.putText(img, text, org, cv2.FONT_HERSHEY_SIMPLEX, scale, color, 1, cv2.LINE_AA)


DETAIL_COLOUR = {'absent': (60, 60, 255), 'blocked': (0, 170, 255), 'modelled': (255, 200, 0), 'matched': (80, 220, 80),
                 'not_built': (160, 160, 160)}


def _sheet(photo, photo_im, model, idimg, pe, me, lacking, rows, dets, cam, reg, W, Hh):
    """Four panels: the photo; the model from its camera; the model's edges (cyan) over the photo with the photo's edges
    the model lacks (magenta); the components, observed (yellow) against the model's (cyan, ghost boxes dashed), and the
    detail inventory by status."""
    import cv2
    a = photo_im.copy()
    b = model.copy()
    c = (photo_im * 0.55).astype(np.uint8)
    c[me] = (255, 255, 0)
    c[lacking] = (255, 0, 255)
    d = cv2.addWeighted(photo_im, 0.6, model, 0.4, 0)
    for r in rows:
        ob = r['observed_px']
        cv2.rectangle(d, (ob[0], ob[1]), (ob[2], ob[3]), (0, 230, 255), 1)
        if 'model_px' in r:
            mb = r['model_px']
            col = (255, 255, 0) if r['model_from'].startswith('cabin') else (255, 0, 255)
            cv2.rectangle(d, (mb[0], mb[1]), (mb[2], mb[3]), col, 1 if col == (255, 255, 0) else 1, cv2.LINE_4)
            cv2.line(d, ((ob[0] + ob[2]) // 2, (ob[1] + ob[3]) // 2), ((mb[0] + mb[2]) // 2, (mb[1] + mb[3]) // 2), col, 1)
        _put(d, '%s %s' % (r['id'].split('.', 1)[-1][:22], r.get('iou', '-')), (ob[0] + 3, ob[1] + 14), (0, 230, 255), 0.38)
    for x in dets['details']:
        col = DETAIL_COLOUR[x['status']]
        pts = [(int(p[0] * W), int(p[1] * Hh)) for p in _detail_points(x)]
        if 'bbox' in x:
            cv2.rectangle(a, pts[0], pts[1], col, 1)
        elif len(pts) > 1 and 'line' in x:
            cv2.polylines(a, [np.array(pts)], False, col, 2)
        else:
            for p in pts:
                cv2.circle(a, p, 4, col, 1)
        _put(a, x['id'], (pts[0][0] + 4, pts[0][1] - 3), col, 0.33)
    for img, title in ((a, '%s: the photo, details by status' % photo), (b, 'the built cabin from its camera'),
                       (c, 'model edges (cyan), photo edges the model lacks (magenta)'),
                       (d, 'components: observed (yellow), model (cyan), register only (magenta)')):
        _put(img, title, (10, 24), (255, 255, 255), 0.6)
    sheet = np.vstack([np.hstack([a, b]), np.hstack([c, d])])
    cv2.imwrite(os.path.join(OUT, photo, 'sheet.jpg'), sheet, [cv2.IMWRITE_JPEG_QUALITY, 88])
    cv2.imwrite(os.path.join(OUT, photo, 'lacking.jpg'), c, [cv2.IMWRITE_JPEG_QUALITY, 90])
    cv2.imwrite(os.path.join(OUT, photo, 'components.jpg'), d, [cv2.IMWRITE_JPEG_QUALITY, 90])
    cv2.imwrite(os.path.join(OUT, photo, 'details.jpg'), a, [cv2.IMWRITE_JPEG_QUALITY, 90])


def cmd_compare(args):
    for photo in args.photos:
        if not _posed(photo):
            continue
        r = compare(photo)
        e = r['edges']
        print('%s: model edges supported %.0f %%, photo edges explained %.0f %%; components %s; details %s' % (
            photo, 100 * e['model_supported'], 100 * e['photo_explained'],
            {k: sum(1 for x in r['components'] if x['verdict'] == k) for k in sorted({x['verdict'] for x in r['components']})},
            r['details']))
    return 0


# ------------------------------------------------------------------------------------------------ cast
def bare_meshes(cab):
    """A bare panel's mesh: the shell part lying in the panel's plane (within 5 cm) inside its outline (SP-2M2's face)."""
    out = {}
    for pid, p in cab['panels'].items():
        if not p.get('bare'):
            continue
        n, r, u = panel_axes(p)
        for sid, s in cab['shell'].items():
            d = np.array(s['centre_m'], float) - np.array(p['centre_m'], float)
            if abs(d @ n) < 0.05 and abs(d @ r) < 0.5 * p['size_m'][0] and abs(d @ u) < 0.5 * p['size_m'][1]:
                out.setdefault(pid, set()).add(sid)
    return out


def ownership(reg, cab):
    """The cabin parts each register component is drawn by: its own `cabin` ids, its children's (PARENTS and the
    entries naming it as parent), a bare panel's mesh, and a block's panel's mesh."""
    bare = bare_meshes(cab)
    blocks = {b['id']: b['panel'] for b in cab['raw']['blocks']}
    children = {}
    for e in reg.values():
        if e.get('parent'):
            children.setdefault(e['parent'], set()).add(e['id'])
    for parent, kids in getattr(H, 'PARENTS', {}).items():
        children.setdefault(parent, set()).update(kids)

    def parts(eid, depth=0):
        e = reg.get(eid)
        if e is None or depth > 4:
            return set()
        s = set(e.get('cabin', []))
        for c in list(s):
            s |= bare.get(c, set())
            if c in blocks:
                s.add(blocks[c])
                s |= bare.get(blocks[c], set())
        for k in children.get(eid, ()):
            s |= parts(k, depth + 1)
        return s
    return {eid: parts(eid) for eid in reg}


def cast(photo):
    """Each detail's points from the photo's camera into the cabin: the ray (eye and direction, for H4 to cross with a
    second photo's), where it meets the built model (Blender's ray cast at render time: the surface, its part and normal)
    and, when the model lacks the detail's parent, where it meets the parent's register box instead. A detail whose ray
    lands on its own parent has its place to within the model's accuracy; one that lands on something else is a thing
    the model lacks, standing in front of the surface named."""
    cdoc = load(os.path.join(OUT, photo, 'camera.json'))
    cam = Camera.from_dict(cdoc['camera'])
    hits_p = os.path.join(OUT, photo, 'render_cast.json')
    hits = load(hits_p) if os.path.exists(hits_p) else {}
    hcam = Camera.from_dict(cdoc['camera'])
    reg = load_register()
    owns = ownership(reg, load_cabin())
    dets = load(details_path(photo))
    out = []
    for x in dets['details']:
        parent = canon(x['on'], reg)
        e = reg.get(parent)
        own = owns.get(parent, set())
        rb = register_box(e) if e else None
        row = {'id': x['id'], 'on': parent, 'kind': x['kind'], 'build': x['build'], 'status': x['status'], 'points': []}
        for i, p in enumerate(_detail_points(x)):
            o, d = hcam.ray((p[0] * cam.w, p[1] * cam.h))
            pt = {'eye_m': np.round(o, 3).tolist(), 'dir': np.round(d, 4).tolist()}
            h = hits.get('%s:%d' % (x['id'], i))
            if h:
                part = h['part'] or ''
                pt['model_hit'] = {'body_m': h['body_m'], 'part': part, 'normal': h['normal']}
                pt['on_parent'] = part in own or part.replace('fix:', '') in own
            if rb is not None and not pt.get('on_parent') and all(v is not None for v in rb[0]):
                X = ray_box(o, d, rb[0], rb[1])
                if X is not None and np.all(np.isfinite(X)):
                    pt['register_box'] = {'body_m': np.round(X, 3).tolist(), 'sigma_m': rb[2]}
            row['points'].append(pt)
        on = [p.get('on_parent') for p in row['points']]
        row['placed'] = 'on its parent in the model' if on and all(on) else \
            ('on its parent\'s register box' if any('register_box' in p for p in row['points']) else
             ('a ray only: in front of %s' % ', '.join(sorted({p['model_hit']['part'] for p in row['points']
                                                                if 'model_hit' in p})) if any('model_hit' in p for p in row['points'])
              else 'a ray only'))
        out.append(row)
    save(os.path.join(OUT, photo, 'details_3d.json'), {'photo': photo, 'camera': cdoc['from'], 'details': out})
    return out


def cmd_cast(args):
    for photo in args.photos:
        if not os.path.exists(details_path(photo)):
            print(photo, ': no detail inventory')
            continue
        if not os.path.exists(os.path.join(OUT, photo, 'render_cast.json')):
            print(photo, ': not rendered (no camera yet), its details stay in the image')
            continue
        out = cast(photo)
        kinds = {}
        for r in out:
            k = r['placed'].split(':')[0]
            kinds[k] = kinds.get(k, 0) + 1
        print('%s: %d details: %s' % (photo, len(out), kinds))
    return 0


# ------------------------------------------------------------------------------------------------ report
def cmd_report(args):
    lines = ['# The Osa interior: photo against model (the look-alike check)', '',
             'Generated by `python tools/recon/holistic_view.py report` from `out/recon/holistic/views/*/compare.json`',
             '(docs/OSA_INTERIOR_HOLISTIC.md sections 11-13). Do not edit by hand.', '',
             '"Supported": the share of the model\'s edges with a photo edge within the tolerance. "Explained": the share '
             'of the photo\'s edges with a model edge within it; what is left is what the model lacks.', '',
             '| Photo | Camera | Eye (m) | Model edges supported | Photo edges explained | Components agree / off / wrong / '
             'register only / not in model | Details absent / blocked / modelled / matched |',
             '|---|---|---|---|---|---|---|']
    gaps = []
    for p in sorted(glob.glob(os.path.join(OUT, '*', 'compare.json'))):
        r = load(p)
        v = [x['verdict'] for x in r['components']]
        cnt = lambda k: sum(1 for x in v if x == k)  # noqa: E731
        d = r['details']
        lines.append('| %s | %s | %s | %.0f %% | %.0f %% | %d / %d / %d / %d / %d | %d / %d / %d / %d |' % (
            r['photo'], r['camera'].get('from'), r['camera'].get('eye_m'), 100 * r['edges']['model_supported'],
            100 * r['edges']['photo_explained'], cnt('agrees'), cnt('off'), cnt('wrong place'),
            cnt('only a box in the register') + cnt('register box off'), cnt('not in the model'),
            d.get('absent', 0), d.get('blocked', 0), d.get('modelled', 0), d.get('matched', 0)))
        for x in r['components']:
            if x['verdict'] not in ('agrees',):
                gaps.append((r['photo'], x))
    lines += ['', '## What the model lacks or has wrong, by photo', '',
              '| Photo | Component | Class | Verdict | IoU | Centre off (of width) | Model from |', '|---|---|---|---|---|---|---|']
    for photo, x in gaps:
        lines.append('| %s | `%s` | %s | %s | %s | %s | %s |' % (photo, x['id'], x.get('class'), x['verdict'], x.get('iou', ''),
                                                             x.get('centre_off', ''), x.get('model_from', '')))
    lines += ['', '## Drawn by the model where the photo\'s observer named nothing', '',
              'Parts covering more than 0.4 % of the frame that no observation names: hidden in the photo, missing from the '
              'vehicle photographed, or in the wrong place in the model. Each is looked at.', '',
              '| Photo | Cabin part | Share of the frame |', '|---|---|---|']
    for p in sorted(glob.glob(os.path.join(OUT, '*', 'compare.json'))):
        r = load(p)
        for u in r['model_unnamed']:
            lines.append('| %s | `%s` | %.1f %% |' % (r['photo'], u['part'], 100 * u['share']))
    lines += ['', '## Where the photo\'s structure is unexplained', '',
              'The tiles (8 across, 6 down) with the most photo edges no model edge comes near, and the observed components '
              'over them.', '', '| Photo | Tile | Edges lacking | Share | Over |', '|---|---|---|---|---|']
    for p in sorted(glob.glob(os.path.join(OUT, '*', 'compare.json'))):
        r = load(p)
        size = None
        cam_p = os.path.join(OUT, r['photo'], 'camera.json')
        if os.path.exists(cam_p):
            c = load(cam_p)['camera']['size_px']
            size = (WORK_W, WORK_W * c[1] / c[0])
        for t in r['lacking_tiles'][:6]:
            over = ''
            if size:
                i, j = t['tile']
                tb = [i * size[0] / GRID[0], j * size[1] / GRID[1], (i + 1) * size[0] / GRID[0], (j + 1) * size[1] / GRID[1]]
                over = ', '.join('`%s`' % x['id'] for x in r['components'] if _iou(tb, x['observed_px']) > 0.05)
            lines.append('| %s | %s | %d | %.0f %% | %s |' % (r['photo'], t['tile'], t['lacking'], 100 * t['share'], over))
    pend = [v for v in views() if v['camera_from'] == 'pending']
    if pend:
        lines += ['', '## Views without a camera yet', '',
                  'The cabin has too little of what these photos show, or what it has disagrees with itself as the photo '
                  'sees it, so no camera can be fitted to it honestly. The anchors clicked so far are kept in the view; the '
                  'pose runs once the zone is built or the pipeline\'s `room`/`ba` solves the photo. Their detail '
                  'inventories stand meanwhile.', '', '| Photo | Why |', '|---|---|']
        for v in pend:
            lines.append('| %s | %s |' % (v['photo'], v['why']))
    lines += ['', '## Details by kind and status', '', '| Photo | Kind | absent | blocked | modelled | matched | not built |',
              '|---|---|---|---|---|---|---|']
    for d in detail_files():
        by = {}
        for x in d['details']:
            by.setdefault(x['kind'], {}).setdefault(x['status'], 0)
            by[x['kind']][x['status']] += 1
        for k in sorted(by):
            s = by[k]
            lines.append('| %s | %s | %d | %d | %d | %d | %d |' % (d['photo'], k, s.get('absent', 0), s.get('blocked', 0),
                                                                 s.get('modelled', 0), s.get('matched', 0), s.get('not_built', 0)))
    os.makedirs(os.path.dirname(REPORT), exist_ok=True)
    with open(REPORT, 'w', encoding='utf-8', newline='\n') as f:
        f.write('\n'.join(lines) + '\n')
    print('wrote', REPORT)
    return 0


def cmd_all(args):
    for step in (cmd_pose, cmd_render, cmd_compare, cmd_cast):
        rc = step(args)
        if rc:
            return rc
    return 0


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__.split('\n')[0])
    sub = ap.add_subparsers(dest='cmd', required=True)
    sub.add_parser('check')
    sub.add_parser('report')
    for name in ('pose', 'render', 'compare', 'cast', 'all'):
        p = sub.add_parser(name)
        p.add_argument('photos', nargs='+')
        p.add_argument('--width', type=int, default=2000, help='render width (px); the photo\'s aspect')
        p.add_argument('--passes', default='model,id')
    a = ap.parse_args(argv)
    return {'check': cmd_check, 'pose': cmd_pose, 'render': cmd_render, 'compare': cmd_compare, 'cast': cmd_cast,
            'report': cmd_report, 'all': cmd_all}[a.cmd](a)


if __name__ == '__main__':
    sys.exit(main())
