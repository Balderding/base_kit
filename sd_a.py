"""Style-only details, part A: badlands, highland, maahl, corridor, infestation (see style_details.py)."""
import math
from sd_reg import d

# ====================================================================================================== badlands
@d("badlands", "growth", "cactus", "saguaro")
def _(cx, cy, a, rng):
    u = rng.uniform; r = u(0.75, 1.0); h = u(3.0, 4.4); ps = turn(cx, cy, [(r, 0.0), (r, h - r)] + crv(r, 0.24, h - r, h), 14)
    for sv in rng.sample((0.0, math.pi), rng.randint(1, 2)):
        t = a + sv + u(-0.5, 0.5); ra = r * 0.7; ha = h * u(0.5, 0.8); e = pol(cx, cy, r + u(0.9, 1.4), t)
        ps += rod([pol(cx, cy, r * 0.5, t), e], ra, 1.0, lift=h * u(0.15, 0.3)) + turn(e[0], e[1], [(ra, 0.0), (ra, ha - ra)] + crv(ra, 0.24, ha - ra, ha), 12)
    return disc(cx, cy, r + 2.2), [ST(ps, "cactus")]

@d("badlands", "growth", "cactus", "barrel_cactus")
def _(cx, cy, a, rng):
    u = rng.uniform; ps = []; foot = []
    for i in range(rng.randint(1, 3)):
        x, y = pol(cx, cy, 0 if i == 0 else u(1.9, 2.4), a + i * 2.4); r = u(0.9, 1.25) * (1 if i == 0 else 0.75)
        ps += fluted(x, y, r, r * u(1.0, 1.5), rng.randint(8, 11), u(0, 1), 0.78); foot.append(disc(x, y, r + 0.3))
    return hull(foot), [ST(ps, "cactus")]

@d("badlands", "growth", "cactus", "prickly_pear")
def _(cx, cy, a, rng):
    u = rng.uniform; ps = []; foot = []
    for i in range(rng.randint(3, 5)):
        x, y = pol(cx, cy, u(0, 1.5), u(0, 6.28)); L = u(1.5, 2.3); t = u(0, 3.14); h = u(1.2, 2.2)
        ps += loaf(x, y, t, L, 0.62, h) + (loaf(*pol(x, y, L * 0.3, t), t + u(-0.6, 0.6), L * 0.7, 0.6, h * 0.6, lift=h * 0.7) if rng.random() < 0.5 else [])
        foot.append(disc(x, y, L / 2 + 0.3))
    return hull(foot), [ST(ps, "cactus")]

@d("badlands", "made", "bone", "longhorn_skull")
def _(cx, cy, a, rng):
    u = rng.uniform; ff = frame(cx, cy, a); k = u(0.9, 1.25); sweep = u(0.2, 1.0)
    ps = spin(cx, cy, a, 3.0 * k, lambda t: k * (0.62 - 0.17 * t / k), 0.95)
    tips = []
    for sv in (-1, 1):
        if rng.random() < 0.12: continue                                             # a horn snapped off
        pts = [ff(-0.9 * k, sv * 0.4 * k), ff(-1.2 * k, sv * 1.7 * k), ff(-1.2 * k + sweep * k, sv * 3.0 * k)]; tips.append(disc(*pts[-1], 0.3))
        ps += rod(pts, lambda q: 0.42 * k * (1 - 0.55 * q), 1.0)
    cuts = [(disc(*ff(-0.45 * k, sv * 0.42 * k), 0.27 * k, 10), 0.3) for sv in (-1, 1)]
    return hull([rect(cx, cy, 3.2 * k, 1.9 * k, a)] + tips), [ST(ps, "bone", cuts=cuts)]

@d("badlands", "growth", "shrub", "tumbleweed")
def _(cx, cy, a, rng):
    u = rng.uniform; r = u(1.2, 1.9)
    return disc(cx, cy, r + 0.3), [ST(fluted(cx, cy, r, r * u(1.0, 1.3), rng.randint(9, 13), u(0, 1), u(0.62, 0.75), 7, u(0.25, 0.6) * rng.choice((-1, 1))), "shrub")]

@d("badlands", "made", "wood", "hitching_rail")
def _(cx, cy, a, rng):
    u = rng.uniform; ff = frame(cx, cy, a); L = u(5.0, 7.0); h = u(2.2, 2.8); lean = u(-0.25, 0.25)
    ps = cyl(*ff(-L / 2, 0), 0.5, h + 0.3, n=10) + cyl(*ff(L / 2, lean), 0.5, h + 0.3, n=10) + hang(barl(ff(-L / 2 - 0.4, 0), ff(L / 2 + 0.4, lean), 0.62), h - 0.65, h)
    lay = [ST(ps, "wood")]
    if rng.random() < 0.5: lay.append(("up", capl(ff(u(-1, 1), 0.9), ff(u(-1, 1), 2.4), 0.95), 0.4, "leather"))            # reins left trailing
    return rect(*ff(0, 0.6), L + 1.4, 3.4, a), lay

@d("badlands", "made", "wood", "water_trough")
def _(cx, cy, a, rng):
    u = rng.uniform; ff = frame(cx, cy, a); L = u(4.6, 6.4); W = u(1.9, 2.3); h = u(1.2, 1.6)
    ps = blk(cx, cy, L, W, a, h) + blk(*ff(-L * 0.34, 0), 0.7, W + 0.7, a, h * 0.7) + blk(*ff(L * 0.34, 0), 0.7, W + 0.7, a, h * 0.7)
    return rect(cx, cy, L + 0.3, W + 0.9, a), [ST(ps, "wood", cuts=[(rect(cx, cy, L - 0.9, W - 0.9, a), h - u(0.35, 0.6))])]

@d("badlands", "made", "wood", "ox_yoke")
def _(cx, cy, a, rng):
    u = rng.uniform; ff = frame(cx, cy, a); L = u(4.6, 5.8); s = L * 0.27
    ps = spin(cx, cy, a, L, lambda t: 0.5 + 0.18 * math.cos(t / L * 12.6), 1.1)
    for sv in (-1, 1): ps += ext(ann(*ff(sv * s, 1.0), 0.95, 0.42, 16), 0.5)
    return rect(*ff(0, 0.6), L + 0.3, 3.2, a), [ST(ps, "wood")]

@d("badlands", "made", "hide", "rattlesnake")
def _(cx, cy, a, rng):
    u = rng.uniform; R = u(2.0, 2.6); pts = spiral(cx, cy, R, 0.45, u(1.7, 2.2) * rng.choice((-1, 1)), a)
    ps = rod(pts, lambda q: 0.24 + 0.2 * min(1.0, q * 4), 1.0) + dome(pts[-1][0], pts[-1][1], 0.5, 0.95, sq=0.75, a=a)
    return disc(cx, cy, R + 0.5), [ST(ps, "hide")]

@d("badlands", "made", "leather", "saddle")
def _(cx, cy, a, rng):
    u = rng.uniform; ff = frame(cx, cy, a); L = u(3.0, 3.6); W = u(2.2, 2.7); h = u(0.9, 1.2)
    ps = loaf(cx, cy, a, L, W, h) + cyl(*ff(L / 2 - 0.4, 0), 0.38, h + 0.75, n=10) + loaf(*ff(-L / 2 + 0.35, 0), a + math.pi / 2, W * 0.8, 0.7, h + 0.5)
    lay = [ST(ps, "leather")] + [("up", capl(ff(0.2, sv * W / 2), ff(0.2 + u(-0.6, 0.6), sv * (W / 2 + 1.4)), 0.95), 0.35, "leather") for sv in (-1, 1) if rng.random() < 0.8]
    return rect(cx, cy, L + 0.4, W + 3.0, a), lay

@d("badlands", "made", "cloth", "bedroll_canteen")
def _(cx, cy, a, rng):
    u = rng.uniform; ff = frame(cx, cy, a); L = u(3.0, 3.8); r = u(0.75, 0.95)
    ps = lie(cx, cy, a, L, r) + lie(*ff(-L * 0.28, 0), a, 0.45, r + 0.12) + lie(*ff(L * 0.28, 0), a, 0.45, r + 0.12)
    c = ff(u(-1, 1), r + 1.5); b = u(0, 6.28); q = dome(c[0], c[1], 0.95, 0.6) + blk(*pol(c[0], c[1], 1.05, b), 0.6, 0.5, b, 0.45)
    return hull([rect(cx, cy, L, 2 * r + 0.3, a), disc(c[0], c[1], 1.5)]), [ST(ps, "cloth"), ST(q, "tin")]

@d("badlands", "made", "leather", "hat")
def _(cx, cy, a, rng):
    u = rng.uniform; R = u(1.6, 2.0); rc = R * u(0.45, 0.55); h = u(1.1, 1.5)
    ps = turn(cx, cy, [(R, 0.0), (R, 0.28), (rc + 0.12, 0.3), (rc + 0.12, 0.55), (rc, 0.58), (rc, h * 0.8)] + crv(rc, rc * 0.55, h * 0.8, h), 24, u(0.78, 0.9), a)
    return disc(cx, cy, R + 0.2), [ST(ps, "leather", cuts=[(capl(pol(cx, cy, rc * 0.4, a), pol(cx, cy, rc * 0.4, a + math.pi), 0.4), h - 0.22)])]

@d("badlands", "made", "wood", "arrows")
def _(cx, cy, a, rng):
    u = rng.uniform; gs = []
    for i in range(rng.randint(2, 4)):
        f2 = frame(cx + u(-1.2, 1.2), cy + u(-1.2, 1.2), a + u(-0.7, 0.7)); L = u(3.6, 4.6) * (0.55 if rng.random() < 0.25 else 1.0)
        gs.append(U([barl(f2(-L / 2, 0), f2(L / 2, 0), 0.5), tri(f2(L / 2 - 0.1, 0), f2(L / 2 + 0.9, 0), 0.95), Polygon([f2(-L / 2 + 0.1, -0.45), f2(-L / 2 + 1.0, 0), f2(-L / 2 + 0.1, 0.45), f2(-L / 2 - 0.25, 0.45), f2(-L / 2 - 0.25, -0.45)])]))
    g = U(gs)
    return g.convex_hull, [ST(ext(g, 0.42), "wood")]

@d("badlands", "debris", "rock", "cairn")
def _(cx, cy, a, rng):
    u = rng.uniform; r = u(1.3, 1.7); ps = []; z = 0.0; x, y = cx, cy
    for j in range(rng.randint(3, 5)):
        h = r * u(0.55, 0.75); ps += rock(x, y, r, h, rng, lift=z); z += h * 0.72; r *= u(0.68, 0.82); x += u(-0.15, 0.15); y += u(-0.15, 0.15)
        if r < 0.45: break
    return disc(cx, cy, 1.9), [ST(ps, "rock")]

@d("badlands", "made", "iron", "horseshoes")
def _(cx, cy, a, rng):
    u = rng.uniform; gs = []
    for i in range(rng.randint(1, 3)):
        x, y = pol(cx, cy, 0 if i == 0 else u(1.7, 2.2), a + i * 2.3 + u(-0.4, 0.4)); b = u(0, 6.28); r = u(0.75, 0.95)
        gs.append(line(arc(x, y, r, b, b + 4.5), 0.55))
    g = U(gs)
    return g.convex_hull.buffer(0.2), [ST(ext(g, 0.4), "iron")]

@d("badlands", "made", "bone", "antlers")
def _(cx, cy, a, rng):
    u = rng.uniform; ps = []; foot = []
    for sv in (-1, 1)[:rng.randint(1, 2)]:
        f2 = frame(cx, cy + sv * 0.2, a + sv * u(0.25, 0.6)); L = u(3.4, 4.4); beam = [f2(0, 0), f2(L * 0.4, sv * 0.5), f2(L * 0.75, sv * 0.4), f2(L, sv * -0.3)]
        ps += rod(beam, lambda q: 0.42 - 0.2 * q, 1.0)
        for k in range(rng.randint(2, 4)):
            q = 0.25 + 0.22 * k; p = LineString(beam).interpolate(q, normalized=True); e = (p.x + u(-0.3, 0.5), p.y + sv * u(0.9, 1.5))
            ps += rod([(p.x, p.y), e], lambda t: 0.32 - 0.1 * t, 1.0); foot.append(disc(e[0], e[1], 0.3))
        foot.append(LineString(beam).buffer(0.5))
    return hull(foot), [ST(ps, "bone")]

@d("badlands", "made", "tin", "gold_pan")
def _(cx, cy, a, rng):
    u = rng.uniform; R = u(1.5, 1.9); h = u(0.55, 0.7); lay = [ST(turn(cx, cy, [(R * 0.62, 0.0), (R, h)]), "tin", cuts=[(disc(cx, cy, R - 0.45, 20), 0.22)])]
    for i in range(rng.randint(2, 5)):
        x, y = pol(cx, cy, R + u(0.6, 1.3), a + u(-1.2, 1.2)); lay.append(("mound", stone(x, y, u(0.45, 0.6), rng, 0.8, 5), u(0.3, 0.45), "gold", PEB_T))
    return disc(cx, cy, R + 1.5), lay

@d("badlands", "debris", "sandstone", "petrified_log")
def _(cx, cy, a, rng):
    u = rng.uniform; ff = frame(cx, cy, a); r = u(0.8, 1.1); x = -3.2; ps = []; foot = []
    for i in range(rng.randint(3, 4)):
        L = u(1.2, 2.2); c = ff(x + L / 2, u(-0.35, 0.35)); b = a + u(-0.3, 0.3); ps += lie(c[0], c[1], b, L, r * u(0.9, 1.0)); foot.append(rect(c[0], c[1], L, 2 * r, b)); x += L + u(0.3, 0.8)
    return hull(foot), [ST(ps, "sandstone")]

@d("badlands", "debris", "earth", "anthill")
def _(cx, cy, a, rng):
    u = rng.uniform; r = u(1.7, 2.4); h = u(0.8, 1.2)
    lay = [ST(turn(cx, cy, [(r, 0.0), (r * 0.55, h * 0.6), (0.6, h)], 18, u(0.8, 1.0), a), "earth", cuts=[(disc(cx, cy, 0.32, 10), h - 0.5)])]
    return disc(cx, cy, r + 0.2), lay

@d("badlands", "marks", "rut", "sidewinder_tracks")
def _(cx, cy, a, rng):
    u = rng.uniform; ff = frame(cx, cy, a); n = rng.randint(4, 7); gs = []
    for i in range(n):
        x = (i - n / 2) * 1.25; gs.append(line([ff(x - 0.5, -1.1), ff(x, -0.4), ff(x + 0.15, 0.5), ff(x + 0.8, 1.1)], 0.42))
    return rect(cx, cy, n * 1.25 + 1.0, 2.8, a), [("gcut", U(gs), 0.3, "rut")]


_OWN = set(globals())
