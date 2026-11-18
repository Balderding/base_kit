"""Second drawing, trederra: the trench details (see sd_r.py for the rule: a thing is never just a box or a blob, it
is a particular thing and carries what that thing has). Nothing hangs: a mortar's barrel and a gun on its tripod are
solid down to the ground, wire lies in its coil, a sign has fallen with its stake."""
import math
from sd_reg import d

def t_along(pts, step, first=None):
    """points along a bent line, one every `step`, each with the direction of the line there"""
    ln = LineString(pts); out = []; s = step * 0.5 if first is None else first
    while s < ln.length - step * 0.3:
        p = ln.interpolate(s); q = ln.interpolate(min(ln.length, s + 0.1)); out.append(((p.x, p.y), math.atan2(q.y - p.y, q.x - p.x))); s += step
    return out

def t_bar(p, q, rf, k=0.4, z0=None, z1=None):
    """a round bar from p to q with flat ends, solid underneath: rf is its radius (a number, or a function of 0..1
    along it: a rim, a neck, a taper). Lying, its axis is k radii off the ground; with z0 and z1 the axis climbs from
    one height to the other (a barrel on its mount, a leg coming down from a collar)."""
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

def t_belt(o, pts, kind="cloth"):
    """a cartridge belt lying on the ground: the webbing and a round across it every little way"""
    b = o.up(line(pts, 1.3), 0.28, kind)
    o.on(U([barl(pol(p[0], p[1], 0.42, t + 1.5708), pol(p[0], p[1], 0.42, t - 1.5708), 0.5) for p, t in t_along(pts, 0.95)]), b, 0.3, "brass")
    return b

def t_tin_box(o, cx, cy, a, L, W, h, rng, open_=False):
    """an ammunition tin: lid line, folding handle, a latch at one end and hinges at the other; open, it shows the
    rounds lying in it side by side and its lid lies beside it"""
    ff = frame(cx, cy, a); g = rrect(cx, cy, L, W, a, 0.15); b = o.up(g, h, "tin")
    if open_:
        o.cut(rect(cx, cy, L - 0.5, W - 0.5, a).difference(U([rect(*ff(x, 0), 0.32, W, a) for x in np.arange(-L / 2 + 0.55, L / 2 - 0.4, 0.6)])), b, 0.4)
        lg = rrect(*ff(rng.uniform(-0.3, 0.3), W + 0.25), L, W, a + rng.uniform(-0.2, 0.2), 0.15); l2 = o.up(lg, 0.26, "tin"); o.groove(inset(lg, 0.25), l2, 0.2, 0.15)
    else:
        lid = o.lid(g, b, 0.17, 0.13, "tin"); o.handle(ff(-L * 0.2, 0), ff(L * 0.2, 0), lid, 0.46, 0.36, "tin")
        o.tabs(ff, [L / 2 - 0.62], 0, lid, a, 0.5, 0.7, 0.2, "tin"); o.tabs(ff, [-L / 2 + 0.6], W * 0.24, lid, a, 0.5, 0.5, 0.18, "tin"); o.tabs(ff, [-L / 2 + 0.6], -W * 0.24, lid, a, 0.5, 0.5, 0.18, "tin")
    return b

# ---------------------------------------------------------------------------------------------------- redrawn
@d("trederra", "made", "wire", "wire_coil")
def _(cx, cy, a, rng):
    """a coil of barbed wire as it comes off the wagon: three or four turns lying on one another, barbs standing out
    all round, bound with ties in two or three places, the loose end run out along the ground"""
    u = rng.uniform; o = Obj("wire"); n = rng.randint(3, 4); R = u(2.0, 2.3) + 0.3 * (n - 3); t0 = u(0, 6.28); Ri = R - (n - 1) * 0.62
    def barbs(r, m, ph): return [barl(pol(p[0], p[1], 0.55, tb), pol(p[0], p[1], 0.55, tb + math.pi), 0.36) for p, tb in ((pol(cx, cy, r, ph + 6.2832 * j / m), ph + 6.2832 * j / m + rng.choice((0.6, -0.6))) for j in range(m))]
    o.st(ext(U(barbs(R + 0.1, int(R * 6.28 / 1.7), t0) + barbs(Ri - 0.1, max(3, int(Ri * 6.28 / 1.7)), t0 + 0.5)), 0.3), foot=disc(cx, cy, R + 0.6))
    o.st([s for k in range(n) for s in rod(arc(cx + u(-0.06, 0.06), cy + u(-0.06, 0.06), R - k * 0.62, t0, t0 + 6.2, 30), 0.33, 1.5 if k % 2 == 0 else 1.9)], foot=disc(cx, cy, R + 0.4))
    o.st(ext(U([rect(*pol(cx, cy, (R + Ri) / 2, t), R - Ri + 0.9, 0.5, t) for t in (t0 + 2.6 * k + u(-0.4, 0.4) for k in range(2))]), 0.74), "rope", foot=disc(cx, cy, R))
    t = a + u(-0.5, 0.5); e = pol(cx, cy, R + u(2.2, 3.6), t); pts = wiggle(pol(cx, cy, R + 0.75, t), e, rng, 0.6, 4)
    o.st(ext(U([line(pts, 0.5)] + [barl(pol(p[0], p[1], 0.5, tt + 0.9), pol(p[0], p[1], 0.5, tt + 0.9 + math.pi), 0.36) for p, tt in t_along(pts, 1.1)]), 0.4), foot=LineString(pts).buffer(0.6))
    return o.out()

@d("trederra", "made", "tin", "tin_hat")
def _(cx, cy, a, rng):
    """a steel helmet of the soup-plate kind: the wide brim with its rolled edge, the round crown, the chin strap
    lying out to one side; some shot through, some lying upside down showing the liner straps and the pad"""
    u = rng.uniform; R = u(1.7, 2.0); sq = u(0.86, 0.94); rc = R * 0.66; o = Obj("tin"); E = lambda r: ell(cx, cy, r, r * sq, a, 28)
    t = a + u(0, 6.28); pts = [pol(cx, cy, R - 0.3, t)] + arc(*pol(cx, cy, R + 0.5, t + 0.3), u(0.8, 1.1), t - 1.2, t + 1.9, 10) + [pol(cx, cy, R - 0.3, t + 0.9)]
    o.st(ext(line(pts, 0.5), 0.26), "leather", foot=LineString(pts).buffer(0.5))
    if rng.random() < 0.3:
        b = o.up(E(R), 0.5); bw = o.on(E(rc + 0.3), b, 0.5)
        o.cut(E(rc).difference(U([barl(pol(cx, cy, rc, a + k * 1.5708), pol(cx, cy, rc, a + k * 1.5708 + math.pi), 0.46) for k in range(2)] + [disc(cx, cy, 0.5, 10)])), bw, 0.5)
        return o.out()
    h = u(1.1, 1.35)
    tb = a + u(0, 6.28); cuts = [(Polygon([pol(cx, cy, R * 0.72, tb), pol(cx, cy, R * 1.2, tb - 0.22), pol(cx, cy, R * 1.2, tb + 0.22)]), 0.0)] if rng.random() < 0.3 else []                # a bite out of the brim
    o.st(turn(cx, cy, [(R, 0.0), (R, 0.2), (R - 0.22, 0.22), (rc + 0.1, 0.42)] + crv(rc, 0.3, 0.42, h, n=8), 28, sq, a), cuts=cuts)
    return o.out()

@d("trederra", "made", "cloth", "gas_mask")
def _(cx, cy, a, rng):
    """a box respirator: the face piece with its two round eye pieces and the mouth valve, head straps lying out
    behind, the ribbed hose running to the filter tin in its satchel with flap and strap"""
    u = rng.uniform; ff = frame(cx, cy, a); o = Obj("cloth")
    g = hull([disc(*ff(-0.35, 0.6), 0.8), disc(*ff(-0.35, -0.6), 0.8), disc(*ff(0.85, 0), 0.62)])
    for sv in (-1, 0, 1):
        e = ff(-u(1.9, 2.4), sv * u(0.8, 1.2)); pts = [ff(-0.8, sv * 0.6), e]; o.st(ext(line(pts, 0.5), 0.24), "leather", foot=LineString(pts).buffer(0.4))
    b = o.up(g, u(0.6, 0.75)); ey = [ff(-0.35, sv * 0.62) for sv in (-1, 1)]
    r1 = o.on(U([disc(x, y, 0.48, 14) for x, y in ey]), b, 0.2, "tin"); o.cut(U([disc(x, y, 0.27, 10) for x, y in ey]), r1, 0.2)
    o.on(disc(*ff(0.75, 0), 0.46, 12), b, 0.36, "tin")
    c = ff(u(3.2, 4.0), u(-1.6, 1.6)); t = a + u(-0.7, 0.7); f2 = frame(c[0], c[1], t); pts = wiggle(ff(1.5, 0), f2(-1.2, 0), rng, 0.7, 4)
    o.st(rod(pts, lambda q: 0.36 + 0.1 * (math.sin(q * 45) > 0), 1.0), "cable", foot=LineString(pts).buffer(0.5))
    sg = rrect(c[0], c[1], 2.1, 1.7, t, 0.3); s = o.up(sg, u(0.7, 0.9)); fl = o.on(rrect(*f2(-0.35, 0), 1.3, 1.6, t, 0.25), s, 0.15)
    o.studs([f2(0.1, sv * 0.45) for sv in (-1, 1)], fl, 0.25, 0.13, "brass")
    e = f2(u(0.5, 1.2), rng.choice((-1, 1)) * u(1.8, 2.4)); pts = [f2(0.7, 0.6), e, f2(0.7, -0.6)] if rng.random() < 0.5 else [f2(0.7, 0), e]
    o.st(ext(line(pts, 0.5), 0.24), "leather", foot=LineString(pts).buffer(0.4))
    return o.out()

@d("trederra", "made", "brass", "shell_cases")
def _(cx, cy, a, rng):
    """spent shell cases: each a brass tube with its rim at the base, the shoulder drawing in to the mouth; a heap
    of them lying side by side with more on top, or thrown down, one or two standing with the open mouth up"""
    u = rng.uniform; o = Obj("brass"); ff = frame(cx, cy, a); r = u(0.62, 0.74); L = u(3.4, 4.2)
    def case(x, y, b, lift=0.0, par=None):
        f2 = frame(x, y, b); sv = rng.choice((-1, 1))
        rf = lambda s: r * (1.17 if s < 0.1 else 1.0 if s < 0.66 else 1.0 - 0.2 * min(1.0, (s - 0.66) / 0.08))
        o.st(t_bar(f2(-sv * L / 2, 0), f2(sv * L / 2, 0), rf, z0=lift + 0.4 * r, z1=lift + 0.4 * r), foot=rect(x, y, L, 2 * r, b))
    if rng.random() < 0.5:
        n = rng.randint(2, 4)
        for i in range(n): case(*ff(u(-0.3, 0.3), (i - (n - 1) / 2) * (2 * r + 0.1)), a + u(-0.06, 0.06))
        for i in range(rng.randint(1, n - 1)): case(*ff(u(-0.4, 0.4), (i - (n - 2) / 2) * (2 * r + 0.1)), a + u(-0.12, 0.12), lift=r * 1.25)
    else:
        for i in range(rng.randint(2, 4)): c = pol(cx, cy, 0 if i == 0 else u(1.6, 2.8), a + i * 1.7); case(c[0], c[1], a + u(-1.2, 1.2))
    for i in range(rng.randint(1, 2)):
        p = ff(u(-1.5, 1.5), (-1) ** i * u(3.2, 3.9)); h = L * 0.75
        o.st(turn(p[0], p[1], [(r * 1.18, 0.0), (r * 1.18, 0.22), (r, 0.26), (r, h * 0.72), (r * 0.82, h * 0.82), (r * 0.82, h)], 16), cuts=[(disc(p[0], p[1], r * 0.82 - 0.2, 10), h - 0.7)], foot=disc(p[0], p[1], r * 1.2))
    return o.out()

@d("trederra", "made", "tin", "ammo_box")
def _(cx, cy, a, rng):
    """an ammunition tin with its lid line, folding handle, latch and hinges, a belt of rounds run out of it along
    the ground; or open, the rounds showing in rows and the lid beside it"""
    u = rng.uniform; ff = frame(cx, cy, a); L = u(3.0, 3.6); W = u(1.6, 1.9); h = u(1.0, 1.3); o = Obj("tin")
    t_tin_box(o, cx, cy, a, L, W, h, rng, rng.random() < 0.4)
    e = ff(L / 2 + u(2.2, 3.4), -u(0.8, 2.0)); t_belt(o, [ff(L / 2 + 0.1, -W * 0.2)] + wiggle(ff(L / 2 + 0.7, -W * 0.3), e, rng, 0.5, 4))
    return o.out()

@d("trederra", "made", "tin", "periscope")
def _(cx, cy, a, rng):
    """a trench periscope laid down: the long square tube with its clamp bands, the head box with the window that
    looks over the parapet, the eyepiece turned out at the foot, the wooden grip below it"""
    u = rng.uniform; ff = frame(cx, cy, a); L = u(6.0, 7.5); o = Obj("tin")
    b = o.up(rect(cx, cy, L, 1.0, a), 0.7); o.straps(ff, [-L * 0.18, L * 0.14], 0.2, b, a, 0.5, 0.14)
    hd = o.up(rrect(*ff(L / 2 - 0.5, 0), 1.7, 1.5, a, 0.15), 1.0); wn = rect(*ff(L / 2 - 0.45, 0), 1.0, 0.9, a); o.cut(wn, hd, 0.35); o.groove(wn.buffer(0.2, join_style=2), hd, 0.17, 0.12)
    sv = rng.choice((-1, 1)); ey = o.up(rect(*ff(-L / 2 + 0.6, sv * 0.8), 1.0, 1.0, a), 0.7); o.on(disc(*ff(-L / 2 + 0.6, sv * 0.85), 0.42, 12), ey, 0.22);
    o.st(rod([ff(-L / 2 - 0.1, 0), ff(-L / 2 - u(1.4, 1.9), 0)], 0.4, 1.1), "wood", foot=disc(*ff(-L / 2 - 1.0, 0), 1.0))
    return o.out()

@d("trederra", "made", "wood", "field_telephone")
def _(cx, cy, a, rng):
    """a field telephone: the wooden case with its lid, the handset lying in its rest on top (ear piece, grip, mouth
    piece), two terminals, the crank at the end, the carrying strap, and the line run off to its cable drum"""
    u = rng.uniform; ff = frame(cx, cy, a); L = u(2.9, 3.4); W = u(2.0, 2.3); h = u(1.0, 1.3); o = Obj("wood"); g = rrect(cx, cy, L, W, a, 0.2)
    pts = [ff(-L * 0.3, W / 2 - 0.2)] + arc(*ff(0, W / 2 + 0.4), L * 0.3, a + math.pi, a, 10)[1:-1] + [ff(L * 0.3, W / 2 - 0.2)]; pts = [(x + u(-0.1, 0.1), y + u(-0.1, 0.1)) for x, y in pts]
    o.st(ext(line(pts, 0.5), 0.24), "leather", foot=LineString(pts).buffer(0.4))
    b = o.up(g, h); lid = o.lid(g, b, 0.16, 0.15); v = W * 0.14
    o.on(U([disc(*ff(sx * L * 0.3, v), 0.5, 12) for sx in (-1, 1)]), lid, 0.4, "tar"); o.on(capl(ff(-L * 0.3, v), ff(L * 0.3, v), 0.46), lid, 0.28, "tar")
    o.studs([ff(sx * 0.45, -W * 0.3) for sx in (-1, 1)], lid, 0.27, 0.2, "brass")
    k = ff(L / 2 + 0.25, -W * 0.1); k2 = ff(L / 2 + 0.3, -W * 0.1 + 0.8)
    o.st(ext(U([rect(k[0], k[1], 0.6, 0.5, a), capl(k, k2, 0.46), disc(k2[0], k2[1], 0.32, 8)]), h * 0.6) + cyl(k2[0], k2[1], 0.32, h * 0.6 + 0.3, n=8), "iron", foot=disc(k2[0], k2[1], 0.5))
    R = u(0.95, 1.15); c = ff(u(-1.0, 1.0), -W / 2 - u(2.2, 3.0)); pts = wiggle(ff(0, -W / 2 - 0.25), pol(c[0], c[1], R + 0.2, a + 1.57), rng, 0.5, 3)
    o.st(rod(pts, 0.28, 1.0), "wire", foot=LineString(pts).buffer(0.4))
    dr = o.up(disc(c[0], c[1], R, 20), 0.3, "wood"); o.on(ann(c[0], c[1], R - 0.12, R - 0.6, 20), dr, 0.4, "wire"); o.on(disc(c[0], c[1], 0.36, 10), dr, 0.62, "wood")
    return o.out()

@d("trederra", "made", "iron", "mortar")
def _(cx, cy, a, rng):
    """a trench mortar set up: the ribbed base plate, the barrel rising from its socket to the open muzzle, the two
    legs of the bipod coming down from the collar to their feet; a bomb or two with their fins lying ready"""
    u = rng.uniform; ff = frame(cx, cy, a); o = Obj("iron"); D = u(3.2, 3.8); zt = u(1.5, 1.9); B = ff(-D / 2, 0); M = ff(D / 2, 0)
    bp = rrect(B[0] - 0.3 * math.cos(a), B[1] - 0.3 * math.sin(a), 2.0, 2.2, a, 0.3); b = o.up(bp, 0.34); f2 = frame(*ff(-D / 2 - 0.3, 0), a)
    o.on(U([rect(*f2(0, sv * 0.75), 1.7, 0.46, a) for sv in (-1, 1)]), b, 0.16)
    C = ff(D * 0.18, 0); zc = 0.4 + (zt - 0.4) * 0.68; ps = t_bar(B, M, 0.52, z0=0.4, z1=zt)
    feet = [ff(D * 0.3 + u(-0.2, 0.3), sv * u(1.5, 1.9)) for sv in (-1, 1)]
    for p in feet: ps += t_bar(p, C, 0.28, z0=0.1, z1=zc - 0.1) + cyl(p[0], p[1], 0.45, 0.25, n=8)
    o.st(ps, cuts=[(disc(*ff(D / 2 - 0.42, 0), 0.3, 10), zt - 0.1)], foot=hull([bp, disc(M[0], M[1], 0.6)] + [disc(x, y, 0.5) for x, y in feet]))
    for i in range(rng.randint(1, 2)):
        c = ff(-D * 0.1 + i * 1.3 + u(-0.3, 0.3), -u(2.6, 3.1)); t = a + u(-0.5, 0.5); f3 = frame(c[0], c[1], t)
        o.st(spin(c[0], c[1], t, 1.7, lambda s: 0.5 * math.sqrt(max(0.02, 1 - (s / 0.86) ** 2)), 1.0) + rod([f3(-0.7, 0), f3(-1.5, 0)], 0.25, 1.0) + ext(rect(*f3(-1.35, 0), 0.5, 1.0, t), 0.3) + cyl(*f3(0.9, 0), 0.25, 0.3, n=6), foot=rect(*f3(-0.3, 0), 2.8, 1.1, t))
    return o.out()

@d("trederra", "made", "cloth", "stretcher")
def _(cx, cy, a, rng):
    """a stretcher set down: two poles with the handles standing out at both ends, the canvas slung between them
    with its seam and stitched bands, the iron spreader bars on their feet; a folded blanket left on it"""
    u = rng.uniform; ff = frame(cx, cy, a); L = u(8.0, 9.5); W = u(2.4, 2.8); o = Obj("cloth")
    cg = rect(cx, cy, L - 2.6, W - 0.2, a); b = o.up(cg, 0.3); o.cut(barl(ff(-L / 2 + 1.5, 0), ff(L / 2 - 1.5, 0), 0.2), b, 0.15)
    o.on(U([rect(*ff(sx * (L / 2 - 2.3), 0), 0.5, W - 0.5, a) for sx in (-1, 1)]), b, 0.13)
    if rng.random() < 0.55:
        c = ff(u(-1.5, 1.5), u(-0.1, 0.1)); t = a + u(-0.15, 0.15); bl = rrect(c[0], c[1], u(1.7, 2.3), W - 0.9, t, 0.25); k = o.on(bl, b, u(0.4, 0.6), "sack"); f2 = frame(c[0], c[1], t)
        o.cut(U([barl(f2(-2, v), f2(2, v), 0.2) for v in (-0.3, 0.3)]), k, 0.15)
    else: o.cut(ell(*ff(u(-0.6, 0.6), 0), L * 0.22, W * 0.26, a, 16), b, 0.15)
    for sv in (-1, 1): o.st(rod([ff(-L / 2, sv * W / 2), ff(L / 2, sv * W / 2)], lambda q: 0.3 if q < 0.1 or q > 0.9 else 0.4, 1.2), "wood", foot=rect(*ff(0, sv * W / 2), L, 0.8, a))
    for sx in (-1, 1):
        x = sx * (L / 2 - 1.45); o.st(ext(rect(*ff(x, 0), 0.5, W, a), 0.52) + [s for sv in (-1, 1) for s in cyl(*ff(x, sv * (W / 2 + 0.1)), 0.4, 0.7, n=8)], "iron", foot=rect(*ff(x, 0), 0.6, W + 0.8, a))
    return o.out()

@d("trederra", "made", "tin", "ration_tins")
def _(cx, cy, a, rng):
    """ration tins: round ones with the pressed lid, one opened with its jagged lid bent back and the hollow showing,
    a tapered bully-beef tin with its key, a kidney-shaped mess tin with its folding handle, a spoon"""
    u = rng.uniform; o = Obj("tin"); n = rng.randint(3, 5); kinds = ["round", "open", "bully", "mess"]; rng.shuffle(kinds)
    for i in range(n):
        x, y = pol(cx, cy, 0 if i == 0 else u(1.9, 2.6), a + i * 6.2832 / max(1, n - 1) + u(-0.3, 0.3)); t = u(0, 6.28); f2 = frame(x, y, t); k = kinds[i % 4]
        if k == "round":
            r = u(0.6, 0.75); b = o.up(disc(x, y, r, 16), u(0.7, 1.1)); o.on(ann(x, y, r, r - 0.3, 16), b, 0.14); o.cut(ann(x, y, r * 0.4, r * 0.4 - 0.18, 10), b, 0.12)
        elif k == "open":
            r = u(0.65, 0.8); b = o.up(disc(x, y, r, 16), u(0.8, 1.1)); o.cut(disc(x, y, r - 0.26, 12), b, 0.5)
            c = pol(x, y, r + 0.35, t); o.up(star(c[0], c[1], r - 0.1, r - 0.3, 9, t), 0.3)
        elif k == "bully":
            g = Polygon([f2(-0.9, -0.6), f2(0.9, -0.48), f2(0.9, 0.48), f2(-0.9, 0.6)]).buffer(-0.12).buffer(0.12, 6); b = o.up(g, u(0.7, 0.9)); o.lid(g, b, 0.13, 0.16)
            o.st(ext(U([capl(f2(1.0, 0), f2(1.6, u(-0.3, 0.3)), 0.46), ann(*f2(1.75, 0), 0.4, 0.17, 8)]), 0.26), "iron", foot=disc(*f2(1.5, 0), 0.6))
        else:
            g = hull([disc(*f2(0, -0.45), 0.62, 12), disc(*f2(0, 0.45), 0.62, 12), disc(*f2(0.25, 0), 0.6, 12)]); b = o.up(g, u(0.8, 1.0)); o.cut(inset(g, 0.26), b, 0.45)
            o.st(ext(line([f2(-0.5, 0.3), f2(-1.6, 0.22), f2(-1.6, -0.22), f2(-0.5, -0.3)], 0.46), 0.3), "iron", foot=disc(*f2(-1.2, 0), 0.6))
    p = pol(cx, cy, u(1.0, 1.4), a + u(0, 6.28)); t = u(0, 6.28); q = pol(p[0], p[1], 1.5, t); e = pol(p[0], p[1], 1.75, t)
    o.st(ext(U([capl(p, q, 0.46), ell(e[0], e[1], 0.52, 0.36, t, 10)]), 0.26), cuts=[(ell(e[0], e[1], 0.32, 0.18, t, 8), 0.1)], foot=LineString([p, e]).buffer(0.5))
    return o.out()

@d("trederra", "made", "iron", "screw_pickets")
def _(cx, cy, a, rng):
    """screw pickets for the wire, thrown down in a bundle: each an iron rod with the eye at its head, two or three
    loops twisted into its length and the corkscrew at its foot"""
    u = rng.uniform; o = Obj("iron"); n = rng.randint(2, 3)
    for i in range(n):
        f2 = frame(cx + u(-0.8, 0.8), cy + (i - (n - 1) / 2) * u(1.5, 1.9), a + u(-0.3, 0.3)); L = u(6.0, 7.5); sv = rng.choice((-1, 1)); x1 = L / 2 - 1.9
        gs = [barl(f2(-L / 2, 0), f2(x1, 0), 0.52), ann(*f2(-L / 2 - 0.4, 0), 0.62, 0.26, 10)] + [ann(*f2(x, sv * 0.6), 0.6, 0.26, 10) for x in ([-L * 0.22, L * 0.08] if rng.random() < 0.5 else [-L * 0.28, -L * 0.06, L * 0.14])]
        gs.append(line([f2(x1 + s, 0.5 * math.sin(s * 6.2832 / 0.95)) for s in np.linspace(0, 1.9, 24)] + [f2(L / 2 + 0.3, 0)], 0.5))
        o.st(ext(U(gs), u(0.45, 0.55)), foot=rect(*f2(0, 0), L + 1.6, 2.0, a))
    return o.out()

@d("trederra", "made", "iron", "sniper_plate")
def _(cx, cy, a, rng):
    """a sniper's loophole plate laid flat: thick iron with rivets round the edge, the keyhole loophole cut through
    it and its shutter swung aside on a pivot, the carrying slot, bullet splashes, the two prop legs folded out"""
    u = rng.uniform; ff = frame(cx, cy, a); L = u(3.8, 4.6); W = u(2.8, 3.4); o = Obj("iron"); g = rrect(cx, cy, L, W, a, 0.25)
    for sx in (-1, 1): p = ff(sx * L * 0.3, W / 2 + 0.35); e = ff(sx * L * u(0.3, 0.45), W / 2 + u(1.6, 2.2)); o.st(ext(capl(p, e, 0.5), 0.36), foot=LineString([p, e]).buffer(0.4))
    b = o.up(g, 0.55)
    o.studs([ff(sx * (L / 2 - 0.4), v) for sx in (-1, 1) for v in np.linspace(-W / 2 + 0.4, W / 2 - 0.4, 4)] + [ff(x, sy * (W / 2 - 0.4)) for sy in (-1, 1) for x in np.linspace(-L / 2 + 0.4, L / 2 - 0.4, 5)[1:-1]], b, 0.26, 0.14)
    c = ff(-L * 0.08, -W * 0.05); o.cut(U([rect(c[0], c[1], 1.5, 0.42, a), disc(c[0], c[1], 0.42, 10)]), b, 0.5)
    pv = ff(-L * 0.08 + 1.2, -W * 0.05 + 0.7); t = a + u(0.3, 1.2); sh = o.on(rrect(*pol(pv[0], pv[1], 0.55, t + math.pi), 1.7, 0.75, t, 0.2), b, 0.17); o.on(disc(pv[0], pv[1], 0.3, 8), b, 0.3)
    o.cut(capl(ff(-0.5, -W / 2 + 0.95), ff(0.5, -W / 2 + 0.95), 0.34), b, 0.5)
    hits = [ff(u(-L * 0.4, -L * 0.2), u(0.2, W * 0.3)) for i in range(rng.randint(0, 3))]
    if hits: o.cut(U([disc(x, y, u(0.24, 0.32), 7) for x, y in hits]), b, 0.22)
    return o.out()

@d("trederra", "made", "iron", "dud_shell", ground=True)
def _(cx, cy, a, rng):
    """a dud: a heavy shell lying where it ploughed in, the copper driving band near its base, the long curve of
    the nose and the fuze cap, the furrow it cut and the earth thrown up beside it"""
    u = rng.uniform; ff = frame(cx, cy, a); L = u(4.4, 5.6); r = u(0.85, 1.05); o = Obj("iron")
    def rf(t):
        x = (t + L / 2) / L
        if x > 0.93: return r * 0.3
        if x > 0.5: return r * max(0.3, math.sqrt(max(0.0, 1 - ((x - 0.5) / 0.5) ** 2)))
        return r * (0.9 if x < 0.05 else 1.1 if 0.09 < x < 0.19 else 1.0)
    fur = hull([disc(*ff(-L / 2 - u(1.2, 2.2), u(-0.3, 0.3)), r * 0.7), disc(*ff(-L * 0.2, 0), r + 0.5)]).difference(hull([disc(*ff(-L / 2, 0), r + 0.25), disc(*ff(L / 2, 0), r + 0.25)]))
    o.gcut(fur, u(0.3, 0.4), "pit")
    o.st(spin(cx, cy, a, L, rf, 1.0))
    for sv in (-1, 1):
        if rng.random() < 0.75: c = ff(u(-L * 0.1, L * 0.3), sv * (r + u(0.8, 1.1))); o.st(dome(c[0], c[1], u(0.7, 1.0), u(0.35, 0.5), sq=u(0.5, 0.7), a=a + u(-0.3, 0.3)), "earth", foot=disc(c[0], c[1], 1.0))
    return o.out()

@d("trederra", "made", "leather", "binoculars_map")
def _(cx, cy, a, rng):
    """a trench map spread out with its fold lines and the trench lines drawn on it, held down by a pair of field
    glasses (two tapering barrels, the eye cups, the hinged bridge between them), a pencil beside them"""
    u = rng.uniform; ff = frame(cx, cy, a); L = u(4.2, 5.2); W = u(3.2, 3.8); o = Obj("leather"); g = rect(cx, cy, L, W, a)
    m = o.up(g, 0.26, "paper"); c = ff(u(-0.7, 0.7), u(-0.3, 0.3)); t = a + u(0.3, 1.3); f2 = frame(c[0], c[1], t); keep = rect(c[0], c[1], 3.0, 2.6, t)
    o.cut(U([barl(ff(x, -W), ff(x, W), 0.2) for x in (-L / 6, L / 6)] + [barl(ff(-L, 0), ff(L, 0), 0.2)]).difference(keep), m, 0.15)
    zz = [line([ff(x, v + 0.3 * (1 if int((x + L) / 0.5) % 2 else -1)) for x in np.arange(-L / 2 + 0.3, L / 2 - 0.2, 0.5)], 0.2) for v in (-W * 0.3, W * 0.28)]
    o.cut(U(zz).difference(keep), m, 0.15)
    ps = []
    for sv in (-1, 1):
        ps += t_bar(f2(-1.2, sv * 0.56), f2(1.0, sv * 0.64), lambda s: 0.45 if s < 0.12 else 0.36 if s < 0.42 else 0.36 + 0.18 * min(1.0, (s - 0.42) / 0.12), 0.5)
    ps += ext(rect(*f2(-0.1, 0), 0.6, 0.9, t), 0.42) + cyl(*f2(-0.1, 0), 0.3, 0.62, n=8)
    o.st(ps, par=m)
    p = ff(u(-L * 0.3, L * 0.3), -W / 2 - u(0.6, 1.0)); tp = a + u(-0.4, 0.4); q = pol(p[0], p[1], 2.0, tp); o.st(ext(U([barl(p, q, 0.5), tri(q, pol(q[0], q[1], 0.6, tp), 0.5)]), 0.34), "wood", foot=LineString([p, q]).buffer(0.5))
    return o.out()

@d("trederra", "growth", "flower", "poppies")
def _(cx, cy, a, rng):
    """poppies: each bloom four round petals cupped about the seed head in its middle, above a rosette of jagged
    leaves; a bud or two still shut"""
    u = rng.uniform; o = Obj("flower"); foot = []
    for i in range(rng.randint(3, 6)):
        x, y = pol(cx, cy, 0 if i == 0 else u(1.5, 2.8), a + i * 1.3 + u(-0.3, 0.3)); r = u(0.8, 1.05); t = u(0, 1.57); h = u(0.6, 0.9); foot.append(disc(x, y, r + 0.3))
        o.st(ext(star(x, y, r * 1.45, r * 0.55, rng.randint(5, 6), u(0, 1)), 0.26), "grass", foot=disc(x, y, r))
        if rng.random() < 0.2: o.st(dome(x, y, 0.42, h, sq=0.75, a=t), foot=disc(x, y, 0.4)); continue
        pet = U([disc(*pol(x, y, r * 0.5, t + k * 1.5708), r * 0.54, 12) for k in range(4)])
        cuts = [(ann(x, y, r * 0.5, 0.3, 10), h - 0.28)] + [(barl(pol(x, y, r * 0.45, t + 0.785 + k * 1.5708), pol(x, y, r * 1.2, t + 0.785 + k * 1.5708), 0.22), h - 0.3) for k in range(4)]
        o.st(ext(pet, h), cuts=cuts, foot=disc(x, y, r))
    return hull(foot), o.L

@d("trederra", "made", "hide", "rats")
def _(cx, cy, a, rng):
    """trench rats: a fat pear of a body, the pointed head with its two round ears, the feet tucked at its sides and
    the long bare tail curling away behind"""
    u = rng.uniform; o = Obj("hide"); foot = []
    for i in range(rng.randint(1, 3)):
        x, y = pol(cx, cy, 0 if i == 0 else u(2.6, 3.6), a + i * 2.2); b = u(0, 6.28); f2 = frame(x, y, b); L = u(2.6, 3.2); w = L * 0.24
        ps = spin(x, y, b, L, lambda t: w * math.sqrt(max(0.02, 1 - (2 * t / L) ** 2)) * (1.0 - 0.5 * t / L), 1.05)
        ps += rod([f2(L * 0.32, 0), f2(L / 2 + 0.75, 0)], lambda q: 0.5 - 0.24 * q, 1.2) + [s for sv in (-1, 1) for s in cyl(*f2(L * 0.36, sv * 0.42), 0.27, 0.78, n=8)]
        sv = rng.choice((-1, 1)); tl = [f2(-L / 2 + 0.2, 0), f2(-L / 2 - 0.8, sv * u(0.1, 0.5)), f2(-L / 2 - 1.6, sv * u(0.5, 1.1)), f2(-L / 2 - 2.1, sv * u(1.2, 1.9))]
        o.st(ps, foot=disc(x, y, L / 2 + 0.8)); o.st(rod(tl, 0.26, 0.9), foot=LineString(tl).buffer(0.4)); foot += [disc(x, y, L / 2 + 0.9), LineString(tl).buffer(0.4)]
    return hull(foot), o.L

@d("trederra", "made", "wood", "stick_grenades")
def _(cx, cy, a, rng):
    """stick grenades: the can of a head with its band, the long wooden handle and the screw cap at its end; a few
    thrown down together, or lying in a row in their open carrying crate with its rope handles"""
    u = rng.uniform; o = Obj("wood"); ff = frame(cx, cy, a); Lg = u(3.2, 3.8)
    def gren(x, y, b, par=None):
        f2 = frame(x, y, b); sv = rng.choice((-1, 1))
        o.st(t_bar(f2(-sv * Lg / 2, 0), f2(sv * Lg / 2, 0), lambda s: 0.38 if s < 0.09 else 0.29 if s < 0.62 else 0.6 if 0.7 < s < 0.78 else 0.54, 0.5), par=par, foot=rect(x, y, Lg, 1.2, b))
    if rng.random() < 0.5:
        n = rng.randint(3, 4); W = n * 1.2 + 1.3; L = Lg + 1.7; g = rect(cx, cy, L, W, a); b = o.up(g, 0.3)
        o.on(g.difference(rect(cx, cy, L - 1.2, W - 1.2, a)), b, 0.75); o.seams(ff, L - 1.3, W - 1.3, b, 1.1, w=0.2, d=0.15)
        for i in range(n):
            if rng.random() < 0.85: gren(*ff(u(-0.1, 0.1), (i - (n - 1) / 2) * 1.2), a + u(-0.05, 0.05), b)
        for sx in (-1, 1): c = ff(sx * (L / 2 + 0.25), 0); pts = arc(c[0], c[1], 0.7, a - 1.5708 if sx > 0 else a + 1.5708, a + 1.5708 if sx > 0 else a + 4.712, 8); o.st(rod(pts, 0.27, 1.0), "rope", foot=disc(c[0], c[1], 1.0))
        if rng.random() < 0.6: c = ff(u(-1, 1), -W / 2 - u(0.9, 1.4)); gren(c[0], c[1], a + u(-0.5, 0.5))
    else:
        for i in range(rng.randint(2, 4)): x, y = pol(cx, cy, 0 if i == 0 else u(1.3, 2.0), a + i * 1.6); gren(x, y, a + u(-0.9, 0.9))
    return o.out()

@d("trederra", "made", "brass", "bugle")
def _(cx, cy, a, rng):
    """a bugle lying on its side: the tube from the mouthpiece doubled once into a long loop, the bell flaring wide,
    the cup of the mouthpiece, and its cord with a tassel thrown out beside it"""
    u = rng.uniform; ff = frame(cx, cy, a); L = u(4.6, 5.4); o = Obj("brass"); sv = rng.choice((-1, 1))
    lp = ell(*ff(-L * 0.12, sv * 0.52), L * 0.3, 0.62, a, 24)
    o.st(ext(U([lp.exterior.buffer(0.27), barl(ff(-L / 2 - 0.3, 0), ff(0.1, 0), 0.6), disc(*ff(-L / 2 - 0.5, 0), 0.42, 10)]), 0.56), foot=rect(*ff(-L * 0.15, 0), L * 0.8, 2.4, a))
    o.st(t_bar(ff(0, 0), ff(L / 2, 0), lambda s: 0.3 + 0.95 * s ** 2.2, 0.3), foot=rect(*ff(L * 0.25, 0), L / 2, 2.6, a))
    p = ff(-L * 0.2, -sv * 0.4); e = ff(-L * 0.2 + u(-1.0, 1.0), -sv * u(1.8, 2.6)); pts = wiggle(p, e, rng, 0.5, 3)
    o.st(rod(pts, 0.26, 0.9) + dome(e[0], e[1], 0.42, 0.5), "cloth", foot=LineString(pts).buffer(0.5))
    return o.out()

@d("trederra", "made", "iron", "machine_gun")
def _(cx, cy, a, rng):
    """a heavy machine gun on its tripod: the fat ribbed water jacket and the muzzle, the breech casing with its
    cover and sight, the two spade grips, three splayed legs with their feet, and the belt run in from an
    ammunition tin at its side"""
    u = rng.uniform; ff = frame(cx, cy, a); o = Obj("iron"); sv = rng.choice((-1, 1))
    feet = [ff(u(1.7, 2.2), 1.7), ff(u(1.7, 2.2), -1.7), ff(-u(2.8, 3.4), u(-0.3, 0.3))]
    o.st([s for p in feet for s in t_bar(p, (cx, cy), 0.32, z0=0.05, z1=0.7) + cyl(p[0], p[1], 0.46, 0.26, n=8)] + cyl(cx, cy, 0.62, 1.0, n=12), foot=hull([disc(x, y, 0.5) for x, y in feet]))
    z = u(1.0, 1.3); Lj = u(3.0, 3.6); rj = 0.62
    ps = t_bar(ff(0.2, 0), ff(1.0 + Lj, 0), lambda q: (rj + 0.07 * (math.sin(q * (Lj + 0.8) * 9.5) > 0)) if q < Lj / (Lj + 0.8) else 0.36 if q < 0.94 else 0.46, z0=z, z1=z)
    o.st(ps, foot=rect(*ff(0.6 + Lj / 2, 0), Lj + 1.2, 1.4, a))
    bg = rect(*ff(-0.95, 0), 2.3, 1.1, a); bx = o.up(bg, z + rj); cv = o.lid(bg, bx, 0.14, 0.15); o.on(rect(*ff(-0.6, 0), 0.46, 0.6, a), cv, 0.3)
    o.on(rect(*ff(-0.2, 0), 0.5, 1.1, a), bx, 0.14)
    o.st([s for s2 in (-1, 1) for s in cyl(*ff(-2.45, s2 * 0.36), 0.26, z + 0.45, n=8)] + ext(rect(*ff(-2.25, 0), 0.5, 0.5, a), z + 0.3), "wood", foot=rect(*ff(-2.4, 0), 0.8, 1.5, a))
    c = ff(u(-0.6, 0.4), sv * u(3.0, 3.6)); t = a + u(-0.3, 0.3); t_tin_box(o, c[0], c[1], t, 2.0, 1.3, u(0.8, 1.0), rng, True)
    t_belt(o, [ff(-0.4, sv * 0.6), ff(-0.4 + u(-0.3, 0.3), sv * 1.5), pol(c[0], c[1], 0.3, a - sv * 1.57)])
    return o.out()

# ---------------------------------------------------------------------------------------------------- new
@d("trederra", "made", "sack", "sandbags")
def _(cx, cy, a, rng):
    """sandbags: each a plump pillow with its seam along the top and the tied neck standing out at one end, laid in
    a row with the next course breaking joint on top; one has slipped off and lies beside the stack"""
    u = rng.uniform; ff = frame(cx, cy, a); L = u(2.9, 3.3); W = u(1.7, 2.0); h = u(0.8, 0.95); n = rng.randint(2, 3); foot = []; lay = []
    def bag(x, y, b, lift=0.0):
        b += math.pi * rng.randint(0, 1); foot.append(rect(x, y, L + 1.0, W, b)); return _S()._sack(x, y, b, L * u(0.93, 1.05), W, h, lift)
    lay.append(ST([s for i in range(n) for s in bag(*ff((i - (n - 1) / 2) * (L - 0.1), u(-0.12, 0.12)), a + u(-0.07, 0.07))], "sack"))
    for lv in range(1, rng.randint(1, 2) + 1):
        if n - lv > 0: lay.append(ST([s for i in range(n - lv) for s in bag(*ff((i - (n - lv - 1) / 2) * (L - 0.1) + u(-0.1, 0.1), u(-0.15, 0.15)), a + u(-0.09, 0.09), lv * h * 0.8)], "sack"))
    if rng.random() < 0.6: c = ff(u(-L, L) * 0.6, rng.choice((-1, 1)) * (W + u(0.3, 0.9))); lay.append(ST(bag(c[0], c[1], a + u(-0.8, 0.8)), "sack"))
    return hull(foot), lay

@d("trederra", "made", "wood", "duckboard")
def _(cx, cy, a, rng):
    """a length of duckboard: two long runners and the slats nailed across them with gaps between, a slat missing
    here, one snapped there, the runners' ends standing out"""
    u = rng.uniform; ff = frame(cx, cy, a); n = rng.randint(6, 8); st = 1.25; L = n * st + 1.0; W = u(3.2, 3.8); o = Obj("wood")
    o.up(U([rect(*ff(0, sv * W * 0.3), L, 0.92, a) for sv in (-1, 1)]), 0.42)
    for i in range(n):
        if rng.random() < 0.14: continue
        x = (i - (n - 1) / 2) * st; t = a + u(-0.06, 0.06); w = W; v = 0.0
        if rng.random() < 0.15: w = W * u(0.5, 0.65); v = rng.choice((-1, 1)) * (W - w) / 2
        c = ff(x + u(-0.05, 0.05), v + u(-0.12, 0.12)); f2 = frame(c[0], c[1], t); s = o.up(rect(c[0], c[1], 0.96, w, t), u(0.72, 0.82))
        o.cut(U([barl(f2(vx, v0), f2(vx + u(-0.1, 0.1), v0 + w * u(0.25, 0.5)), 0.2) for vx, v0 in ((u(-0.25, 0.0), -w * u(0.2, 0.42)), (u(0.05, 0.28), -w * u(0.0, 0.1)))]), s, 0.16)
    return o.out()

@d("trederra", "made", "wood", "rifle")
def _(cx, cy, a, rng):
    """a rifle laid down in the mud: the butt widening to its plate, the wrist, the long fore-end with its bands,
    the breech and bolt with the knob turned down, the magazine, the muzzle and (on some) the fixed bayonet, the
    sling lying slack below it"""
    u = rng.uniform; ff = frame(cx, cy, a); L = u(8.0, 9.2); o = Obj("wood"); sv = rng.choice((-1, 1)); x0 = -L / 2; F = lambda x, v: ff(x, sv * v)
    stock = Polygon([F(x0, -0.2), F(x0, 0.95), F(x0 + 1.9, 0.5), F(x0 + 2.6, 0.38), F(x0 + 2.9, 0.62), F(x0 + L - 1.0, 0.56), F(x0 + L - 1.0, 0.0), F(x0 + 2.6, -0.06), F(x0 + 1.8, -0.22)]).buffer(0.06, 4)
    pts = [F(x0 + 0.9, 0.7), F(x0 + 2.2, u(1.3, 1.9)), F(x0 + 4.0, u(1.2, 1.8)), F(x0 + L * 0.62, 0.5)]; o.st(ext(line(pts, 0.5), 0.24), "leather", foot=LineString(pts).buffer(0.4))
    b = o.up(stock, 0.56)
    o.on(rect(*F(x0 + 3.5, 0.3), 1.9, 0.5, a), b, 0.16, "iron"); o.on(U([rect(*F(x, 0.28), 0.46, 0.7, a) for x in (x0 + L * 0.62, x0 + L - 1.5)]), b, 0.13, "iron")
    o.st(ext(rect(*F(x0 + 3.9, 0.85), 0.75, 0.7, a), 0.4) + ext(ann(*F(x0 + 3.0, 0.82), 0.42, 0.18, 8), 0.3) + cyl(*F(x0 + 3.2, -0.3), 0.3, 0.5, n=8), "iron", foot=rect(*F(x0 + 3.5, 0.3), 1.6, 2.0, a))
    o.st(rod([F(x0 + L - 1.2, 0.2), F(x0 + L - 0.1, 0.2)], 0.3, 1.1), "iron", foot=disc(*F(x0 + L - 0.6, 0.2), 0.6))
    if rng.random() < 0.6: p = F(x0 + L - 0.3, 0.52); q = F(x0 + L + u(1.6, 2.2), 0.52); o.st(ext(U([tri(p, q, 0.56), rect(*F(x0 + L - 0.55, 0.45), 0.6, 0.6, a)]), 0.3), "iron", foot=LineString([p, q]).buffer(0.4))
    return o.out()

@d("trederra", "made", "clay", "rum_jar")
def _(cx, cy, a, rng):
    """the rum ration: a stoneware gallon jar with its shoulder, short neck, lip and the loop handle at the neck
    (standing, or lying on its side), and the tin mugs set down by it"""
    u = rng.uniform; o = Obj("clay"); r = u(1.1, 1.3); ff = frame(cx, cy, a)
    if rng.random() < 0.6:
        h = u(1.7, 2.1); hp = pol(cx, cy, 0.75, a + 2.0)
        o.st(turn(cx, cy, [(r * 0.94, 0.0), (r, 0.2), (r, h * 0.6)] + crv(r, 0.48, h * 0.6, h * 0.85) + [(0.48, h * 0.95), (0.6, h * 0.96), (0.6, h + 0.12)], 24) + ext(capl(pol(cx, cy, 0.4, a + 2.0), pol(cx, cy, r * 0.9, a + 2.0), 0.56), h * 0.9),
             cuts=[(disc(cx, cy, 0.3, 8), h - 0.25)], foot=disc(cx, cy, r))
    else:
        L = u(3.0, 3.6)
        def rf(t):
            x = (t + L / 2) / L
            return r * (0.9 + 0.1 * min(1.0, x / 0.06)) if x < 0.55 else r * max(0.42, math.cos((x - 0.55) / 0.27 * 1.5708)) if x < 0.82 else (0.44 if x < 0.92 else 0.58)
        hp = ff(L * 0.3, r * 0.5)
        o.st(spin(cx, cy, a, L, rf, 1.0) + ext(disc(hp[0], hp[1], 0.6, 10), 0.6), foot=rect(cx, cy, L, 2 * r, a))
    for i in range(rng.randint(1, 3)):
        c = pol(cx, cy, r + u(1.3, 2.0), a + 3.4 + i * u(0.9, 1.2)); t = u(0, 6.28); k = o.up(U([disc(c[0], c[1], 0.64, 14), rect(*pol(c[0], c[1], 0.75, t), 0.7, 0.5, t)]), u(0.75, 0.95), "tin"); o.cut(disc(c[0], c[1], 0.4, 10), k, 0.5)
    return o.out()

@d("trederra", "made", "wood", "trench_sign")
def _(cx, cy, a, rng):
    """a trench signboard fallen with its stake: the pointed stake, the board nailed across its head (one end cut
    to a point on the kind that shows the way), the plank joint, the nails, lettering worn to scratches"""
    u = rng.uniform; ff = frame(cx, cy, a); o = Obj("wood"); Ls = u(5.0, 6.5); L = u(3.4, 4.2); W = u(1.5, 1.9)
    o.up(U([rect(*ff(-0.4, 0), Ls - 0.8, 0.92, a), tri(ff(Ls / 2 - 0.8, 0), ff(Ls / 2 + 0.5, 0), 0.92)]), 0.42)
    c = ff(-Ls / 2 + 1.1, 0); t = a + 1.5708 + u(-0.25, 0.25); f2 = frame(c[0], c[1], t); sv = rng.choice((-1, 1))
    g = Polygon([f2(-L / 2, -W / 2), f2(L / 2 - 0.7, -W / 2), f2(L / 2, 0), f2(L / 2 - 0.7, W / 2), f2(-L / 2, W / 2)]) if rng.random() < 0.6 else rect(c[0], c[1], L, W, t)
    if sv < 0: g = affinity.rotate(g, math.pi, origin=c, use_radians=True)
    b = o.up(g, 0.72); o.cut(barl(f2(-L, u(-0.15, 0.15)), f2(L, u(-0.15, 0.15)), 0.2).intersection(g), b, 0.15)
    o.studs([f2(0, v) for v in (-W * 0.28, W * 0.28)], b, 0.25, 0.13, "iron", 6)
    o.cut(U([barl(f2(x, v - 0.22), f2(x + rng.choice((-0.15, 0, 0.15)), v + 0.22), 0.2) for v in (-W * 0.25, W * 0.25) for x in np.arange(-L / 2 + 0.5, L / 2 - 0.8, 0.42) if abs(x) > 0.5 and rng.random() < 0.8]), b, 0.15)
    return o.out()


_OWN = set(globals())
