"""the example pictures in the README: python tools/make_examples.py
Two bases of every size (30 40 50 60 80 100 130 160 mm), each in a style drawn at random (no style twice while there
are unused ones), built by the normal runner with its print check, then a picture of each STL into examples/ and the
table between the EXAMPLES marks in README.md."""
import sys, os
KIT = os.path.dirname(os.path.dirname(os.path.abspath(__file__))); sys.path[0] = KIT
import random, subprocess, glob, shutil, time
SIZES = [30, 40, 50, 60, 80, 100, 130, 160]
STYLES = "badlands highland maahl corridor infestation urban mine swamp cynder targa trederra fortress beast frontier graveyard forest dungeon temple coast".split()
rng = random.Random(time.time()); pool = []; EX = os.path.join(KIT, "examples"); os.makedirs(EX, exist_ok=True); rows = []
for mm in SIZES:
    row = []
    for k in (1, 2):
        if not pool: pool = STYLES[:]; rng.shuffle(pool)
        st = pool.pop(); before = set(glob.glob(os.path.join(KIT, "out", "run_*")))
        r = subprocess.run([sys.executable, os.path.join(KIT, "run_set.py"), st, "1", "--size", str(mm), "--detail", "4", "-q"], cwd=KIT, capture_output=True, text=True)
        new = sorted(set(glob.glob(os.path.join(KIT, "out", "run_*"))) - before); stl = glob.glob(os.path.join(new[-1], st, "*.stl")) if new else []
        line = next((l.strip() for l in r.stdout.splitlines() if l.strip().startswith(("OK", "FLAG", "FAIL"))), "no result line")
        print(f"{mm} mm {st}: {line}", flush=True)
        if not stl or not line.startswith("OK"): row.append((st, None, "")); continue
        import trimesh, mesher
        png = f"{mm:03d}mm_{k}_{st}.png"; mesher.shot(trimesh.load(stl[0]), os.path.join(EX, png), 1200, 900, 50, -65)
        theme = line.split('plan:', 1)[1].strip() if "plan:" in line else ""; row.append((st, png, theme))
    rows.append((mm, row))
tab = ["| Size | | |", "|---|---|---|"]
for mm, row in rows:
    tab.append(f"| **{mm} mm** | " + " | ".join(f"![{st} {mm} mm](examples/{png})<br>{st}: {theme.replace('table ', '')}" if png else f"{st}: did not build" for st, png, theme in row) + " |")
p = os.path.join(KIT, "README.md"); s = open(p, encoding="utf-8", newline="").read(); a, b = "<!-- EXAMPLES -->", "<!-- /EXAMPLES -->"
if a in s and b in s:
    s = s[:s.index(a) + len(a)] + "\n" + "\n".join(tab) + "\n" + s[s.index(b):]; open(p, "w", encoding="utf-8", newline="").write(s); print("README table written")
else: print("README has no EXAMPLES marks; table:\n" + "\n".join(tab))
