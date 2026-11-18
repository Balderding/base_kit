"""pull down the other free model packs found on 2026-11-17, for looking at and trying: python tools/get_more_models.py
Nothing here is used by the kit yet. Everything goes to models\\_more\\<group>\\<pack>\\ (not in git), each pack with a
_source.txt saying where it came from and which licence its page showed when it was fetched.

  kaykit\\     every repository of github.com/KayKit-Game-Assets (CC0 text in each LICENSE.txt)
  oga_cc0\\    OpenGameArt packs whose page shows CC0 and nothing else
  oga_other\\  OpenGameArt packs whose page shows another licence, or more than one: fetched to look at, NOT free to
              use without reading that licence (credit, share-alike ...). Do not build these into the kit as they are.
  other\\      the Base Mesh mirror (CC0), Gobkit Nature Kit (CC0)

One request at a time with a pause between: these are small sites. A pack that is already there is skipped, so the
script can be run again after a break. A line per pack is printed and kept in models\\_more\\log.txt."""
import sys, os, re, json, time, zipfile, urllib.request, urllib.parse, html
KIT = os.path.dirname(os.path.dirname(os.path.abspath(__file__))); sys.path[0] = KIT
DEST = os.path.join(KIT, "models", "_more"); os.makedirs(DEST, exist_ok=True)
UA = {"User-Agent": "Mozilla/5.0 (Windows NT 10.0; Win64; x64) base-kit model fetch"}
OGA = """low-poly-cc0-western-objects-pack low-poly-desert-assets 3td-starter-pack-1 3d-dungeon-debris-skull-and-bones 3d-greek-weapons-set
3td-africa-savanna-pack-v10 3td-cave-pack-pro-v10 3td-fantasy-ruins-pack 3td-harbour-pack 3td-tropical-environment-pack-v20
4-piece-stonehenge-opendungeons alien-buildingbase-set barbed-wire big-lowpoly-environment-pack crafting-items crate-barrel-bundle
destroyed-city-assets dungeon-assets fantasy-torch-pack-1 free-mine-assets-pack free-pillars-models free-trashes human-skull-0
low-poly-pirate-pack low-poly-rocks lowpoly-modular-dungeon-pack lowpoly-pirate-props medieval-containers medieval-props-pack-1
medieval-smith-pack metal-barrels-pack modular-dungeon-3d-models modular-industrial-kit modular-temple-3d-models modular-village
park-props-lowpoly rocks-02 sci-fi-environment-pack ship-cannon simple-3d-props simple-sandbags treasure-pack wood-barrels-pack
potions3 potions medieval-props-textured nature-kit""".split()          # the last four were asked for by hand
KEEP = (".zip", ".7z", ".rar", ".obj", ".blend", ".fbx", ".glb", ".gltf", ".dae", ".stl", ".tar.gz", ".3ds", ".ply", ".mtl")

def say(msg):
    print(msg, flush=True)
    with open(os.path.join(DEST, "log.txt"), "a", encoding="utf-8") as fh: fh.write(time.strftime("%Y-%m-%d %H:%M:%S ") + msg + "\n")
def get(url, to=None, timeout=180):
    with urllib.request.urlopen(urllib.request.Request(url, headers=UA), timeout=timeout) as r:
        if to is None: return r.read()
        with open(to, "wb") as fh:
            while True:
                b = r.read(1 << 20)
                if not b: break
                fh.write(b)
    return None
def unzip(z, into):
    try:
        with zipfile.ZipFile(z) as zf:
            for n in zf.namelist():          # nothing may land outside the pack's own folder
                t = os.path.abspath(os.path.join(into, n))
                if not t.startswith(os.path.abspath(into) + os.sep): raise RuntimeError("unsafe path in zip: " + n)
            zf.extractall(into)
        return True
    except Exception as e: say(f"      could not unzip {os.path.basename(z)}: {type(e).__name__}: {str(e)[:80]}"); return False
def source(folder, **kw):
    with open(os.path.join(folder, "_source.txt"), "w", encoding="utf-8") as fh: fh.write("".join(f"{k}: {v}\n" for k, v in kw.items()) + f"fetched: {time.strftime('%Y-%m-%d')}\n")

def kaykit():
    repos = []
    for kind in ("users", "orgs"):          # the account is a user account; an organisation address is tried as well in case that changes
        try: repos = [r["name"] for r in json.loads(get(f"https://api.github.com/{kind}/KayKit-Game-Assets/repos?per_page=100"))]; break
        except Exception as e: err = f"{type(e).__name__}: {e}"
    if not repos: say(f"kaykit: could not list the repositories ({err})"); return
    for name in sorted(repos):
        d = os.path.join(DEST, "kaykit", name)
        if os.path.exists(os.path.join(d, "_source.txt")): say(f"kaykit  {name}: already here"); continue
        os.makedirs(d, exist_ok=True); z = os.path.join(d, name + ".zip")
        try:
            for br in ("main", "master"):
                try: get(f"https://github.com/KayKit-Game-Assets/{name}/archive/refs/heads/{br}.zip", z); break
                except urllib.error.HTTPError:
                    if br == "master": raise
            ok = unzip(z, d); lic = ""
            for root, _, files in os.walk(d):
                for f in files:
                    if f.lower().startswith("license"): lic = open(os.path.join(root, f), encoding="utf-8", errors="replace").read(400).replace("\n", " "); break
                if lic: break
            source(d, source=f"https://github.com/KayKit-Game-Assets/{name}", licence_text=lic or "no LICENSE file found: check the page")
            say(f"kaykit  {name}: {os.path.getsize(z) / 1e6:.1f} MB{'' if ok else ' (not unzipped)'}  licence: {'CC0' if 'cc0' in lic.lower() else 'CHECK: ' + lic[:60]}")
        except Exception as e: say(f"kaykit  {name}: FAILED {type(e).__name__}: {str(e)[:80]}")
        time.sleep(2)

def oga():
    for slug in OGA:
        done = [g for g in ("oga_cc0", "oga_other") if os.path.exists(os.path.join(DEST, g, slug, "_source.txt"))]
        if done: say(f"oga     {slug}: already here ({done[0]})"); continue
        url = "https://opengameart.org/content/" + slug
        try:
            page = get(url, timeout=60).decode("utf-8", "replace")
            lics = sorted(set(html.unescape(x).strip() for x in re.findall(r"class=['\"]license-name['\"][^>]*>([^<]+)<", page)))
            if not lics: lics = sorted(set(re.findall(r"licenses/(?:by(?:-sa|-nc)?/[\d.]+|publicdomain/zero/[\d.]+|gpl[^\"'/]*)", page)))
            files = []
            for u in re.findall(r"https?://opengameart\.org/sites/default/files/[^\"'<> ]+", page):
                u = html.unescape(u)
                if u.lower().split("?")[0].endswith(KEEP) and u not in files: files.append(u)
            cc0 = bool(lics) and all("cc0" in l.lower() or "zero" in l.lower() for l in lics)
            d = os.path.join(DEST, "oga_cc0" if cc0 else "oga_other", slug); os.makedirs(d, exist_ok=True); mb = 0
            for u in files:
                fn = urllib.parse.unquote(u.split("/")[-1].split("?")[0]); t = os.path.join(d, fn)
                get(u.replace(" ", "%20"), t); mb += os.path.getsize(t) / 1e6
                if fn.lower().endswith(".zip"): unzip(t, os.path.join(d, fn[:-4]))
                time.sleep(1.5)
            source(d, source=url, licences_on_the_page=", ".join(lics) or "none found: read the page", files=len(files), note="" if cc0 else "NOT CC0-only: read the licence before any use")
            say(f"oga     {slug}: {len(files)} file(s), {mb:.1f} MB, licence on the page: {', '.join(lics) or 'NOT FOUND'}{'' if cc0 else '   <-- not CC0-only, kept in oga_other'}")
        except Exception as e: say(f"oga     {slug}: FAILED {type(e).__name__}: {str(e)[:80]}")
        time.sleep(2)

def other():
    for name, url, lic in (("base-meshes", "https://github.com/M3-org/base-meshes/archive/refs/heads/main.zip", "CC0 (thebasemesh.com: '100% Free. CC0 License.'; this is the M3-org mirror)"),
                           ("gobkit-nature-kit", "https://gobkit.com/freebies/environment/gobkit-nature-kit.zip", "CC0 1.0 Universal (gobkit.com/freebies)")):
        d = os.path.join(DEST, "other", name)
        if os.path.exists(os.path.join(d, "_source.txt")): say(f"other   {name}: already here"); continue
        os.makedirs(d, exist_ok=True); z = os.path.join(d, name + ".zip")
        try: get(url, z, timeout=900); ok = unzip(z, d); source(d, source=url, licence=lic); say(f"other   {name}: {os.path.getsize(z) / 1e6:.1f} MB{'' if ok else ' (not unzipped)'}")
        except Exception as e: say(f"other   {name}: FAILED {type(e).__name__}: {str(e)[:80]}")
        time.sleep(2)

if __name__ == "__main__":
    say("---- start ----"); kaykit(); oga(); other()
    n = sum(len(fs) for _, _, fs in os.walk(DEST)); mb = sum(os.path.getsize(os.path.join(r, f)) for r, _, fs in os.walk(DEST) for f in fs) / 1e6
    say(f"---- done: {n} files, {mb:.0f} MB in {DEST} ----")
