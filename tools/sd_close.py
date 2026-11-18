"""close-up sheets of the style-only details: python tools/sd_close.py <style|all> [seed=0]
Each of a style's twenty alone on a small plain disc, 5 x 4 to a sheet with its name, in work/sd/close_<style>_<seed>.png.
For judging shapes only (does it read as the real thing?); these are not bases."""
import sys, os
KIT = os.path.dirname(os.path.dirname(os.path.abspath(__file__))); sys.path[0] = KIT
os.environ.setdefault("BASE_MM", "25")
import random, trimesh
from PIL import Image, ImageDraw
import gg, gg2, gg3, features, mesher, scenes, style_details as SD
features.install(vars(gg3)); scenes.install(vars(gg3)); SD.install(vars(gg3))
from sd_reg import REG
a = sys.argv[1:]; styles = list(REG) if a[0] == "all" else a[0].split(","); seed = int(a[1]) if len(a) > 1 else 0
out = os.path.join(KIT, "work", "sd"); os.makedirs(out, exist_ok=True); T = 600; Z = 4
for st in styles:
    sheet = Image.new("RGB", (5 * T, 4 * T), "white"); dr = ImageDraw.Draw(sheet)
    for i, (f, cls, kind, name, r, ground) in enumerate([x for x in REG[st] if x[3].startswith(os.environ.get("PREFIX", ""))][int(os.environ.get("SKIP", "0")):][:20]):          # PREFIX=m_ : only the model details; SKIP=20: the next twenty
        s = gg2.Scene("c", "c", "c", "earth"); s.objs, s.keepout = [], []; rng = random.Random(f"{st}{seed}{name}")
        try:
            foot, lay = f(0.0, 0.0, 0.6, rng); features._build(s, gg.H, [L for L in lay if L is not None])
            F, R0, H, rim_tan, mag, skipped = mesher.field_from_cmds(list(gg.build_cmds(s)))
            v, f_ = mesher.mesh_from_field(F, R0, H, rim_tan, mag); m = trimesh.Trimesh(v, f_, process=False); m.merge_vertices(); m, extra = mesher.add_solids(s, m)
            png = os.path.join(out, f"_tile_{os.getpid()}.png"); mesher.shot(m, png, Z * T, Z * T, 50, -65)
            im = Image.open(png).convert("RGB"); cx, cy = im.width // 2, int(im.height * 0.44)
            sheet.paste(im.crop((cx - T // 2, cy - T // 2, cx + T // 2, cy + T // 2)), ((i % 5) * T, (i // 5) * T))
        except Exception as e: dr.text(((i % 5) * T + 20, (i // 5) * T + 300), f"{type(e).__name__}", fill="navy")
        dr.text(((i % 5) * T + 12, (i // 5) * T + 8), f"{name} ({cls})", fill="navy")
    p = os.path.join(out, f"close_{st}_{os.environ.get('PREFIX', '')}{seed}.png"); sheet.save(p); print("saved", p, flush=True)
