"""build every theme of every style once: python tools/all_themes.py [--size 100] [--styles mine,swamp] [--jobs 8] [--seed X] [--out folder]
Each theme is built with the mesh builder and put through the same print check as a normal run. Prints one line per
theme and a total; any failure is listed again at the end."""
import sys, os
KIT = os.path.dirname(os.path.dirname(os.path.abspath(__file__))); sys.path[0] = KIT      # first: tools/grp.py would otherwise be loaded in place of a standard module
import json, subprocess, time
from concurrent.futures import ThreadPoolExecutor
a = sys.argv[1:]
def opt(name, d):
    if name in a: i = a.index(name); v = a[i + 1]; del a[i:i + 2]; return v
    return d
size = opt("--size", "100"); jobs = int(opt("--jobs", "8")); seed = opt("--seed", "themes"); styles = opt("--styles", "")
out = os.path.abspath(opt("--out", os.path.join(KIT, "out", f"themes_{size}mm_{seed}")))
sys.argv = sys.argv[:1]
import themes, run_set
run_set.SIZE = float(size); os.makedirs(out, exist_ok=True)
todo = [(st, th["theme"]) for st in (styles.split(",") if styles else themes.THEMES) for th in themes.THEMES[st]]
print(f"{len(todo)} themes at {size} mm, {jobs} at a time. Saving to: {out}", flush=True)
def attempt(st, th, name, variant):
    env = dict(os.environ, KIT_SCENES="1", THEME=th, SALT=seed, BASE_MM=size, SETDIR=out, BASENAME=name, KIT_WORKER="1", RESEED=variant)
    try: p = subprocess.run([sys.executable, os.path.join(KIT, "mesher.py"), "build", st, "16"], cwd=KIT, env=env, capture_output=True, text=True, timeout=900)
    except subprocess.TimeoutExpired: return "over 900 s", 900, ""
    lines = [l for l in p.stdout.splitlines() if l.startswith("{")]
    if not lines: return "crash: " + (p.stderr.strip().splitlines() or ["no output"])[-1][:120], 0, ""
    r = json.loads(lines[-1]); stl = r.get("path", "")
    if r.get("error"): return r["error"], 0, ""
    try: run_set.drop_specks(stl); run_set.close_seams(stl); probs = run_set.print_check(stl)
    except Exception as e: probs = [f"checker error {type(e).__name__}: {e}"]
    built = (r.get("plan") or {}).get("subject") is not None
    note = "" if r.get("theme", "").lower() == th.lower() and built else f"built as '{r.get('theme')}'" + ("" if built else " without its subject")
    return ", ".join(probs), r.get("secs", 0), note

def one(job):
    """like the runner: a base that fails the print check is built again as a fresh variant (up to 3 tries)"""
    st, th = job; i = [t["theme"] for t in themes.THEMES[st]].index(th) + 1; name = f"{st}_{size}mm_t{i}"; first = ""
    for k, v in enumerate(("", "r1", "r2")):
        why, secs, note = attempt(st, th, name, v)
        if not why: return st, th, "", secs, note, (f"fresh variant after: {first}" if k else "")
        first = first or why
    return st, th, why, secs, note, ""
bad = []; n = 0; again = 0; t0 = time.time()
with ThreadPoolExecutor(jobs) as ex:
    for st, th, why, secs, note, retry in ex.map(one, todo):
        n += 1; ok = not why and not note; again += bool(retry)
        if not ok: bad.append((st, th, why or note))
        print(f"  {'OK  ' if ok else 'FAIL'} {st:12} {th:24} {secs:5.1f} s  {why or note}" + (f"  ({retry})" if retry else ""), flush=True)
print(f"DONE: {n - len(bad)}/{n} themes built and passed the print check at {size} mm ({n - len(bad) - again} first try) in {(time.time() - t0) / 60:.1f} min. Folder: {out}")
for b in bad: print("  failed:", *b)
sys.exit(1 if bad else 0)
