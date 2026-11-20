# Live build view: design

2026-11-19. For: the kit window (gui/bbb_kit.c) and the build (run_set.py, mesher.py, gg3.py, cport/field.c).

## Why

The user wants to see the work being done while bases build:
- the base itself taking shape;
- how much is left;
- what is happening now;
- how many cores and how much of the video card are in use.

Today a building card only shows a spinner and "building ...". Behind it the user has the CPU threads and Video card switches (1.5.0) and wants to see them working. The research (below) says that a long wait with real progress shown feels shorter than a spinner, and that plain words about what is happening help most.

## Goals

- **Three views, all built, picked in the window:**
  - **Small:** a stage line and a time-left bar on each card, and one CPU and one GPU meter in the top bar.
  - **Live card:** the card being built shows the base taking shape, with a stage line, a time-left bar, and a core strip and GPU meter under the top bar.
  - **Big view:** the base being built fills the main area with the same live picture, plus rows over time below it. When that base is done it shrinks back into its card.
- **Each part has its own on/off switch:**
  - core bars;
  - video card meter;
  - the base taking shape (tile fill / layout growing);
  - time left;
  - the rows over time.
- **An oval base gets a wide rectangular box**, with its own proportions, instead of a square card. This applies to every view and to built cards as well.
- **Help covers it all:** every view and every switch has an entry on the Help page, and a hover text in the window.
- **The numbers are true:**
  - stage and progress come from the build itself;
  - cores and GPU come from Windows' own counters (what Task Manager reads);
  - time left is worked out from real timings and is never faked.

## Non-goals

- No change to what gets built. The bases must be byte-identical with the view on or off.
- No new installs, libraries or services.

## Anti-goals (a failure even if it works)

- The display slows the builds down noticeably. Budget: under 2% of build time; drawing is capped at about 10 frames a second.
- A bar that jumps backwards, or a time left that is a guess dressed up as a fact. When there is no basis yet, it says "estimating ...".
- Red anywhere (the user finds red hard to see). Use teal, gold, navy and green, in the existing palette and both looks (smooth and pixel).
- The big view staying up after its base is done.

## Constraints

- Every build-stage message travels from the worker processes to the window without blocking the build. Workers are the persistent `mesher.py serve` processes; the field and mesh run inside `bbb_kit.exe --core-build`, called by the worker.
- With CPU threads off the build is the proven one-core path. Progress reports must not change its results.
- Everything goes in the existing single exe (bbb_kit.exe, clang, Win32 + GDI+). The UI follows the existing look switches (Pixel, Textures, Pixel font).

## Approaches considered

- **A. Live card.** The live picture inside the building card, plus a core and GPU strip under the top bar. Clear, and takes no room.
- **B. Big view.** The live picture large in the main area, with rows over time. Shows the most, but takes over the window while building.
- **C. Small.** Text, a bar and two meters. Cheap, but does not show the base.

**The user picked all three**, as a choice in the window. The big view must go back down when its base is done. Ovals get a rectangular box.

## How it works

### 1. Progress notes (build side)

Each base being built writes a small text file, `<out_folder>\.live\<base name>.txt`, rewritten in place:
- at most every 100 ms;
- and at every stage change.

The window polls the folder. A file is removed when its base is done. A file is the simplest channel that crosses the two process hops (run_set → worker → bbb_kit.exe) without touching the existing JSON-lines pipe. The folder path comes in env `KIT_LIVE` (set by run_set from the out folder).

**Content (key=value lines):**
- `stage=`: one of
  - `layout` (gg3.make laying out the scene);
  - `field` (cutting the height field);
  - `mesh`;
  - `checks` (print check, similarity);
  - `picture`;
- `t0=`: the start time;
- `stage_t0=`: when the current stage began;
- `layout_pieces=`: pieces placed so far;
- `steps_done=` / `steps_total=`: field steps;
- `tiles=`: `x0,y0,x1,y1` boxes of the 12 mm tiles, written once;
- `tile_done=`: a bitmask, or a list of done tile ids;
- `threads=`: how many threads this base's field is using;
- `oval=` / `mm=`: so the window knows the box shape.

**Who writes what:**
- **gg3.make (layout):** a hook in its piece loop bumps `layout_pieces`. Every ~0.5 s, if cheap enough, it also writes a small preview drawing (`.live\<name>_plan.png`, the same look as draw2d) so the window can show the layout growing. Measure draw2d's cost first. If it costs more than ~5% of layout time, draw the outlines in the window instead from a compact list of piece outlines.
- **mesher.build:** writes the stage changes. It draws the final 2D drawing right after layout (moved earlier, same file), so the live card has the plan the moment layout ends.
- **bbb_kit.exe --core-build (threads on):** field.c marks each tile done as its worker thread finishes it. A shared counter goes to a tiny writer that flushes at most every 100 ms; it never takes a lock the threads need.
- **Python field (threads off):** reports `steps_done` per step. The tile view then fills tiles in step order (step-major), so the picture is still honest about the order of the work.

### 2. PC counters (window side)

Read in the window about twice a second.

- **Cores:** NtQuerySystemInformation(SystemProcessorPerformanceInformation), one value per logical core, all processor groups. Cheap, and no PDH setup.
- **Video card:** PDH `\GPU Engine(*engtype_3D)\Utilization Percentage`, summed and capped at 100. If PDH or the counter is missing, the meter says "not available" and hides; it never shows 0 as if it were measured.
- Both meters are labelled as the whole PC's numbers. The base's own thread count is shown beside them ("this base: 36 threads").

### 3. Time left

- **Per base:** the expected total time comes from the last builds of the same style + size + detail, read from earlier `*_report.csv` "build secs" (the median of up to 10). Until there is history it shows "estimating ...".
- **Inside the field stage:** the share of tiles or steps done refines it. Field and mesh share = the steps fraction.
- **Never backwards:** the shown time left can only go down, or hold. If the estimate is exceeded it switches to "taking longer than usual, N s so far".
- **For the whole run:** the window's top bar keeps its bar, now driven by the sum of the per-base fractions instead of only finished bases.

### 4. The three views

- **Small:**
  - the card's caption line shows the stage in words ("cutting the height field 61/96 tiles");
  - a thin bar shows the time left;
  - the top bar gets one CPU meter (the average of all cores) and one GPU meter.
- **Live card (default):**
  - while building, the picture area shows the live plan with the tiles over it;
  - tiles still to do are dimmed, tiles being done are outlined in teal, done tiles are clear;
  - the stage line and time-left bar are under it;
  - under the top bar runs a strip of thin bars, one per logical core (36 here), plus the GPU meter;
  - when done, the 3D picture replaces it as today.
- **Big view:**
  - when a build starts, the oldest base still building fills the main area with the live picture, large;
  - below it are the rows over time: one row per base in flight, coloured by stage. This is a profiler-style timeline (my call: per base, not per thread, because per-thread rows of 12 workers × 36 threads would be unreadable; the core strip already shows the threads);
  - the other cards sit in a narrow row;
  - when the shown base is done it shrinks back into its card (a short animation, under 300 ms), and the next base still building takes its place;
  - when none are left the normal card grid returns;
  - clicking another building card shows that one instead.

### 5. Oval boxes

- An oval base's card picture box is a rectangle with the oval's long:short ratio, laid out wider. Cards in a row share a height; an oval card is wider.
- The 2D drawing, the 3D picture, the live plan and the big view all fit that box.
- The 3D picture of an oval is rendered at a wider aspect: shot() w:h set from the oval.

### 6. Switches and Help

- **A new LIVE VIEW section in the side panel:**
  - three chips: Small / Live card / Big view;
  - small toggles for: Core bars, Video card meter, Base taking shape, Time left, Rows over time.
- **Saved** in the ini, like the other switches.
- **Help:** the Help page gets a "Watching it build" part, one entry per view and per toggle, in plain words. Every chip and toggle gets a hover text.

## Research it draws on

- Long waits with determinate progress feel shorter; spinners only for short waits ([UX Tigers](https://www.uxtigers.com/post/progress-indicators), [Smart Interface Design Patterns](https://smart-interface-design-patterns.com/articles/designing-better-loading-progress-ux/)).
- Plain words beat animation ([Pencil & Paper](https://www.pencilandpaper.io/articles/ux-pattern-analysis-loading-feedback)).
- Tile/bucket display from renderers ([tiled rendering](https://en.wikipedia.org/wiki/Tiled_rendering)).
- Per-thread / per-task rows from profilers ([VS Concurrency Visualizer](https://learn.microsoft.com/en-us/visualstudio/profiling/threads-view-parallel-performance?view=vs-2022)).
- Never slow the work for the animation.

## Calls I made (the user can overrule)

- Progress files in `<out>\.live`, not a pipe or shared memory.
- Rows over time are per base, not per thread.
- Live card is the default view.
- Time left comes from the history of earlier report CSVs.
- The core and GPU numbers are the whole PC's (labelled so).
- The layout preview is a periodic drawing if it is cheap, otherwise outlines drawn by the window.
- The shrink-back animation is under 300 ms.
- The live view is for building only. "Show choices" (drawing the designs to pick from) keeps today's breathing cards plus the core and GPU strip.
- "Rows over time" also works in Small and Live card, as a short strip under the cards. In Big view it is under the big picture.

## Left to the builder

- Exact sizes, spacing and colours within the palette.
- Poll rates within the budgets above.
- The tile mask format.

## Checks

- **Bases unchanged:** one build test per change (the kit's rule). Field_check A==B still byte-identical with `KIT_LIVE` set.
- **Speed:** a temple 160 build with the view on vs off differs by under 2%.
- **Pictures:** each view, in both looks, with a round and an oval base, captured via the existing `BBB_LOOK_SAMPLES` / selftest picture path.
