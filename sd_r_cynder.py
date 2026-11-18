"""Second drawing, cynder (see sd_r.py for the rule). The made things of the forge (anvil, crucible, ingots, bellows,
shackles, trough) are built crisp with Obj, each with what makes it that thing: the horn and hardy hole of an anvil,
the pouring lip of a crucible, the pleats of a bellows. The rock and lava are ST() shapes or plain upright plates.
Outlines given to a plain "up" layer are kept convex (a dented outline loses its body); anything else goes through
ST(ext(...))."""
import math
from sd_reg import d

def _facet(poly, ax, ay, h, n=10, top=0.22):
    """a faceted spire: the outline drawn in towards the point (ax, ay) as it rises, so its sides are flat faces"""
    return [s for k in range(n) for s in ext(affinity.scale(poly, 1 - (1 - top) * k / (n - 1), 1 - (1 - top) * k / (n - 1), origin=(ax, ay)), h * (k + 1) / n)]

def _curl(p, t, L, bend, n=10):
    """points of a line from p, setting off towards t and bending steadily by `bend` radians over its length L"""
    pts = [p]
    for k in range(n): t2 = t + bend * (k + 0.5) / n; pts.append((pts[-1][0] + L / n * math.cos(t2), pts[-1][1] + L / n * math.sin(t2)))
    return pts


@d("cynder", "marks", "lava", "skylight", ground=True)
def _(cx, cy, a, rng):
    """a skylight: the roof of a lava tube fallen in, the glow showing below; round the hole the crust stands broken
    into plates, each lifted towards the edge it broke along, a fallen piece or two lying in the lava"""
    u = rng.uniform; p = blob(cx, cy, u(1.5, 2.1), rng, 4); o = Obj("basalt"); n = rng.randint(6, 9); c = p.centroid; ts = [a + 6.2832 * k / n + u(-0.25, 0.25) for k in range(n)]
    o.gcut(p, u(0.6, 0.85), "lava"); ring = p.buffer(u(1.3, 1.6), 3).difference(p.buffer(0.06)); slits = U([barl((c.x, c.y), pol(c.x, c.y, 9, t), 0.34) for t in ts])
    for g in polys_of(ring.difference(slits)):
        if g.area < 0.5 or rng.random() < 0.12: continue
        h = u(0.4, 0.8); o.st(ext(g, h * 0.55) + ext(g.intersection(p.buffer(u(0.6, 0.9), 3)), h), foot=g)
    for i in range(rng.randint(0, 2)): q = pol(c.x, c.y, u(0.0, 0.7), u(0, 6.28)); o.st(ext(stone(q[0], q[1], u(0.5, 0.7), rng), 0.3), foot=disc(q[0], q[1], 0.6))
    return o.out()

@d("cynder", "debris", "basalt", "basalt_columns")
def _(cx, cy, a, rng):
    """a stand of basalt columns: six-sided pillars packed side by side, each cut off at its own height, the tall
    ones in the middle, dished tops on some, broken stubs at the edge"""
    u = rng.uniform; r = u(0.68, 0.85); s = r * 1.74; o = Obj("basalt"); n = 0; R = u(2.0, 2.8)
    for i in range(-3, 4):
        for j in range(-3, 4):
            p = pol(cx, cy, math.hypot((i + 0.5 * (j % 2)) * s, j * s * 0.866), a + math.atan2(j * s * 0.866, (i + 0.5 * (j % 2)) * s)); dd = math.dist(p, (cx, cy))
            if dd > R + u(-0.5, 0.3): continue
            h = max(0.35, round(u(1.3, 3.2) * (1 - dd / (R * 1.5)) / 0.35) * 0.35 + u(-0.05, 0.05)); b = o.up(ngon(p[0], p[1], r - 0.13, 6, a + 0.5236), h); n += 1
            if rng.random() < 0.35: o.cut(ngon(p[0], p[1], r * 0.5, 6, a + 0.5236), b, 0.16)
            elif rng.random() < 0.3: t = u(0, 6.28); o.cut(Polygon([pol(p[0], p[1], 2 * r, t - 1.2), pol(p[0], p[1], 0.1, t + 3.14), pol(p[0], p[1], 2 * r, t + 1.2)]).intersection(ngon(p[0], p[1], r - 0.13, 6, a + 0.5236)), b, min(0.3, h * 0.5))
    if not n: return None
    return o.out()

@d("cynder", "debris", "basalt", "ropy_lava")
def _(cx, cy, a, rng):
    """ropy lava: a tongue of flow frozen with its skin pushed up into ropes, each one bowed in the direction it ran,
    packed one behind the next from the snout back"""
    u = rng.uniform; ff = frame(cx, cy, a); n = rng.randint(5, 7); st = u(0.9, 1.0); W = u(1.9, 2.6); L = n * st; R0_ = u(1.8, 2.6); ps = []; ends = []; sk = u(-0.3, 0.3)
    ps += spin(cx, cy, a, L + 0.6, lambda t: W * math.sin(math.pi * min(1.0, max(0.0, (t + L / 2 + 0.3) / (L + 0.6))) ** 0.8) ** 0.5 + 0.02, 0.2)
    for k in range(n):
        x = -L / 2 + (k + 0.5) * st; wl = W * math.sin(math.pi * ((k + 0.5) / n) ** 0.8) ** 0.5 * 0.92; Rk = max(R0_, wl * 1.05); half = math.asin(min(1.0, wl / Rk)); c = ff(x - Rk, sk * (k - n / 2) * 0.2)
        pts = arc(c[0], c[1], Rk, a - half, a + half, 12); ps += rod(pts, lambda q: 0.4 - 0.1 * abs(2 * q - 1) ** 2, 1.1, lift=0.12); ends += [disc(*pts[0], 0.5), disc(*pts[-1], 0.5)]
    return hull(ends), [ST(ps, "basalt")]

@d("cynder", "debris", "ember", "fumarole")
def _(cx, cy, a, rng):
    """a fumarole: a vent built up out of its own crust in terraces, one inside the next, the open throat at the top,
    sulphur crystals crowding the rim, a smaller blow hole or two beside it"""
    u = rng.uniform; r = u(1.7, 2.3); o = Obj("basalt"); g = stone(cx, cy, r, rng, u(0.8, 0.95), 9); c = pol(cx, cy, r * u(0.0, 0.2), u(0, 6.28)); m = rng.randint(3, 4)
    b = o.up(g, u(0.3, 0.4)); k = 1.0
    for i in range(m): k *= u(0.7, 0.8); gi = affinity.rotate(affinity.scale(g, k, k, origin=c), u(-0.3, 0.3), origin=c, use_radians=True); b = o.on(gi, b, u(0.26, 0.4))
    rt = max(0.42, r * k * 0.5); o.cut(disc(c[0], c[1], rt, 10), b, 0.5)
    for i in range(rng.randint(4, 7)): q = pol(c[0], c[1], r * u(0.5, 0.95), u(0, 6.28)); o.st(_facet(ngon(q[0], q[1], u(0.36, 0.5), 4, u(0, 1.5)), q[0], q[1], u(0.6, 1.1), 6, 0.5), "ember", foot=disc(q[0], q[1], 0.4))
    for i in range(rng.randint(1, 2)):
        q = pol(cx, cy, r + u(0.9, 1.4), u(0, 6.28)); rr = u(0.36, 0.48); o.st(ext(ann(q[0], q[1], rr + 0.42, rr, 10), u(0.3, 0.4)), foot=disc(q[0], q[1], rr + 0.4)); o.gcut(disc(q[0], q[1], rr - 0.03, 10), 0.5, "pit")
    return o.out()

@d("cynder", "debris", "basalt", "lava_bomb", ground=True)
def _(cx, cy, a, rng):
    """a lava bomb where it came down: a spindle of rock with twisted ends, its crust cracked open like a loaf, lying
    in the dent it made, the thrown-up lip on the far side and the splash of small stones beyond"""
    u = rng.uniform; ff = frame(cx, cy, a); L = u(3.2, 4.2); r = u(1.0, 1.3); o = Obj("basalt"); tw = u(-0.5, 0.5)
    o.gcut(ell(cx, cy, L / 2 + 0.5, r + 0.6, a, 18), 0.3, "scorch")
    lip = [ff((L / 2 + 0.75) * math.cos(t), (r + 0.85) * math.sin(t)) for t in np.linspace(u(-0.4, 0.2), u(1.9, 2.6), 12)]
    o.st(rod(lip, lambda q: 0.26 + 0.24 * math.sin(q * math.pi), 0.9), foot=LineString(lip).buffer(0.5))
    cuts = [(wavy(ff(x - 0.5, -r), ff(x + 0.5 + tw, r), rng, 0.25, 3, 0.32), r * 0.5) for x in np.arange(-L * 0.28, L * 0.3, u(0.75, 1.0))] + [(wavy(ff(-L * 0.3, u(-0.2, 0.2)), ff(L * 0.3, u(-0.2, 0.2)), rng, 0.2, 4, 0.3), r * 0.5)]
    o.st(spin(cx, cy, a, L, lambda t: r * max(0.0, 1 - (2 * t / L) ** 2) ** 0.75 + 0.2 * max(0.0, 1 - abs(abs(t) - L * 0.42) / 0.25) + 0.06, 1.0), cuts=cuts, foot=rect(cx, cy, L, 2 * r, a))
    for i in range(rng.randint(2, 5)): q = ff(u(-L / 2, L / 2), r + u(1.5, 2.2)); o.st(rock(q[0], q[1], u(0.32, 0.45), u(0.3, 0.45), rng), foot=disc(q[0], q[1], 0.4))
    return o.out()

@d("cynder", "made", "iron", "anvil")
def _(cx, cy, a, rng):
    """an anvil: the wide four-cornered foot, the body, the flat face with its square hardy hole and round punch hole,
    the step down to the horn and the horn running out to its point; a hammer lies by it"""
    u = rng.uniform; ff = frame(cx, cy, a); L = u(2.5, 3.0); W = u(1.25, 1.5); h = u(1.5, 2.0); o = Obj("iron"); Lh = u(1.5, 1.9)
    ft = o.up(rect(cx, cy, L * 0.8, W + 0.9, a), h * 0.3); o.cut(rect(cx, cy, L * 0.36, W + 2, a), ft, h * 0.12)
    hc = ff(L / 2 + Lh / 2 - 0.1, 0); o.st(spin(hc[0], hc[1], a, Lh + 0.2, lambda t: W * 0.47 * max(0.0, 1 - (t + Lh / 2 + 0.1) / (Lh + 0.25)) ** 0.75 + 0.14, (h * 0.5 - u(0.22, 0.3)) / (W * 0.47 + 0.14), lift=h * 0.5), foot=rect(hc[0], hc[1], Lh, W, a))
    fc = o.up(rect(cx, cy, L, W, a), h)
    o.cut(U([rect(*ff(-L * 0.34, 0), 0.42, 0.42, a), disc(*ff(-L * 0.34 + 0.55, W * 0.2), 0.2, 6)]), fc, 0.35)
    if rng.random() < 0.8:
        c = ff(u(-0.8, 1.2), rng.choice((-1, 1)) * (W / 2 + u(1.5, 2.0))); t = a + u(-0.7, 0.7); f2 = frame(c[0], c[1], t)
        o.st(ext(capl(f2(-0.2, 0), f2(2.3, 0), 0.56), 0.4), "wood", foot=rect(*f2(1.0, 0), 2.6, 0.5, t)); o.up(rect(c[0], c[1], 0.95, 1.5, t), 0.75)
    return o.out()

@d("cynder", "made", "iron", "crucible")
def _(cx, cy, a, rng):
    """a crucible: a thick-walled pot with its pouring lip and two lifting lugs, the melt standing in it under a torn
    skin of dross (or run out in a puddle from the lip of one knocked over); the long tongs lie beside it"""
    u = rng.uniform; r = u(1.15, 1.45); h = u(1.3, 1.7); o = Obj("iron"); ff = frame(cx, cy, a)
    o.up(Polygon([ff(r * 0.5, -r * 0.62), ff(r + 0.62, -0.2), ff(r + 0.62, 0.2), ff(r * 0.5, r * 0.62)]), h)
    for sv in (-1, 1): o.up(rect(*ff(-r * 0.1, sv * (r + 0.1)), 0.95, 0.9, a), h * 0.7)
    b = o.up(disc(cx, cy, r, 24), h); o.cut(U([disc(cx, cy, r - 0.38, 18), barl(ff(r * 0.6, 0), ff(r + 0.45, 0), 0.34)]), b, u(0.3, 0.5))
    for i in range(rng.randint(1, 3)): q = pol(cx, cy, u(0, r * 0.3), u(0, 6.28)); o.st(ext(stone(q[0], q[1], u(0.3, 0.42), rng), 0.2), "ember", par=b)
    if rng.random() < 0.45: q = ff(r + 1.3, u(-0.3, 0.3)); o.st(dome(q[0], q[1], u(0.8, 1.1), 0.36, sq=u(0.6, 0.9), a=a), "ember", foot=disc(q[0], q[1], 1.0))
    tc = a + rng.choice((-1, 1)) * u(1.5, 2.6); c = pol(cx, cy, r + u(1.7, 2.1), tc); t = tc + 1.5708 + u(-0.3, 0.3) + (math.pi if rng.random() < 0.5 else 0); f2 = frame(c[0], c[1], t); c = f2(0.5, 0); f2 = frame(c[0], c[1], t); op = u(0.2, 0.4); ps = []
    for sv in (-1, 1): ps += ext(line([f2(-2.6, sv * 0.42), f2(0, -sv * 0.05), f2(0.5, -sv * (0.4 + op)), f2(1.1, -sv * (0.55 + op)), f2(1.6, -sv * (0.2 + op))], 0.56), 0.36)
    o.st(ps, foot=rect(*f2(-0.5, 0), 4.4, 1.8, t)); o.st(ext(disc(c[0], c[1], 0.42, 8), 0.56), foot=disc(c[0], c[1], 0.4))
    return o.out()

@d("cynder", "made", "iron", "ingots")
def _(cx, cy, a, rng):
    """cast ingots stacked: each a bar with sloped sides and the founder's stamp sunk in its top, a row on the ground,
    a second row laid across it, one more thrown down beside the pile"""
    u = rng.uniform; ff = frame(cx, cy, a); n = rng.randint(2, 4); o = Obj("iron"); Li = u(2.5, 2.9); Wi = 1.0; st = Wi + 0.27
    def ingot(c, t, z):
        g = rect(c[0], c[1], Li, Wi, t); b = o.up(g, z + 0.42); f2 = frame(c[0], c[1], t); tp = o.on(rect(c[0], c[1], Li - 0.4, Wi - 0.4, t), b, 0.14)
        o.cut(rect(*f2(u(-0.5, 0.5), 0), 0.6, 0.27, t), tp, 0.12)
    for i in range(n): ingot(ff(u(-0.08, 0.08), (i - (n - 1) / 2) * st), a + u(-0.03, 0.03), 0.0)
    m = rng.randint(0, min(2, n - 1)) if n > 2 else rng.randint(0, 1)
    for i in range(m): ingot(ff((i - (m - 1) / 2) * st + u(-0.1, 0.1), u(-0.2, 0.2)), a + 1.5708 + u(-0.12, 0.12), 0.56)
    if rng.random() < 0.6: ingot(ff(Li / 2 + u(1.0, 1.5), u(-1.0, 1.0)), a + u(0.3, 2.8), 0.0)
    return o.out()

@d("cynder", "debris", "obsidian", "obsidian_blades")
def _(cx, cy, a, rng):
    """obsidian shards: three to five glassy blades standing out of the ground, each a spire of flat faces leaning its
    own way, the flakes struck off them lying round their feet"""
    u = rng.uniform; o = Obj("obsidian"); n = rng.randint(3, 5); t0 = u(0, 6.28)
    for i in range(n):
        t = t0 + 6.2832 * i / n + u(-0.4, 0.4); dd = 0.0 if i == 0 else u(1.3, 2.1); c = pol(cx, cy, dd, t); L = u(2.0, 3.0) * (1.15 if i == 0 else 0.85); w = L * u(0.4, 0.52); f2 = frame(c[0], c[1], t + u(-0.6, 0.6)); h = L * u(0.85, 1.15)
        g = Polygon([f2(-L / 2, u(-0.2, 0.2)), f2(-L * 0.15, w / 2), f2(L / 2, u(-0.1, 0.3)), f2(L * 0.1, -w / 2)]); ap = f2(L * u(-0.05, 0.3), u(-0.1, 0.1))
        o.st(_facet(g, ap[0], ap[1], min(3.3, h), 12, 0.2), foot=g)
    for i in range(rng.randint(2, 5)):
        c = pol(cx, cy, u(2.4, 3.6), u(0, 6.28)); t = u(0, 6.28); g = Polygon([pol(c[0], c[1], u(0.5, 0.8), t), pol(c[0], c[1], u(0.35, 0.5), t + 2.2), pol(c[0], c[1], u(0.35, 0.5), t + 4.1)]); o.st(ext(g, u(0.26, 0.36)), foot=g)
    return o.out()

@d("cynder", "made", "ash", "charred_hand")
def _(cx, cy, a, rng):
    """a burnt arm reaching out of the ash: the forearm rising from a drift, the knob of the wrist, the back of the
    hand, four fingers with their knuckles each curled a different way and the thumb standing off to the side"""
    u = rng.uniform; ff = frame(cx, cy, a); k = u(1.0, 1.25); o = Obj("ash"); e = ff(-3.4 * k, u(-0.6, 0.6))
    o.st(dome(e[0], e[1], 1.5 * k, 0.5, sq=u(0.7, 0.95), a=u(0, 3)), foot=disc(e[0], e[1], 1.4 * k))
    o.st(rod([e, ff(-1.0 * k, 0)], lambda q: (0.62 - 0.14 * q) * k, 1.0) + dome(*ff(-0.95 * k, 0), 0.56 * k, 0.6 * k) + dome(*ff(0.0, 0), 0.9 * k, 0.62 * k, sq=0.85, a=a), foot=rect(*ff(-1.6 * k, 0), 4.0 * k, 1.8 * k, a))
    for i in range(4):
        p0 = ff(0.6 * k, (i - 1.5) * 0.5 * k); L = (u(1.7, 2.1) if 0 < i < 3 else u(1.3, 1.6)) * k; pts = _curl(p0, a + (i - 1.5) * 0.36 + u(-0.08, 0.08), L, (i - 1.5) * u(0.2, 0.7) + u(-0.3, 0.3), 9)
        o.st(rod(pts, lambda q: (0.33 - 0.05 * q + 0.07 * max(0.0, math.cos(q * 6.2832 * 1.5)) ** 4) * k, 1.1), foot=LineString(pts).buffer(0.4))
    sv = rng.choice((-1, 1)); pts = _curl(ff(-0.3 * k, sv * 0.7 * k), a + sv * u(0.9, 1.3), 1.4 * k, -sv * u(0.0, 0.7), 6); o.st(rod(pts, lambda q: (0.38 - 0.06 * q) * k, 1.1), foot=LineString(pts).buffer(0.4))
    return o.out()

@d("cynder", "growth", "tooth", "horn_spur")
def _(cx, cy, a, rng):
    """a horn growing out of the rock: thick and ringed at the root where the ground has heaved up round it, curving
    over and thinning to its point; a smaller one beside it on some"""
    u = rng.uniform; o = Obj("tooth")
    for i in range(2 if rng.random() < 0.45 else 1):
        k = 1.0 if i == 0 else u(0.55, 0.7); p = (cx, cy) if i == 0 else pol(cx, cy, u(1.8, 2.4), a + rng.choice((-1, 1)) * u(1.2, 1.9)); r0 = u(0.85, 1.05) * k; L = u(4.0, 5.2) * k
        pts = _curl(p, a + u(-0.3, 0.3) + i * u(-0.8, 0.8), L, rng.choice((-1, 1)) * u(1.0, 2.0), 14); nr = L / 0.5
        o.st(ext(stone(p[0], p[1], r0 + 0.7, rng, 0.9, 8), 0.3) + ext(stone(p[0], p[1], r0 + 0.35, rng, 0.9, 7), 0.5), "basalt", foot=disc(p[0], p[1], r0 + 0.7))
        o.st(rod(pts, lambda q, r0=r0, nr=nr: r0 * (1 - 0.74 * q) * (0.87 + 0.13 * abs(math.sin(q * math.pi * nr))), u(1.5, 2.1)), foot=LineString(pts).buffer(r0))
    return o.out()

@d("cynder", "debris", "ash", "slag_heap")
def _(cx, cy, a, rng):
    """a slag heap: clinker tipped out of the furnace in lumps, big ones heaped in the middle on a spread of ash,
    small ones rolled out to the edge, a tongue of glassy slag run down one side"""
    u = rng.uniform; r = u(2.0, 2.7); o = Obj("ash"); g = blob(cx, cy, r, rng, 5); c = g.centroid
    o.st(ext(g, 0.3) + ext(affinity.scale(g, 0.7, 0.7, origin=c), 0.55), foot=g)
    ps = []
    for i in range(rng.randint(4, 6)): q = pol(c.x, c.y, u(0, r * 0.35), u(0, 6.28)); ps += rock(q[0], q[1], u(0.65, 0.95), u(0.8, 1.3), rng, lift=0.4)
    o.st(ps, "basalt", foot=disc(c.x, c.y, r * 0.6))
    for i in range(rng.randint(6, 10)): t = u(0, 6.28); q = pol(c.x, c.y, r * u(0.55, 1.1), t); o.st(rock(q[0], q[1], u(0.4, 0.62), u(0.45, 0.75), rng), "basalt", foot=disc(q[0], q[1], 0.5))
    for i in range(rng.randint(1, 2)): q = pol(c.x, c.y, r * u(0.5, 0.9), u(0, 6.28)); o.st(ext(stone(q[0], q[1], u(0.6, 0.85), rng, 0.6), 0.62), "obsidian", foot=disc(q[0], q[1], 0.7))
    return o.out()

@d("cynder", "made", "leather", "forge_bellows")
def _(cx, cy, a, rng):
    """a forge bellows lying flat: the pear-shaped bottom board, the leather rising from it fold on fold, the top
    board nailed all round with its air hole, the two handles behind and the iron nozzle with its band in front"""
    u = rng.uniform; ff = frame(cx, cy, a); L = u(3.8, 4.8); R = u(1.4, 1.75); o = Obj("leather"); nf = rng.randint(2, 4); c0 = ff(-L / 2 + R, 0); c1 = ff(L / 2 - 0.5, 0)
    g = hull([disc(c0[0], c0[1], R, 24), disc(c1[0], c1[1], 0.55, 10)])
    for sv in (-1, 1): hp = [ff(-L / 2 + 0.5, sv * 0.5), ff(-L / 2 - u(1.2, 1.6), sv * u(0.45, 0.8))]; o.st(ext(capl(hp[0], hp[1], 0.6), 0.3 if sv < 0 else 0.3 + 0.2 * (nf + 1)), "wood", foot=LineString(hp).buffer(0.4))
    nz = ff(L / 2 + 0.6, 0); o.st(spin(nz[0], nz[1], a, 2.0, lambda t: 0.52 - 0.12 * (t + 1.0) + (0.12 if abs(t + 0.2) < 0.16 else 0.0), 1.05), "iron", foot=rect(nz[0], nz[1], 2.0, 1.0, a))
    b = o.up(g, 0.3, "wood"); ins = 0.16
    for i in range(nf): b = o.on(g.buffer(-ins, 8), b, 0.2); ins += u(0.15, 0.2)
    gt = g.buffer(-ins - 0.05, 8); tp = o.on(gt, b, 0.24, "wood"); ex = gt.buffer(-0.32, 8).exterior
    o.studs([(q.x, q.y) for q in (ex.interpolate(i / 10, normalized=True) for i in range(10))], tp, 0.25, 0.13, "iron")
    o.cut(disc(*ff(-L / 2 + R + u(-0.2, 0.3), 0), 0.34, 8), tp, 0.2)
    return o.out()

@d("cynder", "marks", "lava", "ember_moat", ground=True)
def _(cx, cy, a, rng):
    """an ember moat: a standing stone of flat faces ringed by a channel of lava, a crust lip thrown up along the
    channel's outer bank, one causeway left across"""
    u = rng.uniform; R = u(2.3, 3.0); w = u(0.7, 1.0); o = Obj("basalt"); gapw = u(0.5, 0.9); t0 = a + gapw
    g = ann(cx, cy, R, R - w, 24).difference(Polygon([(cx, cy), pol(cx, cy, 2 * R, a), pol(cx, cy, 2 * R, a + gapw / 2), pol(cx, cy, 2 * R, a + gapw)]))
    o.gcut(g, 0.5, "lava")
    for k in range(3):
        if rng.random() < 0.25: continue
        s0 = t0 + 0.25 + k * (6.2832 - gapw - 0.5) / 3; pts = arc(cx, cy, R + 0.45, s0, s0 + (6.2832 - gapw - 0.5) / 3 - 0.3, 10); o.st(rod(pts, lambda q: 0.3 + 0.16 * math.sin(q * math.pi), 0.9), foot=LineString(pts).buffer(0.4))
    st = stone(cx, cy, u(0.95, 1.25), rng, u(0.6, 0.85), rng.randint(5, 6)); o.st(_facet(st, cx + u(-0.3, 0.3), cy + u(-0.3, 0.3), u(1.6, 2.5), 12, 0.3), foot=st)
    return o.out()

@d("cynder", "debris", "ember", "sulfur_crust")
def _(cx, cy, a, rng):
    """a sulphur crust: a cracked plate of yellow scale round a blow hole, and growing on it two or three clumps of
    crystals, each a sharp four-sided point"""
    u = rng.uniform; r = u(1.8, 2.4); o = Obj("ember"); g = stone(cx, cy, r, rng, u(0.75, 0.95), 9); b = o.up(g, 0.3)
    v = pol(cx, cy, r * 0.45, u(0, 6.28)); o.cut(U([disc(v[0], v[1], 0.42, 8)] + [wavy(v, pol(cx, cy, r * 1.2, t), rng, 0.3, 3, 0.26) for t in (a, a + u(1.6, 2.4), a + u(3.6, 4.6))]).intersection(g), b, 0.28)
    for j in range(rng.randint(2, 3)):
        c = pol(cx, cy, r * u(0.2, 0.6), math.atan2(v[1] - cy, v[0] - cx) + 2.0 + j * 1.9 + u(-0.3, 0.3))
        for i in range(rng.randint(2, 4)):
            q = pol(c[0], c[1], 0 if i == 0 else u(0.5, 0.8), u(0, 6.28)); rr = u(0.65, 0.85) if i == 0 else u(0.45, 0.58); o.st(_facet(ngon(q[0], q[1], rr, 4, u(0, 1.5)), q[0] + u(-0.15, 0.15), q[1] + u(-0.15, 0.15), rr * u(2.2, 2.8), 8, 0.3), foot=disc(q[0], q[1], rr))
    return o.out()

@d("cynder", "made", "iron", "shackles")
def _(cx, cy, a, rng):
    """shackles: a plate bolted to the rock with its staple and ring, one or two lengths of chain running off from the
    ring, link lying flat after link standing on edge, each ending in an open cuff with its hinge knob"""
    u = rng.uniform; ff = frame(cx, cy, a); o = Obj("iron"); g = rrect(cx, cy, 2.0, 1.7, a, 0.3)
    b = o.up(g, 0.36); o.studs([ff(sx * 0.68, sy * 0.53) for sx in (-1, 1) for sy in (-1, 1)], b, 0.25, 0.14); o.on(rect(cx, cy, 0.5, 1.0, a), b, 0.4)
    rc = ff(0.85, 0); o.st(ext(ann(rc[0], rc[1], 0.9, 0.38, 14), 0.32), foot=disc(rc[0], rc[1], 0.9))
    for j in range(rng.randint(1, 2)):
        t = a + (u(-0.5, 0.5) if j == 0 else rng.choice((-1, 1)) * u(0.9, 1.4)); n = rng.randint(3, 5); pts = _curl(pol(rc[0], rc[1], 0.95, t), t, n * 1.0, u(-1.4, 1.4), n)
        for i in range(n):
            p, q = pts[i], pts[i + 1]; m = ((p[0] + q[0]) / 2, (p[1] + q[1]) / 2); t2 = math.atan2(q[1] - p[1], q[0] - p[0])
            if i % 2 == 0: o.st(ext(capl(pol(m[0], m[1], 0.36, t2), pol(m[0], m[1], 0.36, t2 + math.pi), 0.52), 0.56), foot=disc(m[0], m[1], 0.6))
            else: o.st(ext(ell(m[0], m[1], 0.78, 0.5, t2, 12), 0.32), cuts=[(ell(m[0], m[1], 0.36, 0.15, t2, 8), 0.12)], foot=disc(m[0], m[1], 0.7))
        e = pol(pts[-1][0], pts[-1][1], 0.95, t2); gp = Polygon([e, pol(e[0], e[1], 2, t2 + u(-0.4, 0.4) - 0.3), pol(e[0], e[1], 2, t2 + 0.5)])
        o.st(ext(ann(e[0], e[1], 1.1, 0.56, 16).difference(gp), 0.5), foot=disc(e[0], e[1], 1.1)); kq = pol(e[0], e[1], 1.0, t2 + math.pi); o.st(ext(disc(kq[0], kq[1], 0.42, 8), 0.68), foot=disc(kq[0], kq[1], 0.4))
    return o.out()

@d("cynder", "made", "basalt", "imp_idol")
def _(cx, cy, a, rng):
    """an imp idol on its plinth: a squat pot-bellied figure sitting with its knees out, hands on its belly, a round
    head with two horns, eyes and a wide mouth, an offering bowl before it; a horn is broken off on some"""
    u = rng.uniform; ff = frame(cx, cy, a); r = u(1.15, 1.4); h = u(1.3, 1.6); o = Obj("basalt"); S = r * 2 + u(1.5, 1.9)
    b = o.up(rect(*ff(0.35, 0), S, S * 0.9, a), 0.45); o.groove(inset(rect(*ff(0.35, 0), S, S * 0.9, a), 0.26), b, 0.26, 0.16)
    hd = ff(0.15, 0); rh = r * 0.66; hh = rh * 1.2
    body = turn(cx, cy, [(r, 0.0), (r, h * 0.45), (r * 0.8, h * 0.75), (r * 0.5, h)], 18, 0.9, a) + dome(hd[0], hd[1], rh, hh, lift=h * 0.85, sq=0.92, a=a)
    cuts = [(disc(*ff(0.15 + rh * 0.5, sv * rh * 0.42), 0.21, 6), h * 0.85 + hh * 0.5) for sv in (-1, 1)] + [(barl(ff(0.15 + rh * 0.82, -rh * 0.5), ff(0.15 + rh * 0.82, rh * 0.5), 0.27), h * 0.85 + hh * 0.2)]
    gone = rng.choice((-1, 1)) if rng.random() < 0.3 else 0
    for sv in (-1, 1): q = ff(0.0, sv * rh * 0.72); body += cone(q[0], q[1], 0.42, (0.35 if sv == gone else u(0.8, 1.1)), 0.3, lift=h * 0.85 + hh * 0.55, n=8)
    o.st(body, par=b, cuts=cuts)
    lw = []
    for sv in (-1, 1):
        lw += dome(*ff(r * 0.75, sv * r * 0.8), 0.52, 0.6) + rod([ff(r * 0.75, sv * r * 0.8), ff(r * 1.25, sv * r * 0.35)], 0.36, 1.0)
        lw += rod([ff(-0.1, sv * r * 0.95), ff(r * 0.55, sv * r * 0.8), ff(r * 0.92, sv * r * 0.3)], 0.33, 1.0, lift=h * 0.3)
    o.st(lw, par=b)
    c = ff(r + S * 0.25 + 0.1, u(-0.3, 0.3)); bw = o.on(disc(c[0], c[1], 0.56, 12), b, 0.34); o.cut(disc(c[0], c[1], 0.32, 8), bw, 0.2)
    return o.out()

@d("cynder", "made", "basalt", "quench_trough")
def _(cx, cy, a, rng):
    """a quenching trough cut from one block of stone: thick walls with the mason's tool marks across the rim, a
    lifting boss on each end, dark water standing in it; a blade blank with its tang lies beside, and the tongs"""
    u = rng.uniform; ff = frame(cx, cy, a); L = u(4.2, 5.2); W = u(2.1, 2.5); h = u(1.1, 1.4); o = Obj("basalt"); g = rect(cx, cy, L, W, a)
    for sx in (-1, 1): o.up(rect(*ff(sx * (L / 2 + 0.2), 0), 0.9, 1.0, a), h * 0.6)
    b = o.up(g, h); inner = rrect(cx, cy, L - 1.1, W - 1.1, a, 0.25); o.cut(inner, b, u(0.35, 0.55))
    ticks = [barl(ff(x, sy * (W / 2 - 0.55)), ff(x + 0.12, sy * (W / 2 + 0.1)), 0.26) for x in np.arange(-L / 2 + 0.6, L / 2 - 0.4, 0.62) for sy in (-1, 1)]
    o.cut(U(ticks + [Polygon([ff(sx * L / 2, sy * W / 2), ff(sx * (L / 2 - u(0.4, 0.7)), sy * W / 2), ff(sx * L / 2, sy * (W / 2 - u(0.4, 0.7)))]) for sx in (-1, 1) for sy in (-1, 1) if rng.random() < 0.4]).difference(inner.buffer(0.1)), b, 0.2)
    sv = rng.choice((-1, 1)); c = ff(u(-0.5, 0.5), sv * (W / 2 + u(1.0, 1.4))); t = a + u(-0.25, 0.25); f2 = frame(c[0], c[1], t); Lb = u(3.4, 4.2)
    bl = Polygon([f2(-Lb / 2, -0.36), f2(Lb * 0.3, -0.36), f2(Lb / 2, 0), f2(Lb * 0.3, 0.36), f2(-Lb / 2, 0.36)])
    o.st(ext(bl, 0.3) + ext(rect(*f2(-Lb / 2 - 0.5, 0), 1.1, 0.5, t), 0.26), "iron", cuts=[(barl(f2(-Lb * 0.4, 0), f2(Lb * 0.2, 0), 0.26), 0.14)], foot=rect(c[0], c[1], Lb + 1.2, 0.8, t))
    if rng.random() < 0.6:
        c = ff(u(-0.8, 0.8), -sv * (W / 2 + u(1.1, 1.5))); t = a + u(-0.4, 0.4) + (math.pi if rng.random() < 0.5 else 0); f2 = frame(c[0], c[1], t); ps = []
        for s2 in (-1, 1): ps += ext(line([f2(-2.0, s2 * 0.42), f2(0.4, -s2 * 0.05), f2(0.9, -s2 * 0.62), f2(1.5, -s2 * 0.7), f2(1.9, -s2 * 0.45)], 0.56), 0.36)
        o.st(ps, "iron", foot=rect(c[0], c[1], 4.2, 1.8, t)); o.st(ext(disc(*f2(0.4, 0), 0.42, 8), 0.56), "iron", foot=disc(*f2(0.4, 0), 0.4))
    return o.out()

@d("cynder", "debris", "basalt", "cinder_cone")
def _(cx, cy, a, rng):
    """a small cinder cone: a steep heap of spatter with a crater in its top, the rim breached on one side where the
    lava broke out, the channel it cut running away down the slope and over the ground, bombs lying round the foot"""
    u = rng.uniform; r = u(2.3, 3.0); h = u(1.7, 2.4); o = Obj("basalt"); sq = u(0.85, 1.0); tb = a + u(-0.5, 0.5); rc = r * u(0.3, 0.36)
    cuts = [(disc(cx, cy, rc, 12), h * u(0.5, 0.62)), (Polygon([(cx, cy), pol(cx, cy, r * 1.2, tb - 0.2), pol(cx, cy, r * 1.2, tb + 0.2)]), h * 0.72), (wavy(pol(cx, cy, rc * 0.8, tb), pol(cx, cy, r * 1.1, tb), rng, 0.2, 3, 0.42), h * 0.2)]
    o.st(turn(cx, cy, [(r, 0.0), (r * 0.82, h * 0.3), (r * 0.56, h * 0.75), (r * 0.44, h)], 14, sq, a), cuts=cuts, foot=disc(cx, cy, r))
    e = pol(cx, cy, r + u(1.6, 2.6), tb + u(-0.4, 0.4)); ch = wavy(pol(cx, cy, r * 0.96, tb), e, rng, 0.4, 4, 0.6)
    o.gcut(U([ch, blob(e[0], e[1], u(0.6, 0.9), rng, 3)]), 0.36, "lava")
    for i in range(rng.randint(3, 6)):
        q = pol(cx, cy, r + u(0.5, 1.4), tb + u(0.9, 5.4)); o.st(rock(q[0], q[1], u(0.36, 0.55), u(0.35, 0.6), rng), foot=disc(q[0], q[1], 0.5))
    return o.out()
