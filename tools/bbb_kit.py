"""make bases designed by the local model: python tools/bbb_kit.py [--styles mine,swamp,temple] [--size 100 | 60,100 | all] [--basestobuild 4] [--build all of them] [--mini biped] [--url ...]
Starts the Qwen server if it is not running, then for each style: shows what the kit sends, prints Qwen's answer as
it is written, checks every plan out loud, builds some of them with the mesh builder and puts them through the print
check. SolidCraft is not used. Everything is explained on screen as it happens."""
import sys, os
KIT = os.path.dirname(os.path.dirname(os.path.abspath(__file__))); sys.path[0] = KIT      # first: tools/grp.py would otherwise be loaded in place of a standard module
import json, time, subprocess, urllib.request
a = sys.argv[1:]; ARGS = list(a)
def opt(name, d):
    if name in a: i = a.index(name); v = a[i + 1]; del a[i:i + 2]; return v
    return d
styles = opt("--styles", "mine,swamp,temple").split(",")
if styles == ["all"]: styles = "badlands highland maahl corridor infestation urban mine swamp cynder targa trederra fortress beast frontier graveyard forest dungeon temple coast".split()
size = opt("--size", "100"); ALL_SIZES = "30,40,50,60,80,100,130,160"
sizes = [x for x in (ALL_SIZES if size == "all" else size).split(",") if x]; nplans = int(opt("--basestobuild", opt("--plans", "4"))); nbuild = int(opt("--build", str(nplans))); JOBS = int(opt("--jobs", str(max(1, min(12, (os.cpu_count() or 4) // 3)))))
URL = opt("--url", "http://127.0.0.1:8081/v1"); MINI = opt("--mini", ""); SUP = opt("--supports", "")
if SUP in ("on", "off"): os.environ["KIT_SUPPORTS"] = "1" if SUP == "on" else "0"
DETAIL = max(1, min(4, int(opt("--detail", "4")))); OUT = opt("--out", "") or os.path.join(KIT, "out", "bases_" + time.strftime("%Y%m%d_%H%M%S"))          # ONE folder for the whole run, every size and style in it
sys.argv = sys.argv[:1]
if MINI: os.environ["KIT_MINI"] = MINI
os.system("")                                               # switches colour on in a Windows console
C = dict(h="\033[1;97m", q="\033[96m", ok="\033[92m", bad="\033[91m", dim="\033[90m", y="\033[93m", o="\033[0m")
def say(msg="", c=""): print(f"{C.get(c, '')}{msg}{C['o'] if c else ''}", flush=True)
def step(n, title): say(); say("=" * 100, "h"); say(f" STEP {n}: {title}", "h"); say("=" * 100, "h")
def kit(msg): say(f"[kit]  {msg}")
T0 = time.time()

say("Balderding's Ballin Base Kit. Save time and money.", "h")
step(1, "the setup")
try: VERSION = open(os.path.join(KIT, "VERSION"), encoding="utf-8").read().strip()
except Exception: VERSION = "not known (the VERSION file is missing)"
kit(f"kit version     : {VERSION}  (the newest is the one marked Latest on https://github.com/Balderding/base_kit/releases)")
kit(f"kit folder      : {KIT}")
kit(f"builder         : mesh (the STL is made in Python; SolidCraft is not started)")
kit(f"base size       : {', '.join(sizes)} mm, detail level {DETAIL} of 4 (--detail 1 to 4)")
kit(f"styles to do    : {', '.join(styles)}  ({nplans} plans asked for per style, {nbuild} of them built)")
kit(f"designer server : {URL}")
MODELS_OPT = opt("--models", "on")
try:          # the public-domain model packs: said out loud what is fetched and from where, then fetched once (tools\get_models.py)
    _dl = True
    for _nm in ("settings_python.json", "settings.json"):          # models_download lives in settings_python.json since 2026-11-22
        if os.path.exists(os.path.join(KIT, _nm)):
            with open(os.path.join(KIT, _nm), encoding="utf-8") as _fh: _dl = json.load(_fh).get("models_download", _dl)
except Exception: _dl = True
if MODELS_OPT != "off" and _dl:
    try:
        import importlib.util          # by its path: the tools folder must not be on the search path (tools/grp.py would hide a standard module)
        _sp = importlib.util.spec_from_file_location("get_models", os.path.join(KIT, "tools", "get_models.py")); get_models = importlib.util.module_from_spec(_sp); _sp.loader.exec_module(get_models)
        if get_models.missing():
            step("1b", "the model packs (first run only)"); get_models.ensure(kit)
        else: kit(f"model packs     : all here ({os.path.join(KIT, 'models')})")
    except Exception as e: kit(f"model packs     : could not be checked ({type(e).__name__}: {e}); bases are made without the model details that are missing")
else: kit("model packs     : not fetched (--models off); bases are made without the model details that are missing")
kit("supports        : " + ("ALLOWED: objects may overhang (true round barrels, logs, arching ribs). The print needs supports, and the STL holds the objects as separate shells sunk into the base" if os.environ.get("KIT_SUPPORTS") == "1" else "none needed: nothing overhangs (--supports on allows it)"))
kit(f"builds at once  : {JOBS} (--jobs N changes it)")
try:
    g = subprocess.run(["nvidia-smi", "--query-gpu=name,memory.used,memory.total", "--format=csv,noheader"], capture_output=True, text=True, timeout=10).stdout.strip()
    kit(f"graphics card   : {g}")
except Exception: pass

step(2, "the Qwen model server")
def up():
    try:
        with urllib.request.urlopen(URL + "/models", timeout=3) as r: return [m.get("id", "") for m in json.loads(r.read()).get("data", [])]
    except Exception: return []
started = None; ms = up()
if ms: kit(f"a server is already answering at {URL} with model '{ms[0]}'. Using it; it will be left running.")
else:
    R = r"G:\LLM"; M = R + r"\models\Qwen3.8-27B-TurboFCFusion-NEO-CODER-MAX-MTP"; exe = R + r"\llama.cpp\b12131\llama-server.exe"
    log = os.path.join(KIT, "work", "qwen-server.log"); os.makedirs(os.path.dirname(log), exist_ok=True)
    kit("no server is answering, so this script starts llama-server itself (no window; it is stopped again at the end).")
    kit(f"program : {exe}")
    kit(f"model   : Qwen 3.8 27B, 4-bit (Q4_K_M), 18.5 GB file, all layers on the graphics card, 16K context")
    kit(f"its log : {log}")
    env = dict(os.environ, TEMP=R + r"\tmp", TMP=R + r"\tmp", LLAMA_CACHE=R + r"\llama.cpp\cache")
    started = subprocess.Popen([exe, "-m", M + r"\Qwen3.8-27B-TurboFCFusion-735-882-Here-Uncen-NEO-CODER-MAX-MTP-Q4_K_M.gguf", "--alias", "qwen3.8-27b-neo-coder", "--host", "127.0.0.1",
                                "--port", URL.split(":")[2].split("/")[0], "-ngl", "all", "-c", "16384", "-np", "1", "-fa", "on", "-ctk", "q8_0", "-ctv", "q8_0", "--jinja",
                                "--chat-template-file", R + r"\models\templates\chat-template-strata.jinja", "--spec-type", "draft-mtp", "--spec-draft-n-max", "2"],
                               stdout=open(log, "w"), stderr=subprocess.STDOUT, env=env, creationflags=getattr(subprocess, "CREATE_NO_WINDOW", 0))
    import atexit; atexit.register(lambda: started.poll() is None and started.kill())          # whatever happens to this script, the server it started does not stay behind
    t = time.time()
    while not up():
        if started.poll() is not None: say(f"[kit]  the server stopped while loading. See {log}", "bad"); sys.exit(1)
        if time.time() - t > 300: say("[kit]  the server did not come up in 5 minutes.", "bad"); started.kill(); sys.exit(1)
        kit(f"loading the model into graphics memory ... {time.time() - t:3.0f} s"); time.sleep(4)
    kit(f"server ready after {time.time() - t:.0f} s. Model: {up()[0]}")

if len(sizes) > 1:          # several sizes: one run of this script per size, all using the server that is up now
    rest = [x for i, x in enumerate(ARGS) if x not in ("--size", "--out") and (i == 0 or ARGS[i - 1] not in ("--size", "--out"))]; codes = []
    for z in sizes:
        step(3, f"size {z} mm ({sizes.index(z) + 1} of {len(sizes)})")
        codes.append(subprocess.run([sys.executable, os.path.abspath(__file__)] + rest + ["--size", z, "--out", OUT]).returncode)
    say(); kit("sizes done: " + ", ".join(f"{z} mm {'ok' if c == 0 else 'FAILED'}" for z, c in zip(sizes, codes)) + f". Whole run took {time.time() - T0:.0f} s. Files are in: {OUT}")
    if started is not None:
        kit("stopping the Qwen server this script started (graphics memory is freed) ..."); started.kill()
        try: started.wait(15)
        except Exception: pass
        kit("server stopped.")
    sys.exit(max(codes))
size = sizes[0]
EXIT = 0
step(3, "loading the kit")
os.environ.update(BASE_MM=size, KIT_SCENES="1", KIT_DESIGNER="on", KIT_DESIGNER_URL=URL, SALT="run" + time.strftime("%H%M%S"))
kit("importing the generator (gg3), the scene composer (scenes) and the designer client (designer) ...")
import gg3, gg, scenes, designer, run_set
scenes.install(vars(gg3)); run_set.SIZE = float(size); mm = float(size)
out = OUT; os.makedirs(out, exist_ok=True)
kit(f"loaded. The kit knows {len(scenes.SUBJECTS) + 1} subjects, {len(scenes.PATHS)} kinds of path and {len(scenes.PROP_NAMES)} objects.")
kit(f"STLs, pictures and plans of this run are saved to: {out}  (pictures at the top of it, STLs and plans in a folder per style)")
kit(f"miniature these bases are for: {scenes.mini_type()}: {scenes.MINIS[scenes.mini_type()]['says']}.")
kit("height limit: none. The plan's subject.height says how tall the subject stands (each part stays no taller than 3 x its thickness, so it prints and does not snap).")

def ask_stream(style, used):
    """one call to the model, printing its answer as it arrives"""
    msgs = designer.prompt(style, mm, DETAIL - 1, "", used, nplans); sch = designer.schema(style, mm, nplans)
    body = {"model": designer.model_name(), "messages": msgs, "temperature": 0.9, "max_tokens": 4000, "stream": True, "chat_template_kwargs": {"enable_thinking": False},
            "response_format": {"type": "json_schema", "json_schema": {"name": "scene_plans", "strict": True, "schema": sch}}}
    req = urllib.request.Request(URL + "/chat/completions", data=json.dumps(body).encode(), headers={"Content-Type": "application/json"})
    text = ""; t = time.time(); n = 0
    sys.stdout.write(C["q"])
    with urllib.request.urlopen(req, timeout=180) as r:
        for line in r:
            line = line.decode("utf-8", "replace").strip()
            if not line.startswith("data:") or line.endswith("[DONE]"): continue
            try: d = json.loads(line[5:])["choices"][0].get("delta", {}).get("content") or ""
            except (ValueError, KeyError, IndexError): continue
            text += d; n += 1; sys.stdout.write(d.replace("},{\"theme\"", "},\n{\"theme\"")); sys.stdout.flush()
    sys.stdout.write(C["o"] + "\n"); return msgs, text, time.time() - t, n

def build(name, style, plan, variant=""):
    env = dict(os.environ, PLAN_JSON=json.dumps(plan), SETDIR=out, BASENAME=name, KIT_WORKER="1", RESEED=variant)
    p = subprocess.run([sys.executable, os.path.join(KIT, "mesher.py"), "build", style, str(DETAIL * 5 - 4)], cwd=KIT, env=env, capture_output=True, text=True, timeout=900)
    lines = [l for l in p.stdout.splitlines() if l.startswith("{")]
    return json.loads(lines[-1]) if lines else {"error": (p.stderr.strip().splitlines() or ["no output"])[-1][:200]}

def make(name, style, plan):
    """build one base and put it through the print check; a base that fails is built again as a fresh variant (up to 3 tries)"""
    t = time.time(); first = ""; r = {}; probs = []
    for k, v in enumerate(("", "r1", "r2")):
        r = build(name, style, plan, v)
        if r.get("error"): probs = ["build failed: " + r["error"]]
        else:
            try: run_set.drop_specks(r["path"]); run_set.close_seams(r["path"]); probs = run_set.print_check(r["path"])
            except Exception as e: probs = [f"checker error {type(e).__name__}"]
        if not probs: break
        first = first or ", ".join(probs)
    if not r.get("error"):
        try:
            import mesher; png = os.path.join(out, os.path.basename(r["path"])[:-4] + ".png"); iso = r["path"][:-4] + "_iso.png"
            if os.path.exists(iso): os.replace(iso, png)                                  # the builder left its picture beside the STL: it belongs at the top
            else: mesher.shot(r["path"], png)          # pictures all together at the top of the run folder
            with open(r["path"][:-4] + "_plan.json", "w", encoding="utf-8") as fh: json.dump(r.get("plan"), fh, indent=1)
        except Exception: pass
    return r, probs, time.time() - t, (first if not probs else ""), k + 1

from concurrent.futures import ThreadPoolExecutor
pool = ThreadPoolExecutor(JOBS); queue = []          # builds start as soon as a style's plans are checked, while Qwen writes the next style's

tot = dict(asked=0, got=0, clean=0, fixed=0, refused=0, built=0, passed=0, qsecs=0.0); rows = []
for si, style in enumerate(styles):
    step(4 + si, f"style '{style}': {scenes.STYLE_SAYS.get(style, '')}")
    subs, paths, props = scenes.legal_subjects(style), scenes.legal_paths(style), scenes.legal_props(style)
    kit(f"the model may only use names the kit can build. For {style} that is:")
    kit(f"  {len(subs)} subjects: {', '.join(subs)}")
    kit(f"  {len(paths)} paths   : {', '.join(paths)}")
    kit(f"  {len(props)} objects : {', '.join(props)}")
    kit("these lists go into the prompt, and into a JSON schema that the server enforces while Qwen writes,")
    kit("so the answer cannot contain a name the kit does not have or a number outside its range.")
    kit(f"built-in themes of this style (Qwen is told not to repeat names it already used): {', '.join(scenes.theme_names(style))}")
    msgs = designer.prompt(style, mm, DETAIL - 1, "", [], nplans)
    if si == 0:
        say(); kit("THE SYSTEM PROMPT (the same for every style):"); say(msgs[0]["content"], "dim")
    say(); kit("THE REQUEST for this style:"); say(msgs[1]["content"], "y")
    say(); kit(f"sending it to Qwen now and asking for {nplans} plans. Thinking is switched off (plans do not need it).")
    kit("QWEN'S ANSWER, printed as it is written:"); say()
    try: msgs, text, secs, ntok = ask_stream(style, [])
    except Exception as e: say(f"[kit]  the call failed: {type(e).__name__}: {e}. The kit would now use its built-in themes.", "bad"); continue
    say(); kit(f"answer complete: {len(text)} characters in {secs:.1f} s (about {ntok / max(secs, 0.1):.0f} pieces of text per second).")
    plans = designer.parse(text); tot["asked"] += nplans; tot["got"] += len(plans); tot["qsecs"] += secs
    kit(f"reading the answer as JSON: {len(plans)} plan{'s' if len(plans) != 1 else ''} found" + ("." if plans else ". Not usable: the kit would fall back to its built-in themes."))
    say(); kit("CHECKING EACH PLAN (scenes.validate_plan): unknown names are dropped, numbers put back in range, rules applied.")
    good = []; used = []
    for i, r in enumerate(plans, 1):
        p, notes = scenes.validate_plan(r, style, mm, used)
        sub = r.get("subject") or {}; pth = r.get("path") or {}
        say(f"  plan {i}: \"{r.get('theme')}\"", "h")
        say(f"      subject : {sub.get('name')}  ({scenes.SUBJECT_SAYS.get(sub.get('name'), '?')}), at {sub.get('angle')} degrees, "
            + ("on the rim" if sub.get("edge") else f"{sub.get('offset')} of the radius from the centre") + f", size x{sub.get('size')}, height x{sub.get('height')}")
        say(f"      path    : {pth.get('kind')} ({scenes.PATH_SAYS.get(pth.get('kind'), '?')}), " + ("ends at the subject" if pth.get("route") == "to" else "passes in front of the subject") + f", comes in at {pth.get('from_angle')} degrees")
        say("      objects : " + ", ".join(f"{x.get('count')} x {x.get('what')} ({x.get('where')})" for x in r.get("props") or []))
        cl = r.get("clutter") or {}; say(f"      clutter : rubble {cl.get('debris')}, plants {cl.get('growth')}, ground marks {cl.get('marks')};  open ground {r.get('open')}")
        if p is None: tot["refused"] += 1; say(f"      result  : REFUSED ({'; '.join(notes)}). A built-in theme would be used instead.", "bad"); continue
        if notes: tot["fixed"] += 1; say(f"      result  : accepted after {len(notes)} fix{'es' if len(notes) != 1 else ''}: {'; '.join(notes)}", "y")
        else: tot["clean"] += 1; say("      result  : accepted exactly as Qwen wrote it", "ok")
        p["_source"] = "designer"; good.append(p); used.append(p["theme"])
    for i, p in enumerate(good[:nbuild], 1):
        name = f"{style}_{size}mm_qwen{i}"; queue.append((name, p, pool.submit(make, name, style, p)))
    kit(f"{min(nbuild, len(good))} builds of this style started in the background ({JOBS} at once).")

step(4 + len(styles), f"building: {len(queue)} bases, {JOBS} at once")
kit("For each base the kit lays out the style's ground, places the subject, lays the path, keeps the miniature's spots")
kit("flat, puts each object where the plan says, adds clutter where it would gather, clears the edge, writes the STL,")
kit("then runs the print check (one closed solid, right size, magnet pockets open). A base that fails is built again.")
tb = time.time()
for name, p, fut in queue:
    try: r, probs, secs, first, tries = fut.result()
    except Exception as e: r, probs, secs, first, tries = {"error": f"{type(e).__name__}: {e}"}, [f"{type(e).__name__}"], 0, "", 1
    tot["built"] += 1; say(f"  {name}: \"{p['theme']}\"", "h")
    if r.get("error"): say(f"      build failed: {r['error']}", "bad"); rows.append((name, p["theme"], "build failed", secs)); continue
    sd = (r.get("plan") or {}).get("stand")
    kit(f"      {r.get('plan_line')}")
    if sd: kit(f"      the {sd['type']} miniature stands at x {sd['at'][0]}, y {sd['at'][1]} mm from the centre, facing {sd['facing']} degrees")
    kit(f"      tallest point {r.get('top_mm', '?')} mm above the deck; {r.get('tris'):,} triangles; {secs:.0f} s" + (f"; built {tries} times (first try: {first})" if tries > 1 else ""))
    for l in r.get("scene_log") or []: say(f"      note from the composer: {l}", "dim")
    placed = (r.get("plan") or {}).get("subject") is not None and r.get("theme") == p["theme"]
    if not probs: tot["passed"] += 1; say("      print check: PASSED" + ("" if placed else "  (but the subject found no room; a built-in theme was used)"), "ok")
    else: say(f"      print check: FAILED after {tries} tries ({', '.join(probs)})", "bad")
    rows.append((name, p["theme"], "passed" if not probs else "failed: " + ", ".join(probs), secs))
kit(f"all builds done in {time.time() - tb:.0f} s.")

step(4 + len(styles), "summary")
kit(f"plans asked for          : {tot['asked']}")
kit(f"plans Qwen returned      : {tot['got']}   ({tot['qsecs']:.0f} s of model time, {tot['qsecs'] / max(1, tot['got']):.1f} s per plan)")
kit(f"accepted as written      : {tot['clean']}")
kit(f"accepted after fixes     : {tot['fixed']}")
kit(f"refused                  : {tot['refused']}")
kit(f"bases built              : {tot['built']}")
kit(f"passed the print check   : {tot['passed']}")
for name, th, res, secs in rows: say(f"    {name:28} {th:32} {res}  ({secs:.0f} s with checks)", "ok" if res == "passed" else "bad")
if any(res != "passed" for _, _, res, _ in rows): EXIT = 1
kit(f"whole run took {time.time() - T0:.0f} s. Files are in: {out}")
if started is not None:
    kit("stopping the Qwen server this script started (graphics memory is freed) ..."); started.kill()
    try: started.wait(15)
    except Exception: pass
    kit("server stopped.")
sys.exit(EXIT)
