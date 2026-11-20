"""the choices to pick from: python tools/choices.py <style> <out folder> [--size 60 | --size 90x52] [--detail 1|2|3|4|mix] [--count 20] [--seed X] [--mini biped] [--supports on|off] [--jobs 6] [--px 360] [--json]
Draws each candidate base as a small 2D colour picture (a .png per base with a see-through background; no STL is
built, a second or two each) and prints one line per base as it is ready:
    CHOICE <number> | <picture file> | <theme> | detail <1-4>
With --json every line is one JSON record instead (what the window reads; see tools/gui_job.py):
    {"ev": "seed", "seed": "x"}    {"ev": "choice", "seed": "x", "n": 16, "img": "...png", "theme": "ore siding", "tier": 4}
    {"ev": "skip", "n": 17, "why": "..."}
The number is the one run_set.py takes, so   python run_set.py <style> 3,7,12 --seed X --size 60   builds exactly the
bases numbered 3, 7 and 12 of this sheet (same seed, same size, same miniature). For ONE base write the number with a
comma after it ("16,"): a bare 16 means "build 16 bases" to run_set.py."""
import sys, os, random, json
KIT = os.path.dirname(os.path.dirname(os.path.abspath(__file__))); sys.path[0] = KIT
a = sys.argv[1:]
def opt(name, d):
    if name in a: i = a.index(name); v = a[i + 1]; del a[i:i + 2]; return v
    return d
def flag(name):
    if name in a: a.remove(name); return True
    return False
size = opt("--size", "30"); detail = opt("--detail", "mix"); count = int(opt("--count", "20")); mini = opt("--mini", ""); sup = opt("--supports", "")
import kit_settings as _ks
_sz = size.strip().lower().replace("mm", "").replace(" ", "").replace("*", "x")          # "60" round, "90x52" an oval (long x short): laid out round on its long size, cut to the oval
if "x" in _sz: _a, _b = sorted((float(v) for v in _sz.split("x", 1)), reverse=True); oval = (_a, _b) if _a != _b else None
else: _a = float(_sz); oval = None
scale = float(opt("--scale", "") or _ks.CFG.get("scenery_scale", 1.0)); lay = _a if scale == 1.0 else round(max(30.0, _a / scale), 1)          # the scenery scale: the scene is laid out smaller and built larger (see run_set.py --scale)
jobs = int(opt("--jobs", str(max(1, (os.cpu_count() or 4) - 1)))); only = opt("--only", ""); px = int(opt("--px", "360")); JSON = flag("--json")          # jobs: one helper per choice, up to every core but one; only = opt("--only", ""); px = int(opt("--px", "360")); JSON = flag("--json")
seed = opt("--seed", "") or "".join(random.choice("abcdefghjkmnpqrstuvwxyz23456789") for _ in range(5))
style, out = a[0], os.path.abspath(a[1]); os.makedirs(out, exist_ok=True)
os.environ.update({"SALT": seed, "RESEED": "", "BASE_MM": f"{lay:g}", "KIT_QUIET": "1"})
if scale != 1.0: os.environ.update({"KIT_FINAL_MM": f"{_a:g}", "KIT_SCALE": f"{scale:g}"})          # exactly what the runner gives a build
if oval: os.environ["KIT_OVAL"] = f"{oval[0]:g}x{oval[1]:g}"          # the pictures are cropped to the oval (mesher.crop_to_scale)
else: os.environ.pop("KIT_OVAL", None)
if mini: os.environ["KIT_MINI"] = mini
if sup in ("on", "off"): os.environ["KIT_SUPPORTS"] = "1" if sup == "on" else "0"
sys.argv = sys.argv[:1]
def say(rec, plain):
    print(json.dumps(rec) if JSON else plain, flush=True)
import run_set
pl = run_set.plan(count, None if detail == "mix" else int(detail))
if not only and jobs > 1 and len(pl) > 1:          # several at once: each helper draws its share and its lines are passed on as they come
    import subprocess, threading
    say({"ev": "seed", "seed": seed}, f"SEED {seed}"); lock = threading.Lock(); ps = []
    def relay(p):
        for line in p.stdout:
            if line.startswith(("CHOICE", "SKIP", '{"ev": "choice"', '{"ev": "skip"')):
                with lock: print(line.rstrip(), flush=True)
    for k in range(min(jobs, len(pl))):
        mine = ",".join(str(n) for n, _ in pl[k::jobs])
        p = subprocess.Popen([sys.executable, os.path.abspath(__file__), style, out, "--size", size, "--detail", detail, "--count", str(count), "--seed", seed, "--only", mine, "--px", str(px), "--scale", f"{scale:g}"]
                             + (["--json"] if JSON else []) + (["--mini", mini] if mini else []) + (["--supports", sup] if sup else []),
                             stdout=subprocess.PIPE, stderr=subprocess.DEVNULL, text=True, creationflags=getattr(subprocess, "CREATE_NO_WINDOW", 0))
        t = threading.Thread(target=relay, args=(p,)); t.start(); ps.append((p, t))
    for p, t in ps: p.wait(); t.join()
    if not JSON: print("CHOICES DONE", flush=True)
    sys.exit(0)
import gg3, gg
import matplotlib; matplotlib.use("Agg"); import matplotlib.pyplot as plt
if only: keep = {int(x) for x in only.split(",") if x}; pl = [(n, t) for n, t in pl if n in keep]
else: say({"ev": "seed", "seed": seed}, f"SEED {seed}")
for n, tier in pl:
    try:
        os.environ["SALT"] = seed
        s = gg3.make(style, n)
        try: gg.build_cmds(s)
        except Exception: pass
        import mesher; png = os.path.join(out, f"c_{seed}_{n}.png"); mesher.draw2d(s, png, px, transparent=True)          # the same drawing as beside each STL (PIL: fast)
        theme = getattr(s, "theme", "") or style
        say({"ev": "choice", "seed": seed, "n": n, "img": png, "theme": theme, "tier": tier}, f"CHOICE {n} | {png} | {theme} | detail {tier}")
    except Exception as e:
        say({"ev": "skip", "n": n, "why": f"{type(e).__name__}: {str(e)[:120]}"}, f"SKIP {n} | {type(e).__name__}: {str(e)[:120]}")
if not only and not JSON: print("CHOICES DONE", flush=True)
