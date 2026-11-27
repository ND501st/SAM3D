"""Stage 4 of docs/OSA_MODELLING_PROCESS.md: parts as objects. No bpy: the fit, the tests and the Blender builder read
the same object. Each part has its own frame, parameters that mean something, a mesh, and named feature curves (the
edges the photographs show), so a measured outline is compared with the edge of the same name.

Frames: `origin` and three unit axes in the body frame: `right`, `up` (in the part's main plane) and `normal` (out of
it, towards whoever looks at its face). A profile is a list of (a, b) metres along right and up.
"""
import numpy as np


def unit(v):
    v = np.asarray(v, float)
    return v / np.linalg.norm(v)


class Frame:
    def __init__(self, origin, right, up):
        self.origin, self.right, self.up = np.asarray(origin, float), unit(right), unit(up)
        self.normal = np.cross(self.right, self.up)

    @classmethod
    def on_plane(cls, n, d, origin_near, up_hint=(0, 0, 1.0)):
        """A frame lying in the plane n.X = d: its up the hint's part in the plane, its origin the foot of origin_near."""
        n = unit(n)
        up = unit(np.asarray(up_hint, float) - np.dot(up_hint, n) * n)
        o = np.asarray(origin_near, float)
        return cls(o + (d - np.dot(n, o)) * n, np.cross(up, n), up)

    def at(self, ab, out=0.0):
        ab = np.atleast_2d(np.asarray(ab, float))
        return self.origin + np.outer(ab[:, 0], self.right) + np.outer(ab[:, 1], self.up) + np.multiply.outer(np.broadcast_to(out, len(ab)), self.normal)

    def coords(self, X):
        d = np.atleast_2d(np.asarray(X, float)) - self.origin
        return np.stack([d @ self.right, d @ self.up], 1), d @ self.normal

    def to_dict(self):
        return {'origin_m': self.origin.round(5).tolist(), 'right': self.right.round(6).tolist(), 'up': self.up.round(6).tolist()}

    @classmethod
    def from_dict(cls, d):
        return cls(d['origin_m'], d['right'], d['up'])


def ear_clip(poly):
    """Triangles (index triples) of a simple polygon, either winding."""
    P = np.asarray(poly, float)
    idx = list(range(len(P)))
    area = 0.5 * np.sum(P[:, 0] * np.roll(P[:, 1], -1) - np.roll(P[:, 0], -1) * P[:, 1])
    if area < 0:
        idx.reverse()
    tris, guard = [], 0
    while len(idx) > 3 and guard < 10 * len(P):
        guard += 1
        for k in range(len(idx)):
            i, j, l = idx[k - 1], idx[k], idx[(k + 1) % len(idx)]
            a, b, c = P[i], P[j], P[l]
            cross = (b[0] - a[0]) * (c[1] - a[1]) - (b[1] - a[1]) * (c[0] - a[0])
            if cross <= 1e-14:
                continue
            others = [m for m in idx if m not in (i, j, l)]
            if others:
                Q = P[others]
                w0 = (b[0] - a[0]) * (Q[:, 1] - a[1]) - (b[1] - a[1]) * (Q[:, 0] - a[0])
                w1 = (c[0] - b[0]) * (Q[:, 1] - b[1]) - (c[1] - b[1]) * (Q[:, 0] - b[0])
                w2 = (a[0] - c[0]) * (Q[:, 1] - c[1]) - (a[1] - c[1]) * (Q[:, 0] - c[0])
                if np.any((w0 >= 0) & (w1 >= 0) & (w2 >= 0)):
                    continue
            tris.append((i, j, l))
            idx.pop(k)
            break
        else:
            break
    if len(idx) == 3:
        tris.append(tuple(idx))
    return tris


class Extrusion:
    """A profile in the frame's plane carried back along the normal by `depth` (a housing, a plate, a bench): its front
    is the profile at the frame's plane, its back the same profile `depth` behind. Features: `rim` (the front's outline),
    `back` (the back's outline)."""
    kind = 'extrusion'

    def __init__(self, frame, profile, depth, closed=True, material='Paint_Interior', front=True, back=True, out=0.0):
        self.frame, self.profile, self.depth, self.closed, self.material, self.front, self.back, self.out = frame, np.asarray(profile, float), float(depth), closed, material, front, back, float(out)

    def features(self):
        return {'rim': (self.frame.at(self.profile, self.out), self.closed), 'back': (self.frame.at(self.profile, self.out - self.depth), self.closed)}

    def mesh(self):
        n = len(self.profile)
        V = np.vstack([self.frame.at(self.profile, self.out), self.frame.at(self.profile, self.out - self.depth)])
        F = []
        for i in range(n if self.closed else n - 1):
            j = (i + 1) % n
            F += [(i, j, n + j), (i, n + j, n + i)]
        if self.closed:
            tris = ear_clip(self.profile)
            if self.front:
                F += [(a, b, c) for a, b, c in tris]
            if self.back:
                F += [(n + a, n + c, n + b) for a, b, c in tris]
        return V, F

    def to_dict(self):
        return {'type': self.kind, 'frame': self.frame.to_dict(), 'profile_m': self.profile.round(4).tolist(), 'depth_m': round(self.depth, 4), 'closed': self.closed, 'material': self.material, 'front': self.front, 'back': self.back, 'out_m': round(self.out, 4)}

    @classmethod
    def from_dict(cls, d):
        return cls(Frame.from_dict(d['frame']), d['profile_m'], d['depth_m'], d.get('closed', True), d.get('material', 'Paint_Interior'), d.get('front', True), d.get('back', True), d.get('out_m', 0.0))


class Tube:
    """A round bar or wire along a line in space. Feature: `axis`."""
    kind = 'tube'

    def __init__(self, points, radius, material='Metal_Steel', closed=False):
        self.points, self.radius, self.material, self.closed = np.asarray(points, float), float(radius), material, closed

    def features(self):
        return {'axis': (self.points, self.closed)}

    def mesh(self, seg=8):
        P = self.points
        n = len(P)
        T = np.gradient(P, axis=0)
        T /= np.linalg.norm(T, axis=1, keepdims=True)
        a = np.cross(T, [0, 0, 1.0])
        bad = np.linalg.norm(a, axis=1) < 1e-6
        a[bad] = np.cross(T[bad], [1.0, 0, 0])
        a /= np.linalg.norm(a, axis=1, keepdims=True)
        b = np.cross(T, a)
        ang = np.linspace(0, 2 * np.pi, seg, endpoint=False)
        V = (P[:, None, :] + self.radius * (np.cos(ang)[None, :, None] * a[:, None, :] + np.sin(ang)[None, :, None] * b[:, None, :])).reshape(-1, 3)
        F = []
        for i in range(n - 1):
            for k in range(seg):
                m = (k + 1) % seg
                F += [(i * seg + k, i * seg + m, (i + 1) * seg + m), (i * seg + k, (i + 1) * seg + m, (i + 1) * seg + k)]
        return V, F

    def to_dict(self):
        return {'type': self.kind, 'points_m': self.points.round(4).tolist(), 'radius_m': self.radius, 'material': self.material, 'closed': self.closed}

    @classmethod
    def from_dict(cls, d):
        return cls(d['points_m'], d['radius_m'], d.get('material', 'Metal_Steel'), d.get('closed', False))


_DIRS = np.array([[x, y, z] for x in (-1, 0, 1) for y in (-1, 0, 1) for z in (-1, 0, 1) if (x, y, z) != (0, 0, 0)], float)
_DIRS /= np.linalg.norm(_DIRS, axis=1, keepdims=True)


class RoundedBox:
    """A box with rounded edges: a place, a turn (rotation vector), three sizes, a corner radius. Feature: `hull`, the points whose silhouette is the box's."""
    kind = 'rounded_box'

    def __init__(self, centre, rotvec, size, radius, material='Paint_White'):
        self.centre, self.rotvec, self.size, self.radius, self.material = np.asarray(centre, float), np.asarray(rotvec, float), np.asarray(size, float), float(radius), material

    def rotation(self):
        th = float(np.linalg.norm(self.rotvec))
        if th < 1e-12:
            return np.eye(3)
        k = self.rotvec / th
        K = np.array([[0, -k[2], k[1]], [k[2], 0, -k[0]], [-k[1], k[0], 0]])
        return np.eye(3) + np.sin(th) * K + (1 - np.cos(th)) * K @ K

    def hull_points(self):
        half = 0.5 * self.size
        r = min(self.radius, 0.95 * half.min())
        corners = np.array([[sx, sy, sz] for sx in (-1, 1) for sy in (-1, 1) for sz in (-1, 1)], float) * (half - r)
        pts = (corners[:, None, :] + r * _DIRS[None, :, :]).reshape(-1, 3)
        return self.centre + (self.rotation() @ pts.T).T

    def features(self):
        return {'hull': (self.hull_points(), True)}

    def mesh(self):
        """The box without its rounding, as a closed shell (what a part with a mesh of its own is drawn and tested by)."""
        half = 0.5 * self.size
        c = np.array([[sx, sy, sz] for sx in (-1, 1) for sy in (-1, 1) for sz in (-1, 1)], float) * half
        V = self.centre + (self.rotation() @ c.T).T
        quads = [(0, 1, 3, 2), (4, 6, 7, 5), (0, 4, 5, 1), (2, 3, 7, 6), (0, 2, 6, 4), (1, 5, 7, 3)]
        return V, [(a, b, c_) for a, b, c_, d in quads] + [(a, c_, d) for a, b, c_, d in quads]

    def to_dict(self):
        return {'type': self.kind, 'centre_m': self.centre.round(4).tolist(), 'rotvec': self.rotvec.round(5).tolist(), 'size_m': self.size.round(4).tolist(), 'radius_m': round(self.radius, 4), 'material': self.material}

    @classmethod
    def from_dict(cls, d):
        return cls(d['centre_m'], d['rotvec'], d['size_m'], d['radius_m'], d.get('material', 'Paint_White'))


TYPES = {c.kind: c for c in (Extrusion, Tube, RoundedBox)}


def from_dict(d):
    return TYPES[d['type']].from_dict(d)
