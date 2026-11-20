/* bld_features.c: features.py's b_* builders and L_* landmarks in C (see bld_features.h). Each builder function names
   the Python closure it copies; the comments keep the Python's order of random draws where it is not plain. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "bld_features.h"
#include "shp.h"
#include "leaf.h"
#include "pyexact.h"

#define PI 3.141592653589793

/* ================================================================== a small arena: every geometry made during a call */
typedef struct { GEOSGeometry **v; int n, cap; } Ar;
static GEOSGeometry *ar_put(Ar *A, GEOSGeometry *g) {
    if (!g) return NULL;
    if (A->n == A->cap) { A->cap = A->cap ? 2 * A->cap : 64; A->v = (GEOSGeometry **)realloc(A->v, sizeof(GEOSGeometry *) * (size_t)A->cap); }
    A->v[A->n++] = g; return g;
}
static GEOSGeometry *ar_geo(Ar *A, Geo *h) {          /* a leaf.h handle (rc 1) taken over into the arena */
    GEOSGeometry *g; if (!h) return NULL; g = h->g; h->g = NULL; geo_unref(h); return ar_put(A, g);
}
static void ar_free(Ar *A) { for (int i = 0; i < A->n; i++) shp_free(A->v[i]); free(A->v); memset(A, 0, sizeof *A); }
#define T(e) ar_put(A, (e))
#define TG(e) ar_geo(A, (e))
#define CK(x) do { if (!(x)) goto fail; } while (0)

/* a growing list of borrowed geometries (union inputs) */
typedef struct { const GEOSGeometry **v; int n, cap; } GL;
static void gp(GL *l, const GEOSGeometry *g) {
    if (l->n == l->cap) { l->cap = l->cap ? 2 * l->cap : 16; l->v = (const GEOSGeometry **)realloc(l->v, sizeof(*l->v) * (size_t)l->cap); }
    l->v[l->n++] = g;
}
static void gfree(GL *l) { free(l->v); memset(l, 0, sizeof *l); }

/* features._u(gs): unary_union([x for x in gs if x is not None and not x.is_empty]) (style_details.U) */
static GEOSGeometry *U(Ar *A, const GL *l) { return T(sd_U(l->v, l->n)); }
static GEOSGeometry *U2(Ar *A, const GEOSGeometry *a, const GEOSGeometry *b) { const GEOSGeometry *v[2] = { a, b }; return T(sd_U(v, 2)); }
static GEOSGeometry *Disc(Ar *A, double x, double y, double r, int n) { return T(shp_point_buffer(x, y, r, n)); }      /* _disc */
static GEOSGeometry *Ann(Ar *A, double x, double y, double ro, double ri, int n) { return T(so_ann(x, y, ro, ri, n)); }    /* _ann */
static GEOSGeometry *Cap(Ar *A, const double *p, const double *q, double w) { return T(so_cap(p, q, w)); }             /* _cap */
static GEOSGeometry *Bar(Ar *A, const double *p, const double *q, double w) { return T(so_bar(p, q, w)); }             /* _bar */
static GEOSGeometry *Rect(Ar *A, double cx, double cy, double l, double w, double a) { return T(shp_rect(cx, cy, l, w, a)); }
static GEOSGeometry *Inset(Ar *A, const GEOSGeometry *p, double d) { return T(shp_inset(p, d)); }
static GEOSGeometry *Stone(Ar *A, double cx, double cy, double r, PyRandom *rng, double aspect, int nv) { return TG(lf_stone(cx, cy, r, rng, aspect, nv, 0)); }
static GEOSGeometry *Hull(Ar *A, const GEOSGeometry *g) { return T(shp_convex_hull(g)); }
static GEOSGeometry *Diff(Ar *A, const GEOSGeometry *a, const GEOSGeometry *b) { return T(shp_difference(a, b)); }
static GEOSGeometry *Inter(Ar *A, const GEOSGeometry *a, const GEOSGeometry *b) { return T(shp_intersection(a, b)); }
static GEOSGeometry *Bufr(Ar *A, const GEOSGeometry *g, double d) { return T(shp_buffer_q(g, d, 16)); }
static GEOSGeometry *Poly(Ar *A, const double *xy, int n) { return T(shp_polygon(xy, n)); }
static void FF(const Frame *f, double u, double v, double *p) { lf_at(f, u, v, &p[0], &p[1]); }
/* _bar(ff(u0, v0), ff(u1, v1), w) */
static GEOSGeometry *BarF(Ar *A, const Frame *f, double u0, double v0, double u1, double v1, double w) { double p[2], q[2]; FF(f, u0, v0, p); FF(f, u1, v1, q); return Bar(A, p, q, w); }
static GEOSGeometry *CapF(Ar *A, const Frame *f, double u0, double v0, double u1, double v1, double w) { double p[2], q[2]; FF(f, u0, v0, p); FF(f, u1, v1, q); return Cap(A, p, q, w); }
/* rect(*ff(u, v), l, w, a) */
static GEOSGeometry *RectF(Ar *A, const Frame *f, double u, double v, double l, double w, double a) { double p[2]; FF(f, u, v, p); return Rect(A, p[0], p[1], l, w, a); }
/* p.exterior (AttributeError unless a Polygon), cloned */
static GEOSGeometry *Ext(Ar *A, const GEOSGeometry *p) {
    const GEOSGeometry *r; if (!p || shp_type(p) != GEOS_POLYGON) return NULL;
    r = shp_exterior(p); return r ? T(shp_clone(r)) : NULL;
}
/* features._biggest(g): the largest polygon of g (the first of equal areas), or None. *none = 1 for None */
static GEOSGeometry *Biggest(Ar *A, const GEOSGeometry *g, int *none) {
    ShpList ps = { 0 }; int i, b = -1; double ba = 0; GEOSGeometry *r;
    *none = 0;
    shp_polys_of(g, &ps);
    for (i = 0; i < ps.n; i++) { double ar = shp_area(ps.g[i]); if (b < 0 || ar > ba) { b = i; ba = ar; } }
    if (b < 0) { shp_list_free(&ps); *none = 1; return NULL; }
    r = ps.g[b]; ps.g[b] = NULL; shp_list_free(&ps);
    return T(r);
}
/* features.sh_hex_(x, y, r, a) */
static GEOSGeometry *Hex_(Ar *A, double x, double y, double r, double a) {
    double xy[12];
    for (int k = 0; k < 6; k++) { xy[2 * k] = x + r * py_cos(a + k * PI / 3); xy[2 * k + 1] = y + r * py_sin(a + k * PI / 3); }
    return Poly(A, xy, 6);
}
static double P(double x, double y) { double re = 0, im = 0; px_pow(x, y, &re, &im); return re; }        /* x ** y (floats) */
static double mx1(double x) { return x > 1.0 ? x : 1.0; }                                              /* max(1.0, x) */

/* ================================================================== layers */
static GEOSGeometry *cl(const GEOSGeometry *g) { return g ? shp_clone(g) : NULL; }
static SoVal gv(const GEOSGeometry *g) { return g ? lv_geom(shp_clone(g)) : lv_none(); }
static void L_up(SoLayers *L, const GEOSGeometry *g, double h, const char *k) { SoLayer *y = ly_new(4); y->v[0] = lv_str("up"); y->v[1] = gv(g); y->v[2] = lv_float(h); y->v[3] = lv_str(k); lyl_push_own(L, y); }
static void L_on(SoLayers *L, const GEOSGeometry *g, int64_t par, double h, const char *k) { SoLayer *y = ly_new(5); y->v[0] = lv_str("on"); y->v[1] = gv(g); y->v[2] = lv_int(par); y->v[3] = lv_float(h); y->v[4] = lv_str(k); lyl_push_own(L, y); }
static void L_tcut(SoLayers *L, const GEOSGeometry *g, int64_t par, double d) { SoLayer *y = ly_new(4); y->v[0] = lv_str("tcut"); y->v[1] = gv(g); y->v[2] = lv_int(par); y->v[3] = lv_float(d); lyl_push_own(L, y); }
static void L_gcut(SoLayers *L, const GEOSGeometry *g, double d, const char *k) { SoLayer *y = ly_new(4); y->v[0] = lv_str("gcut"); y->v[1] = gv(g); y->v[2] = lv_float(d); y->v[3] = lv_str(k); lyl_push_own(L, y); }
static void L_mound(SoLayers *L, const GEOSGeometry *g, double h, const char *k, const FtTiers *t) {
    SoLayer *y = ly_new(5); y->v[0] = lv_str("mound"); y->v[1] = gv(g); y->v[2] = lv_float(h); y->v[3] = lv_str(k); y->v[4] = lv_none(); y->v[4].t = LV_PTR; y->v[4].p = (void *)t; lyl_push_own(L, y);
}
static void L_pad(SoLayers *L, const GEOSGeometry *g, const char *k) { SoLayer *y = ly_new(3); y->v[0] = lv_str("pad"); y->v[1] = gv(g); y->v[2] = lv_str(k); lyl_push_own(L, y); }
static void put_foot(Geo **foot, const GEOSGeometry *g) { *foot = g ? geo_new(shp_clone(g)) : NULL; }

/* ================================================================== the builders' arguments */
/* ctx: up to 4 strings, 8 numbers, 4 ints / flags, and tiers of its own (b_fairy_ring tiers=) */
typedef struct { const char *s[4]; double v[8]; int i[4]; FtTiers tiers; } BC;
#define BUILD(name) static int name(void *c_, double cx, double cy, double a, PyRandom *rng, int hs, double k, Geo **foot, SoLayers *L)
#define NOK do { if (hs) return SC_ERR; (void)k; } while (0)          /* f(cx, cy, a, rng): a fifth argument is a TypeError */
#define KOPT do { if (!hs) k = 1.0; } while (0)                       /* f(cx, cy, a, rng, k=1.0) */
#define KREQ do { if (!hs) return SC_ERR; } while (0)                 /* f(cx, cy, a, rng, k) */
#define BEGIN BC *c = (BC *)c_; Ar AR = { 0 }, *A = &AR; int R_ = SC_ERR; (void)c; (void)A; *foot = NULL
#define END_OK R_ = 0; fail: ar_free(A); return R_

/* b_wheel(kind="wood", iron="iron") */
BUILD(B_wheel) {
    BEGIN; GL u = { 0 }; GEOSGeometry *g, *d; double R; int none;
    NOK;
    R = py_uniform(rng, 2.4, 3.0);
    CK(d = Ann(A, cx, cy, R, R - 0.95, 24)); gp(&u, d);
    for (int j = 0; j < 3; j++) {
        double p[2] = { cx - (R - 0.3) * py_cos(a + j * PI / 3), cy - (R - 0.3) * py_sin(a + j * PI / 3) }, q[2] = { cx + (R - 0.3) * py_cos(a + j * PI / 3), cy + (R - 0.3) * py_sin(a + j * PI / 3) };
        CK(d = Bar(A, p, q, 0.92)); gp(&u, d);
    }
    CK(d = Disc(A, cx, cy, 0.95, 20)); gp(&u, d);
    CK(g = U(A, &u));
    if (py_random(rng) < 0.5) {          /* a broken wheel: one piece of the rim gone */
        double b = py_uniform(rng, 0, 6.283), xy[6] = { cx, cy, cx + 2 * R * py_cos(b), cy + 2 * R * py_sin(b), cx + 2 * R * py_cos(b + 0.7), cy + 2 * R * py_sin(b + 0.7) };
        GEOSGeometry *tri, *dd;
        CK(tri = Poly(A, xy, 3)); CK(dd = Disc(A, cx, cy, 0.9, 20)); CK(tri = Diff(A, tri, dd)); CK(g = Diff(A, g, tri));
        g = Biggest(A, g, &none); if (!g && !none) goto fail;
    }
    CK(d = Disc(A, cx, cy, R, 20)); put_foot(foot, d);
    L_up(L, g, 0.5, c->s[0]);
    CK(d = Disc(A, cx, cy, 0.6, 20)); L_on(L, d, 0, 0.3, c->s[1]);
    R_ = 0;
fail: gfree(&u); ar_free(A); return R_;
}

/* b_ladder(kind="wood") */
BUILD(B_ladder) {
    BEGIN; GL u = { 0 }; GEOSGeometry *g, *d; double Ln, us[512]; int miss, n; Frame ff;
    NOK;
    Ln = py_uniform(rng, 6.0, 8.5); ff = lf_frame(cx, cy, a); miss = (int)py_randint(rng, 0, 5);
    for (int j = 0; j < 2; j++) { double v = j ? 1.05 : -1.05; CK(d = BarF(A, &ff, -Ln / 2, v, Ln / 2, v, 0.92)); gp(&u, d); }
    n = lf_arange(-Ln / 2 + 0.9, Ln / 2 - 0.6, 1.7, us, 512); CK(n >= 0);
    for (int i = 0; i < n; i++) if (i != miss) { CK(d = BarF(A, &ff, us[i], -1.2, us[i], 1.2, 0.92)); gp(&u, d); }
    CK(g = U(A, &u)); CK(d = Hull(A, g)); put_foot(foot, d);
    L_up(L, g, 0.45, c->s[0]);
    R_ = 0;
fail: gfree(&u); ar_free(A); return R_;
}

/* b_track(rail="iron", tie="wood", L=(8.0, 11.5)): f(cx, cy, a, rng, k=1.0) */
static int track(BC *c, double cx, double cy, double a, PyRandom *rng, double k, double L0, double L1, Geo **foot, SoLayers *L) {
    Ar AR = { 0 }, *A = &AR; int R_ = SC_ERR; GL u = { 0 }, w = { 0 }; GEOSGeometry *ties, *rails, *d; double Ln, us[2048]; int n; Frame ff;
    *foot = NULL;
    Ln = py_uniform(rng, L0, L1) * k; ff = lf_frame(cx, cy, a);
    n = lf_arange(-Ln / 2 + 0.6, Ln / 2 - 0.3, 1.65, us, 2048); CK(n >= 0);
    for (int i = 0; i < n; i++) {
        double p[2], t1 = py_uniform(rng, -0.1, 0.1), t2;
        FF(&ff, us[i] + t1, 0, p); t2 = py_uniform(rng, -0.06, 0.06);
        CK(d = Rect(A, p[0], p[1], 0.95, 3.6, a + t2)); gp(&u, d);
    }
    CK(ties = U(A, &u));
    for (int j = 0; j < 2; j++) { double v = j ? 1.15 : -1.15; CK(d = BarF(A, &ff, -Ln / 2, v, Ln / 2, v, 0.92)); gp(&w, d); }
    CK(rails = U(A, &w));
    CK(d = Rect(A, cx, cy, Ln, 3.8, a)); put_foot(foot, d);
    L_up(L, ties, 0.4, c->s[1]); L_up(L, rails, 0.85, c->s[0]);
    R_ = 0;
fail: gfree(&u); gfree(&w); ar_free(A); return R_;
}
BUILD(B_track) { BC *c = (BC *)c_; KOPT; return track(c, cx, cy, a, rng, k, c->v[0], c->v[1], foot, L); }

/* b_campfire(stone_kind="rock", log="wood") */
BUILD(B_campfire) {
    BEGIN; GL u = { 0 }; GEOSGeometry *st[16], *logs, *d; double R, b; int n;
    NOK;
    n = (int)py_randint(rng, 6, 7); R = py_uniform(rng, 2.2, 2.5);
    for (int i = 0; i < n; i++) {
        double r = py_uniform(rng, 0.6, 0.75); int nv = (int)py_randint(rng, 5, 7);
        CK(st[i] = Stone(A, cx + R * py_cos(a + 6.283 * i / n), cy + R * py_sin(a + 6.283 * i / n), r, rng, 0.85, nv));
    }
    b = py_uniform(rng, 0, PI);
    for (int j = 0; j < 2; j++) {
        double dd = j ? 1.2 : 0, p[2] = { cx - 1.15 * py_cos(b + dd), cy - 1.15 * py_sin(b + dd) }, q[2] = { cx + 1.15 * py_cos(b + dd), cy + 1.15 * py_sin(b + dd) };
        CK(d = Cap(A, p, q, 0.92)); gp(&u, d);
    }
    CK(logs = U(A, &u));
    CK(d = Disc(A, cx, cy, R + 0.7, 20)); put_foot(foot, d);
    for (int i = 0; i < n; i++) L_mound(L, st[i], py_uniform(rng, 0.45, 0.6), c->s[0], &FT_PEB_T);
    L_up(L, logs, 0.5, c->s[1]);
    R_ = 0;
fail: gfree(&u); ar_free(A); return R_;
}

/* b_stairs(kind="tflag", steps=3) */
BUILD(B_stairs) {
    BEGIN; GEOSGeometry *base, *q; double Ln, W; Frame ff; int steps = c->i[0];
    NOK;
    Ln = py_uniform(rng, 4.6, 5.6); W = py_uniform(rng, 3.0, 3.8); ff = lf_frame(cx, cy, a);
    CK(base = Rect(A, cx, cy, Ln, W, a));
    L_up(L, base, 0.5, c->s[0]);
    for (int i = 1; i < steps; i++) { CK(q = RectF(A, &ff, i * 0.62, 0, Ln - i * 1.24, W, a)); L_on(L, q, i - 1, 0.5, c->s[0]); }
    put_foot(foot, base);
    END_OK;
}

/* b_well(wall="rock", water="water") */
BUILD(B_well) {
    BEGIN; GL u = { 0 }; GEOSGeometry *ring_, *seams, *d; double R; int n;
    NOK;
    R = py_uniform(rng, 2.3, 2.9); n = (int)py_randint(rng, 7, 10);
    CK(ring_ = Ann(A, cx, cy, R, R - 0.95, n));
    for (int i = 0; i < n; i++) {
        double t = a + 6.283 * i / n, p[2] = { cx + (R - 1.1) * py_cos(t), cy + (R - 1.1) * py_sin(t) }, q[2] = { cx + (R + 0.2) * py_cos(t), cy + (R + 0.2) * py_sin(t) };
        CK(d = Bar(A, p, q, 0.26)); gp(&u, d);
    }
    CK(seams = U(A, &u));
    CK(d = Disc(A, cx, cy, R, 20)); put_foot(foot, d);
    L_up(L, ring_, py_uniform(rng, 1.1, 1.6), c->s[0]); L_tcut(L, seams, 0, 0.25);
    CK(d = Disc(A, cx, cy, R - 1.3, 16)); L_gcut(L, d, 0.8, c->s[1]);
    R_ = 0;
fail: gfree(&u); ar_free(A); return R_;
}

/* b_fallen_column(kind="tcolumn", w=1.8): f(cx, cy, a, rng, k=1.0) */
BUILD(B_fallen_column) {
    BEGIN; GL dr = { 0 }, se = { 0 }; GEOSGeometry *g, *d; double u, W; Frame ff; int nd;
    KOPT;
    ff = lf_frame(cx, cy, a); u = -4.0 * k; W = c->v[0] * mx1(P(k, 0.5));
    nd = (int)py_randint(rng, 3, 4) + (int)(k > 1.5);
    for (int i = 0; i < nd; i++) {
        double Ld = py_uniform(rng, 1.8, 2.8) * mx1(P(k, 0.6)), v, da, cc[2]; Frame f2;
        v = py_uniform(rng, -0.35, 0.35) * (double)(i > 0); da = py_uniform(rng, -0.18, 0.18) * (double)(i > 0);
        FF(&ff, u + Ld / 2, v, cc); CK(d = Rect(A, cc[0], cc[1], Ld, W, a + da)); gp(&dr, d);
        f2 = lf_frame(cc[0], cc[1], a + da);
        CK(d = BarF(A, &f2, -Ld / 2, -W / 4, Ld / 2, -W / 4, 0.24)); gp(&se, d);
        CK(d = BarF(A, &f2, -Ld / 2, W / 4, Ld / 2, W / 4, 0.24)); gp(&se, d);
        u += Ld + py_uniform(rng, 0.45, 1.1);
    }
    CK(g = U(A, &dr)); CK(d = Hull(A, g)); put_foot(foot, d);
    for (int i = 0; i < dr.n; i++) L_up(L, dr.v[i], py_uniform(rng, 1.0, 1.35) * mx1(P(k, 0.3)), c->s[0]);
    for (int i = 0; i < dr.n; i++) { CK(d = U(A, &se)); L_tcut(L, d, i, 0.22); }
    R_ = 0;
fail: gfree(&dr); gfree(&se); ar_free(A); return R_;
}

/* b_grate(kind="iron", round_=False) */
BUILD(B_grate) {
    BEGIN; GL u = { 0 }; GEOSGeometry *plate, *slots, *d; double S, vs[512]; Frame ff; int n;
    NOK;
    S = py_uniform(rng, 3.0, 3.8); ff = lf_frame(cx, cy, a);
    CK(plate = c->i[0] ? Disc(A, cx, cy, S / 2, 28) : Rect(A, cx, cy, S, S, a));
    n = lf_arange(-S / 2 + 0.75, S / 2 - 0.5, 0.78, vs, 512); CK(n >= 0);
    for (int i = 0; i < n; i++) { CK(d = BarF(A, &ff, -S, vs[i], S, vs[i], 0.36)); gp(&u, d); }
    CK(slots = U(A, &u)); CK(d = Inset(A, plate, 0.5)); CK(slots = Inter(A, slots, d));
    put_foot(foot, plate);
    L_up(L, plate, 0.38, c->s[0]); L_tcut(L, slots, 0, 0.3);
    R_ = 0;
fail: gfree(&u); ar_free(A); return R_;
}

/* b_rune_circle(kind="glyph", R=(3.0, 4.0)): f(cx, cy, a, rng, k=1.0) */
static int rune_circle(const char *kind, double R0, double R1, double cx, double cy, double a, PyRandom *rng, double k, Geo **foot, SoLayers *L) {
    Ar AR = { 0 }, *A = &AR; int R_ = SC_ERR, n; GL u = { 0 }; GEOSGeometry *g, *d; double Rr, w = 0.4;
    static const double SEG[4][4] = { { -0.45, -0.45, 0.45, 0.45 }, { -0.45, 0.45, 0.45, -0.45 }, { 0, -0.55, 0, 0.55 }, { -0.5, 0, 0.5, 0 } };
    GL marks = { 0 };
    *foot = NULL;
    Rr = py_uniform(rng, R0, R1) * k;
    n = (int)py_randint(rng, 6, 9) + (int)(4 * (k - 1));
    for (int i = 0; i < n; i++) {
        double t = a + 6.283 * i / n, rm = Rr * 0.8, mx = cx + rm * py_cos(t), my = cy + rm * py_sin(t); Frame f2 = lf_frame(mx, my, t); int64_t sel[2];
        CK(py_sample_e(rng, 4, 2, sel) == 0);
        for (int j = 0; j < 2; j++) { const double *s = SEG[sel[j]]; CK(d = BarF(A, &f2, s[0], s[1], s[2], s[3], 0.3)); gp(&marks, d); }
    }
    CK(d = Ann(A, cx, cy, Rr, Rr - w, 40)); gp(&u, d);
    CK(d = Ann(A, cx, cy, Rr * 0.6, Rr * 0.6 - w, 32)); gp(&u, d);
    { int64_t ch; GEOSGeometry *st; CK(py_choice_e(rng, 3, &ch) == 0);
      CK(st = T(shp_star(cx, cy, Rr * 0.42, Rr * 0.17, (int)(5 + ch), a))); CK(st = Bufr(A, st, 0)); CK(st = Ext(A, st)); CK(st = Bufr(A, st, 0.2)); gp(&u, st); }
    for (int i = 0; i < marks.n; i++) gp(&u, marks.v[i]);
    CK(g = U(A, &u));
    CK(d = Disc(A, cx, cy, Rr + 0.2, 32)); put_foot(foot, d);
    L_gcut(L, g, 0.35, kind);
    R_ = 0;
fail: gfree(&u); gfree(&marks); ar_free(A); return R_;
}
BUILD(B_rune_circle) { BC *c = (BC *)c_; KOPT; return rune_circle(c->s[0], c->v[0], c->v[1], cx, cy, a, rng, k, foot, L); }

/* b_crystals(kind="crystal", hmax=3.0): f(cx, cy, a, rng, k=1.0) */
static int crystals(const char *kind, double hmax, double cx, double cy, double a, PyRandom *rng, double k, Geo **foot, SoLayers *L) {
    Ar AR = { 0 }, *A = &AR; int R_ = SC_ERR, n; GL ft = { 0 }; GEOSGeometry *q, *d; double kk;
    *foot = NULL;
    kk = mx1(P(k, 0.6));
    { double r = py_uniform(rng, 1.0, 1.35) * kk; CK(q = Hex_(A, cx, cy, r, a)); L_mound(L, q, py_uniform(rng, 2.2, hmax), kind, &FT_SPIKE_T); gp(&ft, q); }
    n = (int)py_randint(rng, 4, 6) + (int)(3 * (k - 1));
    for (int i = 0; i < n; i++) {
        double t = py_uniform(rng, 0, 6.283), d0, dd, r; int ok = 1;
        d0 = py_uniform(rng, 1.7, 2.4) * kk; dd = d0 * (1 + 0.5 * (k - 1) * py_random(rng)); r = py_uniform(rng, 0.55, 0.85) * kk;
        if (py_random(rng) < 0.6) { double rot = py_uniform(rng, 0, 1); CK(q = Hex_(A, cx + dd * py_cos(t), cy + dd * py_sin(t), r, rot)); }
        else CK(q = Stone(A, cx + dd * py_cos(t), cy + dd * py_sin(t), r, rng, 0.7, 3));
        for (int j = 0; j < ft.n; j++) { double ds = shp_distance(q, ft.v[j]); if (!(ds > 0.25)) { ok = 0; break; } }
        if (ok) { gp(&ft, q); L_mound(L, q, py_uniform(rng, 1.0, 1.9), kind, &FT_SPIKE_T); }
    }
    CK(d = U(A, &ft)); CK(d = Hull(A, d)); put_foot(foot, d);
    R_ = 0;
fail: gfree(&ft); ar_free(A); if (R_ < 0) lyl_free(L); return R_;
}
BUILD(B_crystals) { BC *c = (BC *)c_; KOPT; return crystals(c->s[0], c->v[0], cx, cy, a, rng, k, foot, L); }

/* b_roots(kind="stump", root="root", R=(1.3, 1.7)): f(cx, cy, a, rng, k=1.0) */
static int roots(const char *kind, const char *root, double R0, double R1, double cx, double cy, double a, PyRandom *rng, double k, Geo **foot, SoLayers *L) {
    Ar AR = { 0 }, *A = &AR; int R_ = SC_ERR, n; GL parts = { 0 }, rg = { 0 }; GEOSGeometry *st, *rts, *rings, *d; double Rr;
    *foot = NULL;
    Rr = py_uniform(rng, R0, R1) * mx1(P(k, 0.8)); CK(st = Stone(A, cx, cy, Rr, rng, 0.9, 9));
    n = (int)py_randint(rng, 4, 6) + (int)(2 * (k - 1));
    for (int i = 0; i < n; i++) {
        double t = a + 6.283 * i / 5 + py_uniform(rng, -0.3, 0.3), Ln = py_uniform(rng, 3.0, 4.8) * mx1(P(k, 0.8)), p0[2], p1[2], pts[10], j1, j2;
        p0[0] = cx + (Rr - 0.3) * py_cos(t); p0[1] = cy + (Rr - 0.3) * py_sin(t);
        j1 = py_uniform(rng, -0.35, 0.35); p1[0] = cx + (Rr + Ln) * py_cos(t + j1);
        j2 = py_uniform(rng, -0.35, 0.35); p1[1] = cy + (Rr + Ln) * py_sin(t + j2);
        int np_ = lf_wiggle(p0[0], p0[1], p1[0], p1[1], rng, 0.45, 4, pts);
        for (int j = 0; j < np_ - 1; j++) {
            double w0 = 1.15 - 0.08 * j; w0 = w0 > 0.92 ? w0 : 0.92;
            CK(d = Cap(A, &pts[2 * j], &pts[2 * j + 2], w0 * mx1(P(k, 0.4)))); gp(&parts, d);
        }
    }
    CK(rts = U(A, &parts)); CK(d = Bufr(A, st, -0.25)); CK(rts = Diff(A, rts, d));
    CK(d = Ann(A, cx, cy, Rr * 0.45, Rr * 0.45 - 0.25, 18)); gp(&rg, d);
    CK(d = Ann(A, cx, cy, Rr * 0.75, Rr * 0.75 - 0.25, 18)); gp(&rg, d);
    CK(rings = U(A, &rg));
    CK(d = U2(A, st, rts)); CK(d = Hull(A, d)); CK(d = Bufr(A, d, -0.2)); put_foot(foot, d);
    L_up(L, rts, 0.5, root); L_up(L, st, py_uniform(rng, 1.2, 1.9), kind); L_tcut(L, rings, 1, 0.25);
    R_ = 0;
fail: gfree(&parts); gfree(&rg); ar_free(A); return R_;
}
BUILD(B_roots) { BC *c = (BC *)c_; KOPT; return roots(c->s[0], c->s[1], c->v[0], c->v[1], cx, cy, a, rng, k, foot, L); }

/* b_stepping_pool(stone_kind="rock", water="water"): f(cx, cy, a, rng, k=1.0) */
static int stepping_pool(const char *sk, const char *water, double cx, double cy, double a, PyRandom *rng, double k, Geo **foot, SoLayers *L) {
    Ar AR = { 0 }, *A = &AR; int R_ = SC_ERR, n; GL pads = { 0 }; GEOSGeometry *pool, *q, *d; Frame ff;
    *foot = NULL;
    { double r = py_uniform(rng, 3.0, 3.8) * k; int kb = (int)py_randint(rng, 4, 6); CK(pool = TG(lf_blob(cx, cy, r, rng, kb))); }
    ff = lf_frame(cx, cy, a);
    n = (int)(4 * k) + 1;
    for (int i = 0; i < n; i++) {
        double p[2], v = py_uniform(rng, -0.5, 0.5), r; int nv, in;
        FF(&ff, (i - (n - 1) / 2.0) * 1.9, v, p); r = py_uniform(rng, 0.7, 0.95); nv = (int)py_randint(rng, 5, 7);
        CK(q = Stone(A, p[0], p[1], r, rng, 0.85, nv));
        CK(d = Inset(A, pool, 0.3)); in = shp_contains(d, q); CK(in >= 0);
        if (in) gp(&pads, q);
    }
    put_foot(foot, pool);
    L_gcut(L, pool, 0.55, water);
    if (pads.n) { CK(d = U(A, &pads)); L_pad(L, d, sk); }
    R_ = 0;
fail: gfree(&pads); ar_free(A); return R_;
}
BUILD(B_stepping_pool) { BC *c = (BC *)c_; KOPT; return stepping_pool(c->s[0], c->s[1], cx, cy, a, rng, k, foot, L); }

/* b_tool(which="pick", metal="iron", wood="wood"): no random numbers */
BUILD(B_tool) {
    BEGIN; GL u = { 0 }; GEOSGeometry *g, *d, *h; Frame ff; const char *w = c->s[0], *metal = c->s[1], *wood = c->s[2]; double xy[24], p[2];
    NOK;
    ff = lf_frame(cx, cy, a);
#define PT(i, uu, vv) FF(&ff, uu, vv, &xy[2 * (i)])
    if (!strcmp(w, "sword")) {
        PT(0, -1.6, -0.48); PT(1, 2.9, -0.42); PT(2, 3.8, 0); PT(3, 2.9, 0.42); PT(4, -1.6, 0.48);
        CK(d = Poly(A, xy, 5)); gp(&u, d);
        CK(d = RectF(A, &ff, -1.6, 0, 0.92, 2.3, a)); gp(&u, d);
        CK(d = BarF(A, &ff, -3.0, 0, -1.6, 0, 0.92)); gp(&u, d);
        FF(&ff, -3.1, 0, p); CK(d = Disc(A, p[0], p[1], 0.55, 10)); gp(&u, d);
        CK(g = U(A, &u)); CK(d = Hull(A, g)); put_foot(foot, d); L_up(L, g, 0.4, metal);
    } else if (!strcmp(w, "spear")) {
        PT(0, 3.0, -0.65); PT(1, 4.9, 0); PT(2, 3.0, 0.65); PT(3, 2.5, 0); CK(h = Poly(A, xy, 4));
        CK(d = Rect(A, cx, cy, 9.6, 1.1, a)); put_foot(foot, d);
        CK(d = BarF(A, &ff, -4.6, 0, 3.0, 0, 0.92)); L_up(L, d, 0.4, wood); L_up(L, h, 0.5, metal);
    } else if (!strcmp(w, "rifle")) {
        CK(d = BarF(A, &ff, -1.0, 0, 3.8, 0, 0.95)); gp(&u, d);
        PT(0, -3.4, -0.95); PT(1, -1.0, -0.5); PT(2, 0.3, -0.5); PT(3, 0.3, 0.5); PT(4, -1.0, 0.5); PT(5, -3.4, 0.4); CK(d = Poly(A, xy, 6)); gp(&u, d);
        CK(g = U(A, &u)); CK(d = Hull(A, g)); put_foot(foot, d); L_up(L, g, 0.45, wood);
        CK(d = BarF(A, &ff, 0.3, 0, 3.8, 0, 0.92)); L_on(L, d, 0, 0.25, metal);
    } else if (!strcmp(w, "shovel")) {
        CK(h = RectF(A, &ff, 3.0, 0, 1.9, 1.5, a)); CK(h = Bufr(A, h, 0.25));
        CK(d = Rect(A, cx, cy, 8.4, 2.0, a)); put_foot(foot, d);
        CK(d = BarF(A, &ff, -3.6, 0, 2.2, 0, 0.92)); L_up(L, d, 0.4, wood); L_up(L, h, 0.45, metal);
    } else if (!strcmp(w, "anchor")) {
        double ts[9], pts[18];
        CK(lf_linspace(-1.0, 1.0, 9, ts) == 9);
        for (int i = 0; i < 9; i++) FF(&ff, 2.6 * py_cos(ts[i]) - 0.6, 2.6 * py_sin(ts[i]), &pts[2 * i]);
        CK(d = BarF(A, &ff, -3.6, 0, 2.0, 0, 0.85)); gp(&u, d);
        CK(d = BarF(A, &ff, -2.6, -1.5, -2.6, 1.5, 0.92)); gp(&u, d);
        CK(d = T(shp_line_buffer(pts, 9, 0.47, SHP_CAP_ROUND, SHP_JOIN_ROUND, 6, 5.0))); gp(&u, d);
        FF(&ff, -4.3, 0, p); CK(d = Ann(A, p[0], p[1], 1.0, 0.3, 14)); gp(&u, d);
        CK(g = U(A, &u)); CK(d = Hull(A, g)); put_foot(foot, d); L_up(L, g, 0.5, metal);
    } else if (!strcmp(w, "axe")) {
        PT(0, 2.2, -0.3); PT(1, 3.3, -1.3); PT(2, 3.9, -0.9); PT(3, 3.9, 0.9); PT(4, 3.3, 1.3); PT(5, 2.2, 0.3); CK(h = Poly(A, xy, 6));
        CK(d = Rect(A, cx, cy, 7.4, 2.8, a)); put_foot(foot, d);
        CK(d = BarF(A, &ff, -3.4, 0, 3.0, 0, 0.92)); L_up(L, d, 0.4, wood); L_up(L, h, 0.5, metal);
    } else if (!strcmp(w, "wrench")) {
        GEOSGeometry *jaw; FF(&ff, 3.0, 0, p); CK(jaw = Ann(A, p[0], p[1], 1.2, 0.4, 12)); CK(d = RectF(A, &ff, 3.9, 0, 1.4, 0.8, a)); CK(jaw = Diff(A, jaw, d));
        CK(d = BarF(A, &ff, -2.8, 0, 2.2, 0, 0.85)); gp(&u, d); gp(&u, jaw);
        FF(&ff, -3.2, 0, p); CK(d = Ann(A, p[0], p[1], 1.0, 0.3, 12)); gp(&u, d);
        CK(g = U(A, &u)); CK(d = Hull(A, g)); put_foot(foot, d); L_up(L, g, 0.4, metal);
    } else {          /* pick */
        double ts[9], pts[18];
        CK(lf_linspace(-1.05, 1.05, 9, ts) == 9);
        for (int i = 0; i < 9; i++) FF(&ff, 2.6 + 0.9 * py_cos(ts[i]) - 0.9, 2.3 * py_sin(ts[i]), &pts[2 * i]);
        CK(h = T(shp_line_buffer(pts, 9, 0.47, SHP_CAP_ROUND, SHP_JOIN_ROUND, 6, 5.0)));
        CK(d = Rect(A, cx, cy, 7.4, 4.8, a)); put_foot(foot, d);
        CK(d = BarF(A, &ff, -3.4, 0, 2.7, 0, 0.92)); L_up(L, d, 0.4, wood); L_up(L, h, 0.5, metal);
    }
#undef PT
    R_ = 0;
fail: gfree(&u); ar_free(A); return R_;
}

/* b_boss(kind="iron", boss="iron", R=(1.6, 2.1), cross=False) */
BUILD(B_boss) {
    BEGIN; GL u = { 0 }; GEOSGeometry *g, *gr, *d; double Rr;
    NOK;
    Rr = py_uniform(rng, c->v[0], c->v[1]); CK(g = Disc(A, cx, cy, Rr, 28));
    CK(d = Ann(A, cx, cy, Rr - 0.4, Rr - 0.65, 28)); gp(&u, d);
    if (c->i[0]) for (int j = 0; j < 2; j++) {
        double dd = j ? PI / 2 : 0, p[2] = { cx - Rr * py_cos(a + dd), cy - Rr * py_sin(a + dd) }, q[2] = { cx + Rr * py_cos(a + dd), cy + Rr * py_sin(a + dd) };
        CK(d = Bar(A, p, q, 0.26)); gp(&u, d);
    }
    put_foot(foot, g);
    L_up(L, g, 0.45, c->s[0]);
    CK(gr = U(A, &u)); CK(d = Disc(A, cx, cy, 0.75, 20)); CK(gr = Diff(A, gr, d)); L_tcut(L, gr, 0, 0.25);
    CK(d = Disc(A, cx, cy, 0.6, 16)); L_on(L, d, 0, 0.35, c->s[1]);
    R_ = 0;
fail: gfree(&u); ar_free(A); if (R_ < 0) { geo_unref(*foot); *foot = NULL; } return R_;
}

/* b_nest(rim="twig", egg="egg", n=(2, 4)): f(cx, cy, a, rng, k=1.0) */
static int nest(const char *rim, const char *egg, int n0, int n1, double cx, double cy, double a, PyRandom *rng, double k, Geo **foot, SoLayers *L) {
    Ar AR = { 0 }, *A = &AR; int R_ = SC_ERR, n; GL eggs = { 0 }; GEOSGeometry *rim_, *q, *d; double R;
    *foot = NULL;
    R = py_uniform(rng, 2.6, 3.1) * k;
    CK(rim_ = T(shp_star(cx, cy, R, R * 0.9, (int)(13 * k), a))); CK(d = Disc(A, cx, cy, R - 1.15 * mx1(P(k, 0.5)), 18)); CK(rim_ = Diff(A, rim_, d));
    n = (int)py_randint(rng, n0, n1) + (int)(3 * (k - 1));
    for (int i = 0; i < n; i++) {
        double t = py_uniform(rng, 0, 6.283), lim = R - 2.05 * mx1(P(k, 0.5)), dd, ang; int ok = 1;
        dd = py_uniform(rng, 0, lim > 0.1 ? lim : 0.1);
        CK(q = Disc(A, cx + dd * py_cos(t), cy + dd * py_sin(t), 1, 14)); CK(q = T(shp_scale(q, 0.5, 0.66, SHP_ORIGIN_CENTER, 0, 0)));
        ang = py_uniform(rng, 0, 180); CK(q = T(shp_rotate(q, ang, SHP_ORIGIN_CENTER, 0, 0, 0)));
        for (int j = 0; j < eggs.n; j++) if (!(shp_distance(q, eggs.v[j]) > 0.2)) { ok = 0; break; }
        if (ok) gp(&eggs, q);
    }
    CK(d = Disc(A, cx, cy, R, 20)); put_foot(foot, d);
    L_up(L, rim_, py_uniform(rng, 0.5, 0.7), rim);
    for (int i = 0; i < eggs.n; i++) L_mound(L, eggs.v[i], py_uniform(rng, 0.5, 0.65), egg, &FT_PEB_T);
    R_ = 0;
fail: gfree(&eggs); ar_free(A); return R_;
}
BUILD(B_nest) { BC *c = (BC *)c_; KOPT; return nest(c->s[0], c->s[1], c->i[0], c->i[1], cx, cy, a, rng, k, foot, L); }

/* b_crater(rim="rock", pit="pit", R=(2.4, 3.2)): f(cx, cy, a, rng, k=1.0); 1 = None */
static int crater(const char *rim, const char *pit, double R0, double R1, double cx, double cy, double a, PyRandom *rng, double k, Geo **foot, SoLayers *L) {
    Ar AR = { 0 }, *A = &AR; int R_ = SC_ERR, n, e; GEOSGeometry *outer, *inner, *d, *q; double Rr, t;
    *foot = NULL;
    Rr = py_uniform(rng, R0, R1) * k;
    { double asp = py_uniform(rng, 0.85, 1.0); int nv = (int)py_randint(rng, 9, 12); CK(outer = Stone(A, cx, cy, Rr, rng, asp, nv)); }
    t = 0.95 * mx1(P(k, 0.5));
    CK(inner = Inset(A, outer, t));
    e = shp_is_empty(inner); CK(e >= 0);
    if (e) { R_ = 1; goto fail; }
    CK(d = Diff(A, outer, inner)); L_up(L, d, py_uniform(rng, 0.55, 0.85), rim);
    CK(d = Inset(A, inner, 0.3)); L_gcut(L, d, py_uniform(rng, 0.6, 0.92), pit);
    n = (int)py_randint(rng, 4, 7) + (int)(4 * (k - 1));
    for (int i = 0; i < n; i++) {
        double b = py_uniform(rng, 0, 6.283), dd = Rr + py_uniform(rng, 0.8, 2.0), r = py_uniform(rng, 0.6, 0.8);
        CK(q = Stone(A, cx + dd * py_cos(b), cy + dd * py_sin(b), r, rng, 0.85, 6));
        L_mound(L, q, py_uniform(rng, 0.4, 0.55), rim, &FT_PEB_T);
    }
    CK(d = Disc(A, cx, cy, Rr + 2.4, 24)); put_foot(foot, d);
    R_ = 0;
fail: ar_free(A); if (R_ != 0) lyl_free(L); return R_;
}
BUILD(B_crater) { BC *c = (BC *)c_; KOPT; return crater(c->s[0], c->s[1], c->v[0], c->v[1], cx, cy, a, rng, k, foot, L); }

/* b_boardwalk(kind="board", beam="wood", L=(5.5, 8.0)): f(cx, cy, a, rng, k=1.0) */
static int boardwalk(const char *kind, const char *beam, double L0, double L1, double cx, double cy, double a, PyRandom *rng, double k, Geo **foot, SoLayers *L) {
    Ar AR = { 0 }, *A = &AR; int R_ = SC_ERR, n, miss; GL b = { 0 }, p = { 0 }; GEOSGeometry *beams, *planks, *d; double Ln, us[2048]; Frame ff;
    *foot = NULL;
    Ln = py_uniform(rng, L0, L1) * k; ff = lf_frame(cx, cy, a); miss = (int)py_randint(rng, 0, 9);
    for (int j = 0; j < 2; j++) { double v = j ? 1.35 : -1.35; CK(d = BarF(A, &ff, -Ln / 2, v, Ln / 2, v, 0.92)); gp(&b, d); }
    CK(beams = U(A, &b));
    n = lf_arange(-Ln / 2 + 0.6, Ln / 2 - 0.3, 1.22, us, 2048); CK(n >= 0);
    for (int i = 0; i < n; i++) if (i != miss) {
        double q[2], v = py_uniform(rng, -0.15, 0.15), w, da;
        FF(&ff, us[i], v, q); w = py_uniform(rng, 3.5, 4.0); da = py_uniform(rng, -0.05, 0.05);
        CK(d = Rect(A, q[0], q[1], 1.0, w, a + da)); gp(&p, d);
    }
    CK(planks = U(A, &p));
    CK(d = Rect(A, cx, cy, Ln, 4.1, a)); put_foot(foot, d);
    L_up(L, beams, 0.35, beam); L_up(L, planks, 0.7, kind);
    R_ = 0;
fail: gfree(&b); gfree(&p); ar_free(A); return R_;
}
BUILD(B_boardwalk) { BC *c = (BC *)c_; KOPT; return boardwalk(c->s[0], c->s[1], c->v[0], c->v[1], cx, cy, a, rng, k, foot, L); }

/* b_cart(body="wood", wheel="iron") */
BUILD(B_cart) {
    BEGIN; GL u = { 0 }; GEOSGeometry *box_, *wh, *d; double Ln, W; Frame ff;
    NOK;
    Ln = py_uniform(rng, 3.8, 4.6); W = py_uniform(rng, 2.4, 2.9); ff = lf_frame(cx, cy, a); CK(box_ = Rect(A, cx, cy, Ln, W, a));
    for (int su = -1; su <= 1; su += 2) for (int sv = -1; sv <= 1; sv += 2) { CK(d = RectF(A, &ff, su * (Ln / 2 - 0.9), sv * (W / 2 + 0.3), 1.3, 0.95, a)); gp(&u, d); }
    CK(wh = U(A, &u));
    CK(d = Rect(A, cx, cy, Ln, W + 1.6, a)); put_foot(foot, d);
    L_up(L, wh, 0.95, c->s[1]); L_up(L, box_, py_uniform(rng, 1.4, 1.8), c->s[0]);
    CK(d = Inset(A, box_, 0.5)); L_tcut(L, d, 1, 0.92);
    R_ = 0;
fail: gfree(&u); ar_free(A); if (R_ < 0) { geo_unref(*foot); *foot = NULL; } return R_;
}

/* b_chest(kind="crate", metal="iron") */
BUILD(B_chest) {
    BEGIN; GL u = { 0 }; GEOSGeometry *g, *bands, *d; double Ln, W; Frame ff;
    NOK;
    Ln = py_uniform(rng, 3.0, 3.6); W = py_uniform(rng, 2.0, 2.4); ff = lf_frame(cx, cy, a); CK(g = Rect(A, cx, cy, Ln, W, a));
    CK(d = BarF(A, &ff, -Ln / 4, -W, -Ln / 4, W, 0.3)); gp(&u, d);
    CK(d = BarF(A, &ff, Ln / 4, -W, Ln / 4, W, 0.3)); gp(&u, d);
    CK(d = Inset(A, g, 0.3)); CK(d = Ext(A, d)); CK(d = Bufr(A, d, 0.13)); gp(&u, d);
    CK(bands = U(A, &u));
    put_foot(foot, g);
    L_up(L, g, py_uniform(rng, 1.2, 1.5), c->s[0]); L_tcut(L, bands, 0, 0.25);
    CK(d = RectF(A, &ff, 0, W / 2 - 0.45, 0.8, 0.55, a)); L_on(L, d, 0, 0.25, c->s[1]);
    R_ = 0;
fail: gfree(&u); ar_free(A); if (R_ < 0) { geo_unref(*foot); *foot = NULL; } return R_;
}

/* b_tentacle(kind="tendril", L=(6.0, 9.0)): f(cx, cy, a, rng, k=1.0) */
static int tentacle(const char *kind, double L0, double L1, double cx, double cy, double a, PyRandom *rng, double k, Geo **foot, SoLayers *L) {
    Ar AR = { 0 }, *A = &AR; int R_ = SC_ERR, n = 14, ty; GL u = { 0 }, s = { 0 }; GEOSGeometry *g, *suck, *d; double Ln, pts[28], t, x, y, curl, w0; int64_t ch;
    *foot = NULL;
    Ln = py_uniform(rng, L0, L1) * k; t = a; x = cx - Ln / 2 * py_cos(a); y = cy - Ln / 2 * py_sin(a);
    CK(py_choice_e(rng, 2, &ch) == 0); curl = (ch ? 1 : -1) * py_uniform(rng, 0.09, 0.16);
    for (int i = 0; i < n; i++) {
        pts[2 * i] = x; pts[2 * i + 1] = y;
        t += (curl * (1 + 1.6 * P((double)i / n, 2.0)) + py_uniform(rng, -0.08, 0.08)) * (Ln > 11 ? 0.6 : 1.0);
        x += Ln / n * py_cos(t); y += Ln / n * py_sin(t);
    }
    w0 = 0.85 * mx1(P(k, 0.6));
    for (int i = 0; i < n - 1; i++) { CK(d = Cap(A, &pts[2 * i], &pts[2 * i + 2], 2 * (w0 - (w0 - 0.47) * i / (n - 1)))); gp(&u, d); }
    CK(g = U(A, &u));
    for (int i = 1; i < n - 3; i += 2) { CK(d = Disc(A, pts[2 * i], pts[2 * i + 1], 0.2, 8)); gp(&s, d); }
    CK(suck = U(A, &s));
    ty = shp_type(g); CK(ty >= 0);
    if (ty == GEOS_POLYGON) { CK(d = Hull(A, g)); put_foot(foot, d); }
    L_up(L, g, 0.6 * mx1(P(k, 0.4)), kind); L_tcut(L, suck, 0, 0.25);
    R_ = 0;
fail: gfree(&u); gfree(&s); ar_free(A); if (R_ < 0) { geo_unref(*foot); *foot = NULL; lyl_free(L); } return R_;
}
BUILD(B_tentacle) { BC *c = (BC *)c_; KOPT; return tentacle(c->s[0], c->v[0], c->v[1], cx, cy, a, rng, k, foot, L); }

/* b_cracked_slab(kind="concrete", R=(2.2, 2.9)) */
BUILD(B_cracked_slab) {
    BEGIN; GEOSGeometry *g, *d; double Rr;
    NOK;
    Rr = py_uniform(rng, c->v[0], c->v[1]);
    { double asp = py_uniform(rng, 0.7, 0.95); int nv = (int)py_randint(rng, 5, 8); CK(g = Stone(A, cx, cy, Rr, rng, asp, nv)); }
    put_foot(foot, g);
    L_up(L, g, py_uniform(rng, 0.4, 0.55), c->s[0]);
    { int n = (int)py_randint(rng, 4, 6); CK(d = TG(lf_radial(cx, cy, 0.15, Rr * 1.2, n, rng, 0.24))); }
    L_tcut(L, d, 0, 0.3);
    R_ = 0;
fail: ar_free(A); if (R_ < 0) { geo_unref(*foot); *foot = NULL; } return R_;
}

/* b_fairy_ring(kind="shroom", R=(2.4, 3.1), h=(0.6, 0.95), tiers=((0.5, 5), (0.5, 50))): f(cx, cy, a, rng, k=1.0) */
BUILD(B_fairy_ring) {
    BEGIN; GL ps = { 0 }; GEOSGeometry *d; double Rr; int n;
    KOPT;
    Rr = py_uniform(rng, c->v[0], c->v[1]) * k; n = (int)(py_randint(rng, 7, 10) * k);
    for (int i = 0; i < n; i++) {
        double dx, dy, r;
        if (!(py_random(rng) > 0.12)) continue;
        dx = cx + Rr * py_cos(a + 6.283 * i / n) + py_uniform(rng, -0.15, 0.15);
        dy = cy + Rr * py_sin(a + 6.283 * i / n) + py_uniform(rng, -0.15, 0.15);
        r = py_uniform(rng, 0.42, 0.6);
        CK(d = Disc(A, dx, dy, r, 12)); gp(&ps, d);
    }
    { double ri = Rr - 0.7; CK(d = Ann(A, cx, cy, Rr + 0.7, ri > 0.1 ? ri : 0.1, 28)); put_foot(foot, d); }
    for (int i = 0; i < ps.n; i++) L_mound(L, ps.v[i], py_uniform(rng, c->v[2], c->v[3]), c->s[0], &c->tiers);
    R_ = 0;
fail: gfree(&ps); ar_free(A); return R_;
}

/* b_sign(post="wood", board="board") */
BUILD(B_sign) {
    BEGIN; GEOSGeometry *arrow, *d, *e; double b, cc[2], xy[10]; Frame ff, f2;
    NOK;
    ff = lf_frame(cx, cy, a); b = a + py_uniform(rng, -0.5, 0.5); FF(&ff, 2.0, 0, cc); f2 = lf_frame(cc[0], cc[1], b);
    FF(&f2, -1.5, -0.6, &xy[0]); FF(&f2, 0.9, -0.6, &xy[2]); FF(&f2, 1.6, 0, &xy[4]); FF(&f2, 0.9, 0.6, &xy[6]); FF(&f2, -1.5, 0.6, &xy[8]);
    CK(arrow = Poly(A, xy, 5));
    CK(d = BarF(A, &ff, -3.2, 0, 2.0, 0, 0.92)); CK(e = U2(A, d, arrow)); CK(e = Hull(A, e)); put_foot(foot, e);
    CK(d = BarF(A, &ff, -3.2, 0, 1.2, 0, 0.92)); L_up(L, d, 0.4, c->s[0]); L_up(L, arrow, 0.55, c->s[1]);
    CK(d = BarF(A, &f2, -1.2, 0, 0.9, 0, 0.24)); L_tcut(L, d, 1, 0.2);
    R_ = 0;
fail: ar_free(A); if (R_ < 0) { geo_unref(*foot); *foot = NULL; } return R_;
}

/* b_gear(kind="iron", R=(1.6, 2.2)) */
BUILD(B_gear) {
    BEGIN; GL u = { 0 }; GEOSGeometry *g, *d; double Rr; int n;
    NOK;
    Rr = py_uniform(rng, c->v[0], c->v[1]); n = (int)py_randint(rng, 8, 11);
    CK(d = Disc(A, cx, cy, Rr - 0.4, 28)); gp(&u, d);
    for (int i = 0; i < n; i++) { double t = a + 6.283 * i / n; CK(d = Rect(A, cx + (Rr - 0.15) * py_cos(t), cy + (Rr - 0.15) * py_sin(t), 0.75, 0.62, t)); gp(&u, d); }
    CK(g = U(A, &u)); CK(d = Disc(A, cx, cy, 0.48, 14)); CK(g = Diff(A, g, d));
    CK(d = Disc(A, cx, cy, Rr + 0.25, 20)); put_foot(foot, d);
    L_up(L, g, 0.5, c->s[0]);
    CK(d = Ann(A, cx, cy, Rr - 0.75, Rr - 1.0, 24)); L_tcut(L, d, 0, 0.22);
    R_ = 0;
fail: gfree(&u); ar_free(A); if (R_ < 0) { geo_unref(*foot); *foot = NULL; } return R_;
}

/* b_brazier(stand="iron", fire="fire") */
BUILD(B_brazier) {
    BEGIN; GEOSGeometry *base, *d;
    NOK;
    CK(base = Hex_(A, cx, cy, py_uniform(rng, 1.95, 2.2), a));
    put_foot(foot, base);
    L_up(L, base, 0.55, c->s[0]);
    CK(d = Ann(A, cx, cy, 1.5, 0.5, 18)); L_on(L, d, 0, 0.75, c->s[0]);
    CK(d = Disc(A, cx, cy, 0.62, 10)); L_on(L, d, 0, 1.2, c->s[1]);
    R_ = 0;
fail: ar_free(A); if (R_ < 0) { geo_unref(*foot); *foot = NULL; } return R_;
}

/* b_tiles(kind="pavers", nx=3, ny=2, size=1.5); 1 = None */
BUILD(B_tiles) {
    BEGIN; GL u = { 0 }; GEOSGeometry *g, *d, *st; double Ln, W, sz = c->v[0], c2[2]; int nx = c->i[0], ny = c->i[1], none; Frame ff;
    NOK;
    Ln = nx * sz; W = ny * sz; ff = lf_frame(cx, cy, a); CK(g = Rect(A, cx, cy, Ln, W, a));
    FF(&ff, Ln / 2, W / 2, c2);
    { double r = sz * py_uniform(rng, 0.5, 0.9); CK(st = Stone(A, c2[0], c2[1], r, rng, 0.8, 5)); }
    CK(d = Diff(A, g, st)); g = Biggest(A, d, &none);
    if (!g) { R_ = none ? 1 : SC_ERR; goto fail; }
    for (int i = 1; i < nx; i++) { CK(d = BarF(A, &ff, -Ln / 2 + i * sz, -W, -Ln / 2 + i * sz, W, 0.28)); gp(&u, d); }
    for (int j = 1; j < ny; j++) { CK(d = BarF(A, &ff, -Ln, -W / 2 + j * sz, Ln, -W / 2 + j * sz, 0.28)); gp(&u, d); }
    CK(d = U(A, &u));
    put_foot(foot, g);
    L_up(L, g, py_uniform(rng, 0.35, 0.45), c->s[0]); L_tcut(L, d, 0, 0.28);
    R_ = 0;
fail: gfree(&u); ar_free(A); return R_;
}

/* b_pile(kind="log", L=(4.0, 5.2), w=1.0, top=True) */
BUILD(B_pile) {
    BEGIN; GL u = { 0 }, b = { 0 }, t = { 0 }; GEOSGeometry *low, *d; double Ln, w = c->v[2]; Frame ff;
    NOK;
    Ln = py_uniform(rng, c->v[0], c->v[1]); ff = lf_frame(cx, cy, a);
    for (int j = 0; j < 3; j++) {
        double v = j == 0 ? -w : j == 1 ? 0 : w, p[2], q[2], r1 = py_uniform(rng, -0.3, 0.3), r2;
        FF(&ff, -Ln / 2 + r1, v, p); r2 = py_uniform(rng, -0.3, 0.3); FF(&ff, Ln / 2 + r2, v, q);
        CK(d = Cap(A, p, q, w)); gp(&u, d);
    }
    CK(low = U(A, &u)); CK(low = Bufr(A, low, 0.06));
    put_foot(foot, low);
    L_up(L, low, w * 0.95, c->s[0]);
    for (int j = 0; j < 2; j++) { double v = j ? w / 2 : -w / 2; CK(d = BarF(A, &ff, -Ln, v, Ln, v, 0.22)); gp(&b, d); }
    CK(d = U(A, &b)); L_tcut(L, d, 0, 0.3);
    if (c->i[0]) {
        for (int j = 0; j < 2; j++) { double v = j ? w / 2 : -w / 2; CK(d = CapF(A, &ff, -Ln / 2 + 0.3, v, Ln / 2 - 0.3, v, w * 0.9)); gp(&t, d); }
        CK(d = U(A, &t)); CK(d = Bufr(A, d, 0.04)); L_on(L, d, 0, w * 0.85, c->s[0]);
    }
    R_ = 0;
fail: gfree(&u); gfree(&b); gfree(&t); ar_free(A); if (R_ < 0) { geo_unref(*foot); *foot = NULL; } return R_;
}

/* b_hoops(kind="barrel", L=(3.0, 3.8), w=2.0) */
BUILD(B_hoops) {
    BEGIN; GL u = { 0 }; GEOSGeometry *g, *d; double Ln, w = c->v[2], xy[16]; Frame ff;
    NOK;
    Ln = py_uniform(rng, c->v[0], c->v[1]); ff = lf_frame(cx, cy, a);
    FF(&ff, -Ln / 2, -w * 0.38, &xy[0]); FF(&ff, -Ln / 4, -w / 2, &xy[2]); FF(&ff, Ln / 4, -w / 2, &xy[4]); FF(&ff, Ln / 2, -w * 0.38, &xy[6]);
    FF(&ff, Ln / 2, w * 0.38, &xy[8]); FF(&ff, Ln / 4, w / 2, &xy[10]); FF(&ff, -Ln / 4, w / 2, &xy[12]); FF(&ff, -Ln / 2, w * 0.38, &xy[14]);
    CK(g = Poly(A, xy, 8));
    for (int j = 0; j < 3; j++) { double uu = j == 0 ? -Ln / 3 : j == 1 ? 0 : Ln / 3; CK(d = BarF(A, &ff, uu, -w, uu, w, 0.26)); gp(&u, d); }
    CK(d = U(A, &u));
    put_foot(foot, g);
    L_up(L, g, py_uniform(rng, 1.2, 1.5), c->s[0]); L_tcut(L, d, 0, 0.22);
    R_ = 0;
fail: gfree(&u); ar_free(A); if (R_ < 0) { geo_unref(*foot); *foot = NULL; } return R_;
}

/* ================================================================== landmarks: f(cx, cy, a, rng, k) */
/* L_wall(kind="ruin", cap=None); 1 = None */
typedef struct { Ar *A; PyRandom *rng; const char *kind, *cap; double W; GL foot; SoLayers *L; } WallRun;
static int wall_run(WallRun *w, double ox, double oy, double ang, double length) {
    Ar *A = w->A; Frame ff = lf_frame(ox, oy, ang); double u = 0.0; GEOSGeometry *q, *d;
    while (u < length - 1.5) {
        double bl = py_uniform(w->rng, 2.4, 4.2); int i;
        if (py_random(w->rng) < 0.16) { u += bl * 0.6; continue; }          /* a gap where the wall has fallen */
        bl = bl <= length - u ? bl : length - u;
        CK(q = RectF(A, &ff, u + bl / 2, 0, bl - 0.12, w->W, ang)); gp(&w->foot, q);
        L_up(w->L, q, py_uniform(w->rng, 1.0, 2.6), w->kind); i = w->L->n - 1;
        CK(d = BarF(A, &ff, u + bl / 2, -w->W, u + bl / 2, w->W, 0.24)); L_tcut(w->L, d, i, 0.22);
        if (w->cap && w->cap[0] && py_random(w->rng) < 0.35) { CK(d = RectF(A, &ff, u + bl / 2, 0, bl * 0.6, w->W * 0.7, ang)); L_on(w->L, d, i, 0.4, w->cap); }
        u += bl;
    }
    return 0;
fail: return SC_ERR;
}
BUILD(L_wall) {
    BEGIN; WallRun w; double Ln; GEOSGeometry *d;
    KREQ;
    memset(&w, 0, sizeof w); w.A = A; w.rng = rng; w.kind = c->s[0]; w.cap = c->s[1]; w.L = L;
    Ln = py_uniform(rng, 13, 18) * k; w.W = py_uniform(rng, 1.9, 2.4);
    CK(wall_run(&w, cx - Ln / 2 * py_cos(a), cy - Ln / 2 * py_sin(a), a, Ln) == 0);
    if (py_random(rng) < 0.55) {
        double ex = cx + Ln / 2 * py_cos(a), ey = cy + Ln / 2 * py_sin(a), ang, len; int64_t ch;
        CK(py_choice_e(rng, 2, &ch) == 0); ang = a + (ch ? 1 : -1) * PI / 2; len = Ln * py_uniform(rng, 0.4, 0.7);
        CK(wall_run(&w, ex, ey, ang, len) == 0);
    }
    if (w.foot.n < 3) { R_ = 1; goto fail; }
    CK(d = U(A, &w.foot)); CK(d = Bufr(A, d, 0.3)); put_foot(foot, d);
    R_ = 0;
fail: gfree(&w.foot); ar_free(A); if (R_ != 0) lyl_free(L); return R_;
}

/* L_track(rail="iron", tie="wood"): b_track(rail, tie, (20, 28))(cx, cy, a, rng, k * 0.75) */
BUILD(L_track) { BC *c = (BC *)c_; KREQ; return track(c, cx, cy, a, rng, k * 0.75, 20, 28, foot, L); }

/* L_dais(kind="tflag", top_piece="brazier", fire="fire", metal="iron") */
BUILD(L_dais) {
    BEGIN; GEOSGeometry *base, *d; double R, xy[32], kk; int n; int64_t ch; Frame ff; const char *kind = c->s[0], *tp = c->s[1];
    KREQ;
    R = py_uniform(rng, 5.5, 7.0) * k; CK(py_choice_e(rng, 2, &ch) == 0); n = ch ? 8 : 6;
    for (int i = 0; i < n; i++) { xy[2 * i] = cx + R * py_cos(a + 6.283 * i / n); xy[2 * i + 1] = cy + R * py_sin(a + 6.283 * i / n); }
    CK(base = Poly(A, xy, n));
    ff = lf_frame(cx, cy, a + PI / n);
    kk = mx1(P(k, 0.5));
    L_up(L, base, 0.6, kind);
    CK(d = Inset(A, base, 1.1)); L_on(L, d, 0, 0.55, kind);
    CK(d = Inset(A, base, 0.45)); CK(d = Ext(A, d)); CK(d = Bufr(A, d, 0.14)); L_tcut(L, d, 0, 0.22);
    if (!strcmp(tp, "column")) {
        CK(d = Disc(A, cx, cy, 1.5 * kk, 24)); L_on(L, d, 1, 1.9, kind);
        CK(d = Ann(A, cx, cy, R * 0.55, R * 0.55 - 0.3, 32)); L_tcut(L, d, 1, 0.25);
    } else if (!strcmp(tp, "altar")) {
        CK(d = Rect(A, cx, cy, 4.2 * kk, 2.2 * kk, a)); L_on(L, d, 1, 1.3, kind);
        CK(d = Ann(A, cx, cy, R * 0.6, R * 0.6 - 0.3, 32)); L_tcut(L, d, 1, 0.25);
    } else {
        CK(d = Ann(A, cx, cy, 1.9 * kk, 1.1 * kk, 20)); L_on(L, d, 1, 0.9, c->s[3]);
        CK(d = T(shp_star(cx, cy, 0.95 * kk, 0.5 * kk, 6, a))); L_on(L, d, 1, 1.5, c->s[2]);
    }
    CK(d = RectF(A, &ff, R * py_cos(PI / n) + 0.9, 0, 2.0, 4.2, a + PI / n)); L_up(L, d, 0.35, kind);
    CK(d = Bufr(A, base, 1.6)); put_foot(foot, d);
    R_ = 0;
fail: ar_free(A); if (R_ < 0) lyl_free(L); return R_;
}

/* L_river(kind="water", bank="rock", bridge="board"); 1 = None */
BUILD(L_river) {
    BEGIN; GEOSGeometry *ch, *d, *q, *disc; double R, off, amp, w, ph, kk, *us = NULL, *pts = NULL; int np_, none, nst; Frame ff;
    KREQ;
    R = SC.R0 - 1.2; ff = lf_frame(0, 0, a);
    off = py_uniform(rng, -0.3, 0.3) * R; amp = py_uniform(rng, 1.5, 3.5) * k; w = py_uniform(rng, 1.6, 2.4) * mx1(P(k, 0.5));
    ph = py_uniform(rng, 0, 6.283); kk = py_uniform(rng, 0.12, 0.22) / mx1(P(k, 0.5));
    np_ = (int)(40 * k) + 20;
    us = (double *)malloc(sizeof(double) * (size_t)(np_ > 0 ? np_ : 1)); pts = (double *)malloc(sizeof(double) * 2 * (size_t)(np_ > 0 ? np_ : 1));
    CK(lf_linspace(-R - 2, R + 2, np_, us) == np_);
    for (int i = 0; i < np_; i++) FF(&ff, us[i], off + amp * py_sin(kk * us[i] + ph), &pts[2 * i]);
    CK(ch = T(shp_line_buffer(pts, np_, w, SHP_CAP_FLAT, SHP_JOIN_ROUND, 16, 5.0)));
    CK(d = T(shp_point_buffer(0, 0, R - 0.6, 64))); CK(ch = Inter(A, ch, d));
    ch = Biggest(A, ch, &none);
    if (!ch) { R_ = none ? 1 : SC_ERR; goto fail; }
    L_gcut(L, ch, py_uniform(rng, 0.5, 0.7), c->s[0]);
    CK(disc = T(shp_point_buffer(0, 0, R - 1.0, 16)));
    nst = (int)(10 * k * k);
    for (int i = 0; i < nst; i++) {
        int64_t j, cs; double px, py, t, dd, r; int nv, in;
        CK(py_randrange1_e(rng, np_ - 1, &j) == 0); px = pts[2 * j]; py = pts[2 * j + 1];
        CK(py_choice_e(rng, 2, &cs) == 0);
        t = py_atan2(pts[2 * j + 3] - py, pts[2 * j + 2] - px) + (cs ? 1 : -1) * PI / 2; dd = w + py_uniform(rng, 0.9, 1.6);
        r = py_uniform(rng, 0.6, 1.0); nv = (int)py_randint(rng, 5, 7);
        CK(q = Stone(A, px + dd * py_cos(t), py + dd * py_sin(t), r, rng, 0.85, nv));
        if (shp_distance(q, ch) > 0.35) { in = shp_contains(disc, q); CK(in >= 0); if (in) L_mound(L, q, py_uniform(rng, 0.45, 0.7), c->s[1], &FT_PEB_T); }
    }
    CK(d = Bufr(A, ch, 0.2)); put_foot(foot, d);
    R_ = 0;
fail: free(us); free(pts); ar_free(A); if (R_ != 0) lyl_free(L); return R_;
}

/* L_crater(rim="rock", pit="pit"): b_crater(rim, pit, (5.0, 6.5))(cx, cy, a, rng, k) */
BUILD(L_crater) { BC *c = (BC *)c_; KREQ; return crater(c->s[0], c->s[1], 5.0, 6.5, cx, cy, a, rng, k, foot, L); }

/* L_colonnade(kind="tcolumn", floor="tflag") */
BUILD(L_colonnade) {
    BEGIN; GL j = { 0 }, ft = { 0 }; GEOSGeometry *way, *d, *pl; double gapu = 4.6, Ln, half; int n, nj; Frame ff;
    static const double H5[5] = { 0.5, 0.9, 1.6, 2.4, 2.7 };
    KREQ;
    n = (int)(3 + 2 * k); Ln = n * gapu; ff = lf_frame(cx, cy, a); half = 3.4 * mx1(P(k, 0.4));
    CK(way = Rect(A, cx, cy, Ln + 1.0, 2 * half - 2.6, a)); L_up(L, way, 0.35, c->s[1]);
    nj = (int)(Ln / 2.3);
    for (int i = 1; i < nj + 1; i++) { CK(d = BarF(A, &ff, -Ln / 2 + i * 2.3, -half, -Ln / 2 + i * 2.3, half, 0.26)); gp(&j, d); }
    CK(d = BarF(A, &ff, -Ln, 0, Ln, 0, 0.26)); gp(&j, d);
    CK(d = U(A, &j)); L_tcut(L, d, 0, 0.25); gp(&ft, way);
    for (int i = 0; i < n; i++) for (int sv = -1; sv <= 1; sv += 2) {
        double cc[2]; int jj; int64_t ch;
        FF(&ff, -Ln / 2 + (i + 0.5) * gapu, sv * half, cc); CK(pl = Rect(A, cc[0], cc[1], 2.3, 2.3, a)); gp(&ft, pl);
        L_up(L, pl, 0.5, c->s[0]); jj = L->n - 1;
        CK(d = Disc(A, cc[0], cc[1], 0.92, 18)); CK(py_choice_e(rng, 5, &ch) == 0); L_on(L, d, jj, H5[ch], c->s[0]);
    }
    CK(d = U(A, &ft)); CK(d = Hull(A, d)); put_foot(foot, d);
    R_ = 0;
fail: gfree(&j); gfree(&ft); ar_free(A); if (R_ < 0) lyl_free(L); return R_;
}

/* L_hull(kind="hull", rib="hull") */
BUILD(L_hull) {
    BEGIN; GL rb = { 0 }; GEOSGeometry *outline, *wale, *keel, *ribs, *gone, *d; double Ln, Wd, us[21], xy[84], au[4096]; int n; int64_t ch; Frame ff;
    KREQ;
    Ln = py_uniform(rng, 15, 19) * k; Wd = py_uniform(rng, 4.2, 5.2) * mx1(P(k, 0.7)); ff = lf_frame(cx, cy, a);
    CK(lf_linspace(-Ln / 2 * 0.98, Ln / 2 * 0.98, 21, us) == 21);
    for (int i = 0; i < 21; i++) FF(&ff, us[i], 1 * (Wd - 0.0) * P(1 - P(2 * us[i] / Ln, 2.0), 0.6), &xy[2 * i]);              /* edge(1) */
    for (int i = 0; i < 21; i++) FF(&ff, us[20 - i], -1 * (Wd - 0.0) * P(1 - P(2 * us[20 - i] / Ln, 2.0), 0.6), &xy[42 + 2 * i]);   /* edge(-1)[::-1] */
    CK(outline = Poly(A, xy, 42)); CK(outline = Bufr(A, outline, 0));
    CK(d = Inset(A, outline, 1.0)); CK(wale = Diff(A, outline, d));
    CK(keel = BarF(A, &ff, -Ln / 2 + 0.5, 0, Ln / 2 - 0.5, 0, 1.0));
    n = lf_arange(-Ln / 2 + 2.0, Ln / 2 - 1.5, 2.4 * mx1(P(k, 0.4)), au, 4096); CK(n >= 0);
    for (int i = 0; i < n; i++) { CK(d = BarF(A, &ff, au[i], -Wd, au[i], Wd, 0.92)); gp(&rb, d); }
    CK(ribs = U(A, &rb)); CK(ribs = Inter(A, ribs, outline));
    { double uu, len; int side; CK(py_choice_e(rng, 2, &ch) == 0); side = ch ? 1 : -1;
      uu = py_uniform(rng, -Ln / 4, Ln / 4); len = Ln * py_uniform(rng, 0.3, 0.5);
      CK(gone = RectF(A, &ff, uu, side * Wd, len, Wd * 1.3, a)); }
    CK(wale = Diff(A, wale, gone)); CK(d = Bufr(A, gone, -0.3)); CK(ribs = Diff(A, ribs, d));
    L_up(L, ribs, 0.55, c->s[1]); L_up(L, keel, 0.8, c->s[0]); L_up(L, wale, py_uniform(rng, 1.3, 1.9), c->s[0]);
    CK(d = Bufr(A, outline, 0.3)); put_foot(foot, d);
    R_ = 0;
fail: gfree(&rb); ar_free(A); if (R_ < 0) lyl_free(L); return R_;
}

/* L_vehicle(kind="iron", track="iron"): the wreck of a tank or a truck, all metal, every part standing on the ground or on
   the part under it (no overhangs). A tank: two track runs with the links cut across them, the hull over them with a
   lower glacis plate at the front, the engine grille cut into the rear deck, a shell hole; the turret either still on
   (the gun laid over the deck) or blown off and lying beside the hull with its ring left open; one track broken with
   loose links beside it. A truck: tyres on two or three axles, the bonnet and the tall cab at the front, the flatbed
   with its rails (one side torn off), planks cut in its floor and a crate still on it. */
BUILD(L_vehicle) {
    BEGIN; GL ft = { 0 }, lk = { 0 }; GEOSGeometry *d, *e, *g, *body; double Ln, W; Frame ff; int jb, jt;
    KREQ;
    ff = lf_frame(cx, cy, a);
    if (py_random(rng) < 0.6) {          /* ---- a tank */
        double tw, ht, hb, ht2, Lb, Wb, bu, gap0, rt, tu; int bs = py_random(rng) < 0.5 ? -1 : 1, on;
        Ln = py_uniform(rng, 15, 18.5) * k; W = py_uniform(rng, 10.0, 11.5) * mx1(P(k, 0.7));
        tw = W * 0.24; ht = py_uniform(rng, 1.7, 2.1); hb = ht + py_uniform(rng, 0.9, 1.3); ht2 = py_uniform(rng, 1.2, 1.6);
        Lb = Ln * 0.9; Wb = W - 2 * tw * 0.55;
        gap0 = py_uniform(rng, -Ln / 4, Ln / 4);
        for (int s = -1; s <= 1; s += 2) {          /* the track runs */
            GEOSGeometry *tr; int jj;
            CK(tr = RectF(A, &ff, 0, s * (W / 2 - tw / 2), Ln, tw, a)); CK(tr = Bufr(A, tr, -0.6)); CK(tr = Bufr(A, tr, 0.6));
            if (s == bs) { CK(d = RectF(A, &ff, gap0, s * (W / 2 - tw / 2), 2.2, tw * 1.4, a)); CK(tr = Diff(A, tr, d)); }          /* broken */
            L_up(L, tr, ht, c->s[1]); jj = L->n - 1; gp(&ft, tr);
            for (double u = -Ln / 2 + 0.9; u < Ln / 2 - 0.6; u += 1.15) { CK(d = BarF(A, &ff, u, s * (W / 2 - tw - 0.2), u, s * (W / 2 + 0.2), 0.35)); gp(&lk, d); }
            CK(d = U(A, &lk)); CK(d = Inter(A, d, tr)); L_tcut(L, d, jj, 0.3); lk.n = 0;
        }
        for (int i = 0; i < 3; i++) {          /* loose links by the break */
            CK(d = RectF(A, &ff, gap0 + py_uniform(rng, -2.5, 2.5), bs * (W / 2 + py_uniform(rng, 0.9, 2.0)), 0.9, tw * 0.8, a + py_uniform(rng, -0.8, 0.8)));
            L_up(L, d, 0.45, c->s[1]); gp(&ft, d);
        }
        {   /* the hull, the glacis plate lower at the front */
            double xy[12], pu[6] = { -Lb / 2, Lb / 2 - 3.0, Lb / 2 - 3.0, -Lb / 2, 0, 0 }; (void)pu;
            FF(&ff, -Lb / 2, -Wb / 2, &xy[0]); FF(&ff, Lb / 2 - 2.6, -Wb / 2, &xy[2]); FF(&ff, Lb / 2 - 2.6, Wb / 2, &xy[4]); FF(&ff, -Lb / 2, Wb / 2, &xy[6]);
            CK(body = Poly(A, xy, 4)); L_up(L, body, hb, c->s[0]); jb = L->n - 1; gp(&ft, body);
            FF(&ff, Lb / 2 - 2.6, -Wb / 2, &xy[0]); FF(&ff, Lb / 2, -Wb / 2 * 0.78, &xy[2]); FF(&ff, Lb / 2, Wb / 2 * 0.78, &xy[4]); FF(&ff, Lb / 2 - 2.6, Wb / 2, &xy[6]);
            CK(d = Poly(A, xy, 4)); L_up(L, d, hb - 0.7, c->s[0]); gp(&ft, d);
        }
        for (double u = -Lb / 2 + 1.0; u < -Lb / 2 + 4.2; u += 0.8) { CK(d = BarF(A, &ff, u, -Wb / 2 + 1.0, u, Wb / 2 - 1.0, 0.35)); gp(&lk, d); }          /* engine grille */
        CK(d = U(A, &lk)); L_tcut(L, d, jb, 0.3); lk.n = 0;
        rt = Wb * 0.36; tu = Lb * 0.06; on = py_random(rng) < 0.65;
        if (on) {          /* the turret still on, the gun laid over the deck */
            double p[2], q[2], b = py_random(rng) < 0.4 ? 0.0 : py_uniform(rng, -0.9, 0.9), du = py_cos(b), dv = py_sin(b);
            FF(&ff, tu, 0, p); CK(g = Disc(A, p[0], p[1], rt, 10)); L_on(L, g, jb, ht2, c->s[0]); jt = L->n - 1;
            FF(&ff, tu - rt * 0.35, rt * 0.35, q); CK(d = Disc(A, q[0], q[1], 0.75, 12)); L_on(L, d, jt, 0.3, c->s[0]);          /* the hatch */
            CK(d = BarF(A, &ff, tu, 0, tu + du * Lb, dv * Lb, 0.9)); CK(e = Inset(A, body, 0.4)); CK(d = Inter(A, d, e)); L_on(L, d, jb, ht2 * 0.65, c->s[0]);
            CK(d = RectF(A, &ff, tu + du * rt, dv * rt, 1.4, 2.2, a + b)); CK(d = Inter(A, d, e)); L_on(L, d, jb, ht2 * 0.8, c->s[0]);          /* the mantlet */
        } else {           /* blown off: the ring left open, the turret upside down beside the hull */
            double p[2], q[2], side = py_random(rng) < 0.5 ? -1 : 1, u2 = py_uniform(rng, -Ln / 4, Ln / 4), b2 = py_uniform(rng, 0, 6.283);
            FF(&ff, tu, 0, p); CK(d = Ann(A, p[0], p[1], rt, rt - 0.55, 24)); L_tcut(L, d, jb, 0.5);
            FF(&ff, u2, side * (W / 2 + rt + 0.9), q); CK(g = Disc(A, q[0], q[1], rt, 10)); L_up(L, g, ht2 + 0.2, c->s[0]); gp(&ft, g);
            { double r2[2] = { q[0] + Ln * 0.45 * py_cos(b2), q[1] + Ln * 0.45 * py_sin(b2) }; CK(d = Bar(A, q, r2, 0.9)); L_up(L, d, 0.9, c->s[0]); gp(&ft, d); }
        }
        { double p[2]; FF(&ff, py_uniform(rng, Lb * 0.15, Lb * 0.3), py_uniform(rng, -Wb / 4, Wb / 4), p); CK(d = Disc(A, p[0], p[1], 0.9, 12)); if (!on) L_tcut(L, d, jb, 0.6); else { CK(e = Disc(A, p[0], p[1], 0.9, 12)); (void)e; } }          /* a shell hole */
        (void)bu;
    } else {               /* ---- a truck */
        double lc, lb, hc, hbd, rail, ub0, ub1; int nax = py_random(rng) < 0.5 ? 2 : 3, jbd, miss;
        Ln = py_uniform(rng, 15, 18) * k; W = py_uniform(rng, 6.6, 7.6) * mx1(P(k, 0.7));
        lc = Ln * 0.2; lb = Ln * 0.14; hc = py_uniform(rng, 3.6, 4.2); hbd = py_uniform(rng, 1.8, 2.2); rail = py_uniform(rng, 0.8, 1.1);
        miss = (int)(py_random(rng) * (2 * nax + 2));          /* one tyre may be gone */
        for (int ax = 0, t = 0; ax < nax; ax++) {
            double u = ax == 0 ? Ln / 2 - lb * 0.9 - 1.2 : -Ln / 2 + 2.2 + (ax - 1) * 3.0;
            for (int s = -1; s <= 1; s += 2, t++) {
                GEOSGeometry *ty; int jj;
                if (t == miss) continue;
                CK(ty = RectF(A, &ff, u, s * (W / 2 - 0.55), 2.8, 1.4, a)); CK(ty = Bufr(A, ty, -0.4)); CK(ty = Bufr(A, ty, 0.4));
                L_up(L, ty, 2.4, c->s[1]); jj = L->n - 1; gp(&ft, ty);
                for (double w = -1.1; w <= 1.1; w += 0.55) { CK(d = BarF(A, &ff, u + w, s * (W / 2 - 1.4), u + w, s * (W / 2 + 0.3), 0.25)); gp(&lk, d); }
                CK(d = U(A, &lk)); CK(d = Inter(A, d, ty)); L_tcut(L, d, jj, 0.2); lk.n = 0;
            }
        }
        CK(d = RectF(A, &ff, Ln / 2 - lb / 2, 0, lb, W - 1.8, a)); L_up(L, d, 2.6, c->s[0]); gp(&ft, d);          /* the bonnet */
        CK(g = RectF(A, &ff, Ln / 2 - lb - lc / 2, 0, lc, W - 0.9, a)); L_up(L, g, hc, c->s[0]); jt = L->n - 1; gp(&ft, g);          /* the cab */
        CK(d = RectF(A, &ff, Ln / 2 - lb - lc / 2, 0, lc * 0.45, W * 0.35, a)); L_tcut(L, d, jt, 0.25);          /* a roof hatch */
        ub0 = -Ln / 2; ub1 = Ln / 2 - lb - lc - 0.3;
        CK(body = RectF(A, &ff, (ub0 + ub1) / 2, 0, ub1 - ub0, W - 0.4, a)); L_up(L, body, hbd, c->s[0]); jbd = L->n - 1; gp(&ft, body);
        {   GEOSGeometry *in, *rl; double side = py_random(rng) < 0.5 ? -1 : 1, gu = py_uniform(rng, ub0 + 2, ub1 - 2);
            CK(in = Inset(A, body, 0.55)); CK(rl = Diff(A, body, in));
            CK(d = RectF(A, &ff, gu, side * (W / 2), py_uniform(rng, 3.0, 5.0), 1.6, a)); CK(rl = Diff(A, rl, d));          /* torn off */
            L_on(L, rl, jbd, rail, c->s[0]);
            for (double v = -W / 2 + 1.3; v < W / 2 - 0.9; v += 1.0) { CK(d = BarF(A, &ff, ub0, v, ub1, v, 0.2)); gp(&lk, d); }
            CK(d = U(A, &lk)); CK(e = Inset(A, body, 0.8)); CK(d = Inter(A, d, e)); L_tcut(L, d, jbd, 0.15); lk.n = 0;
            CK(d = RectF(A, &ff, py_uniform(rng, ub0 + 2.2, ub1 - 2.2), py_uniform(rng, -0.8, 0.8), 2.4, 2.4, a + py_uniform(rng, -0.3, 0.3))); CK(d = Inter(A, d, in));
            L_on(L, d, jbd, py_uniform(rng, 1.4, 1.9), c->s[0]);          /* a crate still on the bed */
        }
    }
    CK(d = U(A, &ft)); CK(d = Hull(A, d)); CK(d = Bufr(A, d, 0.3)); put_foot(foot, d);
    R_ = 0;
fail: gfree(&ft); gfree(&lk); ar_free(A); if (R_ < 0) lyl_free(L); return R_;
}

/* L_tree(kind="stump", root="root"): b_roots(kind, root, (2.6, 3.4))(cx, cy, a, rng, k) */
BUILD(L_tree) { BC *c = (BC *)c_; KREQ; return roots(c->s[0], c->s[1], 2.6, 3.4, cx, cy, a, rng, k, foot, L); }

/* L_circle(kind="glyph", post="tcolumn") */
BUILD(L_circle) {
    BEGIN; Geo *ft = NULL; GEOSGeometry *q, *d, *cen; double R, b[4]; int n, r; int64_t ch;
    KREQ;
    r = rune_circle(c->s[0], 6.0, 7.5, cx, cy, a, rng, k, &ft, L);
    if (r != 0) { geo_unref(ft); R_ = r; goto fail; }
    shp_bounds(ft->g, b); CK(cen = T(shp_centroid(ft->g))); R = b[2] - shp_x(cen); geo_unref(ft); ft = NULL;
    CK(py_choice_e(rng, 4, &ch) == 0); n = (int)(5 + ch);
    for (int i = 0; i < n; i++) {
        double t = a + 6.283 * i / n + 0.3, rr = py_uniform(rng, 0.85, 1.15); int nv = (int)py_randint(rng, 4, 6);
        CK(q = Stone(A, cx + (R + 1.3) * py_cos(t), cy + (R + 1.3) * py_sin(t), rr, rng, 0.8, nv));
        L_up(L, q, py_uniform(rng, 1.4, 2.6), c->s[1]);
    }
    CK(d = Disc(A, cx, cy, R + 2.5, 32)); put_foot(foot, d);
    R_ = 0;
fail: geo_unref(ft); ar_free(A); if (R_ < 0) lyl_free(L); return R_;
}

/* L_ruin(kind="ruin", floor="pavers") */
BUILD(L_ruin) {
    BEGIN; GL j = { 0 }; GEOSGeometry *fl, *d, *e; double Ln, W, t = 1.7, js, ar_[4096]; int n; int64_t ch, sides[4]; Frame ff;
    KREQ;
    Ln = py_uniform(rng, 11, 14) * k; W = py_uniform(rng, 8, 10.5) * k; ff = lf_frame(cx, cy, a); CK(fl = Rect(A, cx, cy, Ln, W, a));
    L_up(L, fl, 0.35, c->s[1]);
    js = 2.2 * mx1(P(k, 0.3));
    n = lf_arange(-Ln / 2 + js, Ln / 2 - 0.5, js, ar_, 4096); CK(n >= 0);
    for (int i = 0; i < n; i++) { CK(d = BarF(A, &ff, ar_[i], -W, ar_[i], W, 0.26)); gp(&j, d); }
    n = lf_arange(-W / 2 + js, W / 2 - 0.5, js, ar_, 4096); CK(n >= 0);
    for (int i = 0; i < n; i++) { CK(d = BarF(A, &ff, -Ln, ar_[i], Ln, ar_[i], 0.26)); gp(&j, d); }
    CK(d = U(A, &j)); CK(e = Inset(A, fl, t + 0.3)); CK(d = Inter(A, d, e)); L_tcut(L, d, 0, 0.25);
    CK(py_choice_e(rng, 2, &ch) == 0); CK(py_sample_e(rng, 4, 2 + ch, sides) == 0);
    for (int s = 0; s < 2 + ch; s++) {
        int64_t sd = sides[s]; double p0[2], p1[2], length, ang, u = 0.0, door; Frame f2;
        if (sd == 0 || sd == 1) { double v = (sd == 0 ? -1 : 1) * (W / 2 - t / 2); FF(&ff, -Ln / 2 + t / 2, v, p0); FF(&ff, Ln / 2 - t / 2, v, p1); }
        else { double uu = (sd == 2 ? -1 : 1) * (Ln / 2 - t / 2); FF(&ff, uu, -W / 2 + t / 2, p0); FF(&ff, uu, W / 2 - t / 2, p1); }
        length = py_hypot(p0[0] - p1[0], p0[1] - p1[1]); ang = py_atan2(p1[1] - p0[1], p1[0] - p0[0]); f2 = lf_frame(p0[0], p0[1], ang);
        door = py_uniform(rng, 0.3, 0.7) * length;
        while (u < length - 1.2) {
            double bl = py_uniform(rng, 2.2, 3.8); bl = bl <= length - u ? bl : length - u;
            if (fabs(u + bl / 2 - door) < 1.6) { u += bl; continue; }          /* the doorway */
            CK(d = RectF(A, &f2, u + bl / 2, 0, bl - 0.1, t - 0.35, ang)); L_on(L, d, 0, py_uniform(rng, 0.7, 2.6), c->s[0]); u += bl;
        }
    }
    put_foot(foot, fl);
    R_ = 0;
fail: gfree(&j); ar_free(A); if (R_ < 0) { geo_unref(*foot); *foot = NULL; lyl_free(L); } return R_;
}

/* L_pool / L_boardwalk / L_nest / L_spires: the b_* builder at landmark size */
BUILD(L_pool) { BC *c = (BC *)c_; KREQ; return stepping_pool(c->s[0], c->s[1], cx, cy, a, rng, 1.9 * k, foot, L); }
BUILD(L_boardwalk) { BC *c = (BC *)c_; KREQ; return boardwalk(c->s[0], c->s[1], 15, 20, cx, cy, a, rng, k * 0.85, foot, L); }
BUILD(L_nest) { BC *c = (BC *)c_; KREQ; return nest(c->s[0], c->s[1], 4, 6, cx, cy, a, rng, 2.2 * mx1(P(k, 0.7)), foot, L); }
BUILD(L_spires) { BC *c = (BC *)c_; KREQ; return crystals(c->s[0], 3.2, cx, cy, a, rng, 2.0 * mx1(P(k, 0.6)), foot, L); }

/* L_tentacles(kind="tendril", pit="pit") */
BUILD(L_tentacles) {
    BEGIN; GL ft = { 0 }; GEOSGeometry *hole, *d; double R; int n;
    KREQ;
    R = py_uniform(rng, 2.6, 3.4) * mx1(P(k, 0.6)); CK(hole = Stone(A, cx, cy, R, rng, 0.9, 10));
    L_gcut(L, hole, 0.8, c->s[1]); CK(d = Bufr(A, hole, 0.5)); gp(&ft, d);
    n = (int)py_randint(rng, 3, 5);
    for (int i = 0; i < n; i++) {
        double t = a + 6.283 * i / 4 + py_uniform(rng, -0.3, 0.3), Ln = py_uniform(rng, 7, 10) * k, ox, oy; Geo *gf = NULL; SoLayers gl = { 0 }; int r, n0;
        ox = cx + (R + 0.5 + Ln / 2) * py_cos(t); oy = cy + (R + 0.5 + Ln / 2) * py_sin(t);
        r = tentacle(c->s[0], Ln, Ln, ox, oy, t, rng, 1.0, &gf, &gl);
        if (r < 0) { geo_unref(gf); lyl_free(&gl); goto fail; }
        if (gf) {
            n0 = L->n; gp(&ft, T(shp_clone(gf->g)));
            for (int m = 0; m < gl.n; m++) {
                SoLayer *y = gl.v[m];
                if (y->v[0].t == LV_STR && !strcmp(y->v[0].s, "tcut")) {          /* (l[0], l[1], l[2] + n0, l[3]) */
                    SoLayer *z = ly_new(4); z->v[0] = lv_str("tcut"); z->v[1] = y->v[1].t == LV_GEOM ? gv(y->v[1].g) : lv_none(); z->v[2] = lv_int(y->v[2].i + n0); z->v[3] = y->v[3];
                    lyl_push_own(L, z);
                } else lyl_push(L, y);
            }
        }
        geo_unref(gf); lyl_free(&gl);
    }
    CK(d = U(A, &ft)); CK(d = Hull(A, d)); put_foot(foot, d);
    R_ = 0;
fail: gfree(&ft); ar_free(A); if (R_ < 0) lyl_free(L); return R_;
}

/* ================================================================== the factories */
/* the argument at position pos or keyword kw (NULL = not given) */
static const FtArg *arg(const FtCall *c, int pos, const char *kw) {
    int np = 0;
    for (int i = 0; i < c->nargs; i++) {
        const FtArg *x = &c->a[i];
        if (!x->kw) { if (np == pos) return x; np++; }
        else if (kw && !strcmp(x->kw, kw)) return x;
    }
    return NULL;
}
static const char *as_s(const FtCall *c, int pos, const char *kw, const char *def) { const FtArg *x = arg(c, pos, kw); return x ? (x->t == 's' ? so_intern(x->s) : NULL) : def; }
static double as_f(const FtCall *c, int pos, const char *kw, double def) { const FtArg *x = arg(c, pos, kw); return x ? x->v[0] : def; }
static int as_b(const FtCall *c, int pos, const char *kw, int def) { const FtArg *x = arg(c, pos, kw); return x ? (x->t == 's' ? x->s[0] != 0 : x->v[0] != 0) : def; }
static void as_2(const FtCall *c, int pos, const char *kw, double d0, double d1, double *o) { const FtArg *x = arg(c, pos, kw); o[0] = x ? x->v[0] : d0; o[1] = x ? x->v[1] : d1; }
static BC *bc_new(void) { BC *b = (BC *)calloc(1, sizeof *b); bld_keep(b, free); return b; }
static int bad_call(const FtCall *c, int npos) {          /* more positional arguments than the Python function takes: TypeError */
    int np = 0; for (int i = 0; i < c->nargs; i++) if (!c->a[i].kw) np++;
    return np > npos;
}
#define FAC(nm) static int nm(const FtCall *call, FtBuildFn *fn, void **ctx)
#define MK(fnp, npos) BC *b; if (bad_call(call, npos)) return SC_ERR; b = bc_new(); *fn = fnp; *ctx = b

FAC(F_wheel) { MK(B_wheel, 2); b->s[0] = as_s(call, 0, "kind", "wood"); b->s[1] = as_s(call, 1, "iron", "iron"); return 0; }
FAC(F_ladder) { MK(B_ladder, 1); b->s[0] = as_s(call, 0, "kind", "wood"); return 0; }
FAC(F_track) { MK(B_track, 3); b->s[0] = as_s(call, 0, "rail", "iron"); b->s[1] = as_s(call, 1, "tie", "wood"); as_2(call, 2, "L", 8.0, 11.5, b->v); return 0; }
FAC(F_campfire) { MK(B_campfire, 2); b->s[0] = as_s(call, 0, "stone_kind", "rock"); b->s[1] = as_s(call, 1, "log", "wood"); return 0; }
FAC(F_stairs) { MK(B_stairs, 2); b->s[0] = as_s(call, 0, "kind", "tflag"); b->i[0] = (int)as_f(call, 1, "steps", 3); return 0; }
FAC(F_well) { MK(B_well, 2); b->s[0] = as_s(call, 0, "wall", "rock"); b->s[1] = as_s(call, 1, "water", "water"); return 0; }
FAC(F_fallen_column) { MK(B_fallen_column, 2); b->s[0] = as_s(call, 0, "kind", "tcolumn"); b->v[0] = as_f(call, 1, "w", 1.8); return 0; }
FAC(F_grate) { MK(B_grate, 2); b->s[0] = as_s(call, 0, "kind", "iron"); b->i[0] = as_b(call, 1, "round_", 0); return 0; }
FAC(F_rune_circle) { MK(B_rune_circle, 2); b->s[0] = as_s(call, 0, "kind", "glyph"); as_2(call, 1, "R", 3.0, 4.0, b->v); return 0; }
FAC(F_crystals) { MK(B_crystals, 2); b->s[0] = as_s(call, 0, "kind", "crystal"); b->v[0] = as_f(call, 1, "hmax", 3.0); return 0; }
FAC(F_roots) { MK(B_roots, 3); b->s[0] = as_s(call, 0, "kind", "stump"); b->s[1] = as_s(call, 1, "root", "root"); as_2(call, 2, "R", 1.3, 1.7, b->v); return 0; }
FAC(F_stepping_pool) { MK(B_stepping_pool, 2); b->s[0] = as_s(call, 0, "stone_kind", "rock"); b->s[1] = as_s(call, 1, "water", "water"); return 0; }
FAC(F_tool) { MK(B_tool, 3); b->s[0] = as_s(call, 0, "which", "pick"); b->s[1] = as_s(call, 1, "metal", "iron"); b->s[2] = as_s(call, 2, "wood", "wood"); return 0; }
FAC(F_boss) { MK(B_boss, 4); b->s[0] = as_s(call, 0, "kind", "iron"); b->s[1] = as_s(call, 1, "boss", "iron"); as_2(call, 2, "R", 1.6, 2.1, b->v); b->i[0] = as_b(call, 3, "cross", 0); return 0; }
FAC(F_nest) { double n[2]; MK(B_nest, 3); b->s[0] = as_s(call, 0, "rim", "twig"); b->s[1] = as_s(call, 1, "egg", "egg"); as_2(call, 2, "n", 2, 4, n); b->i[0] = (int)n[0]; b->i[1] = (int)n[1]; return 0; }
FAC(F_crater) { MK(B_crater, 3); b->s[0] = as_s(call, 0, "rim", "rock"); b->s[1] = as_s(call, 1, "pit", "pit"); as_2(call, 2, "R", 2.4, 3.2, b->v); return 0; }
FAC(F_boardwalk) { MK(B_boardwalk, 3); b->s[0] = as_s(call, 0, "kind", "board"); b->s[1] = as_s(call, 1, "beam", "wood"); as_2(call, 2, "L", 5.5, 8.0, b->v); return 0; }
FAC(F_cart) { MK(B_cart, 2); b->s[0] = as_s(call, 0, "body", "wood"); b->s[1] = as_s(call, 1, "wheel", "iron"); return 0; }
FAC(F_chest) { MK(B_chest, 2); b->s[0] = as_s(call, 0, "kind", "crate"); b->s[1] = as_s(call, 1, "metal", "iron"); return 0; }
FAC(F_tentacle) { MK(B_tentacle, 2); b->s[0] = as_s(call, 0, "kind", "tendril"); as_2(call, 1, "L", 6.0, 9.0, b->v); return 0; }
FAC(F_cracked_slab) { MK(B_cracked_slab, 2); b->s[0] = as_s(call, 0, "kind", "concrete"); as_2(call, 1, "R", 2.2, 2.9, b->v); return 0; }
FAC(F_fairy_ring) {
    const FtArg *t; MK(B_fairy_ring, 4);
    b->s[0] = as_s(call, 0, "kind", "shroom"); as_2(call, 1, "R", 2.4, 3.1, b->v); as_2(call, 2, "h", 0.6, 0.95, b->v + 2);
    t = arg(call, 3, "tiers");
    if (!t) { b->tiers.n = 2; b->tiers.t[0].frac = num_f(0.5); b->tiers.t[0].a = num_i(5); b->tiers.t[1].frac = num_f(0.5); b->tiers.t[1].a = num_i(50); }
    else {          /* 'R': n / 2 pairs; bit j of imask: v[j] was an int */
        b->tiers.n = t->n / 2;
        for (int j = 0; j < b->tiers.n && j < 8; j++) {
            double f = t->v[2 * j], g = t->v[2 * j + 1];
            b->tiers.t[j].frac = (t->imask >> (2 * j)) & 1 ? num_i(f) : num_f(f);
            b->tiers.t[j].a = (t->imask >> (2 * j + 1)) & 1 ? num_i(g) : num_f(g);
        }
    }
    return 0;
}
FAC(F_sign) { MK(B_sign, 2); b->s[0] = as_s(call, 0, "post", "wood"); b->s[1] = as_s(call, 1, "board", "board"); return 0; }
FAC(F_gear) { MK(B_gear, 2); b->s[0] = as_s(call, 0, "kind", "iron"); as_2(call, 1, "R", 1.6, 2.2, b->v); return 0; }
FAC(F_brazier) { MK(B_brazier, 2); b->s[0] = as_s(call, 0, "stand", "iron"); b->s[1] = as_s(call, 1, "fire", "fire"); return 0; }
FAC(F_tiles) { MK(B_tiles, 4); b->s[0] = as_s(call, 0, "kind", "pavers"); b->i[0] = (int)as_f(call, 1, "nx", 3); b->i[1] = (int)as_f(call, 2, "ny", 2); b->v[0] = as_f(call, 3, "size", 1.5); return 0; }
FAC(F_pile) { MK(B_pile, 4); b->s[0] = as_s(call, 0, "kind", "log"); as_2(call, 1, "L", 4.0, 5.2, b->v); b->v[2] = as_f(call, 2, "w", 1.0); b->i[0] = as_b(call, 3, "top", 1); return 0; }
FAC(F_hoops) { MK(B_hoops, 3); b->s[0] = as_s(call, 0, "kind", "barrel"); as_2(call, 1, "L", 3.0, 3.8, b->v); b->v[2] = as_f(call, 2, "w", 2.0); return 0; }
FAC(F_L_wall) { MK(L_wall, 2); b->s[0] = as_s(call, 0, "kind", "ruin"); b->s[1] = as_s(call, 1, "cap", NULL); return 0; }
FAC(F_L_track) { MK(L_track, 2); b->s[0] = as_s(call, 0, "rail", "iron"); b->s[1] = as_s(call, 1, "tie", "wood"); return 0; }
FAC(F_L_dais) { MK(L_dais, 4); b->s[0] = as_s(call, 0, "kind", "tflag"); b->s[1] = as_s(call, 1, "top_piece", "brazier"); b->s[2] = as_s(call, 2, "fire", "fire"); b->s[3] = as_s(call, 3, "metal", "iron"); return 0; }
FAC(F_L_river) { MK(L_river, 3); b->s[0] = as_s(call, 0, "kind", "water"); b->s[1] = as_s(call, 1, "bank", "rock"); b->s[2] = as_s(call, 2, "bridge", "board"); return 0; }
FAC(F_L_crater) { MK(L_crater, 2); b->s[0] = as_s(call, 0, "rim", "rock"); b->s[1] = as_s(call, 1, "pit", "pit"); return 0; }
FAC(F_L_colonnade) { MK(L_colonnade, 2); b->s[0] = as_s(call, 0, "kind", "tcolumn"); b->s[1] = as_s(call, 1, "floor", "tflag"); return 0; }
FAC(F_L_hull) { MK(L_hull, 2); b->s[0] = as_s(call, 0, "kind", "hull"); b->s[1] = as_s(call, 1, "rib", "hull"); return 0; }
FAC(F_L_vehicle) { MK(L_vehicle, 2); b->s[0] = as_s(call, 0, "kind", "iron"); b->s[1] = as_s(call, 1, "track", "iron"); return 0; }
FAC(F_L_tree) { MK(L_tree, 2); b->s[0] = as_s(call, 0, "kind", "stump"); b->s[1] = as_s(call, 1, "root", "root"); return 0; }
FAC(F_L_circle) { MK(L_circle, 2); b->s[0] = as_s(call, 0, "kind", "glyph"); b->s[1] = as_s(call, 1, "post", "tcolumn"); return 0; }
FAC(F_L_ruin) { MK(L_ruin, 2); b->s[0] = as_s(call, 0, "kind", "ruin"); b->s[1] = as_s(call, 1, "floor", "pavers"); return 0; }
FAC(F_L_pool) { MK(L_pool, 2); b->s[0] = as_s(call, 0, "stone_kind", "rock"); b->s[1] = as_s(call, 1, "water", "water"); return 0; }
FAC(F_L_boardwalk) { MK(L_boardwalk, 2); b->s[0] = as_s(call, 0, "kind", "board"); b->s[1] = as_s(call, 1, "beam", "wood"); return 0; }
FAC(F_L_nest) { MK(L_nest, 2); b->s[0] = as_s(call, 0, "rim", "twig"); b->s[1] = as_s(call, 1, "egg", "egg"); return 0; }
FAC(F_L_spires) { MK(L_spires, 1); b->s[0] = as_s(call, 0, "kind", "crystal"); return 0; }
FAC(F_L_tentacles) { MK(L_tentacles, 2); b->s[0] = as_s(call, 0, "kind", "tendril"); b->s[1] = as_s(call, 1, "pit", "pit"); return 0; }

/* a cart standing on a track (compose.c puts it there): the body over the rails, a wheel on each rail at `gauge`
   from the track's middle (P_track's rails), the hopper open on top */
int ft_track_cart(double cx, double cy, double a, PyRandom *rng, double gauge, const char *body, const char *wheel, Geo **foot, SoLayers *L) {
    Ar AR = { 0 }, *A = &AR; GL u = { 0 }; GEOSGeometry *box_, *wh, *d; double Ln, W; Frame ff; int R_ = SC_ERR;
    *foot = NULL;
    Ln = py_uniform(rng, 3.8, 4.6); W = 2 * gauge - 0.2; ff = lf_frame(cx, cy, a); CK(box_ = Rect(A, cx, cy, Ln, W, a));
    for (int su = -1; su <= 1; su += 2) for (int sv = -1; sv <= 1; sv += 2) { CK(d = RectF(A, &ff, su * (Ln / 2 - 0.9), sv * gauge, 1.3, 0.95, a)); gp(&u, d); }
    CK(wh = U(A, &u));
    CK(d = Rect(A, cx, cy, Ln, 2 * gauge + 1.0, a)); put_foot(foot, d);
    L_up(L, wh, 1.3, wheel); L_up(L, box_, py_uniform(rng, 2.0, 2.4), body);
    CK(d = Inset(A, box_, 0.5)); L_tcut(L, d, 1, 0.92);
    R_ = 0;
fail: gfree(&u); ar_free(A); if (R_ < 0) { lyl_free(L); geo_unref(*foot); *foot = NULL; } return R_;
}

/* a big outcrop (2026-11-20): the framing piece of a true-size base, standing at the rim outside the model's space. size: its
   radius, H: its tallest point (mm), a: the way out (to the rim), where the tallest piece stands; the rest step down toward the
   middle. Stone: 4-7 large stones in a tight cluster (they become chamfered rocks). Crystal: a big hex spire with 4-8 smaller
   ones leaning round it, on a few low stones. */
int ft_outcrop(double cx, double cy, double a, PyRandom *rng, double size, double H, int crystal, const char *rock, const char *gem, Geo **foot, SoLayers *L) {
    Ar AR = { 0 }, *A = &AR; GL ft = { 0 }; GEOSGeometry *q, *d; int R_ = SC_ERR, n, i; double ux = py_cos(a), uy = py_sin(a), vx = -uy, vy = ux;
    *foot = NULL;
    if (!crystal) {
        double r0 = size * py_uniform(rng, 0.6, 0.72);          /* the main boulder, at the back: rounded, about as tall as it is wide */
        CK(q = Stone(A, cx + ux * size * 0.25, cy + uy * size * 0.25, r0, rng, py_uniform(rng, 0.65, 0.9), 10)); L_mound(L, q, H, rock, &FT_ROUND_T); gp(&ft, q);
        n = (int)py_randint(rng, 3, 6);
        for (i = 0; i < n; i++) {
            double t = py_uniform(rng, -1.0, 1.0), f = py_uniform(rng, -0.7, 0.35), px = cx + vx * t * size * 0.75 + ux * f * size, py = cy + vy * t * size * 0.75 + uy * f * size;
            double r = size * py_uniform(rng, 0.3, 0.5), h = H * py_uniform(rng, 0.35, 0.7) * (f > 0 ? 1.0 : 0.7);          /* lower toward the middle */
            CK(q = Stone(A, px, py, r, rng, py_uniform(rng, 0.6, 0.9), 9)); L_mound(L, q, h, rock, py_random(rng) < 0.5 ? &FT_ROUND_T : &FT_ROCK_T); gp(&ft, q);
        }
    } else {
        double r0 = size * py_uniform(rng, 0.26, 0.32);
        CK(q = Hex_(A, cx + ux * size * 0.25, cy + uy * size * 0.25, r0, py_uniform(rng, 0, 1))); L_mound(L, q, H, gem, &FT_GEM_T); gp(&ft, q);
        n = (int)py_randint(rng, 4, 8);
        for (i = 0; i < n; i++) {
            double t = py_uniform(rng, 0, 6.283), dd = size * py_uniform(rng, 0.35, 0.7), r = size * py_uniform(rng, 0.11, 0.2), px = cx + ux * size * 0.25 + dd * py_cos(t), py = cy + uy * size * 0.25 + dd * py_sin(t);
            double back = (px - cx) * ux + (py - cy) * uy, h = H * py_uniform(rng, 0.3, 0.7) * (back > 0 ? 1.0 : 0.7); int ok = 1;
            CK(q = Hex_(A, px, py, r, py_uniform(rng, 0, 1)));
            for (int j = 0; j < ft.n; j++) if (!(shp_distance(q, ft.v[j]) > 0.3)) { ok = 0; break; }
            if (ok) { L_mound(L, q, h, gem, &FT_GEM_T); gp(&ft, q); }
        }
        for (i = 0; i < 3; i++) {          /* the rock they grow from, low */
            double t = py_uniform(rng, 0, 6.283), px = cx + size * 0.45 * py_cos(t), py = cy + size * 0.45 * py_sin(t); int ok = 1;
            CK(q = Stone(A, px, py, size * py_uniform(rng, 0.22, 0.32), rng, 0.8, 8));
            for (int j = 0; j < ft.n; j++) if (!(shp_distance(q, ft.v[j]) > 0.3)) { ok = 0; break; }
            if (ok) { L_mound(L, q, py_uniform(rng, 1.4, 2.4), rock, &FT_ROCK_T); gp(&ft, q); }
        }
    }
    CK(d = U(A, &ft)); CK(d = Hull(A, d)); put_foot(foot, d);
    R_ = 0;
fail: gfree(&ft); ar_free(A); if (R_ < 0) { lyl_free(L); geo_unref(*foot); *foot = NULL; } return R_;
}

/* b_skull / b_ribs / L_skeleton: features.py returns scenes.py's rounded builders */
static int delegate(const FtCall *call, const char *name, FtBuildFn *fn, void **ctx) {
    BldFactory f = bld_get("scenes", name); FtCall c2;
    if (!f) return -2;
    c2 = *call; c2.mod = "scenes"; c2.name = name;
    return f(&c2, fn, ctx);
}
FAC(F_skull) { return delegate(call, "b_skull", fn, ctx); }
FAC(F_ribs) { return delegate(call, "b_ribs", fn, ctx); }
FAC(F_L_skeleton) { return delegate(call, "L_skeleton", fn, ctx); }

static const struct { const char *name; BldFactory f; } TAB[] = {
    { "b_wheel", F_wheel }, { "b_ladder", F_ladder }, { "b_track", F_track }, { "b_campfire", F_campfire }, { "b_skull", F_skull },
    { "b_ribs", F_ribs }, { "b_stairs", F_stairs }, { "b_well", F_well }, { "b_fallen_column", F_fallen_column }, { "b_grate", F_grate },
    { "b_rune_circle", F_rune_circle }, { "b_crystals", F_crystals }, { "b_roots", F_roots }, { "b_stepping_pool", F_stepping_pool },
    { "b_tool", F_tool }, { "b_boss", F_boss }, { "b_nest", F_nest }, { "b_crater", F_crater }, { "b_boardwalk", F_boardwalk },
    { "b_cart", F_cart }, { "b_chest", F_chest }, { "b_tentacle", F_tentacle }, { "b_cracked_slab", F_cracked_slab },
    { "b_fairy_ring", F_fairy_ring }, { "b_sign", F_sign }, { "b_gear", F_gear }, { "b_brazier", F_brazier }, { "b_tiles", F_tiles },
    { "b_pile", F_pile }, { "b_hoops", F_hoops },
    { "L_wall", F_L_wall }, { "L_track", F_L_track }, { "L_dais", F_L_dais }, { "L_river", F_L_river }, { "L_skeleton", F_L_skeleton },
    { "L_crater", F_L_crater }, { "L_colonnade", F_L_colonnade }, { "L_hull", F_L_hull }, { "L_vehicle", F_L_vehicle }, { "L_tree", F_L_tree }, { "L_circle", F_L_circle },
    { "L_ruin", F_L_ruin }, { "L_pool", F_L_pool }, { "L_boardwalk", F_L_boardwalk }, { "L_nest", F_L_nest }, { "L_spires", F_L_spires },
    { "L_tentacles", F_L_tentacles },
};
void bld_register_features(void) { for (size_t i = 0; i < sizeof TAB / sizeof TAB[0]; i++) bld_add("features", TAB[i].name, TAB[i].f); }

int bld_ft_make(const FtCall *call, int landmark, FtBuildFn *fn, void **ctx) {
    char nm[96]; BldFactory f; FtCall c2; const char *mod = call->mod ? call->mod : "features";
    snprintf(nm, sizeof nm, "%s%s", landmark ? "L_" : "b_", call->name);          /* the Python name, whatever the module */
    f = bld_get(mod, nm);
    if (!f) return -2;
    c2 = *call; c2.mod = mod; c2.name = so_intern(nm);
    return f(&c2, fn, ctx);
}
