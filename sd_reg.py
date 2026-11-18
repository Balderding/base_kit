"""The register of the style-only details (see style_details.py). Kept in its own file so the part files can import it
without loading anything else.

  @d(style, class, kind, name)
  def _(cx, cy, a, rng): ... return footprint, layers

class: made (a thing somebody left: placed near the subject, the path or out on its own), debris / growth (natural:
gathers where the scene says such things gather), marks (cut into the ground). kind is the material, which is what a
theme's ban list looks at. Every builder is called `_` on purpose: a real name could hide one of the kit's helpers."""
REG = {}
SCALE = {}          # (style, name) -> how many times larger than drawn the detail is laid (see scaled(); empty = everything as drawn)
import os as _os
if _os.environ.get("KIT_SCALE_FILE"):          # a trial table: {"style|name": factor}
    import json as _json
    try: SCALE.update({tuple(k.split("|", 1)): v for k, v in _json.load(open(_os.environ["KIT_SCALE_FILE"], encoding="utf-8")).items()})
    except Exception: pass

def scaled(out, cx, cy, k):
    """a detail's (footprint, layers) made k times larger about its own middle, heights and depths too. Larger is
    always safe (every part only gets wider); smaller than about 0.8 would thin parts below what prints."""
    from shapely import affinity
    def S(g): return g if g is None or getattr(g, "is_empty", True) else affinity.scale(g, k, k, origin=(cx, cy))
    foot, layers = out; lay = []
    for L in layers:
        if L is None: lay.append(L); continue
        t = L[0]
        if t == "up": L2 = (t, S(L[1]), L[2] * k) + tuple(L[3:])
        elif t == "mound": L2 = (t, S(L[1]), L[2] * k) + tuple(L[3:])
        elif t == "steps":
            def pieces(src=L[2]): return [(S(q[0]), q[1] * k, q[2], q[3] * k) + tuple(q[4:]) for q in src]
            # pieces that are only worked out when looked at (a model's slices, sd_r_models._Later) stay that way: a detail is tried in many places and most tries are thrown away
            L2 = (t, S(L[1]), type(L[2])(pieces) if hasattr(L[2], "_make") else pieces()) + tuple(L[3:])
        elif t == "on": L2 = (t, S(L[1]), L[2], L[3] * k) + tuple(L[4:])
        elif t == "tcut": L2 = (t, S(L[1]), L[2], L[3] * k) + tuple(L[4:])
        elif t == "gcut": L2 = (t, S(L[1]), L[2] * min(k, 1.5)) + tuple(L[3:])          # a cut into the ground may not go through the base
        elif t == "pad": L2 = (t, S(L[1])) + tuple(L[2:])
        else: L2 = L
        lay.append(L2)
    return S(foot), lay

SHOW_SHARE = 0.08          # with a scenery scale: a thing shorter than this share of the base is not drawn on it
_REAL = {}          # with a scenery scale: (style, name) -> the factor that brings the detail to its real size (see real_factor)

def real_factor(style, name, cls):
    """With a scenery scale every object must be true to the miniature. Everything is built `scale` times larger than
    laid out; a detail drawn at d mm whose real thing is r metres long must come out at r x 6 x scale mm (6 mm to the
    metre is the kit's layout scale: x3 gives 18 mm to the metre, right for a 32 mm figure). So it is laid at
    r x 6 / d times its drawn size. scale/real.json holds r for every detail ({style: {name: metres}}; 0 or missing =
    no real size, a rock or a crack: left as drawn), scale/drawn.json holds d (tools/scale_measure.py). A thing
    is never laid so small that it would print under 3 mm long (a coin, a cartridge: shown larger than life, as
    miniature scenery always has), and no detail is changed by more than four times either way."""
    key = (style, name)
    if key in _REAL: return _REAL[key]
    k = 1.0
    try:
        sc = float(_os.environ.get("KIT_SCALE", 1) or 1)
        if sc != 1.0 and not _os.environ.get("KIT_NO_REAL"):
            import json
            if "tables" not in _REAL:
                here = _os.path.dirname(_os.path.abspath(__file__)); t = {}
                for fn in ("real", "drawn"):
                    try: t[fn] = json.load(open(_os.path.join(here, "scale", fn + ".json"), encoding="utf-8"))
                    except Exception: t[fn] = {}
                _REAL["tables"] = t
            t = _REAL["tables"]; r = float(t["real"].get(style, {}).get(name, 0) or 0); dr = float(t["drawn"].get(style, {}).get(name, {}).get("long", 0) or 0)
            D = float(_os.environ.get("KIT_FINAL_MM", 0) or 0)
            if r > 0 and dr > 0:
                want = r * 6.0
                want = max(want, 3.0 / sc)          # nothing with a real size is laid so small that it prints under 3 mm
                k = max(0.25, min(4.0, want / dr))
                # A bigger base carries a bigger model (a 32 mm base a 35 mm trooper, a 160 mm base a 175 mm war
                # machine, all at the one scale). Beside it a bottle or a skull is too small to be worth drawing: a
                # thing shorter than a twelfth of the base is left out, so a large base shows only large things.
                if D and dr * k * sc < SHOW_SHARE * D: k = 0.0
            elif dr > 0 and D:
                # No real size (a rock, a drift, a crater, a growth): ground. Ground is as large as the base makes
                # sense of: boulders for a war machine, stones for a trooper.
                k = max(0.8, min(2.2, (D / 40.0) ** 0.6))
    except Exception: k = 1.0
    _REAL[key] = k; return k

def d(style, cls, kind, name, r=10.5, ground=False):
    def w(f0):
        def f(cx, cy, a, rng):
            k = SCALE.get((style, name), SCALE.get("*", 1.0)) * real_factor(style, name, cls)
            if k <= 0: return None          # too small to be worth drawing on a base this size
            out = f0(cx, cy, a, rng)
            return out if abs(k - 1.0) < 1e-6 or out is None or out[0] is None else scaled(out, cx, cy, k)
        lst = REG.setdefault(style, []); new = (f, cls, kind, name, r, ground)
        for i, x in enumerate(lst):          # the same name again: the later one takes its place (sd_r.py redraws weak ones this way)
            if x[3] == name: lst[i] = new; break
        else: lst.append(new)
        return f0
    return w
