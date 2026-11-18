"""Second drawing, badlands, the ones that needed a third look (loaded after sd_r_badlands.py, so these stand).
Lesson: one ST() per height. A low round part (a snake's coil, a horn) sharing an ST() with a tall one gets only
two of its steps and comes out as a flat strip."""
import math
from sd_reg import d

@d("badlands", "growth", "cactus", "prickly_pear")
def _(cx, cy, a, rng):
    """a prickly pear: thick oval pads leaning on one another, each one rounded on its own, the newer ones smaller,
    round fruit along their rims and the dimples where the spines sit"""
    u = rng.uniform; o = Obj("cactus"); foot = []; n = rng.randint(4, 7)
    for i in range(n):
        x, y = (cx, cy) if i == 0 else pol(cx, cy, u(0.9, 1.9), u(0, 6.28)); r = u(0.85, 1.1) * (1.0 if i < 3 else 0.75); h = u(0.55, 0.8) + (0.4 if i >= 3 else 0.0); t = u(0, 3.14); q = u(0.55, 0.7)
        o.st(dome(x, y, r, h, sq=q, a=t), cuts=[(disc(*pol(x, y, r * 0.32, t + k * 3.14), 0.14, 5), h * 0.8) for k in range(2)], foot=disc(x, y, r)); foot.append(disc(x, y, r))
        for k in range(rng.randint(0, 2)): f = pol(x, y, r * 0.8, t + u(-0.5, 0.5) + k * math.pi); o.st(dome(f[0], f[1], 0.3, 0.5), foot=disc(f[0], f[1], 0.3))
    return hull(foot), o.L

@d("badlands", "made", "bone", "longhorn_skull")
def _(cx, cy, a, rng):
    """a steer's skull: broad at the brow and narrowing to the muzzle, eye sockets, nostrils, and the two long horns
    sweeping out and forward, thick at the root and thinning to a point (one snapped short on some)"""
    u = rng.uniform; ff = frame(cx, cy, a); L = u(3.0, 3.6); o = Obj("bone")
    g = Polygon([ff(-L / 2, -0.85), ff(-L * 0.1, -0.6), ff(L / 2, -0.36), ff(L / 2, 0.36), ff(-L * 0.1, 0.6), ff(-L / 2, 0.85)]).buffer(-0.2).buffer(0.2, 6)
    b = o.st(ext(g, 0.5) + ext(inset(g, 0.2), 0.75) + dome(*ff(-L * 0.28, 0), 0.6, 0.95, sq=0.9, a=a),
             cuts=[(disc(*ff(-L * 0.12, sv * 0.42), 0.22, 8), 0.3) for sv in (-1, 1)] + [(ell(*ff(L * 0.4, sv * 0.15), 0.18, 0.1, a, 6), 0.4) for sv in (-1, 1)] + [(barl(ff(-L * 0.05, 0), ff(L * 0.3, 0), 0.14), 0.62)])
    for sv in (-1, 1):
        k = 0.5 if rng.random() < 0.2 else 1.0; sw = u(-0.3, 0.6)
        pts = [ff(-L * 0.4, sv * 0.6), ff(-L * 0.45, sv * (0.6 + 1.1 * k)), ff(-L * 0.36 + sw * 0.4, sv * (0.6 + 2.0 * k)), ff(-L * 0.16 + sw, sv * (0.6 + 2.7 * k))]
        o.st(rod(pts[:2], 0.4, 1.0) + rod(pts[1:3], 0.33, 1.0) + rod(pts[2:], 0.26, 1.0) + (cone(pts[3][0], pts[3][1], 0.26, 0.26, 0.2) if k == 1.0 else []), foot=LineString(pts).buffer(0.5))
    return o.out()

@d("badlands", "made", "wood", "ox_yoke")
def _(cx, cy, a, rng):
    """an ox yoke lying flat: the carved beam dipping over each neck, the two bows as open loops (one may be gone),
    the pin holes, and the iron ring on its staple in the middle"""
    u = rng.uniform; ff = frame(cx, cy, a); o = Obj("wood"); X = u(1.4, 1.6)
    pts = [ff(-2 * X, 0.35), ff(-1.5 * X, 0.1), ff(-X, -0.25), ff(-0.5 * X, 0.0), ff(0, 0.15), ff(0.5 * X, 0.0), ff(X, -0.25), ff(1.5 * X, 0.1), ff(2 * X, 0.35)]
    b = o.up(line(pts, 1.0), u(0.6, 0.75)); gone = rng.choice((-1, 1)) if rng.random() < 0.3 else 0
    o.cut(U([disc(*pts[i], 0.16, 6) for i in (1, 3, 5, 7)]), b, 0.3)
    for sx in (-1, 1):
        if sx == gone: continue
        c = ff(sx * X, -0.5); o.st(ext(line(arc(c[0], c[1], 0.8, a + math.pi, a + 2 * math.pi, 12), 0.48), 0.4), foot=disc(c[0], c[1], 1.1))
    c = ff(0, 1.15); o.st(ext(ann(c[0], c[1], 0.6, 0.3, 12), 0.3), "iron", foot=disc(c[0], c[1], 0.6))
    return o.out()

@d("badlands", "made", "hide", "rattlesnake")
def _(cx, cy, a, rng):
    """a rattlesnake coiled: the thick body in open rings, the flat wedge of a head resting on the inner coil with
    its two eye pits, the rattle's beads at the tail's end"""
    u = rng.uniform; t0 = u(0, 6.28); turns = u(1.5, 1.9); r0 = 1.0; sp = spiral(cx, cy, r0, r0 + turns * 1.15, turns, t0, 40); o = Obj("hide")
    o.st(rod(sp, 0.42, 1.0), foot=disc(cx, cy, r0 + turns * 1.15 + 0.5))
    hx, hy = pol(cx, cy, 0.15, t0 + 2.6); th = t0 + 2.6 + math.pi; f2 = frame(hx, hy, th)
    o.st(rod([sp[0], sp[1], (hx, hy)], 0.36, 1.1) + spin(hx, hy, th, 1.2, lambda t: 0.2 + 0.3 * max(0.0, 1 - abs(t + 0.15) / 0.75), 1.3, lift=0.15),
         cuts=[(disc(*f2(-0.2, sv * 0.22), 0.1, 5), 0.5) for sv in (-1, 1)], foot=disc(hx, hy, 0.8))
    e = sp[-1]; d2 = math.atan2(sp[-1][1] - sp[-3][1], sp[-1][0] - sp[-3][0])
    o.st([s for k, r in enumerate((0.3, 0.26, 0.22)) for s in dome(*pol(e[0], e[1], 0.45 + k * 0.4, d2), r, r * 1.3)], "bone", foot=disc(*pol(e[0], e[1], 0.8, d2), 0.8))
    return o.out()

@d("badlands", "made", "leather", "saddle")
def _(cx, cy, a, rng):
    """a stock saddle set down: the wide skirts with their tooled border, the dished seat, the horn in front and the
    cantle rising behind it, a stirrup on its leather out to each side"""
    u = rng.uniform; ff = frame(cx, cy, a); o = Obj("leather"); g = rrect(cx, cy, u(3.2, 3.6), u(2.5, 2.9), a, 0.9)
    b = o.up(g, 0.34); o.groove(inset(g, 0.3), b, 0.18, 0.15)
    se = ell(cx, cy, 1.25, 0.8, a, 20); s1 = o.on(se, b, 0.36); o.cut(ell(*ff(-0.1, 0), 0.7, 0.45, a, 14), s1, 0.2)
    c = ff(-0.25, 0); o.on(line(arc(c[0], c[1], 0.8, a + 2.2, a + 4.08, 10), 0.5).intersection(se), s1, 0.4)
    o.st(turn(*ff(0.95, 0), [(0.32, 0.0), (0.25, 0.3), (0.25, 0.5), (0.4, 0.6), (0.4, 0.75)], 10), par=s1)
    for sv in (-1, 1):
        p = ff(u(-0.2, 0.3), sv * 1.1); e = pol(p[0], p[1], u(1.8, 2.4), a + sv * (1.57 + u(-0.5, 0.5)))
        o.st(ext(U([capl(p, e, 0.5), ann(e[0], e[1], 0.62, 0.3, 10)]), 0.26), foot=LineString([p, e]).buffer(0.7))
    return o.out()

@d("badlands", "made", "tin", "gold_pan")
def _(cx, cy, a, rng):
    """a prospector's pan with its rolled rim and the riffle ring pressed in its side, gravel and a nugget or two left
    in the bottom; a poke of dust tied at the neck beside it"""
    u = rng.uniform; R = u(1.6, 2.0); o = Obj("tin"); b = o.up(disc(cx, cy, R, 28), 0.3)
    o.on(ann(cx, cy, R, R - 0.46, 28), b, 0.34); o.on(ann(cx, cy, R - 0.58, R - 1.04, 24), b, 0.16)
    for i in range(rng.randint(3, 6)): p = pol(cx, cy, u(0, R - 1.4), u(0, 6.28)); o.st(rock(p[0], p[1], u(0.25, 0.32), u(0.2, 0.32), rng), "gold" if i < 2 else "rock", par=b)
    c = pol(cx, cy, R + u(1.0, 1.5), u(0, 6.28)); t = u(0, 6.28)
    o.st(dome(c[0], c[1], 0.62, 0.7, sq=0.85, a=t) + rod([c, pol(c[0], c[1], 0.62, t)], 0.24, 1.0, lift=0.2) + dome(*pol(c[0], c[1], 0.8, t), 0.3, 0.42), "leather", foot=disc(c[0], c[1], 1.1))
    return o.out()
