/* bld_details.c: details.py's templates, set pieces, apply / anchors and the DETAILS / ANCHORS tables in C (see
   bld_details.h). Each function names the Python closure it copies. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "bld_details.h"
#include "shp.h"
#include "pyexact.h"

#define PI 3.141592653589793
static Num NF(double v) { return num_f(v); }
static Kind *KS(const char *s) { return kind_str(s ? s : "None"); }

/* details._raise(s, ps, z, h, kind, how, tiers): *top = up()'s top, or None (mound) */
static const char *ROUNDK[] = { "bone", "root", "rope", "vein", "tendril", "cable", "twig", "tooth", "chain" };
int dt_raise(Scene *s, const GeoList *ps, Num z, double h, const char *kind, const char *how, const Tier *tiers, int nt, Num *top) {
    int round_ = 0;
    *top = num_none();
    if (how && !strcmp(how, "up") && kind && h < 1.6)
        for (size_t i = 0; i < sizeof ROUNDK / sizeof ROUNDK[0]; i++) if (!strcmp(kind, ROUNDK[i])) round_ = 1;
    if (round_) { how = "mound"; tiers = FT_ROUND_T.t; nt = FT_ROUND_T.n; }          /* a round back, not a flat top */
    if (how && !strcmp(how, "mound")) return pl_mound(s, ps, z, NF(h), tiers, nt, KS(kind)) < 0 ? SC_ERR : 0;
    return pl_up(s, ps, z, NF(h), KS(kind), top) < 0 ? SC_ERR : 0;
}

/* ---------------- the callbacks the templates hand to find / place_group ---------------- */
/* lambda cx, cy: shape(cx, cy, rng) */
typedef struct { const LfShape *sh; PyRandom *rng; double R; } ShMk;
static Geo *sh_make(void *c, int has_c, double cx, double cy, int *err) {
    ShMk *m = (ShMk *)c; Geo *g;
    if (!has_c) { *err = 1; return NULL; }
    g = lf_shape(m->sh, cx, cy, m->rng);
    if (!g) *err = 1;
    return g;
}
static int rp_at(void *c, double *x, double *y) { ShMk *m = (ShMk *)c; lf_rp(m->rng, 0, m->R, x, y); return 0; }          /* lambda: rp(rng, 0, R) */
/* lambda cx, cy: cluster(s, rng, k, _one(shape, rng), cx, cy, spread, 0.3) */
typedef struct { Scene *s; PyRandom *rng; int k; const LfShape *sh; double spread; } ClMk;
static int cl_sa(void *c, double cx, double cy, GeoList *out) {
    ClMk *m = (ClMk *)c; PlOne one; memset(&one, 0, sizeof one);
    one.has_shape = 1; one.sh = *m->sh; one.srng = m->rng;
    return pl_cluster(m->s, m->rng, m->k, &one, cx, cy, m->spread, 0.3, out);
}
static GeoList one_list(Geo *g) { GeoList l = { 0 }; gl_push(&l, g); return l; }

/* ---------------- the templates ---------------- */
/* scatter(...).fn(s, rng, lvl) */
static int t_scatter(const DtFn *f, Scene *s, PyRandom *rng) {
    ClMk m; GeoList ps = { 0 }, keep = { 0 }; Num z, top; int r;
    m.s = s; m.rng = rng; m.k = (int)py_randint(rng, (int64_t)f->n0, (int64_t)f->n1); m.sh = &f->sh; m.spread = f->x[0];
    r = pl_place_group(s, rng, cl_sa, &m, 120, 0.35, 0, NULL, &ps, &z);
    if (r < 0) goto fail;
    if (r == 1 && ps.n) {
        double h;
        for (int i = 0; i < ps.n; i++) { int w = lf_wide_enough(ps.v[i], 0.7); if (w < 0) goto fail; if (w) gl_push(&keep, ps.v[i]); }
        h = py_uniform(rng, f->h0, f->h1);
        if (dt_raise(s, &keep, z, h, f->k1, f->how, f->tiers, f->nt, &top) < 0) goto fail;
    }
    gl_free(&ps); gl_free(&keep); return DT_NONE;
fail: gl_free(&ps); gl_free(&keep); return SC_ERR;
}
/* single(...).fn(s, rng, lvl) */
static int t_single(const DtFn *f, Scene *s, PyRandom *rng) {
    ShMk m = { &f->sh, rng, 10.5 }; Geo *g = NULL, *pat = NULL, *ins = NULL, *q = NULL, *cen = NULL; Num z, tz; GeoList l = { 0 }; int r, e;
    r = pl_find(s, sh_make, &m, 120, 0.4, NULL, 0, NULL, rp_at, &m, &g, &z);
    if (r < 0) goto fail;
    if (r == 0) return DT_NONE;
    l = one_list(g);
    if (dt_raise(s, &l, z, py_uniform(rng, f->h0, f->h1), f->k1, f->how, f->tiers, f->nt, &tz) < 0) goto fail;
    if (f->has_pat && tz.t != NUM_NONE && tz.v != 0) {
        cen = geo_new(shp_centroid(g->g)); if (!cen) goto fail;
        pat = lf_pat(&f->pat, shp_x(cen->g), shp_y(cen->g), g, rng);
        if (!pat) goto fail;
        e = shp_is_empty(pat->g); if (e < 0) goto fail;
        if (!e) {
            ins = lf_inset(g, 0.35); if (!ins) goto fail;
            q = geo_new(shp_intersection(pat->g, ins->g)); if (!q) goto fail;
            e = shp_is_empty(q->g); if (e < 0) goto fail;
            if (!e) { GeoList ql = one_list(q); r = pl_topcut(s, &ql, tz, NF(0.3), KS("slot")); gl_free(&ql); if (r < 0) goto fail; }
        }
    }
    geo_unref(g); geo_unref(cen); geo_unref(pat); geo_unref(ins); geo_unref(q); gl_free(&l); return DT_NONE;
fail: geo_unref(g); geo_unref(cen); geo_unref(pat); geo_unref(ins); geo_unref(q); gl_free(&l); return SC_ERR;
}
/* sunk(...).fn(s, rng, lvl) */
static int t_sunk(const DtFn *f, Scene *s, PyRandom *rng) {
    int k = (int)py_randint(rng, (int64_t)f->n0, (int64_t)f->n1), r; Num z;
    if (k == 1 && f->x[0] == 0) {
        ShMk m = { &f->sh, rng, 10.5 }; Geo *g = NULL;
        r = pl_find(s, sh_make, &m, 120, 0.3, NULL, 0, NULL, rp_at, &m, &g, &z);
        if (r < 0) return SC_ERR;
        if (r == 1) { GeoList l = one_list(g); r = pl_cut(s, &l, z, NF(py_uniform(rng, f->h0, f->h1)), KS(f->k1)); gl_free(&l); geo_unref(g); if (r < 0) return SC_ERR; }
        return DT_NONE;
    }
    {
        ClMk m; GeoList ps = { 0 };
        m.s = s; m.rng = rng; m.k = k; m.sh = &f->sh; m.spread = f->x[0];
        r = pl_place_group(s, rng, cl_sa, &m, 120, 0.3, 0, NULL, &ps, &z);
        if (r < 0) { gl_free(&ps); return SC_ERR; }
        if (r == 1 && ps.n) r = pl_cut(s, &ps, z, NF(py_uniform(rng, f->h0, f->h1)), KS(f->k1));
        gl_free(&ps);
        return r < 0 ? SC_ERR : DT_NONE;
    }
}
/* trail(...).fn(s, rng, lvl): at(cx, cy) */
typedef struct { const DtFn *f; PyRandom *rng; } TrMk;
static int tr_sa(void *c, double cx, double cy, GeoList *out) {
    TrMk *m = (TrMk *)c; const DtFn *f = m->f; double a = lf_aim(m->rng, cx, cy, 2 * PI); int k = (int)py_randint(m->rng, (int64_t)f->n0, (int64_t)f->n1); Frame ff = lf_frame(cx, cy, a);
    for (int i = 0; i < k; i++) {
        double x, y, ox, oy; Geo *g; GEOSGeometry *r;
        lf_at(&ff, (i - k / 2.0) * f->x[0], 0.45 * (i % 2 ? 1 : -1), &x, &y);
        g = lf_shape(&f->sh, x, y, m->rng);
        if (!g) return SC_ERR;
        lf_at(&ff, (i - k / 2.0) * f->x[0], 0, &ox, &oy);
        r = shp_rotate(g->g, LF_DEGREES(a), SHP_ORIGIN_POINT, ox, oy, 0); geo_unref(g);
        if (!r) return SC_ERR;
        gl_push_own(out, geo_new(r));
    }
    return 0;
}
static int t_trail(const DtFn *f, Scene *s, PyRandom *rng) {
    TrMk m = { f, rng }; GeoList ps = { 0 }; Num z; int r;
    r = pl_place_group(s, rng, tr_sa, &m, 120, 0.3, 0, NULL, &ps, &z);
    if (r == 1 && ps.n) r = pl_cut(s, &ps, z, f->num, KS(f->k1));
    gl_free(&ps);
    return r < 0 ? SC_ERR : DT_NONE;
}
/* path(...).fn(s, rng, lvl) */
static int t_path(const DtFn *f, Scene *s, PyRandom *rng) {
    LfShape wig = lf_sh(SH_WIG, f->x[1], f->x[2], f->x[0], f->x[3], 0); ShMk m = { &wig, rng, 8 }; Geo *g = NULL; Num z, top; GeoList l; int r;
    r = pl_find(s, sh_make, &m, 120, 0.35, NULL, 0, NULL, rp_at, &m, &g, &z);
    if (r < 0) return SC_ERR;
    if (r == 0) return DT_NONE;
    l = one_list(g);
    if (f->how && !strcmp(f->how, "cut")) r = pl_cut(s, &l, z, f->num, KS(f->k1));
    else r = pl_up(s, &l, z, f->num, KS(f->k1), &top);
    gl_free(&l); geo_unref(g);
    return r < 0 ? SC_ERR : DT_NONE;
}

/* ---------------- the set pieces: fn(s, rng, g_) -> True / False ---------------- */
/* a_outcrop: lambda cx, cy: stone(cx, cy, r, rng, rng.uniform(.55, .85), rng.randint(5, 8)) */
typedef struct { PyRandom *rng; double r, R; int t; } AMk;
static Geo *a_make(void *c, int has_c, double cx, double cy, int *err) {
    AMk *m = (AMk *)c; Geo *g = NULL;
    if (!has_c) { *err = 1; return NULL; }
    switch (m->t) {
    case DT_OUTCROP: { double asp = py_uniform(m->rng, .55, .85); int nv = (int)py_randint(m->rng, 5, 8); g = lf_stone(cx, cy, m->r, m->rng, asp, nv, 0); break; }
    case DT_HEAP: { int k = (int)py_randint(m->rng, 4, 6); g = lf_blob(cx, cy, m->r, m->rng, k); break; }
    case DT_PIT: { double asp = py_uniform(m->rng, .75, .95); g = lf_stone(cx, cy, m->r, m->rng, asp, 9, 0); break; }
    }
    if (!g) *err = 1;
    return g;
}
static int a_at(void *c, double *x, double *y) { AMk *m = (AMk *)c; lf_rp(m->rng, 0, m->R, x, y); return 0; }
typedef struct { PyRandom *rng; double L, W; } BMk;
static Geo *b_make(void *c, int has_c, double cx, double cy, int *err) {          /* lambda cx, cy: rect(cx, cy, L, W, rng.uniform(0, math.pi)) */
    BMk *m = (BMk *)c; Geo *g;
    if (!has_c) { *err = 1; return NULL; }
    g = lf_rect(cx, cy, m->L, m->W, py_uniform(m->rng, 0, PI));
    if (!g) *err = 1;
    return g;
}
static int b_at(void *c, double *x, double *y) { BMk *m = (BMk *)c; lf_rp(m->rng, 0, 12.0, x, y); return 0; }
typedef struct { PyRandom *rng; double R_, a0, arc; int k; } RMk;
static int r_sa(void *c, double cx, double cy, GeoList *out) {          /* a_ring's at(cx, cy) */
    RMk *m = (RMk *)c;
    for (int i = 0; i < m->k; i++) {
        double t = m->a0 + m->arc * i / m->k, r = py_uniform(m->rng, 1.1, 1.5); int nv = (int)py_randint(m->rng, 5, 7);
        Geo *g = lf_stone(cx + m->R_ * py_cos(t), cy + m->R_ * py_sin(t), r, m->rng, .8, nv, 0);
        if (!g) return SC_ERR;
        gl_push_own(out, g);
    }
    return 0;
}
static const Tier OUTCROP_T[2] = { { { 0.8, NUM_FLOAT, 0 }, { 10, NUM_INT, 0 } }, { { 0.2, NUM_FLOAT, 0 }, { 45, NUM_INT, 0 } } };
static int t_anchor(const DtFn *f, Scene *s, PyRandom *rng, double g_) {
    Geo *g = NULL; Num z, top; GeoList l = { 0 }; int r;
    switch (f->t) {
    case DT_OUTCROP: case DT_HEAP: case DT_PIT: {
        AMk m; m.rng = rng; m.t = f->t;
        m.r = (f->t == DT_OUTCROP ? py_uniform(rng, 3.0, 4.6) : py_uniform(rng, 2.6, 3.8)) * g_;
        m.R = f->t == DT_PIT ? 12.0 : 12.3;
        r = pl_find(s, a_make, &m, 60, 0.6, NULL, f->t == DT_PIT, NULL, a_at, &m, &g, &z);
        if (r < 0) return SC_ERR;
        if (r == 0) return 0;
        l = one_list(g);
        if (f->t == DT_OUTCROP) r = pl_mound(s, &l, z, NF(py_uniform(rng, 1.8, 2.8)), OUTCROP_T, 2, KS(f->k1));
        else if (f->t == DT_HEAP) r = pl_mound(s, &l, z, NF(py_uniform(rng, f->h0, f->h1)), f->tiers, f->nt, KS(f->k1));
        else r = pl_cut(s, &l, z, NF(py_uniform(rng, f->h0, f->h1)), KS(f->k1));
        gl_free(&l); geo_unref(g);
        return r < 0 ? SC_ERR : 1;
    }
    case DT_BLOCK: {
        BMk m; Geo *cen = NULL, *pat = NULL, *ins = NULL, *q = NULL; int e, ret = SC_ERR;
        m.rng = rng; m.L = py_uniform(rng, 5.5, 7.5) * g_; m.W = py_uniform(rng, 3.6, 5.0) * g_;
        r = pl_find(s, b_make, &m, 60, 0.6, NULL, 0, NULL, b_at, &m, &g, &z);
        if (r < 0) return SC_ERR;
        if (r == 0) return 0;
        l = one_list(g);
        if (pl_up(s, &l, z, NF(py_uniform(rng, f->h0, f->h1)), KS(f->k1), &top) < 0) goto bfail;
        if (top.t == NUM_NONE || top.v == 0) { ret = 0; goto bfail; }
        if (!(cen = geo_new(shp_centroid(g->g)))) goto bfail;
        if (f->has_pat) {
            pat = lf_pat(&f->pat, shp_x(cen->g), shp_y(cen->g), g, rng);
            if (!pat) goto bfail;
            e = shp_is_empty(pat->g); if (e < 0) goto bfail;
            if (!e) {
                if (!(ins = lf_inset(g, 0.45))) goto bfail;
                if (!(q = geo_new(shp_intersection(pat->g, ins->g)))) goto bfail;
                e = shp_is_empty(q->g); if (e < 0) goto bfail;
                if (!e) { GeoList ql = one_list(q); r = pl_topcut(s, &ql, top, NF(0.3), KS("slot")); gl_free(&ql); if (r < 0) goto bfail; }
            }
        } else if (top.v + 0.6 <= SC.CAP) {
            double fx = py_uniform(rng, .4, .6), fy = py_uniform(rng, .5, .7), dx, dy; GEOSGeometry *a1, *a2; ShpList qs = { 0 }; int bi = -1; double ba = 0;
            a1 = shp_scale(g->g, fx, fy, SHP_ORIGIN_CENTER, 0, 0); if (!a1) goto bfail;
            dx = py_uniform(rng, -.8, .8); dy = py_uniform(rng, -.5, .5);
            a2 = shp_translate(a1, dx, dy); shp_free(a1); if (!a2) goto bfail;
            if (!(ins = lf_inset(g, 0.5))) { shp_free(a2); goto bfail; }
            a1 = shp_intersection(a2, ins->g); shp_free(a2); if (!a1) goto bfail;
            shp_polys_of(a1, &qs); shp_free(a1);
            for (int i = 0; i < qs.n; i++) { double ar = shp_area(qs.g[i]); if (ar > 3.0 && (bi < 0 || ar > ba)) { bi = i; ba = ar; } }
            if (bi >= 0) {
                GeoList pl = { 0 }, out = { 0 }; Num t2 = num_add(top, NF(py_uniform(rng, 0.6, 1.0)));
                gl_push_own(&pl, geo_new(qs.g[bi])); qs.g[bi] = NULL;
                r = sc_prism(s, &pl, num_min2(t2, NF(SC.CAP)), KS(f->k2 && f->k2[0] ? f->k2 : f->k1), num_sub(top, NF(0.15)), SC.MINW, 0, &out);
                gl_free(&pl); gl_free(&out);
                if (r < 0) { shp_list_free(&qs); goto bfail; }
            }
            shp_list_free(&qs);
        }
        ret = 1;
    bfail:
        geo_unref(g); geo_unref(cen); geo_unref(pat); geo_unref(ins); geo_unref(q); gl_free(&l);
        return ret;
    }
    case DT_RING: {
        RMk m; GeoList ps = { 0 };
        m.rng = rng; m.R_ = py_uniform(rng, 5.0, 7.0) * g_; m.k = (int)py_randint(rng, (int64_t)f->n0, (int64_t)f->n1); m.a0 = py_uniform(rng, 0, 6.283); m.arc = py_uniform(rng, 0.55, 1.0) * 6.283;
        r = pl_place_group(s, rng, r_sa, &m, 60, 0.5, 0, NULL, &ps, &z);
        if (r < 0) { gl_free(&ps); return SC_ERR; }
        if (r == 0 || !ps.n) { gl_free(&ps); return 0; }
        for (int i = 0; i < ps.n; i++) {
            GeoList one = one_list(ps.v[i]);
            r = pl_up(s, &one, z, NF(py_uniform(rng, f->h0, f->h1)), KS(f->k1), &top); gl_free(&one);
            if (r < 0) { gl_free(&ps); return SC_ERR; }
        }
        gl_free(&ps);
        return 1;
    }
    }
    return SC_ERR;
}

int dt_run(const DtFn *f, Scene *s, PyRandom *rng, double arg) {
    switch (f->t) {
    case DT_SCATTER: return t_scatter(f, s, rng);
    case DT_SINGLE: return t_single(f, s, rng);
    case DT_SUNK: return t_sunk(f, s, rng);
    case DT_TRAIL: return t_trail(f, s, rng);
    case DT_PATH: return t_path(f, s, rng);
    case DT_OUTCROP: case DT_HEAP: case DT_BLOCK: case DT_RING: case DT_PIT: return t_anchor(f, s, rng, arg);
    default: return dt_styles2_run(f, s, rng, arg);
    }
}

/* ---------------- details.anchors / details.apply ---------------- */
int dt_anchors_run(Scene *s, const DtList *pool, PyRandom *rng) {
    double K = SC.K, g_, gr; int n, placed = 0, err = 0;
    if (!pool || !pool->n) return 0;
    gr = lf_fpow(K, 0.5, &err); g_ = gr < 1.9 ? gr : 1.9;          /* min(1.9, K ** 0.5) */
    if (ft_true_scale() != 1.0) g_ = 1.3;          /* with a scenery scale a set piece is one size on every base */
    { double v = py_rint(1.6 * (K * K - 1) / g_); n = (int)(v < 16 ? v : 16); if (n < 1) n = 1; }          /* max(1, min(16, round(...))) */
    for (int i = 0; i < n * 3; i++) {
        int64_t j; int r;
        if (placed >= n) break;
        if (py_choice_e(rng, pool->n, &j) != 0) return SC_ERR;
        r = dt_run(pool->v[j], s, rng, g_);
        if (r == 1) placed++;          /* an error is caught (printed) and the loop goes on */
    }
    return placed;
}
int dt_apply(Scene *s, int n, int (*run)(void *ctx, int i, Scene *s, PyRandom *rng, int t), void *ctx, PyRandom *rng, int t) {
    int64_t k, *ix; int64_t lo = 2 + t + (t > 1), hi = 3 + t + (t > 1) * 2; double m; int err = 0, nn;
    if (n <= 0) return 0;
    k = py_randint(rng, lo, hi);
    m = lf_fpow(SC.K, 2, &err); m = m > 1.0 ? m : 1.0;
    { double v = py_rint((double)k * m); k = v < 60 ? (int64_t)v : 60; }          /* min(60, round(k * max(1.0, gg.K ** 2))) */
    ix = (int64_t *)malloc(sizeof(int64_t) * (size_t)(k > n ? k : n) + 8);
    if (k <= n) { py_sample_e(rng, n, k, ix); nn = (int)k; }
    else { py_sample_e(rng, n, n, ix); for (int64_t i = n; i < k; i++) py_choice_e(rng, n, &ix[i]); nn = (int)k; }
    for (int i = 0; i < nn; i++) run(ctx, (int)ix[i], s, rng, t);          /* try: fn(s, rng, t) except: print */
    free(ix);
    return 0;
}

/* ---------------- the constructors ---------------- */
static DtFn *nw(int t, const char *qual, const char *tmpl, const char *kind, const char *shape) {
    DtFn *f = (DtFn *)calloc(1, sizeof *f); bld_keep(f, free);
    f->t = t; f->qual = qual; f->tmpl = tmpl; f->kind = kind ? so_intern(kind) : NULL; f->shape = shape; f->num = num_none();
    return f;
}
static void put_tiers(DtFn *f, const Tier *t, int nt) { f->nt = nt > 4 ? 4 : nt; for (int i = 0; i < f->nt; i++) f->tiers[i] = t[i]; }
static const Tier T_SCATTER[2] = { { { 0.7, NUM_FLOAT, 0 }, { 6, NUM_INT, 0 } }, { { 0.3, NUM_FLOAT, 0 }, { 40, NUM_INT, 0 } } };
static const Tier T_SINGLE[2] = { { { 0.75, NUM_FLOAT, 0 }, { 5, NUM_INT, 0 } }, { { 0.25, NUM_FLOAT, 0 }, { 40, NUM_INT, 0 } } };
static const Tier T_HEAP[3] = { { { 0.45, NUM_FLOAT, 0 }, { 18, NUM_INT, 0 } }, { { 0.35, NUM_FLOAT, 0 }, { 35, NUM_INT, 0 } }, { { 0.2, NUM_FLOAT, 0 }, { 52, NUM_INT, 0 } } };
DtFn *dt_scatter(const LfShape *sh, double n0, double n1, double h0, double h1, const char *kind, const char *how, double spread, const Tier *tiers, int nt) {
    DtFn *f = nw(DT_SCATTER, "scatter", "scatter", kind, sh ? lf_shape_tag(sh->kind) : "");
    if (sh) { f->has_shape = 1; f->sh = *sh; }
    f->n0 = n0; f->n1 = n1; f->h0 = h0; f->h1 = h1; f->k1 = f->kind; f->how = so_intern(how); f->x[0] = spread;
    if (tiers) put_tiers(f, tiers, nt); else put_tiers(f, T_SCATTER, 2);
    return f;
}
DtFn *dt_single(const LfShape *sh, double h0, double h1, const char *kind, const char *how, const LfPat *top, const Tier *tiers, int nt) {
    DtFn *f = nw(DT_SINGLE, "single", "single", kind, sh ? lf_shape_tag(sh->kind) : "");
    if (sh) { f->has_shape = 1; f->sh = *sh; }
    if (top) { f->has_pat = 1; f->pat = *top; }
    f->h0 = h0; f->h1 = h1; f->k1 = f->kind; f->how = so_intern(how);
    if (tiers) put_tiers(f, tiers, nt); else put_tiers(f, T_SINGLE, 2);
    return f;
}
DtFn *dt_sunk(const LfShape *sh, double d0, double d1, const char *kind, double n0, double n1, double spread) {
    DtFn *f = nw(DT_SUNK, "sunk", "sunk", kind, sh ? lf_shape_tag(sh->kind) : "");
    if (sh) { f->has_shape = 1; f->sh = *sh; }
    f->h0 = d0; f->h1 = d1; f->k1 = f->kind; f->n0 = n0; f->n1 = n1; f->x[0] = spread;
    return f;
}
DtFn *dt_trail(double step, double n0, double n1, const LfShape *sh, Num depth, const char *kind) {
    LfShape d = lf_sh(SH_ELL, 0.35, 0.45, 0.6, 0.75, 0); DtFn *f;
    if (!sh) sh = &d;
    f = nw(DT_TRAIL, "trail", "trail", kind, lf_shape_tag(sh->kind));
    f->has_shape = 1; f->sh = *sh; f->x[0] = step; f->n0 = n0; f->n1 = n1; f->num = depth; f->k1 = f->kind;
    return f;
}
DtFn *dt_path(double w, double l0, double l1, Num h, const char *kind, const char *how, double amp) {
    DtFn *f = nw(DT_PATH, "path", "path", kind, "");
    f->x[0] = w; f->x[1] = l0; f->x[2] = l1; f->x[3] = amp; f->num = h; f->k1 = f->kind; f->how = so_intern(how);
    return f;
}
DtFn *dt_a(int t, const char *kind, const char *k2, const LfPat *pat, double n0, double n1, double h0, double h1, const Tier *tiers, int nt) {
    static const char *Q[] = { "a_outcrop", "a_heap", "a_block", "a_ring", "a_pit" };
    DtFn *f = nw(t, Q[t - DT_OUTCROP], NULL, NULL, NULL);
    f->k1 = kind ? so_intern(kind) : NULL; f->k2 = k2 ? so_intern(k2) : NULL;
    if (pat) { f->has_pat = 1; f->pat = *pat; }
    f->n0 = n0; f->n1 = n1; f->h0 = h0; f->h1 = h1;
    if (tiers) put_tiers(f, tiers, nt); else if (t == DT_HEAP) put_tiers(f, T_HEAP, 3);
    return f;
}
static DtFn *dt_new_any(const char *name) { (void)name; return NULL; }

/* ---------------- the tables (lookups) ---------------- */
static const DtList *lfind(const DtList *t, int n, const char *style) { for (int i = 0; i < n; i++) if (!strcmp(t[i].style, style)) return &t[i]; return NULL; }
const DtList *dt_details(const char *style) { return lfind(DT_DETAILS, DT_NDETAILS, style); }
const DtList *dt_anchors(const char *style) { return lfind(DT_ANCHORS, DT_NANCHORS, style); }

void bld_register_details(void) {
    bld_add_any("details", "dt_run", "dt_run", (void *)dt_run);
    bld_add_any("details", "DETAILS", "dt_list", (void *)dt_details);
    bld_add_any("details", "ANCHORS", "dt_list", (void *)dt_anchors);
    bld_add_any("details", "anchors", "dt_anchors_run", (void *)dt_anchors_run);
    bld_add_any("details", "apply", "dt_apply", (void *)dt_apply);
    bld_add_any("details", "_raise", "dt_raise", (void *)dt_raise);
    bld_add_any("details", "scatter", "dt_new", (void *)dt_scatter);
    bld_add_any("details", "single", "dt_new", (void *)dt_single);
    bld_add_any("details", "sunk", "dt_new", (void *)dt_sunk);
    bld_add_any("details", "trail", "dt_new", (void *)dt_trail);
    bld_add_any("details", "path", "dt_new", (void *)dt_path);
    bld_add_any("details", "a_outcrop", "dt_new", (void *)dt_a);          /* dt_a(DT_OUTCROP, ...) for all five */
    bld_add_any("details", "a_heap", "dt_new", (void *)dt_a);
    bld_add_any("details", "a_block", "dt_new", (void *)dt_a);
    bld_add_any("details", "a_ring", "dt_new", (void *)dt_a);
    bld_add_any("details", "a_pit", "dt_new", (void *)dt_a);
    (void)dt_new_any;
}

/* ---- generated by work\cscene\bld_features\gen_dtables.py from details.DETAILS / ANCHORS ---- */
/* @@DTABLES@@ */
static const DtFn D0 = { DT_SCATTER, "scatter", "scatter", "rock", "rock", "rock", NULL, "mound", 1, { 0, { 0.9, 1.4, 5.0, 8.0, 0.0 } }, 0, { 0, { 0.0, 0.0 } }, 3.0, 5.0, 0.6, 1.0, { 1.6, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 2, { { { 0.7, 0, 0 }, { 6.0, 1, 0 } }, { { 0.3, 0, 0 }, { 40.0, 1, 0 } } }, 0 };
static const DtFn D1 = { DT_SCATTER, "scatter", "scatter", "pebble", "rock", "pebble", NULL, "mound", 1, { 0, { 0.5, 0.8, 5.0, 8.0, 0.0 } }, 0, { 0, { 0.0, 0.0 } }, 6.0, 10.0, 0.35, 0.5, { 1.6, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 2, { { { 0.7, 0, 0 }, { 6.0, 1, 0 } }, { { 0.3, 0, 0 }, { 40.0, 1, 0 } } }, 0 };
static const DtFn D2 = { DT_SINGLE, "single", "single", "sandstone", "rock", "sandstone", NULL, "up", 1, { 0, { 2.2, 3.0, 5.0, 8.0, 0.0 } }, 1, { 1, { 1.1, 0.0 } }, 0.0, 0.0, 1.2, 1.9, { 0.0, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 2, { { { 0.75, 0, 0 }, { 5.0, 1, 0 } }, { { 0.25, 0, 0 }, { 40.0, 1, 0 } } }, 0 };
static const DtFn D3 = { DT_SINGLE, "single", "single", "rock", "rock", "rock", NULL, "mound", 1, { 0, { 1.6, 2.2, 5.0, 8.0, 0.0 } }, 0, { 0, { 0.0, 0.0 } }, 0.0, 0.0, 1.0, 1.5, { 0.0, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 2, { { { 0.7, 0, 0 }, { 6.0, 1, 0 } }, { { 0.3, 0, 0 }, { 40.0, 1, 0 } } }, 0 };
static const DtFn D4 = { DT_SCATTER, "scatter", "scatter", "bone", "bone", "bone", NULL, "up", 1, { 6, { 1.6, 2.4, 0.0, 0.0, 0.0 } }, 0, { 0, { 0.0, 0.0 } }, 2.0, 4.0, 0.4, 0.5, { 1.6, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 2, { { { 0.7, 0, 0 }, { 6.0, 1, 0 } }, { { 0.3, 0, 0 }, { 40.0, 1, 0 } } }, 0 };
static const DtFn D5 = { DT_SINGLE, "single", "single", "bone", "ell", "bone", NULL, "mound", 1, { 3, { 1.1, 1.4, 0.7, 0.9, 0.0 } }, 1, { 4, { 0.22, 0.0 } }, 0.0, 0.0, 0.6, 0.8, { 0.0, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 2, { { { 0.75, 0, 0 }, { 5.0, 1, 0 } }, { { 0.25, 0, 0 }, { 40.0, 1, 0 } } }, 0 };
static const DtFn D6 = { DT_SCATTER, "scatter", "scatter", "shrub", "star", "shrub", NULL, "mound", 1, { 7, { 0.9, 1.3, 5.0, 7.0, 0.0 } }, 0, { 0, { 0.0, 0.0 } }, 2.0, 3.0, 0.8, 1.1, { 2.0, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 2, { { { 0.55, 0, 0 }, { 25.0, 1, 0 } }, { { 0.45, 0, 0 }, { 50.0, 1, 0 } } }, 0 };
static const DtFn D7 = { DT_SINGLE, "single", "single", "shrub", "star", "shrub", NULL, "mound", 1, { 7, { 1.6, 2.2, 6.0, 8.0, 0.0 } }, 0, { 0, { 0.0, 0.0 } }, 0.0, 0.0, 1.0, 1.4, { 0.0, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 2, { { { 0.55, 0, 0 }, { 25.0, 1, 0 } }, { { 0.45, 0, 0 }, { 50.0, 1, 0 } } }, 0 };
static const DtFn D8 = { DT_SCATTER, "scatter", "scatter", "wood", "caps", "wood", NULL, "up", 1, { 5, { 2.0, 3.2, 0.5, 0.0, 0.0 } }, 0, { 0, { 0.0, 0.0 } }, 2.0, 3.0, 0.5, 0.6, { 1.6, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 2, { { { 0.7, 0, 0 }, { 6.0, 1, 0 } }, { { 0.3, 0, 0 }, { 40.0, 1, 0 } } }, 0 };
static const DtFn D9 = { DT_SINGLE, "single", "single", "crate", "rect", "crate", NULL, "up", 1, { 4, { 2.8, 3.6, 1.6, 2.0, 0.0 } }, 1, { 1, { 0.9, 0.0 } }, 0.0, 0.0, 1.0, 1.3, { 0.0, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 2, { { { 0.75, 0, 0 }, { 5.0, 1, 0 } }, { { 0.25, 0, 0 }, { 40.0, 1, 0 } } }, 0 };
static const DtFn D10 = { DT_SINGLE, "single", "single", "wood", "disc", "wood", NULL, "up", 1, { 2, { 0.9, 1.1, 24.0, 0.0, 0.0 } }, 1, { 2, { 0.25, 0.0 } }, 0.0, 0.0, 1.4, 1.8, { 0.0, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 2, { { { 0.75, 0, 0 }, { 5.0, 1, 0 } }, { { 0.25, 0, 0 }, { 40.0, 1, 0 } } }, 0 };
static const DtFn D11 = { DT_SUNK, "sunk", "sunk", "water", "blob", "water", NULL, NULL, 1, { 13, { 1.2, 1.8, 0.0, 0.0, 0.0 } }, 0, { 0, { 0.0, 0.0 } }, 1.0, 1.0, 0.3, 0.4, { 0.0, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 0, { { { 0 } } }, 0 };
static const DtFn D12 = { DT_TRAIL, "trail", "trail", "pit", "ell", "pit", NULL, NULL, 1, { 3, { 0.35, 0.45, 0.6, 0.75, 0.0 } }, 0, { 0, { 0.0, 0.0 } }, 5.0, 8.0, 0.0, 0.0, { 1.4, 0.0, 0.0, 0.0 }, { 0.35, NUM_FLOAT, 0 }, 0, { { { 0 } } }, 0 };
static const DtFn D13 = { DT_TRAIL, "trail", "trail", "pit", "disc", "pit", NULL, NULL, 1, { 2, { 0.28, 0.35, 10.0, 0.0, 0.0 } }, 0, { 0, { 0.0, 0.0 } }, 6.0, 9.0, 0.0, 0.0, { 1.1, 0.0, 0.0, 0.0 }, { 0.3, NUM_FLOAT, 0 }, 0, { { { 0 } } }, 0 };
static const DtFn D14 = { DT_PATH, "path", "path", "rut", "", "rut", NULL, "cut", 0, { 0, { 0.0, 0.0, 0.0, 0.0, 0.0 } }, 0, { 0, { 0.0, 0.0 } }, 0.0, 0.0, 0.0, 0.0, { 0.6, 6.0, 10.0, 0.8 }, { 0.3, NUM_FLOAT, 0 }, 0, { { { 0 } } }, 0 };
static const DtFn D15 = { DT_SUNK, "sunk", "sunk", "crack", "wig", "crack", NULL, NULL, 1, { 14, { 5.0, 8.0, 0.45, 0.9, 0.0 } }, 0, { 0, { 0.0, 0.0 } }, 1.0, 1.0, 0.4, 0.5, { 0.0, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 0, { { { 0 } } }, 0 };
static const DtFn D16 = { DT_SUNK, "sunk", "sunk", "pit", "crescent", "pit", NULL, NULL, 1, { 8, { 1.0, 1.4, 0.0, 0.0, 0.0 } }, 0, { 0, { 0.0, 0.0 } }, 1.0, 1.0, 0.3, 0.4, { 0.0, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 0, { { { 0 } } }, 0 };
static const DtFn D17 = { DT_SCATTER, "scatter", "scatter", "rock", "tri", "rock", NULL, "mound", 1, { 10, { 0.7, 1.1, 0.0, 0.0, 0.0 } }, 0, { 0, { 0.0, 0.0 } }, 4.0, 7.0, 0.4, 0.7, { 1.8, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 2, { { { 0.7, 0, 0 }, { 6.0, 1, 0 } }, { { 0.3, 0, 0 }, { 40.0, 1, 0 } } }, 0 };
static const DtFn D18 = { DT_SINGLE, "single", "single", "sandstone", "hex", "sandstone", NULL, "up", 1, { 9, { 1.2, 1.6, 0.0, 0.0, 0.0 } }, 1, { 3, { 0.0, 0.0 } }, 0.0, 0.0, 0.9, 1.3, { 0.0, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 2, { { { 0.75, 0, 0 }, { 5.0, 1, 0 } }, { { 0.25, 0, 0 }, { 40.0, 1, 0 } } }, 0 };
static const DtFn D19 = { DT_SCATTER, "scatter", "scatter", "pebble", "disc", "pebble", NULL, "mound", 1, { 2, { 0.4, 0.55, 12.0, 0.0, 0.0 } }, 0, { 0, { 0.0, 0.0 } }, 5.0, 9.0, 0.3, 0.4, { 1.4, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 2, { { { 0.7, 0, 0 }, { 10.0, 1, 0 } }, { { 0.3, 0, 0 }, { 45.0, 1, 0 } } }, 0 };
static const DtFn D20 = { DT_SINGLE, "single", "single", "wood", "caps", "wood", NULL, "up", 1, { 5, { 4.0, 6.0, 0.9, 0.0, 0.0 } }, 1, { 1, { 1.2, 0.0 } }, 0.0, 0.0, 0.5, 0.7, { 0.0, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 2, { { { 0.75, 0, 0 }, { 5.0, 1, 0 } }, { { 0.25, 0, 0 }, { 40.0, 1, 0 } } }, 0 };
static const DtFn D21 = { DT_SUNK, "sunk", "sunk", "pit", "rect", "pit", NULL, NULL, 1, { 4, { 1.6, 2.4, 1.0, 1.4, 0.0 } }, 0, { 0, { 0.0, 0.0 } }, 1.0, 1.0, 0.5, 0.7, { 0.0, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 0, { { { 0 } } }, 0 };
static const DtFn D22 = { DT_SCATTER, "scatter", "scatter", "wood", "cross", "wood", NULL, "up", 1, { 11, { 1.3, 1.8, 0.4, 0.0, 0.0 } }, 0, { 0, { 0.0, 0.0 } }, 1.0, 2.0, 0.9, 1.2, { 1.6, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 2, { { { 0.7, 0, 0 }, { 6.0, 1, 0 } }, { { 0.3, 0, 0 }, { 40.0, 1, 0 } } }, 0 };
static const DtFn D23 = { DT_SINGLE, "single", "single", "rock", "ring", "rock", NULL, "up", 1, { 12, { 1.2, 1.6, 0.6, 0.0, 0.0 } }, 0, { 0, { 0.0, 0.0 } }, 0.0, 0.0, 0.6, 0.8, { 0.0, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 2, { { { 0.75, 0, 0 }, { 5.0, 1, 0 } }, { { 0.25, 0, 0 }, { 40.0, 1, 0 } } }, 0 };
static const DtFn D24 = { DT_PATH, "path", "path", "rock", "", "rock", NULL, "up", 0, { 0, { 0.0, 0.0, 0.0, 0.0, 0.0 } }, 0, { 0, { 0.0, 0.0 } }, 0.0, 0.0, 0.0, 0.0, { 0.8, 6.0, 9.0, 0.4 }, { 0.35, NUM_FLOAT, 0 }, 0, { { { 0 } } }, 0 };
static const DtFn D25 = { DT_SCATTER, "scatter", "scatter", "bone", "ell", "bone", NULL, "mound", 1, { 3, { 0.5, 0.7, 0.9, 1.2, 0.0 } }, 0, { 0, { 0.0, 0.0 } }, 3.0, 5.0, 0.4, 0.6, { 1.5, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 2, { { { 0.7, 0, 0 }, { 6.0, 1, 0 } }, { { 0.3, 0, 0 }, { 40.0, 1, 0 } } }, 0 };
static const DtFn D26 = { DT_SINGLE, "single", "single", "rock", "rock", "rock", NULL, "up", 1, { 0, { 1.4, 1.8, 6.0, 8.0, 0.0 } }, 1, { 5, { 0.5, 0.8 } }, 0.0, 0.0, 0.9, 1.2, { 0.0, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 2, { { { 0.75, 0, 0 }, { 5.0, 1, 0 } }, { { 0.25, 0, 0 }, { 40.0, 1, 0 } } }, 0 };
static const DtFn D27 = { DT_SUNK, "sunk", "sunk", "pit", "disc", "pit", NULL, NULL, 1, { 2, { 1.4, 1.9, 24.0, 0.0, 0.0 } }, 0, { 0, { 0.0, 0.0 } }, 1.0, 1.0, 0.4, 0.6, { 0.0, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 0, { { { 0 } } }, 0 };
static const DtFn D28 = { DT_SCATTER, "scatter", "scatter", "rock", "crescent", "rock", NULL, "up", 1, { 8, { 0.7, 1.0, 0.0, 0.0, 0.0 } }, 0, { 0, { 0.0, 0.0 } }, 2.0, 4.0, 0.4, 0.5, { 1.6, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 2, { { { 0.7, 0, 0 }, { 6.0, 1, 0 } }, { { 0.3, 0, 0 }, { 40.0, 1, 0 } } }, 0 };
static const DtFn D29 = { DT_SUNK, "sunk", "sunk", "crack", "star", "crack", NULL, NULL, 1, { 7, { 1.2, 1.6, 5.0, 6.0, 0.0 } }, 0, { 0, { 0.0, 0.0 } }, 1.0, 1.0, 0.3, 0.4, { 0.0, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 0, { { { 0 } } }, 0 };
static const DtFn D30 = { DT_SCATTER, "scatter", "scatter", "slate", "rock", "slate", NULL, "mound", 1, { 0, { 0.9, 1.4, 5.0, 8.0, 0.0 } }, 0, { 0, { 0.0, 0.0 } }, 3.0, 5.0, 0.6, 1.0, { 1.6, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 2, { { { 0.7, 0, 0 }, { 6.0, 1, 0 } }, { { 0.3, 0, 0 }, { 40.0, 1, 0 } } }, 0 };
static const DtFn D31 = { DT_SINGLE, "single", "single", "slate", "rock", "slate", NULL, "up", 1, { 0, { 2.0, 2.8, 5.0, 8.0, 0.0 } }, 0, { 0, { 0.0, 0.0 } }, 0.0, 0.0, 1.4, 2.0, { 0.0, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 2, { { { 0.75, 0, 0 }, { 5.0, 1, 0 } }, { { 0.25, 0, 0 }, { 40.0, 1, 0 } } }, 0 };
static const DtFn D32 = { DT_SCATTER, "scatter", "scatter", "grass", "star", "grass", NULL, "mound", 1, { 7, { 1.0, 1.5, 6.0, 8.0, 0.0 } }, 0, { 0, { 0.0, 0.0 } }, 2.0, 4.0, 0.9, 1.3, { 2.0, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 2, { { { 0.55, 0, 0 }, { 25.0, 1, 0 } }, { { 0.45, 0, 0 }, { 50.0, 1, 0 } } }, 0 };
static const DtFn D33 = { DT_SCATTER, "scatter", "scatter", "flower", "disc", "flower", NULL, "mound", 1, { 2, { 0.45, 0.6, 12.0, 0.0, 0.0 } }, 0, { 0, { 0.0, 0.0 } }, 4.0, 7.0, 0.45, 0.6, { 1.2, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 1, { { { 1.0, 0, 0 }, { 30.0, 1, 0 } } }, 0 };
static const DtFn D34 = { DT_SINGLE, "single", "single", "slate", "rect", "slate", NULL, "up", 1, { 4, { 4.0, 6.0, 0.9, 1.2, 0.0 } }, 1, { 1, { 1.0, 0.0 } }, 0.0, 0.0, 1.0, 1.4, { 0.0, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 2, { { { 0.75, 0, 0 }, { 5.0, 1, 0 } }, { { 0.25, 0, 0 }, { 40.0, 1, 0 } } }, 0 };
static const DtFn D35 = { DT_SCATTER, "scatter", "scatter", "slate", "rect", "slate", NULL, "up", 1, { 4, { 1.2, 1.8, 0.8, 1.1, 0.0 } }, 0, { 0, { 0.0, 0.0 } }, 3.0, 5.0, 0.7, 1.0, { 1.8, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 2, { { { 0.7, 0, 0 }, { 6.0, 1, 0 } }, { { 0.3, 0, 0 }, { 40.0, 1, 0 } } }, 0 };
static const DtFn D36 = { DT_SINGLE, "single", "single", "slate", "disc", "slate", NULL, "up", 1, { 2, { 1.2, 1.6, 24.0, 0.0, 0.0 } }, 1, { 2, { 0.3, 0.0 } }, 0.0, 0.0, 1.8, 2.4, { 0.0, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 2, { { { 0.75, 0, 0 }, { 5.0, 1, 0 } }, { { 0.25, 0, 0 }, { 40.0, 1, 0 } } }, 0 };
static const DtFn D37 = { DT_SUNK, "sunk", "sunk", "water", "blob", "water", NULL, NULL, 1, { 13, { 1.4, 2.0, 0.0, 0.0, 0.0 } }, 0, { 0, { 0.0, 0.0 } }, 1.0, 1.0, 0.3, 0.5, { 0.0, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 0, { { { 0 } } }, 0 };
static const DtFn D38 = { DT_TRAIL, "trail", "trail", "pit", "crescent", "pit", NULL, NULL, 1, { 8, { 0.35, 0.45, 0.0, 0.0, 0.0 } }, 0, { 0, { 0.0, 0.0 } }, 5.0, 8.0, 0.0, 0.0, { 1.3, 0.0, 0.0, 0.0 }, { 0.3, NUM_FLOAT, 0 }, 0, { { { 0 } } }, 0 };
static const DtFn D39 = { DT_PATH, "path", "path", "rut", "", "rut", NULL, "cut", 0, { 0, { 0.0, 0.0, 0.0, 0.0, 0.0 } }, 0, { 0, { 0.0, 0.0 } }, 0.0, 0.0, 0.0, 0.0, { 0.7, 6.0, 10.0, 0.7 }, { 0.35, NUM_FLOAT, 0 }, 0, { { { 0 } } }, 0 };
static const DtFn D40 = { DT_SCATTER, "scatter", "scatter", "fern", "ell", "fern", NULL, "mound", 1, { 3, { 0.6, 0.9, 0.4, 0.6, 0.0 } }, 0, { 0, { 0.0, 0.0 } }, 4.0, 7.0, 0.5, 0.7, { 1.6, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 2, { { { 0.55, 0, 0 }, { 25.0, 1, 0 } }, { { 0.45, 0, 0 }, { 50.0, 1, 0 } } }, 0 };
static const DtFn D41 = { DT_SINGLE, "single", "single", "shrub", "star", "shrub", NULL, "mound", 1, { 7, { 1.6, 2.2, 7.0, 9.0, 0.0 } }, 0, { 0, { 0.0, 0.0 } }, 0.0, 0.0, 1.2, 1.6, { 0.0, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 2, { { { 0.55, 0, 0 }, { 25.0, 1, 0 } }, { { 0.45, 0, 0 }, { 50.0, 1, 0 } } }, 0 };
static const DtFn D42 = { DT_SCATTER, "scatter", "scatter", "wood", "caps", "wood", NULL, "up", 1, { 5, { 2.4, 3.4, 0.55, 0.0, 0.0 } }, 0, { 0, { 0.0, 0.0 } }, 1.0, 3.0, 0.5, 0.6, { 1.6, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 2, { { { 0.7, 0, 0 }, { 6.0, 1, 0 } }, { { 0.3, 0, 0 }, { 40.0, 1, 0 } } }, 0 };
static const DtFn D43 = { DT_SINGLE, "single", "single", "crate", "rect", "crate", NULL, "up", 1, { 4, { 2.4, 3.0, 1.4, 1.8, 0.0 } }, 1, { 1, { 0.8, 0.0 } }, 0.0, 0.0, 1.0, 1.3, { 0.0, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 2, { { { 0.75, 0, 0 }, { 5.0, 1, 0 } }, { { 0.25, 0, 0 }, { 40.0, 1, 0 } } }, 0 };
static const DtFn D44 = { DT_SINGLE, "single", "single", "standing", "rock", "standing", NULL, "up", 1, { 0, { 1.4, 1.8, 5.0, 8.0, 0.0 } }, 1, { 6, { 0.0, 0.0 } }, 0.0, 0.0, 2.0, 2.6, { 0.0, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 2, { { { 0.75, 0, 0 }, { 5.0, 1, 0 } }, { { 0.25, 0, 0 }, { 40.0, 1, 0 } } }, 0 };
static const DtFn D45 = { DT_SINGLE, "single", "single", "slate", "hex", "slate", NULL, "up", 1, { 9, { 1.1, 1.4, 0.0, 0.0, 0.0 } }, 1, { 6, { 0.0, 0.0 } }, 0.0, 0.0, 0.8, 1.0, { 0.0, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 2, { { { 0.75, 0, 0 }, { 5.0, 1, 0 } }, { { 0.25, 0, 0 }, { 40.0, 1, 0 } } }, 0 };
static const DtFn D46 = { DT_SCATTER, "scatter", "scatter", "pebble", "round", "pebble", NULL, "mound", 1, { 1, { 0.55, 0.8, 0.0, 0.0, 0.0 } }, 0, { 0, { 0.0, 0.0 } }, 5.0, 9.0, 0.35, 0.5, { 1.6, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 2, { { { 0.7, 0, 0 }, { 10.0, 1, 0 } }, { { 0.3, 0, 0 }, { 45.0, 1, 0 } } }, 0 };
static const DtFn D47 = { DT_SUNK, "sunk", "sunk", "crack", "wig", "crack", NULL, NULL, 1, { 14, { 5.0, 9.0, 0.5, 0.8, 0.0 } }, 0, { 0, { 0.0, 0.0 } }, 1.0, 1.0, 0.4, 0.5, { 0.0, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 0, { { { 0 } } }, 0 };
static const DtFn D48 = { DT_SCATTER, "scatter", "scatter", "slate", "tri", "slate", NULL, "mound", 1, { 10, { 0.8, 1.2, 0.0, 0.0, 0.0 } }, 0, { 0, { 0.0, 0.0 } }, 3.0, 5.0, 0.5, 0.8, { 1.8, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 2, { { { 0.7, 0, 0 }, { 6.0, 1, 0 } }, { { 0.3, 0, 0 }, { 40.0, 1, 0 } } }, 0 };
static const DtFn D49 = { DT_SINGLE, "single", "single", "slate", "ring", "slate", NULL, "up", 1, { 12, { 1.6, 2.0, 0.7, 0.0, 0.0 } }, 0, { 0, { 0.0, 0.0 } }, 0.0, 0.0, 0.7, 0.9, { 0.0, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 2, { { { 0.75, 0, 0 }, { 5.0, 1, 0 } }, { { 0.25, 0, 0 }, { 40.0, 1, 0 } } }, 0 };
static const DtFn D50 = { DT_SCATTER, "scatter", "scatter", "bone", "bone", "bone", NULL, "up", 1, { 6, { 1.4, 2.0, 0.0, 0.0, 0.0 } }, 0, { 0, { 0.0, 0.0 } }, 1.0, 3.0, 0.4, 0.5, { 1.6, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 2, { { { 0.7, 0, 0 }, { 6.0, 1, 0 } }, { { 0.3, 0, 0 }, { 40.0, 1, 0 } } }, 0 };
static const DtFn D51 = { DT_SINGLE, "single", "single", "wood", "disc", "wood", NULL, "up", 1, { 2, { 0.8, 1.0, 20.0, 0.0, 0.0 } }, 1, { 2, { 0.25, 0.0 } }, 0.0, 0.0, 1.4, 1.8, { 0.0, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 2, { { { 0.75, 0, 0 }, { 5.0, 1, 0 } }, { { 0.25, 0, 0 }, { 40.0, 1, 0 } } }, 0 };
static const DtFn D52 = { DT_SUNK, "sunk", "sunk", "pit", "rect", "pit", NULL, NULL, 1, { 4, { 2.0, 3.0, 1.2, 1.6, 0.0 } }, 0, { 0, { 0.0, 0.0 } }, 1.0, 1.0, 0.4, 0.6, { 0.0, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 0, { { { 0 } } }, 0 };
static const DtFn D53 = { DT_SCATTER, "scatter", "scatter", "moss", "blob", "moss", NULL, "mound", 1, { 13, { 0.7, 1.0, 0.0, 0.0, 0.0 } }, 0, { 0, { 0.0, 0.0 } }, 2.0, 4.0, 0.5, 0.7, { 1.8, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 2, { { { 0.55, 0, 0 }, { 25.0, 1, 0 } }, { { 0.45, 0, 0 }, { 50.0, 1, 0 } } }, 0 };
static const DtFn D54 = { DT_PATH, "path", "path", "slate", "", "slate", NULL, "up", 0, { 0, { 0.0, 0.0, 0.0, 0.0, 0.0 } }, 0, { 0, { 0.0, 0.0 } }, 0.0, 0.0, 0.0, 0.0, { 0.8, 5.0, 8.0, 0.3 }, { 0.4, NUM_FLOAT, 0 }, 0, { { { 0 } } }, 0 };
static const DtFn D55 = { DT_SCATTER, "scatter", "scatter", "heather", "star", "heather", NULL, "mound", 1, { 7, { 0.6, 0.8, 5.0, 6.0, 0.0 } }, 0, { 0, { 0.0, 0.0 } }, 4.0, 7.0, 0.5, 0.7, { 1.6, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 2, { { { 0.55, 0, 0 }, { 25.0, 1, 0 } }, { { 0.45, 0, 0 }, { 50.0, 1, 0 } } }, 0 };
static const DtFn D56 = { DT_SINGLE, "single", "single", "slate", "rect", "slate", NULL, "up", 1, { 4, { 3.0, 4.0, 2.0, 2.6, 0.0 } }, 1, { 3, { 0.0, 0.0 } }, 0.0, 0.0, 0.5, 0.7, { 0.0, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 2, { { { 0.75, 0, 0 }, { 5.0, 1, 0 } }, { { 0.25, 0, 0 }, { 40.0, 1, 0 } } }, 0 };
static const DtFn D57 = { DT_SUNK, "sunk", "sunk", "pit", "disc", "pit", NULL, NULL, 1, { 2, { 0.4, 0.55, 10.0, 0.0, 0.0 } }, 0, { 0, { 0.0, 0.0 } }, 4.0, 7.0, 0.3, 0.4, { 1.5, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 0, { { { 0 } } }, 0 };
static const DtFn D58 = { DT_SCATTER, "scatter", "scatter", "slate", "crescent", "slate", NULL, "up", 1, { 8, { 0.8, 1.1, 0.0, 0.0, 0.0 } }, 0, { 0, { 0.0, 0.0 } }, 2.0, 3.0, 0.5, 0.7, { 1.6, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 2, { { { 0.7, 0, 0 }, { 6.0, 1, 0 } }, { { 0.3, 0, 0 }, { 40.0, 1, 0 } } }, 0 };
static const DtFn D59 = { DT_SINGLE, "single", "single", "moss", "ell", "moss", NULL, "mound", 1, { 3, { 1.4, 1.8, 0.8, 1.0, 0.0 } }, 0, { 0, { 0.0, 0.0 } }, 0.0, 0.0, 0.9, 1.2, { 0.0, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 2, { { { 0.55, 0, 0 }, { 25.0, 1, 0 } }, { { 0.45, 0, 0 }, { 50.0, 1, 0 } } }, 0 };
static const DtFn D60 = { DT_SCATTER, "scatter", "scatter", "crystal", "hex", "crystal", NULL, "up", 1, { 9, { 0.7, 1.0, 0.0, 0.0, 0.0 } }, 0, { 0, { 0.0, 0.0 } }, 3.0, 6.0, 0.8, 1.4, { 1.6, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 2, { { { 0.7, 0, 0 }, { 6.0, 1, 0 } }, { { 0.3, 0, 0 }, { 40.0, 1, 0 } } }, 0 };
static const DtFn D61 = { DT_SINGLE, "single", "single", "crystal", "hex", "crystal", NULL, "up", 1, { 9, { 1.4, 1.8, 0.0, 0.0, 0.0 } }, 1, { 6, { 0.0, 0.0 } }, 0.0, 0.0, 2.0, 2.6, { 0.0, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 2, { { { 0.75, 0, 0 }, { 5.0, 1, 0 } }, { { 0.25, 0, 0 }, { 40.0, 1, 0 } } }, 0 };
static const DtFn D62 = { DT_SCATTER, "scatter", "scatter", "crystal", "tri", "crystal", NULL, "mound", 1, { 10, { 0.6, 0.9, 0.0, 0.0, 0.0 } }, 0, { 0, { 0.0, 0.0 } }, 4.0, 7.0, 0.8, 1.2, { 1.6, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 2, { { { 0.6, 0, 0 }, { 4.0, 1, 0 } }, { { 0.4, 0, 0 }, { 35.0, 1, 0 } } }, 0 };
static const DtFn D63 = { DT_SINGLE, "single", "single", "salt", "disc", "salt", NULL, "up", 1, { 2, { 1.6, 2.0, 24.0, 0.0, 0.0 } }, 1, { 6, { 0.0, 0.0 } }, 0.0, 0.0, 0.5, 0.7, { 0.0, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 2, { { { 0.75, 0, 0 }, { 5.0, 1, 0 } }, { { 0.25, 0, 0 }, { 40.0, 1, 0 } } }, 0 };
static const DtFn D64 = { DT_SCATTER, "scatter", "scatter", "rock", "rock", "rock", NULL, "mound", 1, { 0, { 0.9, 1.4, 5.0, 8.0, 0.0 } }, 0, { 0, { 0.0, 0.0 } }, 3.0, 5.0, 0.6, 0.9, { 1.6, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 2, { { { 0.7, 0, 0 }, { 6.0, 1, 0 } }, { { 0.3, 0, 0 }, { 40.0, 1, 0 } } }, 0 };
static const DtFn D65 = { DT_SINGLE, "single", "single", "ruin", "rect", "ruin", NULL, "up", 1, { 4, { 3.6, 4.6, 1.2, 1.6, 0.0 } }, 1, { 6, { 0.0, 0.0 } }, 0.0, 0.0, 0.8, 1.1, { 0.0, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 2, { { { 0.75, 0, 0 }, { 5.0, 1, 0 } }, { { 0.25, 0, 0 }, { 40.0, 1, 0 } } }, 0 };
static const DtFn D66 = { DT_SINGLE, "single", "single", "column", "disc", "column", NULL, "up", 1, { 2, { 1.0, 1.3, 24.0, 0.0, 0.0 } }, 1, { 2, { 0.25, 0.0 } }, 0.0, 0.0, 2.0, 2.8, { 0.0, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 2, { { { 0.75, 0, 0 }, { 5.0, 1, 0 } }, { { 0.25, 0, 0 }, { 40.0, 1, 0 } } }, 0 };
static const DtFn D67 = { DT_SCATTER, "scatter", "scatter", "ruin", "rect", "ruin", NULL, "up", 1, { 4, { 1.6, 2.2, 1.0, 1.3, 0.0 } }, 0, { 0, { 0.0, 0.0 } }, 2.0, 4.0, 0.6, 0.9, { 2.0, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 2, { { { 0.7, 0, 0 }, { 6.0, 1, 0 } }, { { 0.3, 0, 0 }, { 40.0, 1, 0 } } }, 0 };
static const DtFn D68 = { DT_SUNK, "sunk", "sunk", "water", "blob", "water", NULL, NULL, 1, { 13, { 1.4, 2.0, 0.0, 0.0, 0.0 } }, 0, { 0, { 0.0, 0.0 } }, 1.0, 1.0, 0.4, 0.6, { 0.0, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 0, { { { 0 } } }, 0 };
static const DtFn D69 = { DT_SUNK, "sunk", "sunk", "slot", "ring", "slot", NULL, NULL, 1, { 12, { 1.6, 2.2, 0.45, 0.0, 0.0 } }, 0, { 0, { 0.0, 0.0 } }, 1.0, 1.0, 0.4, 0.5, { 0.0, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 0, { { { 0 } } }, 0 };
static const DtFn D70 = { DT_SUNK, "sunk", "sunk", "crack", "star", "crack", NULL, NULL, 1, { 7, { 1.4, 1.9, 5.0, 7.0, 0.0 } }, 0, { 0, { 0.0, 0.0 } }, 1.0, 1.0, 0.3, 0.4, { 0.0, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 0, { { { 0 } } }, 0 };
static const DtFn D71 = { DT_SINGLE, "single", "single", "crystal", "star", "crystal", NULL, "up", 1, { 7, { 1.4, 1.8, 4.0, 6.0, 0.0 } }, 0, { 0, { 0.0, 0.0 } }, 0.0, 0.0, 1.6, 2.2, { 0.0, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 2, { { { 0.75, 0, 0 }, { 5.0, 1, 0 } }, { { 0.25, 0, 0 }, { 40.0, 1, 0 } } }, 0 };
static const DtFn D72 = { DT_SCATTER, "scatter", "scatter", "shrub", "star", "shrub", NULL, "mound", 1, { 7, { 0.8, 1.1, 6.0, 8.0, 0.0 } }, 0, { 0, { 0.0, 0.0 } }, 2.0, 4.0, 0.8, 1.1, { 1.8, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 2, { { { 0.55, 0, 0 }, { 25.0, 1, 0 } }, { { 0.45, 0, 0 }, { 50.0, 1, 0 } } }, 0 };
static const DtFn D73 = { DT_TRAIL, "trail", "trail", "pit", "tri", "pit", NULL, NULL, 1, { 10, { 0.4, 0.5, 0.0, 0.0, 0.0 } }, 0, { 0, { 0.0, 0.0 } }, 5.0, 7.0, 0.0, 0.0, { 1.3, 0.0, 0.0, 0.0 }, { 0.3, NUM_FLOAT, 0 }, 0, { { { 0 } } }, 0 };
static const DtFn D74 = { DT_PATH, "path", "path", "glyph", "", "glyph", NULL, "cut", 0, { 0, { 0.0, 0.0, 0.0, 0.0, 0.0 } }, 0, { 0, { 0.0, 0.0 } }, 0.0, 0.0, 0.0, 0.0, { 0.6, 6.0, 10.0, 0.3 }, { 0.3, NUM_FLOAT, 0 }, 0, { { { 0 } } }, 0 };
static const DtFn D75 = { DT_SCATTER, "scatter", "scatter", "salt", "disc", "salt", NULL, "mound", 1, { 2, { 0.4, 0.6, 12.0, 0.0, 0.0 } }, 0, { 0, { 0.0, 0.0 } }, 5.0, 8.0, 0.4, 0.6, { 1.4, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 2, { { { 0.7, 0, 0 }, { 10.0, 1, 0 } }, { { 0.3, 0, 0 }, { 45.0, 1, 0 } } }, 0 };
static const DtFn D76 = { DT_SINGLE, "single", "single", "ruin", "cross", "ruin", NULL, "up", 1, { 11, { 2.4, 3.2, 0.8, 0.0, 0.0 } }, 0, { 0, { 0.0, 0.0 } }, 0.0, 0.0, 0.6, 0.8, { 0.0, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 2, { { { 0.75, 0, 0 }, { 5.0, 1, 0 } }, { { 0.25, 0, 0 }, { 40.0, 1, 0 } } }, 0 };
static const DtFn D77 = { DT_SINGLE, "single", "single", "ruin", "ring", "ruin", NULL, "up", 1, { 12, { 1.4, 1.8, 0.6, 0.0, 0.0 } }, 0, { 0, { 0.0, 0.0 } }, 0.0, 0.0, 0.8, 1.0, { 0.0, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 2, { { { 0.75, 0, 0 }, { 5.0, 1, 0 } }, { { 0.25, 0, 0 }, { 40.0, 1, 0 } } }, 0 };
static const DtFn D78 = { DT_SCATTER, "scatter", "scatter", "crystal", "crescent", "crystal", NULL, "up", 1, { 8, { 0.8, 1.2, 0.0, 0.0, 0.0 } }, 0, { 0, { 0.0, 0.0 } }, 2.0, 3.0, 0.6, 0.8, { 1.6, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 2, { { { 0.7, 0, 0 }, { 6.0, 1, 0 } }, { { 0.3, 0, 0 }, { 40.0, 1, 0 } } }, 0 };
static const DtFn D79 = { DT_SINGLE, "single", "single", "pavers", "rect", "pavers", NULL, "up", 1, { 4, { 2.6, 3.2, 2.6, 3.2, 0.0 } }, 1, { 3, { 0.0, 0.0 } }, 0.0, 0.0, 0.4, 0.5, { 0.0, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 2, { { { 0.75, 0, 0 }, { 5.0, 1, 0 } }, { { 0.25, 0, 0 }, { 40.0, 1, 0 } } }, 0 };
static const DtFn D80 = { DT_SUNK, "sunk", "sunk", "pit", "hex", "pit", NULL, NULL, 1, { 9, { 1.1, 1.5, 0.0, 0.0, 0.0 } }, 0, { 0, { 0.0, 0.0 } }, 1.0, 1.0, 0.4, 0.6, { 0.0, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 0, { { { 0 } } }, 0 };
static const DtFn D81 = { DT_SCATTER, "scatter", "scatter", "bone", "bone", "bone", NULL, "up", 1, { 6, { 1.6, 2.2, 0.0, 0.0, 0.0 } }, 0, { 0, { 0.0, 0.0 } }, 1.0, 3.0, 0.4, 0.5, { 1.6, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 2, { { { 0.7, 0, 0 }, { 6.0, 1, 0 } }, { { 0.3, 0, 0 }, { 40.0, 1, 0 } } }, 0 };
static const DtFn D82 = { DT_SINGLE, "single", "single", "rock", "ell", "rock", NULL, "mound", 1, { 3, { 1.5, 1.9, 0.9, 1.1, 0.0 } }, 0, { 0, { 0.0, 0.0 } }, 0.0, 0.0, 1.2, 1.6, { 0.0, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 2, { { { 0.7, 0, 0 }, { 6.0, 1, 0 } }, { { 0.3, 0, 0 }, { 40.0, 1, 0 } } }, 0 };
static const DtFn D83 = { DT_SUNK, "sunk", "sunk", "pit", "disc", "pit", NULL, NULL, 1, { 2, { 0.35, 0.5, 10.0, 0.0, 0.0 } }, 0, { 0, { 0.0, 0.0 } }, 5.0, 8.0, 0.3, 0.4, { 1.6, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 0, { { { 0 } } }, 0 };
static const DtFn D84 = { DT_SINGLE, "single", "single", "obelisk", "tri", "obelisk", NULL, "up", 1, { 10, { 1.6, 2.0, 0.0, 0.0, 0.0 } }, 1, { 6, { 0.0, 0.0 } }, 0.0, 0.0, 1.4, 1.8, { 0.0, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 2, { { { 0.75, 0, 0 }, { 5.0, 1, 0 } }, { { 0.25, 0, 0 }, { 40.0, 1, 0 } } }, 0 };
static const DtFn D85 = { DT_PATH, "path", "path", "crystal", "", "crystal", NULL, "up", 0, { 0, { 0.0, 0.0, 0.0, 0.0, 0.0 } }, 0, { 0, { 0.0, 0.0 } }, 0.0, 0.0, 0.0, 0.0, { 0.8, 5.0, 8.0, 0.3 }, { 0.35, NUM_FLOAT, 0 }, 0, { { { 0 } } }, 0 };
static const DtFn D86 = { DT_SCATTER, "scatter", "scatter", "rock", "ell", "rock", NULL, "mound", 1, { 3, { 0.6, 0.8, 0.9, 1.2, 0.0 } }, 0, { 0, { 0.0, 0.0 } }, 3.0, 5.0, 0.5, 0.7, { 1.6, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 2, { { { 0.7, 0, 0 }, { 6.0, 1, 0 } }, { { 0.3, 0, 0 }, { 40.0, 1, 0 } } }, 0 };
static const DtFn D87 = { DT_SINGLE, "single", "single", "pavers", "hex", "pavers", NULL, "up", 1, { 9, { 1.0, 1.3, 0.0, 0.0, 0.0 } }, 1, { 4, { 0.25, 0.0 } }, 0.0, 0.0, 0.6, 0.8, { 0.0, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 2, { { { 0.75, 0, 0 }, { 5.0, 1, 0 } }, { { 0.25, 0, 0 }, { 40.0, 1, 0 } } }, 0 };
static const DtFn D88 = { DT_SUNK, "sunk", "sunk", "crack", "wig", "crack", NULL, NULL, 1, { 14, { 5.0, 8.0, 0.4, 0.6, 0.0 } }, 0, { 0, { 0.0, 0.0 } }, 1.0, 1.0, 0.4, 0.5, { 0.0, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 0, { { { 0 } } }, 0 };
static const DtFn D89 = { DT_SINGLE, "single", "single", "ruin", "disc", "ruin", NULL, "up", 1, { 2, { 1.4, 1.8, 24.0, 0.0, 0.0 } }, 1, { 6, { 0.0, 0.0 } }, 0.0, 0.0, 0.5, 0.7, { 0.0, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 2, { { { 0.75, 0, 0 }, { 5.0, 1, 0 } }, { { 0.25, 0, 0 }, { 40.0, 1, 0 } } }, 0 };
static const DtFn D90 = { DT_SINGLE, "single", "single", "crate", "rect", "crate", NULL, "up", 1, { 4, { 2.4, 3.2, 1.6, 2.0, 0.0 } }, 1, { 1, { 0.8, 0.0 } }, 0.0, 0.0, 1.0, 1.4, { 0.0, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 2, { { { 0.75, 0, 0 }, { 5.0, 1, 0 } }, { { 0.25, 0, 0 }, { 40.0, 1, 0 } } }, 0 };
static const DtFn D91 = { DT_SCATTER, "scatter", "scatter", "crate", "rect", "crate", NULL, "up", 1, { 4, { 1.2, 1.6, 1.0, 1.3, 0.0 } }, 0, { 0, { 0.0, 0.0 } }, 2.0, 3.0, 0.8, 1.1, { 1.8, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 2, { { { 0.7, 0, 0 }, { 6.0, 1, 0 } }, { { 0.3, 0, 0 }, { 40.0, 1, 0 } } }, 0 };
static const DtFn D92 = { DT_SINGLE, "single", "single", "metal", "disc", "metal", NULL, "up", 1, { 2, { 1.0, 1.3, 24.0, 0.0, 0.0 } }, 1, { 2, { 0.25, 0.0 } }, 0.0, 0.0, 1.6, 2.0, { 0.0, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 2, { { { 0.75, 0, 0 }, { 5.0, 1, 0 } }, { { 0.25, 0, 0 }, { 40.0, 1, 0 } } }, 0 };
static const DtFn D93 = { DT_SCATTER, "scatter", "scatter", "metal", "disc", "metal", NULL, "up", 1, { 2, { 0.6, 0.8, 20.0, 0.0, 0.0 } }, 0, { 0, { 0.0, 0.0 } }, 2.0, 4.0, 1.0, 1.4, { 1.6, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 2, { { { 0.7, 0, 0 }, { 6.0, 1, 0 } }, { { 0.3, 0, 0 }, { 40.0, 1, 0 } } }, 0 };
static const DtFn D94 = { DT_SINGLE, "single", "single", "metal", "rect", "metal", NULL, "up", 1, { 4, { 3.0, 4.0, 1.2, 1.6, 0.0 } }, 1, { 0, { 3.0, 0.35 } }, 0.0, 0.0, 0.5, 0.7, { 0.0, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 2, { { { 0.75, 0, 0 }, { 5.0, 1, 0 } }, { { 0.25, 0, 0 }, { 40.0, 1, 0 } } }, 0 };
static const DtFn D95 = { DT_SINGLE, "single", "single", "plate", "rect", "plate", NULL, "up", 1, { 4, { 2.6, 3.2, 2.6, 3.2, 0.0 } }, 1, { 4, { 0.25, 0.0 } }, 0.0, 0.0, 0.3, 0.4, { 0.0, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 2, { { { 0.75, 0, 0 }, { 5.0, 1, 0 } }, { { 0.25, 0, 0 }, { 40.0, 1, 0 } } }, 0 };
static const DtFn D96 = { DT_PATH, "path", "path", "cable", "", "cable", NULL, "up", 0, { 0, { 0.0, 0.0, 0.0, 0.0, 0.0 } }, 0, { 0, { 0.0, 0.0 } }, 0.0, 0.0, 0.0, 0.0, { 0.6, 7.0, 11.0, 0.9 }, { 0.4, NUM_FLOAT, 0 }, 0, { { { 0 } } }, 0 };
static const DtFn D97 = { DT_PATH, "path", "path", "pipe", "", "pipe", NULL, "up", 0, { 0, { 0.0, 0.0, 0.0, 0.0, 0.0 } }, 0, { 0, { 0.0, 0.0 } }, 0.0, 0.0, 0.0, 0.0, { 0.9, 6.0, 10.0, 0.15 }, { 0.5, NUM_FLOAT, 0 }, 0, { { { 0 } } }, 0 };
static const DtFn D98 = { DT_SCATTER, "scatter", "scatter", "metal", "caps", "metal", NULL, "up", 1, { 5, { 0.6, 0.9, 0.3, 0.0, 0.0 } }, 0, { 0, { 0.0, 0.0 } }, 5.0, 9.0, 0.3, 0.35, { 1.4, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 2, { { { 0.7, 0, 0 }, { 6.0, 1, 0 } }, { { 0.3, 0, 0 }, { 40.0, 1, 0 } } }, 0 };
static const DtFn D99 = { DT_SUNK, "sunk", "sunk", "slot", "rect", "slot", NULL, NULL, 1, { 4, { 2.0, 2.8, 1.2, 1.6, 0.0 } }, 0, { 0, { 0.0, 0.0 } }, 1.0, 1.0, 0.4, 0.6, { 0.0, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 0, { { { 0 } } }, 0 };
static const DtFn D100 = { DT_SUNK, "sunk", "sunk", "slot", "disc", "slot", NULL, NULL, 1, { 2, { 1.0, 1.4, 24.0, 0.0, 0.0 } }, 0, { 0, { 0.0, 0.0 } }, 1.0, 1.0, 0.4, 0.5, { 0.0, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 0, { { { 0 } } }, 0 };
static const DtFn D101 = { DT_SUNK, "sunk", "sunk", "slot", "ring", "slot", NULL, NULL, 1, { 12, { 1.2, 1.5, 0.35, 0.0, 0.0 } }, 0, { 0, { 0.0, 0.0 } }, 1.0, 1.0, 0.3, 0.4, { 0.0, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 0, { { { 0 } } }, 0 };
static const DtFn D102 = { DT_SINGLE, "single", "single", "metal", "hex", "metal", NULL, "up", 1, { 9, { 1.0, 1.3, 0.0, 0.0, 0.0 } }, 1, { 5, { 0.35, 0.5 } }, 0.0, 0.0, 0.8, 1.1, { 0.0, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 2, { { { 0.75, 0, 0 }, { 5.0, 1, 0 } }, { { 0.25, 0, 0 }, { 40.0, 1, 0 } } }, 0 };
static const DtFn D103 = { DT_SCATTER, "scatter", "scatter", "metal", "hex", "metal", NULL, "up", 1, { 9, { 0.45, 0.6, 0.0, 0.0, 0.0 } }, 0, { 0, { 0.0, 0.0 } }, 3.0, 6.0, 0.4, 0.5, { 1.4, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 2, { { { 0.7, 0, 0 }, { 6.0, 1, 0 } }, { { 0.3, 0, 0 }, { 40.0, 1, 0 } } }, 0 };
static const DtFn D104 = { DT_SINGLE, "single", "single", "metal", "rect", "metal", NULL, "up", 1, { 4, { 1.8, 2.4, 1.8, 2.4, 0.0 } }, 1, { 3, { 0.0, 0.0 } }, 0.0, 0.0, 1.2, 1.6, { 0.0, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 2, { { { 0.75, 0, 0 }, { 5.0, 1, 0 } }, { { 0.25, 0, 0 }, { 40.0, 1, 0 } } }, 0 };
static const DtFn D105 = { DT_SUNK, "sunk", "sunk", "water", "blob", "water", NULL, NULL, 1, { 13, { 1.2, 1.7, 0.0, 0.0, 0.0 } }, 0, { 0, { 0.0, 0.0 } }, 1.0, 1.0, 0.25, 0.35, { 0.0, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 0, { { { 0 } } }, 0 };
static const DtFn D106 = { DT_TRAIL, "trail", "trail", "pit", "disc", "pit", NULL, NULL, 1, { 2, { 0.25, 0.32, 10.0, 0.0, 0.0 } }, 0, { 0, { 0.0, 0.0 } }, 5.0, 8.0, 0.0, 0.0, { 1.0, 0.0, 0.0, 0.0 }, { 0.4, NUM_FLOAT, 0 }, 0, { { { 0 } } }, 0 };
static const DtFn D107 = { DT_SCATTER, "scatter", "scatter", "metal", "tri", "metal", NULL, "up", 1, { 10, { 0.6, 0.9, 0.0, 0.0, 0.0 } }, 0, { 0, { 0.0, 0.0 } }, 3.0, 6.0, 0.3, 0.4, { 1.6, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 2, { { { 0.7, 0, 0 }, { 6.0, 1, 0 } }, { { 0.3, 0, 0 }, { 40.0, 1, 0 } } }, 0 };
static const DtFn D108 = { DT_SINGLE, "single", "single", "pipe", "caps", "pipe", NULL, "up", 1, { 5, { 4.0, 6.0, 1.0, 0.0, 0.0 } }, 1, { 1, { 1.4, 0.0 } }, 0.0, 0.0, 0.7, 0.9, { 0.0, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 2, { { { 0.75, 0, 0 }, { 5.0, 1, 0 } }, { { 0.25, 0, 0 }, { 40.0, 1, 0 } } }, 0 };
static const DtFn D109 = { DT_SINGLE, "single", "single", "metal", "ring", "metal", NULL, "up", 1, { 12, { 1.2, 1.5, 0.45, 0.0, 0.0 } }, 0, { 0, { 0.0, 0.0 } }, 0.0, 0.0, 0.5, 0.7, { 0.0, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 2, { { { 0.75, 0, 0 }, { 5.0, 1, 0 } }, { { 0.25, 0, 0 }, { 40.0, 1, 0 } } }, 0 };
static const DtFn D110 = { DT_SCATTER, "scatter", "scatter", "metal", "rect", "metal", NULL, "up", 1, { 4, { 0.9, 1.2, 0.6, 0.8, 0.0 } }, 0, { 0, { 0.0, 0.0 } }, 3.0, 5.0, 0.5, 0.7, { 1.5, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 2, { { { 0.7, 0, 0 }, { 6.0, 1, 0 } }, { { 0.3, 0, 0 }, { 40.0, 1, 0 } } }, 0 };
static const DtFn D111 = { DT_SINGLE, "single", "single", "metal", "cross", "metal", NULL, "up", 1, { 11, { 2.6, 3.2, 0.7, 0.0, 0.0 } }, 0, { 0, { 0.0, 0.0 } }, 0.0, 0.0, 0.4, 0.5, { 0.0, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 2, { { { 0.75, 0, 0 }, { 5.0, 1, 0 } }, { { 0.25, 0, 0 }, { 40.0, 1, 0 } } }, 0 };
static const DtFn D112 = { DT_SUNK, "sunk", "sunk", "slot", "rect", "slot", NULL, NULL, 1, { 4, { 3.0, 4.0, 0.5, 0.7, 0.0 } }, 0, { 0, { 0.0, 0.0 } }, 2.0, 3.0, 0.4, 0.5, { 1.2, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 0, { { { 0 } } }, 0 };
static const DtFn D113 = { DT_SINGLE, "single", "single", "metal", "disc", "metal", NULL, "up", 1, { 2, { 0.7, 0.9, 20.0, 0.0, 0.0 } }, 1, { 2, { 0.2, 0.0 } }, 0.0, 0.0, 2.0, 2.6, { 0.0, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 2, { { { 0.75, 0, 0 }, { 5.0, 1, 0 } }, { { 0.25, 0, 0 }, { 40.0, 1, 0 } } }, 0 };
static const DtFn D114 = { DT_SCATTER, "scatter", "scatter", "bone", "bone", "bone", NULL, "up", 1, { 6, { 1.2, 1.6, 0.0, 0.0, 0.0 } }, 0, { 0, { 0.0, 0.0 } }, 1.0, 2.0, 0.35, 0.45, { 1.6, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 2, { { { 0.7, 0, 0 }, { 6.0, 1, 0 } }, { { 0.3, 0, 0 }, { 40.0, 1, 0 } } }, 0 };
static const DtFn D115 = { DT_SINGLE, "single", "single", "metal", "rect", "metal", NULL, "up", 1, { 4, { 2.0, 2.6, 1.0, 1.3, 0.0 } }, 1, { 0, { 2.0, 0.3 } }, 0.0, 0.0, 0.8, 1.0, { 0.0, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 2, { { { 0.75, 0, 0 }, { 5.0, 1, 0 } }, { { 0.25, 0, 0 }, { 40.0, 1, 0 } } }, 0 };
static const DtFn D116 = { DT_PATH, "path", "path", "seam", "", "seam", NULL, "cut", 0, { 0, { 0.0, 0.0, 0.0, 0.0, 0.0 } }, 0, { 0, { 0.0, 0.0 } }, 0.0, 0.0, 0.0, 0.0, { 0.5, 5.0, 9.0, 0.1 }, { 0.3, NUM_FLOAT, 0 }, 0, { { { 0 } } }, 0 };
static const DtFn D117 = { DT_SCATTER, "scatter", "scatter", "pipe", "caps", "pipe", NULL, "up", 1, { 5, { 1.6, 2.4, 0.45, 0.0, 0.0 } }, 0, { 0, { 0.0, 0.0 } }, 2.0, 4.0, 0.4, 0.5, { 1.8, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 2, { { { 0.7, 0, 0 }, { 6.0, 1, 0 } }, { { 0.3, 0, 0 }, { 40.0, 1, 0 } } }, 0 };
static const DtFn D118 = { DT_SUNK, "sunk", "sunk", "slot", "hex", "slot", NULL, NULL, 1, { 9, { 0.9, 1.2, 0.0, 0.0, 0.0 } }, 0, { 0, { 0.0, 0.0 } }, 1.0, 1.0, 0.4, 0.6, { 0.0, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 0, { { { 0 } } }, 0 };
static const DtFn D119 = { DT_SINGLE, "single", "single", "crate", "rect", "crate", NULL, "up", 1, { 4, { 1.4, 1.8, 1.4, 1.8, 0.0 } }, 1, { 3, { 0.0, 0.0 } }, 0.0, 0.0, 1.6, 2.0, { 0.0, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 2, { { { 0.75, 0, 0 }, { 5.0, 1, 0 } }, { { 0.25, 0, 0 }, { 40.0, 1, 0 } } }, 0 };
static const DtFn D120 = { DT_SCATTER, "scatter", "scatter", "pustule", "disc", "pustule", NULL, "mound", 1, { 2, { 0.6, 0.9, 20.0, 0.0, 0.0 } }, 0, { 0, { 0.0, 0.0 } }, 3.0, 6.0, 0.7, 1.0, { 1.6, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 2, { { { 0.55, 0, 0 }, { 25.0, 1, 0 } }, { { 0.45, 0, 0 }, { 50.0, 1, 0 } } }, 0 };
static const DtFn D121 = { DT_SINGLE, "single", "single", "egg", "ell", "egg", NULL, "mound", 1, { 3, { 1.4, 1.9, 0.9, 1.2, 0.0 } }, 0, { 0, { 0.0, 0.0 } }, 0.0, 0.0, 1.0, 1.4, { 0.0, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 2, { { { 0.55, 0, 0 }, { 25.0, 1, 0 } }, { { 0.45, 0, 0 }, { 50.0, 1, 0 } } }, 0 };
static const DtFn D122 = { DT_SCATTER, "scatter", "scatter", "egg", "ell", "egg", NULL, "mound", 1, { 3, { 0.6, 0.8, 0.9, 1.2, 0.0 } }, 0, { 0, { 0.0, 0.0 } }, 3.0, 6.0, 0.7, 1.0, { 1.6, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 2, { { { 0.55, 0, 0 }, { 25.0, 1, 0 } }, { { 0.45, 0, 0 }, { 50.0, 1, 0 } } }, 0 };
static const DtFn D123 = { DT_PATH, "path", "path", "tendril", "", "tendril", NULL, "up", 0, { 0, { 0.0, 0.0, 0.0, 0.0, 0.0 } }, 0, { 0, { 0.0, 0.0 } }, 0.0, 0.0, 0.0, 0.0, { 0.7, 6.0, 10.0, 1.0 }, { 0.6, NUM_FLOAT, 0 }, 0, { { { 0 } } }, 0 };
static const DtFn D124 = { DT_PATH, "path", "path", "tendril", "", "tendril", NULL, "up", 0, { 0, { 0.0, 0.0, 0.0, 0.0, 0.0 } }, 0, { 0, { 0.0, 0.0 } }, 0.0, 0.0, 0.0, 0.0, { 0.5, 7.0, 11.0, 1.2 }, { 0.5, NUM_FLOAT, 0 }, 0, { { { 0 } } }, 0 };
static const DtFn D125 = { DT_SCATTER, "scatter", "scatter", "flesh", "star", "flesh", NULL, "mound", 1, { 7, { 0.9, 1.3, 5.0, 7.0, 0.0 } }, 0, { 0, { 0.0, 0.0 } }, 2.0, 3.0, 0.8, 1.1, { 1.8, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 2, { { { 0.55, 0, 0 }, { 25.0, 1, 0 } }, { { 0.45, 0, 0 }, { 50.0, 1, 0 } } }, 0 };
static const DtFn D126 = { DT_SINGLE, "single", "single", "flesh", "blob", "flesh", NULL, "mound", 1, { 13, { 1.8, 2.4, 0.0, 0.0, 0.0 } }, 0, { 0, { 0.0, 0.0 } }, 0.0, 0.0, 1.0, 1.4, { 0.0, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 2, { { { 0.55, 0, 0 }, { 25.0, 1, 0 } }, { { 0.45, 0, 0 }, { 50.0, 1, 0 } } }, 0 };
static const DtFn D127 = { DT_SUNK, "sunk", "sunk", "water", "blob", "water", NULL, NULL, 1, { 13, { 1.4, 2.0, 0.0, 0.0, 0.0 } }, 0, { 0, { 0.0, 0.0 } }, 1.0, 1.0, 0.4, 0.6, { 0.0, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 0, { { { 0 } } }, 0 };
static const DtFn D128 = { DT_SUNK, "sunk", "sunk", "pit", "disc", "pit", NULL, NULL, 1, { 2, { 0.4, 0.6, 14.0, 0.0, 0.0 } }, 0, { 0, { 0.0, 0.0 } }, 4.0, 7.0, 0.4, 0.5, { 1.6, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 0, { { { 0 } } }, 0 };
static const DtFn D129 = { DT_SUNK, "sunk", "sunk", "pit", "ring", "pit", NULL, NULL, 1, { 12, { 1.4, 1.8, 0.45, 0.0, 0.0 } }, 0, { 0, { 0.0, 0.0 } }, 1.0, 1.0, 0.4, 0.5, { 0.0, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 0, { { { 0 } } }, 0 };
static const DtFn D130 = { DT_SCATTER, "scatter", "scatter", "bone", "bone", "bone", NULL, "up", 1, { 6, { 1.6, 2.2, 0.0, 0.0, 0.0 } }, 0, { 0, { 0.0, 0.0 } }, 2.0, 4.0, 0.4, 0.5, { 1.6, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 2, { { { 0.7, 0, 0 }, { 6.0, 1, 0 } }, { { 0.3, 0, 0 }, { 40.0, 1, 0 } } }, 0 };
static const DtFn D131 = { DT_SINGLE, "single", "single", "bone", "ell", "bone", NULL, "mound", 1, { 3, { 1.2, 1.5, 0.8, 1.0, 0.0 } }, 1, { 4, { 0.22, 0.0 } }, 0.0, 0.0, 0.7, 0.9, { 0.0, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 2, { { { 0.75, 0, 0 }, { 5.0, 1, 0 } }, { { 0.25, 0, 0 }, { 40.0, 1, 0 } } }, 0 };
static const DtFn D132 = { DT_SCATTER, "scatter", "scatter", "carapace", "crescent", "carapace", NULL, "up", 1, { 8, { 0.8, 1.1, 0.0, 0.0, 0.0 } }, 0, { 0, { 0.0, 0.0 } }, 2.0, 4.0, 0.5, 0.7, { 1.6, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 2, { { { 0.7, 0, 0 }, { 6.0, 1, 0 } }, { { 0.3, 0, 0 }, { 40.0, 1, 0 } } }, 0 };
static const DtFn D133 = { DT_SINGLE, "single", "single", "carapace", "crescent", "carapace", NULL, "up", 1, { 8, { 1.6, 2.2, 0.0, 0.0, 0.0 } }, 1, { 1, { 0.7, 0.0 } }, 0.0, 0.0, 0.8, 1.0, { 0.0, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 2, { { { 0.75, 0, 0 }, { 5.0, 1, 0 } }, { { 0.25, 0, 0 }, { 40.0, 1, 0 } } }, 0 };
static const DtFn D134 = { DT_SCATTER, "scatter", "scatter", "spine", "tri", "spine", NULL, "mound", 1, { 10, { 0.6, 0.9, 0.0, 0.0, 0.0 } }, 0, { 0, { 0.0, 0.0 } }, 4.0, 6.0, 0.8, 1.2, { 1.6, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 2, { { { 0.6, 0, 0 }, { 4.0, 1, 0 } }, { { 0.4, 0, 0 }, { 35.0, 1, 0 } } }, 0 };
static const DtFn D135 = { DT_SINGLE, "single", "single", "spine", "star", "spine", NULL, "mound", 1, { 7, { 1.4, 1.8, 5.0, 6.0, 0.0 } }, 0, { 0, { 0.0, 0.0 } }, 0.0, 0.0, 1.2, 1.6, { 0.0, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 2, { { { 0.6, 0, 0 }, { 4.0, 1, 0 } }, { { 0.4, 0, 0 }, { 35.0, 1, 0 } } }, 0 };
static const DtFn D136 = { DT_TRAIL, "trail", "trail", "pit", "tri", "pit", NULL, NULL, 1, { 10, { 0.35, 0.45, 0.0, 0.0, 0.0 } }, 0, { 0, { 0.0, 0.0 } }, 5.0, 8.0, 0.0, 0.0, { 1.2, 0.0, 0.0, 0.0 }, { 0.35, NUM_FLOAT, 0 }, 0, { { { 0 } } }, 0 };
static const DtFn D137 = { DT_SUNK, "sunk", "sunk", "crack", "wig", "crack", NULL, NULL, 1, { 14, { 5.0, 9.0, 0.5, 1.0, 0.0 } }, 0, { 0, { 0.0, 0.0 } }, 1.0, 1.0, 0.4, 0.5, { 0.0, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 0, { { { 0 } } }, 0 };
static const DtFn D138 = { DT_SCATTER, "scatter", "scatter", "pustule", "round", "pustule", NULL, "mound", 1, { 1, { 0.5, 0.7, 0.0, 0.0, 0.0 } }, 0, { 0, { 0.0, 0.0 } }, 6.0, 10.0, 0.4, 0.5, { 1.4, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 2, { { { 0.6, 0, 0 }, { 20.0, 1, 0 } }, { { 0.4, 0, 0 }, { 50.0, 1, 0 } } }, 0 };
static const DtFn D139 = { DT_SINGLE, "single", "single", "flesh", "disc", "flesh", NULL, "up", 1, { 2, { 1.4, 1.8, 24.0, 0.0, 0.0 } }, 1, { 5, { 0.6, 0.9 } }, 0.0, 0.0, 0.5, 0.7, { 0.0, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 2, { { { 0.75, 0, 0 }, { 5.0, 1, 0 } }, { { 0.25, 0, 0 }, { 40.0, 1, 0 } } }, 0 };
static const DtFn D140 = { DT_SCATTER, "scatter", "scatter", "tendril", "caps", "tendril", NULL, "up", 1, { 5, { 1.6, 2.4, 0.5, 0.0, 0.0 } }, 0, { 0, { 0.0, 0.0 } }, 3.0, 5.0, 0.5, 0.7, { 1.6, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 2, { { { 0.7, 0, 0 }, { 6.0, 1, 0 } }, { { 0.3, 0, 0 }, { 40.0, 1, 0 } } }, 0 };
static const DtFn D141 = { DT_SINGLE, "single", "single", "flesh", "ring", "flesh", NULL, "up", 1, { 12, { 1.4, 1.8, 0.7, 0.0, 0.0 } }, 0, { 0, { 0.0, 0.0 } }, 0.0, 0.0, 0.8, 1.0, { 0.0, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 2, { { { 0.75, 0, 0 }, { 5.0, 1, 0 } }, { { 0.25, 0, 0 }, { 40.0, 1, 0 } } }, 0 };
static const DtFn D142 = { DT_SCATTER, "scatter", "scatter", "flesh", "blob", "flesh", NULL, "mound", 1, { 13, { 0.7, 1.0, 0.0, 0.0, 0.0 } }, 0, { 0, { 0.0, 0.0 } }, 2.0, 4.0, 0.6, 0.8, { 1.8, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 2, { { { 0.55, 0, 0 }, { 25.0, 1, 0 } }, { { 0.45, 0, 0 }, { 50.0, 1, 0 } } }, 0 };
static const DtFn D143 = { DT_SUNK, "sunk", "sunk", "pit", "crescent", "pit", NULL, NULL, 1, { 8, { 1.2, 1.6, 0.0, 0.0, 0.0 } }, 0, { 0, { 0.0, 0.0 } }, 1.0, 1.0, 0.3, 0.4, { 0.0, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 0, { { { 0 } } }, 0 };
static const DtFn D144 = { DT_SCATTER, "scatter", "scatter", "egg", "ell", "egg", NULL, "mound", 1, { 3, { 0.4, 0.5, 0.6, 0.8, 0.0 } }, 0, { 0, { 0.0, 0.0 } }, 6.0, 10.0, 0.4, 0.5, { 1.2, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 2, { { { 0.55, 0, 0 }, { 25.0, 1, 0 } }, { { 0.45, 0, 0 }, { 50.0, 1, 0 } } }, 0 };
static const DtFn D145 = { DT_SINGLE, "single", "single", "carapace", "hex", "carapace", NULL, "up", 1, { 9, { 1.2, 1.6, 0.0, 0.0, 0.0 } }, 1, { 3, { 0.0, 0.0 } }, 0.0, 0.0, 0.6, 0.8, { 0.0, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 2, { { { 0.75, 0, 0 }, { 5.0, 1, 0 } }, { { 0.25, 0, 0 }, { 40.0, 1, 0 } } }, 0 };
static const DtFn D146 = { DT_PATH, "path", "path", "flesh", "", "flesh", NULL, "up", 0, { 0, { 0.0, 0.0, 0.0, 0.0, 0.0 } }, 0, { 0, { 0.0, 0.0 } }, 0.0, 0.0, 0.0, 0.0, { 0.9, 5.0, 8.0, 0.5 }, { 0.45, NUM_FLOAT, 0 }, 0, { { { 0 } } }, 0 };
static const DtFn D147 = { DT_SUNK, "sunk", "sunk", "crack", "star", "crack", NULL, NULL, 1, { 7, { 1.3, 1.7, 5.0, 7.0, 0.0 } }, 0, { 0, { 0.0, 0.0 } }, 1.0, 1.0, 0.3, 0.4, { 0.0, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 0, { { { 0 } } }, 0 };
static const DtFn D148 = { DT_SCATTER, "scatter", "scatter", "rock", "rock", "rock", NULL, "mound", 1, { 0, { 0.8, 1.2, 5.0, 8.0, 0.0 } }, 0, { 0, { 0.0, 0.0 } }, 3.0, 5.0, 0.5, 0.8, { 1.6, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 2, { { { 0.7, 0, 0 }, { 6.0, 1, 0 } }, { { 0.3, 0, 0 }, { 40.0, 1, 0 } } }, 0 };
static const DtFn D149 = { DT_SINGLE, "single", "single", "rock", "rock", "rock", NULL, "mound", 1, { 0, { 1.8, 2.4, 5.0, 8.0, 0.0 } }, 0, { 0, { 0.0, 0.0 } }, 0.0, 0.0, 1.2, 1.6, { 0.0, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 2, { { { 0.7, 0, 0 }, { 6.0, 1, 0 } }, { { 0.3, 0, 0 }, { 40.0, 1, 0 } } }, 0 };
static const DtFn D150 = { DT_SCATTER, "scatter", "scatter", "brick", "rect", "brick", NULL, "up", 1, { 4, { 1.6, 2.0, 0.8, 1.0, 0.0 } }, 0, { 0, { 0.0, 0.0 } }, 4.0, 8.0, 0.45, 0.7, { 1.8, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 2, { { { 0.7, 0, 0 }, { 6.0, 1, 0 } }, { { 0.3, 0, 0 }, { 40.0, 1, 0 } } }, 0 };
static const DtFn D151 = { DT_SINGLE, "single", "single", "concrete", "rect", "concrete", NULL, "up", 1, { 4, { 3.0, 4.0, 1.8, 2.4, 0.0 } }, 1, { 1, { 1.1, 0.0 } }, 0.0, 0.0, 0.6, 0.9, { 0.0, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 2, { { { 0.75, 0, 0 }, { 5.0, 1, 0 } }, { { 0.25, 0, 0 }, { 40.0, 1, 0 } } }, 0 };
static const DtFn D152 = { DT_SCATTER, "scatter", "scatter", "rubble", "rock", "rubble", NULL, "mound", 1, { 0, { 0.8, 1.3, 5.0, 8.0, 0.0 } }, 0, { 0, { 0.0, 0.0 } }, 3.0, 5.0, 0.5, 0.8, { 1.6, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 2, { { { 0.7, 0, 0 }, { 6.0, 1, 0 } }, { { 0.3, 0, 0 }, { 40.0, 1, 0 } } }, 0 };
static const DtFn D153 = { DT_SINGLE, "single", "single", "rubble", "rock", "rubble", NULL, "mound", 1, { 0, { 1.8, 2.4, 5.0, 8.0, 0.0 } }, 0, { 0, { 0.0, 0.0 } }, 0.0, 0.0, 1.0, 1.4, { 0.0, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 2, { { { 0.7, 0, 0 }, { 6.0, 1, 0 } }, { { 0.3, 0, 0 }, { 40.0, 1, 0 } } }, 0 };
static const DtFn D154 = { DT_SCATTER, "scatter", "scatter", "metal", "caps", "metal", NULL, "up", 1, { 5, { 0.7, 1.0, 0.32, 0.0, 0.0 } }, 0, { 0, { 0.0, 0.0 } }, 6.0, 10.0, 0.3, 0.35, { 1.4, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 2, { { { 0.7, 0, 0 }, { 6.0, 1, 0 } }, { { 0.3, 0, 0 }, { 40.0, 1, 0 } } }, 0 };
static const DtFn D155 = { DT_SCATTER, "scatter", "scatter", "iron", "caps", "iron", NULL, "up", 1, { 5, { 2.4, 3.6, 0.4, 0.0, 0.0 } }, 0, { 0, { 0.0, 0.0 } }, 2.0, 3.0, 0.5, 0.6, { 1.6, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 2, { { { 0.7, 0, 0 }, { 6.0, 1, 0 } }, { { 0.3, 0, 0 }, { 40.0, 1, 0 } } }, 0 };
static const DtFn D156 = { DT_SINGLE, "single", "single", "metal", "disc", "metal", NULL, "up", 1, { 2, { 0.9, 1.2, 24.0, 0.0, 0.0 } }, 1, { 2, { 0.25, 0.0 } }, 0.0, 0.0, 1.6, 2.0, { 0.0, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 2, { { { 0.75, 0, 0 }, { 5.0, 1, 0 } }, { { 0.25, 0, 0 }, { 40.0, 1, 0 } } }, 0 };
static const DtFn D157 = { DT_SINGLE, "single", "single", "crate", "rect", "crate", NULL, "up", 1, { 4, { 2.8, 3.4, 1.8, 2.2, 0.0 } }, 1, { 1, { 0.8, 0.0 } }, 0.0, 0.0, 1.0, 1.3, { 0.0, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 2, { { { 0.75, 0, 0 }, { 5.0, 1, 0 } }, { { 0.25, 0, 0 }, { 40.0, 1, 0 } } }, 0 };
static const DtFn D158 = { DT_SINGLE, "single", "single", "concrete", "rect", "concrete", NULL, "up", 1, { 4, { 3.2, 4.0, 1.4, 1.8, 0.0 } }, 1, { 0, { 2.0, 0.4 } }, 0.0, 0.0, 1.4, 1.8, { 0.0, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 2, { { { 0.75, 0, 0 }, { 5.0, 1, 0 } }, { { 0.25, 0, 0 }, { 40.0, 1, 0 } } }, 0 };
static const DtFn D159 = { DT_SUNK, "sunk", "sunk", "water", "blob", "water", NULL, NULL, 1, { 13, { 1.4, 2.0, 0.0, 0.0, 0.0 } }, 0, { 0, { 0.0, 0.0 } }, 1.0, 1.0, 0.3, 0.45, { 0.0, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 0, { { { 0 } } }, 0 };
static const DtFn D160 = { DT_SUNK, "sunk", "sunk", "pothole", "disc", "pothole", NULL, NULL, 1, { 2, { 0.3, 0.45, 10.0, 0.0, 0.0 } }, 0, { 0, { 0.0, 0.0 } }, 5.0, 8.0, 0.4, 0.5, { 1.5, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 0, { { { 0 } } }, 0 };
static const DtFn D161 = { DT_SUNK, "sunk", "sunk", "pothole", "blob", "pothole", NULL, NULL, 1, { 13, { 1.6, 2.2, 0.0, 0.0, 0.0 } }, 0, { 0, { 0.0, 0.0 } }, 1.0, 1.0, 0.5, 0.7, { 0.0, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 0, { { { 0 } } }, 0 };
static const DtFn D162 = { DT_TRAIL, "trail", "trail", "pit", "ell", "pit", NULL, NULL, 1, { 3, { 0.35, 0.45, 0.6, 0.75, 0.0 } }, 0, { 0, { 0.0, 0.0 } }, 5.0, 8.0, 0.0, 0.0, { 1.2, 0.0, 0.0, 0.0 }, { 0.35, NUM_FLOAT, 0 }, 0, { { { 0 } } }, 0 };
static const DtFn D163 = { DT_PATH, "path", "path", "cable", "", "cable", NULL, "up", 0, { 0, { 0.0, 0.0, 0.0, 0.0, 0.0 } }, 0, { 0, { 0.0, 0.0 } }, 0.0, 0.0, 0.0, 0.0, { 0.5, 7.0, 11.0, 0.9 }, { 0.4, NUM_FLOAT, 0 }, 0, { { { 0 } } }, 0 };
static const DtFn D164 = { DT_PATH, "path", "path", "crack", "", "crack", NULL, "cut", 0, { 0, { 0.0, 0.0, 0.0, 0.0, 0.0 } }, 0, { 0, { 0.0, 0.0 } }, 0.0, 0.0, 0.0, 0.0, { 0.45, 6.0, 10.0, 0.9 }, { 0.4, NUM_FLOAT, 0 }, 0, { { { 0 } } }, 0 };
static const DtFn D165 = { DT_SINGLE, "single", "single", "post", "rect", "post", NULL, "up", 1, { 4, { 1.0, 1.3, 1.0, 1.3, 0.0 } }, 1, { 3, { 0.0, 0.0 } }, 0.0, 0.0, 2.0, 2.6, { 0.0, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 2, { { { 0.75, 0, 0 }, { 5.0, 1, 0 } }, { { 0.25, 0, 0 }, { 40.0, 1, 0 } } }, 0 };
static const DtFn D166 = { DT_SCATTER, "scatter", "scatter", "post", "disc", "post", NULL, "up", 1, { 2, { 0.6, 0.75, 20.0, 0.0, 0.0 } }, 0, { 0, { 0.0, 0.0 } }, 2.0, 3.0, 1.6, 2.0, { 2.0, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 2, { { { 0.7, 0, 0 }, { 6.0, 1, 0 } }, { { 0.3, 0, 0 }, { 40.0, 1, 0 } } }, 0 };
static const DtFn D167 = { DT_SINGLE, "single", "single", "iron", "cross", "iron", NULL, "up", 1, { 11, { 2.6, 3.4, 0.7, 0.0, 0.0 } }, 0, { 0, { 0.0, 0.0 } }, 0.0, 0.0, 0.5, 0.7, { 0.0, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 2, { { { 0.75, 0, 0 }, { 5.0, 1, 0 } }, { { 0.25, 0, 0 }, { 40.0, 1, 0 } } }, 0 };
static const DtFn D168 = { DT_SCATTER, "scatter", "scatter", "glass", "tri", "glass", NULL, "up", 1, { 10, { 0.6, 1.0, 0.0, 0.0, 0.0 } }, 0, { 0, { 0.0, 0.0 } }, 4.0, 7.0, 0.3, 0.45, { 1.6, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 2, { { { 0.7, 0, 0 }, { 6.0, 1, 0 } }, { { 0.3, 0, 0 }, { 40.0, 1, 0 } } }, 0 };
static const DtFn D169 = { DT_SINGLE, "single", "single", "metal", "ring", "metal", NULL, "up", 1, { 12, { 1.2, 1.5, 0.45, 0.0, 0.0 } }, 0, { 0, { 0.0, 0.0 } }, 0.0, 0.0, 0.6, 0.8, { 0.0, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 2, { { { 0.75, 0, 0 }, { 5.0, 1, 0 } }, { { 0.25, 0, 0 }, { 40.0, 1, 0 } } }, 0 };
static const DtFn D170 = { DT_SUNK, "sunk", "sunk", "slot", "rect", "slot", NULL, NULL, 1, { 4, { 2.6, 3.4, 1.6, 2.0, 0.0 } }, 0, { 0, { 0.0, 0.0 } }, 1.0, 1.0, 0.4, 0.6, { 0.0, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 0, { { { 0 } } }, 0 };
static const DtFn D171 = { DT_SINGLE, "single", "single", "wood", "rect", "wood", NULL, "up", 1, { 4, { 2.4, 3.0, 0.9, 1.1, 0.0 } }, 1, { 1, { 0.7, 0.0 } }, 0.0, 0.0, 0.6, 0.8, { 0.0, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 2, { { { 0.75, 0, 0 }, { 5.0, 1, 0 } }, { { 0.25, 0, 0 }, { 40.0, 1, 0 } } }, 0 };
static const DtFn D172 = { DT_SCATTER, "scatter", "scatter", "wood", "rect", "wood", NULL, "up", 1, { 4, { 1.0, 1.4, 0.5, 0.7, 0.0 } }, 0, { 0, { 0.0, 0.0 } }, 3.0, 6.0, 0.4, 0.5, { 1.6, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 2, { { { 0.7, 0, 0 }, { 6.0, 1, 0 } }, { { 0.3, 0, 0 }, { 40.0, 1, 0 } } }, 0 };
static const DtFn D173 = { DT_SUNK, "sunk", "sunk", "scorch", "star", "scorch", NULL, NULL, 1, { 7, { 1.3, 1.8, 5.0, 7.0, 0.0 } }, 0, { 0, { 0.0, 0.0 } }, 1.0, 1.0, 0.3, 0.4, { 0.0, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 0, { { { 0 } } }, 0 };
static const DtFn D174 = { DT_SINGLE, "single", "single", "metal", "disc", "metal", NULL, "up", 1, { 2, { 1.3, 1.6, 24.0, 0.0, 0.0 } }, 1, { 3, { 0.0, 0.0 } }, 0.0, 0.0, 0.4, 0.5, { 0.0, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 2, { { { 0.75, 0, 0 }, { 5.0, 1, 0 } }, { { 0.25, 0, 0 }, { 40.0, 1, 0 } } }, 0 };
static const DtFn D175 = { DT_SCATTER, "scatter", "scatter", "concrete", "hex", "concrete", NULL, "up", 1, { 9, { 0.5, 0.7, 0.0, 0.0, 0.0 } }, 0, { 0, { 0.0, 0.0 } }, 3.0, 5.0, 0.5, 0.6, { 1.4, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 2, { { { 0.7, 0, 0 }, { 6.0, 1, 0 } }, { { 0.3, 0, 0 }, { 40.0, 1, 0 } } }, 0 };
static const DtFn D176 = { DT_SINGLE, "single", "single", "paint", "rect", "paint", NULL, "up", 1, { 4, { 1.6, 2.0, 1.6, 2.0, 0.0 } }, 0, { 0, { 0.0, 0.0 } }, 0.0, 0.0, 0.3, 0.4, { 0.0, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 2, { { { 0.75, 0, 0 }, { 5.0, 1, 0 } }, { { 0.25, 0, 0 }, { 40.0, 1, 0 } } }, 0 };
static const DtFn D177 = { DT_SUNK, "sunk", "sunk", "slot", "ring", "slot", NULL, NULL, 1, { 12, { 1.0, 1.3, 0.3, 0.0, 0.0 } }, 0, { 0, { 0.0, 0.0 } }, 1.0, 1.0, 0.3, 0.4, { 0.0, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 0, { { { 0 } } }, 0 };
static const DtFn D178 = { DT_SCATTER, "scatter", "scatter", "rubble", "round", "rubble", NULL, "mound", 1, { 1, { 0.5, 0.7, 0.0, 0.0, 0.0 } }, 0, { 0, { 0.0, 0.0 } }, 6.0, 10.0, 0.35, 0.45, { 1.6, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 2, { { { 0.7, 0, 0 }, { 10.0, 1, 0 } }, { { 0.3, 0, 0 }, { 45.0, 1, 0 } } }, 0 };
static const DtFn D179 = { DT_SINGLE, "single", "single", "metal", "rect", "metal", NULL, "up", 1, { 4, { 2.2, 2.8, 1.2, 1.5, 0.0 } }, 1, { 0, { 3.0, 0.3 } }, 0.0, 0.0, 1.2, 1.5, { 0.0, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 2, { { { 0.75, 0, 0 }, { 5.0, 1, 0 } }, { { 0.25, 0, 0 }, { 40.0, 1, 0 } } }, 0 };
static const DtFn D180 = { DT_SINGLE, "single", "single", "crate", "rect", "crate", NULL, "up", 1, { 4, { 2.4, 3.2, 1.6, 2.0, 0.0 } }, 1, { 1, { 0.8, 0.0 } }, 0.0, 0.0, 1.0, 1.4, { 0.0, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 2, { { { 0.75, 0, 0 }, { 5.0, 1, 0 } }, { { 0.25, 0, 0 }, { 40.0, 1, 0 } } }, 0 };
static const DtFn D181 = { DT_SCATTER, "scatter", "scatter", "crate", "rect", "crate", NULL, "up", 1, { 4, { 1.2, 1.6, 1.0, 1.3, 0.0 } }, 0, { 0, { 0.0, 0.0 } }, 2.0, 3.0, 0.8, 1.1, { 1.8, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 2, { { { 0.7, 0, 0 }, { 6.0, 1, 0 } }, { { 0.3, 0, 0 }, { 40.0, 1, 0 } } }, 0 };
static const DtFn D182 = { DT_SINGLE, "single", "single", "wood", "disc", "wood", NULL, "up", 1, { 2, { 0.9, 1.2, 24.0, 0.0, 0.0 } }, 1, { 2, { 0.25, 0.0 } }, 0.0, 0.0, 1.4, 1.8, { 0.0, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 2, { { { 0.75, 0, 0 }, { 5.0, 1, 0 } }, { { 0.25, 0, 0 }, { 40.0, 1, 0 } } }, 0 };
static const DtFn D183 = { DT_SCATTER, "scatter", "scatter", "wood", "disc", "wood", NULL, "up", 1, { 2, { 0.6, 0.8, 20.0, 0.0, 0.0 } }, 0, { 0, { 0.0, 0.0 } }, 2.0, 3.0, 1.2, 1.5, { 1.6, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 2, { { { 0.7, 0, 0 }, { 6.0, 1, 0 } }, { { 0.3, 0, 0 }, { 40.0, 1, 0 } } }, 0 };
static const DtFn D184 = { DT_SCATTER, "scatter", "scatter", "wood", "caps", "wood", NULL, "up", 1, { 5, { 2.6, 3.6, 0.6, 0.0, 0.0 } }, 0, { 0, { 0.0, 0.0 } }, 2.0, 3.0, 0.5, 0.7, { 1.8, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 2, { { { 0.7, 0, 0 }, { 6.0, 1, 0 } }, { { 0.3, 0, 0 }, { 40.0, 1, 0 } } }, 0 };
static const DtFn D185 = { DT_SINGLE, "single", "single", "wood", "caps", "wood", NULL, "up", 1, { 5, { 5.0, 7.0, 1.0, 0.0, 0.0 } }, 1, { 1, { 1.3, 0.0 } }, 0.0, 0.0, 0.6, 0.8, { 0.0, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 2, { { { 0.75, 0, 0 }, { 5.0, 1, 0 } }, { { 0.25, 0, 0 }, { 40.0, 1, 0 } } }, 0 };
static const DtFn D186 = { DT_SCATTER, "scatter", "scatter", "rock", "rock", "rock", NULL, "mound", 1, { 0, { 0.9, 1.4, 5.0, 8.0, 0.0 } }, 0, { 0, { 0.0, 0.0 } }, 3.0, 5.0, 0.6, 0.9, { 1.6, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 2, { { { 0.7, 0, 0 }, { 6.0, 1, 0 } }, { { 0.3, 0, 0 }, { 40.0, 1, 0 } } }, 0 };
static const DtFn D187 = { DT_SINGLE, "single", "single", "rock", "rock", "rock", NULL, "mound", 1, { 0, { 1.8, 2.4, 5.0, 8.0, 0.0 } }, 0, { 0, { 0.0, 0.0 } }, 0.0, 0.0, 1.2, 1.6, { 0.0, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 2, { { { 0.7, 0, 0 }, { 6.0, 1, 0 } }, { { 0.3, 0, 0 }, { 40.0, 1, 0 } } }, 0 };
static const DtFn D188 = { DT_SCATTER, "scatter", "scatter", "quartz", "hex", "quartz", NULL, "up", 1, { 9, { 0.6, 0.9, 0.0, 0.0, 0.0 } }, 0, { 0, { 0.0, 0.0 } }, 3.0, 5.0, 0.7, 1.0, { 1.6, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 2, { { { 0.7, 0, 0 }, { 6.0, 1, 0 } }, { { 0.3, 0, 0 }, { 40.0, 1, 0 } } }, 0 };
static const DtFn D189 = { DT_SINGLE, "single", "single", "quartz", "tri", "quartz", NULL, "up", 1, { 10, { 1.2, 1.6, 0.0, 0.0, 0.0 } }, 0, { 0, { 0.0, 0.0 } }, 0.0, 0.0, 1.2, 1.6, { 0.0, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 2, { { { 0.75, 0, 0 }, { 5.0, 1, 0 } }, { { 0.25, 0, 0 }, { 40.0, 1, 0 } } }, 0 };
static const DtFn D190 = { DT_SCATTER, "scatter", "scatter", "pebble", "round", "pebble", NULL, "mound", 1, { 1, { 0.5, 0.7, 0.0, 0.0, 0.0 } }, 0, { 0, { 0.0, 0.0 } }, 6.0, 10.0, 0.35, 0.45, { 1.6, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 2, { { { 0.7, 0, 0 }, { 10.0, 1, 0 } }, { { 0.3, 0, 0 }, { 45.0, 1, 0 } } }, 0 };
static const DtFn D191 = { DT_SUNK, "sunk", "sunk", "water", "blob", "water", NULL, NULL, 1, { 13, { 1.4, 2.0, 0.0, 0.0, 0.0 } }, 0, { 0, { 0.0, 0.0 } }, 1.0, 1.0, 0.3, 0.45, { 0.0, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 0, { { { 0 } } }, 0 };
static const DtFn D192 = { DT_TRAIL, "trail", "trail", "pit", "ell", "pit", NULL, NULL, 1, { 3, { 0.35, 0.45, 0.6, 0.75, 0.0 } }, 0, { 0, { 0.0, 0.0 } }, 5.0, 8.0, 0.0, 0.0, { 1.3, 0.0, 0.0, 0.0 }, { 0.35, NUM_FLOAT, 0 }, 0, { { { 0 } } }, 0 };
static const DtFn D193 = { DT_PATH, "path", "path", "rut", "", "rut", NULL, "cut", 0, { 0, { 0.0, 0.0, 0.0, 0.0, 0.0 } }, 0, { 0, { 0.0, 0.0 } }, 0.0, 0.0, 0.0, 0.0, { 0.7, 6.0, 10.0, 0.6 }, { 0.4, NUM_FLOAT, 0 }, 0, { { { 0 } } }, 0 };
static const DtFn D194 = { DT_PATH, "path", "path", "cable", "", "cable", NULL, "up", 0, { 0, { 0.0, 0.0, 0.0, 0.0, 0.0 } }, 0, { 0, { 0.0, 0.0 } }, 0.0, 0.0, 0.0, 0.0, { 0.5, 6.0, 10.0, 0.8 }, { 0.4, NUM_FLOAT, 0 }, 0, { { { 0 } } }, 0 };
static const DtFn D195 = { DT_SINGLE, "single", "single", "iron", "rect", "iron", NULL, "up", 1, { 4, { 2.6, 3.2, 0.9, 1.1, 0.0 } }, 1, { 0, { 2.0, 0.3 } }, 0.0, 0.0, 0.6, 0.8, { 0.0, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 2, { { { 0.75, 0, 0 }, { 5.0, 1, 0 } }, { { 0.25, 0, 0 }, { 40.0, 1, 0 } } }, 0 };
static const DtFn D196 = { DT_SCATTER, "scatter", "scatter", "iron", "caps", "iron", NULL, "up", 1, { 5, { 0.7, 1.0, 0.3, 0.0, 0.0 } }, 0, { 0, { 0.0, 0.0 } }, 5.0, 8.0, 0.3, 0.35, { 1.4, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 2, { { { 0.7, 0, 0 }, { 6.0, 1, 0 } }, { { 0.3, 0, 0 }, { 40.0, 1, 0 } } }, 0 };
static const DtFn D197 = { DT_SUNK, "sunk", "sunk", "pit", "disc", "pit", NULL, NULL, 1, { 2, { 0.4, 0.55, 10.0, 0.0, 0.0 } }, 0, { 0, { 0.0, 0.0 } }, 4.0, 6.0, 0.6, 0.8, { 1.4, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 0, { { { 0 } } }, 0 };
static const DtFn D198 = { DT_SUNK, "sunk", "sunk", "pit", "rect", "pit", NULL, NULL, 1, { 4, { 2.0, 2.6, 1.4, 1.8, 0.0 } }, 0, { 0, { 0.0, 0.0 } }, 1.0, 1.0, 0.5, 0.7, { 0.0, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 0, { { { 0 } } }, 0 };
static const DtFn D199 = { DT_SINGLE, "single", "single", "wood", "cross", "wood", NULL, "up", 1, { 11, { 2.4, 3.0, 0.6, 0.0, 0.0 } }, 0, { 0, { 0.0, 0.0 } }, 0.0, 0.0, 0.6, 0.8, { 0.0, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 2, { { { 0.75, 0, 0 }, { 5.0, 1, 0 } }, { { 0.25, 0, 0 }, { 40.0, 1, 0 } } }, 0 };
static const DtFn D200 = { DT_SCATTER, "scatter", "scatter", "rock", "tri", "rock", NULL, "mound", 1, { 10, { 0.7, 1.0, 0.0, 0.0, 0.0 } }, 0, { 0, { 0.0, 0.0 } }, 4.0, 6.0, 0.5, 0.8, { 1.8, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 2, { { { 0.7, 0, 0 }, { 6.0, 1, 0 } }, { { 0.3, 0, 0 }, { 40.0, 1, 0 } } }, 0 };
static const DtFn D201 = { DT_SINGLE, "single", "single", "iron", "disc", "iron", NULL, "up", 1, { 2, { 1.2, 1.5, 24.0, 0.0, 0.0 } }, 1, { 5, { 0.5, 0.7 } }, 0.0, 0.0, 0.6, 0.8, { 0.0, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 2, { { { 0.75, 0, 0 }, { 5.0, 1, 0 } }, { { 0.25, 0, 0 }, { 40.0, 1, 0 } } }, 0 };
static const DtFn D202 = { DT_SINGLE, "single", "single", "crate", "rect", "crate", NULL, "up", 1, { 4, { 1.6, 2.0, 1.6, 2.0, 0.0 } }, 1, { 3, { 0.0, 0.0 } }, 0.0, 0.0, 1.6, 2.0, { 0.0, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 2, { { { 0.75, 0, 0 }, { 5.0, 1, 0 } }, { { 0.25, 0, 0 }, { 40.0, 1, 0 } } }, 0 };
static const DtFn D203 = { DT_SCATTER, "scatter", "scatter", "bone", "bone", "bone", NULL, "up", 1, { 6, { 1.4, 2.0, 0.0, 0.0, 0.0 } }, 0, { 0, { 0.0, 0.0 } }, 1.0, 3.0, 0.4, 0.5, { 1.6, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 2, { { { 0.7, 0, 0 }, { 6.0, 1, 0 } }, { { 0.3, 0, 0 }, { 40.0, 1, 0 } } }, 0 };
static const DtFn D204 = { DT_SUNK, "sunk", "sunk", "crack", "wig", "crack", NULL, NULL, 1, { 14, { 5.0, 8.0, 0.45, 0.8, 0.0 } }, 0, { 0, { 0.0, 0.0 } }, 1.0, 1.0, 0.4, 0.5, { 0.0, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 0, { { { 0 } } }, 0 };
static const DtFn D205 = { DT_SINGLE, "single", "single", "iron", "ring", "iron", NULL, "up", 1, { 12, { 1.2, 1.6, 0.5, 0.0, 0.0 } }, 0, { 0, { 0.0, 0.0 } }, 0.0, 0.0, 0.5, 0.7, { 0.0, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 2, { { { 0.75, 0, 0 }, { 5.0, 1, 0 } }, { { 0.25, 0, 0 }, { 40.0, 1, 0 } } }, 0 };
static const DtFn D206 = { DT_SCATTER, "scatter", "scatter", "rock", "blob", "rock", NULL, "mound", 1, { 13, { 0.8, 1.1, 0.0, 0.0, 0.0 } }, 0, { 0, { 0.0, 0.0 } }, 2.0, 3.0, 0.6, 0.9, { 1.8, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 2, { { { 0.7, 0, 0 }, { 6.0, 1, 0 } }, { { 0.3, 0, 0 }, { 40.0, 1, 0 } } }, 0 };
static const DtFn D207 = { DT_SINGLE, "single", "single", "rock", "ell", "rock", NULL, "mound", 1, { 3, { 1.6, 2.0, 1.0, 1.2, 0.0 } }, 1, { 4, { 0.25, 0.0 } }, 0.0, 0.0, 0.9, 1.2, { 0.0, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 2, { { { 0.7, 0, 0 }, { 6.0, 1, 0 } }, { { 0.3, 0, 0 }, { 40.0, 1, 0 } } }, 0 };
static const DtFn D208 = { DT_SUNK, "sunk", "sunk", "pit", "crescent", "pit", NULL, NULL, 1, { 8, { 1.2, 1.6, 0.0, 0.0, 0.0 } }, 0, { 0, { 0.0, 0.0 } }, 1.0, 1.0, 0.3, 0.4, { 0.0, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 0, { { { 0 } } }, 0 };
static const DtFn D209 = { DT_SINGLE, "single", "single", "wood", "rect", "wood", NULL, "up", 1, { 4, { 1.0, 1.3, 1.0, 1.3, 0.0 } }, 1, { 3, { 0.0, 0.0 } }, 0.0, 0.0, 2.0, 2.6, { 0.0, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 2, { { { 0.75, 0, 0 }, { 5.0, 1, 0 } }, { { 0.25, 0, 0 }, { 40.0, 1, 0 } } }, 0 };
static const DtFn D210 = { DT_OUTCROP, "a_outcrop", NULL, NULL, NULL, "sandstone", NULL, NULL, 0, { 0, { 0.0, 0.0, 0.0, 0.0, 0.0 } }, 0, { 0, { 0.0, 0.0 } }, 0.0, 0.0, 0.0, 0.0, { 0.0, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 0, { { { 0 } } }, 0 };
static const DtFn D211 = { DT_OUTCROP, "a_outcrop", NULL, NULL, NULL, "rock", NULL, NULL, 0, { 0, { 0.0, 0.0, 0.0, 0.0, 0.0 } }, 0, { 0, { 0.0, 0.0 } }, 0.0, 0.0, 0.0, 0.0, { 0.0, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 0, { { { 0 } } }, 0 };
static const DtFn D212 = { DT_HEAP, "a_heap", NULL, NULL, NULL, "rock", NULL, NULL, 0, { 0, { 0.0, 0.0, 0.0, 0.0, 0.0 } }, 0, { 0, { 0.0, 0.0 } }, 0.0, 0.0, 1.2, 1.8, { 0.0, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 2, { { { 0.5, 0, 0 }, { 12.0, 1, 0 } }, { { 0.5, 0, 0 }, { 40.0, 1, 0 } } }, 0 };
static const DtFn D213 = { DT_RING, "a_ring", NULL, NULL, NULL, "rock", NULL, NULL, 0, { 0, { 0.0, 0.0, 0.0, 0.0, 0.0 } }, 0, { 0, { 0.0, 0.0 } }, 6.0, 9.0, 1.2, 2.0, { 0.0, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 0, { { { 0 } } }, 0 };
static const DtFn D214 = { DT_PIT, "a_pit", NULL, NULL, NULL, "pit", NULL, NULL, 0, { 0, { 0.0, 0.0, 0.0, 0.0, 0.0 } }, 0, { 0, { 0.0, 0.0 } }, 0.0, 0.0, 0.6, 0.85, { 0.0, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 0, { { { 0 } } }, 0 };
static const DtFn D215 = { DT_OUTCROP, "a_outcrop", NULL, NULL, NULL, "slate", NULL, NULL, 0, { 0, { 0.0, 0.0, 0.0, 0.0, 0.0 } }, 0, { 0, { 0.0, 0.0 } }, 0.0, 0.0, 0.0, 0.0, { 0.0, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 0, { { { 0 } } }, 0 };
static const DtFn D216 = { DT_OUTCROP, "a_outcrop", NULL, NULL, NULL, "slate", NULL, NULL, 0, { 0, { 0.0, 0.0, 0.0, 0.0, 0.0 } }, 0, { 0, { 0.0, 0.0 } }, 0.0, 0.0, 0.0, 0.0, { 0.0, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 0, { { { 0 } } }, 0 };
static const DtFn D217 = { DT_RING, "a_ring", NULL, NULL, NULL, "standing", NULL, NULL, 0, { 0, { 0.0, 0.0, 0.0, 0.0, 0.0 } }, 0, { 0, { 0.0, 0.0 } }, 5.0, 8.0, 1.8, 2.6, { 0.0, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 0, { { { 0 } } }, 0 };
static const DtFn D218 = { DT_HEAP, "a_heap", NULL, NULL, NULL, "moss", NULL, NULL, 0, { 0, { 0.0, 0.0, 0.0, 0.0, 0.0 } }, 0, { 0, { 0.0, 0.0 } }, 0.0, 0.0, 1.0, 1.5, { 0.0, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 2, { { { 0.5, 0, 0 }, { 25.0, 1, 0 } }, { { 0.5, 0, 0 }, { 50.0, 1, 0 } } }, 0 };
static const DtFn D219 = { DT_PIT, "a_pit", NULL, NULL, NULL, "water", NULL, NULL, 0, { 0, { 0.0, 0.0, 0.0, 0.0, 0.0 } }, 0, { 0, { 0.0, 0.0 } }, 0.0, 0.0, 0.4, 0.55, { 0.0, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 0, { { { 0 } } }, 0 };
static const DtFn D220 = { DT_OUTCROP, "a_outcrop", NULL, NULL, NULL, "rock", NULL, NULL, 0, { 0, { 0.0, 0.0, 0.0, 0.0, 0.0 } }, 0, { 0, { 0.0, 0.0 } }, 0.0, 0.0, 0.0, 0.0, { 0.0, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 0, { { { 0 } } }, 0 };
static const DtFn D221 = { DT_HEAP, "a_heap", NULL, NULL, NULL, "dune", NULL, NULL, 0, { 0, { 0.0, 0.0, 0.0, 0.0, 0.0 } }, 0, { 0, { 0.0, 0.0 } }, 0.0, 0.0, 1.6, 2.4, { 0.0, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 3, { { { 0.4, 0, 0 }, { 15.0, 1, 0 } }, { { 0.35, 0, 0 }, { 30.0, 1, 0 } }, { { 0.25, 0, 0 }, { 50.0, 1, 0 } } }, 0 };
static const DtFn D222 = { DT_RING, "a_ring", NULL, NULL, NULL, "column", NULL, NULL, 0, { 0, { 0.0, 0.0, 0.0, 0.0, 0.0 } }, 0, { 0, { 0.0, 0.0 } }, 5.0, 7.0, 1.8, 2.6, { 0.0, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 0, { { { 0 } } }, 0 };
static const DtFn D223 = { DT_BLOCK, "a_block", NULL, NULL, NULL, "ruin", NULL, NULL, 0, { 0, { 0.0, 0.0, 0.0, 0.0, 0.0 } }, 1, { 6, { 0.0, 0.0 } }, 0.0, 0.0, 1.4, 2.2, { 0.0, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 0, { { { 0 } } }, 0 };
static const DtFn D224 = { DT_PIT, "a_pit", NULL, NULL, NULL, "pit", NULL, NULL, 0, { 0, { 0.0, 0.0, 0.0, 0.0, 0.0 } }, 0, { 0, { 0.0, 0.0 } }, 0.0, 0.0, 0.6, 0.85, { 0.0, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 0, { { { 0 } } }, 0 };
static const DtFn D225 = { DT_BLOCK, "a_block", NULL, NULL, NULL, "crate", NULL, NULL, 0, { 0, { 0.0, 0.0, 0.0, 0.0, 0.0 } }, 0, { 0, { 0.0, 0.0 } }, 0.0, 0.0, 1.4, 2.2, { 0.0, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 0, { { { 0 } } }, 0 };
static const DtFn D226 = { DT_BLOCK, "a_block", NULL, NULL, NULL, "metal", NULL, NULL, 0, { 0, { 0.0, 0.0, 0.0, 0.0, 0.0 } }, 1, { 0, { 5.0, 0.4 } }, 0.0, 0.0, 1.4, 2.2, { 0.0, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 0, { { { 0 } } }, 0 };
static const DtFn D227 = { DT_BLOCK, "a_block", NULL, NULL, NULL, "metal", "pipe", NULL, 0, { 0, { 0.0, 0.0, 0.0, 0.0, 0.0 } }, 0, { 0, { 0.0, 0.0 } }, 0.0, 0.0, 1.4, 2.2, { 0.0, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 0, { { { 0 } } }, 0 };
static const DtFn D228 = { DT_BLOCK, "a_block", NULL, NULL, NULL, "plate", NULL, NULL, 0, { 0, { 0.0, 0.0, 0.0, 0.0, 0.0 } }, 1, { 3, { 0.0, 0.0 } }, 0.0, 0.0, 0.5, 0.7, { 0.0, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 0, { { { 0 } } }, 0 };
static const DtFn D229 = { DT_PIT, "a_pit", NULL, NULL, NULL, "slot", NULL, NULL, 0, { 0, { 0.0, 0.0, 0.0, 0.0, 0.0 } }, 0, { 0, { 0.0, 0.0 } }, 0.0, 0.0, 0.5, 0.7, { 0.0, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 0, { { { 0 } } }, 0 };
static const DtFn D230 = { DT_HEAP, "a_heap", NULL, NULL, NULL, "flesh", NULL, NULL, 0, { 0, { 0.0, 0.0, 0.0, 0.0, 0.0 } }, 0, { 0, { 0.0, 0.0 } }, 0.0, 0.0, 1.6, 2.4, { 0.0, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 3, { { { 0.45, 0, 0 }, { 18.0, 1, 0 } }, { { 0.35, 0, 0 }, { 35.0, 1, 0 } }, { { 0.2, 0, 0 }, { 52.0, 1, 0 } } }, 0 };
static const DtFn D231 = { DT_HEAP, "a_heap", NULL, NULL, NULL, "flesh", NULL, NULL, 0, { 0, { 0.0, 0.0, 0.0, 0.0, 0.0 } }, 0, { 0, { 0.0, 0.0 } }, 0.0, 0.0, 1.6, 2.4, { 0.0, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 3, { { { 0.45, 0, 0 }, { 18.0, 1, 0 } }, { { 0.35, 0, 0 }, { 35.0, 1, 0 } }, { { 0.2, 0, 0 }, { 52.0, 1, 0 } } }, 0 };
static const DtFn D232 = { DT_RING, "a_ring", NULL, NULL, NULL, "pustule", NULL, NULL, 0, { 0, { 0.0, 0.0, 0.0, 0.0, 0.0 } }, 0, { 0, { 0.0, 0.0 } }, 5.0, 8.0, 1.0, 1.4, { 0.0, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 0, { { { 0 } } }, 0 };
static const DtFn D233 = { DT_BLOCK, "a_block", NULL, NULL, NULL, "crate", NULL, NULL, 0, { 0, { 0.0, 0.0, 0.0, 0.0, 0.0 } }, 0, { 0, { 0.0, 0.0 } }, 0.0, 0.0, 1.4, 2.2, { 0.0, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 0, { { { 0 } } }, 0 };
static const DtFn D234 = { DT_PIT, "a_pit", NULL, NULL, NULL, "acid", NULL, NULL, 0, { 0, { 0.0, 0.0, 0.0, 0.0, 0.0 } }, 0, { 0, { 0.0, 0.0 } }, 0.0, 0.0, 0.5, 0.7, { 0.0, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 0, { { { 0 } } }, 0 };
static const DtFn D235 = { DT_HEAP, "a_heap", NULL, NULL, NULL, "rubble", NULL, NULL, 0, { 0, { 0.0, 0.0, 0.0, 0.0, 0.0 } }, 0, { 0, { 0.0, 0.0 } }, 0.0, 0.0, 1.4, 2.0, { 0.0, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 2, { { { 0.5, 0, 0 }, { 12.0, 1, 0 } }, { { 0.5, 0, 0 }, { 40.0, 1, 0 } } }, 0 };
static const DtFn D236 = { DT_BLOCK, "a_block", NULL, NULL, NULL, "concrete", NULL, NULL, 0, { 0, { 0.0, 0.0, 0.0, 0.0, 0.0 } }, 1, { 1, { 1.4, 0.0 } }, 0.0, 0.0, 1.4, 2.2, { 0.0, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 0, { { { 0 } } }, 0 };
static const DtFn D237 = { DT_BLOCK, "a_block", NULL, NULL, NULL, "brick", NULL, NULL, 0, { 0, { 0.0, 0.0, 0.0, 0.0, 0.0 } }, 0, { 0, { 0.0, 0.0 } }, 0.0, 0.0, 1.4, 2.2, { 0.0, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 0, { { { 0 } } }, 0 };
static const DtFn D238 = { DT_BLOCK, "a_block", NULL, NULL, NULL, "crate", NULL, NULL, 0, { 0, { 0.0, 0.0, 0.0, 0.0, 0.0 } }, 0, { 0, { 0.0, 0.0 } }, 0.0, 0.0, 1.4, 2.2, { 0.0, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 0, { { { 0 } } }, 0 };
static const DtFn D239 = { DT_PIT, "a_pit", NULL, NULL, NULL, "pothole", NULL, NULL, 0, { 0, { 0.0, 0.0, 0.0, 0.0, 0.0 } }, 0, { 0, { 0.0, 0.0 } }, 0.0, 0.0, 0.6, 0.85, { 0.0, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 0, { { { 0 } } }, 0 };
static const DtFn D240 = { DT_OUTCROP, "a_outcrop", NULL, NULL, NULL, "bedrock", NULL, NULL, 0, { 0, { 0.0, 0.0, 0.0, 0.0, 0.0 } }, 0, { 0, { 0.0, 0.0 } }, 0.0, 0.0, 0.0, 0.0, { 0.0, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 0, { { { 0 } } }, 0 };
static const DtFn D241 = { DT_HEAP, "a_heap", NULL, NULL, NULL, "rock", NULL, NULL, 0, { 0, { 0.0, 0.0, 0.0, 0.0, 0.0 } }, 0, { 0, { 0.0, 0.0 } }, 0.0, 0.0, 1.6, 2.4, { 0.0, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 2, { { { 0.5, 0, 0 }, { 12.0, 1, 0 } }, { { 0.5, 0, 0 }, { 40.0, 1, 0 } } }, 0 };
static const DtFn D242 = { DT_BLOCK, "a_block", NULL, NULL, NULL, "crate", NULL, NULL, 0, { 0, { 0.0, 0.0, 0.0, 0.0, 0.0 } }, 0, { 0, { 0.0, 0.0 } }, 0.0, 0.0, 1.4, 2.2, { 0.0, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 0, { { { 0 } } }, 0 };
static const DtFn D243 = { DT_BLOCK, "a_block", NULL, NULL, NULL, "wood", NULL, NULL, 0, { 0, { 0.0, 0.0, 0.0, 0.0, 0.0 } }, 1, { 1, { 1.2, 0.0 } }, 0.0, 0.0, 0.8, 1.2, { 0.0, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 0, { { { 0 } } }, 0 };
static const DtFn D244 = { DT_PIT, "a_pit", NULL, NULL, NULL, "pit", NULL, NULL, 0, { 0, { 0.0, 0.0, 0.0, 0.0, 0.0 } }, 0, { 0, { 0.0, 0.0 } }, 0.0, 0.0, 0.6, 0.85, { 0.0, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 0, { { { 0 } } }, 0 };
static const DtFn D245 = { DT_HEAP, "a_heap", NULL, NULL, NULL, "mud", NULL, NULL, 0, { 0, { 0.0, 0.0, 0.0, 0.0, 0.0 } }, 0, { 0, { 0.0, 0.0 } }, 0.0, 0.0, 1.0, 1.6, { 0.0, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 2, { { { 0.5, 0, 0 }, { 20.0, 1, 0 } }, { { 0.5, 0, 0 }, { 45.0, 1, 0 } } }, 0 };
static const DtFn D246 = { DT_OUTCROP, "a_outcrop", NULL, NULL, NULL, "rock", NULL, NULL, 0, { 0, { 0.0, 0.0, 0.0, 0.0, 0.0 } }, 0, { 0, { 0.0, 0.0 } }, 0.0, 0.0, 0.0, 0.0, { 0.0, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 0, { { { 0 } } }, 0 };
static const DtFn D247 = { DT_RING, "a_ring", NULL, NULL, NULL, "tflag", NULL, NULL, 0, { 0, { 0.0, 0.0, 0.0, 0.0, 0.0 } }, 0, { 0, { 0.0, 0.0 } }, 5.0, 7.0, 1.6, 2.4, { 0.0, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 0, { { { 0 } } }, 0 };
static const DtFn D248 = { DT_BLOCK, "a_block", NULL, NULL, NULL, "tflag", NULL, NULL, 0, { 0, { 0.0, 0.0, 0.0, 0.0, 0.0 } }, 1, { 6, { 0.0, 0.0 } }, 0.0, 0.0, 0.8, 1.2, { 0.0, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 0, { { { 0 } } }, 0 };
static const DtFn D249 = { DT_PIT, "a_pit", NULL, NULL, NULL, "water", NULL, NULL, 0, { 0, { 0.0, 0.0, 0.0, 0.0, 0.0 } }, 0, { 0, { 0.0, 0.0 } }, 0.0, 0.0, 0.5, 0.6, { 0.0, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 0, { { { 0 } } }, 0 };
static const DtFn D250 = { DT_OUTCROP, "a_outcrop", NULL, NULL, NULL, "basalt", NULL, NULL, 0, { 0, { 0.0, 0.0, 0.0, 0.0, 0.0 } }, 0, { 0, { 0.0, 0.0 } }, 0.0, 0.0, 0.0, 0.0, { 0.0, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 0, { { { 0 } } }, 0 };
static const DtFn D251 = { DT_OUTCROP, "a_outcrop", NULL, NULL, NULL, "obsidian", NULL, NULL, 0, { 0, { 0.0, 0.0, 0.0, 0.0, 0.0 } }, 0, { 0, { 0.0, 0.0 } }, 0.0, 0.0, 0.0, 0.0, { 0.0, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 0, { { { 0 } } }, 0 };
static const DtFn D252 = { DT_RING, "a_ring", NULL, NULL, NULL, "basalt", NULL, NULL, 0, { 0, { 0.0, 0.0, 0.0, 0.0, 0.0 } }, 0, { 0, { 0.0, 0.0 } }, 5.0, 8.0, 1.4, 2.2, { 0.0, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 0, { { { 0 } } }, 0 };
static const DtFn D253 = { DT_HEAP, "a_heap", NULL, NULL, NULL, "basalt", NULL, NULL, 0, { 0, { 0.0, 0.0, 0.0, 0.0, 0.0 } }, 0, { 0, { 0.0, 0.0 } }, 0.0, 0.0, 1.6, 2.4, { 0.0, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 2, { { { 0.5, 0, 0 }, { 12.0, 1, 0 } }, { { 0.5, 0, 0 }, { 40.0, 1, 0 } } }, 0 };
static const DtFn D254 = { DT_PIT, "a_pit", NULL, NULL, NULL, "lava", NULL, NULL, 0, { 0, { 0.0, 0.0, 0.0, 0.0, 0.0 } }, 0, { 0, { 0.0, 0.0 } }, 0.0, 0.0, 0.6, 0.75, { 0.0, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 0, { { { 0 } } }, 0 };
static const DtFn D255 = { DT_HEAP, "a_heap", NULL, NULL, NULL, "snow", NULL, NULL, 0, { 0, { 0.0, 0.0, 0.0, 0.0, 0.0 } }, 0, { 0, { 0.0, 0.0 } }, 0.0, 0.0, 1.0, 1.6, { 0.0, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 2, { { { 0.5, 0, 0 }, { 25.0, 1, 0 } }, { { 0.5, 0, 0 }, { 55.0, 1, 0 } } }, 0 };
static const DtFn D256 = { DT_BLOCK, "a_block", NULL, NULL, NULL, "alien", NULL, NULL, 0, { 0, { 0.0, 0.0, 0.0, 0.0, 0.0 } }, 1, { 0, { 5.0, 0.4 } }, 0.0, 0.0, 1.4, 2.2, { 0.0, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 0, { { { 0 } } }, 0 };
static const DtFn D257 = { DT_BLOCK, "a_block", NULL, NULL, NULL, "alien", "crystal", NULL, 0, { 0, { 0.0, 0.0, 0.0, 0.0, 0.0 } }, 0, { 0, { 0.0, 0.0 } }, 0.0, 0.0, 1.4, 2.2, { 0.0, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 0, { { { 0 } } }, 0 };
static const DtFn D258 = { DT_OUTCROP, "a_outcrop", NULL, NULL, NULL, "ice", NULL, NULL, 0, { 0, { 0.0, 0.0, 0.0, 0.0, 0.0 } }, 0, { 0, { 0.0, 0.0 } }, 0.0, 0.0, 0.0, 0.0, { 0.0, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 0, { { { 0 } } }, 0 };
static const DtFn D259 = { DT_RING, "a_ring", NULL, NULL, NULL, "alien", NULL, NULL, 0, { 0, { 0.0, 0.0, 0.0, 0.0, 0.0 } }, 0, { 0, { 0.0, 0.0 } }, 5.0, 7.0, 1.6, 2.4, { 0.0, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 0, { { { 0 } } }, 0 };
static const DtFn D260 = { DT_PIT, "a_pit", NULL, NULL, NULL, "pit", NULL, NULL, 0, { 0, { 0.0, 0.0, 0.0, 0.0, 0.0 } }, 0, { 0, { 0.0, 0.0 } }, 0.0, 0.0, 0.7, 0.9, { 0.0, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 0, { { { 0 } } }, 0 };
static const DtFn D261 = { DT_PIT, "a_pit", NULL, NULL, NULL, "water", NULL, NULL, 0, { 0, { 0.0, 0.0, 0.0, 0.0, 0.0 } }, 0, { 0, { 0.0, 0.0 } }, 0.0, 0.0, 0.55, 0.7, { 0.0, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 0, { { { 0 } } }, 0 };
static const DtFn D262 = { DT_HEAP, "a_heap", NULL, NULL, NULL, "sandbag", NULL, NULL, 0, { 0, { 0.0, 0.0, 0.0, 0.0, 0.0 } }, 0, { 0, { 0.0, 0.0 } }, 0.0, 0.0, 1.2, 1.7, { 0.0, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 2, { { { 0.6, 0, 0 }, { 10.0, 1, 0 } }, { { 0.4, 0, 0 }, { 40.0, 1, 0 } } }, 0 };
static const DtFn D263 = { DT_BLOCK, "a_block", NULL, NULL, NULL, "crate", NULL, NULL, 0, { 0, { 0.0, 0.0, 0.0, 0.0, 0.0 } }, 0, { 0, { 0.0, 0.0 } }, 0.0, 0.0, 1.4, 2.2, { 0.0, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 0, { { { 0 } } }, 0 };
static const DtFn D264 = { DT_HEAP, "a_heap", NULL, NULL, NULL, "mud", NULL, NULL, 0, { 0, { 0.0, 0.0, 0.0, 0.0, 0.0 } }, 0, { 0, { 0.0, 0.0 } }, 0.0, 0.0, 1.0, 1.5, { 0.0, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 2, { { { 0.5, 0, 0 }, { 20.0, 1, 0 } }, { { 0.5, 0, 0 }, { 45.0, 1, 0 } } }, 0 };
static const DtFn D265 = { DT_BLOCK, "a_block", NULL, NULL, NULL, "plank", NULL, NULL, 0, { 0, { 0.0, 0.0, 0.0, 0.0, 0.0 } }, 1, { 1, { 1.2, 0.0 } }, 0.0, 0.0, 0.7, 1.0, { 0.0, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 0, { { { 0 } } }, 0 };
static const DtFn D266 = { DT_OUTCROP, "a_outcrop", NULL, NULL, NULL, "garden", NULL, NULL, 0, { 0, { 0.0, 0.0, 0.0, 0.0, 0.0 } }, 0, { 0, { 0.0, 0.0 } }, 0.0, 0.0, 0.0, 0.0, { 0.0, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 0, { { { 0 } } }, 0 };
static const DtFn D267 = { DT_RING, "a_ring", NULL, NULL, NULL, "garden", NULL, NULL, 0, { 0, { 0.0, 0.0, 0.0, 0.0, 0.0 } }, 0, { 0, { 0.0, 0.0 } }, 5.0, 7.0, 0.9, 1.4, { 0.0, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 0, { { { 0 } } }, 0 };
static const DtFn D268 = { DT_BLOCK, "a_block", NULL, NULL, NULL, "crate", NULL, NULL, 0, { 0, { 0.0, 0.0, 0.0, 0.0, 0.0 } }, 0, { 0, { 0.0, 0.0 } }, 0.0, 0.0, 1.4, 2.2, { 0.0, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 0, { { { 0 } } }, 0 };
static const DtFn D269 = { DT_BLOCK, "a_block", NULL, NULL, NULL, "tatami", NULL, NULL, 0, { 0, { 0.0, 0.0, 0.0, 0.0, 0.0 } }, 1, { 3, { 0.0, 0.0 } }, 0.0, 0.0, 0.4, 0.6, { 0.0, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 0, { { { 0 } } }, 0 };
static const DtFn D270 = { DT_HEAP, "a_heap", NULL, NULL, NULL, "flesh", NULL, NULL, 0, { 0, { 0.0, 0.0, 0.0, 0.0, 0.0 } }, 0, { 0, { 0.0, 0.0 } }, 0.0, 0.0, 1.6, 2.4, { 0.0, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 3, { { { 0.45, 0, 0 }, { 18.0, 1, 0 } }, { { 0.35, 0, 0 }, { 35.0, 1, 0 } }, { { 0.2, 0, 0 }, { 52.0, 1, 0 } } }, 0 };
static const DtFn D271 = { DT_HEAP, "a_heap", NULL, NULL, NULL, "flesh", NULL, NULL, 0, { 0, { 0.0, 0.0, 0.0, 0.0, 0.0 } }, 0, { 0, { 0.0, 0.0 } }, 0.0, 0.0, 1.6, 2.4, { 0.0, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 3, { { { 0.45, 0, 0 }, { 18.0, 1, 0 } }, { { 0.35, 0, 0 }, { 35.0, 1, 0 } }, { { 0.2, 0, 0 }, { 52.0, 1, 0 } } }, 0 };
static const DtFn D272 = { DT_RING, "a_ring", NULL, NULL, NULL, "tooth", NULL, NULL, 0, { 0, { 0.0, 0.0, 0.0, 0.0, 0.0 } }, 0, { 0, { 0.0, 0.0 } }, 6.0, 9.0, 1.6, 2.4, { 0.0, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 0, { { { 0 } } }, 0 };
static const DtFn D273 = { DT_PIT, "a_pit", NULL, NULL, NULL, "bile", NULL, NULL, 0, { 0, { 0.0, 0.0, 0.0, 0.0, 0.0 } }, 0, { 0, { 0.0, 0.0 } }, 0.0, 0.0, 0.55, 0.7, { 0.0, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 0, { { { 0 } } }, 0 };
static const DtFn D274 = { DT_OUTCROP, "a_outcrop", NULL, NULL, NULL, "flesh", NULL, NULL, 0, { 0, { 0.0, 0.0, 0.0, 0.0, 0.0 } }, 0, { 0, { 0.0, 0.0 } }, 0.0, 0.0, 0.0, 0.0, { 0.0, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 0, { { { 0 } } }, 0 };
static const DtFn D275 = { DT_BLOCK, "a_block", NULL, NULL, NULL, "crate", NULL, NULL, 0, { 0, { 0.0, 0.0, 0.0, 0.0, 0.0 } }, 0, { 0, { 0.0, 0.0 } }, 0.0, 0.0, 1.4, 2.2, { 0.0, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 0, { { { 0 } } }, 0 };
static const DtFn D276 = { DT_BLOCK, "a_block", NULL, NULL, NULL, "board", NULL, NULL, 0, { 0, { 0.0, 0.0, 0.0, 0.0, 0.0 } }, 1, { 1, { 1.1, 0.0 } }, 0.0, 0.0, 0.8, 1.2, { 0.0, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 0, { { { 0 } } }, 0 };
static const DtFn D277 = { DT_HEAP, "a_heap", NULL, NULL, NULL, "rock", NULL, NULL, 0, { 0, { 0.0, 0.0, 0.0, 0.0, 0.0 } }, 0, { 0, { 0.0, 0.0 } }, 0.0, 0.0, 1.2, 1.8, { 0.0, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 2, { { { 0.5, 0, 0 }, { 12.0, 1, 0 } }, { { 0.5, 0, 0 }, { 40.0, 1, 0 } } }, 0 };
static const DtFn D278 = { DT_RING, "a_ring", NULL, NULL, NULL, "post", NULL, NULL, 0, { 0, { 0.0, 0.0, 0.0, 0.0, 0.0 } }, 0, { 0, { 0.0, 0.0 } }, 4.0, 6.0, 1.6, 2.2, { 0.0, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 0, { { { 0 } } }, 0 };
static const DtFn D279 = { DT_PIT, "a_pit", NULL, NULL, NULL, "rut", NULL, NULL, 0, { 0, { 0.0, 0.0, 0.0, 0.0, 0.0 } }, 0, { 0, { 0.0, 0.0 } }, 0.0, 0.0, 0.4, 0.5, { 0.0, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 0, { { { 0 } } }, 0 };
static const DtFn D280 = { DT_BLOCK, "a_block", NULL, NULL, NULL, "headstone", NULL, NULL, 0, { 0, { 0.0, 0.0, 0.0, 0.0, 0.0 } }, 1, { 2, { 0.35, 0.0 } }, 0.0, 0.0, 0.8, 1.2, { 0.0, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 0, { { { 0 } } }, 0 };
static const DtFn D281 = { DT_BLOCK, "a_block", NULL, NULL, NULL, "headstone", NULL, NULL, 0, { 0, { 0.0, 0.0, 0.0, 0.0, 0.0 } }, 0, { 0, { 0.0, 0.0 } }, 0.0, 0.0, 1.4, 2.2, { 0.0, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 0, { { { 0 } } }, 0 };
static const DtFn D282 = { DT_HEAP, "a_heap", NULL, NULL, NULL, "earth", NULL, NULL, 0, { 0, { 0.0, 0.0, 0.0, 0.0, 0.0 } }, 0, { 0, { 0.0, 0.0 } }, 0.0, 0.0, 1.0, 1.5, { 0.0, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 2, { { { 0.5, 0, 0 }, { 20.0, 1, 0 } }, { { 0.5, 0, 0 }, { 45.0, 1, 0 } } }, 0 };
static const DtFn D283 = { DT_PIT, "a_pit", NULL, NULL, NULL, "pit", NULL, NULL, 0, { 0, { 0.0, 0.0, 0.0, 0.0, 0.0 } }, 0, { 0, { 0.0, 0.0 } }, 0.0, 0.0, 0.8, 0.95, { 0.0, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 0, { { { 0 } } }, 0 };
static const DtFn D284 = { DT_RING, "a_ring", NULL, NULL, NULL, "headstone", NULL, NULL, 0, { 0, { 0.0, 0.0, 0.0, 0.0, 0.0 } }, 0, { 0, { 0.0, 0.0 } }, 5.0, 7.0, 1.6, 2.4, { 0.0, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 0, { { { 0 } } }, 0 };
static const DtFn D285 = { DT_OUTCROP, "a_outcrop", NULL, NULL, NULL, "rock", NULL, NULL, 0, { 0, { 0.0, 0.0, 0.0, 0.0, 0.0 } }, 0, { 0, { 0.0, 0.0 } }, 0.0, 0.0, 0.0, 0.0, { 0.0, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 0, { { { 0 } } }, 0 };
static const DtFn D286 = { DT_HEAP, "a_heap", NULL, NULL, NULL, "moss", NULL, NULL, 0, { 0, { 0.0, 0.0, 0.0, 0.0, 0.0 } }, 0, { 0, { 0.0, 0.0 } }, 0.0, 0.0, 1.0, 1.6, { 0.0, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 2, { { { 0.5, 0, 0 }, { 25.0, 1, 0 } }, { { 0.5, 0, 0 }, { 50.0, 1, 0 } } }, 0 };
static const DtFn D287 = { DT_RING, "a_ring", NULL, NULL, NULL, "shroom", NULL, NULL, 0, { 0, { 0.0, 0.0, 0.0, 0.0, 0.0 } }, 0, { 0, { 0.0, 0.0 } }, 6.0, 9.0, 0.7, 1.0, { 0.0, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 0, { { { 0 } } }, 0 };
static const DtFn D288 = { DT_RING, "a_ring", NULL, NULL, NULL, "rock", NULL, NULL, 0, { 0, { 0.0, 0.0, 0.0, 0.0, 0.0 } }, 0, { 0, { 0.0, 0.0 } }, 5.0, 7.0, 1.0, 1.6, { 0.0, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 0, { { { 0 } } }, 0 };
static const DtFn D289 = { DT_PIT, "a_pit", NULL, NULL, NULL, "water", NULL, NULL, 0, { 0, { 0.0, 0.0, 0.0, 0.0, 0.0 } }, 0, { 0, { 0.0, 0.0 } }, 0.0, 0.0, 0.4, 0.55, { 0.0, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 0, { { { 0 } } }, 0 };
static const DtFn D290 = { DT_BLOCK, "a_block", NULL, NULL, NULL, "flag", NULL, NULL, 0, { 0, { 0.0, 0.0, 0.0, 0.0, 0.0 } }, 0, { 0, { 0.0, 0.0 } }, 0.0, 0.0, 1.4, 2.2, { 0.0, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 0, { { { 0 } } }, 0 };
static const DtFn D291 = { DT_BLOCK, "a_block", NULL, NULL, NULL, "crate", NULL, NULL, 0, { 0, { 0.0, 0.0, 0.0, 0.0, 0.0 } }, 0, { 0, { 0.0, 0.0 } }, 0.0, 0.0, 1.4, 2.2, { 0.0, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 0, { { { 0 } } }, 0 };
static const DtFn D292 = { DT_RING, "a_ring", NULL, NULL, NULL, "flag", NULL, NULL, 0, { 0, { 0.0, 0.0, 0.0, 0.0, 0.0 } }, 0, { 0, { 0.0, 0.0 } }, 4.0, 6.0, 1.8, 2.6, { 0.0, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 0, { { { 0 } } }, 0 };
static const DtFn D293 = { DT_PIT, "a_pit", NULL, NULL, NULL, "pit", NULL, NULL, 0, { 0, { 0.0, 0.0, 0.0, 0.0, 0.0 } }, 0, { 0, { 0.0, 0.0 } }, 0.0, 0.0, 0.7, 0.9, { 0.0, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 0, { { { 0 } } }, 0 };
static const DtFn D294 = { DT_BLOCK, "a_block", NULL, NULL, NULL, "iron", NULL, NULL, 0, { 0, { 0.0, 0.0, 0.0, 0.0, 0.0 } }, 1, { 0, { 5.0, 0.4 } }, 0.0, 0.0, 0.4, 0.6, { 0.0, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 0, { { { 0 } } }, 0 };
static const DtFn D295 = { DT_BLOCK, "a_block", NULL, NULL, NULL, "tflag", "tcolumn", NULL, 0, { 0, { 0.0, 0.0, 0.0, 0.0, 0.0 } }, 0, { 0, { 0.0, 0.0 } }, 0.0, 0.0, 1.4, 2.2, { 0.0, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 0, { { { 0 } } }, 0 };
static const DtFn D296 = { DT_RING, "a_ring", NULL, NULL, NULL, "tcolumn", NULL, NULL, 0, { 0, { 0.0, 0.0, 0.0, 0.0, 0.0 } }, 0, { 0, { 0.0, 0.0 } }, 5.0, 8.0, 1.8, 2.6, { 0.0, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 0, { { { 0 } } }, 0 };
static const DtFn D297 = { DT_BLOCK, "a_block", NULL, NULL, NULL, "tflag", NULL, NULL, 0, { 0, { 0.0, 0.0, 0.0, 0.0, 0.0 } }, 1, { 6, { 0.0, 0.0 } }, 0.0, 0.0, 0.8, 1.2, { 0.0, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 0, { { { 0 } } }, 0 };
static const DtFn D298 = { DT_OUTCROP, "a_outcrop", NULL, NULL, NULL, "tcolumn", NULL, NULL, 0, { 0, { 0.0, 0.0, 0.0, 0.0, 0.0 } }, 0, { 0, { 0.0, 0.0 } }, 0.0, 0.0, 0.0, 0.0, { 0.0, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 0, { { { 0 } } }, 0 };
static const DtFn D299 = { DT_PIT, "a_pit", NULL, NULL, NULL, "pit", NULL, NULL, 0, { 0, { 0.0, 0.0, 0.0, 0.0, 0.0 } }, 0, { 0, { 0.0, 0.0 } }, 0.0, 0.0, 0.6, 0.8, { 0.0, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 0, { { { 0 } } }, 0 };
static const DtFn D300 = { DT_OUTCROP, "a_outcrop", NULL, NULL, NULL, "shelf", NULL, NULL, 0, { 0, { 0.0, 0.0, 0.0, 0.0, 0.0 } }, 0, { 0, { 0.0, 0.0 } }, 0.0, 0.0, 0.0, 0.0, { 0.0, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 0, { { { 0 } } }, 0 };
static const DtFn D301 = { DT_OUTCROP, "a_outcrop", NULL, NULL, NULL, "shelf", NULL, NULL, 0, { 0, { 0.0, 0.0, 0.0, 0.0, 0.0 } }, 0, { 0, { 0.0, 0.0 } }, 0.0, 0.0, 0.0, 0.0, { 0.0, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 0, { { { 0 } } }, 0 };
static const DtFn D302 = { DT_PIT, "a_pit", NULL, NULL, NULL, "water", NULL, NULL, 0, { 0, { 0.0, 0.0, 0.0, 0.0, 0.0 } }, 0, { 0, { 0.0, 0.0 } }, 0.0, 0.0, 0.55, 0.7, { 0.0, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 0, { { { 0 } } }, 0 };
static const DtFn D303 = { DT_RING, "a_ring", NULL, NULL, NULL, "shelf", NULL, NULL, 0, { 0, { 0.0, 0.0, 0.0, 0.0, 0.0 } }, 0, { 0, { 0.0, 0.0 } }, 5.0, 7.0, 1.0, 1.6, { 0.0, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 0, { { { 0 } } }, 0 };
static const DtFn D304 = { DT_BLOCK, "a_block", NULL, NULL, NULL, "drift", NULL, NULL, 0, { 0, { 0.0, 0.0, 0.0, 0.0, 0.0 } }, 1, { 1, { 1.6, 0.0 } }, 0.0, 0.0, 0.7, 1.0, { 0.0, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 0, { { { 0 } } }, 0 };
static const DtFn *const DL_badlands[] = { &D0, &D1, &D2, &D3, &D4, &D5, &D6, &D7, &D8, &D9, &D10, &D11, &D12, &D13, &D14, &D15, &D16, &D17, &D18, &D19, &D20, &D21, &D22, &D23, &D24, &D25, &D26, &D27, &D28, &D29 };
static const DtFn *const DL_highland[] = { &D30, &D31, &D32, &D33, &D34, &D35, &D36, &D37, &D38, &D39, &D40, &D41, &D42, &D43, &D44, &D45, &D46, &D47, &D48, &D49, &D50, &D51, &D52, &D53, &D54, &D55, &D56, &D57, &D58, &D59 };
static const DtFn *const DL_maahl[] = { &D60, &D61, &D62, &D63, &D64, &D65, &D66, &D67, &D68, &D69, &D70, &D71, &D72, &D73, &D74, &D75, &D76, &D77, &D78, &D79, &D80, &D81, &D82, &D83, &D84, &D85, &D86, &D87, &D88, &D89 };
static const DtFn *const DL_corridor[] = { &D90, &D91, &D92, &D93, &D94, &D95, &D96, &D97, &D98, &D99, &D100, &D101, &D102, &D103, &D104, &D105, &D106, &D107, &D108, &D109, &D110, &D111, &D112, &D113, &D114, &D115, &D116, &D117, &D118, &D119 };
static const DtFn *const DL_infestation[] = { &D120, &D121, &D122, &D123, &D124, &D125, &D126, &D127, &D128, &D129, &D130, &D131, &D132, &D133, &D134, &D135, &D136, &D137, &D138, &D139, &D140, &D141, &D142, &D143, &D144, &D145, &D146, &D147, &D148, &D149 };
static const DtFn *const DL_urban[] = { &D150, &D151, &D152, &D153, &D154, &D155, &D156, &D157, &D158, &D159, &D160, &D161, &D162, &D163, &D164, &D165, &D166, &D167, &D168, &D169, &D170, &D171, &D172, &D173, &D174, &D175, &D176, &D177, &D178, &D179 };
static const DtFn *const DL_mine[] = { &D180, &D181, &D182, &D183, &D184, &D185, &D186, &D187, &D188, &D189, &D190, &D191, &D192, &D193, &D194, &D195, &D196, &D197, &D198, &D199, &D200, &D201, &D202, &D203, &D204, &D205, &D206, &D207, &D208, &D209 };
static const DtFn *const AL_badlands[] = { &D210, &D211, &D212, &D213, &D214 };
static const DtFn *const AL_highland[] = { &D215, &D216, &D217, &D218, &D219 };
static const DtFn *const AL_maahl[] = { &D220, &D221, &D222, &D223, &D224 };
static const DtFn *const AL_corridor[] = { &D225, &D226, &D227, &D228, &D229 };
static const DtFn *const AL_infestation[] = { &D230, &D231, &D232, &D233, &D234 };
static const DtFn *const AL_urban[] = { &D235, &D236, &D237, &D238, &D239 };
static const DtFn *const AL_mine[] = { &D240, &D241, &D242, &D243, &D244 };
static const DtFn *const AL_swamp[] = { &D245, &D246, &D247, &D248, &D249 };
static const DtFn *const AL_cynder[] = { &D250, &D251, &D252, &D253, &D254 };
static const DtFn *const AL_targa[] = { &D255, &D256, &D257, &D258, &D259 };
static const DtFn *const AL_trederra[] = { &D260, &D261, &D262, &D263, &D264 };
static const DtFn *const AL_fortress[] = { &D265, &D266, &D267, &D268, &D269 };
static const DtFn *const AL_beast[] = { &D270, &D271, &D272, &D273, &D274 };
static const DtFn *const AL_frontier[] = { &D275, &D276, &D277, &D278, &D279 };
static const DtFn *const AL_graveyard[] = { &D280, &D281, &D282, &D283, &D284 };
static const DtFn *const AL_forest[] = { &D285, &D286, &D287, &D288, &D289 };
static const DtFn *const AL_dungeon[] = { &D290, &D291, &D292, &D293, &D294 };
static const DtFn *const AL_temple[] = { &D295, &D296, &D297, &D298, &D299 };
static const DtFn *const AL_coast[] = { &D300, &D301, &D302, &D303, &D304 };
const DtList DT_DETAILS[] = {
    { "badlands", 30, DL_badlands },
    { "highland", 30, DL_highland },
    { "maahl", 30, DL_maahl },
    { "corridor", 30, DL_corridor },
    { "infestation", 30, DL_infestation },
    { "urban", 30, DL_urban },
    { "mine", 30, DL_mine },
};
const int DT_NDETAILS = 7;
const DtList DT_ANCHORS[] = {
    { "badlands", 5, AL_badlands },
    { "highland", 5, AL_highland },
    { "maahl", 5, AL_maahl },
    { "corridor", 5, AL_corridor },
    { "infestation", 5, AL_infestation },
    { "urban", 5, AL_urban },
    { "mine", 5, AL_mine },
    { "swamp", 5, AL_swamp },
    { "cynder", 5, AL_cynder },
    { "targa", 5, AL_targa },
    { "trederra", 5, AL_trederra },
    { "fortress", 5, AL_fortress },
    { "beast", 5, AL_beast },
    { "frontier", 5, AL_frontier },
    { "graveyard", 5, AL_graveyard },
    { "forest", 5, AL_forest },
    { "dungeon", 5, AL_dungeon },
    { "temple", 5, AL_temple },
    { "coast", 5, AL_coast },
};
const int DT_NANCHORS = 19;
/* @@END@@ */
