"""Designer: a local model (LM Studio, llama.cpp server, vLLM, Ollama) writes the scene plan; the kit builds it.

The model returns plan dicts only (see PLAN_SCENES.md A1): a theme name, a subject, a path, objects and where they
belong. Every name comes from the kit's own lists, sent in the prompt and enforced by a JSON schema. The kit checks
and repairs each plan (scenes.validate_plan) and does all the geometry. If the model is off, slow, wrong or absent
the kit uses its built-in themes: a base never fails because of the model. Nothing leaves this PC.

Settings: the "designer" block in settings.json. Standard library only."""
import json, os, re, time, hashlib, urllib.request, urllib.error
from kit_settings import CFG, HERE

DEFAULTS = {"enabled": False, "api": "openai", "url": "http://localhost:1234/v1", "model": "", "temperature": 0.9, "timeout_secs": 60, "retries": 2,
            "batch": 8, "cache": True, "max_tokens": 4000, "thinking": False, "extra": {}}
LAST = {"error": "", "secs": 0.0, "raw": "", "model": ""}

def cfg():
    d = dict(DEFAULTS); d.update(CFG.get("designer") or {})
    e = os.environ.get("KIT_DESIGNER", "")
    if e in ("on", "off"): d["enabled"] = e == "on"
    for k, ev in (("url", "KIT_DESIGNER_URL"), ("model", "KIT_DESIGNER_MODEL"), ("api", "KIT_DESIGNER_API")):
        if os.environ.get(ev): d[k] = os.environ[ev]
    d["url"] = str(d["url"]).rstrip("/")
    return d

def enabled(): return bool(cfg()["enabled"])

def _http(url, body=None, timeout=10):
    req = urllib.request.Request(url, data=None if body is None else json.dumps(body).encode("utf-8"), headers={"Content-Type": "application/json"})
    with urllib.request.urlopen(req, timeout=timeout) as r: return json.loads(r.read().decode("utf-8", "replace"))

def models():
    """the model names the server lists ([] if it does not answer)"""
    c = cfg()
    try:
        if c["api"] == "ollama": return [m.get("name", "") for m in _http(c["url"] + "/api/tags", timeout=3).get("models", [])]
        return [m.get("id", "") for m in _http(c["url"] + "/models", timeout=3).get("data", [])]
    except Exception: return []

def available(): return bool(models())

def model_name():
    c = cfg()
    if c["model"]: return c["model"]
    ms = models()
    return ms[0] if ms else ""


# ------------------------------------------------------------------------------------------------ what the model is told
SYSTEM = """You design scenes for round miniature bases that will be 3D printed. You do not draw anything. You fill in a plan, and a program builds it. Use only the names you are given.

A good base is one place, not a pile of objects:
1. One subject. Everything else is part of it or background.
2. A path that comes from the edge and ends at the subject ("to"), or passes in front of it ("past").
3. Every object belongs to the subject or the path: tools by the work, crates by the door, bones in the pit.
4. Natural clutter gathers at the foot of walls and rocks and along edges, not everywhere.
5. Open ground in front of the subject, where a figure will stand.

Give each plan a short theme name (2 to 4 words) that says what the place is. Make every plan a different place with a different subject or a different story. Do not put in an object that the theme cannot explain. Use 5 to 8 object lines per plan.

A miniature will stand on the finished base. You are told what kind. The program keeps the spots where it touches the base flat and clear, in front of the subject, so design the place around a figure standing there: the subject is something it faces, guards, climbs out of or has just reached.

subject.height says how tall the subject stands: 1 is low relief, 2 to 4 makes walls, pillars, doorways and machines stand properly tall, up to 8 is towering. There is no height limit, so use it: a gate, a statue or a ruin should rise well above the ground. Flat things (a pool, a crater, a camp, graves) stay at 1.

rim is a backdrop: one stretch of wall, cave side or cliff on the edge of the base directly behind the subject, the thing the subject is set against. Use it only when the place really has something behind it (a mine working against the rock, a shrine against the temple wall, a barricade against a building). Open country, a road, a beach, a field, a camp in the open have none: rim.cover 0. Otherwise rim.cover is 0.1 to 0.3 (the share of the edge) and rim.height how tall it stands (1 low, 3 strong, 6 a cliff or a high wall). Never ring the base with things.

Angles are degrees round the base. path.from_angle must be on the far side of the base from subject.angle. subject.edge is true only for a portal (always) or for a wall, hull or ruin that is cut off by the edge of the base; for everything else it is false and offset says how far from the centre the subject sits (0 to 0.5). clutter values: 1 is normal, 0 is none, 2 is a lot. open: 1 is normal open ground in front of the subject.

Example of one plan (for a mine; do not copy it):
{"theme":"tunnel mouth","subject":{"name":"portal","edge":true,"angle":200,"offset":0.0,"size":1.0,"height":3.0},"path":{"kind":"track","route":"to","from_angle":20,"bend":0.15},"props":[{"what":"cart","where":"path","count":1},{"what":"timbers","where":"wall","count":2},{"what":"pick","where":"path","count":1},{"what":"orepile","where":"yard","count":1},{"what":"lantern","where":"front","count":1}],"clutter":{"debris":1.0,"growth":0.3,"marks":0.6},"open":1.0,"rim":{"cover":0.0,"height":1.0}}"""

WHERE_SAYS = ("at = beside the subject, wall = against it and lined up with it, front = in front of it, back = behind it, in = inside it, path = beside the path, "
              "start = where the path meets the edge, yard = at the edge of the open ground, far = away from the subject. Nothing goes along the edge of the base: things stand where someone would have put them")

def _lists(style):
    import scenes
    return scenes.legal_subjects(style), scenes.legal_paths(style), scenes.legal_props(style)

def schema(style, mm, n=1):
    """JSON Schema of {"plans": [plan, ...]} with the legal names for this style filled in"""
    import scenes
    subs, paths, props = _lists(style); num = lambda lo, hi: {"type": "number", "minimum": lo, "maximum": hi}
    plan = {"type": "object", "additionalProperties": False, "required": ["theme", "subject", "path", "props", "clutter", "open", "rim"], "properties": {
        "rim": {"type": "object", "additionalProperties": False, "required": ["cover", "height"], "properties": {"cover": num(0, 0.3), "height": num(1, 8)}},
        "theme": {"type": "string", "minLength": 3, "maxLength": 40},
        "subject": {"type": "object", "additionalProperties": False, "required": ["name", "edge", "angle", "offset", "size", "height"], "properties": {
            "name": {"type": "string", "enum": subs}, "edge": {"type": "boolean"}, "angle": num(0, 360), "offset": num(0, 0.5),
            "size": num(0.6, 1.6 if scenes.mini_type() == "hover" else 1.3), "height": num(1, 8)}},
        "path": {"type": "object", "additionalProperties": False, "required": ["kind", "route", "from_angle", "bend"], "properties": {
            "kind": {"type": "string", "enum": paths}, "route": {"type": "string", "enum": ["to", "past"]}, "from_angle": num(0, 360), "bend": num(-0.3, 0.3)}},
        "props": {"type": "array", "minItems": 3, "maxItems": 10, "items": {"type": "object", "additionalProperties": False, "required": ["what", "where", "count"], "properties": {
            "what": {"type": "string", "enum": props}, "where": {"type": "string", "enum": ["at", "wall", "front", "back", "in", "path", "start", "yard", "far"]},
            "count": {"type": "integer", "minimum": 1, "maximum": 4}}}},
        "clutter": {"type": "object", "additionalProperties": False, "required": ["debris", "growth", "marks"], "properties": {"debris": num(0, 2), "growth": num(0, 2), "marks": num(0, 2)}},
        "open": num(0.5, 1.5)}}
    return {"type": "object", "additionalProperties": False, "required": ["plans"], "properties": {"plans": {"type": "array", "minItems": n, "maxItems": n, "items": plan}}}

def prompt(style, mm, t, layout_facts, avoid, n=1):
    """the chat messages for n plans"""
    import scenes
    subs, paths, props = _lists(style)
    user = (f"Style: {style} ({scenes.STYLE_SAYS.get(style, style)}). Base: {mm:g} mm. Detail level: {t + 1} of 4.\n"
            + (f"Ground already on this base: {layout_facts}.\n" if layout_facts else "")
            + f"The miniature that will stand on it: {scenes.mini_type()}: {scenes.MINIS[scenes.mini_type()]['says']}.\n"
            + "Subjects you may use: " + "; ".join(f"{s} ({scenes.SUBJECT_SAYS.get(s, s)})" for s in subs) + ".\n"
            + "Paths: " + "; ".join(f"{p} ({scenes.PATH_SAYS.get(p, p)})" for p in paths) + ".\n"
            + "Objects: " + ", ".join(props) + ".\n"
            + "Places an object can go: " + WHERE_SAYS + ".\n"
            + f"Themes already used, do not repeat them or anything close to them: {', '.join(['tunnel mouth'] + [a for a in avoid if a != 'tunnel mouth'])}.\n"
            + f'Return {n} plan{"s" if n != 1 else ""} as JSON: {{"plans": [...]}}.')
    return [{"role": "system", "content": SYSTEM}, {"role": "user", "content": user}]

def _text_of(resp, api):
    if api == "ollama": return (resp.get("message") or {}).get("content", "")
    ch = (resp.get("choices") or [{}])[0]; return (ch.get("message") or {}).get("content") or ""

def parse(text):
    """the plans in a model reply: [] if it is not usable JSON. Tolerates a thinking block and a code fence."""
    if not isinstance(text, str): return []
    text = re.sub(r"<think>.*?</think>", "", text, flags=re.S)
    if "</think>" in text: text = text.split("</think>")[-1]
    text = re.sub(r"^```(?:json)?|```$", "", text.strip(), flags=re.M).strip()
    i = text.find("{")
    if i < 0: return []
    try: obj = json.JSONDecoder().raw_decode(text[i:])[0]
    except ValueError: return []
    if isinstance(obj, dict) and isinstance(obj.get("plans"), list): return [p for p in obj["plans"] if isinstance(p, dict)]
    if isinstance(obj, dict) and "subject" in obj: return [obj]
    return []

def ask(style, mm, t, layout_facts, n, seed, avoid=()):
    """n raw plans from the model; [] on any failure (LAST says why and how long it took)"""
    c = cfg(); t0 = time.time(); LAST.update(error="", raw="", secs=0.0)
    model = model_name(); LAST["model"] = model
    if not model: LAST["error"] = f"no server at {c['url']}"; return []
    msgs = prompt(style, mm, t, layout_facts, list(avoid), n); sch = schema(style, mm, n); sd = int(hashlib.md5(str(seed).encode()).hexdigest()[:7], 16)
    if c["api"] == "ollama":
        url = c["url"] + "/api/chat"; body = {"model": model, "messages": msgs, "stream": False, "format": sch, "think": False, "options": {"temperature": c["temperature"], "seed": sd}}
    else:
        url = c["url"] + "/chat/completions"
        body = {"model": model, "messages": msgs, "temperature": c["temperature"], "seed": sd, "max_tokens": int(c["max_tokens"]), "stream": False,
                "response_format": {"type": "json_schema", "json_schema": {"name": "scene_plans", "strict": True, "schema": sch}}}
    extra = dict(c.get("extra") or {})
    if not c.get("thinking") and c["api"] != "ollama": extra.setdefault("chat_template_kwargs", {"enable_thinking": False})      # llama.cpp server, vLLM: plans need no thinking, and it costs most of the time
    body.update(extra)
    out = []
    for k in range(1 + max(0, int(c["retries"]))):
        try:
            if k: body["seed"] = sd + k
            resp = _http(url, body, timeout=float(c["timeout_secs"])); LAST["raw"] = _text_of(resp, c["api"]); out = parse(LAST["raw"])
            if out: LAST["error"] = ""; break
            LAST["error"] = "reply was not usable JSON"
        except urllib.error.HTTPError as e:
            try: detail = e.read().decode("utf-8", "replace")[:160]
            except Exception: detail = ""
            LAST["error"] = f"HTTP {e.code} {detail}".strip()
            if e.code in (400, 422) and extra:          # a server that does not know the extra fields: once more without them
                for x in extra: body.pop(x, None)
                extra = {}; continue
            if e.code in (400, 404, 422): break
        except Exception as e:
            LAST["error"] = "timeout" if "timed out" in str(e).lower() else f"{type(e).__name__}: {str(e)[:80]}"
            if LAST["error"] == "timeout": break
    LAST["secs"] = round(time.time() - t0, 1)
    return out[:n]


# ------------------------------------------------------------------------------------------------ cache of unused plans
def _cache_path(): return os.path.join(CFG["out_folder"] if os.path.isabs(CFG["out_folder"]) else os.path.join(HERE, CFG["out_folder"]), "plan_cache.json")

def cache_load(path=None):
    try:
        with open(path or _cache_path(), encoding="utf-8") as f: return json.load(f)
    except (OSError, ValueError): return {}

def cache_save(d, path=None):
    try:
        with open(path or _cache_path(), "w", encoding="utf-8") as f: json.dump(d, f, indent=1)
    except OSError: pass

def plan_hash(plan): return hashlib.md5(json.dumps(plan, sort_keys=True).encode()).hexdigest()[:10]

def plans_for(style, mm, t, count, seed, layout_facts="", used=(), cache_file=None, say=print):
    """`count` checked plans for one style, cached ones first, then asked for in batches. May return fewer (or none):
    the caller builds the rest from the built-in themes. Unused plans go back into the cache."""
    import scenes
    c = cfg(); key = f"{style}@{mm:g}"; cache = cache_load(cache_file) if c["cache"] else {}
    have = []; used = list(used); stats = dict(asked=0, got=0, clean=0, fixed=0, refused=0, calls=0, secs=0.0, error="")
    def take(raw, src):
        p, notes = scenes.validate_plan(raw, style, mm, used)
        if p is None: stats["refused"] += 1; return
        p["_source"] = src; p["_notes"] = notes; have.append(p); used.append(p["theme"])
        stats["clean" if not notes else "fixed"] += 1
    pool = cache.get(key, [])
    while pool and len(have) < count: take(pool.pop(0), "designer (cached)")
    tries = 0
    while len(have) < count and tries < 2 + (count // max(1, int(c["batch"]))):
        tries += 1; n = max(1, int(c["batch"])); stats["calls"] += 1; stats["asked"] += n
        raw = ask(style, mm, t, layout_facts, n, f"{seed}{style}{tries}", used); stats["secs"] += LAST["secs"]
        if not raw: stats["error"] = LAST["error"]; break
        stats["got"] += len(raw)
        for r in raw:
            if len(have) < count: take(r, "designer")
            else:
                p, notes = scenes.validate_plan(r, style, mm, used)
                if p is not None: pool.append(p); used.append(p["theme"])
    if c["cache"]: cache[key] = pool[:40]; cache_save(cache, cache_file)
    return have[:count], stats

def plan_for(style, mm, t, layout_facts, seed, used=()):
    """one checked plan for one base (previews); None means "use the built-in themes" """
    if not enabled(): return None
    got, st = plans_for(style, mm, t, 1, seed, layout_facts, used, say=lambda *a: None)
    return got[0] if got else None
