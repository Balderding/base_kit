# Live build view: implementation plan

> For agentic workers: REQUIRED SUB-SKILL: superpowers:executing-plans (the user does not want agents spawned unasked). Steps use `- [ ]`.

**Goal:** the kit window shows, while bases build:
- the base taking shape;
- what is being done and how much is left;
- the cores and video card in use.

It does this in three views the user picks (Small, Live card, Big view). Every part can be switched on and off and is in Help.

**Architecture:**
- **Build side:** the Python build and `bbb_kit.exe --core-build` write a small progress note per base into `<kit>\out\.live\`.
- **Window side:** `bbb_kit.exe` polls that folder and reads Windows' CPU and GPU counters. It draws the chosen view. Reading the counters and notes lives in a new `gui/live.c`; drawing stays in `bbb_kit.c` with its drawing kit.

**Tech:** C (clang, Win32, GDI+, PDH, ntdll), Python 3.15.

**Spec:** `docs/superpowers/specs/2026-11-19-live-build-view-design.md`

## Global constraints

- Bases byte-identical with the view on or off.
- The display costs under 2% of build time. Notes are written at most every 100 ms plus at stage changes. The window draws at most ~10 frames a second.
- No red. Use teal, gold, navy and green from the existing palette, in both looks.
- The kit's test rule: one build test per change, plus compile and look pictures. No full theme runs.
- Never write the user's real name; no personal email address anywhere.
- No commit unless the user asks.

## Review focus

1. **Notes left behind** by a stopped or crashed build: the window must ignore notes older than its own build start, and gui_job clears `.live` at start.
2. **A PC without the GPU counter** (PDH missing / no `engtype_3D`): the meter hides and says "not available"; it never shows 0 as if measured.
3. **Oval bases at the smallest window size:** the wide box must not overflow the grid. Clamp the width to the grid and keep the ratio.
4. **12 bases in flight:** the Big view picks the oldest, moves on when it finishes, and returns to the grid when none are left.
5. **Threads off (Python field):** still shows a stage and steps, with no tiles from the exe.

---

### Task 1: PC meters (cores + video card) in the window

**Files:**
- Create: `gui/live.c`, `gui/live.h`.
- Modify: `gui/bbb_kit.c` (top bar strip, timer, Small view meters), `gui/build.cmd` (+ live.c, `-lpdh`).

**Interfaces:**
- Produces:
  - `int live_cpu(float *per_core, int cap)`: fills per-core busy 0..1 since the last call; returns the core count.
  - `float live_gpu(void)`: the 3D engine use 0..1, or −1 = not available.
  - Both are cheap; call at most twice a second.

**Steps:**
- [ ] Implement `live_cpu` with `NtQuerySystemInformation(SystemProcessorPerformanceInformation=8)` loaded from ntdll at run time (one entry per logical core of the current group; plus the other groups through `SystemProcessorPerformanceInformationEx`, if more than 64).
- [ ] Implement `live_gpu` with PDH `\GPU Engine(*engtype_3D)\Utilization Percentage` (wildcard, sum, cap 100), loaded from pdh.dll at run time.
- [ ] In the window: a meter timer (id 3, 500 ms) runs while busy, and also while idle when the core strip is on (cheap).
- [ ] Draw the core strip under the top bar: one thin bar per core, height = busy, teal; the GPU meter at its right in gold, with "%".
- [ ] Build; launch hidden (`--resizetest` with `BBB_LOOK_SAMPLES`); look at the picture.

### Task 2: progress notes from the build

**Files:**
- Create: `live.py` (kit root): the note writer.
- Modify: `mesher.py` (stages; 2D drawing right after layout), `gg3.py` (piece count hook), `cmesh.py` (pass `KIT_LIVE_FILE` to the exe), `cport/field.c` + `cport/core.c` (tile done marks), `tools/gui_job.py` (set `KIT_LIVE`, clear `.live` at start).

**Interfaces:**
- Produces: `live.note(**fields)` (throttled 100 ms unless `force=True`) and `live.done()` (removes the note).
- The note file is `%KIT_LIVE%\<SALT>_<n>.txt`, `key=value` lines:
  - `stage`, `t0`, `stage_t0`, `pieces`, `steps_done`, `steps_total`, `threads`, `oval` (`long x short` or empty), `mm`, `plan` (the path of the early 2D drawing), `tiles` (`;`-separated `x0,y0,x1,y1`), `tdone` (a string of `0`/`1`, one per tile), `bounds` (`x0,y0,x1,y1` of the base);
  - written atomically: write `.tmp`, then rename.
- The exe gets `KIT_LIVE_FILE` and writes its own `<note>.tiles` side file (`tiles=` and `tdone=` lines) at most every 100 ms from a lone writer thread. The window merges the two.

**Steps:**
- [ ] `live.py`: writer, a no-op when `KIT_LIVE` is unset.
- [ ] Stages in `mesher.build`: layout → field → mesh → checks/picture. Draw the 2D picture right after layout into `<note>_plan.png` (same draw2d), so the card has it early.
- [ ] gg3: a piece counter hook.
- [ ] field.c: tile done flags (an atomic array) + writer thread when `KIT_LIVE_FILE` is set; the Python field reports `steps_done` per step.
- [ ] Check: rebuild the 6mrbe #16 base three ways (`work/cutorder/rebuild.py`) with `KIT_LIVE` set. A == B byte-identical, and the same md5 as without `KIT_LIVE`.

### Task 3: the three views, time left, ovals, switches, Help

**Files:**
- Modify: `gui/bbb_kit.c`, `gui/live.c` (note reading, time-left history from earlier `*_report.csv`).

**Interfaces:**
- Produces: `int live_read(const WCHAR *dir, const char *seed, int n, LiveNote *out)`, with LiveNote holding the fields above.
- Produces: `double live_expect(const char *style, const char *size, int tier)`: the median of up to 10 earlier build secs; 0 = no history.

**Steps:**
- [ ] Note polling every 100 ms while building. Each building Cell gets a stage, a fraction, time left (never rising) and the plan image (loaded once).
- [ ] Small view: the stage line + time-left bar on the card; one CPU + one GPU meter in the top bar.
- [ ] Live card: the plan + tile overlay (still to do: dimmed; done: clear; teal outlines), with the stage line and bar.
- [ ] Big view: the oldest building cell large in the grid area, rows over time (per base, coloured by stage) below, the others in a narrow row. Shrink back within 300 ms when done; the next one takes over; grid when none are left; a click picks which one is shown.
- [ ] Rows over time also as a strip under the cards in Small / Live card when on.
- [ ] Ovals: the card picture box uses the long:short ratio (a wider card), clamped to the grid; the oval's 3D picture is drawn wider in gui_job's `with_figure`.
- [ ] Side panel LIVE VIEW section: 3 chips (Small / Live card / Big view) + 5 toggles (Core bars, Video card meter, Base taking shape, Time left, Rows over time); saved in the ini; hover texts.
- [ ] Help: a "WATCHING IT BUILD" paragraph: each view and each toggle.
- [ ] Check: build, then a hidden quick test (`--quicktest` with a 75x42 oval and a round one), with pictures of each view in both looks.
