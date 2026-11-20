/* compose.h: survey group G9 of the C port: scenes.py compose and everything it runs (compose.c), plus
   style_details.scatter_made and the themes / PAL / MINIS tables (those live in kinds.h).
   scenes.py   Sc.relation, read_layout, layout_facts, theme_order, expand, pick_plans, validate_plan, _surf, _carve,
               _fits, _taller, _place, _ray_exit, _rim_hit, _curve, place_subject, lay_up, lay_cut, P_ruts .. P_veins,
               lay_path, stand_shapes, hold_stand, hold_yard, release, _share, _count, _snap, _run, no_bare,
               clear_edge, place_also, rim_dressing, place_props, _banned, _samplers, tier_features, clutter, marks,
               lay_bridge, lay_scene, small_scene, _style_made, compose, _solid_of, legal_* and prop_fn.
   Proof: work\cscene\compose (crec.py records real compose runs, ctest.dll replays them; the scene dumps, the plan
   dict and scene_log are compared byte for byte).

   Python semantics kept:
   - every int routine returns 0 / a count / 1 = True, SC_ERR (-1) = an Exception was raised there, CP_EXIT (-2) =
     SystemExit (mini_type / a forced THEME that does not exist): the scene is then left as Python leaves it.
     compose's own try / except blocks catch SC_ERR only, as `except Exception` does.
   - the detail functions (the kit's fns: tier features, DETAILS, the props, style_details' twenty) are ScFn objects
     made by the builder parts (cport\builders.h); compose calls them only through fn->run(fn, s, rng, lvl), inside a
     focus (samplers.h sm_focus_enter) as _run does. A sampler that raised while the detail called it (sm_hook_err)
     counts as the detail raising, as Python's exception comes out of fn(...).
   - the plan is a KVal dict (kinds.h), changed in place where Python changes it (place_subject, lay_path ...).
   Call scene_config(), pl_config(), solids_init(), kinds_init() and cp_config() first. */
#ifndef COMPOSE_H
#define COMPOSE_H
#include "kinds.h"
#include "features.h"
#include "builders.h"

#define CP_EXIT (-2)

/* ---------------- detail functions ---------------- */
struct ScFn {
    int rc;
    const char *qualname;                 /* fn.__qualname__ (a log line names it; TAGS is looked up by its first part) */
    SmTag tag;                            /* fn.tmpl / .kind / .shape / .cls: NULL = no such attribute */
    int (*run)(ScFn *f, Scene *s, PyRandom *rng, int lvl);        /* fn(s, rng, lvl): 0, or SC_ERR = it raised */
    void *ctx; void (*ctx_free)(void *);
    int uid;                              /* the maker's own number (the proof's record number) */
};
ScFn *cp_fn_new(const char *qualname, int (*run)(ScFn *, Scene *, PyRandom *, int), void *ctx, void (*ctx_free)(void *));
ScFn *cp_fn_ref(ScFn *f);
void cp_fn_unref(ScFn *f);
void cp_fnl_push(ScFnList *l, ScFn *f);   /* adds a reference */
void cp_fnl_free(ScFnList *l);
/* fn(s, rng, lvl) guarded as a Python call is: 0, or SC_ERR when it raised (also through ly_jb, or a sampler through sm_hook_err) */
int cp_call_fn(ScFn *f, Scene *s, PyRandom *rng, int lvl);

/* the builder parts register these with bld_add_any (builders.h); compose fetches them with bld_any:
     ("scenes", "SUBJECTS", "CpSubjectFn")  SUBJECTS[name](pal): the subject builder build(cx, cy, a, rng, k) as an
                                            FtBuildFn called with has_scale = 1, scale = k. 0 = made, SC_ERR = raised.
     ("scenes", "prop_fn",  "CpPropFn")     prop_fn(name, pal): *out a NEW reference, NULL = None (prop_fn catches).
   CPH holds what compose uses; cp_config() fills it from the table (a proof may point it elsewhere). */
typedef int (*CpSubjectFn)(const char *name, const ScPal *pal, FtBuildFn *fn, void **ctx);
typedef int (*CpPropFn)(const char *name, const ScPal *pal, ScFn **out);
typedef struct {
    CpSubjectFn subject;
    CpPropFn prop;
    /* kitlog.caught(where, e): the scene log's line for a caught exception (where: what Python passes). NULL = the
       C's own line "<where>: error". */
    const char *(*caught)(void *u, const char *where);
    void *u;
} CpHooks;
extern CpHooks CPH;

/* ---------------- what compose reads from settings.json and the environment ---------------- */
typedef struct {
    /* CFG (kit_settings.CFG): NULL text = the key is not there (Python's default is used) */
    const char *objects_amount;           /* _share(): float(CFG.get("objects_amount", 1.0)), clamped; a bad value -> 1.0 */
    double max_slenderness;               /* float(CFG.get("max_slenderness", 3.0)) */
    double big_base_height_factor;        /* float(CFG.get("big_base_height_factor", 1.5)) */
    int no_bare_ground;                   /* CFG.get("no_bare_ground", True) */
    int style_details;                    /* CFG.get("style_details", True) */
    double style_details_amount;          /* float(CFG.get("style_details_amount", 0.5)) */
    double clear_middle;                  /* float(CFG.get("clear_middle", 0.45)) */
    const char *miniature;                /* CFG.get("miniature", "biped") (NULL = not set) */
    /* os.environ (NULL = not set) */
    const char *kit_mini, *kit_final_mm, *theme, *plan_json, *kit_worker, *salt, *reseed, *kit_fault;          /* KIT_MINI, KIT_FINAL_MM, THEME, PLAN_JSON, KIT_WORKER, SALT, RESEED, KIT_FAULT */
    int designer_enabled;                 /* designer.enabled(): the local model is not part of the C kit (0) */
} CpCfg;
extern CpCfg CPC;
void cp_config_default(void);             /* settings.json's values, nothing in the environment */
void cp_config(void);                     /* fills CPH from the builder table (bld_any) */

/* ---------------- the plan ---------------- */
/* theme_order(style, salt): the shuffled indices of THEMES[style] (*n of them, out holds 64) */
int cp_theme_order(const char *style, const char *salt, int64_t *out, int *n);
/* expand(th, mixr, mm, style): the plan dict (NEW) */
KVal *cp_expand(const KVal *th, PyRandom *mixr, double mm, const char *style);
/* pick_plans(style, mixr, mm, n): appended to plans (a KVal list). CP_EXIT: a forced THEME not known */
int cp_pick_plans(const char *style, PyRandom *mixr, double mm, int n, KVal *plans);
/* validate_plan(plan, style, mm, used): *out the repaired plan (NEW, NULL = None), notes appended (KVal list of str) */
int cp_validate_plan(const KVal *plan, const char *style, double mm, const char *const *used, int nused, KVal **out, KVal *notes);
int cp_legal_paths(const char *style, const char **out, int cap);         /* legal_paths: PATH_OK[style].split() + ["none"] */
int cp_prop_fn(const char *name, const ScPal *pal, ScFn **out);           /* prop_fn: via CPH.prop; NULL = None */
int cp_prop_name(const char *name);       /* name in PROP_NAMES */

/* ---------------- the compose state beyond samplers.h's Sc, and what compose leaves on s ---------------- */
typedef struct {
    int has_stand;                        /* s.stand */
    const char *type; GeoList pads; Geo *body; double at[2]; int64_t facing;
} CpStand;
typedef struct {
    /* s.layout_facts, s.theme, s.plan (NEW KVal), s.plan_source, s.plan_notes (KVal list), s.scene_log (KVal list),
       s.plan_line: set at the end of compose (layout_facts at its start) */
    char *layout_facts, *theme, *plan_source, *plan_line;
    KVal *plan, *plan_notes, *scene_log;
    CpStand stand;
    FtS3dList s3d;                        /* s.solids3d (with --supports on) */
} CpOut;
void cp_out_free(CpOut *o);

/* the detail pools compose reads: details.DETAILS[style] and style_details.NEW20[style] (borrowed) */
typedef struct { const ScFnList *details, *new20; } CpPools;

/* scenes.compose(s, style, mixr, rng, t, fns): out gets what Python leaves on s. The caller (gg3.make) then sets
   FOCUS / AIM to None, HBOOST to 1.0 and calls cp_release(s) (its finally:), as make does. */
int cp_compose(Scene *s, const char *style, PyRandom *mixr, PyRandom *rng, int t, const ScFnList *fns, const CpPools *pools, CpOut *out);
void cp_release(Scene *s);                /* scenes.release(s) */

/* parts, for the proofs */
int cp_klass_of(const ScFn *f, const char **out);         /* klass(fn) */
int cp_stand_shapes(const char *kind, double cx, double cy, double a, GeoList *pads, Geo **body);
#endif
