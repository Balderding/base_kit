"""Third drawing, highland: the objects at their real size beside a 32 mm figure (1 m = 18 mm as printed): a beacon
built as a log crib a man's height, a still with its worm tub, a cross with its knotwork and pierced ring, the
caber with the stone and the hammer. Nothing overhangs: the cross and the caber lie, the lyne arm runs down solid."""
import math
from sd_reg import d

def xon(o, g, par, h, kind=None):
    """a part standing on layer par as a plain "on" layer (keeps parts down to a quarter of a millimetre at true
    scale, where Obj.on, through ST(), drops anything under 0.3)"""
    if g is None or g.is_empty: return None
    o.L.append(("on", g, par, h, kind or o.k)); return len(o.L) - 1

def xstuds(o, pts, par, r=0.2, h=0.1, kind=None, n=8):
    return xon(o, U([disc(x, y, r, n) for x, y in pts]), par, h, kind) if pts else None

def xh_bar(p, q, r, z0, z1):
    """a round pipe from p to q whose axis climbs from z0 to z1, solid underneath (nothing hangs)"""
    L = math.dist(p, q) or 1.0; m = max(8, int(L / 0.12)); ux, uy = (q[0] - p[0]) / L, (q[1] - p[1]) / L
    pt = lambda s, w: (p[0] + ux * s * L - uy * w, p[1] + uy * s * L + ux * w); S = [i / m for i in range(m + 1)]
    def sect(z):
        run = []
        for s in S:
            c = z0 + (z1 - z0) * s; w = r if z <= c else (math.sqrt(max(0.0, r * r - (z - c) ** 2)) if z < c + r else 0.0)
            if w >= 0.16: run.append((s, w))
        if len(run) < 2: return None
        g = Polygon([pt(s_, w_) for s_, w_ in run] + [pt(s_, -w_) for s_, w_ in reversed(run)])
        return g if g.is_valid and g.area > 0.05 else g.buffer(0)
    return [(sect, max(z0, z1) + r)]

def xh_rough(cx, cy, L, W, a, rng, j=0.12):
    """a split slab's outline: a rectangle with its edges and corners knocked about"""
    f = frame(cx, cy, a); pts = []
    for (x0, y0), (x1, y1) in (((-1, -1), (1, -1)), ((1, -1), (1, 1)), ((1, 1), (-1, 1)), ((-1, 1), (-1, -1))):
        for k in range(3): s = k / 3; pts.append(f((x0 + (x1 - x0) * s) * L / 2 + rng.uniform(-j, j), (y0 + (y1 - y0) * s) * W / 2 + rng.uniform(-j, j)))
    return Polygon(pts).buffer(0)


@d("highland", "made", "wood", "beacon_pyre")
def _(cx, cy, a, rng):
    """a hill beacon laid ready: a square crib of split logs built up course on course, each course across the one
    below with the ends standing out, packed with furze and brushwood that mounds up out of the top; a ring of hearth stones round the foot"""
    u = rng.uniform; ff = frame(cx, cy, a); r = u(1.8, 2.3); S = r * u(1.3, 1.4); rl = u(0.26, 0.3); n = rng.randint(5, 6); o = Obj("wood"); dz = rl * 1.55; allg = []
    for k in range(n):
        t = a + (k % 2) * 1.5708 + u(-0.05, 0.05); f2 = frame(cx + u(-0.06, 0.06), cy + u(-0.06, 0.06), t); Lk = S + 0.5 - k * 0.1; vv = S / 2 - rl - k * 0.06
        logs = [s for sv in (-1, 1) for s in lie(*f2(u(-0.15, 0.15), sv * vv), t + u(-0.04, 0.04), Lk, rl, lift=k * dz)]
        if k >= 2 and rng.random() < 0.6: logs += lie(*f2(u(-0.2, 0.2), u(-0.3, 0.3)), t + u(-0.1, 0.1), Lk * 0.8, rl * 0.9, lift=k * dz)
        allg += logs
    ht = min(3.3, n * dz + u(0.3, 0.55)); br = [s for i in range(7) for s in dome(*pol(cx, cy, 0 if i == 0 else S * 0.22, i * 1.05 + u(0, 0.4)), S * u(0.2, 0.26), ht * (1.0 if i == 0 else u(0.85, 0.95)), n=10)]
    o.st(allg + br, foot=rect(cx, cy, S + 0.7, S + 0.7, a))          # crib and brush as one surface: built as separate overlapping pieces they can tear the mesh
    for p in ringpts(cx, cy, r + 1.0, rng.randint(7, 10), a, 0.1, rng): o.st(rock(p[0], p[1], u(0.4, 0.55), u(0.35, 0.55), rng), "slate", foot=disc(p[0], p[1], 0.55))
    o.F.append(disc(cx, cy, r + 1.5)); return o.out()

@d("highland", "made", "slate", "fallen_cross")
def _(cx, cy, a, rng):
    """a high cross fallen on its back: the shaft with its moulded edge and a panel of knotwork, the ring joining the
    arms with the four openings pierced through, a boss in the crossing, the arm ends flared; its stepped socket
    stone lies tipped beside the foot, and on some the shaft is snapped across"""
    u = rng.uniform; ff = frame(cx, cy, a); L = u(4.9, 5.9); w = u(0.85, 1.0); th = u(0.6, 0.7); o = Obj("slate"); xc = L * 0.2; A = L * 0.55; R = L * 0.19
    g = U([rect(*ff(-L / 2 + (xc + A / 2 + L / 2) / 2, 0), xc + A / 2 + L / 2, w, a), rect(*ff(xc, 0), w, A, a), ann(*ff(xc, 0), R, R - 0.45, 24),
           rect(*ff(xc + A / 2 - 0.25, 0), 0.5, w * 1.3, a), rect(*ff(xc, A / 2 - 0.25), w * 1.3, 0.5, a), rect(*ff(xc, -A / 2 + 0.25), w * 1.3, 0.5, a)])
    holes = disc(*ff(xc, 0), R - 0.45, 20).difference(U([rect(*ff(xc, 0), w + 0.1, A, a), rect(*ff(xc, 0), A, w + 0.1, a)]))
    g = g.difference(holes)
    snap = rng.random() < 0.35
    if snap: g = g.difference(Polygon([ff(-L * 0.18, -w), ff(-L * 0.14, -w * 0.1), ff(-L * 0.2, w * 0.2), ff(-L * 0.15, w), ff(-L * 0.08, w), ff(-L * 0.1, 0), ff(-L * 0.06, -w)]))
    b = o.up(g, th); o.groove(rect(*ff(-L * 0.12 - xc * 0.2, 0), L * 0.62, w - 0.32, a), b, 0.13, 0.08)
    pan = rect(*ff(-L * 0.12 - xc * 0.2, 0), L * 0.5, w - 0.6, a); sp = 0.34
    lat = U([barl(ff(x - w, -w), ff(x + w, w), 0.12) for x in np.arange(-L, L, sp)] + [barl(ff(x - w, w), ff(x + w, -w), 0.12) for x in np.arange(-L, L, sp)])
    o.cut(lat.intersection(pan), b, 0.09)
    o.cut(U([ann(*ff(xc, 0), R - 0.12, R - 0.3, 24)]), b, 0.07)
    o.st(dome(*ff(xc, 0), w * 0.36, 0.3, n=14), par=b)
    o.cut(U([barl(ff(xc, sv * (w / 2 + 0.15)), ff(xc, sv * (A / 2 - 0.55)), 0.12) for sv in (-1, 1)] + [barl(ff(xc + w / 2 + 0.15, 0), ff(xc + A / 2 - 0.55, 0), 0.12)]), b, 0.08)
    sv = rng.choice((-1, 1)); c = ff(-L / 2 + u(0.4, 0.9), sv * (w / 2 + u(1.0, 1.2))); t = a + u(-0.4, 0.4)
    bs = o.up(xh_rough(c[0], c[1], 1.5, 1.3, t, rng, 0.08), u(0.9, 1.1)); s2 = xon(o, inset(rect(c[0], c[1], 1.5, 1.3, t), 0.22), bs, 0.16)
    o.cut(rect(c[0], c[1], w * 0.6, 0.45, t), s2, 0.45)
    return o.out()

@d("highland", "made", "wood", "caber_and_stone")
def _(cx, cy, a, rng):
    """the heavy events laid out: the caber, a stripped larch trunk tapering to its thin end, sawn square at both,
    knots and drying checks along it; the putting stone, a rounded river boulder; the hammer, its round head on a long
    cane shaft; and the trig board the throwers toe up to"""
    u = rng.uniform; ff = frame(cx, cy, a); L = u(7.4, 8.6); r0 = u(0.46, 0.52); r1 = r0 * u(0.68, 0.75); o = Obj("wood")
    lf = r0 * 0.8; rf = lambda t: r0 + (r1 - r0) * (t + L / 2) / L; hz = lambda t: lf + rf(t) * 1.1 * 0.93
    kn = [u(-L * 0.4, L * 0.4) for _ in range(rng.randint(3, 5))]
    cuts = [(barl(ff(x0, v), ff(x0 + u(0.8, 1.6), v + u(-0.05, 0.05)), 0.16), hz(x0) - 0.1) for x0, v in ((u(-L / 2, L / 2 - 1.6), u(-0.1, 0.1)) for _ in range(rng.randint(2, 3)))]
    cuts += [(ann(*ff(x, u(-0.12, 0.12)), 0.2, 0.08, 8), hz(x) - 0.08) for x in kn]
    o.st(spin(cx, cy, a, L, rf, 1.1, lift=lf), cuts=cuts, foot=rect(cx, cy, L, 2 * r0, a))
    c = ff(u(-2, 2), u(1.7, 2.0)); rs = u(0.7, 0.85); o.st(dome(c[0], c[1], rs, rs * 1.15, sq=u(0.8, 0.9), a=u(0, 3)), "slate", foot=disc(c[0], c[1], rs))
    sv = -1; t = a + u(-0.15, 0.15); hc = ff(u(-1.2, 1.2), sv * u(1.5, 1.8)); f2 = frame(hc[0], hc[1], t); Lh = u(3.0, 3.5)
    o.st(rod([f2(-Lh / 2, 0), f2(Lh / 2, 0)], 0.25, 1.2), foot=rect(hc[0], hc[1], Lh, 0.4, t))
    o.st(dome(*f2(Lh / 2 + 0.25, 0), 0.38, 0.62, n=14), "iron", foot=disc(*f2(Lh / 2 + 0.25, 0), 0.4))
    if rng.random() < 0.6:
        tc = ff(u(-1.5, 1.5), u(2.9, 3.2)); tb = o.up(rect(tc[0], tc[1], u(2.8, 3.2), 0.45, a + u(-0.08, 0.08)), 0.32); o.cut(barl(pol(tc[0], tc[1], 1.3, a), pol(tc[0], tc[1], 1.3, a + math.pi), 0.12), tb, 0.08)
    return o.out()

@d("highland", "made", "wood", "hurdle")
def _(cx, cy, a, rng):
    """a wattle hurdle lying flat: hazel rods woven over and under the upright zales, the sharpened feet of the zales
    standing out along one edge, the thick end sails with the rods twisted round them, the twilly hole left in the
    middle for carrying; on some a second hurdle lies on the first"""
    u = rng.uniform; o = Obj("wood"); L = u(5.3, 6.2); W = u(3.0, 3.4)
    def one(x0, y0, t, z, rng):
        ff = frame(x0, y0, t); nz = int(L / 0.62); xs = [(-L / 2 + 0.25) + i * (L - 0.5) / nz for i in range(nz + 1)]; vs = np.arange(-W / 2 + 0.2, W / 2 - 0.1, 0.36)
        hole = rect(*ff(0, W * 0.18), 0.9, 0.42, t)
        lo = U([barl(ff(-L / 2 + 0.2, v), ff(L / 2 - 0.2, v), 0.3) for v in vs]).difference(hole)
        hi = U([barl(ff(xs[i], v), ff(xs[i + 1], v), 0.3) for j, v in enumerate(vs) for i in range(nz) if (i + j) % 2 == 0]).difference(hole)
        zl = U([barl(ff(x, -W / 2 - 0.05), ff(x, W / 2), 0.3) for x in xs[1:-1]] + [tri(ff(x, -W / 2 - 0.05), ff(x, -W / 2 - 0.5), 0.3) for x in xs[1:-1]])
        sails = U([barl(ff(sx * (L / 2 - 0.15), -W / 2 - 0.3), ff(sx * (L / 2 - 0.15), W / 2 + 0.1), 0.42) for sx in (-1, 1)])
        tw = U([ann(*ff(sx * (L / 2 - 0.15), v), 0.32, 0.12, 8) for sx in (-1, 1) for v in vs[::2]])
        o.up(zl, z + 0.34); o.up(lo, z + 0.3); o.up(hi, z + 0.44); o.up(sails, z + 0.48); o.up(tw, z + 0.52)
    one(cx, cy, a, 0.0, rng)
    if rng.random() < 0.35: one(cx + u(-0.25, 0.25), cy + u(-0.25, 0.25), a + u(-0.05, 0.05), 0.4, rng)
    return o.out([rect(cx, cy, L, W + 1.0, a)])

@d("highland", "made", "slate", "burial_kist", ground=True)
def _(cx, cy, a, rng):
    """a stone kist opened: four split slabs set on edge lining the grave, their tops knocked about, the great
    capstone dragged half off it with cup marks pecked in its face, one side slab tipped in, and the small stones
    of the cairn that covered it scattered round"""
    u = rng.uniform; ff = frame(cx, cy, a); L = u(3.4, 4.0); W = u(2.0, 2.4); t = u(0.42, 0.52); o = Obj("slate")
    o.gcut(rect(cx, cy, L, W, a), 0.7, "pit"); tip = rng.randrange(4) if rng.random() < 0.4 else -1
    sl = [(ff(0, -(W / 2 + t / 2)), L + 2 * t, t), (ff(0, W / 2 + t / 2), L + 2 * t, t), (ff(-(L / 2 + t / 2), 0), t, W), (ff(L / 2 + t / 2, 0), t, W)]
    for i, (c, l, w) in enumerate(sl):
        if i == tip: c = (c[0] + (cx - c[0]) * 0.25, c[1] + (cy - c[1]) * 0.25); g = xh_rough(c[0], c[1], l * 0.9, w * 1.6, a + u(-0.1, 0.1), rng, 0.08); o.up(g, u(0.25, 0.35)); continue
        g = xh_rough(c[0], c[1], l, w, a, rng, 0.06); b = o.up(g, u(0.7, 1.0))
        o.cut(Polygon([pol(c[0], c[1], max(l, w) * u(0.2, 0.45), a + rng.choice((0.0, math.pi)) + u(-0.2, 0.2)), pol(c[0], c[1], max(l, w), a + u(-0.4, 0.4)), pol(c[0], c[1], max(l, w), a + 1.57)]).intersection(g), b, u(0.15, 0.3))
    cc = ff(u(1.9, 2.6), u(-0.6, 0.6)); ct = a + u(-0.4, 0.4); cg = xh_rough(cc[0], cc[1], L * 0.85, W + 1.0, ct, rng, 0.15)
    cp = o.up(cg, u(0.55, 0.65)); o.cut(U([disc(*pol(cc[0], cc[1], u(0, 1.0), u(0, 6.28)), u(0.18, 0.24), 10) for _ in range(rng.randint(3, 6))]), cp, 0.12)
    if rng.random() < 0.5: c0 = pol(cc[0], cc[1], 0.2, ct); o.cut(U([ann(c0[0], c0[1], 0.55, 0.42, 16), disc(c0[0], c0[1], 0.2, 8)]), cp, 0.1)
    for i in range(rng.randint(4, 7)):
        p = pol(cx, cy, u(L * 0.75, L * 0.95), u(0, 6.28)); o.st(rock(p[0], p[1], u(0.3, 0.45), u(0.25, 0.45), rng), foot=disc(p[0], p[1], 0.45))
    return o.out()

@d("highland", "made", "brass", "pot_still")
def _(cx, cy, a, rng):
    """a bothy still: the copper pot with its riveted seams set on a ring of hearth stones with peats under it, the
    lyne arm running down from its head to the worm tub, a coopered tub with its staves and the coil of the worm
    showing in the water, the spout at its foot dripping into a little keg"""
    u = rng.uniform; ff = frame(cx, cy, a); o = Obj("brass"); r = u(1.25, 1.45); h = u(2.4, 2.8)
    prof = [(r * 0.72, 0.0), (r * 0.92, h * 0.18), (r, h * 0.38)] + crv(r, 0.5, h * 0.38, h * 0.82) + [(0.46, h * 0.9), (0.56, h * 0.92), (0.56, h)]
    def zr(rr):
        for (r0, z0), (r1, z1) in zip(prof[3:], prof[4:]):
            if min(r0, r1) <= rr <= max(r0, r1): return z0 + (z1 - z0) * (rr - r0) / ((r1 - r0) or 1)
        return h * 0.4
    o.st(turn(cx, cy, prof, 24), cuts=[(ann(cx, cy, rr + 0.08, rr - 0.08, 24), zr(rr) - 0.12) for rr in (r * 0.8, r * 0.6)] + [(disc(cx, cy, 0.3, 10), h - 0.25)], foot=disc(cx, cy, r))
    for p in ringpts(cx, cy, r + 0.35, rng.randint(8, 10), a, 0.12, rng): o.st(rock(p[0], p[1], u(0.36, 0.46), u(0.4, 0.6), rng), "slate", foot=disc(p[0], p[1], 0.45))
    tc = ff(r + u(1.25, 1.5), u(-0.4, 0.4)); rt = u(1.0, 1.15); ht = u(2.6, 3.0)
    tb = o.up(ann(tc[0], tc[1], rt, rt - 0.24, 24), ht, "wood"); o.cut(U([barl(pol(tc[0], tc[1], rt - 0.3, k * 0.52), pol(tc[0], tc[1], rt + 0.1, k * 0.52), 0.12) for k in range(12)]), tb, 0.1)
    wt = o.up(disc(tc[0], tc[1], rt - 0.22, 20), ht - u(0.35, 0.5), "water")
    xon(o, ann(tc[0], tc[1], rt * 0.62, rt * 0.62 - 0.3, 20).difference(rect(*pol(tc[0], tc[1], rt * 0.6, a + math.pi), 0.6, 0.6, a)), wt, 0.12, "brass")
    t = math.atan2(tc[1] - cy, tc[0] - cx); p0 = pol(cx, cy, 0.2, t); p1 = pol(tc[0], tc[1], rt * 0.45, t + math.pi)
    o.st(xh_bar(p0, p1, 0.27, h - 0.05, ht + 0.25), foot=LineString([p0, p1]).buffer(0.3))
    sp = pol(tc[0], tc[1], rt + 0.3, t + rng.choice((-1, 1)) * 1.3); o.up(rect(sp[0], sp[1], 0.6, 0.3, t), 0.55, "brass")
    kc = pol(sp[0], sp[1], 0.75, t + rng.choice((-1, 1)) * 0.6); o.st(_S()._barrel_side(kc[0], kc[1], t + 1.57, 1.2, 0.5), "wood", foot=rect(kc[0], kc[1], 1.2, 1.0, t + 1.57))
    for i in range(rng.randint(2, 3)): q = pol(cx, cy, r + u(0.9, 1.4), t + math.pi + u(-1.2, 1.2)); o.up(rect(q[0], q[1], 0.7, 0.42, u(0, 3)), 0.32, "earth")
    return o.out()

@d("highland", "made", "wood", "crook_and_plaid")
def _(cx, cy, a, rng):
    """a shepherd's crook laid down: the hazel shank with a knot or two, the brass ferrule at the foot, the carved horn
    head with its collar, the crook curled round and the nose turned out at its tip; beside it his plaid, folded
    in a thick pile with the tartan sett showing, the fringed end hanging loose on the ground and a brooch on it"""
    u = rng.uniform; ff = frame(cx, cy, a); L = u(6.0, 7.4); o = Obj("wood"); rs = 0.26
    o.st(rod([ff(-L / 2, 0), ff(L / 2, 0)], lambda q: rs * (1.25 if q < 0.03 else 1.0) + (0.06 if any(abs(q - k) < 0.015 for k in (0.3, 0.58)) else 0.0), 1.1), foot=rect(cx, cy, L, 0.5, a))
    h0 = ff(L / 2 + 0.05, 0.7); sv = 1; pts = arc(h0[0], h0[1], 0.7, a - 1.5708, a + 2.4, 12)
    o.st(rod(pts, 0.27, 1.1) + rod([pts[-1], pol(pts[-1][0], pts[-1][1], 0.5, a + 2.4 + 1.9)], 0.25, 1.1) + lie(*ff(L / 2 - 0.15, 0), a, 0.3, 0.3), "bone", foot=disc(h0[0], h0[1], 1.0))
    pc = ff(u(-1.6, 1.2), -u(1.9, 2.2)); t = a + u(-0.3, 0.3); f2 = frame(pc[0], pc[1], t); Lp = u(2.6, 3.0); Wp = u(1.8, 2.1)
    tail = Polygon([f2(Lp / 2 - 0.2, -Wp / 2 + 0.1), f2(Lp / 2 + u(1.0, 1.4), -Wp * 0.4 + u(-0.2, 0.2)), f2(Lp / 2 + u(1.0, 1.4), Wp * 0.3 + u(-0.2, 0.2)), f2(Lp / 2 - 0.2, Wp / 2 - 0.2)]).buffer(0.15)
    tl = o.up(tail, 0.18, "cloth"); te = Lp / 2 + 1.0
    o.cut(U([barl(f2(te, v), f2(te + 0.6, v + 0.02), 0.1) for v in np.arange(-Wp * 0.35, Wp * 0.28, 0.2)]), tl, 0.1)
    b1 = o.up(rrect(pc[0], pc[1], Lp, Wp, t, 0.25), 0.26, "cloth"); b2 = xon(o, rrect(*f2(u(-0.1, 0.1), u(-0.08, 0.08)), Lp - 0.25, Wp - 0.25, t, 0.25), b1, 0.24, "cloth")
    b3 = xon(o, rrect(*f2(u(-0.1, 0.1), u(-0.08, 0.08)), Lp - 0.5, Wp - 0.5, t, 0.25), b2, 0.24, "cloth")
    s0 = u(0, 0.4); sett = U([barl(f2(x, -Wp), f2(x, Wp), 0.11) for x0 in np.arange(-Lp / 2 + s0, Lp / 2, 0.7) for x in (x0, x0 + 0.2)] + [barl(f2(-Lp, v), f2(Lp, v), 0.11) for v0 in np.arange(-Wp / 2 + s0 * 0.6, Wp / 2, 0.7) for v in (v0, v0 + 0.2)])
    o.cut(sett.intersection(rrect(pc[0], pc[1], Lp - 0.5, Wp - 0.5, t, 0.25)), b3, 0.06)
    o.cut(U([barl(f2(-Lp / 2 + 0.3, v), f2(Lp / 2 - 0.3, v), 0.1) for v in (-Wp / 2 + 0.38,)]), b2, 0.05)
    bc = f2(u(-0.6, 0.6), u(-0.3, 0.3)); xon(o, ann(bc[0], bc[1], 0.34, 0.16, 14), b3, 0.1, "brass")
    return o.out()


_OWN = set(globals())
