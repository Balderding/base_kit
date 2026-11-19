"""Third drawing, targa: the ice world's details redrawn at their true size beside a 32 mm figure (1 m = 18 mm
printed). Steps have a real rise, a sled carries a load of real crates, sawn ice blocks are as big as real ones and
a dropped supply case stands to a man's knee. Made things are built in flat layers so seams and bolts stay crisp;
ice, snow and bone are rounded. Nothing hangs: ski tips and a sled's nose step up rather than curl over."""
import math
from sd_reg import d

def xt_bar(p, q, rf, k=0.5):
    """a round bar from p to q with flat ends, solid underneath, its axis k radii off the ground"""
    L = math.dist(p, q) or 1.0; m = max(8, int(L / 0.11)); ux, uy = (q[0] - p[0]) / L, (q[1] - p[1]) / L; pt = lambda s, w: (p[0] + ux * s * L - uy * w, p[1] + uy * s * L + ux * w)
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

def xt_drift(o, x, y, r, h, rng):
    """snow blown up against a thing: a low rounded bank"""
    o.st(dome(x, y, r, h, sq=rng.uniform(0.55, 0.8), a=rng.uniform(0, 3.14)), "snow", foot=disc(x, y, r * 0.8))


@d("targa", "made", "alien", "pod_lid")
def _(cx, cy, a, rng):
    """the lid of a sleeper pod thrown off and lying face up: the flanged rim, the shell rising in two steps to its
    crown, the long window in the crown with its bar across and its frame, the hinge knuckles on one side and the
    catch on the other, a row of tell-tales, a crack across some, snow drifted against it"""
    u = rng.uniform; ff = frame(cx, cy, a); L = u(5.0, 6.2); W = u(2.6, 3.2); o = Obj("alien")
    cap = lambda l, w, dx=0.0: hull([disc(*ff(dx - (l - w) / 2, 0), w / 2, 16), disc(*ff(dx + (l - w) / 2 + w * 0.12, 0), w * 0.38, 16)])
    g = cap(L, W); sv = rng.choice((-1, 1))
    kn = o.up(U([rect(*ff(x, sv * (W / 2 - 0.05)), 0.9, 0.9, a) for x in (-L * 0.22, L * 0.14)]), 0.85)
    o.cut(U([barl(ff(x - 0.45, sv * (W / 2 - 0.05)), ff(x + 0.45, sv * (W / 2 - 0.05)), 0.12) for x in (-L * 0.22, L * 0.14)]), kn, 0.08)
    b = o.up(g, 0.5)
    sh = o.on(inset(g, 0.4), b, u(0.38, 0.45)); cr = o.on(inset(g, 0.8), sh, u(0.35, 0.42))
    wn = cap(L * 0.46, W * 0.36, L * 0.1)
    o.cut(wn.difference(rect(*ff(L * 0.1, 0), 0.45, W, a)), cr, 0.28); o.groove(wn.buffer(0.25), cr, 0.16, 0.12)
    o.studs([ff(-L * 0.33, v) for v in (-0.45, 0.0, 0.45)], cr, 0.2, 0.14)
    o.cut(U([barl(ff(x, -W), ff(x, W), 0.14) for x in (-L * 0.2, L * 0.33)]), sh, 0.12)
    o.on(rect(*ff(-L * 0.05, -sv * (W / 2 - 0.3)), 1.0, 0.5, a), b, 0.35)
    if rng.random() < 0.4: o.cut(wavy(ff(u(-L * 0.3, 0), -W), ff(u(-L * 0.3, 0), W * 0.2), rng, 0.3, 4, 0.22), cr, 0.3)
    if rng.random() < 0.5: c = ff(u(-L * 0.4, L * 0.4), -sv * (W / 2 + 0.6)); xt_drift(o, c[0], c[1], u(1.2, 1.7), u(0.6, 0.85), rng)
    return o.out()

@d("targa", "made", "wood", "sled")
def _(cx, cy, a, rng):
    """a hauling sled: two long runners stepping up at the nose to the nose bar, the slatted deck between them with
    stanchions along its sides, and a load of a real height on most (crates with their lids and corner irons, a
    rolled tarpaulin, the lashings over them), the tow rope thrown forward"""
    u = rng.uniform; ff = frame(cx, cy, a); L = u(6.0, 7.5); W = u(2.4, 2.9); o = Obj("wood")
    rn = [line([ff(-L / 2, sv * W / 2), ff(L / 2 - 1.2, sv * W / 2), ff(L / 2 - 0.3, sv * (W / 2 - 0.45)), ff(L / 2, sv * (W / 2 - 0.9))], 0.55) for sv in (-1, 1)]
    o.st(ext(U(rn + [rect(*ff(L / 2 - 0.05, 0), 0.5, W - 1.4, a)]), 0.5), foot=rect(cx, cy, L, W + 0.6, a))
    o.up(U([rect(*ff(L / 2 - 0.55, sv * (W / 2 - 0.35)), 0.6, 0.5, a + sv * 0.5) for sv in (-1, 1)] + [rect(*ff(L / 2 - 0.05, 0), 0.5, W - 1.4, a)]), 0.95)
    dk = rect(*ff(-0.6, 0), L - 2.2, W - 0.7, a); b = o.up(dk, 0.8); o.seams(lambda x, v: ff(x - 0.6, v), L - 2.2, W - 0.7, b, 0.62, w=0.16, d=0.14)
    o.up(U([rect(*ff(-0.6 + x, sv * (W / 2 - 0.3)), 0.4, 0.4, a) for x in np.linspace(-(L - 2.2) / 2 + 0.3, (L - 2.2) / 2 - 0.3, 4) for sv in (-1, 1)]), 1.45)
    if rng.random() < 0.85:
        x = -(L - 2.2) / 2 - 0.6 + 0.25; n = rng.randint(1, 2)
        for i in range(n):
            cl = u(1.5, 1.9); cw = W - 1.25; c = ff(x + cl / 2, u(-0.1, 0.1)); t = a + u(-0.08, 0.08); f2 = frame(c[0], c[1], t); x += cl + 0.15
            cg = rect(c[0], c[1], cl, cw, t); cs = o.on(cg, b, u(1.0, 1.3), "wood"); ld = o.lid(cg, cs, 0.12, 0.14); o.corners(f2, cl - 0.28, cw - 0.28, ld, t, 0.4, 0.1, "metal")
            o.on(rect(c[0], c[1], 0.4, cw - 0.4, t), ld, 0.12, "rope")
        if x < (L - 2.2) / 2 - 0.6 - 1.0:
            lr = (L - 2.2) / 2 - 0.6 - x - 0.1; c = ff(x + lr / 2, 0); tr = o.on(rrect(c[0], c[1], lr, W - 1.3, a, 0.4), b, u(0.7, 0.9), "cloth")
            o.cut(U([barl(ff(x + lr * f, -W), ff(x + lr * f, W), 0.14) for f in (0.33, 0.66)]), tr, 0.12)
    else: o.on(U([rect(*ff(-0.6 + sx * (L - 2.2) * 0.36, 0), 0.5, W - 0.9, a) for sx in (-1, 1)]), b, 0.14)
    e = ff(L / 2 + u(1.5, 2.4), u(-1.2, 1.2)); pts = [ff(L / 2 + 0.3, -0.3)] + wiggle(ff(L / 2 + 0.6, 0), e, rng, 0.5, 3) + [ff(L / 2 + 0.9, 0.5), ff(L / 2 + 0.3, 0.3)]
    o.st(rod(pts, 0.25, 1.0), "rope", foot=LineString(pts).buffer(0.4))
    return o.out()

@d("targa", "made", "ice", "ice_blocks")
def _(cx, cy, a, rng):
    """ice sawn into building blocks of the real size: a row of them with the saw marks scored along their tops, a
    second course begun on the first, chips at their feet, the long-toothed ice saw with its handle and the lifting
    tongs lying by them"""
    u = rng.uniform; ff = frame(cx, cy, a); n = rng.randint(2, 4); o = Obj("ice"); bl, bw = u(1.7, 2.0), u(1.3, 1.5); tops = []
    for i in range(n):
        c = ff((i - (n - 1) / 2) * (bl + 0.14) + u(-0.05, 0.05), u(-0.12, 0.12)); t = a + u(-0.06, 0.06); f2 = frame(c[0], c[1], t); g = rrect(c[0], c[1], bl, bw, t, 0.12); b = o.up(g, u(1.25, 1.4))
        o.cut(U([barl(f2(-bl, v), f2(bl, v), 0.14) for v in (-bw * 0.2, bw * 0.05, bw * 0.24)]), b, 0.1); tops.append((b, c, t))
    for (b, c, t) in tops:
        if rng.random() < 0.45: g = rrect(c[0], c[1], bl - 0.4, bw - 0.4, t + u(-0.15, 0.15), 0.12); k = o.on(g, b, u(1.1, 1.25)); f2 = frame(c[0], c[1], t); o.cut(barl(f2(-bl, 0), f2(bl, 0), 0.14), k, 0.1)
    for i in range(rng.randint(2, 4)): p = ff(u(-n, n) * 0.9, bw / 2 + u(0.35, 0.7)); o.up(stone(p[0], p[1], u(0.3, 0.45), rng, 0.7, 5), u(0.3, 0.45))
    c = ff(u(-1, 1), -bw / 2 - u(1.2, 1.7)); t = a + u(-0.3, 0.3); f2 = frame(c[0], c[1], t); Ls = u(3.6, 4.4)
    blade = Polygon([f2(-Ls / 2, 0.3)] + [f2(-Ls / 2 + Ls * k / 14, -0.3 if k % 2 else -0.02) for k in range(15)] + [f2(Ls / 2, 0.3)])
    o.st(ext(U([blade.buffer(0.12, 2), ann(*f2(Ls / 2 + 0.55, 0.1), 0.7, 0.25, 10)]), 0.3), "metal", foot=rect(c[0], c[1], Ls + 2.0, 1.4, t))
    if rng.random() < 0.6:
        p = ff(u(-0.8, 0.8), bw / 2 + u(1.1, 1.3)); t2 = a + u(-0.3, 0.3); f3 = frame(p[0], p[1], t2)
        o.st(ext(U([line([f3(-0.9, sv * 0.5), f3(0.3, -sv * 0.15), f3(0.8, -sv * 0.45), f3(0.95, -sv * 0.2)], 0.3) for sv in (-1, 1)] + [disc(*f3(0.0, 0.0), 0.24, 8)]), 0.32), "metal", foot=disc(p[0], p[1], 1.0))
    return o.out()

@d("targa", "made", "alien", "buried_steps")
def _(cx, cy, a, rng):
    """the top of a flight of their steps showing through the snow, each tread a real rise above the last: the
    nosing with its line of inlaid signs, a low wall down each side capped with a coping and a lamp boss at its end,
    a corner broken out of one tread, the drift lying over the lower end"""
    u = rng.uniform; ff = frame(cx, cy, a); W = u(3.2, 4.0); n = rng.randint(3, 4); td = u(1.0, 1.2); o = Obj("alien"); rise = u(0.55, 0.65)
    for i in range(n):
        x = (i - (n - 1) / 2) * td; b = o.up(rect(*ff(x, 0), td + 0.02, W, a), rise * (i + 1)); o.cut(barl(ff(x - td / 2 + 0.3, -W), ff(x - td / 2 + 0.3, W), 0.16), b, 0.12)
        o.cut(U([rect(*ff(x + 0.05, v), 0.24, 0.24, a + 0.785) for v in np.arange(-W / 2 + 0.6, W / 2 - 0.4, 0.55) if rng.random() < 0.7]), b, 0.1)
        if rng.random() < 0.3: sv = rng.choice((-1, 1)); o.cut(Polygon([ff(x - td / 2, sv * W / 2), ff(x - td / 2 + u(0.4, 0.7), sv * W / 2), ff(x - td / 2, sv * (W / 2 - u(0.5, 0.9)))]), b, rise * 0.6)
    for sv in (-1, 1):
        wl = o.up(rect(*ff(0.15, sv * (W / 2 + 0.4)), n * td + 0.5, 0.9, a), rise * n + 0.45); cp = o.on(rect(*ff(0.15, sv * (W / 2 + 0.4)), n * td + 0.5, 0.62, a), wl, 0.14)
        o.cut(barl(ff(-n * td, sv * (W / 2 + 0.4)), ff(n * td, sv * (W / 2 + 0.4)), 0.14), cp, 0.1); o.on(disc(*ff(n * td / 2, sv * (W / 2 + 0.4)), 0.3, 10), cp, 0.25)
    c = ff(-n * td / 2 - u(0.2, 0.7), u(-W * 0.3, W * 0.3)); xt_drift(o, c[0], c[1], u(1.7, 2.3), u(1.0, 1.3), rng)
    if rng.random() < 0.5: c = ff(u(-0.5, 1.0), rng.choice((-1, 1)) * (W / 2 + 1.2)); xt_drift(o, c[0], c[1], u(1.2, 1.6), u(0.7, 1.0), rng)
    return o.out()

@d("targa", "made", "bone", "tusk")
def _(cx, cy, a, rng):
    """a great tusk out of the ice, its real thickness: the sweep of it tapering to the worn point, the growth
    rings round its root and the hollow of the pulp at the broken root end, a crack along its length; snow banked
    over one end"""
    u = rng.uniform; R = u(3.0, 4.5); span = u(1.3, 2.0); o = Obj("bone"); oc = (cx - R * math.cos(a), cy - R * math.sin(a))
    pts = arc(oc[0], oc[1], R, a - span / 2, a + span / 2, 14); r0 = u(0.55, 0.65); hk = 1.45; top = r0 * hk
    ln = LineString(pts); rings = [ln.interpolate(f, normalized=True) for f in (0.06, 0.12, 0.18)]
    cuts = [(U([barl(pol(p.x, p.y, 1.0, a - span / 2 + f * span), pol(p.x, p.y, -1.0, a - span / 2 + f * span), 0.12) for p, f in zip(rings, (0.06, 0.12, 0.18))]), top - 0.12),
            (disc(pts[0][0], pts[0][1], r0 * 0.55, 10), top * 0.45)]
    if rng.random() < 0.6: cuts.append((LineString(pts[2:9]).buffer(0.06), top - 0.15))
    o.st(rod(pts, lambda q: r0 * (1 - 0.7 * q ** 1.2), hk), cuts=cuts, foot=ln.buffer(r0 + 0.2))
    if rng.random() < 0.55:
        e = pts[-1] if rng.random() < 0.5 else pts[0]; xt_drift(o, *pol(e[0], e[1], 0.3, a + u(-1, 1)), u(1.1, 1.4), u(0.55, 0.75), rng)
    return ln.buffer(r0 + 0.2), o.L

@d("targa", "made", "alien", "disc_wreck")
def _(cx, cy, a, rng):
    """one of their small flying discs come down: the thin outer rim with its panel seams, the raised hull ring with
    its ring of ports, the domed canopy with the window band round it, a vane stub on top; a wedge torn out of it
    and the edge plates of the tear bent up"""
    u = rng.uniform; R = u(2.2, 3.0); o = Obj("alien"); tb = a + u(-0.3, 0.3)
    bite = Polygon([pol(cx, cy, R * 0.35, tb), pol(cx, cy, 2 * R, tb - u(0.3, 0.6)), pol(cx, cy, 2 * R, tb + u(0.3, 0.6))])
    D = lambda r, n=28: disc(cx, cy, r, n).difference(bite)
    rim = o.up(D(R), 0.38); o.cut(U([barl(pol(cx, cy, R * 0.78, tb + 0.5 + k * 0.7), pol(cx, cy, R * 1.1, tb + 0.5 + k * 0.7), 0.12) for k in range(8)]), rim, 0.1)
    hl = o.on(D(R * 0.74), rim, 0.36); o.cut(U([disc(*pol(cx, cy, R * 0.6, tb + 0.8 + k * 0.62), 0.18, 8) for k in range(9)]).difference(bite.buffer(0.2)), hl, 0.2)
    o.st(turn(cx, cy, [(R * 0.45, 0.0), (R * 0.45, 0.12)] + crv(R * 0.45, 0.3, 0.12, 0.62, n=5), 24), "alien", par=hl, cuts=[(bite, 0.0), (ann(cx, cy, R * 0.36, R * 0.28, 20), 0.3)])
    o.on(disc(*pol(cx, cy, R * 0.16, tb + math.pi), 0.22, 8), hl, 0.95)
    for sv in (-1, 1):
        p = pol(cx, cy, R * u(0.55, 0.85), tb + sv * u(0.3, 0.45)); o.up(Polygon([pol(p[0], p[1], 0.35, tb + sv * 1.2), pol(p[0], p[1], 0.35, tb + sv * 1.2 + 2.2), pol(p[0], p[1], 0.35, tb + sv * 1.2 + 4.0)]).buffer(0.05), u(0.6, 0.85))
    return disc(cx, cy, R + 0.2), o.L

@d("targa", "made", "metal", "supply_drop")
def _(cx, cy, a, rng):
    """a supply case dropped to the survey party, standing to a man's knee and half drifted over: the ribbed body
    with its lid line, two toggle catches, carrying handles at the ends, the big cross-marked panel on top, stencil
    letters; snow banked on its weather side, or the parachute lying bundled in its folds"""
    u = rng.uniform; ff = frame(cx, cy, a); L = u(4.0, 4.8); W = u(2.3, 2.7); h = u(1.9, 2.2); o = Obj("metal"); g = rrect(cx, cy, L, W, a, 0.35)
    for sx in (-1, 1): c = ff(sx * (L / 2 + 0.05), 0); pts = arc(c[0], c[1], 0.7, a - 1.5708 if sx > 0 else a + 1.5708, a + 1.5708 if sx > 0 else a + 4.7124, 8); o.st(ext(line(pts, 0.45), h * 0.75), foot=disc(c[0], c[1], 0.9))
    b = o.up(g, h); lid = o.lid(g, b, 0.18, 0.16); o.ribs(ff, L - 1.0, W - 0.5, lid, 2, a, 0.45, 0.15)
    pn = rect(cx, cy, L * 0.3, W * 0.5, a); o.cut(pn, lid, 0.16); o.cut(U([barl(ff(-L * 0.1, -W * 0.18), ff(L * 0.1, W * 0.18), 0.22), barl(ff(-L * 0.1, W * 0.18), ff(L * 0.1, -W * 0.18), 0.22)]), lid, 0.3)
    o.cut(U([rect(*ff(x, -W * 0.36), 0.14, 0.34, a) for x in np.arange(-L * 0.3, L * 0.05, 0.3)]), lid, 0.1)
    o.tabs(ff, [-L * 0.3, L * 0.3], W / 2 - 0.45, lid, a, 0.5, 0.55, 0.18)
    sv = rng.choice((-1, 1)); c = ff(u(-L * 0.3, L * 0.3), sv * (W / 2 + 0.7)); xt_drift(o, c[0], c[1], u(1.8, 2.4), u(1.1, 1.5), rng)
    if rng.random() < 0.5:
        c = ff(rng.choice((-1, 1)) * (L / 2 + 0.9), sv * u(0.2, 0.9))
        if rng.random() < 0.5: xt_drift(o, c[0], c[1], u(1.1, 1.5), u(0.6, 0.85), rng)
        else:
            t = a + u(-0.4, 0.4); o.st([s for k in range(3) for s in loaf(*pol(c[0], c[1], (k - 1) * 0.55, t + 1.5708), t, u(1.6, 2.1), 0.75, u(0.45, 0.65))], "cloth", foot=disc(c[0], c[1], 1.0))
    return o.out()

@d("targa", "made", "cloth", "pack_and_snowshoes")
def _(cx, cy, a, rng):
    """a pack set down in the snow, as thick as a real one: the sack with its top flap and two buckled straps, a side
    pocket, the bedroll lashed across its head; a pair of snowshoes (bent frame, cross bars, lacing, the tail) laid by
    it, and an ice axe on some"""
    u = rng.uniform; ff = frame(cx, cy, a); L = u(3.0, 3.6); W = u(2.1, 2.5); o = Obj("cloth"); g = rrect(cx, cy, L, W, a, 0.45)
    b = o.up(g, u(1.25, 1.45)); fl = o.on(rrect(*ff(L * 0.2, 0), L * 0.55, W - 0.25, a, 0.4), b, 0.22); o.cut(barl(ff(L * 0.2, -W), ff(L * 0.2, W), 0.12).intersection(inset(g, 0.5)), fl, 0.08)
    o.on(U([rect(*ff(L * 0.05, sv * W * 0.24), L * 0.8, 0.46, a) for sv in (-1, 1)]), b, 0.32, "leather"); o.studs([ff(-L * 0.12, sv * W * 0.24) for sv in (-1, 1)], b, 0.28, 0.45, "metal")
    sp = o.up(rrect(*ff(-L * 0.1, rng.choice((-1, 1)) * (W / 2 + 0.3)), 1.3, 1.0, a, 0.25), u(0.85, 1.0))
    o.st(xt_bar(ff(L / 2 + 0.6, -W * 0.55), ff(L / 2 + 0.6, W * 0.55), lambda s: 0.6 + 0.07 * (abs(s - 0.25) < 0.06 or abs(s - 0.75) < 0.06), 0.7), "sack", foot=rect(*ff(L / 2 + 0.6, 0), 1.2, W * 1.1, a))
    for i in range(rng.randint(1, 2)):
        c = ff(u(-0.8, 0.8) - L * 0.1, (1 if i == 0 else -1) * (W / 2 + u(1.9, 2.3))) if i == 0 else ff(-L / 2 - u(1.6, 2.0), u(-0.5, 0.5)); t = a + u(-0.4, 0.4) + (1.5708 if i else 0.0); f2 = frame(c[0], c[1], t)
        sh = hull([ell(*f2(0.5, 0), 1.5, 0.95, t, 20), disc(*f2(-1.9, 0), 0.3, 8)]); fr = U([sh.difference(inset(sh, 0.42)), rect(*f2(-2.2, 0), 1.0, 0.45, t)] + [rect(*f2(x, 0), 0.42, 2.2, t).intersection(sh) for x in (-0.1, 1.1)])
        lace = U([barl(f2(-0.1, sv * 0.75), f2(1.1, -sv * 0.75), 0.26) for sv in (-1, 1)] + [barl(f2(-0.1, v), f2(1.1, v), 0.2) for v in (-0.35, 0.35)]).intersection(sh)
        o.st(ext(fr, 0.42) + ext(lace, 0.3), "wood", foot=sh)
    if rng.random() < 0.5:
        p = ff(u(-L * 0.3, L * 0.1), -rng.choice((-1, 1)) * (W / 2 + u(0.6, 0.9))); t = a + u(-0.3, 0.3); f3 = frame(p[0], p[1], t)
        o.st(ext(U([barl(f3(-1.4, 0), f3(1.0, 0), 0.36), Polygon([f3(0.85, -0.9), f3(1.15, -0.2), f3(1.15, 0.5), f3(0.85, 0.3)])]), 0.36), "metal", foot=rect(p[0], p[1], 2.6, 1.0, t))
    return o.out()

@d("targa", "made", "wood", "skis")
def _(cx, cy, a, rng):
    """a pair of skis laid down with their sticks: long narrow boards pointed and stepping up at the tip, the groove
    along each, the foot plate with its toe iron and heel cable in the middle, and two sticks with the basket ring
    near the point and the strap at the grip"""
    u = rng.uniform; o = Obj("wood"); L = u(8.0, 9.5)
    for sv in (-1, 1):
        c = pol(cx, cy, u(0.55, 0.8), a + sv * 1.5708); t = a + u(-0.08, 0.08) + sv * u(0.0, 0.1); f2 = frame(c[0], c[1], t)
        g = Polygon([f2(-L / 2, -0.42), f2(L / 2 - 1.2, -0.46), f2(L / 2, 0), f2(L / 2 - 1.2, 0.46), f2(-L / 2, 0.42)]); b = o.up(g, 0.3)
        pl = o.on(rect(*f2(-0.2, 0), 1.6, 0.62, t), b, 0.14, "metal")
        o.on(U([rect(*f2(0.45, sv2 * 0.25), 0.42, 0.18, t) for sv2 in (-1, 1)] + [rect(*f2(0.6, 0), 0.18, 0.68, t)]), pl, 0.22, "metal")
        o.on(line([f2(0.45, 0.3), f2(-0.9, 0.28), f2(-0.95, 0), f2(-0.9, -0.28), f2(0.45, -0.3)], 0.15), pl, 0.08, "metal")
        o.on(Polygon([f2(L / 2 - 1.3, -0.33), f2(L / 2 - 0.15, 0), f2(L / 2 - 1.3, 0.33)]), b, 0.18); o.on(Polygon([f2(L / 2 - 0.95, -0.2), f2(L / 2 - 0.3, 0), f2(L / 2 - 0.95, 0.2)]), b, 0.4)
    for i in range(rng.randint(1, 2)):
        c = pol(cx, cy, u(1.9, 2.5), a + (1.5708 if i == 0 else -1.5708)); t = a + u(-0.35, 0.35); f2 = frame(c[0], c[1], t); Lp = L * 0.62
        o.st(ext(U([barl(f2(-Lp / 2, 0), f2(Lp / 2, 0), 0.4), ann(*f2(Lp / 2 - 0.8, 0), 0.7, 0.22, 10)] + [Polygon([f2(Lp / 2 - 0.8 + 0.7 * math.cos(k * 1.5708 + 0.785) * 0.9, 0.7 * math.sin(k * 1.5708 + 0.785) * 0.9) for k in range(4)]).exterior.buffer(0.08)]), 0.32)
             + ext(U([rect(*f2(-Lp / 2 + 0.5, 0), 1.0, 0.55, t)]), 0.45), foot=rect(c[0], c[1], Lp, 1.4, t))
        o.st(ext(ann(*f2(-Lp / 2 + 0.2, 0.55), 0.45, 0.22, 10), 0.24), "leather", foot=disc(*f2(-Lp / 2 + 0.2, 0.55), 0.45))
    return o.out()

@d("targa", "made", "alien", "conduit_node", ground=True)
def _(cx, cy, a, rng):
    """a junction in the floor conduits: the six-sided housing with its ring of bolts, the vent slots between them and
    the domed lens in the middle, the conduits themselves lying in their channels melted clear of snow with a clamp
    every little way, a cap where each one goes under"""
    u = rng.uniform; R = u(1.5, 1.9); n = rng.randint(2, 4); gs = []; o = Obj("alien"); ends = []; runs = []
    for k in range(n):
        t = a + 6.2832 * k / n + u(-0.2, 0.2); m = pol(cx, cy, R + 1.3, t); e = pol(m[0], m[1], u(1.2, 2.4), t + rng.choice((-0.5236, 0, 0.5236))); p3 = [pol(cx, cy, R + 0.2, t), m, e]; gs.append(line(p3, 0.75)); ends.append(e); runs.append(p3)
    o.gcut(U(gs), 0.35, "slot")
    o.st([s for p3 in runs for s in rod(p3, 0.28, 1.0)], "cable", foot=U(gs))
    cl = []
    for p3 in runs:
        ln = LineString(p3); t3 = math.atan2(p3[-1][1] - p3[0][1], p3[-1][0] - p3[0][0])
        for f in np.arange(0.7, ln.length - 0.6, 1.1): q = ln.interpolate(f); q2 = ln.interpolate(min(ln.length, f + 0.1)); tt = math.atan2(q2.y - q.y, q2.x - q.x); cl.append(rect(q.x, q.y, 0.3, 0.85, tt))
    if cl: o.st(ext(U(cl), 0.42), "metal", foot=U(cl))
    b = o.up(ngon(cx, cy, R, 6, a), u(0.65, 0.8)); o.on(ann(cx, cy, R * 0.72, R * 0.72 - 0.45, 24), b, 0.2); o.studs([pol(cx, cy, R - 0.38, a + k * 1.0472) for k in range(6)], b, 0.22, 0.15)
    o.cut(U([barl(pol(cx, cy, R * 0.78, a + 0.5236 + k * 1.0472 - 0.12), pol(cx, cy, R * 0.78, a + 0.5236 + k * 1.0472 + 0.12), 0.12) for k in range(6)]), b, 0.1)
    o.st(dome(cx, cy, R * 0.38, R * 0.5, n=14), par=b)
    for e in ends: t = math.atan2(e[1] - cy, e[0] - cx); c = pol(e[0], e[1], 0.8, t); k = o.up(disc(c[0], c[1], 0.62, 12), 0.5); o.cut(disc(c[0], c[1], 0.25, 8), k, 0.18)
    return o.out()


_OWN = set(globals())
