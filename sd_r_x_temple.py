"""Third drawing, temple (see sd_r_x_frontier.py): carved stone at its real size beside a 32 mm figure. The calendar
stone is a thick wheel of carving fallen flat, the lintel lies broken in two with its glyph blocks, the lotus basin
stands knee high in its rows of petals."""
import math
from sd_reg import d
from sd_r_x_frontier import x_d


def x_glyph(c, t, s, rng):
    """the lines of one carved glyph block: a rounded cartouche cut round, carved inside with a face, a dot-and-bar
    number, a spiral or a crossed sign (cut them all into a stone at once)"""
    f2 = frame(c[0], c[1], t); g = rrect(c[0], c[1], s, s, t, s * 0.22); gs = [g.difference(inset(g, 0.13))]
    k = rng.randint(0, 3)
    if k == 0: gs += [disc(*f2(sx * s * 0.2, s * 0.12), s * 0.13, 8) for sx in (-1, 1)] + [barl(f2(-s * 0.22, -s * 0.2), f2(s * 0.22, -s * 0.2), 0.13)]
    elif k == 1: nd = rng.randint(1, 3); gs += [disc(*f2((j - (nd - 1) / 2) * s * 0.26, s * 0.18), s * 0.12, 8) for j in range(nd)] + [barl(f2(-s * 0.3, -s * 0.12), f2(s * 0.3, -s * 0.12), 0.14)]
    elif k == 2: gs.append(line(spiral(c[0], c[1], s * 0.32, s * 0.06, 1.6, t, 20), 0.12))
    else: gs += [barl(f2(-s * 0.3, -s * 0.3), f2(s * 0.3, s * 0.3), 0.12), barl(f2(-s * 0.3, s * 0.3), f2(s * 0.3, -s * 0.3), 0.12), ann(c[0], c[1], s * 0.36, s * 0.27, 12)]
    return U(gs)


@x_d("temple", "made", "tcolumn", "calendar_stone")
def _(cx, cy, a, rng):
    """a great calendar stone fallen flat on its back, a thick wheel of carving: the face of the sun in the middle with
    its eyes and the tongue out, the four panels of the age signs about it, the ring of twenty day signs each in its
    box, the pointed rays, the outer band of beads; a slab broken out of its edge on some"""
    u = rng.uniform; R = u(2.6, 2.8); h = u(1.7, 2.0); o = Obj("tcolumn")
    g = disc(cx, cy, R, 40)
    if rng.random() < 0.35: tb = u(0, 6.28); g = g.difference(Polygon([pol(cx, cy, R * 0.55, tb), pol(cx, cy, R * 1.3, tb - u(0.3, 0.5)), pol(cx, cy, R * 1.3, tb + u(0.4, 0.6))]))
    b = o.up(g, h)
    o.cut(U([disc(*pol(cx, cy, R - 0.28, a + 6.2832 * k / 40), 0.12, 6) for k in range(40)]), b, 0.1)          # the beads round the edge
    o.groove(disc(cx, cy, R - 0.55, 36), b, 0.14, 0.12)
    o.on(U([tri(pol(cx, cy, R * 0.55, a + 6.2832 * (k + 0.5) / 8), pol(cx, cy, R - 0.62, a + 6.2832 * (k + 0.5) / 8), 0.9) for k in range(8)]), b, 0.25)          # the rays
    rd = o.on(ann(cx, cy, R * 0.56, R * 0.38, 32), b, 0.3)
    o.cut(U([barl(pol(cx, cy, R * 0.37, a + 6.2832 * k / 20), pol(cx, cy, R * 0.57, a + 6.2832 * k / 20), 0.1) for k in range(20)]), rd, 0.1)          # the boxes of the day signs
    o.cut(U([disc(*pol(cx, cy, R * 0.47, a + 6.2832 * (k + 0.5) / 20), 0.14, 6) for k in range(20)]), rd, 0.08)
    for k in range(4):          # the four age panels round the face
        t = a + 0.785 + k * 1.5708; c = pol(cx, cy, R * 0.25, t); pn = o.on(rrect(c[0], c[1], R * 0.17, R * 0.15, t, 0.06), b, 0.3)
        o.cut(disc(c[0], c[1], 0.12, 6), pn, 0.08)
    fc = o.on(disc(cx, cy, R * 0.16, 20), b, 0.5); f2 = frame(cx, cy, a)
    o.cut(U([disc(*f2(R * 0.05, sv * R * 0.06), 0.13, 8) for sv in (-1, 1)]), fc, 0.1)
    o.on(tri(f2(-R * 0.02, 0), f2(-R * 0.2, 0), 0.48), fc, 0.12)
    return o.out()

@x_d("temple", "made", "tcolumn", "glyph_lintel")
def _(cx, cy, a, rng):
    """a carved lintel fallen and broken in two: a long dressed block with a plain margin and a row (or two) of glyph
    cartouches along its face, each carved differently; the break jagged, chips of stone lying in the gap"""
    u = rng.uniform; ff = frame(cx, cy, a); L = u(8.6, 9.0); W = u(1.8, 2.0); h = u(1.6, 1.85); o = Obj("tcolumn")
    xb = u(-0.25, 0.25) * L; gap = u(0.3, 0.6)
    jag = Polygon([ff(xb + u(-0.25, 0.25), v) for v in np.linspace(-W, W, 7)] + [ff(xb + L, W), ff(xb + L, -W)])
    whole = rect(cx, cy, L, W, a)
    pieces = [(whole.difference(jag), 0.0, 0.0), (whole.intersection(jag), gap, u(-0.12, 0.12))]
    s = W - 0.95
    for g, dx, db in pieces:
        c0 = g.centroid; g = affinity.rotate(affinity.translate(g, dx * math.cos(a), dx * math.sin(a)), db, origin=(c0.x, c0.y), use_radians=True)
        if g.is_empty: continue
        t = a + db; f2 = frame(g.centroid.x, g.centroid.y, t); b = o.up(g, h * (1.0 if dx == 0 else u(0.92, 1.0)))
        inner = inset(g, 0.4); gl = [inset(g, 0.2).difference(inset(g, 0.32))]
        for x in np.arange(-L, L, s + 0.18):
            c = f2(x, 0)
            if inner.contains(rrect(c[0], c[1], s, s, t, 0.2)): gl.append(x_glyph(c, t, s, rng))
        o.cut(U(gl), b, 0.12)
    for i in range(rng.randint(2, 3)):
        c = ff(xb + gap / 2 + u(-0.3, 0.3), rng.choice((-1, 1)) * u(W / 2 + 0.3, W / 2 + 0.8)); o.up(Polygon([pol(c[0], c[1], u(0.28, 0.45), k * 1.25 + u(-0.2, 0.2)) for k in range(5)]), u(0.4, 0.6))
    return o.out()

@x_d("temple", "made", "tcolumn", "lotus_basin")
def _(cx, cy, a, rng):
    """a stone lotus basin, knee high: the moulded round plinth, the bowl carved as two rows of lotus petals (the outer
    row broad with their ribs, the inner row between them), the rim, and the deep basin inside with a little water
    left at its bottom"""
    u = rng.uniform; R = u(2.0, 2.15); n = rng.randint(8, 10); t0 = u(0, 1); o = Obj("tcolumn")
    pl = o.up(ngon(cx, cy, R * 1.05, 16, t0), u(0.3, 0.36)); o.groove(ngon(cx, cy, R * 1.05 - 0.2, 16, t0), pl, 0.1, 0.08)
    pet1 = U([ell(*pol(cx, cy, R * 0.62, t0 + 6.2832 * k / n), R * 0.4, R * 0.28, t0 + 6.2832 * k / n, 12) for k in range(n)] + [disc(cx, cy, R * 0.66, 24)])
    b1 = o.up(pet1, u(1.35, 1.5))
    o.cut(U([barl(pol(cx, cy, R * 0.68, t0 + 6.2832 * k / n), pol(cx, cy, R * 0.98, t0 + 6.2832 * k / n), 0.12) for k in range(n)]), b1, 0.1)
    pet2 = U([ell(*pol(cx, cy, R * 0.55, t0 + 6.2832 * (k + 0.5) / n), R * 0.3, R * 0.22, t0 + 6.2832 * (k + 0.5) / n, 12) for k in range(n)] + [disc(cx, cy, R * 0.56, 24)])
    b2 = o.on(pet2, b1, 0.5)
    o.cut(U([barl(pol(cx, cy, R * 0.6, t0 + 6.2832 * (k + 0.5) / n), pol(cx, cy, R * 0.8, t0 + 6.2832 * (k + 0.5) / n), 0.12) for k in range(n)]), b2, 0.1)
    rim = o.on(disc(cx, cy, R * 0.56, 28), b2, 0.25)
    o.cut(disc(cx, cy, R * 0.44, 24), rim, u(1.0, 1.3))
    return o.out()


_OWN = set(globals())
