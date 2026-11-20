"""Third drawing, obelis (see sd_r_x_frontier.py): the tomb world's details at their real size beside a 32 mm figure
(1 m = 18 mm printed; scale/real_obelis.json). Everything is clean angular geometry built in flat layers so its
edges stay crisp: chamfered blocks, stepped plinths, slots and rings, the glyphs of the tomb script (style_obelis.
ob_glyph) cut into the tops. Only the sand is rounded. Nothing hangs: a toppled pylon lies on its side, a capstone
stands on its base, a casket's lid lies on the ground beside it."""
import math
from sd_reg import d
from sd_r_x_frontier import x_d
from style_obelis import ob_glyph, ob_band, _chamfer, _oct


def xo_drift(o, x, y, r, h, rng):
    """sand blown against a thing: a low rounded bank"""
    o.st(dome(x, y, r, h, sq=rng.uniform(0.5, 0.75), a=rng.uniform(0, 3.14)), "sand", foot=disc(x, y, r * 0.8))

def xo_chips(o, ff, xs, vs, rng, kind="obsidian", n=(2, 3)):
    """a few flat shards of polished stone"""
    for _ in range(rng.randint(*n)):
        c = ff(rng.uniform(*xs), rng.uniform(*vs)); r = rng.uniform(0.6, 0.85)
        o.up(Polygon([pol(c[0], c[1], r * rng.uniform(0.8, 1.15), k * 1.57 + rng.uniform(-0.35, 0.35)) for k in range(4)]), rng.uniform(0.35, 0.5), kind)


# ====================================================================================================== monoliths
@x_d("obelis", "made", "monolith", "fallen_monolith")
def _(cx, cy, a, rng):
    """a great monolith fallen flat and broken in two: the long slab with its chamfered top edge, the rows of glyphs
    cut into the face that was the front, the break straight and clean, chips of black stone in the gap, sand blown
    over one end"""
    u = rng.uniform; ff = frame(cx, cy, a); L = u(8.4, 9.2); W = u(2.0, 2.4); h = u(1.3, 1.6); o = Obj("monolith")
    xb = u(-0.2, 0.2) * L; gap = u(0.35, 0.6); tb = u(-0.25, 0.25)
    cutl = Polygon([ff(xb - W * math.tan(tb) / 2, -W), ff(xb + W * math.tan(tb) / 2, W), ff(L, W), ff(L, -W)])
    whole = _chamfer(rect(cx, cy, L, W, a), 0.3)
    for g, dx, db in ((whole.difference(cutl), 0.0, 0.0), (whole.intersection(cutl), gap, u(-0.1, 0.1))):
        if g.is_empty: continue
        c0 = g.centroid; g = affinity.rotate(affinity.translate(g, dx * math.cos(a), dx * math.sin(a)), db, origin=(c0.x, c0.y), use_radians=True)
        t = a + db; c = g.centroid; b = o.up(g, h * (1.0 if dx == 0 else u(0.9, 1.0))); tp = o.on(inset(g, 0.28), b, 0.2)
        f2 = frame(c.x, c.y, t); Lp = math.sqrt(g.area / W) * 1.6
        o.cut(ob_band(c.x, c.y, t, Lp, 1.2, rng, 0.24), tp, 0.14)
    xo_chips(o, ff, (xb - 0.3, xb + 0.6), (-W / 2 - 0.7, W / 2 + 0.7), rng)
    if rng.random() < 0.7: e = ff(rng.choice((-1, 1)) * (L / 2 + u(-0.2, 0.3)), u(-0.4, 0.4)); xo_drift(o, e[0], e[1], u(1.5, 2.0), u(0.7, 0.95), rng)
    return o.out()

@x_d("obelis", "made", "monolith", "standing_stub")
def _(cx, cy, a, rng):
    """the stump of a monolith still standing on its plinth of two steps: the slab sheared off at an angle so one end
    stands higher than the other, a ruled band round each step, the broken piece's chips on the plinth"""
    u = rng.uniform; ff = frame(cx, cy, a); P = u(3.4, 3.9); o = Obj("monolith"); g = _chamfer(rect(cx, cy, P, P, a), 0.25)
    b = o.up(g, 0.4); o.groove(inset(g, 0.2), b, 0.15, 0.12); s2 = o.on(inset(g, 0.5), b, 0.35); o.groove(inset(g, 0.7), s2, 0.15, 0.12)
    sw, sl = u(1.1, 1.3), u(2.0, 2.4); slab = _chamfer(rect(cx, cy, sw, sl, a), 0.18); lo = o.on(slab, s2, u(1.1, 1.4))
    hi = o.on(slab.intersection(rect(*ff(0, sl / 4), sw * 2, sl / 2, a)), lo, u(0.35, 0.6))
    o.cut(rect(*ff(0, -sl / 4), sw * 0.5, 0.22, a), lo, 0.12)
    for _ in range(rng.randint(1, 2)):
        c = ff(rng.choice((-1, 1)) * u(1.0, 1.3), u(-1.2, 1.2)); o.on(Polygon([pol(c[0], c[1], 0.35, k * 2.1 + u(-0.3, 0.3)) for k in range(3)]), s2, 0.25, "obsidian")
    return o.out()

@x_d("obelis", "made", "monolith", "glyph_stele")
def _(cx, cy, a, rng):
    """a square stele lying flat: a broad ruled margin, and four glyphs in a two-by-two grid cut into its face; a
    corner knocked off some"""
    u = rng.uniform; ff = frame(cx, cy, a); P = u(4.2, 4.8); o = Obj("monolith"); g = _chamfer(rect(cx, cy, P, P * u(0.9, 1.0), a), 0.25)
    if rng.random() < 0.35: sx, sy = rng.choice((-1, 1)), rng.choice((-1, 1)); g = g.difference(Polygon([ff(sx * P / 2, sy * P / 2), ff(sx * (P / 2 - u(0.8, 1.2)), sy * P / 2), ff(sx * P / 2, sy * (P / 2 - u(0.8, 1.2)))]).buffer(0.05))
    b = o.up(g, u(0.6, 0.8)); o.groove(inset(g, 0.3), b, 0.2, 0.15)
    cell = P * 0.34; gl = [ob_glyph(*ff(i * cell * 0.6, j * cell * 0.6), cell * 0.95, a, rng, 0.26) for i in (-1, 1) for j in (-1, 1)]
    o.cut(U(gl).intersection(inset(g, 0.55)), b, 0.18)
    return o.out()

@x_d("obelis", "made", "monolith", "capstone")
def _(cx, cy, a, rng):
    """the fallen capstone of a monolith standing on its base: a square pyramid in four crisp steps, the glyph band
    round its lowest step, a slot across its peak"""
    u = rng.uniform; P = u(3.4, 3.8); o = Obj("monolith"); g = _chamfer(rect(cx, cy, P, P, a + u(-0.1, 0.1)), 0.15); st = P * 0.13
    b = o.up(g, 0.5); o.groove(inset(g, 0.22), b, 0.16, 0.12); i = b
    for k in range(1, 4):
        i = o.on(inset(g, st * k), i, 0.45)
    ff = frame(cx, cy, a); o.cut(rect(cx, cy, P * 0.25, 0.22, a), i, 0.15)
    return o.out()

@x_d("obelis", "made", "monolith", "obelisk_tip")
def _(cx, cy, a, rng):
    """the broken tip of an obelisk lying on its side: the long shaft narrowing to its point, the ridge of its upper
    face, a glyph band along the shaft, the break square across the thick end"""
    u = rng.uniform; ff = frame(cx, cy, a); L = u(6.6, 7.4); W = u(1.9, 2.2); o = Obj("monolith")
    g = Polygon([ff(-L / 2, -W / 2), ff(L * 0.22, -W * 0.42), ff(L / 2, 0), ff(L * 0.22, W * 0.42), ff(-L / 2, W / 2)])
    b = o.up(g, u(0.9, 1.1)); r = o.on(Polygon([ff(-L / 2 + 0.2, -W * 0.2), ff(L * 0.2, -W * 0.14), ff(L / 2 - 0.6, 0), ff(L * 0.2, W * 0.14), ff(-L / 2 + 0.2, W * 0.2)]), b, 0.3)
    o.cut(U([ob_glyph(*ff(x, -W * 0.32), 0.95, a, rng, 0.2) for x in np.arange(-L / 2 + 0.8, L * 0.1, 1.05)]).intersection(inset(g, 0.2)), b, 0.12)
    xo_chips(o, ff, (-L / 2 - 0.8, -L / 2 - 0.3), (-W / 2, W / 2), rng)
    return o.out()


# ====================================================================================================== pylons
@x_d("obelis", "made", "monolith", "half_pylon")
def _(cx, cy, a, rng):
    """a pylon sunk to its waist in the sand: the eight-sided collar, the shaft rising from it with two ruled bands,
    the slot through its head, the sand banked up its side"""
    u = rng.uniform; r = u(1.5, 1.8); o = Obj("monolith"); g = _oct(cx, cy, r, a)
    b = o.up(g, u(0.5, 0.6)); o.groove(_oct(cx, cy, r - 0.25, a), b, 0.15, 0.12)
    sh = o.on(_oct(cx, cy, r * 0.66, a), b, u(1.6, 2.2)); o.cut(rect(cx, cy, r * 1.6, 0.3, a), sh, 0.25)
    o.groove(_oct(cx, cy, r * 0.66 - 0.22, a), sh, 0.14, 0.1)
    c = pol(cx, cy, r + u(0.6, 0.9), a + u(0, 6.28)); xo_drift(o, c[0], c[1], u(1.4, 1.9), u(0.6, 0.9), rng)
    return o.out()

@x_d("obelis", "made", "monolith", "toppled_pylon")
def _(cx, cy, a, rng):
    """a pylon toppled from its socket and lying on its side: the long shaft with chamfered edges, the collar bands
    standing proud round it, the slotted head at one end, the empty socket stub left standing beside its foot"""
    u = rng.uniform; ff = frame(cx, cy, a); L = u(7.4, 8.4); W = u(1.5, 1.8); o = Obj("monolith")
    g = _chamfer(rect(cx, cy, L, W, a), 0.35); b = o.up(g, u(1.0, 1.2)); tp = o.on(inset(g, 0.3), b, 0.15)
    for x in (-L * 0.2, L * 0.08):
        o.up(_chamfer(rect(*ff(x, 0), 0.55, W + 0.5, a), 0.12), u(1.25, 1.4))
    o.cut(U([rect(*ff(L / 2 - 0.6, v), 0.8, 0.22, a) for v in (-W * 0.18, W * 0.18)]), tp, 0.12)
    c = ff(-L / 2 - u(1.4, 1.8), u(-0.5, 0.5)); so = _oct(c[0], c[1], u(1.1, 1.3), a); sb = o.up(so, u(0.5, 0.7)); o.cut(_oct(c[0], c[1], 0.6, a), sb, 0.3)
    return o.out()

@x_d("obelis", "made", "monolith", "pylon_ring")
def _(cx, cy, a, rng):
    """the heads of a ring of pylons showing through the sand round a sunk metal disc: each a square block with its
    top chamfered and a slot across it, the disc between them ruled with rings"""
    u = rng.uniform; R = u(2.6, 3.0); n = rng.choice((4, 5, 6)); o = Obj("monolith")
    dc = o.up(disc(cx, cy, R * 0.55, 32), 0.3, "obmetal"); o.groove(disc(cx, cy, R * 0.42, 28), dc, 0.14, 0.1); o.cut(disc(cx, cy, R * 0.2, 20), dc, 0.15)
    for i in range(n):
        t = a + 6.2832 * i / n; c = pol(cx, cy, R, t); g = _chamfer(rect(c[0], c[1], 1.15, 1.15, t), 0.15)
        b = o.up(g, u(0.9, 1.4)); tp = o.on(inset(g, 0.22), b, 0.15); o.cut(rect(c[0], c[1], 0.7, 0.22, t), tp, 0.1)
    return o.out()


# ====================================================================================================== caskets
def _casket(o, cx, cy, a, L, W, rng, h=0.95):
    g = _chamfer(rect(cx, cy, L, W, a), 0.45); pl = o.up(g.buffer(0.25, join_style=2), 0.25)
    b = o.on(g, pl, h); return g, b

@x_d("obelis", "made", "casket", "casket")
def _(cx, cy, a, rng):
    """a sealed casket on its low plinth: the long body with its corners cut off, the lid a step smaller, a crest of
    two stepped lozenges along the lid, the seal line ruled round, a row of square bosses down each side"""
    u = rng.uniform; ff = frame(cx, cy, a); L = u(5.2, 5.8); W = u(2.0, 2.3); o = Obj("casket"); g, b = _casket(o, cx, cy, a, L, W, rng)
    lid = o.on(inset(g, 0.2), b, 0.22); o.groove(inset(g, 0.42), lid, 0.14, 0.1)
    cr = Polygon([ff(-L * 0.3, 0), ff(0, -W * 0.22), ff(L * 0.3, 0), ff(0, W * 0.22)]); c1 = o.on(cr, lid, 0.2); o.on(Polygon([ff(-L * 0.15, 0), ff(0, -W * 0.11), ff(L * 0.15, 0), ff(0, W * 0.11)]), c1, 0.18)
    o.cut(U([rect(*ff(x, sv * W * 0.32), 0.3, 0.3, a) for x in (-L * 0.32, L * 0.32) for sv in (-1, 1)]), lid, 0.1)
    return o.out()

@x_d("obelis", "made", "casket", "open_casket")
def _(cx, cy, a, rng):
    """a casket broken open: the body with its deep hollow and the step of the lid's seat round the rim, the lid
    pushed off and lying on the ground at a slant beside it, a crack across the lid"""
    u = rng.uniform; ff = frame(cx, cy, a); L = u(5.0, 5.6); W = u(2.0, 2.3); o = Obj("casket"); g, b = _casket(o, cx, cy, a, L, W, rng)
    o.cut(inset(g, 0.35), b, 0.2); o.cut(inset(g, 0.6), b, 0.75)
    sv = rng.choice((-1, 1)); c = ff(u(-0.5, 0.5), sv * (W + u(0.4, 0.7))); t = a + u(-0.35, 0.35); lg = _chamfer(rect(c[0], c[1], L - 0.3, W - 0.2, t), 0.4)
    ld = o.up(lg, 0.35); o.groove(inset(lg, 0.25), ld, 0.14, 0.1)
    o.cut(wavy(pol(c[0], c[1], L * 0.25, t + 1.4), pol(c[0], c[1], L * 0.25, t - 1.8), rng, 0.25, 3, 0.22), ld, 0.2)
    return o.out()

@x_d("obelis", "made", "casket", "vessel_row")
def _(cx, cy, a, rng):
    """a row of square vessels that stood by a casket: each a block with a stepped lid and a slot in it, one of them
    fallen on its side and its lid lying apart"""
    u = rng.uniform; ff = frame(cx, cy, a); n = rng.randint(3, 4); o = Obj("casket"); s = u(1.05, 1.2); fall = rng.randrange(n)
    for i in range(n):
        c = ff((i - (n - 1) / 2) * (s + 0.3), 0)
        if i == fall:
            g = _chamfer(rect(c[0], c[1] , s * 1.5, s, a + 1.5708 + u(-0.3, 0.3)), 0.12); b = o.up(g, s * 0.8); o.cut(_chamfer(rect(c[0], c[1], s * 1.1, s * 0.5, a + 1.5708), 0.1), b, 0.12)
            q = ff((i - (n - 1) / 2) * (s + 0.3), -s - u(0.4, 0.7)); o.up(_chamfer(rect(q[0], q[1], s * 0.9, s * 0.9, a + u(-0.5, 0.5)), 0.12), 0.3)
            continue
        g = _chamfer(rect(c[0], c[1], s, s, a), 0.12); b = o.up(g, u(1.3, 1.6)); l1 = o.on(inset(g, 0.12), b, 0.15); l2 = o.on(inset(g, 0.32), l1, 0.2)
        o.cut(rect(c[0], c[1], s * 0.4, 0.2, a), l2, 0.1)
    return o.out()


# ====================================================================================================== metal
@x_d("obelis", "made", "obmetal", "metal_shards")
def _(cx, cy, a, rng):
    """the broken panels of a smooth metal skin: two or three angular plates lying where they fell, each with a seam
    ruled in from its edge and a row of fastening holes, one torn edge of each straight and clean"""
    u = rng.uniform; o = Obj("obmetal"); gs = []
    for i in range(rng.randint(2, 3)):
        t = a + i * 2.1 + u(-0.3, 0.3); c = pol(cx, cy, u(0.6, 1.6) if i else 0.0, t + 1.0); f2 = frame(c[0], c[1], t); L, W = u(3.0, 3.8), u(1.8, 2.3)
        g = Polygon([f2(-L / 2, -W / 2), f2(L / 2, -W / 2), f2(L / 2 - u(0.5, 1.4), W / 2), f2(-L / 2 + u(0, 0.6), W / 2)])
        if any(g.distance(q) < 0.35 for q in gs): continue
        gs.append(g); b = o.up(g, u(0.45, 0.55)); o.cut(barl(f2(-L / 2 + 0.2, -W / 2 + 0.45), f2(L / 2 - 0.2, -W / 2 + 0.45), 0.16), b, 0.1)
        o.cut(U([disc(*f2(x, W / 2 - 0.5), 0.13, 6) for x in np.arange(-L / 2 + 0.6, L / 2 - 1.2, 0.55)]).intersection(inset(g, 0.25)), b, 0.1)
    return o.out()

@x_d("obelis", "made", "obmetal", "vent_panel")
def _(cx, cy, a, rng):
    """a heavy panel with its block of louvre slots, the frame ruled round it, four square bolt heads at the corners,
    one corner bent up as a step where it was torn free"""
    u = rng.uniform; ff = frame(cx, cy, a); L, W = u(3.6, 4.2), u(2.6, 3.0); o = Obj("obmetal"); g = _chamfer(rect(cx, cy, L, W, a), 0.2)
    b = o.up(g, u(0.4, 0.5)); o.groove(inset(g, 0.3), b, 0.15, 0.1); o.vents(ff, 0, 0, b, 5, l=L * 0.55, a=a, step=0.42, w=0.2, d=0.2)
    o.on(U([rect(*ff(sx * (L / 2 - 0.45), sy * (W / 2 - 0.45)), 0.4, 0.4, a) for sx in (-1, 1) for sy in (-1, 1)]), b, 0.15)
    sx, sy = rng.choice((-1, 1)), rng.choice((-1, 1)); o.on(Polygon([ff(sx * L / 2, sy * W / 2), ff(sx * (L / 2 - 1.2), sy * W / 2), ff(sx * L / 2, sy * (W / 2 - 1.2))]).intersection(inset(g, 0.13)), b, 0.3)
    return o.out()

@x_d("obelis", "made", "obmetal", "ring_segment")
def _(cx, cy, a, rng):
    """an arc broken from a great ring of metal, lying flat: the curved band with a raised rib along it, square
    notches cut at even steps across it, its broken ends square"""
    u = rng.uniform; R = u(4.0, 4.6); wd = u(1.0, 1.2); sp = u(1.3, 1.8); o = Obj("obmetal"); oc = pol(cx, cy, -R, a)
    band = Polygon(arc(oc[0], oc[1], R + wd / 2, a - sp / 2, a + sp / 2, 20) + arc(oc[0], oc[1], R - wd / 2, a + sp / 2, a - sp / 2, 20))
    b = o.up(band, u(0.45, 0.55)); rb = Polygon(arc(oc[0], oc[1], R + 0.15, a - sp / 2 + 0.05, a + sp / 2 - 0.05, 20) + arc(oc[0], oc[1], R - 0.15, a + sp / 2 - 0.05, a - sp / 2 + 0.05, 20))
    o.on(rb, b, 0.2)
    o.cut(U([rect(*pol(oc[0], oc[1], R + wd * 0.32, a + t), 0.32, 0.32, a + t) for t in np.linspace(-sp / 2 + 0.15, sp / 2 - 0.15, 6)]), b, 0.15)
    return o.out()

@x_d("obelis", "made", "obmetal", "conduit_run")
def _(cx, cy, a, rng):
    """a length of conduit lying in the sand: its square housing with a slot along the top showing the core bar,
    a junction box at one end with its hatch ruled in, the broken-off second length lying beside it at an angle"""
    u = rng.uniform; ff = frame(cx, cy, a); L = u(6.0, 7.0); o = Obj("obmetal")
    hs = o.up(_chamfer(rect(cx, cy, L, 0.95, a), 0.12), 0.6); o.cut(rect(cx, cy, L - 0.8, 0.36, a), hs, 0.3)
    jb = _chamfer(rect(*ff(L / 2 + 0.6, 0), 1.5, 1.6, a), 0.2); bx = o.up(jb, 0.9); o.groove(inset(jb, 0.25), bx, 0.14, 0.1)
    c = ff(u(-1.0, 0.5), rng.choice((-1, 1)) * u(1.1, 1.4)); t = a + u(0.25, 0.5); s2 = o.up(_chamfer(rect(c[0], c[1], u(2.6, 3.2), 0.95, t), 0.12), 0.6); o.cut(rect(c[0], c[1], 2.0, 0.36, t), s2, 0.3)
    return o.out()

@x_d("obelis", "made", "obmetal", "floor_hatch")
def _(cx, cy, a, rng):
    """a square hatch in the floor of the tomb: its frame, the leaves ruled in nested squares, the seam across them,
    four square studs at the corners and a lifting slot by the seam"""
    u = rng.uniform; ff = frame(cx, cy, a); P = u(3.4, 3.8); o = Obj("obmetal"); g = _chamfer(rect(cx, cy, P, P, a), 0.2)
    b = o.up(g, 0.35); o.groove(inset(g, 0.3), b, 0.16, 0.12); o.groove(inset(g, 0.75), b, 0.16, 0.12)
    o.cut(barl(ff(0, -P / 2 + 0.4), ff(0, P / 2 - 0.4), 0.18), b, 0.15); o.cut(rect(*ff(0.45, 0), 0.22, 0.7, a), b, 0.15)
    o.studs([ff(sx * (P / 2 - 0.5), sy * (P / 2 - 0.5)) for sx in (-1, 1) for sy in (-1, 1)], b, 0.2, 0.15)
    return o.out()

@x_d("obelis", "made", "obmetal", "star_dial")
def _(cx, cy, a, rng):
    """a great dial fallen from a pylon's head and half under the sand: the disc of metal with its rings, the even
    ticks round its rim, the square boss in the middle, sand drifted over one side"""
    u = rng.uniform; R = u(2.3, 2.6); o = Obj("obmetal"); b = o.up(ngon(cx, cy, R, 24, a), u(0.45, 0.55))
    o.groove(ngon(cx, cy, R - 0.35, 24, a), b, 0.14, 0.1); o.groove(ngon(cx, cy, R * 0.55, 16, a), b, 0.14, 0.1)
    o.cut(U([barl(pol(cx, cy, R * 0.62, a + 6.2832 * k / 12), pol(cx, cy, R - 0.55, a + 6.2832 * k / 12), 0.16) for k in range(12)]), b, 0.1)
    bs = o.on(_chamfer(rect(cx, cy, R * 0.45, R * 0.45, a + 0.785), 0.1), b, 0.25); o.cut(rect(cx, cy, R * 0.25, 0.2, a), bs, 0.1)
    c = pol(cx, cy, R * u(0.8, 1.0), a + u(0, 6.28)); xo_drift(o, c[0], c[1], u(1.5, 1.9), u(0.7, 0.9), rng)
    return o.out()


# ====================================================================================================== stone pieces
@x_d("obelis", "made", "monolith", "offering_dais")
def _(cx, cy, a, rng):
    """a small dais of three square steps, the top step dished into a square basin, a band of glyphs on the lowest
    step's tread, a square stud at each corner of the top"""
    u = rng.uniform; ff = frame(cx, cy, a); P = u(4.0, 4.4); o = Obj("monolith"); g = _chamfer(rect(cx, cy, P, P, a), 0.2)
    b = o.up(g, 0.4); s2 = o.on(inset(g, 0.6), b, 0.4); s3 = o.on(inset(g, 1.15), s2, 0.4)
    o.cut(inset(g, 1.6), s3, 0.25); o.groove(inset(g, 0.25), b, 0.14, 0.1)
    o.studs([ff(sx * (P / 2 - 1.3), sy * (P / 2 - 1.3)) for sx in (-1, 1) for sy in (-1, 1)], s3, 0.18, 0.12)
    return o.out()

@x_d("obelis", "made", "monolith", "glyph_cubes")
def _(cx, cy, a, rng):
    """cubes of black stone, each with a glyph cut into its top and a ruled margin: two or three lying together,
    one of them set on another"""
    u = rng.uniform; ff = frame(cx, cy, a); n = rng.randint(2, 3); o = Obj("monolith"); s = u(1.7, 1.9); tops = []
    for i in range(n):
        c = ff((i - (n - 1) / 2) * (s + 0.35), u(-0.25, 0.25)); t = a + u(-0.2, 0.2); g = _chamfer(rect(c[0], c[1], s, s, t), 0.12)
        b = o.up(g, u(1.1, 1.3)); tops.append((b, c, t, g))
    b, c, t, g = tops[rng.randrange(n)]
    if rng.random() < 0.6: g2 = _chamfer(rect(c[0], c[1], s * 0.75, s * 0.75, t + u(-0.4, 0.4)), 0.1); b2 = o.on(g2, b, u(0.8, 1.0)); tops.append((b2, c, t, g2))
    for b, c, t, g in tops:
        o.cut(ob_glyph(c[0], c[1], math.sqrt(g.area) * 0.72, t, rng, 0.2).intersection(inset(g, 0.2)), b, 0.12)
    return o.out()

@x_d("obelis", "made", "monolith", "lens_pedestal")
def _(cx, cy, a, rng):
    """an eight-sided pedestal of two steps with a lens of pale crystal set in its top: the six-sided lens standing
    proud in a square seat, a ruled ring round the seat"""
    u = rng.uniform; r = u(1.4, 1.6); o = Obj("monolith"); g = _oct(cx, cy, r, a)
    b = o.up(g, 0.5); s2 = o.on(_oct(cx, cy, r * 0.75, a), b, u(0.9, 1.1)); o.groove(_oct(cx, cy, r * 0.75 - 0.22, a), s2, 0.14, 0.1)
    o.on(ngon(cx, cy, r * 0.42, 6, a), s2, 0.45, "obglow")
    return o.out()

@x_d("obelis", "made", "monolith", "plinth_steps")
def _(cx, cy, a, rng):
    """a run of three broad steps of polished stone up to nothing, half buried, each tread with its nosing ruled in
    and its middle cut with a glyph; sand over the lowest"""
    u = rng.uniform; ff = frame(cx, cy, a); W = u(3.6, 4.2); td = u(1.1, 1.3); o = Obj("obfloor"); rise = u(0.45, 0.55)
    for i in range(3):
        x = (i - 1) * td; g = rect(*ff(x, 0), td + 0.02, W, a); b = o.up(g, rise * (i + 1))
        o.cut(barl(ff(x - td / 2 + 0.3, -W / 2 + 0.3), ff(x - td / 2 + 0.3, W / 2 - 0.3), 0.16), b, 0.12)
        if i: o.cut(ob_glyph(*ff(x + 0.15, 0), td * 0.75, a, rng, 0.18), b, 0.12)
    c = ff(-1.5 * td - u(0.3, 0.6), u(-W * 0.3, W * 0.3)); xo_drift(o, c[0], c[1], u(1.6, 2.1), u(0.8, 1.0), rng)
    return o.out()


# ====================================================================================================== natural, marks
@x_d("obelis", "debris", "sand", "sand_drift")
def _(cx, cy, a, rng):
    """a crescent of drifted sand: a long low bank and a smaller one in its lee"""
    u = rng.uniform; o = Obj("sand"); ff = frame(cx, cy, a)
    xo_drift(o, cx, cy, u(2.0, 2.6), u(0.7, 1.0), rng)
    c = ff(u(1.5, 2.2), u(-1.2, 1.2)); xo_drift(o, c[0], c[1], u(1.1, 1.5), u(0.45, 0.6), rng)
    return o.out()

@x_d("obelis", "debris", "obsidian", "stone_chips")
def _(cx, cy, a, rng):
    """shards of the polished stone, flat and sharp-edged, lying where a block was broken"""
    u = rng.uniform; o = Obj("obsidian"); ff = frame(cx, cy, a)
    xo_chips(o, ff, (-1.8, 1.8), (-1.2, 1.2), rng, "obsidian", (4, 6))
    return o.out()

@x_d("obelis", "marks", "slot", "glyph_marks", ground=True)
def _(cx, cy, a, rng):
    """a line of the tomb script cut straight into the ground stone"""
    u = rng.uniform; o = Obj("slot"); g = ob_band(cx, cy, a, u(5.0, 7.0), u(1.3, 1.5), rng, 0.25)
    o.gcut(g, 0.3, "slot")
    return o.out()

@x_d("obelis", "marks", "rut", "wind_ripples", ground=True)
def _(cx, cy, a, rng):
    """ripples the wind has drawn in the sand: shallow parallel lines, a little bowed"""
    u = rng.uniform; o = Obj("rut"); ff = frame(cx, cy, a); n = rng.randint(4, 6); st = u(0.8, 1.0); gs = []
    for i in range(n):
        v = (i - (n - 1) / 2) * st; L = u(3.0, 4.2)
        gs.append(LineString([ff(-L / 2, v), ff(0, v + u(0.2, 0.4)), ff(L / 2, v)]).buffer(0.17, cap_style=1))
    o.gcut(U(gs), 0.22, "rut")
    return o.out()


_OWN = set(globals())
