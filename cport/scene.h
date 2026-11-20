/* scene.h: the scene object s of the kit (gg.Scene / gg2.Scene) in C, and everything that writes it: the Scene
   methods of gg.py and gg2.py (as sizefix leaves them: the Scene classes and the T_* functions are not rewritten),
   gg.LK, gg3.Z and the gg2 terrain functions T_init .. scatter_any. Proof: work\cscene\scene\prove_scene.py.

   Python semantics kept:
   - Geo is a geometry HANDLE: identity (`is`, id()) is the pointer. Handles are reference counted, so the same
     polygon can sit in s.ops, s.feet and s.objs at once, as in Python.
   - Num is a Python number with its type: int, float, bool, gg3.Z (a float with .bridge) or numpy.float64, because
     the type shows in repr() (hashes, build commands) and decides round() and the results of + - *.
   - Kind is a material name: a str (equal by value) or a gg.LK (equal only to itself, with .loft and .ghost).
   - every routine returns a status; SC_ERR (-1) = Python would raise here (GEOSException, IndexError...), the
     scene is then as Python leaves it at that point.
   Call scene_config() once before making scenes (it calls shp_init). */
#ifndef SCENE_H
#define SCENE_H
#include <stdint.h>
#include <string.h>
#include "geos_dyn.h"
#include "pyrandom.h"

#define SC_ERR (-1)

/* ---------------- geometry handles ---------------- */
typedef struct Geo { GEOSGeometry *g; int rc; } Geo;
Geo *geo_new(GEOSGeometry *g);            /* takes g over (NULL -> NULL); rc = 1 */
Geo *geo_ref(Geo *g);                     /* rc + 1, returns g (NULL is fine) */
void geo_unref(Geo *g);                   /* rc - 1, frees at 0 (NULL is fine) */
typedef struct { Geo **v; int n, cap; } GeoList;          /* a Python list of geometries: holds one reference each */
void gl_push(GeoList *l, Geo *g);         /* append, adds a reference */
void gl_push_own(GeoList *l, Geo *g);     /* append, takes the caller's reference over */
void gl_extend(GeoList *l, const GeoList *o);             /* l += o */
void gl_clear(GeoList *l);                /* l = [] (drops the references, keeps the array) */
void gl_free(GeoList *l);
void gl_copy(GeoList *dst, const GeoList *src);           /* dst = list(src) */
int gl_index(const GeoList *l, const Geo *g);             /* index of g by identity, -1 */

/* ---------------- numbers ---------------- */
enum { NUM_FLOAT = 0, NUM_INT = 1, NUM_Z = 2, NUM_BOOL = 3, NUM_NP = 4, NUM_NONE = 255 };
typedef struct { double v; unsigned char t, bridge; } Num;          /* bridge: gg3.Z.bridge (only for NUM_Z) */
static inline Num num_f(double v) { Num n = { v, NUM_FLOAT, 0 }; return n; }
static inline Num num_i(double v) { Num n = { v, NUM_INT, 0 }; return n; }
static inline Num num_none(void) { Num n = { 0, NUM_NONE, 0 }; return n; }
static inline Num num_z(double v, int bridge) { Num n = { v, NUM_Z, (unsigned char)(bridge != 0) }; return n; }
Num num_add(Num a, Num b);                /* a + b with Python's result type */
Num num_sub(Num a, Num b);
Num num_mul(Num a, Num b);
Num num_round(Num a, int nd);             /* round(a, nd): float -> correctly rounded; int stays; numpy.float64 -> numpy's round */
static inline Num num_max2(Num a, Num b) { return b.v > a.v ? b : a; }          /* max(a, b): the first of equal values */
static inline Num num_min2(Num a, Num b) { return b.v < a.v ? b : a; }          /* min(a, b) */

/* ---------------- kinds (material names) ---------------- */
typedef struct Kind {
    int rc; int lk; char *s;              /* lk: a gg.LK object; else a plain str (interned, never freed) */
    int has_loft; Num loft_chain, loft_z; /* LK.loft = (chain, z), when set */
    int has_ghost; Num ghost;             /* LK.ghost, when set */
} Kind;
Kind *kind_str(const char *s);            /* the str s (interned: the same pointer for the same text) */
Kind *kind_lk(const char *s);             /* gg.LK(s): a new object, rc = 1 */
Kind *kind_ref(Kind *k);
void kind_unref(Kind *k);
int kind_eq(const Kind *a, const Kind *b);                /* a == b as Python compares them */
/* kind_is(k, "s"): k == "s" (below) */

/* ---------------- ops, cuts, chops, terrain ---------------- */
enum { OP_PRISM = 0, OP_TAPER = 1 };
/* ("prism", [P], z0, top, kind) or ("taper", [P], z, h, angle, kind); a tuple: never changed after it is made */
typedef struct Op { int rc; int type; GeoList polys; Num n[3]; Kind *kind; int feat; const char *fname; } Op;          /* feat / fname: the detail that made it (audit.c); 0 = none */
extern int sc_feat; extern const char *sc_fname;          /* the detail being placed now: new ops get it */
double sc_uncovered(const GEOSGeometry *g, const GEOSGeometry *const *plates, int n);          /* the area of g off the plates, joints between them (up to 0.7 mm) not counted */
extern double sc_min_part, sc_min_cut;          /* readable details (scene.c): thinnest detail part, narrowest cut, mm */
extern int (*kd_wet_hook_fn)(const Kind *k);
int sc_feat_push(const char *name);                       /* a new detail starts (only at the outermost call); returns what to give sc_feat_pop */
void sc_feat_pop(int saved);
Op *op_like(const Op *o, const GeoList *ps);              /* the same op on other polygons, its tag kept */
Op *op_prism(const GeoList *polys, Num z0, Num top, Kind *kind);           /* copies the list, refs the kind */
Op *op_taper(const GeoList *polys, Num z, Num h, Num angle, Kind *kind);
Op *op_ref(Op *o);
void op_unref(Op *o);
typedef struct { Op **v; int n, cap; } OpList;
void ol_push(OpList *l, Op *o);           /* adds a reference */
void ol_push_own(OpList *l, Op *o);
void ol_free(OpList *l);
typedef struct { GeoList polys; Num depth; Kind *kind; } Gcut;              /* s.gcuts entry ([P], depth, kind) */
typedef struct { Gcut *v; int n, cap; } GcutList;
typedef struct { GeoList polys; Num z0, z1; Kind *kind; } Tcut;             /* s.tcuts entry ([P], z0, z1, kind) */
typedef struct { Tcut *v; int n, cap; } TcutList;
typedef struct Chop {                     /* s.chops entry: {"kind", "poly", "z0", "top", "run", "drop", "ops"[, "iso"]} */
    int rc; Kind *kind; Geo *poly; Num z0, top, run, drop; OpList ops; int has_iso; Num iso;
} Chop;
typedef struct { Chop **v; int n, cap; } ChopList;
typedef struct { Geo *fp; Num zb, zt; Geo *top; } Terr;                    /* s.terr entry (footprint, zb, zt, top) */
typedef struct { Terr *v; int n, cap; } TerrList;
void gcl_push(GcutList *l, const GeoList *polys, Num depth, Kind *kind);   /* copies the list */
void tcl_push(TcutList *l, const GeoList *polys, Num z0, Num z1, Kind *kind);
void trl_push(TerrList *l, Geo *fp, Num zb, Num zt, Geo *top);              /* refs fp and top */
void chl_push_own(ChopList *l, Chop *c);

/* ---------------- the scene ---------------- */
typedef struct Scene {
    char *name, *title, *ground;
    PyRandom rng;                         /* s.rng = random.Random(seed) */
    int gg2;                              /* a gg2.Scene (its clip / cclip / gcut / tcut / final_gcuts / free) */
    Geo *disc, *cdisc;                    /* Point(0, 0).buffer(CLIP, 256); gg2: cdisc = .buffer(CUTR, 256) */
    int has_kc; int64_t kc;               /* gg2 clip counter (absent until the first clip) */
    OpList ops; GcutList gcuts; TcutList tcuts; GeoList feet; ChopList chops;
    int has_terr; TerrList terr;          /* T_init: hasattr(s, "terr") */
    int has_objs; GeoList objs, keepout;  /* hasattr(s, "objs"): T_init, or gg3.make for a flat style */
    int has_base_ops; OpList base_ops;    /* s.base_ops = {id(o) for o in s.ops}: kept as the ops themselves */
    int has_held; GeoList held;
    int has_cutgap; Num cutgap;           /* getattr(s, "cutgap", 0.05) */
    int has_phi; Num phi;
    Geo *wash;                            /* s.wash (LineString) or NULL */
    /* compose state (theme, plan, notes, stand, solids3d...) belongs to scenes.py: added with G9 */
} Scene;

typedef struct {                          /* the module globals of gg / gg2 the scene code reads */
    double base_mm, R0, H, K, CLIP, BLANK, SINK, MINW, LEDGE, LAP, ts;     /* gg (ts = gg._TS) */
    double CUTR, CAP;                     /* gg2: CUTR = R0 + 1; CAP = H + gg.max_height() */
    int mesh, layout, chamfer_rocks, base_first;          /* gg.MESH, gg.LAYOUT, CFG chamfer_rocks / base_first */
} SceneCfg;
extern SceneCfg SC;
/* base_mm = gg.BASE_MM; mesh = KIT_ENGINE == "mesh"; ts = gg._TS; max_h = gg.max_height(); gg2 = gg2 imported
   (it sets gg.CLIP = R0 + 0.15, else R0 + 1.0). Returns 1 when GEOS is ready. */
int scene_config(double base_mm, int mesh, double ts, double max_h, int chamfer_rocks, int base_first, int gg2);

Scene *scene_new(const char *name, const char *title, const char *seed, const char *ground, int gg2);   /* gg.Scene / gg2.Scene(...) */
Scene *scene_mk(const char *style, const char *title, int n, const char *ground);  /* gg2.mk(style, title, n, ground) */
void scene_free(Scene *s);

/* gg.Scene / gg2.Scene methods. Input lists are read only; results are appended to out lists. */
int sc_clip(Scene *s, const GeoList *polys, GeoList *out);              /* s.clip(polys) (gg2: counts s.kc) */
int sc_cclip(Scene *s, const GeoList *polys, GeoList *out);             /* gg2.Scene.cclip */
/* live.hook_layout: when set, called after s.prism / s.stack ('r') and s.gcut / s.tcut ('c') succeed, with the polys
   as they were passed (the window's live view draws their outlines). NULL = nothing; what is built is the same either way */
extern void (*sc_piece_hook)(char kind, const GeoList *polys);
int sc_gcut(Scene *s, const GeoList *polys, Num depth, Kind *kind);     /* s.gcut(polys, depth, kind) */
int sc_tcut(Scene *s, const GeoList *polys, Num z0, Num z1, Kind *kind);
/* s.prism(polys, top, kind, z0=None|z0, minw, foot): the pieces go to out (the op's own list) */
int sc_prism(Scene *s, const GeoList *polys, Num top, Kind *kind, Num z0, double minw, int foot, GeoList *out);
typedef struct { Num frac, a; } Tier;
/* s._tiers(profs, z, h, tiers, kind): ops -> ops_out, top profiles -> tops, *zt = z */
int sc_tiers(Scene *s, const GeoList *profs, Num z, Num h, const Tier *tiers, int nt, Kind *kind, OpList *ops_out, GeoList *tops, Num *zt);
int sc_solids(Scene *s, const GeoList *profs, Num z, Num top, Num hb, const Tier *tiers, int nt, Kind *kind, GeoList *tops);
/* s.stack(polys, h, tiers, kind, z0): (top profiles -> tops, *top) */
int sc_stack(Scene *s, const GeoList *polys, Num h, const Tier *tiers, int nt, Kind *kind, Num z0, GeoList *tops, Num *top);
/* s.scatter(n, rmin, rmax, region, gap, round_, tries) (tries 0 = None) */
int sc_scatter(Scene *s, int n, double rmin, double rmax, Geo *region, double gap, int round_, int tries, GeoList *out);
/* s.final_gcuts() -> out (a new list of cuts) */
int sc_final_gcuts(Scene *s, GcutList *out);
Num sc_top(Scene *s);                                                   /* s.top() */
int sc_free(Scene *s, Geo *p, double gap);                              /* gg2.Scene.free(p, gap): 1 / 0, SC_ERR */

/* gg2 terrain */
void T_init(Scene *s);
void T_add(Scene *s, Geo *fp, Num zb, Num zt, Geo *top);
int T_zr(Scene *s, Geo *p, Num *lo, Num *hi);
int T_flat(Scene *s, Geo *p, Num *z);                                   /* *z = NUM_NONE for None */
int T_free(Scene *s, Geo *p, double gap);
typedef struct { Num lo, hi; GeoList ps; } TGroup;
int T_group(Scene *s, const GeoList *polys, TGroup **groups, int *ng);  /* the dict, in insertion order; free with tgroups_free */
void tgroups_free(TGroup *g, int ng);
typedef struct { Geo *g; Num top; } TPiece;
int tprism(Scene *s, const GeoList *polys, Num h, Kind *kind, TPiece **out, int *nout);    /* [(g, top)]; free(*out) after unref */
int tstack(Scene *s, const GeoList *polys, Num h, const Tier *tiers, int nt, Kind *kind);
int tcut_flat(Scene *s, const GeoList *polys, Num depth, Kind *kind);
int scatter_any(Scene *s, int n, double rmin, double rmax, Geo *region, double gap, int round_, int tries, GeoList *out);

/* ---------------- the scene dump: one byte format, written the same by Python (work\cscene\scene\sstate.py) ----
   used by the proofs: two scenes are the same when their dumps are the same bytes (geometries as WKB, identity as
   first-seen indices, numbers with their Python type, the rng state). */
typedef struct { unsigned char *b; size_t n, cap; } Buf;
typedef struct DumpCtx DumpCtx;
DumpCtx *dump_begin(Buf *out);
void dump_scene(DumpCtx *d, const Scene *s);
void dump_geo(DumpCtx *d, const Geo *g);
void dump_geolist(DumpCtx *d, const GeoList *l);
void dump_num(DumpCtx *d, Num n);
void dump_kind(DumpCtx *d, const Kind *k);
void dump_op(DumpCtx *d, const Op *o);
void dump_raw(DumpCtx *d, int tag, int count);           /* a list / tuple header: tag 'l' or 't', count */
void dump_none(DumpCtx *d);
void dump_end(DumpCtx *d);

static inline int kind_is(const Kind *k, const char *s) { return k && strcmp(k->s, s) == 0; }
#endif
