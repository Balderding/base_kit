"""Third drawing, trederra (see sd_r_x_frontier.py): redrawn at their real size beside a 32 mm figure. A stretcher
stands on its feet with its load on it, sandbags are three courses high, the trench mortar is broken down beside its
stacked bomb boxes, the duckboard runs over a puddle, the fallen sign still says where it pointed."""
import math
from sd_reg import d
from sd_r_x_frontier import x_d
from sd_r_trederra import t_bar
from sd_r_x_frontier import x_word
from sd_r_x_dungeon import x_wall


@x_d("trederra", "made", "cloth", "stretcher")
def _(cx, cy, a, rng):
    """a stretcher set down on its little iron feet: the two poles with the worn handles standing out at both ends,
    the canvas slung between them sagging in the middle, the iron spreaders; on it a casualty's load wrapped and strapped
    in a blanket, or a folded blanket, a tin hat and a haversack left on the empty canvas"""
    u = rng.uniform; ff = frame(cx, cy, a); L = u(8.0, 8.5); W = u(2.2, 2.4); o = Obj("cloth"); zp = 0.62; Lc = L - 2.4
    cv = rect(cx, cy, Lc, W - 0.2, a)
    o.st([((lambda z: cv if z < zp - 0.12 else (cv.intersection(U([rect(*ff(sx * Lc * 0.38, 0), Lc * 0.26, W, a) for sx in (-1, 1)])) if z < zp else None)), zp)], cuts=[(barl(ff(-Lc / 2, 0), ff(Lc / 2, 0), 0.12), zp - 0.24)], foot=cv)
    for sv in (-1, 1): o.st(t_bar(ff(-L / 2, sv * W / 2), ff(L / 2, sv * W / 2), lambda s: 0.22 if s < 0.1 or s > 0.9 else 0.26, z0=zp - 0.02, z1=zp - 0.02), "wood", foot=rect(*ff(0, sv * W / 2), L, 0.6, a))
    for sx in (-1, 1):
        x = sx * (Lc / 2 + 0.25)
        o.st(ext(rect(*ff(x, 0), 0.3, W, a), zp + 0.05) + [s for sv in (-1, 1) for s in ext(rect(*ff(x, sv * W / 2), 0.62, 0.62, a), zp + 0.26)], "iron", foot=rect(*ff(x, 0), 0.6, W + 0.6, a))
    load = rng.random()
    if load < 0.55:          # wrapped in a blanket and strapped
        Lb = u(5.0, 5.4); c = ff(u(-0.2, 0.2), 0); t = a + u(-0.04, 0.04)
        rf = lambda s: (W * 0.36) * (0.62 if s < 0.12 else (0.62 + 0.38 * min(1.0, (s - 0.12) / 0.08)) if s < 0.3 else 1.0 - 0.3 * (s - 0.3) / 0.7)
        f2 = frame(c[0], c[1], t)
        o.st(t_bar(f2(-Lb / 2, 0), f2(Lb / 2, 0), rf, z0=zp + 0.15, z1=zp + 0.1), "sack", cuts=[(U([rect(*f2(x, 0), 0.18, W, t) for x in (-Lb * 0.18, Lb * 0.12, Lb * 0.38)]), zp + 0.75)], foot=rect(c[0], c[1], Lb, W * 0.75, t))
        o.st(t_bar(f2(-Lb / 2 + 0.2, 0), f2(Lb / 2 - 0.3, 0), 0.22, z0=zp + 0.98, z1=zp + 0.75), "sack", foot=rect(c[0], c[1], Lb, 0.5, t))
    else:
        c = ff(u(-2.0, -0.8), 0); t = a + u(-0.15, 0.15); bl = rrect(c[0], c[1], u(1.7, 2.1), W - 0.7, t, 0.25); k = o.up(bl, zp + 0.5, "sack"); f2 = frame(c[0], c[1], t)
        o.cut(U([barl(f2(-2, v), f2(2, v), 0.14) for v in (-0.35, 0.0, 0.35)]), k, 0.12)
        h = ff(u(0.8, 2.0), u(-0.2, 0.2)); R = 0.95
        o.st(turn(h[0], h[1], [(R, zp), (R, zp + 0.12), (R - 0.18, zp + 0.14), (0.66, zp + 0.3)] + crv(0.62, 0.22, zp + 0.3, zp + 0.95, n=6), 20, 0.9, a), "tin", foot=disc(h[0], h[1], R))
    return o.out()

@x_d("trederra", "made", "sack", "sandbags")
def _(cx, cy, a, rng):
    """a sandbag wall begun and left: plump bags laid in courses, each course breaking joint over the one below, every
    bag with its seam along the top and its tied neck standing out at the end; one fallen from the top lying beside"""
    u = rng.uniform; ff = frame(cx, cy, a); L = u(2.15, 2.3); W = u(1.25, 1.35); h = u(0.64, 0.7); n = 3; foot = []; lay = []
    def course(k, m, dx=0.0, b0=a, v0=0.0):
        ps = []; cuts = []; fp = []; lift = k * h * 0.82
        for i in range(m):
            c = ff((i - (m - 1) / 2) * (L - 0.05) + dx + u(-0.08, 0.08), v0 + u(-0.1, 0.1)); b = b0 + u(-0.06, 0.06) + math.pi * rng.randint(0, 1)
            ps += _S()._sack(c[0], c[1], b, L * u(0.94, 1.0), W, h, lift); foot.append(rect(c[0], c[1], L + 0.8, W, b)); fp.append(rect(c[0], c[1], L, W, b))
            f2 = frame(c[0], c[1], b); cuts.append((barl(f2(-L * 0.36, 0), f2(L * 0.28, 0), 0.12), lift + h - 0.1))
        return ps, cuts, U(fp)
    got = [course(0, n, 0.0, a, v0) for v0 in ((-W / 2 + 0.12, W / 2 - 0.12) if rng.random() < 0.3 else (0.0,))]
    got.append(course(1, n - 1, u(-0.1, 0.1)))
    if rng.random() < 0.7: got.append(course(2, n - 2, u(-0.3, 0.3)))
    keep = [c for i, (_, cs, _f) in enumerate(got) for c in cs if not any(c[0].intersects(g[2]) for g in got[i + 1:] if g[2] is not None and not g[2].is_empty)]          # a seam is only cut where no bag lies over it
    lay.append(ST([s for ps, _, _f in got for s in ps], "sack", cuts=keep))          # one surface for the whole stack: bags of separate layers would meet edge to edge
    if rng.random() < 0.6: c = ff(u(-L, L) * 0.4, rng.choice((-1, 1)) * (W * 1.6 + u(0.2, 0.5))); lay.append(ST(_S()._sack(c[0], c[1], a + u(-0.8, 0.8), L, W, h), "sack")); foot.append(disc(c[0], c[1], L / 2))
    return hull(foot), lay

@x_d("trederra", "made", "iron", "mortar")
def _(cx, cy, a, rng):
    """a trench mortar broken down for carrying: the base plate with its ribs and the socket, the barrel laid beside
    it with its breech cap and the muzzle ring, the folded bipod with its elevating screw; the bombs' wooden boxes
    stacked, rope handles at their ends and lids nailed down, and a bomb or two with its fin lying out"""
    u = rng.uniform; ff = frame(cx, cy, a); o = Obj("iron")
    pc = ff(-1.4, u(-0.8, -0.5)); t = a + u(-0.4, 0.4); f2 = frame(pc[0], pc[1], t)
    pg = rrect(pc[0], pc[1], 2.0, 2.0, t, 0.25); bp = o.up(pg, 0.3); o.on(U([rect(*f2(0, v), 1.8, 0.3, t) for v in (-0.5, 0.5)] + [rect(*f2(v, 0), 0.3, 1.8, t) for v in (-0.5, 0.5)]), bp, 0.14)
    so = o.on(disc(pc[0], pc[1], 0.55, 14), bp, 0.4); o.cut(disc(pc[0], pc[1], 0.3, 10), so, 0.3)
    bt = a + u(-0.15, 0.15); p = ff(-2.6, u(1.0, 1.3)); q = pol(p[0], p[1], u(4.0, 4.4), bt)
    o.st(t_bar(p, q, lambda s: 0.36 if s < 0.06 else 0.3 if s < 0.95 else 0.36, 1.0), foot=LineString([p, q]).buffer(0.4))
    p2 = pol(p[0], p[1], u(-0.1, 0.4), bt - 0.8) if False else ff(-0.2, u(-2.0, -1.7)); bt2 = a + u(-0.3, 0.3); f3 = frame(p2[0], p2[1], bt2)
    o.st(ext(U([barl(f3(-1.4, -0.22), f3(1.2, -0.05), 0.24), barl(f3(-1.4, 0.22), f3(1.2, 0.05), 0.24), rect(*f3(1.3, 0), 0.5, 0.6, bt2), disc(*f3(-1.5, -0.22), 0.25, 6), disc(*f3(-1.5, 0.22), 0.25, 6)]), 0.32) + t_bar(f3(1.3, 0), f3(2.2, 0), 0.22, 1.0), foot=rect(*f3(0.4, 0), 3.8, 0.8, bt2))
    nb = rng.randint(1, 3); bc = ff(1.7, u(-0.3, 0.3)); bl, bw, bh = 2.1, 1.3, u(1.05, 1.15); par = None
    for k in range(nb):
        tb = a + 1.5708 + u(-0.08, 0.08); g = rect(bc[0] + u(-0.06, 0.06), bc[1] + u(-0.06, 0.06), bl - 0.06 * k, bw - 0.06 * k, tb); f4 = frame(bc[0], bc[1], tb)
        bx = o.up(g, bh) if par is None else o.on(g, par, bh)
        lid = o.lid(g, bx, 0.1, 0.12, "wood"); o.cut(U([barl(f4(x, -bw), f4(x, bw), 0.1) for x in (-bl * 0.2, bl * 0.2)]), lid, 0.07)
        o.studs([f4(sx * (bl / 2 - 0.25), sy * (bw / 2 - 0.22)) for sx in (-1, 1) for sy in (-1, 1)], lid, 0.2, 0.08, "iron", 8)
        par = bx
    o.st([s for sx in (-1, 1) for s in t_bar(pol(bc[0], bc[1], bl / 2 - 0.2, a + 1.5708 + (0 if sx > 0 else math.pi)), pol(bc[0], bc[1], bl / 2 + 0.25, a + 1.5708 + (0 if sx > 0 else math.pi)), 0.22, z0=bh * 0.6, z1=bh * 0.6)], "rope", foot=disc(bc[0], bc[1], 1.4))
    if par is not None: o.cut(x_word(rng.choice(["STOKES", "3IN", "HE", "AMMO"]), bc[0], bc[1], a + 1.5708, 0.5, 0.12, bl - 0.6), par, 0.08)
    for i in range(rng.randint(1, 2)):
        c = ff(u(0.8, 2.6), rng.choice((-1, 1)) * u(1.8, 2.1)); tt = a + u(-0.6, 0.6); f5 = frame(c[0], c[1], tt)
        o.st(t_bar(f5(-0.6, 0), f5(0.6, 0), lambda s: 0.36 if s > 0.15 else 0.2, 0.9) + ext(U([barl(f5(-0.95, -0.35), f5(-0.95, 0.35), 0.22), barl(f5(-1.0, 0), f5(-0.6, 0), 0.24)]), 0.42), foot=rect(c[0], c[1], 2.2, 0.8, tt))
    return o.out()

@x_d("trederra", "made", "wood", "duckboard")
def _(cx, cy, a, rng):
    """a length of duckboard laid over a puddle: the two runners standing out at the ends, slats nailed across them
    with a gap between each, two nail heads at every crossing, the grain on every slat; one slat gone, one snapped short,
    mud trodden onto it"""
    u = rng.uniform; ff = frame(cx, cy, a); n = rng.randint(6, 7); st = u(1.12, 1.2); L = n * st + 0.9; W = u(2.5, 2.8); o = Obj("wood")
    o.gcut(ell(*ff(u(-1.0, 1.0), u(-0.4, 0.4)), L * u(0.3, 0.4), W * 0.7, a + u(-0.2, 0.2), 20), 0.3, "water")
    rv = W * 0.3; rn = o.up(U([rect(*ff(0, sv * rv), L, 0.6, a) for sv in (-1, 1)]), 0.36)
    o.cut(U([barl(ff(-L, sv * rv + 0.05), ff(L, sv * rv - 0.05), 0.1) for sv in (-1, 1)]), rn, 0.07)
    gone = rng.randint(0, n - 1) if rng.random() < 0.5 else -1
    for i in range(n):
        if i == gone: continue
        x = (i - (n - 1) / 2) * st; t = a + u(-0.05, 0.05); w = W; v = 0.0
        if rng.random() < 0.15: w = W * u(0.55, 0.7); v = rng.choice((-1, 1)) * (W - w) / 2
        c = ff(x + u(-0.04, 0.04), v + u(-0.08, 0.08)); f2 = frame(c[0], c[1], t); g = rect(c[0], c[1], 0.82, w, t); s = o.up(g, u(0.58, 0.64))
        o.cut(U([barl(f2(vx, -w / 2 + 0.15), f2(vx + u(-0.05, 0.05), w / 2 - 0.15), 0.1) for vx in (-0.2, 0.18)]), s, 0.07)
        o.cut(U([disc(*f2(dx, sv * rv - v), 0.15, 8) for dx in (-0.2, 0.2) for sv in (-1, 1) if abs(sv * rv - v) < w / 2 - 0.2]), s, 0.06)
        if rng.random() < 0.25: o.st(dome(*f2(u(-0.1, 0.1), u(-w * 0.3, w * 0.3)), 0.5, 0.3, sq=0.7, a=t), "earth", par=s, foot=disc(c[0], c[1], 0.4))
    return o.out()

@x_d("trederra", "made", "wood", "trench_sign")
def _(cx, cy, a, rng):
    """a trench signboard fallen with its stake: the stake splintered where it broke off, the board nailed across its
    head with the trench's name cut into it (one end pointed on the kind that shows the way), the plank joint, nail
    heads, a bullet hole or two, the wire it was tied up with"""
    u = rng.uniform; ff = frame(cx, cy, a); o = Obj("wood"); Ls = u(5.8, 6.3); L = u(3.8, 4.3); W = u(1.4, 1.6)
    sp = U([rect(*ff(-0.4, 0), Ls - 0.8, 0.62, a)] + [tri(ff(Ls / 2 - 0.85, v), ff(Ls / 2 - 0.85 + u(0.3, 0.7), v * 0.6), 0.22) for v in (-0.2, 0.0, 0.2)])
    sk = o.up(sp, 0.4); o.cut(barl(ff(-Ls / 2, 0.08), ff(Ls / 2 - 1, -0.05), 0.1), sk, 0.07)
    c = ff(-Ls / 2 + 1.1, 0); t = a + 1.5708 + u(-0.25, 0.25); f2 = frame(c[0], c[1], t); sv = rng.choice((-1, 1)); arrow = rng.random() < 0.55
    g = Polygon([f2(-L / 2, -W / 2), f2(L / 2 - 0.7, -W / 2), f2(L / 2, 0), f2(L / 2 - 0.7, W / 2), f2(-L / 2, W / 2)]) if arrow else rect(c[0], c[1], L, W, t)
    if sv < 0: g = affinity.rotate(g, math.pi, origin=c, use_radians=True)
    b = o.up(g, 0.56); o.cut(barl(f2(-L, u(-0.45, -0.35)), f2(L, u(-0.45, -0.35)), 0.12).intersection(g), b, 0.1)
    word = rng.choice(["PICCADILLY", "HELLFIRE", "TO FRONT", "AID POST", "GAS", "SNIPER", "HQ", "BOND ST", "WIPERS", "DUMP", "KEEP LOW"]).replace(" ", "-")
    wd = x_word(word, *f2(-0.25 * sv if arrow else 0.0, 0.12), t + (math.pi if sv < 0 else 0.0), 0.8, 0.15, L - (1.2 if arrow else 0.5))
    if wd is not None: o.cut(wd.intersection(inset(g, 0.18)), b, 0.12)
    o.studs([f2(0, v) for v in (-W * 0.32, W * 0.34)], b, 0.2, 0.08, "iron", 8)
    if rng.random() < 0.5: o.cut(U([disc(*f2(u(-L * 0.3, L * 0.2), u(-0.4, 0.4)), 0.16, 8) for _ in range(rng.randint(1, 2))]), b, 0.3)
    if rng.random() < 0.6:
        p = ff(-Ls / 2 + 2.2, 0.3); pts = [p, ff(-Ls / 2 + 2.5, 0.7), ff(-Ls / 2 + 3.2, u(0.9, 1.3)), ff(-Ls / 2 + 3.9, u(0.6, 1.1))]
        o.st(rod(pts, 0.17, 1.4), "wire", foot=LineString(pts).buffer(0.25))
    return o.out()


_OWN = set(globals())
