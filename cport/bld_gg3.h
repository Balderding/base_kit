/* bld_gg3.h: group G10 of the C port, part "bld-scenes": the tier functions of gg3.py (bld_gg3.c) and the small
   toolkit bld_gg3.c and bld_scenes.c share. Proof: work\cscene\bld_scenes (bcapture.py records real calls while
   gg3.make builds real bases; bldtest.dll replays them; scene dumps compared byte for byte, rng states included).

   A tier function is fn(s, rng, lvl) (gg3.TIERS[style][tier]): here a BsTierFn. It returns 0, or SC_ERR where the
   Python raises (the caller's try / except: scenes._run catches and logs it; the scene is then as Python leaves it).
   Every tier function is registered in the builder table (builders.h) by bld_register_gg3():
       bld_add_any("gg3", "<python name>", "tier", (void *)fn)        e.g. ("gg3", "b_wash"), ("gg3", "n_ladder")
   and listed in BS_TIERS below in TIERS' order (style, tier 1..3, then the order inside the tier).
   gg3.FOCUS / AIM (rp / aim) are leaf.h's lf_focus / lf_aimfn: set them as scenes.focus does (samplers.h sm_focus_enter).
   Call scene_config(), pl_config(), sm_config_default() (the kind sets) and px_init() first. */
#ifndef BLD_GG3_H
#define BLD_GG3_H
#include <setjmp.h>
#include "builders.h"
#include "layouts.h"
#include "samplers.h"

typedef int (*BsTierFn)(Scene *s, PyRandom *rng, int lvl);

typedef struct { const char *style; int tier; const char *name; BsTierFn fn; } BsTier;      /* tier 1..3 = TIERS[style][tier - 1] */
extern const BsTier BS_TIERS[];
extern const int BS_NTIERS;
BsTierFn bs_tier_fn(const char *name);          /* by Python name ("h_bog"); NULL = not one of gg3's */
int bs_is_late(const char *name);               /* name in gg3.LATE  (b_washbed, b_scree, b_spiral) */
int bs_is_sparse(const char *name);             /* name in gg3.SPARSE (c_cable, c_lights, c_chevron) */
void bld_register_gg3(void);

/* gg2.crater_t(s, cx, cy, r) (n_shaft needs it; a copy of the static one in layouts_gg2.c, proven with n_shaft) */
void bs_crater_t(Scene *s, double cx, double cy, double r);

/* ================================================================ internal: shared by bld_gg3.c and bld_scenes.c
   The code runs inside a raise frame (BS_FRAME): every layouts.h helper (g_*, S_*, LC) raises by longjmp to it, as a
   Python exception leaves the function. Temporaries live in the layouts arena and are dropped when the frame ends. */
typedef struct { jmp_buf jb; jmp_buf *old; int mark; } BsFrame;
/* use:  BsFrame F; bs_begin(&F); if (setjmp(F.jb)) { bs_end(&F); return SC_ERR; } ... bs_end(&F); return 0; */
static inline void bs_begin(BsFrame *f) { f->old = ly_jb; ly_jb = &f->jb; ly_arena_begin(); f->mark = ly_mark(); }
static inline void bs_end(BsFrame *f) { ly_pop(f->mark); ly_arena_end(); ly_jb = f->old; }

/* find / place_group / cluster with bodies that may raise (a raise inside becomes the callback's error) */
typedef Geo *(*BsMkBody)(void *ctx);                                     /* make(): an arena geometry or NULL (None) */
typedef void (*BsSaBody)(void *ctx, double cx, double cy, GeoList *out);  /* shapes_at(cx, cy) */
typedef Geo *(*BsOneBody)(void *ctx, double x, double y);                 /* make_one(x, y) */
/* find(s, make, tries, gap, region (NULL = DISC), ground, allow): the shape (arena) or NULL; *z */
Geo *bs_find(Scene *s, BsMkBody mk, void *ctx, int tries, double gap, Geo *region, int ground, const char *allow1, Num *z);
/* place_group(s, rng, shapes_at, tries, gap, ground, region): the list (arena) or NULL; *z */
GeoList *bs_place_group(Scene *s, PyRandom *rng, BsSaBody sa, void *ctx, int tries, double gap, int ground, Geo *region, Num *z);
/* cluster(s, rng, n, make_one, cx, cy, spread, gap) appended to out */
void bs_cluster(Scene *s, PyRandom *rng, int n, BsOneBody one, void *ctx, double cx, double cy, double spread, double gap, GeoList *out);
/* the scene writers, raising */
Num bs_up(Scene *s, const GeoList *polys, Num z, Num h, const char *kind);        /* gg3.up: NUM_NONE = None */
void bs_mound(Scene *s, const GeoList *polys, Num z, Num h, const Tier *t, int nt, const char *kind);
void bs_cut(Scene *s, const GeoList *polys, Num z, Num depth, const char *kind);
void bs_topcut(Scene *s, const GeoList *polys, Num ztop, Num depth, const char *kind);
void bs_drape(Scene *s, Geo *g, Num h, const char *kind);
GeoList *bs_hard(Scene *s);                                                       /* gg3.hard(s) */
void bs_evict(Scene *s, Geo *g);
Num bs_surf(Scene *s, Geo *g);
static inline int bs_none(Num n) { return n.t == NUM_NONE; }
static inline int bs_truthy(Num n) { return n.t != NUM_NONE && n.v != 0.0; }        /* `if top:` */
static inline Tier TT(double frac, double a) { Tier t = { num_f(frac), num_i(a) }; return t; }   /* (0.7, 6): a float and an int */
/* rp / aim / frame */
static inline void bs_rp(PyRandom *rng, double dmin, double dmax, double *x, double *y) { lf_rp(rng, dmin, dmax, x, y); }
double bs_pow(double x, double y);              /* float ** float; raises where Python does (a complex result raises too: none of these builders meets one) */
double bs_rpow(double x, double y);             /* scenes._real(x ** y) */
int64_t bs_int(double x);                       /* int(x) */
int64_t bs_randint(PyRandom *rng, int64_t a, int64_t b);
double bs_choice_pm1(PyRandom *rng);            /* rng.choice((-1, 1)) */
#endif
