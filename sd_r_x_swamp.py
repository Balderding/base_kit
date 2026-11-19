"""Third drawing, swamp: the objects at their real size beside a 32 mm figure (1 m = 18 mm as printed), each at the
height of the real thing: old pilings knee to hip high with their rotted heads, a dugout with its thwarts and
adze marks, a wicker trap with its hoops, a carved post with its faces. Nothing overhangs."""
import math
from sd_reg import d

def xon(o, g, par, h, kind=None):
    """a part standing on layer par as a plain "on" layer (keeps parts down to a quarter of a millimetre at true
    scale, where Obj.on, through ST(), drops anything under 0.3)"""
    if g is None or g.is_empty: return None
    o.L.append(("on", g, par, h, kind or o.k)); return len(o.L) - 1

def xs_groove(ff, x0, x1, v, rf, hk, n=6, w=0.17, dz=0.12, lift=0.0):
    """a groove along a lying round body (spin) at offset v, cut in short pieces so its floor follows the surface"""
    out = []
    for i in range(n):
        a_ = x0 + (x1 - x0) * i / n; b_ = x0 + (x1 - x0) * (i + 1) / n; r = rf((a_ + b_) / 2)
        if abs(v) < r * 0.85: out.append((barl(ff(a_, v), ff(b_, v), w), hk * 0.93 * math.sqrt(max(0.0, r * r - (abs(v) + w / 2) ** 2)) - dz + lift))
    return out


@d("swamp", "made", "log", "dugout")
def _(cx, cy, a, rng):
    """a dugout canoe drawn up on the mud: one log hollowed out, pointed at both ends with a carved lug and rope hole
    at bow and stern, one or two thwarts left standing in the solid, adze marks across the floor; the paddle lies
    beside it with its midrib and grip, the bow rope runs to a mooring stake, and on some a fish spear lies by it"""
    u = rng.uniform; ff = frame(cx, cy, a); L = u(7.0, 8.4); W = u(1.3, 1.5); hk = 1.2; o = Obj("log")
    rf = lambda t: W / 2 * max(0.0, 1 - abs(2 * t / L) ** 2.4) ** 0.55 + 0.04
    hol = Polygon([ff(t, rf(t) - 0.3) for t in np.linspace(-L * 0.4, L * 0.4, 14)] + [ff(t, -rf(t) + 0.3) for t in np.linspace(L * 0.4, -L * 0.4, 14)])
    th = [ff(x, 0) for x in rng.sample((-L * 0.2, L * 0.05, L * 0.22), rng.randint(1, 2))]
    hol = hol.difference(U([rect(x, y, 0.32, W, a) for x, y in th]))
    adz = U([barl(ff(x, -W), ff(x + 0.1, W), 0.16) for x in np.arange(-L * 0.36, L * 0.36, 0.42)]).intersection(inset(hol, 0.12))
    o.st(spin(cx, cy, a, L, rf, hk, lift=0.25), cuts=[(hol, 0.55), (adz, 0.47)] + [(disc(*ff(sx * (L / 2 - 0.45), 0), 0.12, 6), 0.0) for sx in (-1, 1)], foot=rect(cx, cy, L, W, a))
    for sx in (-1, 1): o.st(ext(rect(*ff(sx * (L / 2 - 0.55), 0), 0.5, 0.5, a), rf(sx * (L / 2 - 0.55)) * hk * 0.93 + 0.37), foot=disc(*ff(sx * (L / 2 - 0.55), 0), 0.3))
    pd = frame(*ff(u(-1, 1), W / 2 + 0.7), a + u(-0.2, 0.2))
    o.st(rod([pd(-2.2, 0), pd(0.8, 0)], 0.25, 1.1) + ext(rect(*pd(-2.35, 0), 0.25, 0.8, a), 0.3), foot=rect(*pd(-0.7, 0), 3.2, 0.8, a))
    bl = ell(*pd(1.75, 0), 1.0, 0.45, a, 16); bd = o.up(bl, 0.22); xon(o, rect(*pd(1.55, 0), 1.5, 0.22, a), bd, 0.1)
    sx = rng.choice((-1, 1)); p0 = ff(sx * (L / 2 - 0.45), 0); sk = ff(sx * (L / 2 - u(1.4, 2.2)), -(W / 2 + u(1.0, 1.4)))
    pts = wiggle(pol(p0[0], p0[1], 0.3, math.atan2(sk[1] - p0[1], sk[0] - p0[0])), pol(sk[0], sk[1], 0.3, math.atan2(p0[1] - sk[1], p0[0] - sk[0])), rng, 0.3, 4)
    o.st(rod(pts, 0.25, 1.0), "rope", foot=LineString(pts).buffer(0.3))
    o.st(turn(sk[0], sk[1], [(0.3, 0.0), (0.28, u(1.3, 1.6)), (0.12, u(1.7, 1.9))], 10), foot=disc(sk[0], sk[1], 0.3))
    if rng.random() < 0.4:
        f3 = frame(*ff(u(-1, 1), -(W / 2 + 0.55)), a + u(-0.15, 0.15)); Ls = u(4.5, 5.5)
        o.st(rod([f3(-Ls / 2, 0), f3(Ls / 2 - 0.4, 0)], 0.25, 1.0) + ext(U([tri(f3(Ls / 2 - 0.5, v), f3(Ls / 2 + 0.4, v * 1.4), 0.3) for v in (-0.3, 0.0, 0.3)] + [barl(f3(Ls / 2 - 0.5, -0.35), f3(Ls / 2 - 0.5, 0.35), 0.3)]), 0.25), "iron", foot=rect(*f3(0, 0), Ls, 0.8, a))
    o.gcut(blob(*ff(u(-L / 4, L / 4), u(-0.3, 0.3)), W * 1.1, rng, 4).difference(rect(cx, cy, L, W + 0.4, a)), 0.18, "water")
    return o.out()

@d("swamp", "made", "log", "pilings")
def _(cx, cy, a, rng):
    """the last pilings of a rotted landing: thick posts standing knee to hip high, their heads sawn or rotted to a
    jagged hollow with the growth rings and drying checks in the end grain, weed and slime round their feet, the
    fallen stringer lying between them with a bolt still standing in it and a rope tied off to one post"""
    u = rng.uniform; ff = frame(cx, cy, a); n = rng.randint(3, 4); sp = u(1.55, 1.8); o = Obj("log"); cs = []
    for i in range(n):
        c = ff((i - (n - 1) / 2) * sp, u(-0.35, 0.35)); r = u(0.5, 0.62); h = u(1.5, 3.8); cs.append((c, r, h)); t = u(0, 6.28)
        cuts = [(ann(c[0], c[1], rr + 0.08, rr - 0.08, 16), h - 0.07) for rr in (r * 0.42, r * 0.72)] + [(barl(c, pol(c[0], c[1], r, t + k * 2.2), 0.14), h - 0.1) for k in range(rng.randint(1, 2))]
        if rng.random() < 0.55: cuts += [(Polygon([pol(c[0], c[1], r * 1.2, t + 1.0), pol(c[0], c[1], r * 1.2, t + 2.6), pol(c[0], c[1], 0.1, t + 1.8)]), h - u(0.3, 0.6)), (disc(*pol(c[0], c[1], r * 0.15, t), r * 0.4, 8), h - u(0.3, 0.5))]
        o.st(turn(c[0], c[1], [(r * 1.08, 0.0), (r, 0.5), (r * 0.97, h * 0.7), (r * 1.0, h)], 12) + [s for k in range(rng.randint(3, 5)) for s in dome(*pol(c[0], c[1], r * 0.9, t + k * 1.4 + u(0, 0.5)), u(0.3, 0.42), u(0.3, 0.5), n=10)], cuts=cuts, foot=disc(c[0], c[1], r + 0.3))
    i = rng.randrange(n - 1); c0, c1 = cs[i][0], cs[i + 1][0]; m = ((c0[0] + c1[0]) / 2, (c0[1] + c1[1]) / 2); t = math.atan2(c1[1] - c0[1], c1[0] - c0[0]) + u(-0.3, 0.3)
    m = pol(m[0], m[1], u(0.9, 1.2), t + rng.choice((-1, 1)) * 1.5708); f2 = frame(m[0], m[1], t); Lb = sp * u(1.5, 1.9)
    bb = o.up(rect(m[0], m[1], Lb, 0.7, t), 0.62); o.cut(U([barl(f2(-Lb / 2, v), f2(Lb / 2, v + 0.03), 0.12) for v in (-0.12, 0.16)]), bb, 0.08)
    bp = f2(Lb * u(0.15, 0.35) * rng.choice((-1, 1)), 0); xon(o, disc(bp[0], bp[1], 0.17, 8), bb, 0.45, "iron")
    o.cut(disc(*f2(-Lb * 0.3, 0), 0.14, 8), bb, 0.3)
    c, r, h = cs[rng.randrange(n)]; e = pol(c[0], c[1], r + u(1.6, 2.4), a + rng.choice((-1, 1)) * u(1.2, 1.9)); pts = wiggle(pol(c[0], c[1], r + 0.1, math.atan2(e[1] - c[1], e[0] - c[0])), e, rng, 0.35, 4)
    o.st(rod(pts, 0.25, 1.0), "rope", foot=LineString(pts).buffer(0.3))
    o.gcut(U([disc(c[0], c[1], r + u(0.6, 0.9), 14) for c, r, h in cs]).buffer(0.3).buffer(-0.3), 0.15, "water")
    return o.out()

@d("swamp", "made", "log", "totem_stake")
def _(cx, cy, a, rng):
    """a carved fetish post fallen in the reeds: its stake end sharpened and charred where it stood in the mud, then
    two or three faces cut one above the other, each with a heavy brow, round staring eyes, a long nose and the
    slot of a mouth, collars between them, and at the head a beaked bird; cords hang from a collar with a bone tied
    to the end of each"""
    u = rng.uniform; ff = frame(cx, cy, a); L = u(6.6, 7.6); r = u(0.62, 0.72); hk = 1.1; lf = r * 0.8; nh = rng.randint(2, 3); x0 = -L / 2 + 1.3; hl = (L - 2.4) / nh
    def rf(t):
        if t < -L / 2 + 1.1: return r * max(0.25, (t + L / 2) / 1.1)
        if t > L / 2 - 1.0: return r * 0.85
        s = ((t - x0) % hl) / hl
        return r * (1.12 if s < 0.1 else 1.0 + 0.1 * math.exp(-((s - 0.32) / 0.07) ** 2))
    hz = lambda v, t: lf * min(1.0, rf(t) / r) + hk * 0.93 * math.sqrt(max(0.0, rf(t) ** 2 - v * v))
    ps = spin(cx, cy, a, L, rf, hk, lift=lf); cuts = []
    for k in range(nh):
        xe = x0 + k * hl + hl * 0.45; xm = x0 + k * hl + hl * 0.8
        cuts += [(disc(*ff(xe, sv * r * 0.38), 0.2, 10), hz(r * 0.38, xe) - 0.16) for sv in (-1, 1)]
        cuts += [(rect(*ff(xm, 0), 0.2, r * 1.0, a), hz(r * 0.5, xm) - 0.12)]
        ps += ext(rect(*ff(xe + hl * 0.17, 0), hl * 0.3, 0.36, a), hz(0, xe) + 0.12)
    cuts += [(barl(ff(-L / 2 + 0.3, v), ff(-L / 2 + 1.0, v + 0.05), 0.16), hz(abs(v), -L / 2 + 0.8) - 0.1) for v in (-0.2, 0.15)]
    hb = ff(L / 2 - 0.5, 0); ps += ext(tri(ff(L / 2 - 0.4, 0), ff(L / 2 + 0.6, u(-0.15, 0.15)), 0.6), r * 1.0 + lf)
    cuts += [(disc(*ff(L / 2 - 0.55, sv * r * 0.42), 0.16, 8), hz(r * 0.42, L / 2 - 0.55) - 0.14) for sv in (-1, 1)]
    o = Obj("log"); o.st(ps, cuts=cuts, foot=rect(cx, cy, L, 2 * r, a))
    xc = x0 + rng.randint(0, nh - 1) * hl + 0.05
    for k in range(rng.randint(2, 3)):
        sv = rng.choice((-1, 1)); p = ff(xc + k * 0.32, sv * r * 0.95); e = ff(xc + u(-0.6, 0.8), sv * (r + u(1.0, 1.6)))
        pts = [p, e]; o.st(rod(pts, 0.25, 1.0), "rope", foot=LineString(pts).buffer(0.3))
        t = math.atan2(e[1] - p[1], e[0] - p[0]); o.st(rod([e, pol(e[0], e[1], 0.7, t)], 0.25, 1.0) + [s for q in (e, pol(e[0], e[1], 0.7, t)) for s in dome(q[0], q[1], 0.25, 0.4, n=8)], "bone", foot=disc(e[0], e[1], 0.9))
    return o.out()

@d("swamp", "growth", "moss", "mossy_log")
def _(cx, cy, a, rng):
    """a rotting log lying in the wet: bark furrowed along it and fallen away in patches, a branch stub, the broken
    end jagged with splinters and gone hollow, cushions of moss over its back, shelf fungus on its flank and ferns
    sprouting at its foot"""
    u = rng.uniform; ff = frame(cx, cy, a); L = u(5.0, 5.8); r = u(0.8, 0.95); hk = 1.05; lf = r * 0.75; o = Obj("moss")
    rf = lambda t: r * (1 - 0.12 * (t + L / 2) / L)
    cuts = [c for v in (-0.55 * r, -0.2 * r, 0.25 * r, 0.6 * r) for c in xs_groove(ff, -L / 2 + u(0.2, 0.8), L / 2 - u(0.3, 1.2), v + u(-0.05, 0.05), rf, hk, 8, lift=lf)]
    cuts += [(disc(*ff(L / 2 - 0.55, u(-0.1, 0.1)), r * 0.5, 12), hk * r * 0.5 + lf)]
    ps = spin(cx, cy, a, L, rf, hk, lift=lf); sv = rng.choice((-1, 1)); bx = u(-L * 0.25, L * 0.15)
    ps += rod([ff(bx, sv * r * 0.5), ff(bx + u(0.5, 0.9), sv * (r + u(0.8, 1.1)))], 0.34, 1.1, lift=lf * 0.6)
    ps += ext(U([tri(ff(L / 2 - 0.1, v), ff(L / 2 + u(0.5, 0.9), v + u(-0.1, 0.1)), 0.34) for v in np.linspace(-r * 0.6, r * 0.6, 4)]), r * 0.9 + lf)
    for i in range(rng.randint(3, 5)):          # moss and fungus in the same surface as the log: built apart, overlapping pieces can tear the mesh
        c = ff(u(-L / 2 + 0.6, L / 2 - 1.0), u(-r * 0.4, r * 0.4)); q = u(0.45, 0.7); z = lf + hk * 0.93 * math.sqrt(max(0.0, rf(0) ** 2 - 0.1))
        ps += dome(c[0], c[1], q, z + q * 0.45, sq=0.85, a=u(0, 3), n=12) + [s for k in range(3) for s in dome(*pol(c[0], c[1], q * 0.55, k * 2.1 + u(0, 1)), q * 0.4, z + q * 0.62, n=8)]
    for i in range(rng.randint(1, 3)):
        sv = rng.choice((-1, 1)); c = ff(u(-L / 2 + 0.6, L / 2 - 0.6), sv * (r - 0.05)); t = a + sv * 1.5708
        ps += ext(disc(c[0], c[1], u(0.5, 0.65), 12).intersection(rect(*pol(c[0], c[1], 0.5, t), 1.0, 1.4, t)), lf + r * u(0.5, 0.8))
    o.st(ps, cuts=cuts, foot=rect(*ff(0.4, 0), L + 0.8, 2 * r, a))
    for i in range(rng.randint(1, 3)):
        c = ff(u(-L / 2, -L / 4), rng.choice((-1, 1)) * (r + u(1.0, 1.3))); o.st(ext(star(c[0], c[1], u(0.6, 0.8), 0.28, rng.randint(5, 7), u(0, 1)), 0.3), "grass", foot=disc(c[0], c[1], 0.6))
    return o.out()

@d("swamp", "made", "reed", "reed_bundle")
def _(cx, cy, a, rng):
    """a bundle of cut reeds for thatching: the stems lying side by side, butts cut square at one end and the
    plumed heads fanning loose out of the other, bound in two places with twisted withies; on some a second smaller
    bundle lies on the first"""
    u = rng.uniform; o = Obj("reed")
    def bundle(x, y, t, Lb, r, lift):
        f2 = frame(x, y, t); hk = 1.0; rf = lambda q: r; lift = lift + r * 0.7
        cuts = [c for v in np.linspace(-r * 0.7, r * 0.7, 5) for c in xs_groove(f2, -Lb / 2 + 0.05, Lb / 2 - 0.05, v, rf, hk, 4, 0.15, 0.1)]
        cuts = [(g, z + lift) for g, z in cuts]
        bd = [rect(*f2(xb, 0), 0.4, 2 * r + 0.3, t) for xb in (-Lb * 0.22, Lb * 0.25)]
        o.st(lie(x, y, t, Lb, r * 0.87, lift=lift), cuts=cuts, foot=rect(x, y, Lb, 2 * r, t))
        o.st([s for xb in (-Lb * 0.22, Lb * 0.25) for s in lie(*f2(xb, 0), t, 0.4, r * 0.87 + 0.1, lift=lift)], "twig", cuts=[(barl(f2(xb - 0.25, -r), f2(xb + 0.25, r), 0.15), lift + r * 0.9) for xb in (-Lb * 0.22, Lb * 0.25)], foot=U(bd))
        tips = []
        for k in range(rng.randint(5, 7)):
            v = (k - 3) * r * 0.3; e = f2(Lb / 2 + u(0.8, 1.4), v * u(1.6, 2.4)); p = f2(Lb / 2 - 0.2, v * 0.8); tt = math.atan2(e[1] - p[1], e[0] - p[0])
            tips += rod([p, e], 0.25, 1.0, lift=lift * 0.8) + spin(*pol(e[0], e[1], 0.35, tt), tt, 0.9, lambda q: 0.25 * math.sqrt(max(0.1, 1 - (2 * q / 0.9) ** 2)), 1.2, lift=lift * 0.8)
        o.st(tips, foot=rect(*f2(Lb / 2 + 0.9, 0), 2.0, 2 * r + 1.4, t))
    ff = frame(cx, cy, a); Lb = u(4.0, 4.6); r = u(0.55, 0.65); bundle(cx, cy, a, Lb, r, 0.0)
    if rng.random() < 0.4: c = ff(u(-0.6, 0.0), u(-0.1, 0.1)); bundle(c[0], c[1], a + u(-0.12, 0.12), Lb * 0.75, r * 0.7, r * 1.25)
    return o.out()

@d("swamp", "made", "twig", "fish_trap")
def _(cx, cy, a, rng):
    """a wicker fish trap lying on its side: a long cone of withies, the hoops standing proud along it, the
    lengthwise rods between them, the wide mouth with the inner funnel showing, the narrow end tied off and its cord
    run to a stone; a fish or two left lying by it"""
    u = rng.uniform; ff = frame(cx, cy, a); L = u(3.0, 3.5); r = u(0.8, 0.95); hk = 1.1; lf = r * 0.6; o = Obj("twig")
    def rf(t):
        s = (t + L / 2) / L; base = r * (1 - 0.58 * (1 - s)) if s > 0.0 else r * 0.42
        return base
    cuts = [(barl(ff(-L / 2 + 0.25, v * r), ff(L / 2 - 0.5, v * r * 1.5), 0.15), lf + hk * 0.93 * r * 0.75) for v in (-0.3, 0.3)]          # the rods show as grooves along the back
    cuts += [(ell(*ff(L / 2 - 0.25, 0), 0.28, r * 0.55, a, 14), hk * r * 0.55 + lf), (disc(*ff(L / 2 - 0.3, 0), 0.16, 8), lf * 0.5)]
    o.st(spin(cx, cy, a, L, rf, hk, lift=lf), cuts=cuts, foot=rect(cx, cy, L, 2 * r, a))
    o.st([q for k in range(1, 6) for q in lie(*ff(-L / 2 + L * k / 6, 0), a, 0.3, rf(-L / 2 + L * k / 6) * 1.02 + 0.06, lift=lf * 0.95)], cuts=[(ell(*ff(L / 2 - 0.25, 0), 0.28, r * 0.55, a, 14), hk * r * 0.55 + lf)], foot=rect(cx, cy, L, 2 * r, a))
    o.st(lie(*ff(-L / 2 - 0.1, 0), a, 0.4, r * 0.48, lift=lf * 0.4), "rope", foot=disc(*ff(-L / 2 - 0.1, 0), 0.45))
    sv = rng.choice((-1, 1)); e = ff(-L / 2 + u(0.0, 0.5), sv * (r + u(0.5, 0.8))); pts = [ff(-L / 2 - 0.3, 0), ff(-L / 2 - 0.5, sv * 0.4), e]
    o.st(rod(pts, 0.25, 1.0), "rope", foot=LineString(pts).buffer(0.3)); o.st(rock(e[0], e[1], u(0.32, 0.4), u(0.3, 0.4), rng), "rock", foot=disc(e[0], e[1], 0.4))
    for i in range(rng.randint(1, 2)):
        c = ff(u(-0.6, 0.9), -sv * (r + u(0.55, 0.7) + i * 0.7)); t = a + u(-0.4, 0.4) + math.pi * rng.randint(0, 1); f2 = frame(c[0], c[1], t); Lf = u(1.5, 1.9)
        o.st(spin(c[0], c[1], t, Lf, lambda q: 0.28 * math.sqrt(max(0.15, 1 - (2 * q / Lf) ** 2)) + 0.02, 1.2) + ext(Polygon([f2(-Lf / 2 + 0.1, 0), f2(-Lf / 2 - 0.45, -0.32), f2(-Lf / 2 - 0.45, 0.32)]), 0.16),
             "hide", cuts=[(disc(*f2(Lf * 0.3, 0.06), 0.09, 6), 0.22), (barl(f2(Lf * 0.18, -0.3), f2(Lf * 0.2, 0.3), 0.15), 0.26)], foot=rect(c[0], c[1], Lf + 0.5, 0.6, t))
    return o.out()

@d("swamp", "made", "hide", "shed_skin")
def _(cx, cy, a, rng):
    """a snake's cast skin lying where it was crawled out of: flattened and papery, wrinkled across every little way,
    crumpled over on itself where it bunched, the tail drawn to a point, the head end turned inside out and split
    open with the eye caps still on it"""
    u = rng.uniform; ff = frame(cx, cy, a); L = u(6.0, 9.0); o = Obj("hide"); ph = u(0, 6.28); A = u(0.7, 1.0)
    pts = [ff(x, A * math.sin(x * 1.3 + ph)) for x in np.linspace(-L / 2, L / 2, 30)]; ln = LineString(pts)
    segs = []; m = len(pts) - 1
    for i in range(m):
        q = (i + 0.5) / m; w = 0.35 + 0.5 * math.sin(max(0.06, q) * math.pi * 0.92) ** 0.6
        segs.append(capl(pts[i], pts[i + 1], w))
    body = U(segs); b = o.up(body, u(0.24, 0.3))
    wr = []
    for s in np.arange(0.35, ln.length - 0.3, 0.32):
        p = ln.interpolate(s); q = ln.interpolate(min(ln.length, s + 0.05)); t = math.atan2(q.y - p.y, q.x - p.x) + 1.5708 + u(-0.25, 0.25)
        wr.append(barl(pol(p.x, p.y, 0.6, t), pol(p.x, p.y, 0.6, t + math.pi), 0.12))
    o.cut(U(wr).intersection(body), b, 0.09)
    for k in range(rng.randint(1, 2)):          # bunched over on itself
        s = u(0.25, 0.7) * ln.length; p = ln.interpolate(s); q = ln.interpolate(s + u(0.5, 0.8))
        xon(o, capl((p.x, p.y), (q.x, q.y), 0.7).intersection(body.buffer(0.1)), b, 0.16)
    e = pts[-1]; t = math.atan2(pts[-1][1] - pts[-2][1], pts[-1][0] - pts[-2][0])
    o.cut(tri(pol(e[0], e[1], 0.3, t), pol(e[0], e[1], -0.6, t), 0.3), b, 0.2)
    xon(o, U([disc(*pol(*pol(e[0], e[1], -0.45, t), 0.22, t + sv * 1.5708), 0.15, 8) for sv in (-1, 1)]), b, 0.08)
    return o.out([ln.buffer(0.8)])


_OWN = set(globals())
