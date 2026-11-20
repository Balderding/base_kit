"""Sundered City: a gothic city shelled to rubble. The squares and streets of a great stone city after the guns: the
flagstones heaved and split, shell craters in the paving, the cathedral walls and arcades come down in drums,
voussoirs and window frames, statues toppled off their plinths, bent rebar from the newer floors, sandbags and
barricades where the fighting went on. Generic gothic ruins only: no emblems of any kind.

Everything a style is (see newstyles.py): its name and palette, its five themes, the ground layout for each base,
the tier features and the detail pool, its pool features and landmarks, and the ground of big bases at figure scale.
Its style-only details are in sd_r_x_sundered.py, their real sizes in scale/real_sundered.json."""
import math

NAME, TITLE = "sundered", "Sundered City"
SAYS = "a gothic stone city shelled to rubble: cracked flagstones, broken fluted columns, fallen statues, shell craters"
GROUND = "#5a5249"          # dust and ash over the paving
PAL = dict(wood="wood", iron="iron", rock="rubble", stone="ashlar", crate="crate", water="water", pit="pothole", floor="flag", plant=None, soft="spoil", bone="bone")
PATH_OK = "way stones ruts cable pipe"
EXTRA_PROPS = "sandbags casings rifle oildrum grate cables"
BUILT, ENCLOSED, CLIFFS = True, False, False
COLOURS = {"ashlar": "#a6a196", "statue": "#c4bfb2", "spoil": "#77716a", "rubble": "#8d877e", "rust": "#7a4a32", "brass": "#b8923c"}


def T(name, subject, path, props, also="", **kw):          # themes.T, kept here so this file never imports themes.py (which loads the styles)
    return dict(theme=name, subject=subject, path=path, props=props, also=also, **kw)

THEMES = [
    T("fallen cathedral wall", "wall", "way:past", "front:rubble*3 at:drum*2 path:slab back:stub front:tiles", also="statue:far", clutter=dict(debris=1.8), ban=("crate",)),
    T("statue toppled in the square", "statue", "way:to", "at:rubble*2 path:slab front:tiles*2 at:drum back:stub", also="crater:far", clutter=dict(debris=1.4), ban=("crate",)),
    T("shell crater in the street", "crater", "way:past", "at:rubble*3 at:slab*2 path:casings far:tiles back:stub", also="wall:far", pal=dict(pit="pothole"), clutter=dict(debris=2.0)),
    T("collapsed arcade", "colonnade", "way:past", "path:drum*2 at:rubble*2 at:slab front:stub far:tiles", also="crater:far", clutter=dict(debris=1.6), ban=("crate",)),
    T("barricaded doorway", "portal", "way:to", "front:sandbags*2 wall:crates path:casings at:beam front:rifle at:rubble*2", also="crater:far", clutter=dict(debris=1.4)),
]


# ------------------------------------------------------------------------------------------------ ground
def su_crater(s, rng, cx, cy, r, depth=0.75):
    """a shell crater in the paving: a scooped hole, a lip of broken stone thrown round it, cracks running out"""
    hole = stone(cx, cy, r, rng, rng.uniform(0.8, 0.95), 10); lip, cr = [], []
    for k in range(rng.randint(7, 11)):
        t = rng.uniform(0, 6.283); dd = r + rng.uniform(0.35, 1.0)
        lip.append(stone(cx + dd * math.cos(t), cy + dd * math.sin(t), rng.uniform(0.55, 0.95), rng, 0.8, 5))
    for k in range(rng.randint(4, 6)):
        t = rng.uniform(0, 6.283); L = rng.uniform(2.0, 4.0)
        cr.append(LineString(wiggle((cx + r * 0.8 * math.cos(t), cy + r * 0.8 * math.sin(t)), (cx + (r + L) * math.cos(t), cy + (r + L) * math.sin(t)), rng, 0.3)).buffer(0.22, cap_style=1, resolution=3))
    rocks = [g for g in polys_of(unary_union(lip)) if Point(0, 0).buffer(R0 - 0.3).contains(g)]
    s.stack(rocks, rng.uniform(0.55, 0.85), ((0.65, 8), (0.35, 40)), "rubble")
    s.gcut([hole], depth, "pothole"); s.gcut([unary_union(cr)], 0.4, "crack")
    return hole

def su_lifted(s, rng, sl, n):
    """flagstones blown out of a floor: square holes cut down through the slab, cracks across it"""
    if sl.is_empty: return
    reg = inset(sl, 1.2); x0, y0, x1, y1 = sl.bounds; got = 0
    for _ in range(n * 20):
        if got >= n or reg.is_empty: break
        c = (rng.uniform(x0, x1), rng.uniform(y0, y1)); g = rect(c[0], c[1], rng.uniform(2.4, 3.6), rng.uniform(2.0, 3.0), rng.uniform(0, 3.14))
        if reg.contains(g): s.tcut([g], H + 0.12, H + 0.6, "pothole"); got += 1
    cs = []
    for _ in range(max(2, round(2 * KK()))):
        a_ = rp(rng, 0, 11); t = rng.uniform(0, 6.283); L = rng.uniform(6, 14); b_ = (a_[0] + L * math.cos(t), a_[1] + L * math.sin(t))
        cs.append(LineString(wiggle(a_, b_, rng, 0.6, 6)).buffer(0.22, cap_style=1, resolution=3))
    q = unary_union(cs).intersection(inset(sl, 0.5))
    if not q.is_empty: s.tcut([q], H + 0.2, H + 0.6, "crack")

def layout(n):
    """the ground: a flagged square, a street with its kerb, shell-torn paving or a rubble field (four kinds)"""
    s = gg2.mk(NAME, TITLE, n, GROUND); rng = s.rng; v = rng.randint(0, 3); D = _D(); phi, f, R = _fr(rng)
    if v == 0:            # the cathedral square: big flags laid in courses, some blown out, cracks across
        sl = floor(s, D, 0.55, "flag", j_planks(phi, rng.uniform(3.4, 4.2), rng.uniform(5.0, 6.5), 0.24))
        su_lifted(s, rng, sl, rng.randint(1, 3))
    elif v == 1:          # a street: the flagged pavement on one side behind its kerb, the roadway torn up
        c = rng.uniform(-3, 2) * gg.K
        sl = floor(s, R(-30, 30, c + 1.0, 30).intersection(D), 0.5, "flag", j_grid(phi, rng.uniform(3.0, 3.6), rng.uniform(4.2, 5.0)))
        kb = s.prism([R(-30, 30, c - 0.1, c + 1.1).intersection(D)], H + 0.8, "ashlar")
        if kb: s.tcut([j_grid(phi, 30, rng.uniform(3.4, 4.4)).intersection(R(-30, 30, c - 0.5, c + 1.5)).intersection(Point(0, 0).buffer(R0 - 0.6))], H + 0.5, H + 0.8, "joint")
        su_lifted(s, rng, sl, 1)
        ln = meander(rng, phi, c - rng.uniform(3.5, 5.5), 0.8, 1.0); s.gcut([ln.buffer(0.5, cap_style=2)], 0.35, "rut")
        cracks(s, rng, rng.randint(2, 3), 0.4, "crack")
    elif v == 2:          # shell-torn paving: craters through broken flags
        reg = R(-30, 30, rng.uniform(-2, 4) * gg.K, 30).intersection(D)
        sl = floor(s, reg, 0.5, "flag", j_voronoi(rng, rng.randint(16, 24), 0.24, 0.25))
        for _ in range(NN(rng.randint(1, 2))):
            for t_ in range(40):
                cx, cy = rp(rng, 0, 8); r = rng.uniform(1.9, 2.7)
                if Point(0, 0).buffer(R0 - 1.5).contains(Point(cx, cy).buffer(r + 1.2)) and s.free(Point(cx, cy).buffer(r + 1.5), 0.3): su_crater(s, rng, cx, cy, r); break
        cracks(s, rng, 2, 0.4, "crack")
    else:                 # a rubble field: spoil heaps drifted over the broken edge of the paving
        reg = R(-30, 30, rng.uniform(-1, 4) * gg.K, 30).intersection(D)
        sl = floor(s, reg, 0.5, "flag", j_voronoi(rng, rng.randint(18, 26), 0.22, 0.2))
        banks(s, rng, rng.randint(2, 3), 1.8, 2.8, (0.5, 0.9), "spoil", ((0.55, 20), (0.45, 45)), region=Point(0, 0).buffer(R0 - 0.8).difference(reg.buffer(0.5)))
        cracks(s, rng, 2, 0.4, "crack")
    for p in s.scatter(rng.randint(2, 4), 0.7, 1.2, gap=0.5, round_=False): s.stack([p], rng.uniform(0.45, 0.8), ROCKY, "rubble")
    grit(s, rng, 8, 16); return s


# ------------------------------------------------------------------------------------------------ tier features
def _tag(fn, tmpl, kind, shape, cls):
    fn.tmpl, fn.kind, fn.shape, fn.cls = tmpl, kind, shape, cls; return fn

def sd_blocks(s, rng, lvl):          # dressed blocks fallen out of a wall, with their bed joints showing
    def at(cx, cy):
        return [rect(cx + rng.uniform(-2.2, 2.2), cy + rng.uniform(-1.6, 1.6), rng.uniform(2.0, 2.6), rng.uniform(1.2, 1.5), rng.uniform(0, math.pi)) for _ in range(rng.randint(2, 3))]
    ps, z = place_group(s, rng, lambda cx, cy: [g for g in polys_of(unary_union(at(cx, cy)))], gap=0.35)
    for p in ps or []:
        top = up(s, [p], z, rng.uniform(0.7, 1.0), "ashlar")
        if top: c = p.centroid; topcut(s, [rect(c.x, c.y, 0.25, 6, rng.uniform(0, math.pi)).intersection(inset(p, 0.3))], top, 0.18, "slot")

def sd_flags(s, rng, lvl):           # a patch of flags heaved up by a near miss, standing proud at odd heights
    def at(cx, cy):
        a = rng.uniform(0, math.pi); f = frame(cx, cy, a)
        return [rect(*f(i * 2.6, j * 2.6), 2.25, 2.25, a + rng.uniform(-0.12, 0.12)) for i in range(-1, 2) for j in range(-1, 1) if rng.random() > 0.3]
    ps, z = place_group(s, rng, at, gap=0.4)
    for p in ps or []: up(s, [p], z, rng.uniform(0.25, 0.6), "ashlar")

def sd_wallstub(s, rng, lvl):        # a stub of wall still standing: two courses of ashlar with a window jamb at one end
    def at(cx, cy):
        a = rng.uniform(0, math.pi); f = frame(cx, cy, a); L = rng.uniform(6.0, 8.0)
        return [rect(*f(0, 0), L, 1.8, a), rect(*f(L / 2 + 0.8, 0), 1.2, 1.8, a)]
    ps, z = place_group(s, rng, at, gap=0.4)
    if not ps: return
    top = up(s, [ps[0]], z, rng.uniform(1.2, 1.8), "ashlar")
    if top:
        mr = ps[0].minimum_rotated_rectangle.exterior.coords; a = math.atan2(mr[1][1] - mr[0][1], mr[1][0] - mr[0][0]); c = ps[0].centroid; f = frame(c.x, c.y, a)
        topcut(s, [unary_union([rect(*f(u, 0), 0.25, 4, a) for u in np.arange(-3.5, 3.6, 2.2)]).intersection(inset(ps[0], 0.25))], top, 0.2, "slot")
    if len(ps) > 1: up(s, [ps[1]], z, rng.uniform(2.0, 2.6), "ashlar")

_tag(sd_blocks, "scatter", "ashlar", "rect", "debris"); _tag(sd_flags, "scatter", "ashlar", "rect", "debris"); _tag(sd_wallstub, "single", "ashlar", "rect", "made")


def _deb(fn): fn.cls = "debris"; return fn

def tables(G):
    """the tier features (detail levels 2, 3 and 4) and the 30-piece detail pool"""
    globals().update({k: v for k, v in G.items() if not k.startswith("__") and k not in _OWN})
    tiers = ([sd_blocks, G["u_bullets"], scatter(sh_tri(0.7, 1.1), (4, 7), (0.45, 0.75), "rubble", "mound", 1.8, ROCKY)],
             [G["f_column"]("ashlar"), sd_flags, G["u_rebar"]],
             [G["u_crater"], G["f_sandbags"], sd_wallstub])
    more = [_deb(scatter(sh_rect(1.0, 1.4, 0.8, 1.0), (3, 5), (0.5, 0.8), "ashlar", "up", 1.8)), _deb(single(sh_rect(2.2, 2.8, 1.3, 1.6), (0.8, 1.1), "ashlar", "up", pat_seams(0.9))),
            _deb(single(sh_disc(1.2, 1.5, 24), (0.7, 1.0), "ashlar", "up", pat_ring(0.25))), _deb(scatter(sh_caps(2.0, 3.0, 0.5), (2, 4), (0.35, 0.45), "rust", "up", 1.8)),
            scatter(sh_caps(0.8, 1.1, 0.35), (4, 7), (0.3, 0.35), "brass", "up", 1.4), sunk(sh_wig(5, 8, 0.45, 0.9), (0.4, 0.5), "crack"),
            sunk(sh_disc(0.45, 0.6, 10), (0.35, 0.45), "pothole", (4, 7), 1.5), sunk(sh_blob(1.4, 2.0), (0.2, 0.3), "scorch"), sunk(sh_blob(1.2, 1.7), (0.3, 0.4), "water"),
            scatter(sh_round(0.5, 0.75), (6, 10), (0.35, 0.5), "rubble", "mound", 1.6, ((0.7, 10), (0.3, 45))), single(sh_blob(1.8, 2.4), (0.7, 1.0), "spoil", "mound", tiers=((0.55, 20), (0.45, 45))),
            _deb(single(sh_hex(1.2, 1.5), (0.5, 0.7), "ashlar", "up", pat_seams(0.7))), sunk(sh_star(1.2, 1.6, 5, 6), (0.3, 0.4), "crack")]
    pool = [x for t in tiers for x in t] + G["_nature"]("rubble", "pebble", False, "water", None)[:7] + G["_gear"]("crate", "wood", "iron")[:4] + more
    fill = [sunk(sh_crescent(1.0, 1.4), (0.3, 0.4), "pothole"), scatter(sh_ell(0.6, 0.8, 0.9, 1.2), (3, 5), (0.5, 0.7), "rubble", "mound", 1.6, ROCKY)]
    k = 0
    while len(pool) < 30: pool.append(fill[k % len(fill)]); k += 1
    return dict(tiers=tiers, details=pool)


def feature_tables(F):
    """pool features (the kit's own builders in this style's stone) and landmarks with the smallest base for each"""
    C, lm = F["comp"], F["lm"]
    feats = [C(F["b_fallen_column"]("ashlar")), C(F["b_cracked_slab"]("ashlar")), C(F["b_tiles"]("flag", 3, 2, 1.5)), C(F["b_stairs"]("ashlar")), C(F["b_crater"]("rubble", "pothole")),
             C(F["b_grate"]("iron")), C(F["b_tool"]("rifle")), C(F["b_pile"]("wood", (4.2, 5.4), 1.0)), C(F["b_boss"]("iron", "iron", cross=True)), C(F["b_brazier"]("iron", "fire"))]
    lms = [(60, lm(F["L_wall"]("ashlar"))), (60, lm(F["L_crater"]("rubble", "pothole"))), (60, lm(F["L_colonnade"]("ashlar", "flag"))),
           (100, lm(F["L_ruin"]("ashlar", "flag"))), (100, lm(F["L_dais"]("flag", "column"))), (130, lm(F["L_hull"]("iron", "iron")))]
    return dict(features=feats, landmarks=lms)


def ground_big(E, B, P, scree, pebbles):
    """big bases at figure scale: rubble heaps and spoil drifts, tilted slabs, shell craters with thrown stone;
    where the paving covers most of the ground, only slabs and rubble lying on it"""
    crat = ("crater", 2, dict(rim=B, pit="pothole", ejecta=(8, 0.5, 1.0, 0.3, 0.7, "rubble")))
    return dict(field=(P, 0.2, 1.0, 0.45), rocks=(4, 0.5, 1.0, 0.3, 0.7, "rubble"),
                feats=[("rubble", 3, dict(soft=P, stone="ashlar", big=(1.0, 1.8))), ("slabs", 2, dict(kind="ashlar", h=(0.3, 1.2))), crat,
                       ("ridge", 1, dict(kind=P, h=(0.5, 1.2), w=(2.4, 3.6)))],
                floored=dict(field=None, feats=[("slabs", 3, dict(kind="flag", h=(0.2, 0.8), tilt=False)), ("rubble", 2, dict(soft=P, stone="ashlar"))]))


_OWN = set(globals()) | {"_OWN"}
