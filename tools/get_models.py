"""fetch the public-domain model packs the kit turns into base details: python tools/get_models.py [--all] [--list]

The kit does not ship other people's models. On its first run it says what it is about to fetch, from where, how big
and under which licence, and then downloads the packs it uses into models\\<pack>. Nothing else is downloaded, nothing
is sent anywhere, and a pack whose licence file does not say CC0 is set aside and not used. If the download does not
work, the same text tells you how to do it by hand (open the pack's page, press Download, unzip into models\\<pack>).

  (nothing)   fetch the packs the kit uses and does not have yet
  --all       fetch every pack in models\\packs.json (about 50, a few hundred MB), for trying more models
  --list      only say which packs are here and which are missing
  --pack NAME fetch that one pack if it is not here yet, used by the kit or not

Three kinds of pack: a Kenney pack (the link its page's Download button uses), a "zip" pack (one zip at a fixed
address, a GitHub repository; only its OBJ folder is unpacked) and a "files" pack (a collection of single models, each
its own file at a fixed address; only the models named in its "list" are fetched, into models\\<pack>\\glb, after the
repository's own licence pages have been read and still say CC0).
bbb_kit.cmd does the first of these by itself; --models off (or "models_download": false in settings.json) stops it."""
import sys, os
KIT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
if os.path.abspath(sys.path[0]) == os.path.dirname(os.path.abspath(__file__)): sys.path[0] = KIT      # first: tools/grp.py would otherwise be loaded in place of a standard module
import json, re, time, zipfile, urllib.request
MODELS = os.path.join(KIT, "models"); DL = os.path.join(MODELS, "_dl")
UA = {"User-Agent": "Mozilla/5.0 base-kit (fetches CC0 model packs once)"}

def manifest():
    with open(os.path.join(MODELS, "packs.json"), encoding="utf-8") as fh: return json.load(fh)

def have(p):
    if p.get("files"): return bool(p.get("list")) and all(os.path.isfile(file_of(p, n)) for n in p["list"])          # a pack of single files: here only when every listed model is
    return os.path.isdir(os.path.join(MODELS, p["name"], "obj" if p.get("zip") else os.path.join("Models", "OBJ format")))

def missing(all_=False): return [p for p in manifest()["packs"] if (all_ or p.get("used")) and not have(p)]

def explain(packs, say=print):
    m = manifest(); mb = sum(p.get("mb", 4) for p in packs)
    by = sorted({p.get("by", m["author"]) for p in packs})
    say(f"The kit turns ready-made 3D models into base details (gravestones, barrels, cannon ...). They are not part of the kit:")
    say(f"they are free models by {' and '.join(by)}, in the public domain ({m['licence']}: anyone may use them for any purpose).")
    say(f"{len(packs)} pack{'s' if len(packs) != 1 else ''} the kit uses {'is' if len(packs) == 1 else 'are'} not on this computer yet, so the kit will now download {'it' if len(packs) == 1 else 'them'} (about {mb:.0f} MB in all):")
    for p in packs: say(f"    {p['title']:<28} {p['page']}" + (f"  ({len(p['list'])} single models out of the collection, not all of it)" if p.get("files") else ""))
    say(f"They go into {MODELS}\\<pack>. This happens once. Nothing else is downloaded and nothing is sent anywhere.")
    say(f"To stop it: start with --models off (bases are then made without the model details).")

def by_hand(p, say=print):
    if p.get("files"):
        say(f"    By hand: open {p['page']} , and from its folder models/<name> download <name>.glb for each of the {len(p['list'])} models")
        say(f"    named under \"list\" for this pack in {os.path.join(MODELS, 'packs.json')}, into {os.path.join(MODELS, p['name'], 'glb')}. Then start again."); return
    if p.get("zip"):
        say(f"    By hand: download {p['zip']} , and copy the .obj files from its folder {p['obj']}")
        say(f"    into {os.path.join(MODELS, p['name'], 'obj')}. Then start again."); return
    say(f"    By hand: open {p['page']} , press Download, and unzip the file into {os.path.join(MODELS, p['name'])}")
    say(f"    (so that {os.path.join(MODELS, p['name'], 'Models', 'OBJ format')} exists). Then start again.")

def fetch(p, say=print):
    """one pack: ask the site for its download link (the same one its Download button uses), fetch, unzip, check the licence"""
    os.makedirs(DL, exist_ok=True); z = os.path.join(DL, p["name"] + ".zip"); dst = os.path.join(MODELS, p["name"])
    if p.get("files"): return fetch_files(p, dst, say)
    if p.get("zip"): return fetch_zip(p, z, dst, say)
    try:
        if not (os.path.exists(z) and zipfile.is_zipfile(z)):
            key = ("assets/" + p["name"]).encode().hex()
            req = urllib.request.Request(m_link(key), headers=dict(UA, Referer=p["page"]))
            with urllib.request.urlopen(req, timeout=30) as r: url = json.loads(r.read().decode()).get("url", "")
            if not url: raise RuntimeError("the site gave no download link")
            with urllib.request.urlopen(urllib.request.Request(url, headers=dict(UA, Referer=p["page"])), timeout=120) as r, open(z, "wb") as fh: fh.write(r.read())
        with zipfile.ZipFile(z) as zf:
            lic = next((n for n in zf.namelist() if n.lower().endswith("license.txt")), None); txt = zf.read(lic).decode("utf-8", "replace") if lic else ""
            if "cc0" not in txt.lower() and "creative commons zero" not in txt.lower():
                say(f"    {p['title']}: its licence file does not say CC0, so it is NOT unpacked or used (the zip stays in {DL})."); return False
            zf.extractall(dst)
        say(f"    {p['title']}: fetched ({os.path.getsize(z) / 1e6:.1f} MB), licence CC0, in {dst}"); return have(p)
    except Exception as e:
        say(f"    {p['title']}: could not be fetched ({type(e).__name__}: {str(e)[:80]})."); by_hand(p, say); return False

def fetch_zip(p, z, dst, say):
    """a pack that is one zip at a fixed address (a GitHub repository): fetch it, check its licence file says CC0, and
    unpack only its OBJ folder (p["obj"], a path inside the zip) into models\\<name>\\obj, with the licence beside it"""
    try:
        if not (os.path.exists(z) and zipfile.is_zipfile(z)):
            with urllib.request.urlopen(urllib.request.Request(p["zip"], headers=UA), timeout=180) as r, open(z, "wb") as fh: fh.write(r.read())
        with zipfile.ZipFile(z) as zf:
            names = zf.namelist(); lic = next((n for n in names if os.path.basename(n).lower().startswith("license")), None); txt = zf.read(lic).decode("utf-8", "replace") if lic else ""
            if "cc0" not in txt.lower() and "creative commons zero" not in txt.lower():
                say(f"    {p['title']}: its licence file does not say CC0, so it is NOT unpacked or used (the zip stays in {DL})."); return False
            pre = p["obj"].strip("/") + "/"; n = 0; os.makedirs(os.path.join(dst, "obj"), exist_ok=True)
            for name in names:
                if name.startswith(pre) and not name.endswith("/") and name.lower().endswith((".obj", ".mtl")):
                    with open(os.path.join(dst, "obj", os.path.basename(name)), "wb") as fh: fh.write(zf.read(name))
                    n += 1
            with open(os.path.join(dst, "License.txt"), "w", encoding="utf-8") as fh: fh.write(txt)
        if not n: say(f"    {p['title']}: the zip holds no models where they were expected ({pre})."); return False
        say(f"    {p['title']}: fetched ({os.path.getsize(z) / 1e6:.1f} MB), licence CC0, in {dst}"); return have(p)
    except Exception as e:
        say(f"    {p['title']}: could not be fetched ({type(e).__name__}: {str(e)[:80]})."); by_hand(p, say); return False

def file_of(p, n): return os.path.join(MODELS, p["name"], "glb", n + ".glb")

def fetch_files(p, dst, say):
    """a pack that is a list of single files (one model each, at p["files"] with <name> filled in): first read the
    repository's own licence pages (p["check"]) and go on only if they still say CC0; then fetch the listed models that
    are not here yet, one at a time, into models\\<name>\\glb. The models carry no licence text themselves, so the
    statement from packs.json is written beside them as License.txt."""
    try:
        seen = []
        for u in p.get("check", []):
            with urllib.request.urlopen(urllib.request.Request(u, headers=UA), timeout=60) as r: txt = r.read().decode("utf-8", "replace")
            if "cc0" not in txt.lower() and "creative commons zero" not in txt.lower():
                say(f"    {p['title']}: {u} no longer says CC0, so nothing is fetched and the pack is NOT used."); return False
            seen.append(u)
        if not seen: say(f"    {p['title']}: packs.json names no licence page to check, so nothing is fetched and the pack is NOT used."); return False
        os.makedirs(os.path.join(dst, "glb"), exist_ok=True); n = 0; size = 0
        for name in p["list"]:
            if not re.fullmatch(r"[A-Za-z0-9_.-]+", name) or ".." in name: raise RuntimeError(f"not a model name: {name[:30]}")
            f = file_of(p, name)
            if os.path.isfile(f): continue
            with urllib.request.urlopen(urllib.request.Request(p["files"].replace("<name>", name), headers=UA), timeout=120) as r: data = r.read()
            if data[:4] != b"glTF": raise RuntimeError(f"{name}.glb is not a GLB model")
            with open(f + ".part", "wb") as fh: fh.write(data)
            os.replace(f + ".part", f); n += 1; size += len(data); time.sleep(0.4)          # one at a time, a short pause between
        with open(os.path.join(dst, "License.txt"), "w", encoding="utf-8", newline="\n") as fh:
            fh.write(f"{p['title']}\nby {p['by']}\n{p['page']}\n\nLicence: {p['licence']}\nStated at: {p['licence_at']}\n\n"
                     f"Checked {time.strftime('%Y-%m-%d')} before fetching; these pages of the repository said CC0:\n" + "".join(f"  {u}\n" for u in seen)
                     + f"\nOnly the {len(p['list'])} models the kit uses are here (glb\\<name>.glb), each fetched from\n  {p['files']}\n")
        say(f"    {p['title']}: fetched {n} model{'s' if n != 1 else ''} ({size / 1e6:.1f} MB), licence CC0 (read at {len(seen)} page{'s' if len(seen) != 1 else ''} of the repository), in {dst}"); return have(p)
    except Exception as e:
        say(f"    {p['title']}: could not be fetched ({type(e).__name__}: {str(e)[:80]})."); by_hand(p, say); return False

def m_link(key): return manifest()["link"].replace("<key>", key)

def ensure(say=print, all_=False, only=None):
    """what bbb_kit.py calls: explain, then fetch what is missing. Returns how many packs are still missing.
    only: the name of one pack, to fetch just that one (used by the kit or not)."""
    todo = [p for p in missing(True) if p["name"] == only] if only else missing(all_)
    if not todo: return 0
    explain(todo, say); left = 0
    for p in todo:
        left += not fetch(p, say); time.sleep(2)          # one at a time, a pause between: the site is one man's
    if left: say(f"{left} pack{'s' if left != 1 else ''} still missing: bases are made without {'its' if left == 1 else 'their'} details until {'it is' if left == 1 else 'they are'} there.")
    return left

if __name__ == "__main__":
    if "--list" in sys.argv:
        for p in manifest()["packs"]: print(f"{'here   ' if have(p) else 'missing'} {'used' if p.get('used') else '    '} {p['name']:<36} {p['page']}")
    elif "--pack" in sys.argv:
        name = (sys.argv[sys.argv.index("--pack") + 1:] or [""])[0]; pk = [p for p in manifest()["packs"] if p["name"] == name]
        if not pk: print(f"no pack called '{name}' in models\\packs.json")
        else: n = ensure(print, only=name); print(f"{name} is here" if not n and have(pk[0]) else "")
    else:
        n = ensure(print, "--all" in sys.argv); print("all packs are here" if not n and not missing("--all" in sys.argv) else "")
