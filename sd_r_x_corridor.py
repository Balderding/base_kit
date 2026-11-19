"""Third drawing, corridor: the deck details redrawn at their true size beside a 32 mm figure (1 m = 18 mm printed).
A console stands to a man's chest, a gas bottle is as thick as a real one, a pallet carries crates of a real height.
Hard-edged things are built in flat layers (Obj) so their seams, bolts and slots stay crisp; nothing overhangs."""
import math
from sd_reg import d

def xc_bar(p, q, rf, k=1.0):
    """a round bar from p to q lying on the ground, solid underneath (its sides upright up to the axis, k radii off
    the ground, then round over the top): rf is its radius, a number or a function of 0..1 along it"""
    L = math.dist(p, q) or 1.0; m = max(8, int(L / 0.1)); ux, uy = (q[0] - p[0]) / L, (q[1] - p[1]) / L; pt = lambda s, w: (p[0] + ux * s * L - uy * w, p[1] + uy * s * L + ux * w)
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

def xc_hose(o, pts, r=0.26, kind="cable"):
    o.st(rod(pts, r, 1.1), kind, foot=LineString(pts).buffer(r + 0.15))


@d("corridor", "made", "metal", "terminal")
def _(cx, cy, a, rng):
    """a deck console standing as high as a man's chest: the kick plinth, the cabinet with its access door and
    louvres, the desk with rows of keys, a trackball and two dials, the tilted screen in its bezel (cracked on some),
    the tall back housing with its vents and a row of status lamps; the kick plate is scuffed with a line"""
    u = rng.uniform; ff = frame(cx, cy, a); L = u(3.3, 3.9); W = u(2.5, 3.0); o = Obj("metal")
    Hd = u(3.6, 4.1); b0 = o.up(rrect(cx, cy, L, W, a, 0.2), 0.45)
    body = o.on(rrect(cx, cy, L - 0.3, W - 0.3, a, 0.15), b0, Hd - 0.45)
    # the desk (front, x > 0) and the screen housing (back, x < 0)
    xs = -L * 0.08
    o.groove(rect(*ff(L * 0.22, 0), L * 0.4, W - 1.0, a), body, 0.16, 0.12)
    keys = [ff(L * 0.08 + i * 0.44, v) for i in range(3) for v in np.arange(-W / 2 + 0.6, W * 0.12, 0.42)]
    o.on(U([rect(x, y, 0.32, 0.32, a) for x, y in keys]), body, 0.12)
    o.on(disc(*ff(L * 0.3, W * 0.3), 0.36, 12), body, 0.22); o.studs([ff(L * 0.1, W * 0.3)], body, 0.24, 0.16)
    sc = o.on(rect(*ff(xs - L * 0.12, 0), L * 0.26, W - 0.6, a), body, u(0.8, 1.0))
    scr = rect(*ff(xs - L * 0.12, -W * 0.04), L * 0.26 - 0.45, W - 1.25, a); o.cut(scr, sc, 0.16)
    if rng.random() < 0.4:
        c = ff(xs - L * 0.12 + u(-0.2, 0.2), u(-0.5, 0.4)); o.cut(U([barl(c, pol(c[0], c[1], u(0.5, 1.2), u(0, 6.28)), 0.12) for _ in range(rng.randint(3, 5))]).intersection(scr), sc, 0.3)
    bk = o.on(rect(*ff(-L / 2 + 0.55, 0), 0.8, W - 0.6, a), body, u(1.6, 1.9))
    o.cut(U([barl(ff(-L / 2 + 0.33, v), ff(-L / 2 + 0.77, v), 0.13) for v in np.arange(-W / 2 + 0.6, W * 0.1, 0.3)]), bk, 0.14)
    o.studs([ff(-L / 2 + 0.55, W / 2 - 0.65 - 0.36 * i) for i in range(3)], bk, 0.14, 0.14)
    o.groove(rect(*ff(L * 0.2, 0), L * 0.3, W - 0.3, a).intersection(inset(rrect(cx, cy, L - 0.3, W - 0.3, a, 0.15), 0.05)), b0, 0.12, 0.1)
    return o.out()

@d("corridor", "made", "plate", "deck_hatch")
def _(cx, cy, a, rng):
    """a pressure hatch in the deck: the raised coaming with hazard stripes and a ring of bolts, two hinge knuckles on
    their pin, the cover with its seal groove and the locking dogs round its rim, the hand wheel of three or four
    spokes on its hub (or on the square kind a lever and a grab handle), the pressure tell-tale beside it"""
    u = rng.uniform; ff = frame(cx, cy, a); S = u(3.8, 4.5); R = S * 0.35; o = Obj("plate"); g = rrect(cx, cy, S, S, a, 0.35)
    b = o.up(g, 0.4); rnd = rng.random() < 0.7
    hole = disc(cx, cy, R + 0.1, 28) if rnd else rrect(cx, cy, S * 0.68, S * 0.68, a, 0.3)
    ring = g.difference(hole.buffer(0.25))
    o.cut(U([barl(ff(t - S, -S), ff(t + S, S), 0.22) for t in np.arange(-S, S, 0.55)]).intersection(inset(g, 0.25)).difference(hole.buffer(0.45)), b, 0.1)
    o.studs([ff(sx * (S / 2 - 0.32), sy * (S / 2 - 0.32)) for sx in (-1, 1) for sy in (-1, 1)] + [ff(sx * (S / 2 - 0.3), 0) for sx in (-1, 1)] + [ff(0, S / 2 - 0.3)], b, 0.22, 0.13)
    hy = -S / 2 + 0.3; o.on(U([rect(*ff(x, hy), 0.7, 0.5, a) for x in (-S * 0.2, S * 0.2)]), b, 0.32); o.on(rect(*ff(0, hy), S * 0.62, 0.3, a), b, 0.2)
    cov = o.on(hole.buffer(-0.12), b, 0.28)
    if rnd:
        o.groove(disc(cx, cy, R - 0.32, 28), cov, 0.14, 0.1)
        nd = rng.randint(6, 8); t0 = u(0, 6.28)
        o.on(U([rect(*pol(cx, cy, R - 0.05, t0 + 6.2832 * k / nd), 0.55, 0.36, t0 + 6.2832 * k / nd) for k in range(nd)]), b, 0.38)
        n = rng.choice((3, 4)); rw = R * 0.6; t1 = u(0, 6.28)
        o.on(U([ann(cx, cy, rw, rw - 0.36, 24)] + [capl((cx, cy), pol(cx, cy, rw - 0.18, t1 + 6.2832 * k / n), 0.36) for k in range(n)]), cov, 0.3)
        o.on(disc(cx, cy, 0.42, 10), cov, 0.5)
    else:
        t0 = a + rng.choice((0, 1.5708)); o.on(capl(pol(cx, cy, 0.0, t0), pol(cx, cy, S * 0.26, t0), 0.42), cov, 0.28); o.on(disc(cx, cy, 0.45, 10), cov, 0.42)
        o.groove(rrect(cx, cy, S * 0.68 - 0.75, S * 0.68 - 0.75, a, 0.2), cov, 0.14, 0.1)
        o.handle(ff(-S * 0.14, S * 0.2), ff(S * 0.14, S * 0.2), cov, 0.36, 0.3)
    c = ff(S / 2 - 0.55, S / 2 - 0.55); k = o.on(disc(c[0], c[1], 0.3, 10), b, 0.2); o.cut(disc(c[0], c[1], 0.13, 8), k, 0.1)
    return o.out()

@d("corridor", "made", "metal", "hover_pallet")
def _(cx, cy, a, rng):
    """a grav pallet set down: the thick deck with its bumper rail, a lifter pod at each corner with its emitter ring,
    lashing eyes along the sides, the tow bar with its grip and display; it carries crates of a real size (lid line,
    corner irons, a stencil, handles, straps over them) or a standing drum, or runs empty showing its tread plates"""
    u = rng.uniform; ff = frame(cx, cy, a); L = u(4.8, 5.6); W = u(3.2, 3.7); o = Obj("metal")
    pods = [ff(sx * (L / 2 - 0.75), sy * (W / 2 - 0.55)) for sx in (-1, 1) for sy in (-1, 1)]
    pd = o.up(U([disc(x, y, 0.72, 14) for x, y in pods]), 0.75); o.cut(U([ann(x, y, 0.5, 0.32, 10) for x, y in pods]), pd, 0.14)
    dk = rrect(*ff(0.1, 0), L - 0.4, W - 0.2, a, 0.4); b = o.up(dk, u(1.05, 1.2))
    o.groove(inset(dk, 0.35), b, 0.16, 0.12)
    o.cut(U([ann(*ff(x, sy * (W / 2 - 0.42)), 0.24, 0.1, 8) for x in (-L * 0.3, 0.0, L * 0.3) for sy in (-1, 1)]), b, 0.12)
    tb = o.up(rect(*ff(-L / 2 + 0.05, 0), 0.6, 1.0, a), u(1.9, 2.2)); o.on(rect(*ff(-L / 2 + 0.05, 0), 0.4, 0.75, a), tb, 0.2)
    c0 = rng.random()
    if c0 < 0.6:
        n = rng.randint(1, 2)
        for i in range(n):
            cl = u(1.7, 2.1) if n == 2 else u(2.6, 3.2); cw = u(2.0, 2.4); c = ff(0.25 + (i - (n - 1) / 2) * 2.25 + u(-0.1, 0.1), u(-0.2, 0.2)); t = a + u(-0.12, 0.12); f2 = frame(c[0], c[1], t)
            cg = rect(c[0], c[1], cl, cw, t); cs = o.on(cg, b, u(2.0, 2.4)); ld = o.lid(cg, cs, 0.14, 0.16)
            o.corners(f2, cl - 0.32, cw - 0.32, ld, t, 0.45, 0.1)
            o.cut(U([barl(f2(x, -0.18), f2(x, 0.18), 0.12) for x in np.arange(-cl * 0.25, cl * 0.25, 0.26)]), ld, 0.1)
            o.on(rect(c[0], c[1], cl - 0.7, 0.42, t + 1.5708 * 0), ld, 0.1)
            o.handle(f2(-cl * 0.2, cw / 2 - 0.45), f2(cl * 0.2, cw / 2 - 0.45), ld, 0.3, 0.28)
    elif c0 < 0.8:
        c = ff(0.2, u(-0.2, 0.2)); r = u(1.05, 1.25); dr = o.on(disc(c[0], c[1], r, 22), b, u(2.4, 2.8)); o.on(ann(c[0], c[1], r - 0.13, r - 0.45, 22), dr, 0.12)
        o.cut(disc(*pol(c[0], c[1], r * 0.5, a + u(0, 6.28)), 0.22, 8), dr, 0.12); o.studs([pol(c[0], c[1], r * 0.45, a + 2.0)], dr, 0.2, 0.15)
    else:
        o.cut(U([line([ff(x - 0.35, -W * 0.28), ff(x, 0), ff(x - 0.35, W * 0.28)], 0.16) for x in np.arange(-L / 2 + 1.3, L / 2 - 0.9, 0.5)]), b, 0.12)
    return o.out()

@d("corridor", "made", "metal", "welding_set")
def _(cx, cy, a, rng):
    """a gas welding set on its trolley, tipped over on its back: the two bottles side by side with their shoulders,
    neck rings and valves, the regulators with their two gauges, the chain across them, the trolley's rails and its
    two wheels on edge at the foot; the twin hose snakes out to the torch with its valves and its nozzle"""
    u = rng.uniform; ff = frame(cx, cy, a); L = u(3.6, 4.4); r = 0.62; o = Obj("metal")
    o.up(rect(*ff(-L / 2 - 0.05, 0), 0.35, 4 * r + 0.6, a), 0.7)
    ps = []
    for sv, f in ((-1, 1.0), (1, 0.92)):
        Lb = L * f; xm = -L / 2 + 0.1 + Lb / 2
        def rf(t, Lb=Lb):
            x = t + Lb / 2
            if x < 0.08: return r * 0.95
            if x < Lb - 0.75: return r + (0.05 if abs(x - Lb * 0.55) < 0.12 else 0.0)
            if x < Lb - 0.45: return r * max(0.52, math.cos((x - (Lb - 0.75)) / 0.3 * 1.15))
            return 0.33 if x < Lb - 0.2 else 0.26
        ps += spin(*ff(xm, sv * (r + 0.02)), a, Lb, rf, 1.25)
        rg = ff(-L / 2 + 0.1 + Lb + 0.25, sv * r); ps += cyl(rg[0], rg[1], 0.34, 1.0, n=10)
        ps += [s for k in (-1, 1) for s in cyl(*pol(rg[0], rg[1], 0.36, a + 1.5708 * k + 0.6), 0.24, 0.8, n=8)]
    ps += lie(*ff(-L * 0.05, 0), a + 1.5708, 0.3, 2 * r + 0.15)
    o.st(ps, foot=rect(*ff(0.2, 0), L + 0.8, 4 * r + 0.3, a))
    o.st([s for sv in (-1, 1) for s in lie(*ff(-L / 2 + 0.35, sv * (2 * r + 0.5)), a + 1.5708, 0.36, 0.62)], "tar", foot=rect(*ff(-L / 2 + 0.35, 0), 1.3, 4 * r + 1.4, a))
    e = ff(L / 2 + u(1.5, 2.5), u(-2.1, 2.1)); h = wiggle(ff(L / 2 + 0.5, 0), e, rng, 0.6, 4)
    hb = [(x + 0.18 * math.cos(a + 1.5708), y + 0.18 * math.sin(a + 1.5708)) for x, y in h]
    o.st(rod(h, 0.22, 1.0) + rod(hb, 0.22, 1.0), "cable", foot=LineString(h).buffer(0.5))
    t = math.atan2(e[1] - h[-2][1], e[0] - h[-2][0]); q = pol(e[0], e[1], 1.2, t)
    o.st(xc_bar(pol(e[0], e[1], -0.2, t), pol(e[0], e[1], 0.7, t), lambda s: 0.3 if s < 0.25 else 0.24, 0.6) + xc_bar(pol(e[0], e[1], 0.7, t), q, lambda s: 0.2 - 0.06 * s, 0.6)
         + cyl(*pol(e[0], e[1], 0.1, t + 1.5708), 0.18, 0.62, n=6), foot=disc(e[0], e[1], 0.8))
    return o.out()

@d("corridor", "made", "metal", "gas_cylinder")
def _(cx, cy, a, rng):
    """a gas cylinder of the real thickness lying on the deck: the foot ring, the long body with a band near the
    shoulder and stencilled letters along it, the domed shoulder, the neck collar, and either the slotted safety cap
    screwed over the valve or the bare valve with its hand wheel and a hose run off to nothing; dented on some"""
    u = rng.uniform; ff = frame(cx, cy, a); r = u(0.75, 0.9); o = Obj("metal"); capd = rng.random() < 0.5; L = u(5.2, 5.8) if capd else u(4.0, 4.8)
    Lb = L - (0.9 if capd else 0.6); xb = -L / 2
    def rf(s):
        x = s * Lb
        if x < 0.12: return r * 1.04
        if x < 0.2: return r * 0.97
        if x < Lb - r: return r + (0.05 if abs(x - (Lb - r - 0.45)) < 0.1 else 0.0)
        return max(0.4, r * math.sqrt(max(0.0, 1 - ((x - (Lb - r)) / r) ** 2)))
    ps = xc_bar(ff(xb, 0), ff(xb + Lb, 0), rf, 0.45); top = 1.45 * r
    if capd:
        ps += xc_bar(ff(xb + Lb - 0.05, 0), ff(xb + Lb + 0.85, 0), lambda s: 0.55 if s < 0.8 else 0.45, 0.45)
        cuts = [(ell(*ff(xb + Lb + 0.4, 0), 0.22, 0.14, a, 8), 0.4)]
    else:
        ps += xc_bar(ff(xb + Lb - 0.05, 0), ff(xb + Lb + 0.55, 0), 0.3, 0.45) + lie(*ff(xb + Lb + 0.3, 0), a + 1.5708, 0.2, 0.5)
        cuts = []
    cuts += [(U([rect(*ff(x, 0), 0.14, 0.42, a) for x in np.arange(xb + Lb * 0.3, xb + Lb * 0.62, 0.3)]), top - 0.12)]
    if rng.random() < 0.35: cuts.append((ell(*ff(xb + Lb * u(0.2, 0.5), r * 0.4), 0.5, 0.35, a, 10), top - 0.18))
    o.st(ps, cuts=cuts, foot=rect(cx, cy, L, 2 * r + 0.3, a))
    if not capd:
        h = [ff(L / 2 + 0.1, 0), ff(L / 2 + 1.0, u(-1, 1)), ff(L / 2 + u(0.4, 1.6), u(1.4, 2.2) * rng.choice((-1, 1)))]; xc_hose(o, h, 0.24)
    else:
        h = [ff(L / 2 + 0.15, 0.4), ff(L / 2 + 0.5, 0.4)]
    return o.out(extra=[LineString(h).buffer(0.4)])

@d("corridor", "made", "plate", "plate_stack")
def _(cx, cy, a, rng):
    """spare deck plates stacked a little askew, each plate's edge showing under the next: the top one carries its
    diamond tread, a bolt hole in each corner and the lifting slot; one or two steel bands strap the stack, a few
    loose bolts lie by it"""
    u = rng.uniform; L = u(3.0, 3.5); W = u(2.4, 2.8); o = Obj("plate"); n = rng.randint(4, 6); th = u(0.22, 0.26); par = None
    ds = u(0, 6.28); st = u(0.13, 0.17); x, y, t = pol(cx, cy, -st * (n - 1) / 2, ds) + (a,)
    for i in range(n):
        g = rect(x, y, L, W, t); par = o.up(g, th * (i + 1) + 0.05)
        if i < n - 1: x, y = pol(x, y, st, ds + u(-0.5, 0.5)); t = a + u(-0.04, 0.04)
    ff = frame(x, y, t)
    o.cut(U([disc(*ff(sx * (L / 2 - 0.4), sy * (W / 2 - 0.4)), 0.17, 6) for sx in (-1, 1) for sy in (-1, 1)] + [capl(ff(-0.35, W / 2 - 0.42), ff(0.35, W / 2 - 0.42), 0.24)]), par, 0.16)
    dm = [ff(i * 0.5 + (j % 2) * 0.25, j * 0.4) for i in range(-5, 6) for j in range(-4, 5)]
    o.cut(U([ell(px, py, 0.2, 0.08, t + (0.6 if (k % 2) else -0.6), 6) for k, (px, py) in enumerate(dm)]).intersection(rect(x, y, L - 1.3, W - 1.3, t)), par, 0.1)
    if rng.random() < 0.6: o.on(U([rect(*ff(sx * L * 0.3, 0), 0.4, W + 0.2, t) for sx in ((-1, 1) if rng.random() < 0.5 else (1,))]), par, 0.08)
    for i in range(rng.randint(1, 3)):
        p = frame(cx, cy, a)(u(-L * 0.35, L * 0.35), rng.choice((-1, 1)) * (W / 2 + u(0.55, 0.8))); o.up(ngon(p[0], p[1], 0.3, 6, u(0, 1)), 0.3)
    return o.out()


_OWN = set(globals())
