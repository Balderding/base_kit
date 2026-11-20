"""ashveil: the style-only details of the Ashveil Wastes (see style_ashveil.py), drawn at their true proportions so that
at a figure scale they come out at the real sizes in scale/real_ashveil.json. A transport cab sits in the ash to its
windows, a container lies half under a drift, a pipeline's stilts still stand where the pipe fell, a marker pylon lies
along the ground with its beacon head. Made things are built in flat layers (Obj) so their seams and slots stay crisp;
drums, pipes, bones and drifts are rounded. Nothing overhangs.

Each kind belongs to one of the themes, so a theme can leave the others' things out (its ban list):
  ashhull   the transports (cab, track, wheel, container, hatch)
  ashpipe   the pipeline (stilts, valve, flanged pipe, drums)
  ashtarp   the scavengers (lean-to, still, sled, mask, helmet)
  ashpylon  the markers (pylon, stub, mast, sign, panel)
  the rest (scrap, bone, slabs, drifts, cracks, cinders) belongs everywhere."""
import math
from sd_reg import d


def xa_bar(p, q, rf, k=0.45):
    """a round bar from p to q lying on the ground, solid underneath (upright sides up to the axis, k radii off the
    ground, then round over the top): rf is its radius, a number or a function of 0..1 along it"""
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


def xa_drift(o, x, y, r, h, a, rng, sq=None):
    """a soft ash drift heaped over part of a thing: a low dome, longer down the wind"""
    o.st(dome(x, y, r, h, sq=sq or rng.uniform(0.55, 0.75), a=a), "ashdrift", foot=ell(x, y, r, r * 0.7, a, 16))


def xa_lattice(o, ff, x0, x1, w0, w1, a, kind, h=0.5, step=1.3):
    """a lattice girder lying flat: two side rails (w0 apart at x0, w1 at x1) with zig-zag braces between them"""
    wv = lambda x: (w0 + (w1 - w0) * (x - x0) / (x1 - x0)) / 2
    rails = U([barl(ff(x0, sv * wv(x0)), ff(x1, sv * wv(x1)), 0.55) for sv in (-1, 1)])
    xs = list(np.arange(x0, x1, step)) + [x1]; br = []
    for i, (xa, xb) in enumerate(zip(xs, xs[1:])):
        sv = 1 if i % 2 else -1; br.append(barl(ff(xa, sv * wv(xa)), ff(xb, -sv * wv(xb)), 0.48))
    b = xa_up(o, rails, h, kind); o.st(ext(U(br), h * 0.8), kind, foot=U(br)); return b


def xa_up(o, g, h, kind=None):
    """an upright piece from the ground built through ST(): keeps parts down to 0.45 mm wide (a plain "up" drops all under 0.88)"""
    return o.st(ext(g, h), kind, foot=g)


# ------------------------------------------------------------------------------------------------ transports
@d("ashveil", "made", "ashhull", "buried_cab")
def _(cx, cy, a, rng):
    """the cab of a light transport sunk in the ash to its windows: the slab sides and the sloped windscreen with its
    pillars, the roof hatch and its hinge, the grille slots across the nose, one huge tyre standing out beside it,
    and the drift heaped up over the back of the cab"""
    u = rng.uniform; ff = frame(cx, cy, a); L = u(4.2, 5.0); W = u(3.0, 3.4); o = Obj("ashhull"); h = u(1.5, 1.8)
    body = rrect(cx, cy, L, W, a, 0.35); b = o.up(body, h)
    o.cut(rect(*ff(L * 0.22, 0), 0.9, W - 0.8, a), b, 0.35)                     # windscreen recess
    rf_ = o.on(rrect(*ff(-L * 0.12, 0), L * 0.5, W - 0.6, a, 0.25), b, 0.22)
    hc = ff(-L * 0.12, u(-0.3, 0.3)); hp = o.on(disc(hc[0], hc[1], 0.6, 16), rf_, 0.14); o.cut(barl(pol(hc[0], hc[1], 0.45, a + 1.57), pol(hc[0], hc[1], 0.45, a - 1.57), 0.12), hp, 0.1)
    o.vents(ff, L / 2 - 0.45, 0, b, 4, W * 0.55, a + 1.5708, 0.38, 0.16, 0.2)
    sv = rng.choice((-1, 1)); tc = ff(u(-0.2, 0.6), sv * (W / 2 + 0.75)); tr = u(1.0, 1.2)
    tread = U([barl(pol(*pol(tc[0], tc[1], tr * f, a), 0.8, a + 1.5708), pol(*pol(tc[0], tc[1], tr * f, a), 0.8, a - 1.5708), 0.16) for f in (-0.6, -0.2, 0.2, 0.6)])
    o.st(xa_bar(pol(tc[0], tc[1], tr, a + math.pi), pol(tc[0], tc[1], tr, a), 0.62, 0.55), "rust", cuts=[(tread, 0.75)], foot=rect(tc[0], tc[1], 2 * tr, 1.3, a))
    xa_drift(o, *ff(-L * 0.45, u(-0.4, 0.4)), u(2.2, 2.8), u(1.2, 1.6), a + u(-0.3, 0.3), rng)
    return o.out()


@d("ashveil", "made", "ashhull", "track_links")
def _(cx, cy, a, rng):
    """a run of crawler track thrown off a transport: the heavy links side by side, each with its grouser bar across
    and the pin bosses at both edges, the run bending a little where it lies; the far end gone under the ash"""
    u = rng.uniform; o = Obj("ashhull"); n = rng.randint(5, 7); st = u(0.95, 1.05); W = u(2.2, 2.6); bend = u(-0.06, 0.06)
    x = -n * st / 2; t = a; p = (cx - math.cos(a) * n * st / 2, cy - math.sin(a) * n * st / 2); last = p
    for i in range(n):
        f2 = frame(p[0], p[1], t); g = rect(p[0], p[1], st - 0.12, W, t); b = o.up(g, u(0.38, 0.45))
        o.on(rect(*f2(0, 0), 0.32, W - 0.4, t), b, 0.22)
        o.on(U([disc(*f2(st / 2 - 0.06, sv * (W / 2 - 0.35)), 0.24, 8) for sv in (-1, 1)]), b, 0.14)
        last = p; t += bend; p = pol(p[0], p[1], st, t)
    xa_drift(o, last[0], last[1], u(1.6, 2.0), u(0.8, 1.0), t + 1.5708, rng)
    return o.out()


@d("ashveil", "made", "ashhull", "hub_wheel")
def _(cx, cy, a, rng):
    """a transport wheel lying flat where it came off: the deep-tread tyre, the dished rim inside it, the hub boss with
    its ring of wheel nuts, the axle hole; ash filling the dish on one side"""
    u = rng.uniform; R = u(1.7, 2.1); o = Obj("rust")
    ty = xa_up(o, ann(cx, cy, R, R * 0.6, 32), u(0.75, 0.9))
    o.cut(U([barl(pol(cx, cy, R * 0.86, a + k * 0.393), pol(cx, cy, R + 0.2, a + k * 0.393), 0.2) for k in range(16)]), ty, 0.2)
    rim = o.up(disc(cx, cy, R * 0.63, 28), 0.45); hb = o.on(disc(cx, cy, R * 0.3, 16), rim, 0.3)
    o.cut(disc(cx, cy, 0.22, 8), hb, 0.3)
    o.studs([pol(cx, cy, R * 0.43, a + k * 6.2832 / 8) for k in range(8)], rim, 0.17, 0.14)
    if rng.random() < 0.6: t = a + u(0, 6.28); p = pol(cx, cy, R * 0.8, t); xa_drift(o, p[0], p[1], R * 0.75, u(0.8, 1.0), t + 1.57, rng)
    return o.out()


@d("ashveil", "made", "ashhull", "cargo_container")
def _(cx, cy, a, rng):
    """a cargo container lying where it fell from its carrier: the long box with its corrugations pressed across, the
    corner castings, the double door at one end with its four locking bars; one end under a drift"""
    u = rng.uniform; ff = frame(cx, cy, a); L = u(6.5, 7.5); W = u(2.4, 2.7); h = u(2.0, 2.3); o = Obj("ashcargo")
    g = rect(cx, cy, L, W, a); b = o.up(g, h)
    o.cut(U([barl(ff(x, -W), ff(x, W), 0.2) for x in np.arange(-L / 2 + 0.9, L / 2 - 0.9, 0.5)]).intersection(inset(g, 0.35)), b, 0.12)
    o.corners(ff, L, W, b, a, 0.55, 0.12)
    o.cut(barl(ff(L / 2 - 0.18, -W), ff(L / 2 - 0.18, W), 0.12), b, 0.2)
    o.on(U([rect(*ff(L / 2 - 0.55, v), 0.5, 0.5, a) for v in (-W * 0.36, -W * 0.12, W * 0.12, W * 0.36)]), b, 0.12)
    sv = rng.choice((-1, 1)); xa_drift(o, *ff(-sv * L * 0.42, u(-0.5, 0.5)), u(2.4, 3.0), h * u(0.75, 0.95), a + 1.5708 + u(-0.3, 0.3), rng)
    return o.out()


@d("ashveil", "made", "ashhull", "buried_hatch", ground=True)
def _(cx, cy, a, rng):
    """the top hatch of something buried deep, just clear of the ash: the round coaming, the domed lid with its
    hand-wheel and spokes, the hinge block, the ash scoured away round it in a shallow hollow"""
    u = rng.uniform; R = u(1.3, 1.6); o = Obj("ashhull")
    o.gcut(disc(cx, cy, R + 0.9, 24), 0.25, "hollow")
    cm = xa_up(o, ann(cx, cy, R, R - 0.5, 28), 0.6); ld = o.up(disc(cx, cy, R - 0.45, 24), 0.4)
    o.on(ann(cx, cy, R * 0.55, R * 0.55 - 0.45, 20), ld, 0.3)
    o.on(U([barl(pol(cx, cy, R * 0.5, a + k * 2.094), pol(cx, cy, 0.0, a), 0.45) for k in range(3)]), ld, 0.3)
    o.up(rect(*pol(cx, cy, R + 0.1, a + math.pi), 0.8, 1.2, a), 0.75)
    return o.out()


# ------------------------------------------------------------------------------------------------ the pipeline
@d("ashveil", "made", "ashpipe", "stilt_pipe")
def _(cx, cy, a, rng):
    """where the pipeline came down: two or three of its stilts still standing in a line, each a square post with the
    saddle and its strap bolts on top, and the pipe section fallen beside them, flanged at its joint and torn ragged
    at its broken end"""
    u = rng.uniform; ff = frame(cx, cy, a); o = Obj("ashpipe"); k = rng.randint(2, 3); gap = u(3.2, 3.8); hs = u(2.2, 2.7)
    for i in range(k):
        c = ff((i - (k - 1) / 2) * gap, -1.2); sp = o.up(rect(c[0], c[1], 0.95, 0.95, a + u(-0.05, 0.05)), hs * u(0.85, 1.0))
        o.cut(rect(c[0], c[1], 1.4, 0.42, a + 1.5708), sp, 0.3)
        o.up(rect(c[0], c[1], 1.6, 1.6, a), 0.3)
    Lp = gap * (k - 1) + u(1.5, 2.5); r = u(0.6, 0.72)
    rf = lambda s: r * (1.22 if abs(s - 0.42) < 0.035 else 1.0) * (0.75 if s > 0.97 else 1.0)
    p = ff(-Lp / 2, 1.0); q = ff(Lp / 2, 1.0 + u(-0.3, 0.3))
    o.st(xa_bar(p, q, rf, 0.5), foot=LineString([p, q]).buffer(r))
    o.st(ext(U([tri(pol(*q, 0, a), pol(*q, u(0.5, 0.8), a + u(-0.4, 0.4)), 0.5)]), r * 1.2), foot=disc(q[0], q[1], 0.8))
    o.gcut(ell(*ff(Lp / 2 + 0.8, 1.2), 1.0, 0.7, a, 14), 0.3, "tar") if rng.random() < 0.6 else None
    return o.out()


@d("ashveil", "made", "ashpipe", "valve_head")
def _(cx, cy, a, rng):
    """a valve head standing up out of the ash where a buried line comes to the surface: the riser pipe with its
    flange and bolts, the valve body, the hand-wheel on top with its spokes"""
    u = rng.uniform; o = Obj("ashpipe"); r = u(0.7, 0.85)
    fl = o.up(disc(cx, cy, r + 0.45, 20), 0.35); o.studs([pol(cx, cy, r + 0.22, a + k * 1.0472) for k in range(6)], fl, 0.17, 0.12)
    rs = o.st(turn(cx, cy, [(r, 0.0), (r, 1.1), (r + 0.25, 1.15), (r + 0.25, 1.4), (r * 0.6, 1.5), (r * 0.6, 1.9)], 20), par=fl)
    wl = o.st(turn(cx, cy, [(r + 0.75, 1.9), (r + 0.75, 2.15), (0.3, 2.2)], 24), par=rs)
    o.cut(U([disc(*pol(cx, cy, (r + 0.75) * 0.55, a + 0.785 + k * 1.5708), 0.25, 8) for k in range(4)]), wl, 0.2)
    if rng.random() < 0.6: t = a + u(0, 6.28); p = pol(cx, cy, r + 1.0, t); xa_drift(o, p[0], p[1], 1.1, 0.7, t + 1.57, rng)
    return o.out()


@d("ashveil", "made", "ashpipe", "toppled_drums")
def _(cx, cy, a, rng):
    """fuel drums from a pipeline depot: one standing with its rolled rim and the two bungs in its lid, one or two on
    their sides with the rolling hoops proud round them, black seep run out under them into the ash"""
    u = rng.uniform; ff = frame(cx, cy, a); o = Obj("rust"); r = u(0.85, 0.95)
    o.gcut(blob(*ff(0.6, 0.4), u(1.6, 2.0), rng, 5), 0.25, "tar")
    c = ff(-1.4, 0); dr = o.up(disc(c[0], c[1], r, 22), u(1.6, 1.8)); o.cut(ann(c[0], c[1], r - 0.12, r - 0.3, 20), dr, 0.15)
    o.cut(U([disc(*pol(c[0], c[1], r * 0.5, a + sv * 1.2), 0.18, 8) for sv in (-1, 1)]), dr, 0.2)
    for i in range(rng.randint(1, 2)):
        t = a + u(-0.5, 0.5) + 1.5708; p = ff(0.6 + 0.2 * i, -0.6 + 2.0 * i); L = 2.4; f2 = frame(p[0], p[1], t)
        hoop = lambda s: r * (1.08 if abs(s - 0.33) < 0.035 or abs(s - 0.67) < 0.035 else 1.0)
        o.st(xa_bar(f2(-L / 2, 0), f2(L / 2, 0), hoop, 0.4), foot=rect(p[0], p[1], L, 2 * r, t))
    return o.out()


# ------------------------------------------------------------------------------------------------ the markers
@d("ashveil", "made", "ashpylon", "fallen_pylon")
def _(cx, cy, a, rng):
    """a marker pylon brought down by the wind: the lattice mast tapering from its foot plate to the beacon head, the
    head a slotted box with its lens ring, the foot torn out with its bolt plate and a lump of footing"""
    u = rng.uniform; ff = frame(cx, cy, a); L = u(7.5, 9.0); o = Obj("ashpylon"); x0 = -L / 2; x1 = L / 2 - 1.4
    xa_lattice(o, ff, x0 + 0.6, x1, 2.0, 1.1, a, "ashpylon", u(0.5, 0.6))
    hd = rect(*ff(L / 2 - 0.8, 0), 1.6, 1.5, a); bh = o.up(hd, u(1.1, 1.3)); o.vents(ff, L / 2 - 0.8, 0, bh, 3, 1.1, a, 0.38, 0.16, 0.25)
    xa_up(o, ann(*ff(L / 2 + 0.35, 0), 0.8, 0.3, 16), 0.9)
    ft = o.up(stone(*ff(x0 + 0.2, 0), u(1.2, 1.4), rng, 0.8, 7), u(0.6, 0.8), "ashstone")
    o.on(rect(*ff(x0 + 0.3, 0), 1.3, 1.6, a), ft, 0.15)
    return o.out()


@d("ashveil", "made", "ashpylon", "pylon_stub")
def _(cx, cy, a, rng):
    """the stump of a marker pylon still standing on its footing: the stepped plinth, the four leg stubs on their bolt
    plates sheared off at knee height, the number plate on the plinth face"""
    u = rng.uniform; ff = frame(cx, cy, a); S = u(2.6, 3.0); o = Obj("ashstone")
    b0 = o.up(rect(cx, cy, S + 0.8, S + 0.8, a), 0.5); b1 = o.on(rect(cx, cy, S, S, a), b0, 0.45)
    for sx in (-1, 1):
        for sy in (-1, 1):
            c = ff(sx * (S / 2 - 0.55), sy * (S / 2 - 0.55)); pl = o.on(rect(c[0], c[1], 0.8, 0.8, a), b1, 0.12, "ashpylon")
            o.on(rect(c[0], c[1], 0.5, 0.5, a), pl, u(0.6, 1.4), "ashpylon")
    o.cut(rect(*ff(S / 2 + 0.2, 0), 0.25, 1.0, a), b0, 0.12)
    return o.out()


@d("ashveil", "made", "ashpylon", "relay_mast")
def _(cx, cy, a, rng):
    """a relay mast lying where it fell: a slim lattice girder of even width, the dish at its top end lying face up
    with its rim and the feed boss in the middle, the guy anchors still in the ground"""
    u = rng.uniform; ff = frame(cx, cy, a); L = u(7.0, 8.5); o = Obj("rust")
    xa_lattice(o, ff, -L / 2, L / 2 - 2.0, 1.2, 1.2, a, "rust", 0.5, 1.1)
    c = ff(L / 2 - 0.6, 0); R = u(1.4, 1.7); ds = xa_up(o, ann(c[0], c[1], R, R - 0.5, 24), 0.7, "ashpylon"); o.up(disc(c[0], c[1], R - 0.45, 20), 0.3, "ashpylon")
    o.up(disc(c[0], c[1], 0.35, 10), 0.9, "ashpylon")
    for sv in (-1, 1): p = ff(u(-L / 2, 0), sv * u(1.8, 2.4)); xa_up(o, disc(p[0], p[1], 0.45, 10), 0.4)
    return o.out()


@d("ashveil", "made", "ashpylon", "warning_sign")
def _(cx, cy, a, rng):
    """a warning board fallen with its post: the post snapped off ragged, the square board across its head with its
    border and the chevrons cut in it, two bolt heads, a corner bent away"""
    u = rng.uniform; ff = frame(cx, cy, a); Ls = u(4.4, 5.2); o = Obj("ashscrap")
    ps = xa_up(o, U([rect(*ff(-0.4, 0), Ls - 0.8, 0.65, a)] + [tri(ff(Ls / 2 - 0.85, v), ff(Ls / 2 - 0.85 + u(0.3, 0.6), v * 0.6), 0.24) for v in (-0.18, 0.0, 0.18)]), 0.4)
    c = ff(-Ls / 2 + 1.2, 0); B = u(2.2, 2.6); t = a + u(-0.2, 0.2); f2 = frame(c[0], c[1], t)
    g = rect(c[0], c[1], B, B, t).difference(Polygon([f2(B / 2 - 0.6, B / 2 + 0.1), f2(B / 2 + 0.1, B / 2 + 0.1), f2(B / 2 + 0.1, B / 2 - 0.6)]))
    b = o.up(g, 0.55, "ashpaint"); o.groove(inset(g, 0.25), b, 0.16, 0.12)
    o.cut(U([line([f2(-0.5 + dx, -0.55), f2(0.05 + dx, 0), f2(-0.5 + dx, 0.55)], 0.24) for dx in (-0.1, 0.5)]), b, 0.14)
    o.studs([f2(0, sv * B * 0.32) for sv in (-1, 1)], b, 0.18, 0.08, "rust")
    return o.out()


@d("ashveil", "made", "ashpylon", "cell_panel")
def _(cx, cy, a, rng):
    """a power panel off a marker, lying tilted on its broken stand: the frame with its grid of cells, a few cells
    knocked out, the stand's two legs folded under it"""
    u = rng.uniform; ff = frame(cx, cy, a); L = u(3.2, 3.8); W = u(2.0, 2.4); o = Obj("ashpylon")
    for sv in (-1, 1): xa_up(o, rect(*ff(0, sv * (W / 2 - 0.3)), L - 0.4, 0.6, a), 0.35, "rust")
    g = rect(cx, cy, L, W, a); b = o.up(g, u(0.75, 0.9))
    nx, ny = 4, 3; gx = [-L / 2 + 0.25 + (L - 0.5) * i / nx for i in range(1, nx)]; gy = [-W / 2 + 0.25 + (W - 0.5) * j / ny for j in range(1, ny)]
    o.cut(U([barl(ff(x, -W / 2 + 0.3), ff(x, W / 2 - 0.3), 0.22) for x in gx] + [barl(ff(-L / 2 + 0.3, v), ff(L / 2 - 0.3, v), 0.22) for v in gy]), b, 0.18)
    for _i in range(rng.randint(1, 2)):
        i, j = rng.randint(0, nx - 1), rng.randint(0, ny - 1); cw, ch = (L - 0.5) / nx, (W - 0.5) / ny
        o.cut(rect(*ff(-L / 2 + 0.25 + cw * (i + 0.5), -W / 2 + 0.25 + ch * (j + 0.5)), cw - 0.3, ch - 0.3, a), b, 0.35)
    return o.out()


# ------------------------------------------------------------------------------------------------ the scavengers
@d("ashveil", "made", "ashtarp", "lean_to")
def _(cx, cy, a, rng):
    """a scavenger's lean-to: a tarp slung low over a frame of scrap poles, its open side to the lee, stones and a
    plate weighing its edges, the guy line pegged out"""
    u = rng.uniform; ff = frame(cx, cy, a); L = u(4.2, 5.0); W = u(2.6, 3.0); o = Obj("ashtarp")
    o.st(loaf(cx, cy, a, L, W, u(1.4, 1.7)), cuts=[(U([barl(ff(x, -W), ff(x, W), 0.16) for x in (-L * 0.2, L * 0.2)]), 1.0)], foot=rect(cx, cy, L, W, a))
    for sx in (-1, 1): xa_up(o, disc(*ff(sx * (L / 2 + 0.25), 0), 0.35, 8), u(1.8, 2.1), "ashscrap")
    for i in range(rng.randint(2, 4)): p = ff(u(-L / 2, L / 2), -W / 2 - 0.3); o.st(rock(p[0], p[1], u(0.4, 0.55), u(0.4, 0.55), rng), "ashcinder", foot=disc(p[0], p[1], 0.5))
    e = ff(L / 2 + u(1.2, 1.8), u(-0.8, 0.8)); o.st(rod([ff(L / 2 + 0.25, 0), e], 0.16, 1.0), "rope", foot=disc(e[0], e[1], 0.3)); o.up(disc(e[0], e[1], 0.3, 8), 0.5, "rust")
    return o.out()


@d("ashveil", "made", "ashtarp", "ash_still")
def _(cx, cy, a, rng):
    """a scavengers' still for wringing water out of the air: a drum with a cone of sheet on top, the coil of pipe
    wound down from its peak to a catch can, a stone on the can lid"""
    u = rng.uniform; o = Obj("rust"); r = u(0.9, 1.0)
    dr = o.up(disc(cx, cy, r, 22), u(1.2, 1.4)); o.st(cone(cx, cy, r + 0.15, 0.9, 0.25, n=20), "ashscrap", par=dr)
    t = a + u(-0.4, 0.4); cn = pol(cx, cy, r + 1.3, t)
    pts = [pol(cx, cy, r + 0.25, t + 1.2 * k / 6) for k in range(7)] + [cn]
    o.st(rod(pts, 0.2, 1.0), "ashpipe", foot=LineString(pts).buffer(0.3))
    cb = o.up(disc(cn[0], cn[1], 0.55, 14), 0.8); o.cut(ann(cn[0], cn[1], 0.45, 0.25, 12), cb, 0.1)
    return o.out()


@d("ashveil", "made", "ashtarp", "salvage_sled")
def _(cx, cy, a, rng):
    """a salvage sled dragged in over the ash: two runners turned up at the front, the deck of plates across them, the
    load lashed on (a drum, a box of parts), the tow rope lying out ahead in the drag marks"""
    u = rng.uniform; ff = frame(cx, cy, a); L = u(4.6, 5.4); W = u(2.2, 2.5); o = Obj("ashscrap")
    for sv in (-1, 1): xa_up(o, U([rect(*ff(0, sv * (W / 2 - 0.3)), L, 0.6, a), disc(*ff(L / 2, sv * (W / 2 - 0.3)), 0.3, 8)]), 0.35, "rust")
    dk = o.up(rect(*ff(-0.2, 0), L - 0.9, W, a), 0.55); o.cut(U([barl(ff(x, -W), ff(x, W), 0.14) for x in np.arange(-L / 2 + 1.0, L / 2 - 1.0, 1.1)]), dk, 0.1)
    bx = o.on(rect(*ff(-0.9, 0), 1.5, 1.4, a), dk, 0.8, "ashcargo"); o.lid(rect(*ff(-0.9, 0), 1.5, 1.4, a), bx, 0.12, 0.15, "ashcargo")
    o.st(xa_bar(ff(0.4, -0.75), ff(0.4, 0.75), 0.68, 0.45), "rust", par=dk, foot=rect(*ff(0.4, 0), 1.4, 1.5, a))
    e = ff(L / 2 + u(2.0, 3.0), u(-0.6, 0.6)); o.st(rod(wiggle(ff(L / 2, 0), e, rng, 0.3, 4), 0.17, 1.0), "rope", foot=disc(e[0], e[1], 0.3))
    o.gcut(U([barl(ff(L / 2 + 0.6, sv * (W / 2 - 0.25)), ff(L / 2 + 3.0, sv * (W / 2 - 0.25)), 0.4) for sv in (-1, 1)]), 0.15, "rut")
    return o.out()


@d("ashveil", "made", "ashtarp", "breath_mask")
def _(cx, cy, a, rng):
    """a breathing mask dropped in the ash, shown a little larger than life: the face plate with its two round eye
    lenses, the twin filter cans at the jaw, the strap trailing"""
    u = rng.uniform; ff = frame(cx, cy, a); o = Obj("rust")
    fc = ell(cx, cy, 1.25, 1.0, a, 20); b = o.up(fc, 0.5)
    o.cut(U([disc(*ff(0.35, sv * 0.45), 0.32, 12) for sv in (-1, 1)]), b, 0.25)
    for sv in (-1, 1): p = ff(-0.75, sv * 0.75); o.up(disc(p[0], p[1], 0.48, 14), 0.75, "ashscrap")
    o.st(rod(wiggle(ff(0.2, 1.0), ff(u(1.2, 2.0), u(1.6, 2.2)), rng, 0.3, 4), 0.17, 1.0), "ashtarp", foot=disc(*ff(1.4, 1.8), 0.3))
    return o.out()


@d("ashveil", "made", "ashtarp", "visor_helmet")
def _(cx, cy, a, rng):
    """a scavenger's helmet half sunk in the ash: the dome with the ridge along its crown, the visor slot across the
    front, the breather boss under it"""
    u = rng.uniform; ff = frame(cx, cy, a); o = Obj("ashscrap"); r = u(1.05, 1.2)
    o.st(dome(cx, cy, r, r * 0.95, sq=0.88, a=a) + ext(rect(cx, cy, 2 * r * 0.95, 0.5, a), r * 1.0), cuts=[(rect(*ff(r * 0.55, 0), 0.4, r * 1.3, a), r * 0.45)], foot=disc(cx, cy, r))
    o.up(disc(*ff(r + 0.2, 0), 0.38, 10), 0.4, "rust")
    xa_drift(o, *ff(-r * 0.9, u(-0.3, 0.3)), r * 1.1, r * 0.6, a + 1.57, rng)
    return o.out()


# ------------------------------------------------------------------------------------------------ bones and scrap
@d("ashveil", "made", "bone", "grazer_skull")
def _(cx, cy, a, rng):
    """the bleached skull of a great grazing beast: the long muzzle with its rows of flat teeth, the deep eye sockets,
    the two heavy horns sweeping back from the brow and curling at the tips"""
    u = rng.uniform; ff = frame(cx, cy, a); o = Obj("bone"); L = u(3.2, 3.8)
    sk = spin(*ff(0, 0), a, L, lambda s: 0.55 + 0.55 * math.sin(min(1.0, s * 1.6) * 1.5708) * (1.0 - 0.45 * max(0.0, s - 0.6) / 0.4), 1.0)
    o.st(sk, cuts=[(U([disc(*ff(-L * 0.12, sv * 0.55), 0.3, 10) for sv in (-1, 1)]), 0.35), (U([barl(ff(L * 0.2, sv * 0.35), ff(L * 0.45, sv * 0.28), 0.12) for sv in (-1, 1)]), 0.45)],
         foot=rect(cx, cy, L, 2.2, a))
    for sv in (-1, 1):
        pts = [ff(-L * 0.3, sv * 0.8), ff(-L * 0.5, sv * 1.6), ff(-L * 0.8, sv * u(1.8, 2.1)), ff(-L * 0.95, sv * u(1.3, 1.6))]
        o.st(rod(pts, 0.34, 1.0), foot=LineString(pts).buffer(0.4))
    return o.out()


@d("ashveil", "made", "bone", "rib_arches")
def _(cx, cy, a, rng):
    """the spine and ribs of a big beast stripped by the wind: the row of vertebrae along the ground, the long ribs
    curving out from it, the far ones gone under a drift"""
    u = rng.uniform; ff = frame(cx, cy, a); o = Obj("bone"); L = u(5.0, 6.0); n = rng.randint(4, 6)
    for i in range(n + 2):
        x = -L / 2 + L * i / (n + 1); o.st(dome(*ff(x, 0), 0.5, 0.65, sq=0.8, a=a), foot=disc(*ff(x, 0), 0.5))
    for i in range(n):
        x = -L / 2 + L * (i + 1) / (n + 1)
        for sv in (-1, 1):
            if rng.random() < 0.15: continue
            pts = [ff(x, sv * 0.4), ff(x + 0.3, sv * 1.3), ff(x + 0.2, sv * u(2.0, 2.4)), ff(x - 0.2, sv * u(2.6, 3.0))]
            o.st(rod(pts, 0.26, 1.0), foot=LineString(pts).buffer(0.3))
    xa_drift(o, *ff(L / 2 - 0.2, rng.choice((-1, 1)) * u(0.5, 1.2)), u(1.2, 1.6), u(0.6, 0.8), a + 1.57, rng)
    return o.out()


@d("ashveil", "made", "ashscrap", "scrap_heap")
def _(cx, cy, a, rng):
    """a heap of salvage dragged together: torn plates leaning on one another at different heights, a length of
    pipe, bent bars, the whole pile half drowned in ash on its windward side"""
    u = rng.uniform; o = Obj("ashscrap"); foot = []
    for i in range(rng.randint(3, 5)):
        p = pol(cx, cy, u(0.2, 1.6), a + u(0, 6.28)); t = u(0, 3.14)
        g = Polygon([pol(p[0], p[1], u(0.9, 1.4), t + k * 1.5708 + u(-0.3, 0.3)) for k in range(4)]).buffer(0)
        if g.is_valid and g.area > 0.8: o.up(g, u(0.6, 1.6), rng.choice(("ashscrap", "ashhull", "rust")))
    p = pol(cx, cy, 1.6, a + 1.57); q = pol(cx, cy, 1.6, a - 1.57); o.st(xa_bar(pol(*p, 1.0, a), pol(*q, 1.0, a), 0.45, 0.5), "ashpipe", foot=LineString([p, q]).buffer(0.5))
    xa_drift(o, *pol(cx, cy, 1.8, a + math.pi), u(1.8, 2.2), u(0.9, 1.2), a + 1.57, rng)
    return o.out()


@d("ashveil", "debris", "ashstone", "rebar_slab")
def _(cx, cy, a, rng):
    """a broken slab of poured stone from some long-gone structure, cracked across, the bent reinforcing bars standing
    out of its torn edge and lying along the ash"""
    u = rng.uniform; ff = frame(cx, cy, a); o = Obj("ashstone")
    g = stone(cx, cy, u(1.6, 2.0), rng, 0.75, 7); b = o.up(g, u(0.6, 0.8))
    o.cut(wavy(pol(cx, cy, 2.2, a + 0.4), pol(cx, cy, 2.2, a + 3.4), rng, 0.3, 4, 0.16).intersection(inset(g, 0.2)), b, 0.2)
    for v in (-0.6, 0.0, 0.6):
        p = ff(1.2, v); q = ff(u(2.4, 3.2), v + u(-0.6, 0.6)); o.st(rod([p, q], 0.3, 1.0), "rust", foot=LineString([p, q]).buffer(0.32))
    return o.out()


@d("ashveil", "debris", "ashcinder", "cinder_rocks")
def _(cx, cy, a, rng):
    """dark cinder lumps blown clean of the ash, a few fist-sized pieces round a bigger one"""
    o = Obj("ashcinder")
    o.st(rock(cx, cy, rng.uniform(0.9, 1.2), rng.uniform(0.8, 1.1), rng), foot=disc(cx, cy, 1.1))
    for i in range(rng.randint(3, 5)): p = pol(cx, cy, rng.uniform(1.4, 2.4), a + rng.uniform(0, 6.28)); o.st(rock(p[0], p[1], rng.uniform(0.4, 0.6), rng.uniform(0.35, 0.55), rng), foot=disc(p[0], p[1], 0.6))
    return o.out()


@d("ashveil", "debris", "ashdrift", "ash_dune")
def _(cx, cy, a, rng):
    """a wind-built ash drift: a long low dune, steep on its lee side, its back combed into ripples by the wind"""
    u = rng.uniform; ff = frame(cx, cy, a); L = u(3.0, 4.0); o = Obj("ashdrift")
    h = u(0.8, 1.1)
    o.st(dome(cx, cy, L / 2, h, sq=u(0.45, 0.6), a=a), foot=ell(cx, cy, L / 2, L / 4, a, 16))
    for v in (-0.9, 0.9): o.st(loaf(*ff(u(-0.3, 0.3), v * L * 0.3), a + u(-0.1, 0.1), L * 0.45, 0.7, h * 0.45), foot=rect(*ff(0, v * L * 0.3), L * 0.45, 0.7, a))
    return o.out()


@d("ashveil", "debris", "ashdrift", "ripple_patch")
def _(cx, cy, a, rng):
    """a patch of wind ripples where the ash lies deeper: four or five soft ridges side by side, wavering"""
    u = rng.uniform; ff = frame(cx, cy, a); L = u(3.4, 4.4); n = rng.randint(4, 5); sols = []
    for i in range(n):
        v = (i - (n - 1) / 2) * 0.9; l = L * (1.0 - 0.25 * abs(i - (n - 1) / 2) / n)
        sols += loaf(*ff(u(-0.3, 0.3), v), a + u(-0.08, 0.08), l, 0.85, u(0.35, 0.45))
    return rect(cx, cy, L, n * 0.9, a), [ST(sols, "ashdrift")]


@d("ashveil", "marks", "crack", "hardpan_cracks")
def _(cx, cy, a, rng):
    """the ash baked to a crust and split into plates: a ring of cracks round a middle and a few running out of it"""
    u = rng.uniform; R = u(1.6, 2.2); n = rng.randint(5, 7)
    pts = [pol(cx, cy, R * u(0.8, 1.1), a + k * 6.2832 / n) for k in range(n)]
    cs = [LineString(wiggle(p, q, rng, 0.25, 3)).buffer(0.13, 4) for p, q in zip(pts, pts[1:] + pts[:1])]
    cs += [LineString(wiggle(p, pol(cx, cy, R * u(1.8, 2.4), math.atan2(p[1] - cy, p[0] - cx) + u(-0.3, 0.3)), rng, 0.3, 3)).buffer(0.13, 4) for p in pts if rng.random() < 0.6]
    g = U(cs); return g.convex_hull, [("gcut", g, 0.3, "crack")]


@d("ashveil", "marks", "rut", "tread_marks")
def _(cx, cy, a, rng):
    """the tread marks of a crawler that passed long ago: two wide bands pressed into the ash with the grouser lines
    across them, drifted over in places"""
    u = rng.uniform; ff = frame(cx, cy, a); L = u(5.0, 6.5); w = u(0.9, 1.1); gs = []
    for sv in (-1, 1):
        v = sv * u(1.0, 1.2); band = rect(*ff(0, v), L, w, a)
        gaps = U([rect(*ff(x, v), 0.3, w + 0.2, a) for x in np.arange(-L / 2 + 0.4, L / 2, 0.75)])
        gap2 = rect(*ff(u(-L / 3, L / 3), v), u(0.8, 1.4), w + 0.2, a) if rng.random() < 0.6 else Polygon()
        gs.append(band.difference(gaps).difference(gap2))
    g = U(gs); return g.convex_hull, [("gcut", g, 0.22, "rut")]


@d("ashveil", "debris", "ashdrift", "drift_wreck")
def _(cx, cy, a, rng):
    """an ash drift heaped over a piece of wreckage, the thing still showing where the wind has not covered it: a
    torn plate standing up out of its lee side, a pipe end with its flange, or the rim of a wheel"""
    u = rng.uniform; ff = frame(cx, cy, a); o = Obj("ashdrift"); L = u(3.0, 3.8)
    k = rng.randint(0, 2)
    if k == 0:
        g = Polygon([ff(L * 0.1, -1.1), ff(L * 0.55, -0.9), ff(L * 0.62, 0.7), ff(L * 0.2, 1.0)]).buffer(0)
        o.up(g, u(1.0, 1.4), "ashhull")
    elif k == 1:
        p, q = ff(-0.2, 0), ff(L * 0.62, u(-0.3, 0.3)); o.st(xa_bar(p, q, lambda s: 0.6 if s < 0.88 else 0.78, 0.5), "ashpipe", foot=LineString([p, q]).buffer(0.6))
    else:
        c = ff(L * 0.35, 0); o.st(ext(ann(c[0], c[1], 1.15, 0.6, 20), u(0.8, 1.0)), "rust", foot=disc(c[0], c[1], 1.15))
    xa_drift(o, *ff(-L * 0.12, 0), L / 2, u(0.8, 1.1), a, rng, sq=u(0.55, 0.7))
    for i in range(rng.randint(1, 3)): p = ff(u(-L * 0.4, L * 0.5), rng.choice((-1, 1)) * u(1.2, 1.7)); o.st(rock(p[0], p[1], u(0.35, 0.5), u(0.3, 0.45), rng), "ashcinder", foot=disc(p[0], p[1], 0.5))
    return o.out()


@d("ashveil", "debris", "ashscrap", "scrap_litter")
def _(cx, cy, a, rng):
    """the litter of a wreck spread over the ash: torn sheet pieces at odd angles, a bent bar or two, a bolt plate,
    a few cinders among them"""
    u = rng.uniform; o = Obj("ashscrap")
    for i in range(rng.randint(3, 5)):
        p = pol(cx, cy, u(0.3, 2.2), u(0, 6.28)); t0 = u(0, 3.14); r = u(0.6, 0.95)
        g = Polygon([pol(p[0], p[1], r * u(0.7, 1.2), t0 + k * 1.5708 + u(-0.35, 0.35)) for k in range(4)]).buffer(0)
        if g.is_valid and g.area > 0.6: o.up(g, u(0.35, 0.7), rng.choice(("ashscrap", "ashhull", "rust")))
    for i in range(rng.randint(1, 2)):
        p = pol(cx, cy, u(0.5, 1.8), u(0, 6.28)); q = pol(p[0], p[1], u(1.6, 2.4), u(0, 6.28)); o.st(rod(wiggle(p, q, rng, 0.25, 3), 0.26, 1.0), "rust", foot=LineString([p, q]).buffer(0.3))
    for i in range(rng.randint(2, 3)): p = pol(cx, cy, u(1.0, 2.6), u(0, 6.28)); o.st(rock(p[0], p[1], u(0.35, 0.5), u(0.3, 0.45), rng), "ashcinder", foot=disc(p[0], p[1], 0.5))
    return o.out()


_OWN = set(globals())
