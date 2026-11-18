"""Details made from ready-made public-domain models (see sd_mesh.py and models/SOURCES.md). Each line of the tables
is one detail: the same model never comes out twice the same, because its pose (standing, lying, tipped), its size, its
lean, how deep it sits in the ground, its facing and its mirror image are all drawn afresh."""
import math
from sd_reg import d
import sd_mesh

USED = set()

def _model(style, name, rel, kind, mm=(5.5, 7.0), poses="s", cls="made", sink=(0.0, 0.0), tilt=0.22, cap=None, with_=()):
    """poses: letters from sd_mesh.POSES, one is drawn (repeat a letter to make it likelier). with_: (model, mm factor)
    small companions set down beside it."""
    import os
    USED.update(x[0].replace("\\", "/").split("/")[0] for x in [(rel,)] + list(with_))          # the packs the details ask for, here or not (tools/used_packs.py reads this)
    if not os.path.exists(os.path.join(sd_mesh.ROOT, rel)): return None          # the pack is not on this computer (tools\get_models.py fetches it): no such detail, the kit goes on without it
    with_ = [w for w in with_ if os.path.exists(os.path.join(sd_mesh.ROOT, w[0]))]
    @d(style, cls, kind, name)
    def _(cx, cy, a, rng):
        u = rng.uniform; pose = rng.choice(poses); lim = cap or (gg.max_height() * 0.92)
        sol, foot, top = sd_mesh.solid(rel, u(*mm), cx, cy, a, pose, u(*sink), rng.random() < 0.5, u(0, tilt) if pose != "s" or rng.random() < 0.5 else 0.0, u(0, 6.28), lim)
        lay = [_fine(sol, top, kind)]; foots = [foot]
        for rel2, k in with_:
            if rng.random() < 0.6:
                r = math.sqrt(foot.area / 3.14) + u(0.8, 1.6); p = pol(cx, cy, r, u(0, 6.28))
                s2, f2, t2 = sd_mesh.solid(rel2, u(*mm) * k, p[0], p[1], u(0, 6.28), rng.choice("sbl"), 0.0, rng.random() < 0.5, u(0, 0.3), u(0, 6.28), lim)
                if not f2.intersects(foot): lay.append(_fine(s2, t2, kind)); foots.append(f2)
        return hull(foots), lay
    return _

class _Later(list):
    """the pieces of a "steps" layer, worked out the first time anything looks at them. A detail is tried in many
    places before one fits; the tries that are thrown away never look, so they never pay for the slicing."""
    def __init__(self, make): super().__init__(); self._make = make
    def _fill(self):
        if self._make is not None: m, self._make = self._make, None; super().extend(m())
    def __iter__(self): self._fill(); return super().__iter__()
    def __len__(self): self._fill(); return super().__len__()
    def __getitem__(self, i): self._fill(); return super().__getitem__(i)
    def __bool__(self): self._fill(); return super().__len__() > 0
    def __contains__(self, x): self._fill(); return super().__contains__(x)
    def __reversed__(self): self._fill(); return super().__reversed__()
    def __add__(self, o): self._fill(); return list(super().__iter__()) + list(o)
    def __radd__(self, o): self._fill(); return list(o) + list(super().__iter__())
    def __eq__(self, o): self._fill(); return list(super().__iter__()) == o
    def __reduce__(self): self._fill(); return (list, (list(super().__iter__()),))

def _fine(sol, top, kind):
    """a "steps" layer cut finer than ST() cuts it (a step every 0.1 mm, 40 at most): a model's small faces need it.
    Its outline is the model's own outline on the ground; the steps themselves come later (_Later)."""
    import scenes, features
    out = sol[0][0](0.0)
    if out is None or out.is_empty: return ("up", Polygon(), 0.0, kind)
    return ("steps", out, _Later(lambda: scenes.sculpt(sol, (), max(8, min(60, int(top * features.true_scale() / 0.1))))), kind)

G = "graveyard-kit/Models/OBJ format/"
for name, f, kind, mm, poses, kw in [
    ("m_gravestone_cross", "gravestone-cross", "headstone", (4.2, 5.2), "sbbf", {}),
    ("m_gravestone_round", "gravestone-round", "headstone", (4.6, 5.6), "ssbf", {}),
    ("m_gravestone_bevel", "gravestone-bevel", "headstone", (4.6, 5.6), "ssbf", {}),
    ("m_gravestone_roof", "gravestone-roof", "headstone", (4.6, 5.6), "ssb", {}),
    ("m_gravestone_wide", "gravestone-wide", "headstone", (4.6, 5.6), "ssb", {}),
    ("m_gravestone_fancy", "gravestone-decorative", "headstone", (5.0, 6.0), "ssbf", {}),
    ("m_gravestone_broken", "gravestone-broken", "headstone", (5.0, 6.0), "s", dict(with_=[("gravestone-debris.obj", 1.0)])),
    ("m_wood_cross", "cross-wood", "wood", (4.2, 5.0), "bbfs", {}),
    ("m_stone_cross", "cross", "headstone", (4.2, 5.0), "bbfs", {}),
    ("m_coffin", "coffin", "wood", (7.0, 8.5), "s", dict(sink=(0.0, 0.3))),
    ("m_coffin_old", "coffin-old", "wood", (7.0, 8.5), "s", dict(sink=(0.0, 0.3))),
    ("m_lantern", "lantern-candle", "iron", (6.0, 7.5), "sslr", {}),
    ("m_lantern_glass", "lantern-glass", "iron", (6.0, 7.5), "sslr", {}),
    ("m_urn", "urn-round", "headstone", (6.0, 7.5), "sslr", {}),
    ("m_urn_square", "urn-square", "headstone", (6.0, 7.5), "sslr", {}),
    ("m_pumpkin", "pumpkin-carved", "shrub", (6.0, 8.0), "s", dict(with_=[("pumpkin.obj", 0.8)])),
    ("m_candles", "candle-multiple", "wax", (8.0, 10.0), "s", {}),
    ("m_shovel", "shovel-dirt", "iron", (5.5, 6.5), "s", {}),
    ("m_fire_basket", "fire-basket", "iron", (7.0, 8.5), "s", {}),
    ("m_obelisk", "pillar-obelisk", "headstone", (4.0, 5.0), "sbl", {}),
    ("m_grave", "grave", "earth", (5.0, 6.0), "s", {}),
    ("m_bench", "bench-damaged", "wood", (5.5, 6.5), "s", {}),
    ("m_chalice", "detail-chalice", "gold", (9.0, 11.0), "slr", {}),
    ("m_altar", "altar-stone", "headstone", (5.0, 6.0), "s", {}),
]:
    _model("graveyard", name, G + f + ".obj", kind, mm, poses, **({**kw, "with_": [(G + m, k) for m, k in kw.get("with_", [])]} if kw else {}))
