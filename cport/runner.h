/* runner.h: the kit's job runner in C (tools\gui_job.py, tools\choices.py, run_set.py, live.py), so bbb_kit.exe runs a
   whole job without python.exe. The layout (gg3.make + gg.build_cmds + mesher's steps up to the height field) is the
   C kit too (pipeline.h): one helper process per worker, bbb_kit.exe --core-layout <mode> <style>, as the Python kit
   kept one layout process per worker. Only for debugging, BBB_LAYOUT_PYTHON=1 starts the old Python bridge
   (work\cscene\runner\layout_bridge.py, with BBB_PYTHON / BBB_BRIDGE) instead; it is off unless that is set.

   int runner_main(argc, argv, out, ctx)
       argv as tools\gui_job.py takes them, without the script name:
           choices <style> --size 60 --detail 4 --count 20 [--mini biped] [--supports on] [--scale 3] [--seed x] [--jobs n]
           build <style> --size 60 --picks x:16,x:17,y:36 [--mini biped] [--supports on] [--scale 3]
       every line it gives out is ONE JSON record, byte for byte what gui_job.py prints ({"ev": "seed", ...}); out(ctx,
       line) gets each line (UTF-8, no newline), or NULL = they go to stdout. Returns the exit code gui_job.py gives.
       --seed / --jobs are choices.py's own options (gui_job.py does not pass them on: proof runs use them).
   Environment: BBB_KIT = the kit folder (else the folder of the exe / dll), BBB_CORE_EXE = the exe for the helpers
   (else this exe, or bbb_kit.exe beside the dll). */
#ifndef RUNNER_H
#include <stddef.h>
#define RUNNER_H
typedef void (*runner_out_fn)(void *ctx, const char *line);
int runner_main(int argc, char **argv, runner_out_fn out, void *ctx);

/* the kit's own data by name ("settings.json", "scale\\real.json", ...): a file of that name beside the exe wins, else
   the RCDATA resource built into the exe (name upper case, '\\' '/' '.' as '_': SETTINGS_JSON), else NULL. malloc'd,
   0-terminated; *len its size. Settings: built-in defaults (runner.c DEFAULT_SETTINGS) + a settings.json beside the exe. */
/* json.dumps(s) of a UTF-8 string into buf (ensure_ascii, as Python's json): length, or -1 if it does not fit */
int runner_json_str(const char *s, char *buf, int cap);
/* repr(x) of a float, as Python prints it (shortest round trip): length */
int runner_float_repr(double x, char *buf, int cap);

/* kit_settings.CFG as runner.c reads it (built-in defaults + a settings.json beside the exe), for the other C parts
   (pipeline.c): call runner_cfg_init() first. text: str() of a string or number value as Python prints it (NULL =
   not there). has: the key is in the settings. threads: cmesh.threads(). kit: the kit folder (the exe's own). */
void runner_cfg_init(void);
double runner_cfg_num(const char *k, double d);
int runner_cfg_bool(const char *k, int d);
const char *runner_cfg_str(const char *k, const char *d);
const char *runner_cfg_text(const char *k);
int runner_cfg_has(const char *k);
int runner_threads(void);
const char *runner_kit(void);
/* the window's Stop: every helper process the runner starts goes into this job object (NULL = none), and a set
   runner_stop starts no new base */
extern void *runner_job;
extern volatile long runner_stop;
#endif
