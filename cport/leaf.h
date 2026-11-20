/* leaf.h: the leaf geometry of the scene code (survey group G2): routines that call only shapely, math and random.
   gg.py: polys_of, inset, wide_enough, frame, edges_survive, fit_for_taper, stone (+ _hull), star, wiggle, area_count.
   gg3.py: rect, blob, ring, hexa, meander, spiral, radial, rp, aim.   gg2.py: line_cut.
   details.py: the sh_* shapes (the lambdas they return) and the pat_* top patterns.
   Each C function names the Python routine it copies; results are NEW handles (Geo *) owned by the caller, NULL =
   Python would raise (or, where said, None). The random numbers come from the caller's PyRandom, drawn in exactly
   Python's order. Proof: work\cscene\scene\prove_leaf.py. */
#ifndef LEAF_H
#define LEAF_H
#include "scene.h"

/* gg.polys_of(g): every polygon in g, depth first. A Polygon is handed back AS ITSELF (the same handle, as Python
   returns the same object); parts of a collection are new handles (shapely makes new objects for g.geoms). */
void lf_polys_of(Geo *g, GeoList *out);
void lf_polys_of_new(GEOSGeometry *g, GeoList *out);      /* the same for a fresh geometry (taken over) */
Geo *lf_inset(const Geo *p, double d);                     /* gg.inset: p.buffer(-d, join_style="mitre", mitre_limit=10.0) */
int lf_wide_enough(const Geo *p, double w);                /* gg.wide_enough(p, w) with SC.ts as gg._TS; SC_ERR on error */
typedef struct { double cx, cy, c, s; } Frame;
Frame lf_frame(double cx, double cy, double a);            /* gg.frame(cx, cy, a) */
static inline void lf_at(const Frame *f, double u, double v, double *x, double *y) { *x = f->cx + u * f->c - v * f->s; *y = f->cy + u * f->s + v * f->c; }
int lf_edges_survive(const GEOSGeometry *p, double d);     /* gg.edges_survive (p a Polygon) */
/* gg.fit_for_taper(p, d): a new polygon, or NULL with *err = 0 for None (*err = 1: Python would raise) */
Geo *lf_fit_for_taper(const Geo *p, double d, int *err);
/* gg.stone(cx, cy, r, rng, aspect, nv, round_): aspect 0 and nv 0 mean None (Python's `aspect or ...` treats 0 the
   same). round_ needs gg._CSX / _CSY (numpy's own cos / sin): lf_set_csx() once, handed over from Python. */
void lf_set_csx(const double *csx, const double *csy);
Geo *lf_stone(double cx, double cy, double r, PyRandom *rng, double aspect, int nv, int round_);
int lf_hull(const double *xy, int n, double *out);         /* gg._hull: count of points written to out (2 * n doubles room) */
Geo *lf_star(double cx, double cy, double ro, double ri, int n, double rot);        /* gg.star */
/* gg.wiggle(a, b, rng, amp, n): n + 1 points into out (2 * (n + 1) doubles), returns the count */
int lf_wiggle(double ax, double ay, double bx, double by, PyRandom *rng, double amp, int n, double *out);
int lf_area_count(int n, const Geo *region);               /* gg.area_count(n, region) (reads SC.layout, SC.K, SC.R0) */
Geo *lf_rect(double cx, double cy, double l, double w, double a);                   /* gg3.rect */
Geo *lf_blob(double cx, double cy, double r, PyRandom *rng, int k);                 /* gg3.blob */
Geo *lf_ring(double cx, double cy, double ro, double ri, int res);                  /* gg3.ring */
Geo *lf_hexa(double cx, double cy, double r, double rot);                           /* gg3.hexa */
Geo *lf_meander(PyRandom *rng, double phi, double off, double amp, double w, int n);    /* gg3.meander: a LineString */
Geo *lf_spiral(double cx, double cy, double r, double turns, double w);             /* gg3.spiral */
Geo *lf_radial(double cx, double cy, double r0, double r1, int n, PyRandom *rng, double w);   /* gg3.radial */
Geo *lf_line_cut(double ax, double ay, double bx, double by, double w);             /* gg2.line_cut */

/* gg3.FOCUS[0] and gg3.AIM[0]: set by scenes.focus(); NULL = None */
typedef void (*LfFocusFn)(void *ctx, PyRandom *rng, double *x, double *y);
typedef double (*LfAimFn)(void *ctx, PyRandom *rng, double x, double y, double a);
extern LfFocusFn lf_focus; extern void *lf_focus_ctx;
extern LfAimFn lf_aimfn; extern void *lf_aim_ctx;
void lf_rp(PyRandom *rng, double dmin, double dmax, double *x, double *y);          /* gg3.rp */
double lf_aim(PyRandom *rng, double x, double y, double span);                      /* gg3.aim */

/* details.py shapes: sh_rock(r0, r1, nv) etc. return lambdas (x, y, rng) -> geometry; here a LfShape carries the
   factory's arguments and lf_shape() is the lambda. The tag (fn.shape) is lf_shape_tag(). */
enum { SH_ROCK, SH_ROUND, SH_DISC, SH_ELL, SH_RECT, SH_CAPS, SH_BONE, SH_STAR, SH_CRESCENT, SH_HEX, SH_TRI, SH_CROSS,
       SH_RING, SH_BLOB, SH_WIG, SH_COUNT };
typedef struct { int kind; double a[5]; } LfShape;
/* the factories, with their Python defaults written out:
   sh_rock(r0, r1, nv0=5, nv1=8)  sh_round(r0, r1)  sh_disc(r0, r1, res=20)  sh_ell(a0, a1, b0, b1)  sh_rect(l0, l1, w0, w1)
   sh_caps(l0, l1, w)  sh_bone(l0, l1)  sh_star(r0, r1, n0, n1)  sh_crescent(r0, r1)  sh_hex(r0, r1)  sh_tri(r0, r1)
   sh_cross(l0, l1, w)  sh_ring(ro0, ro1, t)  sh_blob(r0, r1)  sh_wig(l0, l1, w, amp=0.5) */
LfShape lf_sh(int kind, double a0, double a1, double a2, double a3, double a4);
Geo *lf_shape(const LfShape *sh, double x, double y, PyRandom *rng);
const char *lf_shape_tag(int kind);                        /* "rock", "round", ... */
/* details.py top patterns: pat_slots(n=4, w=0.35), pat_seams(step=0.9), pat_ring(t=0.3), pat_cross(), pat_dots(r=0.25),
   pat_hole(r0=0.5, r1=0.8), pat_rune(); lf_pat() is the returned f(cx, cy, g, rng) */
enum { PAT_SLOTS, PAT_SEAMS, PAT_RING, PAT_CROSS, PAT_DOTS, PAT_HOLE, PAT_RUNE, PAT_COUNT };
typedef struct { int kind; double a[2]; } LfPat;
Geo *lf_pat(const LfPat *pt, double cx, double cy, const Geo *g, PyRandom *rng);

/* thin wrappers over cport\pyexact.h (np_linspace, np_arange, px_pow, px_sum) */
double lf_fpow(double x, double y, int *err);              /* float ** float (float_pow special cases, then UCRT pow) */
int lf_linspace(double start, double stop, int num, double *out);                   /* numpy.linspace (float64) */
int lf_arange(double start, double stop, double step, double *out, int cap);        /* numpy.arange (float64): count */
double lf_fsum(const double *x, int n);                    /* builtin sum() of floats (int 0 start, Neumaier) */
#define LF_RADIANS(x) ((x) * (3.141592653589793 / 180.0))   /* math.radians */
#define LF_DEGREES(x) ((x) * (180.0 / 3.141592653589793))   /* math.degrees */
#endif
