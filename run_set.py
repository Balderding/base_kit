"""Build a set of bases with hard limits so a batch can't run wild.

usage:  python run_set.py <style|all> <how many> [--detail 1|2|3|4|mix] [--workers N] [-q]
        python run_set.py urban 20              20 urban bases, an even mix of the four detail levels
        python run_set.py mine 50 --detail 4    50 mine bases, all at the most detailed level
        python run_set.py all 100               100 of every style
        python run_set.py corridor 1-20
        python run_set.py mine 3,7,12-15 --workers 3
styles: badlands highland maahl corridor infestation urban mine

Rules (all in settings.json):
  - each SolidCraft step gets step_limit_secs; a hung or crashed step is skipped (sc_client.py)
  - each base gets base_limit_secs, then it is killed
  - a base that fails (error, timeout, failed print check, or more than max_dropped_pieces dropped) is rebuilt once
    as a fresh variant; if that fails too it is flagged "investigate" and the batch moves on
  - the batch stops early if stop_if_fail_fraction of bases are flagged, or after batch_limit_mins
Output: out/<style>/*.stl + *_iso.png (3D picture) + *_2d.png (the drawing from above), out/<style>_report.csv, out/<style>_sheet.png
"""
import random, sys, os, json, time, csv, subprocess, argparse, signal, threading
from concurrent.futures import ThreadPoolExecutor, as_completed
from kit_settings import CFG, HERE
import live          # the window's live view: progress notes (nothing unless KIT_LIVE is set)

ALL_STYLES = ["badlands", "highland", "maahl", "corridor", "infestation", "urban", "mine",
              "swamp", "cynder", "targa", "trederra", "fortress", "beast", "frontier", "graveyard", "forest", "dungeon", "temple", "coast"]
VARIANTS = ["", "r1", "r2", "r3", "r4", "r5", "r6", "r7"]


def parse_nums(txt):
    out = []
    for part in txt.split(","):
        if "-" in part: a, b = part.split("-"); out += range(int(a), int(b) + 1)
        elif part: out.append(int(part))
    return out


def kill_tree(p):
    try:
        if os.name == "nt": subprocess.run(["taskkill", "/F", "/T", "/PID", str(p.pid)], capture_output=True)
        else: os.killpg(p.pid, signal.SIGKILL)
    except Exception: pass


QUIET = False
SIZE = 30.0           # base diameter, mm
KNOWN_SIZES = [25, 28, 28.5, 30, 32, 40, 50, 60, 65, 70, 80, 90, 100, 130, 160]      # common round bases
OVAL_SIZES = [(60, 35.5), (75, 42), (90, 52), (105, 70), (120, 92), (150, 95), (170, 109)]      # common oval bases, long x short
OVAL = None           # (long, short) mm for an oval base (--size 90x52), None for a round one


def parse_size(txt):
    """--size: "60" or "60mm" is a round base of that diameter; "90x52" or "90x52mm" an oval, long x short (either
    order). Returns (long or diameter, short or None). Raises ValueError for anything else."""
    t = str(txt).strip().lower().replace("mm", "").replace(" ", "").replace("*", "x")
    if "x" in t:
        a, b = (float(v) for v in t.split("x", 1))
        return max(a, b), min(a, b)
    return float(t), None


def size_tag():
    """the size as it goes into file names: 60 or 90x52"""
    return f"{OVAL[0]:g}x{OVAL[1]:g}" if OVAL else f"{SIZE:g}"


def base_limit():
    """time allowed per base: bigger bases carry more detail"""
    return CFG["base_limit_secs"] * max(1.0, (SIZE / 30.0) ** 2)

TIER = {}
LABEL = {}            # (style, internal number) -> file name, e.g. urban_007


def nm(style, n): return LABEL.get((style, n), f"{style}_{size_tag()}mm_{n:02d}")


def plan(count, detail):
    """internal numbers for `count` bases. detail 1-4 = all at that level; None = an even mix of the four levels.
    (Internally a number's place in 1-20 sets its detail tier; every number is still its own new base.)"""
    out = []
    for i in range(count):
        tier = (detail - 1) if detail else i % 4
        m = i if detail else i // 4
        out.append((20 * (m // 5) + tier * 5 + m % 5 + 1, tier + 1))
    return out

SCALE = 1.0           # the scenery scale (--scale) and the size the scene is laid out at for it
LAYOUT = 0.0
SEEN = set()
RECENT = {}
SIGFILE = ""
SALT = ""
_lock = threading.Lock()


def pump(p, tag, errs):
    """read the build's stderr as it runs: show @@ progress lines, keep the rest for error reporting"""
    for line in p.stderr:
        line = line.rstrip()
        if line.startswith("@@ "):
            if not QUIET:
                with _lock: print(f"    {tag}  {line[3:]}", flush=True)
        elif line: errs.append(line)


ENGINE = str(CFG.get("engine", "mesh"))
_free = []; _servers = []

class _Server:
    """a mesh builder that stays running: Python and the libraries load once per worker instead of once per base"""
    def __init__(self, env, kw):
        import queue
        self.p = subprocess.Popen([sys.executable, os.path.join(HERE, "mesher.py"), "serve"], cwd=HERE, env=env, stdin=subprocess.PIPE,
                                  stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True, bufsize=1, **kw)
        self.q = queue.Queue(); self.errs = []
        threading.Thread(target=self._out, daemon=True).start(); threading.Thread(target=self._err, daemon=True).start()
        _servers.append(self)
    def _out(self):
        for line in self.p.stdout:
            if line.startswith("{"): self.q.put(line)
        self.q.put(None)
    def _err(self):
        for line in self.p.stderr:
            if line.strip(): self.errs.append(line.rstrip()); del self.errs[:-20]
    def ask(self, req, limit):
        import queue
        try:
            self.p.stdin.write(json.dumps(req) + "\n"); self.p.stdin.flush()
            return self.q.get(timeout=limit)
        except queue.Empty: return "timeout"
        except (OSError, ValueError): return None
    def stop(self):
        try: kill_tree(self.p)
        except Exception: pass

def _stop_servers():
    for s in list(_servers): s.stop()
import atexit; atexit.register(_stop_servers)

PLANS = {}            # base name -> scene plan from the designer model (asked for up front, one model call at a time)

def build_once(style, n, variant):
    env = dict(os.environ, RESEED=variant, SALT=SALT, BASENAME=nm(style, n), BASE_MM=f"{LAYOUT or SIZE:g}", SETDIR=CFG["out_folder"], KIT_QUIET="1" if QUIET else "0", KIT_WORKER="1")
    plan = PLANS.get(nm(style, n)) if not variant else None       # a retry variant is a new base: it takes a built-in theme
    if plan is not None and not (ENGINE == "mesh" and CFG.get("mesh_keep_running", True)): env["PLAN_JSON"] = json.dumps(plan)
    kw = {"creationflags": subprocess.CREATE_NEW_PROCESS_GROUP} if os.name == "nt" else {"start_new_session": True}
    t = time.time()
    if ENGINE == "mesh" and CFG.get("mesh_keep_running", True):
        sv = None
        with _lock:
            while _free and sv is None:
                c = _free.pop()
                if c.p.poll() is None: sv = c
        if sv is None: sv = _Server(env, kw)
        ans = sv.ask({"style": style, "n": n, "variant": variant, "name": nm(style, n), "plan": plan}, base_limit())
        if ans == "timeout":
            sv.stop()
            return {"error": f"over {base_limit():.0f} s limit", "secs": round(time.time() - t)}
        if ans is None:                                     # the builder died: say why; the next base starts a new one
            last = (sv.errs or ["no output"])[-1]; sv.stop()
            return {"error": "crash: " + last[:120], "secs": round(time.time() - t)}
        with _lock: _free.append(sv)                        # ready for the next base
        j = json.loads(ans); j.setdefault("secs", round(time.time() - t, 1))
        return j
    p = subprocess.Popen([sys.executable, os.path.join(HERE, "mesher.py" if ENGINE == "mesh" else "gg3.py"), "build", style, str(n)], cwd=HERE, env=env,
                         stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True, **kw)
    tag = nm(style, n) + (f"/{variant}" if variant else "")
    errs = []
    th = threading.Thread(target=pump, args=(p, tag, errs), daemon=True); th.start()
    try:
        out, _ = p.communicate(timeout=base_limit())
    except subprocess.TimeoutExpired:
        kill_tree(p); p.communicate(); th.join(2)
        return {"error": f"over {base_limit():.0f} s limit", "secs": round(time.time() - t)}
    th.join(5)
    lines = [l for l in out.strip().splitlines() if l.startswith("{")]
    if not lines:
        last = (errs or ["no output"])[-1]
        return {"error": "crash: " + last[:120], "secs": round(time.time() - t)}
    j = json.loads(lines[-1]); j.setdefault("secs", round(time.time() - t))
    return j


def _drop_slivers(m):
    """Remove flat sliver triangles (three points on one line) that sit on an open or over-shared edge: SolidCraft
    sometimes writes them along an edge, which leaves the STL one hairline slit short of watertight."""
    import numpy as np
    from trimesh.grouping import group_rows
    for _ in range(10):
        es = m.edges_sorted
        bad = {tuple(map(int, es[i])) for k in (1, 3) for i in np.asarray(group_rows(es, require_count=k)).ravel()}
        if not bad: return
        tri = m.triangles; el = np.linalg.norm(tri - np.roll(tri, 1, axis=1), axis=2).max(axis=1)
        h = 2 * m.area_faces / np.maximum(el, 1e-12)
        fe = es.reshape(-1, 3, 2)
        hit = np.array([any(tuple(map(int, e)) in bad for e in f) for f in fe])
        kill = hit & (h < 1e-4)
        if not kill.any(): return
        m.update_faces(~kill)


def _edge_counts(m):
    import numpy as np
    u, c = np.unique(m.edges_sorted, axis=0, return_counts=True)
    return int((c % 2 == 1).sum()), int(((c % 2 == 0) & (c > 2)).sum())      # (open/odd edges, pinched edges)


def _fill_planar_holes(m, tol=2e-3, max_n=400):
    """SolidCraft's STL writer sometimes leaves a flat face out. Find closed boundary loops that lie in one plane and
    triangulate them (ear clipping), wound to match their neighbours. Nothing moves, the missing face is put back."""
    import numpy as np
    from trimesh.grouping import group_rows
    bnd = m.edges[group_rows(m.edges_sorted, require_count=1)]
    out = {}
    for a, b in bnd: out.setdefault(int(a), []).append(int(b))
    P = m.vertices; new = []
    for start in list(out):
        while out.get(start):
            loop = [start]; cur = out[start].pop()
            while cur != start and len(loop) <= max_n and out.get(cur):
                loop.append(cur); cur = out[cur].pop()
            if cur != start or len(loop) < 3: continue
            ring = loop[::-1]; pts = P[ring]
            nrm = np.zeros(3)
            for i in range(len(pts)): p, q = pts[i], pts[(i + 1) % len(pts)]; nrm += np.cross(p, q)
            L = np.linalg.norm(nrm)
            if L < 1e-9: continue
            nrm /= L
            if np.abs((pts - pts.mean(0)) @ nrm).max() > tol: continue          # not flat: leave it
            ax = np.cross(nrm, [1, 0, 0] if abs(nrm[0]) < 0.9 else [0, 1, 0]); ax /= np.linalg.norm(ax); ay = np.cross(nrm, ax)
            xy = np.c_[pts @ ax, pts @ ay]; idx = list(range(len(ring)))
            def cr(o, a, b): return (a[0] - o[0]) * (b[1] - o[1]) - (a[1] - o[1]) * (b[0] - o[0])
            guard = 0
            while len(idx) > 3 and guard < 5000:
                guard += 1; done = False
                for k in range(len(idx)):
                    i0, i1, i2 = idx[k - 1], idx[k], idx[(k + 1) % len(idx)]
                    if cr(xy[i0], xy[i1], xy[i2]) <= 1e-12: continue
                    if any(j not in (i0, i1, i2) and cr(xy[i0], xy[i1], xy[j]) > 0 and cr(xy[i1], xy[i2], xy[j]) > 0 and cr(xy[i2], xy[i0], xy[j]) > 0 for j in idx): continue
                    new.append([ring[i0], ring[i1], ring[i2]]); idx.pop(k); done = True; break
                if not done:                                    # degenerate (collinear run): drop a flat corner
                    k = min(range(len(idx)), key=lambda k: abs(cr(xy[idx[k - 1]], xy[idx[k]], xy[idx[(k + 1) % len(idx)]])))
                    new.append([ring[idx[k - 1]], ring[idx[k]], ring[idx[(k + 1) % len(idx)]]]); idx.pop(k)
            if len(idx) == 3: new.append([ring[idx[0]], ring[idx[1]], ring[idx[2]]])
    if new: m.faces = np.vstack([m.faces, np.array(new)])
    return len(new)


def _fill_small_loops(m, max_n=8, max_len=4.0):
    """Close tiny closed boundary loops (a hairline triangle or quad left where a curved face meets flat ones) with a
    fan of faces wound to match their neighbours."""
    import numpy as np
    from trimesh.grouping import group_rows
    bnd = m.edges[group_rows(m.edges_sorted, require_count=1)]
    nxt = {int(a): int(b) for a, b in bnd}
    seen = set(); new = []
    for u in list(nxt):
        if u in seen: continue
        loop = [u]
        while nxt.get(loop[-1]) is not None and nxt[loop[-1]] != u and len(loop) <= max_n:
            loop.append(nxt[loop[-1]])
        seen.update(loop)
        if nxt.get(loop[-1]) != u or len(loop) < 3 or len(loop) > max_n: continue
        per = sum(np.linalg.norm(m.vertices[loop[i]] - m.vertices[loop[(i + 1) % len(loop)]]) for i in range(len(loop)))
        if per > max_len: continue
        r = loop[::-1]                                  # faces run opposite to the open edges' direction
        new += [[r[0], r[i], r[i + 1]] for i in range(1, len(r) - 1)]
    if new: m.faces = np.vstack([m.faces, np.array(new)])
    return len(new)


def _fix_tjunctions(m, tol=2e-4):
    """Where one side of an edge was written as one long piece and the other as several short ones, the mesh is open
    along it. Split the face with the long edge at every open vertex that lies on it, so both sides match. No vertex
    moves."""
    import numpy as np
    for _ in range(12):
        F = np.asarray(m.faces); P = np.asarray(m.vertices)
        e = np.vstack([F[:, [0, 1]], F[:, [1, 2]], F[:, [2, 0]]]); fidx = np.tile(np.arange(len(F)), 3)
        es = np.sort(e, axis=1); u, inv, c = np.unique(es, axis=0, return_inverse=True, return_counts=True)
        op = np.where(c[inv.ravel()] % 2 == 1)[0]
        if not len(op): return True
        V = np.unique(e[op]); VP = P[V]
        new = {}; changed = False
        for k in op:
            f = int(fidx[k])
            if f in new: continue
            a, b = int(e[k][0]), int(e[k][1]); d = P[b] - P[a]; L2 = float(d @ d)
            if L2 < 1e-12: continue
            t = ((VP - P[a]) @ d) / L2
            dist = np.linalg.norm(VP - (P[a] + np.outer(t, d)), axis=1)
            on = [(float(t[i]), int(V[i])) for i in np.where((t > 1e-6) & (t < 1 - 1e-6) & (dist < tol))[0] if int(V[i]) not in (a, b)]
            if not on: continue
            x = [int(v) for v in F[f] if int(v) not in (a, b)]
            if len(x) != 1: continue
            chain = [a] + [v for _, v in sorted(on)] + [b]
            new[f] = [[chain[i], chain[i + 1], x[0]] for i in range(len(chain) - 1)]; changed = True
        if not changed: return False
        keep = np.ones(len(F), bool); keep[list(new)] = False
        m.faces = np.vstack([F[keep]] + [np.array(v) for v in new.values()])
    return _edge_counts(m)[0] == 0


def close_seams(path):
    """Repair what SolidCraft's STL writer leaves open, without moving anything: zero-area slivers, T-junctions,
    hairline loops and flat faces left out. Saves the file if it ends up closed. Returns True if it repaired it."""
    import trimesh
    m = trimesh.load(path); m.merge_vertices()
    if _edge_counts(m)[0] == 0: return False
    _drop_slivers(m); m.remove_unreferenced_vertices()
    if _edge_counts(m)[0]: _fix_tjunctions(m); _drop_slivers(m)
    if _edge_counts(m)[0]: _fix_tjunctions(m)
    if _edge_counts(m)[0]: _fill_planar_holes(m)
    if _edge_counts(m)[0]: _fill_small_loops(m)
    m = trimesh.Trimesh(vertices=m.vertices, faces=m.faces, process=False)
    if _edge_counts(m)[0]: return False
    m.export(path); return True


def drop_specks(path, max_mm3=1.5):
    """A cut can slice a crumb off a piece and leave it as a separate speck. Remove loose bits smaller than max_mm3
    (they would print as dust). Returns how many were removed."""
    import trimesh
    if CFG.get("allow_supports", False): return 0          # the separate shells are the objects' true solids, not dust
    m = trimesh.load(path)
    parts = m.split(only_watertight=False)
    if len(parts) < 2: return 0
    vols = [abs(q.volume) for q in parts]; big = max(vols)
    keep = [q for q, v in zip(parts, vols) if v == big or v >= max_mm3]
    if len(keep) == len(parts): return 0
    trimesh.util.concatenate(keep).export(path)
    return len(parts) - len(keep)


def print_check(path):
    import trimesh
    m = trimesh.load(path)
    parts = [q for q in m.split(only_watertight=False) if abs(q.volume) > 0.05]
    b = m.bounds; w = float(b[1][0] - b[0][0]); top = float(b[1][2]); dy = float(b[1][1] - b[0][1])
    probs = []
    m.merge_vertices()
    if _edge_counts(m)[0]: probs.append("not watertight")      # an open edge. Edges where two parts just touch (4 faces) are closed and slice fine
    if len(parts) != 1 and not CFG.get("allow_supports", False): probs.append(f"{len(parts)} loose pieces")      # with supports allowed, objects are closed shells sunk into the base
    if OVAL:                                                    # an oval: its length along x and its width along y
        if abs(w - OVAL[0]) > 0.05 or abs(dy - OVAL[1]) > 0.05: probs.append(f"{w:.2f} x {dy:.2f} mm, not {OVAL[0]:g} x {OVAL[1]:g}")
    elif abs(w - SIZE) > 0.05: probs.append(f"{w:.2f} mm wide, not {SIZE:g}")
    import gg as _gg
    if top > 4.0 + min(_gg.max_height(SIZE) * SCALE, 1000.0) + 0.05 and top > 4.0 + (25.0 if SCALE != 1.0 else 0) + 0.05: probs.append(f"too tall ({top - 4:.1f} mm above deck)")
    import gg
    cens = gg.oval_magnet_centres(*OVAL) if OVAL else gg.magnet_centres(SIZE)
    if any(m.contains([[x, y, 1.0] for x, y in cens])): probs.append("magnet hole blocked")
    if len(cens) > 1 and (0.0, 0.0) not in cens and not m.contains([[0, 0, 1.0]])[0]: probs.append("hole in the middle of the bottom")      # bases over 100 mm (and ovals over 105 mm long) have a pocket in the middle on purpose
    return probs


def one_base(style, n, stop):
    tries = []
    for k in range(max(1, min(len(VARIANTS), int(CFG.get("max_attempts", 4))))):
        if stop["now"]: break
        v = VARIANTS[k]
        with _lock: print(f"  >> {nm(style, n)}  {'building' if k == 0 else 'retry with fresh variant ' + v}", flush=True)
        r = build_once(style, n, v)
        stl = os.path.join(CFG["out_folder"], style, f"{nm(style, n)}.stl")
        reason = r.get("error")
        if r.get("sig") and r.get("plan_hash"): r["sig"] += "-" + r["plan_hash"]      # the scene is part of what makes a base itself
        if not reason and r.get("sig") and SIGFILE:           # never the same base twice, in this run or any earlier one
            with _lock:
                if r["sig"] in SEEN: reason = "duplicate of an earlier base"
                else:
                    SEEN.add(r["sig"])
                    try:
                        with open(SIGFILE, "a") as fh: fh.write(r["sig"] + "\n")
                    except OSError: pass
        seam = False
        if not reason:
            live.set_stage(n, "checks")          # the window's live view (nothing unless it watches)
            if not QUIET:
                with _lock: print(f"    {nm(style, n)}  checking STL (seams, watertight, size, magnet)", flush=True)
            try:
                if os.path.exists(stl): r["specks"] = drop_specks(stl)
                seam = os.path.exists(stl) and close_seams(stl)
                probs = print_check(stl) if os.path.exists(stl) else ["no STL written"]
                if not probs and float(CFG.get("min_difference_pct", 15)) > 0:      # must differ enough from the last 10 of its style
                    import numpy as np, compare_bases as cb
                    cb.set_size(SIZE); Hn = cb.heightmap(stl); xs = np.linspace(-cb.R, cb.R, cb.N); X, Y = np.meshgrid(xs, xs, indexing="ij")
                    disc = X * X + Y * Y <= (cb.R - 0.3) ** 2; Rn = cb.rotations(Hn, 72)
                    with _lock: prev = list(RECENT.get(style, []))[-int(CFG.get("compare_last", 10)):]
                    sc = [(cb.score(Rn, Hp, disc)[0], nm) for nm, Hp in prev]
                    close = min(sc) if sc else (100.0, "")
                    r["diff"] = round(close[0])
                    if close[0] < float(CFG.get("min_difference_pct", 15)): probs = [f"too similar to {close[1]} ({close[0]:.0f}% different)"]
                    else:
                        with _lock: RECENT.setdefault(style, []).append((nm(style, n), Hn))
            except Exception as e:      # a checker problem must never kill the batch
                seam = False; probs = [f"checker error: {type(e).__name__}: {str(e)[:80]}"]
            if not probs and r.get("want_mm", 0) >= 1.5 and r.get("top_mm", 99) < 0.5 * r["want_mm"]:          # the mesh is far lower than the design: the objects were lost on the way (this happened: one bad cut planed a base flat)
                probs = [f"the top came out flat ({r.get('top_mm')} mm high, designed {r['want_mm']} mm): objects lost"]
            dropped = r.get("skipped", [])
            if probs: reason = ("" if probs[0].startswith("too similar") else "print check: ") + ", ".join(probs)
            elif len(dropped) > CFG["max_dropped_pieces"]: reason = f"{len(dropped)} pieces dropped"
        if reason and os.path.exists(stl):        # keep the failed attempt for a look (or a slicer repair) instead of overwriting it
            fd = os.path.join(CFG["out_folder"], style, "failed"); os.makedirs(fd, exist_ok=True)
            try: os.replace(stl, os.path.join(fd, f"{nm(style, n)}_{v or 'original'}.stl"))
            except OSError: pass
            try: os.replace(stl[:-4] + "_2d.png", os.path.join(fd, f"{nm(style, n)}_{v or 'original'}_2d.png"))      # its drawing goes with it
            except OSError: pass
        if not reason and r.get("plan"):                      # the scene plan beside the STL: edit it and rebuild with --plan
            try:
                with open(stl[:-4] + "_plan.json", "w", encoding="utf-8") as fh: json.dump(r["plan"], fh, indent=1)
            except OSError: pass
        tries.append({"variant": v or "original", "secs": r.get("secs"), "reason": reason, "slowest": r.get("slowest", ""), "steps": r.get("steps", ""),
                      "theme": r.get("theme", ""), "plan_line": r.get("plan_line", ""), "plan": json.dumps(r["plan"]) if r.get("plan") else "",
                      "dropped": ", ".join(sorted(set(r.get("skipped", [])))), "restarts": r.get("restarts", 0),
                      "note": "rim seam closed" if seam else "", "diff": r.get("diff", ""),
                      "made": f"mesh: {r['mesher']}; pictures: {r.get('pictures', '-')}" if r.get("mesher") else ""})
        if not reason: break
    ok = bool(tries) and not tries[-1]["reason"]
    live.finish(n)          # its live note goes: the window shows the finished card
    return {"base": nm(style, n), "ok": ok, "tries": tries, "detail": TIER.get((style, n), (n - 1) % 20 // 5 + 1)}


def sheet(style, results):
    import matplotlib; matplotlib.use("Agg"); import matplotlib.pyplot as plt, matplotlib.image as mpimg
    rows = (len(results) + 4) // 5
    fig, axs = plt.subplots(rows, 5, figsize=(20, 4.2 * rows), squeeze=False)
    for ax in axs.flat: ax.axis("off")
    for ax, r in zip(axs.flat, results):
        png = os.path.join(CFG["out_folder"], style, r["base"] + "_iso.png")
        if r["ok"] and os.path.exists(png): ax.imshow(mpimg.imread(png))
        last = r["tries"][-1] if r["tries"] else {}
        note = "OK" if r["ok"] else "FLAGGED"
        if last.get("variant", "original") != "original": note += " (variant)"
        ax.set_title(f"{r['base']}  detail {r.get('detail', '')}  {note}", fontsize=10, color="#222" if r["ok"] else "#c00")
    plt.tight_layout(); fig.savefig(os.path.join(CFG["out_folder"], f"{style}_sheet.png"), dpi=70); plt.close(fig)


def engine_text():
    """what really builds the bases this run: the layout is planned in Python; the height field + mesh run in C
    (bbb_kit.exe, threaded when the CPU threads switch is on); pictures on the video card when that switch is on"""
    import cmesh
    exe = os.path.exists(os.path.join(HERE, "bbb_kit.exe"))
    n = cmesh.threads()
    if exe and n > 1: core = f"height field + mesh in C ({n} threads)"
    elif exe and cmesh.enabled(): core = "height field in Python, mesh in C (1 thread)"
    else: core = "height field + mesh in Python"
    pics = "pictures on the video card" if exe and cmesh.gpu_mode() else "pictures in Python"
    return f"layout in Python, {core}, {pics}"


def preflight():
    """check everything cheap before starting, so problems show up in 2 seconds not 2 minutes"""
    if ENGINE == "mesh":
        print(f"kit    : {HERE}\nengine : mesh, {engine_text()}\nout    : {CFG['out_folder']}", flush=True)
    else:
        from sc_client import cli_path
        cli = cli_path()
        print(f"kit    : {HERE}\nengine : solidcraft\ncli    : {cli}\nout    : {CFG['out_folder']}\nmode   : {'live (app on port 27878)' if CFG.get('live') else 'headless'}", flush=True)
        try:
            v = subprocess.run([cli, "--version"], capture_output=True, text=True, timeout=15)
            print(f"version: {(v.stdout or v.stderr).strip() or 'ok'}", flush=True)
        except Exception as e:
            raise SystemExit(f"cannot run {cli}: {e}")
    miss = []
    for m in ("shapely", "numpy", "trimesh", "matplotlib", "networkx"):
        try: __import__(m)
        except ImportError: miss.append(m)
    if ENGINE == "mesh":
        import shapely
        if not hasattr(shapely, "constrained_delaunay_triangles"):
            raise SystemExit(f"--engine mesh needs shapely 2.1 or newer (you have {shapely.__version__}).\nUpdate: python -m pip install --user --upgrade shapely")
    if miss: raise SystemExit("missing python packages: " + " ".join(miss) + "\nInstall: python -m pip install --user " + " ".join(miss))


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("style", help="one of: " + " ".join(ALL_STYLES) + ", or all")
    ap.add_argument("count", help="how many bases to build")
    ap.add_argument("--size", default="30", help="base diameter in mm (default 30). Common round sizes: " + ", ".join(f"{x:g}" for x in KNOWN_SIZES) + ". Any size from 20 to 200 works. "
                    "An oval base is long x short, e.g. --size 90x52; common ovals: " + ", ".join(f"{a:g}x{b:g}" for a, b in OVAL_SIZES))
    ap.add_argument("--detail", default="mix", help="1 (simplest) to 4 (most detailed), or mix (default: an even mix of all four)")
    ap.add_argument("--workers", type=int, default=CFG["workers"])
    ap.add_argument("--seed", default="", help="reuse an earlier run's seed to rebuild the same bases (shown at the start of every run)")
    ap.add_argument("--engine", default=globals()["ENGINE"], choices=["solidcraft", "mesh"], help="solidcraft: build in SolidCraft. mesh: build the STL in the kit itself: layout in Python, height field + mesh in C (seconds per base, SolidCraft not needed)")
    ap.add_argument("-q", "--quiet", action="store_true", help="only OK/FLAG lines, no per-step progress")
    ap.add_argument("--scenes", choices=["on", "off"], default=None, help="every base is one place with a theme (default: the scenes setting in settings.json)")
    ap.add_argument("--theme", default="", help="build every base with this theme of the style, e.g. --theme \"tunnel mouth\" (a wrong name lists the themes)")
    ap.add_argument("--plan", default="", help="build from a scene plan file (a <name>_plan.json from an earlier run, edited or not)")
    ap.add_argument("--objects", type=float, default=None, help="how many of the things that stand on the base are kept, 0 to 1: 1 = all (the default, settings.json objects_amount), 0.33 = a third. The ground's own shape is never cut")
    ap.add_argument("--supports", choices=["on", "off"], default=None, help="on: objects may overhang, so the print needs supports (a barrel narrower at the foot than the belly, ribs arching off the ground). Default off: nothing overhangs")
    ap.add_argument("--mini", default="", help="the kind of miniature the bases are for: biped (default), wide, walker, tripod, quad, many, scorpion, crawler, tracked, hover, none, or mix. Its contact spots are kept flat and clear")
    ap.add_argument("--scale", default="", help="the scenery scale: 1 = as the kit lays things out (a small landscape, objects about a third of true size beside a 32 mm figure); 3 = true size for 32 mm figures, 2.6 for 28 mm, 1.4 for 15 mm. A small base then shows a fragment of the scene. Default: scenery_scale in settings.json")
    ap.add_argument("--designer", choices=["on", "off"], default=None, help="let the local model write the scene plans (default: the designer setting in settings.json)")
    a = ap.parse_args()
    if a.scenes: os.environ["KIT_SCENES"] = "1" if a.scenes == "on" else "0"; CFG["scenes"] = a.scenes == "on"
    if a.designer: os.environ["KIT_DESIGNER"] = a.designer
    if a.objects is not None: os.environ["KIT_OBJECTS"] = str(max(0.0, min(1.0, a.objects))); CFG["objects_amount"] = max(0.0, min(1.0, a.objects))
    if a.supports: os.environ["KIT_SUPPORTS"] = "1" if a.supports == "on" else "0"; CFG["allow_supports"] = a.supports == "on"
    if a.mini:
        os.environ["KIT_MINI"] = a.mini
        import scenes as _sc
        try: _sc.CFG = CFG; _sc.mini_type()
        except NameError: pass
    if (a.theme or a.plan or a.designer == "on") and not CFG.get("scenes", False):
        raise SystemExit("--theme, --plan and --designer need scenes: add --scenes on (or set scenes to true in settings.json)")
    if a.theme:
        os.environ["THEME"] = a.theme
        if a.style == "all": raise SystemExit("--theme belongs to one style: give the style, not all")
    if a.plan:
        if not os.path.exists(a.plan): raise SystemExit(f"--plan: no such file {a.plan}")
        os.environ["PLAN_JSON"] = os.path.abspath(a.plan)
    global QUIET, SIGFILE; QUIET = a.quiet; globals()["ENGINE"] = a.engine
    global SIZE, OVAL
    try: SIZE, short = parse_size(a.size)
    except ValueError: raise SystemExit("--size must be a number of millimetres, e.g. --size 40, or long x short for an oval, e.g. --size 90x52")
    if not 20 <= SIZE <= 200: raise SystemExit("--size must be between 20 and 200 mm")
    if short is not None and abs(short - SIZE) > 1e-9:          # an oval (a "60x60" is just a round 60)
        if short < 20: raise SystemExit("--size: an oval's short size must be 20 mm or more")
        if a.engine != "mesh": raise SystemExit("oval bases are built with the mesh engine only (--engine mesh)")
        OVAL = (SIZE, short); os.environ["KIT_OVAL"] = size_tag()
        if OVAL not in OVAL_SIZES: print(f"note: {size_tag()} mm is not one of the common oval sizes ({', '.join(f'{x:g}x{y:g}' for x, y in OVAL_SIZES)}); building it anyway", flush=True)
    else:
        os.environ.pop("KIT_OVAL", None)
        if SIZE not in KNOWN_SIZES: print(f"note: {SIZE:g} mm is not one of the common base sizes ({', '.join(f'{x:g}' for x in KNOWN_SIZES)}); building it anyway", flush=True)
    if a.detail != "mix" and a.detail not in ("1", "2", "3", "4"): raise SystemExit("--detail must be 1, 2, 3, 4 or mix")
    global SCALE, LAYOUT
    try: SCALE = float(a.scale or CFG.get("scenery_scale", 1.0))
    except ValueError: raise SystemExit("--scale must be a number, e.g. --scale 3")
    if not 1.0 <= SCALE <= 4.0: raise SystemExit("--scale must be between 1 and 4")
    LAYOUT = SIZE if SCALE == 1.0 else round(max(30.0, SIZE / SCALE), 1)          # the size the scene is laid out at; below 30 mm the kit cannot lay one out, so a small base shows the middle of a 30 mm layout
    if SCALE != 1.0:
        os.environ.update({"KIT_FINAL_MM": f"{SIZE:g}", "KIT_SCALE": f"{SCALE:g}", "BASE_MM": f"{LAYOUT:g}"})
        print(f"scale  : x{SCALE:g}: laid out as a {LAYOUT:g} mm base, built {SCALE:g} times larger on the {size_tag()} mm base" + ("" if LAYOUT * SCALE <= SIZE + 0.5 else f" (it shows the middle {SIZE / SCALE:.0f} mm of that layout)"), flush=True)
    if OVAL: print(f"oval   : {OVAL[0]:g} x {OVAL[1]:g} mm: the scene is laid out round on the long size and cut to the oval", flush=True)
    detail = None if a.detail == "mix" else int(a.detail)
    if a.style != "all" and a.style not in ALL_STYLES:
        import difflib
        near = difflib.get_close_matches(a.style, ALL_STYLES, 1)
        raise SystemExit(f"unknown style '{a.style}'." + (f" Did you mean {near[0]}?" if near else "") + "\nStyles: " + " ".join(ALL_STYLES) + ", or all")
    if a.count.isdigit() and int(a.count) < 1: raise SystemExit("how many bases: give a number of 1 or more")
    if a.workers < 1: raise SystemExit("--workers must be 1 or more")
    if a.count.isdigit():
        pl = plan(int(a.count), detail); nums = [n for n, _ in pl]
        for st in (ALL_STYLES if a.style == "all" else [a.style]):
            for i, (n, tier) in enumerate(pl, 1): LABEL[(st, n)] = f"{st}_{size_tag()}mm_{i:03d}"; TIER[(st, n)] = tier
    else: nums = parse_nums(a.count)             # old form: explicit internal numbers such as 3,7,12-15
    preflight()
    try:          # the public-domain model packs: only say so here (the window and bbb_kit.cmd fetch them; this plain runner never downloads by itself)
        import importlib.util as _iu
        _sp = _iu.spec_from_file_location("get_models", os.path.join(HERE, "tools", "get_models.py")); _gm = _iu.module_from_spec(_sp); _sp.loader.exec_module(_gm); _mi = _gm.missing()
        if _mi and not os.environ.get("KIT_WORKER"): print(f"note: {len(_mi)} model pack(s) are not on this computer, so the details made from them are left out. To fetch them (public domain, about {sum(p.get('mb', 4) for p in _mi):.0f} MB): python tools\\get_models.py", flush=True)
    except Exception: pass
    SIGFILE = "" if a.seed else os.path.join(CFG["out_folder"], "built_signatures.txt")      # one line per base ever built here
    if SIGFILE and os.path.exists(SIGFILE):
        with open(SIGFILE) as fh: SEEN.update(l.strip() for l in fh if l.strip())
    # every run is new: fresh random seed and its own output folder, so nothing is repeated or overwritten
    global SALT
    SALT = a.seed or "".join(random.choice("abcdefghjkmnpqrstuvwxyz23456789") for _ in range(5))
    if CFG.get("always_new", True):
        CFG["out_folder"] = os.path.join(CFG["out_folder"], time.strftime("run_%Y%m%d_%H%M%S") + "_" + SALT)
        os.makedirs(CFG["out_folder"], exist_ok=True)
    else:
        SALT = ""
    print(f"seed   : {SALT or '(fixed designs)'}   folder: {CFG['out_folder']}", flush=True)
    if CFG.get("live"):          # fail fast: live mode needs the app running with its control port open
        import socket
        try: socket.create_connection(("127.0.0.1", 27878), timeout=3).close()
        except OSError:
            print("SolidCraft is not listening on 127.0.0.1:27878.\n"
                  "Start the app with its control port, e.g.:  \"C:\\...\\solidcraft.exe\" --control 27878\n"
                  "or set \"live\": false in settings.json to build without the app.")
            sys.exit(1)
    workers = 1 if CFG.get("live") else max(1, a.workers)        # live mode: one build at a time in the open app
    styles = ALL_STYLES if a.style == "all" else [a.style]
    if a.theme and a.style in ALL_STYLES:          # a wrong theme name: say so now, with the list, not once per base
        import themes
        names = [t["theme"] for t in themes.THEMES.get(a.style, [])]
        if a.theme.strip().lower() not in [x.lower() for x in names]: raise SystemExit(f"unknown theme '{a.theme}' for {a.style}. Themes: " + ", ".join(names))
    for st in styles:
        if CFG.get("scenes", False) and not a.plan and not a.theme: design(st, nums, a.seed)
        run_style(st, nums, workers)


_designer = {"ready": None}

def design(style, nums, seed_given):
    """ask the designer model for this style's scene plans before any base is built (workers never call the model)"""
    import designer
    if not designer.enabled(): return
    c = designer.cfg()
    if _designer["ready"] is None:
        _designer["ready"] = designer.available()
        if _designer["ready"]: print(f"designer: {designer.model_name()} at {c['url']}", flush=True)
        else: print(f"designer: no server at {c['url']}, using the built-in themes", flush=True)
    pfile = os.path.join(CFG["out_folder"], "plans.json"); saved = {}
    if seed_given:                                 # the same seed must give the same bases: reuse that run's plans
        import glob
        for f in sorted(glob.glob(os.path.join(os.path.dirname(CFG["out_folder"]), f"run_*_{SALT}", "plans.json"))):
            if os.path.abspath(f) != os.path.abspath(pfile):
                try:
                    with open(f, encoding="utf-8") as fh: saved = json.load(fh)
                    print(f"designer: plans from the earlier run with this seed ({f})", flush=True); break
                except (OSError, ValueError): pass
    names = [nm(style, n) for n in nums]; got = {k: v for k, v in saved.items() if k in names}
    if not got and _designer["ready"] and not saved:
        os.environ.setdefault("BASE_MM", f"{SIZE:g}")
        import gg3, scenes
        scenes.install(vars(gg3))
        tier = TIER.get((style, nums[0]), 4) - 1
        plans, st = designer.plans_for(style, SIZE, tier, len(names), SALT, cache_file=os.path.join(os.path.dirname(CFG["out_folder"]), "plan_cache.json"))
        got = dict(zip(names, plans))
        print(f"designer: {style}: {len(plans)} of {len(names)} plans in {st['secs']:.0f} s ({st['calls']} calls, {st['clean']} as they came, {st['fixed']} after fixes, {st['refused']} refused)"
              + (f"; {st['error'] or 'too few usable plans'}: the rest from the built-in themes" if len(plans) < len(names) else ""), flush=True)
    PLANS.update(got)
    allp = {}
    if os.path.exists(pfile):
        try:
            with open(pfile, encoding="utf-8") as fh: allp = json.load(fh)
        except (OSError, ValueError): pass
    allp.update(got)
    if allp:
        with open(pfile, "w", encoding="utf-8") as fh: json.dump(allp, fh, indent=1)


def run_style(style, nums, workers):
    class a: pass
    a.style = style
    stop = {"now": False}; t0 = time.time(); results = []; flagged = 0
    print(f"{a.style}: {len(nums)} bases, {size_tag()} mm, {workers} at a time, limits {CFG['step_limit_secs']} s/step, "
          f"{base_limit():.0f} s/base, {CFG['batch_limit_mins']} min/batch", flush=True)
    with ThreadPoolExecutor(workers) as ex:
        futs = {ex.submit(one_base, a.style, n, stop): n for n in nums}
        for f in as_completed(futs):
            if f.cancelled(): continue
            r = f.result(); results.append(r)
            last = r["tries"][-1] if r["tries"] else {"secs": 0, "reason": "not started"}
            if r["ok"]:
                extra = (f"  dropped: {last['dropped']}" if last["dropped"] else "") + (f"  ({last['note']})" if last.get("note") else "")
                swap = "  (fresh variant after: " + r["tries"][0]["reason"] + ")" if len(r["tries"]) > 1 else ""
                with _lock: print(f"  OK   {r['base']}  {last['secs']} s{extra}{swap}" + (f"  {last['plan_line']}" if last.get("plan_line") else ""), flush=True)      # under the lock: a worker's ">> building" line must not land in the middle of this one
            else:
                flagged += 1
                why = " / ".join(t["reason"] or "" for t in r["tries"])
                with _lock: print(f"  FLAG {r['base']}  failed {len(r['tries'])} times: {why}", flush=True)
            if flagged >= max(2, CFG["stop_if_fail_fraction"] * len(nums)):
                stop["now"] = True; print("STOPPING: too many failures, something systematic is wrong", flush=True)
            if time.time() - t0 > CFG["batch_limit_mins"] * 60:
                stop["now"] = True; print("STOPPING: batch time limit reached", flush=True)
            if stop["now"]:
                for g in futs: g.cancel()
    results.sort(key=lambda r: r["base"])
    rep = os.path.join(CFG["out_folder"], f"{a.style}_report.csv")
    with open(rep, "w", newline="", encoding="utf-8") as f:
        w = csv.writer(f); w.writerow(["base", "result", "attempt", "variant", "build secs", "failure reason", "dropped pieces", "restarts", "note", "steps", "slowest steps", "% different from closest of last 10", "detail level", "theme", "made by", "plan from", "plan"])
        for r in results:
            for i, t in enumerate(r["tries"], 1):
                res = ("OK" if not t["reason"] else ("FLAGGED" if i == len(r["tries"]) and not r["ok"] else "retried"))
                w.writerow([r["base"], res, i, t["variant"], t["secs"], t["reason"] or "", t["dropped"], t["restarts"], t.get("note", ""), t.get("steps", ""), t.get("slowest", ""), t.get("diff", ""), r.get("detail", ""), t.get("theme", ""), t.get("made", ""), t.get("plan_line", ""), t.get("plan", "")])
    sheet(a.style, results)
    ok = sum(r["ok"] for r in results)
    print(f"DONE {a.style}: {ok}/{len(nums)} OK, {flagged} flagged, {round((time.time() - t0) / 60, 1)} min. "
          f"Seed: {SALT}. Report: {rep}", flush=True)


if __name__ == "__main__":
    main()
