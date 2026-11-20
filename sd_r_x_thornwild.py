"""Thornwild's own details (see style_thornwild.py and style_details.py): the carnivorous plants, the pods and spores,
the glowing fungi, the roots and thorn vines of a deadly alien jungle, and the bones and gear of the ones it caught.

Everything is one height per spot (nothing overhangs): a jaw lies open facing the sky, a vine creeps along the ground,
a thorn is a cone standing on its vine, a fungus cap is carried down to the ground under its rim on a thick stalk."""
import math
from sd_reg import d
from sd_r_x_frontier import x_d


# ------------------------------------------------------------------------------------------------ shared drawing
def tw_wedge(poly, hinge, run, z0, z1, n=6):
    """a solid that rises from z0 at the hinge line to z1 at `run` from it: the open inner face of a jaw lobe"""
    def sect(z):
        if z >= z1: return None
        if z <= z0: return poly
        g = poly.difference(hinge.buffer(run * (z - z0) / (z1 - z0), 6))
        return g if not g.is_empty else None
    return [(sect, z1)]


def tw_jaws(o, p, t, L, W, z0, z1, teeth=True, par=None):
    """a pair of open jaw lobes on the line through p along t: each a half-oval, its inner face sloping up from the
    hinge to a raised rim, a row of teeth along the rim, the midrib low in the middle"""
    ff = frame(p[0], p[1], t); hinge = LineString([ff(-L / 2, 0), ff(L / 2, 0)]); lobes = []
    for sv in (-1, 1):
        half = ell(p[0], p[1], L / 2, W, t, 20).intersection(Polygon([ff(-L, 0.12 * sv), ff(L, 0.12 * sv), ff(L, W * 2 * sv), ff(-L, W * 2 * sv)]))
        lobes += tw_wedge(half, hinge, W * 0.85, z0, z1)
    i = o.st(lobes, "maw", foot=ell(p[0], p[1], L / 2, W, t, 20))
    o.st(rod([ff(-L / 2 + 0.1, 0), ff(L / 2 - 0.1, 0)], 0.3, 1.2, lift=0.0) + cyl(p[0], p[1], 0.55, z0 * 0.9), "creeper")
    if teeth:
        ts = []
        for sv in (-1, 1):
            m = max(3, int(L / 0.62))
            for k in range(m):
                q = -0.82 + 1.64 * (k + 0.5) / m; x = q * L / 2; y = sv * W * math.sqrt(max(0.0, 1 - q * q)) * 0.98
                ts += cone(*ff(x, y), 0.3, z1 + 0.55, 0.07, n=8)
        o.st(ts, "tooth", foot=ell(p[0], p[1], L / 2 + 0.3, W + 0.3, t, 16))
    return i


def tw_leaves(o, p, rng, n, L0, L1, w=0.62, kind="creeper", t0=None):
    """a rosette of long leaves lying out from p, each a low ridge, its midrib cut along it"""
    u = rng.uniform; ps = []; cuts = []; feet = []; t0 = u(0, 6.28) if t0 is None else t0
    for k in range(n):
        t = t0 + 6.2832 * k / n + u(-0.25, 0.25); L = u(L0, L1); c = pol(p[0], p[1], L / 2 + 0.3, t)
        ps += spin(c[0], c[1], t, L, lambda q, L=L: w * math.sin(max(0.05, min(0.95, (q + L / 2) / L)) ** 0.8 * math.pi) + 0.05, 0.55, prof="ridge")
        feet.append(disc(*pol(p[0], p[1], L + 0.3, t), 0.3))
    o.st(ps, kind, foot=hull(feet + [disc(p[0], p[1], 0.6)]))
    return feet


def tw_vine(pts, r, rng, thorns=True, step=0.75, th=0.9):
    """a creeping vine along pts with thorns standing up off it, alternating sides"""
    ln = LineString(pts); ps = rod(pts, r, 1.2); ts = []
    if thorns:
        k = 0
        for u_ in np.arange(0.4, ln.length - 0.3, step):
            a = ln.interpolate(u_); b = ln.interpolate(min(ln.length, u_ + 0.05)); t = math.atan2(b.y - a.y, b.x - a.x) + (1.5708 if k % 2 else -1.5708)
            q = pol(a.x, a.y, r * 0.75, t); ts += cone(q[0], q[1], 0.32, r * 1.2 + th * rng.uniform(0.8, 1.15), 0.06, n=8); k += 1
    return ps, ts


def tw_curl(p, t, L, bend, n=7):
    """the points of a line from p heading t that bends more and more as it goes (a tendril, a shoot)"""
    pts = [p]; x, y = p
    for i in range(n):
        t += bend * (0.4 + 1.2 * i / n); x += L / n * math.cos(t); y += L / n * math.sin(t); pts.append((x, y))
    return pts


def tw_cap(x, y, r, h, stalk=0.45):
    """a mushroom: a stalk swelling into a domed cap, carried down to the ground under its rim"""
    return turn(x, y, [(stalk * 1.25, 0.0), (stalk, h * 0.35), (r, h * 0.62)] + crv(r, 0.2, h * 0.62, h, True, 5), 14)


# ====================================================================================================== carnivorous plants
@x_d("thornwild", "growth", "maw", "maw_flower")
def _(cx, cy, a, rng):
    """a great carnivorous flower: a rosette of long leaves on the ground, from its heart a swollen bulb holding two
    open jaw lobes, their inner faces sloping up to rims set with a row of long teeth; a half-eaten bone beside it
    on some"""
    u = rng.uniform; o = Obj("maw"); L = u(3.0, 3.6); W = u(1.25, 1.5)
    tw_leaves(o, (cx, cy), rng, rng.randint(5, 7), 2.6, 3.4, 0.7)
    o.st(dome(cx, cy, 1.2, 0.9, n=16), "creeper")
    tw_jaws(o, (cx, cy), a, L, W, 0.85, 1.85)
    if rng.random() < 0.5:
        t = a + u(1.2, 1.9); b0 = pol(cx, cy, W + 1.3, t); b1 = pol(b0[0], b0[1], 1.8, t + u(1.2, 1.9))
        o.st(rod([b0, b1], lambda q: 0.32 + 0.12 * (abs(q - 0.5) > 0.38), 1.1), "bone", foot=capl(b0, b1, 0.7))
    return o.out()

@x_d("thornwild", "growth", "maw", "snap_traps")
def _(cx, cy, a, rng):
    """a clump of small snap traps: three or four pairs of jaw lobes on short stems, each lined with bristles, round
    a knot of leaves"""
    u = rng.uniform; o = Obj("maw"); n = rng.randint(3, 4)
    tw_leaves(o, (cx, cy), rng, rng.randint(4, 6), 1.4, 2.0, 0.45)
    for k in range(n):
        t = a + 6.2832 * k / n + u(-0.3, 0.3); p = pol(cx, cy, u(1.3, 1.7), t)
        tw_jaws(o, p, t + u(-0.5, 0.5) + 1.5708, u(1.5, 1.8), u(0.62, 0.75), 0.45, 1.0)
    return o.out()

@x_d("thornwild", "growth", "pitcher", "pitcher_cluster")
def _(cx, cy, a, rng):
    """pitcher plants: two to four tall swollen tubes, each with a flared lip ringed by a rib and a dark well of
    digesting sap inside, leaves at their feet"""
    u = rng.uniform; o = Obj("pitcher"); n = rng.randint(2, 4); feet = []
    tw_leaves(o, (cx, cy), rng, rng.randint(3, 5), 1.6, 2.3, 0.5)
    for k in range(n):
        p = (cx, cy) if k == 0 else pol(cx, cy, u(1.5, 1.9), a + 6.2832 * k / max(1, n - 1)); r = u(0.75, 0.95) * (1.0 if k == 0 else 0.85); h = u(2.2, 3.0) * (1.0 if k == 0 else 0.8)
        if any(math.dist(p, q) < r + 0.9 for q in feet): continue
        feet.append(p)
        prof = [(r * 1.1, 0.0), (r * 1.2, h * 0.25), (r, h * 0.6), (r * 0.82, h * 0.82), (r * 1.08, h * 0.95), (r * 1.08, h)]
        o.st(turn(p[0], p[1], prof, 18), cuts=[(disc(p[0], p[1], r * 0.6, 14), h * 0.55), (ann(p[0], p[1], r * 0.98, r * 0.82, 18), h * 0.93)], foot=disc(p[0], p[1], r * 1.2))
    return o.out()

@x_d("thornwild", "growth", "sundew", "sundew_rosette")
def _(cx, cy, a, rng):
    """a sticky-leaf trap: a flat rosette of spoon-shaped leaves, each crowded with short beaded tentacles; one leaf
    rolled in over a caught insect"""
    u = rng.uniform; o = Obj("sundew"); n = rng.randint(5, 7); pads = []; ts = []; feet = []
    for k in range(n):
        t = a + 6.2832 * k / n + u(-0.2, 0.2); L = u(1.6, 2.2); c = pol(cx, cy, L, t)
        pads += rod([(cx, cy), c], 0.2, 1.0) + ext(disc(c[0], c[1], u(0.65, 0.8), 12), 0.3); feet.append(disc(c[0], c[1], 0.85))
        for j in range(rng.randint(3, 4)):
            q = pol(c[0], c[1], u(0.15, 0.5), u(0, 6.28)); ts += cone(q[0], q[1], 0.25, u(0.65, 0.85), 0.12, n=6)
    o.st(pads + dome(cx, cy, 0.5, 0.45, n=10), "creeper", foot=hull(feet))
    o.st(ts, "sundew")
    if rng.random() < 0.5:
        t = a + u(0, 6.28); q = pol(cx, cy, u(0.9, 1.3), t); o.st(spin(q[0], q[1], t, 1.3, lambda v: 0.42, 1.2), "chitin", foot=disc(q[0], q[1], 0.7))
    return o.out()


# ====================================================================================================== pods and spores
@x_d("thornwild", "growth", "sporepod", "spore_pods")
def _(cx, cy, a, rng):
    """a clutch of swollen spore pods: fat bulbs drawing in to a puckered crown with a pore in it, ribs running down
    their sides, a creeping root joining them"""
    u = rng.uniform; o = Obj("sporepod"); n = rng.randint(3, 5); feet = []; cuts = []; ps = []
    for k in range(n * 4):
        if len(feet) >= n: break
        r = u(0.8, 1.15); p = (cx, cy) if not feet else pol(cx, cy, u(1.4, 2.4), u(0, 6.28))
        if any(math.dist(p, q) < r + rq + 0.2 for q, rq in feet): continue
        h = r * u(1.5, 1.9); feet.append((p, r))
        ps += turn(p[0], p[1], [(r * 0.85, 0.0), (r, h * 0.35), (r * 0.9, h * 0.62), (r * 0.5, h * 0.86), (r * 0.25, h)], 16)
        cuts += [(disc(p[0], p[1], 0.24, 8), h * 0.72), (ann(p[0], p[1], r * 0.6, r * 0.44, 14), h * 0.8)]          # the pore and the pucker round it
    o.st(ps, cuts=cuts, foot=hull([disc(p[0], p[1], r) for p, r in feet]))
    if len(feet) > 1:
        rs = []
        for (p, r), (q, rq) in zip(feet, feet[1:]): rs += rod(wiggle(p, q, rng, 0.3, 3), 0.32, 1.1)
        o.st(rs, "root")
    return o.out()

@x_d("thornwild", "growth", "sporepod", "burst_pod")
def _(cx, cy, a, rng):
    """a pod that has burst: the lower half of the bulb, the torn top peeled back into four or five petals lying out
    on the ground, the dark hollow inside, a spray of spores spilt round it"""
    u = rng.uniform; o = Obj("sporepod"); r = u(1.1, 1.35); h = r * u(1.0, 1.2); n = rng.randint(4, 5)
    body = turn(cx, cy, [(r * 0.9, 0.0), (r, h * 0.5), (r * 0.95, h)], 18)
    pet = []
    for k in range(n):
        t = a + 6.2832 * k / n + u(-0.2, 0.2); c = pol(cx, cy, r + 0.75, t)
        pet += spin(c[0], c[1], t, 1.9, lambda q: 0.55 * math.sin(max(0.05, min(0.95, (q + 0.95) / 1.9)) * math.pi) + 0.08, 0.6, prof="ridge")
    o.st(body + pet, cuts=[(disc(cx, cy, r * 0.62, 14), h * 0.3)], foot=disc(cx, cy, r + 1.7))
    sp = []
    for k in range(rng.randint(6, 9)):
        q = pol(cx, cy, u(r + 0.9, r + 2.3), u(0, 6.28)); sp += dome(q[0], q[1], u(0.28, 0.38), 0.4, n=8)
    o.st(sp, "spore", foot=disc(cx, cy, r + 2.5))
    return o.out()

@x_d("thornwild", "growth", "spore", "spore_stalks")
def _(cx, cy, a, rng):
    """a stand of spore stalks: thick fleshy stems standing in a cluster, each topped with a round spore head pitted
    with pores"""
    u = rng.uniform; o = Obj("spore"); n = rng.randint(3, 6); feet = []; ps = []; cuts = []; st = []
    for k in range(n * 4):
        if len(feet) >= n: break
        p = (cx, cy) if not feet else pol(cx, cy, u(1.0, 2.0), u(0, 6.28)); r = u(0.5, 0.7)
        if any(math.dist(p, q) < r + rq + 0.15 for q, rq in feet): continue
        feet.append((p, r)); h = u(1.7, 2.8)
        st += cone(p[0], p[1], r * 1.15, h * 0.7, r * 0.6, n=12)
        ps += turn(p[0], p[1], [(r * 0.7, h * 0.55), (r, h * 0.75)] + crv(r, 0.2, h * 0.75, h, True, 4), 14)
        cuts.append((disc(*pol(p[0], p[1], r * 0.45, u(0, 6.28)), 0.13, 6), h * 0.85))
    o.st(st, "sporepod", foot=hull([disc(p[0], p[1], r * 1.15) for p, r in feet]))
    o.st(ps, cuts=cuts)
    return o.out()

@x_d("thornwild", "debris", "seedburr", "seed_burrs")
def _(cx, cy, a, rng):
    """spiked seed burrs dropped from above: round seeds, each bristling with stubby spines"""
    u = rng.uniform; ps = []; feet = []
    for k in range(rng.randint(3, 6)):
        p = pol(cx, cy, 0 if not k else u(0.9, 2.0), u(0, 6.28)); r = u(0.45, 0.6)
        if any(math.dist(p, q) < r + 0.95 for q in feet): continue
        feet.append(p); ps += dome(p[0], p[1], r, r * 1.2, n=10)
        for j in range(6):
            q = pol(p[0], p[1], r * 0.85, u(0, 0.3) + j * 1.0472); ps += cone(q[0], q[1], 0.26, r * 1.25 + 0.35, 0.06, n=6)
    return hull([disc(p[0], p[1], 0.9) for p in feet]), [ST(ps, "seedburr")]


# ====================================================================================================== glowing fungi
@x_d("thornwild", "growth", "glowcap", "glowcap_ring")
def _(cx, cy, a, rng):
    """a ring of glowing caps round a bare patch of soil: seven to ten domed mushrooms of different sizes, gill lines
    cut round the rim of the biggest, a few tiny buttons coming up between them"""
    u = rng.uniform; R = u(2.2, 2.8); n = rng.randint(7, 10); ps = []; small = []; cuts = []
    for k in range(n):
        if rng.random() < 0.12: continue
        t = a + 6.2832 * k / n + u(-0.15, 0.15); p = pol(cx, cy, R + u(-0.2, 0.2), t); r = u(0.5, 0.75); h = u(0.9, 1.4)
        ps += tw_cap(p[0], p[1], r, h, r * 0.55)
        if r > 0.68: cuts.append((ann(p[0], p[1], r * 0.8, r * 0.62, 12), h * 0.72))
        if rng.random() < 0.5: q = pol(cx, cy, R + u(-0.6, 0.6), t + 3.1416 / n); small += dome(q[0], q[1], 0.3, 0.42, n=8)
    return ann(cx, cy, R + 1.0, max(0.2, R - 1.0)), [ST(ps, "glowcap", cuts=cuts), ST(small, "glowcap")]

@x_d("thornwild", "growth", "glowcap", "glow_stalks")
def _(cx, cy, a, rng):
    """a clump of tall glowing fungi: thick stalks of different heights, each crowned with a broad cap, the tallest
    with a ring of gill lines; small caps crowding their feet"""
    u = rng.uniform; ps = []; feet = []; cuts = []
    for k in range(rng.randint(3, 5) * 3):
        if len(feet) >= 5: break
        p = (cx, cy) if not feet else pol(cx, cy, u(2.0, 2.8), u(0, 6.28)); r = u(0.95, 1.3) if not feet else u(0.6, 0.9)
        if any(math.dist(p, q) < r + rq + 0.1 for q, rq in feet): continue
        h = u(2.4, 3.2) if not feet else u(1.3, 2.2); feet.append((p, r))
        ps += tw_cap(p[0], p[1], r, h, max(0.45, r * 0.45))
        cuts.append((ann(p[0], p[1], r * 0.82, r * 0.66, 14), h * 0.74))
    return hull([disc(p[0], p[1], r) for p, r in feet]), [ST(ps, "glowcap", cuts=cuts)]

@x_d("thornwild", "growth", "glowcap", "shelf_fungus_root")
def _(cx, cy, a, rng):
    """a length of fallen root crusted with glowing shelf fungi: the root with its bark furrows, three to five
    half-round brackets stepped up its side"""
    u = rng.uniform; ff = frame(cx, cy, a); L = u(4.6, 5.6); r = u(0.75, 0.9); o = Obj("rootbark")
    pts = [ff(-L / 2 + L * s, 0.3 * math.sin(4 * s + u(0, 1))) for s in np.linspace(0, 1, 6)]
    o.st(rod(pts, lambda q: r * (1.05 - 0.3 * q), 1.3), cuts=[(barl(ff(-L / 2, v), ff(L / 2, v), 0.14), r * 1.15) for v in (-0.3, 0.3)], foot=LineString(pts).buffer(r))
    br = []; sv = rng.choice((-1, 1))
    for k in range(rng.randint(3, 5)):
        x = -L * 0.35 + L * 0.7 * k / 4 + u(-0.2, 0.2); c = ff(x, sv * (r + 0.15)); rr = u(0.55, 0.8)
        half = ell(c[0], c[1], rr, rr * 0.7, a).difference(LineString(pts).buffer(r * 0.6))
        br += ext(half, r * 1.3 + u(0.0, 0.3))
    o.st(br, "glowcap")
    return o.out()


# ====================================================================================================== roots and thorns
@x_d("thornwild", "growth", "root", "root_knot")
def _(cx, cy, a, rng):
    """a knot of giant roots: three or four thick roots heaving up out of the ground and diving back in, crossing over
    one another, their bark furrowed, a thinner root curling off the end of one"""
    u = rng.uniform; ps = []; feet = []
    for k in range(rng.randint(3, 4)):
        t = a + k * u(0.6, 1.0); L = u(5.0, 6.5); p = pol(cx, cy, L / 2, t + 3.1416); q = pol(cx, cy, L / 2, t)
        pts = wiggle(p, q, rng, 0.6, 5); r0 = u(0.7, 0.95)
        ps += rod(pts, lambda s, r0=r0: r0 * (0.55 + 0.45 * math.sin(max(0.0, min(1.0, s)) * math.pi)) + 0.15, 1.5); feet.append(LineString(pts).buffer(r0))
    t = a + u(0, 6.28); e = pol(cx, cy, u(2.4, 3.0), t); ps += rod(tw_curl(e, t, 2.2, rng.choice((-1, 1)) * 0.5, 6), lambda s: 0.45 - 0.2 * s, 1.2)
    return hull(feet), [ST(ps, "root")]

@x_d("thornwild", "growth", "root", "buttress_root")
def _(cx, cy, a, rng):
    """a buttress root of a giant: a great blade of root flaring up out of the ground in a long curve, highest where
    it leaves the trunk, its sides furrowed, smaller roots splaying from its foot"""
    u = rng.uniform; ff = frame(cx, cy, a); L = u(6.5, 8.0); W = u(1.0, 1.3)
    pts = [ff(-L / 2 + L * s, 0.9 * math.sin(2.2 * s)) for s in np.linspace(0, 1, 9)]
    ps = rod(pts, lambda s: W * (1.0 - 0.6 * s) + 0.1, 2.4); feet = [LineString(pts).buffer(W + 0.3)]
    for k in range(rng.randint(2, 3)):
        p = pts[rng.randint(1, 5)]; t = a + rng.choice((-1, 1)) * u(0.9, 1.6); sp = tw_curl(p, t, u(1.8, 2.6), u(-0.4, 0.4), 4)
        ps += rod(sp, lambda s: 0.5 - 0.2 * s, 1.2); feet.append(LineString(sp).buffer(0.6))
    cuts = [(LineString([ff(-L / 2, v), ff(L / 2, v + 0.2)]).buffer(0.08), W * 1.6) for v in (-0.35, 0.35)]
    return hull(feet), [ST(ps, "root", cuts=cuts)]

@x_d("thornwild", "growth", "creeper", "thorn_vine")
def _(cx, cy, a, rng):
    """a thorn vine creeping over the ground: a long snaking stem, thorns standing up off it every little way, a
    side shoot or two curling off, a few leaves"""
    u = rng.uniform; p = pol(cx, cy, 3.2, a + 3.1416); pts = tw_curl(p, a + u(-0.3, 0.3), u(6.0, 7.0), rng.choice((-1, 1)) * u(0.15, 0.3), 9)
    ps, ts = tw_vine(pts, u(0.38, 0.45), rng)
    for k in range(rng.randint(1, 2)):
        q = pts[rng.randint(2, 6)]; sh = tw_curl(q, a + rng.choice((-1, 1)) * u(0.8, 1.3), u(1.8, 2.4), u(-0.6, 0.6), 4)
        p2, t2 = tw_vine(sh, 0.32, rng, step=0.8, th=0.7); ps += p2; ts += t2
    return LineString(pts).buffer(1.6), [ST(ps, "creeper"), ST(ts, "spikethorn")]

@x_d("thornwild", "growth", "spikethorn", "thorn_bramble")
def _(cx, cy, a, rng):
    """a bramble of thorn vines: five or six stems arching out of one clump and crossing over each other, thick with
    thorns"""
    u = rng.uniform; ps = dome(cx, cy, 0.9, 0.9, n=12); ts = []; feet = [disc(cx, cy, 1.0)]
    for k in range(rng.randint(5, 6)):
        t = a + k * 1.15 + u(-0.3, 0.3); pts = tw_curl((cx, cy), t, u(2.6, 3.6), rng.choice((-1, 1)) * u(0.3, 0.6), 5)
        p2, t2 = tw_vine(pts, 0.36, rng, step=0.65, th=0.8); ps += p2; ts += t2; feet.append(LineString(pts).buffer(0.6))
    return hull(feet), [ST(ps, "creeper"), ST(ts, "spikethorn")]

@x_d("thornwild", "growth", "creeper", "strangler_coil")
def _(cx, cy, a, rng):
    """a strangler vine wound round a boulder: the stone half swallowed, the vine coiling up and over it in a spiral,
    thorns along it"""
    u = rng.uniform; R = u(1.4, 1.8); hr = u(1.5, 2.0); o = Obj("rock")
    o.st(rock(cx, cy, R, hr, rng, 0.9, 9))
    pts = spiral(cx, cy, R + 0.15, R * 0.35, 1.4, a, 24); r = 0.36
    ps = []; ts = []
    for i in range(len(pts) - 1):
        d0 = math.dist(pts[i], (cx, cy)); z = hr * (1 - (d0 / (R + 0.2)) ** 2) * 0.95
        ps += rod([pts[i], pts[i + 1]], r, 1.2, lift=max(0.0, z - r * 0.6))
        if i % 3 == 1: ts += cone(pts[i][0], pts[i][1], 0.3, z + r * 1.2 + 0.6, 0.06, n=6)
    tail = tw_curl(pts[0], a - 1.5708, 2.2, 0.3, 4); p2, t2 = tw_vine(tail, 0.36, rng); ps += p2; ts += t2
    o.st(ps, "creeper", foot=LineString(tail).buffer(0.5)); o.st(ts, "spikethorn")
    return o.out()

@x_d("thornwild", "growth", "creeper", "grasping_tendrils")
def _(cx, cy, a, rng):
    """tendrils reaching out of a hole in the ground: four to six of them curling as they go, thick at the hole and
    tapering to coiled tips, suckers cut along them"""
    u = rng.uniform; r = u(0.9, 1.2); ps = []; cuts = []; feet = [disc(cx, cy, r + 0.5)]
    for k in range(rng.randint(4, 6)):
        t = a + 6.2832 * k / 5 + u(-0.3, 0.3); p = pol(cx, cy, r * 0.8, t); pts = tw_curl(p, t, u(2.6, 3.6), rng.choice((-1, 1)) * u(0.35, 0.6), 7)
        ps += rod(pts, lambda s: 0.55 - 0.28 * s, 1.2); feet.append(LineString(pts).buffer(0.5))
        cuts += [(disc(*pts[j], 0.12, 6), 0.42) for j in (2, 4)]
    return hull(feet), [("gcut", disc(cx, cy, r, 14), 0.6, "pit"), ST(ps, "creeper", cuts=cuts)]


# ====================================================================================================== the unlucky
@x_d("thornwild", "made", "bone", "rooted_ribcage")
def _(cx, cy, a, rng):
    """the ribcage of somebody who lay down here: the spine, curved ribs both sides, the skull rolled off to one side,
    and roots grown in between the ribs and over the spine"""
    u = rng.uniform; ff = frame(cx, cy, a); L = u(3.4, 4.0); o = Obj("bone")
    sp = rod([ff(-L / 2, 0), ff(L / 2, 0)], 0.32, 1.0); rb = []
    for k in range(5):
        x = -L * 0.35 + k * L * 0.16
        for sv in (-1, 1): rb += rod([ff(x, 0), ff(x + 0.45, sv * 1.0), ff(x + 0.25, sv * 1.65)], lambda q: 0.36 - 0.06 * q, 1.3)
    o.st(sp + rb, foot=rect(cx, cy, L + 0.4, 3.8, a))
    sk = ff(L / 2 + u(0.9, 1.3), rng.choice((-1, 1)) * u(0.4, 1.0))
    o.st(dome(sk[0], sk[1], 0.78, 1.0, sq=0.85, a=a, n=14), cuts=[(disc(*pol(sk[0], sk[1], 0.35, a + sv * 0.5), 0.2, 8), 0.55) for sv in (-1, 1)], foot=disc(sk[0], sk[1], 0.8))
    rt = []
    for k in range(rng.randint(2, 3)):
        p = ff(u(-L / 2, L / 2), -2.4); q = ff(u(-L / 2, L / 2), 2.4); rt += rod(wiggle(p, q, rng, 0.4, 4), 0.36, 1.5)
    o.st(rt, "root")
    return o.out()

@x_d("thornwild", "made", "metal", "lost_helmet")
def _(cx, cy, a, rng):
    """a lost helmet lying in the moss: a round dome with a brow ridge and a visor slot, a dent in it, a vine grown
    in through the slot and out over the top"""
    u = rng.uniform; ff = frame(cx, cy, a); r = u(1.1, 1.3); o = Obj("metal")
    o.st(dome(cx, cy, r, r * 1.05, sq=0.88, a=a, n=18) + ext(ell(*ff(r * 0.55, 0), r * 0.5, r * 0.95, a, 12), 0.4),
         cuts=[(rect(*ff(r * 0.55, 0), 0.3, r * 1.2, a), r * 0.55), (disc(*ff(-r * 0.3, r * 0.3), 0.3, 8), r * 0.8)])
    pts = tw_curl(ff(r * 1.6, -r), a + 2.2, 4.0, 0.4, 7); ps, ts = tw_vine(pts, 0.34, rng, step=0.9, th=0.6)
    o.st(ps, "creeper", foot=LineString(pts).buffer(0.5))
    o.st(ts, "spikethorn")
    return o.out()

@x_d("thornwild", "made", "leather", "dropped_pack")
def _(cx, cy, a, rng):
    """a dropped pack: the bag lying on its back with its flap, two side pockets and straps, a bedroll tied across
    the top, a tear in its side, a creeper starting to grow over it"""
    u = rng.uniform; ff = frame(cx, cy, a); L = u(2.6, 3.0); W = u(1.9, 2.2); o = Obj("leather")
    b = o.up(rrect(cx, cy, L, W, a, 0.5), u(0.9, 1.1))
    o.lid(rrect(*ff(L * 0.22, 0), L * 0.5, W, a, 0.45), b, 0.18)
    for sv in (-1, 1): o.up(rrect(*ff(-L * 0.1, sv * (W / 2 + 0.3)), L * 0.42, 0.6, a, 0.22), u(0.6, 0.75))          # the side pockets
    o.straps(ff, (-L * 0.25, L * 0.12), W, b, a, 0.32, 0.12, "leather")
    o.st(lie(*ff(-L / 2 - 0.4, 0), a + 1.5708, W + 0.4, 0.55), "cloth", foot=rect(*ff(-L / 2 - 0.4, 0), 1.2, W + 0.4, a))
    o.cut(barl(ff(L * 0.05, -W / 2 + 0.25), ff(L * 0.35, -W / 2 + 0.45), 0.24), b, 0.3)
    pts = tw_curl(ff(L / 2 + 1.2, W / 2), a + 3.6, 3.6, 0.45, 6); ps, ts = tw_vine(pts, 0.3, rng, step=1.0, th=0.5)
    o.st(ps, "creeper", foot=LineString(pts).buffer(0.4)); o.st(ts, "spikethorn")
    return o.out()

@x_d("thornwild", "made", "metal", "overgrown_rifle")
def _(cx, cy, a, rng):
    """a long gun dropped and left: the stock, the body with its sight block and trigger guard, the barrel, a strap
    lying loose, a vine wound over the barrel"""
    u = rng.uniform; ff = frame(cx, cy, a); L = u(5.6, 6.4); o = Obj("metal")
    st = o.up(Polygon([ff(-L / 2, -0.55), ff(-L / 2 + 1.5, -0.35), ff(-L / 2 + 1.5, 0.35), ff(-L / 2, 0.55)]), 0.6, "wood")
    bd = o.up(rect(*ff(-L / 2 + 2.3, 0), 1.7, 0.8, a), 0.75)
    o.on(rect(*ff(-L / 2 + 2.4, 0), 0.8, 0.5, a), bd, 0.3)
    o.up(rect(*ff(-L / 2 + 2.0, -0.6), 0.6, 0.5, a), 0.35)
    o.st(rod([ff(-L / 2 + 3.1, 0), ff(L / 2, 0)], 0.27, 1.6), foot=rect(*ff(0.9, 0), L - 3.1, 0.55, a))
    o.st(rod(wiggle(ff(-L / 2 + 0.4, 0.5), ff(-L / 2 + 3.4, 0.5), rng, 0.5, 4), 0.25, 0.8), "leather")
    pts = [ff(x, 0.9 * math.sin(x * 2.4)) for x in np.linspace(-0.2, L / 2 - 0.4, 10)]; ps, ts = tw_vine(pts, 0.3, rng, step=1.1, th=0.5)
    o.st(ps, "creeper", foot=LineString(pts).buffer(0.4)); o.st(ts, "spikethorn")
    return o.out()

@x_d("thornwild", "made", "metal", "machete")
def _(cx, cy, a, rng):
    """a broad jungle blade lying where it fell: the wide blade swelling to its tip, a notch in its back, the
    wrapped grip and the guard; chopped vines lying round it"""
    u = rng.uniform; ff = frame(cx, cy, a); L = u(3.6, 4.2); o = Obj("metal")
    blade = Polygon([ff(-L / 2 + 1.2, -0.3), ff(L / 2 - 0.6, -0.42), ff(L / 2, 0.1), ff(L / 2 - 0.3, 0.55), ff(-L / 2 + 1.2, 0.3)])
    b = o.up(blade, 0.3)
    o.up(rect(*ff(-L / 2 + 1.15, 0), 0.3, 1.0, a), 0.45)
    g = o.up(rect(*ff(-L / 2 + 0.5, 0), 1.0, 0.5, a), 0.5, "leather")
    o.cut(U([barl(ff(-L / 2 + 0.2 + k * 0.3, -0.4), ff(-L / 2 + 0.3 + k * 0.3, 0.4), 0.14) for k in range(3)]), g, 0.12)
    ps = []
    for k in range(rng.randint(2, 3)):
        p = ff(u(-L / 2, L / 2), rng.choice((-1, 1)) * u(1.0, 1.5)); t = a + u(-0.6, 0.6); ps += rod([pol(p[0], p[1], -0.7, t), pol(p[0], p[1], 0.7, t)], 0.3, 1.2)
    o.st(ps, "creeper", foot=rect(cx, cy, L, 3.2, a))
    return o.out()

@x_d("thornwild", "made", "metal", "field_lantern")
def _(cx, cy, a, rng):
    """a field lamp on its side: the round body with its cage bars, the cap and the ring handle, its glass broken out
    at one end, glowing spores settled round it"""
    u = rng.uniform; ff = frame(cx, cy, a); L = u(2.0, 2.4); r = u(0.62, 0.72); o = Obj("metal")
    o.st(lie(cx, cy, a, L, r), cuts=[(U([barl(ff(x, -r), ff(x, r), 0.2) for x in (-L * 0.22, 0.0, L * 0.22)]), r * 1.6)], foot=rect(cx, cy, L, 2 * r, a))
    o.st(lie(*ff(-L / 2 - 0.2, 0), a, 0.4, r * 1.05), foot=rect(*ff(-L / 2 - 0.2, 0), 0.4, 2 * r, a))
    o.st(rod([ff(-L / 2 - 0.4, -0.45), ff(-L / 2 - 1.0, 0), ff(-L / 2 - 0.4, 0.45)], 0.2, 1.0))
    sp = []
    for k in range(rng.randint(3, 5)):
        q = ff(u(-L / 2, L / 2 + 0.8), rng.choice((-1, 1)) * u(r + 0.5, r + 1.2)); sp += dome(q[0], q[1], 0.28, 0.35, n=8)
    o.st(sp, "glowcap", foot=rect(cx, cy, L + 1.2, 2 * r + 2.4, a))
    return o.out()

@x_d("thornwild", "made", "leather", "boot_and_bones")
def _(cx, cy, a, rng):
    """all that is left of one of them: a boot lying on its side with the leg bones still in it, its sole with the
    tread across it, laces trailing, a thorn shoot through the top"""
    u = rng.uniform; ff = frame(cx, cy, a); o = Obj("leather")
    sole = Polygon([ff(-1.2, -0.55), ff(1.0, -0.6), ff(1.5, -0.3), ff(1.55, 0.25), ff(1.0, 0.55), ff(-1.2, 0.5)])
    b = o.up(sole, 0.85)
    o.on(rrect(*ff(-0.75, 0), 0.9, 1.2, a, 0.3), b, 0.55)
    o.cut(U([barl(ff(x, -0.8), ff(x, 0.8), 0.18) for x in (0.2, 0.6, 1.0)]), b, 0.14)
    bn = rod([ff(-1.0, 0.0), ff(-3.2, u(-0.4, 0.4))], lambda q: 0.36 + 0.1 * (q > 0.85), 1.0)
    o.st(bn, "bone", foot=capl(ff(-1.0, 0), ff(-3.2, 0), 0.8))
    o.st(rod(wiggle(ff(0.2, 0.6), ff(0.6, 1.8), rng, 0.3, 3), 0.2, 0.9), "cloth")
    return o.out()

@x_d("thornwild", "made", "wood", "warning_stake")
def _(cx, cy, a, rng):
    """a crude marker the last ones left: a stake driven in, a cross bar lashed to it, a skull set at its foot, thorn
    vines already round it"""
    u = rng.uniform; ff = frame(cx, cy, a); o = Obj("wood")
    o.st(cone(cx, cy, 0.62, u(2.6, 3.2), 0.4, n=8), foot=disc(cx, cy, 0.62))
    o.st(lie(*ff(0, 0), a + 1.5708, u(2.4, 2.8), 0.35), foot=rect(cx, cy, 0.7, 2.8, a))
    sk = ff(u(1.0, 1.3), 0); o.st(dome(sk[0], sk[1], 0.62, 0.8, sq=0.85, a=a, n=12), "bone", cuts=[(disc(*pol(sk[0], sk[1], 0.3, a + sv * 0.5), 0.16, 6), 0.45) for sv in (-1, 1)], foot=disc(sk[0], sk[1], 0.62))
    pts = tw_curl(ff(-1.6, -1.0), a + 0.8, 3.8, 0.5, 6); ps, ts = tw_vine(pts, 0.32, rng, step=0.9, th=0.6)
    o.st(ps, "creeper", foot=LineString(pts).buffer(0.4)); o.st(ts, "spikethorn")
    return o.out()


# ====================================================================================================== marks
@x_d("thornwild", "marks", "pit", "drag_gouges", ground=True)
def _(cx, cy, a, rng):
    """where something was dragged under: three or four parallel gouges clawed into the soil, ending in a dark hole
    among roots"""
    u = rng.uniform; ff = frame(cx, cy, a); L = u(4.0, 5.0)
    g = U([LineString(wiggle(ff(-L / 2, v), ff(L / 2 - 0.6, v * 0.6), rng, 0.15, 3)).buffer(0.2, 3) for v in np.linspace(-0.7, 0.7, rng.randint(3, 4))])
    hole = disc(*ff(L / 2, 0), u(0.75, 0.95), 12)
    rt = []
    for sv in (-1, 1): rt += rod([ff(L / 2 - 0.4, sv * 1.1), ff(L / 2 + 0.6, sv * 0.9), ff(L / 2 + 1.3, sv * 0.2)], lambda q: 0.4 - 0.15 * q, 1.2)
    return hull([g, hole.buffer(0.4)]), [("gcut", g, 0.3, "crack"), ("gcut", hole, 0.7, "pit"), ST(rt, "root")]

@x_d("thornwild", "marks", "bile", "sap_seep", ground=True)
def _(cx, cy, a, rng):
    """a seep of digesting sap: an oozing puddle with a raised mossy crust round its edge"""
    u = rng.uniform; p = blob(cx, cy, u(1.3, 1.8), rng, 4)
    rim = p.buffer(0.45).difference(p.buffer(0.05))
    return p.buffer(0.5), [("gcut", p, 0.45, "bile"), ("mound", rim, 0.25, "moss", ((0.6, 20), (0.4, 50)))]


_OWN = set(globals())
