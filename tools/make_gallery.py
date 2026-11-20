"""tools/make_gallery.py: the README's example pictures, made fresh by bbb_kit.exe itself for a release (never a reused
build): two bases of every size (30 ... 160 mm), each in a style drawn at random (no style twice while there are unused
ones) with a new random seed, at the kit's defaults (true-size things, the model's space, outcrops). The 3D colour
picture of each goes to examples/, the table between the EXAMPLES marks in README.md is written, and one sheet of all of
them (examples/sheet_<version>.png) is made for the release notes. Built hidden; the builds go to work/gallery/."""
import os, sys, glob, json, random, shutil, subprocess, time
from PIL import Image, ImageDraw
KIT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
EXE = os.path.join(KIT, "bbb_kit.exe"); EX = os.path.join(KIT, "examples"); WORK = os.path.join(KIT, "work", "gallery")
SIZES = [30, 40, 50, 60, 80, 100, 130, 160]
STYLES = ("badlands highland maahl corridor infestation urban mine swamp cynder targa trederra fortress beast frontier graveyard forest "
          "dungeon temple coast sundered ashveil ferrum thornwild obelis blight").split()
ver = open(os.path.join(KIT, "VERSION")).read().strip()
rng = random.Random(time.time()); pool = []; rows = []; shots = []
os.makedirs(EX, exist_ok=True); os.makedirs(WORK, exist_ok=True)
for mm in SIZES:
    row = []
    for k in (1, 2):
        if not pool: pool = STYLES[:]; rng.shuffle(pool)
        st = pool.pop(); seed = "g" + "".join(rng.choice("abcdefghjkmnpqrstuvwxyz23456789") for _ in range(5))
        out = os.path.join(WORK, f"{mm}_{k}_{st}"); os.makedirs(out, exist_ok=True)
        env = dict(os.environ, BBB_OUT_ROOT=out)
        r = subprocess.run([EXE, "--job", "build", st, "--size", str(mm), "--picks", f"{seed}:1"], env=env, capture_output=True, text=True, creationflags=0x08000000)
        ok = '"ok": 1' in r.stdout; pics = sorted(glob.glob(os.path.join(out, "picked_*", "*_3d_color.png")))
        print(f"{mm} mm {st} ({seed}): {'built' if ok and pics else 'did not build'}", flush=True)
        if not ok or not pics: row.append((st, None, "")); continue
        theme = os.path.basename(pics[-1]).split("mm_", 1)[1].rsplit(f"_{seed}_", 1)[0].replace("_", " ")
        png = f"{mm:03d}mm_{k}_{st}.png"; shutil.copy(pics[-1], os.path.join(EX, png)); row.append((st, png, theme)); shots.append((f"{st} {mm} mm: {theme}", pics[-1]))
    rows.append((mm, row))
tab = ["| Size | | |", "|---|---|---|"]
for mm, row in rows:
    tab.append(f"| **{mm} mm** | " + " | ".join(f"![{st} {mm} mm](examples/{png})<br>{st}: \"{theme}\"" if png else f"{st}: did not build" for st, png, theme in row) + " |")
p = os.path.join(KIT, "README.md"); s = open(p, encoding="utf-8", newline="").read(); a, b = "<!-- EXAMPLES -->", "<!-- /EXAMPLES -->"
if a in s and b in s:
    s = s[:s.index(a) + len(a)] + "\n" + "\n".join(tab) + "\n" + s[s.index(b):]; open(p, "w", encoding="utf-8", newline="").write(s); print("README table written")
T = 360; cols = 4; n = len(shots); sh = Image.new("RGB", (cols * T, ((n + cols - 1) // cols) * (T + 24)), (24, 26, 32)); d = ImageDraw.Draw(sh)
for i, (lab, f) in enumerate(shots):
    im = Image.open(f).convert("RGB"); im.thumbnail((T, T)); x, y = (i % cols) * T, (i // cols) * (T + 24)
    sh.paste(im, (x + (T - im.size[0]) // 2, y + 22)); d.text((x + 6, y + 5), lab, fill=(232, 220, 190))
sh.save(os.path.join(EX, f"sheet_{ver}.png")); print("sheet", f"examples/sheet_{ver}.png", n, "bases")
