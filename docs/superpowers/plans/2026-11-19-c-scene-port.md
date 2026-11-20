# The scene code to C (step 3 of the C port)

Asked 2026-11-19 evening by balderding: "convert everything to C and use agents". Chosen order: the hot part first
(the rock clusters), then everything else file by file until no Python is left. SAME BASES: for the same seed the C
code must give byte-identical geometry to the Python, proven routine by routine.

## Rules for every agent (read all of them)
- Work only in O:\bases. Scratch files, test DLLs, proof scripts and outputs go in O:\bases\work\cscene\<your part>\.
- NEVER delete or overwrite files you did not create in this task. Never touch O:\bases\bbb_kit.exe or
  O:\bases\bbbcore.dll (the user's window is running). Build your own DLLs into your work folder.
- Do not commit, push or install anything. Nothing on C: (no pip installs, no downloads to C:).
- New .exe files are blocked by the PC's security rule (ASR): build test code as a DLL and call it from Python with
  ctypes (see cport\bbbcore.c, cmesh.py, cstone.py).
- Compiler: `call O:\appsnotonc\llvm\env.cmd` then clang %CLANG_FLAGS% with `-O2 -ffp-contract=off -fno-fast-math`
  (exact floats: no fused multiply-add, no fast-math). Write build steps into a .cmd file and run it with
  `cmd //c "O:\\...\\x.cmd"` from bash (bash heredocs mangle backslashes: write files with the Write tool).
- Python: C:\Python315\python.exe; shapely 2.2.0 with GEOS 3.14.1. The C code loads THAT same geos_c DLL
  (cport\geos_dyn.c; add any GEOS function you need to the GEOS_FNS list in cport\geos_dyn.h), so a GEOS call made
  from C gives exactly what shapely gives. GEOS 3.14.1 source for reading: O:\appsnotonc\geos-src. shapely source:
  C:\Users\balderding\AppData\Roaming\Python\Python315\site-packages\shapely.
- Python's random: cport\pyrandom.h (bit for bit). Python-exact maths (round, hypot, UCRT cos/sin/atan2...):
  cport\pyx.h. numpy results that feed geometry must be reproduced operation by operation (or handed over from
  Python, like gg._CSX in cstone.py).
- Proof = run the Python and the C on many inputs and compare BYTES (WKB of every geometry, every float with
  repr/hex, rng state after). Random-input tests in a DLL are cheap: use thousands to millions of cases. A routine is
  done only when its proof shows 0 differences; report the counts.
- No full theme runs and no STL builds (balderding's rule: one build test per change). Scene-level proofs
  (gg3.make + gg.build_cmds for a few seeds, like work\choicespeed\prove.py) are fine.
- Keep code readable: one C file per Python file or per topic, a comment naming the Python routine each C function
  copies. Plain C, no C++.

## Pieces
1. Rock clusters (gg3.cluster + gg.stone): cport\scatter.c, cstone.py, proven 8/8 scenes. Done, except the GEOS hull
   + simplify are slow (112 us a rock); being replaced by exact C copies of GEOS ConvexHull + TopologyPreserving
   Simplify for one convex ring (main session).
2. Shapely layer (agent): cport\shp.c / shp.h, the C versions of every shapely call the scene code makes (buffer
   with its options, union / unary_union, difference, intersection, contains / intersects / within / distance,
   simplify, affinity rotate / scale / translate, Point / LineString / Polygon constructors, polys_of, prepared
   geometry, STRtree query / nearest), each proven against shapely on random inputs.
3. Survey (agent, read only): the call graph of gg3.make / scenes.compose / details / features and a measured
   profile over the styles, giving the port order (leaves first, hottest first).
4. Then the scene files, one agent each, in the survey's order, built on piece 2.
