"""Style-only details, part D: trederra, fortress, beast, frontier, graveyard (see style_details.py)."""
import math
from sd_reg import d

# ====================================================================================================== trederra
@d("trederra", "made", "wire", "wire_coil")
def _(cx, cy, a, rng):
    u = rng.uniform; ff = frame(cx, cy, a); n = rng.randint(4, 7); R = u(1.0, 1.3); gs = []
    for i in range(n): c = ff((i - (n - 1) / 2) * 1.1, u(-0.15, 0.15)); gs.append(ell(c[0], c[1], 0.75, R, a + u(-0.15, 0.15), 14).exterior.buffer(0.24))
    ps = ext(U(gs), 0.5) + [s for sv in (-1, 1) for s in turn(*ff(sv * (n * 0.55 + 0.6), 0), [(0.5, 0.0), (0.3, 0.5), (0.3, u(1.6, 2.4))], 6)]
    return rect(cx, cy, n * 1.1 + 2.4, 2 * R + 0.4, a), [ST(ps, "wire")]

@d("trederra", "made", "tin", "tin_hat")
def _(cx, cy, a, rng):
    u = rng.uniform; R = u(1.5, 1.8); rc = R * 0.62; h = u(0.8, 1.0)
    ps = turn(cx, cy, [(R, 0.0), (R, 0.16), (rc + 0.15, 0.3)] + crv(rc, 0.3, 0.3, h), 22, u(0.85, 0.95), a)
    cuts = [(disc(*pol(cx, cy, rc * 0.4, a + u(0, 6)), 0.28, 8), 0.0)] if rng.random() < 0.3 else []                       # shot through
    return disc(cx, cy, R + 0.2), [ST(ps, "tin", cuts=cuts)]

@d("trederra", "made", "cloth", "gas_mask")
def _(cx, cy, a, rng):
    u = rng.uniform; ff = frame(cx, cy, a); r = u(1.0, 1.2); c = ff(u(2.6, 3.4), u(-1.2, 1.2)); b = u(0, 3.14)
    ps = dome(cx, cy, r, r * 0.8, sq=0.85, a=a) + cyl(*ff(r * 0.75, 0), 0.42, r * 0.6, n=8) + rod(wiggle(ff(r * 0.9, 0), c, rng, 0.5, 4), 0.28, 1.0) + lie(c[0], c[1], b, 1.7, 0.62)
    cuts = [(ann(*ff(r * 0.05, sv * r * 0.42), 0.36, 0.16, 8), r * 0.5) for sv in (-1, 1)]
    return hull([disc(cx, cy, r + 0.2), disc(c[0], c[1], 1.2)]), [ST(ps, "cloth", cuts=cuts)]

@d("trederra", "made", "brass", "shell_cases")
def _(cx, cy, a, rng):
    u = rng.uniform; ps = []; foot = []
    for i in range(rng.randint(2, 4)):
        x, y = pol(cx, cy, 0 if i == 0 else u(1.6, 2.6), a + i * 1.7); b = a + u(-0.6, 0.6); L = u(2.6, 3.4); r = u(0.5, 0.62)
        ps += spin(x, y, b, L, lambda t, L=L, r=r: r * (1.18 if t < -L / 2 + 0.3 else 1.0 if t < L * 0.3 else 0.8), 1.1); foot.append(rect(x, y, L, 2 * r + 0.3, b))
    if rng.random() < 0.5: p = pol(cx, cy, u(2.8, 3.4), a + 4.2); ps += turn(p[0], p[1], [(0.6, 0.0), (0.6, 1.5)] + crv(0.6, 0.22, 1.5, 2.6), 12); foot.append(disc(p[0], p[1], 0.8))
    return hull(foot).buffer(0.2), [ST(ps, "brass")]

@d("trederra", "made", "tin", "ammo_box")
def _(cx, cy, a, rng):
    u = rng.uniform; ff = frame(cx, cy, a); L = u(2.6, 3.2); W = u(1.4, 1.7); h = u(0.9, 1.2); e = ff(L / 2 + u(1.8, 3.0), u(-1.5, 1.5))
    ps = blk(cx, cy, L, W, a, h) + blk(*ff(0, -W / 2 - 0.65), L, 1.1, a, 0.3)
    belt = rod(wiggle(ff(L / 2 - 0.5, 0), e, rng, 0.6, 5), lambda q: 0.3 + 0.09 * (math.sin(q * 60) > 0), 1.0, lift=0.0)
    return hull([rect(*ff(0, -0.5), L + 0.3, W + 1.4, a), disc(e[0], e[1], 0.4)]), [ST(ps + belt, "tin", cuts=[(rect(*ff(-0.25, 0), L - 1.4, W - 0.7, a), 0.4)])]

@d("trederra", "made", "tin", "periscope")
def _(cx, cy, a, rng):
    u = rng.uniform; ff = frame(cx, cy, a); L = u(5.0, 6.5)
    ps = blk(cx, cy, L, 0.75, a, 0.7) + blk(*ff(L / 2 - 0.3, 0.45), 1.0, 1.4, a, 0.8) + blk(*ff(-L / 2 + 0.3, -0.45), 1.0, 1.4, a, 0.8)
    cuts = [(rect(*ff(L / 2 - 0.3, 0.75), 0.6, 0.5, a), 0.35), (rect(*ff(-L / 2 + 0.3, -0.75), 0.6, 0.5, a), 0.35)]
    return rect(cx, cy, L + 0.3, 2.4, a), [ST(ps, "tin", cuts=cuts)]

@d("trederra", "made", "wood", "field_telephone")
def _(cx, cy, a, rng):
    u = rng.uniform; ff = frame(cx, cy, a); L = u(2.4, 2.9); W = u(1.6, 1.9); h = u(1.0, 1.3); e = ff(u(-3, 3), -W / 2 - u(2.0, 3.2))
    ps = blk(cx, cy, L, W, a, h) + lie(*ff(L / 2 + 0.3, 0), a, 0.6, 0.36, lift=h * 0.3) + rod([ff(-L * 0.3, 0), ff(L * 0.3, 0)], 0.3, 1.0, lift=h + 0.25) + [s for sv in (-1, 1) for s in dome(*ff(sv * L * 0.32, 0), 0.48, 0.5, lift=h)]
    return hull([rect(cx, cy, L + 1.0, W + 0.2, a), disc(e[0], e[1], 0.4)]), [ST(ps, "wood"), ST(rod(wiggle(ff(0, -W / 2 - 0.3), e, rng, 0.5, 4), 0.24, 1.0), "wire")]

@d("trederra", "made", "iron", "mortar")
def _(cx, cy, a, rng):
    u = rng.uniform; ff = frame(cx, cy, a); L = u(3.4, 4.2); R = u(1.2, 1.5)
    ps = turn(*ff(-L / 2, 0), [(R, 0.0), (R, 0.3), (0.6, 0.5)], 14) + spin(cx, cy, a, L, lambda t: 0.5 + 0.1 * (t > L / 2 - 0.4), 1.0 + 0.0) + [s for sv in (-1, 1) for s in rod([ff(L * 0.2, 0), ff(L * 0.35, sv * 1.6)], 0.26, 1.2)]
    lay = [ST(ps, "iron")]
    for i in range(rng.randint(0, 2)): c = ff(u(-1, 1), -2.0 - i * 1.0); lay.append(ST(spin(c[0], c[1], a + u(-0.4, 0.4), 1.9, lambda t: 0.4 * max(0.0, 1 - abs(t) / 1.2) ** 0.5 + 0.1 * (t < -0.6), 1.1), "iron"))
    return rect(*ff(-0.4, -0.6), L + 3.2, 5.6, a), lay

@d("trederra", "made", "cloth", "stretcher")
def _(cx, cy, a, rng):
    u = rng.uniform; ff = frame(cx, cy, a); L = u(8.0, 9.5); W = u(2.3, 2.7)
    ps = [s for sv in (-1, 1) for s in rod([ff(-L / 2, sv * W / 2), ff(L / 2, sv * W / 2)], 0.3, 1.2)] + blk(cx, cy, L - 2.6, W, a, 0.34) + [s for sx in (-1, 1) for s in blk(*ff(sx * (L / 2 - 1.5), 0), 0.5, W + 0.4, a, 0.5)]
    if rng.random() < 0.5: ps += dome(*ff(u(-1.5, 1.5), 0), W * 0.4, 0.8, sq=0.6, a=a)                                     # a blanket left on it
    return rect(cx, cy, L, W + 0.8, a), [ST(ps, "cloth")]

@d("trederra", "made", "tin", "ration_tins")
def _(cx, cy, a, rng):
    u = rng.uniform; ps = []; cuts = []; foot = []
    for i in range(rng.randint(3, 5)):
        x, y = pol(cx, cy, 0 if i == 0 else u(1.3, 2.2), a + i * 1.4 + u(-0.3, 0.3)); r = u(0.5, 0.65); h = u(0.6, 1.0)
        if rng.random() < 0.3: ps += lie(x, y, u(0, 3.14), h + 0.4, r)
        else:
            ps += cyl(x, y, r, h, n=12)
            if rng.random() < 0.5: cuts.append((disc(x, y, r - 0.24, 8), h - 0.4))
        foot.append(disc(x, y, r + 0.3))
    return hull(foot), [ST(ps, "tin", cuts=cuts)]

@d("trederra", "made", "iron", "screw_pickets")
def _(cx, cy, a, rng):
    u = rng.uniform; ps = []; foot = []
    for i in range(rng.randint(2, 3)):
        f2 = frame(cx + u(-0.8, 0.8), cy + (i - 1) * 1.5, a + u(-0.4, 0.4)); L = u(5.0, 6.5); ps += rod([f2(-L / 2, 0), f2(L / 2 - 1.2, 0)] + spiral(*f2(L / 2 - 0.9, 0), 0.3, 0.6, 1.5, a, 14), 0.25, 1.1)
        for x in (-L * 0.3, 0.0, L * 0.25): ps += ext(ann(*f2(x, 0.5), 0.52, 0.25, 8), 0.42)
        foot.append(rect(*f2(0, 0.2), L + 0.6, 1.8, a))
    return hull(foot), [ST(ps, "iron")]

@d("trederra", "made", "iron", "sniper_plate")
def _(cx, cy, a, rng):
    u = rng.uniform; ff = frame(cx, cy, a); L = u(3.4, 4.2); W = u(2.4, 3.0); R = rect(cx, cy, L, W, a)
    ps = ext(R, 0.5) + [s for sx in (-1, 1) for sy in (-1, 1) for s in cyl(*ff(sx * (L / 2 - 0.45), sy * (W / 2 - 0.45)), 0.27, 0.68, n=6)]
    cuts = [(rect(*ff(0, W * 0.1), 1.2, 0.42, a), 0.0)] + [(disc(*ff(u(-L * 0.35, L * 0.35), u(-W * 0.3, -W * 0.05)), 0.25, 6), 0.25) for i in range(rng.randint(0, 3))]
    return R.buffer(0.2), [ST(ps, "iron", cuts=cuts)]

@d("trederra", "made", "iron", "dud_shell", ground=True)
def _(cx, cy, a, rng):
    u = rng.uniform; R = u(1.6, 2.1); r = u(0.6, 0.75); h = u(1.6, 2.4); o = pol(cx, cy, u(0, 0.3), a)
    ps = turn(o[0], o[1], [(r, 0.0), (r, h * 0.7), (r * 0.7, h * 0.78), (r * 0.7, h)], 12) + [s for k in range(4) for s in ext(barl(o, pol(o[0], o[1], r + 0.45, a + k * 1.5708), 0.3), h, lift=0.0)[:0]]
    fins = U([barl(pol(o[0], o[1], 0.2, a + k * 1.5708), pol(o[0], o[1], r + 0.35, a + k * 1.5708), 0.3) for k in range(4)])
    return disc(cx, cy, R + 0.3), [("gcut", ann(cx, cy, R, r + 0.5, 16), 0.35, "pit"), ST(ps + _S().slab(fins, h * 0.72, h), "iron")]

@d("trederra", "made", "leather", "binoculars_map")
def _(cx, cy, a, rng):
    u = rng.uniform; ff = frame(cx, cy, a); L = u(3.4, 4.2); W = u(2.4, 3.0); c = ff(u(-0.6, 0.6), u(-0.4, 0.4)); b = a + u(0.4, 1.2); f2 = frame(c[0], c[1], b)
    bino = [s for sv in (-1, 1) for s in spin(*f2(0, sv * 0.52), b, 1.9, lambda t: 0.4 + 0.12 * (t > 0.3), 1.15)] + blk(c[0], c[1], 0.5, 0.8, b, 0.5)
    ps = ext(rect(cx, cy, L, W, a), 0.24) + [s for q in bino for s in [( (lambda z, f=q[0]: f(max(0.0, z - 0.2)) if z >= 0.2 else f(0.0)), q[1] + 0.2)]]
    cuts = [(rect(*ff(x, 0), 0.2, W, a).difference(disc(c[0], c[1], 1.6)), 0.12) for x in (-L / 6, L / 6)]
    return rect(cx, cy, L + 0.2, W + 0.2, a), [ST(ps, "leather", cuts=[q for q in cuts if not q[0].is_empty])]

@d("trederra", "growth", "flower", "poppies")
def _(cx, cy, a, rng):
    u = rng.uniform; ps = []; foot = []
    for i in range(rng.randint(4, 8)):
        x, y = pol(cx, cy, u(0, 2.4), u(0, 6.28)); r = u(0.5, 0.75); ps += fluted(x, y, r, u(0.45, 0.8), 4, u(0, 1.5), 0.5, 3) + dome(x, y, 0.26, u(0.6, 0.95), n=6); foot.append(disc(x, y, r))
    return hull(foot).buffer(0.2), [ST(ps, "flower")]

@d("trederra", "made", "hide", "rats")
def _(cx, cy, a, rng):
    u = rng.uniform; ps = []; foot = []
    for i in range(rng.randint(1, 3)):
        x, y = pol(cx, cy, 0 if i == 0 else u(2.0, 3.0), a + i * 2.2); b = u(0, 6.28); f2 = frame(x, y, b); L = u(1.7, 2.2)
        ps += spin(x, y, b, L, lambda t, L=L: 0.5 * math.sqrt(max(0.0, 1 - (2 * t / L) ** 2)) * (1.0 - 0.25 * t / L) + 0.05, 1.05) + dome(*f2(L / 2 + 0.1, 0), 0.32, 0.5, sq=0.7, a=b)
        ps += rod([f2(-L / 2, 0), f2(-L / 2 - 0.9, u(-0.5, 0.5)), f2(-L / 2 - 1.8, u(-0.9, 0.9))], 0.21, 1.0); foot.append(disc(x, y, L / 2 + 1.6))
    return hull(foot), [ST(ps, "hide")]

@d("trederra", "marks", "water", "boot_puddles")
def _(cx, cy, a, rng):
    u = rng.uniform; ff = frame(cx, cy, a); n = rng.randint(3, 6); gs = []
    for i in range(n):
        f2 = frame(*ff((i - (n - 1) / 2) * 1.9, 0.75 * (1 if i % 2 else -1)), a + u(-0.25, 0.25)); gs.append(U([ell(*f2(0.35, 0), 0.75, 0.42, a, 10), ell(*f2(-0.75, 0), 0.38, 0.34, a, 8)]))
    return rect(cx, cy, n * 1.9 + 1.2, 3.2, a), [("gcut", U(gs), u(0.3, 0.45), "water")]

@d("trederra", "made", "wood", "stick_grenades")
def _(cx, cy, a, rng):
    u = rng.uniform; ps = []; foot = []
    for i in range(rng.randint(2, 4)):
        x, y = pol(cx, cy, u(0.2, 1.8), a + i * 1.6); b = a + u(-0.7, 0.7); L = u(3.0, 3.6)
        ps += spin(x, y, b, L, lambda t, L=L: 0.5 if t > L * 0.12 else 0.26 if t > -L / 2 + 0.3 else 0.34, 1.1); foot.append(rect(x, y, L, 1.2, b))
    return hull(foot).buffer(0.2), [ST(ps, "wood")]

@d("trederra", "made", "brass", "bugle")
def _(cx, cy, a, rng):
    u = rng.uniform; ff = frame(cx, cy, a); L = u(3.4, 4.2)
    ps = spin(cx, cy, a, L, lambda t: 0.28 + 0.75 * max(0.0, (t - L * 0.15) / (L * 0.35)) ** 2.2, 1.0) + ext(ell(*ff(-L * 0.12, 0), L * 0.3, 0.62, a, 16).exterior.buffer(0.26), 0.5) + cyl(*ff(-L / 2, 0), 0.34, 0.5, n=8)
    return rect(cx, cy, L + 0.3, 2.4, a), [ST(ps, "brass", cuts=[(disc(*ff(L / 2 - 0.2, 0), 0.5, 8), 0.3)])]

@d("trederra", "made", "iron", "machine_gun")
def _(cx, cy, a, rng):
    u = rng.uniform; ff = frame(cx, cy, a); L = u(6.0, 7.5)
    ps = spin(cx, cy, a, L, lambda t: 0.3 if t < -L * 0.25 else 0.52 if t < L * 0.36 else 0.3, 1.1) + blk(*ff(-L * 0.36, 0), L * 0.26, 0.9, a, 0.7) + turn(*ff(-L * 0.12, 0), [(1.1, 0.0), (1.1, 0.95), (0.4, 1.1)], 16)
    ps += [s for sv in (-1, 1) for s in rod([ff(L * 0.3, 0), ff(L * 0.42, sv * 1.6)], 0.25, 1.1)]
    return rect(cx, cy, L + 0.2, 3.6, a), [ST(ps, "iron")]

# ====================================================================================================== fortress
@d("fortress", "made", "paper", "paper_lantern")
def _(cx, cy, a, rng):
    u = rng.uniform; ff = frame(cx, cy, a); L = u(2.8, 3.6); r = u(0.95, 1.2)
    ps = spin(cx, cy, a, L, lambda t: r * (0.72 + 0.28 * math.cos(t / L * 2.4)) + 0.07 * math.sin(t * 9) ** 2 if abs(t) < L / 2 - 0.3 else r * 0.62, 1.1) + ext(ann(*ff(L / 2 + 0.4, 0), 0.6, 0.3, 10), 0.4)
    return rect(*ff(0.3, 0), L + 1.2, 2 * r + 0.3, a), [ST(ps, "paper")]

@d("fortress", "made", "lacquer", "kabuto")
def _(cx, cy, a, rng):
    u = rng.uniform; ff = frame(cx, cy, a); r = u(1.1, 1.35); h = r * 1.0
    neck = Polygon([pol(cx, cy, r * 0.5, a + 3.14)] + arc(cx, cy, r + 0.95, a + 1.3, a + 5.0, 14))
    ps = ext(neck, 0.3) + ext(neck.intersection(disc(cx, cy, r + 0.5)), 0.5) + dome(cx, cy, r, h) + cyl(cx, cy, 0.3, h + 0.25, n=6)
    cr = ff(r * 0.85, 0); ps += ext(disc(cr[0], cr[1], 1.25, 14).difference(disc(*ff(r * 0.85 + 0.75, 0), 1.2, 14)).intersection(disc(cx, cy, r + 2)), h * 0.55)
    cuts = [(barl(pol(cx, cy, 0.4, a + t), pol(cx, cy, r, a + t), 0.2), h * 0.72) for t in np.arange(0.5, 6.0, 0.7)]
    return disc(cx, cy, r + 1.2), [ST(ps, "lacquer", cuts=cuts)]

@d("fortress", "made", "clay", "sake_set")
def _(cx, cy, a, rng):
    u = rng.uniform; ff = frame(cx, cy, a); L = u(3.6, 4.4); W = u(2.2, 2.6); R = rect(cx, cy, L, W, a).buffer(-0.3).buffer(0.3)
    ps = ext(R, 0.3) + ext(R.difference(R.buffer(-0.3)), 0.48) + turn(*ff(-L * 0.25, 0), [(0.5, 0.3), (0.72, 0.9)] + crv(0.72, 0.28, 0.9, 1.6) + [(0.4, 1.9)], 14); cuts = []
    for i in range(rng.randint(2, 3)): c = ff(L * 0.05 + i * 0.9, u(-0.5, 0.5)); ps += turn(c[0], c[1], [(0.3, 0.3), (0.42, 0.8)], 10); cuts.append((disc(c[0], c[1], 0.22, 6), 0.5))
    return R.buffer(0.2), [ST(ps, "clay", cuts=cuts)]

@d("fortress", "growth", "shrub", "bonsai")
def _(cx, cy, a, rng):
    u = rng.uniform; ff = frame(cx, cy, a); L = u(2.6, 3.2); W = u(1.6, 2.0); e = ff(u(0.6, 1.4), u(-0.8, 0.8))
    ps = blk(cx, cy, L, W, a, 0.7) + rod([ff(-L * 0.2, 0), ff(0, u(-0.5, 0.5)), e], lambda q: 0.5 - 0.2 * q, 3.6)
    for i in range(rng.randint(2, 4)): p = pol(e[0], e[1], u(0, 1.0), u(0, 6.28)); r = u(0.8, 1.2); ps += dome(p[0], p[1], r, 0.7, lift=u(1.5, 2.3), n=9)
    return hull([rect(cx, cy, L, W, a), disc(e[0], e[1], 2.0)]), [ST(ps, "shrub")]

@d("fortress", "made", "garden", "tsukubai", ground=True)
def _(cx, cy, a, rng):
    u = rng.uniform; r = u(1.5, 1.9); h = u(1.0, 1.4); c = pol(cx, cy, r * 0.2, a); e = pol(cx, cy, r + u(1.6, 2.4), a + u(-0.4, 0.4))
    ps = turn(cx, cy, [(r, 0.0), (r * 1.02, h * 0.5), (r * 0.9, h)], 11, u(0.85, 1.0), a) + rod([c, e], 0.26, 1.0, lift=h) + cyl(c[0], c[1], 0.45, 0.5, lift=h, n=8)
    return hull([disc(cx, cy, r + 0.3), disc(e[0], e[1], 0.4)]), [ST(ps, "garden", cuts=[(disc(cx, cy, r * 0.55, 12).difference(LineString([c, e]).buffer(0.5)), h - 0.4)])]

@d("fortress", "made", "bamboo", "deer_scarer")
def _(cx, cy, a, rng):
    u = rng.uniform; ff = frame(cx, cy, a); L = u(4.4, 5.6); h = u(1.0, 1.5)
    ps = [s for sv in (-1, 1) for s in cyl(*ff(0, sv * 0.75), 0.36, h + 0.5, n=8)] + spin(cx, cy, a, L, lambda t: 0.42 + 0.08 * (abs((t + L) % 1.3 - 0.65) < 0.1), 1.0, lift=h * 0.5) + rock(*ff(-L / 2 - 0.3, 0), 0.8, 0.7, rng)
    return rect(*ff(-0.3, 0), L + 1.6, 2.6, a), [ST(ps, "bamboo")]

@d("fortress", "made", "wood", "taiko")
def _(cx, cy, a, rng):
    u = rng.uniform; ff = frame(cx, cy, a); L = u(2.6, 3.2); r = u(1.4, 1.7)
    ps = spin(cx, cy, a, L, lambda t: r * (0.86 + 0.14 * math.cos(t / L * 3.0)) + (0.12 if abs(abs(t) - L / 2 + 0.25) < 0.2 else 0.0), 1.1) + [s for sv in (-1, 1) for s in blk(*ff(sv * L * 0.25, 0), 0.6, 2 * r + 0.9, a, 0.6)]
    c = ff(u(-1, 1), r + 1.4); b = a + u(-0.4, 0.4); st = rod([pol(c[0], c[1], 1.6, b), pol(c[0], c[1], 1.6, b + math.pi)], 0.28, 1.1)
    return hull([rect(cx, cy, L + 0.3, 2 * r + 1.0, a), disc(c[0], c[1], 1.7)]), [ST(ps, "wood"), ST(st, "wood")]

@d("fortress", "made", "wood", "geta")
def _(cx, cy, a, rng):
    u = rng.uniform; ps = []; cuts = []; foot = []
    for sv in (-1, 1):
        c = pol(cx, cy, u(0.8, 1.4), a + sv * 1.57 + u(-0.3, 0.3)); b = a + u(-0.6, 0.6); f2 = frame(c[0], c[1], b); R = rect(c[0], c[1], 2.4, 1.1, b).buffer(-0.3).buffer(0.3)
        ps += ext(R, 0.75); cuts += [(barl(f2(0.9, 0), f2(0.2, s2 * 0.4), 0.22), 0.55) for s2 in (-1, 1)] + [(rect(*f2(0, 0), 0.5, 1.4, b), 0.0)][:0]; foot.append(R)
    return hull(foot).buffer(0.2), [ST(ps, "wood", cuts=cuts)]

@d("fortress", "made", "lacquer", "sword_stand")
def _(cx, cy, a, rng):
    u = rng.uniform; ff = frame(cx, cy, a); L = u(4.0, 5.0); h = u(1.4, 1.9)
    ps = blk(cx, cy, L * 0.8, 1.2, a, 0.4) + [s for sv in (-1, 1) for s in blk(*ff(sv * L * 0.3, 0), 0.6, 1.3, a, h)]
    for i in range(rng.randint(1, 2)): v = (i - 0.5) * 0.7; z = h - 0.2 + i * 0.0; ps += rod([ff(-L / 2 - 0.9, v), ff(0, v + 0.25), ff(L / 2 + 0.9, v)], 0.26, 1.0, lift=h * (0.6 + 0.3 * i))
    return rect(cx, cy, L + 2.0, 1.8, a), [ST(ps, "lacquer")]

@d("fortress", "made", "wood", "go_board")
def _(cx, cy, a, rng):
    u = rng.uniform; ff = frame(cx, cy, a); S = u(3.0, 3.6); h = u(0.8, 1.1); n = 5
    cuts = [(barl(ff(x, -S / 2 + 0.3), ff(x, S / 2 - 0.3), 0.18), h - 0.12) for x in np.linspace(-S / 2 + 0.3, S / 2 - 0.3, n)] + [(barl(ff(-S / 2 + 0.3, y), ff(S / 2 - 0.3, y), 0.18), h - 0.12) for y in np.linspace(-S / 2 + 0.3, S / 2 - 0.3, n)]
    ps = blk(cx, cy, S, S, a, h); lay = []
    for sv in (-1, 1): c = ff(sv * (S / 2 + 1.1), u(-0.6, 0.6)); lay.append(ST(turn(c[0], c[1], [(0.55, 0.0), (0.8, 0.7)], 12), "wood", cuts=[(disc(c[0], c[1], 0.5, 8), 0.4)]))
    return rect(cx, cy, S + 4.0, S + 0.2, a), [ST(ps, "wood", cuts=cuts)] + lay

@d("fortress", "made", "paper", "fan")
def _(cx, cy, a, rng):
    u = rng.uniform; R = u(2.6, 3.4); span = u(1.6, 2.6); n = rng.randint(7, 10); g = Polygon([(cx, cy)] + arc(cx, cy, R, a - span / 2, a + span / 2, 18)).difference(disc(cx, cy, 0.5))
    cuts = [(barl(pol(cx, cy, R * 0.3, a - span / 2 + span * k / n), pol(cx, cy, R, a - span / 2 + span * k / n), 0.2), 0.2) for k in range(1, n)]
    return g.buffer(0.2), [ST(ext(g, 0.34) + cyl(cx, cy, 0.36, 0.5, n=8), "paper", cuts=cuts)]

@d("fortress", "made", "iron", "shuriken")
def _(cx, cy, a, rng):
    u = rng.uniform; gs = []
    for i in range(rng.randint(2, 4)):
        x, y = pol(cx, cy, u(0.4, 2.4), a + i * 1.7)
        if rng.random() < 0.6: gs.append(star(x, y, u(1.05, 1.3), 0.55, rng.choice((4, 4, 5, 6)), u(0, 1)).buffer(0.06).difference(disc(x, y, 0.2, 8)))
        else: b = u(0, 6.28); gs.append(U([tri(pol(x, y, 0.2, b), pol(x, y, 1.9, b), 0.7), barl(pol(x, y, 0.3, b), pol(x, y, 1.1, b + math.pi), 0.42), ann(*pol(x, y, 1.45, b + math.pi), 0.46, 0.22, 8)]))
    g = U(gs)
    return g.convex_hull.buffer(0.2), [ST(ext(g, 0.42), "iron")]

@d("fortress", "made", "wood", "shamisen")
def _(cx, cy, a, rng):
    u = rng.uniform; ff = frame(cx, cy, a); L = u(6.5, 8.0); S = u(1.8, 2.2); c = ff(-L / 2 + S / 2, 0)
    ps = ext(rect(c[0], c[1], S, S, a).buffer(-0.4).buffer(0.4), 0.7) + rod([ff(-L / 2 + S, 0), ff(L / 2, 0)], 0.3, 1.2) + [s for k, sv in ((0, 1), (1, -1), (2, 1)) for s in rod([ff(L / 2 - 0.4 - k * 0.5, 0), ff(L / 2 - 0.4 - k * 0.5, sv * 0.9)], 0.22, 1.0)]
    return rect(cx, cy, L + 0.2, S + 0.3, a), [ST(ps, "wood", cuts=[(barl(ff(-L / 2 + 0.4, v), ff(-L / 2 + S - 0.3, v), 0.16), 0.56) for v in (-0.25, 0.0, 0.25)])]

@d("fortress", "made", "bamboo", "bamboo_fence")
def _(cx, cy, a, rng):
    u = rng.uniform; ff = frame(cx, cy, a); L = u(5.0, 6.5); W = u(3.0, 3.8); n = rng.randint(3, 5); ps = []
    for v in (-W * 0.25, W * 0.25): ps += rod([ff(-L / 2, v + u(-0.1, 0.1)), ff(L / 2, v + u(-0.1, 0.1))], 0.3, 1.0)
    for i in range(n):
        if rng.random() < 0.15: continue
        x = (i - (n - 1) / 2) * L / n; ps += rod([ff(x, -W / 2), ff(x + u(-0.2, 0.2), W / 2 * u(0.5, 1.0))], 0.3, 1.0, lift=0.3)
    return rect(cx, cy, L + 0.2, W + 0.2, a), [ST(ps, "bamboo")]

@d("fortress", "made", "sack", "rice_bales")
def _(cx, cy, a, rng):
    u = rng.uniform; ff = frame(cx, cy, a); L = u(2.8, 3.4); r = u(0.9, 1.1); n = rng.randint(2, 3); ps = []
    f_ = lambda t: r * (1.0 if abs(t) < L / 2 - 0.4 else 0.8) + (0.1 if abs(abs(t) - L * 0.25) < 0.16 or abs(t) < 0.16 else 0.0)
    for i in range(n): c = ff(u(-0.15, 0.15), (i - (n - 1) / 2) * 2 * r); ps += spin(c[0], c[1], a, L, f_, 1.1)
    if rng.random() < 0.6: c = ff(u(-0.2, 0.2), (0.0 if n == 2 else r * rng.choice((-1, 1)))); ps += spin(c[0], c[1], a, L, f_, 1.1, lift=r * 1.3)
    return rect(cx, cy, L + 0.3, n * 2 * r + 0.4, a), [ST(ps, "sack")]

@d("fortress", "made", "lacquer", "torii_beam")
def _(cx, cy, a, rng):
    u = rng.uniform; ff = frame(cx, cy, a); L = u(7.5, 9.5); bow = u(0.4, 0.8)
    pts = [ff(x, bow * (2 * x / L) ** 2) for x in np.linspace(-L / 2, L / 2, 12)]
    ps = rod(pts, lambda q: 0.55 + 0.15 * abs(2 * q - 1) ** 2, 1.0) + blk(*ff(0, -1.25), L * 0.72, 0.75, a, 0.6) + [s for sv in (-1, 1) for s in lie(*ff(sv * L * 0.27, -1.9 - u(0.3, 1.2)), a + 1.57 + u(-0.3, 0.3), u(1.2, 2.2), 0.55)]
    return rect(*ff(0, -1.2), L + 0.4, 5.2, a), [ST(ps, "lacquer")]

@d("fortress", "marks", "pan", "raked_gravel", ground=True)
def _(cx, cy, a, rng):
    u = rng.uniform; r = u(0.8, 1.1); n = rng.randint(3, 5); gap = u(0.6, 1.4)
    gs = [LineString(arc(cx, cy, r + 0.7 + k * 0.75, a + gap / 2, a + 6.2832 - gap / 2, 30)).buffer(0.2, 4) for k in range(n)]
    return disc(cx, cy, r + 0.9 + n * 0.75), [("gcut", U(gs), 0.25, "pan"), ST(rock(cx, cy, r, u(0.8, 1.3), rng), "garden")]

@d("fortress", "made", "brass", "koro")
def _(cx, cy, a, rng):
    u = rng.uniform; r = u(1.0, 1.25); h = u(0.9, 1.2)
    ps = turn(cx, cy, [(r * 0.75, 0.0), (r, h * 0.5), (r * 0.92, h), (r * 0.6, h + 0.3), (0.3, h + 0.4), (0.3, h + 0.7)], 16) + [s for p in ringpts(cx, cy, r * 0.95, 3, a) for s in dome(p[0], p[1], 0.36, 0.5, n=8)]
    cuts = [(disc(*p, 0.18, 6), h * 0.8) for p in ringpts(cx, cy, r * 0.5, 5, a + 0.3)]
    return disc(cx, cy, r + 0.5), [ST(ps, "brass", cuts=cuts)]

@d("fortress", "made", "brass", "suzu_bell")
def _(cx, cy, a, rng):
    u = rng.uniform; r = u(0.9, 1.15); e = pol(cx, cy, r + u(3.0, 4.5), a + u(-0.3, 0.3)); pts = wiggle(pol(cx, cy, r * 0.6, a), e, rng, 0.8, 6)
    ps = dome(cx, cy, r, r * 1.3) + ext(ann(cx, cy, r + 0.12, r - 0.3, 16), r * 0.55) + rod(pts, lambda q: 0.36 + 0.08 * math.sin(q * 50), 1.0)
    return hull([disc(cx, cy, r + 0.3), disc(e[0], e[1], 0.5)]), [ST(ps, "brass", cuts=[(barl(pol(cx, cy, r * 0.7, a + 1.57), pol(cx, cy, r * 0.7, a - 1.57), 0.22), r * 0.6)])]

@d("fortress", "made", "lacquer", "menpo")
def _(cx, cy, a, rng):
    u = rng.uniform; ff = frame(cx, cy, a); r = u(1.2, 1.5); h = r * 0.75
    ps = dome(cx, cy, r, h, sq=0.82, a=a) + ridge(*ff(r * 0.1, 0), a, r * 0.8, 0.5, h + 0.3) + [s for sv in (-1, 1) for s in rod([ff(-r * 0.3, sv * r * 0.6), ff(-r * 0.8, sv * (r + u(0.6, 1.4)))], 0.22, 1.0)]
    cuts = [(ell(*ff(-r * 0.3, sv * r * 0.36), r * 0.22, r * 0.13, a + sv * 0.3, 8), h * 0.4) for sv in (-1, 1)] + [(rect(*ff(r * 0.6, 0), 0.26, r * 0.7, a), h * 0.3)] + [(rect(*ff(r * 0.6, v), 0.5, 0.16, a), h * 0.2) for v in (-0.25, 0, 0.25)]
    return disc(cx, cy, r + 1.4), [ST(ps, "lacquer", cuts=cuts)]

# ====================================================================================================== beast
@d("beast", "debris", "tooth", "molar")
def _(cx, cy, a, rng):
    u = rng.uniform; ff = frame(cx, cy, a); L = u(2.8, 3.6); r = u(0.9, 1.1)
    ps = spin(cx, cy, a, L * 0.55, lambda t: r, 1.0)
    for sx in (-1, 1):
        for sy in (-1, 1): c = ff(sx * L * 0.14 - L * 0.0, sy * r * 0.45); ps += dome(c[0], c[1], r * 0.5, r * 1.25, n=8)
    for sv in (-1, 1): ps += rod([ff(L * 0.25, sv * r * 0.4), ff(L * 0.5, sv * r * 0.6), ff(L * 0.75, sv * r * u(0.1, 0.9))], lambda q: r * 0.5 * (1 - 0.6 * q), 1.2)
    return rect(*ff(L * 0.2, 0), L + 0.6, 2 * r + 0.4, a), [ST(ps, "tooth")]

@d("beast", "growth", "flesh", "polyps")
def _(cx, cy, a, rng):
    u = rng.uniform; ps = []; foot = []
    for i in range(rng.randint(3, 7)):
        x, y = pol(cx, cy, u(0, 2.0), u(0, 6.28)); r = u(0.4, 0.65); h = u(1.0, 2.4); ps += turn(x, y, [(r * 0.9, 0.0), (r * 0.6, h * 0.3), (r * 0.6, h * 0.6), (r, h * 0.75)] + crv(r, 0.25, h * 0.75, h), 10); foot.append(disc(x, y, r))
    return hull(foot).buffer(0.2), [ST(ps, "flesh")]

@d("beast", "growth", "flesh", "gut_folds")
def _(cx, cy, a, rng):
    u = rng.uniform; ff = frame(cx, cy, a); n = rng.randint(3, 5); L = u(5.0, 7.5); ps = []; ph = u(0, 6)
    for i in range(n): pts = [ff(x, (i - (n - 1) / 2) * 1.5 + 0.45 * math.sin(x * 1.2 + ph + i)) for x in np.linspace(-L / 2 * u(0.7, 1.0), L / 2 * u(0.7, 1.0), 14)]; ps += rod(pts, lambda q: 0.62 * math.sin(max(0.06, min(0.94, q)) * math.pi) ** 0.5, 1.0)
    return rect(cx, cy, L, n * 1.5 + 1.0, a), [ST(ps, "flesh")]

@d("beast", "made", "tin", "eaten_cuirass")
def _(cx, cy, a, rng):
    u = rng.uniform; ff = frame(cx, cy, a); r = u(1.5, 1.9); h = r * 0.6
    ps = dome(cx, cy, r, h, sq=0.82, a=a) + ridge(cx, cy, a, r * 1.5, 0.5, h + 0.1)
    cuts = [(disc(*ff(r * 0.75, 0), r * 0.4, 10), 0.0)] + [(disc(*ff(-r * 0.1, sv * r * 0.75), r * 0.3, 8), 0.0) for sv in (-1, 1)] + [(blob(*pol(cx, cy, u(0, r * 0.5), u(0, 6.28)), u(0.3, 0.5), rng, 3), h * u(0.2, 0.6)) for i in range(rng.randint(1, 4))]
    return disc(cx, cy, r + 0.2), [ST(ps, "tin", cuts=cuts)]

@d("beast", "growth", "tendril", "parasite_worm", ground=True)
def _(cx, cy, a, rng):
    u = rng.uniform; L = u(3.5, 5.5); e = pol(cx, cy, L, a); pts = [(cx, cy)] + wiggle(pol(cx, cy, 0.9, a), e, rng, 0.9, 5)[1:]; r = u(0.5, 0.65)
    ps = rod(pts, lambda q: r * (1 - 0.3 * q) * (1 + 0.12 * math.sin(q * 38)), 1.5) + dome(e[0], e[1], r * 0.85, r * 1.4)
    return hull([disc(cx, cy, 1.3), disc(e[0], e[1], 0.9)]), [("gcut", ann(cx, cy, 1.25, 0.55, 12), 0.35, "pit"), ST(ps, "tendril")]

@d("beast", "growth", "flesh", "cilia")
def _(cx, cy, a, rng):
    u = rng.uniform; ps = []; foot = []; lean = pol(0, 0, 0.3, a)
    for i in range(rng.randint(9, 16)):
        x, y = pol(cx, cy, u(0, 2.4), u(0, 6.28)); h = u(1.0, 2.0)
        ps += rod([(x, y), (x + lean[0] * h, y + lean[1] * h)], lambda q: 0.32 - 0.1 * q, 1.0) + cone(x, y, 0.32, h, 0.21, n=6); foot.append(disc(x, y, 0.5))
    return hull(foot).buffer(0.2), [ST(ps, "flesh")]

@d("beast", "marks", "bile", "bile_sump", ground=True)
def _(cx, cy, a, rng):
    u = rng.uniform; p = blob(cx, cy, u(1.8, 2.4), rng, 4); e = pol(cx, cy, u(4.0, 5.5), a); ch = wavy(pol(cx, cy, 1.0, a), e, rng, 0.6, 5, u(0.5, 0.7)); pads = []
    for i in range(rng.randint(1, 3)):
        q = disc(*pol(cx, cy, u(0, 1.0), u(0, 6.28)), u(0.5, 0.62), 10)
        if inset(p, 0.3).contains(q) and all(q.distance(g) > 0.3 for g in pads): pads.append(q)
    return hull([p, ch]), [("gcut", U([p, ch]), 0.45, "bile")] + ([("pad", U(pads), "pustule")] if pads else [])

@d("beast", "debris", "tooth", "gallstones")
def _(cx, cy, a, rng):
    u = rng.uniform; ps = []; foot = []
    for i in range(rng.randint(5, 9)):
        x, y = pol(cx, cy, u(0, 1.6), u(0, 6.28)); r = u(0.45, 0.8); lift = max(0.0, 0.8 - math.dist((x, y), (cx, cy)) * 0.6); ps += rock(x, y, r, r * 1.1, rng, n=6, lift=lift); foot.append(disc(x, y, r))
    return hull(foot).buffer(0.2), [ST(ps, "tooth")]

@d("beast", "made", "bone", "vertebrae")
def _(cx, cy, a, rng):
    u = rng.uniform; n = rng.randint(4, 7); R = u(4.0, 9.0) * rng.choice((-1, 1)); ps = []; foot = []; k = u(0.85, 1.1); x, y, t = cx, cy, a
    for i in range(n):
        f2 = frame(x, y, t); ps += dome(x, y, 0.6 * k, 0.7 * k, n=10) + [s for sv in (-1, 1) for s in rod([f2(0, sv * 0.3), f2(u(-0.2, 0.2), sv * 1.3 * k)], lambda q: 0.3 * k * (1 - 0.3 * q), 1.0)] + cone(*f2(0, 0), 0.34 * k, 1.2 * k, 0.21, n=6)
        foot.append(disc(x, y, 1.4 * k)); x, y = f2(1.25 * k + (0.6 if rng.random() < 0.15 else 0), 0); t += 1.25 / R
    return hull(foot), [ST(ps, "bone")]

@d("beast", "made", "bone", "fish_bones")
def _(cx, cy, a, rng):
    u = rng.uniform; ff = frame(cx, cy, a); L = u(4.6, 6.2); ps = rod([ff(-L / 2, 0), ff(L / 2 - 1.0, 0)], 0.27, 1.0) + spin(*ff(L / 2 - 0.5, 0), a, 1.4, lambda t: 0.62 - 0.4 * (t + 0.7) / 1.4, 0.9)
    for x in np.arange(-L / 2 + 1.1, L / 2 - 1.3, 0.6):
        w = 1.25 * math.sin((x + L / 2) / L * 3.0) + 0.3
        for sv in (-1, 1): ps += rod([ff(x, 0), ff(x - 0.35, sv * w)], 0.21, 1.0)
    tail = Polygon([ff(-L / 2 + 0.3, 0), ff(-L / 2 - 0.9, 0.9), ff(-L / 2 - 0.5, 0), ff(-L / 2 - 0.9, -0.9)])
    return rect(cx, cy, L + 1.2, 3.4, a), [ST(ps + ext(tail, 0.28), "bone", cuts=[(disc(*ff(L / 2 - 0.6, 0.0), 0.2, 6), 0.2)])]

@d("beast", "growth", "flesh", "taste_buds")
def _(cx, cy, a, rng):
    u = rng.uniform; ps = []; cuts = []; foot = []
    for i in range(rng.randint(3, 6)):
        x, y = pol(cx, cy, 0 if i == 0 else u(1.7, 2.5), a + i * 1.3); r = u(0.7, 1.05); h = u(0.6, 1.1)
        if all(disc(x, y, r).distance(g) > 0.2 for g in foot): ps += turn(x, y, [(r * 0.75, 0.0), (r, h * 0.8), (r * 0.85, h)], 12); cuts.append((disc(x, y, r * 0.4, 8), h - 0.2)); foot.append(disc(x, y, r))
    return hull(foot).buffer(0.2), [ST(ps, "flesh", cuts=cuts)]

@d("beast", "growth", "vein", "tendon_cords")
def _(cx, cy, a, rng):
    u = rng.uniform; ff = frame(cx, cy, a); L = u(5.0, 7.5); n = rng.randint(2, 4); ps = []
    for sv in (-1, 1): ps += dome(*ff(sv * L / 2, 0), u(0.9, 1.2), u(0.8, 1.1), sq=0.8, a=a + 1.57)
    for i in range(n): v = (i - (n - 1) / 2) * 0.62; ps += rod([ff(-L / 2, v * 0.6), ff(0, v), ff(L / 2, v * 0.6)], lambda q: 0.3 - 0.06 * math.sin(q * math.pi), 1.3)
    return rect(cx, cy, L + 2.4, 2.6, a), [ST(ps, "vein")]

@d("beast", "growth", "flesh", "embedded_eye")
def _(cx, cy, a, rng):
    u = rng.uniform; ff = frame(cx, cy, a); r = u(1.1, 1.5); look = pol(cx, cy, r * u(0.0, 0.3), u(0, 6.28))
    ps = dome(cx, cy, r, r * 0.8) + [s for sv in (-1, 1) for s in rod([ff(-r * 1.3, 0), ff(0, sv * r * 1.05), ff(r * 1.3, 0)], lambda q: 0.42 * math.sin(max(0.08, min(0.92, q)) * math.pi), 1.3)]
    cuts = [(ann(look[0], look[1], r * 0.5, r * 0.5 - 0.2, 12), r * 0.62), (disc(look[0], look[1], r * 0.2, 8), r * 0.5)]
    return rect(cx, cy, r * 2.9, r * 2.6, a), [ST(ps, "flesh", cuts=cuts)]

@d("beast", "marks", "pit", "ulcer", ground=True)
def _(cx, cy, a, rng):
    u = rng.uniform; p = blob(cx, cy, u(1.3, 1.8), rng, 4); ps = []; ex = p.buffer(0.95).exterior; n = rng.randint(6, 9)
    for i in range(n): q = ex.interpolate((i + u(-0.2, 0.2)) / n % 1.0, normalized=True); ps += dome(q.x, q.y, u(0.45, 0.62), u(0.4, 0.7), n=8)
    return p.buffer(1.8), [("gcut", p, u(0.5, 0.8), "pit"), ST(ps, "flesh")]

@d("beast", "made", "wood", "dissolving_barrel")
def _(cx, cy, a, rng):
    u = rng.uniform; n = rng.randint(6, 9); R = u(2.2, 2.9); ps = ext(ann(cx, cy, 1.0, 0.6, 14), 0.5)
    for k in range(n):
        if rng.random() < 0.2: continue
        t = a + 6.2832 * k / n + u(-0.12, 0.12); c = pol(cx, cy, R * 0.6, t); ps += spin(c[0], c[1], t, R * u(0.75, 1.0), lambda q: 0.36 + 0.1 * math.cos(q * 1.4), 0.9, prof="ridge")
    return disc(cx, cy, R + 0.4), [ST(ps, "wood")]

@d("beast", "growth", "vein", "nerve_cluster")
def _(cx, cy, a, rng):
    u = rng.uniform; ps = dome(cx, cy, 0.8, 0.9); foot = [disc(cx, cy, 0.9)]
    def br(p, t, L, r, dep):
        e = pol(p[0], p[1], L, t); ps.extend(rod(wiggle(p, e, rng, 0.3, 3), lambda q: r * (1 - 0.3 * q), 1.1)); foot.append(disc(e[0], e[1], 0.5))
        if dep < 2:
            for sv in (-1, 1)[:rng.randint(1, 2)]: br(e, t + sv * u(0.4, 0.9), L * 0.7, r * 0.75, dep + 1)
        else: ps.extend(dome(e[0], e[1], 0.36, 0.5, n=8))
    for k in range(rng.randint(3, 4)): br(pol(cx, cy, 0.4, a + k * 1.9), a + k * 1.9 + u(-0.3, 0.3), u(1.6, 2.2), 0.38, 0)
    return hull(foot), [ST(ps, "vein")]

@d("beast", "debris", "bone", "bone_spurs")
def _(cx, cy, a, rng):
    u = rng.uniform; ff = frame(cx, cy, a); gap = u(1.2, 2.2); ps = []
    for sv in (-1, 1):
        r0 = u(0.6, 0.8); ps += rod([ff(sv * (gap + 2.2), u(-0.5, 0.5)), ff(sv * (gap + 1.0), u(-0.3, 0.3)), ff(sv * gap * 0.3, u(-0.3, 0.3))], lambda q, r0=r0: r0 * (1 - 0.68 * q), 1.0 + 0.0) + cone(*ff(sv * (gap + 2.0), 0), r0, u(1.6, 2.6), 0.22, n=8)
    return rect(cx, cy, 2 * gap + 5.4, 2.4, a), [ST(ps, "bone")]

@d("beast", "growth", "tooth", "lamprey_mouth", ground=True)
def _(cx, cy, a, rng):
    u = rng.uniform; R = u(1.9, 2.5); ps = ext(ann(cx, cy, R, R - 0.6, 18), 0.5)
    for q, n in ((0.86, rng.randint(9, 12)), (0.58, rng.randint(6, 8))):
        for p in ringpts(cx, cy, R * q, n, a + q): ps += cone(p[0], p[1], 0.3, u(0.7, 1.1), 0.21, n=6)
    return disc(cx, cy, R + 0.2), [("gcut", disc(cx, cy, R * 0.34, 10), 0.8, "pit"), ST(ps, "tooth")]

@d("beast", "growth", "pustule", "split_cyst")
def _(cx, cy, a, rng):
    u = rng.uniform; ff = frame(cx, cy, a); L = u(2.8, 3.6); r = u(1.1, 1.4); p = blob(*ff(u(-0.5, 0.5), r + 1.2), u(0.9, 1.3), rng, 3)
    zz = line([ff(-L * 0.4 + i * L * 0.8 / 6, 0.3 * (1 if i % 2 else -1)) for i in range(7)], 0.34)
    return hull([rect(cx, cy, L, 2 * r, a), p]), [ST(spin(cx, cy, a, L, lambda t: r * math.sqrt(max(0.0, 1 - (2 * t / L) ** 2)) + 0.05, 0.9), "pustule", cuts=[(zz, r * 0.4)]), ("gcut", p, 0.25, "bile")]

@d("beast", "growth", "tooth", "barnacles")
def _(cx, cy, a, rng):
    u = rng.uniform; ps = []; cuts = []; foot = []
    for i in range(rng.randint(4, 8)):
        x, y = pol(cx, cy, u(0, 1.9), u(0, 6.28)); r = u(0.5, 0.9); h = r * u(0.9, 1.3)
        if all(math.dist((x, y), g.centroid.coords[0]) > 0.75 for g in foot): ps += turn(x, y, [(r, 0.0), (r * 0.5, h)], 7, 1.0, u(0, 1)); cuts.append((disc(x, y, r * 0.26, 6), h - 0.35)); foot.append(disc(x, y, r))
    return hull(foot).buffer(0.2), [ST(ps, "tooth", cuts=cuts)]

# ====================================================================================================== frontier
@d("frontier", "made", "brass", "spittoon")
def _(cx, cy, a, rng):
    u = rng.uniform; r = u(1.0, 1.25); h = u(1.1, 1.5); p = blob(*pol(cx, cy, r + 1.0, a), u(0.6, 0.9), rng, 3)
    ps = turn(cx, cy, [(r * 0.8, 0.0), (r, h * 0.35), (r * 0.55, h * 0.7), (r * 1.05, h)], 16)
    return hull([disc(cx, cy, r + 0.3), p]), [ST(ps, "brass", cuts=[(disc(cx, cy, r * 0.5, 10), h - 0.5)])] + ([("gcut", p, 0.2, "tar")] if rng.random() < 0.6 else [])

@d("frontier", "made", "paper", "poker_hand")
def _(cx, cy, a, rng):
    u = rng.uniform; ps = []; foot = []
    for i in range(rng.randint(3, 6)): c = pol(cx, cy, u(0.2, 1.6), a + i * 0.5); q = rect(c[0], c[1], 1.5, 1.0, a + i * 0.35 + u(-0.2, 0.2)); ps += ext(q, 0.22 + 0.05 * i); foot.append(q)
    for i in range(rng.randint(2, 4)): c = pol(cx, cy, u(2.2, 3.0), a + 2.5 + i * 0.8); ps += cyl(c[0], c[1], 0.5, 0.25 * rng.randint(1, 4), n=10); foot.append(disc(c[0], c[1], 0.6))
    return hull(foot).buffer(0.2), [ST(ps, "paper")]

@d("frontier", "made", "board", "wanted_board")
def _(cx, cy, a, rng):
    u = rng.uniform; ff = frame(cx, cy, a); L = u(3.6, 4.6); W = u(2.8, 3.4); R = rect(cx, cy, L, W, a); P = rect(*ff(u(-0.3, 0.3), u(-0.2, 0.2)), L * 0.55, W * 0.72, a + u(-0.15, 0.15))
    ps = ext(R, 0.45) + ext(P, 0.62)
    cuts = [(rect(*ff(0, v), L + 0.2, 0.18, a).difference(P.buffer(0.1)), 0.3) for v in (-W / 6, W / 6)] + [(ell(P.centroid.x, P.centroid.y, L * 0.12, W * 0.16, a, 10), 0.5)]
    return R.buffer(0.2), [ST(ps, "board", cuts=[c for c in cuts if not c[0].is_empty])]

@d("frontier", "made", "board", "batwing_door")
def _(cx, cy, a, rng):
    u = rng.uniform; ff = frame(cx, cy, a); L = u(3.8, 4.6); W = u(2.4, 2.9)
    g = rect(cx, cy, L, W, a).difference(disc(*ff(L / 2 + 0.9, -W / 2), 1.5, 16)).buffer(-0.2).buffer(0.2)
    cuts = [(rect(*ff(x, 0), 0.24, W - 0.9, a).intersection(g.buffer(-0.4)), 0.25) for x in np.arange(-L / 2 + 0.7, L / 2 - 0.9, 0.52)]
    return g.buffer(0.2), [ST(ext(g, 0.5) + [s for v in (-L * 0.3, L * 0.2) for s in blk(*ff(v, W / 2), 0.7, 0.5, a, 0.6)], "board", cuts=[c for c in cuts if not c[0].is_empty])]

@d("frontier", "made", "tin", "washtub")
def _(cx, cy, a, rng):
    u = rng.uniform; r = u(1.5, 1.9); h = u(1.0, 1.3); ff = frame(cx, cy, a); c = ff(r + 1.3, u(-0.6, 0.6)); b = a + u(-0.4, 0.4); f2 = frame(c[0], c[1], b)
    tub = turn(cx, cy, [(r * 0.85, 0.0), (r, h), (r + 0.12, h + 0.02), (r + 0.12, h + 0.14)], 20) + [s for sv in (-1, 1) for s in blk(*ff(0, sv * (r + 0.2)), 0.9, 0.5, a, h * 0.8)]
    wb = ext(rect(c[0], c[1], 1.5, 2.8, b), 0.42)
    return hull([disc(cx, cy, r + 0.6), rect(c[0], c[1], 1.6, 2.9, b)]), [ST(tub, "tin", cuts=[(disc(cx, cy, r - 0.35, 16), h - u(0.3, 0.6))]), ST(wb, "board", cuts=[(rect(*f2(0, v), 1.0, 0.2, b), 0.2) for v in np.arange(-0.9, 0.95, 0.42)])]

@d("frontier", "made", "iron", "branding_irons")
def _(cx, cy, a, rng):
    u = rng.uniform; ps = []; foot = []
    for i in range(rng.randint(2, 3)):
        f2 = frame(cx + u(-0.6, 0.6), cy + (i - 1) * 1.6, a + u(-0.3, 0.3)); L = u(5.0, 6.5); e = f2(L / 2 + 0.6, 0); k = rng.randint(0, 3)
        mark = ann(e[0], e[1], 0.72, 0.36, 10) if k == 0 else ngon(e[0], e[1], 0.8, 3, a).exterior.buffer(0.2) if k == 1 else U([barl(f2(L / 2, v), f2(L / 2 + 1.2, v), 0.36) for v in (-0.5, 0.5)] + [barl(f2(L / 2 + 0.6, -0.5), f2(L / 2 + 0.6, 0.5), 0.36)]) if k == 2 else star(e[0], e[1], 0.85, 0.36, 5, a)
        ps += rod([f2(-L / 2, 0), f2(L / 2, 0)], 0.25, 1.1) + ext(mark, 0.5) + ext(ann(*f2(-L / 2 - 0.4, 0), 0.5, 0.22, 8), 0.4); foot.append(rect(*f2(0.2, 0), L + 2.6, 1.8, a))
    return hull(foot), [ST(ps, "iron")]

@d("frontier", "made", "wood", "butter_churn")
def _(cx, cy, a, rng):
    u = rng.uniform; r = u(0.95, 1.15); h = u(2.2, 2.8)
    if rng.random() < 0.35:
        ff = frame(cx, cy, a); L = h
        ps = spin(cx, cy, a, L, lambda t: r * (1 - 0.25 * (t + L / 2) / L) + (0.1 if abs(abs(t) - L * 0.25) < 0.15 else 0.0), 1.1) + rod([ff(L / 2, 0), ff(L / 2 + 2.2, u(-0.6, 0.6))], 0.25, 1.1)
        return rect(*ff(1.0, 0), L + 2.6, 2 * r + 0.4, a), [ST(ps, "wood")]
    ps = turn(cx, cy, [(r, 0.0), (r + 0.1, 0.3), (r, 0.5), (r * 0.78, h * 0.6), (r * 0.78 + 0.1, h * 0.7), (r * 0.72, h), (r * 0.5, h + 0.15)], 14) + cyl(cx, cy, 0.28, h + u(0.8, 1.4), n=6)
    return disc(cx, cy, r + 0.3), [ST(ps, "wood")]

@d("frontier", "made", "sack", "hay_bale")
def _(cx, cy, a, rng):
    u = rng.uniform; ff = frame(cx, cy, a); L = u(3.4, 4.2); W = u(2.0, 2.4); h = u(1.3, 1.6); f2 = frame(*ff(u(-1, 1), W / 2 + 1.2), a + u(-0.3, 0.3)); Lp = u(5.5, 7.0)
    ps = ext(rect(cx, cy, L, W, a).buffer(-0.3).buffer(0.3), h); cuts = [(rect(*ff(v, 0), 0.2, W + 0.4, a), h - 0.15) for v in (-L * 0.25, L * 0.25)]
    fork = rod([f2(-Lp / 2, 0), f2(Lp / 2 - 1.3, 0)], 0.26, 1.1) + ext(U([barl(f2(Lp / 2 - 1.3, -0.6), f2(Lp / 2 - 1.3, 0.6), 0.36)] + [tri(f2(Lp / 2 - 1.3, v), f2(Lp / 2, v), 0.42) for v in (-0.5, 0, 0.5)]), 0.4)
    return rect(*ff(0, 0.7), max(L, Lp) + 0.4, W + 3.0, a), [ST(ps, "sack", cuts=cuts), ST(fork, "wood")]

@d("frontier", "made", "leather", "boots_and_spurs")
def _(cx, cy, a, rng):
    u = rng.uniform; lay = []; foot = []
    for sv in (-1, 1):
        c = pol(cx, cy, u(1.0, 1.5), a + sv * 1.57); b = a + u(-0.6, 0.6); f2 = frame(c[0], c[1], b); up_ = rng.random() < 0.5
        ps = loaf(*f2(0.5, 0), b, 1.9, 0.9, 0.55) + blk(*f2(-0.5, 0), 0.6, 0.8, b, 0.75) + (cyl(*f2(-0.5, 0), 0.52, u(1.8, 2.4), n=10) if up_ else lie(*f2(-1.7, 0), b, 2.2, 0.5))
        lay.append(ST(ps, "leather")); sp = f2(-0.9 if up_ else -0.6, sv * 1.0)
        lay.append(ST(ext(U([line(arc(sp[0], sp[1], 0.5, b + 1.0, b + 5.2, 8), 0.3), star(*pol(sp[0], sp[1], 0.95, b + 3.14), 0.45, 0.22, 6, 0)]), 0.34), "iron")); foot.append(rect(*f2(-0.6, 0), 4.6, 2.6, b))
    return hull(foot), [lay[0], lay[2]] + [L_ for L_ in (lay[1], lay[3]) if all(L_[1].distance(o[1]) > 0.25 for o in (lay[0], lay[2]))]

@d("frontier", "made", "rope", "lasso")
def _(cx, cy, a, rng):
    u = rng.uniform; R = u(1.7, 2.3); e = pol(cx, cy, R + u(2.5, 4.5), a + u(-0.5, 0.5))
    ps = rod([pol(cx, cy, R * (1 + 0.08 * math.sin(3 * t)), t) for t in np.linspace(a, a + 6.2832, 28)], 0.3, 1.1) + dome(*pol(cx, cy, R, a), 0.5, 0.6) + rod(wiggle(pol(cx, cy, R, a), e, rng, 0.8, 6), 0.28, 1.1)
    if rng.random() < 0.5: ps += rod([pol(cx, cy, R * 0.72, t) for t in np.linspace(a + 0.4, a + 6.0, 22)], 0.3, 1.1, lift=0.3)
    return hull([disc(cx, cy, R + 0.4), disc(e[0], e[1], 0.4)]), [ST(ps, "rope")]

@d("frontier", "made", "clay", "whiskey_jug")
def _(cx, cy, a, rng):
    u = rng.uniform; r = u(1.0, 1.25); h = u(1.9, 2.5); ps = turn(cx, cy, [(r * 0.85, 0.0), (r, h * 0.3), (r, h * 0.55)] + crv(r, 0.36, h * 0.55, h * 0.85) + [(0.36, h), (0.45, h + 0.1)], 16) + ext(ann(*pol(cx, cy, 0.75, a), 0.5, 0.22, 8), h * 0.92)
    lay = [ST(ps, "clay", cuts=[(barl(pol(cx, cy, r * 0.3, a + 3.14 + sv * 0.6 - 0.3), pol(cx, cy, r * 0.75, a + 3.14 + sv * 0.6 + 0.3), 0.18), h * 0.5) for sv in (-1, 0, 1)])]
    for i in range(rng.randint(1, 3)): c = pol(cx, cy, r + u(0.9, 1.6), a + 2.4 + i * 0.9); lay.append(ST(turn(c[0], c[1], [(0.36, 0.0), (0.45, 0.7)], 8), "glass", cuts=[(disc(c[0], c[1], 0.24, 6), 0.3)]))
    return disc(cx, cy, r + 2.0), lay

@d("frontier", "made", "iron", "busted_safe")
def _(cx, cy, a, rng):
    u = rng.uniform; ff = frame(cx, cy, a); S = u(2.6, 3.2); h = u(1.6, 2.1); b = a + u(0.5, 1.3); hinge = ff(S / 2, -S / 2); dc = pol(hinge[0], hinge[1], S / 2, b + 1.5708 * 0 + 0.0)
    door = rect(*pol(hinge[0], hinge[1], S / 2, b), S, 0.5, b)
    ps = blk(cx, cy, S, S, a, h) + ext(door.difference(rect(cx, cy, S + 0.2, S + 0.2, a)), h * 0.95)
    lay = [ST(ps, "iron", cuts=[(rect(*ff(0.25, 0), S - 0.9, S - 0.8, a), 0.4)])]
    for i in range(rng.randint(2, 5)): c = ff(S / 2 + u(0.8, 2.0), u(-1.0, 1.4)); lay.append(("up", disc(c[0], c[1], 0.48, 10), 0.3, "gold"))
    return hull([rect(cx, cy, S, S, a), door, disc(*ff(S / 2 + 1.4, 0.2), 1.6)]), lay

@d("frontier", "made", "leather", "gunbelt")
def _(cx, cy, a, rng):
    u = rng.uniform; R = u(1.7, 2.2); pts = [pol(cx, cy, R * (1 + 0.18 * math.sin(2 * t + 1)), t) for t in np.linspace(a + 0.3, a + 5.9, 24)]; hp = pts[len(pts) // 2]; b = a + u(0, 6.28)
    ps = rod(pts, lambda q: 0.42 + 0.06 * (math.sin(q * 70) > 0.5), 0.9) + spin(*pol(hp[0], hp[1], 0.9, b), b, 2.4, lambda t: 0.62 - 0.3 * (t + 1.2) / 2.4, 0.9) + ext(disc(*pts[0], 0.5, 8), 0.5)
    c = pol(cx, cy, 0.2, a); st = ST(ext(star(c[0], c[1], 0.85, 0.4, 5, a).union(disc(c[0], c[1], 0.35, 8)), 0.34), "tin")
    return disc(cx, cy, R + 2.0), [ST(ps, "leather"), st]

@d("frontier", "made", "wood", "singletree")
def _(cx, cy, a, rng):
    u = rng.uniform; ff = frame(cx, cy, a); L = u(6.0, 8.0); W = u(3.6, 4.4)
    ps = spin(cx, cy, a, L, lambda t: 0.42 + 0.1 * (t < -L / 2 + 1.0), 1.1) + spin(*ff(L / 2, 0), a + 1.5708, W, lambda t: 0.4 - 0.12 * abs(t) / (W / 2), 1.1)
    ps += [s for sv in (-1, 1) for s in ext(ann(*ff(L / 2 + 0.45, sv * (W / 2 - 0.2)), 0.5, 0.24, 8), 0.42)] + ext(ann(*ff(-L / 2 - 0.4, 0), 0.5, 0.24, 8), 0.42)
    return rect(*ff(0.1, 0), L + 1.8, W + 0.3, a), [ST(ps, "wood")]

@d("frontier", "made", "iron", "potbelly_stove")
def _(cx, cy, a, rng):
    u = rng.uniform; ff = frame(cx, cy, a); L = u(3.0, 3.8); r = u(1.1, 1.35); c = ff(L / 2 + u(1.2, 2.2), u(-1.5, 1.5)); b = a + u(-0.8, 0.8)
    ps = spin(cx, cy, a, L, lambda t: r * (0.7 + 0.3 * math.sin(max(0.0, min(1.0, (t + L / 2) / L)) * math.pi) ** 0.6) + (0.1 if abs(t) > L / 2 - 0.3 else 0.0), 1.1) + lie(c[0], c[1], b, u(1.8, 2.8), 0.5)
    return hull([rect(cx, cy, L, 2 * r + 0.3, a), disc(c[0], c[1], 1.6)]), [ST(ps, "iron", cuts=[(rect(*ff(0, 0), L * 0.3, r * 0.8, a), r * 0.8)])]

@d("frontier", "made", "brass", "church_bell")
def _(cx, cy, a, rng):
    u = rng.uniform; ff = frame(cx, cy, a); L = u(3.0, 3.8); r = u(1.3, 1.6)
    ps = spin(cx, cy, a, L, lambda t: r * (0.45 + 0.2 * (t + L / 2) / L + 0.4 * max(0.0, (t - L * 0.1) / (L * 0.4)) ** 2) + (0.1 if abs(t - L * 0.2) < 0.15 else 0.0), 1.05) + blk(*ff(-L / 2 - 0.3, 0), 0.8, 1.6, a, 0.6)
    c = ff(L / 2 + u(0.6, 1.2), u(-0.6, 0.6))
    return rect(*ff(0.4, 0), L + 2.6, 2 * r * 1.1 + 0.3, a), [ST(ps, "brass", cuts=[(wavy(ff(L * 0.2, -r * 1.2), ff(L / 2, -r * 0.2), rng, 0.2, 3, 0.24), 0.2)] if rng.random() < 0.4 else ()), ST(dome(c[0], c[1], 0.5, 0.75), "iron")]

@d("frontier", "made", "board", "shop_sign")
def _(cx, cy, a, rng):
    u = rng.uniform; ff = frame(cx, cy, a); L = u(4.4, 5.6); W = u(2.2, 2.8); o = ell(cx, cy, L / 2, W / 2, a, 24) if rng.random() < 0.5 else rect(cx, cy, L, W, a).buffer(-0.5).buffer(0.5)
    ps = ext(o, 0.42) + ext(o.difference(o.buffer(-0.36)), 0.6) + [s for sv in (-1, 1) for s in ext(ann(*ff(sv * L * 0.3, W / 2 + 0.35), 0.48, 0.22, 8), 0.4)]
    cuts = [(rect(*ff(0, v), L * u(0.4, 0.62), 0.24, a), 0.24) for v in (-W * 0.14, W * 0.14)]
    return rect(*ff(0, 0.3), L + 0.3, W + 1.2, a), [ST(ps, "board", cuts=cuts)]

@d("frontier", "made", "wood", "nail_keg")
def _(cx, cy, a, rng):
    u = rng.uniform; r = u(0.9, 1.1); h = u(1.2, 1.6); c = pol(cx, cy, r + 1.6, a); b = a + u(0.6, 2.2); f2 = frame(c[0], c[1], b)
    ps = turn(cx, cy, [(r * 0.85, 0.0), (r, h * 0.5), (r * 0.88, h)], 14) + [s for i in range(rng.randint(4, 8)) for s in cone(*pol(cx, cy, u(0, r * 0.45), u(0, 6.28)), 0.22, h + u(0.0, 0.3), 0.2, n=5)]
    ham = rod([f2(-1.6, 0), f2(1.0, 0)], 0.26, 1.1) + blk(*f2(1.2, 0), 0.7, 1.7, b, 0.7)
    return hull([disc(cx, cy, r + 0.3), disc(c[0], c[1], 1.9)]), [ST(ps, "wood", cuts=[(ann(cx, cy, r * 0.88 - 0.2, r * 0.5, 12), h - 0.25)]), ST(ham, "iron")]

@d("frontier", "made", "leather", "horse_collar")
def _(cx, cy, a, rng):
    u = rng.uniform; ff = frame(cx, cy, a); A = u(2.0, 2.5); B = A * 0.66
    pts = [ff(A * math.cos(t) * (1.0 + 0.12 * math.cos(t)), B * math.sin(t)) for t in np.linspace(0, 6.2832, 30)]
    ps = rod(pts, lambda q: 0.5 + 0.12 * math.cos(q * 6.2832), 1.1) + [s for sv in (-1, 1) for s in rod([ff(A * 1.2, sv * 0.25), ff(0, sv * (B + 0.42)), ff(-A * 1.1, sv * 0.5)], 0.22, 1.6)]
    return rect(cx, cy, 2 * A + 1.4, 2 * B + 1.6, a), [ST(ps, "leather")]

@d("frontier", "marks", "water", "rut_puddles")
def _(cx, cy, a, rng):
    u = rng.uniform; ff = frame(cx, cy, a); L = u(7.0, 10.0); gs = []; ph = u(0, 6)
    for sv in (-1, 1):
        pts = [ff(x, sv * 1.4 + 0.3 * math.sin(x * 0.7 + ph)) for x in np.linspace(-L / 2, L / 2, 14)]; gs.append(LineString(pts).buffer(0.32, 4)); gs += [ell(p[0], p[1], u(0.8, 1.3), u(0.5, 0.7), a, 10) for p in pts[2:-2] if rng.random() < 0.22]
    return rect(cx, cy, L, 4.4, a), [("gcut", U(gs), 0.32, "water")]

# ====================================================================================================== graveyard
@d("graveyard", "made", "wood", "wooden_cross")
def _(cx, cy, a, rng):
    u = rng.uniform; ff = frame(cx, cy, a); h = u(2.8, 3.8); W = u(2.2, 2.8); tilt = u(-0.25, 0.25)
    if rng.random() < 0.4:
        L = h + 0.8; c = ff(L * 0.18, 0); g = U([rect(cx, cy, L, 0.75, a), rect(c[0], c[1], 0.75, W, a + tilt)])
        return g.convex_hull, [ST(ext(g, 0.5), "wood")]
    ps = blk(cx, cy, 0.62, 0.7, a, h) + hang(rect(cx, cy, 0.62, W, a + tilt), h * 0.6, h * 0.6 + 0.62)
    return disc(cx, cy, W / 2 + 0.4), [ST(ps, "wood"), ("mound", disc(cx, cy, 1.1, 10).difference(rect(cx, cy, 0.9, 1.0, a)), 0.35, "earth", ROCK_T)][:1]

@d("graveyard", "growth", "leaf", "wreath")
def _(cx, cy, a, rng):
    u = rng.uniform; R = u(1.5, 1.9); k = rng.randint(9, 14); bow = pol(cx, cy, R, a)
    ps = rod([pol(cx, cy, R, t) for t in np.linspace(a, a + 6.2832, 36)], lambda q: 0.42 + 0.12 * math.sin(q * 6.2832 * k) ** 2, 1.0) + [s for sv in (-1, 1) for s in rod([bow, pol(bow[0], bow[1], u(1.2, 1.8), a + sv * u(0.3, 0.8))], lambda q: 0.34 - 0.08 * q, 0.7)] + dome(bow[0], bow[1], 0.42, 0.6, n=8)
    return disc(cx, cy, R + 0.5).union(disc(*pol(cx, cy, R + 1.4, a), 1.0)).convex_hull, [ST(ps, "leaf")]

@d("graveyard", "made", "headstone", "broken_obelisk")
def _(cx, cy, a, rng):
    u = rng.uniform; ff = frame(cx, cy, a); S = u(2.0, 2.5); hs = u(1.0, 1.8); c = ff(S / 2 + u(2.2, 3.0), u(-1, 1)); b = a + u(-0.5, 0.5); L = u(2.6, 3.6)
    ps = blk(cx, cy, S, S, a, 0.4) + blk(cx, cy, S * 0.78, S * 0.78, a, 0.8) + blk(cx, cy, S * 0.55, S * 0.55, a + u(-0.05, 0.05), 0.8 + hs)
    ps += spin(c[0], c[1], b, L, lambda t: S * 0.27 * (1 - 0.3 * (t + L / 2) / L) if t < L / 2 - 0.8 else S * 0.19 * max(0.0, (L / 2 - t) / 0.8), 1.0, prof="ridge")
    return hull([rect(cx, cy, S, S, a), rect(c[0], c[1], L, S * 0.6, b)]), [ST(ps, "headstone", cuts=[(tri(ff(-S * 0.3, -S * 0.3), ff(S * 0.2, S * 0.1), 0.9), 0.8 + hs * u(0.4, 0.8))])]

@d("graveyard", "made", "headstone", "angel_wing")
def _(cx, cy, a, rng):
    u = rng.uniform; n = rng.randint(5, 7); ps = []; foot = []; L0 = u(2.6, 3.4)
    for i in range(n):
        t = a + (i - n / 2) * 0.24; L = L0 * (0.6 + 0.4 * math.sin((i + 0.5) / n * 2.4)); c = pol(cx, cy, L / 2 + 0.3, t)
        ps += spin(c[0], c[1], t, L, lambda q, L=L: 0.46 * math.sqrt(max(0.0, 1 - (2 * q / L) ** 2)) + 0.06, 1.0, lift=0.07 * i); foot.append(disc(*pol(cx, cy, L + 0.3, t), 0.4))
    ps += dome(cx, cy, 0.8, 0.8)
    return hull(foot + [disc(cx, cy, 0.9)]), [ST(ps, "headstone")]

@d("graveyard", "made", "coffin", "open_coffin")
def _(cx, cy, a, rng):
    u = rng.uniform; ff = frame(cx, cy, a); L = u(5.4, 6.4); W = u(2.0, 2.4); h = u(0.9, 1.2)
    box_ = lambda f: Polygon([f(-L / 2, -W * 0.3), f(L * 0.22, -W / 2), f(L / 2, -W * 0.32), f(L / 2, W * 0.32), f(L * 0.22, W / 2), f(-L / 2, W * 0.3)])
    g = box_(ff); f2 = frame(*ff(u(-0.6, 0.6), W + u(0.2, 0.7)), a + u(-0.25, 0.25)); lid = box_(f2)
    lay = [ST(ext(g, h), "coffin", cuts=[(g.buffer(-0.42), u(0.3, 0.5))]), ST(ext(lid, 0.4) + ext(U([barl(f2(-L * 0.25, 0), f2(L * 0.38, 0), 0.4), barl(f2(L * 0.2, -W * 0.3), f2(L * 0.2, W * 0.3), 0.4)]), 0.56), "coffin")]
    return hull([g, lid]), lay

@d("graveyard", "made", "wax", "candles")
def _(cx, cy, a, rng):
    u = rng.uniform; ps = []; foot = []
    for i in range(rng.randint(3, 6)):
        x, y = pol(cx, cy, u(0, 1.6), u(0, 6.28)); r = u(0.34, 0.5); h = u(0.6, 2.4)
        ps += turn(x, y, [(r * 1.9, 0.0), (r * 1.2, 0.2), (r, 0.3), (r, h), (0.2, h + 0.02), (0.2, h + 0.3)], 8); foot.append(disc(x, y, r * 2))
    return hull(foot).buffer(0.2), [ST(ps, "wax")]

@d("graveyard", "made", "fence", "fallen_railing")
def _(cx, cy, a, rng):
    u = rng.uniform; ff = frame(cx, cy, a); L = u(5.0, 6.5); W = u(3.0, 3.8); gs = [barl(ff(-L / 2, v), ff(L / 2, v), 0.5) for v in (-W * 0.3, W * 0.25)]
    for x in np.arange(-L / 2 + 0.5, L / 2 - 0.2, 0.95):
        if rng.random() < 0.12: continue
        bend = u(-0.3, 0.3) * (rng.random() < 0.3); gs += [barl(ff(x, -W / 2), ff(x + bend, W / 2 - 0.5), 0.42), tri(ff(x + bend, W / 2 - 0.6), ff(x + bend, W / 2 + 0.3), 0.8)]
    return rect(cx, cy, L + 0.2, W + 0.8, a), [ST(ext(U(gs), 0.42), "fence")]

@d("graveyard", "made", "bone", "skull_pile")
def _(cx, cy, a, rng):
    u = rng.uniform; ps = []; cuts = []; foot = []; n = rng.randint(3, 6)
    for i in range(n):
        top = i == n - 1 and n > 3; x, y = (cx, cy) if top else pol(cx, cy, u(0.9, 1.3), a + 6.2832 * i / (n - (n > 3))); b = u(0, 6.28); f2 = frame(x, y, b); r = u(0.62, 0.75); z = 0.8 if top else 0.0
        ps += dome(x, y, r, r * 1.25, lift=z, sq=0.85, a=b) + blk(*f2(r * 0.8, 0), 0.5, r * 0.9, b, r * 0.5, lift=z)
        cuts += [(disc(*f2(r * 0.45, sv * r * 0.36), 0.2, 6), z + r * 0.55) for sv in (-1, 1)]; foot.append(disc(x, y, r + 0.3))
    return hull(foot).buffer(0.2), [ST(ps, "bone", cuts=cuts)]

@d("graveyard", "made", "iron", "robbers_hole", ground=True)
def _(cx, cy, a, rng):
    u = rng.uniform; ff = frame(cx, cy, a); p = blob(cx, cy, u(1.4, 1.9), rng, 4); m = blob(*ff(u(2.8, 3.4), u(-1, 1)), u(1.3, 1.7), rng, 4); f2 = frame(*ff(u(-1, 1), -3.0), a + u(-0.4, 0.4)); L = u(5.0, 6.5)
    bar = rod([f2(-L / 2, 0), f2(L / 2 - 0.6, 0), f2(L / 2, 0.55)], 0.26, 1.1)
    return hull([p, m, rect(*f2(0, 0), L, 1.2, a)]), [("gcut", p, u(0.6, 0.85), "pit"), ("mound", m, u(0.7, 1.1), "earth", SOFT_T), ST(bar, "iron")]

@d("graveyard", "made", "hide", "raven")
def _(cx, cy, a, rng):
    u = rng.uniform; ff = frame(cx, cy, a); L = u(2.4, 3.0); dead = rng.random() < 0.4
    ps = spin(cx, cy, a, L, lambda t: 0.62 * math.sqrt(max(0.0, 1 - (2 * t / L) ** 2)) * (1 + 0.25 * t / L) + 0.06, 1.2) + dome(*ff(L / 2 + 0.2, 0), 0.45, 1.2 if not dead else 0.6, lift=0.0) + ext(tri(ff(L / 2 + 0.5, 0), ff(L / 2 + 1.4, u(-0.2, 0.2)), 0.5), 0.5 if dead else 0.95)
    ps += ext(Polygon([ff(-L / 2 + 0.3, -0.3), ff(-L / 2 - 1.2, -0.6), ff(-L / 2 - 1.4, 0), ff(-L / 2 - 1.2, 0.6), ff(-L / 2 + 0.3, 0.3)]), 0.34)
    if dead: ps += [s for sv in (-1, 1) for s in ext(Polygon([ff(0.5, sv * 0.3), ff(0.2, sv * 2.4), ff(-0.6, sv * 2.2), ff(-1.2, sv * 1.2), ff(-0.6, sv * 0.3)]), 0.3)]
    return rect(*ff(-0.2, 0), L + 3.2, 5.2 if dead else 1.8, a), [ST(ps, "hide")]

@d("graveyard", "made", "brass", "grave_bell", ground=True)
def _(cx, cy, a, rng):
    u = rng.uniform; h = u(2.2, 3.0); e = pol(cx, cy, u(2.0, 3.0), a)
    ps = cyl(cx, cy, 0.36, h, n=6) + turn(*pol(cx, cy, 0.75, a), [(0.62, 0.0), (0.62, h * 0.55), (0.3, h * 0.8)], 10) + hang(barl((cx, cy), pol(cx, cy, 0.9, a), 0.4), h - 0.45, h - 0.1)
    return hull([disc(cx, cy, 1.5), disc(e[0], e[1], 0.6)]), [ST(ps, "brass"), ST(rod(wiggle(pol(cx, cy, 1.6, a), e, rng, 0.3, 3), 0.24, 1.0), "rope"), ("gcut", disc(e[0], e[1], 0.42, 8), 0.5, "pit")]

@d("graveyard", "made", "headstone", "effigy_slab")
def _(cx, cy, a, rng):
    u = rng.uniform; ff = frame(cx, cy, a); L = u(6.0, 7.0); W = u(2.6, 3.0)
    ps = blk(cx, cy, L, W, a, 0.5) + spin(*ff(-0.3, 0), a, L * 0.68, lambda t: W * 0.3 * (1 - 0.3 * abs(t) / L) , 0.55, lift=0.45) + dome(*ff(L * 0.36, 0), W * 0.2, 0.55, lift=0.45) + blk(*ff(L * 0.1, 0), 0.7, W * 0.5, a, 0.35, lift=0.8) + blk(*ff(-L * 0.42, 0), 0.6, W * 0.4, a, 0.5, lift=0.45)
    cuts = [(wavy(ff(u(-2, 2), -W), ff(u(-2, 2), W), rng, 0.3, 4, 0.3), 0.0)] if rng.random() < 0.35 else []
    return rect(cx, cy, L + 0.2, W + 0.2, a), [ST(ps, "headstone", cuts=cuts)]

@d("graveyard", "made", "clay", "toppled_vase")
def _(cx, cy, a, rng):
    u = rng.uniform; ff = frame(cx, cy, a); L = u(2.6, 3.2); r = u(0.75, 0.95)
    ps = spin(cx, cy, a, L, lambda t: r * (0.5 + 0.5 * math.sin(max(0.0, min(1.0, (t + L / 2) / (L * 0.75))) * math.pi) ** 0.7) if t < L * 0.25 else r * 0.45 + 0.4 * (t - L * 0.25), 1.15)
    lay = [ST(ps, "clay")]
    for i in range(rng.randint(2, 5)): c = ff(L / 2 + u(0.7, 2.2), u(-1.3, 1.3)); lay.append(ST(fluted(c[0], c[1], u(0.45, 0.62), 0.36, 5, u(0, 1), 0.5, 2), "flower"))
    return rect(*ff(1.0, 0), L + 2.8, 3.2, a), lay[:1] + [L_ for i, L_ in enumerate(lay[1:]) if all(L_[1].distance(o[1]) > 0.3 for o in lay[1:1 + i])]

@d("graveyard", "marks", "pit", "sunken_grave", ground=True)
def _(cx, cy, a, rng):
    u = rng.uniform; ff = frame(cx, cy, a); L = u(5.4, 6.4); W = u(2.0, 2.4)
    g = Polygon([ff(-L / 2, -W * 0.3), ff(L * 0.22, -W / 2), ff(L / 2, -W * 0.32), ff(L / 2, W * 0.32), ff(L * 0.22, W / 2), ff(-L / 2, W * 0.3)]).buffer(0.2)
    return g.buffer(0.3), [("gcut", g, 0.3, "pit"), ("gcut", blob(*ff(u(-1, 1), 0), W * 0.36, rng, 3).intersection(g.buffer(-0.4)), 0.65, "pit")]

@d("graveyard", "made", "headstone", "footstones")
def _(cx, cy, a, rng):
    u = rng.uniform; ff = frame(cx, cy, a); n = rng.randint(3, 5); ps = []
    for i in range(n):
        if rng.random() < 0.15: continue
        c = ff((i - (n - 1) / 2) * 2.0, u(-0.3, 0.3)); ps += loaf(c[0], c[1], a + u(-0.35, 0.35), u(1.2, 1.5), 0.62, u(1.0, 1.9))
    if not ps: return None
    return rect(cx, cy, n * 2.0 + 0.4, 1.8, a), [ST(ps, "headstone")]

@d("graveyard", "growth", "leaf", "ivy")
def _(cx, cy, a, rng):
    u = rng.uniform; L = u(5.0, 8.0); pts = wiggle(pol(cx, cy, L / 2, a + 3.14), pol(cx, cy, L / 2, a), rng, 1.2, 7); ps = rod(pts, 0.25, 1.0); ln = LineString(pts)
    for q in np.arange(0.06, 0.97, 0.09):
        p = ln.interpolate(q, normalized=True); sv = rng.choice((-1, 1)); c = pol(p.x, p.y, 0.6, a + sv * 1.57 + u(-0.4, 0.4)); ps += ext(star(c[0], c[1], u(0.48, 0.62), 0.3, 3, u(0, 2)).buffer(0.1), 0.3)
    return ln.buffer(1.3), [ST(ps, "leaf")]

@d("graveyard", "made", "hide", "dead_bat")
def _(cx, cy, a, rng):
    u = rng.uniform; ff = frame(cx, cy, a); S = u(2.2, 2.9); ps = spin(cx, cy, a, 1.5, lambda t: 0.4 - 0.1 * abs(t), 1.2) + [s for sv in (-1, 1) for s in cone(*ff(0.75, sv * 0.25), 0.22, 0.8, 0.2, n=5)]
    for sv in (-1, 1):
        w = Polygon([ff(0.4, sv * 0.2), ff(0.9, sv * S * 0.5), ff(0.5, sv * S)] + [ff(-0.2 - 0.45 * math.sin(k * 3.14 / 1.0) ** 2, sv * S * (1 - k / 3)) for k in range(1, 3)] + [ff(-0.5, sv * 0.2)])
        ps += ext(w.buffer(0), 0.28) + rod([ff(0.4, sv * 0.2), ff(0.9, sv * S * 0.5), ff(0.5, sv * S)], 0.21, 1.0)
    return rect(cx, cy, 2.4, 2 * S + 0.4, a), [ST(ps, "hide")]

@d("graveyard", "made", "gold", "chalice_rosary")
def _(cx, cy, a, rng):
    u = rng.uniform; ff = frame(cx, cy, a); L = u(2.4, 3.0); c = ff(u(-0.5, 0.5), u(2.2, 2.8)); R = u(1.1, 1.5)
    ps = spin(cx, cy, a, L, lambda t: 0.75 - 0.5 * (t + L / 2) / (L * 0.3) if t < -L * 0.2 else 0.25 if t < 0 else 0.25 + 0.6 * (t / (L / 2)) ** 0.6, 1.1)
    beads = rod([pol(c[0], c[1], R * (1 + 0.1 * math.sin(3 * t)), t) for t in np.linspace(0, 6.2832, 30)], lambda q: 0.24 + 0.12 * (math.sin(q * 90) > 0), 1.0) + ext(U([barl(pol(c[0], c[1], R, a), pol(c[0], c[1], R + 1.5, a), 0.36), barl(pol(c[0], c[1], R + 0.9, a + 0.3), pol(c[0], c[1], R + 0.95, a - 0.3), 0.36)]), 0.34)
    return hull([rect(cx, cy, L, 1.8, a), disc(c[0], c[1], R + 1.6)]), [ST(ps, "gold"), ST(beads, "wood")]

@d("graveyard", "made", "headstone", "gargoyle_head")
def _(cx, cy, a, rng):
    u = rng.uniform; ff = frame(cx, cy, a); r = u(1.3, 1.7); h = r * 0.9
    ps = dome(cx, cy, r, h, sq=0.85, a=a) + loaf(*ff(r * 0.75, 0), a, r * 1.0, r * 0.9, h * 0.6) + [s for sv in (-1, 1) for s in cone(*ff(-r * 0.3, sv * r * 0.6), 0.42, h + u(0.4, 1.0), 0.22, n=6)] + [s for sv in (-1, 1) for s in ext(tri(ff(-r * 0.1, sv * r * 0.8), ff(-r * 0.5, sv * (r + 0.9)), 0.8), h * 0.5)]
    cuts = [(disc(*ff(r * 0.2, sv * r * 0.36), 0.24, 6), h * 0.5) for sv in (-1, 1)] + [(rect(*ff(r * 0.95, 0), 0.5, r * 0.55, a), h * 0.2)]
    return disc(cx, cy, r + 1.0), [ST(ps, "headstone", cuts=cuts)]

@d("graveyard", "made", "fence", "mortsafe")
def _(cx, cy, a, rng):
    u = rng.uniform; ff = frame(cx, cy, a); L = u(6.0, 7.0); W = u(2.6, 3.0); R = rect(cx, cy, L, W, a)
    gs = [R.difference(R.buffer(-0.5))] + [barl(ff(x, -W / 2), ff(x, W / 2), 0.42) for x in np.arange(-L / 2 + 1.0, L / 2 - 0.6, 1.0)] + [barl(ff(-L / 2, 0), ff(L / 2, 0), 0.42)]
    ps = ext(R, 0.3) + ext(U(gs), 0.85)
    return R.buffer(0.2), [ST(ps, "fence")]


_OWN = set(globals())
