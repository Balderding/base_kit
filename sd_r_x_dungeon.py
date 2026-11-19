"""Third drawing, dungeon (see sd_r_x_frontier.py): redrawn at their real size beside a 32 mm figure. A gibbet cage
lies fallen with the bones still in it, the stocks have toppled onto their back with their feet in the air, a mimic
gapes under its half-raised lid."""
import math
from sd_reg import d
from sd_r_x_frontier import x_d
from sd_r_trederra import t_bar
from sd_r_x_frontier import x_word, x_chain

def x_wall(pts, w, zf):
    """a thin upright wall along a line, its top at zf(s) for s 0..1 along it (solid down to the ground): a hoop of a
    cage seen across, a strap running over a curve, a bar bent up off the ground"""
    ln = LineString(pts); m = max(8, int(ln.length / 0.08)); S = [i / m for i in range(m + 1)]; P = [ln.interpolate(s, normalized=True) for s in S]; Z = [zf(s) for s in S]
    def sect(z):
        out = []; run = []
        for p, zz in list(zip(P, Z)) + [(None, -1.0)]:
            if zz > z: run.append((p.x, p.y)); continue
            if len(run) >= 2: out.append(LineString(run).buffer(w / 2, cap_style=2))
            run = []
        return U(out) if out else None
    return [(sect, max(Z))]


@x_d("dungeon", "made", "iron", "gibbet_cage")
def _(cx, cy, a, rng):
    """a gibbet cage come down off its chain and lying on its side: the man-shaped cage of iron hoops and straps, the
    round head cage at one end under the hanging ring, the chain lying out from the ring; through the bars, what is
    left of the man: the skull in the head cage, ribs and the long bones"""
    u = rng.uniform; ff = frame(cx, cy, a); L = u(5.4, 5.7); x0 = -L / 2 + 0.3; o = Obj("iron")
    def R(x):          # the cage's half-width along it: head, neck, shoulders, waist, legs
        s = (x - x0) / L
        if s < 0.16: return 0.62 * math.sin(math.pi * max(0.05, s) / 0.16) ** 0.5 + 0.2
        if s < 0.2: return 0.6
        if s < 0.3: return 0.6 + 0.5 * (s - 0.2) / 0.1
        if s < 0.6: return 1.1 - 0.12 * (s - 0.3) / 0.3
        return max(0.55, 0.98 - 0.4 * (s - 0.6) / 0.4)
    zc = 1.02; ps = []
    hoops = [x0 + L * s for s in (0.04, 0.12, 0.25, 0.36, 0.47, 0.58, 0.7, 0.82, 0.95)]
    for x in hoops:
        r = R(x); ps += x_wall([ff(x, -r), ff(x, r)], 0.28, lambda s, r=r: r * (0.7 + math.sqrt(max(0.0, 1 - (2 * s - 1) ** 2))))
    for th in (0.35, 0.95, 1.5708, 2.2, 2.8):          # straps running the length of it at five places round it
        xs = np.linspace(x0 + 0.15, x0 + L - 0.1, 30)
        ps += x_wall([ff(x, R(x) * math.cos(th)) for x in xs], 0.26, lambda s, th=th: R(x0 + 0.15 + s * (L - 0.25)) * (0.7 + 0.97 * math.sin(th)))
    rg = ff(x0 - 0.45, 0); ps += ext(ann(rg[0], rg[1], 0.5, 0.22, 12), 0.5)
    o.st(ps, foot=rect(*ff(0, 0), L + 0.6, 2.4, a))
    # the bones, lower than the bars
    sk = ff(x0 + L * 0.08, u(-0.1, 0.1)); bones = dome(sk[0], sk[1], 0.46, 0.95, sq=0.85, a=a)
    for x in np.arange(x0 + L * 0.27, x0 + L * 0.45, 0.32): bones += t_bar(ff(x, -0.75), ff(x + 0.12, 0.75), 0.2, z0=0.8, z1=0.8)
    bones += t_bar(ff(x0 + L * 0.22, 0), ff(x0 + L * 0.5, 0), 0.2, z0=0.45, z1=0.45)
    for sv in (-1, 1): bones += t_bar(ff(x0 + L * 0.55, sv * 0.35), ff(x0 + L * 0.93, sv * u(0.2, 0.4)), lambda s: 0.26 if s < 0.08 or s > 0.92 else 0.2, z0=0.32, z1=0.32)
    o.st(bones, "bone", cuts=[(U([disc(*pol(sk[0], sk[1], 0.2, a + sv * 1.0), 0.14, 8) for sv in (-1, 1)]), 0.45)], foot=disc(sk[0], sk[1], 0.5))
    if rng.random() < 0.8: x_chain(o, ff(x0 - 0.95, 0), ff(x0 - 0.95 - u(0.3, 0.6), u(-1.4, 1.4)), "iron", 0.22, 0.56)
    return o.out()

@x_d("dungeon", "made", "flag", "pressure_plate")
def _(cx, cy, a, rng):
    """a trap in the floor: the trigger plate sitting a finger proud of its frame with a gap all round it and a skull
    worn into it, set among the flagstones of the passage; the dart slots in the stones beyond it and the darts that
    flew lying about, a dead man's hand bones by one"""
    u = rng.uniform; ff = frame(cx, cy, a); o = Obj("flag"); S = u(2.1, 2.3); cell = S + 0.5; fw = cell * 0.55
    for x in (-(cell + fw) / 2 - 0.05, (cell + fw) / 2 + 0.05):          # the flags each side of the plate's frame
        for v in (-cell * 0.25, cell * 0.25):
            g = rect(*ff(x + u(-0.04, 0.04), v + u(-0.04, 0.04)), fw - 0.14, cell / 2 - 0.14, a + u(-0.03, 0.03)).buffer(-0.08, join_style=2).buffer(0.08, 4)
            if rng.random() < 0.15: continue
            b = o.up(g, u(0.3, 0.38))
            if rng.random() < 0.4: o.cut(wavy(ff(x - fw * 0.4, v + u(-0.4, 0.4)), ff(x + fw * 0.4, v + u(-0.4, 0.4)), rng, 0.25, 3, 0.12), b, 0.1)
    fr = rect(cx, cy, cell * 0.98, cell * 0.98, a); b = o.up(fr, 0.32)
    pg = rect(cx, cy, S, S, a); o.cut(pg.exterior.buffer(0.12, join_style=2), b, 0.22)
    o.cut(U([rect(*ff(0, sv * (cell / 2 - 0.17)), 1.4, 0.16, a) for sv in (-1, 1)]), b, 0.25)          # the dart slots in its frame
    pl = o.on(inset(pg, 0.16), b, 0.12)
    c = (cx, cy); o.cut(U([disc(*ff(-0.25, sv * 0.36), 0.28, 10) for sv in (-1, 1)] + [tri(ff(0.15, 0), ff(0.45, 0), 0.32)]), pl, 0.12)
    o.groove(ell(cx, cy, S * 0.33, S * 0.3, a, 20), pl, 0.12, 0.08)
    o.cut(U([barl(ff(0.62, v), ff(0.88, v), 0.12) for v in (-0.3, -0.1, 0.1, 0.3)]), pl, 0.08)
    for i in range(rng.randint(2, 3)):          # darts
        p = ff(rng.choice((-1, 1)) * u(cell * 0.55, cell * 0.8), u(-cell * 0.4, cell * 0.4)); t = a + 1.5708 + u(-0.6, 0.6); q = pol(p[0], p[1], u(1.0, 1.2), t)
        o.st(ext(U([barl(p, q, 0.22), tri(q, pol(q[0], q[1], 0.35, t), 0.3), tri(pol(p[0], p[1], 0.3, t), pol(p[0], p[1], -0.15, t), 0.6)]), 0.22), "iron", foot=LineString([p, q]).buffer(0.4))
    return o.out()

@x_d("dungeon", "made", "wood", "stocks")
def _(cx, cy, a, rng):
    """the stocks, toppled onto their back: the two heavy boards with the neck hole and the two wrist holes between
    them, the iron hinge straps at one end and the hasp with its padlock at the other, the two posts lying out behind
    and their foot blocks standing up in the air; old bolt holes, a split along the grain"""
    u = rng.uniform; ff = frame(cx, cy, a); L = u(5.0, 5.3); Wb = u(0.95, 1.05); th = u(0.45, 0.5); o = Obj("wood")
    op = u(0.15, 0.6) if rng.random() < 0.4 else 0.0
    holes = U([disc(*ff(0, 0), 0.62, 16)] + [disc(*ff(sv * L * 0.3, 0), 0.38, 12) for sv in (-1, 1)])
    for sv in (-1, 1):
        g = rect(*ff(0, sv * (Wb / 2 + 0.04) + (op if sv > 0 else 0.0)), L, Wb, a).difference(affinity.translate(holes, *(lambda q: (q[0] - cx, q[1] - cy))(ff(0, op if sv > 0 else 0.0))))
        b = o.up(g, th)
        o.cut(U([barl(ff(-L, sv * Wb * k + (op if sv > 0 else 0)), ff(L, sv * Wb * k + (op if sv > 0 else 0)), 0.11) for k in (0.3, 0.7)]).intersection(inset(g, 0.25)).difference(holes.buffer(0.15)), b, 0.08)
        o.on(U([rect(*ff(x, sv * (Wb / 2 + 0.04) + (op if sv > 0 else 0)), 0.36, Wb - 0.1, a) for x in (-L / 2 + 0.3,)]), b, 0.1, "iron")
    o.studs([ff(-L / 2 + 0.3, sv * 0.5 + (op if sv > 0 else 0)) for sv in (-1, 1)], 0, 0.2, 0.16, "iron", 8)
    hp = ff(L / 2 - 0.35, 0.05 + op / 2); o.st(ext(rect(hp[0], hp[1], 0.4, 1.1 + op, a), th + 0.12) + ext(disc(*ff(L / 2 + 0.25, op / 2), 0.36, 10), th + 0.05), "iron", foot=disc(hp[0], hp[1], 0.6))
    o.st(ext(U([ann(*ff(L / 2 + 0.6, op / 2), 0.32, 0.14, 10), rrect(*ff(L / 2 + 0.95, op / 2), 0.6, 0.75, a, 0.15)]), 0.5), "iron", foot=disc(*ff(L / 2 + 0.8, op / 2), 0.6))
    vb = -(Wb + 0.08); pl = u(3.0, 3.4)
    for sx in (-1, 1):          # the posts lying out behind, their feet in the air
        x = sx * (L / 2 - 0.35); pg = rect(*ff(x, vb - pl / 2 + 0.3), 0.62, pl, a); p = o.up(pg, 0.62)
        o.cut(barl(ff(x - 0.1, vb - 0.3), ff(x + 0.05, vb - pl + 0.6), 0.1), p, 0.08)
        fg = rect(*ff(x + sx * 0.0, vb - pl + 0.1), 1.6, 0.6, a); f = o.up(fg, u(1.6, 1.8)); o.cut(barl(ff(x - 0.8, vb - pl + 0.1), ff(x + 0.8, vb - pl + 0.1), 0.1), f, 0.08)
    return o.out()

@x_d("dungeon", "made", "iron", "portcullis_piece")
def _(cx, cy, a, rng):
    """a broken piece of a portcullis lying in the rubble: the upright bars with the spiked feet, the cross bars over
    them riveted at every crossing with a square plate, the torn-off ends bent up off the ground, lumps of rust; a few
    stones from the arch lying by it"""
    u = rng.uniform; ff = frame(cx, cy, a); o = Obj("iron"); nx = rng.randint(3, 4); ny = rng.randint(3, 4); s = u(1.15, 1.3); w = 0.42
    X = [(i - (nx - 1) / 2) * s for i in range(nx)]; Y = [(j - (ny - 1) / 2) * s for j in range(ny)]; y0, y1 = Y[0] - 0.6, Y[-1] + 0.5
    ups = []
    for x in X:
        top = y1 + (u(0.0, 0.5) if rng.random() < 0.5 else 0.0)
        ups.append(U([barl(ff(x, y0), ff(x, top), w), tri(ff(x, y0 + 0.05), ff(x, y0 - 0.9), w + 0.12)]))
    b = o.up(U(ups), 0.42)
    cr = U([barl(ff(X[0] - 0.45, y), ff(X[-1] + (0.45 if rng.random() < 0.6 else -s * 0.5), y), w) for y in Y])
    c = o.up(cr, 0.84)
    o.on(U([rect(*ff(x, y), 0.6, 0.6, a) for x in X for y in Y if Polygon(cr.convex_hull).contains(Point(*ff(x, y)))]), c, 0.1)
    o.studs([ff(x, y) for x in X for y in Y], c, 0.2, 0.22)
    if rng.random() < 0.7:          # one bar torn off and bent up out of the frame
        x = rng.choice(X); ps = x_wall([ff(x, y1 - 0.1), ff(x, y1 + 1.4)], w, lambda s: 0.42 + 1.1 * s ** 1.5)
        o.st(ps, foot=rect(*ff(x, y1 + 0.7), w, 1.5, a))
    o.st([s_ for _ in range(rng.randint(2, 4)) for s_ in dome(*ff(u(-s * nx / 2, s * nx / 2), u(y0, y1)), 0.3, 0.6, sq=0.8)], "rust", foot=rect(cx, cy, 1, 1, a))
    for i in range(rng.randint(1, 2)):
        c2 = ff(rng.choice((-1, 1)) * (s * nx / 2 + u(0.6, 1.0)), u(y0, y1)); t = u(0, 3.14); g = Polygon([pol(c2[0], c2[1], u(0.5, 0.8), t + k * 1.3 + u(-0.2, 0.2)) for k in range(5)])
        bs = o.up(g, u(0.6, 0.9), "flag"); o.on(inset(g, 0.15), bs, 0.1, "flag")
    return o.out()

@x_d("dungeon", "made", "wood", "mimic")
def _(cx, cy, a, rng):
    """a mimic: an iron-bound chest whose lid has come up like a jaw; rows of teeth along both lips, the gullet dark
    between them and the long tongue lolling out over the front and down onto the floor, two eyes opened on the lid;
    iron bands and corner irons with their rivets, the lock plate on its front split by a fang"""
    u = rng.uniform; ff = frame(cx, cy, a); L = u(4.9, 5.2); W = u(2.5, 2.8); h = u(2.3, 2.6); o = Obj("wood"); g = rect(cx, cy, L, W, a)
    b = o.up(g, h); mo = rect(*ff(0, -W * 0.08), L - 0.7, W * 0.42, a)
    o.cut(mo, b, h * 0.6)
    o.seams(ff, L, W, b, 0.55, across=False, w=0.1, d=0.06, skip=mo.buffer(0.1))
    bands = [-L * 0.32, L * 0.32]; o.on(U([rect(*ff(x, 0), 0.4, W, a) for x in bands] + [rect(*ff(sx * (L / 2 - 0.25), 0), 0.5, W, a) for sx in (-1, 1)]).difference(mo.buffer(0.05)), b, 0.1, "iron")
    o.studs([ff(x, v) for x in bands for v in (-W / 2 + 0.25, W / 2 - 0.25)], b, 0.2, 0.2, "iron", 8)
    tf = [cone(*ff(x, -W * 0.08 - W * 0.21 + 0.22), 0.3, h + 0.65, 0.06, n=6) for x in np.arange(-L / 2 + 0.65, L / 2 - 0.5, 0.55)]
    o.st([s for t in tf for s in t], "bone", foot=rect(*ff(0, -W * 0.29), L - 0.8, 0.5, a))
    # the lid, raised like a jaw: hinged at the back, its front lip up in the air
    lv0 = W * 0.13; lv1 = W / 2; zb, zf = h + 0.1, h + u(1.0, 1.3)
    lg = rect(*ff(0, (lv0 + lv1) / 2), L, lv1 - lv0, a)
    lid = x_wall([ff(0, lv1), ff(0, lv0)], L, lambda s: zb + (zf - zb) * s)
    o.st(lid, cuts=[(U([rect(*ff(x, (lv0 + lv1) / 2), 0.36, W, a) for x in bands]), zf + 0.5)], foot=lg)
    ey = [ff(sx * L * 0.18, lv0 + 0.4) for sx in (-1, 1)]
    o.st([s for e in ey for s in dome(e[0], e[1], 0.32, zf + 0.3, sq=0.8, a=a)] + [s for x in np.arange(-L / 2 + 0.6, L / 2 - 0.4, 0.55) for s in cone(*ff(x, lv0 + 0.16), 0.3, zf + 0.5, 0.06, n=6)], "bone", cuts=[(U([disc(e[0], e[1], 0.12, 6) for e in ey]), zf + 0.1)], foot=rect(*ff(0, lv0 + 0.3), L, 0.6, a))
    e = ff(u(-0.8, 0.8), -W / 2 - u(1.4, 2.0)); tp = [ff(u(-0.3, 0.3), -W * 0.1), ff(u(-0.3, 0.3), -W / 2 - 0.1), e]
    o.st(t_bar(tp[0], tp[1], lambda s: 0.5, z0=h * 0.55, z1=h + 0.05) + t_bar(tp[1], tp[2], lambda s: 0.5 - 0.15 * s, z0=h * 0.9, z1=0.3), "flesh", cuts=[(barl(tp[1], tp[2], 0.12), 0.45)], foot=LineString(tp).buffer(0.5))
    lk = ff(0, -W / 2 - 0.15); o.st(ext(rect(lk[0], lk[1], 0.8, 0.3, a), h * 0.8), "iron", foot=rect(lk[0], lk[1], 0.8, 0.3, a))
    return o.out()

@x_d("dungeon", "made", "flag", "keystone")
def _(cx, cy, a, rng):
    """the keystone of a fallen arch lying face up among two or three of its voussoirs: each a dressed wedge of stone
    with a drafted margin round its face and chipped corners; the keystone deeper and carved, a grim face with its brow,
    eye sockets, nose and mouth, or a boss with a cross or a rune"""
    u = rng.uniform; ff = frame(cx, cy, a); o = Obj("flag"); h = u(1.45, 1.6)
    def wedge(c, t, w0, w1, l):
        f2 = frame(c[0], c[1], t); g = Polygon([f2(-w0 / 2, -l / 2), f2(w0 / 2, -l / 2), f2(w1 / 2, l / 2), f2(-w1 / 2, l / 2)])
        for _ in range(rng.randint(0, 2)): g = g.difference(disc(*f2(rng.choice((-1, 1)) * w1 / 2, rng.choice((-1, 1)) * l / 2), u(0.2, 0.4), 6))
        return g, f2
    ks, f2 = wedge((cx, cy), a + u(-0.2, 0.2), 1.4, 1.95, 2.2); b = o.up(ks, h + 0.25); fc = o.on(inset(ks, 0.22), b, 0.12)
    k = rng.randint(0, 2)
    if k == 0:          # a face
        o.on(U([ell(*f2(sx * 0.32, 0.35), 0.32, 0.16, 0, 8) for sx in (-1, 1)]), fc, 0.12)
        o.cut(U([disc(*f2(sx * 0.32, 0.12), 0.18, 8) for sx in (-1, 1)]), fc, 0.14)
        o.on(Polygon([f2(0, 0.25), f2(0.18, -0.25), f2(-0.18, -0.25)]), fc, 0.14)
        o.cut(ell(*f2(0, -0.55), 0.34, 0.11, 0, 10), fc, 0.12)
    elif k == 1:
        bs = o.on(disc(*f2(0, 0), 0.6, 16), fc, 0.14); o.cut(U([barl(f2(0, -0.42), f2(0, 0.42), 0.14), barl(f2(-0.42, 0), f2(0.42, 0), 0.14)]), bs, 0.08)
    else:
        o.cut(U([line([f2(-0.3, -0.5), f2(0, 0.5), f2(0.3, -0.5)], 0.14), barl(f2(-0.35, 0.0), f2(0.35, 0.0), 0.14)]), fc, 0.12)
    for i, sx in enumerate(rng.sample((-1, 1), 2) + ([rng.choice((-1, 1))] if rng.random() < 0.5 else [])):
        c = ff(sx * u(1.6, 1.75) * (1.0 if i < 2 else 0.0) + (0 if i < 2 else u(-0.4, 0.4)), (0.0 if i < 2 else rng.choice((-1, 1)) * 1.9) + u(-0.2, 0.2))
        g, f3 = wedge(c, a + u(-0.4, 0.4) + (1.57 if i == 2 else 0.0), 1.0, 1.3, 1.8); vb = o.up(g, u(1.25, 1.4)); o.on(inset(g, 0.2), vb, 0.1)
    return o.out()


_OWN = set(globals())
