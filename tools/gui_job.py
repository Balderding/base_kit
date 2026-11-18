"""what the window (bbb_kit.exe) starts in the background. One job per start; every line it prints is ONE JSON record
with an "ev" field, so the window never has to guess from printed text.

  python tools/gui_job.py choices <style> --size 60 --detail 4 --count 20 [--mini biped] [--supports on]
      {"ev": "seed", "seed": "x"}
      {"ev": "choice", "seed": "x", "n": 16, "img": "...png", "theme": "ore siding", "tier": 4}      one per base, as it is drawn
      {"ev": "skip", "n": 17, "why": "..."}
      {"ev": "done", "job": "choices", "count": 20}
      On the first run, before the choices: {"ev": "fetch", "i": 3, "n": 25, "title": "Nature Kit", "mb": 92} for each
      public-domain model pack as it is fetched, then {"ev": "fetched", "n": 25, "missing": 0}. What is fetched, from
      where and under which licence goes out as "log" lines first. "models_download": false in settings.json stops it.

  python tools/gui_job.py build <style> --size 60 --picks x:16,x:17,y:36 [--mini biped] [--supports on]
      {"ev": "folder", "folder": "...\\out\\picked_<date>_<time>"}          every picked base is copied here: .stl, _3d.png, _2d.png, _plan.json
      {"ev": "start", "seed": "x", "n": 16}                                 its build began
      {"ev": "retry", "seed": "x", "n": 16}                                 it failed a check; a fresh variant is being built instead
      {"ev": "built", "seed": "x", "n": 16, "ok": true, "variant": false, "stl": "...", "img": "..._3d.png", "img2": "..._2d.png", "secs": 9.7, "theme": "..."}
      {"ev": "built", "seed": "x", "n": 17, "ok": false, "why": "print check: ..."}
      {"ev": "done", "job": "build", "ok": 2, "failed": 0, "folder": "..."}

  Both also send {"ev": "log", "text": "..."} (what the kit itself printed, for the Details box) and, when something
  goes wrong before the job can start, {"ev": "error", "text": "..."}.

A pick is seed:number, the pair the choices came with. Bases of one seed are built by one run of run_set.py
(python run_set.py <style> 16,17, --seed x): the same seed and number give the same base the picture showed. A base
counts as built only when the runner said OK AND its STL file is really there."""
import sys, os, json, subprocess, re, time, shutil
KIT = os.path.dirname(os.path.dirname(os.path.abspath(__file__))); sys.path[0] = KIT
NOWIN = getattr(subprocess, "CREATE_NO_WINDOW", 0)
def ev(kind, **kw): print(json.dumps(dict(ev=kind, **kw)), flush=True)
a = sys.argv[1:]
def opt(name, d):
    if name in a: i = a.index(name); v = a[i + 1]; del a[i:i + 2]; return v
    return d
size = opt("--size", "30"); detail = opt("--detail", "mix"); count = opt("--count", "20"); mini = opt("--mini", ""); sup = opt("--supports", ""); picks = opt("--picks", ""); scale = opt("--scale", "")
if len(a) < 2 or a[0] not in ("choices", "build"): ev("error", text="usage: gui_job.py choices|build <style> ..."); sys.exit(2)
job, style = a[0], a[1]; extra = (["--mini", mini] if mini else []) + (["--supports", sup] if sup else []) + (["--scale", scale] if scale else [])
env = dict(os.environ, PYTHONWARNINGS="ignore", PYTHONUNBUFFERED="1", PYTHONIOENCODING="utf-8")

def models():
    """the first run: fetch the public-domain model packs the details are made from, saying so (tools/get_models.py)"""
    try:
        with open(os.path.join(KIT, "settings.json"), encoding="utf-8") as fh:
            if not json.load(fh).get("models_download", True): return
        import importlib.util
        sp = importlib.util.spec_from_file_location("get_models", os.path.join(KIT, "tools", "get_models.py")); gm = importlib.util.module_from_spec(sp); sp.loader.exec_module(gm)
        todo = gm.missing()
        if not todo: return
        mb = sum(p.get("mb", 4) for p in todo); gm.explain(todo, lambda t: ev("log", text=t)); left = 0
        for i, p in enumerate(todo, 1):
            ev("fetch", i=i, n=len(todo), title=p["title"], mb=round(mb))
            left += not gm.fetch(p, lambda t: ev("log", text=t)); time.sleep(1.5)
        ev("fetched", n=len(todo), missing=left)
    except Exception as e: ev("log", text=f"the model packs could not be checked ({type(e).__name__}: {e}); bases are made without the details that need them")

if job == "choices":
    models()
    out = os.path.join(KIT, "out", "choices", time.strftime("%Y%m%d_%H%M%S"))
    p = subprocess.Popen([sys.executable, "-u", os.path.join(KIT, "tools", "choices.py"), style, out, "--size", size, "--detail", detail, "--count", count, "--json"] + extra,
                         cwd=KIT, env=env, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True, encoding="utf-8", errors="replace", creationflags=NOWIN)
    n = 0
    for line in p.stdout:
        line = line.rstrip()
        if line.startswith('{"ev"'):
            print(line, flush=True); n += '"choice"' in line[:20]
        elif line: ev("log", text=line)
    p.wait(); ev("done", job="choices", count=n); sys.exit(0)

# ---- build ----
groups = {}
for part in picks.split(","):
    if ":" in part:
        s, n = part.split(":", 1)
        if n.isdigit(): groups.setdefault(s, []).append(int(n))
if not groups: ev("error", text="nothing was picked"); sys.exit(2)
dest = os.path.join(KIT, "out", "picked_" + time.strftime("%Y%m%d_%H%M%S")); os.makedirs(dest, exist_ok=True); ev("folder", folder=dest)
okc = bad = 0

def see_through(src, dst, px=480):
    """the builder's grey picture with its white surround made see-through, so it sits on the window's dark card"""
    from PIL import Image, ImageDraw
    im = Image.open(src).convert("RGBA"); w, h = im.size
    for xy in ((0, 0), (w - 1, 0), (0, h - 1), (w - 1, h - 1)):
        if im.getpixel(xy)[:3] == (255, 255, 255): ImageDraw.floodfill(im, xy, (255, 255, 255, 0), thresh=6)
    box = im.getbbox()
    if box: im = im.crop(box)
    im.thumbnail((px, px), Image.LANCZOS); im.save(dst)

for seed, nums in groups.items():
    p = subprocess.Popen([sys.executable, "-u", os.path.join(KIT, "run_set.py"), style, ",".join(map(str, nums)) + ",", "--seed", seed, "--size", size, "-q"] + extra,
                         cwd=KIT, env=env, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True, encoding="utf-8", errors="replace", creationflags=NOWIN)
    folder = ""; seen = set()
    for raw in p.stdout:
        raw = raw.rstrip()
        if not raw: continue
        ev("log", text=raw)
        m = re.search(r"folder: (.+)$", raw)
        if m and not folder: folder = m.group(1).strip(); continue
        # two workers' lines can run together on one line ("  >> a  building  OK   b  9 s ..."): take every record in it
        marks = list(re.finditer(r"(?:^|\s)(>>|OK|FLAG)\s+(\S+_\d+)(?=\s|$)", raw))
        for q, m in enumerate(marks):
            tok, name = m.group(1), m.group(2); rest = raw[m.end():marks[q + 1].start() if q + 1 < len(marks) else len(raw)].strip(); n = int(name.rsplit("_", 1)[1])
            if n not in nums: continue
            if tok == ">>": ev("retry" if "retry" in rest else "start", seed=seed, n=n); continue
            seen.add(n)
            stl = os.path.join(folder, style, name + ".stl")
            if tok == "OK" and os.path.exists(stl):
                th = re.search(r'plan: \w+ "([^"]*)"', rest); theme = th.group(1) if th else ""; sm = re.match(r"([\d.]+) s", rest)
                tag = re.sub(r"[^a-z0-9]+", "_", theme.lower()).strip("_"); new = f"{style}_{float(size):g}mm_{tag + '_' if tag else ''}{seed}_{n}"
                dstl = os.path.join(dest, new + ".stl"); img = img2 = ""
                try:
                    shutil.copy2(stl, dstl)
                    if os.path.exists(stl[:-4] + "_plan.json"): shutil.copy2(stl[:-4] + "_plan.json", os.path.join(dest, new + "_plan.json"))
                    if os.path.exists(stl[:-4] + "_iso.png"): img = os.path.join(dest, new + "_3d.png"); see_through(stl[:-4] + "_iso.png", img)
                    if os.path.exists(stl[:-4] + "_2d.png"):          # the drawing of the base that was really built (a fresh variant has its own)
                        shutil.copy2(stl[:-4] + "_2d.png", os.path.join(dest, new + "_2d.png"))
                        img2 = os.path.join(KIT, "out", "choices", "built_" + new + "_2d.png"); os.makedirs(os.path.dirname(img2), exist_ok=True); see_through(stl[:-4] + "_2d.png", img2, 360)
                except Exception as e: ev("log", text=f"could not copy {name}: {type(e).__name__}: {e}"); dstl = stl
                okc += 1; ev("built", seed=seed, n=n, ok=True, variant="fresh variant" in rest, stl=dstl, img=img, img2=img2, secs=float(sm.group(1)) if sm else 0, theme=theme)
            else:
                bad += 1; ev("built", seed=seed, n=n, ok=False, why=(rest.split(":", 1)[1].strip() if ":" in rest else rest)[:200] if tok == "FLAG" else "the runner said OK but there is no STL file")
    p.wait()
    for n in nums:
        if n not in seen: bad += 1; ev("built", seed=seed, n=n, ok=False, why=f"the build ended (code {p.returncode}) before this base was done")
ev("done", job="build", ok=okc, failed=bad, folder=dest)
