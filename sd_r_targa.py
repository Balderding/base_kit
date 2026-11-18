"""Second drawing, targa: ice and snow over the ruins of somebody else's machines (see sd_r.py for the rule). The made
things are built in flat layers so panel lines, bolts and straps stay crisp; ice and snow are rounded. Nothing hangs:
icicles have fallen, a sled's runners and a pod's lid lie flat."""
import math
from sd_reg import d

def tg_bar(p, q, rf, k=0.4, z0=None, z1=None):
    """a round bar from p to q with flat ends, solid underneath: rf is its radius (a number, or a function of 0..1
    along it: a collar, a band, a taper). Lying, its axis is k radii off the ground."""
    L = math.dist(p, q) or 1.0; m = max(8, int(L / 0.11)); ux, uy = (q[0] - p[0]) / L, (q[1] - p[1]) / L; pt = lambda s, w: (p[0] + ux * s * L - uy * w, p[1] + uy * s * L + ux * w)
    S = [i / m for i in range(m + 1)]; R = [max(0.0, rf(s) if callable(rf) else rf) for s in S]; C = [(z0 + (z1 - z0) * s) if z0 is not None else k * r for s, r in zip(S, R)]
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

def tg_drift(o, x, y, r, h, rng):
    """snow blown up against a thing: a low rounded bank"""
    o.st(dome(x, y, r, h, sq=rng.uniform(0.55, 0.8), a=rng.uniform(0, 3.14)), "snow", foot=disc(x, y, r * 0.8))

# ---------------------------------------------------------------------------------------------------- redrawn
@d("targa", "debris", "ice", "fallen_icicles")
def _(cx, cy, a, rng):
    """icicles come down and lying where they broke: each a long cone of ice, thick and snapped flat at the root,
    swelling here and there, drawn to a point; chips of ice among them"""
    u = rng.uniform; o = Obj("ice"); foot = []
    for i in range(rng.randint(3, 5)):
        x, y = pol(cx, cy, 0 if i == 0 else u(1.2, 2.6), a + i * 1.45 + u(-0.3, 0.3)); b = a + u(-0.9, 0.9) + (math.pi if rng.random() < 0.3 else 0); L = u(3.0, 5.0); r = u(0.55, 0.8); ph = u(0, 6); f2 = frame(x, y, b)
        o.st([s for f, hh in ((1.0, 0.4), (0.62, 0.8), (0.3, 1.15)) for s in ext(Polygon([f2(-L / 2, -r * f), f2(-L / 2 + L * (0.35 + 0.65 * f), 0), f2(-L / 2, r * f)]), r * hh)], foot=rect(x, y, L, 2 * r, b)); foot.append(rect(x, y, L, 2 * r, b))
    for i in range(rng.randint(2, 5)): p = pol(cx, cy, u(1.0, 3.2), u(0, 6.28)); o.st(ext(stone(p[0], p[1], u(0.4, 0.6), rng, 0.6, 4), u(0.28, 0.4)), foot=disc(p[0], p[1], 0.5)); foot.append(disc(p[0], p[1], 0.5))
    return hull(foot), o.L

@d("targa", "made", "cloth", "pack_and_snowshoes")
def _(cx, cy, a, rng):
    """a pack set down in the snow: the sack with its top flap and two buckled straps, a side pocket, the bedroll
    lashed across its head; and a pair of snowshoes (bent frame, cross bars, lacing, the tail) laid by it"""
    u = rng.uniform; ff = frame(cx, cy, a); L = u(3.0, 3.6); W = u(2.1, 2.5); o = Obj("cloth"); g = rrect(cx, cy, L, W, a, 0.45)
    b = o.up(g, u(0.9, 1.2)); fl = o.on(rrect(*ff(L * 0.2, 0), L * 0.55, W - 0.25, a, 0.4), b, 0.18)
    o.on(U([rect(*ff(L * 0.05, sv * W * 0.24), L * 0.8, 0.5, a) for sv in (-1, 1)]), b, 0.3, "leather"); o.studs([ff(-L * 0.12, sv * W * 0.24) for sv in (-1, 1)], b, 0.3, 0.42, "metal")
    o.up(rrect(*ff(-L * 0.1, rng.choice((-1, 1)) * (W / 2 + 0.3)), 1.3, 1.0, a, 0.25), u(0.6, 0.8))
    o.st(tg_bar(ff(L / 2 + 0.6, -W * 0.55), ff(L / 2 + 0.6, W * 0.55), lambda s: 0.58 + 0.07 * (abs(s - 0.25) < 0.06 or abs(s - 0.75) < 0.06), 0.55), "sack", foot=rect(*ff(L / 2 + 0.6, 0), 1.2, W * 1.1, a))
    for i in range(rng.randint(1, 2)):
        c = ff(u(-0.8, 0.8) - L * 0.1, (1 if i == 0 else -1) * (W / 2 + u(1.9, 2.3))) if i == 0 else ff(-L / 2 - u(1.6, 2.0), u(-0.5, 0.5)); t = a + u(-0.4, 0.4) + (1.5708 if i else 0.0); f2 = frame(c[0], c[1], t)
        sh = hull([ell(*f2(0.5, 0), 1.5, 0.95, t, 20), disc(*f2(-1.9, 0), 0.3, 8)]); fr = U([sh.difference(inset(sh, 0.48)), rect(*f2(-2.2, 0), 1.0, 0.5, t)] + [rect(*f2(x, 0), 0.48, 2.2, t).intersection(sh) for x in (-0.1, 1.1)])
        lace = U([barl(f2(-0.1, sv * 0.75), f2(1.1, -sv * 0.75), 0.3) for sv in (-1, 1)]).intersection(sh)
        o.st(ext(U([fr, lace]), 0.4), "wood", foot=sh)
    return o.out()

@d("targa", "made", "alien", "pylon_stub")
def _(cx, cy, a, rng):
    """the stump of one of their pylons: a six-sided foot plate with a bolt at every corner, three buttress fins, the
    column sheared off above them with its panel seams, the socket in its top and the core standing in the socket"""
    u = rng.uniform; r = u(1.25, 1.6); h = u(1.3, 2.2); o = Obj("alien")
    b = o.up(ngon(cx, cy, r * 1.75, 6, a), 0.4); o.studs([pol(cx, cy, r * 1.75 - 0.5, a + k * 1.0472) for k in range(6)], b, 0.27, 0.16)
    for k in range(3):
        t = a + 0.5236 + k * 2.0944; o.on(rect(*pol(cx, cy, r * 1.15, t), r * 0.9, 0.6, t), b, h * u(0.3, 0.5))
    col = o.on(ngon(cx, cy, r, 6, a), b, h); o.cut(U([barl(pol(cx, cy, r * 0.55, a + k * 1.0472), pol(cx, cy, r * 1.1, a + k * 1.0472), 0.22) for k in range(6)]), col, 0.18)
    o.cut(ngon(cx, cy, r * 0.5, 6, a).difference(disc(cx, cy, 0.3, 8)), col, u(0.3, 0.5))
    if rng.random() < 0.5: t = u(0, 6.28); o.cut(Polygon([pol(cx, cy, r * 0.3, t), pol(cx, cy, r * 1.3, t - 0.5), pol(cx, cy, r * 1.3, t + 0.5)]).difference(disc(cx, cy, r * 0.55)), col, u(0.3, 0.6))
    if rng.random() < 0.6: t = u(0, 6.28); c = pol(cx, cy, r * 1.9, t); tg_drift(o, c[0], c[1], u(1.2, 1.7), u(0.5, 0.8), rng)
    return o.out()

@d("targa", "made", "ice", "ice_blocks")
def _(cx, cy, a, rng):
    """ice sawn into building blocks: a row of them with the saw marks scored along their tops, a second course
    begun on the first, and the long-toothed ice saw with its handle lying by them"""
    u = rng.uniform; ff = frame(cx, cy, a); n = rng.randint(2, 4); o = Obj("ice"); bl, bw = u(1.7, 2.0), u(1.3, 1.5); tops = []
    for i in range(n):
        c = ff((i - (n - 1) / 2) * (bl + 0.12) + u(-0.05, 0.05), u(-0.12, 0.12)); t = a + u(-0.06, 0.06); f2 = frame(c[0], c[1], t); g = rrect(c[0], c[1], bl, bw, t, 0.15); b = o.up(g, u(0.85, 1.0))
        o.cut(U([barl(f2(-bl, v), f2(bl, v), 0.2) for v in (-bw * 0.2, bw * 0.22)]), b, 0.15); tops.append((b, c, t))
    for (b, c, t) in tops:
        if rng.random() < 0.45: g = rrect(c[0], c[1], bl - 0.4, bw - 0.4, t + u(-0.15, 0.15), 0.15); k = o.on(g, b, u(0.7, 0.9)); f2 = frame(c[0], c[1], t); o.cut(barl(f2(-bl, 0), f2(bl, 0), 0.2), k, 0.15)
    c = ff(u(-1, 1), -bw / 2 - u(1.2, 1.7)); t = a + u(-0.3, 0.3); f2 = frame(c[0], c[1], t); Ls = u(3.6, 4.4)
    blade = Polygon([f2(-Ls / 2, 0.3)] + [f2(-Ls / 2 + Ls * k / 14, -0.3 if k % 2 else -0.02) for k in range(15)] + [f2(Ls / 2, 0.3)])
    o.st(ext(U([blade.buffer(0.12, 2), ann(*f2(Ls / 2 + 0.55, 0.1), 0.7, 0.25, 10)]), 0.3), "metal", foot=rect(c[0], c[1], Ls + 2.0, 1.4, t))
    return o.out()

@d("targa", "made", "alien", "pod_lid")
def _(cx, cy, a, rng):
    """the lid of a sleeper pod thrown off and lying face up: the flanged rim, the raised shell, the long window in
    it with its bar across, two hinge knuckles on one side and the catch on the other, a row of tell-tales"""
    u = rng.uniform; ff = frame(cx, cy, a); L = u(5.0, 6.2); W = u(2.6, 3.2); o = Obj("alien")
    cap = lambda l, w, dx=0.0: hull([disc(*ff(dx - (l - w) / 2, 0), w / 2, 16), disc(*ff(dx + (l - w) / 2 + w * 0.12, 0), w * 0.38, 16)])
    g = cap(L, W); sv = rng.choice((-1, 1))
    o.up(U([rect(*ff(x, sv * (W / 2 - 0.05)), 0.9, 0.9, a) for x in (-L * 0.22, L * 0.14)]), 0.55)
    b = o.up(g, 0.42); sh = o.on(inset(g, 0.42), b, u(0.4, 0.55)); wn = cap(L * 0.5, W * 0.42, L * 0.1)
    o.cut(wn.difference(rect(*ff(L * 0.1, 0), 0.5, W, a)), sh, 0.28); o.groove(wn.buffer(0.3), sh, 0.2, 0.15)
    o.studs([ff(-L * 0.36, v) for v in (-0.5, 0.0, 0.5)], sh, 0.25, 0.15); o.on(rect(*ff(-L * 0.05, -sv * (W / 2 - 0.3)), 1.0, 0.5, a), b, 0.3)
    if rng.random() < 0.4: o.cut(wavy(ff(u(-L * 0.3, 0), -W), ff(u(-L * 0.3, 0), W * 0.2), rng, 0.3, 4, 0.24), sh, 0.3)
    if rng.random() < 0.5: c = ff(u(-L * 0.4, L * 0.4), -sv * (W / 2 + 0.6)); tg_drift(o, c[0], c[1], u(1.2, 1.7), u(0.45, 0.7), rng)
    return o.out()

@d("targa", "made", "alien", "conduit_node", ground=True)
def _(cx, cy, a, rng):
    """a junction in the floor conduits: the six-sided cover with its ring of bolts and the domed lens in the
    middle, the channels running off from it melted clear of snow, a small cap where each one goes under"""
    u = rng.uniform; R = u(1.5, 1.9); n = rng.randint(2, 4); gs = []; o = Obj("alien"); ends = []
    for k in range(n):
        t = a + 6.2832 * k / n + u(-0.2, 0.2); m = pol(cx, cy, R + 1.3, t); e = pol(m[0], m[1], u(1.2, 2.4), t + rng.choice((-0.5236, 0, 0.5236))); gs.append(line([pol(cx, cy, R + 0.35, t), m, e], 0.6)); ends.append(e)
    o.gcut(U(gs), 0.35, "slot")
    b = o.up(ngon(cx, cy, R, 6, a), 0.4); o.on(ann(cx, cy, R * 0.72, R * 0.72 - 0.5, 24), b, 0.2); o.studs([pol(cx, cy, R - 0.42, a + k * 1.0472) for k in range(6)], b, 0.25, 0.15)
    o.st(dome(cx, cy, R * 0.36, R * 0.4, n=12), par=b)
    for e in ends: t = math.atan2(e[1] - cy, e[0] - cx); c = pol(e[0], e[1], 0.8, t); k = o.up(disc(c[0], c[1], 0.62, 12), 0.36); o.cut(disc(c[0], c[1], 0.25, 8), k, 0.18)
    return o.out()

@d("targa", "debris", "ice", "pressure_ridge")
def _(cx, cy, a, rng):
    """a pressure ridge: the floe broken along a line and its slabs driven up on edge, each a flat plate tilted
    one way or the other, leaning on its neighbour, with the broken bits at their feet"""
    u = rng.uniform; ff = frame(cx, cy, a); n = rng.randint(4, 6); lay = []; foot = []
    for i in range(n):
        c = ff((i - (n - 1) / 2) * 1.5 + u(-0.2, 0.2), u(-0.4, 0.4)); t = a + 1.5708 + u(-0.5, 0.5) + (math.pi if i % 2 else 0.0); f2 = frame(c[0], c[1], t); L = u(2.2, 3.2); W = u(1.5, 2.1); h = u(1.0, 1.9); ps = []
        loc = [(-L / 2, -W / 2), (L / 2 + u(-0.4, 0.2), -W / 2 + u(-0.2, 0.3)), (L / 2 + u(-0.3, 0.3), W / 2), (-L / 2 + u(-0.2, 0.4), W / 2 + u(-0.3, 0.2))]
        for k in range(5): ps += ext(Polygon([f2(x * (1 - 0.03 * k), W / 2 - (W / 2 - v) * (1 - 0.17 * k)) for x, v in loc]), h * (k + 1) / 5)
        lay.append(ST(ps, "ice")); foot.append(Polygon([f2(x, v) for x, v in loc]))
    for i in range(rng.randint(2, 4)): p = ff(u(-n * 0.7, n * 0.7), rng.choice((-1, 1)) * u(1.5, 2.1)); lay.append(ST(ext(stone(p[0], p[1], u(0.45, 0.7), rng, 0.7, 5), u(0.3, 0.5)), "ice")); foot.append(disc(p[0], p[1], 0.6))
    return hull(foot), lay

@d("targa", "made", "alien", "glyph_tile")
def _(cx, cy, a, rng):
    """one of their floor tiles: six-sided, a border line, and the raised sign in the middle (a ring with three to
    six arms, a stud at the end of each); a bolt in every corner, one corner under snow, cracked on some"""
    u = rng.uniform; R = u(2.2, 2.8); k = rng.randint(3, 6); o = Obj("alien"); g = ngon(cx, cy, R, 6, a); t0 = a + u(0, 1.0)
    b = o.up(g, 0.45); o.groove(ngon(cx, cy, R * 0.82, 6, a), b, 0.2, 0.16)
    sg = U([ann(cx, cy, R * 0.3, R * 0.3 - 0.46, 20)] + [barl(pol(cx, cy, R * 0.28, t0 + 6.2832 * j / k), pol(cx, cy, R * 0.6, t0 + 6.2832 * j / k), 0.46) for j in range(k)] + [disc(*pol(cx, cy, R * 0.6, t0 + 6.2832 * j / k), 0.34, 8) for j in range(k)])
    o.on(sg, b, 0.18); o.studs([pol(cx, cy, R * 0.9, a + j * 1.0472) for j in range(6)], b, 0.25, 0.13)
    if rng.random() < 0.35: o.cut(wavy(pol(cx, cy, R * 1.1, a + 1), pol(cx, cy, R * 1.1, a + 4), rng, 0.4, 4, 0.26), b, 0.25)
    if rng.random() < 0.6: c = pol(cx, cy, R * 0.95, a + u(0, 6.28)); tg_drift(o, c[0], c[1], u(1.1, 1.5), u(0.5, 0.75), rng)
    return o.out()

@d("targa", "made", "ice", "fishing_hole", ground=True)
def _(cx, cy, a, rng):
    """a fishing hole cut through the ice: the round hole with the chipped ice banked round it, the plug that was
    sawn out lying to one side, a short rod on its forked rest with the line to the hole, the catch laid out"""
    u = rng.uniform; R = u(1.3, 1.6); o = Obj("ice"); o.gcut(disc(cx, cy, R, 16), 0.6, "water")
    for p in ringpts(cx, cy, R + 0.75, rng.randint(5, 8), a + 0.9, 0.25, rng):
        if abs(((math.atan2(p[1] - cy, p[0] - cx) - a + 3.1416) % 6.2832) - 3.1416) > 0.5: o.st(ext(stone(p[0], p[1], u(0.5, 0.7), rng, 0.8, 5), u(0.35, 0.6)), foot=disc(p[0], p[1], 0.6))
    c = pol(cx, cy, R + u(2.3, 2.8), a + u(1.8, 2.6)); pl = o.up(disc(c[0], c[1], R * 0.92, 16), u(0.5, 0.7)); o.cut(barl(pol(c[0], c[1], R, a), pol(c[0], c[1], R, a + math.pi), 0.2), pl, 0.15)
    e = pol(cx, cy, R + u(3.2, 4.0), a); m = pol(cx, cy, R + 1.5, a); o.st(ext(U([barl(pol(cx, cy, R + 0.4, a), e, 0.5), rect(m[0], m[1], 0.6, 1.3, a), disc(e[0], e[1], 0.42, 8)]), 0.45), "wood", foot=LineString([m, e]).buffer(0.6))
    for i in range(rng.randint(1, 3)):
        c = pol(cx, cy, R + u(2.0, 2.6), a - 1.3 - i * 0.75); t = u(0, 6.28); f2 = frame(c[0], c[1], t); l = u(0.85, 1.1)
        o.st(ext(U([ell(c[0], c[1], l, l * 0.42, t, 14), Polygon([f2(-l + 0.15, 0), f2(-l - 0.6, 0.5), f2(-l - 0.6, -0.5)])]), 0.34), "bone", cuts=[(disc(*f2(l * 0.55, 0.05), 0.13, 5), 0.2)], foot=disc(c[0], c[1], l))
    return o.out()

@d("targa", "made", "alien", "thermal_grille", ground=True)
def _(cx, cy, a, rng):
    """a warm-air grille of theirs still working: an oval plate bolted down, its slots cut across between the frame
    and a middle bar, and the snow melted back from it all round to a wet edge"""
    u = rng.uniform; ff = frame(cx, cy, a); L = u(3.8, 4.8); W = u(2.3, 2.9); o = Obj("alien"); g = ell(cx, cy, L / 2, W / 2, a, 28)
    o.gcut(g.buffer(u(0.9, 1.3)).difference(g.buffer(0.3)), 0.22, "water")
    b = o.up(g, 0.42); inner = ell(cx, cy, L / 2 - 0.55, W / 2 - 0.5, a, 24).difference(rect(cx, cy, L, 0.46, a))
    o.cut(U([barl(ff(x, -W), ff(x, W), 0.3) for x in np.arange(-L / 2 + 0.8, L / 2 - 0.5, 0.62)]).intersection(inner), b, 0.26)
    o.studs([ff(sx * (L / 2 - 0.32), 0) for sx in (-1, 1)], b, 0.25, 0.14)
    return o.out()

@d("targa", "made", "wood", "sled")
def _(cx, cy, a, rng):
    """a hauling sled: two long runners turned up at the nose and joined by the nose bar, the slatted deck between
    them, a load of cases under a lashed cover on most, and the tow rope thrown forward"""
    u = rng.uniform; ff = frame(cx, cy, a); L = u(6.0, 7.5); W = u(2.4, 2.9); o = Obj("wood")
    rn = [line([ff(-L / 2, sv * W / 2), ff(L / 2 - 1.2, sv * W / 2), ff(L / 2 - 0.3, sv * (W / 2 - 0.45)), ff(L / 2, sv * (W / 2 - 0.9))], 0.6) for sv in (-1, 1)]
    o.st(ext(U(rn + [rect(*ff(L / 2 - 0.05, 0), 0.5, W - 1.4, a)]), 0.5), foot=rect(cx, cy, L, W + 0.6, a))
    dk = rect(*ff(-0.6, 0), L - 2.2, W - 0.7, a); b = o.up(dk, 0.7); o.seams(lambda x, v: ff(x - 0.6, v), L - 2.2, W - 0.7, b, 0.62, w=0.2, d=0.16)
    if rng.random() < 0.7:
        c = ff(-0.6 + u(-0.4, 0.4), 0); Ll = (L - 2.2) * u(0.55, 0.8); ld = o.on(rrect(c[0], c[1], Ll, W - 1.1, a, 0.4), b, u(0.7, 1.1), "cloth")
        o.on(U([rect(*ff(-0.6 + x, 0), 0.46, W - 1.1, a) for x in (-Ll * 0.25, Ll * 0.25)]).intersection(rrect(c[0], c[1], Ll, W - 1.1, a, 0.4)), ld, 0.14, "rope")
    else: o.on(U([rect(*ff(-0.6 + sx * (L - 2.2) * 0.36, 0), 0.5, W - 0.9, a) for sx in (-1, 1)]), b, 0.14)
    e = ff(L / 2 + u(1.5, 2.4), u(-1.2, 1.2)); pts = [ff(L / 2 + 0.3, -0.3)] + wiggle(ff(L / 2 + 0.6, 0), e, rng, 0.5, 3) + [ff(L / 2 + 0.9, 0.5), ff(L / 2 + 0.3, 0.3)]
    o.st(rod(pts, 0.27, 1.0), "rope", foot=LineString(pts).buffer(0.4))
    return o.out()

@d("targa", "made", "alien", "buried_steps")
def _(cx, cy, a, rng):
    """the top of a flight of their steps showing through the snow: three or four treads, each a clear step above
    the last with a line along its nosing, a low wall down each side, and the drift lying over the lower end"""
    u = rng.uniform; ff = frame(cx, cy, a); W = u(3.2, 4.0); n = rng.randint(3, 4); td = u(1.0, 1.2); o = Obj("alien"); rise = u(0.36, 0.46)
    for i in range(n):
        x = (i - (n - 1) / 2) * td; b = o.up(rect(*ff(x, 0), td + 0.02, W, a), rise * (i + 1)); o.cut(barl(ff(x - td / 2 + 0.3, -W), ff(x - td / 2 + 0.3, W), 0.2), b, 0.15)
    for sv in (-1, 1):
        wl = o.up(rect(*ff(0.15, sv * (W / 2 + 0.4)), n * td + 0.5, 0.9, a), rise * n + 0.3); o.cut(barl(ff(-n * td, sv * (W / 2 + 0.4)), ff(n * td, sv * (W / 2 + 0.4)), 0.22), wl, 0.16)
    c = ff(-n * td / 2 - u(0.2, 0.7), u(-W * 0.3, W * 0.3)); tg_drift(o, c[0], c[1], u(1.7, 2.3), u(0.6, 0.9), rng)
    if rng.random() < 0.5: c = ff(u(-0.5, 1.0), rng.choice((-1, 1)) * (W / 2 + 1.2)); tg_drift(o, c[0], c[1], u(1.2, 1.6), u(0.5, 0.8), rng)
    return o.out()

@d("targa", "debris", "ice", "ice_spikes")
def _(cx, cy, a, rng):
    """ice spikes: sharp five- and six-sided blades of ice standing in a clump, a tall one or two and the lesser
    ones about their feet, all leaning the way the wind carved them"""
    u = rng.uniform; lay = []; foot = []; lean = a + u(-0.3, 0.3)
    for i in range(rng.randint(4, 7)):
        x, y = pol(cx, cy, 0 if i == 0 else u(0.9, 2.1), a + i * 1.1 + u(-0.3, 0.3)); r = u(0.75, 1.05) * (1.0 if i < 2 else 0.75); h = min(3.2, r * u(2.4, 3.2)); k = rng.choice((5, 6)); sq = u(0.9, 1.0); ps = []
        for j in range(10): f = j / 9; q = pol(x, y, r * 0.5 * f, lean); ps += turn(q[0], q[1], [(max(0.3, r * (1 - f) ** 0.8), 0.0), (max(0.3, r * (1 - f) ** 0.8), max(0.12, h * f))], k, sq, lean)
        lay.append(ST(ps, "ice")); foot.append(disc(x, y, r + 0.2))
    return hull(foot), lay

@d("targa", "made", "alien", "power_cell")
def _(cx, cy, a, rng):
    """one of their power cells: a canister with a broad collar at each end, bands round its middle and the contact
    stud standing out of one end, lying in its two-block cradle or stood on end; a dead one may lie split beside it"""
    u = rng.uniform; ff = frame(cx, cy, a); L = u(3.6, 4.4); r = u(0.8, 1.0); o = Obj("alien")
    prof = lambda s: r * (1.16 if s < 0.14 or s > 0.86 else 1.08 if abs(s - 0.5) < 0.05 or abs(s - 0.34) < 0.03 or abs(s - 0.66) < 0.03 else 1.0)
    if rng.random() < 0.65:
        o.up(U([rect(*ff(sx * L * 0.3, 0), 0.8, 2 * r + 1.1, a) for sx in (-1, 1)]), 0.45)
        o.st(tg_bar(ff(-L / 2, 0), ff(L / 2, 0), prof, 0.5), foot=rect(cx, cy, L, 2 * r, a)); o.st(tg_bar(ff(L / 2, 0), ff(L / 2 + 0.5, 0), 0.4, 0.5, z0=0.5 * r, z1=0.5 * r), foot=disc(*ff(L / 2 + 0.3, 0), 0.5))
    else:
        h = L * 0.62; o.st(turn(cx, cy, [(r * 1.16, 0.0), (r * 1.16, h * 0.16), (r, h * 0.18), (r, h * 0.46), (r * 1.08, h * 0.48), (r * 1.08, h * 0.56), (r, h * 0.58), (r, h * 0.84), (r * 1.16, h * 0.86), (r * 1.16, h), (0.4, h + 0.02), (0.4, h + 0.35)], 20), foot=disc(cx, cy, r * 1.2))
    if rng.random() < 0.45:
        c = ff(u(-1, 1), -r - u(1.6, 2.2)); t = a + u(-0.5, 0.5); f2 = frame(c[0], c[1], t); l2 = L * 0.5
        o.st(tg_bar(f2(-l2 / 2, 0), f2(l2 / 2, 0), lambda s: r * (1.16 if s < 0.28 else 1.0), 0.5), foot=rect(c[0], c[1], l2, 2 * r, t)); o.gcut(blob(*f2(l2 / 2 + 0.8, 0), u(0.7, 1.0), rng, 3), 0.22, "water")
    return o.out()

# ---------------------------------------------------------------------------------------------------- new
@d("targa", "made", "wood", "skis")
def _(cx, cy, a, rng):
    """a pair of skis laid down with their sticks: long narrow boards pointed and turned up at the tip, the foot
    plate and toe strap in the middle of each, and two sticks with the ring near the point"""
    u = rng.uniform; o = Obj("wood"); L = u(8.0, 9.5)
    for sv in (-1, 1):
        c = pol(cx, cy, u(0.55, 0.8), a + sv * 1.5708); t = a + u(-0.08, 0.08) + sv * u(0.0, 0.1); f2 = frame(c[0], c[1], t)
        g = Polygon([f2(-L / 2, -0.42), f2(L / 2 - 1.2, -0.46), f2(L / 2, 0), f2(L / 2 - 1.2, 0.46), f2(-L / 2, 0.42)]); b = o.up(g, 0.34)
        o.on(rect(*f2(-0.2, 0), 1.3, 0.62, t), b, 0.14, "metal"); o.on(rect(*f2(0.2, 0), 0.46, 0.92, t), b, 0.3, "leather"); o.on(Polygon([f2(L / 2 - 1.1, -0.3), f2(L / 2 - 0.2, 0), f2(L / 2 - 1.1, 0.3)]), b, 0.25)
    for i in range(rng.randint(1, 2)):
        c = pol(cx, cy, u(1.9, 2.5), a + (1.5708 if i == 0 else -1.5708)); t = a + u(-0.35, 0.35); f2 = frame(c[0], c[1], t); Lp = L * 0.62
        o.st(ext(U([barl(f2(-Lp / 2, 0), f2(Lp / 2, 0), 0.5), ann(*f2(Lp / 2 - 0.8, 0), 0.7, 0.22, 10), disc(*f2(-Lp / 2, 0), 0.36, 8)]), 0.36), foot=rect(c[0], c[1], Lp, 1.4, t))
    return o.out()

@d("targa", "made", "metal", "supply_drop")
def _(cx, cy, a, rng):
    """a supply canister dropped to the survey party and half drifted over: a ribbed case with its lid, two toggle
    catches, carrying handles at the ends and the big marker panel on top; snow banked on its weather side"""
    u = rng.uniform; ff = frame(cx, cy, a); L = u(4.0, 4.8); W = u(2.3, 2.7); h = u(1.1, 1.4); o = Obj("metal"); g = rrect(cx, cy, L, W, a, 0.35)
    for sx in (-1, 1): c = ff(sx * (L / 2 + 0.05), 0); pts = arc(c[0], c[1], 0.7, a - 1.5708 if sx > 0 else a + 1.5708, a + 1.5708 if sx > 0 else a + 4.7124, 8); o.st(ext(line(pts, 0.5), 0.5), foot=disc(c[0], c[1], 0.9))
    b = o.up(g, h); lid = o.lid(g, b, 0.18, 0.16); o.ribs(ff, L - 1.0, W - 0.5, lid, 2, a, 0.5, 0.15)
    pn = rect(cx, cy, L * 0.3, W * 0.5, a); o.cut(pn, lid, 0.16); o.cut(U([barl(ff(-L * 0.1, -W * 0.18), ff(L * 0.1, W * 0.18), 0.22), barl(ff(-L * 0.1, W * 0.18), ff(L * 0.1, -W * 0.18), 0.22)]), lid, 0.3)
    o.tabs(ff, [-L * 0.3, L * 0.3], W / 2 - 0.5, lid, a, 0.5, 0.6, 0.18)
    sv = rng.choice((-1, 1)); c = ff(u(-L * 0.3, L * 0.3), sv * (W / 2 + 0.7)); tg_drift(o, c[0], c[1], u(1.8, 2.4), u(0.7, 1.0), rng)
    if rng.random() < 0.5: c = ff(rng.choice((-1, 1)) * (L / 2 + 0.9), sv * u(0.2, 0.9)); tg_drift(o, c[0], c[1], u(1.1, 1.5), u(0.5, 0.7), rng)
    return o.out()


_OWN = set(globals())
