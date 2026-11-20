"""Style-only details, sundered: a gothic city shelled to rubble. Dressed stone fallen out of cathedral walls and
arcades (fluted column drums, capitals, voussoirs, pointed window frames, tracery), the statues that stood in the
squares, the bent rebar of the newer floors, and what the fighting left (shell casings, a dud shell, sandbags,
barricade beams). Drawn at about half their real size (1 m = 3 mm laid out, see scale/real_sundered.json); everything
lies or stands solid on the ground, so nothing overhangs."""
import math
from sd_reg import d
from sd_r_x_frontier import x_d

ST_ = "ashlar"          # dressed stone
SU_ = "statue"          # statue stone, paler


# ---------------------------------------------------------------------------------------------- shared drawing
def su_lancet(ff, W, S, base=0.0):
    """the outline of a pointed (equilateral) arch, W wide, its sides S tall from base up to the springing line,
    in the frame ff (u = up the arch, v = across)"""
    pts = [ff(base, -W / 2), ff(base, W / 2)]
    pts += [ff(base + S + W * math.sin(t), -W / 2 + W * math.cos(t)) for t in np.linspace(0, math.pi / 3, 9)]
    pts += [ff(base + S + W * math.sin(t), W / 2 - W * math.cos(t)) for t in np.linspace(math.pi / 3, 0, 9)[1:]]
    return Polygon(pts).buffer(0)

def su_jag(ff, u, W, rng, amp=0.3, n=6):
    """a ragged break across a piece at u (keep the side below u)"""
    return Polygon([ff(-50, -W), ff(-50, W)] + [ff(u + rng.uniform(-amp, amp), v) for v in np.linspace(W, -W, n)])

def su_chip(g, rng, r=(0.3, 0.6)):
    """a corner knocked off a dressed block"""
    c = list(g.exterior.coords)[:-1]; k = c[rng.randrange(len(c))]
    q = g.difference(stone(k[0], k[1], rng.uniform(*r), rng, 0.8, 5))
    return max(polys_of(q), key=lambda p: p.area, default=g)

def su_drum(cx, cy, a, L, r, nfl=12, d=0.26, k=0.55, ends=None):
    """a fluted column drum lying on its side: upright sides up to k radii, round over the top, the flutes running
    along it as shallow grooves (seen from above: the flutes on its upper half). ends: {-1: [...], 1: [...]} five
    extra lengths across each end, so a broken end is ragged, not square"""
    ff = frame(cx, cy, a); c = k * r
    th = [2 * math.pi * (j + 0.5) / nfl - math.pi for j in range(nfl)]
    vs = [r * math.sin(t) for t in th if abs(t) < 1.25]
    def hz(v): return c + math.sqrt(max(0.0, r * r - v * v))
    def sect(z):
        if z <= c: w = r
        elif z < c + r: w = math.sqrt(max(0.0, r * r - (z - c) ** 2))
        else: return None
        if w < 0.2: return None
        g = rect(cx, cy, L, 2 * w, a)
        if ends: g = U([g] + [Polygon([ff(sx * L / 2, -w), ff(sx * L / 2, w)] + [ff(sx * (L / 2 + e), v) for v, e in zip(np.linspace(w, -w, 5), ends[sx])]).buffer(0) for sx in (-1, 1) if max(ends[sx]) > 0.05])
        gr = [barl(ff(-L, v), ff(L, v), 0.3) for v in vs if z > hz(v) - d and abs(v) < w - 0.2]
        return g.difference(U(gr)) if gr else g
    return [(sect, c + r)]

def su_bars(o, pts_list, r=0.27, kind="rust"):
    """bent reinforcing bars lying on the ground: each a bent line of round bar"""
    ps = []
    for pts in pts_list: ps += rod(pts, r, 1.0)
    if ps: o.st(ps, kind)

def su_bent(p0, ang, L, rng, n=4, bend=0.7):
    """the points of a bar bent as it was torn out: it starts at p0 heading ang and curls one way"""
    pts = [p0]; t = ang; sv = rng.choice((-1, 1)); x, y = p0
    for i in range(n):
        t += sv * bend * rng.uniform(0.3, 1.0) * (i > 0); x += L / n * math.cos(t); y += L / n * math.sin(t); pts.append((x, y))
    return pts


# ---------------------------------------------------------------------------------------------- columns
@x_d("sundered", "made", ST_, "fluted_drum")
def _(cx, cy, a, rng):
    """a column drum fallen out of a nave arcade, lying on its side: the flutes running its length, the bed joint
    face at one end square and the other broken ragged where it split; chips knocked out of the flutes lie by it"""
    u = rng.uniform; L = u(3.2, 4.0); r = u(1.25, 1.45); ff = frame(cx, cy, a)
    o = Obj(ST_)
    o.st(su_drum(cx, cy, a, L, r, rng.choice((12, 16)), ends={-1: [0.0] * 5, 1: [u(0.0, 0.55) for _ in range(5)]}), foot=rect(cx, cy, L + 0.5, 2 * r, a))
    for i in range(rng.randint(1, 3)):
        p = ff(u(-L / 2, L / 2), rng.choice((-1, 1)) * (r + u(0.4, 0.9))); o.up(stone(p[0], p[1], u(0.42, 0.6), rng, 0.7, 5), u(0.35, 0.55), "rubble")
    return o.out()

@x_d("sundered", "made", ST_, "drum_row")
def _(cx, cy, a, rng):
    """a whole column come down in a line: its drums lie end to end where they fell, each knocked a little askew,
    gaps between them, the last one broken off short"""
    u = rng.uniform; n = rng.randint(2, 3); r = u(1.05, 1.2); o = Obj(ST_); ff = frame(cx, cy, a); x = -(n * 2.9) / 2; nfl = rng.choice((12, 16))
    for i in range(n):
        Li = u(2.3, 2.8) if i < n - 1 else u(1.4, 2.0); c = ff(x + Li / 2, u(-0.35, 0.35)); t = a + u(-0.22, 0.22)
        o.st(su_drum(c[0], c[1], t, Li, r * u(0.95, 1.0), nfl), foot=rect(c[0], c[1], Li, 2 * r, t)); x += Li + u(0.25, 0.6)
    return o.out()

@x_d("sundered", "made", ST_, "column_stump")
def _(cx, cy, a, rng):
    """the stump of a fluted column still on its square plinth: the moulded base ring, the shaft with its flutes
    standing knee high, its top shattered into a few ragged steps; a fallen piece leaning at its foot"""
    u = rng.uniform; P = u(3.2, 3.6); r = u(1.15, 1.3); o = Obj(ST_); n = rng.choice((12, 16))
    pl = su_chip(rect(cx, cy, P, P, a), rng); b = o.up(pl, u(0.45, 0.6))
    t1 = o.on(disc(cx, cy, r + 0.32, 28), b, 0.3)
    sh = star(cx, cy, r, r - 0.17, n, a)
    h = u(1.5, 2.4); s1 = o.on(sh, t1, h * 0.6)
    br = stone(cx + u(-0.3, 0.3), cy + u(-0.3, 0.3), r * u(0.7, 0.85), rng, 0.8, 6).intersection(sh)
    s2 = o.on(br, s1, h * 0.25)
    if rng.random() < 0.6: o.on(stone(cx + u(-0.3, 0.3), cy + u(-0.3, 0.3), r * 0.45, rng, 0.8, 5).intersection(br), s2, h * 0.15)
    p = pol(cx, cy, P * 0.75, a + u(0.4, 1.2)); o.up(stone(p[0], p[1], u(0.6, 0.8), rng, 0.7, 6), u(0.5, 0.7), "rubble")
    return o.out()

@x_d("sundered", "made", ST_, "fallen_capital")
def _(cx, cy, a, rng):
    """a column capital fallen on its side, crown up: the square abacus slab on top, the bell under it carved with
    two rows of curled leaves standing out round it, the neck ring at the bottom of the bell"""
    u = rng.uniform; A = u(3.0, 3.4); o = Obj(ST_); f2 = frame(cx, cy, a)
    ab = su_chip(rect(cx, cy, A, A, a), rng, (0.3, 0.5)); b = o.up(ab, u(0.5, 0.6))
    bell = o.on(disc(cx, cy, A * 0.42, 24), b, 0.45)
    lv = U([ell(*pol(cx, cy, A * 0.36, a + 6.2832 * k / 8 + 0.39), 0.42, 0.3, a + 6.2832 * k / 8 + 0.39, 10) for k in range(8)])
    o.on(lv, b, 0.6)
    o.on(U([disc(*pol(cx, cy, A * 0.2, a + 6.2832 * k / 6), 0.3, 8) for k in range(6)]), bell, 0.35)
    nk = o.on(disc(cx, cy, A * 0.16, 16), bell, 0.4); o.cut(ann(cx, cy, A * 0.14, A * 0.07, 12), nk, 0.12)
    return ab.buffer(0.2), o.L


# ---------------------------------------------------------------------------------------------- arches and windows
@x_d("sundered", "made", ST_, "lancet_window")
def _(cx, cy, a, rng):
    """a pointed window frame lying flat in the rubble where it fell out of the wall: the moulded stone frame, the
    mullion up the middle splitting into two lights, a round eye of tracery in the head; the glass long gone, its
    foot broken off on some"""
    u = rng.uniform; W = u(4.0, 4.6); S = u(3.4, 4.2); t = u(0.95, 1.1); ff = frame(cx, cy, a); fw = frame(*ff(-(S + W * 0.866) / 2, 0), a)
    out_ = su_lancet(fw, W, S); inn = su_lancet(fw, W - 2 * t, S - 0.1, t * 0.7)
    if rng.random() < 0.5: out_ = out_.intersection(su_jag(frame(*ff(-(S + W * 0.866) / 2, 0), a + math.pi), -u(0.6, 1.4), W, rng))
    o = Obj(ST_); fr = out_.difference(inn); b = o.up(fr, u(0.65, 0.8))
    o.groove(inset(out_, 0.35), b, 0.2, 0.12)
    ey = fw(S + W * 0.36, 0); re = W * 0.2
    tr = U([barl(fw(0.1, 0), fw(S + W * 0.36 - re, 0), 0.62), ann(ey[0], ey[1], re + 0.3, re - 0.2, 20)]).intersection(inn.buffer(0.1))
    o.st(ext(tr, u(0.55, 0.65)), foot=tr)
    return out_.buffer(0.3), o.L

@x_d("sundered", "made", ST_, "rose_window_shard")
def _(cx, cy, a, rng):
    """a broken piece of a rose window lying flat: a wedge of the great wheel, its outer ring, two rings of tracery
    inside it and the spokes between, the lights empty and the far side shattered off"""
    u = rng.uniform; R = u(4.2, 4.8); span = u(0.9, 1.3); o = Obj(ST_); ox, oy = pol(cx, cy, -R * 0.55, a)
    wedge = Polygon([(ox, oy)] + arc(ox, oy, R, a - span / 2, a + span / 2, 14)).buffer(0)
    wedge = wedge.difference(disc(ox, oy, R * u(0.25, 0.35), 18)).difference(stone(*pol(ox, oy, R * 0.7, a + span * u(0.2, 0.45)), u(0.9, 1.4), rng, 0.8, 6))
    wedge = max(polys_of(wedge), key=lambda g: g.area)
    fr = U([ann(ox, oy, R, R - 0.7, 48), ann(ox, oy, R * 0.66 + 0.3, R * 0.66 - 0.3, 40)] + [barl((ox, oy), pol(ox, oy, R, a + span * (k / 4 - 0.5)), 0.55) for k in range(5)]).intersection(wedge)
    b = o.up(wedge, 0.35); o.on(fr, b, u(0.35, 0.45))
    o.on(U([ann(*pol(ox, oy, R * 0.82, a + span * ((k + 0.5) / 4 - 0.5)), 0.5, 0.25, 12) for k in range(4)]).intersection(wedge), b, 0.3)
    return wedge.buffer(0.3), o.L

@x_d("sundered", "debris", ST_, "voussoirs")
def _(cx, cy, a, rng):
    """the wedge stones of a fallen arch lying in a broken curve, as the arch came down: each a tapered block with
    its moulded face, a few slid out of line, the keystone among them"""
    u = rng.uniform; R = u(4.0, 5.0); n = rng.randint(4, 6); o = Obj(ST_); ox, oy = pol(cx, cy, -R, a); w = u(1.4, 1.7)
    for i in range(n):
        t = a + (i - (n - 1) / 2) * 0.32 + u(-0.05, 0.05); dr = u(-0.4, 0.5)
        q = Polygon([pol(ox, oy, R + dr - w / 2, t - 0.13), pol(ox, oy, R + dr + w / 2, t - 0.16), pol(ox, oy, R + dr + w / 2, t + 0.16), pol(ox, oy, R + dr - w / 2, t + 0.13)])
        key = i == n // 2
        b = o.up(su_chip(q, rng, (0.2, 0.35)) if rng.random() < 0.5 else q, u(0.75, 0.95) * (1.15 if key else 1.0))
        o.cut(LineString([pol(ox, oy, R + dr - w / 2 + 0.45, t - 0.3), pol(ox, oy, R + dr - w / 2 + 0.45, t + 0.3)]).buffer(0.11), b, 0.12)
    return o.out()

@x_d("sundered", "debris", ST_, "tracery_bits")
def _(cx, cy, a, rng):
    """pieces of window tracery thrown about: a quatrefoil, a trefoil, a broken ogee bar, each a thin carved frame of
    stone with its lights open"""
    u = rng.uniform; o = Obj(ST_); foot = []
    for i in range(rng.randint(2, 3)):
        p = pol(cx, cy, u(0.4, 2.4), a + i * 2.2); r = u(0.95, 1.2); k = rng.choice((3, 4)); t = u(0, 6.28)
        lobes = U([disc(*pol(p[0], p[1], r * 0.5, t + 6.2832 * j / k), r * 0.55, 14) for j in range(k)])
        fr = lobes.buffer(0.4).difference(U([disc(*pol(p[0], p[1], r * 0.5, t + 6.2832 * j / k), r * 0.25, 12) for j in range(k)]))
        if rng.random() < 0.4: fr = fr.difference(barl(pol(p[0], p[1], 2, t), pol(p[0], p[1], -2, t + 0.3), 0.3))
        for g in polys_of(fr):
            if g.area > 0.8: o.st(ext(g, u(0.4, 0.55)), foot=g); foot.append(g)
    if not foot: return None
    return o.out()

@x_d("sundered", "made", ST_, "pinnacle")
def _(cx, cy, a, rng):
    """a pinnacle fallen from a buttress, lying on its side: the square shaft with gabled faces, then the spire
    tapering to its finial, knobbed crockets running up its edges; snapped off at the foot"""
    u = rng.uniform; L = u(5.6, 6.6); W = u(1.5, 1.7); ff = frame(cx, cy, a); o = Obj(ST_)
    sh = o.up(rect(*ff(-L / 2 + 1.0, 0), 2.0, W, a), W * 0.7)
    o.on(Polygon([ff(-L / 2 + 0.2, -W / 2), ff(-L / 2 + 1.8, -W / 2), ff(-L / 2 + 1.0, 0)]).buffer(0.0), sh, 0.15)
    sp = o.st(spin(*ff(0.5, 0), a, L - 2.0, lambda q: (W / 2) * (1 - (q + (L - 2.0) / 2) / (L - 2.0)) * 0.85 + 0.12, 1.0, prof="ridge"))
    cr = U([disc(*ff(-L / 2 + 2.2 + j * 0.75, sv * (W / 2) * (1 - (j * 0.75 + 0.2) / (L - 2.0)) * 0.85), 0.32, 8) for j in range(int((L - 2.6) / 0.75)) for sv in (-1, 1)])
    o.st(ext(cr, 0.55), foot=cr)
    o.st(dome(*ff(L / 2 - 0.1, 0), 0.45, 0.7))
    return rect(*ff(0.2, 0), L + 0.6, W + 0.9, a), o.L


# ---------------------------------------------------------------------------------------------- statues
@x_d("sundered", "made", SU_, "fallen_saint")
def _(cx, cy, a, rng):
    """a robed stone figure toppled off its plinth, lying on its back: the hem of the robe with its folds running up
    the body, the hands clasped on the chest, the hooded head; broken off at the ankles, the feet left behind"""
    u = rng.uniform; L = u(6.6, 7.6); w = u(1.15, 1.3); ff = frame(cx, cy, a); o = Obj(SU_)
    def rf(q):
        s = (q + L / 2) / L
        if s < 0.62: return w * (0.78 + 0.32 * s)
        if s < 0.72: return w * (0.98 - 1.1 * (s - 0.62))
        if s < 0.78: return w * 0.5
        return w * 0.58 * math.sqrt(max(0.05, 1 - ((s - 0.89) / 0.12) ** 2))
    body = spin(cx, cy, a, L, rf, 1.05)
    folds = [(barl(ff(-L / 2 - 0.2, v), ff(L * 0.1, v * 0.7), 0.22), w * 0.95) for v in (-0.6, -0.2, 0.25, 0.62)]
    o.st(body, cuts=folds, foot=rect(cx, cy, L, 2 * w, a))
    o.st(dome(*ff(L * 0.05, 0), w * 0.55, w * 1.25))          # the clasped hands
    o.st(ext(rect(*ff(L * 0.02, 0), 0.55, w * 1.3, a), w * 1.15))          # the forearms across the chest
    o.st(dome(*ff(L * 0.39, 0), w * 0.66, w * 1.15, sq=0.85, a=a))          # the hood
    for sx in (-1, 1):
        p = ff(-L / 2 - u(0.9, 1.4), sx * 0.45); o.up(ell(p[0], p[1], 0.62, 0.5, a, 10), u(0.4, 0.6), SU_)
    return rect(*ff(-0.6, 0), L + 2.6, 2 * w + 0.4, a), o.L

@x_d("sundered", "made", SU_, "statue_head")
def _(cx, cy, a, rng):
    """the head of a great statue lying on its cheek: the round of the skull, the brow and the line of the nose
    standing up in profile, the hair in carved locks, the broken neck; a crack across the crown"""
    u = rng.uniform; r = u(1.55, 1.8); ff = frame(cx, cy, a); o = Obj(SU_)
    hd = o.st(dome(cx, cy, r, r * 1.15, sq=0.85, a=a), cuts=[(U([disc(*pol(cx, cy, r * 0.8, a + math.pi + t), 0.3, 8) for t in np.linspace(-1.2, 1.2, 6)]), r * 0.85)])
    o.st(ext(U([ell(*ff(r * 0.82, 0), 0.55, 0.45, a, 10), rect(*ff(r * 0.95, 0), 0.9, 0.55, a)]), r * 0.7))
    o.st(ext(stone(*ff(-r * 0.2, -r * 0.95), 0.75, rng, 0.8, 6), 0.7))
    return disc(cx, cy, r * 1.3), o.L

@x_d("sundered", "made", SU_, "statue_hand")
def _(cx, cy, a, rng):
    """a great stone hand broken off at the wrist, palm down: the back of the hand, four fingers lying side by side
    curling at their tips, the thumb out to one side, the knuckles standing up"""
    u = rng.uniform; ff = frame(cx, cy, a); o = Obj(SU_); s = u(0.9, 1.05); sv = rng.choice((-1, 1))
    palm = ell(*ff(-0.6 * s, 0), 1.3 * s, 1.15 * s, a, 16); o.st(dome(*ff(-0.6 * s, 0), 1.15 * s, 0.95 * s, sq=0.88, a=a), foot=palm)
    for j in range(4):
        v = (j - 1.5) * 0.52 * s; Lf = (1.7 - 0.25 * abs(j - 1.3)) * s
        o.st(rod([ff(0.3 * s, v), ff(0.3 * s + Lf, v * 1.05)], 0.27 * s, 1.4))
    o.st(rod([ff(-0.6 * s, sv * 1.0 * s), ff(0.3 * s, sv * 1.55 * s)], 0.3 * s, 1.4))
    o.st(ext(stone(*ff(-1.9 * s, 0), 0.75 * s, rng, 0.9, 6), 0.75 * s))
    return rect(*ff(0, 0), 4.6 * s, 3.6 * s, a), o.L

@x_d("sundered", "made", ST_, "empty_plinth")
def _(cx, cy, a, rng):
    """a statue's plinth with only the feet left standing on it: the stepped base, the die with a sunk panel where
    the name was cut, the bare feet and the hem of the robe snapped off above the ankles"""
    u = rng.uniform; P = u(3.6, 4.0); o = Obj(ST_); ff = frame(cx, cy, a)
    b0 = o.up(su_chip(rect(cx, cy, P, P, a), rng), 0.45); b1 = o.on(rect(cx, cy, P - 0.7, P - 0.7, a), b0, 0.9)
    o.cut(rect(*ff(P / 2 - 0.55, 0), 0.25, P - 1.4, a), b1, 0.1)
    cap = o.on(rect(cx, cy, P - 0.5, P - 0.5, a), b1, 0.25)
    for sx in (-1, 1): o.on(ell(*ff(0.15, sx * 0.42), 0.6, 0.32, a, 10), cap, u(0.4, 0.6), SU_)
    o.on(ell(*ff(-0.35, 0), 0.6, 0.9, a, 12).intersection(rect(*ff(-0.6, 0), 0.6, 2, a)), cap, u(0.7, 0.9), SU_)
    return rect(cx, cy, P, P, a).buffer(0.3), o.L


# ---------------------------------------------------------------------------------------------- masonry and rubble
@x_d("sundered", "debris", ST_, "ashlar_blocks")
def _(cx, cy, a, rng):
    """dressed blocks thrown out of a wall: squared stones of a course, tumbled at angles, corners knocked off, one
    lying on another"""
    u = rng.uniform; o = Obj(ST_); ff = frame(cx, cy, a); last = None
    want = rng.randint(3, 4)
    for i in range(30):
        if len(o.F) >= want: break
        c = ff(u(-2.4, 2.4), u(-1.6, 1.6)); t = a + u(-0.8, 0.8)
        g = su_chip(rect(c[0], c[1], u(1.9, 2.4), u(1.1, 1.3), t), rng, (0.25, 0.45))
        if any(g.distance(q) < 0.25 for q in o.F): continue
        last = o.up(g, u(0.75, 1.0))
    if last is not None and rng.random() < 0.5:
        g = [q for q in o.F][-1]; c = g.centroid; o.on(rect(c.x, c.y, 1.3, 0.9, a + u(0, 3)), last, 0.6)
    if not o.F: return None
    return o.out()

@x_d("sundered", "debris", "rubble", "masonry_heap")
def _(cx, cy, a, rng):
    """a heap of rubble where a wall came down: a mound of broken stone and dust with squared blocks lying on and
    against it, a cut stone sticking out of its top"""
    u = rng.uniform; R = u(2.2, 2.7); o = Obj("rubble")
    g = blob(cx, cy, R, rng, 6).buffer(0.1); hp = o.st(dome(cx, cy, R, u(1.0, 1.3), sq=u(0.75, 0.95), a=a), "spoil", foot=g)
    for i in range(rng.randint(3, 5)):
        p = pol(cx, cy, R * u(0.55, 1.05), a + i * 1.4 + u(-0.3, 0.3)); o.up(su_chip(rect(p[0], p[1], u(1.2, 1.6), u(0.9, 1.1), u(0, 3)), rng, (0.2, 0.3)), u(0.6, 1.1), ST_)
    o.st(ext(rect(cx + u(-0.4, 0.4), cy + u(-0.4, 0.4), 1.1, 0.9, u(0, 3)), u(1.4, 1.7)), ST_)
    return g.buffer(0.6), o.L

@x_d("sundered", "debris", ST_, "heaved_flags")
def _(cx, cy, a, rng):
    """flagstones of the square heaved up by a near miss: broken slabs lifted at their edges, standing a little proud
    of their neighbours, cracks running across them, one split in two"""
    u = rng.uniform; o = Obj(ST_); ff = frame(cx, cy, a); s = u(1.9, 2.2)
    for i in range(-1, 2):
        for j in range(-1, 1):
            if rng.random() < 0.25: continue
            c = ff(i * (s + 0.25) + u(-0.15, 0.15), (j + 0.5) * (s + 0.25)); t = a + u(-0.15, 0.15); g = rect(c[0], c[1], s, s * u(0.85, 1.0), t)
            if rng.random() < 0.35: g = su_chip(g, rng, (0.35, 0.6))
            b = o.up(g, u(0.3, 0.75))
            if rng.random() < 0.5: o.cut(wavy(pol(c[0], c[1], s, t + u(0.3, 1.2)), pol(c[0], c[1], s, t + math.pi + u(0.3, 1.2)), rng, 0.2, 3, 0.2).intersection(inset(g, 0.2)), b, 0.15)
    if not o.F: return None
    return o.out()

@x_d("sundered", "debris", "concrete", "rebar_slab")
def _(cx, cy, a, rng):
    """a broken slab of a reinforced floor: the concrete snapped off ragged on one side, the bent reinforcing bars
    sticking out of the break and curling across the ground"""
    u = rng.uniform; ff = frame(cx, cy, a); L = u(3.8, 4.6); W = u(2.8, 3.3); o = Obj("concrete")
    g = Polygon([ff(-L / 2, -W / 2), ff(L / 2 - u(0, 0.6), -W / 2)] + [ff(L / 2 + u(-0.5, 0.4), v) for v in np.linspace(-W / 2 + 0.4, W / 2 - 0.4, 5)] + [ff(L / 2 - u(0, 0.6), W / 2), ff(-L / 2, W / 2)]).buffer(0)
    b = o.up(su_chip(g, rng, (0.4, 0.7)), u(0.6, 0.8))
    o.cut(wavy(ff(u(-L / 3, 0), -W), ff(u(0, L / 3), W), rng, 0.3, 4, 0.22).intersection(inset(g, 0.25)), b, 0.15)
    su_bars(o, [su_bent(ff(L / 2 - 0.4, v), a + u(-0.3, 0.3), u(2.2, 3.2), rng) for v in np.linspace(-W / 2 + 0.7, W / 2 - 0.7, rng.randint(2, 3))], 0.24)
    return rect(*ff(1.0, 0), L + 3.0, W + 1.5, a), o.L

@x_d("sundered", "debris", "rust", "rebar_tangle")
def _(cx, cy, a, rng):
    """loose reinforcing bars torn out of the concrete: a few long bars bent and crossing each other on the ground,
    lumps of concrete still on some"""
    u = rng.uniform; o = Obj("rust"); bars = []
    for i in range(rng.randint(3, 4)):
        p = pol(cx, cy, u(0, 1.2), u(0, 6.28)); bars.append(su_bent(p, a + u(-0.7, 0.7) + math.pi * (i % 2), u(3.0, 4.2), rng, 4, 0.5))
    su_bars(o, bars, 0.25)
    for pts in bars[:2]:
        if rng.random() < 0.6: p = pts[-1]; o.up(stone(p[0], p[1], u(0.55, 0.7), rng, 0.8, 5), u(0.45, 0.6), "concrete")
    return hull([disc(x, y, 0.5) for pts in bars for x, y in pts]), o.L

@x_d("sundered", "made", "wood", "gothic_door")
def _(cx, cy, a, rng):
    """a cathedral door blown off its hinges and lying flat: the boards under their pointed head, the iron straps
    running across with their scrolled ends, the studs, a ring handle; split along a board on some"""
    u = rng.uniform; W = u(3.2, 3.6); S = u(3.0, 3.6); ff = frame(cx, cy, a); fw = frame(*ff(-(S + W * 0.866) / 2, 0), a); o = Obj("wood")
    g = su_lancet(fw, W, S)
    if rng.random() < 0.35: g = max(polys_of(g.difference(barl(fw(-1, u(-0.6, 0.6)), fw(S + W, u(-0.6, 0.6)), 0.35))), key=lambda q: q.area)
    b = o.up(g, u(0.45, 0.55))
    o.cut(U([barl(fw(-1, v), fw(S + W, v), 0.2) for v in np.linspace(-W / 2 + W / 5, W / 2 - W / 5, 4)]), b, 0.12)
    st_ = U([barl(fw(x, -W / 2), fw(x, W / 2 * 0.75), 0.5) for x in (0.7, S * 0.55, S + 0.3)]).intersection(inset(g, 0.15))
    o.on(st_, b, 0.15, "iron")
    o.on(U([disc(*fw(x, v), 0.24, 6) for x in (0.7, S * 0.55, S + 0.3) for v in np.linspace(-W / 2 + 0.4, W / 2 - 0.5, 4)]).intersection(inset(g, 0.2)), b, 0.25, "iron")
    rp_ = fw(S * 0.55 - 0.9, W * 0.25); rg = o.on(disc(rp_[0], rp_[1], 0.55, 12), b, 0.2, "iron"); o.cut(disc(rp_[0], rp_[1], 0.22, 8), rg, 0.12)
    return g.buffer(0.3), o.L

@x_d("sundered", "made", "wood", "toppled_pew")
def _(cx, cy, a, rng):
    """a long pew dragged out of the nave and tipped on its back: the seat and the back as two long boards side by
    side, the carved ends with their pointed poppy-head tops, one end board split away"""
    u = rng.uniform; L = u(6.6, 7.6); ff = frame(cx, cy, a); o = Obj("wood")
    st_ = o.up(rect(*ff(0, -0.7), L - 0.7, 1.25, a), 0.55); bk = o.up(rect(*ff(0, 0.75), L - 0.7, 1.3, a), 0.75)
    o.cut(barl(ff(-L, 0.75), ff(L, 0.75), 0.2), bk, 0.12)
    for sx in (-1, 1):
        if sx > 0 and rng.random() < 0.3: c = ff(L / 2 + 1.2, u(-0.5, 0.5)); o.up(rect(c[0], c[1], 0.95, 2.6, a + u(-0.5, 0.5)), 0.5); continue
        e = o.up(U([rect(*ff(sx * (L / 2 - 0.4), 0), 0.95, 3.0, a), tri(ff(sx * (L / 2 - 0.4), 1.4), ff(sx * (L / 2 - 0.4), 2.3), 0.95)]), 1.0)
        o.cut(disc(*ff(sx * (L / 2 - 0.4), -0.5), 0.22, 6), e, 0.1)
    return rect(*ff(0, 0.3), L + 0.6, 4.4, a), o.L

@x_d("sundered", "made", "iron", "spiked_railing")
def _(cx, cy, a, rng):
    """a run of wrought iron railing torn down and lying bent: the two rails with the bars between them, each bar
    ending in a spear point past the top rail, the run kinked where a shell struck it"""
    u = rng.uniform; L = u(6.0, 7.2); o = Obj("iron"); kink = u(-0.15, 0.15) * L; bend = rng.choice((-1, 1)) * u(0.25, 0.5)
    def F(x, v):
        if x <= kink: return frame(cx, cy, a)(x, v)
        p = frame(cx, cy, a)(kink, 0); return frame(p[0], p[1], a + bend)(x - kink, v)
    rails = U([line([F(x, v) for x in np.linspace(-L / 2, L / 2, 9)], 0.5) for v in (-0.9, 0.9)])
    bars = U([barl(F(x, -1.25), F(x, 1.15), 0.48) for x in np.arange(-L / 2 + 0.3, L / 2, 0.95)])
    tips = U([tri(F(x, 1.1), F(x, 1.95), 0.8) for x in np.arange(-L / 2 + 0.3, L / 2, 0.95)])
    o.st(ext(U([rails, bars]), 0.42), foot=U([rails, bars]).buffer(0.1))
    o.st(ext(tips, 0.32), foot=tips)
    return hull([rails, tips]).buffer(0.2), o.L

@x_d("sundered", "made", "brass", "fallen_bell")
def _(cx, cy, a, rng):
    """a great bell brought down with its tower, lying on its side: the crown and the cannons on top, the waist
    flaring to the sound bow and the open mouth, a crack up from the lip; a broken beam of the frame beside it"""
    u = rng.uniform; L = u(3.8, 4.4); R = u(1.6, 1.8); ff = frame(cx, cy, a); o = Obj("brass")
    def rf(q):
        s = (q + L / 2) / L
        return R * (0.45 + 0.25 * s + (0.45 * ((s - 0.72) / 0.28) ** 1.6 if s > 0.72 else 0.0)) if s < 0.97 else R * 1.12
    o.st(spin(cx, cy, a, L, rf, 1.0), cuts=[(ell(*ff(L / 2 - 0.25, 0), 0.45, R * 0.9, a, 14), R * 0.5), (barl(ff(L / 2 - 1.2, R * 0.3), ff(L / 2, R * 0.4), 0.22), R * 1.05)], foot=rect(cx, cy, L, 2.2 * R, a))
    o.st(ext(U([rect(*ff(-L / 2 - 0.35, 0), 0.8, 1.1, a)]), R * 0.8))
    p = ff(u(-0.5, 0.5), rng.choice((-1, 1)) * (R + 1.0)); o.up(rect(p[0], p[1], u(3.6, 4.6), 0.95, a + u(-0.4, 0.4)), 0.8, "wood")
    return rect(*ff(-0.2, 0), L + 1.2, 2 * R + 2.6, a), o.L


# ---------------------------------------------------------------------------------------------- the fighting
@x_d("sundered", "made", "brass", "shell_casings")
def _(cx, cy, a, rng):
    """spent artillery casings thrown out of a gun position: tall brass cases lying about with their rims and primer
    caps showing on the base, one still standing on its base"""
    u = rng.uniform; o = Obj("brass"); foot = []
    want = rng.randint(2, 4)
    for i in range(30):
        if len(foot) >= want: break
        p = pol(cx, cy, u(0.3, 2.0), u(0, 6.28)); t = u(0, 6.28); L = u(2.2, 2.6); r = u(0.42, 0.5)
        g = rect(p[0], p[1], L + 0.3, 2 * r + 0.3, t)
        if any(g.intersects(q) for q in foot): continue
        foot.append(g); o.st(spin(p[0], p[1], t, L, lambda q, r=r, L=L: r * (1.12 if q < -L / 2 + 0.15 else 1.0) * (0.92 if q > L / 2 - 0.25 else 1.0), 1.15))
    if rng.random() < 0.6:
        p = pol(cx, cy, u(2.6, 3.0), u(0, 6.28)); b = o.up(disc(p[0], p[1], 0.58, 16), u(1.8, 2.1)); o.cut(ann(p[0], p[1], 0.34, 0.12, 10), b, 0.35); foot.append(disc(p[0], p[1], 0.6))
    if not foot: return None
    return hull(foot), o.L

@x_d("sundered", "made", "iron", "dud_shell")
def _(cx, cy, a, rng):
    """a heavy shell that never went off, nose down in the ground at a slant: the ogive nose and the body with its
    copper driving band, the base end raised, the ground heaved up round where it struck"""
    u = rng.uniform; L = u(3.4, 4.0); r = u(0.7, 0.8); ff = frame(cx, cy, a); o = Obj("iron")
    def rf(q):
        s = (q + L / 2) / L
        if s < 0.42: return r * math.sqrt(max(0.04, s / 0.42)) * 0.98 + 0.05
        return r * (1.08 if 0.82 < s < 0.88 else 1.0)
    o.st(spin(cx, cy, a, L, rf, 1.0), foot=rect(cx, cy, L, 2 * r, a))
    o.st(ext(rect(*ff(L * 0.35, 0), 0.5, 2 * r * 1.08, a), r * 1.08), "brass")
    o.st(dome(*ff(-L / 2 + 0.1, 0), 1.3, 0.6, sq=0.85, a=a), "spoil")
    return rect(*ff(-0.3, 0), L + 1.6, 2.8, a), o.L

@x_d("sundered", "made", "sack", "sandbag_line")
def _(cx, cy, a, rng):
    """a short curved breastwork of sandbags thrown across a gap: a row of filled bags laid header and stretcher, a
    second row on top set over the joints, a few bags burst and slumped"""
    u = rng.uniform; R = u(6.0, 8.0); n = rng.randint(4, 5); o = Obj("sack"); ox, oy = pol(cx, cy, -R, a); foot = []; ps = []
    for i in range(n):
        t = a + (i - (n - 1) / 2) * 0.27; p = pol(ox, oy, R, t)
        ps += loaf(p[0], p[1], t + math.pi / 2, 1.75, 1.1, u(0.7, 0.85)); foot.append(rect(p[0], p[1], 1.8, 1.15, t + math.pi / 2))
        if i < n - 1 and rng.random() < 0.85:
            q = pol(ox, oy, R, t + 0.135); ps += loaf(q[0], q[1], t + 0.135 + math.pi / 2, 1.6, 0.95, 0.75, lift=0.55)
    o.st(ps, foot=U(foot))
    if rng.random() < 0.5: p = pol(ox, oy, R + 1.4, a + u(-0.5, 0.5)); o.st(dome(p[0], p[1], 0.9, 0.4, sq=0.7, a=a))
    return hull(foot).buffer(0.3), o.L

@x_d("sundered", "made", "wood", "barricade_beams")
def _(cx, cy, a, rng):
    """a barricade thrown up from what was to hand: roof beams laid across each other in a low heap, a cart door,
    blocks of stone piled against them"""
    u = rng.uniform; ff = frame(cx, cy, a); o = Obj("wood"); L = u(6.0, 7.0)
    b1 = o.up(rect(*ff(0, -0.6), L, 1.0, a + u(-0.08, 0.08)), 0.85)
    o.cut(U([barl(ff(x, -2), ff(x, 1), 0.2) for x in (-L / 3, L / 4)]), b1, 0.12)
    o.up(rect(*ff(0, 0.9), L * 0.8, 0.95, a + u(-0.1, 0.1)), 0.8)
    for sx in (-1, 1):
        t = a + sx * u(0.5, 0.8); c = ff(sx * L * 0.22, 0.2); o.st(ext(rect(c[0], c[1], 4.2, 0.9, t), 1.3), foot=rect(c[0], c[1], 4.2, 0.9, t))
    for i in range(rng.randint(2, 3)):
        p = ff(u(-L / 2, L / 2), rng.choice((-1, 1)) * u(1.8, 2.4)); o.up(su_chip(rect(p[0], p[1], 1.4, 1.0, u(0, 3)), rng, (0.2, 0.3)), u(0.7, 0.9), ST_)
    return rect(cx, cy, L + 1.2, 5.6, a), o.L

@d("sundered", "marks", "pothole", "shell_scar", ground=True)
def _(cx, cy, a, rng):
    """where a shell burst on the paving: a shallow scooped hole, the spokes of cracks running out of it, the
    splinter gouges round it"""
    u = rng.uniform; r = u(1.2, 1.6); hole = stone(cx, cy, r, rng, 0.85, 9)
    cr = U([LineString(wiggle(pol(cx, cy, r * 0.8, t), pol(cx, cy, r + u(1.4, 2.4), t + u(-0.2, 0.2)), rng, 0.25, 3)).buffer(0.22, cap_style=1, resolution=3) for t in np.linspace(0, 6.28, rng.randint(5, 7), endpoint=False) + u(0, 1)])
    pk = U([ell(*pol(cx, cy, r + u(0.8, 2.2), u(0, 6.28)), u(0.3, 0.45), 0.24, u(0, 3), 8) for i in range(rng.randint(4, 7))])
    return hull([hole.buffer(r + 2.4)]), [("gcut", hole, 0.55, "pothole"), ("gcut", cr.difference(hole.buffer(0.1)), 0.35, "crack"), ("gcut", pk.difference(cr.buffer(0.15)).difference(hole.buffer(0.2)), 0.3, "pothole")]

@d("sundered", "marks", "crack", "split_flags", ground=True)
def _(cx, cy, a, rng):
    """the paving split by the shock of the shelling: a long crack running across the ground and branching, chips
    gone from its edges"""
    u = rng.uniform; ff = frame(cx, cy, a); L = u(5, 8); pts = wiggle(ff(-L / 2, 0), ff(L / 2, 0), rng, 0.5, 6)
    j = rng.randint(2, 4); b = a + rng.choice((-1, 1)) * u(0.5, 1.0); q = (pts[j][0] + L * 0.35 * math.cos(b), pts[j][1] + L * 0.35 * math.sin(b))
    g = U([LineString(pts).buffer(0.26, cap_style=1, resolution=3), LineString(wiggle(pts[j], q, rng, 0.3, 3)).buffer(0.22, cap_style=1, resolution=3)] + [disc(*pts[k], u(0.35, 0.5), 6) for k in (1, len(pts) - 2)])
    return g.buffer(0.6), [("gcut", g, 0.4, "crack")]
