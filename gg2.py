"""7 styles x 10 bases (30 mm). Builds on gg.py helpers; n = 1..10 picks the variant and the seed.

Build lessons baked in (from the first 6):
- raised pieces stay inside the blank wall (clip R0+0.15); cuts may run past it (R0+1.0); one final 12 deg trim
- dense repeated relief (ripples, paving, plank seams) is CUT into a raised/ground surface, never 50+ separate joins
- stacked parts overlap >= 0.15 mm; parts never share an outline (no same-circle arcs, no exact touching)
- a join that still fails is retried in halves, then the piece is dropped; a base that fails is dropped (no retries)
Usage: python3 gg2.py art <style> | build <style> <n>
"""
import sys, math, random, json, os, time
import numpy as np
from shapely.geometry import Polygon, Point, LineString, MultiPoint, box
from shapely.ops import unary_union, voronoi_diagram
from shapely import affinity
import gg
from kit_settings import CFG
from gg import (R0, H, SINK, MINW, polys_of, inset, wide_enough, frame, stone, star, wiggle, render, sk, OUT, fit_for_taper)

gg.CLIP = R0 + 0.15
CUTR = R0 + 1.0

class Scene(gg.Scene):
    def __init__(s, *a):
        super().__init__(*a); s.cdisc = Point(0, 0).buffer(CUTR, 256)
    def clip(s, polys):
        s.kc = getattr(s, "kc", -1) + 1
        disc = Point(0, 0).buffer(R0 + 0.15 - 0.15 * (s.kc % 4), 256)
        return [g for p in polys for g in polys_of(p.intersection(disc)) if g.area > 0.08]
    def cclip(s, polys):
        return [g for p in polys for g in polys_of(p.intersection(s.cdisc)) if g.area > 0.08]
    def gcut(s, polys, depth, kind="crack"):
        s.gcuts.append((s.cclip(polys), depth, kind))
    def tcut(s, polys, z0, z1, kind="slot"):  # (empty entries are skipped by build_cmds)
        ps = [p for p in s.cclip(polys) if not p.buffer(-0.1).is_empty]
        if ps: s.tcuts.append((ps, z0, z1, kind))
    def final_gcuts(s):
        """Same as gg.Scene.final_gcuts, but the clearance between a ground cut and a raised piece is
        s.cutgap (0.05 mm by default; 0.3 mm on the Urban #11-20 bases so no sliver walls are left)."""
        gap = max(getattr(s, "cutgap", 0.05), 0.35)       # a piece standing in a cut area keeps a real margin of ground round it
        feet = unary_union(s.feet).buffer(gap) if s.feet else Polygon()
        out = []
        for polys, depth, kind in s.gcuts:
            g = unary_union(polys).difference(feet)
            ps = [p.simplify(0.01) for p in polys_of(g) if p.area > 0.1 and not p.buffer(-0.1).is_empty]
            if ps: out.append((ps, depth, kind))
        return out
    def free(s, p, gap=0.4):
        return not s.feet or not p.buffer(gap).intersects(unary_union(s.feet))

def mk(style, title, n, ground):
    return Scene(f"{style}_{n:02d}", f"{title} #{n}", f"{style}{n}", ground)

SCENE_SKIP = set()
FMIX = None      # set by gg3.make for a run with a fresh seed: feature mixes are drawn at random, not fixed per number

def pickF(table, n):
    """the feature list for base n; with FMIX set, a random 2-4 of all the features this style's table uses"""
    if FMIX is None: return table[n].split()
    pool = sorted({w for v in table.values() for w in v.split()} - SCENE_SKIP)      # (in a scene the composer lays the road itself)
    return FMIX.sample(pool, min(len(pool), FMIX.randint(2, 4)))

def line_cut(a, b, w=0.5):
    return LineString([a, b]).buffer(w / 2, cap_style=2)

def strata(s, plates, tops=(0.9, 1.75, 2.55)):
    """A rock mass: the plates merged into ONE body with steep sloped sides and a bevelled top edge (no stacked
    layers), as tall as the highest layer used to be, with a crack across its top."""
    rng = s.rng
    profs = [p for p in s.clip([unary_union(plates)]) if wide_enough(p)]
    if not profs: return
    h = tops[-1]
    ops, tp, zt = s._tiers(profs, H - SINK, h, ((0.82, 9), (0.18, 45)), "rock")
    if not ops: return
    s.ops += ops; s.feet += profs
    if tp:
        top = max(tp, key=lambda q: q.area); c = top.centroid; a = rng.uniform(0, math.pi)
        ln = line_cut((c.x - 6 * math.cos(a), c.y - 6 * math.sin(a)), (c.x + 6 * math.cos(a), c.y + 6 * math.sin(a))).intersection(inset(top, 0.3))
        if not ln.is_empty: s.tcut([ln], H + h - 0.3, H + h + 0.5, "crack")

def crack_net(s, npts, depth=0.45, region=None):
    rng = s.rng
    pts = MultiPoint([(rng.uniform(-17, 17), rng.uniform(-17, 17)) for _ in range(npts)])
    edges = set()
    for cell in voronoi_diagram(pts, envelope=box(-20, -20, 20, 20)).geoms:
        c = [tuple(round(v, 3) for v in xy) for xy in cell.exterior.coords]
        for a, b in zip(c, c[1:]): edges.add(tuple(sorted((a, b))))
    net = unary_union([LineString(wiggle(a, b, rng)).buffer(0.23, cap_style=1, resolution=4) for a, b in edges])
    if region is not None: net = net.intersection(region)
    s.gcut([net], depth, "crack")

# ======================================================================= styles
def badlands(n):
    s = mk("badlands", "Badlands", n, "#a4553b"); rng = s.rng; v = n % 3
    th = rng.uniform(0, 2 * math.pi)
    if v == 0:      # one big layered cluster
        cx, cy = 7.5 * math.cos(th), 7.5 * math.sin(th)
        strata(s, [stone(cx, cy, 4.2, rng, .7, 6), stone(cx + 4.3 * math.cos(th + 1.9), cy + 4.3 * math.sin(th + 1.9), 3.0, rng, .75, 5),
                   stone(cx + 4.0 * math.cos(th - 1.7), cy + 4.0 * math.sin(th - 1.7), 2.4, rng, .8, 5)])
    elif v == 1:    # two smaller clusters on opposite sides
        for a in (th, th + math.pi + rng.uniform(-.5, .5)):
            cx, cy = 8.5 * math.cos(a), 8.5 * math.sin(a)
            strata(s, [stone(cx, cy, 3.0, rng, .7, 5), stone(cx + 2.8 * math.cos(a + 1.6), cy + 2.8 * math.sin(a + 1.6), 2.2, rng, .8, 5)], (0.8, 1.6))
    else:           # low mesa slab with three steps
        d = rng.uniform(3, 6); cx, cy = d * math.cos(th), d * math.sin(th)
        strata(s, [stone(cx, cy, 5.5, rng, .65, 7)], (0.7, 1.5, 2.3))
    for p in s.scatter(rng.randint(2, 6), 1.2, 1.9, gap=0.4, round_=False):
        s.stack([p], 0.75, ((0.75, 5), (0.25, 40)), "rock")
    s.stack(s.scatter(rng.randint(18, 34), 0.5, 0.75, gap=0.35), 0.35, ((0.7, 10), (0.3, 45)), "grit")
    crack_net(s, rng.randint(26, 42))
    return s

def highland(n):
    s = mk("highland", "Highland", n, "#4c4a36"); rng = s.rng
    th = rng.uniform(0, 2 * math.pi)
    for k in range([1, 2, 2, 3][n % 4]):
        a = th + k * 2.1; d = rng.uniform(5.5, 9); r = rng.uniform(2.8, 4.8) if k == 0 else rng.uniform(2.2, 3.2)
        p = stone(d * math.cos(a), d * math.sin(a), r, rng, .5, 5)
        if not s.free(p, 0.6): continue
        h1, h2 = rng.uniform(1.0, 1.5), rng.uniform(1.8, 2.6)
        b = s.prism([p], H + h1, "slate")
        if not b: continue
        q = affinity.translate(affinity.scale(b[0], .72, .8), rng.uniform(-.7, .7), rng.uniform(-.7, .7))
        u = s.prism(polys_of(q.intersection(inset(b[0], 0.4))), H + h2, "slate", z0=H + h1 - 0.15)
        c = (u[0] if u else b[0]).centroid; aa = a + rng.uniform(.6, 1.2)
        if CFG.get("outcrop_cracks", False):   # the straight crack line across the outcrop (off: it reads as a line through the rock)
            s.tcut([line_cut((c.x - 8 * math.cos(aa), c.y - 8 * math.sin(aa)), (c.x + 8 * math.cos(aa), c.y + 8 * math.sin(aa))).intersection(b[0])],
                   H + h1 - 0.45, H + 3.0, "crack")
    for p in s.scatter(rng.randint(2, 6), 1.0, 1.6, gap=0.4, round_=False):
        s.prism([p], H + 0.7, "slate")
    tus = []
    for c in range(rng.randint(3, 6)):
        a = rng.uniform(0, 2 * math.pi); d = rng.uniform(2, 10); x, y = d * math.cos(a), d * math.sin(a)
        for _ in range(14):
            ro = rng.uniform(1.9, 2.8); p = star(x + rng.uniform(-3, 3), y + rng.uniform(-3, 3), ro, ro * .55, 7, rng.uniform(0, 1))
            if Point(0, 0).buffer(R0 - 0.5).contains(p) and s.free(p, 0.3) and all(not p.buffer(0.3).intersects(q) or p.intersection(q).area > 1.0 for q in tus):
                tus.append(p)
    if tus: s.stack([unary_union(tus)], 1.5, ((0.55, 28), (0.45, 50)), "grass")
    for c in range(rng.randint(1, 4)):
        a = rng.uniform(0, 2 * math.pi); d = rng.uniform(2, 11); x, y = d * math.cos(a), d * math.sin(a)
        fl = unary_union([Point(x + rng.uniform(-1.1, 1.1), y + rng.uniform(-1.1, 1.1)).buffer(.5, 8) for _ in range(6)])
        if s.free(fl, 0.3): s.stack([fl], 0.55, ((1.0, 30),), "flower")
    s.stack(s.scatter(rng.randint(14, 26), 0.5, 0.75, gap=0.35), 0.35, ((0.7, 10), (0.3, 45)), "grit")
    return s

def corridor_a(n):
    s = mk("corridor", "Spaceship Corridor", n, "#2f3540"); rng = s.rng; v = n % 3
    phi = rng.uniform(0, math.pi); f = frame(0, 0, phi); off = rng.uniform(-2, 2)
    R = lambda u0, u1, v0, v1: Polygon([f(u0, v0), f(u1, v0), f(u1, v1), f(u0, v1)])
    keep = []
    if v in (0, 2):     # grated walkway
        s.prism([R(-20, 20, off - 4.2, off - 3.1), R(-20, 20, off + 3.1, off + 4.2)], H + 0.9, "rail")
        s.prism([R(u, u + 0.95, off - 3.4, off + 3.4) for u in np.arange(-19, 19, 1.75)], H + 0.55, "metal", z0=H - 0.8, foot=False)
        s.gcut([R(-20, 20, off - 3.15, off + 3.15)], 0.6, "slot"); keep.append(R(-20, 20, off - 4.2, off + 4.2))
    pvs = {0: [off + rng.choice([-1, 1]) * 9.6], 1: [off + 7.5], 2: [off - 9.6, off + 9.6]}[v]
    for pv in pvs:
        pipe = R(-20, 20, pv - 1.15, pv + 1.15)
        s.stack([pipe], 1.5, ((0.45, 6), (0.35, 30), (0.2, 55)), "pipe")
        s.stack([R(u, u + 1.3, pv - 1.6, pv + 1.6) for u in np.arange(-15 + rng.uniform(0, 3), 16, 6.5)], 1.9, ((0.6, 6), (0.4, 40)), "pipe")
        keep.append(pipe.buffer(0.45))
    if v == 1:          # floor hatch with cross vents
        hc = rng.uniform(-3, 1)
        hatch = R(hc - 4, hc + 4, off - 5.5, off + 2.5)
        hp = s.prism([hatch], H + 0.65, "plate")
        if hp:
            s.tcut([R(hc - 3, hc + 3, off - 1.75, off - 1.25), R(hc - 0.25, hc + 0.25, off - 4.5, off + 1.5)], H + 0.25, H + 1.0, "slot")
            s.prism([Point(*f(hc + x, off - 1.5 + y)).buffer(0.5, 8) for x in (-3.1, 3.1) for y in (-3.1, 3.1)], H + 0.95, "metal", z0=H + 0.5)
        keep.append(hatch.buffer(0.5))
    keep = unary_union(keep) if keep else Polygon()
    P = rng.choice([5.6, 6.0, 6.4]); c = 1.7 if n % 2 else 0.7; a = P / 2 - 0.25
    plates, vents, treads = [], [], []
    v0 = off + 4.8 + a
    for i in range(-4, 5):
        for j in range(-4, 5):
            u, vv = i * P, v0 + j * P
            o = Polygon([f(u + x, vv + y) for x, y in ((-a + c, -a), (a - c, -a), (a, -a + c), (a, a - c), (a - c, a), (-a + c, a), (-a, a - c), (-a, -a + c))])
            for g in polys_of(o.difference(keep).intersection(s.disc)):
                if wide_enough(g) and g.area > 1.2:
                    plates.append(g)
                    if abs(g.area - o.area) < 0.01:
                        k = rng.random() * max(1.0, gg.K) ** 1.3      # bigger deck: a smaller share of the plates is patterned
                        if k < 0.35: vents += [R(u - 1.6, u + 1.6, vv + w, vv + w + 0.5) for w in (-1.45, -0.45, 0.55)]
                        elif k < 0.6: treads += [Point(*f(u + x, vv + y)).buffer(0.47, 8) for x in (-1.3, 0, 1.3) for y in (-1.3, 0, 1.3)]
    s.prism(plates, H + 0.5, "plate")
    if vents: s.tcut(vents, H + 0.1, H + 1.0, "slot")
    if treads: s.prism(treads, H + 0.8, "metal", z0=H + 0.3)
    if c > 1:
        riv = [Point(*f(i * P + P / 2, v0 + j * P + P / 2)).buffer(0.5, 8) for i in range(-4, 5) for j in range(-4, 5)]
        riv = [p for p in riv if not p.buffer(0.2).intersects(keep) and Point(0, 0).buffer(R0 - 1.0).contains(p) and not any(p.intersects(g) for g in plates)]
        s.prism(riv, H + 0.4, "metal")
    return s


# ======================================================================= Spaceship Corridor #11-20 (more detail)
def corridor_x(n):
    """Corridor 11-20: deck plates plus a recipe of bigger set pieces, each base a different mix:
    hatch, hazard-stripe band, floor drain, cable bundle, junction box + conduit, light strip, diamond plate,
    walkway between two pipes, single pipe."""
    s = mk("corridor", "Spaceship Corridor", n, "#2f3540"); rng = s.rng
    F = pickF({11: "hatch cables hazard", 12: "walk2 light", 13: "drain cables junction", 14: "hazard diamond pipe",
         15: "light hatch junction", 16: "light cables diamond drain", 17: "cables drain hazard", 18: "junction light diamond pipe",
         19: "hatch hazard cables", 20: "drain light pipe"}, n)
    phi = rng.uniform(0, math.pi); f = frame(0, 0, phi); off = rng.uniform(-1.5, 1.5)
    R = lambda u0, u1, v0, v1: Polygon([f(u0, v0), f(u1, v0), f(u1, v1), f(u0, v1)])
    keep = []                      # set-piece footprints (deck plates stay 0.5 mm clear of these)
    def free(p): return not keep or not p.buffer(0.4).intersects(unary_union(keep))
    lanes = iter([off - 8.5, off + 8.5, off - 2.0, off + 4.5])    # parallel bands across the base for linear pieces
    if "walk2" in F:               # grated walkway between two pipes (the build that worked best before)
        s.prism([R(-20, 20, off - 4.2, off - 3.1), R(-20, 20, off + 3.1, off + 4.2)], H + 0.9, "rail")
        s.prism([R(u, u + 0.95, off - 3.4, off + 3.4) for u in np.arange(-19, 19, 1.75)], H + 0.55, "metal", z0=H - 0.8, foot=False)
        s.gcut([R(-20, 20, off - 3.15, off + 3.15)], 0.6, "slot"); keep.append(R(-20, 20, off - 4.2, off + 4.2))
        lanes = iter([off - 9.6, off + 9.6, off - 13.0, off + 13.0])
        F = F + ["pipe", "pipe"]
    for k in range(F.count("pipe")):
        pv = next(lanes)
        pipe = R(-20, 20, pv - 1.15, pv + 1.15)
        s.stack([pipe], 1.5, ((0.45, 6), (0.35, 30), (0.2, 55)), "pipe")
        s.stack([R(u, u + 1.3, pv - 1.6, pv + 1.6) for u in np.arange(-15 + rng.uniform(0, 3), 16, 6.5)], 1.9, ((0.6, 6), (0.4, 40)), "pipe")
        keep.append(pipe.buffer(0.45))
    if "cables" in F:              # bundle of three cables on a flat carrier strip, rim to rim
        cv = next(lanes)
        s.prism([R(-20, 20, cv - 1.9, cv + 1.9)], H + 0.35, "metal")
        for w in (-1.15, 0.0, 1.15):
            s.stack([R(-20, 20, cv + w - 0.45, cv + w + 0.45)], 0.6, ((0.6, 12), (0.4, 40)), "cable", z0=H + 0.35 - 0.15 + SINK)
        s.tcut([R(u - 0.3, u + 0.3, cv - 2.0, cv + 2.0) for u in np.arange(-14, 15, 5.0)], H + 0.75, H + 1.6, "slot")   # tie bands
        keep.append(R(-20, 20, cv - 1.9, cv + 1.9))
    if "hazard" in F:              # raised hazard band with diagonal stripes (paint guide), rim to rim
        hv = next(lanes)
        band = s.prism([R(-20, 20, hv - 1.4, hv + 1.4)], H + 0.6, "plate")
        if band:
            stripes = [Polygon([f(u, hv - 2), f(u + 0.55, hv - 2), f(u + 0.55 + 4, hv + 2), f(u + 4, hv + 2)]) for u in np.arange(-24, 20, 1.6)]
            s.tcut([unary_union(stripes).intersection(inset(band[0], 0.35))], H + 0.3, H + 1.0, "slot")
        keep.append(R(-20, 20, hv - 1.4, hv + 1.4))
    if "light" in F:               # recessed light channel with raised lens segments
        lv = next(lanes)
        s.gcut([R(-20, 20, lv - 0.7, lv + 0.7)], 0.55, "slot")
        s.prism([R(u, u + 2.6, lv - 0.45, lv + 0.45) for u in np.arange(-18, 18, 3.6)], H + 0.15, "lens", z0=H - 0.75, foot=False)
        keep.append(R(-20, 20, lv - 0.7, lv + 0.7))
    def place(w, l, tries=80):
        for _ in range(tries):
            a = rng.uniform(0, 2 * math.pi); d = rng.uniform(1, 9.5); cx, cy = d * math.cos(a), d * math.sin(a)
            p = Polygon([frame(cx, cy, phi)(x, y) for x, y in ((-l / 2, -w / 2), (l / 2, -w / 2), (l / 2, w / 2), (-l / 2, w / 2))])
            if free(p) and Point(0, 0).buffer(R0 - 0.6).contains(p): return cx, cy, p
        return None
    if "hatch" in F:
        got = place(8.0, 8.0)
        if got:
            cx, cy, hatch = got; ff = frame(cx, cy, phi)
            hp = s.prism([hatch], H + 0.65, "plate")
            if hp:
                s.tcut([Polygon([ff(-3, -0.25), ff(3, -0.25), ff(3, 0.25), ff(-3, 0.25)]), Polygon([ff(-0.25, -3), ff(0.25, -3), ff(0.25, 3), ff(-0.25, 3)])], H + 0.25, H + 1.0, "slot")
                s.prism([Point(*ff(x, y)).buffer(0.5, 8) for x in (-3.1, 3.1) for y in (-3.1, 3.1)], H + 0.95, "metal", z0=H + 0.5)
            keep.append(hatch)
    if "drain" in F:               # round floor drain: ring plate, radial slots, centre boss
        got = place(6.6, 6.6)
        if got:
            cx, cy, _ = got; ring = Point(cx, cy).buffer(3.3, 64)
            rp = s.prism([ring], H + 0.6, "plate")
            if rp:
                slots = [line_cut((cx + 1.3 * math.cos(a), cy + 1.3 * math.sin(a)), (cx + 2.6 * math.cos(a), cy + 2.6 * math.sin(a)), 0.5)
                         for a in np.arange(0, 2 * math.pi, math.pi / 6)]
                s.tcut(slots, H - 0.2, H + 1.0, "slot")
                s.prism([Point(cx, cy).buffer(0.8, 24)], H + 0.9, "metal", z0=H + 0.45)
            keep.append(ring)
    if "junction" in F:            # junction box with lid seam + bolts, conduit running from it to the rim
        got = place(3.6, 4.6)
        if got:
            cx, cy, jb = got; ff = frame(cx, cy, phi)
            jp = s.prism([jb], H + 1.7, "metal")
            if jp:
                s.tcut([Polygon([ff(-2.6, -0.2), ff(2.6, -0.2), ff(2.6, 0.2), ff(-2.6, 0.2)])], H + 1.4, H + 2.2, "slot")
                s.prism([Point(*ff(x, y)).buffer(0.47, 8) for x in (-1.6, 1.6) for y in (-1.1, 1.1)], H + 1.95, "metal", z0=H + 1.55)
                ang = math.atan2(cy, cx); a2 = (cx + 2.3 * math.cos(ang), cy + 2.3 * math.sin(ang)); b2 = (18 * math.cos(ang), 18 * math.sin(ang))
                cl = LineString([(cx, cy), b2]).buffer(0.7, cap_style=2)
                if not keep or not cl.difference(jb).buffer(0.4).intersects(unary_union(keep)):
                    s.stack(polys_of(cl.difference(jb.buffer(-0.4))), 1.0, ((0.6, 10), (0.4, 45)), "pipe")
                keep += [jb, LineString([(cx, cy), b2]).buffer(0.7, cap_style=2)]
    # deck plates everywhere else
    P = rng.choice([5.6, 6.0, 6.4]); c = 1.7 if n % 2 else 0.7; a = P / 2 - 0.25
    kp = unary_union(keep).buffer(0.5) if keep else Polygon()
    plates, vents, treads, diam = [], [], [], []
    v0 = off + 4.8 + a
    for i in range(-4, 5):
        for j in range(-4, 5):
            u, vv = i * P, v0 + j * P
            o = Polygon([f(u + x, vv + y) for x, y in ((-a + c, -a), (a - c, -a), (a, -a + c), (a, a - c), (a - c, a), (-a + c, a), (-a, a - c), (-a, -a + c))])
            for g in polys_of(o.difference(kp).intersection(s.disc)):
                if wide_enough(g) and g.area > 1.2:
                    plates.append(g)
                    if abs(g.area - o.area) < 0.01:
                        k = rng.random() * max(1.0, gg.K) ** 1.3      # bigger deck: a smaller share of the plates is patterned
                        if "diamond" in F and k < 0.4:
                            diam += [affinity.rotate(Point(*f(u + x, vv + y)).buffer(0.5, 4), 45, origin=f(u + x, vv + y))
                                     for x in np.arange(-1.8, 1.9, 1.2) for y in np.arange(-1.8, 1.9, 1.2) if (round(x / 1.2) + round(y / 1.2)) % 2 == 0]
                        elif k < 0.6: vents += [R(u - 1.6, u + 1.6, vv + w, vv + w + 0.5) for w in (-1.45, -0.45, 0.55)]
                        elif k < 0.8: treads += [Point(*f(u + x, vv + y)).buffer(0.47, 8) for x in (-1.3, 0, 1.3) for y in (-1.3, 0, 1.3)]
    s.prism(plates, H + 0.5, "plate")
    if vents: s.tcut(vents, H + 0.1, H + 1.0, "slot")
    if treads: s.prism(treads, H + 0.8, "metal", z0=H + 0.3)
    if diam: s.prism(diam, H + 0.75, "metal", z0=H + 0.3)
    if c > 1:
        riv = [Point(*f(i * P + P / 2, v0 + j * P + P / 2)).buffer(0.5, 8) for i in range(-4, 5) for j in range(-4, 5)]
        riv = [p for p in riv if not p.buffer(0.2).intersects(kp) and Point(0, 0).buffer(R0 - 1.0).contains(p) and not any(p.intersects(g) for g in plates)]
        s.prism(riv, H + 0.4, "metal")
    return s

def corridor(n):
    return corridor_a(n) if n <= 10 else corridor_x(n)

def infestation(n):
    s = mk("infestation", "Alien Infestation", n, "#2b3140"); rng = s.rng
    th = rng.uniform(0, 2 * math.pi); d0 = rng.uniform(6, 9); bx, by = d0 * math.cos(th), d0 * math.sin(th)
    mass = unary_union([Point(bx + rng.uniform(-3, 3), by + rng.uniform(-3, 3)).buffer(rng.uniform(2.0, 3.4), 12) for _ in range(rng.randint(4, 7))])
    if n % 2 == 0:
        m2 = unary_union([Point(-bx * 1.15 + rng.uniform(-2, 2), -by * 1.15 + rng.uniform(-2, 2)).buffer(rng.uniform(1.8, 2.6), 12) for _ in range(3)])
        mass = unary_union([mass, m2])
    mass = mass.buffer(1.0).buffer(-1.0).simplify(0.05)
    tend = []
    for k in range(rng.randint(3, 6)):
        a = th + math.pi + rng.uniform(-1.3, 1.3); x, y = bx + 2.5 * math.cos(a), by + 2.5 * math.sin(a)
        pts = [(x, y)]; L = rng.uniform(6, 12)
        for i in range(8):
            a += rng.uniform(-.5, .5); x += L / 8 * math.cos(a); y += L / 8 * math.sin(a); pts.append((x, y))
        tend += [LineString(pts[i:i + 2]).buffer(0.75 - 0.03 * i, cap_style=1, resolution=6) for i in range(8)] + [Point(*pts[-1]).buffer(0.85, 10)]
    tend = unary_union(tend).difference(mass.buffer(-0.6)).intersection(Point(0, 0).buffer(R0 - 0.3))
    a6 = rng.uniform(2.6, 3.4); plates = []
    MB = mass.buffer(0.3).buffer(0); MBB = box(*MB.bounds).buffer(0.5)
    for i in range(-7, 8):
        for j in range(-7, 8):
            x, y = i * a6 * 1.5, (j + (0.5 if i % 2 else 0)) * a6 * math.sqrt(3)
            if math.hypot(x, y) > R0 + a6 + 1.5: continue          # off the base
            h = Polygon([(x + (a6 - 0.3) * math.cos(k * math.pi / 3), y + (a6 - 0.3) * math.sin(k * math.pi / 3)) for k in range(6)])
            try: cutp = h.difference(MB) if MBB.intersects(h) else h
            except Exception: continue
            plates += [(q, (x, y), abs(q.area - h.area) < 0.01) for q in polys_of(cutp.intersection(s.disc)) if wide_enough(q) and q.area > 1.2]
    s.prism([q for q, _, _ in plates], H + 0.45, "plate")
    slots = []
    for q, (x, y), whole in plates:
        if whole and a6 > 2.9 and rng.random() < 0.3 and not q.intersects(tend):
            slots += [box(x - 1.4, y + w, x + 1.4, y + w + 0.5) for w in (-1.0, 0.0, 1.0)]
    if slots: s.tcut(slots, H + 0.05, H + 1.0, "slot")
    t1 = s.prism([g for g in polys_of(tend) if g.area > 0.5], H + 0.75, "flesh")
    s.prism([g for p in t1 for g in polys_of(inset(p, 0.3))], H + 1.15, "flesh", z0=H + 0.6)
    profs, ztop = s.stack(polys_of(mass), 2.3, ((0.4, 14), (0.35, 32), (0.25, 52)), "flesh")
    pus = []
    big = max(profs, key=lambda p: p.area) if profs else None
    if big is not None:
        for _ in range(60):
            if len(pus) >= rng.randint(2, 5): break
            x0, y0, x1, y1 = big.bounds; p = Point(rng.uniform(x0, x1), rng.uniform(y0, y1)).buffer(rng.uniform(.55, .75), 10)
            if big.buffer(-0.1).contains(p) and all(p.distance(q) > .3 for q in pus): pus.append(p)
        if pus: s.stack(pus, 0.6, ((0.6, 20), (0.4, 55)), "pustule", z0=ztop - 0.15 + SINK)
    return s

def urban_a(n):
    s = mk("urban", "Urban Warfare", n, "#7e7b76"); rng = s.rng
    phi = rng.uniform(0, math.pi); f = frame(0, 0, phi); d = rng.uniform(-5, 2.5) * gg.K
    R = lambda u0, u1, v0, v1: Polygon([f(u0, v0), f(u1, v0), f(u1, v1), f(u0, v1)])
    s.stack([R(u, u + 4.0, d, d + 1.8) for u in np.arange(-19.5 + rng.uniform(0, 2), 19, 4.5)], 1.0, ((0.8, 3), (0.2, 40)), "curb")
    slab = s.prism([R(-20, 20, d + 1.8, 20)], H + 0.75, "slab")
    sl = unary_union(slab) if slab else Polygon()
    pv = n % 3; core = sl.difference(R(-20, 20, d - 0.5, d + 2.3))
    if pv == 2:         # square flagstones at 45 deg: two sets of crossing seams
        g = affinity.rotate(unary_union([box(-20, y - 0.22, 20, y + 0.22) for y in np.arange(-20, 20, 3.6)]), math.degrees(phi) + 45, origin=(0, 0))
        x = affinity.rotate(unary_union([box(y - 0.22, -20, y + 0.22, 20) for y in np.arange(-20, 20, 3.6)]), math.degrees(phi) + 45, origin=(0, 0))
    else:               # running bond, bricks along (pv 0) or across (pv 1) the curb
        L, w = (3.2, 1.55) if pv == 0 else (1.55, 3.2)
        if pv == 0:
            g = unary_union([R(-20, 20, d + 2.3 + j * w - 0.22, d + 2.3 + j * w + 0.22) for j in range(1, 14)])
            x = unary_union([R(i * L + (j % 2) * L / 2 - 0.22, i * L + (j % 2) * L / 2 + 0.22, d + 2.3 + j * w + 0.1, d + 2.3 + (j + 1) * w - 0.1)
                             for j in range(0, 14) for i in range(-8, 9)])
        else:
            g = unary_union([R(u - 0.22, u + 0.22, -20, 20) for u in np.arange(-20, 20, w)])
            x = unary_union([R(u + 0.1, u + w - 0.1, d + 2.3 + j * 3.2 + (k % 2) * 1.6 - 0.22, d + 2.3 + j * 3.2 + (k % 2) * 1.6 + 0.22)
                             for k, u in enumerate(np.arange(-20, 20, w)) for j in range(-1, 6)])
    s.tcut([unary_union([g, x]).intersection(core)], H + 0.33, H + 0.75, "joint")     # one connected joint net: cuts in well under a second
    if n % 2:           # storm drain
        u0 = rng.uniform(-6, 2)
        s.prism([R(u0, u0 + 6.0, d - 2.7, d + 0.1)], H + 0.3, "metal")
        s.tcut([R(u0 + 0.8 + i * 1.1, u0 + 1.3 + i * 1.1, d - 2.1, d - 0.5) for i in range(4)], H - 0.5, H + 1.0, "slot")
    road = R(-20, 20, -20, d - 0.3)
    for p in s.scatter(rng.randint(2, 5), 1.1, 1.9, region=road, gap=0.5, round_=False):
        s.stack([p], rng.uniform(0.8, 1.2), ((0.6, 4), (0.4, 30)), "rubble")
    hole = None
    if n % 3 != 0:
        for _ in range(60):
            px, py = f(rng.uniform(-8, 8), d - rng.uniform(4, 9)); cand = stone(px, py, rng.uniform(1.6, 2.4), rng, .8, 8)
            if Point(0, 0).buffer(R0 - 1.2).contains(cand) and s.free(cand, 0.6): hole = cand; break
    if hole is not None: s.gcut([hole], 0.7, "pothole")
    crk = []
    for k in range(rng.randint(1, 3)):
        x, y = f(rng.uniform(-8, 8), d)
        tx, ty = (hole.centroid.x, hole.centroid.y) if (k == 0 and hole is not None) else f(rng.uniform(-14, 14), d - 30)
        pts = [(x, y)] + [(x + (tx - x) * t + rng.uniform(-.8, .8), y + (ty - y) * t + rng.uniform(-.8, .8)) for t in np.linspace(0, 1, 9)[1:-1]] + [(tx, ty)]
        crk.append(LineString(pts).buffer(0.22, cap_style=1, resolution=4))
    s.gcut([unary_union(crk).intersection(road.buffer(0.3))], 0.45, "crack")
    reg = road.difference(hole.buffer(0.3)) if hole is not None else road
    s.stack(s.scatter(rng.randint(20, 40), 0.47, 0.62, region=reg, gap=0.35), 0.25, ((1.0, 20),), "grit")
    return s

def crate(s, cx, cy, ang, w, l, h, z0=None):
    """Wooden crate: box with plank seams across the lid (seams run over the lid edges like real plank gaps)."""
    f = frame(cx, cy, ang)
    p = Polygon([f(-l / 2, -w / 2), f(l / 2, -w / 2), f(l / 2, w / 2), f(-l / 2, w / 2)])
    zb = H if z0 is None else z0
    got = s.prism([p], zb + h, "crate", z0=None if z0 is None else z0 - 0.15)
    if not got: return None
    k = max(2, int(round(l / 1.3)))
    seams = [line_cut(f(-l / 2 + l * i / k, -w / 2 - 0.6), f(-l / 2 + l * i / k, w / 2 + 0.6), 0.35) for i in range(1, k)]
    s.tcut(seams, zb + h - 0.3, zb + h + 0.5, "crack")
    return got[0], zb + h

def crater(s, cx, cy, r, depth=0.8):
    """Blast hole: irregular pit, broken-rock lip, short radial cracks that each end in a chunk of rock."""
    rng = s.rng
    hole = stone(cx, cy, r, rng, rng.uniform(.75, .95), 9)
    lip, ends, cracks = [], [], []
    for k in range(rng.randint(6, 10)):
        a = rng.uniform(0, 2 * math.pi); d = r + rng.uniform(0.3, 0.9)
        lip.append(stone(cx + d * math.cos(a), cy + d * math.sin(a), rng.uniform(0.6, 1.0), rng, .8, 5))
    for k in range(rng.randint(3, 5)):
        a = rng.uniform(0, 2 * math.pi); L = rng.uniform(2.0, 3.8)
        p0 = (cx + r * 0.8 * math.cos(a), cy + r * 0.8 * math.sin(a))
        p1 = (cx + (r + L) * math.cos(a + rng.uniform(-.2, .2)), cy + (r + L) * math.sin(a + rng.uniform(-.2, .2)))
        cracks.append(LineString(wiggle(p0, p1, rng, 0.25)).buffer(0.22, cap_style=1, resolution=4))
        ends.append(stone(p1[0], p1[1], rng.uniform(0.55, 0.75), rng, .85, 5))
    rocks = [g for g in polys_of(unary_union(lip + ends)) if Point(0, 0).buffer(R0 - 0.3).contains(g)]
    s.stack(rocks, rng.uniform(0.6, 0.9), ((0.65, 8), (0.35, 40)), "rock")
    s.gcut([hole], depth, "pit")
    s.gcut([unary_union(cracks)], 0.4, "crack")
    return hole

def drill_holes(s, cx, cy, ang, rows, cols, z_top=None):
    f = frame(cx, cy, ang)
    holes = [Point(*f(i * 1.8, j * 1.8)).buffer(0.5, 10) for i in range(cols) for j in range(rows)]
    if z_top is None: s.gcut(holes, 0.9, "pit")
    else: s.tcut(holes, z_top - 0.9, z_top + 0.5, "pit")
    return unary_union(holes)

def mine_v3(n):
    """1800s mine dug by pick and powder: packed earth, ruts, blast craters, drill holes, pick-scarred bedrock,
    spoil heaps, crates; a short track on 3 of 10 bases."""
    s = mk("mine", "1800s Mine", n, "#5f4a38"); rng = s.rng
    F = pickF({1: "crater ruts crates heap", 2: "track crates bedrock", 3: "bedrock crater ruts", 4: "crater crater crates",
         5: "planks crates ruts crater", 6: "track heap bedrock", 7: "ruts ruts crates crater", 8: "bedrock crater beam crates",
         9: "track crater crates", 10: "heap ruts crates bedrock"}, n)
    phi = rng.uniform(0, math.pi); f = frame(0, 0, phi)
    R = lambda u0, u1, v0, v1: Polygon([f(u0, v0), f(u1, v0), f(u1, v1), f(u0, v1)])
    if "bedrock" in F:      # raised bedrock shelf at one edge: pick marks + a drill pattern on top
        a = rng.uniform(0, 2 * math.pi); cx, cy = 11.5 * math.cos(a), 11.5 * math.sin(a)
        shelf = stone(cx, cy, rng.uniform(5.5, 7.0), rng, .6, 8)
        h1 = rng.uniform(1.3, 1.8)
        b = s.prism([shelf], H + h1, "bedrock")
        if b:
            core = inset(b[0], 0.5)
            q = affinity.translate(affinity.scale(b[0], .7, .7), -1.2 * math.cos(a), -1.2 * math.sin(a)).intersection(inset(b[0], 0.6))
            top2 = s.prism(polys_of(q), H + h1 + 0.7, "bedrock", z0=H + h1 - 0.15)
            marks = []
            for g in range(rng.randint(3, 5)):
                x0, y0, x1, y1 = core.bounds; px, py = rng.uniform(x0, x1), rng.uniform(y0, y1); aa = rng.uniform(0, math.pi)
                ff = frame(px, py, aa)
                marks += [line_cut(ff(-0.8, k * 0.9), ff(0.8, k * 0.9 + rng.uniform(-.15, .15)), 0.4) for k in range(rng.randint(3, 4))]
            m = unary_union(marks).intersection(inset(b[0], 0.4))
            if top2: m = m.difference(top2[0].buffer(0.3))
            s.tcut([m], H + h1 - 0.35, H + h1 + 0.5, "crack")
            if top2:
                c = top2[0].centroid
                if top2[0].buffer(-0.5).contains(Point(c.x, c.y).buffer(2.4)): drill_holes(s, c.x - 0.9, c.y - 0.9, a, 2, 2, H + h1 + 0.7)
    if "track" in F:        # short spur: rails from the rim to a timber end-stop
        off = rng.uniform(-4, 4); ue = rng.uniform(-3, 4); g2 = 3.0
        s.prism([R(ue - 0.6, ue + 1.4, off - 4.8, off + 4.8)], H + 1.6, "wood")
        tp = [affinity.rotate(R(u - 0.85, u + 0.85, off - 4.4, off + 4.4), rng.uniform(-5, 5), origin=f(u, off))
              for u in np.arange(ue + 3.0, 18, 3.3) if rng.random() > 0.15]
        tp = [t for t in tp if Point(0, 0).buffer(R0 - 0.2).contains(t)]
        s.prism(tp, H + 0.7, "wood")
        s.prism([R(ue + 0.3, 20, off - g2 - 0.45, off - g2 + 0.45), R(ue + 0.3, 20, off + g2 - 0.45, off + g2 + 0.45)], H + 1.25, "iron")
    if "planks" in F:       # short plank walkway, flush, thin seams with staggered joints
        off = rng.uniform(-8, -4)
        fl = s.prism([R(-20, 20, off, off + 6.6)], H + 0.6, "wood")
        if fl:
            seams = [R(-20, 20, off + 2.2 * k - 0.15, off + 2.2 * k + 0.15) for k in (1, 2)]
            for k in range(3):
                for u in np.arange(-18 + rng.uniform(0, 6), 18, rng.uniform(7, 10)):
                    seams.append(R(u - 0.15, u + 0.15, off + 2.2 * k + 0.1, off + 2.2 * (k + 1) - 0.1))
            s.tcut([unary_union(seams).intersection(inset(fl[0], 0.3))], H + 0.25, H + 0.6, "crack")
    if "beam" in F:         # fallen roof timber lying flat, two grain lines
        a = rng.uniform(0, math.pi); d = rng.uniform(-4, 4); ff = frame(d * math.cos(a + 1.57), d * math.sin(a + 1.57), a)
        bm = Polygon([ff(-6, -0.9), ff(6, -0.9), ff(6, 0.9), ff(-6, 0.9)])
        if s.free(bm, 0.5) and Point(0, 0).buffer(R0 - 0.3).contains(bm):
            s.prism([bm], H + 1.2, "wood")
            s.tcut([line_cut(ff(-5, v), ff(5, v), 0.35) for v in (-0.35, 0.35)], H + 0.9, H + 1.7, "crack")
    for k in range(F.count("crater")):
        for _ in range(80):
            r = rng.uniform(1.6, 2.6); a = rng.uniform(0, 2 * math.pi); d = rng.uniform(0, 9)
            cx, cy = d * math.cos(a), d * math.sin(a)
            if s.free(Point(cx, cy).buffer(r + 4.2), 0.2):
                crater(s, cx, cy, r, 0.8 if d > 4 else 0.7); break
    if "heap" in F:         # spoil heap against the rim + loose blast rubble
        a = rng.uniform(0, 2 * math.pi); hx, hy = 11 * math.cos(a), 11 * math.sin(a)
        heap = [stone(hx + rng.uniform(-3, 3), hy + rng.uniform(-3, 3), rng.uniform(1.2, 2.2), rng, .8, 6) for _ in range(rng.randint(5, 8))]
        heap = [h for h in heap if s.free(h, 0.4)]
        if heap:
            prof, z = s.stack([unary_union(heap).intersection(Point(0, 0).buffer(R0 - 0.6, 128))], rng.uniform(1.8, 2.6),
                              ((0.4, 10), (0.3, 25), (0.3, 48)), "rock")
    for k in range(F.count("crates") and rng.randint(1, 3)):
        for _ in range(80):
            a = rng.uniform(0, 2 * math.pi); d = rng.uniform(3, 10); w, l = rng.uniform(2.6, 3.4), rng.uniform(3.2, 4.4)
            cx, cy = d * math.cos(a), d * math.sin(a); ang = rng.uniform(0, math.pi)
            fp = Point(cx, cy).buffer(math.hypot(w, l) / 2)
            if s.free(fp, 0.3) and Point(0, 0).buffer(R0 - 0.5).contains(fp):
                h = rng.uniform(1.3, 1.6); c = crate(s, cx, cy, ang, w, l, h)
                if c and k == 0 and rng.random() < 0.6:     # a smaller crate stacked on the first one
                    i0 = len(s.tcuts) - 1
                    c2 = crate(s, cx + rng.uniform(-.3, .3), cy + rng.uniform(-.3, .3), ang + rng.uniform(-.4, .4), w * 0.7, l * 0.6, 1.2, z0=c[1])
                    if c2:   # lower crate's lid seams stop at the stacked crate
                        ps, z0_, z1_, kd = s.tcuts[i0]
                        keep = [g for p in ps for g in polys_of(p.difference(c2[0].buffer(0.3))) if not g.buffer(-0.1).is_empty]
                        s.tcuts[i0] = (keep, z0_, z1_, kd) if keep else ([], z0_, z1_, kd)
                break
    for r in range(F.count("ruts")):   # pairs of cart ruts, rim to rim, gently curving (stop at anything they meet)
        a = rng.uniform(0, math.pi); off = rng.uniform(-6, 6); ff = frame(0, 0, a); bend = rng.uniform(-0.03, 0.03)
        ruts = [LineString([ff(u, off + o + bend * u * u) for u in np.linspace(-18, 18, 24)]).buffer(0.3, cap_style=2) for o in (-2.5, 2.5)]
        s.gcut([unary_union(ruts)], 0.45, "rut")
    for p in s.scatter(rng.randint(4, 8), 0.9, 1.6, gap=0.35, round_=False):
        s.stack([p], rng.uniform(0.5, 0.9), ((0.7, 6), (0.3, 40)), "rock")
    s.stack(s.scatter(rng.randint(35, 55), 0.48, 0.72, gap=0.3), 0.35, ((0.7, 10), (0.3, 45)), "grit")
    return s

def print_trail(s, phi, off, rng):
    """Three-toed creature prints walking rim to rim (cut), alternating left/right."""
    f = frame(0, 0, phi); cuts = []
    for i, u in enumerate(np.arange(-16, 16, 2.6)):
        side = 1 if i % 2 else -1; ff = frame(*f(u, off + side * 0.9), phi)
        cuts.append(Point(*ff(0, 0)).buffer(0.55, 10))
        for t in (-0.5, 0, 0.5):
            cuts.append(line_cut(ff(0.4, 0), ff(0.4 + 1.3 * math.cos(t), 1.3 * math.sin(t)), 0.5))
    return unary_union(cuts)

def maahl_v3(n):
    """Deserts of Maahl without ripples: dune drifts, pitted sand, pebble pavement, cracked hardpan,
    creature tracks, rounded sandstone outcrops and bubbly pebble clusters."""
    s = mk("maahl", "Deserts of Maahl", n, "#c9a36c"); rng = s.rng
    F = pickF({1: "dune pits outcrop", 2: "pavement outcrop2 trail", 3: "hardpan dune pebbles", 4: "pits pavement outcrop",
         5: "dune trail pebbles", 6: "outcrop2 pits hardpan", 7: "dune pavement pits", 8: "trail outcrop pits",
         9: "hardpan pavement pebbles outcrop", 10: "dune outcrop pits pebbles"}, n)
    th = rng.uniform(0, 2 * math.pi)
    if "dune" in F:         # crescent drift banked against the rim, 4 soft stacked tiers
        cx, cy = 13 * math.cos(th), 13 * math.sin(th)
        body = Point(cx, cy).buffer(rng.uniform(7, 8.5), 48).difference(Point(cx * 0.45, cy * 0.45).buffer(rng.uniform(4.2, 4.8), 48))
        body = body.buffer(-0.6).buffer(0.6).simplify(0.05)
        s.stack(polys_of(body), rng.uniform(1.8, 2.4), ((0.3, 12), (0.25, 18), (0.25, 26), (0.2, 40)), "dune")
    rocks = (["outcrop"] if "outcrop" in F else []) + (["big", "small"] if "outcrop2" in F else [])
    for k, kind in enumerate(rocks):
        for _ in range(60):
            r = {"outcrop": rng.uniform(3.0, 3.8), "big": 3.6, "small": 2.2}[kind]
            a = th + math.pi + rng.uniform(-1.4, 1.4); d = rng.uniform(4, 9.5); cx, cy = d * math.cos(a), d * math.sin(a)
            g = unary_union([Point(cx + rng.uniform(-r, r) * .6, cy + rng.uniform(-r, r) * .6).buffer(r * rng.uniform(.5, .8), 12) for _ in range(4)])
            g = g.buffer(0.8).buffer(-0.8).simplify(0.05)
            if s.free(g, 0.8) and Point(0, 0).buffer(R0 - 0.4).contains(g):
                s.stack([g], 2.7 if kind != "small" else 1.6, ((0.35, 8), (0.25, 20), (0.25, 38), (0.15, 60)), "sandstone"); break
    if "hardpan" in F:      # cracked clay pan in a low hollow
        for _ in range(60):
            a = rng.uniform(0, 2 * math.pi); d = rng.uniform(2, 8); reg = Point(d * math.cos(a), d * math.sin(a)).buffer(rng.uniform(4, 5.5), 32)
            if s.free(reg, 0.6) and Point(0, 0).buffer(R0 - 0.5).contains(reg): break
        pan = s.prism([reg], H + 0.35, "pan")
        if pan:
            pts = MultiPoint([(rng.uniform(-17, 17), rng.uniform(-17, 17)) for _ in range(60)]); edges = set()
            for cell in voronoi_diagram(pts, envelope=box(-20, -20, 20, 20)).geoms:
                c = [tuple(round(v, 3) for v in xy) for xy in cell.exterior.coords]
                for a_, b_ in zip(c, c[1:]): edges.add(tuple(sorted((a_, b_))))
            net = unary_union([LineString(wiggle(a_, b_, rng, 0.25)).buffer(0.2, cap_style=1, resolution=4) for a_, b_ in edges])
            s.tcut([net.intersection(pan[0].buffer(0.6))], H + 0.05, H + 0.8, "crack")
    if "pebbles" in F:      # bubbly pebble clusters
        for c in range(rng.randint(2, 3)):
            a = rng.uniform(0, 2 * math.pi); d = rng.uniform(1, 11); reg = Point(d * math.cos(a), d * math.sin(a)).buffer(2.2)
            if not s.free(reg, 0.4): continue
            peb = s.scatter(9, 0.5, 0.75, region=reg, gap=0.2, tries=2000)
            if peb: s.stack(peb, 0.5, ((0.6, 15), (0.4, 50)), "pebble")
    if "pavement" in F:     # desert pavement: a patch of packed flat pebbles
        for _ in range(60):
            a = rng.uniform(0, 2 * math.pi); d = rng.uniform(2, 9); reg = Point(d * math.cos(a), d * math.sin(a)).buffer(rng.uniform(3.5, 4.8), 24)
            if s.free(reg, 0.5) and Point(0, 0).buffer(R0 - 0.3).contains(reg):
                s.stack(s.scatter(40, 0.48, 0.7, region=reg, gap=0.18, tries=4000), 0.3, ((0.75, 10), (0.25, 45)), "pebble"); break
    if "trail" in F:        # creature tracks crossing the sand
        tr = print_trail(s, rng.uniform(0, math.pi), rng.uniform(-6, 6), rng)
        s.gcut([tr], 0.4, "pit")
    if True:                # pitted, wind-blasted sand: shallow dimples that catch a wash (light scatter if not a "pits" base)
        pits = []; full = "pits" in F
        busy = unary_union(s.feet + [q for ps, _, _ in s.gcuts for q in ps] + [q for ps, *_ in s.tcuts for q in ps]).buffer(0.5)
        for _ in range(3000):
            if len(pits) >= (rng.randint(55, 80) if full else rng.randint(18, 28)): break
            a = rng.uniform(0, 2 * math.pi); d = math.sqrt(rng.random()) * (R0 - 0.6); r = rng.uniform(0.45, 0.85) if full else rng.uniform(0.45, 0.6)
            p = affinity.scale(Point(d * math.cos(a), d * math.sin(a)).buffer(r, 10), 1.0, rng.uniform(.75, 1.0))
            if not p.intersects(busy) and all(p.distance(q) > 0.45 for q in pits): pits.append(p)
        s.gcut(pits, 0.35, "pit")
    s.stack(s.scatter(rng.randint(10, 20), 0.5, 0.75, gap=0.4), 0.4, ((0.7, 10), (0.3, 45)), "pebble")
    return s

# ======================================================================= uneven ground (v4 Mine / Desert)
# Terrain elements: (footprint, z_base, z_top, flat_top_region). Features placed with tprism/tstack rise from the
# LOWEST ground under them (never float) and stand their height above the HIGHEST ground under them.
# Pits/marks (tcut_flat) are cut only where the ground is flat, from that local surface, and never under a feature.
CAP = H + gg.max_height()

def T_init(s):
    if not hasattr(s, "terr"): s.terr, s.objs, s.keepout = [], [], []

def T_add(s, fp, zb, zt, top):
    T_init(s); s.terr.append((fp, zb, zt, top))

def T_zr(s, p):
    T_init(s); lo = hi = H
    for fp, zb, zt, top in s.terr:
        if fp.intersects(p) and fp.intersection(p).area > 1e-3: hi = max(hi, zt)
        if top.contains(p): lo = max(lo, zt)
        elif fp.contains(p): lo = max(lo, zb)
    return round(lo, 3), round(hi, 3)

def T_flat(s, p):
    T_init(s)
    if any(k.intersects(p) for k in s.keepout): return None
    hi = H; own = []
    for fp, zb, zt, top in s.terr:
        if fp.intersects(p) and fp.intersection(p).area > 1e-3:
            if zt > hi + 1e-6: hi, own = zt, [top]
            elif abs(zt - hi) < 1e-6: own.append(top)
    if hi == H: return H
    return hi if any(t.contains(p) for t in own) else None

def T_free(s, p, gap=0.3):
    T_init(s)
    return not s.objs or not p.buffer(gap).intersects(unary_union(s.objs))

def T_group(s, polys):
    g = {}
    for p in polys:
        lo, hi = T_zr(s, p); g.setdefault((lo, hi), []).append(p)
    return g

def tprism(s, polys, h, kind):
    out = []
    for (lo, hi), ps in T_group(s, polys).items():
        top = min(hi + h, CAP)
        if top - hi < 0.25: continue
        got = s.prism(ps, top, kind, z0=lo - SINK, foot=False)
        s.feet += got; s.objs += got; out += [(g, top) for g in got]
    return out

def tstack(s, polys, h, tiers, kind):
    """Tapered stack; the first tier also climbs from the lowest to the highest ground under each piece."""
    for (lo, hi), ps in T_group(s, polys).items():
        hh_tot = min(h, CAP - hi - 0.05)
        if hh_tot < 0.25: continue
        profs = [p for p in s.clip(ps) if wide_enough(p)]
        if not profs: continue
        s.feet += profs; s.objs += profs
        z = lo - SINK; first = True
        if CFG.get("chamfer_rocks", True) and CFG.get("base_first", True):
            s._solids(profs, z, hi + hh_tot, hh_tot, tiers, kind); continue      # one solid per piece, chamfered
        for frac, a in tiers:
            hh = frac * hh_tot + ((hi - lo) + SINK if first else 0.03)
            d = hh * math.tan(math.radians(a)) + 0.03
            ok = [q for q in (fit_for_taper(p, d) for p in profs) if q is not None]
            if not ok: break
            s.ops.append(("taper", ok, round(z, 4), round(hh, 4), a, kind))
            profs = [g for q in ok for g in polys_of(inset(q, hh * math.tan(math.radians(a)) + 0.05)) if g.area > 0.1]
            z += hh - 0.03; first = False

def tcut_flat(s, polys, depth, kind="pit"):
    """Cut pits/marks into the local flat surface; skips anything on a slope, in a hollow or under a feature."""
    T_init(s); block = unary_union(s.objs).buffer(0.3) if s.objs else Polygon()
    g = {}
    for p in polys:
        if p.intersects(block): continue
        z = T_flat(s, p)
        if z is not None: g.setdefault(z, []).append(p)
    for z, ps in g.items(): s.tcut(ps, z - depth, z + 0.3, kind)

def scatter_any(s, n, rmin, rmax, region=None, gap=0.3, round_=True, tries=None):
    """Like scatter, but only avoids features (not terrain)."""
    T_init(s); rng = s.rng
    reg = (region if region is not None else Point(0, 0).buffer(R0 - 0.4, 128)).buffer(-0.2)
    busy = unary_union(s.objs + s.keepout).buffer(gap) if (s.objs or s.keepout) else Polygon()
    got = []; n = gg.area_count(n, region)
    for _ in range(tries or n * 60):
        if len(got) >= n: break
        r = rng.uniform(rmin, rmax); a = rng.uniform(0, 2 * math.pi); d = math.sqrt(rng.random()) * R0
        p = stone(d * math.cos(a), d * math.sin(a), r, rng, round_=round_)
        if reg.contains(p) and not p.intersects(busy) and all(p.distance(q) > gap for q in got): got.append(p)
    return got

def rock_floor(s, avoid, slope_dir, n_cells):
    """Broken bedrock: Voronoi slabs at stepped heights that slope across the base, 0.45 mm cracks between,
    bevelled top edges. ~20 % of cells stay as packed-earth patches."""
    rng = s.rng
    pts = MultiPoint([(rng.uniform(-17, 17), rng.uniform(-17, 17)) for _ in range(n_cells)])
    levels = {}
    for cell in voronoi_diagram(pts, envelope=box(-20, -20, 20, 20)).geoms:
        c = inset(cell, 0.22).difference(avoid)
        for g in polys_of(c.intersection(Point(0, 0).buffer(R0 + 2.0, 64))):
            if g.area < 3.0 or not wide_enough(g, 1.6) or rng.random() < 0.2: continue
            cx, cy = g.centroid.x, g.centroid.y
            lv = 0.75 + 0.45 * (cx * math.cos(slope_dir) + cy * math.sin(slope_dir)) / 14 + rng.uniform(-0.3, 0.3)
            lv = min(1.5, max(0.3, round(lv / 0.25) * 0.25))
            levels.setdefault(lv, []).append(g.simplify(0.05))
    for lv in sorted(levels):
        got = s.prism(levels[lv], H + lv, "bedrock")
        for g in got:
            d = 0.35 * math.tan(math.radians(40)) + 0.03
            q = fit_for_taper(inset(g.intersection(Point(0, 0).buffer(R0 - 0.45, 128)), 0.15), d)
            if CFG.get("chamfer_rocks", True) and CFG.get("base_first", True):
                T_add(s, g, H, H + lv, inset(g, 0.4)); continue             # the slab itself gets a chamfered edge
            if q is not None:
                s.ops.append(("taper", [q], round(H + lv - 0.15, 4), 0.35, 40.0, "bedrock"))
                T_add(s, g, H, H + lv + 0.2, inset(q, 0.35 * math.tan(math.radians(40)) + 0.05))
            else:
                T_add(s, g, H, H + lv, inset(g, 0.05))
    return levels

def mine(n):
    """1800s mine v4: uneven broken-bedrock floor, a packed-earth cart lane (ruts / short track / planks),
    blast craters that clear the rock, crates, spoil heaps, pick marks and drill holes on the high slabs."""
    s = mk("mine", "1800s Mine", n, "#5f4a38"); rng = s.rng; T_init(s)
    F = pickF({1: "crater ruts crates heap", 2: "track crates", 3: "crater ruts marks", 4: "crater crater crates",
         5: "planks crates ruts crater", 6: "track heap marks", 7: "ruts ruts crates crater", 8: "crater beam crates marks",
         9: "track crater crates", 10: "heap ruts crates marks"}, n)
    phi = rng.uniform(0, math.pi); f = frame(0, 0, phi); off = rng.uniform(-5, 5)
    R = lambda u0, u1, v0, v1: Polygon([f(u0, v0), f(u1, v0), f(u1, v1), f(u0, v1)])
    lane = None
    if any(k in F for k in ("track", "ruts", "planks")):
        lw = 9.6 if "track" in F else 7.6
        bend = rng.uniform(-0.025, 0.025)
        lane = LineString([f(u, off + bend * u * u) for u in np.linspace(-19, 19, 30)]).buffer(lw / 2, cap_style=2)
        if F.count("ruts") == 2:
            lane = unary_union([lane, LineString([f(u, off - 6 + 0.02 * u * u) for u in np.linspace(-19, 19, 30)]).buffer(3.6, cap_style=2)])
    craters = []
    for k in range(F.count("crater")):
        for _ in range(80):
            r = rng.uniform(1.8, 2.6); a = rng.uniform(0, 2 * math.pi); d = rng.uniform(0, 9.5)
            c = Point(d * math.cos(a), d * math.sin(a))
            if (lane is None or c.distance(lane) > r + 1.0) and all(c.distance(q[0]) > r + q[1] + 2.5 for q in craters):
                craters.append((c, r)); break
    avoid = unary_union(([lane.buffer(0.3)] if lane is not None else []) + [c.buffer(r + 1.6) for c, r in craters]) if (lane is not None or craters) else Polygon()
    rock_floor(s, avoid, rng.uniform(0, 2 * math.pi), rng.randint(26, 36))
    # cart lane contents (flat packed earth)
    if lane is not None:
        if "track" in F:
            ue = rng.uniform(-3, 4); g2 = 3.0
            s.objs += s.prism([R(ue - 0.6, ue + 1.4, off - 4.4, off + 4.4)], H + 1.6, "wood")
            tp = [affinity.rotate(R(u - 0.85, u + 0.85, off - 4.2, off + 4.2), rng.uniform(-5, 5), origin=f(u, off))
                  for u in np.arange(ue + 3.0, 18, 3.3) if rng.random() > 0.15]
            s.objs += s.prism([t for t in tp if Point(0, 0).buffer(R0 - 0.2).contains(t)], H + 0.7, "wood")
            s.objs += s.prism([R(ue + 0.3, 20, off - g2 - 0.45, off - g2 + 0.45), R(ue + 0.3, 20, off + g2 - 0.45, off + g2 + 0.45)], H + 1.25, "iron")
        if "planks" in F:
            fl = s.prism([R(-20, 20, off - 3.3, off + 3.3).intersection(lane.buffer(-0.4))], H + 0.6, "wood")
            if fl:
                seams = [R(-20, 20, off - 3.3 + 2.2 * k - 0.15, off - 3.3 + 2.2 * k + 0.15) for k in (1, 2)]
                for k in range(3):
                    for u in np.arange(-18 + rng.uniform(0, 6), 18, rng.uniform(7, 10)):
                        seams.append(R(u - 0.15, u + 0.15, off - 3.3 + 2.2 * k + 0.1, off - 3.3 + 2.2 * (k + 1) - 0.1))
                s.tcut([unary_union(seams).intersection(inset(fl[0], 0.3))], H + 0.25, H + 0.6, "crack")
                s.objs += fl
        for r_ in range(F.count("ruts")):
            o2 = off if r_ == 0 else off - 6
            bb = bend if r_ == 0 else 0.02
            ruts = [LineString([f(u, o2 + o + bb * u * u) for u in np.linspace(-19, 19, 30)]).buffer(0.3, cap_style=2) for o in (-2.5, 2.5)]
            s.gcut([unary_union(ruts).intersection(lane.buffer(-0.2))], 0.45, "rut")
    for c, r in craters: crater_t(s, c.x, c.y, r)
    if "beam" in F:
        for _ in range(40):
            a = rng.uniform(0, math.pi); d = rng.uniform(-6, 6); ff = frame(d * math.cos(a + 1.57), d * math.sin(a + 1.57), a)
            bm = Polygon([ff(-5.5, -0.9), ff(5.5, -0.9), ff(5.5, 0.9), ff(-5.5, 0.9)])
            if T_free(s, bm, 0.5) and Point(0, 0).buffer(R0 - 0.4).contains(bm):
                got = tprism(s, [bm], 1.0, "wood")
                if got: s.tcut([line_cut(ff(-5, v), ff(5, v), 0.35) for v in (-0.35, 0.35)], got[0][1] - 0.3, got[0][1] + 0.5, "crack")
                break
    if "heap" in F:
        a = rng.uniform(0, 2 * math.pi); hx, hy = 11 * math.cos(a), 11 * math.sin(a)
        heap = [stone(hx + rng.uniform(-3, 3), hy + rng.uniform(-3, 3), rng.uniform(1.2, 2.2), rng, .8, 6) for _ in range(rng.randint(5, 8))]
        heap = [h for h in heap if T_free(s, h, 0.4)]
        if heap: tstack(s, [unary_union(heap).intersection(Point(0, 0).buffer(R0 - 0.6, 128))], 2.0, ((0.4, 10), (0.3, 25), (0.3, 48)), "rock")
    for k in range(F.count("crates") and rng.randint(1, 3)):
        for _ in range(80):
            a = rng.uniform(0, 2 * math.pi); d = rng.uniform(2, 10); w, l = rng.uniform(2.6, 3.4), rng.uniform(3.2, 4.4)
            cx, cy = d * math.cos(a), d * math.sin(a); ang = rng.uniform(0, math.pi)
            fp = Polygon([frame(cx, cy, ang)(x, y) for x, y in ((-l / 2, -w / 2), (l / 2, -w / 2), (l / 2, w / 2), (-l / 2, w / 2))])
            lo, hi = T_zr(s, fp)
            if T_free(s, fp, 0.3) and Point(0, 0).buffer(R0 - 0.5).contains(fp) and hi - lo <= 0.8 and hi <= H + 1.2:
                got = tprism(s, [fp], rng.uniform(1.3, 1.6), "crate")
                if got:
                    k_ = max(2, int(round(l / 1.3))); ff = frame(cx, cy, ang); top = got[0][1]
                    s.tcut([line_cut(ff(-l / 2 + l * i / k_, -w / 2 - 0.6), ff(-l / 2 + l * i / k_, w / 2 + 0.6), 0.35) for i in range(1, k_)], top - 0.3, top + 0.5, "crack")
                break
    # loose rocks + gravel everywhere (also on the slabs and in the cracks)
    tstack(s, scatter_any(s, rng.randint(4, 8), 0.9, 1.6, gap=0.35, round_=False), 0.7, ((0.7, 6), (0.3, 40)), "rock")
    tstack(s, scatter_any(s, rng.randint(35, 55), 0.48, 0.72, gap=0.3), 0.35, ((0.7, 10), (0.3, 45)), "grit")
    if "marks" in F:     # pick marks + drill-hole patterns on the highest slabs
        tops = sorted([e for e in s.terr if e[2] > H + 1.0], key=lambda e: -e[3].area)[:3]
        marks, holes = [], []
        for fp, zb, zt, top in tops:
            core = inset(top, 0.3)
            if core.is_empty: continue
            x0, y0, x1, y1 = core.bounds; px, py = rng.uniform(x0, x1), rng.uniform(y0, y1); aa = rng.uniform(0, math.pi); ff = frame(px, py, aa)
            for k in range(3):
                m = line_cut(ff(-0.8, k * 0.9), ff(0.8, k * 0.9 + rng.uniform(-.15, .15)), 0.4)
                if core.contains(m): marks.append(m)
            c = top.centroid
            for i in range(2):
                for j in range(2):
                    h_ = Point(c.x + (i - 0.5) * 1.8, c.y + (j - 0.5) * 1.8).buffer(0.5, 10)
                    if core.contains(h_) and not any(h_.intersects(m) for m in marks): holes.append(h_)
        tcut_flat(s, marks, 0.35, "crack"); tcut_flat(s, holes, 0.9, "pit")
    return s

def crater_t(s, cx, cy, r):
    rng = s.rng
    hole = stone(cx, cy, r, rng, rng.uniform(.75, .95), 9)
    s.gcut([hole], 0.8 if math.hypot(cx, cy) > 4 else 0.7, "pit"); s.keepout.append(hole.buffer(0.2))
    lip, cracks = [], []
    for k in range(rng.randint(7, 11)):
        a = rng.uniform(0, 2 * math.pi); d = r + rng.uniform(0.3, 0.9)
        lip.append(stone(cx + d * math.cos(a), cy + d * math.sin(a), rng.uniform(0.6, 1.0), rng, .8, 5))
    for k in range(rng.randint(3, 5)):
        a = rng.uniform(0, 2 * math.pi)
        p0 = (cx + r * 0.8 * math.cos(a), cy + r * 0.8 * math.sin(a)); p1 = (cx + (r + 1.5) * math.cos(a), cy + (r + 1.5) * math.sin(a))
        cracks.append(LineString(wiggle(p0, p1, rng, 0.2)).buffer(0.22, cap_style=1, resolution=4))
    lip = [g for g in polys_of(unary_union(lip)) if Point(0, 0).buffer(R0 - 0.3).contains(g)]
    tstack(s, lip, 0.75, ((0.65, 8), (0.35, 40)), "rock")
    s.gcut([unary_union(cracks)], 0.4, "crack")    # cracks run from the pit out under the lip rocks / into the slabs

def swells(s, blobs, dz=0.2, step=0.55):
    """Smooth rolling ground. Each swell is a rounded mound with a real sloped surface: a few sloped bands one inside
    the other, steep at the foot and easing to a flat crown, with no flat steps between them (each band starts exactly
    where the one below ends). Overlapping swells merge into one outline first, so they flow into each other.
    blobs: (polygon, n) where n sets how high that swell may grow (0.2 mm per unit, as before)."""
    hulls = [(b.convex_hull.buffer(0.5, 16).simplify(0.03), nt) for b, nt in blobs]
    if not hulls: return
    nmax = max(nt for _, nt in hulls); Ht = nmax * dz; D = nmax * step              # full height and full run of the tallest swell
    m = 14 if gg.MESH else max(3, min(5, round(nmax / 1.5)))                        # sloped bands from foot to crown (many and fine in the mesh builder: a smooth curve)
    prof = lambda u: 1 - (1 - u) ** 1.8                                             # steeper at the foot, flatter near the top
    g = unary_union([b for b, nt in hulls]).intersection(Point(0, 0).buffer(R0 + 0.15, 256))
    cur = [p.simplify(0.02) for p in polys_of(g) if p.area > 1.0 and wide_enough(p, 1.2)]
    for k in range(m):
        if not cur: break
        z0, z1 = Ht * prof(k / m), Ht * prof((k + 1) / m); dd = D / m; hh = z1 - z0
        if hh < (0.02 if gg.MESH else 0.1): break                                   # the crown: leave it flat
        ang = math.degrees(math.atan(dd / hh))
        ok = [q for q in (fit_for_taper(p, dd + 0.03) for p in cur) if q is not None]
        if not ok: break
        sink = SINK if k == 0 else gg.LAP
        s.ops.append(("taper", ok, round(H + z0 - sink, 4), round(hh + sink, 4), round(ang, 3), "dune"))
        tops = [t_ for q in ok for t_ in polys_of(inset(q, dd))]
        T_add(s, unary_union(ok), H + z0, H + z1, unary_union(tops) if tops else Polygon())
        if k == 0: s.feet += ok
        cur = [p for q in ok for p in polys_of(inset(q, dd + gg.LEDGE)) if p.area > 1.0 and wide_enough(p, 1.2)]

def hollow(s, poly):
    """Shallow wind-scoured hollow: three nested cuts 0.2 / 0.4 / 0.6 mm."""
    for k, dpt in enumerate((0.2, 0.4, 0.6)):
        g = inset(poly, k * 0.6)
        if not g.is_empty and wide_enough(g, 1.2): s.gcut([g], dpt, "hollow")
    s.keepout.append(poly.buffer(0.3))

def maahl(n):
    """Deserts of Maahl v4: rolling sand (smooth swells, a hollow), plus dune, pitted flats, pebble pavement,
    cracked hardpan, creature tracks, sandstone outcrops and bubbly pebbles placed on the uneven surface."""
    s = mk("maahl", "Deserts of Maahl", n, "#c9a36c"); rng = s.rng; T_init(s)
    F = pickF({1: "dune pits outcrop", 2: "pavement outcrop2 trail", 3: "hardpan dune pebbles", 4: "pits pavement outcrop hollow",
         5: "dune trail pebbles", 6: "outcrop2 pits hardpan", 7: "dune pavement pits hollow", 8: "trail outcrop pits",
         9: "hardpan pavement pebbles outcrop hollow", 10: "dune outcrop pits pebbles"}, n)
    th = rng.uniform(0, 2 * math.pi)
    band = None
    if "trail" in F:
        tphi, toff = rng.uniform(0, math.pi), rng.uniform(-6, 6)
        band = LineString([frame(0, 0, tphi)(u, toff) for u in (-20, 20)]).buffer(2.4, cap_style=2)
    hol = None
    if "hollow" in F:
        for _ in range(60):
            a = rng.uniform(0, 2 * math.pi); d = rng.uniform(1, 7)
            g = affinity.scale(Point(d * math.cos(a), d * math.sin(a)).buffer(rng.uniform(3.2, 4.2), 32), 1.0, rng.uniform(.7, 1.0))
            if band is None or not g.intersects(band.buffer(0.5)): hol = g; break
    # rolling ground: 2-4 broad swells, some running off the rim
    blobs = []
    for k in range(rng.randint(2, 4)):
        a = th + k * 2 * math.pi / 3 + rng.uniform(-.5, .5); d = rng.uniform(5, 13)
        cx, cy = d * math.cos(a), d * math.sin(a)
        b = unary_union([Point(cx + rng.uniform(-3, 3), cy + rng.uniform(-3, 3)).buffer(rng.uniform(3.5, 6.0), 32) for _ in range(3)])
        b = b.buffer(1.0).buffer(-1.0)
        for kk in [x for x in (band, hol) if x is not None]: b = b.difference(kk.buffer(1.0))
        b = max(polys_of(b), key=lambda q: q.area) if polys_of(b) else None
        if b is not None and b.area > 12: blobs.append((b.simplify(0.05), rng.randint(4, 8)))
    if "dune" in F:      # a long dune banked on the rim: three overlapping mounds along an arc
        for t in (-0.45, 0, 0.45):
            a2 = th + math.pi + t; cx, cy = 11.5 * math.cos(a2), 11.5 * math.sin(a2)
            g = affinity.rotate(affinity.scale(Point(cx, cy).buffer(rng.uniform(3.8, 4.6), 32), 1.0, 0.65), math.degrees(a2) + 90, origin=(cx, cy))
            for kk in [x for x in (band, hol) if x is not None]: g = g.difference(kk.buffer(1.0))
            g = max(polys_of(g), key=lambda q: q.area) if polys_of(g) else None
            if g is not None and g.area > 8: blobs.append((g, rng.randint(8, 10)))
    if blobs: swells(s, blobs)
    if hol is not None: hollow(s, hol)
    if band is not None: s.keepout.append(band)
    rocks = (["outcrop"] if "outcrop" in F else []) + (["big", "small"] if "outcrop2" in F else [])
    for kind in rocks:
        for _ in range(80):
            r = {"outcrop": rng.uniform(3.0, 3.8), "big": 3.6, "small": 2.2}[kind]
            a = rng.uniform(0, 2 * math.pi); d = rng.uniform(3, 9.5); cx, cy = d * math.cos(a), d * math.sin(a)
            g = unary_union([Point(cx + rng.uniform(-r, r) * .6, cy + rng.uniform(-r, r) * .6).buffer(r * rng.uniform(.5, .8), 12) for _ in range(4)])
            g = g.buffer(0.8).buffer(-0.8).simplify(0.05)
            lo, hi = T_zr(s, g)
            if T_free(s, g, 0.8) and not any(g.intersects(k) for k in s.keepout) and Point(0, 0).buffer(R0 - 0.4).contains(g) and hi - lo < 1.0:
                tstack(s, [g], 2.6 if kind != "small" else 1.5, ((0.35, 8), (0.25, 20), (0.25, 38), (0.15, 60)), "sandstone"); break
    if "hardpan" in F:   # cracked clay plate, only on flat ground
        for _ in range(100):
            a = rng.uniform(0, 2 * math.pi); d = rng.uniform(1, 9); reg = Point(d * math.cos(a), d * math.sin(a)).buffer(rng.uniform(3.2, 4.6), 32)
            z = T_flat(s, reg)
            if z is not None and T_free(s, reg, 0.6) and Point(0, 0).buffer(R0 - 0.5).contains(reg):
                pan = s.prism([reg], z + 0.35, "pan", z0=z - SINK, foot=False); s.feet += pan; s.objs += pan
                if pan:
                    pts = MultiPoint([(rng.uniform(-17, 17), rng.uniform(-17, 17)) for _ in range(60)]); edges = set()
                    for cell in voronoi_diagram(pts, envelope=box(-20, -20, 20, 20)).geoms:
                        c = [tuple(round(v, 3) for v in xy) for xy in cell.exterior.coords]
                        for a_, b_ in zip(c, c[1:]): edges.add(tuple(sorted((a_, b_))))
                    net = unary_union([LineString(wiggle(a_, b_, rng, 0.25)).buffer(0.2, cap_style=1, resolution=4) for a_, b_ in edges])
                    s.tcut([net.intersection(pan[0].buffer(0.6))], z + 0.05, z + 0.8, "crack")
                break
    if "pebbles" in F:
        for c in range(rng.randint(2, 3)):
            a = rng.uniform(0, 2 * math.pi); d = rng.uniform(1, 11); reg = Point(d * math.cos(a), d * math.sin(a)).buffer(2.2)
            if not T_free(s, reg, 0.4) or any(reg.intersects(k) for k in s.keepout): continue
            tstack(s, scatter_any(s, 9, 0.5, 0.75, region=reg, gap=0.2, tries=2000), 0.5, ((0.6, 15), (0.4, 50)), "pebble")
    if "pavement" in F:
        for _ in range(60):
            a = rng.uniform(0, 2 * math.pi); d = rng.uniform(2, 9); reg = Point(d * math.cos(a), d * math.sin(a)).buffer(rng.uniform(3.5, 4.8), 24)
            if T_free(s, reg, 0.5) and not any(reg.intersects(k) for k in s.keepout) and Point(0, 0).buffer(R0 - 0.3).contains(reg):
                tstack(s, scatter_any(s, 40, 0.48, 0.7, region=reg, gap=0.18, tries=4000), 0.3, ((0.75, 10), (0.25, 45)), "pebble"); break
    tstack(s, scatter_any(s, rng.randint(10, 20), 0.5, 0.75, gap=0.4), 0.4, ((0.7, 10), (0.3, 45)), "pebble")
    if band is not None:  # creature tracks along the flat band, rim to rim
        s.keepout.remove(band)
        s.gcut([print_trail(s, tphi, toff, rng)], 0.4, "pit")
    pits = []; full = "pits" in F
    for _ in range(4000):
        if len(pits) >= (rng.randint(55, 80) if full else rng.randint(18, 28)): break
        a = rng.uniform(0, 2 * math.pi); d = math.sqrt(rng.random()) * (R0 - 0.6); r = rng.uniform(0.45, 0.85) if full else rng.uniform(0.45, 0.6)
        p = affinity.scale(Point(d * math.cos(a), d * math.sin(a)).buffer(r, 10), 1.0, rng.uniform(.75, 1.0))
        if all(p.distance(q) > 0.45 for q in pits) and (band is None or not p.intersects(band)): pits.append(p)
    tcut_flat(s, pits, 0.35, "pit")
    return s


# ======================================================================= Urban Warfare #11-20 (more street detail)
def urban_x(n):
    """Urban 11-20: curb + sidewalk (paving cut in) + road, each base with a different mix of street pieces:
    manhole cover, shell crater, crosswalk, tram rails, cobblestone band, repair patch, tree grate,
    utility cover, gutter, broken curb, storm drain."""
    s = mk("urban", "Urban Warfare", n, "#7e7b76"); rng = s.rng
    F = pickF({11: "manhole crosswalk treegrate", 12: "crater brokencurb utility", 13: "tram gutter treegrate", 14: "cobble manhole utility",
         15: "crater crosswalk drain", 16: "tram repair brokencurb", 17: "cobble crater treegrate", 18: "manhole gutter repair utility",
         19: "crosswalk drain brokencurb", 20: "tram crater utility",
         21: "manhole repair treegrate", 22: "crater gutter utility", 23: "cobble crosswalk brokencurb"}, n)
    phi = rng.uniform(0, math.pi); f = frame(0, 0, phi); d = (rng.uniform(1.0, 4.0) if "tram" not in F else rng.uniform(4.5, 6.0)) * gg.K
    R = lambda u0, u1, v0, v1: Polygon([f(u0, v0), f(u1, v0), f(u1, v1), f(u0, v1)])
    # curb: stones edge to edge, one knocked out on "brokencurb" bases (rubble lies in the gap)
    us = list(np.arange(-19.5 + rng.uniform(0, 2), 19, 4.5))
    gap_u = None
    if "brokencurb" in F:
        mid = [u for u in us if -8 < u < 6]; gap_u = rng.choice(mid) if mid else None
    s.stack([R(u, u + 4.0, d, d + 1.8) for u in us if u != gap_u], 1.0, ((0.8, 3), (0.2, 40)), "curb")
    slab = s.prism([R(-20, 20, d + 1.8, 20)], H + 0.75, "slab")
    sl = unary_union(slab) if slab else Polygon()
    covers = []                      # sidewalk pieces the paving joints must stay clear of
    def on_walk(w, l):
        for _ in range(80):
            u = rng.uniform(-9, 9); v = d + 2.3 + rng.uniform(w / 2 + 0.6, 9)
            c = R(u - l / 2, u + l / 2, v - w / 2, v + w / 2)
            if sl.buffer(-0.6).contains(c) and Point(0, 0).buffer(R0 - 0.8).contains(c) and not any(c.buffer(0.6).intersects(q) for q in covers):
                return u, v, c
        return None
    if "treegrate" in F:             # square tree grate: frame with parallel slots
        got = on_walk(5.0, 5.0)
        if got:
            u, v, c = got
            s.tcut([R(u - 2.0, u + 2.0, v + y - 0.25, v + y + 0.25) for y in np.arange(-1.8, 1.9, 0.9)], H + 0.15, H + 1.2, "slot")
            covers.append(c)
    if "utility" in F:               # small utility cover: groove round the lid + two lift holes
        got = on_walk(3.2, 3.2)
        if got:
            u, v, c = got
            ring = R(u - 1.6, u + 1.6, v - 1.6, v + 1.6).difference(R(u - 1.15, u + 1.15, v - 1.15, v + 1.15))
            s.tcut([ring] + [Point(*f(u + x, v)).buffer(0.3, 10) for x in (-0.6, 0.6)], H + 0.4, H + 1.2, "slot")
            covers.append(c)
    # paving joints, kept clear of the covers
    pv = n % 3; core = sl.difference(R(-20, 20, d - 0.5, d + 2.3)).difference(unary_union(covers).buffer(0.35) if covers else Polygon())
    if pv == 2:
        g = affinity.rotate(unary_union([box(-20, y - 0.22, 20, y + 0.22) for y in np.arange(-20, 20, 3.6)]), math.degrees(phi) + 45, origin=(0, 0))
        x = affinity.rotate(unary_union([box(y - 0.22, -20, y + 0.22, 20) for y in np.arange(-20, 20, 3.6)]), math.degrees(phi) + 45, origin=(0, 0))
    elif pv == 0:
        L, w = 3.2, 1.55
        g = unary_union([R(-20, 20, d + 2.3 + j * w - 0.22, d + 2.3 + j * w + 0.22) for j in range(1, 14)])
        x = unary_union([R(i * L + (j % 2) * L / 2 - 0.22, i * L + (j % 2) * L / 2 + 0.22, d + 2.3 + j * w + 0.1, d + 2.3 + (j + 1) * w - 0.1)
                         for j in range(0, 14) for i in range(-8, 9)])
    else:
        w = 3.2
        g = unary_union([R(u - 0.22, u + 0.22, -20, 20) for u in np.arange(-20, 20, w)])
        x = unary_union([R(u + 0.1, u + w - 0.1, d + 2.3 + j * 3.2 + (k % 2) * 1.6 - 0.22, d + 2.3 + j * 3.2 + (k % 2) * 1.6 + 0.22)
                         for k, u in enumerate(np.arange(-20, 20, w)) for j in range(-1, 6)])
    s.tcut([unary_union([g, x]).intersection(core)], H + 0.33, H + 0.75, "joint")     # one connected joint net: cuts in well under a second
    road = R(-20, 20, -20, d - 0.3)
    busy = []                        # road pieces placed so far (footprints)
    def road_free(p, gap=0.6):
        return road.buffer(-0.4).contains(p) and Point(0, 0).buffer(R0 - 0.8).contains(p) and \
            not any(p.buffer(gap).intersects(q) for q in busy) and s.free(p, gap)
    if "gutter" in F and "drain" not in F:   # shallow gutter channel along the curb
        s.gcut([R(-20, 20, d - 1.1, d - 0.3)], 0.3, "slot"); busy.append(R(-20, 20, d - 1.4, d))
    if "drain" in F:
        u0 = rng.uniform(-6, 2)
        s.prism([R(u0, u0 + 6.0, d - 2.7, d + 0.1)], H + 0.3, "metal")
        s.tcut([R(u0 + 0.8 + i * 1.1, u0 + 1.3 + i * 1.1, d - 2.1, d - 0.5) for i in range(4)], H - 0.5, H + 1.0, "slot")
        busy.append(R(u0, u0 + 6.0, d - 2.7, d + 0.1))
    if "tram" in F:                  # tram rails flush with the road: rail heads + flangeway grooves, rim to rim
        tv = d - rng.uniform(4.5, 5.5)
        for o in (-2.4, 2.4):
            s.prism([R(-20, 20, tv + o - 0.45, tv + o + 0.45)], H + 0.3, "iron")
            s.gcut([R(-20, 20, tv + o + (0.5 if o < 0 else -1.0), tv + o + (1.0 if o < 0 else -0.5))], 0.5, "slot")
        busy.append(R(-20, 20, tv - 3.2, tv + 3.2))
    if "crosswalk" in F:             # crosswalk stripes (low, for paint), across the road up to the curb
        u0 = rng.uniform(-7, 3)
        st = [R(u0 + k * 2.4, u0 + k * 2.4 + 1.4, -20, d - 0.4) for k in range(3)]
        s.prism(st, H + 0.25, "paint", minw=0.88); busy += [R(u0 - 0.5, u0 + 6.7, -20, d)]
    if "cobble" in F:                # band of old cobbles across the road (joint network cut into a low raised band)
        cv = rng.uniform(-6, d - 4)
        band = s.prism([R(-20, 20, cv - 2.2, cv + 2.2)], H + 0.35, "cobble")
        if band:
            pts = MultiPoint([(rng.uniform(-17, 17), rng.uniform(-17, 17)) for _ in range(260)]); edges = set()
            for cell in voronoi_diagram(pts, envelope=box(-20, -20, 20, 20)).geoms:
                c = [tuple(round(v_, 3) for v_ in xy) for xy in cell.exterior.coords]
                for a_, b_ in zip(c, c[1:]): edges.add(tuple(sorted((a_, b_))))
            net = unary_union([LineString([a_, b_]).buffer(0.21, cap_style=1, resolution=3) for a_, b_ in edges])
            s.tcut([net.intersection(inset(band[0], 0.3))], H + 0.05, H + 0.8, "slot")
        busy.append(R(-20, 20, cv - 2.6, cv + 2.6))
    if "manhole" in F:               # round cover: outer ring groove, inner ring groove, cross-hatch
        for _ in range(80):
            cx, cy = f(rng.uniform(-8, 8), rng.uniform(-11, d - 3.5)); c = Point(cx, cy).buffer(2.9, 48)
            if road_free(c):
                rings = Point(cx, cy).buffer(2.8, 48).difference(Point(cx, cy).buffer(2.35, 48))
                inner = Point(cx, cy).buffer(1.8, 48).difference(Point(cx, cy).buffer(1.45, 48))
                hatch = unary_union([LineString([f(-3, y), f(3, y)]).buffer(0.2, cap_style=2) for y in (-0.6, 0, 0.6)] +
                                    [LineString([f(y, -3), f(y, 3)]).buffer(0.2, cap_style=2) for y in (-0.6, 0, 0.6)])
                hatch = affinity.translate(hatch, cx, cy).intersection(Point(cx, cy).buffer(1.1, 32))
                s.gcut([rings, inner], 0.4, "slot"); s.gcut([hatch], 0.3, "slot"); busy.append(c); break
    if "repair" in F:                # sunk asphalt repair patch
        for _ in range(80):
            cx, cy = f(rng.uniform(-8, 8), rng.uniform(-10, d - 3)); a = rng.uniform(-0.3, 0.3)
            p = affinity.rotate(Polygon([(cx - 3, cy - 1.8), (cx + 3, cy - 1.8), (cx + 3, cy + 1.8), (cx - 3, cy + 1.8)]), math.degrees(phi + a), origin=(cx, cy))
            if road_free(p): s.gcut([p], 0.25, "patch"); busy.append(p); break
    if "crater" in F:                # shell crater with broken-asphalt lip
        for _ in range(80):
            cx, cy = f(rng.uniform(-7, 7), rng.uniform(-10, d - 4.5)); r = rng.uniform(1.8, 2.5)
            if road_free(Point(cx, cy).buffer(r + 2.2), 0.3):
                crater(s, cx, cy, r, 0.8 if math.hypot(cx, cy) > 4 else 0.7); busy.append(Point(cx, cy).buffer(r + 2.2)); break
    if gap_u is not None:            # rubble from the knocked-out curb stone
        rub = [stone(*f(gap_u + rng.uniform(0.5, 3.5), d + rng.uniform(0.3, 1.5)), rng.uniform(0.6, 0.9), rng, .8, 5) for _ in range(3)]
        rub = [p for p in rub if s.free(p, 0.3)]
        if rub: s.stack([unary_union(rub)], 0.8, ((0.6, 4), (0.4, 30)), "rubble")
    for p in s.scatter(rng.randint(2, 4), 1.1, 1.8, region=road, gap=0.5, round_=False):
        if not any(p.buffer(0.5).intersects(q) for q in busy):
            s.stack([p], rng.uniform(0.8, 1.2), ((0.6, 4), (0.4, 30)), "rubble")
    # cracks from the curb, ending at a piece already on the road or at the rim
    crk = []
    ends = [q.centroid for q in busy if q.area < 60] or [None]
    for k in range(rng.randint(1, 3)):
        x, y = f(rng.uniform(-8, 8), d - 0.3)
        e = ends[k % len(ends)]; tx, ty = (e.x, e.y) if e is not None and k == 0 else f(rng.uniform(-14, 14), d - 30)
        pts = [(x, y)] + [(x + (tx - x) * t + rng.uniform(-.8, .8), y + (ty - y) * t + rng.uniform(-.8, .8)) for t in np.linspace(0, 1, 9)[1:-1]] + [(tx, ty)]
        crk.append(LineString(pts).buffer(0.22, cap_style=1, resolution=4))
    s.gcut([unary_union(crk).intersection(road.buffer(0.3))], 0.45, "crack")
    reg = road.difference(unary_union(busy).buffer(0.3)) if busy else road
    s.stack(s.scatter(rng.randint(20, 40), 0.47, 0.62, region=reg, gap=0.35), 0.25, ((1.0, 20),), "grit")
    return s

# ======================================================================= urban street layouts (beyond one straight curb)
def curb_blocks(walk, width=1.8, seg=4.5, disc=None):
    """Curb stones all the way round a sidewalk region (follows rounded corners), cut into blocks."""
    band = walk.buffer(width, 24).difference(walk)
    if disc is not None: band = band.intersection(disc)
    gaps = []
    for ring in polys_of(walk):
        ext = ring.exterior; L = ext.length; k = max(1, int(L // seg))
        for i in range(k):
            t = i * L / k; p = ext.interpolate(t); q = ext.interpolate(t + 0.05)
            tx, ty = q.x - p.x, q.y - p.y; n_ = math.hypot(tx, ty) or 1; tx, ty = tx / n_, ty / n_
            nx, ny = -ty, tx
            c = (p.x, p.y)
            gaps.append(Polygon([(c[0] + tx * 0.22 + nx * s_, c[1] + ty * 0.22 + ny * s_) for s_ in (-2.6, 2.6)] +
                                [(c[0] - tx * 0.22 + nx * s_, c[1] - ty * 0.22 + ny * s_) for s_ in (2.6, -2.6)]))
    blocks = band.difference(unary_union(gaps)) if gaps else band
    return [b for b in polys_of(blocks) if b.area > 1.5]

def paving(s, walk, phi, kind, z=H + 0.75, step=3.6):
    """Paving joints cut into a sidewalk slab: running bond or square flags, rotated with the street."""
    core = walk.buffer(-0.6)
    if core.is_empty: return
    if kind == "flags":
        g = unary_union([box(-22, y - 0.22, 22, y + 0.22) for y in np.arange(-22, 22, step)])
        x = unary_union([box(y - 0.22, -22, y + 0.22, 22) for y in np.arange(-22, 22, step)])
        g = affinity.rotate(g, math.degrees(phi) + 45, origin=(0, 0)); x = affinity.rotate(x, math.degrees(phi) + 45, origin=(0, 0))
    else:
        L, w = 3.2, 1.55
        g = unary_union([box(-22, -22 + j * w - 0.22, 22, -22 + j * w + 0.22) for j in range(30)])
        x = unary_union([box(-22 + i * L + (j % 2) * L / 2 - 0.22, -22 + j * w + 0.1, -22 + i * L + (j % 2) * L / 2 + 0.22, -22 + (j + 1) * w - 0.1)
                         for j in range(30) for i in range(15)])
        g = affinity.rotate(g, math.degrees(phi), origin=(0, 0)); x = affinity.rotate(x, math.degrees(phi), origin=(0, 0))
    s.tcut([unary_union([g, x]).intersection(core)], z - 0.42, z, "joint")

def manhole(s, cx, cy, r=2.4, z=H):
    """Round manhole cover: groove round the lid, a cross-hatch of shallow grooves and two lift holes."""
    ring = Point(cx, cy).buffer(r, 48).difference(Point(cx, cy).buffer(r - 0.4, 48))
    hatch = unary_union([box(cx - r, cy + y - 0.15, cx + r, cy + y + 0.15) for y in np.arange(-r + 0.9, r - 0.5, 0.9)]).intersection(Point(cx, cy).buffer(r - 0.75, 32))
    holes = [Point(cx + x, cy).buffer(0.25, 10) for x in (-0.9, 0.9)]
    if z <= H: s.gcut([ring], 0.45, "slot"); s.gcut([hatch.difference(unary_union(holes).buffer(0.3))], 0.2, "slot"); s.gcut(holes, 0.6, "pit")
    else:
        s.tcut([ring], z - 0.45, z + 0.3, "slot"); s.tcut([hatch.difference(unary_union(holes).buffer(0.3))], z - 0.2, z + 0.3, "slot")
        s.tcut(holes, z - 0.6, z + 0.3, "pit")

def road_detail(s, rng, road, f, hole_ok=True):
    """Potholes, cracks running into them, rubble and grit on the road surface."""
    road_in = road.intersection(Point(0, 0).buffer(R0 - 1.2))
    if road_in.is_empty or road_in.area < 20: return
    hole = None
    if hole_ok:
        x0, y0, x1, y1 = road_in.bounds
        for _ in range(80):
            cand = stone(rng.uniform(x0, x1), rng.uniform(y0, y1), rng.uniform(1.4, 2.2), rng, .8, 8)
            if road_in.buffer(-0.5).contains(cand) and s.free(cand, 0.6): hole = cand; break
    if hole is not None: s.gcut([hole], 0.7, "pothole")
    crk = []
    pts_src = [p for p in polys_of(road_in)]
    for k in range(rng.randint(1, 3)):
        P = max(pts_src, key=lambda q: q.area); x0, y0, x1, y1 = P.bounds
        a = (rng.uniform(x0, x1), rng.uniform(y0, y1))
        b = (hole.centroid.x, hole.centroid.y) if (k == 0 and hole is not None) else (rng.uniform(x0, x1), rng.uniform(y0, y1))
        crk.append(LineString(wiggle(a, b, rng, 0.6, 7)).buffer(0.22, cap_style=1, resolution=4))
    s.gcut([unary_union(crk).intersection(road.buffer(-0.3))], 0.45, "crack")
    reg = road_in.difference(hole.buffer(0.3)) if hole is not None else road_in
    for p in s.scatter(rng.randint(1, 4), 1.1, 1.8, region=reg, gap=0.5, round_=False):
        s.stack([p], rng.uniform(0.8, 1.2), ((0.6, 4), (0.4, 30)), "rubble")
    s.stack(s.scatter(rng.randint(15, 30), 0.47, 0.62, region=reg, gap=0.35), 0.25, ((1.0, 20),), "grit")

def urban_layout(n, layout):
    """Urban street layouts: corner, crossroads, median, alley, plaza."""
    s = mk("urban", "Urban Warfare", n, "#7e7b76"); rng = s.rng
    phi = rng.uniform(0, 2 * math.pi); f = frame(0, 0, phi)
    R = lambda u0, u1, v0, v1: Polygon([f(u0, v0), f(u1, v0), f(u1, v1), f(u0, v1)])
    D = Point(0, 0).buffer(R0 + 1.0, 128)
    def corner(a, b, rc, sx=1, sy=1):      # sidewalk quadrant with a rounded kerb corner
        u0, u1 = (a, 40) if sx > 0 else (-40, a); v0, v1 = (b, 40) if sy > 0 else (-40, b)
        sq = R(u0, u1, v0, v1)
        ui = (a + sx * rc); vi = (b + sy * rc)
        inner = R(min(ui, sx * 40), max(ui, sx * 40), min(vi, sy * 40), max(vi, sy * 40)).buffer(rc, 32)
        return sq.intersection(inner)
    walks = []; extra = []
    if layout == "corner":
        walks = [corner(rng.uniform(-4, 1) * gg.K, rng.uniform(-4, 1) * gg.K, rng.uniform(2.5, 4.0))]
    elif layout == "crossroads":
        a, b = rng.uniform(3.5, 5.5) * gg.K, rng.uniform(3.5, 5.5) * gg.K
        walks = [corner(a, b, rng.uniform(2.0, 3.0)), corner(-a, -b, rng.uniform(2.0, 3.0), -1, -1)]
        extra.append("crosswalk")
    elif layout == "median":
        c = rng.uniform(-3, 3) * gg.K; w = rng.uniform(4.0, 5.0) * gg.K ** 0.6
        walks = [R(-40, 40, c - w / 2, c + w / 2)]
        extra.append("planter" if rng.random() < 0.6 else "bollards")
    elif layout == "alley":
        c = rng.uniform(-2, 2) * gg.K; half = rng.uniform(3.8, 4.8) * gg.K
        walks = [R(-40, 40, c + half, 40), R(-40, 40, -40, c - half)]
        extra.append("manhole" if rng.random() < 0.6 else "drain")
    elif layout == "plaza":
        walks = []
    if layout == "plaza":
        slab = s.prism([D], H + 0.6, "slab")
        sl = unary_union(slab) if slab else Polygon()
        cx, cy = f(rng.uniform(-3, 3), rng.uniform(-3, 3))
        manhole(s, cx, cy, rng.uniform(2.2, 2.6), H + 0.6)
        keep = Point(cx, cy).buffer(3.4)
        core = sl.difference(keep)
        paving(s, core, phi, "flags", H + 0.6, step=rng.uniform(4.2, 5.0))     # big flags: a whole-base joint cut stays quick
        a0 = rng.uniform(0, 2 * math.pi)
        for k in range(rng.randint(2, 4)):     # bollards in a gentle arc
            a = a0 + k * 0.35; d = 9.0
            s.prism([Point(d * math.cos(a), d * math.sin(a)).buffer(0.75, 20)], H + 0.6 + 1.6, "post", z0=H + 0.45)
        for p in s.scatter(rng.randint(1, 3), 1.0, 1.6, region=sl.buffer(-1.0), gap=0.6, round_=False):
            s.stack([p], rng.uniform(0.7, 1.0), ((0.6, 4), (0.4, 30)), "rubble", z0=H + 0.6 - 0.15 + SINK)
        return s
    walk = unary_union(walks).intersection(D)
    road = D.difference(walk.buffer(1.8 + 0.3))
    s.stack(curb_blocks(walk, disc=D), 1.0, ((0.8, 3), (0.2, 40)), "curb")
    slab = s.prism(polys_of(walk), H + 0.75, "slab")
    sl = unary_union(slab) if slab else Polygon()
    covers = Polygon()
    if "planter" in extra:                 # concrete planter box on the median with a grass mound in it
        c = sl.centroid; box_ = Point(c.x, c.y).buffer(1.9, cap_style=3)
        box_ = affinity.rotate(box_, math.degrees(phi), origin=(c.x, c.y))
        if sl.buffer(-0.4).contains(box_):
            s.prism([box_.difference(box_.buffer(-0.5))], H + 0.75 + 1.1, "concrete", z0=H + 0.6)
            s.stack([box_.buffer(-0.55)], 0.9, ((0.6, 20), (0.4, 45)), "grass", z0=H + 0.75 - 0.15 + SINK)
            covers = box_.buffer(0.4)
    if "bollards" in extra:
        c = sl.centroid
        for k in (-1, 0, 1):
            x, y = f(*(lambda uv: (uv[0] + k * 4.0, uv[1]))(( (c.x * math.cos(phi) + c.y * math.sin(phi)), (-c.x * math.sin(phi) + c.y * math.cos(phi)) )))
            p = Point(x, y).buffer(0.7, 20)
            if sl.buffer(-0.5).contains(p): s.prism([p], H + 0.75 + 1.5, "post", z0=H + 0.6); covers = covers.union(p.buffer(0.4))
    paving(s, sl.difference(covers), phi, rng.choice(["flags", "bond"]))
    if "crosswalk" in extra:               # zebra stripes across one arm of the crossroads
        u0 = rng.uniform(-2, 2)
        st = [R(-20, 20, u0 + k * 1.6, u0 + k * 1.6 + 0.9) for k in range(-3, 3)]
        arm = R(7.5, 20, -20, 20).intersection(road.buffer(-0.4))
        s.prism([q for p in st for q in polys_of(p.intersection(arm))], H + 0.25, "paint", minw=0.6)
    if "manhole" in extra:
        cx, cy = f(rng.uniform(-6, 6), 0)
        if road.buffer(-0.6).contains(Point(cx, cy).buffer(2.4)): manhole(s, cx, cy)
    if "drain" in extra:
        u0 = rng.uniform(-6, 2); v = -(rng.uniform(3.8, 4.8)) + 1.0
        s.gcut([R(u0 + i * 1.1, u0 + i * 1.1 + 0.5, v - 1.6, v) for i in range(4)], 0.8, "slot")
    road_detail(s, rng, road, f)
    return s

URBAN_LAYOUTS = (("straight", 3), ("corner", 3), ("crossroads", 2), ("median", 1.5), ("alley", 1.5), ("plaza", 1))

def urban(n):
    rng = random.Random(f"urbanlayout{n}" + os.environ.get("SALT", "") + os.environ.get("RESEED", ""))
    lay = rng.choices([k for k, w in URBAN_LAYOUTS], [w for k, w in URBAN_LAYOUTS])[0]
    if lay == "straight": return urban_a(n) if n <= 10 else urban_x(n)
    return urban_layout(n, lay)

STYLES = {"badlands": badlands, "highland": highland, "maahl": maahl, "corridor": corridor,
          "infestation": infestation, "urban": urban, "mine": mine,
          "mine_v3": mine_v3, "maahl_v3": maahl_v3}
gg.COL.update({"wood": "#8a5a32", "iron": "#4d4f55", "quartz": "#e8e4dc", "crate": "#a8743f", "bedrock": "#7a7068", "dune": "#dcbc86", "pan": "#b89a70", "cable": "#3c3f45", "lens": "#9fd7e8", "paint": "#e8e4d8", "cobble": "#8f8a82", "rock": "#b56a4a"})
gg.CUTC.update({"hollow": "#b08e5e", "patch": "#55524e"})
gg.CUTC.update({"water": "#2d4a5a", "trough": "#a88a5c", "pit": "#3a2a1c", "rut": "#3b2a1e"})

def art(style, path):
    import matplotlib; matplotlib.use("Agg"); import matplotlib.pyplot as plt
    fig, axs = plt.subplots(2, 5, figsize=(20, 8.6))
    for n, ax in zip(range(1, 11), axs.flat): render(STYLES[style](n), ax)
    plt.tight_layout(); fig.savefig(path, dpi=80); plt.close(fig)

def build(style, n):
    raise SystemExit("use run_set.py / gg3.py build")
    s = STYLES[style](n); t = time.time(); S.tool("new_design", {"name": s.name}); skipped = []
    cnt = [0]
    def join_split(op, polys):
        halves = [polys[:len(polys) // 2], polys[len(polys) // 2:]] if len(polys) > 1 else [polys]
        for h in halves:
            if not h: continue
            cnt[0] += 1; nm = f"X{cnt[0]}"
            if op[0] == "prism": e = {"sketch": nm, "distance": round(op[3] - op[2], 4), "operation": "join", "targets": ["Base"], "name": "F" + nm}
            else: e = {"sketch": nm, "distance": op[3], "taper": -op[4], "operation": "join", "targets": ["Base"], "name": "F" + nm}
            r = S.tool("batch", {"commands": sk(nm, op[2], h) + [{"command": "solid.extrude", "params": e}], "verbose": False})
            if r["isError"]:
                if len(h) == 1: skipped.append(op[-1])
                else: join_split(op, h)
    for g in gg.build_cmds(s):
        r = S.tool("batch", {"commands": g, "verbose": False})
        if r["isError"]:
            nm = g[-1]["params"].get("name", "?")
            if nm in ("TrimF", "MagCut") or g[0]["params"]["name"] == "Blank": return {"name": s.name, "error": S.txt(r, 300)}
            if nm.startswith("FJ"): join_split(s.ops[int(nm[2:])], s.ops[int(nm[2:])][1])
            else: skipped.append(nm)
    D = os.environ.get("SETDIR", f"{OUT}/set70"); os.makedirs(f"{D}/{style}", exist_ok=True)
    S.tool("export", {"path": f"{D}/{style}/{s.name}.stl"})
    S.shot(f"{D}/{style}/{s.name}_iso.png", view="iso", width=600, height=470, grid=False, axes=False)
    return {"name": s.name, "secs": round(time.time() - t), "skipped": skipped}

if __name__ == "__main__":
    if sys.argv[1] == "art": art(sys.argv[2], sys.argv[3])
    elif sys.argv[1] == "build": print(json.dumps(build(sys.argv[2], int(sys.argv[3]))), flush=True)
