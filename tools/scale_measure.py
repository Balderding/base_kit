"""how large every style-only detail is drawn: python tools/scale_measure.py [style ...]  -> scale/drawn.json
For each detail: the longest side of its footprint and its height, in millimetres as drawn (the median of five draws),
and its class. The real-size tables in scale/<style>.json are set against these (see sd_reg.py)."""
import sys, os, random, statistics, json, math
KIT = os.path.dirname(os.path.dirname(os.path.abspath(__file__))); sys.path[0] = KIT
os.environ.setdefault("BASE_MM", "100"); os.environ.pop("KIT_SCALE", None); os.environ["KIT_NO_REAL"] = "1"
import gg, gg2, gg3, features, scenes, style_details as SD
features.install(vars(gg3)); scenes.install(vars(gg3)); SD.install(vars(gg3))
from sd_reg import REG
p = os.path.join(KIT, "scale", "drawn.json"); out = json.load(open(p, encoding="utf-8")) if os.path.exists(p) else {}
for st in (sys.argv[1:] or list(REG)):
    d = {}
    for f, cls, kind, name, r, ground in REG[st]:
        L = []; Hh = []
        for sd in range(5):
            try:
                foot, lay = f(0.0, 0.0, 0.3, random.Random(f"{st}{name}{sd}")); b = list(foot.minimum_rotated_rectangle.exterior.coords)
                L.append(max(math.dist(b[i], b[i + 1]) for i in range(2))); tops = []
                for x in lay:
                    if x is None: continue
                    if x[0] in ("up", "mound"): tops.append(x[2])
                    elif x[0] == "steps": tops.append(max([q[1] for q in x[2]] or [0]))
                    elif x[0] == "on": tops.append(x[3] + 1.0)
                Hh.append(max(tops or [0]))
            except Exception: pass
        if L: d[name] = {"long": round(statistics.median(L), 1), "high": round(statistics.median(Hh), 1), "class": cls, "kind": kind}
    out[st] = d; print(st, len(d), flush=True)
json.dump(out, open(p, "w", encoding="utf-8", newline="\n"), indent=1)
