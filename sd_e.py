"""Style-only details, part E: forest, dungeon, temple, coast (see style_details.py)."""
import math
from sd_reg import d

# ====================================================================================================== forest
@d("forest", "growth", "fern", "fern_fronds")
def _(cx, cy, a, rng):
    u = rng.uniform; ps = dome(cx, cy, 0.6, 0.6, n=8); foot = []; n = rng.randint(4, 7)
    for k in range(n):
        t = a + 6.2832 * k / n + u(-0.3, 0.3); L = u(2.4, 3.6); c = pol(cx, cy, L / 2 + 0.2, t)
        ps += spin(c[0], c[1], t, L, lambda q, L=L: (0.62 * (1 - (q + L / 2) / L) + 0.1) * (0.72 + 0.28 * (math.sin(q * 8) > 0)), 0.7, prof="ridge"); foot.append(disc(*pol(cx, cy, L + 0.2, t), 0.3))
    return hull(foot), [ST(ps, "fern")]

@d("forest", "growth", "twig", "pine_cones")
def _(cx, cy, a, rng):
    u = rng.uniform; ps = []; foot = []
    for i in range(rng.randint(3, 6)):
        x, y = pol(cx, cy, u(0, 2.2), u(0, 6.28)); b = u(0, 6.28); L = u(1.5, 2.2)
        ps += spin(x, y, b, L, lambda q, L=L: 0.5 * math.sin(max(0.03, min(0.97, (q + L / 2) / L)) ** 0.7 * math.pi) * (0.8 + 0.2 * (math.sin(q * 14) > 0)) + 0.06, 1.1); foot.append(disc(x, y, L / 2 + 0.1))
    return hull(foot).buffer(0.2), [ST(ps, "twig")]

@d("forest", "growth", "leaf", "acorns_and_leaves")
def _(cx, cy, a, rng):
    u = rng.uniform; ps = []; foot = []
    for i in range(rng.randint(2, 4)):
        x, y = pol(cx, cy, u(0.5, 2.4), a + i * 1.9); b = u(0, 6.28); f2 = frame(x, y, b)
        leaf = U([ell(*f2(q, 0), 0.5, w, b, 10) for q, w in ((-0.9, 0.55), (-0.3, 0.75), (0.35, 0.7), (0.95, 0.45))]); ps += ext(leaf, 0.28) + rod([f2(-1.6, 0), f2(1.2, 0)], 0.2, 1.6); foot.append(leaf)
    for i in range(rng.randint(2, 5)):
        x, y = pol(cx, cy, u(0, 2.0), u(0, 6.28))
        if all(Point(x, y).distance(g) > 0.6 for g in foot): ps += dome(x, y, 0.38, 0.75, n=8) + cyl(x, y, 0.44, 0.3, n=8); foot.append(disc(x, y, 0.5))
    return hull(foot).buffer(0.2), [ST(ps, "leaf")]

@d("forest", "growth", "stump", "snag")
def _(cx, cy, a, rng):
    u = rng.uniform; r = u(1.0, 1.4); h = u(2.6, 3.8)
    ps = turn(cx, cy, [(r * 1.4, 0.0), (r * 1.05, h * 0.2), (r, h * 0.5), (r * 0.9, h)], 11, u(0.85, 1.0), a)
    cuts = [(tri(pol(cx, cy, r * 1.2, a + t), pol(cx, cy, r * 0.2, a + t + 3.14), r * u(0.9, 1.4)), h * u(0.5, 0.85)) for t in (u(0, 6.28),) * 1] + [(disc(cx, cy, r * 0.42, 8), h * u(0.3, 0.7))] + [(disc(*pol(cx, cy, r * 0.75, u(0, 6.28)), 0.24, 6), h * 0.3) for i in range(rng.randint(1, 3))]
    return disc(cx, cy, r * 1.5), [ST(ps, "stump", cuts=cuts)]

@d("forest", "growth", "wax", "wild_hive")
def _(cx, cy, a, rng):
    u = rng.uniform; ff = frame(cx, cy, a); L = u(3.0, 3.8); r = u(1.1, 1.4); ph = u(0, 6)
    ps = spin(cx, cy, a, L, lambda t: r * math.sin(max(0.04, min(0.96, (t + L / 2) / L)) ** 0.75 * math.pi) * (1 + 0.07 * math.sin(t * 7 + ph)) + 0.06, 1.0)
    cuts = [(ngon(*ff(L * 0.05 + i * 0.5 + 0.25 * (j % 2), j * 0.44), 0.24, 6, a), r * 0.55) for i in range(-1, 2) for j in range(-1, 2)]
    return rect(cx, cy, L + 0.2, 2 * r + 0.3, a), [ST(ps, "wax", cuts=cuts)]

@d("forest", "marks", "pit", "fox_den", ground=True)
def _(cx, cy, a, rng):
    u = rng.uniform; ff = frame(cx, cy, a); A = u(1.3, 1.7); B = A * 0.7; m = blob(*ff(A + 1.6, u(-0.5, 0.5)), u(1.2, 1.6), rng, 4)
    roots = [s for sv in (-1, 1) for s in rod([ff(-A - 0.9, sv * 0.4), ff(-A * 0.3, sv * (B + 0.55)), ff(A * 0.8, sv * (B + 0.75))], lambda q: 0.5 - 0.25 * q, 1.3)]
    return hull([ell(cx, cy, A + 1.0, B + 1.2, a), m]), [("gcut", ell(cx, cy, A, B, a, 14), u(0.6, 0.85), "pit"), ST(roots, "root"), ("mound", m, u(0.5, 0.8), "earth", SOFT_T)]

@d("forest", "growth", "shroom", "puffballs")
def _(cx, cy, a, rng):
    u = rng.uniform; ps = []; cuts = []; foot = []
    for i in range(rng.randint(3, 6)):
        x, y = pol(cx, cy, 0 if i == 0 else u(1.5, 2.3), a + i * 1.25); r = u(0.55, 1.0)
        if all(disc(x, y, r).distance(g) > 0.15 for g in foot):
            ps += turn(x, y, [(r * 0.7, 0.0), (r, r * 0.6)] + crv(r, 0.25, r * 0.6, r * 1.5), 12); foot.append(disc(x, y, r))
            if rng.random() < 0.3: cuts.append((star(x, y, r * 0.5, r * 0.2, 5, u(0, 1)), r * 0.8))
    return hull(foot).buffer(0.2), [ST(ps, "shroom", cuts=cuts)]

@d("forest", "debris", "rock", "moss_stones")
def _(cx, cy, a, rng):
    u = rng.uniform; ps = []; foot = []
    for i in range(rng.randint(2, 4)):
        x, y = pol(cx, cy, 0 if i == 0 else u(1.8, 2.5), a + i * 2.0); r = u(0.9, 1.4); h = r * u(0.7, 1.0)
        ps += rock(x, y, r, h, rng) + dome(x + u(-0.2, 0.2), y + u(-0.2, 0.2), r * 0.7, 0.4, lift=h * 0.8, n=9); foot.append(disc(x, y, r))
    return hull(foot).buffer(0.2), [ST(ps, "rock")]

@d("forest", "growth", "twig", "brambles")
def _(cx, cy, a, rng):
    u = rng.uniform; ps = []; foot = []
    for i in range(rng.randint(2, 4)):
        t = a + i * 1.1 + u(-0.3, 0.3); R = u(2.0, 3.2); c0 = pol(cx, cy, 0.3 * i, t + 1.57); pts = arc(c0[0], c0[1], R, t, t + u(1.6, 2.6), 12)
        ps += rod(pts, 0.28, 1.4); foot.append(LineString(pts).buffer(0.6))
        for p in pts[1:-1:2]: q = pol(p[0], p[1], 0.36, u(0, 6.28)); ps += cone(q[0], q[1], 0.25, u(0.5, 0.8), 0.2, n=5)
    return hull(foot), [ST(ps, "twig")]

@d("forest", "made", "log", "hollow_log")
def _(cx, cy, a, rng):
    u = rng.uniform; ff = frame(cx, cy, a); L = u(5.0, 7.0); r = u(1.1, 1.4)
    ps = spin(cx, cy, a, L, lambda t: r * (1 - 0.1 * t / L) + 0.1 * math.exp(-((t - L * 0.2) / 0.4) ** 2), 1.1)
    cuts = [(rect(*ff(-L / 2 + 0.55, 0), 1.5, r * 1.1, a), r * 0.35), (ell(*ff(u(0, L * 0.3), 0), u(0.6, 1.0), r * 0.45, a, 10), r * 0.4)]
    return rect(cx, cy, L + 0.2, 2 * r + 0.3, a), [ST(ps, "log", cuts=cuts)]

@d("forest", "made", "rope", "snare")
def _(cx, cy, a, rng):
    u = rng.uniform; R = u(1.1, 1.5); e = pol(cx, cy, R + u(2.5, 4.0), a); pg = pol(cx, cy, R + 1.2, a + 3.14)
    ps = rod([pol(cx, cy, R, t) for t in np.linspace(a, a + 6.2832, 24)], 0.24, 1.0) + rod(wiggle(pol(cx, cy, R, a), e, rng, 0.4, 4), 0.24, 1.0) + cone(e[0], e[1], 0.42, u(1.4, 2.0), 0.24, n=6) + cone(pg[0], pg[1], 0.36, 1.0, 0.24, n=6)
    return hull([disc(cx, cy, R + 0.3), disc(e[0], e[1], 0.5), disc(pg[0], pg[1], 0.5)]), [ST(ps, "rope")]

@d("forest", "made", "wood", "sap_bucket")
def _(cx, cy, a, rng):
    u = rng.uniform; r = u(1.1, 1.4); h = u(1.8, 2.4); c = pol(cx, cy, r + 0.9, a); rb = u(0.7, 0.85)
    ps = turn(cx, cy, [(r * 1.35, 0.0), (r, h * 0.3), (r, h)], 12) + rod([pol(cx, cy, r * 0.6, a), pol(cx, cy, r + 0.6, a)], 0.24, 1.0, lift=h * 0.55) + turn(c[0], c[1], [(rb * 0.8, 0.0), (rb, h * 0.5)], 12)
    return hull([disc(cx, cy, r * 1.4), disc(c[0], c[1], rb + 0.2)]), [ST(ps, "wood", cuts=[(disc(c[0], c[1], rb - 0.3, 8), h * 0.5 - 0.5), (ann(cx, cy, r * 0.7, r * 0.7 - 0.18, 10), h - 0.14)])]

@d("forest", "debris", "hide", "feathers_pellets")
def _(cx, cy, a, rng):
    u = rng.uniform; ps = []; foot = []
    for i in range(rng.randint(2, 4)):
        x, y = pol(cx, cy, u(0.4, 2.2), a + i * 1.7); b = u(0, 6.28); L = u(2.2, 3.2); f2 = frame(x, y, b)
        ps += spin(x, y, b, L, lambda q, L=L: 0.5 * math.sin(max(0.05, min(0.95, (q + L / 2) / L)) * math.pi) ** 0.6 + 0.05, 0.6, prof="ridge") + rod([f2(-L / 2 - 0.7, 0), f2(-L / 2 + 0.3, 0)], 0.2, 1.0); foot.append(disc(x, y, L / 2 + 0.5))
    for i in range(rng.randint(1, 3)): x, y = pol(cx, cy, u(2.6, 3.2), a + 0.9 + i * 2.0); ps += dome(x, y, 0.45, 0.5, sq=0.65, a=u(0, 3), n=8); foot.append(disc(x, y, 0.5))
    return hull(foot), [ST(ps, "hide")]

@d("forest", "marks", "water", "boar_wallow", ground=True)
def _(cx, cy, a, rng):
    u = rng.uniform; ff = frame(cx, cy, a); p = blob(cx, cy, u(1.9, 2.6), rng, 5); gs = []
    for i in range(rng.randint(3, 5)): f2 = frame(*ff(3.4 + i * 1.3, 0.5 * (1 if i % 2 else -1)), a); gs += [ell(*f2(0, sv * 0.2), 0.36, 0.17, a, 6) for sv in (-1, 1)]
    return hull([p] + gs).buffer(0.3), [("gcut", p, u(0.3, 0.45), "water"), ("gcut", U(gs), 0.3, "pit")]

@d("forest", "growth", "leaf", "leaf_litter")
def _(cx, cy, a, rng):
    u = rng.uniform; ps = []; foot = []
    for i in range(rng.randint(7, 13)):
        x, y = pol(cx, cy, u(0, 2.6), u(0, 6.28)); b = u(0, 3.14); g = U([ell(x, y, u(0.6, 0.85), u(0.3, 0.42), b, 8), tri(pol(x, y, 0.5, b), pol(x, y, 1.15, b), 0.42)]); ps += ext(g, u(0.2, 0.45)); foot.append(g)
    return hull(foot).buffer(0.2), [ST(ps, "leaf")]

@d("forest", "growth", "shroom", "amanita")
def _(cx, cy, a, rng):
    u = rng.uniform; ps = []; foot = []
    for i in range(rng.randint(1, 3)):
        x, y = pol(cx, cy, 0 if i == 0 else u(1.8, 2.6), a + i * 2.6); R = u(1.0, 1.5) * (1 if i == 0 else 0.7); h = u(1.6, 2.6) * (1 if i == 0 else 0.7)
        ps += turn(x, y, [(R, 0.0), (R, h * 0.62)] + crv(R, 0.28, h * 0.62, h), 16) + [s for k in range(rng.randint(4, 7)) for s in dome(*pol(x, y, u(0.2, R * 0.72), u(0, 6.28)), 0.26, 0.3, lift=h * 0.82, n=6)]; foot.append(disc(x, y, R))
    return hull(foot).buffer(0.2), [ST(ps, "shroom")]

@d("forest", "made", "rock", "waymarker")
def _(cx, cy, a, rng):
    u = rng.uniform; ff = frame(cx, cy, a); L = u(3.4, 4.4); W = u(1.9, 2.4); h = u(0.7, 0.95)
    g = U([rect(*ff(-W / 4, 0), L - W / 2, W, a), disc(*ff(L / 2 - W / 2, 0), W / 2, 16)])
    arrow = U([barl(ff(-L * 0.3, 0), ff(L * 0.15, 0), 0.26), line([ff(L * 0.0, -0.45), ff(L * 0.2, 0), ff(L * 0.0, 0.45)], 0.26)])
    cuts = [(arrow, h - 0.2)] + [(ngon(*ff(-L * 0.36, sv * W * 0.26), 0.3, 3, a + sv).exterior.buffer(0.1), h - 0.2) for sv in (-1, 1) if rng.random() < 0.6]
    return g.buffer(0.2), [ST(ext(g, h) + dome(*ff(u(-1, 0.5), u(-0.5, 0.5) * 0 + W * 0.3 * rng.choice((-1, 1))), 0.5, 0.3, lift=h * 0.9, n=8), "rock", cuts=cuts)]

@d("forest", "debris", "twig", "fallen_branch")
def _(cx, cy, a, rng):
    u = rng.uniform; ff = frame(cx, cy, a); L = u(6.0, 8.5); pts = [ff(-L / 2, 0), ff(-L * 0.1, u(-0.5, 0.5)), ff(L * 0.25, u(-0.6, 0.6)), ff(L / 2, u(-0.8, 0.8))]; ln = LineString(pts)
    ps = rod(pts, lambda q: 0.5 - 0.26 * q, 1.2); foot = [ln.buffer(0.6)]
    for q in (0.35, 0.55, 0.75, 0.9)[:rng.randint(2, 4)]:
        p = ln.interpolate(q, normalized=True); sv = rng.choice((-1, 1)); e = pol(p.x, p.y, u(1.2, 2.2), a + sv * u(0.5, 1.0)); ps += rod([(p.x, p.y), e], lambda t: 0.3 - 0.08 * t, 1.1) + ext(ell(e[0], e[1], 0.6, 0.36, u(0, 3), 8), 0.26); foot.append(disc(e[0], e[1], 0.6))
    return hull(foot), [ST(ps, "twig")]

@d("forest", "made", "twig", "squirrel_cache", ground=True)
def _(cx, cy, a, rng):
    u = rng.uniform; R = u(1.2, 1.6); ps = []
    for i in range(rng.randint(4, 8)): p = pol(cx, cy, R + u(0.5, 1.3), a + u(-1.3, 1.3)); ps += dome(p[0], p[1], u(0.3, 0.42), u(0.5, 0.7), n=7)
    return disc(cx, cy, R + 1.5), [("gcut", disc(cx, cy, R, 12), u(0.5, 0.7), "pit"), ST(ps, "twig")]

@d("forest", "marks", "pit", "wolf_tracks")
def _(cx, cy, a, rng):
    u = rng.uniform; ff = frame(cx, cy, a); n = rng.randint(3, 6); gs = []
    for i in range(n):
        f2 = frame(*ff((i - (n - 1) / 2) * 2.0, 0.6 * (1 if i % 2 else -1)), a + u(-0.15, 0.15)); gs.append(ell(*f2(-0.2, 0), 0.42, 0.36, a, 8)); gs += [ell(*f2(0.42, v), 0.22, 0.16, a, 6) for v in (-0.42, -0.14, 0.14, 0.42)]
    return rect(cx, cy, n * 2.0 + 1.0, 2.8, a), [("gcut", U(gs), 0.3, "pit")]

# ====================================================================================================== dungeon
@d("dungeon", "made", "wood", "stocks")
def _(cx, cy, a, rng):
    u = rng.uniform; ff = frame(cx, cy, a); L = u(5.0, 6.0); W = u(1.9, 2.3); op = u(0.0, 0.9) if rng.random() < 0.5 else 0.0
    holes = U([disc(*ff(0, 0), 0.62, 12)] + [disc(*ff(sv * L * 0.28, 0), 0.4, 10) for sv in (-1, 1)])
    top = rect(*ff(0, W / 4 + 0.02 + op), L, W / 2 - 0.04, a); bot = rect(*ff(0, -W / 4 - 0.02), L, W / 2 - 0.04, a)
    ps = ext(top.difference(holes.buffer(0.01)) if op == 0 else top.difference(affinity.translate(holes, *[q - w for q, w in zip(ff(0, op), ff(0, 0))])), 0.6) + ext(bot.difference(holes), 0.6) + blk(*ff(-L / 2 - 0.2, op / 2), 0.5, W + op, a, 0.75) + blk(*ff(L / 2 + 0.2, -W / 4), 0.5, W / 2, a, 0.75)
    return rect(*ff(0, op / 2), L + 1.0, W + op + 0.2, a), [ST(ps, "wood")]

@d("dungeon", "made", "iron", "bear_trap")
def _(cx, cy, a, rng):
    u = rng.uniform; ff = frame(cx, cy, a); R = u(1.5, 1.9); e = pol(cx, cy, R + u(2.5, 4.0), a + 3.14 + u(-0.5, 0.5)); ps = ext(disc(cx, cy, R * 0.42, 10), 0.3) + blk(cx, cy, 2 * R + 1.6, 0.6, a, 0.36)
    for sv in (-1, 1):
        pts = arc(cx, cy, R, a + (0.12 if sv > 0 else 3.26), a + (3.02 if sv > 0 else 6.16), 12); ps += rod(pts, 0.25, 1.1)
        for p in pts[1:-1:2]: q = pol(p[0], p[1], 0.3, math.atan2(cy - p[1], cx - p[0])); ps += cone(q[0], q[1], 0.25, 0.65, 0.2, n=5)
    ps += rod(wiggle(ff(-R - 0.8, 0), e, rng, 0.5, 4), lambda q: 0.24 + 0.08 * (math.sin(q * 40) > 0), 1.0)
    return hull([disc(cx, cy, R + 0.9), disc(e[0], e[1], 0.4)]), [ST(ps, "iron")]

@d("dungeon", "made", "wood", "torch")
def _(cx, cy, a, rng):
    u = rng.uniform; ff = frame(cx, cy, a); L = u(4.4, 5.6); p = blob(*ff(L / 2 + 0.6, u(-0.5, 0.5)), u(0.8, 1.2), rng, 3)
    ps = spin(cx, cy, a, L, lambda t: 0.26 + 0.1 * (t + L / 2) / L + (0.3 * math.sin(max(0.0, (t - L * 0.22) / (L * 0.28)) * 3.0) ** 2 if t > L * 0.22 else 0.0), 1.1) + ext(ann(*ff(-L * 0.1, 0.55), 0.55, 0.26, 8), 0.4)
    return hull([rect(cx, cy, L, 1.6, a), p]), [("gcut", p, 0.2, "scorch"), ST(ps, "wood")]

@d("dungeon", "made", "glass", "potions")
def _(cx, cy, a, rng):
    u = rng.uniform; ff = frame(cx, cy, a); ps = []; n = rng.randint(2, 4); lay = []
    for i in range(n):
        c = ff((i - (n - 1) / 2) * 1.7, u(-0.3, 0.3)); k = rng.randint(0, 2); r = u(0.55, 0.72)
        if rng.random() < 0.25: b = u(0, 6.28); ps += spin(c[0], c[1], b, 1.9, lambda t, r=r: r if t < 0.2 else 0.28, 1.1); p = blob(*pol(c[0], c[1], 1.6, b), 0.7, rng, 3); lay.append(("gcut", p, 0.2, "acid"))
        else: ps += (turn(c[0], c[1], [(r * 0.7, 0.0)] + crv(r, 0.26, r * 0.5, r * 1.5) + [(0.26, r * 2.0), (0.34, r * 2.1)], 12) if k == 0 else turn(c[0], c[1], [(r, 0.0), (0.26, r * 1.9), (0.26, r * 2.4)], 12) if k == 1 else turn(c[0], c[1], [(0.36, 0.0), (0.36, r * 2.6), (0.42, r * 2.7)], 8))
    return rect(cx, cy, n * 1.7 + 2.6, 4.6, a), [ST(ps, "glass")] + [L_ for L_ in lay if all(L_[1].distance(disc(*ff((i - (n - 1) / 2) * 1.7, 0), 0.9)) > 0.3 for i in range(n))][:1]

@d("dungeon", "made", "bone", "skull_candle")
def _(cx, cy, a, rng):
    u = rng.uniform; ff = frame(cx, cy, a); r = u(0.95, 1.2); h = r * 1.2
    ps = dome(cx, cy, r, h, sq=0.86, a=a) + blk(*ff(r * 0.8, 0), 0.6, r * 1.0, a, h * 0.45) + turn(*ff(-r * 0.15, 0), [(0.5, 0.0), (0.5, h * 0.9), (0.36, h + 0.1), (0.36, h + u(0.4, 1.1)), (0.2, h + 1.2)], 8) + [s for i in range(rng.randint(2, 4)) for s in rod([ff(-r * 0.15, 0), pol(*ff(-r * 0.15, 0), r * u(0.6, 0.95), u(0, 6.28))], 0.22, 1.0, lift=h * 0.55)]
    cuts = [(disc(*ff(r * 0.5, sv * r * 0.38), 0.25, 6), h * 0.45) for sv in (-1, 1)]
    return disc(cx, cy, r + 0.5), [ST(ps, "bone", cuts=cuts)]

@d("dungeon", "made", "paper", "spellbook")
def _(cx, cy, a, rng):
    u = rng.uniform; ff = frame(cx, cy, a); L = u(2.6, 3.2); W = u(1.9, 2.3)
    ps = blk(cx, cy, 2 * W + 0.5, L + 0.3, a, 0.3) + [s for sv in (-1, 1) for s in loaf(*ff(sv * (W / 2 + 0.06), 0), a + 1.5708, L, W, u(0.5, 0.75))]
    cuts = [(rect(*ff(sv * (W / 2 + 0.1), v), W * 0.62, 0.16, a), 0.36) for sv in (-1, 1) for v in np.arange(-L / 2 + 0.6, L / 2 - 0.4, 0.5) if rng.random() < 0.85]
    if rng.random() < 0.5: c = ff(W / 2, 0); cuts.append((ngon(c[0], c[1], 0.5, rng.choice((3, 5, 6)), a).exterior.buffer(0.09), 0.36)); cuts = [q for q in cuts if q[0].distance(Point(c)) > 0.75 or q is cuts[-1]]
    return rect(cx, cy, 2 * W + 0.7, L + 0.5, a), [ST(ps, "paper", cuts=cuts)]

@d("dungeon", "made", "iron", "key_ring")
def _(cx, cy, a, rng):
    u = rng.uniform; R = u(0.95, 1.2); gs = [ann(cx, cy, R, R - 0.34, 14)]
    for k in range(rng.randint(3, 5)):
        t = a + k * 0.75 + u(-0.15, 0.15); p0 = pol(cx, cy, R - 0.1, t); L = u(2.0, 2.8); e = pol(p0[0], p0[1], L, t); f2 = frame(e[0], e[1], t)
        gs += [barl(p0, e, 0.34), ann(*pol(p0[0], p0[1], 0.5, t), 0.5, 0.2, 8), rect(*f2(-0.3, 0.4), 0.5, 0.6, t), rect(*f2(-1.0, 0.36), 0.36, 0.5, t)]
    g = U(gs)
    return g.convex_hull.buffer(0.2), [ST(ext(g, 0.36), "iron")]

@d("dungeon", "made", "gold", "crown_sceptre")
def _(cx, cy, a, rng):
    u = rng.uniform; r = u(1.0, 1.25); h = u(0.8, 1.1); n = rng.randint(5, 8); f2 = frame(*pol(cx, cy, r + 1.4, a + 1.57), a + u(-0.4, 0.4)); L = u(5.0, 6.5)
    ps = ext(ann(cx, cy, r, r - 0.42, 18), h) + [s for p in ringpts(cx, cy, r - 0.21, n, a) for s in cone(p[0], p[1], 0.34, h + u(0.5, 0.8), 0.2, n=6)]
    sc_ = rod([f2(-L / 2, 0), f2(L / 2, 0)], 0.25, 1.1) + dome(*f2(L / 2, 0), 0.55, 0.9, n=10) + lie(*f2(L / 2 - 0.8, 0), a, 0.36, 0.42) + dome(*f2(-L / 2, 0), 0.36, 0.5, n=8)
    return hull([disc(cx, cy, r + 0.3), rect(*f2(0, 0), L + 1.2, 1.3, a)]), [ST(ps, "gold"), ST(sc_, "gold")]

@d("dungeon", "made", "iron", "portcullis_piece")
def _(cx, cy, a, rng):
    u = rng.uniform; ff = frame(cx, cy, a); nx = rng.randint(3, 5); ny = rng.randint(2, 3); s = 1.35; gs = []
    for i in range(nx): x = (i - (nx - 1) / 2) * s; gs += [barl(ff(x, -ny * s / 2 - 0.2), ff(x, ny * s / 2), 0.5), tri(ff(x, -ny * s / 2 - 0.1), ff(x, -ny * s / 2 - 1.0), 0.5)]
    for j in range(ny): y = (j - (ny - 1) / 2) * s; gs.append(barl(ff(-nx * s / 2, y), ff(nx * s / 2 * (u(0.3, 0.8) if rng.random() < 0.3 else 1.0), y), 0.5))
    g = U(gs)
    return g.convex_hull.buffer(0.2), [ST(ext(g, 0.5), "iron")]

@d("dungeon", "made", "iron", "gibbet_cage")
def _(cx, cy, a, rng):
    u = rng.uniform; ff = frame(cx, cy, a); L = u(5.0, 6.2); r = u(1.1, 1.3); prof = lambda t: r * math.sin(max(0.06, min(0.94, (t + L / 2) / L)) ** 0.6 * math.pi) ** 0.5
    ps = [s for x in np.arange(-L / 2 + 0.6, L / 2 - 0.3, 1.0) for s in lie(*ff(x, 0), a, 0.36, prof(x) + 0.08)] + [s for v in (-0.62, 0.0, 0.62) for s in rod([ff(x, v * prof(x) / r * 1.3) for x in np.linspace(-L / 2 + 0.3, L / 2 - 0.3, 10)], 0.24, 1.0 + 3.4 * (1 - abs(v)))] + ext(ann(*ff(L / 2 + 0.4, 0), 0.6, 0.28, 8), 0.4)
    lay = [ST(ps, "iron")]
    return rect(*ff(0.3, 0), L + 1.2, 2 * r + 0.4, a), lay

@d("dungeon", "made", "wood", "mimic")
def _(cx, cy, a, rng):
    u = rng.uniform; ff = frame(cx, cy, a); L = u(3.4, 4.2); W = u(2.2, 2.6); h = u(1.0, 1.3); e = ff(u(-0.8, 0.8), -W / 2 - u(1.8, 2.8))
    ps = blk(cx, cy, L, W, a, h) + loaf(*ff(0, W * 0.3), a, L, W * 0.5, h + W * 0.3) + [s for x in np.arange(-L / 2 + 0.5, L / 2 - 0.2, 0.62) for s in cone(*ff(x, -W / 2 + 0.38), 0.27, h + u(0.4, 0.7), 0.2, n=5)]
    tongue = rod(wiggle(ff(0, -W * 0.2), e, rng, 0.6, 4), lambda q: 0.55 - 0.2 * q, 0.7)
    return hull([rect(cx, cy, L, W, a), disc(e[0], e[1], 0.6)]), [ST(ps + tongue, "wood", cuts=[(rect(*ff(0, -W * 0.1), L - 1.1, W * 0.3, a).difference(LineString([ff(0, -W * 0.2), e]).buffer(0.8)), h * 0.5)])]

@d("dungeon", "made", "flag", "pressure_plate")
def _(cx, cy, a, rng):
    u = rng.uniform; ff = frame(cx, cy, a); S = u(2.8, 3.6); R = rect(cx, cy, S, S, a)
    slits = U([rect(*ff(S / 2 + u(1.2, 2.0), (i - 1) * 1.0), 0.9, 0.3, a) for i in range(3)])
    return rect(*ff(0.9, 0), S + 2.8, S + 0.2, a), [ST(ext(R, 0.3) + ext(R.buffer(-0.5), 0.42), "flag", cuts=[(star(cx, cy, S * 0.25, S * 0.1, 4, a), 0.25)]), ("gcut", slits, 0.5, "slot")]

@d("dungeon", "made", "wood", "block_and_axe")
def _(cx, cy, a, rng):
    u = rng.uniform; ff = frame(cx, cy, a); r = u(1.3, 1.6); h = u(1.2, 1.6); L = u(4.6, 5.6); b = a + u(-0.4, 0.4); f2 = frame(*ff(0, r + 1.5), b)
    blade = Polygon([f2(L / 2 - 1.0, -0.2), f2(L / 2 - 0.2, -1.5), f2(L / 2 + 0.5, -1.2), f2(L / 2 + 0.6, 0.3), f2(L / 2 - 0.2, 0.3)])
    return hull([disc(cx, cy, r + 0.2), rect(*f2(0, -0.5), L + 1.4, 2.2, b)]), [ST(turn(cx, cy, [(r * 1.08, 0.0), (r, h * 0.2), (r, h)], 9), "wood", cuts=[(ell(*ff(0, -r * 0.5), r * 0.5, r * 0.32, a, 10), h - 0.35), (barl(ff(-r * 0.7, r * 0.3), ff(r * 0.6, r * 0.5), 0.2), h - 0.2)]), ST(rod([f2(-L / 2, 0), f2(L / 2, 0)], 0.27, 1.1) + ext(blade, 0.42), "iron")]

@d("dungeon", "made", "flag", "keystone")
def _(cx, cy, a, rng):
    u = rng.uniform; ff = frame(cx, cy, a); h = u(0.9, 1.2); ps = []; foot = []
    for i, x in enumerate((-2.4, 0.0, 2.5)[:rng.randint(2, 3)]):
        f2 = frame(*ff(x + u(-0.2, 0.2), u(-0.4, 0.4)), a + u(-0.3, 0.3) + 0.2 * x); g = Polygon([f2(-0.75, -1.2), f2(0.75, -1.2), f2(1.1, 1.2), f2(-1.1, 1.2)]); ps += ext(g, h * u(0.85, 1.0)); foot.append(g)
    c = foot[min(1, len(foot) - 1)].centroid
    return hull(foot).buffer(0.2), [ST(ps, "flag", cuts=[(disc(c.x, c.y - 0.0, 0.5, 8).exterior.buffer(0.1), h * 0.6), (disc(c.x, c.y, 0.16, 6), h * 0.5)])]

@d("dungeon", "made", "bone", "dice_and_cup")
def _(cx, cy, a, rng):
    u = rng.uniform; ps = []; cuts = []; foot = []; c = pol(cx, cy, 2.2, a); b = a + u(1.5, 3.5); L = 2.0
    for i in range(rng.randint(2, 3)):
        x, y = pol(cx, cy, u(0.0, 1.2), a + 3.14 + i * 1.6); t = u(0, 1.5); ps += ext(rect(x, y, 0.95, 0.95, t).buffer(-0.15).buffer(0.15), 0.9); f2 = frame(x, y, t)
        cuts += [(disc(*f2(px, py), 0.13, 5), 0.72) for px, py in rng.choice(([(0, 0)], [(-0.22, -0.22), (0.22, 0.22)], [(-0.24, -0.24), (0, 0), (0.24, 0.24)], [(-0.22, -0.22), (0.22, 0.22), (-0.22, 0.22), (0.22, -0.22)]))]; foot.append(disc(x, y, 0.8))
    cup = spin(c[0], c[1], b, L, lambda q: 0.62 + 0.14 * (q + L / 2) / L, 1.1)
    return hull(foot + [rect(c[0], c[1], L, 1.7, b)]).buffer(0.2), [ST(ps, "bone", cuts=cuts), ST(cup, "leather")] if all(g.distance(rect(c[0], c[1], L, 1.7, b)) > 0.2 for g in foot) else [ST(ps, "bone", cuts=cuts)]

@d("dungeon", "debris", "slime", "ooze")
def _(cx, cy, a, rng):
    u = rng.uniform; R = u(1.8, 2.5); ps = []; kn = lambda q: 0.26 + 0.14 * (q < 0.14 or q > 0.86)
    for i in range(rng.randint(4, 6)): p = pol(cx, cy, u(0, R * 0.55), u(0, 6.28)); r = u(0.9, 1.4); ps += turn(p[0], p[1], [(r, 0.0), (r * 0.85, 0.35), (r * 0.4, 0.55)], 12)
    for i in range(rng.randint(1, 3)): t = u(0, 6.28); p = pol(cx, cy, u(0, R * 0.3), u(0, 6.28)); ps += rod([p, pol(p[0], p[1], u(1.6, 2.4), t)], kn, 1.0, lift=0.3)
    return disc(cx, cy, R + 1.2), [ST(ps, "slime")]

@d("dungeon", "made", "iron", "mace_and_shield")
def _(cx, cy, a, rng):
    u = rng.uniform; ff = frame(cx, cy, a); L = u(3.6, 4.4); W = u(2.6, 3.1); f2 = frame(*ff(u(-0.5, 0.5), W / 2 + 1.3), a + u(-0.4, 0.4)); Lm = u(4.0, 5.0)
    sh = spin(cx, cy, a, L, lambda t: W / 2 * (1.0 if t < -L * 0.1 else max(0.0, 1 - ((t + L * 0.1) / (L * 0.6)) ** 1.6)) + 0.04, 0.5) + dome(cx - 0.0, cy, 0.6, 0.95, n=10)
    mace = rod([f2(-Lm / 2, 0), f2(Lm / 2 - 0.8, 0)], 0.28, 1.1) + fluted(*f2(Lm / 2 - 0.4, 0), 0.85, 1.1, 6, a, 0.55, 4)
    return hull([rect(cx, cy, L, W, a), rect(*f2(0, 0), Lm + 0.6, 1.8, a)]), [ST(sh, "iron", cuts=[(barl(ff(-L * 0.42, 0), ff(L * 0.4, 0), 0.22).difference(disc(cx, cy, 0.8)), 0.3)]), ST(mace, "iron")]

@d("dungeon", "made", "paper", "scrolls")
def _(cx, cy, a, rng):
    u = rng.uniform; ff = frame(cx, cy, a); ps = []; n = rng.randint(2, 4)
    for i in range(n):
        c = ff(u(-0.4, 0.4), (i - (n - 1) / 2) * 1.0); b = a + u(-0.25, 0.25); L = u(2.6, 3.4); ps += lie(c[0], c[1], b, L, 0.38) + [s for sv in (-1, 1) for s in lie(*pol(c[0], c[1], L / 2 + 0.15, b + (0 if sv > 0 else math.pi)), b, 0.3, 0.5)]
    f2 = frame(*ff(0, n * 0.5 + 1.9), a + u(-0.2, 0.2)); op = ext(rect(*f2(0, 0), 2.6, 2.6, a), 0.24) + lie(*f2(0, 1.4), a, 3.0, 0.4) + lie(*f2(0, -1.4), a, 3.0, 0.4)
    return rect(*ff(0, 1.2), 4.4, n + 5.0, a), [ST(ps, "paper"), ST(op, "paper", cuts=[(rect(*f2(0, v), 1.9, 0.16, a), 0.12) for v in (-0.6, -0.1, 0.4)])]

@d("dungeon", "marks", "slot", "blood_drain", ground=True)
def _(cx, cy, a, rng):
    u = rng.uniform; R = u(1.1, 1.5); e = pol(cx, cy, R + u(3.0, 5.0), a + u(-0.3, 0.3))
    gs = [ann(cx, cy, R, R - 0.3, 14), disc(cx, cy, 0.34, 8)] + [barl(pol(cx, cy, 0.5, a + k * 0.7854), pol(cx, cy, R - 0.4, a + k * 0.7854), 0.26) for k in range(8)]
    ch = wavy(pol(cx, cy, R + 0.3, a), e, rng, 0.3, 3, 0.42)
    return hull([disc(cx, cy, R + 0.2), disc(e[0], e[1], 0.4)]), [("gcut", U(gs), 0.6, "slot"), ("gcut", ch, 0.28, "seam")]

@d("dungeon", "made", "brass", "candelabra")
def _(cx, cy, a, rng):
    u = rng.uniform; ff = frame(cx, cy, a); L = u(5.0, 6.2); n = rng.choice((3, 5)); W = u(1.2, 1.7)
    ps = turn(*ff(-L / 2, 0), [(1.0, 0.0), (1.0, 0.3), (0.42, 0.5)], 12) + spin(cx, cy, a, L, lambda t: 0.3 + 0.12 * math.sin(t * 2.6) ** 2, 1.1)
    for k in range(n):
        v = (k - (n - 1) / 2) * W; e = ff(L / 2 + 0.6 - abs(v) * 0.2, v); ps += rod([ff(L * 0.2, 0), ff(L * 0.36, v * 0.9), e], 0.25, 1.1) + lie(e[0], e[1], a, 0.7, 0.42)
    return rect(*ff(0.2, 0), L + 2.2, n * W + 1.2, a), [ST(ps, "brass")]

# ====================================================================================================== temple
@d("temple", "made", "gold", "offering_bowl")
def _(cx, cy, a, rng):
    u = rng.uniform; R = u(1.6, 2.0); h = u(0.8, 1.1); ps = turn(cx, cy, [(R * 0.5, 0.0), (R * 0.5, 0.2), (R, h)], 20); fr = []
    for i in range(rng.randint(3, 7)): p = pol(cx, cy, u(0, R * 0.4), u(0, 6.28)); fr += dome(p[0], p[1], u(0.36, 0.5), u(0.4, 0.6), lift=h - 0.25, n=8)
    return disc(cx, cy, R + 0.2), [ST(ps + fr, "gold", cuts=[(ann(cx, cy, R - 0.3, R * 0.62, 16), h - 0.3)])]

@d("temple", "made", "gold", "censer")
def _(cx, cy, a, rng):
    u = rng.uniform; r = u(0.9, 1.1); e = pol(cx, cy, r + u(3.0, 4.5), a); pts = wiggle(pol(cx, cy, r * 0.5, a), e, rng, 0.8, 6)
    ps = turn(cx, cy, [(r * 0.6, 0.0), (r * 0.6, 0.2), (r, r * 0.7)] + crv(r, 0.3, r * 0.7, r * 1.6) + [(0.3, r * 1.9)], 14) + rod(pts, lambda q: 0.24 + 0.08 * (math.sin(q * 50) > 0), 1.0) + ext(ann(e[0], e[1], 0.55, 0.26, 8), 0.4)
    cuts = [(disc(*p, 0.17, 5), r * 0.9) for p in ringpts(cx, cy, r * 0.55, 6, a + 0.5)]
    return hull([disc(cx, cy, r + 0.2), disc(e[0], e[1], 0.7)]), [ST(ps, "gold", cuts=cuts)]

@d("temple", "made", "gold", "idol")
def _(cx, cy, a, rng):
    u = rng.uniform; ff = frame(cx, cy, a); S = u(2.0, 2.5); r = S * 0.36; h = u(1.6, 2.2)
    ps = blk(cx, cy, S, S, a, 0.5) + blk(cx, cy, S * 0.8, S * 0.8, a, 0.75) + turn(cx, cy, [(r * 1.2, 0.75), (r, 0.75 + h * 0.5), (r * 0.5, 0.75 + h * 0.6), (r * 0.72, 0.75 + h * 0.72)] + crv(r * 0.72, 0.26, 0.75 + h * 0.72, 0.75 + h), 14) + [s for sv in (-1, 1) for s in dome(*ff(r * 0.6, sv * r * 0.8), r * 0.5, 0.5, lift=0.75, n=8)]
    cuts = [(disc(*ff(r * 0.4, sv * r * 0.28), 0.17, 5), 0.75 + h * 0.75) for sv in (-1, 1)]
    return rect(cx, cy, S + 0.2, S + 0.2, a), [ST(ps, "gold", cuts=cuts)]

@d("temple", "made", "gold", "prayer_wheel")
def _(cx, cy, a, rng):
    u = rng.uniform; ff = frame(cx, cy, a); L = u(2.4, 3.0); r = u(0.9, 1.15); Lh = u(2.2, 3.0)
    ps = spin(cx, cy, a, L, lambda t: r + 0.1 * (math.sin(t * 9) > 0.2) if abs(t) < L / 2 - 0.3 else r + 0.16, 1.1) + rod([ff(-L / 2 - 0.5, 0), ff(L / 2 + Lh, 0)], 0.27, 1.1) + lie(*ff(L / 2 + Lh * 0.6, 0), a, Lh * 0.5, 0.4)
    return rect(*ff(Lh / 2 - 0.2, 0), L + Lh + 1.2, 2 * r + 0.4, a), [ST(ps, "gold")]

@d("temple", "made", "gold", "gong")
def _(cx, cy, a, rng):
    u = rng.uniform; R = u(2.0, 2.6); f2 = frame(*pol(cx, cy, R + 1.1, a), a + 1.57 + u(-0.4, 0.4)); L = u(3.4, 4.2)
    ps = turn(cx, cy, [(R, 0.0), (R, 0.5), (R - 0.3, 0.5), (R - 0.4, 0.32), (R * 0.36, 0.32)] + crv(R * 0.36, 0.25, 0.32, 0.8), 28)
    cuts = [(disc(*pol(cx, cy, R - 0.16, a + 3.14 + sv * 0.35), 0.12, 5), 0.2) for sv in (-1, 1)]
    mal = rod([f2(-L / 2, 0), f2(L / 2 - 0.5, 0)], 0.25, 1.1) + dome(*f2(L / 2 - 0.2, 0), 0.6, 0.9, n=10)
    return hull([disc(cx, cy, R + 0.2), rect(*f2(0, 0), L + 0.6, 1.4, a + 1.57)]), [ST(ps, "gold", cuts=cuts), ST(mal, "wood")]

@d("temple", "made", "gold", "singing_bowl")
def _(cx, cy, a, rng):
    u = rng.uniform; R = u(1.2, 1.5); r = R * 0.85; h = u(1.0, 1.3); f2 = frame(*pol(cx, cy, R + 1.3, a), a + 1.57 + u(-0.5, 0.5))
    ps = rod([pol(cx, cy, R, t) for t in np.linspace(0, 6.2832, 24)], 0.5, 0.9) + turn(cx, cy, [(r * 0.7, 0.0), (r * 0.7, 0.4), (r, 0.4 + h * 0.5), (r * 0.95, 0.4 + h)], 18)
    return hull([disc(cx, cy, R + 0.6), rect(*f2(0, 0), 3.4, 1.0, a + 1.57)]), [ST(ps, "gold", cuts=[(disc(cx, cy, r * 0.95 - 0.28, 14), 0.4 + h - 0.6)]), ST(spin(*f2(0, 0), f2(1, 0)[0] * 0 + a + 1.57, 3.0, lambda t: 0.26 + 0.14 * (t > 0.5), 1.1), "wood")]

@d("temple", "marks", "glyph", "sun_inlay")
def _(cx, cy, a, rng):
    u = rng.uniform; R = u(1.3, 1.7); n = rng.randint(8, 14); k = rng.random() < 0.5
    gs = [ann(cx, cy, R, R - 0.3, 20)] + [(tri(pol(cx, cy, R + 0.25, a + 6.2832 * i / n), pol(cx, cy, R + (1.6 if i % 2 or not k else 1.0), a + 6.2832 * i / n), 0.6)) for i in range(n)]
    gs += [disc(*pol(cx, cy, R * 0.36, a + sv * 0.6), 0.2, 6) for sv in (-1, 1)] + [LineString(arc(cx, cy, R * 0.5, a + 2.5, a + 3.8, 6)).buffer(0.13)]
    return disc(cx, cy, R + 1.8), [("gcut", U(gs), 0.3, "glyph")]

@d("temple", "made", "tcolumn", "votive_tablets")
def _(cx, cy, a, rng):
    u = rng.uniform; ff = frame(cx, cy, a); n = rng.randint(3, 6); ps = []; cuts = []
    for i in range(n):
        c = ff((i - (n - 1) / 2) * 1.5, u(-0.3, 0.3)); b = a + u(-0.25, 0.25); h = u(0.36, 0.5); R = rect(c[0], c[1], 1.25, 2.0, b).buffer(-0.2).buffer(0.2); ps += ext(R, h); f2 = frame(c[0], c[1], b)
        cuts += [(rect(*f2(0, v), 0.75, 0.16, b), h - 0.16) for v in (-0.5, 0.0, 0.5) if rng.random() < 0.8]
    return rect(cx, cy, n * 1.5 + 0.4, 2.8, a), [ST(ps, "tcolumn", cuts=cuts)]

@d("temple", "made", "gold", "sun_mask")
def _(cx, cy, a, rng):
    u = rng.uniform; ff = frame(cx, cy, a); r = u(1.2, 1.5); h = r * 0.55; n = rng.randint(7, 11)
    ps = dome(cx, cy, r, h, sq=0.85, a=a) + [s for k in range(n) for s in ext(tri(pol(cx, cy, r * 0.7, a + 1.5708 + (k - (n - 1) / 2) * 0.3), pol(cx, cy, r + u(0.9, 1.6), a + 1.5708 + (k - (n - 1) / 2) * 0.3) if False else pol(cx, cy, r + u(0.9, 1.6), a + (k - (n - 1) / 2) * 0.32 + 3.14), 0.62), 0.32)]
    cuts = [(ell(*ff(-r * 0.15, sv * r * 0.36), r * 0.2, r * 0.12, a, 8), 0.1) for sv in (-1, 1)] + [(rect(*ff(r * 0.5, 0), 0.26, r * 0.6, a), h * 0.3)]
    return disc(cx, cy, r + 1.7), [ST(ps, "gold", cuts=cuts)]

@d("temple", "made", "tcolumn", "serpent_drum")
def _(cx, cy, a, rng):
    u = rng.uniform; ff = frame(cx, cy, a); L = u(3.6, 4.6); r = u(1.1, 1.35); k = u(3.0, 4.5); ph = u(0, 6); hd = ff(L / 2 + 0.7, u(-0.4, 0.4))
    ps = spin(cx, cy, a, L, lambda t: r + 0.16 * math.sin(t * k + ph) ** 2, 1.1) + dome(hd[0], hd[1], 0.62, 0.7, sq=0.7, a=a) + rod([ff(L / 2 - 0.3, 0), hd], 0.42, 1.0)
    return rect(*ff(0.5, 0), L + 1.8, 2 * r + 0.5, a), [ST(ps, "tcolumn", cuts=[(disc(*pol(hd[0], hd[1], 0.25, a + sv * 0.9), 0.14, 5), 0.5) for sv in (-1, 1)])]

@d("temple", "made", "tcolumn", "lotus_basin")
def _(cx, cy, a, rng):
    u = rng.uniform; R = u(1.8, 2.3); n = rng.randint(7, 10); h = u(0.9, 1.2)
    ps = [s for k in range(4) for s in ext(star(cx, cy, R * (1 - 0.06 * k), R * (1 - 0.06 * k) * 0.72, n, a + 0.0), h * (k + 1) / 4)]
    return disc(cx, cy, R + 0.2), [ST(ps, "tcolumn", cuts=[(disc(cx, cy, R * 0.55, 16), h - u(0.3, 0.5))])]

@d("temple", "made", "obsidian", "black_mirror")
def _(cx, cy, a, rng):
    u = rng.uniform; ff = frame(cx, cy, a); R = u(1.3, 1.7); n = rng.randint(8, 12)
    ps = turn(cx, cy, [(R, 0.0), (R, 0.5), (R - 0.4, 0.5), (R - 0.42, 0.3), (0.3, 0.34)], 24) + [s for p in ringpts(cx, cy, R - 0.2, n, a) for s in dome(p[0], p[1], 0.26, 0.7, n=6)] + spin(*ff(-R - 1.2, 0), a, 2.6, lambda t: 0.36 + 0.1 * math.sin(t * 4) ** 2, 1.1)
    cuts = [(wavy(pol(cx, cy, R * 0.6, a + 1), pol(cx, cy, R * 0.6, a + 3.6), rng, 0.3, 3, 0.2), 0.12)] if rng.random() < 0.4 else []
    return rect(*ff(-1.2, 0), 2 * R + 2.8, 2 * R + 0.3, a), [ST(ps, "obsidian", cuts=cuts)]

@d("temple", "made", "gold", "ritual_dagger")
def _(cx, cy, a, rng):
    u = rng.uniform; ff = frame(cx, cy, a); L = u(3.6, 4.6); cl = rect(*ff(u(-0.4, 0.4), u(-0.3, 0.3)), L + 1.6, 2.6, a + u(-0.3, 0.3))
    blade = Polygon([ff(0, -0.42)] + [ff(x, 0.42 * (1 - x / L) + 0.16 * math.sin(x * 4)) for x in np.linspace(0, L * 0.72, 10)][::-1][:0] + [ff(x, -0.45 * (1 - x / (L * 0.75)) + 0.14 * math.sin(x * 4.2)) for x in np.linspace(0, L * 0.72, 10)] + [ff(L * 0.75, 0)] + [ff(x, 0.45 * (1 - x / (L * 0.75)) + 0.14 * math.sin(x * 4.2)) for x in np.linspace(L * 0.72, 0, 10)]).buffer(0)
    ps = ext(cl, 0.22) + ext(blade, 0.52) + rod([ff(-L * 0.25, 0), ff(0, 0)], 0.3, 1.5) + blk(*ff(0, 0), 0.4, 1.7, a, 0.62) + dome(*ff(-L * 0.25 - 0.2, 0), 0.42, 0.8, n=8)
    return cl.buffer(0.2), [ST(ps, "gold")]

@d("temple", "made", "tcolumn", "glyph_lintel")
def _(cx, cy, a, rng):
    u = rng.uniform; ff = frame(cx, cy, a); L = u(7.0, 9.0); W = u(1.6, 2.0); h = u(0.9, 1.2); br = u(-0.25, 0.25) * L; gap = u(0.3, 0.9); b = u(-0.25, 0.25)
    f1 = frame(*ff(-L / 4 + br / 2 - gap / 2, 0), a); f2 = frame(*ff(L / 4 + br / 2 + gap / 2, u(-0.5, 0.5)), a + b); L1 = L / 2 + br; L2 = L / 2 - br
    ps = blk(*f1(0, 0), L1, W, a, h) + blk(*f2(0, 0), L2, W, a + b, h * u(0.8, 1.0))
    cuts = [(ngon(*f(x, 0), 0.45, rng.choice((3, 4, 4, 6)), aa + u(0, 1)).exterior.buffer(0.1), h - 0.2) for f, Lx, aa in ((f1, L1, a), (f2, L2, a + b)) for x in np.arange(-Lx / 2 + 0.8, Lx / 2 - 0.6, 1.3)]
    return hull([rect(*f1(0, 0), L1, W, a), rect(*f2(0, 0), L2, W, a + b)]).buffer(0.2), [ST(ps, "tcolumn", cuts=cuts)]

@d("temple", "made", "jade", "figurines")
def _(cx, cy, a, rng):
    u = rng.uniform; ff = frame(cx, cy, a); n = rng.randint(3, 5); ps = []
    for i in range(n):
        c = ff((i - (n - 1) / 2) * 1.5, u(-0.25, 0.25)); r = u(0.48, 0.62); h = u(1.3, 2.2); k = rng.randint(0, 2)
        if rng.random() < 0.2: ps += spin(c[0], c[1], a + 1.57 + u(-0.5, 0.5), h, lambda t, r=r: r * (0.7 + 0.3 * math.sin(t * 5) ** 2), 1.1); continue
        ps += ext(ngon(c[0], c[1], r * 1.3, 4, a + 0.785), 0.25) + turn(c[0], c[1], [(r, 0.0), (r * (0.8 if k else 1.1), h * 0.5), (r * 0.5, h * 0.6), (r * (0.75 if k < 2 else 1.0), h * 0.75), (0.25, h)], 10)
    return rect(cx, cy, n * 1.5 + 0.8, 2.6, a), [ST(ps, "jade")]

@d("temple", "made", "tcolumn", "calendar_stone")
def _(cx, cy, a, rng):
    u = rng.uniform; R = u(2.4, 3.2); h = u(0.6, 0.8); n1 = rng.randint(12, 20)
    cuts = [(ann(cx, cy, R * q, R * q - 0.2, 24), h - 0.18) for q in (0.9, 0.62, 0.36)] + [(barl(pol(cx, cy, R * 0.66, a + 6.2832 * k / n1), pol(cx, cy, R * 0.84, a + 6.2832 * k / n1), 0.2), h - 0.18) for k in range(n1)] + [(disc(*pol(cx, cy, R * 0.14, a + sv * 0.8), 0.17, 5), h - 0.2) for sv in (-1, 1)]
    if rng.random() < 0.3: cuts.append((Polygon([(cx, cy), pol(cx, cy, 2 * R, a + 2), pol(cx, cy, 2 * R, a + 2 + u(0.5, 1.0))]), 0.0))
    return disc(cx, cy, R + 0.2), [ST(turn(cx, cy, [(R, 0.0), (R, h)], 32), "tcolumn", cuts=cuts)]

@d("temple", "made", "gold", "sun_staff")
def _(cx, cy, a, rng):
    u = rng.uniform; ff = frame(cx, cy, a); L = u(8.0, 10.0); e = ff(L / 2, 0); R = u(0.95, 1.2); n = rng.randint(7, 10)
    ps = rod([ff(-L / 2, 0), e], 0.27, 1.1) + ext(disc(e[0], e[1], R, 16), 0.5) + [s for k in range(n) for s in ext(tri(pol(e[0], e[1], R - 0.1, a + (k - (n - 1) / 2) * 0.4), pol(e[0], e[1], R + u(0.8, 1.3), a + (k - (n - 1) / 2) * 0.4), 0.55), 0.34)] + lie(*ff(L * 0.2, 0), a, 0.42, 0.42)
    return rect(*ff(0.6, 0), L + 2.6, 2 * R + 2.4, a), [ST(ps, "gold", cuts=[(ann(e[0], e[1], R * 0.62, R * 0.62 - 0.18, 12), 0.34)])]

@d("temple", "made", "wood", "prayer_beads")
def _(cx, cy, a, rng):
    u = rng.uniform; R = u(1.5, 2.0); tw = u(0.0, 0.5); t0 = pol(cx, cy, R, a)
    pts = [pol(cx, cy, R * (1 + tw * math.sin(2 * t)) * (0.8 + 0.2 * math.cos(t)), a + t) for t in np.linspace(0, 6.2832, 34)]
    ps = rod(pts, lambda q: 0.25 + 0.14 * (math.sin(q * 110) > 0), 1.0) + dome(pts[0][0], pts[0][1], 0.5, 0.7, n=8) + [s for sv in (-0.3, 0.0, 0.3) for s in rod([pts[0], pol(pts[0][0], pts[0][1], u(1.2, 1.7), a + sv)], 0.21, 0.9)]
    return LineString(pts).buffer(0.6).convex_hull.union(disc(*pol(pts[0][0], pts[0][1], 1.2, a), 0.9)).convex_hull, [ST(ps, "wood")]

@d("temple", "made", "tflag", "meander_relief")
def _(cx, cy, a, rng):
    u = rng.uniform; ff = frame(cx, cy, a); n = rng.randint(2, 4); s = 2.0; R = rect(cx, cy, n * s + 0.9, 2.9, a); pts = []
    for i in range(n): x = (i - n / 2) * s; pts += [ff(x, -0.95), ff(x, 0.95), ff(x + 1.45, 0.95), ff(x + 1.45, -0.2), ff(x + 0.7, -0.2), ff(x + 0.7, 0.35)][:6] if False else [ff(x, -0.95), ff(x, 0.95), ff(x + 1.45, 0.95), ff(x + 1.45, -0.95)]
    key = LineString(pts + [ff(n * s / 2, -0.95)]).buffer(0.2, cap_style=2, join_style=2)
    return R.buffer(0.2), [ST(ext(R, 0.34) + ext(key, 0.62) + ext(R.difference(R.buffer(-0.3)), 0.5), "tflag")]

@d("temple", "made", "tcolumn", "water_clock")
def _(cx, cy, a, rng):
    u = rng.uniform; ff = frame(cx, cy, a); ps = []; cuts = []; x = -2.6
    for i in range(3):
        r = 1.15 - 0.2 * i; h = 1.9 - 0.5 * i; c = ff(x + r, 0); ps += turn(c[0], c[1], [(r * 0.8, 0.0), (r, h)], 14); cuts.append((disc(c[0], c[1], r - 0.3, 10), h - u(0.3, 0.6)))
        if i < 2: ps += blk(*ff(x + 2 * r + 0.1, 0), 0.6, 0.42, a, h - 0.45)
        x += 2 * r + 0.2
    return rect(*ff(0.05, 0), 6.2, 2.6, a), [ST(ps, "tcolumn", cuts=cuts)]

# ====================================================================================================== coast
@d("coast", "growth", "shell", "starfish")
def _(cx, cy, a, rng):
    u = rng.uniform; ps = []; foot = []
    for i in range(rng.randint(1, 3)):
        x, y = pol(cx, cy, 0 if i == 0 else u(2.4, 3.2), a + i * 2.4); R = u(1.2, 1.7); b = u(0, 1.2); ps += dome(x, y, R * 0.42, 0.55, n=10)
        for k in range(5): t = b + k * 1.2566 + u(-0.12, 0.12); c = pol(x, y, R * 0.55, t); e = pol(x, y, R, t + u(-0.3, 0.3)); ps += rod([pol(x, y, 0.1, t), c, e], lambda q: 0.42 - 0.2 * q, 1.0)
        foot.append(disc(x, y, R + 0.2))
    return hull(foot), [ST(ps, "shell")]

@d("coast", "made", "shell", "conch")
def _(cx, cy, a, rng):
    u = rng.uniform; ff = frame(cx, cy, a); L = u(3.0, 4.0); r = u(0.95, 1.2)
    ps = spin(cx, cy, a, L, lambda t: (r * (0.25 + 0.75 * (t + L / 2) / (L * 0.62)) if t < L * 0.12 else r * (1 - 0.6 * (t - L * 0.12) / (L * 0.38))) * (0.82 + 0.18 * abs(math.sin(t * 5.5))) + 0.05, 1.05) + ext(ell(*ff(L * 0.08, r * 0.85), L * 0.3, r * 0.5, a, 12), r * 0.45)
    return rect(*ff(0, 0.3), L + 0.2, 2 * r + 1.0, a), [ST(ps, "shell", cuts=[(ell(*ff(L * 0.08, r * 0.8), L * 0.2, r * 0.22, a, 8), 0.2)])]

@d("coast", "made", "shell", "crab")
def _(cx, cy, a, rng):
    u = rng.uniform; ff = frame(cx, cy, a); W = u(2.0, 2.6); D = W * 0.65
    ps = dome(cx, cy, W / 2, W * 0.36, sq=0.68, a=a + 1.5708)
    for sv in (-1, 1):
        el = ff(D * 0.55, sv * W * 0.62); cl = ff(D * 0.95, sv * W * u(0.2, 0.5)); ps += rod([ff(D * 0.2, sv * W * 0.36), el, cl], 0.3, 1.0) + dome(cl[0], cl[1], 0.5, 0.6, sq=0.7, a=a + sv * 0.6, n=8)
        for k in range(3): x = -D * 0.1 - k * 0.42; ps += rod([ff(x, sv * W * 0.4), ff(x - 0.1, sv * (W / 2 + 0.6)), ff(x - 0.5 - 0.2 * k, sv * (W / 2 + 1.1))], 0.22, 1.0)
    return rect(*ff(0.2, 0), D + 2.4, W + 2.6, a), [ST(ps, "shell", cuts=[(disc(*ff(D * 0.42, sv * 0.3), 0.14, 5), 0.3) for sv in (-1, 1)])]

@d("coast", "made", "twig", "lobster_pot")
def _(cx, cy, a, rng):
    u = rng.uniform; ff = frame(cx, cy, a); L = u(4.0, 5.0); W = u(2.6, 3.2); h = W * 0.5; e = ff(L / 2 + u(1.5, 3.0), u(-2, 2))
    ps = blk(cx, cy, L, W, a, 0.3) + [s for x in np.linspace(-L / 2 + 0.25, L / 2 - 0.25, rng.randint(4, 6)) for s in _S().vault(*ff(x, 0), a, 0.5, W, h, "round")] + [s for v in (-W * 0.3, 0.0, W * 0.3) for s in rod([ff(-L / 2, v), ff(L / 2, v)], 0.24, 1.0, lift=h * math.sqrt(max(0.05, 1 - (2 * v / W) ** 2)) - 0.2)]
    rp_ = rod(wiggle(ff(L / 2 + 0.3, 0), e, rng, 0.6, 4), 0.25, 1.0) + dome(e[0], e[1], 0.6, 0.9, n=10)
    return hull([rect(cx, cy, L, W, a), disc(e[0], e[1], 0.8)]), [ST(ps, "twig"), ST(rp_, "rope")]

@d("coast", "made", "glass", "net_floats")
def _(cx, cy, a, rng):
    u = rng.uniform; ff = frame(cx, cy, a); L = u(4.4, 5.6); W = u(3.2, 4.0); gs = []
    for x in np.arange(-L / 2, L / 2 + 0.01, 1.1): gs.append(barl(ff(x, -W / 2), ff(x + u(-0.3, 0.3), W / 2), 0.26))
    for y in np.arange(-W / 2, W / 2 + 0.01, 1.1): gs.append(barl(ff(-L / 2, y), ff(L / 2, y + u(-0.3, 0.3)), 0.26))
    ps = ext(U(gs), 0.3)
    for i in range(rng.randint(2, 4)): c = ff(u(-L / 2 + 0.8, L / 2 - 0.8), u(-W / 2 + 0.8, W / 2 - 0.8)); r = u(0.6, 0.85); ps += dome(c[0], c[1], r, r * 1.4, n=12)
    return rect(cx, cy, L + 0.6, W + 0.6, a), [ST(ps, "glass")]

@d("coast", "debris", "drift", "driftwood")
def _(cx, cy, a, rng):
    u = rng.uniform; ff = frame(cx, cy, a); L = u(5.5, 8.0); pts = [ff(-L / 2, 0), ff(-L * 0.15, u(-0.6, 0.6)), ff(L * 0.2, u(-0.6, 0.6)), ff(L / 2, u(-1.0, 1.0))]
    ps = rod(pts, lambda q: 0.75 - 0.42 * q + 0.1 * math.sin(q * 11), 1.0); foot = [LineString(pts).buffer(0.9)]
    for i in range(rng.randint(2, 4)): t = a + 3.14 + u(-1.1, 1.1); e = pol(pts[0][0], pts[0][1], u(1.0, 1.9), t); ps += rod([pts[0], e], lambda q: 0.5 - 0.26 * q, 1.0); foot.append(disc(e[0], e[1], 0.3))
    return hull(foot), [ST(ps, "drift")]

@d("coast", "debris", "shell", "scallops")
def _(cx, cy, a, rng):
    u = rng.uniform; ps = []; cuts = []; foot = []
    for i in range(rng.randint(3, 5)):
        x, y = pol(cx, cy, 0 if i == 0 else u(1.9, 2.7), a + i * 1.5); R = u(0.9, 1.3); b = u(0, 6.28); h = R * 0.36
        if any(disc(x, y, R).distance(g) < 0.2 for g in foot): continue
        fan = Polygon([pol(x, y, 0.2, b + 3.14)] + arc(x, y, R, b - 1.25, b + 1.25, 12)).buffer(0.1); ps += ext(fan, 0.2) + [s for q in (0.85, 0.65, 0.42) for s in ext(fan.intersection(disc(*pol(x, y, R * (1 - q) * 0.4, b), R * q)), h * (1.1 - q) / 0.68 + 0.2)]
        cuts += [(barl(pol(x, y, R * 0.3, b + t), pol(x, y, R, b + t), 0.17), 0.24) for t in (-0.8, -0.4, 0.0, 0.4, 0.8)]; foot.append(disc(x, y, R))
    return hull(foot).buffer(0.2), [ST(ps, "shell", cuts=cuts)]

@d("coast", "growth", "shell", "urchins")
def _(cx, cy, a, rng):
    u = rng.uniform; ps = []; cuts = []; foot = []
    for i in range(rng.randint(2, 5)):
        x, y = pol(cx, cy, 0 if i == 0 else u(1.8, 2.6), a + i * 1.4); r = u(0.7, 1.1)
        if any(disc(x, y, r).distance(g) < 0.2 for g in foot): continue
        ps += fluted(x, y, r, r * 0.9, rng.randint(11, 15), u(0, 1), 0.55, 5); cuts.append((disc(x, y, 0.2, 6), r * 0.62)); foot.append(disc(x, y, r))
    return hull(foot).buffer(0.2), [ST(ps, "shell", cuts=cuts)]

@d("coast", "made", "iron", "bollard")
def _(cx, cy, a, rng):
    u = rng.uniform; r = u(0.9, 1.15); h = u(1.9, 2.5); e = pol(cx, cy, r + u(3.0, 5.0), a)
    ps = turn(cx, cy, [(r * 1.4, 0.0), (r * 1.4, 0.3), (r * 1.3, 0.3), (r * 1.3, h * 0.75)] + crv(r * 1.3, 0.3, h * 0.75, h), 16)
    rope_ = [s for k in range(rng.randint(1, 3)) for s in rod(arc(cx, cy, r * 1.3 + 0.36, a + 0.5 + k, a + 6.6 + k, 20), 0.34, 1.0, lift=0.3 + k * 0.55)] + rod(wiggle(pol(cx, cy, r * 1.3 + 0.36, a), e, rng, 0.8, 5), 0.34, 1.0)
    return hull([disc(cx, cy, r * 1.4 + 0.8), disc(e[0], e[1], 0.5)]), [ST(ps + rope_, "iron")]

@d("coast", "made", "drift", "oars")
def _(cx, cy, a, rng):
    u = rng.uniform; ps = []; foot = []
    for i in range(rng.randint(1, 2)):
        b = a + (i * u(0.3, 0.9)); f2 = frame(cx + i * u(-0.6, 0.6), cy + i * u(-0.6, 0.6), b); L = u(8.0, 10.0)
        ps += rod([f2(-L / 2, 0), f2(L / 2 - 2.4, 0)], lambda q: 0.25 + 0.12 * (q < 0.1), 1.2) + ext(Polygon([f2(L / 2 - 2.6, -0.3), f2(L / 2 - 1.6, -0.62), f2(L / 2, -0.55), f2(L / 2, 0.55), f2(L / 2 - 1.6, 0.62), f2(L / 2 - 2.6, 0.3)]), 0.34); foot.append(rect(*f2(0, 0), L, 1.5, b))
    return hull(foot), [ST(ps, "drift")]

@d("coast", "made", "drift", "ship_wheel")
def _(cx, cy, a, rng):
    u = rng.uniform; R = u(1.9, 2.4); n = rng.choice((6, 8)); gs = [ann(cx, cy, R, R - 0.6, 24), disc(cx, cy, 0.7, 12)]
    for k in range(n):
        if rng.random() < 0.12: continue
        t = a + 6.2832 * k / n; gs += [barl(pol(cx, cy, 0.4, t), pol(cx, cy, R + 0.75, t), 0.42), disc(*pol(cx, cy, R + 0.8, t), 0.3, 8)]
    g = U(gs)
    if rng.random() < 0.3: g = g.difference(Polygon([(cx, cy), pol(cx, cy, 2 * R, a + 1), pol(cx, cy, 2 * R, a + 1.6)]).difference(disc(cx, cy, 0.8)))
    return disc(cx, cy, R + 1.2), [ST(ext(g, 0.5) + cyl(cx, cy, 0.42, 0.8, n=8), "drift")]

@d("coast", "growth", "kelp", "kelp")
def _(cx, cy, a, rng):
    u = rng.uniform; ps = []; foot = []
    for i in range(rng.randint(2, 4)):
        t = a + (i - 1) * u(0.25, 0.6); L = u(5.0, 8.0); pts = wiggle((cx, cy), pol(cx, cy, L, t), rng, 0.9, 7); ps += rod(pts, lambda q: 0.5 * math.sin(max(0.08, min(0.95, q)) * math.pi) ** 0.4 + 0.1, 0.5); foot.append(LineString(pts).buffer(0.7))
        for p in pts[2:-1:2]: ps += dome(p[0] + u(-0.4, 0.4), p[1] + u(-0.4, 0.4), 0.34, 0.55, n=7)
    return hull(foot), [ST(ps + dome(cx, cy, 0.6, 0.6, n=8), "kelp")]

@d("coast", "marks", "pit", "crab_burrows")
def _(cx, cy, a, rng):
    u = rng.uniform; gs = []; ups = []; foot = []
    for i in range(rng.randint(2, 4)):
        x, y = pol(cx, cy, 0 if i == 0 else u(2.0, 3.0), a + i * 1.9); gs.append(disc(x, y, u(0.36, 0.5), 8)); foot.append(disc(x, y, 1.6)); t = u(0, 6.28)
        for k in range(rng.randint(4, 8)): p = pol(x, y, 0.8 + 0.3 * k, t + (k % 2 - 0.5) * 0.5); gs.append(disc(p[0], p[1], 0.17, 5))
        ups += [("mound", disc(*pol(x, y, u(0.9, 1.3), t + 3.14 + u(-1, 1)), 0.45, 8), 0.3, "pebble", PEB_T) for k in range(rng.randint(1, 3))]
    return hull(foot), [("gcut", U(gs), 0.4, "pit")] + [L_ for i, L_ in enumerate(ups) if all(L_[1].distance(o[1]) > 0.3 for o in ups[:i]) and L_[1].distance(U(gs)) > 0.3]

@d("coast", "growth", "glass", "jellyfish")
def _(cx, cy, a, rng):
    u = rng.uniform; R = u(1.5, 2.1); ps = turn(cx, cy, [(R, 0.0), (R * 0.9, 0.3), (R * 0.4, 0.55)], 20, u(0.85, 1.0), a); foot = [disc(cx, cy, R)]
    for k in range(rng.randint(3, 6)): t = a + u(-0.7, 0.7); e = pol(cx, cy, R + u(1.2, 2.6), t); ps += rod(wiggle(pol(cx, cy, R * 0.7, t), e, rng, 0.4, 4), lambda q: 0.26 - 0.05 * q, 0.8); foot.append(disc(e[0], e[1], 0.3))
    cuts = [(ann(*pol(cx, cy, R * 0.3, k * 1.5708 + a), R * 0.2, R * 0.2 - 0.13, 8), 0.42) for k in range(4)]
    return hull(foot), [ST(ps, "glass", cuts=cuts)]

@d("coast", "made", "drift", "fish_crate")
def _(cx, cy, a, rng):
    u = rng.uniform; ff = frame(cx, cy, a); L = u(3.6, 4.4); W = u(2.4, 2.9); h = u(0.9, 1.2); R = rect(cx, cy, L, W, a); ps = ext(R.difference(R.buffer(-0.42)), h) + ext(R, 0.35)
    fish = lambda c, b, Lf: spin(c[0], c[1], b, Lf, lambda t: 0.4 * math.sin(max(0.05, min(1.0, (t + Lf / 2) / (Lf * 0.8))) * math.pi) ** 0.7 + 0.12 * (t > Lf * 0.36) + 0.06, 0.9)
    for i in range(rng.randint(2, 3)): ps += [(lambda z, f=q[0]: f(max(0.0, z - 0.3)), q[1] + 0.3) for q in fish(ff(u(-0.3, 0.3), (i - 1) * 0.62), a + u(-0.15, 0.15), L - 1.3)]
    lay = [ST(ps, "drift")]
    if rng.random() < 0.6: lay.append(ST(fish(ff(u(-1, 1), W / 2 + 1.0), a + u(-0.5, 0.5), 2.6), "hide"))
    return rect(*ff(0, 0.6), L + 0.3, W + 2.2, a), lay

@d("coast", "made", "drift", "capstan")
def _(cx, cy, a, rng):
    u = rng.uniform; r = u(1.3, 1.6); h = u(1.7, 2.2); n = 6
    ps = turn(cx, cy, [(r * 1.25, 0.0), (r * 1.25, 0.3), (r * 0.9, 0.5), (r * 0.72, h * 0.6), (r, h * 0.75), (r, h), (r * 0.5, h + 0.2)], 18); foot = [disc(cx, cy, r * 1.3)]
    for k in rng.sample(range(n), rng.randint(1, 3)): t = a + k * 1.0472; e = pol(cx, cy, r + u(2.0, 3.0), t); ps += rod([pol(cx, cy, r * 0.5, t), e], 0.28, 1.0, lift=h * 0.78); foot.append(disc(e[0], e[1], 0.4))
    return hull(foot), [ST(ps, "drift")]

@d("coast", "made", "iron", "cannon")
def _(cx, cy, a, rng):
    u = rng.uniform; ff = frame(cx, cy, a); L = u(6.0, 7.5); r = u(0.85, 1.05)
    ps = spin(cx, cy, a, L, lambda t: r * (1 - 0.3 * (t + L / 2) / L) + (0.13 if min(abs(t + L * 0.3), abs(t - L * 0.1), abs(t - L / 2 + 0.25)) < 0.17 else 0.0), 1.05) + dome(*ff(-L / 2 - 0.3, 0), 0.5, 0.8, n=8) + [s for sv in (-1, 1) for s in lie(*ff(-L * 0.08, sv * (r + 0.2)), a + 1.5708, 0.7, 0.36)]
    lay = [ST(ps, "iron", cuts=[(disc(*ff(L / 2 - 0.25, 0), 0.3, 6), 0.3)])]
    for i in range(rng.randint(1, 3)): c = ff(u(-2, 2), -r - 1.2 - 0.0); lay.append(ST(dome(c[0] + i * 1.0 * math.cos(a), c[1] + i * 1.0 * math.sin(a), 0.48, 0.85, n=10), "iron"))
    return rect(*ff(-0.2, -0.5), L + 1.2, 2 * r + 2.6, a), lay[:2]

@d("coast", "made", "brass", "spyglass_compass")
def _(cx, cy, a, rng):
    u = rng.uniform; ff = frame(cx, cy, a); L = u(4.0, 5.0); c = ff(u(-1, 1), 2.0); r = u(0.85, 1.05); nd = u(0, 3.14)
    ps = spin(cx, cy, a, L, lambda t: 0.3 if t < -L * 0.17 else 0.4 if t < L * 0.17 else 0.5, 1.1) + lie(*ff(L / 2 - 0.15, 0), a, 0.3, 0.58)
    cp = turn(c[0], c[1], [(r, 0.0), (r, 0.5), (r - 0.24, 0.5), (r - 0.26, 0.34), (0.25, 0.36)], 16)
    return hull([rect(cx, cy, L, 1.3, a), disc(c[0], c[1], r + 0.2)]), [ST(ps, "brass"), ST(cp + ext(tri(pol(c[0], c[1], 0.1, nd + 3.14), pol(c[0], c[1], r - 0.36, nd), 0.3).union(tri(pol(c[0], c[1], 0.1, nd), pol(c[0], c[1], r - 0.36, nd + 3.14), 0.3)), 0.48), "brass")]

@d("coast", "made", "bone", "whale_vertebra")
def _(cx, cy, a, rng):
    u = rng.uniform; ff = frame(cx, cy, a); r = u(1.3, 1.7); h = u(1.0, 1.4)
    ps = turn(cx, cy, [(r, 0.0), (r * 0.9, h * 0.5), (r, h)], 14) + [s for sv in (-1, 1) for s in spin(*ff(0, sv * (r + 0.9)), a + 1.5708, 2.2, lambda t: 0.5 - 0.22 * abs(t), 1.0, prof="ridge")] + spin(*ff(-r - 0.7, 0), a, 1.9, lambda t: 0.36 + 0.14 * t, 1.6, prof="ridge")
    return rect(*ff(-0.5, 0), 2 * r + 2.2, 2 * r + 4.0, a), [ST(ps, "bone", cuts=[(disc(cx, cy, r * 0.55, 10), h - 0.25)])]

@d("coast", "made", "sandstone", "sand_castle")
def _(cx, cy, a, rng):
    u = rng.uniform; ff = frame(cx, cy, a); S = u(2.6, 3.4); ps = blk(cx, cy, S, S, a, 0.75); cuts = [(rect(*ff(S / 2, 0), 0.6, 0.6, a), 0.0)]
    for sx in (-1, 1):
        for sy in (-1, 1):
            if rng.random() < 0.15: ps += dome(*ff(sx * S / 2, sy * S / 2), 0.8, 0.5, n=9); continue                               # a tower already slumped
            c = ff(sx * S / 2, sy * S / 2); r = u(0.62, 0.8); h = u(1.3, 2.0); ps += turn(c[0], c[1], [(r * 1.1, 0.0), (r, h * 0.8), (r, h)], 12); cuts += [(barl(pol(c[0], c[1], r, t), pol(c[0], c[1], r, t + 3.14), 0.3), h - 0.3) for t in (a, a + 1.5708)]
    bk = ff(u(-1, 1), S / 2 + 2.0); lay = [ST(ps, "sandstone", cuts=cuts), ST(turn(bk[0], bk[1], [(0.75, 0.0), (0.55, 1.0)], 12), "tin")]
    return hull([rect(cx, cy, S + 1.8, S + 1.8, a), disc(bk[0], bk[1], 0.9)]), lay


_OWN = set(globals())
