"""the kit as one zip for the Releases page: python tools/make_release.py
Reads the version from the VERSION file and writes release/bbb_kit_<version>.zip holding the kit as it is committed
(the files git knows, so nothing half-finished gets in), plus bbb_kit.exe, the window, as built by gui\\build.cmd. Left out: the model packs (the kit fetches them on its first
run and says so).
Stops if a committed file has changes that are not committed yet, so the zip always matches one commit."""
import sys, os
KIT = os.path.dirname(os.path.dirname(os.path.abspath(__file__))); sys.path[0] = KIT
import subprocess, zipfile
def git(*a): return subprocess.run(["git", *a], cwd=KIT, capture_output=True, text=True).stdout
ver = open(os.path.join(KIT, "VERSION"), encoding="utf-8").read().strip()
dirty = [l for l in git("status", "--porcelain").splitlines() if l and not l.startswith("??")]
if dirty: sys.exit("not made: these files have changes that are not committed yet:\n" + "\n".join(dirty))
NOTES = {".gitignore"}
files = [f for f in git("ls-files").splitlines() if f and f not in NOTES and not (f.startswith("models/") and f.count("/") > 1)]
os.makedirs(os.path.join(KIT, "release"), exist_ok=True); z = os.path.join(KIT, "release", f"bbb_kit_{ver}.zip")
with zipfile.ZipFile(z, "w", zipfile.ZIP_DEFLATED, compresslevel=9) as zf:
    for f in files: zf.write(os.path.join(KIT, f), "bbb_kit/" + f)
    exe = os.path.join(KIT, "bbb_kit.exe")          # the window: built by gui\\build.cmd, not kept in git
    if os.path.exists(exe) and os.path.getmtime(exe) >= os.path.getmtime(os.path.join(KIT, "gui", "bbb_kit.c")) - 1: zf.write(exe, "bbb_kit/bbb_kit.exe"); files.append("bbb_kit.exe")
    else: print("NOTE: bbb_kit.exe is missing or older than gui\\bbb_kit.c, so the zip has no window. Run gui\\build.cmd first.")
print(f"version {ver}, commit {git('rev-parse', '--short', 'HEAD').strip()}: {len(files)} files, {os.path.getsize(z) / 1e6:.1f} MB -> {z}")
