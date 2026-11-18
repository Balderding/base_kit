"""Twenty more details for every style, each one found in that style only (a saguaro in the badlands, a torii lantern in
the fortress, a ration tin in the trenches). 19 styles x 20 = 380 builders, in sd_a.py to sd_e.py.

Every builder draws its own sizes, counts, angles and its state (whole, broken, tipped over, half buried) from the
random stream, so the same detail never comes out twice the same. A base takes a random handful of its style's twenty
on top of everything else, so the number of different bases is, for any practical purpose, without end.

This file holds the drawing tools the builders share and the two hooks into the kit:
  extend(details)        adds the builders to each style's pool (natural ones are then used by the scene's clutter)
  scatter_made(sc, plan) the scene step that sets down the made things

A builder returns (footprint, layers) exactly like the ones in features.py. Solids (cyl, dome, cone, turn, lie, ridge,
loaf, rod, spin, blk, ext) are joined into ONE smooth surface by ST(); nothing can overhang."""
import math
from sd_reg import REG

def _S():
    import scenes; return scenes

def install(G):
    globals().update({k: v for k, v in G.items() if not k.startswith("__") and k not in _OWN})
    mine = {k: v for k, v in globals().items() if not k.startswith("__")}
    for m in _parts(): m.__dict__.update({k: v for k, v in mine.items() if k not in m._OWN})
    for k, v in {"cactus": "#6f8a4a", "leather": "#7a5236", "cloth": "#b9ad92", "tin": "#9aa0a6", "brass": "#b8923c", "paper": "#e0d8c0", "glass": "#a9cfd6", "hide": "#8a6a4a",
                 "wax": "#e6dfc8", "coral": "#d98a7a", "kelp": "#4f6b3a", "lacquer": "#8a2f2a", "bamboo": "#9aa65a", "jade": "#5fa07a", "slime": "#7fa04a", "web": "#d8d8d0",
                 "chitin": "#3a3028", "resin": "#b07a2a", "rust": "#7a4a32", "tar": "#2a2826", "ash": "#4a4744", "ember": "#d2602a", "frost": "#dfeef4", "wire": "#55585e"}.items():
        gg.COL.setdefault(k, v)

def _parts():
    import sd_a, sd_b, sd_c, sd_d, sd_e, sd_r, glob, os, importlib          # sd_r and sd_r_<style>.py last: their redrawn builders replace the first drafts of the same name
    more = [importlib.import_module(os.path.basename(p)[:-3]) for p in sorted(glob.glob(os.path.join(os.path.dirname(os.path.abspath(__file__)), "sd_r_*.py")))]
    for m in more:
        if not hasattr(m, "_OWN"): m._OWN = {k for k in vars(m) if not k.startswith("__")} - {"_"}
    return (sd_a, sd_b, sd_c, sd_d, sd_e, sd_r) + tuple(more)


# ------------------------------------------------------------------------------------------------ outlines
def U(gs): return unary_union([x for x in gs if x is not None and not x.is_empty])
def disc(x, y, r, n=20): return Point(x, y).buffer(r, n)
def ann(x, y, ro, ri, n=24): return Point(x, y).buffer(ro, n).difference(Point(x, y).buffer(ri, n))
def capl(p, q, w): return LineString([p, q]).buffer(w / 2, 8)                 # a bar with round ends
def barl(p, q, w): return LineString([p, q]).buffer(w / 2, cap_style=2)       # a bar with square ends
def line(pts, w): return LineString(pts).buffer(w / 2, 6)                     # a bent bar
def ell(x, y, rx, ry, a=0.0, n=20): return affinity.rotate(affinity.scale(Point(x, y).buffer(1, n), rx, ry), a, origin=(x, y), use_radians=True)
def hull(gs): return U(gs).convex_hull
def ngon(x, y, r, n, a=0.0): return Polygon([(x + r * math.cos(a + 6.2832 * k / n), y + r * math.sin(a + 6.2832 * k / n)) for k in range(n)])
def arc(x, y, r, a0, a1, n=12): return [(x + r * math.cos(a0 + (a1 - a0) * k / n), y + r * math.sin(a0 + (a1 - a0) * k / n)) for k in range(n + 1)]
def spiral(x, y, r0, r1, turns, a=0.0, n=40): return [(x + (r0 + (r1 - r0) * k / n) * math.cos(a + 6.2832 * turns * k / n), y + (r0 + (r1 - r0) * k / n) * math.sin(a + 6.2832 * turns * k / n)) for k in range(n + 1)]
def pol(x, y, r, t): return (x + r * math.cos(t), y + r * math.sin(t))
def ringpts(x, y, r, n, a=0.0, jit=0.0, rng=None): return [pol(x, y, r, a + 6.2832 * k / n + (rng.uniform(-jit, jit) if rng else 0.0)) for k in range(n)]
def wavy(p, q, rng, amp=0.5, n=5, w=0.5): return LineString(wiggle(p, q, rng, amp, n)).buffer(w / 2, 6)


# ------------------------------------------------------------------------------------------------ solids
def cyl(x, y, r, h, lift=0.0, n=20): return _S().box(disc(x, y, r, n), h, lift)
def blk(x, y, L, W, a, h, lift=0.0): return _S().box(rect(x, y, L, W, a), h, lift)
def ext(poly, h, lift=0.0): return _S().box(poly, h, lift)
def dome(x, y, r, h, lift=0.0, sq=1.0, a=0.0, n=20): return _S().lathe(x, y, _S().curve(r, 0.22, 0.0, h, 7), n, sq, a, lift=lift)
def cone(x, y, r, h, tip=0.24, lift=0.0, n=16, sq=1.0, a=0.0): return _S().lathe(x, y, [(r, 0.0), (tip, h)], n, sq, a, lift=lift)
def turn(x, y, prof, n=24, sq=1.0, a=0.0, lift=0.0): return _S().lathe(x, y, prof, n, sq, a, lift=lift)
def lie(x, y, a, L, r, lift=0.0): return _S().vault(x, y, a, L, 2 * r, r * 1.15, lift=lift, under=True)       # a lying cylinder
def ridge(x, y, a, L, W, h, lift=0.0): return _S().vault(x, y, a, L, W, h, "ridge", lift=lift)
def loaf(x, y, a, L, W, h, lift=0.0): return _S().vault(x, y, a, L, W, h, "loaf", lift=lift)
def rod(pts, r, hk=1.0, lift=0.0, arch=0.0): return _S().tube(pts, r, hk, lift=lift, arch=arch)                # a round bar along a bent line
def spin(x, y, a, L, rf, hk=1.2, lift=0.0, prof="round"): return _S().loft(x, y, a, L, rf, hk, prof, lift=lift)    # a lying shape, half-width rf(u)
def crv(r0, r1, z0, z1, out=True, n=6): return _S().curve(r0, r1, z0, z1, n, out)
def ST(solids, kind, parent=None, cuts=()): return _S()._steps(solids, kind, parent, cuts)
def pot(x, y, r, h, neck=0.5, lip=0.66, lift=0.0, n=24):
    """a turned vessel: foot, belly, neck and lip"""
    return turn(x, y, [(r * 0.62, 0.05), (r * 0.62, h * 0.08), (r, h * 0.42)] + crv(r, r * neck, h * 0.42, h * 0.84) + [(r * lip, h * 0.9), (r * lip, h)], n, lift=lift)
def rock(x, y, r, h, rng, sq=None, n=9, lift=0.0): return _S().lathe(x, y, [(r, 0.0), (r * 0.92, h * 0.45), (r * 0.6, h * 0.85), (0.25, h)], n, sq or rng.uniform(0.7, 0.95), rng.uniform(0, 3.14), lift=lift)
def fluted(x, y, r, h, n=8, a=0.0, deep=0.8, m=6, twist=0.0, lift=0.0):
    """a ribbed dome (a barrel cactus, a gourd, a spiny ball): star-shaped all the way up"""
    return [s for k in range(m) for s in ext(star(x, y, r * math.cos(1.45 * k / m), r * math.cos(1.45 * k / m) * deep, n, a + twist * k), h * math.sin(1.45 * (k + 1) / m) / math.sin(1.45), lift)]
def hang(poly, z0, z1): return _S().slab(poly, z0, z1)       # hangs in the air with --supports on, else solid from the ground
def tri(p, q, w):
    """a pointed blade from p (w wide) to its tip q"""
    L = math.dist(p, q) or 1; nx, ny = -(q[1] - p[1]) / L * w / 2, (q[0] - p[0]) / L * w / 2
    return Polygon([(p[0] + nx, p[1] + ny), q, (p[0] - nx, p[1] - ny)])

ROCK_T = ((0.7, 6), (0.3, 40)); SOFT_T = ((0.55, 25), (0.45, 50)); PEB_T = ((0.7, 10), (0.3, 45)); SPIKE_T = ((0.75, 4), (0.25, 30)); ROUND_T = ((0.45, 12), (0.3, 38), (0.25, 62))


# ------------------------------------------------------------------------------------------------ hooks

# ------------------------------------------------------------------------------------------------ hard-edged objects
class Obj:
    """A made object with upright sides and a flat top, built in layers: a body, parts standing on it, cuts into it.
    ST() rounds everything into one slope, which is right for a skull or a sack and wrong for a toolbox: a box built
    with ST() loses its lid line, its latches and its corners. Anything with flat faces is built with this instead.
    Every method returns the index of its layer, to stand more on it or cut into it."""
    def __init__(s, kind): s.k = kind; s.L = []; s.F = []
    def up(s, g, h, kind=None): s.L.append(("up", g, h, kind or s.k)); s.F.append(g); return len(s.L) - 1
    def on(s, g, par, h, kind=None):          # as one upright piece through ST(): that keeps parts down to 0.45 mm wide (a plain "on" layer drops all under 0.88)
        s.L.append(ST(ext(g, h), kind or s.k, par)); return len(s.L) - 1
    def cut(s, g, par, d=0.18):
        if g is not None and not g.is_empty: s.L.append(("tcut", g, par, d))
    def st(s, solids, kind=None, par=None, cuts=(), foot=None):
        s.L.append(ST(solids, kind or s.k, par, cuts))
        if par is None: s.F.append(foot if foot is not None else s.L[-1][1])
        return len(s.L) - 1
    def gcut(s, g, d, kind): s.L.append(("gcut", g, d, kind)); s.F.append(g)
    def out(s, extra=()): return hull(s.F + list(extra)), s.L
    # ---- what makes a box a particular box
    def lid(s, g, par, h=0.16, edge=0.16, kind=None):
        """a lid a little smaller than the body, so its line shows all round"""
        return s.on(inset(g, edge), par, h, kind)
    def groove(s, g, par, w=0.2, d=0.16):
        """a closed line cut along an outline: a panel, a door, a lid seam"""
        s.cut(g.exterior.buffer(w / 2, 2), par, d)
    def seams(s, ff, L, W, par, step, across=False, w=0.2, d=0.16, skip=None):
        """parallel board or panel lines"""
        vs = [v for v in np.arange(-(W if not across else L) / 2 + step, (W if not across else L) / 2 - step * 0.5, step)]
        gs = [barl(ff(v, -W), ff(v, W), w) if across else barl(ff(-L, v), ff(L, v), w) for v in vs]
        g = U(gs); s.cut(g if skip is None else g.difference(skip), par, d)
    def studs(s, pts, par, r=0.27, h=0.15, kind=None, n=8):
        """rivets, bolt heads, nail heads, buttons"""
        return s.on(U([disc(x, y, r, n) for x, y in pts]), par, h, kind)
    def handle(s, p, q, par, w=0.46, h=0.42, kind=None):
        """a grab bar: two feet and the bar between them, standing proud"""
        i = s.on(U([disc(p[0], p[1], w * 0.75, 8), disc(q[0], q[1], w * 0.75, 8)]), par, h * 0.55, kind)
        return s.on(capl(p, q, w), par, h, kind)
    def tabs(s, ff, xs, v, par, a, l=0.5, w=0.62, h=0.17, kind=None):
        """latches, hinges or clasps in a row along one edge (v is how far across the top)"""
        return s.on(U([rect(*ff(x, v), l, w, a) for x in xs]), par, h, kind)
    def ribs(s, ff, L, W, par, n, a, w=0.46, h=0.15, across=True, kind=None):
        """pressed stiffening ribs standing a hair proud"""
        gs = [rect(*ff((i + 0.5) / n * L - L / 2, 0), w, W, a) if across else rect(*ff(0, (i + 0.5) / n * W - W / 2), L, w, a) for i in range(n)]
        return s.on(U(gs), par, h, kind)
    def vents(s, ff, x, v, par, n, l=1.0, a=0.0, step=0.42, w=0.2, d=0.22):
        """a block of louvre slots"""
        s.cut(U([barl(ff(x - l / 2, v + (i - (n - 1) / 2) * step), ff(x + l / 2, v + (i - (n - 1) / 2) * step), w) for i in range(n)]), par, d)
    def corners(s, ff, L, W, par, a, size=0.6, h=0.14, kind=None):
        """corner irons"""
        return s.on(U([rect(*ff(sx * (L / 2 - size / 2), sy * (W / 2 - size / 2)), size, size, a) for sx in (-1, 1) for sy in (-1, 1)]), par, h, kind)
    def straps(s, ff, xs, W, par, a, w=0.5, h=0.13, kind=None):
        """bands right across: iron bands, leather straps, tape"""
        return s.on(U([rect(*ff(x, 0), w, W + 1.0, a) for x in xs]), par, h, kind)

def rrect(x, y, L, W, a, r=0.3):
    """a rectangle with rounded corners"""
    return rect(x, y, L, W, a).buffer(-r, join_style=2).buffer(r, 6)

NEW20 = {}

def extend(details_mod):
    """wrap every registered builder as a detail and add it to its style's pool (once)"""
    if NEW20: return
    import features as F
    _parts()
    for st, lst in REG.items():
        for f, cls, kind, name, r, ground in lst:
            fn = F.comp(f, r=r, ground=ground); fn.cls = cls; fn.kind = kind; fn.shape = name; fn.sd = True
            NEW20.setdefault(st, []).append(fn)
        if st in details_mod.DETAILS: details_mod.DETAILS[st] = list(details_mod.DETAILS[st]) + NEW20[st]

def scatter_made(sc, plan):
    """the scene step for the made things among a style's twenty: a different handful on every base, each set down
    where somebody would have left it: by the subject, against it, beside the path, or out on its own"""
    S = _S()
    fs = [f for f in NEW20.get(sc.style, []) if f.cls == "made" and not S._banned(f, plan)]
    if not fs or not CFG.get("style_details", True): return 0
    n = max(1, round(float(CFG.get("style_details_amount", 0.5)) * sc.K * sc.K)); sc.mixr.shuffle(fs); placed = 0
    if sc.subj is not None:
        spots = [(3, "at"), (2, "wall"), (2, "path"), (2, "far"), (1, "also"), (1, "back")]
    else: spots = []
    for i in range(n * 2):
        if placed >= n: break
        fn = fs[i % len(fs)]
        if spots:
            at, aim_ = sc.relation(sc.mixr.choices([w for _, w in spots], [k for k, _ in spots])[0])
        else:
            foc = sc.focal; at = S.s_near(foc, 1.0, 4.5) if foc is not None and sc.mixr.random() < 0.6 else S.s_rim(gg.R0 * 0.25, gg.R0 - 1.0); aim_ = None
        placed += bool(S._run(sc, fn, at, aim_))
    return placed


_OWN = set(globals())
