/* leaf.c: the leaf geometry of the scene code (see leaf.h). Every function names the Python routine it copies; the
   arithmetic is the Python's, operation by operation (built with -ffp-contract=off, no fast-math), the shapely calls
   are cport\shp.c (shapely 2.2.0's own GEOS calls), the random numbers cport\pyrandom.h, the maths cport\pyx.h. */
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "shp.h"
#include "pyx.h"
#include "pyexact.h"
#include "leaf.h"

#define PI 3.141592653589793          /* math.pi */

/* ======================= Python-exact helpers: cport\pyexact.h (G1) ======================= */
/* float ** float; *err = 1 where Python raises or gives a complex number */
double lf_fpow(double x, double y, int *err) { double re = 0, im = 0; int r = px_pow(x, y, &re, &im); if (r != 0) *err = 1; return re; }
int lf_linspace(double start, double stop, int num, double *out) { return np_linspace(start, stop, num, 1, out) < 0 ? 0 : num; }
int lf_arange(double start, double stop, double step, double *out, int cap) {
    int64_t n = np_arange(start, stop, step, NULL, 0);
    if (n < 0 || n > cap) return -1;
    return (int)np_arange(start, stop, step, out, cap);
}
double lf_fsum(const double *x, int n) { return px_sum(x, n); }

/* ======================= gg.py ======================= */

/* gg.polys_of */
static void polys_of_g(const GEOSGeometry *g, GeoList *out) {
    int t, i, n;
    if (!g || G.GEOSisEmpty_r(gctx, g)) return;
    t = G.GEOSGeomTypeId_r(gctx, g);
    if (t == GEOS_POLYGON) { gl_push_own(out, geo_new(shp_clone(g))); return; }
    if (t >= GEOS_MULTIPOINT) for (i = 0, n = G.GEOSGetNumGeometries_r(gctx, g); i < n; i++) polys_of_g(G.GEOSGetGeometryN_r(gctx, g, i), out);
}
void lf_polys_of(Geo *g, GeoList *out) {
    if (!g || G.GEOSisEmpty_r(gctx, g->g)) return;
    if (G.GEOSGeomTypeId_r(gctx, g->g) == GEOS_POLYGON) { gl_push(out, g); return; }          /* [g]: the object itself */
    polys_of_g(g->g, out);
}
void lf_polys_of_new(GEOSGeometry *g, GeoList *out) {
    Geo *h = geo_new(g);
    if (!h) return;
    lf_polys_of(h, out);
    geo_unref(h);
}

Geo *lf_inset(const Geo *p, double d) { return geo_new(shp_inset(p->g, d)); }
int lf_wide_enough(const Geo *p, double w) { int r = shp_wide_enough(p->g, w, SC.ts); return r < 0 ? SC_ERR : r; }

Frame lf_frame(double cx, double cy, double a) { Frame f; f.cx = cx; f.cy = cy; f.c = py_cos(a); f.s = py_sin(a); return f; }

static int ext_count(const GEOSGeometry *p) {          /* len(p.exterior.coords) */
    const GEOSGeometry *r = G.GEOSGetExteriorRing_r(gctx, p);
    return r ? G.GEOSGetNumCoordinates_r(gctx, r) : -1;
}
/* gg.edges_survive: q = inset(p, d); not empty, a Polygon, as many outline points as p */
int lf_edges_survive(const GEOSGeometry *p, double d) {
    GEOSGeometry *q = shp_inset(p, d); int r;
    if (!q) return SC_ERR;
    r = !G.GEOSisEmpty_r(gctx, q) && G.GEOSGeomTypeId_r(gctx, q) == GEOS_POLYGON && ext_count(q) == ext_count(p);
    shp_free(q);
    return r;
}

/* gg.fit_for_taper: Polygon(p.exterior).simplify(0.05); up to 400 times: if edges_survive -> it; else drop the point
   after the shortest edge (np.argmin: the first shortest) */
Geo *lf_fit_for_taper(const Geo *pin, double d, int *err) {
    GEOSGeometry *p, *q; double *c = NULL; int it, n, i, k;
    *err = 0;
    if (!pin || G.GEOSisEmpty_r(gctx, pin->g) || G.GEOSGeomTypeId_r(gctx, pin->g) != GEOS_POLYGON) return NULL;
    q = shp_polygon_from_ring(shp_exterior(pin->g));
    if (!q) { *err = 1; return NULL; }
    p = shp_simplify(q, 0.05, 1); shp_free(q);
    if (!p) { *err = 1; return NULL; }
    for (it = 0; it < 400; it++) {
        int ok = lf_edges_survive(p, d), best = 0; double bl = 0;
        if (ok < 0) { *err = 1; break; }
        if (ok) { free(c); return geo_new(p); }
        n = ext_count(p);          /* list(p.exterior.coords)[:-1] */
        if (n < 0) { *err = 1; break; }
        c = (double *)realloc(c, sizeof(double) * 2 * (size_t)(n > 0 ? n : 1));
        n = shp_coords(shp_exterior(p), c, n);
        if (n < 0) { *err = 1; break; }
        n = n > 0 ? n - 1 : 0;
        if (n <= 4) break;
        for (i = 0; i < n; i++) {
            int j = (i + 1) % n;
            double el = py_hypot(c[2 * i] - c[2 * j], c[2 * i + 1] - c[2 * j + 1]);          /* math.dist */
            if (i == 0 || el < bl) { bl = el; best = i; }
        }
        k = (best + 1) % n;          /* del c[k] */
        memmove(c + 2 * k, c + 2 * k + 2, sizeof(double) * 2 * (size_t)(n - k - 1));
        shp_free(p);
        p = shp_polygon(c, n - 1);
        if (!p) { *err = 1; free(c); return NULL; }
        if (!shp_is_valid(p)) break;
    }
    shp_free(p); free(c);
    return NULL;
}

/* gg._hull: P = sorted(set(pts)); lo, up = half(P), half(reversed(P)); lo[:-1] + up[:-1] (P itself when < 3) */
int lf_hull(const double *xy, int n, double *out) {
    double *P = (double *)malloc(sizeof(double) * 2 * (size_t)(n + 1)), *h = (double *)malloc(sizeof(double) * 2 * (size_t)(n + 1));
    int m = 0, i, j, k, nh = 0, pass;
    for (i = 0; i < n; i++) {          /* set(): the first of equal points stays */
        for (j = 0; j < m; j++) if (P[2 * j] == xy[2 * i] && P[2 * j + 1] == xy[2 * i + 1]) break;
        if (j == m) { P[2 * m] = xy[2 * i]; P[2 * m + 1] = xy[2 * i + 1]; m++; }
    }
    for (i = 1; i < m; i++) {          /* sorted(): tuple order, stable insertion sort */
        double x = P[2 * i], y = P[2 * i + 1];
        for (j = i - 1; j >= 0 && (P[2 * j] > x || (P[2 * j] == x && P[2 * j + 1] > y)); j--) { P[2 * j + 2] = P[2 * j]; P[2 * j + 3] = P[2 * j + 1]; }
        P[2 * j + 2] = x; P[2 * j + 3] = y;
    }
    if (m < 3) { memcpy(out, P, sizeof(double) * 2 * (size_t)m); free(P); free(h); return m; }
    for (pass = 0; pass < 2; pass++) {
        k = 0;
        for (int t = 0; t < m; t++) {
            int q = pass ? m - 1 - t : t; double px = P[2 * q], py = P[2 * q + 1];
            while (k >= 2 && (h[2 * k - 2] - h[2 * k - 4]) * (py - h[2 * k - 3]) - (h[2 * k - 1] - h[2 * k - 3]) * (px - h[2 * k - 4]) <= 0) k--;
            h[2 * k] = px; h[2 * k + 1] = py; k++;
        }
        for (i = 0; i < k - 1; i++) { out[2 * nh] = h[2 * i]; out[2 * nh + 1] = h[2 * i + 1]; nh++; }
    }
    free(P); free(h);
    return nh;
}

static double CSX[40], CSY[40]; static int have_csx = 0;
void lf_set_csx(const double *csx, const double *csy) { memcpy(CSX, csx, sizeof CSX); memcpy(CSY, csy, sizeof CSY); have_csx = 1; }

static int cmp_d(const void *a, const void *b) { double x = *(const double *)a, y = *(const double *)b; return x < y ? -1 : x > y; }

/* gg.stone */
Geo *lf_stone(double cx, double cy, double r, PyRandom *rng, double aspect, int nv, int round_) {
    if (round_) {          /* convex_hull(multipoints(3 x 40 circle points)).simplify(0.04) */
        double c3[3][3], pts[240]; int i, j; GEOSGeometry *mp, *hl, *sm;
        if (!have_csx) return NULL;
        for (i = 0; i < 3; i++) {
            double x0 = cx + py_uniform(rng, -.4, .4) * r, y0 = cy + py_uniform(rng, -.4, .4) * r;
            c3[i][0] = x0; c3[i][1] = y0; c3[i][2] = r * py_uniform(rng, .55, .8);
        }
        for (i = 0; i < 3; i++) for (j = 0; j < 40; j++) { pts[2 * (i * 40 + j)] = c3[i][0] + c3[i][2] * CSX[j]; pts[2 * (i * 40 + j) + 1] = c3[i][1] + c3[i][2] * CSY[j]; }
        mp = shp_multipoint(pts, 120);
        if (!mp) return NULL;
        hl = shp_convex_hull(mp); shp_free(mp);
        if (!hl) return NULL;
        sm = shp_simplify(hl, 0.04, 1); shp_free(hl);
        return geo_new(sm);
    }
    {
        int n = nv ? nv : (int)py_randint(rng, 5, 8), i, nh;
        double rot, *ts, *pts, *hp; Geo *g;
        if (aspect == 0.0) aspect = py_uniform(rng, .6, .95);
        rot = py_uniform(rng, 0, PI);
        if (n < 0) n = 0;
        ts = (double *)malloc(sizeof(double) * (size_t)(n + 1)); pts = (double *)malloc(sizeof(double) * 2 * (size_t)(n + 1)); hp = (double *)malloc(sizeof(double) * 4 * (size_t)(n + 1));
        for (i = 0; i < n; i++) ts[i] = py_uniform(rng, 0, 2 * PI);
        qsort(ts, (size_t)n, sizeof(double), cmp_d);
        for (i = 0; i < n; i++) ts[i] = (double)(i * 2) * PI / n * 0.6 + ts[i] * 0.4;
        for (i = 0; i < n; i++) {
            double rr = r * py_uniform(rng, .82, 1.1), x = rr * py_cos(ts[i]), y = rr * py_sin(ts[i]) * aspect;
            pts[2 * i] = cx + x * py_cos(rot) - y * py_sin(rot); pts[2 * i + 1] = cy + x * py_sin(rot) + y * py_cos(rot);
        }
        nh = lf_hull(pts, n, hp);
        g = geo_new(shp_polygon(hp, nh));
        free(ts); free(pts); free(hp);
        return g;
    }
}

Geo *lf_star(double cx, double cy, double ro, double ri, int n, double rot) { return geo_new(shp_star(cx, cy, ro, ri, n, rot)); }

/* gg.wiggle */
int lf_wiggle(double ax, double ay, double bx, double by, PyRandom *rng, double amp, int n, double *out) {
    double L = py_hypot(ax - bx, ay - by), nx, ny; int i, m = 0;
    if (L == 0.0) L = 1;          /* math.dist(a, b) or 1 */
    nx = -(by - ay) / L; ny = (bx - ax) / L;
    out[m++] = ax; out[m++] = ay;
    for (i = 1; i < n; i++) {
        double t = (double)i / n, o = py_uniform(rng, -amp, amp);
        out[m++] = ax + (bx - ax) * t + nx * o; out[m++] = ay + (by - ay) * t + ny * o;
    }
    out[m++] = bx; out[m++] = by;
    return m / 2;
}

/* gg.area_count */
int lf_area_count(int n, const Geo *region) {
    double k2;
    if (!SC.layout || SC.K <= 1.001) return n;
    if (region && shp_area(region->g) < 0.2 * PI * SC.R0 * SC.R0) return n;
    k2 = SC.K * SC.K;
    return (int)py_rint(n * (k2 < 30.0 ? k2 : 30.0));
}

/* ======================= gg3.py / gg2.py ======================= */

Geo *lf_rect(double cx, double cy, double l, double w, double a) { return geo_new(shp_rect(cx, cy, l, w, a)); }
Geo *lf_ring(double cx, double cy, double ro, double ri, int res) { return geo_new(shp_ring(cx, cy, ro, ri, res)); }
Geo *lf_hexa(double cx, double cy, double r, double rot) { return geo_new(shp_hexa(cx, cy, r, rot)); }

static GEOSGeometry *union_owned(GEOSGeometry **gs, int n) {          /* unary_union(list), the list freed */
    GEOSGeometry *u = shp_union_all((const GEOSGeometry *const *)gs, n); int i;
    for (i = 0; i < n; i++) shp_free(gs[i]);
    return u;
}

/* gg3.blob: unary_union of k discs, then .buffer(0.5).buffer(-0.5).simplify(0.05) */
Geo *lf_blob(double cx, double cy, double r, PyRandom *rng, int k) {
    GEOSGeometry **d = (GEOSGeometry **)malloc(sizeof(GEOSGeometry *) * (size_t)(k > 0 ? k : 1)), *u, *a, *b, *c; int i, bad = 0;
    for (i = 0; i < k; i++) {
        double x = cx + py_uniform(rng, -r, r) * .5, y = cy + py_uniform(rng, -r, r) * .5;
        d[i] = shp_point_buffer(x, y, r * py_uniform(rng, .5, .8), 16);
        if (!d[i]) bad = 1;
    }
    if (bad) { for (i = 0; i < k; i++) shp_free(d[i]); free(d); return NULL; }
    u = union_owned(d, k <= 0 ? 0 : k); free(d);
    if (!u) return NULL;
    a = shp_buffer_q(u, 0.5, 16); shp_free(u);
    if (!a) return NULL;
    b = shp_buffer_q(a, -0.5, 16); shp_free(a);
    if (!b) return NULL;
    c = shp_simplify(b, 0.05, 1); shp_free(b);
    return geo_new(c);
}

/* gg3.meander: LineString of f(u, off * K + amp * sin(k * u + ph) + 0.3 * amp * sin(2.3 * k * u)) over linspace(-E, E, n) */
Geo *lf_meander(PyRandom *rng, double phi, double off, double amp, double w, int n) {
    Frame f = lf_frame(0, 0, phi); double k = py_uniform(rng, 0.15, 0.3), ph = py_uniform(rng, 0, 6.3), km = SC.K > 1.0 ? SC.K : 1.0;
    double E = 19 * km, *u, *xy; int nn = (int)(n * km), i; Geo *g;
    (void)w;
    if (nn < 0) nn = 0;
    u = (double *)malloc(sizeof(double) * (size_t)(nn + 1)); xy = (double *)malloc(sizeof(double) * 2 * (size_t)(nn + 1));
    lf_linspace(-E, E, nn, u);
    for (i = 0; i < nn; i++) lf_at(&f, u[i], off * SC.K + amp * py_sin(k * u[i] + ph) + 0.3 * amp * py_sin(2.3 * k * u[i]), &xy[2 * i], &xy[2 * i + 1]);
    g = geo_new(shp_linestring(xy, nn));
    free(u); free(xy);
    return g;
}

/* gg3.spiral */
Geo *lf_spiral(double cx, double cy, double r, double turns, double w) {
    int n = (int)(turns * 40), i; double *ts, *xy, last; GEOSGeometry *l, *b;
    if (n <= 0) return NULL;          /* ts[-1] of an empty array: IndexError */
    ts = (double *)malloc(sizeof(double) * (size_t)n); xy = (double *)malloc(sizeof(double) * 2 * (size_t)n);
    lf_linspace(0, turns * 2 * PI, n, ts); last = ts[n - 1];
    for (i = 0; i < n; i++) { double t = ts[i]; xy[2 * i] = cx + (0.2 + r * t / last) * py_cos(t); xy[2 * i + 1] = cy + (0.2 + r * t / last) * py_sin(t); }
    l = shp_linestring(xy, n); free(ts); free(xy);
    if (!l) return NULL;
    b = shp_buffer(l, w / 2, 16, SHP_CAP_ROUND, SHP_JOIN_ROUND, 5.0, 0); shp_free(l);
    return geo_new(b);
}

static GEOSGeometry *line_buf(const double *xy, int n, double d, int cap, int quad) { return shp_line_buffer(xy, n, d, cap, SHP_JOIN_ROUND, quad, 5.0); }

/* gg3.radial */
Geo *lf_radial(double cx, double cy, double r0, double r1, int n, PyRandom *rng, double w) {
    GEOSGeometry **o = (GEOSGeometry **)malloc(sizeof(GEOSGeometry *) * (size_t)(n > 0 ? n : 1)), *u; int k, m = 0, bad = 0; double pts[8];
    for (k = 0; k < n; k++) {
        double a = py_uniform(rng, 0, 2 * PI); int np_;
        np_ = lf_wiggle(cx + r0 * py_cos(a), cy + r0 * py_sin(a), cx + r1 * py_cos(a), cy + r1 * py_sin(a), rng, 0.25, 3, pts);
        o[m] = line_buf(pts, np_, w, SHP_CAP_ROUND, 4);
        if (!o[m]) { bad = 1; break; }
        m++;
    }
    if (bad) { for (k = 0; k < m; k++) shp_free(o[k]); free(o); return NULL; }
    u = union_owned(o, m); free(o);
    return geo_new(u);
}

/* gg2.line_cut(a, b, w=0.5): LineString([a, b]).buffer(w / 2, cap_style=2) */
Geo *lf_line_cut(double ax, double ay, double bx, double by, double w) { double xy[4] = { ax, ay, bx, by }; return geo_new(line_buf(xy, 2, w / 2, SHP_CAP_FLAT, 16)); }

LfFocusFn lf_focus = NULL; void *lf_focus_ctx = NULL;
LfAimFn lf_aimfn = NULL; void *lf_aim_ctx = NULL;

/* gg3.rp */
void lf_rp(PyRandom *rng, double dmin, double dmax, double *x, double *y) {
    double K = SC.K, a, d; int err = 0;
    if (lf_focus) { lf_focus(lf_focus_ctx, rng, x, y); return; }
    dmin = dmin * K; dmax = dmax * K + (K - 1) * 2.0 * (double)(K > 1);
    a = py_uniform(rng, 0, 2 * PI);
    d = sqrt(py_uniform(rng, lf_fpow(dmin, 2, &err) / lf_fpow(dmax, 2, &err), 1)) * dmax;
    *x = d * py_cos(a); *y = d * py_sin(a);
}
/* gg3.aim */
double lf_aim(PyRandom *rng, double x, double y, double span) {
    double a = py_uniform(rng, 0, span);
    return lf_aimfn ? lf_aimfn(lf_aim_ctx, rng, x, y, a) : a;
}

/* ======================= details.py shapes ======================= */

static const char *TAGS[SH_COUNT] = { "rock", "round", "disc", "ell", "rect", "caps", "bone", "star", "crescent", "hex", "tri", "cross", "ring", "blob", "wig" };
const char *lf_shape_tag(int kind) { return kind >= 0 && kind < SH_COUNT ? TAGS[kind] : ""; }
LfShape lf_sh(int kind, double a0, double a1, double a2, double a3, double a4) { LfShape s; s.kind = kind; s.a[0] = a0; s.a[1] = a1; s.a[2] = a2; s.a[3] = a3; s.a[4] = a4; return s; }

static Geo *union2(GEOSGeometry *a, GEOSGeometry *b) { GEOSGeometry *v[2] = { a, b }; if (!a || !b) { shp_free(a); shp_free(b); return NULL; } return geo_new(union_owned(v, 2)); }

Geo *lf_shape(const LfShape *sh, double x, double y, PyRandom *rng) {
    const double *A = sh->a;
    switch (sh->kind) {
    case SH_ROCK: { double r = py_uniform(rng, A[0], A[1]); int nv = (int)py_randint(rng, (int64_t)A[2], (int64_t)A[3]); return lf_stone(x, y, r, rng, 0.0, nv, 0); }
    case SH_ROUND: { double r = py_uniform(rng, A[0], A[1]); return lf_stone(x, y, r, rng, 0.0, 0, 1); }
    case SH_DISC: return geo_new(shp_point_buffer(x, y, py_uniform(rng, A[0], A[1]), (int)A[2]));
    case SH_ELL: {          /* rotate(scale(Point(x, y).buffer(1, 20), U(a0, a1), U(b0, b1)), U(0, 180)) */
        GEOSGeometry *c = shp_point_buffer(x, y, 1, 20), *s1, *r; double fx, fy;
        if (!c) return NULL;
        fx = py_uniform(rng, A[0], A[1]); fy = py_uniform(rng, A[2], A[3]);
        s1 = shp_scale(c, fx, fy, SHP_ORIGIN_CENTER, 0, 0); shp_free(c);
        if (!s1) return NULL;
        r = shp_rotate(s1, py_uniform(rng, 0, 180), SHP_ORIGIN_CENTER, 0, 0, 0); shp_free(s1);
        return geo_new(r);
    }
    case SH_RECT: { double l = py_uniform(rng, A[0], A[1]), w = py_uniform(rng, A[2], A[3]); return lf_rect(x, y, l, w, lf_aim(rng, x, y, PI)); }
    case SH_CAPS: {
        double a = lf_aim(rng, x, y, PI), L = py_uniform(rng, A[0], A[1]) / 2, xy[4];
        xy[0] = x - L * py_cos(a); xy[1] = y - L * py_sin(a); xy[2] = x + L * py_cos(a); xy[3] = y + L * py_sin(a);
        return geo_new(line_buf(xy, 2, A[2] / 2, SHP_CAP_ROUND, 8));
    }
    case SH_BONE: {
        double a = py_uniform(rng, 0, PI), L = py_uniform(rng, A[0], A[1]) / 2, c = py_cos(a), s_ = py_sin(a), xy[4];
        GEOSGeometry *v[5]; int i = 1, bad = 0;
        xy[0] = x - L * c; xy[1] = y - L * s_; xy[2] = x + L * c; xy[3] = y + L * s_;
        v[0] = line_buf(xy, 2, 0.32, SHP_CAP_ROUND, 8);
        for (int e = -1; e <= 1; e += 2) for (int k = -1; k <= 1; k += 2) {
            v[i] = shp_point_buffer(x + e * L * c + k * 0.32 * -s_, y + e * L * s_ + k * 0.32 * c, 0.36, 10);
            if (!v[i]) bad = 1;
            i++;
        }
        if (!v[0] || bad) { for (i = 0; i < 5; i++) shp_free(v[i]); return NULL; }
        return geo_new(union_owned(v, 5));
    }
    case SH_STAR: {
        double ro = py_uniform(rng, A[0], A[1]), ri = ro * py_uniform(rng, .45, .6); int n = (int)py_randint(rng, (int64_t)A[2], (int64_t)A[3]);
        return lf_star(x, y, ro, ri, n, py_uniform(rng, 0, 1));
    }
    case SH_CRESCENT: {
        double r = py_uniform(rng, A[0], A[1]), a = py_uniform(rng, 0, 2 * PI);
        GEOSGeometry *p = shp_point_buffer(x, y, r, 24), *q = shp_point_buffer(x + r * .55 * py_cos(a), y + r * .55 * py_sin(a), r * .85, 24), *g;
        GeoList ps = { 0 }; Geo *best = NULL; double ba = 0;
        g = p && q ? shp_difference(p, q) : NULL; shp_free(p); shp_free(q);
        if (!g) return NULL;
        if (shp_is_empty(g)) { shp_free(g); return geo_new(shp_point_buffer(x, y, r * .5, 16)); }
        lf_polys_of_new(g, &ps);
        for (int i = 0; i < ps.n; i++) { double ar = shp_area(ps.v[i]->g); if (!best || ar > ba) { best = ps.v[i]; ba = ar; } }          /* max(key=area): the first */
        geo_ref(best); gl_free(&ps);
        return best;          /* NULL: max() of an empty list raises */
    }
    case SH_HEX: { double r = py_uniform(rng, A[0], A[1]), a = py_uniform(rng, 0, 1); return lf_hexa(x, y, r, a); }
    case SH_TRI: { double r = py_uniform(rng, A[0], A[1]), asp = py_uniform(rng, .5, .8); return lf_stone(x, y, r, rng, asp, 3, 0); }
    case SH_CROSS: { double L = py_uniform(rng, A[0], A[1]), a = py_uniform(rng, 0, PI); return union2(shp_rect(x, y, L, A[2], a), shp_rect(x, y, A[2], L, a)); }
    case SH_RING: { double ro = py_uniform(rng, A[0], A[1]); return lf_ring(x, y, ro, ro - A[2], 24); }
    case SH_BLOB: { double r = py_uniform(rng, A[0], A[1]); int k = (int)py_randint(rng, 3, 5); return lf_blob(x, y, r, rng, k); }
    case SH_WIG: {
        double a = py_uniform(rng, 0, PI), L = py_uniform(rng, A[0], A[1]) / 2, *pts; int n = (int)L, m; Geo *g;
        double p0x = x - L * py_cos(a), p0y = y - L * py_sin(a), p1x = x + L * py_cos(a), p1y = y + L * py_sin(a);
        if (n < 3) n = 3;
        pts = (double *)malloc(sizeof(double) * 2 * (size_t)(n + 1));
        m = lf_wiggle(p0x, p0y, p1x, p1y, rng, A[3], n, pts);
        g = geo_new(line_buf(pts, m, A[2] / 2, SHP_CAP_ROUND, 4));
        free(pts);
        return g;
    }
    }
    return NULL;
}

/* ======================= details.py top patterns ======================= */

static int mrr_angle(const Geo *g, double *a) {          /* mr = g.minimum_rotated_rectangle.exterior.coords; atan2 of its first edge */
    GEOSGeometry *m = shp_min_rot_rect(g->g); double xy[64]; int n;
    if (!m) return 0;
    if (G.GEOSGeomTypeId_r(gctx, m) != GEOS_POLYGON) { shp_free(m); return 0; }          /* no .exterior: AttributeError */
    n = shp_coords(shp_exterior(m), xy, 32);
    shp_free(m);
    if (n < 2) return 0;          /* mr[1] of an empty ring: IndexError */
    *a = py_atan2(xy[3] - xy[1], xy[2] - xy[0]);
    return 1;
}

Geo *lf_pat(const LfPat *pt, double cx, double cy, const Geo *g, PyRandom *rng) {
    switch (pt->kind) {
    case PAT_SLOTS: case PAT_SEAMS: {
        double a, us[256], x, y; int n, i; Frame ff; GEOSGeometry **v; Geo *r;
        if (!mrr_angle(g, &a)) return NULL;
        ff = lf_frame(cx, cy, a);
        if (pt->kind == PAT_SLOTS) { n = (int)pt->a[0]; for (i = 0; i < n; i++) us[i] = (i - (pt->a[0] - 1) / 2) * 0.8; }
        else { n = lf_arange(-3, 3.1, pt->a[0], us, 256); if (n < 0) return NULL; }
        if (n < 0) n = 0;
        v = (GEOSGeometry **)malloc(sizeof(GEOSGeometry *) * (size_t)(n + 1));
        for (i = 0; i < n; i++) {
            if (pt->kind == PAT_SLOTS) { lf_at(&ff, 0, us[i], &x, &y); v[i] = shp_rect(x, y, 6, pt->a[1], a); }
            else { lf_at(&ff, us[i], 0, &x, &y); v[i] = shp_rect(x, y, 0.3, 8, a); }
        }
        r = geo_new(union_owned(v, n)); free(v);
        return r;
    }
    case PAT_RING: {
        GEOSGeometry *a = shp_inset(g->g, 0.35), *b = shp_inset(g->g, 0.35 + pt->a[0]), *r = a && b ? shp_difference(a, b) : NULL;
        shp_free(a); shp_free(b);
        return geo_new(r);
    }
    case PAT_CROSS: return union2(shp_rect(cx, cy, 8, 0.35, 0.3), shp_rect(cx, cy, 0.35, 8, 0.3));
    case PAT_DOTS: {
        GEOSGeometry *v[4]; int i = 0; static const double D[2] = { -0.8, 0.8 };
        for (int a = 0; a < 2; a++) for (int b = 0; b < 2; b++) v[i++] = shp_point_buffer(cx + D[a], cy + D[b], pt->a[0], 10);
        return geo_new(union_owned(v, 4));
    }
    case PAT_HOLE: return geo_new(shp_point_buffer(cx, cy, py_uniform(rng, pt->a[0], pt->a[1]), 20));
    case PAT_RUNE: {
        static const double S[4][4] = { { -0.9, -0.9, 0.9, 0.9 }, { -0.9, 0.9, 0, 0 }, { 0, -1.0, 0, 1.0 }, { -0.9, 0, 0.9, 0 } };
        double a = py_uniform(rng, 0, PI), px, py_, qx, qy; Frame ff = lf_frame(cx, cy, a); int64_t k = py_randint(rng, 2, 3), idx[4]; GEOSGeometry *v[4]; int i;
        py_sample_idx(rng, 4, k, idx);
        for (i = 0; i < k; i++) {
            const double *sg = S[idx[i]];
            lf_at(&ff, sg[0], sg[1], &px, &py_); lf_at(&ff, sg[2], sg[3], &qx, &qy);
            { double xy[4] = { px, py_, qx, qy }; v[i] = line_buf(xy, 2, 0.3 / 2, SHP_CAP_FLAT, 16); }
        }
        return geo_new(union_owned(v, (int)k));
    }
    }
    return NULL;
}
