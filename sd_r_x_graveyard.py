"""Third drawing, graveyard: the objects laid at their real size beside a 32 mm figure (1 m = 18 mm as printed).
Each is drawn at the height of the real thing and carries what that thing has: lead plugs and spear heads on a
railing, a skeleton in the open coffin, kerbs round the sunken grave, the cut letters on a fallen footstone.
Nothing overhangs: a cross, a bell post and an obelisk's top lie where they fell."""
import math
from sd_reg import d

_GLYPHS = {"H": ((0, 0, 0, 1), (1, 0, 1, 1), (0, .5, 1, .5)), "I": ((.5, 0, .5, 1),), "L": ((0, 0, 0, 1), (0, 0, 1, 0)),
           "T": ((0, 1, 1, 1), (.5, 0, .5, 1)), "E": ((0, 0, 0, 1), (0, 1, 1, 1), (0, .5, .8, .5), (0, 0, 1, 0)),
           "A": ((0, 0, .5, 1), (.5, 1, 1, 0), (.25, .45, .75, .45)), "M": ((0, 0, 0, 1), (0, 1, .5, .45), (.5, .45, 1, 1), (1, 1, 1, 0)),
           "N": ((0, 0, 0, 1), (0, 1, 1, 0), (1, 0, 1, 1)), "V": ((0, 1, .5, 0), (.5, 0, 1, 1)), "1": ((.5, 0, .5, 1), (.2, .8, .5, 1)),
           "7": ((0, 1, 1, 1), (1, 1, .35, 0)), "F": ((0, 0, 0, 1), (0, 1, 1, 1), (0, .5, .75, .5)), "K": ((0, 0, 0, 1), (0, .5, 1, 1), (.3, .65, 1, 0))}

def xon(o, g, par, h, kind=None):
    """a part standing on layer par as a plain "on" layer: at true scale it keeps parts down to a quarter of a
    millimetre wide (rivets, nail heads, spear tips) where Obj.on, going through ST(), drops anything under 0.3"""
    if g is None or g.is_empty: return None
    o.L.append(("on", g, par, h, kind or o.k)); return len(o.L) - 1

def xstuds(o, pts, par, r=0.2, h=0.1, kind=None, n=8):
    """rivets, bolt heads, nail heads: small discs standing on layer par"""
    return xon(o, U([disc(x, y, r, n) for x, y in pts]), par, h, kind) if pts else None

def xg_text(f2, x0, v0, n, size, w, rng, step=None, chars="HILTEAMNVFK17"):
    """n cut letters in a row along u from x0, centred on v0: each a few chisel strokes"""
    step = step or size * 0.95; gs = []; cw = size * 0.68
    for i in range(n):
        ch = rng.choice(chars); x = x0 + i * step
        if ch == " ": continue
        for p0, q0, p1, q1 in _GLYPHS[ch]:
            gs.append(barl(f2(x + p0 * cw, v0 + (q0 - 0.5) * size), f2(x + p1 * cw + 1e-3, v0 + (q1 - 0.5) * size + 1e-3), w))
    return U(gs)

def xg_coffin(f, L, W):
    """the six-sided coffin outline, widest at the shoulders (towards +L)"""
    return Polygon([f(-L / 2, -W * 0.3), f(L * 0.22, -W / 2), f(L / 2, -W * 0.32), f(L / 2, W * 0.32), f(L * 0.22, W / 2), f(-L / 2, W * 0.3)])

def xg_grain(f2, L, W, rng, n=2, w=0.12):
    """weathered grain: broken lines along a board"""
    gs = []
    for k in range(n):
        v = (k + 0.5 - n / 2) * W / (n + 0.4) + rng.uniform(-0.05, 0.05); x0 = rng.uniform(-L / 2, -L * 0.1); x1 = rng.uniform(L * 0.1, L / 2)
        gs.append(barl(f2(x0, v), f2(x1, v + rng.uniform(-0.06, 0.06)), w))
    return U(gs)


@d("graveyard", "made", "fence", "fallen_railing")
def _(cx, cy, a, rng):
    """a length of wrought-iron grave railing pulled down flat: square bars between two rails with a rivet at every
    crossing, a spear head with its collar on each bar, short dog bars at the foot; the bars still leaded into a
    broken length of the stone coping they stood in. A bar bent here, a spear snapped there."""
    u = rng.uniform; ff = frame(cx, cy, a); L = u(5.3, 6.3); W = u(3.0, 3.6); o = Obj("fence"); sp = u(0.62, 0.72)
    cl = L * u(0.55, 0.85); cx0 = u(-(L - cl) / 2, (L - cl) / 2); vb = -W / 2 - 0.05
    cg = Polygon([ff(cx0 - cl / 2, vb - 0.45), ff(cx0 + cl / 2, vb - 0.45), ff(cx0 + cl / 2 + u(-0.2, 0.1), vb + 0.1), ff(cx0 + cl / 2 - u(0.1, 0.35), vb + 0.45), ff(cx0 - cl / 2, vb + 0.45)])
    cp = o.up(cg, u(0.8, 0.95), "headstone"); ct = xon(o, inset(cg, 0.16), cp, 0.12, "headstone")
    xs = [x for x in np.arange(-L / 2 + 0.35, L / 2 - 0.2, sp)]; gone = {i for i in range(len(xs)) if rng.random() < 0.1}; bent = rng.randrange(len(xs))
    bars = []; heads = []; dogs = []; plugs = []
    for i, x in enumerate(xs):
        if i in gone: continue
        b = u(-0.5, 0.5) if i == bent else 0.0; top = W / 2 - 0.45
        bars.append(line([ff(x, vb + 0.25), ff(x, 0.0), ff(x + b, top)], 0.3))
        if abs(x - cx0) < cl / 2 - 0.2: plugs.append(ff(x, vb + 0.05))
        if rng.random() < 0.88: heads.append(U([tri(ff(x + b, top - 0.05), ff(x + b, top + 0.62), 0.62), Polygon([ff(x + b - 0.3, top + 0.12), ff(x + b, top - 0.1), ff(x + b + 0.3, top + 0.12), ff(x + b, top + 0.3)])]))
        if i < len(xs) - 1: dogs.append(U([barl(ff(x + sp / 2, vb + 0.3), ff(x + sp / 2, -W * 0.28), 0.26), tri(ff(x + sp / 2, -W * 0.36), ff(x + sp / 2, -W * 0.16), 0.42)]))
    o.up(U(dogs), 0.24); o.up(U(bars), 0.3); hd = o.up(U(heads), 0.34)
    rv = [-W * 0.3, W * 0.2]; br = u(L * 0.25, L * 0.45) * rng.choice((-1, 1)) if rng.random() < 0.4 else None          # one rail snapped short
    rails = [barl(ff(-L / 2 + 0.15, v), ff(L / 2 - 0.05, v), 0.42) if (br is None or k == 0) else barl(ff(-L / 2 + 0.15 if br < 0 else br, v), ff(L / 2 - 0.05 if br > 0 else br, v), 0.42) for k, v in enumerate(rv)]
    rl = o.up(U(rails), 0.44)
    xstuds(o, [ff(x, v) for i, x in enumerate(xs) if i not in gone for v in rv], rl, 0.17, 0.09, n=6)
    if plugs: xon(o, U([rect(x, y, 0.5, 0.5, a) for x, y in plugs]), ct, 0.07, "tin")
    o.cut(U([barl(ff(cx0 + cl * f, vb - 0.6), ff(cx0 + cl * f + u(-0.15, 0.15), vb + 0.6), 0.15) for f in (-0.2, 0.25) if rng.random() < 0.7]), ct, 0.16)
    return o.out()

@d("graveyard", "made", "coffin", "open_coffin")
def _(cx, cy, a, rng):
    """a coffin dug up and opened: plank walls with the rim moulding, brass grips on its sides, and in it the
    skeleton (skull, ribs about the spine, the pelvis, arm and leg bones) or the body in its bound shroud; the lid
    lies beside it with its raised panel, the cross, the name plate with its letters and the nails round the edge"""
    u = rng.uniform; ff = frame(cx, cy, a); L = u(5.4, 6.4); W = u(2.0, 2.3); h = u(1.3, 1.5); o = Obj("coffin")
    g = xg_coffin(ff, L, W); b = o.up(g.difference(inset(g, 0.3)), h); o.up(inset(g, 0.3).buffer(0.02), 0.32)          # walls and floor, so what lies in it is not cut away
    o.cut(U([barl(ff(x, -W), ff(x + 0.01, W), 0.12) for x in (-L * 0.1, L * 0.3)]), b, 0.08)
    o.up(U([rect(*ff(x, sv * (W / 2 - 0.02 if x > L * 0.05 else W * 0.33)), 0.55, 0.5, a) for x in (-L * 0.25, L * 0.12, L * 0.34) for sv in (-1, 1)]), h * 0.62, "brass")
    fl = 0.32
    if rng.random() < 0.65:          # the bones, lying a little out of order
        hx = L / 2 - 0.62; o.st(dome(*ff(hx, u(-0.08, 0.08)), 0.42, fl + 0.48, sq=0.82, a=a), "bone", cuts=[(disc(*ff(hx - 0.12, sv * 0.17), 0.12, 6), fl + 0.3) for sv in (-1, 1)])
        sx = L / 2 - 1.15; bones = [barl(ff(sx, 0), ff(-L * 0.12, 0), 0.24)]
        for k in range(rng.randint(4, 5)):
            x = sx - 0.12 - k * 0.3; bones += [line([ff(x, 0.08), ff(x - 0.08, 0.42), ff(x - 0.3, 0.58)], 0.18), line([ff(x, -0.08), ff(x - 0.08, -0.42), ff(x - 0.3, -0.58)], 0.18)]
        pv = -L * 0.14; bones += [ell(*ff(pv, sv * 0.3), 0.3, 0.24, a, 10) for sv in (-1, 1)]
        for sv in (-1, 1):
            k0 = ff(pv - 1.2, sv * u(0.18, 0.3)); bones += [capl(ff(pv - 0.15, sv * 0.3), k0, 0.24), capl(k0, ff(-L / 2 + 0.45, sv * u(0.12, 0.3)), 0.22)]
            e = ff(sx - 0.8, sv * 0.78); bones += [capl(ff(sx + 0.1, sv * 0.65), e, 0.2), capl(e, ff(pv + u(-0.2, 0.3), sv * u(0.35, 0.7)), 0.2)]
        o.st(ext(U(bones).intersection(inset(g, 0.32)), fl + 0.24), "bone")
    else:            # the shroud, bound at the neck, the waist and the ankles
        c = ff(-0.1, 0); sk = _S()._sack(c[0], c[1], a, L * 0.78, W * 0.62, 0.55, fl)
        o.st(sk, "cloth", cuts=[(rect(*ff(x, 0), 0.18, W, a), fl + 0.42) for x in (L * 0.24, -L * 0.06, -L * 0.32)])
    lt = a + u(-0.25, 0.25); f2 = frame(*ff(u(-0.6, 0.6), W + u(0.25, 0.6)), lt); lg = xg_coffin(f2, L, W)
    ld = o.up(lg, 0.4); pn = xon(o, inset(lg, 0.32), ld, 0.12)
    xon(o, U([barl(f2(-L * 0.28, 0), f2(L * 0.3, 0), 0.36), barl(f2(L * 0.14, -W * 0.27), f2(L * 0.14, W * 0.27), 0.36)]), pn, 0.13, "brass")
    pl = xon(o, rect(*f2(-L * 0.12, 0), 0.75, 0.6, lt), pn, 0.08, "brass"); o.cut(xg_text(f2, -L * 0.12 - 0.24, 0.0, 2, 0.34, 0.1, rng), pl, 0.07)
    xstuds(o, [f2(x, sv * (W * 0.3 + (W * 0.2 - abs(x - L * 0.22) / L * 0.0) - 0.2)) for x in np.arange(-L / 2 + 0.4, L / 2 - 0.2, 0.8) for sv in (-1, 1)], ld, 0.13, 0.08, "iron", 6)
    if rng.random() < 0.35: o.cut(line([f2(-L * 0.4, u(-0.3, 0.3)), f2(-L * 0.1, u(-0.2, 0.2)), f2(L * 0.3, u(-0.3, 0.3))], 0.16), ld, 0.3)          # split along the grain
    return o.out()

@d("graveyard", "marks", "pit", "sunken_grave", ground=True)
def _(cx, cy, a, rng):
    """a grave that has sunk: its kerbstones still round it, corner blocks with their pyramid tops, one kerb tipped
    down into the hollow, the turf cracked along the edges and slumped deepest over the coffin"""
    u = rng.uniform; ff = frame(cx, cy, a); L = u(5.4, 6.0); W = u(2.2, 2.5); o = Obj("headstone"); k = 0.42
    inner = rect(cx, cy, L, W, a)
    o.gcut(inner.buffer(-0.05), u(0.28, 0.36), "pit")
    o.gcut(blob(*ff(u(-0.6, 0.6), 0), W * 0.33, rng, 3).intersection(inner.buffer(-0.45)), u(0.6, 0.7), "pit")
    o.gcut(U([barl(ff(x, sv * (W / 2 - 0.32)), ff(x + u(0.6, 1.2), sv * (W / 2 - u(0.35, 0.6))), 0.15) for x in np.arange(-L / 2 + 0.5, L / 2 - 1.2, 1.4) for sv in (-1, 1) if rng.random() < 0.6]).intersection(inner), 0.12, "pit")
    drop = rng.choice(range(4)) if rng.random() < 0.6 else -1; sides = []
    for i, (p, q) in enumerate(((ff(-L / 2, -W / 2 - k / 2), ff(L / 2, -W / 2 - k / 2)), (ff(-L / 2, W / 2 + k / 2), ff(L / 2, W / 2 + k / 2)), (ff(-L / 2 - k / 2, -W / 2), ff(-L / 2 - k / 2, W / 2)), (ff(L / 2 + k / 2, -W / 2), ff(L / 2 + k / 2, W / 2)))):
        if i == drop:          # this one has gone down into the hollow, tipped
            m = ((p[0] + q[0]) / 2, (p[1] + q[1]) / 2); t = math.atan2(q[1] - p[1], q[0] - p[0]) + u(-0.12, 0.12); c = (m[0] + (cx - m[0]) * 0.18, m[1] + (cy - m[1]) * 0.18); n_ = math.dist(p, q) * 0.85
            kb = o.up(rect(c[0], c[1], n_, k, t), 0.3); o.cut(barl(pol(c[0], c[1], n_ * u(0.1, 0.3), t), pol(pol(c[0], c[1], n_ * u(0.1, 0.3), t)[0], pol(c[0], c[1], n_ * u(0.1, 0.3), t)[1], 0.6, t + 1.57), 0.14), kb, 0.2)
        else: sides.append(barl(p, q, k))
    kb = o.up(U(sides), u(0.5, 0.6)); o.cut(U([barl(ff(x, -W), ff(x + 0.02, W), 0.13) for x in (-L * 0.2, L * 0.2)]), kb, 0.1)
    cs = [ff(sx * (L / 2 + k / 2), sy * (W / 2 + k / 2)) for sx in (-1, 1) for sy in (-1, 1)]
    cb = o.up(U([rect(x, y, 0.7, 0.7, a) for x, y in cs]), 0.72); c2 = xon(o, U([rect(x, y, 0.42, 0.42, a) for x, y in cs]), cb, 0.12); xon(o, U([rect(x, y, 0.22, 0.22, a) for x, y in cs]), c2, 0.1)
    return o.out()

@d("graveyard", "made", "fence", "mortsafe")
def _(cx, cy, a, rng):
    """a mortsafe against the body-snatchers: a stone kerb, the heavy grave slab inside it held down by iron bands
    bolted at their ends, a hasp and padlock at the foot, and round it all a rank of spear-headed iron posts with
    thicker corner posts carrying ball finials"""
    u = rng.uniform; ff = frame(cx, cy, a); L = u(6.3, 7.1); W = u(2.6, 3.0); o = Obj("fence"); R = rect(cx, cy, L, W, a)
    kb = o.up(R, 0.5, "headstone"); sl = xon(o, inset(R, 0.5), kb, 0.25, "headstone"); o.groove(inset(R, 0.7), sl, 0.14, 0.08)
    nb = rng.randint(3, 4); xb = [(i + 0.5) / nb * (L - 1.6) - (L - 1.6) / 2 for i in range(nb)]
    bd = xon(o, U([rect(*ff(x, 0), 0.5, W - 0.9, a) for x in xb]), sl, 0.18, "iron")
    xstuds(o, [ff(x, sv * (W / 2 - 0.62)) for x in xb for sv in (-1, 1)], bd, 0.16, 0.08, "iron", 6)
    hp = xon(o, rect(*ff(-L / 2 + 0.95, 0), 0.5, 0.8, a), sl, 0.12, "iron"); xon(o, disc(*ff(-L / 2 + 0.95, 0), 0.27, 10), hp, 0.12, "iron")
    if rng.random() < 0.6: o.cut(xg_text(ff, -L * 0.1, 0.0, rng.randint(3, 4), 0.42, 0.11, rng, 0.52).difference(U([rect(*ff(x, 0), 0.62, W, a) for x in xb])), sl, 0.08)
    ps = []; e = 0.62
    for v in (-W / 2 + 0.22, W / 2 - 0.22):
        ps += [ff(x, v) for x in np.linspace(-L / 2 + e, L / 2 - e, max(3, int((L - 2 * e) / 0.85) + 1))]
    for x in (-L / 2 + 0.22, L / 2 - 0.22):
        ps += [ff(x, v) for v in np.linspace(-W / 2 + e, W / 2 - e, max(2, int((W - 2 * e) / 0.85) + 1))]
    lean = {i for i in range(len(ps)) if rng.random() < 0.08}
    ps = [p for i, p in enumerate(ps) if i not in lean]
    hp_ = u(1.6, 1.9); po = xon(o, U([rect(x, y, 0.32, 0.32, a + 0.785) for x, y in ps]), kb, hp_, "iron"); xon(o, U([rect(x, y, 0.18, 0.18, a + 0.785) for x, y in ps]), po, 0.22, "iron")
    cs = [ff(sx * (L / 2 - 0.3), sy * (W / 2 - 0.3)) for sx in (-1, 1) for sy in (-1, 1)]
    cp = xon(o, U([rect(x, y, 0.56, 0.56, a) for x, y in cs]), kb, hp_ + 0.3, "iron")
    for x, y in cs: o.st(dome(x, y, 0.28, 0.34), "iron", par=cp)
    return o.out()

@d("graveyard", "made", "headstone", "effigy_slab")
def _(cx, cy, a, rng):
    """a tomb slab with its recumbent effigy: the chamfered slab with a border line and the cut inscription, the
    figure lying on it in a long robe with folds down to the feet, hands joined on the breast, the head on a tasselled
    cushion and a little dog at the feet; on some a sword laid along the body, the head broken away or a crack across"""
    u = rng.uniform; ff = frame(cx, cy, a); L = u(6.0, 7.0); W = u(2.6, 3.0); o = Obj("headstone"); g = rect(cx, cy, L, W, a)
    sb = o.up(g, u(0.7, 0.85)); ch = xon(o, inset(g, 0.18), sb, 0.12); o.groove(inset(g, 0.42), ch, 0.13, 0.08)
    for sv in (-1, 1):
        if rng.random() < 0.8: o.cut(xg_text(ff, -L / 2 + u(0.7, 1.2), sv * (W / 2 - 0.62), rng.randint(7, 10), 0.3, 0.1, rng, 0.36), ch, 0.08)
    if rng.random() < 0.3: o.cut(line([ff(u(-1.5, 1.5), -W), ff(u(-0.6, 0.6), 0), ff(u(-1.5, 1.5), W)], 0.16), ch, 0.5)
    bl = L * 0.6; x0 = -L * 0.07; hw = W * 0.24; hx = x0 + bl / 2 + 0.45
    cu = xon(o, rect(*ff(hx + 0.05, 0), 0.95, hw * 2 * 0.95, a), ch, 0.22); xstuds(o, [ff(hx + 0.05 + sx * 0.4, sy * hw * 0.88) for sx in (-1, 1) for sy in (-1, 1)], cu, 0.13, 0.06, n=6)
    def rf(t):
        s = (t + bl / 2) / bl
        return hw * (0.62 + 0.38 * min(1.0, s / 0.75)) * (1.0 if s < 0.93 else max(0.55, 1 - (s - 0.93) * 6))
    ps = spin(*ff(x0, 0), a, bl, rf, 1.0)
    folds = [(barl(ff(x0 - bl / 2 + 0.1, v), ff(x0 + bl * 0.05, v * 0.8), 0.17), hw * 0.78) for v in (-hw * 0.45, 0.0, hw * 0.45)]
    ps += spin(*ff(x0 + bl * 0.22, 0), a, 0.9, lambda t: 0.2 * (1 - abs(t) / 0.6), 1.2, lift=hw * 0.75)          # the joined hands
    ps += [s for sv in (-1, 1) for s in dome(*ff(x0 - bl / 2 + 0.25, sv * hw * 0.32), 0.24, hw * 0.7, sq=0.8, a=a)]
    sword = rng.random() < 0.4
    if sword: ps += ext(U([barl(ff(x0 - bl * 0.38, hw * 0.4), ff(x0 + bl * 0.12, hw * 0.4), 0.2), barl(ff(x0 + bl * 0.06, hw * 0.12), ff(x0 + bl * 0.06, hw * 0.68), 0.2)]), hw * 0.95)
    if rng.random() < 0.75: ps += dome(*ff(hx, 0), hw * 0.62, hw * 1.0 + 0.22, sq=0.85, a=a)
    o.st(ps, par=ch, cuts=folds)
    dx = x0 - bl / 2 - 0.5; o.st(loaf(*ff(dx, 0), a + 1.57, hw * 1.4, 0.6, 0.45) + dome(*ff(dx, hw * 0.55), 0.26, 0.55), par=ch)
    return o.out()

@d("graveyard", "made", "headstone", "broken_obelisk")
def _(cx, cy, a, rng):
    """a broken obelisk: two plain steps, the die block and the plinth of the shaft, the shaft snapped off above it
    with a jagged top; the fallen part lies beside it tapering to its pyramid point, a line of letters cut down its
    face and chips of stone lying by the break"""
    u = rng.uniform; ff = frame(cx, cy, a); S = u(2.0, 2.4); o = Obj("headstone")
    b1 = o.up(rect(cx, cy, S, S, a), 0.45); b2 = xon(o, rect(cx, cy, S * 0.84, S * 0.84, a), b1, 0.42)
    dd = xon(o, rect(cx, cy, S * 0.66, S * 0.66, a), b2, u(1.3, 1.6)); o.groove(rect(cx, cy, S * 0.66 - 0.34, S * 0.66 - 0.34, a), dd, 0.12, 0.07)
    pl = xon(o, rect(cx, cy, S * 0.5, S * 0.5, a), dd, 0.2); sw = S * 0.38; st = xon(o, rect(cx, cy, sw, sw, a + u(-0.04, 0.04)), pl, u(0.7, 1.0))
    o.cut(Polygon([ff(-sw, -sw * 0.1), ff(sw * u(0.0, 0.4), sw), ff(sw, sw), ff(sw, -sw)]), st, u(0.35, 0.55))
    o.cut(tri(ff(-sw, sw * 0.3), ff(sw * 0.1, sw * u(0.1, 0.4)), sw * 0.5), st, 0.2)
    c = ff(S / 2 + u(2.2, 2.7), u(-0.6, 0.6)); b = a + u(-0.5, 0.5); f2 = frame(c[0], c[1], b); Lf = u(2.6, 3.4); w0 = sw * 0.95; w1 = sw * 0.72; tp = 0.55
    n = 8; ps = []; xs = np.linspace(-Lf / 2, Lf / 2 - tp, n + 1)
    for i in range(n):
        s = (i + 0.5) / n; w = w0 + (w1 - w0) * s; ps += ext(rect(*f2((xs[i] + xs[i + 1]) / 2, 0), xs[i + 1] - xs[i] + 0.02, w, b), w * 0.95)
    ps += ext(Polygon([f2(Lf / 2 - tp, -w1 / 2), f2(Lf / 2, 0), f2(Lf / 2 - tp, w1 / 2)]), w1 * 0.55) + ridge(*f2(Lf / 2 - tp / 2, 0), b, tp, w1, w1 * 0.95)
    jag = Polygon([f2(-Lf / 2 - 0.05, -w0), f2(-Lf / 2 + u(0.15, 0.4), -w0 * 0.2), f2(-Lf / 2 + u(0.0, 0.2), w0 * 0.15), f2(-Lf / 2 + u(0.2, 0.45), w0), f2(-Lf / 2 - 0.05, w0)])
    cuts = [(jag, w0 * 0.45)]
    lt = xg_text(f2, -Lf / 2 + 0.5, 0.0, rng.randint(3, 4), 0.5, 0.16, rng, 0.56)
    cuts += [(q, (w0 + (w1 - w0) * ((q.centroid.x - c[0]) * math.cos(b) + (q.centroid.y - c[1]) * math.sin(b) + Lf / 2) / Lf) * 0.95 - 0.08) for q in polys_of(lt)]
    o.st(ps, cuts=cuts, foot=rect(c[0], c[1], Lf, w0, b))
    for i in range(rng.randint(2, 4)): p = f2(-Lf / 2 - u(0.3, 0.9), u(-1.0, 1.0)); o.st(rock(p[0], p[1], u(0.22, 0.35), u(0.2, 0.35), rng))
    return o.out()

@d("graveyard", "made", "headstone", "footstones")
def _(cx, cy, a, rng):
    """a row of footstones: small round- or gable-headed stones standing in their turf, a little out of true, one or
    two fallen flat on their backs showing the cut initials and the year and the line round the head"""
    u = rng.uniform; ff = frame(cx, cy, a); n = rng.randint(3, 5); o = Obj("headstone"); any_ = False
    for i in range(n):
        if rng.random() < 0.12: continue
        any_ = True; c = ff((i - (n - 1) / 2) * 2.0 + u(-0.15, 0.15), u(-0.2, 0.2)); w = u(1.15, 1.45); h = u(1.6, 2.2); t = a + u(-0.2, 0.2); f2 = frame(c[0], c[1], t)
        if rng.random() < 0.3:          # fallen on its back, the head away from the row
            sv = rng.choice((-1, 1)); f3 = frame(c[0], c[1], t + sv * 1.5708)
            g = U([rect(*f3(-0.2, 0), h - 0.4, w, t + sv * 1.5708), ell(*f3(h / 2 - 0.42, 0), w * 0.5, 0.42, t + sv * 1.5708, 16)])
            sb = o.up(g, 0.42); o.groove(inset(g, 0.2), sb, 0.12, 0.07)
            ft = frame(*f3(0.1, 0), t + sv * 1.5708 - 1.5708); o.cut(U([xg_text(ft, -0.42, 0.22, 2, 0.4, 0.11, rng, 0.48, "HILTEAMNVFK"), xg_text(ft, -0.5, -0.35, 4, 0.3, 0.1, rng, 0.3, "17")]), sb, 0.08)
            continue
        o.st(dome(c[0], c[1], w * 0.62, 0.22, sq=0.6, a=t), "grass", foot=disc(c[0], c[1], w * 0.6))
        th = u(0.45, 0.55); gable = rng.random() < 0.35
        ps = ext(rect(c[0], c[1], w, th, t), h * 0.78)
        if gable: ps += ext(rect(c[0], c[1], w * 0.62, th, t), h * 0.9) + ext(rect(c[0], c[1], w * 0.3, th, t), h)
        else: ps += ext(rect(c[0], c[1], w * 0.85, th, t), h * 0.9) + ext(rect(c[0], c[1], w * 0.58, th, t), h)
        o.st(ps, cuts=[(barl(f2(-w * 0.3, 0), f2(w * 0.3, 0), 0.16), h * 0.86)] if rng.random() < 0.5 else [], foot=rect(c[0], c[1], w, th, t))
    if not any_: return None
    return o.out([rect(cx, cy, n * 2.0 + 0.4, 1.8, a)])

@d("graveyard", "made", "wood", "wooden_cross")
def _(cx, cy, a, rng):
    """a wooden grave cross fallen flat: the post with its pointed foot still dark from the ground, the arm let into it
    with the joint lines showing, a little pitched cap on the head, the name board nailed over the crossing with its
    letters, the grain split along the boards; on some the arm has come away and lies beside it"""
    u = rng.uniform; ff = frame(cx, cy, a); L = u(3.2, 3.7); W = u(1.6, 2.0); o = Obj("wood"); th = u(0.34, 0.4); pw = 0.52
    xc = L / 2 - L * 0.3; loose = rng.random() < 0.2
    post = U([rect(*ff(-0.1, 0), L - 0.4, pw, a), tri(ff(-L / 2 + 0.25, 0), ff(-L / 2 - 0.2, 0), pw)])
    p = o.up(post, th); o.cut(xg_grain(ff, L * 0.8, pw, rng, 1, 0.11), p, 0.1)
    if loose:
        t2 = a + u(0.9, 1.3) * rng.choice((-1, 1)); c = ff(u(-0.6, 0.2), rng.choice((-1, 1)) * u(0.9, 1.2)); f2 = frame(c[0], c[1], t2)
        o.cut(U([barl(ff(xc, -pw), ff(xc, pw), 0.12), barl(ff(xc + pw, -pw), ff(xc + pw, pw), 0.12)]), p, 0.14)
        ar = o.up(rect(c[0], c[1], W, pw, t2), th); o.cut(U([xg_grain(f2, W * 0.8, pw, rng, 1, 0.11), rect(c[0], c[1], pw, pw - 0.16, t2)]), ar, 0.1)
    else:
        ar = o.up(rect(*ff(xc, 0), pw, W, a), th + 0.02); o.cut(U([barl(ff(xc - pw / 2, -W), ff(xc - pw / 2, W), 0.11), barl(ff(xc + pw / 2, -W), ff(xc + pw / 2, W), 0.11)]), ar, 0.1)
        nb = xon(o, rect(*ff(xc, 0), pw * 1.1, W * 0.46, a), ar, 0.1); o.cut(xg_text(frame(*ff(xc, 0), a + 1.5708), -W * 0.17, 0.0, 3, 0.26, 0.09, rng, 0.17), nb, 0.07)
        xstuds(o, [ff(xc, sv * (W / 2 - 0.15)) for sv in (-1, 1)], ar, 0.11, 0.06, "iron", 6)
    cp = xon(o, rect(*ff(L / 2 - 0.55, 0), 0.42, pw + 0.04, a), p, 0.12); o.cut(barl(ff(L / 2 - 0.55, -pw), ff(L / 2 - 0.55, pw), 0.1), cp, 0.08)
    return o.out()

@d("graveyard", "made", "brass", "grave_bell", ground=True)
def _(cx, cy, a, rng):
    """a safety-coffin bell come down: the iron breathing pipe still stands out of the grave with its flange and open
    mouth, beside it the stump of the post snapped short; the rest of the post lies on the grass with its iron arm,
    the bell fallen on its side under the arm's end, and the cord runs back from the bell into the pipe"""
    u = rng.uniform; ff = frame(cx, cy, a); o = Obj("iron")
    pc = ff(-1.35, u(-0.2, 0.2)); fl = o.up(disc(pc[0], pc[1], 0.55, 16), 0.14); pp = xon(o, disc(pc[0], pc[1], 0.32, 14), fl, u(1.1, 1.4)); o.cut(disc(pc[0], pc[1], 0.17, 10), pp, 0.5)
    sc = ff(-1.0, u(0.55, 0.7)); sb = o.up(rect(sc[0], sc[1], 0.52, 0.52, a), u(0.9, 1.2), "wood"); o.cut(tri(ff(-1.25, 0.6), ff(-0.75, u(0.5, 0.8)), 0.4), sb, 0.3)
    t = a + u(-0.35, 0.35); f2 = frame(*ff(-0.65, 0.6), t); Lp = u(1.9, 2.3)
    pg = Polygon([f2(u(0.0, 0.2), -0.25), f2(Lp, -0.25), f2(Lp, 0.25), f2(u(0.1, 0.3), 0.25), f2(u(0.25, 0.45), 0.0)])
    po = o.up(pg, 0.46, "wood"); o.cut(xg_grain(frame(*f2(Lp / 2, 0), t), Lp * 0.8, 0.5, rng, 1, 0.1), po, 0.1)
    sv = rng.choice((-1, 1)); arm = o.up(U([rect(*f2(Lp - 0.15, sv * 0.55), 0.3, 1.1, t), rect(*f2(Lp - 0.35, sv * 0.28), 0.6, 0.28, t + 0.785)]), 0.3)
    xstuds(o, [f2(Lp - 0.15, sv * 0.12)], arm, 0.11, 0.07, n=6)
    bc = f2(Lp + u(-0.3, 0.1), sv * 1.35); bt = t + sv * u(1.2, 1.9); fb = frame(bc[0], bc[1], bt); bl = 1.3
    rf = lambda q: 0.3 + 0.28 * ((q + bl / 2) / bl) ** 1.6 + (0.06 if q > bl / 2 - 0.12 else 0.0)
    o.st(spin(bc[0], bc[1], bt, bl, rf, 1.0) + ext(ann(*fb(-bl / 2 - 0.12, 0), 0.22, 0.09, 8), 0.24), "brass", cuts=[(ell(*fb(bl / 2 - 0.14, 0), 0.14, 0.36, bt, 10), 0.4)], foot=disc(bc[0], bc[1], 0.75))
    pts = wiggle(fb(-bl / 2 - 0.3, 0), pol(pc[0], pc[1], 0.38, math.atan2(bc[1] - pc[1], bc[0] - pc[0])), rng, 0.35, 4)
    o.st(rod(pts, 0.25, 1.1), "rope", foot=LineString(pts).buffer(0.3))
    return o.out()

@d("graveyard", "made", "iron", "robbers_hole", ground=True)
def _(cx, cy, a, rng):
    """where the resurrection men dug: the hole at the head of the grave, deepest at the bottom, the spoil thrown up
    beside it in clods, the splintered boards of the coffin lid they broke and dragged out, the crowbar with its claw
    and bent end dropped by them, and on some their dark lantern or the sack and a coil of rope"""
    u = rng.uniform; ff = frame(cx, cy, a); o = Obj("iron")
    p = blob(cx, cy, u(1.5, 1.9), rng, 4); o.gcut(p, u(0.6, 0.85), "pit"); o.gcut(inset(p, 0.55), u(1.1, 1.4), "pit")
    mc = ff(u(2.8, 3.4), u(-1, 1)); rm = u(1.3, 1.6); hm = u(1.4, 1.9); t0 = u(0, 6.28)
    hp = dome(mc[0], mc[1], rm, hm, sq=u(0.75, 0.9), a=t0) + [s for k in range(3) for s in dome(*pol(mc[0], mc[1], rm * 0.75, t0 + k * 2.1 + u(-0.4, 0.4)), rm * u(0.45, 0.6), hm * u(0.4, 0.6))]
    o.st(hp, "earth", foot=disc(mc[0], mc[1], rm * 1.3))
    for i in range(rng.randint(3, 5)): q = pol(mc[0], mc[1], u(0.4, 1.4), u(0, 6.28)); o.st(rock(q[0], q[1], u(0.3, 0.45), u(0.25, 0.4), rng, lift=hm * max(0.0, 1 - (math.dist(q, mc) / rm) ** 2) * 0.85), "earth", foot=disc(q[0], q[1], 0.4))
    f2 = frame(*ff(u(-1, 1), -3.0), a + u(-0.4, 0.4)); L = u(5.0, 6.5)
    o.st(rod([f2(-L / 2, 0), f2(L / 2 - 0.6, 0), f2(L / 2, 0.55)], 0.27, 1.15) + ext(U([tri(f2(-L / 2 + 0.05, 0), f2(-L / 2 - 0.45, 0.2), 0.5), tri(f2(-L / 2 + 0.05, 0), f2(-L / 2 - 0.45, -0.2), 0.5)]), 0.22), foot=rect(*f2(0, 0), L, 1.2, a))
    if rng.random() < 0.75:          # splintered lid boards
        t = a + u(1.2, 1.9); c = pol(cx, cy, u(1.2, 1.8), a + u(2.2, 4.0)); fb = frame(c[0], c[1], t)
        for k in range(rng.randint(1, 2)):
            Lb = u(1.8, 2.4); v = k * 0.78; ty = t + u(-0.15, 0.15)
            g = Polygon([fb(-Lb / 2, v - 0.34), fb(Lb / 2, v - 0.34), fb(Lb / 2 + u(0.1, 0.5), v - 0.1), fb(Lb / 2 - u(0.0, 0.3), v + 0.05), fb(Lb / 2 + u(0.1, 0.4), v + 0.34), fb(-Lb / 2, v + 0.34)])
            bb = o.up(affinity.rotate(g, ty - t, origin=c, use_radians=True), 0.32, "coffin"); o.cut(xg_grain(frame(*fb(0, v), ty), Lb * 0.7, 0.68, rng, 1, 0.1), bb, 0.1)
    if rng.random() < 0.6:          # the dark lantern, set down on the spoil's edge
        lc = pol(mc[0], mc[1], u(1.7, 2.1), u(0, 6.28)); ln = o.up(disc(lc[0], lc[1], 0.42, 14), 1.0, "tin"); o.st(cone(lc[0], lc[1], 0.42, 0.36, 0.14), "tin", par=ln)
        xon(o, rect(*pol(lc[0], lc[1], 0.32, a), 0.3, 0.42, a), ln, 0.08, "glass")
    elif rng.random() < 0.6:
        rc = ff(u(-0.5, 0.5), u(1.8, 2.3)); o.st(rod(spiral(rc[0], rc[1], 0.25, 0.85, 2.2, u(0, 6.28), 30), 0.25, 1.0), "rope", foot=disc(rc[0], rc[1], 1.0))
    return o.out()


_OWN = set(globals())
