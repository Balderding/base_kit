"""Third drawing, urban: the street details redrawn at their true size beside a 32 mm figure (1 m = 18 mm printed).
A bench has its seat at knee height and its back slats rising behind it, a bath stands as high as a real one, a lamp
post has fallen at its real thickness with its lantern. Made things are built in flat layers (Obj) so their seams,
slots and bolts stay crisp; posts and bins are round. Nothing overhangs: what stands on legs stands solid."""
import math
from sd_reg import d

def xu_bar(p, q, rf, k=0.5):
    """a round bar from p to q lying on the ground, solid underneath (upright sides up to the axis, k radii off the
    ground, then round over the top): rf is its radius, a number or a function of 0..1 along it"""
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

def xu_chunk(o, x, y, r, h, rng, kind="concrete"):
    """a broken lump of pavement or concrete: a jagged slab with a crack across it"""
    g = stone(x, y, r, rng, 0.8, 7); b = o.up(g, h, kind)
    t = rng.uniform(0, 3.14); o.cut(wavy(pol(x, y, r, t), pol(x, y, r, t + math.pi), rng, 0.2, 3, 0.14).intersection(inset(g, 0.25)), b, 0.12)
    return b


@d("urban", "made", "iron", "street_lamp")
def _(cx, cy, a, rng):
    """a cast-iron street lamp brought down: the fluted base with its rings, torn out with a slab of pavement, the
    shaft at its real thickness tapering up with a collar, the ladder bar across its neck, and the glazed lantern
    lying on its side (its frame, the panes broken out of some, the roof and finial); glass about it"""
    u = rng.uniform; ff = frame(cx, cy, a); L = u(8.0, 11.0); o = Obj("iron"); x0 = -L / 2; xh = L / 2 - 0.9
    xu_chunk(o, *ff(x0 + 0.15, u(-0.2, 0.2)), u(0.95, 1.1), u(0.45, 0.6), rng)
    Lb = 1.4; band = lambda x: 0.06 if any(abs(x - y) < 0.08 for y in (0.25, 0.55, 1.15)) else 0.0
    o.st(xu_bar(ff(x0, 0), ff(x0 + Lb, 0), lambda s: (0.62 if s < 0.6 else 0.62 - 0.25 * (s - 0.6) / 0.4) + band(s * Lb), 0.45), foot=rect(*ff(x0 + Lb / 2, 0), Lb, 1.4, a))
    Ls = xh - (x0 + Lb) + 0.1
    o.st(xu_bar(ff(x0 + Lb - 0.05, 0), ff(xh, 0), lambda s: 0.3 - 0.08 * s + (0.07 if abs(s - 0.38) < 0.03 or s > 0.93 else 0.0), 0.45), foot=rect(*ff(x0 + Lb + Ls / 2, 0), Ls, 0.8, a))
    xl = xh - u(0.7, 1.0); o.st(ext(barl(ff(xl, -0.8), ff(xl, 0.8), 0.32), 0.32), foot=rect(*ff(xl, 0), 0.4, 1.7, a))
    c = ff(xh + 0.75, 0); t = a + u(-0.15, 0.15); f2 = frame(c[0], c[1], t); lw = u(1.25, 1.45)
    lg = rect(c[0], c[1], 1.3, lw, t); lb = o.up(lg, u(1.2, 1.4))
    panes = [rect(*f2(0, sv * lw * 0.22), 0.85, lw * 0.32, t) for sv in (-1, 1)]
    keep = [g for g in panes if rng.random() < 0.55]
    if keep: o.cut(U(keep), lb, 0.75)
    o.cut(U([g for g in panes if g not in keep]), lb, 0.12) if len(keep) < 2 else None
    rf_ = o.up(Polygon([f2(0.62, -lw / 2 - 0.05), f2(1.1, -lw * 0.2), f2(1.1, lw * 0.2), f2(0.62, lw / 2 + 0.05)]), 1.0); o.on(rect(*f2(0.95, 0), 0.3, lw * 0.32, t), rf_, 0.15)
    o.st(xu_bar(f2(1.05, 0), f2(1.55, 0), lambda s: 0.24 if s < 0.6 else 0.3, 0.6), foot=disc(*f2(1.3, 0), 0.4))
    for i in range(rng.randint(2, 4)):
        p = ff(xh + u(0.2, 1.6), rng.choice((-1, 1)) * u(1.0, 1.4)); o.up(star(p[0], p[1], u(0.32, 0.45), 0.16, 3, u(0, 6.28)), 0.25, "glass")
    return rect(*ff(0.8, 0), L + 3.2, 3.0, a), o.L

@d("urban", "made", "iron", "park_bench")
def _(cx, cy, a, rng):
    """a park bench at its real height: the seat at the knee with its planks, the back slats stepping up behind it
    at a lean, the cast-iron ends rising to the arm rests with a scroll at the front; a plank missing or split on
    some, a plaque on the top rail of others"""
    u = rng.uniform; ff = frame(cx, cy, a); L = u(4.9, 5.8); o = Obj("wood"); hs = u(1.3, 1.4); xe = L / 2 - 0.2
    sg = rect(*ff(0, 0.3), L - 0.5, 1.3, a); s = o.up(sg, hs)
    gv = [barl(ff(-L, v), ff(L, v), 0.13) for v in (0.0, 0.32, 0.62)]
    miss = rng.random() < 0.3
    o.cut(U(gv), s, 0.12)
    if miss: x1 = u(-L * 0.3, L * 0.1); o.cut(rect(*ff(x1, 0.78), u(1.0, 2.0), 0.34, a), s, 0.5)
    nb = rng.choice((3, 3, 4)); vb = [-0.42 - 0.24 * i for i in range(nb)]; Hb = u(1.15, 1.3); tops = [hs + 0.3 + Hb * (i + 1) / nb for i in range(nb)]
    last = None
    for v, h in zip(vb, tops): last = o.up(rect(*ff(0, (v + vb[-1] - 0.14) / 2), L - 0.5, v - vb[-1] + 0.26, a), h)
    if rng.random() < 0.5: o.on(rect(*ff(u(-0.4, 0.4), vb[-1] + 0.02), 0.8, 0.3, a), last, 0.08, "brass")
    ha = u(1.85, 2.0)
    for sx in (-1, 1):
        g = U([rect(*ff(sx * xe, (0.95 + vb[-1] - 0.14) / 2), 0.38, 0.95 - vb[-1] + 0.14, a), disc(*ff(sx * xe, 0.95), 0.27, 10)])
        e = o.up(g, ha, "iron"); o.on(disc(*ff(sx * xe, 0.95), 0.2, 8), e, 0.12, "iron")
        o.up(rect(*ff(sx * xe, vb[-1] - 0.05), 0.38, 0.3, a), tops[-1] + 0.2, "iron")
    return o.out()

@d("urban", "made", "iron", "bathtub")
def _(cx, cy, a, rng):
    """a cast-iron bath dragged out into the street at its real height: the rolled rim, the deep basin with the plug
    hole in its floor, the wider end with two taps and the spout, four claw feet standing out at the corners; rubble
    thrown into some"""
    u = rng.uniform; ff = frame(cx, cy, a); L = u(4.6, 5.6); W = u(2.2, 2.6); h = u(1.75, 1.95); o = Obj("iron")
    og = U([rect(cx, cy, L - W, W, a), disc(*ff(-(L - W) / 2, 0), W / 2, 24), disc(*ff((L - W) / 2, 0), W / 2, 24)])
    for sx in (-1, 1):
        for sy in (-1, 1):
            p = ff(sx * (L / 2 - 0.75), sy * (W / 2 - 0.05)); fg = ell(p[0], p[1], 0.42, 0.34, a, 10); f = o.up(fg, 0.55)
            o.cut(U([barl(pol(p[0], p[1], 0.05, a + sy * 1.5708 + k), pol(p[0], p[1], 0.5, a + sy * 1.5708 + k), 0.12) for k in (-0.5, 0.5)]), f, 0.1)
    b = o.up(og, h); o.on(og.difference(inset(og, 0.32)), b, 0.12)
    sv = rng.choice((-1, 1)); inner = U([rect(*ff(-sv * 0.25, 0), L - W - 0.5, W - 0.85, a), disc(*ff(-sv * 0.25 - (L - W - 0.5) / 2, 0), W / 2 - 0.42, 20), disc(*ff(-sv * 0.25 + (L - W - 0.5) / 2, 0), W / 2 - 0.42, 20)])
    o.cut(inner, b, h - 0.45)
    tx = sv * (L / 2 - 0.42)
    o.on(U([disc(*ff(tx, v), 0.22, 8) for v in (-0.5, 0.5)]), b, 0.38); o.on(capl(ff(tx, 0), ff(tx - sv * 0.55, 0), 0.26), b, 0.3)
    o.on(U([barl(ff(tx - 0.15, v), ff(tx + 0.15, v), 0.16) for v in (-0.5, 0.5)]), b, 0.48)
    if rng.random() < 0.3:
        for i in range(rng.randint(2, 4)): p = ff(u(-L * 0.25, L * 0.2), u(-0.4, 0.4)); o.up(stone(p[0], p[1], u(0.4, 0.55), rng, 0.7, 5), u(0.8, 1.1), "concrete")
    return og.buffer(0.4), o.L

@d("urban", "made", "iron", "post_box")
def _(cx, cy, a, rng):
    """a pillar box knocked flat, at its real thickness: the plinth torn out with a lump of pavement, the round body
    with its door outline and the cipher on the door, the letter slot under its hood, the collection plate, the
    cap overhanging the body and domed over the end; letters spilled out of it"""
    u = rng.uniform; ff = frame(cx, cy, a); L = u(3.4, 4.2); r = u(0.92, 1.02); o = Obj("iron"); k = 0.65; top = (1 + k) * r
    def rf(s):
        if s < 0.1: return r * 1.1
        if s < 0.8: return r
        if s < 0.86: return r * 1.1
        return r * max(0.45, math.sqrt(max(0.0, 1 - ((s - 0.86) / 0.16) ** 2)) * 1.1)
    ps = xu_bar(ff(-L / 2, 0), ff(L / 2, 0), rf, k)
    xs = L * 0.25; ps += ext(rect(*ff(xs + 0.28, 0), 0.24, 1.15, a), top + 0.08)
    ps += ext(rect(*ff(-L * 0.12 + L * 0.27, 0), 0.5, 0.7, a), top + 0.06)
    door = rect(*ff(-L * 0.12, 0), L * 0.4, 1.0 * r, a)
    cuts = [(rect(*ff(xs, 0), 0.26, 1.0, a), top - 0.32), (door.exterior.buffer(0.07), top - 0.24), (ann(*ff(-L * 0.12, 0), 0.3, 0.17, 10), top - 0.1), (U([barl(ff(x, -2), ff(x, 2), 0.14) for x in (-L * 0.4, L * 0.3)]), top - 0.3)]
    o.st(ps, cuts=cuts, foot=rect(cx, cy, L, 2 * r, a))
    xu_chunk(o, *ff(-L / 2 + 0.1, u(-0.2, 0.2)), u(0.95, 1.1), u(0.5, 0.65), rng)
    for i in range(rng.randint(1, 3)): o.up(rect(*ff(L / 2 + u(0.9, 1.8), u(-1.2, 1.2)), 1.2, 0.85, u(0, 3)), 0.25, "paper")
    return rect(*ff(0.8, 0), L + 3.6, 2 * r + 1.6, a), o.L

@d("urban", "debris", "concrete", "kerbstones")
def _(cx, cy, a, rng):
    """kerbstones torn up and thrown down in a row: each the real kerb section, the rounded nosing a step down along
    its road side, ragged at a broken end, a crack across some, lumps of bedding mortar still stuck to them"""
    u = rng.uniform; ff = frame(cx, cy, a); n = rng.randint(3, 5); o = Obj("concrete")
    for i in range(n):
        c = ff((i - (n - 1) / 2) * 2.1, u(-0.45, 0.45)); t = a + u(-0.3, 0.3); f2 = frame(c[0], c[1], t); Lk = 1.9; Wk = u(0.95, 1.05); h = u(0.7, 0.85)
        e0 = -Lk / 2 if rng.random() < 0.5 else Lk / 2; brk = rng.random() < 0.45
        pts = [f2(-Lk / 2, -Wk / 2), f2(Lk / 2, -Wk / 2), f2(Lk / 2, Wk / 2), f2(-Lk / 2, Wk / 2)]
        if brk:
            sx = 1 if e0 > 0 else -1; jag = [f2(sx * (Lk / 2 - u(0.0, 0.35)), v) for v in np.linspace(Wk / 2, -Wk / 2, 4)]
            pts = ([f2(-Lk / 2, -Wk / 2)] + jag[::-1] + [f2(-Lk / 2, Wk / 2)]) if sx > 0 else ([f2(Lk / 2, -Wk / 2), f2(Lk / 2, Wk / 2)] + jag)
        g = Polygon(pts).buffer(0)
        if not g.is_valid or g.area < 0.5: g = rect(c[0], c[1], Lk, Wk, t)
        sv = rng.choice((-1, 1)); b = o.up(g, h - 0.2); top = o.on(g.intersection(rect(*f2(0, -sv * 0.12), Lk + 1, Wk - 0.24, t)), b, 0.2)
        if rng.random() < 0.5: o.cut(wavy(f2(u(-0.5, 0.5), -Wk), f2(u(-0.5, 0.5), Wk), rng, 0.15, 3, 0.13), top, 0.14)
        if rng.random() < 0.5: p = f2(u(-0.5, 0.5), sv * (Wk / 2 + 0.15)); o.up(stone(p[0], p[1], u(0.3, 0.42), rng, 0.6, 5), u(0.3, 0.42), "earth")
    return rect(cx, cy, n * 2.1 + 0.4, 2.6, a), o.L

@d("urban", "made", "wood", "window_frame")
def _(cx, cy, a, rng):
    """a sash window torn out and lying flat: the box frame with the sill standing out at its foot, the two sashes
    meeting at the thicker rail with the lock on it, glazing bars dividing the panes, shards of glass still stuck in
    a corner of some, the sash cords trailing out; broken glass lying about"""
    u = rng.uniform; ff = frame(cx, cy, a); L = u(3.6, 4.6); W = u(2.8, 3.4); R = rect(cx, cy, L, W, a); o = Obj("wood")
    sv = rng.choice((-1, 1)); o.up(rect(*ff(0, sv * (W / 2 + 0.05)), L + 0.6, 0.6, a), 0.55)
    fr = o.up(R.difference(inset(R, 0.42)), 0.42); o.cut(inset(R, 0.21).exterior.buffer(0.06), fr, 0.1)
    o.up(rect(cx, cy, L - 0.6, 0.42, a), 0.48)
    o.on(disc(cx, cy, 0.24, 8), o.up(rect(cx, cy, 0.62, 0.42, a), 0.48), 0.12, "brass")
    nb = rng.choice((1, 2)); bars = [rect(*ff(x, 0), 0.26, W - 0.6, a) for x in np.linspace(-L / 2, L / 2, nb + 2)[1:-1]]
    o.up(U(bars), 0.32)
    pane = inset(R, 0.42).difference(U(bars + [rect(cx, cy, L, 0.42, a)]).buffer(0.01))
    for q in polys_of(pane):
        if rng.random() < 0.4:
            m = q.centroid; bx = q.bounds; corner = (bx[0] if rng.random() < 0.5 else bx[2], bx[1] if rng.random() < 0.5 else bx[3])
            sh = Polygon([corner, (corner[0] + (m.x - corner[0]) * u(0.4, 0.8), corner[1] + (m.y - corner[1]) * u(0.1, 0.3)), (corner[0] + (m.x - corner[0]) * u(0.1, 0.3), corner[1] + (m.y - corner[1]) * u(0.4, 0.8))]).buffer(0.12).intersection(q.buffer(0.1))
            if not sh.is_empty and sh.area > 0.2: o.up(sh, 0.25, "glass")
    for s2 in (-1, 1):
        if rng.random() < 0.6: p = ff(s2 * L / 2, u(-W * 0.3, W * 0.3)); e = ff(s2 * (L / 2 + u(0.4, 0.8)), u(-W * 0.4, W * 0.4)); o.st(rod(wiggle(p, e, rng, 0.2, 3), 0.18, 1.0), "rope", foot=LineString([p, e]).buffer(0.3))
    for s2 in (-1, 1):
        for i in range(rng.randint(0, 2)): p = ff(u(-L, L) * 0.4, s2 * (W / 2 + u(0.8, 1.4))); o.up(star(p[0], p[1], u(0.35, 0.5), 0.17, 3, u(0, 6.28)), 0.22, "glass")
    return rect(cx, cy, L + 1.8, W + 3.6, a), o.L

@d("urban", "made", "wood", "crossarm")
def _(cx, cy, a, rng):
    """a telegraph crossarm come down with the line: the timber arm with its bolts, the steel pins along it each
    carrying a glass insulator (the skirt, the neck the wire is tied round, the domed crown), the snapped stub of
    the pole still bolted on with its two braces, the wires trailing away"""
    u = rng.uniform; ff = frame(cx, cy, a); L = u(5.0, 6.5); o = Obj("wood"); foot = [rect(cx, cy, L, 1.2, a)]
    if rng.random() < 0.65:
        sv = rng.choice((-1, 1)); lp = u(1.6, 2.2); r = u(0.5, 0.58)
        o.st(xu_bar(ff(0, sv * 0.25), ff(u(-0.3, 0.3), sv * (0.25 + lp)), lambda s: r * (1.0 if s < 0.85 else 0.85), 0.35), foot=rect(*ff(0, sv * (0.25 + lp / 2)), 2 * r, lp, a))
        o.st(ext(U([barl(ff(sx * L * 0.28, sv * 0.2), ff(sx * 0.35, sv * (0.25 + lp * 0.6)), 0.3) for sx in (-1, 1)]), 0.3), "iron", foot=rect(*ff(0, sv * 0.8), L * 0.6, 1.4, a))
    ha = u(0.42, 0.48); arm = o.up(rect(cx, cy, L, 0.62, a), ha)
    o.studs([ff(x, 0) for x in (-0.25, 0.25)], arm, 0.16, 0.1, "iron", 6)
    xs = [-L * 0.42, -L * 0.17, L * 0.17, L * 0.42] if rng.random() < 0.6 else [-L * 0.38, 0.0, L * 0.38]
    for x in xs:
        c = ff(x, 0)
        if rng.random() < 0.15: o.studs([c], arm, 0.14, 0.2, "iron", 6); continue
        o.st(turn(c[0], c[1], [(0.34, 0.0), (0.34, ha + 0.16), (0.24, ha + 0.22), (0.24, ha + 0.32), (0.28, ha + 0.36)] + crv(0.28, 0.12, ha + 0.36, ha + 0.58, n=4), 12), "glass", foot=disc(c[0], c[1], 0.34))
        if rng.random() < 0.6: s2 = rng.choice((-1, 1)); e = ff(x + u(-1.2, 1.2), s2 * u(2.0, 2.9)); o.st(rod(wiggle(ff(x, s2 * 0.35), e, rng, 0.5, 4), 0.2, 1.0), "wire", foot=disc(e[0], e[1], 0.3)); foot.append(disc(e[0], e[1], 0.3))
    return hull(foot), o.L

@d("urban", "marks", "seam", "chalk_outline")
def _(cx, cy, a, rng):
    """the chalk outline where a body lay: head, shoulders, one arm flung out and one bent, the hands, the legs
    apart with the feet turned out; a dark stain by the head and the folded evidence markers set round it"""
    u = rng.uniform; ff = frame(cx, cy, a); k = u(0.85, 1.1); sv = rng.choice((-1, 1))
    tor = hull([ell(*ff(0.55 * k, 0), 0.55 * k, 0.85 * k, a, 14), ell(*ff(-0.75 * k, 0), 0.5 * k, 0.65 * k, a, 14)])
    hd = disc(*ff(1.9 * k, u(-0.25, 0.25)), 0.6 * k, 14)
    e1 = ff(u(1.0, 1.5) * k, sv * 1.6 * k); h1 = ff(u(1.9, 2.4) * k, sv * u(2.0, 2.4) * k)
    arm1 = U([capl(ff(1.0 * k, sv * 0.7 * k), e1, 0.48 * k), capl(e1, h1, 0.42 * k), disc(h1[0], h1[1], 0.32 * k, 10)])
    h2 = ff(u(0.3, 0.9) * k, -sv * u(1.9, 2.3) * k); arm2 = U([capl(ff(0.9 * k, -sv * 0.7 * k), h2, 0.48 * k), disc(h2[0], h2[1], 0.32 * k, 10)])
    legs = []
    for s2 in (-1, 1):
        kn = ff(-u(1.8, 2.1) * k, s2 * u(0.35, 0.8) * k); ft = ff(-u(2.9, 3.4) * k, s2 * u(0.5, 1.3) * k)
        legs += [capl(ff(-1.0 * k, s2 * 0.3 * k), kn, 0.58 * k), capl(kn, ft, 0.5 * k), ell(*pol(ft[0], ft[1], 0.15, a + s2 * 1.0), 0.28 * k, 0.45 * k, a + s2 * 0.5, 10)]
    body = U([tor, hd, capl(ff(1.2 * k, 0), ff(1.5 * k, 0), 0.4 * k), arm1, arm2] + legs)
    g = body.buffer(0.16).difference(body.buffer(-0.12))
    lay = [("gcut", g, 0.22, "seam")]
    st = blob(*ff(2.0 * k, -sv * u(0.9, 1.3) * k), u(0.6, 0.8), rng, 4)
    if rng.random() < 0.7: lay.append(("gcut", st.difference(body.buffer(0.3)), 0.08, "seam"))
    for i in range(rng.randint(2, 3)):
        p = ff(u(-2.4, 2.0) * k, rng.choice((-1, 1)) * u(1.3, 1.7) * k)
        if body.buffer(0.5).contains(Point(p)): continue
        lay.append(ST(ridge(p[0], p[1], a + u(0, 3.14), 0.6, 0.62, 0.52), "paper"))
    return body.convex_hull, lay

@d("urban", "made", "iron", "road_sign")
def _(cx, cy, a, rng):
    """a road sign knocked down: the post bent where it was hit and torn out with its lump of concrete, the plate
    lying face up with its raised border and its sign (STOP in raised letters, a warning mark, a pointing arrow) or
    face down showing its channel rails and clamps"""
    u = rng.uniform; ff = frame(cx, cy, a); L = u(4.6, 6.4); k = rng.randint(0, 2); c = ff(L / 2, 0); R = u(1.4, 1.8); o = Obj("iron")
    tp = a + u(0, 1) if k == 1 else a + u(-0.4, 0.4)
    plate = ngon(c[0], c[1], R, 8, a + 0.39) if k == 0 else ngon(c[0], c[1], R * 1.15, 3, tp) if k == 1 else rect(c[0], c[1], R * 2.2, R * 1.2, tp)
    bend = ff(-L * 0.1, u(-0.8, 0.8)); pts = [ff(-L / 2, 0), bend, ff(L / 2 - 0.3, 0)]
    xu_chunk(o, *ff(-L / 2 - 0.2, 0), u(0.75, 0.9), u(0.85, 1.05), rng)
    o.st(rod(pts, 0.24, 1.25), foot=LineString(pts).buffer(0.3))
    b = o.up(plate, 0.3)
    if rng.random() < 0.7:
        rim = o.on(plate.difference(inset(plate, 0.28)), b, 0.14)
        f2 = frame(c[0], c[1], a if k != 2 else tp)
        if k == 0: o.on(U([rect(*f2(0, (i - 1.5) * 0.52), 0.85, 0.34, a) for i in range(4)]), b, 0.14)
        elif k == 1:
            tc = c; o.on(U([barl(pol(tc[0], tc[1], 0.1, tp + math.pi), pol(tc[0], tc[1], 0.62, tp + math.pi), 0.28), disc(*pol(tc[0], tc[1], 0.38, tp), 0.17, 8)]), b, 0.14)
        else: o.on(U([barl(f2(-R * 0.75, 0), f2(R * 0.3, 0), 0.4), Polygon([f2(R * 0.25, -0.55), f2(R * 0.85, 0), f2(R * 0.25, 0.55)])]), b, 0.14)
    else:
        f2 = frame(c[0], c[1], a); o.on(U([rect(*f2(0, v), R * 1.8, 0.32, a).intersection(inset(plate, 0.2)) for v in (-0.45, 0.45)]), b, 0.2)
        o.studs([f2(0, v) for v in (-0.45, 0.45)], b, 0.2, 0.32)
    return o.out([plate, rect(cx, cy, L, 1.8, a)])

@d("urban", "made", "tin", "dustbin")
def _(cx, cy, a, rng):
    """a galvanised dustbin of the real size: knocked over on its side with its pressed hoops and the side handles,
    the mouth open with the rubbish spilling out of it, the lid rolled off (its rim, the ribs and the grip); or still
    standing, its lid knocked off and lying beside it, rubbish strewn between"""
    u = rng.uniform; ff = frame(cx, cy, a); L = u(3.0, 3.6); r = u(1.05, 1.18); o = Obj("tin")
    if rng.random() < 0.65:
        c = ff(L / 2 + u(1.4, 2.2), u(-1.5, 1.5))
        hoop = lambda s: 0.07 if any(abs(s - y) < 0.035 for y in (0.18, 0.5, 0.82)) else 0.0
        ps = xu_bar(ff(-L / 2, 0), ff(L / 2, 0), lambda s: r * (0.9 + 0.1 * s) + hoop(s) + (0.06 if s > 0.96 else 0.0), 0.55)
        ps += [s for sv in (-1, 1) for s in ext(ann(*ff(L * 0.15, sv * (r + 0.12)), 0.34, 0.14, 10).difference(rect(*ff(L * 0.15, 0), 2, 2 * r, a)), r * 0.7)]
        o.st(ps, cuts=[(U([rect(*ff(L / 2 - 0.35, 0), 0.9, 2 * r * 0.72, a)]), r * 0.85)], foot=rect(cx, cy, L, 2 * r + 0.3, a))
        for i in range(rng.randint(2, 4)):
            p = ff(L / 2 + u(0.1, 1.0), u(-1.0, 1.0)); k = rng.random()
            if k < 0.4: o.up(rect(p[0], p[1], u(0.7, 1.0), u(0.5, 0.7), u(0, 3)), 0.22, "paper")
            elif k < 0.7: o.st(xu_bar(p, pol(p[0], p[1], 0.75, u(0, 6.28)), 0.3, 0.6), foot=disc(p[0], p[1], 0.8))
            else: o.up(stone(p[0], p[1], u(0.35, 0.5), rng, 0.7, 6), u(0.3, 0.45), "sack")
    else:
        b0 = ff(-L * 0.55, 0); c = ff(L * 0.58 + u(-0.2, 0.2), u(-0.6, 0.6)); hb = u(3.0, 3.3)
        b = o.up(disc(b0[0], b0[1], r, 24), hb); o.on(ann(b0[0], b0[1], r - 0.05, r - 0.35, 24), b, 0.12); o.cut(disc(b0[0], b0[1], r - 0.42, 20), b, 0.8)
        for i in range(rng.randint(2, 3)):
            p = pol(b0[0], b0[1], u(r + 0.8, r + 1.8), a + u(-0.8, 0.8)); o.up(rect(p[0], p[1], u(0.6, 0.9), u(0.45, 0.6), u(0, 3)), 0.22, "paper")
    lr = r + 0.12; lid = o.st(turn(c[0], c[1], [(lr, 0.0), (lr, 0.22), (lr - 0.2, 0.26)] + crv(lr - 0.2, 0.55, 0.26, 0.6, n=4), 24), foot=disc(c[0], c[1], lr))
    tl = a + u(0, 3.14); o.st(ext(U([barl(pol(c[0], c[1], 0.62, tl), pol(c[0], c[1], 0.62, tl + math.pi), 0.3)] + [disc(*pol(c[0], c[1], 0.62, tl + k * math.pi), 0.22, 8) for k in (0, 1)]), 0.85), foot=disc(c[0], c[1], 0.7))
    return hull([rect(cx, cy, L, 2 * r + 0.3, a), disc(c[0], c[1], r + 0.3)]), o.L

@d("urban", "made", "tin", "hubcap_and_bumper")
def _(cx, cy, a, rng):
    """a chrome bumper torn off a car, its brackets bent at the ends: the rubber strip along it, two over-riders,
    the number plate still bolted on with its letters; and a hubcap rolled away (rim, the ring of slots, the dished
    middle with its badge), dented on some"""
    u = rng.uniform; ff = frame(cx, cy, a); R = u(1.1, 1.4); c = ff(u(-1.5, 1.5), 2.4); L = u(5.0, 6.5); o = Obj("tin")
    e1 = -0.7; e2 = -0.7 * u(0.2, 2.0); path = [ff(-L / 2, e1), ff(-L * 0.3, 0), ff(L * 0.3, 0), ff(L / 2, e2)]
    b = o.up(line(path, 0.62), 0.6); o.cut(barl(ff(-L * 0.3, 0), ff(L * 0.3, 0), 0.14), b, 0.1)
    o.up(U([rrect(*ff(sx * L * 0.2, -0.05), 0.5, 0.85, a, 0.18) for sx in (-1, 1)]), 0.82)
    if rng.random() < 0.75:
        xp = u(-0.4, 0.4); pg = rect(*ff(xp, -0.85), 1.8, 0.62, a); p = o.up(pg, 0.3, "paper"); 
        o.on(U([rect(*ff(xp - 0.66 + 0.22 * i, -0.85), 0.14, 0.32, a) for i in range(7) if i != 3]), p, 0.08)
    o.cut(U([disc(*ff(sx * L * 0.42, 0), 0.12, 6) for sx in (-1, 1)]), b, 0.15)
    h = o.up(disc(c[0], c[1], R, 28), 0.3); o.on(disc(c[0], c[1], R - 0.2, 26), h, 0.12)
    m = o.on(disc(c[0], c[1], R * 0.48, 18), h, 0.32); o.cut(ann(c[0], c[1], R * 0.3, R * 0.3 - 0.12, 14), m, 0.08)
    o.cut(U([barl(pol(c[0], c[1], R * 0.55, t), pol(c[0], c[1], R * 0.82, t), 0.22) for t in np.arange(0, 6.2, 6.2832 / rng.choice((6, 8, 10)))]), h, 0.22)
    if rng.random() < 0.4: o.cut(ell(*pol(c[0], c[1], R * 0.75, u(0, 6.28)), 0.4, 0.25, u(0, 3), 8), h, 0.18)
    return hull([rect(cx, cy, L, 2.0, a), disc(c[0], c[1], R)]), o.L

@d("urban", "made", "leather", "suitcase")
def _(cx, cy, a, rng):
    """a suitcase dropped flat, or two stacked: the lid with its piped edge, corner caps, the two locks and the
    handle on the front edge, a buckled strap round it, a luggage label and stickers; a shoe lost beside it"""
    u = rng.uniform; ff = frame(cx, cy, a); L = u(2.8, 3.4); W = u(1.9, 2.3); h = u(0.95, 1.1); o = Obj("leather")
    c = ff(u(-1, 1), W / 2 + 1.3); bs = u(0, 6.28)
    def case(x, y, t, L, W, h, par=None):
        f2 = frame(x, y, t); g = rrect(x, y, L, W, t, 0.25)
        b = o.up(g, h) if par is None else o.on(g, par, h)
        o.on(g.difference(inset(g, 0.2)), b, 0.1); o.corners(f2, L - 0.1, W - 0.1, b, t, 0.5, 0.14, "brass")
        o.tabs(f2, [-L * 0.3, L * 0.3], -W / 2 + 0.38, b, t, 0.36, 0.32, 0.16, "brass")
        if rng.random() < 0.55: o.straps(f2, [L * u(-0.15, 0.15)], W - 1.4, b, t, 0.36, 0.1)
        if rng.random() < 0.6: p = f2(u(-L * 0.25, L * 0.25), u(0.0, W * 0.2)); o.on(rrect(p[0], p[1], 0.55, 0.38, t + u(-0.5, 0.5), 0.08), b, 0.08, "paper")
        return b
    case(cx, cy, a, L, W, h)
    o.up(U([capl(ff(-0.42, -W / 2 - 0.22), ff(0.42, -W / 2 - 0.22), 0.3), disc(*ff(-0.42, -W / 2 - 0.1), 0.2, 8), disc(*ff(0.42, -W / 2 - 0.1), 0.2, 8)]), h * 0.75)
    if rng.random() < 0.4: case(*ff(u(-0.15, 0.15), u(-0.1, 0.1)), a + u(-0.18, 0.18), L * 0.78, W * 0.78, u(0.7, 0.85), 0)
    f3 = frame(c[0], c[1], bs); sole = hull([ell(*f3(0.25, 0), 0.6, 0.38, bs, 12), ell(*f3(-0.55, 0), 0.32, 0.3, bs, 10)])
    sb = o.up(sole, 0.2, "tar"); up_ = o.on(hull([ell(*f3(0.1, 0), 0.5, 0.3, bs, 12), disc(*f3(-0.5, 0), 0.24, 10)]), sb, 0.36)
    o.cut(ell(*f3(-0.25, 0), 0.24, 0.16, bs, 8), up_, 0.2)
    return hull([rect(cx, cy, L, W + 0.8, a), disc(c[0], c[1], 1.0)]), o.L

@d("urban", "made", "concrete", "storm_drain", ground=True)
def _(cx, cy, a, rng):
    """a kerb-side drain: the kerb with its nosing and the dark throat of the inlet cut into its front, letters cast
    along its top, the iron grate in the gutter (its frame, the bars and the slots between them, a hinge lug), a
    shallow gutter channel along the kerb and leaves caught on the bars"""
    u = rng.uniform; ff = frame(cx, cy, a); L = u(4.0, 5.2); o = Obj("concrete")
    o.gcut(rect(*ff(0, -0.3), L, 0.8, a), 0.08, "water")
    kg = rect(*ff(0, 0.45), L, 1.1, a); k = o.up(kg, u(0.7, 0.8)); o.on(rect(*ff(0, 0.6), L, 0.75, a), k, 0.14)
    o.cut(rect(*ff(0, 0.05), L * u(0.5, 0.62), 0.42, a), k, 0.5)
    o.cut(U([rect(*ff(x, 0.7), 0.14, 0.32, a) for x in np.arange(-L / 2 + 0.6, -L / 2 + 0.6 + 0.3 * rng.randint(4, 6), 0.3)]), k, 0.08)
    Lg = L * u(0.55, 0.68); gg_ = rect(*ff(0, -1.05), Lg, 1.3, a)
    o.up(gg_.difference(inset(gg_, 0.28)), 0.25, "iron")
    sl = [rect(*ff(x, -1.05), 0.26, 0.82, a) for x in np.arange(-Lg / 2 + 0.5, Lg / 2 - 0.35, 0.48)]
    o.up(inset(gg_, 0.28).difference(U(sl).buffer(0.01)), 0.18 + 0.07, "iron")
    o.gcut(U(sl), 0.7, "slot")
    o.up(rect(*ff(Lg / 2 - 0.1, -1.6), 0.5, 0.3, a), 0.28, "iron")
    for i in range(rng.randint(1, 3)):
        p = ff(u(-Lg / 2, Lg / 2), -1.05 + u(-0.3, 0.3)); o.up(ell(p[0], p[1], 0.36, 0.2, u(0, 3), 8), 0.3, "grass")
    return rect(*ff(0, -0.4), L, 2.9, a), o.L

@d("urban", "made", "brick", "brick_pallet")
def _(cx, cy, a, rng):
    """a pallet of bricks part used: the pallet's deck boards showing round the load, the bricks of the real size
    laid in courses that change direction every course, the top courses taken down in steps so the joints and the
    courses show; loose bricks set down beside it, one broken"""
    u = rng.uniform; ff = frame(cx, cy, a); n = rng.randint(3, 4); m = rng.randint(2, 3); o = Obj("wood")
    Lp, Wp = n * 1.25 + 0.8, m * 0.75 + 0.8; pg = rect(cx, cy, Lp, Wp, a); p = o.up(pg, 0.6)
    Bx, By = Lp - 0.5, Wp - 0.4; bl, bw, ch, gap = 1.05, 0.5, 0.32, 0.1
    o.cut(U([barl(ff(x, -Wp), ff(x, Wp), 0.12) for x in np.arange(-Lp / 2 + 0.45, Lp / 2, 0.55)]).difference(rect(cx, cy, Bx, By, a)), p, 0.12)
    nc = rng.randint(4, 7); xs = u(0.35, 0.65); sx = rng.choice((-1, 1)); sy = rng.choice((-1, 1))
    for k in range(nc):
        top = 0.6 + ch * (k + 1); left = nc - k
        if left == 2: keep = rect(*ff(sx * Bx * (1 - xs) / 2, 0), Bx * xs, By, a)
        elif left == 1: keep = rect(*ff(sx * Bx * (1 - xs * 0.5) / 2, sy * By * 0.25), Bx * xs * 0.5, By * 0.5 + 0.1, a)
        else: keep = rect(cx, cy, Bx + 0.01, By + 0.01, a)
        bricks = []
        if k % 2 == 0:
            for j in range(int(By // bw)):
                for i in range(int(Bx // bl)):
                    bricks.append(rect(*ff(-Bx / 2 + (i + 0.5) * (Bx / int(Bx // bl)), -By / 2 + (j + 0.5) * (By / int(By // bw))), Bx / int(Bx // bl) - gap, By / int(By // bw) - gap, a))
        else:
            for j in range(int(Bx // bw)):
                for i in range(max(1, int(By // bl))):
                    bricks.append(rect(*ff(-Bx / 2 + (j + 0.5) * (Bx / int(Bx // bw)), -By / 2 + (i + 0.5) * (By / max(1, int(By // bl)))), Bx / int(Bx // bw) - gap, By / max(1, int(By // bl)) - gap, a))
        got = [b for b in bricks if b.centroid.within(keep) and (left > 2 or rng.random() > 0.12)]
        if got: o.up(U(got), top, "brick")
    for i in range(rng.randint(1, 3)):
        q = ff(u(-Lp * 0.35, Lp * 0.35), rng.choice((-1, 1)) * (Wp / 2 + u(0.45, 0.7))); t = a + u(-0.6, 0.6)
        o.up(rect(q[0], q[1], bl * (0.55 if i == 1 else 1.0), bw, t), ch + 0.02 if rng.random() < 0.6 else bw, "brick")
    return o.out()


_OWN = set(globals())
