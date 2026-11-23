/* compose.c: scenes.py compose and all of survey group G9 in C (see compose.h). Each C function names the Python
   routine it copies. Exceptions: the arena helpers of layouts.h raise through ly_jb (longjmp), as Python raises;
   a try / except of the Python is cp_try() here. */
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>
#include <setjmp.h>
#include "compose.h"
#include "layouts.h"
#include "bld_features.h"
#include "pyexact.h"
#include "pyx.h"

CpHooks CPH;
CpCfg CPC;

/* ================================================================ exceptions */
static int cp_exc = SC_ERR;              /* what the raise in flight is: SC_ERR (Exception) or CP_EXIT (SystemExit) */
static void cp_raise(int code) __attribute__((noreturn));
static void cp_raise(int code) { cp_exc = code; ly_raise(); }
#define R_(x) LC(x)                      /* a status of another part: < 0 raises */
static int RX(int r) { if (r == CP_EXIT) cp_raise(CP_EXIT); if (r < 0) cp_raise(SC_ERR); return r; }
/* try: fn(arg) under its own jmp_buf. 0 = it finished, SC_ERR = an Exception was caught. A SystemExit goes on up. */
static int cp_try(void (*fn)(void *), void *arg) {
    jmp_buf jb, *old = ly_jb; int r = 0;
    ly_jb = &jb; cp_exc = SC_ERR;
    if (setjmp(jb) == 0) fn(arg);
    else r = cp_exc;
    ly_jb = old;
    if (r == CP_EXIT) cp_raise(CP_EXIT);
    return r;
}

/* ================================================================ detail functions */
ScFn *cp_fn_new(const char *qualname, int (*run)(ScFn *, Scene *, PyRandom *, int), void *ctx, void (*ctx_free)(void *)) {
    ScFn *f = (ScFn *)calloc(1, sizeof *f);
    f->rc = 1; f->qualname = qualname ? so_intern(qualname) : ""; f->run = run; f->ctx = ctx; f->ctx_free = ctx_free;
    return f;
}
ScFn *cp_fn_ref(ScFn *f) { if (f) f->rc++; return f; }
void cp_fn_unref(ScFn *f) { if (f && --f->rc == 0) { if (f->ctx_free) f->ctx_free(f->ctx); free(f); } }
void cp_fnl_push(ScFnList *l, ScFn *f) {
    if (l->n == l->cap) { l->cap = l->cap ? 2 * l->cap : 16; l->v = (ScFn **)realloc(l->v, sizeof(ScFn *) * (size_t)l->cap); }
    l->v[l->n++] = cp_fn_ref(f);
}
void cp_fnl_free(ScFnList *l) { int i; for (i = 0; i < l->n; i++) cp_fn_unref(l->v[i]); free(l->v); memset(l, 0, sizeof *l); }
/* borrowed pushes (lists that live inside one compose: the fns are held by FNKEEP) */
static void fl_add(ScFnList *l, ScFn *f) {
    if (l->n == l->cap) { l->cap = l->cap ? 2 * l->cap : 16; l->v = (ScFn **)realloc(l->v, sizeof(ScFn *) * (size_t)l->cap); }
    l->v[l->n++] = f;
}
static ScFnList FNKEEP;                  /* every fn compose made (prop_fn): released when compose ends */
static void keep_fn(ScFn *f) { if (f) { cp_fnl_push(&FNKEEP, f); cp_fn_unref(f); } }

/* samplers and aims made during one compose (released at its end) */
static SmAt **ATK; static int nATK, capATK; static SmAim **AIK; static int nAIK, capAIK;
static SmAt *KA(SmAt *a) { if (a) { if (nATK == capATK) { capATK = capATK ? 2 * capATK : 256; ATK = (SmAt **)realloc(ATK, sizeof *ATK * (size_t)capATK); } ATK[nATK++] = a; } return a; }
static SmAim *KI(SmAim *a) { if (a) { if (nAIK == capAIK) { capAIK = capAIK ? 2 * capAIK : 256; AIK = (SmAim **)realloc(AIK, sizeof *AIK * (size_t)capAIK); } AIK[nAIK++] = a; } return a; }
static void release_samplers(void) { int i; for (i = 0; i < nATK; i++) sm_at_unref(ATK[i]); for (i = 0; i < nAIK; i++) sm_aim_unref(AIK[i]); nATK = nAIK = 0; }

/* ================================================================ settings */
void cp_config_default(void) {
    memset(&CPC, 0, sizeof CPC);
    CPC.objects_amount = "1"; CPC.max_slenderness = 3.0; CPC.big_base_height_factor = 1.5; CPC.no_bare_ground = 1;
    CPC.style_details = 1; CPC.style_details_amount = 0.5; CPC.clear_middle = 0.45; CPC.miniature = "biped";
}
void cp_config(void) {
    CPH.subject = (CpSubjectFn)bld_any("scenes", "SUBJECTS", "CpSubjectFn");
    CPH.prop = (CpPropFn)bld_any("scenes", "prop_fn", "CpPropFn");
    CPH.caught = NULL; CPH.u = NULL;
}
static const char *env_or(const char *v) { return v && *v ? v : NULL; }          /* os.environ.get(k) or ... */

/* ================================================================ small helpers */
#define H_ (SC.H)
static Geo *U(const GeoList *l) { return LAG(sm_u(l)); }                         /* scenes._u */
static int E(Geo *g) { return g_isempty(g); }
static double area(Geo *g) { return g_area(g); }
static int terr(Scene *s) { return pl_terr(s); }
static Geo *LIn(Geo *a, Geo *b) { return g_inter(a, b); }
static int intersects(Geo *a, Geo *b) { return g_intersects(a, b); }
static double pmax(double a, double b) { return b > a ? b : a; }                  /* max(a, b): the first of equal */
static double pmin(double a, double b) { return b < a ? b : a; }
static double ppow(double x, double y) { double re = 0, im = 0; int r = px_pow(x, y, &re, &im); if (r != 0) cp_raise(SC_ERR); return re; }   /* a complex result: raised (never met) */
static double pmod(double a, double b) { double m; if (px_mod_d(a, b, &m) != 0) cp_raise(SC_ERR); return m; }
static int64_t print_(double x) { return ly_rint(x); }                           /* round(x) -> int */
static double pround(double x, int nd) { return px_round(x, nd); }
static int ksame(const Kind *k, const char *s) { return kind_is(k, s); }
static int in_bridge(const Kind *k) { return kd_kind_in(KD.bridge, k); }
static int in_soft(const Kind *k) { return kd_kind_in(KD.soft, k); }
static int in_terrk(const Kind *k) { return kd_kind_in(KD.terrk, k); }
static int in_wetk(const Kind *k) { return kd_kind_in(KD.wetk, k); }
int pl_is_wet(const Kind *k) { return kd_kind_in(KD.wetk, k); }
int pl_is_terrk(const Kind *k) { return kd_kind_in(KD.terrk, k); }          /* water / tar / lava (scene.c leaves those cuts as they are) */
static double share(void);
static void cp_log(Sc *sc, const char *where) {          /* sc.log.append(kitlog.caught(where, e)) */
    const char *line = CPH.caught ? CPH.caught(CPH.u, where) : NULL; char buf[600];
    if (!line) { snprintf(buf, sizeof buf, "%s: error", where); line = buf; }
    if (sc->log.n == sc->log.cap) { sc->log.cap = sc->log.cap ? 2 * sc->log.cap : 16; sc->log.v = (char **)realloc(sc->log.v, sizeof(char *) * (size_t)sc->log.cap); }
    sc->log.v[sc->log.n++] = _strdup(line);
}
static void set_geo(Geo **slot, Geo *g) { Geo *o = *slot; *slot = geo_ref(g); geo_unref(o); }          /* sc.x = g (holds its own reference) */

/* KVal number helpers */
static double KN(const KVal *x) { if (!kv_is_num(x)) cp_raise(SC_ERR); return kv_num(x); }
static KVal *KG(const KVal *d, const char *k) { KVal *x = kv_get(d, k); if (!x) cp_raise(SC_ERR); return x; }          /* d[k]: KeyError */
static const char *KS_(const KVal *d, const char *k) { KVal *x = KG(d, k); if (x->t != KV_STR) cp_raise(SC_ERR); return x->s; }
static KVal *kv_num_typed(double v, int is_int) { return is_int ? kv_int((int64_t)v) : kv_float(v); }

/* Python's repr(float) (the shortest text that reads back), str() of the plan values used in the notes */
static void py_repr_float(double v, char *out, size_t n) {
    char b[40]; int p;
    if (isnan(v)) { snprintf(out, n, "nan"); return; }
    if (isinf(v)) { snprintf(out, n, v > 0 ? "inf" : "-inf"); return; }
    for (p = 1; p <= 17; p++) { snprintf(b, sizeof b, "%.*e", p - 1, v); if (strtod(b, NULL) == v) break; }
    {   /* b = d.ddde[+-]XX with p digits: Python's float_repr_style 'short' */
        char dig[24]; int nd = 0, e, i; const char *q = b; int neg = 0;
        if (*q == '-') { neg = 1; q++; }
        for (; *q && *q != 'e'; q++) if (*q >= '0' && *q <= '9') dig[nd++] = *q;
        e = atoi(q + 1);
        while (nd > 1 && dig[nd - 1] == '0') nd--;
        {
            char r[64]; size_t k = 0;
            if (neg) r[k++] = '-';
            if (e < -4 || e >= 16) {
                r[k++] = dig[0];
                if (nd > 1) { r[k++] = '.'; for (i = 1; i < nd; i++) r[k++] = dig[i]; }
                k += (size_t)snprintf(r + k, sizeof r - k, "e%c%02d", e < 0 ? '-' : '+', e < 0 ? -e : e);
            } else if (e < 0) {
                r[k++] = '0'; r[k++] = '.'; for (i = 0; i < -e - 1; i++) r[k++] = '0'; for (i = 0; i < nd; i++) r[k++] = dig[i];
            } else {
                for (i = 0; i <= e; i++) r[k++] = i < nd ? dig[i] : '0';
                r[k++] = '.';
                if (nd > e + 1) for (i = e + 1; i < nd; i++) r[k++] = dig[i]; else r[k++] = '0';
            }
            r[k] = 0; snprintf(out, n, "%s", r);
        }
    }
}
static void pystr(const KVal *x, char *out, size_t n) {          /* str(x) for the scalar plan values */
    if (!x || x->t == KV_NONE) snprintf(out, n, "None");
    else if (x->t == KV_BOOL) snprintf(out, n, x->i ? "True" : "False");
    else if (x->t == KV_INT) snprintf(out, n, "%lld", (long long)x->i);
    else if (x->t == KV_FLOAT) py_repr_float(x->f, out, n);
    else if (x->t == KV_STR) snprintf(out, n, "%s", x->s);
    else snprintf(out, n, x->t == KV_LIST ? "[...]" : "{...}");          /* a list / dict as a name: never met */
}
static void note(KVal *notes, const char *fmt, ...) __attribute__((format(printf, 2, 3)));
#include <stdarg.h>
static void note(KVal *notes, const char *fmt, ...) { char b[1200]; va_list a; va_start(a, fmt); vsnprintf(b, sizeof b, fmt, a); va_end(a); kv_push(notes, kv_str(b)); }
static int truthy(const KVal *x) {          /* bool(x) */
    if (!x) return 0;
    switch (x->t) { case KV_NONE: return 0; case KV_BOOL: case KV_INT: return x->i != 0; case KV_FLOAT: return x->f != 0.0; case KV_STR: return x->s[0] != 0; default: return x->n > 0; }
}
/* the first n code points of a UTF-8 string */
static void utf8_cut(char *s, int n) { int k = 0; char *p = s; while (*p) { if (((unsigned char)*p & 0xC0) != 0x80) { if (k == n) { *p = 0; return; } k++; } p++; } }

/* ================================================================ samplers.h glue */
static SmAt *NEAR(Geo *g, double d0, double d1) { SmAt *a = NULL; R_(sm_near(g, d0, d1, 0, NULL, NULL, 4, &a)); return KA(a); }
static SmAt *NEARS(Geo *g, double d0, double d1, int side, const double *c, const double *front) { SmAt *a = NULL; R_(sm_near(g, d0, d1, side, c, front, 4, &a)); return KA(a); }
static SmAt *MIX2(double w0, SmAt *f0, double w1, SmAt *f1) { double w[2] = { w0, w1 }; SmAt *f[2] = { f0, f1 }; return KA(sm_mix(w, f, 2)); }
static SmAt *MIXN(const double *w, SmAt *const *f, int n) { return KA(sm_mix(w, f, n)); }
static SmAt *RIM(double d0, double d1) { return KA(sm_rim(d0, d1)); }
static SmAt *NOT(SmAt *f, Geo *avoid, double d) { return KA(sm_not(f, avoid, d)); }
static SmAim *APAR(Geo *g, double jit) { SmAim *a = NULL; R_(sm_a_par(g, jit, &a)); return KI(a); }
static SmAim *ATO(double x, double y, double jit) { return KI(sm_a_to(x, y, jit)); }
static Geo *HULL(Geo *g) { return g_hull(g); }

/* ================================================================ Sc.relation */
/* (sampler, aim) for a relation name; (None, None) when this scene has nothing of that kind */
static void relation(Sc *sc, const char *where, SmAt **at, SmAim **aim) {
    Geo *sub = sc->subj, *pg = sc->pathg;
    *at = NULL; *aim = NULL;
    if ((!strcmp(where, "path") || !strcmp(where, "start")) && !pg) where = "at";
    if (!strcmp(where, "also") && !sc->also.n) where = "at";
    if (!strcmp(where, "yard") && !sc->yard) where = "front";
    if (!sub && (!strcmp(where, "at") || !strcmp(where, "wall") || !strcmp(where, "front") || !strcmp(where, "back") || !strcmp(where, "in"))) return;
    if (!strcmp(where, "at")) { *at = NEAR(sub, 1.2, 5.5); return; }
    if (!strcmp(where, "wall")) { *at = NEAR(sub, 1.0, 3.0); *aim = APAR(HULL(sub), 0.12); return; }
    if (!strcmp(where, "front")) { SmAt *n = NEARS(sub, 1.5, 5.0, SM_FRONT, sc->c, sc->front); *at = NOT(n, sc->yard, 0.3); *aim = ATO(sc->c[0], sc->c[1], 0.15); return; }
    if (!strcmp(where, "back")) { *at = NEARS(sub, 1.2, 5.0, SM_BACK, sc->c, sc->front); *aim = APAR(HULL(sub), 0.12); return; }
    if (!strcmp(where, "in")) {
        Geo *g = g_buf(sub, -1.6);
        if (!E(g)) { SmAt *a = NULL; R_(sm_in(g, &a)); *at = KA(a); } else *at = NEAR(sub, 1.2, 4.0);
        return;
    }
    if (!strcmp(where, "path")) { *at = NEAR(LIn(pg, sc->disc), 1.0, 4.5); *aim = APAR(pg, 0.44); return; }
    if (!strcmp(where, "start")) {
        Geo *g = LIn(LIn(pg, g_ptbuf(sc->start[0], sc->start[1], 9.0, 16)), sc->disc);
        *at = !E(g) ? NEAR(g, 1.0, 4.0) : NEAR(LIn(pg, sc->disc), 1.0, 4.5); *aim = APAR(pg, 0.3); return;
    }
    if (!strcmp(where, "yard")) {
        Geo *c; *at = NEAR(sc->yard, -0.5, 2.5); c = g_centroid(sc->yard);
        *aim = ATO(g_x(c), g_y(c), 0.15); return;
    }
    if (!strcmp(where, "rim")) { relation(sc, sub ? "back" : "far", at, aim); return; }
    if (!strcmp(where, "also")) { *at = NEAR(U(&sc->also), 1.2, 4.5); return; }
    if (!strcmp(where, "far")) { GeoList *l = LL(); if (sub) gl_push(l, sub); if (pg) gl_push(l, pg); *at = KA(sm_far(U(l), sc->mm < 99 ? 8.0 : 10.0)); return; }
}

/* ================================================================ the plan */
static int64_t num_of(const char *name) {          /* _num(n): int(str(n).rsplit("_", 1)[-1]), else 1 */
    const char *p = strrchr(name, '_'), *q; char b[64]; size_t k = 0; int dig = 0, us = 0; char *e;
    p = p ? p + 1 : name;
    while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r' || *p == '\f' || *p == '\v') p++;
    q = p; if (*q == '+' || *q == '-') b[k++] = *q++;
    for (; *q && k < 60; q++) {
        if (*q >= '0' && *q <= '9') { b[k++] = *q; dig = 1; us = 0; }
        else if (*q == '_' && dig && !us && q[1] >= '0' && q[1] <= '9') us = 1;
        else break;
    }
    while (*q == ' ' || *q == '\t' || *q == '\n' || *q == '\r' || *q == '\f' || *q == '\v') q++;
    if (!dig || *q) return 1;
    b[k] = 0; return _strtoi64(b, &e, 10);
}
static const KVal *themes_of(const char *style) {
    const KStyle *st = kd_style(style); const char *want = getenv("KIT_THEME"); static KVal one; static KVal *pv[1]; int i;
    if (!st) return NULL;
    if (want && *want && st->themes)          /* --theme name (the window's theme picker, plan B step 6): only that theme, when the style has it */
        for (i = 0; i < st->themes->n; i++) { const char *nm = kv_s(st->themes->v[i], "theme"); if (nm && !strcmp(nm, want)) { memset(&one, 0, sizeof one); one.t = KV_LIST; one.n = one.cap = 1; pv[0] = st->themes->v[i]; one.v = pv; return &one; } }
    return st->themes;
}

int cp_theme_order(const char *style, const char *salt, int64_t *out, int *n) {          /* theme_order(style, salt) */
    const KVal *th = themes_of(style); int i; PyRandom r; char seed[512];
    *n = th ? th->n : 0;
    for (i = 0; i < *n; i++) out[i] = i;
    snprintf(seed, sizeof seed, "themes%s%s", style, salt ? salt : "");
    py_seed_str(&r, seed); py_shuffle_i64(&r, out, *n);
    return 0;
}
double cp_model_room = 1.0;          /* settings "model_room": the model's space times this (features.c ft_model_space) */
static int na_of(double mm) { return mm < 59 ? 0 : mm < 79 ? 1 : mm < 129 ? 2 : mm < 159 ? 3 : 4; }          /* _na */
static double big_of(double mm) { return ft_figure() ? 1.0 : mm >= 59.9 ? 1.2 : 1.0; }        /* _big */

/* split on whitespace into a static word array (borrowed copies) */
typedef struct { char *buf; char *w[256]; int n; } Words;
static void words(Words *W, const char *s) {
    char *p; W->n = 0; W->buf = (char *)LM(strlen(s) + 1); strcpy(W->buf, s); p = W->buf;
    for (;;) {
        while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r' || *p == '\f' || *p == '\v') p++;
        if (!*p) break;
        if (W->n < 256) W->w[W->n++] = p;
        while (*p && !(*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r' || *p == '\f' || *p == '\v')) p++;
        if (*p) *p++ = 0;
    }
}
static int cmp_str(const void *a, const void *b) { return strcmp(*(const char *const *)a, *(const char *const *)b); }

KVal *cp_expand(const KVal *th, PyRandom *mixr, double mm, const char *style) {          /* expand(th, mixr, mm, style) */
    double sa = py_uniform(mixr, 0, 360); const char *path = KS_(th, "path"), *colon = strchr(path, ':');
    char kind[64], route[64]; Words L; int nl, keep, i, na; int64_t ch[256]; char chosen[256];
    KVal *props = kv_list(), *also = kv_list(), *cl, *plan, *sub, *pth, *rim, *x; const char *subject = KS_(th, "subject");
    if (colon) { snprintf(kind, sizeof kind, "%.*s", (int)(colon - path), path); snprintf(route, sizeof route, "%s", colon + 1); }
    else { snprintf(kind, sizeof kind, "%s", path); route[0] = 0; }
    words(&L, KS_(th, "props")); nl = L.n;
    keep = nl > 2 ? (int)pmax(1, (double)print_(nl * py_uniform(mixr, 0.6, 1.0))) : nl;
    if (py_sample_e(mixr, nl, keep, ch) != 0) cp_raise(SC_ERR);
    memset(chosen, 0, sizeof chosen); for (i = 0; i < keep; i++) chosen[ch[i]] = 1;
    for (i = 0; i < nl; i++) {
        char *w = L.w[i], *c1, *st, what[128], cnt[64]; KVal *d;
        if (!chosen[i]) continue;
        c1 = strchr(w, ':'); if (!c1 || strchr(c1 + 1, ':')) cp_raise(SC_ERR);          /* where, what = w.split(":") */
        *c1 = 0; st = strchr(c1 + 1, '*');
        if (st) { snprintf(what, sizeof what, "%.*s", (int)(st - c1 - 1), c1 + 1); snprintf(cnt, sizeof cnt, "%s", st + 1); } else { snprintf(what, sizeof what, "%s", c1 + 1); cnt[0] = 0; }
        d = kv_dict(); kv_set(d, "what", kv_str(what)); kv_set(d, "where", kv_str(w));
        if (cnt[0]) { char *e; int64_t v = _strtoi64(cnt, &e, 10); if (*e) cp_raise(SC_ERR); kv_set(d, "count", kv_int(v)); } else kv_set(d, "count", kv_int(1));
        kv_push(props, d); *c1 = ':';
    }
    na = na_of(mm);
    {   /* also = [... for w in th.get("also", "").split()][:na]: every entry draws its size */
        const char *a = kv_s(th, "also"); Words A; int k = 0;
        words(&A, a ? a : "");
        for (i = 0; i < A.n; i++) {
            char *w = A.w[i], *c1 = strchr(w, ':'); KVal *d = kv_dict(); char nm[128];
            if (!c1) cp_raise(SC_ERR);          /* w.split(":")[1]: IndexError */
            snprintf(nm, sizeof nm, "%.*s", (int)(c1 - w), w);
            { char *c2 = strchr(c1 + 1, ':'); char wh[128]; snprintf(wh, sizeof wh, "%.*s", c2 ? (int)(c2 - c1 - 1) : (int)strlen(c1 + 1), c1 + 1);
              kv_set(d, "name", kv_str(nm)); kv_set(d, "where", kv_str(wh)); }
            kv_set(d, "size", kv_float(pround(py_uniform(mixr, 0.62, 0.85), 2)));
            if (k < na) { kv_push(also, d); k++; } else kv_free(d);
        }
    }
    if (also->n < na && style && *style) {
        const KVal *ths = themes_of(style); const char *more[64]; int nm = 0, j; int64_t idx[64];
        for (i = 0; ths && i < ths->n; i++) {
            const char *s2 = kv_s(ths->v[i], "subject"); int dup = 0;
            if (!s2 || !kd_has(&KD.also_ok, s2) || !strcmp(s2, subject)) continue;
            for (j = 0; j < also->n; j++) if (!strcmp(kv_s(also->v[j], "name"), s2)) dup = 1;
            for (j = 0; j < nm; j++) if (!strcmp(more[j], s2)) dup = 1;
            if (!dup && nm < 64) more[nm++] = s2;
        }
        qsort((void *)more, (size_t)nm, sizeof *more, cmp_str);
        for (j = 0; j < nm; j++) idx[j] = j;
        py_shuffle_i64(mixr, idx, nm);
        for (j = 0; j < nm && also->n < na; j++) {
            KVal *d = kv_dict(); int64_t c; static const char *W3[3] = { "far", "far", "path" };
            kv_set(d, "name", kv_str(more[idx[j]]));
            if (py_choice_e(mixr, 3, &c) != 0) cp_raise(SC_ERR);
            kv_set(d, "where", kv_str(W3[c]));
            kv_set(d, "size", kv_float(pround(py_uniform(mixr, 0.5, 0.7), 2)));
            kv_push(also, d);
        }
    }
    cl = kv_dict(); kv_set(cl, "debris", kv_float(1.0)); kv_set(cl, "growth", kv_float(1.0)); kv_set(cl, "marks", kv_float(1.0));
    if ((x = kv_get(th, "clutter"))) for (i = 0; i < x->n; i++) kv_set(cl, x->k[i], kv_copy(x->v[i]));
    plan = kv_dict();
    kv_set(plan, "theme", kv_copy(KG(th, "theme")));
    sub = kv_dict(); kv_set(sub, "name", kv_str(subject)); kv_set(sub, "edge", kv_bool(!strcmp(subject, "portal")));
    kv_set(sub, "angle", kv_float(pround(sa, 1)));
    kv_set(sub, "offset", kv_float(pround(py_uniform(mixr, 0.12, 0.42), 2)));
    kv_set(sub, "size", kv_float(pround(py_uniform(mixr, 0.85, 1.15), 2)));
    kv_set(sub, "height", kd_has(&KD.tall, subject) ? kv_float(pround(py_uniform(mixr, 1.3, 3.0), 1)) : kv_float(1.0));
    kv_set(plan, "subject", sub);
    pth = kv_dict(); kv_set(pth, "kind", kv_str(kind)); kv_set(pth, "route", kv_str(route[0] ? route : "to"));
    kv_set(pth, "from_angle", kv_float(pround(pmod(sa + py_uniform(mixr, 110, 250), 360), 1)));
    kv_set(pth, "bend", kv_float(pround(py_uniform(mixr, -0.22, 0.22), 2)));
    kv_set(plan, "path", pth);
    kv_set(plan, "also", also); kv_set(plan, "props", props); kv_set(plan, "clutter", cl);
    x = kv_get(th, "open"); kv_set(plan, "open", kv_float(x ? KN(x) : 1.0));
    rim = kv_dict();
    {
        int yes = kd_has(&KD.enclosed, style) || (kd_has(&KD.cliffs, style) && py_random(mixr) < 0.35);
        kv_set(rim, "cover", yes ? kv_float(pround(py_uniform(mixr, 0.12, 0.24), 2)) : kv_float(0.0));
        kv_set(rim, "height", kv_float(pround(py_uniform(mixr, 1.5, 3.5), 1)));
    }
    kv_set(plan, "rim", rim);
    if (truthy(kv_get(th, "pal"))) kv_set(plan, "pal", kv_copy(kv_get(th, "pal")));
    if (truthy(kv_get(th, "ban"))) kv_set(plan, "ban", kv_copy(kv_get(th, "ban")));
    return plan;
}

static void lower(char *s) { for (; *s; s++) if (*s >= 'A' && *s <= 'Z') *s = (char)(*s - 'A' + 'a'); }
static void strip_ws(char *s) {
    char *a = s, *b; while (*a == ' ' || *a == '\t' || *a == '\n' || *a == '\r' || *a == '\f' || *a == '\v') a++;
    memmove(s, a, strlen(a) + 1); b = s + strlen(s);
    while (b > s && (b[-1] == ' ' || b[-1] == '\t' || b[-1] == '\n' || b[-1] == '\r' || b[-1] == '\f' || b[-1] == '\v')) *--b = 0;
}
int cp_pick_plans(const char *style, PyRandom *mixr, double mm, int n, KVal *plans) {          /* pick_plans(style, mixr, mm, n) */
    const KVal *ths = themes_of(style); int i, no, k; int64_t order[64];
    if (!ths || !ths->n) return 0;
    if (CPC.theme) {
        char forced[256]; snprintf(forced, sizeof forced, "%s", CPC.theme); strip_ws(forced); lower(forced);
        if (forced[0]) {
            for (i = 0; i < ths->n; i++) { char t[256]; snprintf(t, sizeof t, "%s", KS_(ths->v[i], "theme")); lower(t); if (!strcmp(t, forced)) { kv_push(plans, cp_expand(ths->v[i], mixr, mm, style)); return 0; } }
            return CP_EXIT;          /* raise SystemExit(f"unknown theme ...") */
        }
    }
    cp_theme_order(style, CPC.salt ? CPC.salt : "", order, &no);
    k = (int)(((n - 1) % 5 + (n - 1) / 5) % no);
    for (i = k; i < no; i++) kv_push(plans, cp_expand(ths->v[order[i]], mixr, mm, style));
    for (i = 0; i < k; i++) kv_push(plans, cp_expand(ths->v[order[i]], mixr, mm, style));
    return 0;
}

/* _clamp(v, lo, hi, dflt) -> (value, out of range): lo / hi / dflt carry their Python type (1 = int) */
typedef struct { double v; int is_int; } TN;
static TN clampv(const KVal *x, double lo, int lo_i, double hi, int hi_i, TN dflt, int *bad) {
    double v; TN r;
    if (!x) { r.v = 0; r.is_int = 0; *bad = 1; return dflt; }          /* (a missing key gives the caller's default value, never None here) */
    if (x->t == KV_INT || x->t == KV_BOOL) v = (double)x->i;
    else if (x->t == KV_FLOAT) v = x->f;
    else if (x->t == KV_STR) { if (ft_pyfloat(x->s, &v) != 0) { *bad = 1; return dflt; } }
    else { *bad = 1; return dflt; }          /* float(None / list / dict): TypeError */
    r.v = v; r.is_int = 0;
    if (!(v > lo)) { r.v = lo; r.is_int = lo_i; }          /* max(lo, v) */
    if (!(r.v < hi)) { r.v = hi; r.is_int = hi_i; }        /* min(hi, ...) */
    *bad = !(lo <= v && v <= hi);
    return r;
}
static TN tn(double v, int i) { TN t; t.v = v; t.is_int = i; return t; }
static KVal *tnv(TN t) { return kv_num_typed(t.v, t.is_int); }
static TN kvtn(const KVal *x) { return tn(kv_num(x), x->t == KV_INT || x->t == KV_BOOL); }

int cp_legal_paths(const char *style, const char **out, int cap) {
    const KStyle *st = kd_style(style); Words W; int i, n = 0;
    if (!st) cp_raise(SC_ERR);
    words(&W, st->path_ok);
    for (i = 0; i < W.n && n < cap - 1; i++) out[n++] = so_intern(W.w[i]);
    out[n++] = "none";
    return n;
}
int cp_prop_name(const char *name) { int i; if (!name) return 0; for (i = 0; i < KD.nprop_names; i++) if (!strcmp(KD.prop_names[i], name)) return 1; return 0; }
int cp_prop_fn(const char *name, const ScPal *pal, ScFn **out) {          /* prop_fn(name, pal) */
    *out = NULL;
    if (!CPH.prop) return 0;
    CPH.prop(name, pal, out);          /* prop_fn catches whatever the maker raised: None */
    return 0;
}
static int is_rel(const char *w) { int i; for (i = 0; i < KD.nrelations; i++) if (!strcmp(KD.relations[i], w)) return 1; return 0; }
static int mini_hover(void) {          /* mini_type() == "hover" (no rng: "mix" draws from the module's random) */
    static PyRandom glob; static int seeded; const char *m;
    if (!seeded) { py_seed_u64(&glob, 0); seeded = 1; }
    if (sm_mini_type(CPC.kit_mini ? CPC.kit_mini : CPC.miniature ? CPC.miniature : "biped", &glob, &m) < 0) cp_raise(CP_EXIT);
    return !strcmp(m, "hover");
}

int cp_validate_plan(const KVal *plan, const char *style, double mm, const char *const *used, int nused, KVal **out, KVal *notes0) {
    jmp_buf jb, *old = ly_jb; volatile int st = 0; KVal *volatile p = NULL; KVal *volatile notes = kv_list();
    *out = NULL;
    ly_jb = &jb; cp_exc = SC_ERR;
    if (setjmp(jb) == 0) {
        ScPal pal; KVal *sub, *x, *pth, *raw, *props, *kept, *also, *cl, *rm, *pp; char th[256], nm[256]; const char *name; int edge, fixes = 0, b, i;
        TN ang, off, size, hgt, fa, bend; const char *kind; const char *ok[32]; int nok; const char *route; double tot; int na;
        const KStyle *sty = kd_style(style);
        if (!plan || plan->t != KV_DICT) { note(notes, "not a plan"); goto done; }
        if (!sty) cp_raise(SC_ERR);
        p = kv_copy(plan);
        kd_pal_copy(&pal, &sty->pal);
        if (truthy(kv_get(p, "pal"))) {
            x = kv_get(p, "pal"); if (x->t != KV_DICT) cp_raise(SC_ERR);
            for (i = 0; i < x->n; i++) { int f; kd_pal_get(&sty->pal, x->k[i], &f); if (f && x->v[i]->t == KV_STR) kd_pal_set(&pal, x->k[i], x->v[i]->s); }
        }
        {   /* p["theme"] = " ".join(str(p.get("theme") or "").split())[:40] or "a place" */
            Words W; char raw_[1024], j[1024]; size_t k = 0;
            x = kv_get(p, "theme");
            if (truthy(x)) pystr(x, raw_, sizeof raw_); else raw_[0] = 0;
            words(&W, raw_); j[0] = 0;
            for (i = 0; i < W.n; i++) k += (size_t)snprintf(j + k, sizeof j - k, "%s%s", i ? " " : "", W.w[i]);
            utf8_cut(j, 40);
            snprintf(th, sizeof th, "%s", j[0] ? j : "a place");
            kv_set(p, "theme", kv_str(th));
        }
        for (i = 0; i < nused; i++) { char a[256], c[256]; snprintf(a, sizeof a, "%s", th); snprintf(c, sizeof c, "%s", used[i]); lower(a); lower(c); if (!strcmp(a, c)) { kv_free(p); p = NULL; kv_free(notes); notes = kv_list(); note(notes, "theme '%s' already used", th); goto done; } }
        x = kv_get(p, "subject"); sub = x && x->t == KV_DICT ? x : NULL;
        x = sub ? kv_get(sub, "name") : NULL; pystr(x, nm, sizeof nm);
        if (x && (x->t == KV_LIST || x->t == KV_DICT)) cp_raise(SC_ERR);          /* unhashable */
        name = x && x->t == KV_STR ? x->s : NULL;
        if (!(name && !strcmp(name, "bridge")) && !(name && kd_subject(name))) { kv_free(p); p = NULL; kv_free(notes); notes = kv_list(); note(notes, "unknown subject '%s'", nm); goto done; }
        edge = truthy(sub ? kv_get(sub, "edge") : NULL);
        if (!strcmp(name, "portal") && !edge) { edge = 1; note(notes, "portal moved to the rim"); }
        if (edge) { int e = 0; for (i = 0; i < KD.nedge_ok; i++) if (!strcmp(KD.edge_ok[i], name)) e = 1; if (!e) { edge = 0; note(notes, "%s cannot sit on the rim", name); } }
        { KVal dz = { KV_FLOAT }; KVal *v;
          v = sub ? kv_get(sub, "angle") : NULL; ang = v ? clampv(v, 0, 1, 360, 1, tn(0.0, 0), &b) : (b = 1, tn(0.0, 0)); if (!v) b = 1; fixes += b;          /* sub.get("angle"): None -> float(None) fails */
          v = sub ? kv_get(sub, "offset") : NULL; dz.f = 0.3; off = clampv(v ? v : &dz, 0.0, 0, 0.5, 0, tn(0.3, 0), &b); fixes += b;
          v = sub ? kv_get(sub, "size") : NULL; dz.f = 1.0; size = clampv(v ? v : &dz, 0.6, 0, mini_hover() ? 1.6 : 1.3, 0, tn(1.0, 0), &b); fixes += b;
          v = sub ? kv_get(sub, "height") : NULL; dz.f = 1.0; hgt = clampv(v ? v : &dz, 0.5, 0, 20.0, 0, tn(1.0, 0), &b); fixes += b; }
        if (!kd_has(&KD.tall, name) && strcmp(name, "bridge") && hgt.v > 1.5) { hgt = tn(1.5, 0); note(notes, "%s lies low: height 1.5", name); }
        sub = kv_dict(); kv_set(sub, "name", kv_str(name)); kv_set(sub, "edge", kv_bool(edge)); kv_set(sub, "angle", tnv(ang)); kv_set(sub, "offset", tnv(off));
        kv_set(sub, "size", tnv(size)); kv_set(sub, "height", tnv(hgt)); kv_set(p, "subject", sub);
        x = kv_get(p, "path"); pth = x && x->t == KV_DICT ? x : NULL;
        x = pth ? kv_get(pth, "kind") : NULL;
        if (x && x->t != KV_STR) { char ks[256]; if (x->t == KV_LIST || x->t == KV_DICT) cp_raise(SC_ERR); pystr(x, ks, sizeof ks); kv_free(p); p = NULL; kv_free(notes); notes = kv_list(); note(notes, "unknown path '%s'", ks); goto done; }
        kind = x ? x->s : "none";
        nok = cp_legal_paths(style, ok, 32);
        if (kd_path(kind) < 0 && strcmp(kind, "none")) { kv_free(p); p = NULL; kv_free(notes); notes = kv_list(); note(notes, "unknown path '%s'", kind); goto done; }
        if (!strcmp(name, "bridge") && (!strcmp(kind, "none") || !strcmp(kind, "stream"))) { kind = "stones"; note(notes, "bridge needs a crossing: stones"); }
        { int good = 0; for (i = 0; i < nok; i++) if (!strcmp(ok[i], kind)) good = 1; if (!good) { note(notes, "path %s does not suit %s: %s", kind, style, ok[0]); kind = ok[0]; } }
        kind = so_intern(kind);
        x = pth ? kv_get(pth, "route") : NULL;
        route = x && x->t == KV_STR && (!strcmp(x->s, "to") || !strcmp(x->s, "past")) ? (!strcmp(x->s, "to") ? "to" : "past") : "to";
        if (!strcmp(kind, "stream") && !strcmp(route, "to") && strcmp(name, "pool") && strcmp(name, "well") && strcmp(name, "crater") && strcmp(name, "tentacles")) { route = "past"; note(notes, "a stream runs past, not to"); }
        {   /* fa, b = _clamp(pth.get("from_angle", ang + 180), 0, 360, (ang + 180) % 360) */
            TN a180 = tn(ang.v + 180, ang.is_int), dfl; KVal dv; KVal *v = pth ? kv_get(pth, "from_angle") : NULL;
            dfl = ang.is_int ? tn((double)(((int64_t)a180.v % 360 + 360) % 360), 1) : tn(pmod(a180.v, 360), 0);
            memset(&dv, 0, sizeof dv); dv.t = a180.is_int ? KV_INT : KV_FLOAT; dv.i = (int64_t)a180.v; dv.f = a180.v;
            fa = clampv(v ? v : &dv, 0, 1, 360, 1, dfl, &b); fixes += b;
            {   double t = pmod(fa.v - ang.v + 180, 360) - 180;
                if (fabs(t) < (edge ? 110 : 70)) { fa = dfl; note(notes, "path start moved to the far side"); } }
        }
        { KVal dz = { KV_FLOAT }; KVal *v = pth ? kv_get(pth, "bend") : NULL; dz.f = 0.0; bend = clampv(v ? v : &dz, -0.3, 0, 0.3, 0, tn(0.0, 0), &b); fixes += b; }
        pth = kv_dict(); kv_set(pth, "kind", kv_str(kind)); kv_set(pth, "route", kv_str(route)); kv_set(pth, "from_angle", tnv(fa)); kv_set(pth, "bend", tnv(bend)); kv_set(p, "path", pth);
        props = kv_list(); x = kv_get(p, "props"); raw = x && x->t == KV_LIST ? x : NULL;
        for (i = 0; raw && i < raw->n; i++) {
            KVal *pr = raw->v[i], *wv, *wh, *d; char ws[256], whs[256]; const char *what, *where; ScFn *f = NULL; TN cnt; KVal one = { KV_INT };
            if (pr->t != KV_DICT) continue;
            wv = kv_get(pr, "what"); wh = kv_get(pr, "where");
            what = wv && wv->t == KV_STR ? wv->s : NULL; pystr(wv, ws, sizeof ws);
            if (!what || !cp_prop_name(what)) { note(notes, "dropped unknown object '%s'", ws); continue; }
            cp_prop_fn(what, &pal, &f);
            if (!f) { note(notes, "dropped unknown object '%s'", ws); continue; }
            cp_fn_unref(f);
            if (wh && (wh->t == KV_LIST || wh->t == KV_DICT)) { pystr(wh, whs, sizeof whs); note(notes, "dropped '%s': no such place '%s'", what, whs); continue; }
            where = wh ? (wh->t == KV_STR ? wh->s : NULL) : "at";
            if (!where || !is_rel(where)) { pystr(wh, whs, sizeof whs); note(notes, "dropped '%s': no such place '%s'", what, whs); continue; }
            one.i = 1; cnt = clampv(kv_get(pr, "count") ? kv_get(pr, "count") : &one, 1, 1, 4, 1, tn(1, 1), &b); fixes += b;
            d = kv_dict(); kv_set(d, "what", kv_str(what)); kv_set(d, "where", kv_str(where)); kv_set(d, "count", kv_int(cnt.is_int ? (int64_t)cnt.v : print_(cnt.v)));
            kv_push(props, d);
        }
        if (raw && raw->n && !props->n) { kv_free(props); kv_free(p); p = NULL; note(notes, "no usable objects"); goto done; }
        if (props->n > 12) { while (props->n > 12) kv_free(props->v[--props->n]); note(notes, "kept the first 12 object lines"); }
        tot = 0; kept = kv_list();
        for (i = 0; i < props->n; i++) {
            double c = (double)kv_get(props->v[i], "count")->i;
            if (tot + c > 10 * pmax(1.0, SC.K)) { note(notes, "too many objects: rest dropped"); break; }
            tot += c; kv_push(kept, kv_copy(props->v[i]));
        }
        kv_free(props); kv_set(p, "props", kept);
        na = na_of(mm); also = kv_list(); x = kv_get(p, "also");
        for (i = 0; x && x->t == KV_LIST && i < x->n; i++) {
            KVal *al = x->v[i], *an, *aw; KVal dz = { KV_FLOAT }; TN sz;
            if (al->t != KV_DICT) continue;
            an = kv_get(al, "name"); aw = kv_get(al, "where");
            if (an && (an->t == KV_LIST || an->t == KV_DICT)) cp_raise(SC_ERR);
            if (!(an && an->t == KV_STR && kd_subject(an->s) && strcmp(an->s, "portal"))) continue;
            if (aw && aw->t != KV_STR) { if (aw->t == KV_LIST || aw->t == KV_DICT) continue; continue; }
            if (!is_rel(aw ? aw->s : "far")) continue;
            dz.f = 0.7; sz = clampv(kv_get(al, "size") ? kv_get(al, "size") : &dz, 0.4, 0, 1.0, 0, tn(0.7, 0), &b);
            { KVal *d = kv_dict(); kv_set(d, "name", kv_copy(an)); kv_set(d, "where", kv_str(aw ? aw->s : "far")); kv_set(d, "size", tnv(sz)); kv_push(also, d); }
        }
        if (also->n > na) { while (also->n > na) kv_free(also->v[--also->n]); note(notes, "second features cut to %d", na); }
        kv_set(p, "also", also);
        x = kv_get(p, "clutter"); cl = kv_dict();
        { static const char *CK[3] = { "debris", "growth", "marks" }; int k;
          for (k = 0; k < 3; k++) { KVal dz = { KV_FLOAT }; KVal *v = x && x->t == KV_DICT ? kv_get(x, CK[k]) : NULL; dz.f = 1.0; kv_set(cl, CK[k], tnv(clampv(v ? v : &dz, 0, 1, 2, 1, tn(1.0, 0), &b))); } }
        kv_set(p, "clutter", cl);
        { KVal dz = { KV_FLOAT }; KVal *v = kv_get(p, "open"); dz.f = 1.0; kv_set(p, "open", tnv(clampv(v ? v : &dz, 0.5, 0, 1.5, 0, tn(1.0, 0), &b))); }
        x = kv_get(p, "rim"); rm = kv_dict();
        { KVal dz = { KV_FLOAT }; KVal *v = x && x->t == KV_DICT ? kv_get(x, "cover") : NULL; dz.f = 0.0; kv_set(rm, "cover", tnv(clampv(v ? v : &dz, 0.0, 0, 0.3, 0, tn(0.0, 0), &b)));
          v = x && x->t == KV_DICT ? kv_get(x, "height") : NULL; dz.f = 2.0; kv_set(rm, "height", tnv(clampv(v ? v : &dz, 0.5, 0, 20.0, 0, tn(2.0, 0), &b))); }
        kv_set(p, "rim", rm);
        if (truthy(kv_get(p, "pal"))) {
            KVal *o = kv_get(p, "pal"), *d = kv_dict(); int f;
            for (i = 0; i < o->n; i++) { kd_pal_get(&sty->pal, o->k[i], &f); if (f && o->v[i]->t == KV_STR) kv_set(d, o->k[i], kv_copy(o->v[i])); }
            kv_set(p, "pal", d);
        }
        if ((pp = kv_get(p, "ban")) && pp->t != KV_NONE) {
            KVal *d = kv_list();
            if (pp->t == KV_LIST) for (i = 0; i < pp->n; i++) { char s2[512]; pystr(pp->v[i], s2, sizeof s2); kv_push(d, kv_str(s2)); }
            kv_set(p, "ban", d);
        }
        if (fixes) note(notes, "%d numbers put back in range", fixes);
        kd_pal_free(&pal);
    done:;
    } else { st = cp_exc; kv_free(p); p = NULL; }
    ly_jb = old;
    if (st < 0) { kv_free(notes); return st; }
    { int i; for (i = 0; i < notes->n; i++) kv_push(notes0, kv_copy(notes->v[i])); }
    kv_free(notes);
    *out = p;
    return 0;
}

/* ================================================================ removing and reading the layout */
static void ids_of_cells(Scene *s, SmIdSet *c) { memset(c, 0, sizeof *c); sm_cells(s, c); }

static void read_layout(Sc *sc) {          /* read_layout(sc) */
    Scene *s = sc->s; SmIdSet cells, inops; int i, j; SmPlate *pl = NULL; int npl = 0; GeoList *l;
    ids_of_cells(s, &cells); memset(&inops, 0, sizeof inops);
    for (i = 0; i < s->ops.n; i++) for (j = 0; j < s->ops.v[i]->polys.n; j++) sm_ids_add(&inops, s->ops.v[i]->polys.v[j]);
    gl_clear(&sc->anchors);
    for (i = 0; i < s->ops.n; i++) {
        Op *op = s->ops.v[i]; double top;
        if (in_bridge(op->kind) || in_soft(op->kind)) continue;
        top = op->type == OP_PRISM ? op->n[1].v : num_add(op->n[0], op->n[1]).v;
        for (j = 0; j < op->polys.n; j++) {
            Geo *p = op->polys.v[j];
            if (area(p) > 12 && !sm_ids_has(&cells, p) && top - H_ >= 1.0 && op->n[0].v <= H_ + 0.05) gl_push(&sc->anchors, p);
        }
    }
    gl_clear(&sc->tall_cells);
    if (s->has_terr) for (i = 0; i < s->terr.n; i++) if (sm_ids_has(&inops, s->terr.v[i].fp) && s->terr.v[i].zt.v - H_ >= 1.0) gl_push(&sc->tall_cells, s->terr.v[i].fp);
    R_(sm_plates(s, &pl, &npl));
    l = LL(); for (i = 0; i < npl; i++) gl_push(l, pl[i].g);
    set_geo(&sc->floor, npl ? LIn(U(l), sc->disc) : g_empty());
    sm_plates_free(pl, npl);
    {
        Geo *fe = !E(sc->floor) ? LIn(LAG(geo_new(shp_boundary(sc->floor->g))), g_ptbuf(0, 0, SC.R0 - 2.0, 16)) : NULL;
        Geo *e = (fe && !E(fe) && g_len(fe) > 4) ? g_buf(fe, 0.2) : NULL;
        set_geo(&sc->floor_edge, e);
    }
    l = LL();
    for (i = 0; i < s->gcuts.n; i++) if (in_wetk(s->gcuts.v[i].kind)) for (j = 0; j < s->gcuts.v[i].polys.n; j++) if (area(s->gcuts.v[i].polys.v[j]) > 4) gl_push(l, s->gcuts.v[i].polys.v[j]);
    set_geo(&sc->waters, LIn(U(l), sc->disc));
    l = LL();
    for (i = 0; i < s->gcuts.n; i++) {
        Kind *k = s->gcuts.v[i].kind;
        if (!(ksame(k, "rut") || ksame(k, "wash") || ksame(k, "water") || ksame(k, "lava") || ksame(k, "bile") || ksame(k, "acid"))) continue;
        for (j = 0; j < s->gcuts.v[i].polys.n; j++) { Geo *p = s->gcuts.v[i].polys.v[j]; double L = g_len(p); if (L > 30 && area(p) / L < 4.0) gl_push(l, p); }
    }
    set_geo(&sc->lines, LIn(U(l), sc->disc));
    sc->paved = !E(sc->floor) && area(sc->floor) > 0.8 * area(sc->disc);
    sm_ids_free(&cells); sm_ids_free(&inops);
}

static char *layout_facts(Sc *sc) {          /* layout_facts(sc) */
    Scene *s = sc->s; int i, j, nk = 0, cap = 0; struct { Kind *k; double a; } *ks = NULL; char out[2048]; size_t n = 0; const char *top[3]; int ntop = 0;
    for (i = 0; i < s->ops.n; i++) {
        Op *op = s->ops.v[i]; double *as = NULL, sum; int np = op->polys.n;
        if (in_soft(op->kind)) continue;
        as = (double *)LM(sizeof(double) * (size_t)(np + 1));
        for (j = 0; j < np; j++) as[j] = area(op->polys.v[j]);
        sum = px_sum(as, np);
        for (j = 0; j < nk; j++) {          /* the dict's key match: an LK only equals itself; a str equals anything of its text */
            if ((ks[j].k->lk && op->kind->lk) ? ks[j].k == op->kind : strcmp(ks[j].k->s, op->kind->s) == 0) break;
        }
        if (j == nk) { if (nk == cap) { cap = cap ? 2 * cap : 32; ks = realloc(ks, sizeof *ks * (size_t)cap); } ks[nk].k = op->kind; ks[nk].a = 0; nk++; }
        ks[j].a = ks[j].a + sum;
    }
    {   /* sorted(kinds.items(), key=-area)[:3] (stable), a > 0.03 * disc.area */
        double *key = (double *)LM(sizeof(double) * (size_t)(nk + 1)); int64_t *idx = (int64_t *)LM(sizeof(int64_t) * (size_t)(nk + 1)); double da = 0.03 * area(sc->disc);
        for (i = 0; i < nk; i++) key[i] = -ks[i].a;
        if (nk) R_(px_sorted_idx(key, nk, 1, idx) == 0 ? 0 : SC_ERR);
        for (i = 0; i < nk && i < 3; i++) if (ks[idx[i]].a > da) {
            char *t = (char *)LM(strlen(ks[idx[i]].k->s) + 1), *q; strcpy(t, ks[idx[i]].k->s);
            for (q = t; *q; q++) if (*q == '_') *q = ' ';
            top[ntop++] = t;
        }
    }
    free(ks);
    if (sc->paved) n += (size_t)snprintf(out + n, sizeof out - n, "a floor of %s all over", ntop ? top[0] : "plates");
    else if (!E(sc->floor) && area(sc->floor) > 0.15 * area(sc->disc)) n += (size_t)snprintf(out + n, sizeof out - n, "a floor of %s over part of it, bare ground beside", ntop ? top[0] : "plates");
    else if (ntop) { n += (size_t)snprintf(out + n, sizeof out - n, "bare ground with "); for (i = 0; i < ntop; i++) n += (size_t)snprintf(out + n, sizeof out - n, "%s%s", i ? ", " : "", top[i]); }
    else n += (size_t)snprintf(out + n, sizeof out - n, "bare open ground");
    if (!E(sc->waters)) {
        const char *k = "water";
        for (i = 0; i < s->gcuts.n; i++) if (in_wetk(s->gcuts.v[i].kind)) { k = s->gcuts.v[i].kind->s; break; }
        n += (size_t)snprintf(out + n, sizeof out - n, ", %s of %s", !E(sc->lines) ? "a channel" : "pools", k);
    }
    if (sc->anchors.n >= 2) n += (size_t)snprintf(out + n, sizeof out - n, ", %d big rocks or blocks", sc->anchors.n);
    return _strdup(out);
}

/* ================================================================ features: layers */
static const char *ltag(const SoLayer *L) { return L && L->n > 0 && L->v[0].t == LV_STR ? L->v[0].s : ""; }
static Geo *lgeo(const SoLayer *L, int i) {          /* L[i] as a handle (a clone in the arena), NULL = None */
    if (i >= L->n || L->v[i].t != LV_GEOM || !L->v[i].g) return NULL;
    return LA(shp_clone(L->v[i].g));
}
/* _solid_of(layers, amin) */
static Geo *solid_of(const SoLayers *lay, double amin) {
    GeoList *l = LL(); int i, j;
    for (i = 0; i < lay->n; i++) {
        const SoLayer *L = lay->v[i]; const char *t; GeoList *ps; Geo *g;
        if (!L) cp_raise(SC_ERR);          /* None[0]: TypeError */
        t = ltag(L);
        if (strcmp(t, "up") && strcmp(t, "mound") && strcmp(t, "gcut") && strcmp(t, "steps")) continue;
        if (L->n < 2) cp_raise(SC_ERR);
        if (L->v[1].t != LV_GEOM || !L->v[1].g) cp_raise(SC_ERR);          /* polys_of(None) */
        g = lgeo(L, 1); ps = g_polys(g);
        for (j = 0; j < ps->n; j++) if (area(ps->v[j]) >= amin) gl_push(l, ps->v[j]);
    }
    return U(l);
}
static double lnum(const SoVal *v) {
    if (v->t == LV_INT) return (double)v->i;
    if (v->t == LV_FLOAT) return v->f;
    cp_raise(SC_ERR);
}
static SoLayer *ly_copy(const SoLayer *L) {          /* a new tuple of the same items (geometries cloned: owned per layer) */
    SoLayer *y = ly_new(L->n); int i;
    for (i = 0; i < L->n; i++) {
        y->v[i] = L->v[i];
        if (L->v[i].t == LV_GEOM && L->v[i].g) y->v[i].g = shp_clone(L->v[i].g);
        if (L->v[i].t == LV_LATER && L->v[i].later) so_later_ref(L->v[i].later);
    }
    return y;
}
static double thick_of(const SoLayer *L) {          /* min((4 * q.area / q.length for q in polys_of(L[1]) if q.length > 0), default=1.0) */
    Geo *g = lgeo(L, 1); GeoList *ps; int j, any = 0; double m = 1.0;
    if (!g) cp_raise(SC_ERR);
    ps = g_polys(g);
    for (j = 0; j < ps->n; j++) { double len = g_len(ps->v[j]), v; if (!(len > 0)) continue; v = 4 * area(ps->v[j]) / len; if (!any || v < m) m = v; any = 1; }
    return m;
}
/* _taller(layers, height, boost) -> out (layers shared by reference where unchanged) */
static void taller(const SoLayers *lay, double height, double boost, SoLayers *out) {
    int i; double sl = CPC.max_slenderness;
    if (height <= 1.001) { for (i = 0; i < lay->n; i++) lyl_push(out, lay->v[i]); return; }
    for (i = 0; i < lay->n; i++) {
        const SoLayer *L = lay->v[i]; const char *t; int ix; double f;
        if (!L) cp_raise(SC_ERR);
        t = ltag(L);
        if (!strcmp(t, "steps")) {
            const SoPieces *P; SoPieces q = { 0 }; double hs = 0, thick; int k; SoLayer *y; SoVal pv;
            f = height;
            if (L->n < 3) cp_raise(SC_ERR);
            P = ft_pieces(&L->v[2]);
            if (!P) cp_raise(SC_ERR);
            if (!P->n) cp_raise(SC_ERR);          /* max() of nothing */
            hs = P->v[0].ztop; for (k = 1; k < P->n; k++) if (P->v[k].ztop > hs) hs = P->v[k].ztop;
            thick = thick_of(L);
            if (sl > 0) { if (hs * boost == 0) cp_raise(SC_ERR); f = pmax(1.0, pmin(f, sl * thick / (hs * boost))); }
            for (k = 0; k < P->n; k++) {          /* (p[0], p[1] * f) + ((p[2], p[3] * f) if len(p) > 3 else ()) + tuple(p[4:]) */
                SoPiece x = P->v[k]; x.g = x.g ? shp_clone(x.g) : NULL; x.ztop = x.ztop * f; x.z = x.z * f;
                if (q.n == q.cap) { q.cap = q.cap ? 2 * q.cap : 16; q.v = (SoPiece *)realloc(q.v, sizeof(SoPiece) * (size_t)q.cap); }
                q.v[q.n++] = x;
            }
            y = ly_copy(L);
            if (y->v[2].t == LV_LATER) so_later_unref(y->v[2].later);
            memset(&pv, 0, sizeof pv); pv.t = LV_PTR; pv.p = ft_steps_list(&q); y->v[2] = pv;
            sop_free(&q);
            lyl_push_own(out, y); continue;
        }
        ix = !strcmp(t, "up") ? 2 : !strcmp(t, "mound") ? 2 : !strcmp(t, "on") ? 3 : -1;
        if (ix < 0) { lyl_push(out, (SoLayer *)L); continue; }
        if (ix >= L->n) cp_raise(SC_ERR);
        if (lnum(&L->v[ix]) < 1.0 || L->v[1].t != LV_GEOM || !L->v[1].g || shp_is_empty(L->v[1].g)) { lyl_push(out, (SoLayer *)L); continue; }
        f = height;
        if (sl > 0) { double thick = thick_of(L), den = lnum(&L->v[ix]) * boost; if (den == 0) cp_raise(SC_ERR); f = pmax(1.0, pmin(f, sl * thick / den)); }
        { SoLayer *y = ly_copy(L); y->v[ix] = lv_float(lnum(&L->v[ix]) * f); lyl_push_own(out, y); }
    }
}

/* ================================================================ placing a feature */
static Num surf(Scene *s, Geo *g) {          /* _surf(s, g) */
    Num z = num_f(H_); int br = 0, i; SmPlate *pl = NULL; int npl = 0; double ga = area(g);
    R_(sm_plates(s, &pl, &npl));
    for (i = 0; i < npl; i++) {
        Geo *x = geo_new(shp_intersection(pl[i].g->g, g->g)); double a;
        if (!x) { sm_plates_free(pl, npl); cp_raise(SC_ERR); }
        a = area(x); geo_unref(x);
        if (a > 0.05 * ga) { z = num_max2(z, pl[i].z); br = 1; }
    }
    sm_plates_free(pl, npl);
    return num_z(z.v, br);
}
static FtS3dList *S3D;                   /* s.solids3d of the scene compose makes */
static void uncover(Sc *sc, Geo *foot);
/* the model's space kept clear (true size, 2026-11-20): a detail standing more than 2 mm that reaches into it goes, all of it;
   low things (cracks, plates, thin rubble) stay, so the model sits on real ground */
static Geo *cp_msp; static int cp_mbox; static double cp_mhx, cp_mhy, cp_mrad;          /* the model's space of the base being made (NULL: none) */
static Geo *g_moved(Geo *g, double dx, double dy) { return LA(shp_translate(g->g, dx, dy)); }
static double op_top(const Op *o) { return o->type == OP_PRISM ? o->n[1].v : o->n[0].v + o->n[1].v; }
static double op_h(const Op *o) { return o->type == OP_PRISM ? o->n[1].v - o->n[0].v : o->n[1].v; }          /* one piece's own height */
/* how high a detail stands (the model's space): a tagged one from its lowest piece to its highest top (a spire's or a rock's tiers
   count in full); a loose piece from the deck to its top; anything on the scene's path (rails, sleepers) not at all */
static double stand_h(Sc *sc, const Op *o, Geo *pb) {          /* pb: the path, a little wider (NULL: none) */
    Scene *s = sc->s; int i, on = 0; double lo = 1e300, hi = -1e300, h;
    if (pb) { int j; on = o->polys.n > 0; for (j = 0; j < o->polys.n && on; j++) if (!g_contains(pb, o->polys.v[j])) on = 0; }
    if (o->feat <= 0) h = op_top(o) - SC.H;
    else { for (i = 0; i < s->ops.n; i++) if (s->ops.v[i]->feat == o->feat) { double z0 = s->ops.v[i]->n[0].v, t = op_top(s->ops.v[i]); if (z0 < lo) lo = z0; if (t > hi) hi = t; } h = hi - pmin(lo, SC.H); }          /* from the ground: a piece left standing on something moved away floats */
    return on && h <= 3.0 ? 0 : h;          /* on the path and low as a whole (rails, sleepers, pipe): part of it; a portal, frame or wreck over it is not */
}
/* a detail standing in the model's space is moved out of it, to the side strip (a box) or outward (a disc), when it fits there
   clear of the other standing details and inside the rim; else it goes */
/* a chamfered rock is built from its chop, not from its stand-in op (kind rockc, sharing the polygons): moving or dropping the op
   must do the same to its chop */
static void chops_of(Sc *sc, const Op *o, double dx, double dy, int drop) {
    Scene *s = sc->s; int i, j, w = 0;
    for (i = 0; i < s->chops.n; i++) {
        Chop *c = s->chops.v[i]; int hit = 0;
        for (j = 0; j < o->polys.n && !hit; j++) if (c->poly == o->polys.v[j] || (c->poly && g_intersects(c->poly, o->polys.v[j]) && fabs(area(c->poly) - area(o->polys.v[j])) < 0.25 * area(o->polys.v[j]) + 0.05 && area(LIn(c->poly, o->polys.v[j])) > 0.6 * area(o->polys.v[j]))) hit = 1;          /* the same polygon, or the same shape (a grown copy) */
        if (hit && drop) continue;          /* out of the list (its memory goes with the scene's) */
        if (hit) { Geo *g = g_moved(c->poly, dx, dy); int k; geo_ref(g); geo_unref(c->poly); c->poly = g;
            for (k = 0; k < c->ops.n; k++) { Op *q = c->ops.v[k], *n; GeoList ps = { 0 }; int z; for (z = 0; z < q->polys.n; z++) gl_push(&ps, g_moved(q->polys.v[z], dx, dy)); n = op_like(q, &ps); gl_free(&ps); op_unref(q); c->ops.v[k] = n; } }
        s->chops.v[w++] = c;
    }
    s->chops.n = w;
}
static int op_ground(const Scene *s, const Op *o);
static int move_out(Sc *sc, Geo *space, int f, int box, double hx, double hy, double rad) {
    Scene *s = sc->s; double bb[4], lo[2] = { 1e300, 1e300 }, hi[2] = { -1e300, -1e300 }, d[2][2]; int i, j, t, nt;
    Geo *inner = g_buf(sc->disc, -0.8);
    (void)hx;
    for (i = 0; i < s->ops.n; i++) if (s->ops.v[i]->feat == f) for (j = 0; j < s->ops.v[i]->polys.n; j++) { g_bounds(s->ops.v[i]->polys.v[j], bb); lo[0] = pmin(lo[0], bb[0]); lo[1] = pmin(lo[1], bb[1]); hi[0] = pmax(hi[0], bb[2]); hi[1] = pmax(hi[1], bb[3]); }
    if (lo[0] > hi[0]) return 0;
    if (box) {          /* up or down past the long side, the nearer first */
        double up = hy + 0.6 - lo[1], dn = -hy - 0.6 - hi[1];
        if (fabs(up) <= fabs(dn)) { d[0][0] = 0; d[0][1] = up; d[1][0] = 0; d[1][1] = dn; } else { d[0][0] = 0; d[0][1] = dn; d[1][0] = 0; d[1][1] = up; }
        nt = 2;
    } else {
        double cx = (lo[0] + hi[0]) / 2, cy = (lo[1] + hi[1]) / 2, L = sqrt(cx * cx + cy * cy), rf = pmax(hi[0] - lo[0], hi[1] - lo[1]) / 2, want = rad + rf + 0.6;
        if (L < 0.5) { cx = 1; cy = 0; L = 1; }
        d[0][0] = cx / L * want - cx; d[0][1] = cy / L * want - cy; nt = 1;
    }
    for (t = 0; t < nt; t++) {
        int ok = 1;
        for (i = 0; i < s->ops.n && ok; i++) {
            Op *o = s->ops.v[i];
            if (o->feat != f) continue;
            for (j = 0; j < o->polys.n && ok; j++) {
                Geo *m = g_moved(o->polys.v[j], d[t][0], d[t][1]); int k2;
                if (!g_contains(inner, m) || (g_intersects(m, space) && area(LIn(m, space)) > 0.3)) { ok = 0; break; }
                for (k2 = 0; k2 < s->ops.n && ok; k2++) { Op *q = s->ops.v[k2]; int z;
                    if (q->feat == f || op_ground(s, q) || op_top(q) <= o->n[0].v + 0.2) continue;          /* any standing piece is in the way (the layout's own rocks too), only the ground is not */
                    for (z = 0; z < q->polys.n; z++) if (g_intersects(m, q->polys.v[z]) && area(LIn(m, q->polys.v[z])) > 0.2) { ok = 0; break; } }
            }
        }
        if (!ok) continue;
        for (i = 0; i < s->ops.n; i++) {
            Op *o = s->ops.v[i], *n; GeoList ps = { 0 };
            if (o->feat != f) continue;
            chops_of(sc, o, d[t][0], d[t][1], 0);
            for (j = 0; j < o->polys.n; j++) gl_push(&ps, g_moved(o->polys.v[j], d[t][0], d[t][1]));
            n = op_like(o, &ps); gl_free(&ps); op_unref(o); s->ops.v[i] = n;
        }
        return 1;
    }
    return 0;
}
/* true size: the ground cuts (pits, holes, pools, grooves) cut back out of the model's space, so the model stands on solid ground */
static void clear_cuts(Sc *sc, Geo *space) {
    Scene *s = sc->s; int i, j, k;
    for (i = 0; i < s->gcuts.n; i++) {
        GeoList *pl = &s->gcuts.v[i].polys, np = { 0 }; int hit = 0;
        for (j = 0; j < pl->n; j++) if (intersects(pl->v[j], space)) { hit = 1; break; }
        if (!hit) continue;
        for (j = 0; j < pl->n; j++) {
            if (!intersects(pl->v[j], space)) { gl_push(&np, pl->v[j]); continue; }
            { GeoList *ps = g_polys(g_diff(pl->v[j], space)); for (k = 0; k < ps->n; k++) if (area(ps->v[k]) > 0.6 && g_wide(ps->v[k], SC.MINW)) gl_push(&np, ps->v[k]); }          /* no slivers left at its edge */
        }
        gl_free(pl); *pl = np;
    }
}
/* G2 (PLAN-better, 2026-11-22; the basing guides: a clear, level spot for the feet): settings "feet_pad", OFF by default.
   With it on, the model's space keeps only the ground and its grain: every detail that reaches into it goes (low ones too:
   rubble, plates laid as details, loose grit), and the cracks and cuts on the ground and on plates stop at its edge.
   With it off nothing here runs, so the bases are byte-identical to before. */
int cp_feet_pad = 0;
/* the pad is the figure's own footprint where it stands (the middle of the model's space), not the whole space: an oval
   0.55 x 0.38 of the figure's height (a 32 mm man: about 17.6 x 12.2 mm; 6 mm a metre times the object size, a man
   1.8 m), half as big again for a many-legged model (walker, tripod, quad, scorpion, crawler, many); a tank's box is its
   tracks, so the box itself. Never more than the model's space. BBB_PAD_LOG: each pad's size, a line per base. */
static Geo *pad_shape(Sc *sc) {
    double o = ft_obj_scale(), hf = o > 0 ? 10.8 * o : 32.0, a = 0.55 * hf / 2, b = 0.38 * hf / 2; const char *mn = CPC.kit_mini ? CPC.kit_mini : "";
    Geo *g;
    if (cp_mbox) g = cp_msp;
    else {
        if (!strcmp(mn, "walker") || !strcmp(mn, "tripod") || !strcmp(mn, "quad") || !strcmp(mn, "scorpion") || !strcmp(mn, "crawler") || !strcmp(mn, "many")) { a *= 1.5; b *= 1.5; }
        g = LIn(LA(shp_scale(g_ptbuf(0, 0, 1.0, 32)->g, a, b, SHP_ORIGIN_POINT, 0.0, 0.0)), cp_msp);
    }
    { const char *lp = getenv("BBB_PAD_LOG"); FILE *f = lp && *lp ? fopen(lp, "a") : NULL; double bb[4];
      if (f) { g_bounds(g, bb); fprintf(f, "base %.1f mm (%s, %s): pad %.1f x %.1f mm, model space %s %.1f mm\n", sc->mm, mn[0] ? mn : "default", cp_mbox ? "a tank's box" : "an oval", bb[2] - bb[0], bb[3] - bb[1], cp_mbox ? "box" : "disc", cp_mbox ? 2 * cp_mhx : 2 * cp_mrad); fclose(f); } }
    return g;
}
/* drop the details named in bad (a feature id > 0, or -(index + 1) for one loose piece), and with them every kept piece
   standing higher on one that goes (so nothing floats); used by the feet pad (G2) and the small-base cap (G1) */
static void drop_keys(Sc *sc, int **badp, int *nbp, int *capp) {
    Scene *s = sc->s; int i, k, nb = *nbp, cap = *capp, *bad = *badp; OpList keep = { 0 };
    for (;;) {          /* nothing left floating: a kept piece standing higher on a piece that goes, goes too (again until none) */
        int more = 0;
        for (i = 0; i < s->ops.n; i++) {
            Op *o = s->ops.v[i]; int key = o->feat > 0 ? o->feat : -(i + 1), d, gone = 0;
            for (k = 0; k < nb && bad[k] != key && !(bad[k] > 0 && o->feat == bad[k]); k++) {}
            if (k < nb || (op_ground(s, o) && !(o->kind && in_soft(o->kind)))) continue;
            for (d = 0; d < s->ops.n && !gone; d++) {
                Op *q = s->ops.v[d]; int qk = q->feat > 0 ? q->feat : -(d + 1), m, z, w;
                if (d == i || o->n[0].v <= q->n[0].v + 0.01) continue;
                for (m = 0; m < nb && bad[m] != qk; m++) {}
                if (m == nb) continue;
                for (z = 0; z < o->polys.n && !gone; z++) for (w = 0; w < q->polys.n && !gone; w++) if (g_intersects(o->polys.v[z], q->polys.v[w]) && area(LIn(o->polys.v[z], q->polys.v[w])) > 0.05) gone = 1;
            }
            if (gone) { if (nb == cap) { cap = cap ? 2 * cap : 16; bad = (int *)realloc(bad, sizeof(int) * (size_t)cap); } bad[nb++] = key; more = 1; }
        }
        if (!more) break;
    }
    if (nb) {
        for (i = 0; i < s->ops.n; i++) {
            Op *o = s->ops.v[i]; for (k = 0; k < nb && !(bad[k] > 0 ? o->feat == bad[k] : bad[k] == -(i + 1)); k++) {}
            if (k < nb) {
                { const char *lp = getenv("BBB_PAD_LOG"); FILE *f = lp && *lp ? fopen(lp, "a") : NULL; double bb[4] = { 0, 0, 0, 0 }, lo[2] = { 1e9, 1e9 }, hi[2] = { -1e9, -1e9 }; int z;
                  for (z = 0; z < o->polys.n; z++) { g_bounds(o->polys.v[z], bb); lo[0] = pmin(lo[0], bb[0]); lo[1] = pmin(lo[1], bb[1]); hi[0] = pmax(hi[0], bb[2]); hi[1] = pmax(hi[1], bb[3]); }
                  if (f) { fprintf(f, "  dropped: kind %s feat %d pieces %d from (%.1f, %.1f) to (%.1f, %.1f) top %.2f\n", o->kind ? o->kind->s : "?", o->feat, o->polys.n, lo[0], lo[1], hi[0], hi[1], op_top(o)); fclose(f); } }
                chops_of(sc, o, 0, 0, 1); op_unref(o);
            } else ol_push_own(&keep, o);
        }
        free(s->ops.v); s->ops = keep;
    }
    *badp = bad; *nbp = nb; *capp = cap;
}
/* G1 part 2 (PLAN-better, 2026-11-22; the basing guides: "a rocky base with grass and snow quickly looks overloaded on
   20-25 mm", "one point of interest"): settings "small_base_cap", OFF by default. On a base under 33 mm at most 4
   details (under 27 mm) or 5 stand on it: the ones laid last (the fill, the clutter) go first; the layout's ground,
   cracks and grain stay. Off: nothing here runs, the bases are as before. BBB_PAD_LOG also logs what it drops. */
int cp_small_cap = 0;
static void small_cap(Sc *sc) {
    Scene *s = sc->s; int i, k, n = 0, nf = 0, *fe = NULL, cap = 0, nb = 0, cb = 0, *bad = NULL, maxd = sc->mm < 26.9 ? 4 : 5;
    if (sc->mm >= 32.9) return;
    for (i = 0; i < s->ops.n; i++) {          /* the details on the base, by feature, in the order they were laid */
        Op *o = s->ops.v[i];
        if (o->feat <= 0 || op_ground(s, o) || op_top(o) <= SC.H + 0.3) continue;
        for (k = 0; k < nf && fe[k] != o->feat; k++) {}
        if (k < nf) continue;
        if (nf == cap) { cap = cap ? 2 * cap : 16; fe = (int *)realloc(fe, sizeof(int) * (size_t)cap); }
        fe[nf++] = o->feat;
    }
    for (i = 1; i < nf; i++) { int x = fe[i], j = i; while (j > 0 && fe[j - 1] > x) { fe[j] = fe[j - 1]; j--; } fe[j] = x; }
    for (i = maxd; i < nf; i++) { if (nb == cb) { cb = cb ? 2 * cb : 16; bad = (int *)realloc(bad, sizeof(int) * (size_t)cb); } bad[nb++] = fe[i]; n++; }
    { const char *lp = getenv("BBB_PAD_LOG"); FILE *f = lp && *lp ? fopen(lp, "a") : NULL; if (f) { fprintf(f, "base %.1f mm: small-base cap %d details of %d kept, %d go\n", sc->mm, nf < maxd ? nf : maxd, nf, n); fclose(f); } }
    if (nb) drop_keys(sc, &bad, &nb, &cb);
    free(bad); free(fe);
}
static void feet_pad(Sc *sc, Geo *space) {
    Scene *s = sc->s; int i, j, k, nb = 0, cap = 0, *bad = NULL;
    for (i = 0; i < s->ops.n; i++) {
        Op *o = s->ops.v[i]; int in = 0, key;
        if (op_ground(s, o) && !(o->kind && in_soft(o->kind))) continue;          /* the ground stays; loose grit on it does not */
        if (op_top(o) <= SC.H + 1.2) {          /* a low piece mostly outside the pad (a rail, a plank, a floor passing under the feet) stays */
            double ar = 0, ai = 0; for (j = 0; j < o->polys.n; j++) { ar += area(o->polys.v[j]); if (g_intersects(o->polys.v[j], space)) ai += area(LIn(o->polys.v[j], space)); }
            if (ar > 0 && ai < 0.4 * ar) continue;
        }
        for (j = 0; j < o->polys.n && !in; j++) if (g_intersects(o->polys.v[j], space) && area(LIn(o->polys.v[j], space)) > 0.05) in = 1;
        if (!in) continue;
        key = o->feat > 0 ? o->feat : -(i + 1);
        for (k = 0; k < nb && bad[k] != key; k++) {}
        if (k < nb) continue;
        if (nb == cap) { cap = cap ? 2 * cap : 16; bad = (int *)realloc(bad, sizeof(int) * (size_t)cap); }
        bad[nb++] = key;
    }
    drop_keys(sc, &bad, &nb, &cap);
    free(bad);
    clear_cuts(sc, space);
    for (i = 0; i < s->tcuts.n; i++) {          /* the cuts on top of plates (cracks, seams): stopped at the pad's edge too */
        GeoList *pl = &s->tcuts.v[i].polys, np = { 0 }; int hit = 0;
        for (j = 0; j < pl->n; j++) if (intersects(pl->v[j], space)) { hit = 1; break; }
        if (!hit) continue;
        for (j = 0; j < pl->n; j++) {
            if (!intersects(pl->v[j], space)) { gl_push(&np, pl->v[j]); continue; }
            { GeoList *ps = g_polys(g_diff(pl->v[j], space)); for (k = 0; k < ps->n; k++) if (area(ps->v[k]) > 0.6 && g_wide(ps->v[k], SC.MINW)) gl_push(&np, ps->v[k]); }
        }
        gl_free(pl); *pl = np;
    }
}
/* true size: where a track runs, the ground is cleared for it, as at a level crossing: raised paving cut back to the track's
   edge, the layout's loose ground pieces (kerb bricks, stones) taken away, the cut marks (grooves, gutters) stopped there.
   The subject and the details are not touched; old, nold: the ops there were before the track was laid (the track's own are the rest) */
static void track_clear(Sc *sc, Op **old, int nold) {
    Scene *s = sc->s; Geo *strip; int i, j, k, w; OpList keep = { 0 };
    if (!sc->pathg || E(sc->pathg)) return;
    strip = g_buf(sc->pathg, 0.8);
    for (i = 0; i < s->ops.n; i++) {
        Op *o = s->ops.v[i]; int hit = 0;
        int was = 0, q; for (q = 0; q < nold && !was; q++) was = old[q] == o;
        if (!was || o->feat > 0 || !o->kind) { ol_push_own(&keep, o); continue; }
        for (j = 0; j < o->polys.n && !hit; j++) if (intersects(o->polys.v[j], strip) && area(LIn(o->polys.v[j], strip)) > 0.2) hit = 1;
        if (!hit || op_top(o) <= SC.H + 0.05) { ol_push_own(&keep, o); continue; }          /* the floor at ground level stays */
        if (in_bridge(o->kind) || in_terrk(o->kind)) {          /* a raised plate or slab: cut back to the track's edge */
            GeoList ps = { 0 }; Op *n;
            for (j = 0; j < o->polys.n; j++) { GeoList *xs = g_polys(g_diff(o->polys.v[j], strip)); for (k = 0; k < xs->n; k++) if (area(xs->v[k]) > 0.6 && g_wide(xs->v[k], SC.MINW)) gl_push(&ps, xs->v[k]); }
            if (ps.n) { n = op_like(o, &ps); ol_push_own(&keep, n); }
            gl_free(&ps); op_unref(o); continue;
        }
        chops_of(sc, o, 0, 0, 1); op_unref(o);          /* a loose ground piece on the track: gone */
    }
    free(s->ops.v); s->ops = keep;
    clear_cuts(sc, strip);
    (void)w;
}
static void drop_tall_in(Sc *sc, Geo *space, int box, double hx, double hy, double rad) {
    Scene *s = sc->s; int i, j, k, nb = 0, cap = 0, *bad = NULL; OpList keep = { 0 }; Geo *pb = sc->pathg && !E(sc->pathg) ? g_buf(sc->pathg, 0.6) : NULL;
    for (i = 0; i < s->ops.n; i++) {
        Op *o = s->ops.v[i]; double h; int in = 0;
        if (o->feat <= 0) continue;
        h = stand_h(sc, o, pb); if (h <= 2.0) continue;
        for (k = 0; k < nb; k++) if (bad[k] == o->feat) break;
        if (k < nb) continue;
        for (j = 0; j < o->polys.n && !in; j++) if (g_intersects(o->polys.v[j], space) && area(LIn(o->polys.v[j], space)) > 0.3) in = 1;
        if (!in) continue;
        if (move_out(sc, space, o->feat, box, hx, hy, rad)) continue;          /* moved to the side: a framing piece */
        if (nb == cap) { cap = cap ? 2 * cap : 16; bad = (int *)realloc(bad, sizeof(int) * (size_t)cap); }
        bad[nb++] = o->feat;
    }
    for (i = 0; i < s->ops.n; i++) {          /* the layout's own standing pieces (untagged, not ground: no plate, bedrock or grit) in the space go too */
        Op *o = s->ops.v[i]; int in = 0;
        int gb = 0, z; for (z = 0; z < s->base_ops.n && !gb; z++) gb = s->base_ops.v[z] == o;          /* the ground laid before the details stays */
        (void)gb;          /* the layout's own pieces too (a well's pillars): only the ground kinds stay */
        if (getenv("BBB_DBG_GROW") && op_top(o) > SC.H + 2.0) { int in2 = 0, jj; for (jj = 0; jj < o->polys.n; jj++) if (g_intersects(o->polys.v[jj], space)) in2 = 1; if (in2) { FILE *df = fopen(getenv("BBB_DBG_GROW"), "a"); if (df) { fprintf(df, "tall in space: kind %s feat %d base %d bridge %d soft %d terrk %d top %.2f\n", o->kind ? o->kind->s : "?", o->feat, gb, o->kind && in_bridge(o->kind), o->kind && in_soft(o->kind), o->kind && in_terrk(o->kind), op_top(o)); fclose(df); } } }
        if (o->feat > 0 || stand_h(sc, o, pb) <= 2.0 || !o->kind || in_bridge(o->kind) || in_soft(o->kind) || in_terrk(o->kind) || kd_has(KD.wetk, o->kind->s)) continue;
        for (j = 0; j < o->polys.n && !in; j++) if (g_intersects(o->polys.v[j], space) && area(LIn(o->polys.v[j], space)) > 0.3) in = 1;
        if (in) { if (nb == cap) { cap = cap ? 2 * cap : 16; bad = (int *)realloc(bad, sizeof(int) * (size_t)cap); } bad[nb++] = -(i + 1); }          /* -(index): this op alone */
    }
    if (!nb) return;
    for (i = 0; i < s->ops.n; i++) {
        Op *o = s->ops.v[i]; for (k = 0; k < nb && !(bad[k] > 0 ? o->feat == bad[k] : bad[k] == -(i + 1)); k++) {}
        if (k < nb) { chops_of(sc, o, 0, 0, 1); op_unref(o); } else ol_push_own(&keep, o);
    }
    free(s->ops.v); s->ops = keep; free(bad);
}
static int place_(Sc *sc, Geo *foot, const SoLayers *layers0, double height) {          /* _place(sc, foot, layers, height) */
    SoLayers lay = { 0 }; Scene *s = sc->s; Geo *solid; Num z; int r; double boost = sc->mm >= 59.9 ? CPC.big_base_height_factor : 1.0;
    taller(layers0, height, boost, &lay);
    solid = LIn(solid_of(&lay, 0.0), sc->wide);
    if (terr(s)) R_(sm_clear_terrain(s, LIn(g_buf(g_uu2(foot, solid), 0.8), sc->wide)));          /* under every part of it (a skeleton's bones reach past its footprint) */
    R_(sm_clear_small(s, g_buf(solid, 0.4))); R_(pl_evict(s, solid));
    uncover(sc, LIn(g_uu2(foot, solid), sc->disc));          /* the footprint and every part that reaches past it */          /* partly on the floor plates: the plates are cut back round it, so it stands on one level */
    z = terr(s) ? num_f(H_) : surf(s, LIn(foot, sc->disc));
    PL.hboost = boost;
    { int sv = sc_feat_push(sc->subj ? "feature" : "subject"); r = ft_build(s, z, &lay, S3D); sc_feat_pop(sv); }
    PL.hboost = 1.0;
    lyl_free(&lay);
    return R_(r);
}

static int fits(Sc *sc, Geo *foot, Geo *solid, int edge, int force) {          /* _fits(sc, foot, solid, edge, force) */
    Scene *s = sc->s; Geo *inside = LIn(foot, sc->disc), *test; int i;
    if (edge) { if (area(inside) < 0.45 * area(foot)) return 0; }
    else if (area(inside) < 0.995 * area(foot)) return 0;
    test = g_buf(solid, 0.5);
    if (!force) for (i = 0; i < sc->bigs.n; i++) if (intersects(sc->bigs.v[i], test)) return 0;
    if (!force && sc->lline && intersects(sc->lline, g_buf(foot, -1.5))) return 0;
    if (s->has_objs) for (i = 0; i < s->keepout.n; i++) if (intersects(s->keepout.v[i], test)) return 0;
    if (sc->wet && (area(LIn(sc->wet, solid)) > 0.15 * area(solid) || area(LIn(sc->wet, inside)) > 0.2 * area(inside))) return 0;
    for (i = 0; i < sc->taken.n; i++) if (intersects(sc->taken.v[i], foot)) return 0;
    if (!force) {          /* one level: on the floor plates for (nearly) all of it, or off them (a tank across the plate edge and the dust reads wrong) */
        SmPlate *pl = NULL; int npl = 0; double cov = 0.0, ia = area(inside);
        R_(sm_plates(s, &pl, &npl));
        const GEOSGeometry **pg = (const GEOSGeometry **)malloc(sizeof *pg * (size_t)(npl ? npl : 1)); int np2 = 0, bad;
        for (i = 0; i < npl; i++) { Geo *x = geo_new(shp_intersection(pl[i].g->g, inside->g)); if (x) { double xa = area(x); cov += xa; if (xa > 1e-3) pg[np2++] = pl[i].g->g; geo_unref(x); } }
        bad = ia > 0 && cov > 0.05 * ia && cov < PL.cover_big * ia && sc_uncovered(inside->g, pg, np2) > (1.0 - PL.cover_big) * ia;          /* a joint between tiles is fine */
        free(pg); sm_plates_free(pl, npl);
        (void)bad;          /* not refused any more: place_ cuts the plates back round it (uncover), so it still stands on one level; refusing made crowded plate floors force the subject in, which carved the floor's own pipes away */
    }
    return 1;
}

/* (op[0], ps) + op[2:]: a new op with other pieces */
static Op *op_with(const Op *o, const GeoList *ps) {
    return op_like(o, ps);
}
/* swap: id(p) -> [x ...]: the C keeps (p, list) pairs */
typedef struct { const Geo *p; GeoList *xs; } Swap;
typedef struct { Swap *v; int n, cap; } SwapMap;
static GeoList *swap_get(const SwapMap *m, const Geo *p) { int i; for (i = m->n - 1; i >= 0; i--) if (m->v[i].p == p) return m->v[i].xs; return NULL; }
static void swap_put(SwapMap *m, const Geo *p, GeoList *xs) {
    int i; for (i = 0; i < m->n; i++) if (m->v[i].p == p) { m->v[i].xs = xs; return; }
    if (m->n == m->cap) { m->cap = m->cap ? 2 * m->cap : 64; m->v = (Swap *)realloc(m->v, sizeof(Swap) * (size_t)m->cap); }
    m->v[m->n].p = p; m->v[m->n].xs = xs; m->n++;
}
/* s.ops = [o for o in ((op[0], [x for p in op[1] for x in swap.get(id(p), [p])]) + tuple(op[2:]) for op in s.ops) if o[1]];
   s.feet = [...]; s.objs = [...] when the scene has them */
static void apply_swap(Scene *s, const SwapMap *sw) {
    OpList ops = { 0 }; GeoList feet = { 0 }, objs = { 0 }; int i, j, k;
    for (i = 0; i < s->ops.n; i++) {
        Op *o = s->ops.v[i]; GeoList ps = { 0 };
        for (j = 0; j < o->polys.n; j++) { GeoList *x = swap_get(sw, o->polys.v[j]); if (x) for (k = 0; k < x->n; k++) gl_push(&ps, x->v[k]); else gl_push(&ps, o->polys.v[j]); }
        if (ps.n) ol_push_own(&ops, op_with(o, &ps));
        gl_free(&ps);
    }
    ol_free(&s->ops); s->ops = ops;
    for (j = 0; j < s->feet.n; j++) { GeoList *x = swap_get(sw, s->feet.v[j]); if (x) for (k = 0; k < x->n; k++) gl_push(&feet, x->v[k]); else gl_push(&feet, s->feet.v[j]); }
    gl_free(&s->feet); s->feet = feet;
    if (s->has_objs) {
        for (j = 0; j < s->objs.n; j++) { GeoList *x = swap_get(sw, s->objs.v[j]); if (x) for (k = 0; k < x->n; k++) gl_push(&objs, x->v[k]); else gl_push(&objs, s->objs.v[j]); }
        gl_free(&s->objs); s->objs = objs;
    }
}
static void gcuts_replace(Scene *s, GcutList *nw) { int i; for (i = 0; i < s->gcuts.n; i++) gl_free(&s->gcuts.v[i].polys); free(s->gcuts.v); s->gcuts = *nw; }
static void tcuts_replace(Scene *s, TcutList *nw) { int i; for (i = 0; i < s->tcuts.n; i++) gl_free(&s->tcuts.v[i].polys); free(s->tcuts.v); s->tcuts = *nw; }

/* a big feature only partly on the floor plates (or raised ground slabs): the plates are cut back round it (0.3 mm
   clear), slivers dropped, the cuts in their tops trimmed to what is left; it then stands on the ground, one level */
static void uncover(Sc *sc, Geo *foot) {
    Scene *s = sc->s; Geo *gb; SwapMap sw = { 0 }; int i, j, k; double fa = area(foot), cov = 0.0;
    SmPlate *pl = NULL; int npl = 0;
    if (PL.cover_big <= 0 || fa <= 0) return;
    (void)pl; (void)npl;
    {   GeoList pg0 = { 0 }; const GEOSGeometry **pg; int n2 = 0, part;          /* the raised plates and ground slabs (dunes, bedrock) under it */
        for (i = 0; i < s->ops.n; i++) { Op *op = s->ops.v[i]; if (!(in_bridge(op->kind) || in_terrk(op->kind)) || (op->type == OP_PRISM ? op->n[1].v : op->n[0].v + op->n[1].v) <= H_ + 0.05) continue;          /* prisms and tapered slabs (dunes) */
            for (j = 0; j < op->polys.n; j++) if (intersects(op->polys.v[j], foot)) gl_push(&pg0, op->polys.v[j]); }
        pg = (const GEOSGeometry **)malloc(sizeof *pg * (size_t)(pg0.n ? pg0.n : 1));
        for (i = 0; i < pg0.n; i++) { Geo *x = geo_new(shp_intersection(pg0.v[i]->g, foot->g)); if (x) { double xa = area(x); cov += xa; if (xa > 1e-3) pg[n2++] = pg0.v[i]->g; geo_unref(x); } }
        if (n2) { GEOSGeometry *uu = shp_union_all(pg, n2), *xx = uu ? shp_intersection(uu, foot->g) : NULL; cov = xx ? shp_area(xx) : 0.0; shp_free(uu); shp_free(xx); }          /* stacked tiers (a dune) count once */
        part = cov > 0.05 * fa && cov < PL.cover_big * fa && sc_uncovered(foot->g, pg, n2) > (1.0 - PL.cover_big) * fa;
        free(pg); gl_free(&pg0);
        if (!part) return;
    }
    gb = g_buf(foot, 0.3);
    for (i = 0; i < s->ops.n; i++) {
        Op *op = s->ops.v[i];
        if (!(in_bridge(op->kind) || in_terrk(op->kind))) continue;
        for (j = 0; j < op->polys.n; j++) {
            Geo *p = op->polys.v[j]; GeoList *xs, *ps;
            if (!intersects(p, gb)) continue;
            xs = LL(); ps = g_polys(g_diff(p, gb));
            for (k = 0; k < ps->n; k++) if (area(ps->v[k]) > 0.6 && g_wide(ps->v[k], SC.MINW)) gl_push(xs, ps->v[k]);
            swap_put(&sw, p, xs);
        }
    }
    if (!sw.n) return;
    apply_swap(s, &sw);
    {   TcutList tc = { 0 };
        for (i = 0; i < s->tcuts.n; i++) {
            Tcut *t = &s->tcuts.v[i]; GeoList ps2 = { 0 };
            for (j = 0; j < t->polys.n; j++) {
                Geo *p = t->polys.v[j]; GeoList *qs;
                if (t->z1.v > H_ + 0.05 && intersects(p, gb)) qs = g_polys(g_diff(p, gb)); else qs = L1(p);
                for (k = 0; k < qs->n; k++) if (!E(g_buf(qs->v[k], -0.1))) gl_push(&ps2, qs->v[k]);
            }
            if (ps2.n) tcl_push(&tc, &ps2, t->z0, t->z1, t->kind);
            gl_free(&ps2);
        }
        tcuts_replace(s, &tc);
    }
}

static void carve(Scene *s, Geo *g) {          /* _carve(s, g) */
    Geo *gb = g_buf(g, 0.5); SmIdSet cells; GeoList *dead = LL(); SwapMap sw = { 0 }; int i, j;
    ids_of_cells(s, &cells);
    for (i = 0; i < s->ops.n; i++) {
        Op *op = s->ops.v[i];
        if (in_soft(op->kind) || in_bridge(op->kind) || in_terrk(op->kind) || op->n[0].v > H_ + 0.05) continue;
        for (j = 0; j < op->polys.n; j++) {
            Geo *p = op->polys.v[j];
            if (area(p) < 10 || sm_ids_has(&cells, p) || !intersects(p, gb)) continue;
            if (op->type == OP_PRISM) {
                GeoList *xs = LL(), *ps = g_polys(g_diff(p, gb)); int k;
                for (k = 0; k < ps->n; k++) if (area(ps->v[k]) > 2.0 && g_wide(ps->v[k], 1.2)) gl_push(xs, ps->v[k]);
                swap_put(&sw, p, xs);
            } else gl_push(dead, p);
        }
    }
    if (sw.n) {
        TcutList tc = { 0 };
        apply_swap(s, &sw);
        for (i = 0; i < s->ops.n; i++) {
            Op *op = s->ops.v[i];
            if (!(op->n[0].v > H_ + 0.05) || in_bridge(op->kind) || in_terrk(op->kind)) continue;
            for (j = 0; j < op->polys.n; j++) { Geo *rp = g_reppt(op->polys.v[j]); if (g_contains(gb, rp)) gl_push(dead, op->polys.v[j]); }
        }
        for (i = 0; i < s->tcuts.n; i++) {
            Tcut *t = &s->tcuts.v[i]; GeoList ps2 = { 0 };
            for (j = 0; j < t->polys.n; j++) {
                Geo *p = t->polys.v[j]; GeoList *qs; int k;
                if (t->z1.v > H_ + 0.35 && intersects(p, gb)) qs = g_polys(g_diff(p, gb)); else qs = L1(p);
                for (k = 0; k < qs->n; k++) if (!E(g_buf(qs->v[k], -0.1))) gl_push(&ps2, qs->v[k]);
            }
            if (ps2.n) tcl_push(&tc, &ps2, t->z0, t->z1, t->kind);
            gl_free(&ps2);
        }
        tcuts_replace(s, &tc);
    }
    free(sw.v);
    R_(sm_remove(s, dead));
    sm_ids_free(&cells);
}

/* _ray_exit(c, d, g, far=400.0) */
static void ray_exit(const double *c, const double *d, Geo *g, double *out) {
    double xy[4] = { c[0], c[1], c[0] + d[0] * 400.0, c[1] + d[1] * 400.0 }; Geo *x = LIn(g_line(xy, 2), g); int t, i, k, n, best = -1; double bk = 0, bx = 0, by = 0;
    if (E(x)) { out[0] = c[0]; out[1] = c[1]; return; }
    t = g_type(x);
    n = (t == GEOS_MULTIPOINT || t == GEOS_MULTILINESTRING || t == GEOS_MULTIPOLYGON || t == GEOS_GEOMETRYCOLLECTION) ? shp_num_geoms(x->g) : 1;
    for (i = 0; i < n; i++) {
        const GEOSGeometry *part = n == 1 && !(t == GEOS_MULTIPOINT || t == GEOS_MULTILINESTRING || t == GEOS_MULTIPOLYGON || t == GEOS_GEOMETRYCOLLECTION) ? x->g : shp_geom_n(x->g, i);
        int pt = shp_type(part), m; double *xy2;
        if (pt != GEOS_POINT && pt != GEOS_LINESTRING && pt != GEOS_LINEARRING) cp_raise(SC_ERR);          /* .coords of a polygon / a multi part */
        m = shp_coords(part, NULL, 0); xy2 = (double *)LM(sizeof(double) * 2 * (size_t)(m > 0 ? m : 1) + 64);
        { int cap = 1 << 10; for (;;) { xy2 = (double *)LM(sizeof(double) * 2 * (size_t)cap); m = shp_coords(part, xy2, cap); if (m >= 0) break; cap *= 4; if (cap > (1 << 24)) cp_raise(SC_ERR); } }
        for (k = 0; k < m; k++) {
            double key = (xy2[2 * k] - c[0]) * d[0] + (xy2[2 * k + 1] - c[1]) * d[1];
            if (best < 0 || key > bk) { best = 1; bk = key; bx = xy2[2 * k]; by = xy2[2 * k + 1]; }
        }
    }
    if (best < 0) cp_raise(SC_ERR);          /* max() of nothing */
    out[0] = bx; out[1] = by;
}
static void rim_hit(const double *q, const double *d, double R, double *out) {          /* _rim_hit(q, d, R) */
    double b = q[0] * d[0] + q[1] * d[1], cc = ppow(q[0], 2) + ppow(q[1], 2) - R * R, t = -b + sqrt(pmax(0.0, b * b - cc));
    out[0] = q[0] + d[0] * t; out[1] = q[1] + d[1] * t;
}
static void unit_(double dx, double dy, double *out) { double L = py_hypot(dx, dy); if (L == 0) L = 1.0; out[0] = dx / L; out[1] = dy / L; }
static void rot_(const double *v, double a, double *out) { out[0] = py_cos(a) * v[0] - py_sin(a) * v[1]; out[1] = py_sin(a) * v[0] + py_cos(a) * v[1]; }
static Geo *curve_(const double *p0, const double *ctrl, const double *p1) {          /* _curve(p0, ctrl, p1) */
    double L = px_dist(p0, ctrl, 2) + px_dist(ctrl, p1, 2), *xy; int64_t n; int i;
    if (px_int(L / 0.5, &n) != 0) cp_raise(SC_ERR);
    if (n < 8) n = 8;
    xy = (double *)LM(sizeof(double) * 2 * (size_t)(n + 1));
    for (i = 0; i <= n; i++) {
        double t = (double)i / (double)n, a = ppow(1 - t, 2), b2 = 2 * t * (1 - t), c2 = t * t;
        xy[2 * i] = a * p0[0] + b2 * ctrl[0] + c2 * p1[0];
        xy[2 * i + 1] = a * p0[1] + b2 * ctrl[1] + c2 * p1[1];
    }
    return g_line(xy, (int)n + 1);
}

/* ================================================================ calling builders and details (guarded: a builder
   that raises through ly_jb lands here, as an exception out of the call) */
typedef struct { FtBuildFn fn; void *ctx; double cx, cy, a; PyRandom *rng; int hs; double k; Geo **foot; SoLayers *lay; int r; } BCall;
static void bcall_(void *p) { BCall *b = (BCall *)p; b->r = b->fn(b->ctx, b->cx, b->cy, b->a, b->rng, b->hs, b->k, b->foot, b->lay); }
static int call_build(FtBuildFn fn, void *ctx, double cx, double cy, double a, PyRandom *rng, double k, Geo **foot, SoLayers *lay) {
    BCall b; int t;
    b.fn = fn; b.ctx = ctx; b.cx = cx; b.cy = cy; b.a = a; b.rng = rng; b.hs = 1; b.k = k; b.foot = foot; b.lay = lay; b.r = SC_ERR;
    *foot = NULL;
    t = cp_try(bcall_, &b);
    return t < 0 ? SC_ERR : b.r;
}
typedef struct { ScFn *f; Scene *s; PyRandom *rng; int lvl; int r; } FCall;
const char *cp_fn_label(const ScFn *f) {          /* the audit's name for a detail: its shape or template when its qualname is a wrapper */
    static char buf[64][96]; static int k; const char *q = f->qualname ? f->qualname : "detail", *t = f->tag.shape ? f->tag.shape : f->tag.tmpl ? f->tag.tmpl : f->tag.cls; char *b;
    if (!t) return q;
    b = buf[k++ & 63]; snprintf(b, 96, "%s:%s", t, f->tag.kind ? f->tag.kind : ""); return so_intern(b);
}
static void fcall_(void *p) { FCall *c = (FCall *)p; int sv = sc_feat_push(cp_fn_label(c->f)); c->r = c->f->run(c->f, c->s, c->rng, c->lvl); sc_feat_pop(sv); }
static int call_fn(ScFn *f, Scene *s, PyRandom *rng, int lvl) {          /* fn(s, rng, lvl): 0, SC_ERR = raised */
    FCall c; int t;
    c.f = f; c.s = s; c.rng = rng; c.lvl = lvl; c.r = SC_ERR;
    sm_hook_err = 0;
    t = cp_try(fcall_, &c);
    if (t < 0 || c.r < 0 || sm_hook_err) { sm_hook_err = 0; return SC_ERR; }
    return 0;
}
int cp_call_fn(ScFn *f, Scene *s, PyRandom *rng, int lvl) { return call_fn(f, s, rng, lvl); }
/* SUBJECTS[name](pal) */
static void subject_builder(const char *name, const ScPal *pal, FtBuildFn *fn, void **ctx) {
    if (!kd_subject(name)) cp_raise(SC_ERR);          /* KeyError */
    if (!CPH.subject) cp_raise(SC_ERR);
    RX(CPH.subject(name, pal, fn, ctx));
}

/* ================================================================ the subject */
static int place_subject(Sc *sc, KVal *plan, int small, int force) {          /* place_subject(sc, plan, small, force) */
    KVal *sub = KG(plan, "subject"), *pth = KG(plan, "path"); double R = sc->R, k0, seed, kks[3]; int edge, past, ki, oi, i, noffs;
    FtBuildFn build = NULL; void *bctx = NULL; KVal *offs[8]; KVal off_lit[4]; static const double OL[4] = { 0.3, 0.15, 0.42, 0.0 };
    const char *name = KS_(sub, "name");
    subject_builder(name, &sc->pal, &build, &bctx);
    k0 = (small && !ft_figure() ? 1.0 : ft_figure() ? 1.6 : ft_size_factor(sc->mm) * big_of(sc->mm)) * KN(KG(sub, "size"));          /* true size: 1.6 as the old --scale had it, then grown (lay_scene) */
    seed = py_random(sc->mixr); edge = truthy(KG(sub, "edge"));
    past = strcmp(KS_(pth, "kind"), "none") != 0 && !strcmp(KS_(pth, "route"), "past");
    offs[0] = KG(sub, "offset"); noffs = 1;
    for (i = 0; i < 4; i++) { if (fabs(OL[i] - KN(offs[0])) > 0.06) { memset(&off_lit[i], 0, sizeof off_lit[i]); off_lit[i].t = KV_FLOAT; off_lit[i].f = OL[i]; offs[noffs++] = &off_lit[i]; } }
    kks[0] = k0; kks[1] = 0.8 * k0; kks[2] = 0.62 * k0;
    for (ki = 0; ki < 3; ki++) {
        double kk = kks[ki]; int nof = edge ? 1 : noffs;
        for (oi = 0; oi < nof; oi++) {
            double off = edge ? 0.0 : KN(offs[oi]);
            for (i = 0; i < 15; i++) {
                int mark = ly_mark(); int64_t da = (int64_t)((i + 1) / 2) * 25 * (i % 2 ? 1 : -1);
                double th = LF_RADIANS(KN(KG(sub, "angle")) + (double)da), r = edge ? (R - 2.4) : R * off, c[2], fa, p0[2], front[2];
                Geo *foot = NULL, *solid; SoLayers lay = { 0 }; PyRandom br; int g;
                c[0] = r * py_cos(th); c[1] = r * py_sin(th);
                fa = LF_RADIANS(KN(KG(pth, "from_angle")) + (double)da); p0[0] = (R + 2) * py_cos(fa); p0[1] = (R + 2) * py_sin(fa);
                if (edge) { front[0] = -py_cos(th); front[1] = -py_sin(th); }
                else if (past) { if (off > 0.1) unit_(-c[0], -c[1], front); else { front[0] = py_cos(fa); front[1] = py_sin(fa); } }
                else unit_(p0[0] - c[0], p0[1] - c[1], front);
                py_seed_float(&br, seed);
                g = call_build(build, bctx, c[0], c[1], py_atan2(front[1], front[0]), &br, kk, &foot, &lay);
                if (g == SC_ERR) { char w[300]; snprintf(w, sizeof w, "subject %s", name); cp_log(sc, w); lyl_free(&lay); geo_unref(foot); return 0; }
                if (g == 1 || !foot || E(foot)) { lyl_free(&lay); geo_unref(foot); ly_pop(mark); continue; }
                LAG(foot);
                solid = LIn(solid_of(&lay, 0.0), sc->wide);
                if (small && area(foot) > 0.30 * PI * R * R) { lyl_free(&lay); return 0; }
                if (!fits(sc, foot, solid, edge, force)) { lyl_free(&lay); ly_pop(mark); continue; }
                if (force) carve(sc->s, solid);
                { KVal *h = kv_get(sub, "height"); if (!place_(sc, foot, &lay, h ? KN(h) : 1.0)) { lyl_free(&lay); return 0; } }
                set_geo(&sc->subj, LIn(foot, sc->wide)); set_geo(&sc->core, LIn(solid_of(&lay, 6.0), sc->wide)); set_geo(&sc->solid, solid);
                sc->c[0] = c[0]; sc->c[1] = c[1]; sc->front[0] = front[0]; sc->front[1] = front[1]; sc->has_p0 = 1; sc->p0[0] = p0[0]; sc->p0[1] = p0[1];
                ray_exit(c, front, sc->subj, sc->p1); sc->has_q = 1; sc->q[0] = sc->p1[0]; sc->q[1] = sc->p1[1];
                gl_push(&sc->taken, g_buf(sc->subj, 0.6));
                {   /* sub["angle"] = round((sub["angle"] + da) % 360, 1) ... */
                    KVal *an = KG(sub, "angle"), *fr = KG(pth, "from_angle"), *nv;
                    if (an->t == KV_INT || an->t == KV_BOOL) { int64_t m; px_mod_i(an->i + da, 360, &m); nv = kv_int(m); } else nv = kv_float(pround(pmod(an->f + (double)da, 360), 1));
                    kv_set(sub, "angle", nv);
                    if (fr->t == KV_INT || fr->t == KV_BOOL) { int64_t m; px_mod_i(fr->i + da, 360, &m); nv = kv_int(m); } else nv = kv_float(pround(pmod(fr->f + (double)da, 360), 1));
                    kv_set(pth, "from_angle", nv);
                    kv_set(sub, "size", kv_float(pround(KN(KG(sub, "size")) * kk / k0, 2)));
                    if (!edge) kv_set(sub, "offset", kv_copy(offs[oi]));
                }
                lyl_free(&lay);
                return 1;
            }
        }
    }
    return 0;
}

/* ================================================================ paths */
typedef struct { Geo *g; double h; const char *k; } UpL;
static int wide_d(Geo *q) { return g_wide(q, SC.MINW); }
static Geo *lay_up(Sc *sc, const UpL *in, int nin, int loose) {          /* lay_up(sc, layers, loose) */
    Scene *s = sc->s; UpL L[8]; int n = 0, i, j, k; GeoList *l = LL(), *bigs = LL(); Geo *foot, *ob; SmPlate *pl = NULL; int npl = 0;
    for (i = 0; i < nin; i++) {
        if (!in[i].g || E(in[i].g)) continue;
        L[n].g = LIn(in[i].g, sc->wide); L[n].h = in[i].h;
        L[n].k = loose ? in[i].k : (in[i].k ? sm_kfix(kind_str(in[i].k))->s : NULL);
        n++;
    }
    for (i = 0; i < n; i++) gl_push(l, L[i].g);
    foot = U(l);
    if (E(foot)) return foot;
    R_(sm_bigs(s, bigs)); l = LL();
    for (i = 0; i < bigs->n; i++) if (intersects(bigs->v[i], foot)) gl_push(l, bigs->v[i]);
    if (sc->core) gl_push(l, sc->core);
    ob = U(l);
    if (!E(ob)) foot = g_diff(foot, g_buf(ob, 0.35));
    if (E(foot)) return foot;
    if (terr(s)) R_(sm_clear_terrain(s, g_buf(foot, 0.5)));
    R_(sm_clear_small(s, g_buf(foot, 0.15))); R_(pl_evict(s, foot));
    if (!terr(s)) R_(sm_plates(s, &pl, &npl));
    for (i = 0; i < n; i++) {
        Geo *g = LIn(L[i].g, foot), *rest; Kind *kind = L[i].k ? kind_str(L[i].k) : NULL; GeoList *ps = LL();
        if (terr(s)) {
            GeoList *qs = g_polys(g), *flat = LL(), *rs = LL();
            for (j = 0; j < qs->n; j++) { Num lo, hi; R_(T_zr(s, qs->v[j], &lo, &hi)); if (lo.v == H_ && hi.v == H_) gl_push(flat, qs->v[j]); }
            for (j = 0; j < qs->n; j++) { Num lo, hi; R_(T_zr(s, qs->v[j], &lo, &hi)); if (!(lo.v == H_ && hi.v == H_)) gl_push(rs, qs->v[j]); }
            for (j = 0; j < rs->n; j++) { GeoList *st = LL(); R_(sm_strips_box(rs->v[j], 3.0, st)); for (k = 0; k < st->n; k++) gl_push(flat, st->v[k]); }
            for (j = 0; j < flat->n; j++) if (wide_d(flat->v[j]) && area(flat->v[j]) > 0.5) gl_push(ps, flat->v[j]);
            if (ps->n) { TPiece *tp = NULL; int nt = 0, q; R_(tprism(s, ps, num_f(L[i].h), kind, &tp, &nt)); for (q = 0; q < nt; q++) geo_unref(tp[q].g); free(tp); }
            continue;
        }
        rest = g;
        for (j = 0; j < npl; j++) {
            GeoList *qs = g_polys(LIn(rest, pl[j].g)), *part = LL(); Num top;
            for (k = 0; k < qs->n; k++) if (wide_d(qs->v[k]) && area(qs->v[k]) > 0.5) gl_push(part, qs->v[k]);
            if (part->n) R_(pl_up(s, part, num_z(pl[j].z.v, 1), num_f(L[i].h), kind, &top));
            rest = g_diff(rest, pl[j].g);
        }
        { GeoList *qs = g_polys(rest); Num top;
          for (k = 0; k < qs->n; k++) if (wide_d(qs->v[k]) && area(qs->v[k]) > 0.5) gl_push(ps, qs->v[k]);
          if (ps->n) R_(pl_up(s, ps, num_f(H_), num_f(L[i].h), kind, &top)); }
    }
    sm_plates_free(pl, npl);
    return foot;
}
static Geo *lay_cut(Sc *sc, Geo *g, double depth, const char *kind, int meet) {          /* lay_cut(sc, g, depth, kind, meet) */
    Scene *s = sc->s; GeoList *bigs = LL(), *l = LL(); Geo *ob, *rest; int i; SmPlate *pl = NULL; int npl = 0; Kind *kd = kind ? kind_str(kind) : NULL;
    g = LIn(g, sc->wide);
    R_(sm_bigs(s, bigs));
    for (i = 0; i < bigs->n; i++) if (intersects(bigs->v[i], g)) gl_push(l, bigs->v[i]);
    if (sc->core && !meet) gl_push(l, sc->core);
    ob = U(l);
    if (!E(ob)) g = g_diff(g, g_buf(ob, 0.35));
    if (E(g)) return g;
    if (terr(s)) R_(sm_clear_terrain(s, g_buf(g, 0.4)));
    R_(sm_clear_small(s, g_buf(g, 0.15))); R_(pl_evict(s, g));
    if (terr(s)) {
        GeoList *tl = LL(); Geo *tu, *flat;
        for (i = 0; i < s->terr.n; i++) if (intersects(s->terr.v[i].fp, g)) gl_push(tl, s->terr.v[i].fp);
        tu = U(tl);
        flat = !E(tu) ? g_diff(g, tu) : g;
        if (!E(flat)) R_(sc_gcut(s, g_polys(flat), num_f(depth), kd));
        if (!E(tu)) { GeoList *st = LL(); R_(sm_strips_box(LIn(g, tu), 3.0, st)); R_(tcut_flat(s, st, num_f(depth), kd)); }
        return g;
    }
    rest = g;
    R_(sm_plates(s, &pl, &npl));
    for (i = 0; i < npl; i++) {
        GeoList *part = g_polys(LIn(rest, pl[i].g));
        if (part->n) { int r = sc_tcut(s, part, num_sub(pl[i].z, num_f(depth)), num_add(pl[i].z, num_f(0.3)), kd); if (r < 0) { sm_plates_free(pl, npl); cp_raise(SC_ERR); } }
        rest = g_diff(rest, pl[i].g);
    }
    sm_plates_free(pl, npl);
    if (!E(rest)) R_(sc_gcut(s, g_polys(rest), num_f(depth), kd));
    return g;
}
static void at_(Geo *line, double u, double *x, double *y, double *a) { R_(sm_at_line(line, u, x, y, a)); }
static Geo *side_(Geo *line, double d, double w) { return LAG(sm_side(line, d, w)); }
static Geo *var_(Geo *line, PyRandom *rng, double w0, double w1) { return LAG(sm_var(line, rng, w0, w1, 0.9)); }
static double *arange_(double a, double b, double st, int64_t *n) {
    int64_t m = np_arange(a, b, st, NULL, 0); double *v;
    if (m < 0) cp_raise(SC_ERR);
    v = (double *)LM(sizeof(double) * (size_t)(m + 1)); if (m) np_arange(a, b, st, v, m);
    *n = m; return v;
}
static Geo *rect_(double x, double y, double l, double w, double a) { return LAG(lf_rect(x, y, l, w, a)); }
static Geo *linebuf_flat(Geo *line, double w) { return LA(shp_buffer(line->g, w, 16, SHP_CAP_FLAT, SHP_JOIN_ROUND, 5.0, 0)); }          /* line.buffer(w, cap_style=2) */
static void ff_(double x, double y, double a, double u, double v, double *out) { Frame f = lf_frame(x, y, a); lf_at(&f, u, v, &out[0], &out[1]); }

static Geo *P_ruts(Sc *sc, Geo *line, PyRandom *rng) { GeoList *l = LL(); gl_push(l, side_(line, -1.3, 0.5)); gl_push(l, side_(line, 1.3, 0.5)); return lay_cut(sc, U(l), 0.4, "rut", 0); }
static Geo *P_trail(Sc *sc, Geo *line, PyRandom *rng) {
    Geo *g = lay_cut(sc, var_(line, rng, 3.5, 4.5), 0.3, "rut", 0), *core; GeoList *prints = LL(), *ps = LL(); int64_t n, i; double *us = arange_(2.5, g_len(line) - 1.0, 1.35, &n);
    for (i = 0; i < n; i++) {
        double x, y, a, c[2]; Geo *d;
        at_(line, us[i], &x, &y, &a); ff_(x, y, a, 0, i % 2 ? 0.5 : -0.5, c);
        d = LAG(sm_disc(c[0], c[1], 1, 10));
        gl_push(prints, LA(shp_rotate(LA(shp_scale(d->g, 0.62, 0.36, SHP_ORIGIN_CENTER, 0, 0))->g, LF_DEGREES(a), SHP_ORIGIN_POINT, c[0], c[1], 0)));
    }
    core = !E(g) ? g_inset(g, 0.5) : g;
    for (i = 0; i < prints->n; i++) if (g_contains(core, prints->v[i])) gl_push(ps, prints->v[i]);
    if (ps->n && !terr(sc->s)) {
        SmPlate *pl = NULL; int npl = 0; R_(sm_plates(sc->s, &pl, &npl)); sm_plates_free(pl, npl);
        if (!npl) R_(sc_gcut(sc->s, ps, num_f(0.55), kind_str("pit")));
    }
    return g;
}
/* a track's real parts at 1:56 (2026-11-21), mm at object size 3: in a mine an 18 in (457 mm) tub track on 12 lb/yd rail (ASCE
   12: 50.8 mm high, foot 50.8, head 25.4), sleepers 0.9 x 0.11 m every 0.6 m; elsewhere (a trench railway, a foundry line) a
   600 mm track on 20 lb/yd rail (ASCE 20 / BS 20M: 66.7 mm high, foot 56-67, head 31-34), sleepers 1.2 x 0.15 m every 0.64 m.
   Sleepers about 2 x gauge long, half in the ballast. Rails at least the 0.88 mm that prints. */
typedef struct { double sp, sw, sl, sh, ro, rw, rh, hw, rmin; } Trk;          /* rmin: the tightest curve, m (a mine line about 6 m, a 600 mm line about 10 m) */
static Trk trk_of(const Sc *sc) {
    Trk t; int mine = sc->style && !strcmp(sc->style, "mine");
    if (mine) { t.sp = 10.7; t.sw = 2.0; t.sl = 16.1; t.sh = 0.8; t.ro = 4.3; t.rw = 0.9; t.rh = 0.9; t.hw = 8.6; t.rmin = 6.0; }          /* gauge 8.2 mm between the rails */
    else { t.sp = 11.5; t.sw = 2.7; t.sl = 21.4; t.sh = 1.0; t.ro = 5.4; t.rw = 1.1; t.rh = 1.2; t.hw = 11.0; t.rmin = 10.0; }          /* gauge 10.7 mm */
    return t;
}
static Geo *P_track(Sc *sc, Geo *line, PyRandom *rng) {
    GeoList *t = LL(), *sd = LL(); int64_t n, i; UpL L[2]; double *us; Trk T = trk_of(sc);          /* true size: the real parts (trk_of), times the size slider; the classic one as drawn */
    double f = ft_obj_scale() > 0 ? ft_obj_scale() / 3.0 : 0, sp = f ? T.sp * f : 1.65, sw = f ? T.sw * f : 0.95, sl = f ? T.sl * f : 3.6, ro = f ? T.ro * f : 1.15, rw = f ? pmax(T.rw * f, 0.88) : 0.92;
    us = arange_(f ? sp * 0.5 : 0.8, g_len(line), sp, &n);
    for (i = 0; i < n; i++) { double x, y, a; at_(line, us[i], &x, &y, &a); gl_push(t, rect_(x, y, sw, sl, a + py_uniform(rng, -0.05, 0.05))); }
    L[0].g = U(t); L[0].h = f ? T.sh : 0.4; L[0].k = sc->pal.wood;
    gl_push(sd, side_(line, -ro, rw)); gl_push(sd, side_(line, ro, rw));
    L[1].g = U(sd); L[1].h = f ? T.sh + T.rh * f : 0.85; L[1].k = sc->pal.iron;          /* the rail's top: on the sleeper (low enough to stay under a model in its space: the path's own pieces up to 3 mm) */
    return lay_up(sc, L, 2, 0);
}
static Geo *P_boards(Sc *sc, Geo *line, PyRandom *rng) {
    GeoList *t = LL(), *sd = LL(); int64_t n, i; double *us = arange_(0.7, g_len(line), 1.22, &n); UpL L[2]; Geo *pl; const char *fl = sc->pal.floor;
    for (i = 0; i < n; i++) {
        double x, y, a, w; at_(line, us[i], &x, &y, &a);
        if (!(py_random(rng) > 0.07)) continue;
        w = py_uniform(rng, 3.5, 4.0);
        gl_push(t, rect_(x, y, 1.0, w, a + py_uniform(rng, -0.05, 0.05)));
    }
    pl = U(t);
    gl_push(sd, side_(line, -1.35, 0.92)); gl_push(sd, side_(line, 1.35, 0.92));
    L[0].g = U(sd); L[0].h = 0.35; L[0].k = sc->pal.wood;
    L[1].g = pl; L[1].h = 0.7;
    L[1].k = fl && (!strcmp(fl, "board") || !strcmp(fl, "duck") || !strcmp(fl, "plank") || !strcmp(fl, "log") || !strcmp(fl, "drift")) ? fl : sc->pal.wood;
    return lay_up(sc, L, 2, 0);
}
static Geo *P_stream(Sc *sc, Geo *line, PyRandom *rng) {
    double w = py_uniform(rng, 2.6, 3.6) * ppow(pmax(1.0, SC.K), 0.3);
    set_geo(&sc->s->wash, line);
    return lay_cut(sc, var_(line, rng, w * 0.8, w * 1.25), 0.55, sc->pal.water, 1);
}
static Geo *P_stones(Sc *sc, Geo *line, PyRandom *rng) {
    const char *kind = sc->pal.rock ? sc->pal.rock : sc->pal.stone; Geo *inn = g_ptbuf(0, 0, SC.R0 - 1.0, 64), *wet; GeoList *sts = LL(), *wl = LL(), *pads = LL(), *dry = LL();
    int64_t n, i; double *us = arange_(1.5, g_len(line), 2.6, &n); int j; UpL L[1]; Geo *g;
    for (i = 0; i < n; i++) {
        double x, y, a, c[2], u0, r; int64_t nv; Geo *q;
        at_(line, us[i], &x, &y, &a); u0 = py_uniform(rng, -0.3, 0.3); ff_(x, y, a, u0, i % 2 ? 0.7 : -0.7, c);
        r = py_uniform(rng, 1.0, 1.3); nv = py_randint(rng, 6, 8);
        q = LAG(lf_stone(c[0], c[1], r, rng, 0.85, (int)nv, 0));
        if (g_contains(inn, q)) gl_push(sts, q);
    }
    for (i = 0; i < sc->s->gcuts.n; i++) if (in_wetk(sc->s->gcuts.v[i].kind)) for (j = 0; j < sc->s->gcuts.v[i].polys.n; j++) gl_push(wl, sc->s->gcuts.v[i].polys.v[j]);
    wet = U(wl);
    for (j = 0; j < sts->n; j++) if (!E(wet) && g_contains(g_inset(wet, 0.3), sts->v[j])) gl_push(pads, sts->v[j]);
    for (j = 0; j < sts->n; j++) if (E(wet) || !intersects(sts->v[j], g_buf(wet, 0.3))) gl_push(dry, sts->v[j]);
    if (pads->n) {
        char k2[200]; GeoList *out = LL(); const char *c0;
        if (!kind) cp_raise(SC_ERR);          /* None + "_pad": TypeError */
        snprintf(k2, sizeof k2, "%s_pad", kind);
        c0 = kmap_get(&KD.col, kind ? kind : ""); kmap_setdefault(&KD.col, k2, c0 ? c0 : "#999999");
        R_(sc_prism(sc->s, pads, num_f(H_ - 0.12), kind_str(k2), num_f(H_ - 0.8), SC.MINW, 0, out));
    }
    L[0].g = U(dry); L[0].h = 0.4; L[0].k = kind;
    g = lay_up(sc, L, 1, 1);
    return g_union(g, U(pads));
}
static Geo *P_way(Sc *sc, Geo *line, PyRandom *rng) {
    Geo *strip = linebuf_flat(line, 2.25); GeoList *cuts = LL(), *sl = LL(), *ps; int64_t n, i; double *us = arange_(0.0, g_len(line), 3.0, &n); UpL L[1];
    for (i = 0; i < n; i++) { double x, y, a; at_(line, us[i], &x, &y, &a); gl_push(cuts, rect_(x, y, 0.28, 6, a)); }
    gl_push(cuts, linebuf_flat(line, 0.14));
    ps = g_polys(g_diff(strip, U(cuts)));
    for (i = 0; i < ps->n; i++) if (area(ps->v[i]) > 1.5 && g_wide(ps->v[i], 1.2)) gl_push(sl, ps->v[i]);
    L[0].g = U(sl); L[0].h = sc->paved ? 0.45 : 0.3; L[0].k = sc->pal.floor ? sc->pal.floor : sc->pal.stone;
    return lay_up(sc, L, 1, 0);
}
static Geo *P_pipe(Sc *sc, Geo *line, PyRandom *rng) {
    GeoList *c = LL(); int64_t n, i; double *us = arange_(2.0, g_len(line), 5.0, &n); UpL L[2];
    for (i = 0; i < n; i++) { double x, y, a; at_(line, us[i], &x, &y, &a); gl_push(c, rect_(x, y, 0.95, 2.4, a)); }
    L[0].g = linebuf_flat(line, 0.8); L[0].h = 0.9; L[0].k = "pipe";
    L[1].g = U(c); L[1].h = 0.98; L[1].k = "pipe";
    return lay_up(sc, L, 2, 0);
}
static Geo *P_cable(Sc *sc, Geo *line, PyRandom *rng) {
    static const double POP[3] = { -1.0, 0.0, 1.0 }; int64_t k = py_randint(rng, 2, 3), idx[3], j, n, i; GeoList *out = LL(); UpL L[1];
    if (py_sample_e(rng, 3, k, idx) != 0) cp_raise(SC_ERR);
    for (j = 0; j < k; j++) {
        double d = POP[idx[j]], fq = py_uniform(rng, 0.25, 0.45), ph = py_uniform(rng, 0, 6.28), *us = arange_(0, g_len(line) + 0.7, 0.7, &n), *xy;
        xy = (double *)LM(sizeof(double) * 2 * (size_t)(n + 1));
        for (i = 0; i < n; i++) { double x, y, a; at_(line, us[i], &x, &y, &a); ff_(x, y, a, 0, d + 0.3 * py_sin(fq * us[i] + ph), &xy[2 * i]); }
        gl_push(out, LA(shp_line_buffer(xy, (int)n, 0.46, SHP_CAP_ROUND, SHP_JOIN_ROUND, 4, 5.0)));
    }
    L[0].g = U(out); L[0].h = 0.4; L[0].k = "cable";
    return lay_up(sc, L, 1, 0);
}
static Geo *P_veins(Sc *sc, Geo *line, PyRandom *rng) {
    double L = g_len(line), *us, *u2; int64_t n, i, m; GeoList *parts = LL(); UpL Ly[1];
    us = arange_(0, L, 1.2, &n);
    u2 = (double *)LM(sizeof(double) * (size_t)(n + 2)); memcpy(u2, us, sizeof(double) * (size_t)n); u2[n] = L;
    for (i = 0; i < n; i++) {          /* zip(us, us[1:]) */
        double u0 = u2[i], u1 = u2[i + 1], w = 0.95 + 1.2 * (u0 / L), x0, y0, a0, x1, y1, a1, p[2], q[2];
        at_(line, u0, &x0, &y0, &a0); at_(line, u1, &x1, &y1, &a1);
        ff_(x0, y0, a0, 0, 0.35 * py_sin(u0 * 0.5), p); ff_(x1, y1, a1, 0, 0.35 * py_sin(u1 * 0.5), q);
        gl_push(parts, LAG(sm_cap(p, q, w)));
    }
    {   double st = py_uniform(rng, 5.0, 7.5), *v = arange_(4.0, L - 3.0, st, &m);
        for (i = 0; i < m; i++) {
            double x, y, a, b, Ln, pts[16]; int64_t ch; int np_, j;
            at_(line, v[i], &x, &y, &a);
            if (py_choice_e(rng, 2, &ch) != 0) cp_raise(SC_ERR);
            b = a + (ch ? 1 : -1) * py_uniform(rng, 0.6, 1.0) + PI; Ln = py_uniform(rng, 3.0, 5.5);
            np_ = lf_wiggle(x, y, x + Ln * py_cos(b), y + Ln * py_sin(b), rng, 0.5, 3, pts);
            for (j = 0; j < np_ - 1; j++) gl_push(parts, LAG(sm_cap(&pts[2 * j], &pts[2 * j + 2], 0.95)));
        }
    }
    Ly[0].g = U(parts); Ly[0].h = 0.6; Ly[0].k = sc->pal.vein;
    return lay_up(sc, Ly, 1, 0);
}
typedef Geo *(*PathFn)(Sc *, Geo *, PyRandom *);
static PathFn path_fn(const char *kind) {
    if (!strcmp(kind, "ruts")) return P_ruts;
    if (!strcmp(kind, "trail")) return P_trail;
    if (!strcmp(kind, "track")) return P_track;
    if (!strcmp(kind, "boards")) return P_boards;
    if (!strcmp(kind, "stream")) return P_stream;
    if (!strcmp(kind, "stones")) return P_stones;
    if (!strcmp(kind, "way")) return P_way;
    if (!strcmp(kind, "pipe")) return P_pipe;
    if (!strcmp(kind, "cable")) return P_cable;
    if (!strcmp(kind, "veins")) return P_veins;
    cp_raise(SC_ERR);          /* PATHS[kind]: KeyError */
}
static double path_w(const char *kind) { int i = kd_path(kind); if (i < 0) cp_raise(SC_ERR); return KD.path_w[i]; }
typedef struct { Sc *sc; PathFn f; Geo *line; PyRandom *rng; Geo *g; } PCall;
static void pcall_(void *p) { PCall *c = (PCall *)p; c->g = c->f(c->sc, c->line, c->rng); if (c->g) geo_ref(c->g); }

static void lay_path(Sc *sc, KVal *plan) {          /* lay_path(sc, plan) */
    KVal *pth = KG(plan, "path"); const char *kind = KS_(pth, "kind"); double R = sc->R, c[2], front[2], p1[2], n[2], hw, bend; Geo *line; PCall pc; PyRandom pr;
    if (!strcmp(kind, "none")) return;
    c[0] = sc->c[0]; c[1] = sc->c[1]; front[0] = sc->front[0]; front[1] = sc->front[1]; p1[0] = sc->p1[0]; p1[1] = sc->p1[1];
    n[0] = -front[1]; n[1] = front[0]; hw = path_w(kind); bend = KN(KG(pth, "bend"));
    if (ft_obj_scale() > 0 && !strcmp(kind, "track")) hw = trk_of(sc).hw * ft_obj_scale() / 3.0;          /* true size: the real track's half width (trk_of) */
    if (ft_obj_scale() > 0 && !strcmp(kind, "track") && !strcmp(KS_(pth, "route"), "past")) {          /* true size: a real track cannot turn round a subject; it runs past it as a chord across the base, bowed no more than its tightest curve allows */
        double q[2], d1[2], pa[2], pb[2], ctrl[2], mb[2], m2[2], ch, sag, rm = trk_of(sc).rmin * 6.0 * ft_obj_scale();
        q[0] = p1[0] + front[0] * (1.2 + hw); q[1] = p1[1] + front[1] * (1.2 + hw);
        rot_(front, PI / 2 + LF_RADIANS(bend * 40), d1); rim_hit(q, d1, R + 2, pa); m2[0] = -d1[0]; m2[1] = -d1[1]; rim_hit(q, m2, R + 2, pb);
        ch = px_dist(pa, pb, 2); sag = pmin(fabs(bend) * ch * 0.5, ch * ch / (8 * rm)); if (bend < 0) sag = -sag;
        mb[0] = (pa[0] + pb[0]) / 2 + front[0] * 2 * sag; mb[1] = (pa[1] + pb[1]) / 2 + front[1] * 2 * sag; ctrl[0] = mb[0]; ctrl[1] = mb[1];
        line = curve_(pa, ctrl, pb); sc->has_start = 1; sc->start[0] = pa[0]; sc->start[1] = pa[1];
        sc->has_q = 1; sc->q[0] = q[0] + front[0] * hw; sc->q[1] = q[1] + front[1] * hw;
        kv_set(pth, "from_angle", kv_float(pround(pmod(LF_DEGREES(py_atan2(pa[1], pa[0])), 360), 1)));
    } else if (!strcmp(KS_(pth, "route"), "past")) {
        double q[2], d1[2], d2[2], pa[2], pb[2], ctrl[2];
        q[0] = p1[0] + front[0] * (1.2 + hw); q[1] = p1[1] + front[1] * (1.2 + hw);
        rot_(front, PI / 2 - LF_RADIANS(6 + fabs(bend) * 110), d1); rot_(front, -PI / 2 + LF_RADIANS(6 + fabs(bend) * 60), d2);
        if (bend < 0) { double t0 = d1[0], t1 = d1[1]; d1[0] = d2[0]; d1[1] = d2[1]; d2[0] = t0; d2[1] = t1; }
        rim_hit(q, d1, R + 2, pa); rim_hit(q, d2, R + 2, pb);
        ctrl[0] = 2 * q[0] - (pa[0] + pb[0]) / 2; ctrl[1] = 2 * q[1] - (pa[1] + pb[1]) / 2;
        line = curve_(pa, ctrl, pb); sc->has_start = 1; sc->start[0] = pa[0]; sc->start[1] = pa[1];
        sc->has_q = 1; sc->q[0] = q[0] + front[0] * hw; sc->q[1] = q[1] + front[1] * hw;
        kv_set(pth, "from_angle", kv_float(pround(pmod(LF_DEGREES(py_atan2(pa[1], pa[0])), 360), 1)));
    } else {
        double end[2], L, ctrl[2];
        if (sc->core && !E(sc->core)) ray_exit(c, front, sc->core, end); else { end[0] = p1[0]; end[1] = p1[1]; }
        if (px_dist(end, c, 2) < px_dist(p1, c, 2) * 0.5) { end[0] = p1[0] - front[0] * 1.0; end[1] = p1[1] - front[1] * 1.0; }
        if (ft_obj_scale() > 0 && !strcmp(kind, "track")) {          /* true size: a real track runs straight out of the subject (a tunnel mouth, a shaft head) to the rim, bowed no more than its tightest curve allows (sagitta L^2 / 8r; the control point twice that) */
            double s0[2], rm = trk_of(sc).rmin * 6.0 * ft_obj_scale(), bw, mx;
            rim_hit(end, front, R + 2, s0); L = px_dist(s0, end, 2);
            bw = bend * L; mx = 2 * L * L / (8 * rm); if (fabs(bw) > mx) bw = bw < 0 ? -mx : mx;
            ctrl[0] = (s0[0] + end[0]) / 2 + n[0] * bw; ctrl[1] = (s0[1] + end[1]) / 2 + n[1] * bw;
            line = curve_(s0, ctrl, end); sc->has_start = 1; sc->start[0] = s0[0]; sc->start[1] = s0[1];
            kv_set(pth, "from_angle", kv_float(pround(pmod(LF_DEGREES(py_atan2(s0[1], s0[0])), 360), 1)));
        } else {
        L = px_dist(sc->p0, end, 2);
        ctrl[0] = end[0] + front[0] * L * 0.5 + n[0] * bend * L; ctrl[1] = end[1] + front[1] * L * 0.5 + n[1] * bend * L;
        line = curve_(sc->p0, ctrl, end); sc->has_start = 1; sc->start[0] = sc->p0[0]; sc->start[1] = sc->p0[1];
        }
        sc->has_q = 1; sc->q[0] = p1[0]; sc->q[1] = p1[1];
    }
    pc.sc = sc; pc.f = path_fn(kind); pc.line = line; pc.g = NULL;
    py_seed_float(&pr, py_random(sc->mixr)); pc.rng = &pr;
    if (cp_try(pcall_, &pc) < 0) { char w[200]; snprintf(w, sizeof w, "path %s", kind); cp_log(sc, w); pc.g = NULL; }
    if (pc.g) {
        LAG(pc.g);
        if (!E(pc.g)) { set_geo(&sc->line, line); set_geo(&sc->pathg, LIn(linebuf_flat(line, hw), sc->wide)); gl_push(&sc->taken, sc->pathg); }
    }
}

/* ================================================================ the miniature's stand and the yard */
int cp_stand_shapes(const char *kind, double cx, double cy, double a, GeoList *pads, Geo **body) {          /* stand_shapes(kind, cx, cy, a) */
    const struct KMini *m = kd_mini(kind); double D = 2 * SC.R0; Frame f = lf_frame(cx, cy, a); int i;
    if (!m) return SC_ERR;
    for (i = 0; i < m->npads; i++) {
        const double *p = m->pad[i]; double x, y; GEOSGeometry *g;
        lf_at(&f, p[0] * D, p[1] * D, &x, &y);
        if (m->padn[i] == 3) g = shp_point_buffer(x, y, pmax(2.2, p[2] * D), 20);
        else { GEOSGeometry *r = shp_rect(x, y, 2 * p[2] * D, 2 * p[3] * D, a); g = r ? shp_buffer_q(r, 1.0, 6) : NULL; shp_free(r); }
        if (!g) return SC_ERR;
        gl_push_own(pads, geo_new(g));
    }
    if (pads->n) {
        const GEOSGeometry **gs = (const GEOSGeometry **)malloc(sizeof *gs * (size_t)pads->n); GEOSGeometry *u, *h;
        for (i = 0; i < pads->n; i++) gs[i] = pads->v[i]->g;
        u = shp_union_all(gs, pads->n); free(gs);
        h = u ? shp_convex_hull(u) : NULL; shp_free(u);
        *body = h ? geo_new(shp_buffer_q(h, m->reach * D + 0.5, 12)) : NULL; shp_free(h);
    } else *body = geo_new(shp_polygon(NULL, 0));
    return *body ? 0 : SC_ERR;
}
static CpOut *CO;                         /* what the running compose leaves on s */
static int hold_stand(Sc *sc, KVal *plan, const double *centre, int has_facing, double facing) {          /* hold_stand(sc, plan, centre, facing) */
    Scene *s = sc->s; const char *kind = sc->mini; const struct KMini *mi = kd_mini(kind); GeoList *raised = LL(), *pads0 = LL(); SmIdSet cells; PlHits block;
    Geo *wet = sc->wet, *b0; double bb[4], depth; PyRandom rr; int i, j, nc = 0, have = 0; double (*cands)[3]; double best_score = 0; GeoList *best_pads = NULL; Geo *best_body = NULL; double bc[2] = { 0, 0 }, ba = 0;
    if (!strcmp(kind, "none") || !mi || !mi->npads) return 0;
    ids_of_cells(s, &cells);
    for (i = 0; i < s->ops.n; i++) {
        Op *op = s->ops.v[i];
        if (in_bridge(op->kind) || in_soft(op->kind) || in_terrk(op->kind)) continue;
        for (j = 0; j < op->polys.n; j++) if (area(op->polys.v[j]) >= 10 && !sm_ids_has(&cells, op->polys.v[j])) gl_push(raised, op->polys.v[j]);
    }
    if (sc->solid && !E(sc->solid)) gl_push(raised, sc->solid);
    R_(pl_hits_init(&block, raised));
    R_(cp_stand_shapes(kind, 0, 0, 0, pads0, &b0)); LAG(b0);
    g_bounds(b0, bb); depth = (bb[2] - bb[0]) / 2;
    py_seed_float(&rr, py_random(sc->mixr));
    cands = (double (*)[3])LM(sizeof(double) * 3 * 100);
    if (!centre) {
        double fa = py_atan2(-sc->front[1], -sc->front[0]), n[2] = { -sc->front[1], sc->front[0] }; static const double TURN[4] = { 0.0, 1.5707963267948966, 0.5, -0.5 }, BACK[3] = { 0.0, 3.0, 7.0 }, SIDE[7] = { 0.0, 4.0, -4.0, 8.0, -8.0, 13.0, -13.0 }; int t, b, sd;
        for (t = 0; t < 4; t++) {
            double d = (fabs(TURN[t]) < 1.0 ? depth : (bb[3] - bb[1]) / 2) + 1.5;
            for (b = 0; b < 3; b++) for (sd = 0; sd < 7; sd++) {
                cands[nc][0] = sc->q[0] + sc->front[0] * (d + BACK[b]) + n[0] * SIDE[sd]; cands[nc][1] = sc->q[1] + sc->front[1] * (d + BACK[b]) + n[1] * SIDE[sd]; cands[nc][2] = fa + TURN[t]; nc++;
            }
        }
    } else {
        for (i = 0; i < 14; i++) {
            double ux = py_uniform(&rr, -1, 1), x = centre[0] + ux * i * 0.6, uy = py_uniform(&rr, -1, 1), y = centre[1] + uy * i * 0.6;
            double base = has_facing && facing != 0.0 ? facing : 0.0;
            cands[nc][0] = x; cands[nc][1] = y; cands[nc][2] = i < 6 ? base + 0 : base + py_uniform(&rr, 0, 6.28); nc++;
        }
    }
    for (i = 0; i < nc; i++) {
        int mark = ly_mark(); GeoList *pads = LL(); Geo *body, *pu; double score; int hits = 0;
        R_(cp_stand_shapes(kind, cands[i][0], cands[i][1], cands[i][2], pads, &body)); LAG(body);
        pu = g_uu(pads);
        if (!g_contains(g_buf(sc->disc, -1.0), pu)) { ly_pop(mark); continue; }
        for (j = 0; j < pads->n; j++) { int h = pl_hits_hit(&block, pads->v[j]->g); R_(h); hits += h; }
        score = hits * 10 + (wet ? area(LIn(pu, wet)) / area(pu) * 8 : 0);
        if (!have || score < best_score) {
            have = 1; best_score = score; best_pads = pads; best_body = body; bc[0] = cands[i][0]; bc[1] = cands[i][1]; ba = cands[i][2];
        }
        if (score == 0) break;
    }
    pl_hits_free(&block);
    if (!have) {
        sm_ids_free(&cells);
        if (!centre) { double c2[2] = { sc->q[0] * 0.4, sc->q[1] * 0.4 }; return hold_stand(sc, plan, c2, 1, py_atan2(-sc->front[1], -sc->front[0])); }
        return 0;
    }
    {
        Geo *pu = g_uu(best_pads), *g = g_buf(pu, 0.4), *keep; ShpPrep gp; GeoList *dead = LL(); SwapMap sw = { 0 }; GcutList gc = { 0 }; TcutList tc = { 0 };
        if (terr(s)) R_(sm_clear_terrain(s, g));
        gp = shp_prep(g->g); sm_ids_free(&cells); ids_of_cells(s, &cells);
        keep = sc->solid ? g_buf(sc->solid, 0.1) : g_empty();
        for (i = 0; i < s->ops.n; i++) {
            Op *op = s->ops.v[i];
            if (in_bridge(op->kind) || in_terrk(op->kind)) continue;
            for (j = 0; j < op->polys.n; j++) {
                Geo *p = op->polys.v[j];
                if (sm_ids_has(&cells, p) || !R_(shp_prep_intersects(&gp, p->g))) continue;
                if (!(area(p) < 10 || g_contains(g, p))) continue;
                if (intersects(keep, p)) continue;
                gl_push(dead, p);
            }
        }
        R_(sm_remove(s, dead));
        for (i = 0; i < s->ops.n; i++) {
            Op *op = s->ops.v[i];
            if (op->type != OP_PRISM || in_bridge(op->kind) || in_terrk(op->kind) || in_soft(op->kind)) continue;
            for (j = 0; j < op->polys.n; j++) {
                Geo *p = op->polys.v[j];
                if (!sm_ids_has(&cells, p) && R_(shp_prep_intersects(&gp, p->g)) && !intersects(keep, p)) {
                    GeoList *xs = LL(), *ps = g_polys(g_diff(p, g)); int k;
                    for (k = 0; k < ps->n; k++) if (area(ps->v[k]) > 0.8 && wide_d(ps->v[k])) gl_push(xs, ps->v[k]);
                    swap_put(&sw, p, xs);
                }
            }
        }
        if (sw.n) apply_swap(s, &sw);
        free(sw.v);
        for (i = 0; i < s->gcuts.n; i++) {
            Gcut *c = &s->gcuts.v[i]; GeoList ps2 = { 0 };
            for (j = 0; j < c->polys.n; j++) {
                Geo *p = c->polys.v[j]; GeoList *qs; int k;
                if (R_(shp_prep_intersects(&gp, p->g)) && !ksame(c->kind, "crack") && !ksame(c->kind, "joint")) qs = g_polys(g_diff(p, g)); else qs = L1(p);
                for (k = 0; k < qs->n; k++) if (area(qs->v[k]) > 0.1) gl_push(&ps2, qs->v[k]);
            }
            if (ps2.n) gcl_push(&gc, &ps2, c->depth, c->kind);
            gl_free(&ps2);
        }
        gcuts_replace(s, &gc);
        for (i = 0; i < s->tcuts.n; i++) {
            Tcut *t = &s->tcuts.v[i]; GeoList ps2 = { 0 };
            for (j = 0; j < t->polys.n; j++) {
                Geo *p = t->polys.v[j]; GeoList *qs; int k;
                if (R_(shp_prep_intersects(&gp, p->g)) && !ksame(t->kind, "crack") && !ksame(t->kind, "joint")) qs = g_polys(g_diff(p, g)); else qs = L1(p);
                for (k = 0; k < qs->n; k++) if (!E(g_buf(qs->v[k], -0.1))) gl_push(&ps2, qs->v[k]);
            }
            if (ps2.n) tcl_push(&tc, &ps2, t->z0, t->z1, t->kind);
            gl_free(&ps2);
        }
        tcuts_replace(s, &tc);
        shp_prep_free(&gp);
        {   /* s.held = getattr(s, "held", []) + pads; s.feet += pads; s.keepout += pads (terrain) */
            GeoList nh = { 0 };
            if (s->has_held) gl_copy(&nh, &s->held);
            gl_extend(&nh, best_pads); gl_free(&s->held); s->held = nh; s->has_held = 1;
            gl_extend(&s->feet, best_pads);
            if (terr(s)) gl_extend(&s->keepout, best_pads);
        }
        CO->stand.has_stand = 1; CO->stand.type = kind; gl_free(&CO->stand.pads); gl_copy(&CO->stand.pads, best_pads); set_geo(&CO->stand.body, best_body);
        CO->stand.at[0] = pround(bc[0], 1); CO->stand.at[1] = pround(bc[1], 1); CO->stand.facing = print_(pmod(LF_DEGREES(ba), 360));
        gl_clear(&sc->stand_pads); gl_push(&sc->stand_pads, pu);
        gl_push(&sc->taken, best_body);
        set_geo(&sc->yard, !E(best_body) ? LIn(best_body, sc->disc) : NULL);
        {
            KVal *st = kv_dict(), *at = kv_list();
            kv_set(st, "type", kv_str(kind)); kv_push(at, kv_float(CO->stand.at[0])); kv_push(at, kv_float(CO->stand.at[1])); kv_set(st, "at", at); kv_set(st, "facing", kv_int(CO->stand.facing));
            kv_set(plan, "stand", st);
        }
        sm_ids_free(&cells);
    }
    return 1;
}

static void hold_yard(Sc *sc, KVal *plan) {          /* hold_yard(sc, plan) */
    Scene *s = sc->s; double yr, cy[2]; Geo *y, *keep, *pg; GeoList *ps, *dead = LL(); SmIdSet cells; ShpPrep yp; int i, j; KVal *op_;
    if (hold_stand(sc, plan, NULL, 0, 0)) return;
    op_ = kv_get(plan, "open");
    yr = (4.5 + 2.0 * sc->K) * (op_ ? KN(op_) : 1.0) * (sc->mm < 59.9 ? 0.75 : 1.0);
    cy[0] = sc->q[0] + sc->front[0] * yr * 0.9; cy[1] = sc->q[1] + sc->front[1] * yr * 0.9;
    y = g_diff(LIn(g_ptbuf(cy[0], cy[1], yr, 48), sc->disc), g_buf(sc->subj, 0.3));
    ps = g_polys(y);
    if (!ps->n) return;
    { double *a = (double *)LM(sizeof(double) * (size_t)ps->n); for (i = 0; i < ps->n; i++) a[i] = area(ps->v[i]); y = ps->v[px_max_idx(a, ps->n)]; }
    if (area(y) < 20) return;
    set_geo(&sc->yard, y);
    if (terr(s)) R_(sm_clear_terrain(s, y));
    ids_of_cells(s, &cells); yp = shp_prep(y->g); keep = g_buf(sc->solid, 0.1); pg = sc->pathg ? g_buf(sc->pathg, 0.3) : NULL;
    for (i = 0; i < s->ops.n; i++) {
        Op *op = s->ops.v[i];
        if (in_bridge(op->kind) || in_soft(op->kind) || in_terrk(op->kind)) continue;
        for (j = 0; j < op->polys.n; j++) {
            Geo *p = op->polys.v[j];
            if (area(p) < 10 && !sm_ids_has(&cells, p) && R_(shp_prep_intersects(&yp, p->g)) && !intersects(keep, p) && (!pg || !intersects(pg, p))) gl_push(dead, p);
        }
    }
    shp_prep_free(&yp); sm_ids_free(&cells);
    R_(sm_remove(s, dead));
    { GeoList nh = { 0 }; if (s->has_held) gl_copy(&nh, &s->held); gl_push(&nh, y); gl_free(&s->held); s->held = nh; s->has_held = 1; }
    gl_push(&s->feet, y);
    if (terr(s)) gl_push(&s->keepout, y);
}

void cp_release(Scene *s) {          /* release(s) */
    int i, k, j;
    if (s->has_held) for (i = 0; i < s->held.n; i++) {
        Geo *p = s->held.v[i];
        for (k = 0; k < 2; k++) {
            GeoList *lst = k == 0 ? &s->feet : (s->has_objs ? &s->keepout : NULL);
            if (!lst) continue;
            for (j = lst->n - 1; j >= 0; j--) if (lst->v[j] == p) { geo_unref(lst->v[j]); memmove(&lst->v[j], &lst->v[j + 1], sizeof(Geo *) * (size_t)(lst->n - j - 1)); lst->n--; }
        }
    }
    gl_free(&s->held); s->has_held = 1;
}

/* ================================================================ running details */
static double share(void) {          /* _share() */
    double v;
    if (!CPC.objects_amount) return 1.0;
    if (ft_pyfloat(CPC.objects_amount, &v) != 0) return 1.0;
    return pmax(0.0, pmin(1.0, v));
}
static int count_(Sc *sc, double n) {          /* _count(sc, n) */
    n = n * pmax(1.0, sc->K / 2);
    return (int)print_(n + (n >= 2 ? py_uniform(sc->mixr, -1, 1) : 0));
}
typedef struct { int a, b, c; int64_t d; } Snap;
static Snap snap(Scene *s) {          /* _snap(s) */
    Snap r; int i; r.a = s->ops.n; r.b = s->gcuts.n; r.c = s->tcuts.n; r.d = 0;
    for (i = s->ops.n - 3 < 0 ? 0 : s->ops.n - 3; i < s->ops.n; i++) r.d += s->ops.v[i]->polys.n;
    return r;
}
static double cp_grow = 1.0;          /* true size on a big base (clutter): loose debris and growth made this much larger, fewer of them */
static int cp_grow_check = 0;          /* grow_new: 1 = keep the growth only if it cuts into no detail made before (run_ places at the small size) */
static int op_ground(const Scene *s, const Op *o) {          /* the ground, not a standing detail (as audit.c counts it) */
    static const char *G[] = { "bedrock", "garden", "grass", "flower", "flesh", "moss", "pan", "mud", "sand", "ash", "dust", "spoil", "pavers", "slate", NULL }; int i; size_t n;
    for (i = 0; i < s->base_ops.n; i++) if (s->base_ops.v[i] == o) return 1;
    if (!o->kind) return 0;
    if (in_soft(o->kind)) return 1;
    if (o->feat) return 0;
    if (in_bridge(o->kind) || in_terrk(o->kind)) return 1;
    for (i = 0; G[i]; i++) if (!strcmp(o->kind->s, G[i])) return 1;
    n = strlen(o->kind->s); return n > 2 && !strcmp(o->kind->s + n - 2, "_p");
}
static int op_hits(Sc *sc, Op *n, int upto, int skip) {          /* n (not ground) cuts into a detail among ops[0..upto) other than ops[skip] */
    Scene *s = sc->s; int i, j, b; double ba[4], bb[4], z0 = n->n[0].v, t = op_top(n);
    for (i = 0; i < upto; i++) {
        Op *e = s->ops.v[i];
        if (i == skip || !e || op_ground(s, e) || (n->feat && e->feat == n->feat)) continue;
        if (z0 >= op_top(e) - 0.2 || e->n[0].v >= t - 0.2) continue;          /* one stands on the other */
        for (j = 0; j < n->polys.n; j++) { g_bounds(n->polys.v[j], ba);
            for (b = 0; b < e->polys.n; b++) { g_bounds(e->polys.v[b], bb);
                if (ba[2] < bb[0] || bb[2] < ba[0] || ba[3] < bb[1] || bb[3] < ba[1]) continue;
                { Geo *x = LIn(n->polys.v[j], e->polys.v[b]); if (x && !g_isempty(x) && area(x) > 0.1) return 1; }
            }
        }
    }
    return 0;
}
static int scene_hits(Sc *sc, int from) {          /* a piece made since from cuts into a standing piece the layout or scene laid itself (a pipe run, veins, rails) */
    Scene *s = sc->s; int i, j, a, b; double ba[4], bb[4];
    for (i = 0; i < from; i++) {
        Op *e = s->ops.v[i];
        if (!e || e->feat > 0 || op_ground(s, e) || op_top(e) - e->n[0].v <= 1.0 || !e->polys.n) continue;          /* over 1 mm tall: low planks and slabs a detail can stand among stay allowed */
        for (a = from; a < s->ops.n; a++) {
            Op *n = s->ops.v[a];
            if (op_ground(s, n) || n->n[0].v >= op_top(e) - 0.2 || e->n[0].v >= op_top(n) - 0.2) continue;
            for (j = 0; j < n->polys.n; j++) { g_bounds(n->polys.v[j], ba);
                for (b = 0; b < e->polys.n; b++) { g_bounds(e->polys.v[b], bb);
                    if (ba[2] < bb[0] || bb[2] < ba[0] || ba[3] < bb[1] || bb[3] < ba[1]) continue;
                    { Geo *x = LIn(n->polys.v[j], e->polys.v[b]); if (x && !g_isempty(x) && area(x) > 0.1) return 1; }
                }
            }
        }
    }
    return 0;
}
static int grown_hits(Sc *sc, int from, Op **news, int nm) {          /* a grown piece in the same place and height as an earlier detail */
    int a; for (a = 0; a < nm; a++) if (!op_ground(sc->s, news[a]) && op_hits(sc, news[a], from, -1)) return 1;
    return 0;
}
static int cp_piv = 0; static double cp_pivx, cp_pivy;          /* grow_new: grow about this point, not the middle (set by grow_since) */
static int grow_new(Sc *sc, int from, int cfrom) {          /* returns 0 when cp_grow_check found the grown detail cutting into another (nothing changed) */          /* the ops (and chamfered rocks) made since from, larger about their middle; heights by the root */
    Scene *s = sc->s; double bb[4], lo[2] = { 1e300, 1e300 }, hi[2] = { -1e300, -1e300 }, cx, cy, k = cp_grow, kh = sqrt(cp_grow); int i, j, nm = 0; Op **olds = (Op **)LM(sizeof(Op *) * (size_t)(s->ops.n - from + 1)), **news = (Op **)LM(sizeof(Op *) * (size_t)(s->ops.n - from + 1));
    for (i = from; i < s->ops.n; i++) for (j = 0; j < s->ops.v[i]->polys.n; j++) { g_bounds(s->ops.v[i]->polys.v[j], bb); lo[0] = pmin(lo[0], bb[0]); lo[1] = pmin(lo[1], bb[1]); hi[0] = pmax(hi[0], bb[2]); hi[1] = pmax(hi[1], bb[3]); }
    if (lo[0] > hi[0]) return 1;
    cx = (lo[0] + hi[0]) / 2; cy = (lo[1] + hi[1]) / 2;
    if (cp_piv) { cx = cp_pivx; cy = cp_pivy; }
    for (i = from; i < s->ops.n; i++) {
        Op *o = s->ops.v[i], *n; GeoList ps = { 0 };
        for (j = 0; j < o->polys.n; j++) { Geo *g = LIn(g_scale(o->polys.v[j], k, k, SHP_ORIGIN_POINT, cx, cy), sc->disc); if (g && !g_isempty(g) && area(g) > 0.05) gl_push(&ps, g); }
        if (!ps.n) { gl_free(&ps); continue; }
        n = op_like(o, &ps); gl_free(&ps);
        if (n->type == OP_PRISM) n->n[1].v = n->n[0].v + (n->n[1].v - n->n[0].v) * kh; else n->n[1].v *= kh;
        olds[nm] = o; news[nm++] = n;
    }
    if (cp_grow_check && grown_hits(sc, from, news, nm)) { for (i = 0; i < nm; i++) op_unref(news[i]); return 0; }
    for (i = from, j = 0; i < s->ops.n && j < nm; i++) if (s->ops.v[i] == olds[j]) s->ops.v[i] = news[j++];          /* o stays alive until its chop (if any) lets go of it */
    for (i = cfrom; i < s->chops.n; i++) {          /* a chamfered rock is built from its chop: the same growth, and it points at the new ops */
        Chop *c = s->chops.v[i]; Geo *g = c->poly ? LIn(g_scale(c->poly, k, k, SHP_ORIGIN_POINT, cx, cy), sc->disc) : NULL;
        if (g && !g_isempty(g)) { geo_ref(g); geo_unref(c->poly); c->poly = g; }
        c->top.v = c->z0.v + (c->top.v - c->z0.v) * kh;
        for (j = 0; j < c->ops.n; j++) { int m; for (m = 0; m < nm && c->ops.v[j] != olds[m]; m++) {} if (m < nm) { op_unref(c->ops.v[j]); c->ops.v[j] = op_ref(news[m]); } }
    }
    for (i = 0; i < nm; i++) op_unref(olds[i]);
    return 1;
}
/* true size on a big base: the layout's own loose pebbles (chamfered rocks under 3 mm2) made k larger, each about its middle */
static void grow_pebbles(Sc *sc, double k) {          /* (before the release: they are still plain rock ops; a rock's tiers grow about one middle) */
    Scene *s = sc->s; int i, q, ngrit = 0, w; double kh = k, bb[4], c[2], pot = -1e300, pnt = 0;
    if (k <= 1.0) return;
    for (i = 0; i < s->ops.n; i++) {
        Op *o = s->ops.v[i], *n; GeoList ps = { 0 }; double a = 0;
        int grit = o->kind && !strcmp(o->kind->s, "grit");
        if (!o->kind || !(grit || strstr(o->kind->s, "rock") || strstr(o->kind->s, "pebble") || strstr(o->kind->s, "stone")) || !o->polys.n) continue;
        if (grit && (ngrit++ % (int)(k * k + 0.5)) != 0) { op_unref(o); s->ops.v[i] = NULL; continue; }
        if (grit) continue;          /* the kept grit stays its size: grown it reads as blocks */          /* grit: one bit in k*k kept (and grown), smooth ground between */
        for (q = 0; q < o->polys.n; q++) a += area(o->polys.v[q]);
        if (a > 3.0) continue;
        g_bounds(o->polys.v[0], bb); c[0] = (bb[0] + bb[2]) / 2; c[1] = (bb[1] + bb[3]) / 2;
        for (q = 0; q < o->polys.n; q++) { Geo *h = LIn(g_scale(o->polys.v[q], k, k, SHP_ORIGIN_POINT, c[0], c[1]), sc->disc); if (h && !g_isempty(h) && area(h) > 0.05) gl_push(&ps, h); }
        if (!ps.n) { gl_free(&ps); continue; }
        n = op_like(o, &ps); gl_free(&ps);
        if (n->type == OP_PRISM) n->n[1].v = n->n[0].v + (n->n[1].v - n->n[0].v) * kh; else { double z0 = o->n[0].v; if (fabs(z0 - pot) < 1e-3) n->n[0].v = pnt; n->n[1].v *= kh; pot = z0 + o->n[1].v; pnt = n->n[0].v + n->n[1].v; }          /* a rock's next tier (it starts where the last one ended): on the grown one */
        if (op_hits(sc, n, s->ops.n, i)) { op_unref(n); pot = -1e300; continue; }          /* grown it would cut into a detail standing there: it stays its size */
        op_unref(o); s->ops.v[i] = n;
    }
    for (i = w = 0; i < s->ops.n; i++) if (s->ops.v[i]) s->ops.v[w++] = s->ops.v[i];          /* the dropped grit out of the list */
    s->ops.n = w;
}
/* true size (2026-11-20): what lay_scene just made (the subject, then its path) grown k times about its own middle: ops, chamfered
   rocks and the cuts that go with it (ballast, ruts), as the old --scale grew the whole layout */
static void grow_since(Sc *sc, Snap b, int c0, double k) {
    Scene *s = sc->s; double bb[4], lo[2] = { 1e300, 1e300 }, hi[2] = { -1e300, -1e300 }, cx, cy; int i, j; double keep = cp_grow;
    if (k <= 1.0) return;
    for (i = b.a; i < s->ops.n; i++) for (j = 0; j < s->ops.v[i]->polys.n; j++) { g_bounds(s->ops.v[i]->polys.v[j], bb); lo[0] = pmin(lo[0], bb[0]); lo[1] = pmin(lo[1], bb[1]); hi[0] = pmax(hi[0], bb[2]); hi[1] = pmax(hi[1], bb[3]); }
    if (lo[0] > hi[0]) return;
    cx = (lo[0] + hi[0]) / 2; cy = (lo[1] + hi[1]) / 2;
    if (sc->mm < 39.9 && cp_msp) {          /* a base for one figure: the subject beside it grows to at most a fifth of the base (a true-size ore cart or depot is as long as the base) */
        double ba = (hi[0] - lo[0]) * (hi[1] - lo[1]), cap = ba > 0 ? sqrt(0.2 * PI * sc->R * sc->R / ba) : k;
        if (cap < k) k = pmax(1.0, cap);
        if (k <= 1.0) return;
    }
    if (cp_msp) {          /* true size: placed clear of the model's space, it grows from its outer corner, and only as far as stays clear of it */
        int in0 = 0, q, step; double px = 0, py = 0, best = -1, kk = k;
        for (q = 0; q < 4; q++) { double x = q & 1 ? hi[0] : lo[0], y = q & 2 ? hi[1] : lo[1]; if (x * x + y * y > best) { best = x * x + y * y; px = x; py = y; } }
        for (i = b.a; i < s->ops.n && !in0; i++) for (j = 0; j < s->ops.v[i]->polys.n; j++) { Geo *x = LIn(s->ops.v[i]->polys.v[j], cp_msp); if (x && !g_isempty(x) && area(x) > 0.5) { in0 = 1; break; } }
        if (!in0) {          /* (a track across the middle was laid there on purpose: grown about its middle as before) */
            for (step = 0; step <= 12; step++) {
                int bad = 0; kk = k - (k - 1.0) * step / 12.0;
                for (i = b.a; i < s->ops.n && !bad; i++) for (j = 0; j < s->ops.v[i]->polys.n; j++) { Geo *x = LIn(g_scale(s->ops.v[i]->polys.v[j], kk, kk, SHP_ORIGIN_POINT, px, py), cp_msp); if (x && !g_isempty(x) && area(x) > 0.5) { bad = 1; break; } }
                if (!bad) break;
            }
            k = kk; cx = px; cy = py;
            if (k <= 1.0) return;
        }
    }
    for (i = b.b; i < s->gcuts.n; i++) { GeoList *pl = &s->gcuts.v[i].polys; for (j = 0; j < pl->n; j++) { Geo *g = LIn(g_scale(pl->v[j], k, k, SHP_ORIGIN_POINT, cx, cy), sc->disc); if (g) { geo_ref(g); geo_unref(pl->v[j]); pl->v[j] = g; } } }
    for (i = b.c; i < s->tcuts.n; i++) { GeoList *pl = &s->tcuts.v[i].polys; for (j = 0; j < pl->n; j++) { Geo *g = LIn(g_scale(pl->v[j], k, k, SHP_ORIGIN_POINT, cx, cy), sc->disc); if (g) { geo_ref(g); geo_unref(pl->v[j]); pl->v[j] = g; } } }
    if (getenv("BBB_DBG_GROW")) { FILE *df = fopen(getenv("BBB_DBG_GROW"), "a"); if (df) { fprintf(df, "grow_since k %.2f ops %d gcuts %d tcuts %d chops %d box %.1fx%.1f\n", k, s->ops.n - b.a, s->gcuts.n - b.b, s->tcuts.n - b.c, s->chops.n - c0, hi[0] - lo[0], hi[1] - lo[1]); fclose(df); } }
    cp_grow = k; cp_piv = 1; cp_pivx = cx; cp_pivy = cy; grow_new(sc, b.a, c0); cp_piv = 0; cp_grow = keep;
}
static int run_(Sc *sc, ScFn *fn, SmAt *at, SmAim *aim) {          /* _run(sc, fn, at, aim) */
    Snap b, a; SmFocus keep; int r, c0, f0;
    if (!at) return 0;
    b = snap(sc->s); c0 = sc->s->chops.n; f0 = sc->s->feet.n;
    sm_focus_enter(&keep, at, aim);
    r = call_fn(fn, sc->s, sc->rng, sc->t);
    sm_focus_exit(&keep);
    if (cp_grow > 1.0 && r >= 0 && sc->s->ops.n > b.a) {          /* grown where it was placed small: only as large as fits beside the details already there */
        double kk = cp_grow; cp_grow_check = 1;
        if (!grow_new(sc, b.a, c0)) { cp_grow = sqrt(kk); if (cp_grow > 1.05) grow_new(sc, b.a, c0); }
        cp_grow = kk; cp_grow_check = 0;
    }
    if (ft_obj_scale() > 0 && r >= 0 && sc->s->ops.n > b.a && scene_hits(sc, b.a)) {          /* true size: not on the layout's own pipes, veins or rails (taken back) */
        Scene *s = sc->s; int i;
        for (i = b.a; i < s->ops.n; i++) op_unref(s->ops.v[i]);
        s->ops.n = b.a; s->chops.n = c0; s->gcuts.n = b.b; s->tcuts.n = b.c; s->feet.n = f0;          /* (their memory goes with the scene's) */
        return 0;
    }
    if (r < 0) cp_log(sc, fn->qualname && *fn->qualname ? fn->qualname : "detail");
    a = snap(sc->s);
    return a.a != b.a || a.b != b.b || a.c != b.c || a.d != b.d;
}
static int banned(const ScFn *f, const KVal *plan) {          /* _banned(fn, plan) */
    KVal *ban = kv_get(plan, "ban"); int i; const char *k = f->tag.kind ? f->tag.kind : "", *sh = f->tag.shape ? f->tag.shape : "";
    if (!truthy(ban) || ban->t != KV_LIST) return 0;
    for (i = 0; i < ban->n; i++) if (ban->v[i]->t == KV_STR && (!strcmp(ban->v[i]->s, k) || !strcmp(ban->v[i]->s, sh))) return 1;
    return 0;
}
int cp_klass_of(const ScFn *f, const char **out) { *out = sm_klass(&f->tag); return 0; }
static const char *klass(const ScFn *f) { return sm_klass(&f->tag); }
static const ScFnList *DETAILS;           /* details.DETAILS[style] of the running compose */
/* by = {klass: [fns]} of the style's DETAILS that the plan does not ban */
typedef struct { const char *c; ScFnList l; } ByC;
typedef struct { ByC v[16]; int n; } By;
static void by_klass(By *by, const KVal *plan) {
    int i, j; memset(by, 0, sizeof *by);
    for (i = 0; DETAILS && i < DETAILS->n; i++) {
        ScFn *f = DETAILS->v[i]; const char *c;
        if (banned(f, plan)) continue;
        c = klass(f);
        for (j = 0; j < by->n; j++) if (!strcmp(by->v[j].c, c)) break;
        if (j == by->n) { if (by->n == 16) continue; by->v[by->n].c = c; memset(&by->v[by->n].l, 0, sizeof(ScFnList)); by->n++; }
        fl_add(&by->v[j].l, f);
    }
}
static const ScFnList *by_get(const By *by, const char *c) { int j; for (j = 0; j < by->n; j++) if (!strcmp(by->v[j].c, c)) return &by->v[j].l; return NULL; }
static void by_free(By *by) { int j; for (j = 0; j < by->n; j++) free(by->v[j].l.v); by->n = 0; }

/* ================================================================ no bare ground */
static double env_final_or(double dflt) {          /* float(os.environ.get("KIT_FINAL_MM") or dflt) */
    double v;
    if (!env_or(CPC.kit_final_mm)) return dflt;
    if (ft_pyfloat(CPC.kit_final_mm, &v) != 0) cp_raise(SC_ERR);
    return v;
}
static int no_bare(Sc *sc, KVal *plan, int marks_only, int cracks) {          /* no_bare(sc, plan, marks_only, cracks) */
    Scene *s = sc->s; double ts, lim; PyRandom rr; int put = 0, rnd, it, k, i; By by; ScFnList fs = { 0 }; const ScFnList *l;
    if (!CPC.no_bare_ground) return 0;
    if (CPC.kit_fault && !strcmp(CPC.kit_fault, "no_bare")) cp_raise(SC_ERR);          /* kitlog.fault("no_bare") */
    ts = ft_true_scale();
    lim = pmax(3.0, pmin(7.0, 0.07 * env_final_or(sc->mm)));
    lim = ts == 1.0 ? lim : pmax(1.7, lim / ts);
    lim *= cp_grow;          /* true size on a big base: bigger bare patches are fine (smooth ground beside the model), fewer pebbles */
    py_seed_float(&rr, py_random(sc->mixr));
    by_klass(&by, plan);
    l = by_get(&by, "marks");
    for (k = 0; k < 2; k++) for (i = 0; l && i < l->n; i++) fl_add(&fs, l->v[i]);
    if (!(marks_only || share() < 0.999)) {
        l = by_get(&by, "debris"); for (i = 0; l && i < l->n; i++) fl_add(&fs, l->v[i]);
        l = by_get(&by, "growth"); for (i = 0; l && i < l->n; i++) fl_add(&fs, l->v[i]);
    }
    if (cracks) fs.n = 0;
    for (rnd = 0; rnd < 3; rnd++) {
        double *P = NULL, *d = NULL, *w; int64_t n = 0, j; unsigned char *free_; int any;
        R_(sm_bare_grid(s, lim, &P, &d, &n));
        free_ = (unsigned char *)malloc((size_t)(n + 1)); w = (double *)malloc(sizeof(double) * (size_t)(n + 1));
        for (j = 0; j < n; j++) free_[j] = d[j] > lim;
        for (it = 0; it < 60; it++) {
            double x, y; int ok = 0; int64_t ix; SmAt *at;
            any = 0; for (j = 0; j < n; j++) if (free_[j]) { any = 1; break; }
            if (!any) break;
            for (j = 0; j < n; j++) w[j] = free_[j] ? d[j] : -1.0;
            ix = np_argmax(w, n); x = P[2 * ix]; y = P[2 * ix + 1];
            at = KA(sm_jit(x, y, lim));
            for (k = 0; k < (fs.n ? 3 : 0); k++) {
                int64_t c; int mark = ly_mark();
                if (py_choice_e(&rr, fs.n, &c) != 0) cp_raise(SC_ERR);
                if (run_(sc, fs.v[c], at, NULL)) { ok = 1; ly_pop(mark); break; }
                ly_pop(mark);
            }
            if (!ok) {          /* nothing from the pool found room: the ground itself cracks here */
                double a = py_uniform(&rr, 0, 6.283), L, pts[16], q2[4], b; int64_t nn = py_randint(&rr, 3, 5), jj; Geo *g; GeoList *gl = LL(); Num top; Geo *fp = NULL; int has;
                L = lim * py_uniform(&rr, 1.1, 1.7);
                for (i = 0; i < nn; i++) {
                    double fr = (double)i / (double)(nn - 1) - 0.5;
                    pts[2 * i] = x + fr * L * py_cos(a) + py_uniform(&rr, -1, 1) * lim * 0.16;
                    pts[2 * i + 1] = y + fr * L * py_sin(a) + py_uniform(&rr, -1, 1) * lim * 0.16;
                }
                if (py_randrange_e(&rr, 1, nn - 1, 1, &jj) != 0) cp_raise(SC_ERR);
                { int64_t c; if (py_choice_e(&rr, 2, &c) != 0) cp_raise(SC_ERR); b = a + (c ? 1 : -1) * py_uniform(&rr, 0.6, 1.2); }
                q2[0] = pts[2 * jj]; q2[1] = pts[2 * jj + 1]; q2[2] = pts[2 * jj] + L * 0.4 * py_cos(b); q2[3] = pts[2 * jj + 1] + L * 0.4 * py_sin(b);
                gl_push(gl, LA(shp_line_buffer(pts, (int)nn, 0.3, SHP_CAP_ROUND, SHP_JOIN_ROUND, 2, 5.0)));
                gl_push(gl, LA(shp_line_buffer(q2, 2, 0.26, SHP_CAP_ROUND, SHP_JOIN_ROUND, 2, 5.0)));
                g = U(gl);
                has = R_(sm_top_at(s, x, y, &top, &fp));
                if (has) {
                    GeoList *ps = g_polys(LIn(g, g_buf(LAG(fp), -0.3))), *keep = LL();
                    for (i = 0; i < ps->n; i++) if (area(ps->v[i]) > 0.05) gl_push(keep, ps->v[i]);
                    { extern double kit_strength; R_(sc_tcut(s, keep, num_sub(top, num_f(0.35 * kit_strength)), top, kind_str("crack"))); }
                } else { extern double kit_strength; R_(sc_gcut(s, g_polys(g), num_f(0.4 * kit_strength), kind_str("crack"))); }
            }
            put++;
            { double r2 = ppow(lim * 1.2, 2); for (j = 0; j < n; j++) { double dx = P[2 * j] - x, dy = P[2 * j + 1] - y; free_[j] = free_[j] && (dx * dx + dy * dy > r2); } }
        }
        free(P); free(d); free(free_); free(w);
    }
    free(fs.v); by_free(&by);
    return put;
}

static void clear_edge(Sc *sc) {          /* clear_edge(sc) */
    double R = sc->R; Geo *ring, *zone; GeoList *keep = LL(), *src = LL(); int i;
    if (sc->mm < 39.9) return;
    ring = g_diff(sc->disc, g_ptbuf(0, 0, R * (sc->mm >= 59.9 ? 0.8 : 0.86), 96));
    if (sc->subj) gl_push(src, sc->subj);
    gl_extend(src, &sc->also);
    if (sc->has_rim) gl_extend(src, &sc->rim);
    if (sc->focal) gl_push(src, sc->focal);
    for (i = 0; i < src->n; i++) if (src->v[i] && !E(src->v[i])) gl_push(keep, g_buf(src->v[i], 2.5));
    if (sc->pathg) gl_push(keep, g_buf(sc->pathg, 2.0));
    if (!E(sc->waters)) gl_push(keep, g_buf(sc->waters, 1.5));
    zone = keep->n ? g_diff(ring, U(keep)) : ring;
    if (!E(zone)) R_(sm_clear_small(sc->s, zone));
}

/* ================================================================ second features, backdrop, props */
static void place_also(Sc *sc, KVal *plan) {          /* place_also(sc, plan) */
    KVal *al = kv_get(plan, "also"); int a, i;
    for (a = 0; al && a < al->n; a++) {
        KVal *e = al->v[a]; const char *where = KS_(e, "where"), *name = KS_(e, "name"); SmAt *at; SmAim *aim; FtBuildFn build = NULL; void *bctx = NULL; double k, seed;
        relation(sc, (!strcmp(where, "at") || !strcmp(where, "wall") || !strcmp(where, "in") || !strcmp(where, "yard") || !strcmp(where, "front")) ? "far" : where, &at, &aim);
        if (!at) continue;
        subject_builder(name, &sc->pal, &build, &bctx);
        k = ft_size_factor(sc->mm) * KN(KG(e, "size")) * big_of(sc->mm); seed = py_random(sc->mixr);
        for (i = 0; i < 60; i++) {
            double x, y, an; Geo *foot = NULL; SoLayers lay = { 0 }; PyRandom br; int g, mark = ly_mark();
            R_(sm_at_call(at, sc->mixr, &x, &y)); an = py_atan2(sc->q[1] - y, sc->q[0] - x);
            if (py_hypot(x, y) > sc->R * 0.72 && i < 50) continue;
            py_seed_float(&br, seed);
            g = call_build(build, bctx, x, y, an, &br, k * (i < 40 ? 1.0 : 0.8), &foot, &lay);
            if (g == SC_ERR) { char w[300]; snprintf(w, sizeof w, "also %s", name); cp_log(sc, w); lyl_free(&lay); geo_unref(foot); break; }
            if (g == 1 || !foot) { lyl_free(&lay); geo_unref(foot); ly_pop(mark); continue; }
            LAG(foot);
            if (!fits(sc, foot, solid_of(&lay, 0.0), 0, 0)) { lyl_free(&lay); ly_pop(mark); continue; }
            if (place_(sc, foot, &lay, 1.0)) { gl_push(&sc->also, foot); gl_push(&sc->taken, g_buf(foot, 0.6)); }
            lyl_free(&lay);
            break;
        }
    }
}

static const FtTiers RIM_T = { 2, { { { 0.75, NUM_FLOAT, 0 }, { 8, NUM_INT, 0 } }, { { 0.25, NUM_FLOAT, 0 }, { 38, NUM_INT, 0 } } } };
static const FtTiers SC_ROCK_T = { 2, { { { 0.7, NUM_FLOAT, 0 }, { 6, NUM_INT, 0 } }, { { 0.3, NUM_FLOAT, 0 }, { 40, NUM_INT, 0 } } } };          /* scenes.ROCK_T */
static SoLayer *mk_up(Geo *q, double h, const char *kind) { SoLayer *y = ly_new(4); y->v[0] = lv_str("up"); y->v[1] = lv_geom(shp_clone(q->g)); y->v[2] = lv_float(h); y->v[3] = lv_str(kind); return y; }
static SoLayer *mk_mound(Geo *q, double h, const char *kind, const FtTiers *t) {
    SoLayer *y = ly_new(5); y->v[0] = lv_str("mound"); y->v[1] = lv_geom(shp_clone(q->g)); y->v[2] = lv_float(h); y->v[3] = lv_str(kind);
    y->v[4].t = LV_PTR; y->v[4].p = (void *)t; return y;
}
static void rim_dressing(Sc *sc, KVal *plan) {          /* rim_dressing(sc, plan) */
    KVal *rim = kv_get(plan, "rim"), *sub = kv_get(plan, "subject"); double cover, hf, R = sc->R, b[2], behind, boost, ln, t0, t; Scene *s = sc->s;
    int built; PyRandom rr; const char *kind; PlHits avoid; GeoList *av = LL(), *made = LL(); int i; KVal *x;
    if (!truthy(rim)) rim = NULL;
    x = rim ? kv_get(rim, "cover") : NULL; cover = x ? KN(x) : 0.0;
    x = rim ? kv_get(rim, "height") : NULL; hf = x ? KN(x) : 2.0;
    if (!truthy(sub)) sub = NULL;
    if (cover <= 0.02 || sc->mm < 59.9 || !sc->subj || (sub && truthy(kv_get(sub, "edge"))) || (sub && kv_s(sub, "name") && !strcmp(kv_s(sub, "name"), "bridge"))) return;
    py_seed_float(&rr, py_random(sc->mixr)); built = kd_has(&KD.built, sc->style);
    { double c[2] = { sc->c[0], sc->c[1] }, d[2] = { -sc->front[0], -sc->front[1] }; rim_hit(c, d, R, b); } behind = py_atan2(b[1], b[0]);
    { Kind *k = sm_kfix(kind_str(built ? sc->pal.stone : (sc->pal.rock ? sc->pal.rock : sc->pal.stone))); kind = k ? k->s : NULL; }
    boost = sc->mm >= 59.9 ? CPC.big_base_height_factor : 1.0;
    for (i = 0; i < sc->taken.n; i++) if (sc->taken.v[i] && !E(sc->taken.v[i])) gl_push(av, g_buf(sc->taken.v[i], 1.5));
    R_(sm_bigs(s, av));
    if (s->has_objs) gl_extend(av, &s->keepout);
    R_(pl_hits_init(&avoid, av));
    ln = cover * 6.283; t0 = behind - ln / 2 + py_uniform(&rr, -0.1, 0.1); t = t0;
    while (t < t0 + ln) {
        double prof = 0.4 + 0.6 * ppow(py_sin(PI * (t - t0) / ln), 0.8); SoLayers lay = { 0 }, l2 = { 0 }; Geo *foot; int j;
        if (built) {
            double bl = py_uniform(&rr, 3.0, 4.8), tc = t + bl / 2 / R, h, p0[2], p1[2]; Geo *q;
            t += (bl + 0.12) / R;
            if (py_random(&rr) < 0.1) continue;
            q = rect_((R - 2.1) * py_cos(tc), (R - 2.1) * py_sin(tc), 3.4, bl - 0.15, tc); h = py_uniform(&rr, 1.4, 2.3) * hf * prof;
            lyl_push_own(&lay, mk_up(q, h, kind));
            p0[0] = (R - 4.2) * py_cos(tc); p0[1] = (R - 4.2) * py_sin(tc); p1[0] = (R + 1) * py_cos(tc); p1[1] = (R + 1) * py_sin(tc);
            { SoLayer *y = ly_new(4); y->v[0] = lv_str("tcut"); y->v[1] = lv_geom(so_bar(p0, p1, 0.26)); y->v[2] = lv_int(0); y->v[3] = lv_float(0.25); lyl_push_own(&lay, y); }
            if (py_random(&rr) < 0.45) {
                double w2 = bl * py_uniform(&rr, 0.45, 0.8); Geo *r2 = rect_((R - 5.3) * py_cos(tc), (R - 5.3) * py_sin(tc), 2.6, w2, tc);
                lyl_push_own(&lay, mk_up(r2, pmax(0.6, h * py_uniform(&rr, 0.3, 0.55)), kind));
            }
            if (py_random(&rr) < 0.3) {
                Geo *r3 = rect_((R - 2.1) * py_cos(tc), (R - 2.1) * py_sin(tc), 2.2, bl * 0.55, tc); SoLayer *y = ly_new(5);
                y->v[0] = lv_str("on"); y->v[1] = lv_geom(shp_clone(r3->g)); y->v[2] = lv_int(0); y->v[3] = lv_float(py_uniform(&rr, 0.5, 1.1)); y->v[4] = lv_str(kind);
                lyl_push_own(&lay, y);
            }
        } else {
            double r = py_uniform(&rr, 2.0, 3.8) * pmin(1.35, pmax(1.0, ppow(sc->K, 0.3))), tc = t + r / R, d, asp, h; int64_t nv; Geo *q;
            t += (2 * r + 0.5) / R; d = R - r - 0.9 - py_uniform(&rr, 0, 1.3);
            asp = py_uniform(&rr, 0.7, 0.95); nv = py_randint(&rr, 6, 9);
            q = LAG(lf_stone(d * py_cos(tc), d * py_sin(tc), r, &rr, asp, (int)nv, 0)); h = py_uniform(&rr, 1.3, 2.3) * hf * prof;
            lyl_push_own(&lay, mk_mound(q, h, kind, &RIM_T));
            if (py_random(&rr) < 0.4) {
                double d2 = d - r - py_uniform(&rr, 1.2, 2.0), r2 = r * py_uniform(&rr, 0.35, 0.55); Geo *q2 = LAG(lf_stone(d2 * py_cos(tc + 0.02), d2 * py_sin(tc + 0.02), r2, &rr, 0.8, 6, 0));
                lyl_push_own(&lay, mk_mound(q2, pmax(0.5, h * 0.3), kind, &SC_ROCK_T));
            }
        }
        if (built) { SoLayers w = { 0 }; R_(so_weather(&lay, &rr, 0.6, &w)); lyl_free(&lay); lay = w; }
        foot = solid_of(&lay, 0.0);
        if (!g_contains(g_ptbuf(0, 0, R - 0.35, 96), foot) && !built) { lyl_free(&lay); continue; }
        if (R_(pl_hits_hit(&avoid, foot->g))) { lyl_free(&lay); continue; }
        taller(&lay, 1.0, boost, &l2); lyl_free(&lay); lay = l2; memset(&l2, 0, sizeof l2);
        if (CPC.max_slenderness > 0) {
            double sl = CPC.max_slenderness;
            for (j = 0; j < lay.n; j++) {
                SoLayer *L = lay.v[j]; const char *tg = ltag(L);
                if ((!strcmp(tg, "up") || !strcmp(tg, "mound")) && lnum(&L->v[2]) >= 1.0) {
                    Geo *g1 = lgeo(L, 1); double len = g_len(g1), v; SoLayer *y;
                    if (len == 0 || boost == 0) cp_raise(SC_ERR);
                    v = pmax(1.0, sl * 4 * area(g1) / len / boost);          /* min(L[2], v): L[2] itself unless v is smaller */
                    y = ly_copy(L);
                    if (v < lnum(&L->v[2])) y->v[2] = lv_float(v);
                    lyl_push_own(&l2, y);
                } else lyl_push(&l2, L);
            }
            lyl_free(&lay); lay = l2;
        }
        if (place_(sc, foot, &lay, 1.0)) gl_push(made, foot);
        lyl_free(&lay);
    }
    pl_hits_free(&avoid);
    if (made->n) {
        for (i = 0; i < made->n; i++) gl_push(&sc->anchors, made->v[i]);
        for (i = 0; i < made->n; i++) gl_push(&sc->taken, g_buf(made->v[i], 0.3));
        gl_clear(&sc->rim); gl_extend(&sc->rim, made); sc->has_rim = 1;
    }
}

double cp_prop_sink = 0.4;          /* settings "prop_sink_mm" (place.c pl_up) */
int cp_story = 1;          /* settings "story_direction": debris lies out from the subject (clutter) */
int cp_cart_on_track = 1;          /* settings.json "cart_on_track": a cart by a track stands on it */
static int cart_on_track(Sc *sc) {          /* the cart on its track: on the rails, along them, clear of everything else */
    double L, u, x, y, a; int t, i;
    if (!sc->line || E(sc->line)) return 0;
    L = g_len(sc->line);
    for (t = 0; t < 24; t++) {
        Geo *foot = NULL, *test; SoLayers lay = { 0 }; int ok = 1;
        u = py_uniform(sc->mixr, 0.2 * L, 0.8 * L); at_(sc->line, u, &x, &y, &a);
        if (x * x + y * y > (sc->R - 4.0) * (sc->R - 4.0)) continue;
        if (ft_track_cart(x, y, a, sc->rng, ft_obj_scale() > 0 ? trk_of(sc).ro * ft_obj_scale() / 3.0 : 1.15, sc->pal.wood ? sc->pal.wood : "wood", sc->pal.iron ? sc->pal.iron : "iron", &foot, &lay) < 0 || !foot) { lyl_free(&lay); continue; }
        test = g_buf(foot, 0.3);
        if (area(LIn(foot, sc->disc)) < 0.995 * area(foot)) ok = 0;
        for (i = 0; ok && i < sc->bigs.n; i++) if (intersects(sc->bigs.v[i], test)) ok = 0;
        for (i = 0; ok && i < sc->taken.n; i++) if (sc->taken.v[i] != sc->pathg && intersects(sc->taken.v[i], test)) ok = 0;
        if (ok) { int sv = sc_feat_push("cart on track"), r = place_(sc, foot, &lay, 1.0); sc_feat_pop(sv); if (r >= 0) gl_push(&sc->taken, test); geo_unref(foot); lyl_free(&lay); return r >= 0; }
        geo_unref(foot); lyl_free(&lay);
    }
    return 0;
}

static int place_props(Sc *sc, KVal *plan) {          /* place_props(sc, plan) */
    KVal *props = KG(plan, "props"); int placed = 0, i, k;
    for (i = 0; i < props->n; i++) {
        KVal *pr = props->v[i]; ScFn *fn = NULL; SmAt *at; SmAim *aim; int n;
        if (!strcmp(KS_(pr, "what"), "cart") && (!strcmp(KS_(pr, "where"), "path") || !strcmp(KS_(pr, "where"), "start")) && cp_cart_on_track && !strcmp(KS_(KG(plan, "path"), "kind"), "track") && sc->line) {
            n = count_(sc, KN(KG(pr, "count"))); for (k = 0; k < n; k++) placed += cart_on_track(sc);          /* the cart stands on its rails */
            continue;
        }
        cp_prop_fn(KS_(pr, "what"), &sc->pal, &fn);
        if (!fn) continue;
        keep_fn(fn);
        relation(sc, KS_(pr, "where"), &at, &aim);
        n = count_(sc, KN(KG(pr, "count")));
        for (k = 0; k < n; k++) if (py_random(sc->mixr) < share()) { int mark = ly_mark(); placed += run_(sc, fn, at, aim); ly_pop(mark); }
    }
    return placed;
}

/* ================================================================ natural clutter */
typedef struct { SmAt *debris, *growth, *foot, *wat, *near_path; } Smp;
static Smp samplers_(Sc *sc) {          /* _samplers(sc) */
    Smp r; GeoList *feet = LL(), *own = LL(), *at = LL(); SmAt *fl, *wat, *lee = NULL, *npath, *inner, *f2[4]; Geo *pg; double w4[4]; int i;
    if (sc->subj) gl_push(feet, sc->subj);
    gl_extend(feet, &sc->also); gl_extend(feet, &sc->anchors); gl_extend(feet, &sc->tall_cells);
    if (sc->focal) gl_push(feet, sc->focal);
    if (sc->subj) gl_push(own, sc->subj);
    gl_extend(own, &sc->also);
    if (sc->focal) gl_push(own, sc->focal);
    gl_extend(at, &sc->anchors); gl_extend(at, &sc->tall_cells);
    {   SmAt *a = own->n ? NEAR(U(own), 0.8, 3.2) : NULL, *b = (sc->anchors.n || sc->tall_cells.n) ? NEAR(U(at), 0.8, 3.0) : NULL;
        r.foot = MIX2(3, a, 1, b); }
    fl = sc->floor_edge ? NEAR(sc->floor_edge, 0.6, 2.2) : NULL;
    wat = !E(sc->waters) ? NEAR(sc->waters, 0.8, 2.6) : NULL;
    pg = sc->pathg ? LIn(sc->pathg, sc->disc) : NULL;
    if (pg && !strcmp(KS_(KG(sc->plan ? (KVal *)sc->plan : NULL, "path"), "kind"), "stream")) wat = MIX2(1, wat, 2, NEAR(pg, 0.6, 2.4));
    if (feet->n) {
        GeoList *c = LL();
        for (i = 0; i < feet->n; i++) if (!sc->pathg || g_dist(feet->v[i], sc->pathg) > 2.0) gl_push(c, feet->v[i]);
        if (!c->n) c = feet;
        lee = sc->subj ? NEARS(U(c), 0.8, 3.0, SM_BACK, sc->c, sc->front) : NEAR(U(c), 0.8, 3.0);
    }
    npath = pg ? NEAR(pg, 0.8, 3.0) : NULL; inner = RIM(SC.R0 * 0.3, SC.R0 - 1.0);
    w4[0] = 60; w4[1] = 22; w4[2] = 12; w4[3] = 6; f2[0] = r.foot; f2[1] = fl; f2[2] = wat; f2[3] = npath;
    r.debris = MIXN(w4, f2, 4); if (!r.debris) r.debris = inner;
    w4[0] = 40; w4[1] = 35; w4[2] = 25; f2[0] = lee; f2[1] = wat; f2[2] = npath;
    { SmAt *g = MIXN(w4, f2, 3); r.growth = NOT(g ? g : inner, sc->pathg, 2.0); }
    r.wat = wat; r.near_path = npath;
    return r;
}

static void tier_features(Sc *sc, KVal *plan, const ScFnList *fns, ScFnList *late) {          /* tier_features(sc, plan, fns) -> late */
    Smp sm = samplers_(sc); int reps = (int)pmax(0, (double)(print_(ppow(sc->K, 0.7)) - 1)), i, r; SmAt *made_at, *a, *b, *c; SmAim *x;
    relation(sc, "at", &a, &x); relation(sc, "wall", &b, &x); c = NULL; if (sc->also.n) relation(sc, "also", &c, &x);
    { double w[3] = { 3, 2, 1 }; SmAt *f[3] = { a, b, c }; made_at = MIXN(w, f, 3); }
    if (!made_at) made_at = sm.foot;
    if (!made_at) made_at = RIM(SC.R0 * 0.3, SC.R0 - 1.0);
    for (i = 0; i < fns->n; i++) {
        ScFn *fn = fns->v[i]; const char *cl = klass(fn), *tm = fn->tag.tmpl ? fn->tag.tmpl : "", *sh = fn->tag.shape ? fn->tag.shape : "", *kd = fn->tag.kind ? fn->tag.kind : "";
        ScFn *alt = NULL;
        if ((!strcmp(tm, "single") || !strcmp(tm, "scatter")) && !strcmp(sh, "rect") && (!strcmp(kd, "crate") || !strcmp(kd, "coffin"))) { cp_prop_fn(!strcmp(tm, "scatter") ? "crates" : "crate", &sc->pal, &alt); if (alt) { keep_fn(alt); fn = alt; } }
        else if (!strcmp(tm, "single") && !strcmp(sh, "disc") && (!strcmp(kd, "wood") || !strcmp(kd, "barrel"))) { cp_prop_fn("keg", &sc->pal, &alt); if (alt) { keep_fn(alt); fn = alt; } }
        else if ((!strcmp(tm, "single") || !strcmp(tm, "scatter")) && !strcmp(kd, "sack")) { cp_prop_fn(!strcmp(tm, "scatter") ? "sacks" : "sack", &sc->pal, &alt); if (alt) { keep_fn(alt); fn = alt; } }
        if (banned(fn, plan) || !strcmp(cl, "abstract") || !strcmp(cl, "bones")) continue;
        if (!strcmp(cl, "late")) { fl_add(late, fn); continue; }
        if (!strcmp(cl, "line")) { if (!sc->pathg && !sc->subj) fl_add(late, fn); continue; }
        for (r = 0; r < 1 + reps; r++) {
            int mark = ly_mark();
            if (r && py_random(sc->mixr) < 0.4) continue;
            if (strcmp(cl, "marks") && py_random(sc->mixr) >= share()) continue;
            if (!strcmp(cl, "made")) { SmAim *aim = (sc->subj && py_random(sc->mixr) < 0.5) ? APAR(HULL(sc->subj), 0.12) : NULL; run_(sc, fn, made_at, aim); }
            else if (!strcmp(cl, "marks")) fl_add(&sc->marks_later, fn);
            else {
                SmAt *s2 = !strcmp(cl, "debris") ? sm.debris : !strcmp(cl, "growth") ? sm.growth : !strcmp(cl, "foot") ? sm.foot : !strcmp(cl, "wat") ? sm.wat : !strcmp(cl, "near_path") ? sm.near_path : NULL;
                run_(sc, fn, s2 ? s2 : sm.debris, NULL);
            }
            ly_pop(mark);
        }
    }
}

static void clutter(Sc *sc, KVal *plan, int has_n, double n_each) {          /* clutter(sc, plan, n_each) */
    By by; Smp sm; KVal *cl = kv_get(plan, "clutter"); int k, i; static const char *CS[2] = { "debris", "growth" };
    by_klass(&by, plan); sm = samplers_(sc);
    for (k = 0; k < 2; k++) {
        const ScFnList *fs = by_get(&by, CS[k]); SmAt *f = k == 0 ? sm.debris : sm.growth; KVal *cv = cl ? kv_get(cl, CS[k]) : NULL; int64_t n;
        double am = k == 0 ? KD.amount_debris : KD.amount_growth;
        if (!fs || !fs->n || !f) continue;
        cp_grow = ft_obj_scale() > 0 && sc->mm >= 39.9 ? pmax(1.0, pmin(2.2, pow(sc->mm / 40.0, 0.7))) : 1.0;          /* true size: rubble chunks, not grit */
        n = print_((!has_n ? am * sc->K * sc->K : n_each) * (cv ? KN(cv) : 1.0) * share() / (cp_grow * cp_grow));
        {   double g0 = cp_grow; uint32_t h = (uint32_t)(sc->mm * 10.0) * 2654435761u ^ (uint32_t)(k + 1) * 0x85ebca6bu ^ (uint32_t)n * 0xc2b2ae35u;
            for (i = 0; i < n; i++) {
                int64_t c; int mark = ly_mark(); double u;
                /* mixed sizes (plan B step 4, 2026-11-21): about half as made, a third up to 1.4 times, the rest up to 1.8
                   times (grown only where it fits); its own numbers, so the layout's choices are the same as before */
                h ^= h >> 16; h *= 0x7feb352du; h ^= h >> 15; h *= 0x846ca68bu; h ^= h >> 16; u = (h & 0xffffff) / 16777216.0;
                cp_grow = g0 * (u < 0.5 ? 1.0 : u < 0.85 ? 1.15 + 0.25 * (u - 0.5) / 0.35 : 1.45 + 0.35 * (u - 0.85) / 0.15);
                if (py_choice_e(sc->mixr, fs->n, &c) != 0) cp_raise(SC_ERR);
                /* one story direction (plan B2, 2026-11-22): with a subject on the base (the wreck, the ruin, the crater),
                   debris lies along lines out from it, as if thrown or fallen from it; growth stays as it grows */
                run_(sc, fs->v[c], f, k == 0 && sc->subj && !E(sc->subj) && cp_story ? ATO(sc->c[0], sc->c[1], 0.35) : NULL); ly_pop(mark);
            }
        }
        cp_grow = 1.0;
    }
    {   const ScFnList *m = by_get(&by, "marks");
        sc->mark_pool.n = 0; sc->has_mark_pool = 1;
        for (i = 0; m && i < m->n; i++) fl_add(&sc->mark_pool, m->v[i]); }
    by_free(&by);
}

static void marks(Sc *sc, KVal *plan, int has_n, double n_each) {          /* marks(sc, plan, n_each) */
    Smp sm = samplers_(sc); ScFnList todo = { 0 }; KVal *cl = kv_get(plan, "clutter"), *cm = cl ? kv_get(cl, "marks") : NULL; int64_t n; int i; SmAt *anyw;
    n = print_((!has_n ? KD.amount_marks * sc->K * sc->K : n_each) * (cm ? KN(cm) : 1.0));
    for (i = 0; i < sc->marks_later.n; i++) fl_add(&todo, sc->marks_later.v[i]);
    if (sc->has_mark_pool && sc->mark_pool.n) for (i = 0; i < n; i++) { int64_t c; if (py_choice_e(sc->mixr, sc->mark_pool.n, &c) != 0) cp_raise(SC_ERR); fl_add(&todo, sc->mark_pool.v[c]); }
    anyw = RIM(SC.R0 * 0.18, SC.R0 - 1.0);
    for (i = 0; i < todo.n; i++) {
        ScFn *fn = todo.v[i]; const char *kd = fn->tag.kind ? fn->tag.kind : "", *tm = fn->tag.tmpl ? fn->tag.tmpl : "", *sh = fn->tag.shape ? fn->tag.shape : ""; int mark = ly_mark();
        if (!strcmp(tm, "trail")) {
            if (sm.near_path) { SmAim *a = APAR(sc->pathg, 0.15); run_(sc, fn, sm.near_path, a); }
            else if (sc->subj) { SmAt *a; SmAim *x; relation(sc, "front", &a, &x); run_(sc, fn, a, ATO(sc->c[0], sc->c[1], 0.15)); }
            else run_(sc, fn, anyw, NULL);
        } else if (kd_has(KD.wetk, kd)) { double w[3] = { 2, 2, 1 }; SmAt *f[3] = { sm.near_path, sm.wat, sm.foot }, *m = MIXN(w, f, 3); run_(sc, fn, m ? m : anyw, NULL); }
        else if (!strcmp(kd, "crack") || !strcmp(kd, "glyph") || !strcmp(kd, "seam") || !strcmp(kd, "mudcrack") || !strcmp(sh, "wig") || !strcmp(sh, "star")) run_(sc, fn, anyw, NULL);
        else { SmAt *m = MIX2(3, sm.foot, 1, anyw); run_(sc, fn, m ? m : anyw, NULL); }
        ly_pop(mark);
    }
    free(todo.v);
}

/* ================================================================ the scene */
static int lay_bridge(Sc *sc, KVal *plan) {          /* lay_bridge(sc, plan) */
    double R = sc->R, th, off, q[2], phi, d[2], n[2], bow, sa[2], sb[2], ctrl[2], ca[2], cb[2], bend, sw; KVal *sub = KG(plan, "subject"), *pth = KG(plan, "path");
    PyRandom rr; Geo *sline, *sg, *cline; const char *kind; PCall pc;
    th = LF_RADIANS(KN(KG(sub, "angle"))); off = pmin(0.25, KN(KG(sub, "offset"))) * R; q[0] = off * py_cos(th); q[1] = off * py_sin(th);
    phi = LF_RADIANS(KN(KG(pth, "from_angle"))); d[0] = py_cos(phi); d[1] = py_sin(phi); n[0] = -d[1]; n[1] = d[0];
    py_seed_float(&rr, py_random(sc->mixr));
    bow = py_uniform(&rr, -0.25, 0.25) * R;
    { double mn[2] = { -n[0], -n[1] }; rim_hit(q, n, R + 2, sa); rim_hit(q, mn, R + 2, sb); }
    ctrl[0] = 2 * q[0] - (sa[0] + sb[0]) / 2 + d[0] * bow; ctrl[1] = 2 * q[1] - (sa[1] + sb[1]) / 2 + d[1] * bow;
    sline = curve_(sa, ctrl, sb);
    sg = P_stream(sc, sline, &rr);
    if (!sg || E(sg)) return 0;
    { Geo *m = g_interp(sline, g_len(sline) / 2); q[0] = g_x(m); q[1] = g_y(m); }
    { double md[2] = { -d[0], -d[1] }; rim_hit(q, d, R + 2, ca); rim_hit(q, md, R + 2, cb); }
    bend = KN(KG(pth, "bend")) * 0.6;
    ctrl[0] = 2 * q[0] - (ca[0] + cb[0]) / 2 + n[0] * bend * R; ctrl[1] = 2 * q[1] - (ca[1] + cb[1]) / 2 + n[1] * bend * R;
    cline = curve_(ca, ctrl, cb);
    kind = KS_(pth, "kind"); sw = area(LIn(sg, g_ptbuf(q[0], q[1], 7, 16))) / 14 + 1.0;
    if (strcmp(kind, "stones")) {
        double x, y, a, xy[4]; Geo *deck;
        at_(cline, g_len(cline) / 2, &x, &y, &a);
        xy[0] = x - (sw + 1.6) * py_cos(a); xy[1] = y - (sw + 1.6) * py_sin(a); xy[2] = x + (sw + 1.6) * py_cos(a); xy[3] = y + (sw + 1.6) * py_sin(a);
        deck = g_line(xy, 2);
        P_boards(sc, deck, &rr); set_geo(&sc->core, linebuf_flat(deck, 2.1));
    }
    pc.sc = sc; pc.f = path_fn(kind); pc.line = cline; pc.rng = &rr; pc.g = NULL;
    if (cp_try(pcall_, &pc) < 0) { char w[200]; snprintf(w, sizeof w, "bridge way %s", kind); cp_log(sc, w); }
    else if (pc.g) LAG(pc.g);
    set_geo(&sc->subj, g_ptbuf(q[0], q[1], sw + 2.2, 24)); set_geo(&sc->solid, g_empty()); set_geo(&sc->core, NULL);
    sc->c[0] = q[0]; sc->c[1] = q[1]; sc->front[0] = d[0]; sc->front[1] = d[1];
    sc->p1[0] = q[0] + d[0] * (sw + 2.2); sc->p1[1] = q[1] + d[1] * (sw + 2.2); sc->has_q = 1; sc->q[0] = sc->p1[0]; sc->q[1] = sc->p1[1];
    sc->has_start = 1; sc->start[0] = ca[0]; sc->start[1] = ca[1]; set_geo(&sc->line, cline);
    { GeoList *l = LL(); gl_push(l, linebuf_flat(sline, path_w("stream") + 0.6)); gl_push(l, linebuf_flat(cline, path_w(kind))); set_geo(&sc->pathg, LIn(U(l), sc->wide)); }
    gl_push(&sc->taken, sc->pathg);
    { KVal *o = kv_get(plan, "open"); kv_set(plan, "open", kv_float((o ? KN(o) : 1.0) * 0.7)); }
    return 1;
}

static int PAL_OWN;                       /* sc.pal is compose's own copy (else PAL[style] itself, shared) */
static void set_pal(Sc *sc, const KVal *plan) {          /* sc.pal = dict(PAL[sc.style]); sc.pal.update(plan.get("pal") or {}) */
    const KStyle *st = kd_style(sc->style); KVal *p = kv_get(plan, "pal"); int i;
    if (PAL_OWN) kd_pal_free(&sc->pal);
    kd_pal_copy(&sc->pal, &st->pal); PAL_OWN = 1;
    if (truthy(p)) { if (p->t != KV_DICT) cp_raise(SC_ERR); for (i = 0; i < p->n; i++) kd_pal_set(&sc->pal, p->k[i], p->v[i]->t == KV_STR ? p->v[i]->s : NULL); }
}
static int lay_scene(Sc *sc, KVal *plan, int small, int force) {          /* lay_scene(sc, plan, small, force) */
    Scene *s = sc->s; GeoList *wet = LL(), *ll = LL(); int i, j;
    sc->plan = (ScPlan *)plan; set_pal(sc, plan);
    gl_clear(&sc->bigs); R_(sm_bigs(s, &sc->bigs)); gl_clear(&sc->taken);
    for (i = 0; i < s->gcuts.n; i++) {
        Kind *k = s->gcuts.v[i].kind;
        if (!(in_wetk(k) || ksame(k, "pit") || ksame(k, "hollow"))) continue;
        for (j = 0; j < s->gcuts.v[i].polys.n; j++) if (area(s->gcuts.v[i].polys.v[j]) > 6) gl_push(wet, s->gcuts.v[i].polys.v[j]);
    }
    set_geo(&sc->wet, wet->n ? g_uu(wet) : NULL);
    for (i = 0; i < s->gcuts.n; i++) {
        Kind *k = s->gcuts.v[i].kind;
        if (!(ksame(k, "rut") || ksame(k, "wash") || in_wetk(k))) continue;
        for (j = 0; j < s->gcuts.v[i].polys.n; j++) { Geo *p = s->gcuts.v[i].polys.v[j]; double L = g_len(p); if (L > 30 && area(p) / L < 4.0) gl_push(ll, p); }
    }
    set_geo(&sc->lline, ll->n ? g_uu(ll) : NULL);
    if (!strcmp(KS_(KG(plan, "subject"), "name"), "bridge")) { if (small || !lay_bridge(sc, plan)) return 0; }
    else {
        Snap b0 = snap(s); int c0 = s->chops.n; double k = ft_obj_scale() > 0 ? ft_obj_scale() : 1.0;          /* true size: the subject and its path at their real size */
        const char *sn = KS_(KG(plan, "subject"), "name"); double kt = (strstr(sn, "track") || strstr(sn, "rail")) ? 1.6 : 1.0;          /* a mine track: ~0.6 m gauge, 1.1 m sleepers, 18-20 mm across at 1:56 (the classic one is drawn narrow) */
        if (!place_subject(sc, plan, small, force)) return 0;
        grow_since(sc, b0, c0, k * kt);
        b0 = snap(s); c0 = s->chops.n; { KVal *pth = KG(plan, "path"); const char *pk = pth ? KS_(pth, "kind") : ""; Op **was = NULL; int nwas = s->ops.n; if (!strcmp(pk, "track") && ft_obj_scale() > 0) { was = (Op **)malloc(sizeof(Op *) * (size_t)(nwas ? nwas : 1)); memcpy(was, s->ops.v, sizeof(Op *) * (size_t)nwas); { int q; for (q = 0; q < nwas; q++) op_ref(was[q]); } } lay_path(sc, plan); if (strcmp(pk, "track")) grow_since(sc, b0, c0, k); else if (was) track_clear(sc, was, nwas); if (was) { int q; for (q = 0; q < nwas; q++) op_unref(was[q]); } free(was); }          /* (held while the track is laid: a freed old op cannot lend its address to a new one) */          /* a track is drawn at its real size by P_track; the other paths grown */
    }
    hold_yard(sc, plan);
    return 1;
}

/* big outcrops (2026-11-20): on a true-size base from 50 mm, 1 to 3 framing pieces of stone or crystal at the rim, outside the
   model's space and clear of the other standing details (web research: one tall framing piece at 60-80 mm, 15-25 mm high;
   20-35 mm at 90-130 mm; 30-45 mm at 130-170 mm, at one end). Crystal about one time in three where the style has a gem
   material. settings "outcrops": false turns them off. */
int cp_outcrops = 1;
static void outcrops(Sc *sc) {
    Scene *s = sc->s; double mm = sc->mm, R = sc->R; int want, made = 0, t, i, j; PyRandom rr; const ScPal *p = &sc->pal;
    const char *rock = p->rock ? p->rock : p->stone ? p->stone : "rock", *gem = p->gem;
    if (!cp_outcrops || ft_obj_scale() <= 0 || mm < 49.9) return;
    py_seed_float(&rr, py_random(sc->mixr));
    want = mm < 79.9 ? 1 : mm < 129.9 ? (int)py_randint(&rr, 1, 2) : (int)py_randint(&rr, 2, 3);
    for (t = 0; t < 60 && made < want; t++) {
        double size = pmax(7.0, pmin(22.0, 0.13 * mm)) * py_uniform(&rr, 0.85, 1.15) * (made ? 0.8 : 1.0), H;
        double a = cp_mbox ? (py_random(&rr) < 0.5 ? 1.5708 : -1.5708) + py_uniform(&rr, -0.6, 0.6) : py_uniform(&rr, 0, 6.283), rc, cx, cy, room;
        int cr = gem && py_random(&rr) < 0.35, ok = 1, mark = ly_mark();
        room = cp_msp ? R - (cp_mbox ? cp_mhy / pmax(0.3, fabs(sin(a))) : cp_mrad) : R;          /* the strip between the model's space and the rim, this way */
        if (room < 6.0) continue;
        size = pmin(size, 0.52 * (room - 1.2)); rc = R - size * 0.85; cx = rc * cos(a); cy = rc * sin(a);
        H = pmin(0.3 * mm, size * (cr ? py_uniform(&rr, 1.1, 1.5) : py_uniform(&rr, 0.9, 1.3)));          /* stone: as tall as wide; crystal: a little taller, pointed */          /* stone: as tall as wide; crystal: spires */
        Geo *foot = NULL, *solid, *test; SoLayers lay = { 0 }; Num z;
        if (ft_outcrop(cx, cy, a, &rr, size, H, cr, rock, gem ? gem : "crystal", &foot, &lay) < 0 || !foot) { lyl_free(&lay); ly_pop(mark); continue; }
        LAG(foot);
        test = g_buf(foot, 0.6);
        if (area(LIn(foot, sc->disc)) < 0.75 * area(foot)) ok = 0;
        if (ok && cp_msp && intersects(g_buf(foot, 1.0), cp_msp)) ok = 0;
        for (i = 0; ok && i < sc->bigs.n; i++) if (intersects(sc->bigs.v[i], test)) ok = 0;
        for (i = 0; ok && i < s->ops.n; i++) {          /* clear of the other standing details (not the ground) */
            Op *o = s->ops.v[i];
            if (!o->kind || op_ground(s, o) || op_top(o) <= SC.H + 0.2) continue;          /* low pieces too (a grate, planks): an outcrop on them cuts in */
            for (j = 0; j < o->polys.n; j++) if (intersects(o->polys.v[j], test)) { ok = 0; break; }
        }
        if (!ok) { lyl_free(&lay); ly_pop(mark); continue; }
        solid = LIn(solid_of(&lay, 0.0), sc->wide);
        if (terr(s)) R_(sm_clear_terrain(s, LIn(g_buf(g_uu2(foot, solid), 0.8), sc->wide)));
        R_(sm_clear_small(s, g_buf(solid, 0.4))); R_(pl_evict(s, solid));
        uncover(sc, LIn(g_uu2(foot, solid), sc->disc));          /* one level under it */
        z = terr(s) ? num_f(H_) : surf(s, LIn(foot, sc->disc));
        { int sv = sc_feat_push(cr ? "outcrop of crystal" : "outcrop of stone"); R_(ft_build(s, z, &lay, S3D)); sc_feat_pop(sv); }
        gl_push(&sc->bigs, g_buf(solid, 0.0));
        lyl_free(&lay); ly_pop(mark); made++;
    }
}
static int small_scene(Sc *sc, KVal *plan) {          /* small_scene(sc, plan) */
    Scene *s = sc->s; KVal *pl = KG(plan, "props"); int np = pl->n, i, nall = 0, placed = 0, want; int64_t *idx = (int64_t *)LM(sizeof(int64_t) * (size_t)(np + 1)); KVal **all; Geo *line; double R = sc->R;
    SmAt *ring = KA(sm_ring(R));
    for (i = 0; i < np; i++) idx[i] = i;
    py_shuffle_i64(sc->mixr, idx, np);
    want = (int)pmax(1, (double)print_((1 + sc->t) * sc->K * sc->K * share()));
    line = !E(sc->lines) ? sc->lines : NULL;
    all = (KVal **)LM(sizeof(KVal *) * (size_t)(2 * np + 1));
    for (i = 0; i < np; i++) all[nall++] = pl->v[idx[i]];
    for (i = 0; i < np; i++) if (KN(KG(pl->v[idx[i]], "count")) > 1) all[nall++] = pl->v[idx[i]];
    for (i = 0; i < nall; i++) {
        KVal *pr = all[i]; ScFn *fn = NULL; SmIdSet before; int ok, j, mark = ly_mark(); const char *where = KS_(pr, "where");
        if (placed >= want) break;
        cp_prop_fn(KS_(pr, "what"), &sc->pal, &fn);
        if (!fn) continue;
        keep_fn(fn);
        memset(&before, 0, sizeof before); for (j = 0; j < s->feet.n; j++) sm_ids_add(&before, s->feet.v[j]);
        if (!sc->focal) ok = run_(sc, fn, ring, NULL);
        else if ((!strcmp(where, "path") || !strcmp(where, "start")) && line) ok = run_(sc, fn, NEAR(line, 1.0, 3.5), APAR(line, 0.4));
        else {
            SmAt *a = NEAR(sc->focal, 1.0, 4.0), *b = sc->anchors.n ? NEAR(U(&sc->anchors), 0.8, 3.0) : NULL, *m = MIX2(3, a, 1, b);
            SmAim *aim = (!strcmp(where, "wall") || !strcmp(where, "back")) ? APAR(HULL(sc->focal), 0.3) : NULL;
            ok = run_(sc, fn, m, aim);
        }
        placed += ok;
        if (ok && !sc->focal) {
            GeoList *nw = LL(); double *as;
            for (j = 0; j < s->feet.n; j++) if (!sm_ids_has(&before, s->feet.v[j])) gl_push(nw, s->feet.v[j]);
            as = (double *)LM(sizeof(double) * (size_t)(nw->n + 1)); for (j = 0; j < nw->n; j++) as[j] = area(nw->v[j]);
            if (nw->n && px_sum(as, nw->n) >= 6) set_geo(&sc->focal, HULL(g_uu(nw)));
        }
        sm_ids_free(&before);
        ly_pop(mark);
    }
    return placed;
}

static const ScFnList *NEW20;             /* style_details.NEW20[style] */
static int scatter_made(Sc *sc, KVal *plan) {          /* style_details.scatter_made(sc, plan) */
    ScFnList fs = { 0 }; int i, n, placed = 0; int64_t *idx; static const char *SPOT[6] = { "at", "wall", "path", "far", "also", "back" }; static const double SW[6] = { 3, 2, 2, 2, 1, 1 };
    for (i = 0; NEW20 && i < NEW20->n; i++) {
        ScFn *f = NEW20->v[i];
        if (!f->tag.cls) cp_raise(SC_ERR);          /* f.cls: AttributeError */
        if (!strcmp(f->tag.cls, "made") && !banned(f, plan)) fl_add(&fs, f);
    }
    if (!fs.n || !CPC.style_details) { free(fs.v); return 0; }
    n = (int)pmax(1, (double)print_(CPC.style_details_amount * sc->K * sc->K * share()));
    idx = (int64_t *)LM(sizeof(int64_t) * (size_t)fs.n); for (i = 0; i < fs.n; i++) idx[i] = i;
    py_shuffle_i64(sc->mixr, idx, fs.n);
    { ScFn **v = (ScFn **)LM(sizeof(ScFn *) * (size_t)fs.n); for (i = 0; i < fs.n; i++) v[i] = fs.v[idx[i]]; memcpy(fs.v, v, sizeof(ScFn *) * (size_t)fs.n); }
    for (i = 0; i < n * 2; i++) {
        ScFn *fn; SmAt *at; SmAim *aim = NULL; int mark = ly_mark();
        if (placed >= n) break;
        fn = fs.v[i % fs.n];
        if (sc->subj) { int64_t c; if (py_choices_w(sc->mixr, 6, SW, 1, &c) != 0) cp_raise(SC_ERR); relation(sc, SPOT[c], &at, &aim); }
        else { Geo *foc = sc->focal; at = (foc && py_random(sc->mixr) < 0.6) ? NEAR(foc, 1.0, 4.5) : RIM(SC.R0 * 0.25, SC.R0 - 1.0); }
        placed += run_(sc, fn, at, aim) ? 1 : 0;
        ly_pop(mark);
    }
    free(fs.v);
    return placed;
}
typedef struct { Sc *sc; KVal *plan; } SmArg;
static void style_made_(void *p) { SmArg *a = (SmArg *)p; scatter_made(a->sc, a->plan); }
static void style_made(Sc *sc, KVal *plan) { SmArg a; a.sc = sc; a.plan = plan; if (cp_try(style_made_, &a) < 0) cp_log(sc, "style details"); }

/* ================================================================ compose */
typedef struct { Sc *sc; KVal *plan; int marks_only, cracks; int r; } NbArg;
static void nb_(void *p) { NbArg *a = (NbArg *)p; a->r = no_bare(a->sc, a->plan, a->marks_only, a->cracks); }
static void ce_(void *p) { clear_edge((Sc *)p); }
typedef struct { Sc *sc; } GbArg;
static void gb_(void *p) {          /* ground_big.apply(sc) */
    Sc *sc = ((GbArg *)p)->sc; LyGbSc g; int r, i;
    memset(&g, 0, sizeof g);
    g.s = sc->s; g.style = sc->style; g.wide = sc->wide; g.core = sc->core; g.final_mm = env_final_or(0); g.true_scale = ft_true_scale();
    g.salt = CPC.salt ? CPC.salt : ""; g.reseed = CPC.reseed ? CPC.reseed : "";
    g.stand_body = CO->stand.has_stand ? CO->stand.body : NULL;
    if (CO->stand.has_stand) for (i = 0; i < CO->stand.pads.n; i++) if (CO->stand.pads.v[i]) gl_push(&g.stand_pads, CO->stand.pads.v[i]);
    r = ly_ground_big(&g);
    for (i = 0; i < g.nlog; i++) cp_log(sc, "ground_big feature");
    gl_free(&g.stand_pads);
    if (r < 0) cp_raise(SC_ERR);
}
typedef struct { const char *pj; const char *style; double mm; KVal *plans; char **src; KVal *notes; char err[600]; } PjArg;
static void pj_(void *p) {          /* the PLAN_JSON block's try */
    PjArg *a = (PjArg *)p; KVal *raw = NULL, *pl = NULL, *nt = kv_list(); FILE *f = fopen(a->pj, "rb");
    if (f) {
        long n; char *b; fseek(f, 0, SEEK_END); n = ftell(f); fseek(f, 0, SEEK_SET); b = (char *)LM((size_t)n + 1);
        if (fread(b, 1, (size_t)n, f) != (size_t)n) { fclose(f); cp_raise(SC_ERR); }
        fclose(f); b[n] = 0;
        raw = kv_parse_json_e(b, a->err, sizeof a->err);
    } else raw = kv_parse_json_e(a->pj, a->err, sizeof a->err);
    if (!raw) cp_raise(SC_ERR);
    RX(cp_validate_plan(raw, a->style, a->mm, NULL, 0, &pl, nt));
    if (pl) {
        KVal *s = kv_get(raw, "_source"), *n0 = kv_get(raw, "_notes"); char b[400]; int i;
        kv_push(a->plans, pl);
        if (s) pystr(s, b, sizeof b); else snprintf(b, sizeof b, "file");
        free(*a->src); *a->src = _strdup(b);
        while (a->notes->n) kv_free(a->notes->v[--a->notes->n]);
        if (n0 && n0->t == KV_LIST) for (i = 0; i < n0->n; i++) kv_push(a->notes, kv_copy(n0->v[i]));
        for (i = 0; i < nt->n; i++) kv_push(a->notes, kv_copy(nt->v[i]));
    } else {
        char b[2400]; size_t k = 0; int i;
        k += (size_t)snprintf(b + k, sizeof b - k, "given plan refused: ");
        for (i = 0; i < nt->n && k < sizeof b; i++) k += (size_t)snprintf(b + k, sizeof b - k, "%s%s", i ? "; " : "", nt->v[i]->s);
        while (a->notes->n) kv_free(a->notes->v[--a->notes->n]);
        kv_push(a->notes, kv_str(b));
    }
    kv_free(nt); kv_free(raw);
}

void cp_out_free(CpOut *o) {
    free(o->layout_facts); free(o->theme); free(o->plan_source); free(o->plan_line);
    kv_free(o->plan); kv_free(o->plan_notes); kv_free(o->scene_log);
    gl_free(&o->stand.pads); geo_unref(o->stand.body); ft_s3d_free(&o->s3d);
    memset(o, 0, sizeof *o);
}

static int compose_body(Scene *s, const char *style, PyRandom *mixr, PyRandom *rng, int t, const ScFnList *fns, CpOut *out, Sc *sc) {
    double mm; int64_t n; int big, given, scene = 0, i; double ts; KVal *plans = kv_list(), *plan = NULL, *notes = kv_list(); char *src = _strdup("table"); ScFnList late = { 0 };
    const KStyle *st = kd_style(style);
    if (!st) cp_raise(SC_ERR);          /* PAL[style]: KeyError */
    RX(sm_sc_init(sc, s, style, mixr, rng, t, &st->pal, CPC.kit_mini && *CPC.kit_mini ? CPC.kit_mini : CPC.miniature ? CPC.miniature : "biped") == SC_ERR ? CP_EXIT : 0);
    mm = sc->mm; n = num_of(s->name); big = mm >= 59.9;
    read_layout(sc); out->layout_facts = layout_facts(sc);
    if (env_or(CPC.plan_json)) {
        PjArg a; a.pj = CPC.plan_json; a.style = style; a.mm = mm; a.plans = plans; a.src = &src; a.notes = notes; a.err[0] = 0;
        if (cp_try(pj_, &a) < 0) {          /* f"given plan unreadable: {type(e).__name__} {e}" */
            char b[800]; snprintf(b, sizeof b, "given plan unreadable: %s %s", a.err[0] ? "JSONDecodeError" : "Exception", a.err);
            while (notes->n) kv_free(notes->v[--notes->n]); kv_push(notes, kv_str(b));
        }
    }
    if (!plans->n && !env_or(CPC.plan_json) && mm >= 59.9 && !env_or(CPC.kit_worker) && !env_or(CPC.theme)) {
        if (CPC.designer_enabled) { while (notes->n) kv_free(notes->v[--notes->n]); kv_push(notes, kv_str("designer failed (no usable plan)")); }
    }
    given = plans->n;
    RX(cp_pick_plans(style, mixr, mm, (int)n, plans));
    ts = ft_true_scale();
    cp_msp = NULL;          /* none unless this base is true size from 25 mm (never left from the last base) */
    if (ft_obj_scale() > 0 && mm >= 24.9) {          /* true size (2026-11-20; every size from 25 mm since 11-21: a cart stood where the figure goes): the model's space marked before the scene, so its subject and props keep out of it */
        double mr, hx, hy; int bx = ft_model_space(mm, sc->R, CPC.kit_mini, cp_model_room, &mr, &hx, &hy); Geo *clear = bx ? g_box(-hx, -hy, hx, hy) : g_ptbuf(0, 0, mr, 24); GeoList nk = { 0 };
        (void)nk; gl_push(&sc->taken, clear);          /* the subject steers clear of it; the clip (keepout) comes after the scene, so a track runs right across */
        cp_msp = LIn(clear, sc->disc); cp_mbox = bx; cp_mhx = hx; cp_mhy = hy; cp_mrad = mr;
    }
    if ((big || mm >= 39.9 || (ft_obj_scale() > 0 && mm >= 24.9)) && (!ft_figure() || ft_obj_scale() > 0)) {          /* the theme's scene (paths, subject, props); not with the old --scale, where the model is the subject */
        int np = big ? plans->n : (plans->n ? 1 : 0);
        for (i = 0; i < np; i++) {
            if (lay_scene(sc, plans->v[i], !big, 0)) { plan = plans->v[i]; scene = 1; break; }
            if (i < given) { kv_push(notes, kv_str("no room for the given plan's subject")); free(src); src = _strdup("table"); }
        }
        if (!plan && big) {
            for (i = given; i < plans->n; i++) if (lay_scene(sc, plans->v[i], 0, 1)) { plan = plans->v[i]; scene = 1; kv_push(notes, kv_str("big pieces cut back to make room")); break; }
        }
    }
    if (!plan && plans->n) {
        plan = !big ? plans->v[0] : plans->v[given < plans->n - 1 ? given : plans->n - 1];
        if (big) kv_push(notes, kv_str("no theme's subject found room: props only"));
    }
    if (!plan) {
        KVal *pth = kv_dict(), *sub = kv_dict();
        plan = kv_dict(); kv_set(plan, "theme", kv_str("")); kv_set(plan, "props", kv_list()); kv_set(plan, "clutter", kv_dict());
        kv_set(pth, "kind", kv_str("none")); kv_set(plan, "path", pth); kv_set(sub, "name", kv_str("")); kv_set(plan, "subject", sub);
        kv_push(plans, plan);
    }
    sc->plan = (ScPlan *)plan;
    if (scene && cp_msp) {          /* true size: from here on the model's space keeps details out (the scene's track and subject are laid) */
        GeoList nk = { 0 }; if (s->has_objs) gl_copy(&nk, &s->keepout); gl_push(&nk, cp_msp); gl_free(&s->keepout); s->keepout = nk; s->has_objs = 1;
    }
    if (scene) {
        place_also(sc, plan); { int sv = sc_feat_push("rim backdrop"); rim_dressing(sc, plan); sc_feat_pop(sv); } place_props(sc, plan);          /* the backdrop: one detail, however many blocks */
        style_made(sc, plan);
        tier_features(sc, plan, fns, &late);
        if (cp_msp) {          /* true size: the model's space cleared and level, the outcrops round it */
            grow_pebbles(sc, pmax(1.0, pmin(2.2, pow(sc->mm / 40.0, 0.7))));
            drop_tall_in(sc, cp_msp, cp_mbox, cp_mhx, cp_mhy, cp_mrad); if (terr(s)) R_(sm_clear_terrain(s, cp_msp)); uncover(sc, cp_msp); clear_cuts(sc, cp_msp);
            outcrops(sc);
        }
        clutter(sc, plan, !big, (1 + t) * sc->K * sc->K);
    } else {
        Geo *msp = NULL; int mbox = 0; double mrad = 0, mhx = 0, mhy = 0; cp_msp = NULL;          /* the model's space (true size from 40 mm): one level, cut clear of plates and steps */
        set_pal(sc, plan);
        if (!CO->stand.has_stand) {
            double c[2];
            if (!ft_figure()) { c[0] = py_uniform(mixr, -0.08, 0.08) * sc->R; c[1] = py_uniform(mixr, -0.08, 0.08) * sc->R; } else { c[0] = 0.0; c[1] = 0.0; }
            hold_stand(sc, plan, c, 1, py_uniform(mixr, 0, 6.28));
        }
        if (ft_figure()) {
            double rv = pmin(sc->R, env_final_or(2 * sc->R * ts) / (2 * ts)), mr, hx, hy; Geo *clear;
            if (ft_obj_scale() <= 0) clear = g_ptbuf(0, 0, CPC.clear_middle * rv, 24);          /* KIT_SCALE: the middle 45 % */
            else if (ft_model_space(sc->mm, rv, CPC.kit_mini, cp_model_room, &mr, &hx, &hy)) clear = g_box(-hx, -hy, hx, hy);
            else clear = g_ptbuf(0, 0, mr, 24);
            if (ft_obj_scale() > 0 && sc->mm >= 24.9) { msp = clear; mbox = ft_model_space(sc->mm, rv, CPC.kit_mini, cp_model_room, &mrad, &mhx, &mhy); }
            GeoList nk = { 0 };
            if (s->has_objs) gl_copy(&nk, &s->keepout);
            gl_push(&nk, clear); gl_free(&s->keepout); s->keepout = nk;
            if (!s->has_objs) { s->has_objs = 1; }
            gl_push(&sc->taken, clear);
            if (env_final_or(0) >= 49.9) { GbArg g; g.sc = sc; if (cp_try(gb_, &g) < 0) cp_log(sc, "ground_big"); }
        }
        small_scene(sc, plan);
        style_made(sc, plan);
        tier_features(sc, plan, fns, &late);
        if (ft_obj_scale() > 0 && sc->mm >= 39.9) grow_pebbles(sc, pmax(1.0, pmin(2.2, pow(sc->mm / 40.0, 0.7))));
        if (msp) { cp_msp = LIn(msp, sc->disc); cp_mbox = mbox; cp_mhx = mhx; cp_mhy = mhy; cp_mrad = mrad; }
        if (msp) { Geo *g = LIn(msp, sc->disc); drop_tall_in(sc, g, mbox, mhx, mhy, mrad); if (terr(s)) R_(sm_clear_terrain(s, g)); uncover(sc, g); clear_cuts(sc, g); }
        outcrops(sc);          /* the framing pieces: big stone or crystal at the rim */          /* a tank sits level: no split level, no plate edge under it */
        clutter(sc, plan, 1, (1 + t) * sc->K * sc->K);
    }
    {   int has_n = !(scene && big); NbArg a; double kgrow = ft_obj_scale() > 0 && sc->mm >= 39.9 ? pmax(1.0, pmin(2.2, pow(sc->mm / 40.0, 0.7))) : 1.0;
        cp_grow = kgrow;          /* true size on a big base: marks and the bare-ground fill larger too, fewer of them */
        marks(sc, plan, has_n, (1 + t) * sc->K * sc->K * 0.6 / (kgrow * kgrow));
        a.sc = sc; a.plan = plan; a.marks_only = 0; a.cracks = 0;
        if (cp_try(nb_, &a) < 0) cp_log(sc, "no_bare");
        if (cp_msp) { drop_tall_in(sc, cp_msp, cp_mbox, cp_mhx, cp_mhy, cp_mrad); clear_cuts(sc, cp_msp); }          /* again: the details and marks laid after the first clearing (style details, fill) */
        cp_release(s);
        a.marks_only = 1; if (cp_try(nb_, &a) < 0) cp_log(sc, "no_bare");
        cp_grow = 1.0;
        for (i = 0; i < late.n; i++) {
            int r = call_fn(late.v[i], s, rng, t);
            if (r < 0) cp_log(sc, late.v[i]->qualname && *late.v[i]->qualname ? late.v[i]->qualname : "late");
        }
        if (cp_try(ce_, sc) < 0) cp_log(sc, "clear_edge");
        a.marks_only = 0; a.cracks = 1; if (cp_try(nb_, &a) < 0) cp_log(sc, "no_bare");
        if (cp_small_cap) small_cap(sc);          /* G1 part 2: the small-base cap (off by default) */
        if (cp_feet_pad && cp_msp) feet_pad(sc, pad_shape(sc));          /* G2: the feet pad, last, after every fill (off by default) */
    }
    free(late.v);
    for (i = 0; i < s->ops.n; i++) kmap_setdefault(&KD.col, s->ops.v[i]->kind ? s->ops.v[i]->kind->s : "None", "#8a8a8a");
    kv_set(plan, "miniature", kv_str(sc->mini));
    {   const char *th = kv_s(plan, "theme"); KVal *thv = kv_get(plan, "theme"); char tb[512];
        if (thv && thv->t != KV_STR) { pystr(thv, tb, sizeof tb); th = tb; }
        out->theme = _strdup(th ? th : "");
        out->plan = kv_copy(plan);
        if (!scene) { kv_set(out->plan, "subject", kv_none()); kv_set(out->plan, "path", kv_none()); kv_set(out->plan, "also", kv_list()); }          /* dict(plan, subject=None, path=None, also=[]) */
        out->plan_source = src; src = NULL; out->plan_notes = kv_copy(notes);
        out->scene_log = kv_list(); for (i = 0; i < sc->log.n; i++) kv_push(out->scene_log, kv_str(sc->log.v[i]));
        {   /* plan_line */
            char b[4096]; size_t k = 0; int nf = 0, nx = 0, first_fail = -1; char fx[2048]; size_t kx = 0;
            for (i = 0; i < notes->n; i++) {
                const char *x = notes->v[i]->s;
                int fail = strstr(x, "failed") || strstr(x, "refused") || strstr(x, "unreadable") || strstr(x, "no room for the given");
                if (fail) { if (first_fail < 0) first_fail = i; nf++; }
                else { kx += (size_t)snprintf(fx + kx, kx < sizeof fx ? sizeof fx - kx : 0, "%s%s", nx ? "; " : "", x); nx++; }
            }
            k += (size_t)snprintf(b + k, sizeof b - k, "plan: ");
            if (nf) k += (size_t)snprintf(b + k, sizeof b - k, "%s, ", notes->v[first_fail]->s);
            k += (size_t)snprintf(b + k, sizeof b - k, "%s \"%s\"", out->plan_source, out->theme);
            if (nx && !strncmp(out->plan_source, "designer", 8)) k += (size_t)snprintf(b + k, sizeof b - k, " (%d fix%s)", nx, nx != 1 ? "es" : "");
            else if (nx) { utf8_cut(fx, 160); k += (size_t)snprintf(b + k, sizeof b - k, " (%s)", fx); }
            out->plan_line = _strdup(b);
        }
    }
    kv_free(notes); kv_free(plans); free(src);
    return 0;
}
typedef struct { Scene *s; const char *style; PyRandom *mixr, *rng; int t; const ScFnList *fns; CpOut *out; Sc *sc; int r; } CArg;
static void compose_(void *p) { CArg *a = (CArg *)p; a->r = compose_body(a->s, a->style, a->mixr, a->rng, a->t, a->fns, a->out, a->sc); }
int cp_compose(Scene *s, const char *style, PyRandom *mixr, PyRandom *rng, int t, const ScFnList *fns, const CpPools *pools, CpOut *out) {
    Sc sc; CArg a; jmp_buf jb, *old = ly_jb; int r;
    memset(&sc, 0, sizeof sc); memset(out, 0, sizeof *out);
    CO = out; DETAILS = pools ? pools->details : NULL; NEW20 = pools ? pools->new20 : NULL; S3D = &out->s3d; PAL_OWN = 0;
    ly_arena_begin();
    a.s = s; a.style = style; a.mixr = mixr; a.rng = rng; a.t = t; a.fns = fns; a.out = out; a.sc = &sc; a.r = 0;
    ly_jb = &jb; cp_exc = SC_ERR;
    if (setjmp(jb) == 0) { compose_(&a); r = a.r; }
    else r = cp_exc;
    ly_jb = old;
    lf_focus = NULL; lf_aimfn = NULL; PL.hboost = 1.0;
    release_samplers();
    if (PAL_OWN) kd_pal_free(&sc.pal);
    PAL_OWN = 0;
    sm_sc_free(&sc);
    ly_arena_end();
    cp_fnl_free(&FNKEEP);
    return r;
}
