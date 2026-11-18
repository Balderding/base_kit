"""compare designer models: python tools/designer_bench.py --models "qwen=http://127.0.0.1:8081/v1,deepseek=http://127.0.0.1:8080/v1"
        [--styles mine,swamp,temple] [--n 20] [--size 100] [--build 4] [--out folder] [--api openai|ollama] [--timeout 60] [--batch 8]
Each entry of --models is name=url (the server's first model is used) or name=url|model-id. Only servers that answer
are tested, so models can be benched one at a time: start one server, run this, stop it, start the next. Results are
added to <out>/bench.json, and report.md is rewritten from everything in it.

For each model: asks for --n plans per style, checks them, builds --build of them per style with the mesh builder
and the print check, and draws a picture sheet (the real test is whether the scenes look like places)."""
import sys, os
KIT = os.path.dirname(os.path.dirname(os.path.abspath(__file__))); sys.path[0] = KIT      # first: tools/grp.py would otherwise be loaded in place of a standard module
import json, time, subprocess
from concurrent.futures import ThreadPoolExecutor
a = sys.argv[1:]
def opt(name, d):
    if name in a: i = a.index(name); v = a[i + 1]; del a[i:i + 2]; return v
    return d
size = opt("--size", "100"); n = int(opt("--n", "20")); nbuild = int(opt("--build", "4")); styles = opt("--styles", "mine,swamp,temple").split(",")
out = os.path.abspath(opt("--out", os.path.join(KIT, "out", "designer_bench"))); api = opt("--api", "openai"); models = opt("--models", "")
timeout = opt("--timeout", ""); batch = opt("--batch", "")
if not models: raise SystemExit(__doc__)
sys.argv = sys.argv[:1]; os.environ.update(BASE_MM=size, KIT_SCENES="1", KIT_DESIGNER="on", KIT_DESIGNER_API=api, SALT="bench")
import gg3, gg, scenes, designer, run_set
scenes.install(vars(gg3)); run_set.SIZE = float(size); os.makedirs(out, exist_ok=True); mm = float(size)
designer.CFG.setdefault("designer", {})["cache"] = False
if timeout: designer.CFG["designer"]["timeout_secs"] = float(timeout)          # a slow model needs longer than the 60 s default
if batch: designer.CFG["designer"]["batch"] = int(batch)
bj = os.path.join(out, "bench.json")
try: ALL = json.load(open(bj, encoding="utf-8"))
except (OSError, ValueError): ALL = {}

def build(job):
    name, style, plan = job
    env = dict(os.environ, PLAN_JSON=json.dumps(plan), SETDIR=out, BASENAME=name, KIT_WORKER="1", RESEED="")
    try: p = subprocess.run([sys.executable, os.path.join(KIT, "mesher.py"), "build", style, "16"], cwd=KIT, env=env, capture_output=True, text=True, timeout=900)
    except subprocess.TimeoutExpired: return name, False, False, "timeout"
    lines = [l for l in p.stdout.splitlines() if l.startswith("{")]
    if not lines: return name, False, False, "crash"
    r = json.loads(lines[-1])
    if r.get("error"): return name, False, False, r["error"]
    try: run_set.drop_specks(r["path"]); run_set.close_seams(r["path"]); probs = run_set.print_check(r["path"])
    except Exception as e: probs = [f"checker {type(e).__name__}"]
    placed = r.get("plan_line", "").startswith("plan: designer") and r.get("theme", "") == plan["theme"]
    return name, placed, not probs, ", ".join(probs)

for ent in models.split(","):
    name, _, rest = ent.partition("="); url, _, mid = rest.partition("|")
    os.environ["KIT_DESIGNER_URL"] = url; os.environ.pop("KIT_DESIGNER_MODEL", None)
    if mid: os.environ["KIT_DESIGNER_MODEL"] = mid
    if not designer.available(): print(f"{name}: no server at {url}, skipped", flush=True); continue
    print(f"{name}: {designer.model_name()} at {url}", flush=True)
    R = dict(model=designer.model_name(), url=url, size=mm, asked=0, parsed=0, clean=0, fixed=0, refused=0, props=0, props_ok=0, secs=0.0, themes=[], builds=0, placed=0, printed=0, errors=[], when=time.strftime("%Y-%m-%d %H:%M"))
    todo = []; shown = []
    for st in styles:
        legal = set(scenes.legal_props(st)); used = []; good = []; k = 0; left = n; batch = int(designer.cfg()["batch"])
        while left > 0:
            k += 1; want = min(batch, left); left -= want
            raw = designer.ask(st, mm, 3, "", want, f"bench{st}{k}", used); R["asked"] += want; R["secs"] += designer.LAST["secs"]
            if not raw: R["errors"].append(f"{st}: {designer.LAST['error']}"); continue
            R["parsed"] += len(raw)
            for r in raw:
                pr = [x for x in (r.get("props") or []) if isinstance(x, dict)]; R["props"] += len(pr); R["props_ok"] += sum(1 for x in pr if x.get("what") in legal)
                p, notes = scenes.validate_plan(r, st, mm, used)
                if p is None: R["refused"] += 1; continue
                R["clean" if not notes else "fixed"] += 1; used.append(p["theme"]); R["themes"].append(p["theme"]); p["_source"] = "designer"; good.append(p)
        print(f"  {st}: {len(good)} usable plans of {n} asked, {R['secs']:.0f} s so far", flush=True)
        for i, p in enumerate(good[:nbuild]): todo.append((f"{name}_{st}_{i + 1}", st, p))
        shown += [(st, p) for p in good[:6]]
    with ThreadPoolExecutor(8) as ex:
        for nm_, placed, ok, why in ex.map(build, todo):
            R["builds"] += 1; R["placed"] += placed; R["printed"] += ok
            if why: R["errors"].append(f"{nm_}: {why}")
    # picture sheet: the first 6 usable plans of each style, drawn as 2D layouts
    import matplotlib; matplotlib.use("Agg"); import matplotlib.pyplot as plt
    if shown:
        fig, axs = plt.subplots(len(styles), 6, figsize=(30, 5.2 * len(styles)), squeeze=False)
        for ax in axs.flat: ax.axis("off")
        for r_, st in enumerate(styles):
            for c_, (s2, p) in enumerate([x for x in shown if x[0] == st][:6]):
                os.environ["PLAN_JSON"] = json.dumps(p); os.environ["RESEED"] = f"b{c_}"
                try:
                    s = gg3.make(st, 16)
                    try: gg.build_cmds(s)
                    except Exception: pass
                    gg3.render(s, axs[r_, c_]); axs[r_, c_].set_title(f"{st}: {getattr(s, 'theme', '')}" + ("" if getattr(s, "plan_source", "").startswith("designer") else "  (fell back)"), fontsize=12)
                except Exception as e: axs[r_, c_].set_title(f"{st}: {type(e).__name__}", fontsize=10)
        os.environ.pop("PLAN_JSON", None)
        plt.tight_layout(); sp = os.path.join(out, f"sheet_{name}.png"); fig.savefig(sp, dpi=60); plt.close(fig); R["sheet"] = os.path.basename(sp)
    ALL[name] = R
    with open(bj, "w", encoding="utf-8") as f: json.dump(ALL, f, indent=1)

pc = lambda x, y: f"{100 * x / y:.0f} %" if y else "-"
rows = [("Model", lambda r: r["model"]), ("Plans asked for", lambda r: str(r["asked"])), ("Valid JSON (plans read / asked)", lambda r: pc(r["parsed"], r["asked"])),
        ("Accepted as they came", lambda r: pc(r["clean"], r["asked"])), ("Accepted after fixes (total)", lambda r: pc(r["clean"] + r["fixed"], r["asked"])),
        ("Distinct theme names", lambda r: f"{len(set(t.lower() for t in r['themes']))} of {len(r['themes'])}"), ("Objects from the style's legal list", lambda r: pc(r["props_ok"], r["props"])),
        ("Seconds per plan", lambda r: f"{r['secs'] / max(1, r['parsed']):.1f}"), ("Built with the subject placed", lambda r: f"{r['placed']} of {r['builds']}"),
        ("Passed the print check", lambda r: f"{r['printed']} of {r['builds']}"), ("Picture sheet", lambda r: r.get("sheet", "")), ("Run", lambda r: r["when"])]
names = list(ALL)
md = ["# Designer model bench", "", f"Styles: {', '.join(styles)}. Base {size} mm. Made by tools/designer_bench.py.", "", "| Measure | " + " | ".join(names) + " |", "|---|" + "---|" * len(names)]
md += [f"| {lab} | " + " | ".join(fn(ALL[k]) for k in names) + " |" for lab, fn in rows]
for k in names:
    if ALL[k]["errors"]: md += ["", f"Problems with {k}:"] + [f"- {e}" for e in ALL[k]["errors"][:12]]
with open(os.path.join(out, "report.md"), "w", encoding="utf-8") as f: f.write("\n".join(md) + "\n")
print("\n".join(md)); print("saved", os.path.join(out, "report.md"))
