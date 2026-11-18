"""put the per-style real-size tables together: python tools/scale_merge.py  -> scale/real.json
scale/real_<style>.json holds, for every style-only detail, the real-world length in metres of the longest side of
its footprint as drawn (0 = no real size: a rock, a crack). sd_reg.real_factor() reads the merged file."""
import sys, os, json, glob
KIT = os.path.dirname(os.path.dirname(os.path.abspath(__file__))); out = {}
drawn = json.load(open(os.path.join(KIT, "scale", "drawn.json"), encoding="utf-8"))
for f in sorted(glob.glob(os.path.join(KIT, "scale", "real_*.json"))):
    st = os.path.basename(f)[5:-5]; t = json.load(open(f, encoding="utf-8")); out[st] = t
    miss = sorted(set(drawn.get(st, {})) - set(t)); extra = sorted(set(t) - set(drawn.get(st, {})))
    print(f"{st:<12} {len(t):3d} details, {sum(1 for v in t.values() if not v):2d} with no real size" + (f"  MISSING {miss}" if miss else "") + (f"  NOT DRAWN {extra}" if extra else ""))
json.dump(out, open(os.path.join(KIT, "scale", "real.json"), "w", encoding="utf-8", newline="\n"), indent=1, sort_keys=True)
print(f"{len(out)} styles of {len(drawn)} -> scale/real.json" + ("" if len(out) == len(drawn) else "   STILL TO COME: " + ", ".join(sorted(set(drawn) - set(out)))))
