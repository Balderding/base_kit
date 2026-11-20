"""Thornwild: a deadly alien jungle. Giant twisting roots heave out of a dark mossy floor, carnivorous plants open
their toothed jaws, spore pods swell and burst, rings of glowing fungi stand in the shade, thorn vines crawl over
everything, and among it all lie the bones and the dropped gear of the ones who came before.

One file holds the whole style (see newstyles.py); its own twenty-odd details are in sd_r_x_thornwild.py and their
real sizes in scale/real_thornwild.json.

  root tangle     the stump of a giant with its roots, thick roots across the ground, grasping vines, a lost skull
  maw flower      a digesting pit ringed with grasping vines, the bones and the gear of what it caught
  spore field     a cradle of roots holding swollen pods, pods and spores all round
  fungus ring     a ring of giant glowing fungus stalks round a cut circle, small rings of glowcaps about it
  overgrown wreck a crashed pod or vehicle with vines grown over it, the crew's crates and weapons left behind"""
import math

NAME = "thornwild"
TITLE = "Thornwild"
SAYS = ("a deadly alien jungle: giant twisting roots, carnivorous plants with toothed jaws, spore pods, rings of glowing "
        "fungi, thorn vines, and the bones and dropped gear of the unlucky")
GROUND = "#3c4629"
PAL = dict(wood="wood", iron="iron", rock="rock", stone="rock", crate="crate", water="water", pit="pit", gem=None, floor=None,
           plant="spikethorn", soft="moss", vein="root", twig="root", bone="bone")
PATH_OK = "veins trail stream stones"
EXTRA_PROPS = "tentacle eggs fairy nest pustules rifle cables"
BUILT, ENCLOSED, CLIFFS = False, False, False
COLOURS = {"spikethorn": "#5b6130", "creeper": "#3b5a2a", "maw": "#9a3a55", "glowcap": "#8fdcc0", "sporepod": "#b39a3c",
           "pitcher": "#6c7e34", "hullplate": "#6b737a", "sundew": "#a04a4a", "seedburr": "#7a6a34", "rootbark": "#5a4430"}

def T(name, subject, path, props, also="", **kw):          # as themes.T (not imported: themes.py loads this file)
    return dict(theme=name, subject=subject, path=path, props=props, also=also, **kw)

THEMES = [
    T("root tangle", "tree", "veins:past", "at:tentacle*2 at:skull wall:shrubs*2 at:bones far:fairy", ban=("crate", "metal", "hullplate")),
    T("maw flower", "tentacles", "trail:to", "at:bones*2 at:skull front:rifle at:pustules back:shrubs*2",
      pal=dict(pit="bile", vein="creeper"), clutter=dict(growth=1.3)),
    T("spore field", "nest", "trail:past", "at:pustules*2 at:eggs*2 path:prints far:puddle back:shrubs",
      pal=dict(twig="root"), clutter=dict(growth=1.5), ban=("crate", "hullplate")),
    T("fungus ring", "circle", "stones:to", "in:bones at:fairy*2 far:fairy back:shrubs*2",
      pal=dict(stone="glowcap"), clutter=dict(growth=1.3), ban=("crate", "maw", "hullplate")),
    T("overgrown wreck", "hull", "trail:to", "at:crates at:rifle at:tentacle*2 wall:shrubs*2 at:bones at:cables",
      pal=dict(wood="hullplate"), also="nest:far"),
]


# ------------------------------------------------------------------------------------------------ the ground
def layout(n):
    import gg, gg2, styles2 as S2
    from shapely.geometry import LineString, Point
    from shapely.ops import unary_union
    s = gg2.mk(NAME, TITLE, n, GROUND); rng = s.rng; v = rng.randint(0, 3); K = S2.KK()
    if v == 0:            # giant roots heaving out of the ground and crossing the base, splitting as they go
        a0 = rng.uniform(0, 6.28); c = (14.0 * gg.K * math.cos(a0), 14.0 * gg.K * math.sin(a0)); roots = []
        for da in (-0.75, -0.35, 0.05, 0.45, 0.8)[:rng.randint(3, 5)]:
            L = (17 + rng.uniform(0, 9)) * gg.K; e = (c[0] - L * math.cos(a0 + da), c[1] - L * math.sin(a0 + da))
            pts = S2.wiggle(c, e, rng, 1.4, 7)
            roots.append(unary_union([LineString(pts[i:i + 2]).buffer(rng.uniform(0.9, 1.3) * (1.0 - 0.45 * i / len(pts)) * K ** 0.5, cap_style=1, resolution=4) for i in range(len(pts) - 1)]))
        s.stack([unary_union(roots)], 1.1, ((0.45, 18), (0.35, 40), (0.2, 62)), "root")
    elif v == 1:          # a sluggish sap stream
        S2.channel(s, rng, rng.uniform(2.6, 3.6), 0.5, "bile", 4.0)
    elif v == 2:          # digesting hollows: small pools of sap with mossy banks
        S2.pools(s, rng, rng.randint(1, 3), 1.6, 2.6, 0.5, "bile")
    else:                 # a mat of thin roots netting the ground
        net = S2.j_voronoi(rng, rng.randint(7, 11), 0.42, 0.9).intersection(Point(0, 0).buffer(gg.R0 - 1.2))
        if not net.is_empty: s.stack([net], 0.5, ((0.5, 20), (0.5, 50)), "root")
    S2.banks(s, rng, rng.randint(1, 3), 1.5, 2.4, (0.4, 0.8), "moss")
    for p in s.scatter(rng.randint(1, 3), 0.9, 1.5, gap=0.5, round_=False): s.stack([p], rng.uniform(0.6, 1.0), S2.ROCKY, "rock")
    S2.grit(s, rng, 10, 22); return s


# ------------------------------------------------------------------------------------------------ tier features and the pool
def _sd(name, cls, kind):
    """one of this style's own details (sd_r_x_thornwild.py) used as a tier feature"""
    def fn(s, rng, lvl=0, scale=1.0):
        import style_details as SD
        for f in SD.NEW20.get(NAME, []):
            if getattr(f, "shape", "") == name: return f(s, rng, lvl)
        return False
    fn.tmpl, fn.kind, fn.shape, fn.cls = "comp", kind, name, cls; fn.__name__ = f"thornwild_{name}"
    return fn


def _as(fn, cls):
    fn.cls = cls; return fn


def tables(G):
    sc, si, su, pa, tr = G["scatter"], G["single"], G["sunk"], G["path"], G["trail"]
    sh = lambda n: G["sh_" + n]; SOFTT, ROCKY = G["SOFTT"], G["ROCKY"]; pat = lambda n: G["pat_" + n]
    RND = ((0.45, 12), (0.3, 38), (0.25, 62)); SPK = ((0.75, 4), (0.25, 30))
    thorn_bush = _as(sc(sh("star")(0.9, 1.3, 6, 9), (2, 4), (0.8, 1.2), "spikethorn", "mound", 2.0, SPK), "growth")
    big_thorn = _as(si(sh("star")(1.5, 2.1, 7, 10), (1.0, 1.5), "spikethorn", "mound", tiers=SPK), "growth")
    glowcaps = _as(sc(sh("disc")(0.5, 0.75, 14), (4, 7), (0.6, 0.9), "glowcap", "mound", 1.5, ((0.5, 5), (0.5, 50))), "growth")
    pods = _as(sc(sh("ell")(0.6, 0.8, 0.8, 1.1), (2, 4), (0.8, 1.2), "sporepod", "mound", 1.7, RND), "growth")
    big_pod = _as(si(sh("disc")(1.1, 1.4, 20), (1.2, 1.6), "sporepod", "mound", tiers=RND), "growth")
    roots = pa(0.9, (6, 10), 0.6, "root", "up", 1.2)
    vines = _as(pa(0.6, (7, 11), 0.45, "creeper", "up", 1.4), "growth")
    spores = _as(sc(sh("round")(0.3, 0.45), (6, 10), (0.3, 0.4), "spore", "mound", 1.4, RND), "growth")
    sap = su(sh("blob")(1.3, 1.9), (0.35, 0.5), "bile")
    bones = sc(sh("bone")(1.5, 2.1), (2, 4), (0.4, 0.5), "bone", "up")
    tier2 = [_sd("maw_flower", "growth", "maw"), thorn_bush, roots, pods]
    tier3 = [_sd("glowcap_ring", "growth", "glowcap"), _sd("thorn_vine", "growth", "creeper"), sap, glowcaps]
    tier4 = [_sd("snap_traps", "growth", "maw"), _sd("spore_pods", "growth", "sporepod"), _sd("dropped_pack", "made", "leather"), bones, vines]
    nature = G["_nature"]("rock", "pebble", True, "bile", "spikethorn")
    for f in nature:
        if getattr(f, "kind", "") == "spikethorn": f.cls = "growth"
    gear = G["_gear"]("crate", "wood", "iron")[:5]
    extra = [big_thorn, big_pod, spores, vines,
             _as(sc(sh("disc")(0.35, 0.5, 10), (5, 9), (0.5, 0.8), "glowcap", "mound", 1.2, ((0.5, 5), (0.5, 50))), "growth"),
             _as(si(sh("blob")(1.6, 2.2), (0.5, 0.8), "moss", "mound", tiers=SOFTT), "growth"),
             _as(sc(sh("blob")(0.7, 1.0), (2, 4), (0.4, 0.6), "moss", "mound", 1.8, SOFTT), "growth"),
             _as(sc(sh("tri")(0.5, 0.8), (4, 7), (0.7, 1.1), "spikethorn", "mound", 1.5, SPK), "growth"),
             pa(0.6, (6, 9), 0.45, "root", "up", 1.4),
             su(sh("disc")(0.4, 0.6, 10), (0.4, 0.5), "bile", (3, 5), 1.5),
             su(sh("wig")(5, 8, 0.45, 0.9), (0.4, 0.5), "crack"),
             tr(1.3, (5, 8)),
             si(sh("ell")(1.1, 1.4, 0.7, 0.9), (0.6, 0.8), "bone", "mound", pat("dots")(0.22))]
    pool = [x for t in (tier2, tier3, tier4) for x in t if not getattr(x, "shape", "") or getattr(x, "tmpl", "") != "comp"] + nature + gear + extra
    fill = [su(sh("crescent")(1.0, 1.4), (0.3, 0.4), "pit"), sc(sh("ell")(0.6, 0.8, 0.9, 1.2), (3, 5), (0.5, 0.7), "rock", "mound", 1.6, ROCKY),
            su(sh("star")(1.2, 1.6, 5, 6), (0.3, 0.4), "crack")]
    k = 0
    while len(pool) < 30: pool.append(fill[k % len(fill)]); k += 1
    return dict(tiers=(tier2, tier3, tier4), details=pool)


def feature_tables(F):
    C, lm = F["comp"], F["lm"]
    feats = [C(F["b_roots"]("stump", "root")), C(F["b_fairy_ring"]("glowcap", h=(0.6, 0.95))), C(F["b_tentacle"]("creeper")),
             C(F["b_nest"]("root", "sporepod")), C(F["b_skull"]("bone", 1.1)), C(F["b_ribs"]("bone")), C(F["b_tool"]("rifle")),
             C(F["b_crater"]("root", "bile")), C(F["b_crystals"]("spikethorn", 2.4)), C(F["b_pile"]("root", (4.0, 5.2), 1.0))]
    lms = [(60, lm(F["L_tree"]("stump", "root"))), (60, lm(F["L_tentacles"]("creeper", "bile"))), (60, lm(F["L_nest"]("root", "sporepod"))),
           (100, lm(F["L_circle"]("glyph", "glowcap"))), (100, lm(F["L_hull"]("hullplate", "hullplate"))), (130, lm(F["L_spires"]("spikethorn")))]
    return dict(features=feats, landmarks=lms)


def ground_big(E, B, P, scree, pebbles):
    return dict(field=("moss", 0.35, 1.8, 0.65), rocks=(2, 0.6, 1.0, 0.4, 0.7, "rock"),
                feats=[("strands", 3, dict(kind="root", hub="stump", n=(4, 7))), ("pool", 1, dict(kind="bile", rim="moss", ripples=2)),
                       ("rise", 3, dict(kind="moss", h=(0.5, 1.2), r=(0.07, 0.13))), ("outcrop", 1, dict(kind="rock", soft="moss"))])
