"""Ferrum (a red, rusted factory world): its style-only details, drawn at their true size beside a 32 mm figure
(1 m = 6 mm as laid out, 18 mm printed at figure scale). Hard-edged things (plates, boxes, girders, gears) are built
in flat layers (Obj) so their rivets, slots and teeth stay crisp; round things (pipes, cables, slag) go through ST().
Rounded pieces that would overlap are drawn as one solid (two overlapping rounded pieces can tear the mesh), red
dust banked against a thing is a plain mound beside it. Nothing overhangs: a gear standing on its edge is its own
side outline carried straight through its thickness."""
import math
from sd_reg import d

XF_SOFT = ((0.55, 25), (0.45, 50))


def xf_gear(cx, cy, R, n, a, td=0.6, tw=None):
    """a gear wheel's outline: the disc and n teeth td deep round it"""
    tw = tw or max(0.5, min(0.85, 3.1416 * R / n))
    teeth = [rect(*pol(cx, cy, R - td * 0.45, a + 6.2832 * k / n), td * 1.1, tw, a + 6.2832 * k / n) for k in range(n)]
    return U([disc(cx, cy, R - td * 0.75, max(24, 3 * n))] + teeth)


def xf_edge(cx, cy, a, prof, T):
    """an upright plate standing on its edge along `a`: prof is its side outline in (along, up) millimetres, z = 0 the
    ground (what lies below it is buried); seen from above it is T thick. One solid for ST()."""
    ff = frame(cx, cy, a); x0, _, x1, top = prof.bounds
    def sect(z):
        if z >= top: return None
        band = prof.intersection(box(x0 - 1, z, x1 + 1, z + 0.002)); out = []
        for q in polys_of(band):
            b0, _, b1, _ = q.bounds
            if b1 - b0 >= 0.25: out.append(Polygon([ff(b0, -T / 2), ff(b1, -T / 2), ff(b1, T / 2), ff(b0, T / 2)]))
        return U(out) if out else None
    return [(sect, top)]


def xf_dust(o, x, y, r, h, rng, sq=0.7, a=0.0):
    """red dust banked against something: a low soft mound (a plain mound, so it may touch the thing)"""
    g = ell(x, y, r, r * sq, a, 16).buffer(0.0)
    g = U([g, blob(*pol(x, y, r * 0.5, a + rng.uniform(0, 6.28)), r * 0.55, rng, 4)])
    o.L.append(("mound", g, h, "dust", XF_SOFT)); o.F.append(g)


def xf_chain(pts, w=0.38, step=0.62):
    """chain links along a line: flat oval rings and edge-on bars in turn. Returns solids for ST()"""
    ln = LineString(pts); out = []; L = ln.length; k = 0; s = 0.0
    while s + step <= L + 1e-6:
        p = ln.interpolate(s); q = ln.interpolate(min(L, s + step)); t = math.atan2(q.y - p.y, q.x - p.x); m = ((p.x + q.x) / 2, (p.y + q.y) / 2)
        if k % 2 == 0:
            g = ell(m[0], m[1], step * 0.85, 0.42, t, 14).difference(ell(m[0], m[1], step * 0.85 - w, 0.42 - w * 0.75, t, 12))
            out += ext(g, 0.3)
        else: out += ext(barl(pol(m[0], m[1], -step * 0.75, t), pol(m[0], m[1], step * 0.75, t), w), 0.52)
        s += step; k += 1
    return out


# ====================================================================================================== gears
@d("ferrum", "made", "iron", "half_gear")
def _(cx, cy, a, rng):
    """a great gear wheel lying flat and half drowned in the red dust: the toothed rim, the web with its lightening
    holes, the raised boss with its keyway, a tooth or two snapped off; the dust banked over one side of it"""
    u = rng.uniform; R = u(2.6, 3.3); n = rng.randint(14, 18); o = Obj("iron")
    g = xf_gear(cx, cy, R, n, a)
    for _k in range(rng.randint(0, 2)): g = g.difference(disc(*pol(cx, cy, R + 0.1, a + 6.2832 * (rng.randrange(n) + 0.0) / n), 0.5, 8))
    b = o.up(g, u(0.55, 0.7))
    o.cut(ann(cx, cy, R - 0.8, R - 1.12, 28), b, 0.18)
    nh = rng.choice((4, 5, 6)); t0 = u(0, 6.28)
    o.cut(U([disc(*pol(cx, cy, R * 0.55, t0 + 6.2832 * k / nh), R * 0.16, 12) for k in range(nh)]), b, 0.35)
    hb = o.on(disc(cx, cy, R * 0.3 + 0.25, 18), b, 0.35)
    o.cut(U([disc(cx, cy, 0.38, 12), rect(*pol(cx, cy, 0.42, t0), 0.3, 0.3, t0)]), hb, 0.3)
    t = a + u(0, 6.28); c = pol(cx, cy, R * u(0.55, 0.8), t)
    xf_dust(o, c[0], c[1], R * u(0.85, 1.05), u(1.0, 1.3), rng, 0.6, t + 1.5708)
    return o.out()

@d("ferrum", "made", "iron", "gear_on_edge")
def _(cx, cy, a, rng):
    """a gear wheel stood on its edge and sunk to a third of its height in the dust, as it rolled off its shaft and
    fell: the teeth round the top, the boss standing out of both faces, dust heaped up against both sides"""
    u = rng.uniform; R = u(1.8, 2.3); n = rng.randint(12, 16); T = u(0.8, 0.95); zc = R * u(0.15, 0.35); o = Obj("iron"); ff = frame(cx, cy, a)
    prof = xf_gear(0.0, zc, R, n, u(0, 1), 0.6)
    o.st(xf_edge(cx, cy, a, prof, T) + xf_edge(cx, cy, a, disc(0.0, zc, 0.8, 16), T + 0.8), foot=rect(cx, cy, 2 * R + 0.4, T + 0.9, a))
    for sv in (-1, 1):
        rr = R * u(0.6, 0.8); c = ff(u(-R * 0.3, R * 0.3), sv * (T / 2 + rr * 0.38)); xf_dust(o, c[0], c[1], rr, u(0.6, 0.85), rng, 0.45, a)
    return o.out()

@d("ferrum", "made", "iron", "gear_pair")
def _(cx, cy, a, rng):
    """two gear wheels still in mesh on their bracket plate, torn out of a machine: the plate with its bolt holes,
    a big wheel and its pinion with their teeth meshed, the axle caps on them"""
    u = rng.uniform; ff = frame(cx, cy, a); R1 = u(1.5, 1.8); R2 = u(0.9, 1.15); o = Obj("iron"); n1 = rng.randint(13, 16); n2 = max(8, round(n1 * R2 / R1))
    c1 = ff(-R2 * 0.55, 0); c2 = ff(R1 + R2 - 0.5 - R2 * 0.55, 0)
    pl = hull([disc(c1[0], c1[1], R1 + 0.35, 20), disc(c2[0], c2[1], R2 + 0.35, 16)]); b = o.up(pl, 0.35)
    o.cut(U([disc(*ff(-R2 * 0.55 - R1 - 0.05, sv * 0.5), 0.2, 8) for sv in (-1, 1)]), b, 0.2)
    g1 = o.on(xf_gear(c1[0], c1[1], R1, n1, a, 0.5), b, 0.4); g2 = o.on(xf_gear(c2[0], c2[1], R2, n2, a + 3.1416 / n2, 0.5), b, 0.4)
    for c, g, r in ((c1, g1, R1), (c2, g2, R2)):
        k = o.on(disc(c[0], c[1], r * 0.32 + 0.15, 12), g, 0.3); o.cut(ngon(c[0], c[1], 0.3, 6, a), k, 0.15)
    return o.out()


# ====================================================================================================== cables and pipes
@d("ferrum", "made", "cable", "cable_bundle")
def _(cx, cy, a, rng):
    """a bundle of heavy power cables lying where they were dragged: three to five lines side by side, sagging apart
    between the clamp bands that still hold them, the cut end splayed"""
    u = rng.uniform; ff = frame(cx, cy, a); L = u(7.0, 9.5); k = rng.randint(3, 5); o = Obj("cable"); r = u(0.3, 0.36)
    A = u(0.5, 1.1); fq = u(0.5, 0.9) * 6.2832 / L; ph0 = u(0, 6.28)
    ln = LineString([ff(x, A * math.sin(fq * x + ph0)) for x in np.linspace(-L / 2, L / 2, 41)]); m = 40; P = [ln.interpolate(i / m, normalized=True) for i in range(m + 1)]
    def nrm(i):
        p, q = P[max(0, i - 1)], P[min(m, i + 1)]; dx, dy = q.x - p.x, q.y - p.y; l = math.hypot(dx, dy) or 1; return -dy / l, dx / l
    clamps = sorted(rng.sample(range(7, m - 6), rng.randint(2, 3))); sols = []
    for j in range(k):
        off0 = (j - (k - 1) / 2) * 2 * r * 1.08; ph = u(0, 6.28); pts = []
        for i in range(m + 1):
            dm = min(abs(i - c) for c in clamps); sp = 1.0 + (0.3 * min(1.0, dm / 5) * (0.6 + 0.4 * math.sin(i * 0.45 + ph)) if i < m - 2 else 1.3 + 0.25 * (i - m + 2))
            nx, ny = nrm(i); pts.append((P[i].x + nx * off0 * sp, P[i].y + ny * off0 * sp))
        sols += rod(pts, r, 1.5)
    for c in clamps:
        nx, ny = nrm(c); t = math.atan2(ny, nx); sols += ext(rect(P[c].x, P[c].y, k * 2 * r * 1.08 + 0.55, 0.6, t), 2 * r * 1.5 * 0.5 + 0.22)
    o.st(sols, foot=ln.buffer(k * r * 1.6 + 0.4))
    return o.out()

@d("ferrum", "made", "cable", "cable_drum")
def _(cx, cy, a, rng):
    """a cable drum fallen on its side: the two big flanges with their bolt rings, the wound cable showing between
    them in ridges, the axle hole, and the free end running off across the dust"""
    u = rng.uniform; ff = frame(cx, cy, a); Rf = u(1.6, 1.9); Wd = u(2.0, 2.4); o = Obj("cable")
    sols = []
    for sx in (-1, 1): sols += lie(*ff(sx * (Wd / 2 - 0.2), 0), a, 0.4, Rf)
    for i in range(int((Wd - 0.8) / 0.42)): sols += lie(*ff(-Wd / 2 + 0.6 + i * 0.42, 0), a, 0.36, Rf * 0.72)
    e0 = ff(Wd * 0.1, Rf * 0.6); e = ff(u(-1, 2), Rf + u(2.5, 3.5)); pts = wiggle(e0, e, rng, 0.6, 4)
    sols += rod(pts, 0.3, 1.5)
    o.st(sols, foot=U([rect(cx, cy, Wd, 2 * Rf, a), LineString(pts).buffer(0.4)]))
    return o.out()

@d("ferrum", "made", "pipe", "pipe_run")
def _(cx, cy, a, rng):
    """a run of pipe lying on its saddles: one pipe or two side by side, a bolted flange joint along it, and on some
    a valve block standing on it with its hand wheel; the saddles stand out either side"""
    u = rng.uniform; ff = frame(cx, cy, a); L = u(7.0, 9.0); r = u(0.55, 0.7); o = Obj("pipe"); two = rng.random() < 0.5
    vs = [-(r + 0.12), r + 0.12] if two else [0.0]; Wt = (4 * r + 0.24 if two else 2 * r)
    sols = []
    for x in (-L * 0.32, L * 0.32): sols += ext(rect(*ff(x, 0), 0.8, Wt + 0.8, a), 2 * r * 0.85)
    xf = u(-L * 0.15, L * 0.15)
    for v in vs:
        sols += lie(*ff(0, v), a, L, r)
        sols += lie(*ff(xf, v), a, 0.45, r + 0.22)
    o.st(sols, foot=rect(cx, cy, L, Wt + 0.8, a))
    if rng.random() < 0.55:
        c = ff(xf + rng.choice((-1, 1)) * u(1.4, 2.2), vs[0]); vb = o.up(rect(c[0], c[1], 1.25, 1.25, a), 2 * r * 1.15 + 0.45)
        o.on(U([ann(c[0], c[1], 0.6, 0.38, 16)] + [barl(c, pol(c[0], c[1], 0.5, a + 0.8 + 1.5708 * k), 0.28) for k in range(4)]), vb, 0.28); o.studs([c], vb, 0.22, 0.45)
    return o.out()

@d("ferrum", "made", "pipe", "pipe_elbow")
def _(cx, cy, a, rng):
    """a length of broken pipe with its elbow: the flange at one end with its bolt heads, the bend, the other end torn
    off ragged with shards of it lying by"""
    u = rng.uniform; ff = frame(cx, cy, a); r = u(0.6, 0.75); L1 = u(3.0, 4.0); L2 = u(1.8, 2.8); rb = 1.3; o = Obj("pipe")
    pts = [ff(-L1, 0)] + [ff(-rb + rb * math.sin(t), rb - rb * math.cos(t)) for t in np.linspace(0, 1.5708, 7)] + [ff(0, L2)]
    sols = rod(pts, r, 1.6) + rod([ff(-L1 - 0.05, 0), ff(-L1 + 0.4, 0)], r + 0.3, 1.6 * r / (r + 0.3))
    o.st(sols, foot=LineString(pts).buffer(r + 0.35))
    for i in range(rng.randint(2, 3)):
        c = ff(u(-1.0, 1.2), L2 + u(0.6, 1.4)); t = u(0, 6.28); o.st(ext(Polygon([pol(c[0], c[1], 0.55, t), pol(c[0], c[1], 0.5, t + 2.3), pol(c[0], c[1], 0.45, t + 4.0)]), u(0.25, 0.4)), "rust")
    return o.out()

@d("ferrum", "made", "iron", "piston_ram")
def _(cx, cy, a, rng):
    """a hydraulic ram fallen from a machine: the fat cylinder with its end caps, the polished rod run out of it, a
    clevis eye at each end with its pin, a torn hose looping off the cylinder"""
    u = rng.uniform; ff = frame(cx, cy, a); Lc = u(3.0, 3.8); Lr = u(1.8, 2.6); R = u(0.6, 0.72); o = Obj("iron")
    x0 = -(Lc + Lr) / 2
    sols = lie(*ff(x0 + Lc / 2, 0), a, Lc, R) + lie(*ff(x0 + 0.2, 0), a, 0.4, R + 0.12) + lie(*ff(x0 + Lc - 0.2, 0), a, 0.4, R + 0.12) + lie(*ff(x0 + Lc + Lr / 2, 0), a, Lr, 0.3)
    hs = [ff(x0 + Lc * 0.7, R * 0.7), ff(x0 + Lc * 0.5, R + 1.2), ff(x0 + Lc * u(0.1, 0.3), R + u(1.6, 2.2))]
    sols += rod(hs, 0.24, 1.5)
    o.st(sols, foot=rect(cx, cy, Lc + Lr, 2 * R + 0.3, a))
    for x, sx in ((x0 - 0.45, -1), (x0 + Lc + Lr + 0.4, 1)):
        c = ff(x, 0); e = o.st(ext(U([disc(c[0], c[1], 0.62, 14), rect(*ff(x - sx * 0.45, 0), 0.6, 0.8, a)]), 0.75)); o.cut(disc(c[0], c[1], 0.26, 10), e, 0.3)
    return o.out(extra=[LineString(hs).buffer(0.35)])


# ====================================================================================================== stacks and boxes
@d("ferrum", "made", "iron", "vent_stack")
def _(cx, cy, a, rng):
    """a vent stack standing over a man's head: the square foot plate with its bolts, the flange, the round stack in
    two sections, its sooted open mouth; a louvred blower box at its foot and on some a smaller second stack"""
    u = rng.uniform; ff = frame(cx, cy, a); R = u(0.95, 1.2); Hh = u(3.8, 4.8); o = Obj("iron"); S = 2 * R + 1.3
    b = o.up(rect(cx, cy, S, S, a), 0.4)
    o.studs([ff(sx * (S / 2 - 0.35), sy * (S / 2 - 0.35)) for sx in (-1, 1) for sy in (-1, 1)], b, 0.24, 0.15)
    fl = o.on(disc(cx, cy, R + 0.35, 24), b, 0.3); s1 = o.on(disc(cx, cy, R, 24), fl, Hh * 0.55)
    s2 = o.on(disc(cx, cy, R - 0.14, 24), s1, Hh * 0.4); o.cut(disc(cx, cy, R - 0.5, 20), s2, 0.8)
    c = ff(S / 2 + 0.85, u(-0.3, 0.3)); bx = o.up(rect(c[0], c[1], 1.6, 1.9, a), u(1.4, 1.8)); o.vents(ff, S / 2 + 0.85, 0, bx, 3, 1.1, a, 0.42, 0.2, 0.25)
    if rng.random() < 0.45:
        c2 = ff(-S / 2 - 0.9, u(-0.5, 0.5)); r2 = u(0.55, 0.7); p2 = o.up(disc(c2[0], c2[1], r2 + 0.2, 16), 0.3); t2 = o.on(disc(c2[0], c2[1], r2, 16), p2, Hh * u(0.45, 0.6)); o.cut(disc(c2[0], c2[1], r2 - 0.3, 12), t2, 0.5)
    return o.out()

@d("ferrum", "made", "iron", "blower_hood")
def _(cx, cy, a, rng):
    """a blower housing set on the deck: the boxed body with its riveted seams and louvres, the round fan grille on
    top with its hub and guard bars, the outlet duct stub at one end"""
    u = rng.uniform; ff = frame(cx, cy, a); L = u(3.0, 3.6); W = u(2.4, 2.8); o = Obj("iron"); g = rrect(cx, cy, L, W, a, 0.25)
    b = o.up(g, u(1.4, 1.7)); lid = o.lid(g, b, 0.15, 0.16)
    R = min(L, W) * 0.36; c = ff(-L * 0.08, 0); fr = o.on(disc(c[0], c[1], R + 0.2, 24), lid, 0.18)
    o.cut(disc(c[0], c[1], R - 0.05, 24).difference(U([disc(c[0], c[1], 0.35, 10)] + [barl(pol(c[0], c[1], -R, a + k * 1.0472), pol(c[0], c[1], R, a + k * 1.0472), 0.24) for k in range(3)])), fr, 0.3)
    o.vents(ff, L * 0.36, 0, lid, 4, 0.7, a + 1.5708, 0.36, 0.18, 0.2)
    o.studs([ff(sx * (L / 2 - 0.35), sy * (W / 2 - 0.35)) for sx in (-1, 1) for sy in (-1, 1)], lid, 0.2, 0.12)
    du = o.up(rect(*ff(L / 2 + 0.55, 0), 1.1, W * 0.5, a), u(0.9, 1.1)); o.cut(rect(*ff(L / 2 + 0.75, 0), 0.6, W * 0.5 - 0.5, a), du, 0.5)
    return o.out()

@d("ferrum", "made", "iron", "gauge_panel")
def _(cx, cy, a, rng):
    """a control cabinet torn off its mounting and fallen on its back: the box with its door seam, two round gauges
    with their needles, a row of switches, a big lever in its slot, and the cable stub hanging out of the bottom"""
    u = rng.uniform; ff = frame(cx, cy, a); L = u(3.2, 3.8); W = u(2.0, 2.4); o = Obj("iron"); g = rrect(cx, cy, L, W, a, 0.2)
    b = o.up(g, u(1.0, 1.2)); o.groove(rrect(cx, cy, L - 0.6, W - 0.6, a, 0.15), b, 0.26, 0.14)
    for x in (-L * 0.22, L * 0.08):
        c = ff(x, W * 0.12); gz = o.on(disc(c[0], c[1], 0.62, 18), b, 0.2); o.cut(disc(c[0], c[1], 0.45, 16), gz, 0.15); t = u(0.4, 2.6)
        o.on(barl(c, pol(c[0], c[1], 0.38, a + t), 0.16), gz, 0.05)
    o.studs([ff(-L * 0.3 + i * 0.4, -W * 0.28) for i in range(rng.randint(3, 4))], b, 0.17, 0.22)
    o.cut(rect(*ff(L * 0.33, 0), 0.32, W * 0.55, a), b, 0.25); o.on(rect(*ff(L * 0.33, u(-0.4, 0.4)), 0.5, 0.5, a), b, 0.55)
    st = [ff(-L / 2, 0), ff(-L / 2 - 0.8, u(-0.6, 0.6)), ff(-L / 2 - u(1.4, 2.0), u(-1.0, 1.0))]; o.st(rod(st, 0.3, 1.5), "cable", foot=LineString(st).buffer(0.4))
    return o.out()


# ====================================================================================================== the broken machines
@d("ferrum", "made", "iron", "walker_foot")
def _(cx, cy, a, rng):
    """the foot of a broken work-walker: the heel plate and three splayed toes with their armour seams and rivets,
    the ball of the ankle on it, the shin stub snapped off above it, a hydraulic line torn out behind"""
    u = rng.uniform; ff = frame(cx, cy, a); s = u(1.0, 1.2); o = Obj("iron"); hc = ff(-0.6 * s, 0)
    heel = rrect(hc[0], hc[1], 2.4 * s, 2.3 * s, a, 0.35)
    toes = [hull([disc(*pol(hc[0], hc[1], 0.6 * s, a + t), 0.62 * s, 12), disc(*pol(hc[0], hc[1], 3.2 * s, a + t), 0.36 * s, 10)]) for t in (-0.62, 0.0, 0.62)]
    b = o.up(U([heel] + toes), u(0.6, 0.75))
    o.cut(U([barl(pol(hc[0], hc[1], 1.4 * s, a + t - 0.35), pol(hc[0], hc[1], 1.4 * s, a + t + 0.35), 0.16) for t in (-0.62, 0.0, 0.62)]).buffer(0.06), b, 0.14)
    o.studs([pol(hc[0], hc[1], r_ * s, a + t) for t in (-0.62, 0.0, 0.62) for r_ in (2.1, 2.6)], b, 0.18, 0.12)
    an = o.on(disc(hc[0], hc[1], 0.95 * s, 18), b, 0.7); sh = o.on(rect(hc[0], hc[1], 1.15 * s, 1.3 * s, a + u(-0.3, 0.3)), an, u(1.2, 2.0))
    o.cut(Polygon([pol(hc[0], hc[1], 1.0, a + 0.3), pol(hc[0], hc[1], 0.2, a + 1.9), pol(hc[0], hc[1], 1.0, a + 3.4)]), sh, u(0.3, 0.6))
    hs = [ff(-1.4 * s, 0.5), ff(-2.4 * s, u(0.8, 1.6)), ff(-u(3.0, 3.8) * s, u(-0.5, 1.5))]; o.st(rod(hs, 0.26, 1.5), "cable", foot=LineString(hs).buffer(0.35))
    return o.out()

@d("ferrum", "made", "iron", "walker_leg")
def _(cx, cy, a, rng):
    """the whole leg of a fallen work-walker lying in the dust: the torn hip socket with cables spilling out of it,
    the armoured thigh with its ram along it, the knee joint, the shin and the flat foot plate, bent at the knee"""
    u = rng.uniform; s = u(0.9, 1.05); o = Obj("iron"); kb = u(-0.5, 0.5)
    hip = pol(cx, cy, -4.2 * s, a); knee = (cx, cy); ank = pol(cx, cy, 3.8 * s, a + kb); ft = pol(cx, cy, 4.9 * s, a + kb)
    th = o.up(barl(hip, knee, 1.7 * s), u(1.0, 1.15)); o.cut(U([barl(pol(*pol(hip[0], hip[1], 1.1, a), sv * 0.55 * s, a + 1.5708), pol(*pol(cx, cy, -0.9, a), sv * 0.55 * s, a + 1.5708), 0.26) for sv in (-1, 1)]), th, 0.14)
    o.on(barl(pol(hip[0], hip[1], 0.9, a), pol(cx, cy, -1.0, a), 0.5), th, 0.3)
    sn = o.up(barl(knee, ank, 1.35 * s), u(0.85, 0.95)); o.studs([pol(cx, cy, d_ * s, a + kb) for d_ in (1.2, 2.0, 2.8)], sn, 0.2, 0.12)
    kn = o.up(disc(cx, cy, 1.05 * s, 18), u(1.35, 1.55)); o.cut(ann(cx, cy, 0.7 * s, 0.45 * s, 16), kn, 0.2)
    o.up(rrect(*pol(ft[0], ft[1], 0.0, 0.0), 1.6 * s, 2.1 * s, a + kb, 0.3), 0.55)
    hp = o.up(disc(hip[0], hip[1], 1.25 * s, 18), u(1.2, 1.4)); o.cut(disc(hip[0], hip[1], 0.8 * s, 14), hp, 0.5)
    cs = []
    for k in range(rng.randint(2, 3)):
        t = a + 3.1416 + u(-0.9, 0.9); e = pol(hip[0], hip[1], u(2.2, 3.2), t); cs.append(wiggle(pol(hip[0], hip[1], 1.2, t), e, rng, 0.4, 3))
    o.st([x for p in cs for x in rod(p, 0.24, 1.5)], "cable", foot=U([LineString(p).buffer(0.32) for p in cs]))
    return o.out()

@d("ferrum", "made", "iron", "crane_hook")
def _(cx, cy, a, rng):
    """a crane's hook block dropped with its chain still on it: the block with its two sheave slots and pin caps, the
    hook curling out of its foot, the heavy chain running off in a loose line"""
    u = rng.uniform; ff = frame(cx, cy, a); L = u(2.0, 2.4); W = u(1.5, 1.8); o = Obj("iron"); g = rrect(cx, cy, L, W, a, 0.3)
    b = o.up(g, u(1.0, 1.2)); o.cut(U([rect(*ff(-0.15, sv * 0.35), L * 0.6, 0.24, a) for sv in (-1, 1)]), b, 0.3); o.studs([ff(-0.15, 0)], b, 0.3, 0.2)
    hc = ff(L / 2 + 0.95, 0.15); pts = [ff(L / 2, 0)] + arc(hc[0], hc[1], 0.9, a + 3.1416, a + 3.1416 + 4.6, 10)
    sols = rod(pts, lambda q: 0.36 - 0.12 * q, 1.5)
    e = ff(-L / 2 - u(3.5, 5.0), u(-2.0, 2.0)); cp = wiggle(ff(-L / 2 - 0.1, 0), e, rng, 0.8, 4)
    o.st(sols, foot=LineString(pts).buffer(0.45)); o.st(xf_chain(cp), foot=LineString(cp).buffer(0.5))
    return o.out()

@d("ferrum", "made", "girder", "crane_jib")
def _(cx, cy, a, rng):
    """a length of lattice crane jib lying broken: the two chords with the zigzag lacing between them, gusset plates at
    the joints, the end bent away where it snapped"""
    u = rng.uniform; L = u(7.5, 9.5); W = u(1.9, 2.3); o = Obj("girder"); kink = u(-0.35, 0.35); xk = L * u(0.15, 0.3)
    f1 = frame(cx, cy, a); pk = f1(xk, 0); f2 = frame(pk[0], pk[1], a + kink)
    def seg(ff, x0, x1):
        gs = [barl(ff(x0, sv * W / 2), ff(x1, sv * W / 2), 0.6) for sv in (-1, 1)]
        n = max(1, int((x1 - x0) / 1.4)); st = (x1 - x0) / n
        gs += [barl(ff(x0 + i * st, (-1) ** i * W / 2), ff(x0 + (i + 1) * st, (-1) ** (i + 1) * W / 2), 0.45) for i in range(n)]
        return U(gs)
    body = U([seg(f1, -L / 2, xk), seg(f2, 0, L / 2 - xk)])
    o.st(ext(body, 0.6), foot=body)
    gus = U([disc(*f1(x, sv * W / 2), 0.42, 10) for x in np.arange(-L / 2 + 1.4, xk, 2.8) for sv in (-1, 1)])
    o.st(ext(gus, 0.8), foot=gus)
    return o.out()

@d("ferrum", "made", "girder", "girder_stub")
def _(cx, cy, a, rng):
    """an I-beam lying on its side: the two flanges standing up along it, the web between them with its rivet rows,
    one end bent where the building came down on it"""
    u = rng.uniform; L = u(5.0, 6.5); W = u(1.6, 1.9); o = Obj("girder"); kink = rng.choice((-1, 1)) * u(0.15, 0.45); xk = L * u(0.1, 0.3)
    f1 = frame(cx, cy, a); pk = f1(xk, 0); f2 = frame(pk[0], pk[1], a + kink)
    web = U([rect(*f1((xk - L / 2) / 2, 0), xk + L / 2, W, a), rect(*f2((L / 2 - xk) / 2, 0), L / 2 - xk, W, a + kink)])
    b = o.up(web, 0.45)
    fl = U([barl(f1(-L / 2, sv * (W / 2 - 0.25)), f1(xk + 0.1, sv * (W / 2 - 0.25)), 0.5) for sv in (-1, 1)] + [barl(f2(-0.1, sv * (W / 2 - 0.25)), f2(L / 2 - xk, sv * (W / 2 - 0.25)), 0.5) for sv in (-1, 1)])
    o.st(ext(fl, u(1.0, 1.2)), foot=fl)
    o.studs([f1(x, 0) for x in np.arange(-L / 2 + 0.5, xk - 0.3, 0.6)] + [f2(x, 0) for x in np.arange(0.4, L / 2 - xk - 0.3, 0.6)], b, 0.2, 0.14)
    return o.out()


# ====================================================================================================== small iron
@d("ferrum", "made", "iron", "bolt_head")
def _(cx, cy, a, rng):
    """a foundation bolt the size of a man's arm standing out of the deck where its machine was torn away: the square
    washer plate, the hex nut on it, the threaded end above; a second nut lying by on some"""
    u = rng.uniform; ff = frame(cx, cy, a); o = Obj("iron")
    w = o.up(rect(cx, cy, 2.0, 2.0, a + u(-0.3, 0.3)), 0.3); nt = o.on(ngon(cx, cy, 0.85, 6, a), w, 0.6); th = o.on(disc(cx, cy, 0.45, 12), nt, u(0.6, 1.0))
    o.cut(barl(pol(cx, cy, 0.5, a + 0.5), pol(cx, cy, -0.5, a + 0.5), 0.26), th, 0.12)
    if rng.random() < 0.6:
        c = ff(u(1.8, 2.4), u(-1.0, 1.0)); n2 = o.st(ext(ngon(c[0], c[1], 0.75, 6, u(0, 1)), 0.5), cuts=[(disc(c[0], c[1], 0.32, 10), 0.15)])
    return o.out()

@d("ferrum", "made", "iron", "valve_wheel")
def _(cx, cy, a, rng):
    """a valve hand wheel broken off its stem lying flat: the rim, three or four spokes, the hub with its square hole,
    the stem stub lying beside it"""
    u = rng.uniform; R = u(1.2, 1.5); n = rng.choice((3, 4, 5)); o = Obj("iron")
    g = U([ann(cx, cy, R, R - 0.45, 28)] + [barl((cx, cy), pol(cx, cy, R - 0.2, a + 6.2832 * k / n), 0.42) for k in range(n)] + [disc(cx, cy, 0.55, 12)])
    o.st(ext(g, 0.4), cuts=[(rect(cx, cy, 0.38, 0.38, a), 0.15)], foot=disc(cx, cy, R))
    c = pol(cx, cy, R + 0.9, a + u(0, 6.28)); t = u(0, 6.28); o.st(lie(c[0], c[1], t, 1.4, 0.26), foot=rect(c[0], c[1], 1.4, 0.6, t))
    return o.out()

@d("ferrum", "debris", "iron", "rivets_and_nuts")
def _(cx, cy, a, rng):
    """loose hardware scattered where a machine came apart: hex nuts with their holes, round-headed rivets, a washer"""
    u = rng.uniform; o = Obj("iron"); taken = []; want = rng.randint(5, 8)
    for i in range(60):
        if len(taken) >= want: break
        c = pol(cx, cy, u(0, 2.4), u(0, 6.28)); g = disc(c[0], c[1], 0.62, 8)
        if any(g.intersects(q) for q in taken): continue
        taken.append(g); k = rng.random()
        if k < 0.45: o.st(ext(ngon(c[0], c[1], 0.52, 6, u(0, 1)), 0.38), cuts=[(disc(c[0], c[1], 0.22, 8), 0.12)], foot=g)
        elif k < 0.8: o.st(cyl(c[0], c[1], 0.28, 0.5, n=8) + dome(c[0], c[1], 0.5, 0.3, n=10), foot=g)
        else: o.st(ext(ann(c[0], c[1], 0.6, 0.22, 12), 0.22), foot=g)
    return (hull(taken) if taken else disc(cx, cy, 0.7)), o.L

@d("ferrum", "made", "rust", "torn_plate")
def _(cx, cy, a, rng):
    """a deck plate torn up and thrown down askew: the rusted plate with its rows of rivet holes along the edges, one
    end ripped ragged, a second strip of plate lying across it"""
    u = rng.uniform; ff = frame(cx, cy, a); L = u(3.6, 4.4); W = u(2.4, 2.9); o = Obj("rust")
    g = rect(cx, cy, L, W, a).difference(LineString(wiggle(ff(L / 2 - u(0.6, 1.1), -W), ff(L / 2 - u(0.3, 0.9), W), rng, 0.5, 6)).buffer(1.2).intersection(rect(*ff(L / 2, 0), 2.4, W + 1, a)))
    g = max(polys_of(g), key=lambda q: q.area); b = o.up(g, 0.32)
    o.cut(U([disc(*ff(x, sv * (W / 2 - 0.55)), 0.22, 8) for x in np.arange(-L / 2 + 0.55, L / 2 - 0.9, 0.6) for sv in (-1, 1)] + [disc(*ff(-L / 2 + 0.55, v), 0.22, 8) for v in np.arange(-W / 2 + 1.15, W / 2 - 1.0, 0.6)]).intersection(inset(g, 0.35)), b, 0.2)
    t = a + u(0.5, 1.1); o.on(rect(*ff(u(-0.6, 0.2), 0), 0.95, W + 2, t), b, 0.28)
    return o.out()

@d("ferrum", "made", "iron", "ingot_stack")
def _(cx, cy, a, rng):
    """pig iron stacked to dry where it was cast: a course of bars laid one way, the next across it, the last course
    half taken; the bars' joints show as lines, one bar fallen off at the foot"""
    u = rng.uniform; ff = frame(cx, cy, a); L = u(2.6, 3.0); nb = 4; bw = L / nb; o = Obj("iron")
    g = rect(cx, cy, L, L, a); b = o.up(g, 0.5); o.cut(U([barl(ff(-L, -L / 2 + i * bw), ff(L, -L / 2 + i * bw), 0.28) for i in range(1, nb)]), b, 0.2)
    c2 = o.on(rect(cx, cy, L, L, a), b, 0.5); o.cut(U([barl(ff(-L / 2 + i * bw, -L), ff(-L / 2 + i * bw, L), 0.28) for i in range(1, nb)]), c2, 0.2)
    k = rng.randint(1, 3); c3 = o.on(rect(*ff(0, -L / 2 + k * bw / 2), L, k * bw, a), c2, 0.5)
    if k > 1: o.cut(U([barl(ff(-L, -L / 2 + i * bw), ff(L, -L / 2 + i * bw), 0.28) for i in range(1, k)]), c3, 0.2)
    t = a + u(0.2, 0.6); c = ff(L / 2 + 0.9, u(-0.6, 0.6)); o.up(rect(c[0], c[1], bw * 0.95, L, t), 0.45)
    return o.out()

@d("ferrum", "made", "iron", "crucible")
def _(cx, cy, a, rng):
    """a casting ladle tipped on its side: the bucket widening to its lip, the trunnion pins on its flanks, the black
    slag that ran out of its mouth set hard in a tongue across the dust"""
    u = rng.uniform; ff = frame(cx, cy, a); L = u(2.8, 3.4); r0 = u(0.95, 1.1); r1 = r0 * 1.25; o = Obj("iron")
    sols = spin(cx, cy, a, L, lambda t: r0 + (r1 - r0) * (t + L / 2) / L + (0.12 if (t + L / 2) / L > 0.92 else 0.0), 1.15)
    for sv in (-1, 1): sols += lie(*ff(0, sv * (r0 + 0.35)), a + 1.5708, 0.55, 0.33)
    o.st(sols, cuts=[(ell(*ff(L / 2 - 0.15, 0), 0.35, r1 * 0.75, a, 14), r1 * 0.6)], foot=rect(cx, cy, L, 2 * r1 + 0.8, a))
    sp = U([ell(*ff(L / 2 + 1.5, 0), 1.6, 1.0, a, 16), blob(*ff(L / 2 + 2.6, u(-0.6, 0.6)), 0.9, rng, 4)])
    s0 = o.up(sp, 0.35, "slag"); o.cut(U([arc_band for arc_band in [ann(*ff(L / 2 + 0.3, 0), r_, r_ - 0.18, 20) for r_ in (1.4, 2.0, 2.6)]]).buffer(0.06).intersection(inset(sp, 0.2)), s0, 0.14)
    return o.out()

@d("ferrum", "made", "iron", "chain_heap")
def _(cx, cy, a, rng):
    """a heap of heavy chain dropped in coils, its end trailing off with the hook still on it"""
    u = rng.uniform; o = Obj("iron"); n = rng.randint(2, 3)
    pts = spiral(cx, cy, 0.6, 0.6 + 0.65 * n, n, a, 14 * n)
    e = pol(cx, cy, u(3.8, 4.8), a + u(0, 6.28)); tail = wiggle(pts[-1], e, rng, 0.5, 3)
    o.st(xf_chain(pts + tail[1:]), foot=U([disc(cx, cy, 0.9 + 0.65 * n), LineString(tail).buffer(0.5)]))
    return o.out()


# ====================================================================================================== ground
@d("ferrum", "debris", "slag", "slag_lumps")
def _(cx, cy, a, rng):
    """lumps of black slag tipped from the furnace: rough rounded heaps, pocked with gas bubbles"""
    u = rng.uniform; o = Obj("slag"); taken = []
    for i in range(rng.randint(3, 6)):
        c = pol(cx, cy, 0 if i == 0 else u(1.2, 2.4), u(0, 6.28)); r = u(0.6, 1.1) * (1.3 if i == 0 else 1.0); g = disc(c[0], c[1], r + 0.1, 10)
        if any(g.intersects(q) for q in taken): continue
        taken.append(g); h = r * u(0.8, 1.2)
        cuts = [(disc(*pol(c[0], c[1], r * u(0, 0.45), u(0, 6.28)), u(0.12, 0.2), 6), h * 0.55) for _k in range(rng.randint(1, 3))]
        o.st(rock(c[0], c[1], r, h, rng), cuts=cuts, foot=g)
    return hull(taken), o.L

@d("ferrum", "debris", "slag", "slag_tongue")
def _(cx, cy, a, rng):
    """a frozen tongue of slag that ran out over the dust and set: the flat lobed spill with its pour ripples, a few
    clinkers sitting on it"""
    u = rng.uniform; ff = frame(cx, cy, a); o = Obj("slag")
    g = U([ell(*ff(-1.0, 0), 1.6, 1.1, a, 16), ell(*ff(1.0, u(-0.3, 0.3)), 1.4, 0.9, a, 14), blob(*ff(2.2, u(-0.5, 0.5)), 0.8, rng, 4)]).buffer(0.2).buffer(-0.2)
    b = o.up(g, u(0.35, 0.45))
    o.cut(U([ann(*ff(-2.6, 0), r_, r_ - 0.28, 24) for r_ in (1.5, 2.2, 2.9, 3.6)]).intersection(inset(g, 0.25)), b, 0.14)
    for i in range(rng.randint(1, 2)):
        c = ff(u(-1.4, 1.2), u(-0.4, 0.4)); o.studs([c], b, u(0.35, 0.45), 0.35)
    return o.out()

@d("ferrum", "marks", "tar", "oil_slick", ground=True)
def _(cx, cy, a, rng):
    """a black oil slick soaked into the dust, run off from a broken sump: the lobed pool and its drips"""
    u = rng.uniform; ff = frame(cx, cy, a)
    g = U([blob(cx, cy, u(1.3, 1.8), rng, 4), blob(*ff(u(1.2, 1.8), u(-0.5, 0.5)), u(0.7, 1.0), rng, 3)])
    dr = U([disc(*ff(u(-2.8, 3.2), u(-2.0, 2.0)), u(0.3, 0.45), 8) for _k in range(rng.randint(2, 4))]).difference(g.buffer(0.3))
    return hull([g, dr]) if not dr.is_empty else g.buffer(0.2), [("gcut", g, u(0.3, 0.4), "tar")] + ([("gcut", dr, 0.25, "tar")] if not dr.is_empty else [])

@d("ferrum", "made", "iron", "floor_vent")
def _(cx, cy, a, rng):
    """a vent grating set in the ground: the low iron frame with its corner bolts, the bars across it and the slots
    between them falling away into the dark"""
    u = rng.uniform; ff = frame(cx, cy, a); L = u(3.2, 3.8); W = u(2.4, 2.8); o = Obj("iron"); g = rect(cx, cy, L, W, a)
    fr = o.up(g, 0.35); hole = rect(cx, cy, L - 1.2, W - 1.2, a)
    o.cut(U([rect(*ff(x, 0), 0.42, W - 1.2, a) for x in np.arange(-(L - 1.2) / 2 + 0.3, (L - 1.2) / 2 - 0.15, 0.78)]).intersection(hole), fr, 0.3)
    o.studs([ff(sx * (L / 2 - 0.35), sy * (W / 2 - 0.35)) for sx in (-1, 1) for sy in (-1, 1)], fr, 0.2, 0.12)
    return o.out()

@d("ferrum", "debris", "dust", "dust_drift")
def _(cx, cy, a, rng):
    """red iron dust blown into a low drift, ripples on its back, a lump of slag showing out of its end"""
    u = rng.uniform; ff = frame(cx, cy, a); o = Obj("dust")
    g = U([ell(cx, cy, u(2.2, 2.8), u(1.0, 1.3), a, 18), blob(*ff(u(1.0, 1.6), u(-0.5, 0.5)), u(0.8, 1.1), rng, 4)])
    o.L.append(("mound", g, u(0.6, 0.85), "dust", XF_SOFT)); o.F.append(g)
    c = ff(-u(2.5, 3.0), u(-0.6, 0.6)); o.st(rock(c[0], c[1], u(0.55, 0.75), u(0.5, 0.7), rng), "slag", foot=disc(c[0], c[1], 0.8))
    return o.out()


_OWN = set(globals())
