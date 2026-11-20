/* bld_sd_a.c: sd_a.py in C (the badlands' twenty style-only details), plus the helpers all three files share
   (bld_sd_abc.h) and bld_register_sd_abc(). Agent "bld-sdabc", 2026-11-19. Each builder names the Python one it
   copies (style / name); proof: work\cscene\bld_sdabc\prove.py. */
#include <stdio.h>
#include "bld_sd_abc.h"

/* ================================================================== the arena */
SdArena SD;
jmp_buf SD_J;

void sd_start(void) { SD.ng = SD.ns = SD.nm = 0; SD.out.n = 0; }
void sd_end(void) {
    for (int i = 0; i < SD.ng; i++) shp_free(SD.g[i]);
    for (int i = 0; i < SD.ns; i++) { sol_free(SD.s[i]); free(SD.s[i]); }
    for (int i = 0; i < SD.nm; i++) free(SD.m[i]);
    for (int i = 0; i < SD.out.n; i++) ly_unref(SD.out.v[i]);
    SD.ng = SD.ns = SD.nm = 0; SD.out.n = 0;
}
_Noreturn void sd_fail(void) { longjmp(SD_J, 1); }
int sd_ret(const GEOSGeometry *fg, Geo **out, SoLayers *layers) {
    GEOSGeometry *c = fg ? shp_clone(fg) : NULL;
    if (fg && !c) { sd_end(); return SC_ERR; }
    *out = c ? geo_new(c) : NULL;
    for (int i = 0; i < SD.out.n; i++) lyl_push_own(layers, SD.out.v[i]);          /* the arena's reference goes to the caller */
    SD.out.n = 0;
    sd_end();
    return 0;
}
#define GROW(arr, n, cap, T) do { if ((n) == (cap)) { int nc_ = (cap) ? 2 * (cap) : 64; T *t_ = (T *)realloc((arr), sizeof(T) * (size_t)nc_); if (!t_) sd_fail(); (arr) = t_; (cap) = nc_; } } while (0)
GEOSGeometry *Gk(GEOSGeometry *g) { if (!g) sd_fail(); GROW(SD.g, SD.ng, SD.capg, GEOSGeometry *); SD.g[SD.ng++] = g; return g; }
SoSolids *NS(void) { SoSolids *s = (SoSolids *)calloc(1, sizeof(SoSolids)); if (!s) sd_fail(); GROW(SD.s, SD.ns, SD.caps, SoSolids *); SD.s[SD.ns++] = s; return s; }
void *Mk(size_t n) { void *p = calloc(1, n ? n : 1); if (!p) sd_fail(); GROW(SD.m, SD.nm, SD.capm, void *); SD.m[SD.nm++] = p; return p; }
void K(int r) { if (r != SO_OK) sd_fail(); }

/* ================================================================== numbers */
double PW(double x, double y) { double re, im; int k = px_pow(x, y, &re, &im); if (k != 0) sd_fail(); return re; }
double PWR(double x, double y) { double re, im; int k = px_pow(x, y, &re, &im); if (k < 0) sd_fail(); return re; }
int RF_PW(double x, double y, double *v) { double im; return px_pow(x, y, v, &im) == 0 ? SO_OK : SO_ERR; }
double FMOD(double x, double y) { double r; if (px_mod_d(x, y, &r) != PX_OK) sd_fail(); return r; }
int64_t IMOD(int64_t x, int64_t y) { int64_t r; if (px_mod_i(x, y, &r) != PX_OK) sd_fail(); return r; }
double DIST(P2 p, P2 q) { return px_dist(&p.x, &q.x, 2); }
int64_t RI(PyRandom *r, int64_t lo, int64_t hi) { int64_t v; if (py_randint_e(r, lo, hi, &v) != PX_OK) sd_fail(); return v; }
int64_t CH(PyRandom *r, int64_t n) { int64_t v; if (py_choice_e(r, n, &v) != PX_OK) sd_fail(); return v; }

/* ================================================================== points */
PV *PVn(int cap) { PV *v = (PV *)Mk(sizeof(PV)); v->cap = cap > 4 ? cap : 4; v->xy = (double *)Mk(sizeof(double) * 2 * (size_t)v->cap); return v; }
void pv_add(PV *v, double x, double y) {
    if (v->n == v->cap) { double *n = (double *)Mk(sizeof(double) * 4 * (size_t)v->cap); memcpy(n, v->xy, sizeof(double) * 2 * (size_t)v->n); v->xy = n; v->cap *= 2; }
    v->xy[2 * v->n] = x; v->xy[2 * v->n + 1] = y; v->n++;
}
void pv_crv(PV *v, double r0, double r1, double z0, double z1) { double p[14]; K(sd_crv(r0, r1, z0, z1, 1, 6, p)); for (int i = 0; i < 7; i++) pv_add(v, p[2 * i], p[2 * i + 1]); }
PV *ARC(double x, double y, double r, double a0, double a1, int n) { PV *v = PVn(n + 1); K(sd_arc(x, y, r, a0, a1, n, v->xy)); v->n = n + 1; return v; }
PV *SPIRAL(double x, double y, double r0, double r1, double turns, double a_, int n) { PV *v = PVn(n + 1); K(sd_spiral(x, y, r0, r1, turns, a_, n, v->xy)); v->n = n + 1; return v; }
PV *WIG(P2 p, P2 q, PyRandom *r, double amp, int n) { PV *v = PVn(n + 2); v->n = lf_wiggle(p.x, p.y, q.x, q.y, r, amp, n, v->xy); return v; }
PV *RINGPTS(double x, double y, double r, int n, double a_, double jit, PyRandom *rg) { PV *v = PVn(n); K(sd_ringpts(x, y, r, n, a_, jit, rg, v->xy)); v->n = n; return v; }
DV ARANGE(double start, double stop, double step) {
    DV d; int64_t n = np_arange(start, stop, step, NULL, 0);
    if (n < 0) sd_fail();
    d.v = (double *)Mk(sizeof(double) * (size_t)(n + 1)); d.n = (int)np_arange(start, stop, step, d.v, n);
    return d;
}
DV LINSPACE(double start, double stop, int num) { DV d; d.v = (double *)Mk(sizeof(double) * (size_t)(num + 1)); if (np_linspace(start, stop, num, 1, d.v) != PX_OK) sd_fail(); d.n = num; return d; }

/* ================================================================== outlines */
GV *GVn(void) { GV *l = (GV *)Mk(sizeof(GV)); return l; }
void gv_add(GV *l, const GEOSGeometry *g) {
    if (l->n == l->cap) { int nc = l->cap ? 2 * l->cap : 16; const GEOSGeometry **v = (const GEOSGeometry **)Mk(sizeof(*v) * (size_t)nc); if (l->n) memcpy(v, l->v, sizeof(*v) * (size_t)l->n); l->v = v; l->cap = nc; }
    l->v[l->n++] = g;
}
GEOSGeometry *DISC(double x, double y, double r, int n) { return Gk(sd_disc(x, y, r, n)); }
GEOSGeometry *ANN(double x, double y, double ro, double ri, int n) { return Gk(sd_ann(x, y, ro, ri, n)); }
GEOSGeometry *CAPL(P2 p, P2 q, double w) { return Gk(sd_capl(&p.x, &q.x, w)); }
GEOSGeometry *BARL(P2 p, P2 q, double w) { return Gk(sd_barl(&p.x, &q.x, w)); }
GEOSGeometry *LINE(const PV *pts, double w) { return Gk(sd_line(pts->xy, pts->n, w)); }
GEOSGeometry *ELL(double x, double y, double rx, double ry, double a_, int n) { return Gk(sd_ell(x, y, rx, ry, a_, n)); }
GEOSGeometry *NGON(double x, double y, double r, int n, double a_) { return Gk(sd_ngon(x, y, r, n, a_)); }
GEOSGeometry *TRI(P2 p, P2 q, double w) { return Gk(sd_tri(&p.x, &q.x, w)); }
GEOSGeometry *RECT(double x, double y, double l, double w, double a_) { return Gk(shp_rect(x, y, l, w, a_)); }
GEOSGeometry *STAR(double x, double y, double ro, double ri, int n, double rot) { return Gk(shp_star(x, y, ro, ri, n, rot)); }
GEOSGeometry *STONE(double x, double y, double r, PyRandom *rg, double aspect, int nv) { return Gk(so_stone(x, y, r, rg, aspect, nv)); }
GEOSGeometry *BLOB(double x, double y, double r, PyRandom *rg, int k) {
    Geo *b = lf_blob(x, y, r, rg, k); GEOSGeometry *g;
    if (!b) sd_fail();
    g = shp_clone(b->g); geo_unref(b);
    return Gk(g);
}
GEOSGeometry *WAVY(P2 p, P2 q, PyRandom *rg, double amp, int n, double w) { return Gk(sd_wavy(&p.x, &q.x, rg, amp, n, w)); }
GEOSGeometry *POLY(const PV *pts) { return Gk(shp_polygon(pts->xy, pts->n)); }
GEOSGeometry *LSTR(const PV *pts) { return Gk(shp_linestring(pts->xy, pts->n)); }
GEOSGeometry *LBUF(const PV *pts, double d, int qs) { return Gk(shp_line_buffer(pts->xy, pts->n, d, SHP_CAP_ROUND, SHP_JOIN_ROUND, qs, 5.0)); }
GEOSGeometry *BUF(const GEOSGeometry *g, double d, int qs) { return Gk(shp_buffer_q(g, d, qs)); }
GEOSGeometry *BUFM(const GEOSGeometry *g, double d) { return Gk(shp_buffer(g, d, 16, SHP_CAP_ROUND, SHP_JOIN_MITRE, 5.0, 0)); }
GEOSGeometry *DIFF(const GEOSGeometry *x, const GEOSGeometry *y) { return Gk(shp_difference(x, y)); }
GEOSGeometry *UNI(const GEOSGeometry *x, const GEOSGeometry *y) { return Gk(shp_union(x, y)); }
GEOSGeometry *INTER(const GEOSGeometry *x, const GEOSGeometry *y) { return Gk(shp_intersection(x, y)); }
GEOSGeometry *CHULL(const GEOSGeometry *g) { return Gk(shp_convex_hull(g)); }
GEOSGeometry *INSET(const GEOSGeometry *g, double d) { return Gk(shp_inset(g, d)); }
GEOSGeometry *UL(const GV *l) { return Gk(sd_U(l->v, l->n)); }
GEOSGeometry *HL(const GV *l) { return Gk(sd_hull(l->v, l->n)); }
GEOSGeometry *EXTR(const GEOSGeometry *p) { const GEOSGeometry *e = shp_exterior(p); if (!e) sd_fail(); return Gk(shp_clone(e)); }
int EMPTY(const GEOSGeometry *g) { int e = shp_is_empty(g); if (e < 0) sd_fail(); return e; }
double GDIST(const GEOSGeometry *x, const GEOSGeometry *y) { return shp_distance(x, y); }
double AREA(const GEOSGeometry *g) { return shp_area(g); }
int CONTAINS(const GEOSGeometry *x, const GEOSGeometry *y) { int r = shp_contains(x, y); if (r < 0) sd_fail(); return r; }
GEOSGeometry *INTERP(const GEOSGeometry *line, double d, int normalized) { return Gk(shp_interpolate(line, d, normalized)); }

/* ================================================================== solids */
void CYL(SoSolids *o, double x, double y, double r, double h, double lift, int n) { K(sd_cyl(o, x, y, r, h, lift, n)); }
void BLK(SoSolids *o, double x, double y, double L, double W, double a_, double h, double lift) { K(sd_blk(o, x, y, L, W, a_, h, lift)); }
void EXT(SoSolids *o, const GEOSGeometry *poly, double h, double lift) { K(sd_ext(o, poly, h, lift)); }
void DOME(SoSolids *o, double x, double y, double r, double h, double lift, double sq, double a_, int n) { K(sd_dome(o, x, y, r, h, lift, sq, a_, n)); }
void CONE(SoSolids *o, double x, double y, double r, double h, double tip, double lift, int n, double sq, double a_) { K(sd_cone(o, x, y, r, h, tip, lift, n, sq, a_)); }
void TURN(SoSolids *o, double x, double y, const PV *prof, int n, double sq, double a_, double lift) { K(sd_turn(o, x, y, prof->xy, prof->n, n, sq, a_, lift)); }
void LIE(SoSolids *o, double x, double y, double a_, double L, double r, double lift) { K(sd_lie(o, x, y, a_, L, r, lift)); }
void RIDGE(SoSolids *o, double x, double y, double a_, double L, double W, double h, double lift) { K(sd_ridge(o, x, y, a_, L, W, h, lift)); }
void LOAF(SoSolids *o, double x, double y, double a_, double L, double W, double h, double lift) { K(sd_loaf(o, x, y, a_, L, W, h, lift)); }
void ROD(SoSolids *o, const PV *pts, double r, SoNumFn rf, void *c, double hk, double lift, double arch) { K(sd_rod(o, pts->xy, pts->n, r, rf, c, hk, lift, arch)); }
void SPIN(SoSolids *o, double x, double y, double a_, double L, SoNumFn rf, void *c, double hk, double lift, const char *prof) { K(sd_spin(o, x, y, a_, L, rf, c, hk, lift, prof)); }
void POT(SoSolids *o, double x, double y, double r, double h, double neck, double lip, double lift, int n) { K(sd_pot(o, x, y, r, h, neck, lip, lift, n)); }
void ROCK(SoSolids *o, double x, double y, double r, double h, PyRandom *rg, double sq, double lift, int n) { K(sd_rock(o, x, y, r, h, rg, sq, lift, n)); }
void FLUTED(SoSolids *o, double x, double y, double r, double h, int n, double a_, double deep, int m, double twist, double lift) { K(sd_fluted(o, x, y, r, h, n, a_, deep, m, twist, lift)); }
void HANG(SoSolids *o, const GEOSGeometry *poly, double z0, double z1) { K(sd_hang(o, poly, z0, z1)); }
void SLAB(SoSolids *o, const GEOSGeometry *poly, double z0, double z1) { K(so_slab(o, poly, z0, z1)); }
void CAT(SoSolids *o, const SoSolids *x) { sol_extend(o, x); }

/* ================================================================== layers */
CV *CVn(void) { return (CV *)Mk(sizeof(CV)); }
void cv_add(CV *c, const GEOSGeometry *q, double fl) {
    if (c->n == c->cap) { int nc = c->cap ? 2 * c->cap : 16; SoCut *v = (SoCut *)Mk(sizeof(SoCut) * (size_t)nc); if (c->n) memcpy(v, c->v, sizeof(SoCut) * (size_t)c->n); c->v = v; c->cap = nc; }
    c->v[c->n].q = q; c->v[c->n].fl = fl; c->n++;
}
static void push_layer(SoLayer *l) { if (!l) sd_fail(); lyl_push_own(&SD.out, l); }
void ST(const SoSolids *ps, const char *kind, const CV *cuts) { SoLayer *l = NULL; K(sd_ST(ps, kind, 0, 0, cuts ? cuts->v : NULL, cuts ? cuts->n : 0, &l)); push_layer(l); }
static GEOSGeometry *cl(const GEOSGeometry *g) { GEOSGeometry *c = shp_clone(g); if (!c) sd_fail(); return c; }
void LUP(const GEOSGeometry *g, double h, const char *kind) { push_layer(ly_up(cl(g), h, kind)); }
void LGCUT(const GEOSGeometry *g, double d, const char *kind) { push_layer(ly_gcut(cl(g), d, kind)); }
void LMOUND(const GEOSGeometry *g, double h, const char *kind, const FtTiers *t) {
    SoLayer *l = ly_new(5); SoVal p = lv_none();
    p.t = LV_PTR; p.p = (void *)t;
    l->v[0] = lv_str("mound"); l->v[1] = lv_geom(cl(g)); l->v[2] = lv_float(h); l->v[3] = lv_str(kind); l->v[4] = p;
    push_layer(l);
}
void LPAD(const GEOSGeometry *g, const char *kind) { SoLayer *l = ly_new(3); l->v[0] = lv_str("pad"); l->v[1] = lv_geom(cl(g)); l->v[2] = lv_str(kind); push_layer(l); }

void sd_reg(FtBuildFn f, const char *style, const char *cls, const char *kind, const char *name, int ground) {
    SdrEntry e; e.f0 = f; e.ctx = NULL; e.style = style; e.cls = cls; e.kind = kind; e.name = name; e.r = 10.5; e.ground = ground;
    sdr_register(&e);
}

/* style_details._parts(): sd_a, sd_b, sd_c (then sd_d, sd_e: bld_register_sd_de; sd_r ...: theirs) */
void bld_register_sd_abc(void) { bld_register_sd_a(); bld_register_sd_b(); bld_register_sd_c(); }

/* ====================================================================================================== badlands */
/* badlands / saguaro */
static int b_saguaro(SD_ARGS) {
    SD_BEGIN
    double r = u(0.75, 1.0), h = u(3.0, 4.4); SoSolids *ps = NS(); PV *pf = PVn(16); int64_t k, idx[2]; static const double SV[2] = { 0.0, SD_PI };
    pv_add(pf, r, 0.0); pv_add(pf, r, h - r); pv_crv(pf, r, 0.24, h - r, h); TURN(ps, cx, cy, pf, 14, 1.0, 0.0, 0.0);
    k = RI(rng, 1, 2); K(py_sample_e(rng, 2, k, idx));
    for (int j = 0; j < k; j++) {
        double sv = SV[idx[j]], t = a + sv; t = t + u(-0.5, 0.5);
        double ra = r * 0.7, ha = h * u(0.5, 0.8); P2 e = POL(cx, cy, r + u(0.9, 1.4), t);
        PV *rp = PVn(2); pv_p(rp, POL(cx, cy, r * 0.5, t)); pv_p(rp, e);
        double lift = h * u(0.15, 0.3);
        ROD(ps, rp, ra, NULL, NULL, 1.0, lift, 0.0);
        PV *p2_ = PVn(16); pv_add(p2_, ra, 0.0); pv_add(p2_, ra, ha - ra); pv_crv(p2_, ra, 0.24, ha - ra, ha); TURN(ps, e.x, e.y, p2_, 12, 1.0, 0.0, 0.0);
    }
    GEOSGeometry *f = DISC(cx, cy, r + 2.2, 20);
    ST(ps, "cactus", NULL);
    SD_RET(f);
}

/* badlands / barrel_cactus */
static int b_barrel_cactus(SD_ARGS) {
    SD_BEGIN
    SoSolids *ps = NS(); GV *foot_ = GVn(); int64_t n = RI(rng, 1, 3);
    for (int64_t i = 0; i < n; i++) {
        double rr = i == 0 ? 0.0 : u(1.9, 2.4); P2 p = POL(cx, cy, rr, a + (double)i * 2.4);
        double r = u(0.9, 1.25) * (i == 0 ? 1.0 : 0.75);
        double h = r * u(1.0, 1.5); int64_t nn = RI(rng, 8, 11); double aa = u(0, 1);
        FLUTED(ps, p.x, p.y, r, h, (int)nn, aa, 0.78, 6, 0.0, 0.0); gv_add(foot_, DISC(p.x, p.y, r + 0.3, 20));
    }
    GEOSGeometry *f = HL(foot_);
    ST(ps, "cactus", NULL);
    SD_RET(f);
}

/* badlands / prickly_pear */
static int b_prickly_pear(SD_ARGS) {
    SD_BEGIN
    SoSolids *ps = NS(); GV *foot_ = GVn(); int64_t n = RI(rng, 3, 5);
    for (int64_t i = 0; i < n; i++) {
        double r0 = u(0, 1.5), t0 = u(0, 6.28); P2 p = POL(cx, cy, r0, t0);
        double L = u(1.5, 2.3), t = u(0, 3.14), h = u(1.2, 2.2);
        LOAF(ps, p.x, p.y, t, L, 0.62, h, 0.0);
        if (RND() < 0.5) { P2 q = POL(p.x, p.y, L * 0.3, t); double tt = t + u(-0.6, 0.6); LOAF(ps, q.x, q.y, tt, L * 0.7, 0.6, h * 0.6, h * 0.7); }
        gv_add(foot_, DISC(p.x, p.y, L / 2 + 0.3, 20));
    }
    GEOSGeometry *f = HL(foot_);
    ST(ps, "cactus", NULL);
    SD_RET(f);
}

/* badlands / longhorn_skull */
typedef struct { double k; } CtxK;
static int rf_longhorn_body(void *c, double t, double *o) { double k = ((CtxK *)c)->k; *o = k * (0.62 - 0.17 * t / k); return SO_OK; }
static int rf_longhorn_horn(void *c, double q, double *o) { double k = ((CtxK *)c)->k; *o = 0.42 * k * (1 - 0.55 * q); return SO_OK; }
static int b_longhorn_skull(SD_ARGS) {
    SD_BEGIN
    SoFrame ff = so_frame(cx, cy, a); double k = u(0.9, 1.25), sweep = u(0.2, 1.0); CtxK ck = { k }; SoSolids *ps = NS(); GV *tips = GVn(); CV *cuts = CVn();
    SPIN(ps, cx, cy, a, 3.0 * k, rf_longhorn_body, &ck, 0.95, 0.0, "round");
    for (int s = -1; s <= 1; s += 2) {
        double sv = s;
        if (RND() < 0.12) continue;
        PV *pts = PVn(3); pv_p(pts, FF(&ff, -0.9 * k, sv * 0.4 * k)); pv_p(pts, FF(&ff, -1.2 * k, sv * 1.7 * k)); pv_p(pts, FF(&ff, -1.2 * k + sweep * k, sv * 3.0 * k));
        gv_add(tips, DISC(pts->xy[4], pts->xy[5], 0.3, 20));
        ROD(ps, pts, 0, rf_longhorn_horn, &ck, 1.0, 0.0, 0.0);
    }
    for (int s = -1; s <= 1; s += 2) { double sv = s; P2 p = FF(&ff, -0.45 * k, sv * 0.42 * k); cv_add(cuts, DISC(p.x, p.y, 0.27 * k, 10), 0.3); }
    GV *hl = GVn(); gv_add(hl, RECT(cx, cy, 3.2 * k, 1.9 * k, a)); for (int i = 0; i < tips->n; i++) gv_add(hl, tips->v[i]);
    GEOSGeometry *f = HL(hl);
    ST(ps, "bone", cuts);
    SD_RET(f);
}

/* badlands / tumbleweed */
static int b_tumbleweed(SD_ARGS) {
    SD_BEGIN
    double r = u(1.2, 1.9); GEOSGeometry *f = DISC(cx, cy, r + 0.3, 20); SoSolids *ps = NS();
    double h = r * u(1.0, 1.3); int64_t n = RI(rng, 9, 13); double aa = u(0, 1), deep = u(0.62, 0.75), tw = u(0.25, 0.6); tw = tw * CHPM(rng);
    FLUTED(ps, cx, cy, r, h, (int)n, aa, deep, 7, tw, 0.0);
    ST(ps, "shrub", NULL);
    SD_RET(f);
}

/* badlands / hitching_rail */
static int b_hitching_rail(SD_ARGS) {
    SD_BEGIN
    SoFrame ff = so_frame(cx, cy, a); double L = u(5.0, 7.0), h = u(2.2, 2.8), lean = u(-0.25, 0.25); SoSolids *ps = NS(); P2 p;
    p = FF(&ff, -L / 2, 0); CYL(ps, p.x, p.y, 0.5, h + 0.3, 0.0, 10);
    p = FF(&ff, L / 2, lean); CYL(ps, p.x, p.y, 0.5, h + 0.3, 0.0, 10);
    HANG(ps, BARL(FF(&ff, -L / 2 - 0.4, 0), FF(&ff, L / 2 + 0.4, lean), 0.62), h - 0.65, h);
    ST(ps, "wood", NULL);
    if (RND() < 0.5) { double u1 = u(-1, 1); P2 p1 = FF(&ff, u1, 0.9); double u2 = u(-1, 1); P2 p2_ = FF(&ff, u2, 2.4); LUP(CAPL(p1, p2_, 0.95), 0.4, "leather"); }
    p = FF(&ff, 0, 0.6);
    SD_RET(RECT(p.x, p.y, L + 1.4, 3.4, a));
}

/* badlands / water_trough */
static int b_water_trough(SD_ARGS) {
    SD_BEGIN
    SoFrame ff = so_frame(cx, cy, a); double L = u(4.6, 6.4), W = u(1.9, 2.3), h = u(1.2, 1.6); SoSolids *ps = NS(); P2 p; CV *cuts = CVn();
    BLK(ps, cx, cy, L, W, a, h, 0.0);
    p = FF(&ff, -L * 0.34, 0); BLK(ps, p.x, p.y, 0.7, W + 0.7, a, h * 0.7, 0.0);
    p = FF(&ff, L * 0.34, 0); BLK(ps, p.x, p.y, 0.7, W + 0.7, a, h * 0.7, 0.0);
    GEOSGeometry *f = RECT(cx, cy, L + 0.3, W + 0.9, a), *q = RECT(cx, cy, L - 0.9, W - 0.9, a);
    cv_add(cuts, q, h - u(0.35, 0.6));
    ST(ps, "wood", cuts);
    SD_RET(f);
}

/* badlands / ox_yoke */
typedef struct { double L; } CtxL;
static int rf_ox_yoke(void *c, double t, double *o) { double L = ((CtxL *)c)->L; *o = 0.5 + 0.18 * COS(t / L * 12.6); return SO_OK; }
static int b_ox_yoke(SD_ARGS) {
    SD_BEGIN
    SoFrame ff = so_frame(cx, cy, a); double L = u(4.6, 5.8), s = L * 0.27; CtxL cl_ = { L }; SoSolids *ps = NS(); P2 p;
    SPIN(ps, cx, cy, a, L, rf_ox_yoke, &cl_, 1.1, 0.0, "round");
    for (int sv = -1; sv <= 1; sv += 2) { p = FF(&ff, sv * s, 1.0); EXT(ps, ANN(p.x, p.y, 0.95, 0.42, 16), 0.5, 0.0); }
    p = FF(&ff, 0, 0.6); GEOSGeometry *f = RECT(p.x, p.y, L + 0.3, 3.2, a);
    ST(ps, "wood", NULL);
    SD_RET(f);
}

/* badlands / rattlesnake */
static int rf_rattlesnake(void *c, double q, double *o) { (void)c; *o = 0.24 + 0.2 * pmin(1.0, q * 4); return SO_OK; }
static int b_rattlesnake(SD_ARGS) {
    SD_BEGIN
    double R = u(2.0, 2.6), tu = u(1.7, 2.2); tu = tu * CHPM(rng); PV *pts = SPIRAL(cx, cy, R, 0.45, tu, a, 40); SoSolids *ps = NS();
    ROD(ps, pts, 0, rf_rattlesnake, NULL, 1.0, 0.0, 0.0);
    DOME(ps, pts->xy[2 * (pts->n - 1)], pts->xy[2 * (pts->n - 1) + 1], 0.5, 0.95, 0.0, 0.75, a, 20);
    GEOSGeometry *f = DISC(cx, cy, R + 0.5, 20);
    ST(ps, "hide", NULL);
    SD_RET(f);
}

/* badlands / saddle */
static int b_saddle(SD_ARGS) {
    SD_BEGIN
    SoFrame ff = so_frame(cx, cy, a); double L = u(3.0, 3.6), W = u(2.2, 2.7), h = u(0.9, 1.2); SoSolids *ps = NS(); P2 p;
    LOAF(ps, cx, cy, a, L, W, h, 0.0);
    p = FF(&ff, L / 2 - 0.4, 0); CYL(ps, p.x, p.y, 0.38, h + 0.75, 0.0, 10);
    p = FF(&ff, -L / 2 + 0.35, 0); LOAF(ps, p.x, p.y, a + SD_PI / 2, W * 0.8, 0.7, h + 0.5, 0.0);
    ST(ps, "leather", NULL);
    for (int sv = -1; sv <= 1; sv += 2) {
        if (!(RND() < 0.8)) continue;
        P2 p1 = FF(&ff, 0.2, sv * W / 2); double uu = 0.2 + u(-0.6, 0.6); P2 p2_ = FF(&ff, uu, sv * (W / 2 + 1.4));
        LUP(CAPL(p1, p2_, 0.95), 0.35, "leather");
    }
    SD_RET(RECT(cx, cy, L + 0.4, W + 3.0, a));
}

/* badlands / bedroll_canteen */
static int b_bedroll_canteen(SD_ARGS) {
    SD_BEGIN
    SoFrame ff = so_frame(cx, cy, a); double L = u(3.0, 3.8), r = u(0.75, 0.95); SoSolids *ps = NS(), *q = NS(); P2 p;
    LIE(ps, cx, cy, a, L, r, 0.0);
    p = FF(&ff, -L * 0.28, 0); LIE(ps, p.x, p.y, a, 0.45, r + 0.12, 0.0);
    p = FF(&ff, L * 0.28, 0); LIE(ps, p.x, p.y, a, 0.45, r + 0.12, 0.0);
    double cu = u(-1, 1); P2 c = FF(&ff, cu, r + 1.5); double b = u(0, 6.28);
    DOME(q, c.x, c.y, 0.95, 0.6, 0.0, 1.0, 0.0, 20);
    p = POL(c.x, c.y, 1.05, b); BLK(q, p.x, p.y, 0.6, 0.5, b, 0.45, 0.0);
    GV *hl = GVn(); gv_add(hl, RECT(cx, cy, L, 2 * r + 0.3, a)); gv_add(hl, DISC(c.x, c.y, 1.5, 20));
    GEOSGeometry *f = HL(hl);
    ST(ps, "cloth", NULL); ST(q, "tin", NULL);
    SD_RET(f);
}

/* badlands / hat */
static int b_hat(SD_ARGS) {
    SD_BEGIN
    double R = u(1.6, 2.0), rc = R * u(0.45, 0.55), h = u(1.1, 1.5); SoSolids *ps = NS(); PV *pf = PVn(16); CV *cuts = CVn();
    pv_add(pf, R, 0.0); pv_add(pf, R, 0.28); pv_add(pf, rc + 0.12, 0.3); pv_add(pf, rc + 0.12, 0.55); pv_add(pf, rc, 0.58); pv_add(pf, rc, h * 0.8);
    pv_crv(pf, rc, rc * 0.55, h * 0.8, h);
    double sq = u(0.78, 0.9); TURN(ps, cx, cy, pf, 24, sq, a, 0.0);
    GEOSGeometry *f = DISC(cx, cy, R + 0.2, 20);
    cv_add(cuts, CAPL(POL(cx, cy, rc * 0.4, a), POL(cx, cy, rc * 0.4, a + SD_PI), 0.4), h - 0.22);
    ST(ps, "leather", cuts);
    SD_RET(f);
}

/* badlands / arrows */
static int b_arrows(SD_ARGS) {
    SD_BEGIN
    GV *gs = GVn(); int64_t n = RI(rng, 2, 4); SoSolids *ps = NS();
    for (int64_t i = 0; i < n; i++) {
        double x = cx + u(-1.2, 1.2), y = cy + u(-1.2, 1.2), b = a + u(-0.7, 0.7); SoFrame f2 = so_frame(x, y, b);
        double L = u(3.6, 4.6); L = L * (RND() < 0.25 ? 0.55 : 1.0);
        GV *one = GVn(); PV *pg = PVn(5);
        gv_add(one, BARL(FF(&f2, -L / 2, 0), FF(&f2, L / 2, 0), 0.5));
        gv_add(one, TRI(FF(&f2, L / 2 - 0.1, 0), FF(&f2, L / 2 + 0.9, 0), 0.95));
        pv_p(pg, FF(&f2, -L / 2 + 0.1, -0.45)); pv_p(pg, FF(&f2, -L / 2 + 1.0, 0)); pv_p(pg, FF(&f2, -L / 2 + 0.1, 0.45)); pv_p(pg, FF(&f2, -L / 2 - 0.25, 0.45)); pv_p(pg, FF(&f2, -L / 2 - 0.25, -0.45));
        gv_add(one, POLY(pg));
        gv_add(gs, UL(one));
    }
    GEOSGeometry *g = UL(gs), *f = CHULL(g);
    EXT(ps, g, 0.42, 0.0); ST(ps, "wood", NULL);
    SD_RET(f);
}

/* badlands / cairn */
static int b_cairn(SD_ARGS) {
    SD_BEGIN
    double r = u(1.3, 1.7), z = 0.0, x = cx, y = cy; SoSolids *ps = NS(); int64_t n = RI(rng, 3, 5);
    for (int64_t j = 0; j < n; j++) {
        double h = r * u(0.55, 0.75); ROCK(ps, x, y, r, h, rng, 0.0, z, 9); z += h * 0.72; r *= u(0.68, 0.82); x += u(-0.15, 0.15); y += u(-0.15, 0.15);
        if (r < 0.45) break;
    }
    GEOSGeometry *f = DISC(cx, cy, 1.9, 20);
    ST(ps, "rock", NULL);
    SD_RET(f);
}

/* badlands / horseshoes */
static int b_horseshoes(SD_ARGS) {
    SD_BEGIN
    GV *gs = GVn(); int64_t n = RI(rng, 1, 3); SoSolids *ps = NS();
    for (int64_t i = 0; i < n; i++) {
        double rr = i == 0 ? 0.0 : u(1.7, 2.2), t = a + (double)i * 2.3; t = t + u(-0.4, 0.4); P2 p = POL(cx, cy, rr, t);
        double b = u(0, 6.28), r = u(0.75, 0.95);
        gv_add(gs, LINE(ARC(p.x, p.y, r, b, b + 4.5, 12), 0.55));
    }
    GEOSGeometry *g = UL(gs), *f = BUF(CHULL(g), 0.2, 16);
    EXT(ps, g, 0.4, 0.0); ST(ps, "iron", NULL);
    SD_RET(f);
}

/* badlands / antlers */
static int rf_antler_beam(void *c, double q, double *o) { (void)c; *o = 0.42 - 0.2 * q; return SO_OK; }
static int rf_antler_tine(void *c, double t, double *o) { (void)c; *o = 0.32 - 0.1 * t; return SO_OK; }
static int b_antlers(SD_ARGS) {
    SD_BEGIN
    SoSolids *ps = NS(); GV *foot_ = GVn(); int64_t m = RI(rng, 1, 2);
    for (int s = 0; s < m; s++) {
        double sv = s == 0 ? -1.0 : 1.0, b = a + sv * u(0.25, 0.6); SoFrame f2 = so_frame(cx, cy + sv * 0.2, b);
        double L = u(3.4, 4.4); PV *beam = PVn(4);
        pv_p(beam, FF(&f2, 0, 0)); pv_p(beam, FF(&f2, L * 0.4, sv * 0.5)); pv_p(beam, FF(&f2, L * 0.75, sv * 0.4)); pv_p(beam, FF(&f2, L, sv * -0.3));
        ROD(ps, beam, 0, rf_antler_beam, NULL, 1.0, 0.0, 0.0);
        int64_t nk = RI(rng, 2, 4);
        for (int64_t k = 0; k < nk; k++) {
            double q = 0.25 + 0.22 * (double)k; GEOSGeometry *pt = INTERP(LSTR(beam), q, 1); double px = shp_x(pt), py = shp_y(pt);
            double ex = px + u(-0.3, 0.5), ey = py + sv * u(0.9, 1.5); PV *rp = PVn(2); pv_add(rp, px, py); pv_add(rp, ex, ey);
            ROD(ps, rp, 0, rf_antler_tine, NULL, 1.0, 0.0, 0.0); gv_add(foot_, DISC(ex, ey, 0.3, 20));
        }
        gv_add(foot_, LBUF(beam, 0.5, 16));
    }
    GEOSGeometry *f = HL(foot_);
    ST(ps, "bone", NULL);
    SD_RET(f);
}

/* badlands / gold_pan */
static int b_gold_pan(SD_ARGS) {
    SD_BEGIN
    double R = u(1.5, 1.9), h = u(0.55, 0.7); SoSolids *ps = NS(); PV *pf = PVn(2); CV *cuts = CVn();
    pv_add(pf, R * 0.62, 0.0); pv_add(pf, R, h); TURN(ps, cx, cy, pf, 24, 1.0, 0.0, 0.0);
    cv_add(cuts, DISC(cx, cy, R - 0.45, 20), 0.22); ST(ps, "tin", cuts);
    int64_t n = RI(rng, 2, 5);
    for (int64_t i = 0; i < n; i++) {
        double rr = R + u(0.6, 1.3), t = a + u(-1.2, 1.2); P2 p = POL(cx, cy, rr, t);
        GEOSGeometry *st = STONE(p.x, p.y, u(0.45, 0.6), rng, 0.8, 5); double hh = u(0.3, 0.45);
        LMOUND(st, hh, "gold", SD_PEB_T);
    }
    SD_RET(DISC(cx, cy, R + 1.5, 20));
}

/* badlands / petrified_log */
static int b_petrified_log(SD_ARGS) {
    SD_BEGIN
    SoFrame ff = so_frame(cx, cy, a); double r = u(0.8, 1.1), x = -3.2; SoSolids *ps = NS(); GV *foot_ = GVn(); int64_t n = RI(rng, 3, 4);
    for (int64_t i = 0; i < n; i++) {
        double L = u(1.2, 2.2), v = u(-0.35, 0.35); P2 c = FF(&ff, x + L / 2, v); double b = a + u(-0.3, 0.3);
        double rr = r * u(0.9, 1.0); LIE(ps, c.x, c.y, b, L, rr, 0.0); gv_add(foot_, RECT(c.x, c.y, L, 2 * r, b)); x += L + u(0.3, 0.8);
    }
    GEOSGeometry *f = HL(foot_);
    ST(ps, "sandstone", NULL);
    SD_RET(f);
}

/* badlands / anthill */
static int b_anthill(SD_ARGS) {
    SD_BEGIN
    double r = u(1.7, 2.4), h = u(0.8, 1.2); SoSolids *ps = NS(); PV *pf = PVn(3); CV *cuts = CVn();
    pv_add(pf, r, 0.0); pv_add(pf, r * 0.55, h * 0.6); pv_add(pf, 0.6, h);
    double sq = u(0.8, 1.0); TURN(ps, cx, cy, pf, 18, sq, a, 0.0);
    cv_add(cuts, DISC(cx, cy, 0.32, 10), h - 0.5); ST(ps, "earth", cuts);
    SD_RET(DISC(cx, cy, r + 0.2, 20));
}

/* badlands / sidewinder_tracks */
static int b_sidewinder_tracks(SD_ARGS) {
    SD_BEGIN
    SoFrame ff = so_frame(cx, cy, a); int64_t n = RI(rng, 4, 7); GV *gs = GVn();
    for (int64_t i = 0; i < n; i++) {
        double x = ((double)i - (double)n / 2) * 1.25; PV *pl = PVn(4);
        pv_p(pl, FF(&ff, x - 0.5, -1.1)); pv_p(pl, FF(&ff, x, -0.4)); pv_p(pl, FF(&ff, x + 0.15, 0.5)); pv_p(pl, FF(&ff, x + 0.8, 1.1));
        gv_add(gs, LINE(pl, 0.42));
    }
    GEOSGeometry *f = RECT(cx, cy, (double)n * 1.25 + 1.0, 2.8, a);
    LGCUT(UL(gs), 0.3, "rut");
    SD_RET(f);
}

void bld_register_sd_a(void) {
    sd_reg(b_saguaro, "badlands", "growth", "cactus", "saguaro", 0);
    sd_reg(b_barrel_cactus, "badlands", "growth", "cactus", "barrel_cactus", 0);
    sd_reg(b_prickly_pear, "badlands", "growth", "cactus", "prickly_pear", 0);
    sd_reg(b_longhorn_skull, "badlands", "made", "bone", "longhorn_skull", 0);
    sd_reg(b_tumbleweed, "badlands", "growth", "shrub", "tumbleweed", 0);
    sd_reg(b_hitching_rail, "badlands", "made", "wood", "hitching_rail", 0);
    sd_reg(b_water_trough, "badlands", "made", "wood", "water_trough", 0);
    sd_reg(b_ox_yoke, "badlands", "made", "wood", "ox_yoke", 0);
    sd_reg(b_rattlesnake, "badlands", "made", "hide", "rattlesnake", 0);
    sd_reg(b_saddle, "badlands", "made", "leather", "saddle", 0);
    sd_reg(b_bedroll_canteen, "badlands", "made", "cloth", "bedroll_canteen", 0);
    sd_reg(b_hat, "badlands", "made", "leather", "hat", 0);
    sd_reg(b_arrows, "badlands", "made", "wood", "arrows", 0);
    sd_reg(b_cairn, "badlands", "debris", "rock", "cairn", 0);
    sd_reg(b_horseshoes, "badlands", "made", "iron", "horseshoes", 0);
    sd_reg(b_antlers, "badlands", "made", "bone", "antlers", 0);
    sd_reg(b_gold_pan, "badlands", "made", "tin", "gold_pan", 0);
    sd_reg(b_petrified_log, "badlands", "debris", "sandstone", "petrified_log", 0);
    sd_reg(b_anthill, "badlands", "debris", "earth", "anthill", 0);
    sd_reg(b_sidewinder_tracks, "badlands", "marks", "rut", "sidewinder_tracks", 0);
}
