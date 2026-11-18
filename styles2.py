"""Twelve more themes. Seven are Shadows of Brimstone worlds, five are general tabletop themes.

  swamp      Swamps of Jargono: mud, pools, roots, reeds, logs, serpent-temple stones
  cynder     Caverns of Cynder: cracked basalt, lava channels, obsidian, vents
  targa      Targa Plateau: frozen alien city: ice sheets, snow drifts, alien floor panels
  trederra   Trederra: trench war: duckboards, sandbags, shell holes, wire
  fortress   Forbidden Fortress: feudal Japan: plank walkways, tatami, garden stones, lanterns
  beast      Belly of the Beast: flesh folds, veins, bile pools, teeth
  frontier   Old West frontier town: boardwalk, wagon ruts, barrels, hitching posts
  graveyard  headstones, coffin lids, disturbed earth, iron fence
  forest     roots, stumps, mushrooms, leaf litter, moss rocks
  dungeon    flagstones, drains, chains, trapdoors, bones
  temple     ancient temple: big flags, broken columns, steps, carved glyphs
  coast      rock shelves, tide pools, shells, driftwood, rope

Each theme = a ground layout (several random variants) + features added per detail level + a 30-piece detail library.
Helper functions come from gg3 (install) and details.py."""
import math

def install(G):
    globals().update({k: v for k, v in G.items() if not k.startswith("__") and k not in ("install", "STYLES2", "TIERS2", "DETAILS2", "TITLES")})
    import details as D
    globals().update({k: getattr(D, k) for k in dir(D) if k.startswith(("sh_", "pat_")) or k in ("scatter", "single", "sunk", "trail", "path", "ROCKY", "SOFTT")})
    gg.COL.update(COLS); gg.CUTC.update(CUTS)
    _build_tables()


COLS = {"mud": "#5b4a33", "root": "#4a3622", "reed": "#6f8a3a", "lily": "#4f8a4a", "log": "#6b4a2b", "basalt": "#3b3534", "obsidian": "#1c1a22",
        "ember": "#c8501e", "ice": "#bfe3ee", "snow": "#eef4f7", "alien": "#5a7f8c", "duck": "#7a5a36", "sandbag": "#a8946a", "wire": "#55504a",
        "tatami": "#b9b06a", "plank": "#8a5a32", "lantern": "#9a968c", "garden": "#7d8a80", "fleshfloor": "#8c4046", "vein": "#5e2238",
        "tooth": "#e8dcc0", "board": "#94683c", "barrel": "#7a5230", "headstone": "#8f9396", "coffin": "#5c4630", "earth": "#5a4634",
        "fence": "#33363a", "stump": "#7a5a3a", "shroom": "#c9a27a", "leaf": "#9a6a2a", "flag": "#77726c", "chain": "#4a4c50",
        "trap": "#6a4c2c", "tflag": "#b8a888", "tcolumn": "#cbbf9f", "gold": "#d4a72c", "shelf": "#6f6a64", "shell": "#e6d8c4",
        "drift": "#b09a78", "rope": "#b8a070", "weed": "#3f6a4a", "bone": "#e6dcc4", "rock": "#8a7f74", "pebble": "#8f8a82", "crate": "#a8743f",
        "metal": "#7d838c", "iron": "#4d4f55", "wood": "#8a5a32", "shrub": "#6f7a3a", "moss": "#5d7a3a", "crystal": "#b9dcec", "post": "#6b5a44",
        "concrete": "#a3a09a", "slate": "#6f7377", "grass": "#6d7a3a", "flesh": "#9c4a4a", "pustule": "#c9a35a", "egg": "#c7b37a"}
CUTS = {"joint": "#1a1714", "water": "#2d4a5a", "lava": "#e2571c", "bile": "#9aa63a", "tar": "#15130f", "pit": "#2a2018", "rut": "#3b2a1e",
        "crack": "#1e1612", "slot": "#0d1014", "glyph": "#3a2c18", "hollow": "#3a3026"}

D_ALL = None


def _D(): return Point(0, 0).buffer(R0 + 1.0, 128)


def KK(): return max(1.0, gg.K)                  # extents and counts grow with the base; nothing shrinks below the 30 mm design
def NN(n): return max(1, round(n * min(12.0, gg.K ** 2))) if gg.K > 1 else n


def _fr(rng):
    phi = rng.uniform(0, 2 * math.pi); f = frame(0, 0, phi)
    e = lambda x: x * KK() if abs(x) >= 20 else x          # "past the rim" extents follow the base size
    return phi, f, (lambda u0, u1, v0, v1: Polygon([f(e(u0), e(v0)), f(e(u1), e(v0)), f(e(u1), e(v1)), f(e(u0), e(v1))]))


# ------------------------------------------------------------------------------------------------ joint networks
def j_planks(phi, w, L=None, jw=0.22):
    """parallel boards of width w (joints along), optional butt joints every L, staggered"""
    E = 22 * KK()
    g = [box(-E, -E + j * w - jw, E, -E + j * w + jw) for j in range(int(2 * E / w) + 1)]
    if L: g += [box(-E + i * L + (j % 2) * L / 2 - jw, -E + j * w, -E + i * L + (j % 2) * L / 2 + jw, -E + (j + 1) * w) for j in range(int(2 * E / w) + 1) for i in range(int(2 * E / L) + 2)]
    return affinity.rotate(unary_union(g), math.degrees(phi), origin=(0, 0))

def j_grid(phi, sx, sy=None, jw=0.22):
    sy = sy or sx
    E = 22 * KK()
    g = [box(-E, y - jw, E, y + jw) for y in np.arange(-E, E, sy)] + [box(x - jw, -E, x + jw, E) for x in np.arange(-E, E, sx)]
    return affinity.rotate(unary_union(g), math.degrees(phi), origin=(0, 0))

def j_voronoi(rng, n=26, jw=0.24, amp=0.0):
    E = 17 * KK(); n = round(n * KK() ** 2)
    pts = MultiPoint([(rng.uniform(-E, E), rng.uniform(-E, E)) for _ in range(n)]); edges = set()
    for cell in voronoi_diagram(pts, envelope=box(-E - 3, -E - 3, E + 3, E + 3)).geoms:
        c = [tuple(round(v, 3) for v in xy) for xy in cell.exterior.coords]
        for a_, b_ in zip(c, c[1:]): edges.add(tuple(sorted((a_, b_))))
    return unary_union([(LineString(wiggle(a_, b_, rng, amp)) if amp else LineString([a_, b_])).buffer(jw, cap_style=1, resolution=3) for a_, b_ in edges])

def j_hex(r, phi=0.0, jw=0.22):
    segs = []
    m = int(12 * KK()) + 1
    for i in range(-m, m + 1):
        for j in range(-m, m + 1):
            cx = 1.5 * r * i; cy = math.sqrt(3) * r * (j + 0.5 * (i % 2))
            if abs(cx) > 19 * KK() or abs(cy) > 19 * KK(): continue
            hp = [(cx + r * math.cos(k * math.pi / 3), cy + r * math.sin(k * math.pi / 3)) for k in range(4)]
            segs.append(LineString(hp).buffer(jw, cap_style=1, resolution=3))
    return affinity.rotate(unary_union(segs), math.degrees(phi), origin=(0, 0))


def floor(s, region, h, kind, joints=None, depth=0.38, edge=0.5):
    """a floor slab over `region` (one solid) with a joint network cut into its top. Returns the slab polygon."""
    got = s.prism(polys_of(region), H + h, kind)
    if not got: return Polygon()
    sl = unary_union(got)
    if joints is not None:
        j = joints.intersection(sl.buffer(-edge))
        if not j.is_empty: s.tcut([j], H + h - depth, H + h, "joint")
    return sl


def channel(s, rng, w, depth, kind, amp=3.0):
    phi = rng.uniform(0, math.pi); ln = meander(rng, phi, rng.uniform(-5, 5), amp, w)
    g = ln.buffer(w / 2, cap_style=2).simplify(0.05)
    s.gcut([g], depth, kind); return g


def pools(s, rng, n, r0, r1, depth, kind, avoid=None):
    out = []; n = NN(n) if gg.K > 1.3 else n
    for _ in range(n * 30):
        if len(out) >= n: break
        p = blob(*rp(rng, 0, 10), rng.uniform(r0, r1), rng, rng.randint(3, 5))
        if Point(0, 0).buffer(R0 - 1.0).contains(p) and s.free(p, 0.5) and all(p.distance(q) > 0.8 for q in out) and (avoid is None or not p.intersects(avoid)): out.append(p)
    if out: s.gcut(out, depth, kind)
    return unary_union(out) if out else Polygon()


def cracks(s, rng, n, depth=0.4, kind="crack", w=0.22):
    cs = []
    for _ in range(max(n, round(n * KK()))):         # a few more on a bigger base, not a web of them
        a = rp(rng, 0, 12); t_ = rng.uniform(0, 6.283); L_ = rng.uniform(8, 20); b = (a[0] + L_ * math.cos(t_), a[1] + L_ * math.sin(t_))
        cs.append(LineString(wiggle(a, b, rng, 0.7, 7)).buffer(w, cap_style=1, resolution=3))
    s.gcut([unary_union(cs)], depth, kind)


def banks(s, rng, n, r0, r1, h, kind, tiers=((0.6, 25), (0.4, 50)), region=None):
    reg = region if region is not None else Point(0, 0).buffer(R0 - 0.8)
    n = NN(n)
    for _ in range(n * 25):
        if n <= 0: break
        p = blob(*rp(rng, 0, 11), rng.uniform(r0, r1), rng, rng.randint(3, 5))
        if reg.contains(p) and s.free(p, 0.5): s.stack([p], rng.uniform(*h), tiers, kind); n -= 1


def grit(s, rng, n0, n1, kind="grit"):
    s.stack(s.scatter(rng.randint(n0, n1), 0.47, 0.65, gap=0.35), 0.28, ((1.0, 20),), kind)


# ------------------------------------------------------------------------------------------------ layouts
def swamp(n):
    s = gg2.mk("swamp", "Swamps of Jargono", n, "#4f4a30"); rng = s.rng; v = rng.randint(0, 3); D = _D()
    if v == 0: channel(s, rng, rng.uniform(4, 6), 0.6, "water", 4.0)
    elif v == 1: pools(s, rng, rng.randint(2, 4), 2.2, 3.6, 0.55, "water")
    elif v == 2:          # raised mud causeway through water
        phi, f, R = _fr(rng); c = rng.uniform(-3, 3) * gg.K
        w = 4.5 * KK() ** 0.6                     # the causeway widens a little with the base, the water stays the larger part
        s.gcut([D.difference(R(-30, 30, c - w, c + w).buffer(1.5))], 0.55, "water")
    else:                 # serpent-temple paving sinking into the mud
        phi, f, R = _fr(rng); reg = R(-30, 30, rng.uniform(-2, 3), 30).intersection(D)
        floor(s, reg, 0.55, "tflag", j_grid(phi, rng.uniform(4.2, 5.0)))
        pools(s, rng, 2, 1.8, 2.6, 0.5, "water", avoid=reg.buffer(1.0))
    banks(s, rng, rng.randint(1, 3), 1.6, 2.6, (0.5, 0.9), "mud")
    grit(s, rng, 10, 20); return s


def cynder(n):
    s = gg2.mk("cynder", "Caverns of Cynder", n, "#3a3230"); rng = s.rng; v = rng.randint(0, 3); D = _D()
    if v == 0: channel(s, rng, rng.uniform(3.5, 5.5), 0.7, "lava", 4.0)
    elif v == 1: pools(s, rng, rng.randint(2, 3), 2.0, 3.2, 0.65, "lava")
    elif v == 2:          # cracked basalt plates with glowing gaps
        floor(s, D, 0.55, "basalt", j_voronoi(rng, rng.randint(14, 22), 0.32, 0.3), depth=0.5)
    else:                 # basalt shelf over a lava lake
        phi, f, R = _fr(rng); c = rng.uniform(-4, 2) * gg.K
        s.gcut([R(-30, 30, -30, c).intersection(D)], 0.7, "lava")
        floor(s, R(-30, 30, c + 0.8, 30).intersection(D), 0.5, "basalt", j_voronoi(rng, 16, 0.28, 0.3), depth=0.45)
    if v < 2: cracks(s, rng, rng.randint(2, 4), 0.45, "lava")
    for p in s.scatter(rng.randint(2, 4), 1.1, 1.9, gap=0.5, round_=False): s.stack([p], rng.uniform(0.8, 1.4), ROCKY, "basalt")
    grit(s, rng, 10, 20); return s


def targa(n):
    s = gg2.mk("targa", "Targa Plateau", n, "#8fa9b3"); rng = s.rng; v = rng.randint(0, 3); D = _D(); phi, f, R = _fr(rng)
    if v == 0: floor(s, D, 0.5, "alien", j_hex(rng.uniform(3.4, 4.2), phi))
    elif v == 1:
        c = rng.uniform(-3, 3) * gg.K; floor(s, R(-30, 30, c - 5, c + 5).intersection(D), 0.6, "alien", j_grid(phi, 5.0, 3.3))
    elif v == 2: floor(s, D, 0.45, "ice", j_voronoi(rng, rng.randint(8, 13), 0.2, 0.5), depth=0.3)
    else:
        reg = R(-30, 30, rng.uniform(-3, 2), 30).intersection(D); floor(s, reg, 0.55, "alien", j_grid(phi + 0.785, 4.6))
        cracks(s, rng, 2, 0.35)
    banks(s, rng, rng.randint(1, 3), 1.8, 3.0, (0.4, 0.8), "snow", ((0.5, 30), (0.5, 55)))
    return s


def trederra(n):
    s = gg2.mk("trederra", "Trederra", n, "#5a5340"); rng = s.rng; v = rng.randint(0, 3); D = _D(); phi, f, R = _fr(rng)
    if v in (0, 1):       # duckboard walkway through the mud
        c = rng.uniform(-4, 4) * gg.K; w = rng.uniform(5.5, 7.0)
        floor(s, R(-30, 30, c - w / 2, c + w / 2).intersection(D), 0.55, "duck", j_planks(phi + math.pi / 2, 1.5, None, 0.25), depth=0.45)
        s.prism([R(-30, 30, c + o - 0.5, c + o + 0.5).intersection(D) for o in (-w / 2 + 0.9, w / 2 - 0.9)], H + 0.85, "wood", z0=H + 0.4, foot=False)
        if v == 1: pools(s, rng, 2, 1.6, 2.4, 0.5, "water", avoid=R(-30, 30, c - w / 2 - 1, c + w / 2 + 1))
    elif v == 2:          # shell-torn ground
        for _ in range(rng.randint(1, 2)):
            cx, cy = rp(rng, 0, 7); r = rng.uniform(2.0, 2.8)
            if s.free(Point(cx, cy).buffer(r + 2), 0.3): crater(s, cx, cy, r, 0.75)
    else:                 # trench floor between sandbag walls
        c = rng.uniform(-2, 2) * gg.K; half = rng.uniform(4.0, 5.0)
        for sgn in (-1, 1):
            bags = [affinity.rotate(R(u, u + 3.4, sgn * (half + 0.2), sgn * (half + 2.4)).buffer(0.35), rng.uniform(-3, 3), origin="centroid") for u in np.arange(-20 * KK(), 20 * KK(), 3.7)]
            s.stack([b.intersection(D) for b in bags], 1.2, ((0.7, 8), (0.3, 40)), "sandbag")
        s.gcut([R(-30, 30, c - 1.0, c + 1.0).intersection(D)], 0.35, "water")
    cracks(s, rng, rng.randint(1, 2), 0.4, "rut", 0.35)
    grit(s, rng, 10, 22); return s


def fortress(n):
    s = gg2.mk("fortress", "Forbidden Fortress", n, "#6a6658"); rng = s.rng; v = rng.randint(0, 3); D = _D(); phi, f, R = _fr(rng)
    if v == 0: floor(s, D, 0.55, "plank", j_planks(phi, rng.uniform(2.0, 2.6), rng.uniform(9, 12)))
    elif v == 1:          # tatami mats: 2:1 mats with a border groove
        floor(s, D, 0.5, "tatami", j_grid(phi, 9.0, 4.5, 0.3), depth=0.3)
    elif v == 2:          # raised walkway beside a raked gravel garden
        c = rng.uniform(-2, 4) * gg.K
        floor(s, R(-30, 30, c, 30).intersection(D), 0.7, "plank", j_planks(phi, 2.2, 10))
        rake = unary_union([R(-30, 30, c - 1.5 - k * 1.5, c - 1.5 - k * 1.5 + 0.5) for k in range(1, int(12 * KK()))]).intersection(Point(0, 0).buffer(R0 - 0.6))
        s.gcut([rake], 0.3, "rut")
    else:                 # courtyard flagstones with a path of stepping stones
        floor(s, D, 0.5, "flag", j_voronoi(rng, rng.randint(16, 24), 0.22))
    return s


def beast(n):
    s = gg2.mk("beast", "Belly of the Beast", n, "#7a3038"); rng = s.rng; v = rng.randint(0, 3); D = _D()
    if v == 0: floor(s, D, 0.55, "fleshfloor", j_voronoi(rng, rng.randint(9, 14), 0.3, 0.9), depth=0.45)
    elif v == 1: pools(s, rng, rng.randint(2, 4), 2.0, 3.2, 0.6, "bile")
    elif v == 2: channel(s, rng, rng.uniform(3.5, 5.0), 0.6, "bile", 4.5)
    else:                 # ribbed floor: parallel muscle ridges
        phi, f, R = _fr(rng)
        ridges = [R(-30, 30, y, y + 1.7).intersection(D) for y in np.arange(-14 * KK() + rng.uniform(0, 3), 16 * KK(), rng.uniform(6.5, 8.5))]
        s.stack(ridges, 0.7, ((0.5, 20), (0.5, 50)), "flesh")
    banks(s, rng, rng.randint(1, 3), 1.6, 2.8, (0.6, 1.2), "flesh")
    return s


def frontier(n):
    s = gg2.mk("frontier", "Frontier Town", n, "#8a6f4c"); rng = s.rng; v = rng.randint(0, 3); D = _D(); phi, f, R = _fr(rng)
    if v == 0:            # boardwalk edge + dirt street
        c = rng.uniform(-3, 3) * gg.K
        floor(s, R(-30, 30, c, 30).intersection(D), 0.85, "board", j_planks(phi + math.pi / 2, rng.uniform(1.8, 2.3)))
        s.prism([R(-30, 30, c - 0.9, c + 0.1).intersection(D)], H + 1.0, "wood")
        ruts = [meander(rng, phi, c - o, 0.6, 1.0).buffer(0.5, cap_style=2) for o in (4.5, 8.5)]
        s.gcut([unary_union(ruts)], 0.35, "rut")
    elif v == 1: floor(s, D, 0.6, "board", j_planks(phi, rng.uniform(1.9, 2.5), rng.uniform(10, 13)))
    elif v == 2:          # dirt street with wagon ruts and hoof prints
        o = rng.uniform(-3, 3)
        s.gcut([unary_union([meander(rng, phi, o + d, 0.8, 1.0).buffer(0.55, cap_style=2) for d in (-2.6, 2.6)])], 0.4, "rut")
        cracks(s, rng, 2, 0.3)
    else:                 # corner of a plank porch
        a, b = rng.uniform(-4, 1), rng.uniform(-4, 1)
        floor(s, R(a, 30, b, 30).intersection(D), 0.8, "board", j_planks(phi, 2.1))
    grit(s, rng, 12, 24); return s


def graveyard(n):
    s = gg2.mk("graveyard", "Graveyard", n, "#4c4a3a"); rng = s.rng; v = rng.randint(0, 3); D = _D(); phi, f, R = _fr(rng)
    if v == 0:            # open grave
        cx, cy = rp(rng, 0, 4); g = rect(cx, cy, 9.0, 4.2, phi)
        s.gcut([g], 0.9, "pit"); banks(s, rng, 2, 1.8, 2.6, (0.7, 1.1), "earth", region=Point(0, 0).buffer(R0 - 0.8).difference(g.buffer(0.6)))
    elif v == 1:          # gravel path between plots
        c = rng.uniform(-3, 3) * gg.K; floor(s, R(-30, 30, c - 3, c + 3).intersection(D), 0.45, "flag", j_voronoi(rng, 26, 0.2))
    elif v == 2:          # grave slab
        cx, cy = rp(rng, 0, 4); g = rect(cx, cy, 9.5, 4.6, phi)
        top = s.prism([g], H + 0.8, "headstone")
        if top: s.tcut([inset(g, 0.6).difference(inset(g, 0.95))], H + 0.5, H + 0.8, "slot")
    else: cracks(s, rng, 3, 0.35)
    for p in s.scatter(rng.randint(1, 3), 1.0, 1.6, gap=0.5, round_=False): s.stack([p], rng.uniform(0.6, 1.0), ROCKY, "rock")
    grit(s, rng, 10, 20); return s


def forest(n):
    s = gg2.mk("forest", "Forest Floor", n, "#4a4630"); rng = s.rng; v = rng.randint(0, 3)
    if v == 0:            # big root system crossing the base
        a0 = rng.uniform(0, 6.28); c = (13.5 * gg.K * math.cos(a0), 13.5 * gg.K * math.sin(a0))
        roots = [LineString(wiggle(c, (c[0] - (16 + rng.uniform(0, 8)) * gg.K * math.cos(a0 + da), c[1] - (16 + rng.uniform(0, 8)) * gg.K * math.sin(a0 + da)), rng, 1.2, 7)).buffer(rng.uniform(0.7, 1.1), cap_style=1, resolution=4)
                 for da in (-0.7, -0.3, 0.1, 0.5)]
        s.stack([unary_union(roots)], 1.0, ((0.5, 20), (0.5, 50)), "root")
    elif v == 1: channel(s, rng, rng.uniform(2.5, 3.5), 0.5, "water", 4.0)
    elif v == 2: pools(s, rng, rng.randint(1, 2), 1.8, 2.8, 0.45, "water")
    else:                 # animal trail
        phi = rng.uniform(0, math.pi); s.gcut([meander(rng, phi, rng.uniform(-3, 3), 2.5, 2.4).buffer(1.2, cap_style=2)], 0.3, "rut")
    for p in s.scatter(rng.randint(2, 4), 1.0, 1.7, gap=0.5, round_=False): s.stack([p], rng.uniform(0.6, 1.1), ROCKY, "rock")
    grit(s, rng, 12, 24); return s


def dungeon(n):
    s = gg2.mk("dungeon", "Dungeon", n, "#55524e"); rng = s.rng; v = rng.randint(0, 3); D = _D(); phi, f, R = _fr(rng)
    if v == 0: floor(s, D, 0.55, "flag", j_grid(phi, rng.uniform(4.6, 5.6), rng.uniform(4.6, 5.6)))
    elif v == 1: floor(s, D, 0.55, "flag", j_voronoi(rng, rng.randint(18, 28), 0.24))
    elif v == 2: floor(s, D, 0.55, "flag", j_planks(phi, rng.uniform(3.0, 3.6), rng.uniform(5.5, 7.0)))
    else:                 # flagged floor with a drain channel
        c = rng.uniform(-4, 4) * gg.K
        for reg in (R(-30, 30, c + 0.9, 30), R(-30, 30, -30, c - 0.9)): floor(s, reg.intersection(D), 0.55, "flag", j_grid(phi, 5.0))
        s.gcut([R(-30, 30, c - 0.55, c + 0.55).intersection(D)], 0.4, "water")
    return s


def temple(n):
    s = gg2.mk("temple", "Ancient Temple", n, "#9c8d6e"); rng = s.rng; v = rng.randint(0, 3); D = _D(); phi, f, R = _fr(rng)
    if v == 0: floor(s, D, 0.55, "tflag", j_grid(phi, rng.uniform(6.5, 8.0)))
    elif v == 1:          # steps
        c = rng.uniform(-6, -2) * gg.K
        for k in range(3):
            floor(s, R(-30, 30, c + k * 4.0, 30).intersection(D), 0.5 + 0.55 * k, "tflag", j_grid(phi, 30, 30) if k else None)
        s.tcut([j_grid(phi, 6.0, 40).intersection(R(-30, 30, c + 8.6, 30)).intersection(Point(0, 0).buffer(R0 - 0.6))], H + 1.6 - 0.35, H + 1.6, "joint")
    elif v == 2:          # inlaid circle
        sl = floor(s, D, 0.55, "tflag", None)
        cx, cy = rp(rng, 0, 3); rings = unary_union([ring(cx, cy, r, r - 0.4, 48) for r in (3.0, 6.0, 9.0)] + [radial(cx, cy, 3.0, 9.0, 8, rng, 0.2)])
        s.tcut([rings.intersection(sl.buffer(-0.6))], H + 0.2, H + 0.55, "joint")
    else:                 # sand-drifted paving
        reg = R(-30, 30, rng.uniform(-3, 3), 30).intersection(D); floor(s, reg, 0.55, "tflag", j_grid(phi + 0.4, 5.5))
        banks(s, rng, 2, 2.0, 3.0, (0.4, 0.7), "snow", ((0.5, 25), (0.5, 50)), region=Point(0, 0).buffer(R0 - 0.8).difference(reg.buffer(0.5)))
    return s


def coast(n):
    s = gg2.mk("coast", "Rocky Coast", n, "#7a705e"); rng = s.rng; v = rng.randint(0, 3); D = _D()
    if v == 0: pools(s, rng, rng.randint(2, 4), 1.8, 3.0, 0.6, "water")
    elif v == 1: floor(s, D, 0.6, "shelf", j_voronoi(rng, rng.randint(7, 11), 0.35, 0.8), depth=0.5)
    elif v == 2:          # water's edge
        phi, f, R = _fr(rng); ln = meander(rng, phi, rng.uniform(-4, 2), 1.5, 1.0)
        s.gcut([unary_union([ln.buffer(0.1), R(-30, 30, -30, -6)]).convex_hull.intersection(D) if False else R(-30, 30, -30, rng.uniform(-6, -1)).intersection(D)], 0.55, "water")
    else: channel(s, rng, rng.uniform(2.5, 4.0), 0.55, "water", 3.5)
    for p in s.scatter(rng.randint(2, 5), 1.2, 2.2, gap=0.5, round_=False): s.stack([p], rng.uniform(0.7, 1.4), ROCKY, "shelf")
    grit(s, rng, 14, 26, "pebble"); return s


# ------------------------------------------------------------------------------------------------ signature features
def f_post_row(kind, h=(1.8, 2.4), rail=True, n=(2, 3), gap=4.2):
    """posts in a line, optional rail between them (fence, hitching post, wire pickets)"""
    def fn(s, rng, lvl):
        def at(cx, cy):
            a = rng.uniform(0, math.pi); k = rng.randint(*n); ff = frame(cx, cy, a)
            return [rect(*ff((i - (k - 1) / 2) * gap, 0), 0.95, 0.95, a) for i in range(k)] + ([rect(cx, cy, (k - 1) * gap, 0.5, a)] if rail else [])
        ps, z = place_group(s, rng, at, gap=0.4)
        if not ps: return
        hh = rng.uniform(*h); posts = ps[:-1] if rail else ps
        top = up(s, posts, z, hh, kind)
        if rail and top: s.prism([ps[-1]], min(top - 0.3, CAPZ), kind, z0=(H - SINK) if z <= H else z - 0.15)
    return fn

def f_headstone(s, rng, lvl):
    def mkst():
        cx, cy = rp(rng, 1, 10); a = rng.uniform(0, math.pi)
        return rect(cx, cy, rng.uniform(2.6, 3.2), 1.0, a)
    for _ in range(rng.randint(1, 2) + (lvl >= 3)):
        g, z = find(s, lambda: mkst().buffer(0.45, join_style=2), gap=0.4)
        if g is None: continue
        top = up(s, [g], z, 0.4, "headstone")
        if top: s.prism([inset(g, 0.45)], min(top + rng.uniform(1.8, 2.5), CAPZ), "headstone", z0=top - 0.15, foot=False)

def f_lantern(s, rng, lvl):
    g, z = find(s, lambda: Point(*rp(rng, 2, 10)).buffer(1.5, 6), gap=0.4)
    if g is None: return
    c = g.centroid; top = up(s, [g], z, 0.5, "lantern")
    if not top: return
    s.prism([Point(c.x, c.y).buffer(0.7, 16)], min(top + 1.2, CAPZ), "lantern", z0=top - 0.15, foot=False)
    s.prism([Point(c.x, c.y).buffer(1.25, 6)], min(top + 1.9, CAPZ), "lantern", z0=top + 1.05, foot=False)

def f_column(kind="tcolumn", broken=True):
    def fn(s, rng, lvl):
        g, z = find(s, lambda: Point(*rp(rng, 2, 10)).buffer(1.9, 4), gap=0.4)
        if g is None: return
        c = g.centroid; g = affinity.rotate(g, rng.uniform(0, 90), origin=c); top = up(s, [g], z, 0.5, kind)
        if top: s.prism([Point(c.x, c.y).buffer(1.25, 20)], min(top + rng.uniform(1.4, 2.4), CAPZ), kind, z0=top - 0.15, foot=False)
    return fn

def f_stump(s, rng, lvl):
    g, z = find(s, lambda: stone(*rp(rng, 1, 9), rng.uniform(1.5, 2.1), rng, 0.9, 9), gap=0.4)
    if g is None: return
    c = g.centroid; top = up(s, [g], z, rng.uniform(1.0, 1.6), "stump")
    if top: topcut(s, [unary_union([ring(c.x, c.y, r, r - 0.25, 20) for r in (0.7, 1.2)]).intersection(inset(g, 0.3))], top, 0.25, "slot")

def f_teeth(s, rng, lvl):
    def at(cx, cy):
        a0 = rng.uniform(0, 2 * math.pi); R_ = rng.uniform(4, 6); ox, oy = cx - R_ * math.cos(a0), cy - R_ * math.sin(a0)
        return [stone(ox + R_ * math.cos(a0 + t), oy + R_ * math.sin(a0 + t), rng.uniform(0.6, 0.85), rng, 0.8, 3) for t in np.linspace(-0.5, 0.5, rng.randint(4, 6))]
    ps, z = place_group(s, rng, at, gap=0.4)
    if ps: mound(s, ps, z, rng.uniform(1.4, 2.0), ((0.6, 6), (0.4, 28)), "tooth")

def f_sandbags(s, rng, lvl):
    def at(cx, cy):
        a = rng.uniform(0, math.pi); ff = frame(cx, cy, a)
        return [rect(*ff((i - 1) * 3.0, 0), 2.7, 1.5, a).buffer(0.3) for i in range(3)]
    ps, z = place_group(s, rng, at, gap=0.4)
    if not ps: return
    mound(s, ps, z, 0.9, ((0.7, 8), (0.3, 40)), "sandbag")

def f_coffin(s, rng, lvl):
    def mkc():
        cx, cy = rp(rng, 0, 8); a = rng.uniform(0, math.pi); ff = frame(cx, cy, a)
        return Polygon([ff(-3.2, -0.8), ff(-1.6, -1.3), ff(3.2, -0.9), ff(3.2, 0.9), ff(-1.6, 1.3), ff(-3.2, 0.8)])
    g, z = find(s, mkc, gap=0.4)
    if g is None: return
    top = up(s, [g], z, rng.uniform(0.6, 0.9), "coffin")
    if top:
        c = g.centroid; topcut(s, [inset(g, 0.4).difference(inset(g, 0.7))], top, 0.25, "slot")

def f_trapdoor(s, rng, lvl):
    g, z = find(s, lambda: rect(*rp(rng, 0, 8), 5.0, 5.0, rng.uniform(0, math.pi)), gap=0.4)
    if g is None: return
    top = up(s, [g], z, 0.35, "trap")
    if top:
        mr = g.minimum_rotated_rectangle.exterior.coords; a = math.atan2(mr[1][1] - mr[0][1], mr[1][0] - mr[0][0]); c = g.centroid; ff = frame(c.x, c.y, a)
        topcut(s, [unary_union([rect(*ff(0, v), 6, 0.3, a) for v in (-1.25, 0, 1.25)]).intersection(inset(g, 0.3))], top, 0.25, "slot")
        s.prism([ring(*ff(1.6, 0), 0.75, 0.35, 16)], min(top + 0.3, CAPZ), "iron", z0=top - 0.15, foot=False)

def f_lily(s, rng, lvl):
    water = [p for ps, d, k in s.gcuts if k == "water" for p in ps]
    if not water: return
    reg = unary_union(water).buffer(-0.9)
    if reg.is_empty: return
    pads = []
    x0, y0, x1, y1 = reg.bounds
    for _ in range(200):
        if len(pads) >= rng.randint(3, 6): break
        c = Point(rng.uniform(x0, x1), rng.uniform(y0, y1)); r = rng.uniform(0.7, 1.0)
        p = c.buffer(r, 16).difference(Polygon([(c.x, c.y), (c.x + 2 * r, c.y - 0.5 * r), (c.x + 2 * r, c.y + 0.5 * r)]))
        if reg.contains(c.buffer(r)) and all(p.distance(q) > 0.4 for q in pads): pads.append(p)
    if pads: s.prism(pads, H - 0.2, "lily", z0=H - 0.75, foot=False)       # sits in the pool, just below the bank

def f_vent(kind_cut, kind_rim):
    def fn(s, rng, lvl):
        g, z = find(s, lambda: Point(*rp(rng, 1, 9)).buffer(rng.uniform(1.5, 2.0), 9), gap=0.4)
        if g is None: return
        top = up(s, [g], z, rng.uniform(0.7, 1.1), kind_rim)
        if top: topcut(s, [inset(g, 0.55)], top, 0.5, kind_cut)
    return fn


TITLES = {"swamp": "Swamps of Jargono", "cynder": "Caverns of Cynder", "targa": "Targa Plateau", "trederra": "Trederra", "fortress": "Forbidden Fortress",
          "beast": "Belly of the Beast", "frontier": "Frontier Town", "graveyard": "Graveyard", "forest": "Forest Floor", "dungeon": "Dungeon",
          "temple": "Ancient Temple", "coast": "Rocky Coast"}
STYLES2 = {}; TIERS2 = {}; DETAILS2 = {}


def _nature(rock="rock", pebble="pebble", bone=True, water="water", plant=None):
    out = [scatter(sh_rock(0.9, 1.4), (3, 5), (0.6, 1.0), rock), scatter(sh_round(0.5, 0.75), (6, 10), (0.35, 0.5), pebble, "mound", 1.6, ((0.7, 10), (0.3, 45))),
           single(sh_rock(1.8, 2.5), (1.1, 1.7), rock, "mound", tiers=ROCKY), scatter(sh_tri(0.7, 1.1), (3, 6), (0.5, 0.8), rock, "mound", 1.8, ROCKY),
           sunk(sh_blob(1.3, 1.9), (0.3, 0.45), water), sunk(sh_wig(5, 8, 0.45, 0.9), (0.4, 0.5), "crack"), trail(1.3, (5, 8)),
           sunk(sh_disc(0.4, 0.55, 10), (0.3, 0.4), "pit", (4, 7), 1.5)]
    if bone: out += [scatter(sh_bone(1.5, 2.2), (2, 4), (0.4, 0.5), "bone", "up"), single(sh_ell(1.1, 1.4, 0.7, 0.9), (0.6, 0.8), "bone", "mound", pat_dots(0.22))]
    if plant: out += [scatter(sh_star(0.9, 1.3, 5, 8), (2, 4), (0.8, 1.2), plant, "mound", 2.0, SOFTT), single(sh_star(1.5, 2.1, 6, 9), (1.0, 1.4), plant, "mound", tiers=SOFTT)]
    return out


def _gear(crate="crate", wood="wood", metal="iron"):
    return [single(sh_rect(2.4, 3.2, 1.6, 2.0), (1.0, 1.4), crate, "up", pat_seams(0.8)), scatter(sh_rect(1.2, 1.6, 1.0, 1.3), (2, 3), (0.8, 1.1), crate, "up", 1.8),
            single(sh_disc(0.9, 1.2, 24), (1.4, 1.8), wood, "up", pat_ring(0.25)), scatter(sh_caps(2.4, 3.4, 0.55), (2, 3), (0.5, 0.6), wood, "up", 1.8),
            single(sh_caps(4.5, 6.5, 0.95), (0.6, 0.8), wood, "up", pat_seams(1.3)), scatter(sh_caps(0.7, 1.0, 0.3), (5, 8), (0.3, 0.35), metal, "up", 1.4),
            single(sh_ring(1.2, 1.5, 0.5), (0.5, 0.7), metal, "up"), path(0.5, (6, 10), 0.4, "rope", "up", 0.9)]


def _build_tables():
    N, G = _nature, _gear
    STYLES2.update({"swamp": swamp, "cynder": cynder, "targa": targa, "trederra": trederra, "fortress": fortress, "beast": beast, "frontier": frontier,
                    "graveyard": graveyard, "forest": forest, "dungeon": dungeon, "temple": temple, "coast": coast})
    log = single(sh_caps(5.0, 7.5, 1.3), (0.8, 1.1), "log", "up", pat_seams(1.6)); stumpf = f_stump
    reeds = scatter(sh_disc(0.42, 0.52, 8), (6, 10), (1.4, 2.2), "reed", "up", 1.6)
    roots = path(0.9, (6, 10), 0.6, "root", "up", 1.2)
    TIERS2.update({
        "swamp": ([log, reeds], [f_lily, roots, stumpf], [f_column("tflag"), single(sh_hex(1.4, 1.8), (0.8, 1.1), "tflag", "up", pat_rune()), f_post_row("wood", (1.6, 2.2), False)]),
        "cynder": ([f_vent("lava", "basalt"), scatter(sh_tri(0.7, 1.0), (4, 6), (1.0, 1.6), "obsidian", "mound", 1.6, ((0.6, 4), (0.4, 30)))],
                   [single(sh_tri(1.5, 2.0), (1.8, 2.6), "obsidian", "mound", tiers=((0.6, 4), (0.4, 25))), sunk(sh_star(1.4, 1.9, 5, 7), (0.4, 0.5), "lava"), path(0.5, (6, 10), 0.45, "lava", "cut", 1.0)],
                   [f_column("basalt"), single(sh_ring(1.6, 2.0, 0.7), (0.8, 1.0), "basalt", "up"), f_post_row("iron", (1.4, 2.0), True, (2, 2), 4.5)]),
        "targa": ([scatter(sh_hex(0.7, 1.0), (3, 5), (0.9, 1.5), "crystal", "up", 1.6), single(sh_rect(2.6, 3.4, 1.6, 2.0), (0.8, 1.1), "alien", "up", pat_slots(3))],
                  [single(sh_disc(1.3, 1.6, 24), (0.6, 0.9), "alien", "up", pat_ring()), path(0.6, (7, 11), 0.4, "cable", "up", 0.6), sunk(sh_ring(1.4, 1.8, 0.4), (0.35, 0.45), "slot")],
                  [f_column("alien"), single(sh_hex(1.5, 1.9), (1.8, 2.6), "crystal", "up"), f_vent("slot", "alien")]),
        "trederra": ([f_sandbags, scatter(sh_caps(0.7, 1.0), (6, 10), (0.3, 0.35), "iron", "up", 1.4) if False else scatter(sh_caps(0.7, 1.0, 0.3), (6, 10), (0.3, 0.35), "iron", "up", 1.4)],
                     [f_post_row("wire", (1.4, 1.9), True, (2, 3), 4.0), single(sh_rect(2.6, 3.2, 1.6, 2.0), (1.0, 1.3), "crate", "up", pat_seams(0.8)), sunk(sh_blob(1.5, 2.1), (0.5, 0.7), "water")],
                     [f_sandbags, single(sh_disc(0.9, 1.1, 20), (1.6, 2.0), "iron", "up", pat_ring(0.25)), path(0.45, (8, 12), 0.45, "wire", "up", 1.3)]),
        "fortress": ([f_lantern, scatter(sh_round(0.9, 1.4), (3, 5), (0.5, 0.8), "garden", "mound", 2.2, ROCKY)],
                     [single(sh_rect(3.0, 3.8, 1.2, 1.5), (0.8, 1.0), "plank", "up", pat_seams(0.9)), single(sh_disc(1.0, 1.3, 24), (1.2, 1.6), "wood", "up", pat_ring(0.25)), f_post_row("wood", (1.8, 2.4), True, (2, 2), 5.0)],
                     [f_lantern, single(sh_rect(2.2, 2.8, 2.2, 2.8), (0.5, 0.6), "tatami", "up", pat_cross()), scatter(sh_caps(3.0, 4.0, 0.4), (1, 2), (0.4, 0.5), "iron", "up", 1.5)]),
        "beast": ([f_teeth, path(0.7, (7, 11), 0.6, "vein", "up", 1.3)],
                  [scatter(sh_disc(0.6, 0.9, 16), (3, 6), (0.7, 1.0), "pustule", "mound", 1.6, SOFTT), sunk(sh_blob(1.4, 2.0), (0.45, 0.6), "bile"), path(0.5, (8, 12), 0.5, "vein", "up", 1.5)],
                  [f_teeth, f_vent("bile", "flesh"), scatter(sh_bone(1.6, 2.2), (2, 4), (0.4, 0.5), "bone", "up")]),
        "frontier": ([single(sh_disc(1.0, 1.25, 24), (1.5, 1.9), "barrel", "up", pat_ring(0.25)), f_post_row("post", (1.8, 2.3), True, (2, 2), 5.0)],
                     [single(sh_rect(2.6, 3.4, 1.8, 2.2), (1.1, 1.4), "crate", "up", pat_seams(0.8)), trail(1.5, (5, 8), sh_crescent(0.45, 0.55), 0.3, "pit"), scatter(sh_caps(2.4, 3.2, 0.5), (2, 3), (0.5, 0.6), "board", "up", 1.6)],
                     [single(sh_ring(1.8, 2.2, 0.5), (0.5, 0.6), "iron", "up"), scatter(sh_disc(0.9, 1.1, 20), (2, 3), (1.4, 1.8), "barrel", "up", 2.2), single(sh_rect(3.4, 4.2, 1.2, 1.5), (0.9, 1.1), "board", "up", pat_seams(1.0))]),
        "graveyard": ([f_headstone, scatter(sh_bone(1.5, 2.1), (2, 3), (0.4, 0.5), "bone", "up")],
                      [f_coffin, f_post_row("fence", (1.8, 2.4), True, (2, 3), 4.0), single(sh_cross(2.6, 3.2, 0.8), (0.5, 0.7), "headstone", "up")],
                      [f_headstone, single(sh_blob(1.8, 2.4), (0.7, 1.0), "earth", "mound", tiers=SOFTT), single(sh_ell(1.1, 1.4, 0.8, 1.0), (0.7, 0.9), "bone", "mound", pat_dots(0.22))]),
        "forest": ([f_stump, scatter(sh_disc(0.55, 0.8, 14), (3, 6), (0.6, 0.9), "shroom", "mound", 1.5, ((0.5, 5), (0.5, 50)))],
                   [log, roots, scatter(sh_star(1.0, 1.4, 6, 9), (2, 4), (0.9, 1.3), "shrub", "mound", 2.0, SOFTT)],
                   [f_stump, single(sh_blob(1.6, 2.2), (0.6, 0.9), "moss", "mound", tiers=SOFTT), scatter(sh_ell(0.5, 0.7, 0.9, 1.2), (5, 8), (0.3, 0.35), "leaf", "up", 1.8)]),
        "dungeon": ([f_trapdoor, scatter(sh_bone(1.5, 2.1), (2, 4), (0.4, 0.5), "bone", "up")],
                    [single(sh_disc(1.4, 1.7, 24), (0.4, 0.5), "iron", "up", pat_slots(4, 0.3)), path(0.6, (7, 11), 0.4, "chain", "up", 1.0), single(sh_rect(2.6, 3.2, 1.6, 2.0), (1.0, 1.3), "crate", "up", pat_seams(0.8))],
                    [f_column("flag"), single(sh_ring(1.3, 1.6, 0.45), (0.5, 0.6), "iron", "up"), sunk(sh_blob(1.4, 2.0), (0.3, 0.4), "water")]),
        "temple": ([f_column("tcolumn"), scatter(sh_rect(1.4, 2.0, 1.0, 1.4), (2, 4), (0.6, 0.9), "tcolumn", "up", 2.0)],
                   [single(sh_hex(1.5, 1.9), (0.7, 0.9), "tflag", "up", pat_rune()), single(sh_caps(4.5, 6.0, 1.9), (1.0, 1.3), "tcolumn", "up", pat_seams(1.5)), sunk(sh_ring(1.6, 2.0, 0.4), (0.35, 0.45), "glyph")],
                   [f_column("tcolumn"), f_vent("pit", "tflag"), scatter(sh_disc(0.4, 0.5, 12), (5, 9), (0.3, 0.35), "gold", "up", 1.2)]),
        "coast": ([scatter(sh_crescent(0.6, 0.9), (3, 6), (0.35, 0.45), "shell", "up", 1.6), single(sh_caps(5.0, 7.0, 1.0), (0.6, 0.8), "drift", "up", pat_seams(1.8))],
                  [path(0.6, (7, 11), 0.45, "rope", "up", 1.2), scatter(sh_star(0.8, 1.1, 5, 5), (1, 3), (0.4, 0.5), "shell", "mound", 2.0, ((0.5, 15), (0.5, 45))), sunk(sh_blob(1.4, 2.0), (0.4, 0.55), "water")],
                  [single(sh_ring(1.5, 1.9, 0.6), (0.6, 0.8), "iron", "up"), f_post_row("drift", (1.6, 2.2), False, (2, 3), 3.2), scatter(sh_wig(2.5, 3.5, 0.5, 0.6), (2, 4), (0.4, 0.5), "weed", "up", 1.8)]),
    })
    def pad(lst, n=30):
        lst = list(lst); k = 0
        fill = [sunk(sh_crescent(1.0, 1.4), (0.3, 0.4), "pit"), scatter(sh_ell(0.6, 0.8, 0.9, 1.2), (3, 5), (0.5, 0.7), "rock", "mound", 1.6, ROCKY), sunk(sh_star(1.2, 1.6, 5, 6), (0.3, 0.4), "crack"),
                single(sh_hex(1.1, 1.4), (0.7, 0.9), "rock", "up", pat_cross()), scatter(sh_crescent(0.8, 1.1), (2, 3), (0.5, 0.6), "rock", "up"), sunk(sh_rect(1.8, 2.4, 1.0, 1.4), (0.4, 0.6), "pit")]
        while len(lst) < n: lst.append(fill[k % len(fill)]); k += 1
        return lst[:max(n, len(lst))]
    T = TIERS2
    DETAILS2.update({
        "swamp": pad(N("rock", "pebble", True, "water", "reed") + G("crate", "log", "iron")[:5] + [x for t in T["swamp"] for x in t] + [scatter(sh_disc(0.5, 0.7, 12), (4, 7), (0.5, 0.7), "shroom", "mound", 1.4, ((0.5, 5), (0.5, 50))), sunk(sh_ring(1.3, 1.7, 0.4), (0.3, 0.4), "glyph"), single(sh_ell(1.5, 1.9, 0.9, 1.1), (0.8, 1.1), "mud", "mound", tiers=SOFTT)]),
        "cynder": pad(N("basalt", "pebble", True, "lava", None) + [x for t in T["cynder"] for x in t] + G("crate", "wood", "iron")[:4] + [scatter(sh_hex(0.6, 0.9), (3, 5), (0.7, 1.1), "obsidian", "up", 1.6), sunk(sh_disc(0.5, 0.7, 10), (0.4, 0.5), "lava", (3, 5), 1.6), scatter(sh_round(0.5, 0.7), (5, 8), (0.4, 0.5), "ember", "mound", 1.4, ((0.6, 15), (0.4, 45))), single(sh_blob(1.6, 2.2), (0.7, 1.0), "basalt", "mound", tiers=ROCKY)]),
        "targa": pad(N("ice", "snow", True, "water", None) + [x for t in T["targa"] for x in t] + G("crate", "alien", "metal")[:5] + [scatter(sh_tri(0.7, 1.0), (4, 6), (0.9, 1.4), "ice", "mound", 1.6, ((0.6, 4), (0.4, 30))), single(sh_blob(1.8, 2.4), (0.5, 0.8), "snow", "mound", tiers=((0.5, 30), (0.5, 55))), single(sh_rect(1.8, 2.2, 1.8, 2.2), (0.4, 0.5), "alien", "up", pat_dots()), sunk(sh_hex(1.0, 1.3), (0.4, 0.5), "slot")]),
        "trederra": pad(N("rock", "pebble", True, "water", None) + G("crate", "wood", "iron") + [x for t in T["trederra"] for x in t] + [scatter(sh_rect(1.2, 1.6, 0.5, 0.7), (3, 5), (0.4, 0.5), "duck", "up", 1.6), single(sh_cross(2.6, 3.2, 0.6), (0.6, 0.8), "wood", "up"), sunk(sh_blob(1.8, 2.4), (0.6, 0.8), "pit"), scatter(sh_disc(0.55, 0.7, 14), (2, 3), (0.5, 0.6), "iron", "mound", 1.6, ((0.5, 10), (0.5, 45)))]),
        "fortress": pad([x for t in T["fortress"] for x in t] + G("crate", "wood", "iron") + N("garden", "pebble", True, "water", "shrub")[:9] + [scatter(sh_rect(1.6, 2.0, 0.4, 0.5), (2, 4), (0.35, 0.45), "wood", "up", 1.6), single(sh_disc(1.2, 1.5, 24), (0.5, 0.6), "wood", "up", pat_cross()), single(sh_hex(1.1, 1.4), (0.9, 1.2), "lantern", "up", pat_hole(0.35, 0.5)), scatter(sh_disc(0.5, 0.65, 14), (3, 5), (0.6, 0.8), "wood", "up", 1.5), sunk(sh_rect(2.2, 2.8, 1.4, 1.8), (0.3, 0.4), "slot")]),
        "beast": pad([x for t in T["beast"] for x in t] + N("flesh", "pustule", True, "bile", None)[:8] + [scatter(sh_ell(0.6, 0.8, 0.9, 1.2), (3, 6), (0.7, 1.0), "egg", "mound", 1.6, SOFTT), single(sh_blob(1.8, 2.4), (1.0, 1.4), "flesh", "mound", tiers=SOFTT), sunk(sh_disc(0.4, 0.6, 12), (0.4, 0.5), "bile", (4, 7), 1.6), sunk(sh_ring(1.4, 1.8, 0.45), (0.4, 0.5), "pit"), scatter(sh_crescent(0.8, 1.1), (2, 4), (0.5, 0.7), "tooth", "up"),
                       scatter(sh_caps(1.6, 2.4, 0.5), (3, 5), (0.5, 0.7), "vein", "up", 1.6), single(sh_ring(1.4, 1.8, 0.7), (0.8, 1.0), "flesh", "up"), single(sh_ell(1.4, 1.9, 0.9, 1.2), (1.0, 1.4), "egg", "mound", tiers=SOFTT), scatter(sh_tri(0.6, 0.9), (4, 6), (0.8, 1.2), "tooth", "mound", 1.6, ((0.6, 4), (0.4, 35))), single(sh_disc(1.4, 1.8, 24), (0.5, 0.7), "flesh", "up", pat_hole(0.6, 0.9)),
                       single(sh_rect(2.0, 2.6, 1.2, 1.5), (0.7, 0.9), "iron", "up", pat_slots(2, 0.3)), scatter(sh_caps(0.7, 1.0, 0.3), (4, 7), (0.3, 0.35), "iron", "up", 1.4)]),
        "frontier": pad([x for t in T["frontier"] for x in t] + G("crate", "board", "iron") + N("rock", "pebble", True, "water", "shrub") + [single(sh_rect(1.0, 1.3, 1.0, 1.3), (2.0, 2.6), "post", "up", pat_cross()), scatter(sh_crescent(0.5, 0.65), (3, 6), (0.3, 0.35), "iron", "up", 1.5), single(sh_cross(2.4, 3.0, 0.6), (0.5, 0.7), "board", "up")]),
        "graveyard": pad([x for t in T["graveyard"] for x in t] + N("rock", "pebble", True, "water", "shrub") + G("coffin", "wood", "iron")[:5] + [single(sh_rect(2.4, 3.0, 1.6, 2.0), (0.4, 0.5), "headstone", "up", pat_rune()), sunk(sh_rect(2.4, 3.2, 1.4, 1.8), (0.6, 0.8), "pit"), scatter(sh_disc(0.5, 0.65, 12), (3, 5), (0.5, 0.7), "shroom", "mound", 1.4, ((0.5, 5), (0.5, 50))), single(sh_disc(0.9, 1.1, 16), (1.6, 2.2), "headstone", "up", pat_cross())]),
        "forest": pad([x for t in T["forest"] for x in t] + N("rock", "pebble", True, "water", "shrub") + [scatter(sh_caps(2.2, 3.2, 0.5), (2, 4), (0.45, 0.55), "log", "up", 1.8), scatter(sh_star(0.6, 0.8, 5, 6), (4, 7), (0.5, 0.7), "grass", "mound", 1.6, SOFTT), single(sh_disc(1.0, 1.3, 16), (0.8, 1.1), "shroom", "mound", tiers=((0.5, 5), (0.5, 50))), scatter(sh_blob(0.7, 1.0), (2, 4), (0.5, 0.7), "moss", "mound", 1.8, SOFTT),
                        path(0.6, (6, 9), 0.45, "root", "up", 1.4), scatter(sh_disc(0.35, 0.45, 10), (5, 9), (0.3, 0.4), "pebble", "mound", 1.3, ((0.6, 15), (0.4, 45))), single(sh_ring(1.3, 1.7, 0.6), (0.6, 0.8), "log", "up"), trail(1.2, (5, 8), sh_crescent(0.35, 0.45), 0.3, "pit")]),
        "dungeon": pad([x for t in T["dungeon"] for x in t] + G("crate", "wood", "iron") + N("flag", "pebble", True, "water", None) + [single(sh_rect(1.2, 1.5, 1.2, 1.5), (1.8, 2.4), "flag", "up", pat_cross()), scatter(sh_ring(0.7, 0.9, 0.3), (2, 4), (0.3, 0.35), "iron", "up", 1.6), sunk(sh_rect(2.2, 2.8, 2.2, 2.8), (0.5, 0.7), "pit"), single(sh_disc(1.0, 1.2, 20), (1.2, 1.6), "wood", "up", pat_ring(0.25))]),
        "temple": pad([x for t in T["temple"] for x in t] + N("tcolumn", "pebble", True, "water", "shrub") + G("crate", "wood", "gold")[:4] + [single(sh_rect(2.8, 3.6, 2.8, 3.6), (0.4, 0.5), "tflag", "up", pat_rune()), single(sh_disc(1.5, 1.9, 24), (0.5, 0.7), "tflag", "up", pat_rune()), scatter(sh_hex(0.6, 0.85), (3, 5), (0.5, 0.7), "tcolumn", "up", 1.6), sunk(sh_star(1.4, 1.9, 6, 8), (0.3, 0.4), "glyph"), single(sh_tri(1.6, 2.0), (1.4, 1.9), "tcolumn", "up", pat_rune())]),
        "coast": pad([x for t in T["coast"] for x in t] + N("shelf", "pebble", True, "water", "weed") + G("crate", "drift", "iron")[:6] + [scatter(sh_disc(0.4, 0.55, 12), (5, 9), (0.3, 0.4), "shell", "mound", 1.3, ((0.6, 15), (0.4, 45))), single(sh_ell(1.2, 1.5, 0.8, 1.0), (0.6, 0.8), "shell", "mound", pat_seams(0.5), tiers=((0.5, 15), (0.5, 45))), sunk(sh_disc(1.2, 1.7, 20), (0.4, 0.6), "water"), scatter(sh_caps(1.8, 2.6, 0.45), (3, 5), (0.4, 0.5), "drift", "up", 1.8)]),
    })
