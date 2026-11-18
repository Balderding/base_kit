"""prove the style-only details: python tools/sd_bench.py <style|all> [mm=100] [seeds=2] [--shot]
Lays each of a style's twenty on a bare base (every seed a new base), builds the mesh and says which builder failed,
which laid nothing and whether the mesh is closed. --shot saves a picture per base in work/sd/."""
import sys, os
KIT = os.path.dirname(os.path.dirname(os.path.abspath(__file__))); sys.path[0] = KIT
a = [x for x in sys.argv[1:] if not x.startswith("--")]; SHOT = "--shot" in sys.argv
os.environ.setdefault("BASE_MM", a[1] if len(a) > 1 else "100")
import math, random, traceback, trimesh
import gg, gg2, gg3, features, mesher, scenes, style_details as SD
features.install(vars(gg3)); scenes.install(vars(gg3)); SD.install(vars(gg3))
from sd_reg import REG
styles = list(REG) if a[0] == "all" else a[0].split(","); seeds = int(a[2]) if len(a) > 2 else 2
BARE = "--bare" in sys.argv or bool(os.environ.get("ONLY"))      # bare disc: only for hunting a tear; a base top is never plain, so the normal run lays the style's own ground first
WRAP = {st: {fn.shape: fn for fn in fl} for st, fl in SD.NEW20.items()}
def ground_of(st, seed):
    """the style's own ground, laid as gg3.make lays it, before any scene or detail"""
    n = seed % 10 + 1; mk0 = gg2.mk
    gg2.mk = lambda s_, title, k, ground: gg2.Scene(f"{s_}_{n:02d}", f"{title} #{n}", f"{s_}{k}bench{seed}", ground)
    gg.LAYOUT = True
    try: s = gg3.ORIG[st](n)
    finally: gg2.mk = mk0; gg.LAYOUT = False
    if gg3.terr(s): gg3.T_init(s)
    else: s.objs, s.keepout = [], []
    return s
out = os.path.join(KIT, "work", "sd"); os.makedirs(out, exist_ok=True); bad = 0
only = os.environ.get("ONLY", "")
for st in styles:
    lst = [x for x in REG.get(st, []) if not only or x[3] in only.split(",")]
    for seed in range(int(os.environ.get("SEED0", "0")), int(os.environ.get("SEED0", "0")) + seeds):
        if BARE: s = gg2.Scene("bench", "bench", "bench", "earth"); s.objs, s.keepout = [], []
        else: s = ground_of(st, seed)
        R = gg.BASE_MM / 2; rng = random.Random(f"{st}{seed}"); n = len(lst); spots = [(0.0, 0.0)] + [(R * 0.36 * math.cos(6.283 * i / 7), R * 0.36 * math.sin(6.283 * i / 7)) for i in range(7)] + [(R * 0.72 * math.cos(6.283 * i / 13 + 0.2), R * 0.72 * math.sin(6.283 * i / 13 + 0.2)) for i in range(13)]
        if only: spots = [spots[i] for i, x in enumerate(REG.get(st, [])) if x[3] in only.split(",")]
        elif n <= 4: spots = [(R * 0.4 * math.cos(6.283 * i / max(1, n)), R * 0.4 * math.sin(6.283 * i / max(1, n))) for i in range(n)] if n > 1 else [(0.0, 0.0)]
        if n < 20 and not only: print(f"  FAIL  {st}: only {n} details"); bad += 1
        for i, (f, cls, kind, name, r, ground) in enumerate(lst):
            x, y = spots[i % len(spots)]; rng = random.Random(f"{st}{seed}{name}")
            try:
                got = f(x, y, rng.uniform(0, 6.283), rng)
                if got is None: print(f"  NONE  {st}:{name}"); continue
                foot, lay = got
                if foot is None or foot.is_empty or not foot.is_valid: print(f"  FAIL  {st}:{name} bad footprint"); bad += 1
                elif foot.bounds[2] - foot.bounds[0] > 16 or foot.bounds[3] - foot.bounds[1] > 16: print(f"  BIG   {st}:{name} footprint {foot.bounds[2] - foot.bounds[0]:.1f} x {foot.bounds[3] - foot.bounds[1]:.1f}")
                if BARE:
                    if not features._build(s, gg.H, [L for L in lay if L is not None]): print(f"  FAIL  {st}:{name} laid nothing"); bad += 1
                elif not WRAP[st][name](s, random.Random(f"{st}{seed}{name}g"), 3): print(f"  NOROOM {st}:{name}")
            except Exception as e:
                bad += 1; tb = traceback.extract_tb(e.__traceback__)[-1]; print(f"  FAIL  {st}:{name} {type(e).__name__}: {str(e)[:90]} (line {tb.lineno} {tb.name})")
        try:
            F, R0, H, rim_tan, mag, skipped = mesher.field_from_cmds(list(gg.build_cmds(s)))
            v, f_ = mesher.mesh_from_field(F, R0, H, rim_tan, mag); m = trimesh.Trimesh(v, f_, process=False); m.merge_vertices(); m, extra = mesher.add_solids(s, m)
            import numpy as np          # the runner's own test (run_set._edge_counts): an open edge fails; an edge where two parts just touch (4 faces) is closed and slices fine
            _u, _c = np.unique(m.edges_sorted, axis=0, return_counts=True); wt = int((_c % 2 == 1).sum()) == 0; bad += not wt
            if not wt and not only and not os.environ.get("NOSEARCH"):          # name the one that tears the mesh: each alone, same seed
                import subprocess
                for (f, cls, kind, name, r, ground) in lst:
                    o = subprocess.run([sys.executable, os.path.abspath(__file__), st, os.environ["BASE_MM"], "1", "--bare"], env=dict(os.environ, ONLY=name, SEED0=str(seed)), capture_output=True, text=True).stdout
                    if "NOT closed" in o: print(f"  TEAR  {st}:{name} (seed {seed})", flush=True)
            print(f"{st} seed {seed}: {n} details, mesh {'closed' if wt else 'NOT closed (FAIL)'}, top {float(m.bounds[1][2]) - H:.1f} mm, skipped {len(skipped) if hasattr(skipped, '__len__') else skipped}", flush=True)
            if SHOT:
                png = os.path.join(out, f"{st}_{seed}.png"); mesher.shot(m, png, 2400, 1800, float(os.environ.get("ELEV", "55")), float(os.environ.get("AZ", "-70"))); print("  saved", png)
        except Exception as e:
            bad += 1; print(f"{st} seed {seed}: mesh FAIL {type(e).__name__}: {str(e)[:120]}")
print("failures:", bad)
sys.exit(1 if bad else 0)
