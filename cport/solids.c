/* solids.c: group G5 of the C port (see solids.h): the solids of scenes.py, sculpt and the "steps" layers, the
   _weather / _crate_layers layer makers, sd_r_models._Later / _fine and the drawing tools + class Obj of
   style_details.py. Each function names the Python routine it copies; the arithmetic is written in the Python's own
   order (built with -ffp-contract=off and no fast-math, so nothing is fused or reordered), the maths go through the
   UCRT functions CPython calls (pyx.h), and every shapely call is its shp.h twin. Proof: work\cscene\solids. */
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <math.h>
#include "geos_dyn.h"
#include "pyx.h"
#include "pyrandom.h"
#include "pyexact.h"
#include "shp.h"
#include "solids.h"

#define PI 3.141592653589793          /* math.pi */
#define NEST 0.015                     /* scenes.NEST */

SolidsCfg SO = { 0, 1.0, 1.0, 0.88, 15.0, 3.0, 12.0 };
int64_t so_chain_counter = 0;

int solids_init(int supports, double true_scale, double ts, double minw, double R0, double H, double rim) {
    SO.supports = supports; SO.true_scale = true_scale; SO.ts = ts; SO.minw = minw; SO.R0 = R0; SO.H = H; SO.rim = rim;
    return shp_init() && px_init();
}

/* scenes._chain: _CHAIN[0] += 1; return _CHAIN[0] */
int64_t so_chain(void) { return ++so_chain_counter; }

/* Python's max(a, b) of two floats: the first unless the second is greater */
static inline double pmax(double a, double b) { return b > a ? b : a; }
static inline double pmin(double a, double b) { return b < a ? b : a; }

/* ================================================================== gg.frame */
SoFrame so_frame(double cx, double cy, double a) { SoFrame f = { cx, cy, py_cos(a), py_sin(a) }; return f; }
void so_ff(const SoFrame *f, double u, double v, double *x, double *y) {
    *x = f->cx + u * f->c - v * f->s; *y = f->cy + u * f->s + v * f->c;
}

/* ================================================================== small lists */
static void gpush(SoGeoms *l, GEOSGeometry *g) {          /* takes g over */
    if (l->n == l->cap) { l->cap = l->cap ? 2 * l->cap : 8; l->v = (GEOSGeometry **)realloc(l->v, sizeof(*l->v) * (size_t)l->cap); }
    l->v[l->n++] = g;
}
static void gfree(SoGeoms *l) { for (int i = 0; i < l->n; i++) shp_free(l->v[i]); free(l->v); l->v = NULL; l->n = l->cap = 0; }

/* ================================================================== scenes._u and friends */
/* _u(gs): gs = [x for x in gs if x is not None and not x.is_empty]; unary_union(gs) if gs else Polygon() */
GEOSGeometry *so_u(const GEOSGeometry *const *gs, int n) {
    const GEOSGeometry **k = (const GEOSGeometry **)malloc(sizeof(*k) * (size_t)(n ? n : 1)); int m = 0; GEOSGeometry *r;
    for (int i = 0; i < n; i++) {
        if (!gs[i]) continue;
        int e = shp_is_empty(gs[i]);
        if (e < 0) { free(k); return NULL; }
        if (!e) k[m++] = gs[i];
    }
    r = m ? shp_union_all(k, m) : shp_polygon(NULL, 0);
    free(k);
    return r;
}
/* style_details.U(gs): unary_union([x for x in gs if x is not None and not x.is_empty]) (nothing: GEOMETRYCOLLECTION EMPTY) */
GEOSGeometry *sd_U(const GEOSGeometry *const *gs, int n) {
    const GEOSGeometry **k = (const GEOSGeometry **)malloc(sizeof(*k) * (size_t)(n ? n : 1)); int m = 0; GEOSGeometry *r;
    for (int i = 0; i < n; i++) {
        if (!gs[i]) continue;
        int e = shp_is_empty(gs[i]);
        if (e < 0) { free(k); return NULL; }
        if (!e) k[m++] = gs[i];
    }
    r = shp_union_all(k, m);
    free(k);
    return r;
}
static GEOSGeometry *u_list(SoGeoms *l) { GEOSGeometry *r = so_u((const GEOSGeometry *const *)l->v, l->n); gfree(l); return r; }   /* _u of an owned list, freed */
static GEOSGeometry *U_list(SoGeoms *l) { GEOSGeometry *r = sd_U((const GEOSGeometry *const *)l->v, l->n); gfree(l); return r; }

/* _disc(x, y, r, n=20): Point(x, y).buffer(r, n); disc() is the same */
GEOSGeometry *so_disc(double x, double y, double r, int n) { return shp_point_buffer(x, y, r, n); }
GEOSGeometry *sd_disc(double x, double y, double r, int n) { return shp_point_buffer(x, y, r, n); }
/* _ann(x, y, ro, ri, n=24): Point(x, y).buffer(ro, n).difference(Point(x, y).buffer(ri, n)); ann() is the same */
GEOSGeometry *so_ann(double x, double y, double ro, double ri, int n) {
    GEOSGeometry *a = shp_point_buffer(x, y, ro, n), *b = a ? shp_point_buffer(x, y, ri, n) : NULL, *r = b ? shp_difference(a, b) : NULL;
    shp_free(a); shp_free(b);
    return r;
}
GEOSGeometry *sd_ann(double x, double y, double ro, double ri, int n) { return so_ann(x, y, ro, ri, n); }
/* _cap(p, q, w) / capl: LineString([p, q]).buffer(w / 2, 8) */
GEOSGeometry *so_cap(const double *p, const double *q, double w) {
    double xy[4] = { p[0], p[1], q[0], q[1] };
    return shp_line_buffer(xy, 2, w / 2, SHP_CAP_ROUND, SHP_JOIN_ROUND, 8, 5.0);
}
GEOSGeometry *sd_capl(const double *p, const double *q, double w) { return so_cap(p, q, w); }
/* _bar(p, q, w) / barl: LineString([p, q]).buffer(w / 2, cap_style=2) */
GEOSGeometry *so_bar(const double *p, const double *q, double w) {
    double xy[4] = { p[0], p[1], q[0], q[1] };
    return shp_line_buffer(xy, 2, w / 2, SHP_CAP_FLAT, SHP_JOIN_ROUND, 16, 5.0);
}
GEOSGeometry *sd_barl(const double *p, const double *q, double w) { return so_bar(p, q, w); }
static GEOSGeometry *bar_ff(const SoFrame *f, double u0, double v0, double u1, double v1, double w) {   /* _bar(ff(u0, v0), ff(u1, v1), w) */
    double p[2], q[2]; so_ff(f, u0, v0, &p[0], &p[1]); so_ff(f, u1, v1, &q[0], &q[1]);
    return so_bar(p, q, w);
}

/* ================================================================== gg.stone (TODO-leaf: G2 owns it) */
static int cmp_xy(const void *a, const void *b) {          /* Python's tuple order */
    const double *p = (const double *)a, *q = (const double *)b;
    if (p[0] < q[0]) return -1;
    if (p[0] > q[0]) return 1;
    return p[1] < q[1] ? -1 : p[1] > q[1];
}
static int cmp_dbl(const void *a, const void *b) { double x = *(const double *)a, y = *(const double *)b; return x < y ? -1 : x > y; }
/* gg._hull(pts): P = sorted(set(pts)); half(P) + half(reversed(P)) minus their last points. -1 when < 3 (Python
   then hands back the points and Polygon() raises) */
static int hull_pts(const double *pts, int n, double *out) {
    double P[64], h[66]; int m = 0, k, nh = 0;
    if (n > 32) return -1;
    memcpy(P, pts, sizeof(double) * 2 * (size_t)n); qsort(P, (size_t)n, 2 * sizeof(double), cmp_xy);
    for (int i = 0; i < n; i++) if (!m || P[2 * i] != P[2 * (m - 1)] || P[2 * i + 1] != P[2 * (m - 1) + 1]) { P[2 * m] = P[2 * i]; P[2 * m + 1] = P[2 * i + 1]; m++; }
    if (m < 3) return -1;
    for (int pass = 0; pass < 2; pass++) {
        k = 0;
        for (int t = 0; t < m; t++) {
            int j = pass ? m - 1 - t : t; double px = P[2 * j], py = P[2 * j + 1];
            while (k >= 2 && (h[2 * (k - 1)] - h[2 * (k - 2)]) * (py - h[2 * (k - 2) + 1]) - (h[2 * (k - 1) + 1] - h[2 * (k - 2) + 1]) * (px - h[2 * (k - 2)]) <= 0) k--;
            h[2 * k] = px; h[2 * k + 1] = py; k++;
        }
        for (int i = 0; i < k - 1; i++) { out[2 * nh] = h[2 * i]; out[2 * nh + 1] = h[2 * i + 1]; nh++; }
    }
    return nh;
}
/* gg.stone(cx, cy, r, rng, aspect, nv) (not round_): aspect 0 / nv 0 = None (drawn) */
GEOSGeometry *so_stone(double cx, double cy, double r, PyRandom *rng, double aspect, int nv) {
    int n = nv ? nv : (int)py_randint(rng, 5, 8), nh;
    double asp = aspect != 0.0 ? aspect : py_uniform(rng, .6, .95), rot = py_uniform(rng, 0, PI), ts[32], pts[64], hp[128], cr, sr;
    if (n < 1 || n > 32) return NULL;
    for (int i = 0; i < n; i++) ts[i] = py_uniform(rng, 0, 2 * PI);
    qsort(ts, (size_t)n, sizeof(double), cmp_dbl);
    for (int i = 0; i < n; i++) ts[i] = (double)(i * 2) * PI / n * 0.6 + ts[i] * 0.4;          /* i * 2 * math.pi / n * 0.6 + t * 0.4 */
    cr = py_cos(rot); sr = py_sin(rot);          /* math.cos(rot) etc. are computed per point in Python: the same values */
    for (int i = 0; i < n; i++) {
        double rr = r * py_uniform(rng, .82, 1.1), x = rr * py_cos(ts[i]), y = rr * py_sin(ts[i]) * asp;
        pts[2 * i] = cx + x * cr - y * sr; pts[2 * i + 1] = cy + x * sr + y * cr;
    }
    nh = hull_pts(pts, n, hp);
    if (nh < 3) return NULL;
    return shp_polygon(hp, nh);
}

/* ================================================================== solids */
enum { S_BOX, S_SLAB, S_VAULT, S_LATHE, S_LOFT, S_TUBE, S_CUSTOM };
enum { PROF_ROUND = 0, PROF_RIDGE = 1, PROF_LOAF = 2, PROF_OTHER = 3 };
struct Solid {
    int rc, type, has3d; double top;
    GEOSGeometry *poly; double lift, h, z0, z1;                       /* box, slab */
    double cx, cy, a, L, W; int prof, under, has_axle; double axle;  /* vault (loft: prof round / other) */
    double *sm; int nsm, seg; double squash; double *real; int nreal; /* lathe */
    SoFrame ff; int m; double *us, *rs, hk;                           /* loft */
    GEOSGeometry **P; double *R, arch;                                /* tube */
    SoSectFn fn; void *ctx; void (*ctx_free)(void *);                 /* custom */
};
static Solid *sol_new(int type) { Solid *s = (Solid *)calloc(1, sizeof(Solid)); s->rc = 1; s->type = type; return s; }
Solid *sol_ref(Solid *s) { if (s) s->rc++; return s; }
void sol_unref(Solid *s) {
    if (!s || --s->rc > 0) return;
    shp_free(s->poly); free(s->sm); free(s->real); free(s->us); free(s->rs); free(s->R);
    if (s->P) { for (int i = 0; i <= s->m; i++) shp_free(s->P[i]); free(s->P); }
    if (s->ctx_free) s->ctx_free(s->ctx);
    free(s);
}
void sol_push(SoSolids *l, Solid *s) {
    if (l->n == l->cap) { l->cap = l->cap ? 2 * l->cap : 8; l->v = (Solid **)realloc(l->v, sizeof(*l->v) * (size_t)l->cap); }
    l->v[l->n++] = sol_ref(s);
}
static void sol_push_own(SoSolids *l, Solid *s) { sol_push(l, s); sol_unref(s); }
void sol_extend(SoSolids *l, const SoSolids *o) { for (int i = 0; i < o->n; i++) sol_push(l, o->v[i]); }
void sol_free(SoSolids *l) { for (int i = 0; i < l->n; i++) sol_unref(l->v[i]); free(l->v); l->v = NULL; l->n = l->cap = 0; }
double sol_top(const Solid *s) { return s->top; }
int sol_has3d(const Solid *s) { return s->has3d; }
Solid *sol_custom(SoSectFn fn, void *ctx, void (*ctx_free)(void *), double top) {
    Solid *s = sol_new(S_CUSTOM); s->fn = fn; s->ctx = ctx; s->ctx_free = ctx_free; s->top = top; return s;
}
/* what the x[2] lambdas of slab / vault / lathe / loft / tube capture (solids3d.c builds them) */
int so_view3d(const Solid *s, So3dView *v) {
    memset(v, 0, sizeof *v);
    if (!s || !s->has3d) return SO_ERR;
    v->cx = s->cx; v->cy = s->cy; v->a = s->a; v->lift = s->lift;
    switch (s->type) {
    case S_SLAB: v->type = SO3_SLAB; v->poly = s->poly; v->z0 = s->z0; v->z1 = s->z1; break;
    case S_VAULT: v->type = SO3_VAULT; v->L = s->L; v->W = s->W; v->has_axle = s->has_axle; v->axle = s->axle; break;
    case S_LATHE: v->type = SO3_LATHE; v->real = s->real; v->nreal = s->nreal; v->seg = s->seg; v->squash = s->squash; break;
    case S_LOFT: v->type = SO3_LOFT; v->ff = s->ff; v->m = s->m; v->us = s->us; v->rs = s->rs; break;
    case S_TUBE: v->type = SO3_TUBE; v->m = s->m; v->P = s->P; v->R = s->R; v->arch = s->arch; break;
    default: return SO_ERR;
    }
    return SO_OK;
}

/* box(poly, h, lift=0.0): [((lambda z: poly if z < lift + h else None), lift + h)] */
int so_box(SoSolids *out, const GEOSGeometry *poly, double h, double lift) {
    Solid *s = sol_new(S_BOX); s->poly = shp_clone(poly); s->h = h; s->lift = lift; s->top = lift + h;
    sol_push_own(out, s); return SO_OK;
}
/* slab(poly, z0, z1): sect = lambda z: poly if z < z1 else None; [(sect, z1[, the 3D block])] */
int so_slab(SoSolids *out, const GEOSGeometry *poly, double z0, double z1) {
    Solid *s = sol_new(S_SLAB); s->poly = shp_clone(poly); s->z0 = z0; s->z1 = z1; s->top = z1; s->has3d = SO.supports;
    sol_push_own(out, s); return SO_OK;
}
static int prof_code(const char *p) {
    if (!p) return PROF_OTHER;
    if (!strcmp(p, "round")) return PROF_ROUND;
    if (!strcmp(p, "ridge")) return PROF_RIDGE;
    if (!strcmp(p, "loaf")) return PROF_LOAF;
    return PROF_OTHER;
}
/* vault(cx, cy, a, L, W, h, prof="round", n=None, lift=0.0, under=False, axle=None) */
int so_vault(SoSolids *out, double cx, double cy, double a, double L, double W, double h, const char *prof, double lift, int under, int has_axle, double axle) {
    Solid *s = sol_new(S_VAULT);
    s->cx = cx; s->cy = cy; s->a = a; s->L = L; s->W = W; s->h = h; s->prof = prof_code(prof); s->lift = lift; s->under = under;
    s->has_axle = has_axle; s->axle = axle; s->top = lift + h;
    s->has3d = under && s->prof == PROF_ROUND && SO.supports;
    sol_push_own(out, s); return SO_OK;
}
/* lathe(cx, cy, prof, seg=28, squash=1.0, a=0.0, n=None, lift=0.0, real=None) */
int so_lathe(SoSolids *out, double cx, double cy, const double *prof, int np, int seg, double squash, double a, double lift, const double *real, int nreal) {
    int cap = 64, nsm = 0; double *sm = (double *)malloc(sizeof(double) * 2 * (size_t)cap);
    for (int i = 0; i + 1 < np; i++) {
        double r0 = prof[2 * i], z0 = prof[2 * i + 1], r1 = prof[2 * i + 2], z1 = prof[2 * i + 3]; int64_t mi;
        if (px_int(pmax(fabs(z1 - z0), fabs(r1 - r0)) / 0.03, &mi) != PX_OK) { free(sm); return SO_ERR; }
        int64_t m = mi > 1 ? mi : 1;          /* max(1, int(...)) */
        if (m > 100000000) { free(sm); return SO_ERR; }
        for (int64_t k = 0; k <= m; k++) {
            if (nsm == cap) { cap *= 2; sm = (double *)realloc(sm, sizeof(double) * 2 * (size_t)cap); }
            sm[2 * nsm] = r0 + (r1 - r0) * (double)k / (double)m; sm[2 * nsm + 1] = z0 + (z1 - z0) * (double)k / (double)m; nsm++;
        }
    }
    if (!nsm) { free(sm); return SO_ERR; }          /* top = max(z for _, z in sm): ValueError on an empty profile */
    double top = sm[1];
    for (int i = 1; i < nsm; i++) top = pmax(top, sm[2 * i + 1]);
    Solid *s = sol_new(S_LATHE);
    s->cx = cx; s->cy = cy; s->sm = sm; s->nsm = nsm; s->seg = seg; s->squash = squash; s->a = a; s->lift = lift; s->top = lift + top;
    if (real && nreal > 0 && SO.supports) {          /* real and SUPPORTS(): the true turned solid */
        s->has3d = 1; s->real = (double *)malloc(sizeof(double) * 2 * (size_t)nreal); memcpy(s->real, real, sizeof(double) * 2 * (size_t)nreal); s->nreal = nreal;
    }
    sol_push_own(out, s); return SO_OK;
}
/* loft(cx, cy, a, L, rfun, hk=1.2, prof="round", n=None, lift=0.0, under=False) */
int so_loft(SoSolids *out, double cx, double cy, double a, double L, SoNumFn rfun, void *ctx, double hk, const char *prof, double lift, int under) {
    int64_t mi; int m;
    if (px_int(L / 0.13, &mi) != PX_OK) return SO_ERR;
    if (mi < 12) mi = 12;          /* max(12, int(L / 0.13)) */
    if (mi > 10000000) return SO_ERR;
    m = (int)mi;
    Solid *s = sol_new(S_LOFT);
    s->ff = so_frame(cx, cy, a); s->m = m; s->us = (double *)malloc(sizeof(double) * (size_t)(m + 1)); s->rs = (double *)malloc(sizeof(double) * (size_t)(m + 1));
    for (int i = 0; i <= m; i++) s->us[i] = -L / 2 + L * (double)i / (double)m;          /* us = [-L / 2 + L * i / m ...] */
    for (int i = 0; i <= m; i++) {          /* rs = [max(0.0, _real(rfun(u))) for u in us] */
        double v;
        if (rfun(ctx, s->us[i], &v) != SO_OK) { sol_unref(s); return SO_ERR; }
        s->rs[i] = v > 0.0 ? v : 0.0;
    }
    s->hk = hk; s->prof = prof && !strcmp(prof, "round") ? PROF_ROUND : PROF_OTHER; s->lift = lift; s->under = under;
    double mr = s->rs[0];
    for (int i = 1; i <= m; i++) mr = pmax(mr, s->rs[i]);
    s->top = lift + mr * hk;
    if (under && s->prof == PROF_ROUND && SO.supports) {          /* k3 = [i for i in range(0, m + 1, 2) if rs[i] >= 0.15] */
        int nk = 0;
        for (int i = 0; i <= m; i += 2) nk += s->rs[i] >= 0.15;
        s->has3d = nk >= 2;
    }
    sol_push_own(out, s); return SO_OK;
}
/* tube(pts, r, hk=1.0, n=None, lift=0.0, under=False, arch=0.0) */
int so_tube(SoSolids *out, const double *pts, int npts, double r, SoNumFn rfun, void *ctx, double hk, double lift, int under, double arch) {
    GEOSGeometry *ln = shp_linestring(pts, npts); int64_t mi; int m;
    if (!ln) return SO_ERR;
    if (px_int(shp_length(ln) / 0.22, &mi) != PX_OK) { shp_free(ln); return SO_ERR; }
    if (mi < 2) mi = 2;
    if (mi > 10000000) { shp_free(ln); return SO_ERR; }
    m = (int)mi;
    Solid *s = sol_new(S_TUBE);
    s->m = m; s->P = (GEOSGeometry **)calloc((size_t)(m + 1), sizeof(GEOSGeometry *)); s->R = (double *)malloc(sizeof(double) * (size_t)(m + 1));
    for (int i = 0; i <= m; i++) if (!(s->P[i] = shp_interpolate(ln, (double)i / (double)m, 1))) { shp_free(ln); sol_unref(s); return SO_ERR; }
    shp_free(ln);
    for (int i = 0; i <= m; i++) {          /* R = [max(0.0, r(i / m) if callable(r) else r) ...] */
        double v = r;
        if (rfun && rfun(ctx, (double)i / (double)m, &v) != SO_OK) { sol_unref(s); return SO_ERR; }
        s->R[i] = v > 0.0 ? v : 0.0;
    }
    s->hk = hk; s->lift = lift; s->under = under; s->arch = arch;
    double mr = s->R[0];
    for (int i = 1; i <= m; i++) mr = pmax(mr, s->R[i]);
    s->top = lift + mr * hk;
    if ((under || arch != 0.0) && SO.supports) {
        int nk = 0;
        for (int i = 0; i <= m; i++) nk += s->R[i] >= 0.12;
        s->has3d = nk >= 2;
    }
    sol_push_own(out, s); return SO_OK;
}

/* the section functions */
static int sect_vault(const Solid *s, double z, GEOSGeometry **out) {
    double w, t;
    if (s->h == 0.0) return SO_ERR;                    /* (z - lift) / h: ZeroDivisionError */
    t = pmax(0.0, (z - s->lift) / s->h);
    if (t >= 1) { *out = NULL; return SO_OK; }
    if (s->prof == PROF_ROUND) w = s->W / 2 * py_sqrt_u(1 - t * t);
    else if (s->prof == PROF_RIDGE) w = s->W / 2 * (1 - t);
    else if (s->prof == PROF_LOAF) { double re, im; if (px_pow(1 - t * t, 0.25, &re, &im) != 0) return SO_ERR; w = s->W / 2 * re; }
    else return SO_ERR;                                /* KeyError */
    if (!(w >= 0.2)) { *out = NULL; return SO_OK; }
    return (*out = shp_rect(s->cx, s->cy, s->L, 2 * w, s->a)) ? SO_OK : SO_ERR;
}
static int sect_lathe(const Solid *s, double z, GEOSGeometry **out) {
    double zz = z - s->lift, rr = 0.0; int first = 1; GEOSGeometry *d, *k, *r;
    for (int i = 0; i < s->nsm; i++) if (s->sm[2 * i + 1] > zz) { if (first || s->sm[2 * i] > rr) rr = s->sm[2 * i]; first = 0; }   /* max(..., default=0.0) */
    if (rr < 0.2) { *out = NULL; return SO_OK; }
    if (!(d = so_disc(s->cx, s->cy, rr, s->seg))) return SO_ERR;
    if (s->squash == 1.0) { *out = d; return SO_OK; }
    k = shp_scale(d, 1.0, s->squash, SHP_ORIGIN_POINT, s->cx, s->cy); shp_free(d);          /* _ascale(d, 1.0, squash, origin=(cx, cy)) */
    if (!k) return SO_ERR;
    r = shp_rotate(k, s->a, SHP_ORIGIN_POINT, s->cx, s->cy, 1); shp_free(k);                  /* _arot(..., a, origin=(cx, cy), use_radians=True) */
    return (*out = r) ? SO_OK : SO_ERR;
}
static int sect_loft(const Solid *s, double z, GEOSGeometry **out) {
    double zl = pmax(0.0, z - s->lift); int m = s->m, nrun = 0, i0 = 0; SoGeoms parts = { 0 };
    double *ru = (double *)malloc(sizeof(double) * (size_t)(m + 2)), *rw = (double *)malloc(sizeof(double) * (size_t)(m + 2));
    for (int i = 0; i <= m + 1; i++) {          /* list(zip(us, rs)) + [(0, 0.0)] */
        double u = i <= m ? s->us[i] : 0.0, r = i <= m ? s->rs[i] : 0.0, t, w;
        if (r > 0.05) { double d = r * s->hk; if (d == 0.0) goto err; t = zl / d; } else t = 2.0;
        if (t >= 1) w = 0.0;
        else if (s->prof == PROF_ROUND) { double q = 1 - t * t; if (q < 0) goto err; w = r * py_sqrt_u(q); }          /* math.sqrt: ValueError below 0 */
        else w = r * (1 - t);
        if (w >= 0.2) { ru[nrun] = u; rw[nrun] = w; nrun++; continue; }
        if (nrun >= 2 && ru[nrun - 1] - ru[0] >= 0.3) {
            double *xy = (double *)malloc(sizeof(double) * 4 * (size_t)nrun); GEOSGeometry *g; int v;
            for (int j = 0; j < nrun; j++) so_ff(&s->ff, ru[j], rw[j], &xy[2 * j], &xy[2 * j + 1]);
            for (int j = 0; j < nrun; j++) so_ff(&s->ff, ru[nrun - 1 - j], -rw[nrun - 1 - j], &xy[2 * (nrun + j)], &xy[2 * (nrun + j) + 1]);
            g = shp_polygon(xy, 2 * nrun); free(xy);
            if (!g) goto err;
            v = shp_is_valid(g);
            if (v < 0) { shp_free(g); goto err; }
            if (v) gpush(&parts, g); else shp_free(g);
        }
        nrun = 0;
    }
    (void)i0;
    free(ru); free(rw);
    if (!parts.n) { *out = NULL; return SO_OK; }
    return (*out = u_list(&parts)) ? SO_OK : SO_ERR;
err:
    free(ru); free(rw); gfree(&parts); return SO_ERR;
}
static int sect_tube(const Solid *s, double z, GEOSGeometry **out) {
    double zl = pmax(0.0, z - s->lift); int m = s->m; SoGeoms parts = { 0 };
    double *ws = (double *)malloc(sizeof(double) * (size_t)(m + 1));
    for (int i = 0; i <= m; i++) {          /* ri * math.sqrt(1 - (zl / (ri * hk)) ** 2) if ri > 0.05 and zl < ri * hk else 0.0 */
        double ri = s->R[i];
        if (ri > 0.05 && zl < ri * s->hk) {
            double re, im, q;
            if (px_pow(zl / (ri * s->hk), 2.0, &re, &im) != 0) { free(ws); return SO_ERR; }
            q = 1 - re;
            if (q < 0) { free(ws); return SO_ERR; }
            ws[i] = ri * py_sqrt_u(q);
        } else ws[i] = 0.0;
    }
    for (int i = 0; i < m; i++) {
        if (!(ws[i] >= 0.2 && ws[i + 1] >= 0.2)) continue;
        GEOSGeometry *b[2], *u, *h;          /* _u([P[i].buffer(ws[i], 5), P[i + 1].buffer(ws[i + 1], 5)]).convex_hull */
        b[0] = shp_buffer_q(s->P[i], ws[i], 5); b[1] = b[0] ? shp_buffer_q(s->P[i + 1], ws[i + 1], 5) : NULL;
        u = b[1] ? so_u((const GEOSGeometry *const *)b, 2) : NULL;
        shp_free(b[0]); shp_free(b[1]);
        h = u ? shp_convex_hull(u) : NULL; shp_free(u);
        if (!h) { free(ws); gfree(&parts); return SO_ERR; }
        gpush(&parts, h);
    }
    free(ws);
    if (!parts.n) { *out = NULL; return SO_OK; }
    return (*out = u_list(&parts)) ? SO_OK : SO_ERR;
}
int sol_sect(const Solid *s, double z, GEOSGeometry **out) {
    *out = NULL;
    switch (s->type) {
    case S_BOX: if (z < s->lift + s->h) { if (!(*out = shp_clone(s->poly))) return SO_ERR; } return SO_OK;
    case S_SLAB: if (z < s->z1) { if (!(*out = shp_clone(s->poly))) return SO_ERR; } return SO_OK;
    case S_VAULT: return sect_vault(s, z, out);
    case S_LATHE: return sect_lathe(s, z, out);
    case S_LOFT: return sect_loft(s, z, out);
    case S_TUBE: return sect_tube(s, z, out);
    case S_CUSTOM: { int r = s->fn(s->ctx, z, out); if (r != SO_OK) { shp_free(*out); *out = NULL; } return r; }
    }
    return SO_ERR;
}

/* curve(r0, r1, z0, z1, n=6, out=True): t = k / n * math.pi / 2 */
int so_curve(double r0, double r1, double z0, double z1, int n, int out_, double *prof) {
    if (n == 0) return SO_ERR;                         /* k / n: ZeroDivisionError */
    for (int k = 0; k <= n; k++) {
        double t = (double)k / (double)n * PI / 2;
        if (out_) { prof[2 * k] = r1 + (r0 - r1) * py_cos(t); prof[2 * k + 1] = z0 + (z1 - z0) * py_sin(t); }
        else { prof[2 * k] = r0 + (r1 - r0) * py_sin(t); prof[2 * k + 1] = z1 + (z0 - z1) * py_cos(t); }
    }
    return SO_OK;
}

/* ================================================================== TRUE3D / TOP3 */
typedef struct { int64_t c; SoSolids s; double top3; } T3;
static T3 *t3v; static int t3n, t3cap;
static T3 *t3_find(int64_t c) { for (int i = t3n - 1; i >= 0; i--) if (t3v[i].c == c) return &t3v[i]; return NULL; }
const SoSolids *so_true3d(int64_t c) { T3 *t = t3_find(c); return t ? &t->s : NULL; }
int so_top3(int64_t c, double *top3) { T3 *t = t3_find(c); if (t) *top3 = t->top3; return t != NULL; }
void so_true3d_clear(void) { for (int i = 0; i < t3n; i++) sol_free(&t3v[i].s); t3n = 0; }
static void t3_set(int64_t c, const SoSolids *s, double top3) {          /* TRUE3D[c] = ...; TOP3[c] = ... (a dict: a key set again is replaced) */
    T3 *t = t3_find(c);
    if (!t) {
        if (t3n == t3cap) { t3cap = t3cap ? 2 * t3cap : 16; t3v = (T3 *)realloc(t3v, sizeof(T3) * (size_t)t3cap); }
        t = &t3v[t3n++]; memset(t, 0, sizeof(*t)); t->c = c;
    } else sol_free(&t->s);
    sol_extend(&t->s, s); t->top3 = top3;
}

/* ================================================================== sculpt */
void sop_free(SoPieces *p) { for (int i = 0; i < p->n; i++) shp_free(p->v[i].g); free(p->v); p->v = NULL; p->n = p->cap = 0; }
static void sop_push(SoPieces *p, GEOSGeometry *g, double ztop, int64_t c, double z, int ghost) {
    if (p->n == p->cap) { p->cap = p->cap ? 2 * p->cap : 16; p->v = (SoPiece *)realloc(p->v, sizeof(SoPiece) * (size_t)p->cap); }
    SoPiece *q = &p->v[p->n++]; q->g = g; q->ztop = ztop; q->c = c; q->z = z; q->ghost = ghost;
}
/* _u([q for q in polys_of(g) if q.area > amin]) */
static GEOSGeometry *u_polys_over(const GEOSGeometry *g, double amin) {
    ShpList l = { 0 }; SoGeoms k = { 0 };
    shp_polys_of(g, &l);
    for (int i = 0; i < l.n; i++) { if (shp_area(l.g[i]) > amin) { gpush(&k, l.g[i]); l.g[i] = NULL; } }
    shp_list_free(&l);
    return u_list(&k);
}

/* sculpt(solids, cuts=(), n=None) */
int so_sculpt(const SoSolids *solids, const SoCut *cuts, int ncuts, int n, SoPieces *out) {
    SoSolids t3 = { 0 }, sol = { 0 }; int64_t c = 0; GEOSGeometry *foot3 = NULL, *prev = NULL, *raw = NULL; double top3 = 0, top, *zs = NULL, dz; int ok = SO_ERR, nz;
    out->n = 0;
    for (int i = 0; i < solids->n; i++) if (solids->v[i]->has3d) sol_push(&t3, solids->v[i]);
    if (t3.n) {          /* --supports on: true solids, a flat marker of their footprint stays in the layout */
        SoGeoms f = { 0 };
        c = so_chain(); top3 = t3.v[0]->top;
        for (int i = 1; i < t3.n; i++) top3 = pmax(top3, t3.v[i]->top);
        t3_set(c, &t3, top3);          /* TRUE3D[c] = [x[2] for x in t3]; TOP3[c] = max(x[1] for x in t3) */
        for (int i = 0; i < t3.n; i++) { GEOSGeometry *g; if (sol_sect(t3.v[i], 0.0, &g) != SO_OK) { gfree(&f); goto done; } if (g) gpush(&f, g); }
        if (!(foot3 = u_list(&f))) goto done;
        for (int i = 0; i < solids->n; i++) if (!solids->v[i]->has3d) sol_push(&sol, solids->v[i]);
        if (!sol.n) { sop_push(out, foot3, top3, c, 0.0, 1); foot3 = NULL; ok = SO_OK; goto done; }
    } else sol_extend(&sol, solids);
    if (!sol.n) goto done;                              /* max() of nothing: ValueError */
    top = sol.v[0]->top;
    for (int i = 1; i < sol.n; i++) top = pmax(top, sol.v[i]->top);
    if (!n) {          /* n or (max(6, min(18, int(top / 0.15))) if _ts == 1.0 else max(8, min(40, int(top * _ts / 0.15)))) */
        int64_t k;
        if (SO.true_scale == 1.0) { if (px_int(top / 0.15, &k) != PX_OK) goto done; k = k < 18 ? k : 18; n = (int)(k > 6 ? k : 6); }
        else { if (px_int(top * SO.true_scale / 0.15, &k) != PX_OK) goto done; k = k < 40 ? k : 40; n = (int)(k > 8 ? k : 8); }
    }
    if (!t3.n) c = so_chain();
    if (t3.n) { sop_push(out, foot3, top3, c, 0.0, 1); foot3 = NULL; }
    if (n < 0) n = 0;
    zs = (double *)malloc(sizeof(double) * (size_t)(n + 3));
    for (int j = 0; j < n; j++) zs[j] = top * (double)j / (double)n;
    zs[n] = top * (1 - 0.45 / n); zs[n + 1] = top * (1 - 0.12 / n); zs[n + 2] = top;
    nz = n + 2;                                         /* n = len(zs) - 1 */
    dz = top / (double)(nz - 2 > 1 ? nz - 2 : 1);       /* top / max(1, n - 2) */
    for (int j = 0; j < nz; j++) {
        double z = zs[j], zq = z + 1e-6; SoGeoms parts = { 0 }; GEOSGeometry *U; int e;
        for (int i = 0; i < sol.n; i++) {
            GEOSGeometry *g;
            if (!(sol.v[i]->top > zq)) continue;
            if (sol_sect(sol.v[i], zq, &g) != SO_OK) { gfree(&parts); goto done; }
            if (g) { int ge = shp_is_empty(g); if (ge < 0) { shp_free(g); gfree(&parts); goto done; } if (ge) shp_free(g); else gpush(&parts, g); }
        }
        if (!(U = u_list(&parts))) goto done;
        for (int k = 0; k < ncuts; k++) {
            if (z >= cuts[k].fl - 1e-9) {          /* U.difference(q.buffer(0.025 * (z - fl) / dz, join_style=2)) */
                GEOSGeometry *b, *d;
                if (dz == 0.0) { shp_free(U); goto done; }
                b = shp_buffer(cuts[k].q, 0.025 * (z - cuts[k].fl) / dz, 16, SHP_CAP_ROUND, SHP_JOIN_MITRE, 5.0, 0);
                d = b ? shp_difference(U, b) : NULL; shp_free(b); shp_free(U);
                if (!(U = d)) goto done;
            }
        }
        if ((e = shp_is_empty(U)) < 0) { shp_free(U); goto done; }
        if (!e) {          /* hairline gaps closed, no neck thinner than 0.14 */
            GEOSGeometry *b1 = shp_buffer_q(U, 0.06, 4), *b2 = b1 ? shp_buffer_q(b1, -0.14, 4) : NULL, *b3 = b2 ? shp_buffer_q(b2, 0.08, 4) : NULL;
            shp_free(b1); shp_free(b2); shp_free(U);
            if (!b3) goto done;
            U = u_polys_over(b3, 0.12); shp_free(b3);
            if (!U) goto done;
        }
        if ((e = shp_is_empty(U)) < 0) { shp_free(U); goto done; }
        if (e) { shp_free(U); break; }
        if (raw && out->n && !out->v[out->n - 1].ghost) {          /* the outline did not change: the piece below stands taller */
            GEOSGeometry *x = shp_intersection(U, raw);
            if (!x) { shp_free(U); goto done; }
            double d = shp_area(raw) - shp_area(x); shp_free(x);
            if (d < 0.02) { out->v[out->n - 1].ztop = zs[j + 1]; shp_free(U); continue; }
        }
        shp_free(raw); raw = U; U = shp_clone(raw);
        if (prev) {          /* each piece a hair inside the one below */
            GEOSGeometry *pb = shp_buffer(prev, -NEST, 16, SHP_CAP_ROUND, SHP_JOIN_MITRE, 5.0, 0), *x = pb ? shp_intersection(U, pb) : NULL;
            shp_free(pb); shp_free(U);
            if (!x) goto done;
            U = u_polys_over(x, 0.1); shp_free(x);
            if (!U) goto done;
            if ((e = shp_is_empty(U)) < 0) { shp_free(U); goto done; }
            if (e) { shp_free(U); break; }
        }
        sop_push(out, U, zs[j + 1], c, z, 0); prev = U;
    }
    ok = SO_OK;
done:
    free(zs); shp_free(raw); shp_free(foot3); sol_free(&t3); sol_free(&sol);
    if (ok != SO_OK) sop_free(out);
    return ok;
}

/* ================================================================== _Later */
enum { LATER_STEPS = 0, LATER_FINE = 1, LATER_READY = 2 };
struct SoLater { int rc, mode, filled; SoSolids solids; SoCut *cuts; GEOSGeometry **cutg; int ncuts; double top; SoPieces pieces; };
SoLater *so_later_ref(SoLater *l) { if (l) l->rc++; return l; }
void so_later_unref(SoLater *l) {
    if (!l || --l->rc > 0) return;
    sol_free(&l->solids); for (int i = 0; i < l->ncuts; i++) shp_free(l->cutg[i]); free(l->cutg); free(l->cuts); sop_free(&l->pieces); free(l);
}
static SoLater *later_new(int mode, const SoSolids *solids, const SoCut *cuts, int ncuts, double top) {
    SoLater *l = (SoLater *)calloc(1, sizeof(SoLater)); l->rc = 1; l->mode = mode; l->top = top;
    if (solids) sol_extend(&l->solids, solids);
    if (ncuts) {
        l->cuts = (SoCut *)malloc(sizeof(SoCut) * (size_t)ncuts); l->cutg = (GEOSGeometry **)malloc(sizeof(GEOSGeometry *) * (size_t)ncuts); l->ncuts = ncuts;
        for (int i = 0; i < ncuts; i++) { l->cutg[i] = shp_clone(cuts[i].q); l->cuts[i].q = l->cutg[i]; l->cuts[i].fl = cuts[i].fl; }
    }
    return l;
}
int so_later_filled(const SoLater *l) { return l->filled; }
/* _Later._fill: if self._make is not None: m, self._make = self._make, None; super().extend(m()) -- the maker is
   dropped BEFORE it runs, so a sculpt that raises leaves the list empty for good */
int so_later_fill(SoLater *l) {
    int n = 0, r;
    if (l->filled) return SO_OK;
    l->filled = 1;
    if (l->mode == LATER_FINE) {          /* max(8, min(60, int(top * features.true_scale() / 0.1))) */
        int64_t k;
        if (px_int(l->top * SO.true_scale / 0.1, &k) != PX_OK) return SO_ERR;
        k = k < 60 ? k : 60; n = (int)(k > 8 ? k : 8);
    }
    r = so_sculpt(&l->solids, l->cuts, l->ncuts, n, &l->pieces);
    sol_free(&l->solids);
    return r;
}
const SoPieces *so_later_pieces(SoLater *l) { if (!l->filled && so_later_fill(l) != SO_OK) return NULL; return &l->pieces; }

/* ================================================================== layers */
#define NINTERN 4096
static char *intern_tab[NINTERN]; static int nintern;
const char *so_intern(const char *s) {
    if (!s) return NULL;
    for (int i = 0; i < nintern; i++) if (!strcmp(intern_tab[i], s)) return intern_tab[i];
    if (nintern == NINTERN) return NULL;
    size_t n = strlen(s) + 1; char *c = (char *)malloc(n); memcpy(c, s, n);
    return intern_tab[nintern++] = c;
}
SoVal lv_none(void) { SoVal v; memset(&v, 0, sizeof v); v.t = LV_NONE; return v; }
SoVal lv_int(int64_t i) { SoVal v = lv_none(); v.t = LV_INT; v.i = i; v.f = (double)i; return v; }
SoVal lv_float(double f) { SoVal v = lv_none(); v.t = LV_FLOAT; v.f = f; return v; }
SoVal lv_str(const char *s) { SoVal v = lv_none(); if (s) { v.t = LV_STR; v.s = so_intern(s); } return v; }
SoVal lv_geom(GEOSGeometry *g) { SoVal v = lv_none(); v.t = LV_GEOM; v.g = g; return v; }
SoVal lv_later(SoLater *l) { SoVal v = lv_none(); v.t = LV_LATER; v.later = l; return v; }
static SoVal lv_copy(const SoVal *a) {          /* a new owner of the same value */
    SoVal v = *a;
    if (v.t == LV_GEOM) v.g = shp_clone(a->g);
    else if (v.t == LV_LATER) so_later_ref(v.later);
    return v;
}
SoLayer *ly_new(int n) { SoLayer *l = (SoLayer *)calloc(1, sizeof(SoLayer)); l->rc = 1; l->n = n; return l; }
SoLayer *ly_ref(SoLayer *l) { if (l) l->rc++; return l; }
void ly_unref(SoLayer *l) {
    if (!l || --l->rc > 0) return;
    for (int i = 0; i < l->n; i++) { if (l->v[i].t == LV_GEOM) shp_free(l->v[i].g); else if (l->v[i].t == LV_LATER) so_later_unref(l->v[i].later); }
    free(l);
}
void lyl_push(SoLayers *l, SoLayer *y) {
    if (l->n == l->cap) { l->cap = l->cap ? 2 * l->cap : 8; l->v = (SoLayer **)realloc(l->v, sizeof(*l->v) * (size_t)l->cap); }
    l->v[l->n++] = ly_ref(y);
}
void lyl_push_own(SoLayers *l, SoLayer *y) { lyl_push(l, y); ly_unref(y); }
void lyl_free(SoLayers *l) { for (int i = 0; i < l->n; i++) ly_unref(l->v[i]); free(l->v); l->v = NULL; l->n = l->cap = 0; }
SoLayer *ly_up(GEOSGeometry *g, double h, const char *kind) { SoLayer *l = ly_new(4); l->v[0] = lv_str("up"); l->v[1] = lv_geom(g); l->v[2] = lv_float(h); l->v[3] = lv_str(kind); return l; }
SoLayer *ly_on(GEOSGeometry *g, int64_t par, double h, const char *kind) { SoLayer *l = ly_new(5); l->v[0] = lv_str("on"); l->v[1] = lv_geom(g); l->v[2] = lv_int(par); l->v[3] = lv_float(h); l->v[4] = lv_str(kind); return l; }
SoLayer *ly_tcut(GEOSGeometry *g, int64_t par, double d) { SoLayer *l = ly_new(4); l->v[0] = lv_str("tcut"); l->v[1] = lv_geom(g); l->v[2] = lv_int(par); l->v[3] = lv_float(d); return l; }
SoLayer *ly_gcut(GEOSGeometry *g, double d, const char *kind) { SoLayer *l = ly_new(4); l->v[0] = lv_str("gcut"); l->v[1] = lv_geom(g); l->v[2] = lv_float(d); l->v[3] = lv_str(kind); return l; }
static SoLayer *ly_steps(GEOSGeometry *foot, SoLater *lat, const char *kind, int has_parent, int64_t parent) {
    SoLayer *l = ly_new(has_parent ? 5 : 4); l->v[0] = lv_str("steps"); l->v[1] = lv_geom(foot); l->v[2] = lv_later(lat); l->v[3] = lv_str(kind);
    if (has_parent) l->v[4] = lv_int(parent);
    return l;
}
static SoLayer *ly_up_empty(const char *kind) { GEOSGeometry *e = shp_polygon(NULL, 0); return e ? ly_up(e, 0.0, kind) : NULL; }   /* ("up", Polygon(), 0.0, kind) */

/* ================================================================== _steps, _fine */
/* scenes._steps(solids, kind, parent=None, cuts=()) */
int so_steps(const SoSolids *solids, const char *kind, int has_parent, int64_t parent, const SoCut *cuts, int ncuts, SoLayer **out) {
    SoGeoms f = { 0 }; GEOSGeometry *foot = NULL; int bad = 0, e;
    *out = NULL;
    for (int i = 0; i < solids->n && !bad; i++) {          /* try: foot = _u([g for g in (x[0](1e-6) for x in sol if x[1] > 1e-6) if g ...]) */
        GEOSGeometry *g;
        if (!(solids->v[i]->top > 1e-6)) continue;
        if (sol_sect(solids->v[i], 1e-6, &g) != SO_OK) { bad = 1; break; }
        if (g) { int ge = shp_is_empty(g); if (ge < 0) { shp_free(g); bad = 1; break; } if (ge) shp_free(g); else gpush(&f, g); }
    }
    if (bad) gfree(&f); else foot = u_list(&f);          /* except Exception: foot = None */
    if (!foot) {          /* an odd solid: the old way, all at once */
        SoPieces p = { 0 }; SoGeoms fp = { 0 }; SoLater *lat; GEOSGeometry *u;
        if (so_sculpt(solids, cuts, ncuts, 0, &p) != SO_OK) return SO_ERR;
        if (!p.n) { sop_free(&p); return (*out = ly_up_empty(kind)) ? SO_OK : SO_ERR; }
        for (int i = 0; i < p.n; i++) if (p.v[i].g) gpush(&fp, shp_clone(p.v[i].g));
        if (!(u = u_list(&fp))) { sop_free(&p); return SO_ERR; }
        lat = later_new(LATER_READY, NULL, NULL, 0, 0.0); lat->filled = 1; lat->pieces = p;
        *out = ly_steps(u, lat, kind, has_parent, parent);
        return SO_OK;
    }
    if ((e = shp_is_empty(foot)) < 0) { shp_free(foot); return SO_ERR; }
    if (e) { shp_free(foot); return (*out = ly_up_empty(kind)) ? SO_OK : SO_ERR; }
    *out = ly_steps(foot, later_new(LATER_STEPS, solids, cuts, ncuts, 0.0), kind, has_parent, parent);
    return SO_OK;
}
int sd_ST(const SoSolids *solids, const char *kind, int has_parent, int64_t parent, const SoCut *cuts, int ncuts, SoLayer **out) {
    return so_steps(solids, kind, has_parent, parent, cuts, ncuts, out);
}
/* sd_r_models._fine(sol, top, kind) */
int so_fine(const SoSolids *sol, double top, const char *kind, SoLayer **out) {
    GEOSGeometry *o; int e;
    *out = NULL;
    if (!sol->n) return SO_ERR;                         /* sol[0]: IndexError */
    if (sol_sect(sol->v[0], 0.0, &o) != SO_OK) return SO_ERR;
    if (!o) return (*out = ly_up_empty(kind)) ? SO_OK : SO_ERR;
    if ((e = shp_is_empty(o)) < 0) { shp_free(o); return SO_ERR; }
    if (e) { shp_free(o); return (*out = ly_up_empty(kind)) ? SO_OK : SO_ERR; }
    *out = ly_steps(o, later_new(LATER_FINE, sol, NULL, 0, top), kind, 0, 0);
    return SO_OK;
}

/* ================================================================== _weather, _crate_layers */
static int lv_num(const SoVal *v, double *x) { if (v->t == LV_FLOAT || v->t == LV_INT) { *x = v->f; return 1; } return 0; }
/* scenes._weather(layers, rng, chip=0.6) */
int so_weather(const SoLayers *layers, PyRandom *rng, double chip, SoLayers *out) {
    out->n = 0;
    for (int i = 0; i < layers->n; i++) lyl_push(out, layers->v[i]);          /* out = list(layers) */
    for (int i = 0; i < layers->n; i++) {
        const SoLayer *L = layers->v[i]; const GEOSGeometry *q; GEOSGeometry *q2 = NULL, *own = NULL; double h; int up, t, nc;
        if (L->n < 1 || L->v[0].t != LV_STR) continue;
        up = !strcmp(L->v[0].s, "up");
        if (!up && strcmp(L->v[0].s, "on")) continue;
        if (L->n < 2 || L->n <= (up ? 2 : 3)) goto err;          /* IndexError */
        if (!lv_num(&L->v[up ? 2 : 3], &h)) goto err;          /* h < 1.0 on a non-number: TypeError */
        if (h < 1.0) continue;
        if (L->v[1].t == LV_NONE) continue;
        if (L->v[1].t != LV_GEOM) goto err;
        q = L->v[1].g;
        if ((t = shp_type(q)) < 0) goto err;
        if (t != GEOS_POLYGON || shp_area(q) < 6) continue;
        if ((nc = shp_coords(shp_exterior(q), NULL, 0)) < 0) {          /* len(q.exterior.coords) */
            nc = G.GEOSGetNumCoordinates_r(gctx, shp_exterior(q));
        }
        if (nc > 6) continue;
        if (py_random(rng) < chip) {          /* a corner knocked off */
            double xy[16], kx, ky, rs; int64_t k; GEOSGeometry *st, *d; ShpList pl = { 0 };
            shp_coords(shp_exterior(q), xy, 8);
            k = py_choice(rng, nc - 1); kx = xy[2 * k]; ky = xy[2 * k + 1];          /* rng.choice(list(q.exterior.coords)[:-1]) */
            rs = py_uniform(rng, 0.7, 1.2);
            if (!(st = so_stone(kx, ky, rs, rng, 0.8, 5))) goto err;
            d = shp_difference(q, st); shp_free(st);
            if (!d) goto err;
            shp_polys_of(d, &pl); shp_free(d);
            if (pl.n) {          /* max(polys_of(...), key=lambda g: g.area, default=q) */
                int bi = 0; double ba = shp_area(pl.g[0]);
                for (int j = 1; j < pl.n; j++) { double aj = shp_area(pl.g[j]); if (aj > ba) { ba = aj; bi = j; } }
                q2 = pl.g[bi]; pl.g[bi] = NULL;
            }
            shp_list_free(&pl);
            if (q2 && shp_area(q2) > 0.7 * shp_area(q)) {          /* out[i] = L[:1] + (q2,) + tuple(L[2:]); q = q2 */
                SoLayer *n2 = ly_new(L->n);
                n2->v[0] = lv_copy(&L->v[0]); n2->v[1] = lv_geom(shp_clone(q2));
                for (int j = 2; j < L->n; j++) n2->v[j] = lv_copy(&L->v[j]);
                ly_unref(out->v[i]); out->v[i] = n2;
                own = q2; q2 = NULL; q = own;
            }
            shp_free(q2); q2 = NULL;
        }
        {
            GEOSGeometry *cc = shp_centroid(q), *iq = NULL; double ccx, ccy, r; int64_t cnt;
            if (!cc) goto err2;
            ccx = shp_x(cc); ccy = shp_y(cc); shp_free(cc);
            r = py_sqrt_u(shp_area(q)) * 0.45;
            cnt = py_randint(rng, 0, 2);
            for (int64_t it = 0; it < cnt; it++) {
                double x = ccx + py_uniform(rng, -r, r), y = ccy + py_uniform(rng, -r, r), rad = r * py_uniform(rng, 0.7, 1.1);
                int nv = (int)py_randint(rng, 4, 6), w;
                GEOSGeometry *st = so_stone(x, y, rad, rng, 0.8, nv), *piece;
                if (!st) goto err2;
                if (!iq && !(iq = shp_inset(q, 0.15))) { shp_free(st); goto err2; }          /* inset(q, 0.15): the same every time */
                piece = shp_intersection(st, iq); shp_free(st);
                if (!piece) { shp_free(iq); goto err2; }
                if (shp_type(piece) == GEOS_POLYGON && shp_area(piece) > 1.3) {
                    if ((w = shp_wide_enough(piece, SO.minw, SO.ts)) < 0) { shp_free(piece); shp_free(iq); goto err2; }
                    if (w) {
                        SoLayer *on = ly_new(5);
                        on->v[0] = lv_str("on"); on->v[1] = lv_geom(piece); piece = NULL; on->v[2] = lv_int(i); on->v[3] = lv_float(py_uniform(rng, 0.5, 1.4));
                        on->v[4] = lv_copy(&L->v[L->n - 1]);
                        lyl_push_own(out, on);
                    }
                }
                shp_free(piece);
            }
            shp_free(iq);
        }
        shp_free(own);
        continue;
err2:
        shp_free(own);
        goto err;
    }
    return SO_OK;
err:
    lyl_free(out); return SO_ERR;
}

/* scenes._crate_layers(g, ff, L, W, a, h, kind, rng, parent=None, n0=0) */
int so_crate_layers(const GEOSGeometry *g, const SoFrame *ff, double L, double W, double a, double h, const char *kind, PyRandom *rng, int has_parent, int64_t parent, int64_t n0, SoLayers *out) {
    GEOSGeometry *brace, *i1 = NULL, *d = NULL, *x = NULL, *proud = NULL, *seams = NULL, *i2 = NULL, *bb = NULL, *t = NULL; int e; (void)a;
    if (py_random(rng) < 0.5) brace = bar_ff(ff, -L / 2 + 0.6, -W / 2 + 0.6, L / 2 - 0.6, W / 2 - 0.6, 0.92);
    else brace = bar_ff(ff, -L / 2 + 0.6, W / 2 - 0.6, L / 2 - 0.6, -W / 2 + 0.6, 0.92);
    if (!brace) return SO_ERR;
    if (!(i1 = shp_inset(g, 0.92)) || !(d = shp_difference(g, i1)) || !(x = shp_intersection(brace, g))) goto err;
    { GEOSGeometry *two[2] = { d, x }; if (!(proud = so_u((const GEOSGeometry *const *)two, 2))) goto err; }
    {          /* _u([_bar(ff(-L, v), ff(L, v), 0.22) for v in np.arange(-W / 2 + 1.45, W / 2 - 1.2, 0.62)]) */
        int64_t nv = np_arange(-W / 2 + 1.45, W / 2 - 1.2, 0.62, NULL, 0); SoGeoms bars = { 0 }; double *vs;
        if (nv < 0) goto err;
        vs = (double *)malloc(sizeof(double) * (size_t)(nv ? nv : 1)); np_arange(-W / 2 + 1.45, W / 2 - 1.2, 0.62, vs, nv);
        for (int64_t k = 0; k < nv; k++) { GEOSGeometry *b = bar_ff(ff, -L, vs[k], L, vs[k], 0.22); if (!b) { free(vs); gfree(&bars); goto err; } gpush(&bars, b); }
        free(vs);
        if (!(t = u_list(&bars))) goto err;
    }
    if (!(i2 = shp_inset(g, 1.05)) || !(seams = shp_intersection(t, i2)) || !(bb = shp_buffer_q(brace, 0.15, 16))) goto err;
    { GEOSGeometry *s2 = shp_difference(seams, bb); shp_free(seams); seams = s2; if (!seams) goto err; }
    if ((e = shp_is_empty(seams)) < 0) goto err;
    lyl_push_own(out, has_parent ? ly_on(shp_clone(g), parent, h, kind) : ly_up(shp_clone(g), h, kind));
    lyl_push_own(out, ly_on(proud, n0, 0.3, kind)); proud = NULL;
    if (!e) { lyl_push_own(out, ly_tcut(seams, n0, 0.18)); seams = NULL; }
    shp_free(brace); shp_free(i1); shp_free(d); shp_free(x); shp_free(t); shp_free(i2); shp_free(bb); shp_free(seams);
    return SO_OK;
err:
    shp_free(brace); shp_free(i1); shp_free(d); shp_free(x); shp_free(proud); shp_free(seams); shp_free(i2); shp_free(bb); shp_free(t);
    return SO_ERR;
}

/* ================================================================== style_details tools */
/* line(pts, w): LineString(pts).buffer(w / 2, 6) */
GEOSGeometry *sd_line(const double *pts, int n, double w) { return shp_line_buffer(pts, n, w / 2, SHP_CAP_ROUND, SHP_JOIN_ROUND, 6, 5.0); }
/* ell(x, y, rx, ry, a=0.0, n=20): rotate(scale(Point(x, y).buffer(1, n), rx, ry), a, origin=(x, y), use_radians=True) */
GEOSGeometry *sd_ell(double x, double y, double rx, double ry, double a, int n) {
    GEOSGeometry *d = shp_point_buffer(x, y, 1, n), *s = d ? shp_scale(d, rx, ry, SHP_ORIGIN_CENTER, 0, 0) : NULL, *r = s ? shp_rotate(s, a, SHP_ORIGIN_POINT, x, y, 1) : NULL;
    shp_free(d); shp_free(s);
    return r;
}
/* hull(gs): U(gs).convex_hull */
GEOSGeometry *sd_hull(const GEOSGeometry *const *gs, int n) { GEOSGeometry *u = sd_U(gs, n), *h = u ? shp_convex_hull(u) : NULL; shp_free(u); return h; }
/* ngon(x, y, r, n, a=0.0): Polygon([(x + r * math.cos(a + 6.2832 * k / n), ...) for k in range(n)]) */
GEOSGeometry *sd_ngon(double x, double y, double r, int n, double a) {
    double *xy; GEOSGeometry *g;
    if (n <= 0) return shp_polygon(NULL, 0);          /* range(n) empty: Polygon([]) */
    xy = (double *)malloc(sizeof(double) * 2 * (size_t)n);
    for (int k = 0; k < n; k++) { double t = a + 6.2832 * (double)k / (double)n; xy[2 * k] = x + r * py_cos(t); xy[2 * k + 1] = y + r * py_sin(t); }
    g = shp_polygon(xy, n); free(xy);
    return g;
}
/* arc(x, y, r, a0, a1, n=12) */
int sd_arc(double x, double y, double r, double a0, double a1, int n, double *xy) {
    if (n == 0) return SO_ERR;
    for (int k = 0; k <= n; k++) { double t = a0 + (a1 - a0) * (double)k / (double)n; xy[2 * k] = x + r * py_cos(t); xy[2 * k + 1] = y + r * py_sin(t); }
    return SO_OK;
}
/* spiral(x, y, r0, r1, turns, a=0.0, n=40) */
int sd_spiral(double x, double y, double r0, double r1, double turns, double a, int n, double *xy) {
    if (n == 0) return SO_ERR;
    for (int k = 0; k <= n; k++) {
        double rr = r0 + (r1 - r0) * (double)k / (double)n, t = a + 6.2832 * turns * (double)k / (double)n;
        xy[2 * k] = x + rr * py_cos(t); xy[2 * k + 1] = y + rr * py_sin(t);
    }
    return SO_OK;
}
/* pol(x, y, r, t) */
void sd_pol(double x, double y, double r, double t, double *xy) { xy[0] = x + r * py_cos(t); xy[1] = y + r * py_sin(t); }
/* ringpts(x, y, r, n, a=0.0, jit=0.0, rng=None) */
int sd_ringpts(double x, double y, double r, int n, double a, double jit, PyRandom *rng, double *xy) {
    for (int k = 0; k < n; k++) {
        double t = a + 6.2832 * (double)k / (double)n + (rng ? py_uniform(rng, -jit, jit) : 0.0);
        sd_pol(x, y, r, t, &xy[2 * k]);
    }
    return SO_OK;
}
/* gg.wiggle(a, b, rng, amp=0.35, n=3) (TODO-leaf: G2 owns it): the points into xy (n + 1 of them) */
static int wiggle(const double *a, const double *b, PyRandom *rng, double amp, int n, double *xy) {
    double x0 = a[0], y0 = a[1], x1 = b[0], y1 = b[1], L = px_dist(a, b, 2), nx, ny; int k = 0;
    if (L == 0.0) L = 1;          /* math.dist(a, b) or 1 */
    nx = -(y1 - y0) / L; ny = (x1 - x0) / L;
    xy[k++] = x0; xy[k++] = y0;
    for (int i = 1; i < n; i++) {
        double t = (double)i / (double)n, o = py_uniform(rng, -amp, amp);
        xy[k++] = x0 + (x1 - x0) * t + nx * o; xy[k++] = y0 + (y1 - y0) * t + ny * o;
    }
    xy[k++] = x1; xy[k++] = y1;
    return k / 2;
}
/* wavy(p, q, rng, amp=0.5, n=5, w=0.5): LineString(wiggle(p, q, rng, amp, n)).buffer(w / 2, 6) */
GEOSGeometry *sd_wavy(const double *p, const double *q, PyRandom *rng, double amp, int n, double w) {
    double *xy = (double *)malloc(sizeof(double) * 2 * (size_t)(n > 1 ? n + 1 : 2)); int m = wiggle(p, q, rng, amp, n, xy);
    GEOSGeometry *g = sd_line(xy, m, w); free(xy);
    return g;
}
/* tri(p, q, w): a pointed blade */
GEOSGeometry *sd_tri(const double *p, const double *q, double w) {
    double L = px_dist(p, q, 2), nx, ny, xy[6];
    if (L == 0.0) L = 1;
    nx = -(q[1] - p[1]) / L * w / 2; ny = (q[0] - p[0]) / L * w / 2;
    xy[0] = p[0] + nx; xy[1] = p[1] + ny; xy[2] = q[0]; xy[3] = q[1]; xy[4] = p[0] - nx; xy[5] = p[1] - ny;
    return shp_polygon(xy, 3);
}
/* rrect(x, y, L, W, a, r=0.3): rect(...).buffer(-r, join_style=2).buffer(r, 6) */
GEOSGeometry *sd_rrect(double x, double y, double L, double W, double a, double r) {
    GEOSGeometry *g = shp_rect(x, y, L, W, a), *b = g ? shp_buffer(g, -r, 16, SHP_CAP_ROUND, SHP_JOIN_MITRE, 5.0, 0) : NULL, *c = b ? shp_buffer_q(b, r, 6) : NULL;
    shp_free(g); shp_free(b);
    return c;
}
/* cyl(x, y, r, h, lift=0.0, n=20): box(disc(x, y, r, n), h, lift) */
int sd_cyl(SoSolids *out, double x, double y, double r, double h, double lift, int n) {
    GEOSGeometry *d = sd_disc(x, y, r, n); int k;
    if (!d) return SO_ERR;
    k = so_box(out, d, h, lift); shp_free(d); return k;
}
/* blk(x, y, L, W, a, h, lift=0.0): box(rect(x, y, L, W, a), h, lift) */
int sd_blk(SoSolids *out, double x, double y, double L, double W, double a, double h, double lift) {
    GEOSGeometry *d = shp_rect(x, y, L, W, a); int k;
    if (!d) return SO_ERR;
    k = so_box(out, d, h, lift); shp_free(d); return k;
}
int sd_ext(SoSolids *out, const GEOSGeometry *poly, double h, double lift) { return so_box(out, poly, h, lift); }
/* dome(x, y, r, h, lift=0.0, sq=1.0, a=0.0, n=20): lathe(x, y, curve(r, 0.22, 0.0, h, 7), n, sq, a, lift=lift) */
int sd_dome(SoSolids *out, double x, double y, double r, double h, double lift, double sq, double a, int n) {
    double prof[16]; so_curve(r, 0.22, 0.0, h, 7, 1, prof);
    return so_lathe(out, x, y, prof, 8, n, sq, a, lift, NULL, 0);
}
/* cone(x, y, r, h, tip=0.24, lift=0.0, n=16, sq=1.0, a=0.0): lathe(x, y, [(r, 0.0), (tip, h)], n, sq, a, lift=lift) */
int sd_cone(SoSolids *out, double x, double y, double r, double h, double tip, double lift, int n, double sq, double a) {
    double prof[4] = { r, 0.0, tip, h };
    return so_lathe(out, x, y, prof, 2, n, sq, a, lift, NULL, 0);
}
/* turn(x, y, prof, n=24, sq=1.0, a=0.0, lift=0.0) */
int sd_turn(SoSolids *out, double x, double y, const double *prof, int np, int n, double sq, double a, double lift) { return so_lathe(out, x, y, prof, np, n, sq, a, lift, NULL, 0); }
/* lie(x, y, a, L, r, lift=0.0): vault(x, y, a, L, 2 * r, r * 1.15, lift=lift, under=True) */
int sd_lie(SoSolids *out, double x, double y, double a, double L, double r, double lift) { return so_vault(out, x, y, a, L, 2 * r, r * 1.15, "round", lift, 1, 0, 0.0); }
int sd_ridge(SoSolids *out, double x, double y, double a, double L, double W, double h, double lift) { return so_vault(out, x, y, a, L, W, h, "ridge", lift, 0, 0, 0.0); }
int sd_loaf(SoSolids *out, double x, double y, double a, double L, double W, double h, double lift) { return so_vault(out, x, y, a, L, W, h, "loaf", lift, 0, 0, 0.0); }
/* rod(pts, r, hk=1.0, lift=0.0, arch=0.0): tube(pts, r, hk, lift=lift, arch=arch) */
int sd_rod(SoSolids *out, const double *pts, int npts, double r, SoNumFn rfun, void *ctx, double hk, double lift, double arch) { return so_tube(out, pts, npts, r, rfun, ctx, hk, lift, 0, arch); }
/* spin(x, y, a, L, rf, hk=1.2, lift=0.0, prof="round"): loft(x, y, a, L, rf, hk, prof, lift=lift) */
int sd_spin(SoSolids *out, double x, double y, double a, double L, SoNumFn rf, void *ctx, double hk, double lift, const char *prof) { return so_loft(out, x, y, a, L, rf, ctx, hk, prof, lift, 0); }
/* crv(r0, r1, z0, z1, out=True, n=6): curve(r0, r1, z0, z1, n, out) */
int sd_crv(double r0, double r1, double z0, double z1, int out_, int n, double *prof) { return so_curve(r0, r1, z0, z1, n, out_, prof); }
/* pot(x, y, r, h, neck=0.5, lip=0.66, lift=0.0, n=24) */
int sd_pot(SoSolids *out, double x, double y, double r, double h, double neck, double lip, double lift, int n) {
    double p[24] = { r * 0.62, 0.05, r * 0.62, h * 0.08, r, h * 0.42 };
    so_curve(r, r * neck, h * 0.42, h * 0.84, 6, 1, p + 6);
    p[20] = r * lip; p[21] = h * 0.9; p[22] = r * lip; p[23] = h;
    return so_lathe(out, x, y, p, 12, n, 1.0, 0.0, lift, NULL, 0);
}
/* rock(x, y, r, h, rng, sq=None, n=9, lift=0.0) */
int sd_rock(SoSolids *out, double x, double y, double r, double h, PyRandom *rng, double sq, double lift, int n) {
    double p[8] = { r, 0.0, r * 0.92, h * 0.45, r * 0.6, h * 0.85, 0.25, h };
    double s = sq != 0.0 ? sq : py_uniform(rng, 0.7, 0.95), a = py_uniform(rng, 0, 3.14);          /* sq or rng.uniform(0.7, 0.95), rng.uniform(0, 3.14) */
    return so_lathe(out, x, y, p, 4, n, s, a, lift, NULL, 0);
}
/* fluted(x, y, r, h, n=8, a=0.0, deep=0.8, m=6, twist=0.0, lift=0.0) */
int sd_fluted(SoSolids *out, double x, double y, double r, double h, int n, double a, double deep, int m, double twist, double lift) {
    for (int k = 0; k < m; k++) {
        double ro = r * py_cos(1.45 * (double)k / (double)m), ri = r * py_cos(1.45 * (double)k / (double)m) * deep;
        GEOSGeometry *s = shp_star(x, y, ro, ri, n, a + twist * (double)k); int e;
        if (!s) return SO_ERR;
        e = so_box(out, s, h * py_sin(1.45 * (double)(k + 1) / (double)m) / py_sin(1.45), lift); shp_free(s);
        if (e != SO_OK) return e;
    }
    return SO_OK;
}
int sd_hang(SoSolids *out, const GEOSGeometry *poly, double z0, double z1) { return so_slab(out, poly, z0, z1); }

/* ================================================================== style_details.Obj */
void so_obj_init(SoObj *o, const char *kind) { memset(o, 0, sizeof *o); o->k = so_intern(kind); }
void so_obj_free(SoObj *o) { lyl_free(&o->L); gfree(&o->F); }
static const char *okind(const SoObj *o, const char *kind) { return kind && *kind ? kind : o->k; }          /* kind or s.k */
/* up(g, h, kind=None): s.L.append(("up", g, h, kind or s.k)); s.F.append(g) */
int so_obj_up(SoObj *o, const GEOSGeometry *g, double h, const char *kind) {
    GEOSGeometry *a = shp_clone(g), *b = shp_clone(g);
    if (!a || !b) { shp_free(a); shp_free(b); return SO_ERR; }
    lyl_push_own(&o->L, ly_up(a, h, okind(o, kind))); gpush(&o->F, b);
    return o->L.n - 1;
}
/* on(g, par, h, kind=None): s.L.append(ST(ext(g, h), kind or s.k, par)) */
int so_obj_on(SoObj *o, const GEOSGeometry *g, int has_par, int64_t par, double h, const char *kind) {
    SoSolids s = { 0 }; SoLayer *l; int r;
    so_box(&s, g, h, 0.0);
    r = so_steps(&s, okind(o, kind), has_par, par, NULL, 0, &l); sol_free(&s);
    if (r != SO_OK) return SO_ERR;
    lyl_push_own(&o->L, l);
    return o->L.n - 1;
}
/* cut(g, par, d=0.18): if g is not None and not g.is_empty: s.L.append(("tcut", g, par, d)) */
int so_obj_cut(SoObj *o, const GEOSGeometry *g, int has_par, int64_t par, double d) {
    int e; SoLayer *l;
    if (!g) return SO_OK;
    if ((e = shp_is_empty(g)) < 0) return SO_ERR;
    if (e) return SO_OK;
    l = ly_new(4); l->v[0] = lv_str("tcut"); l->v[1] = lv_geom(shp_clone(g)); l->v[2] = has_par ? lv_int(par) : lv_none(); l->v[3] = lv_float(d);
    lyl_push_own(&o->L, l);
    return SO_OK;
}
/* st(solids, kind=None, par=None, cuts=(), foot=None) */
int so_obj_st(SoObj *o, const SoSolids *solids, const char *kind, int has_par, int64_t par, const SoCut *cuts, int ncuts, const GEOSGeometry *foot) {
    SoLayer *l;
    if (so_steps(solids, okind(o, kind), has_par, par, cuts, ncuts, &l) != SO_OK) return SO_ERR;
    lyl_push_own(&o->L, l);
    if (!has_par) gpush(&o->F, shp_clone(foot ? foot : l->v[1].g));          /* s.F.append(foot if foot is not None else s.L[-1][1]) */
    return o->L.n - 1;
}
/* gcut(g, d, kind): s.L.append(("gcut", g, d, kind)); s.F.append(g) */
int so_obj_gcut(SoObj *o, const GEOSGeometry *g, double d, const char *kind) {
    lyl_push_own(&o->L, ly_gcut(shp_clone(g), d, kind)); gpush(&o->F, shp_clone(g));
    return SO_OK;
}
/* out(extra=()): hull(s.F + list(extra)), s.L */
int so_obj_out(SoObj *o, const GEOSGeometry *const *extra, int nextra, GEOSGeometry **hull) {
    const GEOSGeometry **v = (const GEOSGeometry **)malloc(sizeof(*v) * (size_t)(o->F.n + nextra + 1));
    for (int i = 0; i < o->F.n; i++) v[i] = o->F.v[i];
    for (int i = 0; i < nextra; i++) v[o->F.n + i] = extra[i];
    *hull = sd_hull(v, o->F.n + nextra); free(v);
    return *hull ? SO_OK : SO_ERR;
}
/* lid(g, par, h=0.16, edge=0.16, kind=None): s.on(inset(g, edge), par, h, kind) */
int so_obj_lid(SoObj *o, const GEOSGeometry *g, int has_par, int64_t par, double h, double edge, const char *kind) {
    GEOSGeometry *i = shp_inset(g, edge); int r;
    if (!i) return SO_ERR;
    r = so_obj_on(o, i, has_par, par, h, kind); shp_free(i); return r;
}
/* groove(g, par, w=0.2, d=0.16): s.cut(g.exterior.buffer(w / 2, 2), par, d) */
int so_obj_groove(SoObj *o, const GEOSGeometry *g, int has_par, int64_t par, double w, double d) {
    GEOSGeometry *b; int r;
    if (shp_type(g) != GEOS_POLYGON) return SO_ERR;          /* .exterior: AttributeError on anything else */
    if (!(b = shp_buffer_q(shp_exterior(g), w / 2, 2))) return SO_ERR;
    r = so_obj_cut(o, b, has_par, par, d); shp_free(b); return r;
}
/* seams(ff, L, W, par, step, across=False, w=0.2, d=0.16, skip=None) */
int so_obj_seams(SoObj *o, const SoFrame *ff, double L, double W, int has_par, int64_t par, double step, int across, double w, double d, const GEOSGeometry *skip) {
    double S = across ? L : W; int64_t nv = np_arange(-S / 2 + step, S / 2 - step * 0.5, step, NULL, 0); double *vs; SoGeoms gs = { 0 }; GEOSGeometry *g; int r;
    if (nv < 0) return SO_ERR;
    vs = (double *)malloc(sizeof(double) * (size_t)(nv ? nv : 1)); np_arange(-S / 2 + step, S / 2 - step * 0.5, step, vs, nv);
    for (int64_t k = 0; k < nv; k++) {
        GEOSGeometry *b = across ? bar_ff(ff, vs[k], -W, vs[k], W, w) : bar_ff(ff, -L, vs[k], L, vs[k], w);
        if (!b) { free(vs); gfree(&gs); return SO_ERR; }
        gpush(&gs, b);
    }
    free(vs);
    if (!(g = U_list(&gs))) return SO_ERR;
    if (skip) { GEOSGeometry *x = shp_difference(g, skip); shp_free(g); if (!(g = x)) return SO_ERR; }
    r = so_obj_cut(o, g, has_par, par, d); shp_free(g); return r;
}
static int on_list(SoObj *o, SoGeoms *gs, int has_par, int64_t par, double h, const char *kind) {          /* s.on(U(gs), par, h, kind) */
    GEOSGeometry *g = U_list(gs); int r;
    if (!g) return SO_ERR;
    r = so_obj_on(o, g, has_par, par, h, kind); shp_free(g); return r;
}
/* studs(pts, par, r=0.27, h=0.15, kind=None, n=8) */
int so_obj_studs(SoObj *o, const double *pts, int npts, int has_par, int64_t par, double r, double h, const char *kind, int n) {
    SoGeoms gs = { 0 };
    for (int i = 0; i < npts; i++) { GEOSGeometry *g = sd_disc(pts[2 * i], pts[2 * i + 1], r, n); if (!g) { gfree(&gs); return SO_ERR; } gpush(&gs, g); }
    return on_list(o, &gs, has_par, par, h, kind);
}
/* handle(p, q, par, w=0.46, h=0.42, kind=None) */
int so_obj_handle(SoObj *o, const double *p, const double *q, int has_par, int64_t par, double w, double h, const char *kind) {
    SoGeoms gs = { 0 }; GEOSGeometry *a = sd_disc(p[0], p[1], w * 0.75, 8), *b = a ? sd_disc(q[0], q[1], w * 0.75, 8) : NULL, *c; int r;
    if (!b) { shp_free(a); return SO_ERR; }
    gpush(&gs, a); gpush(&gs, b);
    if (on_list(o, &gs, has_par, par, h * 0.55, kind) < 0) return SO_ERR;
    if (!(c = sd_capl(p, q, w))) return SO_ERR;
    r = so_obj_on(o, c, has_par, par, h, kind); shp_free(c); return r;
}
/* tabs(ff, xs, v, par, a, l=0.5, w=0.62, h=0.17, kind=None) */
int so_obj_tabs(SoObj *o, const SoFrame *ff, const double *xs, int nx, double v, int has_par, int64_t par, double a, double l, double w, double h, const char *kind) {
    SoGeoms gs = { 0 };
    for (int i = 0; i < nx; i++) { double px, py; so_ff(ff, xs[i], v, &px, &py); GEOSGeometry *g = shp_rect(px, py, l, w, a); if (!g) { gfree(&gs); return SO_ERR; } gpush(&gs, g); }
    return on_list(o, &gs, has_par, par, h, kind);
}
/* ribs(ff, L, W, par, n, a, w=0.46, h=0.15, across=True, kind=None) */
int so_obj_ribs(SoObj *o, const SoFrame *ff, double L, double W, int has_par, int64_t par, int n, double a, double w, double h, int across, const char *kind) {
    SoGeoms gs = { 0 };
    for (int i = 0; i < n; i++) {
        double px, py; GEOSGeometry *g;
        if (across) { so_ff(ff, ((double)i + 0.5) / n * L - L / 2, 0, &px, &py); g = shp_rect(px, py, w, W, a); }
        else { so_ff(ff, 0, ((double)i + 0.5) / n * W - W / 2, &px, &py); g = shp_rect(px, py, L, w, a); }
        if (!g) { gfree(&gs); return SO_ERR; }
        gpush(&gs, g);
    }
    return on_list(o, &gs, has_par, par, h, kind);
}
/* vents(ff, x, v, par, n, l=1.0, a=0.0, step=0.42, w=0.2, d=0.22) */
int so_obj_vents(SoObj *o, const SoFrame *ff, double x, double v, int has_par, int64_t par, int n, double l, double a, double step, double w, double d) {
    SoGeoms gs = { 0 }; GEOSGeometry *g; int r; (void)a;
    for (int i = 0; i < n; i++) {
        double vv = v + ((double)i - (double)(n - 1) / 2) * step; GEOSGeometry *b = bar_ff(ff, x - l / 2, vv, x + l / 2, vv, w);
        if (!b) { gfree(&gs); return SO_ERR; }
        gpush(&gs, b);
    }
    if (!(g = U_list(&gs))) return SO_ERR;
    r = so_obj_cut(o, g, has_par, par, d); shp_free(g); return r;
}
/* corners(ff, L, W, par, a, size=0.6, h=0.14, kind=None) */
int so_obj_corners(SoObj *o, const SoFrame *ff, double L, double W, int has_par, int64_t par, double a, double size, double h, const char *kind) {
    SoGeoms gs = { 0 };
    for (int sx = -1; sx <= 1; sx += 2) for (int sy = -1; sy <= 1; sy += 2) {
        double px, py; so_ff(ff, (double)sx * (L / 2 - size / 2), (double)sy * (W / 2 - size / 2), &px, &py);
        GEOSGeometry *g = shp_rect(px, py, size, size, a);
        if (!g) { gfree(&gs); return SO_ERR; }
        gpush(&gs, g);
    }
    return on_list(o, &gs, has_par, par, h, kind);
}
/* straps(ff, xs, W, par, a, w=0.5, h=0.13, kind=None) */
int so_obj_straps(SoObj *o, const SoFrame *ff, const double *xs, int nx, double W, int has_par, int64_t par, double a, double w, double h, const char *kind) {
    SoGeoms gs = { 0 };
    for (int i = 0; i < nx; i++) { double px, py; so_ff(ff, xs[i], 0, &px, &py); GEOSGeometry *g = shp_rect(px, py, w, W + 1.0, a); if (!g) { gfree(&gs); return SO_ERR; } gpush(&gs, g); }
    return on_list(o, &gs, has_par, par, h, kind);
}
