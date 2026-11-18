"""Ready-made 3D models as base details. A model file (OBJ or GLB, y up) from the models folder is posed (standing,
lying on its back or front, tipped), scaled to millimetres and handed to scenes.sculpt() as a solid: sect(z) is the
outline of everything in the model above height z, seen from above. That makes the kit's usual one surface with no
overhangs: what would hang (the arm of a cross) is filled down to the ground, which is why such models are also
offered lying down, where they come out as a clean relief.

Only public-domain (CC0) models go in models/. Each pack keeps its own License.txt beside it. See models/SOURCES.md."""
import shapely
import os, math, functools
import numpy as np
from shapely.geometry import Polygon, MultiPoint
from shapely.ops import unary_union

ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "models")

@functools.lru_cache(maxsize=512)
def _load(rel):
    import trimesh
    if rel.lower().endswith(".glb"):          # a GLB is a scene: its parts carry their own placing, so take them where the scene puts them (world space), as one mesh
        sc = trimesh.load(os.path.join(ROOT, rel), force="scene", process=False, skip_materials=True)
        m = sc.to_mesh() if hasattr(sc, "to_mesh") else sc.dump(concatenate=True)
    else: m = trimesh.load(os.path.join(ROOT, rel), force="mesh", process=False, skip_materials=True)
    v = np.asarray(m.vertices, float); v = np.stack([v[:, 0], -v[:, 2], v[:, 1]], axis=1)          # y up -> z up
    return v, np.asarray(m.faces, int)

def _rx(t): c, s = math.cos(t), math.sin(t); return np.array([[1, 0, 0], [0, c, -s], [0, s, c]])
def _ry(t): c, s = math.cos(t), math.sin(t); return np.array([[c, 0, s], [0, 1, 0], [-s, 0, c]])
def _rz(t): c, s = math.cos(t), math.sin(t); return np.array([[c, -s, 0], [s, c, 0], [0, 0, 1]])
POSES = {"s": np.eye(3), "b": _rx(-math.pi / 2), "f": _rx(math.pi / 2), "l": _ry(math.pi / 2), "r": _ry(-math.pi / 2), "u": _rx(math.pi)}          # stand, on its back, on its front, on its left, on its right, upside down

def _above(t, z):
    """the part of triangle t (3 x 3) above height z, as xy points"""
    out = []
    for i in range(3):
        p, q = t[i], t[(i + 1) % 3]; pin, qin = p[2] > z, q[2] > z
        if pin: out.append((p[0], p[1]))
        if pin != qin: k = (z - p[2]) / (q[2] - p[2]); out.append((p[0] + k * (q[0] - p[0]), p[1] + k * (q[1] - p[1])))
    return out

def solid(rel, mm, cx, cy, yaw=0.0, pose="s", sink=0.0, mirror=False, tilt=0.0, tilt_dir=0.0, cap=None):
    """-> ([(sect, top)], footprint, top). mm: millimetres per model metre. sink: how deep it sits in the ground, as a
    part of its height. tilt: lean in radians towards tilt_dir. cap: tallest it may stand (scaled down to fit)."""
    V, F = _load(rel); v = V * ([-1, 1, 1] if mirror else [1, 1, 1])
    R = _rz(yaw) @ _rz(tilt_dir) @ _rx(tilt) @ _rz(-tilt_dir) @ POSES[pose]; v = v @ R.T * mm
    h = v[:, 2].max() - v[:, 2].min()
    if cap and h * (1 - sink) > cap: v = v * (cap / (h * (1 - sink))); h = v[:, 2].max() - v[:, 2].min()
    c = (v[:, :2].max(axis=0) + v[:, :2].min(axis=0)) / 2
    v = v - [c[0] - cx, c[1] - cy, v[:, 2].min() + sink * h]
    tri = v[F] if not mirror else v[F[:, ::-1]]; zmax = tri[:, :, 2].max(axis=1); top = float(zmax.max())
    # A section is asked for at many heights, and a model is tried in many places before one fits. So: the outline
    # for placing comes straight from the points (no joining of triangles at all), and the sections are built once,
    # from the top down, each level adding only the triangles that reach into its band to the level above it.
    zmin = tri[:, :, 2].min(axis=1); e1 = tri[:, 1, :2] - tri[:, 0, :2]; e2 = tri[:, 2, :2] - tri[:, 0, :2]
    seen = np.abs(e1[:, 0] * e2[:, 1] - e1[:, 1] * e2[:, 0]) * 0.5 > 1e-4          # an upright face covers no ground seen from above
    tri_, zmax_, zmin_ = tri[seen], zmax[seen], zmin[seen]
    def clip(pick, z):
        whole = pick & (zmin_ >= z); ps = list(shapely.polygons(tri_[whole][:, :, :2])) if whole.any() else []          # wholly above the level: the triangle as it is, all of them in one go
        for t in tri_[pick & ~whole]:          # cut by the level: its part above
            p = _above(t, z)
            if len(p) >= 3:
                g = Polygon(p)
                if g.area > 1e-4: ps.append(g if g.is_valid else g.buffer(0))
        return ps
    def join(ps):
        if not ps: return None
        try: return unary_union(ps)
        except Exception: return unary_union([g.buffer(0.004) for g in ps])          # a crossing the union trips on: a hair of fat on each triangle settles it
    K = 24; lad = {}; ground = {}
    def ladder():
        zp, gp = float("inf"), None          # nothing above the top: the first level takes every triangle that reaches over it
        for k in range(1, K + 1):
            z = top * (1 - k / K); ps = clip((zmax_ > z) & (zmin_ <= zp), z)
            if gp is not None: ps.append(gp)
            g = join(ps); lad[k] = (z, g); zp = z; gp = g if g is not None else gp
    def sect(z):
        if top <= 0 or z >= top: return None
        if z == 0.0 and not lad:          # only the outline on the ground is wanted so far (a try at placing): one joining, not the whole ladder
            if "g" not in ground: ground["g"] = join(clip(zmax_ > 0.0, 0.0))
            return ground["g"]
        if not lad: ladder()
        k = max(0, min(K, int(math.floor((1 - z / top) * K + 1e-9))))          # the built level at or just above z
        zp, gp = lad[k] if k else (float("inf"), None)
        if abs(zp - z) < 1e-9: return gp
        ps = clip((zmax_ > z) & (zmin_ <= zp), z)          # <= : a flat face lying exactly on a built level is taken again, which costs nothing
        if gp is not None: ps.append(gp)
        return join(ps)
    pts = [v[v[:, 2] >= 0][:, :2]]
    for a_, b_ in ((0, 1), (1, 2), (2, 0)):          # where edges cross the ground
        p_, q_ = tri[:, a_], tri[:, b_]; m_ = (p_[:, 2] > 0) != (q_[:, 2] > 0)
        if m_.any(): k_ = (0 - p_[m_, 2]) / (q_[m_, 2] - p_[m_, 2]); pts.append(p_[m_, :2] + k_[:, None] * (q_[m_, :2] - p_[m_, :2]))
    foot = MultiPoint(np.concatenate(pts)).convex_hull if sum(len(x) for x in pts) >= 3 else None
    if foot is None or foot.geom_type != "Polygon" or foot.area < 1e-4: foot = MultiPoint([(cx, cy)]).buffer(0.5)
    return [(sect, top)], foot, top
