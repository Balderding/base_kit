"""patch_geos.py <geos-src> <out-dir>: copies GEOS (without .git) to out-dir and points the few spots whose results
depend on the C++ standard library at msvc_stl.h (Microsoft's versions), so clang + libstdc++ builds the same GEOS
answers as the MSVC-built geos.dll in shapely's wheel:
  every std::sort / unqualified sort(...begin()...)  -> geos_msvc::sort          (unstable: ties)
  std::hash<double>{} / std::hash<size_t>{}          -> geos_msvc::hash_double / hash_size
  OverlayGraph's node map (iterated: the order of the result rings) -> geos_msvc::unordered_map
Every change is printed. The source tree itself is never touched."""
import os, re, sys, shutil

SRC, OUT = sys.argv[1], sys.argv[2]
HERE = os.path.dirname(os.path.abspath(__file__))
if os.path.exists(OUT):
    shutil.rmtree(OUT)
shutil.copytree(SRC, OUT, ignore=shutil.ignore_patterns(".git"))
shutil.copy(os.path.join(HERE, "msvc_stl.h"), os.path.join(OUT, "include", "geos", "msvc_stl.h"))

RULES = [
    (re.compile(r"\bstd::sort\("), "geos_msvc::sort("),
    (re.compile(r"(?<![\w:.>~])sort\((?=[^;]*begin\(\))"), "geos_msvc::sort("),
    (re.compile(r"\bstd::hash<double>\{\}"), "geos_msvc::hash_double{}"),
    (re.compile(r"\bstd::hash<(?:std::)?size_t>\{\}"), "geos_msvc::hash_size{}"),
    (re.compile(r"\bstd::unordered_map<Coordinate, OverlayEdge\*, geom::Coordinate::HashCode>"),
     "geos_msvc::unordered_map<Coordinate, OverlayEdge*, geom::Coordinate::HashCode>"),
]
n_files = n_changes = 0
for top in ("src", "include", "capi"):
    for dp, dn, fn in os.walk(os.path.join(OUT, top)):
        if os.sep + "vend" in dp:
            continue
        for f in fn:
            if not f.endswith((".cpp", ".h")) or f == "msvc_stl.h":
                continue
            p = os.path.join(dp, f)
            txt = open(p, encoding="utf-8", errors="surrogateescape").read()
            lines = txt.split("\n"); changed = False
            for i, ln in enumerate(lines):
                s = ln.lstrip()
                if s.startswith("//") or s.startswith("*") or s.startswith("/*"):
                    continue
                new = ln
                for rx, rep in RULES:
                    new = rx.sub(rep, new)
                if new != ln:
                    print(f"{os.path.relpath(p, OUT)}:{i + 1}: {new.strip()}")
                    lines[i] = new; changed = True; n_changes += 1
            if changed:
                n_files += 1
                txt = "\n".join(lines)
                txt = "#include <geos/msvc_stl.h> // patch_geos.py: MSVC STL sort / hash / unordered_map\n" + txt
                open(p, "w", encoding="utf-8", errors="surrogateescape", newline="").write(txt)
print(f"{n_changes} changes in {n_files} files")
