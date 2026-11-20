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

## 2026-11-19 night: EVERYTHING to C (balderding: "EVERYTHING NEEDS TO BE C")
Goal: bbb_kit.exe does every step itself; no python.exe anywhere (choices, builds, checks, pictures, model packs).
The survey is in work\cscene\survey\REPORT.md (call graph, the shapely/numpy calls, profile, port groups G0-G12,
hard spots): every agent reads it first.

Ownership (each agent writes ONLY its own files; the main session integrates; never edit another agent's files):
| piece | owner | files |
|---|---|---|
| G0 shapely layer | agent "shp" (running) | cport\shp.c, cport\shp.h, the SHP_GEOS_FNS list in geos_dyn.h |
| G1 Python-exact helpers | agent "pyexact" | cport\pyexact.c, cport\pyexact.h (numpy arange/linspace/meshgrid, CPython set + dict iteration order for float tuples, the random.Random calls not yet in pyrandom.h) |
| G3 scene core | agent "scene" | cport\scene.c, cport\scene.h (the scene object s, ops/gcuts/tcuts/feet/chops/objs/terr, LK, Z, the gg/gg2 Scene methods, gg2 terrain T_*), cport\leaf.c/.h (G2 leaf geometry) |
| models | agent "models" | cport\models.c, cport\models.h (OBJ/GLB loading as trimesh does it with process=False, the packs fetch tools\get_models.py, sd_mesh posing) |
| 2D drawing + runner | agent "runner" | cport\draw2d.c/.h (mesher.draw2d), cport\runner.c/.h (run_set.py, tools\gui_job.py, tools\choices.py, the print checks, the report CSV, live notes) |
| G4-G11 layout files | later agents, after G0 + G3 | one file each, in the survey's order |
Until the C layout exists, the runner may call the Python layout through one clearly marked function, so it can be
built and proven now; that call is the last Python left and goes when G4-G11 land.
| G0 shapely layer | DONE 2026-11-19 (79 calls, 233,000 cases, 0 differences; work\cscene\shp\proof_3000_seed11.txt) | cport\shp.c/.h |
| G5 solids + sculpt | agent "solids" | cport\solids.c, cport\solids.h (scenes.py _chain, box, vault, lathe, loft, tube, slab, curve, sculpt, _steps, _weather, _crate_layers, _Later, sd_r_models._fine; style_details.Obj and its tools) |
| G12 build_cmds | agent "buildcmds" | cport\buildcmds.c, cport\buildcmds.h (gg.build_cmds and its helpers) |

## 2026-11-19 night: ONE SELF-CONTAINED EXE (balderding: "The c exe has to run from anywhere and have everything it needs inside of it")
- bbb_kit.exe alone, copied to any folder on any Windows PC, makes choices and builds: no Python, no shapely DLL,
  no model folder, no settings file needed (a settings.json beside it may override the built-in defaults).
- GEOS: GEOS 3.14.1 compiled from O:\appsnotonc\geos-src straight into the exe (static), proven byte-identical to
  shapely's geos_c.dll with the shp proof (79 calls) and the mesh/field proofs. Owner: agent "geosstatic"
  (cport\geos_static\ build files, cport\geos_dyn.c gets a static mode only through a new file it owns).
- Model packs: only the files the kit uses, packed and compressed inside the exe (a resource), read in memory; no
  download, no models folder. Owner: agent "models" (cport\models.c/.h, a packer tool in its work folder).
- Data the scene code reads (scale\real.json, drawn.json, settings defaults, fonts, art): built into the exe too.
- Output: an "out" folder beside the exe (as now), created on first use.
- Release = that one exe (plus a README); the zip of Python files goes once the C exe makes the same bases.

## Progress (2026-11-19 night)
| piece | state |
|---|---|
| G0 shapely layer (cport\shp) | DONE: 79 calls, 233,000 cases, 0 differences |
| G1 Python-exact (cport\pyexact) | DONE: every proof 0 differences (work\cscene\pyexact\proof_*.txt) |
| G2 leaf + G3 scene core (cport\leaf, cport\scene) | DONE: 60,000 random scene cases, 428,000 leaf cases, 12,108 replayed real calls, 0 differences |
| G12 build_cmds (cportuildcmds) | DONE: 175 captured scenes, 34,271 lines, 0 differences (rerun by the main session) |
| G4 placement | agent "place" running |
| G5 solids, models + sd_mesh, runner + draw2d, GEOS static | agents running |
| G7-G11 | next, after G4 and G5 |

## Progress (later the same night)
- G4 placement DONE (20,876 replayed calls, 0 differences). G5 solids DONE (58,463 + 36,474 with supports, 0). models DONE
  (207 files byte-identical, blob 2.76 MB in work\cscene\models\models.bin). runner + draw2d DONE (draw2d pixel-identical on
  68 drawings; choices and build jobs match Python). GEOS static DONE (shp proof 233,000 x2, 2M rocks, 600,000 direct
  compares, mesher 10/10: 0 differences) and LINKED INTO bbb_kit.exe (guiuild.cmd: geos_link.c + cport\geos_static\lib):
  the exe imports only Windows DLLs; exe vs Python mesher STLs byte-identical; window quicktest 2/2.
- Running: layouts (G11), meshglue, features (G7), samplers (G8). Next: compose (G9), builders (G10), then wire everything
  into bbb_kit.exe and drop Python.

- G8 samplers DONE (55 bases, 1,092,141 checks, 0 differences). scenes._strips Python bug FIXED (it called the solid box(); now shapely box): the C must use samplers.h sm_strips_box, not sm_strips.

## The last wave (2026-11-19, late)
- DONE: G7 features (25,955 of 25,959; the 4 were place.c's floor list missing "obfloor", now added), G8 samplers,
  meshglue (153 scenes; close_seams exact on 136 STLs).
- cportuilders.h/.c (main session): the ONE builder table. Every builder file registers in bld_register_<part>():
  features, scenes, details, gg3, styles2, sd_abc, sd_de, sd_r, sd_rx, newstyles (weak symbols: missing parts skip).
- Running: compose (G9 + kinds.c, the one shared kind table + make.c), bld-features (features/details/styles2),
  bld-scenes (subjects, props, gg3 tiers), bld-sdabc, bld-sdde (+ model details), bld-sdrx (19 sd_r_x files),
  bld-newstyles (the six newer styles), layouts (G11, finishing).
- After them: wire runner + compose + make into bbb_kit.exe (layout_python goes), models.bin and data as resources,
  the window calls runner_main in-process, then the exe runs with no Python.

## Done (2026-11-20)
- Every builder group proven: features/details/styles2 361,906 cases, sd_abc 59,646, scenes/tiers 113,219, sd_rx 58,264,
  new styles 45k+, sd_d/sd_e/sd_r + the 303 model details 150,273 (0 differences each).
- Integrated (cport\stylefns.c, cport\pipeline.c): bbb_kit.exe makes every base in C, no python.exe, runs alone from an
  empty folder. 25 styles x 30/60/100/90x52 at scale 1 and 3: 391/400 STLs and 795/800 pictures identical; the rest is
  the Python kit's id() reuse (proven with work\cscene\integrate\idfix), except one.
- That one (ashveil_90x52mm_09) FIXED: meshglue fill_holes now drops zero-area new faces as trimesh's extend_faces does
  and keeps the part's cached normals. Proof: work\specks\prove_holes.py (T-junctions in the base) 120/120 same; the old
  code failed 12/60. provespecks.py 120/120 same. e2e ashveil 90x52 zr4tb 9,12: 0 problems.
- Left: --supports on (true 3D solids not in C), live view piece outlines (.shapes).
- --supports on DONE (cport\solids3d.c): makers 170,772 builds 0 differ; 112 jobs, 324 STLs byte-identical.
- Live view piece outlines DONE (scene.h sc_piece_hook, pipeline.c lv_piece: <note>.shapes + pieces count).
- Colour view (COLOUR switch on built cards, _3d_color.png): shot.c shot_render_tex, the 2D drawing projected from above.
- Known, copied from Python on purpose: ~7% of tube solids (supports on) drop as "solid N: TypeError" (math.pi * m / m
  a hair above pi -> sin < 0 -> complex ** 0.7). A fix changes the bases; balderding's call.
- Released as 2.0.0: bbb_kit.exe alone is the kit (no Python).
