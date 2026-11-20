"""Ferrum Works: a red, rusted factory world. Riveted deck plates laid on red iron dust, gear wheels half sunk in it,
cable bundles, vent stacks, pipe runs, broken machinery and heaps of black slag. Generic industrial: no maker, no
emblem. One file holds the whole style (see newstyles.py); its own details are in sd_r_x_ferrum.py and their real
sizes in scale/real_ferrum.json.

The ground (layout): four kinds, each drawn afresh on every base
  0  deck yard     riveted plates over part of the base, the rest bare dust with slag lumps and an oil pool
  1  full deck     riveted plates all over, a few plates gone (dust shows), a cable trench across
  2  dust flats    bare red dust: drifts, slag lumps, oil puddles, cracks
  3  slag crust    a crust of black slag cracked into plates, dust banked between, a channel of oil"""
import math


def T(name, subject, path, props, also="", **kw):          # the same as themes.T (not imported: themes.py loads this file)
    return dict(theme=name, subject=subject, path=path, props=props, also=also, **kw)

NAME = "ferrum"
TITLE = "Ferrum Works"
SAYS = ("a red, rusted factory world: riveted iron deck plates laid on red iron dust, gear wheels half sunk in it, "
        "cable bundles, vent stacks, pipe runs, broken machinery and heaps of black slag")
GROUND = "#8a3f26"
PAL = dict(wood="rust", iron="iron", rock="slag", stone="girder", crate="rust", water="tar", pit="pit", gem=None, floor="plate",
           plant=None, soft="dust", bone="bone", twig="cable", vein="cable")
PATH_OK = "way cable pipe track ruts trail"
EXTRA_PROPS = "oildrum tank gear grate wrench vent cables casings"
BUILT, ENCLOSED, CLIFFS = True, False, True
COLOURS = {"slag": "#3d3431", "girder": "#5e3526", "dust": "#9c4a2c", "oxide": "#b35a2c"}

THEMES = [
    T("gear pit", "crater", "cable:to", "at:gear*2 in:gear at:rubble*2 front:wrench back:oildrum far:crack", also="machine:far",
      clutter=dict(debris=1.5)),
    T("cable run", "wall", "cable:past", "wall:cables*2 at:grate path:cables front:lid back:oildrum*2 at:wrench", also="depot:far"),
    T("vent stack", "machine", "pipe:to", "at:vent*2 at:grate back:tank at:cables front:lid", also="shaft:far"),
    T("fallen work-machine", "wagon", "ruts:to", "at:gear*2 at:wheel path:wrench at:cables back:rubble*2 far:oildrum", also="crater:far",
      clutter=dict(debris=1.4)),
    T("slag heap", "spires", "track:past", "path:cart at:rubble*3 at:rocks wall:oildrum far:puddle", pal=dict(gem="slag"), clutter=dict(debris=1.8),
      also="depot:far"),
]


# ------------------------------------------------------------------------------------------------ the ground
def _rivets(phi, sx, sy, R, inset_=0.75):
    """rivet heads (cut as small dimples) at the corners and mid edges of every plate of an sx x sy grid laid out
    like styles2.j_grid (its lines start at -22 x KK), so they sit just inside the joints"""
    from shapely.geometry import Point
    from shapely.ops import unary_union
    from shapely import affinity
    import styles2 as S2
    E0 = 22 * S2.KK(); pts = []
    i0 = max(0, int((E0 - R - sx) / sx)); j0 = max(0, int((E0 - R - sy) / sy))
    for i in range(i0, int((E0 + R) / sx) + 2):
        for j in range(j0, int((E0 + R) / sy) + 2):
            x, y = -E0 + i * sx, -E0 + j * sy
            for u, v in ((inset_, inset_), (sx - inset_, inset_), (inset_, sy - inset_), (sx - inset_, sy - inset_), (sx / 2, inset_), (sx / 2, sy - inset_)):
                px, py = x + u, y + v
                if px * px + py * py < (R + 1) ** 2: pts.append(Point(px, py).buffer(0.24, 6))
    return affinity.rotate(unary_union(pts), math.degrees(phi), origin=(0, 0)) if pts else None


def _grid(phi, sx, sy, R):
    import styles2 as S2
    j = S2.j_grid(phi, sx, sy, 0.22); r = _rivets(phi, sx, sy, R)
    return j if r is None else j.union(r)


def layout(n):
    import gg, gg2, styles2 as S2
    from shapely.geometry import Point
    from shapely.ops import unary_union
    s = gg2.mk(NAME, TITLE, n, GROUND); rng = s.rng; v = rng.choices((0, 1, 2, 3), (35, 30, 15, 20))[0]; D = S2._D(); phi, f, R = S2._fr(rng)          # deck plates on most bases: they are what the place is
    sx, sy = rng.uniform(5.5, 6.5), rng.uniform(3.6, 4.4); Rb = gg.R0 + 2
    if v == 0:            # deck yard: plates over part of the base, dust beside
        c = rng.uniform(-3, 2) * gg.K
        reg = R(-30, 30, c, 30).intersection(D)
        S2.floor(s, reg, 0.5, "plate", _grid(phi, sx, sy, Rb), depth=0.32)
        S2.pools(s, rng, rng.randint(1, 2), 1.4, 2.2, 0.4, "tar", avoid=reg.buffer(1.0))
        S2.banks(s, rng, rng.randint(1, 2), 1.4, 2.2, (0.4, 0.7), "dust", ((0.5, 25), (0.5, 50)), region=Point(0, 0).buffer(gg.R0 - 0.8).difference(reg.buffer(0.6)))
    elif v == 1:          # full deck with a few plates gone and a cable trench
        from shapely.geometry import Polygon
        E0 = 22 * S2.KK(); cells = []
        for _k in range(rng.randint(1, 3) + (gg.K > 1.5)):
            x0 = -E0 + sx * round((E0 + rng.uniform(-0.7, 0.7) * gg.R0) / sx); y0 = -E0 + sy * round((E0 + rng.uniform(-0.7, 0.7) * gg.R0) / sy)
            cells.append(Polygon([f(x0, y0), f(x0 + sx, y0), f(x0 + sx, y0 + sy), f(x0, y0 + sy)]))
        gone = unary_union(cells).buffer(0.25, join_style=2)          # a plate gone: the dust under it shows
        reg = D.difference(gone)
        c = rng.uniform(-4, 4) * gg.K; tr = R(-30, 30, c - 0.6, c + 0.6).intersection(D)
        S2.floor(s, reg.difference(tr.buffer(0.3)), 0.5, "plate", _grid(phi, sx, sy, Rb), depth=0.32)
        s.gcut([tr], 0.4, "slot")
    elif v == 2:          # bare dust flats
        S2.banks(s, rng, rng.randint(2, 4), 1.6, 2.8, (0.4, 0.8), "dust", ((0.5, 25), (0.5, 50)))
        S2.pools(s, rng, rng.randint(1, 3), 1.2, 2.0, 0.35, "tar")
        S2.cracks(s, rng, rng.randint(1, 3), 0.35, "crack")
    else:                 # slag crust cracked into plates, an oil channel beside it
        c = rng.uniform(-4, 1) * gg.K
        reg = R(-30, 30, c, 30).intersection(D)
        S2.floor(s, reg, 0.45, "basalt", S2.j_voronoi(rng, rng.randint(12, 18), 0.3, 0.5), depth=0.4)
        s.gcut([R(-30, 30, c - 2.2, c - 0.7).intersection(D)], 0.4, "tar")
    for p in s.scatter(rng.randint(2, 4), 0.9, 1.6, gap=0.5, round_=False): s.stack([p], rng.uniform(0.6, 1.1), S2.ROCKY, "slag")
    S2.grit(s, rng, 10, 20); return s


# ------------------------------------------------------------------------------------------------ tier features and the detail pool
def _gear_poly(cx, cy, R, n, a, td=0.6):
    from shapely.geometry import Point
    from shapely.ops import unary_union
    import gg3
    tw = max(0.55, min(0.85, 3.1416 * R / n))
    teeth = [gg3.rect(cx + (R - td * 0.45) * math.cos(a + 6.2832 * k / n), cy + (R - td * 0.45) * math.sin(a + 6.2832 * k / n), td * 1.1, tw, a + 6.2832 * k / n) for k in range(n)]
    return unary_union([Point(cx, cy).buffer(R - td * 0.75, 3 * n)] + teeth)


def b_sunk_gear(kind="iron", R=(1.8, 2.5)):
    """a gear wheel lying flat, half under a bank of red dust: rim and teeth, the web ring, the hub hole"""
    def f(cx, cy, a, rng):
        from shapely.geometry import Point
        Rr = rng.uniform(*R); n = rng.randint(11, 15); g = _gear_poly(cx, cy, Rr, n, a)
        t = a + rng.uniform(0, 6.28); dc = (cx + Rr * 0.65 * math.cos(t), cy + Rr * 0.65 * math.sin(t))
        dust = Point(*dc).buffer(Rr * 0.85, 16)
        ring = Point(cx, cy).buffer(Rr - 0.9, 24).difference(Point(cx, cy).buffer(Rr - 1.2, 24))
        return Point(cx, cy).buffer(Rr + 0.3).union(dust), [("up", g, rng.uniform(0.5, 0.65), kind), ("tcut", ring.union(Point(cx, cy).buffer(0.45, 10)), 0, 0.3),
                                                            ("mound", dust, rng.uniform(0.9, 1.2), "dust", ((0.55, 25), (0.45, 50)))]
    return f


def b_vent_cowl(kind="iron"):
    """a short round vent cowl on its flange, the mouth open and sooted"""
    def f(cx, cy, a, rng):
        from shapely.geometry import Point
        r = rng.uniform(0.9, 1.2)
        return Point(cx, cy).buffer(r + 0.5), [("up", Point(cx, cy).buffer(r + 0.4, 20), 0.35, kind), ("on", Point(cx, cy).buffer(r, 20), 0, rng.uniform(1.6, 2.4), kind),
                                               ("tcut", Point(cx, cy).buffer(r - 0.42, 16), 1, 0.6)]
    return f


def b_pipe_stub(kind="pipe"):
    """a short pipe lying on two blocks, its flange at one end"""
    def f(cx, cy, a, rng):
        import gg3
        L = rng.uniform(4.5, 6.0); w = rng.uniform(1.0, 1.3); ff = gg3.frame(cx, cy, a)
        pipe = gg3.rect(cx, cy, L, w, a); fl = gg3.rect(*ff(L / 2 - 0.3, 0), 0.5, w + 0.5, a); blocks = [gg3.rect(*ff(x, 0), 0.9, w + 0.7, a) for x in (-L * 0.3, L * 0.25)]
        from shapely.ops import unary_union
        return unary_union([pipe, fl] + blocks).convex_hull, [("up", unary_union(blocks), 0.55, "iron"), ("up", pipe, w * 0.9, kind), ("up", fl, w * 0.9 + 0.15, kind)]
    return f


def _tag(f, kind, tag): f.kind, f.tag = kind, tag; return f


def tables(G):
    import features as F
    sc, si, su, pa, tr = G["scatter"], G["single"], G["sunk"], G["path"], G["trail"]
    C = F.comp; ROCKY, SOFTT = G["ROCKY"], G["SOFTT"]
    gear = C(_tag(b_sunk_gear("iron"), "iron", "sunk_gear")); cowl = C(_tag(b_vent_cowl("iron"), "iron", "vent_cowl")); pipe = C(_tag(b_pipe_stub("pipe"), "pipe", "pipe_stub"))
    cables = pa(0.6, (7, 11), 0.45, "cable", "up", 0.8)
    slag = sc(G["sh_rock"](0.8, 1.3), (3, 5), (0.6, 1.0), "slag", "mound", 1.6, ROCKY)
    t2 = [gear, cables, slag]
    t3 = [cowl, pipe, si(G["sh_rect"](2.6, 3.4, 1.8, 2.3), (0.3, 0.4), "rust", "up", G["pat_dots"](0.22)), su(G["sh_blob"](1.3, 1.9), (0.3, 0.4), "tar")]
    t4 = [G["f_column"]("girder"), G["f_vent"]("slot", "iron"), G["f_post_row"]("girder", (1.6, 2.2), True, (2, 2), 4.5),
          sc(G["sh_caps"](0.7, 1.0, 0.3), (5, 8), (0.3, 0.35), "iron", "up", 1.4)]
    N = G["_nature"]("slag", "slag", False, "tar", None)
    Gg = G["_gear"]("rust", "rust", "iron")
    more = [sc(G["sh_hex"](0.5, 0.7), (4, 7), (0.3, 0.4), "iron", "up", 1.4), single_ring(G), si(G["sh_blob"](1.6, 2.2), (0.6, 0.9), "dust", "mound", tiers=SOFTT), su(G["sh_rect"](2.0, 2.8, 1.2, 1.6), (0.35, 0.45), "slot"),
            pa(0.9, (6, 9), 0.5, "pipe", "up", 0.15), si(G["sh_disc"](1.4, 1.8, 24), (0.4, 0.5), "iron", "up", G["pat_slots"](4, 0.3)),
            tr(1.2, (5, 8), G["sh_disc"](0.3, 0.4, 8), 0.3, "pit"), su(G["sh_wig"](4, 7, 0.4, 0.8), (0.35, 0.45), "crack")]
    pool = [x for t in (t2, t3, t4) for x in t] + N + Gg[:6] + more
    while len(pool) < 30: pool.append(more[len(pool) % len(more)])
    return dict(tiers=(t2, t3, t4), details=pool)


def single_ring(G):
    """a loose flange ring lying in the dust"""
    return G["single"](G["sh_ring"](1.3, 1.7, 0.55), (0.4, 0.55), "iron", "up")


def feature_tables(F):
    C, lm = F["comp"], F["lm"]
    return dict(features=[C(F["b_gear"]("iron")), C(F["b_gear"]("rust", (1.3, 1.8))), C(F["b_grate"]("iron", True)), C(F["b_pile"]("pipe", (3.6, 4.8), 0.9)),
                          C(F["b_hoops"]("rust", (2.8, 3.4), 1.8)), C(F["b_track"]("iron", "rust", (6.5, 9.0))), C(F["b_boss"]("iron", "iron", cross=True)),
                          C(F["b_cart"]("rust", "iron")), C(F["b_tiles"]("plate", 2, 2, 1.7)), C(F["b_ladder"]("rust")), C(F["b_tool"]("wrench", "iron")),
                          C(_tag(b_sunk_gear("rust", (2.2, 3.0)), "rust", "sunk_gear"))],
                landmarks=[(60, lm(F["L_track"]("iron", "rust"), 0.35)), (60, lm(F["L_wall"]("girder", "pipe"))), (60, lm(F["L_crater"]("slag", "tar"))),
                           (100, lm(F["L_hull"]("rust", "rust"))), (100, lm(F["L_spires"]("slag"))), (130, lm(F["L_ruin"]("girder", "plate")))])


def ground_big(E, B, P, scree, pebbles):
    return dict(field=("dust", 0.3, 1.5, 0.55), rocks=(3, 0.5, 1.0, 0.4, 0.8, "slag"),
                feats=[("rubble", 2, dict(soft=P, stone="slag", h=(0.5, 1.3), big=(0.6, 1.3))), ("slabs", 3, dict(kind="plate", h=(0.3, 1.3), r=(0.15, 0.25), n=(5, 9))),
                       ("pool", 2, dict(kind="tar", rim="dust", r=(0.05, 0.09))), ("trench", 1, dict(bed="slot", wall="girder", pipe="pipe")),
                       ("outcrop", 1, dict(kind="slag", h=(1.2, 2.6), soft="dust")), ("ridge", 2, dict(kind="dust", h=(0.5, 1.4), w=(2.4, 3.8)))],
                floored=dict(field=None, feats=[("slabs", 3, dict(kind="plate", h=(0.2, 0.9), tilt=False)), ("rubble", 2, dict(soft=P, stone="slag")),
                                                ("trench", 1, dict(bed="slot", wall="girder", pipe="pipe"))]))
