"""Second drawing, infestation (see sd_r.py for the rule). An alien hive is all grown things, so nearly everything
here goes through ST(); what makes each one itself is drawn in: the wrapping on a cocoon, the petals of an opened egg,
the segments of a grub, the spokes and rings of a web. Thin limbs and tall bodies never share an ST() (a low part in
a tall ST() comes out as a flat strip). The two made things (flame unit, tracker) are built crisp with Obj."""
import math
from sd_reg import d

def _pl(pts, s):
    """a value read off a broken line of (s, value) points"""
    for (s0, v0), (s1, v1) in zip(pts, pts[1:]):
        if s <= s1: return v0 + (v1 - v0) * (s - s0) / max(1e-9, s1 - s0)
    return pts[-1][1]

def _lean(x, y, r, h, dx, dy, n=10, tip=0.26, sq=1.0, a=0.0, pw=1.0):
    """a spike that leans: its point stands (dx, dy) away from the middle of its foot; nothing overhangs"""
    return [s for k in range(n) for s in ext(ell(x + dx * k / (n - 1), y + dy * k / (n - 1), max(tip, r * (1 - k / (n - 1)) ** pw), max(tip, r * (1 - k / (n - 1)) ** pw * sq), a, 12), h * (k + 1) / n)]

def _fin(x, y, t, Lf, h, w, rake=1.0, n=10):
    """a back spine as a raked fin: long at the foot, running up to a point over its back end (rake 1) or its middle (0)"""
    out = []
    for k in range(n):
        q = k / (n - 1); l = Lf + (0.5 - Lf) * q; c = pol(x, y, rake * (Lf - l) / 2, t); out += ext(rect(c[0], c[1], l, w + (0.5 - w) * q, t), h * (k + 1) / n)
    return out

def _curl(p, t, L, bend, n=10):
    """points of a line from p, setting off towards t and bending steadily by `bend` radians over its length L"""
    pts = [p]
    for k in range(n): t2 = t + bend * (k + 0.5) / n; pts.append((pts[-1][0] + L / n * math.cos(t2), pts[-1][1] + L / n * math.sin(t2)))
    return pts


@d("infestation", "growth", "web", "cocoon")
def _(cx, cy, a, rng):
    """somebody wrapped up: the round of the head, the neck, the shoulders, narrowing down to the feet, the wrapping
    crossing over it in slanting bands, strands holding it to the ground on a skirt of dried resin; a boot or a hand
    left sticking out of the end on some"""
    u = rng.uniform; ff = frame(cx, cy, a); L = u(5.4, 6.8); r = u(1.1, 1.35); o = Obj("web")
    env = [(0.0, 0.0), (0.03, 0.4), (0.3, 0.72), (0.66, 1.0), (0.76, 0.9), (0.81, 0.56), (0.87, 0.66), (0.95, 0.56), (1.0, 0.0)]
    nb = rng.randint(6, 8); sl = rng.choice((-1, 1))
    def rf(t):
        s = (t + L / 2) / L; return r * _pl(env, s) * (1.0 if s > 0.79 else 0.86 + 0.14 * (1 - ((s / 0.79 * nb) % 1.0)) ** 0.6) + 0.03
    cuts = [(barl(ff(-L * 0.42, -sl * r * sv), ff(L * 0.24, sl * r * sv), 0.34), r * 0.6) for sv in (-1, 1)[:rng.randint(1, 2)]]
    o.st(spin(cx, cy, a, L, rf, 1.0), cuts=cuts, foot=rect(cx, cy, L, 2 * r, a))
    ps = []
    for i in range(rng.randint(3, 5)):
        x = u(-L * 0.4, L * 0.35); sv = rng.choice((-1, 1)); p = ff(x, sv * r * 0.3); e = ff(x + u(-1.2, 1.2), sv * (r + u(1.3, 2.2))); ps += rod([p, e], lambda q: 0.4 - 0.14 * q, 0.9); o.F.append(disc(e[0], e[1], 0.3))
    o.st(ps, foot=rect(cx, cy, L, 2 * r, a))
    k = rng.random()
    if k < 0.45:          # a boot: sole, heel and the tread across it
        c = ff(-L / 2 - 0.55, u(-0.2, 0.2)); t = a + u(-0.4, 0.4); f2 = frame(c[0], c[1], t); b = o.up(rrect(c[0], c[1], 1.8, 0.98, t, 0.3), 0.55, "metal")
        o.cut(U([barl(f2(x, -1), f2(x, 1), 0.26) for x in (-0.5, -0.15, 0.2)]), b, 0.18); o.on(rrect(*f2(0.5, 0), 0.6, 0.8, t, 0.2), b, 0.16, "metal")
    elif k < 0.7:          # a hand: the back of it and four fingers
        c = ff(-L * 0.1, rng.choice((-1, 1)) * (r + 0.5)); t = math.atan2(c[1] - cy, c[0] - cx); hs = dome(c[0], c[1], 0.5, 0.45)
        for j in range(4): p = pol(c[0], c[1], 0.3, t + (j - 1.5) * 0.4); hs += rod([p, pol(p[0], p[1], u(0.7, 1.0), t + (j - 1.5) * 0.3)], 0.24, 1.1)
        o.st(hs, "flesh", foot=disc(c[0], c[1], 1.3))
    return o.out()

@d("infestation", "growth", "egg", "burst_egg")
def _(cx, cy, a, rng):
    """an egg that has opened: a leathery body swelling out from its foot, the top split into three to five petals
    with a dark hollow between them, roots gripping the ground, slime run out on one side"""
    u = rng.uniform; r = u(1.4, 1.8); h = r * u(1.35, 1.6); n = rng.choice((3, 4, 4, 5)); o = Obj("egg")
    ps = turn(cx, cy, [(r * 0.86, 0.0), (r, h * 0.3), (r * 0.97, h * 0.55), (r * 0.88, h * 0.8), (r * 0.78, h)], 24)
    cuts = [(disc(cx, cy, r * 0.5, 14), h * 0.3)] + [(Polygon([(cx, cy)] + arc(cx, cy, r * 1.3, a + 6.2832 * k / n - math.pi / n * (0.3 + 0.15 * j), a + 6.2832 * k / n + math.pi / n * (0.3 + 0.15 * j), 6)), h * (0.42 + 0.145 * j)) for k in range(n) for j in range(4)]
    rs = []
    for k in range(rng.randint(4, 6)): t = a + 6.2832 * k / 5 + u(-0.3, 0.3); rs += rod(_curl(pol(cx, cy, r * 0.6, t), t, r * 0.4 + u(1.0, 1.6), u(-0.9, 0.9), 6), lambda q: 0.46 - 0.2 * q, 0.9)
    o.st(rs, foot=disc(cx, cy, r + 1.4)); o.st(ps, cuts=cuts, foot=disc(cx, cy, r))
    if rng.random() < 0.6: o.gcut(blob(*pol(cx, cy, r + 1.0, u(0, 6.28)), u(0.6, 0.9), rng, 3), 0.26, "acid")
    return o.out()

@d("infestation", "made", "carapace", "dead_crawler")
def _(cx, cy, a, rng):
    """a dead crawler on its back: the plated oval body with the seam down its belly, four pairs of jointed legs
    curled in over it, the long tail lying in a loose coil behind"""
    u = rng.uniform; ff = frame(cx, cy, a); L = u(3.4, 4.1); W = L * u(0.5, 0.58); o = Obj("carapace"); ns = rng.randint(5, 6)
    def rf(t):
        s = (t + L / 2) / L; return W / 2 * math.sin(math.pi * min(1.0, max(0.0, s)) ** 0.8) ** 0.6 * (0.84 + 0.16 * (1 - ((s * ns) % 1.0)) ** 0.6) + 0.04
    o.st(spin(cx, cy, a, L, rf, 1.25), cuts=[(barl(ff(-L * 0.3, 0), ff(L * 0.34, 0), 0.34), W * 0.36)], foot=rect(cx, cy, L, W, a))
    for k in range(3):
        for sv in (-1, 1):
            x = (k - 1) * L * 0.27; t = a + sv * (1.5708 - (k - 1) * 0.5) + u(-0.15, 0.15); p0 = ff(x, sv * W * 0.3); out = W * 0.2 + u(1.1, 1.6); kn = pol(p0[0], p0[1], out, t)
            pts = [p0, kn] + _curl(kn, t + sv * rng.choice((-1, 1)) * u(1.6, 2.2), u(1.0, 1.5), sv * u(-0.8, 0.8), 4)[1:]
            o.st(rod(pts, lambda q: 0.4 - 0.14 * q + 0.1 * math.exp(-((q - 0.5) / 0.1) ** 2), 1.0), foot=LineString(pts).buffer(0.5))
    t0 = a + math.pi + u(-0.4, 0.4); tl = _curl(ff(-L / 2 + 0.2, 0), t0, u(4.5, 6.5), rng.choice((-1, 1)) * u(2.6, 4.2), 16)
    o.st(rod(tl, lambda q: 0.44 - 0.2 * q, 1.0), foot=LineString(tl).buffer(0.5))
    hd = ff(L / 2 + 0.1, 0); o.st([s for sv in (-1, 1) for s in rod([ff(L / 2 - 0.3, sv * 0.3), ff(L / 2 + 0.7, sv * 0.45), ff(L / 2 + 1.0, sv * 0.15)], 0.26, 1.0)], foot=disc(hd[0], hd[1], 1.0))
    return o.out()

@d("infestation", "growth", "resin", "resin_stump")
def _(cx, cy, a, rng):
    """the stump of a hive pillar: a column of twisted ribs flaring out at the foot into buttress roots, snapped off
    slantwise so one side stands higher, the hollow core showing in the break"""
    u = rng.uniform; r = u(1.5, 2.0); h = u(1.9, 2.8); n = rng.randint(5, 7); tw = u(0.05, 0.11) * rng.choice((-1, 1)); m = 9; o = Obj("resin")
    ps = [s for k in range(m) for s in ext(star(cx, cy, r * (0.6 + 0.4 * (1 - k / (m - 1)) ** 2), r * (0.6 + 0.4 * (1 - k / (m - 1)) ** 2) * 0.66, n, a + tw * k), h * (k + 1) / m)]
    tb = u(0, 6.28); brk = Polygon([pol(cx, cy, 3 * r, tb - 1.5), pol(cx, cy, 0.1 * r, tb + math.pi), pol(cx, cy, 3 * r, tb + 1.5), pol(cx, cy, 4 * r, tb)])
    cuts = [(brk, h * u(0.55, 0.7)), (ngon(cx, cy, r * 0.3, 5, u(0, 3)), h * 0.3)]
    rs = []
    for k in range(n):
        t = a + 6.2832 * k / n; rs += rod(_curl(pol(cx, cy, r * 0.5, t), t, r * 0.5 + u(0.5, 1.1), u(-0.7, 0.7), 6), lambda q: 0.6 - 0.32 * q, 1.0)
    o.st(rs, foot=disc(cx, cy, r + 1.0)); o.st(ps, cuts=cuts, foot=disc(cx, cy, r))
    for i in range(rng.randint(1, 3)): c = pol(cx, cy, r + u(1.0, 1.8), tb + math.pi + u(-0.8, 0.8)); o.st(_lean(c[0], c[1], u(0.4, 0.55), u(0.4, 0.7), u(-0.2, 0.2), u(-0.2, 0.2), 5, sq=0.7, a=u(0, 3)), foot=disc(c[0], c[1], 0.5))
    return o.out()

@d("infestation", "growth", "spore", "spore_chimneys")
def _(cx, cy, a, rng):
    """spore chimneys: each a swollen bulb, a collar and a flue with a thick lip round an open mouth, two to four of
    them of different heights crowded together, spore pods lying round their feet"""
    u = rng.uniform; o = Obj("spore"); n = rng.randint(2, 4)
    for i in range(n):
        x, y = pol(cx, cy, 0 if i == 0 else u(1.9, 2.5), a + i * 2.1 + u(-0.3, 0.3)); r = u(1.0, 1.3) * (1 if i == 0 else u(0.65, 0.85)); h = r * u(1.9, 2.5); rf = max(0.72, r * 0.62)
        ps = dome(x, y, r, h * 0.42, sq=u(0.85, 1.0), a=u(0, 3)) + turn(x, y, [(max(rf + 0.1, r * 0.8), 0.0), (max(rf + 0.1, r * 0.8), h * 0.5), (rf, h * 0.58), (rf, h)], 16)
        o.st(ps, cuts=[(disc(x, y, rf - 0.3, 10), h * 0.45)], foot=disc(x, y, r))
    ps = []
    for i in range(rng.randint(3, 6)): p = pol(cx, cy, u(2.6, 3.6), u(0, 6.28)); ps += dome(p[0], p[1], u(0.32, 0.42), u(0.36, 0.5)); o.F.append(disc(p[0], p[1], 0.4))
    o.st(ps, foot=disc(cx, cy, 1.0))
    return o.out()

@d("infestation", "marks", "acid", "acid_holes")
def _(cx, cy, a, rng):
    """holes eaten by acid: each pit with a swollen, melted lip round it, a big one and its smaller neighbours, and
    the pin holes where single drops fell"""
    u = rng.uniform; lay = []; foot = []; n = rng.randint(3, 5); hs = []
    for i in range(n):
        p = (cx, cy) if i == 0 else pol(cx, cy, u(1.9, 3.0), a + i * 6.2832 / (n - 1) + u(-0.4, 0.4)); g = blob(p[0], p[1], u(0.9, 1.2) if i == 0 else u(0.5, 0.8), rng, 3)
        if any(g.distance(q) < 1.0 for q in hs): continue
        hs.append(g); lay.append(ST(ext(g.buffer(0.5, 6).difference(g.buffer(0.03)), u(0.3, 0.42)), "acid")); lay.append(("gcut", g, u(0.5, 0.8), "acid")); foot.append(g.buffer(0.6))
    dr = U([disc(*pol(cx, cy, u(1.2, 3.4), u(0, 6.28)), u(0.26, 0.36), 8) for i in range(rng.randint(3, 7))]).difference(U(foot).buffer(0.3))
    if not dr.is_empty: lay.append(("gcut", dr, 0.4, "acid")); foot.append(dr)
    return hull(foot).buffer(0.2), lay

@d("infestation", "growth", "flesh", "hive_vent", ground=True)
def _(cx, cy, a, rng):
    """a breathing vent of the hive: a puckered ring of flesh, folded all round, closing on a deep throat; thick
    veins run out from it under the ground's skin"""
    u = rng.uniform; R = u(1.9, 2.5); h = u(1.0, 1.4); n = rng.randint(8, 11); o = Obj("flesh"); sq = u(0.8, 1.0); ri = R * 0.3
    vs = []
    for k in range(rng.randint(2, 4)): t = u(0, 6.28); vs += rod(_curl(pol(cx, cy, R * 0.7, t), t, u(1.1, 1.6), u(-1.0, 1.0), 6), lambda q: 0.5 - 0.24 * q, 0.8)
    o.st(vs, foot=disc(cx, cy, R + 1.0))
    cuts = [(disc(cx, cy, ri, 14), 0.0)] + [(tri(pol(cx, cy, R * 0.98, a + 6.2832 * k / n), pol(cx, cy, ri * 0.8, a + 6.2832 * k / n), 0.4), h * 0.55) for k in range(n)]
    o.st(turn(cx, cy, [(R, 0.0), (R * 0.86, h * 0.5), (R * 0.62, h * 0.9), (R * 0.46, h)], 24, sq, a), cuts=cuts, foot=disc(cx, cy, R))
    o.gcut(disc(cx, cy, ri - 0.1, 12), u(0.6, 0.85), "pit")
    return o.out()

@d("infestation", "made", "carapace", "molt")
def _(cx, cy, a, rng):
    """a shed skin: a long husk of overlapping back plates, wide at the head shield and narrowing to two tail prongs,
    split open down the back where the thing climbed out, the empty leg cases still sticking out to the sides"""
    u = rng.uniform; ff = frame(cx, cy, a); L = u(4.8, 6.0); W = u(1.0, 1.25); ns = rng.randint(6, 8); o = Obj("carapace"); bow = u(-0.5, 0.5)
    env = [(0.0, 0.3), (0.12, 0.5), (0.6, 0.86), (0.72, 0.8), (0.78, 1.0), (0.93, 0.9), (1.0, 0.25)]
    def rf(t):
        s = (t + L / 2) / L; sc = 1.0 if s > 0.75 else 0.8 + 0.2 * (1 - ((s / 0.75 * ns) % 1.0)) ** 0.7
        return W * _pl(env, s) * sc + 0.03
    legs = []
    for k in range(3):
        for sv in (-1, 1):
            if rng.random() < 0.2: continue
            x = L * (0.02 + 0.13 * k); p0 = ff(x, sv * W * 0.4); kn = ff(x + u(-0.2, 0.3), sv * (W + u(0.6, 1.0))); tp = ff(x - u(0.5, 1.0), sv * (W + u(1.2, 1.9)))
            legs += rod([p0, kn, tp], lambda q: 0.32 - 0.07 * q, 1.0)
    for sv in (-1, 1): legs += rod([ff(-L / 2 + 0.3, sv * 0.2), ff(-L / 2 - u(0.6, 1.1), sv * u(0.4, 0.8))], lambda q: 0.32 - 0.07 * q, 1.0)
    o.st(legs, foot=rect(cx, cy, L + 2.0, 2 * W + 3.6, a))
    split = Polygon([ff(-L * 0.12, 0), ff(L * 0.1, -0.36), ff(L * 0.42, -0.3), ff(L * 0.47, 0), ff(L * 0.42, 0.3), ff(L * 0.1, 0.36)])
    o.st(spin(cx, cy, a, L, rf, 0.85), cuts=[(split, 0.25)], foot=rect(cx, cy, L, 2 * W, a))
    return o.out()

@d("infestation", "growth", "tendril", "tendril_knot")
def _(cx, cy, a, rng):
    """a knot of tendrils: fat ringed arms winding out of one heap, each curling its own way and thinning to a tip,
    two of them looped over the others in the middle"""
    u = rng.uniform; o = Obj("tendril"); n = rng.randint(4, 6); ps = []
    for i in range(n):
        t = a + 6.2832 * i / n + u(-0.3, 0.3); Lt = u(3.0, 4.6); pts = _curl(pol(cx, cy, 0.3, t), t, Lt, rng.choice((-1, 1)) * u(1.0, 2.4), 14); nr = Lt / 0.55
        ps += rod(pts, lambda q, nr=nr: (0.68 - 0.44 * q) * (0.86 + 0.14 * abs(math.sin(q * math.pi * nr))), 1.0); o.F.append(LineString(pts).buffer(0.5))
    o.st(ps, foot=disc(cx, cy, 1.0))
    t = u(0, 6.28); o.st(rod(arc(*pol(cx, cy, 0.5, t), 0.85, t, t + 4.4, 14), 0.5, 1.0, lift=0.45) + rod(arc(*pol(cx, cy, 0.5, t + 3.0), 0.75, t + 2.0, t + 5.6, 14), 0.46, 1.0, lift=0.75), foot=disc(cx, cy, 1.6))
    return o.out()

@d("infestation", "growth", "pustule", "sac_cluster")
def _(cx, cy, a, rng):
    """a cluster of sacs on a skin of membrane: taut bladders of different sizes, each with its teat, one burst and
    fallen in, veins feeding them from the ground around"""
    u = rng.uniform; o = Obj("pustule"); n = rng.randint(4, 6); cs = []
    for i in range(n):
        for _try in range(8):
            p = (cx, cy) if i == 0 else pol(cx, cy, u(1.0, 2.4), u(0, 6.28)); r = u(0.95, 1.25) if i == 0 else u(0.6, 0.95)
            if all(math.dist(p, q) > (r + rq) * 0.8 for q, rq in cs): cs.append((p, r)); break
    mem = U([disc(p[0], p[1], r + 0.45, 14) for p, r in cs]).buffer(0.4).buffer(-0.4); vs = []
    for k in range(rng.randint(2, 4)): t = u(0, 6.28); e = pol(cx, cy, u(3.2, 4.2), t); vs += rod(wiggle(pol(cx, cy, 1.0, t), e, rng, 0.4, 3), lambda q: 0.42 - 0.16 * q, 0.9); o.F.append(disc(e[0], e[1], 0.3))
    o.st(vs, foot=mem); b = o.up(mem, 0.3, "resin"); burst = rng.randint(1, len(cs)) if rng.random() < 0.6 else -1
    for i, (p, r) in enumerate(cs):
        h = r * u(1.0, 1.3); sq = u(0.82, 1.0); t = u(0, 3)
        if i == burst: o.st(dome(p[0], p[1], r, h * 0.6, sq=sq, a=t), par=b, cuts=[(star(p[0], p[1], r * 0.8, r * 0.3, 5, t), h * 0.2)])
        else: o.st(dome(p[0], p[1], r, h, sq=sq, a=t) + dome(p[0], p[1], 0.34, 0.3, lift=h * 0.93), par=b)
    return o.out()

@d("infestation", "made", "metal", "flamethrower")
def _(cx, cy, a, rng):
    """a flame unit thrown down: two or three fuel bottles with domed ends and bands, strapped on a back frame, the
    valve bar across their necks, a hose running to the gun: a long barrel in a slotted heat shield, a flared
    nozzle, the grip and the igniter bottle under it"""
    u = rng.uniform; ff = frame(cx, cy, a); L = u(3.0, 3.6); r = u(0.58, 0.68); n = rng.choice((2, 2, 3)); o = Obj("metal"); Wt = n * 2 * r + 0.1
    fr = o.up(rrect(*ff(-0.1, 0), L + 0.5, Wt + 0.5, a, 0.3), 0.3)
    def tank(t): e = abs(t) - (L / 2 - r); return r * (math.sqrt(max(0.0, 1 - (e / r) ** 2)) if e > 0 else (1.12 if abs(abs(t) - L * 0.2) < 0.16 else 1.0)) + 0.02
    ps = []
    for i in range(n): c = ff(0, (i - (n - 1) / 2) * (2 * r + 0.05)); ps += spin(c[0], c[1], a, L, tank, 1.05, lift=0.2) + lie(*pol(c[0], c[1], L / 2 + 0.1, a), a, 0.5, 0.3, lift=0.3)
    o.st(ps + blk(*ff(L / 2 + 0.45, 0), 0.5, Wt - 0.3, a, 0.75), foot=rect(cx, cy, L + 1.2, Wt, a))
    for sv in (-1, 1):          # the shoulder straps lying out to the side, each with its buckle
        p = ff(u(-0.6, 0.6), sv * (Wt / 2 + 0.1)); pts = _curl(p, a + sv * 1.57 + u(-0.4, 0.4), u(1.6, 2.4), u(-1.2, 1.2), 8); o.st(ext(line(pts, 0.6), 0.3), "leather", foot=LineString(pts).buffer(0.5))
    sv = rng.choice((-1, 1)); g0 = ff(u(0.6, 1.6), sv * (Wt / 2 + u(2.0, 2.6))); b = a + u(-0.3, 0.3); f2 = frame(g0[0], g0[1], b); Lg = u(4.2, 5.0)
    rc = o.up(rrect(*f2(-Lg * 0.28, 0), Lg * 0.36, 0.95, b, 0.2), 0.62); o.cut(U([barl(f2(-Lg * 0.28 + x, -0.3), f2(-Lg * 0.28 + x, 0.3), 0.26) for x in (-0.35, 0.0, 0.35)]), rc, 0.18)
    gp = Polygon([f2(-Lg * 0.36, -sv * 0.3), f2(-Lg * 0.22, -sv * 0.3), f2(-Lg * 0.3, -sv * 1.4), f2(-Lg * 0.46, -sv * 1.3)]); o.st(ext(gp, 0.5), foot=gp)
    def brl(t): return 0.5 if t > Lg * 0.42 - Lg * 0.2 else (0.42 if abs(t + Lg * 0.2 - Lg * 0.14) < Lg * 0.14 else 0.3)
    hs = Lg * 0.6; o.st(spin(*f2(Lg * 0.2, 0), b, hs, brl, 1.05), cuts=[(barl(f2(Lg * 0.2 - hs / 2 + 0.3 + i * 0.5, -0.5), f2(Lg * 0.2 - hs / 2 + 0.3 + i * 0.5, 0.5), 0.26), 0.32) for i in range(int(Lg * 0.28 / 0.5))] + [(disc(*f2(Lg * 0.2 + hs / 2, 0), 0.3, 8), 0.2)], foot=rect(*f2(Lg * 0.2, 0), hs, 1.0, b))
    o.st(lie(*f2(-Lg * 0.02, sv * 0.66), b, 1.3, 0.32), foot=rect(*f2(-Lg * 0.02, sv * 0.66), 1.3, 0.6, b))
    o.st(rod(wiggle(ff(L / 2 + 0.6, 0), f2(-Lg * 0.46, 0), rng, 0.6, 4), 0.3, 1.1), "cable")
    return o.out()

@d("infestation", "made", "metal", "scanner_and_mags")
def _(cx, cy, a, rng):
    """a motion tracker dropped face up: the screen in its hood with the range rings and the sweep line on it, three
    keys, the dial, the pistol grip behind and the sensor horn in front; beside it one to three rifle magazines,
    ribbed, the top round showing"""
    u = rng.uniform; ff = frame(cx, cy, a); L = u(2.8, 3.3); W = u(1.9, 2.2); o = Obj("metal"); g = rrect(cx, cy, L, W, a, 0.3)
    gr = o.up(Polygon([ff(-L / 2 + 0.2, -0.5), ff(-L / 2 - 1.8, -0.5 + u(-0.3, 0.3)), ff(-L / 2 - 1.8, 0.5), ff(-L / 2 + 0.2, 0.5)]).buffer(-0.12).buffer(0.12, 4), 0.5)
    o.cut(U([barl(ff(-L / 2 - x, -1), ff(-L / 2 - x, 1), 0.26) for x in (0.5, 0.9, 1.3)]), gr, 0.16)
    b = o.up(g, 0.6); o.up(Polygon([ff(L / 2 - 0.1, -W * 0.3), ff(L / 2 + 0.7, -W * 0.18), ff(L / 2 + 0.7, W * 0.18), ff(L / 2 - 0.1, W * 0.3)]), 0.45)
    sc = ff(L * 0.12, 0); hd = o.on(rrect(sc[0], sc[1], L * 0.58, W - 0.3, a, 0.2), b, 0.3); R = min(L * 0.58, W - 0.3) / 2 - 0.22
    o.cut(disc(sc[0], sc[1], R, 20), hd, 0.2)
    half = Polygon([sc] + arc(sc[0], sc[1], 3, a - 1.2, a + 1.2, 8))
    rings = U([ann(sc[0], sc[1], R * 0.75, R * 0.75 - 0.26, 16), disc(sc[0], sc[1], 0.26, 8)]).intersection(half)
    sw = u(-0.9, 0.9); o.cut(U([rings, barl(sc, pol(sc[0], sc[1], R, a + sw), 0.26)]), hd, 0.4)
    o.studs([ff(-L / 2 + 0.5, (i - 1) * 0.55) for i in range(3)], b, 0.3, 0.16)
    kn = ff(u(-0.3, 0.6), -W / 2 - 0.05); o.up(disc(kn[0], kn[1], 0.52, 10), 0.4)
    for i in range(rng.randint(1, 3)):          # magazines: a slightly bent box, ribs pressed in its side, the feed lips and the top round
        c = ff(-1.4 + i * 1.7 + u(-0.2, 0.2), W / 2 + u(1.5, 2.2)); t = a + 1.57 + u(-0.7, 0.7); f2 = frame(c[0], c[1], t); Lm = u(2.2, 2.6)
        g = Polygon([f2(-Lm / 2, -0.5), f2(0, -0.6), f2(Lm / 2, -0.5), f2(Lm / 2, 0.5), f2(0, 0.4), f2(-Lm / 2, 0.5)])
        o.st(ext(g, 0.42), foot=g); o.st(ext(U([rect(*f2(x, -0.05), 0.5, 0.96, t) for x in (-0.75, 0.0, 0.75)]), 0.58), foot=g)
        if rng.random() < 0.7: o.st(lie(*f2(Lm / 2 + 0.3, 0), t, 0.7, 0.24) + cone(*f2(Lm / 2 + 0.72, 0), 0.24, 0.26, 0.2, n=8), "brass", foot=disc(*f2(Lm / 2 + 0.4, 0), 0.5))
    return o.out()

@d("infestation", "debris", "spine", "spine_row")
def _(cx, cy, a, rng):
    """a row of back spines breaking through the ground: a ridge of knuckled bone along a curve and from each
    knuckle a flattened spine raking backwards, the tallest in the middle, one snapped off short on some"""
    u = rng.uniform; R = u(3.2, 5.2); n = rng.randint(3, 5); o = Obj("spine"); half = 1.15 * (n - 1) / R; ox, oy = cx - R * math.cos(a), cy - R * math.sin(a)
    pts = arc(ox, oy, R, a - half, a + half, n - 1); rk = rng.choice((-1, 1)); brk = rng.randint(0, n - 1) if rng.random() < 0.4 else -1
    ext_ = arc(ox, oy, R, a - half - 0.7 / R, a + half + 0.7 / R, 3 * n)
    o.st(rod(ext_, lambda q: 0.7 + 0.25 * abs(math.cos(q * math.pi * (n - 1) * (1 + 1.4 / (2 * half * R)))), 0.7), foot=LineString(ext_).buffer(0.8))
    for i, p in enumerate(pts):
        k = 1 - abs(i - (n - 1) / 2) / n; t = a - half + 2 * half * i / (n - 1) + rk * 1.5708; h = u(2.0, 2.9) * (0.5 + 0.65 * k); Lf = u(1.5, 2.0) * (0.7 + 0.4 * k)
        o.st(_fin(p[0], p[1], t, Lf, h, u(0.7, 0.85), u(0.5, 1.0)), cuts=[(disc(*pol(p[0], p[1], Lf * 0.4, t), 2.0), h * u(0.3, 0.45))] if i == brk else (), foot=disc(p[0], p[1], Lf / 2))
    return o.out()

@d("infestation", "growth", "flesh", "larvae")
def _(cx, cy, a, rng):
    """grubs: three to five fat ringed larvae, each curled its own way, blunt at the head with the two jaw points,
    thinner at the tail"""
    u = rng.uniform; o = Obj("flesh"); cs = []
    for i in range(rng.randint(3, 5)):
        for _try in range(10):
            p = pol(cx, cy, u(0, 3.0), u(0, 6.28))
            if all(math.dist(p, q) > 2.0 for q in cs): cs.append(p); break
    for p in cs:
        L = u(2.4, 3.3); R = u(0.52, 0.64); t = u(0, 6.28); bend = rng.choice((-1, 1)) * u(0.8, 2.6); ns = rng.randint(6, 8)
        pts = _curl(p, t, L, bend, 12); mid = pts[len(pts) // 2]; pts = [(x - mid[0] + p[0], y - mid[1] + p[1]) for x, y in pts]
        o.st(rod(pts, lambda q, R=R, ns=ns: R * (0.55 + 0.45 * math.sin(min(1.0, q * 1.6 + 0.12) * math.pi / 2)) * math.sqrt(max(0.05, 1 - max(0.0, (q - 0.85) / 0.15) ** 2)) * (0.8 + 0.2 * abs(math.sin(q * math.pi * ns))), 1.05), foot=LineString(pts).buffer(R))
        e = pts[-1]; t2 = math.atan2(pts[-1][1] - pts[-2][1], pts[-1][0] - pts[-2][0])
        o.st([s for sv in (-1, 1) for s in cone(*pol(e[0], e[1], 0.3, t2 + sv * 0.7), 0.26, 0.3, 0.2, n=6)], "carapace", foot=disc(e[0], e[1], 0.5))
    return o.out()

@d("infestation", "growth", "web", "web_anchor")
def _(cx, cy, a, rng):
    """a web laid over the ground: spokes running out from a knot of resin to their glue points, ring after ring of
    sagging threads strung between them, torn away in places"""
    u = rng.uniform; o = Obj("web"); n = rng.randint(6, 8); ts = [a + 6.2832 * k / n + u(-0.2, 0.2) for k in range(n)]; Ls = [u(3.0, 4.6) for k in range(n)]; gs = []; ends = []
    for t, L in zip(ts, Ls): e = pol(cx, cy, L, t); gs.append(capl((cx, cy), e, 0.56)); ends.append(disc(e[0], e[1], 0.5, 8))
    for f in (0.42, 0.68, 0.92):
        for k in range(n):
            k2 = (k + 1) % n
            if rng.random() < 0.2: continue
            p, q = pol(cx, cy, Ls[k] * f, ts[k]), pol(cx, cy, Ls[k2] * f, ts[k2]); tm = ts[k] + ((ts[k2] - ts[k]) % 6.2832) / 2; m = pol(cx, cy, (Ls[k] + Ls[k2]) / 2 * f * 0.8, tm)
            gs.append(line([p, ((p[0] + m[0] * 2) / 3, (p[1] + m[1] * 2) / 3), ((q[0] + m[0] * 2) / 3, (q[1] + m[1] * 2) / 3), q], 0.52))
    o.st(ext(U(gs + ends).buffer(0.08).buffer(-0.08), 0.38), foot=hull(ends))
    o.st(dome(cx, cy, u(0.75, 0.95), u(0.75, 1.0), sq=0.85, a=u(0, 3)), "resin", foot=disc(cx, cy, 0.8))
    return o.out()

@d("infestation", "made", "carapace", "mandibles")
def _(cx, cy, a, rng):
    """a pair of jaws torn from something big: each a heavy curved blade, thick at the socket knob and sweeping round
    to a point, saw teeth along the inside edge; some lie alone"""
    u = rng.uniform; ff = frame(cx, cy, a); L = u(4.4, 5.6); o = Obj("carapace"); both = rng.random() < 0.75
    for sv in (-1, 1)[:2 if both else 1]:
        sp = u(1.0, 1.8); L = L * u(0.85, 1.0)
        cl = lambda s, sv=sv, sp=sp, L=L: ff(-L / 2 + L * s, sv * (0.62 + sp * math.sin(math.pi * s ** 0.7)))
        pts = [cl(k / 14) for k in range(15)]; teeth = []
        for s in np.arange(0.36, 0.86, u(0.11, 0.15)):
            p = cl(s); w = 0.75 * (1 - 0.4 * s); p2 = ff(-L / 2 + L * s, sv * (0.62 + sp * math.sin(math.pi * s ** 0.7) - 0.3)); q = ff(-L / 2 + L * s + 0.25, sv * (0.62 + sp * math.sin(math.pi * s ** 0.7) - 0.3 - 1.25 * (1 - 0.5 * s)))
            teeth.append(tri(p2, q, w))
        o.st(ext(U(teeth), 0.42), foot=LineString(pts).buffer(0.9))
        o.st(rod(pts, lambda q: 0.78 - 0.5 * q, 0.95), foot=LineString(pts).buffer(0.8))
        k = pts[0]; o.st(dome(k[0], k[1], 0.92, 1.0, sq=0.85, a=a), cuts=[(disc(*ff(-L / 2 - 0.2, sv * 0.62), 0.3, 8), 0.6)], foot=disc(k[0], k[1], 0.9))
    return o.out()

@d("infestation", "growth", "egg", "egg_ledge")
def _(cx, cy, a, rng):
    """a shelf of hardened resin with a clutch of eggs standing on it: the shelf curved, ribbed across and scalloped
    along its edge, each egg with the four closed lips of its top; one has opened on some"""
    u = rng.uniform; R = u(3.2, 4.2); n = rng.randint(3, 4); span = n * u(2.15, 2.35) / R; o = Obj("resin"); ox, oy = cx - R * math.cos(a), cy - R * math.sin(a)
    pts = arc(ox, oy, R, a - span / 2, a + span / 2, 14)
    g = U([line(pts, 2.6)] + [disc(*pol(ox, oy, R + 1.25, t), u(0.4, 0.62), 10) for t in np.arange(a - span / 2, a + span / 2, 0.85 / R)])
    b = o.up(g, u(0.55, 0.75)); o.cut(U([barl(pol(ox, oy, R - 2, a - span / 2 + span * (k + 1) / n), pol(ox, oy, R + 2, a - span / 2 + span * (k + 1) / n), 0.28) for k in range(n - 1)]), b, 0.2)
    op = rng.randint(0, n - 1) if rng.random() < 0.4 else -1
    for k in range(n):
        p = pol(ox, oy, R - 0.1 + u(-0.15, 0.15), a - span / 2 + span * (k + 0.5) / n); r = u(0.85, 1.0); h = u(1.5, 1.9); t = u(0, 1.5)
        ps = turn(p[0], p[1], [(r * 0.85, 0.0), (r, h * 0.35), (r * 0.88, h * 0.6), (r * 0.62, h * 0.84), (0.3, h)], 16)
        cuts = [(star(p[0], p[1], r * 0.85, 0.2, 4, t), h * 0.5)] if k == op else [(barl(pol(p[0], p[1], r * 0.62, t + j * 1.5708), pol(p[0], p[1], r * 0.62, t + j * 1.5708 + math.pi), 0.34), h * 0.55) for j in range(2)]
        o.st(ps, "egg", par=b, cuts=cuts)
    return o.out()

@d("infestation", "debris", "spine", "victim_bones")
def _(cx, cy, a, rng):
    """what the hive left of somebody: the skull with its eye holes and the jaw gone, the backbone with the ribs
    still on it curving out to both sides, a thigh bone or two with their knuckle ends lying apart"""
    u = rng.uniform; ff = frame(cx, cy, a); o = Obj("spine"); L = u(4.4, 5.2); nr = rng.randint(3, 4)
    sp = [ff(-L / 2 + L * k / 8, 0.25 * math.sin(k * 0.8 + a)) for k in range(9)]; ps = rod(sp, lambda q: 0.34 + 0.1 * abs(math.sin(q * math.pi * 9)), 1.1)
    for k in range(nr):
        x = -L * 0.15 + k * 0.9; w = 1.6 * (0.75 + 0.25 * math.sin(math.pi * (k + 0.5) / nr))
        for sv in (-1, 1):
            if rng.random() < 0.15: continue
            ps += rod([ff(x, sv * 0.15), ff(x + 0.25, sv * w * 0.6), ff(x + 0.1, sv * w), ff(x - 0.35, sv * w * 1.25)], 0.31, 1.1)
    o.st(ps, foot=rect(cx, cy, L, 4.4, a))
    c = ff(L / 2 + u(0.7, 1.1), u(-0.5, 0.5)); t = a + u(-0.8, 0.8); f2 = frame(c[0], c[1], t)
    o.st(dome(c[0], c[1], 0.85, 1.0, sq=0.85, a=t) + dome(*f2(0.6, 0), 0.52, 0.6), cuts=[(disc(*f2(0.42, sv * 0.34), 0.25, 8), 0.4) for sv in (-1, 1)] + [(tri(f2(0.98, 0), f2(0.6, 0), 0.34), 0.25)], foot=disc(c[0], c[1], 1.2))
    for i in range(rng.randint(1, 2)):
        c = ff(u(-L * 0.4, L * 0.2), rng.choice((-1, 1)) * u(2.6, 3.2)); t = a + u(-0.6, 0.6); Lb = u(2.2, 2.8); p, q = pol(c[0], c[1], Lb / 2, t), pol(c[0], c[1], Lb / 2, t + math.pi)
        o.st(rod([p, q], 0.29, 1.1) + [s for e in (p, q) for sv in (-1, 1) for s in dome(*pol(e[0], e[1], 0.24, t + sv * 1.57), 0.33, 0.42)], foot=LineString([p, q]).buffer(0.6))
    return o.out()
