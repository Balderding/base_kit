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
#include "mfjoin.h"
#include "draw2d.h"
#include "pyx.h"
#include "magnets.h"
#include "meshglue.h"
#include "features.h"
#include "grain.h"

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
    " \"scenery_scale\": 1, \"object_size\": 3, \"model_room\": 1, \"drawing_2d\": true, \"models_download\": true, \"no_bare_ground\": true, \"style_details\": true,"
    " \"style_details_amount\": 0.5, \"objects_amount\": 1, \"cpu_threads\": true, \"video_card\": true, \"max_slenderness\": 3.0, \"miniature\": \"biped\","
    " \"smooth_details\": false, \"organic_rocks\": true}";
static JV CFG_DEF;
static void load_cfg(void) {
    static int done; if (done) return; done = 1;
    jparse(DEFAULT_SETTINGS, &CFG_DEF);
    char p[MAX_PATH * 3]; snprintf(p, sizeof p, "%s\\settings.json", KIT);
    char *t = read_file(p, NULL);          /* only beside the exe: a built-in copy is DEFAULT_SETTINGS */
    if (t) { const char *s = t; if ((unsigned char)s[0] == 0xef) s += 3; jparse(s, &CFG); free(t); }
}
static JV *cfg_get(const char *k) { JV *v = jget(&CFG, k); return v ? v : jget(&CFG_DEF, k); }
static int workers_auto(void);          /* below: the "workers" setting */
static double cfg_num(const char *k, double d) { JV *v = cfg_get(k); return v && v->t == 3 ? v->d : v && (v->t == 1 || v->t == 2) ? v->t == 2 : d; }
static int cfg_bool(const char *k, int d) { JV *v = cfg_get(k); if (!v) return d; if (v->t == 3) return v->d != 0; if (v->t == 4) return v->s[0] != 0; return v->t == 2; }
static const char *cfg_str(const char *k, const char *d) { JV *v = cfg_get(k); return v && v->t == 4 ? v->s : d; }
/* run_set.py --supports on|off: CFG["allow_supports"] = a.supports == "on" for this run's checks (-1: not given, settings.json decides) */
static int SUP_CLI = -1;
static int allow_supports(void) { return SUP_CLI >= 0 ? SUP_CLI : cfg_bool("allow_supports", 0); }
static int JOIN_CLI = -1;          /* --join on|off: supports on, the shells joined into the base as one solid (mfjoin.h, Manifold) */
static int supports_join(void) { return JOIN_CLI >= 0 ? JOIN_CLI : cfg_bool("supports_join", 1); }
static int SMALL_CLI = -1, STRICT_CLI = -1;          /* --smaller on|off, --strict on|off (mfjoin.h, Manifold) */
static int smaller_files(void) { return SMALL_CLI >= 0 ? SMALL_CLI : cfg_bool("smaller_files", 1); }
static int strict_check(void) { return STRICT_CLI >= 0 ? STRICT_CLI : cfg_bool("strict_check", 1); }
static int on_off(const char *v) { return !strcmp(v, "on") ? 1 : !strcmp(v, "off") ? 0 : -1; }
static int SMOOTH_CLI = -1;          /* --smooth on|off: smooth details (cport\smooth3d.c), settings "smooth_details" */
static int smooth_details(void) { return SMOOTH_CLI >= 0 ? SMOOTH_CLI : cfg_bool("smooth_details", 0); }
static void theme_arg(const char *s, char *out, size_t cap) { size_t i; snprintf(out, cap, "%s", s); for (i = 0; out[i]; i++) if (out[i] == '+') out[i] = ' '; }          /* --theme fenced+plot: the window joins the words with + */
static double STRENGTH_CLI = -1;          /* --strength x: style strength (settings "style_strength", 0.25 to 2.5, 1 standard) */
static double strength(void) { double v = STRENGTH_CLI > 0 ? STRENGTH_CLI : cfg_num("style_strength", 1.0); return v >= 0.25 && v <= 2.5 ? v : 1.0; }
static int ROCKS_CLI = -1;          /* --rocks on|off: organic rocks (cport\rocks3d.c), settings "organic_rocks" */
static int organic_rocks(void) { return ROCKS_CLI >= 0 ? ROCKS_CLI : cfg_bool("organic_rocks", 1); }          /* on by default since 2026-11-21 (plan B step 4) */
/* shells to join into the base: supports on (with its join switch), or smooth details / organic rocks with supports off */
static int join_shells(void) { return allow_supports() ? supports_join() : (smooth_details() || organic_rocks()); }

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
/* --obj (settings "object_size", default 3 = true beside a 32 mm figure): each detail at its real size on a base laid out at
   its own size (features.c ft_obj_scale). 1 or under: the classic sizes. The old --scale wins when it is given. */
static double obj_of(const char *obj_s, double sc) { double v; if (sc != 1.0) return 0.0; v = obj_s[0] ? strtod(obj_s, NULL) : cfg_num("object_size", 3.0); return v > 1.0 ? v : 0.0; }
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
    int rect = 0; { size_t L = strlen(t); if (L > 2 && !strcmp(t + L - 2, "sq")) { t[L - 2] = 0; rect = 1; } else if (L > 1 && t[L - 1] == 'r' && strchr(t, 'x')) { t[L - 1] = 0; rect = 1; } }          /* 2026-11-22: "25sq", "25x50r" */
    char *x = strchr(t, 'x'); *oval = 0;
    if (x) { *x = 0; double u = strtod(t, NULL), v = strtod(x + 1, NULL); *a = u > v ? u : v; *b = u > v ? v : u; *oval = *a != *b; }
    else { *a = strtod(t, NULL); *b = *a; }
    if (rect && *a > 0) *oval = 2;          /* a square or rectangle: on the oval path, its own outline */
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
    { extern void kinds_init(void); extern char kd_themes_note[600]; kinds_init(); if (kd_themes_note[0]) ev("log", "s", "text", kd_themes_note); }          /* themes.json: say what was taken (review P2-3) */
    const char *size = opt(a, "--size", "30"), *detail = opt(a, "--detail", "mix"), *count_s = opt(a, "--count", "20");
    const char *mini = opt(a, "--mini", ""), *sup = opt(a, "--supports", ""), *scale_s = opt(a, "--scale", ""), *seed_s = opt(a, "--seed", "");
    const char *jobs_s = opt(a, "--jobs", ""), *px_s = opt(a, "--px", "360"), *obj_s = opt(a, "--obj", "");
    /* gui_job.models(): nothing to fetch, the model packs the kit uses are inside the exe (models.c, MODELS_BIN) */
    char ts[32], outdir[MAX_PATH * 3], od[MAX_PATH * 3]; strftime_now(ts, sizeof ts, "%Y%m%d_%H%M%S");
    snprintf(od, sizeof od, "%s\\choices\\%s", out_root(), ts); abs_path(od, outdir, sizeof outdir); make_dirs(outdir);
    /* tools/choices.py */
    int count = atoi(count_s), px = atoi(px_s); double A, B; int oval; read_size(size, &A, &B, &oval);
    double sc = figure_scale(A, scale_s[0] ? strtod(scale_s, NULL) : cfg_num("scenery_scale", 1.0));
    double lay = sc == 1.0 ? A : py_round(fmax(30.0, A / sc), 1);
    int jobs = jobs_s[0] ? atoi(jobs_s) : workers_auto();          /* 2026-11-22: the same cap as the build (the user games meanwhile) */
    char seed[64]; if (seed_s[0]) snprintf(seed, sizeof seed, "%s", seed_s); else new_seed(seed);
    char bm[32], fm[32], sk[32], ov[64]; fmt_g(bm, sizeof bm, lay); fmt_g(fm, sizeof fm, A); fmt_g(sk, sizeof sk, sc);
    if (oval) { char x[32], y[32]; fmt_g(x, sizeof x, A); fmt_g(y, sizeof y, B); snprintf(ov, sizeof ov, "%sx%s%s", x, y, oval == 2 ? "r" : ""); }
    EnvSet es[16]; int ne = 0;
    es[ne++] = (EnvSet){ "PYTHONWARNINGS", "ignore" }; es[ne++] = (EnvSet){ "PYTHONUNBUFFERED", "1" }; es[ne++] = (EnvSet){ "PYTHONIOENCODING", "utf-8" };
    es[ne++] = (EnvSet){ "SALT", seed }; es[ne++] = (EnvSet){ "RESEED", "" }; es[ne++] = (EnvSet){ "BASE_MM", bm }; es[ne++] = (EnvSet){ "KIT_QUIET", "1" };
    if (sc != 1.0) { es[ne++] = (EnvSet){ "KIT_FINAL_MM", fm }; es[ne++] = (EnvSet){ "KIT_SCALE", sk }; }
    char obv[32]; double obd = obj_of(obj_s, sc); fmt_g(obv, sizeof obv, obd); if (obd > 0) es[ne++] = (EnvSet){ "KIT_OBJ", obv };
    es[ne++] = (EnvSet){ "KIT_OVAL", oval ? ov : NULL };
    if (mini[0]) es[ne++] = (EnvSet){ "KIT_MINI", mini };
    if (!strcmp(sup, "on") || !strcmp(sup, "off")) es[ne++] = (EnvSet){ "KIT_SUPPORTS", !strcmp(sup, "on") ? "1" : "0" };
    { const char *sm = opt(a, "--smooth", ""); if (on_off(sm) >= 0) es[ne++] = (EnvSet){ "KIT_SMOOTH", on_off(sm) ? "1" : "0" }; }          /* smooth details: the same layout as the build */
    { const char *rk = opt(a, "--rocks", ""); if (on_off(rk) >= 0) es[ne++] = (EnvSet){ "KIT_ROCKS", on_off(rk) ? "1" : "0" }; }          /* organic rocks: the same */
    static char thc[128]; theme_arg(opt(a, "--theme", ""), thc, sizeof thc); if (thc[0]) es[ne++] = (EnvSet){ "KIT_THEME", thc };          /* only this theme (the window's theme picker) */
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
/* review P2-4 (2026-11-22): the STL holds float32 points, so points a few 1e-6 mm apart become one when it is written
   and a sliver collapses (an edge in 4 faces, a face with two equal corners). Before the check and the write: every
   point rounded to float32, equal points made one, faces with two equal corners dropped, and a face whose exact
   opposite is also there dropped with it (a fold of no thickness). Returns how many faces were dropped. */
static int cmp_tri3(const void *a, const void *b) { const int *x = a, *y = b; for (int k = 0; k < 4; k++) if (x[k] != y[k]) return x[k] < y[k] ? -1 : 1; return 0; }
static int mesh_f32_clean(Mesh *m) {
    int nt0 = m->nt, n = 0, *key; 
    for (int i = 0; i < 3 * m->nv; i++) m->v[i] = (double)(float)m->v[i];
    mesh_merge(m);
    for (int t = 0; t < m->nt; t++) { int a = m->f[3 * t], b = m->f[3 * t + 1], c = m->f[3 * t + 2]; if (a == b || b == c || a == c) continue; m->f[3 * n] = a; m->f[3 * n + 1] = b; m->f[3 * n + 2] = c; n++; }
    m->nt = n;
    key = malloc(sizeof(int) * 4 * (size_t)(n ? n : 1));          /* sorted corners + the face number: opposite pairs side by side */
    for (int t = 0; t < n; t++) { int q[3] = { m->f[3 * t], m->f[3 * t + 1], m->f[3 * t + 2] }, x;
        if (q[0] > q[1]) { x = q[0]; q[0] = q[1]; q[1] = x; } if (q[1] > q[2]) { x = q[1]; q[1] = q[2]; q[2] = x; } if (q[0] > q[1]) { x = q[0]; q[0] = q[1]; q[1] = x; }
        key[4 * t] = q[0]; key[4 * t + 1] = q[1]; key[4 * t + 2] = q[2]; key[4 * t + 3] = t; }
    qsort(key, (size_t)n, sizeof(int) * 4, cmp_tri3);
    {   unsigned char *drop = calloc((size_t)(n ? n : 1), 1); int k = 0;
        for (int i = 0; i + 1 < n; i++) if (key[4 * i] == key[4 * i + 4] && key[4 * i + 1] == key[4 * i + 5] && key[4 * i + 2] == key[4 * i + 6]) {
            int ta = key[4 * i + 3], tb = key[4 * i + 7], *fa = m->f + 3 * ta, *fb = m->f + 3 * tb, same = 0;
            for (int r = 0; r < 3; r++) if (fa[0] == fb[r] && fa[1] == fb[(r + 1) % 3]) same = 1;          /* the same way round: a true duplicate, keep one */
            drop[tb] = 1; if (!same) drop[ta] = 1; i++;
        }
        for (int t = 0; t < n; t++) if (!drop[t]) { memmove(m->f + 3 * k, m->f + 3 * t, sizeof(int) * 3); k++; }
        m->nt = k; free(drop);
    }
    free(key);
    return nt0 - m->nt;
}
/* trimesh.exchange.stl.export_stl: 80 zero bytes, u32 count, per face: the unit normal (triangles.normals: np.cross of
   the edges v1-v0 and v2-v1, made unit by 1/sqrt(x*x + y*y + z*z); zero when not above 1e-13) and the three corners,
   as float32, then u16 0 */
/* loose slivers (2026-11-21): a part of the mesh under 1 mm2 that does not reach the bottom (left by a cut at a detail's edge)
   does not print and makes slicers warn: its faces are left out. Parts are faces joined through edges at the same place. */
static int sl_find(int *p, int i) { while (p[i] != i) { p[i] = p[p[i]]; i = p[i]; } return i; }
static const double *vs_v; static int vs_cmp(const void *a, const void *b) {
    const double *x = vs_v + 3 * (size_t)*(const int *)a, *y = vs_v + 3 * (size_t)*(const int *)b;
    for (int k = 0; k < 3; k++) { if (x[k] < y[k]) return -1; if (x[k] > y[k]) return 1; } return 0;
}
typedef struct { int a, b, f; } SlEdge;
static int sle_cmp(const void *x, const void *y) { const SlEdge *p = x, *q = y; return p->a != q->a ? (p->a < q->a ? -1 : 1) : p->b != q->b ? (p->b < q->b ? -1 : 1) : 0; }
static char *loose_slivers(const Mesh *m) {          /* per face: 1 = leave out (NULL: none); faces joined through shared edges */
    int nv = m->nv, nt = m->nt, i, any = 0, *cid, *ord, *p; double *mz, *ar; char *drop; SlEdge *e;
    if (nv <= 0 || nt <= 0) return NULL;
    ord = malloc(sizeof(int) * (size_t)nv); cid = malloc(sizeof(int) * (size_t)nv);
    for (i = 0; i < nv; i++) ord[i] = i;
    vs_v = m->v; qsort(ord, (size_t)nv, sizeof(int), vs_cmp);
    for (i = 0; i < nv; i++) cid[ord[i]] = (i > 0 && !vs_cmp(&ord[i - 1], &ord[i])) ? cid[ord[i - 1]] : ord[i];          /* one id per corner position */
    e = malloc(sizeof(SlEdge) * 3 * (size_t)nt);
    for (i = 0; i < nt; i++) for (int k = 0; k < 3; k++) { int a = cid[m->f[3 * i + k]], b = cid[m->f[3 * i + (k + 1) % 3]]; e[3 * i + k].a = a < b ? a : b; e[3 * i + k].b = a < b ? b : a; e[3 * i + k].f = i; }
    qsort(e, 3 * (size_t)nt, sizeof(SlEdge), sle_cmp);
    p = malloc(sizeof(int) * (size_t)nt); for (i = 0; i < nt; i++) p[i] = i;
    for (i = 1; i < 3 * nt; i++) if (!sle_cmp(&e[i - 1], &e[i])) { int a = sl_find(p, e[i - 1].f), b = sl_find(p, e[i].f); if (a != b) p[a] = b; }
    mz = malloc(sizeof(double) * (size_t)nt); ar = calloc((size_t)nt, sizeof(double));
    for (i = 0; i < nt; i++) mz[i] = 1e300;
    for (i = 0; i < nt; i++) {
        const double *a = m->v + 3 * m->f[3 * i], *b = m->v + 3 * m->f[3 * i + 1], *c = m->v + 3 * m->f[3 * i + 2]; int r = sl_find(p, i);
        double u0 = b[0] - a[0], u1 = b[1] - a[1], u2 = b[2] - a[2], w0 = c[0] - a[0], w1 = c[1] - a[1], w2 = c[2] - a[2];
        double cx = u1 * w2 - u2 * w1, cy = u2 * w0 - u0 * w2, cz = u0 * w1 - u1 * w0;
        ar[r] += 0.5 * sqrt(cx * cx + cy * cy + cz * cz);
        if (a[2] < mz[r]) mz[r] = a[2]; if (b[2] < mz[r]) mz[r] = b[2]; if (c[2] < mz[r]) mz[r] = c[2];
    }
    drop = calloc((size_t)nt, 1);
    for (i = 0; i < nt; i++) { int r = sl_find(p, i); if (mz[r] > 0.5 && ar[r] < 1.0) { drop[i] = 1; any = 1; } }
    free(p); free(ord); free(cid); free(mz); free(ar); free(e);
    if (!any) { free(drop); return NULL; }
    return drop;
}
static int stl_write(const char *path, const Mesh *m) {
    WCHAR w[MAX_PATH * 2]; wpath(path, w, MAX_PATH * 2); FILE *f = _wfopen(w, L"wb"); if (!f) return 0;
    char *drop = loose_slivers(m); uint32_t nt = 0; for (int i = 0; i < m->nt; i++) nt += !(drop && drop[i]);
    unsigned char hd[84] = { 0 }; memcpy(hd + 80, &nt, 4); fwrite(hd, 1, 84, f);
    unsigned char *buf = malloc((size_t)50 * (m->nt ? m->nt : 1)); int wn = 0;
    for (int i = 0; i < m->nt; i++) {
        if (drop && drop[i]) continue;
        const double *a = m->v + 3 * m->f[3 * i], *b = m->v + 3 * m->f[3 * i + 1], *c = m->v + 3 * m->f[3 * i + 2];
        double u0 = b[0] - a[0], u1 = b[1] - a[1], u2 = b[2] - a[2], w0 = c[0] - b[0], w1 = c[1] - b[1], w2 = c[2] - b[2];
        double cx = u1 * w2 - u2 * w1, cy = u2 * w0 - u0 * w2, cz = u0 * w1 - u1 * w0;
        double nn = sqrt(cx * cx + cy * cy + cz * cz); float o[12];
        if (nn > 1e-13) { double r = 1.0 / nn; o[0] = (float)(cx * r); o[1] = (float)(cy * r); o[2] = (float)(cz * r); } else o[0] = o[1] = o[2] = 0;
        for (int k = 0; k < 3; k++) { o[3 + k] = (float)a[k]; o[6 + k] = (float)b[k]; o[9 + k] = (float)c[k]; }
        memcpy(buf + 50 * (size_t)wn, o, 48); buf[50 * (size_t)wn + 48] = buf[50 * (size_t)wn + 49] = 0; wn++;
    }
    fwrite(buf, 50, (size_t)wn, f); free(buf); free(drop);
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
/* smooth details: the base's seams closed in memory before the shells are joined to it (run_set.close_seams runs on the
   written STL later; Manifold needs a closed base first). The mesh as stl_write would write it, close_seams, read back,
   corners merged. */
static void seams_now(Mesh *m) {
    size_t n = 84 + 50 * (size_t)m->nt; unsigned char *b = calloc(n, 1); MgBuf o = { 0 }; int rep = 0; uint32_t nt = (uint32_t)m->nt;
    memcpy(b + 80, &nt, 4);
    for (int i = 0; i < m->nt; i++) {
        const double *a = m->v + 3 * m->f[3 * i], *p = m->v + 3 * m->f[3 * i + 1], *c = m->v + 3 * m->f[3 * i + 2];
        double u0 = p[0] - a[0], u1 = p[1] - a[1], u2 = p[2] - a[2], w0 = c[0] - p[0], w1 = c[1] - p[1], w2 = c[2] - p[2];
        double cx = u1 * w2 - u2 * w1, cy = u2 * w0 - u0 * w2, cz = u0 * w1 - u1 * w0, nn = sqrt(cx * cx + cy * cy + cz * cz); float q[12];
        if (nn > 1e-13) { double r = 1.0 / nn; q[0] = (float)(cx * r); q[1] = (float)(cy * r); q[2] = (float)(cz * r); } else q[0] = q[1] = q[2] = 0;
        for (int k = 0; k < 3; k++) { q[3 + k] = (float)a[k]; q[6 + k] = (float)p[k]; q[9 + k] = (float)c[k]; }
        memcpy(b + 84 + 50 * (size_t)i, q, 48);
    }
    if (mg_close_seams(b, n, &o, &rep) == 0 && rep && o.n >= 84) {
        uint32_t k; memcpy(&k, o.b + 80, 4);
        if (84 + 50 * (size_t)k <= o.n) {
            Mesh r; r.nt = (int)k; r.nv = 3 * (int)k; r.v = malloc(sizeof(double) * 9 * (k ? k : 1)); r.f = malloc(sizeof(int) * 3 * (k ? k : 1));
            for (uint32_t i = 0; i < k; i++) { float c[9]; memcpy(c, o.b + 84 + 50 * (size_t)i + 12, 36); for (int j = 0; j < 9; j++) r.v[9 * (size_t)i + j] = c[j]; for (int j = 0; j < 3; j++) r.f[3 * i + j] = 3 * (int)i + j; }
            mesh_merge(&r); free(m->v); free(m->f); *m = r;
        }
    }
    free(b); free(o.b);
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
/* PLAN-next step 1 (BBB_MESH_DIAG=<file>): the odd and pinched edges after a stage; at the end, where each bad edge is */
static void diag_stage(const Mesh *m, const char *base, const char *stage) {
    const char *p = getenv("BBB_MESH_DIAG"); int odd, pin, wt; FILE *f; if (!p || !*p) return;
    edge_counts(m, &odd, &pin, &wt); if (!(f = fopen(p, "a"))) return;
    fprintf(f, "%s stage %-8s odd %d pinched %d watertight %d faces %d" "\n", base, stage, odd, pin, wt, m->nt);
    if ((!strcmp(stage, "final") && (odd || pin)) || (!strcmp(stage, "mesh") && odd)) {          /* 3b: where the mesher left open edges, too */
        Ed *e = mesh_edges(m); size_t ne = 3 * (size_t)m->nt; int k = 0;
        for (size_t i = 0; i < ne && k < 40;) {
            size_t j = i; while (j < ne && e[j].key == e[i].key) j++;
            if (j - i != 2) { int64_t lo = e[i].key / m->nv, hi = e[i].key % m->nv; const double *a = m->v + 3 * lo, *b = m->v + 3 * hi;
                fprintf(f, "%s   bad edge in %d faces: (%.5f %.5f %.4f) - (%.5f %.5f %.4f)" "\n", base, (int)(j - i), a[0], a[1], a[2], b[0], b[1], b[2]); k++; }
            i = j;
        }
        free(e);
    }
    fclose(f);
}
/* Trimesh.split(only_watertight=False): faces joined across edges shared by exactly two faces; comp[f] = part number,
   parts numbered by their first face. Returns the number of parts; vol[k] = each part's volume (signed, mm^3) */
static int mesh_split(const Mesh *m, int *comp, double **vol) {
    int *p = malloc(sizeof(int) * (m->nt ? m->nt : 1)); for (int i = 0; i < m->nt; i++) p[i] = i;
    Ed *e = mesh_edges(m); size_t ne = 3 * (size_t)m->nt;
    for (size_t i = 0; i < ne;) { size_t j = i; while (j < ne && e[j].key == e[i].key) j++; if (j - i == 2) { int a = sl_find(p, e[i].face), b = sl_find(p, e[i + 1].face); if (a != b) p[a < b ? b : a] = a < b ? a : b; } i = j; }
    free(e);
    int nc = 0; int *lab = malloc(sizeof(int) * (m->nt ? m->nt : 1)); for (int i = 0; i < m->nt; i++) lab[i] = -1;
    for (int i = 0; i < m->nt; i++) { int r = sl_find(p, i); if (lab[r] < 0) lab[r] = nc++; comp[i] = lab[r]; }
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
static int magnet_centres(double dia, double *xy) { return mag_centres(dia, xy); }          /* magnets.h: the one copy */
static int oval_magnet_centres(double A, double *xy) { return mag_oval_centres(A, xy); }
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
static const double KNOWN_SIZES[17] = { 20, 25, 28, 28.5, 30, 32, 40, 50, 60, 65, 70, 80, 90, 100, 120, 130, 160 };
static const double OVAL_SIZES[7][2] = { { 60, 35.5 }, { 75, 42 }, { 90, 52 }, { 105, 70 }, { 120, 92 }, { 150, 95 }, { 170, 109 } };
typedef struct {
    char variant[16], *errors, *reason, secs[32], slowest[64], steps[16], theme[256], *plan_line, *plan, *dropped, restarts[16], note[128], diff[16], *made;
} Try;
typedef struct { char base[160]; int n, ok, ntries, detail; Try tries[9]; } BaseRes;          /* 9: up to 8 attempts and the mend */
typedef struct RunSet RunSet;
struct RunSet {
    const char *style, *salt; double SIZE, OVAL[2]; int oval; double SCALE, LAYOUT; char out[MAX_PATH * 3], size_tag[64], ovtag[64];          /* oval: 1 an oval, 2 a square or rectangle; ovtag: "AxB" / "AxBr" for KIT_OVAL */
    int workers, nnums, *nums; volatile LONG next, flagged, stop; ULONGLONG t0;
    CRITICAL_SECTION lock; Recent *recent; int nrecent; Bridge *free_br; int nfree, capfree;
    WCHAR *envw; const EnvSet *env; int nenv; BaseRes *res; int nres;
    void (*done)(RunSet *rs, BaseRes *r, void *ctx); void *done_ctx;          /* gui_job: one base finished (its OK / FLAG line) */
};
static void rs_print(const char *fmt, ...) {          /* a line run_set prints: gui_job passes each on as a "log" event */
    char t[4096]; va_list ap; va_start(ap, fmt); vsnprintf(t, sizeof t, fmt, ap); va_end(ap);
    for (char *l = t, *e; *l; l = e) { e = strchr(l, '\n'); char c = 0; if (e) { c = *e; *e = 0; } if (*l) ev("log", "s", "text", l); if (!e) break; *e = c; e++; }
}
/* settings "workers": "auto" (or 0 / "") = half the CPU cores, at most 4 (step 5 of the speed plan, 2026-11-22), else
   the number given; at least 1. Bases built at once, and the choice cards drawn at once. */
static int workers_auto(void) {
    runner_cfg_init(); JV *w = cfg_get("workers"); int n;
    if (!w || (w->t == 4 && (!strcmp(w->s, "auto") || !w->s[0] || !strcmp(w->s, "0"))) || (w->t == 3 && w->d == 0)) { int c = (int)GetActiveProcessorCount(ALL_PROCESSOR_GROUPS) / 2; n = c > 4 ? 4 : c; }
    else n = w->t == 3 ? (int)w->d : atoi(w->s);
    return n < 1 ? 1 : n;
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
typedef struct { char *error; double secs; JV head; char path[MAX_PATH * 3], mesher[128], pictures[200], note[200]; int cells, tris, watertight, pinched; double top_mm; Sb skipped; double t_lay, t_mesh, t_fin, t_pic, t_grain, t_join, t_simp, t_clean, t_stl; } Built;          /* t_*: seconds per stage (speed plan step 1) */
/* review P2-1 + N2 (2026-11-22): where grain goes and which texture. The base's open ground gets its style's texture;
   ground-like patches (mud, sand, grit ...) their own; planks a wood grain along them; stone slabs and headstones small
   pits; water and the like stay smooth (gloss, resin); every other object keeps its own top. A grid of 0.1 mm cells
   over the base (scene -> mm: x * sc + tx). */
static int grain_op_tex(const char *k) {
    static const struct { const char *w; int t; } M[] = {
        { "mud", GT_MUD }, { "sand", GT_SAND }, { "grit", GT_GRAVEL }, { "gravel", GT_GRAVEL }, { "rubble", GT_GRAVEL }, { "scree", GT_GRAVEL },
        { "ash", GT_ASH }, { "cinder", GT_ASH }, { "earth", GT_SOIL }, { "soil", GT_SOIL }, { "dirt", GT_SOIL }, { "loam", GT_SOIL }, { "clay", GT_SOIL },
        { "peat", GT_SOIL }, { "silt", GT_SOIL }, { "dust", GT_SOIL }, { "mound", GT_SOIL }, { "heap", GT_SOIL }, { "bank", GT_SOIL }, { "snow", GT_MUD },
        { "plank", GT_PLANK }, { "board", GT_PLANK }, { "deck", GT_PLANK }, { "wood", GT_PLANK }, { "crate", GT_PLANK },
        { "slab", GT_STONE }, { "flag", GT_STONE }, { "paving", GT_STONE }, { "tomb", GT_STONE }, { "headstone", GT_STONE }, { "grave", GT_STONE },
        { "step", GT_STONE }, { "altar", GT_STONE }, { "plinth", GT_STONE }, { "tile", GT_STONE }, { NULL, 0 } };
    for (int i = 0; M[i].w; i++) if (strstr(k, M[i].w)) return M[i].t;
    return GT_SMOOTH;
}
static int grain_smooth_kind(const char *k) {          /* ground cuts that stay smooth */
    static const char *W[] = { "water", "ice", "pool", "pond", "puddle", "lava", "acid", "oil", "tar", "slime", "blood", "metal", "plate", "grate", NULL };
    for (int i = 0; W[i]; i++) if (strstr(k, W[i])) return 1;
    return 0;
}
static int grain_style_tex(const char *st) {          /* each style's own ground (settings "ground_texture" wins: soil mud sand gravel ash) */
    static const struct { const char *s; int t; } M[] = {
        { "mine", GT_GRAVEL }, { "corridor", GT_GRAVEL }, { "urban", GT_GRAVEL }, { "fortress", GT_GRAVEL }, { "ferrum", GT_GRAVEL }, { "trederra", GT_GRAVEL },
        { "sundered", GT_GRAVEL }, { "dungeon", GT_GRAVEL }, { "temple", GT_GRAVEL },
        { "badlands", GT_SAND }, { "frontier", GT_SAND }, { "coast", GT_SAND }, { "maahl", GT_SAND }, { "obelis", GT_SAND }, { "targa", GT_SAND },
        { "swamp", GT_MUD }, { "infestation", GT_MUD }, { "cynder", GT_ASH }, { "ashveil", GT_ASH }, { NULL, 0 } };
    static const char *NM[] = { "", "soil", "mud", "sand", "gravel", "ash" };
    const char *o = cfg_str("ground_texture", "");
    for (int i = 1; i <= 5; i++) if (!strcmp(o, NM[i])) return i;
    for (int i = 0; M[i].s; i++) if (!strcmp(st, M[i].s)) return M[i].t;
    return GT_SOIL;
}
static void grain_fill(unsigned char *g, unsigned char *ga, int w, int h, double x0, double y0, double st, const D2Poly *p, double sc, double tx, double ty, int val) {
    if (p->type != 1 || p->nr < 1) return;
    double bx0 = 1e30, by0 = 1e30, bx1 = -1e30, by1 = -1e30, bestl = -1, deg = 0;
    for (int i = 0; i < p->len[0]; i++) {
        double x = p->xy[2 * i] * sc + tx, y = p->xy[2 * i + 1] * sc + ty, xn = p->xy[2 * ((i + 1) % p->len[0])] * sc + tx, yn = p->xy[2 * ((i + 1) % p->len[0]) + 1] * sc + ty, l = hypot(xn - x, yn - y);
        if (x < bx0) bx0 = x; if (x > bx1) bx1 = x; if (y < by0) by0 = y; if (y > by1) by1 = y;
        if (l > bestl) { bestl = l; deg = atan2(yn - y, xn - x) * 180.0 / 3.14159265358979; }          /* a plank runs along its longest side */
    }
    while (deg < 0) deg += 180; while (deg >= 180) deg -= 180;
    int i0 = (int)floor((bx0 - x0) / st), i1 = (int)ceil((bx1 - x0) / st), j0 = (int)floor((by0 - y0) / st), j1 = (int)ceil((by1 - y0) / st);
    if (i0 < 0) i0 = 0; if (j0 < 0) j0 = 0; if (i1 > w - 1) i1 = w - 1; if (j1 > h - 1) j1 = h - 1;
    for (int j = j0; j <= j1; j++) for (int i = i0; i <= i1; i++) {
        double X = x0 + (i + 0.5) * st, Y = y0 + (j + 0.5) * st; int in = 0, at = 0;
        for (int r = 0; r < p->nr; r++) {          /* even-odd over every ring: holes come out */
            int n = p->len[r]; const double *xy = p->xy + 2 * at;
            for (int a = 0, b = n - 1; a < n; b = a++) {
                double ax = xy[2 * a] * sc + tx, ay = xy[2 * a + 1] * sc + ty, bx = xy[2 * b] * sc + tx, by = xy[2 * b + 1] * sc + ty;
                if ((ay > Y) != (by > Y) && X < (bx - ax) * (Y - ay) / (by - ay) + ax) in = !in;
            }
            at += n;
        }
        if (in) { g[(size_t)j * w + i] = (unsigned char)val; if (ga) ga[(size_t)j * w + i] = (unsigned char)deg; }
    }
}
static unsigned char *grain_mask(const D2Scene *s, const Mesh *m, const char *style, GrainOpt *o, unsigned char **angp) {
    double mn[3], mx[3], sc = s->has_sp ? s->sc : 1.0, tx = s->has_sp ? s->tx : 0, ty = s->has_sp ? s->ty : 0, st = 0.1; int w, h; unsigned char *g, *ga;
    o->deftex = grain_style_tex(style); *angp = NULL;
    mesh_bounds(m, mn, mx); w = (int)ceil((mx[0] - mn[0]) / st) + 2; h = (int)ceil((mx[1] - mn[1]) / st) + 2;
    if (w <= 0 || h <= 0 || (size_t)w * h > 40000000u) return NULL;
    g = calloc((size_t)w * h, 1); ga = calloc((size_t)w * h, 1); if (!g || !ga) { free(g); free(ga); return NULL; }
    for (int k = 0; k < s->ngc; k++) if (s->gc[k].kind && grain_smooth_kind(s->gc[k].kind)) for (int q = 0; q < s->gc[k].np; q++) grain_fill(g, NULL, w, h, mn[0] - st, mn[1] - st, st, &s->gc[k].p[q], sc, tx, ty, GT_SMOOTH);
    for (int k = 0; k < s->ntc; k++) if (s->tc[k].kind && grain_smooth_kind(s->tc[k].kind)) for (int q = 0; q < s->tc[k].np; q++) grain_fill(g, NULL, w, h, mn[0] - st, mn[1] - st, st, &s->tc[k].p[q], sc, tx, ty, GT_SMOOTH);
    for (int k = 0; k < s->nop; k++) if (s->op[k].kind) { int t = grain_op_tex(s->op[k].kind); for (int q = 0; q < s->op[k].np; q++) grain_fill(g, t == GT_PLANK ? ga : NULL, w, h, mn[0] - st, mn[1] - st, st, &s->op[k].p[q], sc, tx, ty, t); }
    o->mask = g; o->mw = w; o->mh = h; o->mx0 = mn[0] - st; o->my0 = mn[1] - st; o->mstep = st; o->ang = ga; *angp = ga;
    return g;
}
static int printer_resin(void) {          /* review N1: "printer": "fdm" (standard, the user's) or "resin"; --printer wins */
    const char *e = getenv("KIT_PRINTER"); const char *p = e && *e ? e : cfg_str("printer", "fdm");
    return !_stricmp(p, "resin");
}
static int MEND_CLI = -1;          /* the sliver mend: --mend on|off; else KIT_MEND; else settings mend_thin_faces (OFF) */
static int mend_on(void) { const char *e = getenv("KIT_MEND"); if (MEND_CLI >= 0) return MEND_CLI; if (e && (e[0] == '0' || e[0] == '1') && !e[1]) return e[0] == '1'; return cfg_bool("mend_thin_faces", 0); }
static int mesh_fault(const char *r) { return r && (strstr(r, "not watertight") || strstr(r, "not a valid solid") || strstr(r, "loose piece")); }          /* a mend is only for these */
static void build_once(RunSet *rs, Bridge *br, int n, const char *variant, Built *B, int mend) {
    memset(B, 0, sizeof *B); ULONGLONG t = GetTickCount64(); char name[160]; nm(rs, n, name, sizeof name);
    char png2d[MAX_PATH * 3]; snprintf(B->path, sizeof B->path, "%s/%s/%s.stl", rs->out, rs->style, name); snprintf(png2d, sizeof png2d, "%.*s_2d.png", (int)strlen(B->path) - 4, B->path);
    char dir[MAX_PATH * 3]; snprintf(dir, sizeof dir, "%s\\%s", rs->out, rs->style); make_dirs(dir);
    Sb rq = { 0 }; sb_printf(&rq, "{\"n\": %d, \"variant\": ", n); json_str(&rq, variant); sb_puts(&rq, ", \"name\": "); json_str(&rq, name); sb_puts(&rq, ", \"png2d\": "); json_str(&rq, png2d); sb_puts(&rq, "}");
    unsigned char **bl; size_t *ln; int nb;
    int r = layout_python_build(br, rq.s, base_limit(rs), &B->head, &bl, &ln, &nb); free(rq.s);
    ULONGLONG tq = GetTickCount64(); B->t_lay = (tq - t) / 1000.0;
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
    EnvSet es[4] = { { "KIT_MESH_THREADS", thr }, { "KIT_LIVE_FILE", lp[0] ? lp : NULL }, { "KIT_DIAG_NAME", name }, { "KIT_MEND_THIN", mend ? "1" : NULL } };          /* KIT_DIAG_NAME: step 1's log names the base; KIT_MEND_THIN: the mend */
    Sb out = { 0 }, err = { 0 }; int rc = run_core("--core-build", bl[0], ln[0], es, 4, &out, &err);
    if (rc != 0 || out.n < 12 || memcmp(out.s, "BBR1", 4)) {          /* cmesh.build_c gave None: Python would build this one in Python (not in C) */
        snprintf(e, sizeof e, "crash: the C field builder failed (%.200s)", err.s ? err.s : ""); B->error = xstrdup(e);
        free(out.s); free(err.s); free_blobs(bl, ln, nb); if (have_sc) d2_scene_free(&sc); B->secs = py_rint((GetTickCount64() - t) / 1000.0); return;
    }
    { ULONGLONG t2 = GetTickCount64(); B->t_mesh = (t2 - tq) / 1000.0; tq = t2; }
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
    if (have_sc && getenv("BBB_GRAIN_KINDS")) { FILE *kf = fopen(getenv("BBB_GRAIN_KINDS"), "a"); if (kf) { fprintf(kf, "%s ground=%s sp=%d sc=%g tx=%g ty=%g\n", name, sc.ground ? sc.ground : "-", sc.has_sp, sc.sc, sc.tx, sc.ty);
        for (int g = 0; g < sc.ngc; g++) fprintf(kf, " gc %s np=%d\n", sc.gc[g].kind ? sc.gc[g].kind : "?", sc.gc[g].np);
        for (int g = 0; g < sc.nop; g++) fprintf(kf, " op %s type=%d z0=%g b=%g np=%d\n", sc.op[g].kind ? sc.op[g].kind : "?", sc.op[g].optype, sc.op[g].z0, sc.op[g].b, sc.op[g].np);
        for (int g = 0; g < sc.ntc; g++) fprintf(kf, " tc %s np=%d\n", sc.tc[g].kind ? sc.tc[g].kind : "?", sc.tc[g].np); fclose(kf); } }
    diag_stage(&m, name, "mesh");
    mesh_merge(&m); diag_stage(&m, name, "merge");
    ULONGLONG tg0 = GetTickCount64();
    if (cfg_bool("ground_grain", 1)) {          /* plan B step 2: fine grain on every flat top (grain.c), so no open ground is smooth */
        uint32_t sd = 2166136261u; for (const char *c = name; *c; c++) sd = (sd ^ (unsigned char)*c) * 16777619u;
        GrainOpt go = { 0 }; int resin = printer_resin(); unsigned char *gm;
        go.amp = (resin ? cfg_num("ground_grain_mm", 0.12) : cfg_num("ground_grain_fdm_mm", 0.25)) * strength(); go.coarse = !resin;          /* review P2-2: FDM grain big enough to print */
        unsigned char *ga = NULL; go.deftex = grain_style_tex(rs->style);
        gm = have_sc && cfg_bool("grain_by_ground", 1) ? grain_mask(&sc, &m, rs->style, &go, &ga) : NULL;
        int t0 = m.nt, mv = grain_apply(&m.v, &m.nv, &m.f, &m.nt, &go, sd); free(gm); free(ga);
        const char *gl = getenv("BBB_GRAIN_LOG");
        if (gl && *gl) { FILE *lf = fopen(gl, "a"); if (lf) { fprintf(lf, "%s: faces %d -> %d, points moved %d\n", name, t0, m.nt, mv); fclose(lf); } }
    }
    B->t_grain = (GetTickCount64() - tg0) / 1000.0; diag_stage(&m, name, "grain");
    int tris_field = m.nt;          /* len(f): the surface's faces, before the solids */
    {   /* --supports on: m, extra = add_solids(...) (the shells were built by the layout helper: solids3d.c, the "BBS1" blob); trimesh.util.concatenate([m] + extra) */
        JV *sk3 = jget(&B->head, "skipped3d");
        for (int i = 0; sk3 && i < sk3->n; i++) { if (B->skipped.n) sb_add(&B->skipped, "\n", 1); sb_puts(&B->skipped, sk3->a[i].s); }
        if (nb > 2 && ln[2] >= 8 && !memcmp(bl[2], "BBS1", 4)) {
            const unsigned char *p = bl[2] + 8, *end = bl[2] + ln[2]; uint32_t ns; memcpy(&ns, bl[2] + 4, 4);
            if (ns && !allow_supports() && join_shells()) seams_now(&m);          /* smooth details: a closed base to join to */
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
    diag_stage(&m, name, "shells");
    ULONGLONG tj0 = GetTickCount64();
    if (join_shells()) {          /* supports join / smooth details: the base and the shells one solid (Manifold); pieces left in the air dropped */
        MfJoin jr; char je[200];
        if (mf_join(&m.v, &m.nv, &m.f, &m.nt, &jr, je, sizeof je) == 0) {
            char t2[120]; t2[0] = 0;
            if (jr.floating) snprintf(t2, sizeof t2, "%d piece%s in the air", jr.floating, jr.floating > 1 ? "s" : "");
            if (jr.not_closed) snprintf(t2 + strlen(t2), sizeof t2 - strlen(t2), "%s%d piece%s not closed", t2[0] ? ", " : "", jr.not_closed, jr.not_closed > 1 ? "s" : "");
            if (t2[0]) { if (B->skipped.n) sb_add(&B->skipped, "\n", 1); sb_puts(&B->skipped, t2); }
        } else { if (B->skipped.n) sb_add(&B->skipped, "\n", 1); sb_puts(&B->skipped, je); mg_log_caught("join", "RuntimeError", je, "cport/mfjoin.c"); }
    }
    ULONGLONG ts0 = GetTickCount64(); B->t_join = (ts0 - tj0) / 1000.0; diag_stage(&m, name, "join");
    if (smaller_files() && (!allow_supports() || supports_join())) {          /* smaller files: flat areas in fewer triangles (Manifold), the shape moves no more than 0.005 mm */
        char se[200]; if (mf_simplify(&m.v, &m.nv, &m.f, &m.nt, cfg_num("smaller_files_mm", 0.005), se, sizeof se) != 0) mg_log_caught("smaller files", "RuntimeError", se, "cport/mfjoin.c");
    }
    ULONGLONG tc0 = GetTickCount64(); B->t_simp = (tc0 - ts0) / 1000.0; diag_stage(&m, name, "smaller");
    { int dr = mesh_f32_clean(&m); if (dr && getenv("BBB_F32_LOG")) { FILE *lf = fopen(getenv("BBB_F32_LOG"), "a"); if (lf) { fprintf(lf, "%s: %d faces dropped at float32\n", B->path, dr); fclose(lf); } } }          /* the mesh as the STL will hold it */
    diag_stage(&m, name, "final");
    int odd, pin; edge_counts(&m, &odd, &pin, &B->watertight); B->tris = tris_field;
    B->pinched = pin;          /* review P2-4: edges in more than two faces are said in the note, not hidden */
    double mn[3], mx[3]; mesh_bounds(&m, mn, mx); B->top_mm = py_round(mx[2] - jget(&B->head, "H")->d, 1);
    ULONGLONG tw0 = GetTickCount64(); B->t_clean = (tw0 - tc0) / 1000.0;
    stl_write(B->path, &m); mesh_free(&m); B->t_stl = (GetTickCount64() - tw0) / 1000.0;
    if (cfg_bool("drawing_2d", 1) && !drawn && !py_drawn && have_sc) { double win[4]; char er[200]; if (d2_draw(&sc, png2d, 900, 0, win, er, sizeof er)) save_win(png2d, win); }
    if (have_sc) d2_scene_free(&sc);
    free_blobs(bl, ln, nb);
    B->secs = py_round((GetTickCount64() - t) / 1000.0, 1);
    { ULONGLONG t2 = GetTickCount64(); B->t_fin = (t2 - tq) / 1000.0; tq = t2; }
    { const char *kv[] = { "stage", "picture" }; live_note(rs->salt, n, 1, kv); }          /* mesher serve: the 3D picture */
    Mesh soup; char iso[MAX_PATH * 3], by[200]; snprintf(iso, sizeof iso, "%.*s_iso.png", (int)strlen(B->path) - 4, B->path);
    if (stl_read(B->path, &soup) && shot_png(&soup, iso, 900, 560, 30, -60, by, sizeof by)) snprintf(B->pictures, sizeof B->pictures, "%s", (by[0] && strncmp(by, "failed", 6)) ? by : "Python");
    else snprintf(B->note, sizeof B->note, "no picture: %s", by);
    mesh_free(&soup);
    B->t_pic = (GetTickCount64() - tq) / 1000.0;
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
    if (strict_check() && (!allow_supports() || supports_join())) {          /* the strict check: Manifold's own test of a valid solid */
        int st = mf_check(m.v, m.nv, m.f, m.nt); if (st) { snprintf(t, sizeof t, "not a valid solid (%s)", mf_status_text(st)); if (probs->n) sb_puts(probs, ", "); sb_puts(probs, t); }
    }
    if (parts != 1 && (!allow_supports() || supports_join())) {          /* joined: one solid again */ snprintf(t, sizeof t, "%d loose pieces", parts); if (probs->n) sb_puts(probs, ", "); sb_puts(probs, t); }
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
    int vi = 0, mtried = 0;          /* vi: the next variant; the mend (3b) is not one: the same design again, once, before any variant */
    for (int k = 0; k < attempts + 1; k++) {
        if (rs->stop || runner_stop) break;
        int mp = mend_on() && !mtried && R->ntries == 1 && mesh_fault(R->tries[0].reason);
        if (!mp && vi >= attempts) break;
        if (mp) mtried = 1;
        const char *v = mp ? "" : VARIANTS[vi];
        if (!mp) vi++;
        EnterCriticalSection(&rs->lock);
        if (mp) rs_print("  >> %s  mending the same design (faces thinner than 0.02 mm dissolved)", R->base);
        else if (k == 0) rs_print("  >> %s  building", R->base); else rs_print("  >> %s  retry with fresh variant %s", R->base, v);
        if (mp) ev("mend", "si", "seed", rs->salt, "n", n);          /* the mend: no retry event, the card is not a variant */
        else if (k == 0) ev("start", "si", "seed", rs->salt, "n", n);          /* gui_job: the ">>" line */
        else {          /* PLAN-next step 2: why the picked design was not kept, in a few words, for its card */
            const char *r = R->ntries ? R->tries[R->ntries - 1].reason : NULL, *w;
            if (!r) w = "failed";
            else if (strstr(r, "too similar")) w = "was too like a design built just before";
            else if (strstr(r, "came out flat")) w = "came out with a flat top";
            else if (strstr(r, "pieces dropped")) w = "lost too many of its pieces";
            else if (strstr(r, "loose piece") || strstr(r, "watertight") || strstr(r, "solid")) w = "was not one closed solid";
            else if (strstr(r, "over ") && strstr(r, "limit")) w = "took too long";
            else w = "failed the print check";
            ev("retry", "sis", "seed", rs->salt, "n", n, "why", w);
        }
        LeaveCriticalSection(&rs->lock);
        Bridge *br = rs_bridge(rs); Built B; build_once(rs, br, n, v, &B, mp); rs_give_back(rs, br);
        char stl[MAX_PATH * 3]; snprintf(stl, sizeof stl, "%s\\%s\\%s.stl", rs->out, rs->style, R->base);
        Sb reason = { 0 }; if (B.error) sb_puts(&reason, B.error);
        int seam = 0; char diff[16] = ""; ULONGLONG tck = GetTickCount64();
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
        if (!B.error) { double tc = (GetTickCount64() - tck) / 1000.0; EnterCriticalSection(&rs->lock);          /* speed plan step 1: where the time went */
            JV *a1 = jget(&B.head, "t_scene"), *a2 = jget(&B.head, "t_input"), *a3 = jget(&B.head, "t_shells");
            rs_print("  time %s: layout %.1f s (scene %.1f, command groups %.1f, shells %.1f), height field + mesh %.1f s, grain + join + STL %.1f s (grain %.1f, join %.1f, smaller %.1f, clean + check %.1f, write %.1f), picture %.1f s, checks %.1f s", R->base, B.t_lay,
                     a1 ? a1->d : -1.0, a2 ? a2->d : -1.0, a3 ? a3->d : -1.0, B.t_mesh, B.t_fin, B.t_grain, B.t_join, B.t_simp, B.t_clean, B.t_stl, B.t_pic, tc);
            LeaveCriticalSection(&rs->lock); }
        WCHAR ws[MAX_PATH * 3]; wpath(stl, ws, MAX_PATH * 3);
        if (reason.n && GetFileAttributesW(ws) != INVALID_FILE_ATTRIBUTES) {          /* the failed attempt is kept for a look */
            char fd[MAX_PATH * 3], dst[MAX_PATH * 3], s2[MAX_PATH * 3]; snprintf(fd, sizeof fd, "%s\\%s\\failed", rs->out, rs->style); make_dirs(fd);
            WCHAR a[MAX_PATH * 3], b[MAX_PATH * 3];
            snprintf(dst, sizeof dst, "%s\\%s_%s.stl", fd, R->base, mp ? "mended" : v[0] ? v : "original"); wpath(dst, b, MAX_PATH * 3); MoveFileExW(ws, b, MOVEFILE_REPLACE_EXISTING);
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
        snprintf(T->variant, sizeof T->variant, "%s", mp ? "mended" : v[0] ? v : "original"); T->reason = reason.n ? reason.s : NULL;
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
        snprintf(T->restarts, sizeof T->restarts, "0"); { char pn[90] = ""; if (B.pinched) snprintf(pn, sizeof pn, "%d edge%s in more than two faces (a slicer mends this)", B.pinched, B.pinched == 1 ? "" : "s");
            snprintf(T->note, sizeof T->note, "%s%s%s", seam ? "rim seam closed" : "", seam && pn[0] ? "; " : "", pn); } snprintf(T->diff, sizeof T->diff, "%s", diff);
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
            if (R->ntries > 1 && !strcmp(last->variant, "mended")) sb_printf(&l, "  (mended, the same design, after: %s)", R->tries[0].reason);
            else if (R->ntries > 1) sb_printf(&l, "  (fresh variant after: %s%s%s)", R->tries[0].reason, R->ntries > 2 && !strcmp(R->tries[1].variant, "mended") ? "; the mend: " : "", R->ntries > 2 && !strcmp(R->tries[1].variant, "mended") && R->tries[1].reason ? R->tries[1].reason : "");
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
typedef struct { const char *style, *size, *seed, *dest; double scale; int *seen, okc, bad; double obj, mm, fig; const char *mini; } PickCtx;          /* obj: --obj (0 = classic), fig: the figure post's height (mm) */
static void size_tag_gui(const char *txt, char *out, size_t cap) {          /* gui_job.size_tag */
    double a, b; int ov; read_size(txt, &a, &b, &ov); char x[32], y[32]; fmt_g(x, sizeof x, a); fmt_g(y, sizeof y, b);
    if (ov == 2) { if (a == b) snprintf(out, cap, "%ssq", x); else snprintf(out, cap, "%sx%sr", x, y); }          /* a square or rectangle */
    else if (a == b) snprintf(out, cap, "%s", x); else snprintf(out, cap, "%sx%s", x, y);
}
static void copy_file(const char *a, const char *b) { WCHAR x[MAX_PATH * 3], y[MAX_PATH * 3]; wpath(a, x, MAX_PATH * 3); wpath(b, y, MAX_PATH * 3); CopyFileW(x, y, FALSE); }
/* the picked file's name (plan B4, 2026-11-22): settings "file_name" or --name, with {style} {size} {theme} {seed} {n} {date};
   the standard one makes the same names as before. Only letters, digits, - and _ are kept; doubled or end _ dropped. */
static const char *NAME_CLI = NULL;
static void pick_name(const char *style, const char *size, const char *theme, const char *seed, int n, char *out, size_t cap) {
    const char *t = NAME_CLI && NAME_CLI[0] ? NAME_CLI : cfg_str("file_name", "{style}_{size}mm_{theme}_{seed}_{n}"); char raw[600]; size_t j = 0, k = 0; int us = 1;
    for (const char *c = t; *c && j < sizeof raw - 80; c++) {
        char v[64]; const char *put = NULL; const char *e = *c == '{' ? strchr(c, '}') : NULL;
        if (e) { size_t L = (size_t)(e - c - 1);
            if (L == 5 && !strncmp(c + 1, "style", 5)) put = style; else if (L == 4 && !strncmp(c + 1, "size", 4)) put = size;
            else if (L == 5 && !strncmp(c + 1, "theme", 5)) put = theme; else if (L == 4 && !strncmp(c + 1, "seed", 4)) put = seed;
            else if (L == 1 && c[1] == 'n') { snprintf(v, sizeof v, "%d", n); put = v; }
            else if (L == 4 && !strncmp(c + 1, "date", 4)) { SYSTEMTIME s; GetLocalTime(&s); snprintf(v, sizeof v, "%04d%02d%02d", s.wYear, s.wMonth, s.wDay); put = v; } }
        if (put) { for (; *put && j < sizeof raw - 80; put++) raw[j++] = *put; c = e; } else raw[j++] = *c;
    }
    raw[j] = 0;
    for (j = 0; raw[j] && k + 1 < cap; j++) { char ch = raw[j]; int ok = (ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') || (ch >= '0' && ch <= '9') || ch == '-' || ch == '.';
        if (ok) { out[k++] = ch; us = 0; } else if (!us) { out[k++] = '_'; us = 1; } }
    while (k && out[k - 1] == '_') k--; out[k] = 0;
    if (!k) snprintf(out, cap, "%s_%smm_%s_%d", style, size, seed, n);
}
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
        pick_name(P->style, st, tg, P->seed, n, nw, sizeof nw); (void)tl;
        char dstl[MAX_PATH * 3], img[MAX_PATH * 3] = "", img2[MAX_PATH * 3] = "", img3c[MAX_PATH * 3] = "", t[MAX_PATH * 3], er[300];
        snprintf(dstl, sizeof dstl, "%s\\%s.stl", P->dest, nw); copy_file(stl, dstl);
        snprintf(t, sizeof t, "%.*s_plan.json", (int)strlen(stl) - 4, stl); if (file_exists(t)) { char d[MAX_PATH * 3]; snprintf(d, sizeof d, "%s\\%s_plan.json", P->dest, nw); copy_file(t, d); }
        snprintf(img, sizeof img, "%s\\%s_3d.png", P->dest, nw);
        {   /* with_figure: the classic look is the base alone; a figure scale adds a plain figure beside it (meshglue.h mg_figure) */
            int okp = 0; Mesh soup; char by[200];
            if (stl_read(stl, &soup)) {
                char tmp[MAX_PATH * 3]; snprintf(tmp, sizeof tmp, "%.*s_raw.png", (int)strlen(img) - 4, img);
                int W = strchr(P->size, 'x') ? 1100 : 900, Hh = strchr(P->size, 'x') ? 620 : 700;
                if (P->scale > 1.01 || P->obj > 0) {          /* concatenate([m, fig]) */
                    double r = -INFINITY; MgMesh fig = { 0 }, all;
                    for (int i = 0; i < soup.nv; i++) if (soup.v[3 * i] > r) r = soup.v[3 * i];
                    if (P->obj > 0 && P->mm >= 39.9) {          /* the model's space: a low wall round it, so a big base shows where the model goes */
                        double rad, hx, hy, xy[2 * 160], zt = 4.0; int box = ft_model_space(P->mm, r, P->mini, cfg_num("model_room", 1.0), &rad, &hx, &hy), n = 0; MgMesh ol = { 0 };
                        if (box) { double per = 4 * (hx + hy); for (int k = 0; k < 160; k++) { double t = per * k / 160.0, x, y;
                                if (t < 2 * hx) { x = -hx + t; y = -hy; } else if (t < 2 * hx + 2 * hy) { x = hx; y = -hy + (t - 2 * hx); } else if (t < 4 * hx + 2 * hy) { x = hx - (t - 2 * hx - 2 * hy); y = hy; } else { x = -hx; y = hy - (t - 4 * hx - 2 * hy); }
                                xy[2 * n] = x; xy[2 * n + 1] = y; n++; } }
                        else for (int k = 0; k < 64; k++) { xy[2 * n] = rad * cos(6.2831853 * k / 64); xy[2 * n + 1] = rad * sin(6.2831853 * k / 64); n++; }
                        {   /* inside the base: an oval's short sides too */
                            double ry = -INFINITY; for (int i = 0; i < soup.nv; i++) if (soup.v[3 * i + 1] > ry) ry = soup.v[3 * i + 1];
                            for (int k = 0; k < n; k++) { double x = xy[2 * k], y = xy[2 * k + 1], e = sqrt((x / r) * (x / r) + (y / ry) * (y / ry)); if (e > 0.95) { xy[2 * k] = x * 0.95 / e; xy[2 * k + 1] = y * 0.95 / e; } }
                        }
                        {   /* on the ground the model stands on (cleared and level): the commonest surface height in the space, in 0.1 mm steps */
                            static int hist[400]; int best = -1; memset(hist, 0, sizeof hist);
                            for (int i = 0; i < soup.nv; i++) { double x = soup.v[3 * i], y = soup.v[3 * i + 1], z = soup.v[3 * i + 2];
                                if ((box ? fabs(x) <= hx && fabs(y) <= hy : x * x + y * y <= rad * rad) && x * x + y * y < 0.9 * r * r && z > 1.0 && z < 40.0) hist[(int)(z * 10)]++; }
                            for (int q = 0; q < 400; q++) if (hist[q] && (best < 0 || hist[q] > hist[best])) best = q;
                            if (best >= 0) zt = best / 10.0;
                        }
                        if (mg_outline(xy, n, 0.9, zt + 0.2, zt + 1.0, 2, &ol) == 0) { all.v = soup.v; all.nv = soup.nv; all.f = soup.f; all.nt = soup.nt; mg_concat(&all, &ol, 1); soup.v = all.v; soup.nv = all.nv; soup.f = all.f; soup.nt = all.nt; }
                        mg_mesh_free(&ol);
                    }
                    if (mg_figure(P->obj > 0 ? P->fig * 3.0 / 32.0 : P->scale, r, &fig) == 0) {
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
                if (okp && cfg_bool("side_picture", 1)) {          /* plan B4 (2026-11-22): the side view, low and level, shows the real edge and how tall things are */
                    char sp[MAX_PATH * 3], by3[200] = ""; Mesh bare; int sok = 0; snprintf(sp, sizeof sp, "%s\\%s_side.png", P->dest, nw);
                    if (stl_read(stl, &bare)) {          /* the base alone (no figure), from level */
                        sok = shot_png(&bare, tmp, 1200, 300, 0, -90, by3, sizeof by3) && d2_see_through(tmp, sp, 600, er, sizeof er); mesh_free(&bare); }
                    if (!sok) { char msg[400]; snprintf(msg, sizeof msg, "no side picture for %s: %s", R->base, by3[0] ? by3 : er); ev("log", "s", "text", msg); }
                    DeleteFileW(wt);
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
        {   int mended = !strcmp(last->variant, "mended"), isvar = strcmp(last->variant, "original") && !mended;          /* a mended base is the picked design: not a variant */
            ev("built", "sibbbssssfs", "seed", P->seed, "n", n, "ok", 1, "variant", isvar, "mended", mended, "stl", dstl, "img", img, "img2", img2, "img3c", img3c, "secs", secs, "theme", theme); }
    } else {
        P->bad++;
        if (!R->ok) {
            Sb why = { 0 }; for (int t = 0; t < R->ntries; t++) { if (t) sb_puts(&why, " / "); sb_puts(&why, R->tries[t].reason ? R->tries[t].reason : ""); }
            char w[201]; snprintf(w, sizeof w, "%.200s", why.s ? why.s : ""); ev("built", "sibs", "seed", P->seed, "n", n, "ok", 0, "why", w); free(why.s);
        } else ev("built", "sibs", "seed", P->seed, "n", n, "ok", 0, "why", "the runner said OK but there is no STL file");
    }
}
#include "plate3mf.c"          /* the print plate: the picked bases in one 3MF (the user, 2026-11-22) */
static int job_build(Args *a, const char *style) {
    const char *size = opt(a, "--size", "30"), *mini = opt(a, "--mini", ""), *sup = opt(a, "--supports", ""), *join = opt(a, "--join", ""), *smaller = opt(a, "--smaller", ""), *strict = opt(a, "--strict", ""), *picks = opt(a, "--picks", ""), *scale_s = opt(a, "--scale", "");
    const char *smooth = opt(a, "--smooth", ""), *rocks = opt(a, "--rocks", "");          /* smooth details, organic rocks on|off */
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
    {   /* --count N [--seed word]: N new bases (numbers 1..N) without picking them first (plan B step 3, 2026-11-21) */
        int cnt = atoi(opt(a, "--count", "0")); const char *sw = opt(a, "--seed", "");
        if (!ng && cnt > 0) {
            char w[16]; if (sw[0]) snprintf(w, sizeof w, "%.15s", sw);
            else { uint64_t r = GetTickCount64() * 0x9e3779b97f4a7c15ULL ^ (uint64_t)GetCurrentProcessId() * 0xc2b2ae3d27d4eb4fULL; for (int i = 0; i < 5; i++) { w[i] = (char)('a' + (r >> 20) % 26); r = r * 6364136223846793005ULL + 1442695040888963407ULL; } w[5] = 0; }
            seeds[0] = xstrdup(w); nums[0] = malloc(sizeof(int) * 1024); nn[0] = 0; ng = 1;
            for (int i = 1; i <= cnt && i <= 1024; i++) nums[0][nn[0]++] = i;
        }
    }
    { extern void kinds_init(void); extern char kd_themes_note[600]; kinds_init(); if (kd_themes_note[0]) ev("log", "s", "text", kd_themes_note); }          /* themes.json: say what was taken */
    if (!ng) { ev("error", "s", "text", "nothing was picked (give --picks seed:n,... or --count N [--seed word])"); return 2; }
    char ts[32], dest[MAX_PATH * 3]; strftime_now(ts, sizeof ts, "%Y%m%d_%H%M%S");
    {   /* review P2-6 / N5 (2026-11-22): --folder picked_x puts this build in a batch's folder under out (several sizes:
           one folder); else a new picked_<time>, with a number added when that folder is already there (two jobs in one second) */
        const char *fo = opt(a, "--folder", ""); int okn = fo[0] && !strpbrk(fo, "\\/:.");
        if (okn) snprintf(dest, sizeof dest, "%s\\%s", out_root(), fo);
        else { snprintf(dest, sizeof dest, "%s\\picked_%s", out_root(), ts); if (GetFileAttributesA(dest) != INVALID_FILE_ATTRIBUTES) { char s2[40]; snprintf(s2, sizeof s2, "_%lu", (unsigned long)(GetTickCount() % 100000u)); strncat(dest, s2, sizeof dest - strlen(dest) - 1); } }
    }
    make_dirs(dest);
    ev("folder", "s", "folder", dest);
    snprintf(LIVE_DIR, sizeof LIVE_DIR, "%s\\.live", out_root()); make_dirs(LIVE_DIR);          /* gui_job: KIT_LIVE for the window's live view */
    int okc = 0, bad = 0;
    for (int g = 0; g < ng; g++) {
        /* run_set.py <style> 16,17, --seed x --size 60 -q [--mini --supports --scale] */
        RunSet rs; memset(&rs, 0, sizeof rs); InitializeCriticalSection(&rs.lock); rs.style = style; rs.salt = seeds[g];
        double A, B; int ov; read_size(size, &A, &B, &ov); rs.SIZE = A; rs.oval = ov; rs.OVAL[0] = A; rs.OVAL[1] = B;
        if (rs.oval) { char x[32], y[32]; fmt_g(x, sizeof x, A); fmt_g(y, sizeof y, B); snprintf(rs.size_tag, sizeof rs.size_tag, "%sx%s", x, y); snprintf(rs.ovtag, sizeof rs.ovtag, "%sx%s%s", x, y, rs.oval == 2 ? "r" : "");
            if (rs.oval == 2) { if (A == B) snprintf(rs.size_tag, sizeof rs.size_tag, "%ssq", x); else snprintf(rs.size_tag, sizeof rs.size_tag, "%sx%sr", x, y); } }          /* names: 25sq, 25x50r */
        else fmt_g(rs.size_tag, sizeof rs.size_tag, A);
        int known = 0; for (int i = 0; i < 17; i++) if (KNOWN_SIZES[i] == A) known = 1;
        int okv = 0; for (int i = 0; i < 7; i++) if (OVAL_SIZES[i][0] == A && OVAL_SIZES[i][1] == B) okv = 1;
        if (rs.oval == 2) { static const double RS[6][2] = { { 20, 20 }, { 25, 25 }, { 40, 40 }, { 50, 50 }, { 50, 25 }, { 100, 50 } }; int okr = 0; for (int i = 0; i < 6; i++) if (RS[i][0] == A && RS[i][1] == B) okr = 1;
            if (!okr) rs_print("note: %s is not one of the common square or rectangle sizes (20sq, 25sq, 40sq, 50sq, 25x50r, 50x100r); building it anyway", rs.size_tag); }
        else if (rs.oval && !okv) rs_print("note: %s mm is not one of the common oval sizes (60x35.5, 75x42, 90x52, 105x70, 120x92, 150x95, 170x109); building it anyway", rs.size_tag);
        if (!rs.oval && !known) rs_print("note: %s mm is not one of the common base sizes (20, 25, 28, 28.5, 30, 32, 40, 50, 60, 65, 70, 80, 90, 100, 120, 130, 160); building it anyway", rs.size_tag);
        rs.SCALE = scale_s[0] ? strtod(scale_s, NULL) : cfg_num("scenery_scale", 1.0);
        if (rs.SCALE != 1.0) { char w[360]; snprintf(w, sizeof w, "note: scenery scale %g (--scale / scenery_scale) makes the whole layout bigger, so the base shows part of it; it overrides --obj. For the size of things on the base use --obj (1 classic, 3 true for 32 mm) and leave --scale at 1.", rs.SCALE); ev("log", "s", "text", w); }          /* plan B4: one scale setting (--obj); --scale kept, said */
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
        if (!rs.workers) rs.workers = workers_auto();          /* step 5: half the cores, at most 4 bases at once (was a third, at most 12) */
        rs.nums = nums[g]; rs.nnums = nn[g];
        /* the layout helpers' environment: gui_job's + run_set's + build_once's */
        EnvSet es[24]; int ne = 0;
        es[ne++] = (EnvSet){ "PYTHONWARNINGS", "ignore" }; es[ne++] = (EnvSet){ "PYTHONUNBUFFERED", "1" }; es[ne++] = (EnvSet){ "PYTHONIOENCODING", "utf-8" };
        es[ne++] = (EnvSet){ "KIT_LIVE", LIVE_DIR }; es[ne++] = (EnvSet){ "SALT", rs.salt }; es[ne++] = (EnvSet){ "BASE_MM", lay }; es[ne++] = (EnvSet){ "SETDIR", rs.out };
        es[ne++] = (EnvSet){ "KIT_QUIET", "1" }; es[ne++] = (EnvSet){ "KIT_WORKER", "1" }; es[ne++] = (EnvSet){ "KIT_OVAL", rs.oval ? rs.ovtag : NULL };
        if (rs.SCALE != 1.0) { es[ne++] = (EnvSet){ "KIT_FINAL_MM", fin }; es[ne++] = (EnvSet){ "KIT_SCALE", skl }; }
        char obv[32]; double obd = obj_of(opt(a, "--obj", ""), rs.SCALE); fmt_g(obv, sizeof obv, obd); if (obd > 0) es[ne++] = (EnvSet){ "KIT_OBJ", obv };
        if (mini[0]) es[ne++] = (EnvSet){ "KIT_MINI", mini };
        if (sup[0]) es[ne++] = (EnvSet){ "KIT_SUPPORTS", !strcmp(sup, "on") ? "1" : "0" };
        if (on_off(smooth) >= 0) es[ne++] = (EnvSet){ "KIT_SMOOTH", on_off(smooth) ? "1" : "0" };
        if (on_off(rocks) >= 0) es[ne++] = (EnvSet){ "KIT_ROCKS", on_off(rocks) ? "1" : "0" };
        { const char *nmo = opt(a, "--name", ""); if (nmo[0]) NAME_CLI = nmo; }
        { const char *pr = opt(a, "--printer", ""); static char pb[16]; snprintf(pb, sizeof pb, "%s", pr[0] ? pr : cfg_str("printer", "fdm")); _putenv_s("KIT_PRINTER", pb); es[ne++] = (EnvSet){ "KIT_PRINTER", pb }; }          /* review N1 */
        { const char *st = opt(a, "--strength", ""); static char sb[32]; if (st[0]) STRENGTH_CLI = strtod(st, NULL); snprintf(sb, sizeof sb, "%g", strength()); es[ne++] = (EnvSet){ "KIT_STRENGTH", sb }; }
        { static char thb[128]; theme_arg(opt(a, "--theme", ""), thb, sizeof thb); if (thb[0]) es[ne++] = (EnvSet){ "KIT_THEME", thb }; }          /* only this theme (the window's theme picker) */
        rs.envw = env_block(es, ne);
        SUP_CLI = !strcmp(sup, "on") ? 1 : !strcmp(sup, "off") ? 0 : -1;          /* --supports: CFG allow_supports for the checks */
        JOIN_CLI = on_off(join); SMALL_CLI = on_off(smaller); STRICT_CLI = on_off(strict); MEND_CLI = on_off(opt(a, "--mend", "")); SMOOTH_CLI = on_off(smooth); ROCKS_CLI = on_off(rocks);
        int *seen = calloc(100000, sizeof(int)); PickCtx P = { style, size, rs.salt, dest, rs.SCALE, seen, 0, 0 };
        if (scale_s[0]) P.scale = strtod(scale_s, NULL); else P.scale = cfg_num("scenery_scale", 1.0);
        P.obj = obj_of(opt(a, "--obj", ""), rs.SCALE); P.mm = rs.SIZE; P.mini = mini; { const char *fg = opt(a, "--figure", ""); P.fig = fg[0] ? strtod(fg, NULL) : 32.0; }
        rs.done = picked_done; rs.done_ctx = &P;
        run_style(&rs);
        for (int i = 0; i < rs.nfree; i++) bridge_stop(&rs.free_br[i]);
        for (int i = 0; i < rs.nnums; i++) if (!seen[rs.nums[i]]) { P.bad++; char w[120]; snprintf(w, sizeof w, "the build ended (code 0) before this base was done"); ev("built", "sibs", "seed", rs.salt, "n", rs.nums[i], "ok", 0, "why", w); }
        okc += P.okc; bad += P.bad;
        free(seen); free(rs.envw); free(rs.res); for (int i = 0; i < rs.nrecent; i++) free(rs.recent[i].H); free(rs.recent); free(rs.free_br); DeleteCriticalSection(&rs.lock);
    }
    {   int pv = on_off(opt(a, "--plate", ""));          /* the print plate: --plate on|off, else settings print_plate (OFF by default) */
        if (pv < 0) pv = cfg_bool("print_plate", 0);
        if (pv && okc) plate_write(dest, printer_resin()); }
    ev("done", "siis", "job", "build", "ok", okc, "failed", bad, "folder", dest);
    for (int g = 0; g < ng; g++) { free(seeds[g]); free(nums[g]); }
    return 0;
}

/* ================================================================ runner_main: gui_job.py */
#include "models_job.c"          /* --job models: the Models preview pictures */
int runner_main(int argc, char **argv, runner_out_fn out, void *ctx) {
    if (!OUTL_ready) { InitializeCriticalSection(&OUTL); OUTL_ready = 1; }
    OUTF = out; OUTC = ctx; set_kit(); load_cfg(); pyx_load();          /* the UCRT cos / sin (magnet spots, compare_bases rotations) */
    char **av = malloc(sizeof(char *) * (argc + 1)); for (int i = 0; i < argc; i++) av[i] = argv[i];
    Args a = { argc, av };
    if (a.argc >= 1 && !strcmp(a.argv[0], "themes")) {          /* --job themes [--out file]: every built-in theme, to copy into themes.json */
        extern int kd_themes_write(const char *path); char p[MAX_PATH * 3]; const char *o = opt(&a, "--out", "");
        if (o[0]) snprintf(p, sizeof p, "%s", o); else { WCHAR w[MAX_PATH]; GetModuleFileNameW(NULL, w, MAX_PATH); WCHAR *s = wcsrchr(w, L'\\'); if (s) *s = 0; WideCharToMultiByte(CP_UTF8, 0, w, -1, p, (int)sizeof p - 24, NULL, NULL); strcat(p, "\\themes.builtin.json"); }
        int r3 = kd_themes_write(p); if (r3 == 0) ev("done", "ss", "job", "themes", "file", p); else ev("error", "s", "text", "the themes file could not be written");
        free(av); return r3 ? 2 : 0;
    }
    if (a.argc >= 1 && !strcmp(a.argv[0], "models")) { int r2 = job_models(opt(&a, "--pack", "")); free(av); return r2; }          /* the window's Models preview */
    if (a.argc < 2 || (strcmp(a.argv[0], "choices") && strcmp(a.argv[0], "build"))) { ev("error", "s", "text", "usage: bbb_kit.exe --job choices|build <style> [--size N] [--picks seed:n,...] [--count N] [--seed word] [--obj 1..3 (size of things; 3 = true beside 32 mm)] [--strength 0.25..2.5] [--theme name] [--printer fdm|resin] [--name {style}_{size}mm_{theme}_{seed}_{n}] [--supports on|off]; --job themes [--out file]"); free(av); return 2; }
    char job[16], style[64]; snprintf(job, sizeof job, "%s", a.argv[0]); snprintf(style, sizeof style, "%s", a.argv[1]);
    int rc;
    /* speed plan step 5 (2026-11-22, the user games while bases build): the job and every helper it starts (they inherit
       it) run below normal, so a game in front keeps the CPU; idle cores still go to the kit. settings below_normal_priority */
    if (runner_cfg_bool("below_normal_priority", 1)) SetPriorityClass(GetCurrentProcess(), BELOW_NORMAL_PRIORITY_CLASS);
    if (!strcmp(job, "choices")) rc = job_choices(&a, style);
    else rc = job_build(&a, style);
    free(av);
    return rc;
}
