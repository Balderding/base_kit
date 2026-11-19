"""Recognisable multi-part details (10 more per theme) and landmark set pieces for 60 mm bases and up.

Unlike the one-shape templates in details.py, each feature here is made of several parts placed together: a wagon
wheel is a rim, a hub and spokes; a well is a wall round a hole. A builder returns a list of layers:

  ("up", polygon, height, kind)                 raised from the surface the feature stands on
  ("mound", polygon, height, kind, tiers)       raised with sloped sides
  ("steps", footprint, [(polygon, height), ...], kind, parent) nested pieces, each from the ground (or the parent's top) to its own height: together a rounded,
                                                turned or ridged shape (a barrel, a log, a tent, an urn). "on" and "tcut" can stand on its highest piece
  ("on", polygon, parent layer no., height, kind)   stands on top of an earlier raised layer
  ("tcut", polygon, parent layer no., depth)    cut into the top of an earlier raised layer
  ("gcut", polygon, depth, kind)                cut into the ground
  ("pad", polygon, kind)                        low piece standing in a pool that this feature cut (stepping stone)

comp() finds free space for the whole footprint and then builds the layers. Sizes are real millimetres and do not
grow with the base; landmarks (the second half of the file) are the things that do.
gg3 calls install() once, then extend() adds everything to the theme tables in details.py."""
import math

def install(G):
    _c = {"fire": "#e2792b", "ash": "#4a4744", "twig": "#7b5a36", "rail": "#55585e", "hull": "#7a5a3c", "rope": "#b59b63", "sail": "#d9d1b8"}
    for k, v in _c.items(): G["gg"].COL.setdefault(k, v)
    globals().update({k: v for k, v in G.items() if not k.startswith("__") and k not in _OWN})       # helpers from gg3, never over this file's own names
    for k in list(G["BRIDGE"]):                              # floor kinds: a feature made of one gets its own name, so nothing treats it as a floor to stand on
        G["gg"].COL.setdefault(k + "_p", G["gg"].COL.get(k, "#999999"))

def _k(kind): return kind + "_p" if kind in BRIDGE else kind


def _u(gs):
    g = unary_union([x for x in gs if x is not None and not x.is_empty])
    return g

def _disc(x, y, r, n=20): return Point(x, y).buffer(r, n)
def _ann(x, y, ro, ri, n=24): return Point(x, y).buffer(ro, n).difference(Point(x, y).buffer(ri, n))
def _cap(p, q, w): return LineString([p, q]).buffer(w / 2, 8)
def _bar(p, q, w): return LineString([p, q]).buffer(w / 2, cap_style=2)
def _biggest(g):
    ps = polys_of(g)
    return max(ps, key=lambda q: q.area) if ps else None

SOFT_T = ((0.55, 25), (0.45, 50))
ROCK_T = ((0.7, 6), (0.3, 40))
SPIKE_T = ((0.75, 4), (0.25, 30))
PEB_T = ((0.7, 10), (0.3, 45))      # small stones: gentle sides, so even a 1.2 mm pebble keeps a top


def comp(build, r=10.5, gap=0.4, ground=False, tries=60):
    """turn a builder(cx, cy, angle, rng) -> (footprint, layers) into a detail function"""
    def fn(s, rng, lvl=0, scale=1.0):
        hold = {}
        def mk(cx, cy):
            got = build(cx, cy, aim(rng, cx, cy), rng) if scale == 1.0 else build(cx, cy, aim(rng, cx, cy), rng, scale)
            if got is None: return None
            hold["g"] = got; return got[0]
        g, z = find(s, mk, gap=gap, at=lambda: rp(rng, 0, r), tries=tries, ground=ground)
        if g is None: return False
        return _build(s, z, hold["g"][1])
    fn.tmpl, fn.kind, fn.shape = "comp", getattr(build, "kind", ""), getattr(build, "tag", "")
    return fn


ROUND_KINDS = {"bone", "root", "rope", "vein", "tendril", "cable", "twig", "tooth", "chain", "sack", "egg", "pustule", "flesh", "moss"}
ROUND_T = ((0.45, 12), (0.3, 38), (0.25, 62))          # a round back: steep at the foot, easing over the top

def _build(s, z, layers):
    tops = {}; shape = {}; done = False; carry = {L[2] for L in layers if L[0] in ("on", "tcut")}          # layers that something stands on or is cut into keep their flat top
    for i, L in enumerate(layers):
        t = L[0]
        try:
            if t == "up" and i not in carry and str(L[3]) in ROUND_KINDS and L[2] < 1.6:
                ps = [q for q in polys_of(L[1]) if wide_enough(q, 0.7)]
                if ps: mound(s, ps, z, L[2], ROUND_T, _k(L[3])); done = True
            elif t == "up":
                ps = [q for q in polys_of(L[1]) if wide_enough(q)]
                if ps: tops[i] = (up(s, ps, z, L[2], _k(L[3])), L[1]); done = done or bool(tops[i][0])
            elif t == "mound":
                ps = [q for q in polys_of(L[1]) if wide_enough(q, 0.7)]
                if ps: mound(s, ps, z, L[2], L[4] if len(L) > 4 else ROCK_T, _k(L[3])); done = True
            elif t == "steps":      # ("steps", footprint, [(polygon, height), ...], kind[, parent]): nested pieces that together draw a rounded or turned shape
                par = tops.get(L[4]) if len(L) > 4 and L[4] is not None else None
                if len(L) > 4 and L[4] is not None and (not par or not par[0]): continue
                zb = par[0] if par else z; b = HBOOST[0] if max((q[1] for q in L[2]), default=0.0) >= 1.0 else 1.0; best = None; rough = terr(s) and not par; zlow = None
                if rough: zlow, zb = T_zr(s, L[1])                                   # stepped bedrock: lowest and highest ground under the piece
                for k, pc in enumerate(L[2]):
                    q, h = pc[0], pc[1]; kd = _k(L[3])
                    if len(pc) > 3: kd = gg.LK(kd); kd.loft = (pc[2], zb + pc[3] * b)          # the mesh builder slopes from piece to piece
                    if len(pc) > 4:          # the footprint of true 3D solids (--supports on): not built into the surface, it only holds the spot; the solids go on at the end
                        kd.ghost = pc[2]
                        if not hasattr(s, "solids3d"): s.solids3d = []
                        s.solids3d.append((pc[2], float(zb), b * pc[1] / max(0.01, _sc().TOP3.get(pc[2], pc[1]))))
                    if par: q = q.intersection(inset(par[1], 0.12))
                    ps = [x for x in polys_of(q) if x.area > 0.12]
                    if not ps: continue
                    top = zb + h * b; got = None
                    if par or rough or (k and (best or len(L[2][0]) > 4)):
                        if top - zb >= 0.1:
                            new = s.prism(ps, top, kd, z0=(zlow - 0.2) if rough else (zb if par else max(zb, H)) - 0.15, minw=0.45, foot=False)
                            if new: got = top
                            if new and rough and best is None: s.feet += new; s.objs += new
                    elif getattr(z, "bridge", False) and best is None:               # on a plank floor the first piece goes the usual way (it notches the planks)
                        b0 = HBOOST[0]; HBOOST[0] = 1.0
                        try: got = up(s, [x for x in ps if wide_enough(x)], zb, h * b, kd)
                        finally: HBOOST[0] = b0
                    elif top - zb >= 0.1:                                             # the foot of a rounded shape may be slender (a rib, a rope): it is held by the rest of it
                        new = s.prism(ps, top, kd, z0=None if zb <= H else zb - 0.15, minw=0.45, foot=zb <= H)
                        if new: got = top
                    if got and len(pc) > 4: done = True; continue                    # the marker of true solids: nothing stands on it or is cut into it
                    if got and (best is None or got > best[0]): best = (got, _u(ps))
                if best: tops[i] = best; done = True; shape[i] = (L[1], zb, b)
            elif t == "on":
                par = tops.get(L[2])
                if not par or not par[0] or par[0] + 0.3 > CAPZ: continue
                q = L[1].intersection(inset(par[1], 0.12))
                ps = [x for x in polys_of(q) if wide_enough(x, 0.45) and x.area > 0.15]
                if ps:
                    top = min(par[0] + L[3] * (HBOOST[0] if L[3] >= 1.0 else 1.0), CAPZ); s.prism(ps, top, _k(L[4]), z0=par[0] - 0.15, foot=False); tops[i] = (top, _u(ps))
            elif t == "tcut":
                par = tops.get(L[2])
                if not par or not par[0]: continue
                sh = shape.get(L[2])
                q = L[1].intersection(inset(sh[0] if sh else par[1], 0.3))
                ps = [x for x in polys_of(q) if x.area > 0.05]
                dep = (par[0] - (sh[1] + L[3] * sh[2])) if sh and len(L) > 4 and L[4] == "floor" else L[3]          # "floor": L[3] is the height left standing, not a depth
                if ps and dep > 0.05: topcut(s, ps, par[0], dep, "slot")
            elif t == "gcut":
                ps = [x for x in polys_of(L[1]) if x.area > 0.08]
                if ps: cut(s, ps, z, L[2], L[3]); done = True
            elif t == "pad":
                if z <= H + 1e-6 and not terr(s):
                    ps = [x for x in polys_of(L[1]) if wide_enough(x, 0.92)]
                    if ps: gg.COL.setdefault(L[2] + "_pad", gg.COL.get(L[2], "#999999")); s.prism(ps, H - 0.12, L[2] + "_pad", z0=H - 0.8, foot=False)      # own kind: a stone standing in water is meant to be there
        except Exception as e:
            print("  ! feature layer", t, type(e).__name__, str(e)[:70], flush=True)
    return done


# ====================================================================================================== builders
def b_wheel(kind="wood", iron="iron"):
    def f(cx, cy, a, rng):
        R = rng.uniform(2.4, 3.0)
        spokes = [_bar((cx - (R - 0.3) * math.cos(a + k * math.pi / 3), cy - (R - 0.3) * math.sin(a + k * math.pi / 3)),
                       (cx + (R - 0.3) * math.cos(a + k * math.pi / 3), cy + (R - 0.3) * math.sin(a + k * math.pi / 3)), 0.92) for k in range(3)]
        g = _u([_ann(cx, cy, R, R - 0.95)] + spokes + [_disc(cx, cy, 0.95)])
        if rng.random() < 0.5:                                # a broken wheel: one piece of the rim gone
            b = rng.uniform(0, 6.283); g = _biggest(g.difference(Polygon([(cx, cy), (cx + 2 * R * math.cos(b), cy + 2 * R * math.sin(b)), (cx + 2 * R * math.cos(b + 0.7), cy + 2 * R * math.sin(b + 0.7))]).difference(_disc(cx, cy, 0.9))))
        return _disc(cx, cy, R), [("up", g, 0.5, kind), ("on", _disc(cx, cy, 0.6), 0, 0.3, iron)]
    return f

def b_ladder(kind="wood"):
    def f(cx, cy, a, rng):
        L = rng.uniform(6.0, 8.5); ff = frame(cx, cy, a); miss = rng.randint(0, 5)
        parts = [_bar(ff(-L / 2, v), ff(L / 2, v), 0.92) for v in (-1.05, 1.05)]
        us = np.arange(-L / 2 + 0.9, L / 2 - 0.6, 1.7)
        parts += [_bar(ff(u, -1.2), ff(u, 1.2), 0.92) for i, u in enumerate(us) if i != miss]
        g = _u(parts)
        return g.convex_hull, [("up", g, 0.45, kind)]
    return f

def b_track(rail="iron", tie="wood", L=(8.0, 11.5)):
    def f(cx, cy, a, rng, k=1.0):
        Ln = rng.uniform(*L) * k; ff = frame(cx, cy, a)
        ties = _u([rect(*ff(u + rng.uniform(-0.1, 0.1), 0), 0.95, 3.6, a + rng.uniform(-0.06, 0.06)) for u in np.arange(-Ln / 2 + 0.6, Ln / 2 - 0.3, 1.65)])
        rails = _u([_bar(ff(-Ln / 2, v), ff(Ln / 2, v), 0.92) for v in (-1.15, 1.15)])
        return rect(cx, cy, Ln, 3.8, a), [("up", ties, 0.4, tie), ("up", rails, 0.85, rail)]
    return f

def b_campfire(stone_kind="rock", log="wood"):
    def f(cx, cy, a, rng):
        n = rng.randint(6, 7); R = rng.uniform(2.2, 2.5)
        st = [stone(cx + R * math.cos(a + 6.283 * i / n), cy + R * math.sin(a + 6.283 * i / n), rng.uniform(0.6, 0.75), rng, 0.85, rng.randint(5, 7)) for i in range(n)]
        b = rng.uniform(0, math.pi); logs = _u([_cap((cx - 1.15 * math.cos(b + d), cy - 1.15 * math.sin(b + d)), (cx + 1.15 * math.cos(b + d), cy + 1.15 * math.sin(b + d)), 0.92) for d in (0, 1.2)])
        return _disc(cx, cy, R + 0.7), [("mound", q, rng.uniform(0.45, 0.6), stone_kind, PEB_T) for q in st] + [("up", logs, 0.5, log)]
    return f

def _sc():
    import scenes; return scenes

def b_skull(kind="bone", big=1.0):
    return _sc().b_skull(kind, big)          # the rounded version; the flat one below is kept for reference
    def f(cx, cy, a, rng):
        k = big * rng.uniform(0.9, 1.15); ff = frame(cx, cy, a)
        cran = affinity.rotate(affinity.scale(_disc(cx, cy, 1.0, 24), 1.45 * k, 1.2 * k), math.degrees(a), origin=(cx, cy))
        jaw = rect(*ff(1.25 * k, 0), 1.1 * k, 1.25 * k, a)
        g = _u([cran, jaw]).buffer(0.25).buffer(-0.25)
        eyes = _u([_disc(*ff(0.45 * k, v * 0.5 * k), 0.3 * k, 12) for v in (-1, 1)] + [Polygon([ff(1.0 * k, 0), ff(1.35 * k, -0.2 * k), ff(1.35 * k, 0.2 * k)])])
        teeth = _u([_bar(ff(1.75 * k, v), ff(1.5 * k, v), 0.16) for v in (-0.35 * k, 0, 0.35 * k)])
        return g, [("up", g, 0.75 * min(k, 1.3), kind), ("tcut", eyes, 0, 0.4), ("tcut", teeth, 0, 0.25)]
    return f

def b_ribs(kind="bone", L=(5.5, 7.5)):
    return _sc().b_ribs(kind, L)          # the rounded version; the flat one below is kept for reference
    def f(cx, cy, a, rng, k=1.0):
        Ln = rng.uniform(*L) * k; ff = frame(cx, cy, a); n = max(3, int(Ln / (1.7 * max(1.0, k ** 0.5)))); w = 0.92 * max(1.0, k ** 0.5); reach = 2.3 * max(1.0, k ** 0.7)
        parts = [_cap(ff(-Ln / 2, 0), ff(Ln / 2, 0), 0.95 * max(1.0, k ** 0.5))]
        for i in range(n):
            u = -Ln / 2 + (i + 0.6) * Ln / (n + 0.4); t = 1 - abs(i - (n - 1) / 2) / n
            for sgn in (-1, 1):
                if rng.random() < 0.12: continue             # a rib is missing
                parts.append(LineString([ff(u, 0), ff(u + 0.25 * reach * 0.4, sgn * reach * 0.55 * (0.7 + 0.5 * t)), ff(u + 0.9, sgn * reach * (0.7 + 0.45 * t))]).buffer(w / 2, 6))
        g = _u(parts)
        return g.convex_hull, [("up", g, 0.6 * max(1.0, k ** 0.4), kind)]
    return f

def b_stairs(kind="tflag", steps=3):
    def f(cx, cy, a, rng):
        L = rng.uniform(4.6, 5.6); W = rng.uniform(3.0, 3.8); ff = frame(cx, cy, a); lay = []; base = rect(cx, cy, L, W, a)
        lay.append(("up", base, 0.5, kind))
        for i in range(1, steps):
            q = rect(*ff(i * 0.62, 0), L - i * 1.24, W, a)
            lay.append(("on", q, i - 1, 0.5, kind))
        return base, lay
    return f

def b_well(wall="rock", water="water"):
    def f(cx, cy, a, rng):
        R = rng.uniform(2.3, 2.9); n = rng.randint(7, 10)
        ring_ = _ann(cx, cy, R, R - 0.95, n)
        seams = _u([_bar((cx + (R - 1.1) * math.cos(a + 6.283 * i / n), cy + (R - 1.1) * math.sin(a + 6.283 * i / n)), (cx + (R + 0.2) * math.cos(a + 6.283 * i / n), cy + (R + 0.2) * math.sin(a + 6.283 * i / n)), 0.26) for i in range(n)])
        return _disc(cx, cy, R), [("up", ring_, rng.uniform(1.1, 1.6), wall), ("tcut", seams, 0, 0.25), ("gcut", _disc(cx, cy, R - 1.3, 16), 0.8, water)]
    return f

def b_fallen_column(kind="tcolumn", w=1.8):
    def f(cx, cy, a, rng, k=1.0):
        ff = frame(cx, cy, a); u = -4.0 * k; drums = []; seams = []; W = w * max(1.0, k ** 0.5)
        for i in range(rng.randint(3, 4) + int(k > 1.5)):
            Ld = rng.uniform(1.8, 2.8) * max(1.0, k ** 0.6); v = rng.uniform(-0.35, 0.35) * (i > 0); da = rng.uniform(-0.18, 0.18) * (i > 0)
            c = ff(u + Ld / 2, v); d = rect(c[0], c[1], Ld, W, a + da); drums.append(d)
            f2 = frame(c[0], c[1], a + da); seams += [_bar(f2(-Ld / 2, vv), f2(Ld / 2, vv), 0.24) for vv in (-W / 4, W / 4)]
            u += Ld + rng.uniform(0.45, 1.1)
        g = _u(drums)
        return g.convex_hull, [("up", d, rng.uniform(1.0, 1.35) * max(1.0, k ** 0.3), kind) for d in drums] + [("tcut", _u(seams), i, 0.22) for i in range(len(drums))]
    return f

def b_grate(kind="iron", round_=False):
    def f(cx, cy, a, rng):
        S = rng.uniform(3.0, 3.8); ff = frame(cx, cy, a)
        plate = _disc(cx, cy, S / 2, 28) if round_ else rect(cx, cy, S, S, a)
        slots = _u([_bar(ff(-S, v), ff(S, v), 0.36) for v in np.arange(-S / 2 + 0.75, S / 2 - 0.5, 0.78)]).intersection(inset(plate, 0.5))
        return plate, [("up", plate, 0.38, kind), ("tcut", slots, 0, 0.3)]
    return f

def b_rune_circle(kind="glyph", R=(3.0, 4.0)):
    def f(cx, cy, a, rng, k=1.0):
        Rr = rng.uniform(*R) * k; w = 0.4
        marks = []
        n = rng.randint(6, 9) + int(4 * (k - 1))
        for i in range(n):
            t = a + 6.283 * i / n; rm = Rr * 0.8; mx, my = cx + rm * math.cos(t), cy + rm * math.sin(t); f2 = frame(mx, my, t)
            segs = rng.sample([((-0.45, -0.45), (0.45, 0.45)), ((-0.45, 0.45), (0.45, -0.45)), ((0, -0.55), (0, 0.55)), ((-0.5, 0), (0.5, 0))], 2)
            marks += [_bar(f2(*p), f2(*q), 0.3) for p, q in segs]
        g = _u([_ann(cx, cy, Rr, Rr - w, 40), _ann(cx, cy, Rr * 0.6, Rr * 0.6 - w, 32), star(cx, cy, Rr * 0.42, Rr * 0.17, rng.choice((5, 6, 7)), a).buffer(0).exterior.buffer(0.2)] + marks)
        return _disc(cx, cy, Rr + 0.2, 32), [("gcut", g, 0.35, kind)]
    return f

def b_crystals(kind="crystal", hmax=3.0):
    def f(cx, cy, a, rng, k=1.0):
        kk = max(1.0, k ** 0.6); lay = [("mound", sh_hex_(cx, cy, rng.uniform(1.0, 1.35) * kk, a), rng.uniform(2.2, hmax), kind, SPIKE_T)]; foot = [lay[0][1]]
        for i in range(rng.randint(4, 6) + int(3 * (k - 1))):
            t = rng.uniform(0, 6.283); d = rng.uniform(1.7, 2.4) * kk * (1 + 0.5 * (k - 1) * rng.random()); r = rng.uniform(0.55, 0.85) * kk
            q = sh_hex_(cx + d * math.cos(t), cy + d * math.sin(t), r, rng.uniform(0, 1)) if rng.random() < 0.6 else stone(cx + d * math.cos(t), cy + d * math.sin(t), r, rng, 0.7, 3)
            if all(q.distance(p) > 0.25 for p in foot): foot.append(q); lay.append(("mound", q, rng.uniform(1.0, 1.9), kind, SPIKE_T))
        return _u(foot).convex_hull, lay
    return f

def sh_hex_(x, y, r, a): return Polygon([(x + r * math.cos(a + k * math.pi / 3), y + r * math.sin(a + k * math.pi / 3)) for k in range(6)])

def b_roots(kind="stump", root="root", R=(1.3, 1.7)):
    def f(cx, cy, a, rng, k=1.0):
        Rr = rng.uniform(*R) * max(1.0, k ** 0.8); st = stone(cx, cy, Rr, rng, 0.9, 9); parts = []
        for i in range(rng.randint(4, 6) + int(2 * (k - 1))):
            t = a + 6.283 * i / 5 + rng.uniform(-0.3, 0.3); Ln = rng.uniform(3.0, 4.8) * max(1.0, k ** 0.8)
            p0 = (cx + (Rr - 0.3) * math.cos(t), cy + (Rr - 0.3) * math.sin(t)); p1 = (cx + (Rr + Ln) * math.cos(t + rng.uniform(-0.35, 0.35)), cy + (Rr + Ln) * math.sin(t + rng.uniform(-0.35, 0.35)))
            pts = wiggle(p0, p1, rng, 0.45, 4)
            for j in range(len(pts) - 1): parts.append(_cap(pts[j], pts[j + 1], max(0.92, 1.15 - 0.08 * j) * max(1.0, k ** 0.4)))
        roots = _u(parts).difference(st.buffer(-0.25))
        rings = _u([_ann(cx, cy, r, r - 0.25, 18) for r in (Rr * 0.45, Rr * 0.75)])
        return _u([st, roots]).convex_hull.buffer(-0.2), [("up", roots, 0.5, root), ("up", st, rng.uniform(1.2, 1.9), kind), ("tcut", rings, 1, 0.25)]
    return f

def b_stepping_pool(stone_kind="rock", water="water"):
    def f(cx, cy, a, rng, k=1.0):
        pool = blob(cx, cy, rng.uniform(3.0, 3.8) * k, rng, rng.randint(4, 6)); ff = frame(cx, cy, a); pads = []
        n = int(4 * k) + 1
        for i in range(n):
            q = stone(*ff((i - (n - 1) / 2) * 1.9, rng.uniform(-0.5, 0.5)), rng.uniform(0.7, 0.95), rng, 0.85, rng.randint(5, 7))
            if inset(pool, 0.3).contains(q): pads.append(q)
        return pool, [("gcut", pool, 0.55, water)] + ([("pad", _u(pads), stone_kind)] if pads else [])
    return f

def b_tool(which="pick", metal="iron", wood="wood"):
    def f(cx, cy, a, rng):
        ff = frame(cx, cy, a)
        if which == "sword":
            blade = Polygon([ff(-1.6, -0.48), ff(2.9, -0.42), ff(3.8, 0), ff(2.9, 0.42), ff(-1.6, 0.48)])
            g = _u([blade, rect(*ff(-1.6, 0), 0.92, 2.3, a), _bar(ff(-3.0, 0), ff(-1.6, 0), 0.92), _disc(*ff(-3.1, 0), 0.55, 10)])
            return g.convex_hull, [("up", g, 0.4, metal)]
        if which == "spear":
            head = Polygon([ff(3.0, -0.65), ff(4.9, 0), ff(3.0, 0.65), ff(2.5, 0)])
            return rect(cx, cy, 9.6, 1.1, a), [("up", _bar(ff(-4.6, 0), ff(3.0, 0), 0.92), 0.4, wood), ("up", head, 0.5, metal)]
        if which == "rifle":
            g = _u([_bar(ff(-1.0, 0), ff(3.8, 0), 0.95), Polygon([ff(-3.4, -0.95), ff(-1.0, -0.5), ff(0.3, -0.5), ff(0.3, 0.5), ff(-1.0, 0.5), ff(-3.4, 0.4)])])
            return g.convex_hull, [("up", g, 0.45, wood), ("on", _bar(ff(0.3, 0), ff(3.8, 0), 0.92), 0, 0.25, metal)]
        if which == "shovel":
            blade = rect(*ff(3.0, 0), 1.9, 1.5, a).buffer(0.25)
            return rect(cx, cy, 8.4, 2.0, a), [("up", _bar(ff(-3.6, 0), ff(2.2, 0), 0.92), 0.4, wood), ("up", blade, 0.45, metal)]
        if which == "anchor":
            arc = LineString([ff(2.6 * math.cos(t) - 0.6, 2.6 * math.sin(t)) for t in np.linspace(-1.0, 1.0, 9)]).buffer(0.47, 6)
            g = _u([_bar(ff(-3.6, 0), ff(2.0, 0), 0.85), _bar(ff(-2.6, -1.5), ff(-2.6, 1.5), 0.92), arc, _ann(*ff(-4.3, 0), 1.0, 0.3, 14)])
            return g.convex_hull, [("up", g, 0.5, metal)]
        if which == "axe":
            head = Polygon([ff(2.2, -0.3), ff(3.3, -1.3), ff(3.9, -0.9), ff(3.9, 0.9), ff(3.3, 1.3), ff(2.2, 0.3)])
            return rect(cx, cy, 7.4, 2.8, a), [("up", _bar(ff(-3.4, 0), ff(3.0, 0), 0.92), 0.4, wood), ("up", head, 0.5, metal)]
        if which == "wrench":
            jaw = _ann(*ff(3.0, 0), 1.2, 0.4, 12).difference(rect(*ff(3.9, 0), 1.4, 0.8, a))
            g = _u([_bar(ff(-2.8, 0), ff(2.2, 0), 0.85), jaw, _ann(*ff(-3.2, 0), 1.0, 0.3, 12)])
            return g.convex_hull, [("up", g, 0.4, metal)]
        # pick
        head = LineString([ff(2.6 + 0.9 * math.cos(t) - 0.9, 2.3 * math.sin(t)) for t in np.linspace(-1.05, 1.05, 9)]).buffer(0.47, 6)
        return rect(cx, cy, 7.4, 4.8, a), [("up", _bar(ff(-3.4, 0), ff(2.7, 0), 0.92), 0.4, wood), ("up", head, 0.5, metal)]
    return f

def b_boss(kind="iron", boss="iron", R=(1.6, 2.1), cross=False):
    """a round plate with a raised middle: shield, hub cap, gong, lid, mill stone"""
    def f(cx, cy, a, rng):
        Rr = rng.uniform(*R); g = _disc(cx, cy, Rr, 28)
        grooves = [_ann(cx, cy, Rr - 0.4, Rr - 0.65, 28)]
        if cross: grooves += [_bar((cx - Rr * math.cos(a + d), cy - Rr * math.sin(a + d)), (cx + Rr * math.cos(a + d), cy + Rr * math.sin(a + d)), 0.26) for d in (0, math.pi / 2)]
        return g, [("up", g, 0.45, kind), ("tcut", _u(grooves).difference(_disc(cx, cy, 0.75)), 0, 0.25), ("on", _disc(cx, cy, 0.6, 16), 0, 0.35, boss)]
    return f

def b_nest(rim="twig", egg="egg", n=(2, 4)):
    def f(cx, cy, a, rng, k=1.0):
        R = rng.uniform(2.6, 3.1) * k
        rim_ = star(cx, cy, R, R * 0.9, int(13 * k), a).difference(_disc(cx, cy, R - 1.15 * max(1.0, k ** 0.5), 18))
        eggs = []
        for i in range(rng.randint(*n) + int(3 * (k - 1))):
            t = rng.uniform(0, 6.283); d = rng.uniform(0, max(0.1, R - 2.05 * max(1.0, k ** 0.5)))
            q = affinity.rotate(affinity.scale(_disc(cx + d * math.cos(t), cy + d * math.sin(t), 1, 14), 0.5, 0.66), rng.uniform(0, 180))
            if all(q.distance(e) > 0.2 for e in eggs): eggs.append(q)
        return _disc(cx, cy, R), [("up", rim_, rng.uniform(0.5, 0.7), rim)] + [("mound", e, rng.uniform(0.5, 0.65), egg, PEB_T) for e in eggs]
    return f

def b_crater(rim="rock", pit="pit", R=(2.4, 3.2)):
    def f(cx, cy, a, rng, k=1.0):
        Rr = rng.uniform(*R) * k; outer = stone(cx, cy, Rr, rng, rng.uniform(0.85, 1.0), rng.randint(9, 12)); t = 0.95 * max(1.0, k ** 0.5)
        inner = inset(outer, t)
        if inner.is_empty: return None
        lay = [("up", outer.difference(inner), rng.uniform(0.55, 0.85), rim), ("gcut", inset(inner, 0.3), rng.uniform(0.6, 0.92), pit)]
        for i in range(rng.randint(4, 7) + int(4 * (k - 1))):
            b = rng.uniform(0, 6.283); d = Rr + rng.uniform(0.8, 2.0)
            lay.append(("mound", stone(cx + d * math.cos(b), cy + d * math.sin(b), rng.uniform(0.6, 0.8), rng, 0.85, 6), rng.uniform(0.4, 0.55), rim, PEB_T))
        return _disc(cx, cy, Rr + 2.4, 24), lay
    return f

def b_boardwalk(kind="board", beam="wood", L=(5.5, 8.0)):
    def f(cx, cy, a, rng, k=1.0):
        Ln = rng.uniform(*L) * k; ff = frame(cx, cy, a); miss = rng.randint(0, 9)
        beams = _u([_bar(ff(-Ln / 2, v), ff(Ln / 2, v), 0.92) for v in (-1.35, 1.35)])
        planks = _u([rect(*ff(u, rng.uniform(-0.15, 0.15)), 1.0, rng.uniform(3.5, 4.0), a + rng.uniform(-0.05, 0.05)) for i, u in enumerate(np.arange(-Ln / 2 + 0.6, Ln / 2 - 0.3, 1.22)) if i != miss])
        return rect(cx, cy, Ln, 4.1, a), [("up", beams, 0.35, beam), ("up", planks, 0.7, kind)]
    return f

def b_cart(body="wood", wheel="iron"):
    def f(cx, cy, a, rng):
        L = rng.uniform(3.8, 4.6); W = rng.uniform(2.4, 2.9); ff = frame(cx, cy, a); box_ = rect(cx, cy, L, W, a)
        wheels = _u([rect(*ff(su * (L / 2 - 0.9), sv * (W / 2 + 0.3)), 1.3, 0.95, a) for su in (-1, 1) for sv in (-1, 1)])
        return rect(cx, cy, L, W + 1.6, a), [("up", wheels, 0.95, wheel), ("up", box_, rng.uniform(1.4, 1.8), body), ("tcut", inset(box_, 0.5), 1, 0.92)]
    return f

def b_chest(kind="crate", metal="iron"):
    def f(cx, cy, a, rng):
        L = rng.uniform(3.0, 3.6); W = rng.uniform(2.0, 2.4); ff = frame(cx, cy, a); g = rect(cx, cy, L, W, a)
        bands = _u([_bar(ff(u, -W), ff(u, W), 0.3) for u in (-L / 4, L / 4)] + [inset(g, 0.3).exterior.buffer(0.13)])
        return g, [("up", g, rng.uniform(1.2, 1.5), kind), ("tcut", bands, 0, 0.25), ("on", rect(*ff(0, W / 2 - 0.45), 0.8, 0.55, a), 0, 0.25, metal)]
    return f

def b_tentacle(kind="tendril", L=(6.0, 9.0)):
    def f(cx, cy, a, rng, k=1.0):
        Ln = rng.uniform(*L) * k; n = 14; pts = []; t = a; x, y = cx - Ln / 2 * math.cos(a), cy - Ln / 2 * math.sin(a); curl = rng.choice((-1, 1)) * rng.uniform(0.09, 0.16)
        for i in range(n):
            pts.append((x, y)); t += (curl * (1 + 1.6 * (i / n) ** 2) + rng.uniform(-0.08, 0.08)) * (0.6 if Ln > 11 else 1.0); x += Ln / n * math.cos(t); y += Ln / n * math.sin(t)
        w0 = 0.85 * max(1.0, k ** 0.6)
        g = _u([_cap(pts[i], pts[i + 1], 2 * (w0 - (w0 - 0.47) * i / (n - 1))) for i in range(n - 1)])
        suck = _u([_disc(*pts[i], 0.2, 8) for i in range(1, n - 3, 2)])
        return g.convex_hull if g.geom_type == "Polygon" else None, [("up", g, 0.6 * max(1.0, k ** 0.4), kind), ("tcut", suck, 0, 0.25)]
    return f

def b_cracked_slab(kind="concrete", R=(2.2, 2.9)):
    def f(cx, cy, a, rng):
        Rr = rng.uniform(*R); g = stone(cx, cy, Rr, rng, rng.uniform(0.7, 0.95), rng.randint(5, 8))
        return g, [("up", g, rng.uniform(0.4, 0.55), kind), ("tcut", radial(cx, cy, 0.15, Rr * 1.2, rng.randint(4, 6), rng, 0.24), 0, 0.3)]
    return f

def b_fairy_ring(kind="shroom", R=(2.4, 3.1), h=(0.6, 0.95), tiers=((0.5, 5), (0.5, 50))):
    def f(cx, cy, a, rng, k=1.0):
        Rr = rng.uniform(*R) * k; n = int(rng.randint(7, 10) * k)
        ps = [_disc(cx + Rr * math.cos(a + 6.283 * i / n) + rng.uniform(-0.15, 0.15), cy + Rr * math.sin(a + 6.283 * i / n) + rng.uniform(-0.15, 0.15), rng.uniform(0.42, 0.6), 12) for i in range(n) if rng.random() > 0.12]
        return _ann(cx, cy, Rr + 0.7, max(0.1, Rr - 0.7), 28), [("mound", q, rng.uniform(*h), kind, tiers) for q in ps]
    return f

def b_sign(post="wood", board="board"):
    def f(cx, cy, a, rng):
        ff = frame(cx, cy, a); b = a + rng.uniform(-0.5, 0.5); c = ff(2.0, 0); f2 = frame(c[0], c[1], b)
        arrow = Polygon([f2(-1.5, -0.6), f2(0.9, -0.6), f2(1.6, 0), f2(0.9, 0.6), f2(-1.5, 0.6)])
        return _u([_bar(ff(-3.2, 0), ff(2.0, 0), 0.92), arrow]).convex_hull, [("up", _bar(ff(-3.2, 0), ff(1.2, 0), 0.92), 0.4, post), ("up", arrow, 0.55, board), ("tcut", _bar(f2(-1.2, 0), f2(0.9, 0), 0.24), 1, 0.2)]
    return f

def b_gear(kind="iron", R=(1.6, 2.2)):
    def f(cx, cy, a, rng):
        Rr = rng.uniform(*R); n = rng.randint(8, 11)
        teeth = [rect(cx + (Rr - 0.15) * math.cos(a + 6.283 * i / n), cy + (Rr - 0.15) * math.sin(a + 6.283 * i / n), 0.75, 0.62, a + 6.283 * i / n) for i in range(n)]
        g = _u([_disc(cx, cy, Rr - 0.4, 28)] + teeth).difference(_disc(cx, cy, 0.48, 14))
        return _disc(cx, cy, Rr + 0.25), [("up", g, 0.5, kind), ("tcut", _ann(cx, cy, Rr - 0.75, Rr - 1.0, 24), 0, 0.22)]
    return f

def b_brazier(stand="iron", fire="fire"):
    def f(cx, cy, a, rng):
        base = sh_hex_(cx, cy, rng.uniform(1.95, 2.2), a)
        return base, [("up", base, 0.55, stand), ("on", _ann(cx, cy, 1.5, 0.5, 18), 0, 0.75, stand), ("on", _disc(cx, cy, 0.62, 10), 0, 1.2, fire)]
    return f

def b_tiles(kind="pavers", nx=3, ny=2, size=1.5):
    def f(cx, cy, a, rng):
        L, W = nx * size, ny * size; ff = frame(cx, cy, a); g = rect(cx, cy, L, W, a)
        cx2, cy2 = ff(L / 2, W / 2); g = _biggest(g.difference(stone(cx2, cy2, size * rng.uniform(0.5, 0.9), rng, 0.8, 5)))     # one corner broken away
        if g is None: return None
        joints = _u([_bar(ff(-L / 2 + i * size, -W), ff(-L / 2 + i * size, W), 0.28) for i in range(1, nx)] + [_bar(ff(-L, -W / 2 + j * size), ff(L, -W / 2 + j * size), 0.28) for j in range(1, ny)])
        return g, [("up", g, rng.uniform(0.35, 0.45), kind), ("tcut", joints, 0, 0.28)]
    return f

def b_pile(kind="log", L=(4.0, 5.2), w=1.0, top=True):
    def f(cx, cy, a, rng):
        Ln = rng.uniform(*L); ff = frame(cx, cy, a)
        low = _u([_cap(ff(-Ln / 2 + rng.uniform(-0.3, 0.3), v), ff(Ln / 2 + rng.uniform(-0.3, 0.3), v), w) for v in (-w, 0, w)]).buffer(0.06)
        lay = [("up", low, w * 0.95, kind), ("tcut", _u([_bar(ff(-Ln, v), ff(Ln, v), 0.22) for v in (-w / 2, w / 2)]), 0, 0.3)]
        if top: lay += [("on", _u([_cap(ff(-Ln / 2 + 0.3, v), ff(Ln / 2 - 0.3, v), w * 0.9) for v in (-w / 2, w / 2)]).buffer(0.04), 0, w * 0.85, kind)]
        return low, lay
    return f

def b_hoops(kind="barrel", L=(3.0, 3.8), w=2.0):
    """a barrel or drum lying on its side"""
    def f(cx, cy, a, rng):
        Ln = rng.uniform(*L); ff = frame(cx, cy, a)
        g = Polygon([ff(-Ln / 2, -w * 0.38), ff(-Ln / 4, -w / 2), ff(Ln / 4, -w / 2), ff(Ln / 2, -w * 0.38), ff(Ln / 2, w * 0.38), ff(Ln / 4, w / 2), ff(-Ln / 4, w / 2), ff(-Ln / 2, w * 0.38)])
        hoops = _u([_bar(ff(u, -w), ff(u, w), 0.26) for u in (-Ln / 3, 0, Ln / 3)])
        return g, [("up", g, rng.uniform(1.2, 1.5), kind), ("tcut", hoops, 0, 0.22)]
    return f


# ====================================================================================================== 10 more per theme
def _new():
    C = comp
    return {
    "badlands": [C(b_wheel("wood")), C(b_skull("bone", 1.2)), C(b_ribs("bone")), C(b_campfire("rock", "wood")), C(b_tool("pick")), C(b_sign("wood", "wood")),
                 C(b_crater("sandstone", "pit")), C(b_nest("shrub", "bone")), C(b_roots("wood", "wood")), C(b_cracked_slab("sandstone"))],
    "highland": [C(b_campfire("slate", "wood")), C(b_well("slate", "water")), C(b_tool("sword")), C(b_tool("axe")), C(b_boss("wood", "iron", cross=True)), C(b_roots("wood", "wood")),
                 C(b_fairy_ring("flower", h=(0.45, 0.6), tiers=((1.0, 30),))), C(b_rune_circle("glyph")), C(b_pile("wood")), C(b_stepping_pool("slate", "water"), ground=True)],
    "maahl": [C(b_crystals("crystal")), C(b_fallen_column("column")), C(b_rune_circle("glyph")), C(b_stairs("ruin")), C(b_skull("bone", 1.3)), C(b_ribs("bone")),
              C(b_brazier("ruin", "crystal")), C(b_tiles("pavers")), C(b_tool("spear", "crystal", "ruin")), C(b_crater("rock", "pit"))],
    "corridor": [C(b_gear("metal")), C(b_tool("wrench", "metal")), C(b_grate("metal", True)), C(b_pile("pipe", (3.6, 4.8), 0.9)), C(b_hoops("metal", (2.8, 3.4), 1.8)), C(b_track("metal", "metal", (6.5, 9.0))),
                 C(b_boss("metal", "metal", cross=True)), C(b_cart("crate", "metal")), C(b_tiles("plate", 2, 2, 1.7)), C(b_ladder("metal"))],
    "infestation": [C(b_tentacle("tendril")), C(b_nest("flesh", "egg", (3, 5))), C(b_skull("bone", 1.1)), C(b_ribs("bone")), C(b_fairy_ring("pustule", h=(0.6, 0.9), tiers=SOFT_T)), C(b_crater("flesh", "acid")),
                    C(b_tool("rifle", "metal", "metal")), C(b_hoops("metal", (2.8, 3.4), 1.8)), C(b_crystals("spine", 2.4)), C(b_cracked_slab("carapace"))],
    "urban": [C(b_boss("metal", "metal", (1.7, 2.1), True)), C(b_grate("iron")), C(b_sign("iron", "metal")), C(b_tool("wrench", "iron")), C(b_pile("concrete", (3.6, 4.8), 0.95)), C(b_cracked_slab("concrete")),
              C(b_wheel("rubble", "metal")), C(b_tiles("concrete", 3, 2, 1.4)), C(b_hoops("metal", (3.0, 3.6), 2.0)), C(b_crater("rubble", "pothole"))],
    "mine": [C(b_cart("wood", "iron")), C(b_track("iron", "wood")), C(b_tool("pick")), C(b_tool("shovel")), C(b_ladder("wood")), C(b_pile("wood", (4.2, 5.4), 1.0)),
             C(b_crystals("quartz", 2.4)), C(b_wheel("wood")), C(b_hoops("wood", (3.0, 3.6), 2.0)), C(b_campfire("rock", "wood"))],
    "swamp": [C(b_boardwalk("log", "log")), C(b_stepping_pool("rock", "water"), ground=True), C(b_roots("stump", "root")), C(b_tentacle("root")), C(b_skull("bone", 1.2)), C(b_fairy_ring("shroom")),
              C(b_nest("reed", "egg")), C(b_tool("spear", "iron", "log")), C(b_fallen_column("tflag")), C(b_ribs("bone"))],
    "cynder": [C(b_crystals("obsidian", 2.8)), C(b_crater("basalt", "lava")), C(b_brazier("basalt", "fire")), C(b_skull("bone", 1.2)), C(b_ribs("bone")), C(b_tool("pick")),
               C(b_rune_circle("lava", (2.6, 3.4))), C(b_cracked_slab("basalt")), C(b_track("iron", "basalt", (6.5, 9.0))), C(b_fairy_ring("basalt", h=(0.7, 1.2), tiers=((0.6, 6), (0.4, 35))))],
    "targa": [C(b_crystals("ice", 2.8)), C(b_gear("alien")), C(b_grate("alien", True)), C(b_rune_circle("slot")), C(b_fallen_column("alien", 1.6)), C(b_boss("alien", "crystal", cross=True)),
              C(b_cracked_slab("ice")), C(b_stairs("alien")), C(b_ribs("bone")), C(b_crater("ice", "pit"))],
    "trederra": [C(b_crater("mud", "water")), C(b_tool("rifle")), C(b_tool("shovel")), C(b_pile("iron", (2.6, 3.4), 0.85)), C(b_boardwalk("duck", "wood")), C(b_wheel("wood")),
                 C(b_ladder("wood")), C(b_boss("iron", "iron", (1.2, 1.5))), C(b_cart("wood", "iron")), C(b_sign("wood", "wood"))],
    "fortress": [C(b_tool("sword")), C(b_tool("spear")), C(b_stairs("plank")), C(b_well("garden", "water")), C(b_brazier("iron", "fire")), C(b_stepping_pool("garden", "water"), ground=True),
                 C(b_boss("wood", "iron", cross=True)), C(b_tiles("tatami", 2, 2, 1.8)), C(b_chest("crate")), C(b_hoops("wood", (3.0, 3.6), 2.0))],
    "beast": [C(b_tentacle("vein")), C(b_ribs("bone", (6.0, 8.5))), C(b_skull("bone", 1.3)), C(b_nest("flesh", "egg", (3, 5))), C(b_fairy_ring("pustule", h=(0.6, 0.9), tiers=SOFT_T)), C(b_crater("flesh", "bile")),
              C(b_tool("spear", "bone", "bone")), C(b_crystals("tooth", 2.4)), C(b_tool("anchor")), C(b_chest("crate"))],
    "frontier": [C(b_wheel("wood")), C(b_hoops("barrel")), C(b_well("rock", "water")), C(b_sign("post", "board")), C(b_campfire("rock", "wood")), C(b_tool("rifle")),
                 C(b_boardwalk("board", "post")), C(b_cart("board", "iron")), C(b_ladder("post")), C(b_chest("crate"))],
    "graveyard": [C(b_skull("bone", 1.2)), C(b_ribs("bone")), C(b_tool("shovel")), C(b_cracked_slab("headstone")), C(b_fallen_column("headstone", 1.5)), C(b_well("headstone", "pit")),
                  C(b_roots("earth", "earth")), C(b_fairy_ring("bone", h=(0.4, 0.55), tiers=ROCK_T)), C(b_brazier("iron", "fire")), C(b_tiles("headstone", 2, 2, 1.6))],
    "forest": [C(b_roots("stump", "root")), C(b_fairy_ring("shroom")), C(b_pile("log")), C(b_campfire("rock", "log")), C(b_tool("axe")), C(b_nest("leaf", "shroom")),
               C(b_stepping_pool("rock", "water"), ground=True), C(b_skull("bone", 1.0)), C(b_boardwalk("log", "log")), C(b_well("rock", "water"))],
    "dungeon": [C(b_chest("crate")), C(b_tool("sword")), C(b_skull("bone", 1.1)), C(b_grate("iron")), C(b_stairs("flag")), C(b_brazier("iron", "fire")),
                C(b_rune_circle("glyph")), C(b_hoops("crate")), C(b_boss("iron", "iron", cross=True)), C(b_ribs("bone"))],
    "temple": [C(b_fallen_column("tcolumn")), C(b_stairs("tflag")), C(b_brazier("tcolumn", "gold")), C(b_rune_circle("glyph")), C(b_tiles("tflag", 3, 2, 1.6)), C(b_boss("gold", "gold", cross=True)),
               C(b_well("tcolumn", "water")), C(b_cracked_slab("tflag")), C(b_tool("spear", "gold", "tcolumn")), C(b_chest("tcolumn", "gold"))],
    "coast": [C(b_tool("anchor")), C(b_boardwalk("drift", "drift")), C(b_hoops("drift")), C(b_chest("drift")), C(b_ribs("bone")), C(b_stepping_pool("shelf", "water"), ground=True),
              C(b_nest("weed", "shell")), C(b_tentacle("weed")), C(b_wheel("drift")), C(b_pile("drift"))],
    }


# ====================================================================================================== landmarks (60 mm and up)
def lm(build, r_frac=0.55, gap=0.6, ground=False):
    """a landmark: like comp(), but the builder gets a size factor k that grows with the base"""
    def fn(s, rng, k):
        hold = {}
        def mk(cx, cy):
            got = build(cx, cy, rng.uniform(0, 2 * math.pi), rng, k)
            if got is None or got[0] is None: return None
            hold["g"] = got; return got[0]
        Rr = gg.R0 * r_frac
        at = lambda: (lambda d, t: (d * math.cos(t), d * math.sin(t)))(Rr * math.sqrt(rng.random()), rng.uniform(0, 6.283))
        g, z = find(s, mk, gap=gap, at=at, tries=60, ground=ground)
        if g is None and not terr(s):
            # no clear spot: a landmark matters more than loose stones, so take a spot where only small loose pieces
            # are in the way and remove those
            for _ in range(40):
                g = mk(*at())
                if g is None or not DISC.contains(g): g = None; continue
                gb = g.buffer(gap); blk = [p for p in hard(s) if p.intersects(gb)]
                if any(p.area >= 10 for p in blk) or any(q.intersects(gb) for ps, d, kd in s.gcuts for q in ps) or any(q.intersects(gb) for ps, a_, b_, kd in s.tcuts for q in ps): g = None; continue
                if any(p.area >= 10 and p.intersects(gb) for op in s.ops for p in op[1]): g = None; continue
                s.ops = [o for o in ((op[0], [p for p in op[1] if not p.intersects(gb)]) + tuple(op[2:]) for op in s.ops) if o[1]]
                s.feet = [p for p in s.feet if not p.intersects(gb)]
                s.chops = [c for c in getattr(s, "chops", []) if not c["poly"].intersects(gb)]
                z = H; break
        if g is None: return False
        return _build(s, z, hold["g"][1])
    return fn

def L_wall(kind="ruin", cap=None):
    """a ruined wall: a run of blocks of uneven height with gaps, sometimes with a corner"""
    def f(cx, cy, a, rng, k):
        Ln = rng.uniform(13, 18) * k; W = rng.uniform(1.9, 2.4); lay = []; foot = []
        def run(ox, oy, ang, length):
            ff = frame(ox, oy, ang); u = 0.0
            while u < length - 1.5:
                bl = rng.uniform(2.4, 4.2)
                if rng.random() < 0.16: u += bl * 0.6; continue                     # a gap where the wall has fallen
                bl = min(bl, length - u); q = rect(*ff(u + bl / 2, 0), bl - 0.12, W, ang); foot.append(q)
                lay.append(("up", q, rng.uniform(1.0, 2.6), kind)); i = len(lay) - 1
                lay.append(("tcut", _bar(ff(u + bl / 2, -W), ff(u + bl / 2, W), 0.24), i, 0.22))
                if cap and rng.random() < 0.35: lay.append(("on", rect(*ff(u + bl / 2, 0), bl * 0.6, W * 0.7, ang), i, 0.4, cap))
                u += bl
        ox, oy = cx - Ln / 2 * math.cos(a), cy - Ln / 2 * math.sin(a); run(ox, oy, a, Ln)
        if rng.random() < 0.55:
            ex, ey = cx + Ln / 2 * math.cos(a), cy + Ln / 2 * math.sin(a); run(ex, ey, a + rng.choice((-1, 1)) * math.pi / 2, Ln * rng.uniform(0.4, 0.7))
        if len(foot) < 3: return None
        return _u(foot).buffer(0.3), lay
    return f

def L_track(rail="iron", tie="wood"):
    def f(cx, cy, a, rng, k):
        return b_track(rail, tie, (20, 28))(cx, cy, a, rng, k * 0.75)
    return f

def L_dais(kind="tflag", top_piece="brazier", fire="fire", metal="iron"):
    """a raised platform with steps up one side and something standing on it"""
    def f(cx, cy, a, rng, k):
        R = rng.uniform(5.5, 7.0) * k; n = rng.choice((6, 8)); base = Polygon([(cx + R * math.cos(a + 6.283 * i / n), cy + R * math.sin(a + 6.283 * i / n)) for i in range(n)])
        ff = frame(cx, cy, a + math.pi / n)
        lay = [("up", base, 0.6, kind), ("on", inset(base, 1.1), 0, 0.55, kind), ("tcut", inset(base, 0.45).exterior.buffer(0.14), 0, 0.22)]
        if top_piece == "column":
            lay += [("on", _disc(cx, cy, 1.5 * max(1.0, k ** 0.5), 24), 1, 1.9, kind), ("tcut", _ann(cx, cy, R * 0.55, R * 0.55 - 0.3, 32), 1, 0.25)]
        elif top_piece == "altar":
            lay += [("on", rect(cx, cy, 4.2 * max(1.0, k ** 0.5), 2.2 * max(1.0, k ** 0.5), a), 1, 1.3, kind), ("tcut", _ann(cx, cy, R * 0.6, R * 0.6 - 0.3, 32), 1, 0.25)]
        else:
            lay += [("on", _ann(cx, cy, 1.9 * max(1.0, k ** 0.5), 1.1 * max(1.0, k ** 0.5), 20), 1, 0.9, metal), ("on", star(cx, cy, 0.95 * max(1.0, k ** 0.5), 0.5 * max(1.0, k ** 0.5), 6, a), 1, 1.5, fire)]
        steps = [("up", rect(*ff(R * math.cos(math.pi / n) + 0.9, 0), 2.0, 4.2, a + math.pi / n), 0.35, kind)]
        return base.buffer(1.6), lay + steps
    return f

def L_river(kind="water", bank="rock", bridge="board"):
    """a stream right across the base with stones on its banks and a plank bridge over it"""
    def f(cx, cy, a, rng, k):
        R = gg.R0 - 1.2; ff = frame(0, 0, a); off = rng.uniform(-0.3, 0.3) * R; amp = rng.uniform(1.5, 3.5) * k; w = rng.uniform(1.6, 2.4) * max(1.0, k ** 0.5)
        ph = rng.uniform(0, 6.283); kk = rng.uniform(0.12, 0.22) / max(1.0, k ** 0.5)
        pts = [ff(u, off + amp * math.sin(kk * u + ph)) for u in np.linspace(-R - 2, R + 2, int(40 * k) + 20)]
        ch = LineString(pts).buffer(w, cap_style=2, join_style=1).intersection(Point(0, 0).buffer(R - 0.6, 64))
        ch = _biggest(ch)
        if ch is None: return None
        lay = [("gcut", ch, rng.uniform(0.5, 0.7), kind)]
        for i in range(int(10 * k * k)):
            j = rng.randrange(len(pts) - 1); px, py = pts[j]; t = math.atan2(pts[j + 1][1] - py, pts[j + 1][0] - px) + rng.choice((-1, 1)) * math.pi / 2; d = w + rng.uniform(0.9, 1.6)
            q = stone(px + d * math.cos(t), py + d * math.sin(t), rng.uniform(0.6, 1.0), rng, 0.85, rng.randint(5, 7))
            if q.distance(ch) > 0.35 and Point(0, 0).buffer(R - 1.0).contains(q): lay.append(("mound", q, rng.uniform(0.45, 0.7), bank, PEB_T))
        return ch.buffer(0.2), lay
    return f

def L_skeleton(kind="bone"):
    return _sc().L_skeleton(kind)          # the rounded version; the flat one below is kept for reference
    """the skeleton of something huge: a long spine, great ribs, a skull at one end"""
    def f(cx, cy, a, rng, k):
        foot, lay = b_ribs(kind, (13, 17))(cx, cy, a, rng, k * 0.9)
        Ln = 15 * k * 0.9; ff = frame(cx, cy, a); hx, hy = ff(Ln / 2 + 2.2 * k ** 0.6, 0)
        sf, sl = b_skull(kind, 1.9 * max(1.0, k ** 0.6))(hx, hy, a, rng)
        n0 = len(lay); sl = [(l[0], l[1], l[2] + n0, l[3]) if l[0] == "tcut" else l for l in sl]
        return _u([foot, sf]).convex_hull, lay + sl
    return f

def L_crater(rim="rock", pit="pit"):
    def f(cx, cy, a, rng, k):
        return b_crater(rim, pit, (5.0, 6.5))(cx, cy, a, rng, k)
    return f

def L_colonnade(kind="tcolumn", floor="tflag"):
    """two rows of columns with a paved way between them; some columns are broken stumps"""
    def f(cx, cy, a, rng, k):
        n = int(3 + 2 * k); gapu = 4.6; Ln = n * gapu; ff = frame(cx, cy, a); half = 3.4 * max(1.0, k ** 0.4)
        way = rect(cx, cy, Ln + 1.0, 2 * half - 2.6, a); lay = [("up", way, 0.35, floor)]
        joints = _u([_bar(ff(-Ln / 2 + i * 2.3, -half), ff(-Ln / 2 + i * 2.3, half), 0.26) for i in range(1, int(Ln / 2.3) + 1)] + [_bar(ff(-Ln, 0), ff(Ln, 0), 0.26)])
        lay.append(("tcut", joints, 0, 0.25)); foot = [way]
        for i in range(n):
            for sv in (-1, 1):
                c = ff(-Ln / 2 + (i + 0.5) * gapu, sv * half); pl = rect(c[0], c[1], 2.3, 2.3, a); foot.append(pl)
                lay.append(("up", pl, 0.5, kind)); j = len(lay) - 1
                lay.append(("on", _disc(c[0], c[1], 0.92, 18), j, rng.choice((0.5, 0.9, 1.6, 2.4, 2.7)), kind))
        return _u(foot).convex_hull, lay
    return f

def L_hull(kind="hull", rib="hull"):
    """the bones of a wrecked boat / wagon / pod: a keel, curved ribs and a few planks still on"""
    def f(cx, cy, a, rng, k):
        Ln = rng.uniform(15, 19) * k; Wd = rng.uniform(4.2, 5.2) * max(1.0, k ** 0.7); ff = frame(cx, cy, a)
        def edge(sv, shrink=0.0): return [ff(u, sv * (Wd - shrink) * (1 - (2 * u / Ln) ** 2) ** 0.6) for u in np.linspace(-Ln / 2 * 0.98, Ln / 2 * 0.98, 21)]
        outline = Polygon(edge(1) + edge(-1)[::-1]).buffer(0)
        wale = outline.difference(inset(outline, 1.0))
        keel = _bar(ff(-Ln / 2 + 0.5, 0), ff(Ln / 2 - 0.5, 0), 1.0)
        ribs = _u([_bar(ff(u, -Wd), ff(u, Wd), 0.92) for u in np.arange(-Ln / 2 + 2.0, Ln / 2 - 1.5, 2.4 * max(1.0, k ** 0.4))]).intersection(outline)
        side = rng.choice((-1, 1)); gone = rect(*ff(rng.uniform(-Ln / 4, Ln / 4), side * Wd), Ln * rng.uniform(0.3, 0.5), Wd * 1.3, a)      # one side stove in
        wale = wale.difference(gone); ribs = ribs.difference(gone.buffer(-0.3))
        lay = [("up", ribs, 0.55, rib), ("up", keel, 0.8, kind), ("up", wale, rng.uniform(1.3, 1.9), kind)]
        return outline.buffer(0.3), lay
    return f

def L_tree(kind="stump", root="root"):
    def f(cx, cy, a, rng, k):
        return b_roots(kind, root, (2.6, 3.4))(cx, cy, a, rng, k)
    return f

def L_circle(kind="glyph", post="tcolumn"):
    """a great ritual circle cut into the ground with standing stones or posts round it"""
    def f(cx, cy, a, rng, k):
        foot, lay = b_rune_circle(kind, (6.0, 7.5))(cx, cy, a, rng, k)
        R = foot.bounds[2] - foot.centroid.x; n = rng.choice((5, 6, 7, 8))
        for i in range(n):
            t = a + 6.283 * i / n + 0.3; q = stone(cx + (R + 1.3) * math.cos(t), cy + (R + 1.3) * math.sin(t), rng.uniform(0.85, 1.15), rng, 0.8, rng.randint(4, 6))
            lay.append(("up", q, rng.uniform(1.4, 2.6), post))
        return _disc(cx, cy, R + 2.5, 32), lay
    return f

def L_ruin(kind="ruin", floor="pavers"):
    """the footprint of a ruined building: a tiled floor with broken walls round two or three sides"""
    def f(cx, cy, a, rng, k):
        L = rng.uniform(11, 14) * k; W = rng.uniform(8, 10.5) * k; ff = frame(cx, cy, a); fl = rect(cx, cy, L, W, a); t = 1.7
        lay = [("up", fl, 0.35, floor)]
        js = 2.2 * max(1.0, k ** 0.3)
        joints = _u([_bar(ff(u, -W), ff(u, W), 0.26) for u in np.arange(-L / 2 + js, L / 2 - 0.5, js)] + [_bar(ff(-L, v), ff(L, v), 0.26) for v in np.arange(-W / 2 + js, W / 2 - 0.5, js)])
        lay.append(("tcut", joints.intersection(inset(fl, t + 0.3)), 0, 0.25))
        sides = rng.sample(range(4), rng.choice((2, 3)))
        for sd in sides:
            if sd in (0, 1): p0, p1 = ff(-L / 2 + t / 2, (-1 if sd == 0 else 1) * (W / 2 - t / 2)), ff(L / 2 - t / 2, (-1 if sd == 0 else 1) * (W / 2 - t / 2))
            else: p0, p1 = ff((-1 if sd == 2 else 1) * (L / 2 - t / 2), -W / 2 + t / 2), ff((-1 if sd == 2 else 1) * (L / 2 - t / 2), W / 2 - t / 2)
            length = math.dist(p0, p1); ang = math.atan2(p1[1] - p0[1], p1[0] - p0[0]); f2 = frame(p0[0], p0[1], ang); u = 0.0
            door = rng.uniform(0.3, 0.7) * length
            while u < length - 1.2:
                bl = min(rng.uniform(2.2, 3.8), length - u)
                if abs(u + bl / 2 - door) < 1.6: u += bl; continue           # the doorway
                lay.append(("on", rect(*f2(u + bl / 2, 0), bl - 0.1, t - 0.35, ang), 0, rng.uniform(0.7, 2.6), kind)); u += bl
        return fl, lay
    return f

def L_pool(stone_kind="rock", water="water"):
    def f(cx, cy, a, rng, k):
        return b_stepping_pool(stone_kind, water)(cx, cy, a, rng, 1.9 * k)
    return f

def L_boardwalk(kind="board", beam="wood"):
    def f(cx, cy, a, rng, k):
        return b_boardwalk(kind, beam, (15, 20))(cx, cy, a, rng, k * 0.85)
    return f

def L_nest(rim="twig", egg="egg"):
    def f(cx, cy, a, rng, k):
        return b_nest(rim, egg, (4, 6))(cx, cy, a, rng, 2.2 * max(1.0, k ** 0.7))
    return f

def L_spires(kind="crystal"):
    def f(cx, cy, a, rng, k):
        return b_crystals(kind, 3.2)(cx, cy, a, rng, 2.0 * max(1.0, k ** 0.6))
    return f

def L_tentacles(kind="tendril", pit="pit"):
    """a pit with great tentacles coming out of it"""
    def f(cx, cy, a, rng, k):
        R = rng.uniform(2.6, 3.4) * max(1.0, k ** 0.6); hole = stone(cx, cy, R, rng, 0.9, 10); lay = [("gcut", hole, 0.8, pit)]; foot = [hole.buffer(0.5)]
        for i in range(rng.randint(3, 5)):
            t = a + 6.283 * i / 4 + rng.uniform(-0.3, 0.3); Ln = rng.uniform(7, 10) * k; ox, oy = cx + (R + 0.5 + Ln / 2) * math.cos(t), cy + (R + 0.5 + Ln / 2) * math.sin(t)
            got = b_tentacle(kind, (Ln, Ln))(ox, oy, t, rng, 1.0)
            if got and got[0] is not None:
                n0 = len(lay); foot.append(got[0]); lay += [(l[0], l[1], l[2] + n0, l[3]) if l[0] == "tcut" else l for l in got[1]]
        return _u(foot).convex_hull, lay
    return f


def _landmarks():
    """per theme: (smallest base in mm, landmark). Bigger bases unlock more kinds."""
    return {
    "badlands": [(60, lm(L_skeleton("bone"))), (60, lm(L_crater("sandstone", "pit"))), (60, lm(L_track("iron", "wood"), 0.35)), (100, lm(L_river("water", "rock"), 0.2, ground=True)), (100, lm(L_hull("wood", "wood"))), (130, lm(L_ruin("sandstone", "sandstone")))],
    "highland": [(60, lm(L_circle("glyph", "standing"))), (60, lm(L_wall("slate"))), (60, lm(L_tree("wood", "wood"))), (100, lm(L_river("water", "slate"), 0.2, ground=True)), (100, lm(L_ruin("slate", "slate"))), (130, lm(L_dais("slate", "column")))],
    "maahl": [(60, lm(L_spires("crystal"))), (60, lm(L_wall("ruin"))), (60, lm(L_dais("ruin", "altar"))), (100, lm(L_colonnade("column", "pavers"))), (100, lm(L_skeleton("bone"))), (130, lm(L_ruin("ruin", "pavers")))],
    "corridor": [(60, lm(L_track("metal", "metal"), 0.35)), (60, lm(L_wall("metal", "pipe"))), (60, lm(L_dais("plate", "column"))), (100, lm(L_hull("metal", "metal"))), (100, lm(L_ruin("metal", "plate"))), (130, lm(L_colonnade("metal", "plate")))],
    "infestation": [(60, lm(L_tentacles("tendril", "acid"))), (60, lm(L_nest("flesh", "egg"))), (60, lm(L_skeleton("bone"))), (100, lm(L_river("acid", "pustule"), 0.2, ground=True)), (100, lm(L_hull("carapace", "spine"))), (130, lm(L_spires("spine")))],
    "urban": [(60, lm(L_wall("brick", "concrete"))), (60, lm(L_crater("rubble", "pothole"))), (60, lm(L_track("iron", "concrete"), 0.35)), (100, lm(L_ruin("brick", "concrete"))), (100, lm(L_hull("metal", "iron"))), (130, lm(L_colonnade("concrete", "concrete")))],
    "mine": [(60, lm(L_track("iron", "wood"), 0.35)), (60, lm(L_spires("quartz"))), (60, lm(L_boardwalk("wood", "wood"))), (100, lm(L_ruin("wood", "wood"))), (100, lm(L_river("water", "rock"), 0.2, ground=True)), (130, lm(L_skeleton("bone")))],
    "swamp": [(60, lm(L_boardwalk("log", "log"))), (60, lm(L_tree("stump", "root"))), (60, lm(L_pool("rock", "water"), ground=True)), (100, lm(L_hull("log", "log"))), (100, lm(L_tentacles("root", "water"))), (130, lm(L_ruin("tflag", "tflag")))],
    "cynder": [(60, lm(L_crater("basalt", "lava"))), (60, lm(L_spires("obsidian"))), (60, lm(L_circle("lava", "basalt"))), (100, lm(L_river("lava", "basalt"), 0.2, ground=True)), (100, lm(L_skeleton("bone"))), (130, lm(L_dais("basalt", "brazier", "fire", "iron")))],
    "targa": [(60, lm(L_spires("ice"))), (60, lm(L_circle("slot", "alien"))), (60, lm(L_wall("alien", "crystal"))), (100, lm(L_hull("alien", "alien"))), (100, lm(L_colonnade("alien", "alien"))), (130, lm(L_dais("alien", "column")))],
    "trederra": [(60, lm(L_crater("mud", "water"))), (60, lm(L_boardwalk("duck", "wood"))), (60, lm(L_wall("sandbag"))), (100, lm(L_track("iron", "wood"), 0.35)), (100, lm(L_ruin("brick", "duck"))), (130, lm(L_hull("iron", "iron")))],
    "fortress": [(60, lm(L_dais("plank", "brazier", "fire", "iron"))), (60, lm(L_pool("garden", "water"), ground=True)), (60, lm(L_wall("garden"))), (100, lm(L_ruin("wood", "tatami"))), (100, lm(L_colonnade("wood", "plank"))), (130, lm(L_river("water", "garden"), 0.2, ground=True))],
    "beast": [(60, lm(L_skeleton("bone"))), (60, lm(L_tentacles("vein", "bile"))), (60, lm(L_nest("flesh", "egg"))), (100, lm(L_river("bile", "pustule"), 0.2, ground=True)), (100, lm(L_hull("bone", "bone"))), (130, lm(L_spires("tooth")))],
    "frontier": [(60, lm(L_track("iron", "post"), 0.35)), (60, lm(L_boardwalk("board", "post"))), (60, lm(L_hull("board", "post"))), (100, lm(L_ruin("board", "board"))), (100, lm(L_river("water", "rock"), 0.2, ground=True)), (130, lm(L_skeleton("bone")))],
    "graveyard": [(60, lm(L_wall("headstone"))), (60, lm(L_tree("earth", "earth"))), (60, lm(L_circle("glyph", "headstone"))), (100, lm(L_ruin("headstone", "headstone"))), (100, lm(L_skeleton("bone"))), (130, lm(L_colonnade("headstone", "headstone")))],
    "forest": [(60, lm(L_tree("stump", "root"))), (60, lm(L_pool("rock", "water"), ground=True)), (60, lm(L_circle("glyph", "rock"))), (100, lm(L_river("water", "rock"), 0.2, ground=True)), (100, lm(L_boardwalk("log", "log"))), (130, lm(L_ruin("rock", "rock")))],
    "dungeon": [(60, lm(L_circle("glyph", "flag"))), (60, lm(L_dais("flag", "altar"))), (60, lm(L_wall("flag"))), (100, lm(L_colonnade("flag", "flag"))), (100, lm(L_skeleton("bone"))), (130, lm(L_river("water", "flag"), 0.2, ground=True))],
    "temple": [(60, lm(L_dais("tflag", "brazier", "gold", "tcolumn"))), (60, lm(L_colonnade("tcolumn", "tflag"))), (60, lm(L_circle("glyph", "tcolumn"))), (100, lm(L_ruin("tcolumn", "tflag"))), (100, lm(L_pool("tcolumn", "water"), ground=True)), (130, lm(L_wall("tcolumn", "gold")))],
    "coast": [(60, lm(L_hull("drift", "drift"))), (60, lm(L_pool("shelf", "water"), ground=True)), (60, lm(L_boardwalk("drift", "drift"))), (100, lm(L_skeleton("bone"))), (100, lm(L_river("water", "shelf"), 0.2, ground=True)), (130, lm(L_tentacles("weed", "water")))],
    }


def _tag_builders():
    for nm, f in list(globals().items()):
        if nm.startswith("b_") and callable(f) and not getattr(f, "_w", False):
            def w(*a, _f=f, _n=nm[2:], **k):
                g = _f(*a, **k); g.tag = _n + (":" + str(a[0]) if _n == "tool" and a else ""); g.kind = (k.get("kind") or (a[0] if a and isinstance(a[0], str) and _n != "tool" else "")) or (a[1] if _n == "tool" and len(a) > 1 else "iron" if _n == "tool" else ""); return g
            w._w = True; globals()[nm] = w
_tag_builders()

NEW = {}; LANDMARKS = {}

def extend(details_mod):
    """add the new details to each theme's pool (once) and hand the landmark table to details.anchors"""
    if NEW: return
    NEW.update(_new()); LANDMARKS.update(_landmarks())
    for st, lst in NEW.items():
        if st in details_mod.DETAILS: details_mod.DETAILS[st] = list(details_mod.DETAILS[st]) + lst


def landmarks(s, style, rng):
    """place the landmarks for this base size before anything else needs the room: one on a 60 mm base, two at 100,
    three at 130 and four at 160, each a different kind"""
    mm = gg.BASE_MM
    pool = [fn for lo, fn in LANDMARKS.get(style, []) if mm >= lo - 0.01]
    if not pool: return 0
    want = 1 if mm < 80 else 2 if mm <= 100 else 3 if mm <= 130 else 4
    k = size_factor(mm)
    rng.shuffle(pool); placed = 0
    for fn in pool:
        if placed >= want: break
        for kk in (k, 0.8 * k, 0.62 * k):                   # full size first; a little smaller if the base is already too full for it
            try:
                if fn(s, rng, kk): placed += 1; break
            except Exception as e: print("  ! landmark", style, type(e).__name__, str(e)[:80], flush=True); break
    return placed

def true_scale():
    """the scenery scale in use (1.0 = none). With one, objects are sized for the miniature and must be the same size
    on every base, so nothing may grow with the base any more."""
    import os
    try: return float(os.environ.get("KIT_SCALE", 1) or 1)
    except ValueError: return 1.0

def size_factor(mm):
    """how much bigger than its 30 mm drawing a landmark is: it should span roughly a third to a half of the base.
    With a scenery scale it is one size on every base (a well is a well's size)."""
    if true_scale() != 1.0: return 1.6
    return 1.6 if mm < 80 else 2.1 if mm <= 100 else 2.5 if mm <= 130 else 2.9


_OWN = set(globals())
