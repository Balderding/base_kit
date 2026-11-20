/* stylefns.c: the detail-function lists gg3.make reads (see stylefns.h). Each part names the Python it copies. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "stylefns.h"
#include "bld_details.h"
#include "bld_gg3.h"
#include "bld_ns.h"
#include "bld_features.h"
#include "shp.h"

FtS3dList *sf_s3d;
int sf_new_details = 1, sf_style_details = 1;
int sf_nmissing; char sf_missing_text[4096];
static FtS3dList S3D_OWN;
static FtS3dList *s3d(void) { return sf_s3d ? sf_s3d : &S3D_OWN; }

static void missing(const char *what) {
    size_t n = strlen(sf_missing_text);
    sf_nmissing++;
    if (n + strlen(what) + 3 < sizeof sf_missing_text) snprintf(sf_missing_text + n, sizeof sf_missing_text - n, "%s%s", n ? "; " : "", what);
}

/* ================================================================ one ScFn per Python function object */
typedef struct { const void *key; int kind; ScFn *f; } Memo;          /* kind: what key points at (a DtFn, a tier fn, an NS entry ...) */
static Memo *MEMO; static int NMEMO, CMEMO;
static ScFn *memo_get(const void *key, int kind) { for (int i = 0; i < NMEMO; i++) if (MEMO[i].key == key && MEMO[i].kind == kind) return MEMO[i].f; return NULL; }
static ScFn *memo_put(const void *key, int kind, ScFn *f) {
    if (NMEMO == CMEMO) { CMEMO = CMEMO ? 2 * CMEMO : 512; MEMO = (Memo *)realloc(MEMO, sizeof(Memo) * (size_t)CMEMO); }
    MEMO[NMEMO].key = key; MEMO[NMEMO].kind = kind; MEMO[NMEMO].f = f; NMEMO++;          /* the memo keeps its reference for good */
    return f;
}
enum { MK_DT = 1, MK_TIER, MK_NEW, MK_SDR, MK_NS };

/* debugging: called before (after = 0) and after (1) every detail function made here (pipeline.c sets it with PIPE_TRACE) */
void (*sf_trace)(const ScFn *f, Scene *s, PyRandom *rng, int after);
#define TRACED(body) do { int r_; if (sf_trace) sf_trace(f, s, rng, 0); r_ = (body); if (sf_trace) sf_trace(f, s, rng, 1); return r_; } while (0)
/* ---- fn(s, rng, lvl) for each kind ---- */
static int run_dt(ScFn *f, Scene *s, PyRandom *rng, int lvl) { TRACED(dt_run((const DtFn *)f->ctx, s, rng, (double)lvl) < 0 ? SC_ERR : 0); }
static int run_tier(ScFn *f, Scene *s, PyRandom *rng, int lvl) { TRACED(((BsTierFn)f->ctx)(s, rng, lvl) < 0 ? SC_ERR : 0); }
/* features.comp(build, r, gap, ground, tries).fn(s, rng, lvl=0, scale=1.0) */
static int run_comp(ScFn *f, Scene *s, PyRandom *rng, int lvl) { (void)lvl; TRACED(ft_comp_run((const FtComp *)f->ctx, s, rng, 1.0, s3d()) < 0 ? SC_ERR : 0); }

/* a DtFn's function object: scatter(...).fn is "scatter.<locals>.fn"; styles2's plain f_* are themselves */
static ScFn *fn_dt(const DtFn *d) {
    ScFn *f = memo_get(d, MK_DT); char q[96];
    if (f) return f;
    if (d->t >= DT_HEADSTONE) snprintf(q, sizeof q, "%s", d->qual); else snprintf(q, sizeof q, "%s.<locals>.fn", d->qual);
    f = cp_fn_new(q, run_dt, (void *)d, NULL);
    f->tag.tmpl = d->tmpl; f->tag.kind = d->kind; f->tag.shape = d->shape; f->tag.cls = NULL;
    return memo_put(d, MK_DT, f);
}
/* a tier function of gg3.py or a style file: the function itself */
static ScFn *fn_tier(const char *name, BsTierFn fn) {
    ScFn *f = memo_get((const void *)fn, MK_TIER);
    if (f) return f;
    f = cp_fn_new(name, run_tier, (void *)fn, NULL);
    return memo_put((const void *)fn, MK_TIER, f);
}
static ScFn *fn_comp(FtComp c, const void *key, int kind) {
    FtComp *cc = (FtComp *)malloc(sizeof *cc); ScFn *f;
    *cc = c;
    f = cp_fn_new("comp.<locals>.fn", run_comp, cc, free);
    f->tag.tmpl = "comp"; f->tag.kind = c.kind; f->tag.shape = c.shape; f->tag.cls = c.cls;
    return memo_put(key, kind, f);
}
/* the classes a style file's feature_tables gives its features (_cls(C(b_x(...)), cls)): FT_NEW does not carry them.
   Only style_blight sets any. */
static const char *new_cls(const FtNewSpec *sp) {
    static const char *const T[][3] = { { "blight", "thorn_cluster", "debris" }, { "blight", "bone_heap", "bones" }, { "blight", "boiling_pool", "marks" },
                                        { "blight", "rune_ring", "marks" }, { "blight", "torn_rift", "marks" }, { "blight", "flesh_rock", "growth" } };
    if (!sp->b.mod) return NULL;
    for (size_t i = 0; i < sizeof T / sizeof T[0]; i++) if (!strcmp(sp->style, T[i][0]) && !strcmp(sp->b.name, T[i][1])) return T[i][2];
    return NULL;
}
/* features.NEW: C(b_x(...), r, gap, ground, tries) */
static ScFn *fn_new(const FtNewSpec *sp) {
    ScFn *f = memo_get(sp, MK_NEW); FtBuildFn b = NULL; void *ctx = NULL; FtComp c; int r;
    if (f) return f;
    r = bld_ft_make(&sp->b, 0, &b, &ctx);
    if (r != 0) { char t[160]; snprintf(t, sizeof t, "features.NEW %s: b_%s (%d)", sp->style, sp->b.name, r); missing(t); return NULL; }
    c = ft_comp(b, ctx, sp->r, sp->gap, sp->ground, sp->tries); c.kind = sp->kind; c.shape = sp->shape; c.cls = new_cls(sp);
    return fn_comp(c, sp, MK_NEW);
}
/* style_details.extend: fn = F.comp(f, r=r, ground=ground); fn.cls = cls; fn.kind = kind; fn.shape = name; fn.sd = True */
static ScFn *fn_sdr(const SdrEntry *e) {
    ScFn *f = memo_get(e, MK_SDR); FtComp c;
    if (f) return f;
    c = ft_comp(sdr_f, (void *)e, e->r, 0.4, e->ground, 60); c.kind = e->kind; c.shape = e->name; c.cls = e->cls; c.sd = 1;
    return fn_comp(c, e, MK_SDR);
}

/* ================================================================ the newer styles' entries (NS_V F nodes) */
static const NsV *kw(const NsV *f, const char *name) { return ns_get(ns_kid(f, 1), name); }          /* a closure variable */
static double nv_d(const NsV *v, double d) { return v && (v->t == 'i' || v->t == 'f' || v->t == 'b') ? v->v : d; }
static Num nv_num(const NsV *v, double d) { if (!v) return num_f(d); return v->t == 'i' ? num_i(v->v) : v->t == 'b' ? (Num){ v->v, NUM_BOOL, 0 } : num_f(v->v); }
static const char *nv_s(const NsV *v) { return v && v->t == 's' ? so_intern(v->s) : NULL; }
static void nv_pair(const NsV *v, double d0, double d1, double *a, double *b) {
    *a = d0; *b = d1;
    if (v && (v->t == 't' || v->t == 'l') && v->n >= 2) { *a = nv_d(ns_kid(v, 0), d0); *b = nv_d(ns_kid(v, 1), d1); }
    else if (v && (v->t == 'i' || v->t == 'f')) *a = *b = v->v;
}
static int nv_tiers(const NsV *v, const double *dflt, int nd, Tier *t) {
    if (!v || (v->t != 't' && v->t != 'l')) { for (int i = 0; i < nd; i++) { t[i].frac = num_f(dflt[2 * i]); t[i].a = num_i(dflt[2 * i + 1]); } return nd; }
    int n = v->n > 4 ? 4 : v->n;
    for (int i = 0; i < n; i++) { const NsV *p = ns_kid(v, i); t[i].frac = nv_num(ns_kid(p, 0), 0); t[i].a = nv_num(ns_kid(p, 1), 0); }
    return n;
}
static const char *attr(const NsV *f, const char *k) { const NsV *v = ns_get(ns_kid(f, 0), k); return v && v->t == 's' ? so_intern(v->s) : NULL; }
/* details.sh_* : the shape factory's arguments (its closure) */
static int nv_shape(const NsV *f, LfShape *sh) {
    static const char *const NM[SH_COUNT] = { "sh_rock", "sh_round", "sh_disc", "sh_ell", "sh_rect", "sh_caps", "sh_bone", "sh_star", "sh_crescent", "sh_hex", "sh_tri", "sh_cross", "sh_ring", "sh_blob", "sh_wig" };
    char nm[64]; const char *q;
    if (!f || f->t != 'F' || !(q = strchr(f->s, ':'))) return 0;
    snprintf(nm, sizeof nm, "%s", q + 1); { char *d = strchr(nm, '.'); if (d) *d = 0; }
    for (int k = 0; k < SH_COUNT; k++) if (!strcmp(NM[k], nm)) {
        double a[5] = { 0 };
        switch (k) {
        case SH_ROCK: { double n0, n1; a[0] = nv_d(kw(f, "r0"), 0); a[1] = nv_d(kw(f, "r1"), 0); nv_pair(kw(f, "nv"), 5, 8, &n0, &n1); a[2] = n0; a[3] = n1; break; }
        case SH_DISC: a[0] = nv_d(kw(f, "r0"), 0); a[1] = nv_d(kw(f, "r1"), 0); a[2] = nv_d(kw(f, "res"), 20); break;
        case SH_ELL: a[0] = nv_d(kw(f, "a0"), 0); a[1] = nv_d(kw(f, "a1"), 0); a[2] = nv_d(kw(f, "b0"), 0); a[3] = nv_d(kw(f, "b1"), 0); break;
        case SH_RECT: a[0] = nv_d(kw(f, "l0"), 0); a[1] = nv_d(kw(f, "l1"), 0); a[2] = nv_d(kw(f, "w0"), 0); a[3] = nv_d(kw(f, "w1"), 0); break;
        case SH_CAPS: case SH_CROSS: a[0] = nv_d(kw(f, "l0"), 0); a[1] = nv_d(kw(f, "l1"), 0); a[2] = nv_d(kw(f, "w"), 0); break;
        case SH_BONE: a[0] = nv_d(kw(f, "l0"), 0); a[1] = nv_d(kw(f, "l1"), 0); break;
        case SH_STAR: a[0] = nv_d(kw(f, "r0"), 0); a[1] = nv_d(kw(f, "r1"), 0); a[2] = nv_d(kw(f, "n0"), 0); a[3] = nv_d(kw(f, "n1"), 0); break;
        case SH_RING: a[0] = nv_d(kw(f, "ro0"), 0); a[1] = nv_d(kw(f, "ro1"), 0); a[2] = nv_d(kw(f, "t"), 0); break;
        case SH_WIG: a[0] = nv_d(kw(f, "l0"), 0); a[1] = nv_d(kw(f, "l1"), 0); a[2] = nv_d(kw(f, "w"), 0); a[3] = nv_d(kw(f, "amp"), 0.5); break;
        default: a[0] = nv_d(kw(f, "r0"), 0); a[1] = nv_d(kw(f, "r1"), 0); break;          /* round crescent hex tri blob */
        }
        *sh = lf_sh(k, a[0], a[1], a[2], a[3], a[4]);
        return 1;
    }
    return -1;
}
/* details.pat_*: 1 = an LfPat; 2 = style_blight.pat_brune (not an LfPat); 0 = None; -1 = unknown */
static int nv_pat(const NsV *f, LfPat *pt) {
    static const char *const NM[PAT_COUNT] = { "pat_slots", "pat_seams", "pat_ring", "pat_cross", "pat_dots", "pat_hole", "pat_rune" };
    static const double D0[PAT_COUNT][2] = { { 4, 0.35 }, { 0.9, 0 }, { 0.3, 0 }, { 0, 0 }, { 0.25, 0 }, { 0.5, 0.8 }, { 0, 0 } };
    char nm[64]; const char *q;
    if (!f || f->t == 'N') return 0;
    if (f->t != 'F' || !(q = strchr(f->s, ':'))) return -1;
    if (!strncmp(f->s, "style_blight:pat_brune", 22)) return 2;
    snprintf(nm, sizeof nm, "%s", q + 1); { char *d = strchr(nm, '.'); if (d) *d = 0; }
    for (int k = 0; k < PAT_COUNT; k++) if (!strcmp(NM[k], nm)) {
        pt->kind = k; pt->a[0] = D0[k][0]; pt->a[1] = D0[k][1];
        if (k == PAT_SLOTS) { pt->a[0] = nv_d(kw(f, "n"), 4); pt->a[1] = nv_d(kw(f, "w"), 0.35); }
        else if (k == PAT_SEAMS) pt->a[0] = nv_d(kw(f, "step"), 0.9);
        else if (k == PAT_RING) pt->a[0] = nv_d(kw(f, "t"), 0.3);
        else if (k == PAT_DOTS) pt->a[0] = nv_d(kw(f, "r"), 0.25);
        else if (k == PAT_HOLE) { pt->a[0] = nv_d(kw(f, "r0"), 0.5); pt->a[1] = nv_d(kw(f, "r1"), 0.8); }
        return 1;
    }
    return -1;
}

/* ---- details.single(shape, h, kind, how, top=pat_brune()).fn: the template with style_blight's rune pattern (the one
   pattern that is not details.py's own). A copy of bld_details.c t_single with the pattern call swapped. ---- */
typedef Geo *(*NsPatFn)(double cx, double cy, const Geo *g, PyRandom *rng, int *err);
typedef struct { const DtFn *d; NsPatFn pat; } ExtSingle;
typedef struct { const LfShape *sh; PyRandom *rng; double R; } ShMk;
static Geo *sh_make(void *c, int has_c, double cx, double cy, int *err) {
    ShMk *m = (ShMk *)c; Geo *g;
    if (!has_c) { *err = 1; return NULL; }
    g = lf_shape(m->sh, cx, cy, m->rng);
    if (!g) *err = 1;
    return g;
}
static int rp_at(void *c, double *x, double *y) { ShMk *m = (ShMk *)c; lf_rp(m->rng, 0, m->R, x, y); return 0; }
static int run_ext_single(ScFn *fn, Scene *s, PyRandom *rng, int lvl) {
    const ExtSingle *E = (const ExtSingle *)fn->ctx; const DtFn *f = E->d; (void)lvl;
    ShMk m = { &f->sh, rng, 10.5 }; Geo *g = NULL, *pat = NULL, *ins = NULL, *q = NULL, *cen = NULL; Num z, tz; GeoList l = { 0 }; int r, e;
    r = pl_find(s, sh_make, &m, 120, 0.4, NULL, 0, NULL, rp_at, &m, &g, &z);
    if (r < 0) goto fail;
    if (r == 0) return 0;
    gl_push(&l, g);
    if (dt_raise(s, &l, z, py_uniform(rng, f->h0, f->h1), f->k1, f->how, f->tiers, f->nt, &tz) < 0) goto fail;
    if (tz.t != NUM_NONE && tz.v != 0) {
        int err = 0;
        cen = geo_new(shp_centroid(g->g)); if (!cen) goto fail;
        pat = E->pat(shp_x(cen->g), shp_y(cen->g), g, rng, &err);
        if (!pat || err) goto fail;
        e = shp_is_empty(pat->g); if (e < 0) goto fail;
        if (!e) {
            ins = lf_inset(g, 0.35); if (!ins) goto fail;
            q = geo_new(shp_intersection(pat->g, ins->g)); if (!q) goto fail;
            e = shp_is_empty(q->g); if (e < 0) goto fail;
            if (!e) { GeoList ql = { 0 }; gl_push(&ql, q); r = pl_topcut(s, &ql, tz, num_f(0.3), kind_str("slot")); gl_free(&ql); if (r < 0) goto fail; }
        }
    }
    geo_unref(g); geo_unref(cen); geo_unref(pat); geo_unref(ins); geo_unref(q); gl_free(&l); return 0;
fail: geo_unref(g); geo_unref(cen); geo_unref(pat); geo_unref(ins); geo_unref(q); gl_free(&l); return SC_ERR;
}

/* style_ashveil._kind(fn, k, shape).w(s, rng, lvl): fn(s, rng, lvl) */
static int run_wrap(ScFn *f, Scene *s, PyRandom *rng, int lvl) { ScFn *in = (ScFn *)f->ctx; TRACED(in && in->run(in, s, rng, lvl) < 0 ? SC_ERR : 0); }
/* style_ashveil._sd(name, kind, n).w / style_thornwild._sd(name, cls, kind).fn */
typedef struct { const char *name; int n0, n1, thorn; } SfSd;
typedef int (*SfSdTier)(const char *name, int n0, int n1, Scene *s, PyRandom *rng, int lvl);
typedef int (*SfSdTier1)(const char *name, Scene *s, PyRandom *rng, int lvl);
static int run_nssd(ScFn *f, Scene *s, PyRandom *rng, int lvl) {
    const SfSd *d = (const SfSd *)f->ctx; int r;
    if (d->thorn) { SfSdTier1 t = (SfSdTier1)bld_any("style_thornwild", "_sd", "ns_sd_tier1"); r = t ? t(d->name, s, rng, lvl) : SC_ERR; }
    else { SfSdTier t = (SfSdTier)bld_any("style_ashveil", "_sd", "ns_sd_tier"); r = t ? t(d->name, d->n0, d->n1, s, rng, lvl) : SC_ERR; }
    return r < 0 ? SC_ERR : 0;
}

static ScFn *ns_fn(const NsV *f);
/* an entry (NS_ENTRY[i]) or a node inside one */
static ScFn *ns_ref(const NsV *v) {
    if (!v) return NULL;
    if (v->t == 'E') {
        int e = (int)v->v; ScFn *f = memo_get(&NS_ENTRY[e], MK_NS);
        if (f) return f;
        f = ns_fn(&NS_V[NS_ENTRY[e].v]);
        return f ? memo_put(&NS_ENTRY[e], MK_NS, f) : NULL;
    }
    return ns_fn(v);
}
/* the builder call a features.comp closure holds: F<mod:b_x.<locals>.f>(its closure) as an FtCall with keywords */
static int ns_call(const NsV *b, FtCall *c, char *modbuf, char *namebuf) {
    const char *col = strchr(b->s, ':'), *dot; const NsV *cl = ns_kid(b, 1);
    if (!col) return -1;
    snprintf(modbuf, 64, "%.*s", (int)(col - b->s), b->s);
    dot = strchr(col + 1, '.'); snprintf(namebuf, 64, "%.*s", dot ? (int)(dot - col - 1) : (int)strlen(col + 1), col + 1);
    memset(c, 0, sizeof *c); c->mod = so_intern(modbuf); c->name = so_intern(namebuf);
    for (int j = 0; cl && j < cl->n && c->nargs < 6; j++) {
        const NsV *k = ns_kid(cl, j), *v = ns_kid(k, 0); FtArg *a = &c->a[c->nargs];
        memset(a, 0, sizeof *a); a->kw = so_intern(k->s);
        if (v->t == 's') { a->t = 's'; a->s = so_intern(v->s); }
        else if (v->t == 'i' || v->t == 'f') { a->t = v->t == 'i' ? 'i' : 'f'; a->n = 1; a->v[0] = v->v; a->imask = v->t == 'i'; }
        else if (v->t == 'b') { a->t = 'b'; a->n = 1; a->v[0] = v->v; }
        else if (v->t == 't' || v->t == 'l') {
            int pairs = v->n && ns_kid(v, 0)->t == 't';
            a->t = pairs ? 'R' : 'T';
            for (int q = 0; q < v->n && a->n < 8; q++) {
                const NsV *x = ns_kid(v, q);
                if (pairs) { for (int r = 0; r < 2 && a->n < 8; r++) { const NsV *y = ns_kid(x, r); if (y->t == 'i') a->imask |= 1u << a->n; a->v[a->n++] = y->v; } }
                else { if (x->t == 'i') a->imask |= 1u << a->n; a->v[a->n++] = x->v; }
            }
        } else if (v->t == 'N') a->t = 'N';
        else return -1;
        c->nargs++;
    }
    return 0;
}
static ScFn *ns_fn(const NsV *f) {
    char q[160]; const char *col, *qual; LfShape sh; LfPat pt; Tier t[4]; int nt, hs, hp; double a, b, d, e; DtFn *df = NULL; ScFn *out;
    static const double ROCKY[4] = { 0.7, 6, 0.3, 40 }, SINGLE_T[4] = { 0.75, 5, 0.25, 40 };
    if (!f || f->t != 'F' || !(col = strchr(f->s, ':'))) { missing("a newer style's entry that is not a function"); return NULL; }
    qual = col + 1;
    if (!strncmp(f->s, "details:", 8) && strstr(qual, ".<locals>.fn")) {
        hs = nv_shape(kw(f, "shape"), &sh);
        if (hs < 0) { snprintf(q, sizeof q, "shape of %s", f->s); missing(q); return NULL; }
        if (!strncmp(qual, "scatter.", 8)) {
            nv_pair(kw(f, "n"), 3, 6, &a, &b); nv_pair(kw(f, "h"), 0.5, 0.9, &d, &e); nt = nv_tiers(kw(f, "tiers"), ROCKY, 2, t);
            df = dt_scatter(hs ? &sh : NULL, a, b, d, e, nv_s(kw(f, "kind")), nv_s(kw(f, "how")), nv_d(kw(f, "spread"), 1.6), t, nt);
        } else if (!strncmp(qual, "single.", 7)) {
            nv_pair(kw(f, "h"), 0.8, 1.4, &a, &b); nt = nv_tiers(kw(f, "tiers"), SINGLE_T, 2, t);
            hp = nv_pat(kw(f, "top"), &pt);
            if (hp < 0) { snprintf(q, sizeof q, "top pattern of %s", f->s); missing(q); return NULL; }
            df = dt_single(hs ? &sh : NULL, a, b, nv_s(kw(f, "kind")), nv_s(kw(f, "how")), hp == 1 ? &pt : NULL, t, nt);
            if (hp == 2) {          /* top=style_blight.pat_brune() */
                ExtSingle *E = (ExtSingle *)malloc(sizeof *E);
                E->d = df; E->pat = (NsPatFn)bld_any("style_blight", "pat_brune", "ns_pat");
                if (!E->pat) { free(E); missing("style_blight.pat_brune"); return NULL; }
                out = cp_fn_new("single.<locals>.fn", run_ext_single, E, free);
                out->tag.tmpl = attr(f, "tmpl"); out->tag.kind = attr(f, "kind"); out->tag.shape = attr(f, "shape"); out->tag.cls = attr(f, "cls");
                return out;
            }
        } else if (!strncmp(qual, "sunk.", 5)) {
            nv_pair(kw(f, "depth"), 0.3, 0.6, &a, &b); nv_pair(kw(f, "n"), 1, 1, &d, &e);
            df = dt_sunk(hs ? &sh : NULL, a, b, nv_s(kw(f, "kind")), d, e, nv_d(kw(f, "spread"), 0.0));
        } else if (!strncmp(qual, "trail.", 6)) {
            nv_pair(kw(f, "n"), 5, 8, &a, &b);
            df = dt_trail(nv_d(kw(f, "step"), 1.3), a, b, hs ? &sh : NULL, nv_num(kw(f, "depth"), 0.35), nv_s(kw(f, "kind")));
        } else if (!strncmp(qual, "path.", 5)) {
            nv_pair(kw(f, "l"), 6, 10, &a, &b);
            df = dt_path(nv_d(kw(f, "w"), 0.7), a, b, nv_num(kw(f, "h"), 0.4), nv_s(kw(f, "kind")), nv_s(kw(f, "how")), nv_d(kw(f, "amp"), 0.6));
        }
    } else if (!strncmp(f->s, "styles2:f_post_row.", 19)) {
        const NsV *rl = kw(f, "rail");
        nv_pair(kw(f, "h"), 1.8, 2.4, &a, &b); nv_pair(kw(f, "n"), 2, 3, &d, &e);
        df = dt_post_row(nv_s(kw(f, "kind")), a, b, rl ? rl->v != 0 : 1, d, e, nv_d(kw(f, "gap"), 4.2));
    } else if (!strncmp(f->s, "styles2:f_column.", 17)) df = dt_column(nv_s(kw(f, "kind")) ? nv_s(kw(f, "kind")) : "tcolumn");
    else if (!strncmp(f->s, "styles2:f_vent.", 15)) df = dt_vent(nv_s(kw(f, "kind_cut")), nv_s(kw(f, "kind_rim")));
    else if (!strncmp(f->s, "styles2:f_", 10) && !strchr(qual, '.')) {
        static const char *const PL[] = { "f_headstone", "f_lantern", "f_stump", "f_teeth", "f_sandbags", "f_coffin", "f_trapdoor", "f_lily" };
        for (int k = 0; k < 8; k++) if (!strcmp(qual, PL[k])) { df = (DtFn *)dt_plain(DT_HEADSTONE + k); break; }
    } else if (!strncmp(f->s, "style_ashveil:_kind.", 20)) {
        ScFn *in = ns_ref(kw(f, "fn"));
        if (!in) return NULL;
        snprintf(q, sizeof q, "ashveil_%s", attr(f, "shape") && attr(f, "shape")[0] ? attr(f, "shape") : attr(f, "kind") ? attr(f, "kind") : "");
        out = cp_fn_new(q, run_wrap, cp_fn_ref(in), (void (*)(void *))cp_fn_unref);
        out->tag.tmpl = attr(f, "tmpl"); out->tag.kind = attr(f, "kind"); out->tag.shape = attr(f, "shape"); out->tag.cls = attr(f, "cls");
        return out;
    } else if (!strncmp(f->s, "style_ashveil:_sd.", 18) || !strncmp(f->s, "style_thornwild:_sd.", 20)) {
        SfSd *sd = (SfSd *)calloc(1, sizeof *sd);
        sd->thorn = f->s[6] == 't'; sd->name = nv_s(kw(f, "name"));
        nv_pair(kw(f, "n"), 1, 1, &a, &b); sd->n0 = (int)a; sd->n1 = (int)b;
        if (sd->thorn) snprintf(q, sizeof q, "_sd.<locals>.fn"); else snprintf(q, sizeof q, "ashveil_%s", sd->name ? sd->name : "");
        out = cp_fn_new(q, run_nssd, sd, free);
        out->tag.tmpl = attr(f, "tmpl"); out->tag.kind = attr(f, "kind"); out->tag.shape = attr(f, "shape"); out->tag.cls = attr(f, "cls");
        return out;
    } else if (!strncmp(f->s, "features:comp.", 14)) {
        const NsV *bn = kw(f, "build"); FtCall c; char mb[64], nb[64]; FtBuildFn bf; void *ctx; int r; FtComp cc;
        if (!bn || bn->t != 'F' || ns_call(bn, &c, mb, nb) < 0) { snprintf(q, sizeof q, "the builder of %s", f->s); missing(q); return NULL; }
        r = bld_make(&c, &bf, &ctx);
        if (r == -2 && !strncmp(c.name, "b_", 2)) { c.name = so_intern(nb + 2); r = bld_make(&c, &bf, &ctx); }
        if (r != 0) { snprintf(q, sizeof q, "%s.%s (%d)", c.mod, nb, r); missing(q); return NULL; }
        cc = ft_comp(bf, ctx, nv_d(kw(f, "r"), 10.5), nv_d(kw(f, "gap"), 0.4), (int)nv_d(kw(f, "ground"), 0), (int)nv_d(kw(f, "tries"), 60));
        cc.kind = attr(f, "kind"); cc.shape = attr(f, "shape"); cc.cls = attr(f, "cls");
        { FtComp *p = (FtComp *)malloc(sizeof *p); *p = cc; out = cp_fn_new("comp.<locals>.fn", run_comp, p, free); }
        out->tag.tmpl = attr(f, "tmpl"); out->tag.kind = attr(f, "kind"); out->tag.shape = attr(f, "shape"); out->tag.cls = attr(f, "cls");
        return out;
    } else if (!strchr(qual, '.')) {          /* gg3:u_rebar, style_obelis:f_monolith ...: the tier function itself */
        char mod[64]; BsTierFn fn;
        snprintf(mod, sizeof mod, "%.*s", (int)(col - f->s), f->s);
        fn = !strcmp(mod, "gg3") ? bs_tier_fn(qual) : (BsTierFn)bld_any(mod, qual, "tier");
        if (fn) {
            out = fn_tier(so_intern(qual), fn);
            if (!out->tag.tmpl && !out->tag.kind && !out->tag.shape && !out->tag.cls) { out->tag.tmpl = attr(f, "tmpl"); out->tag.kind = attr(f, "kind"); out->tag.shape = attr(f, "shape"); out->tag.cls = attr(f, "cls"); }
            return cp_fn_ref(out);
        }
    }
    if (!df) { snprintf(q, sizeof q, "%s", f->s); missing(q); return NULL; }
    if (df->t >= DT_HEADSTONE) snprintf(q, sizeof q, "%s", df->qual); else snprintf(q, sizeof q, "%s.<locals>.fn", df->qual);
    out = cp_fn_new(q, run_dt, df, NULL);
    out->tag.tmpl = attr(f, "tmpl"); out->tag.kind = attr(f, "kind"); out->tag.shape = attr(f, "shape"); out->tag.cls = attr(f, "cls");
    return out;
}

/* ================================================================ the per-style lists */
static const NsV *ns_style(const char *style) { for (int i = 0; i < NS_NSTYLE; i++) if (!strcmp(NS_STYLE[i].style, style)) return &NS_V[NS_STYLE[i].v]; return NULL; }
static void push(ScFnList *l, ScFn *f) { if (f) cp_fnl_push(l, f); }
/* the base pool of DETAILS[style] before features / style_details extend it; 0 = the style has none */
static int base_pool(const char *style, ScFnList *out) {
    const NsV *ns = ns_style(style); const DtList *d;
    if (ns) { const NsV *dl = ns_get(ns, "details"); for (int i = 0; dl && i < dl->n; i++) push(out, ns_ref(ns_kid(dl, i))); return 1; }
    if ((d = dt_details2(style)) || (d = dt_details(style))) { for (int i = 0; i < d->n; i++) push(out, fn_dt(d->v[i])); return 1; }
    return 0;
}
static void new20(const char *style, ScFnList *out) {
    int n = sdr_reg_count(style);
    for (int i = 0; i < n; i++) push(out, fn_sdr(sdr_reg_get(style, i)));
}
int sf_style_fns(const char *style, MkStyleFns *out) {
    const NsV *ns = ns_style(style); const DtTiers *t2; int found = 0;
    memset(out, 0, sizeof *out);
    /* gg3.TIERS[style] */
    if (ns) {
        const NsV *tl = ns_get(ns, "tiers");
        for (int k = 0; tl && k < 3 && k < tl->n; k++) { const NsV *l = ns_kid(tl, k); for (int i = 0; i < l->n; i++) push(&out->tiers[k], ns_ref(ns_kid(l, i))); }
        found = 1;
    } else if ((t2 = dt_tiers2(style))) {
        for (int k = 0; k < 3; k++) for (int i = 0; i < t2->tier[k].n; i++) push(&out->tiers[k], fn_dt(t2->tier[k].v[i]));
        found = 1;
    } else {
        for (int i = 0; i < BS_NTIERS; i++) if (!strcmp(BS_TIERS[i].style, style)) {
            ScFn *f = fn_tier(BS_TIERS[i].name, BS_TIERS[i].fn); push(&out->tiers[BS_TIERS[i].tier - 1], f); found = 1;
            if (bs_is_late(BS_TIERS[i].name)) push(&out->late, f);
            if (bs_is_sparse(BS_TIERS[i].name)) push(&out->sparse, f);
        }
    }
    if (!found) return SC_ERR;
    /* details.DETAILS[style] + features.NEW[style] + style_details.NEW20[style] */
    if (base_pool(style, &out->details)) {
        if (sf_new_details) for (int i = 0; i < FT_NNEW; i++) if (!strcmp(FT_NEW[i].style, style)) push(&out->details, fn_new(&FT_NEW[i]));
        if (sf_style_details) { ScFnList w = { 0 }; new20(style, &w); for (int i = 0; i < w.n; i++) push(&out->details, w.v[i]); cp_fnl_free(&w); }
    }
    if (sf_style_details) new20(style, &out->new20);
    /* features.LANDMARKS[style] (scenes off) */
    {
        int n = 0;
        for (int i = 0; i < FT_NLMS; i++) if (!strcmp(FT_LMS[i].style, style)) n++;
        out->landmarks = (FtLmEntry *)calloc((size_t)n + 1, sizeof(FtLmEntry));
        for (int i = 0; i < FT_NLMS; i++) if (!strcmp(FT_LMS[i].style, style)) {
            FtBuildFn bf = NULL; void *ctx = NULL; FtLmEntry *e = &out->landmarks[out->nlandmarks];
            if (bld_ft_make(&FT_LMS[i].b, 1, &bf, &ctx) != 0) { char t[160]; snprintf(t, sizeof t, "features.LANDMARKS %s: L_%s", style, FT_LMS[i].b.name); missing(t); continue; }
            e->lo = FT_LMS[i].lo; e->lm.build = bf; e->lm.ctx = ctx; e->lm.r_frac = FT_LMS[i].r_frac; e->lm.gap = FT_LMS[i].gap; e->lm.ground = FT_LMS[i].ground;
            out->nlandmarks++;
        }
    }
    return 0;
}

/* ================================================================ details.anchors / details.apply (scenes off) */
static int sf_anchors(Scene *s, const char *style, PyRandom *rng, int t) {
    const DtList *pool = dt_anchors(style); (void)t;
    if (!pool || !pool->n) return 0;
    return dt_anchors_run(s, pool, rng) < 0 ? SC_ERR : 0;
}
static int apply_one(void *ctx, int i, Scene *s, PyRandom *rng, int t) { ScFn *f = ((ScFnList *)ctx)->v[i]; int sv = sc_feat_push(cp_fn_label(f)), r = f->run(f, s, rng, t); sc_feat_pop(sv); return r; }
static int sf_apply(Scene *s, const char *style, PyRandom *rng, int t) {
    ScFnList pool = { 0 }; int r = 0;
    if (base_pool(style, &pool)) {          /* DETAILS[style] as the kit has it then (extended) */
        if (sf_new_details) for (int i = 0; i < FT_NNEW; i++) if (!strcmp(FT_NEW[i].style, style)) push(&pool, fn_new(&FT_NEW[i]));
        if (sf_style_details) new20(style, &pool);
    }
    if (pool.n) r = dt_apply(s, pool.n, apply_one, &pool, rng, t);
    cp_fnl_free(&pool);
    return r < 0 ? SC_ERR : 0;
}

/* ================================================================ the subjects a style file adds
   style_blight._register: SUBJECTS.setdefault("blight_thorns", lambda p: L_thorns(p.get("gem") or "thorn")),
   "blight_rift": L_rift(p.get("pit") or "rift", "blightrock"), "blight_runes": L_rune_ring("runecut", p.get("pit") or "rift").
   The rest are scenes.py's (bld_scenes.c bs_subject). */
CpSubjectFn sf_subject_base;
static FtArg sarg(const char *s) { FtArg a; memset(&a, 0, sizeof a); a.t = 's'; a.s = s; return a; }
int sf_subject(const char *name, const ScPal *pal, FtBuildFn *fn, void **ctx) {
    FtCall c; memset(&c, 0, sizeof c); c.mod = "style_blight";
    if (!strcmp(name, "blight_thorns")) { c.name = "L_thorns"; c.nargs = 1; c.a[0] = sarg(pal->gem && pal->gem[0] ? pal->gem : "thorn"); }
    else if (!strcmp(name, "blight_rift")) { c.name = "L_rift"; c.nargs = 2; c.a[0] = sarg(pal->pit && pal->pit[0] ? pal->pit : "rift"); c.a[1] = sarg("blightrock"); }
    else if (!strcmp(name, "blight_runes")) { c.name = "L_rune_ring"; c.nargs = 2; c.a[0] = sarg("runecut"); c.a[1] = sarg(pal->pit && pal->pit[0] ? pal->pit : "rift"); }
    else return sf_subject_base ? sf_subject_base(name, pal, fn, ctx) : SC_ERR;
    return bld_make(&c, fn, ctx) == 0 ? 0 : SC_ERR;
}

void sf_register(void) {
    bld_add_any("gg3", "style_fns", "MkStyleFn", (void *)sf_style_fns);
    bld_add_any("details", "anchors", "MkDetailsFn", (void *)sf_anchors);
    bld_add_any("details", "apply", "MkDetailsFn", (void *)sf_apply);
}

/* ================================================================ the lists as text (the proof) */
typedef struct { char *s; size_t n, cap; } Tx;
static void tx(Tx *b, const char *fmt, ...) __attribute__((format(printf, 2, 3)));
#include <stdarg.h>
static void tx(Tx *b, const char *fmt, ...) {
    char t[1024]; va_list ap; va_start(ap, fmt); int n = vsnprintf(t, sizeof t, fmt, ap); va_end(ap);
    if (n < 0) return;
    if (b->n + (size_t)n + 1 > b->cap) { b->cap = (b->n + (size_t)n + 1) * 2 + 4096; b->s = (char *)realloc(b->s, b->cap); }
    memcpy(b->s + b->n, t, (size_t)n + 1); b->n += (size_t)n;
}
static const char *tq(const char *s) { return s ? s : "-"; }
char *sf_describe(const char *style, int tagged) {
    MkStyleFns F; Tx b = { 0 }; ScFn *seen[8192]; int ns = 0;
    if (sf_style_fns(style, &F) < 0) { tx(&b, "KeyError %s\n", style); return b.s; }
    if (tagged) {          /* make's tag_all */
        SmTag tg[8192]; const char *nm[8192]; int same[8192], n = 0; ScFn *all[8192];
        for (int k = 0; k < 3; k++) for (int i = 0; i < F.tiers[k].n && n < 8192; i++) all[n++] = F.tiers[k].v[i];
        for (int i = 0; i < F.details.n && n < 8192; i++) all[n++] = F.details.v[i];
        for (int i = 0; i < n; i++) { tg[i] = all[i]->tag; nm[i] = all[i]->qualname; same[i] = i; for (int j = 0; j < i; j++) if (all[j] == all[i]) { same[i] = j; break; } }
        sm_tag_all(tg, nm, same, n);
        for (int i = 0; i < n; i++) if (same[i] == i) all[i]->tag = tg[i];
    }
    const char *LN[5] = { "T0", "T1", "T2", "D", "W" }; const ScFnList *L[5] = { &F.tiers[0], &F.tiers[1], &F.tiers[2], &F.details, &F.new20 };
    tx(&b, "== %s\n", style);
    for (int k = 0; k < 5; k++) for (int i = 0; i < L[k]->n; i++) {
        ScFn *f = L[k]->v[i]; int at = -1;
        for (int j = 0; j < ns; j++) if (seen[j] == f) { at = j; break; }
        if (at >= 0) { tx(&b, "%s E%d\n", LN[k], at); continue; }
        if (ns < 8192) seen[ns++] = f;
        int late = 0, sp = 0; for (int j = 0; j < F.late.n; j++) if (F.late.v[j] == f) late = 1; for (int j = 0; j < F.sparse.n; j++) if (F.sparse.v[j] == f) sp = 1;
        tx(&b, "%s N%d %s %s|%s|%s|%s %s%s\n", LN[k], ns - 1, f->qualname, tq(f->tag.tmpl), tq(f->tag.kind), tq(f->tag.shape), tq(f->tag.cls), late ? "L" : "", sp ? "S" : "");
    }
    for (int i = 0; i < F.nlandmarks; i++) tx(&b, "M %.17g %.17g %.17g %d\n", F.landmarks[i].lo, F.landmarks[i].lm.r_frac, F.landmarks[i].lm.gap, F.landmarks[i].lm.ground);
    mk_style_fns_free(&F);
    return b.s;
}
