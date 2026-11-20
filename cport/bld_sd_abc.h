/* bld_sd_abc.h: the shared helpers of bld_sd_a.c, bld_sd_b.c and bld_sd_c.c (agent "bld-sdabc", group G10): the
   style-only details of sd_a.py, sd_b.py and sd_c.py, in C, registered with sdr_register in Python's order.

   How a builder is written (each copies one `@d(...) def _(cx, cy, a, rng)` of the Python, statement by statement):
   - SD_BEGIN starts it: every geometry, solid list, point array and layer made on the way is kept in one arena (SD)
     and freed at the end, whatever happens.
   - a call that fails where Python would raise (a GEOS error, a ValueError of randint ...) jumps out of the builder
     (longjmp), which then returns SC_ERR; the random stream is left exactly where Python's raise leaves it.
   - C does not fix the order in which a call's arguments are worked out; Python does (left to right). So every random
     draw is made in its own statement, in Python's order.
   - the layers go into SD.out in Python's list order; SD_RET(foot) hands foot and the layers to the caller (0), SD_NONE
     says the builder returned None (1).
   Builders are not re-entrant (one arena): nothing in them calls another builder. */
#ifndef BLD_SD_ABC_H
#define BLD_SD_ABC_H
#include <stdlib.h>
#include <string.h>
#include <setjmp.h>
#include <math.h>
#include "builders.h"
#include "features.h"
#include "solids.h"
#include "leaf.h"
#include "shp.h"
#include "pyexact.h"
#include "pyx.h"

#define SD_PI 3.141592653589793          /* math.pi */

/* the helpers keep short names in these three files, but link under sdabc_* names, so they never clash with the
   other builder files' own helpers (bld_sd_rx.h and others use some of the same short names) */
#define SD sdabc_SD
#define SD_J sdabc_SD_J
#define sd_start sdabc_start
#define sd_end sdabc_end
#define sd_fail sdabc_fail
#define sd_ret sdabc_ret
#define sd_reg sdabc_reg
#define Gk sdabc_Gk
#define NS sdabc_NS
#define Mk sdabc_Mk
#define K sdabc_K
#define PW sdabc_PW
#define PWR sdabc_PWR
#define RF_PW sdabc_RF_PW
#define FMOD sdabc_FMOD
#define IMOD sdabc_IMOD
#define DIST sdabc_DIST
#define RI sdabc_RI
#define CH sdabc_CH
#define PVn sdabc_PVn
#define pv_add sdabc_pv_add
#define pv_crv sdabc_pv_crv
#define ARC sdabc_ARC
#define SPIRAL sdabc_SPIRAL
#define WIG sdabc_WIG
#define RINGPTS sdabc_RINGPTS
#define ARANGE sdabc_ARANGE
#define LINSPACE sdabc_LINSPACE
#define GVn sdabc_GVn
#define gv_add sdabc_gv_add
#define DISC sdabc_DISC
#define ANN sdabc_ANN
#define CAPL sdabc_CAPL
#define BARL sdabc_BARL
#define LINE sdabc_LINE
#define ELL sdabc_ELL
#define NGON sdabc_NGON
#define TRI sdabc_TRI
#define RECT sdabc_RECT
#define STAR sdabc_STAR
#define STONE sdabc_STONE
#define BLOB sdabc_BLOB
#define WAVY sdabc_WAVY
#define POLY sdabc_POLY
#define LSTR sdabc_LSTR
#define LBUF sdabc_LBUF
#define BUF sdabc_BUF
#define BUFM sdabc_BUFM
#define DIFF sdabc_DIFF
#define UNI sdabc_UNI
#define INTER sdabc_INTER
#define CHULL sdabc_CHULL
#define INSET sdabc_INSET
#define UL sdabc_UL
#define HL sdabc_HL
#define EXTR sdabc_EXTR
#define EMPTY sdabc_EMPTY
#define GDIST sdabc_GDIST
#define AREA sdabc_AREA
#define CONTAINS sdabc_CONTAINS
#define INTERP sdabc_INTERP
#define CYL sdabc_CYL
#define BLK sdabc_BLK
#define EXT sdabc_EXT
#define DOME sdabc_DOME
#define CONE sdabc_CONE
#define TURN sdabc_TURN
#define LIE sdabc_LIE
#define RIDGE sdabc_RIDGE
#define LOAF sdabc_LOAF
#define ROD sdabc_ROD
#define SPIN sdabc_SPIN
#define POT sdabc_POT
#define ROCK sdabc_ROCK
#define FLUTED sdabc_FLUTED
#define HANG sdabc_HANG
#define SLAB sdabc_SLAB
#define CAT sdabc_CAT
#define CVn sdabc_CVn
#define cv_add sdabc_cv_add
#define ST sdabc_ST
#define LUP sdabc_LUP
#define LGCUT sdabc_LGCUT
#define LMOUND sdabc_LMOUND
#define LPAD sdabc_LPAD

/* the registration of each file, called by bld_register_sd_abc() (bld_sd_a.c) in sd_a, sd_b, sd_c order */
void bld_register_sd_a(void);
void bld_register_sd_b(void);
void bld_register_sd_c(void);

/* ---------------------------------------------------------------- the arena */
typedef struct {
    GEOSGeometry **g; int ng, capg;      /* geometries */
    SoSolids **s; int ns, caps;          /* solid lists */
    void **m; int nm, capm;              /* plain blocks */
    SoLayers out;                        /* the builder's layers, in order */
} SdArena;
extern SdArena SD;
extern jmp_buf SD_J;
void sd_start(void);
void sd_end(void);                       /* frees everything the arena holds (the layers too, unless handed over) */
_Noreturn void sd_fail(void);            /* Python raised here */
int sd_ret(const GEOSGeometry *foot, Geo **out, SoLayers *layers);

#define SD_BEGIN if (setjmp(SD_J)) { sd_end(); return SC_ERR; } sd_start();
#define SD_RET(fg) return sd_ret((fg), foot, layers)
#define SD_NONE do { sd_end(); return 1; } while (0)
#define SD_ARGS void *ctx, double cx, double cy, double a, PyRandom *rng, int has_scale, double scale, Geo **foot, SoLayers *layers

GEOSGeometry *Gk(GEOSGeometry *g);       /* keep g in the arena (NULL: Python raised) */
SoSolids *NS(void);                      /* a new empty solid list */
void *Mk(size_t n);                      /* a zeroed block */
void K(int r);                           /* SO_OK / PX_OK or fail */

/* ---------------------------------------------------------------- numbers */
typedef struct { double x, y; } P2;
static inline P2 p2(double x, double y) { P2 p = { x, y }; return p; }
static inline double pmax(double a, double b) { return b > a ? b : a; }           /* max(a, b): the first of equals */
static inline double pmin(double a, double b) { return b < a ? b : a; }           /* min(a, b) */
double PW(double x, double y);           /* x ** y for floats (float_pow); a complex result fails (TypeError later) */
double PWR(double x, double y);          /* scenes._real-wise: x ** y, or the real part of a complex x ** y */
int RF_PW(double x, double y, double *v);          /* in a callback: x ** y, SO_ERR on a complex (or error) */
double FMOD(double x, double y);         /* x % y for floats */
int64_t IMOD(int64_t x, int64_t y);      /* x % y for ints */
double DIST(P2 p, P2 q);                 /* math.dist */
#define COS py_cos                       /* math.cos / math.sin: the UCRT's own */
#define SIN py_sin

/* ---------------------------------------------------------------- random (rng is the builder's) */
#define u(lo, hi) py_uniform(rng, (lo), (hi))
#define RND() py_random(rng)
int64_t RI(PyRandom *r, int64_t lo, int64_t hi);          /* rng.randint (ValueError: fail) */
int64_t CH(PyRandom *r, int64_t n);                        /* rng.choice: the index */
static inline double CHPM(PyRandom *r) { return CH(r, 2) ? 1.0 : -1.0; }           /* rng.choice((-1, 1)) */

/* ---------------------------------------------------------------- points */
static inline P2 FF(const SoFrame *f, double u_, double v) { P2 p; so_ff(f, u_, v, &p.x, &p.y); return p; }
static inline P2 POL(double x, double y, double r, double t) { P2 p; sd_pol(x, y, r, t, &p.x); return p; }
typedef struct { double *xy; int n, cap; } PV;           /* a list of points (or of (r, z) pairs) */
PV *PVn(int cap);
void pv_add(PV *v, double x, double y);
static inline void pv_p(PV *v, P2 p) { pv_add(v, p.x, p.y); }
void pv_crv(PV *v, double r0, double r1, double z0, double z1);          /* += crv(r0, r1, z0, z1) (out True, n 6) */
PV *ARC(double x, double y, double r, double a0, double a1, int n);
PV *SPIRAL(double x, double y, double r0, double r1, double turns, double a, int n);
PV *WIG(P2 p, P2 q, PyRandom *r, double amp, int n);    /* gg.wiggle */
PV *RINGPTS(double x, double y, double r, int n, double a, double jit, PyRandom *rg);
typedef struct { double *v; int n; } DV;
DV ARANGE(double start, double stop, double step);      /* list(np.arange(...)) */
DV LINSPACE(double start, double stop, int num);        /* np.linspace(start, stop, num) */

/* ---------------------------------------------------------------- outlines (all kept in the arena) */
typedef struct { const GEOSGeometry **v; int n, cap; } GV;          /* a Python list of geometries (borrowed) */
GV *GVn(void);
void gv_add(GV *l, const GEOSGeometry *g);
GEOSGeometry *DISC(double x, double y, double r, int n);
GEOSGeometry *ANN(double x, double y, double ro, double ri, int n);
GEOSGeometry *CAPL(P2 p, P2 q, double w);
GEOSGeometry *BARL(P2 p, P2 q, double w);
GEOSGeometry *LINE(const PV *pts, double w);
GEOSGeometry *ELL(double x, double y, double rx, double ry, double a_, int n);
GEOSGeometry *NGON(double x, double y, double r, int n, double a_);
GEOSGeometry *TRI(P2 p, P2 q, double w);
GEOSGeometry *RECT(double x, double y, double l, double w, double a_);           /* gg3.rect */
GEOSGeometry *STAR(double x, double y, double ro, double ri, int n, double rot);  /* gg.star */
GEOSGeometry *STONE(double x, double y, double r, PyRandom *rg, double aspect, int nv);   /* gg.stone */
GEOSGeometry *BLOB(double x, double y, double r, PyRandom *rg, int k);            /* gg3.blob */
GEOSGeometry *WAVY(P2 p, P2 q, PyRandom *rg, double amp, int n, double w);
GEOSGeometry *POLY(const PV *pts);                       /* Polygon(pts) */
GEOSGeometry *LSTR(const PV *pts);                       /* LineString(pts) */
GEOSGeometry *LBUF(const PV *pts, double d, int qs);     /* LineString(pts).buffer(d, qs) */
GEOSGeometry *BUF(const GEOSGeometry *g, double d, int qs);          /* g.buffer(d, qs) */
GEOSGeometry *BUFM(const GEOSGeometry *g, double d);     /* g.buffer(d, join_style=2) */
GEOSGeometry *DIFF(const GEOSGeometry *a, const GEOSGeometry *b);
GEOSGeometry *UNI(const GEOSGeometry *a, const GEOSGeometry *b);
GEOSGeometry *INTER(const GEOSGeometry *a, const GEOSGeometry *b);
GEOSGeometry *CHULL(const GEOSGeometry *g);              /* g.convex_hull */
GEOSGeometry *INSET(const GEOSGeometry *g, double d);    /* gg.inset */
GEOSGeometry *UL(const GV *l);                           /* U(list) */
GEOSGeometry *HL(const GV *l);                           /* hull(list) */
GEOSGeometry *EXTR(const GEOSGeometry *p);               /* p.exterior (a copy) */
int EMPTY(const GEOSGeometry *g);
double GDIST(const GEOSGeometry *a, const GEOSGeometry *b);
double AREA(const GEOSGeometry *g);
int CONTAINS(const GEOSGeometry *a, const GEOSGeometry *b);
GEOSGeometry *INTERP(const GEOSGeometry *line, double d, int normalized);

/* ---------------------------------------------------------------- solids (appended to a list, fail on error) */
void CYL(SoSolids *o, double x, double y, double r, double h, double lift, int n);
void BLK(SoSolids *o, double x, double y, double L, double W, double a_, double h, double lift);
void EXT(SoSolids *o, const GEOSGeometry *poly, double h, double lift);
void DOME(SoSolids *o, double x, double y, double r, double h, double lift, double sq, double a_, int n);
void CONE(SoSolids *o, double x, double y, double r, double h, double tip, double lift, int n, double sq, double a_);
void TURN(SoSolids *o, double x, double y, const PV *prof, int n, double sq, double a_, double lift);
void LIE(SoSolids *o, double x, double y, double a_, double L, double r, double lift);
void RIDGE(SoSolids *o, double x, double y, double a_, double L, double W, double h, double lift);
void LOAF(SoSolids *o, double x, double y, double a_, double L, double W, double h, double lift);
void ROD(SoSolids *o, const PV *pts, double r, SoNumFn rf, void *c, double hk, double lift, double arch);
void SPIN(SoSolids *o, double x, double y, double a_, double L, SoNumFn rf, void *c, double hk, double lift, const char *prof);
void POT(SoSolids *o, double x, double y, double r, double h, double neck, double lip, double lift, int n);
void ROCK(SoSolids *o, double x, double y, double r, double h, PyRandom *rg, double sq, double lift, int n);
void FLUTED(SoSolids *o, double x, double y, double r, double h, int n, double a_, double deep, int m, double twist, double lift);
void HANG(SoSolids *o, const GEOSGeometry *poly, double z0, double z1);
void SLAB(SoSolids *o, const GEOSGeometry *poly, double z0, double z1);
void CAT(SoSolids *o, const SoSolids *x);                /* o += x */

/* ---------------------------------------------------------------- layers (appended to SD.out in order) */
typedef struct { SoCut *v; int n, cap; } CV;             /* cuts: (polygon, floor) */
CV *CVn(void);
void cv_add(CV *c, const GEOSGeometry *q, double fl);
void ST(const SoSolids *ps, const char *kind, const CV *cuts);          /* ST(ps, kind, cuts=cuts) (cuts NULL: ()) */
void LUP(const GEOSGeometry *g, double h, const char *kind);            /* ("up", g, h, kind) */
void LGCUT(const GEOSGeometry *g, double d, const char *kind);          /* ("gcut", g, d, kind) */
void LMOUND(const GEOSGeometry *g, double h, const char *kind, const FtTiers *t);   /* ("mound", g, h, kind, T) */
void LPAD(const GEOSGeometry *g, const char *kind);                     /* ("pad", g, kind) */

/* style_details.ROCK_T ... (the same numbers as features.py's) */
#define SD_PEB_T (&FT_PEB_T)
#define SD_SOFT_T (&FT_SOFT_T)

/* a registration line: d(style, cls, kind, name, r=10.5, ground=False) */
void sd_reg(FtBuildFn f, const char *style, const char *cls, const char *kind, const char *name, int ground);
#endif
