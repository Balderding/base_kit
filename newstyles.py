"""Styles kept one per file: style_<name>.py (2026-11-19). Each file holds everything a style is, and this module
hands each part to the table that wants it, so a new style never has to edit the kit's shared tables.

What a style_<name>.py defines (see style_sundered.py for a full one):
  NAME, TITLE, SAYS                  the style's name (as typed: run_set.py <name>), its title, what the designer is told
  GROUND                             the ground colour "#rrggbb" (gg2.mk)
  PAL                                dict for scenes._P: which material plays which part (wood, rock, stone, iron, crate,
                                     floor, plant, soft, twig, vein, gem, water)
  PATH_OK                            the ways that suit the ground, first = fallback ("way stones ruts cable ...")
  EXTRA_PROPS                        extra prop names this style may use (scenes._EXTRA), "" for none
  BUILT, ENCLOSED, CLIFFS            True/False: masonry backdrop / a wall or cave side behind / sometimes a cliff
  COLOURS                            {kind: "#rrggbb"} for every new material kind the style uses (gg.COL)
  THEMES                             five themes, themes.T(...) each (subjects from scenes.SUBJECTS, props from
                                     scenes.PROP_NAMES or EXTRA_PROPS)
  layout(n)                          the ground layout for base n (like styles2.coast); returns the scene
  tables(G)                          -> dict(tiers=(tier2, tier3, tier4), details=[...]): G holds the names of
                                     details.py and styles2.py (scatter, single, sunk, trail, path, sh_*, pat_*, ROCKY,
                                     SOFTT, f_post_row, f_column, f_vent, _nature, _gear ...; styles2's pad() is local
                                     to its _build_tables, so pad the pool yourself)
  feature_tables(F)                  -> dict(features=[...], landmarks=[(smallest mm, lm(...)), ...]): F holds the
                                     names of features.py (comp, lm, b_*, L_*; the kit's C(...) is features.comp)
  ground_big(E, B, P, scree, pebbles) -> the dict ground_big.STYLE gives for big bases at figure scale
Its style-only details go in sd_r_x_<name>.py (loaded by style_details like every sd_r_*.py file), and the real
sizes of those details in scale/real_<name>.json (tools/scale_measure.py then measures how big they are drawn)."""
import glob, os, importlib

HERE = os.path.dirname(os.path.abspath(__file__))
_MODS = None
_BY = {}          # new material -> (the style that named it, its colour)


_LOADING = [False]


def mods():
    """the style modules, in name order. A style file may import themes (for themes.T): that import calls
    hook_themes, which comes back here while the styles are still loading; it then gets nothing, and the themes are
    filled in once every style has loaded (below), so none is lost."""
    global _MODS
    if _MODS is not None: return _MODS
    if _LOADING[0]: return []
    _LOADING[0] = True
    try:
        got = []
        for p in sorted(glob.glob(os.path.join(HERE, "style_*.py"))):
            nm = os.path.basename(p)[:-3]
            if nm == "style_details": continue          # the kit's own style-only details module, not a style
            got.append(importlib.import_module(nm))
        _MODS = got
    finally: _LOADING[0] = False
    import sys
    th = sys.modules.get("themes")
    if th is not None and hasattr(th, "THEMES"): hook_themes(th.THEMES)          # themes was loaded first (or by a style file): fill it now
    return _MODS


def names():
    return [m.NAME for m in mods()]


def hook_themes(THEMES):
    for m in mods(): THEMES[m.NAME] = list(m.THEMES)


def hook_scenes(G):
    """scenes.py, at its end: palette, ways, what the style is, extra props, backdrop kinds"""
    for m in mods():
        G["PAL"][m.NAME] = G["_P"](**m.PAL)
        G["PATH_OK"][m.NAME] = m.PATH_OK
        G["STYLE_SAYS"][m.NAME] = m.SAYS
        G["_EXTRA"][m.NAME] = getattr(m, "EXTRA_PROPS", "")
        for flag, setname in (("BUILT", "BUILT"), ("ENCLOSED", "ENCLOSED"), ("CLIFFS", "CLIFFS")):
            if getattr(m, flag, False) and setname in G: G[setname].add(m.NAME)


def hook_gg3(G):
    """gg3.py, after styles2: the layout, the tier features and the detail pool, the colours"""
    import gg, details, styles2
    ns = dict(vars(details)); ns.update(vars(styles2))
    for m in mods():
        for k, v in getattr(m, "COLOURS", {}).items():
            if k in _BY and _BY[k][1].lower() != v.lower():          # two new styles giving one new material two colours: the first wins, said out loud (reusing a kit material is fine)
                __import__("kitlog").note("newstyles colours", f"{m.NAME}: material '{k}' is already {_BY[k][1]} from {_BY[k][0]} (wanted {v}); give it a style-own name")
            if k not in gg.COL: _BY[k] = (m.NAME, v)
            gg.COL.setdefault(k, v)
        G["ORIG"][m.NAME] = m.layout
        t = m.tables(ns)
        G["TIERS"][m.NAME] = t["tiers"]
        details.DETAILS[m.NAME] = list(t["details"])
        G["NOTES"][m.NAME] = [""] * 4
        styles2.TITLES[m.NAME] = m.TITLE


def hook_features(F):
    """features.extend: the new details for each style's pool and its landmarks"""
    for m in mods():
        t = m.feature_tables(F)
        F["NEW"][m.NAME] = list(t.get("features", []))
        F["LANDMARKS"][m.NAME] = list(t.get("landmarks", []))


def ground_big(E, B, P, scree, pebbles):
    out = {}
    for m in mods():
        f = getattr(m, "ground_big", None)
        if f: out[m.NAME] = f(E, B, P, scree, pebbles)
    return out
