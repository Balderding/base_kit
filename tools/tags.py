"""list every detail with its tags: python tools/tags.py [style]  -> template, kind, shape, scene class; flags any that are empty"""
import sys, os
sys.path[0] = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
a = [x for x in sys.argv[1:] if not x.startswith("--")]
import gg3, details, scenes
scenes.install(vars(gg3))
bad = 0; tot = 0; cls = {}
for st in ([a[0]] if a else list(gg3.TIERS)):
    seen = set()
    for src, fns in (("tier", [f for t in gg3.TIERS[st] for f in t]), ("pool", details.DETAILS.get(st, []))):
        for f in fns:
            if id(f) in seen: continue
            seen.add(id(f)); tot += 1
            t, k, sh = getattr(f, "tmpl", ""), getattr(f, "kind", ""), getattr(f, "shape", ""); c = scenes.klass(f); cls[c] = cls.get(c, 0) + 1
            miss = not (t and k and (sh or t in ("path", "comp")))
            bad += miss
            if a or miss: print(f"{st:12} {src:5} {getattr(f, '__qualname__', '?').split('.')[0]:14} {t:8} {k:10} {sh:10} {c}" + ("   <-- EMPTY TAG" if miss else ""))
print(f"{tot} details, {bad} with an empty tag. By scene class: " + ", ".join(f"{k} {v}" for k, v in sorted(cls.items())))
