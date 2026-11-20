"""Scenes: every base is one place. A subject, a way to it, things that belong to them, clutter where it would
gather, and open ground on purpose. See PLAN_SCENES.md.

compose() is called by gg3.make() after the style has laid out its ground. It takes a plan (a small dict: theme,
subject, path, props, clutter, open), places the subject, lays the path, holds the yard open and then runs the kit's
ordinary details under a "focus": gg3.FOCUS decides where a detail's random spot is, gg3.AIM how it is turned.
The plan comes from the theme table (themes.py), from a file (--plan) or from a local model (designer.py); all three
go through validate_plan(), so they are built the same way."""
import shapely
import math, os, json, random as _random
from shapely.affinity import rotate as _arot, scale as _ascale

def install(G):
    globals().update({k: v for k, v in G.items() if not k.startswith("__") and k not in _OWN})
    global F, D, S2
    import features as F, details as D, styles2 as S2
    for k, v in {"sack": "#b8a47a", "sail": "#d9d1b8", "fire": "#e2792b", "twig": "#7b5a36", "rope": "#b59b63", "chain": "#4a4c50", "gold": "#d4a72c",
                 "earth": "#5a4634", "vein": "#5e2238", "root": "#4a3622", "egg": "#c7b37a", "cable": "#3c3f45", "pipe": "#6f747c", "sandbag": "#a8946a",
                 "stump": "#7a5a3a", "clay": "#a8623c", "tendril": "#7a3b4a", "shroom": "#c9a27a", "tooth": "#e8dcc0", "fence": "#33363a", "standing": "#80858b"}.items():
        gg.COL.setdefault(k, v)
    _tag_all()


# ====================================================================================================== materials
def _P(wood="wood", iron="iron", rock="rock", stone="rock", crate="crate", water="water", pit="pit", gem=None, floor=None, plant=None, soft=None, bone="bone", **kw):
    d = dict(wood=wood, iron=iron, rock=rock, stone=stone, crate=crate, water=water, pit=pit, gem=gem, floor=floor, plant=plant, soft=soft, bone=bone, twig="twig", vein="vein")
    d.update(kw); return d

PAL = {     # which material plays which part in each style, so one builder serves all 19
    "badlands": _P(stone="sandstone", floor="sandstone", plant="shrub"),
    "highland": _P(rock="slate", stone="slate", floor="slate", plant="heather", soft="moss"),
    "maahl": _P(stone="ruin", gem="crystal", floor="pavers", plant="shrub"),
    "corridor": _P(wood="metal", iron="metal", rock=None, stone="metal", water="coolant", pit="slot", floor="plate"),
    "infestation": _P(wood="metal", iron="metal", stone="carapace", water="acid", gem="spine", floor="plate", soft="flesh", vein="tendril"),
    "urban": _P(rock="rubble", stone="concrete", pit="pothole", floor="concrete"),
    "mine": _P(gem="quartz", floor="wood"),
    "swamp": _P(wood="log", stone="tflag", floor="tflag", plant="reed", soft="mud", vein="root", twig="reed"),
    "cynder": _P(rock="basalt", stone="basalt", water="lava", gem="obsidian", floor="basalt"),
    "targa": _P(wood="alien", iron="metal", rock="ice", stone="alien", gem="crystal", floor="alien", soft="snow"),
    "trederra": _P(stone="brick", floor="duck", soft="mud"),
    "fortress": _P(rock="garden", stone="garden", floor="plank", plant="shrub"),
    "beast": _P(wood="bone", rock="flesh", stone="tooth", water="bile", gem="tooth", floor="fleshfloor", soft="flesh", twig="flesh"),
    "frontier": _P(floor="board", plant="shrub"),
    "graveyard": _P(iron="fence", stone="headstone", crate="coffin", floor="headstone", plant="shrub", soft="earth"),
    "forest": _P(wood="log", floor="log", plant="shrub", soft="moss", vein="root"),
    "dungeon": _P(rock="flag", stone="flag", floor="flag"),
    "temple": _P(iron="gold", rock="tcolumn", stone="tcolumn", gem="gold", floor="tflag", plant="shrub"),
    "coast": _P(wood="drift", rock="shelf", stone="shelf", floor="drift", plant="weed", twig="weed"),
}
PATH_OK = {     # the ways that suit each style's ground (the first is the fallback)
    "badlands": "trail ruts track stream stones", "highland": "trail stones stream ruts", "maahl": "trail way stones ruts",
    "corridor": "way pipe cable track", "infestation": "veins cable stream pipe", "urban": "way cable pipe track ruts",
    "mine": "track trail ruts cable boards stream", "swamp": "boards stones stream veins trail", "cynder": "trail way stream track stones",
    "targa": "way pipe cable stones trail", "trederra": "boards trail ruts track stream", "fortress": "stones way stream boards",
    "beast": "veins stream", "frontier": "ruts trail boards track stream", "graveyard": "trail stones way",
    "forest": "trail stones stream boards veins", "dungeon": "way stones stream", "temple": "way stones stream", "coast": "trail boards stones stream",
}
STYLE_SAYS = {
    "badlands": "dry red rock country", "highland": "wet moor and slate", "maahl": "a desert with buried ruins and crystals",
    "corridor": "the deck of a spaceship", "infestation": "a ship overrun by an alien growth", "urban": "a ruined city street",
    "mine": "an 1800s mine dug by pick and powder", "swamp": "a swamp with a sunken serpent temple", "cynder": "volcanic caverns with lava",
    "targa": "a frozen plateau with an alien city under the ice", "trederra": "a trench war in mud", "fortress": "a feudal Japanese castle and its garden",
    "beast": "the inside of a gigantic living creature", "frontier": "a wild west town", "graveyard": "an old graveyard",
    "forest": "a forest floor", "dungeon": "a stone dungeon", "temple": "an ancient temple", "coast": "a rocky sea shore",
}
# What stands on the base. Sizes are shares of the base diameter. pads: where it touches the base (kept flat and clear);
# each is (forward, sideways, radius) or (forward, sideways, half length, half width) for a long contact.
# reach: how far its body spreads beyond the pads (tall things keep out of that). says: what the designer model is told.
MINIS = {
    "biped":   dict(pads=[(0, -0.11, 0.07), (0, 0.11, 0.07)], reach=0.05, says="a figure on two legs. It needs two flat, clear foot spots side by side"),
    "wide":    dict(pads=[(0, -0.2, 0.085), (0, 0.2, 0.085)], reach=0.07, says="a very fat or very broad figure on two legs set far apart. It needs two big flat foot spots wide apart, and its belly hangs low between them"),
    "walker":  dict(pads=[(0, -0.14, 0.085, 0.065), (0, 0.14, 0.085, 0.065)], reach=0.06, says="a two-legged robot or walker with big flat feet. It needs two large flat foot plates"),
    "tripod":  dict(pads=[(0.17, 0, 0.06), (-0.085, -0.147, 0.06), (-0.085, 0.147, 0.06)], reach=0.05, says="a creature or machine on three legs. It needs three flat foot spots in a triangle; low things may sit between them"),
    "quad":    dict(pads=[(0.16, -0.1, 0.055), (0.16, 0.1, 0.055), (-0.16, -0.1, 0.055), (-0.16, 0.1, 0.055)], reach=0.05, says="a four-legged beast (horse, wolf, bear). It needs four flat foot spots in a long rectangle; low things may pass under its belly"),
    "many":    dict(pads=[(0.24 * math.cos(t), 0.17 * math.sin(t), 0.035) for t in (0.45, 1.15, 1.99, 2.69, 3.59, 4.29, 5.13, 5.83)], reach=0.04,
                    says="a creature on many legs (spider, insect, crab). It needs eight small flat spots in a ring; its body hangs over the middle, so only low things go there"),
    "scorpion": dict(pads=[(0.2 * math.cos(t), 0.15 * math.sin(t), 0.035) for t in (0.6, 1.25, 1.89, 2.54, 3.74, 4.39, 5.03, 5.68)] + [(0.3, -0.13, 0.045), (0.3, 0.13, 0.045)], reach=0.05,
                     says="a giant scorpion: eight legs in a ring, two claws reaching forward, a tail arched over its back. It needs small flat spots for the legs and room in front for the claws"),
    "crawler": dict(pads=[(0, 0, 0.25, 0.075)], reach=0.05, says="a long low animal on its belly (crocodile, lizard, snake, slug). It needs one long flat strip to lie on; nothing may stick up under it"),
    "tracked": dict(pads=[(0, -0.12, 0.18, 0.045), (0, 0.12, 0.18, 0.045)], reach=0.04, says="a tracked or wheeled machine (tank, crawler robot). It needs two long flat strips side by side for its tracks"),
    "hover":   dict(pads=[(0, 0, 0.045)], reach=0.0, says="a flying or hovering figure on a thin flight stem. It only needs one small flat spot for the stem, so the rest of the base can be as wild, tall and crowded as you like"),
    "none":    dict(pads=[], reach=0.0, says="nothing in particular"),
}
MINI_ALIAS = {"fat": "wide", "broad": "wide", "two": "biped", "2": "biped", "human": "biped", "robot": "walker", "mech": "walker", "three": "tripod", "3": "tripod", "four": "quad", "4": "quad",
              "beast": "quad", "horse": "quad", "spider": "many", "insect": "many", "crab": "many", "legs": "many", "crocodile": "crawler", "croc": "crawler", "snake": "crawler", "lizard": "crawler",
              "slug": "crawler", "tank": "tracked", "wheels": "tracked", "flying": "hover", "fly": "hover", "flyer": "hover", "hovering": "hover", "off": "none"}

def mini_type(rng=None):
    """the kind of miniature this base is for: KIT_MINI / --mini, else "miniature" in settings.json, else biped. "mix" picks one per base."""
    k = str(os.environ.get("KIT_MINI") or CFG.get("miniature", "biped")).strip().lower(); k = MINI_ALIAS.get(k, k)
    if k in ("mix", "random", "any"): return (rng or _random).choice([m for m in MINIS if m != "none"])
    if k not in MINIS: raise SystemExit(f"unknown miniature type '{k}'. Types: " + ", ".join(MINIS) + ", mix. Also understood: " + ", ".join(sorted(MINI_ALIAS)))
    return k

def stand_shapes(kind, cx, cy, a):
    """the contact pads of a miniature standing at (cx, cy) facing angle a, and the body outline round them"""
    D = 2 * gg.R0; f = frame(cx, cy, a); pads = []
    for p in MINIS[kind]["pads"]:
        if len(p) == 3: pads.append(Point(*f(p[0] * D, p[1] * D)).buffer(max(2.2, p[2] * D), 20))
        else: pads.append(rect(*f(p[0] * D, p[1] * D), 2 * p[2] * D, 2 * p[3] * D, a).buffer(1.0, 6))
    body = unary_union(pads).convex_hull.buffer(MINIS[kind]["reach"] * D + 0.5, 12) if pads else Polygon()
    return pads, body

EDGE_OK = ("portal", "wall", "hull", "ruin")
TALL = {"portal", "wall", "ruin", "colonnade", "statue", "spires", "dais_brazier", "dais_column", "dais_altar", "machine"}      # things that stand up; a log, a wreck or a camp stays low
ALSO_OK = {"camp", "depot", "shaft", "graves", "well", "tent", "statue", "crater", "nest", "pool", "tree", "machine", "wagon"}      # compact things: a second feature never outweighs the subject
RELATIONS = ("at", "wall", "front", "back", "path", "start", "yard", "rim", "also", "far", "in")
TERRK = {"dune", "bedrock"}
WETK = {"water", "lava", "bile", "acid", "wash", "tar", "coolant"}

def kfix(kind):
    """a scene part made of a floor material or of a loose-piece material gets its own name, so the kit neither stands
    things on it as a floor nor prunes it as a loose stone when it reaches the rim"""
    if kind in BRIDGE: k2 = kind + "_p"
    elif kind in gg.PIECES: k2 = kind + "_s"
    else: return kind
    gg.COL.setdefault(k2, gg.COL.get(kind, "#999999"))
    if kind in gg.CHAMFER_PRISMS: gg.CHAMFER_PRISMS.add(k2)          # stone keeps its bevelled top edge under its scene name
    return k2


# ====================================================================================================== small helpers
def _u(gs):
    gs = [x for x in gs if x is not None and not x.is_empty]
    return unary_union(gs) if gs else Polygon()
def _disc(x, y, r, n=20): return Point(x, y).buffer(r, n)
def _ann(x, y, ro, ri, n=24): return Point(x, y).buffer(ro, n).difference(Point(x, y).buffer(ri, n))
def _cap(p, q, w): return LineString([p, q]).buffer(w / 2, 8)
def _bar(p, q, w): return LineString([p, q]).buffer(w / 2, cap_style=2)
def _kk(k, e=0.5): return max(1.0, k) ** e
def _unit(dx, dy):
    L = math.hypot(dx, dy) or 1.0
    return dx / L, dy / L
def _join(lay, more):
    """append another builder's layers, moving its layer numbers along"""
    n0 = len(lay)
    for l in more: lay.append((l[0], l[1], l[2] + n0) + tuple(l[3:]) if l[0] in ("on", "tcut") else l)
def _turn(build, da):
    return lambda cx, cy, a, rng, k: build(cx, cy, a + da, rng, k)
def _solid_of(layers, amin=0.0):
    return _u([q for l in layers if l[0] in ("up", "mound", "gcut", "steps") for q in polys_of(l[1]) if q.area >= amin])

SACK_T = ((0.4, 18), (0.35, 40), (0.25, 62)); SOFT_T = ((0.55, 25), (0.45, 50)); ROCK_T = ((0.7, 6), (0.3, 40)); PEB_T = ((0.7, 10), (0.3, 45)); BAG_T = ((0.7, 8), (0.3, 40))


# ====================================================================================================== new subjects
# builder(cx, cy, a, rng, k) -> (footprint, layers). +u (angle a) is the front: the side the path arrives at.
def S_portal(p):
    """a doorway on the rim: threshold, two jambs, the dark way in running off the base, wall blocks along the rim"""
    st, fl = kfix(p["stone"]), p["floor"] or p["stone"]
    def f(cx, cy, a, rng, k):
        ff = frame(cx, cy, a); W = rng.uniform(6.0, 7.4) * _kk(k, 0.8); T = 2.6 * _kk(k); Dp = 3.6 * _kk(k); far = 14 * _kk(k)
        sill = rect(*ff(Dp / 2 + 0.7, 0), 1.8, W + 1.4, a)
        dark = Polygon([ff(-far, -W / 2 + 0.15), ff(Dp / 2 - 0.5, -W / 2 + 0.15), ff(Dp / 2 - 0.5, W / 2 - 0.15), ff(-far, W / 2 - 0.15)])
        lay = [("gcut", dark, 0.85, p["pit"]), ("up", sill, 0.45, fl)]; foot = [sill]
        if SUPPORTS():          # a real doorway: two jambs, a lintel across them, a keystone, and nothing under it
            hj = rng.uniform(2.9, 3.4); js = [rect(*ff(0, sv * (W / 2 + T / 2)), Dp, T, a) for sv in (-1, 1)]; foot += js
            lin = rect(*ff(0, 0), Dp * 0.92, W + 2 * T + 0.6, a); key = rect(*ff(0, 0), Dp * 1.04, T * 0.7, a)
            lay.append(_steps([(j, hj) for j in js] + slab(lin, hj - 0.25, hj + 1.0) + slab(key, hj - 0.55, hj + 1.35), st))
        for sv in (-1, 1) if not SUPPORTS() else ():
            j = rect(*ff(0, sv * (W / 2 + T / 2)), Dp, T, a); foot.append(j)
            lay.append(("up", j, rng.uniform(2.3, 2.9), st)); lay.append(("tcut", _bar(ff(0, sv * W / 2), ff(0, sv * (W / 2 + T)), 0.24), len(lay) - 1, 0.22))
        th = math.atan2(cy, cx); Rr = gg.R0
        for sv in (-1, 1):
            u = (W / 2 + T + 0.5) / Rr
            for i in range(rng.randint(3, 4) + int(1.5 * k)):
                bl = rng.uniform(2.8, 4.4); t = th + sv * (u + bl / 2 / Rr); u += (bl + 0.1) / Rr
                if rng.random() < 0.15: continue
                q = rect((Rr - 1.6) * math.cos(t), (Rr - 1.6) * math.sin(t), 3.2, bl - 0.15, t); foot.append(q)
                lay.append(("up", q, rng.uniform(1.0, 2.3) * (1 - 0.1 * i), st))
        return _u(foot).buffer(0.9).buffer(-0.6), lay
    return f

def S_ruin(p):
    """the floor of a ruined building, broken walls round the back and sides, the doorway at the front"""
    st, fl = kfix(p["stone"]), p["floor"] or p["stone"]
    def f(cx, cy, a, rng, k):
        L = rng.uniform(10, 12.5) * k * 0.85; W = rng.uniform(9, 11.5) * k * 0.85; ff = frame(cx, cy, a); flr = rect(cx, cy, L, W, a); t = 1.7
        js = 2.2 * _kk(k, 0.3)
        joints = _u([_bar(ff(u, -W), ff(u, W), 0.26) for u in np.arange(-L / 2 + js, L / 2 - 0.5, js)] + [_bar(ff(-L, v), ff(L, v), 0.26) for v in np.arange(-W / 2 + js, W / 2 - 0.5, js)])
        lay = [("up", flr, 0.35, fl), ("tcut", joints.intersection(inset(flr, t + 0.3)), 0, 0.25)]
        def run(p0, p1):
            length = math.dist(p0, p1); ang = math.atan2(p1[1] - p0[1], p1[0] - p0[0]); f2 = frame(p0[0], p0[1], ang); u = 0.0
            while u < length - 1.2:
                bl = min(rng.uniform(2.2, 3.8), length - u)
                if rng.random() > 0.12: lay.append(("on", rect(*f2(u + bl / 2, 0), bl - 0.1, t - 0.35, ang), 0, rng.uniform(0.7, 2.6), st))
                u += bl
        e = t / 2
        run(ff(-L / 2 + e, -W / 2 + e), ff(-L / 2 + e, W / 2 - e))
        for sv in (-1, 1):
            if rng.random() < 0.8: run(ff(-L / 2 + e, sv * (W / 2 - e)), ff(L / 2 * rng.uniform(-0.2, 1.0) - e, sv * (W / 2 - e)))
            if rng.random() < 0.7: run(ff(L / 2 - e, sv * (W / 2 - e)), ff(L / 2 - e, sv * rng.uniform(1.9, 2.6)))
        return flr, lay
    return f

def S_shaft(p):
    """a square shaft with a timber frame, a windlass across it and the end of a ladder poking out"""
    wd = kfix(p["wood"])
    def f(cx, cy, a, rng, k):
        ff = frame(cx, cy, a); S = rng.uniform(5.0, 6.2) * _kk(k, 0.8); t = 1.3 * _kk(k, 0.4)
        outer = rect(cx, cy, S + 2 * t, S + 2 * t, a); hole = rect(cx, cy, S, S, a)
        seams = _u([_bar(ff(su * S / 2, sv * S / 2), ff(su * (S / 2 + t + 0.3), sv * (S / 2 + t + 0.3)), 0.24) for su in (-1, 1) for sv in (-1, 1)])
        lay = [("gcut", inset(hole, 0.15), 0.9, p["pit"]), ("up", outer.difference(hole), 0.9, wd), ("tcut", seams, 1, 0.25)]
        lay += [("on", rect(*ff(-S * 0.15, sv * (S / 2 + t / 2)), 1.25, 1.25, a), 1, 1.6, wd) for sv in (-1, 1)]
        lay.append(("up", _bar(ff(-S * 0.15, -S / 2 - 0.1), ff(-S * 0.15, S / 2 + 0.1), 1.2), 1.9, wd))            # the drum
        sv = rng.choice((-1, 1)); lf = frame(*ff(S * 0.2, sv * (S / 2 - 1.6)), a + sv * math.pi / 2)
        lad = _u([_bar(lf(0, v), lf(4.6 + t, v), 0.92) for v in (-0.95, 0.95)] + [_bar(lf(u, -1.0), lf(u, 1.0), 0.92) for u in np.arange(0.5, 4.4 + t, 1.6)])
        lay.append(("up", lad.difference(outer.difference(hole).buffer(0.3)), 0.5, wd))
        return _u([outer.buffer(0.4), lad.convex_hull]), lay
    return f

def S_camp(p):
    """a fire ring with bedrolls round the far side, a log to sit on and a kettle"""
    def f(cx, cy, a, rng, k):
        foot0, lay = F.b_campfire(p["rock"] or p["stone"], p["wood"])(cx, cy, a, rng); lay = list(lay); foot = [foot0]
        n = rng.randint(2, 3) + int(k > 2.2); d = 5.6 + 1.0 * _kk(k)
        for i in range(n):
            t = a + math.pi + (i - (n - 1) / 2) * 1.0 + rng.uniform(-0.12, 0.12); bx, by = cx + d * math.cos(t), cy + d * math.sin(t)
            b = t + math.pi / 2 + rng.uniform(-0.3, 0.3); bf = frame(bx, by, b); roll = rect(bx, by, 4.6, 1.9, b).buffer(0.3); foot.append(roll)
            lay.append(_steps(vault(bx, by, b, 5.0, 2.4, 0.6, "loaf", 5), "sack")); i0 = len(lay) - 1
            lay += [("on", rect(*bf(1.7, 0), 1.3, 1.7, b), i0, 0.35, "sack"), ("tcut", _bar(bf(-0.6, -1.5), bf(-0.6, 1.5), 0.24), i0, 0.2)]
        sd = rng.choice((-1, 1)); t = a + sd * rng.uniform(1.2, 1.5); lx, ly = cx + (d - 0.8) * math.cos(t), cy + (d - 0.8) * math.sin(t)
        lg = _cap((lx - 2.6 * math.sin(t), ly + 2.6 * math.cos(t)), (lx + 2.6 * math.sin(t), ly - 2.6 * math.cos(t)), 1.4); foot.append(lg)
        lay += [_steps(vault(lx, ly, t + math.pi / 2, 5.4, 1.7, 0.95), p["wood"])]
        t = a - sd * 0.6; kx, ky = cx + 4.0 * math.cos(t), cy + 4.0 * math.sin(t); kd = _disc(kx, ky, 0.85, 16); foot.append(kd)
        lay += [_steps(lathe(kx, ky, [(0.95, 0.0), (0.95, 0.45)] + curve(0.95, 0.5, 0.45, 0.9, 5), 16), p["iron"]), ("on", _disc(kx, ky, 0.34, 10), len(lay), 0.25, p["iron"])]
        return _u(foot).convex_hull.buffer(0.3), lay
    return f

def S_depot(p):
    """crates and barrels in neat rows on a plank pallet, the back row a level higher"""
    wd = kfix(p["wood"])
    def f(cx, cy, a, rng, k):
        ff = frame(cx, cy, a); nx = 2 + int(k > 1.9); ny = 2 + int(k + 0.4); cw = 2.7; L, W = nx * cw + 0.8, ny * cw + 0.8
        pal_ = rect(cx, cy, L, W, a)
        lay = [("up", pal_, 0.35, wd), ("tcut", _u([_bar(ff(-L, v), ff(L, v), 0.24) for v in np.arange(-W / 2 + 1.2, W / 2 - 0.4, 1.2)]), 0, 0.22)]
        for i in range(nx):
            for j in range(ny):
                if rng.random() < 0.16: continue
                c = ff((i - (nx - 1) / 2) * cw, (j - (ny - 1) / 2) * cw); hi = 1.0 if i == 0 else 0.0
                if rng.random() < 0.3:
                    hb = 1.7 + hi; lay.append(_steps(lathe(c[0], c[1], [(1.3, 0.0), (1.3, hb * 0.5)] + curve(1.3, 1.08, hb * 0.5, hb, 6)), p["wood"], 0)); lay.append(("on", _ann(c[0], c[1], 1.25, 0.5, 24), len(lay) - 1, 0.22, p["wood"]))
                else:
                    b = a + rng.uniform(-0.07, 0.07); f2 = frame(c[0], c[1], b)
                    gq = rect(c[0], c[1], 2.4, 2.4, b); n0 = len(lay); lay += _crate_layers(gq, f2, 2.4, 2.4, b, 1.3 + hi, p["crate"], rng, parent=0, n0=n0)
        return pal_, lay
    return f

def S_graves(p):
    """a row of grave mounds, a headstone at the head of each, one open with the spoil heaped beside it"""
    st, ea = kfix(p["stone"]), p["soft"] or "earth"
    def f(cx, cy, a, rng, k):
        ff = frame(cx, cy, a); n = 2 + int(k + 0.5); gap = 4.1; lay = []; foot = []; op = rng.randrange(n)
        for i in range(n):
            v = (i - (n - 1) / 2) * gap; b = a + rng.uniform(-0.05, 0.05); m = rect(*ff(0.6, v), 5.6, 2.2, b).buffer(0.45, 4); hs = rect(*ff(-3.9, v), 1.05, 2.3, b); foot += [m, hs]
            if i == op:
                lay.append(("gcut", inset(m, 0.3), 0.85, p["pit"])); sp = blob(*ff(5.6, v + rng.uniform(-0.6, 0.6)), 1.5, rng); foot.append(sp)
                lay.append(("mound", sp, 1.1, ea, SOFT_T))
            else: lay.append(("mound", m, 0.7, ea, SOFT_T))
            lay.append(("up", hs, rng.uniform(1.8, 2.6), st))
        return _u(foot).convex_hull.buffer(0.3), lay
    return f

def S_wagon(p):
    """a wagon lying across the way: bed with plank seams, wheels, the tongue pole, one wheel off and lying beside it"""
    wd = kfix(p["wood"])
    def f(cx, cy, a, rng, k):
        b = a + math.pi / 2 + rng.uniform(-0.25, 0.25); ff = frame(cx, cy, b); L = rng.uniform(9, 11) * _kk(k, 0.7); W = rng.uniform(4.6, 5.4) * _kk(k, 0.6)
        bed = rect(cx, cy, L, W, b); corners = [(su, sv) for su in (-1, 1) for sv in (-1, 1)]; gone = rng.choice(corners)
        wheels = [q for su, sv in corners if (su, sv) != gone for q in vault(*ff(su * (L / 2 - 1.9), sv * (W / 2 + 0.6)), b + math.pi / 2, 1.05, 3.4, 2.2, n=12) + vault(*ff(su * (L / 2 - 1.9), sv * (W / 2 + 0.6)), b + math.pi / 2, 1.6, 0.95, 1.2)]
        if SUPPORTS():
            Rw = 1.75; ax = Rw * 0.95; wl = [q for su, sv in corners if (su, sv) != gone for q in vault(*ff(su * (L / 2 - 1.9), sv * (W / 2 + 0.6)), b + math.pi / 2, 0.9, 2 * Rw, Rw, under=True, axle=ax) + vault(*ff(su * (L / 2 - 1.9), sv * (W / 2 + 0.6)), b + math.pi / 2, 1.5, 0.9, 0.5, under=True, axle=ax)]
            axles = [q for su in (-1, 1) for q in vault(*ff(su * (L / 2 - 1.9), 0), b + math.pi / 2, W + 1.0, 0.7, 0.4, under=True, axle=ax)]
            body = slab(bed, ax + 0.25, ax + 0.95) + slab(bed.difference(inset(bed, 0.8)), ax + 0.9, ax + 1.9)                      # the bed and its side boards, up on the axles
            prop = [(rect(*ff(gone[0] * (L / 2 - 1.9), gone[1] * (W / 2 - 0.9)), 1.3, 1.3, b), ax + 0.3)]                           # a block under the corner that lost its wheel
            tg = _bar(ff(L / 2, 0), ff(L / 2 + 5.0 * _kk(k), 0), 0.95); wx, wy = ff(gone[0] * (L / 2 - 1.0 + rng.uniform(0, 1.5)), gone[1] * (W / 2 + 4.2))
            wf, wl2 = F.b_wheel(p["wood"], p["iron"])(wx, wy, rng.uniform(0, 3), rng)
            lay = [_steps(wl + axles + body + prop + slab(tg, ax - 0.1, ax + 0.4), wd)]; _join(lay, wl2)
            return _u([bed.buffer(0.9), tg, wf]).convex_hull, lay
        lay = [_steps(wheels, wd), ("up", bed, 1.0, wd), ("tcut", _u([_bar(ff(-L, v), ff(L, v), 0.24) for v in np.arange(-W / 2 + 1.1, W / 2 - 0.4, 1.1)]), 1, 0.22),
               ("on", bed.difference(inset(bed, 0.95)), 1, 0.9, wd)]
        tg = _bar(ff(L / 2, 0), ff(L / 2 + 5.0 * _kk(k), 0), 0.95); lay.append(("up", tg, 0.5, wd))
        wx, wy = ff(gone[0] * (L / 2 - 1.0 + rng.uniform(0, 1.5)), gone[1] * (W / 2 + 4.2))
        wf, wl = F.b_wheel(p["wood"], p["iron"])(wx, wy, rng.uniform(0, 3), rng); _join(lay, wl)
        return _u([bed.buffer(0.9), tg, wf]).convex_hull, lay
    return f

def S_corral(p):
    """a ring of posts and rails with the gate gap at the front and a trough inside against the fence"""
    wd = kfix(p["wood"])
    def f(cx, cy, a, rng, k):
        R = rng.uniform(7.0, 8.5) * _kk(k, 0.8); n = max(7, int(2 * math.pi * R / 4.2)); pts = [(cx + R * math.cos(a + 6.283 * (i + 0.5) / n), cy + R * math.sin(a + 6.283 * (i + 0.5) / n)) for i in range(n)]
        posts = _u([rect(x, y, 1.15, 1.15, a + 6.283 * (i + 0.5) / n) for i, (x, y) in enumerate(pts)])
        rails = _u([_bar(pts[i], pts[i + 1], 0.92) for i in range(n - 1) if rng.random() > 0.08])
        lay = [("up", rails.difference(posts.buffer(0.12)), 0.9, wd), ("up", posts, 1.9, wd)]
        t = a + math.pi + rng.uniform(-0.9, 0.9); tr = rect(cx + (R - 2.0) * math.cos(t), cy + (R - 2.0) * math.sin(t), 1.7, 4.2, t)
        lay += [("up", tr, 0.95, wd), ("tcut", inset(tr, 0.45), len(lay), 0.5)]
        return _disc(cx, cy, R + 0.8, 40), lay
    return f

def S_machine(p):
    """a machine: bed plate, housing with a grille, a round tank, a control box at the front, pipe stubs out of two sides"""
    m = kfix(p["iron"])
    def f(cx, cy, a, rng, k):
        ff = frame(cx, cy, a); Dp = rng.uniform(6.0, 7.5) * _kk(k, 0.8); W = rng.uniform(8.0, 10.0) * _kk(k, 0.8); base = rect(cx, cy, Dp, W, a)
        lay = [("up", base, 0.6, m), ("tcut", inset(base, 0.45).exterior.buffer(0.14), 0, 0.22)]
        hc = ff(-Dp * 0.12, -W * 0.2); hw, hl = Dp * 0.6, W * 0.46; lay.append(("on", rect(hc[0], hc[1], hw, hl, a), 0, 1.8, m)); f2 = frame(hc[0], hc[1], a)
        lay.append(("tcut", _u([_bar(f2(-hw * 0.32, v), f2(hw * 0.32, v), 0.36) for v in np.arange(-hl * 0.3, hl * 0.31, 0.8)]), len(lay) - 1, 0.3))
        tc = ff(-Dp * 0.05, W * 0.27); tr = min(Dp, W) * 0.2 + 0.4; lay.append(("on", _disc(tc[0], tc[1], tr, 28), 0, 2.3, m)); lay.append(("tcut", _ann(tc[0], tc[1], tr * 0.62, tr * 0.62 - 0.3), len(lay) - 1, 0.25))
        lay.append(("on", rect(*ff(Dp * 0.33, -W * 0.28), 1.8, 2.6, a), 0, 1.1, m))
        foot = [base.buffer(0.4)]
        for sv in (-1, 1):
            u = rng.uniform(-Dp * 0.25, Dp * 0.2); st = _bar(ff(u, sv * (W / 2 - 0.2)), ff(u, sv * (W / 2 + 2.8)), 1.5); fl_ = rect(*ff(u, sv * (W / 2 + 2.6)), 2.2, 0.95, a); foot += [st, fl_]
            lay += [("up", st.difference(base.buffer(0.15)).difference(fl_.buffer(0.15)), 0.9, "pipe"), ("up", fl_, 0.98, "pipe")]
        return _u(foot).convex_hull, lay
    return f

def S_emplacement(p):
    """a horseshoe of sandbags, open on the side the duckboards come in, with a field gun inside and a shell stack"""
    def f(cx, cy, a, rng, k):
        ff = frame(cx, cy, a); R = rng.uniform(6.3, 7.3) * _kk(k, 0.8); lay = []; i = 0
        for t in np.arange(0.8, 6.283 - 0.8, 2.95 / R):
            q = rect(cx + R * math.cos(a + t), cy + R * math.sin(a + t), 1.7, 2.6, a + t).buffer(0.3, 3)
            lay.append(("mound", q, 1.5 if i % 2 else 1.05, "sandbag", BAG_T)); i += 1
        iw = kfix(p["iron"])
        lay += [("up", _u([rect(*ff(-0.7, sv * 2.0), 2.7, 1.0, a) for sv in (-1, 1)]), 1.5, kfix(p["wood"])), ("up", _bar(ff(-1.0, 0), ff(-R * 0.8, 0), 1.05), 1.3, iw),
                ("up", rect(*ff(0.3, 0), 2.2, 1.9, a), 1.6, iw), ("up", _bar(ff(1.6, 0), ff(4.4, 0), 1.0), 0.6, iw)]
        sv = rng.choice((-1, 1)); sc_ = frame(*ff(1.0, sv * R * 0.55), a + rng.uniform(-0.3, 0.3))
        lay.append(("up", _u([_cap(sc_(-1.1, v), sc_(1.1, v), 0.9) for v in (-1.0, 0, 1.0)]).buffer(0.05), 0.7, p["iron"]))
        return _disc(cx, cy, R + 1.6, 32), lay
    return f

def S_tent(p):
    """a ridge tent: the flaps open at the front, guy ropes to pegs at the corners"""
    def f(cx, cy, a, rng, k):
        ff = frame(cx, cy, a); L = rng.uniform(8, 10) * _kk(k, 0.7); W = rng.uniform(6.0, 7.0) * _kk(k, 0.5); body = rect(cx, cy, L, W, a)
        lay = [_steps(vault(cx, cy, a, L, W, W * 0.46, "ridge", 12) + vault(cx, cy, a, L + 0.9, 0.55, W * 0.46 + 0.2, "loaf", 3), "sail")]; foot = [body]
        lay += [("up", Polygon([ff(L / 2 + 0.4, sv * 0.45), ff(L / 2 + 2.3, sv * 2.3), ff(L / 2 + 0.4, sv * 2.6)]), 0.45, "sail") for sv in (-1, 1)]
        for su in (-1, 1):
            for sv in (-1, 1):
                c0 = ff(su * (L / 2 + 0.4), sv * (W / 2 + 0.4)); c1 = ff(su * (L / 2 + 1.9), sv * (W / 2 + 1.9)); c2 = ff(su * (L / 2 + 2.5), sv * (W / 2 + 2.5))
                lay += [("up", _bar(c0, c1, 0.92), 0.35, "rope"), ("up", rect(c2[0], c2[1], 1.0, 1.0, a + 0.785), 0.8, p["wood"])]; foot.append(_disc(c2[0], c2[1], 0.8))
        return _u(foot).convex_hull, lay
    return f

def S_log(p):
    """a fallen giant: the long trunk with bark seams, the root plate at one end, broken branches, the hole it left"""
    wd = kfix(p["wood"])
    def f(cx, cy, a, rng, k):
        b = a + math.pi / 2 + rng.uniform(-0.3, 0.3); ff = frame(cx, cy, b); L = rng.uniform(16, 21) * _kk(k, 0.8); W = rng.uniform(2.7, 3.3) * _kk(k, 0.5)
        trunk = Polygon([ff(-L / 2, -W / 2), ff(L / 2, -W * 0.36), ff(L / 2, W * 0.36), ff(-L / 2, W / 2)])
        seams = _u([LineString(wiggle(ff(-L / 2 + 0.6, v), ff(L / 2 - 0.6, v * 0.75), rng, 0.25, 6)).buffer(0.13) for v in (-W * 0.22, 0.05, W * 0.24)])
        lay = [_steps(loft(cx, cy, b, L, lambda u: W / 2 - W * 0.14 * (u / L + 0.5), 1.15), wd), ("tcut", seams, 0, 0.25)]
        rc = ff(-L / 2 - 0.5, 0); rp_ = star(rc[0], rc[1], W * 1.1, W * 0.68, 7, rng.uniform(0, 1)); lay.append(("up", rp_.difference(trunk.buffer(0.15)), 2.3, kfix("root")))
        foot = [trunk, rp_]
        for i in range(rng.randint(2, 4)):
            u = rng.uniform(-L * 0.25, L * 0.42); sv = rng.choice((-1, 1)); br = _bar(ff(u, sv * W * 0.3), ff(u + rng.uniform(0.8, 1.6), sv * (W / 2 + rng.uniform(1.8, 3.2))), 1.0)
            lay.append(("up", br.difference(trunk.buffer(0.15)), 0.8, wd)); foot.append(br)
        hc = ff(-L / 2 - W * 1.1 - 1.6, 0); hole = blob(hc[0], hc[1], 1.7, rng); lay.append(("gcut", hole, 0.5, p["pit"])); foot.append(hole)
        return _u(foot).convex_hull.buffer(0.2), lay
    return f

def S_well(p):
    """a big ring wall with seams, water inside, a bucket and a coil of rope beside it"""
    st = kfix(p["stone"])
    def f(cx, cy, a, rng, k):
        ff = frame(cx, cy, a); R = rng.uniform(3.6, 4.4) * _kk(k, 0.6); n = rng.randint(9, 12)
        seams = _u([_bar((cx + (R - 1.5) * math.cos(a + 6.283 * i / n), cy + (R - 1.5) * math.sin(a + 6.283 * i / n)), (cx + (R + 0.2) * math.cos(a + 6.283 * i / n), cy + (R + 0.2) * math.sin(a + 6.283 * i / n)), 0.26) for i in range(n)])
        lay = [("up", _ann(cx, cy, R, R - 1.3, 2 * n), 1.5, st), ("tcut", seams, 0, 0.25), ("gcut", _disc(cx, cy, R - 1.7, 24), 0.85, p["water"])]
        sv = rng.choice((-1, 1)); bc = ff(R * 0.45, sv * (R + 1.5)); rc = ff(-R * 0.3, sv * (R + 1.9))
        lay += [("up", _disc(bc[0], bc[1], 0.95, 16), 0.98, p["wood"]), ("tcut", _disc(bc[0], bc[1], 0.55, 12), len(lay), 0.5), ("up", _ann(rc[0], rc[1], 1.35, 0.45, 18), 0.45, "rope")]
        return _u([_disc(cx, cy, R + 0.3, 32), _disc(bc[0], bc[1], 1.2), _disc(rc[0], rc[1], 1.5)]).convex_hull, lay
    return f

def S_statue(p):
    """a plinth with the stump of a statue; the rest of the figure lies broken in three pieces beside the way up to it"""
    st = kfix(p["stone"])
    def f(cx, cy, a, rng, k):
        P = rng.uniform(4.4, 5.2) * _kk(k, 0.6); pl = rect(cx, cy, P, P, a); s_ = _kk(k, 0.5)
        lay = [("up", pl, 0.7, st), ("on", inset(pl, 0.75), 0, 0.9, st), _steps(lathe(cx, cy, [(P * 0.27, 0.0), (P * 0.22, 0.7), (P * 0.2, 1.0)], 12), st, 1), ("tcut", inset(pl, 0.4).exterior.buffer(0.13), 0, 0.22)]
        sv = rng.choice((-1, 1)); b = a + sv * rng.uniform(0.75, 1.05); bf = frame(cx, cy, b); u = P / 2 + 1.6; foot = [pl.buffer(0.4)]
        for ln, w in ((3.8 * s_, 2.5 * s_), (3.2 * s_, 2.0 * s_)):
            qc = bf(u + ln / 2, rng.uniform(-0.3, 0.3)); qa = b + rng.uniform(-0.25, 0.25); q = rect(qc[0], qc[1], ln, w, qa); foot.append(q); u += ln + 0.7
            lay.append(_steps(loft(qc[0], qc[1], qa, ln, lambda u_, w=w, ln=ln: w / 2 * (0.82 + 0.18 * math.cos(u_ / ln * 2.4)), 0.95), st))          # a drum of the fallen figure
        hc = bf(u + 1.2 * s_, rng.uniform(-0.8, 0.8)); hd = _disc(hc[0], hc[1], 1.15 * s_, 16); foot.append(hd)
        lay.append(_steps(lathe(hc[0], hc[1], curve(1.15 * s_, 0.35, 0.0, 1.5 * s_, 7), 16), st))                                              # the head, face down
        return _u(foot).convex_hull, lay
    return f

def S_organ(p):
    """a great soft mound with thick veins running out of it and tapering away"""
    def f(cx, cy, a, rng, k):
        R = rng.uniform(4.4, 5.4) * _kk(k, 0.8); body = blob(cx, cy, R, rng, 5).convex_hull.buffer(0.3); lay = [("mound", body, 2.4, p["soft"] or "flesh", SOFT_T)]; parts = []
        n = rng.randint(5, 7)
        for i in range(n):
            t = a + 6.283 * (i + 0.5) / n + rng.uniform(-0.25, 0.25); Ln = rng.uniform(5, 8) * _kk(k, 0.6); m = 6
            pts = wiggle((cx + (R * 0.7) * math.cos(t), cy + (R * 0.7) * math.sin(t)), (cx + (R + Ln) * math.cos(t), cy + (R + Ln) * math.sin(t)), rng, 0.8, m)
            parts += [_cap(pts[j], pts[j + 1], 1.7 - 0.75 * j / (m - 1)) for j in range(m)]
        veins = _u(parts).difference(inset(body, 1.2)); lay.append(("up", veins, 0.6, p["vein"]))      # the veins run well into the mound, so the two fuse
        return body.buffer(1.2), lay
    return f

# ---------------------------------------------------------------------------------------- made things, made properly
def _weather(layers, rng, chip=0.6):
    """worn masonry: a block (a plain rectangle at least 1 mm tall) gets a corner knocked off and a broken top,
    one or two ragged pieces of it still standing higher than the rest. Layer numbers stay as they were."""
    out = list(layers)
    for i, L in enumerate(layers):
        if L[0] not in ("up", "on"): continue
        q = L[1]; h = L[2] if L[0] == "up" else L[3]
        if h < 1.0 or q is None or q.geom_type != "Polygon" or q.area < 6 or len(q.exterior.coords) > 6: continue
        if rng.random() < chip:
            k = rng.choice(list(q.exterior.coords)[:-1]); q2 = max(polys_of(q.difference(stone(k[0], k[1], rng.uniform(0.7, 1.2), rng, 0.8, 5))), key=lambda g: g.area, default=q)
            if q2.area > 0.7 * q.area: out[i] = L[:1] + (q2,) + tuple(L[2:]); q = q2
        cc = q.centroid; r = math.sqrt(q.area) * 0.45
        for _ in range(rng.randint(0, 2)):
            piece = stone(cc.x + rng.uniform(-r, r), cc.y + rng.uniform(-r, r), r * rng.uniform(0.7, 1.1), rng, 0.8, rng.randint(4, 6)).intersection(inset(q, 0.15))
            if piece.geom_type == "Polygon" and piece.area > 1.3 and wide_enough(piece): out.append(("on", piece, i, rng.uniform(0.5, 1.4), L[-1]))
    return out

def _worn(mk):
    def mk2(p):
        b = mk(p)
        def f(cx, cy, a, rng, k):
            got = b(cx, cy, a, rng, k)
            return got if got is None or got[0] is None else (got[0], _weather(got[1], rng))
        return f
    return mk2

def _crate_layers(g, ff, L, W, a, h, kind, rng, parent=None, n0=0):
    """a plank crate seen from above: frame boards standing proud round a panel of planks, a diagonal brace across it"""
    brace = _bar(ff(-L / 2 + 0.6, -W / 2 + 0.6), ff(L / 2 - 0.6, W / 2 - 0.6), 0.92) if rng.random() < 0.5 else _bar(ff(-L / 2 + 0.6, W / 2 - 0.6), ff(L / 2 - 0.6, -W / 2 + 0.6), 0.92)
    proud = _u([g.difference(inset(g, 0.92)), brace.intersection(g)])
    seams = _u([_bar(ff(-L, v), ff(L, v), 0.22) for v in np.arange(-W / 2 + 1.45, W / 2 - 1.2, 0.62)]).intersection(inset(g, 1.05)).difference(brace.buffer(0.15))
    body = ("up", g, h, kind) if parent is None else ("on", g, parent, h, kind)
    return [body, ("on", proud, n0, 0.3, kind)] + ([("tcut", seams, n0, 0.18)] if not seams.is_empty else [])

def b_crate(kind="crate", wood="wood"):
    def f(cx, cy, a, rng):
        L = rng.uniform(3.4, 4.2); W = rng.uniform(2.9, 3.4); ff = frame(cx, cy, a); g = rect(cx, cy, L, W, a); h = rng.uniform(1.2, 1.7)
        if rng.random() < 0.3:          # an opened crate: the inside dark and deep, the lid lying against it
            lay = [("up", g, h, kind), ("tcut", inset(g, 0.92), 0, 0.9)]; lf = frame(*ff(rng.uniform(-0.5, 0.5), W / 2 + 1.55), a + rng.uniform(-0.35, 0.35)); lid = rect(*lf(0, 0), L * 0.95, W * 0.8, a + rng.uniform(-0.3, 0.3))
            lay += [("up", lid, 0.4, kind), ("tcut", _u([_bar(lf(-L, v), lf(L, v), 0.22) for v in (-0.6, 0, 0.6)]), len(lay), 0.18)]
            return _u([g, lid]).convex_hull, lay
        return g, _crate_layers(g, ff, L, W, a, h, kind, rng)
    f.tag, f.kind = "crate", kind
    return f

def b_crate_stack(kind="crate", wood="wood"):
    def f(cx, cy, a, rng):
        L = rng.uniform(3.4, 4.0); W = rng.uniform(2.9, 3.3); ff = frame(cx, cy, a); g = rect(cx, cy, L, W, a); lay = _crate_layers(g, ff, L, W, a, rng.uniform(1.3, 1.6), kind, rng); foot = [g]
        b = a + rng.uniform(-0.3, 0.3); c2 = ff(rng.uniform(-0.4, 0.4), W + 0.5); f2 = frame(c2[0], c2[1], b); L2 = L * rng.uniform(0.8, 0.95); g2 = rect(c2[0], c2[1], L2, W, b)
        n0 = len(lay); lay += _crate_layers(g2, f2, L2, W, b, rng.uniform(1.1, 1.4), kind, rng, n0=n0); foot.append(g2)
        if rng.random() < 0.6:          # a smaller box left on top of the first, askew
            b3 = a + rng.uniform(0.2, 0.6); g3 = rect(*ff(rng.uniform(-0.3, 0.3), rng.uniform(-0.2, 0.2)), 2.1, 1.9, b3); n0 = len(lay)
            lay += [("on", g3, 0, 1.0, kind), ("tcut", inset(g3, 0.35).exterior.buffer(0.11), n0, 0.18)]
        return _u(foot).convex_hull, lay
    f.tag, f.kind = "crates", kind
    return f

# ---------------------------------------------------------------------------------------- rounded and turned shapes
# The mesh is a height map: one height per spot, no overhangs. A curved shape is drawn as many thin nested pieces whose
# heights follow the curve, fine enough (about 0.12 mm a step) that the printer's own layers hide the steps.
_CHAIN = [0]
def _chain(): _CHAIN[0] += 1; return _CHAIN[0]

# A solid is (section, top): section(z) is the outline of the part of it that stands higher than z, top its highest
# point. vault / lathe / loft / tube / box make solids; sculpt() joins any number of them, with hollows cut in, into
# ONE surface: at each of a dozen heights the outlines of all the solids are merged, and the mesh builder slopes from
# each merged outline to the next. Because it is one surface, overlapping solids (a brow on a skull, a barrel in the
# saddle of two others) cannot tear the mesh.
SUPPORTS = lambda: bool(CFG.get("allow_supports", False))          # True: objects may overhang (they need supports to print)
TOP3 = {}            # chain number -> the height its true solids were drawn at (the layer may be made taller afterwards)
TRUE3D = {}          # chain number -> [build(zb, zs) -> (points, triangles)]; zs = how much taller than drawn: the true 3D solids of that object, added to the mesh after the surface is made

def _extrude(poly, z0, z1):
    """a closed block: the outline `poly` from height z0 up to z1. With z0 above the ground it hangs in the air."""
    import shapely
    from shapely.geometry.polygon import orient
    V = []; Fc = []; ix = {}
    def vid(x, y, z):
        k = (round(x, 5), round(y, 5), z)
        if k not in ix: ix[k] = len(V); V.append((x, y, z))
        return ix[k]
    for g in polys_of(poly):
        g = orient(g, 1.0)
        for t in shapely.constrained_delaunay_triangles(g).geoms:
            (ax, ay), (bx, by), (cx_, cy_) = list(t.exterior.coords)[:3]
            if (bx - ax) * (cy_ - ay) - (by - ay) * (cx_ - ax) < 0: (bx, by), (cx_, cy_) = (cx_, cy_), (bx, by)
            Fc.append((vid(ax, ay, z1), vid(bx, by, z1), vid(cx_, cy_, z1))); Fc.append((vid(ax, ay, z0), vid(cx_, cy_, z0), vid(bx, by, z0)))
        for ring in [g.exterior] + list(g.interiors):
            cs = list(ring.coords)
            for (ax, ay), (bx, by) in zip(cs, cs[1:]):
                if (ax, ay) == (bx, by): continue
                Fc.append((vid(ax, ay, z0), vid(bx, by, z0), vid(bx, by, z1))); Fc.append((vid(ax, ay, z0), vid(bx, by, z1), vid(ax, ay, z1)))
    return V, Fc

def slab(poly, z0, z1, kind=None):
    """a block that hangs in the air between two heights when supports are allowed (a lintel, a wagon bed, a sign
    board); otherwise it is built solid from the ground"""
    sect = lambda z: poly if z < z1 else None
    return [(sect, z1, lambda zb, zs=1.0: _extrude(poly.intersection(Point(0, 0).buffer(gg.R0 - gg.H * math.tan(math.radians(getattr(gg, 'RIM', 12))) - 0.15, 96)), zb + z0 * zs, zb + z1 * zs))] if SUPPORTS() else [(sect, z1)]          # cut off at the edge of the base's top

def _revolve(cx, cy, prof, seg, squash, a, zb, sink=0.3):
    """a closed solid turned on a lathe from the profile exactly as given: it may draw in below its widest part"""
    prof = [(prof[0][0], prof[0][1] - sink)] + list(prof); ca, sa = math.cos(a), math.sin(a); V = []; Fc = []
    for r, z in prof:
        for k in range(seg):
            t = 6.283185 * k / seg; u, v = r * math.cos(t), r * squash * math.sin(t); V.append((cx + u * ca - v * sa, cy + u * sa + v * ca, zb + z))
    n = len(prof)
    for i in range(n - 1):
        for k in range(seg):
            p0, p1, q0, q1 = i * seg + k, i * seg + (k + 1) % seg, (i + 1) * seg + k, (i + 1) * seg + (k + 1) % seg; Fc += [(p0, p1, q1), (p0, q1, q0)]
    b0 = len(V); V.append((cx, cy, zb + prof[0][1])); t0 = len(V); V.append((cx, cy, zb + prof[-1][1]))
    for k in range(seg): Fc += [(b0, (k + 1) % seg, k), (t0, (n - 1) * seg + k, (n - 1) * seg + (k + 1) % seg)]
    return V, Fc

def _sweep(pts, radii, seg=12, squash=1.0):
    """a closed round bar through 3D points, its radius given at each: the true shape of a log, a barrel on its side,
    a rib arching off the ground. squash < 1 flattens it."""
    V = []; Fc = []; n = len(pts)
    for i, (p, r) in enumerate(zip(pts, radii)):
        a_, b_ = pts[max(0, i - 1)], pts[min(n - 1, i + 1)]; T = (b_[0] - a_[0], b_[1] - a_[1], b_[2] - a_[2]); L = math.sqrt(sum(c * c for c in T)) or 1.0; T = tuple(c / L for c in T)
        N = (-T[1], T[0], 0.0); Ln = math.hypot(N[0], N[1])
        N = (1.0, 0.0, 0.0) if Ln < 1e-6 else (N[0] / Ln, N[1] / Ln, 0.0); B = (T[1] * N[2] - T[2] * N[1], T[2] * N[0] - T[0] * N[2], T[0] * N[1] - T[1] * N[0])
        if B[2] < 0: B = tuple(-c for c in B)
        for k in range(seg):
            t = 6.283185 * k / seg; c_, s_ = r * math.cos(t), r * squash * math.sin(t); V.append((p[0] + c_ * N[0] + s_ * B[0], p[1] + c_ * N[1] + s_ * B[1], p[2] + c_ * N[2] + s_ * B[2]))
    for i in range(n - 1):
        for k in range(seg):
            p0, p1, q0, q1 = i * seg + k, i * seg + (k + 1) % seg, (i + 1) * seg + k, (i + 1) * seg + (k + 1) % seg; Fc += [(p0, q1, p1), (p0, q0, q1)]
    b0 = len(V); V.append(tuple(pts[0])); t0 = len(V); V.append(tuple(pts[-1]))
    for k in range(seg): Fc += [(b0, k, (k + 1) % seg), (t0, (n - 1) * seg + (k + 1) % seg, (n - 1) * seg + k)]
    return V, Fc

def box(poly, h, lift=0.0):
    """a plain block: straight sides, flat top"""
    return [((lambda z: poly if z < lift + h else None), lift + h)]

def vault(cx, cy, a, L, W, h, prof="round", n=None, lift=0.0, under=False, axle=None):
    """the same cross-section all along its length L (along `a`), W wide and h high: round = a lying cylinder (log,
    pipe), ridge = a tent, loaf = flat-topped with rounded shoulders (a chest lid, a bedroll). The ends are upright."""
    def sect(z):
        t = max(0.0, (z - lift) / h)
        if t >= 1: return None
        w = W / 2 * {"round": math.sqrt(1 - t * t), "ridge": 1 - t, "loaf": (1 - t * t) ** 0.25}[prof]
        return rect(cx, cy, L, 2 * w, a) if w >= 0.2 else None
    if under and prof == "round" and SUPPORTS():          # a true cylinder lying on the ground: you can see under its sides
        r = W / 2; c = lift * 1.5 + 0.8 * r; ca, sa = math.cos(a), math.sin(a)
        return [(sect, lift + h, lambda zb, zs=1.0: _sweep([(cx + u * ca, cy + u * sa, zb + (c if axle is None else axle)) for u in (-L / 2, 0.0, L / 2)], [r] * 3, 20 if axle is not None else 14))]
    return [(sect, lift + h)]

def lathe(cx, cy, prof, seg=28, squash=1.0, a=0.0, n=None, lift=0.0, real=None):
    """a turned shape, round seen from above: prof lists (radius, height) from the foot up; between the points the
    side runs straight, so list enough of them to draw a curve. squash < 1 makes it oval, long along `a`. Where the
    radius grows with height the widest part is carried down to the ground: nothing can overhang."""
    sm = []
    for (r0, z0), (r1, z1) in zip(prof, prof[1:]):
        m = max(1, int(max(abs(z1 - z0), abs(r1 - r0)) / 0.03))
        sm += [(r0 + (r1 - r0) * k / m, z0 + (z1 - z0) * k / m) for k in range(m + 1)]
    top = max(z for _, z in sm)
    def sect(z):
        rr_ = max((r for r, zp in sm if zp > z - lift), default=0.0)
        if rr_ < 0.2: return None
        d = _disc(cx, cy, rr_, seg)
        return d if squash == 1.0 else _arot(_ascale(d, 1.0, squash, origin=(cx, cy)), a, origin=(cx, cy), use_radians=True)
    if real and SUPPORTS(): return [(sect, lift + top, lambda zb, zs=1.0: _revolve(cx, cy, [(r_, z_ * zs) for r_, z_ in real], min(seg, 24), squash, a, zb + lift * zs))]          # real: the profile as it truly is, drawing in below the belly
    return [(sect, lift + top)]

def _real(x):
    """a width worked out as (1 - t ** n) ** 0.5 comes out complex when rounding leaves a hair below zero at the very end: take it as the number it stands for"""
    return x.real if isinstance(x, complex) else x

def loft(cx, cy, a, L, rfun, hk=1.2, prof="round", n=None, lift=0.0, under=False):
    """a lying shape whose thickness changes along its length (a barrel's bulge and hoops, a log's taper, a sack, a
    tank's domed ends): rfun(u) is the half-width at u (-L/2..L/2 along `a`), its height there is rfun(u) * hk."""
    ff = frame(cx, cy, a); m = max(12, int(L / 0.13)); us = [-L / 2 + L * i / m for i in range(m + 1)]; rs = [max(0.0, _real(rfun(u))) for u in us]
    def sect(z):
        zl = max(0.0, z - lift); out = []; run = []
        for u, r in list(zip(us, rs)) + [(0, 0.0)]:
            t = zl / (r * hk) if r > 0.05 else 2.0
            w = 0.0 if t >= 1 else r * (math.sqrt(1 - t * t) if prof == "round" else 1 - t)
            if w >= 0.2: run.append((u, w)); continue
            if len(run) >= 2 and run[-1][0] - run[0][0] >= 0.3:
                g = Polygon([ff(u_, w_) for u_, w_ in run] + [ff(u_, -w_) for u_, w_ in reversed(run)])
                if g.is_valid: out.append(g)
            run = []
        return _u(out) if out else None
    if under and prof == "round" and SUPPORTS():          # the true body, round underneath, resting on its belly
        k3 = [i for i in range(0, m + 1, 2) if rs[i] >= 0.15] ; c = lift * 1.5 + 0.8 * max(rs)
        if len(k3) >= 2: return [(sect, lift + max(rs) * hk, lambda zb, zs=1.0: _sweep([ff(us[i], 0) + (zb + c,) for i in k3], [rs[i] for i in k3], 14))]
    return [(sect, lift + max(rs) * hk)]

def tube(pts, r, hk=1.0, n=None, lift=0.0, under=False, arch=0.0):
    """a round bar along a bent line (a rib, a limb bone, a root, a horn): r is its radius in mm, or a function of
    0..1 along the line, so it can taper or swell into a knuckle. Its height is radius * hk."""
    ln = LineString(pts); m = max(2, int(ln.length / 0.22)); P = [ln.interpolate(i / m, normalized=True) for i in range(m + 1)]
    R = [max(0.0, r(i / m) if callable(r) else r) for i in range(m + 1)]
    def sect(z):
        zl = max(0.0, z - lift); ws = [ri * math.sqrt(1 - (zl / (ri * hk)) ** 2) if ri > 0.05 and zl < ri * hk else 0.0 for ri in R]
        parts = [_u([P[i].buffer(ws[i], 5), P[i + 1].buffer(ws[i + 1], 5)]).convex_hull for i in range(m) if ws[i] >= 0.2 and ws[i + 1] >= 0.2]
        return _u(parts) if parts else None
    if (under or arch) and SUPPORTS():          # a true round bar; arch = how high its middle lifts off the ground (a rib)
        k3 = [i for i in range(m + 1) if R[i] >= 0.12]
        if len(k3) >= 2: return [(sect, lift + max(R) * hk, lambda zb, zs=1.0: _sweep([(P[i].x, P[i].y, zb + lift + 0.75 * R[i] + arch * max(0.0, math.sin(math.pi * i / m)) ** 0.7) for i in k3], [R[i] for i in k3], 10))]
    return [(sect, lift + max(R) * hk)]

NEST = 0.015         # each piece sits this far inside the one below, all the way round, so two outlines never share an edge or a corner

def sculpt(solids, cuts=(), n=None):
    """one smooth surface from several solids. cuts: (polygon, floor height) hollows with upright sides (an eye
    socket, the mouth of a pot). Returns one chain of nested pieces for a "steps" layer."""
    solids = [box(*x[:2])[0] if not callable(x[0]) else x for x in solids]; t3 = [x for x in solids if len(x) > 2]; foot3 = None
    if t3:          # --supports on: these are added to the mesh as true solids. A flat marker of their footprint stays in the layout, so the kit knows the spot is taken
        c = _chain(); TRUE3D[c] = [x[2] for x in t3]; TOP3[c] = max(x[1] for x in t3); foot3 = _u([x[0](0.0) for x in t3 if x[0](0.0) is not None]); top3 = max(x[1] for x in t3)
        solids = [x[:2] for x in solids if len(x) == 2]
        if not solids: return [(foot3, top3, c, 0.0, "ghost")]
    solids = [x[:2] for x in solids]; top = max(t for _, t in solids)
    _ts = F.true_scale(); n = n or (max(6, min(18, int(top / 0.15))) if _ts == 1.0 else max(8, min(40, int(top * _ts / 0.15)))); c = c if t3 else _chain(); out = [(foot3, top3, c, 0.0, "ghost")] if t3 else []; prev = None; raw = None
    zs = [top * j / n for j in range(n)] + [top * (1 - 0.45 / n), top * (1 - 0.12 / n), top]; n = len(zs) - 1; dz = top / max(1, n - 2)              # even steps: a low part of a tall object (a rib beside a skull) still gets several
    for j in range(n):
        z = zs[j]; U = _u([g for g in (sect(z + 1e-6) for sect, t in solids if t > z + 1e-6) if g is not None and not g.is_empty])
        for q, fl in cuts:
            if z >= fl - 1e-9: U = U.difference(q.buffer(0.025 * (z - fl) / dz, join_style=2))          # the hollow opens a hair wider at each step up, so its side is a steep slope, never a shared edge
        U = _u([q for q in polys_of(U.buffer(0.06, 4).buffer(-0.14, 4).buffer(0.08, 4)) if q.area > 0.12]) if not U.is_empty else U          # hairline gaps closed, no neck or sliver thinner than 0.14: they would pinch the mesh
        if U.is_empty: break
        if raw is not None and raw.area - U.intersection(raw).area < 0.02 and len(out[-1]) == 4: out[-1] = (out[-1][0], zs[j + 1], c, out[-1][3]); continue          # the outline did not change: the piece below just stands taller
        raw = U
        if prev is not None:                                                         # each piece sits a hair inside the one below, all the way round: two outlines never share an edge or a corner
            U = _u([q for q in polys_of(U.intersection(prev.buffer(-NEST, join_style=2))) if q.area > 0.1])
            if U.is_empty: break
        out.append((U, zs[j + 1], c, z)); prev = U
    return out

def curve(r0, r1, z0, z1, n=6, out=True):
    """profile points for a quarter-round from (r0, z0) up to (r1, z1): bulging outwards (a barrel's belly, a dome) or
    drawn in (a neck, a flared foot)"""
    pts = []
    for k in range(n + 1):
        t = k / n * math.pi / 2
        pts.append((r1 + (r0 - r1) * math.cos(t), z0 + (z1 - z0) * math.sin(t)) if out else (r0 + (r1 - r0) * math.sin(t), z1 + (z0 - z1) * math.cos(t)))
    return pts

def _steps(solids, kind, parent=None, cuts=()):
    """the "steps" layer of one object: its solids merged into one smooth surface. The outline comes from the
    solids' own ground sections (cheap); the nested pieces are only cut when the object is really placed (a detail
    is tried in many spots and most tries are thrown away: they never pay for the 6-40 sections, sd_r_models._Later)."""
    from sd_r_models import _Later
    try:
        sol = [box(*x[:2])[0] if not callable(x[0]) else x for x in solids]
        foot = _u([g for g in (x[0](1e-6) for x in sol if x[1] > 1e-6) if g is not None and not g.is_empty])
    except Exception: foot = None
    if foot is None:          # an odd solid: the old way, all at once
        pieces = sculpt(solids, cuts)
        if not pieces: return ("up", Polygon(), 0.0, kind)
        return ("steps", _u([p[0] for p in pieces]), pieces, kind) + ((parent,) if parent is not None else ())
    if foot.is_empty: return ("up", Polygon(), 0.0, kind)
    return ("steps", foot, _Later(lambda: sculpt(solids, cuts)), kind) + ((parent,) if parent is not None else ())

def b_keg(kind="wood", iron="iron"):
    """a barrel standing up: a round belly drawing in to the head, hoops, the chime standing proud, staves and a bung"""
    def f(cx, cy, a, rng):
        r = rng.uniform(1.3, 1.6); h = rng.uniform(1.9, 2.5); rb = r * 1.2
        body = lathe(cx, cy, [(rb, 0.05), (rb, h * 0.5)] + curve(rb, r, h * 0.5, h, 7), real=[(r * 0.98, 0.0)] + [(r + (rb - r) * math.sin(math.pi * j / 10) ** 0.8, h * j / 10) for j in range(1, 10)] + [(r, h)])
        staves = _u([_bar((cx + 0.3 * math.cos(a + t), cy + 0.3 * math.sin(a + t)), (cx + r * math.cos(a + t), cy + r * math.sin(a + t)), 0.2) for t in np.arange(0, 6.28, 6.283 / 9)]).intersection(_disc(cx, cy, r - 0.5, 24))
        lay = [_steps(body, kind), ("on", _ann(cx, cy, r + 0.2, r - 0.6, 28), 0, 0.22, kind)]
        if not staves.is_empty: lay.append(("tcut", staves, 0, 0.16))
        if rng.random() < 0.6: lay.append(("on", _disc(cx + r * 0.45 * math.cos(a + 1), cy + r * 0.45 * math.sin(a + 1), 0.28, 10), 0, 0.2, kind))
        return _disc(cx, cy, rb + 0.2, 24), lay
    f.tag, f.kind = "keg", kind
    return f

def _barrel_side(cx, cy, a, Ln, r, lift=0.0):
    """the nested pieces of a barrel lying on its side: fattest in the middle, two hoops standing proud"""
    hoop = lambda u: 0.13 * max(0.0, 1 - (abs(abs(u) - Ln * 0.29) / 0.34) ** 2)
    return loft(cx, cy, a, Ln, lambda u: r * (0.84 + 0.16 * math.cos(u / (Ln / 2) * 1.35)) + hoop(u), 1.2, lift=lift, under=True)

def b_barrel(kind="wood", iron="iron"):
    """a barrel on its side: round over the top, bulged in the middle, hoops standing proud, chocked so it cannot roll"""
    def f(cx, cy, a, rng):
        Ln = rng.uniform(3.6, 4.4); r = rng.uniform(1.15, 1.4); ff = frame(cx, cy, a)
        lay = [_steps(_barrel_side(cx, cy, a, Ln, r), kind)]
        if rng.random() < 0.7: lay.append(("up", rect(*ff(rng.uniform(-0.8, 0.8), (r + 0.5) * rng.choice((-1, 1))), 1.4, 0.95, a + rng.uniform(-0.3, 0.3)), 0.5, kind))
        return rect(cx, cy, Ln, 2 * r + 0.3, a), lay
    f.tag, f.kind = "barrel", kind
    return f

def b_barrels(kind="wood", iron="iron"):
    """barrels stacked on their sides: two below, one in the saddle between them"""
    def f(cx, cy, a, rng):
        Ln = rng.uniform(3.6, 4.2); r = rng.uniform(1.15, 1.35); ff = frame(cx, cy, a)
        ps = _barrel_side(*ff(0, -r - 0.2), a, Ln, r) + _barrel_side(*ff(rng.uniform(-0.2, 0.2), r + 0.2), a, Ln * 0.96, r) + _barrel_side(*ff(rng.uniform(-0.3, 0.3), 0), a, Ln, r, lift=r * 1.25 * 0.8)
        return rect(cx, cy, Ln + 0.4, 4 * r + 0.8, a), [_steps(ps, kind)]
    f.tag, f.kind = "barrels", kind
    return f

def _sack(cx, cy, a, L, W, h, lift=0.0):
    """a filled sack lying down: full in the middle, pinched at the tied neck"""
    full = lambda u: (1 - abs(2 * u / L) ** 3.2) ** 0.45 if abs(2 * u / L) < 1 else 0.0
    return loft(cx, cy, a, L, lambda u: W / 2 * full(u) * (1.0 if u < L * 0.2 else 1 - 0.5 * ((u - L * 0.2) / (L * 0.3)) ** 2), 2 * h / W, lift=lift) \
        + loft(*frame(cx, cy, a)(L / 2 + 0.25, 0), a, 0.9, lambda u: 0.34 + 0.14 * u / 0.45, 1.0 * h / W + 0.6, lift=lift)

def b_sacks(kind="sack"):
    """filled sacks: soft and full, slumped against each other, one thrown across the top, the tied necks sticking out"""
    def f(cx, cy, a, rng):
        ff = frame(cx, cy, a); ps = []; foot = []; n = rng.randint(2, 3); L = rng.uniform(3.2, 3.8); W = rng.uniform(2.1, 2.5); h = rng.uniform(0.85, 1.0)
        for i in range(n):
            c = ff(rng.uniform(-0.3, 0.3), (i - (n - 1) / 2) * (W - 0.1)); b = a + rng.uniform(-0.2, 0.2) + math.pi * rng.randint(0, 1)
            ps += _sack(c[0], c[1], b, L, W, h); foot.append(rect(c[0], c[1], L + 1.0, W, b))
        if n == 3 or rng.random() < 0.5:                                                          # one thrown across the others
            c = ff(rng.uniform(-0.3, 0.3), rng.uniform(-0.2, 0.2)); ps += _sack(c[0], c[1], a + math.pi / 2 + rng.uniform(-0.4, 0.4), L * 0.92, W, h, lift=h * 0.72)
        return _u(foot).convex_hull.buffer(0.2), [_steps(ps, kind)]
    f.tag, f.kind = "sacks", kind
    return f

def b_sack(kind="sack"):
    """one filled sack on its own"""
    def f(cx, cy, a, rng):
        L = rng.uniform(3.2, 3.8); W = rng.uniform(2.1, 2.5)
        return rect(cx, cy, L + 1.0, W, a), [_steps(_sack(cx, cy, a, L, W, rng.uniform(0.85, 1.0)), kind)]
    f.tag, f.kind = "sack", kind
    return f

def b_log(kind="wood"):
    """a felled log: round, thicker at the butt, a stub of a branch"""
    def f(cx, cy, a, rng):
        L = rng.uniform(6.0, 8.0); r = rng.uniform(0.95, 1.25); ff = frame(cx, cy, a); ps = []
        kn = rng.uniform(-L * 0.3, L * 0.3); ps += loft(cx, cy, a, L, lambda u: r * (1.0 - 0.16 * u / L) + 0.14 * math.exp(-((u - kn) / 0.5) ** 2), 1.15, under=True)      # thicker at the butt, a swelling where a knot is
        u = rng.uniform(-L * 0.2, L * 0.25); sv = rng.choice((-1, 1)); b = a + sv * rng.uniform(0.9, 1.3); c = ff(u, sv * (r + 0.5))
        ps += vault(c[0], c[1], b, 1.7, 1.0, 0.7)                                                                           # the branch stub
        return rect(cx, cy, L, 2 * r + 0.4, a), [_steps(ps, kind)]
    f.tag, f.kind = "log", kind
    return f

def b_logpile(kind="wood"):
    """cut logs stacked: three on the ground, one or two in the saddles above them"""
    def f(cx, cy, a, rng):
        L = rng.uniform(4.6, 5.8); r = rng.uniform(0.75, 0.9); ff = frame(cx, cy, a); ps = []
        for v in (-2 * r, 0, 2 * r): ps += vault(*ff(rng.uniform(-0.35, 0.35), v), a, L * rng.uniform(0.9, 1.0), 2 * r, r * 1.15, under=True)
        for v in (-r, r)[:rng.randint(1, 2)]: ps += vault(*ff(rng.uniform(-0.35, 0.35), v), a, L * rng.uniform(0.85, 1.0), 2 * r, r * 1.15, lift=r * 1.6, under=True)
        return rect(cx, cy, L + 0.5, 6 * r + 0.4, a), [_steps(ps, kind)]
    f.tag, f.kind = "logpile", kind
    return f

def b_urn(kind="rock"):
    """a turned pot or urn: a foot, a full belly, a drawn-in neck and a lip, hollow at the mouth"""
    def f(cx, cy, a, rng):
        r = rng.uniform(1.2, 1.55); h = rng.uniform(2.0, 2.7)
        prof = [(r * 0.62, 0.05), (r * 0.62, h * 0.08), (r, h * 0.42)] + curve(r, r * 0.5, h * 0.42, h * 0.84, 7) + [(r * 0.66, h * 0.9), (r * 0.66, h)]
        return _disc(cx, cy, r + 0.2, 20), [_steps(lathe(cx, cy, prof), kind, cuts=[(_disc(cx, cy, r * 0.66 - 0.3, 16), h - 0.55)])]
    f.tag, f.kind = "urn", kind
    return f

def b_urn_fallen(kind="rock"):
    """a pot knocked over and broken: the belly lying on its side, sherds beside its mouth"""
    def f(cx, cy, a, rng):
        r = rng.uniform(1.15, 1.4); ff = frame(cx, cy, a); ps = []; L = 2.6
        ps += loft(cx, cy, a, L, lambda u: r * (0.5 + 0.5 * math.sin(math.pi * min(1.0, (u + L / 2) / L * 1.25))), 1.2)                # belly towards the foot, the broken mouth open at the other end
        lay = [_steps(ps, kind)]
        for _ in range(rng.randint(2, 4)): c = ff(rng.uniform(L / 2 + 0.5, L / 2 + 1.6), rng.uniform(-1.3, 1.3)); lay.append(("up", stone(c[0], c[1], rng.uniform(0.55, 0.75), rng, 0.7, 4), 0.3, kind))
        return rect(*ff(0.6, 0), L + 2.2, 2 * r + 0.8, a), lay
    f.tag, f.kind = "urn", kind
    return f

def b_drum_can(kind="iron"):
    """a steel drum standing up: straight sides with two rolled ribs, a rimmed lid with a bung"""
    def f(cx, cy, a, rng):
        r = rng.uniform(1.3, 1.55); h = rng.uniform(2.1, 2.6)
        ps = lathe(cx, cy, [(r + 0.1, 0.0), (r + 0.1, h * 0.72), (r, h * 0.8), (r, h)])                                   # the top rolled rib is the widest thing seen from above
        lay = [_steps(ps, kind), ("on", _ann(cx, cy, r + 0.2, r - 0.62, 28), 0, 0.2, kind), ("on", _disc(cx + r * 0.4 * math.cos(a), cy + r * 0.4 * math.sin(a), 0.3, 10), 0, 0.18, kind)]
        return _disc(cx, cy, r + 0.25, 24), lay
    f.tag, f.kind = "drum", kind
    return f

def b_tank(kind="iron"):
    """a pressure tank on its side: a long cylinder with domed ends, bands and a filler cap, on two cradles"""
    def f(cx, cy, a, rng):
        L = rng.uniform(6.4, 7.8); r = rng.uniform(1.4, 1.7); ff = frame(cx, cy, a); e = r * 0.75
        dome = lambda u: 1.0 if abs(u) < L / 2 - e else math.sqrt(max(0.0, 1 - ((abs(u) - L / 2 + e) / e) ** 2))                  # the domed ends
        ps = loft(cx, cy, a, L, lambda u: r * dome(u) + (0.14 if abs(abs(u) - L * 0.24) < 0.27 else 0.0), 1.2, under=True)
        c = ff(rng.uniform(-0.5, 0.5), 0); ps += [(_disc(c[0], c[1], 0.5, 12), r * 1.2 + 0.3)]
        lay = [_steps(ps, kind), ("up", _u([rect(*ff(u, 0), 0.95, 2 * r + 1.0, a) for u in (-L * 0.24, L * 0.24)]), 0.5, kind)]
        return rect(cx, cy, L + 0.4, 2 * r + 1.0, a), lay
    f.tag, f.kind = "tank", kind
    return f

def b_chest(kind="crate", iron="iron"):
    """a strongbox: a round-topped lid, iron straps over it, a lock plate on the front"""
    def f(cx, cy, a, rng):
        L = rng.uniform(3.6, 4.4); W = rng.uniform(2.4, 2.8); h = rng.uniform(1.0, 1.3); ff = frame(cx, cy, a)
        ps = [(rect(cx, cy, L, W, a), h)] + vault(cx, cy, a, L, W, W * 0.42, "loaf", lift=h)
        for u in (-L * 0.3, L * 0.3): ps += [(rect(*ff(u, 0), 0.5, W + 0.24, a), h)] + vault(*ff(u, 0), a, 0.5, W + 0.24, W * 0.42 + 0.14, "loaf", lift=h)
        lay = [_steps(ps, kind), ("up", rect(*ff(0, -W / 2 - 0.2), 0.95, 0.95, a), h * 0.8, iron)]
        return rect(cx, cy, L + 0.3, W + 0.8, a), lay
    f.tag, f.kind = "chest", kind
    return f

def b_stump(kind="stump"):
    """a tree stump: flared where the roots run out, rings and a felling split on the cut face"""
    def f(cx, cy, a, rng):
        r = rng.uniform(1.4, 1.9); h = rng.uniform(1.2, 1.8); ps = lathe(cx, cy, curve(r * 1.45, r, 0.05, h * 0.7, 6, out=False) + [(r, h)], 14)
        for k in range(rng.randint(3, 5)):                                              # roots running out, each a low ridge falling away
            b = a + k * 6.283 / 4.3 + rng.uniform(-0.3, 0.3); Lr = r * rng.uniform(1.0, 1.5)
            d = r * 0.8 + Lr / 2; ps += loft(cx + d * math.cos(b), cy + d * math.sin(b), b, Lr, lambda u: 0.55 * (1 - (u + Lr / 2) / Lr * 0.8), h * 0.55 / 0.55, "ridge", 8)
        rings = _u([_ann(cx, cy, r * q, r * q - 0.2, 14) for q in (0.75, 0.45)] + [_bar((cx, cy), (cx + r * math.cos(a), cy + r * math.sin(a)), 0.22)]).intersection(_disc(cx, cy, r - 0.3, 14))
        return _disc(cx, cy, r * 2.2, 14), [_steps(ps, kind), ("tcut", rings, 0, 0.16)]
    f.tag, f.kind = "stump", kind
    return f

def b_wheel_up(kind="wood", iron="iron"):
    """a wheel standing on edge where it was left: the round of it seen from the side, the hub standing out"""
    def f(cx, cy, a, rng):
        R = rng.uniform(2.2, 2.7); ps = vault(cx, cy, a + math.pi / 2, 0.95, 2 * R, R * 0.92, n=14) + vault(cx, cy, a + math.pi / 2, 1.5, 0.9, R * 0.5)
        return rect(cx, cy, 2 * R, 1.6, a), [_steps(ps, kind)]
    f.tag, f.kind = "wheel", kind
    return f

def b_tent(kind="sail", wood="wood"):
    """a ridge tent: two cloth slopes to a ridge pole, upright ends, pegs along the sides"""
    def f(cx, cy, a, rng, k=1.0):
        L = rng.uniform(6.5, 8.0) * k; W = rng.uniform(5.2, 6.2) * k; h = W * 0.5; ff = frame(cx, cy, a)
        ps = vault(cx, cy, a, L, W, h, "ridge", n=14) + vault(cx, cy, a, L + 0.9, 0.5, h + 0.2, "loaf", n=3)               # the cloth, and the ridge pole standing out at each end
        lay = [_steps(ps, kind), ("up", _u([_disc(*ff(u, sv * (W / 2 + 0.75)), 0.5, 8) for u in (-L * 0.35, 0, L * 0.35) for sv in (-1, 1)]), 0.45, wood)]
        return rect(cx, cy, L + 1.2, W + 2.2, a), lay
    f.tag, f.kind = "tent", kind
    return f

def b_signpost(post="wood", board="wood"):
    """a signpost standing up: the board nailed across the top of the post, pointing somewhere"""
    def f(cx, cy, a, rng):
        ff = frame(cx, cy, a); h = rng.uniform(3.4, 4.2); b = a + rng.uniform(-0.4, 0.4); f2 = frame(cx, cy, b)
        arrow = Polygon([f2(-1.2, -0.3), f2(1.7, -0.3), f2(2.4, 0), f2(1.7, 0.3), f2(-1.2, 0.3)])
        return _disc(cx, cy, 2.6, 12), [_steps([(_disc(cx, cy, 0.42, 10), h)] + slab(arrow, h - 1.3, h - 0.25), post), ("mound", _disc(cx, cy, 0.9, 10), 0.35, "earth", ROCK_T)]
    f.tag, f.kind = "sign", post
    return f

def b_shrooms(kind="shroom", R=(2.4, 3.1)):
    """a ring of toadstools: a thin stem and a wide domed cap each"""
    def f(cx, cy, a, rng, k=1.0):
        Rr = rng.uniform(*R) * k; n = int(rng.randint(6, 9) * k); ps = []
        for i in range(n):
            if rng.random() < 0.12: continue
            x, y = cx + Rr * math.cos(a + 6.283 * i / n) + rng.uniform(-0.15, 0.15), cy + Rr * math.sin(a + 6.283 * i / n) + rng.uniform(-0.15, 0.15)
            rc = rng.uniform(0.5, 0.8); h = rng.uniform(0.9, 1.6)
            ps += lathe(x, y, curve(rc, 0.25, 0.0, h, 5), 12, real=[(0.2, 0.0), (0.17, h * 0.55), (rc, h * 0.58)] + curve(rc, 0.24, h * 0.62, h, 5))
        return _ann(cx, cy, Rr + 0.9, max(0.1, Rr - 0.9), 28), [_steps(ps, kind)] if ps else None
    f.tag, f.kind = "fairy_ring", kind
    return f

def b_table_rock(kind="rock"):
    """a balanced rock: a wide cap of stone on a narrow neck, the way wind leaves them"""
    def f(cx, cy, a, rng, k=1.0):
        r = rng.uniform(1.5, 2.1) * k; h = rng.uniform(2.6, 3.6) * max(1.0, k ** 0.5)
        hf = [(r * 0.75, 0.0), (r * 0.62, h * 0.5), (r * 0.9, h * 0.62), (r, h * 0.8), (r * 0.7, h)]
        return _disc(cx, cy, r + 0.3, 12), [_steps(lathe(cx, cy, hf, 9, 0.85, a, real=[(r * 0.72, 0.0), (r * 0.5, h * 0.25), (r * 0.42, h * 0.5), (r * 0.98, h * 0.64), (r, h * 0.8), (r * 0.72, h)]), kind)]
    f.tag, f.kind = "hoodoo", kind
    return f

def _several(build, n=(2, 4), spread=2.5):
    """a few of the same thing lying together at odd angles, as one feature"""
    def f(cx, cy, a, rng):
        foot = []; lay = []
        for i in range(rng.randint(*n)):
            for _ in range(12):
                x, y = cx + rng.uniform(-spread, spread), cy + rng.uniform(-spread, spread); ft, ly = build(x, y, rng.uniform(0, 6.283), rng)
                if all(ft.distance(g) > 0.3 for g in foot): foot.append(ft); n0 = len(lay); lay += [(L[:2] + (L[2] + n0,) + L[3:]) if L[0] in ("on", "tcut") else L for L in ly]; break
        return (_u(foot).convex_hull, lay) if foot else None
    f.tag, f.kind = getattr(build, "tag", ""), getattr(build, "kind", "")
    return f

def _either(f1, f2, share=0.5):
    """one of two ways to show the same thing, picked per piece"""
    def f(cx, cy, a, rng): return (f2 if rng.random() < share else f1)(cx, cy, a, rng)
    f.tag, f.kind = getattr(f1, "tag", ""), getattr(f1, "kind", "")
    return f

def _knuckle(r0, ends=0.75, w=0.11):
    """radius along a long bone: a shaft swelling into a knuckle at each end"""
    return lambda t: r0 * (1 + ends * math.exp(-(t / w) ** 2) + ends * math.exp(-((1 - t) / w) ** 2))

def _skull_parts(cx, cy, a, k, rng, jaw=True):
    """a skull lying face up, the way it is seen from above: the dome of the cranium, the brow ridge over two deep
    eye sockets, cheekbones, the nose hole, the upper jaw narrowing to the teeth. Returns (footprint, pieces, cuts):
    cuts are (polygon, floor height) hollows."""
    ff = frame(cx, cy, a); cc = ff(-0.72 * k, 0)
    ps = lathe(cc[0], cc[1], curve(1.38 * k, 0.3 * k, 0.0, 1.3 * k, 8), 24, 1.16 / 1.38, a, n=9)                    # the cranium: an egg, widest behind the temples
    fc = ff(1.1 * k, 0); Lf = 2.1 * k
    face = lambda u: k * (1.02 - 0.2 * ((u + Lf / 2) / Lf) - 0.26 * max(0.0, (u + Lf / 2) / Lf - 0.45) ** 1.3 / 0.55 ** 1.3)   # cheekbones wide, drawing in to the jaw
    ps += loft(fc[0], fc[1], a, Lf, face, 1.02, n=7)
    bc = ff(0.42 * k, 0); ps += vault(bc[0], bc[1], a + math.pi / 2, 1.95 * k, 0.55 * k, 1.18 * k, n=5)             # the brow ridge standing over the sockets
    for sv in (-1, 1):                                                                                            # cheekbones: a low arch out from each side
        ps += tube([ff(0.75 * k, sv * 0.78 * k), ff(1.05 * k, sv * 1.02 * k), ff(1.4 * k, sv * 0.8 * k)], 0.2 * k, 2.6, 4)
    ps += tube([ff(0.55 * k, 0), ff(1.0 * k, 0)], 0.17 * k, 5.2, 4)                                              # the bridge of the nose between the sockets
    cuts = [(_arot(_ascale(_disc(*ff(0.82 * k, sv * 0.5 * k), 0.36 * k, 14), 1.0, 0.85, origin=ff(0.82 * k, sv * 0.5 * k)), a + sv * 0.25, origin=ff(0.82 * k, sv * 0.5 * k), use_radians=True), 0.28 * k) for sv in (-1, 1)]
    cuts.append((Polygon([ff(1.08 * k, 0), ff(1.5 * k, -0.24 * k), ff(1.42 * k, 0), ff(1.5 * k, 0.24 * k)]).buffer(0.04), 0.3 * k))   # the nose hole, an upside-down heart
    nt = 7; ps += tube([ff(1.72 * k + 0.36 * k * math.cos(t), 0.62 * k * math.sin(t)) for t in [-1.25 + 2.5 * j / 12 for j in range(13)]], lambda t: 0.15 * k * (1 + 0.5 * max(0.0, math.cos(2 * math.pi * nt * t)) ** 2), 4.2, 5)      # the row of teeth round the front of the jaw
    foot = _u([_disc(cc[0], cc[1], 1.45 * k, 16), rect(fc[0], fc[1], Lf + 0.5, 2.3 * k, a)])
    if jaw and rng.random() < 0.55:                                                                               # the lower jaw lying where it fell: a horseshoe of bone
        jc = ff(2.6 * k + rng.uniform(0, 0.8 * k), rng.uniform(-1.2, 1.2) * k); jf = frame(jc[0], jc[1], a + rng.uniform(-0.9, 0.9))
        ps += tube([jf(-0.8 * k, -0.7 * k), jf(0.1 * k, -0.62 * k), jf(0.62 * k, 0), jf(0.1 * k, 0.62 * k), jf(-0.8 * k, 0.7 * k)], lambda t: 0.2 * k * (1.25 - abs(t - 0.5)), 1.5, 5)
        foot = _u([foot, _disc(jc[0], jc[1], 1.0 * k, 12)])
    return foot, ps, cuts

def b_skull(kind="bone", big=1.0):
    """a skull, face up: domed, with brow, eye sockets, cheekbones, nose hole and jaw"""
    def f(cx, cy, a, rng):
        k = big * rng.uniform(0.9, 1.15); foot, ps, cuts = _skull_parts(cx, cy, a, k, rng)
        return foot.convex_hull, [_steps(ps, kind, cuts=cuts)]
    f.tag, f.kind = "skull", kind
    return f

def _ribcage(cx, cy, a, Ln, kk, reach, rng):
    """a spine of knobbed vertebrae with ribs curving out and back from it, longest in the middle, tapering to the
    tips; some are missing and some snapped short"""
    ff = frame(cx, cy, a); bend = rng.uniform(-0.06, 0.06) * Ln; sp = lambda u: ff(u, bend * math.sin(math.pi * (u / Ln + 0.5)))
    nv = max(6, int(Ln / (1.25 * kk)))
    ps = tube([sp(-Ln / 2 + Ln * i / 16) for i in range(17)], lambda t: 0.44 * kk * (1 + 0.42 * max(0.0, math.cos(2 * math.pi * nv * t)) ** 3), 1.5, 6)
    n = max(4, int(Ln * 0.78 / (1.45 * kk))); sw = rng.uniform(0.5, 1.1) * kk
    for i in range(n):
        u = -Ln * 0.36 + (i + 0.5) * Ln * 0.72 / n; bell = math.sin(math.pi * (i + 0.5) / n) ** 0.7
        for sv in (-1, 1):
            q = rng.random()
            if q < 0.1: continue                                       # a rib is gone
            stop = rng.uniform(0.35, 0.6) if q < 0.24 else 1.0         # a rib snapped short
            rr_ = reach * (0.5 + 0.62 * bell) * rng.uniform(0.92, 1.06); x0, y0 = u, bend * math.sin(math.pi * (u / Ln + 0.5))
            path = [ff(x0 + sw * s ** 1.6, y0 + sv * rr_ * math.sin(2.05 * s) / 0.89) for s in [stop * j / 7 for j in range(8)]]
            ps += tube(path, lambda t, st=stop: kk * (0.42 - 0.17 * t * st), 1.7, 5, arch=rr_ * 0.55 * stop)          # with supports allowed the ribs arch up off the ground
    return ps

def b_ribs(kind="bone", L=(5.5, 7.5)):
    def f(cx, cy, a, rng, k=1.0):
        Ln = rng.uniform(*L) * k; kk = max(1.0, k ** 0.5); reach = 2.3 * max(1.0, k ** 0.7)
        ps = _ribcage(cx, cy, a, Ln, kk, reach, rng)
        return rect(cx, cy, Ln, 2 * reach * 1.15, a), [_steps(ps, kind)]
    f.tag, f.kind = "ribs", kind
    return f

def L_skeleton(kind="bone"):
    """the skeleton of something huge where it fell: skull, a long knobbed spine, a cage of curved ribs, the two wings
    of the pelvis, the long bones of the legs fallen out to the sides, the tail trailing away"""
    def f(cx, cy, a, rng, k):
        k9 = k * 0.9; Ln = rng.uniform(13, 17) * k9; kk = max(1.0, k9 ** 0.5); reach = 2.3 * max(1.0, k9 ** 0.7); ff = frame(cx, cy, a)
        ps = _ribcage(cx, cy, a, Ln, kk, reach, rng); foot = [rect(cx, cy, Ln, 2 * reach * 1.15, a)]
        for sv in (-1, 1):                                                                                        # the pelvis: two wings at the tail end
            pc = ff(-Ln / 2 - 0.2 * kk, sv * 1.25 * kk); ps += lathe(pc[0], pc[1], curve(1.45 * kk, 0.4 * kk, 0.0, 0.95 * kk, 6), 14, 0.62, a + sv * 0.55, n=6)
        tail = [ff(-Ln / 2 - 1.2 * kk - j * 1.25 * kk, (0.45 * kk * j * j / 4) * rng.choice((-1, 1)) if j else 0) for j in range(5)]      # the tail: vertebrae getting smaller, curling off
        tv = rng.choice((-1, 1)); tail = [ff(-Ln / 2 - 1.3 * kk - j * 1.2 * kk, tv * 0.16 * kk * j * j) for j in range(5)]
        for j, t in enumerate(tail): ps += lathe(t[0], t[1], curve((0.5 - 0.07 * j) * kk, 0.22 * kk, 0.0, (0.55 - 0.07 * j) * kk, 4), 10, n=4)
        foot.append(_u([Point(t).buffer(0.6 * kk) for t in tail]))
        for su, sv in ((-0.34, -1), (-0.34, 1), (0.3, -1), (0.3, 1)):                                              # leg bones fallen out to the sides
            if rng.random() < 0.25: continue
            b = a + sv * rng.uniform(0.9, 1.5) + rng.uniform(-0.3, 0.3); c0 = ff(su * Ln + rng.uniform(-0.6, 0.6) * kk, sv * (reach * 0.95 + rng.uniform(0.2, 0.9) * kk)); Lb = rng.uniform(3.0, 4.2) * kk
            c1 = (c0[0] + Lb * math.cos(b), c0[1] + Lb * math.sin(b)); ps += tube([c0, c1], _knuckle(0.3 * kk), 1.5, 5, under=True); foot.append(_bar(c0, c1, 1.2 * kk))
        hx, hy = ff(Ln / 2 + 2.0 * k ** 0.6, 0); ks = 1.9 * max(1.0, k ** 0.6) * rng.uniform(0.9, 1.1)
        sf, sp, cuts = _skull_parts(hx, hy, a + rng.uniform(-0.35, 0.35), ks, rng); foot.append(sf)
        return _u(foot).convex_hull, [_steps(ps + sp, kind, cuts=cuts)]
    return f

def b_bone(kind="bone"):
    """one long bone: a shaft with a knuckle at each end"""
    def f(cx, cy, a, rng, k=1.0):
        L = rng.uniform(2.6, 3.8) * k; ff = frame(cx, cy, a)
        return rect(cx, cy, L + 1.0, 1.4 * k, a), [_steps(tube([ff(-L / 2, 0), ff(L / 2, 0)], _knuckle(0.3 * max(1.0, k ** 0.5)), 1.4, 5, under=True), kind)]
    f.tag, f.kind = "bone", kind
    return f

def _grow(mk, m=1.3):
    """the made subjects are drawn near real size; as the subject of a base they stand a little larger"""
    def mk2(p):
        b = mk(p)
        return lambda cx, cy, a, rng, k: b(cx, cy, a, rng, k * m)
    return mk2

SUBJECTS = {
    "wall": _worn(lambda p: _turn(F.L_wall(kfix(p["stone"])), math.pi / 2)),
    "dais_brazier": lambda p: _turn(F.L_dais(p["floor"] or p["stone"], "brazier", "fire", p["iron"]), -math.pi / 7),
    "dais_column": lambda p: _turn(F.L_dais(p["floor"] or p["stone"], "column"), -math.pi / 7),
    "dais_altar": lambda p: _turn(F.L_dais(p["floor"] or p["stone"], "altar"), -math.pi / 7),
    "skeleton": lambda p: _turn(F.L_skeleton(p["bone"]), math.pi / 2),
    "crater": lambda p: F.L_crater(p["rock"] or p["stone"], p["pit"]),
    "colonnade": lambda p: _turn(F.L_colonnade(kfix(p["stone"]), p["floor"] or p["stone"]), math.pi / 2),
    "hull": lambda p: _turn(F.L_hull(kfix(p["wood"]), kfix(p["wood"])), math.pi / 2),
    "tree": lambda p: F.L_tree("stump", "root"),
    "circle": lambda p: F.L_circle("glyph", kfix(p["stone"])),
    "ruin": _worn(S_ruin), "pool": lambda p: F.L_pool(p["rock"] or p["stone"], p["water"]),
    "nest": lambda p: F.L_nest(p["twig"], "egg"),
    "spires": lambda p: F.L_spires(p["gem"] or p["rock"] or p["stone"]),
    "tentacles": lambda p: F.L_tentacles(p["vein"] if p["vein"] != "vein" or p["soft"] else "tendril", p["pit"]),
    "portal": _worn(S_portal), "shaft": _grow(S_shaft), "camp": _grow(S_camp), "depot": _grow(S_depot), "graves": _grow(S_graves), "wagon": _grow(S_wagon), "corral": S_corral,
    "machine": _grow(S_machine), "emplacement": S_emplacement, "tent": _grow(S_tent), "log": S_log, "well": _grow(S_well), "statue": _worn(_grow(S_statue)), "organ": S_organ,
}
SUBJECT_SAYS = {
    "wall": "a ruined wall", "dais_brazier": "a stepped platform with a brazier", "dais_column": "a stepped platform with a column", "dais_altar": "a stepped platform with an altar",
    "skeleton": "the skeleton of something huge", "crater": "a rimmed crater or pit", "colonnade": "two rows of columns with a paved way", "hull": "a wrecked boat, wagon bed or pod",
    "tree": "a great stump with roots", "circle": "a ritual circle with standing stones", "ruin": "a ruined building floor with broken walls", "pool": "a pool with stepping stones",
    "nest": "a great nest with eggs", "spires": "a cluster of crystal or rock spikes", "tentacles": "a pit with tentacles", "portal": "a doorway on the edge of the base",
    "shaft": "a square shaft with a windlass", "camp": "a camp fire with bedrolls", "depot": "crates and barrels stacked on a pallet", "graves": "a row of graves, one open",
    "wagon": "a broken wagon", "corral": "a fenced ring with a gate", "machine": "a machine with a tank and pipes", "emplacement": "a sandbagged gun pit",
    "tent": "a ridge tent", "log": "a fallen giant tree", "well": "a big stone well", "statue": "a plinth with a fallen statue", "organ": "a great soft organ with veins",
    "bridge": "a stream with a crossing",
}


# ====================================================================================================== props
def _orepile(rock, gem):
    def fn(s, rng, lvl=0):
        ps, z = place_group(s, rng, lambda cx, cy: cluster(s, rng, 7, lambda x, y: stone(x, y, rng.uniform(0.6, 1.0), rng, .7, 5), cx, cy, 1.6, 0.15), gap=0.4)
        if ps:
            for i, q in enumerate(ps): mound(s, [q], z, 0.7, ROCK_T, gem if (i % 2 and gem) else rock)
    fn.tmpl, fn.kind, fn.shape = "scatter", rock, "rock"
    return fn

def _props():
    C = F.comp; sc, si, su, tr, pa = D.scatter, D.single, D.sunk, D.trail, D.path
    tool = lambda w: (lambda p: C(F.b_tool(w, p["iron"], p["wood"])))
    R = lambda p: p["rock"] or p["stone"]
    out = {w: tool(w) for w in ("pick", "shovel", "axe", "sword", "spear", "rifle", "anchor", "wrench")}
    out.update({
        "wheel": lambda p: C(_either(F.b_wheel(p["wood"], p["iron"]), b_wheel_up(p["wood"], p["iron"]), 0.4)), "ladder": lambda p: C(F.b_ladder(p["wood"])), "cart": lambda p: C(F.b_cart(p["wood"], p["iron"])),
        "chest": lambda p: C(b_chest(p["crate"], p["iron"])), "sign": lambda p: C(b_signpost(p["wood"], p["wood"]) if SUPPORTS() else F.b_sign(p["wood"], p["wood"])), "barrel": lambda p: C(b_barrel(p["wood"], p["iron"])),
        "keg": lambda p: C(b_keg(p["wood"], p["iron"])), "crate": lambda p: C(b_crate(p["crate"], p["wood"])), "crates": lambda p: C(b_crate_stack(p["crate"], p["wood"])),
        "timbers": lambda p: C(F.b_pile(p["wood"], (4.2, 5.4), 1.0)), "planks": lambda p: sc(D.sh_caps(2.6, 3.6, 0.6), (2, 3), (0.5, 0.7), p["wood"], "up", 1.8),
        "beam": lambda p: si(D.sh_caps(5.0, 7.0, 1.0), (0.6, 0.8), p["wood"], "up", D.pat_seams(1.3)),
        "campfire": lambda p: C(F.b_campfire(R(p), p["wood"])), "brazier": lambda p: C(F.b_brazier(p["iron"], "fire")), "wellhead": lambda p: C(F.b_well(R(p), p["water"])),
        "skull": lambda p: C(F.b_skull(p["bone"], 1.2)), "ribs": lambda p: C(F.b_ribs(p["bone"])), "bones": lambda p: C(_several(b_bone(p["bone"]), (2, 4), 2.6)),
        "carcass": lambda p: C(lambda cx, cy, a, rng: F.L_skeleton(p["bone"])(cx, cy, a, rng, 0.45)),
        "grate": lambda p: C(F.b_grate(p["iron"])), "gear": lambda p: C(F.b_gear(p["iron"])), "lid": lambda p: C(F.b_boss(p["iron"], p["iron"])),
        "tiles": lambda p: C(F.b_tiles(p["floor"] or p["stone"])), "slab": lambda p: C(F.b_cracked_slab(p["stone"])), "steps": lambda p: C(F.b_stairs(p["floor"] or p["stone"])),
        "drum": lambda p: C(F.b_fallen_column(p["stone"])), "stub": lambda p: S2.f_column(p["stone"]), "runes": lambda p: C(F.b_rune_circle("glyph")),
        "crystals": lambda p: C(F.b_crystals(p["gem"])) if p["gem"] else None, "stump": lambda p: C(b_stump("stump")), "puddle": lambda p: su(D.sh_blob(1.3, 1.9), (0.3, 0.45), p["water"]),
        "nest": lambda p: C(F.b_nest(p["twig"], "egg")), "fairy": lambda p: C(b_shrooms("shroom") if SUPPORTS() else F.b_fairy_ring("shroom")), "tentacle": lambda p: C(F.b_tentacle(p["vein"] if p["vein"] != "vein" else "tendril")),
        "pothole": lambda p: C(F.b_crater(R(p), p["pit"])), "posts": lambda p: S2.f_post_row(p["wood"], (1.8, 2.3), True, (2, 3), 4.2), "headstone": lambda p: S2.f_headstone,
        "coffin": lambda p: S2.f_coffin, "lantern": lambda p: S2.f_lantern, "teeth": lambda p: S2.f_teeth, "sandbags": lambda p: S2.f_sandbags, "trapdoor": lambda p: S2.f_trapdoor,
        "vent": lambda p: S2.f_vent(p["pit"], p["stone"]), "sacks": lambda p: C(b_sacks("sack")), "sack": lambda p: C(b_sack("sack")), "barrels": lambda p: C(b_barrels(p["wood"], p["iron"])),
        "log": lambda p: C(b_log(p["wood"])), "logpile": lambda p: C(b_logpile(p["wood"])), "urn": lambda p: C(b_urn("clay")), "pot": lambda p: C(b_urn_fallen("clay")),
        "oildrum": lambda p: C(b_drum_can(p["iron"])), "tank": lambda p: C(b_tank(p["iron"])),
        "rope": lambda p: pa(0.5, (6, 10), 0.4, "rope", "up", 0.9), "chain": lambda p: pa(0.6, (7, 11), 0.4, "chain", "up", 1.0), "cables": lambda p: pa(0.6, (7, 11), 0.4, "cable", "up", 0.9),
        "casings": lambda p: sc(D.sh_caps(0.7, 1.0, 0.3), (5, 8), (0.3, 0.35), p["iron"], "up", 1.4), "coins": lambda p: sc(D.sh_disc(0.4, 0.5, 12), (5, 9), (0.3, 0.35), "gold", "up", 1.2),
        "eggs": lambda p: sc(D.sh_ell(0.6, 0.8, 0.9, 1.2), (3, 6), (0.7, 1.0), "egg", "mound", 1.6, D.SOFTT),
        "pustules": lambda p: sc(D.sh_disc(0.6, 0.9, 16), (3, 6), (0.7, 1.0), "pustule", "mound", 1.6, D.SOFTT),
        "orepile": lambda p: _orepile(R(p), p["gem"]), "holes": lambda p: su(D.sh_disc(0.4, 0.55, 10), (0.6, 0.8), "pit", (4, 6), 1.4),
        "rocks": lambda p: sc(D.sh_rock(0.9, 1.4), (3, 5), (0.6, 1.0), R(p)) if p["rock"] else None, "boulder": lambda p: C(b_table_rock(R(p))) if SUPPORTS() else si(D.sh_rock(1.8, 2.5), (1.1, 1.7), R(p), "mound", tiers=D.ROCKY),
        "pebbles": lambda p: sc(D.sh_round(0.5, 0.75), (6, 10), (0.35, 0.5), "pebble", "mound", 1.6, PEB_T),
        "rubble": lambda p: sc(D.sh_tri(0.7, 1.1), (4, 7), (0.4, 0.7), R(p), "mound", 1.8, D.ROCKY),
        "shrubs": lambda p: sc(D.sh_star(0.9, 1.3, 5, 8), (2, 4), (0.8, 1.2), p["plant"], "mound", 2.0, D.SOFTT) if p["plant"] else None,
        "bush": lambda p: si(D.sh_star(1.5, 2.1, 6, 9), (1.0, 1.4), p["plant"], "mound", tiers=D.SOFTT) if p["plant"] else None,
        "crack": lambda p: su(D.sh_wig(5, 8, 0.45, 0.9), (0.4, 0.5), "crack"), "prints": lambda p: tr(1.3, (5, 8)),
    })
    return out

PROPS = {}
PROP_NAMES = ("pick shovel axe sword spear rifle anchor wrench wheel ladder cart chest sign barrel keg crate crates timbers planks beam campfire brazier wellhead skull ribs "
              "bones carcass grate gear lid tiles slab steps drum stub runes crystals stump puddle nest fairy tentacle pothole posts headstone coffin lantern teeth sandbags "
              "trapdoor vent sacks sack barrels log logpile urn pot oildrum tank rope chain cables casings coins eggs pustules orepile holes rocks boulder pebbles rubble shrubs bush crack prints").split()
_SPECIAL = set("oildrum tank gear grate wrench vent cables casings rifle tentacle teeth pustules eggs anchor coins trapdoor coffin headstone sandbags fairy nest".split())
_EXTRA = {"mine": "gear cables trapdoor", "swamp": "tentacle eggs fairy nest anchor", "targa": "oildrum tank gear grate wrench vent cables", "trederra": "oildrum tank rifle casings sandbags gear cables",
          "beast": "tentacle teeth pustules eggs anchor nest", "badlands": "rifle nest", "highland": "fairy nest", "maahl": "nest", "corridor": "oildrum tank gear grate wrench vent cables casings rifle",
          "infestation": "oildrum tank gear grate wrench vent cables casings rifle tentacle pustules eggs nest", "urban": "oildrum tank gear grate wrench vent cables casings rifle sandbags",
          "frontier": "rifle casings nest", "graveyard": "coffin headstone fairy", "forest": "fairy nest", "dungeon": "coins trapdoor coffin grate", "temple": "coins grate",
          "coast": "anchor nest eggs", "cynder": "", "fortress": ""}

def _theme_parts(style):
    import themes
    subs, props, paths = set(), set(), set()
    for th in themes.THEMES.get(style, []):
        subs.add(th["subject"]); paths.add(th["path"].split(":")[0])
        props.update(w.split(":")[1].split("*")[0] for w in th["props"].split())
        subs.update(w.split(":")[0] for w in th.get("also", "").split())
    return subs, props, paths

def legal_props(style):
    pal = PAL[style]; ok = (set(PROP_NAMES) - _SPECIAL) | set(_EXTRA.get(style, "").split()) | _theme_parts(style)[1]
    return [n for n in PROP_NAMES if n in ok and prop_fn(n, pal) is not None]

def legal_subjects(style):
    ok = {"wall", "crater", "skeleton", "camp", "depot"} | _theme_parts(style)[0]
    return [n for n in list(SUBJECTS) + ["bridge"] if n in ok]

def legal_paths(style): return PATH_OK[style].split() + ["none"]

def prop_fn(name, pal):
    if not PROPS: PROPS.update(_props())
    mk = PROPS.get(name)
    if mk is None: return None
    try: return mk(pal)
    except Exception: return None


# ====================================================================================================== tags
TAGS = {    # hand-written features: (template, kind, shape, class). class: made / debris / growth / marks / line / late
    "b_wash": ("line", "wash", "channel", "line"), "b_hoodoo": ("single", "spire", "rock", "debris"), "b_mudcrack": ("sunk", "pan", "blob", "marks"),
    "b_agave": ("scatter", "agave", "star", "growth"), "b_washbed": ("scatter", "pebble", "round", "late"), "b_spiral": ("sunk", "glyph", "spiral", "late"),
    "b_split": ("single", "spire", "rock", "debris"), "b_scree": ("scatter", "rock", "rock", "late"),
    "h_bog": ("sunk", "water", "blob", "marks"), "h_steps": ("line", "slate", "stones", "line"), "h_wall": ("line", "slate", "wall", "line"), "h_moss": ("scatter", "moss", "blob", "growth"),
    "h_heather": ("scatter", "heather", "disc", "growth"), "h_standing": ("single", "standing", "rect", "made"), "h_stream": ("line", "water", "channel", "line"),
    "h_fern": ("scatter", "fern", "frond", "growth"),
    "m_ruin": ("scatter", "ruin", "rect", "made"), "m_shrub": ("scatter", "shrub", "star", "growth"), "m_sinkhole": ("sunk", "pit", "disc", "marks"),
    "m_crystal": ("scatter", "crystal", "shard", "debris"), "m_salt": ("sunk", "salt", "blob", "marks"), "m_obelisk": ("single", "obelisk", "bar", "made"),
    "m_column": ("single", "obelisk", "disc", "made"), "m_pavers": ("scatter", "ruin", "rect", "made"),
    "c_vent": ("single", "metal", "rect", "made"), "c_chevron": ("scatter", "paint", "chevron", "made"), "c_cable": ("line", "cable", "cable", "line"),
    "c_panel": ("single", "metal", "rect", "made"), "c_spill": ("sunk", "coolant", "blob", "marks"), "c_hexdeck": ("sunk", "slot", "hexnet", "marks"),
    "c_scorch": ("sunk", "scorch", "blob", "marks"), "c_lights": ("scatter", "lens", "rect", "made"),
    "i_eggs": ("scatter", "egg", "ell", "growth"), "i_acid": ("sunk", "acid", "blob", "marks"), "i_veins": ("line", "vein", "veins", "line"),
    "i_spores": ("scatter", "spore", "disc", "growth"), "i_hatched": ("scatter", "shell", "disc", "growth"), "i_torn": ("single", "plate", "shard", "late"),
    "i_ribs": ("scatter", "carapace", "rect", "growth"),
    "u_bricks": ("scatter", "brick", "rect", "debris"), "u_bullets": ("sunk", "pothole", "disc", "marks"), "u_wall": ("single", "brick", "rect", "debris"),
    "u_dashes": ("scatter", "paint", "rect", "line"), "u_scorch": ("sunk", "scorch", "blob", "marks"), "u_rebar": ("single", "concrete", "slab", "debris"),
    "u_tilt": ("scatter", "paver", "rect", "late"), "u_crater": ("sunk", "pothole", "crater", "marks"),
    "n_posts": ("line", "post", "rect", "line"), "n_ore": ("line", "quartz", "rock", "line"), "n_timbers": ("scatter", "wood", "rect", "made"),
    "n_seep": ("sunk", "water", "blob", "marks"), "n_orepile": ("scatter", "quartz", "rock", "made"), "n_shaft": ("sunk", "pit", "crater", "marks"),
    "n_barrel": ("single", "wood", "ring", "made"), "n_ladder": ("single", "wood", "ladder", "made"),
    "f_post_row": ("scatter", "post", "rect", "made"), "f_headstone": ("single", "headstone", "rect", "made"), "f_lantern": ("single", "lantern", "hex", "made"),
    "f_column": ("single", "column", "disc", "made"), "f_stump": ("single", "stump", "rock", "growth"), "f_teeth": ("scatter", "tooth", "tri", "growth"),
    "f_sandbags": ("scatter", "sandbag", "rect", "made"), "f_coffin": ("single", "coffin", "coffin", "made"), "f_trapdoor": ("single", "trap", "rect", "made"),
    "f_lily": ("scatter", "lily", "disc", "late"), "f_vent": ("single", "vent", "disc", "debris"),
}
DEBRIS = set("rock pebble sandstone slate basalt obsidian shelf garden rubble ice quartz brick concrete flag tcolumn crystal salt snow ember glass spire".split())
GROWTH = set("shrub grass flower fern moss heather reed shroom leaf weed agave flesh pustule egg tendril vein spore root lily stump mud earth tooth spine carapace shell".split())
CUTK = set("crack pit water rut scorch lava bile acid pothole slot glyph coolant seam pan wash tar hollow".split())
ABSTRACT = {"ring", "cross", "hex", "crescent", "star"}
_tagged = [False]

def _tag_all():
    if _tagged[0]: return
    _tagged[0] = True
    seen = set()
    for fn in [f for tiers in TIERS.values() for tier in tiers for f in tier] + [f for lst in D.DETAILS.values() for f in lst]:
        if id(fn) in seen: continue
        seen.add(id(fn)); nm = getattr(fn, "__qualname__", "").split(".")[0]
        if nm in TAGS:
            try: fn.tmpl, fn.kind, fn.shape, fn.cls = TAGS[nm]
            except AttributeError: pass

def klass(fn):
    """where a detail may go in a scene: made things only where the plan puts them, natural things where they gather"""
    c = getattr(fn, "cls", None)
    if c: return c
    tmpl, kind, shape = getattr(fn, "tmpl", ""), getattr(fn, "kind", ""), getattr(fn, "shape", "")
    if tmpl == "comp":          # multi-part features go by what they are, not by what they are made of
        b = shape.split(":")[0]
        return {"crystals": "debris", "fairy_ring": "growth", "roots": "growth", "tentacle": "growth", "skull": "bones", "ribs": "bones"}.get(b, "made")
    if kind == "bone": return "bones"
    if tmpl in ("sunk", "trail") or (tmpl == "path" and kind in CUTK): return "marks"
    if kind in DEBRIS: return "abstract" if shape in ("ring", "cross") else "debris"
    if kind in GROWTH: return "abstract" if shape in ("ring", "cross", "hex") else "growth"
    if tmpl in ("single", "scatter") and shape in ABSTRACT: return "abstract"
    return "made"


# ====================================================================================================== removing things
def _remove(s, dead):
    """take pieces off the base, with whatever stands on them and whatever is cut into them"""
    if not dead: return
    ids = {id(p) for p in dead}; tree = _Hits(dead)
    def rider(p, z0): return z0 > H + 0.05 and tree.hit(p.representative_point())
    ops = []
    for op in s.ops:
        ps = []
        for p in op[1]:
            if id(p) in ids: continue
            if op[-1] not in BRIDGE and op[-1] not in TERRK and rider(p, op[2]): ids.add(id(p)); continue
            ps.append(p)
        if ps: ops.append(op if len(ps) == len(op[1]) else (op[0], ps) + tuple(op[2:]))
    s.ops = ops
    s.chops = [c for c in getattr(s, "chops", []) if id(c["poly"]) not in ids]
    s.feet = [p for p in s.feet if id(p) not in ids]
    if hasattr(s, "objs"): s.objs = [p for p in s.objs if id(p) not in ids]
    if hasattr(s, "terr"): s.terr = [e for e in s.terr if id(e[0]) not in ids]
    s.tcuts = [(ps2, z0, z1, k) for ps, z0, z1, k in s.tcuts for ps2 in [[p for p in ps if not (z1 > H + 0.35 and tree.hit(p.representative_point()))]] if ps2]

def _cells(s): return {id(e[0]) for e in getattr(s, "terr", [])}

def clear_small(s, g):
    """drop the small loose pieces (under 10 mm2, never a floor plate) that touch g"""
    from shapely.prepared import prep
    gp = prep(g); cells = _cells(s)
    dead = [p for op in s.ops if op[-1] not in BRIDGE and op[-1] not in TERRK for p in op[1] if p.area < 10 and id(p) not in cells and gp.intersects(p)]
    _remove(s, dead)
    s.gcuts = [(ps2, d, k) for ps, d, k in s.gcuts for ps2 in [[p for p in ps if not (k == "pit" and p.area < 2.5 and gp.intersects(p))]] if ps2]

def clear_terrain(s, g):
    """stepped ground (bedrock slabs): a slab mostly under g goes, a slab g only clips is cut back, so g lies on flat ground"""
    if not terr(s) or not s.terr: return
    inop = {id(p) for op in s.ops if op[0] == "prism" and op[-1] in TERRK for p in op[1]}
    dead = []; swap = {}; gb = None
    for e in s.terr:
        fp = e[0]
        if id(fp) not in inop or not fp.intersects(g): continue
        a = fp.intersection(g).area
        if a < 1e-3: continue
        if a <= 0.15 * fp.area:
            gb = gb or g.buffer(0.35)
            q = [x for x in polys_of(fp.difference(gb)) if x.area > 3 and wide_enough(x, 1.6)]
            if len(q) == 1: swap[id(fp)] = q[0].simplify(0.03); continue
        dead.append(fp)
    if swap:
        s.ops = [(op[0], [swap.get(id(p), p) for p in op[1]]) + tuple(op[2:]) for op in s.ops]
        s.feet = [swap.get(id(p), p) for p in s.feet]
        s.terr = [(swap[id(e[0])], e[1], e[2], inset(swap[id(e[0])], 0.4)) if id(e[0]) in swap else e for e in s.terr]
    if dead:
        loose = [p for op in s.ops if op[-1] not in BRIDGE and op[-1] not in TERRK for p in op[1] if p.area < 10 and op[2] > H + 0.05 and any(d.contains(p.representative_point()) for d in dead)]
        _remove(s, dead + loose)

def _bigs(s):
    """raised things a subject or a path does not go through"""
    cells = _cells(s)
    return [p for op in s.ops if op[-1] not in SOFT and op[-1] not in BRIDGE and op[-1] not in TERRK for p in op[1] if p.area >= 10 and id(p) not in cells and op[2] <= H + 0.05]

def _top_at(s, x, y):
    """the highest flat-topped raised piece under (x, y): (its top z, its outline), or None. Prisms (floors, plates,
    blocks) and the placeholders of chamfered solids (their op keeps the solid's outline and top)."""
    pt = Point(x, y); best = None
    for op in s.ops:
        top = op[3] if op[0] == "prism" else op[2] + op[3] if op[-1] == "rockc" else None
        if top is None or top <= H + 0.05 or (best and top <= best[0]): continue
        for p in op[1]:
            if p.contains(pt) and p.area > 4.0: best = (top, p); break
    return best

def _plates(s):
    """floor plates by height: [(top z, union)]"""
    lv = {}
    for op in s.ops:
        if op[0] == "prism" and op[-1] in BRIDGE: lv.setdefault(round(op[3], 3), []).extend(op[1])
    return [(z, unary_union(ps)) for z, ps in sorted(lv.items(), reverse=True)]

from shapely.geometry import box as _shapely_box

def _strips(g, step=3.0):
    if g.is_empty: return []
    x0, y0, x1, y1 = g.bounds; out = []
    for x in np.arange(x0, x1, step):
        for y in np.arange(y0, y1, step): out += polys_of(g.intersection(_shapely_box(x, y, x + step, y + step)))          # shapely's box: in this file box() is the kit's own solid (it raised TypeError, and paths over terrain were dropped)
    return out


# ====================================================================================================== laying paths
def lay_up(sc, layers, loose=False):
    """raised strips that share one footprint (ties and rails): on bare ground, through floor plates, over terrain"""
    s = sc.s; layers = [(g.intersection(sc.wide), h, kfix(k) if not loose else k) for g, h, k in layers if g is not None and not g.is_empty]
    foot = _u([g for g, h, k in layers])
    if foot.is_empty: return foot
    ob = _u([p for p in _bigs(s) if p.intersects(foot)] + ([sc.core] if sc.core is not None else []))
    if not ob.is_empty: foot = foot.difference(ob.buffer(0.35))
    if foot.is_empty: return foot
    if terr(s): clear_terrain(s, foot.buffer(0.5))
    clear_small(s, foot.buffer(0.15)); evict(s, foot)
    plates = [] if terr(s) else _plates(s)
    for g, h, kind in layers:
        g = g.intersection(foot)
        if terr(s):
            flat = [q for q in polys_of(g) if T_zr(s, q) == (H, H)]; rest = [q for q in polys_of(g) if T_zr(s, q) != (H, H)]
            ps = [q for q in flat + [x for r in rest for x in _strips(r)] if wide_enough(q) and q.area > 0.5]
            if ps: tprism(s, ps, h, kind)
            continue
        rest = g
        for zt, pl in plates:
            part = [q for q in polys_of(rest.intersection(pl)) if wide_enough(q) and q.area > 0.5]
            if part: z = Z(zt); z.bridge = True; up(s, part, z, h, kind)
            rest = rest.difference(pl)
        ps = [q for q in polys_of(rest) if wide_enough(q) and q.area > 0.5]
        if ps: up(s, ps, H, h, kind)
    return foot

def lay_cut(sc, g, depth, kind, meet=False):
    """a cut path (ruts, trail, stream) into bare ground, into floor plates, or into the flat parts of terrain"""
    s = sc.s; g = g.intersection(sc.wide)
    ob = _u([p for p in _bigs(s) if p.intersects(g)] + ([sc.core] if sc.core is not None and not meet else []))
    if not ob.is_empty: g = g.difference(ob.buffer(0.35))
    if g.is_empty: return g
    if terr(s): clear_terrain(s, g.buffer(0.4))
    clear_small(s, g.buffer(0.15)); evict(s, g)
    if terr(s):
        tu = _u([e[0] for e in s.terr if e[0].intersects(g)])
        flat = g.difference(tu) if not tu.is_empty else g
        if not flat.is_empty: s.gcut(polys_of(flat), depth, kind)
        if not tu.is_empty: tcut_flat(s, _strips(g.intersection(tu)), depth, kind)
        return g
    rest = g
    for zt, pl in _plates(s):
        part = polys_of(rest.intersection(pl))
        if part: s.tcut(part, zt - depth, zt + 0.3, kind)
        rest = rest.difference(pl)
    if not rest.is_empty: s.gcut(polys_of(rest), depth, kind)
    return g

def _at(line, u):
    u = max(0.0, min(line.length, u)); p = line.interpolate(u); q = line.interpolate(min(line.length, u + 0.3)); p0 = line.interpolate(max(0.0, u - 0.3))
    return p.x, p.y, math.atan2(q.y - p0.y, q.x - p0.x)
def _side(line, d, w): return line.offset_curve(d).buffer(w / 2, cap_style=2)
def _var(line, rng, w0, w1, step=0.9):
    g = unary_union([_disc(*_at(line, u)[:2], rng.uniform(w0, w1) / 2, 10) for u in np.arange(0, line.length + step, step)])
    return g.buffer(0.5).buffer(-0.5).simplify(0.04)

def P_ruts(sc, line, rng): return lay_cut(sc, _u([_side(line, d, 0.5) for d in (-1.3, 1.3)]), 0.4, "rut")
def P_trail(sc, line, rng):
    g = lay_cut(sc, _var(line, rng, 3.5, 4.5), 0.3, "rut")
    prints = []
    for i, u in enumerate(np.arange(2.5, line.length - 1.0, 1.35)):
        x, y, a = _at(line, u); c = frame(x, y, a)(0, 0.5 if i % 2 else -0.5)
        prints.append(affinity.rotate(affinity.scale(_disc(c[0], c[1], 1, 10), 0.62, 0.36), math.degrees(a), origin=c))
    core = inset(g, 0.5) if not g.is_empty else g
    ps = [q for q in prints if core.contains(q)]
    if ps and not terr(sc.s) and not _plates(sc.s): sc.s.gcut(ps, 0.55, "pit")
    return g
def P_track(sc, line, rng):
    ties = _u([rect(x, y, 0.95, 3.6, a + rng.uniform(-0.05, 0.05)) for x, y, a in (_at(line, u) for u in np.arange(0.8, line.length, 1.65))])
    return lay_up(sc, [(ties, 0.4, sc.pal["wood"]), (_u([_side(line, d, 0.92) for d in (-1.15, 1.15)]), 0.85, sc.pal["iron"])])
def P_boards(sc, line, rng):
    pl = _u([rect(x, y, 1.0, rng.uniform(3.5, 4.0), a + rng.uniform(-0.05, 0.05)) for x, y, a in (_at(line, u) for u in np.arange(0.7, line.length, 1.22)) if rng.random() > 0.07])
    return lay_up(sc, [(_u([_side(line, d, 0.92) for d in (-1.35, 1.35)]), 0.35, sc.pal["wood"]), (pl, 0.7, sc.pal["floor"] if sc.pal["floor"] in ("board", "duck", "plank", "log", "drift") else sc.pal["wood"])])
def P_stream(sc, line, rng):
    w = rng.uniform(2.6, 3.6) * max(1.0, gg.K) ** 0.3; sc.s.wash = line
    return lay_cut(sc, _var(line, rng, w * 0.8, w * 1.25), 0.55, sc.pal["water"], meet=True)
def P_stones(sc, line, rng):
    kind = sc.pal["rock"] or sc.pal["stone"]; inn = Point(0, 0).buffer(gg.R0 - 1.0, 64); sts = []
    for i, u in enumerate(np.arange(1.5, line.length, 2.6)):
        x, y, a = _at(line, u); c = frame(x, y, a)(rng.uniform(-0.3, 0.3), 0.7 if i % 2 else -0.7); q = stone(c[0], c[1], rng.uniform(1.0, 1.3), rng, 0.85, rng.randint(6, 8))
        if inn.contains(q): sts.append(q)
    wet = _u([p for ps, d, k in sc.s.gcuts if k in WETK for p in ps])
    pads = [q for q in sts if not wet.is_empty and inset(wet, 0.3).contains(q)]; dry = [q for q in sts if wet.is_empty or not q.intersects(wet.buffer(0.3))]
    if pads: gg.COL.setdefault(kind + "_pad", gg.COL.get(kind, "#999999")); sc.s.prism(pads, H - 0.12, kind + "_pad", z0=H - 0.8, foot=False)
    return lay_up(sc, [(_u(dry), 0.4, kind)], loose=True).union(_u(pads))
def P_way(sc, line, rng):
    strip = line.buffer(2.25, cap_style=2)
    cuts = [rect(x, y, 0.28, 6, a) for x, y, a in (_at(line, u) for u in np.arange(0.0, line.length, 3.0))] + [line.buffer(0.14, cap_style=2)]
    slabs = [q for q in polys_of(strip.difference(_u(cuts))) if q.area > 1.5 and wide_enough(q, 1.2)]
    return lay_up(sc, [(_u(slabs), 0.45 if sc.paved else 0.3, sc.pal["floor"] or sc.pal["stone"])])
def P_pipe(sc, line, rng):
    col = _u([rect(x, y, 0.95, 2.4, a) for x, y, a in (_at(line, u) for u in np.arange(2.0, line.length, 5.0))])
    return lay_up(sc, [(line.buffer(0.8, cap_style=2), 0.9, "pipe"), (col, 0.98, "pipe")])
def P_cable(sc, line, rng):
    out = []
    for d in rng.sample((-1.0, 0.0, 1.0), rng.randint(2, 3)):
        fq = rng.uniform(0.25, 0.45); ph = rng.uniform(0, 6.28)
        pts = [frame(x, y, a)(0, d + 0.3 * math.sin(fq * u + ph)) for u, (x, y, a) in ((u, _at(line, u)) for u in np.arange(0, line.length + 0.7, 0.7))]
        out.append(LineString(pts).buffer(0.46, 4))
    return lay_up(sc, [(_u(out), 0.4, "cable")])
def P_veins(sc, line, rng):
    L = line.length; parts = []; us = list(np.arange(0, L, 1.2)) + [L]
    for u0, u1 in zip(us, us[1:]):
        w = 0.95 + 1.2 * (u0 / L); x0, y0, a0 = _at(line, u0); x1, y1, a1 = _at(line, u1)
        parts.append(_cap(frame(x0, y0, a0)(0, 0.35 * math.sin(u0 * 0.5)), frame(x1, y1, a1)(0, 0.35 * math.sin(u1 * 0.5)), w))
    for u in np.arange(4.0, L - 3.0, rng.uniform(5.0, 7.5)):
        x, y, a = _at(line, u); b = a + rng.choice((-1, 1)) * rng.uniform(0.6, 1.0) + math.pi; Ln = rng.uniform(3.0, 5.5)
        pts = wiggle((x, y), (x + Ln * math.cos(b), y + Ln * math.sin(b)), rng, 0.5, 3)
        parts += [_cap(pts[j], pts[j + 1], 0.95) for j in range(len(pts) - 1)]
    return lay_up(sc, [(_u(parts), 0.6, sc.pal["vein"])])

PATHS = {"ruts": P_ruts, "trail": P_trail, "track": P_track, "boards": P_boards, "stream": P_stream, "stones": P_stones, "way": P_way, "pipe": P_pipe, "cable": P_cable, "veins": P_veins}
PATH_W = {"ruts": 1.6, "trail": 2.2, "track": 1.9, "boards": 2.0, "stream": 2.2, "stones": 1.9, "way": 2.3, "pipe": 1.2, "cable": 1.5, "veins": 1.2}
PATH_SAYS = {"ruts": "wagon ruts", "trail": "a worn footpath", "track": "a rail track", "boards": "a boardwalk", "stream": "a stream of the style's liquid", "stones": "stepping stones",
             "way": "a paved way", "pipe": "a pipe run", "cable": "cables or a fuse", "veins": "a thick vein or root", "none": "no path"}

def _curve(p0, ctrl, p1):
    L = math.dist(p0, ctrl) + math.dist(ctrl, p1); n = max(8, int(L / 0.5))
    return LineString([((1 - t) ** 2 * p0[0] + 2 * t * (1 - t) * ctrl[0] + t * t * p1[0], (1 - t) ** 2 * p0[1] + 2 * t * (1 - t) * ctrl[1] + t * t * p1[1]) for t in (i / n for i in range(n + 1))])


# ====================================================================================================== relations
def s_near(g, d0, d1, side=None, c=None, front=None, n=4):
    """a spot d0..d1 mm outside g (inside it for negative distances); side keeps the front or the back half"""
    if g is None or g.is_empty: return None
    rings = [p.exterior for d in np.linspace(d0, d1, n) for p in polys_of(g.buffer(d, 6))]
    if not rings: return None
    w = [r.length for r in rings]
    def f(rng):
        for _ in range(16):
            r = rng.choices(rings, w)[0]; pt = r.interpolate(rng.random() * r.length)
            if side and ((pt.x - c[0]) * front[0] + (pt.y - c[1]) * front[1] > 0) != (side == "front"): continue
            break
        return pt.x, pt.y
    return f
def s_rim(d0, d1):
    return lambda rng: (lambda a, r: (r * math.cos(a), r * math.sin(a)))(rng.uniform(0, 6.2832), gg.R0 - rng.uniform(d0, d1))
def s_in(g):
    x0, y0, x1, y1 = g.bounds
    def f(rng):
        for _ in range(30):
            x, y = rng.uniform(x0, x1), rng.uniform(y0, y1)
            if g.contains(Point(x, y)): break
        return x, y
    return f
def s_far(avoid, d):
    R = gg.R0 - 2.0
    def f(rng):
        for _ in range(30):
            a = rng.uniform(0, 6.2832); r = R * math.sqrt(rng.random()); x, y = r * math.cos(a), r * math.sin(a)
            if avoid.is_empty or avoid.distance(Point(x, y)) > d: break
        return x, y
    return f
def s_mix(ws):
    ws = [(w, f) for w, f in ws if f is not None and w > 0]
    if not ws: return None
    return lambda rng: rng.choices([f for w, f in ws], [w for w, f in ws])[0](rng)
def s_not(f, avoid, d):
    """f, but never within d mm of avoid"""
    if f is None or avoid is None or avoid.is_empty: return f
    def g(rng):
        for _ in range(12):
            x, y = f(rng)
            if avoid.distance(Point(x, y)) > d: break
        return x, y
    return g

def a_par(g, jit=0.12):
    ext = [p.exterior for p in polys_of(g)]
    def f(rng, x, y, a):
        if not ext: return a
        pt = Point(x, y); r = min(ext, key=lambda e: e.distance(pt)); u = r.project(pt)
        p0 = r.interpolate((u - 0.7) % r.length); p1 = r.interpolate((u + 0.7) % r.length)
        return math.atan2(p1.y - p0.y, p1.x - p0.x) + rng.uniform(-jit, jit)
    return f
def a_to(c, jit=0.15): return lambda rng, x, y, a: math.atan2(c[1] - y, c[0] - x) + rng.uniform(-jit, jit)
def a_rim(rng, x, y, a): return math.atan2(y, x) + math.pi / 2 + rng.uniform(-0.15, 0.15)

class focus:
    """while open, every detail takes its spot from `at` and its angle from `aim`"""
    def __init__(self, at, aim_=None): self.at, self.aim = at, aim_
    def __enter__(self): self.keep = (FOCUS[0], AIM[0]); FOCUS[0], AIM[0] = self.at, self.aim; return self
    def __exit__(self, *a): FOCUS[0], AIM[0] = self.keep; return False


class Sc:
    """the state of one scene while it is put together"""
    def __init__(self, s, style, mixr, rng, t):
        self.s, self.style, self.mixr, self.rng, self.t = s, style, mixr, rng, t
        self.mm = gg.BASE_MM; self.K = gg.K; self.R = gg.R0; self.pal = PAL[style]
        self.wide = Point(0, 0).buffer(gg.R0 + 1.6, 128); self.disc = Point(0, 0).buffer(gg.R0 - 0.7, 128)
        self.core = None; self.subj = None; self.solid = Polygon(); self.c = (0.0, 0.0); self.front = (1.0, 0.0); self.p1 = (0.0, 0.0)
        self.mini = mini_type(mixr); self.wet = None
        self.pathg = None; self.line = None; self.start = None; self.yard = None; self.also = []; self.log = []; self.taken = []; self.focal = None; self.marks_later = []

    def relation(self, where):
        """(sampler, aim) for a relation name; (None, None) when this scene has nothing of that kind"""
        sub, pg = self.subj, self.pathg
        if where in ("path", "start") and pg is None: where = "at"
        if where == "also" and not self.also: where = "at"
        if where == "yard" and self.yard is None: where = "front"
        if sub is None and where in ("at", "wall", "front", "back", "in"): return None, None
        if where == "at": return s_near(sub, 1.2, 5.5), None
        if where == "wall": return s_near(sub, 1.0, 3.0), a_par(sub.convex_hull)
        if where == "front": return s_not(s_near(sub, 1.5, 5.0, "front", self.c, self.front), self.yard, 0.3), a_to(self.c)
        if where == "back": return s_near(sub, 1.2, 5.0, "back", self.c, self.front), a_par(sub.convex_hull)
        if where == "in":
            g = sub.buffer(-1.6)
            return (s_in(g), None) if not g.is_empty else (s_near(sub, 1.2, 4.0), None)
        if where == "path": return s_near(pg.intersection(self.disc), 1.0, 4.5), a_par(pg, 0.44)
        if where == "start":
            g = pg.intersection(Point(self.start).buffer(9.0)).intersection(self.disc)
            return (s_near(g, 1.0, 4.0) if not g.is_empty else s_near(pg.intersection(self.disc), 1.0, 4.5)), a_par(pg, 0.3)
        if where == "yard": return s_near(self.yard, -0.5, 2.5), a_to(tuple(self.yard.centroid.coords[0]))
        if where == "rim": return self.relation("back" if sub is not None else "far")          # nothing is lined up along the edge any more: "rim" means behind the subject
        if where == "also": return s_near(_u(self.also), 1.2, 4.5), None
        if where == "far": return s_far(_u([sub, pg]), 8.0 if self.mm < 99 else 10.0), None
        return None, None


# ====================================================================================================== the plan
def _num(n):
    try: return int(str(n).rsplit("_", 1)[-1])
    except ValueError: return 1

def theme_order(style, salt):
    import themes
    idx = list(range(len(themes.THEMES.get(style, [])))); _random.Random(f"themes{style}{salt}").shuffle(idx); return idx

def expand(th, mixr, mm, style=""):
    """a theme line from themes.py -> the full plan dict, with this base's own angles, sizes and pick of props"""
    sa = mixr.uniform(0, 360); kind, _, route = th["path"].partition(":")
    lines = th["props"].split()
    keep = max(1, round(len(lines) * mixr.uniform(0.6, 1.0))) if len(lines) > 2 else len(lines)
    chosen = set(mixr.sample(range(len(lines)), keep)); props = []
    for i, w in enumerate(lines):
        if i not in chosen: continue
        where, what = w.split(":"); what, _, cnt = what.partition("*")
        props.append({"what": what, "where": where, "count": int(cnt or 1)})
    na = _na(mm)
    also = [{"name": w.split(":")[0], "where": w.split(":")[1], "size": round(mixr.uniform(0.62, 0.85), 2)} for w in th.get("also", "").split()][:na]
    if len(also) < na and style:          # a big base holds more than one thing: other places of the same style, smaller and off to the side
        import themes
        more = sorted(({t["subject"] for t in themes.THEMES.get(style, [])} & ALSO_OK) - {th["subject"]} - {a["name"] for a in also})
        mixr.shuffle(more)
        also += [{"name": m, "where": mixr.choice(("far", "far", "path")), "size": round(mixr.uniform(0.5, 0.7), 2)} for m in more[:na - len(also)]]
    cl = dict(debris=1.0, growth=1.0, marks=1.0); cl.update(th.get("clutter", {}))
    plan = {"theme": th["theme"], "subject": {"name": th["subject"], "edge": th["subject"] == "portal", "angle": round(sa, 1), "offset": round(mixr.uniform(0.12, 0.42), 2), "size": round(mixr.uniform(0.85, 1.15), 2),
                                           "height": round(mixr.uniform(1.3, 3.0), 1) if th["subject"] in TALL else 1.0},
            "path": {"kind": kind, "route": route or "to", "from_angle": round((sa + mixr.uniform(110, 250)) % 360, 1), "bend": round(mixr.uniform(-0.22, 0.22), 2)},
            "also": also, "props": props, "clutter": cl, "open": float(th.get("open", 1.0)),
            "rim": {"cover": round(mixr.uniform(0.12, 0.24), 2) if (style in ENCLOSED or (style in CLIFFS and mixr.random() < 0.35)) else 0.0, "height": round(mixr.uniform(1.5, 3.5), 1)}}
    if th.get("pal"): plan["pal"] = dict(th["pal"])
    if th.get("ban"): plan["ban"] = list(th["ban"])
    return plan

def theme_names(style):
    import themes
    return [t["theme"] for t in themes.THEMES.get(style, [])]

def pick_plans(style, mixr, mm, n):
    """the plans to try for base n, in order: this base's theme first, then the others (a theme whose subject finds no
    room gives way to the next). Base numbers walk through a shuffled list, so a run uses every theme before one repeats."""
    import themes
    ths = themes.THEMES.get(style, [])
    if not ths: return []
    forced = os.environ.get("THEME", "").strip().lower()
    if forced:
        hit = [t for t in ths if t["theme"].lower() == forced]
        if hit: return [expand(hit[0], mixr, mm, style)]
        raise SystemExit(f"unknown theme '{forced}' for {style}. Themes: " + ", ".join(t["theme"] for t in ths))
    order = theme_order(style, os.environ.get("SALT", "")); k = ((n - 1) % 5 + (n - 1) // 5) % len(order)
    return [expand(ths[i], mixr, mm, style) for i in order[k:] + order[:k]]

def _clamp(v, lo, hi, dflt):
    try: v = float(v)
    except (TypeError, ValueError): return dflt, True
    return min(hi, max(lo, v)), not (lo <= v <= hi)

def validate_plan(plan, style, mm, used=()):
    """check a plan from any source and repair what can safely be repaired. Returns (plan, notes); plan is None when
    it cannot be built (unknown subject, nothing usable in it, or a theme already used in this run)."""
    notes = []
    if not isinstance(plan, dict): return None, ["not a plan"]
    try: p = json.loads(json.dumps(plan))
    except (TypeError, ValueError): return None, ["not a plan"]
    pal = dict(PAL[style]); pal.update({k: v for k, v in (p.get("pal") or {}).items() if k in pal and isinstance(v, str)})
    p["theme"] = " ".join(str(p.get("theme") or "").split())[:40] or "a place"
    if p["theme"].lower() in {u.lower() for u in used}: return None, [f"theme '{p['theme']}' already used"]
    sub = p.get("subject") if isinstance(p.get("subject"), dict) else {}
    name = sub.get("name")
    if name != "bridge" and name not in SUBJECTS: return None, [f"unknown subject '{name}'"]
    edge = bool(sub.get("edge", False))
    if name == "portal" and not edge: edge = True; notes.append("portal moved to the rim")
    if edge and name not in EDGE_OK: edge = False; notes.append(f"{name} cannot sit on the rim")
    fixes = 0
    ang, b = _clamp(sub.get("angle"), 0, 360, 0.0); fixes += b
    off, b = _clamp(sub.get("offset", 0.3), 0.0, 0.5, 0.3); fixes += b
    size, b = _clamp(sub.get("size", 1.0), 0.6, 1.6 if mini_type() == "hover" else 1.3, 1.0); fixes += b
    hgt, b = _clamp(sub.get("height", 1.0), 0.5, 20.0, 1.0); fixes += b          # no height limit: 20 is only a guard against a typing slip
    if name not in TALL and name != "bridge" and hgt > 1.5: hgt = 1.5; notes.append(f"{name} lies low: height 1.5")
    p["subject"] = {"name": name, "edge": edge, "angle": ang, "offset": off, "size": size, "height": hgt}
    pth = p.get("path") if isinstance(p.get("path"), dict) else {}
    kind = pth.get("kind", "none"); ok = legal_paths(style)
    if kind not in PATHS and kind != "none": return None, [f"unknown path '{kind}'"]
    if name == "bridge" and kind in ("none", "stream"): kind = "stones"; notes.append("bridge needs a crossing: stones")
    if kind not in ok: notes.append(f"path {kind} does not suit {style}: {ok[0]}"); kind = ok[0]
    route = pth.get("route") if pth.get("route") in ("to", "past") else "to"
    if kind == "stream" and route == "to" and name not in ("pool", "well", "crater", "tentacles"): route = "past"; notes.append("a stream runs past, not to")
    fa, b = _clamp(pth.get("from_angle", ang + 180), 0, 360, (ang + 180) % 360); fixes += b
    if abs((fa - ang + 180) % 360 - 180) < (110 if edge else 70): fa = (ang + 180) % 360; notes.append("path start moved to the far side")
    bend, b = _clamp(pth.get("bend", 0.0), -0.3, 0.3, 0.0); fixes += b
    p["path"] = {"kind": kind, "route": route, "from_angle": fa, "bend": bend}
    props = []; raw = p.get("props") if isinstance(p.get("props"), list) else []
    for pr in raw:
        if not isinstance(pr, dict): continue
        what, where = pr.get("what"), pr.get("where", "at")
        if what not in PROP_NAMES or prop_fn(what, pal) is None: notes.append(f"dropped unknown object '{what}'"); continue
        if where not in RELATIONS: notes.append(f"dropped '{what}': no such place '{where}'"); continue
        cnt, b = _clamp(pr.get("count", 1), 1, 4, 1); fixes += b
        props.append({"what": what, "where": where, "count": int(round(cnt))})
    if raw and not props: return None, notes + ["no usable objects"]
    if len(props) > 12: props = props[:12]; notes.append("kept the first 12 object lines")
    tot = 0; kept = []
    for pr in props:
        if tot + pr["count"] > 10 * max(1.0, gg.K): notes.append("too many objects: rest dropped"); break
        tot += pr["count"]; kept.append(pr)
    p["props"] = kept
    na = _na(mm); also = []
    for al in (p.get("also") if isinstance(p.get("also"), list) else []):
        if isinstance(al, dict) and al.get("name") in SUBJECTS and al.get("name") != "portal" and al.get("where", "far") in RELATIONS:
            sz, _ = _clamp(al.get("size", 0.7), 0.4, 1.0, 0.7); also.append({"name": al["name"], "where": al.get("where", "far"), "size": sz})
    if len(also) > na: also = also[:na]; notes.append(f"second features cut to {na}")
    p["also"] = also
    cl = p.get("clutter") if isinstance(p.get("clutter"), dict) else {}
    p["clutter"] = {k: _clamp(cl.get(k, 1.0), 0, 2, 1.0)[0] for k in ("debris", "growth", "marks")}
    p["open"] = _clamp(p.get("open", 1.0), 0.5, 1.5, 1.0)[0]
    rm = p.get("rim") if isinstance(p.get("rim"), dict) else {}
    p["rim"] = {"cover": _clamp(rm.get("cover", 0.0), 0.0, 0.3, 0.0)[0], "height": _clamp(rm.get("height", 2.0), 0.5, 20.0, 2.0)[0]}
    if p.get("pal"): p["pal"] = {k: v for k, v in p["pal"].items() if k in PAL[style] and isinstance(v, str)}
    if p.get("ban") is not None: p["ban"] = [str(x) for x in p["ban"]] if isinstance(p["ban"], list) else []
    if fixes: notes.append(f"{int(fixes)} numbers put back in range")
    return p, notes


# ====================================================================================================== composing
def read_layout(sc):
    """what the style's ground already has: big raised things, floor edges, liquid, long lines"""
    s = sc.s; cells = _cells(s); inops = {id(p) for op in s.ops for p in op[1]}
    sc.anchors = [p for op in s.ops if op[-1] not in BRIDGE and op[-1] not in SOFT for p in op[1]
                  if p.area > 12 and id(p) not in cells and (op[3] if op[0] == "prism" else op[2] + op[3]) - H >= 1.0 and op[2] <= H + 0.05]
    sc.tall_cells = [e[0] for e in getattr(s, "terr", []) if id(e[0]) in inops and e[2] - H >= 1.0]
    pl = _plates(s); sc.floor = _u([g for z, g in pl]).intersection(sc.disc) if pl else Polygon()
    fe = sc.floor.boundary.intersection(Point(0, 0).buffer(gg.R0 - 2.0)) if not sc.floor.is_empty else None
    sc.floor_edge = fe.buffer(0.2) if fe is not None and not fe.is_empty and fe.length > 4 else None
    sc.waters = _u([p for ps, d, k in s.gcuts if k in WETK for p in ps if p.area > 4]).intersection(sc.disc)
    sc.lines = _u([p for ps, d, k in s.gcuts if k in ("rut", "wash", "water", "lava", "bile", "acid") for p in ps if p.length > 30 and p.area / p.length < 4.0]).intersection(sc.disc)
    sc.paved = (not sc.floor.is_empty) and sc.floor.area > 0.8 * sc.disc.area

def layout_facts(sc):
    s = sc.s; out = []; kinds = {}
    for op in s.ops:
        if op[-1] in SOFT: continue
        kinds[op[-1]] = kinds.get(op[-1], 0) + sum(p.area for p in op[1])
    top = [k.replace("_", " ") for k, a in sorted(kinds.items(), key=lambda kv: -kv[1])[:3] if a > 0.03 * sc.disc.area]
    if sc.paved: out.append("a floor of " + (top[0] if top else "plates") + " all over")
    elif not sc.floor.is_empty and sc.floor.area > 0.15 * sc.disc.area: out.append("a floor of " + (top[0] if top else "plates") + " over part of it, bare ground beside")
    elif top: out.append("bare ground with " + ", ".join(top))
    else: out.append("bare open ground")
    if not sc.waters.is_empty: out.append(("a channel" if not sc.lines.is_empty else "pools") + " of " + next((k for ps, d, k in s.gcuts if k in WETK), "water"))
    if len(sc.anchors) >= 2: out.append(f"{len(sc.anchors)} big rocks or blocks")
    return ", ".join(out)

def _surf(s, g):
    """the surface a big feature stands on: the ground, or the floor plates it lies across"""
    z = H; br = False
    for zt, pl in _plates(s):
        if pl.intersection(g).area > 0.05 * g.area: z = max(z, zt); br = True
    z = Z(z); z.bridge = br
    return z

def _carve(s, g):
    """last resort, when no theme's subject finds room anywhere: cut the big raised pieces back round g"""
    gb = g.buffer(0.5); cells = _cells(s); dead = []; swap = {}
    for op in s.ops:
        if op[-1] in SOFT or op[-1] in BRIDGE or op[-1] in TERRK or op[2] > H + 0.05: continue
        for p in op[1]:
            if p.area < 10 or id(p) in cells or not p.intersects(gb): continue
            if op[0] == "prism": swap[id(p)] = [x for x in polys_of(p.difference(gb)) if x.area > 2.0 and wide_enough(x, 1.2)]
            else: dead.append(p)
    if swap:
        s.ops = [o for o in ((op[0], [x for p in op[1] for x in swap.get(id(p), [p])]) + tuple(op[2:]) for op in s.ops) if o[1]]
        s.feet = [x for p in s.feet for x in swap.get(id(p), [p])]
        if hasattr(s, "objs"): s.objs = [x for p in s.objs for x in swap.get(id(p), [p])]
        dead += [p for op in s.ops if op[2] > H + 0.05 and op[-1] not in BRIDGE and op[-1] not in TERRK for p in op[1] if gb.contains(p.representative_point())]
        s.tcuts = [(ps2, z0, z1, k) for ps, z0, z1, k in s.tcuts
                   for ps2 in [[q for p in ps for q in (polys_of(p.difference(gb)) if z1 > H + 0.35 and p.intersects(gb) else [p]) if not q.buffer(-0.1).is_empty]] if ps2]
    _remove(s, dead)

def _fits(sc, foot, solid, edge, force=False):
    s = sc.s; inside = foot.intersection(sc.disc)
    if edge:
        if inside.area < 0.45 * foot.area: return False
    elif inside.area < 0.995 * foot.area: return False
    test = solid.buffer(0.5)
    if not force and any(p.intersects(test) for p in sc.bigs): return False
    if not force and sc.lline is not None and sc.lline.intersects(foot.buffer(-1.5)): return False
    if any(k.intersects(test) for k in getattr(s, "keepout", [])): return False
    if sc.wet is not None and (sc.wet.intersection(solid).area > 0.15 * solid.area or sc.wet.intersection(inside).area > 0.2 * inside.area): return False
    return not any(g.intersects(foot) for g in sc.taken)

def _taller(layers, height, boost):
    """a subject stands `height` times taller. Each part is kept printable and sturdy: no taller than SLENDER times
    its own thickness (a thin post stays short, a thick wall block may tower). SLENDER 0 = no such limit."""
    if height <= 1.001: return layers
    sl = float(CFG.get("max_slenderness", 3.0)); out = []
    for L in layers:
        if L[0] == "steps":              # a rounded piece grows as a whole, held by the thickness of its foot
            f = height; hs = max(p[1] for p in L[2]); thick = min((4 * q.area / q.length for q in polys_of(L[1]) if q.length > 0), default=1.0)
            if sl > 0: f = max(1.0, min(f, sl * thick / (hs * boost)))
            out.append(L[:2] + ([(p[0], p[1] * f) + ((p[2], p[3] * f) if len(p) > 3 else ()) + tuple(p[4:]) for p in L[2]],) + L[3:]); continue
        i = {"up": 2, "mound": 2, "on": 3}.get(L[0])
        if i is None or L[i] < 1.0 or L[1] is None or L[1].is_empty: out.append(L); continue
        f = height
        if sl > 0:
            thick = min((4 * q.area / q.length for q in polys_of(L[1]) if q.length > 0), default=1.0)
            f = max(1.0, min(f, sl * thick / (L[i] * boost)))
        out.append(L[:i] + (L[i] * f,) + L[i + 1:])
    return out

def _place(sc, foot, layers, height=1.0):
    """put a built feature on the base (the spot has been checked): clear what is under it, then its layers"""
    layers = _taller(layers, height, float(CFG.get("big_base_height_factor", 1.5)) if sc.mm >= 59.9 else 1.0)
    s = sc.s; solid = _solid_of(layers).intersection(sc.wide)
    if terr(s): clear_terrain(s, foot.buffer(0.8).intersection(sc.wide))
    clear_small(s, solid.buffer(0.4)); evict(s, solid)
    z = H if terr(s) else _surf(s, foot.intersection(sc.disc))
    HBOOST[0] = float(CFG.get("big_base_height_factor", 1.5)) if sc.mm >= 59.9 else 1.0
    try: return F._build(s, z, layers)
    finally: HBOOST[0] = 1.0

def _ray_exit(c, d, g, far=400.0):
    """the last point of g on the ray from c along d"""
    x = LineString([c, (c[0] + d[0] * far, c[1] + d[1] * far)]).intersection(g)
    if x.is_empty: return c
    pts = [q for geom in (x.geoms if hasattr(x, "geoms") else [x]) for q in geom.coords]
    return max(pts, key=lambda q: (q[0] - c[0]) * d[0] + (q[1] - c[1]) * d[1])

def _rim_hit(q, d, R):
    """where the ray from q along d meets the circle of radius R"""
    b = q[0] * d[0] + q[1] * d[1]; cc = q[0] ** 2 + q[1] ** 2 - R * R; t = -b + math.sqrt(max(0.0, b * b - cc))
    return q[0] + d[0] * t, q[1] + d[1] * t

def place_subject(sc, plan, small=False, force=False):
    """A3 step 4: the subject at its spot; if something big is in the way turn the spot round the base, then try it
    smaller. Returns False when it fits nowhere (the caller moves on to the next theme)."""
    sub = plan["subject"]; pth = plan["path"]; R = sc.R
    build = SUBJECTS[sub["name"]](sc.pal); k0 = (1.0 if small and F.true_scale() == 1.0 else F.size_factor(sc.mm) * _big(sc.mm)) * sub["size"]
    seed = sc.mixr.random(); edge = sub["edge"]; past = pth["kind"] != "none" and pth["route"] == "past"
    offs = [sub["offset"]] + [o for o in (0.3, 0.15, 0.42, 0.0) if abs(o - sub["offset"]) > 0.06]
    for kk in (k0, 0.8 * k0, 0.62 * k0):
        for off in ([0] if edge else offs):
            for i in range(15):
                da = (i + 1) // 2 * 25 * (1 if i % 2 else -1); th = math.radians(sub["angle"] + da)
                r = (R - 2.4) if edge else R * off; c = (r * math.cos(th), r * math.sin(th))
                fa = math.radians(pth["from_angle"] + da); p0 = ((R + 2) * math.cos(fa), (R + 2) * math.sin(fa))
                if edge: front = (-math.cos(th), -math.sin(th))
                elif past: front = _unit(-c[0], -c[1]) if off > 0.1 else (math.cos(fa), math.sin(fa))
                else: front = _unit(p0[0] - c[0], p0[1] - c[1])
                try: got = build(c[0], c[1], math.atan2(front[1], front[0]), _random.Random(seed), kk)
                except Exception as e: sc.log.append(__import__("kitlog").caught(f"subject {sub['name']}", e)); return False
                if got is None or got[0] is None or got[0].is_empty: continue
                foot, layers = got; solid = _solid_of(layers).intersection(sc.wide)
                if small and foot.area > 0.30 * math.pi * R * R: return False
                if not _fits(sc, foot, solid, edge, force): continue
                if force: _carve(sc.s, solid)
                if not _place(sc, foot, layers, float(sub.get("height", 1.0))): return False
                sc.subj = foot.intersection(sc.wide); sc.core = _solid_of(layers, 6.0).intersection(sc.wide); sc.solid = solid
                sc.c, sc.front, sc.p0 = c, front, p0; sc.p1 = _ray_exit(c, front, sc.subj); sc.q = sc.p1; sc.taken.append(sc.subj.buffer(0.6))
                sub["angle"] = round((sub["angle"] + da) % 360, 1); pth["from_angle"] = round((pth["from_angle"] + da) % 360, 1); sub["size"] = round(sub["size"] * kk / k0, 2)
                if not edge: sub["offset"] = off
                return True
    return False

def _rot(v, a): return (math.cos(a) * v[0] - math.sin(a) * v[1], math.sin(a) * v[0] + math.cos(a) * v[1])

def lay_path(sc, plan):
    """A3 step 5: the path, with the subject's real footprint"""
    pth = plan["path"]; kind = pth["kind"]
    if kind == "none": return
    R = sc.R; c, front, p1 = sc.c, sc.front, sc.p1; n = (-front[1], front[0]); hw = PATH_W[kind]; bend = pth["bend"]
    if pth["route"] == "past":
        q = (p1[0] + front[0] * (1.2 + hw), p1[1] + front[1] * (1.2 + hw))
        d1 = _rot(front, math.pi / 2 - math.radians(6 + abs(bend) * 110)); d2 = _rot(front, -math.pi / 2 + math.radians(6 + abs(bend) * 60))
        if bend < 0: d1, d2 = d2, d1
        pa, pb = _rim_hit(q, d1, R + 2), _rim_hit(q, d2, R + 2)
        line = _curve(pa, (2 * q[0] - (pa[0] + pb[0]) / 2, 2 * q[1] - (pa[1] + pb[1]) / 2), pb); sc.start = pa; sc.q = (q[0] + front[0] * hw, q[1] + front[1] * hw)
        pth["from_angle"] = round(math.degrees(math.atan2(pa[1], pa[0])) % 360, 1)
    else:
        p0 = sc.p0; end = _ray_exit(c, front, sc.core) if sc.core is not None and not sc.core.is_empty else p1
        if math.dist(end, c) < math.dist(p1, c) * 0.5: end = (p1[0] - front[0] * 1.0, p1[1] - front[1] * 1.0)
        L = math.dist(p0, end)
        ctrl = (end[0] + front[0] * L * 0.5 + n[0] * bend * L, end[1] + front[1] * L * 0.5 + n[1] * bend * L)
        line = _curve(p0, ctrl, end); sc.start = p0; sc.q = p1
    try: g = PATHS[kind](sc, line, _random.Random(sc.mixr.random()))
    except Exception as e: sc.log.append(__import__("kitlog").caught(f"path {kind}", e)); g = None
    if g is not None and not g.is_empty:
        sc.line = line; sc.pathg = line.buffer(hw, cap_style=2).intersection(sc.wide); sc.taken.append(sc.pathg)

def hold_stand(sc, plan, centre=None, facing=None):
    """where the miniature stands: its contact pads are cleared, made flat and kept free; tall things keep out of
    its body outline. With a subject it stands in front of it, astride the path; without one, near the middle."""
    from shapely.prepared import prep
    s = sc.s; kind = sc.mini
    if kind == "none" or not MINIS[kind]["pads"]: return False
    raised = [p for op in s.ops if op[-1] not in BRIDGE and op[-1] not in SOFT and op[-1] not in TERRK for p in op[1] if p.area >= 10 and id(p) not in _cells(s)]
    block = _Hits(raised + ([sc.solid] if sc.solid is not None and not sc.solid.is_empty else []))
    wet = sc.wet if getattr(sc, "wet", None) is not None else None
    _, b0 = stand_shapes(kind, 0, 0, 0); x0, y0, x1, y1 = b0.bounds; depth = (x1 - x0) / 2; best = None; rr = _random.Random(sc.mixr.random())
    if centre is None:
        fa = math.atan2(-sc.front[1], -sc.front[0]); n = (-sc.front[1], sc.front[0]); cands = []
        for turn in (0.0, math.pi / 2, 0.5, -0.5):
            d = (depth if abs(turn) < 1.0 else (y1 - y0) / 2) + 1.5
            for back in (0.0, 3.0, 7.0):
                for side in (0.0, 4.0, -4.0, 8.0, -8.0, 13.0, -13.0):
                    cands.append((sc.q[0] + sc.front[0] * (d + back) + n[0] * side, sc.q[1] + sc.front[1] * (d + back) + n[1] * side, fa + turn))
    else: cands = [(centre[0] + rr.uniform(-1, 1) * i * 0.6, centre[1] + rr.uniform(-1, 1) * i * 0.6, (facing or 0.0) + (0 if i < 6 else rr.uniform(0, 6.28))) for i in range(14)]
    for cx, cy, a in cands:
        pads, body = stand_shapes(kind, cx, cy, a); pu = unary_union(pads)
        if not sc.disc.buffer(-1.0).contains(pu): continue
        score = sum(1 for p in pads if block.hit(p)) * 10 + (pu.intersection(wet).area / pu.area * 8 if wet is not None else 0)
        if best is None or score < best[0]: best = (score, pads, body, (cx, cy), a)
        if score == 0: break
    if best is None:
        if centre is None: return hold_stand(sc, plan, centre=(sc.q[0] * 0.4, sc.q[1] * 0.4), facing=math.atan2(-sc.front[1], -sc.front[0]))      # no room in front of the subject: nearer the middle
        return False
    _, pads, body, c, a = best; pu = unary_union(pads); g = pu.buffer(0.4)
    if terr(s): clear_terrain(s, g)
    gp = prep(g); cells = _cells(s); keep = sc.solid.buffer(0.1) if sc.solid is not None else Polygon()
    _remove(s, [p for op in s.ops if op[-1] not in BRIDGE and op[-1] not in TERRK for p in op[1] if id(p) not in cells and gp.intersects(p) and (p.area < 10 or g.contains(p)) and not keep.intersects(p)])
    swap = {}                                   # a long raised path under a foot is cut back, so the foot stands on flat ground
    for op in s.ops:
        if op[0] != "prism" or op[-1] in BRIDGE or op[-1] in TERRK or op[-1] in SOFT: continue
        for p in op[1]:
            if id(p) not in cells and gp.intersects(p) and not keep.intersects(p): swap[id(p)] = [x for x in polys_of(p.difference(g)) if x.area > 0.8 and wide_enough(x)]
    if swap:
        s.ops = [o for o in ((op[0], [x for p in op[1] for x in swap.get(id(p), [p])]) + tuple(op[2:]) for op in s.ops) if o[1]]
        s.feet = [x for p in s.feet for x in swap.get(id(p), [p])]
        if hasattr(s, "objs"): s.objs = [x for p in s.objs for x in swap.get(id(p), [p])]
    s.gcuts = [(ps2, d, k) for ps, d, k in s.gcuts for ps2 in [[q for p in ps for q in (polys_of(p.difference(g)) if gp.intersects(p) and k not in ("crack", "joint") else [p]) if q.area > 0.1]] if ps2]
    s.tcuts = [(ps2, z0, z1, k) for ps, z0, z1, k in s.tcuts for ps2 in [[q for p in ps for q in (polys_of(p.difference(g)) if gp.intersects(p) and k not in ("crack", "joint") else [p]) if not q.buffer(-0.1).is_empty]] if ps2]
    s.held = getattr(s, "held", []) + pads; s.feet += pads
    if terr(s): s.keepout += pads
    s.stand = {"type": kind, "pads": pads, "body": body, "at": [round(c[0], 1), round(c[1], 1)], "facing": round(math.degrees(a) % 360)}
    sc.stand_pads = pu; sc.taken.append(body); sc.yard = body.intersection(sc.disc) if not body.is_empty else None
    plan["stand"] = {"type": kind, "at": s.stand["at"], "facing": s.stand["facing"]}
    return True

def hold_yard(sc, plan):
    """A3 step 6: open ground in front of the subject, held free until the scene is finished. The miniature's stand
    takes its place when the base is for a kind of miniature (the default); the round yard is the fallback."""
    from shapely.prepared import prep
    if hold_stand(sc, plan): return
    s = sc.s; yr = (4.5 + 2.0 * sc.K) * plan.get("open", 1.0) * (0.75 if sc.mm < 59.9 else 1.0)
    q = sc.q; cy = (q[0] + sc.front[0] * yr * 0.9, q[1] + sc.front[1] * yr * 0.9)
    y = Point(cy).buffer(yr, 48).intersection(sc.disc).difference(sc.subj.buffer(0.3))
    y = max(polys_of(y), key=lambda g: g.area, default=None)
    if y is None or y.area < 20: return
    sc.yard = y
    if terr(s): clear_terrain(s, y)
    cells = _cells(s); yp = prep(y); keep = sc.solid.buffer(0.1); pg = sc.pathg.buffer(0.3) if sc.pathg is not None else None
    _remove(s, [p for op in s.ops if op[-1] not in BRIDGE and op[-1] not in SOFT and op[-1] not in TERRK for p in op[1]
                if p.area < 10 and id(p) not in cells and yp.intersects(p) and not keep.intersects(p) and (pg is None or not pg.intersects(p))])
    s.held = getattr(s, "held", []) + [y]; s.feet.append(y)
    if terr(s): s.keepout.append(y)

def release(s):
    """give back the ground that was held open while the scene was laid out"""
    for p in getattr(s, "held", []):
        for lst in (s.feet, getattr(s, "keepout", [])):
            for i in range(len(lst) - 1, -1, -1):
                if lst[i] is p: del lst[i]
    s.held = []

def _share():
    """how many of the things that stand on the base are kept: 1 (the default) = all, 0.33 = a third. The ground
    itself (its shape, cracks, puddles, ruts, paving) is never cut."""
    try: return max(0.0, min(1.0, float(CFG.get("objects_amount", 1.0))))
    except (TypeError, ValueError): return 1.0

def _count(sc, n):
    n = n * max(1.0, sc.K / 2)
    return max(1, int(round(n + (sc.mixr.uniform(-1, 1) if n >= 2 else 0))))

def _snap(s): return (len(s.ops), len(s.gcuts), len(s.tcuts), sum(len(o[1]) for o in s.ops[-3:]))

def _run(sc, fn, at, aim_=None):
    """one detail under a focus; True if it put something on the base"""
    if at is None: return False
    s = sc.s; b = _snap(s)
    with focus(at, aim_):
        try: fn(s, sc.rng, sc.t)
        except Exception as e: sc.log.append(__import__("kitlog").caught(f"{getattr(fn, '__qualname__', 'detail')}", e))
    return _snap(s) != b

def _bare(sc, lim):
    """the parts of the top where a circle of radius lim finds nothing at all: no piece, no cut, no edge of a plate"""
    s = sc.s; big = 3.1416 * lim * lim * 1.5; gs = []
    for ps in [op[1] for op in s.ops] + [g[0] for g in s.gcuts] + [c[0] for c in s.tcuts]:
        for q in ps:
            if q is None or q.is_empty: continue
            gs.append(q if q.area < big else q.exterior.buffer(0.5, 2))          # a wide flat plate is only its edge: its middle is still plain
    disc = Point(0, 0).buffer(gg.R0 - 1.0, 64)
    return disc.difference(_u(gs).buffer(lim, 4)) if gs else disc.buffer(-lim)

def _bare_grid(sc, lim):
    """the same question as _bare, fast: a grid of points over the top (spacing lim / 3) and how far each is from
    the nearest piece, cut or plate edge (one GEOS nearest-search for all of them, no union, no buffer). A point
    whose clearance is over lim is bare ground. Returns (points (N, 2), clearance (N,))."""
    import numpy as np
    s = sc.s; big = 3.1416 * lim * lim * 1.5; small, rings = [], []
    for ps in [op[1] for op in s.ops] + [g[0] for g in s.gcuts] + [c[0] for c in s.tcuts]:
        for q in ps:
            if q is None or q.is_empty: continue
            if q.area < big * 0.5 or q.buffer(-lim * 0.9, 4).is_empty: small.append(q)          # too narrow to hold a plain patch: it covers the ground
            else: rings.append(q.exterior)          # a flat top wide enough for a plain patch is only its edge: its middle can still be plain
    r = gg.R0 - 1.0; st = lim / 3.0; xs = np.arange(-r + st / 2, r, st)
    gx, gy = np.meshgrid(xs, xs); keep = gx * gx + gy * gy <= r * r; P = np.column_stack([gx[keep], gy[keep]])
    if not len(P): return P, np.zeros(0)
    pts = shapely.points(P); d = np.full(len(P), np.inf)
    for geoms, off in ((small, 0.0), (rings, 0.5)):
        if not geoms: continue
        tree = shapely.STRtree(geoms); (ia, ib), dist = tree.query_nearest(pts, return_distance=True, all_matches=False)
        np.minimum.at(d, ia, dist - off)
    return P, d

def no_bare(sc, plan, marks_only=False, cracks=False):
    """the rule: no part of a base's top is plain smooth. Wherever a circle of bare ground is left (7 mm radius on a
    100 mm base, 3 mm on a 30 mm one), the style's own natural details go down there; a crack if nothing else fits."""
    if not CFG.get("no_bare_ground", True): return 0
    __import__("kitlog").fault("no_bare")          # KIT_FAULT=no_bare: a test of the error log
    s = sc.s; _ts = F.true_scale(); lim = max(3.0, min(7.0, 0.07 * float(os.environ.get("KIT_FINAL_MM") or sc.mm))); lim = lim if _ts == 1.0 else max(1.7, lim / _ts); rr = _random.Random(sc.mixr.random()); put = 0          # built larger, a bare patch grows with it: so it is measured on the finished base
    by = {}
    for f in D.DETAILS.get(sc.style, []):
        if not _banned(f, plan): by.setdefault(klass(f), []).append(f)
    fs = by.get("marks", []) * 2 + ([] if marks_only or _share() < 0.999 else by.get("debris", []) + by.get("growth", []))          # with fewer things on top, bare ground is filled with ground marks
    if cracks: fs = []          # the edge ring, after it was swept: cuts only, nothing loose stands there
    import numpy as np
    for rnd in range(3):
        P, clear = _bare_grid(sc, lim); free = clear > lim
        for _ in range(60):
            if not free.any(): break
            i = int(np.argmax(np.where(free, clear, -1.0))); x, y = float(P[i, 0]), float(P[i, 1]); ok = False          # the middle of the widest bare patch
            at = lambda rng, x=x, y=y: (x + rng.uniform(-1, 1) * lim * 0.35, y + rng.uniform(-1, 1) * lim * 0.35)
            for k in range(3 if fs else 0):
                if _run(sc, rr.choice(fs), at): ok = True; break
            if not ok:          # nothing from the pool found room: the ground itself cracks here
                a = rr.uniform(0, 6.283); n = rr.randint(3, 5); L = lim * rr.uniform(1.1, 1.7)
                pts = [(x + (i / (n - 1) - 0.5) * L * math.cos(a) + rr.uniform(-1, 1) * lim * 0.16, y + (i / (n - 1) - 0.5) * L * math.sin(a) + rr.uniform(-1, 1) * lim * 0.16) for i in range(n)]
                j = rr.randrange(1, n - 1); b = a + rr.choice((-1, 1)) * rr.uniform(0.6, 1.2)
                g = _u([LineString(pts).buffer(0.3, 2), LineString([pts[j], (pts[j][0] + L * 0.4 * math.cos(b), pts[j][1] + L * 0.4 * math.sin(b))]).buffer(0.26, 2)])
                fl = _top_at(s, x, y)          # on a raised top (a floor plate, a pipe, a slab): cut into that top, or it never shows
                if fl: s.tcut([q for q in polys_of(g.intersection(fl[1].buffer(-0.3))) if q.area > 0.05], fl[0] - 0.35, fl[0], "crack")
                else: s.gcut(polys_of(g), 0.4, "crack")
            put += 1; free &= (P[:, 0] - x) ** 2 + (P[:, 1] - y) ** 2 > (lim * 1.2) ** 2
    return put

def clear_edge(sc):
    """nothing small stands round the edge of the base without a reason: loose pieces in the outer ring go, unless
    they are beside the subject, a second feature, the backdrop, the path or water"""
    if sc.mm < 39.9: return
    R = sc.R; ring = sc.disc.difference(Point(0, 0).buffer(R * (0.8 if sc.mm >= 59.9 else 0.86), 96))
    keep = [g.buffer(2.5) for g in ([sc.subj] if sc.subj is not None else []) + list(sc.also) + list(getattr(sc, "rim", []) or []) + ([sc.focal] if sc.focal is not None else []) if g is not None and not g.is_empty]
    if sc.pathg is not None: keep.append(sc.pathg.buffer(2.0))
    if not sc.waters.is_empty: keep.append(sc.waters.buffer(1.5))
    zone = ring.difference(_u(keep)) if keep else ring
    if not zone.is_empty: clear_small(sc.s, zone)

def _big(mm): return 1.0 if F.true_scale() != 1.0 else 1.2 if mm >= 59.9 else 1.0          # on 60 mm and up the subject and the second features are drawn a fifth larger
def _na(mm): return 0 if mm < 59 else 1 if mm < 79 else 2 if mm < 129 else 3 if mm < 159 else 4          # how many second features a base holds

def place_also(sc, plan):
    """A3 step 7: smaller second features that belong to the theme"""
    for al in plan.get("also", []):
        at, _ = sc.relation("far" if al["where"] in ("at", "wall", "in", "yard", "front") else al["where"])
        if at is None: continue
        build = SUBJECTS[al["name"]](sc.pal); k = F.size_factor(sc.mm) * al["size"] * _big(sc.mm); seed = sc.mixr.random()
        for i in range(60):
            x, y = at(sc.mixr); a = math.atan2(sc.q[1] - y, sc.q[0] - x)
            if math.hypot(x, y) > sc.R * 0.72 and i < 50: continue                # a second feature stands in the scene, not out on the edge
            try: got = build(x, y, a, _random.Random(seed), k * (1.0 if i < 40 else 0.8))
            except Exception as e: sc.log.append(__import__("kitlog").caught(f"also {al['name']}", e)); break
            if got is None or got[0] is None: continue
            foot, layers = got
            if not _fits(sc, foot, _solid_of(layers), False): continue
            if _place(sc, foot, layers): sc.also.append(foot); sc.taken.append(foot.buffer(0.6))
            break

BUILT = {"corridor", "urban", "fortress", "dungeon", "temple", "targa", "trederra", "graveyard"}      # styles whose backdrop is masonry; the others get a rock face
ENCLOSED = {"mine", "cynder", "dungeon", "temple", "corridor", "fortress", "infestation", "beast"}      # places with a wall or a cave side behind them
CLIFFS = {"badlands", "highland", "coast", "maahl", "urban", "targa"}                                   # places that sometimes have one

def rim_dressing(sc, plan):
    """a backdrop, only where the place has one: ONE run of wall (built styles) or rock face (cave, cliff) on the rim
    directly behind the subject, the thing the subject is set against. Highest in the middle, falling away to its
    ends. Open country has none. plan["rim"]: cover = share of the edge (0 = none), height = how tall."""
    rim = plan.get("rim") or {}; cover = float(rim.get("cover", 0.0)); hf = float(rim.get("height", 2.0))
    if cover <= 0.02 or sc.mm < 59.9 or sc.subj is None or (plan.get("subject") or {}).get("edge") or (plan.get("subject") or {}).get("name") == "bridge": return
    s = sc.s; R = sc.R; rr = _random.Random(sc.mixr.random()); built = sc.style in BUILT
    bx, by = _rim_hit(sc.c, (-sc.front[0], -sc.front[1]), R); behind = math.atan2(by, bx)
    kind = kfix(sc.pal["stone"] if built else (sc.pal["rock"] or sc.pal["stone"])); boost = float(CFG.get("big_base_height_factor", 1.5)) if sc.mm >= 59.9 else 1.0
    avoid = _Hits([g.buffer(1.5) for g in sc.taken if g is not None and not g.is_empty] + _bigs(s) + list(getattr(s, "keepout", [])))
    made = []
    for i in range(1):
        ln = cover * 6.283; t0 = behind - ln / 2 + rr.uniform(-0.1, 0.1); t = t0
        while t < t0 + ln:
            prof = 0.4 + 0.6 * math.sin(math.pi * (t - t0) / ln) ** 0.8
            if built:
                bl = rr.uniform(3.0, 4.8); tc = t + bl / 2 / R; t += (bl + 0.12) / R
                if rr.random() < 0.1: continue
                q = rect((R - 2.1) * math.cos(tc), (R - 2.1) * math.sin(tc), 3.4, bl - 0.15, tc); h = rr.uniform(1.4, 2.3) * hf * prof
                lay = [("up", q, h, kind), ("tcut", _bar(((R - 4.2) * math.cos(tc), (R - 4.2) * math.sin(tc)), ((R + 1) * math.cos(tc), (R + 1) * math.sin(tc)), 0.26), 0, 0.25)]
                if rr.random() < 0.45: lay.append(("up", rect((R - 5.3) * math.cos(tc), (R - 5.3) * math.sin(tc), 2.6, bl * rr.uniform(0.45, 0.8), tc), max(0.6, h * rr.uniform(0.3, 0.55)), kind))      # a buttress or fallen course inside
                if rr.random() < 0.3: lay.append(("on", rect((R - 2.1) * math.cos(tc), (R - 2.1) * math.sin(tc), 2.2, bl * 0.55, tc), 0, rr.uniform(0.5, 1.1), kind))                                # a merlon / capstone
            else:
                r = rr.uniform(2.0, 3.8) * min(1.35, max(1.0, sc.K ** 0.3)); tc = t + r / R; t += (2 * r + 0.5) / R; d = R - r - 0.9 - rr.uniform(0, 1.3)
                q = stone(d * math.cos(tc), d * math.sin(tc), r, rr, rr.uniform(0.7, 0.95), rr.randint(6, 9)); h = rr.uniform(1.3, 2.3) * hf * prof
                lay = [("mound", q, h, kind, ((0.75, 8), (0.25, 38)))]
                if rr.random() < 0.4:
                    d2 = d - r - rr.uniform(1.2, 2.0); lay.append(("mound", stone(d2 * math.cos(tc + 0.02), d2 * math.sin(tc + 0.02), r * rr.uniform(0.35, 0.55), rr, 0.8, 6), max(0.5, h * 0.3), kind, ROCK_T))
            if built: lay = _weather(lay, rr)
            foot = _solid_of(lay)
            if not Point(0, 0).buffer(R - 0.35, 96).contains(foot) and not built: continue
            if avoid.hit(foot): continue
            lay = _taller(lay, 1.0, boost) if True else lay
            sl = float(CFG.get("max_slenderness", 3.0))
            if sl > 0: lay = [(L[0], L[1], min(L[2], max(1.0, sl * 4 * L[1].area / L[1].length / boost))) + tuple(L[3:]) if L[0] in ("up", "mound") and L[2] >= 1.0 else L for L in lay]
            if _place(sc, foot, lay): made.append(foot)
    if made: sc.anchors = sc.anchors + made; sc.taken += [g.buffer(0.3) for g in made]; sc.rim = made

def place_props(sc, plan):
    """A3 step 8: each prop where it belongs. One that finds no room there is left out; it never goes somewhere else."""
    placed = 0
    for pr in plan["props"]:
        fn = prop_fn(pr["what"], sc.pal)
        if fn is None: continue
        at, aim_ = sc.relation(pr["where"])
        for _ in range(_count(sc, pr["count"])):
            if sc.mixr.random() < _share(): placed += _run(sc, fn, at, aim_)
    return placed

def _banned(fn, plan):
    ban = plan.get("ban") or ()
    return getattr(fn, "kind", "") in ban or getattr(fn, "shape", "") in ban

def _samplers(sc):
    """A5: where each class of natural clutter gathers"""
    feet = ([sc.subj] if sc.subj is not None else []) + sc.also + sc.anchors + sc.tall_cells + ([sc.focal] if sc.focal is not None else [])
    own = ([sc.subj] if sc.subj is not None else []) + sc.also + ([sc.focal] if sc.focal is not None else [])
    foot = s_mix([(3, s_near(_u(own), 0.8, 3.2) if own else None), (1, s_near(_u(sc.anchors + sc.tall_cells), 0.8, 3.0) if sc.anchors or sc.tall_cells else None)])
    fl = s_near(sc.floor_edge, 0.6, 2.2) if sc.floor_edge is not None else None
    wat = s_near(sc.waters, 0.8, 2.6) if not sc.waters.is_empty else None
    pg = sc.pathg.intersection(sc.disc) if sc.pathg is not None else None
    if pg is not None and sc.plan["path"]["kind"] == "stream": wat = s_mix([(1, wat), (2, s_near(pg, 0.6, 2.4))])
    lee = None
    if feet:
        cands = [g for g in feet if sc.pathg is None or g.distance(sc.pathg) > 2.0] or feet
        lee = s_near(_u(cands), 0.8, 3.0, "back", sc.c, sc.front) if sc.subj is not None else s_near(_u(cands), 0.8, 3.0)
    npath = s_near(pg, 0.8, 3.0) if pg is not None else None; inner = s_rim(gg.R0 * 0.3, gg.R0 - 1.0)      # inner: anywhere but the outer ring
    debris = s_mix([(60, foot), (22, fl), (12, wat), (6, npath)]) or inner                                    # nothing gathers along the edge of the base: it is not a wall
    growth = s_not(s_mix([(40, lee), (35, wat), (25, npath)]) or inner, sc.pathg, 2.0)
    return dict(debris=debris, growth=growth, foot=foot, wat=wat, near_path=npath)

def tier_features(sc, plan, fns):
    """A3 step 9: the style's own signature features, each where a thing of its kind belongs"""
    sm = _samplers(sc); reps = max(0, round(sc.K ** 0.7) - 1); late = []
    made_at = s_mix([(3, sc.relation("at")[0]), (2, sc.relation("wall")[0]), (1, sc.relation("also")[0] if sc.also else None)]) or sm["foot"] or s_rim(gg.R0 * 0.3, gg.R0 - 1.0)
    for fn in fns:
        c = klass(fn)
        if getattr(fn, "tmpl", "") in ("single", "scatter") and getattr(fn, "shape", "") == "rect" and getattr(fn, "kind", "") in ("crate", "coffin"):
            fn = prop_fn("crates" if fn.tmpl == "scatter" else "crate", sc.pal) or fn
        elif getattr(fn, "tmpl", "") == "single" and getattr(fn, "shape", "") == "disc" and getattr(fn, "kind", "") in ("wood", "barrel"): fn = prop_fn("keg", sc.pal) or fn
        elif getattr(fn, "tmpl", "") in ("single", "scatter") and getattr(fn, "kind", "") == "sack": fn = prop_fn("sacks" if fn.tmpl == "scatter" else "sack", sc.pal) or fn
        if _banned(fn, plan) or c in ("abstract", "bones"): continue
        if c == "late": late.append(fn); continue
        if c == "line":
            if sc.pathg is None and sc.subj is None: late.append(fn)
            continue
        for r in range(1 + reps):
            if r and sc.mixr.random() < 0.4: continue
            if c != "marks" and sc.mixr.random() >= _share(): continue
            if c == "made": _run(sc, fn, made_at, a_par(sc.subj.convex_hull) if sc.subj is not None and sc.mixr.random() < 0.5 else None)
            elif c == "marks": sc.marks_later.append(fn)
            else: _run(sc, fn, sm.get(c) or sm["debris"])
    return late

AMOUNT = dict(debris=0.8, growth=1.4, marks=0.8)      # clusters per class on a base, times K * K (K = 2 at 60 mm)

def clutter(sc, plan, n_each=None):
    """A5: natural things from the style's own pool, in groups where they would gather"""
    by = {}
    for f in D.DETAILS.get(sc.style, []):
        if not _banned(f, plan): by.setdefault(klass(f), []).append(f)
    sm = _samplers(sc); cl = plan.get("clutter", {})
    for c in ("debris", "growth"):
        fs = by.get(c, [])
        if not fs or sm[c] is None: continue
        for _ in range(round((AMOUNT[c] * sc.K * sc.K if n_each is None else n_each) * cl.get(c, 1.0) * _share())): _run(sc, sc.mixr.choice(fs), sm[c])
    sc.mark_pool = by.get("marks", [])

def marks(sc, plan, n_each=None):
    """cracks anywhere (they are ground texture); puddles beside the path and by water; footprints along the path;
    pits and scorch round the subject. Run after the yard is released: marks may cross open ground."""
    sm = _samplers(sc); fs = list(getattr(sc, "mark_pool", []))
    n = round((AMOUNT["marks"] * sc.K * sc.K if n_each is None else n_each) * plan.get("clutter", {}).get("marks", 1.0))
    todo = list(sc.marks_later) + ([sc.mixr.choice(fs) for _ in range(n)] if fs else [])
    anyw = s_rim(gg.R0 * 0.18, gg.R0 - 1.0)
    for fn in todo:
        kind, tmpl, shape = getattr(fn, "kind", ""), getattr(fn, "tmpl", ""), getattr(fn, "shape", "")
        if tmpl == "trail":
            if sm["near_path"] is not None: _run(sc, fn, sm["near_path"], a_par(sc.pathg, 0.15))
            elif sc.subj is not None: _run(sc, fn, sc.relation("front")[0], a_to(sc.c))
            else: _run(sc, fn, anyw)
        elif kind in WETK: _run(sc, fn, s_mix([(2, sm["near_path"]), (2, sm["wat"]), (1, sm["foot"])]) or anyw)
        elif kind in ("crack", "glyph", "seam", "mudcrack") or shape in ("wig", "star"): _run(sc, fn, anyw)
        else: _run(sc, fn, s_mix([(3, sm["foot"]), (1, anyw)]) or anyw)

def lay_bridge(sc, plan):
    """the crossing is the subject: a stream right across the base and a way over it"""
    R = sc.R; sub = plan["subject"]; th = math.radians(sub["angle"]); off = min(0.25, sub["offset"]) * R; q = (off * math.cos(th), off * math.sin(th))
    phi = math.radians(plan["path"]["from_angle"]); d = (math.cos(phi), math.sin(phi)); n = (-d[1], d[0]); rr = _random.Random(sc.mixr.random())
    bow = rr.uniform(-0.25, 0.25) * R
    sa, sb = _rim_hit(q, n, R + 2), _rim_hit(q, (-n[0], -n[1]), R + 2)
    sline = _curve(sa, (2 * q[0] - (sa[0] + sb[0]) / 2 + d[0] * bow, 2 * q[1] - (sa[1] + sb[1]) / 2 + d[1] * bow), sb)
    sg = P_stream(sc, sline, rr)
    if sg is None or sg.is_empty: return False
    q = tuple(sline.interpolate(sline.length / 2).coords[0])
    ca, cb = _rim_hit(q, d, R + 2), _rim_hit(q, (-d[0], -d[1]), R + 2); bend = plan["path"]["bend"] * 0.6
    cline = _curve(ca, (2 * q[0] - (ca[0] + cb[0]) / 2 + n[0] * bend * R, 2 * q[1] - (ca[1] + cb[1]) / 2 + n[1] * bend * R), cb)
    kind = plan["path"]["kind"]; sw = sg.intersection(Point(q).buffer(7)).area / 14 + 1.0
    if kind != "stones":      # a plank deck over the water, the chosen way on both banks
        x, y, a = _at(cline, cline.length / 2); deck = LineString([(x - (sw + 1.6) * math.cos(a), y - (sw + 1.6) * math.sin(a)), (x + (sw + 1.6) * math.cos(a), y + (sw + 1.6) * math.sin(a))])
        P_boards(sc, deck, rr); sc.core = deck.buffer(2.1, cap_style=2)
    try: PATHS[kind](sc, cline, rr)
    except Exception as e: sc.log.append(__import__("kitlog").caught(f"bridge way {kind}", e))
    sc.subj = Point(q).buffer(sw + 2.2, 24); sc.solid = Polygon(); sc.core = None
    sc.c = q; sc.front = d; sc.p1 = (q[0] + d[0] * (sw + 2.2), q[1] + d[1] * (sw + 2.2)); sc.q = sc.p1; sc.start = ca; sc.line = cline
    sc.pathg = _u([sline.buffer(PATH_W["stream"] + 0.6, cap_style=2), cline.buffer(PATH_W[kind], cap_style=2)]).intersection(sc.wide); sc.taken.append(sc.pathg)
    plan["open"] = plan.get("open", 1.0) * 0.7
    return True

def lay_scene(sc, plan, small=False, force=False):
    """a whole scene from one plan. False = the subject found no room (nothing has been changed)."""
    sc.plan = plan; sc.pal = dict(PAL[sc.style]); sc.pal.update(plan.get("pal") or {})
    s = sc.s; sc.bigs = _bigs(s); sc.taken = []
    wet = [p for ps, d, k in s.gcuts if k in WETK or k in ("pit", "hollow") for p in ps if p.area > 6]; sc.wet = unary_union(wet) if wet else None
    ll = [p for ps, d, k in s.gcuts if k in ("rut", "wash") or k in WETK for p in ps if p.length > 30 and p.area / p.length < 4.0]; sc.lline = unary_union(ll) if ll else None
    if plan["subject"]["name"] == "bridge":
        if small or not lay_bridge(sc, plan): return False
    else:
        if not place_subject(sc, plan, small, force): return False
        lay_path(sc, plan)
    hold_yard(sc, plan)
    return True

def small_scene(sc, plan):
    """A6, under 60 mm: no room for a subject and a path, so the theme's props gather round one focal piece"""
    s = sc.s; props = list(plan["props"]); sc.mixr.shuffle(props); want = max(1, round((1 + sc.t) * sc.K * sc.K * _share())); R = sc.R; placed = 0
    line = sc.lines if not sc.lines.is_empty else None
    ring_ = lambda rng: (lambda a, r: (r * math.cos(a), r * math.sin(a)))(rng.uniform(0, 6.2832), R * rng.uniform(0.25, 0.5))
    for pr in props + [pr for pr in props if pr["count"] > 1]:
        if placed >= want: break
        fn = prop_fn(pr["what"], sc.pal)
        if fn is None: continue
        before = {id(p) for p in s.feet}
        if sc.focal is None: ok = _run(sc, fn, ring_)
        elif pr["where"] in ("path", "start") and line is not None: ok = _run(sc, fn, s_near(line, 1.0, 3.5), a_par(line, 0.4))
        else: ok = _run(sc, fn, s_mix([(3, s_near(sc.focal, 1.0, 4.0)), (1, s_near(_u(sc.anchors), 0.8, 3.0) if sc.anchors else None)]), a_par(sc.focal.convex_hull, 0.3) if pr["where"] in ("wall", "back") else None)
        placed += ok
        if ok and sc.focal is None:
            new = [p for p in s.feet if id(p) not in before]
            if new and sum(p.area for p in new) >= 6: sc.focal = unary_union(new).convex_hull
    return placed

def _style_made(sc, plan):
    """the made things among the style's own twenty details (style_details.py): a different handful on every base"""
    try:
        import style_details
        n = style_details.scatter_made(sc, plan)
        if os.environ.get("SCENE_DEBUG"): print(f"  style details: {n} made things set down", flush=True)
    except Exception as e: sc.log.append(__import__("kitlog").caught(f"style details", e))

def compose(s, style, mixr, rng, t, fns):
    sc = Sc(s, style, mixr, rng, t); mm = sc.mm; n = _num(s.name); big = mm >= 59.9
    read_layout(sc); s.layout_facts = layout_facts(sc)
    plans = []; src = "table"; notes = []
    pj = os.environ.get("PLAN_JSON", "")
    if pj:
        try:
            if os.path.exists(pj):
                with open(pj, encoding="utf-8") as fh: raw = json.load(fh)
            else: raw = json.loads(pj)
            p, notes = validate_plan(raw, style, mm)
            if p: plans.append(p); src = str(raw.get("_source", "file")); notes = list(raw.get("_notes", [])) + notes
            else: notes = ["given plan refused: " + "; ".join(notes)]
        except Exception as e: notes = [f"given plan unreadable: {type(e).__name__} {e}"]
    if not plans and not pj and mm >= 59.9 and not os.environ.get("KIT_WORKER") and not os.environ.get("THEME"):
        try:
            import designer
            if designer.enabled():
                p = designer.plan_for(style, mm, t, s.layout_facts, f"{os.environ.get('SALT', '')}{s.name}")
                if p: notes = list(p.pop("_notes", [])); src = p.pop("_source", "designer"); plans.append(p)
                else: notes = [f"designer failed ({designer.LAST['error'] or 'no usable plan'})"]
        except Exception as e: notes = [f"designer failed ({type(e).__name__})"]
    given = len(plans)
    plans += pick_plans(style, mixr, mm, n)
    plan = None; scene = False; ts = F.true_scale()
    # At the miniature's scale a base is a patch of ground, not a model of a place: no landmark is laid (it read as a
    # little house), the miniature stands in the middle, and the things lie round it towards the edge.
    if (big or mm >= 39.9) and ts == 1.0:
        for i, p in enumerate(plans if big else plans[:1]):
            if lay_scene(sc, p, small=not big): plan = p; scene = True; break
            if i < given: notes.append("no room for the given plan's subject"); src = "table"
        if plan is None and big:          # every theme's subject is blocked by this layout: make room for the first one
            for p in plans[given:]:
                if lay_scene(sc, p, force=True): plan = p; scene = True; notes.append("big pieces cut back to make room"); break
    if plan is None and plans:
        plan = plans[0] if not big else plans[min(given, len(plans) - 1)]
        if big: notes.append("no theme's subject found room: props only")
    if plan is None: plan = {"theme": "", "props": [], "clutter": {}, "path": {"kind": "none"}, "subject": {"name": ""}}
    sc.plan = plan
    if scene:
        place_also(sc, plan); rim_dressing(sc, plan); place_props(sc, plan)
        _style_made(sc, plan)
        late = tier_features(sc, plan, fns); clutter(sc, plan, None if big else (1 + t) * sc.K * sc.K)
    else:
        sc.pal = dict(PAL[style]); sc.pal.update(plan.get("pal") or {})
        if getattr(s, "stand", None) is None:
            hold_stand(sc, plan, centre=(mixr.uniform(-0.08, 0.08) * sc.R, mixr.uniform(-0.08, 0.08) * sc.R) if ts == 1.0 else (0.0, 0.0), facing=mixr.uniform(0, 6.28))
        if ts != 1.0:          # the middle of the base belongs to the miniature (on a large base, a large one): nothing is laid there
            rv = min(sc.R, float(os.environ.get("KIT_FINAL_MM") or 2 * sc.R * ts) / (2 * ts)); clear = Point(0, 0).buffer(float(CFG.get("clear_middle", 0.45)) * rv, 24)
            s.keepout = list(getattr(s, "keepout", [])) + [clear]; sc.taken.append(clear)
            if float(os.environ.get("KIT_FINAL_MM") or 0) >= 49.9:          # a base is mostly ground: terrain at the scale of the base (ground_big.py), lower on smaller bases
                try: import ground_big; ground_big.apply(sc)
                except Exception as e: sc.log.append(__import__("kitlog").caught(f"ground_big", e))
        small_scene(sc, plan)
        _style_made(sc, plan)
        late = tier_features(sc, plan, fns); clutter(sc, plan, (1 + t) * sc.K * sc.K)
    marks(sc, plan, None if (scene and big) else (1 + t) * sc.K * sc.K * 0.6)
    try: nb = no_bare(sc, plan)          # the rule: the top is never plain smooth anywhere
    except Exception as e: nb = 0; sc.log.append(__import__("kitlog").caught(f"no_bare", e))
    release(s)
    try: nb += no_bare(sc, plan, marks_only=True)          # the miniature's own patch: ground marks only, nothing raised
    except Exception as e: sc.log.append(__import__("kitlog").caught(f"no_bare", e))
    if os.environ.get("SCENE_DEBUG"): print(f"  no bare ground: {nb} patches filled", flush=True)
    for fn in late:
        try: fn(s, rng, t)
        except Exception as e: sc.log.append(__import__("kitlog").caught(f"{getattr(fn, '__qualname__', 'late')}", e))
    try: clear_edge(sc)
    except Exception as e: sc.log.append(__import__("kitlog").caught(f"clear_edge", e))
    try: no_bare(sc, plan, cracks=True)
    except Exception as e: sc.log.append(__import__("kitlog").caught(f"no_bare", e))
    for op in s.ops: gg.COL.setdefault(op[-1], "#8a8a8a")
    plan["miniature"] = sc.mini
    s.theme = plan.get("theme", ""); s.plan = plan if scene else dict(plan, subject=None, path=None, also=[])
    s.plan_source = src; s.plan_notes = notes; s.scene_log = sc.log
    fails = [x for x in notes if "failed" in x or "refused" in x or "unreadable" in x or "no room for the given" in x]; fixes = [x for x in notes if x not in fails]
    s.plan_line = "plan: " + (f"{fails[0]}, " if fails else "") + f'{src} "{s.theme}"' + (f" ({len(fixes)} fix{'es' if len(fixes) != 1 else ''})" if fixes and src.startswith("designer") else f" ({'; '.join(fixes)[:160]})" if fixes else "")
    if os.environ.get("SCENE_DEBUG"): print("  " + s.plan_line, *("    ! " + l for l in sc.log), sep="\n", flush=True)


__import__("newstyles").hook_scenes(globals())          # the styles kept one per file (style_<name>.py)
_OWN = set(globals())
