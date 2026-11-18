"""designer safety test: python tools/test_designer.py
Starts a fake model server on this PC that answers badly on purpose (cut-off JSON, an unknown object, numbers out of
range, an unknown subject, a slow answer, no server at all) and checks that each case ends in a repair or in the
built-in themes, with the right log line. No model and no GPU needed. Prints PASS or FAIL per case."""
import sys, os
KIT = os.path.dirname(os.path.dirname(os.path.abspath(__file__))); sys.path[0] = KIT      # first: tools/grp.py would otherwise be loaded in place of a standard module
import json, threading, time
from http.server import BaseHTTPRequestHandler, HTTPServer
sys.argv = sys.argv[:1]
os.environ.update(BASE_MM="100", KIT_SCENES="1", KIT_DESIGNER="on", SALT="fake")
import gg3, gg, scenes, designer
scenes.install(vars(gg3))
GOOD = {"theme": "flooded shaft", "subject": {"name": "shaft", "edge": False, "angle": 40, "offset": 0.3, "size": 1.0}, "path": {"kind": "trail", "route": "to", "from_angle": 220, "bend": 0.1},
        "props": [{"what": "ladder", "where": "front", "count": 1}, {"what": "cart", "where": "path", "count": 1}, {"what": "puddle", "where": "at", "count": 2}],
        "clutter": {"debris": 1.0, "growth": 0.2, "marks": 1.0}, "open": 1.0}
def variant(**kw): p = json.loads(json.dumps(GOOD)); p.update(kw); return p
MODE = {"reply": "", "delay": 0.0}

class H(BaseHTTPRequestHandler):
    def log_message(self, *a): pass
    def _send(self, obj):
        b = json.dumps(obj).encode(); self.send_response(200); self.send_header("Content-Type", "application/json"); self.send_header("Content-Length", str(len(b))); self.end_headers(); self.wfile.write(b)
    def do_GET(self): self._send({"data": [{"id": "fake-model"}]})
    def do_POST(self):
        self.rfile.read(int(self.headers.get("Content-Length", 0))); time.sleep(MODE["delay"])
        try: self._send({"choices": [{"message": {"content": MODE["reply"]}}]})
        except OSError: pass

srv = HTTPServer(("127.0.0.1", 0), H); threading.Thread(target=srv.serve_forever, daemon=True).start()
URL = f"http://127.0.0.1:{srv.server_address[1]}/v1"; os.environ["KIT_DESIGNER_URL"] = URL
designer.CFG.setdefault("designer", {}).update(cache=False, retries=0, timeout_secs=2, batch=1)
fails = 0

def case(name, reply, want, delay=0.0, url=None):
    """build one base through the same path as a preview: designer.plan_for inside scenes.compose"""
    global fails
    MODE.update(reply=reply if isinstance(reply, str) else json.dumps(reply), delay=delay); os.environ["KIT_DESIGNER_URL"] = url or URL; os.environ["RESEED"] = name.replace(" ", "")
    s = gg3.make("mine", 16); line = getattr(s, "plan_line", ""); ok = all(w in line for w in want) and bool(s.ops)
    fails += not ok
    print(f"  {'PASS' if ok else 'FAIL'}  {name:34} -> {line}", flush=True)

print("designer safety test (fake server at " + URL + ")")
case("good plan", {"plans": [GOOD]}, ['designer "flooded shaft"'])
case("plan in a thinking block and fence", "<think>let me think</think>\n```json\n" + json.dumps({"plans": [GOOD]}) + "\n```", ['designer "flooded shaft"'])
case("cut-off JSON", json.dumps({"plans": [GOOD]})[:140], ["designer failed", "table"])
case("unknown object", {"plans": [variant(props=GOOD["props"] + [{"what": "laser cannon", "where": "at", "count": 1}])]}, ['designer "flooded shaft"', "1 fix)"])
case("numbers out of range", {"plans": [variant(open=9, subject=dict(GOOD["subject"], size=5, offset=3))]}, ['designer "flooded shaft"', "fix"])
case("unknown subject", {"plans": [variant(subject=dict(GOOD["subject"], name="volcano"))]}, ["designer failed", "table"])
case("all objects unknown", {"plans": [variant(props=[{"what": "robot", "where": "at", "count": 1}])]}, ["designer failed", "table"])
case("not JSON at all", "Sure! Here is a lovely mine scene for you.", ["designer failed", "table"])
case("slow answer (over the time limit)", {"plans": [GOOD]}, ["designer failed (timeout)", "table"], delay=3.5)
case("no server", {"plans": [GOOD]}, ["designer failed (no server", "table"], url="http://127.0.0.1:9/v1")
print("ALL PASS" if not fails else f"{fails} FAILED"); sys.exit(1 if fails else 0)
