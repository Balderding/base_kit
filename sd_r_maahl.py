"""Second drawing, maahl: the dune world's details (see sd_r.py for the rule). Carved stone is built in flat layers
so its lines stay crisp; pots, lamps and pods are turned or rounded. Nothing hangs: an arch lies in its stones, an
obelisk and a column lie where they fell."""
import math
from sd_reg import d

def ma_bar(p, q, rf, k=0.4, z0=None, z1=None):
    """a round bar from p to q with flat ends, solid underneath: rf is its radius (a number, or a function of 0..1
    along it: a foot, a belly, a neck, a flare). Lying, its axis is k radii off the ground."""
    L = math.dist(p, q) or 1.0; m = max(8, int(L / 0.11)); ux, uy = (q[0] - p[0]) / L, (q[1] - p[1]) / L; pt = lambda s, w: (p[0] + ux * s * L - uy * w, p[1] + uy * s * L + ux * w)
    S = [i / m for i in range(m + 1)]; R = [max(0.0, rf(s) if callable(rf) else rf) for s in S]; C = [(z0 + (z1 - z0) * s) if z0 is not None else k * r for s, r in zip(S, R)]
    def sect(z):
        out = []; run = []
        for s, r, c in list(zip(S, R, C)) + [(0.0, 0.0, 0.0)]:
            w = r if z <= c else (math.sqrt(max(0.0, r * r - (z - c) ** 2)) if z < c + r else 0.0)
            if w >= 0.2: run.append((s, w)); continue
            if len(run) >= 2 and (run[-1][0] - run[0][0]) * L >= 0.3:
                g = Polygon([pt(s_, w_) for s_, w_ in run] + [pt(s_, -w_) for s_, w_ in reversed(run)])
                if g.is_valid and g.area > 0.05: out.append(g)
            run = []
        return U(out) if out else None
    return [(sect, max(c + r for c, r in zip(C, R)))]

def ma_glyphs(ff, xs, vs, a, rng, keep=0.8):
    """carved signs in rows: a stroke, a pair of strokes, a ring, a hook; each one drawn by chance"""
    gs = []
    for x in xs:
        for v in vs:
            if rng.random() > keep: continue
            k = rng.randint(0, 3); c = ff(x, v)
            if k == 0: gs.append(rect(c[0], c[1], 0.26, 0.6, a + rng.choice((0.0, 0.6, -0.6))))
            elif k == 1: gs += [rect(*ff(x - 0.2, v), 0.22, 0.56, a), rect(*ff(x + 0.2, v), 0.22, 0.56, a)]
            elif k == 2: gs.append(ann(c[0], c[1], 0.32, 0.1, 8))
            else: gs.append(line([ff(x - 0.22, v - 0.25), ff(x + 0.2, v - 0.25), ff(x + 0.2, v + 0.25)], 0.22))
    return U(gs) if gs else None

# ---------------------------------------------------------------------------------------------------- redrawn
@d("maahl", "made", "ruin", "buried_head")
def _(cx, cy, a, rng):
    """the head of a fallen colossus lying face up, half under the sand: the striped head-cloth round it, the long
    face with its brow ridge, nose, eye hollows (three eyes on some: this is not our world) and the slit of a
    mouth; a drift lies over the chin"""
    u = rng.uniform; ff = frame(cx, cy, a); o = Obj("ruin"); s = u(1.05, 1.3)
    hd = Polygon([ff(-2.3 * s, -1.4 * s), ff(0.3 * s, -2.1 * s), ff(1.9 * s, -1.9 * s), ff(1.9 * s, 1.9 * s), ff(0.3 * s, 2.1 * s), ff(-2.3 * s, 1.4 * s)]).buffer(-0.3).buffer(0.3, 6)
    fc = ell(*ff(0.15 * s, 0), 1.6 * s, 1.12 * s, a, 28); b = o.up(hd, u(0.5, 0.65))
    o.cut(U([fc.buffer(k).exterior.buffer(0.11) for k in (0.42, 0.84, 1.26)]).intersection(inset(hd, 0.1)), b, 0.17)
    f = o.on(fc, b, u(0.45, 0.6))
    eyes = [ell(*ff(-0.3 * s, sv * 0.58 * s), 0.27 * s, 0.4 * s, a, 12) for sv in (-1, 1)] + ([ell(*ff(-1.05 * s, 0), 0.3 * s, 0.22 * s, a, 12)] if rng.random() < 0.4 else [])
    o.cut(U(eyes), f, 0.3)
    o.on(U([rect(*ff(-0.72 * s, sv * 0.6 * s), 0.46, 0.95 * s, a + sv * 0.15) for sv in (-1, 1)]), f, 0.17)
    o.on(Polygon([ff(-0.6 * s, -0.23), ff(0.4 * s, -0.36), ff(0.4 * s, 0.36), ff(-0.6 * s, 0.23)]), f, u(0.3, 0.42))
    lp = o.on(ell(*ff(0.95 * s, 0), 0.3 * s, 0.6 * s, a, 12), f, 0.16); o.cut(rect(*ff(0.95 * s, 0), 0.2, 0.8 * s, a), lp, 0.15)
    if rng.random() < 0.3: o.cut(wavy(ff(u(-1, 1), -2.2 * s), ff(u(-1, 1), 2.2 * s), rng, 0.4, 4, 0.3), f, 0.3)
    c = ff(u(1.7, 2.2) * s, u(-1.0, 1.0)); o.st(dome(c[0], c[1], u(1.5, 2.0), u(0.6, 0.9), sq=u(0.6, 0.8), a=a + 1.57 + u(-0.4, 0.4)), "sand", foot=disc(c[0], c[1], 1.5))
    return o.out()

@d("maahl", "made", "obelisk", "obelisk_tip")
def _(cx, cy, a, rng):
    """the top of a fallen obelisk: the square shaft narrowing to the pyramid point, a framed column of carved signs
    down its face, the foot broken off ragged; a further piece of the shaft may lie behind it"""
    u = rng.uniform; ff = frame(cx, cy, a); L = u(5.5, 7.0); w0 = u(1.9, 2.3); w1 = w0 * 0.74; Lt = u(1.5, 1.9); h = u(0.8, 1.0); o = Obj("obelisk"); x0 = -L / 2; x1 = L / 2 - Lt
    shaft = Polygon([ff(x0 + u(-0.3, 0.3), v) for v in np.linspace(-w0 / 2, w0 / 2, 5)] + [ff(x1, w1 / 2), ff(x1, -w1 / 2)]); b = o.up(shaft, h)
    Lp = x1 - x0 - 1.2; xm = (x0 + x1) / 2 + 0.15; o.groove(rect(*ff(xm, 0), Lp, w1 - 0.7, a), b, 0.2, 0.16)
    o.cut(ma_glyphs(ff, np.arange(xm - Lp / 2 + 0.6, xm + Lp / 2 - 0.4, 0.85), [0.0], a, rng, 0.9), b, 0.17)
    ps = []
    for f, hh in ((1.0, 0.4), (0.62, 0.72), (0.3, 1.0)): ps += ext(Polygon([ff(x1 + 0.06, -w1 / 2 * f), ff(x1 + 0.06 + Lt * (0.4 + 0.6 * f), 0), ff(x1 + 0.06, w1 / 2 * f)]), h * hh)
    o.st(ps, foot=Polygon([ff(x1, -w1 / 2), ff(L / 2, 0), ff(x1, w1 / 2)]))
    if rng.random() < 0.5:
        l2 = u(1.4, 2.4); c = ff(x0 - l2 / 2 - u(0.6, 1.0), u(-0.4, 0.4)); t = a + u(-0.3, 0.3); f2 = frame(c[0], c[1], t); w2 = w0 * 1.04
        g = Polygon([f2(-l2 / 2 + u(-0.25, 0.25), v) for v in np.linspace(-w2 / 2, w2 / 2, 4)] + [f2(l2 / 2 + u(-0.25, 0.25), v) for v in np.linspace(w2 / 2, -w2 / 2, 4)])
        b2 = o.up(g, h); o.cut(U([barl(f2(-l2, sv * (w2 / 2 - 0.45)), f2(l2, sv * (w2 / 2 - 0.45)), 0.2) for sv in (-1, 1)]), b2, 0.16); o.cut(ma_glyphs(f2, np.arange(-l2 / 2 + 0.6, l2 / 2 - 0.4, 0.85), [0.0], t, rng, 0.9), b2, 0.17)
    return o.out()

@d("maahl", "made", "ruin", "sundial")
def _(cx, cy, a, rng):
    """a sundial on its eight-sided plinth: the dial plate with its ring and the hour lines cut round it, the gnomon
    standing as a stepped fin from the middle to the rim; cracked across on some, sand banked against one side"""
    u = rng.uniform; R = u(2.0, 2.6); o = Obj("ruin")
    b = o.up(ngon(cx, cy, R + 0.55, 8, a + 0.39), u(0.35, 0.5)); dl = o.on(disc(cx, cy, R, 32), b, 0.32)
    hrs = [a + 0.95 + k * (6.2832 - 1.9) / 10 for k in range(11)]
    o.cut(U([ann(cx, cy, R * 0.6, R * 0.6 - 0.2, 24)] + [barl(pol(cx, cy, R * (0.66 if k % 2 == 0 else 0.78), t), pol(cx, cy, R * 0.95, t), 0.24) for k, t in enumerate(hrs)]).difference(rect(*pol(cx, cy, R * 0.45, a), R, 0.9, a)), dl, 0.17)
    if rng.random() < 0.35: o.cut(wavy(pol(cx, cy, R * 1.1, a + 2.0), pol(cx, cy, R * 1.1, a + 4.6), rng, 0.4, 4, 0.3).difference(disc(cx, cy, 0.5)), dl, 0.3)
    o.on(rect(*pol(cx, cy, R * 0.45, a), R * 0.86, 0.52, a), dl, u(0.4, 0.5)); o.on(rect(*pol(cx, cy, R * 0.25, a), R * 0.45, 0.52, a), dl, u(0.85, 1.1))
    if rng.random() < 0.6: t = a + u(1.5, 4.7); c = pol(cx, cy, R + 0.9, t); o.st(dome(c[0], c[1], u(1.3, 1.8), u(0.45, 0.7), sq=0.6, a=t + 1.57), "sand", foot=disc(c[0], c[1], 1.3))
    return o.out()

@d("maahl", "made", "clay", "amphorae")
def _(cx, cy, a, rng):
    """wine jars lying in the sand: the pointed foot, the swelling belly, the shoulder drawing in to a narrow neck
    and the flared lip, a handle on each side from shoulder to neck; shards of a broken one beside them"""
    u = rng.uniform; o = Obj("clay"); n = rng.randint(1, 3)
    for i in range(n):
        c = pol(cx, cy, 0 if n == 1 else 1.25, a + 1.57 + i * 6.2832 / n); b = a + u(-0.5, 0.5) + (math.pi if rng.random() < 0.3 else 0); L = u(3.8, 4.6); r = u(0.85, 1.05); f2 = frame(c[0], c[1], b)
        def rf(s, r=r):
            if s < 0.07: return 0.3
            if s < 0.6: return 0.3 + (r - 0.3) * math.sin((s - 0.07) / 0.53 * 1.5708) ** 0.8
            if s < 0.76: return r - (r - 0.36) * ((s - 0.6) / 0.16) ** 1.4
            return 0.36 if s < 0.91 else 0.55
        o.st(ext(U([line([f2(L * 0.12, sv * r * 0.5), f2(L * 0.2, sv * (r * 0.5 + 0.55)), f2(L * 0.36, sv * (r * 0.5 + 0.45)), f2(L * 0.4, sv * 0.3)], 0.5) for sv in (-1, 1)]), 0.42), foot=rect(c[0], c[1], L, 2 * r + 0.6, b))
        o.st(ma_bar(f2(-L / 2, 0), f2(L / 2, 0), rf, 0.4), foot=rect(c[0], c[1], L, 2 * r, b))
    for i in range(rng.randint(2, 4)): p = pol(cx, cy, u(2.6, 3.4), a + u(-0.6, 0.6) + (i % 2) * math.pi); o.st(ext(stone(p[0], p[1], u(0.45, 0.65), rng, 0.7, 4), 0.28), foot=disc(p[0], p[1], 0.6))
    return o.out()

@d("maahl", "made", "chitin", "scarab")
def _(cx, cy, a, rng):
    """a great dune beetle, dead on its belly: the two wing cases domed and ridged with the seam between them, the
    broad shield behind the head, the head with its horn, six jointed legs splayed out flat"""
    u = rng.uniform; ff = frame(cx, cy, a); L = u(3.4, 4.2); W = L * 0.66; h = u(0.9, 1.15); o = Obj("chitin"); legs = []
    for sv in (-1, 1):
        for x, dx, rch in ((-L * 0.22, -0.7, 1.3), (L * 0.05, -0.1, 1.4), (L * 0.28, 0.6, 1.2)):
            if rng.random() < 0.12: continue
            j = u(-0.2, 0.2); legs.append(line([ff(x, sv * W * 0.3), ff(x + dx * 0.4 + j, sv * (W / 2 + rch * 0.5)), ff(x + dx + j * 2, sv * (W / 2 + rch))], 0.5))
    if legs: o.st(ext(U(legs), 0.36), foot=rect(cx, cy, L, W + 3.0, a))
    for sv in (-1, 1): o.st(dome(*ff(-L * 0.14, sv * W * 0.235), L * 0.37, h, sq=(W * 0.26) / (L * 0.37), a=a), foot=rect(*ff(-L * 0.14, sv * W * 0.25), L * 0.7, W / 2, a))
    o.st(dome(*ff(L * 0.27, 0), W * 0.46, h * 0.85, sq=0.6, a=a + 1.5708), foot=rect(*ff(L * 0.27, 0), W * 0.5, W * 0.9, a))
    o.st(dome(*ff(L * 0.5, 0), W * 0.26, h * 0.5, sq=0.8, a=a) + rod([ff(L * 0.5, 0), ff(L * 0.5 + u(0.7, 1.1), u(-0.2, 0.2))], 0.27, 1.3), foot=disc(*ff(L * 0.55, 0), 0.9))
    return o.out()

@d("maahl", "made", "ruin", "arch_fragment")
def _(cx, cy, a, rng):
    """a fallen arch lying in its stones: wedge-cut blocks still in their curve, a moulding of two lines carved
    along them, the keystone standing proud with its boss, a block or two knocked out of line"""
    u = rng.uniform; R = u(3.2, 4.4); w = u(1.4, 1.7); span = u(1.4, 2.2); n = max(3, int(span * R / 1.45)); o = Obj("ruin"); kq = n // 2; a0 = a - span / 2
    for k in range(n):
        if k != kq and n > 3 and rng.random() < 0.12: continue
        t0 = a0 + span * k / n + 0.04; t1 = a0 + span * (k + 1) / n - 0.04; dr = u(-0.2, 0.2) if k != kq else 0.0; ex = 0.3 if k == kq else 0.0; c = pol(cx, cy, u(0, 0.15), u(0, 6.28))
        g = Polygon(arc(c[0], c[1], R + ex + dr, t0, t1, 4) + arc(c[0], c[1], R - w + dr, t1, t0, 4)); b = o.up(g, u(0.75, 0.95) + ex * 0.6)
        o.cut(U([line(arc(c[0], c[1], R - w * f + dr, t0 - 0.1, t1 + 0.1, 5), 0.22) for f in (0.28, 0.72)]), b, 0.17)
        if k == kq: m = pol(c[0], c[1], R - w * 0.5, (t0 + t1) / 2); o.on(disc(m[0], m[1], 0.3, 10), b, 0.17)
    for i in range(rng.randint(0, 2)): p = pol(cx, cy, R - w - u(0.8, 1.6), a + u(-span / 2, span / 2)); o.st(rock(p[0], p[1], u(0.4, 0.55), u(0.35, 0.5), rng), foot=disc(p[0], p[1], 0.5))
    return o.out()

@d("maahl", "debris", "crystal", "geode")
def _(cx, cy, a, rng):
    """a split geode: the rough rind of the stone, the hollow inside it and the ring of six-sided crystals standing in
    the hollow, each to its own height; the smaller half may lie beside it"""
    u = rng.uniform; lay = []; foot = []
    def half(x, y, r, h):
        lay.append(ST(rock(x, y, r, h, rng, 0.9, 10), "rock", cuts=[(disc(x, y, r * 0.64, 12), h * 0.3)])); m = max(3, int(r * 0.42 * 6.28 / 0.85)); cs = []
        for p in ringpts(x, y, r * 0.4, m, u(0, 6.28), 0.15, rng) + ([(x, y)] if r > 1.5 else []): rr = u(0.3, 0.4); hh = u(0.75, 1.35) * (h / 1.2); cs += turn(p[0], p[1], [(rr, 0.0), (rr, hh * 0.7), (0.2, hh)], 6, 1.0, u(0, 1))
        lay.append(ST(cs, "crystal")); foot.append(disc(x, y, r + 0.2))
    r = u(1.7, 2.2); half(cx, cy, r, u(1.0, 1.4))
    if rng.random() < 0.45: r2 = r * u(0.6, 0.75); p = pol(cx, cy, r + r2 + u(0.3, 0.8), a); half(p[0], p[1], r2, u(0.8, 1.0))
    return hull(foot), lay

@d("maahl", "made", "cloth", "saddlebags")
def _(cx, cy, a, rng):
    """a pair of saddlebags thrown down: two full pouches joined by the broad yoke strap that lies over the beast's
    back, each with its flap, the strap over the flap and its buckle, tassels at the outer ends"""
    u = rng.uniform; ff = frame(cx, cy, a); s = u(1.9, 2.3); o = Obj("cloth")
    y = o.up(rect(cx, cy, 2 * s - 1.0, u(1.5, 1.8), a), 0.3, "leather"); o.cut(U([barl(ff(-s, v), ff(s, v), 0.2) for v in (-0.45, 0.45)]), y, 0.15)
    for sv in (-1, 1):
        c = ff(sv * s, u(-0.2, 0.2)); t = a + u(-0.15, 0.15); f2 = frame(c[0], c[1], t); g = rrect(c[0], c[1], 2.1, u(2.4, 2.8), t, 0.55); b = o.up(g, u(0.8, 1.1))
        fl = o.on(rrect(*f2(-sv * 0.4, 0), 1.25, 2.3, t, 0.45), b, 0.17); o.on(rect(*f2(-sv * 0.05, 0), 1.7, 0.5, t), b, 0.3, "leather"); o.studs([f2(sv * 0.55, 0)], b, 0.3, 0.42, "brass")
        if rng.random() < 0.7: o.st(ext(U([barl(f2(sv * 1.1, v), f2(sv * u(1.6, 1.9), v + u(-0.3, 0.3)), 0.46) for v in (-0.75, 0, 0.75)]), 0.25), foot=disc(*f2(sv * 1.5, 0), 0.9))
    return o.out()

@d("maahl", "made", "brass", "oil_lamp")
def _(cx, cy, a, rng):
    """an oil lamp of the old kind: the round squat body on its foot, the filling hole in its collar, the long spout
    with the wick hole at its tip, the ring handle behind; its stopper on a chain beside it"""
    u = rng.uniform; o = Obj("brass"); r = u(1.05, 1.3); h = u(0.8, 1.0); Ls = u(1.5, 2.0)
    q = pol(cx, cy, r + 0.5, a + math.pi); o.st(ext(ann(q[0], q[1], 0.74, 0.27, 12), 0.5), foot=disc(q[0], q[1], 0.7))
    e = pol(cx, cy, r + Ls, a); w = pol(cx, cy, r + Ls - 0.42, a); o.st(ma_bar(pol(cx, cy, r * 0.5, a), e, lambda s: 0.62 - 0.22 * s, 0.5), foot=disc(e[0], e[1], 0.6))
    o.st(turn(cx, cy, [(r * 0.62, 0.0), (r * 0.62, 0.12), (r, h * 0.45), (r, h * 0.55)] + crv(r, 0.52, h * 0.55, h) + [(0.52, h + 0.14)], 24), cuts=[(disc(cx, cy, 0.28, 8), h - 0.25)], foot=disc(cx, cy, r))
    if rng.random() < 0.6:
        c = pol(cx, cy, r + u(0.9, 1.4), a + rng.choice((-1, 1)) * u(1.2, 1.9)); pts = wiggle(pol(cx, cy, r + 0.1, math.atan2(c[1] - cy, c[0] - cx)), c, rng, 0.25, 3)
        o.st(rod(pts, 0.25, 0.9) + dome(c[0], c[1], 0.4, 0.5), foot=disc(c[0], c[1], 0.5))
    return o.out()

@d("maahl", "made", "pavers", "mosaic")
def _(cx, cy, a, rng):
    """a patch of mosaic floor left in the sand: its border line, a figure of rings or lozenges with rays between
    them in the middle, one corner broken away and the loose cubes of it lying about"""
    u = rng.uniform; ff = frame(cx, cy, a); L = u(4.4, 5.6); W = L * u(0.75, 1.0); o = Obj("pavers"); g0 = rect(cx, cy, L, W, a)
    sx, sy = rng.choice((-1, 1)), rng.choice((-1, 1)); bite = blob(*ff(sx * L / 2, sy * W / 2), u(1.2, 1.8), rng, 4); g = g0.difference(bite)
    if g.geom_type != "Polygon": g = max(polys_of(g), key=lambda q: q.area)
    b = o.up(g, u(0.3, 0.38)); Rm = min(L, W) / 2 - 0.75; k = rng.choice((4, 8, 24)); t0 = a + (0.785 if k == 4 and rng.random() < 0.5 else 0.0)
    fig = [inset(g0, 0.42).exterior.buffer(0.11)] + [ngon(cx, cy, Rm * f, k, t0).exterior.buffer(0.11) for f in (1.0, 0.5)] + [barl(pol(cx, cy, Rm * 0.56, t0 + 6.2832 * j / 8 + 0.39), pol(cx, cy, Rm * 0.86, t0 + 6.2832 * j / 8 + 0.39), 0.22) for j in range(8)]
    o.cut(U(fig), b, 0.17); o.on(disc(cx, cy, Rm * 0.2, 12), b, 0.14)
    for i in range(rng.randint(3, 6)): p = ff(sx * (L / 2 - u(-0.3, 1.2)), sy * (W / 2 - u(-0.3, 1.2))); o.st(ext(rect(p[0], p[1], 0.58, 0.58, u(0, 1.5)), u(0.26, 0.34)), foot=disc(p[0], p[1], 0.5))
    return o.out()

@d("maahl", "made", "ruin", "sphinx_paw")
def _(cx, cy, a, rng):
    """the paw of a great stone beast on what is left of its plinth: the foreleg with a carved band round it, four
    long toes rounded at the knuckle, the grooves between them and a blunt claw before each"""
    u = rng.uniform; ff = frame(cx, cy, a); L = u(4.0, 4.8); W = u(2.9, 3.5); h = u(0.9, 1.15); o = Obj("ruin"); tw = W / 4
    pg = Polygon([ff(-L / 2 - 0.2 + u(-0.3, 0.3), v) for v in np.linspace(-W / 2 - 0.5, W / 2 + 0.5, 5)] + [ff(L / 2 + 1.8, W / 2 + 0.5), ff(L / 2 + 1.8, -W / 2 - 0.5)]); pl = o.up(pg, u(0.3, 0.4))
    ag = rrect(*ff(-0.75, 0), L - 1.5, W - 0.1, a, 0.5).intersection(inset(pg, 0.2)); arm = o.on(ag, pl, h)
    o.on(rect(*ff(-L * 0.28, 0), 0.62, W - 0.5, a), arm, 0.15); o.cut(U([barl(ff(-L * 0.05, (i - 1) * tw), ff(L / 2, (i - 1) * tw), 0.24) for i in range(3)]), arm, 0.22)
    o.st([s for i in range(4) for s in dome(*ff(L / 2 - 0.3, (i - 1.5) * tw), 1.05, h * u(0.85, 0.95), sq=(tw / 2 - 0.02) / 1.05, a=a)], par=pl)
    o.on(U([Polygon([ff(L / 2 + 0.9, (i - 1.5) * tw - 0.27), ff(L / 2 + 1.5, (i - 1.5) * tw - 0.23), ff(L / 2 + 1.5, (i - 1.5) * tw + 0.23), ff(L / 2 + 0.9, (i - 1.5) * tw + 0.27)]) for i in range(4)]), pl, 0.3)
    return o.out()

@d("maahl", "debris", "salt", "salt_plates")
def _(cx, cy, a, rng):
    """a dried salt pan: six-sided plates fitted like tiles, every one with its edges curled up into a rim and a
    crack or two across it"""
    u = rng.uniform; o = Obj("salt"); sp = u(2.0, 2.4); ff = frame(cx, cy, a)
    for i in range(-1, 2):
        for j in range(-1, 2):
            c = ff(i * sp + (j % 2) * sp / 2 - sp * 0.25, j * sp * 0.87)
            if (i or j) and (math.dist(c, (cx, cy)) > sp * 1.45 or rng.random() < 0.15): continue
            g = Polygon([pol(c[0], c[1], sp * 0.555 * u(0.9, 1.02), a + 0.5236 + 1.0472 * k + u(-0.07, 0.07)) for k in range(6)]); b = o.up(g, u(0.25, 0.32)); o.on(g.difference(inset(g, 0.5)), b, u(0.2, 0.3))
            if rng.random() < 0.5: t = u(0, 3.14); o.cut(wavy(pol(c[0], c[1], sp * 0.4, t), pol(c[0], c[1], sp * 0.4, t + math.pi + u(-0.5, 0.5)), rng, 0.2, 3, 0.22).intersection(inset(g, 0.6)), b, 0.16)
    return o.out()

@d("maahl", "made", "ruin", "stele")
def _(cx, cy, a, rng):
    """a round-headed standing stone fallen on its back: the border line, the sun disc with its rays in the head, rows
    of carved signs below; whole, or broken across with the two pieces lying a little apart"""
    u = rng.uniform; ff = frame(cx, cy, a); L = u(4.6, 5.8); W = u(2.6, 3.2); h = u(0.6, 0.85); o = Obj("ruin"); xt = L / 2 - W / 2
    g = U([rect(*ff(-W / 4, 0), L - W / 2, W, a), disc(*ff(xt, 0), W / 2, 24)]); ct = ff(xt, 0); rs = W * 0.2
    dec = [inset(g, 0.36).exterior.buffer(0.11)] + [barl(pol(ct[0], ct[1], rs + 0.2, a + t), pol(ct[0], ct[1], rs + 0.62, a + t), 0.22) for t in np.linspace(-1.9, 1.9, 7)] + [barl(ff(xt - rs - 0.5, -W / 2 + 0.6), ff(xt - rs - 0.5, W / 2 - 0.6), 0.2)]
    gl = ma_glyphs(ff, np.arange(-L / 2 + 0.95, xt - rs - 0.9, 0.85), np.arange(-W / 2 + 0.95, W / 2 - 0.7, 0.85), a, rng, 0.85)
    parts = [g]
    if rng.random() < 0.4:
        xs = u(-L * 0.25, L * 0.05); parts = sorted(polys_of(g.difference(wavy(ff(xs + u(-0.4, 0.4), -W), ff(xs + u(-0.4, 0.4), W), rng, 0.3, 4, 0.1))), key=lambda q: -q.area)[:2]
        if len(parts) == 2: dx, dy = pol(0, 0, u(0.5, 0.9), a + math.pi); parts[1] = affinity.rotate(affinity.translate(parts[1], dx, dy), u(-0.12, 0.12), origin=parts[1].centroid, use_radians=True) if parts[1].centroid.distance(Point(ct)) > parts[0].centroid.distance(Point(ct)) else parts[1]
    for q in parts:
        b = o.up(q, h); o.cut(U(dec + ([gl] if gl is not None else [])), b, 0.18)
        if q.contains(Point(ct)): o.on(disc(ct[0], ct[1], rs, 14), b, 0.15)
    return o.out()

@d("maahl", "made", "wood", "well_bucket")
def _(cx, cy, a, rng):
    """a well bucket and its rope: the staved pail with its hollow, the two ears on the rim and the iron bail fallen
    to one side (or the pail tipped over, its hoops showing), the rope run off to its coil"""
    u = rng.uniform; o = Obj("wood"); r = u(1.05, 1.3); ff = frame(cx, cy, a)
    if rng.random() < 0.55:
        g = U([disc(cx, cy, r, 20)] + [rect(*pol(cx, cy, r + 0.1, a + sv * 1.5708), 0.7, 0.6, a + sv * 1.5708) for sv in (-1, 1)]); b = o.up(g, u(1.3, 1.7)); o.cut(disc(cx, cy, r - 0.36, 16), b, u(0.5, 0.9))
        o.cut(U([barl(pol(cx, cy, r - 0.4, a + 0.3 + 0.6283 * k), pol(cx, cy, r + 0.1, a + 0.3 + 0.6283 * k), 0.24) for k in range(10)]), b, 0.16)
        pts = [pol(cx, cy, r + 0.35 + u(0.9, 1.2) * math.cos(t), a + t) for t in np.linspace(-1.35, 1.35, 12)]; o.st(ext(line(pts, 0.48), 0.3), "iron", foot=LineString(pts).buffer(0.4)); p0 = pol(cx, cy, r + 0.35, a + 2.4)
    else:
        L = u(2.3, 2.8); hoop = lambda s: 0.12 if abs(s - 0.2) < 0.07 or abs(s - 0.78) < 0.07 else 0.0
        o.st(ma_bar(ff(-L / 2, 0), ff(L / 2, 0), lambda s: r * (0.78 + 0.22 * s) + hoop(s), 0.45), foot=rect(cx, cy, L, 2 * r + 0.3, a))
        pts = [pol(*ff(L / 2, 0), 0.3 + 1.0 * math.cos(t), a + t) for t in np.linspace(-1.3, 1.3, 10)]; o.st(ext(line(pts, 0.48), 0.3), "iron", foot=LineString(pts).buffer(0.4)); p0 = ff(-L / 2 - 0.3, u(-0.4, 0.4))
    t = math.atan2(p0[1] - cy, p0[0] - cx) + u(-0.4, 0.4); c = pol(p0[0], p0[1], u(2.0, 2.6), t); Rc = u(1.15, 1.4); t1 = math.atan2(p0[1] - c[1], p0[0] - c[0])
    o.st(rod(wiggle(p0, pol(c[0], c[1], Rc, t1), rng, 0.3, 3) + spiral(c[0], c[1], Rc, 0.42, u(2.0, 2.6), t1, 34)[1:], 0.3, 1.1), "rope", foot=disc(c[0], c[1], Rc + 0.3))
    return o.out()

@d("maahl", "debris", "sandstone", "desert_rose")
def _(cx, cy, a, rng):
    """a desert rose: thin curved blades of crystal standing on edge and folded round one another like petals,
    low at the outside and rising to the bud in the middle"""
    u = rng.uniform; o = Obj("sandstone"); r = u(1.6, 2.1); t0 = u(0, 6.28); b = o.up(ell(cx, cy, r, r * u(0.88, 0.96), t0, 18), u(0.3, 0.4)); m = rng.randint(5, 6)
    for k in range(m): t = t0 + 6.2832 * k / m + u(-0.15, 0.15); c = pol(cx, cy, r * 0.45, t); o.on(line(arc(c[0], c[1], r * 0.44, t - u(0.9, 1.3), t + u(0.9, 1.3), 8), 0.5), b, u(0.3, 0.6))
    for k in range(3): t = t0 + 0.5 + 6.2832 * k / 3 + u(-0.2, 0.2); c = pol(cx, cy, r * 0.2, t); o.on(line(arc(c[0], c[1], r * 0.27, t - u(1.0, 1.4), t + u(1.0, 1.4), 8), 0.5), b, u(0.7, 0.95))
    o.on(disc(cx, cy, 0.3, 8), b, u(1.0, 1.2))
    return o.out()

@d("maahl", "made", "column", "fallen_capital")
def _(cx, cy, a, rng):
    """a column head lying on its side: a stub of the fluted shaft, the collar ring, the bell flaring out to the
    square slab that carried the beam; a drum of the same column may stand beside it"""
    u = rng.uniform; ff = frame(cx, cy, a); L = u(3.2, 4.0); r = u(0.95, 1.15); o = Obj("column")
    rf = lambda s: r * (1.0 if s < 0.3 else 1.16 if s < 0.4 else 1.0 + 0.62 * ((s - 0.4) / 0.6) ** 1.5)
    o.st(ma_bar(ff(-L / 2, 0), ff(L / 2 - 0.3, 0), rf, 0.4), cuts=[(barl(ff(-L / 2 - 0.2, v), ff(-L / 2 + (L - 0.3) * 0.28, v), 0.24), r * 1.12) for v in (-r * 0.5, 0.0, r * 0.5)], foot=rect(cx, cy, L, 3.3 * r, a))
    ab = o.up(rect(*ff(L / 2 + 0.15, 0), 0.9, r * 3.5, a), r * 2.3); o.cut(barl(ff(L / 2 + 0.15, -r * 2), ff(L / 2 + 0.15, r * 2), 0.22), ab, 0.16)
    if rng.random() < 0.55:
        c = ff(-L / 2 - u(1.6, 2.3), u(-1.0, 1.0)); dr = o.up(star(c[0], c[1], r * 1.04, r * 0.9, 10, u(0, 1)), u(0.8, 1.3)); o.cut(U([disc(c[0], c[1], 0.28, 8), ann(c[0], c[1], r * 0.66, r * 0.66 - 0.2, 16)]), dr, 0.18)
    return o.out()

@d("maahl", "debris", "glass", "fulgurite")
def _(cx, cy, a, rng):
    """a fulgurite: the knobbly glass tube lightning leaves in sand, forking as it goes, thinner at every fork, each
    broken end showing the hollow through it"""
    u = rng.uniform; ps = []; foot = [disc(cx, cy, 0.7)]; cuts = [(disc(cx, cy, 0.24, 8), 0.3)]
    def br(p, t, L, r, dep):
        e = pol(p[0], p[1], L, t); m = pol((p[0] + e[0]) / 2, (p[1] + e[1]) / 2, u(-0.4, 0.4), t + 1.57); foot.append(disc(e[0], e[1], 0.5)); ph = u(0, 6)
        ps.extend(rod([p, m, e], lambda q: r * (1 - 0.25 * q) * (1 + 0.1 * math.sin(q * L * 6.0 + ph)), 1.25))
        kids = rng.randint(1, 2) if dep < 2 else 0
        if kids:
            for k in range(kids): br(e if k == 0 else m, t + u(-0.9, 0.9), L * u(0.6, 0.8), r * 0.78, dep + 1)
        elif r * 0.75 > 0.38: cuts.append((disc(e[0], e[1], 0.2, 6), 0.25))
    br((cx, cy), a, u(2.4, 3.2), u(0.58, 0.7), 0)
    return hull(foot), [ST(ps, "glass", cuts=cuts)]

# ---------------------------------------------------------------------------------------------------- new
@d("maahl", "debris", "chitin", "worm_maw", ground=True)
def _(cx, cy, a, rng):
    """the mouth of a sand worm where it has come up: a ring of sand heaved up round the throat, and inside the lip
    the circle of long teeth all pointing to the middle"""
    u = rng.uniform; R = u(2.2, 2.9); n = rng.randint(7, 10); t0 = u(0, 6.28); sq = u(0.85, 1.0)
    lip = turn(cx, cy, [(R + 1.0, 0.0), (R + 0.45, 0.45), (R + 0.1, 0.62), (R - 0.15, 0.62)], 28, sq, a)
    th = []
    for k in range(n):
        t = t0 + 6.2832 * k / n + u(-0.08, 0.08); l = R * u(0.5, 0.68); p = pol(cx, cy, (R - 0.25) * (sq if abs(math.sin(t - a)) > 0.7 else 1.0), t)
        for f, hh in ((1.0, 0.42), (0.55, 0.7)): th += ext(tri(p, pol(p[0], p[1], l * f, t + math.pi), 0.8 * (0.6 + 0.4 * f)), hh)
    return ell(cx, cy, R + 1.1, (R + 1.1) * sq, a), [ST(lip, "sand", cuts=[(ell(cx, cy, R - 0.3, (R - 0.3) * sq, a), 0.14)]), ST(th, "tooth")]

@d("maahl", "growth", "chitin", "spore_pods")
def _(cx, cy, a, rng):
    """the pods some dune thing leaves: leathery eggs half sunk in the sand, ridged down their sides, most split
    open at the top in three or four lips with the hollow showing, one or two still shut"""
    u = rng.uniform; lay = []; foot = []; got = []
    for i in range(rng.randint(3, 6)):
        r = u(0.75, 1.1); x, y = pol(cx, cy, 0 if i == 0 else u(1.6, 3.0), a + i * 1.25 + u(-0.3, 0.3))
        if any(math.dist((x, y), (px, py)) < r + pr + 0.3 for px, py, pr in got): continue          # pods never touch: each is its own clean shape
        got.append((x, y, r)); h = r * u(1.1, 1.5); t = u(0, 6.28); sq = u(0.8, 0.95)
        cuts = [(barl(pol(x, y, r * 0.75, t + 6.2832 * j / 6), pol(x, y, r * 1.1, t + 6.2832 * j / 6), 0.22), h * 0.3) for j in range(6)]
        if rng.random() < 0.6: tg = t + 0.52; cuts.append((Polygon([pol(x, y, r * 0.1, tg + math.pi), pol(x, y, r * 1.3, tg - 0.3), pol(x, y, r * 1.3, tg + 0.3)]), h * 0.35))          # split down one side
        lay.append(ST(turn(x, y, [(r * 0.85, 0.0), (r, h * 0.3)] + crv(r, 0.32, h * 0.3, h, n=7), 16, sq, t), "chitin", cuts=cuts)); foot.append(disc(x, y, r + 0.2))
    return hull(foot), lay

@d("maahl", "made", "cloth", "water_skins")
def _(cx, cy, a, rng):
    """water skins laid down together: each a full bag swelling from its tied neck, the stopper and the knot of the
    cord at the neck, the carrying cord lying loose"""
    u = rng.uniform; o = Obj("leather")
    for i in range(rng.randint(2, 3)):
        c = pol(cx, cy, 0 if i == 0 else u(1.5, 2.0), a + 1.57 + i * 2.6); b = a + u(-0.6, 0.6); L = u(3.0, 3.8); w = u(0.95, 1.2); f2 = frame(c[0], c[1], b)
        rf = lambda s, w=w: w * math.sin(min(1.0, s / 0.72) * 1.5708) ** 0.6 * (1.0 if s < 0.6 else max(0.3, 1 - ((s - 0.6) / 0.3) ** 1.6)) if s < 0.88 else (0.3 if s < 0.95 else 0.42)
        o.st(spin(c[0], c[1], b, L, lambda t, L=L: rf((t + L / 2) / L) + 0.02, 0.85), foot=rect(c[0], c[1], L, 2 * w, b))
        if rng.random() < 0.7: e = f2(u(-0.5, 0.8), rng.choice((-1, 1)) * (w + u(0.8, 1.3))); pts = [f2(L * 0.4, 0), e, f2(-L * 0.25, 0)]; o.st(rod(pts, 0.26, 0.9), "rope", foot=LineString(pts).buffer(0.4))
    return o.out()


_OWN = set(globals())
