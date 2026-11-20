"""Extra detail library: about 30 detail types per theme, drawn at random onto each base (more of them at higher tiers).

Each detail is built from a few templates:
  scatter  - a cluster of small shapes (rocks, bones, casings...) raised as one-solid chamfered pieces
  single   - one larger piece (crate, slab, barrel...) with an optional pattern cut into its top
  sunk     - something cut into the ground (puddle, footprints, runes, fissure...)
  line     - a long thin raised or cut path (cable, root, pipe, rut...)
gg3.make() calls apply(); helper functions (place_group, find, up, mound, cut...) come from gg3 via install()."""
import math

def install(G):
    _c = {"bone": "#e6dcc4", "glass": "#a9cfd6", "column": "#c9bfa8", "tendril": "#7a3b4a", "spine": "#d8c9a8", "egg": "#c7b37a", "pavers": "#b8ab94", "ruin": "#a89e8c", "obelisk": "#6e6a66", "slate": "#6f7377", "crate": "#a8743f", "post": "#6b6e72", "metal": "#7d838c", "concrete": "#a3a09a", "brick": "#a5553a", "rubble": "#8f8a82", "wood": "#8a5a32", "iron": "#4d4f55", "quartz": "#e8e4dc", "moss": "#5d7a3a", "heather": "#8a5a8a", "fern": "#52803c", "standing": "#80858b", "shrub": "#8c8248", "crystal": "#b9dcec", "salt": "#ece6d6", "pustule": "#c9a35a", "flesh": "#9c4a4a", "carapace": "#4a3a2a", "sandstone": "#c9935a", "cable": "#3c3f45", "pipe": "#6f747c", "paint": "#e8e4d8", "plate": "#5b616b"}
    for k, v in _c.items(): G["gg"].COL.setdefault(k, v)
    globals().update({k: v for k, v in G.items() if not k.startswith("__") and k not in ("apply", "install", "DETAILS")})


# ------------------------------------------------------------------------------------------------ shapes (x, y, rng)
def sh_rock(r0, r1, nv=(5, 8)):
    f = lambda x, y, rng: stone(x, y, rng.uniform(r0, r1), rng, None, rng.randint(*nv))
    f.cspec = (0, r0, r1, nv[0], nv[1], 0, 0.0); return f          # cspec: what cstone.cluster needs to make the same rocks in C
def sh_round(r0, r1):
    f = lambda x, y, rng: stone(x, y, rng.uniform(r0, r1), rng, None, None, round_=True)
    f.cspec = (1, r0, r1, 0, 0, 0, 0.0); return f
def _one(shape, rng):
    """make_one for gg3.cluster: shape(x, y, rng), carrying the shape's cspec so the cluster can be made in C"""
    f = lambda x, y: shape(x, y, rng); c = getattr(shape, "cspec", None)
    if c is not None: f.cspec = c
    return f
def sh_disc(r0, r1, res=20):
    return lambda x, y, rng: Point(x, y).buffer(rng.uniform(r0, r1), res)
def sh_ell(a0, a1, b0, b1):
    return lambda x, y, rng: affinity.rotate(affinity.scale(Point(x, y).buffer(1, 20), rng.uniform(a0, a1), rng.uniform(b0, b1)), rng.uniform(0, 180))
def sh_rect(l0, l1, w0, w1):
    return lambda x, y, rng: rect(x, y, rng.uniform(l0, l1), rng.uniform(w0, w1), aim(rng, x, y, math.pi))
def sh_caps(l0, l1, w):
    def f(x, y, rng):
        a = aim(rng, x, y, math.pi); L = rng.uniform(l0, l1) / 2
        return LineString([(x - L * math.cos(a), y - L * math.sin(a)), (x + L * math.cos(a), y + L * math.sin(a))]).buffer(w / 2, 8)
    return f
def sh_bone(l0, l1):
    def f(x, y, rng):
        a = rng.uniform(0, math.pi); L = rng.uniform(l0, l1) / 2; c, s_ = math.cos(a), math.sin(a)
        p = LineString([(x - L * c, y - L * s_), (x + L * c, y + L * s_)]).buffer(0.32, 8)
        knobs = [Point(x + e * L * c + k * 0.32 * -s_, y + e * L * s_ + k * 0.32 * c).buffer(0.36, 10) for e in (-1, 1) for k in (-1, 1)]
        return unary_union([p] + knobs)
    return f
def sh_star(r0, r1, n0, n1):
    return lambda x, y, rng: (lambda ro: star(x, y, ro, ro * rng.uniform(.45, .6), rng.randint(n0, n1), rng.uniform(0, 1)))(rng.uniform(r0, r1))
def sh_crescent(r0, r1):
    def f(x, y, rng):
        r = rng.uniform(r0, r1); a = rng.uniform(0, 2 * math.pi)
        g = Point(x, y).buffer(r, 24).difference(Point(x + r * .55 * math.cos(a), y + r * .55 * math.sin(a)).buffer(r * .85, 24))
        return max(polys_of(g), key=lambda q: q.area) if not g.is_empty else Point(x, y).buffer(r * .5)
    return f
def sh_hex(r0, r1):
    return lambda x, y, rng: (lambda r, a: Polygon([(x + r * math.cos(a + k * math.pi / 3), y + r * math.sin(a + k * math.pi / 3)) for k in range(6)]))(rng.uniform(r0, r1), rng.uniform(0, 1))
def sh_tri(r0, r1):
    return lambda x, y, rng: stone(x, y, rng.uniform(r0, r1), rng, rng.uniform(.5, .8), 3)
def sh_cross(l0, l1, w):
    def f(x, y, rng):
        L = rng.uniform(l0, l1); a = rng.uniform(0, math.pi)
        return unary_union([rect(x, y, L, w, a), rect(x, y, w, L, a)])
    return f
def sh_ring(ro0, ro1, t):
    return lambda x, y, rng: (lambda ro: Point(x, y).buffer(ro, 24).difference(Point(x, y).buffer(ro - t, 24)))(rng.uniform(ro0, ro1))
def sh_blob(r0, r1):
    return lambda x, y, rng: blob(x, y, rng.uniform(r0, r1), rng, rng.randint(3, 5))
def sh_wig(l0, l1, w, amp=0.5):
    def f(x, y, rng):
        a = rng.uniform(0, math.pi); L = rng.uniform(l0, l1) / 2
        p0 = (x - L * math.cos(a), y - L * math.sin(a)); p1 = (x + L * math.cos(a), y + L * math.sin(a))
        return LineString(wiggle(p0, p1, rng, amp, max(3, int(L)))).buffer(w / 2, cap_style=1, resolution=4)
    return f


def _tag_shapes():
    for nm, f in list(globals().items()):
        if nm.startswith("sh_") and callable(f) and not getattr(f, "_w", False):
            def w(*a, _f=f, _n=nm[3:], **k):
                g = _f(*a, **k); g.tag = _n; return g
            w._w = True; globals()[nm] = w
_tag_shapes()


# ------------------------------------------------------------------------------------------------ templates
def _raise(s, ps, z, h, kind, how, tiers):
    if how == "up" and kind in ("bone", "root", "rope", "vein", "tendril", "cable", "twig", "tooth", "chain") and h < 1.6: how, tiers = "mound", ((0.45, 12), (0.3, 38), (0.25, 62))      # a round back, not a flat top
    if how == "mound": mound(s, ps, z, h, tiers, kind); return None
    return up(s, ps, z, h, kind)

def scatter(shape, n=(3, 6), h=(0.5, 0.9), kind="rock", how="mound", spread=1.6, tiers=((0.7, 6), (0.3, 40))):
    def fn(s, rng, lvl):
        k = rng.randint(*n)
        ps, z = place_group(s, rng, lambda cx, cy: cluster(s, rng, k, _one(shape, rng), cx, cy, spread, 0.3), gap=0.35)
        if ps: _raise(s, [p for p in ps if wide_enough(p, 0.7)], z, rng.uniform(*h), kind, how, tiers)
    fn.tmpl, fn.kind, fn.shape = "scatter", kind, getattr(shape, "tag", "") if shape is not None else ""
    return fn

def single(shape, h=(0.8, 1.4), kind="crate", how="up", top=None, tiers=((0.75, 5), (0.25, 40))):
    """one piece; top(cx, cy, a, g, rng) -> polygons cut 0.3 mm into its top"""
    def fn(s, rng, lvl):
        g, z = find(s, lambda cx, cy: shape(cx, cy, rng), gap=0.4, at=lambda: rp(rng, 0, 10.5))
        if g is None: return
        tz = _raise(s, [g], z, rng.uniform(*h), kind, how, tiers)
        if top and tz:
            c = g.centroid
            pat = top(c.x, c.y, g, rng)
            if pat is not None and not pat.is_empty:
                q = pat.intersection(inset(g, 0.35))
                if not q.is_empty: topcut(s, [q], tz, 0.3, "slot")
    fn.tmpl, fn.kind, fn.shape = "single", kind, getattr(shape, "tag", "") if shape is not None else ""
    return fn

def sunk(shape, depth=(0.3, 0.6), kind="pit", n=(1, 1), spread=0.0):
    def fn(s, rng, lvl):
        k = rng.randint(*n)
        if k == 1 and spread == 0:
            g, z = find(s, lambda cx, cy: shape(cx, cy, rng), gap=0.3, at=lambda: rp(rng, 0, 10.5))
            if g is not None: cut(s, [g], z, rng.uniform(*depth), kind)
            return
        ps, z = place_group(s, rng, lambda cx, cy: cluster(s, rng, k, _one(shape, rng), cx, cy, spread, 0.3), gap=0.3)
        if ps: cut(s, ps, z, rng.uniform(*depth), kind)
    fn.tmpl, fn.kind, fn.shape = "sunk", kind, getattr(shape, "tag", "") if shape is not None else ""
    return fn

def trail(step=1.3, n=(5, 8), shape=None, depth=0.35, kind="pit"):
    """a line of marks (footprints, hoof prints, drag marks, bullet holes)"""
    shape = shape or sh_ell(0.35, 0.45, 0.6, 0.75)
    def fn(s, rng, lvl):
        def at(cx, cy):
            a = aim(rng, cx, cy); k = rng.randint(*n); f = frame(cx, cy, a)
            return [affinity.rotate(shape(*f((i - k / 2) * step, 0.45 * (1 if i % 2 else -1)), rng), math.degrees(a), origin=f((i - k / 2) * step, 0)) for i in range(k)]
        ps, z = place_group(s, rng, at, gap=0.3)
        if ps: cut(s, ps, z, depth, kind)
    fn.tmpl, fn.kind, fn.shape = "trail", kind, getattr(shape, "tag", "") if shape is not None else ""
    return fn

def path(w=0.7, l=(6, 10), h=0.4, kind="cable", how="up", amp=0.6):
    def fn(s, rng, lvl):
        g, z = find(s, lambda cx, cy: sh_wig(l[0], l[1], w, amp)(cx, cy, rng), gap=0.35, at=lambda: rp(rng, 0, 8))
        if g is None: return
        if how == "cut": cut(s, [g], z, h, kind)
        else: up(s, [g], z, h, kind)
    fn.tmpl, fn.kind, fn.shape = "path", kind, getattr(None, "tag", "") if None is not None else ""
    return fn


# ------------------------------------------------------------------------------------------------ top patterns
def pat_slots(n=4, w=0.35):
    def f(cx, cy, g, rng):
        mr = g.minimum_rotated_rectangle.exterior.coords; a = math.atan2(mr[1][1] - mr[0][1], mr[1][0] - mr[0][0]); ff = frame(cx, cy, a)
        return unary_union([rect(*ff(0, (i - (n - 1) / 2) * 0.8), 6, w, a) for i in range(n)])
    return f
def pat_seams(step=0.9):
    def f(cx, cy, g, rng):
        mr = g.minimum_rotated_rectangle.exterior.coords; a = math.atan2(mr[1][1] - mr[0][1], mr[1][0] - mr[0][0]); ff = frame(cx, cy, a)
        return unary_union([rect(*ff(u, 0), 0.3, 8, a) for u in np.arange(-3, 3.1, step)])
    return f
def pat_ring(t=0.3):
    def f(cx, cy, g, rng):
        return inset(g, 0.35).difference(inset(g, 0.35 + t))
    return f
def pat_cross():
    return lambda cx, cy, g, rng: unary_union([rect(cx, cy, 8, 0.35, 0.3), rect(cx, cy, 0.35, 8, 0.3)])
def pat_dots(r=0.25):
    def f(cx, cy, g, rng):
        return unary_union([Point(cx + dx, cy + dy).buffer(r, 10) for dx in (-0.8, 0.8) for dy in (-0.8, 0.8)])
    return f
def pat_hole(r0=0.5, r1=0.8):
    return lambda cx, cy, g, rng: Point(cx, cy).buffer(rng.uniform(r0, r1), 20)
def pat_rune():
    def f(cx, cy, g, rng):
        a = rng.uniform(0, math.pi); ff = frame(cx, cy, a)
        segs = [((-0.9, -0.9), (0.9, 0.9)), ((-0.9, 0.9), (0, 0)), ((0, -1.0), (0, 1.0)), ((-0.9, 0), (0.9, 0))]
        return unary_union([line_cut(ff(*p), ff(*q), 0.3) for p, q in rng.sample(segs, rng.randint(2, 3))])
    return f


# ------------------------------------------------------------------------------------------------ the 30 per theme
ROCKY = ((0.7, 6), (0.3, 40))
SOFTT = ((0.55, 25), (0.45, 50))
DETAILS = {
"badlands": [
    scatter(sh_rock(0.9, 1.4), (3, 5), (0.6, 1.0), "rock"), scatter(sh_rock(0.5, 0.8), (6, 10), (0.35, 0.5), "pebble"),
    single(sh_rock(2.2, 3.0), (1.2, 1.9), "sandstone", "up", pat_seams(1.1)), single(sh_rock(1.6, 2.2), (1.0, 1.5), "rock", "mound", tiers=ROCKY),
    scatter(sh_bone(1.6, 2.4), (2, 4), (0.4, 0.5), "bone", "up"), single(sh_ell(1.1, 1.4, 0.7, 0.9), (0.6, 0.8), "bone", "mound", pat_dots(0.22)),
    scatter(sh_star(0.9, 1.3, 5, 7), (2, 3), (0.8, 1.1), "shrub", "mound", 2.0, SOFTT), single(sh_star(1.6, 2.2, 6, 8), (1.0, 1.4), "shrub", "mound", tiers=SOFTT),
    scatter(sh_caps(2.0, 3.2, 0.5), (2, 3), (0.5, 0.6), "wood", "up"), single(sh_rect(2.8, 3.6, 1.6, 2.0), (1.0, 1.3), "crate", "up", pat_seams(0.9)),
    single(sh_disc(0.9, 1.1, 24), (1.4, 1.8), "wood", "up", pat_ring(0.25)), sunk(sh_blob(1.2, 1.8), (0.3, 0.4), "water"),
    trail(1.4, (5, 8)), trail(1.1, (6, 9), sh_disc(0.28, 0.35, 10), 0.3, "pit"), path(0.6, (6, 10), 0.3, "rut", "cut", 0.8),
    sunk(sh_wig(5, 8, 0.45, 0.9), (0.4, 0.5), "crack"), sunk(sh_crescent(1.0, 1.4), (0.3, 0.4), "pit"),
    scatter(sh_tri(0.7, 1.1), (4, 7), (0.4, 0.7), "rock", "mound", 1.8, ROCKY), single(sh_hex(1.2, 1.6), (0.9, 1.3), "sandstone", "up", pat_cross()),
    scatter(sh_disc(0.4, 0.55, 12), (5, 9), (0.3, 0.4), "pebble", "mound", 1.4, ((0.7, 10), (0.3, 45))),
    single(sh_caps(4.0, 6.0, 0.9), (0.5, 0.7), "wood", "up", pat_seams(1.2)), sunk(sh_rect(1.6, 2.4, 1.0, 1.4), (0.5, 0.7), "pit"),
    scatter(sh_cross(1.3, 1.8, 0.4), (1, 2), (0.9, 1.2), "wood", "up"), single(sh_ring(1.2, 1.6, 0.6), (0.6, 0.8), "rock", "up"),
    path(0.8, (6, 9), 0.35, "rock", "up", 0.4), scatter(sh_ell(0.5, 0.7, 0.9, 1.2), (3, 5), (0.4, 0.6), "bone", "mound", 1.5, ROCKY),
    single(sh_rock(1.4, 1.8, (6, 8)), (0.9, 1.2), "rock", "up", pat_hole()), sunk(sh_disc(1.4, 1.9, 24), (0.4, 0.6), "pit"),
    scatter(sh_crescent(0.7, 1.0), (2, 4), (0.4, 0.5), "rock", "up"), sunk(sh_star(1.2, 1.6, 5, 6), (0.3, 0.4), "crack"),
],
"highland": [
    scatter(sh_rock(0.9, 1.4), (3, 5), (0.6, 1.0), "slate"), single(sh_rock(2.0, 2.8), (1.4, 2.0), "slate", "up"),
    scatter(sh_star(1.0, 1.5, 6, 8), (2, 4), (0.9, 1.3), "grass", "mound", 2.0, SOFTT), scatter(sh_disc(0.45, 0.6, 12), (4, 7), (0.45, 0.6), "flower", "mound", 1.2, ((1.0, 30),)),
    single(sh_rect(4.0, 6.0, 0.9, 1.2), (1.0, 1.4), "slate", "up", pat_seams(1.0)), scatter(sh_rect(1.2, 1.8, 0.8, 1.1), (3, 5), (0.7, 1.0), "slate", "up", 1.8),
    single(sh_disc(1.2, 1.6, 24), (1.8, 2.4), "slate", "up", pat_ring()), sunk(sh_blob(1.4, 2.0), (0.3, 0.5), "water"),
    trail(1.3, (5, 8), sh_crescent(0.35, 0.45), 0.3, "pit"), path(0.7, (6, 10), 0.35, "rut", "cut", 0.7),
    scatter(sh_ell(0.6, 0.9, 0.4, 0.6), (4, 7), (0.5, 0.7), "fern", "mound", 1.6, SOFTT), single(sh_star(1.6, 2.2, 7, 9), (1.2, 1.6), "shrub", "mound", tiers=SOFTT),
    scatter(sh_caps(2.4, 3.4, 0.55), (1, 3), (0.5, 0.6), "wood", "up"), single(sh_rect(2.4, 3.0, 1.4, 1.8), (1.0, 1.3), "crate", "up", pat_seams(0.8)),
    single(sh_rock(1.4, 1.8), (2.0, 2.6), "standing", "up", pat_rune()), single(sh_hex(1.1, 1.4), (0.8, 1.0), "slate", "up", pat_rune()),
    scatter(sh_round(0.55, 0.8), (5, 9), (0.35, 0.5), "pebble", "mound", 1.6, ((0.7, 10), (0.3, 45))), sunk(sh_wig(5, 9, 0.5, 0.8), (0.4, 0.5), "crack"),
    scatter(sh_tri(0.8, 1.2), (3, 5), (0.5, 0.8), "slate", "mound", 1.8, ROCKY), single(sh_ring(1.6, 2.0, 0.7), (0.7, 0.9), "slate", "up"),
    scatter(sh_bone(1.4, 2.0), (1, 3), (0.4, 0.5), "bone", "up"), single(sh_disc(0.8, 1.0, 20), (1.4, 1.8), "wood", "up", pat_ring(0.25)),
    sunk(sh_rect(2.0, 3.0, 1.2, 1.6), (0.4, 0.6), "pit"), scatter(sh_blob(0.7, 1.0), (2, 4), (0.5, 0.7), "moss", "mound", 1.8, SOFTT),
    path(0.8, (5, 8), 0.4, "slate", "up", 0.3), scatter(sh_star(0.6, 0.8, 5, 6), (4, 7), (0.5, 0.7), "heather", "mound", 1.6, SOFTT),
    single(sh_rect(3.0, 4.0, 2.0, 2.6), (0.5, 0.7), "slate", "up", pat_cross()), sunk(sh_disc(0.4, 0.55, 10), (0.3, 0.4), "pit", (4, 7), 1.5),
    scatter(sh_crescent(0.8, 1.1), (2, 3), (0.5, 0.7), "slate", "up"), single(sh_ell(1.4, 1.8, 0.8, 1.0), (0.9, 1.2), "moss", "mound", tiers=SOFTT),
],
"maahl": [
    scatter(sh_hex(0.7, 1.0), (3, 6), (0.8, 1.4), "crystal", "up", 1.6), single(sh_hex(1.4, 1.8), (2.0, 2.6), "crystal", "up", pat_rune()),
    scatter(sh_tri(0.6, 0.9), (4, 7), (0.8, 1.2), "crystal", "mound", 1.6, ((0.6, 4), (0.4, 35))), single(sh_disc(1.6, 2.0, 24), (0.5, 0.7), "salt", "up", pat_rune()),
    scatter(sh_rock(0.9, 1.4), (3, 5), (0.6, 0.9), "rock"), single(sh_rect(3.6, 4.6, 1.2, 1.6), (0.8, 1.1), "ruin", "up", pat_rune()),
    single(sh_disc(1.0, 1.3, 24), (2.0, 2.8), "column", "up", pat_ring(0.25)), scatter(sh_rect(1.6, 2.2, 1.0, 1.3), (2, 4), (0.6, 0.9), "ruin", "up", 2.0),
    sunk(sh_blob(1.4, 2.0), (0.4, 0.6), "water"), sunk(sh_ring(1.6, 2.2, 0.45), (0.4, 0.5), "slot"), sunk(sh_star(1.4, 1.9, 5, 7), (0.3, 0.4), "crack"),
    single(sh_star(1.4, 1.8, 4, 6), (1.6, 2.2), "crystal", "up"), scatter(sh_star(0.8, 1.1, 6, 8), (2, 4), (0.8, 1.1), "shrub", "mound", 1.8, SOFTT),
    trail(1.3, (5, 7), sh_tri(0.4, 0.5), 0.3, "pit"), path(0.6, (6, 10), 0.3, "glyph", "cut", 0.3),
    scatter(sh_disc(0.4, 0.6, 12), (5, 8), (0.4, 0.6), "salt", "mound", 1.4, ((0.7, 10), (0.3, 45))), single(sh_cross(2.4, 3.2, 0.8), (0.6, 0.8), "ruin", "up"),
    single(sh_ring(1.4, 1.8, 0.6), (0.8, 1.0), "ruin", "up"), scatter(sh_crescent(0.8, 1.2), (2, 3), (0.6, 0.8), "crystal", "up"),
    single(sh_rect(2.6, 3.2, 2.6, 3.2), (0.4, 0.5), "pavers", "up", pat_cross()), sunk(sh_hex(1.1, 1.5), (0.4, 0.6), "pit"),
    scatter(sh_bone(1.6, 2.2), (1, 3), (0.4, 0.5), "bone", "up"), single(sh_ell(1.5, 1.9, 0.9, 1.1), (1.2, 1.6), "rock", "mound", tiers=ROCKY),
    sunk(sh_disc(0.35, 0.5, 10), (0.3, 0.4), "pit", (5, 8), 1.6), single(sh_tri(1.6, 2.0), (1.4, 1.8), "obelisk", "up", pat_rune()),
    path(0.8, (5, 8), 0.35, "crystal", "up", 0.3), scatter(sh_ell(0.6, 0.8, 0.9, 1.2), (3, 5), (0.5, 0.7), "rock", "mound", 1.6, ROCKY),
    single(sh_hex(1.0, 1.3), (0.6, 0.8), "pavers", "up", pat_dots()), sunk(sh_wig(5, 8, 0.4, 0.6), (0.4, 0.5), "crack"),
    single(sh_disc(1.4, 1.8, 24), (0.5, 0.7), "ruin", "up", pat_rune()),
],
"corridor": [
    single(sh_rect(2.4, 3.2, 1.6, 2.0), (1.0, 1.4), "crate", "up", pat_seams(0.8)), scatter(sh_rect(1.2, 1.6, 1.0, 1.3), (2, 3), (0.8, 1.1), "crate", "up", 1.8),
    single(sh_disc(1.0, 1.3, 24), (1.6, 2.0), "metal", "up", pat_ring(0.25)), scatter(sh_disc(0.6, 0.8, 20), (2, 4), (1.0, 1.4), "metal", "up", 1.6),
    single(sh_rect(3.0, 4.0, 1.2, 1.6), (0.5, 0.7), "metal", "up", pat_slots(3)), single(sh_rect(2.6, 3.2, 2.6, 3.2), (0.3, 0.4), "plate", "up", pat_dots()),
    path(0.6, (7, 11), 0.4, "cable", "up", 0.9), path(0.9, (6, 10), 0.5, "pipe", "up", 0.15), scatter(sh_caps(0.6, 0.9, 0.3), (5, 9), (0.3, 0.35), "metal", "up", 1.4),
    sunk(sh_rect(2.0, 2.8, 1.2, 1.6), (0.4, 0.6), "slot"), sunk(sh_disc(1.0, 1.4, 24), (0.4, 0.5), "slot"), sunk(sh_ring(1.2, 1.5, 0.35), (0.3, 0.4), "slot"),
    single(sh_hex(1.0, 1.3), (0.8, 1.1), "metal", "up", pat_hole(0.35, 0.5)), scatter(sh_hex(0.45, 0.6), (3, 6), (0.4, 0.5), "metal", "up", 1.4),
    single(sh_rect(1.8, 2.4, 1.8, 2.4), (1.2, 1.6), "metal", "up", pat_cross()), sunk(sh_blob(1.2, 1.7), (0.25, 0.35), "water"),
    trail(1.0, (5, 8), sh_disc(0.25, 0.32, 10), 0.4, "pit"), scatter(sh_tri(0.6, 0.9), (3, 6), (0.3, 0.4), "metal", "up", 1.6),
    single(sh_caps(4.0, 6.0, 1.0), (0.7, 0.9), "pipe", "up", pat_seams(1.4)), single(sh_ring(1.2, 1.5, 0.45), (0.5, 0.7), "metal", "up"),
    scatter(sh_rect(0.9, 1.2, 0.6, 0.8), (3, 5), (0.5, 0.7), "metal", "up", 1.5), single(sh_cross(2.6, 3.2, 0.7), (0.4, 0.5), "metal", "up"),
    sunk(sh_rect(3.0, 4.0, 0.5, 0.7), (0.4, 0.5), "slot", (2, 3), 1.2), single(sh_disc(0.7, 0.9, 20), (2.0, 2.6), "metal", "up", pat_ring(0.2)),
    scatter(sh_bone(1.2, 1.6), (1, 2), (0.35, 0.45), "bone", "up"), single(sh_rect(2.0, 2.6, 1.0, 1.3), (0.8, 1.0), "metal", "up", pat_slots(2, 0.3)),
    path(0.5, (5, 9), 0.3, "seam", "cut", 0.1), scatter(sh_caps(1.6, 2.4, 0.45), (2, 4), (0.4, 0.5), "pipe", "up", 1.8),
    sunk(sh_hex(0.9, 1.2), (0.4, 0.6), "slot"), single(sh_rect(1.4, 1.8, 1.4, 1.8), (1.6, 2.0), "crate", "up", pat_cross()),
],
"infestation": [
    scatter(sh_disc(0.6, 0.9, 20), (3, 6), (0.7, 1.0), "pustule", "mound", 1.6, SOFTT), single(sh_ell(1.4, 1.9, 0.9, 1.2), (1.0, 1.4), "egg", "mound", tiers=SOFTT),
    scatter(sh_ell(0.6, 0.8, 0.9, 1.2), (3, 6), (0.7, 1.0), "egg", "mound", 1.6, SOFTT), path(0.7, (6, 10), 0.6, "tendril", "up", 1.0),
    path(0.5, (7, 11), 0.5, "tendril", "up", 1.2), scatter(sh_star(0.9, 1.3, 5, 7), (2, 3), (0.8, 1.1), "flesh", "mound", 1.8, SOFTT),
    single(sh_blob(1.8, 2.4), (1.0, 1.4), "flesh", "mound", tiers=SOFTT), sunk(sh_blob(1.4, 2.0), (0.4, 0.6), "water"),
    sunk(sh_disc(0.4, 0.6, 14), (0.4, 0.5), "pit", (4, 7), 1.6), sunk(sh_ring(1.4, 1.8, 0.45), (0.4, 0.5), "pit"),
    scatter(sh_bone(1.6, 2.2), (2, 4), (0.4, 0.5), "bone", "up"), single(sh_ell(1.2, 1.5, 0.8, 1.0), (0.7, 0.9), "bone", "mound", pat_dots(0.22)),
    scatter(sh_crescent(0.8, 1.1), (2, 4), (0.5, 0.7), "carapace", "up"), single(sh_crescent(1.6, 2.2), (0.8, 1.0), "carapace", "up", pat_seams(0.7)),
    scatter(sh_tri(0.6, 0.9), (4, 6), (0.8, 1.2), "spine", "mound", 1.6, ((0.6, 4), (0.4, 35))), single(sh_star(1.4, 1.8, 5, 6), (1.2, 1.6), "spine", "mound", tiers=((0.6, 4), (0.4, 35))),
    trail(1.2, (5, 8), sh_tri(0.35, 0.45), 0.35, "pit"), sunk(sh_wig(5, 9, 0.5, 1.0), (0.4, 0.5), "crack"),
    scatter(sh_round(0.5, 0.7), (6, 10), (0.4, 0.5), "pustule", "mound", 1.4, ((0.6, 20), (0.4, 50))), single(sh_disc(1.4, 1.8, 24), (0.5, 0.7), "flesh", "up", pat_hole(0.6, 0.9)),
    scatter(sh_caps(1.6, 2.4, 0.5), (3, 5), (0.5, 0.7), "tendril", "up", 1.6), single(sh_ring(1.4, 1.8, 0.7), (0.8, 1.0), "flesh", "up"),
    scatter(sh_blob(0.7, 1.0), (2, 4), (0.6, 0.8), "flesh", "mound", 1.8, SOFTT), sunk(sh_crescent(1.2, 1.6), (0.3, 0.4), "pit"),
    scatter(sh_ell(0.4, 0.5, 0.6, 0.8), (6, 10), (0.4, 0.5), "egg", "mound", 1.2, SOFTT), single(sh_hex(1.2, 1.6), (0.6, 0.8), "carapace", "up", pat_cross()),
    path(0.9, (5, 8), 0.45, "flesh", "up", 0.5), sunk(sh_star(1.3, 1.7, 5, 7), (0.3, 0.4), "crack"),
    scatter(sh_rock(0.8, 1.2), (3, 5), (0.5, 0.8), "rock"), single(sh_rock(1.8, 2.4), (1.2, 1.6), "rock", "mound", tiers=ROCKY),
],
"urban": [
    scatter(sh_rect(1.6, 2.0, 0.8, 1.0), (4, 8), (0.45, 0.7), "brick", "up", 1.8), single(sh_rect(3.0, 4.0, 1.8, 2.4), (0.6, 0.9), "concrete", "up", pat_seams(1.1)),
    scatter(sh_rock(0.8, 1.3), (3, 5), (0.5, 0.8), "rubble"), single(sh_rock(1.8, 2.4), (1.0, 1.4), "rubble", "mound", tiers=ROCKY),
    scatter(sh_caps(0.7, 1.0, 0.32), (6, 10), (0.3, 0.35), "metal", "up", 1.4), scatter(sh_caps(2.4, 3.6, 0.4), (2, 3), (0.5, 0.6), "iron", "up", 1.6),
    single(sh_disc(0.9, 1.2, 24), (1.6, 2.0), "metal", "up", pat_ring(0.25)), single(sh_rect(2.8, 3.4, 1.8, 2.2), (1.0, 1.3), "crate", "up", pat_seams(0.8)),
    single(sh_rect(3.2, 4.0, 1.4, 1.8), (1.4, 1.8), "concrete", "up", pat_slots(2, 0.4)), sunk(sh_blob(1.4, 2.0), (0.3, 0.45), "water"),
    sunk(sh_disc(0.3, 0.45, 10), (0.4, 0.5), "pothole", (5, 8), 1.5), sunk(sh_blob(1.6, 2.2), (0.5, 0.7), "pothole"),
    trail(1.2, (5, 8)), path(0.5, (7, 11), 0.4, "cable", "up", 0.9), path(0.45, (6, 10), 0.4, "crack", "cut", 0.9),
    single(sh_rect(1.0, 1.3, 1.0, 1.3), (2.0, 2.6), "post", "up", pat_cross()), scatter(sh_disc(0.6, 0.75, 20), (2, 3), (1.6, 2.0), "post", "up", 2.0),
    single(sh_cross(2.6, 3.4, 0.7), (0.5, 0.7), "iron", "up"), scatter(sh_tri(0.6, 1.0), (4, 7), (0.3, 0.45), "glass", "up", 1.6),
    single(sh_ring(1.2, 1.5, 0.45), (0.6, 0.8), "metal", "up"), sunk(sh_rect(2.6, 3.4, 1.6, 2.0), (0.4, 0.6), "slot"),
    single(sh_rect(2.4, 3.0, 0.9, 1.1), (0.6, 0.8), "wood", "up", pat_seams(0.7)), scatter(sh_rect(1.0, 1.4, 0.5, 0.7), (3, 6), (0.4, 0.5), "wood", "up", 1.6),
    sunk(sh_star(1.3, 1.8, 5, 7), (0.3, 0.4), "scorch"), single(sh_disc(1.3, 1.6, 24), (0.4, 0.5), "metal", "up", pat_cross()),
    scatter(sh_hex(0.5, 0.7), (3, 5), (0.5, 0.6), "concrete", "up", 1.4), single(sh_rect(1.6, 2.0, 1.6, 2.0), (0.3, 0.4), "paint", "up"),
    sunk(sh_ring(1.0, 1.3, 0.3), (0.3, 0.4), "slot"), scatter(sh_round(0.5, 0.7), (6, 10), (0.35, 0.45), "rubble", "mound", 1.6, ((0.7, 10), (0.3, 45))),
    single(sh_rect(2.2, 2.8, 1.2, 1.5), (1.2, 1.5), "metal", "up", pat_slots(3, 0.3)),
],
"mine": [
    single(sh_rect(2.4, 3.2, 1.6, 2.0), (1.0, 1.4), "crate", "up", pat_seams(0.8)), scatter(sh_rect(1.2, 1.6, 1.0, 1.3), (2, 3), (0.8, 1.1), "crate", "up", 1.8),
    single(sh_disc(0.9, 1.2, 24), (1.4, 1.8), "wood", "up", pat_ring(0.25)), scatter(sh_disc(0.6, 0.8, 20), (2, 3), (1.2, 1.5), "wood", "up", 1.6),
    scatter(sh_caps(2.6, 3.6, 0.6), (2, 3), (0.5, 0.7), "wood", "up", 1.8), single(sh_caps(5.0, 7.0, 1.0), (0.6, 0.8), "wood", "up", pat_seams(1.3)),
    scatter(sh_rock(0.9, 1.4), (3, 5), (0.6, 0.9), "rock"), single(sh_rock(1.8, 2.4), (1.2, 1.6), "rock", "mound", tiers=ROCKY),
    scatter(sh_hex(0.6, 0.9), (3, 5), (0.7, 1.0), "quartz", "up", 1.6), single(sh_tri(1.2, 1.6), (1.2, 1.6), "quartz", "up"),
    scatter(sh_round(0.5, 0.7), (6, 10), (0.35, 0.45), "pebble", "mound", 1.6, ((0.7, 10), (0.3, 45))), sunk(sh_blob(1.4, 2.0), (0.3, 0.45), "water"),
    trail(1.3, (5, 8)), path(0.7, (6, 10), 0.4, "rut", "cut", 0.6), path(0.5, (6, 10), 0.4, "cable", "up", 0.8),
    single(sh_rect(2.6, 3.2, 0.9, 1.1), (0.6, 0.8), "iron", "up", pat_slots(2, 0.3)), scatter(sh_caps(0.7, 1.0, 0.3), (5, 8), (0.3, 0.35), "iron", "up", 1.4),
    sunk(sh_disc(0.4, 0.55, 10), (0.6, 0.8), "pit", (4, 6), 1.4), sunk(sh_rect(2.0, 2.6, 1.4, 1.8), (0.5, 0.7), "pit"),
    single(sh_cross(2.4, 3.0, 0.6), (0.6, 0.8), "wood", "up"), scatter(sh_tri(0.7, 1.0), (4, 6), (0.5, 0.8), "rock", "mound", 1.8, ROCKY),
    single(sh_disc(1.2, 1.5, 24), (0.6, 0.8), "iron", "up", pat_hole(0.5, 0.7)), single(sh_rect(1.6, 2.0, 1.6, 2.0), (1.6, 2.0), "crate", "up", pat_cross()),
    scatter(sh_bone(1.4, 2.0), (1, 3), (0.4, 0.5), "bone", "up"), sunk(sh_wig(5, 8, 0.45, 0.8), (0.4, 0.5), "crack"),
    single(sh_ring(1.2, 1.6, 0.5), (0.5, 0.7), "iron", "up"), scatter(sh_blob(0.8, 1.1), (2, 3), (0.6, 0.9), "rock", "mound", 1.8, ROCKY),
    single(sh_ell(1.6, 2.0, 1.0, 1.2), (0.9, 1.2), "rock", "mound", pat_dots(), tiers=ROCKY), sunk(sh_crescent(1.2, 1.6), (0.3, 0.4), "pit"),
    single(sh_rect(1.0, 1.3, 1.0, 1.3), (2.0, 2.6), "wood", "up", pat_cross()),
],
}
DETAILS["infestation"] = DETAILS["infestation"][:30]


def apply(s, style, rng, t):
    """draw a random set of extra details for this base: tier 0 gets 2-3, tier 3 gets 6-8"""
    pool = DETAILS.get(style, [])
    if not pool: return
    k = rng.randint(2 + 1 * t + (t > 1), 3 + 1 * t + (t > 1) * 2)
    k = min(60, round(k * max(1.0, gg.K ** 2)))             # a bigger base has room for more
    for fn in (rng.sample(pool, k) if k <= len(pool) else rng.sample(pool, len(pool)) + [rng.choice(pool) for _ in range(k - len(pool))]):
        try: fn(s, rng, t)
        except Exception as e: print("  ! detail", style, type(e).__name__, str(e)[:80], flush=True)


# ------------------------------------------------------------------------------------------------ set pieces (big bases)
def a_outcrop(kind, tiers=2):
    """a large rock outcrop: ONE mass with steep sloped sides and a bevelled top (no stacked layers)"""
    def fn(s, rng, g_):
        r = rng.uniform(3.0, 4.6) * g_
        g, z = find(s, lambda cx, cy: stone(cx, cy, r, rng, rng.uniform(.55, .85), rng.randint(5, 8)), gap=0.6, at=lambda: rp(rng, 0, 12.3), tries=60)
        if g is None: return False
        mound(s, [g], z, rng.uniform(1.8, 2.8), ((0.8, 10), (0.2, 45)), kind); return True
    return fn

def a_heap(kind, tiers=((0.45, 18), (0.35, 35), (0.2, 52)), h=(1.6, 2.4)):
    """a large soft mound: rubble heap, dune, flesh mass, snow drift, earth pile"""
    def fn(s, rng, g_):
        r = rng.uniform(2.6, 3.8) * g_
        g, z = find(s, lambda cx, cy: blob(cx, cy, r, rng, rng.randint(4, 6)), gap=0.6, at=lambda: rp(rng, 0, 12.3), tries=60)
        if g is None: return False
        mound(s, [g], z, rng.uniform(*h), tiers, kind); return True
    return fn

def a_block(kind, top_kind=None, pattern=None, h=(1.4, 2.2)):
    """a large built piece: crate stack, machine, tomb, altar, platform: a big block with a smaller one on top"""
    def fn(s, rng, g_):
        L, W = rng.uniform(5.5, 7.5) * g_, rng.uniform(3.6, 5.0) * g_
        g, z = find(s, lambda cx, cy: rect(cx, cy, L, W, rng.uniform(0, math.pi)), gap=0.6, at=lambda: rp(rng, 0, 12.0), tries=60)
        if g is None: return False
        top = up(s, [g], z, rng.uniform(*h), kind)
        if not top: return False
        c = g.centroid
        if pattern is not None:
            pat = pattern(c.x, c.y, g, rng)
            if pat is not None and not pat.is_empty:
                q = pat.intersection(inset(g, 0.45))
                if not q.is_empty: topcut(s, [q], top, 0.3, "slot")
        elif top + 0.6 <= CAPZ:
            q = affinity.translate(affinity.scale(g, rng.uniform(.4, .6), rng.uniform(.5, .7)), rng.uniform(-.8, .8), rng.uniform(-.5, .5)).intersection(inset(g, 0.5))
            qs = [x for x in polys_of(q) if x.area > 3.0]
            if qs: s.prism([max(qs, key=lambda x: x.area)], min(top + rng.uniform(0.6, 1.0), CAPZ), top_kind or kind, z0=top - 0.15, foot=False)
        return True
    return fn

def a_ring(kind, n=(6, 9), h=(1.2, 2.0)):
    """a ring or arc of large stones / posts / columns around an open centre"""
    def fn(s, rng, g_):
        R_ = rng.uniform(5.0, 7.0) * g_; k = rng.randint(*n); a0 = rng.uniform(0, 6.283); arc = rng.uniform(0.55, 1.0) * 6.283
        def at(cx, cy): return [stone(cx + R_ * math.cos(a0 + arc * i / k), cy + R_ * math.sin(a0 + arc * i / k), rng.uniform(1.1, 1.5), rng, .8, rng.randint(5, 7)) for i in range(k)]
        ps, z = place_group(s, rng, at, tries=60, gap=0.5)
        if not ps: return False
        for p in ps: up(s, [p], z, rng.uniform(*h), kind)
        return True
    return fn

def a_pit(kind="pit", depth=(0.6, 0.85)):
    """a large hole: crater, sinkhole, pool, open grave"""
    def fn(s, rng, g_):
        r = rng.uniform(2.6, 3.8) * g_
        g, z = find(s, lambda cx, cy: stone(cx, cy, r, rng, rng.uniform(.75, .95), 9), gap=0.6, at=lambda: rp(rng, 0, 12.0), tries=60, ground=True)
        if g is None: return False
        cut(s, [g], z, rng.uniform(*depth), kind); return True
    return fn

ANCHORS = {
    "badlands": [a_outcrop("sandstone"), a_outcrop("rock"), a_heap("rock", ((0.5, 12), (0.5, 40)), (1.2, 1.8)), a_ring("rock"), a_pit("pit")],
    "highland": [a_outcrop("slate"), a_outcrop("slate"), a_ring("standing", (5, 8), (1.8, 2.6)), a_heap("moss", ((0.5, 25), (0.5, 50)), (1.0, 1.5)), a_pit("water", (0.4, 0.55))],
    "maahl": [a_outcrop("rock"), a_heap("dune", ((0.4, 15), (0.35, 30), (0.25, 50))), a_ring("column", (5, 7), (1.8, 2.6)), a_block("ruin", pattern=pat_rune()), a_pit("pit")],
    "corridor": [a_block("crate"), a_block("metal", pattern=pat_slots(5, 0.4)), a_block("metal", "pipe"), a_block("plate", pattern=pat_cross(), h=(0.5, 0.7)), a_pit("slot", (0.5, 0.7))],
    "infestation": [a_heap("flesh"), a_heap("flesh"), a_ring("pustule", (5, 8), (1.0, 1.4)), a_block("crate"), a_pit("acid", (0.5, 0.7))],
    "urban": [a_heap("rubble", ((0.5, 12), (0.5, 40)), (1.4, 2.0)), a_block("concrete", pattern=pat_seams(1.4)), a_block("brick"), a_block("crate"), a_pit("pothole")],
    "mine": [a_outcrop("bedrock"), a_heap("rock", ((0.5, 12), (0.5, 40))), a_block("crate"), a_block("wood", pattern=pat_seams(1.2), h=(0.8, 1.2)), a_pit("pit")],
    "swamp": [a_heap("mud", ((0.5, 20), (0.5, 45)), (1.0, 1.6)), a_outcrop("rock"), a_ring("tflag", (5, 7), (1.6, 2.4)), a_block("tflag", pattern=pat_rune(), h=(0.8, 1.2)), a_pit("water", (0.5, 0.6))],
    "cynder": [a_outcrop("basalt"), a_outcrop("obsidian"), a_ring("basalt", (5, 8), (1.4, 2.2)), a_heap("basalt", ((0.5, 12), (0.5, 40))), a_pit("lava", (0.6, 0.75))],
    "targa": [a_heap("snow", ((0.5, 25), (0.5, 55)), (1.0, 1.6)), a_block("alien", pattern=pat_slots(5, 0.4)), a_block("alien", "crystal"), a_outcrop("ice"), a_ring("alien", (5, 7), (1.6, 2.4))],
    "trederra": [a_pit("pit", (0.7, 0.9)), a_pit("water", (0.55, 0.7)), a_heap("sandbag", ((0.6, 10), (0.4, 40)), (1.2, 1.7)), a_block("crate"), a_heap("mud", ((0.5, 20), (0.5, 45)), (1.0, 1.5))],
    "fortress": [a_block("plank", pattern=pat_seams(1.2), h=(0.7, 1.0)), a_outcrop("garden"), a_ring("garden", (5, 7), (0.9, 1.4)), a_block("crate"), a_block("tatami", pattern=pat_cross(), h=(0.4, 0.6))],
    "beast": [a_heap("flesh"), a_heap("flesh"), a_ring("tooth", (6, 9), (1.6, 2.4)), a_pit("bile", (0.55, 0.7)), a_outcrop("flesh")],
    "frontier": [a_block("crate"), a_block("board", pattern=pat_seams(1.1), h=(0.8, 1.2)), a_heap("rock", ((0.5, 12), (0.5, 40)), (1.2, 1.8)), a_ring("post", (4, 6), (1.6, 2.2)), a_pit("rut", (0.4, 0.5))],
    "graveyard": [a_block("headstone", pattern=pat_ring(0.35), h=(0.8, 1.2)), a_block("headstone"), a_heap("earth", ((0.5, 20), (0.5, 45)), (1.0, 1.5)), a_pit("pit", (0.8, 0.95)), a_ring("headstone", (5, 7), (1.6, 2.4))],
    "forest": [a_outcrop("rock"), a_heap("moss", ((0.5, 25), (0.5, 50)), (1.0, 1.6)), a_ring("shroom", (6, 9), (0.7, 1.0)), a_ring("rock", (5, 7), (1.0, 1.6)), a_pit("water", (0.4, 0.55))],
    "dungeon": [a_block("flag"), a_block("crate"), a_ring("flag", (4, 6), (1.8, 2.6)), a_pit("pit", (0.7, 0.9)), a_block("iron", pattern=pat_slots(5, 0.4), h=(0.4, 0.6))],
    "temple": [a_block("tflag", "tcolumn"), a_ring("tcolumn", (5, 8), (1.8, 2.6)), a_block("tflag", pattern=pat_rune(), h=(0.8, 1.2)), a_outcrop("tcolumn"), a_pit("pit", (0.6, 0.8))],
    "coast": [a_outcrop("shelf"), a_outcrop("shelf"), a_pit("water", (0.55, 0.7)), a_ring("shelf", (5, 7), (1.0, 1.6)), a_block("drift", pattern=pat_seams(1.6), h=(0.7, 1.0))],
}


def anchors(s, style, rng):
    """large set pieces for bases bigger than 30 mm: more of them, and somewhat bigger, as the base grows"""
    pool = ANCHORS.get(style, [])
    if not pool: return
    K = gg.K; g_ = min(1.9, K ** 0.5)                       # 40 mm: x1.15, 60 mm: x1.4, 100 mm: x1.8
    import os
    if float(os.environ.get("KIT_SCALE", 1) or 1) != 1.0: g_ = 1.3          # with a scenery scale a set piece is one size on every base
    n = max(1, min(16, round(1.6 * (K * K - 1) / g_)))       # 40 mm: 1, 60 mm: 3, 100 mm: 9
    placed = 0
    for i in range(n * 3):
        if placed >= n: break
        try:
            if rng.choice(pool)(s, rng, g_): placed += 1
        except Exception as e: print("  ! set piece", style, type(e).__name__, str(e)[:80], flush=True)
