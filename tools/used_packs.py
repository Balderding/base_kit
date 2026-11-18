"""mark the packs the kit really uses: python tools/used_packs.py
Loads the kit's details (every detail made from a model notes the pack it wants in sd_r_models.USED) and
sets "used": true on exactly those packs in models/packs.json (and removes it from the others), so the first run
fetches what the details need and nothing more. Prints the list and the download size. Run it after adding details."""
import sys, os, json
KIT = os.path.dirname(os.path.dirname(os.path.abspath(__file__))); sys.path[0] = KIT
os.environ.setdefault("BASE_MM", "30")
import gg, gg2, gg3, features, scenes, style_details as SD          # loading the details makes every one of them say which pack it wants
features.install(vars(gg3)); scenes.install(vars(gg3)); SD.install(vars(gg3))
import sd_r_models; used = set(sd_r_models.USED)
p = os.path.join(KIT, "models", "packs.json"); m = json.load(open(p, encoding="utf-8")); known = {x["name"] for x in m["packs"]}
for x in m["packs"]:
    if x["name"] in used: x["used"] = True
    else: x.pop("used", None)
open(p, "w", encoding="utf-8", newline="\n").write(json.dumps(m, indent=1) + "\n")
mb = sum(x.get("mb", 4) for x in m["packs"] if x.get("used"))
print(f"{len(used & known)} packs in use, about {mb:.0f} MB to fetch on a first run: " + ", ".join(sorted(used & known)))
if used - known: print("NOT in packs.json (their details will never appear on another computer): " + ", ".join(sorted(used - known)))
