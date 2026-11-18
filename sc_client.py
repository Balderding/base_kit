"""SolidCraft MCP client with a watchdog (Windows, macOS and Linux).

Each call gets step_limit_secs. A call that hangs, or a SolidCraft process that crashes, is handled the same way:
the process is restarted, every call that worked so far is replayed, and the bad call is reported as failed so the
build can skip that piece and carry on."""
import json, subprocess, threading, queue, time, base64, sys, os, shutil
from kit_settings import CFG

LIMIT = float(CFG["step_limit_secs"])
PER_CMD = float(CFG.get("step_limit_per_command_secs", 1.5))
SHOW = os.environ.get("KIT_QUIET") != "1"


def say(msg):
    """progress line for the terminal; run_set.py picks up lines starting with @@ and prints them."""
    if SHOW: sys.stderr.write("@@ " + msg + "\n"); sys.stderr.flush()


def cli_path():
    """Resolve solidcraft_cli to a real file, or stop with a plain message."""
    c = CFG["solidcraft_cli"]
    found = c if os.path.isfile(c) else shutil.which(c)
    if not found:
        raise SystemExit(f"solidcraft_cli not found: {c!r}\n"
                         f"Fix the path in settings.json (double backslashes), e.g. O:\\\\appsnotonc\\\\solidcraft\\\\solidcraft-cli.exe")
    return found


class Session:
    def __init__(s):
        s.done = []          # successful state-changing calls, replayed after a restart
        s.restarts = 0
        s.timings = []
        s._start()

    def _start(s):
        cmd = [cli_path(), "mcp", "--allow-path", CFG["out_folder"]]
        if CFG.get("live"): cmd += ["--connect", "127.0.0.1:27878"]
        flags = getattr(subprocess, "CREATE_NO_WINDOW", 0)
        s.p = subprocess.Popen(cmd, stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.DEVNULL,
                               text=True, encoding="utf-8", bufsize=1, creationflags=flags)
        import atexit; atexit.register(s.close)
        s.q = queue.Queue(); s._id = 0
        threading.Thread(target=s._reader, args=(s.p, s.q), daemon=True).start()
        s._rpc("initialize", {"protocolVersion": "2025-06-18", "capabilities": {},
                              "clientInfo": {"name": "base-kit", "version": "1"}}, limit=60)
        s._rpc("notifications/initialized", notify=True)
        say("solidcraft started" + (" (live)" if CFG.get("live") else ""))

    @staticmethod
    def _reader(p, q):
        for line in p.stdout: q.put(line)
        q.put(None)                                   # process ended

    def _rpc(s, method, params=None, notify=False, limit=None):
        msg = {"jsonrpc": "2.0", "method": method}
        if params is not None: msg["params"] = params
        if not notify: s._id += 1; msg["id"] = s._id
        try: s.p.stdin.write(json.dumps(msg) + "\n"); s.p.stdin.flush()
        except (OSError, ValueError): return None     # process already gone
        if notify: return None
        deadline = time.time() + (limit or LIMIT)
        while True:
            left = deadline - time.time()
            if left <= 0: return None                 # hung
            try: line = s.q.get(timeout=left)
            except queue.Empty: return None
            if line is None: return None              # crashed
            try: r = json.loads(line)
            except ValueError: continue
            if r.get("id") == s._id: return r

    def _restart(s):
        s.restarts += 1
        say(f"RESTART #{s.restarts}: replaying {len(s.done)} steps")
        s.close()
        s._start()
        for name, args in s.done:
            s._rpc("tools/call", {"name": name, "arguments": args}, limit=60)

    def tool(s, name, args=None, keep=True, limit=None, restart=True):
        args = args or {}
        if limit is None:     # big steps (many sketch commands) get more time than small ones
            limit = LIMIT + PER_CMD * len(args.get("commands", []))
        t0 = time.time()
        r = s._rpc("tools/call", {"name": name, "arguments": args}, limit=limit)
        s.timings.append((round(time.time() - t0, 1), name, len(args.get("commands", [])) if isinstance(args, dict) else 0))
        n = len(args.get("commands", [])) if isinstance(args, dict) else 0
        secs = time.time() - t0
        say(f"{name}{f' x{n}' if n else ''} {secs:.1f}s" + (" HUNG/CRASHED, skipped" if r is None else
            (" ERROR" if r.get("error") or r["result"].get("isError") else "")))
        if r is None:
            if not restart: return {"isError": True, "text": ["watchdog: step hung, not restarted"], "images": []}
            s._restart()
            return {"isError": True, "text": ["watchdog: step hung or crashed, skipped"], "images": []}
        if "error" in r: return {"isError": True, "text": [json.dumps(r["error"])], "images": []}
        out = {"isError": r["result"].get("isError", False), "text": [], "images": []}
        for c in r["result"]["content"]:
            if c["type"] == "text": out["text"].append(c["text"])
            elif c["type"] == "image": out["images"].append(c["data"])
        if keep and not out["isError"]: s.done.append((name, args))
        return out

    def shot(s, path, **kw):
        a = {"source": "model", "width": 900, "height": 560}; a.update(kw)
        r = s.tool("screenshot", a, keep=False, limit=60)
        if r["images"]:
            with open(path, "wb") as f: f.write(base64.b64decode(r["images"][0]))

    def close(s):
        try:
            s.p.kill(); s.p.wait(5)
        except Exception: pass
