"""Gamers Grass style 30 mm bases (6 styles) -> op list -> 2D art + SolidCraft build commands.

Rules: 30 mm, 4.0 mm tall, 12 deg rim, 6x2 magnet pocket underneath (6.2 x 2.1), no centre zone,
no dead rim (detail laid past the edge, one final 12 deg trim), nothing floats (ground cuts are removed
under every raised piece, raised pieces start 0.2 mm inside the ground), nothing taller than 3 mm above the
deck, raised detail >= 0.88 mm wide, grooves 0.2-0.6 mm, no loose props, lines end at something.
Usage: python3 gg.py art            -> layouts.png
       python3 gg.py build <style>  -> SolidCraft build (needs solidcraft-cli)
"""
import sys, math, random, json, os, time
import numpy as np
from shapely.geometry import Polygon, Point, LineString, MultiPoint, box
from shapely.ops import unary_union, voronoi_diagram
from shapely import affinity

def _base_mm():
    """base diameter in mm: --size on the command line, else the BASE_MM environment variable, else 30"""
    if "--size" in sys.argv:
        try: return float(str(sys.argv[sys.argv.index("--size") + 1]).lower().replace("mm", ""))
        except (IndexError, ValueError): pass
    try: return float(os.environ.get("BASE_MM", "30"))
    except ValueError: return 30.0

BASE_MM = _base_mm()
MESH = os.environ.get("KIT_ENGINE") == "mesh"
LEDGE = 0.0 if MESH else 0.05      # sloped bands stacked into one mound: SolidCraft needs each band a hair inside the one below;
LAP = 0.0 if MESH else 0.03        # the mesh builder does not, so there the bands meet exactly and the slope has no ledge at all

def max_height(mm=None):
    """tallest a detail may stand above the 4 mm deck: 3.3 mm, and half as much again on bases of 60 mm and up"""
    mm = BASE_MM if mm is None else mm
    if float(CFG.get("max_detail_height_mm", 3.3)) <= 0: return 1000.0          # 0 = no limit at all
    return float(CFG.get("max_detail_height_mm", 3.3)) * (float(CFG.get("big_base_height_factor", 1.5)) if mm >= 59.9 else 1.0)
R0, H, RIM = BASE_MM / 2.0, 4.0, 12.0
K = R0 / 15.0             # size factor against the 30 mm base the layouts were designed on
CLIP = R0 + 1.0           # detail runs past the blank wall; the final 12 deg rim trim cuts it flush
BLANK = R0 + 0.3
ZCAP = H + 3.0
SINK = 0.2
MINW = 0.88
from kit_settings import CFG
OUT = CFG["out_folder"]

def polys_of(g):
    if g is None or g.is_empty: return []
    if g.geom_type == "Polygon": return [g]
    if hasattr(g, "geoms"): return [x for q in g.geoms for x in polys_of(q)]
    return []

def inset(p, d): return p.buffer(-d, join_style="mitre", mitre_limit=10.0)

_TS = max(1.0, float(os.environ.get("KIT_SCALE", 1) or 1))          # the scenery scale: everything is built this many times larger than it is laid out,
def wide_enough(p, w=MINW): return not p.buffer(-(w / _TS) / 2 + 0.02 / _TS).is_empty          # so a part may be this many times thinner here and still print

def frame(cx, cy, a):
    c, s = math.cos(a), math.sin(a)
    return lambda u, v: (cx + u * c - v * s, cy + u * s + v * c)

def edges_survive(p, d):
    q = inset(p, d)
    return (not q.is_empty and q.geom_type == "Polygon" and len(q.exterior.coords) == len(p.exterior.coords))

def fit_for_taper(p, d):
    if p is None or p.is_empty or p.geom_type != "Polygon": return None
    p = Polygon(p.exterior).simplify(0.05)
    for _ in range(400):
        if edges_survive(p, d): return p
        c = list(p.exterior.coords)[:-1]
        if len(c) <= 4: return None
        el = [math.dist(c[i], c[(i + 1) % len(c)]) for i in range(len(c))]
        del c[(int(np.argmin(el)) + 1) % len(c)]
        p = Polygon(c)
        if not p.is_valid: return None
    return None

def stone(cx, cy, r, rng, aspect=None, nv=None, round_=False):
    if round_:
        g = unary_union([Point(cx + rng.uniform(-.4, .4) * r, cy + rng.uniform(-.4, .4) * r).buffer(r * rng.uniform(.55, .8), 10)
                         for _ in range(3)])
        return g.convex_hull.simplify(0.04)
    n = nv or rng.randint(5, 8); aspect = aspect or rng.uniform(.6, .95); rot = rng.uniform(0, math.pi)
    ts = sorted(rng.uniform(0, 2 * math.pi) for _ in range(n))
    ts = [i * 2 * math.pi / n * 0.6 + t * 0.4 for i, t in enumerate(ts)]
    pts = []
    for t in ts:
        rr = r * rng.uniform(.82, 1.1); x, y = rr * math.cos(t), rr * math.sin(t) * aspect
        pts.append((cx + x * math.cos(rot) - y * math.sin(rot), cy + x * math.sin(rot) + y * math.cos(rot)))
    return Polygon(pts).convex_hull

def star(cx, cy, ro, ri, n, rot):
    return Polygon([(cx + (ro if i % 2 == 0 else ri) * math.cos(rot + i * math.pi / n),
                     cy + (ro if i % 2 == 0 else ri) * math.sin(rot + i * math.pi / n)) for i in range(2 * n)])

def wiggle(a, b, rng, amp=0.35, n=3):
    (x0, y0), (x1, y1) = a, b; L = math.dist(a, b) or 1; nx, ny = -(y1 - y0) / L, (x1 - x0) / L
    pts = [a]
    for i in range(1, n):
        t = i / n; o = rng.uniform(-amp, amp)
        pts.append((x0 + (x1 - x0) * t + nx * o, y0 + (y1 - y0) * t + ny * o))
    return pts + [b]

LAYOUT = False

def area_count(n, region=None):
    """how many pieces to scatter: on a bigger base the count grows with the area (only while laying out the ground,
    and only for regions that are a real share of the base, not a small cluster spot)"""
    if not LAYOUT or K <= 1.001: return n
    if region is not None and region.area < 0.2 * math.pi * R0 * R0: return n
    return int(round(n * min(K * K, 30.0)))

class Scene:
    def __init__(s, name, title, seed, ground):
        s.name, s.title, s.rng, s.ground = name, title, random.Random(seed), ground
        s.disc = Point(0, 0).buffer(CLIP, 256)
        s.gcuts, s.ops, s.tcuts, s.feet, s.chops = [], [], [], [], []

    def clip(s, polys):
        out = []
        for p in polys:
            out += [g for g in polys_of(p.intersection(s.disc)) if g.area > 0.08]
        return out

    def gcut(s, polys, depth, kind="crack"):
        s.gcuts.append((s.clip(polys), depth, kind))

    def prism(s, polys, top, kind, z0=None, minw=MINW, foot=True):
        ps = [p for p in s.clip(polys) if wide_enough(p, minw)]
        if not ps: return []
        s.ops.append(("prism", ps, (H - SINK) if z0 is None else z0, top, kind))
        if foot and (z0 is None or z0 <= H): s.feet += ps
        return ps

    def _tiers(s, profs, z, h, tiers, kind):
        """the stacked tapered tiers for these profiles; returns (ops, top profiles, z)"""
        ops = []; first = True
        for frac, a in tiers:
            hh = frac * h + (SINK if first else LAP)
            d = hh * math.tan(math.radians(a)) + 0.03
            ok = [q for q in (fit_for_taper(p, d) for p in profs) if q is not None]
            if not ok: break
            ops.append(("taper", ok, round(z, 4), round(hh, 4), a, kind))
            profs = [g for q in ok for g in polys_of(inset(q, hh * math.tan(math.radians(a)) + LEDGE)) if g.area > 0.1]
            z += hh - LAP; first = False
        return ops, profs, z

    def _solids(s, profs, z, top, hb, tiers, kind):
        """ONE solid per piece (no stacked tiers): medium pieces get a straight extrude + chamfered top (s.chops),
        tiny ones (grit, pebbles) a single tapered extrude. The chamfer fallback is that same single tapered solid."""
        hh = top - z
        ins = sum(fr * hb * math.tan(math.radians(a)) for fr, a in tiers)          # how far the tiers stepped in
        a_eff = math.degrees(math.atan(ins / hh)) if hh > 0 else 0
        drop = min(tiers[-1][0] * hb, 0.6 * hb) if len(tiers) > 1 else 0.45 * hb
        run = drop * math.tan(math.radians(max(tiers[-1][1], 30)))
        tops = []
        for p in profs:
            q = fit_for_taper(p, hh * math.tan(math.radians(a_eff)) + 0.03) if a_eff > 0.5 else p
            single = ("taper", [q], round(z, 4), round(hh, 4), round(a_eff, 3), kind) if q is not None else ("prism", [p], round(z, 4), round(top, 4), kind)
            tops += [t for t in polys_of(inset(p, ins + 0.05)) if t.area > 0.1]
            concave = p.convex_hull.area - p.area > 0.04 * p.convex_hull.area
            if MESH and tiers[0][1] >= 12 and p.area >= 0.9 and kind not in ("grit", "pebble") and ins > 0.15:
                # a soft heap (dune, drift, earth, flesh, moss): a smooth dome, steep at the foot and easing to the crown,
                # made of many fine sloped bands that meet exactly, so there is no step and no flat-topped block
                inr = 2 * p.area / p.length; ins_ = min(max(ins, 0.6 * inr), 0.95 * inr)          # never further in than the shape is thick: a thin or lobed piece still closes over its crown
                m = max(5, min(12, int(hh / 0.1))); cur = [p]; zz = z
                for k in range(m):
                    u0, u1 = k / m, (k + 1) / m; hk = hh * ((1 - (1 - u1) ** 1.8) - (1 - (1 - u0) ** 1.8)); dd = ins_ / m
                    if hk < 0.02 or not cur: break
                    s.ops.append(("taper", cur, round(zz, 4), round(hk, 4), round(math.degrees(math.atan(dd / hk)), 3), kind))
                    zz += hk; cur = [t for q in cur for t in polys_of(inset(q, dd)) if t.area > 0.3]
                continue
            if kind in ("grit", "pebble") or p.area < 1.2 or run < 0.04 or concave:
                s.ops.append(single); continue           # tiny or star-shaped (tufts, splats): one tapered solid
            s.chops.append({"kind": kind, "poly": p, "z0": round(z, 4), "top": round(top, 4), "run": round(run, 4), "drop": round(drop, 4), "ops": [single]})
            s.ops.append(single[:-1] + ("rockc",))      # placement logic still sees the piece
        return tops

    def _tiers(s, profs, z, h, tiers, kind):
        """legacy stacked tapered tiers (only when chamfering is switched off); returns (ops, top profiles, z)"""
        ops = []; first = True
        for frac, a in tiers:
            hh = frac * h + (SINK if first else LAP)
            d = hh * math.tan(math.radians(a)) + 0.03
            ok = [q for q in (fit_for_taper(p, d) for p in profs) if q is not None]
            if not ok: break
            ops.append(("taper", ok, round(z, 4), round(hh, 4), a, kind))
            profs = [g for q in ok for g in polys_of(inset(q, hh * math.tan(math.radians(a)) + LEDGE)) if g.area > 0.1]
            z += hh - LAP; first = False
        return ops, profs, z

    def stack(s, polys, h, tiers, kind, z0=None):
        """A raised piece: rocks, mounds, tufts... Returns (top profiles, top z)."""
        profs = [p for p in s.clip(polys) if wide_enough(p)]
        if not profs: return [], H
        if z0 is None: s.feet += profs
        z = (H if z0 is None else z0) - SINK
        if CFG.get("chamfer_rocks", True) and CFG.get("base_first", True):
            top = z + SINK + h
            return s._solids(profs, z, top, h, tiers, kind), top
        ops, tp, zt = s._tiers(profs, z, h, tiers, kind)
        s.ops += ops
        return tp, zt + 0.03

    def tcut(s, polys, z0, z1, kind="slot"):
        ps = [p for p in s.clip(polys) if not p.buffer(-0.1).is_empty]
        if ps: s.tcuts.append((ps, z0, z1, kind))

    def scatter(s, n, rmin, rmax, region=None, gap=0.3, round_=True, tries=None):
        reg = (region if region is not None else Point(0, 0).buffer(R0 - 0.4, 128)).buffer(-0.2)
        busy = unary_union(s.feet).buffer(gap) if s.feet else Polygon()
        got = []; n = area_count(n, region)
        for _ in range(tries or n * 60):
            if len(got) >= n: break
            r = s.rng.uniform(rmin, rmax); a = s.rng.uniform(0, 2 * math.pi); d = math.sqrt(s.rng.random()) * CLIP
            p = stone(d * math.cos(a), d * math.sin(a), r, s.rng, round_=round_)
            if reg.contains(p) and not p.intersects(busy) and all(p.distance(q) > gap for q in got): got.append(p)
        return got

    def final_gcuts(s):
        feet = unary_union(s.feet).buffer(0.35 if CFG.get("base_first", True) else 0.05) if s.feet else Polygon()   # cuts stay clear of every piece's buried foot
        out = []
        for polys, depth, kind in s.gcuts:
            g = unary_union(polys).difference(feet)
            ps = [p.simplify(0.01) for p in polys_of(g) if p.area > 0.1 and not p.buffer(-0.1).is_empty]
            if ps: out.append((ps, depth, kind))
        return out

    def top(s):
        return max([o[3] for o in s.ops if o[0] == "prism"] + [o[2] + o[3] for o in s.ops if o[0] == "taper"] + [H]) - H

# ======================================================================= styles
def badlands():
    s = Scene("badlands", "Badlands", 11, "#a4553b"); rng = s.rng
    th = rng.uniform(0, 2 * math.pi); cx, cy = 7.5 * math.cos(th), 7.5 * math.sin(th)
    # layered shale/sandstone plates in one cluster: each plate 2-3 strata, upper strata shifted
    plates = [stone(cx, cy, 4.2, rng, .7, 6), stone(cx + 4.3 * math.cos(th + 1.9), cy + 4.3 * math.sin(th + 1.9), 3.0, rng, .75, 5),
              stone(cx + 4.0 * math.cos(th - 1.7), cy + 4.0 * math.sin(th - 1.7), 2.4, rng, .8, 5)]
    t1 = s.prism([unary_union(plates)], H + 0.9, "rock")
    t2 = []
    for i, p in enumerate(plates[:2]):
        q = affinity.translate(affinity.scale(p, .78, .78), rng.uniform(-.6, .6), rng.uniform(-.6, .6))
        t2 += polys_of(q.intersection(inset(unary_union(t1), 0.35)))
    t2 = s.prism(t2, H + 1.75, "rock", z0=H + 0.85)
    if t2:
        q = affinity.translate(affinity.scale(t2[0], .62, .62), rng.uniform(-.4, .4), rng.uniform(-.4, .4))
        s.prism(polys_of(q.intersection(inset(t2[0], 0.35))), H + 2.55, "rock", z0=H + 1.7)
        # a split running right across the top plate (ends at both edges of the plate)
        c = t2[0].centroid; a = th + 1.2
        ln = LineString([(c.x - 6 * math.cos(a), c.y - 6 * math.sin(a)), (c.x + 6 * math.cos(a), c.y + 6 * math.sin(a))])
        s.tcut([ln.buffer(0.25, cap_style=2).intersection(t2[0])], H + 1.3, H + 3.0, "crack")
    # loose shale slabs + grit
    for p in s.scatter(4, 1.2, 1.9, gap=0.4, round_=False):
        s.stack([p], 0.75, ((0.75, 5), (0.25, 40)), "rock")
    s.stack(s.scatter(30, 0.5, 0.75, gap=0.35), 0.35, ((0.7, 10), (0.3, 45)), "grit")
    # dried-mud crack network across the whole floor (cracks end at rocks or the rim)
    pts = MultiPoint([(rng.uniform(-17, 17), rng.uniform(-17, 17)) for _ in range(34)])
    edges = set()
    for cell in voronoi_diagram(pts, envelope=box(-20, -20, 20, 20)).geoms:
        c = [tuple(round(v, 3) for v in xy) for xy in cell.exterior.coords]
        for a, b in zip(c, c[1:]): edges.add(tuple(sorted((a, b))))
    lines = [LineString(wiggle(a, b, rng)).buffer(0.23, cap_style=1, resolution=4) for a, b in edges]
    s.gcut([unary_union(lines)], 0.45, "crack")
    return s

def highland():
    s = Scene("highland", "Highland", 22, "#4c4a36"); rng = s.rng
    th = rng.uniform(0, 2 * math.pi)
    # two grey slate slabs pushing up through the turf, stepped laminations + a split
    for k, (d, r, h1, h2) in enumerate(((7.0, 4.6, 1.4, 2.5), (8.5, 3.0, 1.0, 1.8))):
        a = th + k * 2.3; p = stone(d * math.cos(a), d * math.sin(a), r, rng, .5, 5)
        b = s.prism([p], H + h1, "slate")
        if not b: continue
        q = affinity.translate(affinity.scale(b[0], .72, .8), rng.uniform(-.7, .7), rng.uniform(-.7, .7))
        u = s.prism(polys_of(q.intersection(inset(b[0], 0.4))), H + h2, "slate", z0=H + h1 - 0.05)
        top = u[0] if u else b[0]; c = top.centroid; aa = a + rng.uniform(.6, 1.2)
        ln = LineString([(c.x - 8 * math.cos(aa), c.y - 8 * math.sin(aa)), (c.x + 8 * math.cos(aa), c.y + 8 * math.sin(aa))])
        s.tcut([ln.buffer(0.25, cap_style=2).intersection(b[0])], H + h1 - 0.45, H + 3.0, "crack")
    # small slate chips
    for p in s.scatter(4, 1.0, 1.6, gap=0.4, round_=False):
        s.prism([p], H + 0.7, "slate")
    # grass tussocks in clumps (spiky stars, two tiers) + yellow flower clusters
    tus = []
    for c in range(5):
        a = th + math.pi + (c - 2) * 1.0; d = rng.uniform(3, 10); x, y = d * math.cos(a), d * math.sin(a)
        for _ in range(14):
            ro = rng.uniform(1.9, 2.8); p = star(x + rng.uniform(-3, 3), y + rng.uniform(-3, 3), ro, ro * .55, 7, rng.uniform(0, 1))
            if not p.buffer(0.3).intersects(unary_union(s.feet)) and all(not p.buffer(0.3).intersects(q) or p.intersection(q).area > 1.0 for q in tus):
                tus.append(p)
    profs, _ = s.stack([unary_union(tus)] if tus else [], 1.5, ((0.55, 28), (0.45, 50)), "grass")
    for c in range(3):
        a = rng.uniform(0, 2 * math.pi); d = rng.uniform(2, 11); x, y = d * math.cos(a), d * math.sin(a)
        fl = [Point(x + rng.uniform(-1.1, 1.1), y + rng.uniform(-1.1, 1.1)).buffer(.5, 8) for _ in range(6)]
        fl = unary_union(fl)
        if not fl.buffer(0.3).intersects(unary_union(s.feet)): s.stack([fl], 0.55, ((1.0, 30),), "flower")
    s.stack(s.scatter(22, 0.5, 0.75, gap=0.35), 0.35, ((0.7, 10), (0.3, 45)), "grit")
    return s

def maahl():
    s = Scene("maahl", "Deserts of Maahl", 33, "#c9a36c"); rng = s.rng
    th = rng.uniform(0, 2 * math.pi)
    # wind-eroded sandstone outcrop (rounded tiers) + a smaller one
    for d, r, h, a in ((8.0, 3.6, 2.7, th), (9.0, 2.2, 1.6, th + 2.6)):
        cx, cy = d * math.cos(a), d * math.sin(a)
        g = unary_union([Point(cx + rng.uniform(-r, r) * .6, cy + rng.uniform(-r, r) * .6).buffer(r * rng.uniform(.5, .8), 12) for _ in range(4)])
        g = g.buffer(0.8).buffer(-0.8).simplify(0.05)
        s.stack([g], h, ((0.35, 8), (0.25, 20), (0.25, 38), (0.15, 60)), "sandstone")
    # bubbly pebble clusters
    clusters = []
    for c in range(3):
        a = rng.uniform(0, 2 * math.pi); d = rng.uniform(1, 11); x, y = d * math.cos(a), d * math.sin(a)
        reg = Point(x, y).buffer(2.4)
        if reg.intersects(unary_union(s.feet).buffer(0.5)): continue
        peb = s.scatter(9, 0.5, 0.75, region=reg, gap=0.2, tries=2000)
        if peb: s.stack(peb, 0.5, ((0.6, 15), (0.4, 50)), "pebble"); clusters.append(reg.buffer(0.3))
    # wind ripples across the whole floor, stopping against rocks and pebble patches
    phi = rng.uniform(0, math.pi); f = frame(0, 0, phi); rip = []
    for t in np.arange(-17, 17, 2.5):
        pts = [f(u, t + 0.6 * math.sin(u * 0.33 + 0.12 * t) + 0.22 * math.sin(u * 0.9 + 0.25 * t)) for u in np.linspace(-18, 18, 60)]
        rip.append(LineString(pts).simplify(0.12).buffer(0.55, cap_style=1, resolution=3))
    block = unary_union(s.feet + clusters).buffer(0.5)
    # ripples = ground left standing; the troughs between them are cut (sand kept flat 0.5 mm round rocks)
    tr = Point(0, 0).buffer(CLIP, 256).difference(unary_union(rip)).difference(block)
    s.gcut([g for g in polys_of(tr) if g.area > 0.3], 0.45, "trough")
    return s

def corridor():
    s = Scene("corridor", "Spaceship Corridor", 44, "#2f3540"); rng = s.rng
    phi = rng.uniform(0, math.pi); f = frame(0, 0, phi); off = rng.uniform(-2, 2)
    R = lambda u0, u1, v0, v1: Polygon([f(u0, v0), f(u1, v0), f(u1, v1), f(u0, v1)])
    # central grated walkway edge to edge: rails + rungs, deep slots between rungs
    walk = R(-20, 20, off - 4.2, off + 4.2)
    s.prism([R(-20, 20, off - 4.2, off - 3.1), R(-20, 20, off + 3.1, off + 4.2)], H + 0.9, "rail")
    s.prism([R(u, u + 0.95, off - 3.4, off + 3.4) for u in np.arange(-19, 19, 1.75)], H + 0.55, "metal", z0=H - 0.8, foot=False)
    s.gcut([R(-20, 20, off - 3.15, off + 3.15)], 0.6, "slot")
    # conduit along one side, edge to edge, with collars
    pv = off + rng.choice([-1, 1]) * 9.6
    pipe = R(-20, 20, pv - 1.15, pv + 1.15)
    s.stack([pipe], 1.5, ((0.45, 6), (0.35, 30), (0.2, 55)), "pipe")
    s.stack([R(u, u + 1.3, pv - 1.6, pv + 1.6) for u in np.arange(-15, 16, 6.5)], 1.9, ((0.6, 6), (0.4, 40)), "pipe")
    # octagon deck plates on both sides, seams between, vents + rivets
    P, c = 6.0, 1.7; octs = []
    for i in range(-4, 5):
        for j in range(-4, 5):
            u, v = i * P, off + 4.8 + 2.75 + j * P
            a = 2.75
            o = Polygon([f(u + x, v + y) for x, y in ((-a + c, -a), (a - c, -a), (a, -a + c), (a, a - c), (a - c, a), (-a + c, a), (-a, a - c), (-a, -a + c))])
            octs.append((o, (u, v)))
    keep = unary_union([walk.buffer(0.5), pipe.buffer(0.5)])
    plates = []
    for o, (u, v) in octs:
        for g in polys_of(o.difference(keep).intersection(s.disc)):
            if wide_enough(g) and g.area > 1.2: plates.append((g, (u, v), abs(g.area - o.area) < 0.01))
    s.prism([g for g, _, _ in plates], H + 0.5, "plate")
    vents, treads = [], []
    for g, (u, v), whole in plates:
        if not whole: continue
        k = rng.random()
        if k < 0.35: vents += [R(u - 1.6, u + 1.6, v + w, v + w + 0.5) for w in (-1.45, -0.45, 0.55)]
        elif k < 0.6: treads += [Point(*f(u + x, v + y)).buffer(0.47, 8) for x in (-1.3, 0, 1.3) for y in (-1.3, 0, 1.3)]
    if vents: s.tcut(vents, H + 0.1, H + 1.0, "slot")
    if treads: s.prism(treads, H + 0.8, "metal", z0=H + 0.3)
    # rivets on the deck diamonds between plates
    riv = []
    for i in range(-4, 5):
        for j in range(-4, 5):
            u, v = i * P + P / 2, off + 4.8 + 2.75 + j * P + P / 2
            p = Point(*f(u, v)).buffer(0.5, 8)
            if not p.buffer(0.2).intersects(keep) and Point(0, 0).buffer(R0 - 1.0).contains(p) and not any(p.intersects(g) for g, _, _ in plates): riv.append(p)
    s.prism(riv, H + 0.4, "metal")
    return s

def infestation():
    s = Scene("infestation", "Alien Infestation", 55, "#2b3140"); rng = s.rng
    th = rng.uniform(0, 2 * math.pi); bx, by = 8.0 * math.cos(th), 8.0 * math.sin(th)
    mass = unary_union([Point(bx + rng.uniform(-3, 3), by + rng.uniform(-3, 3)).buffer(rng.uniform(2.0, 3.4), 12) for _ in range(6)])
    m2 = unary_union([Point(-bx * 1.15 + rng.uniform(-2, 2), -by * 1.15 + rng.uniform(-2, 2)).buffer(rng.uniform(1.8, 2.6), 12) for _ in range(3)])
    mass = unary_union([mass, m2]).buffer(1.0).buffer(-1.0).simplify(0.05)
    # tendrils creeping out of the mass over the deck, each ends in a bulb
    tend = []
    for k in range(5):
        a = th + math.pi + rng.uniform(-1.3, 1.3); x, y = bx + 2.5 * math.cos(a), by + 2.5 * math.sin(a)
        pts = [(x, y)]; L = rng.uniform(7, 12)
        for i in range(8):
            a += rng.uniform(-.5, .5); x += L / 8 * math.cos(a); y += L / 8 * math.sin(a); pts.append((x, y))
        segs = [LineString(pts[i:i + 2]).buffer(0.75 - 0.03 * i, cap_style=1, resolution=6) for i in range(8)]
        tend += segs + [Point(*pts[-1]).buffer(0.85, 10)]
    tend = unary_union(tend).difference(mass.buffer(-0.6))
    # hex deck plates (removed under the mass, which grew over them)
    a6 = 3.0; plates = []
    for i in range(-5, 6):
        for j in range(-5, 6):
            x, y = i * a6 * 1.5, (j + (0.5 if i % 2 else 0)) * a6 * math.sqrt(3)
            h = Polygon([(x + (a6 - 0.3) * math.cos(k * math.pi / 3), y + (a6 - 0.3) * math.sin(k * math.pi / 3)) for k in range(6)])
            g = [q for q in polys_of(h.difference(mass.buffer(0.3)).intersection(s.disc)) if wide_enough(q) and q.area > 1.2]
            plates += [(q, (x, y), abs(q.area - h.area) < 0.01) for q in g]
    s.prism([q for q, _, _ in plates], H + 0.45, "plate")
    slots = []
    for q, (x, y), whole in plates:
        if whole and rng.random() < 0.3 and not q.intersects(tend):
            slots += [box(x - 1.4, y + w, x + 1.4, y + w + 0.5) for w in (-1.0, 0.0, 1.0)]
    if slots: s.tcut(slots, H + 0.05, H + 1.0, "slot")
    t1 = s.prism([g for g in polys_of(tend) if g.area > 0.5], H + 0.75, "flesh")
    s.prism([g for p in t1 for g in polys_of(inset(p, 0.3))], H + 1.15, "flesh", z0=H + 0.7)
    profs, ztop = s.stack(polys_of(mass), 2.3, ((0.4, 14), (0.35, 32), (0.25, 52)), "flesh")
    # pustules on the mass and a few egg sacs on the deck
    pus = []
    big = max(profs, key=lambda p: p.area) if profs else None
    if big is not None:
        for _ in range(40):
            if len(pus) >= 3: break
            x0, y0, x1, y1 = big.bounds; p = Point(rng.uniform(x0, x1), rng.uniform(y0, y1)).buffer(rng.uniform(.55, .75), 10)
            if big.buffer(-0.1).contains(p) and all(p.distance(q) > .3 for q in pus): pus.append(p)
        if pus: s.stack(pus, 0.6, ((0.6, 20), (0.4, 55)), "pustule", z0=ztop - 0.05 + SINK)
    return s

def urban():
    s = Scene("urban", "Urban Warfare", 66, "#7e7b76"); rng = s.rng
    phi = rng.uniform(0, math.pi); f = frame(0, 0, phi); d = rng.uniform(-3, 0)
    R = lambda u0, u1, v0, v1: Polygon([f(u0, v0), f(u1, v0), f(u1, v1), f(u0, v1)])
    side = R(-20, 20, d + 1.5, 20)
    # curb stones edge to edge
    curbs = [R(u, u + 4.0, d, d + 1.8) for u in np.arange(-19.5, 19, 4.5)]
    s.stack(curbs, 1.0, ((0.8, 3), (0.2, 40)), "curb")
    # sidewalk slab + herringbone pavers (pattern ends at the curb and the rim)
    slab = s.prism([side.intersection(Point(0, 0).buffer(R0 + 0.15, 200))], H + 0.75, "slab")
    w = 1.55; bricks = []
    for k in range(-16, 17):
        for m in range(-8, 9):
            ox, oy = k * w - m * 2 * w, k * w + m * 2 * w
            bricks += [box(ox, oy, ox + 2 * w, oy + w), box(ox + 2 * w, oy - w, ox + 3 * w, oy + w)]
    sl = unary_union(slab) if slab else Polygon()
    near = sl.buffer(2)
    joints = unary_union([affinity.rotate(b, math.degrees(phi) + 45, origin=(0, 0)).exterior.buffer(0.22, cap_style=2, join_style=2)
                          for b in bricks if affinity.rotate(b, math.degrees(phi) + 45, origin=(0, 0)).intersects(near)])
    # herringbone joints cut into the slab; a plain edging strip runs along the curb
    s.tcut([joints.intersection(sl.difference(R(-20, 20, d - 0.5, d + 2.3)))], H + 0.35, H + 0.75, "slot")
    # storm drain against the curb
    u0 = rng.uniform(-6, 2)
    s.prism([R(u0, u0 + 6.0, d - 2.7, d + 0.1)], H + 0.3, "metal")
    s.tcut([R(u0 + 0.8 + i * 1.1, u0 + 1.3 + i * 1.1, d - 2.1, d - 0.5) for i in range(4)], H - 0.5, H + 1.0, "slot")
    # broken concrete chunks on the road
    road = R(-20, 20, -20, d - 0.3)
    for p in s.scatter(4, 1.1, 1.9, region=road, gap=0.5, round_=False):
        s.stack([p], rng.uniform(0.8, 1.2), ((0.6, 4), (0.4, 30)), "rubble")
    # pothole + asphalt cracks: cracks start at the curb and end in the pothole / at the rim
    for _ in range(50):
        px, py = f(rng.uniform(-8, 8), d - rng.uniform(5, 9))
        hole = stone(px, py, 2.2, rng, .8, 8)
        if Point(0, 0).buffer(R0 - 1.2).contains(hole) and not hole.buffer(0.6).intersects(unary_union(s.feet)): break
    s.gcut([hole], 0.7, "pothole")
    crk = []
    for k in range(2):
        x, y = f(rng.uniform(-8, 8), d); tx, ty = (px, py) if k == 0 else f(rng.uniform(-14, 14), d - 30)
        pts = [(x, y)]
        for i in range(1, 9):
            t = i / 8; pts.append((x + (tx - x) * t + rng.uniform(-.8, .8), y + (ty - y) * t + rng.uniform(-.8, .8)))
        pts[-1] = (tx, ty)
        crk.append(LineString(pts).buffer(0.22, cap_style=1, resolution=4))
    s.gcut([unary_union(crk).intersection(road.buffer(0.3))], 0.45, "crack")
    # asphalt aggregate
    s.stack(s.scatter(35, 0.47, 0.62, region=road.difference(hole.buffer(0.3)), gap=0.35), 0.25, ((1.0, 20),), "grit")
    return s

STYLES = {"badlands": badlands, "highland": highland, "maahl": maahl, "corridor": corridor,
          "infestation": infestation, "urban": urban}

# ======================================================================= 2D art
COL = {"rockc": "#b56a4a", "rock": "#b56a4a", "grit": "#9a7158", "slate": "#8d9399", "grass": "#6d7a3a", "flower": "#e3c22b",
       "sandstone": "#d8b47c", "pebble": "#c8a36e", "ripple": "#d7b682", "rail": "#7c8796", "metal": "#8e99a8",
       "pipe": "#6e7c8f", "plate": "#56606e", "flesh": "#c0307a", "pustule": "#e05aa8", "curb": "#b9b6b0",
       "slab": "#9f9c96", "paver": "#b2aea6", "rubble": "#c4c0b8"}
CUTC = {"crack": "#2a1a12", "trough": "#a88a5c", "slot": "#0d1014", "pothole": "#3a3836"}

def shade(hexc, k):
    c = [max(0, min(255, int(int(hexc[i:i + 2], 16) * k))) for i in (1, 3, 5)]
    return "#%02x%02x%02x" % tuple(c)

def render(s, ax):
    import matplotlib.patches as mp
    from matplotlib.path import Path
    def pp(p):
        v, c = [], []
        for ring in [p.exterior] + list(p.interiors):
            xy = list(ring.coords); v += xy; c += [Path.MOVETO] + [Path.LINETO] * (len(xy) - 2) + [Path.CLOSEPOLY]
        return Path(v, c)
    clipc = mp.Circle((0, 0), R0, transform=ax.transData)
    ax.add_patch(mp.Circle((0, 0), R0 + 0.6, color="#1d1b1a", zorder=0))
    ax.add_patch(mp.Circle((0, 0), R0, color=s.ground, zorder=0.1))
    zo = 1
    for ps, dep, kind in s.final_gcuts():
        for p in ps:
            pa = mp.PathPatch(pp(p), color=CUTC.get(kind, "#222"), lw=0, zorder=zo); pa.set_clip_path(clipc); ax.add_patch(pa)
    for op in s.ops:
        zo += 0.01
        if op[0] == "prism": _, ps, z0, top, kind = op; tier = z0 > H
        else: _, ps, z0, hh, a, kind = op; top = z0 + hh; tier = z0 > H - SINK + 0.01
        k = 0.78 + 0.13 * (top - H)
        for p in ps:
            if not tier:
                sh = affinity.translate(p, .2 + .12 * (top - H), -.2 - .12 * (top - H))
                pa = mp.PathPatch(pp(sh), color="black", alpha=.35, lw=0, zorder=zo - .005); pa.set_clip_path(clipc); ax.add_patch(pa)
            pa = mp.PathPatch(pp(p), facecolor=shade(COL[kind], k), edgecolor=shade(COL[kind], .55), lw=.3, zorder=zo)
            pa.set_clip_path(clipc); ax.add_patch(pa)
    for ps, z0, z1, kind in s.tcuts:
        for p in ps:
            pa = mp.PathPatch(pp(p), color=CUTC.get(kind, "#111"), lw=0, zorder=zo + 1); pa.set_clip_path(clipc); ax.add_patch(pa)
    ax.set_xlim(-R0 - 1, R0 + 1); ax.set_ylim(-R0 - 1, R0 + 1); ax.set_aspect("equal"); ax.axis("off")
    ax.set_title(f"{s.title}  (tallest {s.top():.1f} mm)", fontsize=11)

def art(path):
    import matplotlib; matplotlib.use("Agg"); import matplotlib.pyplot as plt
    fig, axs = plt.subplots(2, 3, figsize=(13, 9))
    for ax, k in zip(axs.flat, STYLES): render(STYLES[k](), ax)
    fig.suptitle("30 mm bases in the Gamers Grass styles (top view; dark = cut in, light/shadowed = raised)", fontsize=12)
    plt.tight_layout(); fig.savefig(path, dpi=110)

# ======================================================================= SolidCraft
def loops(p):
    yield [[round(x, 4), round(y, 4)] for x, y in list(p.exterior.coords)[:-1]]
    for r in p.interiors: yield [[round(x, 4), round(y, 4)] for x, y in list(r.coords)[:-1]]

SKETCH_TOL = CFG.get("sketch_tolerance_mm", 0.03)   # drop outline points that change the shape by less than this

def _arc_loop(coords, rim):
    """Turn a closed outline into line + true-arc segments: runs of 3+ vertices lying on the rim circle (radius `rim`)
    become one arc each, so the edge is a real curve instead of a chain of flat facets. Returns commands or None."""
    if os.environ.get("NOARC"): return None
    pts = list(coords)[:-1]; n = len(pts)
    on = [abs(math.hypot(x, y) - rim) < 0.006 for x, y in pts]
    if n < 4 or all(on) or not any(on): return None
    k0 = next(i for i in range(n) if not on[i])                  # start at a vertex that is not on the rim
    pts = pts[k0:] + pts[:k0]; on = on[k0:] + on[:k0]
    seg = []; i = 0                                              # ("line", [pts]) or ("arc", start, through, end)
    cur = []
    while i < n:
        if on[i]:
            j = i
            while j + 1 < n and on[j + 1]: j += 1
            if j - i >= 2:
                def proj(q): r = math.hypot(*q) or 1; return (q[0] * rim / r, q[1] * rim / r)
                a, m, b = proj(pts[i]), proj(pts[(i + j) // 2]), proj(pts[j])
                if cur: cur.append(a); seg.append(("line", cur)); cur = []
                else: seg.append(("line", [seg[-1][3] if seg and seg[-1][0] == "arc" else a, a]))
                seg.append(("arc", a, m, b)); cur = [b]
                i = j + 1; continue
            cur += pts[i:j + 1]; i = j + 1; continue
        cur.append(pts[i]); i += 1
    if not any(x[0] == "arc" for x in seg): return None
    if cur:
        cur.append(seg[0][1][0] if seg[0][0] == "line" else seg[0][1])
        seg.append(("line", cur))
    out = []
    for s in seg:
        if s[0] == "line":
            ln = LineString(s[1]).simplify(SKETCH_TOL) if SKETCH_TOL else LineString(s[1])
            pp = [[round(x, 4), round(y, 4)] for x, y in ln.coords]
            if len(pp) >= 2 and pp[0] != pp[-1]: out.append({"command": "sketch.line", "params": {"points": pp, "closed": False}})
        else:
            f = lambda q: [round(q[0], 4), round(q[1], 4)]
            out.append({"command": "sketch.arc.three_point", "params": {"start": f(s[1]), "through": f(s[2]), "end": f(s[3])}})
    return out

def sk(name, z, polys, rim=None):
    c = [{"command": "sketch.create", "params": {"plane": "XY", "offset": round(z, 4), "name": name}}]
    for p in polys:
        if p.area < 0.01 or len(set(p.exterior.coords)) < 4: continue      # slivers leave stray sketch stubs
        if rim and not p.interiors:
            a = _arc_loop(p.exterior.coords, rim)
            if a: c += a; continue
        q = p.simplify(SKETCH_TOL, preserve_topology=True) if SKETCH_TOL else p
        if q.is_empty or q.geom_type != "Polygon": q = p
        for l in loops(q): c.append({"command": "sketch.line", "params": {"points": l, "closed": True}})
    return c + [{"command": "sketch.finish", "params": {}}]

def magnet_centres(dia=None):
    """where the 6x2 mm magnet pockets go, by base size (the project's base spec): up to 40 mm one in the middle;
    over 40 up to 60 mm two; over 60 up to 100 mm three; over 100 mm five (four round and one in the middle). Several
    pockets sit evenly round a circle a quarter of the base diameter out from the middle."""
    dia = BASE_MM if dia is None else dia
    n = 1 if dia <= 40 else 2 if dia <= 60 else 3 if dia <= 100 else 4
    if n <= 1: return [(0.0, 0.0)]
    r = dia * 0.25
    return [(round(r * math.cos(math.pi / 2 + 2 * math.pi * i / n), 4), round(r * math.sin(math.pi / 2 + 2 * math.pi * i / n), 4)) for i in range(n)] + ([(0.0, 0.0)] if dia > 100 else [])

def mag_sketch():
    """one sketch with a true circle per magnet pocket (6.2 mm across; the cut makes them 2.1 mm deep)"""
    return ([{"command": "sketch.create", "params": {"plane": "XY", "offset": 2.1, "name": "Mag"}}]
            + [{"command": "sketch.circle.center", "params": {"center": [x, y], "radius": 3.1}} for x, y in magnet_centres()]
            + [{"command": "sketch.finish", "params": {}}])

def sk_circle(name, z, r):
    """A true circle (one curved face instead of hundreds of flat ones)."""
    return [{"command": "sketch.create", "params": {"plane": "XY", "offset": round(z, 4), "name": name}},
            {"command": "sketch.circle.center", "params": {"center": [0, 0], "radius": round(r, 4)}},
            {"command": "sketch.finish", "params": {}}]

def piece_group(body, sketch_cmds, distance, top, d, fallback=(), poly=None):
    """a piece with a chamfered top, as its own small body, with no chamfer command at all: a straight extrude up to
    (top - d), then a 45 degree tapered cap of height d on the same outline, joined into the same body. That is the
    same shape as an equal chamfer of d, and takes SolidCraft a few hundredths of a second instead of seconds."""
    sk_name = sketch_cmds[0]["params"]["name"]; z0 = sketch_cmds[0]["params"]["offset"]
    if poly is not None:
        while d >= 0.05 and (inset(poly, d + 0.06).is_empty or inset(poly, d + 0.06).area < 0.25 * poly.area): d *= 0.6     # cap must leave a real top
    if poly is None or d < 0.05 or distance - d < 0.1:       # too small for a cap: plain piece
        grp = Group(sketch_cmds + [{"command": "solid.extrude", "params": {"sketch": sk_name, "distance": round(distance, 4), "operation": "new", "name": "F" + body, "body_name": body}}])
    else:
        e = CAP_LAP            # the cap starts a hair inside the straight part, so the two overlap instead of only touching
        cap = [dict(c) for c in sketch_cmds]; cap[0] = {"command": "sketch.create", "params": dict(sketch_cmds[0]["params"], name=sk_name + "c", offset=round(top - d - e, 4))}
        grp = Group(sketch_cmds + [{"command": "solid.extrude", "params": {"sketch": sk_name, "distance": round(distance - d, 4), "operation": "new", "name": "F" + body, "body_name": body}}]
                    + cap + [{"command": "solid.extrude", "params": {"sketch": sk_name + "c", "distance": round(d + e, 4), "taper": -45.0, "operation": "join", "targets": [body], "name": "F" + body + "c"}}])
    grp.piece = {"body": body, "top": round(top, 4), "dist": []}
    grp.fallback = list(fallback)
    return grp

CAP_LAP = 0.04
DEEP = 0.3     # mm every raised piece reaches down into what it stands on

class Group(list):
    """a command group that may carry fallback groups (run if this one is refused)"""
    fallback = ()
    alts = ()
    polish = False
    piece = None

def _surface_z(s, p, z0, z1):
    """height of the plate top that a slot cuts through: the highest op top under the slot, inside the cut's span"""
    pt = p.representative_point(); best = None
    for op in s.ops:
        if op[-1] == "rockc": continue
        top = op[3] if op[0] == "prism" else op[2] + op[3]
        if z0 + 0.05 < top <= z1 + 1e-6 and any(q.contains(pt) for q in op[1]) and (best is None or top > best): best = top
    return best

def _wall_r(z):
    """radius of the finished 12 deg wall at height z"""
    import math
    return R0 - max(z, 0) * math.tan(math.radians(RIM))

RIM_K = {}          # id(op) -> its own rim slot, so no two pieces end on exactly the same rim arc

def _rim(z, k=0):
    """radius that detail is clipped to at height z; each raised op (k >= 1) gets its own inset so walls never coincide"""
    return _wall_r(z) - float(CFG.get("rim_gap", 0.02)) - 0.03 * k

def _clip(polys, z, need=0.0, k=0):
    """keep detail inside the finished wall at height z (a hair inside so no faces coincide)"""
    disc = Point(0, 0).buffer(_rim(z, k), 96)
    out = []
    for p in polys:
        q = p.intersection(disc)
        out += [x for x in polys_of(q) if x.area > 1e-3 and (need <= 0 or not x.buffer(-(need + 0.03)).is_empty)]
    return out

def op_k(op):
    return RIM_K.get(id(op), 0)

def clip_z(op):
    """height whose wall radius a piece is clipped to: straight pieces at their top (so they never overhang the 12 deg
    wall), tapered ones at their base (the taper keeps them inside)"""
    return max(op[3], H) if op[0] == "prism" else max(op[2], H)

def op_clip(op):
    """clipped footprint of a raised op; tapered ops drop rim slivers that the taper would close"""
    need = 0.0 if op[0] == "prism" else op[3] * math.tan(math.radians(op[4]))
    return _clip(op[1], clip_z(op), need, op_k(op))

class LK(str):
    """a material name that also says: this piece is one step of a rounded shape. .loft = (chain number, height at
    this piece's outline). The mesh builder slopes from each piece of a chain to the next. Two tagged names are
    only equal when they are the same object, so pieces of different shapes are never grouped into one."""
    __slots__ = ("loft", "ghost")
    def __eq__(s, o): return s is o if isinstance(o, LK) else str.__eq__(s, o)
    def __ne__(s, o): return not s.__eq__(o)
    __hash__ = str.__hash__

CHAMFER_PRISMS = {"slate", "bedrock", "rock", "brick", "concrete", "sandstone", "rubble"}

PIECES = {"rock", "pebble", "grit", "rubble", "crate", "pustule", "obelisk", "flower", "spire", "quartz", "lens", "post"}

def _prism_groups(i, op):
    """a rocky slab (slate, bedrock, brick...): one solid per piece with a chamfered top edge; smaller chamfer if
    refused, plain slab as the last resort. Pieces cut by the rim stay plain."""
    out = []
    vis = op[3] - max(op[2] + 0.15, H)
    rk = op_k(op)
    for k, p in enumerate(_clip(op[1], clip_z(op), 0.0, rk)):
        if p.area < 0.01: continue
        plain = sk(f"P{i}_{k}", op[2] - DEEP, [p], rim=_rim(clip_z(op), rk)) + [{"command": "solid.extrude", "params": {"sketch": f"P{i}_{k}", "distance": round(op[3] - op[2] + DEEP, 4), "operation": "join", "targets": ["Base"], "name": f"FP{i}_{k}"}}]
        at_rim = max(math.hypot(*c) for c in p.exterior.coords) > _rim(clip_z(op), rk) - 0.01
        if vis < 0.6 or p.area < 0.8 or at_rim:      # low slabs (bricks, paint) stay square: tiny chamfers there break the STL
            out.append(Group(plain)); continue
        d = round(min(0.3, 0.35 * vis), 4)
        grp = piece_group(f"PP{i}_{k}", sk(f"P{i}_{k}", op[2] - DEEP, [p], rim=_rim(clip_z(op), rk)), op[3] - op[2] + DEEP, op[3], d, [plain], poly=p)
        out.append(grp)
    return out

def _prune(s):
    """Loose pieces (rocks, pebbles, crates...) must sit wholly on the top of the base: drop any that reach the rim,
    sit over a ground cut (crack, leak, pit...) or get cut by a top cut, together with any tiers stacked on them.
    A chamfered rock that touches another raised piece is built from its tiers instead (the chamfer would clash)."""
    gc = [p for ps, _, _ in s.final_gcuts() for p in ps]
    gcu = unary_union(gc).buffer(0.1) if gc else Polygon()
    def bad(p, kind, z0, top):
        if kind not in PIECES or p.area >= 12: return False
        if not Point(0, 0).buffer(_wall_r(max(top, H)) - 0.3, 64).contains(p): return True
        if z0 <= H + 0.05 and p.intersects(gcu): return True
        return any(b > z0 + 0.05 and a < z0 + 0.05 and q.intersects(p) for ps, a, b, k in s.tcuts for q in ps)     # a cut in the floor under it; a pattern cut into the piece's own top is fine
    dropped = []
    keep_ch = []
    for c in getattr(s, "chops", []):
        if bad(c["poly"], c["kind"], c["z0"], c["top"]): dropped.append((c["kind"], c["poly"]))
        else: keep_ch.append(c)
    new = []
    for op in s.ops:
        kind = op[-1]
        if kind == "rockc": new.append(op); continue
        z0 = op[2]; top = op[3] if op[0] == "prism" else op[2] + op[3]
        ok = []
        for p in op[1]:
            pt = p.representative_point()
            if kind in PIECES and any(d.contains(pt) for k, d in dropped): continue
            if bad(p, kind, z0, top): dropped.append((kind, p)); continue
            ok.append(p)
        if ok: new.append((op[0], ok) + tuple(op[2:]))
    s.ops = new
    raised = [q for op in s.ops if op[-1] not in ("rockc", "grit") for q in op[1]]
    for c in keep_ch:
        c["iso"] = not any(c["poly"].buffer(0.05).intersects(q) for q in raised) and \
                   not any(c is not d and c["poly"].buffer(0.05).intersects(d["poly"]) for d in keep_ch)
    s.chops = keep_ch

def _coincide(a, b, tol=0.005):
    """True if the two outlines share a stretch of edge, or a corner of one sits on the other's outline"""
    bi = a.boundary.intersection(b.boundary)
    if not bi.is_empty and bi.length > 1e-3: return True
    for p, q in ((a, b), (b, a)):
        qb = q.boundary
        for c in p.exterior.coords:
            if qb.distance(Point(c)) < tol: return True
    return False

def _separate(s, gap=0.12, touch=0.08, sliver=0.02):
    """Pieces that only just touch (an edge, a corner, or a sliver of overlap) give the STL edges shared by more than
    two faces, which is not watertight. Pull every such pair apart by `gap` (the smaller piece gives way); pieces that
    really overlap are left alone (they fuse cleanly)."""
    from shapely.strtree import STRtree
    items = []                                     # [where, index, polygon]
    for i, op in enumerate(s.ops):
        if op[-1] == "rockc": continue
        for j, p in enumerate(op[1]): items.append([("op", i), j, p])
    for i, c in enumerate(getattr(s, "chops", [])): items.append([("ch", i), 0, c["poly"]])
    for i, (ps, d, k) in enumerate(s.gcuts):
        for j, p in enumerate(ps): items.append([("gc", i), j, p])
    if len(items) < 2: return 0
    geoms = [it[2] for it in items]; tree = STRtree(geoms); fixed = 0
    for a in range(len(items)):
        pa = items[a][2]
        for b in tree.query(pa.buffer(touch)):
            b = int(b)
            if b <= a or items[b][0] == items[a][0]: continue
            if all(w[0] == "op" and getattr(s.ops[w[1]][-1], "loft", None) is not None for w in (items[a][0], items[b][0])): continue      # steps of rounded shapes are nested on purpose
            pb = items[b][2]
            if pa.is_empty or pb.is_empty or pa.distance(pb) >= touch: continue
            lo, hi = (a, b) if pa.area <= pb.area else (b, a)
            if pa.intersection(pb).area >= sliver:                   # a real overlap: fine unless outlines coincide
                if not _coincide(pa, pb): continue
                q = items[lo][2].buffer(-gap, join_style=2)          # move the smaller piece's walls off the shared line
                if q.is_empty or q.area < 0.3: continue
                q = max(polys_of(q), key=lambda x: x.area)
                items[lo][2] = q; pa = items[a][2]; fixed += 1; continue
            q = items[lo][2].difference(items[hi][2].buffer(gap, 8))
            q = max(polys_of(q), key=lambda x: x.area) if not q.is_empty else q
            items[lo][2] = q; pa = items[a][2]; fixed += 1
    ops = [list(op) for op in s.ops]; gcs = [list(x) for x in s.gcuts]
    for (kind, i), j, p in items:
        if kind == "op": ops[i][1] = list(ops[i][1]); ops[i][1][j] = p
        elif kind == "gc": gcs[i][0] = list(gcs[i][0]); gcs[i][0][j] = p
        else: s.chops[i]["poly"] = p
    s.ops = [(op[0], [p for p in op[1] if not p.is_empty and p.area > 0.05]) + tuple(op[2:]) for op in ops]
    s.ops = [op for op in s.ops if op[1]]
    s.gcuts = [(ps2, d, k) for ps, d, k in gcs for ps2 in [[p for p in ps if not p.is_empty and p.area > 0.05]] if ps2]
    s.chops = [c for c in getattr(s, "chops", []) if not c["poly"].is_empty and c["poly"].area > 0.5]
    return fixed

def _group_ops(s, close=0.06):
    """Pieces of the same type, base height, top and taper become ONE extrude (fewer bodies, fewer booleans), and
    same-group pieces closer than 2*close are fused into one outline so they never meet along a hairline."""
    if not CFG.get("group_pieces", True): return
    groups = {}; order = []
    for op in s.ops:
        if op[-1] == "rockc": key = ("keep", id(op))
        else: key = (op[0], round(op[2], 3), round(op[3], 3), round(op[4], 3) if op[0] == "taper" else 0, op[-1])
        if key not in groups: groups[key] = []; order.append(key)
        groups[key].append(op)
    new = []
    for key in order:
        ops = groups[key]
        if key[0] == "keep" or len(ops) == 1 and len(ops[0][1]) == 1: new += ops; continue
        polys = [p for op in ops for p in op[1]]
        fused = unary_union([p.buffer(close, join_style=2, mitre_limit=3) for p in polys]).buffer(-close, join_style=2, mitre_limit=3)
        ps = [q for q in polys_of(fused) if q.area > 0.05]
        if not ps: continue
        if key[0] == "taper": ps = [q for q in (fit_for_taper(p, ops[0][3] * math.tan(math.radians(ops[0][4])) + 0.03) for p in ps) if q is not None] or ps
        new.append((ops[0][0], ps) + tuple(ops[0][2:]))
    s.ops = new

def build_cmds(s):
    """BASE FIRST: finished 30 mm base (12 deg wall) with the magnet pocket, then ground cuts, raised ops, top cuts.
    Detail is clipped to the wall instead of trimmed afterwards, so there is no final boolean over the whole model."""
    if not CFG.get("base_first", True): return build_cmds_trim_last(s)
    _prune(s)
    _group_ops(s)
    _separate(s)
    RIM_K.clear()
    for k, op in enumerate(s.ops): RIM_K[id(op)] = 1 + k % 6      # 0.03-0.18 mm in from the rim, different per piece
    G = [sk_circle("Blank", 0, R0) + [{"command": "solid.extrude", "params": {
        "sketch": "Blank", "distance": H, "taper": -RIM, "operation": "new", "name": "Base", "body_name": "Base"}}]]
    G.append(mag_sketch() + [{"command": "solid.extrude", "params": {
        "sketch": "Mag", "distance": 2.2, "direction": "negative", "operation": "cut", "targets": ["Base"], "name": "MagCut"}}])
    for i, (ps, dep, kind) in enumerate(s.final_gcuts()):
        ps = _clip(ps, H)
        if not ps: continue
        for k, part in enumerate([[p] for p in ps] if CFG.get("split_cuts", True) and 1 < len(ps) <= 12 else [ps]):
            j = (i * 7 + k) % 23 + 1                 # every cut tool its own top and floor: no two tools share a face
            zt = H + 0.2 + 0.004 * j; dd = round(dep + (zt - H) + 0.003 * j, 4)
            G.append(sk(f"G{i}_{k}", zt, part, rim=_rim(H)) + [{"command": "solid.extrude", "params": {"sketch": f"G{i}_{k}", "distance": dd,
                     "direction": "negative", "operation": "cut", "targets": ["Base"], "name": f"FG{i}_{k}"}}])
    for i, op in enumerate(s.ops):
        if op[-1] == "rockc": continue
        lf = getattr(op[-1], "loft", None)
        if getattr(op[-1], "ghost", None) is not None: continue          # holds the place of true 3D solids that the mesh builder adds
        if op[0] == "prism" and lf is None and op[-1] in CHAMFER_PRISMS and CFG.get("chamfer_rocks", True):
            G += _prism_groups(i, op); continue
        polys = op_clip(op)
        if not polys: continue
        z0 = op[2] - DEEP                          # sink every piece into what it stands on: overlap, never just touch
        if op[0] == "prism":
            e = {"sketch": f"J{i}", "distance": round(op[3] - z0, 4), "operation": "join", "targets": ["Base"], "name": f"FJ{i}"}
            if lf is not None: e["loft"] = [lf[0], round(lf[1], 4)]          # read by the mesh builder only
        else:
            grow = DEEP * math.tan(math.radians(op[4]))
            polys = [q for p in polys for q in polys_of(p.buffer(grow, join_style=2, mitre_limit=3).intersection(Point(0, 0).buffer(_rim(max(z0, H), op_k(op)), 96)))] if grow > 1e-3 else polys
            if not polys: continue
            e = {"sketch": f"J{i}", "distance": round(op[3] + DEEP, 4), "taper": -op[4], "operation": "join", "targets": ["Base"], "name": f"FJ{i}"}
        tol = SKETCH_TOL
        if lf is not None: globals()["SKETCH_TOL"] = 0          # the nested outlines of a rounded shape are drawn exactly: simplifying two of them separately could lay one on the other
        try: G.append(sk(f"J{i}", z0, polys, rim=_rim(clip_z(op), op_k(op)) if op[0] == "prism" else None) + [{"command": "solid.extrude", "params": e}])
        finally: globals()["SKETCH_TOL"] = tol
    for i, c in enumerate(getattr(s, "chops", [])):
        q = _clip([c["poly"]], max(c["top"], H))
        fb = []
        for j, op in enumerate(c["ops"]):
            pp = op_clip(op)
            if not pp: continue
            e = ({"distance": round(op[3] - op[2], 4)} if op[0] == "prism" else {"distance": op[3], "taper": -op[4]})
            e.update({"sketch": f"CF{i}_{j}", "operation": "join", "targets": ["Base"], "name": f"FCF{i}_{j}"})
            fb.append(sk(f"CF{i}_{j}", op[2], pp, rim=_rim(clip_z(op), op_k(op)) if op[0] == "prism" else None) + [{"command": "solid.extrude", "params": e}])
        if len(q) != 1 or q[0].area < 0.98 * c["poly"].area or not c.get("iso", True):
            G += fb; continue                      # touches the rim or another piece: use the tiers                      # clipped by the rim: use the tiers
        p = q[0]
        grp = piece_group(f"PC{i}", sk(f"C{i}", c["z0"] - DEEP, [p]), c["top"] - c["z0"] + DEEP, c["top"], min(c["run"], c["drop"]), fb, poly=p)
        G.append(grp)
    for i, (ps, z0, z1, kind) in enumerate(s.tcuts):
        ps = _clip(ps, max(z0, H)) if ps else ps
        if not ps: continue
        if CFG.get("split_cuts", True) and 1 < len(ps) <= 12:      # a few separate outlines: one tool each: SolidCraft merges many simple tools far faster
            for k, p in enumerate(ps):
                j = (i * 7 + k) % 23 + 1
                G.append(sk(f"T{i}_{k}", z1 + 0.004 * j, [p], rim=_rim(max(z0, H))) + [{"command": "solid.extrude", "params": {"sketch": f"T{i}_{k}", "distance": round(z1 - z0 + 0.007 * j, 4),
                         "direction": "negative", "operation": "cut", "targets": ["Base"], "name": f"FT{i}_{k}"}}])
        else:
            G.append(sk(f"T{i}", z1, ps, rim=_rim(max(z0, H))) + [{"command": "solid.extrude", "params": {"sketch": f"T{i}", "distance": round(z1 - z0, 4),
                     "direction": "negative", "operation": "cut", "targets": ["Base"], "name": f"FT{i}"}}])
        if kind == "slot" and CFG.get("chamfer_grates", True):
            for p in ps:
                top = _surface_z(s, p, z0, z1)
                c = list(p.exterior.coords)
                if (top is None or p.area < 0.5 or p.interiors or len(c) - 1 > 8 or p.convex_hull.area - p.area > 0.02 * p.area
                        or min(math.dist(a, b) for a, b in zip(c, c[1:])) < 0.45): continue
                edges = [[round((a[0] + b[0]) / 2, 4), round((a[1] + b[1]) / 2, 4), round(top, 4)] for a, b in zip(c, c[1:])]
                grp = Group([{"command": "solid.chamfer", "params": {"edges": edges, "distance": 0.1}}])
                grp.polish = True
                grp.alts = [Group([{"command": "solid.chamfer", "params": {"edges": edges, "distance": 0.06}}])]
                G.append(grp)
    return G

def build_cmds_trim_last(s):
    """Blank -> ground cuts -> raised ops -> top cuts -> 12 deg rim trim -> magnet pocket. One group per op."""
    G = [sk_circle("Blank", 0, BLANK) + [{"command": "solid.extrude", "params": {
        "sketch": "Blank", "distance": H, "operation": "new", "name": "Base", "body_name": "Base"}}]]
    for i, (ps, dep, kind) in enumerate(s.final_gcuts()):
        G.append(sk(f"G{i}", H, ps) + [{"command": "solid.extrude", "params": {"sketch": f"G{i}", "distance": dep,
                 "direction": "negative", "operation": "cut", "targets": ["Base"], "name": f"FG{i}"}}])
    for i, op in enumerate(s.ops):
        if op[0] == "prism":
            e = {"sketch": f"J{i}", "distance": round(op[3] - op[2], 4), "operation": "join", "targets": ["Base"], "name": f"FJ{i}"}
        else:
            e = {"sketch": f"J{i}", "distance": op[3], "taper": -op[4], "operation": "join", "targets": ["Base"], "name": f"FJ{i}"}
        G.append(sk(f"J{i}", op[2], op[1]) + [{"command": "solid.extrude", "params": e}])
    for i, (ps, z0, z1, kind) in enumerate(s.tcuts):
        if not ps: continue
        G.append(sk(f"T{i}", z1, ps) + [{"command": "solid.extrude", "params": {"sketch": f"T{i}", "distance": round(z1 - z0, 4),
                 "direction": "negative", "operation": "cut", "targets": ["Base"], "name": f"FT{i}"}}])
    G.append((sk_circle("Trim", 0, R0) if CFG.get("true_circle_rim", True) else sk("Trim", 0, [Point(0, 0).buffer(R0, 96)])) + [
        {"command": "solid.extrude", "params": {"sketch": "Trim", "distance": 8.0, "taper": -RIM, "operation": "new", "name": "TrimF", "body_name": "Trim"}},
        {"command": "solid.combine", "params": {"target": "Base", "tools": ["Trim"], "operation": "intersect", "keep_tools": False}}])
    G.append(mag_sketch() + [{"command": "solid.extrude", "params": {
        "sketch": "Mag", "distance": 2.2, "direction": "negative", "operation": "cut", "targets": ["Base"], "name": "MagCut"}}])
    return G

def build(key):
    raise SystemExit("use run_set.py / gg3.py build")
    s = STYLES[key](); t = time.time(); S.tool("new_design", {"name": key}); skipped = []
    cnt = [0]
    def join_split(op, polys, depth=0):
        """Retry a failed join in halves; single pieces that still fail are dropped."""
        if len(polys) == 1 and depth > 0:
            pass
        halves = [polys[:len(polys) // 2], polys[len(polys) // 2:]] if len(polys) > 1 else [polys]
        for h in halves:
            if not h: continue
            cnt[0] += 1; nm = f"X{cnt[0]}"
            if op[0] == "prism": e = {"sketch": nm, "distance": round(op[3] - op[2], 4), "operation": "join", "targets": ["Base"], "name": "F" + nm}
            else: e = {"sketch": nm, "distance": op[3], "taper": -op[4], "operation": "join", "targets": ["Base"], "name": "F" + nm}
            r = S.tool("batch", {"commands": sk(nm, op[2], h) + [{"command": "solid.extrude", "params": e}], "verbose": False})
            if r["isError"]:
                if len(h) == 1: skipped.append(op[-1])
                else: join_split(op, h, depth + 1)
    for g in build_cmds(s):
        t0 = time.time(); r = S.tool("batch", {"commands": g, "verbose": False})
        print(g[-1]["params"].get("name", "?"), len(g), round(time.time() - t0, 1), "ERR" if r["isError"] else "", file=sys.stderr, flush=True)
        if r["isError"]:
            nm = g[-1]["params"].get("name", "?")
            if nm in ("TrimF", "MagCut") or g[0]["params"]["name"] == "Blank": return {"name": key, "error": S.txt(r, 400)}
            if nm.startswith("FJ"): join_split(s.ops[int(nm[2:])], s.ops[int(nm[2:])][1])
            else: skipped.append(nm)
    os.makedirs(OUT, exist_ok=True)
    S.tool("export", {"path": f"{OUT}/{key}.stl"}); S.tool("export", {"path": f"{OUT}/{key}.3mf"})
    S.shot(f"{OUT}/{key}_iso.png", view="iso", width=900, height=700, grid=False, axes=False)
    m = S.txt(S.tool("measure"), 400)
    return {"name": key, "secs": round(time.time() - t), "skipped": skipped, "measure": m}

if __name__ == "__main__":
    if sys.argv[1] == "art": art(sys.argv[2] if len(sys.argv) > 2 else f"{OUT}/layouts.png")
    elif sys.argv[1] == "build": print(json.dumps(build(sys.argv[2])))
