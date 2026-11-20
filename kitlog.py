"""One error log for the whole kit: out\\kit_errors.log (2026-11-19).

The kit carries on past a part that fails (a detail that finds no room, a step that raises): the base is still built
without it. Every such failure is written down here, with what was being made and the full traceback (each file and
line), so it can be seen and fixed instead of quietly lost:

    caught(where, e)      record a caught exception (e: the exception; default: the one being handled)
    note(where, text)     record a problem that is not an exception (a fallback taken, a part dropped)
    mark() / since(m)     the records made in this process since mark(): what one base caught (mesher.build)
    base(name)            say which base this process is making now (shown in every record)

Each record is one block in out\\kit_errors.log (all processes append to the same file):
    2026-11-19 21:20:03  fortress_100mm_16  [scenes.no_bare]  ValueError: ... (scenes.py:1873)
      Traceback (most recent call last): ...
Uncaught exceptions (a crash) are recorded too: the module installs sys.excepthook in every process that imports it.
KIT_ERRORS_PRINT=1 also prints each record's first line on stderr."""
import os, sys, time, traceback, threading

_LOCK = threading.Lock()
_REC = []                # this process's records: dicts (time, base, where, kind, text, at, tb)
_BASE = [os.environ.get("BASENAME", "")]
_FILE = [None]


def _file():
    if _FILE[0] is None:
        d = os.environ.get("KIT_ERRORS_DIR") or os.path.join(os.path.dirname(os.path.abspath(__file__)), "out")          # always the kit's own out folder (a run's folder changes)
        os.makedirs(d, exist_ok=True); _FILE[0] = os.path.join(d, "kit_errors.log")
    return _FILE[0]


def base(name):
    _BASE[0] = str(name or "")


def _who():
    if _BASE[0]: return _BASE[0]
    salt = os.environ.get("SALT", "")
    return f"seed {salt}" if salt else f"pid {os.getpid()}"


def _at(tb):
    """the last frame in the kit's own files: where it really went wrong"""
    here = os.path.dirname(os.path.abspath(__file__)).lower(); at = ""
    for fr in traceback.extract_tb(tb) if tb else []:
        f = os.path.abspath(fr.filename).lower()
        if f.startswith(here) and not f.endswith("kitlog.py"): at = f"{os.path.relpath(fr.filename, here)}:{fr.lineno} in {fr.name}"
    return at


def _write(rec):
    with _LOCK:
        _REC.append(rec)
        line = f"{rec['time']}  {rec['base']}  [{rec['where']}]  {rec['kind']}: {rec['text']}" + (f"  ({rec['at']})" if rec["at"] else "")
        try:
            with open(_file(), "a", encoding="utf-8") as f:
                f.write(line + "\n")
                if rec["tb"]: f.write("".join("    " + l + "\n" for l in rec["tb"].rstrip().splitlines()))
        except OSError: pass
        if os.environ.get("KIT_ERRORS_PRINT"): print("! " + line, file=sys.stderr, flush=True)
    return line


def caught(where, e=None):
    """record a caught exception; returns its one-line form (for the scene's own log)"""
    if e is None: e = sys.exc_info()[1]
    tb = e.__traceback__ if e is not None else None
    rec = dict(time=time.strftime("%Y-%m-%d %H:%M:%S"), base=_who(), where=str(where), kind=type(e).__name__ if e is not None else "error",
               text=str(e)[:400], at=_at(tb), tb="".join(traceback.format_exception(type(e), e, tb)) if e is not None else "")
    _write(rec)
    return f"{where}: {rec['kind']} {rec['text'][:80]}" + (f" ({rec['at']})" if rec["at"] else "")


def note(where, text):
    rec = dict(time=time.strftime("%Y-%m-%d %H:%M:%S"), base=_who(), where=str(where), kind="note", text=str(text)[:400], at="", tb="")
    _write(rec)
    return f"{where}: {text}"


def mark(): return len(_REC)


def since(m):
    """the records since mark m, as short lines: 'where: Kind text (file:line)'"""
    return [f"{r['where']}: {r['kind']} {r['text'][:120]}" + (f" ({r['at']})" if r["at"] else "") for r in _REC[m:]]


def _hook(t, v, tb):
    try:
        rec = dict(time=time.strftime("%Y-%m-%d %H:%M:%S"), base=_who(), where="crash", kind=t.__name__, text=str(v)[:400], at=_at(tb),
                   tb="".join(traceback.format_exception(t, v, tb)))
        if not issubclass(t, (KeyboardInterrupt, SystemExit)): _write(rec)
    except Exception: pass
    _prev(t, v, tb)


_prev = sys.excepthook
sys.excepthook = _hook


def fault(where):
    """a test of the log: KIT_FAULT=<where> makes that step fail on purpose (unset in normal runs)"""
    if os.environ.get("KIT_FAULT") == where: raise RuntimeError(f"KIT_FAULT test failure in {where}")
