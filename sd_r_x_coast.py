"""Third drawing, coast (see sd_r_x_frontier.py): redrawn at their real size beside a 32 mm figure. A ship's gun sits
on its truck carriage with its shot piled by it, a capstan stands as tall as a man's hip with its bars pulled out, the
lobster pot is a slatted half barrel with something still in it."""
import math
from sd_reg import d
from sd_r_x_frontier import x_d
from sd_r_trederra import t_bar
from sd_r_x_frontier import x_chain
from sd_r_x_dungeon import x_wall

def x_ball(x, y, r, lift=0.0, n=16):
    """a ball resting on the ground (or on something lift high): round over the top, solid below its middle"""
    return turn(x, y, [(r * 0.75, 0.0), (r, r * 0.55)] + crv(r, 0.12, r * 0.55 + 0.01, r * 1.9, n=6), n, lift=lift)

def x_coil(o, c, R, turns_, w, kind="rope", h=0.36):
    """a coil of rope lying flat: rings one inside the other, the end run off"""
    rings = [ann(c[0], c[1], R - k * (w + 0.08), R - k * (w + 0.08) - w, 20) for k in range(turns_) if R - k * (w + 0.08) - w > 0.25]
    return o.st(ext(U(rings), h), kind, foot=disc(c[0], c[1], R))


@x_d("coast", "made", "drift", "oars")
def _(cx, cy, a, rng):
    """oars left on the shingle: each a round loom with its narrower grip and end button, the leather collar where it
    sat in the rowlock, the long blade with its raised spine fading out to a worn, split tip; one or two, and the iron
    rowlock lying with them"""
    u = rng.uniform; o = Obj("drift"); n = 2 if rng.random() < 0.6 else 1; foot = []
    for i in range(n):
        b = a + (0.0 if i == 0 else rng.choice((-1, 1)) * u(0.12, 0.3)); f2 = frame(*pol(cx, cy, 0.0 if i == 0 else u(0.8, 1.1), a + 1.5708), b); L = u(8.4, 8.8); x0 = -L / 2; xb = L / 2 - 2.6
        def rf(s, L=L):
            x = s * (L - 2.6 - 0.1)
            if x < 0.25: return 0.3
            if x < 1.6: return 0.22
            if 2.7 < x < 3.5: return 0.33
            return 0.27 - 0.04 * x / (L - 2.6)
        o.st(t_bar(f2(x0, 0), f2(xb + 0.1, 0), rf, 0.95), cuts=[(U([barl(f2(x0 + 2.75 + k * 0.2, -0.5), f2(x0 + 2.75 + k * 0.2, 0.5), 0.08) for k in range(4)]), 0.5)], foot=rect(*f2(-1.3, 0), L - 2.6, 0.7, b))
        tip = Polygon([f2(xb - 0.1, -0.22), f2(xb + 1.0, -0.6), f2(L / 2 - 0.1, -0.62), f2(L / 2, -0.4), f2(L / 2, 0.45), f2(L / 2 - 0.15, 0.62), f2(xb + 1.0, 0.6), f2(xb - 0.1, 0.22)])
        if rng.random() < 0.35: tip = tip.difference(tri(f2(L / 2 + 0.05, u(-0.3, 0.3)), f2(L / 2 - u(0.7, 1.1), u(-0.1, 0.1)), 0.24))
        bl = o.up(tip, 0.24); o.st(t_bar(f2(xb - 0.1, 0), f2(L / 2 - 0.5, 0), lambda s: 0.24 - 0.04 * s, 0.4), par=None, foot=rect(*f2(xb + 1, 0), 2.2, 0.5, b))
        foot.append(rect(*f2(0, 0), L, 1.3, b))
    c = pol(cx, cy, u(1.4, 1.8), a - 1.5708) ; t = a + u(-0.5, 0.5); f3 = frame(c[0], c[1], t)
    o.st(ext(U([LineString(arc(*f3(0.35, 0), 0.42, -1.5708, 1.5708, 8)).buffer(0.13), barl(f3(0.35, 0), f3(-0.75, 0), 0.26)]), 0.3), "iron", foot=disc(c[0], c[1], 0.8))
    return o.out()

@x_d("coast", "made", "iron", "cannon")
def _(cx, cy, a, rng):
    """a ship's gun on its truck carriage: the barrel with its muzzle swell, the reinforce rings and the cascabel knob,
    the trunnions held down by iron cap squares on the stepped cheeks, the four solid trucks on their axletrees, the
    quoin under the breech; round shot piled beside it and the rammer lying by; or the barrel alone, dismounted in the
    sand with its shot"""
    u = rng.uniform; ff = frame(cx, cy, a); o = Obj("iron"); L = u(7.4, 7.8); r = u(0.8, 0.9); xb = -L / 2 + 0.5
    def rf(s):
        x = s * (L - 0.9)
        if x < 0.25: return r * 0.55
        if x < 0.5: return r * 1.02
        if x < 2.0: return r * 1.0
        if x < 2.3: return r * 1.08
        if x < 3.2: return r * 0.94
        if x < 3.4: return r * 1.0
        if x < L - 1.8: return r * (0.9 - 0.18 * (x - 3.4) / (L - 5.2))
        if x < L - 1.4: return r * 0.8
        return r * (0.72 + 0.16 * math.sin(min(1.0, (x - L + 1.4) / 0.5) * 1.5708))
    if rng.random() < 0.65:
        zc = u(2.3, 2.5); Lc = u(4.6, 5.0); xc = -L / 2 + 0.6 + Lc / 2 - 0.3; Wc = 2.2
        for sv in (-1, 1):          # the cheeks, stepped down towards the breech
            for k, (x, l, h) in enumerate(((xc + Lc * 0.3, Lc * 0.4, zc + 0.1), (xc - Lc * 0.05, Lc * 0.3 + 0.3, zc - 0.25), (xc - Lc * 0.35, Lc * 0.3 + 0.3, zc - 0.65))):
                ch = o.up(rect(*ff(x, sv * (Wc / 2 - 0.25)), l, 0.5, a), h, "wood")
                if k == 0: cp = o.on(rect(*ff(x + 0.3, sv * (Wc / 2 - 0.25)), 0.9, 0.5, a), ch, 0.14); o.studs([ff(x + 0.3 + dx, sv * (Wc / 2 - 0.25)) for dx in (-0.3, 0.3)], ch, 0.2, 0.24)
        for x in (xc + Lc * 0.38, xc - Lc * 0.38):          # the trucks: thick wooden wheels on the axletrees
            o.up(rect(*ff(x, 0), 0.7, Wc + 1.2, a), 0.75, "wood")
            for sv in (-1, 1): o.st(x_wall([ff(x - 0.65, sv * (Wc / 2 + 0.32)), ff(x + 0.65, sv * (Wc / 2 + 0.32))], 0.42, lambda s: 0.1 + 1.2 * math.sqrt(max(0.0, 1 - (2 * s - 1) ** 2))), "wood", foot=rect(*ff(x, sv * (Wc / 2 + 0.32)), 1.3, 0.45, a))
        o.up(rect(*ff(xc - Lc * 0.38, 0), 1.1, Wc - 1.0, a), zc - r * 0.75, "wood")          # the quoin under the breech
        o.st(t_bar(ff(xb, 0), ff(L / 2, 0), rf, z0=zc, z1=zc) + t_bar(ff(xb - 0.5, 0), ff(xb + 0.05, 0), lambda s: 0.35 if s < 0.6 else 0.22, z0=zc, z1=zc) + [s_ for sv in (-1, 1) for s_ in t_bar(ff(xc + Lc * 0.3 + 0.3, sv * 0.6), ff(xc + Lc * 0.3 + 0.3, sv * (Wc / 2 + 0.1)), 0.3, z0=zc + 0.05, z1=zc + 0.05)], cuts=[(disc(*ff(L / 2 - 0.25, 0), r * 0.42, 10), zc + 0.2)], foot=rect(cx, cy, L, 2 * r, a))
        sv = rng.choice((-1, 1)); pile = ff(u(-1.0, 1.0), sv * u(2.8, 3.1))
    else:          # dismounted, the barrel half in the sand
        t = a + u(-0.15, 0.15); f2 = frame(cx, cy, t)
        o.st(t_bar(f2(xb, 0), f2(L / 2, 0), rf, z0=r * 0.85, z1=r * 0.7) + t_bar(f2(xb - 0.5, 0), f2(xb + 0.05, 0), lambda s: 0.35 if s < 0.6 else 0.22, z0=r * 0.85, z1=r * 0.85), cuts=[(disc(*f2(L / 2 - 0.25, 0), r * 0.42, 10), r * 0.9)], foot=rect(cx, cy, L, 2 * r, t))
        for sv in (-1, 1): o.st(t_bar(f2(-L * 0.08, sv * r * 0.8), f2(-L * 0.08, sv * (r + 0.35)), 0.3, z0=r * 0.8, z1=r * 0.8), foot=disc(*f2(-L * 0.08, sv * r), 0.4))
        sv = rng.choice((-1, 1)); pile = ff(u(-1.0, 1.0), sv * u(1.8, 2.2))
    rb = u(0.42, 0.48); n = rng.choice((3, 4)); balls = []
    for k in range(n): balls += x_ball(*pol(pile[0], pile[1], rb * 1.05 * (1 if n == 3 else 1.42), a + 6.2832 * k / n), rb)
    balls += x_ball(pile[0], pile[1], rb, lift=rb * 0.85)
    o.st(balls, foot=disc(pile[0], pile[1], rb * 2.3))
    if rng.random() < 0.6:
        p = ff(-L * 0.35, sv * u(2.0, 2.4) * (1.0 if n else 1)); q = ff(L * 0.3, sv * u(2.0, 2.4))
        if math.dist(p, pile) > 1.6 and math.dist(q, pile) > 1.6: o.st(t_bar(p, q, lambda s: 0.36 if s > 0.92 else 0.2, 0.6), "wood", foot=LineString([p, q]).buffer(0.4))
    return o.out()

@x_d("coast", "made", "drift", "capstan")
def _(cx, cy, a, rng):
    """a ship's capstan standing on its deck plank: the round step it stands on with the pawls round its foot, the
    barrel with its ribbed whelps, a turn of hawser round the waist, the broad drumhead with its square bar holes and the
    iron cap with its bolt heads; the bars pulled out and lying by it"""
    u = rng.uniform; o = Obj("drift"); r = u(1.05, 1.2); h = u(3.5, 3.8); n = 8; t0 = u(0, 1)
    base = o.up(ngon(cx, cy, r + 0.75, 12, t0), 0.35)
    o.studs([pol(cx, cy, r + 0.45, t0 + 6.2832 * k / 6) for k in range(6)], base, 0.22, 0.2, "iron", 8)
    wh = star(cx, cy, r + 0.12, r - 0.08, n, t0 + 0.2)
    o.st([((lambda z, g=wh: g if z < h * 0.72 else None), h * 0.72)] + turn(cx, cy, [(r - 0.06, 0.0), (r - 0.06, h * 0.72), (r + 0.25, h * 0.74), (r + 0.25, h - 0.08), (r * 0.8, h)], 24) + ext(ann(cx, cy, r + 0.3, r - 0.1, 24), h * 0.36), par=base,
         cuts=[(U([rect(*pol(cx, cy, r + 0.1, t0 + 6.2832 * k / 6), 0.5, 0.36, t0 + 6.2832 * k / 6) for k in range(6)]), h * 0.8)], foot=disc(cx, cy, r + 0.3))
    o.st(ext(disc(cx, cy, r * 0.62, 18), h - 0.5 + 0.35 + 0.18) + [s for k in range(6) for s in cyl(*pol(cx, cy, r * 0.42, t0 + 6.2832 * k / 6), 0.2, h - 0.5 + 0.35 + 0.3, n=6)], "iron", foot=disc(cx, cy, r * 0.62))
    nb = rng.randint(1, 3)
    for i in range(nb):
        t = a + i * 2.1 + u(-0.3, 0.3); p = pol(cx, cy, r + 1.0, t + 0.5); q = pol(p[0], p[1], u(2.9, 3.2), t + 1.5708 + 0.5)
        o.st(t_bar(p, q, lambda s: 0.26 if s < 0.85 else 0.32, 0.7), "drift", foot=LineString([p, q]).buffer(0.4))
    return o.out()

@x_d("coast", "debris", "drift", "driftwood")
def _(cx, cy, a, rng):
    """a drift log thrown up by the sea: the long trunk bleached and grooved along its grain, a split opened along it,
    knots and the snapped stubs of branches, the root plate at its butt with its worn roots splayed out; a scrap of rope
    still round it on some"""
    u = rng.uniform; ff = frame(cx, cy, a); o = Obj("drift"); L = u(6.9, 7.4); r = u(0.55, 0.65); bend = u(-0.6, 0.6)
    x0 = -L / 2 + 1.0; pts = [ff(x0 + L * s, bend * math.sin(math.pi * s)) for s in np.linspace(0, 1, 8)]
    ps = []
    ps += rod(pts, lambda s: r * (1.12 - 0.45 * s) * (1.0 + 0.06 * math.sin(s * 23)), 1.85)
    for k in range(rng.randint(1, 3)):          # stubs of branches
        s = u(0.3, 0.85); i = int(s * 7); p = pts[i]; t = a + rng.choice((-1, 1)) * u(0.6, 1.2); q = pol(p[0], p[1], r + u(0.6, 1.0), t)
        ps += rod([p, q], lambda s_: 0.34 - 0.08 * s_, 1.6 * r / 0.34)
    rp = pts[0]; ps += dome(rp[0], rp[1], r * 1.5, r * 2.2, sq=0.85, a=a, n=12)
    for k in range(rng.randint(3, 5)):
        t = a + math.pi + u(-1.4, 1.4); q = pol(rp[0], rp[1], r * 1.5 + u(0.6, 1.1), t); ps += rod([pol(rp[0], rp[1], r * 0.8, t), q], lambda s: 0.4 - 0.14 * s, 2.0)
    gr = U([LineString([pol(p[0], p[1], v, a + 1.5708) for p in pts[1:-1]]).buffer(0.06) for v in (-r * 0.5, -r * 0.15, r * 0.25, r * 0.55)])
    cuts = [(gr, r * 1.75)]
    if rng.random() < 0.6: cuts.append((LineString([pol(p[0], p[1], u(-0.1, 0.1), a + 1.5708) for p in pts[2:6]]).buffer(0.1), r * 1.4))
    o.st(ps, cuts=cuts, foot=hull([LineString(pts).buffer(r + 0.6), disc(rp[0], rp[1], r * 1.5 + 1.0)]))
    if rng.random() < 0.4:
        p = pts[4]; o.st(t_bar(pol(p[0], p[1], r + 0.2, a + 1.5708), pol(p[0], p[1], r + 0.2, a - 1.5708), 0.22, z0=r * 2.0, z1=r * 2.0), "rope", foot=disc(p[0], p[1], r + 0.3))
    return o.out()

@x_d("coast", "made", "glass", "net_floats")
def _(cx, cy, a, rng):
    """a heap of old fishing net spread on the shingle, its square mesh of cord and the thicker head rope along one
    edge with cork floats strung on it; glass floats in it, each a ball in its knotted net bag with the tie at its top"""
    u = rng.uniform; ff = frame(cx, cy, a); o = Obj("rope"); L = u(4.8, 5.2); W = u(3.4, 3.8); m = u(0.62, 0.7); t = a + u(-0.1, 0.1)
    edge = rect(cx, cy, L, W, a).buffer(-0.4).buffer(0.4 + u(0.0, 0.3), 3)
    gs = [barl(ff(x + u(-0.1, 0.1), -W), ff(x + u(-0.3, 0.3), W), 0.36) for x in np.arange(-L / 2 + 0.2, L / 2, m)] + [barl(ff(-L, v + u(-0.1, 0.1)), ff(L, v + u(-0.3, 0.3)), 0.36) for v in np.arange(-W / 2 + 0.2, W / 2, m)]
    o.up(U(gs).intersection(edge), 0.22, "twine")
    hr = [ff(x, W / 2 - 0.15 + 0.2 * math.sin(x * 1.3)) for x in np.linspace(-L / 2 - 0.1, L / 2 + 0.1, 12)]
    o.st(rod(hr, 0.24, 1.0), "rope", foot=LineString(hr).buffer(0.3))
    for x in np.arange(-L / 2 + 0.5, L / 2 - 0.3, 1.3): p = ff(x, W / 2 - 0.15 + 0.2 * math.sin(x * 1.3)); o.st(ext(rrect(p[0], p[1], 0.62, 0.5, a, 0.12), 0.4), "wood", foot=disc(p[0], p[1], 0.4))
    cs = []
    for i in range(rng.randint(2, 4)):
        for _ in range(12):
            r = u(0.62, 0.8); c = ff(u(-L / 2 + 0.8, L / 2 - 0.8), u(-W / 2 + 0.8, W / 2 - 1.0))
            if all(math.dist(c, q) > r + rq + 0.3 for q, rq in cs): break
        cs.append((c, r))
        rings = [(ann(c[0], c[1], rr + 0.07, rr - 0.07, 18), r * (0.55 + 1.35 * math.sqrt(max(0.0, 1 - (rr / r) ** 2))) - 0.1) for rr in (r * 0.45, r * 0.8)]
        o.st(x_ball(c[0], c[1], r) + cyl(c[0], c[1], 0.24, r * 1.9 + 0.22, n=8), "glass", foot=disc(c[0], c[1], r)); o.st(ext(ann(c[0], c[1], r + 0.12, r - 0.12, 18), r * 0.62), "rope", foot=disc(c[0], c[1], r + 0.1))
    return o.out()

@x_d("coast", "made", "twig", "lobster_pot")
def _(cx, cy, a, rng):
    """a slatted lobster pot, a half barrel of bent hoops on a plank base: the laths running over the hoops with gaps
    between them, the netting eye in each end, the flat stone weighting its base; something caught still inside it on
    some; its rope run out to a cork buoy"""
    u = rng.uniform; ff = frame(cx, cy, a); o = Obj("wood"); L = u(3.7, 4.0); W = u(2.5, 2.7); R = W / 2; x0 = -L / 2
    b = o.up(rect(cx, cy, L, W, a), 0.26)
    o.cut(U([barl(ff(-L, v), ff(L, v), 0.1) for v in (-W / 6, W / 6)]), b, 0.08)
    if rng.random() < 0.6:          # a lobster inside
        c = ff(u(-0.6, 0.2), u(-0.3, 0.3)); t = a + u(-0.4, 0.4); f2 = frame(c[0], c[1], t)
        o.st(spin(c[0], c[1], t, 1.6, lambda s: 0.36 * math.sqrt(max(0.05, 1 - (2 * s / 1.6) ** 2)), 1.1) + [s_ for sv in (-1, 1) for s_ in dome(*f2(1.1, sv * 0.4), 0.3, 0.45, sq=0.6, a=t)], "hide", par=b, foot=disc(c[0], c[1], 1.0))
    ps = []
    for s in np.linspace(0.03, 0.97, rng.randint(4, 5)):
        x = x0 + L * s; ps += x_wall([ff(x, -R + 0.05), ff(x, R - 0.05)], 0.3, lambda q: 0.25 + R * 1.1 * math.sqrt(max(0.0, 1 - (2 * q - 1) ** 2)))
    nl = rng.randint(5, 6)
    for k in range(nl):
        th = math.pi * (k + 0.5) / nl; v = R * 0.95 * math.cos(th); z = 0.25 + R * 1.1 * math.sin(th)
        ps += x_wall([ff(x0 + 0.05, v), ff(-x0 - 0.05, v)], 0.3, lambda q, z=z: z + 0.06)
    for sx in (-1, 1): ps += ext(ann(*ff(sx * (L / 2 - 0.1), 0), 0.55, 0.32, 12), 0.7)
    o.st(ps, "wood", foot=rect(cx, cy, L, W, a))
    e = ff(L / 2 + u(2.9, 3.3), u(-1.4, 1.4)); pts = wiggle(ff(L / 2 + 0.2, u(-0.5, 0.5)), pol(e[0], e[1], 0.7, a + math.pi), rng, 0.6, 4)
    o.st(rod(pts, 0.22, 1.0), "rope", foot=LineString(pts).buffer(0.3))
    o.st(turn(e[0], e[1], [(0.62, 0.0), (0.7, 0.4), (0.62, 0.8), (0.3, 0.9), (0.22, 1.5)], 14), "wood", foot=disc(e[0], e[1], 0.7))
    return o.out()


_OWN = set(globals())
