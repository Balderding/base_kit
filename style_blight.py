"""blight: ground corrupted by something unnatural (2026-11-19). Twisted spikes and thorns of rock push up through
it, bones lie heaped where things died, pools of ichor bubble, rune circles of an unknown script are cut into it and
cracked across, the rock itself has swollen into fleshy lumps, and rifts are torn open in the ground. Everything that
is here belongs to this file (see newstyles.py for what a style defines); its style-only details are in
sd_r_x_blight.py and their real sizes in scale/real_blight.json.

The runes are the style's own invention (strokes in BLIGHT_RUNES below): hooks, barbed stems, broken chevrons, an eye
with a tick. No star with many arms, no sign of any faction from any game.

Ground layouts (one picked per base):
  0  corrupted plates: the ground split into plates of blighted stone with wavering seams, thorns through the seams
  1  torn rift: a jagged rift right across the base, its lips heaved up in broken blocks, cracks running off it
  2  ichor seeps: pools of bubbling ichor with fleshy swellings of rock round them
  3  thorn ground: cracked ground with clumps of rock thorns pushing up
  4  swollen ground: the rock gone soft and lumpy, deep folds between the lumps, an ichor pocket or two"""
import math

NAME = "blight"
TITLE = "The Blight"
SAYS = ("ground corrupted by something unnatural: twisted spikes and thorns of rock, heaps of bones, bubbling pools of ichor, "
        "cracked rune circles of an unknown script, fleshy lumpy rock and rifts torn open in the ground")
GROUND = "#6a6152"
PAL = dict(wood="bone", iron="iron", rock="blightrock", stone="blightrock", crate="crate", water="ichor", pit="rift", gem=None,          # gem None: the kit's crystals are hex towers, not thorns
           floor="blightrock", plant=None, soft="fleshrock", bone="bone", twig="bone", vein="tendril")
PATH_OK = "trail stones way stream"
EXTRA_PROPS = "tentacle teeth pustules eggs"
BUILT, ENCLOSED, CLIFFS = False, False, False          # no cliff: the kit's rock face came out as a plain tall block on the rim
COLOURS = {"blightrock": "#584a54", "thorn": "#2a2230", "fleshrock": "#8c5a60", "blister": "#b48a6a", "runestone": "#6a6270",
           "tendril": "#7a3b4a", "pustule": "#c9a35a"}
CUTS = {"ichor": "#8fb03a", "rift": "#140a14", "runecut": "#a8452c"}          # new cut kinds: their colour in the pictures

SPIKE = ((0.75, 4), (0.25, 30))          # steep thorn sides; the top tier eases so the tip keeps a printable point


def T(name, subject, path, props, also="", **kw):          # as themes.T (not imported: themes.py loads this file while it is itself loading)
    return dict(theme=name, subject=subject, path=path, props=props, also=also, **kw)

THEMES = [
    T("spike garden", "blight_thorns", "trail:to", "at:rubble*3 front:skull at:bones*2 far:crack*2 back:pebbles", also="crater:far",
      clutter=dict(debris=2.6, growth=1.6, marks=1.4), ban=("crate", "wood")),
    T("bone altar", "dais_altar", "way:to", "front:skull at:bones*3 at:ribs path:brazier*2 back:rubble*2", also="skeleton:far",
      clutter=dict(debris=2.0, growth=1.6, marks=1.4), ban=("crate",)),
    T("boiling pool", "pool", "stones:to", "at:pustules*2 at:puddle*2 at:bones at:rubble far:crack", also="crater:far",
      clutter=dict(debris=1.8, growth=2.0, marks=1.6), ban=("crate", "wood")),
    T("rune circle", "blight_runes", "stones:to", "at:brazier*2 at:slab front:skull at:rubble*2 far:crack*2", pal=dict(stone="runestone"),
      clutter=dict(debris=2.0, growth=1.6, marks=1.6), ban=("crate",)),
    T("rift in the ground", "blight_rift", "trail:past", "at:crack*3 at:rubble*3 at:tentacle at:bones far:skull",
      pal=dict(rock="thorn"), clutter=dict(debris=2.4, growth=1.6, marks=1.6), ban=("crate", "wood")),
]

# the style's own script: each rune a few strokes in a box from -1 to 1 (made up for this style, nothing borrowed)
BLIGHT_RUNES = [
    [((0, -1), (0, 1)), ((0, 1), (0.6, 0.55)), ((0.6, 0.55), (0.6, 0.1))],                          # the hook
    [((-0.6, 1), (0.25, 0.35)), ((0.25, 0.35), (-0.25, -0.35)), ((-0.25, -0.35), (0.6, -1))],       # the zig
    [((0, -1), (0, 1)), ((0, 0.45), (0.55, 0.85)), ((0, -0.15), (-0.55, 0.2))],                     # the barbed stem
    [((-0.6, -0.9), (0, 0.9)), ((0, 0.9), (0.15, -0.2)), ((0.15, -0.2), (0.6, 0.3))],               # the broken chevron
    [((0, 0.2), (0, 1)), ((-0.42, -0.25), (0, -0.67)), ((0, -0.67), (0.42, -0.25)), ((0.42, -0.25), (0, 0.17)), ((0, 0.17), (-0.42, -0.25))],   # the eye with a tick
    [((-0.3, -1), (-0.3, 1)), ((-0.3, 0.5), (0.45, 0.15)), ((-0.3, -0.25), (0.45, -0.6))],          # the ladder
    [((0.5, -1), (-0.2, -0.2)), ((-0.2, -0.2), (0.3, 0.4)), ((0.3, 0.4), (-0.4, 1))],               # the crook
    [((-0.55, -0.8), (0.55, -0.8)), ((-0.35, -0.8), (-0.1, 0.9)), ((0.35, -0.8), (0.5, 0.3))],      # the forked foot
]

_MINE = None          # this module's own names: tables() fills the rest of its globals from the kit's and must not overwrite them


def _take(G):
    global _MINE
    if _MINE is None: _MINE = {k for k in globals()}
    globals().update({k: v for k, v in G.items() if not k.startswith("__") and k not in _MINE})


_DONE = [False]

def _register():
    """the style's new kinds where the kit looks them up at run time: cut colours for the pictures, ichor counted as a
    liquid (things gather by it as by water), the rift and the rune cuts as ground marks. Only adds, never changes."""
    if _DONE[0]: return
    import sys
    gg_ = sys.modules.get("gg")
    if gg_ is not None:
        for k, v in CUTS.items(): gg_.CUTC.setdefault(k, v)
    sc = sys.modules.get("scenes")
    if sc is None: return
    try:
        sc.WETK.add("ichor"); sc.CUTK.update(CUTS)
        sc.DEBRIS.update({"blightrock", "thorn", "runestone"}); sc.GROWTH.update({"fleshrock", "blister"})
        # the style's own three subjects, under names no other style uses (the kit's spires are hex crystals, its circle
        # carries its own star): twisted thorns, a torn rift, a ring of this style's runes
        sc.SUBJECTS.setdefault("blight_thorns", lambda p: L_thorns(p.get("gem") or "thorn"))
        sc.SUBJECTS.setdefault("blight_rift", lambda p: L_rift(p.get("pit") or "rift", "blightrock"))
        sc.SUBJECTS.setdefault("blight_runes", lambda p: L_rune_ring("runecut", p.get("pit") or "rift"))
        sc.SUBJECT_SAYS.setdefault("blight_thorns", "a cluster of twisted rock thorns")
        sc.SUBJECT_SAYS.setdefault("blight_rift", "a rift torn open in the ground")
        sc.SUBJECT_SAYS.setdefault("blight_runes", "a cracked ring of unknown runes cut into the ground")
        sc.TALL.add("blight_thorns")
    except Exception: pass
    _DONE[0] = True


def rune(f2, k, s, w):
    """rune number k of BLIGHT_RUNES, s mm high, strokes w wide, in the frame f2 (u along, v across)"""
    strokes = BLIGHT_RUNES[k % len(BLIGHT_RUNES)]
    return unary_union([LineString([f2(p[0] * s * 0.5, p[1] * s * 0.5), f2(q[0] * s * 0.5, q[1] * s * 0.5)]).buffer(w / 2, cap_style=1, resolution=3) for p, q in strokes])


# ------------------------------------------------------------------------------------------------ ground
def _thorn_clumps(s, rng, n, r=(0.7, 1.05), h=(1.4, 2.3), region=None):
    """clumps of rock thorns: a big one and its smaller neighbours, each a steep three or four sided spike"""
    for p in s.scatter(n, 1.4, 1.8, region=region, gap=0.6, round_=False):
        c = p.centroid; ps = [stone(c.x, c.y, rng.uniform(*r), rng, rng.uniform(0.6, 0.85), rng.choice((3, 4)))]
        for _ in range(rng.randint(1, 3)):
            t = rng.uniform(0, 6.283); d = rng.uniform(1.3, 1.9); q = stone(c.x + d * math.cos(t), c.y + d * math.sin(t), rng.uniform(0.5, 0.7), rng, 0.7, 3)
            if all(q.distance(o) > 0.25 for o in ps) and Point(0, 0).buffer(R0 - 0.8).contains(q) and s.free(q, 0.3) and (region is None or region.contains(q)): ps.append(q)
        s.stack(ps[:1], rng.uniform(*h), SPIKE, "thorn")
        if len(ps) > 1: s.stack(ps[1:], rng.uniform(0.9, 1.4), SPIKE, "thorn")


def _rift(s, rng, phi, w=(1.5, 2.4), depth=0.85, keep=None, lip_h=(0.5, 1.0)):
    """a rift torn right across the base: jagged sides (teeth bitten out of both edges), its lips heaved up in broken
    blocks on either side, short cracks running off into the ground. Returns the rift polygon."""
    wd = rng.uniform(*w); ln = meander(rng, phi, rng.uniform(-3.5, 3.5), rng.uniform(1.8, 3.2), wd)
    g = ln.buffer(wd / 2, cap_style=2, join_style=1)
    L = ln.length; teeth = []; cr = []
    for i in range(int(L / 1.1)):
        u = (i + rng.uniform(0.2, 0.8)) * 1.1
        if u >= L - 0.5: break
        p = ln.interpolate(u); q = ln.interpolate(min(L, u + 0.3)); t = math.atan2(q.y - p.y, q.x - p.x) + rng.choice((-1, 1)) * math.pi / 2
        d = wd / 2 + rng.uniform(-0.1, 0.25); teeth.append(stone(p.x + d * math.cos(t), p.y + d * math.sin(t), rng.uniform(0.35, 0.6), rng, 0.7, 3))
        if rng.random() < 0.16:            # a crack running off the rift
            a = t + rng.uniform(-0.5, 0.5); L2 = rng.uniform(2.5, 5.0); b = (p.x + (d + L2) * math.cos(a), p.y + (d + L2) * math.sin(a))
            cr.append(LineString(wiggle((p.x, p.y), b, rng, 0.5, 4)).buffer(0.22, cap_style=1, resolution=3))
    disc = Point(0, 0).buffer(R0 - 0.9, 96)
    g = unary_union([g] + teeth).intersection(disc).simplify(0.04)
    if g.is_empty: return Polygon()
    s.gcut(polys_of(g), depth, "rift")
    if cr: s.gcut(polys_of(unary_union(cr).difference(g).intersection(disc)), 0.45, "rift")
    lips = []
    for i in range(int(L / 1.6)):          # the lips: broken blocks heaved up along both edges
        u = (i + rng.uniform(0, 1)) * 1.6
        if u >= L: break
        p = ln.interpolate(u); q = ln.interpolate(min(L, u + 0.3)); t = math.atan2(q.y - p.y, q.x - p.x) + rng.choice((-1, 1)) * math.pi / 2
        d = wd / 2 + rng.uniform(1.0, 1.5); b = stone(p.x + d * math.cos(t), p.y + d * math.sin(t), rng.uniform(0.7, 1.05), rng, rng.uniform(0.55, 0.8), rng.randint(4, 6))
        if b.distance(g) > 0.25 and disc.buffer(-0.4).contains(b) and all(b.distance(o) > 0.3 for o in lips) and (keep is None or not b.intersects(keep)): lips.append(b)
    for b in lips: s.stack([b], rng.uniform(*lip_h), ROCKY, "blightrock")
    return g


def _theme_here(s):
    """the theme this base will get (the kit picks it from the base number, the same way): forced, or worked out"""
    import os
    th = os.environ.get("THEME", "").strip().lower()
    if th: return th
    try:
        import scenes
        n = int(str(s.name).rsplit("_", 1)[1]); order = scenes.theme_order(NAME, os.environ.get("SALT", ""))
        return THEMES[order[((n - 1) % 5 + (n - 1) // 5) % len(order)]]["theme"]
    except Exception: return ""


def _figure_ground(s, rng, theme, keep, rg, floored=False):
    """At figure scale the middle is kept clear for the miniature and only the ring round it carries things, so the
    ground itself has to say blight: a web of cracks over the whole base (through the middle too), the style's runes
    cut small in the middle, and the ring crowded with thorns, swollen rock, broken stone and the theme's own ground."""
    D = Point(0, 0).buffer(R0 - 0.9, 96); A = rg.area / 200.0          # how many: by the room the ring has (200 mm2 = a 60 mm base's ring)
    busy = unary_union([q for ps, d_, k_ in s.gcuts for q in ps] + list(s.feet)).buffer(0.35) if (s.gcuts or s.feet) else Polygon()
    web = Polygon() if floored else j_voronoi(rng, rng.randint(16, 24), 0.2, 0.5).intersection(D).difference(busy)          # a floor has its own seams
    if not web.is_empty: s.gcut(polys_of(web), 0.32, "rift")
    busy = unary_union([busy, web.buffer(0.3)]) if not web.is_empty else busy
    rk = (keep.bounds[2] - keep.bounds[0]) / 2; mk = []
    for i in range(rng.randint(4, 6)):          # small runes cut in the miniature's patch
        for _ in range(20):
            t_ = rng.uniform(0, 6.283); d_ = rng.uniform(0.25, 0.85) * rk; c = (d_ * math.cos(t_), d_ * math.sin(t_))
            g = rune(frame(c[0], c[1], rng.uniform(0, 6.283)), rng.randrange(len(BLIGHT_RUNES)), rng.uniform(1.1, 1.5), 0.26)
            if not g.buffer(0.3).intersects(busy) and all(g.distance(o) > 0.5 for o in mk): mk.append(g); break
    if mk and floored: s.tcut([q for g in mk for q in polys_of(g)], H + 0.2, H + 0.9, "runecut")
    elif mk: s.gcut([q for g in mk for q in polys_of(g)], 0.3, "runecut")
    if theme == "rune circle":          # a ring of runes round the miniature's patch
        Rr = rk + 1.6; n_ = rng.randint(9, 12); a0 = rng.uniform(0, 6.283)
        rr = [ring(0, 0, Rr + 1.4, Rr + 1.05, 64)] + [rune(frame(Rr * math.cos(a0 + 6.283 * i / n_), Rr * math.sin(a0 + 6.283 * i / n_), a0 + 6.283 * i / n_ + math.pi / 2), rng.randrange(len(BLIGHT_RUNES)), 1.4, 0.3) for i in range(n_)]
        s.gcut(polys_of(unary_union(rr).intersection(D)), 0.35, "runecut")
    k2 = max(0.6, A)
    _thorn_clumps(s, rng, round(rng.randint(3, 5) * k2 * (1.8 if theme == "spike garden" else 1.0)), region=rg)
    banks(s, rng, round(rng.randint(2, 3) * k2), 1.4, 2.4, (0.6, 1.1), "fleshrock", tiers=SOFTT, region=rg)
    for p in s.scatter(round(rng.randint(3, 5) * k2), 0.9, 1.6, region=rg, gap=0.5, round_=False): s.stack([p], rng.uniform(0.6, 1.1), ROCKY, "blightrock")
    for p in s.scatter(round(rng.randint(3, 5) * k2), 0.5, 0.8, region=rg, gap=0.4): s.stack([p], rng.uniform(0.6, 0.9), SOFTT, "blister")
    if theme in ("bone altar", "spike garden", "rift in the ground"):          # bones lie about
        for p in s.scatter(round(rng.randint(3, 5) * k2), 0.6, 0.8, region=rg, gap=0.4):
            c = p.centroid; a_ = rng.uniform(0, math.pi); L_ = rng.uniform(1.6, 2.4); ff = frame(c.x, c.y, a_)
            bn = unary_union([LineString([ff(-L_ / 2, 0), ff(L_ / 2, 0)]).buffer(0.3), Point(*ff(-L_ / 2, 0)).buffer(0.42, 10), Point(*ff(L_ / 2, 0)).buffer(0.42, 10)])
            if rg.contains(bn) and s.free(bn, 0.3): s.stack([bn], rng.uniform(0.45, 0.6), ((0.45, 12), (0.3, 38), (0.25, 62)), "bone")
    if theme == "boiling pool": pools(s, rng, rng.randint(2, 3), 1.5, 2.4, 0.6, "ichor", avoid=keep.buffer(0.5))


def layout(n):
    _register()
    import os
    s = gg2.mk(NAME, TITLE, n, GROUND); rng = s.rng; v = rng.randint(0, 4); D = _D(); phi, f, R = _fr(rng)
    ts = float(os.environ.get("KIT_SCALE", 1) or 1); fig = ts != 1.0
    keep = ring_ = None; theme = _theme_here(s) if fig else ""
    if fig:          # the miniature's patch in the middle (as scenes.compose keeps it clear), a little wider
        rv = min(R0, float(os.environ.get("KIT_FINAL_MM") or 2 * R0 * ts) / (2 * ts)); keep = Point(0, 0).buffer(0.5 * rv, 48)
        ring_ = Point(0, 0).buffer(min(R0 - 0.9, rv - 0.5), 96).difference(keep.buffer(0.4))          # rv: what the finished base shows of the layout
        v = {"rift in the ground": 1, "boiling pool": 2, "spike garden": 3}.get(theme, rng.choice((0, 1, 3, 4)))
    if v == 1 and fig:          # a bold rift: wider, deeper, its lips heaved higher (kept out of the miniature's patch)
        _rift(s, rng, phi, w=(2.4, 3.2), depth=0.95, keep=keep.buffer(0.3), lip_h=(0.8, 1.4))
    elif v == 0:            # corrupted plates with wavering seams
        floor(s, D, 0.5, "blightrock", j_voronoi(rng, rng.randint(14, 22), 0.3, 0.6), depth=0.45)
        _thorn_clumps(s, rng, rng.randint(1, 3), region=ring_)
    elif v == 1:          # a rift torn across the base
        _rift(s, rng, phi)
        if rng.random() < 0.5: _thorn_clumps(s, rng, rng.randint(1, 2))
    elif v == 2:          # ichor seeps with fleshy swellings round them
        pools(s, rng, rng.randint(2, 3), 1.8, 3.0, 0.6, "ichor", avoid=keep)
        banks(s, rng, rng.randint(2, 3), 1.5, 2.4, (0.6, 1.1), "fleshrock", tiers=SOFTT, region=ring_)
        cracks(s, rng, rng.randint(1, 2), 0.4, "rift")
    elif v == 3:          # thorn ground
        cracks(s, rng, rng.randint(3, 4), 0.45, "rift")
        _thorn_clumps(s, rng, rng.randint(2, 4), region=ring_)
        banks(s, rng, 1, 1.4, 2.0, (0.5, 0.9), "fleshrock", tiers=SOFTT, region=ring_)
    else:                 # swollen ground: soft lumpy rock with deep folds
        floor(s, D, 0.55, "fleshrock", j_voronoi(rng, rng.randint(9, 14), 0.34, 0.9), depth=0.5)
    if fig: _figure_ground(s, rng, theme, keep, ring_, floored=v in (0, 4))
    else:
        for p in s.scatter(rng.randint(1, 3), 1.0, 1.7, gap=0.5, round_=False): s.stack([p], rng.uniform(0.7, 1.2), ROCKY, "blightrock")
    grit(s, rng, 10, 20); return s


# ------------------------------------------------------------------------------------------------ signature features
def f_thorns(s, rng, lvl):
    """a clump of rock thorns round a tall one"""
    def at(cx, cy):
        ps = [stone(cx, cy, rng.uniform(0.75, 1.05), rng, rng.uniform(0.6, 0.85), rng.choice((3, 4)))]
        for _ in range(rng.randint(2, 4)):
            t = rng.uniform(0, 6.283); d = rng.uniform(1.4, 2.0); q = stone(cx + d * math.cos(t), cy + d * math.sin(t), rng.uniform(0.5, 0.7), rng, 0.7, 3)
            if all(q.distance(o) > 0.25 for o in ps): ps.append(q)
        return ps
    ps, z = place_group(s, rng, at, gap=0.4)
    if not ps: return
    mound(s, ps[:1], z, rng.uniform(1.8, 2.6), SPIKE, "thorn")
    if len(ps) > 1: mound(s, ps[1:], z, rng.uniform(1.0, 1.5), SPIKE, "thorn")

def f_boil(s, rng, lvl):
    """a small boiling pool: ichor in a hollow, a ring of blistered rim, bubbles standing in it just under the rim"""
    def mk():
        cx, cy = rp(rng, 1, 9); r = rng.uniform(1.5, 2.1)
        return blob(cx, cy, r, rng, 4).buffer(1.0)
    g, z = find(s, mk, gap=0.4)
    if g is None or z > H + 1e-6 or terr(s): return
    pool = inset(g, 1.0)
    if pool.is_empty: return
    cut(s, [pool], z, 0.6, "ichor")
    rim = [q for q in polys_of(g.difference(pool.buffer(0.25))) if q.area > 0.3]
    bl = []
    for q in rim:          # the rim broken into blisters
        for _ in range(5):
            c = q.representative_point(); b = stone(c.x + rng.uniform(-0.6, 0.6), c.y + rng.uniform(-0.6, 0.6), rng.uniform(0.45, 0.7), rng, 0.85, 7).intersection(q)
            if not b.is_empty and b.area > 0.3 and all(b.distance(o) > 0.2 for o in bl): bl.append(b)
    if bl: mound(s, [p for b in bl for p in polys_of(b) if wide_enough(p, 0.7)], z, rng.uniform(0.4, 0.65), SOFTT, "blister")
    bub = []
    x0, y0, x1, y1 = pool.bounds
    for _ in range(80):
        if len(bub) >= rng.randint(2, 4): break
        c = Point(rng.uniform(x0, x1), rng.uniform(y0, y1)); r = rng.uniform(0.5, 0.75); b = c.buffer(r, 14)
        if inset(pool, 0.3).contains(b) and all(b.distance(o) > 0.35 for o in bub): bub.append(b)
    if bub: s.prism(bub, H - 0.15, "blister", z0=H - 0.75, foot=False)          # bubbles breaking the surface, a hair under the rim

def f_runes(s, rng, lvl):
    """a ring of runes cut into the ground, a crack split right across it"""
    def mk():
        cx, cy = rp(rng, 1, 8); return Point(cx, cy).buffer(rng.uniform(2.6, 3.2), 24)
    g, z = find(s, mk, gap=0.3)
    if g is None: return
    c = g.centroid; Rr = (g.bounds[2] - g.bounds[0]) / 2 - 0.25; a0 = rng.uniform(0, 6.283); n = rng.randint(5, 7)
    marks = [ring(c.x, c.y, Rr, Rr - 0.35, 40)]
    for i in range(n):
        t = a0 + 6.283 * i / n; m = (c.x + Rr * 0.6 * math.cos(t), c.y + Rr * 0.6 * math.sin(t))
        marks.append(rune(frame(m[0], m[1], t + math.pi / 2), rng.randrange(len(BLIGHT_RUNES)), 1.1, 0.28))
    g2 = unary_union(marks)
    ca = rng.uniform(0, 6.283); cr = LineString(wiggle((c.x - (Rr + 0.6) * math.cos(ca), c.y - (Rr + 0.6) * math.sin(ca)), (c.x + (Rr + 0.6) * math.cos(ca), c.y + (Rr + 0.6) * math.sin(ca)), rng, 0.5, 5)).buffer(0.25, cap_style=1, resolution=3)
    cut(s, [p for p in polys_of(g2.difference(cr.buffer(0.3)))], z, 0.35, "runecut")
    cut(s, polys_of(cr), z, 0.5, "rift")

def f_bones(s, rng, lvl):
    """a heap of bones: long bones lying across each other on a low mound of smaller ones"""
    def at(cx, cy):
        base = blob(cx, cy, rng.uniform(1.3, 1.7), rng, 4); out = [base]
        for _ in range(rng.randint(2, 3)):
            t = rng.uniform(0, math.pi); L = rng.uniform(2.6, 3.4); ff = frame(cx + rng.uniform(-0.6, 0.6), cy + rng.uniform(-0.6, 0.6), t)
            out.append(unary_union([LineString([ff(-L / 2, 0), ff(L / 2, 0)]).buffer(0.32, cap_style=1), Point(*ff(-L / 2, 0)).buffer(0.5, 10), Point(*ff(L / 2, 0)).buffer(0.5, 10)]))
        return out
    ps, z = place_group(s, rng, at, gap=0.4)
    if not ps: return
    mound(s, [ps[0]], z, rng.uniform(0.5, 0.7), SOFTT, "bone")
    for q in ps[1:]: mound(s, [q], z, rng.uniform(0.7, 0.95), ((0.45, 12), (0.3, 38), (0.25, 62)), "bone")


def pat_brune():
    """a top pattern: one of the style's own runes cut into the top of a stone"""
    def f(cx, cy, g, rng):
        return rune(frame(cx, cy, rng.uniform(0, 6.283)), rng.randrange(len(BLIGHT_RUNES)), 1.7, 0.32)
    return f


def _cls(fn, c):
    fn.cls = c; return fn


def tables(G):
    _take(G)
    sp = scatter(sh_tri(0.6, 0.9), (4, 7), (1.1, 1.7), "thorn", "mound", 1.7, SPIKE)
    big = single(sh_tri(1.3, 1.8), (2.0, 2.8), "thorn", "mound", tiers=SPIKE)
    ichor = sunk(sh_blob(1.3, 1.9), (0.45, 0.6), "ichor")
    drops = sunk(sh_disc(0.45, 0.65, 12), (0.4, 0.5), "ichor", (3, 6), 1.6)
    rift = sunk(sh_wig(5, 9, 0.5, 1.0), (0.5, 0.65), "rift")
    lump = single(sh_blob(1.6, 2.2), (0.9, 1.3), "fleshrock", "mound", tiers=SOFTT)
    lumps = scatter(sh_round(0.6, 0.9), (3, 6), (0.6, 0.9), "fleshrock", "mound", 1.6, SOFTT)
    blis = scatter(sh_disc(0.55, 0.85, 16), (3, 6), (0.6, 0.9), "blister", "mound", 1.5, SOFTT)
    bones = scatter(sh_bone(1.5, 2.2), (2, 4), (0.4, 0.5), "bone", "up")
    rocks = scatter(sh_rock(0.9, 1.4), (3, 5), (0.6, 1.0), "blightrock")
    rubble = scatter(sh_tri(0.7, 1.1), (3, 6), (0.5, 0.8), "blightrock", "mound", 1.8, ROCKY)
    boulder = single(sh_rock(1.8, 2.5), (1.1, 1.6), "blightrock", "mound", tiers=ROCKY)
    pebbles = scatter(sh_round(0.5, 0.75), (6, 10), (0.35, 0.5), "pebble", "mound", 1.6, ((0.7, 10), (0.3, 45)))
    glyph = single(sh_hex(1.2, 1.6), (0.6, 0.8), "runestone", "up", pat_brune())
    ring_ = sunk(sh_ring(1.3, 1.7, 0.4), (0.3, 0.4), "runecut")
    star_ = sunk(sh_star(1.2, 1.6, 5, 6), (0.35, 0.45), "rift")          # a burst crack (five or six arms)
    pits = sunk(sh_disc(0.4, 0.55, 10), (0.3, 0.4), "rift", (4, 7), 1.5)
    skull = single(sh_ell(1.1, 1.4, 0.7, 0.9), (0.6, 0.8), "bone", "mound", pat_dots(0.22))
    teeth = scatter(sh_crescent(0.8, 1.1), (2, 4), (0.5, 0.7), "bone", "up")
    tend = path(0.6, (6, 10), 0.5, "tendril", "up", 1.3)
    for fn, c in ((sp, "debris"), (big, "debris"), (rocks, "debris"), (rubble, "debris"), (boulder, "debris"), (lump, "growth"), (lumps, "growth"),
                  (blis, "growth"), (tend, "growth"), (glyph, "made")):
        _cls(fn, c)
    for fn in (f_thorns, f_boil, f_runes, f_bones): fn.tmpl, fn.kind, fn.shape = "single", {"f_thorns": "thorn", "f_boil": "ichor", "f_runes": "runecut", "f_bones": "bone"}[fn.__name__], ""
    _cls(f_thorns, "debris"); _cls(f_boil, "marks"); _cls(f_runes, "marks"); _cls(f_bones, "bones")
    tiers = ([f_thorns, ichor, bones],
             [f_boil, rift, lump, f_bones],
             [f_runes, big, blis, f_thorns])
    pool = [sp, big, ichor, drops, rift, lump, lumps, blis, bones, rocks, rubble, boulder, pebbles, glyph, ring_, star_, pits, skull, teeth, tend,
            f_thorns, f_boil, f_runes, f_bones,
            scatter(sh_tri(0.5, 0.7), (5, 9), (0.8, 1.2), "thorn", "mound", 1.4, SPIKE), sunk(sh_crescent(1.0, 1.4), (0.4, 0.5), "rift"),
            single(sh_blob(1.4, 1.9), (0.5, 0.7), "fleshrock", "mound", pat_hole(0.4, 0.6), tiers=SOFTT), sunk(sh_disc(1.2, 1.7, 20), (0.45, 0.6), "ichor"),
            _cls(scatter(sh_ell(0.55, 0.75, 0.8, 1.05), (3, 5), (0.6, 0.85), "fleshrock", "mound", 1.5, SOFTT), "growth"),
            _cls(single(sh_tri(0.9, 1.2), (1.4, 1.9), "thorn", "mound", tiers=SPIKE), "debris")]
    return dict(tiers=tiers, details=pool)


# ------------------------------------------------------------------------------------------------ multi-part features and landmarks
def _S():
    import scenes; return scenes

def _spike_solid(cx, cy, r, h, lean=0.0, la=0.0, twist=0.0, a0=0.0, pts=4, inner=0.62):
    """one twisted rock thorn: a star-shaped section that shrinks, turns and leans as it rises (each step stays
    inside the one below, so it prints without supports), ending in a blunt point"""
    def sect(z):
        t = z / h
        if t >= 1: return None
        rr = r * (1 - t) ** 0.9
        if rr < 0.24: return None
        x = cx + lean * t * math.cos(la); y = cy + lean * t * math.sin(la)
        return star(x, y, rr, rr * inner, pts, a0 + twist * t)
    return (sect, h)

def b_thorn_cluster(kind="thorn", n=(3, 6)):
    """a cluster of twisted thorns of rock, the tallest in the middle leaning out, the others round it"""
    def f(cx, cy, a, rng, k=1.0):
        kk = max(1.0, k ** 0.6); sol = []; foot = []
        r0 = rng.uniform(1.0, 1.3) * kk; h0 = min(CAPZ - H - 0.1, rng.uniform(2.6, 3.2) * max(1.0, k ** 0.35))
        sol.append(_spike_solid(cx, cy, r0, h0, r0 * 0.45, rng.uniform(0, 6.283), rng.uniform(0.8, 1.6) * rng.choice((-1, 1)), rng.uniform(0, 3), rng.choice((4, 5)))); foot.append(Point(cx, cy).buffer(r0))
        for i in range(rng.randint(*n) + int(2 * (k - 1))):
            t = a + rng.uniform(0, 6.283); d = rng.uniform(1.6, 2.4) * kk; r = rng.uniform(0.55, 0.85) * kk; x, y = cx + d * math.cos(t), cy + d * math.sin(t)
            g = Point(x, y).buffer(r)
            if all(g.distance(q) > 0.2 for q in foot):
                foot.append(g); sol.append(_spike_solid(x, y, r, min(CAPZ - H - 0.3, rng.uniform(1.4, 2.6) * max(1.0, k ** 0.35)), r * 0.5, t + rng.uniform(-0.5, 0.5), rng.uniform(0.8, 2.0) * rng.choice((-1, 1)), rng.uniform(0, 3), rng.choice((3, 4))))
        return _u(foot).convex_hull.buffer(0.2), [_S()._steps(sol, kind)]
    return f

def b_bone_heap(kind="bone"):
    """bones heaped where something died: a low heap, long bones across it, a skull on top"""
    def f(cx, cy, a, rng, k=1.0):
        S = _S(); kk = max(1.0, k ** 0.7); R_ = rng.uniform(1.6, 2.0) * kk
        sol = [S.lathe(cx, cy, [(R_, 0.0), (R_ * 0.8, 0.35), (R_ * 0.45, 0.6), (0.3, 0.7)], 14, rng.uniform(0.7, 0.9), a)[0]]
        for i in range(rng.randint(3, 5) + int(2 * (k - 1))):
            t = rng.uniform(0, math.pi); L = rng.uniform(2.6, 3.6) * kk; ox, oy = cx + rng.uniform(-0.7, 0.7) * kk, cy + rng.uniform(-0.7, 0.7) * kk; ff = frame(ox, oy, t)
            sol += S.tube([ff(-L / 2, 0), ff(L / 2, 0)], lambda q: 0.42 if q < 0.1 or q > 0.9 else 0.28, 1.2, lift=rng.uniform(0.0, 0.3))
        sx, sy = cx + rng.uniform(-0.5, 0.5), cy + rng.uniform(-0.5, 0.5)
        sol += S.lathe(sx, sy, S.curve(0.75, 0.22, 0.0, 0.9, 6), 16, 0.8, a + 1.2, lift=0.45)
        return Point(cx, cy).buffer(R_ + 1.6 * kk), [S._steps(sol, kind, cuts=[(Point(sx + 0.25 * math.cos(a + 1.2 + sv * 0.6), sy + 0.25 * math.sin(a + 1.2 + sv * 0.6)).buffer(0.17, 6), 1.0) for sv in (-1, 1)])]
    return f

def b_boiling_pool(water="ichor", rim="blister"):
    """a pool of ichor boiling in a blistered hollow: bubbles standing in it, a ring of swellings round it"""
    def f(cx, cy, a, rng, k=1.0):
        kk = max(1.0, k ** 0.8); R_ = rng.uniform(2.0, 2.6) * kk; pool = blob(cx, cy, R_, rng, 5)
        lay = [("gcut", pool, rng.uniform(0.6, 0.8), water)]
        x0, y0, x1, y1 = pool.bounds; bub = []
        for _ in range(120):
            if len(bub) >= rng.randint(3, 5) + int(3 * (k - 1)): break
            c = Point(rng.uniform(x0, x1), rng.uniform(y0, y1)); r = rng.uniform(0.5, 0.8) * max(1.0, k ** 0.3); b = c.buffer(r, 14)
            if inset(pool, 0.35).contains(b) and all(b.distance(o) > 0.35 for o in bub): bub.append(b)
        for b in bub: lay.append(("pad", b, rim))          # bubbles breaking the surface, a hair under the rim
        n = rng.randint(6, 9) + int(5 * (k - 1)); out = []
        for i in range(n):
            t = a + 6.283 * i / n + rng.uniform(-0.2, 0.2); d = R_ + rng.uniform(0.5, 1.0) * kk
            q = stone(cx + d * math.cos(t), cy + d * math.sin(t), rng.uniform(0.55, 0.85) * kk, rng, 0.85, 7)
            if q.distance(pool) > 0.2 and all(q.distance(o) > 0.2 for o in out): out.append(q); lay.append(("mound", q, rng.uniform(0.45, 0.8), rim, SOFT_T))
        return _u([pool] + out).convex_hull.buffer(0.3), lay
    return f

def b_rune_ring(kind="runecut", crack="rift", R=(3.0, 3.8)):
    """a circle of the style's own runes cut into the ground between two rings, cracked across and broken where the
    crack runs; one rune in the middle"""
    def f(cx, cy, a, rng, k=1.0):
        Rr = rng.uniform(*R) * k; w = 0.38
        g = [_ann(cx, cy, Rr, Rr - w, 48), _ann(cx, cy, Rr * 0.62, Rr * 0.62 - w * 0.85, 40)]
        n = rng.randint(6, 8) + int(4 * (k - 1)); s_ = min(1.5, (Rr * 0.38 - 0.1) * 1.7) * 1.0
        for i in range(n):
            t = a + 6.283 * i / n; rm = Rr * 0.81; m = (cx + rm * math.cos(t), cy + rm * math.sin(t))
            g.append(rune(frame(m[0], m[1], t + math.pi / 2), rng.randrange(len(BLIGHT_RUNES)), s_, 0.3))
        g.append(rune(frame(cx, cy, a), rng.randrange(len(BLIGHT_RUNES)), Rr * 0.7, 0.36))
        cr = []
        for j in range(rng.randint(1, 2)):
            ca = a + rng.uniform(0, 6.283); L = Rr + 1.2
            cr.append(LineString(wiggle((cx - L * math.cos(ca) * rng.uniform(0.2, 1.0), cy - L * math.sin(ca) * rng.uniform(0.2, 1.0)), (cx + L * math.cos(ca), cy + L * math.sin(ca)), rng, 0.6, 6)).buffer(0.26, cap_style=1, resolution=3))
        cr = _u(cr); marks = _u(g).difference(cr.buffer(0.35))
        lay = [("gcut", marks, 0.35, kind), ("gcut", cr, 0.55, crack)]
        for i in range(rng.randint(0, 2)):          # a chip of the ring heaved up beside the crack
            p = cr.representative_point(); q = stone(p.x + rng.uniform(-1, 1), p.y + rng.uniform(-1, 1), rng.uniform(0.5, 0.7), rng, 0.7, 4)
            if q.distance(cr) > 0.2: lay.append(("mound", q, rng.uniform(0.4, 0.6), "blightrock", PEB_T))
        return _disc(cx, cy, Rr + 0.5, 32), lay
    return f

def b_torn_rift(kind="rift", lip="blightrock", L=(7.0, 9.5)):
    """a short rift torn in the ground: a jagged slot, wide in the middle and closing at the ends, its lips heaved
    up in broken blocks, a crack running on from each end"""
    def f(cx, cy, a, rng, k=1.0):
        Ln = rng.uniform(*L) * k; W = rng.uniform(1.2, 1.7) * max(1.0, k ** 0.6); ff = frame(cx, cy, a); m = 14
        us = [-Ln / 2 + Ln * i / m for i in range(m + 1)]
        top = [ff(u, W * max(0.0, 1 - (2 * u / Ln) ** 2) ** 0.7 * rng.uniform(0.6, 1.0) + rng.uniform(-0.15, 0.15)) for u in us]
        bot = [ff(u, -W * max(0.0, 1 - (2 * u / Ln) ** 2) ** 0.7 * rng.uniform(0.6, 1.0) + rng.uniform(-0.15, 0.15)) for u in us]
        slot = Polygon(top + bot[::-1]).buffer(0)
        if slot.is_empty or slot.area < 2: return None
        ends = _u([LineString(wiggle(ff(sx * Ln / 2 - sx * 0.3, 0), ff(sx * (Ln / 2 + rng.uniform(1.5, 3.0) * k), rng.uniform(-1, 1)), rng, 0.4, 3)).buffer(0.24, cap_style=1, resolution=3) for sx in (-1, 1)])
        lay = [("gcut", slot, rng.uniform(0.85, 1.0), kind), ("gcut", ends.difference(slot), 0.5, kind)]; foot = [slot, ends]
        for i in range(rng.randint(5, 8) + int(4 * (k - 1))):
            u = rng.uniform(-Ln * 0.4, Ln * 0.4); sv = rng.choice((-1, 1)); half = W * max(0.0, 1 - (2 * u / Ln) ** 2) ** 0.7
            q = stone(*ff(u, sv * (half + rng.uniform(0.9, 1.3))), rng.uniform(0.65, 0.95) * max(1.0, k ** 0.4), rng, rng.uniform(0.55, 0.8), rng.randint(4, 5))
            if q.distance(slot) > 0.25 and all(q.distance(o) > 0.25 for o in foot[2:]): foot.append(q); lay.append(("mound", q, rng.uniform(0.6, 1.1), lip, ROCK_T))
        return _u(foot).convex_hull.buffer(0.3), lay
    return f

def b_flesh_rock(kind="fleshrock", pock="blister"):
    """rock gone soft: three or four swollen lumps grown into one, a blister or two on them"""
    def f(cx, cy, a, rng, k=1.0):
        S = _S(); kk = max(1.0, k ** 0.6); sol = []; foot = []
        for i in range(rng.randint(3, 4)):
            t = rng.uniform(0, 6.283); d = 0 if i == 0 else rng.uniform(1.0, 1.6) * kk; r = rng.uniform(1.0, 1.4) * kk * (1.0 if i == 0 else 0.8); x, y = cx + d * math.cos(t), cy + d * math.sin(t)
            h = rng.uniform(1.0, 1.5) * (1.0 if i == 0 else 0.75)
            sol += S.lathe(x, y, S.curve(r, 0.25, 0.0, h, 7), 16, rng.uniform(0.75, 0.95), t); foot.append(Point(x, y).buffer(r))
        lay = [S._steps(sol, kind)]
        for i in range(rng.randint(1, 2)):
            t = rng.uniform(0, 6.283); d = rng.uniform(1.9, 2.6) * kk; x, y = cx + d * math.cos(t), cy + d * math.sin(t)
            q = Point(x, y).buffer(rng.uniform(0.45, 0.6), 12)
            if q.distance(_u(foot)) > 0.2: foot.append(q); lay.append(("mound", q, rng.uniform(0.45, 0.6), pock, SOFT_T))
        return _u(foot).convex_hull.buffer(0.2), lay
    return f


def L_thorns(kind="thorn"):
    def f(cx, cy, a, rng, k):
        return b_thorn_cluster(kind, (6, 9))(cx, cy, a, rng, 1.8 * max(1.0, k ** 0.6))
    return f

def L_rift(kind="rift", lip="blightrock"):
    def f(cx, cy, a, rng, k):
        return b_torn_rift(kind, lip, (13.0, 17.0))(cx, cy, a, rng, k)
    return f

def L_rune_ring(kind="runecut", crack="rift"):
    def f(cx, cy, a, rng, k):
        return b_rune_ring(kind, crack, (5.5, 7.0))(cx, cy, a, rng, k)
    return f

def L_boil(water="ichor", rim="blister"):
    def f(cx, cy, a, rng, k):
        return b_boiling_pool(water, rim)(cx, cy, a, rng, 1.7 * k)
    return f

def L_bones(kind="bone"):
    def f(cx, cy, a, rng, k):
        return b_bone_heap(kind)(cx, cy, a, rng, 1.6 * k)
    return f


def feature_tables(F):
    _take(F)
    C = comp
    feats = [_cls(C(b_thorn_cluster("thorn")), "debris"), _cls(C(b_bone_heap("bone")), "bones"), _cls(C(b_boiling_pool("ichor", "blister"), ground=True), "marks"),
             _cls(C(b_rune_ring("runecut", "rift"), ground=True), "marks"), _cls(C(b_torn_rift("rift", "blightrock"), ground=True), "marks"),
             _cls(C(b_flesh_rock("fleshrock", "blister")), "growth"),
             C(b_skull("bone", 1.2)), C(b_ribs("bone")), C(b_crater("blightrock", "rift")),
             C(b_tentacle("tendril")), C(b_brazier("blightrock", "fire")), C(b_cracked_slab("runestone"))]
    lms = [(60, lm(L_thorns("thorn"))), (60, lm(L_rift("rift", "blightrock"), 0.35, ground=True)), (60, lm(L_rune_ring("runecut", "rift"), ground=True)),
           (100, lm(L_boil("ichor", "blister"), ground=True)), (100, lm(L_skeleton("bone"))), (130, lm(L_bones("bone")))]
    return dict(features=feats, landmarks=lms)


def ground_big(E, B, P, scree, pebbles):
    """big bases at figure scale: low fleshy swellings over part of the ground (not a field of them: the ring round
    the miniature is for the style's own things), torn holes with thorns thrown round them, ichor pools, broken stone"""
    return dict(field=("fleshrock", 0.3, 1.2, 0.3), rocks=(3, 0.5, 0.9, 0.3, 0.6, "blightrock"),
                feats=[("crater", 2, dict(rim="blightrock", pit="rift", rim_h=(0.5, 1.0), ejecta=(6, 0.4, 0.8, 0.3, 0.6, "thorn"))),          # (no gully: its offset banks can fail)
                       ("pool", 2, dict(kind="ichor", rim="blister", ripples=2)),
                       ("rubble", 1, dict(soft=P, stone="blightrock")),
                       ("cracked_flat", 1, dict(kind="rift"))])
