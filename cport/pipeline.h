/* pipeline.h: one base end to end in C, in the process that makes it (pipeline.c; 2026-11-20, agent "integrate"):
   what work\cscene\runner\layout_bridge.py did with the Python kit, now with the C one:
     the kit's module set-up as `import gg3` does it for this process's environment (BASE_MM, KIT_SCALE, KIT_FINAL_MM,
     KIT_OVAL, KIT_MINI, KIT_SUPPORTS, SALT, RESEED, settings.json): scene_config, pl_config, solids_init, kinds_init,
     the builder table, the style lists (stylefns.h), compose, make
     layout_choice: gg3.make + gg.build_cmds, the scene for the drawing ("BBD1", draw2d.h)
     layout_build: gg3.make, live.start, then meshglue's mg_build_input (sig, smooth_isolated, build_cmds, scale_plan,
                   rescale, field_ops, the standing spots, pack_ops, want_mm), gg3.scene_report, kitlog's error list,
                   the "BBF1" field input and the "BBD1" drawing.
   The kit's C parts keep module state as the Python modules do (kind sets, caches, counters), so one process makes
   one base at a time and keeps its state from base to base, as one Python layout process did. runner.c starts one
   such process per worker: bbb_kit.exe --core-layout <choices|build> <style>, which answers on stdout exactly as the
   Python bridge did ("BBL1" / "BBL2" records, see layout_bridge.py), one request line on stdin per base.
   GEOS is the one linked into the exe (geos_link.c); the model packs come from the exe's MODELS_BIN resource. */
#ifndef PIPELINE_H
#define PIPELINE_H
#include <stddef.h>

/* bbb_kit.exe --core-layout <mode> <style>: serve requests until stdin closes. Returns the exit code. */
int pipe_serve(const char *mode, const char *style);

/* the same in-process, one request at a time (the proofs: a DLL): mode "choices" / "build", req = the request's JSON
   line. *out = the answer record exactly as pipe_serve writes it (malloc'd, *n bytes). 0 = answered. The first call
   sets the process up from the environment as it is then. */
int pipe_request(const char *mode, const char *style, const char *req, unsigned char **out, size_t *n);
#endif
