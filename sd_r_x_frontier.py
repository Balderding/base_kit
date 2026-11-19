"""Third drawing, frontier: the things drawn first for a smaller look, redrawn at their real size beside a 32 mm figure
(1 m = 18 mm printed). Each is laid at its real size and built about four times larger than drawn, so a real height of
h metres is drawn as h x 18 / (that factor). Nothing hangs: a sign lies where it fell with its chains, a safe lies on
its back so its blown-open front faces up, a stove and a bell lie on their sides.

Also the letters (x_word) other third drawings carve into boards and signs."""
import math
from sd_reg import d, real_factor
from sd_r_trederra import t_bar

def x_cutfix(out, style, name, cls):
    """a cut into a top is dropped by the kit when no part of it is 0.2 mm wide as laid: every line cut too thin is
    widened just enough to stay, at the size this detail is laid"""
    if out is None or out[0] is None: return out
    k = real_factor(style, name, cls) or 1.0; wmin = 0.205 / k; lay = []
    for L in out[1]:
        if L is not None and L[0] == "tcut" and L[1] is not None and not L[1].is_empty:
            ps = []
            for q in polys_of(L[1]):
                for _ in range(8):
                    if not q.buffer(-wmin / 2).is_empty: break
                    q = q.buffer(0.015, 3)
                ps.append(q)
            L = (L[0], U(ps)) + tuple(L[2:])
        lay.append(L)
    return out[0], lay

def x_d(style, cls, kind, name, **kw):
    """@d, with x_cutfix on what the builder returns"""
    def w(f0):
        def f(cx, cy, a, rng): return x_cutfix(f0(cx, cy, a, rng), style, name, cls)
        f.__doc__ = f0.__doc__; d(style, cls, kind, name, **kw)(f)
        return f0
    return w

# ---------------------------------------------------------------------------------------------- stroke letters
_P = [(0, 0), (0, 1), (0.45, 1), (0.6, 0.85), (0.6, 0.62), (0.45, 0.5), (0, 0.5)]
X_GLYPH = {
    "A": [[(0, 0), (0.3, 1), (0.6, 0)], [(0.12, 0.38), (0.48, 0.38)]],
    "B": [[(0, 0), (0, 1), (0.42, 1), (0.55, 0.88), (0.55, 0.62), (0.42, 0.52), (0, 0.52)], [(0.42, 0.52), (0.6, 0.4), (0.6, 0.12), (0.45, 0), (0, 0)]],
    "C": [[(0.6, 0.85), (0.45, 1), (0.15, 1), (0, 0.8), (0, 0.2), (0.15, 0), (0.45, 0), (0.6, 0.15)]],
    "D": [[(0, 0), (0, 1), (0.35, 1), (0.6, 0.75), (0.6, 0.25), (0.35, 0), (0, 0)]],
    "E": [[(0.6, 1), (0, 1), (0, 0), (0.6, 0)], [(0, 0.52), (0.42, 0.52)]],
    "F": [[(0.6, 1), (0, 1), (0, 0)], [(0, 0.52), (0.42, 0.52)]],
    "G": [[(0.6, 0.85), (0.45, 1), (0.15, 1), (0, 0.8), (0, 0.2), (0.15, 0), (0.45, 0), (0.6, 0.15), (0.6, 0.45), (0.32, 0.45)]],
    "H": [[(0, 0), (0, 1)], [(0.6, 0), (0.6, 1)], [(0, 0.52), (0.6, 0.52)]],
    "I": [[(0.3, 0), (0.3, 1)], [(0.1, 1), (0.5, 1)], [(0.1, 0), (0.5, 0)]],
    "J": [[(0.6, 1), (0.6, 0.2), (0.45, 0), (0.15, 0), (0, 0.2)]],
    "K": [[(0, 0), (0, 1)], [(0.6, 1), (0, 0.42)], [(0.2, 0.6), (0.6, 0)]],
    "L": [[(0, 1), (0, 0), (0.6, 0)]],
    "M": [[(0, 0), (0, 1), (0.3, 0.45), (0.6, 1), (0.6, 0)]],
    "N": [[(0, 0), (0, 1), (0.6, 0), (0.6, 1)]],
    "O": [[(0.15, 0), (0, 0.2), (0, 0.8), (0.15, 1), (0.45, 1), (0.6, 0.8), (0.6, 0.2), (0.45, 0), (0.15, 0)]],
    "P": [_P],
    "R": [_P, [(0.3, 0.5), (0.6, 0)]],
    "S": [[(0.6, 0.85), (0.45, 1), (0.15, 1), (0, 0.85), (0, 0.64), (0.15, 0.53), (0.45, 0.47), (0.6, 0.36), (0.6, 0.15), (0.45, 0), (0.15, 0), (0, 0.15)]],
    "T": [[(0, 1), (0.6, 1)], [(0.3, 1), (0.3, 0)]],
    "U": [[(0, 1), (0, 0.2), (0.15, 0), (0.45, 0), (0.6, 0.2), (0.6, 1)]],
    "V": [[(0, 1), (0.3, 0), (0.6, 1)]],
    "W": [[(0, 1), (0.14, 0), (0.3, 0.55), (0.46, 0), (0.6, 1)]],
    "X": [[(0, 0), (0.6, 1)], [(0, 1), (0.6, 0)]],
    "Y": [[(0, 1), (0.3, 0.5), (0.6, 1)], [(0.3, 0.5), (0.3, 0)]],
    "Z": [[(0, 1), (0.6, 1), (0, 0), (0.6, 0)]],
    "1": [[(0.15, 0.8), (0.35, 1), (0.35, 0)]],
    "2": [[(0, 0.8), (0.15, 1), (0.45, 1), (0.6, 0.8), (0.6, 0.62), (0, 0), (0.6, 0)]],
    "3": [[(0, 1), (0.6, 1), (0.25, 0.58), (0.45, 0.55), (0.6, 0.4), (0.6, 0.15), (0.45, 0), (0.15, 0), (0, 0.15)]],
    "4": [[(0.45, 0), (0.45, 1), (0, 0.32), (0.6, 0.32)]],
    "-": [[(0.1, 0.5), (0.5, 0.5)]],
}

def x_word(txt, cx, cy, b, h, w=0.16, width=None):
    """a word in stroke letters h tall (shrunk to fit `width`), centred on (cx, cy), reading along angle b: the lines
    to carve into a board"""
    adv = 0.88
    n = len(txt)
    if width is not None and n * adv * h - 0.28 * h > width: h = width / (n * adv - 0.28)
    ff = frame(cx, cy, b); x0 = -(n * adv - 0.28) * h / 2; gs = []
    for i, ch in enumerate(txt):
        for st in X_GLYPH.get(ch, []):
            gs.append(LineString([ff(x0 + (i * adv + gx) * h, (gy - 0.5) * h) for gx, gy in st]).buffer(w / 2, 3))
    return U(gs) if gs else None

def x_chain(o, p, q, kind="iron", w=0.2, link=0.52):
    """a chain lying on the ground from p to q: links alternately flat (a ring) and on edge (a bar)"""
    L = math.dist(p, q) or 1.0; t = math.atan2(q[1] - p[1], q[0] - p[0]); n = max(2, int(L / (link * 0.78))); gs = []
    for i in range(n):
        c = (p[0] + (q[0] - p[0]) * (i + 0.5) / n, p[1] + (q[1] - p[1]) * (i + 0.5) / n)
        gs.append(ell(c[0], c[1], link / 2, link * 0.32, t, 12).exterior.buffer(w / 2, 3) if i % 2 == 0 else barl(pol(c[0], c[1], link * 0.42, t), pol(c[0], c[1], link * 0.42, t + math.pi), w))
    return o.st(ext(U(gs), 0.26), kind, foot=LineString([p, q]).buffer(0.3))


# ---------------------------------------------------------------------------------------------- frontier
@x_d("frontier", "made", "board", "shop_sign")
def _(cx, cy, a, rng):
    """a shop sign come down off its bracket: a heavy painted board with a moulded border standing proud, the name
    carved into it (SALOON, BANK, ASSAY...), the plank joint across it, nail heads at the corners; the two eye bolts on
    its top edge with the chains lying out from them, and on some the iron bracket with its scroll lying beside it.
    Some are shot through or have a corner split off."""
    u = rng.uniform; ff = frame(cx, cy, a); L = u(4.9, 5.3); W = u(1.7, 2.0); o = Obj("board")
    word = rng.choice(["SALOON", "BANK", "HOTEL", "STORE", "ASSAY", "BARBER", "JAIL", "GUNS", "FEED", "SHERIFF", "LIVERY", "GOLD", "TAILOR"])
    k = rng.randint(0, 2)
    if k == 0: g = rrect(cx, cy, L, W, a, 0.25)
    elif k == 1: g = rect(cx, cy, L, W, a).difference(U([disc(*ff(sx * L / 2, sy * W / 2), 0.42, 10) for sx in (-1, 1) for sy in (-1, 1)]))
    else: g = U([rect(*ff(0, -W * 0.08), L, W * 0.84, a), ell(*ff(0, W * 0.2), L * 0.34, W * 0.3, a, 24)])
    if rng.random() < 0.3:          # a corner split off along the grain
        sx = rng.choice((-1, 1)); g = g.difference(Polygon([ff(sx * L / 2 + sx, -W), ff(sx * (L / 2 - u(0.6, 1.0)), -W), ff(sx * L / 2 + sx, u(-0.2, 0.3))]))
    b = o.up(g, u(0.32, 0.36)); inner = inset(g, 0.4)
    o.st(ext(g.difference(inner), u(0.28, 0.32)), par=b)
    o.cut(barl(ff(-L, W * u(-0.3, -0.18)), ff(L, W * u(-0.3, -0.18)), 0.12).intersection(inner), b, 0.08)
    wd = x_word(word, *ff(0, W * 0.04), a, W * 0.5, 0.16, L - 1.1)
    if wd is not None: o.cut(wd.intersection(inset(inner, 0.08)), b, 0.13)
    if rng.random() < 0.5: o.cut(U([disc(*ff(u(-L * 0.35, L * 0.35), u(-W * 0.25, W * 0.25)), 0.17, 8) for _ in range(rng.randint(2, 4))]), b, 0.2)
    o.studs([ff(sx * (L / 2 - 0.25), sy * (W * 0.3)) for sx in (-1, 1) for sy in (-1, 1)], b, 0.21, 0.08, "iron", 8)
    top = W / 2 if k != 2 else W * 0.48
    for sx in (-1, 1):
        if rng.random() < 0.85:
            e = ff(sx * L * 0.3, top + 0.2); o.st(ext(ann(e[0], e[1], 0.3, 0.12, 10), 0.3), "iron", foot=disc(e[0], e[1], 0.3))
            if rng.random() < 0.8: x_chain(o, ff(sx * L * 0.3 + sx * 0.1, top + 0.55), ff(sx * L * u(0.22, 0.42), top + u(1.2, 1.7)))
    if rng.random() < 0.5:          # the bracket that held it: a flat bar with a curl at its end
        v = top + u(1.9, 2.2); p = ff(-L * 0.42, v); q = ff(L * 0.3, v); sp = spiral(*ff(L * 0.3, v - 0.35), 0.35, 0.08, 1.1, a + 1.57, 18)
        o.st(ext(U([barl(p, q, 0.26), line(sp, 0.2), rect(*ff(-L * 0.42, v), 0.3, 0.7, a)]), 0.3), "iron", foot=rect(*ff(0, v), L * 0.8, 0.8, a))
    return o.out()

@x_d("frontier", "made", "sack", "hay_bale")
def _(cx, cy, a, rng):
    """a small square bale of hay: ragged ends where the straw sticks out, straw lying along its top, two twines bound
    round it; on some a second bale on top laid the other way or with its twine burst and the straw spilling, loose straw
    on the ground about it, and the pitchfork stuck down beside it"""
    u = rng.uniform; ff = frame(cx, cy, a); L = u(3.7, 4.1); W = u(1.8, 2.0); h = u(1.4, 1.55); o = Obj("sack")
    def bale(c, b, L, W, par=None):
        f2 = frame(c[0], c[1], b); m = 9
        end = lambda sx: [f2(sx * (L / 2 + (0.14 if j % 2 else -0.02) + u(-0.06, 0.06)), sx * (-W / 2 + W * j / m)) for j in range(m + 1)]
        g = Polygon(end(1) + end(-1)).buffer(0.0)
        i = o.up(g, h) if par is None else o.on(g, par, h)
        gs = [barl(f2(x0, v), f2(x0 + u(0.5, 1.3), v + u(-0.06, 0.06)), 0.13) for v in np.arange(-W / 2 + 0.25, W / 2 - 0.2, 0.2) for x0 in np.arange(-L / 2 + u(0.1, 0.5), L / 2 - 0.6, 1.25)]
        tw = [x * L for x in (-0.26, 0.26)]
        o.cut(U(gs).difference(U([rect(*f2(x, 0), 0.4, W, b) for x in tw])).intersection(inset(g, 0.2)), i, 0.12)
        o.cut(U([rect(*f2(x, 0), 0.17, W + 1, b) for x in tw]).intersection(g), i, 0.14)
        return i, g
    b0, g0 = bale((cx, cy), a, L, W)
    if rng.random() < 0.45:
        c = ff(u(-0.2, 0.2), u(-0.1, 0.1)); bale(c, a + rng.choice((0.0, 0.0, 1.5708)) + u(-0.12, 0.12), L * 0.9 if rng.random() < 0.5 else W * 0.95, W * 0.95 if True else W, b0)
    elif rng.random() < 0.5:          # straw spilt from a burst bale lying against it
        c = ff(u(-0.6, 0.6), -W / 2 - u(0.6, 0.9)); o.st(dome(c[0], c[1], u(1.0, 1.3), u(0.5, 0.7), sq=0.6, a=a), "sack", foot=disc(c[0], c[1], 1.2))
    st = [barl(p, pol(p[0], p[1], u(0.5, 1.0), u(0, 6.28)), 0.22) for p in (ff(u(-L / 2, L / 2), sv * u(W / 2 + 0.3, W / 2 + 1.0)) for sv in (-1, 1) for _ in range(3))]
    o.st(ext(U(st), 0.12), "sack", foot=U(st))
    f2 = frame(*ff(u(-0.8, 0.8), W / 2 + u(0.9, 1.2)), a + u(-0.3, 0.3)); Lp = u(6.0, 6.8); x1 = Lp / 2 - 1.3
    ps = t_bar(f2(-Lp / 2, 0), f2(x1, 0), lambda s: 0.22 if s < 0.97 else 0.3, 0.5)
    ps += ext(U([barl(f2(x1, -0.62), f2(x1, 0.62), 0.3), rect(*f2(x1 - 0.2, 0), 0.5, 0.5, a)] + [tri(f2(x1, v), f2(Lp / 2, v * 1.15), 0.24) for v in ((-0.55, 0, 0.55) if rng.random() < 0.6 else (-0.6, -0.2, 0.2, 0.6))]), 0.28)
    o.st(ps, "wood", foot=rect(*f2(0, 0), Lp, 1.4, a))
    return o.out()

@x_d("frontier", "made", "iron", "busted_safe")
def _(cx, cy, a, rng):
    """a strongbox safe blown and tipped on its back, so its open front looks up: the thick frame of the doorway, the
    strong room inside with its shelf and a small cash drawer, the bolt holes down the lock side and the hinge knuckles
    on the other, the plinth and the feet; the door lies face up beside it with its combination dial, the spoked handle,
    the keyhole cover, the bolts standing out of its edge and its corner torn by the blast; coins and notes spilt"""
    u = rng.uniform; ff = frame(cx, cy, a); Hs = u(3.1, 3.4); S = u(2.5, 2.8); dp = u(2.5, 2.8); o = Obj("iron")
    xb = -1.25; g = rrect(*ff(xb, 0), Hs, S, a, 0.15); b = o.up(g, dp)
    f2 = frame(*ff(xb, 0), a)
    o.st(ext(U([rect(*f2(-Hs / 2 - 0.18, sv * (S / 2 - 0.4)), 0.4, 0.6, a) for sv in (-1, 1)]), 0.6), foot=rect(*f2(-Hs / 2 - 0.2, 0), 0.45, S, a))          # the feet, now standing out at its foot
    hw, hl = S - 0.9, Hs - 0.9; sh = u(-0.1, 0.25)
    up_ = rect(*f2(hl / 4 + sh / 2 + 0.1, 0), hl / 2 - sh - 0.2 + 0.0, hw, a); lo = rect(*f2(-hl / 4 + sh / 2 - 0.1, 0), hl / 2 + sh - 0.2, hw, a)
    o.cut(up_, b, dp * 0.42); o.cut(lo, b, dp * 0.62)
    o.groove(rect(*f2(0, 0), hl + 0.35, hw + 0.35, a), b, 0.12, 0.1)
    o.on(U([rect(*f2(x, -S / 2 + 0.22), 0.36, 0.3, a) for x in (-hl * 0.3, hl * 0.3)]), b, 0.22)
    o.cut(U([disc(*f2(x, S / 2 - 0.28), 0.1, 6) for x in np.linspace(-hl * 0.38, hl * 0.38, 4)]), b, 0.25)
    o.cut(rect(*f2(-Hs / 2 + 0.22, 0), 0.14, S - 0.3, a), b, 0.12)          # the plinth line
    # the door, lying face up
    dg = rect(*ff(1.7, u(-0.2, 0.2)), u(2.1, 2.3), S - 0.15, a + u(-0.25, 0.25)); dc = dg.centroid; t = a + u(-0.25, 0.25); f3 = frame(dc.x, dc.y, t)
    if rng.random() < 0.6: dg = dg.difference(Polygon([f3(1.3, -1.6), f3(0.4, -1.6), f3(0.7, -1.15), f3(0.55, -0.9), f3(1.3, -0.6)]))
    dd = o.up(dg, 0.5); pn = o.on(inset(dg, 0.3), dd, 0.12); o.groove(inset(dg, 0.55), pn, 0.1, 0.08)
    o.st(ext(U([rect(*f3(-1.15 - 0.05, v), 0.3, 0.26, t) for v in (-0.7, 0.0, 0.7)]), 0.3), foot=rect(*f3(-1.2, 0), 0.35, 1.8, t))
    dl = f3(-0.25, 0.35); di = o.on(disc(dl[0], dl[1], 0.42, 16), pn, 0.2); o.cut(ann(dl[0], dl[1], 0.36, 0.25, 16), di, 0.06)
    o.on(disc(dl[0], dl[1], 0.18, 8), di, 0.14)
    hc = f3(0.35, -0.4); o.on(U([ann(hc[0], hc[1], 0.42, 0.26, 12)] + [barl(hc, pol(hc[0], hc[1], 0.38, k * 2.094 + t), 0.12) for k in range(3)]), pn, 0.16)
    o.on(ell(*f3(0.45, 0.45), 0.14, 0.2, t, 8), pn, 0.08)
    spill = []
    for i in range(rng.randint(4, 8)):
        c = ff(u(-0.2, 2.8), rng.choice((-1, 1)) * u(1.5, 2.2)); spill.append(disc(c[0], c[1], 0.24, 10) if rng.random() < 0.7 else rect(c[0], c[1], 0.7, 0.36, u(0, 3)))
    o.st(ext(U(spill), 0.1), "gold", foot=U(spill))
    return o.out()

@x_d("frontier", "made", "iron", "potbelly_stove")
def _(cx, cy, a, rng):
    """a cast-iron potbelly stove: the round belly with its band, the firebox door with its draught vents, the
    collar and the domed top with its finial, the base ring and the three curled feet; standing, or knocked over onto its
    back, and the stovepipe lying beside it in sections with an elbow"""
    u = rng.uniform; ff = frame(cx, cy, a); o = Obj("iron"); Rb = u(1.15, 1.3)
    if rng.random() < 0.5:          # knocked over, lying on its back
        L = u(4.2, 4.6); zc = Rb * 0.98
        def rf(s):
            if s < 0.07: return Rb * 0.86
            if s < 0.12: return Rb * 0.76
            if s < 0.56: return Rb * (0.78 + 0.22 * math.sin(math.pi * (s - 0.12) / 0.44))
            if s < 0.6: return Rb * 0.86
            if s < 0.74: return Rb * 0.72
            if s < 0.8: return Rb * 0.8
            if s < 0.9: return Rb * (0.7 - 2.6 * (s - 0.8))
            return Rb * 0.26
        ps = t_bar(ff(-L / 2, 0), ff(L / 2, 0), rf, z0=zc, z1=zc)
        for v, z in ((-Rb * 0.55, 0.35), (Rb * 0.55, 0.35), (0.0, Rb * 1.4)):
            ps += t_bar(ff(-L / 2 + 0.05, v), ff(-L / 2 - 0.55, v * 1.2), 0.22, z0=z, z1=max(0.25, z - 0.2))
        dx = -L * 0.16; dg = rect(*ff(dx, 0), 1.05, 1.1, a)
        ps += ext(dg, zc + Rb + 0.12)
        o.st(ps, cuts=[(U([barl(ff(dx - 0.25, v), ff(dx + 0.3, v), 0.12) for v in (-0.25, 0.0, 0.25)]), zc + Rb + 0.03)], foot=rect(cx, cy, L + 1.0, 2 * Rb, a))
        sv = rng.choice((-1, 1)); e0 = ff(-L / 2 + 0.3, sv * (Rb + 0.6)); t = a + u(-0.12, 0.12); bud = L - 0.6
    else:          # standing on its feet
        hz = u(4.1, 4.5); c = ff(-2.1, 0)
        prof = [(Rb * 0.88, 0.45), (Rb * 0.88, 0.7), (Rb * 0.8, 0.78), (Rb, 1.6), (Rb, 2.1), (Rb * 0.82, 2.6), (Rb * 0.86, 2.66), (Rb * 0.86, 2.8), (Rb * 0.7, 2.86), (Rb * 0.7, 3.3), (Rb * 0.78, 3.36), (Rb * 0.78, 3.5)] + crv(Rb * 0.7, 0.32, 3.5, hz - 0.3, n=5) + [(0.22, hz - 0.3), (0.22, hz)]
        ps = turn(c[0], c[1], prof, 28) + [s for k in range(3) for s in cyl(*pol(c[0], c[1], Rb * 0.98, a + 0.5 + k * 2.094), 0.3, 0.55, n=8)]
        dp_ = pol(c[0], c[1], Rb - 0.1, a + math.pi); ps += ext(rect(dp_[0], dp_[1], 0.7, 1.1, a), 2.0)
        o.st(ps, cuts=[(disc(c[0], c[1], 0.38, 10), hz - 0.85)], foot=disc(c[0], c[1], Rb + 0.25))
        e0 = ff(-0.55, u(-0.5, 0.5)); t = a + u(-0.2, 0.2); bud = u(3.4, 3.8)
    # the stovepipe in sections, one with its elbow
    rp = 0.36; n = rng.randint(1, 2); p = e0; el = rng.random() < 0.6; bud -= 0.9 if el else 0.0
    for i in range(n):
        Lp = bud / n - 0.25; q = pol(p[0], p[1], Lp, t)
        o.st(t_bar(p, q, lambda s: rp + (0.06 if s < 0.08 else 0.0), 1.0), foot=LineString([p, q]).buffer(rp))
        p = pol(q[0], q[1], 0.25, t + u(-0.3, 0.3)); t += u(-0.5, 0.5)
    if el:
        tt = t + rng.choice((-1, 1)) * 1.5708; q = pol(p[0], p[1], 0.8, t)
        o.st(t_bar(p, q, rp, 1.0) + t_bar(q, pol(q[0], q[1], 0.9, tt), rp, 1.0), foot=LineString([p, q]).buffer(0.6))
    return o.out()

@x_d("frontier", "made", "board", "batwing_door")
def _(cx, cy, a, rng):
    """one leaf of a saloon's swinging doors, torn off its hinges and lying flat: the stiles and rails standing proud,
    the louvred lower panel, the turned spindles in the upper one, the batwing curve of its top rail, the spring hinge
    straps still screwed to it; some are shot through or have a louvre kicked out"""
    u = rng.uniform; ff = frame(cx, cy, a); L = u(4.5, 4.8); W = u(2.4, 2.7); o = Obj("board"); hs = rng.choice((-1, 1))
    m = 12; drop = u(0.6, 0.9)
    def utop(v): q = (hs * v + W / 2) / W; return L / 2 - drop * (math.sin(q * math.pi) * 0.6 + q * 0.5)
    pts = [ff(-L / 2, -W / 2), ff(-L / 2, W / 2)] + [ff(utop(v), v) for v in np.linspace(W / 2, -W / 2, m)]
    g = Polygon(pts).buffer(0.0); b = o.up(g, u(0.3, 0.34)); st = 0.42
    xm = -L / 2 + L * u(0.52, 0.58)
    lower = rect(*ff((-L / 2 + 0.55 + xm - 0.2) / 2, 0), xm - 0.2 - (-L / 2 + 0.55), W - 2 * st, a)
    upper = Polygon([ff(xm + 0.2, -W / 2 + st), ff(xm + 0.2, W / 2 - st)] + [ff(utop(v) - st, v) for v in np.linspace(W / 2 - st, -W / 2 + st, m)]).buffer(0.0)
    fr = g.difference(U([lower, upper]))
    o.st(ext(fr, 0.15), par=b)
    o.cut(U([barl(ff(x, -W), ff(x, W), 0.14) for x in np.arange(-L / 2 + 0.75, xm - 0.25, 0.3)]).intersection(inset(lower, 0.05)), b, 0.12)
    ns = rng.randint(4, 6); vs = np.linspace(-W / 2 + st, W / 2 - st, ns + 1)
    slots = U([rect(*ff(xm + 0.2 + L, (vs[i] + vs[i + 1]) / 2), 2 * L, (vs[i + 1] - vs[i]) - 0.3, a) for i in range(ns)])
    o.cut(slots.intersection(inset(upper, 0.12)), b, 0.2)
    o.cut(U([ell(*ff(xm + 0.2 + 0.25 + (j * 0.55), v), 0.08, 0.16, a, 6) for v in vs[1:-1] for j in range(2)]), b, 0.06)
    hv = hs * (W / 2 - 0.05)
    o.st(ext(U([rect(*ff(x, hv - hs * 0.4), 0.42, 0.85, a) for x in (-L * 0.32, L * 0.22)]), 0.12), "iron", par=b)
    o.studs([ff(x + dx, hv - hs * 0.55) for x in (-L * 0.32, L * 0.22) for dx in (-0.1, 0.1)], b, 0.2, 0.08, "iron", 8)
    if rng.random() < 0.5: o.cut(U([disc(*ff(u(-L * 0.3, L * 0.25), u(-W * 0.3, W * 0.3)), 0.17, 8) for _ in range(rng.randint(2, 4))]), b, 0.3)
    return o.out()

@x_d("frontier", "made", "brass", "church_bell")
def _(cx, cy, a, rng):
    """a church bell fallen from its tower and lying on its side: the crown with its hanging loop, the shoulder, the
    long waist with the raised bead lines and the inscription band, the sound bow flaring to the lip; the oak headstock
    across its crown with its iron straps and the gudgeons at its ends; the clapper lying in front of the mouth; some are
    cracked"""
    u = rng.uniform; ff = frame(cx, cy, a); Lb = u(3.7, 4.0); Rm = u(1.75, 1.95); o = Obj("brass"); x0 = -2.0
    def rf(s):
        r = Rm * (0.5 + 0.08 * s + 0.42 * s ** 3.2)
        if s > 0.86: r = Rm * (1.0 + 0.06 * math.sin((s - 0.86) / 0.14 * math.pi)) if s < 0.97 else Rm * 0.97
        if abs(s - 0.2) < 0.02 or abs(s - 0.3) < 0.02 or abs(s - 0.72) < 0.02: r += 0.09
        return r
    p = ff(x0, 0); q = ff(x0 + Lb, 0)
    cuts = []
    if rng.random() < 0.45: cuts.append((wavy(ff(x0 + Lb, u(-0.3, 0.3)), ff(x0 + Lb * u(0.45, 0.6), u(-0.6, 0.6)), rng, 0.25, 4, 0.16), Rm * 1.5))
    o.st(t_bar(p, q, rf, z0=Rm * 0.52, z1=Rm * 0.98), cuts=cuts, foot=rect(*ff(x0 + Lb / 2, 0), Lb, 2 * Rm, a))
    o.st(ext(ell(*ff(x0 - 0.35, 0), 0.4, 0.55, a, 12).exterior.buffer(0.14), Rm * 1.05), "brass", foot=disc(*ff(x0 - 0.35, 0), 0.6))          # the crown loop
    if rng.random() < 0.8:          # the headstock across the crown
        hx = x0 - 0.75; t = a + 1.5708 + u(-0.15, 0.15); hl = u(3.4, 3.9); f2 = frame(*ff(hx, 0), t)
        hg = rect(*ff(hx, 0), hl, 0.95, t); hb = o.up(hg, u(1.1, 1.25), "wood")
        o.cut(U([barl(f2(-hl, v), f2(hl, v), 0.1) for v in (-0.22, 0.2)]), hb, 0.08)
        o.on(U([rect(*f2(x, 0), 0.3, 0.95, t) for x in (-0.6, 0.6, -hl * 0.38, hl * 0.38)]), hb, 0.1, "iron")
        o.studs([f2(x, v) for x in (-0.6, 0.6) for v in (-0.25, 0.25)], hb, 0.2, 0.1, "iron", 8)
        o.st([s for sv in (-1, 1) for s in t_bar(f2(sv * (hl / 2 - 0.25), 0), f2(sv * (hl / 2 + 0.4), 0), 0.24, z0=0.6, z1=0.6)], "iron", foot=rect(*ff(hx, 0), hl + 0.8, 0.5, t))
    c = ff(x0 + Lb + u(0.85, 1.0), u(-0.9, 0.9)); t = a + u(-0.6, 0.6) + rng.choice((1.5708, -1.5708)); f3 = frame(c[0], c[1], t)
    o.st(dome(*f3(0.0, 0), 0.5, 0.75) + t_bar(f3(0.2, 0), f3(1.5, 0), lambda s: 0.24 if s < 0.85 else 0.32, 0.6), "iron", foot=disc(c[0], c[1], 0.6))
    return o.out()


_OWN = set(globals())
