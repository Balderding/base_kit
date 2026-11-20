/* bld_sdde.h: the helpers shared by the C copies of the style-only details in sd_d.py, sd_e.py, sd_r.py, sd_r_<style>.py
   and sd_r_models*.py (cport\bld_sd_d.c, bld_sd_e.c, bld_sd_r.c, bld_sd_models.c; agent "bld-sdde", 2026-11-19).

   A builder `_`(cx, cy, a, rng) -> (footprint, layers) is written here as
       BLD(name) { ... RET(foot); }
   and registered with sdr_register in Python's order (see bld_sd_*.c). Inside a builder:
   - every geometry made through these helpers lives in a per-call arena (freed when the builder returns); results
     handed out (the footprint, layer geometries) are copies;
   - the random stream is the builder's rng: u(a, b) = rng.uniform, rnd() = rng.random, ri(a, b) = rng.randint ...
   - a Python exception is the arena's error flag: once it is set, no helper draws a random number or makes anything
     any more, so the rng stops exactly where Python's raise stopped it, and the builder gives SC_ERR.
   - C evaluates function arguments in no fixed order: an expression never holds two random draws (each draw is its
     own statement, in the Python's order).
   Python semantics kept by hand in the builders: true division (2.0, never int / int), max/min keep the first of
   equals (MX / MN), ** through fpw (float_pow), math.* through the UCRT copies (py_cos, py_sin, py_atan2 ...). */
#ifndef BLD_SDDE_H
#define BLD_SDDE_H
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "features.h"
#include "leaf.h"
#include "pyexact.h"

#define SDU __attribute__((unused)) static
typedef GEOSGeometry GG;
typedef struct { double x, y; } P2;                       /* a point tuple (x, y) */
typedef struct { double *xy; int n, cap; } PT;            /* a list of points */
typedef struct { const GG **v; int n, cap; } GL;           /* a list of geometries (arena owned) */
typedef struct { SoCut *v; int n, cap; } CL;              /* a cuts list: (polygon, floor) */
#define NOP ((int64_t)-1)                                 /* par = None */
#define PI_ 3.141592653589793                             /* math.pi */

/* ------------------------------------------------------------------ the per-call arena */
typedef struct BA {
    PyRandom *rng; int err;
    GG **g; int ng, capg;
    SoSolids **s; int ns, caps;
    void **m; int nm, capm;
    SoObj **o; int no, capo;
    SoLayer **y; int ny, capy;
} BA;
extern _Thread_local struct BA *bsdde_ba;                             /* the builder running now (bld_sd_models.c) */
#define BA_ bsdde_ba
#define E_ (BA_->err)
#define GROW(p, n, cap) do { if ((n) == (cap)) { (cap) = (cap) ? 2 * (cap) : 16; (p) = realloc((p), sizeof(*(p)) * (size_t)(cap)); } } while (0)
SDU void fail(void) { BA_->err = 1; }
SDU GG *gk(GG *g) { if (!g) { BA_->err = 1; return NULL; } GROW(BA_->g, BA_->ng, BA_->capg); BA_->g[BA_->ng++] = g; return g; }   /* keep: NULL = raised */
SDU void *mk(size_t n) { void *p = calloc(1, n ? n : 1); GROW(BA_->m, BA_->nm, BA_->capm); BA_->m[BA_->nm++] = p; return p; }
SDU int ck(int r) { if (r < 0) BA_->err = 1; return r; }   /* an SO_OK / index result: < 0 = raised */
int bsdde_run(int (*body)(double, double, double, SoLayers *, GG **), double cx, double cy, double a, PyRandom *rng, Geo **foot, SoLayers *lay);
#define BLD(fn) static int fn##_b(double cx, double cy, double a, SoLayers *lay, GG **foot_); \
    static int fn(void *ctx, double cx, double cy, double a, PyRandom *rng, int hs, double sc, Geo **foot, SoLayers *lay) { \
        (void)ctx; (void)hs; (void)sc; return bsdde_run(fn##_b, cx, cy, a, rng, foot, lay); } \
    static int fn##_b(double cx, double cy, double a, SoLayers *lay, GG **foot_)
#define RET(g) do { *foot_ = (g); return 0; } while (0)    /* return (g, lay) */
#define RETNONE do { return 1; } while (0)                 /* return None */

/* ------------------------------------------------------------------ random */
SDU double u(double a, double b) { return E_ ? 0.0 : py_uniform(BA_->rng, a, b); }
SDU double rnd(void) { return E_ ? 0.0 : py_random(BA_->rng); }
SDU int ri(int a, int b) { int64_t o; if (E_) return 0; if (py_randint_e(BA_->rng, a, b, &o)) { fail(); return 0; } return (int)o; }
SDU int rch(int n) { int64_t o; if (E_) return 0; if (py_choice_e(BA_->rng, n, &o)) { fail(); return 0; } return (int)o; }   /* rng.choice: the index */
SDU double rchd(int n, ...) { va_list ap; double v = 0; int k = rch(n); va_start(ap, n); for (int i = 0; i <= k && i < n; i++) v = va_arg(ap, double); va_end(ap); return v; }   /* rng.choice((x, y, ...)) of floats */
SDU int rchi(int n, ...) { va_list ap; int v = 0; int k = rch(n); va_start(ap, n); for (int i = 0; i <= k && i < n; i++) v = va_arg(ap, int); va_end(ap); return v; }   /* of ints */
SDU int rsample(int n, int k, int64_t *out) { if (E_) return 0; if (py_sample_e(BA_->rng, n, k, out)) { fail(); return 0; } return k; }   /* rng.sample(range(n), k): indices */

/* ------------------------------------------------------------------ Python maths */
#define MX(a, b) mx_((a), (b))
#define MN(a, b) mn_((a), (b))
SDU double mx_(double a, double b) { return b > a ? b : a; }           /* max(a, b): the first of equals */
SDU double mn_(double a, double b) { return b < a ? b : a; }
SDU double fpw(double x, double y) { int e = 0; double r = lf_fpow(x, y, &e); if (e) fail(); return r; }   /* x ** y (floats) */
SDU double fsq(double x) { if (x < 0) { fail(); return 0.0; } return sqrt(x); }   /* math.sqrt (ValueError below 0) */
SDU double fdiv(double a, double b) { if (b == 0.0) { fail(); return 0.0; } return a / b; }   /* a / b (ZeroDivisionError) */
SDU double fmodp(double a, double b) { double o; if (px_mod_d(a, b, &o)) { fail(); return 0.0; } return o; }   /* a % b */
SDU double fdist(P2 p, P2 q) { return px_dist(&p.x, &q.x, 2); }       /* math.dist */
#define COS(x) py_cos(x)
#define SIN(x) py_sin(x)
#define ATAN2(y, x) py_atan2((y), (x))

/* ------------------------------------------------------------------ points */
SDU P2 p2(double x, double y) { P2 p = { x, y }; return p; }
SDU P2 F(const SoFrame *f, double uu, double vv) { P2 p; so_ff(f, uu, vv, &p.x, &p.y); return p; }   /* ff(u, v) */
SDU SoFrame frame(double cx, double cy, double a) { return so_frame(cx, cy, a); }
SDU P2 pol(double x, double y, double r, double t) { P2 p; sd_pol(x, y, r, t, &p.x); return p; }
SDU PT *pl(void) { return (PT *)mk(sizeof(PT)); }
SDU void pl_add(PT *l, P2 p) { if (l->n + 1 > l->cap) { l->cap = l->cap ? 2 * l->cap : 16; l->xy = realloc(l->xy, sizeof(double) * 2 * (size_t)l->cap); } l->xy[2 * l->n] = p.x; l->xy[2 * l->n + 1] = p.y; l->n++; }
SDU PT *plv(int n, ...) { PT *l = pl(); va_list ap; va_start(ap, n); for (int i = 0; i < n; i++) pl_add(l, va_arg(ap, P2)); va_end(ap); return l; }
SDU P2 pl_at(const PT *l, int i) { if (i < 0) i += l->n; if (i < 0 || i >= l->n) { fail(); return p2(0, 0); } return p2(l->xy[2 * i], l->xy[2 * i + 1]); }   /* l[i] (IndexError) */
SDU PT *pl_slice(const PT *l, int i0, int i1) { PT *o = pl(); if (i0 < 0) i0 += l->n; if (i1 < 0) i1 += l->n; if (i0 < 0) i0 = 0; if (i1 > l->n) i1 = l->n; for (int i = i0; i < i1; i++) pl_add(o, p2(l->xy[2 * i], l->xy[2 * i + 1])); return o; }   /* l[i0:i1] */
SDU PT *pl_cat(const PT *a, const PT *b) { PT *o = pl_slice(a, 0, a->n); for (int i = 0; i < b->n; i++) pl_add(o, p2(b->xy[2 * i], b->xy[2 * i + 1])); return o; }
SDU PT *arc(double x, double y, double r, double a0, double a1, int n) { PT *l = pl(); if (E_) return l; l->xy = malloc(sizeof(double) * 2 * (size_t)(n + 1)); l->cap = n + 1; if (ck(sd_arc(x, y, r, a0, a1, n, l->xy)) == 0) l->n = n + 1; return l; }
SDU PT *spiral(double x, double y, double r0, double r1, double turns, double a, int n) { PT *l = pl(); if (E_) return l; l->xy = malloc(sizeof(double) * 2 * (size_t)(n + 1)); l->cap = n + 1; if (ck(sd_spiral(x, y, r0, r1, turns, a, n, l->xy)) == 0) l->n = n + 1; return l; }
SDU PT *ringpts(double x, double y, double r, int n, double a, double jit, int with_rng) { PT *l = pl(); if (E_ || n <= 0) return l; l->xy = malloc(sizeof(double) * 2 * (size_t)n); l->cap = n; sd_ringpts(x, y, r, n, a, jit, with_rng ? BA_->rng : NULL, l->xy); l->n = n; return l; }
SDU PT *wiggle(P2 p, P2 q, double amp, int n) { PT *l = pl(); if (E_) return l; l->xy = malloc(sizeof(double) * 2 * (size_t)(n + 2)); l->cap = n + 2; l->n = lf_wiggle(p.x, p.y, q.x, q.y, BA_->rng, amp, n, l->xy); return l; }   /* gg.wiggle(p, q, rng, amp, n) */

/* ------------------------------------------------------------------ geometry lists */
SDU GL *gl(void) { return (GL *)mk(sizeof(GL)); }
SDU void gl_add(GL *l, const GG *g) { if (l->n == l->cap) { l->cap = l->cap ? 2 * l->cap : 16; l->v = realloc(l->v, sizeof(*l->v) * (size_t)l->cap); } l->v[l->n++] = g; }
SDU GL *glv(int n, ...) { GL *l = gl(); va_list ap; va_start(ap, n); for (int i = 0; i < n; i++) gl_add(l, va_arg(ap, const GG *)); va_end(ap); return l; }
SDU GG *U(const GL *l) { return E_ ? NULL : gk(sd_U(l->v, l->n)); }                         /* U(gs) */
SDU GG *hull(const GL *l) { return E_ ? NULL : gk(sd_hull(l->v, l->n)); }                   /* hull(gs) */
SDU GG *uu(const GL *l) { return E_ ? NULL : gk(shp_union_all(l->v, l->n)); }               /* unary_union(gs) as it is */

/* ------------------------------------------------------------------ outlines (style_details / gg / gg3) */
SDU GG *disc(double x, double y, double r, int n) { return E_ ? NULL : gk(sd_disc(x, y, r, n)); }
SDU GG *ann(double x, double y, double ro, double ri, int n) { return E_ ? NULL : gk(sd_ann(x, y, ro, ri, n)); }
SDU GG *capl(P2 p, P2 q, double w) { return E_ ? NULL : gk(sd_capl(&p.x, &q.x, w)); }
SDU GG *barl(P2 p, P2 q, double w) { return E_ ? NULL : gk(sd_barl(&p.x, &q.x, w)); }
SDU GG *line(const PT *l, double w) { return E_ ? NULL : gk(sd_line(l->xy, l->n, w)); }
SDU GG *ell(double x, double y, double rx, double ry, double a, int n) { return E_ ? NULL : gk(sd_ell(x, y, rx, ry, a, n)); }
SDU GG *ngon(double x, double y, double r, int n, double a) { return E_ ? NULL : gk(sd_ngon(x, y, r, n, a)); }
SDU GG *wavy(P2 p, P2 q, double amp, int n, double w) { return E_ ? NULL : gk(sd_wavy(&p.x, &q.x, BA_->rng, amp, n, w)); }
SDU GG *tri(P2 p, P2 q, double w) { return E_ ? NULL : gk(sd_tri(&p.x, &q.x, w)); }
SDU GG *rrect(double x, double y, double L, double W, double a, double r) { return E_ ? NULL : gk(sd_rrect(x, y, L, W, a, r)); }
SDU GG *rect(double x, double y, double L, double W, double a) { return E_ ? NULL : gk(shp_rect(x, y, L, W, a)); }   /* gg3.rect */
SDU GG *star(double x, double y, double ro, double ri, int n, double rot) { return E_ ? NULL : gk(shp_star(x, y, ro, ri, n, rot)); }   /* gg.star */
SDU GG *inset(const GG *g, double d) { return E_ || !g ? (fail(), NULL) : gk(shp_inset(g, d)); }   /* gg.inset */
SDU GG *geo_take(Geo *h) { GG *g; if (!h) { fail(); return NULL; } g = shp_clone(h->g); geo_unref(h); return gk(g); }
SDU GG *blob(double x, double y, double r, int k) { return E_ ? NULL : geo_take(lf_blob(x, y, r, BA_->rng, k)); }   /* gg3.blob(x, y, r, rng, k) */
SDU GG *stone(double x, double y, double r, double aspect, int nv, int round_) { return E_ ? NULL : geo_take(lf_stone(x, y, r, BA_->rng, aspect, nv, round_)); }   /* gg.stone */
SDU GG *Pt(double x, double y) { return E_ ? NULL : gk(shp_point(x, y)); }
SDU GG *Poly(const PT *l) { return E_ ? NULL : gk(shp_polygon(l->xy, l->n)); }               /* Polygon(pts) */
SDU GG *Line(const PT *l) { return E_ ? NULL : gk(shp_linestring(l->xy, l->n)); }            /* LineString(pts) */
SDU GG *Pempty(void) { return E_ ? NULL : gk(shp_polygon(NULL, 0)); }                       /* Polygon() */
/* methods */
SDU GG *buf(const GG *g, double d, int q) { return E_ || !g ? (fail(), NULL) : gk(shp_buffer_q(g, d, q)); }   /* g.buffer(d, q) (q 16 default) */
SDU GG *bufx(const GG *g, double d, int q, int cap, int join, double ml) { return E_ || !g ? (fail(), NULL) : gk(shp_buffer(g, d, q, cap, join, ml, 0)); }   /* g.buffer(d, q, cap_style=, join_style=, mitre_limit=) */
SDU GG *dif(const GG *a, const GG *b) { return E_ || !a || !b ? (fail(), NULL) : gk(shp_difference(a, b)); }
SDU GG *isect(const GG *a, const GG *b) { return E_ || !a || !b ? (fail(), NULL) : gk(shp_intersection(a, b)); }
SDU GG *uni(const GG *a, const GG *b) { return E_ || !a || !b ? (fail(), NULL) : gk(shp_union(a, b)); }
SDU GG *chull(const GG *g) { return E_ || !g ? (fail(), NULL) : gk(shp_convex_hull(g)); }
SDU GG *exter(const GG *g) {                                 /* g.exterior (a Polygon; AttributeError otherwise) */
    if (E_ || !g) { fail(); return NULL; }
    if (shp_type(g) != GEOS_POLYGON) { fail(); return NULL; }
    return gk(shp_clone(shp_exterior(g)));
}
SDU GG *rot(const GG *g, double ang, int origin, double ox, double oy, int rad) { return E_ || !g ? (fail(), NULL) : gk(shp_rotate(g, ang, origin, ox, oy, rad)); }
SDU GG *scl(const GG *g, double fx, double fy, int origin, double ox, double oy) { return E_ || !g ? (fail(), NULL) : gk(shp_scale(g, fx, fy, origin, ox, oy)); }
SDU GG *trn(const GG *g, double dx, double dy) { return E_ || !g ? (fail(), NULL) : gk(shp_translate(g, dx, dy)); }
SDU GG *cent(const GG *g) { return E_ || !g ? (fail(), NULL) : gk(shp_centroid(g)); }
SDU GG *interp(const GG *ln, double d, int norm) { return E_ || !ln ? (fail(), NULL) : gk(shp_interpolate(ln, d, norm)); }
SDU int empty(const GG *g) { int e; if (E_ || !g) return 1; e = shp_is_empty(g); if (e < 0) { fail(); return 1; } return e; }   /* g.is_empty */
SDU double area(const GG *g) { return E_ || !g ? 0.0 : shp_area(g); }
SDU double gx(const GG *pt) { if (E_ || !pt || empty(pt)) { fail(); return 0.0; } return shp_x(pt); }   /* pt.x (empty: raises) */
SDU double gy(const GG *pt) { if (E_ || !pt || empty(pt)) { fail(); return 0.0; } return shp_y(pt); }
SDU int intersects(const GG *a, const GG *b) { int r; if (E_ || !a || !b) return 0; r = shp_intersects(a, b); if (r < 0) { fail(); return 0; } return r; }
SDU int contains(const GG *a, const GG *b) { int r; if (E_ || !a || !b) return 0; r = shp_contains(a, b); if (r < 0) { fail(); return 0; } return r; }
SDU double gdist(const GG *a, const GG *b) { return E_ || !a || !b ? 0.0 : shp_distance(a, b); }

SDU GL *polys(const GG *g) { GL *o = gl(); ShpList l = { 0 }; if (E_ || !g) return o; shp_polys_of(g, &l); for (int i = 0; i < l.n; i++) gl_add(o, gk(l.g[i])); free(l.g); return o; }   /* gg.polys_of(g) */
SDU GG *scl_at(const GG *g, double fx, double fy, double ox, double oy) { return scl(g, fx, fy, SHP_ORIGIN_POINT, ox, oy); }          /* affinity.scale(g, fx, fy, origin=(ox, oy)) */
SDU GG *rot_at(const GG *g, double ang, double ox, double oy) { return rot(g, ang, SHP_ORIGIN_POINT, ox, oy, 1); }          /* affinity.rotate(g, ang, origin=(ox, oy), use_radians=True) */
SDU int pmod(int a, int b) { int r = a % b; return r < 0 ? r + b : r; }          /* a % b for ints, Python's sign */

/* ------------------------------------------------------------------ solids */
SDU SoSolids *SL(void) { SoSolids *s = (SoSolids *)calloc(1, sizeof(SoSolids)); GROW(BA_->s, BA_->ns, BA_->caps); BA_->s[BA_->ns++] = s; return s; }
SDU void cat(SoSolids *d, const SoSolids *s) { if (!E_) sol_extend(d, s); }                   /* d += s */
SDU SoSolids *SLc(const SoSolids *a, const SoSolids *b) { SoSolids *o = SL(); cat(o, a); cat(o, b); return o; }   /* a + b */
SDU void cyl(SoSolids *o, double x, double y, double r, double h, double lift, int n) { if (!E_) ck(sd_cyl(o, x, y, r, h, lift, n)); }
SDU void blk(SoSolids *o, double x, double y, double L, double W, double a, double h, double lift) { if (!E_) ck(sd_blk(o, x, y, L, W, a, h, lift)); }
SDU void ext(SoSolids *o, const GG *g, double h, double lift) { if (E_) return; if (!g) { fail(); return; } ck(sd_ext(o, g, h, lift)); }
SDU void dome(SoSolids *o, double x, double y, double r, double h, double lift, double sq, double a, int n) { if (!E_) ck(sd_dome(o, x, y, r, h, lift, sq, a, n)); }
SDU void cone(SoSolids *o, double x, double y, double r, double h, double tip, double lift, int n, double sq, double a) { if (!E_) ck(sd_cone(o, x, y, r, h, tip, lift, n, sq, a)); }
SDU void turn(SoSolids *o, double x, double y, const PT *prof, int n, double sq, double a, double lift) { if (!E_) ck(sd_turn(o, x, y, prof->xy, prof->n, n, sq, a, lift)); }
SDU void lie(SoSolids *o, double x, double y, double a, double L, double r, double lift) { if (!E_) ck(sd_lie(o, x, y, a, L, r, lift)); }
SDU void ridge(SoSolids *o, double x, double y, double a, double L, double W, double h, double lift) { if (!E_) ck(sd_ridge(o, x, y, a, L, W, h, lift)); }
SDU void loaf(SoSolids *o, double x, double y, double a, double L, double W, double h, double lift) { if (!E_) ck(sd_loaf(o, x, y, a, L, W, h, lift)); }
SDU void rod(SoSolids *o, const PT *pts, double r, double hk, double lift, double arch) { if (!E_) ck(sd_rod(o, pts->xy, pts->n, r, NULL, NULL, hk, lift, arch)); }
SDU void rodf(SoSolids *o, const PT *pts, SoNumFn rf, void *ctx, double hk, double lift, double arch) { if (!E_) ck(sd_rod(o, pts->xy, pts->n, 0.0, rf, ctx, hk, lift, arch)); }   /* rod with r a function */
SDU void spin(SoSolids *o, double x, double y, double a, double L, SoNumFn rf, void *ctx, double hk, double lift, const char *prof) { if (!E_) ck(sd_spin(o, x, y, a, L, rf, ctx, hk, lift, prof)); }
SDU void pot(SoSolids *o, double x, double y, double r, double h, double neck, double lip, double lift, int n) { if (!E_) ck(sd_pot(o, x, y, r, h, neck, lip, lift, n)); }
SDU void rock(SoSolids *o, double x, double y, double r, double h, double sq, double lift, int n) { if (!E_) ck(sd_rock(o, x, y, r, h, BA_->rng, sq, lift, n)); }   /* sq 0 = None */
SDU void fluted(SoSolids *o, double x, double y, double r, double h, int n, double a, double deep, int m, double twist, double lift) { if (!E_) ck(sd_fluted(o, x, y, r, h, n, a, deep, m, twist, lift)); }
SDU void hang(SoSolids *o, const GG *g, double z0, double z1) { if (E_) return; if (!g) { fail(); return; } ck(sd_hang(o, g, z0, z1)); }
SDU void slab(SoSolids *o, const GG *g, double z0, double z1) { if (E_) return; if (!g) { fail(); return; } ck(so_slab(o, g, z0, z1)); }   /* scenes.slab */
SDU void vault(SoSolids *o, double x, double y, double a, double L, double W, double h, const char *prof, double lift, int under) { if (!E_) ck(so_vault(o, x, y, a, L, W, h, prof, lift, under, 0, 0.0)); }   /* scenes.vault */
SDU PT *crv(double r0, double r1, double z0, double z1, int out, int n) { PT *l = pl(); if (E_) return l; l->xy = malloc(sizeof(double) * 2 * (size_t)(n + 1)); l->cap = n + 1; if (ck(sd_crv(r0, r1, z0, z1, out, n, l->xy)) == 0) l->n = n + 1; return l; }   /* crv(...) as points (r, z) */

/* ------------------------------------------------------------------ cuts and layers */
SDU CL *cl(void) { return (CL *)mk(sizeof(CL)); }
SDU void cl_add(CL *l, const GG *g, double fl) { if (l->n == l->cap) { l->cap = l->cap ? 2 * l->cap : 8; l->v = realloc(l->v, sizeof(SoCut) * (size_t)l->cap); } l->v[l->n].q = g; l->v[l->n].fl = fl; l->n++; if (!g) fail(); }
SDU SoLayer *yk(SoLayer *y) { GROW(BA_->y, BA_->ny, BA_->capy); BA_->y[BA_->ny++] = y; return y; }   /* the arena holds one reference */
SDU SoLayer *ST(const SoSolids *ps, const char *kind, int64_t par, const CL *cuts) {          /* ST(solids, kind, parent, cuts) */
    SoLayer *y = NULL; if (E_) return NULL;
    if (ck(sd_ST(ps, kind, par >= 0, par, cuts ? cuts->v : NULL, cuts ? cuts->n : 0, &y)) < 0) return NULL;
    return yk(y);
}
SDU void push(SoLayers *lay, SoLayer *y) { if (!E_) lyl_push(lay, y); }               /* lay.append(L) */
SDU GG *cl_(const GG *g) { return g ? shp_clone(g) : NULL; }
SDU SoLayer *L_up(const GG *g, double h, const char *kind) { return E_ ? NULL : yk(ly_up(cl_(g), h, kind)); }
SDU SoLayer *L_on(const GG *g, int64_t par, double h, const char *kind) { return E_ ? NULL : yk(ly_on(cl_(g), par, h, kind)); }
SDU SoLayer *L_tcut(const GG *g, int64_t par, double d) { return E_ ? NULL : yk(ly_tcut(cl_(g), par, d)); }
SDU SoLayer *L_gcut(const GG *g, double d, const char *kind) { return E_ ? NULL : yk(ly_gcut(cl_(g), d, kind)); }
SDU SoLayer *L_gcut_i(const GG *g, int64_t d, const char *kind) { SoLayer *y; if (E_) return NULL; y = ly_new(4); y->v[0] = lv_str("gcut"); y->v[1] = lv_geom(cl_(g)); y->v[2] = lv_int(d); y->v[3] = lv_str(kind); return yk(y); }   /* depth an int */
SDU SoLayer *L_mound(const GG *g, double h, const char *kind, const FtTiers *t) { SoLayer *y; if (E_) return NULL; y = ly_new(5); y->v[0] = lv_str("mound"); y->v[1] = lv_geom(cl_(g)); y->v[2] = lv_float(h); y->v[3] = lv_str(kind); y->v[4].t = LV_PTR; y->v[4].p = (void *)t; return yk(y); }
SDU SoLayer *L_pad(const GG *g) { SoLayer *y; if (E_) return NULL; y = ly_new(2); y->v[0] = lv_str("pad"); y->v[1] = lv_geom(cl_(g)); return yk(y); }
SDU const GG *ly1(const SoLayer *y) { return y && y->n > 1 && y->v[1].t == LV_GEOM ? y->v[1].g : NULL; }          /* L[1] of a layer */
SDU SoLayer *L_pad3(const GG *g, const char *kind) { SoLayer *y; if (E_) return NULL; y = ly_new(3); y->v[0] = lv_str("pad"); y->v[1] = lv_geom(cl_(g)); y->v[2] = lv_str(kind); return yk(y); }   /* ("pad", g, kind) */
/* style_details.ROCK_T ... (the tiers a "mound" layer carries) */
extern const FtTiers BSDDE_ROCK_T, BSDDE_SOFT_T, BSDDE_PEB_T, BSDDE_SPIKE_T, BSDDE_ROUND_T;

/* ------------------------------------------------------------------ Obj */
SDU SoObj *OBJ(const char *kind) { SoObj *o = (SoObj *)calloc(1, sizeof(SoObj)); so_obj_init(o, kind); GROW(BA_->o, BA_->no, BA_->capo); BA_->o[BA_->no++] = o; return o; }
#define HP(par) ((par) >= 0), (par)
SDU int64_t o_up(SoObj *o, const GG *g, double h, const char *kind) { if (E_) return 0; if (!g) { fail(); return 0; } return ck(so_obj_up(o, g, h, kind)); }
SDU int64_t o_on(SoObj *o, const GG *g, int64_t par, double h, const char *kind) { if (E_) return 0; if (!g) { fail(); return 0; } return ck(so_obj_on(o, g, HP(par), h, kind)); }
SDU void o_cut(SoObj *o, const GG *g, int64_t par, double d) { if (!E_) ck(so_obj_cut(o, g, HP(par), d)); }
SDU int64_t o_st(SoObj *o, const SoSolids *ps, const char *kind, int64_t par, const CL *cuts, const GG *foot) { if (E_) return 0; return ck(so_obj_st(o, ps, kind, HP(par), cuts ? cuts->v : NULL, cuts ? cuts->n : 0, foot)); }
SDU void o_gcut(SoObj *o, const GG *g, double d, const char *kind) { if (E_) return; if (!g) { fail(); return; } ck(so_obj_gcut(o, g, d, kind)); }
SDU int64_t o_lid(SoObj *o, const GG *g, int64_t par, double h, double edge, const char *kind) { if (E_) return 0; if (!g) { fail(); return 0; } return ck(so_obj_lid(o, g, HP(par), h, edge, kind)); }
SDU void o_groove(SoObj *o, const GG *g, int64_t par, double w, double d) { if (E_) return; if (!g) { fail(); return; } ck(so_obj_groove(o, g, HP(par), w, d)); }
SDU void o_seams(SoObj *o, const SoFrame *ff, double L, double W, int64_t par, double step, int across, double w, double d, const GG *skip) { if (!E_) ck(so_obj_seams(o, ff, L, W, HP(par), step, across, w, d, skip)); }
SDU int64_t o_studs(SoObj *o, const PT *pts, int64_t par, double r, double h, const char *kind, int n) { if (E_) return 0; return ck(so_obj_studs(o, pts->xy, pts->n, HP(par), r, h, kind, n)); }
SDU int64_t o_handle(SoObj *o, P2 p, P2 q, int64_t par, double w, double h, const char *kind) { if (E_) return 0; return ck(so_obj_handle(o, &p.x, &q.x, HP(par), w, h, kind)); }
SDU int64_t o_tabs(SoObj *o, const SoFrame *ff, int nx, const double *xs, double v, int64_t par, double a, double l, double w, double h, const char *kind) { if (E_) return 0; return ck(so_obj_tabs(o, ff, xs, nx, v, HP(par), a, l, w, h, kind)); }
SDU int64_t o_ribs(SoObj *o, const SoFrame *ff, double L, double W, int64_t par, int n, double a, double w, double h, int across, const char *kind) { if (E_) return 0; return ck(so_obj_ribs(o, ff, L, W, HP(par), n, a, w, h, across, kind)); }
SDU void o_vents(SoObj *o, const SoFrame *ff, double x, double v, int64_t par, int n, double l, double a, double step, double w, double d) { if (!E_) ck(so_obj_vents(o, ff, x, v, HP(par), n, l, a, step, w, d)); }
SDU int64_t o_corners(SoObj *o, const SoFrame *ff, double L, double W, int64_t par, double a, double size, double h, const char *kind) { if (E_) return 0; return ck(so_obj_corners(o, ff, L, W, HP(par), a, size, h, kind)); }
SDU int64_t o_straps(SoObj *o, const SoFrame *ff, int nx, const double *xs, double W, int64_t par, double a, double w, double h, const char *kind) { if (E_) return 0; return ck(so_obj_straps(o, ff, xs, nx, W, HP(par), a, w, h, kind)); }
/* return o.out(extra): the hull, and the object's layers appended to lay */
SDU GG *o_out(SoObj *o, SoLayers *lay, const GL *extra) {
    GG *h = NULL; if (E_) return NULL;
    if (ck(so_obj_out(o, extra ? extra->v : NULL, extra ? extra->n : 0, &h)) < 0) return NULL;
    gk(h); for (int i = 0; i < o->L.n; i++) lyl_push(lay, o->L.v[i]);
    return h;
}
SDU void o_F(SoObj *o, const GG *g) { if (E_) return; if (!g) { fail(); return; } GROW(o->F.v, o->F.n, o->F.cap); o->F.v[o->F.n++] = shp_clone(g); }   /* o.F.append(g) */
SDU void o_layers(SoObj *o, SoLayers *lay) { if (!E_) for (int i = 0; i < o->L.n; i++) lyl_push(lay, o->L.v[i]); }   /* ..., o.L */

/* ------------------------------------------------------------------ numpy */
SDU int arange(double a, double b, double st, double *out, int cap) { int64_t n; if (E_) return 0; n = np_arange(a, b, st, out, cap); if (n < 0 || n > cap) { fail(); return 0; } return (int)n; }   /* np.arange (float) */
SDU int linspace(double a, double b, int num, double *out) { if (E_) return 0; if (np_linspace(a, b, num, 1, out) < 0) { fail(); return 0; } return num; }   /* np.linspace */

/* ------------------------------------------------------------------ registration */
void bsdde_reg(const char *style, const char *cls, const char *kind, const char *name, double r, int ground, FtBuildFn f0, void *ctx);
int bsdde_replaced(void *ctx, double cx, double cy, double a, PyRandom *rng, int hs, double sc, Geo **foot, SoLayers *lay);   /* a stub: see bld_sd_models.c */
/* the settings the model details read (bld_sd_models.c) */
void bsdde_config(const char *models_root, double max_height);
void bsdde_models_register(void);                       /* sd_r_models.py + sd_r_models_a..d.py, in their order */
#endif
