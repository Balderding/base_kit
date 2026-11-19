"""Third drawing, cynder (see sd_r_x_frontier.py): the forge's quenching trough at its real size beside a 32 mm
figure, waist high to a dwarf and knee high to a man, its water standing a hand below the rim."""
import math
from sd_reg import d
from sd_r_x_frontier import x_d


@x_d("cynder", "made", "basalt", "quench_trough")
def _(cx, cy, a, rng):
    """a quenching trough cut from one block of stone, knee high: thick walls with the mason's tool marks across the
    rim and a drafted band round its foot, a lifting boss on each end, the drain plug in one end wall, dark water
    standing a hand below the rim with a skin of scale on it; a blade blank with its tang lies beside, and the tongs"""
    u = rng.uniform; ff = frame(cx, cy, a); L = u(4.6, 5.0); W = u(2.4, 2.7); h = u(2.5, 2.8); o = Obj("basalt"); g = rect(cx, cy, L, W, a)
    for sx in (-1, 1):
        bs = o.up(rect(*ff(sx * (L / 2 + 0.2), 0), 0.6, 1.1, a), h * 0.62); o.on(rect(*ff(sx * (L / 2 + 0.2), 0), 0.45, 0.8, a), bs, 0.12)
    pl = o.up(rect(cx, cy, L + 0.2, W + 0.2, a), 0.35)
    b = o.up(g, h); inner = rrect(cx, cy, L - 1.0, W - 1.0, a, 0.25); o.cut(inner, b, u(0.4, 0.55))
    ticks = [barl(ff(x, sy * (W / 2 - 0.5)), ff(x + 0.12, sy * (W / 2 + 0.1)), 0.24) for x in np.arange(-L / 2 + 0.6, L / 2 - 0.4, 0.55) for sy in (-1, 1)] + [barl(ff(sx * (L / 2 - 0.5), v), ff(sx * (L / 2 + 0.1), v + 0.12), 0.24) for v in np.arange(-W / 2 + 0.6, W / 2 - 0.4, 0.55) for sx in (-1, 1)]
    o.cut(U(ticks + [Polygon([ff(sx * L / 2, sy * W / 2), ff(sx * (L / 2 - u(0.4, 0.7)), sy * W / 2), ff(sx * L / 2, sy * (W / 2 - u(0.4, 0.7)))]) for sx in (-1, 1) for sy in (-1, 1) if rng.random() < 0.4]).difference(inner.buffer(0.08)), b, 0.2)
    sx = rng.choice((-1, 1)); o.st(ext(disc(*ff(sx * (L / 2 + 0.25), u(-0.5, 0.5) + 0.0), 0.3, 10), 0.75), "iron", foot=disc(*ff(sx * (L / 2 + 0.25), 0), 0.3))
    sc = [blob(*ff(u(-L * 0.3, L * 0.3), u(-W * 0.2, W * 0.2)), u(0.3, 0.5), rng, 3) for _ in range(rng.randint(1, 3))]
    o.st(ext(U(sc).intersection(inset(inner, 0.1)), h - 0.45 + 0.04), "ash", foot=inner)
    sv = rng.choice((-1, 1)); c = ff(u(-0.5, 0.5), sv * (W / 2 + u(1.0, 1.3))); t = a + u(-0.2, 0.2); f2 = frame(c[0], c[1], t); Lb = u(3.4, 3.9)
    bl = Polygon([f2(-Lb / 2, -0.38), f2(Lb * 0.3, -0.38), f2(Lb / 2, 0), f2(Lb * 0.3, 0.38), f2(-Lb / 2, 0.38)])
    o.st(ext(bl, 0.32) + ext(rect(*f2(-Lb / 2 - 0.45, 0), 0.95, 0.5, t), 0.28), "iron", cuts=[(barl(f2(-Lb * 0.4, 0), f2(Lb * 0.2, 0), 0.24), 0.16)], foot=rect(c[0], c[1], Lb + 1.0, 0.8, t))
    if rng.random() < 0.6:
        c = ff(u(-0.6, 0.6), -sv * (W / 2 + u(1.0, 1.2))); t = a + u(-0.3, 0.3) + (math.pi if rng.random() < 0.5 else 0); f2 = frame(c[0], c[1], t); ps = []
        for s2 in (-1, 1): ps += ext(line([f2(-1.9, s2 * 0.42), f2(0.4, -s2 * 0.05), f2(0.9, -s2 * 0.62), f2(1.4, -s2 * 0.7), f2(1.7, -s2 * 0.45)], 0.5), 0.36)
        o.st(ps, "iron", foot=rect(c[0], c[1], 3.8, 1.8, t)); o.st(ext(disc(*f2(0.4, 0), 0.42, 8), 0.56), "iron", foot=disc(*f2(0.4, 0), 0.4))
    return o.out()


_OWN = set(globals())
