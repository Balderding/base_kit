"""Second drawing of the style-only details that came out as plain blocks (see style_details.py and Obj there).
A builder here with the name of one in sd_a.py to sd_e.py takes its place. The rule: a box is never just a box. It is
a particular kind (a cantilever toolbox, an ammunition tin, a tea chest) and carries what that kind has: a lid line,
latches, a handle, rivets, ribs, a label, and now and then it is open, dented or tipped over."""
import math
from sd_reg import d

# ====================================================================================================== corridor
@d("corridor", "made", "metal", "toolbox")
def _(cx, cy, a, rng):
    """a cantilever toolbox: two lid flaps meeting on the centre line under a carrying bar, latches at the ends, rivets
    along both long sides; or standing open with its trays showing. Spanners lie beside it."""
    u = rng.uniform; ff = frame(cx, cy, a); L = u(3.6, 4.4); W = u(2.0, 2.4); h = u(1.0, 1.3); o = Obj("metal"); g = rrect(cx, cy, L, W, a, 0.25)
    b = o.up(g, h)
    if rng.random() < 0.3:          # open: a rim, two trays with a divider
        o.cut(U([rect(*ff(-L * 0.24, 0), L * 0.42, W - 0.6, a), rect(*ff(L * 0.24, 0), L * 0.42, W - 0.6, a)]), b, 0.55)
    else:
        lid = o.lid(g, b, 0.18, 0.14); o.cut(rect(cx, cy, L - 0.5, 0.2, a), lid, 0.16)
        o.handle(ff(-L * 0.24, 0), ff(L * 0.24, 0), lid, 0.5, 0.45)
        o.tabs(ff, [-L / 2 + 0.42, L / 2 - 0.42], 0, lid, a, 0.46, 0.7, 0.16)
        o.studs([ff(x, sv * (W / 2 - 0.45)) for x in np.arange(-L / 2 + 0.9, L / 2 - 0.6, 0.9) for sv in (-1, 1)], lid, 0.25, 0.12)
    for i in range(rng.randint(1, 3)):          # spanners: a shaft with a ring at one end and an open jaw at the other
        c = ff(u(-L / 2, L / 2), -W / 2 - u(1.0, 1.9)); t = u(0, 3.14); p, q = pol(c[0], c[1], 1.0, t), pol(c[0], c[1], 1.0, t + math.pi)
        o.st(ext(U([capl(p, q, 0.5), disc(p[0], p[1], 0.48, 8), disc(q[0], q[1], 0.5, 8)]), 0.36), cuts=[(disc(p[0], p[1], 0.2, 6), 0.1), (barl(q, pol(q[0], q[1], 0.6, t + math.pi), 0.3), 0.1)])
    return o.out()

@d("corridor", "made", "metal", "terminal")
def _(cx, cy, a, rng):
    """a deck console: a desk with rows of keys and dials, a raised screen housing behind it with the screen set into
    it (cracked on some) and cooling slots down one side"""
    u = rng.uniform; ff = frame(cx, cy, a); L = u(3.2, 3.9); W = u(2.6, 3.2); h = u(0.7, 0.9); o = Obj("metal"); g = rrect(cx, cy, L, W, a, 0.2)
    b = o.up(g, h); xb = -L * 0.2
    hs = o.on(rect(*ff(xb - 0.1, 0), L * 0.5, W - 0.3, a), b, u(0.6, 1.0)); sc = rect(*ff(xb - 0.1, -W * 0.08), L * 0.5 - 0.6, W * 0.62, a)
    o.cut(sc, hs, 0.22)
    if rng.random() < 0.4:          # a cracked screen: lines running out from one point
        c = ff(xb + u(-0.4, 0.4), u(-0.5, 0.3)); o.cut(U([barl(c, pol(c[0], c[1], u(0.8, 1.6), u(0, 6.28)), 0.16) for _ in range(rng.randint(3, 5))]).intersection(sc.buffer(0.3)), hs, 0.36)
    o.vents(ff, xb - 0.1, W * 0.38, hs, 3, L * 0.34, a, 0.3, 0.16)
    keys = [ff(L * 0.2 + i * 0.52, v) for i in range(2) for v in np.arange(-W / 2 + 0.55, W * 0.12, 0.52)]
    o.on(U([rect(x, y, 0.46, 0.46, a) for x, y in keys]), b, 0.14)
    o.studs([ff(L * 0.32, W * 0.3)], b, 0.34, 0.2); o.studs([ff(L * 0.1, W * 0.36), ff(L * 0.42, W * 0.12)], b, 0.25, 0.14)
    return o.out()

@d("corridor", "made", "plate", "deck_hatch")
def _(cx, cy, a, rng):
    """a pressure hatch in its frame: bolts round the frame, the cover with its seam, a locking wheel of three or four
    spokes on a hub (or a lever on the square kind), two hinge blocks, tread grooves in the corners"""
    u = rng.uniform; ff = frame(cx, cy, a); S = u(3.8, 4.6); R = S * 0.36; o = Obj("plate"); g = rrect(cx, cy, S, S, a, 0.3)
    b = o.up(g, 0.4); rnd = rng.random() < 0.7
    cov = o.on(disc(cx, cy, R, 28) if rnd else rrect(cx, cy, S * 0.66, S * 0.66, a, 0.25), b, 0.3)
    o.studs([ff(sx * (S / 2 - 0.4), sy * (S / 2 - 0.4)) for sx in (-1, 1) for sy in (-1, 1)] + [ff(sx * (S / 2 - 0.35), 0) for sx in (-1, 1)] + [ff(0, S / 2 - 0.35)], b, 0.25, 0.13)
    o.tabs(ff, [-S * 0.18, S * 0.18], -S / 2 + 0.35, b, a, 0.7, 0.5, 0.36)
    if rnd:
        n = rng.choice((3, 4)); t0 = u(0, 6.28); rw = R * 0.62
        o.on(U([ann(cx, cy, rw, rw - 0.46, 20)] + [capl((cx, cy), pol(cx, cy, rw - 0.2, t0 + 6.2832 * k / n), 0.46) for k in range(n)]), cov, 0.3); o.on(disc(cx, cy, 0.5, 10), cov, 0.5)
    else:
        t0 = u(0, 6.28); o.on(capl(pol(cx, cy, 0.1, t0), pol(cx, cy, S * 0.24, t0), 0.5), cov, 0.3); o.on(disc(cx, cy, 0.5, 10), cov, 0.45)
        o.groove(rrect(cx, cy, S * 0.66 - 0.7, S * 0.66 - 0.7, a, 0.2), cov, 0.18, 0.12)
    return o.out()

@d("corridor", "made", "metal", "cell_rack")
def _(cx, cy, a, rng):
    """a charging rack of power cells: a base with end cheeks and a bus bar, each cell a banded can with a terminal cap;
    one socket stands empty and one cell may lie beside the rack"""
    u = rng.uniform; ff = frame(cx, cy, a); n = rng.randint(3, 5); L = n * 1.35 + 0.9; W = 2.0; o = Obj("metal"); miss = rng.randint(0, n)
    b = o.up(rrect(cx, cy, L, W, a, 0.15), 0.5)
    o.on(U([rect(*ff(sx * (L / 2 - 0.25), 0), 0.5, W, a) for sx in (-1, 1)]), b, 0.5); o.on(rect(*ff(0, W / 2 - 0.3), L - 1.2, 0.46, a), b, 0.22)
    for i in range(n):
        c = ff((i - (n - 1) / 2) * 1.35, -0.15)
        if i == miss: o.cut(disc(c[0], c[1], 0.45, 12), b, 0.3); continue
        h = u(1.3, 1.8); o.st(turn(c[0], c[1], [(0.56, 0.0), (0.56, 0.2), (0.5, 0.22), (0.5, h * 0.5), (0.56, h * 0.52), (0.56, h * 0.62), (0.5, h * 0.64), (0.5, h), (0.3, h + 0.04), (0.3, h + 0.3)], 14), par=b)
    if rng.random() < 0.5:
        c = ff(u(-L / 3, L / 3), -W / 2 - u(1.0, 1.6)); t = a + u(-0.6, 0.6); f2 = frame(c[0], c[1], t)
        o.st(lie(c[0], c[1], t, 1.7, 0.5) + lie(*f2(0.95, 0), t, 0.3, 0.3) + lie(*f2(-0.2, 0), t, 0.2, 0.56))
    return o.out()

@d("corridor", "made", "metal", "junction_box")
def _(cx, cy, a, rng):
    """a deck junction box: a screwed cover plate with a warning triangle, cable glands on two or three sides and the
    cables running off from them; on some the cover lies beside it and the inside shows"""
    u = rng.uniform; ff = frame(cx, cy, a); S = u(2.2, 2.7); h = u(0.9, 1.2); o = Obj("metal"); ks = rng.sample(range(4), rng.randint(2, 3))
    gl = [rect(*pol(cx, cy, S / 2 + 0.2, a + k * 1.5708), 0.7, 0.9, a + k * 1.5708) for k in ks]
    b = o.up(U([rect(cx, cy, S, S, a)] + gl), h * 0.7); bx = o.on(rect(cx, cy, S, S, a), b, h * 0.3)
    if rng.random() < 0.3:
        o.cut(rect(cx, cy, S - 0.7, S - 0.7, a), bx, 0.4)
        t2 = a + u(-0.6, 0.6); c = ff(u(-0.5, 0.5), -S / 2 - u(1.7, 2.3)); cg = rect(c[0], c[1], S - 0.3, S - 0.3, t2); c2 = o.up(cg, 0.3); f2 = frame(c[0], c[1], t2); q = S / 2 - 0.55
        o.studs([f2(sx * q, sy * q) for sx in (-1, 1) for sy in (-1, 1)], c2, 0.25, 0.12)
    else:
        lid = o.lid(rect(cx, cy, S, S, a), bx, 0.15, 0.18); o.studs([ff(sx * (S / 2 - 0.5), sy * (S / 2 - 0.5)) for sx in (-1, 1) for sy in (-1, 1)], lid, 0.25, 0.12)
        o.cut(U([barl(ff(-0.45, -0.35), ff(0, 0.5), 0.17), barl(ff(0.45, -0.35), ff(0, 0.5), 0.17), barl(ff(-0.45, -0.35), ff(0.45, -0.35), 0.17)]), lid, 0.12)
    for k in ks:          # each cable leaves its gland with a small gap, so cable and box never share an edge
        t = a + k * 1.5708; p0 = pol(cx, cy, S / 2 + 0.95, t); e = pol(cx, cy, S / 2 + u(2.4, 3.6), t + u(-0.35, 0.35)); o.st(rod(wiggle(p0, e, rng, 0.4, 3), 0.3, 1.1), "cable")
    return o.out()

@d("corridor", "made", "metal", "vent_fan")
def _(cx, cy, a, rng):
    """an extractor fan lying loose: a square mounting plate with bolt holes, the shroud ring, swept blades round a
    hub (one or two snapped off), and a guard bar across on some"""
    u = rng.uniform; R = u(1.8, 2.2); n = rng.randint(4, 6); S = 2 * R + 0.9; o = Obj("metal"); ff = frame(cx, cy, a)
    b = o.up(rrect(cx, cy, S, S, a, 0.4), 0.3); o.cut(U([disc(*ff(sx * (S / 2 - 0.4), sy * (S / 2 - 0.4)), 0.2, 6) for sx in (-1, 1) for sy in (-1, 1)]), b, 0.25)
    o.on(ann(cx, cy, R, R - 0.5, 28), b, 0.6); t0 = u(0, 6.28); bl = []
    for k in range(n):
        if rng.random() < 0.18: continue
        t = t0 + 6.2832 * k / n; bl.append(Polygon([pol(cx, cy, 0.35, t - 0.5), pol(cx, cy, R - 0.75, t - 0.1), pol(cx, cy, R - 0.7, t + 0.36), pol(cx, cy, R - 1.0, t + 0.5), pol(cx, cy, 0.35, t + 0.4)]))
    if bl: o.on(U(bl), b, 0.3)
    hub = o.on(disc(cx, cy, 0.62, 12), b, 0.5); o.cut(disc(cx, cy, 0.22, 6), hub, 0.2)
    if rng.random() < 0.4: t = u(0, 3.14); o.on(barl(pol(cx, cy, R - 0.3, t), pol(cx, cy, R - 0.3, t + math.pi), 0.46), b, 0.62)
    return o.out()

@d("corridor", "made", "metal", "hover_pallet")
def _(cx, cy, a, rng):
    """a grav pallet: a deck with tread grooves, a lifter pod at each corner with its ring, lashing eyes, a control
    stalk at one end; it may carry a strapped case or two"""
    u = rng.uniform; ff = frame(cx, cy, a); L = u(4.8, 5.8); W = u(3.2, 3.8); o = Obj("metal")
    b = o.up(rrect(cx, cy, L, W, a, 0.5), 0.5); pods = [ff(sx * (L / 2 - 0.7), sy * (W / 2 - 0.7)) for sx in (-1, 1) for sy in (-1, 1)]
    pd = o.on(U([disc(x, y, 0.56, 12) for x, y in pods]), b, 0.28); o.cut(U([ann(x, y, 0.36, 0.2, 8) for x, y in pods]), pd, 0.14)
    o.on(rect(*ff(-L / 2 + 0.5, 0), 0.6, 0.9, a), b, u(0.7, 1.1))
    if rng.random() < 0.55:
        for i in range(rng.randint(1, 2)):
            cl, cw = u(1.5, 2.0), u(1.3, 1.7); c = ff(u(-0.2, 0.3) + (i * 2.0 - 0.6), u(-0.3, 0.3)); t = a + u(-0.25, 0.25); f2 = frame(c[0], c[1], t); cg = rect(c[0], c[1], cl, cw, t)
            cs = o.on(cg, b, u(0.9, 1.4)); ld = o.lid(cg, cs, 0.14, 0.14); o.on(rect(c[0], c[1], 0.46, cw - 0.28, t), ld, 0.12); o.cut(U([barl(f2(-cl * 0.3, -cw), f2(-cl * 0.3, cw), 0.16), barl(f2(cl * 0.3, -cw), f2(cl * 0.3, cw), 0.16)]), ld, 0.1)
    else:
        o.cut(U([line([ff(x - 0.4, -W * 0.26), ff(x, 0), ff(x - 0.4, W * 0.26)], 0.2) for x in np.arange(-L / 2 + 1.7, L / 2 - 1.2, 0.6)]), b, 0.16)
    o.cut(U([ann(*ff(0, sy * (W / 2 - 0.4)), 0.3, 0.14, 8) for sy in (-1, 1)]), b, 0.14)
    return o.out()

@d("corridor", "made", "plate", "plate_stack")
def _(cx, cy, a, rng):
    """spare deck plates stacked a little askew: the top one shows its diamond tread, bolt holes in the corners and a
    lifting slot"""
    u = rng.uniform; L = u(3.4, 4.2); W = u(2.6, 3.2); o = Obj("plate"); n = rng.randint(2, 4); par = None; x, y, t = cx, cy, a
    for i in range(n):          # each plate lies wholly on the one below (a hair smaller, a little askew): edges never cross
        g = rect(x, y, L, W, t); par = o.up(g, 0.34) if par is None else o.on(g, par, 0.3)
        if i < n - 1: x += u(-0.12, 0.12); y += u(-0.12, 0.12); t += u(-0.05, 0.05); L -= 0.5; W -= 0.5
    ff = frame(x, y, t)
    o.cut(U([disc(*ff(sx * (L / 2 - 0.42), sy * (W / 2 - 0.42)), 0.2, 6) for sx in (-1, 1) for sy in (-1, 1)] + [capl(ff(-0.4, W / 2 - 0.45), ff(0.4, W / 2 - 0.45), 0.28)]), par, 0.2)
    dm = [ff(i * 0.62 + (j % 2) * 0.31, j * 0.5) for i in range(-4, 5) for j in range(-3, 4)]
    o.cut(U([ell(px, py, 0.26, 0.11, t + (0.6 if (k % 2) else -0.6), 6) for k, (px, py) in enumerate(dm)]).intersection(rect(x, y, L - 1.5, W - 1.5, t)), par, 0.12)
    return o.out()

@d("corridor", "made", "tin", "mess_tray")
def _(cx, cy, a, rng):
    """a pressed ration tray: a raised rim and dividers making one long and two short wells, food left in some, a
    beaker with its hollow and a spoon beside it"""
    u = rng.uniform; ff = frame(cx, cy, a); L = u(3.2, 3.8); W = u(2.3, 2.7); o = Obj("tin"); g = rrect(cx, cy, L, W, a, 0.35)
    b = o.up(g, 0.3); wells = [rrect(*ff(-L * 0.23, 0), L * 0.4, W - 0.7, a, 0.2), rrect(*ff(L * 0.22, W * 0.21), L * 0.38, W * 0.36, a, 0.15), rrect(*ff(L * 0.22, -W * 0.22), L * 0.38, W * 0.34, a, 0.15)]
    o.on(g.difference(U(wells)), b, 0.2)
    for wl in wells:
        if rng.random() < 0.6: c = wl.centroid; o.on(blob(c.x + u(-0.15, 0.15), c.y + u(-0.15, 0.15), u(0.34, 0.42), rng, 3).intersection(inset(wl, 0.08)), b, u(0.16, 0.3), "moss")
    c = ff(L / 2 + u(1.0, 1.5), u(-0.8, 0.8)); cup = o.up(disc(c[0], c[1], 0.6, 14), u(0.8, 1.0)); o.cut(disc(c[0], c[1], 0.36, 10), cup, 0.45)
    p = ff(u(-0.6, 0.6), -W / 2 - u(0.8, 1.2)); t = a + u(-0.5, 0.5); o.st(ext(U([capl(p, pol(p[0], p[1], 1.5, t), 0.46), ell(*pol(p[0], p[1], 1.7, t), 0.5, 0.34, t, 8)]), 0.3), cuts=[(ell(*pol(p[0], p[1], 1.7, t), 0.3, 0.18, t, 6), 0.14)])
    return o.out()

@d("corridor", "made", "metal", "magboots")
def _(cx, cy, a, rng):
    """a pair of magnetic boots left where they were stepped out of: thick sole plates, the upper, two buckled straps,
    the open shaft"""
    u = rng.uniform; o = Obj("metal")
    for sv in (-1, 1):
        c = pol(cx, cy, u(0.95, 1.4), a + sv * 1.57); t = a + u(-0.7, 0.7); f2 = frame(c[0], c[1], t); sole = rrect(c[0], c[1], 2.8, 1.3, t, 0.45)
        b = o.up(sole, 0.4); up_ = o.on(rrect(*f2(0.05, 0), 2.5, 1.06, t, 0.4), b, 0.4)
        o.on(U([rect(*f2(x, 0), 0.46, 1.06, t) for x in (0.3, 0.95)]), up_, 0.14)
        sh = o.on(disc(*f2(-0.7, 0), 0.54, 14), up_, u(0.7, 1.2)); o.cut(disc(*f2(-0.7, 0), 0.32, 10), sh, 0.5)
    return o.out()

@d("corridor", "made", "cable", "hose_reel")
def _(cx, cy, a, rng):
    """a hose reel lying flat: the flange, the hose wound round the hub in rings, a crank on the hub, and the free end
    running off to a nozzle"""
    u = rng.uniform; R = u(1.9, 2.3); o = Obj("metal"); t0 = u(0, 6.28)
    b = o.up(disc(cx, cy, R, 28), 0.34); k = 0; r = R - 0.3
    while r > 1.1:
        o.on(ann(cx, cy, r, r - 0.46, 24), b, 0.42 if k % 2 == 0 else 0.3, "cable"); r -= 0.48; k += 1
    hub = o.on(disc(cx, cy, 0.62, 12), b, 0.8); o.on(capl((cx, cy), pol(cx, cy, 0.5, t0), 0.46), hub, 0.16)
    e = pol(cx, cy, R + u(2.2, 3.6), a + u(-0.4, 0.4)); pts = wiggle(pol(cx, cy, R + 0.3, a), e, rng, 0.6, 4)
    o.st(rod(pts, 0.3, 1.1) + cone(e[0], e[1], 0.42, 0.6, 0.26), "cable")
    return o.out()


_OWN = set(globals())
