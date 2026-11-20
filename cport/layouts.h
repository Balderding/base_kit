/* layouts.h: the ground layouts of every style in C (survey group G11): ORIG[style](n) as gg3.make calls it.
   Files: layouts.c (this API, gg3.make's first part, the shared helpers below), layouts_gg2.c (the gg2.py layouts as
   sizefix rewrites them: badlands, highland, corridor_a/_x, infestation, urban_a/_x/urban_layout, mine, mine_v3,
   maahl, maahl_v3 and their helpers), layouts_s2.c (styles2.py: its 12 layouts and floor, j_*, pools, channel,
   cracks, banks, grit, _fr, _D, KK, NN), layouts_new.c (style_ashveil/blight/ferrum/obelis/sundered/thornwild
   layout(n)), groundbig.c (ground_big.py: Ground and apply). Each C function names the Python routine it copies.
   Proof: work\cscene\layouts (lprove.py: the Python layout and the C one, scene dumps compared byte for byte).

   The gg.py layouts (gg.badlands() ... gg.urban(), no argument) are not reached: gg3.ORIG starts from gg2.STYLES,
   which has its own layouts under every one of those names, so they are not ported.

   Python semantics kept:
   - an exception inside a layout propagates out of gg3.make (no try around ORIG[style]): ly_orig / ly_ground then
     return NULL with *err = 1 (the scene as it was is lost, as in Python). Local try/except blocks of the Python
     (infestation's plate cut, ground_big's per-feature try) are copied where they are.
   - the module state gg3.make sets around the layout: gg2.mk (the scene's name / seed suffix), gg2.FMIX (pickF's
     random feature pick), gg2.SCENE_SKIP, gg.LAYOUT (SC.layout, set by ly_ground for the layout's duration).
   Call scene_config(), pl_config() (ground_big: evict) and ly_config() first. */
#ifndef LAYOUTS_H
#define LAYOUTS_H
#include <setjmp.h>
#include "scene.h"
#include "leaf.h"
#include "place.h"
#include "shp.h"
#include "pyexact.h"

/* ================================================================ the API */
typedef struct {
    int random_mix;           /* CFG random_mix (default True) */
    int scenes;               /* CFG scenes (default False; settings.json True): gg2.SCENE_SKIP */
    int outcrop_cracks;       /* CFG outcrop_cracks (default False): gg2.highland */
} LyCfg;
extern LyCfg LYC;
void ly_config(int random_mix, int scenes, int outcrop_cracks);

/* the gg2.mk that gg3.make installs: Scene(f"{st}_{n:02d}", f"{title} #{n}", f"{st}{k}{suf}", ground); fmix =
   gg2.FMIX (NULL = None); scene_skip = gg2.SCENE_SKIP is {"track", "ruts", "planks", "trail"} (else empty);
   salt / reseed = os.environ SALT / RESEED as the layout reads them (urban's own Random) */
typedef struct { int n; const char *suf; PyRandom *fmix; int scene_skip; const char *salt, *reseed; } LyMk;
int ly_has_style(const char *style);                       /* style in gg3.ORIG */
/* ORIG[style](nb): a new scene, or NULL (*err = 1: Python raised) */
Scene *ly_orig(const char *style, int nb, const LyMk *mk, int *err);
/* gg3.make(style, n) up to and including the rename after the layout (s.name, s.title = ...): the n > 20 recursion
   (SALT + "x<n>"), nb / suf / mixr, the RND feature mix, ORIG[style](nb). salt / reseed: os.environ SALT / RESEED.
   *mixr gets mixr as make has it after the layout (make keeps using it). Python's make leaves os.environ["SALT"]
   = salt + "x<n>" behind for n > 20: *salt_after gets that (NULL to ignore; static storage). */
Scene *ly_ground(const char *style, int n, const char *salt, const char *reseed, PyRandom *mixr, const char **salt_after, int *err);

/* ground_big.apply(sc) (groundbig.c). The compose state it reads: */
typedef struct {
    Scene *s; const char *style;
    Geo *wide;                /* sc.wide (lay_cut intersects with it) */
    Geo *core;                /* sc.core (NULL = None; lay_cut with meet=True does not use it) */
    double final_mm;          /* float(os.environ.get("KIT_FINAL_MM") or 0) */
    double true_scale;        /* features.true_scale() (KIT_SCALE, 1.0) */
    const char *salt, *reseed;          /* os.environ SALT / RESEED */
    Geo *stand_body;          /* s.stand.get("body") (NULL = None / no stand) */
    GeoList stand_pads;       /* s.stand.get("pads") (as given, None entries left out) */
    int nlog;                 /* sc.log entries apply added (a caught exception each) */
} LyGbSc;
/* the scenes.py sets ground_big reads and adds to: TERRK (gb_* kinds added), WETK, BRIDGE (gg3.BRIDGE) */
typedef struct { const char **v; int n, cap; } LyStrSet;
extern LyStrSet LY_TERRK, LY_WETK, LY_BRIDGE, LY_SOFT, LY_CHAMFER;
int lyset_has(const LyStrSet *s, const char *k);
void lyset_add(LyStrSet *s, const char *k);
void ly_sets_default(void);              /* the sets as the kit has them when compose runs (see groundbig.c) */
/* returns g.made (0 when not applied); SC_ERR if Python would raise out of apply */
int ly_ground_big(LyGbSc *sc);

/* ================================================================ shared by the layout files (internal) */
/* the layout's error exit: Python raised. Every helper below raises through it. */
extern jmp_buf *ly_jb;
void ly_raise(void) __attribute__((noreturn));
static inline int LC(int r) { if (r < 0) ly_raise(); return r; }          /* a predicate / scene status: -1 raises */

/* the arena: every geometry and list the helpers make is held here until the layout ends (or ly_pop) */
void ly_arena_begin(void);
void ly_arena_end(void);
int ly_mark(void);
void ly_pop(int mark);                   /* drops what was made after mark (lists made before keep their own refs) */
Geo *LA(GEOSGeometry *g);                /* arena-own g; NULL raises */
Geo *LAG(Geo *h);                        /* arena-own a handle (takes the caller's reference); NULL raises */
GeoList *LL(void);                       /* a new list (freed with the arena) */
GeoList *L1(Geo *g);                     /* [g] */
GeoList *L2(Geo *a, Geo *b);
static inline void LP(GeoList *l, Geo *g) { gl_push(l, g); }
void *LM(size_t n);                      /* arena memory */

/* numbers */
#define NF(x) num_f(x)
#define NI(x) num_i(x)
#define NONE num_none()
#define PI 3.141592653589793
double ly_pow(double x, double y);       /* float ** float (raises where Python does / gives complex) */
int64_t ly_rint(double x);               /* round(x) -> int */
double ly_round(double x, int nd);       /* round(x, nd) */
int64_t ly_int(double x);                /* int(x) */
static inline int64_t ly_mod(int64_t a, int64_t b) { int64_t m = a % b; return (m != 0 && ((m < 0) != (b < 0))) ? m + b : m; }   /* a % b */
int ly_arange(double a, double b, double st, double **out);          /* np.arange (float64): arena array */
int ly_linspace(double a, double b, int n, double **out);
static inline int64_t KN_(double v) { double k = SC.K > 1.0 ? SC.K : 1.0; return v >= 0 ? (int64_t)ceil(v * k) : -(int64_t)ceil(-v * k); }   /* sizefix._KN */
static inline double KX_(double v) { double k = SC.K > 1.0 ? SC.K : 1.0; return v * k; }           /* sizefix._KX */
int64_t KA_(double v);                    /* sizefix._KA: int(round(v * _k() ** 2)) */

/* geometry (arena results; inputs are never changed) */
Geo *g_empty(void);                       /* Polygon() */
Geo *g_pt(double x, double y);
Geo *g_ptbuf(double x, double y, double r, int q);                    /* Point(x, y).buffer(r, q) */
Geo *g_buf(Geo *g, double d);             /* g.buffer(d) */
Geo *g_bufq(Geo *g, double d, int q);     /* g.buffer(d, q) */
Geo *g_bufx(Geo *g, double d, int q, int cap, int join, double mitre);
Geo *g_poly(const double *xy, int n);
Geo *g_box(double x0, double y0, double x1, double y1);
Geo *g_line(const double *xy, int n);
Geo *g_linebuf(const double *xy, int n, double w, int cap, int q);  /* LineString(pts).buffer(w, cap_style=cap, resolution=q) */
Geo *g_inter(Geo *a, Geo *b);
Geo *g_diff(Geo *a, Geo *b);
Geo *g_union(Geo *a, Geo *b);             /* a.union(b) */
Geo *g_uu(const GeoList *l);              /* unary_union(list) */
Geo *g_uu2(Geo *a, Geo *b);               /* unary_union([a, b]) */
Geo *g_simp(Geo *g, double t);            /* g.simplify(t) */
Geo *g_hull(Geo *g);
Geo *g_centroid(Geo *g);
Geo *g_reppt(Geo *g);
Geo *g_rot(Geo *g, double deg, int origin, double ox, double oy);    /* affinity.rotate (degrees) */
Geo *g_scale(Geo *g, double fx, double fy, int origin, double ox, double oy);
Geo *g_trans(Geo *g, double dx, double dy);
Geo *g_inset(Geo *p, double d);           /* gg.inset */
Geo *g_ext(Geo *p);                       /* p.exterior (a copy) */
Geo *g_interp(Geo *line, double d);       /* line.interpolate(d) */
GeoList *g_polys(Geo *g);                 /* gg.polys_of(g) */
Geo *g_max_area(const GeoList *l);        /* max(l, key=area) (l not empty) */
double g_area(Geo *g);
double g_len(Geo *g);
double g_dist(Geo *a, Geo *b);
void g_bounds(Geo *g, double b[4]);
double g_x(Geo *p);
double g_y(Geo *p);
int g_isempty(Geo *g);
int g_contains(Geo *a, Geo *b);
int g_intersects(Geo *a, Geo *b);
int g_valid(Geo *g);
int g_type(Geo *g);
int g_wide(Geo *p, double w);             /* gg.wide_enough */
Geo *g_fit(Geo *p, double d);             /* gg.fit_for_taper (NULL = None) */
Geo *g_stone(double cx, double cy, double r, PyRandom *rng, double aspect, int nv);          /* gg.stone(cx, cy, r, rng, aspect, nv) (0 = None) */
Geo *g_stone_r(double cx, double cy, double r, PyRandom *rng, double aspect, int nv, int round_);
Geo *g_wigline(double ax, double ay, double bx, double by, PyRandom *rng, double amp, int n);   /* LineString(wiggle(a, b, rng, amp, n)) */
int g_coords(Geo *g, double **xy);        /* list(g.coords) of a line / ring: count (arena array) */
Geo *g_frpoly(const Frame *f, const double *uv, int n);                /* Polygon([f(u, v) ...]) */
Geo *g_R(const Frame *f, double u0, double u1, double v0, double v1);  /* the layouts' R = lambda u0, u1, v0, v1: ... */
Geo *g_linecut(double ax, double ay, double bx, double by, double w); /* gg2.line_cut */

/* the Scene, raising where the Python raises */
Kind *KS(const char *s);                  /* the str s as a kind */
GeoList *S_prism(Scene *s, const GeoList *polys, Num top, const char *kind, Num z0, double minw, int foot);   /* s.prism(...) -> the list it returns */
GeoList *S_prismk(Scene *s, const GeoList *polys, Num top, Kind *kind, Num z0, double minw, int foot);
#define PR(s, l, top, kind) S_prism(s, l, NF(top), kind, NONE, SC.MINW, 1)
GeoList *S_stack(Scene *s, const GeoList *polys, Num h, const Tier *t, int nt, const char *kind, Num z0, Num *top);   /* (profs, top) */
void S_gcut(Scene *s, const GeoList *polys, Num depth, const char *kind);
void S_tcut(Scene *s, const GeoList *polys, Num z0, Num z1, const char *kind);
GeoList *S_scatter(Scene *s, int n, double rmin, double rmax, Geo *region, double gap, int round_, int tries);
int S_free(Scene *s, Geo *p, double gap);
void S_taper(Scene *s, const GeoList *polys, Num z, Num h, Num a, const char *kind);   /* s.ops.append(("taper", ...)) */
void S_tadd(Scene *s, Geo *fp, Num zb, Num zt, Geo *top);              /* gg2.T_add */
void S_tzr(Scene *s, Geo *p, Num *lo, Num *hi);
int S_tfree(Scene *s, Geo *p, double gap);
Num S_tflat(Scene *s, Geo *p);
GeoList *S_tprism(Scene *s, const GeoList *polys, Num h, const char *kind, Num **tops);   /* gg2.tprism: [(g, top)] as the geos + their tops */
void S_tstack(Scene *s, const GeoList *polys, Num h, const Tier *t, int nt, const char *kind);
void S_tcutflat(Scene *s, const GeoList *polys, Num depth, const char *kind);
GeoList *S_scatter_any(Scene *s, int n, double rmin, double rmax, Geo *region, double gap, int round_, int tries);
void S_extend_feet(Scene *s, const GeoList *l);
void S_extend_objs(Scene *s, const GeoList *l);

/* a voronoi joint / crack net: pts = MultiPoint([(rng.uniform(lo, hi), rng.uniform(lo, hi)) for _ in range(n)]); the
   cells of voronoi_diagram(pts, envelope=box(e0, e0, e1, e1)), their edges rounded to 3 decimals in a set (CPython's
   order); then unary_union([LineString(wiggle(a, b, rng, amp, 3)) if wig else LineString([a, b]).buffer(w,
   cap_style=1, resolution=q) for a, b in edges]) */
Geo *ly_vor_net(PyRandom *rng, double lo, double hi, int64_t n, double e0, double e1, int wig, double amp, double w, int q);

/* the layouts, one per file (ORIG[style] = the function named in ly_orig's table) */
typedef Scene *(*LyFn)(int n);
extern const LyMk *LYMK;                  /* the mk in force while a layout runs */
Scene *ly_mk(const char *style, const char *title, int k, const char *ground);          /* gg2.mk as gg3.make installs it */
/* gg2.pickF(table, n): table = n -> "w w w" strings for keys k0..k0+cnt-1; out gets the words (static storage); count */
int ly_pickF(const char *const *table, int k0, int cnt, int n, const char **out, int cap);
int ly_count(const char *const *F, int nF, const char *w);           /* F.count(w) */
static inline int ly_in(const char *const *F, int nF, const char *w) { return ly_count(F, nF, w) > 0; }

Scene *gg2_badlands(int n); Scene *gg2_highland(int n); Scene *gg2_maahl(int n); Scene *gg2_corridor(int n);
Scene *gg2_infestation(int n); Scene *gg2_urban(int n); Scene *gg2_mine(int n); Scene *gg2_mine_v3(int n); Scene *gg2_maahl_v3(int n);
Scene *s2_swamp(int n); Scene *s2_cynder(int n); Scene *s2_targa(int n); Scene *s2_trederra(int n); Scene *s2_fortress(int n);
Scene *s2_beast(int n); Scene *s2_frontier(int n); Scene *s2_graveyard(int n); Scene *s2_forest(int n); Scene *s2_dungeon(int n);
Scene *s2_temple(int n); Scene *s2_coast(int n);
Scene *new_ashveil(int n); Scene *new_blight(int n); Scene *new_ferrum(int n); Scene *new_obelis(int n);
Scene *new_sundered(int n); Scene *new_thornwild(int n);
/* gg2 helpers the other files use */
Geo *gg2_crater(Scene *s, double cx, double cy, double r, double depth);
/* styles2 helpers the new styles use */
double s2_KK(void);
int64_t s2_NN(int64_t n);
Geo *s2_D(void);
typedef struct { double phi; Frame f; } S2Fr;
S2Fr s2_fr(PyRandom *rng);
Geo *s2_R(const S2Fr *fr, double u0, double u1, double v0, double v1);
Geo *s2_floor(Scene *s, Geo *region, double h, const char *kind, Geo *joints, double depth, double edge);
Geo *s2_channel(Scene *s, PyRandom *rng, double w, double depth, const char *kind, double amp);
Geo *s2_pools(Scene *s, PyRandom *rng, int64_t n, double r0, double r1, double depth, const char *kind, Geo *avoid);
void s2_cracks(Scene *s, PyRandom *rng, int64_t n, double depth, const char *kind, double w);
void s2_banks(Scene *s, PyRandom *rng, int64_t n, double r0, double r1, double h0, double h1, const char *kind, const Tier *t, int nt, Geo *region);
void s2_grit(Scene *s, PyRandom *rng, int n0, int n1, const char *kind);
Geo *s2_j_planks(double phi, double w, double L, double jw);         /* L 0 = None */
Geo *s2_j_grid(double phi, double sx, double sy, double jw);         /* sy 0 = None */
Geo *s2_j_voronoi(PyRandom *rng, double n, double jw, double amp);
Geo *s2_j_hex(double r, double phi, double jw);
extern const Tier S2_ROCKY[2], S2_SOFTT[2];
#endif
