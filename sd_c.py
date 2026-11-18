"""Style-only details, part C: urban, mine, swamp, cynder, targa (see style_details.py)."""
import math
from sd_reg import d

# ====================================================================================================== urban
@d("urban", "made", "iron", "hydrant")
def _(cx, cy, a, rng):
    u = rng.uniform; ff = frame(cx, cy, a); r = u(0.75, 0.9); h = u(2.0, 2.6)
    if rng.random() < 0.4:                                                            # knocked flat, water still running
        L = h + 0.6; ps = spin(cx, cy, a, L, lambda t: r * (1.25 if t < -L / 2 + 0.4 else 1.0 if t < L / 2 - r else math.sqrt(max(0.0, 1 - ((t - L / 2 + r) / r) ** 2))) + 0.02, 1.1) + cyl(*ff(L * 0.1, 0), 0.4, r * 1.1 + 0.45, n=8)
        p = blob(*ff(-L / 2 - 1.2, 0), u(1.1, 1.6), rng, 3)
        return hull([rect(cx, cy, L, 2 * r + 0.6, a), p]), [ST(ps, "iron"), ("gcut", p, 0.28, "water")]
    ps = turn(cx, cy, [(r * 1.3, 0.0), (r * 1.3, 0.3), (r, 0.32), (r, h * 0.75)] + crv(r, 0.3, h * 0.75, h), 16) + lie(*ff(r, 0), a, 1.0, 0.42, lift=h * 0.35) + lie(*ff(0, r), a + 1.57, 0.9, 0.36, lift=h * 0.4) + lie(*ff(0, -r), a + 1.57, 0.9, 0.36, lift=h * 0.4)
    return disc(cx, cy, r + 0.9), [ST(ps, "iron")]

@d("urban", "made", "iron", "street_lamp")
def _(cx, cy, a, rng):
    u = rng.uniform; ff = frame(cx, cy, a); L = u(8.0, 11.0); e = ff(L / 2, 0)
    ps = spin(cx, cy, a, L, lambda t: 0.5 - 0.16 * (t + L / 2) / L + (0.3 if t < -L / 2 + 0.7 else 0.0), 1.1) + turn(e[0], e[1], [(0.95, 0.0), (0.95, 0.5)] + crv(0.95, 0.3, 0.5, 1.2), 8)
    lay = [ST(ps, "iron")] + [("up", stone(*ff(L / 2 + u(1.2, 2.0), u(-1.2, 1.2)), u(0.5, 0.65), rng, 0.6, 3), 0.3, "glass") for i in range(rng.randint(1, 3))]
    return rect(*ff(0.8, 0), L + 3.2, 3.0, a), lay

@d("urban", "made", "tar", "tyres")
def _(cx, cy, a, rng):
    u = rng.uniform; R = u(1.3, 1.6); ps = []; n = rng.randint(1, 3); x, y = cx, cy
    for i in range(n): ps += rod(arc(x, y, R, 0, 6.2832, 20), 0.55, 1.1, lift=i * 0.95); x += u(-0.35, 0.35); y += u(-0.35, 0.35)
    return disc(cx, cy, R + 1.0), [ST(ps, "tar")]

@d("urban", "made", "iron", "post_box")
def _(cx, cy, a, rng):
    u = rng.uniform; ff = frame(cx, cy, a); L = u(3.4, 4.2); W = u(1.8, 2.2)
    ps = loaf(cx, cy, a, L, W, W * 0.55) + blk(*ff(-L / 2 - 0.25, 0), 0.5, W + 0.3, a, W * 0.45)
    cuts = [(rect(*ff(L * 0.22, 0), 0.35, W * 0.6, a), W * 0.55 - 0.22), (rect(*ff(-L * 0.1, 0), L * 0.3, W * 0.5, a), W * 0.55 - 0.14)]
    lay = [ST(ps, "iron", cuts=cuts)] + [("up", rect(*ff(L / 2 + u(0.9, 1.8), u(-1.2, 1.2)), 1.3, 1.0, u(0, 3)), 0.25, "paper") for i in range(rng.randint(0, 3))]
    return rect(*ff(0.8, 0), L + 3.6, W + 1.6, a), lay

@d("urban", "made", "tin", "dustbin")
def _(cx, cy, a, rng):
    u = rng.uniform; ff = frame(cx, cy, a); L = u(3.0, 3.6); r = u(1.0, 1.2); c = ff(L / 2 + u(1.4, 2.2), u(-1.5, 1.5))
    ps = spin(cx, cy, a, L, lambda t: r * (0.88 + 0.12 * (t + L / 2) / L) + 0.07 * (math.sin(t * 7) > 0.3), 1.12)
    lid = turn(c[0], c[1], [(r + 0.15, 0.0), (r + 0.15, 0.25), (0.5, 0.5)], 16) + blk(c[0], c[1], 0.9, 0.4, a, 0.8)
    return hull([rect(cx, cy, L, 2 * r + 0.3, a), disc(c[0], c[1], r + 0.3)]), [ST(ps, "tin"), ST(lid, "tin")]

@d("urban", "made", "brick", "brick_pallet")
def _(cx, cy, a, rng):
    u = rng.uniform; ff = frame(cx, cy, a); n = rng.randint(3, 4); m = rng.randint(2, 3); ps = blk(cx, cy, n * 1.25 + 0.5, m * 0.75 + 0.5, a, 0.3)
    for lv in range(rng.randint(1, 3)):
        for i in range(n):
            for j in range(m):
                if rng.random() < 0.15 * (lv + 1): continue
                c = ff((i - (n - 1) / 2) * 1.25, (j - (m - 1) / 2) * 0.75); ps += blk(c[0], c[1], 1.1, 0.62, a, 0.5, lift=0.28 + lv * 0.48)
    return rect(cx, cy, n * 1.25 + 0.8, m * 0.75 + 0.8, a), [ST(ps, "brick")]

@d("urban", "made", "concrete", "storm_drain", ground=True)
def _(cx, cy, a, rng):
    u = rng.uniform; ff = frame(cx, cy, a); L = u(4.0, 5.2)
    slots = U([rect(*ff(x, -1.0), 0.4, 1.3, a) for x in np.arange(-L / 2 + 0.7, L / 2 - 0.4, 0.8)])
    return rect(*ff(0, -0.4), L, 2.9, a), [("up", rect(*ff(0, 0.45), L, 1.1, a), u(0.5, 0.7), "concrete"), ("gcut", slots, 0.7, "slot")]

@d("urban", "made", "glass", "bottles")
def _(cx, cy, a, rng):
    u = rng.uniform; lay = []; foot = []
    for i in range(rng.randint(2, 5)):
        x, y = pol(cx, cy, u(0.3, 2.6), a + i * 1.4 + u(-0.4, 0.4)); b = u(0, 6.28); L = u(2.0, 2.6); broke = rng.random() < 0.3
        lay.append(ST(spin(x, y, b, L, lambda t, L=L: 0.46 if t < L * 0.1 else 0.46 - 0.2 * min(1.0, (t - L * 0.1) / (L * 0.15)), 1.15), "glass")); foot.append(disc(x, y, L / 2 + 0.2))
        if broke: p = pol(x, y, L / 2 + 0.7, b + math.pi); lay.append(("up", stone(p[0], p[1], 0.6, rng, 0.6, 3), 0.25, "glass"))
    return hull(foot).buffer(0.5), lay

@d("urban", "made", "iron", "road_sign")
def _(cx, cy, a, rng):
    u = rng.uniform; ff = frame(cx, cy, a); L = u(5.0, 7.0); k = rng.randint(0, 2); c = ff(L / 2, 0); R = u(1.4, 1.8)
    plate = ngon(c[0], c[1], R, 8, a + 0.39) if k == 0 else ngon(c[0], c[1], R * 1.15, 3, a + u(0, 1)) if k == 1 else rect(c[0], c[1], R * 2.2, R * 1.2, a + u(-0.4, 0.4))
    bend = ff(-L * 0.1, u(-0.8, 0.8)); ps = rod([ff(-L / 2, 0), bend, ff(L / 2 - 0.5, 0)], 0.3, 1.1) + ext(plate, 0.4) + ext(plate.difference(inset(plate, 0.4)), 0.55)
    return hull([plate, rect(cx, cy, L, 1.8, a)]), [ST(ps, "iron")]

@d("urban", "made", "wood", "crossarm")
def _(cx, cy, a, rng):
    u = rng.uniform; ff = frame(cx, cy, a); L = u(5.0, 6.5); ps = blk(cx, cy, L, 0.75, a, 0.6); lay = []; foot = [rect(cx, cy, L, 1.2, a)]
    for x in (-L * 0.38, 0, L * 0.38):
        c = ff(x, 0); ps += turn(c[0], c[1], [(0.42, 0.0), (0.42, 0.9), (0.3, 0.95), (0.42, 1.1), (0.3, 1.3)], 10)
        if rng.random() < 0.6: sv = rng.choice((-1, 1)); e = ff(x + u(-1.5, 1.5), sv * u(2.2, 3.4)); lay.append(ST(rod(wiggle(ff(x, sv * 0.85), e, rng, 0.5, 4), 0.25, 1.0), "wire")); foot.append(disc(e[0], e[1], 0.3))
    return hull(foot), [ST(ps, "wood")] + lay

@d("urban", "made", "tin", "hubcap_and_bumper")
def _(cx, cy, a, rng):
    u = rng.uniform; ff = frame(cx, cy, a); R = u(1.1, 1.4); c = ff(u(-1.5, 1.5), 2.4); L = u(5.0, 6.5)
    cuts = [(barl(pol(c[0], c[1], R * 0.45, t), pol(c[0], c[1], R * 0.85, t), 0.3), 0.25) for t in np.arange(0, 6.2, 6.2832 / 8)]
    bump = rod([ff(-L / 2, -0.7), ff(-L * 0.3, 0), ff(L * 0.3, 0), ff(L / 2, -0.7 * u(0.2, 2.0))], 0.42, 1.1)
    return hull([rect(cx, cy, L, 2.0, a), disc(c[0], c[1], R)]), [ST(bump, "tin"), ST(turn(c[0], c[1], [(R, 0.0), (R, 0.2), (R * 0.4, 0.5)], 20), "tin", cuts=cuts)]

@d("urban", "debris", "concrete", "kerbstones")
def _(cx, cy, a, rng):
    u = rng.uniform; ff = frame(cx, cy, a); n = rng.randint(3, 5); ps = []
    for i in range(n): c = ff((i - (n - 1) / 2) * 2.1, u(-0.5, 0.5)); ps += loaf(c[0], c[1], a + u(-0.3, 0.3), 1.9, 1.0, u(0.6, 0.8))
    return rect(cx, cy, n * 2.1 + 0.4, 2.6, a), [ST(ps, "concrete")]

@d("urban", "made", "wood", "window_frame")
def _(cx, cy, a, rng):
    u = rng.uniform; ff = frame(cx, cy, a); L = u(3.6, 4.6); W = u(2.8, 3.4); R = rect(cx, cy, L, W, a)
    g = U([R.difference(inset(R, 0.5)), rect(cx, cy, 0.45, W, a), rect(cx, cy, L, 0.45, a)])
    lay = [ST(ext(g, 0.45), "wood")] + [("up", stone(*ff(u(-L, L) * 0.7, sv * (W / 2 + u(0.8, 1.4))), u(0.55, 0.75), rng, 0.55, 3), 0.25, "glass") for sv in (-1, 1) for i in range(rng.randint(0, 2))]
    return rect(cx, cy, L + 1.8, W + 3.6, a), lay

@d("urban", "made", "iron", "park_bench")
def _(cx, cy, a, rng):
    u = rng.uniform; ff = frame(cx, cy, a); L = u(5.0, 6.2); n = 4; miss = rng.randint(0, n + 1)
    slats = U([barl(ff(-L / 2, (i - 1.5) * 1.05), ff(L / 2 * (u(0.2, 0.6) if i == miss else 1.0), (i - 1.5) * 1.05), 0.92) for i in range(n)])
    ends = U([rect(*ff(sv * (L / 2 - 0.6), 0), 0.95, n * 1.05 + 0.9, a) for sv in (-1, 1)])
    return rect(cx, cy, L, n * 1.05 + 1.0, a), [("up", ends, 0.75, "iron"), ("up", slats.difference(ends.buffer(0.02)), 0.45, "wood")]

@d("urban", "debris", "concrete", "rebar_chunk")
def _(cx, cy, a, rng):
    u = rng.uniform; r = u(1.3, 1.8); ps = rock(cx, cy, r, u(0.9, 1.3), rng, 0.85, 6); foot = [disc(cx, cy, r)]
    for i in range(rng.randint(2, 4)):
        t = a + u(-0.9, 0.9); e = pol(cx, cy, r + u(1.4, 2.6), t); m = pol(cx, cy, r + 0.6, t + u(-0.5, 0.5)); ps += rod([pol(cx, cy, r * 0.4, t), m, e], 0.24, 1.3); foot.append(disc(e[0], e[1], 0.3))
    return hull(foot), [ST(ps, "concrete")]

@d("urban", "made", "leather", "suitcase")
def _(cx, cy, a, rng):
    u = rng.uniform; ff = frame(cx, cy, a); L = u(2.8, 3.4); W = u(1.9, 2.3); h = u(0.75, 0.95); c = ff(u(-1, 1), W / 2 + 1.3); b = u(0, 6.28)
    ps = ext(rect(cx, cy, L, W, a).buffer(-0.3).buffer(0.3), h) + ext(ann(*ff(0, -W / 2 - 0.2), 0.55, 0.26, 10), 0.4) + [s for sv in (-1, 1) for s in blk(*ff(sv * L * 0.3, 0), 0.4, W + 0.1, a, h + 0.12)]
    shoe = loaf(c[0], c[1], b, 1.7, 0.7, 0.5) + cyl(*pol(c[0], c[1], -0.45, b), 0.4, 0.75, n=8)
    return hull([rect(cx, cy, L, W + 0.8, a), disc(c[0], c[1], 1.0)]), [ST(ps, "leather"), ST(shoe, "leather")]

@d("urban", "made", "iron", "radiator")
def _(cx, cy, a, rng):
    u = rng.uniform; ff = frame(cx, cy, a); n = rng.randint(5, 9); W = u(2.4, 3.0); ps = []
    for i in range(n): c = ff((i - (n - 1) / 2) * 0.62, 0); ps += loaf(c[0], c[1], a + 1.5708, W, 0.5, 0.7)
    ps += lie(*ff(0, W / 2 - 0.35), a, n * 0.62 + 0.9, 0.3) + lie(*ff(0, -W / 2 + 0.35), a, n * 0.62 + 0.9, 0.3)
    return rect(cx, cy, n * 0.62 + 1.2, W + 0.2, a), [ST(ps, "iron")]

@d("urban", "made", "iron", "bathtub")
def _(cx, cy, a, rng):
    u = rng.uniform; ff = frame(cx, cy, a); L = u(4.6, 5.6); W = u(2.2, 2.6); h = u(1.2, 1.5); o = U([rect(cx, cy, L - W, W, a), disc(*ff(-(L - W) / 2, 0), W / 2), disc(*ff((L - W) / 2, 0), W / 2)])
    ps = ext(o.buffer(-0.25), h - 0.15) + ext(o.difference(o.buffer(-0.3)), h) + [s for sx in (-1, 1) for sy in (-1, 1) for s in dome(*ff(sx * (L / 2 - 0.9), sy * (W / 2 - 0.1)), 0.42, 0.5)]
    return o.buffer(0.4), [ST(ps, "iron", cuts=[(o.buffer(-0.5), u(0.35, 0.6))])]

@d("urban", "marks", "seam", "chalk_outline")
def _(cx, cy, a, rng):
    u = rng.uniform; ff = frame(cx, cy, a); k = u(0.85, 1.1)
    body = U([capl(ff(-1.2 * k, 0), ff(1.2 * k, 0), 1.5 * k), disc(*ff(2.2 * k, u(-0.3, 0.3)), 0.62 * k, 12)] + [capl(ff(0.9 * k, sv * 0.5), ff(u(0.4, 2.2) * k, sv * u(1.6, 2.2) * k), 0.55 * k) for sv in (-1, 1)] + [capl(ff(-1.2 * k, sv * 0.4), ff(-u(2.6, 3.4) * k, sv * u(0.4, 1.4) * k), 0.62 * k) for sv in (-1, 1)])
    g = body.buffer(0.18).difference(body.buffer(-0.14))
    return body.convex_hull, [("gcut", g, 0.22, "seam")]

@d("urban", "debris", "concrete", "setts")
def _(cx, cy, a, rng):
    u = rng.uniform; ff = frame(cx, cy, a); n = rng.randint(3, 5); m = rng.randint(2, 4); ps = []
    for i in range(n):
        for j in range(m):
            if rng.random() < 0.2: continue
            c = ff((i - (n - 1) / 2) * 1.2 + 0.6 * (j % 2), (j - (m - 1) / 2) * 1.0); ps += loaf(c[0], c[1], a, 1.05, 0.85, u(0.35, 0.55))
    if not ps: return None
    return rect(*ff(0.3, 0), n * 1.2 + 1.0, m * 1.0 + 0.4, a), [ST(ps, "concrete")]

# ====================================================================================================== mine
@d("mine", "made", "paper", "dynamite")
def _(cx, cy, a, rng):
    u = rng.uniform; ff = frame(cx, cy, a); n = rng.randint(3, 5); L = u(2.6, 3.2); ps = []
    for i in range(n): ps += lie(*ff(u(-0.1, 0.1), (i - (n - 1) / 2) * 0.78), a, L, 0.4)
    ps += blk(cx, cy, 0.45, n * 0.78 + 0.2, a, 0.6); e = ff(L / 2 + u(1.5, 3.0), u(-2, 2))
    return hull([rect(cx, cy, L, n * 0.8 + 0.3, a), disc(e[0], e[1], 0.4)]), [ST(ps, "paper"), ST(rod(wiggle(ff(L / 2 + 0.3, 0), e, rng, 0.6, 5), 0.24, 1.0), "rope")]

@d("mine", "made", "wood", "plunger")
def _(cx, cy, a, rng):
    u = rng.uniform; ff = frame(cx, cy, a); L = u(2.2, 2.7); W = u(1.6, 2.0); h = u(1.3, 1.7); up_ = u(0.5, 1.3)
    ps = blk(cx, cy, L, W, a, h) + cyl(cx, cy, 0.34, h + up_, n=8) + hang(barl(ff(0, -W * 0.6), ff(0, W * 0.6), 0.6), h + up_ - 0.5, h + up_) + [s for sv in (-1, 1) for s in cyl(*ff(sv * L * 0.3, W * 0.25), 0.28, h + 0.3, n=6)]
    e = ff(L / 2 + u(2.0, 3.5), u(-2, 2))
    return hull([rect(cx, cy, L + 0.3, W + 0.6, a), disc(e[0], e[1], 0.4)]), [ST(ps, "wood"), ST(rod(wiggle(ff(L / 2 + 0.3, W * 0.25), e, rng, 0.6, 4), 0.24, 1.0), "wire")]

@d("mine", "made", "tin", "miner_helmet")
def _(cx, cy, a, rng):
    u = rng.uniform; ff = frame(cx, cy, a); r = u(1.05, 1.3)
    ps = turn(cx, cy, [(r + 0.5, 0.0), (r + 0.5, 0.22), (r, 0.25)] + crv(r, 0.3, 0.25, r * 0.95), 20, 0.88, a) + lie(*ff(r + 0.1, 0), a, 0.7, 0.45, lift=0.2) + blk(cx, cy, 2 * r, 0.4, a, r * 0.95 + 0.1)
    return disc(cx, cy, r + 1.0), [ST(ps, "tin")]

@d("mine", "made", "brass", "canary_cage")
def _(cx, cy, a, rng):
    u = rng.uniform; r = u(1.0, 1.25); h = u(1.7, 2.3); n = rng.randint(7, 10)
    ps = turn(cx, cy, [(r + 0.15, 0.0), (r + 0.15, 0.3), (r, 0.32), (r, h * 0.7)] + crv(r, 0.3, h * 0.7, h), 20) + ext(ann(cx, cy, 0.5, 0.24, 10), h + 0.25)
    cuts = [(Polygon([pol(cx, cy, 0.35, a + 6.2832 * (k + 0.2) / n), pol(cx, cy, r - 0.2, a + 6.2832 * (k + 0.2) / n), pol(cx, cy, r - 0.2, a + 6.2832 * (k + 0.8) / n), pol(cx, cy, 0.35, a + 6.2832 * (k + 0.8) / n)]), h * 0.45) for k in range(n)]
    return disc(cx, cy, r + 0.3), [ST(ps, "brass", cuts=cuts)]

@d("mine", "made", "iron", "kibble")
def _(cx, cy, a, rng):
    u = rng.uniform; r = u(1.2, 1.5); h = u(1.5, 2.0); full = rng.random() < 0.5
    ps = turn(cx, cy, [(r * 0.8, 0.0), (r, h * 0.5), (r * 0.9, h), (r * 0.9 + 0.12, h + 0.02), (r * 0.9 + 0.12, h + 0.15)], 16)
    lay = [ST(ps, "iron", cuts=[(disc(cx, cy, r * 0.9 - 0.3, 14), h - (0.3 if full else u(0.6, 1.0)))])]
    lay.append(ST(rod(arc(*pol(cx, cy, r + 0.8, a), 1.0, a + 1.2, a + 5.0, 10), 0.26, 1.0), "iron"))
    for i in range(rng.randint(1, 4)): p = pol(cx, cy, r + u(0.7, 1.5), a + 3.14 + u(-1, 1)); lay.append(("mound", stone(p[0], p[1], u(0.45, 0.65), rng, 0.8, 5), u(0.35, 0.5), "quartz", PEB_T))
    return disc(cx, cy, r + 2.0), lay

@d("mine", "made", "wood", "sluice_box")
def _(cx, cy, a, rng):
    u = rng.uniform; ff = frame(cx, cy, a); L = u(6.0, 8.0); W = u(1.9, 2.3); h = u(0.8, 1.0); xs = list(np.arange(-L / 2 + 0.5, L / 2 - 0.9, 1.1))
    cuts = [(rect(*ff(x + 0.42, 0), 0.8, W - 0.7, a), 0.3) for x in xs]
    ps = blk(cx, cy, L, W, a, h) + [s for sv in (-1, 1) for s in blk(*ff(sv * L * 0.3, 0), 0.6, W + 0.9, a, 0.45)]
    return rect(cx, cy, L + 0.2, W + 1.0, a), [ST(ps, "wood", cuts=cuts)]

@d("mine", "made", "brass", "assay_scale")
def _(cx, cy, a, rng):
    u = rng.uniform; ff = frame(cx, cy, a); L = u(3.6, 4.4); h = u(1.5, 2.0); tilt = u(-0.35, 0.35)
    ps = blk(cx, cy, 1.8, 1.2, a, 0.4) + cyl(cx, cy, 0.34, h, n=8) + hang(barl(ff(-L / 2, 0), ff(L / 2, 0), 0.5), h - 0.4, h)
    for sv in (-1, 1): c = ff(sv * L / 2, 0); ps += turn(c[0], c[1], [(0.55, 0.0), (0.85, 0.45 + sv * tilt)], 14)
    cuts = [(disc(*ff(sv * L / 2, 0), 0.55, 10), 0.22) for sv in (-1, 1)]
    return rect(cx, cy, L + 2.0, 2.0, a), [ST(ps, "brass", cuts=cuts)]

@d("mine", "made", "tin", "lunch_pail")
def _(cx, cy, a, rng):
    u = rng.uniform; ff = frame(cx, cy, a); L = u(2.2, 2.7); W = u(1.4, 1.7); h = u(0.9, 1.2); c = ff(L / 2 + u(1.0, 1.6), u(-1, 1))
    ps = ext(rect(cx, cy, L, W, a).buffer(-0.35).buffer(0.35), h) + loaf(cx, cy, a, L, W, 0.45, lift=h) + rod([ff(-L * 0.3, 0), ff(L * 0.3, 0)], 0.27, 1.0, lift=h + 0.4)
    mug = turn(c[0], c[1], [(0.52, 0.0), (0.58, 0.85)], 12) + ext(ann(*pol(c[0], c[1], 0.75, a + 1), 0.4, 0.18, 8), 0.6)
    return hull([rect(cx, cy, L, W, a), disc(c[0], c[1], 1.2)]), [ST(ps, "tin"), ST(mug, "tin", cuts=[(disc(c[0], c[1], 0.3, 8), 0.4)])]

@d("mine", "made", "iron", "drill_steels")
def _(cx, cy, a, rng):
    u = rng.uniform; ff = frame(cx, cy, a); ps = []; n = rng.randint(2, 4)
    for i in range(n):
        f2 = frame(*ff(u(-0.5, 0.5), (i - (n - 1) / 2) * 0.95), a + u(-0.12, 0.12)); L = u(5.0, 7.5); ps += rod([f2(-L / 2, 0), f2(L / 2, 0)], 0.28, 1.1) + ext(star(*f2(L / 2, 0), 0.5, 0.25, 4, a), 0.5)
    h = frame(*ff(u(-1, 1), n * 0.5 + 1.2), a + u(-0.5, 0.5)); ps2 = rod([h(-2.2, 0), h(1.6, 0)], 0.3, 1.1) + lie(*h(1.9, 0), a + 1.57 + u(-0.5, 0.5), 1.8, 0.55)
    return rect(*ff(0, 0.6), 8.0, n * 0.95 + 3.4, a), [ST(ps, "iron"), ST(ps2, "wood")]

@d("mine", "made", "tin", "oil_lantern")
def _(cx, cy, a, rng):
    u = rng.uniform; ff = frame(cx, cy, a); L = u(2.6, 3.2); r = u(0.7, 0.85)
    f_ = lambda t: r if t < -L * 0.22 else r * (0.62 + 0.3 * math.sin((t + L * 0.22) / (L * 0.45) * math.pi)) if t < L * 0.23 else r * 0.75 if t < L * 0.4 else 0.35
    ps = spin(cx, cy, a, L, f_, 1.12) + ext(ann(*ff(L / 2 + 0.5, 0), 0.75, 0.42, 12), 0.4)
    lay = [ST(ps, "tin")] + ([("gcut", blob(*ff(-L * 0.2, u(1.2, 1.8) * rng.choice((-1, 1))), u(0.7, 1.0), rng, 3), 0.22, "scorch")] if rng.random() < 0.5 else [])
    return rect(*ff(0.3, 0), L + 1.8, 2 * r + 3.0, a), lay

@d("mine", "made", "iron", "wheelset")
def _(cx, cy, a, rng):
    u = rng.uniform; ff = frame(cx, cy, a); L = u(3.4, 4.2); R = u(1.0, 1.25); one = rng.random() < 0.25
    ps = lie(cx, cy, a, L + 0.8, 0.32) + lie(*ff(-L / 2, 0), a, 0.55, R) + lie(*ff(-L / 2 - 0.3, 0), a, 0.3, R + 0.25)
    if not one: ps += lie(*ff(L / 2, 0), a, 0.55, R) + lie(*ff(L / 2 + 0.3, 0), a, 0.3, R + 0.25)
    return rect(cx, cy, L + 1.4, 2 * R + 0.8, a), [ST(ps, "iron")]

@d("mine", "debris", "rock", "stalagmites")
def _(cx, cy, a, rng):
    u = rng.uniform; ps = []; foot = []
    for i in range(rng.randint(3, 6)):
        x, y = pol(cx, cy, u(0, 2.0), u(0, 6.28)); r = u(0.5, 1.0); h = r * u(2.0, 3.4); ps += turn(x, y, [(r, 0.0), (r * 0.65, h * 0.3), (r * 0.5, h * 0.6), (0.24, h)], 10, u(0.8, 1.0), u(0, 3)); foot.append(disc(x, y, r))
    return hull(foot).buffer(0.2), [ST(ps, "rock")]

@d("mine", "made", "wood", "sorting_table")
def _(cx, cy, a, rng):
    u = rng.uniform; ff = frame(cx, cy, a); L = u(4.0, 5.0); W = u(2.4, 2.9); h = u(0.9, 1.2); R = rect(cx, cy, L, W, a)
    ps = ext(R, h) + ext(R.difference(inset(R, 0.4)), h + 0.3)
    for i in range(rng.randint(3, 7)): c = ff(u(-L / 2 + 0.9, L / 2 - 0.9), u(-W / 2 + 0.9, W / 2 - 0.9)); ps += rock(c[0], c[1], u(0.4, 0.6), u(0.4, 0.6), rng, lift=h)
    return rect(cx, cy, L + 0.2, W + 0.2, a), [ST(ps, "wood")]

@d("mine", "made", "iron", "pulley_block")
def _(cx, cy, a, rng):
    u = rng.uniform; ff = frame(cx, cy, a); R = u(1.0, 1.3); hk = ff(-R - 0.9, 0)
    ps = lie(cx, cy, a + 1.5708, 0.6, R) + ext(U([capl(ff(-R - 0.3, sv * 0.52), ff(R + 0.3, sv * 0.52), 0.42) for sv in (-1, 1)]), R * 0.75) + rod(arc(hk[0], hk[1], 0.6, a + 0.6, a + 5.2, 10), 0.26, 1.1)
    e = ff(R + u(2.0, 4.0), u(-2, 2))
    return hull([rect(*ff(-0.6, 0), 2 * R + 2.6, 2 * R, a), disc(e[0], e[1], 0.4)]), [ST(ps, "iron"), ST(rod(wiggle(ff(R + 0.6, 0), e, rng, 0.6, 4), 0.28, 1.0), "rope")]

@d("mine", "made", "wood", "claim_stake")
def _(cx, cy, a, rng):
    u = rng.uniform; ff = frame(cx, cy, a); h = u(2.2, 3.0)
    ps = cyl(cx, cy, 0.42, h, n=6) + hang(rect(*ff(0, 0), 2.6, 0.55, a + u(-0.15, 0.15)), h - 1.2, h - 0.3)
    lay = [ST(ps, "wood")] + [("mound", stone(*p, u(0.5, 0.75), rng, 0.85, 6), u(0.4, 0.6), "rock", PEB_T) for p in ringpts(cx, cy, 1.05, rng.randint(4, 6), a, 0.2, rng)]
    return disc(cx, cy, 2.0), lay

@d("mine", "made", "iron", "hand_pump")
def _(cx, cy, a, rng):
    u = rng.uniform; ff = frame(cx, cy, a); r = u(0.6, 0.75); h = u(2.2, 2.8)
    ps = turn(cx, cy, [(r * 1.5, 0.0), (r * 1.5, 0.3), (r, 0.32), (r, h), (r * 0.7, h + 0.2)], 12) + rod([ff(r * 0.5, 0), ff(r + 1.3, 0)], 0.34, 1.0, lift=h * 0.55) + hang(barl(ff(0, 0), ff(-r - u(1.4, 2.0), 0), 0.5), h - 0.3, h + 0.15)
    p = blob(*ff(r + 1.6, 0), u(0.8, 1.1), rng, 3)
    return hull([disc(cx, cy, r + 2.2), p]), [ST(ps, "iron"), ("gcut", p, 0.25, "water")]

@d("mine", "marks", "crack", "gold_seam")
def _(cx, cy, a, rng):
    u = rng.uniform; ff = frame(cx, cy, a); L = u(5.0, 8.0); pts = wiggle(ff(-L / 2, 0), ff(L / 2, 0), rng, 0.9, 6); g = LineString(pts).buffer(0.26, 4)
    lay = [("gcut", g, 0.4, "crack")] + [("mound", stone(p[0] + u(-0.2, 0.2), p[1] + 0.85 * rng.choice((-1, 1)), u(0.42, 0.55), rng, 0.8, 5), u(0.3, 0.45), "gold", PEB_T) for p in pts[1:-1] if rng.random() < 0.6]
    return g.buffer(1.3).convex_hull, lay

@d("mine", "made", "iron", "switch_lever")
def _(cx, cy, a, rng):
    u = rng.uniform; ff = frame(cx, cy, a); L = u(2.4, 3.0); e = ff(u(1.8, 2.6), 0); h = u(1.2, 1.8)
    ps = blk(cx, cy, L, 1.5, a, 0.4) + lie(cx, cy, a + 1.57, 1.1, 0.5, lift=0.3) + rod([ff(0, 0), e], 0.3, 1.0, lift=h * 0.6) + dome(e[0], e[1], 0.55, 0.9, lift=h * 0.55)
    return rect(*ff(0.6, 0), L + 2.2, 1.9, a), [ST(ps, "iron")]

@d("mine", "made", "sack", "spilled_ore")
def _(cx, cy, a, rng):
    u = rng.uniform; ff = frame(cx, cy, a); L = u(3.0, 3.6); W = u(1.9, 2.3)
    ps = spin(cx, cy, a, L, lambda t: W / 2 * max(0.0, 1 - abs(2 * t / L) ** 3) ** 0.5 * (1.0 if t < L * 0.25 else 0.75) + 0.05, 0.75)
    lay = [ST(ps, "sack")]
    for i in range(rng.randint(4, 8)): c = ff(L / 2 + u(0.5, 2.6), u(-1.4, 1.4)); lay.append(("mound", stone(c[0], c[1], u(0.42, 0.65), rng, 0.8, 5), u(0.3, 0.5), "quartz", PEB_T))
    return rect(*ff(1.4, 0), L + 3.2, W + 1.4, a), lay

@d("mine", "made", "wood", "fallen_stull")
def _(cx, cy, a, rng):
    u = rng.uniform; ff = frame(cx, cy, a); L = u(3.0, 4.2); b = u(0.3, 0.8) * rng.choice((-1, 1)); r = u(0.6, 0.75); c2 = pol(*ff(0.3, 0), L / 2, a + b)
    ps = lie(*ff(-L / 2, 0), a, L, r) + lie(c2[0], c2[1], a + b, L * u(0.7, 1.0), r)
    for sv in (-1, 1):
        if rng.random() < 0.7: ps += ext(tri(ff(u(-0.3, 0.3), sv * 0.3), ff(u(-0.5, 0.9), sv * u(1.5, 2.2)), 0.95), 0.35)                # splinters where it snapped
    return hull([rect(*ff(-L / 2, 0), L, 2 * r, a), rect(c2[0], c2[1], L, 2 * r, a + b)]).buffer(1.0), [ST(ps, "wood")]

# ====================================================================================================== swamp
@d("swamp", "growth", "lily", "lily_pool", ground=True)
def _(cx, cy, a, rng):
    u = rng.uniform; p = blob(cx, cy, u(2.6, 3.4), rng, 5); pads = []
    for i in range(rng.randint(2, 5)):
        x, y = pol(cx, cy, u(0, 1.8), u(0, 6.28)); r = u(0.75, 1.0); t = u(0, 6.28); q = disc(x, y, r, 14).difference(Polygon([(x, y), pol(x, y, 2 * r, t), pol(x, y, 2 * r, t + 0.5)]))
        if inset(p, 0.3).contains(q) and all(q.distance(g) > 0.3 for g in pads): pads.append(q)
    return p, [("gcut", p, 0.5, "water")] + ([("pad", U(pads), "lily")] if pads else [])

@d("swamp", "growth", "root", "cypress_knees")
def _(cx, cy, a, rng):
    u = rng.uniform; ps = []; foot = []
    for i in range(rng.randint(4, 8)):
        x, y = pol(cx, cy, u(0, 2.4), u(0, 6.28)); r = u(0.45, 0.8); h = r * u(1.8, 3.2); ps += turn(x, y, [(r, 0.0), (r * 0.7, h * 0.5)] + crv(r * 0.7, 0.24, h * 0.5, h), 10, u(0.75, 1.0), u(0, 3)); foot.append(disc(x, y, r))
    return hull(foot).buffer(0.2), [ST(ps, "root")]

@d("swamp", "made", "log", "dugout")
def _(cx, cy, a, rng):
    u = rng.uniform; ff = frame(cx, cy, a); L = u(7.0, 9.5); W = u(1.9, 2.3); f_ = lambda t: W / 2 * max(0.0, 1 - abs(2 * t / L) ** 2.6) ** 0.6 + 0.04
    hollow = Polygon([ff(t, f_(t) - 0.42) for t in np.linspace(-L * 0.38, L * 0.38, 12)] + [ff(t, -f_(t) + 0.42) for t in np.linspace(L * 0.38, -L * 0.38, 12)])
    pd = frame(*ff(u(-1, 1), W / 2 + 0.9), a + u(-0.2, 0.2)); paddle = rod([pd(-2.4, 0), pd(1.0, 0)], 0.26, 1.0) + ext(ell(*pd(1.9, 0), 1.0, 0.5, a, 12), 0.35)
    return rect(*ff(0, 0.5), L, W + 2.2, a), [ST(spin(cx, cy, a, L, f_, 0.9, prof="ridge") + spin(cx, cy, a, L, lambda t: f_(t) * 0.99, 0.55), "log", cuts=[(hollow, 0.3)]), ST(paddle, "log")]

@d("swamp", "made", "twig", "fish_trap")
def _(cx, cy, a, rng):
    u = rng.uniform; ff = frame(cx, cy, a); L = u(3.6, 4.6); r = u(0.9, 1.1)
    ps = spin(cx, cy, a, L, lambda t: r * (1 - 0.7 * (t + L / 2) / L) + 0.1 * (math.sin(t * 6.5) > 0.2) + 0.12, 1.1)
    return rect(cx, cy, L, 2 * r + 0.4, a), [ST(ps, "twig", cuts=[(disc(*ff(-L / 2 + 0.3, 0), r * 0.55, 8), r * 0.35)])]

@d("swamp", "made", "log", "totem_stake")
def _(cx, cy, a, rng):
    u = rng.uniform; ff = frame(cx, cy, a); L = u(6.0, 8.0); r = u(0.6, 0.75); ph = u(0, 6)
    ps = spin(cx, cy, a, L, lambda t: r * (1 + 0.16 * math.sin(t * 2.6 + ph)) * (1.0 if t < L / 2 - 1.0 else max(0.3, (L / 2 - t))), 1.1)
    for i in range(rng.randint(1, 3)): c = ff(-L / 2 + 1.2 + i * 1.7, 0); ps += dome(c[0], c[1], r + 0.25, r * 1.1 + 0.5, sq=0.85, a=a)
    return rect(cx, cy, L, 2 * r + 0.8, a), [ST(ps, "log")]

@d("swamp", "growth", "shroom", "shelf_stump")
def _(cx, cy, a, rng):
    u = rng.uniform; r = u(1.1, 1.5); h = u(1.8, 2.6); ps = turn(cx, cy, [(r * 1.25, 0.0), (r, h * 0.3), (r, h)], 10); lay = []
    for i in range(rng.randint(2, 4)):
        t = a + i * 1.9 + u(-0.4, 0.4); c = pol(cx, cy, r * 0.9, t); R = u(0.8, 1.2); z = h * u(0.3, 0.8)
        ps += turn(c[0], c[1], [(R, 0.0), (R, z), (R * 0.5, z + 0.3)], 12)
    return disc(cx, cy, r + 1.4), [ST(ps, "shroom", cuts=[(disc(cx, cy, r * 0.5, 8), h - 0.3)])]

@d("swamp", "made", "tflag", "frog_idol")
def _(cx, cy, a, rng):
    u = rng.uniform; ff = frame(cx, cy, a); r = u(1.3, 1.7); h = r * u(0.85, 1.05)
    ps = dome(cx, cy, r, h, sq=0.85, a=a) + [s for sv in (-1, 1) for s in dome(*ff(r * 0.45, sv * r * 0.42), r * 0.3, h * 0.55, lift=h * 0.62)] + [s for sv in (-1, 1) for s in loaf(*ff(-r * 0.2, sv * r * 0.85), a + sv * 0.4, r * 1.3, r * 0.5, h * 0.45)]
    return disc(cx, cy, r * 1.4), [ST(ps, "tflag", cuts=[(line([ff(r * 0.7, -r * 0.45), ff(r * 0.9, 0), ff(r * 0.7, r * 0.45)], 0.26), h * 0.3)])]

@d("swamp", "made", "reed", "reed_bundle")
def _(cx, cy, a, rng):
    u = rng.uniform; ff = frame(cx, cy, a); L = u(5.0, 7.0); n = rng.randint(5, 8); ps = []; tips = []
    for i in range(n):
        v = (i - (n - 1) / 2); e = ff(L / 2 + u(-0.4, 0.4), v * u(0.45, 0.8)); ps += rod([ff(-L / 2, v * 0.3), ff(0, v * 0.3), e], 0.26, 1.1) + spin(e[0], e[1], a + v * 0.08, 1.3, lambda t: 0.36, 1.1); tips.append(disc(e[0], e[1], 0.5))
    ps += blk(*ff(-L * 0.1, 0), 0.5, n * 0.3 + 0.7, a, 0.5)
    return hull(tips + [rect(cx, cy, L, n * 0.4, a)]), [ST(ps, "reed")]

@d("swamp", "growth", "root", "root_tangle")
def _(cx, cy, a, rng):
    u = rng.uniform; ps = []; foot = []; n = rng.randint(4, 6)
    for i in range(n):
        t = a + 6.2832 * i / n + u(-0.3, 0.3); L = u(3.0, 4.6); pts = wiggle(pol(cx, cy, 0.3, t), pol(cx, cy, L, t + u(-0.4, 0.4)), rng, 0.6, 5)
        ps += rod(pts, lambda q: 0.55 - 0.3 * q + 0.12 * math.sin(q * 9), 1.4); foot.append(disc(pts[-1][0], pts[-1][1], 0.3))
    return hull(foot), [ST(ps + dome(cx, cy, 0.9, 1.3), "root")]

@d("swamp", "made", "hide", "shed_skin")
def _(cx, cy, a, rng):
    u = rng.uniform; ff = frame(cx, cy, a); L = u(6.0, 9.0); pts = [ff(x, 0.9 * math.sin(x * 1.3 + 1)) for x in np.linspace(-L / 2, L / 2, 18)]
    return LineString(pts).buffer(0.8), [ST(rod(pts, lambda q: 0.5 * math.sin(max(0.05, q) * math.pi) ** 0.5 + 0.22, 0.45), "hide")]

@d("swamp", "growth", "mud", "mud_boil")
def _(cx, cy, a, rng):
    u = rng.uniform; R = u(1.8, 2.5); ps = turn(cx, cy, [(R, 0.0), (R * 0.75, 0.3), (R * 0.5, 0.36)], 18, u(0.8, 1.0), a)
    for i in range(rng.randint(2, 5)): p = pol(cx, cy, u(0, R * 0.5), u(0, 6.28)); r = u(0.35, 0.6); ps += dome(p[0], p[1], r, r * 1.0, lift=0.3, n=10)
    return disc(cx, cy, R + 0.2), [ST(ps, "mud", cuts=[(ann(cx, cy, R * 0.72, R * 0.72 - 0.26, 16), 0.16)])]

@d("swamp", "growth", "moss", "mossy_log")
def _(cx, cy, a, rng):
    u = rng.uniform; ff = frame(cx, cy, a); L = u(5.0, 7.0); r = u(0.9, 1.15); ps = spin(cx, cy, a, L, lambda t: r * (1 - 0.15 * t / L), 0.75)
    for i in range(rng.randint(3, 6)): c = ff(u(-L / 2 + 0.6, L / 2 - 0.6), u(-r * 0.5, r * 0.5)); q = u(0.5, 0.8); ps += dome(c[0], c[1], q, q * 0.8, lift=r * 0.55, n=10)
    return rect(cx, cy, L, 2 * r + 0.3, a), [ST(ps, "moss")]

@d("swamp", "made", "hide", "shaman_drum")
def _(cx, cy, a, rng):
    u = rng.uniform; r = u(1.2, 1.5); h = u(1.0, 1.5); n = rng.randint(6, 9); c = pol(cx, cy, r + 1.3, a)
    ps = turn(cx, cy, [(r * 0.9, 0.0), (r, h * 0.5), (r * 0.94, h), (r * 0.94 + 0.12, h + 0.02), (r * 0.94 + 0.12, h + 0.14)], 20)
    for k in range(n): ps += cyl(*pol(cx, cy, r * 0.94 + 0.05, a + 6.2832 * k / n), 0.26, h + 0.3, n=6)
    beat = rod([pol(c[0], c[1], 1.3, a + 1.2), pol(c[0], c[1], 1.3, a + 1.2 + math.pi)], 0.25, 1.0) + dome(*pol(c[0], c[1], 1.3, a + 1.2), 0.42, 0.7)
    return hull([disc(cx, cy, r + 0.4), disc(c[0], c[1], 1.6)]), [ST(ps, "hide"), ST(beat, "log")]

@d("swamp", "made", "bone", "croc_skull")
def _(cx, cy, a, rng):
    u = rng.uniform; ff = frame(cx, cy, a); L = u(4.6, 6.0); W = u(1.9, 2.3)
    ps = spin(cx, cy, a, L, lambda t: W / 2 * (1.0 if t < -L * 0.15 else 0.5 + 0.5 * math.cos(min(1.0, (t + L * 0.15) / (L * 0.3)) * 1.5708) ** 2 * 1.0 if t < L * 0.15 else 0.5) * (0.9 if t > L / 2 - 0.4 else 1.0) + 0.03, 0.7)
    for x in np.arange(-L * 0.05, L / 2 - 0.3, 0.7):
        for sv in (-1, 1): c = ff(x, sv * (W * 0.27 + 0.1)); ps += cone(c[0], c[1], 0.3, 0.75, 0.21, n=6)
    cuts = [(disc(*ff(-L * 0.25, sv * W * 0.25), 0.34, 8), 0.28) for sv in (-1, 1)] + [(disc(*ff(L / 2 - 0.5, sv * 0.22), 0.2, 6), 0.3) for sv in (-1, 1)]
    return rect(cx, cy, L + 0.2, W + 0.3, a), [ST(ps, "bone", cuts=cuts)]

@d("swamp", "made", "carapace", "turtle_shell")
def _(cx, cy, a, rng):
    u = rng.uniform; r = u(1.5, 2.0); h = r * u(0.5, 0.65); sq = u(0.78, 0.9)
    ps = turn(cx, cy, [(r + 0.2, 0.0), (r + 0.2, 0.18), (r, 0.2)] + crv(r, 0.3, 0.2, h), 20, sq, a)
    cuts = [(ngon(cx, cy, r * 0.36, 6, a).exterior.buffer(0.13), h * 0.72)] + [(barl(pol(cx, cy, r * 0.36, a + k * 1.0472), pol(cx, cy, r * 0.82, a + k * 1.0472), 0.25), h * 0.45) for k in range(6)]
    return disc(cx, cy, r + 0.3), [ST(ps, "carapace", cuts=cuts)]

@d("swamp", "made", "clay", "dart_gourds")
def _(cx, cy, a, rng):
    u = rng.uniform; ff = frame(cx, cy, a); ps = []; n = rng.randint(2, 3)
    for i in range(n): c = ff((i - (n - 1) / 2) * 1.8, u(-0.3, 0.3)); r = u(0.65, 0.85); h = u(1.4, 1.9); ps += turn(c[0], c[1], [(r * 0.6, 0.0), (r, h * 0.3)] + crv(r, 0.36, h * 0.3, h * 0.7) + [(0.3, h * 0.85), (0.4, h)], 14)
    L = u(5.5, 7.5); pp = frame(*ff(0, 1.7), a + u(-0.2, 0.2))
    return rect(*ff(0, 0.6), max(L, n * 1.8) + 0.4, 3.6, a), [ST(ps, "clay"), ST(rod([pp(-L / 2, 0), pp(L / 2, 0)], 0.27, 1.1) + lie(*pp(-L / 2 + 0.4, 0), a, 0.6, 0.4), "reed")]

@d("swamp", "made", "reed", "mat_and_bowl")
def _(cx, cy, a, rng):
    u = rng.uniform; ff = frame(cx, cy, a); L = u(4.0, 5.0); W = u(2.8, 3.4); c = ff(u(-1, 1), u(-0.6, 0.6)); r = u(0.8, 1.0)
    cuts = [(rect(*ff(x, 0), 0.24, W - 0.5, a), 0.14) for x in np.arange(-L / 2 + 0.6, L / 2 - 0.3, 0.6) if abs(x - (c[0] - cx)) > 9] + [(rect(*ff(x, v), 0.24, 0.9, a), 0.14) for x in np.arange(-L / 2 + 0.6, L / 2 - 0.3, 0.6) for v in (-W / 2 + 0.7, W / 2 - 0.7) if rect(*ff(x, v), 0.3, 1.0, a).distance(disc(c[0], c[1], r)) > 0.3]
    ps = ext(rect(cx, cy, L, W, a), 0.3) + turn(c[0], c[1], [(r * 0.6, 0.0), (r, 0.85)], 14)
    return rect(cx, cy, L + 0.2, W + 0.2, a), [ST(ps, "reed", cuts=cuts + [(disc(c[0], c[1], r - 0.3, 10), 0.5)])]

@d("swamp", "made", "log", "pilings")
def _(cx, cy, a, rng):
    u = rng.uniform; ff = frame(cx, cy, a); n = rng.randint(3, 5); ps = []; cuts = []
    for i in range(n):
        c = ff((i - (n - 1) / 2) * u(1.5, 1.9), u(-0.4, 0.4)); r = u(0.5, 0.7); h = u(0.9, 2.8); ps += turn(c[0], c[1], [(r * 1.15, 0.0), (r, 0.4), (r, h)], 9)
        cuts.append((tri(pol(c[0], c[1], r, u(0, 6.28)), (c[0], c[1]), r * 1.2), h - u(0.3, 0.6)))
    return rect(cx, cy, n * 1.9 + 0.6, 2.4, a), [ST(ps, "log", cuts=cuts)]

@d("swamp", "made", "bone", "bone_fetish")
def _(cx, cy, a, rng):
    u = rng.uniform; L = u(3.6, 4.6); ps = dome(cx, cy, 0.55, 0.75); kn = lambda q: 0.3 + 0.16 * (q < 0.12 or q > 0.88)
    for b in (a + u(0.5, 0.9), a - u(0.5, 0.9)): ps += rod([pol(cx, cy, L / 2, b), pol(cx, cy, L / 2, b + math.pi)], kn, 1.0)
    lay = [ST(ps, "bone")]
    for i in range(rng.randint(2, 3)): t = a + 1.57 + (i - 1) * 0.5; p0 = pol(cx, cy, 0.9, t); e = pol(cx, cy, u(2.4, 3.2), t); lay.append(ST(spin((p0[0] + e[0]) / 2, (p0[1] + e[1]) / 2, t, math.dist(p0, e), lambda q: 0.42 - 0.1 * abs(q), 0.5, prof="ridge"), "reed"))
    return disc(cx, cy, L / 2 + 0.9), lay

@d("swamp", "marks", "rut", "gator_slide")
def _(cx, cy, a, rng):
    u = rng.uniform; ff = frame(cx, cy, a); L = u(6.0, 9.0); pts = [ff(x, 0.5 * math.sin(x * 0.9)) for x in np.linspace(-L / 2, L / 2, 14)]
    gs = [LineString(pts).buffer(0.55, 6)] + [U([tri(pol(*ff(x, sv * 1.5 + 0.5 * math.sin(x * 0.9)), 0.1, a), pol(*ff(x, sv * 1.5 + 0.5 * math.sin(x * 0.9)), 0.8, a + sv * 0.6 + t), 0.4) for t in (-0.5, 0, 0.5)]) for x in np.arange(-L / 2 + 1, L / 2 - 0.8, 1.9) for sv in (-1, 1)]
    return rect(cx, cy, L + 0.6, 4.6, a), [("gcut", U(gs), 0.3, "rut")]

# ====================================================================================================== cynder
@d("cynder", "marks", "lava", "skylight", ground=True)
def _(cx, cy, a, rng):
    u = rng.uniform; p = blob(cx, cy, u(1.5, 2.1), rng, 4); ps = []
    n = rng.randint(5, 8); ex = p.buffer(1.15).exterior
    for i in range(n): q = ex.interpolate((i + u(-0.2, 0.2)) / n % 1.0, normalized=True); ps += rock(q.x, q.y, u(0.45, 0.6), u(0.4, 0.7), rng)
    return p.buffer(2.0), [("gcut", p, u(0.6, 0.85), "lava"), ST(ps, "basalt")]

@d("cynder", "debris", "basalt", "basalt_columns")
def _(cx, cy, a, rng):
    u = rng.uniform; r = u(0.6, 0.8); s = r * 1.74; ps = []; foot = []
    for i in range(-2, 3):
        for j in range(-2, 3):
            x = cx + (i + 0.5 * (j % 2)) * s; y = cy + j * s * 0.866; dd = math.dist((x, y), (cx, cy))
            if dd < u(1.6, 2.6): ps += ext(ngon(x, y, r - 0.04, 6, 0.5236), max(0.4, u(1.4, 3.2) * (1 - dd / 4.2))); foot.append(disc(x, y, r))
    if not ps: return None
    return hull(foot).buffer(0.2), [ST(ps, "basalt")]

@d("cynder", "debris", "basalt", "ropy_lava")
def _(cx, cy, a, rng):
    u = rng.uniform; n = rng.randint(4, 6); span = u(0.9, 1.4); ps = []; R0_ = u(1.6, 2.2)
    for k in range(n): R = R0_ + k * 0.85; ps += rod(arc(cx - R0_ * math.cos(a), cy - R0_ * math.sin(a), R, a - span / 2 * (1 - 0.06 * k), a + span / 2 * (1 - 0.06 * k), 10), lambda q: 0.5 - 0.15 * abs(2 * q - 1), 0.9)
    return hull([disc(*pol(cx - R0_ * math.cos(a), cy - R0_ * math.sin(a), R0_ + k * 0.85, a + sv * span / 2), 0.5) for k in (0, n - 1) for sv in (-1, 1)]), [ST(ps, "basalt")]

@d("cynder", "debris", "ember", "fumarole")
def _(cx, cy, a, rng):
    u = rng.uniform; r = u(1.3, 1.8); h = u(1.0, 1.6)
    lay = [ST(turn(cx, cy, [(r, 0.0), (r * 0.6, h * 0.6), (r * 0.42, h)], 12, u(0.8, 1.0), a), "basalt", cuts=[(disc(cx, cy, r * 0.25, 8), 0.2)])]
    for i in range(rng.randint(3, 7)): p = pol(cx, cy, r + u(0.4, 1.2), u(0, 6.28)); lay.append(("mound", stone(p[0], p[1], u(0.4, 0.55), rng, 0.8, 5), u(0.25, 0.4), "ember", PEB_T))
    return disc(cx, cy, r + 1.5), lay

@d("cynder", "debris", "basalt", "lava_bomb", ground=True)
def _(cx, cy, a, rng):
    u = rng.uniform; ff = frame(cx, cy, a); L = u(2.6, 3.6); r = u(0.9, 1.2)
    ps = spin(cx, cy, a, L, lambda t: r * math.sqrt(max(0.0, 1 - (2 * t / L) ** 2)) ** 1.4 + 0.08, 1.0)
    return disc(cx, cy, L / 2 + 1.0), [("gcut", ell(cx, cy, L / 2 + 0.8, r + 0.8, a, 16), 0.3, "scorch"), ST(ps, "basalt", cuts=[(wavy(ff(-L * 0.3, -r), ff(L * 0.2, r), rng, 0.3, 3, 0.26), r * 0.45)])]

@d("cynder", "made", "iron", "anvil")
def _(cx, cy, a, rng):
    u = rng.uniform; ff = frame(cx, cy, a); L = u(2.6, 3.2); W = u(1.2, 1.5); h = u(1.6, 2.1)
    ps = blk(cx, cy, L * 0.8, W + 0.5, a, h * 0.25) + blk(cx, cy, L * 0.5, W * 0.8, a, h * 0.7) + hang(rect(*ff(0.1, 0), L, W, a), h * 0.66, h) + _S().slab(tri(ff(L / 2, 0), ff(L / 2 + 1.4, 0), W), h * 0.72, h)
    return rect(*ff(0.6, 0), L + 1.6, W + 0.8, a), [ST(ps, "iron", cuts=[(rect(*ff(-L * 0.35, 0), 0.34, 0.34, a), h - 0.3)])]

@d("cynder", "made", "iron", "crucible")
def _(cx, cy, a, rng):
    u = rng.uniform; r = u(1.0, 1.3); h = u(1.3, 1.7); c = pol(cx, cy, r + 1.6, a); b = a + u(0.6, 2.4)
    tongs = [s for sv in (-1, 1) for s in rod([pol(c[0], c[1], 2.2, b + math.pi + sv * 0.12), pol(c[0], c[1], 0.2, b), pol(c[0], c[1], 1.5, b - sv * 0.3)], 0.25, 1.1)]
    return hull([disc(cx, cy, r + 0.3), disc(c[0], c[1], 2.3)]), [ST(turn(cx, cy, [(r * 0.7, 0.0), (r, h * 0.8), (r + 0.12, h)], 14) + blk(*pol(cx, cy, r + 0.2, a + 3.14), 0.7, 0.6, a, h), "iron", cuts=[(disc(cx, cy, r - 0.32, 12), h - u(0.3, 0.7))]), ST(tongs, "iron")]

@d("cynder", "made", "iron", "ingots")
def _(cx, cy, a, rng):
    u = rng.uniform; ff = frame(cx, cy, a); n = rng.randint(2, 4); ps = []
    for i in range(n): c = ff(u(-0.1, 0.1), (i - (n - 1) / 2) * 1.05); ps += loaf(c[0], c[1], a + u(-0.05, 0.05), 2.4, 0.95, 0.5)
    for i in range(rng.randint(0, n - 1)): c = ff(u(-0.2, 0.2), (i - (n - 2) / 2) * 1.05); ps += loaf(c[0], c[1], a + u(-0.3, 0.3), 2.4, 0.95, 0.5, lift=0.46)
    return rect(cx, cy, 2.9, n * 1.05 + 0.4, a), [ST(ps, "iron")]

@d("cynder", "debris", "obsidian", "obsidian_blades")
def _(cx, cy, a, rng):
    u = rng.uniform; ps = []; foot = []
    for i in range(rng.randint(4, 8)):
        x, y = pol(cx, cy, u(0, 2.0), u(0, 6.28)); L = u(1.2, 2.4); t = u(0, 3.14); ps += spin(x, y, t, L, lambda q, L=L: 0.36 * (1 - abs(2 * q / L)) + 0.12, u(2.5, 5.0), prof="ridge"); foot.append(disc(x, y, L / 2))
    return hull(foot).buffer(0.2), [ST(ps, "obsidian")]

@d("cynder", "made", "ash", "charred_hand")
def _(cx, cy, a, rng):
    u = rng.uniform; ff = frame(cx, cy, a); k = u(0.9, 1.2); ps = spin(cx, cy, a, 1.9 * k, lambda t: 0.8 * k, 0.6) + rod([ff(-0.9 * k, 0), ff(-3.0 * k, u(-0.6, 0.6))], 0.42 * k, 1.0); tips = []
    for i in range(5):
        v = (i - 2) * 0.42 * k; L = (u(1.5, 2.0) if 0 < i < 4 else 1.1) * k; b = (i - 2) * 0.22 + u(-0.15, 0.15) + (0.9 if i == 0 else 0)
        p0 = ff(0.8 * k, v); p1 = pol(p0[0], p0[1], L * 0.55, a + b); p2 = pol(p1[0], p1[1], L * 0.45, a + b + u(-0.5, 0.5)); ps += rod([p0, p1, p2], lambda q: 0.25 + 0.06 * math.sin(q * 9.4) ** 2, 1.1); tips.append(disc(p2[0], p2[1], 0.3))
    return hull(tips + [rect(*ff(-1.2 * k, 0), 4.0 * k, 1.8 * k, a)]), [ST(ps, "ash")]

@d("cynder", "growth", "tooth", "horn_spur")
def _(cx, cy, a, rng):
    u = rng.uniform; R = u(2.2, 3.2); pts = arc(cx - R * math.cos(a), cy - R * math.sin(a), R, a - u(0.5, 0.8), a + u(0.5, 0.8), 12); r0 = u(0.75, 1.0)
    return LineString(pts).buffer(r0 + 0.2), [ST(rod(pts, lambda q: r0 * (1 - 0.72 * q), u(1.6, 2.4)), "tooth")]

@d("cynder", "debris", "ash", "slag_heap")
def _(cx, cy, a, rng):
    u = rng.uniform; r = u(1.8, 2.5); h = u(0.8, 1.2); ff = frame(cx, cy, a)
    cuts = [(wavy(pol(cx, cy, r * 0.9, a + t), pol(cx, cy, r * 0.15, a + t + u(-0.6, 0.6)), rng, 0.3, 3, 0.26), h * 0.3) for t in (0.3, 2.2, 4.3)[:rng.randint(2, 3)]]
    return disc(cx, cy, r + 0.2), [ST(turn(cx, cy, [(r, 0.0), (r * 0.8, h * 0.6), (r * 0.4, h)], 11, u(0.75, 0.95), a), "ash", cuts=cuts)]

@d("cynder", "made", "leather", "forge_bellows")
def _(cx, cy, a, rng):
    u = rng.uniform; ff = frame(cx, cy, a); L = u(3.6, 4.6); W = u(2.2, 2.8)
    g = Polygon([ff(-L / 2, -W / 2), ff(L * 0.2, -W * 0.3), ff(L / 2, -0.3), ff(L / 2, 0.3), ff(L * 0.2, W * 0.3), ff(-L / 2, W / 2)]).buffer(-0.2).buffer(0.2)
    ps = ext(g, 0.9) + ext(g.buffer(-0.35), 1.15) + rod([ff(L / 2 - 0.2, 0), ff(L / 2 + 1.3, 0)], 0.3, 1.1) + [s for sv in (-1, 1) for s in rod([ff(-L / 2 + 0.2, sv * 0.4), ff(-L / 2 - 1.2, sv * 0.5)], 0.28, 1.0)]
    cuts = [(rect(*ff(x, 0), 0.24, W, a), 0.95) for x in np.arange(-L / 2 + 0.7, L * 0.2, 0.6)]
    return rect(cx, cy, L + 2.8, W + 0.2, a), [ST(ps, "leather", cuts=cuts)]

@d("cynder", "marks", "lava", "ember_moat", ground=True)
def _(cx, cy, a, rng):
    u = rng.uniform; R = u(2.0, 2.7); w = u(0.6, 0.9); g = ann(cx, cy, R, R - w, 22); gap = Polygon([(cx, cy), pol(cx, cy, 2 * R, a), pol(cx, cy, 2 * R, a + u(0.4, 0.8))])
    return disc(cx, cy, R + 0.2), [("gcut", g.difference(gap), 0.5, "lava"), ST(rock(cx, cy, u(0.7, 1.0), u(0.8, 1.3), rng), "basalt")]

@d("cynder", "debris", "ember", "sulfur_crust")
def _(cx, cy, a, rng):
    u = rng.uniform; p = blob(cx, cy, u(1.4, 1.9), rng, 4); ps = ext(p, 0.25)
    for i in range(rng.randint(5, 9)): x, y = pol(cx, cy, u(0, 1.0), u(0, 6.28)); ps += cone(x, y, u(0.3, 0.45), u(0.6, 1.2), 0.21, n=5)
    return p.buffer(0.3), [ST(ps, "ember")]

@d("cynder", "made", "iron", "shackles")
def _(cx, cy, a, rng):
    u = rng.uniform; L = u(3.0, 4.5); pts = wiggle((cx, cy), pol(cx, cy, L, a), rng, 0.7, 5); e = pts[-1]
    ps = turn(cx, cy, [(0.6, 0.0), (0.6, 0.6), (0.3, 0.9)], 8) + ext(ann(cx, cy, 0.9, 0.5, 12), 0.4) + rod(pts, lambda q: 0.24 + 0.08 * (math.sin(q * 40) > 0), 1.0)
    for sv in (-1, 1): c = pol(e[0], e[1], 0.75, a + sv * 0.9); ps += ext(ann(c[0], c[1], 0.7, 0.36, 12).difference(Polygon([c, pol(c[0], c[1], 2, a + sv * 0.9 - 0.3), pol(c[0], c[1], 2, a + sv * 0.9 + 0.3)])), 0.45)
    return hull([disc(cx, cy, 1.0), disc(e[0], e[1], 1.6)]), [ST(ps, "iron")]

@d("cynder", "debris", "ash", "ash_drift")
def _(cx, cy, a, rng):
    u = rng.uniform; ff = frame(cx, cy, a); L = u(5.0, 7.0); W = u(1.3, 1.8); bow = u(0.6, 1.4)
    pts = [ff(x, bow * (1 - (2 * x / L) ** 2)) for x in np.linspace(-L / 2, L / 2, 14)]
    cuts = [(ell(*ff((i - 1) * 1.2, bow * 0.8 + 0.25 * (1 if i % 2 else -1)), 0.42, 0.26, a, 8), 0.2) for i in range(3)] if rng.random() < 0.5 else []
    return LineString(pts).buffer(W), [ST(rod(pts, lambda q: W * math.sin(max(0.04, min(0.96, q)) * math.pi) ** 0.6, 0.5), "ash", cuts=cuts)]

@d("cynder", "made", "basalt", "imp_idol")
def _(cx, cy, a, rng):
    u = rng.uniform; ff = frame(cx, cy, a); r = u(0.9, 1.15); h = u(1.9, 2.5)
    ps = ext(ngon(cx, cy, r * 1.5, 4, a + 0.785), 0.4) + turn(cx, cy, [(r, 0.0), (r, h * 0.45), (r * 0.6, h * 0.55), (r * 0.8, h * 0.7)] + crv(r * 0.8, 0.3, h * 0.7, h), 12) + [s for sv in (-1, 1) for s in cone(*ff(0, sv * r * 0.6), 0.36, h + 0.6, 0.21, n=6)]
    cuts = [(disc(*ff(r * 0.5, sv * r * 0.3), 0.2, 6), h * 0.72) for sv in (-1, 1)]
    return disc(cx, cy, r * 1.6), [ST(ps, "basalt", cuts=cuts)]

@d("cynder", "marks", "lava", "crust_cracks")
def _(cx, cy, a, rng):
    u = rng.uniform; gs = []; ends = []
    for i in range(rng.randint(3, 5)):
        t = a + 6.2832 * i / 4 + u(-0.4, 0.4); e = pol(cx, cy, u(2.4, 4.0), t); gs.append(wavy((cx, cy), e, rng, 0.5, 4, u(0.4, 0.6))); ends.append(disc(e[0], e[1], 0.3))
        if rng.random() < 0.6: m = pol(cx, cy, 1.6, t); e2 = pol(m[0], m[1], u(1.2, 2.0), t + u(0.6, 1.1) * rng.choice((-1, 1))); gs.append(wavy(m, e2, rng, 0.3, 3, 0.36)); ends.append(disc(e2[0], e2[1], 0.3))
    return hull(ends), [("gcut", U(gs), u(0.35, 0.5), "lava")]

@d("cynder", "made", "basalt", "quench_trough")
def _(cx, cy, a, rng):
    u = rng.uniform; ff = frame(cx, cy, a); L = u(4.0, 5.2); W = u(1.9, 2.3); h = u(1.1, 1.4); b = a + u(0.5, 1.2)
    blade = U([barl(pol(cx, cy, 2.6, b + math.pi), pol(cx, cy, 2.4, b), 0.62), tri(pol(cx, cy, 2.35, b), pol(cx, cy, 3.1, b), 0.62)])
    ps = blk(cx, cy, L, W, a, h)
    return hull([rect(cx, cy, L, W, a), blade]), [ST(ps + ext(blade, h + 0.3), "basalt", cuts=[(rect(cx, cy, L - 1.0, W - 1.0, a).difference(blade.buffer(0.15)), h - u(0.3, 0.5))])]

# ====================================================================================================== targa
@d("targa", "debris", "ice", "fallen_icicles")
def _(cx, cy, a, rng):
    u = rng.uniform; ps = []; foot = []
    for i in range(rng.randint(3, 6)):
        x, y = pol(cx, cy, u(0, 2.2), u(0, 6.28)); b = a + u(-0.6, 0.6); L = u(1.6, 4.0); r = u(0.4, 0.6)
        ps += spin(x, y, b, L, lambda t, L=L, r=r: r * (1 - 0.75 * (t + L / 2) / L) + 0.06, 1.1); foot.append(rect(x, y, L, 2 * r, b))
    return hull(foot).buffer(0.2), [ST(ps, "ice")]

@d("targa", "growth", "snow", "snow_crescent")
def _(cx, cy, a, rng):
    u = rng.uniform; ff = frame(cx, cy, a); L = u(5.0, 8.0); W = u(1.2, 1.9); bow = u(0.8, 2.0)
    pts = [ff(x, bow * (1 - (2 * x / L) ** 2)) for x in np.linspace(-L / 2, L / 2, 16)]
    return LineString(pts).buffer(W), [ST(rod(pts, lambda q: W * math.sin(max(0.04, min(0.96, q)) * math.pi) ** 0.6, u(0.5, 0.8)), "snow")]

@d("targa", "made", "cloth", "pack_and_snowshoes")
def _(cx, cy, a, rng):
    u = rng.uniform; ff = frame(cx, cy, a); L = u(2.6, 3.2); W = u(1.8, 2.2)
    ps = loaf(cx, cy, a, L, W, u(1.0, 1.3)) + lie(*ff(L / 2 + 0.3, 0), a + 1.57, W * 0.9, 0.42) + [s for sv in (-1, 1) for s in blk(*ff(0, sv * W * 0.28), L + 0.1, 0.36, a, 1.38)]
    lay = [ST(ps, "cloth")]
    for sv in (-1, 1)[:rng.randint(1, 2)]:
        c = ff(u(-1, 1), sv * (W / 2 + 1.6)); b = a + u(-0.5, 0.5); o = ell(c[0], c[1], 2.0, 0.85, b, 16); f2 = frame(c[0], c[1], b)
        g = U([o.difference(o.buffer(-0.4))] + [barl(f2(x, -0.8), f2(x, 0.8), 0.3).intersection(o) for x in (-0.9, 0, 0.9)] + [barl(f2(-1.6, 0), f2(1.6, 0), 0.3)])
        lay.append(ST(ext(g, 0.4), "wood"))
    return rect(cx, cy, 4.6, W + 5.4, a), lay

@d("targa", "made", "alien", "pylon_stub")
def _(cx, cy, a, rng):
    u = rng.uniform; r = u(1.1, 1.5); h = u(1.8, 3.0); ps = ext(ngon(cx, cy, r * 1.25, 6, a), 0.4)
    for k in range(5): ps += ext(ngon(cx, cy, r * (1 - 0.07 * k), 6, a), 0.4 + (h - 0.4) * (k + 1) / 5)
    cuts = [(barl(pol(cx, cy, r * 0.25, a + 0.5236 + k * 1.0472), pol(cx, cy, r * 1.3, a + 0.5236 + k * 1.0472), 0.26), u(0.5, h * 0.7)) for k in range(6) if rng.random() < 0.7]
    cuts.append((ngon(cx, cy, r * 0.3, 6, a), h - 0.4))
    return disc(cx, cy, r * 1.35), [ST(ps, "alien", cuts=cuts)]

@d("targa", "made", "ice", "ice_blocks")
def _(cx, cy, a, rng):
    u = rng.uniform; ff = frame(cx, cy, a); n = rng.randint(2, 4); ps = []
    for lv in range(rng.randint(1, 2)):
        for i in range(n - lv):
            c = ff((i - (n - lv - 1) / 2) * 1.75 + u(-0.1, 0.1), u(-0.15, 0.15)); ps += ext(rect(c[0], c[1], 1.6, 1.2, a + u(-0.08, 0.08)).buffer(-0.15).buffer(0.15), 0.9, lift=lv * 0.86)
    return rect(cx, cy, n * 1.75 + 0.4, 1.9, a), [ST(ps, "ice")]

@d("targa", "made", "alien", "pod_lid")
def _(cx, cy, a, rng):
    u = rng.uniform; ff = frame(cx, cy, a); L = u(4.6, 5.8); W = u(2.4, 3.0); h = u(0.8, 1.1)
    ps = spin(cx, cy, a, L, lambda t: W / 2 * max(0.0, 1 - abs(2 * t / L) ** 3) ** 0.5 + 0.05, 2 * h / W) + [s for v in (-L * 0.25, L * 0.25) for s in blk(*ff(v, W / 2), 0.7, 0.6, a, 0.5)]
    win = ell(*ff(L * 0.12, 0), L * 0.22, W * 0.24, a, 16)
    return rect(cx, cy, L + 0.2, W + 0.8, a), [ST(ps, "alien", cuts=[(win, h * 0.72), (win.buffer(0.45).difference(win.buffer(0.22)), h * 0.5)])]

@d("targa", "made", "alien", "conduit_node", ground=True)
def _(cx, cy, a, rng):
    u = rng.uniform; R = u(1.3, 1.7); n = rng.randint(2, 4); gs = []; ends = []
    for k in range(n): t = a + 6.2832 * k / n + u(-0.2, 0.2); e = pol(cx, cy, R + u(2.0, 3.5), t); m = pol(cx, cy, R + 1.2, t); gs.append(line([pol(cx, cy, R + 0.3, t), m, pol(m[0], m[1], math.dist(m, e), t + rng.choice((-0.5236, 0, 0.5236)))], 0.5)); ends.append(disc(e[0], e[1], 0.8))
    ps = ext(ann(cx, cy, R, R - 0.5, 6), 0.6) + dome(cx, cy, R * 0.5, R * 0.7, n=6)
    return hull(ends + [disc(cx, cy, R + 0.3)]), [("gcut", U(gs), 0.35, "slot"), ST(ps, "alien")]

@d("targa", "made", "alien", "disc_wreck")
def _(cx, cy, a, rng):
    u = rng.uniform; R = u(2.2, 3.0); bite = Polygon([pol(cx, cy, R * 0.35, a), pol(cx, cy, 2 * R, a - u(0.3, 0.6)), pol(cx, cy, 2 * R, a + u(0.3, 0.6))])
    ps = turn(cx, cy, [(R, 0.0), (R, 0.2), (R * 0.5, 0.75)] + crv(R * 0.42, 0.3, 0.75, 1.4), 24)
    cuts = [(bite, 0.0)] + [(disc(*pol(cx, cy, R * 0.72, a + 0.8 + k * 0.7), 0.26, 8), 0.3) for k in range(7)]
    return disc(cx, cy, R + 0.2), [ST(ps, "alien", cuts=cuts)]

@d("targa", "debris", "ice", "pressure_ridge")
def _(cx, cy, a, rng):
    u = rng.uniform; ff = frame(cx, cy, a); n = rng.randint(4, 7); ps = []
    for i in range(n): c = ff((i - (n - 1) / 2) * 1.2, u(-0.4, 0.4)); ps += ridge(c[0], c[1], a + 1.57 + u(-0.6, 0.6), u(1.6, 2.6), u(0.9, 1.3), u(0.8, 1.6))
    return rect(cx, cy, n * 1.2 + 1.2, 3.2, a), [ST(ps, "ice")]

@d("targa", "made", "alien", "glyph_tile")
def _(cx, cy, a, rng):
    u = rng.uniform; R = u(1.9, 2.5); k = rng.randint(3, 6)
    cuts = [(ngon(cx, cy, R * 0.62, 6, a).exterior.buffer(0.14), 0.2), (ngon(cx, cy, R * 0.3, k, a).exterior.buffer(0.14), 0.2)] + [(barl(pol(cx, cy, R * 0.3, a + 6.2832 * j / k), pol(cx, cy, R * 0.6, a + 6.2832 * j / k), 0.26), 0.2) for j in range(k)]
    g = ngon(cx, cy, R, 6, a)
    if rng.random() < 0.3: cuts.append((wavy(pol(cx, cy, R * 1.1, a + 1), pol(cx, cy, R * 1.1, a + 4), rng, 0.4, 4, 0.3), 0.0))
    return g.buffer(0.2), [ST(ext(g, 0.45), "alien", cuts=cuts)]

@d("targa", "growth", "frost", "frost_flowers")
def _(cx, cy, a, rng):
    u = rng.uniform; ps = []; foot = []
    for i in range(rng.randint(3, 6)):
        x, y = pol(cx, cy, u(0, 2.2), u(0, 6.28)); r = u(0.7, 1.1); ps += fluted(x, y, r, r * u(0.5, 0.8), rng.randint(6, 9), u(0, 1), 0.45, 4); foot.append(disc(x, y, r))
    return hull(foot).buffer(0.2), [ST(ps, "frost")]

@d("targa", "made", "ice", "fishing_hole", ground=True)
def _(cx, cy, a, rng):
    u = rng.uniform; R = u(1.3, 1.7); ps = []; e = pol(cx, cy, R + u(2.5, 3.5), a)
    for p in ringpts(cx, cy, R + 0.8, rng.randint(4, 7), a + 0.6, 0.2, rng): ps += rock(p[0], p[1], u(0.5, 0.7), u(0.4, 0.7), rng)
    pole = rod([pol(cx, cy, R + 1.7, a + 0.15), e], 0.26, 1.0)
    return hull([disc(cx, cy, R + 1.6), disc(e[0], e[1], 0.4)]), [("gcut", disc(cx, cy, R, 14), 0.6, "water"), ST(ps, "ice"), ST(pole, "wood")]

@d("targa", "made", "alien", "thermal_grille", ground=True)
def _(cx, cy, a, rng):
    u = rng.uniform; ff = frame(cx, cy, a); L = u(3.4, 4.4); W = u(2.0, 2.6); o = ell(cx, cy, L / 2, W / 2, a, 20)
    slots = [(barl(ff(x, -W), ff(x, W), 0.3).intersection(o.buffer(-0.45)), 0.12) for x in np.arange(-L / 2 + 0.7, L / 2 - 0.5, 0.62)]
    melt = o.buffer(u(0.9, 1.3)).difference(o.buffer(0.25))
    return o.buffer(1.5), [("gcut", melt, 0.22, "water"), ST(ext(o, 0.42), "alien", cuts=[c for c in slots if not c[0].is_empty])]

@d("targa", "made", "wood", "sled")
def _(cx, cy, a, rng):
    u = rng.uniform; ff = frame(cx, cy, a); L = u(5.5, 7.0); W = u(2.2, 2.7); ps = []
    for sv in (-1, 1): ps += rod([ff(-L / 2, sv * W / 2), ff(L / 2 - 0.9, sv * W / 2), ff(L / 2, sv * (W / 2 - 0.5))], 0.3, 1.3)
    for x in np.arange(-L / 2 + 0.7, L / 2 - 1.2, 1.25): ps += ext(barl(ff(x, -W / 2), ff(x, W / 2), 0.5), 0.5)
    if rng.random() < 0.6: ps += loaf(*ff(-L * 0.12, 0), a, L * 0.4, W - 0.9, u(0.9, 1.3))                # a lashed-down load
    return rect(cx, cy, L + 0.4, W + 0.8, a), [ST(ps, "wood")]

@d("targa", "made", "bone", "tusk")
def _(cx, cy, a, rng):
    u = rng.uniform; R = u(3.0, 4.5); span = u(1.3, 2.0); pts = arc(cx - R * math.cos(a), cy - R * math.sin(a), R, a - span / 2, a + span / 2, 14); r0 = u(0.65, 0.85)
    return LineString(pts).buffer(r0 + 0.2), [ST(rod(pts, lambda q: r0 * (1 - 0.68 * q), 1.05), "bone")]

@d("targa", "made", "metal", "survey_beacon")
def _(cx, cy, a, rng):
    u = rng.uniform; r = u(0.8, 1.0); h = u(2.2, 3.2); ps = dome(cx, cy, r, r * 1.1) + cyl(cx, cy, 0.3, h, n=6) + dome(cx, cy, 0.42, 0.5, lift=h - 0.1, n=8); tips = []
    for k in range(3): e = pol(cx, cy, r + u(1.3, 1.8), a + k * 2.094); ps += rod([pol(cx, cy, r * 0.5, a + k * 2.094), e], 0.28, 1.0) + ext(disc(e[0], e[1], 0.5, 8), 0.3); tips.append(disc(e[0], e[1], 0.6))
    return hull(tips), [ST(ps, "metal")]

@d("targa", "debris", "crystal", "lattice_shard")
def _(cx, cy, a, rng):
    u = rng.uniform; R = u(1.8, 2.5); gs = [ngon(cx, cy, R * 0.28, 6, a)]
    for k in range(6):
        if rng.random() < 0.15: continue
        t = a + k * 1.0472; gs.append(barl((cx, cy), pol(cx, cy, R, t), 0.5))
        for q in (0.5, 0.75): m = pol(cx, cy, R * q, t); gs += [barl(m, pol(m[0], m[1], R * 0.28, t + sv * 1.0472), 0.42) for sv in (-1, 1)]
    return disc(cx, cy, R + 0.3), [ST(ext(U(gs), u(0.4, 0.6)), "crystal")]

@d("targa", "made", "alien", "buried_steps")
def _(cx, cy, a, rng):
    u = rng.uniform; ff = frame(cx, cy, a); W = u(3.0, 3.8); n = 3; ps = []
    for i in range(n): c = ff(i * 0.5, 0); ps += blk(c[0], c[1], (n - i) * 1.0, W, a, 0.5 * (i + 1))
    c = ff(u(-0.6, 0.6), W / 2 * rng.choice((-1, 1))); r = u(1.5, 2.0)
    return hull([rect(cx, cy, n + 0.2, W, a), disc(c[0], c[1], r)]), [ST(ps + dome(c[0], c[1], r, u(0.9, 1.4), sq=0.8, a=u(0, 3)), "alien")]                # a drift lying over one end

@d("targa", "debris", "ice", "ice_spikes")
def _(cx, cy, a, rng):
    u = rng.uniform; ps = []; foot = []
    for i in range(rng.randint(3, 6)):
        x, y = pol(cx, cy, u(0, 1.9), u(0, 6.28)); r = u(0.45, 0.85); h = r * u(2.2, 3.6); ps += turn(x, y, [(r, 0.0), (r * 0.8, h * 0.2), (r * 0.62, h * 0.25), (r * 0.5, h * 0.55), (r * 0.36, h * 0.6), (0.22, h)], 7, 1.0, 0); foot.append(disc(x, y, r))
    return hull(foot).buffer(0.2), [ST(ps, "ice")]

@d("targa", "made", "alien", "power_cell")
def _(cx, cy, a, rng):
    u = rng.uniform; ff = frame(cx, cy, a); L = u(3.2, 4.2); r = u(0.75, 0.95)
    ps = spin(cx, cy, a, L, lambda t: r * (0.75 if abs(t) > L / 2 - 0.5 else 1.0) + (0.14 if abs(abs(t) - L * 0.2) < 0.22 else 0.0), 1.0, prof="ridge") + [s for sv in (-1, 1) for s in blk(*ff(sv * L * 0.3, 0), 0.6, 2 * r + 0.8, a, 0.42)]
    return rect(cx, cy, L + 0.2, 2 * r + 1.0, a), [ST(ps, "alien", cuts=[(rect(cx, cy, L * 0.24, 0.3, a), r * 0.6)])]


_OWN = set(globals())
