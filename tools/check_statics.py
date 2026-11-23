"""check_statics.py: shared mutable state in the code the window's worker threads run (2026-11-23: four such statics, in
grain.c, mfjoin.c, draw2d.c and runner.c, made bases differ between runs and crashed 4-base builds). Lists every `static`
variable that is neither const nor _Thread_local in those files; a name not on the ALLOWED list below (each one checked
and explained) fails the check. tools/make_release.py runs it before it makes a release zip. Exit 1 on a new one."""
import re, sys, os
KIT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
FILES = ["runner.c", "grain.c", "mfjoin.c", "audit.c", "shot.c", "plate3mf.c", "draw2d.c", "pyx.c", "pyexact.c", "pyrandom.c"]
ALLOWED = {          # name: why it is safe with the runner's worker threads
    "on": "runner.c RUNNER_DEBUG flag, read once", "KIT": "the kit folder, set before the workers start", "CFG": "settings.json, read before the workers start",
    "CFG_DEF": "settings defaults, never written", "done": "runner.c: once-only init flag, before the workers start",
    "SUP_CLI": "a command-line switch, set before the workers", "JOIN_CLI": "same", "SMALL_CLI": "same", "STRICT_CLI": "same", "SMOOTH_CLI": "same",
    "STRENGTH_CLI": "same", "ROCKS_CLI": "same", "MEND_CLI": "same", "runner_child_class": "set at the job's start, before the workers",
    "o": "runner.c out root path: the same text from every thread", "OUTF": "the output sink, set before the workers (writes under OUTL)",
    "OUTC": "same", "OUTL": "the output lock itself", "OUTL_ready": "set before the workers", "SPAWN": "the spawn lock itself",
    "thc": "runner.c choices theme argument: the choices job's own thread", "CB_R": "set once per run before the workers", "CB_N": "same",
    "LIVE_DIR": "set before the workers", "calls": "grain.c test-only counter (BBB_GRAIN_FAIL)", "z_crc_tab": "plate3mf.c: the plate is written after the builds, one thread",
    "u_sin": "draw2d.c: the same function pointer from any thread", "u_hypot": "same",
}
bad = 0
for fn in FILES:
    p = os.path.join(KIT, "cport", fn)
    if not os.path.exists(p): continue
    s = open(p, encoding="utf-8", errors="replace").read()
    for m in re.finditer(r"^[ \t]*static\s+(?!const\b)(?!inline\b)(?!_Thread_local\b)([^;(]*?);", s, re.M):
        decl = m.group(1)
        names = []
        for part in decl.split(","):          # each declarator: its last identifier, after dropping [..] and "= .."
            part = re.sub(r"\[[^\]]*\]", "", part.split("=")[0]); w = re.findall(r"\w+", part)
            if w: names.append(w[-1])
        for name in names:
            if name in ALLOWED: continue
            line = s.count("\n", 0, m.start()) + 1
            print(f"NEW SHARED STATIC: cport/{fn}:{line}  {name}   ({m.group(0).strip()[:80]})"); bad += 1
print(f"check_statics: {bad} new shared statics" if bad else "check_statics: OK, nothing shared that is not on the checked list")
sys.exit(1 if bad else 0)
