"""live.py: the progress note the kit window reads while a base builds (docs/superpowers/specs/2026-11-19-live-build-view-design.md).

Nothing happens unless KIT_LIVE names a folder (tools/gui_job.py sets it for the window). One note per base,
<KIT_LIVE>/<SALT>_<n>.txt, key=value lines, rewritten whole (written to .tmp, then renamed) at most every 0.1 s and
at every change of stage. Keys: stage (layout, field, mesh, picture, checks), t0, stage_t0, now, pieces, steps_done,
steps_total, threads, oval, mm, lay_r (the layout's radius, mm: what the piece outlines are drawn in), plan (the
build's 2D drawing, made as soon as the layout is done), plan_box (what that drawing shows, in the built base's
millimetres: x0,y0,x1,y1). Beside it: <note>.shapes (the pieces' outlines as they are placed) and <note>.tiles
(bbb_kit.exe --core-build: the height field's tiles and which are done).
The note is only ever read by the window: what is built is the same with it on or off."""
import os, time

_D = {}
_P = [None]          # this process's note
_T = [0.0]           # last write


def _path(n):
    d = os.environ.get("KIT_LIVE")
    return os.path.join(d, f"{os.environ.get('SALT', '')}_{int(n)}.txt") if d else None


def on():
    return _P[0] is not None


def start(n, **kw):
    """a new base: a fresh note, stage layout"""
    p = _path(n); _D.clear(); _SH.clear(); _P[0] = p
    if not p: return
    try:
        os.makedirs(os.path.dirname(p), exist_ok=True)
        for q in (p + ".shapes", p + ".tiles"):
            if os.path.exists(q): os.remove(q)          # a retry of the same base starts its picture again
    except OSError: _P[0] = None; return
    t = time.time(); _D.update(stage="layout", t0=t, stage_t0=t, pieces=0, **kw); _write()


def note(force=False, **kw):
    if not _P[0]: return
    if "stage" in kw and kw["stage"] != _D.get("stage"): kw["stage_t0"] = time.time(); force = True
    _D.update(kw)
    if force or time.time() - _T[0] >= 0.1: _write()


def _write():
    p = _P[0]; t = time.time(); _D["now"] = t; _T[0] = t
    try:
        with open(p + ".tmp", "w", encoding="utf-8") as f: f.write("".join(f"{k}={v}\n" for k, v in _D.items()))
        os.replace(p + ".tmp", p)
    except OSError: pass


_SH = []            # piece outlines not yet written to <note>.shapes


def piece(kind, polys):
    """the layout placed one more piece (called from the Scene's own methods while the window watches). Its outline
    goes to <note>.shapes, one line each: r (raised) or c (cut), then x y pairs in the layout's millimetres. A drawing
    of the whole layout costs about 0.2 s, far too much to repeat; the window draws these outlines itself instead"""
    if not _P[0]: return
    _D["pieces"] = _D.get("pieces", 0) + 1
    try:
        for p in (polys if isinstance(polys, (list, tuple)) else [polys]):
            ext = getattr(p, "exterior", None)
            if ext is None: continue
            cs = ext.coords
            if len(cs) > 48: cs = cs[::len(cs) // 48 + 1]
            _SH.append(kind + " " + " ".join(f"{x:.2f} {y:.2f}" for x, y in cs))
    except Exception: pass
    if time.time() - _T[0] >= 0.1: _flush(); _write()


def _flush():
    if not _SH or not _P[0]: return
    try:
        with open(_P[0] + ".shapes", "a", encoding="utf-8") as f: f.write("\n".join(_SH) + "\n")
    except OSError: pass
    _SH.clear()


def plan(png, box):
    """the layout is done and drawn from above (the build's own 2D drawing, made now instead of at the end): the
    window shows it, with the tiles over it. box: what the drawing shows, in the built base's millimetres"""
    _flush(); note(force=True, plan=png, plan_box=",".join(f"{v:.3f}" for v in box))


def hook_layout():
    """count pieces as the layout places them: wraps the Scene methods that add one (only while a note is kept)"""
    import gg, gg2
    for cls in {gg.Scene, getattr(gg2, "Scene", gg.Scene)}:
        for name, kind in (("prism", "r"), ("stack", "r"), ("gcut", "c"), ("tcut", "c")):
            f = cls.__dict__.get(name)
            if f is None or getattr(f, "_live", False): continue
            def wrap(f=f, kind=kind):
                def g(self, *a, **k):
                    r = f(self, *a, **k)
                    if _P[0] and a: piece(kind, a[0])
                    return r
                g._live = True; g.__doc__ = f.__doc__; return g
            setattr(cls, name, wrap())


def set_stage(n, stage):
    """another process (run_set's checks) moves a base's note on: read it, change the stage, write it back"""
    p = _path(n)
    if not p or not os.path.exists(p): return
    try:
        d = dict(l.split("=", 1) for l in open(p, encoding="utf-8").read().splitlines() if "=" in l)
        t = time.time(); d.update(stage=stage, stage_t0=t, now=t)
        with open(p + ".tmp", "w", encoding="utf-8") as f: f.write("".join(f"{k}={v}\n" for k, v in d.items()))
        os.replace(p + ".tmp", p)
    except OSError: pass


def finish(n):
    """the base is done (built or given up): its note goes; the window then shows the finished card"""
    p = _path(n)
    if not p: return
    for q in (p, p + ".tiles", p + ".shapes", p + ".tmp"):
        try: os.remove(q)
        except OSError: pass
