"""Third drawing, forest (see sd_r_x_frontier.py): at their real size beside a 32 mm figure. The snag stands chest
high with its splintered top, the hollow log is round and as thick as a man's waist, the fallen branch forks and
carries its leaves."""
import math
from sd_reg import d
from sd_r_x_frontier import x_d


def x_bark(pts, r, top, n=4, w=0.1):
    """bark furrows running along a lying limb: a cut each a hair under its top (cut floor `top`)"""
    return [(U([LineString([pol(p[0], p[1], v, math.atan2(q[1] - p[1], q[0] - p[0]) + 1.5708) for p, q in zip(pts[:-1], pts[1:])]).buffer(w / 2) for v in np.linspace(-r * 0.55, r * 0.55, n)]), top)]

@x_d("forest", "growth", "stump", "snag")
def _(cx, cy, a, rng):
    """a dead tree snapped off chest high: the trunk flaring into its roots, the bark furrowed up its length and
    sloughed away in a patch, the top broken into splintered spikes of different heights round a rotten hollow, a
    woodpecker's hole, a bracket fungus or two low on its side"""
    u = rng.uniform; r = u(0.95, 1.1); h = u(4.6, 5.4); sq = u(0.88, 1.0); t0 = u(0, 6.28)
    prof = [(r * 1.42, 0.0), (r * 1.3, h * 0.05), (r * 1.08, h * 0.16), (r, h * 0.3), (r * 0.94, h)]
    def zr(z):
        for (r0, z0), (r1, z1) in zip(prof, prof[1:]):
            if z <= z1: return r0 + (r1 - r0) * (z - z0) / max(1e-6, z1 - z0)
        return prof[-1][0]
    nf = rng.randint(13, 17)
    ps = [(lambda z: affinity.scale(star(cx, cy, zr(z), zr(z) - 0.13, nf, t0), 1.0, sq) if z < h else None, h)]          # the trunk, its bark furrowed up its length
    for k in range(5):          # the root flare: buttresses running out into the ground
        t = t0 + 6.2832 * k / 5 + u(-0.3, 0.3); p = pol(cx, cy, r * 0.9, t); q = pol(cx, cy, r * 1.55, t + u(-0.2, 0.2))
        ps += rod([p, q], lambda s: 0.42 - 0.18 * s, 1.6)
    cuts = []
    sp = rng.randint(3, 4)          # the broken top: notches between spikes, each cut down to its own height
    for k in range(sp):
        t = t0 + 6.2832 * (k + 0.5) / sp + u(-0.2, 0.2)
        cuts.append((Polygon([(cx, cy), pol(cx, cy, r * 1.6, t - u(0.35, 0.55)), pol(cx, cy, r * 1.6, t + u(0.3, 0.45))]), h * u(0.72, 0.9)))
    cuts.append((disc(cx, cy, r * 0.5, 10), h * u(0.45, 0.6)))
    wp = pol(cx, cy, r * 0.82, t0 + u(1, 2)); cuts.append((disc(wp[0], wp[1], 0.24, 8), h * 0.25))
    if rng.random() < 0.5: tb = t0 + u(2.5, 4); cuts.append((Polygon([pol(cx, cy, r * 0.7, tb - 0.4), pol(cx, cy, r * 1.3, tb - 0.5), pol(cx, cy, r * 1.3, tb + 0.5), pol(cx, cy, r * 0.7, tb + 0.4)]), h * u(0.3, 0.5)))
    o = Obj("stump"); o.st(ps, cuts=cuts, foot=disc(cx, cy, r * 1.6))
    for i in range(rng.randint(1, 2)):          # bracket fungus low on the trunk
        t = t0 + u(0, 6.28); c = pol(cx, cy, r * 1.15, t); o.st(ext(ell(c[0], c[1], 0.62, 0.36, t + 1.5708, 12), h * u(0.12, 0.22)) + ext(ell(*pol(c[0], c[1], 0.12, t), 0.42, 0.24, t + 1.5708, 10), h * u(0.12, 0.22) + 0.15), "fungus", foot=disc(c[0], c[1], 0.6))
    return o.out()

@x_d("forest", "made", "log", "hollow_log")
def _(cx, cy, a, rng):
    """a hollow log lying on the forest floor, as thick as a man's waist: round, its bark furrowed along it, rotted
    open along the top so the hollow shows, a ring of saw cut or a broken end, a branch stub, a mossy patch and a few
    toadstools at its side"""
    u = rng.uniform; ff = frame(cx, cy, a); o = Obj("log"); L = u(3.9, 4.2); r = u(1.05, 1.2); x0 = -L / 2 + 0.15
    pts = [ff(x0 + L * s, 0.15 * math.sin(3 * s)) for s in np.linspace(0, 0.97, 7)]
    ps = rod(pts, lambda s: r * (1.05 - 0.12 * s) + 0.08 * math.exp(-((s - 0.3) / 0.06) ** 2), 1.9)
    p = pts[4]; t = a + rng.choice((-1, 1)) * u(1.0, 1.5); ps += rod([p, pol(p[0], p[1], r + 0.6, t)], lambda s: 0.42 - 0.1 * s, 2.0)
    hol = ell(*ff(x0 + L * u(0.05, 0.12), 0), r * 0.62, r * 0.5, a, 14) if rng.random() < 0.6 else ell(*ff(u(-0.5, 0.5), 0), L * 0.16, r * 0.35, a, 16)
    cuts = [(hol, r * 0.7), (barl(ff(x0 + L * 0.2, u(-0.2, 0.2)), ff(x0 + L * u(0.55, 0.8), u(-0.2, 0.2)), 0.16), r * 1.4)] + x_bark(pts, r, r * 1.9 - 0.16, 5, 0.12)
    if rng.random() < 0.7: c = ff(u(0.0, L * 0.3), rng.choice((-1, 1)) * r * 0.3); ps += dome(c[0], c[1], u(1.0, 1.3), r * 1.95 + 0.12, sq=0.6, a=a)          # a mossy hump, part of the log's own surface
    o.st(ps, cuts=cuts, foot=hull([LineString(pts).buffer(r), disc(*pol(p[0], p[1], r + 0.6, t), 0.4)]))
    sv = rng.choice((-1, 1))
    for i in range(rng.randint(2, 4)):
        rr = u(0.32, 0.45); hh = u(0.6, 0.9); c = ff(u(-L * 0.3, L * 0.3), sv * (r * 1.15 + rr + u(0.2, 0.35)))
        o.st(turn(c[0], c[1], [(0.18, 0.0), (0.18, hh * 0.6), (rr, hh * 0.7), (rr * 0.8, hh * 0.9), (0.2, hh)], 10), "fungus", foot=disc(c[0], c[1], rr))
    return o.out()

@x_d("forest", "debris", "twig", "fallen_branch")
def _(cx, cy, a, rng):
    """a limb come down in the wind: the main bough with its bark furrows, forking into side branches that fork again,
    a snapped end where it broke from the tree, clumps of leaves still on the outer twigs"""
    u = rng.uniform; ff = frame(cx, cy, a); o = Obj("twig"); L = u(6.3, 6.8); r = u(0.42, 0.5)
    pts = [ff(-L / 2, 0), ff(-L * 0.15, u(-0.4, 0.4)), ff(L * 0.2, u(-0.5, 0.5)), ff(L / 2 - 0.5, u(-0.7, 0.7))]; ln = LineString(pts)
    ps = rod(pts, lambda q: r * (1.0 - 0.45 * q), 1.9) + [s for v in (-0.15, 0.15) for s in cone(*pol(pts[0][0], pts[0][1], 0.15, a + 1.57 + v * 6), 0.26, r * 2.0 + 0.2, 0.08, n=5)]
    leaves = []; foot = [ln.buffer(r + 0.2)]
    for q in sorted(rng.sample([0.3, 0.45, 0.6, 0.75, 0.88], rng.randint(3, 4))):
        p = ln.interpolate(q, normalized=True); sv = rng.choice((-1, 1)); t = a + sv * u(0.5, 0.9); e = pol(p.x, p.y, u(1.4, 2.2), t)
        ps += rod([(p.x, p.y), e], lambda s: r * (0.7 - 0.4 * q) * (1 - 0.4 * s), 1.9)
        for k in range(rng.randint(1, 2)):
            e2 = pol(e[0], e[1], u(0.6, 1.0), t + rng.choice((-1, 1)) * u(0.4, 0.8)); ps += rod([e, e2], 0.3, 1.6)
            leaves.append(star(e2[0], e2[1], u(0.6, 0.8), 0.32, rng.randint(5, 7), u(0, 1))); foot.append(disc(e2[0], e2[1], 0.6))
    o.st(ps, cuts=x_bark(pts, r, r * 1.9 - 0.12, 3, 0.1), foot=hull(foot))
    if leaves: o.st(ext(U(leaves), 0.5), "leaf", foot=U(leaves))
    return o.out()


_OWN = set(globals())
