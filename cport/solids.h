/* solids.h: group G5 of the C port: the solids of scenes.py and the drawing tools of style_details.py (solids.c).
   Copied routine by routine from the Python, bit for bit; proof: work\cscene\solids\prove.py (real inputs captured
   from running scenes + random inputs, every geometry compared as WKB, every float by its bits, the rng state and
   the _CHAIN counter after each call).

   scenes.py            _u _disc _ann _cap _bar  box vault lathe loft tube slab curve  _chain  sculpt _steps
                        _weather _crate_layers
   sd_r_models.py       _Later (as SoLater) _fine
   style_details.py     U disc ann capl barl line ell hull ngon arc spiral pol ringpts wavy cyl blk ext dome cone
                        turn lie ridge loaf rod spin crv ST pot rock fluted hang tri rrect, and the class Obj

   Rules (as shp.h):
   - every GEOSGeometry * handed back is NEW and owned by the caller; inputs are never changed or taken over.
   - functions returning int give SO_OK (0) or SO_ERR (-1) = Python would raise here (GEOSException, ValueError,
     ZeroDivisionError, IndexError, KeyError, AttributeError ...); index-returning ones give the index >= 0 or SO_ERR.
   - "None" is NULL where a geometry may be None.
   Call solids_init() once (it calls shp_init()).

   What Python does lazily stays lazy: a "steps" layer holds an SoLater that runs sculpt() the first time it is
   looked at (so_later_fill / so_later_pieces), and sculpt numbers its chain from the global counter at THAT moment.
   The caller (features._build, G7) must look at the layers in the order Python does. */
#ifndef SOLIDS_H
#define SOLIDS_H
#include <stdint.h>
#include "geos_dyn.h"
#include "pyrandom.h"

#define SO_OK 0
#define SO_ERR (-1)

/* ---------------- settings the Python reads from modules / the environment ---------------- */
typedef struct {
    int supports;          /* scenes.SUPPORTS(): CFG allow_supports (KIT_SUPPORTS) */
    double true_scale;     /* features.true_scale(): float(KIT_SCALE or 1) (sculpt, _fine) */
    double ts;             /* gg._TS = max(1, KIT_SCALE) (wide_enough in _weather) */
    double minw;           /* gg.MINW (0.88): wide_enough's default, bound when gg was imported */
    double R0, H, rim;     /* gg.R0, gg.H, getattr(gg, "RIM", 12): slab's 3D build only */
    double step;           /* sculpt's step height, mm (settings "detail_step_mm"): 0 = the Python kit's (top / 0.15, 6..18 steps) */
    int smooth;            /* smooth details (settings "smooth_details", --smooth; KIT_SMOOTH): rounded solids become smooth shells */
} SolidsCfg;
extern SolidsCfg SO;
int solids_init(int supports, double true_scale, double ts, double minw, double R0, double H, double rim);   /* 1 = ready */

/* scenes._CHAIN[0] and _chain() */
extern int64_t so_chain_counter;
int64_t so_chain(void);                                  /* _CHAIN[0] += 1; return _CHAIN[0] */

/* ---------------- gg.frame ---------------- */
typedef struct { double cx, cy, c, s; } SoFrame;          /* c, s = math.cos(a), math.sin(a) */
SoFrame so_frame(double cx, double cy, double a);
void so_ff(const SoFrame *f, double u, double v, double *x, double *y);   /* f(u, v) = (cx + u * c - v * s, cy + u * s + v * c) */

/* ---------------- solids: (section, top[, 3D build]) ---------------- */
/* section(z) = the outline of the part standing higher than z (NULL = None). A custom section (sd_mesh, builder
   lambdas) is a callback: it returns SO_OK with *out set (NULL = None) or SO_ERR. */
typedef int (*SoSectFn)(void *ctx, double z, GEOSGeometry **out);
/* loft's rfun(u) and tube's r(t): the value AFTER scenes._real (use px_real for x ** y); SO_ERR if Python raises */
typedef int (*SoNumFn)(void *ctx, double x, double *out);
typedef struct Solid Solid;
typedef struct { Solid **v; int n, cap; } SoSolids;      /* a list of solids: one reference each */
void sol_push(SoSolids *l, Solid *s);                     /* append, adds a reference */
void sol_extend(SoSolids *l, const SoSolids *o);          /* l += o */
void sol_free(SoSolids *l);
Solid *sol_ref(Solid *s);
void sol_unref(Solid *s);
int sol_sect(const Solid *s, double z, GEOSGeometry **out);          /* s[0](z) */
double sol_top(const Solid *s);                                      /* s[1] */
int sol_has3d(const Solid *s);                                       /* len(s) > 2 (only with supports on) */
/* a custom solid; ctx_free (may be NULL) is called when the last reference goes */
Solid *sol_custom(SoSectFn fn, void *ctx, void (*ctx_free)(void *), double top);

/* scenes.py constructors: each appends its solid(s) to out. Python's n= arguments are ignored there, so they are
   left out here. Profiles are (r, z) pairs, flat. */
int so_box(SoSolids *out, const GEOSGeometry *poly, double h, double lift);                       /* box(poly, h, lift) */
/* vault(cx, cy, a, L, W, h, prof, lift, under, axle); prof "round" | "ridge" | "loaf" (anything else: KeyError when cut) */
int so_vault(SoSolids *out, double cx, double cy, double a, double L, double W, double h, const char *prof, double lift, int under, int has_axle, double axle);
int so_lathe(SoSolids *out, double cx, double cy, const double *prof, int np, int seg, double squash, double a, double lift, const double *real, int nreal);
int so_loft(SoSolids *out, double cx, double cy, double a, double L, SoNumFn rfun, void *ctx, double hk, const char *prof, double lift, int under);
/* tube(pts, r, hk, lift, under, arch): r constant (rfun NULL) or r(i / m) */
int so_tube(SoSolids *out, const double *pts, int npts, double r, SoNumFn rfun, void *ctx, double hk, double lift, int under, double arch);
int so_slab(SoSolids *out, const GEOSGeometry *poly, double z0, double z1);
int so_curve(double r0, double r1, double z0, double z1, int n, int out_, double *prof);         /* prof gets n + 1 (r, z) pairs */

/* ---------------- sculpt and its pieces ---------------- */
typedef struct { const GEOSGeometry *q; double fl; } SoCut;          /* (polygon, floor height), borrowed */
typedef struct { GEOSGeometry *g; double ztop; int64_t c; double z; int ghost; } SoPiece;
/* a piece: (U, ztop, c, z), or the marker of true 3D solids (foot3, top3, c, 0.0, "ghost") when ghost = 1 */
typedef struct { SoPiece *v; int n, cap; } SoPieces;
void sop_free(SoPieces *p);
int so_sculpt(const SoSolids *solids, const SoCut *cuts, int ncuts, int n, SoPieces *out);       /* n = 0: None */
/* --supports on: what sculpt registered (scenes.TRUE3D / TOP3) for chain c; NULL / 0 when none */
const SoSolids *so_true3d(int64_t c);
int so_top3(int64_t c, double *top3);
void so_true3d_clear(void);                               /* TRUE3D.clear(); TOP3.clear() (a new base) */
/* --supports on: what a solid's true 3D build (the lambda in its x[2]) reads, for solids3d.c. Pointers borrowed. */
enum { SO3_NONE = 0, SO3_SLAB, SO3_VAULT, SO3_LATHE, SO3_LOFT, SO3_TUBE };
typedef struct {
    int type;
    const GEOSGeometry *poly; double z0, z1;                  /* slab */
    double cx, cy, a, L, W, lift; int has_axle; double axle;  /* vault (cx, cy, a, lift: lathe too; lift: loft, tube) */
    const double *real; int nreal, seg; double squash;        /* lathe */
    SoFrame ff; int m; const double *us, *rs;                 /* loft (m: tube too) */
    GEOSGeometry *const *P; const double *R; double arch;     /* tube */
} So3dView;
int so_view3d(const Solid *s, So3dView *v);              /* SO_OK; SO_ERR when s has no 3D build */

/* ---------------- smooth details (2.1, not in the Python kit) ----------------
   With SO.smooth on (and supports off) sculpt takes the rounded solids of a group (lathe, vault, loft, tube and the
   custom solids that can raster their height: the model details) out of the steps: it registers them for chain c
   (so_smooth3d), leaves the ghost marker of their footprint in the layout (as supports on does) and a hidden column
   inside them (its own chain) so what stands on them still finds a top. smooth3d.c builds the shell: the envelope
   height of the group (the highest point above each spot, so nothing overhangs) with vertical sides down into the
   ground. A group with cuts (sculpt's cuts, or a layer whose top is cut: so_smooth_block) stays as steps. */
extern int so_smooth_block;                               /* 1 while features.c fills a layer whose top gets cut */
/* a custom solid's height raster: for every node (x0 + i g, y0 + j g), i < nx, j < ny, H[j * nx + i] = max(H, the
   solid's top above that spot); nodes it does not cover are left alone. SO_OK / SO_ERR */
typedef int (*SoHRasterFn)(void *ctx, double x0, double y0, double g, int nx, int ny, double *H);
void sol_custom_hraster(Solid *s, SoHRasterFn fn);       /* a custom solid that can (sd_mesh models) */
int so_rounded(const Solid *s);                           /* 1 = smooth details takes it */
int so_hraster(const Solid *s, double x0, double y0, double g, int nx, int ny, double *H);   /* as SoHRasterFn, any rounded solid */
/* the exact no-overhang shell of one rounded solid standing at zb, heights times zs, down to zbot, curves sampled about
   every res mm: each closed piece handed to emit (v: nv x 3, f: nt x 3, faces outwards). SO_OK, or SO_ERR when the
   solid has no exact shell (a custom one: then its height raster is used) */
typedef void (*SoEmitFn)(void *u, const double *v, int nv, const int *f, int nt);
int so_envelope(const Solid *s, double zb, double zs, double zbot, double res, SoEmitFn emit, void *u);
/* what sculpt registered for chain c with smooth details on: the rounded solids and their footprint (union of sect(0));
   0 when c is not a smooth chain */
int so_smooth3d(int64_t c, const SoSolids **sol, const GEOSGeometry **foot);

/* sd_r_models._Later: the pieces of a "steps" layer, cut the first time anything looks at them */
typedef struct SoLater SoLater;
SoLater *so_later_ref(SoLater *l);
void so_later_unref(SoLater *l);
int so_later_filled(const SoLater *l);                   /* 1 once sculpt has run (or it held its pieces from the start) */
int so_later_fill(SoLater *l);                           /* runs sculpt now if not yet: SO_OK / SO_ERR (sculpt raised; it then stays unfilled, as Python leaves it) */
const SoPieces *so_later_pieces(SoLater *l);            /* fills, then the pieces (NULL if sculpt raised) */

/* ---------------- layers: the tuples a builder returns ---------------- */
enum { LV_NONE = 0, LV_INT, LV_FLOAT, LV_STR, LV_GEOM, LV_LATER, LV_PTR };
typedef struct { int t; int64_t i; double f; const char *s; GEOSGeometry *g; SoLater *later; void *p; } SoVal;
/* a layer tuple: v[0] is the tag ("up", "on", "tcut", "gcut", "steps", "mound" ...). It owns its geometries and
   holds a reference to its SoLater; LV_PTR items are borrowed (any other Python object the caller keeps alive).
   Layers are never changed after they are made (Python tuples), so lists share them by reference. */
#define SO_MAXV 8
typedef struct SoLayer { int rc, n; SoVal v[SO_MAXV]; } SoLayer;
typedef struct { SoLayer **v; int n, cap; } SoLayers;
SoLayer *ly_new(int n);                                   /* n items, all None; set them with the lv_* helpers */
SoLayer *ly_ref(SoLayer *l);
void ly_unref(SoLayer *l);
void lyl_push(SoLayers *l, SoLayer *y);                   /* adds a reference */
void lyl_push_own(SoLayers *l, SoLayer *y);               /* takes the caller's reference over */
void lyl_free(SoLayers *l);
const char *so_intern(const char *s);                     /* the same pointer for the same text (never freed) */
SoVal lv_none(void);
SoVal lv_int(int64_t i);
SoVal lv_float(double f);
SoVal lv_str(const char *s);                              /* NULL -> None */
SoVal lv_geom(GEOSGeometry *g);                           /* takes g over */
SoVal lv_later(SoLater *l);                               /* takes a reference over */
/* the common shapes (each takes g over) */
SoLayer *ly_up(GEOSGeometry *g, double h, const char *kind);                        /* ("up", g, h, kind) */
SoLayer *ly_on(GEOSGeometry *g, int64_t par, double h, const char *kind);           /* ("on", g, par, h, kind) */
SoLayer *ly_tcut(GEOSGeometry *g, int64_t par, double d);                           /* ("tcut", g, par, d) */
SoLayer *ly_gcut(GEOSGeometry *g, double d, const char *kind);                      /* ("gcut", g, d, kind) */

/* scenes._steps(solids, kind, parent, cuts): ("steps", foot, _Later, kind[, parent]) or ("up", Polygon(), 0.0, kind) */
int so_steps(const SoSolids *solids, const char *kind, int has_parent, int64_t parent, const SoCut *cuts, int ncuts, SoLayer **out);
/* sd_r_models._fine(sol, top, kind): a steps layer cut a step every 0.1 mm (8..60) */
int so_fine(const SoSolids *sol, double top, const char *kind, SoLayer **out);
/* scenes._weather(layers, rng, chip): out gets the new list (shares the untouched layers) */
int so_weather(const SoLayers *layers, PyRandom *rng, double chip, SoLayers *out);
/* scenes._crate_layers(g, ff, L, W, a, h, kind, rng, parent, n0): appended to out */
int so_crate_layers(const GEOSGeometry *g, const SoFrame *ff, double L, double W, double a, double h, const char *kind, PyRandom *rng, int has_parent, int64_t parent, int64_t n0, SoLayers *out);

/* ---------------- scenes.py small helpers ---------------- */
GEOSGeometry *so_u(const GEOSGeometry *const *gs, int n);                   /* _u: None / empties left out; nothing -> Polygon() */
GEOSGeometry *so_disc(double x, double y, double r, int n);                 /* _disc (n = 20) */
GEOSGeometry *so_ann(double x, double y, double ro, double ri, int n);      /* _ann (n = 24) */
GEOSGeometry *so_cap(const double *p, const double *q, double w);           /* _cap */
GEOSGeometry *so_bar(const double *p, const double *q, double w);           /* _bar */

/* ---------------- style_details.py tools ---------------- */
GEOSGeometry *sd_U(const GEOSGeometry *const *gs, int n);                   /* U: nothing -> GEOMETRYCOLLECTION EMPTY */
GEOSGeometry *sd_disc(double x, double y, double r, int n);                 /* n = 20 */
GEOSGeometry *sd_ann(double x, double y, double ro, double ri, int n);      /* n = 24 */
GEOSGeometry *sd_capl(const double *p, const double *q, double w);
GEOSGeometry *sd_barl(const double *p, const double *q, double w);
GEOSGeometry *sd_line(const double *pts, int n, double w);
GEOSGeometry *sd_ell(double x, double y, double rx, double ry, double a, int n);          /* a = 0.0, n = 20 */
GEOSGeometry *sd_hull(const GEOSGeometry *const *gs, int n);
GEOSGeometry *sd_ngon(double x, double y, double r, int n, double a);
int sd_arc(double x, double y, double r, double a0, double a1, int n, double *xy);        /* n + 1 points (n = 12) */
int sd_spiral(double x, double y, double r0, double r1, double turns, double a, int n, double *xy);   /* n + 1 points (n = 40) */
void sd_pol(double x, double y, double r, double t, double *xy);
int sd_ringpts(double x, double y, double r, int n, double a, double jit, PyRandom *rng, double *xy);   /* rng NULL: None */
GEOSGeometry *sd_wavy(const double *p, const double *q, PyRandom *rng, double amp, int n, double w);   /* amp .5, n 5, w .5; NULL on error */
GEOSGeometry *sd_tri(const double *p, const double *q, double w);
GEOSGeometry *sd_rrect(double x, double y, double L, double W, double a, double r);       /* r = 0.3 */
int sd_cyl(SoSolids *out, double x, double y, double r, double h, double lift, int n);    /* lift 0, n 20 */
int sd_blk(SoSolids *out, double x, double y, double L, double W, double a, double h, double lift);
int sd_ext(SoSolids *out, const GEOSGeometry *poly, double h, double lift);
int sd_dome(SoSolids *out, double x, double y, double r, double h, double lift, double sq, double a, int n);   /* 0, 1.0, 0.0, 20 */
int sd_cone(SoSolids *out, double x, double y, double r, double h, double tip, double lift, int n, double sq, double a);   /* tip .24, 0, 16, 1, 0 */
int sd_turn(SoSolids *out, double x, double y, const double *prof, int np, int n, double sq, double a, double lift);    /* n 24 */
int sd_lie(SoSolids *out, double x, double y, double a, double L, double r, double lift);
int sd_ridge(SoSolids *out, double x, double y, double a, double L, double W, double h, double lift);
int sd_loaf(SoSolids *out, double x, double y, double a, double L, double W, double h, double lift);
int sd_rod(SoSolids *out, const double *pts, int npts, double r, SoNumFn rfun, void *ctx, double hk, double lift, double arch);   /* hk 1.0 */
int sd_spin(SoSolids *out, double x, double y, double a, double L, SoNumFn rf, void *ctx, double hk, double lift, const char *prof);   /* hk 1.2, "round" */
int sd_crv(double r0, double r1, double z0, double z1, int out_, int n, double *prof);    /* out True, n 6: n + 1 pairs */
int sd_ST(const SoSolids *solids, const char *kind, int has_parent, int64_t parent, const SoCut *cuts, int ncuts, SoLayer **out);
int sd_pot(SoSolids *out, double x, double y, double r, double h, double neck, double lip, double lift, int n);   /* .5, .66, 0, 24 */
int sd_rock(SoSolids *out, double x, double y, double r, double h, PyRandom *rng, double sq, double lift, int n);   /* sq 0 = None; n 9 */
int sd_fluted(SoSolids *out, double x, double y, double r, double h, int n, double a, double deep, int m, double twist, double lift);   /* 8, 0, .8, 6, 0, 0 */
int sd_hang(SoSolids *out, const GEOSGeometry *poly, double z0, double z1);

/* ---------------- style_details.Obj ---------------- */
typedef struct { GEOSGeometry **v; int n, cap; } SoGeoms;          /* owned geometries */
typedef struct { const char *k; SoLayers L; SoGeoms F; } SoObj;
void so_obj_init(SoObj *o, const char *kind);           /* Obj(kind) */
void so_obj_free(SoObj *o);
/* each returns the index of its layer (>= 0), SO_ERR, or (cut / groove / seams / vents / gcut, which return None
   in Python) SO_OK. kind NULL = None (-> o.k); par: has_par 0 = None. g is borrowed (cloned in). */
int so_obj_up(SoObj *o, const GEOSGeometry *g, double h, const char *kind);
int so_obj_on(SoObj *o, const GEOSGeometry *g, int has_par, int64_t par, double h, const char *kind);
int so_obj_cut(SoObj *o, const GEOSGeometry *g, int has_par, int64_t par, double d);     /* d .18 */
int so_obj_st(SoObj *o, const SoSolids *solids, const char *kind, int has_par, int64_t par, const SoCut *cuts, int ncuts, const GEOSGeometry *foot);
int so_obj_gcut(SoObj *o, const GEOSGeometry *g, double d, const char *kind);
int so_obj_out(SoObj *o, const GEOSGeometry *const *extra, int nextra, GEOSGeometry **hull);   /* hull(F + extra); the layers are o->L */
int so_obj_lid(SoObj *o, const GEOSGeometry *g, int has_par, int64_t par, double h, double edge, const char *kind);   /* .16 .16 */
int so_obj_groove(SoObj *o, const GEOSGeometry *g, int has_par, int64_t par, double w, double d);                    /* .2 .16 */
int so_obj_seams(SoObj *o, const SoFrame *ff, double L, double W, int has_par, int64_t par, double step, int across, double w, double d, const GEOSGeometry *skip);   /* F .2 .16 None */
int so_obj_studs(SoObj *o, const double *pts, int npts, int has_par, int64_t par, double r, double h, const char *kind, int n);   /* .27 .15 None 8 */
int so_obj_handle(SoObj *o, const double *p, const double *q, int has_par, int64_t par, double w, double h, const char *kind);   /* .46 .42 */
int so_obj_tabs(SoObj *o, const SoFrame *ff, const double *xs, int nx, double v, int has_par, int64_t par, double a, double l, double w, double h, const char *kind);   /* .5 .62 .17 */
int so_obj_ribs(SoObj *o, const SoFrame *ff, double L, double W, int has_par, int64_t par, int n, double a, double w, double h, int across, const char *kind);   /* .46 .15 True */
int so_obj_vents(SoObj *o, const SoFrame *ff, double x, double v, int has_par, int64_t par, int n, double l, double a, double step, double w, double d);   /* 1.0 0 .42 .2 .22 */
int so_obj_corners(SoObj *o, const SoFrame *ff, double L, double W, int has_par, int64_t par, double a, double size, double h, const char *kind);   /* .6 .14 */
int so_obj_straps(SoObj *o, const SoFrame *ff, const double *xs, int nx, double W, int has_par, int64_t par, double a, double w, double h, const char *kind);   /* .5 .13 */

/* gg.stone(cx, cy, r, rng, aspect, nv) for the non-round case with aspect and nv given (what _weather draws) */
GEOSGeometry *so_stone(double cx, double cy, double r, PyRandom *rng, double aspect, int nv);   /* TODO-leaf: G2 owns gg.stone */
#endif
