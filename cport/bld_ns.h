/* bld_ns.h: the shared helpers of the newer styles' builders in C (group G10, agent "bld-newstyles"):
   bld_newstyles.c (the helpers, the registration, the style tables) and bld_ns_<style>.c (each style's own builders
   and the sd details of its sd_r_x_<style>.py). Private to these files.

   How a builder is written here:
   - every geometry made while a builder runs is a temporary owned by the ARENA (T(g) registers it; NULL from a
     shapely call means Python raised there: T() then jumps straight out of the builder, which returns SC_ERR at
     exactly the point Python would). Layers get their own clones (ns_up, ns_gcut, ...), the foot is cloned into a
     Geo at the end (ns_done).
   - Python evaluates a call's arguments left to right; C does not. Every random draw is therefore made in its own
     statement, in Python's order (never two draws, or a draw and a function that draws, in one C expression).
   - a builder entry (NS_BEGIN ... NS_END) is one arena frame; frames nest (a callback from place.c opens its own). */
#ifndef BLD_NS_H
#define BLD_NS_H
#include <setjmp.h>
#include <stdio.h>
#include <math.h>
#include <string.h>
#include <stdlib.h>
#include "builders.h"
#include "pyexact.h"

typedef GEOSGeometry NsGeom;
#define G NsGeom          /* (geos_dyn.h names its function table G; these files never use it) */
/* every helper below links as ns_<name> (function-like macros: a struct member of the same name is left alone), so these
   files share no global name with the other builder files */
#define T(...) ns_T(__VA_ARGS__)
#define TN(...) ns_TN(__VA_ARGS__)
#define gl_add(...) ns_gl_add(__VA_ARGS__)
#define gl_new(...) ns_gl_new(__VA_ARGS__)
#define pl_add(...) ns_pl_add(__VA_ARGS__)
#define pl_new(...) ns_pl_new(__VA_ARGS__)
#define P(...) ns_P(__VA_ARGS__)
#define PB(...) ns_PB(__VA_ARGS__)
#define buf(...) ns_buf(__VA_ARGS__)
#define bufx(...) ns_bufx(__VA_ARGS__)
#define dif(...) ns_dif(__VA_ARGS__)
#define inter(...) ns_inter(__VA_ARGS__)
#define uni(...) ns_uni(__VA_ARGS__)
#define uu(...) ns_uu(__VA_ARGS__)
#define U(...) ns_U(__VA_ARGS__)
#define u_(...) ns_u_(__VA_ARGS__)
#define hull(...) ns_hull(__VA_ARGS__)
#define chull(...) ns_chull(__VA_ARGS__)
#define poly(...) ns_poly(__VA_ARGS__)
#define polyv(...) ns_polyv(__VA_ARGS__)
#define line(...) ns_line(__VA_ARGS__)
#define simp(...) ns_simp(__VA_ARGS__)
#define rect(...) ns_rect(__VA_ARGS__)
#define star(...) ns_star(__VA_ARGS__)
#define stone(...) ns_stone(__VA_ARGS__)
#define blob(...) ns_blob(__VA_ARGS__)
#define inset(...) ns_inset(__VA_ARGS__)
#define ring(...) ns_ring(__VA_ARGS__)
#define disc(...) ns_disc(__VA_ARGS__)
#define ann(...) ns_ann(__VA_ARGS__)
#define capl(...) ns_capl(__VA_ARGS__)
#define barl(...) ns_barl(__VA_ARGS__)
#define lineb(...) ns_lineb(__VA_ARGS__)
#define ell(...) ns_ell(__VA_ARGS__)
#define ngon(...) ns_ngon(__VA_ARGS__)
#define tri(...) ns_tri(__VA_ARGS__)
#define rrect(...) ns_rrect(__VA_ARGS__)
#define wavy(...) ns_wavy(__VA_ARGS__)
#define rot(...) ns_rot(__VA_ARGS__)
#define scl(...) ns_scl(__VA_ARGS__)
#define mrr(...) ns_mrr(__VA_ARGS__)
#define cen(...) ns_cen(__VA_ARGS__)
#define rpt(...) ns_rpt(__VA_ARGS__)
#define ext_ring(...) ns_ext_ring(__VA_ARGS__)
#define interp(...) ns_interp(__VA_ARGS__)
#define polys(...) ns_polys(__VA_ARGS__)
#define clone(...) ns_clone(__VA_ARGS__)
#define area(...) ns_area(__VA_ARGS__)
#define len_(...) ns_len_(__VA_ARGS__)
#define dist(...) ns_dist(__VA_ARGS__)
#define contains(...) ns_contains(__VA_ARGS__)
#define intersects(...) ns_intersects(__VA_ARGS__)
#define bounds(...) ns_bounds(__VA_ARGS__)
#define gx(...) ns_gx(__VA_ARGS__)
#define gy(...) ns_gy(__VA_ARGS__)
#define wide(...) ns_wide(__VA_ARGS__)
#define wiggle(...) ns_wiggle(__VA_ARGS__)
#define sl_new(...) ns_sl_new(__VA_ARGS__)
#define sl_add(...) ns_sl_add(__VA_ARGS__)
#define sl_custom(...) ns_sl_custom(__VA_ARGS__)
#define sl_custom_f(...) ns_sl_custom_f(__VA_ARGS__)
#define cut_add(...) ns_cut_add(__VA_ARGS__)
#define S_ext(...) ns_S_ext(__VA_ARGS__)
#define S_box(...) ns_S_box(__VA_ARGS__)
#define S_lie(...) ns_S_lie(__VA_ARGS__)
#define S_cyl(...) ns_S_cyl(__VA_ARGS__)
#define S_dome(...) ns_S_dome(__VA_ARGS__)
#define S_cone(...) ns_S_cone(__VA_ARGS__)
#define S_turn(...) ns_S_turn(__VA_ARGS__)
#define S_rod(...) ns_S_rod(__VA_ARGS__)
#define S_rodf(...) ns_S_rodf(__VA_ARGS__)
#define S_spin(...) ns_S_spin(__VA_ARGS__)
#define S_ridge(...) ns_S_ridge(__VA_ARGS__)
#define S_loaf(...) ns_S_loaf(__VA_ARGS__)
#define S_rock(...) ns_S_rock(__VA_ARGS__)
#define S_fluted(...) ns_S_fluted(__VA_ARGS__)
#define S_pot(...) ns_S_pot(__VA_ARGS__)
#define S_hang(...) ns_S_hang(__VA_ARGS__)
#define S_vault(...) ns_S_vault(__VA_ARGS__)
#define S_lathe(...) ns_S_lathe(__VA_ARGS__)
#define crv(...) ns_crv(__VA_ARGS__)
#define ext1(...) ns_ext1(__VA_ARGS__)
#define box_(...) ns_box_(__VA_ARGS__)
#define obj_new(...) ns_obj_new(__VA_ARGS__)
#define ob_up(...) ns_ob_up(__VA_ARGS__)
#define ob_on(...) ns_ob_on(__VA_ARGS__)
#define ob_cut(...) ns_ob_cut(__VA_ARGS__)
#define ob_st(...) ns_ob_st(__VA_ARGS__)
#define ob_gcut(...) ns_ob_gcut(__VA_ARGS__)
#define ob_lid(...) ns_ob_lid(__VA_ARGS__)
#define ob_groove(...) ns_ob_groove(__VA_ARGS__)
#define ob_seams(...) ns_ob_seams(__VA_ARGS__)
#define ob_studs(...) ns_ob_studs(__VA_ARGS__)
#define ob_handle(...) ns_ob_handle(__VA_ARGS__)
#define ob_tabs(...) ns_ob_tabs(__VA_ARGS__)
#define ob_ribs(...) ns_ob_ribs(__VA_ARGS__)
#define ob_vents(...) ns_ob_vents(__VA_ARGS__)
#define ob_corners(...) ns_ob_corners(__VA_ARGS__)
#define ob_straps(...) ns_ob_straps(__VA_ARGS__)
#define ob_raw_mound(...) ns_ob_raw_mound(__VA_ARGS__)
#define ob_raw(...) ns_ob_raw(__VA_ARGS__)
#define ob_stroke(...) ns_ob_stroke(__VA_ARGS__)
#define ob_chamfer(...) ns_ob_chamfer(__VA_ARGS__)
#define ob_oct(...) ns_ob_oct(__VA_ARGS__)
#define ob_glyph(...) ns_ob_glyph(__VA_ARGS__)
#define ob_band(...) ns_ob_band(__VA_ARGS__)

/* ---------------- the arena and the raise frames ---------------- */
typedef struct NsFrame { jmp_buf jb; int mark; struct NsFrame *up; } NsFrame;
extern NsFrame *ns_top;
void ns_enter(NsFrame *f);
void ns_leave(NsFrame *f);                       /* frees everything the arena got since ns_enter */
_Noreturn void ns_raise(void);                   /* Python raised: back to the innermost frame */
void ns_keep(void *p, void (*fr)(void *));       /* anything else to free when the frame ends */
G *T(G *g);                                      /* a temporary geometry; NULL raises */
G *TN(G *g);                                     /* the same, NULL kept (None) */
#define NS_BEGIN NsFrame fr_; ns_enter(&fr_); if (setjmp(fr_.jb)) { ns_leave(&fr_); return SC_ERR; }
#define NS_END(r) do { int r_ = (r); ns_leave(&fr_); return r_; } while (0)
#define NS_CHECK(c) do { if (!(c)) ns_raise(); } while (0)

/* ---------------- lists of temporaries (borrowed geometries) ---------------- */
typedef struct { G **v; int n, cap; } GL;
void gl_add(GL *l, G *g);                        /* l.append(g) (NULL = None allowed) */
GL *gl_new(void);                                /* a list freed with the frame */
/* points */
typedef struct { double *v; int n, cap; } PTS;    /* n points, v[2 i], v[2 i + 1] */
void pl_add(PTS *l, double x, double y);
PTS *pl_new(void);

/* ---------------- maths as Python does it ---------------- */
#define PI 3.141592653589793
static inline double ns_cos(double x) { return py_cos(x); }
static inline double ns_sin(double x) { return py_sin(x); }
static inline double ns_atan2(double y, double x) { return py_atan2(y, x); }
double ns_pow(double x, double y);               /* float ** float; a complex result or an error raises */
double ns_mdist(double x0, double y0, double x1, double y1);          /* math.dist */
double ns_hypot(double x, double y);

/* ---------------- random draws (Python's random.Random) ---------------- */
static inline double U_(PyRandom *r, double a, double b) { return py_uniform(r, a, b); }
static inline double RR(PyRandom *r) { return py_random(r); }
static inline int64_t RI(PyRandom *r, int64_t a, int64_t b) { return py_randint(r, a, b); }
static inline int64_t RB(PyRandom *r, int64_t n) { return py_randbelow(r, n); }          /* randrange(n), n > 0 */
#define CH(r, ...) ns_choice_d((r), (const double[]){ __VA_ARGS__ }, sizeof((const double[]){ __VA_ARGS__ }) / sizeof(double))
double ns_choice_d(PyRandom *r, const double *v, int n);             /* rng.choice((a, b, ...)) of numbers */

/* ---------------- geometry (each returns an arena temporary) ---------------- */
G *P(double x, double y);                        /* Point(x, y) */
G *PB(double x, double y, double r, int q);      /* Point(x, y).buffer(r, q) (q 16 = the default) */
G *buf(const G *g, double d, int q);             /* g.buffer(d, q) */
G *bufx(const G *g, double d, int q, int cap, int join, double mitre);   /* g.buffer(d, quad_segs=q, cap_style, join_style, mitre_limit) */
G *dif(const G *a, const G *b);
G *inter(const G *a, const G *b);
G *uni(const G *a, const G *b);
G *uu(GL *l);                                    /* shapely unary_union(list) */
G *U(GL *l);                                     /* style_details.U: None / empty left out, nothing -> GEOMETRYCOLLECTION EMPTY */
G *u_(GL *l);                                    /* scenes._u: nothing -> Polygon() */
G *hull(GL *l);                                  /* style_details.hull: U(gs).convex_hull */
G *chull(const G *g);                            /* g.convex_hull */
G *poly(PTS *p);                                  /* Polygon(pts) */
G *polyv(const double *xy, int n);
G *line(PTS *p);                                  /* LineString(pts) */
G *simp(const G *g, double tol);                 /* g.simplify(tol) */
G *rect(double cx, double cy, double l, double w, double a);         /* gg3.rect */
G *star(double cx, double cy, double ro, double ri, int n, double rot);   /* gg.star */
G *stone(double cx, double cy, double r, PyRandom *rng, double aspect, int nv);   /* gg.stone (aspect / nv 0 = None) */
G *blob(double cx, double cy, double r, PyRandom *rng, int k);      /* gg3.blob */
G *inset(const G *g, double d);                  /* gg.inset */
G *ring(double cx, double cy, double ro, double ri, int res);       /* gg3.ring */
G *disc(double x, double y, double r, int n);    /* style_details.disc (n 20) */
G *ann(double x, double y, double ro, double ri, int n);            /* style_details.ann (n 24) */
G *capl(double px, double py, double qx, double qy, double w);
G *barl(double px, double py, double qx, double qy, double w);
G *lineb(PTS *p, double w);                       /* style_details.line(pts, w) */
G *ell(double x, double y, double rx, double ry, double a, int n);   /* style_details.ell (a 0, n 20) */
G *ngon(double x, double y, double r, int n, double a);
G *tri(double px, double py, double qx, double qy, double w);
G *rrect(double x, double y, double L, double W, double a, double r);
G *wavy(double px, double py, double qx, double qy, PyRandom *rng, double amp, int n, double w);   /* style_details.wavy */
G *rot(const G *g, double ang, int origin, double ox, double oy, int rad);
G *scl(const G *g, double fx, double fy, int origin, double ox, double oy);
G *mrr(const G *g);                              /* minimum_rotated_rectangle */
G *cen(const G *g);                              /* centroid */
G *rpt(const G *g);                              /* representative_point */
G *ext_ring(const G *g);                         /* g.exterior (a clone) */
G *interp(const G *ln, double d);                /* line.interpolate(d) */
GL *polys(const G *g);                           /* gg.polys_of(g): a new list of temporaries */
G *clone(const G *g);
G *box_(double x0, double y0, double x1, double y1);          /* shapely.geometry.box (ccw) */
static inline double pmin(double a, double b) { return b < a ? b : a; }    /* min(a, b): the first of equal values */
static inline double pmax(double a, double b) { return b > a ? b : a; }    /* max(a, b) */
double ns_round(double x);                       /* round(x) -> int (ties to even) */
int64_t ns_int(double x);                        /* int(x) (truncation; raises on nan / inf) */
static inline int empty(const G *g) { int r = shp_is_empty(g); if (r < 0) ns_raise(); return r; }
double area(const G *g);
double len_(const G *g);
double dist(const G *a, const G *b);             /* a.distance(b) */
int contains(const G *a, const G *b);
int intersects(const G *a, const G *b);
void bounds(const G *g, double b[4]);
double gx(const G *pt);
double gy(const G *pt);
int wide(const G *p, double w);                  /* gg.wide_enough(p, w) */
/* the frame f(u, v) */
typedef SoFrame FF;
static inline FF ff_(double cx, double cy, double a) { return so_frame(cx, cy, a); }
static inline double fx(const FF *f, double u, double v) { return f->cx + u * f->c - v * f->s; }
static inline double fy(const FF *f, double u, double v) { return f->cy + u * f->s + v * f->c; }
#define FX(f, u, v) fx(&(f), (u), (v)), fy(&(f), (u), (v))
/* pol(x, y, r, t) as a pair */
static inline double polx(double x, double r, double t) { return x + r * ns_cos(t); }
static inline double poly_(double y, double r, double t) { return y + r * ns_sin(t); }
#define POL(x, y, r, t) polx((x), (r), (t)), poly_((y), (r), (t))
/* wiggle(a, b, rng, amp, n): n + 1 points */
PTS *wiggle(double ax, double ay, double bx, double by, PyRandom *rng, double amp, int n);
/* numpy */
int ns_linspace(double a, double b, int n, double *out);            /* np.linspace(a, b, n) */
int ns_arange(double a, double b, double st, double *out, int cap);  /* np.arange(a, b, st): count */

/* ---------------- solids ---------------- */
SoSolids *sl_new(void);                          /* a list of solids freed with the frame */
void sl_add(SoSolids *dst, const SoSolids *src); /* dst += src */
/* a custom solid (sect, top): fn(ctx, z, &out) gives a NEW geometry or NULL (None); ctx is copied (size bytes) */
typedef int (*NsSectFn)(void *ctx, double z, G **out);
void sl_custom(SoSolids *dst, NsSectFn fn, const void *ctx, size_t size, double top);
/* a list of (poly, floor height) cuts */
typedef struct { SoCut v[64]; int n; } NsCuts;
void cut_add(NsCuts *c, const G *q, double fl);
/* the same as sl_custom, the ctx freed by fr (it may own geometries) */
void sl_custom_f(SoSolids *dst, NsSectFn fn, const void *ctx, size_t size, double top, void (*fr)(void *));
/* style_details solids, appended to sol (each raises where the Python would) */
void S_ext(SoSolids *sol, const G *g, double h, double lift);
void S_lie(SoSolids *sol, double x, double y, double a, double L, double r, double lift);
void S_cyl(SoSolids *sol, double x, double y, double r, double h, double lift, int n);
void S_dome(SoSolids *sol, double x, double y, double r, double h, double lift, double sq, double a, int n);
void S_cone(SoSolids *sol, double x, double y, double r, double h, double tip, double lift, int n, double sq, double a);
void S_turn(SoSolids *sol, double x, double y, const double *prof, int np, int n, double sq, double a, double lift);
void S_rod(SoSolids *sol, PTS *p, double r, double hk, double lift, double arch);
void S_rodf(SoSolids *sol, PTS *p, SoNumFn f, void *ctx, double hk, double lift, double arch);
void S_spin(SoSolids *sol, double x, double y, double a, double L, SoNumFn f, void *ctx, double hk, double lift, const char *prof);
void S_ridge(SoSolids *sol, double x, double y, double a, double L, double W, double h, double lift);
void S_loaf(SoSolids *sol, double x, double y, double a, double L, double W, double h, double lift);
void S_rock(SoSolids *sol, double x, double y, double r, double h, PyRandom *rng, double sq, double lift, int n);
void S_fluted(SoSolids *sol, double x, double y, double r, double h, int n, double a, double deep, int m, double twist, double lift);
void S_pot(SoSolids *sol, double x, double y, double r, double h, double neck, double lip, double lift, int n);
void S_hang(SoSolids *sol, const G *poly, double z0, double z1);
void S_box(SoSolids *sol, const G *poly, double h, double lift);          /* scenes.box */
void S_vault(SoSolids *sol, double cx, double cy, double a, double L, double W, double h, const char *prof, double lift, int under);
void S_lathe(SoSolids *sol, double cx, double cy, const double *prof, int np, int seg, double sq, double a, double lift);
int crv(double r0, double r1, double z0, double z1, int out_, int n, double *prof);   /* n + 1 pairs */
SoSolids *ext1(const G *g, double h);            /* ext(g, h) as a new list */

/* ---------------- the result: footprint and layers ---------------- */
typedef struct { SoLayers *lay; } NsOut;
int ns_count(NsOut *o);                          /* len(layers) so far */
void ns_up(NsOut *o, const G *g, double h, const char *kind);
void ns_on(NsOut *o, const G *g, int64_t par, double h, const char *kind);
void ns_tcut(NsOut *o, const G *g, int64_t par, double d);
void ns_gcut(NsOut *o, const G *g, double d, const char *kind);
void ns_pad(NsOut *o, const G *g, const char *kind);                     /* ("pad", g, kind) */
void ns_mound(NsOut *o, const G *g, double h, const char *kind, const FtTiers *t);   /* ("mound", g, h, kind, tiers) */
void ns_st(NsOut *o, const SoSolids *s, const char *kind, int has_par, int64_t par, const NsCuts *c);   /* ST(...) / scenes._steps */
void ns_obj_out(NsOut *o, SoObj *ob, GL *extra, G **foot);              /* Obj.out(extra): the layers moved over */
SoObj *obj_new(const char *kind);                /* an Obj freed with the frame */
/* Obj methods (raise where the Python would); kind NULL = the Obj's own; par < 0 = None */
int ob_up(SoObj *o, const G *g, double h, const char *kind);
int ob_on(SoObj *o, const G *g, int64_t par, double h, const char *kind);
void ob_cut(SoObj *o, const G *g, int64_t par, double d);
int ob_st(SoObj *o, const SoSolids *s, const char *kind, int64_t par, const NsCuts *c, const G *foot);
void ob_gcut(SoObj *o, const G *g, double d, const char *kind);
int ob_lid(SoObj *o, const G *g, int64_t par, double h, double edge, const char *kind);
void ob_groove(SoObj *o, const G *g, int64_t par, double w, double d);
void ob_seams(SoObj *o, const FF *f, double L, double W, int64_t par, double step, int across, double w, double d, const G *skip);
int ob_studs(SoObj *o, PTS *p, int64_t par, double r, double h, const char *kind, int n);
int ob_handle(SoObj *o, double px, double py, double qx, double qy, int64_t par, double w, double h, const char *kind);
int ob_tabs(SoObj *o, const FF *f, const double *xs, int nx, double v, int64_t par, double a, double l, double w, double h, const char *kind);
int ob_ribs(SoObj *o, const FF *f, double L, double W, int64_t par, int n, double a, double w, double h, int across, const char *kind);
void ob_vents(SoObj *o, const FF *f, double x, double v, int64_t par, int n, double l, double a, double step, double w, double d);
int ob_corners(SoObj *o, const FF *f, double L, double W, int64_t par, double a, double size, double h, const char *kind);
int ob_straps(SoObj *o, const FF *f, const double *xs, int nx, double W, int64_t par, double a, double w, double h, const char *kind);
/* o.L.append(("mound", g, h, kind, tiers)); o.F.append(g) */
void ob_raw_mound(SoObj *o, const G *g, double h, const char *kind, const FtTiers *t);
/* o.L.append(layer) [; o.F.append(g)] for any other layer made with ns_* into a scratch NsOut */
void ob_raw(SoObj *o, SoLayer *l, const G *foot);
/* the end of a builder: (foot, layers); foot NULL = None */
int ns_done(Geo **foot_out, const G *foot);

/* ---------------- sd_reg and the style-only details ---------------- */
/* f0(cx, cy, a, rng, o, &foot): the body of a `_` builder. Returns 0 (foot set, NULL = None) or 1 (it returned None) */
typedef int (*NsBody)(double cx, double cy, double a, PyRandom *rng, NsOut *o, G **foot);
typedef struct { NsBody body; const char *style, *cls, *kind, *name; int cutfix; } NsSd;
/* d(style, cls, kind, name, r, ground) / x_d(...): registered into sd_reg's REG (sdr_register) with f0 = the body
   (through sd_r_x_frontier.x_cutfix when cutfix) */
void ns_sd_reg(const NsSd *d, double r, int ground);
int ns_sd_call(void *ctx, double cx, double cy, double a, PyRandom *rng, int has_scale, double scale, Geo **foot, SoLayers *layers);
const NsSd *ns_sd_find(const char *style, const char *name);     /* the one registered last (the proof's lookup) */

/* x_cutfix(out, style, name, cls): widen every tcut too thin to stay */
int ns_cutfix(Geo *foot, SoLayers *lay, const char *style, const char *name, const char *cls);

/* the tier tables of features.py / style_details.py (borrowed, static) */
extern const FtTiers NS_SOFT_T, NS_ROCK_T, NS_PEB_T, NS_SPIKE_T, NS_ROUND_T;

/* style_obelis.py's glyph helpers (bld_ns_obelis.c) */
G *ob_stroke(PTS *p, double w);
G *ob_chamfer(const G *g, double c);
G *ob_oct(double x, double y, double r, double a);
G *ob_glyph(double cx, double cy, double s, double a, PyRandom *rng, double w);
G *ob_band(double cx, double cy, double a, double L, double cell, PyRandom *rng, double w, int border);

/* ---------------- the data tables (bld_ns_tables.c, written by work\csceneld_newstyles\gen_tables.py) ----------------
   One value tree NS_V: a node is t (N None, A absent attribute, b bool, i int, f float, s str, t tuple, l list,
   d dict (its kids are k nodes: s = key, one kid = value), S set (sorted by repr), F a function, E a reference to
   NS_ENTRY[v], X another object (s = its type name)); its kids are NS_KID[a .. a + n). An F node is a builder call as
   the kit made it: s = "module:code qualname" (details:scatter.<locals>.fn, details:sh_tri.<locals>.<lambda>,
   style_obelis:f_monolith, features:comp.<locals>.fn ...), kid 0 = its attributes (tuple of k nodes tmpl kind shape cls
   sd tag, A when it has none), kid 1 = its closure (tuple of k nodes: the factory's arguments by name, nested calls as F).
   NS_ENTRY: every tier / pool function of the six styles, one entry per Python object (the same object twice is one
   entry: identity kept). NS_STYLE[i].v: a dict {NAME TITLE SAYS GROUND PAL PATH_OK EXTRA_PROPS BUILT ENCLOSED CLIFFS
   COLOURS THEMES [FLOORS LOOSE CUTS SPIKE] tiers details}; tiers / details hold E references. NS_KIT: a dict of what
   these styles put into the kit's shared tables (gg.COL, gg.CUTC, gg3.BRIDGE, scenes.DEBRIS / GROWTH / WETK / CUTK /
   TALL / SUBJECTS / SUBJECT_SAYS / PAL / PATH_OK / STYLE_SAYS / _EXTRA / BUILT / ENCLOSED / CLIFFS, styles2.TITLES). */
typedef struct { char t; unsigned char isint; double v; const char *s; int a, n; } NsV;
typedef struct { const char *style; int v; } NsEntry;
typedef struct { const char *style; int v; } NsStyleT;
extern const NsV NS_V[]; extern const int NS_NV; extern const int NS_KID[]; extern const int NS_NKID;
extern const NsEntry NS_ENTRY[]; extern const int NS_NENTRY; extern const NsStyleT NS_STYLE[]; extern const int NS_NSTYLE; extern const int NS_KIT;
const NsV *ns_kid(const NsV *v, int j);                       /* the j-th kid */
const NsV *ns_get(const NsV *d, const char *key);             /* a dict's (or a k-node tuple's) value by key, NULL = missing */
int ns_tables_text(Buf *out);                                 /* the canonical text gen_tables.py compares */

/* style_blight.rune(f2, k, s, w) (bld_ns_blight.c) */
G *ns_bl_rune(const FF *f2, int64_t k, double s, double w);

/* ---------------- the styles' own tier features (bld_ns_tiers.c) ----------------
   Each is a tier function fn(s, rng, lvl) as bld_gg3.h's BsTierFn, registered as bld_add_any("style_<name>", "<python
   name>", "tier", fn): style_sundered sd_blocks sd_flags sd_wallstub; style_ashveil _plate _stilts _drums; style_obelis
   f_monolith f_pylon f_casket f_panels f_glyphslab f_ring f_drift; style_blight f_thorns f_boil f_runes f_bones. They
   return 0 (Python's None / False), 1 (True) or SC_ERR (raised). The parametrised ones:
     bld_any("style_ashveil", "_sd", "ns_sd_tier"):   int fn(const char *name, int n0, int n1, Scene *s, PyRandom *rng, int lvl)
     bld_any("style_thornwild", "_sd", "ns_sd_tier1"): int fn(const char *name, Scene *s, PyRandom *rng, int lvl)
     bld_any("style_blight", "pat_brune", "ns_pat"):  Geo *fn(double cx, double cy, const Geo *g, PyRandom *rng, int *err)
   (style_ashveil._kind(fn, k, shape) only names fn's kind: the entry in NS_ENTRY says which function it wraps.)
   ns_s3d: s.solids3d for the comps the _sd features run (NULL unless --supports; the compose sets it). */
extern FtS3dList *ns_s3d;
void ns_bld_tiers(void);

/* each style file's registration (called by bld_register_newstyles, in sorted(glob("sd_r_*.py")) order) */
void ns_reg_ashveil(void); void ns_reg_blight(void); void ns_reg_ferrum(void);
void ns_reg_obelis(void); void ns_reg_sundered(void); void ns_reg_thornwild(void);
/* the style files' own builders (style_<name>.py b_* / L_*) into builders.c's table */
void ns_bld_blight(void); void ns_bld_ferrum(void); void ns_bld_obelis(void);
/* FtCall argument helpers for the factories: the j-th positional or keyword kw, Python's default when left out */
const FtArg *ns_arg(const FtCall *c, int j, const char *kw);
const char *ns_arg_s(const FtCall *c, int j, const char *kw, const char *dflt);
int ns_arg_pair(const FtCall *c, int j, const char *kw, double dflt0, double dflt1, double *a, double *b);
#endif
