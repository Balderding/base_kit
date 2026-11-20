"""the kit as one zip for the Releases page: python tools/make_release.py
Writes release/bbb_kit_<version>.zip holding the kit exactly as it is in the last commit (HEAD): every file is
taken from the commit, not from the folder, so nothing half-finished gets in even while other work is under way.
Added beside them: bbb_kit.exe (the window, built by gui\\build.cmd) and bbbcore.dll (the C parts Python calls,
built by cport\\build.cmd); neither is kept in git. Left out: the model packs (the kit fetches them on its first
run and says so). The version is the committed VERSION file."""
import sys, os, time
KIT = os.path.dirname(os.path.dirname(os.path.abspath(__file__))); sys.path[0] = KIT
import subprocess, zipfile
def git(*a): return subprocess.run(["git", *a], cwd=KIT, capture_output=True, text=True).stdout
def blob(f): return subprocess.run(["git", "show", "HEAD:" + f], cwd=KIT, capture_output=True).stdout
ver = blob("VERSION").decode("utf-8").strip()
dirty = [l for l in git("status", "--porcelain").splitlines() if l and not l.startswith("??")]
if dirty: print("NOTE: these have changes not committed; the zip has their committed version:\n" + "\n".join(dirty))
NOTES = {".gitignore"}
files = [f for f in git("ls-tree", "-r", "--name-only", "HEAD").splitlines() if f and f not in NOTES and not (f.startswith("models/") and f.count("/") > 1)]
os.makedirs(os.path.join(KIT, "release"), exist_ok=True); z = os.path.join(KIT, "release", f"bbb_kit_{ver}.zip")
now = time.localtime()[:6]
with zipfile.ZipFile(z, "w", zipfile.ZIP_DEFLATED, compresslevel=9) as zf:
    for f in files: zf.writestr(zipfile.ZipInfo("bbb_kit/" + f, date_time=now), blob(f), zipfile.ZIP_DEFLATED, 9)
    exe = os.path.join(KIT, "bbb_kit.exe")          # the window: built by gui\\build.cmd, not kept in git
    if os.path.exists(exe) and os.path.getmtime(exe) >= os.path.getmtime(os.path.join(KIT, "gui", "bbb_kit.c")) - 1: zf.write(exe, "bbb_kit/bbb_kit.exe"); files.append("bbb_kit.exe")
    else: print("NOTE: bbb_kit.exe is missing or older than gui\\bbb_kit.c, so the zip has no window. Run gui\\build.cmd first.")
    dll = os.path.join(KIT, "bbbcore.dll")          # the C parts Python calls (the rocks, the mesher): built by cport\\build.cmd, not kept in git
    if os.path.exists(dll) and os.path.getmtime(dll) >= max(os.path.getmtime(os.path.join(KIT, "cport", f)) for f in ("bbbcore.c", "scatter.c", "mesh.c", "field.c")) - 1: zf.write(dll, "bbb_kit/bbbcore.dll"); files.append("bbbcore.dll")
    else: print("NOTE: bbbcore.dll is missing or older than its sources, so the zip has none (the kit then uses Python for those parts). Run cport\\build.cmd first.")
print(f"version {ver}, commit {git('rev-parse', '--short', 'HEAD').strip()}: {len(files)} files, {os.path.getsize(z) / 1e6:.1f} MB -> {z}")
