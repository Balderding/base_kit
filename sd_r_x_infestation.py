"""Third drawing, infestation (see sd_r_x_frontier.py): the hive's grown things at their real size beside a 32 mm
figure. The eggs stand as tall as a man's knee, an opened one gapes with its petals peeled back, the shed husk and the
cocoon stand to the height of the thing that made them."""
import math
from sd_reg import d
from sd_r_x_frontier import x_d
from sd_r_infestation import _pl, _lean, _curl


def x_veins(p, prof, t, n=8, w=0.12):
    """grooves running down a turned dome from its crown: short pieces, each cut a hair below the dome where it lies
    (a cut keeps one floor, so one long cut would bite deep into the crown)"""
    top = [(r, z) for r, z in prof]; i = max(range(len(top)), key=lambda k: top[k][0]); top = top[i:]
    def zat(rr):
        for (r0, z0), (r1, z1) in zip(top, top[1:]):
            if r1 <= rr <= r0: return z0 + (z1 - z0) * (r0 - rr) / max(1e-6, r0 - r1)
        return top[-1][1]
    rmax = top[0][0]; out = []
    for k in range(4):
        r0 = rmax * (0.25 + 0.19 * k); r1 = rmax * (0.25 + 0.19 * (k + 1)) + 0.05
        out.append((U([barl(pol(p[0], p[1], r0, t + 6.2832 * j / n + 0.04 * k), pol(p[0], p[1], r1, t + 6.2832 * j / n + 0.04 * (k + 1)), w) for j in range(n)]), zat(min(r1, rmax)) - 0.1))
    return out

def x_egg(o, p, r, h, t, par=None, opened=False, kind="egg"):
    """a leathery egg: swelling out from its foot, drawing in to a crown of four closed lips, veins running down from
    the crown; opened, the lips stand apart over the dark hollow"""
    prof = [(r * 0.8, 0.0), (r * 0.98, h * 0.25), (r, h * 0.42), (r * 0.92, h * 0.62), (r * 0.72, h * 0.8), (r * 0.42, h * 0.93), (r * 0.18, h)]
    ps = turn(p[0], p[1], prof, 20); veins = x_veins(p, prof, t + 0.4)
    if opened: cuts = [(star(p[0], p[1], r * 0.7, 0.24, 4, t), h * 0.45)]
    else: cuts = [(U([barl(pol(p[0], p[1], r * 0.45, t + j * 1.5708), pol(p[0], p[1], r * 0.45, t + j * 1.5708 + math.pi), 0.16) for j in range(2)]), h * 0.84)]
    return o.st(ps, kind, par=par, cuts=cuts + veins, foot=disc(p[0], p[1], r))


@x_d("infestation", "growth", "egg", "egg_ledge")
def _(cx, cy, a, rng):
    """a shelf of hardened resin with a clutch of eggs standing on it: the shelf curved, ribbed across and scalloped
    along its edge, each egg knee high with the four closed lips of its crown and veins running down from them; one has
    opened on some"""
    u = rng.uniform; R = u(3.2, 4.2); n = rng.randint(3, 4); span = n * u(2.15, 2.35) / R; o = Obj("resin"); ox, oy = cx - R * math.cos(a), cy - R * math.sin(a)
    pts = arc(ox, oy, R, a - span / 2, a + span / 2, 14)
    g = U([line(pts, 2.6)] + [disc(*pol(ox, oy, R + 1.25, t), u(0.4, 0.62), 10) for t in np.arange(a - span / 2, a + span / 2, 0.85 / R)])
    b = o.up(g, u(0.55, 0.75)); o.cut(U([barl(pol(ox, oy, R - 2, a - span / 2 + span * (k + 1) / n), pol(ox, oy, R + 2, a - span / 2 + span * (k + 1) / n), 0.28) for k in range(n - 1)]), b, 0.2)
    o.cut(U([barl(pol(ox, oy, R + 0.5, t), pol(ox, oy, R + 1.4, t + 0.05), 0.12) for t in np.arange(a - span / 2 + 0.2 / R, a + span / 2, 0.55 / R)]), b, 0.1)
    op = rng.randint(0, n - 1) if rng.random() < 0.4 else -1
    for k in range(n):
        p = pol(ox, oy, R - 0.1 + u(-0.15, 0.15), a - span / 2 + span * (k + 0.5) / n); r = u(0.9, 1.02); h = u(2.4, 2.9)
        x_egg(o, p, r, h, u(0, 1.5), b, k == op)
    return o.out()

@x_d("infestation", "growth", "egg", "burst_egg")
def _(cx, cy, a, rng):
    """an egg that has hatched: the leathery body swelling out from its foot, the top torn open into four or five
    petals peeled back from a dark hollow, veins down its sides, roots gripping the ground, slime run out on one side"""
    u = rng.uniform; r = u(1.4, 1.6); h = r * u(2.0, 2.2); n = rng.choice((4, 4, 5)); o = Obj("egg")
    prof = [(r * 0.82, 0.0), (r, h * 0.3), (r * 0.98, h * 0.52), (r * 0.88, h * 0.74), (r * 0.76, h * 0.86)]
    ps = turn(cx, cy, prof, 24)
    for k in range(n):          # the petals, leaning out from the torn rim
        t = a + 6.2832 * k / n + u(-0.2, 0.2); p = pol(cx, cy, r * 0.66, t); lean = u(0.5, 0.75)
        ps += [((lambda z, sect=sect, z0=h * 0.7: sect(z - z0) if z >= z0 else sect(0.0)), h * 0.7 + top) for sect, top in _lean(p[0], p[1], r * 0.62, h * u(0.5, 0.6), lean * math.cos(t), lean * math.sin(t), 8, 0.26, 0.55, t + 1.5708, 0.6)]
    vein = x_veins((cx, cy), prof, a + 0.3, 7)
    o.st(ps, cuts=[(disc(cx, cy, r * 0.5, 16), h * 0.4)] + vein, foot=disc(cx, cy, r + 0.3))
    rs = []
    for k in range(rng.randint(4, 6)): t = a + 6.2832 * k / 5 + u(-0.3, 0.3); rs += rod(_curl(pol(cx, cy, r * 0.75, t), t, r * 0.3 + u(1.0, 1.5), u(-0.9, 0.9), 6), lambda q: 0.46 - 0.2 * q, 0.9)
    o.st(rs, foot=disc(cx, cy, r + 1.4))
    if rng.random() < 0.6: o.gcut(blob(*pol(cx, cy, r + 1.0, u(0, 6.28)), u(0.6, 0.9), rng, 3), 0.26, "acid")
    return o.out()

@x_d("infestation", "made", "carapace", "molt")
def _(cx, cy, a, rng):
    """a shed skin: a long husk of overlapping back plates, the head shield with its empty eye sockets, a row of back
    spines, narrowing to two tail prongs, split open down the back where the thing climbed out, the empty leg cases
    still sticking out to the sides"""
    u = rng.uniform; ff = frame(cx, cy, a); L = u(4.8, 6.0); W = u(1.05, 1.25); ns = rng.randint(6, 8); o = Obj("carapace"); hk = u(1.15, 1.3)
    env = [(0.0, 0.3), (0.12, 0.5), (0.6, 0.86), (0.72, 0.8), (0.78, 1.0), (0.93, 0.9), (1.0, 0.25)]
    def rf(t):
        s = (t + L / 2) / L; sc = 1.0 if s > 0.75 else 0.78 + 0.22 * (1 - ((s / 0.75 * ns) % 1.0)) ** 0.7
        return W * _pl(env, s) * sc + 0.03
    legs = []
    for k in range(3):
        for sv in (-1, 1):
            if rng.random() < 0.2: continue
            x = L * (0.02 + 0.13 * k); p0 = ff(x, sv * W * 0.4); kn = ff(x + u(-0.2, 0.3), sv * (W + u(0.6, 1.0))); tp = ff(x - u(0.5, 1.0), sv * (W + u(1.2, 1.9)))
            legs += rod([p0, kn, tp], lambda q: 0.32 - 0.07 * q, 1.4)
    for sv in (-1, 1): legs += rod([ff(-L / 2 + 0.3, sv * 0.2), ff(-L / 2 - u(0.6, 1.1), sv * u(0.4, 0.8))], lambda q: 0.32 - 0.07 * q, 1.4)
    o.st(legs, foot=rect(cx, cy, L + 2.0, 2 * W + 3.6, a))
    split = Polygon([ff(-L * 0.12, 0), ff(L * 0.1, -0.38), ff(L * 0.42, -0.32), ff(L * 0.47, 0), ff(L * 0.42, 0.32), ff(L * 0.1, 0.38)])
    eyes = U([ell(*ff(L * 0.38, sv * W * 0.45), 0.24, 0.16, a + sv * 0.4, 8) for sv in (-1, 1)])
    body = spin(cx, cy, a, L, rf, hk)
    sp = [s for x in np.linspace(-L * 0.4, -L * 0.15, rng.randint(3, 4)) for s in cone(*ff(x, 0), 0.3, W * 0.55 * hk + 0.5, 0.06, n=6)]
    o.st(body + sp, cuts=[(split, 0.3), (eyes, W * hk * 0.5)], foot=rect(cx, cy, L, 2 * W, a))
    return o.out()

@x_d("infestation", "growth", "web", "cocoon")
def _(cx, cy, a, rng):
    """somebody wrapped up: the round of the head, the neck, the shoulders, narrowing down to the feet, the wrapping
    crossing over it in slanting bands, strands holding it down to a skirt of dried resin; a boot or a hand left
    sticking out of the end on some"""
    u = rng.uniform; ff = frame(cx, cy, a); L = u(5.4, 6.4); r = u(1.2, 1.4); o = Obj("web")
    env = [(0.0, 0.0), (0.03, 0.4), (0.3, 0.72), (0.66, 1.0), (0.76, 0.9), (0.81, 0.56), (0.87, 0.66), (0.95, 0.56), (1.0, 0.0)]
    nb = rng.randint(6, 8); sl = rng.choice((-1, 1))
    def rf(t):
        s = (t + L / 2) / L; return r * _pl(env, s) * (1.0 if s > 0.79 else 0.86 + 0.14 * (1 - ((s / 0.79 * nb) % 1.0)) ** 0.6) + 0.03
    o.up(rect(cx, cy, L * 0.85, 2 * r + 0.8, a).buffer(0.4).buffer(-0.2, 4), 0.2, "resin")
    cuts = [(barl(ff(x - 0.6, -r * 1.2), ff(x + 0.6, r * 1.2), 0.16) if sl > 0 else barl(ff(x + 0.6, -r * 1.2), ff(x - 0.6, r * 1.2), 0.16), r * 0.9) for x in np.arange(-L * 0.42, L * 0.25, 0.75)]
    o.st(spin(cx, cy, a, L, rf, 1.15), cuts=cuts, foot=rect(cx, cy, L, 2 * r, a))
    ps = []
    for i in range(rng.randint(3, 5)):
        x = u(-L * 0.4, L * 0.35); sv = rng.choice((-1, 1)); p = ff(x, sv * r * 0.3); e = ff(x + u(-1.2, 1.2), sv * (r + u(1.3, 2.0))); ps += rod([p, e], lambda q: 0.4 - 0.14 * q, 1.1); o.F.append(disc(e[0], e[1], 0.3))
    o.st(ps, foot=rect(cx, cy, L, 2 * r, a))
    k = rng.random()
    if k < 0.45:          # a boot: sole, heel and the tread across it
        c = ff(-L / 2 - 0.55, u(-0.2, 0.2)); t = a + u(-0.4, 0.4); f2 = frame(c[0], c[1], t); b = o.up(rrect(c[0], c[1], 1.8, 0.98, t, 0.3), 0.8, "metal")
        o.cut(U([barl(f2(x, -1), f2(x, 1), 0.2) for x in (-0.5, -0.15, 0.2)]), b, 0.14); o.on(rrect(*f2(0.5, 0), 0.6, 0.8, t, 0.2), b, 0.2, "metal")
    elif k < 0.7:          # a hand: the back of it and four fingers
        c = ff(-L * 0.1, rng.choice((-1, 1)) * (r + 0.5)); t = math.atan2(c[1] - cy, c[0] - cx); hs = dome(c[0], c[1], 0.5, 0.55)
        for j in range(4): p = pol(c[0], c[1], 0.3, t + (j - 1.5) * 0.4); hs += rod([p, pol(p[0], p[1], u(0.7, 1.0), t + (j - 1.5) * 0.3)], 0.24, 1.3)
        o.st(hs, "flesh", foot=disc(c[0], c[1], 1.3))
    return o.out()


_OWN = set(globals())
