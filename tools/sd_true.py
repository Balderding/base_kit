"""close-ups of details as they really print at true scale: python tools/sd_true.py <style> [names,comma,separated] [--seed N] [--out file.png]
Each detail alone, laid at its real size (sd_reg.real_factor) and built three times larger, as a 100 mm base at the
32 mm figure scale builds it, with a 32 mm figure's outline (a 9 mm disc and its height ring) beside it for scale.
For judging whether an object reads and has enough detail at the size it is printed. Default: the style's objects
listed in scale/redraw_targets.json; out: work/sd/true_<style>.png"""
import sys, os
KIT = os.path.dirname(os.path.dirname(os.path.abspath(__file__))); sys.path[0] = KIT
os.environ.update({"KIT_SCALE": "3", "KIT_FINAL_MM": "100", "BASE_MM": "33.3", "KIT_QUIET": "1"})
import json, random, trimesh
from PIL import Image, ImageDraw
import gg, gg2, gg3, features, mesher, scenes, style_details as SD
features.install(vars(gg3)); scenes.install(vars(gg3)); SD.install(vars(gg3))
from sd_reg import REG
a = sys.argv[1:]
def opt(n, d):
    if n in a: i = a.index(n); v = a[i + 1]; del a[i:i + 2]; return v
    return d
seed = int(opt("--seed", "0")); out = opt("--out", ""); st = a[0]
names = a[1].split(",") if len(a) > 1 else [t["name"] for t in json.load(open(os.path.join(KIT, "scale", "redraw_targets.json"), encoding="utf-8")).get(st, [])]
rows = [x for x in REG[st] if x[3] in names]
T = 520; cols = 4; sheet = Image.new("RGB", (cols * T, max(1, (len(rows) + cols - 1) // cols) * T), "white"); dr = ImageDraw.Draw(sheet)
tmp = os.path.join(KIT, "work", "sd"); os.makedirs(tmp, exist_ok=True)
for i, (f, cls, kind, name, r, ground) in enumerate(rows):
    x0, y0 = (i % cols) * T, (i // cols) * T
    try:
        s = gg2.Scene("c", "c", "c", "earth"); s.objs, s.keepout = [], []; got = f(0.0, 0.0, 0.6, random.Random(f"{st}{seed}{name}"))
        if got is None: dr.text((x0 + 12, y0 + 30), "not drawn at 100 mm (too small)", fill="navy"); dr.text((x0 + 12, y0 + 8), name, fill="navy"); continue
        foot, lay = got; features._build(s, gg.H, [L for L in lay if L is not None])
        b = foot.bounds; span = max(b[2] - b[0], b[3] - b[1]) * 3; disc = min(100.0, max(30.0, 2 * 3 * max(abs(v) for v in b) + 24))          # a disc just large enough to hold it, so the close-up is close
        groups = mesher.rescale(list(gg.build_cmds(s)), 3.0, disc)
        F, R0, H, rim_tan, mag, skipped = mesher.field_from_cmds(groups)
        v, f_ = mesher.mesh_from_field(F, R0, H, rim_tan, mag); m = trimesh.Trimesh(v, f_, process=False); m.merge_vertices()
        fig = trimesh.creation.cylinder(radius=4.5, height=32, sections=24).apply_translation([disc / 2 - 7, -(disc / 2 - 7) * 0.4, 4 + 16])          # a 32 mm figure, as a plain post, for scale
        png = os.path.join(tmp, f"_true_{os.getpid()}.png"); mesher.shot(trimesh.util.concatenate([m, fig]), png, T * 2, T * 2, 40, -60)
        im = Image.open(png).convert("RGB").resize((T, T)); sheet.paste(im, (x0, y0))
        dr.text((x0 + 12, y0 + 8), f"{name} ({cls}) {span:.0f} mm long; the post is a 32 mm figure", fill="navy")
    except Exception as e:
        dr.text((x0 + 12, y0 + 8), name, fill="navy"); dr.text((x0 + 12, y0 + 30), f"{type(e).__name__}: {str(e)[:60]}", fill="navy")
p = out or os.path.join(tmp, f"true_{st}.png"); sheet.save(p); print("saved", p)
