"""Second drawing, badlands (see sd_r.py for the rule). Nothing here hangs in the air: what would overhang in the
real thing (a rail on its posts, a cactus arm) is drawn fallen, lying or standing on its own foot."""
import math
from sd_reg import d

@d("badlands", "growth", "cactus", "saguaro")
def _(cx, cy, a, rng):
    """a saguaro: either a stand of ribbed columns with round heads (the trunk and its young arms rising beside it),
    or an old one fallen full length, ribs along it, its arms lying out to the sides, bits broken off at the root"""
    u = rng.uniform; o = Obj("cactus"); ff = frame(cx, cy, a)
    if rng.random() < 0.45:
        L = u(4.6, 6.0); r = u(0.7, 0.9)
        ps = spin(cx, cy, a, L, lambda t: r * (1.0 if t < L / 2 - r else math.sqrt(max(0.03, 1 - ((t - L / 2 + r) / r) ** 2))), 1.1)
        cuts = [(barl(ff(-L / 2 + 0.2, v), ff(L / 2 - r * 0.7, v), 0.17), r * 1.1 * f) for v, f in ((0, 0.86), (-r * 0.48, 0.7), (r * 0.48, 0.7))]
        for sv in rng.sample((-1, 1), rng.randint(1, 2)):
            p = ff(u(-L * 0.2, L * 0.2), sv * r * 0.3); e = pol(p[0], p[1], u(1.5, 2.1), a + sv * u(0.9, 1.3)); ra = r * 0.64
            ps += rod([p, e], ra, 1.05) + dome(e[0], e[1], ra, ra * 1.05)
        o.st(ps, cuts=cuts)
        for i in range(rng.randint(1, 3)): c = ff(-L / 2 - u(0.6, 1.3), u(-0.8, 0.8)); o.st(rock(c[0], c[1], u(0.32, 0.45), u(0.3, 0.5), rng))
        return o.out()
    def col(x, y, r, h, n): return ext(star(x, y, r, r * 0.8, n, a), h - r * 0.8) + fluted(x, y, r, r * 0.8, n, a, 0.8, 5, lift=h - r * 0.8)
    r = u(0.8, 1.0); h = u(2.8, 3.8); n = rng.randint(8, 11); ps = col(cx, cy, r, h, n); foot = [disc(cx, cy, r + 0.2)]; t0 = u(0, 6.28)
    for k in range(rng.randint(1, 3)):
        ra = r * u(0.55, 0.7); c = pol(cx, cy, r + ra * u(0.5, 0.8), t0 + k * u(1.6, 2.6)); ps += col(c[0], c[1], ra, h * u(0.35, 0.7), max(6, n - 3)); foot.append(disc(c[0], c[1], ra + 0.2))
    o.st(ps, foot=hull(foot))
    return o.out()

@d("badlands", "growth", "cactus", "prickly_pear")
def _(cx, cy, a, rng):
    """a prickly pear: thick oval pads heaped on one another, the newer ones smaller and higher, round fruit on their
    rims and the dimples where the spines sit"""
    u = rng.uniform; ps = []; foot = []; cuts = []; n = rng.randint(4, 7)
    for i in range(n):
        x, y = (cx, cy) if i == 0 else pol(cx, cy, u(0.8, 1.7), u(0, 6.28)); r = u(0.8, 1.05) * (1.0 if i < 3 else 0.72); h = u(0.5, 0.75) + (0.45 if i >= 3 else 0.0); t = u(0, 3.14)
        ps += dome(x, y, r, h, sq=u(0.55, 0.72), a=t); foot.append(disc(x, y, r))
        for k in range(rng.randint(0, 2)): f = pol(x, y, r * 0.55, t + u(-0.4, 0.4) + k * math.pi); ps += dome(f[0], f[1], 0.27, h + 0.22)
        if i < 3: cuts += [(disc(*pol(x, y, r * 0.3, t + 1.57 + k * 2.1), 0.13, 5), h * 0.72) for k in range(3)]
    return hull(foot), [ST(ps, "cactus", cuts=cuts)]

@d("badlands", "made", "bone", "longhorn_skull")
def _(cx, cy, a, rng):
    """a steer's skull: broad at the brow and narrowing to the muzzle, eye sockets, nostrils, and the two long horns
    sweeping out and forward (one snapped short on some)"""
    u = rng.uniform; ff = frame(cx, cy, a); L = u(2.7, 3.3)
    ps = spin(cx, cy, a, L, lambda t: 0.36 + 0.42 * max(0.0, min(1.0, (L * 0.2 - t) / (L * 0.5))), 1.0) + dome(*ff(-L * 0.3, 0), 0.82, 0.95, sq=0.85, a=a) + dome(*ff(L * 0.45, 0), 0.4, 0.5)
    cuts = [(disc(*ff(-L * 0.16, sv * 0.52), 0.24, 8), 0.42) for sv in (-1, 1)] + [(ell(*ff(L * 0.42, sv * 0.16), 0.17, 0.1, a, 6), 0.36) for sv in (-1, 1)]
    foot = [rect(cx, cy, L, 1.8, a)]
    for sv in (-1, 1):
        k = 0.5 if rng.random() < 0.2 else 1.0; sw = u(-0.3, 0.5)
        pts = [ff(-L * 0.36, sv * 0.5), ff(-L * 0.4, sv * (0.5 + 1.2 * k)), ff(-L * 0.3 + sw * 0.4, sv * (0.5 + 2.1 * k)), ff(-L * 0.1 + sw, sv * (0.5 + 2.7 * k))]
        ps += rod(pts[:3], 0.33, 1.0) + rod(pts[2:], 0.26, 1.0) + (cone(pts[3][0], pts[3][1], 0.26, 0.5, 0.2) if k == 1.0 else []); foot.append(disc(pts[3][0], pts[3][1], 0.5))
    return hull(foot), [ST(ps, "bone", cuts=cuts)]

@d("badlands", "made", "wood", "hitching_rail")
def _(cx, cy, a, rng):
    """a hitching rail come down: the two posts still stand, each with the seat cut for the rail in its head and the
    weather checks in the end grain; the rail lies between them and a rope is coiled by one post"""
    u = rng.uniform; ff = frame(cx, cy, a); L = u(5.0, 6.4); o = Obj("wood")
    for sx in (-1, 1):
        p = ff(sx * L / 2, 0); b = o.up(disc(p[0], p[1], 0.56, 10), u(2.2, 2.9))
        o.cut(barl(ff(sx * L / 2 - 0.8, 0), ff(sx * L / 2 + 0.8, 0), 0.5), b, 0.4)
        o.cut(U([barl(pol(p[0], p[1], 0.3, t), pol(p[0], p[1], 0.5, t), 0.16) for t in (a + 1.2, a + 2.2, a - 1.6)]), b, 0.14)
    sv = rng.choice((-1, 1)); c = ff(u(-0.5, 0.5), sv * u(0.9, 1.4)); t = a + u(-0.22, 0.22); f2 = frame(c[0], c[1], t); Lr = L * u(0.8, 0.95)
    o.st(lie(c[0], c[1], t, Lr, 0.36), cuts=[(barl(f2(-Lr * 0.3, 0), f2(Lr * u(0.0, 0.3), 0), 0.15), 0.36 * 1.15 * 0.8)])
    p = ff(rng.choice((-1, 1)) * L / 2 + u(-0.3, 0.3), -sv * u(1.4, 1.8)); o.st(rod(spiral(p[0], p[1], 0.3, 0.9, 2.1, u(0, 6.28), 30), 0.24, 1.0), "rope")
    return o.out([rect(cx, cy, L + 1.4, 4.4, a)])

@d("badlands", "made", "wood", "water_trough")
def _(cx, cy, a, rng):
    """a plank water trough on two cross skids: thick sides, the end boards showing their joint, water standing low
    in it, a bucket beside it with its hollow, a wet patch under one end"""
    u = rng.uniform; ff = frame(cx, cy, a); L = u(4.4, 5.4); W = u(1.7, 2.1); h = u(1.0, 1.3); o = Obj("wood"); g = rect(cx, cy, L, W, a)
    b0 = o.up(U([g] + [rect(*ff(sx * L * 0.3, 0), 0.62, W + 1.0, a) for sx in (-1, 1)]), 0.4); bx = o.on(g, b0, h - 0.4)
    o.cut(rect(cx, cy, L - 0.8, W - 0.8, a), bx, u(0.35, 0.6))
    o.cut(U([barl(ff(sx * (L / 2 - 0.4), -W), ff(sx * (L / 2 - 0.4), W), 0.17) for sx in (-1, 1)] + [barl(ff(x, sy * (W / 2 - 0.2)), ff(x + 0.01, sy * (W / 2 + 0.1)), 0.17) for x in (-L * 0.12, L * 0.2) for sy in (-1, 1)]), bx, 0.16)
    c = ff(L / 2 + u(1.1, 1.6), u(-0.8, 0.8)); bk = o.up(disc(c[0], c[1], 0.64, 12), u(0.9, 1.1), "tin"); o.cut(disc(c[0], c[1], 0.42, 10), bk, 0.5)
    o.gcut(blob(*ff(-L / 2 - 0.7, u(-0.5, 0.5)), u(0.7, 1.0), rng, 3), 0.25, "water")
    return o.out()

@d("badlands", "made", "wood", "ox_yoke")
def _(cx, cy, a, rng):
    """an ox yoke lying flat: the carved beam dipping over each neck, the two bows (one may be gone), the pin holes,
    and the iron ring in the middle"""
    u = rng.uniform; ff = frame(cx, cy, a); o = Obj("wood"); X = u(1.3, 1.5)
    pts = [ff(-2 * X, 0.35), ff(-1.5 * X, 0.1), ff(-X, -0.25), ff(-0.5 * X, 0.0), ff(0, 0.15), ff(0.5 * X, 0.0), ff(X, -0.25), ff(1.5 * X, 0.1), ff(2 * X, 0.35)]
    b = o.up(line(pts, 0.98), u(0.55, 0.7)); gone = rng.choice((-1, 1)) if rng.random() < 0.3 else 0
    o.cut(U([disc(*ff(sx * X + k * 0.62, -0.2 + abs(k) * 0.14), 0.15, 6) for sx in (-1, 1) for k in (-1, 1)]), b, 0.3)
    for sx in (-1, 1):
        if sx == gone: continue
        c = ff(sx * X, -0.3); o.st(ext(line(arc(c[0], c[1], 0.66, a + math.pi, a + 2 * math.pi, 10), 0.5), 0.38))
    c = ff(0, 0.95); o.st(ext(ann(c[0], c[1], 0.56, 0.26, 12), 0.3), "iron"); o.st(ext(rect(*ff(0, 0.5), 0.46, 0.5, a), 0.34), "iron")
    return o.out()

@d("badlands", "made", "hide", "rattlesnake")
def _(cx, cy, a, rng):
    """a rattlesnake coiled: the body in open rings, the head raised in the middle, the rattle at the tail's end"""
    u = rng.uniform; t0 = u(0, 6.28); turns = u(1.6, 2.0); sp = spiral(cx, cy, 0.75, 0.75 + turns * 1.0, turns, t0, 40)
    e = sp[-1]; d2 = math.atan2(sp[-1][1] - sp[-3][1], sp[-1][0] - sp[-3][0])
    ps = rod(sp, 0.35, 1.0) + rod([sp[0], (cx, cy)], 0.33, 1.0, lift=0.3) + dome(cx, cy, 0.46, 1.1, sq=0.72, a=t0 + 2.0)
    ps += [s for k, r in enumerate((0.24, 0.2, 0.16)) for s in dome(*pol(e[0], e[1], 0.3 + k * 0.34, d2), r, r * 1.5)]
    return disc(cx, cy, 0.75 + turns * 1.0 + 1.3), [ST(ps, "hide", cuts=[(disc(*pol(cx, cy, 0.2, t0 + 2.0 + sv * 0.9), 0.1, 5), 0.85) for sv in (-1, 1)])]

@d("badlands", "made", "leather", "saddle")
def _(cx, cy, a, rng):
    """a stock saddle set down: the wide skirts with their tooled border, the dished seat, the horn in front and the
    high cantle behind, a stirrup on its leather out to each side, the cinch ring"""
    u = rng.uniform; ff = frame(cx, cy, a); o = Obj("leather"); g = rrect(cx, cy, u(3.0, 3.4), u(2.4, 2.8), a, 0.8)
    b = o.up(g, 0.34); o.groove(inset(g, 0.3), b, 0.17, 0.14)
    o.st(spin(cx, cy, a, 2.3, lambda t: 0.72 * math.sqrt(max(0.04, 1 - (t / 1.2) ** 2)), 0.75), par=b, cuts=[(ell(*ff(-0.1, 0), 0.6, 0.4, a, 12), 0.36)])
    o.st(turn(*ff(1.05, 0), [(0.34, 0.0), (0.24, 0.5), (0.24, 0.8), (0.4, 0.9), (0.4, 1.05)], 10), par=b)
    c = ff(-0.5, 0); o.on(line(arc(c[0], c[1], 0.75, a + 1.9, a + 4.38, 10), 0.5), b, 0.8)
    for sv in (-1, 1):
        p = ff(u(-0.2, 0.3), sv * 1.0); e = pol(p[0], p[1], u(1.7, 2.3), a + sv * (1.57 + u(-0.5, 0.5)))
        o.st(ext(U([capl(p, e, 0.5), ann(e[0], e[1], 0.6, 0.3, 10)]), 0.26))
    return o.out()

@d("badlands", "made", "tin", "gold_pan")
def _(cx, cy, a, rng):
    """a prospector's pan with its rolled rim, the riffle rings pressed in its side, gravel and a few nuggets left in
    the bottom; a poke of dust tied at the neck beside it"""
    u = rng.uniform; R = u(1.5, 1.9); o = Obj("tin"); b = o.up(disc(cx, cy, R, 28), 0.22)
    o.on(ann(cx, cy, R, R - 0.46, 28), b, 0.34); o.on(ann(cx, cy, R - 0.56, R - 1.02, 24), b, 0.16)
    for i in range(rng.randint(3, 6)): p = pol(cx, cy, u(0, R - 1.3), u(0, 6.28)); o.st(rock(p[0], p[1], u(0.25, 0.32), u(0.2, 0.32), rng), "gold" if i < 2 else "rock", par=b)
    c = pol(cx, cy, R + u(1.0, 1.5), u(0, 6.28)); t = u(0, 6.28); nk = pol(c[0], c[1], 0.62, t)
    o.st(dome(c[0], c[1], 0.62, 0.7, sq=0.85, a=t) + rod([c, nk], 0.24, 1.0, lift=0.2) + dome(*pol(c[0], c[1], 0.8, t), 0.3, 0.42), "leather")
    return o.out()
