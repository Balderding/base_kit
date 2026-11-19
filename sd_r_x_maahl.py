"""Third drawing, maahl: the dune world's ruins redrawn at their true height beside a 32 mm figure (1 m = 18 mm
printed). An obelisk lies as thick as it is wide, a colossal head stands out of the sand to a man's waist, a stele is
a slab of real thickness. Carved stone is built in flat layers so its lines stay crisp; jars and teeth are rounded."""
import math
from sd_reg import d

def xm_bar(p, q, rf, k=0.4):
    """a round bar from p to q with flat ends, solid underneath, its axis k radii off the ground"""
    L = math.dist(p, q) or 1.0; m = max(8, int(L / 0.11)); ux, uy = (q[0] - p[0]) / L, (q[1] - p[1]) / L; pt = lambda s, w: (p[0] + ux * s * L - uy * w, p[1] + uy * s * L + ux * w)
    S = [i / m for i in range(m + 1)]; R = [max(0.0, rf(s) if callable(rf) else rf) for s in S]; C = [k * r for r in R]
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

def xm_glyphs(ff, xs, vs, a, rng, keep=0.8):
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


@d("maahl", "made", "ruin", "buried_head")
def _(cx, cy, a, rng):
    """the head of a fallen colossus lying face up, standing out of the sand to a man's waist: the striped head-cloth
    with its lappets, the serpent on the brow, the long face with its brow ridge, the nose, the eye hollows (three
    eyes on some: this is not our world), the lips, the plaited beard at the chin; a drift lies over the beard"""
    u = rng.uniform; ff = frame(cx, cy, a); o = Obj("ruin"); s = u(1.05, 1.3)
    hd = Polygon([ff(-2.3 * s, -1.4 * s), ff(0.3 * s, -2.1 * s), ff(1.9 * s, -1.9 * s), ff(1.9 * s, 1.9 * s), ff(0.3 * s, 2.1 * s), ff(-2.3 * s, 1.4 * s)]).buffer(-0.3).buffer(0.3, 6)
    fc = ell(*ff(0.15 * s, 0), 1.6 * s, 1.12 * s, a, 28); b = o.up(hd, u(1.0, 1.2))
    o.cut(U([fc.buffer(k).exterior.buffer(0.12) for k in (0.42, 0.84, 1.26)]).intersection(inset(hd, 0.1)), b, 0.2)
    f = o.on(fc, b, u(0.6, 0.75))
    eyes = [ell(*ff(-0.3 * s, sv * 0.58 * s), 0.27 * s, 0.4 * s, a, 12) for sv in (-1, 1)] + ([ell(*ff(-1.05 * s, 0), 0.3 * s, 0.22 * s, a, 12)] if rng.random() < 0.4 else [])
    o.cut(U(eyes), f, 0.35)
    o.on(U([rect(*ff(-0.72 * s, sv * 0.6 * s), 0.46, 0.95 * s, a + sv * 0.15) for sv in (-1, 1)]), f, 0.2)
    o.on(Polygon([ff(-0.6 * s, -0.23), ff(0.4 * s, -0.38), ff(0.4 * s, 0.38), ff(-0.6 * s, 0.23)]), f, u(0.45, 0.55))
    lp = o.on(ell(*ff(0.95 * s, 0), 0.3 * s, 0.6 * s, a, 12), f, 0.2); o.cut(rect(*ff(0.95 * s, 0), 0.16, 0.8 * s, a), lp, 0.15)
    ur = o.on(ell(*ff(-1.45 * s, 0), 0.32, 0.26, a, 10), b, 0.75 if rng.random() < 0.7 else 0.4)
    bg = rect(*ff(1.75 * s, 0), 0.9 * s, 0.8 * s, a); bd = o.up(bg, u(1.1, 1.3)); o.cut(U([barl(ff(1.75 * s + x, -1), ff(1.75 * s + x, 1), 0.12) for x in (-0.25 * s, 0.0, 0.25 * s)]), bd, 0.12)
    if rng.random() < 0.3: o.cut(wavy(ff(u(-1, 1), -2.2 * s), ff(u(-1, 1), 2.2 * s), rng, 0.4, 4, 0.3), f, 0.35)
    c = ff(u(2.0, 2.4) * s, u(-0.8, 0.8)); o.st(dome(c[0], c[1], u(1.5, 2.0), u(1.1, 1.4), sq=u(0.6, 0.8), a=a + 1.57 + u(-0.4, 0.4)), "sand", foot=disc(c[0], c[1], 1.5))
    return o.out()

@d("maahl", "made", "obelisk", "obelisk_tip")
def _(cx, cy, a, rng):
    """the top of a fallen obelisk, as thick as it is wide: the square shaft narrowing to the pyramid point, a framed
    column of carved signs down its face under a cartouche, a corner knocked off, the foot broken off ragged; a
    further piece of the shaft may lie behind it"""
    u = rng.uniform; ff = frame(cx, cy, a); L = u(5.5, 7.0); w0 = u(1.9, 2.3); w1 = w0 * 0.74; Lt = u(1.5, 1.9); h = w1 * u(0.9, 1.0); o = Obj("obelisk"); x0 = -L / 2; x1 = L / 2 - Lt
    shaft = Polygon([ff(x0 + u(-0.3, 0.3), v) for v in np.linspace(-w0 / 2, w0 / 2, 5)] + [ff(x1, w1 / 2), ff(x1, -w1 / 2)]); b = o.up(shaft, h)
    Lp = x1 - x0 - 1.2; xm = (x0 + x1) / 2 + 0.15; o.groove(rect(*ff(xm, 0), Lp, w1 - 0.7, a), b, 0.2, 0.18)
    o.cut(xm_glyphs(ff, np.arange(xm - Lp / 2 + 0.6, xm + Lp / 2 - 1.5, 0.85), [0.0], a, rng, 0.9), b, 0.2)
    o.cut(rrect(*ff(xm + Lp / 2 - 0.75, 0), 1.0, w1 - 1.05, a, 0.25).exterior.buffer(0.08), b, 0.15)
    sv = rng.choice((-1, 1)); o.cut(Polygon([ff(x0 + u(0.6, 1.4), sv * w0), ff(x0 + u(1.6, 2.4), sv * w0), ff(x0 + 1.5, sv * (w0 / 2 - 0.45))]), b, u(0.4, 0.6))
    ps = []
    for f, hh in ((1.0, 0.36), (0.68, 0.62), (0.4, 0.84), (0.16, 1.0)): ps += ext(Polygon([ff(x1 + 0.06, -w1 / 2 * f), ff(x1 + 0.06 + Lt * (0.3 + 0.7 * f), 0), ff(x1 + 0.06, w1 / 2 * f)]), h * hh)
    o.st(ps, foot=Polygon([ff(x1, -w1 / 2), ff(L / 2, 0), ff(x1, w1 / 2)]))
    if rng.random() < 0.97:
        l2 = u(1.4, 2.4); c = ff(x0 - l2 / 2 - u(0.6, 1.0), u(-0.4, 0.4)); t = a + u(-0.3, 0.3); f2 = frame(c[0], c[1], t); w2 = w0 * 1.04
        g = Polygon([f2(-l2 / 2 + u(-0.25, 0.25), v) for v in np.linspace(-w2 / 2, w2 / 2, 4)] + [f2(l2 / 2 + u(-0.25, 0.25), v) for v in np.linspace(w2 / 2, -w2 / 2, 4)])
        b2 = o.up(g, h * u(0.9, 1.0)); o.cut(U([barl(f2(-l2, sv * (w2 / 2 - 0.45)), f2(l2, sv * (w2 / 2 - 0.45)), 0.2) for sv in (-1, 1)]), b2, 0.18); o.cut(xm_glyphs(f2, np.arange(-l2 / 2 + 0.6, l2 / 2 - 0.4, 0.85), [0.0], t, rng, 0.9), b2, 0.2)
    return o.out()

@d("maahl", "made", "ruin", "sundial")
def _(cx, cy, a, rng):
    """a sundial on its stepped eight-sided plinth: two steps up to the dial plate with its ring and the hour lines
    cut round it, the gnomon standing up from the middle as a stepped fin to the rim; cracked across on some, sand
    banked against one side"""
    u = rng.uniform; R = u(2.0, 2.6); o = Obj("ruin")
    b0 = o.up(ngon(cx, cy, R + 0.55, 8, a + 0.39), u(0.55, 0.7)); b = o.on(ngon(cx, cy, R + 0.2, 8, a + 0.39), b0, u(0.5, 0.6))
    dl = o.on(disc(cx, cy, R - 0.1, 32), b, 0.3)
    hrs = [a + 0.95 + k * (6.2832 - 1.9) / 10 for k in range(11)]
    o.cut(U([ann(cx, cy, R * 0.6, R * 0.6 - 0.2, 24)] + [barl(pol(cx, cy, R * (0.66 if k % 2 == 0 else 0.78), t), pol(cx, cy, R * 0.92, t), 0.24) for k, t in enumerate(hrs)]).difference(rect(*pol(cx, cy, R * 0.45, a), R, 0.9, a)), dl, 0.2)
    if rng.random() < 0.35: o.cut(wavy(pol(cx, cy, R * 1.1, a + 2.0), pol(cx, cy, R * 1.1, a + 4.6), rng, 0.4, 4, 0.3).difference(disc(cx, cy, 0.5)), dl, 0.3)
    g1 = o.on(rect(*pol(cx, cy, R * 0.43, a), R * 0.82, 0.5, a), dl, u(0.45, 0.55)); g2 = o.on(rect(*pol(cx, cy, R * 0.3, a), R * 0.56, 0.5, a), dl, u(0.95, 1.1))
    o.on(rect(*pol(cx, cy, R * 0.17, a), R * 0.3, 0.5, a), dl, u(1.35, 1.5))
    if rng.random() < 0.4: t = a + u(1.5, 4.7); c = pol(cx, cy, R + 0.6, t); o.st(dome(c[0], c[1], u(1.2, 1.6), u(0.7, 1.0), sq=0.6, a=t + 1.57), "sand", foot=disc(c[0], c[1], 0.9))
    return o.out()

@d("maahl", "made", "clay", "amphorae")
def _(cx, cy, a, rng):
    """wine jars in the sand at their real size: lying ones with the pointed foot, the swelling belly ringed with the
    potter's lines, the shoulder drawing in to the narrow neck and the flared lip, a handle each side from shoulder to
    neck; one may stand sunk to its belly in a drift; shards of a broken one beside them"""
    u = rng.uniform; o = Obj("clay"); n = rng.randint(1, 3); stand = rng.random() < 0.35
    for i in range(n):
        c = pol(cx, cy, 0 if n == 1 else 1.25, a + 1.57 + i * 6.2832 / n); b = a + u(-0.5, 0.5) + (math.pi if rng.random() < 0.3 else 0); L = u(3.8, 4.6); r = u(0.85, 1.0); f2 = frame(c[0], c[1], b)
        if stand and i == n - 1:
            hb = u(1.0, 1.2); hn = hb + r * 1.1
            o.st(dome(c[0], c[1], r + 1.0, hb * 0.75, sq=u(0.75, 0.95), a=b), "sand", foot=disc(c[0], c[1], r + 0.8))
            o.st(turn(c[0], c[1], [(r, 0.0), (r, hb)] + crv(r, 0.36, hb, hn, n=6) + [(0.36, hn + 0.6), (0.52, hn + 0.65), (0.52, hn + 0.85)], 24) + [s for sv in (-1, 1) for s in ext(capl(f2(0, sv * 0.4), f2(0, sv * (r * 0.7 + 0.15)), 0.36), hn + 0.4)],
                 cuts=[(disc(c[0], c[1], 0.3, 10), hn + 0.45)], foot=disc(c[0], c[1], r))
            continue
        def rf(s, r=r):
            ring = -0.04 if any(abs(s - y) < 0.012 for y in (0.3, 0.4, 0.5)) else 0.0
            if s < 0.07: return 0.3
            if s < 0.6: return 0.3 + (r - 0.3) * math.sin((s - 0.07) / 0.53 * 1.5708) ** 0.8 + ring
            if s < 0.76: return r - (r - 0.36) * ((s - 0.6) / 0.16) ** 1.4
            return 0.36 if s < 0.9 else 0.55
        o.st(ext(U([line([f2(L * 0.12, sv * r * 0.5), f2(L * 0.2, sv * (r * 0.5 + 0.5)), f2(L * 0.36, sv * (r * 0.5 + 0.4)), f2(L * 0.4, sv * 0.3)], 0.42) for sv in (-1, 1)]), r * 1.25), foot=rect(c[0], c[1], L, 2 * r + 0.6, b))
        o.st(xm_bar(f2(-L / 2, 0), f2(L / 2, 0), rf, 0.75), cuts=[(disc(*f2(L / 2 - 0.08, 0), 0.25, 8), 0.5)], foot=rect(c[0], c[1], L, 2 * r, b))
    for i in range(rng.randint(2, 4)): p = pol(cx, cy, u(2.6, 3.4), a + u(-0.6, 0.6) + (i % 2) * math.pi); o.st(ext(stone(p[0], p[1], u(0.45, 0.65), rng, 0.7, 4), 0.32), foot=disc(p[0], p[1], 0.6))
    return o.out()

@d("maahl", "made", "ruin", "arch_fragment")
def _(cx, cy, a, rng):
    """a fallen arch lying in its stones, each block as deep as a real voussoir: wedge-cut blocks still in their
    curve, their faces dressed with a raised panel and a moulding of two lines along them, the keystone standing
    proud with its boss, a block or two knocked out of line"""
    u = rng.uniform; R = u(3.2, 4.4); w = u(1.4, 1.7); span = u(1.4, 2.2); n = max(3, int(span * R / 1.45)); o = Obj("ruin"); kq = n // 2; a0 = a - span / 2
    for k in range(n):
        if k != kq and n > 3 and rng.random() < 0.12: continue
        t0 = a0 + span * k / n + 0.04; t1 = a0 + span * (k + 1) / n - 0.04; dr = u(-0.2, 0.2) if k != kq else 0.0; ex = 0.3 if k == kq else 0.0; c = pol(cx, cy, u(0, 0.15), u(0, 6.28))
        g = Polygon(arc(c[0], c[1], R + ex + dr, t0, t1, 4) + arc(c[0], c[1], R - w + dr, t1, t0, 4)); b = o.up(g, u(1.15, 1.4) + ex * 0.8)
        pn = o.on(inset(g, 0.2), b, 0.14)
        o.cut(U([line(arc(c[0], c[1], R - w * f + dr, t0 - 0.1, t1 + 0.1, 5), 0.2) for f in (0.3, 0.7)]), pn, 0.16)
        if k == kq: m = pol(c[0], c[1], R - w * 0.5, (t0 + t1) / 2); o.on(disc(m[0], m[1], 0.32, 10), pn, 0.22)
        elif rng.random() < 0.3: o.cut(Polygon([pol(c[0], c[1], R + dr + 0.3, t0 - 0.05), pol(c[0], c[1], R + dr - u(0.4, 0.7), t0 - 0.05), pol(c[0], c[1], R + dr + 0.3, t0 + u(0.1, 0.2))]), b, u(0.3, 0.5))
    for i in range(rng.randint(0, 2)): p = pol(cx, cy, R - w - u(0.8, 1.6), a + u(-span / 2, span / 2)); o.st(rock(p[0], p[1], u(0.4, 0.55), u(0.45, 0.6), rng), foot=disc(p[0], p[1], 0.5))
    return o.out()

@d("maahl", "made", "pavers", "mosaic")
def _(cx, cy, a, rng):
    """a patch of mosaic floor left in the sand on its bed of mortar: its border line, a field of tesserae in their
    grid, a figure of rings or lozenges with rays between them in the middle, one corner broken away and the loose
    cubes of it lying about"""
    u = rng.uniform; ff = frame(cx, cy, a); L = u(4.4, 5.6); W = L * u(0.75, 1.0); o = Obj("pavers"); g0 = rect(cx, cy, L, W, a)
    sx, sy = rng.choice((-1, 1)), rng.choice((-1, 1)); bite = blob(*ff(sx * L / 2, sy * W / 2), u(1.2, 1.8), rng, 4); g = g0.difference(bite)
    if g.geom_type != "Polygon": g = max(polys_of(g), key=lambda q: q.area)
    b = o.up(g, u(0.42, 0.5)); Rm = min(L, W) / 2 - 0.75; k = rng.choice((4, 8, 24)); t0 = a + (0.785 if k == 4 and rng.random() < 0.5 else 0.0)
    fig = [inset(g0, 0.42).exterior.buffer(0.11)] + [ngon(cx, cy, Rm * f, k, t0).exterior.buffer(0.11) for f in (1.0, 0.5)] + [barl(pol(cx, cy, Rm * 0.56, t0 + 6.2832 * j / 8 + 0.39), pol(cx, cy, Rm * 0.86, t0 + 6.2832 * j / 8 + 0.39), 0.22) for j in range(8)]
    o.cut(U(fig), b, 0.17); o.on(disc(cx, cy, Rm * 0.2, 12), b, 0.14)
    field = inset(g0, 0.62).difference(ngon(cx, cy, Rm * 1.0, k, t0).buffer(0.15))
    st = 0.36; grid = U([barl(ff(x, -W), ff(x, W), 0.1) for x in np.arange(-L / 2, L / 2, st)] + [barl(ff(-L, v), ff(L, v), 0.1) for v in np.arange(-W / 2, W / 2, st)])
    o.cut(grid.intersection(field), b, 0.08)
    for i in range(rng.randint(3, 6)): p = ff(sx * (L / 2 - u(-0.3, 1.2)), sy * (W / 2 - u(-0.3, 1.2))); o.st(ext(rect(p[0], p[1], 0.5, 0.5, u(0, 1.5)), u(0.28, 0.36)), foot=disc(p[0], p[1], 0.5))
    return o.out()

@d("maahl", "made", "ruin", "sphinx_paw")
def _(cx, cy, a, rng):
    """the paw of a great stone beast on what is left of its plinth, as high as a man's thigh: the foreleg with a
    carved band round it, four long toes rounded at the knuckle with deep grooves between them and a blunt claw
    before each, the plinth's moulded edge"""
    u = rng.uniform; ff = frame(cx, cy, a); L = u(4.0, 4.8); W = u(2.9, 3.5); h = u(1.7, 2.0); o = Obj("ruin"); tw = W / 4
    pg = Polygon([ff(-L / 2 - 0.2 + u(-0.3, 0.3), v) for v in np.linspace(-W / 2 - 0.5, W / 2 + 0.5, 5)] + [ff(L / 2 + 1.8, W / 2 + 0.5), ff(L / 2 + 1.8, -W / 2 - 0.5)]); pl = o.up(pg, u(0.5, 0.6))
    o.cut(barl(ff(L / 2 + 1.55, -W), ff(L / 2 + 1.55, W), 0.14).intersection(pg), pl, 0.12)
    ag = rrect(*ff(-0.75, 0), L - 1.5, W - 0.1, a, 0.5).intersection(inset(pg, 0.2)); arm = o.on(ag, pl, h)
    o.on(rect(*ff(-L * 0.28, 0), 0.62, W - 0.5, a), arm, 0.2); o.cut(U([barl(ff(-L * 0.05, (i - 1) * tw), ff(L / 2, (i - 1) * tw), 0.26) for i in range(3)]), arm, 0.35)
    o.st([s for i in range(4) for s in dome(*ff(L / 2 - 0.3, (i - 1.5) * tw), 1.05, h * u(0.85, 0.95), sq=(tw / 2 - 0.02) / 1.05, a=a)], par=pl)
    o.on(U([Polygon([ff(L / 2 + 0.9, (i - 1.5) * tw - 0.27), ff(L / 2 + 1.5, (i - 1.5) * tw - 0.2), ff(L / 2 + 1.5, (i - 1.5) * tw + 0.2), ff(L / 2 + 0.9, (i - 1.5) * tw + 0.27)]) for i in range(4)]), pl, 0.45)
    return o.out()

@d("maahl", "made", "ruin", "stele")
def _(cx, cy, a, rng):
    """a round-headed standing stone fallen on its back, a slab of real thickness: the border line, the winged sun
    disc in the head, rows of carved signs below; whole, or broken across with the two pieces lying a little apart"""
    u = rng.uniform; ff = frame(cx, cy, a); L = u(4.6, 5.8); W = u(2.6, 3.2); h = u(1.05, 1.3); o = Obj("ruin"); xt = L / 2 - W / 2
    g = U([rect(*ff(-W / 4, 0), L - W / 2, W, a), disc(*ff(xt, 0), W / 2, 24)]); ct = ff(xt, 0); rs = W * 0.17
    dec = [inset(g, 0.36).exterior.buffer(0.11)] + [barl(pol(ct[0], ct[1], rs + 0.25, a + t), pol(ct[0], ct[1], rs + 0.55, a + t), 0.2) for t in np.linspace(-1.2, 1.2, 5)] + [barl(ff(xt - rs - 0.5, -W / 2 + 0.6), ff(xt - rs - 0.5, W / 2 - 0.6), 0.2)]
    gl = xm_glyphs(ff, np.arange(-L / 2 + 0.95, xt - rs - 0.9, 0.85), np.arange(-W / 2 + 0.95, W / 2 - 0.7, 0.85), a, rng, 0.85)
    wings = U([disc(ct[0], ct[1], rs, 14)] + [Polygon([pol(ct[0], ct[1], rs * 0.6, a + sv * 1.5708 + 0.5 * sv), pol(ct[0], ct[1], W * 0.42, a + sv * 1.5708 + 0.25 * sv), pol(ct[0], ct[1], W * 0.4, a + sv * 1.5708 - 0.15 * sv), pol(ct[0], ct[1], rs * 0.6, a + sv * 1.5708 - 0.5 * sv)]) for sv in (-1, 1)])
    parts = [g]
    if rng.random() < 0.4:
        xs = u(-L * 0.25, L * 0.05); parts = sorted(polys_of(g.difference(wavy(ff(xs + u(-0.4, 0.4), -W), ff(xs + u(-0.4, 0.4), W), rng, 0.3, 4, 0.1))), key=lambda q: -q.area)[:2]
        if len(parts) == 2: dx, dy = pol(0, 0, u(0.5, 0.9), a + math.pi); parts[1] = affinity.rotate(affinity.translate(parts[1], dx, dy), u(-0.12, 0.12), origin=parts[1].centroid, use_radians=True) if parts[1].centroid.distance(Point(ct)) > parts[0].centroid.distance(Point(ct)) else parts[1]
    for q in parts:
        b = o.up(q, h); o.cut(U(dec + ([gl] if gl is not None else [])), b, 0.2)
        if q.contains(Point(ct)): w_ = o.on(wings.intersection(inset(q, 0.4)), b, 0.18); o.cut(U([barl(pol(ct[0], ct[1], rs + 0.1, a + sv * 1.5708), pol(ct[0], ct[1], W * 0.38, a + sv * 1.5708), 0.1) for sv in (-1, 1)]), w_, 0.08)
    return o.out()

@d("maahl", "made", "column", "fallen_capital")
def _(cx, cy, a, rng):
    """a column head lying on its side: a stub of the fluted shaft, the collar ring, the bell flaring out with the
    lotus leaves carved up it to the square slab that carried the beam; a drum of the same column may stand beside
    it to half a man's height"""
    u = rng.uniform; ff = frame(cx, cy, a); L = u(3.2, 4.0); r = u(0.95, 1.15); o = Obj("column")
    rf = lambda s: r * (1.0 if s < 0.3 else 1.16 if s < 0.4 else 1.0 + 0.62 * ((s - 0.4) / 0.6) ** 1.5)
    x0 = -L / 2; x4 = x0 + (L - 0.3) * 0.45
    cuts = [(barl(ff(-L / 2 - 0.2, v), ff(-L / 2 + (L - 0.3) * 0.28, v), 0.24), r * 1.12) for v in (-r * 0.5, 0.0, r * 0.5)]
    cuts += [(Polygon([ff(x4, v - 0.12), ff(L / 2 - 0.4, v * 1.5 - 0.15), ff(L / 2 - 0.4, v * 1.5 + 0.15), ff(x4, v + 0.12)]), r * 1.3) for v in (-r * 0.45, 0.0, r * 0.45)]
    o.st(xm_bar(ff(-L / 2, 0), ff(L / 2 - 0.3, 0), rf, 0.4), cuts=cuts, foot=rect(cx, cy, L, 3.3 * r, a))
    ab = o.up(rect(*ff(L / 2 + 0.15, 0), 0.9, r * 3.5, a), r * 2.3); o.cut(barl(ff(L / 2 + 0.15, -r * 2), ff(L / 2 + 0.15, r * 2), 0.22), ab, 0.16)
    if rng.random() < 0.55:
        c = ff(-L / 2 - u(1.6, 2.3), u(-1.0, 1.0)); dr = o.up(star(c[0], c[1], r * 1.04, r * 0.9, 10, u(0, 1)), u(1.7, 2.3)); o.cut(U([disc(c[0], c[1], 0.28, 8), ann(c[0], c[1], r * 0.66, r * 0.66 - 0.2, 16)]), dr, 0.18)
    return o.out()

@d("maahl", "debris", "chitin", "worm_maw", ground=True)
def _(cx, cy, a, rng):
    """the mouth of a sand worm where it has come up: a ring of sand heaved up round the throat to a man's knee, the
    ribbed gum inside the lip and on it the circle of long hooked teeth all pointing to the middle, a second row of
    lesser teeth behind them"""
    u = rng.uniform; R = u(2.2, 2.9); n = rng.randint(7, 10); t0 = u(0, 6.28); sq = u(0.85, 1.0)
    lip = turn(cx, cy, [(R + 1.0, 0.0), (R + 0.6, 0.7), (R + 0.25, 1.15), (R - 0.1, 1.25)], 28, sq, a)
    gum = [s for k in range(14) for s in ext(rect(*pol(cx, cy, R - 0.35, t0 + 6.2832 * k / 14 + 0.2), 0.7, 0.3, t0 + 6.2832 * k / 14 + 0.2).intersection(ell(cx, cy, R - 0.05, (R - 0.05) * sq, a)), 0.75)]
    th = []
    for k in range(n):
        t = t0 + 6.2832 * k / n + u(-0.08, 0.08); l = R * u(0.5, 0.66); p = pol(cx, cy, (R - 0.35) * (sq if abs(math.sin(t - a)) > 0.7 else 1.0), t)
        for f, hh in ((1.0, 0.6), (0.68, 1.05), (0.38, 1.45)): th += ext(tri(p, pol(p[0], p[1], l * f, t + math.pi), 0.85 * (0.55 + 0.45 * f)), hh)
        t2 = t + 3.1416 / n; p2 = pol(cx, cy, (R - 0.3) * 0.92, t2)
        th += ext(tri(p2, pol(p2[0], p2[1], l * 0.42, t2 + math.pi), 0.5), 0.7)
    return ell(cx, cy, R + 1.1, (R + 1.1) * sq, a), [ST(lip, "sand", cuts=[(ell(cx, cy, R - 0.3, (R - 0.3) * sq, a), 0.14)]), ST(gum, "flesh"), ST(th, "tooth")]


_OWN = set(globals())
