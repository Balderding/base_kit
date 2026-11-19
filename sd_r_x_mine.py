"""Third drawing, mine: the objects at their real size beside a 32 mm figure (1 m = 18 mm as printed), each at the
height of the real thing: a kibble knee high, a sorting table at working height, a ground-throw switch with its
lever laid down along the sleepers. Nothing overhangs: what would stand on legs stands solid on its own feet."""
import math
from sd_reg import d

def xon(o, g, par, h, kind=None):
    """a part standing on layer par as a plain "on" layer (keeps parts down to a quarter of a millimetre at true
    scale, where Obj.on, through ST(), drops anything under 0.3)"""
    if g is None or g.is_empty: return None
    o.L.append(("on", g, par, h, kind or o.k)); return len(o.L) - 1

def xstuds(o, pts, par, r=0.2, h=0.1, kind=None, n=8):
    return xon(o, U([disc(x, y, r, n) for x, y in pts]), par, h, kind) if pts else None

def xm_grain(f2, L, W, rng, n=2, w=0.12):
    gs = []
    for k in range(n):
        v = (k + 0.5 - n / 2) * W / (n + 0.4) + rng.uniform(-0.05, 0.05); x0 = rng.uniform(-L / 2, -L * 0.1); x1 = rng.uniform(L * 0.1, L / 2)
        gs.append(barl(f2(x0, v), f2(x1, v + rng.uniform(-0.06, 0.06)), w))
    return U(gs)

def xm_ore(o, pts, rng, lift=0.0, kind="rock", r=(0.3, 0.45), avoid=()):
    """lumps of ore: broken, angular, a few with a vein of quartz across. A lump that would touch another lump or
    anything in avoid is left out: two separately built rounded pieces that overlap can tear the mesh"""
    if not hasattr(o, "_ore"): o._ore = []
    for i, (x, y) in enumerate(pts):
        rr = rng.uniform(*r); h = rr * rng.uniform(1.1, 1.5); t = rng.uniform(0, 3.14); g = disc(x, y, rr + 0.1, 10)
        if any(g.intersects(q) for q in list(o._ore) + list(avoid)): continue
        o._ore.append(g)
        cuts = [(barl(pol(x, y, rr, t), pol(x, y, rr, t + math.pi), 0.15), lift + h * 0.7)] if rng.random() < 0.35 else []
        o.st(_S().lathe(x, y, [(rr, 0.0), (rr * 0.92, h * 0.55), (rr * 0.6, h * 0.92), (0.18, h)], rng.choice((5, 6, 7)), rng.uniform(0.7, 0.95), t, lift=lift), "quartz" if i % 4 == 3 else kind, cuts=cuts, foot=disc(x, y, rr))


@d("mine", "made", "wood", "sluice_box")
def _(cx, cy, a, rng):
    """a sluice box set on its skids: plank sides and bottom with iron straps across and nails along the top edges,
    the riffle bars across the floor with gravel caught behind them, the grizzly of bars over the head end, and at
    the open tail the wet fan of washed gravel it spat out"""
    u = rng.uniform; ff = frame(cx, cy, a); L = u(5.1, 6.0); W = u(1.7, 2.0); h = u(1.0, 1.2); o = Obj("wood"); t = 0.3
    sk = o.up(U([rect(*ff(x, 0), 0.6, W + 1.0, a) for x in (-L * 0.3, L * 0.32)]), 0.36); o.cut(xm_grain(frame(*ff(-L * 0.3, 0), a + 1.5708), W, 0.6, rng, 1, 0.11), sk, 0.08)
    sides = U([rect(*ff(0, sv * (W / 2 - t / 2)), L, t, a) for sv in (-1, 1)] + [rect(*ff(-L / 2 + t / 2, 0), t, W, a)])
    wl = o.up(sides, h); fl = o.up(rect(*ff(t / 2, 0), L - t, W - 2 * t + 0.04, a), 0.3)
    xstuds(o, [ff(x, sv * (W / 2 - t / 2)) for x in np.arange(-L / 2 + 0.4, L / 2, 0.7) for sv in (-1, 1)], wl, 0.12, 0.06, "iron", 6)
    o.cut(U([barl(ff(x, -W), ff(x + 0.01, W), 0.12) for x in (-L * 0.15, L * 0.2)]), wl, 0.08)
    xs = list(np.arange(-L / 2 + 1.6, L / 2 - 0.4, 0.62)); o.up(U([rect(*ff(x, 0), 0.26, W - 2 * t + 0.04, a) for x in xs]), 0.58, "iron")
    for x in xs[: rng.randint(3, len(xs))]:
        if rng.random() < 0.7: xm_ore(o, [ff(x - 0.32, u(-W * 0.25, W * 0.25))], rng, 0.3, "rock", (0.17, 0.24))
    gz = o.up(U([rect(*ff(-L / 2 + 0.75 + 0.0, v), 1.0, 0.24, a) for v in np.linspace(-W / 2 + 0.45, W / 2 - 0.45, 4)]), h + 0.12, "iron")
    o.up(U([rect(*ff(x, 0), 0.4, W + 0.12, a) for x in (-L * 0.32, 0.0, L * 0.32)]).difference(rect(*ff(t / 2, 0), L - t, W - 2 * t, a)), h + 0.08, "iron")
    fan = Polygon([ff(L / 2, -W / 2 + 0.2), ff(L / 2 + u(1.0, 1.4), -W * 0.7), ff(L / 2 + u(1.2, 1.6), W * 0.6), ff(L / 2, W / 2 - 0.2)]); o.gcut(fan, 0.2, "water")
    xm_ore(o, [ff(L / 2 + u(0.3, 1.2), u(-W * 0.4, W * 0.4)) for _ in range(rng.randint(2, 4))], rng, 0.0, "rock", (0.2, 0.3))
    return o.out([rect(cx, cy, L + 0.2, W + 1.0, a)])

@d("mine", "made", "wood", "sorting_table")
def _(cx, cy, a, rng):
    """an ore-picking table at working height: the plank top with its board joints, rim boards round three sides
    and the open front where the waste is raked off, the corner legs standing proud of the aprons with the
    stretchers between them at the foot; ore lying sorted in heaps on it with the picker's hammer, and the waste
    fallen at the open side"""
    u = rng.uniform; ff = frame(cx, cy, a); L = u(4.0, 4.6); W = u(2.3, 2.7); h = u(2.3, 2.6); o = Obj("wood"); g = rect(cx, cy, L, W, a)
    o.up(U([rect(*ff(sx * (L / 2 - 0.5), 0), 0.3, W + 0.2, a) for sx in (-1, 1)] + [rect(*ff(0, sv * (W / 2 - 0.45)), L - 0.6, 0.3, a) for sv in (-1, 1)]), 0.5)
    o.up(U([rect(*ff(sx * (L / 2 - 0.3), sv * (W / 2 - 0.3)), 0.55, 0.55, a) for sx in (-1, 1) for sv in (-1, 1)]), h - 0.1)
    top = o.up(inset(g, 0.42), h)
    rim = o.up(U([rect(*ff(0, W / 2 - 0.57), L - 0.84, 0.3, a), rect(*ff(-L / 2 + 0.57, 0), 0.3, W - 0.84, a), rect(*ff(L / 2 - 0.57, 0), 0.3, W - 0.84, a)]), h + 0.3)
    o.seams(ff, L - 0.7, W - 0.7, top, 0.55, w=0.12, d=0.08)
    hp = [ff(u(-L * 0.3, -L * 0.1), u(-0.2, 0.3)), ff(u(L * 0.05, L * 0.3), u(-0.2, 0.3))]
    for c in hp: xm_ore(o, [pol(c[0], c[1], u(0, 0.45), u(0, 6.28)) for _ in range(rng.randint(3, 5))], rng, h, "rock", (0.2, 0.3))
    hc = ff(u(-0.4, 0.4), -W / 2 + 0.55); t = a + u(-0.4, 0.4); f2 = frame(hc[0], hc[1], t)
    o.st(ext(U([barl(f2(-0.9, 0), f2(0.5, 0), 0.26), rect(*f2(0.6, 0), 0.3, 0.7, t)]), 0.25, lift=h), "iron", foot=disc(hc[0], hc[1], 0.9))
    xm_ore(o, [ff(u(-L / 2, L / 2), -W / 2 - u(0.35, 0.8)) for _ in range(rng.randint(3, 5))], rng, 0.0, "rock", (0.25, 0.4))
    return o.out([rect(cx, cy, L + 0.2, W + 0.2, a)])

@d("mine", "made", "iron", "kibble")
def _(cx, cy, a, rng):
    """an ore kibble set down: the riveted iron bucket knee high, banded at the rim and the foot, the lugs on its
    sides and the great bail laid down flat on the ground with the shackle on it; full to the brim with ore or
    standing empty, with lumps spilled round it"""
    u = rng.uniform; o = Obj("iron"); r = u(1.25, 1.45); h = u(2.9, 3.3); full = rng.random() < 0.55; t0 = a + u(-0.3, 0.3)
    fb = o.up(ann(cx, cy, r + 0.08, r - 0.3, 24), 0.32); wl = o.up(ann(cx, cy, r, r - 0.32, 24), h); fl = o.up(disc(cx, cy, r - 0.3, 20), h - (0.55 if full else u(1.4, 1.8)))
    xstuds(o, [pol(cx, cy, r - 0.16, k * 0.393 + t0) for k in range(16)], wl, 0.11, 0.07, n=6)
    o.cut(U([barl(pol(cx, cy, r - 0.35, t0 + k * 2.1), pol(cx, cy, r + 0.1, t0 + k * 2.1), 0.12) for k in range(3)]), wl, 0.08)
    if full: xm_ore(o, [pol(cx, cy, u(0, r * 0.55), u(0, 6.28)) for _ in range(rng.randint(4, 6))], rng, h - 0.6, "rock", (0.28, 0.38))
    for sv in (-1, 1): o.st(ext(rect(*pol(cx, cy, r + 0.1, t0 + sv * 1.5708), 0.4, 0.5, t0), h * 0.55), foot=disc(*pol(cx, cy, r + 0.12, t0 + sv * 1.5708), 0.3))
    side = rng.choice((-1, 1)); p1 = pol(cx, cy, r + 0.35, t0 + 1.5708); p2 = pol(cx, cy, r + 0.35, t0 - 1.5708); far = pol(cx, cy, r + u(1.4, 1.8), t0 + side * 0.0 + (0 if side > 0 else math.pi))
    bp = [p1, pol(far[0], far[1], r * 0.8, t0 + 1.5708), far, pol(far[0], far[1], r * 0.8, t0 - 1.5708), p2]
    o.st(rod(bp, 0.27, 1.1), foot=LineString(bp).buffer(0.35))
    o.st(ext(ann(*pol(far[0], far[1], 0.45, math.atan2(far[1] - cy, far[0] - cx)), 0.42, 0.18, 12), 0.32), foot=disc(far[0], far[1], 0.8))
    xm_ore(o, [pol(cx, cy, r + u(0.6, 1.4), t0 + math.pi * (side > 0) + u(-1.2, 1.2) + 1.5708 * rng.choice((-1, 1))) for _ in range(rng.randint(2, 4))], rng, 0.0, "rock", (0.3, 0.45), [disc(cx, cy, r + 0.5), LineString(bp).buffer(0.45), disc(far[0], far[1], 0.9)])
    o.F.append(disc(cx, cy, r + 1.9)); return o.out()

@d("mine", "made", "iron", "switch_lever")
def _(cx, cy, a, rng):
    """a ground-throw switch stand: the cast housing bolted to its base plate on two sleepers, the hand lever thrown
    and lying down along the ground with its weighted handle in the keeper, the connecting rod running off to the
    switch point, and a short length of rail on the sleeper ends"""
    u = rng.uniform; ff = frame(cx, cy, a); o = Obj("iron")
    sl = o.up(U([rect(*ff(x, 0), 0.9, 3.4, a) for x in (-0.6, 0.6)]), 0.5, "wood"); o.cut(U([xm_grain(frame(*ff(x, 0), a + 1.5708), 3.2, 0.9, rng, 2, 0.11) for x in (-0.6, 0.6)]), sl, 0.08)
    bp = o.up(rect(*ff(0, -0.4), 2.2, 1.5, a), 0.65); xstuds(o, [ff(sx * 0.85, -0.4 + sv * 0.55) for sx in (-1, 1) for sv in (-1, 1)], bp, 0.16, 0.12, n=6)
    hs = xon(o, rect(*ff(-0.2, -0.4), 1.0, 0.9, a), bp, 0.55); o.st(dome(*ff(-0.2, -0.4), 0.42, 0.35), par=hs)
    Ll = u(2.3, 2.7); e = ff(0.2 + Ll, -0.4 + u(-0.15, 0.15))
    o.st(rod([ff(0.25, -0.4), e], 0.25, 1.1, lift=0.15) + dome(e[0], e[1], 0.42, 0.75), foot=LineString([ff(0.25, -0.4), e]).buffer(0.45))
    kp = o.up(U([rect(*pol(e[0], e[1], 0.55, a + sv * 1.5708), 0.5, 0.25, a) for sv in (-1, 1)]), 0.5)
    rv = 1.3; rl = o.up(rect(*ff(0, rv), 3.6, 0.55, a + 0.0), 0.32); hd = xon(o, rect(*ff(0, rv), 3.6, 0.26, a), rl, 0.5)
    xstuds(o, [ff(x, rv + sv * 0.2) for x in (-0.6, 0.6) for sv in (-1, 1)], rl, 0.11, 0.06, n=6)
    o.st(rod([ff(-0.2, 0.0), ff(-0.2, rv - 0.4)], 0.25, 1.0, lift=0.45), foot=rect(*ff(-0.2, 0.5), 0.5, 1.0, a))
    return o.out()

@d("mine", "made", "sack", "spilled_ore")
def _(cx, cy, a, rng):
    """an ore sack split open where it was dropped: the burlap bag tied at the neck with the cord ends loose, the
    stencilled letters on it and the seam along its side, a rent at its foot and the ore run out of it in a fan of
    broken lumps, some with quartz veins, a shovel left lying by it on some"""
    u = rng.uniform; ff = frame(cx, cy, a); L = u(3.0, 3.4); W = u(1.9, 2.2); h = u(1.05, 1.25); o = Obj("sack")
    sk = _S()._sack(cx, cy, a, L, W, h, 0.0)
    gl = [(0, 0, 0, 1), (0, 1, 1, 1), (1, 1, 1, 0), (1, 0, 0, 0)], [(0, 0, 0, 1), (0, 1, 1, 1), (1, 1, 1, .5), (1, .5, 0, .5), (.3, .5, 1, 0)], [(0, 0, 0, 1), (0, 1, 1, 1), (0, .5, .8, .5), (0, 0, 1, 0)]
    f2 = frame(*ff(-L * 0.1, 0), a + 1.5708); lt = []
    for i, g in enumerate(gl):
        for p0, q0, p1, q1 in g: lt.append(barl(f2(-0.75 + i * 0.55 + p0 * 0.38, -0.25 + q0 * 0.5), f2(-0.75 + i * 0.55 + p1 * 0.38 + 1e-3, -0.25 + q1 * 0.5 + 1e-3), 0.15))
    cuts = [(q, h * 0.78) for q in polys_of(U(lt))] + [(barl(ff(-L / 2, W * 0.32), ff(L / 2, W * 0.32), 0.15), h * 0.55), (tri(ff(L / 2 - 0.1, 0), ff(L / 2 - 0.9, u(-0.3, 0.3)), 0.6), h * 0.35)]
    o.st(sk, cuts=cuts, foot=rect(cx, cy, L, W, a))
    nk = ff(-L / 2 - 0.15, 0); o.st(lie(nk[0], nk[1], a, 0.5, 0.42), "rope", foot=disc(nk[0], nk[1], 0.45))
    for sv in (-1, 1): pts = [ff(-L / 2 - 0.2, sv * 0.2), ff(-L / 2 - u(0.6, 1.0), sv * u(0.4, 0.8))]; o.st(rod(pts, 0.25, 1.0), "rope", foot=LineString(pts).buffer(0.3))
    xm_ore(o, [ff(L / 2 + u(0.65, 2.5), u(-1.3, 1.3)) for _ in range(rng.randint(10, 14))], rng, 0.0, "rock", (0.28, 0.48), [rect(cx, cy, L, W, a).buffer(0.1), disc(nk[0], nk[1], 0.6), rect(*ff(-L / 2 - 0.8, 0), 1.2, 2.0, a), rect(*ff(0.3, -(W / 2 + 0.6)), 6.0, 1.0, a)])
    if rng.random() < 0.4:
        f3 = frame(*ff(u(-0.5, 0.8), -(W / 2 + u(0.5, 0.7))), a + u(-0.2, 0.2)); o.st(rod([f3(-2.0, 0), f3(0.6, 0)], 0.25, 1.1) + ext(U([rect(*f3(-2.2, 0), 0.25, 0.7, a)]), 0.3), "wood", foot=rect(*f3(-0.6, 0), 3.0, 0.8, a))
        sp = Polygon([f3(0.6, -0.5), f3(1.7, -0.45), f3(1.95, 0), f3(1.7, 0.45), f3(0.6, 0.5)]); o.up(sp, 0.24)
    return o.out()

@d("mine", "made", "wood", "fallen_stull")
def _(cx, cy, a, rng):
    """a stull that gave way: the hewn prop snapped in two, the halves lying at an angle with long splinters torn out
    of the break, axe flats along them and bark left in strips, the cap board that sat on its head lying by it with
    a wedge, and the rock that came down with it"""
    u = rng.uniform; ff = frame(cx, cy, a); L = u(3.0, 4.0); b = u(0.3, 0.8) * rng.choice((-1, 1)); r = u(0.6, 0.72); o = Obj("wood"); lf = r * 0.8
    c1 = ff(-L / 2, 0); c2 = pol(*ff(0.3, 0), L / 2, a + b)
    for c, t, Lx in ((c1, a, L), (c2, a + b, L * u(0.7, 1.0))):
        f2 = frame(c[0], c[1], t); hz = lf + r * 1.1 * 0.93
        cuts = [(barl(f2(-Lx / 2 + 0.2, v), f2(Lx / 2 - 0.3, v + 0.02), 0.17), hz - 0.12 - abs(v) * 0.3) for v in (-r * 0.3, r * 0.35)] + [(rect(*f2(u(-Lx * 0.3, Lx * 0.3), 0), u(0.8, 1.3), 2 * r, t), hz - 0.08)]
        o.st(spin(c[0], c[1], t, Lx, lambda q: r, 1.1, lift=lf), cuts=cuts, foot=rect(c[0], c[1], Lx, 2 * r, t))
    for sv in (-1, 1):
        if rng.random() < 0.75: o.st(ext(tri(ff(u(-0.3, 0.3), sv * 0.3), ff(u(-0.5, 0.9), sv * u(1.5, 2.2)), 0.6), 0.45), foot=disc(*ff(0, sv * 1.0), 1.0))
    o.st(ext(U([tri(ff(-0.05, v), ff(u(0.3, 0.7), v + u(-0.1, 0.1)), 0.32) for v in np.linspace(-r * 0.6, r * 0.6, 4)]), lf + r * 0.8), foot=disc(*ff(0.2, 0), r))
    cc = ff(-L * u(0.3, 0.6), rng.choice((-1, 1)) * (r + u(0.9, 1.2))); t = a + u(-0.3, 0.3); f2 = frame(cc[0], cc[1], t)
    cb = o.up(rect(cc[0], cc[1], u(2.4, 3.0), 1.1, t), 0.42); o.cut(xm_grain(f2, 2.4, 1.1, rng, 2, 0.12), cb, 0.08)
    o.up(Polygon([f2(1.6, -0.3), f2(2.6, -0.3), f2(2.6, 0.3), f2(1.6, -0.1)]), 0.42)
    xm_ore(o, [pol(cx, cy, u(1.2, 3.2), u(0, 6.28)) for _ in range(rng.randint(4, 8))], rng, 0.0, "rock", (0.4, 0.7), [q.buffer(0.15) for q in o.F])
    return o.out([hull([rect(*ff(-L / 2, 0), L, 2 * r, a), rect(c2[0], c2[1], L, 2 * r, a + b)]).buffer(1.0)])


_OWN = set(globals())
