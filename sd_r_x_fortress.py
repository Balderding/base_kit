"""Third drawing, fortress (see sd_r_x_frontier.py): at their real size beside a 32 mm figure. The torii's great
top beam lies fallen with its tie beam and its name tablet, a length of bamboo fence lies tied at every crossing, the
straw rice bales stand stacked with their rope bands."""
import math
from sd_reg import d
from sd_r_x_frontier import x_d
from sd_r_trederra import t_bar
from sd_r_x_dungeon import x_wall


@x_d("fortress", "made", "lacquer", "torii_beam")
def _(cx, cy, a, rng):
    """the fallen top of a torii: the great lacquered beam with its upswept ends and its cap, the second beam under
    it, the lower tie beam lying beside with the wedges through its ends, the name tablet between them in its frame with
    the characters carved down it; a post stump broken off at one end on some"""
    u = rng.uniform; ff = frame(cx, cy, a); o = Obj("lacquer"); L = u(8.6, 9.0); W = u(1.1, 1.25); h0 = u(1.0, 1.1); up = u(0.5, 0.8)
    o.st(x_wall([ff(-L / 2, 0), ff(L / 2, 0)], W, lambda s: h0 + up * abs(2 * s - 1) ** 2.5) + x_wall([ff(-L / 2 + 0.3, 0), ff(L / 2 - 0.3, 0)], W - 0.5, lambda s: h0 + 0.22 + up * abs(2 * s - 1) ** 2.5), foot=rect(cx, cy, L, W, a))
    sm = o.up(rect(*ff(0, -W / 2 - 0.3), L * 0.86, 0.85, a), u(0.75, 0.85))
    o.cut(U([barl(ff(x, -W), ff(x, -W / 2 - 0.85), 0.22) for x in (-L * 0.3, L * 0.3)]), sm, 0.1)
    v = -W / 2 - u(2.3, 2.6); t = a + u(-0.08, 0.08); f2 = frame(*ff(u(-0.4, 0.4), v), t); Ln = L * u(0.72, 0.8)
    nk = o.up(rect(*f2(0, 0), Ln, 0.9, t), u(0.55, 0.65))
    for sx in (-1, 1):
        if rng.random() < 0.8: o.on(rect(*f2(sx * Ln * 0.3, 0), 0.35, 0.9, t), nk, 0.25, "wood")
    pc = ff(u(-1.2, 1.2), (v - W / 2 - 0.42) / 2 - 0.3); tp = a + u(-0.3, 0.3) + rng.choice((0.0, math.pi)); f3 = frame(pc[0], pc[1], tp)
    pg = rect(pc[0], pc[1], 2.0, 1.3, tp); pb = o.up(pg, 0.42, "wood"); o.on(pg.difference(inset(pg, 0.22)), pb, 0.16, "lacquer")
    strokes = []
    for j in range(rng.randint(2, 3)):
        x = -0.55 + j * 0.55; strokes += [barl(f3(x - 0.12, -0.32), f3(x + 0.12, -0.32), 0.18), barl(f3(x, -0.38), f3(x, 0.3), 0.18), barl(f3(x - 0.15, 0.08), f3(x + 0.15, 0.12), 0.18)]
    o.cut(U(strokes).intersection(inset(pg, 0.3)), pb, 0.1)
    if rng.random() < 0.5:
        sx = rng.choice((-1, 1)); c = ff(sx * L * 0.33, W / 2 + 0.9); o.st(turn(c[0], c[1], [(0.85, 0.0), (0.85, u(0.9, 1.3))], 18), cuts=[(Polygon([pol(c[0], c[1], 1.0, a), pol(c[0], c[1], 1.0, a + 1.6), pol(c[0], c[1], 1.0, a + 3.2)]), 0.7)], foot=disc(c[0], c[1], 0.85))
    return o.out()

@x_d("fortress", "made", "bamboo", "bamboo_fence")
def _(cx, cy, a, rng):
    """a length of bamboo lattice fence come down flat: the cross rails, the uprights laid over them with a node
    swelling every little way, a knot of black cord tied at every crossing, the two thick end posts with their tops cut
    on the slant; one upright snapped and lying askew"""
    u = rng.uniform; ff = frame(cx, cy, a); o = Obj("bamboo"); L = u(5.3, 5.5); W = u(3.4, 3.8); rh = 0.24; rv = 0.24
    nodes = lambda Lx: (lambda s: (rh + 0.05) if (s * Lx / 0.9) % 1.0 < 0.1 else rh)
    vs = [v for v in np.linspace(-W * 0.36, W * 0.36, rng.randint(2, 3))]; ps = []; cuts = []          # all one surface: separate layers of bamboo would meet edge to edge where they cross
    for v in vs: ps += t_bar(ff(-L / 2, v), ff(L / 2, v + u(-0.1, 0.1)), nodes(L), z0=rh, z1=rh)
    nx = rng.randint(4, 6); xs = np.linspace(-L / 2 + 1.1, L / 2 - 1.1, nx); brk = rng.randint(0, nx - 1) if rng.random() < 0.5 else -1
    for i, x in enumerate(xs):
        if i == brk:
            p = ff(x, -W / 2 + 0.2); q = pol(p[0], p[1], W * 0.55, a + 1.5708 + u(0.3, 0.6)); ps += t_bar(p, q, rv, 1.0); continue
        ps += t_bar(ff(x, -W / 2), ff(x + u(-0.1, 0.1), W / 2), nodes(W), z0=2 * rh + rv * 0.6, z1=2 * rh + rv * 0.6)
        ps += [s_ for v in vs for s_ in dome(*ff(x, v), 0.36, 2 * rh + rv * 1.6 + 0.2, sq=0.8, a=a + 0.785, n=8)]
    for sx in (-1, 1):
        x = sx * (L / 2 - 0.25); rp = 0.42
        ps += t_bar(ff(x, -W / 2 - 0.3), ff(x, W / 2 + 0.3), lambda s_: rp if s_ < 0.92 else rp * (1 - (s_ - 0.92) / 0.1), 1.0); cuts.append((disc(*ff(x, -W / 2 - 0.3), 0.22, 8), rp))
    o.st(ps, cuts=cuts, foot=rect(cx, cy, L + 0.4, W + 0.6, a))
    return o.out()

@x_d("fortress", "made", "sack", "rice_bales")
def _(cx, cy, a, rng):
    """straw rice bales stacked by a storehouse: each a fat round bale, the straw running along it, three rope bands
    round it and the woven straw lids at its ends standing proud; two or three side by side and one laid in the
    saddle on top"""
    u = rng.uniform; ff = frame(cx, cy, a); L = u(3.1, 3.4); r = u(0.88, 0.95); n = rng.randint(2, 3); o = Obj("sack")
    def rf(s):
        x = s * L
        if x < 0.25 or x > L - 0.25: return r * 0.92
        if x < 0.4 or x > L - 0.4: return r * 0.8
        band = min(abs(x - L * 0.25), abs(x - L * 0.5), abs(x - L * 0.75))
        return r * (0.9 + 0.1 * math.sin(math.pi * (x - 0.4) / (L - 0.8))) + (0.08 if band < 0.12 else 0.0)
    def bale(c, b, z):
        f2 = frame(c[0], c[1], b)
        o.st(t_bar(f2(-L / 2, 0), f2(L / 2, 0), rf, z0=z + r * 0.95, z1=z + r * 0.95), cuts=[(U([barl(f2(-L / 2 + 0.45, v), f2(L / 2 - 0.45, v), 0.12) for v in (-r * 0.4, -r * 0.13, r * 0.13, r * 0.4)]).difference(U([rect(*f2(L * q - L / 2, 0), 0.3, 2 * r, b) for q in (0.25, 0.5, 0.75)])), z + r * 1.85)], foot=rect(c[0], c[1], L, 2 * r, b))
    for i in range(n): bale(ff(u(-0.12, 0.12), (i - (n - 1) / 2) * (2 * r + 0.05)), a + u(-0.05, 0.05), 0.0)
    if rng.random() < 0.75: bale(ff(u(-0.15, 0.15), (r + 0.03) * (0 if n == 2 else rng.choice((-1, 1)))), a + u(-0.06, 0.06), r * 1.5)
    return o.out()


_OWN = set(globals())
