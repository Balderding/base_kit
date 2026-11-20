/* runner.c: tools\gui_job.py + tools\choices.py (+ run_set.py, live.py: see the build job) in C. Each part names the
   Python it copies. The layout is the C kit (pipeline.c) in one helper process per worker: this exe with
   --core-layout (the old Python bridge only with BBB_LAYOUT_PYTHON=1, for debugging).
   Plain C, Windows (CreateProcess for the helpers, threads for the workers). */
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <math.h>
#include <time.h>
#include <ctype.h>
#include <stdarg.h>
#include "runner.h"
#include "draw2d.h"
#include "pyx.h"
#include "meshglue.h"

/* ================================================================ small strings */
typedef struct { char *s; size_t n, cap; } Sb;
static void sb_add(Sb *b, const char *p, size_t n) {
    if (b->n + n + 1 > b->cap) { b->cap = (b->n + n + 1) * 2 + 64; b->s = realloc(b->s, b->cap); }
    memcpy(b->s + b->n, p, n); b->n += n; b->s[b->n] = 0;
}
static void sb_puts(Sb *b, const char *p) { sb_add(b, p, strlen(p)); }
static void sb_printf(Sb *b, const char *fmt, ...) {
    char t[2048]; va_list ap; va_start(ap, fmt); int n = vsnprintf(t, sizeof t, fmt, ap); va_end(ap);
    if (n >= (int)sizeof t) { char *big = malloc(n + 1); va_start(ap, fmt); vsnprintf(big, n + 1, fmt, ap); va_end(ap); sb_add(b, big, n); free(big); }
    else if (n > 0) sb_add(b, t, n);
}
static void dbg(const char *fmt, ...) {          /* RUNNER_DEBUG=1: progress on stderr */
    static int on = -1; if (on < 0) on = getenv("RUNNER_DEBUG") != NULL; if (!on) return;
    char t[1024]; va_list ap; va_start(ap, fmt); int n = vsnprintf(t, sizeof t, fmt, ap); va_end(ap); DWORD put; WriteFile(GetStdHandle(STD_ERROR_HANDLE), t, n > 0 ? (DWORD)n : 0, &put, NULL);
}
static char *xstrdup(const char *s) { size_t n = strlen(s); char *d = malloc(n + 1); memcpy(d, s, n + 1); return d; }

/* ================================================================ Python's json.dumps and repr(float) */
/* json.dumps of a str (ensure_ascii=True): \" \\ \n \r \t \b \f, other controls \u00XX, non-ASCII \uXXXX (surrogate pairs) */
static void json_str(Sb *b, const char *s) {
    const unsigned char *p = (const unsigned char *)s; char t[16];
    sb_add(b, "\"", 1);
    while (*p) {
        uint32_t c = *p; int n = 1;
        if (c >= 0xf0 && p[1] && p[2] && p[3]) { c = ((c & 7) << 18) | ((p[1] & 63) << 12) | ((p[2] & 63) << 6) | (p[3] & 63); n = 4; }
        else if (c >= 0xe0 && p[1] && p[2]) { c = ((c & 15) << 12) | ((p[1] & 63) << 6) | (p[2] & 63); n = 3; }
        else if (c >= 0xc0 && p[1]) { c = ((c & 31) << 6) | (p[1] & 63); n = 2; }
        p += n;
        if (c == '"') sb_puts(b, "\\\""); else if (c == '\\') sb_puts(b, "\\\\"); else if (c == '\n') sb_puts(b, "\\n");
        else if (c == '\r') sb_puts(b, "\\r"); else if (c == '\t') sb_puts(b, "\\t"); else if (c == '\b') sb_puts(b, "\\b");
        else if (c == '\f') sb_puts(b, "\\f");
        else if (c < 0x20 || (c > 0x7e && c < 0x10000)) { snprintf(t, sizeof t, "\\u%04x", c); sb_puts(b, t); }          /* json's ESCAPE_ASCII: all but ' '..'~' */
        else if (c >= 0x10000) { uint32_t v = c - 0x10000; snprintf(t, sizeof t, "\\u%04x\\u%04x", 0xd800 + (v >> 10), 0xdc00 + (v & 0x3ff)); sb_puts(b, t); }
        else { char ch = (char)c; sb_add(b, &ch, 1); }
    }
    sb_add(b, "\"", 1);
}
int runner_json_str(const char *s, char *buf, int cap) { Sb b = { 0 }; json_str(&b, s); int n = (int)b.n; if (n >= cap) n = -1; else memcpy(buf, b.s, b.n + 1); free(b.s); return n; }

/* float.__repr__ (short repr: the shortest digits that read back the same; exponent form below 1e-4 and from 1e16) */
int runner_float_repr(double x, char *out, int cap) {
    char t[64], dig[32]; int nd = 0, e = 0;
    if (isnan(x)) return snprintf(out, cap, "NaN");          /* json.dumps spelling */
    if (isinf(x)) return snprintf(out, cap, x > 0 ? "Infinity" : "-Infinity");
    int neg = signbit(x) != 0; double ax = fabs(x);
    if (ax == 0) return snprintf(out, cap, neg ? "-0.0" : "0.0");
    for (int p = 1; p <= 17; p++) { snprintf(t, sizeof t, "%.*e", p - 1, ax); if (strtod(t, NULL) == ax) break; }
    for (char *c = t; *c && *c != 'e'; c++) if (*c >= '0' && *c <= '9') dig[nd++] = *c;
    e = atoi(strchr(t, 'e') + 1);
    while (nd > 1 && dig[nd - 1] == '0') nd--;
    dig[nd] = 0;
    Sb b = { 0 }; if (neg) sb_puts(&b, "-");
    int decpt = e + 1;
    if (e >= -5 && e < 16) {          /* repr: fixed when -4 <= exponent < 16 (Python's float_repr_style 'short', 'r' format) */
        if (e < -4) goto sci;
        if (decpt <= 0) { sb_puts(&b, "0."); for (int i = 0; i < -decpt; i++) sb_puts(&b, "0"); sb_add(&b, dig, nd); }
        else if (decpt >= nd) { sb_add(&b, dig, nd); for (int i = nd; i < decpt; i++) sb_puts(&b, "0"); sb_puts(&b, ".0"); }
        else { sb_add(&b, dig, decpt); sb_puts(&b, "."); sb_add(&b, dig + decpt, nd - decpt); }
    } else {
sci:    sb_add(&b, dig, 1); if (nd > 1) { sb_puts(&b, "."); sb_add(&b, dig + 1, nd - 1); }
        sb_printf(&b, "e%c%02d", e < 0 ? '-' : '+', e < 0 ? -e : e);
    }
    int n = snprintf(out, cap, "%s", b.s); free(b.s); return n;
}
static void json_float(Sb *b, double x) { char t[64]; runner_float_repr(x, t, sizeof t); sb_puts(b, t); }
static void fmt_g(char *t, size_t n, double v) { snprintf(t, n, "%g", v); }          /* f"{v:g}" */

/* ================================================================ a small JSON reader (settings.json, the layout's answers) */
typedef struct JV { int t; char *s; double d; int n; struct JV *a; char **k; } JV;          /* t: 0 null 1 false 2 true 3 number 4 string 5 array 6 object */
static const char *jskip(const char *p) { while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r') p++; return p; }
static void put_utf8(Sb *b, uint32_t c) {
    char u[4];
    if (c < 0x80) { u[0] = (char)c; sb_add(b, u, 1); }
    else if (c < 0x800) { u[0] = (char)(0xc0 | (c >> 6)); u[1] = (char)(0x80 | (c & 63)); sb_add(b, u, 2); }
    else if (c < 0x10000) { u[0] = (char)(0xe0 | (c >> 12)); u[1] = (char)(0x80 | ((c >> 6) & 63)); u[2] = (char)(0x80 | (c & 63)); sb_add(b, u, 3); }
    else { u[0] = (char)(0xf0 | (c >> 18)); u[1] = (char)(0x80 | ((c >> 12) & 63)); u[2] = (char)(0x80 | ((c >> 6) & 63)); u[3] = (char)(0x80 | (c & 63)); sb_add(b, u, 4); }
}
static const char *jstring(const char *p, char **out) {
    Sb b = { 0 }; sb_add(&b, "", 0); p++;
    while (*p && *p != '"') {
        if (*p == '\\') {
            p++;
            if (*p == 'u') {
                uint32_t c = (uint32_t)strtoul((char[5]){ p[1], p[2], p[3], p[4], 0 }, NULL, 16); p += 5;
                if (c >= 0xd800 && c < 0xdc00 && p[0] == '\\' && p[1] == 'u') { uint32_t d = (uint32_t)strtoul((char[5]){ p[2], p[3], p[4], p[5], 0 }, NULL, 16); c = 0x10000 + ((c - 0xd800) << 10) + (d - 0xdc00); p += 6; }
                put_utf8(&b, c); continue;
            }
            char c = *p == 'n' ? '\n' : *p == 't' ? '\t' : *p == 'r' ? '\r' : *p == 'b' ? '\b' : *p == 'f' ? '\f' : *p; sb_add(&b, &c, 1); p++;
        } else { sb_add(&b, p, 1); p++; }
    }
    *out = b.s; return *p ? p + 1 : p;
}
static const char *jparse(const char *p, JV *v) {
    memset(v, 0, sizeof *v); p = jskip(p);
    if (*p == '{' || *p == '[') {
        int obj = *p == '{'; v->t = obj ? 6 : 5; p = jskip(p + 1);
        while (*p && *p != (obj ? '}' : ']')) {
            v->a = realloc(v->a, sizeof(JV) * (v->n + 1)); if (obj) v->k = realloc(v->k, sizeof(char *) * (v->n + 1));
            if (obj) { p = jskip(p); p = jstring(p, &v->k[v->n]); p = jskip(p); if (*p == ':') p++; }
            p = jparse(p, &v->a[v->n]); v->n++; p = jskip(p); if (*p == ',') p = jskip(p + 1);
        }
        return *p ? p + 1 : p;
    }
    if (*p == '"') { v->t = 4; return jstring(p, &v->s); }
    if (!strncmp(p, "true", 4)) { v->t = 2; return p + 4; }
    if (!strncmp(p, "false", 5)) { v->t = 1; return p + 5; }
    if (!strncmp(p, "null", 4)) { v->t = 0; return p + 4; }
    const char *q = p; while (*q && strchr("+-0123456789.eEInfityNa", *q)) q++;
    v->t = 3; v->s = malloc(q - p + 1); memcpy(v->s, p, q - p); v->s[q - p] = 0; v->d = strtod(v->s, NULL); return q;
}
static void jfree(JV *v) { for (int i = 0; i < v->n; i++) { jfree(&v->a[i]); if (v->k) free(v->k[i]); } free(v->a); free(v->k); free(v->s); memset(v, 0, sizeof *v); }
static JV *jget(JV *o, const char *key) { if (!o || o->t != 6) return NULL; for (int i = o->n - 1; i >= 0; i--) if (!strcmp(o->k[i], key)) return &o->a[i]; return NULL; }          /* json.load keeps the last of a repeated key */

/* ================================================================ the kit folder, settings.json (kit_settings.py) */
static char KIT[MAX_PATH * 3];
static JV CFG;          /* settings.json as read; kit_settings._DEFAULTS are applied where read */
static void set_kit(void) {
    if (KIT[0]) return;
    const char *e = getenv("BBB_KIT");
    if (e && *e) { snprintf(KIT, sizeof KIT, "%s", e); }
    else {
        HMODULE h = NULL; WCHAR w[MAX_PATH];
        GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, (LPCWSTR)(void *)set_kit, &h);
        GetModuleFileNameW(h, w, MAX_PATH); WCHAR *s = wcsrchr(w, L'\\'); if (s) *s = 0;
        WideCharToMultiByte(CP_UTF8, 0, w, -1, KIT, sizeof KIT, NULL, NULL);
    }
    size_t n = strlen(KIT); while (n > 3 && (KIT[n - 1] == '\\' || KIT[n - 1] == '/')) KIT[--n] = 0;
}
static char *read_file(const char *path, size_t *len) {
    WCHAR w[MAX_PATH * 2]; MultiByteToWideChar(CP_UTF8, 0, path, -1, w, MAX_PATH * 2);
    FILE *f = _wfopen(w, L"rb"); if (!f) return NULL;
    fseek(f, 0, SEEK_END); long n = ftell(f); fseek(f, 0, SEEK_SET);
    char *b = malloc(n + 1); size_t got = fread(b, 1, n, f); fclose(f); b[got] = 0; if (len) *len = got; return b;
}
/* The kit's own data, by name ("settings.json", "scale\\real.json", ...): a file of that name beside the exe wins (an
   override); else the copy built into the exe (an RCDATA resource of that name, upper case, '\\' and '.' as '_', e.g.
   SETTINGS_JSON); else NULL. *len = its size. The caller frees it. */
char *runner_data(const char *name, size_t *len) {
    char p[MAX_PATH * 3]; snprintf(p, sizeof p, "%s\\%s", KIT, name);
    char *t = read_file(p, len); if (t) return t;
    char rn[128]; int j = 0;
    for (const char *c = name; *c && j < 120; c++) rn[j++] = (*c == '\\' || *c == '/' || *c == '.') ? '_' : (char)toupper((unsigned char)*c);
    rn[j] = 0;
    HMODULE h = NULL; GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, (LPCWSTR)(void *)runner_data, &h);
    HRSRC r = FindResourceA(h, rn, MAKEINTRESOURCEA(10));          /* RT_RCDATA */
    if (!r) return NULL;
    HGLOBAL g = LoadResource(h, r); DWORD n = SizeofResource(h, r); const void *d = g ? LockResource(g) : NULL;
    if (!d) return NULL;
    t = malloc(n + 1); memcpy(t, d, n); t[n] = 0; if (len) *len = n; return t;
}
/* settings: kit_settings._DEFAULTS + the kit's settings.json of 2026-11-19, built in; a settings.json beside the exe
   overrides key by key (kit_settings.load: cfg.update(json.load(f))) */
static const char DEFAULT_SETTINGS[] =
    "{\"solidcraft_cli\": \"solidcraft-cli\", \"out_folder\": \"out\", \"live\": false, \"always_new\": true, \"slow_step_secs\": 60,"
    " \"step_limit_secs\": 15, \"step_limit_per_command_secs\": 1.5, \"base_limit_secs\": 180, \"batch_limit_mins\": 300, \"workers\": \"auto\","
    " \"stop_if_fail_fraction\": 0.25, \"max_dropped_pieces\": 4, \"sketch_tolerance_mm\": 0.03, \"touching_tolerance_mm\": 0.005,"
    " \"true_circle_rim\": true, \"allow_supports\": false, \"trim_limit_secs\": 10, \"base_first\": true, \"chamfer_budget_secs\": 45,"
    " \"chamfer_rocks\": true, \"chamfer_grates\": true, \"max_detail_height_mm\": 0, \"random_mix\": true, \"extra_details\": true,"
    " \"max_attempts\": 4, \"merge_joins\": true, \"group_pieces\": true, \"min_difference_pct\": 15, \"compare_last\": 10, \"merge_chunk\": 12,"
    " \"join_on_parent\": true, \"engine\": \"mesh\", \"mesh_keep_running\": true, \"big_base_height_factor\": 1.5, \"scenes\": true,"
    " \"scenery_scale\": 1, \"drawing_2d\": true, \"models_download\": true, \"no_bare_ground\": true, \"style_details\": true,"
    " \"style_details_amount\": 0.5, \"objects_amount\": 1, \"cpu_threads\": true, \"video_card\": true, \"max_slenderness\": 3.0, \"miniature\": \"biped\"}";
static JV CFG_DEF;
static void load_cfg(void) {
    static int done; if (done) return; done = 1;
    jparse(DEFAULT_SETTINGS, &CFG_DEF);
    char p[MAX_PATH * 3]; snprintf(p, sizeof p, "%s\\settings.json", KIT);
    char *t = read_file(p, NULL);          /* only beside the exe: a built-in copy is DEFAULT_SETTINGS */
    if (t) { const char *s = t; if ((unsigned char)s[0] == 0xef) s += 3; jparse(s, &CFG); free(t); }
}
static JV *cfg_get(const char *k) { JV *v = jget(&CFG, k); return v ? v : jget(&CFG_DEF, k); }
static double cfg_num(const char *k, double d) { JV *v = cfg_get(k); return v && v->t == 3 ? v->d : v && (v->t == 1 || v->t == 2) ? v->t == 2 : d; }
static int cfg_bool(const char *k, int d) { JV *v = cfg_get(k); if (!v) return d; if (v->t == 3) return v->d != 0; if (v->t == 4) return v->s[0] != 0; return v->t == 2; }
static const char *cfg_str(const char *k, const char *d) { JV *v = cfg_get(k); return v && v->t == 4 ? v->s : d; }
/* run_set.py --supports on|off: CFG["allow_supports"] = a.supports == "on" for this run's checks (-1: not given, settings.json decides) */
static int SUP_CLI = -1;
static int allow_supports(void) { return SUP_CLI >= 0 ? SUP_CLI : cfg_bool("allow_supports", 0); }

/* the same for the other C parts (runner.h) */
void runner_cfg_init(void) { set_kit(); load_cfg(); }
double runner_cfg_num(const char *k, double d) { runner_cfg_init(); return cfg_num(k, d); }
int runner_cfg_bool(const char *k, int d) { runner_cfg_init(); return cfg_bool(k, d); }
const char *runner_cfg_str(const char *k, const char *d) { runner_cfg_init(); return cfg_str(k, d); }
int runner_cfg_has(const char *k) { runner_cfg_init(); return cfg_get(k) != NULL; }
const char *runner_cfg_text(const char *k) {          /* str(CFG[k]) for a str or a number (the JSON number's own text) */
    JV *v; runner_cfg_init(); v = cfg_get(k);
    if (!v) return NULL;
    if (v->t == 3 || v->t == 4) return v->s;
    return v->t == 2 ? "True" : v->t == 1 ? "False" : "None";
}
const char *runner_kit(void) { set_kit(); return KIT; }
void *runner_job;
volatile long runner_stop;

/* kit_settings.figure_scale */
static double figure_scale(double size_mm, double scale) { return size_mm < cfg_num("figure_scale_from_mm", 50) - 1e-6 ? 1.0 : scale; }

/* ================================================================ files and folders */
static int file_exists(const char *p);
static const char *out_root(void) {          /* KIT\out (BBB_OUT_ROOT: somewhere else, for proof runs) */
    static char o[MAX_PATH * 3]; const char *e = getenv("BBB_OUT_ROOT");
    if (e && *e) snprintf(o, sizeof o, "%s", e); else snprintf(o, sizeof o, "%s\\out", KIT);
    return o;
}
static void wpath(const char *p, WCHAR *w, int cap) { MultiByteToWideChar(CP_UTF8, 0, p, -1, w, cap); }
static void make_dirs(const char *p) {          /* os.makedirs(p, exist_ok=True) */
    char t[MAX_PATH * 3]; snprintf(t, sizeof t, "%s", p); WCHAR w[MAX_PATH * 2];
    for (char *c = t + 3; *c; c++) if (*c == '\\' || *c == '/') { char s = *c; *c = 0; wpath(t, w, MAX_PATH * 2); CreateDirectoryW(w, NULL); *c = s; }
    wpath(t, w, MAX_PATH * 2); CreateDirectoryW(w, NULL);
}
static void abs_path(const char *p, char *out, int cap) {          /* os.path.abspath */
    WCHAR w[MAX_PATH * 2], o[MAX_PATH * 2]; wpath(p, w, MAX_PATH * 2); GetFullPathNameW(w, MAX_PATH * 2, o, NULL);
    WideCharToMultiByte(CP_UTF8, 0, o, -1, out, cap, NULL, NULL);
}
static void strftime_now(char *out, size_t n, const char *fmt) { time_t t = time(NULL); struct tm lt; localtime_s(&lt, &t); strftime(out, n, fmt, &lt); }

/* ================================================================ the event lines (gui_job.ev) */
static runner_out_fn OUTF; static void *OUTC; static CRITICAL_SECTION OUTL; static int OUTL_ready;
static void emit(Sb *line) {
    EnterCriticalSection(&OUTL);
    if (OUTF) OUTF(OUTC, line->s);
    else { DWORD put; HANDLE h = GetStdHandle(STD_OUTPUT_HANDLE); WriteFile(h, line->s, (DWORD)line->n, &put, NULL); WriteFile(h, "\n", 1, &put, NULL); }
    LeaveCriticalSection(&OUTL);
    free(line->s); line->s = NULL; line->n = line->cap = 0;
}
/* an event, keys and values given as a format: 's' string, 'i' int, 'f' float, 'b' bool ("true"/"false") */
static void ev(const char *kind, const char *fmt, ...) {
    Sb b = { 0 }; va_list ap; va_start(ap, fmt);
    sb_puts(&b, "{\"ev\": "); json_str(&b, kind);
    for (const char *f = fmt; *f; f++) {
        const char *key = va_arg(ap, const char *); sb_puts(&b, ", "); json_str(&b, key); sb_puts(&b, ": ");
        if (*f == 's') json_str(&b, va_arg(ap, const char *));
        else if (*f == 'i') sb_printf(&b, "%d", va_arg(ap, int));
        else if (*f == 'f') json_float(&b, va_arg(ap, double));
        else if (*f == 'b') sb_puts(&b, va_arg(ap, int) ? "true" : "false");
    }
    va_end(ap); sb_puts(&b, "}"); emit(&b);
}

/* ================================================================ run_set.plan, sizes, seeds */
typedef struct { int n, tier; } Pick;
static int plan(int count, int detail, Pick *out) {          /* run_set.plan: detail 1-4, or 0 = an even mix */
    for (int i = 0; i < count; i++) {
        int tier = detail ? detail - 1 : i % 4, m = detail ? i : i / 4;
        out[i].n = 20 * (m / 5) + tier * 5 + m % 5 + 1; out[i].tier = tier + 1;
    }
    return count;
}
static void new_seed(char out[6]) {          /* "".join(random.choice("abcdefghjkmnpqrstuvwxyz23456789") for _ in range(5)): any fresh seed */
    static const char al[] = "abcdefghjkmnpqrstuvwxyz23456789"; LARGE_INTEGER q; QueryPerformanceCounter(&q);
    uint64_t x = (uint64_t)q.QuadPart ^ ((uint64_t)GetCurrentProcessId() << 32) ^ (uint64_t)time(NULL) * 0x9e3779b97f4a7c15ULL;
    for (int i = 0; i < 5; i++) { x ^= x << 13; x ^= x >> 7; x ^= x << 17; out[i] = al[x % 31]; }
    out[5] = 0;
}
/* the size text as choices.py reads it: "60" round, "90x52" an oval (long x short); a == b is round */
static int read_size(const char *txt, double *a, double *b, int *oval) {
    char t[64]; int j = 0;
    for (const char *p = txt; *p && j < 60; p++) { char c = (char)tolower((unsigned char)*p); if (c == ' ') continue; if (c == '*') c = 'x'; t[j++] = c; }
    t[j] = 0; char *mm = strstr(t, "mm"); while (mm) { memmove(mm, mm + 2, strlen(mm + 2) + 1); mm = strstr(t, "mm"); }
    char *x = strchr(t, 'x'); *oval = 0;
    if (x) { *x = 0; double u = strtod(t, NULL), v = strtod(x + 1, NULL); *a = u > v ? u : v; *b = u > v ? v : u; *oval = *a != *b; }
    else { *a = strtod(t, NULL); *b = *a; }
    return 1;
}

/* ================================================================ THE LAYOUT HELPERS ========================================
   layout_python (the name stayed from when it was Python): gg3.make + gg.build_cmds (+ mesher's steps up to the height
   field for a build) for one base, in a helper process: bbb_kit.exe --core-layout <mode> <style> (cport\pipeline.c),
   one per worker, started with the job's environment and kept for its bases, as the Python kit kept one layout
   process per worker (the kit's module state then grows from base to base exactly as Python's did).
   BBB_LAYOUT_PYTHON=1 (debugging only, off unless set): work\cscene\runner\layout_bridge.py with python.exe instead.
   ========================================================================================================================= */
typedef struct { HANDLE proc, in_w, out_r; int dead; } Bridge;
static SRWLOCK SPAWN = SRWLOCK_INIT;          /* one child started at a time (the inheritable pipe ends of one must not go to another) */
static void core_exe(WCHAR *w);
static int find_python(WCHAR *py) {
    WCHAR e[MAX_PATH];
    if (GetEnvironmentVariableW(L"BBB_PYTHON", e, MAX_PATH) && GetFileAttributesW(e) != INVALID_FILE_ATTRIBUTES) { wcscpy(py, e); return 1; }
    return SearchPathW(NULL, L"python.exe", NULL, MAX_PATH, py, NULL) > 0;          /* no fixed install path: BBB_PYTHON or PATH */
}
typedef struct { const char *k, *v; } EnvSet;          /* v NULL: removed */
static WCHAR *env_block(const EnvSet *set, int n) {
    WCHAR *cur = GetEnvironmentStringsW(), *p = cur; size_t cap = 1 << 16, len = 0; WCHAR *out = malloc(cap * sizeof(WCHAR));
    #define PUTW(s, l) do { size_t _l = (l); if (len + _l + 2 > cap) { cap = (len + _l + 2) * 2; out = realloc(out, cap * sizeof(WCHAR)); } memcpy(out + len, s, _l * sizeof(WCHAR)); len += _l; } while (0)
    for (; *p; p += wcslen(p) + 1) {
        int skip = 0;
        for (int i = 0; i < n; i++) {
            WCHAR k[256]; MultiByteToWideChar(CP_UTF8, 0, set[i].k, -1, k, 256); size_t kl = wcslen(k);
            if (!_wcsnicmp(p, k, kl) && p[kl] == L'=') { skip = 1; break; }
        }
        if (!skip) { PUTW(p, wcslen(p) + 1); }
    }
    for (int i = 0; i < n; i++) if (set[i].v) {
        WCHAR kv[4096]; char t[4096]; snprintf(t, sizeof t, "%s=%s", set[i].k, set[i].v); MultiByteToWideChar(CP_UTF8, 0, t, -1, kv, 4096); PUTW(kv, wcslen(kv) + 1);
    }
    PUTW(L"", 1);
    FreeEnvironmentStringsW(cur);
    return out;
    #undef PUTW
}
static int bridge_start(Bridge *b, const char *mode, const char *style, const WCHAR *env) {
    memset(b, 0, sizeof *b); WCHAR py[MAX_PATH * 2], cmd[4096], bridge[MAX_PATH * 2], st[128], md[32], kit[MAX_PATH * 2];
    const char *dbgpy = getenv("BBB_LAYOUT_PYTHON");
    wpath(style, st, 128); wpath(mode, md, 32); wpath(KIT, kit, MAX_PATH * 2);
    if (dbgpy && !strcmp(dbgpy, "1")) {          /* debugging only: the old Python bridge */
        if (!find_python(py)) return 0;
        char bp[MAX_PATH * 3]; const char *be = getenv("BBB_BRIDGE");
        if (be && *be) snprintf(bp, sizeof bp, "%s", be); else snprintf(bp, sizeof bp, "%s\\layout_bridge.py", KIT);          /* BBB_BRIDGE, else beside the exe */
        wpath(bp, bridge, MAX_PATH * 2);
        swprintf(cmd, 4096, L"\"%ls\" -u \"%ls\" %ls %ls", py, bridge, md, st);
    } else { core_exe(py); swprintf(cmd, 4096, L"\"%ls\" --core-layout %ls %ls", py, md, st); }          /* the C kit: this exe */
    SECURITY_ATTRIBUTES sa = { sizeof sa, NULL, TRUE }; HANDLE in_r, out_w;
    AcquireSRWLockExclusive(&SPAWN);          /* no other thread's pipe ends may leak into this child (it would never see its stdin close) */
    if (!CreatePipe(&in_r, &b->in_w, &sa, 0) || !CreatePipe(&b->out_r, &out_w, &sa, 0)) { ReleaseSRWLockExclusive(&SPAWN); return 0; }
    SetHandleInformation(b->in_w, HANDLE_FLAG_INHERIT, 0); SetHandleInformation(b->out_r, HANDLE_FLAG_INHERIT, 0);
    HANDLE nul = CreateFileW(L"NUL", GENERIC_WRITE, FILE_SHARE_WRITE, &sa, OPEN_EXISTING, 0, NULL);          /* the helpers' stderr goes nowhere (choices.py: DEVNULL) */
    STARTUPINFOW si; memset(&si, 0, sizeof si); si.cb = sizeof si; si.dwFlags = STARTF_USESTDHANDLES; si.hStdInput = in_r; si.hStdOutput = out_w; si.hStdError = nul;
    PROCESS_INFORMATION pi;
    BOOL ok = CreateProcessW(NULL, cmd, NULL, NULL, TRUE, CREATE_NO_WINDOW | CREATE_UNICODE_ENVIRONMENT | CREATE_SUSPENDED, (LPVOID)env, kit, &si, &pi);
    CloseHandle(in_r); CloseHandle(out_w); CloseHandle(nul);
    ReleaseSRWLockExclusive(&SPAWN);
    if (!ok) { CloseHandle(b->in_w); CloseHandle(b->out_r); return 0; }
    if (runner_job) AssignProcessToJobObject((HANDLE)runner_job, pi.hProcess);          /* the window's Stop ends it */
    ResumeThread(pi.hThread); CloseHandle(pi.hThread); b->proc = pi.hProcess; return 1;
}
static int read_n(HANDLE h, void *dst, size_t n) { unsigned char *d = dst; DWORD got; while (n) { if (!ReadFile(h, d, (DWORD)(n > (1u << 30) ? (1u << 30) : n), &got, NULL) || !got) return 0; d += got; n -= got; } return 1; }
/* one base: the request line out, the answer (JSON head + blob) back. 0 = the helper is gone */
static int layout_python(Bridge *b, const char *req, JV *head, unsigned char **blob, size_t *blen) {
    DWORD put; *blob = NULL; *blen = 0;
    if (b->dead) return 0;
    if (!WriteFile(b->in_w, req, (DWORD)strlen(req), &put, NULL) || !WriteFile(b->in_w, "\n", 1, &put, NULL)) { b->dead = 1; return 0; }
    char mg[4]; uint32_t n;
    if (!read_n(b->out_r, mg, 4) || memcmp(mg, "BBL1", 4) || !read_n(b->out_r, &n, 4)) { b->dead = 1; return 0; }
    char *h = malloc(n + 1); if (!read_n(b->out_r, h, n)) { free(h); b->dead = 1; return 0; } h[n] = 0;
    jparse(h, head); free(h);
    if (!read_n(b->out_r, &n, 4)) { b->dead = 1; return 0; }
    *blob = malloc(n + 1); *blen = n;
    if (n && !read_n(b->out_r, *blob, n)) { b->dead = 1; return 0; }
    return 1;
}
static void bridge_stop(Bridge *b) {
    if (b->in_w) CloseHandle(b->in_w);
    if (b->proc) { if (WaitForSingleObject(b->proc, 20000) != WAIT_OBJECT_0) TerminateProcess(b->proc, 1); CloseHandle(b->proc); }
    if (b->out_r) CloseHandle(b->out_r);
    memset(b, 0, sizeof *b);
}
/* ================================================================ end of the Python layout ================================ */

/* ================================================================ argument helpers (gui_job.opt / choices.opt) */
typedef struct { int argc; char **argv; } Args;
static const char *opt(Args *a, const char *name, const char *d) {          /* the first --name value, taken out of the list */
    for (int i = 0; i + 1 < a->argc; i++) if (!strcmp(a->argv[i], name)) {
        const char *v = a->argv[i + 1]; memmove(a->argv + i, a->argv + i + 2, sizeof(char *) * (a->argc - i - 2)); a->argc -= 2; return v;
    }
    return d;
}

/* ================================================================ the choices job: gui_job.py choices + tools/choices.py */
typedef struct {
    const char *style, *seed, *outdir; int px; const Pick *pl; int npl, k, jobs; const WCHAR *env; volatile LONG *count;
} ChoiceWork;
static DWORD WINAPI choice_worker(void *arg) {          /* one choices.py helper: the bases pl[k::jobs], one after another, in one layout process */
    ChoiceWork *w = arg; Bridge br;
    if (!bridge_start(&br, "choices", w->style, w->env)) { dbg("worker %d: no helper\n", w->k); return 0; }
    dbg("worker %d: helper started\n", w->k);
    for (int i = w->k; i < w->npl; i += w->jobs) {
        if (runner_stop) break;          /* the window's Stop */
        int n = w->pl[i].n; char png[MAX_PATH * 3], req[MAX_PATH * 4];
        snprintf(png, sizeof png, "%s\\c_%s_%d.png", w->outdir, w->seed, n);
        Sb r = { 0 }; sb_printf(&r, "{\"n\": %d, \"px\": %d, \"png\": ", n, w->px); json_str(&r, png); sb_puts(&r, "}"); snprintf(req, sizeof req, "%s", r.s); free(r.s);
        JV head; unsigned char *blob; size_t bl;
        if (!layout_python(&br, req, &head, &blob, &bl)) { dbg("worker %d: helper gone\n", w->k); free(blob); break; }          /* the helper died: its other bases never come (as in Python) */
        dbg("worker %d: base %d laid out (%d bytes)\n", w->k, n, (int)bl);
        JV *ok = jget(&head, "ok");
        if (ok && ok->t == 3 && ok->d) {
            JV *th = jget(&head, "theme"), *pd = jget(&head, "py_drawn"); int drawn = pd && pd->t == 3 && pd->d;
            char err[256] = ""; int good = 1;
            if (!drawn) {
                D2Scene sc; double win[4];
                if (!d2_read(blob, bl, &sc)) { good = 0; snprintf(err, sizeof err, "the scene could not be read"); }
                else { good = d2_draw(&sc, png, w->px, 1, win, err, sizeof err); }
                d2_scene_free(&sc);
            }
            if (good) {
                ev("choice", "sissi", "seed", w->seed, "n", n, "img", png, "theme", th && th->s ? th->s : w->style, "tier", w->pl[i].tier); InterlockedIncrement(w->count);
                JV *er = jget(&head, "errors");          /* choices.py: what this choice caught (kitlog), one log line */
                if (er && er->t == 5 && er->n) { Sb t = { 0 }; sb_printf(&t, "choice %d: %d error(s) caught, first: %s (all of them in out/kit_errors.log)", n, er->n, er->a[0].s ? er->a[0].s : ""); ev("log", "s", "text", t.s); free(t.s); }
            }
            else { char why[300]; snprintf(why, sizeof why, "RuntimeError: %.120s", err); ev("skip", "is", "n", n, "why", why); }
        } else { JV *why = jget(&head, "why"); ev("skip", "is", "n", n, "why", why && why->s ? why->s : "?"); }
        jfree(&head); free(blob);
    }
    bridge_stop(&br);
    return 0;
}
static int job_choices(Args *a, const char *style) {
    const char *size = opt(a, "--size", "30"), *detail = opt(a, "--detail", "mix"), *count_s = opt(a, "--count", "20");
    const char *mini = opt(a, "--mini", ""), *sup = opt(a, "--supports", ""), *scale_s = opt(a, "--scale", ""), *seed_s = opt(a, "--seed", "");
    const char *jobs_s = opt(a, "--jobs", ""), *px_s = opt(a, "--px", "360");
    /* gui_job.models(): nothing to fetch, the model packs the kit uses are inside the exe (models.c, MODELS_BIN) */
    char ts[32], outdir[MAX_PATH * 3], od[MAX_PATH * 3]; strftime_now(ts, sizeof ts, "%Y%m%d_%H%M%S");
    snprintf(od, sizeof od, "%s\\choices\\%s", out_root(), ts); abs_path(od, outdir, sizeof outdir); make_dirs(outdir);
    /* tools/choices.py */
    int count = atoi(count_s), px = atoi(px_s); double A, B; int oval; read_size(size, &A, &B, &oval);
    double sc = figure_scale(A, scale_s[0] ? strtod(scale_s, NULL) : cfg_num("scenery_scale", 1.0));
    double lay = sc == 1.0 ? A : py_round(fmax(30.0, A / sc), 1);
    int ncpu = (int)GetActiveProcessorCount(ALL_PROCESSOR_GROUPS); int jobs = jobs_s[0] ? atoi(jobs_s) : (ncpu - 1 > 1 ? ncpu - 1 : 1);
    char seed[64]; if (seed_s[0]) snprintf(seed, sizeof seed, "%s", seed_s); else new_seed(seed);
    char bm[32], fm[32], sk[32], ov[64]; fmt_g(bm, sizeof bm, lay); fmt_g(fm, sizeof fm, A); fmt_g(sk, sizeof sk, sc);
    if (oval) { char x[32], y[32]; fmt_g(x, sizeof x, A); fmt_g(y, sizeof y, B); snprintf(ov, sizeof ov, "%sx%s", x, y); }
    EnvSet es[16]; int ne = 0;
    es[ne++] = (EnvSet){ "PYTHONWARNINGS", "ignore" }; es[ne++] = (EnvSet){ "PYTHONUNBUFFERED", "1" }; es[ne++] = (EnvSet){ "PYTHONIOENCODING", "utf-8" };
    es[ne++] = (EnvSet){ "SALT", seed }; es[ne++] = (EnvSet){ "RESEED", "" }; es[ne++] = (EnvSet){ "BASE_MM", bm }; es[ne++] = (EnvSet){ "KIT_QUIET", "1" };
    if (sc != 1.0) { es[ne++] = (EnvSet){ "KIT_FINAL_MM", fm }; es[ne++] = (EnvSet){ "KIT_SCALE", sk }; }
    es[ne++] = (EnvSet){ "KIT_OVAL", oval ? ov : NULL };
    if (mini[0]) es[ne++] = (EnvSet){ "KIT_MINI", mini };
    if (!strcmp(sup, "on") || !strcmp(sup, "off")) es[ne++] = (EnvSet){ "KIT_SUPPORTS", !strcmp(sup, "on") ? "1" : "0" };
    dbg("choices: out %s seed %s lay %s jobs %d\n", outdir, seed, bm, jobs);
    WCHAR *env = env_block(es, ne);
    dbg("env ok\n");
    Pick *pl = malloc(sizeof(Pick) * (count > 0 ? count : 1)); int npl = plan(count > 0 ? count : 0, strcmp(detail, "mix") ? atoi(detail) : 0, pl);
    ev("seed", "s", "seed", seed);
    volatile LONG made = 0;
    int nw = (jobs > 1 && npl > 1) ? (jobs < npl ? jobs : npl) : 1, step = (jobs > 1 && npl > 1) ? jobs : 1;
    ChoiceWork *ws = calloc(nw, sizeof *ws); HANDLE *th = calloc(nw, sizeof(HANDLE));
    for (int k = 0; k < nw; k++) {
        ws[k] = (ChoiceWork){ style, seed, outdir, px, pl, npl, k, step, env, &made };
        th[k] = CreateThread(NULL, 0, choice_worker, &ws[k], 0, NULL);
    }
    for (int k = 0; k < nw; k++) { WaitForSingleObject(th[k], INFINITE); CloseHandle(th[k]); }
    ev("done", "si", "job", "choices", "count", (int)made);
    free(ws); free(th); free(pl); free(env);
    return 0;
}

/* ================================================================ THE BUILD JOB ================================================ */
/* ---- the layout helper's build answer ("BBL2": head + several blobs), with the run_set base time limit ---- */
static int read_n_until(HANDLE h, void *dst, size_t n, HANDLE proc, ULONGLONG deadline) {
    unsigned char *d = dst; DWORD got, avail;
    while (n) {
        if (!PeekNamedPipe(h, NULL, 0, NULL, &avail, NULL)) return 0;
        if (!avail) { if (GetTickCount64() > deadline) return -1; if (WaitForSingleObject(proc, 0) == WAIT_OBJECT_0 && (!PeekNamedPipe(h, NULL, 0, NULL, &avail, NULL) || !avail)) return 0; Sleep(5); continue; }
        if (!ReadFile(h, d, avail < n ? avail : (DWORD)n, &got, NULL) || !got) return 0;
        d += got; n -= got;
    }
    return 1;
}
/* layout_python for a build (part of THE LAYOUT, STILL IN PYTHON above): 1 ok, 0 the helper died, -1 over the time limit */
static int layout_python_build(Bridge *b, const char *req, double limit_s, JV *head, unsigned char ***blobs, size_t **lens, int *nb) {
    DWORD put; *blobs = NULL; *lens = NULL; *nb = 0;
    if (b->dead) return 0;
    if (!WriteFile(b->in_w, req, (DWORD)strlen(req), &put, NULL) || !WriteFile(b->in_w, "\n", 1, &put, NULL)) { b->dead = 1; return 0; }
    ULONGLONG dl = GetTickCount64() + (ULONGLONG)(limit_s * 1000); char mg[4]; uint32_t n; int r;
    if ((r = read_n_until(b->out_r, mg, 4, b->proc, dl)) != 1 || memcmp(mg, "BBL2", 4) || (r = read_n_until(b->out_r, &n, 4, b->proc, dl)) != 1) { b->dead = 1; return r < 0 ? -1 : 0; }
    char *h = malloc(n + 1); if ((r = read_n_until(b->out_r, h, n, b->proc, dl)) != 1) { free(h); b->dead = 1; return r < 0 ? -1 : 0; } h[n] = 0;
    jparse(h, head); free(h);
    uint32_t k; if ((r = read_n_until(b->out_r, &k, 4, b->proc, dl)) != 1) { b->dead = 1; return r < 0 ? -1 : 0; }
    *blobs = calloc(k + 1, sizeof(unsigned char *)); *lens = calloc(k + 1, sizeof(size_t)); *nb = (int)k;
    for (uint32_t i = 0; i < k; i++) {
        if ((r = read_n_until(b->out_r, &n, 4, b->proc, dl)) != 1) { b->dead = 1; return r < 0 ? -1 : 0; }
        (*blobs)[i] = malloc(n + 1); (*lens)[i] = n;
        if (n && (r = read_n_until(b->out_r, (*blobs)[i], n, b->proc, dl)) != 1) { b->dead = 1; return r < 0 ? -1 : 0; }
    }
    return 1;
}
static void free_blobs(unsigned char **b, size_t *l, int n) { for (int i = 0; i < n; i++) free(b[i]); free(b); free(l); }

/* ---- the kit's C parts in a child bbb_kit.exe (as cmesh.build_c / cmesh.shot run them): --core-build, --core-shot ---- */
typedef struct { HANDLE h; const unsigned char *p; size_t n; } WrJob;
static DWORD WINAPI wr_thread(void *a) { WrJob *j = a; DWORD put; size_t off = 0; while (off < j->n) { DWORD c = (DWORD)(j->n - off > (1u << 20) ? (1u << 20) : j->n - off); if (!WriteFile(j->h, j->p + off, c, &put, NULL) || !put) break; off += put; } CloseHandle(j->h); return 0; }
typedef struct { HANDLE h; Sb s; } RdJob;
static DWORD WINAPI rd_thread(void *a) { RdJob *j = a; char b[1 << 16]; DWORD got; while (ReadFile(j->h, b, sizeof b, &got, NULL) && got) sb_add(&j->s, b, got); return 0; }
static void core_exe(WCHAR *w) {          /* BBB_CORE_EXE, else this exe when the runner is inside bbb_kit.exe, else bbb_kit.exe beside it */
    WCHAR e[MAX_PATH * 2];
    if (GetEnvironmentVariableW(L"BBB_CORE_EXE", e, MAX_PATH * 2)) { wcscpy(w, e); return; }
    HMODULE h = NULL; GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, (LPCWSTR)(void *)core_exe, &h);
    GetModuleFileNameW(h, w, MAX_PATH * 2); size_t n = wcslen(w);
    if (n > 4 && !_wcsicmp(w + n - 4, L".exe")) return;
    WCHAR k[MAX_PATH * 2]; wpath(KIT, k, MAX_PATH * 2); swprintf(w, MAX_PATH * 2, L"%ls\\bbb_kit.exe", k);
}
static int run_core(const char *arg, const unsigned char *in, size_t n, const EnvSet *es, int ne, Sb *out, Sb *errs) {
    WCHAR exe[MAX_PATH * 2], cmd[MAX_PATH * 3], wa[64]; core_exe(exe); wpath(arg, wa, 64); swprintf(cmd, MAX_PATH * 3, L"\"%ls\" %ls", exe, wa);
    WCHAR *env = env_block(es, ne);
    SECURITY_ATTRIBUTES sa = { sizeof sa, NULL, TRUE }; HANDLE ir, iw, or_, ow, er, ew;
    AcquireSRWLockExclusive(&SPAWN);
    CreatePipe(&ir, &iw, &sa, 0); CreatePipe(&or_, &ow, &sa, 0); CreatePipe(&er, &ew, &sa, 0);
    SetHandleInformation(iw, HANDLE_FLAG_INHERIT, 0); SetHandleInformation(or_, HANDLE_FLAG_INHERIT, 0); SetHandleInformation(er, HANDLE_FLAG_INHERIT, 0);
    STARTUPINFOW si; memset(&si, 0, sizeof si); si.cb = sizeof si; si.dwFlags = STARTF_USESTDHANDLES; si.hStdInput = ir; si.hStdOutput = ow; si.hStdError = ew;
    PROCESS_INFORMATION pi; BOOL ok = CreateProcessW(exe, cmd, NULL, NULL, TRUE, CREATE_NO_WINDOW | CREATE_UNICODE_ENVIRONMENT | CREATE_SUSPENDED, env, NULL, &si, &pi);
    CloseHandle(ir); CloseHandle(ow); CloseHandle(ew); free(env);
    ReleaseSRWLockExclusive(&SPAWN);
    if (!ok) { CloseHandle(iw); CloseHandle(or_); CloseHandle(er); sb_puts(errs, "bbb_kit.exe could not be started"); return -1; }
    if (runner_job) AssignProcessToJobObject((HANDLE)runner_job, pi.hProcess);
    ResumeThread(pi.hThread);
    WrJob wj = { iw, in, n }; RdJob ej = { er, { 0 } }; HANDLE t1 = CreateThread(NULL, 0, wr_thread, &wj, 0, NULL), t2 = CreateThread(NULL, 0, rd_thread, &ej, 0, NULL);
    RdJob oj = { or_, { 0 } }; rd_thread(&oj);
    WaitForSingleObject(t1, INFINITE); WaitForSingleObject(t2, INFINITE); CloseHandle(t1); CloseHandle(t2);
    WaitForSingleObject(pi.hProcess, 900000); DWORD code = 1; GetExitCodeProcess(pi.hProcess, &code);
    CloseHandle(pi.hProcess); CloseHandle(pi.hThread); CloseHandle(or_); CloseHandle(er);
    *out = oj.s; if (ej.s.s) { sb_add(errs, ej.s.s, ej.s.n); free(ej.s.s); }
    return (int)code;
}

/* ---- meshes: trimesh's merge_vertices, binary STL export / load, is_watertight, split, contains ---- */
typedef struct { int nv, nt; double *v; int *f; } Mesh;
static void mesh_free(Mesh *m) { free(m->v); free(m->f); memset(m, 0, sizeof *m); }
/* Trimesh.merge_vertices(): vertices equal after round(v * 1e8) (tol.merge) become one, the first referenced one kept */
static void mesh_merge(Mesh *m) {
    int nv = m->nv; size_t cap = 16; while (cap < (size_t)nv * 2) cap <<= 1;
    int *slot = malloc(sizeof(int) * cap); for (size_t i = 0; i < cap; i++) slot[i] = -1;
    int64_t *key = malloc(sizeof(int64_t) * 3 * (nv ? nv : 1)); int *rep = malloc(sizeof(int) * (nv ? nv : 1)), *newi = malloc(sizeof(int) * (nv ? nv : 1));
    unsigned char *ref = calloc(nv ? nv : 1, 1); for (int i = 0; i < 3 * m->nt; i++) ref[m->f[i]] = 1;
    int nu = 0; double *nvv = malloc(sizeof(double) * 3 * (nv ? nv : 1));
    for (int i = 0; i < nv; i++) {
        rep[i] = -1; if (!ref[i]) continue;
        for (int c = 0; c < 3; c++) key[3 * i + c] = (int64_t)rint(m->v[3 * i + c] * 100000000.0);
        uint64_t h = (uint64_t)key[3 * i] * 0x9e3779b97f4a7c15ULL ^ (uint64_t)key[3 * i + 1] * 0xc2b2ae3d27d4eb4fULL ^ (uint64_t)key[3 * i + 2] * 0x165667b19e3779f9ULL;
        size_t s = (size_t)(h ^ (h >> 29)) & (cap - 1);
        while (slot[s] >= 0) { int j = slot[s]; if (key[3 * j] == key[3 * i] && key[3 * j + 1] == key[3 * i + 1] && key[3 * j + 2] == key[3 * i + 2]) break; s = (s + 1) & (cap - 1); }
        if (slot[s] < 0) { slot[s] = i; newi[i] = nu; memcpy(nvv + 3 * nu, m->v + 3 * i, 24); nu++; rep[i] = i; }
        else { rep[i] = slot[s]; newi[i] = newi[slot[s]]; }
    }
    for (int i = 0; i < 3 * m->nt; i++) m->f[i] = newi[m->f[i]];
    free(m->v); m->v = nvv; m->nv = nu;
    free(slot); free(key); free(rep); free(newi); free(ref);
}
/* trimesh.exchange.stl.export_stl: 80 zero bytes, u32 count, per face: the unit normal (triangles.normals: np.cross of
   the edges v1-v0 and v2-v1, made unit by 1/sqrt(x*x + y*y + z*z); zero when not above 1e-13) and the three corners,
   as float32, then u16 0 */
static int stl_write(const char *path, const Mesh *m) {
    WCHAR w[MAX_PATH * 2]; wpath(path, w, MAX_PATH * 2); FILE *f = _wfopen(w, L"wb"); if (!f) return 0;
    unsigned char hd[84] = { 0 }; uint32_t nt = (uint32_t)m->nt; memcpy(hd + 80, &nt, 4); fwrite(hd, 1, 84, f);
    unsigned char *buf = malloc((size_t)50 * (m->nt ? m->nt : 1));
    for (int i = 0; i < m->nt; i++) {
        const double *a = m->v + 3 * m->f[3 * i], *b = m->v + 3 * m->f[3 * i + 1], *c = m->v + 3 * m->f[3 * i + 2];
        double u0 = b[0] - a[0], u1 = b[1] - a[1], u2 = b[2] - a[2], w0 = c[0] - b[0], w1 = c[1] - b[1], w2 = c[2] - b[2];
        double cx = u1 * w2 - u2 * w1, cy = u2 * w0 - u0 * w2, cz = u0 * w1 - u1 * w0;
        double nn = sqrt(cx * cx + cy * cy + cz * cz); float o[12];
        if (nn > 1e-13) { double r = 1.0 / nn; o[0] = (float)(cx * r); o[1] = (float)(cy * r); o[2] = (float)(cz * r); } else o[0] = o[1] = o[2] = 0;
        for (int k = 0; k < 3; k++) { o[3 + k] = (float)a[k]; o[6 + k] = (float)b[k]; o[9 + k] = (float)c[k]; }
        memcpy(buf + 50 * (size_t)i, o, 48); buf[50 * (size_t)i + 48] = buf[50 * (size_t)i + 49] = 0;
    }
    fwrite(buf, 50, m->nt, f); free(buf);
    int ok = !ferror(f); fclose(f); return ok;
}
/* trimesh.load(stl, process=False): every face its own three corners (float32 read as float64) */
static int stl_read(const char *path, Mesh *m) {
    size_t n; char *b = read_file(path, &n); memset(m, 0, sizeof *m);
    if (!b || n < 84) { free(b); return 0; }
    uint32_t nt; memcpy(&nt, b + 80, 4); if (84 + 50 * (size_t)nt > n) { free(b); return 0; }
    m->nt = (int)nt; m->nv = 3 * (int)nt; m->v = malloc(sizeof(double) * 9 * (nt ? nt : 1)); m->f = malloc(sizeof(int) * 3 * (nt ? nt : 1));
    for (uint32_t i = 0; i < nt; i++) { float c[9]; memcpy(c, b + 84 + 50 * (size_t)i + 12, 36); for (int k = 0; k < 9; k++) m->v[9 * (size_t)i + k] = c[k]; for (int k = 0; k < 3; k++) m->f[3 * i + k] = 3 * (int)i + k; }
    free(b); return 1;
}
typedef struct { int64_t key; int dir, face; } Ed;          /* an edge: min * nv + max, 1 if it runs min -> max, its face */
static int ed_cmp(const void *a, const void *b) { const Ed *x = a, *y = b; return x->key < y->key ? -1 : x->key > y->key ? 1 : x->face - y->face; }
static Ed *mesh_edges(const Mesh *m) {
    Ed *e = malloc(sizeof(Ed) * 3 * (m->nt ? m->nt : 1));
    for (int i = 0; i < m->nt; i++) for (int k = 0; k < 3; k++) {
        int a = m->f[3 * i + k], b = m->f[3 * i + (k + 1) % 3]; int lo = a < b ? a : b, hi = a < b ? b : a;
        e[3 * i + k] = (Ed){ (int64_t)lo * m->nv + hi, a < b, i };
    }
    qsort(e, 3 * (size_t)m->nt, sizeof(Ed), ed_cmp); return e;
}
/* run_set._edge_counts: (open/odd edges, pinched edges); is_watertight: every edge in exactly two faces, opposite ways */
static void edge_counts(const Mesh *m, int *odd, int *pinched, int *watertight) {
    Ed *e = mesh_edges(m); size_t ne = 3 * (size_t)m->nt; *odd = *pinched = 0; int wt = 1, wind = 1;
    for (size_t i = 0; i < ne;) {
        size_t j = i; while (j < ne && e[j].key == e[i].key) j++;
        size_t c = j - i; if (c % 2 == 1) (*odd)++; else if (c > 2) (*pinched)++;
        if (c != 2) wt = 0; else if (e[i].dir == e[i + 1].dir) wind = 0;
        i = j;
    }
    *watertight = wt && wind; free(e);
}
static int uf_find(int *p, int x) { while (p[x] != x) { p[x] = p[p[x]]; x = p[x]; } return x; }
/* Trimesh.split(only_watertight=False): faces joined across edges shared by exactly two faces; comp[f] = part number,
   parts numbered by their first face. Returns the number of parts; vol[k] = each part's volume (signed, mm^3) */
static int mesh_split(const Mesh *m, int *comp, double **vol) {
    int *p = malloc(sizeof(int) * (m->nt ? m->nt : 1)); for (int i = 0; i < m->nt; i++) p[i] = i;
    Ed *e = mesh_edges(m); size_t ne = 3 * (size_t)m->nt;
    for (size_t i = 0; i < ne;) { size_t j = i; while (j < ne && e[j].key == e[i].key) j++; if (j - i == 2) { int a = uf_find(p, e[i].face), b = uf_find(p, e[i + 1].face); if (a != b) p[a < b ? b : a] = a < b ? a : b; } i = j; }
    free(e);
    int nc = 0; int *lab = malloc(sizeof(int) * (m->nt ? m->nt : 1)); for (int i = 0; i < m->nt; i++) lab[i] = -1;
    for (int i = 0; i < m->nt; i++) { int r = uf_find(p, i); if (lab[r] < 0) lab[r] = nc++; comp[i] = lab[r]; }
    *vol = calloc(nc ? nc : 1, sizeof(double));
    for (int i = 0; i < m->nt; i++) {
        const double *a = m->v + 3 * m->f[3 * i], *b = m->v + 3 * m->f[3 * i + 1], *c = m->v + 3 * m->f[3 * i + 2];
        (*vol)[comp[i]] += (a[0] * (b[1] * c[2] - b[2] * c[1]) - a[1] * (b[0] * c[2] - b[2] * c[0]) + a[2] * (b[0] * c[1] - b[1] * c[0])) / 6.0;
    }
    free(p); free(lab); return nc;
}
/* Trimesh.contains([pt]): the ray parity test of trimesh.ray.ray_util.contains_points (direction
   [0.4395064455, 0.617598629942, 0.652231566745], cast both ways; when the two disagree a third ray decides) */
static int ray_hits(const Mesh *m, const double o[3], const double d[3]) {
    int hits = 0;
    for (int i = 0; i < m->nt; i++) {
        const double *a = m->v + 3 * m->f[3 * i], *b = m->v + 3 * m->f[3 * i + 1], *c = m->v + 3 * m->f[3 * i + 2];
        double e1[3] = { b[0] - a[0], b[1] - a[1], b[2] - a[2] }, e2[3] = { c[0] - a[0], c[1] - a[1], c[2] - a[2] };
        double pv[3] = { d[1] * e2[2] - d[2] * e2[1], d[2] * e2[0] - d[0] * e2[2], d[0] * e2[1] - d[1] * e2[0] };
        double det = e1[0] * pv[0] + e1[1] * pv[1] + e1[2] * pv[2]; if (fabs(det) < 1e-14) continue;
        double inv = 1.0 / det, tv[3] = { o[0] - a[0], o[1] - a[1], o[2] - a[2] };
        double u = (tv[0] * pv[0] + tv[1] * pv[1] + tv[2] * pv[2]) * inv; if (u < 0 || u > 1) continue;
        double q[3] = { tv[1] * e1[2] - tv[2] * e1[1], tv[2] * e1[0] - tv[0] * e1[2], tv[0] * e1[1] - tv[1] * e1[0] };
        double v = (d[0] * q[0] + d[1] * q[1] + d[2] * q[2]) * inv; if (v < 0 || u + v > 1) continue;
        double t = (e2[0] * q[0] + e2[1] * q[1] + e2[2] * q[2]) * inv; if (t > 1e-12) hits++;
    }
    return hits;
}
static int mesh_contains(const Mesh *m, const double bmin[3], const double bmax[3], double x, double y, double z) {
    double o[3] = { x, y, z };
    for (int k = 0; k < 3; k++) if (o[k] < bmin[k] || o[k] > bmax[k]) return 0;
    double d[3] = { 0.4395064455, 0.617598629942, 0.652231566745 }, nd[3] = { -d[0], -d[1], -d[2] };
    int f = ray_hits(m, o, d) % 2, b = ray_hits(m, o, nd) % 2;
    if (f == b) return f;
    double d3[3] = { 0.267, -0.534, 0.802 }; return ray_hits(m, o, d3) % 2;
}
static void mesh_bounds(const Mesh *m, double mn[3], double mx[3]) {
    for (int k = 0; k < 3; k++) { mn[k] = INFINITY; mx[k] = -INFINITY; }
    for (int i = 0; i < m->nv; i++) for (int k = 0; k < 3; k++) { double v = m->v[3 * i + k]; if (v < mn[k]) mn[k] = v; if (v > mx[k]) mx[k] = v; }
}

/* ---- gg.magnet_centres / oval_magnet_centres / max_height ---- */
static int magnet_centres(double dia, double *xy) {
    int n = dia <= 40 ? 1 : dia <= 60 ? 2 : dia <= 100 ? 3 : 4;
    if (n <= 1) { xy[0] = xy[1] = 0; return 1; }
    double r = dia * 0.25;
    for (int i = 0; i < n; i++) { double t = M_PI / 2 + 2 * M_PI * i / n; xy[2 * i] = py_round(r * py_cos(t), 4); xy[2 * i + 1] = py_round(r * py_sin(t), 4); }
    if (dia > 100) { xy[2 * n] = xy[2 * n + 1] = 0; n++; }
    return n;
}
static int oval_magnet_centres(double A, double *xy) {
    int n = A <= 60.5 ? 1 : A <= 105.5 ? 2 : 3; double x = py_round(A * 0.25, 4);
    if (n == 1) { xy[0] = xy[1] = 0; return 1; }
    if (n == 2) { xy[0] = -x; xy[1] = 0; xy[2] = x; xy[3] = 0; return 2; }
    xy[0] = -x; xy[1] = 0; xy[2] = 0; xy[3] = 0; xy[4] = x; xy[5] = 0; return 3;
}
static double max_height(double mm) {
    double m = cfg_num("max_detail_height_mm", 3.3);
    if (m <= 0) return 1000.0;
    return m * (mm >= 59.9 ? cfg_num("big_base_height_factor", 1.5) : 1.0);
}

/* ---- compare_bases: heightmap, rotations, score (the "too similar" check and the report's "% different") ---- */
typedef struct { char name[160]; float *H; } Recent;
static double CB_R; static int CB_N;
static void cb_set_size(double mm) { CB_R = mm / 2.0 - 1.4; double v = py_rint(2 * CB_R / 0.4945); v = v < 120 ? v : 120; v = v > 40 ? v : 40; CB_N = (int)v; }
static void linspace(double a, double b, int n, double *x) { double step = (b - a) / (n - 1); for (int i = 0; i < n; i++) x[i] = i * step + a; if (n > 1) x[n - 1] = b; }
static int ss_left(const double *x, int n, double v) { int lo = 0, hi = n; while (lo < hi) { int mid = (lo + hi) / 2; if (x[mid] < v) lo = mid + 1; else hi = mid; } return lo; }
static int ss_right(const double *x, int n, double v) { int lo = 0, hi = n; while (lo < hi) { int mid = (lo + hi) / 2; if (x[mid] <= v) lo = mid + 1; else hi = mid; } return lo; }
static float *cb_heightmap(const Mesh *m) {          /* m: the STL as trimesh.load gives it (merged) */
    int N = CB_N; double R = CB_R, *xs = malloc(sizeof(double) * N); linspace(-R, R, N, xs);
    float *H = malloc(sizeof(float) * N * N); for (int i = 0; i < N * N; i++) H[i] = 4.0f;
    for (int t = 0; t < m->nt; t++) {
        const double *a = m->v + 3 * m->f[3 * t], *b = m->v + 3 * m->f[3 * t + 1], *c = m->v + 3 * m->f[3 * t + 2];
        double zmax = fmax(fmax(a[2], b[2]), c[2]); if (!(zmax > 3.0)) continue;
        if (!(fabs(a[2] - 4.0) > 1e-6 || fabs(b[2] - 4.0) > 1e-6 || fabs(c[2] - 4.0) > 1e-6)) continue;
        double dd = (b[1] - c[1]) * (a[0] - c[0]) + (c[0] - b[0]) * (a[1] - c[1]); if (!(fabs(dd) > 1e-12)) continue;
        double x0 = fmin(fmin(a[0], b[0]), c[0]), x1 = fmax(fmax(a[0], b[0]), c[0]), y0 = fmin(fmin(a[1], b[1]), c[1]), y1 = fmax(fmax(a[1], b[1]), c[1]);
        int i0 = ss_left(xs, N, x0), i1 = ss_right(xs, N, x1), j0 = ss_left(xs, N, y0), j1 = ss_right(xs, N, y1);
        if (!(i0 < i1 && j0 < j1)) continue;
        double d = (b[1] - c[1]) * (a[0] - c[0]) + (c[0] - b[0]) * (a[1] - c[1]); if (fabs(d) < 1e-12) continue;
        for (int i = i0; i < i1; i++) for (int j = j0; j < j1; j++) {
            double X = xs[i], Y = xs[j];
            double w0 = ((b[1] - c[1]) * (X - c[0]) + (c[0] - b[0]) * (Y - c[1])) / d, w1 = ((c[1] - a[1]) * (X - c[0]) + (a[0] - c[0]) * (Y - c[1])) / d, w2 = 1 - w0 - w1;
            if (!(w0 >= -1e-9 && w1 >= -1e-9 && w2 >= -1e-9)) continue;
            double Z = w0 * a[2] + w1 * b[2] + w2 * c[2]; float *h = &H[i * N + j];
            if (Z > (double)*h) *h = (float)Z;
        }
    }
    free(xs); return H;
}
static float *cb_rotations(const float *H, int steps) {
    int N = CB_N; double R = CB_R, *xs = malloc(sizeof(double) * N); linspace(-R, R, N, xs);
    float *out = malloc(sizeof(float) * steps * N * N);
    for (int k = 0; k < steps; k++) {
        double t = 2 * M_PI * k / steps, c = py_cos(t), s = py_sin(t);
        for (int i = 0; i < N; i++) for (int j = 0; j < N; j++) {
            double X = xs[i], Y = xs[j], U = c * X + s * Y, V = -s * X + c * Y;
            int iu = (int)rint((U + R) / (2 * R) * (N - 1)), iv = (int)rint((V + R) / (2 * R) * (N - 1));
            iu = iu < 0 ? 0 : iu > N - 1 ? N - 1 : iu; iv = iv < 0 ? 0 : iv > N - 1 ? N - 1 : iv;
            out[((size_t)k * N + i) * N + j] = H[iu * N + iv];
        }
    }
    free(xs); return out;
}
static double cb_score(const float *Ri, int steps, const float *Hj, const unsigned char *disc) {          /* 100 * the best rotation's differing share */
    int N = CB_N; size_t NN = (size_t)N * N; double best = 1e300;
    float *d1 = malloc(sizeof(float) * NN), *d2 = malloc(sizeof(float) * NN);
    for (int k = 0; k < steps; k++) {
        const float *Rk = Ri + k * NN;
        for (size_t q = 0; q < NN; q++) d1[q] = d2[q] = INFINITY;
        for (int dx = -1; dx <= 1; dx++) for (int dy = -1; dy <= 1; dy++)
            for (int i = 0; i < N; i++) for (int j = 0; j < N; j++) {
                int si = ((i - dx) % N + N) % N, sj = ((j - dy) % N + N) % N;          /* np.roll(np.roll(H, dx, 0), dy, 1)[i, j] = H[i - dx, j - dy] */
                float a = fabsf(Rk[i * N + j] - Hj[si * N + sj]), b = fabsf(Rk[si * N + sj] - Hj[i * N + j]);
                if (dx == -1 && dy == -1) { d1[i * N + j] = a; d2[i * N + j] = b; }
                else { if (a < d1[i * N + j]) d1[i * N + j] = a; if (b < d2[i * N + j]) d2[i * N + j] = b; }
            }
        long cnt = 0, det = 0;
        for (size_t q = 0; q < NN; q++) if (disc[q]) {
            float d = d1[q] > d2[q] ? d1[q] : d2[q]; int detail = fabsf(Rk[q] - 4.0f) > 0.15f || fabsf(Hj[q] - 4.0f) > 0.15f;
            if (detail) { det++; if (d > 0.3f) cnt++; }
        }
        double frac = (double)cnt / (double)(det > 1 ? det : 1);
        if (frac < best) best = frac;
    }
    free(d1); free(d2); return 100 * best;
}

/* ---- live.py: the progress notes the window reads (out\.live\<SALT>_<n>.txt), rewritten whole via .tmp ---- */
static char LIVE_DIR[MAX_PATH * 3];
static void live_path(const char *salt, int n, char *p, size_t cap) { snprintf(p, cap, "%s\\%s_%d.txt", LIVE_DIR, salt, n); }
static void now_repr(char *t, size_t n) { FILETIME ft; GetSystemTimePreciseAsFileTime(&ft); uint64_t v = ((uint64_t)ft.dwHighDateTime << 32) | ft.dwLowDateTime; runner_float_repr((double)(v - 116444736000000000ULL) / 1e7, t, (int)n); }
/* live.note / live.set_stage: the note's key=value lines (in their order), the given keys set; a new stage sets stage_t0 */
static void live_note(const char *salt, int n, int nkv, const char **kv) {
    if (!LIVE_DIR[0]) return;
    char p[MAX_PATH * 3], tmp[MAX_PATH * 3 + 8], now[40]; live_path(salt, n, p, sizeof p); now_repr(now, sizeof now);
    char *t = read_file(p, NULL); if (!t) return;          /* no note: the base is not being watched (or is done) */
    char *keys[96], *vals[96]; int nk = 0;
    for (char *l = strtok(t, "\n"); l && nk < 90; l = strtok(NULL, "\n")) { char *e = strchr(l, '='); if (!e) continue; *e = 0; keys[nk] = xstrdup(l); vals[nk] = xstrdup(e + 1); nk++; }
    free(t);
    for (int i = 0; i < nkv; i++) {
        const char *k = kv[2 * i], *v = kv[2 * i + 1]; int at = -1;
        for (int j = 0; j < nk; j++) if (!strcmp(keys[j], k)) at = j;
        if (!strcmp(k, "stage")) { int st = -1; for (int j = 0; j < nk; j++) if (!strcmp(keys[j], "stage")) st = j; if (st < 0 || strcmp(vals[st], v)) { int s0 = -1; for (int j = 0; j < nk; j++) if (!strcmp(keys[j], "stage_t0")) s0 = j; if (s0 >= 0) { free(vals[s0]); vals[s0] = xstrdup(now); } else if (nk < 95) { keys[nk] = xstrdup("stage_t0"); vals[nk++] = xstrdup(now); } } }
        if (at >= 0) { free(vals[at]); vals[at] = xstrdup(v); } else if (nk < 95) { keys[nk] = xstrdup(k); vals[nk++] = xstrdup(v); }
    }
    int at = -1; for (int j = 0; j < nk; j++) if (!strcmp(keys[j], "now")) at = j;
    if (at >= 0) { free(vals[at]); vals[at] = xstrdup(now); } else { keys[nk] = xstrdup("now"); vals[nk++] = xstrdup(now); }
    Sb o = { 0 }; for (int j = 0; j < nk; j++) { sb_printf(&o, "%s=%s\n", keys[j], vals[j]); free(keys[j]); free(vals[j]); }
    snprintf(tmp, sizeof tmp, "%s.tmp", p); WCHAR wt[MAX_PATH * 3], wp[MAX_PATH * 3]; wpath(tmp, wt, MAX_PATH * 3); wpath(p, wp, MAX_PATH * 3);
    FILE *f = _wfopen(wt, L"wb"); if (f) { fwrite(o.s, 1, o.n, f); fclose(f); MoveFileExW(wt, wp, MOVEFILE_REPLACE_EXISTING); }
    free(o.s);
}
static void live_finish(const char *salt, int n) {          /* live.finish: the note and its side files go */
    if (!LIVE_DIR[0]) return;
    char p[MAX_PATH * 3], q[MAX_PATH * 3 + 16]; live_path(salt, n, p, sizeof p); const char *suf[4] = { "", ".tiles", ".shapes", ".tmp" };
    for (int i = 0; i < 4; i++) { snprintf(q, sizeof q, "%s%s", p, suf[i]); WCHAR w[MAX_PATH * 3]; wpath(q, w, MAX_PATH * 3); DeleteFileW(w); }
}

/* ---- a JSON value written back as Python's json.dumps / json.dump(indent=1) would (numbers keep Python's own text) ---- */
static void jdump(Sb *b, const JV *v, int indent, int level) {
    switch (v->t) {
    case 0: sb_puts(b, "null"); return; case 1: sb_puts(b, "false"); return; case 2: sb_puts(b, "true"); return;
    case 3: sb_puts(b, v->s); return; case 4: json_str(b, v->s); return;
    }
    int obj = v->t == 6; sb_puts(b, obj ? "{" : "[");
    if (!v->n) { sb_puts(b, obj ? "}" : "]"); return; }
    for (int i = 0; i < v->n; i++) {
        if (i) sb_puts(b, ",");
        if (indent) { sb_puts(b, "\n"); for (int k = 0; k < (level + 1) * indent; k++) sb_puts(b, " "); } else if (i) sb_puts(b, " ");
        if (obj) { json_str(b, v->k[i]); sb_puts(b, ": "); }
        jdump(b, &v->a[i], indent, level + 1);
    }
    if (indent) { sb_puts(b, "\n"); for (int k = 0; k < level * indent; k++) sb_puts(b, " "); }
    sb_puts(b, obj ? "}" : "]");
}
/* str() of a JSON number / string as Python prints the value */
static const char *jtext(const JV *v, const char *d) { return !v ? d : v->t == 3 || v->t == 4 ? v->s : v->t == 2 ? "True" : v->t == 1 ? "False" : d; }

/* ---- run_set.py ---- */
static const char *ALL_STYLES[19] __attribute__((unused)) = { "badlands", "highland", "maahl", "corridor", "infestation", "urban", "mine", "swamp", "cynder", "targa", "trederra", "fortress", "beast", "frontier", "graveyard", "forest", "dungeon", "temple", "coast" };
static const char *VARIANTS[8] = { "", "r1", "r2", "r3", "r4", "r5", "r6", "r7" };
static const double KNOWN_SIZES[15] = { 25, 28, 28.5, 30, 32, 40, 50, 60, 65, 70, 80, 90, 100, 130, 160 };
static const double OVAL_SIZES[7][2] = { { 60, 35.5 }, { 75, 42 }, { 90, 52 }, { 105, 70 }, { 120, 92 }, { 150, 95 }, { 170, 109 } };
typedef struct {
    char variant[16], *errors, *reason, secs[32], slowest[64], steps[16], theme[256], *plan_line, *plan, *dropped, restarts[16], note[64], diff[16], *made;
} Try;
typedef struct { char base[160]; int n, ok, ntries, detail; Try tries[8]; } BaseRes;
typedef struct RunSet RunSet;
struct RunSet {
    const char *style, *salt; double SIZE, OVAL[2]; int oval; double SCALE, LAYOUT; char out[MAX_PATH * 3], size_tag[64];
    int workers, nnums, *nums; volatile LONG next, flagged, stop; ULONGLONG t0;
    CRITICAL_SECTION lock; Recent *recent; int nrecent; Bridge *free_br; int nfree, capfree;
    WCHAR *envw; const EnvSet *env; int nenv; BaseRes *res; int nres;
    void (*done)(RunSet *rs, BaseRes *r, void *ctx); void *done_ctx;          /* gui_job: one base finished (its OK / FLAG line) */
};
static void rs_print(const char *fmt, ...) {          /* a line run_set prints: gui_job passes each on as a "log" event */
    char t[4096]; va_list ap; va_start(ap, fmt); vsnprintf(t, sizeof t, fmt, ap); va_end(ap);
    for (char *l = t, *e; *l; l = e) { e = strchr(l, '\n'); char c = 0; if (e) { c = *e; *e = 0; } if (*l) ev("log", "s", "text", l); if (!e) break; *e = c; e++; }
}
static void nm(const RunSet *rs, int n, char *out, size_t cap) { snprintf(out, cap, "%s_%smm_%02d", rs->style, rs->size_tag, n); }          /* run_set.nm for numbers given as 3,7,12 */
static double base_limit(const RunSet *rs) { double f = (rs->SIZE / 30.0) * (rs->SIZE / 30.0); return cfg_num("base_limit_secs", 120) * (f > 1.0 ? f : 1.0); }
static int threads_kit(void) {          /* cmesh.threads() */
    char e[16]; int v;
    if (GetEnvironmentVariableA("KIT_THREADS", e, sizeof e) && e[0] && strspn(e, "0123456789") == strlen(e)) v = atoi(e);
    else { JV *c = cfg_get("cpu_threads"); if (!c || c->t == 2) v = (int)GetActiveProcessorCount(ALL_PROCESSOR_GROUPS); else if (c->t == 1 || c->t == 0) v = 0; else v = (int)c->d; }
    return v <= 1 ? 0 : v > 256 ? 256 : v;
}
int runner_threads(void) { runner_cfg_init(); return threads_kit(); }
static int gpu_mode(void) { char e[8]; if (GetEnvironmentVariableA("KIT_GPU", e, sizeof e) && (!strcmp(e, "0") || !strcmp(e, "1"))) return e[0] == '1'; return cfg_bool("video_card", 1); }

/* the colour view: what the 2D drawing <name>_2d.png shows (draw2d's win, in the built base's mm), kept beside it as
   <name>_2d.win ("x0 y0 x1 y1"), so the finished base can be coloured from it later. Always 1 (a drawing without it
   just has no colour view) */
static int save_win(const char *png2d, const double win[4]) {
    char p[MAX_PATH * 3]; size_t n = strlen(png2d); if (n < 4 || n + 1 > sizeof p) return 1;
    snprintf(p, sizeof p, "%.*s.win", (int)(n - 4), png2d);
    WCHAR w[MAX_PATH * 3]; wpath(p, w, MAX_PATH * 3); FILE *f = _wfopen(w, L"wb");
    if (f) { fprintf(f, "%.17g %.17g %.17g %.17g\n", win[0], win[1], win[2], win[3]); fclose(f); }
    return 1;
}
static int load_win(const char *png2d, double win[4]) {
    char p[MAX_PATH * 3]; size_t n = strlen(png2d); if (n < 4 || n + 1 > sizeof p) return 0;
    snprintf(p, sizeof p, "%.*s.win", (int)(n - 4), png2d);
    WCHAR w[MAX_PATH * 3]; wpath(p, w, MAX_PATH * 3); FILE *f = _wfopen(w, L"rb"); if (!f) return 0;
    int ok = fscanf(f, "%lf %lf %lf %lf", &win[0], &win[1], &win[2], &win[3]) == 4 && win[2] > win[0] && win[3] > win[1]; fclose(f); return ok;
}

/* mesher.shot(stl, png, w, h, elev, azim) through bbb_kit.exe --core-shot (cmesh.shot); by: what drew it */
static int shot_png(const Mesh *soup, const char *png, int w, int h, double elev, double azim, char *by, size_t bycap) {
    size_t n = 40 + 24 * (size_t)soup->nv + 12 * (size_t)soup->nt; unsigned char *b = malloc(n);          /* "BBS1" + w h mode + elev azim + nv nt = 40 bytes, then the arrays (was 36: 4 bytes past the end) */ uint32_t u[3] = { (uint32_t)w, (uint32_t)h, gpu_mode() ? 1u : 2u }, c2[2] = { (uint32_t)soup->nv, (uint32_t)soup->nt };
    memcpy(b, "BBS1", 4); memcpy(b + 4, u, 12); memcpy(b + 16, &elev, 8); memcpy(b + 24, &azim, 8); memcpy(b + 32, c2, 8);
    memcpy(b + 40 - 4 + 0, c2, 0); memcpy(b + 40, soup->v, 24 * (size_t)soup->nv); memcpy(b + 40 + 24 * (size_t)soup->nv, soup->f, 12 * (size_t)soup->nt);
    Sb out = { 0 }, err = { 0 }; int rc = run_core("--core-shot", b, 40 + 24 * (size_t)soup->nv + 12 * (size_t)soup->nt, NULL, 0, &out, &err); free(b);
    int ok = 0;
    if (rc == 0 && out.n >= 13 && !memcmp(out.s, "BBI1", 4)) {
        uint32_t W, Hh; memcpy(&W, out.s + 4, 4); memcpy(&Hh, out.s + 8, 4); int dl = (unsigned char)out.s[12];
        snprintf(by, bycap, "%.*s", dl, out.s + 13);
        if (out.n >= 13 + (size_t)dl + (size_t)W * Hh * 3) ok = d2_png_write(png, (unsigned char *)out.s + 13 + dl, (int)W, (int)Hh, 3);
    } else snprintf(by, bycap, "failed: %s", err.s ? err.s : "");
    free(out.s); free(err.s); return ok;
}

/* the colour view of the same picture: each point coloured by the 2D drawing straight above it ("BBS2": tex RGB tw x th
   showing win). 1 = ok */
static int shot_png_col(const Mesh *soup, const char *png, int w, int h, double elev, double azim, const unsigned char *tex, int tw, int th, const double win[4], char *by, size_t bycap) {
    size_t mesh = 24 * (size_t)soup->nv + 12 * (size_t)soup->nt, n = 40 + mesh + 40 + (size_t)tw * th * 3; unsigned char *b = malloc(n); if (!b) return 0;
    uint32_t u[3] = { (uint32_t)w, (uint32_t)h, gpu_mode() ? 1u : 2u }, c2[2] = { (uint32_t)soup->nv, (uint32_t)soup->nt }, t2[2] = { (uint32_t)tw, (uint32_t)th };
    memcpy(b, "BBS2", 4); memcpy(b + 4, u, 12); memcpy(b + 16, &elev, 8); memcpy(b + 24, &azim, 8); memcpy(b + 32, c2, 8);
    memcpy(b + 40, soup->v, 24 * (size_t)soup->nv); memcpy(b + 40 + 24 * (size_t)soup->nv, soup->f, 12 * (size_t)soup->nt);
    memcpy(b + 40 + mesh, win, 32); memcpy(b + 40 + mesh + 32, t2, 8); memcpy(b + 40 + mesh + 40, tex, (size_t)tw * th * 3);
    Sb out = { 0 }, err = { 0 }; int rc = run_core("--core-shot", b, n, NULL, 0, &out, &err); free(b);
    int ok = 0;
    if (rc == 0 && out.n >= 13 && !memcmp(out.s, "BBI1", 4)) {
        uint32_t W, Hh; memcpy(&W, out.s + 4, 4); memcpy(&Hh, out.s + 8, 4); int dl = (unsigned char)out.s[12];
        snprintf(by, bycap, "%.*s", dl, out.s + 13);
        if (out.n >= 13 + (size_t)dl + (size_t)W * Hh * 3) ok = d2_png_write(png, (unsigned char *)out.s + 13 + dl, (int)W, (int)Hh, 3);
    } else snprintf(by, bycap, "failed: %s", err.s ? err.s : "");
    free(out.s); free(err.s); return ok;
}

/* one base, mesher.build + mesher serve's picture: the report r (a JSON object as Sb text) or an error */
typedef struct { char *error; double secs; JV head; char path[MAX_PATH * 3], mesher[128], pictures[200], note[200]; int cells, tris, watertight; double top_mm; Sb skipped; } Built;
static void build_once(RunSet *rs, Bridge *br, int n, const char *variant, Built *B) {
    memset(B, 0, sizeof *B); ULONGLONG t = GetTickCount64(); char name[160]; nm(rs, n, name, sizeof name);
    char png2d[MAX_PATH * 3]; snprintf(B->path, sizeof B->path, "%s/%s/%s.stl", rs->out, rs->style, name); snprintf(png2d, sizeof png2d, "%.*s_2d.png", (int)strlen(B->path) - 4, B->path);
    char dir[MAX_PATH * 3]; snprintf(dir, sizeof dir, "%s\\%s", rs->out, rs->style); make_dirs(dir);
    Sb rq = { 0 }; sb_printf(&rq, "{\"n\": %d, \"variant\": ", n); json_str(&rq, variant); sb_puts(&rq, ", \"name\": "); json_str(&rq, name); sb_puts(&rq, ", \"png2d\": "); json_str(&rq, png2d); sb_puts(&rq, "}");
    unsigned char **bl; size_t *ln; int nb;
    int r = layout_python_build(br, rq.s, base_limit(rs), &B->head, &bl, &ln, &nb); free(rq.s);
    char e[400];
    if (r < 0) { snprintf(e, sizeof e, "over %.0f s limit", base_limit(rs)); B->error = xstrdup(e); B->secs = py_rint((GetTickCount64() - t) / 1000.0); free_blobs(bl, ln, nb); return; }
    if (r == 0) { B->error = xstrdup("crash: the layout helper stopped"); B->secs = py_rint((GetTickCount64() - t) / 1000.0); free_blobs(bl, ln, nb); return; }
    JV *ok = jget(&B->head, "ok");
    if (!ok || !ok->d) { JV *w = jget(&B->head, "why"); B->error = xstrdup(w && w->s ? w->s : "crash"); B->secs = py_rint((GetTickCount64() - t) / 1000.0); free_blobs(bl, ln, nb); return; }
    JV *sk = jget(&B->head, "skipped"); for (int i = 0; sk && i < sk->n; i++) { if (B->skipped.n) sb_add(&B->skipped, "\n", 1); sb_puts(&B->skipped, sk->a[i].s); }
    JV *thj = jget(&B->head, "threads"); int nthreads = thj ? (int)thj->d : threads_kit();          /* cmesh.threads() as the layout's process sees it */
    char lp[MAX_PATH * 3] = ""; if (LIVE_DIR[0]) live_path(rs->salt, n, lp, sizeof lp);
    int drawn = 0; D2Scene sc; int have_sc = nb > 1 && d2_read(bl[1], ln[1], &sc); JV *pyd = jget(&B->head, "py_draw"); int py_drawn = pyd && pyd->d;
    if (LIVE_DIR[0]) {          /* mesher.build with the window watching: the 2D drawing first, then the field */
        double win[4] = { 0 }; char er[200];
        if (cfg_bool("drawing_2d", 1) && (py_drawn || (have_sc && d2_draw(&sc, png2d, 900, 0, win, er, sizeof er) && save_win(png2d, win)))) {
            drawn = 1; char box[200]; snprintf(box, sizeof box, "%.3f,%.3f,%.3f,%.3f", win[0], win[1], win[2], win[3]);
            const char *kv[] = { "plan", png2d, "plan_box", box }; live_note(rs->salt, n, 2, kv);
        }
        char st[16]; snprintf(st, sizeof st, "%d", (int)jget(&B->head, "steps")->d);
        const char *kv[] = { "stage", "field", "steps_total", st, "steps_done", "0" }; live_note(rs->salt, n, 3, kv);
        char th[16], ns[16]; snprintf(th, sizeof th, "%d", nthreads); snprintf(ns, sizeof ns, "%d", (int)jget(&B->head, "nsteps")->d);
        const char *kv2[] = { "threads", th, "steps_total", ns }; live_note(rs->salt, n, 2, kv2);
    }
    /* the height field and the mesh: bbb_kit.exe --core-build (cmesh.build_c) */
    char thr[16]; snprintf(thr, sizeof thr, "%d", nthreads);
    EnvSet es[2] = { { "KIT_MESH_THREADS", thr }, { "KIT_LIVE_FILE", lp[0] ? lp : NULL } };
    Sb out = { 0 }, err = { 0 }; int rc = run_core("--core-build", bl[0], ln[0], es, 2, &out, &err);
    if (rc != 0 || out.n < 12 || memcmp(out.s, "BBR1", 4)) {          /* cmesh.build_c gave None: Python would build this one in Python (not in C) */
        snprintf(e, sizeof e, "crash: the C field builder failed (%.200s)", err.s ? err.s : ""); B->error = xstrdup(e);
        free(out.s); free(err.s); free_blobs(bl, ln, nb); if (have_sc) d2_scene_free(&sc); B->secs = py_rint((GetTickCount64() - t) / 1000.0); return;
    }
    uint32_t nv, nt; memcpy(&nv, out.s + 4, 4); memcpy(&nt, out.s + 8, 4); size_t en = 12 + 24 * (size_t)nv + 12 * (size_t)nt;
    Mesh m = { (int)nv, (int)nt, malloc(24 * (size_t)(nv ? nv : 1)), malloc(12 * (size_t)(nt ? nt : 1)) };
    memcpy(m.v, out.s + 12, 24 * (size_t)nv); memcpy(m.f, out.s + 12 + 24 * (size_t)nv, 12 * (size_t)nt);
    if (out.n >= en + 12 && !memcmp(out.s + en, "BBX1", 4)) {
        uint32_t nc, ns; memcpy(&nc, out.s + en + 4, 4); memcpy(&ns, out.s + en + 8, 4); B->cells = (int)nc;
        for (char *l = out.s + en + 12, *end = out.s + en + 12 + ns; l < end;) { char *q = memchr(l, '\n', end - l); if (!q) q = end; if (q > l) { if (B->skipped.n) sb_add(&B->skipped, "\n", 1); sb_add(&B->skipped, l, q - l); } l = q + 1; }
    }
    free(out.s); free(err.s);
    JV *cf = jget(&B->head, "cfield"); int cfield = cf && cf->t == 2;
    if (nthreads > 1 || cfield) snprintf(B->mesher, sizeof B->mesher, "C, bbb_kit.exe, field in C, %d thread%s", nthreads ? nthreads : 1, nthreads > 1 ? "s" : "");
    else snprintf(B->mesher, sizeof B->mesher, "C, bbb_kit.exe");          /* one core: the Python field + the C mesher (the same mesh) */
    mesh_merge(&m);
    int tris_field = m.nt;          /* len(f): the surface's faces, before the solids */
    {   /* --supports on: m, extra = add_solids(...) (the shells were built by the layout helper: solids3d.c, the "BBS1" blob); trimesh.util.concatenate([m] + extra) */
        JV *sk3 = jget(&B->head, "skipped3d");
        for (int i = 0; sk3 && i < sk3->n; i++) { if (B->skipped.n) sb_add(&B->skipped, "\n", 1); sb_puts(&B->skipped, sk3->a[i].s); }
        if (nb > 2 && ln[2] >= 8 && !memcmp(bl[2], "BBS1", 4)) {
            const unsigned char *p = bl[2] + 8, *end = bl[2] + ln[2]; uint32_t ns; memcpy(&ns, bl[2] + 4, 4);
            for (uint32_t k = 0; k < ns && p + 8 <= end; k++) {
                uint32_t sv, st; memcpy(&sv, p, 4); memcpy(&st, p + 4, 4); p += 8;
                if (p + 24 * (size_t)sv + 12 * (size_t)st > end) break;
                m.v = realloc(m.v, 24 * (size_t)(m.nv + sv ? m.nv + sv : 1)); m.f = realloc(m.f, 12 * (size_t)(m.nt + st ? m.nt + st : 1));
                memcpy(m.v + 3 * (size_t)m.nv, p, 24 * (size_t)sv); p += 24 * (size_t)sv;
                for (size_t q = 0; q < 3 * (size_t)st; q++) { int32_t fi; memcpy(&fi, p + 4 * q, 4); m.f[3 * (size_t)m.nt + q] = fi + m.nv; }
                p += 12 * (size_t)st; m.nv += (int)sv; m.nt += (int)st;
            }
        }
    }
    int odd, pin; edge_counts(&m, &odd, &pin, &B->watertight); B->tris = tris_field;
    double mn[3], mx[3]; mesh_bounds(&m, mn, mx); B->top_mm = py_round(mx[2] - jget(&B->head, "H")->d, 1);
    stl_write(B->path, &m); mesh_free(&m);
    if (cfg_bool("drawing_2d", 1) && !drawn && !py_drawn && have_sc) { double win[4]; char er[200]; if (d2_draw(&sc, png2d, 900, 0, win, er, sizeof er)) save_win(png2d, win); }
    if (have_sc) d2_scene_free(&sc);
    free_blobs(bl, ln, nb);
    B->secs = py_round((GetTickCount64() - t) / 1000.0, 1);
    { const char *kv[] = { "stage", "picture" }; live_note(rs->salt, n, 1, kv); }          /* mesher serve: the 3D picture */
    Mesh soup; char iso[MAX_PATH * 3], by[200]; snprintf(iso, sizeof iso, "%.*s_iso.png", (int)strlen(B->path) - 4, B->path);
    if (stl_read(B->path, &soup) && shot_png(&soup, iso, 900, 560, 30, -60, by, sizeof by)) snprintf(B->pictures, sizeof B->pictures, "%s", (by[0] && strncmp(by, "failed", 6)) ? by : "Python");
    else snprintf(B->note, sizeof B->note, "no picture: %s", by);
    mesh_free(&soup);
}

/* run_set.drop_specks(stl) then run_set.close_seams(stl) (meshglue.h, proven against trimesh): the STL rewritten when
   either changes it. *seam = close_seams' answer (it repaired a seam). 0 = ok, -1 = it raised (*err says what) */
static int specks_and_seams(const char *stl, int *specks, int *seam, char *err, size_t errcap) {
    size_t n; unsigned char *b = (unsigned char *)read_file(stl, &n); MgBuf o = { 0 }; int rem = 0, rep = 0;
    *specks = 0; *seam = 0;
    if (!b) { snprintf(err, errcap, "OSError: the STL cannot be read"); return -1; }
    if (mg_drop_specks(b, n, 1.5, allow_supports(), &o, &rem) < 0) { snprintf(err, errcap, "%s", mg_err[0] ? mg_err : "Exception"); free(b); free(o.b); return -1; }
    if (rem > 0 && o.n) { WCHAR w[MAX_PATH * 3]; FILE *f; wpath(stl, w, MAX_PATH * 3); f = _wfopen(w, L"wb"); if (f) { fwrite(o.b, 1, o.n, f); fclose(f); } free(b); b = o.b; n = o.n; o.b = NULL; o.n = o.cap = 0; }
    free(o.b); memset(&o, 0, sizeof o);
    *specks = rem;
    if (mg_close_seams(b, n, &o, &rep) < 0) { snprintf(err, errcap, "%s", mg_err[0] ? mg_err : "Exception"); free(b); free(o.b); return -1; }
    if (rep && o.n) { WCHAR w[MAX_PATH * 3]; FILE *f; wpath(stl, w, MAX_PATH * 3); f = _wfopen(w, L"wb"); if (f) { fwrite(o.b, 1, o.n, f); fclose(f); } }
    *seam = rep;
    free(o.b); free(b); return 0;
}
/* run_set.print_check */
static void print_check(RunSet *rs, const char *stl, Sb *probs, int *specks, float **Hn) {
    Mesh m; *specks = 0; *Hn = NULL;
    if (!stl_read(stl, &m)) { sb_puts(probs, "checker error: OSError: the STL cannot be read"); return; }
    mesh_merge(&m);
    int *comp = malloc(sizeof(int) * (m.nt ? m.nt : 1)); double *vol; int nc = mesh_split(&m, comp, &vol);
    int odd, pin, wt; edge_counts(&m, &odd, &pin, &wt);
    if (odd) sb_puts(probs, "not watertight");
    int parts = 0; for (int k = 0; k < nc; k++) if (fabs(vol[k]) > 0.05) parts++;
    double mn[3], mx[3]; mesh_bounds(&m, mn, mx); double w = mx[0] - mn[0], top = mx[2], dy = mx[1] - mn[1];
    char t[200];
    if (parts != 1 && !allow_supports()) { snprintf(t, sizeof t, "%d loose pieces", parts); if (probs->n) sb_puts(probs, ", "); sb_puts(probs, t); }
    if (rs->oval) {
        if (fabs(w - rs->OVAL[0]) > 0.05 || fabs(dy - rs->OVAL[1]) > 0.05) { char a[32], b[32]; fmt_g(a, sizeof a, rs->OVAL[0]); fmt_g(b, sizeof b, rs->OVAL[1]); snprintf(t, sizeof t, "%.2f x %.2f mm, not %s x %s", w, dy, a, b); if (probs->n) sb_puts(probs, ", "); sb_puts(probs, t); }
    } else if (fabs(w - rs->SIZE) > 0.05) { char a[32]; fmt_g(a, sizeof a, rs->SIZE); snprintf(t, sizeof t, "%.2f mm wide, not %s", w, a); if (probs->n) sb_puts(probs, ", "); sb_puts(probs, t); }
    double mh = max_height(rs->SIZE) * rs->SCALE; if (mh > 1000.0) mh = 1000.0;
    if (top > 4.0 + mh + 0.05 && top > 4.0 + (rs->SCALE != 1.0 ? 25.0 : 0) + 0.05) { snprintf(t, sizeof t, "too tall (%.1f mm above deck)", top - 4); if (probs->n) sb_puts(probs, ", "); sb_puts(probs, t); }
    double cens[12]; int ncen = rs->oval ? oval_magnet_centres(rs->OVAL[0], cens) : magnet_centres(rs->SIZE, cens); int blocked = 0, has0 = 0;
    for (int i = 0; i < ncen; i++) { if (mesh_contains(&m, mn, mx, cens[2 * i], cens[2 * i + 1], 1.0)) blocked = 1; if (cens[2 * i] == 0 && cens[2 * i + 1] == 0) has0 = 1; }
    if (blocked) { if (probs->n) sb_puts(probs, ", "); sb_puts(probs, "magnet hole blocked"); }
    if (ncen > 1 && !has0 && !mesh_contains(&m, mn, mx, 0, 0, 1.0)) { if (probs->n) sb_puts(probs, ", "); sb_puts(probs, "hole in the middle of the bottom"); }
    if (!probs->n && cfg_num("min_difference_pct", 15) > 0) { cb_set_size(rs->SIZE); *Hn = cb_heightmap(&m); }
    free(comp); free(vol); mesh_free(&m);
}

static Bridge *rs_bridge(RunSet *rs) {          /* run_set._Server: a free helper, or a new one */
    Bridge *b = NULL;
    EnterCriticalSection(&rs->lock);
    while (rs->nfree && !b) { Bridge *c = &rs->free_br[--rs->nfree]; if (!c->dead && WaitForSingleObject(c->proc, 0) == WAIT_TIMEOUT) { b = malloc(sizeof *b); *b = *c; } else bridge_stop(c); }
    LeaveCriticalSection(&rs->lock);
    if (b) return b;
    b = malloc(sizeof *b); if (!bridge_start(b, "build", rs->style, rs->envw)) b->dead = 1;
    return b;
}
static void rs_give_back(RunSet *rs, Bridge *b) {
    EnterCriticalSection(&rs->lock);
    if (b->dead) bridge_stop(b);
    else { if (rs->nfree == rs->capfree) { rs->capfree = rs->capfree * 2 + 8; rs->free_br = realloc(rs->free_br, sizeof(Bridge) * rs->capfree); } rs->free_br[rs->nfree++] = *b; }
    LeaveCriticalSection(&rs->lock); free(b);
}

/* run_set.one_base */
static void one_base(RunSet *rs, int n, BaseRes *R) {
    memset(R, 0, sizeof *R); nm(rs, n, R->base, sizeof R->base); R->n = n; R->detail = (n - 1) % 20 / 5 + 1;
    int attempts = (int)cfg_num("max_attempts", 4); if (attempts > 8) attempts = 8; if (attempts < 1) attempts = 1;
    for (int k = 0; k < attempts; k++) {
        if (rs->stop || runner_stop) break;
        const char *v = VARIANTS[k];
        EnterCriticalSection(&rs->lock);
        if (k == 0) rs_print("  >> %s  building", R->base); else rs_print("  >> %s  retry with fresh variant %s", R->base, v);
        ev(k == 0 ? "start" : "retry", "si", "seed", rs->salt, "n", n);          /* gui_job: the ">>" line */
        LeaveCriticalSection(&rs->lock);
        Bridge *br = rs_bridge(rs); Built B; build_once(rs, br, n, v, &B); rs_give_back(rs, br);
        char stl[MAX_PATH * 3]; snprintf(stl, sizeof stl, "%s\\%s\\%s.stl", rs->out, rs->style, R->base);
        Sb reason = { 0 }; if (B.error) sb_puts(&reason, B.error);
        int seam = 0; char diff[16] = "";
        if (!B.error) {
            const char *kv[] = { "stage", "checks" }; live_note(rs->salt, n, 1, kv);
            Sb probs = { 0 }; int specks; float *Hn = NULL; WCHAR ws[MAX_PATH * 3]; wpath(stl, ws, MAX_PATH * 3);
            if (GetFileAttributesW(ws) == INVALID_FILE_ATTRIBUTES) sb_puts(&probs, "no STL written");
            else {
                char er[200]; int sk2 = 0;
                if (specks_and_seams(stl, &sk2, &seam, er, sizeof er) < 0) { char t[300]; seam = 0; snprintf(t, sizeof t, "checker error: %.80s", er); sb_puts(&probs, t); }
                else print_check(rs, stl, &probs, &specks, &Hn);
            }
            if (!probs.n && Hn) {          /* must differ enough from the last ones of its style */
                float *Rn = cb_rotations(Hn, 72); int N = CB_N; unsigned char *disc = malloc((size_t)N * N); double *xs = malloc(sizeof(double) * N); linspace(-CB_R, CB_R, N, xs);
                for (int i = 0; i < N; i++) for (int j = 0; j < N; j++) disc[i * N + j] = xs[i] * xs[i] + xs[j] * xs[j] <= (CB_R - 0.3) * (CB_R - 0.3);
                EnterCriticalSection(&rs->lock); int last = (int)cfg_num("compare_last", 10), from = rs->nrecent > last ? rs->nrecent - last : 0; double best = 100.0; char who[160] = "";
                int first = 1;
                for (int i = from; i < rs->nrecent; i++) { double s = cb_score(Rn, 72, rs->recent[i].H, disc); if (first || s < best || (s == best && strcmp(rs->recent[i].name, who) < 0)) { best = s; snprintf(who, sizeof who, "%s", rs->recent[i].name); first = 0; } }
                LeaveCriticalSection(&rs->lock);
                snprintf(diff, sizeof diff, "%d", (int)py_rint(best));
                if (best < cfg_num("min_difference_pct", 15)) { char t[300]; snprintf(t, sizeof t, "too similar to %s (%.0f%% different)", who, best); sb_puts(&probs, t); free(Hn); }
                else { EnterCriticalSection(&rs->lock); rs->recent = realloc(rs->recent, sizeof(Recent) * (rs->nrecent + 1)); snprintf(rs->recent[rs->nrecent].name, 160, "%s", R->base); rs->recent[rs->nrecent++].H = Hn; LeaveCriticalSection(&rs->lock); }
                free(Rn); free(disc); free(xs);
            } else free(Hn);
            JV *want = jget(&B.head, "want_mm");
            if (!probs.n && want && want->d >= 1.5 && B.top_mm < 0.5 * want->d) { char t[300], a[32], b[32]; runner_float_repr(B.top_mm, a, 32); runner_float_repr(want->d, b, 32); snprintf(t, sizeof t, "the top came out flat (%s mm high, designed %s mm): objects lost", a, b); sb_puts(&probs, t); }
            int ndrop = 0; for (size_t i = 0; i < B.skipped.n; i++) if (B.skipped.s[i] == '\n') ndrop++; if (B.skipped.n) ndrop++;
            if (probs.n) { if (strncmp(probs.s, "too similar", 11)) sb_puts(&reason, "print check: "); sb_puts(&reason, probs.s); }
            else if (ndrop > cfg_num("max_dropped_pieces", 4)) { char t[64]; snprintf(t, sizeof t, "%d pieces dropped", ndrop); sb_puts(&reason, t); }
            free(probs.s);
        }
        WCHAR ws[MAX_PATH * 3]; wpath(stl, ws, MAX_PATH * 3);
        if (reason.n && GetFileAttributesW(ws) != INVALID_FILE_ATTRIBUTES) {          /* the failed attempt is kept for a look */
            char fd[MAX_PATH * 3], dst[MAX_PATH * 3], s2[MAX_PATH * 3]; snprintf(fd, sizeof fd, "%s\\%s\\failed", rs->out, rs->style); make_dirs(fd);
            WCHAR a[MAX_PATH * 3], b[MAX_PATH * 3];
            snprintf(dst, sizeof dst, "%s\\%s_%s.stl", fd, R->base, v[0] ? v : "original"); wpath(dst, b, MAX_PATH * 3); MoveFileExW(ws, b, MOVEFILE_REPLACE_EXISTING);
            snprintf(s2, sizeof s2, "%.*s_2d.png", (int)strlen(stl) - 4, stl); snprintf(dst, sizeof dst, "%s\\%s_%s_2d.png", fd, R->base, v[0] ? v : "original"); wpath(s2, a, MAX_PATH * 3); wpath(dst, b, MAX_PATH * 3); MoveFileExW(a, b, MOVEFILE_REPLACE_EXISTING);
        }
        JV *plan = jget(&B.head, "plan");
        if (!reason.n && plan && plan->t == 6 && plan->n) {          /* the scene plan beside the STL */
            char pj[MAX_PATH * 3]; snprintf(pj, sizeof pj, "%.*s_plan.json", (int)strlen(stl) - 4, stl); Sb o = { 0 }, crlf = { 0 }; jdump(&o, plan, 1, 0);
            for (size_t i = 0; i < o.n; i++) { if (o.s[i] == '\n') sb_add(&crlf, "\r", 1); sb_add(&crlf, o.s + i, 1); }          /* a text-mode file: CR LF */
            free(o.s); o = crlf;
            WCHAR w[MAX_PATH * 3]; wpath(pj, w, MAX_PATH * 3); FILE *f = _wfopen(w, L"wb"); if (f) { fwrite(o.s, 1, o.n, f); fclose(f); } free(o.s);
        }
        Try *T = &R->tries[R->ntries++]; memset(T, 0, sizeof *T);
        snprintf(T->variant, sizeof T->variant, "%s", v[0] ? v : "original"); T->reason = reason.n ? reason.s : NULL;
        if (B.error) snprintf(T->secs, sizeof T->secs, "%d", (int)B.secs); else runner_float_repr(B.secs, T->secs, sizeof T->secs);
        if (!B.error) { snprintf(T->steps, sizeof T->steps, "%s", jtext(jget(&B.head, "steps"), "")); snprintf(T->theme, sizeof T->theme, "%s", jtext(jget(&B.head, "theme"), "")); }
        JV *pl = jget(&B.head, "plan_line"); T->plan_line = xstrdup(!B.error && pl && pl->s ? pl->s : "");
        if (!B.error && plan && plan->t == 6 && plan->n) { Sb o = { 0 }; jdump(&o, plan, 0, 0); T->plan = o.s; } else T->plan = xstrdup("");
        {   /* ", ".join(sorted(set(skipped))) */
            int ns = 0; char **sv = NULL; Sb d = { 0 };
            for (char *l = B.skipped.s, *e2; l && *l; l = e2) { e2 = strchr(l, '\n'); size_t L = e2 ? (size_t)(e2 - l) : strlen(l); sv = realloc(sv, sizeof(char *) * (ns + 1)); sv[ns] = malloc(L + 1); memcpy(sv[ns], l, L); sv[ns][L] = 0; ns++; if (!e2) break; e2++; }
            for (int i = 1; i < ns; i++) for (int j = i; j > 0 && strcmp(sv[j - 1], sv[j]) > 0; j--) { char *x = sv[j]; sv[j] = sv[j - 1]; sv[j - 1] = x; }
            for (int i = 0; i < ns; i++) { if (i && !strcmp(sv[i], sv[i - 1])) { free(sv[i]); continue; } if (d.n) sb_puts(&d, ", "); sb_puts(&d, sv[i]); free(sv[i]); }
            free(sv); T->dropped = d.s ? d.s : xstrdup("");
        }
        snprintf(T->restarts, sizeof T->restarts, "0"); snprintf(T->note, sizeof T->note, "%s", seam ? "rim seam closed" : ""); snprintf(T->diff, sizeof T->diff, "%s", diff);
        if (!B.error) { Sb md = { 0 }; sb_printf(&md, "mesh: %s; pictures: %s", B.mesher, B.pictures[0] ? B.pictures : "-"); T->made = md.s; } else T->made = xstrdup("");
        {   /* r["errors"]: what went wrong inside the base (kitlog), joined for the report; run_set prints the first */
            JV *er = jget(&B.head, "errors"); Sb j = { 0 };
            for (int i = 0; er && er->t == 5 && i < er->n; i++) { if (i) sb_puts(&j, " | "); sb_puts(&j, er->a[i].s ? er->a[i].s : ""); }
            T->errors = j.s ? j.s : xstrdup("");
            if (er && er->t == 5 && er->n) { JV *ef = jget(&B.head, "errors_file"); EnterCriticalSection(&rs->lock); rs_print("  !    %s: %d error(s) caught while making it, first: %s  (all of them in %s)", R->base, er->n, er->a[0].s ? er->a[0].s : "", ef && ef->s ? ef->s : ""); LeaveCriticalSection(&rs->lock); }
        }
        jfree(&B.head); free(B.error); free(B.skipped.s);
        if (!T->reason) break;
    }
    R->ok = R->ntries && !R->tries[R->ntries - 1].reason;
    live_finish(rs->salt, n);
}
typedef struct { RunSet *rs; } RsWork;
static DWORD WINAPI rs_worker(void *a) {          /* ThreadPoolExecutor(workers): bases in submission order */
    RunSet *rs = ((RsWork *)a)->rs;
    for (;;) {
        LONG i = InterlockedIncrement(&rs->next) - 1; if (i >= rs->nnums) break;
        if (rs->stop || runner_stop) continue;
        BaseRes *R = &rs->res[i]; one_base(rs, rs->nums[i], R);
        EnterCriticalSection(&rs->lock);
        Try *last = &R->tries[R->ntries ? R->ntries - 1 : 0];
        if (R->ok) {
            Sb l = { 0 }; sb_printf(&l, "  OK   %s  %s s", R->base, last->secs);
            if (last->dropped[0]) sb_printf(&l, "  dropped: %s", last->dropped);
            if (last->note[0]) sb_printf(&l, "  (%s)", last->note);
            if (R->ntries > 1) sb_printf(&l, "  (fresh variant after: %s)", R->tries[0].reason);
            if (last->plan_line[0]) sb_printf(&l, "  %s", last->plan_line);
            rs_print("%s", l.s); free(l.s);
        } else {
            rs->flagged++; Sb why = { 0 }; for (int t = 0; t < R->ntries; t++) { if (t) sb_puts(&why, " / "); sb_puts(&why, R->tries[t].reason ? R->tries[t].reason : ""); }
            rs_print("  FLAG %s  failed %d times: %s", R->base, R->ntries, why.s ? why.s : ""); free(why.s);
        }
        if (rs->done) rs->done(rs, R, rs->done_ctx);
        double frac = cfg_num("stop_if_fail_fraction", 0.25) * rs->nnums;
        if (rs->flagged >= (frac > 2 ? frac : 2)) { rs->stop = 1; rs_print("STOPPING: too many failures, something systematic is wrong"); }
        if ((GetTickCount64() - rs->t0) / 1000.0 > cfg_num("batch_limit_mins", 45) * 60) { rs->stop = 1; rs_print("STOPPING: batch time limit reached"); }
        rs->nres++;
        LeaveCriticalSection(&rs->lock);
    }
    return 0;
}
static void csv_field(Sb *b, const char *s) {          /* csv.writer, QUOTE_MINIMAL */
    if (strpbrk(s, ",\"\r\n")) { sb_puts(b, "\""); for (const char *c = s; *c; c++) { if (*c == '"') sb_puts(b, "\""); sb_add(b, c, 1); } sb_puts(b, "\""); }
    else sb_puts(b, s);
}
static int res_cmp(const void *a, const void *b) { return strcmp(((const BaseRes *)a)->base, ((const BaseRes *)b)->base); }
/* run_set.run_style: the bases on `workers` threads, the report CSV */
static void run_style(RunSet *rs) {
    rs->t0 = GetTickCount64(); rs->res = calloc(rs->nnums + 1, sizeof(BaseRes));
    rs_print("%s: %d bases, %s mm, %d at a time, limits %s s/step, %.0f s/base, %s min/batch", rs->style, rs->nnums, rs->size_tag, rs->workers,
             jtext(cfg_get("step_limit_secs"), "12"), base_limit(rs), jtext(cfg_get("batch_limit_mins"), "45"));
    int nw = rs->workers < rs->nnums ? rs->workers : rs->nnums; HANDLE *th = calloc(nw + 1, sizeof(HANDLE)); RsWork w = { rs };
    for (int i = 0; i < nw; i++) th[i] = CreateThread(NULL, 1 << 22, rs_worker, &w, 0, NULL);
    for (int i = 0; i < nw; i++) { WaitForSingleObject(th[i], INFINITE); CloseHandle(th[i]); }
    free(th);
    BaseRes *done = calloc(rs->nnums + 1, sizeof(BaseRes)); int nd = 0; for (int i = 0; i < rs->nnums; i++) if (rs->res[i].ntries || rs->res[i].base[0]) done[nd++] = rs->res[i];
    qsort(done, nd, sizeof(BaseRes), res_cmp);
    char rep[MAX_PATH * 3]; snprintf(rep, sizeof rep, "%s\\%s_report.csv", rs->out, rs->style);
    Sb c = { 0 }; sb_puts(&c, "base,result,attempt,variant,build secs,failure reason,dropped pieces,restarts,note,steps,slowest steps,% different from closest of last 10,detail level,theme,made by,errors caught,plan from,plan\r\n");
    int ok = 0;
    for (int i = 0; i < nd; i++) {
        BaseRes *r = &done[i]; ok += r->ok;
        for (int t = 0; t < r->ntries; t++) {
            Try *T = &r->tries[t]; char at[8], det[8]; snprintf(at, sizeof at, "%d", t + 1); snprintf(det, sizeof det, "%d", r->detail);
            const char *res = !T->reason ? "OK" : (t == r->ntries - 1 && !r->ok) ? "FLAGGED" : "retried";
            const char *f[18] = { r->base, res, at, T->variant, T->secs, T->reason ? T->reason : "", T->dropped, T->restarts, T->note, T->steps, T->slowest, T->diff, det, T->theme, T->made, T->errors ? T->errors : "", T->plan_line, T->plan };
            for (int k = 0; k < 18; k++) { if (k) sb_puts(&c, ","); csv_field(&c, f[k]); }
            sb_puts(&c, "\r\n");
        }
    }
    WCHAR w2[MAX_PATH * 3]; wpath(rep, w2, MAX_PATH * 3); FILE *f = _wfopen(w2, L"wb"); if (f) { fwrite(c.s, 1, c.n, f); fclose(f); } free(c.s);
    if (nd > 0) {          /* run_set.sheet: the <style>_sheet.png contact sheet (meshglue.h mg_sheet: matplotlib's layout, our own pixels) */
        MgSheetItem *it = calloc(nd + 1, sizeof(MgSheetItem)); char **titles = calloc(nd + 1, sizeof(char *)); unsigned char **px = calloc(nd + 1, sizeof(unsigned char *));
        for (int i = 0; i < nd; i++) {
            BaseRes *r = &done[i]; Try *last = r->ntries ? &r->tries[r->ntries - 1] : NULL; char png[MAX_PATH * 3], det[16]; int w = 0, h = 0;
            snprintf(png, sizeof png, "%s\\%s\\%s_iso.png", rs->out, rs->style, r->base); snprintf(det, sizeof det, "%d", r->detail);
            if (r->ok && file_exists(png)) px[i] = d2_png_read_rgba(png, &w, &h);
            titles[i] = malloc(400); mg_sheet_title(r->base, det, r->ok, last && strcmp(last->variant, "original") != 0, titles[i], 400);
            it[i].rgba = px[i]; it[i].w = w; it[i].h = h; it[i].title = titles[i]; it[i].ok = r->ok;
        }
        int SW = 0, SH = 0; unsigned char *sheet = mg_sheet(it, nd, &SW, &SH, NULL);
        if (sheet) { char sp[MAX_PATH * 3]; snprintf(sp, sizeof sp, "%s\\%s_sheet.png", rs->out, rs->style); d2_png_write(sp, sheet, SW, SH, 4); free(sheet); }
        for (int i = 0; i < nd; i++) { free(px[i]); free(titles[i]); }
        free(it); free(titles); free(px);
    }
    char mins[32]; runner_float_repr(py_round((GetTickCount64() - rs->t0) / 60000.0, 1), mins, sizeof mins);
    rs_print("DONE %s: %d/%d OK, %d flagged, %s min. Seed: %s. Report: %s", rs->style, ok, rs->nnums, (int)rs->flagged, mins, rs->salt, rep);
    free(done);
}

/* ---- gui_job.py build: run_set per seed, every OK base copied into out\picked_<date>_<time> ---- */
typedef struct { const char *style, *size, *seed, *dest; double scale; int *seen, okc, bad; } PickCtx;
static void size_tag_gui(const char *txt, char *out, size_t cap) {          /* gui_job.size_tag */
    double a, b; int ov; read_size(txt, &a, &b, &ov); char x[32], y[32]; fmt_g(x, sizeof x, a); fmt_g(y, sizeof y, b);
    if (a == b) snprintf(out, cap, "%s", x); else snprintf(out, cap, "%sx%s", x, y);
}
static void copy_file(const char *a, const char *b) { WCHAR x[MAX_PATH * 3], y[MAX_PATH * 3]; wpath(a, x, MAX_PATH * 3); wpath(b, y, MAX_PATH * 3); CopyFileW(x, y, FALSE); }
static int file_exists(const char *p) { WCHAR w[MAX_PATH * 3]; wpath(p, w, MAX_PATH * 3); return GetFileAttributesW(w) != INVALID_FILE_ATTRIBUTES; }
static void picked_done(RunSet *rs, BaseRes *R, void *vctx) {
    PickCtx *P = vctx; int n = R->n; P->seen[n] = 1; Try *last = &R->tries[R->ntries ? R->ntries - 1 : 0];
    char stl[MAX_PATH * 3]; snprintf(stl, sizeof stl, "%s\\%s\\%s.stl", rs->out, P->style, R->base);
    if (R->ok && file_exists(stl)) {
        const char *theme = ""; char th[256] = "";
        const char *pl = strstr(last->plan_line, "plan: ");          /* re.search(r'plan: \w+ "([^"]*)"', rest) */
        for (; pl; pl = strstr(pl + 1, "plan: ")) {
            const char *q = pl + 6; const char *w0 = q; while (*q && (isalnum((unsigned char)*q) || *q == '_' || (unsigned char)*q >= 0x80)) q++;
            if (q > w0 && q[0] == ' ' && q[1] == '"') { const char *e = strchr(q + 2, '"'); if (e) { snprintf(th, sizeof th, "%.*s", (int)(e - q - 2), q + 2); theme = th; break; } }
        }
        char tag[256]; int j = 0, us = 0;
        for (const char *c = theme; *c && j < 250; c++) { char ch = (char)tolower((unsigned char)*c); if ((ch >= 'a' && ch <= 'z') || (ch >= '0' && ch <= '9')) { tag[j++] = ch; us = 0; } else if (!us) { tag[j++] = '_'; us = 1; } }
        tag[j] = 0; char *tg = tag; while (*tg == '_') tg++; size_t tl = strlen(tg); while (tl && tg[tl - 1] == '_') tg[--tl] = 0;
        char st[64], nw[600]; size_tag_gui(P->size, st, sizeof st);
        snprintf(nw, sizeof nw, "%s_%smm_%s%s%s_%d", P->style, st, tg, tl ? "_" : "", P->seed, n);
        char dstl[MAX_PATH * 3], img[MAX_PATH * 3] = "", img2[MAX_PATH * 3] = "", img3c[MAX_PATH * 3] = "", t[MAX_PATH * 3], er[300];
        snprintf(dstl, sizeof dstl, "%s\\%s.stl", P->dest, nw); copy_file(stl, dstl);
        snprintf(t, sizeof t, "%.*s_plan.json", (int)strlen(stl) - 4, stl); if (file_exists(t)) { char d[MAX_PATH * 3]; snprintf(d, sizeof d, "%s\\%s_plan.json", P->dest, nw); copy_file(t, d); }
        snprintf(img, sizeof img, "%s\\%s_3d.png", P->dest, nw);
        {   /* with_figure: the classic look is the base alone; a figure scale adds a plain figure beside it (meshglue.h mg_figure) */
            int okp = 0; Mesh soup; char by[200];
            if (stl_read(stl, &soup)) {
                char tmp[MAX_PATH * 3]; snprintf(tmp, sizeof tmp, "%.*s_raw.png", (int)strlen(img) - 4, img);
                int W = strchr(P->size, 'x') ? 1100 : 900, Hh = strchr(P->size, 'x') ? 620 : 700;
                if (P->scale > 1.01) {          /* concatenate([m, fig]) */
                    double r = -INFINITY; MgMesh fig = { 0 }, all;
                    for (int i = 0; i < soup.nv; i++) if (soup.v[3 * i] > r) r = soup.v[3 * i];
                    if (mg_figure(P->scale, r, &fig) == 0) {
                        all.v = soup.v; all.nv = soup.nv; all.f = soup.f; all.nt = soup.nt; mg_concat(&all, &fig, 1);
                        soup.v = all.v; soup.nv = all.nv; soup.f = all.f; soup.nt = all.nt;
                    }
                    mg_mesh_free(&fig);
                }
                if (shot_png(&soup, tmp, W, Hh, 34, -60, by, sizeof by) && d2_see_through(tmp, img, 480, er, sizeof er)) okp = 1;
                WCHAR wt[MAX_PATH * 3]; wpath(tmp, wt, MAX_PATH * 3); DeleteFileW(wt);
                {   /* the colour view: the same picture, coloured as the 2D drawing (the window's toggle on the card) */
                    char p2[MAX_PATH * 3]; double win[4]; int tw, th; snprintf(p2, sizeof p2, "%.*s_2d.png", (int)strlen(stl) - 4, stl);
                    unsigned char *px4 = (okp && load_win(p2, win)) ? d2_png_read_rgba(p2, &tw, &th) : NULL;
                    if (px4) {
                        for (size_t q = 0; q < (size_t)tw * th; q++) { px4[3 * q] = px4[4 * q]; px4[3 * q + 1] = px4[4 * q + 1]; px4[3 * q + 2] = px4[4 * q + 2]; }
                        char cpath[MAX_PATH * 3], by2[200]; snprintf(cpath, sizeof cpath, "%s\\%s_3d_color.png", P->dest, nw);
                        if (shot_png_col(&soup, tmp, W, Hh, 34, -60, px4, tw, th, win, by2, sizeof by2) && d2_see_through(tmp, cpath, 480, er, sizeof er)) snprintf(img3c, sizeof img3c, "%s", cpath);
                        else { char msg[400]; snprintf(msg, sizeof msg, "no colour picture for %s: %s", R->base, by2[0] ? by2 : er); ev("log", "s", "text", msg); }
                        DeleteFileW(wt); free(px4);
                    }
                }
                mesh_free(&soup);
            }
            if (!okp) {
                char msg[400]; snprintf(msg, sizeof msg, "no picture with a figure for %s: %s", R->base, "RuntimeError: the picture could not be drawn");
                ev("log", "s", "text", msg);
                snprintf(t, sizeof t, "%.*s_iso.png", (int)strlen(stl) - 4, stl); if (file_exists(t)) d2_see_through(t, img, 480, er, sizeof er);
            }
        }
        snprintf(t, sizeof t, "%.*s_2d.png", (int)strlen(stl) - 4, stl);
        if (file_exists(t)) {
            char d[MAX_PATH * 3], cd[MAX_PATH * 3]; snprintf(d, sizeof d, "%s\\%s_2d.png", P->dest, nw); copy_file(t, d);
            snprintf(cd, sizeof cd, "%s\\choices", out_root()); make_dirs(cd);
            snprintf(img2, sizeof img2, "%s\\built_%s_2d.png", cd, nw); d2_see_through(t, img2, 360, er, sizeof er);
        }
        P->okc++;
        double secs = strtod(last->secs, NULL);
        ev("built", "sibbssssfs", "seed", P->seed, "n", n, "ok", 1, "variant", R->ntries > 1, "stl", dstl, "img", img, "img2", img2, "img3c", img3c, "secs", secs, "theme", theme);
    } else {
        P->bad++;
        if (!R->ok) {
            Sb why = { 0 }; for (int t = 0; t < R->ntries; t++) { if (t) sb_puts(&why, " / "); sb_puts(&why, R->tries[t].reason ? R->tries[t].reason : ""); }
            char w[201]; snprintf(w, sizeof w, "%.200s", why.s ? why.s : ""); ev("built", "sibs", "seed", P->seed, "n", n, "ok", 0, "why", w); free(why.s);
        } else ev("built", "sibs", "seed", P->seed, "n", n, "ok", 0, "why", "the runner said OK but there is no STL file");
    }
}
static int job_build(Args *a, const char *style) {
    const char *size = opt(a, "--size", "30"), *mini = opt(a, "--mini", ""), *sup = opt(a, "--supports", ""), *picks = opt(a, "--picks", ""), *scale_s = opt(a, "--scale", "");
    /* the picks: seed:number, grouped by seed in the order they first come */
    char *seeds[256]; int *nums[256], nn[256], ng = 0; char *pk = xstrdup(picks);
    for (char *part = strtok(pk, ","); part; part = strtok(NULL, ",")) {
        char *c = strchr(part, ':'); if (!c) continue; *c = 0; const char *ns = c + 1;
        if (!*ns || strspn(ns, "0123456789") != strlen(ns)) continue;
        int g = -1; for (int i = 0; i < ng; i++) if (!strcmp(seeds[i], part)) g = i;
        if (g < 0 && ng < 256) { g = ng++; seeds[g] = xstrdup(part); nums[g] = malloc(sizeof(int) * 1024); nn[g] = 0; }
        if (g >= 0 && nn[g] < 1024) nums[g][nn[g]++] = atoi(ns);
    }
    free(pk);
    if (!ng) { ev("error", "s", "text", "nothing was picked"); return 2; }
    char ts[32], dest[MAX_PATH * 3]; strftime_now(ts, sizeof ts, "%Y%m%d_%H%M%S"); snprintf(dest, sizeof dest, "%s\\picked_%s", out_root(), ts); make_dirs(dest);
    ev("folder", "s", "folder", dest);
    snprintf(LIVE_DIR, sizeof LIVE_DIR, "%s\\.live", out_root()); make_dirs(LIVE_DIR);          /* gui_job: KIT_LIVE for the window's live view */
    int okc = 0, bad = 0;
    for (int g = 0; g < ng; g++) {
        /* run_set.py <style> 16,17, --seed x --size 60 -q [--mini --supports --scale] */
        RunSet rs; memset(&rs, 0, sizeof rs); InitializeCriticalSection(&rs.lock); rs.style = style; rs.salt = seeds[g];
        double A, B; int ov; read_size(size, &A, &B, &ov); rs.SIZE = A; rs.oval = ov; rs.OVAL[0] = A; rs.OVAL[1] = B;
        if (rs.oval) { char x[32], y[32]; fmt_g(x, sizeof x, A); fmt_g(y, sizeof y, B); snprintf(rs.size_tag, sizeof rs.size_tag, "%sx%s", x, y); } else fmt_g(rs.size_tag, sizeof rs.size_tag, A);
        int known = 0; for (int i = 0; i < 15; i++) if (KNOWN_SIZES[i] == A) known = 1;
        int okv = 0; for (int i = 0; i < 7; i++) if (OVAL_SIZES[i][0] == A && OVAL_SIZES[i][1] == B) okv = 1;
        if (rs.oval && !okv) rs_print("note: %s mm is not one of the common oval sizes (60x35.5, 75x42, 90x52, 105x70, 120x92, 150x95, 170x109); building it anyway", rs.size_tag);
        if (!rs.oval && !known) rs_print("note: %s mm is not one of the common base sizes (25, 28, 28.5, 30, 32, 40, 50, 60, 65, 70, 80, 90, 100, 130, 160); building it anyway", rs.size_tag);
        rs.SCALE = scale_s[0] ? strtod(scale_s, NULL) : cfg_num("scenery_scale", 1.0);
        if (rs.SCALE != 1.0 && figure_scale(A, rs.SCALE) == 1.0) { char s1[32], f1[32]; fmt_g(s1, sizeof s1, rs.SCALE); fmt_g(f1, sizeof f1, cfg_num("figure_scale_from_mm", 50)); rs_print("scale  : x%s asked, but a %s mm base is under %s mm: built in the classic look, with its detail", s1, rs.size_tag, f1); rs.SCALE = 1.0; }
        rs.LAYOUT = rs.SCALE == 1.0 ? A : py_round(fmax(30.0, A / rs.SCALE), 1);
        char lay[32], fin[32], skl[32]; fmt_g(lay, sizeof lay, rs.LAYOUT); fmt_g(fin, sizeof fin, A); fmt_g(skl, sizeof skl, rs.SCALE);
        if (rs.SCALE != 1.0) {
            char extra[128] = ""; if (!(rs.LAYOUT * rs.SCALE <= A + 0.5)) snprintf(extra, sizeof extra, " (it shows the middle %.0f mm of that layout)", A / rs.SCALE);
            rs_print("scale  : x%s: laid out as a %s mm base, built %s times larger on the %s mm base%s", skl, lay, skl, rs.size_tag, extra);
        }
        if (rs.oval) { char x[32], y[32]; fmt_g(x, sizeof x, A); fmt_g(y, sizeof y, B); rs_print("oval   : %s x %s mm: the scene is laid out round on the long size and cut to the oval", x, y); }
        char outf[MAX_PATH * 3]; snprintf(outf, sizeof outf, "%s", out_root());
        { const char *of = cfg_str("out_folder", "out"); if (of[0] && (of[1] == ':' || of[0] == '\\')) snprintf(outf, sizeof outf, "%s", of); }          /* "out": beside the exe; an absolute one in settings.json wins */
        int th = threads_kit(); char core[96];          /* run_set.engine_text */
        if (th > 1) snprintf(core, sizeof core, "height field + mesh in C (%d threads)", th); else snprintf(core, sizeof core, "height field + mesh in C (1 thread)");
        rs_print("kit    : %s\nengine : mesh, layout in C, %s, %s\nout    : %s", KIT, core, gpu_mode() ? "pictures on the video card" : "pictures in software", outf);
        if (cfg_bool("always_new", 1)) { char t2[32]; strftime_now(t2, sizeof t2, "%Y%m%d_%H%M%S"); snprintf(rs.out, sizeof rs.out, "%s\\run_%s_%s", outf, t2, rs.salt); }
        else snprintf(rs.out, sizeof rs.out, "%s", outf);
        make_dirs(rs.out);
        rs_print("seed   : %s   folder: %s", rs.salt, rs.out);
        rs.workers = cfg_bool("live", 0) ? 1 : 0;
        if (!rs.workers) { JV *w = cfg_get("workers"); if (!w || (w->t == 4 && (!strcmp(w->s, "auto") || !w->s[0] || !strcmp(w->s, "0"))) || (w->t == 3 && w->d == 0)) { int c = (int)GetActiveProcessorCount(ALL_PROCESSOR_GROUPS) / 3; rs.workers = c < 2 ? 2 : c > 12 ? 12 : c; } else rs.workers = w->t == 3 ? (int)w->d : atoi(w->s); if (rs.workers < 1) rs.workers = 1; }
        rs.nums = nums[g]; rs.nnums = nn[g];
        /* the layout helpers' environment: gui_job's + run_set's + build_once's */
        EnvSet es[24]; int ne = 0;
        es[ne++] = (EnvSet){ "PYTHONWARNINGS", "ignore" }; es[ne++] = (EnvSet){ "PYTHONUNBUFFERED", "1" }; es[ne++] = (EnvSet){ "PYTHONIOENCODING", "utf-8" };
        es[ne++] = (EnvSet){ "KIT_LIVE", LIVE_DIR }; es[ne++] = (EnvSet){ "SALT", rs.salt }; es[ne++] = (EnvSet){ "BASE_MM", lay }; es[ne++] = (EnvSet){ "SETDIR", rs.out };
        es[ne++] = (EnvSet){ "KIT_QUIET", "1" }; es[ne++] = (EnvSet){ "KIT_WORKER", "1" }; es[ne++] = (EnvSet){ "KIT_OVAL", rs.oval ? rs.size_tag : NULL };
        if (rs.SCALE != 1.0) { es[ne++] = (EnvSet){ "KIT_FINAL_MM", fin }; es[ne++] = (EnvSet){ "KIT_SCALE", skl }; }
        if (mini[0]) es[ne++] = (EnvSet){ "KIT_MINI", mini };
        if (sup[0]) es[ne++] = (EnvSet){ "KIT_SUPPORTS", !strcmp(sup, "on") ? "1" : "0" };
        rs.envw = env_block(es, ne);
        SUP_CLI = !strcmp(sup, "on") ? 1 : !strcmp(sup, "off") ? 0 : -1;          /* --supports: CFG allow_supports for the checks */
        int *seen = calloc(100000, sizeof(int)); PickCtx P = { style, size, rs.salt, dest, rs.SCALE, seen, 0, 0 };
        if (scale_s[0]) P.scale = strtod(scale_s, NULL); else P.scale = cfg_num("scenery_scale", 1.0);
        rs.done = picked_done; rs.done_ctx = &P;
        run_style(&rs);
        for (int i = 0; i < rs.nfree; i++) bridge_stop(&rs.free_br[i]);
        for (int i = 0; i < rs.nnums; i++) if (!seen[rs.nums[i]]) { P.bad++; char w[120]; snprintf(w, sizeof w, "the build ended (code 0) before this base was done"); ev("built", "sibs", "seed", rs.salt, "n", rs.nums[i], "ok", 0, "why", w); }
        okc += P.okc; bad += P.bad;
        free(seen); free(rs.envw); free(rs.res); for (int i = 0; i < rs.nrecent; i++) free(rs.recent[i].H); free(rs.recent); free(rs.free_br); DeleteCriticalSection(&rs.lock);
    }
    ev("done", "siis", "job", "build", "ok", okc, "failed", bad, "folder", dest);
    for (int g = 0; g < ng; g++) { free(seeds[g]); free(nums[g]); }
    return 0;
}

/* ================================================================ runner_main: gui_job.py */
int runner_main(int argc, char **argv, runner_out_fn out, void *ctx) {
    if (!OUTL_ready) { InitializeCriticalSection(&OUTL); OUTL_ready = 1; }
    OUTF = out; OUTC = ctx; set_kit(); load_cfg(); pyx_load();          /* the UCRT cos / sin (magnet spots, compare_bases rotations) */
    char **av = malloc(sizeof(char *) * (argc + 1)); for (int i = 0; i < argc; i++) av[i] = argv[i];
    Args a = { argc, av };
    if (a.argc < 2 || (strcmp(a.argv[0], "choices") && strcmp(a.argv[0], "build"))) { ev("error", "s", "text", "usage: gui_job.py choices|build <style> ..."); free(av); return 2; }
    char job[16], style[64]; snprintf(job, sizeof job, "%s", a.argv[0]); snprintf(style, sizeof style, "%s", a.argv[1]);
    int rc;
    if (!strcmp(job, "choices")) rc = job_choices(&a, style);
    else rc = job_build(&a, style);
    free(av);
    return rc;
}
