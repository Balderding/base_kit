/* bld_sd_rx.h: the small runtime the sd_r_x_<style>.py ports share (bld_sd_rx.c, bld_sd_rx_<style>.c; G10, agent
   "bld-sdrx", 2026-11-19). The 18 original styles' third drawings (sd_r_x_badlands .. sd_r_x_urban) in C.

   How a builder is written (one C function per Python `_`, named after the detail):
   - every geometry made inside a builder is kept in one arena (xr_keep) and freed when the builder returns, so the C
     reads like the Python: no frees. A NULL from GEOS or an SO_ERR from the solids is where Python raises: the
     builder stops right there (longjmp to xr_run), the rng left exactly as Python leaves it.
   - Python evaluates arguments left to right; C does not. EVERY random draw is its own statement, in Python's
     order (u(), rnd(), RI(), CH()), never two draws inside one C expression or call.
   - the Obj of style_details is solids.h SoObj; parents are layer indices (-1 = None).
   Callbacks that run later (a section cut when a steps layer is first looked at) never longjmp: they return SO_ERR. */
#ifndef BLD_SD_RX_H
#define BLD_SD_RX_H
#include <setjmp.h>
#include <math.h>
#include <string.h>
#include <stdlib.h>
#include "builders.h"
#include "features.h"
#include "solids.h"
#include "shp.h"
#include "leaf.h"
#include "pyexact.h"
#include "pyx.h"

/* every name below is kept private to these files (other builder files use the same short names) */
#define RI xr_RI
#define CHI xr_CHI
#define CH xr_CH
#define PW xr_PW
#define PWs xr_PWs
#define AR xr_AR
#define LS xr_LS
#define PINT xr_PINT
#define DISC xr_DISC
#define ANN xr_ANN
#define CAPL xr_CAPL
#define BARL xr_BARL
#define LINE xr_LINE
#define ELL xr_ELL
#define NGON xr_NGON
#define RECT xr_RECT
#define RRECT xr_RRECT
#define TRI xr_TRI
#define POLY xr_POLY
#define LSTR xr_LSTR
#define PTG xr_PTG
#define BUF xr_BUF
#define BUFX xr_BUFX
#define INS xr_INS
#define DIF xr_DIF
#define INT xr_INT
#define UNI xr_UNI
#define EXTR xr_EXTR
#define CHULL xr_CHULL
#define SIMP xr_SIMP
#define TRANS xr_TRANS
#define ROT xr_ROT
#define SCL xr_SCL
#define BLOB xr_BLOB
#define STAR xr_STAR
#define WAVY xr_WAVY
#define GEMPTY xr_GEMPTY
#define EMPTY xr_EMPTY
#define AREA xr_AREA
#define LEN xr_LEN
#define CENT xr_CENT
#define BOUNDS xr_BOUNDS
#define VALID xr_VALID
#define CONTAINS xr_CONTAINS
#define INTERSECTS xr_INTERSECTS
#define DISTG xr_DISTG
#define GA xr_GA
#define UL xr_UL
#define U1 xr_U1
#define U2 xr_U2
#define U3 xr_U3
#define HULL xr_HULL
#define POLYS xr_POLYS
#define SN xr_SN
#define EXT xr_EXT
#define CYL xr_CYL
#define BLK xr_BLK
#define DOME xr_DOME
#define CONE xr_CONE
#define TURN xr_TURN
#define LIE xr_LIE
#define RIDGE xr_RIDGE
#define LOAF xr_LOAF
#define ROD xr_ROD
#define SPIN xr_SPIN
#define CRV xr_CRV
#define POT xr_POT
#define ROCK xr_ROCK
#define FLUTED xr_FLUTED
#define HANG xr_HANG
#define SCAT xr_SCAT
#define TBAR xr_TBAR
#define XWALL xr_XWALL
#define LEAN xr_LEAN
#define XPL xr_XPL
#define CURL xr_CURL
#define UP xr_UP
#define UPi xr_UPi
#define ON xr_ON
#define CUT xr_CUT
#define ST xr_ST
#define GCUT xr_GCUT
#define LID xr_LID
#define GROOVE xr_GROOVE
#define SEAMS xr_SEAMS
#define STUDS xr_STUDS
#define HANDLE xr_HANDLE
#define TABS xr_TABS
#define RIBS xr_RIBS
#define VENTS xr_VENTS
#define CORNERS xr_CORNERS
#define STRAPS xr_STRAPS
#define XWORD xr_XWORD
#define XCHAIN xr_XCHAIN
#define OUT xr_OUT
#define FPUSH xr_FPUSH
#define XON xr_XON
#define XSACK xr_XSACK
#define XBAR xr_XBAR
#define XSTUDS xr_XSTUDS
#define OUTX xr_OUTX
typedef GEOSGeometry GG;
typedef struct { double x, y; } P;                 /* a point (x, y): P arrays are flat x, y doubles */
#define PXY(p) (p).x, (p).y

/* ---------------- the run: arena + the jump out where Python raises ---------------- */
_Noreturn void xr_fail(void);
GG *xr_keep(GG *g);                                   /* g into the arena; NULL -> xr_fail */
void *xr_mem(size_t n);                             /* zeroed scratch memory, freed with the arena */
void xr_ok(int r);                                  /* r < 0 (SO_ERR / PX_E*) -> xr_fail */
typedef void (*XrBody)(double cx, double cy, double a, PyRandom *rng, SoObj *o, GG **foot);
/* a builder f0: d(style, cls, kind, name, r, ground) or x_d (x = 1: x_cutfix on what it returns) */
typedef struct { const char *style, *cls, *kind, *name; double r; int ground, x; XrBody body; } XrDef;
int xr_f0(void *ctx, double cx, double cy, double a, PyRandom *rng, int has_scale, double scale, Geo **foot, SoLayers *layers);
void xr_register(const XrDef *defs, int n);         /* sdr_register each, in order */
/* a body ends with: *foot = OUT(o)  (o.out(): hull of o.F; the layers are o->L) */
GG *OUT(SoObj *o);
void FPUSH(SoObj *o, const GG *g);                  /* o.F.append(g) */
/* a body may also say "return None": */
_Noreturn void xr_none(void);

/* ---------------- random (each ONE draw: never two in one expression) ---------------- */
#define u(a_, b_) py_uniform(rng, (a_), (b_))
#define rnd() py_random(rng)
int64_t RI(PyRandom *rng, int64_t a, int64_t b);    /* rng.randint(a, b) */
int64_t CHI(PyRandom *rng, int64_t n);              /* rng.choice(seq of n): the index */
double CH(PyRandom *rng, const double *v, int n);   /* rng.choice(v) */
static inline double CH2(PyRandom *rng) { static const double v[2] = { -1, 1 }; return CH(rng, v, 2); }   /* rng.choice((-1, 1)) */

/* ---------------- numbers ---------------- */
double PW(double x, double y);                      /* x ** y (float_pow); a complex result or an error: xr_fail */
double PWs(double x, double y, int *err);           /* the same for callbacks: *err = 1 instead of failing */
static inline double MX(double a, double b) { return b > a ? b : a; }      /* max(a, b): the first of equal values */
static inline double MN(double a, double b) { return b < a ? b : a; }      /* min(a, b) */
static inline double DIST(P p, P q) { double v[2] = { p.x, p.y }, w[2] = { q.x, q.y }; return px_dist(v, w, 2); }   /* math.dist */
#define COS py_cos
#define SIN py_sin
#define ATAN2 py_atan2
#define PI 3.141592653589793
int AR(double *out, int cap, double start, double stop, double step);       /* np.arange: count (out gets them) */
void LS(double *out, double start, double stop, int num);                  /* np.linspace(start, stop, num) */
int64_t PINT(double x);                             /* int(x) */

/* ---------------- frames and points ---------------- */
typedef SoFrame F;
static inline F FR(double cx, double cy, double a) { return so_frame(cx, cy, a); }
static inline P FF(const F *f, double u_, double v_) { P p; so_ff(f, u_, v_, &p.x, &p.y); return p; }
static inline P PT(double x, double y) { P p = { x, y }; return p; }
static inline P POL(double x, double y, double r, double t) { P p; sd_pol(x, y, r, t, &p.x); return p; }
static inline P POLp(P c, double r, double t) { return POL(c.x, c.y, r, t); }

/* ---------------- outlines (all kept in the arena) ---------------- */
GG *DISC(double x, double y, double r, int n);       /* disc(x, y, r, n=20) */
static inline GG *DISCp(P c, double r, int n) { return DISC(c.x, c.y, r, n); }
GG *ANN(double x, double y, double ro, double ri, int n);
GG *CAPL(P p, P q, double w);
GG *BARL(P p, P q, double w);
GG *LINE(const P *pts, int n, double w);
GG *ELL(double x, double y, double rx, double ry, double a, int n);
GG *NGON(double x, double y, double r, int n, double a);
GG *RECT(double x, double y, double L, double W, double a);
static inline GG *RECTp(P c, double L, double W, double a) { return RECT(c.x, c.y, L, W, a); }
GG *RRECT(double x, double y, double L, double W, double a, double r);
static inline GG *RRECTp(P c, double L, double W, double a, double r) { return RRECT(c.x, c.y, L, W, a, r); }
GG *TRI(P p, P q, double w);
GG *POLY(const P *pts, int n);                       /* Polygon(pts) */
GG *LSTR(const P *pts, int n);                       /* LineString(pts) */
GG *PTG(double x, double y);                         /* Point(x, y) */
GG *BUF(const GG *g, double d, int q);                /* g.buffer(d, q) */
GG *BUFX(const GG *g, double d, int q, int cap, int join, double mitre);     /* g.buffer(d, q, cap_style, join_style, mitre_limit) */
GG *INS(const GG *g, double d);                       /* inset */
GG *DIF(const GG *a, const GG *b);
GG *INT(const GG *a, const GG *b);
GG *UNI(const GG *a, const GG *b);
GG *EXTR(const GG *p);                                /* p.exterior (a copy) */
GG *CHULL(const GG *g);                               /* g.convex_hull */
GG *SIMP(const GG *g, double tol);                    /* g.simplify(tol) (preserve_topology) */
GG *TRANS(const GG *g, double dx, double dy);         /* affinity.translate */
GG *ROT(const GG *g, double ang, int origin, double ox, double oy, int rad);  /* affinity.rotate */
GG *SCL(const GG *g, double fx, double fy, int origin, double ox, double oy); /* affinity.scale */
GG *BLOB(double x, double y, double r, PyRandom *rng, int k);              /* gg3.blob */
GG *STAR(double x, double y, double ro, double ri, int n, double rot);      /* gg.star */
GG *WAVY(P p, P q, PyRandom *rng, double amp, int n, double w);            /* wavy */
GG *GEMPTY(void);                                    /* Polygon() */
int EMPTY(const GG *g);                              /* g.is_empty */
double AREA(const GG *g);
double LEN(const GG *g);
P CENT(const GG *g);                                 /* g.centroid as (x, y) */
void BOUNDS(const GG *g, double b[4]);
int VALID(const GG *g);
int CONTAINS(const GG *a, const GG *b);
int INTERSECTS(const GG *a, const GG *b);
double DISTG(const GG *a, const GG *b);
/* a list of geometries (U([...]) and friends); the items are arena-owned, the list holds pointers */
typedef struct { GG **v; int n, cap; } GL;
void GA(GL *l, GG *g);                               /* append (None allowed: NULL) */
GG *UL(const GL *l);                                 /* U(list): unary_union of the non-empty ones */
GG *U1(GG *a);                                        /* U([a]) */
GG *U2(GG *a, GG *b);                                  /* U([a, b]) */
GG *U3(GG *a, GG *b, GG *c);
GG *HULL(const GL *l);                               /* hull(list) */
void POLYS(const GG *g, GL *out);                    /* polys_of(g) into out (arena copies) */

/* ---------------- solids (a list, Python's [] + [] is pushing into the same list) ---------------- */
typedef SoSolids S;
S *SN(void);                                        /* a new empty list (freed with the arena) */
void EXT(S *s, const GG *g, double h, double lift);  /* ext(poly, h, lift=0) */
void CYL(S *s, double x, double y, double r, double h, double lift, int n);
void BLK(S *s, double x, double y, double L, double W, double a, double h, double lift);
void DOME(S *s, double x, double y, double r, double h, double lift, double sq, double a, int n);   /* 0, 1, 0, 20 */
void CONE(S *s, double x, double y, double r, double h, double tip, double lift, int n, double sq, double a);   /* .24, 0, 16, 1, 0 */
void TURN(S *s, double x, double y, const double *prof, int np, int n, double sq, double a, double lift);   /* 24, 1, 0, 0 */
void LIE(S *s, double x, double y, double a, double L, double r, double lift);
void RIDGE(S *s, double x, double y, double a, double L, double W, double h, double lift);
void LOAF(S *s, double x, double y, double a, double L, double W, double h, double lift);
void ROD(S *s, const P *pts, int n, double r, SoNumFn rf, void *ctx, double hk, double lift, double arch);   /* hk 1.0 */
void SPIN(S *s, double x, double y, double a, double L, SoNumFn rf, void *ctx, double hk, double lift, const char *prof);   /* 1.2, 0, "round" */
int CRV(double *prof, double r0, double r1, double z0, double z1, int out_, int n);   /* appends n + 1 (r, z) pairs at prof: returns 2 * (n + 1) */
void POT(S *s, double x, double y, double r, double h, double neck, double lip, double lift, int n);   /* .5 .66 0 24 */
void ROCK(S *s, double x, double y, double r, double h, PyRandom *rng, double sq, double lift, int n);  /* sq 0 = None, 0, 9 */
void FLUTED(S *s, double x, double y, double r, double h, int n, double a, double deep, int m, double twist, double lift);   /* 8 0 .8 6 0 0 */
void HANG(S *s, const GG *g, double z0, double z1);
void SCAT(S *s, const S *o);                        /* s += o */
/* sd_r_trederra.t_bar(p, q, rf, k=0.4, z0=None, z1=None): rf a number (rfn NULL) or rfn(ctx, s) */
typedef double (*XrNum)(void *ctx, double s);
void TBAR(S *s, P p, P q, double r, XrNum rfn, void *ctx, double k, int has_z, double z0, double z1);
static inline void TBARc(S *s, P p, P q, double r, double k) { TBAR(s, p, q, r, NULL, NULL, k, 0, 0, 0); }
static inline void TBARz(S *s, P p, P q, double r, double z0, double z1) { TBAR(s, p, q, r, NULL, NULL, 0.4, 1, z0, z1); }
/* xc_bar / xu_bar (step 0.1), xm_bar / xt_bar (0.11): t_bar with C = k * r and m = max(8, int(L / step)) */
void XBAR(S *s, P p, P q, double r, XrNum rfn, void *ctx, double k, double step);
/* sd_r_x_dungeon.x_wall(pts, w, zf) */
void XWALL(S *s, const P *pts, int n, double w, XrNum zf, void *ctx);
/* sd_r_infestation._lean(x, y, r, h, dx, dy, n=10, tip=0.26, sq=1.0, a=0.0, pw=1.0) */
void LEAN(S *s, double x, double y, double r, double h, double dx, double dy, int n, double tip, double sq, double a, double pw);
/* sd_r_infestation._pl(pts, s): pts flat (s, v) pairs */
double XPL(const double *pts, int n, double s);
/* sd_r_infestation._curl(p, t, L, bend, n=10): n + 1 points into out */
int CURL(P *out, P p, double t, double L, double bend, int n);

/* scenes._sack(cx, cy, a, L, W, h, lift=0.0): two lofts */
void XSACK(S *s, double cx, double cy, double a, double L, double W, double h, double lift);

/* ---------------- the Obj ---------------- */
int UP(SoObj *o, const GG *g, double h, const char *kind);           /* o.up(g, h, kind) */
int UPi(SoObj *o, const GG *g, int64_t h, const char *kind);         /* o.up(g, <int h>) */
int ON(SoObj *o, const GG *g, int par, double h, const char *kind);  /* o.on(g, par, h, kind); par -1 = None */
void CUT(SoObj *o, const GG *g, int par, double d);                  /* o.cut(g, par, d) */
typedef SoCut C;
int ST(SoObj *o, const S *s, const char *kind, int par, const C *cuts, int nc, const GG *foot);   /* o.st(...) */
void GCUT(SoObj *o, const GG *g, double d, const char *kind);
int LID(SoObj *o, const GG *g, int par, double h, double edge, const char *kind);    /* .16 .16 */
void GROOVE(SoObj *o, const GG *g, int par, double w, double d);                    /* .2 .16 */
void SEAMS(SoObj *o, const F *ff, double L, double W, int par, double step, int across, double w, double d, const GG *skip);
int STUDS(SoObj *o, const P *pts, int n, int par, double r, double h, const char *kind, int nn);   /* .27 .15 None 8 */
int HANDLE(SoObj *o, P p, P q, int par, double w, double h, const char *kind);     /* .46 .42 */
int TABS(SoObj *o, const F *ff, const double *xs, int nx, double v, int par, double a, double l, double w, double h, const char *kind);
int RIBS(SoObj *o, const F *ff, double L, double W, int par, int n, double a, double w, double h, int across, const char *kind);
void VENTS(SoObj *o, const F *ff, double x, double v, int par, int n, double l, double a, double step, double w, double d);
int CORNERS(SoObj *o, const F *ff, double L, double W, int par, double a, double size, double h, const char *kind);
int STRAPS(SoObj *o, const F *ff, const double *xs, int nx, double W, int par, double a, double w, double h, const char *kind);

/* xon(o, g, par, h, kind=None): ("on", g, par, h, kind or o.k), -1 (None) when g is None or empty; par -1 = None */
int XON(SoObj *o, const GG *g, int par, double h, const char *kind);
/* xstuds(o, pts, par, r=0.2, h=0.1, kind=None, n=8) */
int XSTUDS(SoObj *o, const P *pts, int npts, int par, double r, double h, const char *kind, int n);
GG *OUTX(SoObj *o, GG *const *extra, int n);       /* o.out(extra) */

/* ---------------- sd_r_x_frontier's shared tools ---------------- */
GG *XWORD(const char *txt, double cx, double cy, double b, double h, double w, int has_width, double width);   /* x_word (None -> NULL) */
int XCHAIN(SoObj *o, P p, P q, const char *kind, double w, double link);  /* x_chain(o, p, q, kind="iron", w=0.2, link=0.52) */

/* every part's own table */
void bld_register_sd_rx(void);
void bld_sdrx_badlands(void); void bld_sdrx_coast(void); void bld_sdrx_corridor(void); void bld_sdrx_cynder(void);
void bld_sdrx_dungeon(void); void bld_sdrx_forest(void); void bld_sdrx_fortress(void); void bld_sdrx_frontier(void);
void bld_sdrx_graveyard(void); void bld_sdrx_highland(void); void bld_sdrx_infestation(void); void bld_sdrx_maahl(void);
void bld_sdrx_mine(void); void bld_sdrx_swamp(void); void bld_sdrx_targa(void); void bld_sdrx_temple(void);
void bld_sdrx_trederra(void); void bld_sdrx_urban(void);
#endif
