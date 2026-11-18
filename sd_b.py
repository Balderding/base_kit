"""Style-only details, part B: highland, maahl, corridor, infestation (see style_details.py)."""
import math
from sd_reg import d

# ====================================================================================================== highland
@d("highland", "made", "slate", "fallen_cross")
def _(cx, cy, a, rng):
    u = rng.uniform; ff = frame(cx, cy, a); L = u(5.0, 6.4); w = u(0.95, 1.2); c = ff(L * 0.2, 0)
    g = U([rect(cx, cy, L, w, a), rect(c[0], c[1], w, L * 0.55, a), ann(c[0], c[1], L * 0.2, L * 0.2 - 0.5, 20)])
    if rng.random() < 0.4: g = g.difference(rect(*ff(-L * u(0.1, 0.25), 0), 0.35, 3, a + u(-0.5, 0.5)))                  # snapped across the shaft
    return g.convex_hull, [ST(ext(g, u(0.6, 0.85)) + ext(disc(c[0], c[1], 0.5, 12), 1.0), "slate")]

@d("highland", "made", "earth", "peat_stack")
def _(cx, cy, a, rng):
    u = rng.uniform; ff = frame(cx, cy, a); n = rng.randint(3, 5); ps = []
    for row in range(2):
        for i in range(n - row):
            c = ff((i - (n - row - 1) / 2) * 1.25 + u(-0.1, 0.1), u(-0.1, 0.1)); ps += loaf(c[0], c[1], a + math.pi / 2 + u(-0.1, 0.1), 2.4, 1.1, 0.75, lift=row * 0.7)
    return rect(cx, cy, n * 1.25 + 0.4, 2.8, a), [ST(ps, "earth")]

@d("highland", "made", "wood", "targe")
def _(cx, cy, a, rng):
    u = rng.uniform; R = u(1.7, 2.2); n = rng.randint(6, 9)
    ps = turn(cx, cy, [(R, 0.0), (R, 0.35), (R * 0.9, 0.5), (0.7, 0.62)]) + dome(cx, cy, 0.6, 1.0)
    for p in ringpts(cx, cy, R * 0.62, n, a): ps += cyl(p[0], p[1], 0.26, 0.8, n=8)
    return disc(cx, cy, R + 0.2), [ST(ps, "wood")]

@d("highland", "made", "cloth", "bagpipes")
def _(cx, cy, a, rng):
    u = rng.uniform; ff = frame(cx, cy, a); L = u(2.6, 3.2)
    ps = spin(cx, cy, a, L, lambda t: 0.95 * math.sqrt(max(0.0, 1 - (2 * t / L) ** 2)) + 0.05, 1.0); tips = []
    for k in range(3):
        e = ff(L * 0.2 + u(2.8, 3.8), (k - 1) * u(0.7, 1.0)); ps += rod([ff(L * 0.25, (k - 1) * 0.35), e], 0.3, 1.0); tips.append(disc(e[0], e[1], 0.3))
    e = ff(-L * 0.4, -2.2); ps += rod([ff(-L * 0.3, -0.4), e], 0.28, 1.0); tips.append(disc(e[0], e[1], 0.3))
    return hull([rect(cx, cy, L, 2.0, a)] + tips), [ST(ps, "cloth")]

@d("highland", "made", "bone", "ram_skull")
def _(cx, cy, a, rng):
    u = rng.uniform; ff = frame(cx, cy, a); k = u(0.9, 1.2)
    ps = spin(cx, cy, a, 2.6 * k, lambda t: k * (0.6 - 0.2 * t / k), 0.95)
    for sv in (-1, 1): c = ff(-0.7 * k, sv * 1.15 * k); ps += rod(spiral(c[0], c[1], 0.85 * k, 0.3 * k, 1.1 * sv, a - sv * 1.57, 20), lambda q: 0.42 * k * (1 - 0.5 * q), 1.0)
    return rect(cx, cy, 2.8 * k, 4.4 * k, a), [ST(ps, "bone", cuts=[(disc(*ff(-0.2 * k, sv * 0.36 * k), 0.24 * k, 8), 0.3) for sv in (-1, 1)])]

@d("highland", "debris", "slate", "drystone_run")
def _(cx, cy, a, rng):
    u = rng.uniform; ff = frame(cx, cy, a); n = rng.randint(4, 6); ps = []
    for i in range(n):
        c = ff((i - (n - 1) / 2) * 1.5 + u(-0.15, 0.15), u(-0.2, 0.2)); ps += rock(c[0], c[1], u(0.8, 1.0), u(0.8, 1.1), rng)
        if rng.random() < 0.6 and 0 < i < n - 1: ps += rock(c[0] + 0.5, c[1], u(0.6, 0.8), u(0.6, 0.8), rng, lift=0.75)
    return rect(cx, cy, n * 1.5 + 0.6, 2.4, a), [ST(ps, "slate")]

@d("highland", "growth", "heather", "thistles")
def _(cx, cy, a, rng):
    u = rng.uniform; ps = []; foot = []
    for i in range(rng.randint(2, 4)):
        x, y = pol(cx, cy, u(0, 1.8), u(0, 6.28)); h = u(1.4, 2.6)
        ps += fluted(x, y, u(0.9, 1.2), 0.5, rng.randint(5, 7), u(0, 1), 0.4, 3) + cyl(x, y, 0.3, h, n=8) + fluted(x, y, 0.55, 0.7, 9, 0, 0.7, 4, lift=h - 0.1); foot.append(disc(x, y, 1.2))
    return hull(foot), [ST(ps, "heather")]

@d("highland", "growth", "heather", "heather_hummock")
def _(cx, cy, a, rng):
    u = rng.uniform; ps = []; foot = []
    for i in range(rng.randint(4, 7)):
        x, y = pol(cx, cy, u(0, 1.7), u(0, 6.28)); r = u(0.8, 1.3); ps += dome(x, y, r, r * u(0.6, 0.9), n=10); foot.append(disc(x, y, r))
    return hull(foot).buffer(0.2), [ST(ps, "heather")]

@d("highland", "made", "slate", "quern")
def _(cx, cy, a, rng):
    u = rng.uniform; R = u(1.5, 1.9); h = u(0.7, 0.95); off = pol(cx, cy, u(0.0, 0.9) if rng.random() < 0.5 else 0.0, a)
    ps = turn(cx, cy, [(R, 0.0), (R, h * 0.5)]) + turn(off[0], off[1], [(R * 0.92, 0.0), (R * 0.92, h), (R * 0.8, h + 0.12)]) + cyl(*pol(off[0], off[1], R * 0.62, a + 2), 0.3, h + 0.7, n=8)
    return disc(cx, cy, R + 1.0), [ST(ps, "slate", cuts=[(disc(off[0], off[1], 0.4, 10), h - 0.4)])]

@d("highland", "made", "brass", "pot_still")
def _(cx, cy, a, rng):
    u = rng.uniform; r = u(1.3, 1.6); h = u(1.9, 2.5); e = pol(cx, cy, r + u(1.8, 2.6), a); m = pol(cx, cy, r + 0.7, a + 0.25)
    ps = turn(cx, cy, [(r * 0.9, 0.0), (r, h * 0.35)] + crv(r, 0.42, h * 0.35, h * 0.85) + [(0.42, h)]) + rod([pol(cx, cy, 0.2, a), m, e], lambda q: 0.4 - 0.1 * q, 1.0, lift=0.4) + pot(e[0], e[1], 0.75, 1.1)
    return hull([disc(cx, cy, r + 0.2), disc(e[0], e[1], 1.0)]), [ST(ps, "brass")]

@d("highland", "marks", "pit", "peat_cutting", ground=True)
def _(cx, cy, a, rng):
    u = rng.uniform; ff = frame(cx, cy, a); L = u(4.0, 5.5); W = u(2.0, 2.6); lay = [("gcut", rect(cx, cy, L, W, a), 0.3, "pit"), ("gcut", rect(*ff(-L * 0.15, 0), L * 0.6, W - 0.5, a), 0.6, "pit")]
    for i in range(rng.randint(2, 4)): lay.append(("up", rect(*ff(-L / 2 + 0.8 + i * 1.3, W / 2 + 0.85), 1.1, 1.0, a + u(-0.2, 0.2)), 0.45, "earth"))
    return rect(*ff(0, 0.5), L + 0.4, W + 2.0, a), lay

@d("highland", "marks", "glyph", "cup_and_ring")
def _(cx, cy, a, rng):
    u = rng.uniform; gs = []; foot = []
    for i in range(rng.randint(1, 3)):
        x, y = pol(cx, cy, 0 if i == 0 else u(2.6, 3.4), a + i * 2.2); R = u(1.0, 1.5); gs += [disc(x, y, 0.32, 10)] + [ann(x, y, r, r - 0.3, 20) for r in np.arange(0.85, R + 0.01, 0.6)]
        gs.append(barl((x, y), pol(x, y, R + 0.5, u(0, 6.28)), 0.3)); foot.append(disc(x, y, R + 0.6))
    return hull(foot), [("gcut", U(gs), 0.3, "glyph")]

@d("highland", "made", "twig", "creel")
def _(cx, cy, a, rng):
    u = rng.uniform; r = u(1.1, 1.4); h = u(1.5, 2.0); lay = [ST(turn(cx, cy, [(r * 0.75, 0.0), (r, h * 0.9), (r + 0.12, h * 0.92), (r + 0.12, h)], 12), "twig", cuts=[(disc(cx, cy, r - 0.35, 12), h - u(0.4, 0.8))])]
    for i in range(rng.randint(0, 3)): p = pol(cx, cy, r + u(0.9, 1.6), a + i * 1.1); lay.append(ST(spin(p[0], p[1], u(0, 3.14), 1.7, lambda t: 0.36 * (1 - abs(t) / 0.9) + 0.14, 0.9), "hide"))     # the catch
    return disc(cx, cy, r + 2.0), lay

@d("highland", "made", "wood", "caber_and_stone")
def _(cx, cy, a, rng):
    u = rng.uniform; ff = frame(cx, cy, a); L = u(7.5, 10.0); c = ff(u(-2, 2), 1.9)
    return hull([rect(cx, cy, L, 1.5, a), disc(c[0], c[1], 1.1)]), [ST(spin(cx, cy, a, L, lambda t: 0.62 - 0.18 * t / L, 1.1), "wood"), ST(dome(c[0], c[1], u(0.75, 0.95), 1.0, sq=0.85, a=u(0, 3)), "slate")]

@d("highland", "growth", "grass", "tussocks")
def _(cx, cy, a, rng):
    u = rng.uniform; ps = []; foot = []
    for i in range(rng.randint(3, 6)):
        x, y = pol(cx, cy, u(0, 2.2), u(0, 6.28)); r = u(0.7, 1.1); ps += fluted(x, y, r, r * u(0.9, 1.5), rng.randint(7, 10), u(0, 1), 0.55, 5, u(-0.2, 0.2)); foot.append(disc(x, y, r))
    return hull(foot).buffer(0.2), [ST(ps, "grass")]

@d("highland", "made", "slate", "burial_kist", ground=True)
def _(cx, cy, a, rng):
    u = rng.uniform; ff = frame(cx, cy, a); L = u(3.4, 4.2); W = u(2.0, 2.5); t = 0.9
    slabs = U([rect(*ff(0, sv * (W / 2 + t / 2 - 0.1)), L + 2 * t, t, a) for sv in (-1, 1)] + [rect(*ff(sv * (L / 2 + t / 2 - 0.1), 0), t, W, a) for sv in (-1, 1)])
    cap = rect(*ff(u(1.2, L), u(-0.6, 0.6)), L * 0.8, W + 1.2, a + u(-0.4, 0.4)).difference(rect(cx, cy, L + 2 * t + 0.6, W + 2 * t + 0.6, a))
    lay = [("gcut", rect(cx, cy, L, W, a), 0.7, "pit"), ("up", slabs, u(0.45, 0.7), "slate")]
    if not cap.is_empty: lay.append(("up", max(polys_of(cap), key=lambda q: q.area), 0.6, "slate"))
    return hull([slabs, cap]), lay

@d("highland", "made", "wood", "hurdle")
def _(cx, cy, a, rng):
    u = rng.uniform; ff = frame(cx, cy, a); L = u(5.0, 6.5); W = u(3.0, 3.6); n = rng.randint(3, 4)
    g = U([barl(ff(-L / 2, v), ff(L / 2, v), 0.92) for v in np.linspace(-W / 2 + 0.46, W / 2 - 0.46, n)] + [barl(ff(sv * (L / 2 - 0.5), -W / 2 - 0.5), ff(sv * (L / 2 - 0.5), W / 2 + 0.5), 0.92) for sv in (-1, 1)] + [barl(ff(-L / 2 + 0.6, -W / 2 + 0.5), ff(L / 2 - 0.6, W / 2 - 0.5), 0.92)])
    return rect(cx, cy, L, W + 1.0, a), [("up", g, 0.45, "wood")]

@d("highland", "debris", "slate", "split_boulder")
def _(cx, cy, a, rng):
    u = rng.uniform; r = u(1.6, 2.2); h = u(1.3, 1.9); gap = barl(pol(cx, cy, 3 * r, a), pol(cx, cy, 3 * r, a + math.pi), u(0.45, 0.8))
    return disc(cx, cy, r + 0.2), [ST(rock(cx, cy, r, h, rng, 0.9, 10), "slate", cuts=[(gap, 0.0)])]

@d("highland", "made", "wood", "crook_and_plaid")
def _(cx, cy, a, rng):
    u = rng.uniform; ff = frame(cx, cy, a); L = u(6.0, 7.5); h0 = ff(L / 2, 0.6)
    ps = rod([ff(-L / 2, 0), ff(L / 2, 0)] + arc(h0[0], h0[1], 0.6, a - 1.57, a + 2.2, 8), 0.3, 1.1)
    b = blob(*ff(u(-1.5, 1.5), -1.9), u(1.3, 1.7), rng, 4)
    return hull([rect(cx, cy, L + 1.4, 1.4, a), b]), [ST(ps, "wood"), ("mound", b, u(0.5, 0.75), "cloth", SOFT_T)]

@d("highland", "made", "wood", "beacon_pyre")
def _(cx, cy, a, rng):
    u = rng.uniform; r = u(1.8, 2.4); h = u(2.4, 3.3); n = rng.randint(7, 10)
    ps = [s for k in range(6) for s in ext(star(cx, cy, r * (1 - k / 6.5), r * (1 - k / 6.5) * 0.6, n, a + 0.05 * k), h * (k + 1) / 6)]
    lay = [ST(ps, "wood")] + [("mound", stone(p[0], p[1], u(0.5, 0.65), rng, 0.85, 6), 0.45, "slate", PEB_T) for p in ringpts(cx, cy, r + 0.8, rng.randint(6, 9), a, 0.1, rng)]
    return disc(cx, cy, r + 1.5), lay

# ====================================================================================================== maahl
@d("maahl", "made", "ruin", "buried_head")
def _(cx, cy, a, rng):
    u = rng.uniform; ff = frame(cx, cy, a); r = u(1.7, 2.3); h = r * u(0.7, 0.95)
    ps = dome(cx, cy, r, h, sq=0.8, a=a) + ridge(*ff(r * 0.25, 0), a, r * 0.9, 0.7, h + 0.25) + blk(*ff(-r * 0.75, 0), 0.6, r * 1.3, a, h * 0.7)
    cuts = [(ell(*ff(r * 0.1, sv * r * 0.36), r * 0.2, r * 0.13, a, 10), h * 0.55) for sv in (-1, 1)] + [(rect(*ff(r * 0.72, 0), 0.3, r * 0.5, a), h * 0.3)]
    return disc(cx, cy, r + 0.3), [ST(ps, "ruin", cuts=cuts)]

@d("maahl", "made", "obelisk", "obelisk_tip")
def _(cx, cy, a, rng):
    u = rng.uniform; L = u(5.0, 7.0); w = u(0.8, 1.05)
    ps = spin(cx, cy, a, L, lambda t: w * (1 - 0.3 * (t + L / 2) / L) if t < L / 2 - 1.2 else w * 0.7 * max(0.0, (L / 2 - t) / 1.2), 1.0, prof="ridge")
    return rect(cx, cy, L, 2 * w + 0.3, a), [ST(ps, "obelisk")]

@d("maahl", "made", "ruin", "sundial")
def _(cx, cy, a, rng):
    u = rng.uniform; R = u(1.9, 2.5); h = u(0.5, 0.7); n = 12
    ps = turn(cx, cy, [(R, 0.0), (R, h)], 28) + ridge(*pol(cx, cy, R * 0.25, a), a, R * 0.9, 0.6, h + u(0.8, 1.2))
    cuts = [(barl(pol(cx, cy, R * 0.72, a + 6.2832 * k / n), pol(cx, cy, R * 0.94, a + 6.2832 * k / n), 0.28), h - 0.22) for k in range(n) if 2 < k < 10]
    return disc(cx, cy, R + 0.2), [ST(ps, "ruin", cuts=cuts)]

@d("maahl", "made", "clay", "amphorae")
def _(cx, cy, a, rng):
    u = rng.uniform; ps = []; foot = []
    for i in range(rng.randint(1, 3)):
        c = pol(cx, cy, 0 if i == 0 else u(1.6, 2.2), a + 1.57 + i * 3.14); b = a + u(-0.5, 0.5); L = u(3.4, 4.2); r = u(0.75, 0.95)
        ps += spin(c[0], c[1], b, L, lambda t, L=L, r=r: r * math.sin(min(1.0, (t + L / 2) / (L * 0.62)) * math.pi) ** 0.7 + 0.05 if t < L * 0.12 else (0.3 if t < L / 2 - 0.3 else 0.45), 1.15)
        f2 = frame(c[0], c[1], b); ps += [s for sv in (-1, 1) for s in rod([f2(L * 0.1, sv * 0.45), f2(L * 0.3, sv * 0.75), f2(L * 0.42, sv * 0.3)], 0.24, 1.0)]; foot.append(rect(c[0], c[1], L, 2 * r + 0.4, b))
    return hull(foot), [ST(ps, "clay")]

@d("maahl", "marks", "pan", "wind_ripples")
def _(cx, cy, a, rng):
    u = rng.uniform; ff = frame(cx, cy, a); n = rng.randint(4, 7); L = u(5.0, 7.5); ph = u(0, 6)
    gs = [line([ff(x, (i - n / 2) * 0.95 + 0.3 * math.sin(x * 1.1 + ph + i * 0.4)) for x in np.linspace(-L / 2 * (1 - 0.1 * abs(i - n / 2)), L / 2 * (1 - 0.1 * abs(i - n / 2)), 14)], 0.42) for i in range(n)]
    return rect(cx, cy, L, n * 0.95 + 0.8, a), [("gcut", U(gs), 0.28, "pan")]

@d("maahl", "made", "chitin", "scarab")
def _(cx, cy, a, rng):
    u = rng.uniform; ff = frame(cx, cy, a); L = u(2.8, 3.6); W = L * 0.68
    ps = spin(cx, cy, a, L, lambda t: W / 2 * math.sqrt(max(0.0, 1 - (2 * t / L) ** 2)) ** 0.8 + 0.05, 0.9) + dome(*ff(L / 2, 0), W * 0.3, 0.6)
    for k in (-1, 0, 1):
        for sv in (-1, 1): ps += rod([ff(k * L * 0.25, sv * W * 0.4), ff(k * L * 0.32 + 0.3 * k, sv * (W / 2 + 0.7)), ff(k * L * 0.45, sv * (W / 2 + 1.1))], 0.25, 1.0)
    return rect(cx, cy, L + 0.8, W + 2.6, a), [ST(ps, "chitin", cuts=[(barl(ff(-L / 2, 0), ff(L * 0.2, 0), 0.24), 0.75 * W / 2)])]

@d("maahl", "made", "ruin", "arch_fragment")
def _(cx, cy, a, rng):
    u = rng.uniform; R = u(3.0, 4.2); w = u(1.1, 1.4); span = u(1.2, 2.2); n = max(3, int(span * R / 1.3)); gs = []
    for k in range(n):
        t0 = a + span * k / n + 0.04; t1 = a + span * (k + 1) / n - 0.04
        gs.append(Polygon([pol(cx, cy, R, t0), pol(cx, cy, R, t1), pol(cx, cy, R - w, t1), pol(cx, cy, R - w, t0)]))
    return U(gs).convex_hull, [ST([s for q in gs for s in ext(q, u(0.7, 1.0))], "ruin")]

@d("maahl", "debris", "crystal", "geode")
def _(cx, cy, a, rng):
    u = rng.uniform; r = u(1.5, 2.0); h = u(1.0, 1.4)
    lay = [ST(rock(cx, cy, r, h, rng, 0.9), "rock", cuts=[(disc(cx, cy, r * 0.62, 12), h * 0.3)])]
    cs = [s for i in range(rng.randint(3, 5)) for s in cone(*pol(cx, cy, u(0, r * 0.3), u(0, 6.28)), u(0.35, 0.5), u(0.9, 1.6), 0.22, n=6)]
    lay.append(ST(cs, "crystal"))
    return disc(cx, cy, r + 0.3), lay

@d("maahl", "made", "cloth", "saddlebags")
def _(cx, cy, a, rng):
    u = rng.uniform; ff = frame(cx, cy, a); s = u(1.5, 1.9); ps = blk(cx, cy, 2 * s, 0.9, a, 0.45)
    for sv in (-1, 1): c = ff(sv * s, 0); ps += dome(c[0], c[1], u(1.0, 1.25), u(0.8, 1.1), sq=0.85, a=a + 1.57) + blk(c[0], c[1], 0.5, 1.6, a, 0.6)
    return rect(cx, cy, 2 * s + 2.8, 2.8, a), [ST(ps, "cloth")]

@d("maahl", "made", "brass", "oil_lamp")
def _(cx, cy, a, rng):
    u = rng.uniform; ff = frame(cx, cy, a); L = u(2.4, 3.0)
    ps = spin(cx, cy, a, L, lambda t: 0.85 * math.sqrt(max(0.0, 1 - (2 * t / L) ** 2)) + 0.05, 1.0) + rod([ff(L * 0.3, 0), ff(L / 2 + 1.1, 0)], lambda q: 0.42 - 0.14 * q, 1.2) + ext(ann(*ff(-L / 2 - 0.35, 0), 0.62, 0.28, 12), 0.5) + cyl(cx, cy, 0.34, 1.15, n=8)
    return rect(*ff(0.2, 0), L + 2.6, 2.0, a), [ST(ps, "brass")]

@d("maahl", "made", "pavers", "mosaic")
def _(cx, cy, a, rng):
    u = rng.uniform; ff = frame(cx, cy, a); n = rng.randint(3, 5); m = rng.randint(2, 4); gs = []
    for i in range(n):
        for j in range(m):
            if rng.random() < 0.22: continue
            gs.append(rect(*ff((i - (n - 1) / 2) * 1.25, (j - (m - 1) / 2) * 1.25), 1.0, 1.0, a + u(-0.08, 0.08)))
    if not gs: return None
    return rect(cx, cy, n * 1.25 + 0.3, m * 1.25 + 0.3, a), [ST([s for q in gs for s in ext(q, u(0.26, 0.36))], "pavers")]

@d("maahl", "marks", "pit", "beast_tracks")
def _(cx, cy, a, rng):
    u = rng.uniform; ff = frame(cx, cy, a); n = rng.randint(3, 5); gs = []
    for i in range(n):
        c = ff((i - (n - 1) / 2) * 2.3, 0.8 * (1 if i % 2 else -1)); gs.append(disc(c[0], c[1], 0.5, 10))
        gs += [tri(pol(c[0], c[1], 0.3, a + t), pol(c[0], c[1], 1.25, a + t), 0.5) for t in (-0.6, 0, 0.6)]
    return rect(cx, cy, n * 2.3 + 1.0, 3.6, a), [("gcut", U(gs), 0.35, "pit")]

@d("maahl", "made", "clay", "canopic_jars")
def _(cx, cy, a, rng):
    u = rng.uniform; ff = frame(cx, cy, a); n = rng.randint(2, 4); ps = []
    for i in range(n):
        c = ff((i - (n - 1) / 2) * 1.75, u(-0.2, 0.2)); r = u(0.6, 0.75); h = u(1.3, 1.8); lid = rng.randint(0, 2)
        ps += turn(c[0], c[1], [(r * 0.8, 0.0), (r, h * 0.6), (r * 0.8, h)], 14) + (dome(c[0], c[1], r * 0.8, 0.6, lift=h) if lid == 0 else cone(c[0], c[1], r * 0.8, 0.8, 0.25, lift=h) if lid == 1 else cyl(c[0], c[1], 0.3, 0.4, lift=h, n=8))
    return rect(cx, cy, n * 1.75 + 0.4, 2.0, a), [ST(ps, "clay")]

@d("maahl", "made", "ruin", "sphinx_paw")
def _(cx, cy, a, rng):
    u = rng.uniform; ff = frame(cx, cy, a); L = u(3.6, 4.6); W = u(2.6, 3.2); h = u(1.1, 1.5); n = 4
    ps = loaf(cx, cy, a, L, W, h)
    for i in range(n): c = ff(L / 2 + 0.2, (i - 1.5) * W / n); ps += dome(c[0], c[1], W / n * 0.56, h * 0.8, sq=0.7, a=a + 1.57)
    cuts = [(barl(ff(L * 0.1, (i - 1) * W / n), ff(L / 2 + 0.6, (i - 1) * W / n), 0.24), h * 0.55) for i in range(3)]
    return rect(*ff(0.3, 0), L + 1.2, W + 0.3, a), [ST(ps, "ruin", cuts=cuts)]

@d("maahl", "debris", "salt", "salt_plates")
def _(cx, cy, a, rng):
    u = rng.uniform; gs = []; foot = []
    for i in range(rng.randint(3, 6)):
        x, y = pol(cx, cy, 0 if i == 0 else u(2.0, 2.6), a + i * 1.15); q = stone(x, y, u(0.95, 1.25), rng, 0.9, rng.randint(5, 7))
        if all(q.distance(g) > 0.25 for g in foot): foot.append(q); gs.append(q.difference(inset(q, 0.5)))
    return hull(foot).buffer(0.2), [ST([s for q in gs for s in ext(q, u(0.3, 0.45))], "salt")]

@d("maahl", "made", "ruin", "stele")
def _(cx, cy, a, rng):
    u = rng.uniform; ff = frame(cx, cy, a); L = u(4.0, 5.2); W = u(2.2, 2.8); h = u(0.6, 0.85)
    g = U([rect(*ff(-W / 4, 0), L - W / 2, W, a), disc(*ff(L / 2 - W / 2, 0), W / 2, 20)])
    cuts = [(rect(*ff(x, v + u(-0.15, 0.15)), 0.26, 0.6, a + rng.choice((0.0, 0.6, -0.6))), h - 0.2) for x in np.arange(-L / 2 + 0.7, L / 2 - W / 2, 0.75) for v in np.arange(-W / 2 + 0.6, W / 2 - 0.4, 0.8) if rng.random() < 0.8]
    if rng.random() < 0.35: cuts.append((wavy(ff(u(-1, 1), -W), ff(u(-1, 1), W), rng, 0.4, 4, 0.4), 0.0))                  # broken in two
    return g.convex_hull, [ST(ext(g, h), "ruin", cuts=cuts)]

@d("maahl", "made", "wood", "well_bucket")
def _(cx, cy, a, rng):
    u = rng.uniform; r = u(0.95, 1.2); h = u(1.3, 1.7); c = pol(cx, cy, r + u(1.9, 2.4), a)
    ps = turn(cx, cy, [(r * 0.8, 0.0), (r, h)], 14) + turn(cx, cy, [(r * 0.86 + 0.1, 0.0), (r * 0.86 + 0.1, h * 0.3), (r * 0.8, h * 0.32)], 14)
    return hull([disc(cx, cy, r + 0.2), disc(c[0], c[1], 1.5)]), [ST(ps, "wood", cuts=[(disc(cx, cy, r - 0.35, 12), h - u(0.4, 0.9))]), ST(rod(spiral(c[0], c[1], 1.25, 0.4, u(1.6, 2.2), a, 30), 0.3, 1.1), "rope")]

@d("maahl", "debris", "sandstone", "desert_rose")
def _(cx, cy, a, rng):
    u = rng.uniform; ps = []; r = u(1.2, 1.7)
    for i in range(rng.randint(5, 8)): t = u(0, 3.14); c = pol(cx, cy, u(0, r * 0.5), u(0, 6.28)); ps += loaf(c[0], c[1], t, r * u(1.0, 1.6), 0.55, u(0.6, 1.2))
    return disc(cx, cy, r + 0.4), [ST(ps, "sandstone")]

@d("maahl", "made", "column", "fallen_capital")
def _(cx, cy, a, rng):
    u = rng.uniform; ff = frame(cx, cy, a); L = u(2.6, 3.4); r = u(0.9, 1.15)
    ps = spin(cx, cy, a, L, lambda t: r * (1 + 0.55 * max(0.0, (t + L * 0.1) / (L * 0.6)) ** 2), 1.1) + blk(*ff(L / 2 + 0.3, 0), 0.7, r * 3.6, a, r * 1.9)
    return rect(*ff(0.3, 0), L + 1.0, r * 3.8, a), [ST(ps, "column")]

@d("maahl", "debris", "glass", "fulgurite")
def _(cx, cy, a, rng):
    u = rng.uniform; ps = []; foot = []
    def br(p, t, L, r, dep):
        e = pol(p[0], p[1], L, t); m = pol((p[0] + e[0]) / 2, (p[1] + e[1]) / 2, u(-0.4, 0.4), t + 1.57); foot.append(disc(e[0], e[1], 0.4))
        ps.extend(rod([p, m, e], lambda q: r * (1 - 0.35 * q), 1.3))
        if dep < 2:
            for k in range(rng.randint(1, 2)): br(e if k == 0 else m, t + u(-0.9, 0.9), L * u(0.55, 0.8), r * 0.7, dep + 1)
    br((cx, cy), a, u(2.2, 3.0), u(0.45, 0.55), 0); foot.append(disc(cx, cy, 0.6))
    return hull(foot), [ST(ps, "glass")]

# ====================================================================================================== corridor
@d("corridor", "made", "metal", "gas_cylinder")
def _(cx, cy, a, rng):
    u = rng.uniform; ff = frame(cx, cy, a); L = u(4.0, 5.2); r = u(0.75, 0.95); e = r * 0.9
    ps = spin(cx, cy, a, L, lambda t: r if t < L / 2 - e else r * math.sqrt(max(0.0, 1 - ((t - L / 2 + e) / e) ** 2)) + 0.02, 1.15) + lie(*ff(L / 2 + 0.25, 0), a, 0.7, 0.4)
    h = [ff(L / 2 + 0.5, 0), ff(L / 2 + 1.4, u(-1, 1)), ff(L / 2 + u(0.5, 2.0), u(1.5, 2.4) * rng.choice((-1, 1)))]
    return hull([rect(cx, cy, L, 2 * r + 0.3, a), LineString(h).buffer(0.4)]), [ST(ps, "metal"), ST(rod(h, 0.26, 1.1), "cable")]

@d("corridor", "made", "metal", "toolbox")
def _(cx, cy, a, rng):
    u = rng.uniform; ff = frame(cx, cy, a); L = u(3.2, 4.0); W = u(1.7, 2.1); h = u(0.9, 1.2)
    ps = blk(cx, cy, L, W, a, h) + blk(*ff(0, W / 2 + 0.75), L, 1.2, a, 0.35)
    lay = [ST(ps, "metal", cuts=[(rect(cx, cy, L - 0.8, W - 0.8, a), 0.4)])]
    for i in range(rng.randint(1, 3)): c = ff(u(-L / 2, L / 2), -W / 2 - u(0.9, 1.8)); b = u(0, 3.14); lay.append(ST(ext(U([capl(pol(c[0], c[1], 1.0, b), pol(c[0], c[1], 1.0, b + math.pi), 0.5), disc(*pol(c[0], c[1], 1.0, b), 0.45, 8)]), 0.38), "metal"))
    return rect(*ff(0, -0.4), L + 0.4, W + 5.0, a), lay

@d("corridor", "made", "metal", "helmet")
def _(cx, cy, a, rng):
    u = rng.uniform; ff = frame(cx, cy, a); r = u(1.1, 1.4)
    ps = turn(cx, cy, [(r + 0.15, 0.0), (r + 0.15, 0.3), (r, 0.32)] + crv(r, 0.3, 0.32, r * 1.25), 20) + rod([ff(-r * 0.6, 0), ff(-r - u(1.0, 2.0), u(-1, 1))], 0.27, 1.0)
    vis = Polygon([ff(r * 0.2, -r * 0.6), ff(r * 1.2, -r * 0.5), ff(r * 1.2, r * 0.5), ff(r * 0.2, r * 0.6)])
    return disc(cx, cy, r + 0.4).union(disc(*ff(-r - 1.0, 0), 1.2)).convex_hull, [ST(ps, "metal", cuts=[(vis, r * u(0.45, 0.65))])]

@d("corridor", "made", "metal", "terminal")
def _(cx, cy, a, rng):
    u = rng.uniform; ff = frame(cx, cy, a); L = u(2.8, 3.6); W = u(2.2, 2.8); h = u(1.2, 1.7)
    ps = blk(cx, cy, L, W, a, h * 0.45) + ridge(*ff(-L * 0.18, 0), a + math.pi / 2, W, L * 0.64, h)
    keys = [(rect(*ff(L * 0.3, v), 0.5, 0.4, a), h * 0.45 - 0.16) for v in np.arange(-W / 2 + 0.5, W / 2 - 0.3, 0.62)]
    return rect(cx, cy, L + 0.2, W + 0.2, a), [ST(ps, "metal", cuts=keys)]

@d("corridor", "made", "plate", "deck_hatch")
def _(cx, cy, a, rng):
    u = rng.uniform; ff = frame(cx, cy, a); S = u(3.4, 4.2); R = S * 0.3; op = rng.random() < 0.35
    ps = ext(rect(cx, cy, S, S, a).buffer(-0.3).buffer(0.3), 0.42) + ext(ann(cx, cy, R, R - 0.45, 20), 0.85) + cyl(cx, cy, 0.42, 0.95, n=10)
    for k in range(3): ps += ext(barl(pol(cx, cy, 0.2, a + k * 2.094), pol(cx, cy, R - 0.2, a + k * 2.094), 0.36), 0.75)
    ps += [s for v in (-S * 0.25, S * 0.25) for s in blk(*ff(-S / 2, v), 0.6, 0.8, a, 0.7)]
    return rect(*ff(0.3, 0), S + 1.6, S + 0.2, a), [ST(ps, "plate")] + ([("gcut", rect(*ff(S / 2 + 0.6, 0), 0.6, S * 0.8, a), 0.6, "slot")] if op else [])

@d("corridor", "made", "metal", "cell_rack")
def _(cx, cy, a, rng):
    u = rng.uniform; ff = frame(cx, cy, a); n = rng.randint(3, 5); ps = blk(cx, cy, n * 1.3 + 0.4, 1.7, a, 0.5); miss = rng.randint(0, n)
    for i in range(n):
        if i == miss: continue
        c = ff((i - (n - 1) / 2) * 1.3, 0); h = u(1.4, 1.9); ps += turn(c[0], c[1], [(0.5, 0.0), (0.5, h), (0.3, h + 0.05), (0.3, h + 0.3)], 12)
    return rect(cx, cy, n * 1.3 + 0.6, 1.9, a), [ST(ps, "metal")]

@d("corridor", "made", "cable", "hose_reel")
def _(cx, cy, a, rng):
    u = rng.uniform; R = u(1.6, 2.1); e = pol(cx, cy, R + u(2.0, 3.5), a + u(-0.4, 0.4))
    ps = turn(cx, cy, [(R, 0.0), (R, 0.35)]) + rod(spiral(cx, cy, 0.7, R - 0.4, u(1.8, 2.6), a, 36), 0.33, 1.0, lift=0.3) + cyl(cx, cy, 0.5, 1.0, n=10)
    return hull([disc(cx, cy, R + 0.2), disc(e[0], e[1], 0.5)]), [ST(ps, "metal"), ST(rod(wiggle(pol(cx, cy, R + 0.2, a), e, rng, 0.6, 4), 0.3, 1.1), "cable")]

@d("corridor", "made", "metal", "junction_box")
def _(cx, cy, a, rng):
    u = rng.uniform; S = u(1.8, 2.3); h = u(0.9, 1.3); ps = blk(cx, cy, S, S, a, h); lay = []; foot = [rect(cx, cy, S + 0.3, S + 0.3, a)]
    for k in rng.sample(range(4), rng.randint(2, 3)):
        t = a + k * 1.5708; p0 = pol(cx, cy, S / 2 - 0.3, t); e = pol(cx, cy, S / 2 + u(2.0, 3.2), t + u(-0.3, 0.3)); ps += rod(wiggle(p0, e, rng, 0.4, 3), 0.3, 1.1); foot.append(disc(e[0], e[1], 0.4))          # one surface with the box: nothing meets edge to edge
    return hull(foot), [ST(ps, "metal", cuts=[(rect(cx, cy, S - 0.8, S - 0.8, a), h - 0.3)])] + lay

@d("corridor", "made", "metal", "magboots")
def _(cx, cy, a, rng):
    u = rng.uniform; ps = []; foot = []
    for sv in (-1, 1):
        c = pol(cx, cy, u(0.9, 1.4), a + sv * 1.57); b = a + u(-0.7, 0.7); f2 = frame(c[0], c[1], b)
        ps += loaf(c[0], c[1], b, 2.6, 1.2, 0.6) + cyl(*f2(-0.7, 0), 0.55, u(1.3, 1.8), n=12) + blk(*f2(0.3, 0), 0.4, 1.3, b, 0.75); foot.append(rect(c[0], c[1], 2.8, 1.4, b))
    return hull(foot), [ST(ps, "metal")]

@d("corridor", "made", "metal", "vent_fan")
def _(cx, cy, a, rng):
    u = rng.uniform; R = u(1.8, 2.3); n = rng.randint(4, 6)
    ps = ext(ann(cx, cy, R, R - 0.5, 24), 0.7) + ext(disc(cx, cy, R - 0.4, 24), 0.2) + cyl(cx, cy, 0.5, 0.75, n=10)
    for k in range(n):
        if rng.random() < 0.15: continue
        t = a + 6.2832 * k / n; ps += ext(Polygon([pol(cx, cy, 0.4, t - 0.3), pol(cx, cy, R - 0.6, t - 0.12), pol(cx, cy, R - 0.6, t + 0.42), pol(cx, cy, 0.4, t + 0.3)]), 0.5)
    return disc(cx, cy, R + 0.2), [ST(ps, "metal")]

@d("corridor", "made", "metal", "hover_pallet")
def _(cx, cy, a, rng):
    u = rng.uniform; ff = frame(cx, cy, a); L = u(4.4, 5.6); W = u(3.0, 3.6)
    ps = ext(rect(cx, cy, L, W, a).buffer(-0.4).buffer(0.4), 0.5) + [s for sx in (-1, 1) for sy in (-1, 1) for s in cyl(*ff(sx * (L / 2 - 0.7), sy * (W / 2 - 0.7)), 0.5, 0.75, n=10)]
    cuts = [(line([ff(x - 0.5, -W * 0.25), ff(x, 0), ff(x - 0.5, W * 0.25)], 0.3), 0.3) for x in np.arange(-L / 2 + 1.6, L / 2 - 1.2, 0.8)]
    return rect(cx, cy, L + 0.2, W + 0.2, a), [ST(ps, "metal", cuts=cuts)]

@d("corridor", "made", "metal", "dead_drone")
def _(cx, cy, a, rng):
    u = rng.uniform; r = u(0.9, 1.15); arm = u(1.9, 2.4); ps = dome(cx, cy, r, 0.8) + cyl(cx, cy, r + 0.12, 0.3); foot = [disc(cx, cy, r)]
    for k in range(4):
        t = a + 0.785 + k * 1.5708; e = pol(cx, cy, arm * (0.55 if rng.random() < 0.2 else 1.0), t); ps += ext(barl(pol(cx, cy, r * 0.5, t), e, 0.5), 0.4) + ext(ann(e[0], e[1], 0.75, 0.3, 12), 0.5); foot.append(disc(e[0], e[1], 0.8))
    return hull(foot), [ST(ps, "metal")]

@d("corridor", "made", "metal", "welding_set")
def _(cx, cy, a, rng):
    u = rng.uniform; ff = frame(cx, cy, a); L = u(3.6, 4.4); r = 0.6
    ps = lie(*ff(0, -r), a, L, r) + lie(*ff(0, r), a, L * 0.9, r) + blk(cx, cy, 0.5, 4 * r + 0.3, a, 0.8) + lie(*ff(L / 2 + 0.2, -r), a, 0.5, 0.3) + lie(*ff(L / 2 + 0.2, r), a, 0.5, 0.3)
    e = ff(L / 2 + u(1.5, 2.6), u(-2.2, 2.2)); h = wiggle(ff(L / 2 + 0.7, 0), e, rng, 0.7, 4)
    return hull([rect(cx, cy, L + 0.6, 4 * r + 0.4, a), disc(e[0], e[1], 0.8)]), [ST(ps, "metal"), ST(rod(h, 0.26, 1.1) + cone(e[0], e[1], 0.42, 0.7, 0.24), "cable")]

@d("corridor", "made", "pipe", "valve_stub")
def _(cx, cy, a, rng):
    u = rng.uniform; ff = frame(cx, cy, a); L = u(3.6, 4.8); r = u(0.6, 0.75); R = r + 0.75
    ps = lie(cx, cy, a, L, r) + lie(*ff(-L / 2 + 0.2, 0), a, 0.45, r + 0.3) + lie(*ff(L / 2 - 0.2, 0), a, 0.45, r + 0.3) + cyl(cx, cy, 0.35, r * 1.15 + 0.9, n=8) + hang(ann(cx, cy, R, R - 0.42, 16).union(barl(pol(cx, cy, R, a + 0.8), pol(cx, cy, R, a + 0.8 + math.pi), 0.36)), r * 1.15 + 0.5, r * 1.15 + 0.9)
    return rect(cx, cy, L + 0.2, 2 * R + 0.3, a), [ST(ps, "pipe")]

@d("corridor", "marks", "slot", "deck_stencil")
def _(cx, cy, a, rng):
    u = rng.uniform; ff = frame(cx, cy, a); gs = []; x = -2.6
    segs = {"0": "abcdef", "1": "bc", "2": "abged", "3": "abgcd", "4": "fgbc", "5": "afgcd", "7": "abc", "8": "abcdefg", "9": "abfgcd"}
    S = dict(a=((0, 1.4), (0.9, 1.4)), b=((0.9, 1.4), (0.9, 0)), c=((0.9, 0), (0.9, -1.4)), d=((0, -1.4), (0.9, -1.4)), e=((0, 0), (0, -1.4)), f=((0, 1.4), (0, 0)), g=((0, 0), (0.9, 0)))
    for ch in [rng.choice(list(segs)) for _ in range(rng.randint(2, 3))]:
        gs += [barl(ff(x + S[k][0][0], S[k][0][1]), ff(x + S[k][1][0], S[k][1][1]), 0.34) for k in segs[ch]]; x += 1.7
    gs.append(line([ff(x + 0.2, -1.2), ff(x + 1.3, 0), ff(x + 0.2, 1.2)], 0.36))
    return rect(*ff((x - 1.2) / 2, 0), x + 4.6, 3.6, a), [("gcut", U(gs), 0.25, "slot")]

@d("corridor", "marks", "coolant", "drip_tray")
def _(cx, cy, a, rng):
    u = rng.uniform; p = blob(cx, cy, u(1.6, 2.2), rng, 4); S = u(1.4, 1.8); f2 = frame(cx + u(-0.4, 0.4), cy + u(-0.4, 0.4), a)
    drain = U([barl(f2(-S / 2, v), f2(S / 2, v), 0.3) for v in np.arange(-S / 2 + 0.3, S / 2, 0.55)]).intersection(inset(p, 0.3))
    return p.buffer(0.3), [("gcut", p, 0.3, "coolant")] + ([("gcut", drain, 0.7, "slot")] if not drain.is_empty else [])

@d("corridor", "made", "glass", "cryo_canister")
def _(cx, cy, a, rng):
    u = rng.uniform; ff = frame(cx, cy, a); L = u(3.4, 4.4); r = u(0.85, 1.05); e = r
    ps = spin(cx, cy, a, L, lambda t: r * (1.0 if abs(t) < L / 2 - e else math.sqrt(max(0.0, 1 - ((abs(t) - L / 2 + e) / e) ** 2))) + (0.12 if abs(abs(t) - L * 0.3) < 0.2 else 0.0) + 0.02, 1.15)
    ps += [s for sv in (-1, 1) for s in blk(*ff(sv * L * 0.3, 0), 0.6, 2 * r + 0.9, a, 0.45)]
    return rect(cx, cy, L + 0.3, 2 * r + 1.0, a), [ST(ps, "glass", cuts=[(rect(cx, cy, L * 0.32, r * 0.9, a).buffer(0.15), r * 1.15 * 0.75)])]

@d("corridor", "made", "cable", "cable_spool")
def _(cx, cy, a, rng):
    u = rng.uniform; ff = frame(cx, cy, a); R = u(1.3, 1.7); W = u(1.6, 2.2)
    ps = lie(cx, cy, a, W, R * 0.6) + [s for sv in (-1, 1) for s in lie(*ff(sv * W / 2, 0), a, 0.42, R)]
    e = ff(u(-0.5, 0.5), -R - u(1.5, 3.0))
    return hull([rect(cx, cy, W + 0.6, 2 * R + 0.2, a), disc(e[0], e[1], 0.4)]), [ST(ps, "metal"), ST(rod(wiggle(ff(0, -R - 0.3), e, rng, 0.5, 3), 0.28, 1.1), "cable")]

@d("corridor", "made", "plate", "plate_stack")
def _(cx, cy, a, rng):
    u = rng.uniform; r = u(1.5, 1.9); ps = []; n = rng.randint(2, 4)
    for i in range(n): ps += ext(ngon(cx + u(-0.35, 0.35) * (i > 0), cy + u(-0.35, 0.35) * (i > 0), r, 6, a + u(-0.3, 0.3) * (i > 0)), 0.36, lift=i * 0.34)
    return disc(cx, cy, r + 0.5), [ST(ps, "plate")]

@d("corridor", "made", "tin", "mess_tray")
def _(cx, cy, a, rng):
    u = rng.uniform; ff = frame(cx, cy, a); L = u(2.8, 3.4); W = u(2.0, 2.4); c = ff(L / 2 + u(0.9, 1.5), u(-1, 1))
    cuts = [(rect(*ff(-L * 0.22, 0), L * 0.38, W - 0.7, a), 0.2), (rect(*ff(L * 0.24, W * 0.2), L * 0.34, W * 0.36, a), 0.2), (rect(*ff(L * 0.24, -W * 0.22), L * 0.34, W * 0.32, a), 0.2)]
    return hull([rect(cx, cy, L, W, a), disc(c[0], c[1], 0.8)]), [ST(ext(rect(cx, cy, L, W, a).buffer(-0.3).buffer(0.3), 0.42), "tin", cuts=cuts), ST(turn(c[0], c[1], [(0.5, 0.0), (0.6, 0.9)], 12), "tin", cuts=[(disc(c[0], c[1], 0.32, 8), 0.4)])]

# ====================================================================================================== infestation
@d("infestation", "growth", "web", "cocoon")
def _(cx, cy, a, rng):
    u = rng.uniform; ff = frame(cx, cy, a); L = u(5.0, 6.5); r = u(1.0, 1.3); ph = u(0, 6)
    ps = spin(cx, cy, a, L, lambda t: r * math.sqrt(max(0.0, 1 - (2 * t / L) ** 2)) ** 0.7 * (1 + 0.1 * math.sin(t * 5 + ph)) + 0.05, 1.0)
    for i in range(rng.randint(2, 4)): t = u(0, 6.28); ps += rod([pol(cx, cy, r * 0.5, t), pol(cx, cy, r + u(1.6, 2.6), t)], lambda q: 0.34 - 0.1 * q, 0.9)
    lay = [ST(ps, "web")]
    if rng.random() < 0.6: lay.append(ST(loaf(*ff(L / 2 + 0.9, u(-0.3, 0.3)), a, 1.5, 0.75, 0.6), "metal"))            # a boot left sticking out
    return rect(cx, cy, L + 3.4, 2 * r + 3.0, a), lay

@d("infestation", "growth", "egg", "burst_egg")
def _(cx, cy, a, rng):
    u = rng.uniform; r = u(1.0, 1.35); h = r * u(1.3, 1.7); n = 4
    ps = turn(cx, cy, [(r * 0.8, 0.0), (r, h * 0.4), (r * 0.75, h * 0.85), (r * 0.6, h)], 16)
    cuts = [(disc(cx, cy, r * 0.42, 10), h * 0.35)] + [(barl((cx, cy), pol(cx, cy, r, a + k * 6.2832 / n), 0.26), h * 0.6) for k in range(n)]
    return disc(cx, cy, r + 0.3), [ST(ps, "egg", cuts=cuts)]

@d("infestation", "made", "carapace", "dead_crawler")
def _(cx, cy, a, rng):
    u = rng.uniform; ff = frame(cx, cy, a); L = u(2.6, 3.4); W = L * 0.6; n = rng.randint(3, 4)
    ps = spin(cx, cy, a, L, lambda t: W / 2 * math.sqrt(max(0.0, 1 - (2 * t / L) ** 2)) + 0.05, 0.85) + rod([ff(-L / 2, 0), ff(-L / 2 - 1.2, u(-0.6, 0.6)), ff(-L / 2 - 2.2, u(-1.2, 1.2))], lambda q: 0.36 - 0.14 * q, 1.0)
    for k in range(n):
        for sv in (-1, 1):
            x = (k - (n - 1) / 2) * L * 0.24; curl = u(0.2, 0.9)
            ps += rod([ff(x, sv * W * 0.3), ff(x + 0.2, sv * (W / 2 + 0.8)), ff(x + curl, sv * (W / 2 + 0.3 + curl))], lambda q: 0.3 - 0.07 * q, 1.0)
    return rect(*ff(-1.0, 0), L + 2.6, W + 2.6, a), [ST(ps, "carapace")]

@d("infestation", "growth", "resin", "resin_stump")
def _(cx, cy, a, rng):
    u = rng.uniform; r = u(1.2, 1.7); h = u(1.6, 2.6); n = rng.randint(5, 8); tw = u(0.12, 0.3) * rng.choice((-1, 1))
    ps = [s for k in range(7) for s in ext(star(cx, cy, r * (1 - 0.07 * k), r * (1 - 0.07 * k) * 0.7, n, a + tw * k), h * (k + 1) / 7)]
    return disc(cx, cy, r + 0.2), [ST(ps, "resin", cuts=[(disc(cx, cy, r * 0.25, 8), h - 0.4)])]

@d("infestation", "growth", "spore", "spore_chimneys")
def _(cx, cy, a, rng):
    u = rng.uniform; ps = []; cuts = []; foot = []
    for i in range(rng.randint(2, 4)):
        x, y = pol(cx, cy, 0 if i == 0 else u(1.6, 2.2), a + i * 2.1); r = u(0.8, 1.2) * (1 if i == 0 else 0.75); h = r * u(1.6, 2.4)
        ps += turn(x, y, [(r, 0.0), (r * 0.6, h * 0.5), (r * 0.5, h)], 12); cuts.append((disc(x, y, r * 0.28, 8), h - 0.6)); foot.append(disc(x, y, r))
    return hull(foot).buffer(0.2), [ST(ps, "spore", cuts=cuts)]

@d("infestation", "marks", "acid", "acid_holes")
def _(cx, cy, a, rng):
    u = rng.uniform; g = U([blob(*pol(cx, cy, u(0, 2.2), u(0, 6.28)), u(0.5, 0.95), rng, 3) for i in range(rng.randint(3, 6))])
    return g.convex_hull.buffer(0.3), [("gcut", g, u(0.4, 0.7), "acid")]

@d("infestation", "growth", "flesh", "hive_vent", ground=True)
def _(cx, cy, a, rng):
    u = rng.uniform; R = u(1.5, 2.0); n = rng.randint(6, 9); ps = []
    for k in range(n): p = pol(cx, cy, R, a + 6.2832 * k / n); ps += dome(p[0], p[1], R * 3.4 / n, u(0.7, 1.0), sq=0.7, a=a + 6.2832 * k / n)
    return disc(cx, cy, R + 1.0), [("gcut", disc(cx, cy, R - 0.9, 14), u(0.6, 0.85), "pit"), ST(ps, "flesh")]

@d("infestation", "made", "carapace", "molt")
def _(cx, cy, a, rng):
    u = rng.uniform; ff = frame(cx, cy, a); n = rng.randint(4, 6); ps = []; x = -n * 0.5
    for i in range(n):
        w = u(2.0, 2.4) * (1 - 0.12 * i); c = ff(x, u(-0.1, 0.1)); ps += loaf(c[0], c[1], a + math.pi / 2, w, 1.1, 0.9 * (1 - 0.1 * i)); x += 0.9
    return rect(cx, cy, n * 0.9 + 1.4, 2.6, a), [ST(ps, "carapace", cuts=[(barl(ff(-n * 0.5 - 0.6, 0), ff(n * 0.2, 0), 0.3), 0.3)])]

@d("infestation", "growth", "tendril", "tendril_knot")
def _(cx, cy, a, rng):
    u = rng.uniform; ps = dome(cx, cy, u(0.9, 1.2), u(0.9, 1.2)); foot = []
    for i in range(rng.randint(4, 6)):
        t = a + i * 1.2 + u(-0.3, 0.3); e = pol(cx, cy, u(2.4, 3.6), t + u(-0.5, 0.5)); m = pol(cx, cy, 1.5, t + u(-0.5, 0.5)); ps += rod([pol(cx, cy, 0.3, t), m, e], lambda q: 0.5 - 0.27 * q, 1.0); foot.append(disc(e[0], e[1], 0.3))
    return hull(foot), [ST(ps, "tendril")]

@d("infestation", "marks", "slot", "claw_marks")
def _(cx, cy, a, rng):
    u = rng.uniform; ff = frame(cx, cy, a); n = rng.randint(3, 4); L = u(4.0, 6.0); bend = u(-0.8, 0.8)
    gs = [U([tri(ff(0, (i - (n - 1) / 2) * 0.85), ff(L / 2, (i - (n - 1) / 2) * 0.7 + bend), 0.5), tri(ff(0, (i - (n - 1) / 2) * 0.85), ff(-L / 2 * u(0.7, 1.0), (i - (n - 1) / 2) * 0.95), 0.5)]) for i in range(n)]
    return rect(cx, cy, L + 0.4, n * 0.9 + 1.8, a), [("gcut", U(gs), 0.4, "slot")]

@d("infestation", "growth", "pustule", "sac_cluster")
def _(cx, cy, a, rng):
    u = rng.uniform; ps = []; foot = []
    for i in range(rng.randint(3, 6)):
        x, y = pol(cx, cy, u(0, 1.6), u(0, 6.28)); r = u(0.6, 1.1); ps += dome(x, y, r, r * u(0.9, 1.3), n=12); foot.append(disc(x, y, r))
    for i in range(rng.randint(2, 4)): t = u(0, 6.28); e = pol(cx, cy, u(2.6, 3.4), t); ps += rod(wiggle(pol(cx, cy, 0.8, t), e, rng, 0.3, 3), lambda q: 0.36 - 0.12 * q, 0.9); foot.append(disc(e[0], e[1], 0.3))
    return hull(foot), [ST(ps, "pustule")]

@d("infestation", "made", "metal", "flamethrower")
def _(cx, cy, a, rng):
    u = rng.uniform; ff = frame(cx, cy, a); L = u(2.6, 3.2)
    ps = lie(*ff(0, -0.5), a, L, 0.5) + lie(*ff(0, 0.5), a, L, 0.5) + blk(cx, cy, 0.5, 2.3, a, 0.7)
    g0 = ff(L / 2 + u(2.6, 3.2), u(1.6, 2.6) * rng.choice((-1, 1))); b = a + u(-0.4, 0.4); f2 = frame(g0[0], g0[1], b)
    gun = lie(g0[0], g0[1], b, 3.4, 0.36) + blk(*f2(-0.9, -0.5), 0.5, 1.0, b, 0.5) + cone(*f2(1.9, 0), 0.5, 0.6, 0.3)
    return hull([rect(cx, cy, L, 2.4, a), rect(g0[0], g0[1], 4.2, 1.8, b)]), [ST(ps + gun + rod(wiggle(ff(L / 2 - 0.2, 0), f2(-1.6, 0), rng, 0.4, 3), 0.26, 1.1), "metal")]

@d("infestation", "made", "metal", "scanner_and_mags")
def _(cx, cy, a, rng):
    u = rng.uniform; ff = frame(cx, cy, a); L = u(2.0, 2.5); W = u(1.4, 1.7)
    lay = [ST(blk(cx, cy, L, W, a, 0.6) + blk(*ff(-L / 2 - 0.4, 0), 0.9, 0.6, a, 0.45), "metal", cuts=[(rect(*ff(0.15, 0), L * 0.55, W - 0.6, a), 0.38)])]
    for i in range(rng.randint(1, 3)): c = ff(-1.5 + i * 1.9, W / 2 + u(1.3, 2.0)); lay.append(ST(blk(c[0], c[1], 1.5, 0.6, a + u(-0.5, 0.5), 0.42), "metal"))
    return rect(*ff(0.3, 1.0), L + 3.6, W + 3.6, a), lay

@d("infestation", "marks", "acid", "slime_trail")
def _(cx, cy, a, rng):
    u = rng.uniform; ff = frame(cx, cy, a); L = u(6.0, 9.0); pts = wiggle(ff(-L / 2, 0), ff(L / 2, 0), rng, 1.0, 6)
    g = U([LineString(pts).buffer(u(0.45, 0.6), 6)] + [disc(p[0] + u(-0.5, 0.5), p[1] + u(-0.5, 0.5), u(0.5, 0.8), 10) for p in pts[1:-1:2]])
    return g.convex_hull, [("gcut", g, 0.28, "acid")]

@d("infestation", "debris", "spine", "spine_row")
def _(cx, cy, a, rng):
    u = rng.uniform; R = u(3.0, 5.0); n = rng.randint(4, 7); ps = []; half = 0.75 * (n - 1) / R; pts = arc(cx - R * math.cos(a), cy - R * math.sin(a), R, a - half, a + half, n - 1)
    for i, p in enumerate(pts): k = 1 - abs(i - (n - 1) / 2) / n; ps += cone(p[0], p[1], 0.6 * (0.6 + 0.6 * k), u(1.6, 2.8) * (0.5 + 0.7 * k), 0.23, n=7, sq=0.8, a=a)
    return LineString(pts).buffer(1.0), [ST(ps, "spine")]

@d("infestation", "growth", "flesh", "larvae")
def _(cx, cy, a, rng):
    u = rng.uniform; ps = []; foot = []
    for i in range(rng.randint(4, 8)):
        x, y = pol(cx, cy, u(0, 2.6), u(0, 6.28)); b = u(0, 6.28); L = u(1.4, 2.0); ph = u(0, 3)
        ps += spin(x, y, b, L, lambda t, L=L, ph=ph: 0.42 * math.sqrt(max(0.0, 1 - (2 * t / L) ** 2)) * (1 + 0.18 * math.sin(t * 9 + ph)) + 0.05, 1.0); foot.append(disc(x, y, L / 2))
    return hull(foot).buffer(0.2), [ST(ps, "flesh")]

@d("infestation", "growth", "web", "web_anchor")
def _(cx, cy, a, rng):
    u = rng.uniform; ps = dome(cx, cy, 0.7, 0.8); foot = []; n = rng.randint(5, 8)
    for k in range(n):
        t = a + 6.2832 * k / n + u(-0.25, 0.25); e = pol(cx, cy, u(2.6, 4.2), t); ps += rod([pol(cx, cy, 0.3, t), e], lambda q: 0.34 - 0.1 * q, 0.9); foot.append(disc(e[0], e[1], 0.3))
        if rng.random() < 0.5: t2 = a + 6.2832 * (k + 1) / n; ps += rod([pol(cx, cy, 1.8, t), pol(cx, cy, 1.8, t2)], 0.25, 0.9)
    return hull(foot), [ST(ps, "web")]

@d("infestation", "made", "carapace", "mandibles")
def _(cx, cy, a, rng):
    u = rng.uniform; ff = frame(cx, cy, a); L = u(3.4, 4.6); ps = []
    for sv in (-1, 1)[:2 if rng.random() < 0.75 else 1]:
        ps += rod([ff(-L / 2, sv * 0.5), ff(-L * 0.1, sv * 1.5), ff(L * 0.3, sv * 1.3), ff(L / 2, sv * 0.3)], lambda q: 0.6 - 0.36 * q, 1.0)
        for x in (-L * 0.05, L * 0.15, L * 0.32): c = ff(x, sv * 0.75); ps += cone(c[0], c[1], 0.32, 0.6, 0.22, n=6)
    return rect(cx, cy, L + 0.4, 3.8, a), [ST(ps, "carapace")]

@d("infestation", "growth", "egg", "egg_ledge")
def _(cx, cy, a, rng):
    u = rng.uniform; R = u(2.6, 3.6); span = u(1.2, 2.0); pts = arc(cx, cy, R, a, a + span, 12); n = rng.randint(3, 5)
    ps = rod(pts, 0.75, 0.7)
    for k in range(n): p = pol(cx, cy, R, a + span * (k + 0.5) / n); ps += dome(p[0], p[1], u(0.5, 0.65), u(0.9, 1.3), lift=0.4, sq=0.85, a=u(0, 3))
    return LineString(pts).buffer(1.0), [ST(ps, "egg")]

@d("infestation", "marks", "pit", "hive_pores")
def _(cx, cy, a, rng):
    u = rng.uniform; r = u(0.6, 0.8); gs = []; s = r * 2 * 0.866 + 0.38
    for i in range(-2, 3):
        for j in range(-2, 3):
            x = cx + (i + 0.5 * (j % 2)) * s; y = cy + j * s * 0.866
            if math.dist((x, y), (cx, cy)) < u(2.0, 3.2): gs.append(ngon(x, y, r, 6, 0.5236))
    if not gs: return None
    return hull(gs).buffer(0.3), [("gcut", U(gs), u(0.35, 0.6), "pit")]


_OWN = set(globals())
