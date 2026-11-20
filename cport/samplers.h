/* samplers.h: survey group G8 of the C port (samplers.c): the samplers and the removal helpers of scenes.py, and the
   compose state Sc.
   scenes.py   s_near s_rim s_in s_far s_mix s_not (where a detail goes), a_par a_to a_rim (how it is turned),
               focus (gg3.FOCUS / AIM while it is open), _remove _cells clear_small clear_terrain _bigs _plates _top_at
               _strips _at _side _var kfix klass _tag_all (TAGS) _bare_grid mini_type, and the class Sc (state + __init__).
               _u _disc _ann _cap _bar are solids.h's so_u .. so_bar; sm_u .. sm_bar below only wrap them into handles.
   Proof: work\cscene\samplers (smprove.py: real calls recorded while gg3.make builds real bases and replayed on
   smtest.dll in the same process, plus random cases; every geometry compared as WKB, every float by its bits, the rng
   state after each call).

   Python semantics kept:
   - samplers and aims are closures: here a reference counted object (SmAt / SmAim) made by its factory, called with
     sm_at_call / sm_aim_call. NULL = the factory gave None. s_not with nothing to avoid hands back f ITSELF (one more
     reference), as Python returns the same function.
   - every int-returning routine gives 0 (or a count / 1 = found) or SC_ERR (-1) = Python would raise there; the scene
     is then left as Python leaves it at that point.
   - the routines that rebuild s.ops / s.feet / s.objs / s.terr / s.chops / s.tcuts / s.gcuts REPLACE those lists by
     new ones (the old arrays are freed), as Python rebinds s.ops = [...]: iterate a copy across such a call.
     A changed op is a NEW Op (op[0], ps) + op[2:]; clear_terrain with a swap makes every op new (Python rebuilds them all).
   - the kind sets (gg3.BRIDGE, scenes.TERRK, gg3.SOFT, gg.PIECES, gg.CHAMFER_PRISMS, scenes.DEBRIS / GROWTH / CUTK,
     which style_blight / style_obelis extend while they run) are pointers in SM, so the
     integration can point them at the sets other parts add to (layouts.h LY_BRIDGE, LY_TERRK, LY_SOFT, LY_CHAMFER have
     the same layout as SmStrSet). An LK is found in a set by its text, as Python's str hash + __eq__ find it.
   Call scene_config() first (cport\scene.h), then sm_config_default(). */
#ifndef SAMPLERS_H
#define SAMPLERS_H
#include "scene.h"
#include "leaf.h"
#include "place.h"
#include "solids.h"
#include "pyexact.h"

/* ---------------- the kind sets and the colour table hook ---------------- */
typedef struct { const char **v; int n, cap; } SmStrSet;          /* == layouts.h LyStrSet */
int sm_set_has(const SmStrSet *s, const char *k);
void sm_set_add(SmStrSet *s, const char *k);                       /* k is copied (kept for good) */
typedef struct {
    SmStrSet *bridge, *terrk, *soft, *pieces, *chamfer;           /* gg3.BRIDGE, scenes.TERRK, gg3.SOFT, gg.PIECES, gg.CHAMFER_PRISMS */
    SmStrSet *debris, *growth, *cutk;                             /* scenes.DEBRIS, GROWTH, CUTK (klass): style files add to them */
    /* gg.COL.setdefault(k2, gg.COL.get(kind, "#999999")) in kfix (pictures only); NULL = not kept */
    void (*col_setdefault)(void *ctx, const char *k2, const char *kind); void *col_ctx;
} SmCfg;
extern SmCfg SM;
extern SmStrSet SM_BRIDGE0, SM_TERRK0, SM_SOFT0, SM_PIECES0, SM_CHAMFER0, SM_DEBRIS0, SM_GROWTH0, SM_CUTK0;     /* the sets as the .py files define them */
void sm_config_default(void);                                      /* SM points at the *0 sets (filled the first time) */

/* ---------------- samplers: f(rng) -> (x, y) ---------------- */
typedef struct SmAt SmAt;
enum { SM_FRONT = 1, SM_BACK = 2 };                                /* s_near side: 0 = None */
/* s_near(g, d0, d1, side, c, front, n=4): *out NULL = None (g None / empty, or no ring). c / front only with a side. */
int sm_near(Geo *g, double d0, double d1, int side, const double *c, const double *front, int n, SmAt **out);
SmAt *sm_rim(double d0, double d1);                                /* s_rim(d0, d1) (reads SC.R0 at each call, as the lambda reads gg.R0) */
int sm_in(Geo *g, SmAt **out);                                     /* s_in(g) */
SmAt *sm_far(Geo *avoid, double d);                                /* s_far(avoid, d) (R = SC.R0 - 2.0 now) */
/* s_mix(ws): ws = (weight, f) pairs; f NULL / weight <= 0 left out; NULL = None. Weights are Python numbers: the
   kit's are small ints, which a double holds exactly. */
SmAt *sm_mix(const double *w, SmAt *const *f, int n);
SmAt *sm_not(SmAt *f, Geo *avoid, double d);                       /* s_not(f, avoid, d): f itself (new ref) when f / avoid is None or avoid empty */
/* the scenes.py lambdas G9 uses as samplers: */
SmAt *sm_jit(double x, double y, double lim);                      /* no_bare: lambda rng: (x + rng.uniform(-1, 1) * lim * 0.35, y + ...) */
SmAt *sm_ring(double R);                                           /* small_scene ring_: angle uniform(0, 6.2832), radius R * uniform(0.25, 0.5) */
typedef int (*SmAtFn)(void *ctx, PyRandom *rng, double *x, double *y);         /* any other sampler: 0 or SC_ERR */
SmAt *sm_fn(SmAtFn fn, void *ctx);
SmAt *sm_at_ref(SmAt *f);
void sm_at_unref(SmAt *f);
int sm_at_call(SmAt *f, PyRandom *rng, double *x, double *y);      /* f(rng): 0, SC_ERR (Python raised) */
int sm_at_kind(const SmAt *f);                                     /* for tests: SMK_* */
enum { SMK_NEAR, SMK_RIM, SMK_IN, SMK_FAR, SMK_MIX, SMK_NOT, SMK_JIT, SMK_RING, SMK_FN };

/* ---------------- aims: f(rng, x, y, a) -> angle ---------------- */
typedef struct SmAim SmAim;
int sm_a_par(Geo *g, double jit, SmAim **out);                     /* a_par(g, jit=0.12) */
SmAim *sm_a_to(double cx, double cy, double jit);                  /* a_to(c, jit=0.15) */
SmAim *sm_a_rim(void);                                             /* the function a_rim */
typedef int (*SmAimFn)(void *ctx, PyRandom *rng, double x, double y, double a, double *out);
SmAim *sm_aim_fn(SmAimFn fn, void *ctx);
SmAim *sm_aim_ref(SmAim *a);
void sm_aim_unref(SmAim *a);
int sm_aim_call(SmAim *f, PyRandom *rng, double x, double y, double a, double *out);

/* ---------------- focus: `with focus(at, aim):` ---------------- */
/* sets gg3.FOCUS[0] / AIM[0] (leaf.h lf_focus / lf_aimfn) to at / aim (NULL = None) and keeps the old ones in *keep.
   A sampler that raises while called through the hooks sets sm_hook_err (the hooks cannot return an error: the
   caller of the detail checks it, as _run's try / except catches what the detail raised). */
typedef struct { LfFocusFn f; void *fc; LfAimFn a; void *ac; SmAt *at; SmAim *aim; } SmFocus;
void sm_focus_enter(SmFocus *keep, SmAt *at, SmAim *aim);          /* holds a reference to at / aim while open */
void sm_focus_exit(SmFocus *keep);
extern int sm_hook_err;

/* ---------------- removing things ---------------- */
/* _cells(s): {id(e[0]) for e in s.terr}: a set of handles */
typedef struct { const void **t; int64_t mask, n; } SmIdSet;
void sm_ids_add(SmIdSet *s, const void *p);
int sm_ids_has(const SmIdSet *s, const void *p);
void sm_ids_free(SmIdSet *s);
void sm_cells(const Scene *s, SmIdSet *out);
int sm_remove(Scene *s, const GeoList *dead);                      /* _remove(s, dead) */
int sm_clear_small(Scene *s, Geo *g);                              /* clear_small(s, g) */
int sm_clear_terrain(Scene *s, Geo *g);                            /* clear_terrain(s, g) */
int sm_bigs(const Scene *s, GeoList *out);                         /* _bigs(s): appended to out */
/* _top_at(s, x, y): 1 = (top, p) (p a new reference), 0 = None, SC_ERR */
int sm_top_at(const Scene *s, double x, double y, Num *top, Geo **p);
typedef struct { Num z; Geo *g; } SmPlate;                         /* (top z, union): g owned */
int sm_plates(const Scene *s, SmPlate **out, int *n);              /* _plates(s), highest first; free with sm_plates_free */
void sm_plates_free(SmPlate *p, int n);
int sm_strips(Geo *g, double step, GeoList *out);                  /* _strips(g, step=3.0): appended to out. As the kit is now it RAISES
                                                                      (SC_ERR) whenever a strip would be cut: scenes.box is the solid there */
int sm_strips_box(Geo *g, double step, GeoList *out);              /* _strips with shapely's box, as meant (proven against a copy) */

/* ---------------- path helpers ---------------- */
int sm_at_line(Geo *line, double u, double *x, double *y, double *a);              /* _at(line, u) -> (x, y, angle) */
Geo *sm_side(Geo *line, double d, double w);                       /* _side(line, d, w); NULL = raised */
Geo *sm_var(Geo *line, PyRandom *rng, double w0, double w1, double step);          /* _var(line, rng, w0, w1, step=0.9) */

/* ---------------- _u _disc _ann _cap _bar as handles (solids.h so_u .. so_bar) ---------------- */
Geo *sm_u(const GeoList *gs);                                      /* _u(gs): NULL entries / empties left out; nothing -> Polygon() */
Geo *sm_disc(double x, double y, double r, int n);
Geo *sm_ann(double x, double y, double ro, double ri, int n);
Geo *sm_cap(const double *p, const double *q, double w);
Geo *sm_bar(const double *p, const double *q, double w);

/* ---------------- material names and detail classes ---------------- */
Kind *sm_kfix(Kind *kind);                                         /* kfix(kind): NULL (None) -> NULL; may add to SM.chamfer */
/* a detail function's tags (fn.cls / .tmpl / .kind / .shape); NULL = the attribute is not there */
typedef struct { const char *cls, *tmpl, *kind, *shape; } SmTag;
const char *sm_klass(const SmTag *t);                              /* klass(fn) */
int sm_tag_lookup(const char *name, SmTag *out);                   /* TAGS[name] (the qualname before the first "."): 1 found, 0 not */
/* _tag_all over a list of functions: fns[i] gets TAGS[names[i]] (the first time a function is seen); same[i] = the index
   of the first entry that is the same function object (or i). Python's `try ... except AttributeError: pass` is the
   caller's (a function that cannot take attributes: leave it as it is). */
void sm_tag_all(SmTag *fns, const char *const *names, const int *same, int n);

/* _bare_grid(sc, lim): the grid points (P: 2 doubles a point) and their clearance d; *n points (0: P, d NULL).
   free(*P), free(*d). */
int sm_bare_grid(const Scene *s, double lim, double **P, double **d, int64_t *n);

/* mini_type(rng): k = str(KIT_MINI or CFG miniature or "biped").strip().lower(), MINI_ALIAS; "mix" / "random" / "any"
   picks one of MINIS (not "none") with rng.choice. *out = a static name. SC_ERR = SystemExit (unknown type). */
int sm_mini_type(const char *raw, PyRandom *rng, const char **out);

/* ---------------- the compose state (scenes.Sc) ---------------- */
typedef struct ScFn ScFn;                  /* a detail function (the kit's fns / builders: defined by G9 / G10) */
typedef struct { ScFn **v; int n, cap; } ScFnList;
typedef struct ScPlan ScPlan;              /* the plan dict (G9) */
/* PAL[style] (scenes._P): every value a material name or NULL (None); extra = the **kw keys a style adds */
typedef struct { const char *wood, *iron, *rock, *stone, *crate, *water, *pit, *gem, *floor, *plant, *soft, *bone, *twig, *vein;
                 int nextra; const char **extra_k, **extra_v; } ScPal;
typedef struct { char **v; int n, cap; } ScStrList;
typedef struct Sc {
    Scene *s; const char *style; PyRandom *mixr, *rng; int t;      /* t: the tier (an int) */
    double mm, K, R;                       /* gg.BASE_MM, gg.K, gg.R0 */
    ScPal pal;                             /* PAL[style]; compose copies it: sc.pal = dict(PAL[sc.style]) */
    Geo *wide, *disc;                      /* Point(0, 0).buffer(R0 + 1.6, 128), .buffer(R0 - 0.7, 128) */
    Geo *core, *subj, *solid;              /* None, None, Polygon() at first */
    double c[2], front[2];                 /* (0.0, 0.0), (1.0, 0.0) */
    int has_p0; double p0[2];              /* set by place_subject */
    double p1[2];                          /* (0.0, 0.0) */
    int has_q; double q[2];
    const char *mini;                      /* mini_type(mixr) */
    Geo *wet, *lline;                      /* None / unary_union (lay_scene) */
    Geo *pathg, *line;                     /* None until a path is laid */
    int has_start; double start[2];        /* None until then */
    Geo *yard;                             /* None */
    GeoList also;                          /* second features' footprints */
    ScStrList log;                         /* caught errors (kitlog.caught texts) */
    GeoList taken;
    Geo *focal;                            /* None */
    ScFnList marks_later;
    int has_mark_pool; ScFnList mark_pool; /* read with getattr(sc, "mark_pool", []) */
    ScPlan *plan;
    GeoList bigs, anchors, tall_cells;     /* _bigs(s); read_layout */
    Geo *floor, *floor_edge, *waters, *lines;          /* read_layout (floor_edge may be None) */
    int paved;
    int has_rim; GeoList rim;              /* rim_dressing (read with getattr(sc, "rim", [])) */
    GeoList stand_pads;                    /* hold_stand */
} Sc;
/* Sc.__init__(s, style, mixr, rng, t): pal = PAL[style] as the caller has it; mini_raw as for sm_mini_type (it draws
   from mixr for "mix"). SC_ERR: mini_type raised (SystemExit) or GEOS failed. */
int sm_sc_init(Sc *sc, Scene *s, const char *style, PyRandom *mixr, PyRandom *rng, int t, const ScPal *pal, const char *mini_raw);
void sm_sc_free(Sc *sc);                   /* drops the handles and lists (not s, mixr, rng, plan, the fns) */
#endif
