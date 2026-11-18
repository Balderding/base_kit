"""2D concept sheets: 20 bases per style, four tiers of 5, each tier adds a new layer of detail on top of the last.
Tier 1 = the layouts we already built. Tiers 2-4 add features through the same Scene API, so they stay buildable.
Usage: python3 gg3.py <style|all> <outdir>
"""
import sys, math, random, os, json
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import numpy as np
import sizefix
gg = sizefix.load("gg"); gg2 = sizefix.load("gg2")        # layouts follow the base size (see sizefix.py)
from gg import R0, H, SINK, polys_of, inset, wide_enough, frame, stone, star, wiggle, render
from gg2 import line_cut, crater, tprism, tstack, tcut_flat, T_zr, T_flat, T_init
from shapely.geometry import Polygon, Point, LineString, MultiPoint, box
from shapely.ops import unary_union, voronoi_diagram
from shapely import affinity
from kit_settings import CFG

CAPZ = H + gg.max_height()   # tallest point above the 4 mm deck (half as much again on 60 mm bases and up)
HBOOST = [1.0]              # landmarks and set pieces on big bases: their tall parts stand this much taller
DISC = Point(0, 0).buffer(R0 - 0.7, 128)
SOFT = {"grit"}
BRIDGE = {"plate", "tflag", "basalt", "alien", "ice", "duck", "plank", "tatami", "flag", "fleshfloor", "board", "shelf"}      # deck plates: a feature may sit across several (raised from the ground through them)

class Z(float):
    bridge = False

gg.COL.update({"spire": "#c47450", "agave": "#7f8c5c", "reed": "#b3a85a", "moss": "#5f7c3a", "heather": "#9b5d9b",
               "fern": "#52803c", "standing": "#80858b", "shrub": "#8c8248", "crystal": "#b9dcec", "salt": "#ece6d6",
               "obelisk": "#c2a576", "ruin": "#cfb28c", "egg": "#dccca2", "vein": "#a3295f", "spore": "#b45ed2",
               "shell": "#ece6d0", "carapace": "#45536a", "brick": "#9c5242", "concrete": "#aaa69e", "post": "#7c5432",
               "acidrim": "#7fae3a", "gauge": "#c9b458"})
gg.CUTC.update({"wash": "#6e3a28", "water": "#2d4a5a", "acid": "#63e042", "coolant": "#2f92b4", "scorch": "#1f1d1b",
                "glyph": "#5c4228", "mudcrack": "#2a1a12", "seam": "#15120f", "pit": "#3a2a1c"})

# ----------------------------------------------------------------------------------------------- placement helpers
def terr(s): return hasattr(s, "terr")

def soft_union(s):
    ps = [p for op in s.ops if op[-1] in SOFT for p in op[1]]
    return unary_union(ps) if ps else Polygon()

from shapely.strtree import STRtree
class _Hits:
    """does a shape touch any of these polygons? (a search tree: no need to merge them all into one shape first)"""
    def __init__(self, polys): self.polys = polys; self.tree = STRtree(polys) if polys else None
    def hit(self, g): return self.tree is not None and len(self.tree.query(g, predicate="intersects")) > 0
    def near(self, pt, d): return self.tree is not None and len(self.tree.query(pt, predicate="dwithin", distance=d)) > 0

def _soft_hits(s): return _Hits([p for op in s.ops if op[-1] in SOFT for p in op[1]])

def _soft_cut(kind, p): return kind in ("crack", "joint") or (kind == "pit" and p.area < 2.5)

def hard(s, allow=()):
    """Everything a new feature must not overlap (soft grit / hairline cracks / small pits excluded)."""
    import shapely as _sh
    out = []
    base = (s.objs + s.keepout) if terr(s) else s.feet
    if base:
        small = _sh.area(np.array(base, dtype=object)) < 2.0
        drop = set()
        if small.any():
            su = _soft_hits(s)
            if su.tree is not None:
                idx = np.nonzero(small)[0]
                hit = su.tree.query(np.array([base[i] for i in idx], dtype=object), predicate="intersects")[0]      # all small pieces in one go
                drop = {int(idx[i]) for i in set(hit.tolist())}
        out = [p for i, p in enumerate(base) if i not in drop]
    for ps, d, k in s.gcuts: out += [p for p in ps if not _soft_cut(k, p) and k not in allow]
    for ps, z0, z1, k in s.tcuts: out += [p for p in ps if not _soft_cut(k, p) and k not in allow]
    return out

def evict(s, g):
    """Remove grit / small pits under a new feature."""
    gb = g.buffer(0.3); su = [None]; ops = []
    from shapely.prepared import prep
    gp = prep(gb)
    for op in s.ops:
        if op[-1] in SOFT:
            ps = [p for p in op[1] if not gp.intersects(p)]
            if not ps: continue
            if len(ps) != len(op[1]): op = (op[0], ps) + tuple(op[2:])
        ops.append(op)
    def keep(p):
        if not (p.area < 2.0 and gp.intersects(p)): return True
        if su[0] is None: su[0] = _soft_hits(s)          # the soft pieces as they were before this call
        return not su[0].hit(p)
    su_before = s.ops
    feet = [p for p in s.feet if keep(p)]
    objs = [p for p in s.objs if keep(p)] if terr(s) else None
    s.ops = ops; s.feet = feet
    if objs is not None: s.objs = objs
    s.tcuts = [(ps2, z0, z1, k) for ps, z0, z1, k in s.tcuts
               for ps2 in [[q for p in ps for q in (polys_of(p.difference(gb)) if k == "joint" and gp.intersects(p) else [p]) if k == "joint" or not (_soft_cut(k, p) and gp.intersects(p))]] if ps2]
    s.gcuts = [(ps2, d, k) for ps, d, k in s.gcuts
               for ps2 in [[p for p in ps if not (k == "pit" and p.area < 2.5 and gp.intersects(p))]] if ps2]

def surf(s, g):
    """Height of the surface g would sit on (H = ground, or the top of a prism that fully holds it); None if it
    straddles something."""
    if terr(s):
        lo, hi = T_zr(s, g)
        return hi if hi - lo <= 1.0 else None
    z = H; br = False
    for op in s.ops:
        if op[-1] in SOFT: continue
        for p in op[1]:
            if not p.intersects(g) or p.intersection(g).area < 1e-3: continue
            if op[0] == "prism" and p.buffer(-0.3).contains(g): z = max(z, op[3])
            elif op[0] == "prism" and op[-1] in BRIDGE: z = max(z, op[3]); br = True
            else: return None
    z = Z(z); z.bridge = br
    return z

def find(s, make, tries=120, gap=0.4, region=DISC, ground=False, allow=(), at=None):
    """Try make() until a shape lands on free, level space. Returns (shape, surface z) or (None, None).
    With at=(function giving a centre), make(cx, cy) is only called for centres that are not already taken, which
    keeps crowded and large bases quick."""
    from shapely.prepared import prep
    hl = hard(s, allow); hb = _Hits(hl)
    hold = {id(p) for op in s.ops if op[0] == "prism" and op[-1] not in SOFT for p in op[1]}
    brid = {id(p) for op in s.ops if op[0] == "prism" and op[-1] in BRIDGE for p in op[1]}
    if at is not None:
        blk = [p for p in hl if id(p) not in hold and id(p) not in brid]
        taken = _Hits(blk); regp = prep(region)
        if blk and sum(p.area for p in blk) > 0.5 * region.area:          # crowded: worth working out exactly how much room is left
            if region.difference(unary_union(blk).buffer(gap)).area < 0.02 * region.area: return None, None      # no room left
    for _ in range(tries):
        if at is not None:
            c = at(); pt = Point(c)
            if not regp.contains(pt) or taken.near(pt, gap): continue
            g = make(*c)
        else: g = make()
        if g is None or g.is_empty or not region.contains(g): continue
        gb = g.buffer(gap)
        if not hb.hit(gb): z = H if not terr(s) else surf(s, g)
        else:
            if terr(s): continue
            if any(p.intersects(gb) and not (id(p) in brid or (id(p) in hold and p.buffer(-0.3).contains(g))) for p in (hl[int(i)] for i in hb.tree.query(gb))): continue
            z = surf(s, g)
        if z is None or (ground and z != H): continue
        evict(s, g); return g, z
    return None, None

def notch(s, polys, gap=0.3):
    """Cut the deck plates back around a new feature (gap all round) so it stands on the floor in its own hole and
    never shares a face with a plate."""
    if not os.environ.get("NOTCH"): return      # off: in real builds the cut-outs made plate joins fail
    g = unary_union(polys).buffer(gap)
    old, new = set(), []
    for i, op in enumerate(s.ops):
        if op[0] != "prism" or op[-1] not in BRIDGE: continue
        ps = []
        for p in op[1]:
            if p.intersects(g):
                old.add(id(p)); q = [r.simplify(0.03) for r in polys_of(p.difference(g).buffer(-0.45, join_style=2).buffer(0.45, join_style=2)) if wide_enough(r) and r.area > 0.6]; ps += q; new += q
            else: ps.append(p)
        s.ops[i] = (op[0], ps) + tuple(op[2:])
    s.feet = [p for p in s.feet if id(p) not in old] + new
    s.tcuts = [(ps2, z0, z1, k) for ps, z0, z1, k in s.tcuts
               for ps2 in [[r for p in ps for r in polys_of(p.difference(g)) if not r.buffer(-0.1).is_empty]] if ps2]

def up(s, polys, z, h, kind):
    """Raise polys h mm above surface z. Returns top z (or None)."""
    polys = [p for p in polys if p is not None and not p.is_empty]
    if not polys: return None
    if h >= 1.0: h *= HBOOST[0]
    if terr(s):
        got = tprism(s, polys, h, kind)
        return got[0][1] if got else None
    top = min(z + h, CAPZ)
    if top - z < 0.25: return None
    if getattr(z, "bridge", False): notch(s, polys)
    got = s.prism(polys, top, kind, z0=(H - SINK) if (z <= H or getattr(z, "bridge", False)) else z - 0.15)
    return top if got else None

def mound(s, polys, z, h, tiers, kind):
    if h >= 1.0: h *= HBOOST[0]
    if terr(s): tstack(s, polys, h, tiers, kind); return
    h = min(h, CAPZ - z - 0.05)
    if h < 0.25: return
    if getattr(z, "bridge", False): notch(s, polys); h += z - H; z = H
    s.stack(polys, h, tiers, kind, z0=None if z <= H else z - 0.15 + SINK)

def cut(s, polys, z, depth, kind):
    if terr(s): tcut_flat(s, polys, depth, kind); return
    if z <= H: s.gcut(polys, depth, kind)
    else: s.tcut(polys, z - depth, z + 0.3, kind)

def topcut(s, polys, ztop, depth, kind="seam"):
    s.tcut(polys, ztop - depth, ztop + 0.5, kind)

def drape(s, g, h, kind):
    """Raise a long shape over whatever it crosses: split per surface (ground / each prism top), skipping tapers."""
    g = g.intersection(DISC)
    if terr(s):
        b = unary_union(hard(s)).buffer(0.35) if hard(s) else Polygon()
        g = g.difference(b)
        E = 16 * max(1.0, gg.K); xs = np.arange(-E, E, 3.0)
        pieces = [q for x in xs for q in polys_of(g.intersection(box(x, -E, x + 3.0, E))) if wide_enough(q)]
        if pieces: evict(s, g); tprism(s, pieces, h, kind)
        return
    tap = [p for op in s.ops if op[0] == "taper" and op[-1] not in SOFT for p in op[1]]
    tap += [p for ps, z0, z1, k in s.tcuts for p in ps]
    if tap: g = g.difference(unary_union(tap).buffer(0.35))
    evict(s, g)
    rest = g; levels = {}
    prisms = sorted([op for op in s.ops if op[0] == "prism" and op[-1] not in SOFT and op[-1] not in BRIDGE], key=lambda o: -o[3])
    for op in prisms:
        for p in op[1]:
            piece = rest.intersection(inset(p, 0.3))
            if not piece.is_empty: levels.setdefault(op[3], []).extend(polys_of(piece))
            rest = rest.difference(p.buffer(0.3))
    bt = [op[3] for op in s.ops if op[0] == "prism" and op[-1] in BRIDGE for p in op[1] if p.intersects(rest)]
    zg = Z(max(bt) if bt else H); zg.bridge = bool(bt)
    levels.setdefault(zg, []).extend(polys_of(rest))
    allp = [p for ps in levels.values() for p in ps if wide_enough(p) and p.area > 0.6]
    if bt and allp: notch(s, allp)
    for z, ps in levels.items():
        ps = [p for p in ps if wide_enough(p) and p.area > 0.6]
        if ps: up(s, ps, z, h, kind)

# shapes
FOCUS = [None]             # scenes.py: when set, every "random spot" comes from here (beside the track, at the foot of the wall...)
AIM = [None]               # scenes.py: when set, pieces turn to suit the thing they belong to

def aim(rng, x, y, span=2 * math.pi):
    """the angle a new piece is laid at: random, unless the scene says how pieces here should lie"""
    a = rng.uniform(0, span)
    return a if AIM[0] is None else AIM[0](rng, x, y, a)

def rp(rng, dmin=0, dmax=11):
    if FOCUS[0] is not None: return FOCUS[0](rng)
    dmin, dmax = dmin * gg.K, dmax * gg.K + (gg.K - 1) * 2.0 * (gg.K > 1)      # radii follow the base size
    a = rng.uniform(0, 2 * math.pi); d = math.sqrt(rng.uniform(dmin ** 2 / dmax ** 2, 1)) * dmax
    return d * math.cos(a), d * math.sin(a)

def rect(cx, cy, l, w, a):
    f = frame(cx, cy, a); return Polygon([f(-l / 2, -w / 2), f(l / 2, -w / 2), f(l / 2, w / 2), f(-l / 2, w / 2)])

def blob(cx, cy, r, rng, k=4):
    g = unary_union([Point(cx + rng.uniform(-r, r) * .5, cy + rng.uniform(-r, r) * .5).buffer(r * rng.uniform(.5, .8), 16) for _ in range(k)])
    return g.buffer(0.5).buffer(-0.5).simplify(0.05)

def ring(cx, cy, ro, ri, res=32):
    return Point(cx, cy).buffer(ro, res).difference(Point(cx, cy).buffer(ri, res))

def hexa(cx, cy, r, rot=0.0):
    return Polygon([(cx + r * math.cos(rot + k * math.pi / 3), cy + r * math.sin(rot + k * math.pi / 3)) for k in range(6)])

def meander(rng, phi, off, amp, w, n=40):
    f = frame(0, 0, phi); k = rng.uniform(0.15, 0.3); ph = rng.uniform(0, 6.3)
    E = 19 * max(1.0, gg.K); n = int(n * max(1.0, gg.K))
    pts = [f(u, off * gg.K + amp * math.sin(k * u + ph) + 0.3 * amp * math.sin(2.3 * k * u)) for u in np.linspace(-E, E, n)]
    return LineString(pts)

def spiral(cx, cy, r, turns, w):
    ts = np.linspace(0, turns * 2 * math.pi, int(turns * 40))
    pts = [(cx + (0.2 + r * t / ts[-1]) * math.cos(t), cy + (0.2 + r * t / ts[-1]) * math.sin(t)) for t in ts]
    return LineString(pts).buffer(w / 2, cap_style=1)

def crack_plate(s, rng, reg, z, h, kind, npts=420, w=0.2):
    top = up(s, [reg], z, h, kind)
    if top is None: return
    E = 17 * max(1.0, gg.K); npts = round(npts * max(1.0, gg.K) ** 2)
    pts = MultiPoint([(rng.uniform(-E, E), rng.uniform(-E, E)) for _ in range(npts)]); edges = set()
    for cell in voronoi_diagram(pts, envelope=box(-E - 3, -E - 3, E + 3, E + 3)).geoms:
        c = [tuple(round(v, 3) for v in xy) for xy in cell.exterior.coords]
        for a_, b_ in zip(c, c[1:]): edges.add(tuple(sorted((a_, b_))))
    from shapely.prepared import prep
    inner = inset(reg, 0.35); near = prep(inner.buffer(w + 0.6)); bx = inner.buffer(w + 0.6).bounds; segs = []
    for a_, b_ in edges:
        pts_ = wiggle(a_, b_, rng, 0.2)                                   # always drawn, so the same seed gives the same cracks
        if max(a_[0], b_[0]) < bx[0] - 1 or min(a_[0], b_[0]) > bx[2] + 1 or max(a_[1], b_[1]) < bx[1] - 1 or min(a_[1], b_[1]) > bx[3] + 1: continue
        ln = LineString(pts_)
        if near.intersects(ln): segs.append(ln.buffer(w, cap_style=1, resolution=4))       # only cracks that reach this plate
    if not segs: return
    net = unary_union(segs)
    topcut(s, [net.intersection(inner)], top, 0.3, "mudcrack")

def radial(cx, cy, r0, r1, n, rng, w=0.22):
    out = []
    for k in range(n):
        a = rng.uniform(0, 2 * math.pi)
        out.append(LineString(wiggle((cx + r0 * math.cos(a), cy + r0 * math.sin(a)), (cx + r1 * math.cos(a), cy + r1 * math.sin(a)), rng, 0.25)).buffer(w, cap_style=1, resolution=4))
    return unary_union(out)

def cluster(s, rng, n, make_one, cx, cy, spread, gap=0.25):
    """n small shapes near (cx, cy) that don't touch each other. Returns list."""
    got = []
    for _ in range(n * 30):
        if len(got) >= n: break
        g = make_one(cx + rng.uniform(-spread, spread), cy + rng.uniform(-spread, spread))
        if all(g.distance(q) > gap for q in got): got.append(g)
    return got

def place_group(s, rng, shapes_at, tries=120, gap=0.4, ground=False, region=DISC):
    """shapes_at(cx, cy) -> list of polygons; places the group where its union is free. Returns (list, z)."""
    holder = {}
    def mk(cx, cy):
        ps = shapes_at(cx, cy)
        if not ps: return None
        holder["ps"] = ps; return unary_union(ps)
    g, z = find(s, mk, tries, gap, region, ground, at=lambda: rp(rng, 0, 12))
    return (holder["ps"], z) if g is not None else (None, None)

# ----------------------------------------------------------------------------------------------- BADLANDS
def b_wash(s, rng, lvl):          # dry wash: meandering shallow channel rim to rim
    ln = meander(rng, rng.uniform(0, math.pi), rng.uniform(-6, 6), rng.uniform(1.0, 2.2), 1.6)
    s.gcut([ln.buffer(rng.uniform(0.7, 0.95), cap_style=2)], 0.5, "wash")
    s.wash = ln

def b_hoodoo(s, rng, lvl):        # thin spires with a cap rock
    for k in range(rng.randint(1, 2) + (lvl >= 3)):
        r = rng.uniform(1.5, 1.9)
        g, z = find(s, lambda: stone(*rp(rng, 2, 12), r, rng, .85, 8), gap=0.5)
        if g is None: continue
        if terr(s): continue
        profs, zt = s.stack([g], 2.3, ((0.4, 4), (0.3, 7), (0.3, 10)), "spire")
        if profs: s.prism([max(profs, key=lambda p: p.area).buffer(0.35)], min(zt + 0.45, CAPZ), "rock", z0=zt - 0.15, foot=False)

def b_mudcrack(s, rng, lvl):      # cracked mud flat (low plate)
    g, z = find(s, lambda: blob(*rp(rng, 0, 9), rng.uniform(3.0, 4.0), rng), gap=0.5, ground=True)
    if g is not None: crack_plate(s, rng, g, z, 0.3, "pan")

def b_agave(s, rng, lvl):         # agave / yucca rosettes
    for k in range(rng.randint(2, 3)):
        ro = rng.uniform(1.3, 1.8)
        g, z = find(s, lambda: star(*rp(rng, 1, 12), ro, ro * 0.45, 8, rng.uniform(0, 1)), gap=0.4)
        if g is not None: mound(s, [g], z, 1.4, ((0.5, 20), (0.5, 45)), "agave")

def b_washbed(s, rng, lvl):       # pebble bed in the wash
    if not hasattr(s, "wash"): return
    band = s.wash.buffer(1.1)
    for _ in range(rng.randint(10, 16)):
        g, z = find(s, lambda: stone(*s.wash.interpolate(rng.uniform(0, s.wash.length)).coords[0], rng.uniform(0.5, 0.7), rng, .8, 7),
                    tries=30, gap=0.15, region=band.intersection(DISC), allow=("wash",))
        if g is not None: s.stack([g], 0.4, ((0.7, 10), (0.3, 45)), "pebble")

def b_spiral(s, rng, lvl):        # petroglyph spiral on the highest flat rock top
    tops = [(op[3], p) for op in s.ops if op[0] == "prism" and op[-1] == "rock" for p in op[1] if wide_enough(p, 4.0)]
    if not tops: return
    zt, p = max(tops, key=lambda t: (t[0], t[1].area))
    c = inset(p, 1.4).representative_point()
    sp = spiral(c.x, c.y, rng.uniform(1.2, 1.7), 2.2, 0.4).intersection(inset(p, 0.4))
    topcut(s, [sp], zt, 0.3, "glyph")

def b_split(s, rng, lvl):         # split boulder: two halves with a slot between
    r = rng.uniform(2.4, 2.9)
    g, z = find(s, lambda: stone(*rp(rng, 2, 11), r, rng, .8, 8), gap=0.5, ground=True)
    if g is None: return
    c = g.centroid; a = rng.uniform(0, math.pi)
    slot = rect(c.x, c.y, 8, 0.6, a)
    halves = polys_of(g.difference(slot))
    for i, h_ in enumerate(halves[:2]): up(s, [h_], z, 2.2 - 0.4 * i, "spire")

def b_scree(s, rng, lvl):         # scree fan at the foot of the biggest rock
    rocks = [p for op in s.ops if op[0] == "prism" and op[-1] == "rock" and op[2] <= H for p in op[1]]
    if not rocks: return
    big = max(rocks, key=lambda p: p.area); c = big.centroid
    for _ in range(rng.randint(16, 22)):
        a = math.atan2(-c.y, -c.x) + rng.uniform(-0.9, 0.9); d = rng.uniform(0.6, 4.0)
        b = big.exterior.interpolate(big.exterior.project(Point(c.x + 20 * math.cos(a), c.y + 20 * math.sin(a))))
        g, z = find(s, lambda: stone(b.x + d * math.cos(a), b.y + d * math.sin(a), rng.uniform(0.5, 0.8), rng, .7, 5, round_=False), tries=20, gap=0.12)
        if g is not None: s.stack([g], 0.5, ((0.7, 6), (0.3, 40)), "rock")

# ----------------------------------------------------------------------------------------------- HIGHLAND
def h_bog(s, rng, lvl):           # peat pool with reeds round the edge
    g, z = find(s, lambda: blob(*rp(rng, 0, 9), rng.uniform(2.4, 3.2), rng), gap=1.2, ground=True)
    if g is None: return
    s.gcut([g], 0.6, "water")
    for _ in range(rng.randint(4, 7)):
        p = g.exterior.interpolate(rng.uniform(0, g.exterior.length))
        r_, z_ = find(s, lambda: star(p.x + rng.uniform(-.9, .9), p.y + rng.uniform(-.9, .9), 1.25, 0.55, 5, rng.uniform(0, 1)), tries=15, gap=0.15)
        if r_ is not None: mound(s, [r_], z_, 1.6, ((0.6, 15), (0.4, 40)), "reed")

def h_steps(s, rng, lvl):         # stepping stones in a gentle curve
    a = rng.uniform(0, math.pi); ln = meander(rng, a, rng.uniform(-4, 4), 1.2, 1)
    for u in np.arange(rng.uniform(0, 3), ln.length, 3.0):
        p = ln.interpolate(u)
        g, z = find(s, lambda: stone(p.x, p.y, rng.uniform(1.0, 1.3), rng, .8, 9), tries=3, gap=0.3)
        if g is not None: up(s, [g], z, 0.5, "slate")

def h_wall(s, rng, lvl):          # tumbled drystone wall along the rim: two courses
    a0 = rng.uniform(0, 2 * math.pi); n = 0
    for t in np.arange(0, rng.uniform(1.0, 1.5), 0.15):
        a = a0 + t; d = 12.2 + rng.uniform(-.3, .3)
        g, z = find(s, lambda: stone(d * math.cos(a), d * math.sin(a), rng.uniform(1.0, 1.25), rng, .7, 6, round_=False), tries=4, gap=0.12)
        if g is None: continue
        top = up(s, [g], z, 1.2 if n % 2 else 1.8, "slate"); n += 1       # stones of two heights, each one piece

def h_moss(s, rng, lvl):          # low moss / lichen patches
    for _ in range(rng.randint(2, 4)):
        g, z = find(s, lambda: blob(*rp(rng, 0, 12), rng.uniform(1.4, 2.4), rng, 5), gap=0.3)
        if g is not None: up(s, [g], z, 0.25, "moss")

def h_heather(s, rng, lvl):       # heather: tight clusters of tiny purple bells
    for _ in range(rng.randint(2, 3)):
        ps, z = place_group(s, rng, lambda cx, cy: cluster(s, rng, 8, lambda x, y: Point(x, y).buffer(0.48, 8), cx, cy, 1.4, 0.15), gap=0.3)
        if ps: mound(s, ps, z, 0.7, ((1.0, 25),), "heather")

def h_standing(s, rng, lvl):      # standing stone with a cup-and-ring mark on top
    g, z = find(s, lambda: rect(*rp(rng, 3, 10), rng.uniform(3.4, 4.2), rng.uniform(2.6, 3.0), rng.uniform(0, math.pi)).buffer(0.3).buffer(-0.3), tries=300, gap=0.5, ground=True)
    if g is None: return
    top = up(s, [g], z, 2.9, "standing")
    if top:
        c = g.centroid
        topcut(s, [ring(c.x, c.y, 1.05, 0.7), Point(c.x, c.y).buffer(0.35, 10)], top, 0.3, "crack")

def h_stream(s, rng, lvl):        # stream with a pebble bed
    ln = meander(rng, rng.uniform(0, math.pi), rng.uniform(-7, 7), 1.4, 1.4)
    s.gcut([ln.buffer(0.75, cap_style=2)], 0.6, "water")
    band = ln.buffer(0.9).intersection(DISC)
    for _ in range(12):
        g, z = find(s, lambda: Point(*ln.interpolate(rng.uniform(0, ln.length)).coords[0]).buffer(0.5, 8), tries=10, gap=0.15, region=band, allow=("water",))
        if g is not None: s.stack([g], 0.35, ((0.7, 10), (0.3, 45)), "pebble")

def h_fern(s, rng, lvl):          # fern fronds: tapered leaf with leaflets
    for _ in range(rng.randint(2, 3)):
        def mk():
            cx, cy = rp(rng, 1, 11); a = rng.uniform(0, 2 * math.pi); f = frame(cx, cy, a); L = rng.uniform(3.5, 4.5)
            parts = [LineString([f(0, 0), f(L, 0)]).buffer(0.45)]
            for u in np.linspace(0.6, L - 0.5, 5):
                w = 1.4 * (1 - u / L) + 0.5
                parts += [LineString([f(u, 0), f(u + 0.6, s_ * w)]).buffer(0.45) for s_ in (-1, 1)]
            return unary_union(parts)
        g, z = find(s, mk, gap=0.3)
        if g is not None: up(s, [g], z, 0.55, "fern")

# ----------------------------------------------------------------------------------------------- DESERTS OF MAAHL
def m_ruin(s, rng, lvl):          # half-buried ancient blocks with joint lines
    def at(cx, cy):
        a = rng.uniform(0, math.pi); f = frame(cx, cy, a); out = []
        for i in range(rng.randint(3, 4)):
            u = i * 2.6 + rng.uniform(-.3, .3); out.append(rect(*f(u, rng.uniform(-.4, .4)), 2.3, 2.0, a + rng.uniform(-.15, .15)))
        return out
    ps, z = place_group(s, rng, at, gap=0.5)
    if ps:
        for p in ps:
            t = up(s, [p], z, rng.uniform(0.6, 1.2), "ruin")
            if t: c = p.centroid; topcut(s, [rect(c.x, c.y, 1.6, 0.35, rng.uniform(0, math.pi))], t, 0.25, "glyph")

def m_shrub(s, rng, lvl):         # scrubby desert bushes
    for _ in range(rng.randint(2, 4)):
        ro = rng.uniform(1.0, 1.5)
        g, z = find(s, lambda: star(*rp(rng, 0, 12), ro, ro * 0.55, 9, rng.uniform(0, 1)), gap=0.4)
        if g is not None: mound(s, [g], z, 0.9, ((0.6, 20), (0.4, 45)), "shrub")

def m_sinkhole(s, rng, lvl):      # sinkhole on flat sand: nested cuts + rays of cracks
    for _ in range(300):
        cx, cy = rp(rng, 0, 10); r = rng.uniform(1.5, 2.0); g = Point(cx, cy).buffer(r + 1.4, 32)
        if DISC.contains(g) and not unary_union(hard(s)).intersects(g) and T_flat(s, g) == H:
            evict(s, g)
            for k, d in enumerate((0.3, 0.6, 0.9)): s.gcut([Point(cx, cy).buffer(r - k * 0.6, 32)], d, "hollow" if k < 2 else "pit")
            s.gcut([radial(cx, cy, r, r + 1.3, 6, rng)], 0.35, "crack"); s.keepout.append(g); return

def m_crystal(s, rng, lvl):       # crystal shard cluster, fanned
    def at(cx, cy):
        a0 = rng.uniform(0, 2 * math.pi); out = []
        for k in range(rng.randint(3, 5)):
            a = a0 + rng.uniform(-0.8, 0.8); L = rng.uniform(1.8, 3.0); f = frame(cx, cy, a)
            out.append(Polygon([f(0.3, -0.5), f(L, -0.5), f(L + 0.6, 0), f(L, 0.5), f(0.3, 0.5)]))
        return [g for g in polys_of(unary_union(out))]
    ps, z = place_group(s, rng, at, gap=0.4)
    if ps: up(s, ps, z, rng.uniform(1.6, 2.2), "crystal")

def m_salt(s, rng, lvl):          # salt crust plate with polygon cracks
    for _ in range(60):
        reg = blob(*rp(rng, 0, 9), rng.uniform(2.8, 3.6), rng)
        if DISC.contains(reg) and not unary_union(hard(s)).buffer(0.4).intersects(reg) and T_flat(s, reg) is not None:
            evict(s, reg); z = T_flat(s, reg)
            top = tprism(s, [reg], 0.25, "salt")
            if top:
                pts = MultiPoint([(rng.uniform(-17, 17), rng.uniform(-17, 17)) for _ in range(520)]); edges = set()
                for cell in voronoi_diagram(pts, envelope=box(-20, -20, 20, 20)).geoms:
                    c = [tuple(round(v, 3) for v in xy) for xy in cell.exterior.coords]
                    for a_, b_ in zip(c, c[1:]): edges.add(tuple(sorted((a_, b_))))
                net = unary_union([LineString([a_, b_]).buffer(0.2, cap_style=1, resolution=3) for a_, b_ in edges])
                topcut(s, [net.intersection(inset(reg, 0.3))], top[0][1], 0.2, "crack")
            return

def m_obelisk(s, rng, lvl):       # toppled obelisk with a glyph band
    def mk():
        cx, cy = rp(rng, 0, 6); a = rng.uniform(0, math.pi); f = frame(cx, cy, a)
        return Polygon([f(-5, -1.0), f(4.0, -1.0), f(5, 0), f(4.0, 1.0), f(-5, 1.0)])
    g, z = find(s, mk, tries=200, gap=0.5)
    if g is None: return
    got = tprism(s, [g], 1.3, "obelisk")
    if got:
        c = g.minimum_rotated_rectangle.exterior.coords; dx, dy = c[1][0] - c[0][0], c[1][1] - c[0][1]
        a = math.atan2(dy, dx) if math.hypot(dx, dy) > 3 else math.atan2(c[2][1] - c[1][1], c[2][0] - c[1][0])
        f = frame(g.centroid.x, g.centroid.y, a)
        glyphs = [rect(*f(u, 0), 0.5, 0.9 if i % 3 else 0.5, a) for i, u in enumerate(np.arange(-3.6, 3.4, 0.95))]
        topcut(s, [q for q in glyphs if inset(g, 0.3).contains(q)], got[0][1], 0.3, "glyph")

def m_column(s, rng, lvl):        # fallen column drum with fluting notches
    r = rng.uniform(1.8, 2.1)
    g, z = find(s, lambda: Point(*rp(rng, 2, 11), ).buffer(r, 48), gap=0.5)
    if g is None: return
    got = tprism(s, [g], 1.8, "obelisk")
    if got:
        c = g.centroid; zt = got[0][1]
        fl = [Point(c.x + r * math.cos(k * math.pi / 6), c.y + r * math.sin(k * math.pi / 6)).buffer(0.32, 8) for k in range(12)]
        topcut(s, [unary_union(fl).intersection(g)], zt, 0.6, "glyph")
        topcut(s, [ring(c.x, c.y, 1.0, 0.75)], zt, 0.25, "crack")

def m_pavers(s, rng, lvl):        # patch of half-buried paving slabs, some missing
    def at(cx, cy):
        a = rng.uniform(0, math.pi); f = frame(cx, cy, a)
        return [rect(*f(i * 2.3, j * 2.3), 1.95, 1.95, a) for i in range(-1, 2) for j in range(-1, 2) if rng.random() > 0.3]
    ps, z = place_group(s, rng, at, gap=0.4)
    if ps:
        for p in ps: up(s, [p], z, rng.uniform(0.25, 0.45), "ruin")

# ----------------------------------------------------------------------------------------------- CORRIDOR
def c_vent(s, rng, lvl):          # floor vent grille
    for _ in range(2 + (lvl >= 3)):
        def mk():
            cx, cy = rp(rng, 0, 9); return rect(cx, cy, 4.4, 3.2, getattr(s, "phi", rng.choice([0, math.pi / 2])))
        g, z = find(s, mk, gap=0.4)
        if g is None: continue
        top = up(s, [g], z, 0.3, "metal")
        if top:
            c = g.centroid; a = getattr(s, "phi", 0)
            mr = g.minimum_rotated_rectangle.exterior.coords; a = math.atan2(mr[1][1] - mr[0][1], mr[1][0] - mr[0][0])
            f = frame(c.x, c.y, a)
            topcut(s, [rect(*f(0, v), 3.4, 0.4, a) for v in np.arange(-1.0, 1.01, 0.66)], top, 0.5, "slot")

def c_chevron(s, rng, lvl):       # hazard chevron stripe band (paint relief)
    def at(cx, cy):
        a = rng.uniform(0, math.pi); f = frame(cx, cy, a)
        return [Polygon([f(u, -1.1), f(u + 1.45, -1.1), f(u + 2.55, 0), f(u + 1.45, 1.1), f(u, 1.1), f(u + 1.1, 0)]) for u in np.arange(-3.6, 2.6, 2.4)]
    ps, z = place_group(s, rng, at, tries=300, gap=0.25)
    if ps: up(s, ps, z, 0.25, "paint")

def c_cable(s, rng, lvl):         # cable runs draped over the deck
    for k in range(rng.randint(1, 2)):
        ln = meander(rng, rng.uniform(0, math.pi), rng.uniform(-8, 8), rng.uniform(0.8, 2.0), 1.0, 30)
        drape(s, ln.buffer(0.5, cap_style=1), 0.7, "cable")

def c_panel(s, rng, lvl):         # bolted access panel: seam groove + 4 bolts
    g, z = find(s, lambda: rect(*rp(rng, 0, 9), 4.6, 4.6, rng.choice([0, 0.78, getattr(s, 'phi', 0)])), gap=0.4)
    if g is None: return
    top = up(s, [g], z, 0.35, "metal")
    if top:
        topcut(s, [inset(g, 0.5).difference(inset(g, 0.85))], top, 0.25, "slot")
        topcut(s, [Point(p).buffer(0.3, 8) for p in list(inset(g, 0.25).exterior.coords)[:4]], top, 0.4, "slot")

def c_spill(s, rng, lvl):         # coolant spill puddle
    g, z = find(s, lambda: blob(*rp(rng, 0, 10), rng.uniform(1.6, 2.4), rng, 5), gap=0.3)
    if g is not None: cut(s, [g], z, 0.3, "coolant")

def c_hexdeck(s, rng, lvl):       # hex tile patch cut into the deck
    g, z = find(s, lambda: Point(*rp(rng, 0, 9)).buffer(rng.uniform(2.6, 3.2), 32), tries=300, gap=0.2)
    if g is None: return
    c = g.centroid; r = 1.2
    cells = [hexa(c.x + i * r * 1.5, c.y + (j + 0.5 * (i % 2)) * r * math.sqrt(3), r) for i in range(-4, 5) for j in range(-4, 5)]
    net = unary_union([h_.exterior.buffer(0.18) for h_ in cells]).intersection(inset(g, 0.3))
    cut(s, [net], z, 0.3, "slot")
    cut(s, [g.exterior.buffer(0.25)], z, 0.3, "slot")

def c_scorch(s, rng, lvl):        # blast scorch: dark patch + radial cracks
    g, z = find(s, lambda: Point(*rp(rng, 0, 9)).buffer(2.3, 24), gap=0.2)
    if g is None: return
    c = g.centroid; cut(s, [blob(c.x, c.y, 1.4, rng)], z, 0.25, "scorch")
    cut(s, [radial(c.x, c.y, 1.2, 2.3, 7, rng, 0.2)], z, 0.3, "crack")

def c_lights(s, rng, lvl):        # recessed light strip segments
    def at(cx, cy):
        a = getattr(s, "phi", rng.uniform(0, math.pi)) + rng.choice([0, math.pi / 2]); f = frame(cx, cy, a)
        return [rect(*f(u, 0), 3.0, 0.95, a) for u in (-3.6, 0, 3.6)]
    ps, z = place_group(s, rng, at, gap=0.35)
    if ps: up(s, ps, z, 0.3, "lens")

# ----------------------------------------------------------------------------------------------- INFESTATION
def i_eggs(s, rng, lvl):          # egg cluster
    for _ in range(1 + (lvl >= 3)):
        ps, z = place_group(s, rng, lambda cx, cy: cluster(s, rng, rng.randint(4, 6), lambda x, y: affinity.rotate(
            affinity.scale(Point(x, y).buffer(1.1, 16), 1.0, 1.4), rng.uniform(0, 180)), cx, cy, 2.0, 0.2), tries=200, gap=0.3)
        if ps: mound(s, ps, z, 1.5, ((0.6, 10), (0.4, 35)), "egg")

def i_acid(s, rng, lvl):          # acid pools with a raised scum rim
    for _ in range(rng.randint(1, 2)):
        g, z = find(s, lambda: blob(*rp(rng, 0, 10), rng.uniform(1.5, 2.2), rng), gap=0.4)
        if g is None: continue
        if up(s, [g.buffer(0.95).difference(g)], z, 0.3, "acidrim"): cut(s, [g], z, 0.35, "acid")

def i_veins(s, rng, lvl):         # branching veins from the mass across the plates
    mass = [p for op in s.ops if op[-1] == "flesh" and op[0] == "taper" for p in op[1]]
    c = unary_union(mass).centroid if mass else Point(0, 0)
    lines = []
    for k in range(rng.randint(3, 4)):
        a = math.atan2(-c.y, -c.x) + rng.uniform(-1.4, 1.4); x, y = c.x, c.y; pts = [(x, y)]
        for i in range(10):
            a += rng.uniform(-.35, .35); x += 1.6 * math.cos(a); y += 1.6 * math.sin(a); pts.append((x, y))
            if i == 5 and rng.random() < 0.7:
                b = a + rng.choice([-1, 1]) * 0.8; lines.append(LineString([(x, y), (x + 4 * math.cos(b), y + 4 * math.sin(b))]))
        lines.append(LineString(pts))
    drape(s, unary_union([l.buffer(0.46, cap_style=1) for l in lines]), 0.35, "vein")

def i_spores(s, rng, lvl):        # spore puffballs
    for _ in range(rng.randint(2, 3)):
        ps, z = place_group(s, rng, lambda cx, cy: cluster(s, rng, rng.randint(3, 5), lambda x, y: Point(x, y).buffer(rng.uniform(0.55, 0.95), 14), cx, cy, 1.2, 0.15), gap=0.3)
        if ps: mound(s, ps, z, 1.1, ((0.5, 15), (0.5, 50)), "spore")

def i_hatched(s, rng, lvl):       # hatched egg shells, torn open
    for _ in range(rng.randint(2, 3)):
        def mk():
            cx, cy = rp(rng, 1, 11)
            return Point(cx, cy).buffer(1.7, 20).difference(star(cx, cy, 1.15, 0.6, 7, rng.uniform(0, 1)))
        g, z = find(s, mk, gap=0.35)
        if g is not None: up(s, [g], z, 1.3, "shell")

def i_torn(s, rng, lvl):          # a plate cracked across with a lifted shard
    plates = [(op[3], p) for op in s.ops if op[0] == "prism" and op[-1] == "plate" for p in op[1] if p.area > 12]
    rng.shuffle(plates)
    for zt, p in plates[:3]:
        tc = [q for ps, *_ in s.tcuts for q in ps]
        if any(p.intersects(q) for q in tc) or any(p.intersects(q) for op in s.ops if op[-1] not in ("plate",) + tuple(SOFT) for q in op[1]): continue
        c = p.centroid; a = rng.uniform(0, math.pi)
        crk = LineString(wiggle((c.x - 4 * math.cos(a), c.y - 4 * math.sin(a)), (c.x + 4 * math.cos(a), c.y + 4 * math.sin(a)), rng, 0.4)).buffer(0.25)
        shard = polys_of(inset(p, 0.4).difference(crk.buffer(0.35)))
        if len(shard) < 2: continue
        sh = min(shard, key=lambda q: q.area)
        topcut(s, [crk.intersection(inset(p, 0.05))], zt, 0.45, "slot")
        s.prism([inset(sh, 0.15)], zt + 0.4, "plate", z0=zt - 0.15, foot=False); return

def i_ribs(s, rng, lvl):          # curved row of carapace ribs
    def at(cx, cy):
        a0 = rng.uniform(0, 2 * math.pi); R_ = rng.uniform(4, 5.5); ox, oy = cx - R_ * math.cos(a0), cy - R_ * math.sin(a0)
        return [rect(ox + R_ * math.cos(a0 + t), oy + R_ * math.sin(a0 + t), 3.2, 0.95, a0 + t) for t in np.linspace(-0.45, 0.45, 5)]
    ps, z = place_group(s, rng, at, gap=0.4)
    if ps: mound(s, ps, z, 1.6, ((0.6, 6), (0.4, 30)), "carapace")

# ----------------------------------------------------------------------------------------------- URBAN
def u_bricks(s, rng, lvl):        # brick rubble piles
    for _ in range(rng.randint(1, 2) + (lvl >= 3)):
        ps, z = place_group(s, rng, lambda cx, cy: cluster(s, rng, rng.randint(6, 10), lambda x, y: rect(x, y, 1.8, 0.95, rng.uniform(0, math.pi)), cx, cy, 1.8, 0.12), gap=0.35)
        if ps:
            for p in ps: up(s, [p], z, rng.uniform(0.45, 0.8), "brick")

def u_bullets(s, rng, lvl):       # bullet pockmarks in clusters
    for _ in range(rng.randint(2, 3)):
        ps, z = place_group(s, rng, lambda cx, cy: cluster(s, rng, rng.randint(5, 8), lambda x, y: Point(x, y).buffer(rng.uniform(0.3, 0.45), 8), cx, cy, 1.5, 0.3), gap=0.25)
        if ps: cut(s, ps, z, 0.4, "pothole")

def u_wall(s, rng, lvl):          # fallen wall chunk with brick coursing
    def mk():
        cx, cy = rp(rng, 2, 10); a = rng.uniform(0, math.pi)
        return rect(cx, cy, rng.uniform(5.5, 6.5), rng.uniform(3.0, 3.6), a).intersection(stone(cx, cy, 3.4, rng, .8, 9).buffer(0.8))
    g, z = find(s, mk, gap=0.4)
    if g is None: return
    top = up(s, [g], z, 1.0, "brick")
    if not top: return
    mr = g.minimum_rotated_rectangle.exterior.coords; a = math.atan2(mr[1][1] - mr[0][1], mr[1][0] - mr[0][0]); c = g.centroid; f = frame(c.x, c.y, a)
    cs = [rect(*f(0, v), 9, 0.3, a) for v in np.arange(-2, 2.1, 1.0)]
    cs += [rect(*f(u + (0.9 if int(round(v)) % 2 else 0), v + 0.5), 0.3, 0.7, a) for v in np.arange(-2, 2, 1.0) for u in np.arange(-4.5, 4.5, 1.8)]
    topcut(s, [unary_union(cs).intersection(inset(g, 0.3))], top, 0.3, "slot")

def u_dashes(s, rng, lvl):        # road paint dashes
    def at(cx, cy):
        a = rng.uniform(0, math.pi); f = frame(cx, cy, a)
        return [rect(*f(u, 0), 2.6, 0.95, a) for u in (-4.4, 0, 4.4)]
    ps, z = place_group(s, rng, at, gap=0.35, ground=True)
    if ps: up(s, ps, z, 0.25, "paint")

def u_scorch(s, rng, lvl):        # scorch marks
    for _ in range(rng.randint(1, 2)):
        g, z = find(s, lambda: blob(*rp(rng, 0, 10), rng.uniform(1.6, 2.4), rng, 5), gap=0.3)
        if g is not None: cut(s, [g], z, 0.2, "scorch")

def u_rebar(s, rng, lvl):         # broken concrete slab with rebar sticking out of the break
    def at(cx, cy):
        a = rng.uniform(0, math.pi); f = frame(cx, cy, a)
        sl = Polygon([f(-1.8, -1.5), f(1.4, -1.5), f(1.9, -0.4), f(1.2, 0.3), f(1.7, 1.1), f(1.0, 1.5), f(-1.8, 1.5)])
        bars = [rect(*f(1.9, v), 2.2, 0.9, a + rng.uniform(-.2, .2)) for v in (-0.9, 0.8)]
        return [sl] + bars
    ps, z = place_group(s, rng, at, tries=500, gap=0.3)
    if ps:
        top = up(s, [ps[0]], z, 0.8, "concrete")
        if top:
            for b in ps[1:]: s.prism([b], min(top + 0.2, CAPZ), "iron", z0=(H - SINK) if z <= H else z - 0.15)

def u_tilt(s, rng, lvl):          # lifted, tilted paving stones on the sidewalk
    slabs = [(op[3], p) for op in s.ops if op[0] == "prism" and op[-1] == "slab" for p in op[1]]
    if not slabs: return
    zt, sl = max(slabs, key=lambda t: t[1].area)
    for _ in range(rng.randint(2, 3)):
        g, z = find(s, lambda: rect(*rp(rng, 0, 12), 2.9, 2.9, rng.uniform(0, math.pi)), tries=200, gap=0.35, region=inset(sl, 0.6).intersection(DISC), allow=("slot",))
        if g is None: continue
        s.tcuts = [(ps2, z0, z1, k) for ps, z0, z1, k in s.tcuts
                   for ps2 in [[q for p in ps for q in polys_of(p.difference(g.buffer(0.15))) if not q.buffer(-0.1).is_empty]] if ps2]
        up(s, [g], z, 0.4, "paver")

def u_crater(s, rng, lvl):        # shell crater with spoke cracks on the road
    for _ in range(80):
        cx, cy = rp(rng, 0, 9); r = rng.uniform(1.8, 2.3); g = Point(cx, cy).buffer(r + 2.4)
        if DISC.contains(g) and surf(s, g) == H and not unary_union(hard(s)).intersects(g):
            evict(s, g); crater(s, cx, cy, r, 0.7); s.gcut([radial(cx, cy, r + 1.2, r + 2.6, 6, rng)], 0.4, "crack"); return

# ----------------------------------------------------------------------------------------------- MINE
def n_posts(s, rng, lvl):         # timber shoring posts at the wall + a short sill beam
    a0 = rng.uniform(0, 2 * math.pi)
    for k in range(2 + (lvl >= 3)):
        a = a0 + k * 2.1; d = 12.3
        g, z = find(s, lambda: (lambda b: rect(d * math.cos(b), d * math.sin(b), 1.9, 1.9, b))(a + rng.uniform(-0.6, 0.6)), tries=40, gap=0.3)
        if g is None: continue
        got = tprism(s, [g], 2.6, "post")
        if got: c = g.centroid; topcut(s, [ring(c.x, c.y, 0.7, 0.45, 12)], got[0][1], 0.25, "crack")

def n_ore(s, rng, lvl):           # quartz ore vein glinting through the floor
    ln = meander(rng, rng.uniform(0, math.pi), rng.uniform(-8, 8), 1.0, 1, 30)
    for u in np.arange(0, ln.length, 1.0):
        p = ln.interpolate(u)
        g, z = find(s, lambda: stone(p.x + rng.uniform(-.5, .5), p.y + rng.uniform(-.5, .5), rng.uniform(0.6, 0.9), rng, .7, 5, round_=False), tries=8, gap=0.12)
        if g is not None: tprism(s, [g], 0.35, "quartz")

def n_timbers(s, rng, lvl):       # collapsed roof timbers lying in a V
    def at(cx, cy):
        a = rng.uniform(0, 2 * math.pi); f = frame(cx, cy, a); b = rng.uniform(0.5, 0.8)
        return [rect(*f(3.2, -0.4), 6.2, 1.3, a - b / 2), rect(*f(3.2, 1.9), 6.2, 1.3, a + b / 2)]
    ps, z = place_group(s, rng, at, gap=0.4)
    if ps:
        ps = polys_of(unary_union(ps))
        for p in ps:
            got = tprism(s, [p], 0.9, "wood")
            if got:
                c = p.centroid; mr = p.minimum_rotated_rectangle.exterior.coords
                a = max([(math.hypot(mr[i + 1][0] - mr[i][0], mr[i + 1][1] - mr[i][1]), math.atan2(mr[i + 1][1] - mr[i][1], mr[i + 1][0] - mr[i][0])) for i in range(2)])[1]
                topcut(s, [rect(c.x, c.y, 5.0, 0.3, a)], got[0][1], 0.25, "crack")

def n_seep(s, rng, lvl):          # water seep puddle on flat ground with drip pits
    for _ in range(80):
        cx, cy = rp(rng, 0, 10); g = blob(cx, cy, rng.uniform(1.6, 2.2), rng)
        if DISC.contains(g) and T_flat(s, g.buffer(1.0)) is not None and not unary_union(hard(s)).intersects(g.buffer(1.0)):
            evict(s, g.buffer(1.0)); tcut_flat(s, [g], 0.4, "water")
            tcut_flat(s, [Point(cx + 2.6 * math.cos(t), cy + 2.6 * math.sin(t)).buffer(0.35, 8) for t in np.linspace(0, 6.28, 6)[:-1]], 0.4, "pit")
            return

def n_orepile(s, rng, lvl):       # pile of ore chunks (quartz + rock)
    ps, z = place_group(s, rng, lambda cx, cy: cluster(s, rng, 7, lambda x, y: stone(x, y, rng.uniform(0.6, 1.0), rng, .7, 5, round_=False), cx, cy, 1.6, 0.15), gap=0.4)
    if ps:
        for i, p in enumerate(ps): tstack(s, [p], 0.7, ((0.7, 6), (0.3, 40)), "quartz" if i % 2 else "rock")

def n_shaft(s, rng, lvl):         # shaft collapse: big pit with radial cracks
    for _ in range(80):
        cx, cy = rp(rng, 0, 8); r = rng.uniform(2.5, 2.9); g = Point(cx, cy).buffer(r + 2.2)
        if DISC.contains(g) and not unary_union(hard(s)).intersects(g):
            evict(s, g); gg2.crater_t(s, cx, cy, r); s.gcut([radial(cx, cy, r + 0.8, r + 3.0, 7, rng)], 0.4, "crack"); return

def n_barrel(s, rng, lvl):        # broken barrel: a ring of staves with gaps + two loose staves
    def at(cx, cy):
        rg = ring(cx, cy, 1.9, 0.85, 32); a0 = rng.uniform(0, 1)
        out = [g for g in polys_of(rg.difference(unary_union([rect(cx + 1.3 * math.cos(a0 + k * 0.785), cy + 1.3 * math.sin(a0 + k * 0.785), 1.2, 0.25, a0 + k * 0.785) for k in range(8)])))]
        out = [g for i, g in enumerate(out) if i % 4 != 1]
        out += [rect(cx + 2.8, cy + rng.uniform(-1, 1), 2.4, 0.95, rng.uniform(0, 3))]
        return out
    ps, z = place_group(s, rng, at, tries=300, gap=0.3)
    if ps: tprism(s, [p for p in ps if wide_enough(p)], 1.0, "wood")

def n_ladder(s, rng, lvl):        # ladder lying on the floor
    def at(cx, cy):
        a = rng.uniform(0, math.pi); f = frame(cx, cy, a)
        rails = [rect(*f(0, v), 7.5, 0.9, a) for v in (-1.1, 1.1)]
        rungs = [rect(*f(u, 0), 0.9, 1.5, a) for u in np.arange(-3, 3.1, 1.5)]
        return polys_of(unary_union(rails + rungs))
    ps, z = place_group(s, rng, at, tries=300, gap=0.3)
    if ps: tprism(s, ps, 0.6, "wood")

# ----------------------------------------------------------------------------------------------- tiers
TIERS = {
 "badlands": ([b_wash, b_hoodoo], [b_mudcrack, b_agave, b_washbed], [b_split, b_scree, b_spiral]),
 "highland": ([h_bog, h_steps], [h_wall, h_moss, h_heather], [h_stream, h_standing, h_fern]),
 "maahl": ([m_ruin, m_shrub], [m_sinkhole, m_crystal, m_salt], [m_obelisk, m_column, m_pavers]),
 "corridor": ([c_vent, c_chevron], [c_cable, c_panel, c_spill], [c_hexdeck, c_scorch, c_lights]),
 "infestation": ([i_eggs, i_acid], [i_veins, i_spores], [i_hatched, i_torn, i_ribs]),
 "urban": ([u_bricks, u_bullets], [u_wall, u_dashes, u_scorch], [u_rebar, u_tilt, u_crater]),
 "mine": ([n_posts, n_ore], [n_timbers, n_seep, n_orepile], [n_shaft, n_barrel, n_ladder]),
}
LATE = {b_washbed, b_scree, b_spiral}
SPARSE = {c_cable, c_lights, c_chevron}          # long or loud features: repeated sparingly on big bases
NOTES = {
 "badlands": ["Tier 1: the layouts we built (layered rock, loose rock, grit, crack net)",
              "Tier 2: + dry wash channel, hoodoo spires with cap rocks",
              "Tier 3: + cracked mud flat, agave rosettes, pebble bed in the wash",
              "Tier 4: + split boulder, scree fan, petroglyph spiral on the top rock"],
 "highland": ["Tier 1: the layouts we built (slate outcrops, grass tufts, flowers)",
              "Tier 2: + peat bog pool with reeds, stepping stones",
              "Tier 3: + tumbled drystone wall, moss patches, heather clusters",
              "Tier 4: + stream with pebble bed, standing stone (cup-and-ring), ferns"],
 "maahl": ["Tier 1: the v4 layouts (swells, dune, outcrops, pits, pavement, hardpan)",
           "Tier 2: + half-buried ruin blocks, desert shrubs",
           "Tier 3: + sinkhole with crack rays, crystal shards, salt crust",
           "Tier 4: + toppled glyph obelisk, fluted column drum, buried pavers"],
 "corridor": ["Tier 1: deck layouts 1-5 (plates, grates, trims)",
              "Tier 2: layouts 6-10 + vent grilles, hazard chevrons",
              "Tier 3: layouts 11-15 + cable runs, bolted panels, coolant spill",
              "Tier 4: layouts 16-20 + hex tile patch, blast scorch, light strips"],
 "infestation": ["Tier 1: the layouts we built (hex plates, flesh mass, tendrils, pustules)",
                 "Tier 2: + egg clusters, acid pools with scum rims",
                 "Tier 3: + branching veins over the plates, spore puffballs",
                 "Tier 4: + hatched shells, cracked plate with lifted shard, carapace ribs"],
 "urban": ["Tier 1: street layouts 1-5 (curb, sidewalk, road, rubble)",
           "Tier 2: layouts 6-10 + brick rubble piles, bullet pockmarks",
           "Tier 3: layouts 11-15 (manholes, crosswalks, cobbles...) + fallen brick wall, lane dashes, scorch",
           "Tier 4: layouts 16-20 + broken slab with rebar, lifted pavers, shell crater"],
 "mine": ["Tier 1: the v4 layouts (bedrock slabs, cart lane, craters, crates, heaps)",
          "Tier 2: + shoring posts at the wall, quartz ore vein",
          "Tier 3: + collapsed timbers, water seep with drip pits, ore pile",
          "Tier 4: + shaft collapse, broken barrel, fallen ladder"],
}

_mk0 = gg2.mk
ORIG = dict(gg2.STYLES)
import details, styles2
details.install(globals()); styles2.install(globals())
ORIG.update(styles2.STYLES2); TIERS.update(styles2.TIERS2); details.DETAILS.update(styles2.DETAILS2)
import features, scenes
if CFG.get("new_details", True): features.extend(details)
import style_details
if CFG.get("style_details", True): style_details.extend(details)          # twenty more per style, found in that style only (sd_a.py to sd_e.py)
else: features.LANDMARKS.update(features._landmarks())
NOTES.update({k: [""] * 4 for k in styles2.STYLES2})
import contextlib

@contextlib.contextmanager
def design_space():
    """run layout code written for the 30 mm base: the size globals read 15 mm radius inside this block"""
    keep = (gg.R0, gg.CLIP, gg.BLANK, gg2.R0, gg2.CUTR, gg.K)
    gg.R0 = gg2.R0 = 15.0; gg.CLIP = 15.15; gg.BLANK = 15.3; gg2.CUTR = 16.0; gg.K = 1.0
    try: yield
    finally: gg.R0, gg.CLIP, gg.BLANK, gg2.R0, gg2.CUTR, gg.K = keep


def scale_scene(s, k):
    """scale every footprint of a laid-out scene by k about the centre (heights stay); object identity is kept
    consistent between the lists, which the placement logic relies on"""
    memo = {}
    def sc(p):
        if id(p) not in memo: memo[id(p)] = (affinity.scale(p, k, k, origin=(0, 0)), p)
        return memo[id(p)][0]
    s.ops = [(op[0], [sc(p) for p in op[1]]) + tuple(op[2:]) for op in s.ops]
    s.gcuts = [([sc(p) for p in ps], d, kd) for ps, d, kd in s.gcuts]
    s.tcuts = [([sc(p) for p in ps], z0, z1, kd) for ps, z0, z1, kd in s.tcuts]
    s.feet = [sc(p) for p in s.feet]
    for c in getattr(s, "chops", []):
        c["poly"] = sc(c["poly"]); c["ops"] = [(op[0], [sc(p) for p in op[1]]) + tuple(op[2:]) for op in c["ops"]]
    if hasattr(s, "terr"): s.terr = [(sc(fp), zb, zt, sc(top)) for fp, zb, zt, top in s.terr]
    for nm_ in ("objs", "keepout"):
        if hasattr(s, nm_): setattr(s, nm_, [sc(p) for p in getattr(s, nm_)])
    s.disc = Point(0, 0).buffer(gg.CLIP, 256); s.cdisc = Point(0, 0).buffer(gg2.CUTR, 256)


def make(style, n):
    if n > 20:      # 21+: same 4 detail tiers in turn (21-25 like 1-5 ...), each with its own seed so every one is new
        k = n; n = (n - 1) % 20 + 1
        os.environ["SALT"] = os.environ.get("SALT", "") + f"x{k}"
        s = make(style, n)
        s.name, s.title = f"{style}_{k:02d}", s.title.rsplit("#", 1)[0] + f"#{k}"
        return s
    t = (n - 1) // 5                          # the number sets the detail tier: 1-5 simplest ... 16-20 most detailed
    salt = os.environ.get("SALT", "") + os.environ.get("RESEED", "")
    RND = bool(salt) and CFG.get("random_mix", True)
    mixr = random.Random(f"{style}{n}mix{salt}")
    if style in styles2.STYLES2: nb, suf = n, ""
    elif style == "corridor": nb, suf = n, ""
    elif style == "urban": nb, suf = n, ""
    else: nb, suf = (n - 1) % 10 + 1, ("" if n <= 10 else "b")
    if RND:                                   # complete randomness within the style: any layout family, any feature mix
        nb = n if style in styles2.STYLES2 else mixr.randint(1, 20) if style in ("corridor", "urban") else mixr.randint(1, 10)
        suf += f"n{n}"                            # the layout seed is this base's own: same layout family never means same layout
        gg2.FMIX = mixr
        gg2.SCENE_SKIP = {"track", "ruts", "planks", "trail"} if CFG.get("scenes", False) else set()
    suf += os.environ.get("RESEED", "") + os.environ.get("SALT", "")   # RESEED=r1: retry variant. SALT: per-run random seed so every run makes new bases
    gg2.mk = lambda st, title, k, ground: gg2.Scene(f"{st}_{n:02d}", f"{title} #{n}", f"{st}{k}{suf}", ground)
    gg.LAYOUT = True                              # while the ground is laid out, scatter counts follow the base area
    try: s = ORIG[style](nb)
    finally: gg2.mk = _mk0; gg2.FMIX = None; gg.LAYOUT = False
    s.name, s.title = f"{style}_{n:02d}", s.title.rsplit("#", 1)[0] + f"#{n}"
    if terr(s): T_init(s)
    else:
        s.objs, s.keepout = [], []
    s.base_ops = {id(o) for o in s.ops}
    rng = random.Random(f"{style}{n}tier" + os.environ.get("SALT", "") + os.environ.get("RESEED", ""))
    fns = [fn for lvl in range(t, 0, -1) for fn in TIERS[style][lvl - 1]]     # newest tier first: its features get room
    if RND and fns:                           # a random pick of the extras up to this tier: more of them the higher the tier
        k = max(t, min(len(fns), round(len(fns) * mixr.uniform(0.55, 1.0))))
        keep = set(mixr.sample(fns, k)); fns = [f for f in fns if f in keep]
    fns = [f for f in fns if f not in LATE] + [f for f in fns if f in LATE]
    features.install(globals()); details.install(globals()); scenes.install(globals()); style_details.install(globals())
    FOCUS[0] = AIM[0] = None
    if CFG.get("scenes", False):              # every base is one place: a subject, a way to it, things that belong to them, open ground
        try: scenes.compose(s, style, mixr, rng, t, fns)
        finally: FOCUS[0] = AIM[0] = None; HBOOST[0] = 1.0; scenes.release(s)
        return s
    HBOOST[0] = float(CFG.get("big_base_height_factor", 1.5)) if gg.BASE_MM >= 59.9 else 1.0
    if gg.BASE_MM >= 59.9 and CFG.get("landmarks", True):    # 60 mm and up: one to four landmark features, placed first while there is room
        features.landmarks(s, style, mixr)
    if gg.K > 1.15 and CFG.get("set_pieces", True):          # bigger bases get large set pieces to anchor the scene
        details.install(globals()); details.anchors(s, style, mixr)
    HBOOST[0] = 1.0
    reps = max(1, min(10, round(gg.K ** 1.4)))     # bigger base, more room: features repeat (a little slower than the area)
    for rep in range(reps):
        for fn in fns:
            if rep and mixr.random() < (0.75 if fn in SPARSE else 0.35): continue
            try: fn(s, rng, t)
            except Exception as e: print("  !", style, n, getattr(fn, "__name__", "detail"), type(e).__name__, e, flush=True)
    if CFG.get("extra_details", True):       # random extras from the theme's detail library (details.py)
        details.install(globals()); details.apply(s, style, mixr, t)
    return s

def scene_report(s):
    """what the runner writes down about a base's scene: theme, where the plan came from, the plan itself"""
    if not getattr(s, "plan", None): return {}
    import hashlib
    plan = {k: v for k, v in s.plan.items() if not k.startswith("_")}
    return {"theme": getattr(s, "theme", ""), "plan": plan, "plan_line": getattr(s, "plan_line", ""), "plan_hash": hashlib.md5(json.dumps(plan, sort_keys=True).encode()).hexdigest()[:10],
            "scene_log": getattr(s, "scene_log", [])[:8]}

def smooth_isolated(s, tol=None):
    """Drop outline points (shape change < tol mm) only on pieces that touch nothing else: loose pebbles, rocks, grit.
    Pieces that meet other pieces keep their exact outlines so shared edges still line up (no hairline cracks)."""
    from shapely.strtree import STRtree
    tol = CFG.get("sketch_tolerance_mm", 0.03) if tol is None else tol
    gg.SKETCH_TOL = 0
    if not tol: return
    items = [(i, j, p) for i, op in enumerate(s.ops) for j, p in enumerate(op[1])]
    others = [p for ps, d, k in s.final_gcuts() for p in ps] + [p for ps, z0, z1, k in s.tcuts for p in ps]
    geoms = [p for _, _, p in items] + others
    owner = [i for i, _, _ in items] + [-1] * len(others)
    tree = STRtree(geoms); gap = 2 * tol + 0.01
    new = {i: list(op[1]) for i, op in enumerate(s.ops)}
    for idx, (i, j, p) in enumerate(items):
        near = tree.query(p.buffer(gap))
        touching = any(k != idx and p.boundary.distance(geoms[k].boundary) < gap and
                       (owner[k] != i or geoms[k].intersects(p)) for k in near)
        q = p.simplify(CFG.get("touching_tolerance_mm", 0.005) if touching else tol, preserve_topology=True)
        if q.geom_type == "Polygon" and not q.is_empty: new[i][j] = q
    s.ops = [(op[0], new[i]) + tuple(op[2:]) for i, op in enumerate(s.ops)]

class Stuck(Exception): pass

def build(style, n):
    try: return _build(style, n)
    except Stuck as e: return {"name": f"{style}_{n:02d}", "error": str(e)}

def _build(style, n):
    """Same build as gg2.build, but through the watchdog client: a hung step is skipped instead of hanging the base."""
    import time
    import sc_client as scw
    from gg import sk
    S = scw.Session(); s = make(style, n); t = time.time(); skipped = []; cnt = [0]
    if os.environ.get("BASENAME"): s.name = os.environ["BASENAME"]
    import hashlib
    SIG = hashlib.md5(repr([(op[0], op[-1], round(op[2], 2), [[round(v, 1) for c in p.exterior.coords for v in c] for p in op[1]]) for op in s.ops]).encode()).hexdigest()[:16]
    smooth_isolated(s)
    S.tool("new_design", {"name": s.name})
    def join_split(op, polys):
        halves = [polys[:len(polys) // 2], polys[len(polys) // 2:]] if len(polys) > 1 else [polys]
        for h in halves:
            if not h: continue
            cnt[0] += 1; nm = f"X{cnt[0]}"
            if op[0] == "prism": e = {"sketch": nm, "distance": round(op[3] - op[2], 4), "operation": "join", "targets": ["Base"], "name": "F" + nm}
            else: e = {"sketch": nm, "distance": op[3], "taper": -op[4], "operation": "join", "targets": ["Base"], "name": "F" + nm}
            r = S.tool("batch", {"commands": sk(nm, op[2], h, rim=gg._rim(gg.clip_z(op), gg.op_k(op)) if op[0] == "prism" else None) + [{"command": "solid.extrude", "params": e}], "verbose": False})
            if r["isError"]:
                if len(h) == 1: skipped.append(op[-1])
                else: join_split(op, h)
    from gg import RIM
    def trim_retry():
        """The 12 deg rim trim sometimes fails on a face clash: retry with a slightly different trim cone."""
        for k, (dr, seg, rot) in enumerate(((0.012, 388, 0.7), (-0.009, 380, 1.3), (0.021, 396, 2.1)), 1):
            cmds = (gg.sk_circle(f"Trim{k}", 0, R0 + dr) if CFG.get("true_circle_rim", True) else
                    sk(f"Trim{k}", 0, [affinity.rotate(Point(0, 0).buffer(R0 + dr, seg // 4), rot, origin=(0, 0))])) + [
                {"command": "solid.extrude", "params": {"sketch": f"Trim{k}", "distance": 8.0, "taper": -RIM, "operation": "new", "name": f"TrimF{k}", "body_name": f"Trim{k}"}},
                {"command": "solid.combine", "params": {"target": "Base", "tools": [f"Trim{k}"], "operation": "intersect", "keep_tools": False}}]
            rr = S.tool("batch", {"commands": cmds, "verbose": False}, limit=TRIM_LIMIT)
            if not rr["isError"]: return True
            if "watchdog" in " ".join(rr["text"]): return False      # a hang means this layout won't trim: don't burn restarts, swap variant
        return False
    SLOW = float(CFG.get("slow_step_secs", 10))
    TRIM_LIMIT = float(CFG.get("trim_limit_secs", 10))       # the trim normally takes under 1 s
    budget = float(CFG.get("chamfer_budget_secs", 45)) * max(1.0, gg.K ** 2); spent = 0.0; GT = []
    pending = []                                   # piece bodies waiting to be merged into Base
    def top_edges(body, z):
        rr = S.tool("body_topology", {"body": body}, keep=False, limit=20)
        tx = " ".join(rr["text"])
        try: j = json.JSONDecoder().raw_decode(tx[tx.index("{"):])[0]
        except Exception: return []
        return [{"body": body, "index": e["index"]} for e in j.get("edges", []) if abs(e.get("mid", [0, 0, -9])[2] - z) < 2e-3]
    pend = {"op": None}                            # pending bodies are all joins or all cuts
    FOOT = {}                                      # pending join body -> (footprint, z bottom, z top)
    merged = {"fp": []}                            # footprints of raised bodies already merged into Base
    def split_loops(mid):
        """the separate closed outlines in a sketch: [(polygon, [commands])]. A closed line is one outline; a run of open
        lines and arcs is one outline once it comes back to where it started. None if a run never closes."""
        out = []; run = []; pts = []
        for c in mid:
            p = c["params"]
            if c["command"] == "sketch.line" and p.get("closed"):
                if run: return None
                if len(p["points"]) >= 3: out.append((Polygon(p["points"]).buffer(0), [c]))
                continue
            if c["command"] == "sketch.line": seg = list(p["points"])
            elif c["command"] == "sketch.arc.three_point": seg = [p["start"], p["through"], p["end"]]
            else: return None
            run.append(c); pts += seg
            if len(pts) >= 4 and math.hypot(pts[-1][0] - pts[0][0], pts[-1][1] - pts[0][1]) < 2e-3:
                out.append((Polygon(pts).buffer(0), run)); run = []; pts = []
        return None if run else out
    def footprint(cmds):
        """2D outline(s) and z range of what a sketch+extrude group makes"""
        polys = []; open_pts = []; z = None; zb = zt = None
        for c in cmds:
            p = c["params"]
            if c["command"] == "sketch.create":
                if open_pts and len(open_pts) >= 3: polys.append(Polygon(open_pts).buffer(0))
                open_pts = []; z = p.get("offset", 0.0)
            elif c["command"] == "sketch.line":
                if p.get("closed") and len(p["points"]) >= 3: polys.append(Polygon(p["points"]).buffer(0))
                else: open_pts += p["points"]
            elif c["command"] == "sketch.arc.three_point": open_pts += [p["start"], p["through"], p["end"]]
            elif c["command"] == "solid.extrude" and z is not None:
                d = p.get("distance", 0.0); lo, hi = (z - d, z) if p.get("direction") == "negative" else (z, z + d)
                zb = lo if zb is None else min(zb, lo); zt = hi if zt is None else max(zt, hi)
        if open_pts and len(open_pts) >= 3: polys.append(Polygon(open_pts).buffer(0))
        polys = [q for q in polys if not q.is_empty]
        return (unary_union(polys) if polys else Polygon()), (zb or 0.0), (zt or 0.0)
    def flush():
        """put the queued bodies into Base. One combine per body (much faster in SolidCraft than one combine with many
        tools), all sent in a single call; a body that will not go in is left out and reported."""
        if not pending: return
        t0 = time.time(); opn = pend["op"]
        def comb(b): return {"command": "solid.combine", "params": {"target": PAR.get(b, "Base") if opn == "join" else "Base", "tools": [b], "operation": opn, "keep_tools": False}}
        def leftovers(rr, mine):                   # a tool with several separate parts leaves the others as "X (2)" bodies
            tx = " ".join(rr["text"])
            try: j = json.JSONDecoder().raw_decode(tx[tx.index("{"):])[0]
            except Exception: return []
            out = []
            for res in j.get("results") or []:
                for b in (res or {}).get("bodies", []) if isinstance(res, dict) else []:
                    if b != "Base" and b.split(" (")[0] in mine and b not in out: out.append(b)
            return out
        tools = list(pending); mine = {b.split(" (")[0] for b in tools}; done = set()
        if opn == "join":
            # a small piece standing wholly on a bigger queued piece (a rivet on a plate) goes into THAT piece first: joining
            # two small bodies takes a few hundredths of a second, joining into the busy base takes half a second each
            PAR.clear()
            if CFG.get("join_on_parent", True):
                fb = [b for b in tools if b in FOOT]
                for b in fb:
                    pb, zb, zt = FOOT[b]; best = None
                    for q in fb:
                        if q is b: continue
                        pq, qb, qt = FOOT[q]
                        if qb < zb - 0.05 and qt > zb + 0.05 and qt < zt and pq.area > 2 * pb.area and pq.buffer(-0.08).contains(pb):
                            if best is None or FOOT[q][1] > FOOT[best][1]: best = q
                    if best: PAR[b] = best
                for b in list(PAR):                    # a rider on a rider goes into the same bottom piece, in the same combine
                    q = PAR[b]; n_ = 0
                    while q in PAR and n_ < 8: q = PAR[q]; n_ += 1
                    PAR[b] = q
            z = lambda b: FOOT[b][1] if b in FOOT else 0.0
            kids = sorted([b for b in tools if b in PAR], key=z, reverse=True)       # highest first, so a piece has its own riders before it rides
            tools = kids + sorted([b for b in tools if b not in PAR], key=z)         # then lowest first into the base
        for _ in range(5):
            if not tools: break
            cl = [comb(b) for b in tools]
            if opn == "join" and PAR and CFG.get("join_on_parent_bulk", True) and _ == 0:      # all riders of one piece in a single combine
                byp = {}
                for b in tools:
                    if b in PAR: byp.setdefault(PAR[b], []).append(b)
                cl = [{"command": "solid.combine", "params": {"target": q, "tools": bs, "operation": "join", "keep_tools": False}} for q, bs in byp.items()] \
                     + [comb(b) for b in tools if b not in PAR]
                for c in cl: c["params"]["tools"].sort(key=lambda b: FOOT[b][1] if b in FOOT else 0.0)
            left = []; CH = int(CFG.get("merge_chunk", 12))
            for ci in range(0, len(cl), CH):               # a few at a time: each call has its own time limit, so a big base is never
                part = cl[ci:ci + CH]                      # mistaken for a hung one just because it has many pieces
                r = S.tool("batch", {"commands": part}, limit=20 + 8 * len(part), restart=False)
                if "watchdog" in " ".join(r["text"]): raise Stuck(f"merge hung (bodies {ci + 1}-{ci + len(part)} of {len(cl)})")
                if r["isError"]:                       # one by one
                    for c in part:
                        r1 = S.tool("batch", {"commands": [c]}, limit=15, restart=False)
                        if "watchdog" in " ".join(r1["text"]): raise Stuck(f"merge of {c['params']['tools'][0]} hung")     # session is busy: give up on this variant now
                        if r1["isError"]:
                            if "not found" not in " ".join(r1["text"]).lower(): skipped.extend(c["params"]["tools"])          # stays behind as a loose body; only Base is exported
                        else: left += [x for x in leftovers(r1, mine) if x not in done]
                else: left += [x for x in leftovers(r, mine) if x not in done]
            done.update(tools); tools = [x for x in left if x not in done]
        GT.append((time.time() - t0, f"{opn} {len(pending)} bodies"))
        if opn == "join": merged["fp"] += [FOOT[b][0] for b in pending if b in FOOT]
        FOOT.clear(); pending.clear()
    PAR = {}
    def queue(body, opn, fp=None):
        if pend["op"] != opn: flush(); pend["op"] = opn
        pending.append(body)
        if opn == "join" and fp is not None: FOOT[body] = fp
    for g in gg.build_cmds(s):
        if pending and isinstance(g, gg.Group) and g.polish: flush()      # grate chamfers need the slots cut first
        if isinstance(g, gg.Group) and g.piece:    # chamfered piece: own body, top edges by index, equal-distance chamfer
            if pend["op"] not in (None, "join"): flush(); pend["op"] = "join"
            t0 = time.time()
            r = S.tool("batch", {"commands": g, "verbose": False}, limit=20)
            if r["isError"]:
                for fg in g.fallback:
                    if S.tool("batch", {"commands": fg, "verbose": False})["isError"]: skipped.append(g.piece["body"])
                continue
            ed = top_edges(g.piece["body"], g.piece["top"]) if (g.piece["dist"] and spent <= budget) else []
            done = False
            for d in (g.piece["dist"] if ed else []):
                if not S.tool("batch", {"commands": [{"command": "solid.chamfer", "params": {"edges": ed, "distance": d}}], "verbose": False}, limit=20)["isError"]: done = True; break
            if not done and ed: S.fb = getattr(S, "fb", 0) + 1
            queue(g.piece["body"], "join", footprint(g)); spent += time.time() - t0
            GT.append((time.time() - t0, g.piece["body"]))
            continue
        if (not isinstance(g, gg.Group) and g[-1]["command"] == "solid.extrude" and g[-1]["params"].get("operation") == "cut"
                and str(g[-1]["params"].get("name", "")).startswith("FT") and pend["op"] == "join" and pending and CFG.get("cut_in_pieces", True)):
            # a cut into the top of raised pieces: cut it from those pieces while they are still small separate bodies
            tfp, tzb, tzt = footprint(g)
            tg = [b for b in pending if b in FOOT and FOOT[b][2] > tzb + 0.02 and FOOT[b][0].intersects(tfp)]
            rest = tfp.difference(unary_union([FOOT[b][0] for b in tg]).buffer(0.05)) if tg else tfp
            hits_merged = any(m.intersects(tfp) for m in merged["fp"]) or tzb < gg.H - 0.01
            if tg and not hits_merged:
                g2 = [dict(c) for c in g]; g2[-1] = {"command": "solid.extrude", "params": dict(g[-1]["params"], targets=tg)}
                # a cut that ends exactly on a piece's outline leaves faces lying on each other and the later merge hangs:
                # where the cut comes near the edge of a piece, carry it on past the edge (it only cuts these pieces anyway)
                lps = [Polygon(c["params"]["points"]).buffer(0) for c in g[1:-2] if c["command"] == "sketch.line" and c["params"].get("closed") and len(c["params"]["points"]) >= 3]
                U = unary_union([FOOT[b][0] for b in tg])
                if (len(lps) == len(g) - 3 and lps and not any(a is not b and a.contains(b) for a in lps for b in lps)
                        and not unary_union(lps).buffer(0.12).within(U)):
                    cutp = unary_union(lps); grown = cutp.union(cutp.buffer(0.3, join_style=2).difference(U.buffer(-0.13))).buffer(0)
                    parts = [q for q in (grown.geoms if hasattr(grown, "geoms") else [grown]) if q.geom_type == "Polygon" and q.area > 0.01]
                    if parts and not any(q.interiors for q in parts):
                        g2 = gg.sk(g[0]["params"]["name"], g[0]["params"]["offset"], parts) + [g2[-1]]
                t0 = time.time(); r = S.tool("batch", {"commands": g2, "verbose": False}, limit=20 + 2 * len(tg), restart=False)
                GT.append((time.time() - t0, str(g[-1]["params"].get("name")) + " in pieces"))
                if "watchdog" in " ".join(r["text"]): raise Stuck("cut into pieces hung")
                if not r["isError"]: continue
            elif not tg and not hits_merged: continue          # the cut touches nothing that exists: nothing to do
        plain_group = isinstance(g, gg.Group) and not g.piece and not g.polish and not g.alts      # an unchamfered slab: treat like any raised piece
        opn = g[-1]["params"].get("operation") if (plain_group or not isinstance(g, gg.Group)) and g[-1]["command"] == "solid.extrude" else None
        LP = split_loops(g[1:-2]) if opn in ("join", "cut") else None       # the separate outlines in this sketch (None: could not tell them apart)
        if opn == "cut" and LP is None and sum(1 for c in g if c["command"] == "sketch.line" and c["params"].get("closed")) + (1 if any(c["command"].startswith("sketch.arc") for c in g) else 0) > 1:
            opn = None                               # several outlines that cannot be told apart: cut it directly
        if opn in ("join", "cut") and CFG.get("merge_joins", True) and str(g[-1]["params"].get("name")) != "MagCut":
            # raised piece or cut: made as its own body now, all of a kind go into Base together (one boolean, not dozens)
            if pend["op"] not in (None, opn): flush(); pend["op"] = opn      # switch join <-> cut: finish the other kind first
            mid = g[1:-2]; p0 = g[-1]["params"]; nm0 = str(p0.get("name", f"J{len(pending)}"))
            if LP and len(LP) > 1 and CFG.get("body_per_outline", True):
                # one body per outline, all in ONE batch call: each merges whole, nothing is left over for a second pass
                cmds = []; names = []
                lp = [q for q, _ in LP]                                                  # an outline inside another is a hole in it:
                outer = [i for i in range(len(LP)) if not any(j != i and lp[j].contains(lp[i]) for j in range(len(LP)))]
                sets = [LP[i][1] + [c for j in range(len(LP)) if j not in outer and lp[i].contains(lp[j]) for c in LP[j][1]] for i in outer]   # keep them in one sketch
                mid = [LP[i][1] for i in outer]
                for k, ss in enumerate(sets):
                    sn = f"{g[0]['params']['name']}o{k}"; p = dict(p0); bn = f"B{nm0}o{k}"
                    p.update({"sketch": sn, "operation": "new", "body_name": bn, "name": f"{nm0}o{k}"}); p.pop("targets", None)
                    cmds += [{"command": "sketch.create", "params": dict(g[0]["params"], name=sn)}] + ss + [g[-2], {"command": "solid.extrude", "params": p}]; names.append(bn)
                t0 = time.time(); r = S.tool("batch", {"commands": cmds, "verbose": False}, limit=min(scw.LIMIT + 0.4 * len(cmds), SLOW + 2))
                GT.append((time.time() - t0, nm0))
                if not r["isError"]:
                    for bn, c in zip(names, mid): queue(bn, opn, footprint([g[0]] + c + [g[-1]]) if opn == "join" else None)
                    continue
            g2 = [dict(c) for c in g]; p = dict(p0); bn = "B" + nm0
            p.update({"operation": "new", "body_name": bn}); p.pop("targets", None); g2[-1] = {"command": "solid.extrude", "params": p}
            t0 = time.time(); r = S.tool("batch", {"commands": g2, "verbose": False}, limit=min(scw.LIMIT + scw.PER_CMD * len(g2), SLOW + 2))
            GT.append((time.time() - t0, nm0))
            if not r["isError"]: queue(bn, opn, footprint(g) if opn == "join" else None); continue
            # could not make it as a body: fall through and do it the old way
        flush()                                    # anything done directly must see everything queued before it
        if isinstance(g, gg.Group) and spent > budget:       # chamfer time used up: single solids without chamfer, polish skipped
            for fg in (g.fallback or ([] if g.polish else [g])):
                if S.tool("batch", {"commands": fg, "verbose": False})["isError"]: skipped.append("rock")
            continue
        is_trim = g[-1]["command"] == "solid.combine"
        lim = TRIM_LIMIT if is_trim else 20 if isinstance(g, gg.Group) else min(scw.LIMIT + scw.PER_CMD * len(g), SLOW + 2)
        r = S.tool("batch", {"commands": g, "verbose": False}, limit=lim, restart=not (lim >= SLOW and not isinstance(g, gg.Group)))
        if not isinstance(g, gg.Group) and "watchdog" in " ".join(r["text"]) and lim >= SLOW:      # a layout that makes SolidCraft crawl
            S.close(); return {"name": s.name, "error": f"slow step {g[-1]['params'].get('name', '?')} ({len(g)} cmds) > {SLOW:.0f}s"}
        if is_trim and "watchdog" in " ".join(r["text"]):
            S.close(); return {"name": s.name, "error": f"rim trim hung >{TRIM_LIMIT:.0f}s"}
        secs, _, ncmd = S.timings[-1]
        GT.append((secs, f"{g[-1]['params'].get('name') or g[-1]['command']}({ncmd})"))
        if isinstance(g, gg.Group): spent += secs
        if secs > SLOW and not isinstance(g, gg.Group):      # a layout that makes SolidCraft crawl: drop it now, the runner builds a fresh variant
            S.close(); return {"name": s.name, "error": f"slow step {g[-1]['params'].get('name', 'trim')} ({ncmd} cmds) {secs:.0f}s > {SLOW:.0f}s"}
        if r["isError"] and g[-1]["command"] == "solid.combine":
            if trim_retry(): continue
            S.close(); return {"name": s.name, "error": "rim trim failed 4 ways"}
        if r["isError"] and isinstance(g, gg.Group):
            for alt in g.alts:              # try the chamfer smaller, rock by rock
                t0 = time.time()
                if not S.tool("batch", {"commands": alt, "verbose": False})["isError"]: r = {"isError": False}; break
                spent += time.time() - t0
            if not r["isError"]: continue
            if g.polish: continue            # grate chamfer refused: the slot is already cut, just leave the edge sharp
            for fg in g.fallback:           # chamfer refused: build this rock from its tiers instead
                fr = S.tool("batch", {"commands": fg, "verbose": False})
                if fr["isError"]: skipped.append("rock")
            S.fb = getattr(S, "fb", 0) + 1
            continue
        if r["isError"]:
            nm = g[-1]["params"].get("name", "?")
            if nm in ("TrimF", "MagCut") or g[0]["params"]["name"] == "Blank":
                S.close(); return {"name": s.name, "error": " ".join(r["text"])[:300]}
            if nm.startswith("FJ"):
                op = s.ops[int(nm[2:])]; join_split(op, gg.op_clip(op) if CFG.get("base_first", True) else op[1])
            else: skipped.append(nm)
    flush()
    D = os.environ.get("SETDIR", CFG["out_folder"]); os.makedirs(f"{D}/{style}", exist_ok=True)
    S.tool("export", {"path": f"{D}/{style}/{s.name}.stl", "bodies": ["Base"]}, keep=False, limit=180)   # only the base: stray tool bodies never ship
    S.shot(f"{D}/{style}/{s.name}_iso.png", view="iso", width=600, height=470, grid=False, axes=False)
    S.close()
    r = {"name": s.name, "secs": round(time.time() - t), "skipped": skipped, "restarts": S.restarts, "rock_fallbacks": getattr(S, "fb", 0), "sig": f"{style}{gg.BASE_MM:g}" + SIG, "variant": os.environ.get("RESEED", ""),
         "slowest": "; ".join(f"{nm} {sec:.0f}s" for sec, nm in sorted(GT, reverse=True)[:3]), "merges": [nm for sec, nm in GT if "bodies" in nm],
         "steps": len(S.timings), "step_secs": round(sum(x[0] for x in S.timings), 1)}
    r.update(scene_report(s)); return r

def sheet(style, path):
    import matplotlib; matplotlib.use("Agg"); import matplotlib.pyplot as plt
    fig, axs = plt.subplots(4, 5, figsize=(20, 19))
    for n, ax in zip(range(1, 21), axs.flat):
        s = make(style, n)
        try: gg.build_cmds(s)          # same clean-up the real build does (pieces off the rim and off cuts, grouping)
        except Exception as e: print("  ! preview clean-up", style, n, type(e).__name__, e, flush=True)
        render(s, ax); ax.set_title(f"{style} {gg.BASE_MM:g} mm #{n}" + (f": {s.theme}" if getattr(s, "theme", "") else ""), fontsize=10)
    plt.subplots_adjust(left=0.01, right=0.99, top=0.95, bottom=0.005, wspace=0.04, hspace=0.36)
    for r in range(4):
        p = axs[r, 0].get_position()
        fig.text(0.012, p.y1 + 0.033, f"Detail {r + 1}: " + ("simplest", "more detail", "detailed", "most detailed")[r], fontsize=14, weight="bold", va="bottom", color="#222")
    fig.savefig(path, dpi=80); plt.close(fig)

if __name__ == "__main__":
    if sys.argv[1] == "build":       # python3 gg3.py build <style> <n>   (same SolidCraft build as gg2.py, with the tier features)
        import json
        st, n = sys.argv[2], int(sys.argv[3])
        print(json.dumps(build(st, n)), flush=True); sys.exit()
    # python gg3.py <style|all> <folder> [seed] [--size N] [--scenes on|off] [--theme "name"] [--plan file.json] [--designer on|off]   2D preview sheets, no SolidCraft needed
    if "--size" in sys.argv: i = sys.argv.index("--size"); del sys.argv[i:i + 2]     # already read by gg.py
    def _flag(name):
        if name in sys.argv: i = sys.argv.index(name); v = sys.argv[i + 1]; del sys.argv[i:i + 2]; return v
    _v = _flag("--scenes")
    if _v: CFG["scenes"] = _v == "on"
    _v = _flag("--theme")
    if _v: os.environ["THEME"] = _v
    _v = _flag("--plan")
    if _v: os.environ["PLAN_JSON"] = os.path.abspath(_v)
    _v = _flag("--designer")
    if _v: os.environ["KIT_DESIGNER"] = _v
    _v = _flag("--mini")
    if _v: os.environ["KIT_MINI"] = _v
    out = sys.argv[2]; os.makedirs(out, exist_ok=True)
    if CFG.get("always_new", True) and not os.environ.get("SALT"):
        os.environ["SALT"] = sys.argv[3] if len(sys.argv) > 3 else "".join(random.choice("abcdefghjkmnpqrstuvwxyz23456789") for _ in range(5))
        print("Saving previews to:", os.path.abspath(out), flush=True)
        print("seed:", os.environ["SALT"], f"(python run_set.py <style> 20 --size {gg.BASE_MM:g} --seed", os.environ["SALT"], "builds these same 20 bases)", flush=True)
    for st in (TIERS if sys.argv[1] == "all" else [sys.argv[1]]):
        fp = os.path.abspath(os.path.join(out, f"{st}_{gg.BASE_MM:g}mm_preview.png"))
        sheet(st, fp); print("saved", fp, flush=True)
    print("All previews are in:", os.path.abspath(out), flush=True)
