"""Third drawing, badlands: the objects at their real size beside a 32 mm figure (1 m = 18 mm as printed), each at
the height of the real thing: prickly pear pads standing on edge in tiers, a saddle with its horn and cantle up
off the blanket, posts a metre tall. What would overhang (a rail on its posts, a cactus arm) lies fallen."""
import math
from sd_reg import d

def xon(o, g, par, h, kind=None):
    """a part standing on layer par as a plain "on" layer: at true scale it keeps parts down to a quarter of a
    millimetre wide, where Obj.on (through ST()) drops anything under 0.3"""
    if g is None or g.is_empty: return None
    o.L.append(("on", g, par, h, kind or o.k)); return len(o.L) - 1

def xstuds(o, pts, par, r=0.2, h=0.1, kind=None, n=8):
    return xon(o, U([disc(x, y, r, n) for x, y in pts]), par, h, kind) if pts else None

def xb_pad(cx, cy, t, Lp, Hp, th, z0):
    """a prickly-pear pad standing on edge: an upright oval Lp long and Hp tall, th thick, its foot at z0"""
    n = 15; ps = []; f = frame(cx, cy, t)
    for i in range(n):
        s0 = -Lp / 2 + Lp * i / n; s1 = s0 + Lp / n; s = (s0 + s1) / 2; e = math.sqrt(max(0.0, 1 - (2 * s / Lp) ** 2))
        ps += ext(rect(*f(s, 0), s1 - s0 + 0.03, max(0.32, th * (0.7 + 0.3 * e)), t), z0 + Hp * max(0.25, e ** 0.85))
    return ps

def xb_grain(f2, L, W, rng, n=2, w=0.12):
    gs = []
    for k in range(n):
        v = (k + 0.5 - n / 2) * W / (n + 0.4) + rng.uniform(-0.05, 0.05); x0 = rng.uniform(-L / 2, -L * 0.1); x1 = rng.uniform(L * 0.1, L / 2)
        gs.append(barl(f2(x0, v), f2(x1, v + rng.uniform(-0.06, 0.06)), w))
    return U(gs)


@d("badlands", "growth", "cactus", "saguaro")
def _(cx, cy, a, rng):
    """a saguaro come down: the trunk lying full length with its pleated ribs along it, an arm or two lying out to
    the side, the rounded crown at one end and at the root end the woody ribs of the skeleton sticking out of the
    rotted stump; on some a young one still stands beside it, ribbed, with buds round its crown"""
    u = rng.uniform; o = Obj("cactus"); ff = frame(cx, cy, a); L = u(4.6, 5.6); r = u(0.62, 0.76); hk = 1.1
    rf = lambda t: r * (1.0 if t < L / 2 - r else math.sqrt(max(0.03, 1 - ((t - L / 2 + r) / r) ** 2))) * (0.9 if t < -L / 2 + 0.3 else 1.0)
    lf = r * 0.7; ps = spin(cx, cy, a, L, rf, hk, lift=lf); hz = lambda v: lf + hk * 0.93 * math.sqrt(max(0.0, r * r - v * v))
    cuts = [(barl(ff(-L / 2 + 0.25, v), ff(L / 2 - r * 0.9, v), 0.17), hz(abs(v) + 0.09) - 0.12) for v in (-0.62 * r, -0.21 * r, 0.21 * r, 0.62 * r)]
    for sv in rng.sample((-1, 1), rng.randint(1, 2)):
        p = ff(u(-L * 0.15, L * 0.25), sv * r * 0.5); e = pol(p[0], p[1], u(1.4, 2.0), a + sv * u(0.7, 1.2)); ra = r * 0.66
        ps += rod([p, e], ra, 1.05, lift=ra * 0.6) + dome(e[0], e[1], ra, ra * 1.55)
    o.st(ps, cuts=cuts, foot=rect(cx, cy, L, 2 * r, a))
    rb = [barl(ff(-L / 2 + 0.2, v), ff(-L / 2 - u(0.6, 1.1), v + u(-0.15, 0.15)), 0.3) for v in np.linspace(-r * 0.7, r * 0.7, 4)]
    o.st(ext(U(rb), r * 1.3), "wood", foot=rect(*ff(-L / 2 - 0.5, 0), 1.2, 2 * r, a))
    if rng.random() < 0.45:          # a young one standing by the root
        c = ff(-L / 2 + u(0.3, 1.0), rng.choice((-1, 1)) * (r + u(0.9, 1.2))); ry = u(0.45, 0.55); hy = u(2.4, 3.2); n = rng.randint(9, 11)
        o.st(ext(star(c[0], c[1], ry, ry * 0.78, n, a), hy - ry * 0.8) + fluted(c[0], c[1], ry, ry * 0.8, n, a, 0.78, 5, lift=hy - ry * 0.8), foot=disc(c[0], c[1], ry))
        o.st([s for k in range(rng.randint(2, 4)) for s in dome(*pol(c[0], c[1], ry * 0.55, k * 1.9 + u(0, 1)), 0.17, hy + 0.12, n=8)], "flower", foot=disc(c[0], c[1], ry))
    for i in range(rng.randint(1, 3)): c = ff(-L / 2 - u(0.4, 1.0), u(-1.0, 1.0)); o.st(rock(c[0], c[1], u(0.3, 0.42), u(0.28, 0.45), rng))
    return o.out()

@d("badlands", "made", "wood", "water_trough")
def _(cx, cy, a, rng):
    """a plank stock trough on two cross skids: thick plank walls, iron bands strapped over the rim and riveted,
    corner irons, water standing in it with a ring of ripples; a wooden bucket by one end, and at the other the
    iron hand pump with its spout over the trough or a wet trampled patch with hoof prints in it"""
    u = rng.uniform; ff = frame(cx, cy, a); L = u(4.4, 5.4); W = u(1.7, 2.0); h = u(1.4, 1.65); o = Obj("wood"); g = rect(cx, cy, L, W, a); t = 0.34
    sk = o.up(U([rect(*ff(sx * L * 0.3, 0), 0.62, W + 1.0, a) for sx in (-1, 1)]), 0.42); o.cut(U([barl(ff(sx * L * 0.3 - 0.4, sv * (W / 2 + 0.3)), ff(sx * L * 0.3 + 0.4, sv * (W / 2 + 0.3)), 0.12) for sx in (-1, 1) for sv in (-1, 1)]), sk, 0.08)
    wl = o.up(g.difference(inset(g, t)), h)
    o.cut(U([barl(ff(x, -W), ff(x + 0.01, W), 0.13) for x in (-L * u(0.1, 0.2), L * u(0.15, 0.3))] + [barl(ff(sx * (L / 2 - t), -W), ff(sx * (L / 2 - t), W), 0.13) for sx in (-1, 1)]), wl, 0.09)
    wa = o.up(inset(g, t).buffer(0.02), h - u(0.3, 0.45), "water"); cw = ff(u(-L * 0.2, L * 0.2), 0)
    o.cut(U([ann(cw[0], cw[1], rr, rr - 0.12, 20) for rr in (0.4, 0.75)]).intersection(inset(g, t + 0.15)), wa, 0.06)
    xb = [-L * 0.36, 0.0, L * 0.36]; bd = xon(o, U([rect(*ff(x, 0), 0.4, W + 0.1, a) for x in xb]).intersection(g.difference(inset(g, t))), wl, 0.09, "iron")
    xstuds(o, [ff(x, sv * (W / 2 - t / 2)) for x in xb for sv in (-1, 1)], bd, 0.13, 0.06, "iron", 6)
    xon(o, U([Polygon([ff(sx * L / 2, sv * W / 2), ff(sx * (L / 2 - 0.6), sv * W / 2), ff(sx * (L / 2 - 0.6), sv * (W / 2 - t)), ff(sx * (L / 2 - t), sv * (W / 2 - t)), ff(sx * (L / 2 - t), sv * (W / 2 - 0.6)), ff(sx * L / 2, sv * (W / 2 - 0.6))]) for sx in (-1, 1) for sv in (-1, 1)]), wl, 0.08, "iron")
    c = ff(L / 2 + u(1.1, 1.5), u(-0.7, 0.7)); br = 0.62; bk = o.up(ann(c[0], c[1], br, br - 0.16, 16), u(1.0, 1.15)); o.up(disc(c[0], c[1], br - 0.14, 14), 0.5, "water")
    o.st(ext(line(arc(c[0], c[1], br + 0.28, a + 0.3, a + math.pi - 0.3, 10), 0.3), 0.22), "iron", foot=disc(c[0], c[1], br + 0.5))
    if rng.random() < 0.5:          # the hand pump at the far end
        p = ff(-L / 2 - 0.45, u(-0.3, 0.3)); pb = o.up(disc(p[0], p[1], 0.42, 14), u(2.3, 2.6), "iron"); o.st(dome(p[0], p[1], 0.36, 0.3), "iron", par=pb)
        p_v = (p[0] - cx) * -math.sin(a) + (p[1] - cy) * math.cos(a); o.up(rect(*ff(-L / 2 - 0.1, p_v), 0.8, 0.32, a), h + 0.35, "iron")
        hd = ff(-L / 2 - 1.3, p_v + u(-0.3, 0.3)); o.st(ext(capl(pol(p[0], p[1], 0.3, math.atan2(hd[1] - p[1], hd[0] - p[0])), hd, 0.3), 0.3), "iron", foot=disc(hd[0], hd[1], 0.5))
    else:
        o.gcut(blob(*ff(-L / 2 - 0.8, u(-0.5, 0.5)), u(0.8, 1.05), rng, 3), 0.25, "water")
    hp = [ff(u(-L / 2, L / 2), sv * (W / 2 + u(0.5, 0.9))) for sv in (-1, 1) for _ in range(rng.randint(1, 2))]
    o.gcut(U([ann(x, y, 0.3, 0.14, 10).difference(rect(*pol(x, y, 0.25, a + 1.57), 0.3, 0.3, a)) for x, y in hp]), 0.12, "earth")
    return o.out()

@d("badlands", "made", "wood", "hitching_rail")
def _(cx, cy, a, rng):
    """a hitching rail come down: two hewn posts still standing a metre high, chamfered at the head with the seat cut
    for the rail and the checks in the end grain; the rail lies between them with its tie rings on their staples
    and the nails still in its ends, a lead rope run off from one ring, a coiled lariat, droppings in the dust"""
    u = rng.uniform; ff = frame(cx, cy, a); L = u(5.0, 6.3); o = Obj("wood")
    for sx in (-1, 1):
        p = ff(sx * L / 2, 0); t = a + u(-0.15, 0.15); b = o.up(rect(p[0], p[1], 0.62, 0.62, t), u(2.6, 3.0)); ch = xon(o, rect(p[0], p[1], 0.42, 0.42, t), b, 0.12)
        o.cut(barl(pol(p[0], p[1], 0.5, t + math.pi), pol(p[0], p[1], 0.5, t), 0.3), b, 0.22)
        o.cut(U([barl(pol(p[0], p[1], 0.05, t + k), pol(p[0], p[1], 0.3, t + k), 0.1) for k in (0.8, 2.6, 4.4)]), b, 0.1)
    sv = rng.choice((-1, 1)); c = ff(u(-0.4, 0.4), sv * u(1.0, 1.3)); t = a + u(-0.2, 0.2); f2 = frame(c[0], c[1], t); Lr = L * u(0.82, 0.95); rr = 0.36
    o.st(lie(c[0], c[1], t, Lr, rr), cuts=[(barl(f2(-Lr * 0.42, sv2 * 0.12), f2(Lr * 0.42, sv2 * 0.1), 0.16), rr * 0.95) for sv2 in (-1, 1)], foot=rect(c[0], c[1], Lr, 2 * rr, t))
    rings = [f2(x, -sv * (rr + 0.5)) for x in (-Lr * 0.25, Lr * 0.22)]
    o.st(ext(U([ann(x, y, 0.42, 0.2, 12) for x, y in rings] + [rect(*f2(xx, -sv * rr * 0.9), 0.3, 0.42, t) for xx in (-Lr * 0.25, Lr * 0.22)]), 0.26), "iron", foot=U([disc(x, y, 0.45) for x, y in rings]))
    q = rings[rng.randint(0, 1)]; e = pol(q[0], q[1], u(1.6, 2.4), a - sv * 1.57 + u(-0.6, 0.6)); pts = wiggle(pol(q[0], q[1], 0.4, a - sv * 1.57), e, rng, 0.4, 3)
    o.st(rod(pts, 0.25, 1.1), "rope", foot=LineString(pts).buffer(0.3))
    p = ff(rng.choice((-1, 1)) * L / 2 + u(-0.3, 0.3), -sv * u(1.4, 1.8)); o.st(rod(spiral(p[0], p[1], 0.3, 0.95, 2.4, u(0, 6.28), 34), 0.25, 1.0), "rope", foot=disc(p[0], p[1], 1.1))
    for i in range(rng.randint(0, 2)): q = ff(u(-L / 3, L / 3), -sv * u(0.6, 1.6)); o.st([s for k in range(3) for s in dome(*pol(q[0], q[1], 0.22, k * 2.1), 0.22, 0.24)], "earth", foot=disc(q[0], q[1], 0.4))
    return o.out([rect(cx, cy, L + 1.4, 4.4, a)])

@d("badlands", "growth", "cactus", "prickly_pear")
def _(cx, cy, a, rng):
    """a prickly pear clump knee to waist high: thick oval pads standing on edge round a woody stem, more pads
    growing from the rims of the first and smaller ones again on those, red fruit along the top edges of the
    highest, and an old pad fallen flat on the ground with its spine dimples"""
    u = rng.uniform; o = Obj("cactus"); th = u(0.4, 0.46)
    o.st(cyl(cx, cy, 0.36, 0.7, n=8), "wood", foot=disc(cx, cy, 0.4))
    pads = []; t0 = u(0, 6.28)
    for k in range(rng.randint(3, 4)):
        t = t0 + k * 6.2832 / 3.5 + u(-0.3, 0.3); c = pol(cx, cy, u(0.55, 0.85), t); Lp = u(1.2, 1.5); Hp = u(1.5, 1.8)
        pads.append((c, t + 1.5708 + u(-0.5, 0.5), Lp, Hp, 0.0))
    for tier in (1, 2):
        base = [p for p in pads if (tier == 1 and p[4] == 0.0) or (tier == 2 and p[4] > 0.0)]
        for p in rng.sample(base, min(len(base), rng.randint(1, 3) if tier == 1 else rng.randint(0, 2))):
            (x, y), t, Lp, Hp, z0 = p; s = u(-0.25, 0.25) * Lp; e = math.sqrt(max(0.0, 1 - (2 * s / Lp) ** 2)); zt = z0 + Hp * max(0.25, e ** 0.85)
            c = pol(*pol(x, y, s, t), u(0.15, 0.35), t + rng.choice((-1, 1)) * 1.5708); pads.append((c, t + u(-0.7, 0.7), Lp * u(0.75, 0.9), Hp * u(0.75, 0.9), zt - u(0.35, 0.5)))
    top = max(pads, key=lambda p: p[4] + p[3])
    for (x, y), t, Lp, Hp, z0 in pads:
        o.st(xb_pad(x, y, t, Lp, Hp, th, z0), foot=rect(x, y, Lp, th, t))
    for (x, y), t, Lp, Hp, z0 in sorted(pads, key=lambda p: -(p[4] + p[3]))[:2]:
        fr = [pol(x, y, s * Lp, t) for s in (-0.22, 0.05, 0.25) if rng.random() < 0.75]
        if fr: o.st([q for f in fr for q in dome(f[0], f[1], 0.22, z0 + Hp * max(0.25, math.sqrt(max(0.0, 1 - (2 * ((f[0] - x) * math.cos(t) + (f[1] - y) * math.sin(t)) / Lp) ** 2)) ** 0.85) + 0.3, n=10)], "flower", foot=U([disc(f[0], f[1], 0.22) for f in fr]))
    c = pol(cx, cy, u(2.0, 2.4), t0 + u(2.5, 3.5)); t = u(0, 3.14)
    o.st(dome(c[0], c[1], 0.7, 0.34, sq=0.65, a=t), cuts=[(disc(*pol(c[0], c[1], 0.3, t + k * 3.14), 0.1, 6), 0.24) for k in range(2)], foot=disc(c[0], c[1], 0.7))
    return o.out()

@d("badlands", "made", "wood", "ox_yoke")
def _(cx, cy, a, rng):
    """an ox yoke lying flat: the hewn beam rounded over its back and dipping over each neck, grain along it, the two
    bent bows lying as open loops with their keys through the beam (one may be gone), and the iron staple and ring
    in the middle with a length of chain run out from it"""
    u = rng.uniform; ff = frame(cx, cy, a); o = Obj("wood"); X = u(1.4, 1.6)
    pts = [ff(-2 * X, 0.35), ff(-1.5 * X, 0.1), ff(-X, -0.25), ff(-0.5 * X, 0.0), ff(0, 0.15), ff(0.5 * X, 0.0), ff(X, -0.25), ff(1.5 * X, 0.1), ff(2 * X, 0.35)]
    bg = line(pts, 1.0); b = o.up(bg, u(0.5, 0.6)); cr = xon(o, inset(bg, 0.2), b, 0.14); xon(o, inset(bg, 0.36), cr, 0.08)
    o.cut(U([barl(ff(-1.9 * X, v), ff(1.9 * X, v + 0.02), 0.11) for v in (0.08,)]).intersection(inset(bg, 0.1)), cr, 0.08)
    gone = rng.choice((-1, 1)) if rng.random() < 0.3 else 0
    for sx in (-1, 1):
        if sx == gone: o.cut(U([disc(*ff(sx * X + k * 0.75, -0.2), 0.16, 8) for k in (-1, 1)]), cr, 0.3); continue
        c = ff(sx * X, -0.45); o.st(rod(arc(c[0], c[1], 0.78, a + math.pi + 0.05, a + 2 * math.pi - 0.05, 14), 0.26, 1.2), foot=disc(c[0], c[1], 1.1))
        xon(o, U([rect(*ff(sx * X + k * 0.75, -0.2), 0.5, 0.2, a) for k in (-1, 1)]), cr, 0.16, "iron")
    st_ = xon(o, rect(*ff(0, 0.3), 0.5, 0.36, a), cr, 0.1, "iron"); c = ff(0, 1.12)
    o.st(ext(ann(c[0], c[1], 0.58, 0.3, 14), 0.3), "iron", foot=disc(c[0], c[1], 0.6))
    e = pol(c[0], c[1], u(1.5, 2.0), a + rng.choice((0.0, math.pi)) + rng.choice((-1, 1)) * u(0.3, 0.6)); n = 5; ls = []
    for k in range(n):
        p = (c[0] + (e[0] - c[0]) * (k + 0.8) / n, c[1] + (e[1] - c[1]) * (k + 0.8) / n); tt = math.atan2(e[1] - c[1], e[0] - c[0])
        ls.append(ell(p[0], p[1], 0.3, 0.2, tt, 10).exterior.buffer(0.08) if k % 2 == 0 else barl(pol(p[0], p[1], 0.24, tt), pol(p[0], p[1], 0.24, tt + math.pi), 0.2))
    o.st(ext(U(ls), 0.22), "iron", foot=LineString([c, e]).buffer(0.4))
    return o.out()

@d("badlands", "made", "cloth", "bedroll_canteen")
def _(cx, cy, a, rng):
    """a cowhand's bedroll: the blanket rolled tight in its canvas with the outer edge lying along it and the striped
    blanket showing at the ends, two leather straps with their buckles; beside it a flat round canteen in its felt
    cover with the stitched cross, the spout and cork and its sling, and a tin cup"""
    u = rng.uniform; ff = frame(cx, cy, a); L = u(3.0, 3.6); r = u(0.75, 0.9); o = Obj("cloth"); hk = 1.15
    ps = lie(cx, cy, a, L, r) + [s for x in (-L * 0.28, L * 0.28) for s in lie(*ff(x, 0), a, 0.42, r + 0.1)]
    hz = lambda v: 1.06 * math.sqrt(max(0.0, r * r - v * v))
    cuts = [(barl(ff(-L / 2 + 0.1, r * 0.42), ff(L / 2 - 0.1, r * 0.42), 0.16), hz(r * 0.42) - 0.12)] + [(barl(ff(sx * x, -r), ff(sx * x, r), 0.16), hz(0.0) - 0.1) for sx in (-1, 1) for x in (L / 2 - 0.25, L / 2 - 0.5)]
    b = o.st(ps, cuts=cuts, foot=rect(cx, cy, L, 2 * r, a))
    o.st(ext(U([rect(*ff(x, 0), 0.5, 0.42, a) for x in (-L * 0.28, L * 0.28)]), r * 1.2 + 0.16), "brass", foot=disc(cx, cy, 0.5))
    sv = rng.choice((-1, 1)); c = ff(u(-0.6, 0.6), sv * (r + u(0.75, 0.9))); R = u(0.6, 0.7); t = u(0, 6.28)
    cn = o.up(disc(c[0], c[1], R, 20), 0.42, "tin"); fc = xon(o, disc(c[0], c[1], R - 0.12, 20), cn, 0.06, "sack"); o.cut(U([barl(pol(c[0], c[1], R, t + k * 1.5708), pol(c[0], c[1], R, t + k * 1.5708 + math.pi), 0.12) for k in range(2)]), fc, 0.05)
    sp = pol(c[0], c[1], R + 0.2, t + 0.785); o.st(cyl(sp[0], sp[1], 0.22, 0.32, n=10), "tin", foot=disc(sp[0], sp[1], 0.25))
    pts = [pol(c[0], c[1], R, t + 0.785 + 0.6)] + arc(c[0], c[1], R + u(0.35, 0.5), t + 0.785 + 0.8, t + 0.785 + 5.4, 12) + [pol(c[0], c[1], R, t + 0.785 - 0.6)]
    o.st(ext(line(pts, 0.28), 0.14), "leather", foot=LineString(pts).buffer(0.3))
    q = ff(u(-0.9, 0.9), -sv * (r + u(0.5, 0.6))); cp = o.up(ann(q[0], q[1], 0.36, 0.22, 14), 0.48, "tin"); o.up(disc(q[0], q[1], 0.24, 12), 0.08, "tin")
    o.st(ext(ann(*pol(q[0], q[1], 0.48, t), 0.2, 0.06, 8), 0.34), "tin", foot=disc(q[0], q[1], 0.6))
    return o.out()

@d("badlands", "made", "leather", "saddle")
def _(cx, cy, a, rng):
    """a stock saddle set down on its striped blanket: the skirts with their tooled border and conchos, the seat
    dished between the swell and the cantle rising behind it, the horn standing up with its cap, the fenders lying
    out to each side with their stitching and a wooden stirrup at the end of each, the cinch ring behind"""
    u = rng.uniform; ff = frame(cx, cy, a); o = Obj("leather"); Ls = u(3.2, 3.6); Ws = u(2.5, 2.8)
    bl = rect(*ff(u(-0.1, 0.1), 0), Ls + u(0.5, 0.8), Ws + 0.5, a + u(-0.08, 0.08)); bb = o.up(bl, 0.16, "cloth")
    o.cut(U([barl(ff(x, -Ws), ff(x + 0.01, Ws), 0.14) for x in (-Ls / 2 - 0.1, Ls / 2 + 0.1)]), bb, 0.08)
    g = rrect(cx, cy, Ls, Ws, a, 0.9); sk = o.up(g, 0.4); o.groove(inset(g, 0.26), sk, 0.13, 0.08)
    xstuds(o, [ff(sx * (Ls / 2 - 0.5), sy * (Ws / 2 - 0.45)) for sx in (-1, 1) for sy in (-1, 1)], sk, 0.2, 0.08, "brass", 10)
    body = spin(cx, cy, a, Ls * 0.82, lambda t: 0.85 * math.sqrt(max(0.05, 1 - (t / (Ls * 0.41)) ** 2)), 1.0)
    body += dome(*ff(Ls * 0.2, 0), 0.75, 1.5, sq=0.9, a=a)
    c = ff(-Ls * 0.12, 0); body += ext(line(arc(c[0], c[1], 0.8, a + 2.1, a + 4.18, 10), 0.42), 1.6)
    st_ = o.st(body, par=sk, cuts=[(ell(*ff(-Ls * 0.02, 0), 0.55, 0.42, a, 14), 0.62)])
    hx = ff(Ls * 0.3, 0); o.st(turn(hx[0], hx[1], [(0.4, 0.0), (0.3, 1.5), (0.27, 1.95), (0.36, 2.0), (0.36, 2.2)], 12), par=sk)
    for sv in (-1, 1):
        f2 = frame(*ff(u(-0.1, 0.3), sv * (Ws / 2 + 0.8)), a + sv * u(-0.25, 0.25)); fg = rrect(*f2(0, 0), 1.3, 1.9, a, 0.3)
        fd = o.up(fg, 0.22); o.groove(inset(fg, 0.2), fd, 0.12, 0.07)
        sc = f2(u(-0.2, 0.2), sv * 1.55); sg = rrect(sc[0], sc[1], 1.0, 0.75, a, 0.15)
        s1 = o.up(sg.difference(rrect(sc[0], sc[1], 0.55, 0.32, a, 0.1)), 0.42, "wood")
    rc = ff(-Ls / 2 - 0.3, u(-0.5, 0.5)); o.st(ext(ann(rc[0], rc[1], 0.4, 0.2, 12), 0.22), "iron", foot=disc(rc[0], rc[1], 0.45))
    return o.out()


_OWN = set(globals())
