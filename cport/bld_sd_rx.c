/* bld_sd_rx.c: the runtime of bld_sd_rx.h, the shared tools of the sd_r_x_* files (x_cutfix, x_d, x_word, x_chain
   of sd_r_x_frontier.py, x_wall of sd_r_x_dungeon.py, t_bar of sd_r_trederra.py, _pl / _lean / _curl of
   sd_r_infestation.py) and bld_register_sd_rx(): the 18 original styles' sd_r_x_<style>.py builders registered with
   sd_reg in Python's order (each file's own decorators, top to bottom; every file registers only its own style). */
#include <stdio.h>
#include "bld_sd_rx.h"

/* ------------------------------------------------------------------ the arena and the jump */
static GG **AG; static int nAG, capAG;
static void **AM; static int nAM, capAM;
static S **AS; static int nAS, capAS;
static jmp_buf *XJ; static int NONE_;

_Noreturn void xr_fail(void) { if (!XJ) abort(); longjmp(*XJ, 1); }
_Noreturn void xr_none(void) { NONE_ = 1; xr_fail(); }
GG *xr_keep(GG *g) {
    if (!g) xr_fail();
    if (nAG == capAG) { capAG = capAG ? 2 * capAG : 1024; AG = (GG **)realloc(AG, sizeof(GG *) * (size_t)capAG); }
    AG[nAG++] = g; return g;
}
void *xr_mem(size_t n) {
    void *p = calloc(1, n ? n : 1);
    if (!p) xr_fail();
    if (nAM == capAM) { capAM = capAM ? 2 * capAM : 256; AM = (void **)realloc(AM, sizeof(void *) * (size_t)capAM); }
    AM[nAM++] = p; return p;
}
void xr_ok(int r) { if (r < 0) xr_fail(); }
static void arena_free(void) {
    for (int i = 0; i < nAG; i++) shp_free(AG[i]);
    for (int i = 0; i < nAM; i++) free(AM[i]);
    for (int i = 0; i < nAS; i++) { sol_free(AS[i]); free(AS[i]); }
    nAG = nAM = nAS = 0;
}
#define K(x) xr_keep(x)

/* ------------------------------------------------------------------ random, numbers */
int64_t RI(PyRandom *rng, int64_t a, int64_t b) { int64_t v; xr_ok(py_randint_e(rng, a, b, &v)); return v; }
int64_t CHI(PyRandom *rng, int64_t n) { int64_t v; xr_ok(py_choice_e(rng, n, &v)); return v; }
double CH(PyRandom *rng, const double *v, int n) { return v[CHI(rng, n)]; }
double PWs(double x, double y, int *err) { double re, im; int r = px_pow(x, y, &re, &im); if (r != 0) { *err = 1; return NAN; } return re; }
double PW(double x, double y) { int e = 0; double v = PWs(x, y, &e); if (e) xr_fail(); return v; }
int AR(double *out, int cap, double start, double stop, double step) { int64_t n = np_arange(start, stop, step, out, cap); if (n < 0 || n > cap) xr_fail(); return (int)n; }
void LS(double *out, double start, double stop, int num) { xr_ok(np_linspace(start, stop, num, 1, out)); }
int64_t PINT(double x) { int64_t v; xr_ok(px_int(x, &v)); return v; }

/* ------------------------------------------------------------------ outlines */
GG *DISC(double x, double y, double r, int n) { return K(sd_disc(x, y, r, n)); }
GG *ANN(double x, double y, double ro, double ri, int n) { return K(sd_ann(x, y, ro, ri, n)); }
GG *CAPL(P p, P q, double w) { return K(sd_capl(&p.x, &q.x, w)); }
GG *BARL(P p, P q, double w) { return K(sd_barl(&p.x, &q.x, w)); }
GG *LINE(const P *pts, int n, double w) { return K(sd_line(&pts[0].x, n, w)); }
GG *ELL(double x, double y, double rx, double ry, double a, int n) { return K(sd_ell(x, y, rx, ry, a, n)); }
GG *NGON(double x, double y, double r, int n, double a) { return K(sd_ngon(x, y, r, n, a)); }
GG *RECT(double x, double y, double L, double W, double a) { return K(shp_rect(x, y, L, W, a)); }
GG *RRECT(double x, double y, double L, double W, double a, double r) { return K(sd_rrect(x, y, L, W, a, r)); }
GG *TRI(P p, P q, double w) { return K(sd_tri(&p.x, &q.x, w)); }
GG *POLY(const P *pts, int n) { return K(shp_polygon(n ? &pts[0].x : NULL, n)); }
GG *LSTR(const P *pts, int n) { return K(shp_linestring(n ? &pts[0].x : NULL, n)); }
GG *PTG(double x, double y) { return K(shp_point(x, y)); }
GG *BUF(const GG *g, double d, int q) { return K(shp_buffer_q(g, d, q)); }
GG *BUFX(const GG *g, double d, int q, int cap, int join, double mitre) { return K(shp_buffer(g, d, q, cap, join, mitre, 0)); }
GG *INS(const GG *g, double d) { return K(shp_inset(g, d)); }
GG *DIF(const GG *a, const GG *b) { return K(shp_difference(a, b)); }
GG *INT(const GG *a, const GG *b) { return K(shp_intersection(a, b)); }
GG *UNI(const GG *a, const GG *b) { return K(shp_union(a, b)); }
GG *EXTR(const GG *p) { const GG *e = shp_exterior(p); if (!e) xr_fail(); return K(shp_clone(e)); }
GG *CHULL(const GG *g) { return K(shp_convex_hull(g)); }
GG *SIMP(const GG *g, double tol) { return K(shp_simplify(g, tol, 1)); }
GG *TRANS(const GG *g, double dx, double dy) { return K(shp_translate(g, dx, dy)); }
GG *ROT(const GG *g, double ang, int origin, double ox, double oy, int rad) { return K(shp_rotate(g, ang, origin, ox, oy, rad)); }
GG *SCL(const GG *g, double fx, double fy, int origin, double ox, double oy) { return K(shp_scale(g, fx, fy, origin, ox, oy)); }
GG *BLOB(double x, double y, double r, PyRandom *rng, int k) {
    Geo *g = lf_blob(x, y, r, rng, k); GG *c;
    if (!g) xr_fail();
    c = shp_clone(g->g); geo_unref(g); return K(c);
}
GG *STAR(double x, double y, double ro, double ri, int n, double rot) { return K(shp_star(x, y, ro, ri, n, rot)); }
GG *WAVY(P p, P q, PyRandom *rng, double amp, int n, double w) { return K(sd_wavy(&p.x, &q.x, rng, amp, n, w)); }
GG *GEMPTY(void) { return K(shp_polygon(NULL, 0)); }
int EMPTY(const GG *g) { int e = shp_is_empty(g); if (e < 0) xr_fail(); return e; }
double AREA(const GG *g) { return shp_area(g); }
double LEN(const GG *g) { return shp_length(g); }
P CENT(const GG *g) { GG *c = K(shp_centroid(g)); P p = { shp_x(c), shp_y(c) }; return p; }
void BOUNDS(const GG *g, double b[4]) { shp_bounds(g, b); }
int VALID(const GG *g) { int v = shp_is_valid(g); if (v < 0) xr_fail(); return v; }
int CONTAINS(const GG *a, const GG *b) { int v = shp_contains(a, b); if (v < 0) xr_fail(); return v; }
int INTERSECTS(const GG *a, const GG *b) { int v = shp_intersects(a, b); if (v < 0) xr_fail(); return v; }
double DISTG(const GG *a, const GG *b) { return shp_distance(a, b); }
void GA(GL *l, GG *g) {
    if (l->n == l->cap) { int nc = l->cap ? 2 * l->cap : 32; GG **v = (GG **)xr_mem(sizeof(GG *) * (size_t)nc); if (l->n) memcpy(v, l->v, sizeof(GG *) * (size_t)l->n); l->v = v; l->cap = nc; }
    l->v[l->n++] = g;
}
GG *UL(const GL *l) { return K(sd_U((const GEOSGeometry *const *)l->v, l->n)); }
GG *U1(GG *a) { return K(sd_U((const GEOSGeometry *const *)&a, 1)); }
GG *U2(GG *a, GG *b) { GG *v[2] = { a, b }; return K(sd_U((const GEOSGeometry *const *)v, 2)); }
GG *U3(GG *a, GG *b, GG *c) { GG *v[3] = { a, b, c }; return K(sd_U((const GEOSGeometry *const *)v, 3)); }
GG *HULL(const GL *l) { return K(sd_hull((const GEOSGeometry *const *)l->v, l->n)); }
void POLYS(const GG *g, GL *out) { ShpList l = { 0 }; shp_polys_of(g, &l); for (int i = 0; i < l.n; i++) GA(out, K(l.g[i])); free(l.g); }

/* ------------------------------------------------------------------ solids */
S *SN(void) {
    S *s = (S *)calloc(1, sizeof *s);
    if (!s) xr_fail();
    if (nAS == capAS) { capAS = capAS ? 2 * capAS : 256; AS = (S **)realloc(AS, sizeof(S *) * (size_t)capAS); }
    AS[nAS++] = s; return s;
}
void EXT(S *s, const GG *g, double h, double lift) { xr_ok(sd_ext(s, g, h, lift)); }
void CYL(S *s, double x, double y, double r, double h, double lift, int n) { xr_ok(sd_cyl(s, x, y, r, h, lift, n)); }
void BLK(S *s, double x, double y, double L, double W, double a, double h, double lift) { xr_ok(sd_blk(s, x, y, L, W, a, h, lift)); }
void DOME(S *s, double x, double y, double r, double h, double lift, double sq, double a, int n) { xr_ok(sd_dome(s, x, y, r, h, lift, sq, a, n)); }
void CONE(S *s, double x, double y, double r, double h, double tip, double lift, int n, double sq, double a) { xr_ok(sd_cone(s, x, y, r, h, tip, lift, n, sq, a)); }
void TURN(S *s, double x, double y, const double *prof, int np, int n, double sq, double a, double lift) { xr_ok(sd_turn(s, x, y, prof, np, n, sq, a, lift)); }
void LIE(S *s, double x, double y, double a, double L, double r, double lift) { xr_ok(sd_lie(s, x, y, a, L, r, lift)); }
void RIDGE(S *s, double x, double y, double a, double L, double W, double h, double lift) { xr_ok(sd_ridge(s, x, y, a, L, W, h, lift)); }
void LOAF(S *s, double x, double y, double a, double L, double W, double h, double lift) { xr_ok(sd_loaf(s, x, y, a, L, W, h, lift)); }
void ROD(S *s, const P *pts, int n, double r, SoNumFn rf, void *ctx, double hk, double lift, double arch) { xr_ok(sd_rod(s, &pts[0].x, n, r, rf, ctx, hk, lift, arch)); }
void SPIN(S *s, double x, double y, double a, double L, SoNumFn rf, void *ctx, double hk, double lift, const char *prof) { xr_ok(sd_spin(s, x, y, a, L, rf, ctx, hk, lift, prof)); }
int CRV(double *prof, double r0, double r1, double z0, double z1, int out_, int n) { xr_ok(sd_crv(r0, r1, z0, z1, out_, n, prof)); return 2 * (n + 1); }
void POT(S *s, double x, double y, double r, double h, double neck, double lip, double lift, int n) { xr_ok(sd_pot(s, x, y, r, h, neck, lip, lift, n)); }
void ROCK(S *s, double x, double y, double r, double h, PyRandom *rng, double sq, double lift, int n) { xr_ok(sd_rock(s, x, y, r, h, rng, sq, lift, n)); }
void FLUTED(S *s, double x, double y, double r, double h, int n, double a, double deep, int m, double twist, double lift) { xr_ok(sd_fluted(s, x, y, r, h, n, a, deep, m, twist, lift)); }
void HANG(S *s, const GG *g, double z0, double z1) { xr_ok(sd_hang(s, g, z0, z1)); }
void SCAT(S *s, const S *o) { sol_extend(s, o); }

/* ---- sd_r_trederra.t_bar ---- */
typedef struct { int m; double *S, *R, *C; double px, py, ux, uy, L; } TBarCtx;
static void tbar_free(void *c) { TBarCtx *t = (TBarCtx *)c; free(t->S); free(t->R); free(t->C); free(t); }
static int tbar_sect(void *c, double z, GEOSGeometry **out) {
    TBarCtx *t = (TBarCtx *)c; int n = t->m + 1, nrun = 0, nout = 0, bad = 0;
    double *rs = (double *)malloc(sizeof(double) * (size_t)n), *rw = (double *)malloc(sizeof(double) * (size_t)n), *xy = (double *)malloc(sizeof(double) * 4 * (size_t)n);
    GEOSGeometry **gs = (GEOSGeometry **)malloc(sizeof(GEOSGeometry *) * (size_t)(n + 1));
    *out = NULL;
    for (int i = 0; i <= n && !bad; i++) {
        double s = i < n ? t->S[i] : 0.0, r = i < n ? t->R[i] : 0.0, cc = i < n ? t->C[i] : 0.0, w;
        if (z <= cc) w = r;
        else if (z < cc + r) { int e = 0; double d2 = PWs(z - cc, 2.0, &e), v = r * r - d2; if (e) { bad = 1; break; } w = sqrt(v > 0.0 ? v : 0.0); }
        else w = 0.0;
        if (w >= 0.2) { rs[nrun] = s; rw[nrun] = w; nrun++; continue; }
        if (nrun >= 2 && (rs[nrun - 1] - rs[0]) * t->L >= 0.3) {
            int k = 0;
            for (int j = 0; j < nrun; j++) { xy[k++] = t->px + t->ux * rs[j] * t->L - t->uy * rw[j]; xy[k++] = t->py + t->uy * rs[j] * t->L + t->ux * rw[j]; }
            for (int j = nrun - 1; j >= 0; j--) { xy[k++] = t->px + t->ux * rs[j] * t->L - t->uy * -rw[j]; xy[k++] = t->py + t->uy * rs[j] * t->L + t->ux * -rw[j]; }
            GEOSGeometry *g = shp_polygon(xy, 2 * nrun);
            if (!g) { bad = 1; break; }
            int v = shp_is_valid(g);
            if (v < 0) { shp_free(g); bad = 1; break; }
            if (v && shp_area(g) > 0.05) gs[nout++] = g; else shp_free(g);
        }
        nrun = 0;
    }
    if (!bad && nout) { *out = sd_U((const GEOSGeometry *const *)gs, nout); if (!*out) bad = 1; }
    for (int i = 0; i < nout; i++) shp_free(gs[i]);
    free(rs); free(rw); free(xy); free(gs);
    return bad ? SO_ERR : SO_OK;
}
/* the round bars of sd_r_trederra.t_bar (step 0.11) and the files' own copies xc_bar / xu_bar (0.1), xm_bar / xt_bar (0.11) */
static void tbar_make(S *s, P p, P q, double r, XrNum rfn, void *ctx, double k, int has_z, double z0, double z1, double step) {
    double L = DIST(p, q), top = 0, *R; int m; TBarCtx *t;
    if (L == 0.0) L = 1.0;
    m = (int)PINT(L / step); if (m < 8) m = 8;
    R = (double *)xr_mem(sizeof(double) * (size_t)(m + 1));          /* the radii first: an rf may raise (xr_fail) */
    for (int i = 0; i <= m; i++) { double v = rfn ? rfn(ctx, (double)i / (double)m) : r; R[i] = v > 0.0 ? v : 0.0; }
    t = (TBarCtx *)calloc(1, sizeof *t); t->m = m; t->L = L; t->px = p.x; t->py = p.y; t->ux = (q.x - p.x) / L; t->uy = (q.y - p.y) / L;
    t->S = (double *)malloc(sizeof(double) * (size_t)(m + 1)); t->R = (double *)malloc(sizeof(double) * (size_t)(m + 1)); t->C = (double *)malloc(sizeof(double) * (size_t)(m + 1));
    for (int i = 0; i <= m; i++) { t->S[i] = (double)i / (double)m; t->R[i] = R[i]; }
    for (int i = 0; i <= m; i++) t->C[i] = has_z ? (z0 + (z1 - z0) * t->S[i]) : k * t->R[i];
    for (int i = 0; i <= m; i++) { double v = t->C[i] + t->R[i]; if (i == 0 || v > top) top = v; }
    Solid *so = sol_custom(tbar_sect, t, tbar_free, top); sol_push(s, so); sol_unref(so);
}
void TBAR(S *s, P p, P q, double r, XrNum rfn, void *ctx, double k, int has_z, double z0, double z1) { tbar_make(s, p, q, r, rfn, ctx, k, has_z, z0, z1, 0.11); }
void XBAR(S *s, P p, P q, double r, XrNum rfn, void *ctx, double k, double step) { tbar_make(s, p, q, r, rfn, ctx, k, 0, 0, 0, step); }

/* ---- sd_r_x_dungeon.x_wall ---- */
typedef struct { int n; double *x, *y, *z, w; } XWallCtx;
static void xwall_free(void *c) { XWallCtx *t = (XWallCtx *)c; free(t->x); free(t->y); free(t->z); free(t); }
static int xwall_sect(void *c, double z, GEOSGeometry **out) {
    XWallCtx *t = (XWallCtx *)c; int nrun = 0, nout = 0, bad = 0;
    double *xy = (double *)malloc(sizeof(double) * 2 * (size_t)(t->n + 1));
    GEOSGeometry **gs = (GEOSGeometry **)malloc(sizeof(GEOSGeometry *) * (size_t)(t->n + 1));
    *out = NULL;
    for (int i = 0; i <= t->n; i++) {
        double zz = i < t->n ? t->z[i] : -1.0;
        if (zz > z) { xy[2 * nrun] = t->x[i]; xy[2 * nrun + 1] = t->y[i]; nrun++; continue; }
        if (nrun >= 2) {
            GEOSGeometry *g = shp_line_buffer(xy, nrun, t->w / 2, SHP_CAP_FLAT, SHP_JOIN_ROUND, 16, 5.0);
            if (!g) { bad = 1; break; }
            gs[nout++] = g;
        }
        nrun = 0;
    }
    if (!bad && nout) { *out = sd_U((const GEOSGeometry *const *)gs, nout); if (!*out) bad = 1; }
    for (int i = 0; i < nout; i++) shp_free(gs[i]);
    free(xy); free(gs);
    return bad ? SO_ERR : SO_OK;
}
void XWALL(S *s, const P *pts, int n, double w, XrNum zf, void *ctx) {
    GG *ln = LSTR(pts, n); double top = 0, *X, *Y, *Z; int m = (int)PINT(LEN(ln) / 0.08); XWallCtx *t;
    if (m < 8) m = 8;
    X = (double *)xr_mem(sizeof(double) * (size_t)(m + 1)); Y = (double *)xr_mem(sizeof(double) * (size_t)(m + 1)); Z = (double *)xr_mem(sizeof(double) * (size_t)(m + 1));
    for (int i = 0; i <= m; i++) { GG *p = K(shp_interpolate(ln, (double)i / (double)m, 1)); X[i] = shp_x(p); Y[i] = shp_y(p); }
    for (int i = 0; i <= m; i++) Z[i] = zf(ctx, (double)i / (double)m);
    for (int i = 0; i <= m; i++) if (i == 0 || Z[i] > top) top = Z[i];
    t = (XWallCtx *)calloc(1, sizeof *t); t->n = m + 1; t->w = w;
    t->x = (double *)malloc(sizeof(double) * (size_t)(m + 1)); t->y = (double *)malloc(sizeof(double) * (size_t)(m + 1)); t->z = (double *)malloc(sizeof(double) * (size_t)(m + 1));
    memcpy(t->x, X, sizeof(double) * (size_t)(m + 1)); memcpy(t->y, Y, sizeof(double) * (size_t)(m + 1)); memcpy(t->z, Z, sizeof(double) * (size_t)(m + 1));
    Solid *so = sol_custom(xwall_sect, t, xwall_free, top); sol_push(s, so); sol_unref(so);
}

/* ---- sd_r_infestation._pl, _lean, _curl ---- */
double XPL(const double *pts, int n, double s) {
    for (int i = 0; i + 1 < n; i++) {
        double s0 = pts[2 * i], v0 = pts[2 * i + 1], s1 = pts[2 * i + 2], v1 = pts[2 * i + 3];
        if (s <= s1) return v0 + (v1 - v0) * (s - s0) / MX(1e-9, s1 - s0);
    }
    return pts[2 * n - 1];
}
void LEAN(S *s, double x, double y, double r, double h, double dx, double dy, int n, double tip, double sq, double a, double pw) {
    for (int k = 0; k < n; k++) {
        double f = (double)k / (double)(n - 1), b = PW(1 - f, pw), rx = MX(tip, r * b), ry = MX(tip, r * b * sq);
        /* x + dx * k / (n - 1): (dx * k) / (n - 1) */
        GG *e = ELL(x + dx * k / (n - 1), y + dy * k / (n - 1), rx, ry, a, 12);
        EXT(s, e, h * (k + 1) / n, 0.0);
    }
}
int CURL(P *out, P p, double t, double L, double bend, int n) {
    out[0] = p;
    for (int k = 0; k < n; k++) { double t2 = t + bend * (k + 0.5) / n; out[k + 1].x = out[k].x + L / n * COS(t2); out[k + 1].y = out[k].y + L / n * SIN(t2); }
    return n + 1;
}

/* scenes._sack(cx, cy, a, L, W, h, lift=0.0) */
typedef struct { double L, W; } Sack;
static int sack_rf(void *c, double u_, double *out) {
    Sack *s = (Sack *)c; double L = s->L, W = s->W, full = 0.0, k; int e = 0;
    double q = fabs(2 * u_ / L);
    if (q < 1) { double p = PWs(q, 3.2, &e); full = PWs(1 - p, 0.45, &e); }
    if (u_ < L * 0.2) k = 1.0; else k = 1 - 0.5 * PWs((u_ - L * 0.2) / (L * 0.3), 2, &e);
    if (e) return SO_ERR;
    *out = W / 2 * full * k; return SO_OK;
}
static int neck_rf(void *c, double u_, double *out) { (void)c; *out = 0.34 + 0.14 * u_ / 0.45; return SO_OK; }
void XSACK(S *s, double cx, double cy, double a, double L, double W, double h, double lift) {
    Sack *sc = (Sack *)xr_mem(sizeof *sc); sc->L = L; sc->W = W;
    xr_ok(so_loft(s, cx, cy, a, L, sack_rf, sc, 2 * h / W, "round", lift, 0));
    F f = FR(cx, cy, a); P c = FF(&f, L / 2 + 0.25, 0);
    xr_ok(so_loft(s, c.x, c.y, a, 0.9, neck_rf, NULL, 1.0 * h / W + 0.6, "round", lift, 0));
}

/* ------------------------------------------------------------------ the Obj */
int UP(SoObj *o, const GG *g, double h, const char *kind) { int r = so_obj_up(o, g, h, kind); xr_ok(r); return r; }
int UPi(SoObj *o, const GG *g, int64_t h, const char *kind) { int r = so_obj_up(o, g, (double)h, kind); xr_ok(r); o->L.v[r]->v[2] = lv_int(h); return r; }
int ON(SoObj *o, const GG *g, int par, double h, const char *kind) { int r = so_obj_on(o, g, par >= 0, par, h, kind); xr_ok(r); return r; }
void CUT(SoObj *o, const GG *g, int par, double d) { xr_ok(so_obj_cut(o, g, par >= 0, par, d)); }
int ST(SoObj *o, const S *s, const char *kind, int par, const C *cuts, int nc, const GG *foot) { int r = so_obj_st(o, s, kind, par >= 0, par, cuts, nc, foot); xr_ok(r); return r; }
void GCUT(SoObj *o, const GG *g, double d, const char *kind) { xr_ok(so_obj_gcut(o, g, d, kind)); }
int LID(SoObj *o, const GG *g, int par, double h, double edge, const char *kind) { int r = so_obj_lid(o, g, par >= 0, par, h, edge, kind); xr_ok(r); return r; }
void GROOVE(SoObj *o, const GG *g, int par, double w, double d) { xr_ok(so_obj_groove(o, g, par >= 0, par, w, d)); }
void SEAMS(SoObj *o, const F *ff, double L, double W, int par, double step, int across, double w, double d, const GG *skip) { xr_ok(so_obj_seams(o, ff, L, W, par >= 0, par, step, across, w, d, skip)); }
int STUDS(SoObj *o, const P *pts, int n, int par, double r, double h, const char *kind, int nn) { int v = so_obj_studs(o, &pts[0].x, n, par >= 0, par, r, h, kind, nn); xr_ok(v); return v; }
int HANDLE(SoObj *o, P p, P q, int par, double w, double h, const char *kind) { int v = so_obj_handle(o, &p.x, &q.x, par >= 0, par, w, h, kind); xr_ok(v); return v; }
int TABS(SoObj *o, const F *ff, const double *xs, int nx, double v, int par, double a, double l, double w, double h, const char *kind) { int r = so_obj_tabs(o, ff, xs, nx, v, par >= 0, par, a, l, w, h, kind); xr_ok(r); return r; }
int RIBS(SoObj *o, const F *ff, double L, double W, int par, int n, double a, double w, double h, int across, const char *kind) { int r = so_obj_ribs(o, ff, L, W, par >= 0, par, n, a, w, h, across, kind); xr_ok(r); return r; }
void VENTS(SoObj *o, const F *ff, double x, double v, int par, int n, double l, double a, double step, double w, double d) { xr_ok(so_obj_vents(o, ff, x, v, par >= 0, par, n, l, a, step, w, d)); }
int CORNERS(SoObj *o, const F *ff, double L, double W, int par, double a, double size, double h, const char *kind) { int r = so_obj_corners(o, ff, L, W, par >= 0, par, a, size, h, kind); xr_ok(r); return r; }
int STRAPS(SoObj *o, const F *ff, const double *xs, int nx, double W, int par, double a, double w, double h, const char *kind) { int r = so_obj_straps(o, ff, xs, nx, W, par >= 0, par, a, w, h, kind); xr_ok(r); return r; }
void FPUSH(SoObj *o, const GG *g) {
    if (o->F.n == o->F.cap) { o->F.cap = o->F.cap ? 2 * o->F.cap : 16; o->F.v = (GEOSGeometry **)realloc(o->F.v, sizeof(GEOSGeometry *) * (size_t)o->F.cap); }
    o->F.v[o->F.n++] = shp_clone(g);
}
int XON(SoObj *o, const GG *g, int par, double h, const char *kind) {
    if (!g || EMPTY(g)) return -1;
    SoLayer *l = ly_new(5); l->v[0] = lv_str("on"); l->v[1] = lv_geom(shp_clone(g)); l->v[2] = par >= 0 ? lv_int(par) : lv_none(); l->v[3] = lv_float(h);
    l->v[4] = lv_str(so_intern(kind && *kind ? kind : o->k)); lyl_push_own(&o->L, l);
    return o->L.n - 1;
}
int XSTUDS(SoObj *o, const P *pts, int npts, int par, double r, double h, const char *kind, int n) {
    GL l = { 0 };
    if (!npts) return -1;
    for (int i = 0; i < npts; i++) GA(&l, DISC(pts[i].x, pts[i].y, r, n));
    return XON(o, UL(&l), par, h, kind);
}
GG *OUTX(SoObj *o, GG *const *extra, int n) { GG *h; xr_ok(so_obj_out(o, (const GEOSGeometry *const *)extra, n, &h)); return K(h); }
GG *OUT(SoObj *o) { GG *h; xr_ok(so_obj_out(o, NULL, 0, &h)); return K(h); }

/* ------------------------------------------------------------------ sd_r_x_frontier: x_word, x_chain */
typedef struct { char ch; int ns; const double *st[3]; int np[3]; } Glyph;
#define S_(...) (const double[]){ __VA_ARGS__ }
static const double GP[] = { 0, 0, 0, 1, 0.45, 1, 0.6, 0.85, 0.6, 0.62, 0.45, 0.5, 0, 0.5 };          /* _P */
static const Glyph GLYPHS[] = {
    { 'A', 2, { S_(0, 0, 0.3, 1, 0.6, 0), S_(0.12, 0.38, 0.48, 0.38) }, { 3, 2 } },
    { 'B', 2, { S_(0, 0, 0, 1, 0.42, 1, 0.55, 0.88, 0.55, 0.62, 0.42, 0.52, 0, 0.52), S_(0.42, 0.52, 0.6, 0.4, 0.6, 0.12, 0.45, 0, 0, 0) }, { 7, 5 } },
    { 'C', 1, { S_(0.6, 0.85, 0.45, 1, 0.15, 1, 0, 0.8, 0, 0.2, 0.15, 0, 0.45, 0, 0.6, 0.15) }, { 8 } },
    { 'D', 1, { S_(0, 0, 0, 1, 0.35, 1, 0.6, 0.75, 0.6, 0.25, 0.35, 0, 0, 0) }, { 7 } },
    { 'E', 2, { S_(0.6, 1, 0, 1, 0, 0, 0.6, 0), S_(0, 0.52, 0.42, 0.52) }, { 4, 2 } },
    { 'F', 2, { S_(0.6, 1, 0, 1, 0, 0), S_(0, 0.52, 0.42, 0.52) }, { 3, 2 } },
    { 'G', 1, { S_(0.6, 0.85, 0.45, 1, 0.15, 1, 0, 0.8, 0, 0.2, 0.15, 0, 0.45, 0, 0.6, 0.15, 0.6, 0.45, 0.32, 0.45) }, { 10 } },
    { 'H', 3, { S_(0, 0, 0, 1), S_(0.6, 0, 0.6, 1), S_(0, 0.52, 0.6, 0.52) }, { 2, 2, 2 } },
    { 'I', 3, { S_(0.3, 0, 0.3, 1), S_(0.1, 1, 0.5, 1), S_(0.1, 0, 0.5, 0) }, { 2, 2, 2 } },
    { 'J', 1, { S_(0.6, 1, 0.6, 0.2, 0.45, 0, 0.15, 0, 0, 0.2) }, { 5 } },
    { 'K', 3, { S_(0, 0, 0, 1), S_(0.6, 1, 0, 0.42), S_(0.2, 0.6, 0.6, 0) }, { 2, 2, 2 } },
    { 'L', 1, { S_(0, 1, 0, 0, 0.6, 0) }, { 3 } },
    { 'M', 1, { S_(0, 0, 0, 1, 0.3, 0.45, 0.6, 1, 0.6, 0) }, { 5 } },
    { 'N', 1, { S_(0, 0, 0, 1, 0.6, 0, 0.6, 1) }, { 4 } },
    { 'O', 1, { S_(0.15, 0, 0, 0.2, 0, 0.8, 0.15, 1, 0.45, 1, 0.6, 0.8, 0.6, 0.2, 0.45, 0, 0.15, 0) }, { 9 } },
    { 'P', 1, { GP }, { 7 } },
    { 'R', 2, { GP, S_(0.3, 0.5, 0.6, 0) }, { 7, 2 } },
    { 'S', 1, { S_(0.6, 0.85, 0.45, 1, 0.15, 1, 0, 0.85, 0, 0.64, 0.15, 0.53, 0.45, 0.47, 0.6, 0.36, 0.6, 0.15, 0.45, 0, 0.15, 0, 0, 0.15) }, { 12 } },
    { 'T', 2, { S_(0, 1, 0.6, 1), S_(0.3, 1, 0.3, 0) }, { 2, 2 } },
    { 'U', 1, { S_(0, 1, 0, 0.2, 0.15, 0, 0.45, 0, 0.6, 0.2, 0.6, 1) }, { 6 } },
    { 'V', 1, { S_(0, 1, 0.3, 0, 0.6, 1) }, { 3 } },
    { 'W', 1, { S_(0, 1, 0.14, 0, 0.3, 0.55, 0.46, 0, 0.6, 1) }, { 5 } },
    { 'X', 2, { S_(0, 0, 0.6, 1), S_(0, 1, 0.6, 0) }, { 2, 2 } },
    { 'Y', 2, { S_(0, 1, 0.3, 0.5, 0.6, 1), S_(0.3, 0.5, 0.3, 0) }, { 3, 2 } },
    { 'Z', 1, { S_(0, 1, 0.6, 1, 0, 0, 0.6, 0) }, { 4 } },
    { '1', 1, { S_(0.15, 0.8, 0.35, 1, 0.35, 0) }, { 3 } },
    { '2', 1, { S_(0, 0.8, 0.15, 1, 0.45, 1, 0.6, 0.8, 0.6, 0.62, 0, 0, 0.6, 0) }, { 7 } },
    { '3', 1, { S_(0, 1, 0.6, 1, 0.25, 0.58, 0.45, 0.55, 0.6, 0.4, 0.6, 0.15, 0.45, 0, 0.15, 0, 0, 0.15) }, { 9 } },
    { '4', 1, { S_(0.45, 0, 0.45, 1, 0, 0.32, 0.6, 0.32) }, { 4 } },
    { '-', 1, { S_(0.1, 0.5, 0.5, 0.5) }, { 2 } },
};
GG *XWORD(const char *txt, double cx, double cy, double b, double h, double w, int has_width, double width) {
    double adv = 0.88; int n = (int)strlen(txt); GL gs = { 0 }; F ff; double x0;
    if (has_width && n * adv * h - 0.28 * h > width) h = width / (n * adv - 0.28);
    ff = FR(cx, cy, b); x0 = -(n * adv - 0.28) * h / 2;
    for (int i = 0; i < n; i++) {
        const Glyph *gl = NULL;
        for (size_t j = 0; j < sizeof GLYPHS / sizeof GLYPHS[0]; j++) if (GLYPHS[j].ch == txt[i]) gl = &GLYPHS[j];
        if (!gl) continue;
        for (int k = 0; k < gl->ns; k++) {
            P pts[16];
            for (int j = 0; j < gl->np[k]; j++) pts[j] = FF(&ff, x0 + (i * adv + gl->st[k][2 * j]) * h, (gl->st[k][2 * j + 1] - 0.5) * h);
            GA(&gs, BUF(LSTR(pts, gl->np[k]), w / 2, 3));
        }
    }
    return gs.n ? UL(&gs) : NULL;
}
int XCHAIN(SoObj *o, P p, P q, const char *kind, double w, double link) {
    double L = DIST(p, q), t; int n; GL gs = { 0 }; S *s = SN(); P pq[2] = { p, q };
    if (L == 0.0) L = 1.0;
    t = ATAN2(q.y - p.y, q.x - p.x); n = (int)PINT(L / (link * 0.78)); if (n < 2) n = 2;
    for (int i = 0; i < n; i++) {
        P c = { p.x + (q.x - p.x) * (i + 0.5) / n, p.y + (q.y - p.y) * (i + 0.5) / n };
        if (i % 2 == 0) GA(&gs, BUF(EXTR(ELL(c.x, c.y, link / 2, link * 0.32, t, 12)), w / 2, 3));
        else GA(&gs, BARL(POLp(c, link * 0.42, t), POLp(c, link * 0.42, t + PI), w));
    }
    EXT(s, UL(&gs), 0.26, 0.0);
    return ST(o, s, kind, -1, NULL, 0, BUF(LSTR(pq, 2), 0.3, 16));
}

/* ------------------------------------------------------------------ x_cutfix, the builder f0, registration */
static void cutfix(const XrDef *d, SoObj *o) {
    double k = sdr_real_factor(d->style, d->name, d->cls), wmin;
    if (k == 0.0) k = 1.0;
    wmin = 0.205 / k;
    for (int i = 0; i < o->L.n; i++) {
        SoLayer *y = o->L.v[i], *z; GL ps = { 0 }, qs = { 0 };
        if (!y || y->n < 2 || y->v[0].t != LV_STR || strcmp(y->v[0].s, "tcut") || y->v[1].t != LV_GEOM || !y->v[1].g || EMPTY(y->v[1].g)) continue;
        POLYS(y->v[1].g, &qs);
        for (int j = 0; j < qs.n; j++) {
            GG *q = qs.v[j];
            for (int t = 0; t < 8; t++) {
                if (!EMPTY(BUF(q, -wmin / 2, 16))) break;
                q = BUF(q, 0.015, 3);
            }
            GA(&ps, q);
        }
        z = ly_new(y->n);
        for (int j = 0; j < y->n; j++) {
            z->v[j] = y->v[j];
            if (y->v[j].t == LV_GEOM) z->v[j].g = shp_clone(y->v[j].g);
            if (y->v[j].t == LV_LATER) z->v[j].later = so_later_ref(y->v[j].later);
        }
        shp_free(z->v[1].g); z->v[1].g = shp_clone(UL(&ps));
        ly_unref(y); o->L.v[i] = z;
    }
}
static SoObj OBJ; static const XrDef *CUR;
int xr_f0(void *ctx, double cx, double cy, double a, PyRandom *rng, int has_scale, double scale, Geo **foot, SoLayers *layers) {
    jmp_buf jb, *prev = XJ; GG *ft = NULL; int r;
    (void)scale;
    *foot = NULL;
    if (has_scale) return SC_ERR;
    CUR = (const XrDef *)ctx; memset(&OBJ, 0, sizeof OBJ); NONE_ = 0;
    XJ = &jb;
    if (setjmp(jb)) {
        XJ = prev; so_obj_free(&OBJ); arena_free();
        r = NONE_ ? 1 : SC_ERR; NONE_ = 0;
        return r;
    }
    CUR->body(cx, cy, a, rng, &OBJ, &ft);
    if (CUR->x) cutfix(CUR, &OBJ);
    XJ = prev;
    *foot = ft ? geo_new(shp_clone(ft)) : NULL;
    for (int i = 0; i < OBJ.L.n; i++) lyl_push(layers, OBJ.L.v[i]);
    so_obj_free(&OBJ); arena_free();
    return 0;
}
void xr_register(const XrDef *defs, int n) {
    for (int i = 0; i < n; i++) {
        SdrEntry e; memset(&e, 0, sizeof e);
        e.f0 = xr_f0; e.ctx = (void *)&defs[i]; e.style = defs[i].style; e.cls = defs[i].cls; e.kind = defs[i].kind; e.name = defs[i].name; e.r = defs[i].r; e.ground = defs[i].ground;
        sdr_register(&e);
    }
}

/* Python's order: sorted(glob("sd_r_*.py")); an import of another sd_r_x file runs that one first, but every file
   registers only its own style, so the order inside each style is each file's own, top to bottom. */
#define XR_PARTS(X) X(badlands) X(coast) X(corridor) X(cynder) X(dungeon) X(forest) X(fortress) X(frontier) X(graveyard)                     X(highland) X(infestation) X(maahl) X(mine) X(swamp) X(targa) X(temple) X(trederra) X(urban)
#define XR_WEAK(nm) void bld_sdrx_##nm(void) __attribute__((weak));
#define XR_RUN(nm) if (bld_sdrx_##nm) bld_sdrx_##nm();
XR_PARTS(XR_WEAK)
void bld_register_sd_rx(void) { static int done; if (done) return; done = 1; XR_PARTS(XR_RUN) }
