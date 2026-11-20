"""ashveil: the grey ash plains of a dead world (2026-11-19). Wind-rippled ash drifts, cracked hardpan, transports
half buried where they stopped, a pipeline broken off its stilts, bleached bones, scrap and the marker pylons that
once showed the way. Everything that is here belongs to this file (see newstyles.py for what a style defines); its
style-only details are in sd_r_x_ashveil.py and their real sizes in scale/real_ashveil.json.

Ground layouts (one picked per base):
  0  wind ripples: the wavy troughs of low ash ridges across the base, a drift banked up on one side
  1  cracked hardpan: plates of baked ash with dark cracks, drifts lying over part of it
  2  buried plating: a few hull plates standing out of the ash, ripple furrows round them
  3  drift edge: a long ash dune along one side of the base, a crack field on the open ground"""
import math

NAME = "ashveil"
TITLE = "Ashveil Wastes"
SAYS = "grey ash plains of a dead world, with half-buried wrecked transports, broken pipelines on stilts, bleached bones and scrap"
GROUND = "#8a8580"
PAL = dict(wood="ashscrap", iron="rust", rock="ashcinder", stone="ashstone", crate="ashcargo", water="tar", pit="pit", gem=None, floor="ashstone",
           plant=None, soft="ashdrift", bone="bone", twig="ashscrap", vein="ashpipe")
PATH_OK = "trail ruts pipe cable stones track"
EXTRA_PROPS = "oildrum tank gear grate wrench cables casings rifle"
BUILT, ENCLOSED, CLIFFS = False, False, True
COLOURS = {"ashdrift": "#a39e97", "ashstone": "#77736d", "ashcinder": "#4f4c49", "ashscrap": "#73625a", "ashhull": "#5f5a55", "ashcargo": "#6d6a5f",
           "ashpipe": "#7d7b76", "ashpylon": "#8e8a82", "ashtarp": "#857a66", "ashpaint": "#b89a3c"}

def T(name, subject, path, props, also="", **kw):          # as themes.T (not imported: themes.py loads this file while it is itself loading)
    return dict(theme=name, subject=subject, path=path, props=props, also=also, **kw)

THEMES = [
    T("buried transport", "hull", "ruts:past", "at:wheel at:crate*2 at:tank back:rubble*2 far:bones", also="depot:far", clutter=dict(debris=2.2, growth=0.0),
      ban=("ashpipe", "ashpylon", "ashtarp")),
    T("pipeline break", "machine", "pipe:past", "at:oildrum*2 at:puddle*2 at:wrench path:cables back:rubble*2", also="crater:far", clutter=dict(debris=2.2, growth=0.0),
      ban=("ashhull", "ashpylon", "ashtarp")),
    T("scavengers' camp", "camp", "trail:to", "at:crates at:sacks at:oildrum at:rifle at:wrench back:timbers", also="tent:at", clutter=dict(debris=2.2, growth=0.0),
      ban=("ashpipe", "ashpylon")),
    T("marker pylon", "dais_column", "stones:to", "front:sign path:posts*2 at:cables at:skull far:bones*2", clutter=dict(debris=2.2, growth=0.0),
      ban=("ashhull", "ashpipe", "ashtarp")),
    T("dust-choked crater", "crater", "trail:past", "at:rubble*3 at:slab*3 at:ribs far:bones at:skull back:boulder*2", clutter=dict(debris=2.6, growth=0.0),
      ban=("ashtarp", "ashpylon")),
]

_MINE = None          # this module's own names: tables() fills the rest of its globals from the kit's and must not overwrite them


def _take(G):
    global _MINE
    if _MINE is None: _MINE = {k for k in globals()}
    globals().update({k: v for k, v in G.items() if not k.startswith("__") and k not in _MINE})


# ------------------------------------------------------------------------------------------------ ground
def _ripples(s, rng, phi, D, region=None, n=(2, 3), step=(2.2, 2.8), h=(0.22, 0.3)):
    """wind ripples as background texture: a few short patches of wavy troughs where the ash lies loose (cut into
    the ground as joints, so things may still stand over them)"""
    reg = Point(0, 0).buffer(R0 - 0.8) if region is None else region.intersection(Point(0, 0).buffer(R0 - 0.8))
    if reg.is_empty: return Polygon()
    E = 15.5 * KK(); st = rng.uniform(*step); out = []
    for _ in range(NN(rng.randint(*n))):
        c0 = rp(rng, 0, 11); r = rng.uniform(3.0, 4.8); patch = blob(c0[0], c0[1], r, rng, 5).intersection(reg)
        if patch.is_empty: continue
        bands = []
        for c in np.arange(-E + rng.uniform(0, st), E, st):
            g = meander(rng, phi + rng.uniform(-0.1, 0.1), c / max(1.0, gg.K), rng.uniform(0.3, 0.6), 1.0).buffer(rng.uniform(0.24, 0.3), cap_style=1).intersection(patch.buffer(-0.3))
            if not g.is_empty and g.area > 0.4: bands.append(g)
        out += bands
    if out: s.gcut([unary_union(out)], rng.uniform(*h), "joint")          # a joint, like a floor's: things may still stand over it
    return unary_union(out) if out else Polygon()


def _shards(s, rng, n, kind="ashscrap", r=(0.7, 1.2), h=(0.35, 0.6)):
    """bits of torn sheet lying about: small sharp-cornered pieces flat on the ash"""
    for p in s.scatter(n, r[0], r[1], gap=0.4, round_=False):
        g = affinity.rotate(affinity.scale(p, 1.5, 0.7), rng.uniform(0, 180), origin="centroid")
        if s.free(g, 0.3) and Point(0, 0).buffer(R0 - 1.0).contains(g): s.prism([g], H + rng.uniform(*h), kind)


def layout(n):
    s = gg2.mk(NAME, TITLE, n, GROUND); rng = s.rng; v = rng.randint(0, 3); D = _D(); phi, f, R = _fr(rng)
    if v == 0:            # baked hardpan over most of the base, loose rippled ash and drifts beyond its broken edge
        c = rng.uniform(-6, 0) * gg.K; reg = R(-30, 30, c, 30).intersection(D)
        floor(s, reg, 0.42, "flag", j_voronoi(rng, rng.randint(14, 20), 0.26, 0.45), depth=0.34)
        rest = Point(0, 0).buffer(R0 - 0.8).difference(reg.buffer(0.6))
        _ripples(s, rng, phi, D, rest, (1, 2))
        banks(s, rng, rng.randint(1, 2), 1.6, 2.4, (0.45, 0.7), "ashdrift", ((0.5, 25), (0.5, 50)), region=rest)
    elif v == 1:          # cracked hardpan all over, ash drifted onto it in heaps
        floor(s, D, 0.42, "flag", j_voronoi(rng, rng.randint(14, 20), 0.26, 0.45), depth=0.34)
        banks(s, rng, rng.randint(2, 4), 1.5, 2.6, (0.35, 0.6), "ashdrift", ((0.5, 25), (0.5, 50)))
    elif v == 2:          # hull plates standing out of the ash among drifts and ripple patches
        _ripples(s, rng, phi, D, None, (2, 3))
        for p in s.scatter(rng.randint(2, 3), 1.4, 2.0, gap=0.6, round_=False):
            g = affinity.rotate(affinity.scale(p, 1.8, 0.55), rng.uniform(0, 180), origin="centroid")
            if s.free(g, 0.4) and Point(0, 0).buffer(R0 - 1.0).contains(g): s.stack([g], rng.uniform(0.7, 1.1), ((0.8, 6), (0.2, 30)), "ashhull")
        banks(s, rng, rng.randint(1, 3), 1.6, 2.6, (0.45, 0.75), "ashdrift", ((0.5, 25), (0.5, 50)))
        cracks(s, rng, rng.randint(2, 3), 0.35)
    else:                 # a narrow dune along one side, cracked hardpan on the open ground
        c = rng.uniform(6, 9) * gg.K
        dune = R(-30, 30, c, 30).intersection(Point(0, 0).buffer(R0 - 0.6))
        if not dune.is_empty: s.stack([dune], rng.uniform(0.5, 0.75), ((0.35, 18), (0.35, 35), (0.3, 55)), "ashdrift")
        floor(s, R(-30, 30, -30, c - 1.5).intersection(D), 0.4, "flag", j_voronoi(rng, rng.randint(14, 20), 0.26, 0.45), depth=0.32)
    for p in s.scatter(rng.randint(3, 6), 0.8, 1.4, gap=0.5, round_=False): s.stack([p], rng.uniform(0.5, 0.9), ROCKY, "ashcinder")
    _shards(s, rng, rng.randint(2, 4))
    grit(s, rng, 10, 18, "ashcinder"); return s


# ------------------------------------------------------------------------------------------------ the style's own features
def _plate(s, rng, lvl):
    """a torn hull plate standing slanted out of the ash, a row of rivet holes along its edge"""
    def mk():
        cx, cy = rp(rng, 1, 9); a = rng.uniform(0, math.pi); ff = frame(cx, cy, a); L = rng.uniform(2.6, 3.6); W = rng.uniform(1.0, 1.4)
        return Polygon([ff(-L / 2, -W / 2), ff(L / 2 * rng.uniform(0.6, 1.0), -W / 2), ff(L / 2, W / 2 * rng.uniform(-0.2, 0.6)), ff(L / 2 * rng.uniform(0.2, 0.7), W / 2), ff(-L / 2, W / 2)]).buffer(0)
    g, z = find(s, mk, gap=0.4)
    if g is None: return
    top = up(s, [g], z, rng.uniform(0.9, 1.4), "ashhull")
    if top:
        c = g.centroid; holes = [Point(p).buffer(0.17, 6) for p in list(inset(g, 0.4).exterior.coords)[:-1:2]] if not inset(g, 0.4).is_empty else []
        if holes: topcut(s, [unary_union(holes)], top, 0.25, "slot")


def _stilts(s, rng, lvl):
    """the stilts of a pipeline still standing in a row, the pipe gone: square posts with a saddle cut in each top"""
    def at(cx, cy):
        a = rng.uniform(0, math.pi); k = rng.randint(2, 3); gap = rng.uniform(4.0, 4.8); ff = frame(cx, cy, a)
        return [rect(*ff((i - (k - 1) / 2) * gap, 0), 1.0, 1.0, a) for i in range(k)]
    ps, z = place_group(s, rng, at, gap=0.4)
    if not ps: return
    top = up(s, ps, z, rng.uniform(1.7, 2.3), "ashpipe")
    if top: topcut(s, [unary_union([p.centroid.buffer(0.32, 8) for p in ps])], top, 0.35, "slot")


def _drums(s, rng, lvl):
    """fuel drums standing in the ash, their lids ringed"""
    def at(cx, cy): return cluster(s, rng, rng.randint(2, 3), lambda x, y: Point(x, y).buffer(rng.uniform(0.85, 1.0), 20), cx, cy, 2.2, 0.3)
    ps, z = place_group(s, rng, at, gap=0.4)
    if not ps: return
    top = up(s, ps, z, rng.uniform(1.4, 1.8), "rust")
    if top: topcut(s, [unary_union([p.centroid.buffer(0.55, 16).difference(p.centroid.buffer(0.32, 16)) for p in ps])], top, 0.2, "slot")


def _cls(fn, c):
    fn.cls = c; return fn


def _kind(fn, k, shape=""):
    """name what a feature is made of, so a theme's ban list can leave it out"""
    def w(s, rng, lvl): return fn(s, rng, lvl)
    w.kind = k; w.shape = shape; w.cls = "made"; w.__qualname__ = w.__name__ = f"ashveil_{shape or k}"; return w


def _sd(name, kind, n=(1, 1)):
    """one of the style-only details (sd_r_x_ashveil.py) as a tier feature: it is looked up when first used (the
    register is filled after tables() runs), and it carries its kind so each theme keeps only its own"""
    hold = []
    def w(s, rng, lvl):
        if not hold:
            import features, sd_reg
            got = [x for x in sd_reg.REG.get(NAME, []) if x[3] == name]
            hold.append(features.comp(got[0][0], r=got[0][4], ground=got[0][5]) if got else None)
        if not hold[0]: return False
        got = False
        for _ in range(rng.randint(*n)): got = bool(hold[0](s, rng, lvl)) or got
        return got
    w.kind = kind; w.shape = name; w.cls = "made"; w.__qualname__ = w.__name__ = f"ashveil_{name}"; return w


def tables(G):
    _take(G)
    plate = single(sh_rect(2.6, 3.4, 1.0, 1.4), (0.8, 1.2), "ashhull", "up", pat_seams(0.8))
    pipe = single(sh_caps(4.5, 6.5, 1.1), (0.7, 0.9), "ashpipe", "up", pat_seams(1.6))
    drift = _cls(single(sh_blob(1.8, 2.6), (0.5, 0.8), "ashdrift", "mound", tiers=SOFTT), "debris")
    cinders = _cls(scatter(sh_rock(0.8, 1.3), (3, 5), (0.5, 0.9), "ashcinder"), "debris")
    bones = scatter(sh_bone(1.5, 2.2), (2, 4), (0.4, 0.5), "bone", "up")
    scrapbits = _cls(scatter(sh_tri(0.7, 1.1), (3, 6), (0.4, 0.6), "ashscrap", "up", 1.8), "debris")
    wreck = _cls(_sd("drift_wreck", "ashdrift", (1, 2)), "debris"); litter = _cls(_sd("scrap_litter", "ashscrap", (1, 2)), "debris")
    tier2 = [_kind(_plate, "ashhull", "plate"), bones, cinders, scrapbits, wreck, litter,
             _sd("buried_cab", "ashhull", (1, 2)), _sd("pylon_stub", "ashpylon", (1, 2)), _sd("valve_head", "ashpipe", (2, 3)), _sd("lean_to", "ashtarp"), _sd("ash_still", "ashtarp")]
    tier3 = [_kind(_stilts, "ashpipe", "stilts"), pipe, drift, wreck, _sd("track_links", "ashhull"), _sd("hub_wheel", "ashhull", (1, 2)), _sd("stilt_pipe", "ashpipe"),
             _sd("warning_sign", "ashpylon", (1, 2)), _sd("cell_panel", "ashpylon"), _sd("salvage_sled", "ashtarp"), _sd("visor_helmet", "ashtarp"), _sd("rebar_slab", "ashstone", (2, 3)), _cls(_sd("cinder_rocks", "ashcinder", (1, 2)), "debris")]
    tier4 = [_kind(f_column("ashpylon"), "ashpylon", "pylon"), _kind(f_vent("tar", "ashpipe"), "ashpipe", "pipe_mouth"), path(0.45, (8, 12), 0.4, "cable", "up", 1.0), _kind(_drums, "rust", "drums"),
             _sd("cargo_container", "ashhull"), _sd("fallen_pylon", "ashpylon"), _sd("toppled_drums", "ashpipe", (1, 2)), _sd("breath_mask", "ashtarp"), _sd("scrap_heap", "ashscrap"), litter]
    fill = [sunk(sh_crescent(1.0, 1.4), (0.3, 0.4), "pit"), _cls(scatter(sh_ell(0.6, 0.8, 0.9, 1.2), (3, 5), (0.5, 0.7), "ashcinder", "mound", 1.6, ROCKY), "debris"),
            sunk(sh_star(1.2, 1.6, 5, 6), (0.3, 0.4), "crack"), sunk(sh_rect(1.8, 2.4, 1.0, 1.4), (0.4, 0.6), "pit")]
    nat = _nature("ashcinder", "ashcinder", True, "tar", None); nat = [_cls(x, "debris") for x in nat[:4]] + nat[4:]
    det = (nat + _gear("ashcargo", "ashscrap", "rust") + tier2 + tier3 + tier4
           + [plate, scrapbits, _cls(scatter(sh_blob(0.8, 1.2), (2, 4), (0.4, 0.6), "ashdrift", "mound", 1.8, SOFTT), "debris"),
              sunk(sh_wig(5, 8, 0.5, 0.9), (0.35, 0.45), "rut"), trail(1.6, (5, 8), sh_rect(0.9, 1.1, 0.45, 0.55), 0.3, "rut"),
              single(sh_disc(1.0, 1.3, 24), (1.3, 1.7), "rust", "up", pat_ring(0.25)), single(sh_ring(1.3, 1.7, 0.5), (0.5, 0.7), "rust", "up"),
              sunk(sh_blob(1.4, 2.0), (0.35, 0.5), "tar")])
    k = 0
    while len(det) < 30: det.append(fill[k % len(fill)]); k += 1
    return dict(tiers=(tier2, tier3, tier4), details=det)


def feature_tables(F):
    C, lm = F["comp"], F["lm"]
    feats = [C(F["b_wheel"]("ashscrap", "rust")), C(F["b_skull"]("bone", 1.2)), C(F["b_ribs"]("bone")), C(F["b_tool"]("wrench", "rust", "ashscrap")),
             C(F["b_pile"]("ashpipe", (3.6, 4.8), 0.9)), C(F["b_cracked_slab"]("ashstone")), C(F["b_crater"]("ashcinder", "pit")), C(F["b_gear"]("rust")),
             C(F["b_hoops"]("rust", (2.8, 3.4), 1.8)), C(F["b_sign"]("rust", "ashscrap"))]
    lms = [(60, lm(F["L_hull"]("ashhull", "ashscrap"))), (60, lm(F["L_crater"]("ashcinder", "pit"))), (60, lm(F["L_skeleton"]("bone"))),
           (100, lm(F["L_dais"]("ashstone", "column"))), (100, lm(F["L_track"]("rust", "ashscrap"), 0.35)), (130, lm(F["L_wall"]("ashstone")))]
    return dict(features=feats, landmarks=lms)


def ground_big(E, B, P, scree, pebbles):
    return dict(field=("ashdrift", 0.3, 1.6, 0.45), rocks=(2, 0.5, 1.0, 0.3, 0.7, "ashcinder"),
                feats=[("ridge", 2, dict(kind="ashdrift", h=(0.6, 1.8), w=(2.6, 4.2))), ("cracked_flat", 2, dict(kind="crack")),
                       ("crater", 1, dict(rim=B, pit="pit", ejecta=(8, 0.5, 1.0, 0.3, 0.7, "ashcinder"))),
                       ("slabs", 1, dict(kind="ashhull", h=(0.3, 1.2))), ("outcrop", 1, dict(kind="ashcinder", h=(1.2, 2.6), soft="ashdrift"))])
