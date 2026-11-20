"""Style-only details, blight (2026-11-19): the things found only on corrupted ground. Twisted rock thorns (each a
star-shaped section that shrinks, turns and leans as it rises, every step inside the one below, so it prints
without supports), heaped bones and skulls, bubbling ichor, the style's own runes (style_blight.BLIGHT_RUNES: made
up for it, nothing borrowed) cut into stones and rings and cracked across, swollen fleshy rock, rifts torn open.
Real sizes: scale/real_blight.json."""
import math
from sd_reg import d


def bl_runes(f2, k, s, w):
    """rune k of the style's script, s high, strokes w wide, in frame f2"""
    import style_blight
    st = style_blight.BLIGHT_RUNES[k % len(style_blight.BLIGHT_RUNES)]
    return U([capl(f2(p[0] * s * 0.5, p[1] * s * 0.5), f2(q[0] * s * 0.5, q[1] * s * 0.5), w) for p, q in st])

def bl_spike(cx, cy, r, h, lean=0.0, la=0.0, twist=0.0, a0=0.0, pts=4, inner=0.62):
    """one twisted thorn of rock as a solid: shrinks, turns and leans as it rises, ends in a blunt point"""
    def sect(z):
        t = z / h
        if t >= 1: return None
        rr = r * (1 - t) ** 0.9
        if rr < 0.24: return None
        return star(cx + lean * t * math.cos(la), cy + lean * t * math.sin(la), rr, rr * inner, pts, a0 + twist * t)
    return (sect, h)

def bl_skull(x, y, r, a, lift=0.0):
    """a skull lying face up: the dome of the cranium, the brow, two eye sockets and the nose hole as cuts"""
    ff = frame(x, y, a); h = r * 1.05
    sol = dome(x, y, r, h, lift, sq=0.82, a=a) + dome(*ff(r * 0.55, 0), r * 0.62, h * 0.7, lift, sq=0.8, a=a)
    cuts = [(disc(*ff(r * 0.55, sv * r * 0.36), r * 0.24, 8), lift + h * 0.45) for sv in (-1, 1)] + [(tri(ff(r * 0.95, 0), ff(r * 0.72, 0), r * 0.26), lift + h * 0.3)]
    return sol, cuts

def bl_bone(p, q, r=0.26):
    """a long bone: a shaft swelling to a knuckle at each end"""
    return rod([p, q], lambda t: r * (1.55 if t < 0.08 or t > 0.92 else 1.0), 1.1)


# ------------------------------------------------------------------------------------------------ thorns
@d("blight", "debris", "thorn", "twisted_spike")
def _(cx, cy, a, rng):
    """a great thorn of rock twisting up out of the ground, two or three young ones at its foot"""
    u = rng.uniform; r = u(1.0, 1.3); sol = [bl_spike(cx, cy, r, u(2.7, 3.2), r * 0.5, u(0, 6.28), u(1.0, 1.8) * rng.choice((-1, 1)), u(0, 3), rng.choice((4, 5)))]; foot = [disc(cx, cy, r)]
    for i in range(rng.randint(2, 3)):
        t = a + i * 2.2 + u(-0.4, 0.4); x, y = pol(cx, cy, r + u(0.8, 1.2), t); rr = u(0.5, 0.7)
        sol.append(bl_spike(x, y, rr, u(1.1, 1.7), rr * 0.5, t, u(1.0, 2.0), u(0, 3), 3)); foot.append(disc(x, y, rr))
    return hull(foot).buffer(0.2), [ST(sol, "thorn")]

@d("blight", "debris", "thorn", "thorn_ring")
def _(cx, cy, a, rng):
    """thorns grown up in a ring round a bare spot, each leaning in over it"""
    u = rng.uniform; R = u(1.9, 2.4); n = rng.randint(5, 7); sol = []; foot = []
    for i in range(n):
        t = a + 6.2832 * i / n + u(-0.2, 0.2); x, y = pol(cx, cy, R, t); rr = u(0.55, 0.8)
        sol.append(bl_spike(x, y, rr, u(1.3, 2.3), rr * 0.55, t + math.pi, u(0.6, 1.4), u(0, 3), rng.choice((3, 4)))); foot.append(disc(x, y, rr))
    return hull(foot).buffer(0.2), [ST(sol, "thorn")]

@d("blight", "debris", "thorn", "leaning_thorns")
def _(cx, cy, a, rng):
    """a row of thorns all bent one way, as if something passed over them"""
    u = rng.uniform; ff = frame(cx, cy, a); n = rng.randint(3, 5); sol = []; foot = []; lean = a + math.pi / 2 + u(-0.3, 0.3)
    for i in range(n):
        x, y = ff((i - (n - 1) / 2) * 1.5, u(-0.3, 0.3)); rr = u(0.55, 0.8)
        sol.append(bl_spike(x, y, rr, u(1.4, 2.4), rr * 0.6, lean, u(0.4, 1.0), u(0, 3), rng.choice((3, 4)))); foot.append(disc(x, y, rr))
    return hull(foot).buffer(0.2), [ST(sol, "thorn")]

@d("blight", "debris", "thorn", "thorn_shards")
def _(cx, cy, a, rng):
    """a thorn snapped off: its stump still standing, the broken pieces lying beside it"""
    u = rng.uniform; r = u(0.8, 1.0); sol = [bl_spike(cx, cy, r, u(1.0, 1.3), 0.0, 0.0, u(0.6, 1.2), u(0, 3), 4, 0.66)]; foot = [disc(cx, cy, r)]
    for i in range(rng.randint(2, 3)):
        t = a + i * 2.0 + u(-0.3, 0.3); x, y = pol(cx, cy, r + u(1.3, 1.9), t); L = u(1.8, 2.6); b = t + u(-1, 1)
        sol += spin(x, y, b, L, lambda q, L=L: 0.5 * (0.5 - q / L) + 0.12, 1.1, prof="ridge"); foot.append(rect(x, y, L, 1.0, b))
    return hull(foot).buffer(0.2), [ST(sol, "thorn")]

@d("blight", "made", "thorn", "impaled_skull")
def _(cx, cy, a, rng):
    """a thorn that grew through a skull: the skull pushed down its shaft to the ground, bones round its foot"""
    u = rng.uniform; ff = frame(cx, cy, a); r = u(0.9, 1.1); sol = [bl_spike(cx, cy, r, u(2.8, 3.2), r * 0.4, u(0, 6.28), u(0.6, 1.2), u(0, 3), 4)]
    sk, cuts = bl_skull(*ff(r + 0.55, 0), 0.75, a); bones = []
    for i in range(rng.randint(2, 3)):
        t = a + u(1.2, 5.0); p = pol(cx, cy, r + 0.4, t); q = pol(cx, cy, r + u(2.0, 2.8), t + u(-0.3, 0.3)); bones += bl_bone(p, q, 0.22)
    return disc(cx, cy, r + 2.6), [ST(sol, "thorn"), ST(sk, "bone", cuts=cuts), ST(bones, "bone")]


# ------------------------------------------------------------------------------------------------ bones
@d("blight", "debris", "bone", "bone_pile")
def _(cx, cy, a, rng):
    """bones heaped where something died: a low heap of small ones, long bones across it, a skull on top"""
    u = rng.uniform; R = u(1.5, 1.9)
    sol = turn(cx, cy, [(R, 0.0), (R * 0.8, 0.3), (R * 0.45, 0.55), (0.3, 0.62)], 14, u(0.7, 0.9), a); foot = [disc(cx, cy, R)]
    for i in range(rng.randint(3, 5)):
        t = u(0, 3.14); L = u(2.6, 3.4); ff = frame(cx + u(-0.6, 0.6), cy + u(-0.6, 0.6), t)
        sol += bl_bone(ff(-L / 2, 0), ff(L / 2, 0), u(0.22, 0.28)); foot.append(disc(*ff(L / 2, 0), 0.4)); foot.append(disc(*ff(-L / 2, 0), 0.4))
    sk, cuts = bl_skull(cx + u(-0.4, 0.4), cy + u(-0.4, 0.4), 0.7, a + u(-1, 1), lift=0.35)
    return hull(foot).buffer(0.2), [ST(sol + sk, "bone", cuts=cuts)]

@d("blight", "debris", "bone", "skull_heap")
def _(cx, cy, a, rng):
    """three to five skulls heaped together, staring up"""
    u = rng.uniform; sol = []; cuts = []; foot = []
    for i in range(rng.randint(3, 5)):
        x, y = (cx, cy) if i == 0 else pol(cx, cy, u(1.25, 1.5), a + i * 1.3 + u(-0.2, 0.2)); r = u(0.65, 0.8) if i else 0.8
        if any(Point(x, y).distance(g.centroid) < r * 1.55 for g in foot): continue
        s_, c_ = bl_skull(x, y, r, u(0, 6.28), lift=0.0); sol += s_; cuts += c_; foot.append(disc(x, y, r * 1.3))
    return hull(foot).buffer(0.2), [ST(sol, "bone", cuts=cuts)]

@d("blight", "debris", "bone", "ribcage")
def _(cx, cy, a, rng):
    """the ribcage of some beast lying on its side: the spine, the ribs curving off it, one or two broken"""
    u = rng.uniform; ff = frame(cx, cy, a); L = u(4.2,5.0); n = rng.randint(5, 6); sol = rod([ff(-L / 2, 0), ff(L / 2, 0)], 0.3, 1.1); foot = [rect(cx, cy, L, 0.8, a)]
    for i in range(n):
        x = -L / 2 + 0.5 + i * (L - 1.0) / (n - 1); ln = u(1.8, 2.4) * (1 - 0.25 * abs(i - n / 2) / n) * (0.55 if rng.random() < 0.2 else 1.0)
        pts = [ff(x, 0.2), ff(x + 0.3, ln * 0.5), ff(x + 0.1, ln)]; sol += rod(pts, lambda t: 0.22 - 0.06 * t, 1.1); foot.append(disc(*pts[-1], 0.3))
    return hull(foot).buffer(0.2), [ST(sol, "bone")]

@d("blight", "debris", "bone", "horned_skull")
def _(cx, cy, a, rng):
    """the skull of a horned beast, its horns sweeping back and curling over the ground"""
    u = rng.uniform; ff = frame(cx, cy, a); sk, cuts = bl_skull(cx, cy, 1.0, a)
    for sv in (-1, 1):
        pts = [ff(-0.3, sv * 0.6), ff(-1.1, sv * 1.4), ff(-2.2, sv * u(1.4, 1.8)), ff(-2.7, sv * u(0.6, 1.0))]
        sk += rod(pts, lambda t: 0.42 - 0.26 * t, 1.0)
    return hull([disc(cx, cy, 1.2), disc(*ff(-2.5, 1.5), 0.6), disc(*ff(-2.5, -1.5), 0.6)]).buffer(0.2), [ST(sk, "bone", cuts=cuts)]


# ------------------------------------------------------------------------------------------------ ichor
@d("blight", "marks", "ichor", "bubbling_pool", ground=True)
def _(cx, cy, a, rng):
    """a small pool of ichor on the boil: bubbles standing in it just under the rim, blisters round its edge"""
    u = rng.uniform; R = u(1.7, 2.2); pool = blob(cx, cy, R, rng, 4); lay = [("gcut", pool, u(0.55, 0.7), "ichor")]; bub = []
    x0, y0, x1, y1 = pool.bounds
    for _i in range(80):
        if len(bub) >= rng.randint(2, 4): break
        b = disc(u(x0, x1), u(y0, y1), u(0.5, 0.72), 14)
        if inset(pool, 0.3).contains(b) and all(b.distance(o) > 0.35 for o in bub): bub.append(b)
    lay += [("pad", b, "blister") for b in bub]; foot = [pool]
    for i in range(rng.randint(3, 5)):
        t = a + i * 1.4 + u(-0.3, 0.3); q = disc(*pol(cx, cy, R + u(0.5, 0.8), t), u(0.45, 0.6), 12)
        if q.distance(pool) > 0.2 and all(q.distance(o) > 0.2 for o in foot[1:]): foot.append(q); lay.append(("mound", q, u(0.4, 0.6), "blister", SOFT_T))
    return hull(foot).buffer(0.2), lay

@d("blight", "marks", "ichor", "ichor_seep", ground=True)
def _(cx, cy, a, rng):
    """ichor welling out of a crack and running down into a little pool at its end"""
    u = rng.uniform; ff = frame(cx, cy, a); L = u(5, 7)
    cr = wavy(ff(-L / 2, 0), ff(L / 2 - 1.0, u(-0.5, 0.5)), rng, 0.5, 5, 0.45); pool = blob(*ff(L / 2, 0), u(1.0, 1.3), rng, 3)
    return hull([cr, pool]).buffer(0.2), [("gcut", U([cr, pool]), u(0.5, 0.6), "ichor"), ("pad", disc(*ff(L / 2, 0), 0.5, 12), "blister")]

@d("blight", "growth", "blister", "blisters")
def _(cx, cy, a, rng):
    """the ground come up in blisters: round swellings, some burst open at the top"""
    u = rng.uniform; ps = []; cuts = []; foot = []
    for i in range(rng.randint(4, 7)):
        x, y = (cx, cy) if i == 0 else pol(cx, cy, u(0.9, 1.9), a + i * 1.1); r = u(0.45, 0.8)
        if any(disc(x, y, r).distance(g) < 0.1 for g in foot): continue
        h = r * u(0.8, 1.1); ps += dome(x, y, r, h, n=14); foot.append(disc(x, y, r))
        if r > 0.6 and rng.random() < 0.5: cuts.append((disc(x, y, r * 0.42, 10), h * 0.5))
    return hull(foot).buffer(0.2), [ST(ps, "blister", cuts=cuts)]


# ------------------------------------------------------------------------------------------------ runes
@d("blight", "made", "runestone", "rune_stone")
def _(cx, cy, a, rng):
    """a squat block of dark stone with a rune cut deep into its top, split by a crack, its corners knocked off"""
    u = rng.uniform; ff = frame(cx, cy, a); L = u(2.4, 2.9); W = u(2.0, 2.4); o = Obj("runestone")
    g = Polygon([ff(-L / 2 + u(0, 0.4), -W / 2), ff(L / 2, -W / 2 + u(0, 0.4)), ff(L / 2 - u(0, 0.4), W / 2), ff(-L / 2, W / 2 - u(0, 0.4))])
    b = o.up(g, u(1.1, 1.5)); o.cut(bl_runes(frame(cx, cy, a + u(-0.3, 0.3)), rng.randrange(8), min(L, W) * 0.62, 0.3), b, 0.3)
    o.cut(wavy(ff(-L / 2 - 0.3, u(-0.8, 0.8)), ff(L / 2 + 0.3, u(-0.8, 0.8)), rng, 0.3, 4, 0.22), b, 0.4)
    return o.out()

@d("blight", "marks", "runecut", "cracked_rune_ring", ground=True)
def _(cx, cy, a, rng):
    """a small ring of runes cut in the ground, a crack through it where the ground has heaved"""
    u = rng.uniform; R = u(2.2, 2.7); n = rng.randint(5, 6)
    g = [ann(cx, cy, R, R - 0.34, 36)] + [bl_runes(frame(*pol(cx, cy, R * 0.6, a + 6.2832 * i / n), a + 6.2832 * i / n + math.pi / 2), rng.randrange(8), 1.0, 0.28) for i in range(n)]
    t = u(0, 6.28); cr = wavy(pol(cx, cy, R + 0.6, t), pol(cx, cy, R + 0.6, t + math.pi + u(-0.4, 0.4)), rng, 0.5, 5, 0.42)
    return disc(cx, cy, R + 0.7), [("gcut", U(g).difference(cr.buffer(0.3)), 0.35, "runecut"), ("gcut", cr, 0.5, "rift")]

@d("blight", "made", "runestone", "rune_slab")
def _(cx, cy, a, rng):
    """a flat slab of runes broken in two, the halves pushed apart and tilted up at the break"""
    u = rng.uniform; ff = frame(cx, cy, a); L = u(4.2, 5.0); W = u(2.4, 2.8); o = Obj("runestone"); gap = u(0.35, 0.5); br = u(-0.4, 0.4)
    for sx in (-1, 1):
        g = Polygon([ff(sx * L / 2, -W / 2), ff(br + sx * gap / 2 + u(-0.2, 0.2), -W / 2), ff(br + sx * gap / 2 + u(-0.3, 0.3), W / 2), ff(sx * L / 2, W / 2)]).buffer(0)
        b = o.up(g, u(0.5, 0.65)); o.on(inset(g, 0.9).intersection(rect(*ff(br + sx * (gap / 2 + 0.8), 0), 1.6, W, a)), b, u(0.2, 0.3))
        f2 = frame(*ff(sx * L * 0.27, 0), a); o.cut(U([bl_runes(frame(*f2(0, v), a + math.pi / 2), rng.randrange(8), 0.9, 0.26) for v in (-W * 0.24, W * 0.24)]).intersection(inset(g, 0.3)), b, 0.25)
    return o.out()

@d("blight", "made", "runestone", "blood_stone")
def _(cx, cy, a, rng):
    """an offering stone: a low block with channels cut in its top, running from a cup in the middle off its sides"""
    u = rng.uniform; R = u(1.6, 1.9); o = Obj("runestone"); g = ngon(cx, cy, R, rng.choice((5, 6, 7)), a)
    b = o.up(g, u(0.9, 1.2)); n = rng.randint(3, 4)
    o.cut(U([disc(cx, cy, 0.55, 14)] + [barl((cx, cy), pol(cx, cy, R + 0.3, a + 6.2832 * i / n + 0.3), 0.3) for i in range(n)]), b, 0.35)
    return o.out()


# ------------------------------------------------------------------------------------------------ rifts
@d("blight", "marks", "rift", "rift_tear", ground=True)
def _(cx, cy, a, rng):
    """a rift torn in the ground: a jagged slot widest in the middle, broken lumps of the ground heaved up on its lips"""
    u = rng.uniform; ff = frame(cx, cy, a); L = u(6.5, 8.0); W = u(1.0, 1.4); m = 12
    us = [-L / 2 + L * i / m for i in range(m + 1)]; wf = lambda q: W * max(0.0, 1 - (2 * q / L) ** 2) ** 0.7
    slot = Polygon([ff(q, wf(q) * u(0.6, 1.0)) for q in us] + [ff(q, -wf(q) * u(0.6, 1.0)) for q in reversed(us)]).buffer(0)
    lay = [("gcut", slot, u(0.8, 0.95), "rift")]; foot = [slot]
    for i in range(rng.randint(4, 6)):
        q = u(-L * 0.38, L * 0.38); sv = rng.choice((-1, 1)); p = stone(*ff(q, sv * (wf(q) + u(0.8, 1.1))), u(0.55, 0.8), rng, 0.7, rng.randint(4, 5))
        if p.distance(slot) > 0.22 and all(p.distance(o) > 0.2 for o in foot[1:]): foot.append(p); lay.append(("mound", p, u(0.5, 0.9), "blightrock", ROCK_T))
    return hull(foot).buffer(0.2), lay

@d("blight", "marks", "rift", "rift_mouth", ground=True)
def _(cx, cy, a, rng):
    """where the ground burst open: a dark mouth with cracks running out of it like spokes, chips thrown round it"""
    u = rng.uniform; R = u(1.0, 1.3); mouth = stone(cx, cy, R, rng, 0.85, 7); n = rng.randint(4, 6)
    sp = U([wavy(pol(cx, cy, R * 0.6, a + 6.2832 * i / n + u(-0.3, 0.3)), pol(cx, cy, R + u(1.6, 2.6), a + 6.2832 * i / n + u(-0.3, 0.3)), rng, 0.3, 3, 0.38) for i in range(n)])
    lay = [("gcut", mouth, u(0.85, 1.0), "rift"), ("gcut", sp.difference(mouth), 0.5, "rift")]; foot = [mouth, sp]
    for i in range(rng.randint(2, 4)):
        p = stone(*pol(cx, cy, R + u(1.0, 2.0), u(0, 6.28)), u(0.4, 0.55), rng, 0.7, 4)
        if p.distance(sp) > 0.2 and all(p.distance(o) > 0.2 for o in foot[2:]): foot.append(p); lay.append(("mound", p, u(0.4, 0.55), "blightrock", PEB_T))
    return hull(foot).buffer(0.2), lay


# ------------------------------------------------------------------------------------------------ swollen rock
@d("blight", "growth", "fleshrock", "flesh_lump")
def _(cx, cy, a, rng):
    """rock gone soft: three or four swollen lumps grown into one, folds between them"""
    u = rng.uniform; ps = []; foot = []
    for i in range(rng.randint(3, 4)):
        x, y = (cx, cy) if i == 0 else pol(cx, cy, u(1.0, 1.5), a + i * 2.0 + u(-0.4, 0.4)); r = u(1.0, 1.3) * (1 if i == 0 else 0.8)
        ps += dome(x, y, r, u(1.0, 1.5) * (1 if i == 0 else 0.75), sq=u(0.75, 0.95), a=u(0, 3)); foot.append(disc(x, y, r))
    return hull(foot).buffer(0.2), [ST(ps, "fleshrock")]

@d("blight", "growth", "fleshrock", "veined_rock")
def _(cx, cy, a, rng):
    """a swollen rock with dark veins standing out on it and running off into the ground"""
    u = rng.uniform; r = u(1.4, 1.8); h = u(1.0, 1.3); ps = dome(cx, cy, r, h, sq=u(0.75, 0.9), a=a); vs = []; foot = [disc(cx, cy, r)]
    for i in range(rng.randint(3, 4)):
        t = a + i * 1.6 + u(-0.3, 0.3); pts = [pol(cx, cy, r * 0.3, t), pol(cx, cy, r * 0.9, t + u(-0.3, 0.3)), pol(cx, cy, r + u(1.2, 2.0), t + u(-0.4, 0.4))]
        vs += rod(pts, lambda q: 0.26 - 0.08 * q, 1.0, lift=0.0); foot.append(disc(*pts[-1], 0.3))
    return hull(foot).buffer(0.2), [ST(ps, "fleshrock"), ST(vs, "tendril")]

@d("blight", "growth", "fleshrock", "eye_stone")
def _(cx, cy, a, rng):
    """a swollen lump of rock with one great eye open in it: the lids round it, the eye a dome, its pupil a slit"""
    u = rng.uniform; ff = frame(cx, cy, a); r = u(1.6, 1.9); h = u(1.1, 1.4)
    lump = dome(cx, cy, r, h, sq=0.8, a=a); e = ff(r * 0.15, 0); lids = ell(*e, 1.0, 0.62, a, 18)
    eye = dome(e[0], e[1], 0.58, 0.45, lift=h * 0.62, sq=0.8, a=a)
    return disc(cx, cy, r), [ST(lump, "fleshrock", cuts=[(lids, h * 0.62)]), ST(eye, "blister", cuts=[(rect(*e, 0.9, 0.22, a + math.pi / 2), h * 0.62 + 0.25)])]

@d("blight", "growth", "tendril", "tendril_hole", ground=True)
def _(cx, cy, a, rng):
    """tendrils reaching out of a hole in the ground and lying along it, thinning to their tips"""
    u = rng.uniform; R = u(0.8, 1.0); hole = stone(cx, cy, R, rng, 0.85, 8); vs = []; foot = [hole]
    for i in range(rng.randint(3, 4)):
        t = a + i * 6.2832 / 3.5 + u(-0.3, 0.3); L = u(2.6, 3.6); b = t + u(-0.7, 0.7)
        pts = [pol(cx, cy, R * 0.9, t), pol(*pol(cx, cy, R * 0.9, t), L * 0.5, t + (b - t) * 0.5), pol(*pol(cx, cy, R * 0.9, t), L, b)]
        vs += rod(pts, lambda q: 0.42 - 0.22 * q, 1.0); foot.append(disc(*pts[-1], 0.3))
    return hull(foot).buffer(0.2), [("gcut", hole, u(0.8, 0.95), "rift"), ST(vs, "tendril")]


# ------------------------------------------------------------------------------------------------ left by those who came
@d("blight", "made", "wax", "ritual_candles")
def _(cx, cy, a, rng):
    """candle stubs burnt down in a ring round a skull, wax run down their sides and pooled at their feet"""
    u = rng.uniform; n = rng.randint(4, 6); R = u(1.6, 2.0); sol = []; foot = []
    for i in range(n):
        x, y = pol(cx, cy, R, a + 6.2832 * i / n); r = u(0.32, 0.42)
        sol += cyl(x, y, r + 0.18, 0.18, n=12) + cyl(x, y, r, u(0.7, 1.4), n=12); foot.append(disc(x, y, r + 0.18))
    sk, cuts = bl_skull(cx, cy, 0.78, a + u(-1, 1))
    return hull(foot).buffer(0.2), [ST(sol, "wax"), ST(sk, "bone", cuts=cuts)]

@d("blight", "made", "iron", "offering_bowl")
def _(cx, cy, a, rng):
    """a squat iron bowl on three stubby feet, dark ichor standing in it"""
    u = rng.uniform; r = u(1.1, 1.3); h = u(0.9, 1.1)
    sol = turn(cx, cy, [(r * 0.6, 0.0), (r * 0.7, h * 0.2)] + crv(r * 0.7, r, h * 0.2, h * 0.75) + [(r, h)], 20) + [s for i in range(3) for s in cyl(*pol(cx, cy, r * 0.62, a + i * 2.094), 0.3, 0.25, n=8)]
    return disc(cx, cy, r + 0.1), [ST(sol, "iron", cuts=[(disc(cx, cy, r - 0.28, 20), h * 0.55)])]

@d("blight", "made", "iron", "ritual_knife")
def _(cx, cy, a, rng):
    """a wavy-bladed knife left lying on the ground, its hilt wound with cord, a crooked guard"""
    u = rng.uniform; ff = frame(cx, cy, a); Lb = u(2.6, 3.2); o = Obj("iron")
    pts = [ff(-0.2, 0.55)] + [ff(x, 0.5 * (1 - x / Lb) + 0.12 + 0.1 * math.sin(x * 4)) for x in np.linspace(0, Lb, 7)] + [ff(x, -0.5 * (1 - x / Lb) - 0.12 + 0.1 * math.sin(x * 4)) for x in np.linspace(Lb, 0, 7)] + [ff(-0.2, -0.55)]
    blade = Polygon(pts).buffer(0); o.st(ext(blade, 0.32), foot=blade)
    gd = rect(*ff(-0.4, 0), 0.5, 1.7, a + u(-0.2, 0.2)); o.st(ext(gd, 0.45), foot=gd)
    hg = rect(*ff(-1.2, 0), 1.2, 0.62, a); hd = o.st(ext(hg, 0.48), "leather", foot=hg)
    o.cut(U([barl(ff(x, -0.45), ff(x + 0.12, 0.45), 0.12) for x in np.arange(-1.7, -0.7, 0.25)]), hd, 0.12)
    pm = disc(*ff(-1.95, 0), 0.42, 10); o.st(ext(pm, 0.52), foot=pm)
    return o.out()
