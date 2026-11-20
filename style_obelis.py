"""Obelis Tombs (2026-11-19): an ancient alien tomb world. Sleek black monoliths on stepped plinths, slabs cut with
the straight-line glyphs of a script nobody reads any more, pylons half sunk in the sand, broken panels of smooth
metal, sealed caskets in rows, and sand drifting over polished stone.

The look is clean angular geometry: chamfered blocks, slots, stepped plinths, rings. The glyphs (ob_glyph) are this
style's own: each one sits in a square cell and is built only from straight strokes on a four-by-four lattice: an
open bracket or a nested frame, then one or two of a bar, a stem, a staircase, a chevron or a square dot. No curves,
no figures.

Everything the kit needs from a style is in this file (see newstyles.py); its style-only details are in
sd_r_x_obelis.py and their real sizes in scale/real_obelis.json."""
import math, random
from shapely.geometry import Point, Polygon, LineString, box
from shapely.ops import unary_union
from shapely import affinity

NAME = "obelis"
TITLE = "Obelis Tombs"
SAYS = ("an ancient alien tomb world: sleek black monoliths on stepped plinths, slabs cut with straight geometric glyph "
        "lines, half-buried pylons, broken panels of smooth metal, sealed caskets, sand drifting over polished stone. "
        "Clean angular shapes only: chamfered blocks, slots, steps and rings")
GROUND = "#a89270"
PAL = dict(wood="obmetal", iron="obmetal", rock="obsidian", stone="monolith", crate="casket", water="water", pit="slot",
           gem="obglow", floor="obfloor", plant=None, soft="sand", bone="bone", twig="obmetal", vein="obmetal")
PATH_OK = "way stones trail cable"
EXTRA_PROPS = "grate vent cables coins"
BUILT, ENCLOSED, CLIFFS = True, False, True
COLOURS = {"monolith": "#1e1f26", "obfloor": "#4b4954", "obmetal": "#8d939e", "casket": "#34323c", "sand": "#cdb68a",
           "obglow": "#5fc4b8", "obinlay": "#b49a52"}
FLOORS = ("obfloor",)          # floor kinds: things may stand across them (gg3.BRIDGE)
LOOSE = ("sand",)              # loose natural kinds: gather as debris (scenes.DEBRIS)


def _T(name, subject, path, props, also="", **kw):          # the same record as themes.T (not imported: themes loads this file)
    return dict(theme=name, subject=subject, path=path, props=props, also=also, **kw)

THEMES = [
    _T("tomb gate", "portal", "way:to", "front:steps wall:slab*2 back:tiles path:coins", also="graves:far", clutter=dict(growth=0.0)),
    _T("fallen monolith", "statue", "stones:to", "at:slab*2 at:rubble front:tiles far:grate", also="crater:far", clutter=dict(debris=1.0, growth=0.0)),
    _T("pylon array", "colonnade", "way:past", "path:grate at:cables path:slab back:crystals", also="crater:far", clutter=dict(growth=0.0)),
    _T("sarcophagus field", "graves", "way:to", "at:slab at:chest back:tiles path:coins", also="statue:far", clutter=dict(growth=0.0)),
    _T("glyph plaza", "dais_altar", "way:to", "front:tiles at:slab*2 front:steps back:grate", also="well:far", clutter=dict(growth=0.0)),
]


# ------------------------------------------------------------------------------------------------ the glyphs
_Q = (-0.4, -0.133, 0.133, 0.4)          # the lattice every stroke runs on, in shares of the cell

def _stroke(pts, w):
    return LineString(pts).buffer(w / 2, cap_style=2, join_style=2, mitre_limit=4.0)

def ob_glyph(cx, cy, s, a, rng, w=0.25):
    """one glyph of the tomb script in a square cell of side s turned to angle a: a frame (a bracket open on one
    side, or a nested square) and one or two marks inside it, all straight strokes w wide. Returns the grooves."""
    w = min(w, 0.14 * s); q = [v * s for v in _Q]; c, sn = math.cos(a), math.sin(a)          # strokes never so wide that the ridges between them close up
    P = lambda u, v: (cx + u * c - v * sn, cy + u * sn + v * c)
    L = lambda pts: _stroke([P(u, v) for u, v in pts], w)
    lo, hi = q[0], q[3]; g = []
    if rng.random() < 0.6:          # the bracket: three sides of the square
        sides = [[(lo, hi), (lo, lo)], [(lo, lo), (hi, lo)], [(hi, lo), (hi, hi)], [(hi, hi), (lo, hi)]]; k = rng.randint(0, 3)
        run = sides[(k + 1) % 4:] + sides[:k]
        g.append(L([run[0][0]] + [p[1] for p in run]))
    else:
        g.append(L([(lo, lo), (hi, lo), (hi, hi), (lo, hi), (lo, lo)]))
        if rng.random() < 0.5: m = s * 0.18; g.append(L([(-m, -m), (m, -m), (m, m), (-m, m), (-m, -m)]))
    marks = rng.sample(range(6), rng.randint(1, 2))
    for k in marks:
        if k == 0: v = rng.choice((q[1], q[2])); g.append(L([(lo, v), (rng.choice((q[2], hi)), v)]))                       # a bar
        elif k == 1: u = rng.choice((q[1], q[2])); g.append(L([(u, lo), (u, rng.choice((q[1], q[2])))]))                  # a stem
        elif k == 2: g.append(L([(lo, lo), (q[1], lo), (q[1], q[1]), (q[2], q[1]), (q[2], q[2]), (hi, q[2])]))           # a staircase
        elif k == 3: sv = rng.choice((-1, 1)); g.append(L([(q[1], sv * q[2]), (0, -sv * q[1] * 0.5), (q[2], sv * q[2])]) if rng.random() < 0.5 else L([(lo * 0.6, sv * q[2]), (0, -sv * q[1]), (hi * 0.6, sv * q[2])]))          # a chevron
        elif k == 4: u, v = rng.choice(q[1:3]), rng.choice(q[1:3]); d = s * 0.09; g.append(Polygon([P(u - d, v - d), P(u + d, v - d), P(u + d, v + d), P(u - d, v + d)]))          # a square dot
        else: g.append(L([(q[1], q[1]), (q[2], q[1]), (q[2], q[2])]))                                                    # a hook
    return unary_union(g)

def ob_band(cx, cy, a, L, cell, rng, w=0.25, border=True):
    """a row of glyphs along a line L long through (cx, cy) at angle a, each in its own cell, with a ruled line along
    each side of the row"""
    c, sn = math.cos(a), math.sin(a); n = max(1, int(L / (cell * 1.12))); g = []
    for i in range(n):
        u = (i - (n - 1) / 2) * cell * 1.12; g.append(ob_glyph(cx + u * c, cy + u * sn, cell, a, rng, w))
    if border:
        for sv in (-1, 1):
            v = sv * cell * 0.72; h = n * cell * 1.12 / 2
            g.append(_stroke([(cx - h * c - v * sn, cy - h * sn + v * c), (cx + h * c - v * sn, cy + h * sn + v * c)], w))
    return unary_union(g)


# ------------------------------------------------------------------------------------------------ the kit's helpers
_NEED = ("gg", "gg2", "np", "H", "CAPZ", "find", "up", "mound", "cut", "topcut", "place_group", "rect", "frame", "inset", "rp", "aim",
         "stone", "blob", "polys_of", "wide_enough", "floor", "j_grid", "j_hex", "j_voronoi", "_D", "_fr", "banks", "cracks", "grit",
         "KK", "NN", "ring", "radial", "ROCKY", "SOFTT", "HBOOST", "BRIDGE",
         "scatter", "single", "sunk", "trail", "path", "sh_rect", "sh_hex", "sh_ring", "sh_disc", "sh_tri", "sh_round", "sh_blob", "sh_wig",
         "sh_caps", "pat_slots", "pat_seams", "pat_ring", "pat_dots", "pat_hole")

def _take(G):
    globals().update({k: G[k] for k in _NEED if k in G})

def _register(late=False):
    """the style's floor and loose kinds, told to the kit's own sets (newstyles could do this for every style).
    The floor kinds go in at once (tables(), while gg3 loads); the loose kinds when the first base is laid out,
    by which time scenes is loaded."""
    B = globals().get("BRIDGE")
    if B is not None:
        for k in FLOORS: B.add(k)
    if late:
        import sys
        sc = sys.modules.get("scenes")
        if sc is not None:
            for k in LOOSE: sc.DEBRIS.add(k)


def _chamfer(g, c):
    """a rectangle with its corners cut off at 45 degrees"""
    return g.buffer(-c, join_style=2).buffer(c, join_style=3)

def _oct(x, y, r, a=0.0):
    return Polygon([(x + r * math.cos(a + math.pi / 8 + k * math.pi / 4), y + r * math.sin(a + math.pi / 8 + k * math.pi / 4)) for k in range(8)])


# ------------------------------------------------------------------------------------------------ the ground
def _inlay(s, rng, sl, z, phi, n=2, cell=1.9):
    """glyph bands cut into the polished floor, along its slabs"""
    if sl is None or sl.is_empty: return
    keep = inset(sl, 0.7); gs = []
    for _ in range(n):
        c = rp(rng, 0, 9); a = phi + rng.choice((0.0, math.pi / 2)); L = rng.uniform(6, 10) * KK() ** 0.5
        g = ob_band(c[0], c[1], a, L, cell, rng, 0.3).intersection(keep)
        if not g.is_empty: gs.append(g)
    if gs: s.tcut([unary_union(gs)], z - 0.3, z, "slot")

def _grid(phi, sx, sy=None, jw=0.22):
    """the joints of a square-cut floor as separate straight pieces (the kit's j_grid gives one joined net, which the
    bare-ground check reads as a plain plate)"""
    sy = sy or sx; E = 22 * KK()
    hs = [box(-E, y - jw, E, y + jw) for y in np.arange(-E, E, sy)]; hu = unary_union(hs).buffer(0.01)
    vs = [p for x in np.arange(-E, E, sx) for p in polys_of(box(x - jw, -E, x + jw, E).difference(hu))]
    return [affinity.rotate(g, math.degrees(phi), origin=(0, 0)) for g in hs + vs]

def _jfloor(s, region, h, kind, joints, depth=0.38, edge=0.5):
    """floor() with its joints cut piece by piece"""
    sl = floor(s, region, h, kind, None)
    if sl is None or sl.is_empty: return sl
    keep = sl.buffer(-edge); ps = [q for g in joints for q in polys_of(g.intersection(keep)) if q.area > 0.05]
    if ps: s.tcut(ps, H + h - depth, H + h, "joint")
    return sl

def _drifts(s, rng, n, region=None, big=(1.8, 3.0)):
    banks(s, rng, n, big[0], big[1], (0.4, 0.8), "sand", ((0.5, 25), (0.5, 50)), region=region)

def _rim_sand(rng, D, n):
    """where the sand has blown in over the edge of a floor: a few broad lobes at the rim"""
    gs = []
    for _ in range(n):
        t = rng.uniform(0, 6.283); d = gg.R0 * rng.uniform(0.85, 1.0); r = rng.uniform(3.0, 4.5) * KK() ** 0.7
        gs.append(blob(d * math.cos(t), d * math.sin(t), r, rng, 4))
    return unary_union(gs).intersection(D)

def _lay_sand(s, rng, sand):
    ps = [p for p in polys_of(sand.intersection(Point(0, 0).buffer(gg.R0 - 0.6))) if p.area > 2.0]
    if ps: s.stack(ps, rng.uniform(0.5, 0.9), ((0.5, 25), (0.5, 50)), "sand")

def layout(n):
    _register(True)
    s = gg2.mk(NAME, TITLE, n, GROUND); rng = s.rng; v = rng.randint(0, 4); D = _D(); phi, f, R = _fr(rng); s.phi = phi; s.ob_v = v
    if v == 0:          # great slabs of polished stone, glyph bands inlaid along some, sand blown over them
        sand = _rim_sand(rng, D, rng.randint(1, 2) + (gg.K > 2))
        step = rng.uniform(6.0, 7.5); sl = _jfloor(s, D.difference(sand.buffer(0.3)), 0.55, "obfloor", _grid(phi, step))
        _inlay(s, rng, sl, H + 0.55, phi, rng.randint(1, 2) + int(0.8 * gg.K ** 2) if gg.K > 1.2 else rng.randint(1, 2))
        _lay_sand(s, rng, sand)
    elif v == 1:        # a processional band of paving across the sand, its glyph line down the middle
        c = rng.uniform(-3, 3) * gg.K; w = rng.uniform(4.2, 5.2) * KK() ** 0.5
        band = R(-30, 30, c - w, c + w).intersection(D)
        sl = _jfloor(s, band, 0.6, "obfloor", _grid(phi, rng.uniform(4.4, 5.4), 400))
        g = ob_band(*f(0, c), phi, 60 * KK(), 1.9, rng, 0.3).intersection(inset(sl, 0.6)) if sl is not None and not sl.is_empty else None
        if g is not None and not g.is_empty: s.tcut([g], H + 0.3, H + 0.6, "slot")
        _drifts(s, rng, rng.randint(2, 3), region=Point(0, 0).buffer(gg.R0 - 0.8).difference(band.buffer(0.6)))
    elif v == 2:        # paving half buried, set on the diagonal, cracked
        reg = R(-30, 30, rng.uniform(-3, 2) * gg.K, 30).intersection(D)
        sl = _jfloor(s, reg, 0.55, "obfloor", _grid(phi + 0.785, rng.uniform(4.6, 5.4)))
        if gg.K > 1.2: _inlay(s, rng, sl, H + 0.55, phi + 0.785, int(0.4 * gg.K ** 2))
        _drifts(s, rng, rng.randint(2, 3), region=Point(0, 0).buffer(gg.R0 - 0.8).difference(reg.buffer(0.5)))
        cracks(s, rng, 2, 0.35)
    elif v == 3:        # a seal laid in the floor: nested squares turned against each other, tied by straight spokes
        sand = _rim_sand(rng, D, rng.randint(1, 2))
        sl = floor(s, D.difference(sand.buffer(0.3)), 0.55, "obfloor", None)
        cx, cy = rp(rng, 0, 3); st = 3.0 * KK() ** 0.5; gs = []; k = 0; r = st
        while r < gg.R0 * 1.3:
            sq = rect(cx, cy, 2 * r, 2 * r, phi + (math.pi / 4 if k % 2 else 0.0))
            gs.append(sq.exterior.buffer(0.2, cap_style=2, join_style=2)); r += st; k += 1
        gs += [_stroke([(cx + st * math.cos(phi + j * math.pi / 2), cy + st * math.sin(phi + j * math.pi / 2)),
                        (cx + 3 * gg.R0 * math.cos(phi + j * math.pi / 2), cy + 3 * gg.R0 * math.sin(phi + j * math.pi / 2))], 0.4) for j in range(4)]
        keep = sl.buffer(-0.6); sq = unary_union(gs[:k]); parts = [g.intersection(keep) for g in gs[:k]] + [g.difference(sq).intersection(keep) for g in gs[k:]]
        s.tcut([p for g in parts for p in polys_of(g)], H + 0.2, H + 0.55, "joint")          # each ring its own piece: the kit sees every line, not only the outline
        if gg.K > 1.2: _inlay(s, rng, sl.difference(sq.buffer(0.5)), H + 0.55, phi, int(0.4 * gg.K ** 2))
        _lay_sand(s, rng, sand)
    else:               # open sand with slabs of the old floor lying tilted in it, half under the drifts
        m = NN(rng.randint(3, 5)); got = []
        for _ in range(m * 25):
            if len(got) >= m: break
            c = rp(rng, 0, 11); a = phi + rng.uniform(-0.25, 0.25) + rng.choice((0, math.pi / 2)); q = _chamfer(rect(c[0], c[1], rng.uniform(4.0, 6.0), rng.uniform(3.0, 4.2), a), 0.35)
            if Point(0, 0).buffer(gg.R0 - 1.0).contains(q) and all(q.distance(o) > 1.0 for o, _ in got): got.append((q, a))
        for q, a in got:
            h = rng.uniform(0.4, 0.6); top = s.prism([q], H + h, "obfloor")
            if top and rng.random() < 0.7:
                c = q.centroid; g = ob_glyph(c.x, c.y, min(2.6, math.sqrt(q.area) * 0.6), a, rng, 0.3).intersection(inset(q, 0.4))
                if not g.is_empty: s.tcut([g], H + h - 0.3, H + h, "slot")
        rip = unary_union([LineString([f(-40, y), f(40, y)]).buffer(0.18) for y in np.arange(-30 * KK(), 30 * KK(), rng.uniform(1.4, 1.8))])
        patch = Point(*rp(rng, 2, 8)).buffer(rng.uniform(4.0, 6.0) * KK() ** 0.6).intersection(Point(0, 0).buffer(gg.R0 - 1.0))
        rip = rip.intersection(patch).difference(unary_union([q.buffer(0.6) for q, _ in got]) if got else Polygon())
        if not rip.is_empty: s.gcut([rip], 0.25, "rut")
        _drifts(s, rng, rng.randint(2, 3))
    grit(s, rng, 4, 10)
    return s


# ------------------------------------------------------------------------------------------------ tier features
def _tag(fn, cls, kind, shape):
    fn.tmpl, fn.kind, fn.shape, fn.cls = "single", kind, shape, cls
    return fn

def f_monolith(s, rng, lvl):
    """a black monolith on its two-step plinth, a band of glyphs ruled round the plinth's top"""
    hold = {}
    def mk():
        c = rp(rng, 2, 10); a = rng.uniform(0, math.pi); P = rng.uniform(3.0, 3.6); hold["a"] = a
        return _chamfer(rect(c[0], c[1], P, P, a), 0.25)
    g, z = find(s, mk, gap=0.4)
    if g is None: return
    a = hold["a"]; c = g.centroid; top = up(s, [g], z, 0.45, "monolith")
    if not top: return
    p2 = inset(g, 0.45); t2 = min(top + 0.35, CAPZ)
    if not s.prism([p2], t2, "monolith", z0=top - 0.15, foot=False): return
    topcut(s, [inset(g, 0.12).difference(inset(g, 0.3))], top, 0.15, "slot")
    sl = _chamfer(rect(c.x, c.y, rng.uniform(1.1, 1.4), rng.uniform(2.2, 2.7), a), 0.2)
    t3 = min(t2 + rng.uniform(1.6, 2.6), CAPZ)
    if s.prism([sl], t3, "monolith", z0=t2 - 0.15, foot=False):
        topcut(s, [inset(sl, 0.25).difference(inset(sl, 0.45))], t3, 0.15, "slot")

def f_pylon(s, rng, lvl):
    """a pylon half sunk in the sand: an eight-sided collar, the shaft rising from it, a slot across its head"""
    hold = {}
    def mk():
        c = rp(rng, 1, 10); r = rng.uniform(1.25, 1.55); hold["a"] = a = rng.uniform(0, math.pi); hold["r"] = r
        return _oct(c[0], c[1], r, a)
    g, z = find(s, mk, gap=0.4)
    if g is None: return
    a, r = hold["a"], hold["r"]; c = g.centroid; top = up(s, [g], z, rng.uniform(0.4, 0.55), "monolith")
    if not top: return
    sh = _oct(c.x, c.y, r * 0.68, a); t2 = min(top + rng.uniform(1.4, 2.4), CAPZ)
    if s.prism([sh], t2, "monolith", z0=top - 0.15, foot=False):
        topcut(s, [rect(c.x, c.y, 2 * r, 0.32, a)], t2, 0.25, "slot")
    topcut(s, [inset(g, 0.12).difference(inset(g, 0.3))], top, 0.15, "slot")

def f_casket(s, rng, lvl):
    """a sealed casket: a long block with chamfered corners, its lid a step smaller, a glyph line down the lid"""
    hold = {}
    def mk():
        c = rp(rng, 0, 9); a = rng.uniform(0, math.pi); hold["a"] = a; L = rng.uniform(4.2, 5.0); W = rng.uniform(1.8, 2.2); hold["LW"] = (L, W)
        return _chamfer(rect(c[0], c[1], L, W, a), 0.45)
    g, z = find(s, mk, gap=0.4)
    if g is None: return
    a = hold["a"]; L, W = hold["LW"]; c = g.centroid; top = up(s, [g], z, rng.uniform(0.8, 1.0), "casket")
    if not top: return
    lid = inset(g, 0.25); t2 = min(top + 0.25, CAPZ)
    if s.prism([lid], t2, "casket", z0=top - 0.15, foot=False):
        ff = frame(c.x, c.y, a); b = unary_union([inset(lid, 0.3).difference(inset(lid, 0.52))] + [rect(*ff(x, 0), 0.26, W * 0.4, a) for x in (-L * 0.25, 0.0, L * 0.25)])
        topcut(s, [b.intersection(lid)], t2, 0.18, "slot")

def f_panels(s, rng, lvl):
    """broken panels of smooth metal: angular plates lying flat, a seam ruled along each"""
    def at(cx, cy):
        out = []; a0 = rng.uniform(0, math.pi)
        for i in range(rng.randint(2, 3)):
            ff = frame(cx, cy, a0 + i * 1.9); x, y = ff(rng.uniform(-1.2, 1.2), rng.uniform(-1.2, 1.2)); a = a0 + i * 1.9 + rng.uniform(-0.3, 0.3)
            q = rect(x, y, rng.uniform(2.2, 3.0), rng.uniform(1.4, 1.9), a); cc = frame(x, y, a)
            cutl = Polygon([cc(rng.uniform(-0.3, 0.5), -2), cc(2, -2), cc(2, 2), cc(rng.uniform(0.2, 1.2), 2)])
            q = q.difference(cutl) if rng.random() < 0.7 else q
            if not q.is_empty and q.area > 1.2: out.append(max(polys_of(q), key=lambda p: p.area))
        return [p for p in out if all(p is o or p.distance(o) > 0.35 for o in out)]
    ps, z = place_group(s, rng, at, gap=0.4)
    if not ps: return
    top = up(s, ps, z, rng.uniform(0.3, 0.4), "obmetal")
    if top: topcut(s, [unary_union([inset(p, 0.3).difference(inset(p, 0.5)) for p in ps])], top, 0.12, "slot")

def f_glyphslab(s, rng, lvl):
    """a square slab of black stone lying loose, one great glyph cut into its face"""
    hold = {}
    def mk():
        c = rp(rng, 0, 10); a = rng.uniform(0, math.pi); P = rng.uniform(2.8, 3.4); hold["a"] = a; hold["P"] = P
        return _chamfer(rect(c[0], c[1], P, P, a), 0.2)
    g, z = find(s, mk, gap=0.4)
    if g is None: return
    c = g.centroid; top = up(s, [g], z, rng.uniform(0.4, 0.6), "monolith")
    if top:
        gl = ob_glyph(c.x, c.y, hold["P"] * 0.78, hold["a"], rng, 0.3).intersection(inset(g, 0.3))
        if not gl.is_empty: topcut(s, [gl], top, 0.2, "slot")

def f_ring(s, rng, lvl):
    """a fallen ring of smooth metal lying flat, notched at even steps"""
    hold = {}
    def mk():
        c = rp(rng, 1, 10); R_ = rng.uniform(1.8, 2.3); hold["R"] = R_; hold["a"] = rng.uniform(0, 1)
        return Point(c).buffer(R_, 32).difference(Point(c).buffer(R_ - rng.uniform(0.6, 0.75), 32))
    g, z = find(s, mk, gap=0.4)
    if g is None: return
    c = g.centroid; R_ = hold["R"]; top = up(s, [g], z, rng.uniform(0.4, 0.55), "obmetal")
    if top:
        n = rng.choice((6, 8)); nt = unary_union([rect(c.x + R_ * math.cos(hold["a"] + 6.2832 * k / n), c.y + R_ * math.sin(hold["a"] + 6.2832 * k / n), 0.6, 0.3, hold["a"] + 6.2832 * k / n) for k in range(n)])
        topcut(s, [nt.intersection(g)], top, 0.2, "slot")

def f_drift(s, rng, lvl):
    _drifts(s, rng, 1, big=(1.6, 2.4))


def tables(G):
    _take(G); _register()
    for nm in ("f_monolith", "f_pylon", "f_casket", "f_panels", "f_glyphslab", "f_ring"): _tag(globals()[nm], "made", "monolith" if nm != "f_panels" and nm != "f_ring" else "obmetal", nm[2:])
    _tag(f_drift, "debris", "sand", "blob")
    chips = scatter(sh_tri(0.6, 1.0), (3, 6), (0.4, 0.7), "obsidian", "mound", 1.6, ROCKY)
    sandp = single(sh_blob(1.8, 2.4), (0.5, 0.8), "sand", "mound", tiers=SOFTT)
    t2 = [f_glyphslab, f_panels, chips]
    t3 = [f_casket, f_pylon, sunk(sh_rect(1.8, 2.6, 1.0, 1.4), (0.3, 0.45), "slot")]
    t4 = [f_monolith, f_ring, sandp]
    det = t2 + t3 + t4 + [
        f_drift,
        scatter(sh_rect(1.0, 1.5, 0.8, 1.2), (2, 4), (0.5, 0.8), "monolith", "up", 1.8),
        single(sh_rect(2.4, 3.2, 1.6, 2.0), (0.4, 0.6), "obfloor", "up", pat_slots(3)),
        single(sh_hex(1.2, 1.6), (0.5, 0.7), "obmetal", "up", pat_ring()),
        sunk(sh_ring(1.3, 1.7, 0.4), (0.3, 0.4), "slot"),
        scatter(sh_blob(0.8, 1.2), (2, 4), (0.3, 0.5), "sand", "mound", 1.8, SOFTT),
        scatter(sh_round(0.4, 0.6), (5, 8), (0.3, 0.4), "obsidian", "mound", 1.4, ((0.6, 15), (0.4, 45))),
        single(sh_disc(1.2, 1.5, 2), (0.5, 0.7), "obmetal", "up", pat_hole(0.4, 0.6)),
        sunk(sh_wig(5, 8, 0.4, 0.6), (0.3, 0.4), "crack"),
        single(sh_rect(1.2, 1.5, 1.2, 1.5), (1.6, 2.2), "monolith", "up", pat_dots()),
        scatter(sh_caps(2.0, 3.0, 0.5), (2, 3), (0.4, 0.5), "obmetal", "up", 1.8),
        single(sh_rect(3.0, 3.8, 0.9, 1.2), (0.9, 1.2), "monolith", "up", pat_seams(1.0)),
        sunk(sh_rect(2.2, 2.8, 2.2, 2.8), (0.4, 0.6), "slot"),
        single(sh_ring(1.4, 1.8, 0.5), (0.4, 0.5), "obmetal", "up"),
        scatter(sh_hex(0.6, 0.85), (3, 5), (0.6, 0.9), "obglow", "up", 1.6),
        single(sh_rect(2.0, 2.6, 2.0, 2.6), (0.35, 0.45), "obfloor", "up", pat_ring(0.3)),
        sunk(sh_disc(0.4, 0.55, 2), (0.3, 0.4), "slot", (4, 7), 1.5),
        single(sh_blob(1.4, 2.0), (0.4, 0.6), "sand", "mound", tiers=SOFTT),
        scatter(sh_rect(0.9, 1.3, 0.5, 0.7), (3, 5), (0.35, 0.5), "obmetal", "up", 1.6),
        sunk(sh_rect(3.0, 4.0, 0.5, 0.6), (0.3, 0.4), "slot"),
        f_glyphslab, f_casket]
    return dict(tiers=(t2, t3, t4), details=det[:30])


# ------------------------------------------------------------------------------------------------ features and landmarks
def _b(fn, kind, tag):
    fn.kind, fn.tag = kind, tag
    return fn

def b_ob_slab(kind="monolith"):
    def f(cx, cy, a, rng):
        P = rng.uniform(2.6, 3.2); g = _chamfer(rect(cx, cy, P, P * rng.uniform(0.8, 1.0), a), 0.2)
        return g, [("up", g, rng.uniform(0.45, 0.6), kind), ("tcut", ob_glyph(cx, cy, P * 0.75, a, rng, 0.3), 0, 0.2)]
    return _b(f, kind, "ob_slab")

def b_ob_casket(kind="casket"):
    def f(cx, cy, a, rng):
        L = rng.uniform(4.2, 5.0); W = rng.uniform(1.8, 2.2); g = _chamfer(rect(cx, cy, L, W, a), 0.45); lid = inset(g, 0.25)
        crest = _chamfer(rect(cx, cy, L * 0.35, W * 0.35, a), 0.2)
        return g, [("up", g, rng.uniform(0.8, 1.0), kind), ("on", lid, 0, 0.25, kind), ("on", crest, 1, 0.2, kind),
                   ("tcut", inset(lid, 0.25).difference(inset(lid, 0.47)), 1, 0.15)]
    return _b(f, kind, "ob_casket")

def b_ob_pylon(kind="monolith"):
    def f(cx, cy, a, rng):
        r = rng.uniform(1.2, 1.5); g = _oct(cx, cy, r, a); sh = _oct(cx, cy, r * 0.66, a)
        return g, [("up", g, 0.5, kind), ("on", sh, 0, rng.uniform(1.4, 2.2), kind), ("tcut", rect(cx, cy, 2 * r, 0.32, a), 1, 0.25)]
    return _b(f, kind, "ob_pylon")

def b_ob_panel(kind="obmetal"):
    def f(cx, cy, a, rng):
        ff = frame(cx, cy, a); L = rng.uniform(3.0, 3.8); W = rng.uniform(1.8, 2.3)
        g = Polygon([ff(-L / 2, -W / 2), ff(L / 2, -W / 2), ff(L / 2 - rng.uniform(0.4, 1.2), W / 2), ff(-L / 2, W / 2)])
        sl = unary_union([rect(*ff(x, 0), 0.25, W * 0.5, a) for x in np.arange(-L / 2 + 0.7, L / 2 - 1.0, 0.5)])
        return g, [("up", g, 0.35, kind), ("tcut", sl, 0, 0.15)]
    return _b(f, kind, "ob_panel")

def L_obelisk(kind="monolith", floor="obfloor"):
    """a black monolith on a stepped plinth of three tiers, a band of glyphs on each tier's top"""
    def f(cx, cy, a, rng, k):
        P = rng.uniform(5.0, 5.8) * max(1.0, k ** 0.6); g1 = _chamfer(rect(cx, cy, P, P, a), 0.3); g2 = inset(g1, P * 0.13); g3 = inset(g2, P * 0.13)
        sl = _chamfer(rect(cx, cy, P * 0.22, P * 0.42, a), 0.15)
        lay = [("up", g1, 0.45, floor), ("on", g2, 0, 0.45, kind), ("on", g3, 1, 0.45, kind), ("on", sl, 2, 2.0, kind)]
        for i, g in ((0, g1), (1, g2)):
            lay.append(("tcut", inset(g, 0.12).difference(inset(g, 0.32)), i, 0.15))
        return g1.buffer(0.3), lay
    return f

def L_pylons(kind="monolith", floor="obmetal"):
    """a ring of square pylons round a round metal dais with a sunk centre"""
    def f(cx, cy, a, rng, k):
        R = rng.uniform(3.6, 4.2) * max(1.0, k ** 0.6); n = rng.choice((4, 6)); dais = Point(cx, cy).buffer(R * 0.55, 32)
        lay = [("up", dais, 0.4, floor), ("tcut", Point(cx, cy).buffer(R * 0.25, 24), 0, 0.25)]; foot = [dais]
        for i in range(n):
            t = a + 6.2832 * i / n; p = _chamfer(rect(cx + R * math.cos(t), cy + R * math.sin(t), 1.3, 1.3, t), 0.2); foot.append(p)
            j = len(lay); lay += [("up", p.buffer(0.3, join_style=2), 0.4, kind), ("on", p, j, rng.uniform(1.4, 2.4), kind)]
        return unary_union(foot).convex_hull, lay
    return f

def feature_tables(F):
    C, lm = F["comp"], F["lm"]
    feats = [C(b_ob_slab()), C(b_ob_casket()), C(b_ob_pylon()), C(b_ob_panel()), C(F["b_stairs"]("obfloor")), C(F["b_grate"]("obmetal")),
             C(F["b_chest"]("casket", "obmetal")), C(F["b_tiles"]("obfloor", 3, 2, 1.6)), C(F["b_cracked_slab"]("monolith")), C(F["b_crystals"]("obglow", 2.4))]
    marks = [(60, lm(L_obelisk())), (60, lm(L_pylons())), (60, lm(F["L_colonnade"]("monolith", "obfloor"))), (100, lm(F["L_ruin"]("monolith", "obfloor"))),
             (100, lm(F["L_dais"]("obfloor", "column"))), (130, lm(F["L_wall"]("monolith", "obmetal")))]
    return dict(features=feats, landmarks=marks)


def ground_big(E, B, P, scree, pebbles):
    return dict(field=("sand", 0.3, 1.4, 0.5),
                feats=[("slabs", 3, dict(kind="obfloor", h=(0.3, 1.1))), ("slabs", 2, dict(kind="monolith", h=(0.6, 1.5), tilt=False)),
                       ("ridge", 2, dict(kind="sand", h=(0.5, 1.4), w=(2.6, 4.2)))])          # no mesa, outcrop or rubble: their ragged rock is not this world
