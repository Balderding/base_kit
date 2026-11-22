/* place.h: the placement helpers of gg3.py in C (survey group G4): terr, _Hits (hit, near), hard (with _soft_hits,
   _soft_cut), evict, surf, find, notch, up, mound, cut, topcut, drape, cluster (rocks through cport\scatter.c, as
   cstone.cluster does), place_group, crack_plate. Each C function names the Python routine it copies.
   Proof: work\cscene\place (pcapture.py records real calls and random cases, placetest.dll replays them).

   Python semantics kept:
   - make / at / shapes_at / make_one are Python closures: here a function pointer plus a context. A callback that
     "raises" returns SC_ERR (or sets *err); the routine then returns SC_ERR at the same point Python would.
   - evict / notch REPLACE s->ops, s->feet, s->objs, s->tcuts, s->gcuts by new lists (the old arrays are freed), as
     Python rebinds s.ops = ops ...: a caller that iterates one of them across such a call must iterate a copy (Python's
     `for op in s.ops:` keeps the old list alive). notch also puts NEW op objects into s.ops in place (s.ops[i] = ...).
   - Geo results are NEW references owned by the caller; NULL = None.
   - the module globals gg3.HBOOST[0], gg3.DISC, os.environ NOTCH and cstone.enabled() are PL (pl_config).
   Call scene_config() first, then pl_config(). */
#ifndef PLACE_H
#define PLACE_H
#include "scene.h"
#include "leaf.h"
#include "shp.h"

typedef struct {
    double hboost;                        /* gg3.HBOOST[0] (1.0, or the big-base height factor while compose runs) */
    int notch;                            /* os.environ.get("NOTCH") is set (else notch() does nothing) */
    int crocks;                           /* cstone.enabled(): rock clusters made by scatter.c */
    Geo *disc;                            /* gg3.DISC = Point(0, 0).buffer(R0 - 0.7, 128) */
    double csx[40], csy[40];              /* gg._CSX / _CSY (for the rock clusters) */
    double cover, step, cover_big;          /* cover_big: the same for the centrepiece and second features */                   /* a detail on a floor plate must be on it for at least `cover` of its area (else it straddles: not placed there); on terrain at most `step` mm of step under it */
} PlCfg;
extern PlCfg PL;
/* call after scene_config (DISC follows SC.R0); csx / csy = gg._CSX / _CSY, handed over from Python. 1 = ready */
int pl_config(double hboost, int notch, int crocks, const double *csx, const double *csy);

typedef struct { int n; const char *const *v; } PlAllow;          /* allow=("wash", ...): a tuple of str; NULL = () */

int pl_terr(const Scene *s);
int pl_is_wet(const Kind *k);
int pl_is_terrk(const Kind *k);
extern int (*pl_terrk_fn)(const Kind *k);                                       /* gg3.terr: hasattr(s, "terr") */

/* gg3._Hits(polys): a search tree over polys (none when polys is empty) */
typedef struct { GeoList polys; const GEOSGeometry **gs; ShpTree *tree; } PlHits;
int pl_hits_init(PlHits *h, const GeoList *polys);                 /* SC_ERR if the tree cannot be made */
void pl_hits_free(PlHits *h);
int pl_hits_hit(PlHits *h, const GEOSGeometry *g);                 /* _Hits.hit(g): 1 / 0, SC_ERR */
int pl_hits_near(PlHits *h, const GEOSGeometry *pt, double d);     /* _Hits.near(pt, d) */
int pl_soft_hits(const Scene *s, PlHits *h);                       /* gg3._soft_hits(s) */
int pl_soft_cut(const Kind *k, const Geo *p);                      /* gg3._soft_cut(kind, p) */

int pl_hard(Scene *s, const PlAllow *allow, GeoList *out);          /* gg3.hard(s, allow) -> out */
int pl_evict(Scene *s, Geo *g);                                    /* gg3.evict(s, g) */
int pl_surf(Scene *s, Geo *g, Num *z);                             /* gg3.surf(s, g): *z (a gg3.Z, or T_zr's hi), NUM_NONE = None */

/* find(s, make, tries=120, gap=0.4, region=DISC, ground=False, allow=(), at=None).
   make(ctx, has_c, cx, cy, err): has_c = 0 for make(), 1 for make(cx, cy); returns a NEW geometry or NULL (None);
   *err = 1 when it raised. at(ctx, &cx, &cy): 0, or SC_ERR when it raised; at NULL = None. region NULL = DISC.
   Returns 1 found (*g new reference, *z), 0 = (None, None), SC_ERR. */
typedef Geo *(*PlMakeFn)(void *ctx, int has_c, double cx, double cy, int *err);
typedef int (*PlAtFn)(void *ctx, double *cx, double *cy);
int pl_find(Scene *s, PlMakeFn make, void *mctx, int tries, double gap, Geo *region, int ground, const PlAllow *allow,
            PlAtFn at, void *actx, Geo **g, Num *z);

/* speed plan (2026-11-22): while pl_find's make() runs, pl_pre offers find's own test of one try's footprint, so a
   builder can skip work that only matters when the try is kept (a model's outline). fn: 1 kept, 0 rejected, SC_ERR.
   armed: set by pl_find right around make(cx, cy), taken (cleared) by the first one to use it. BBB_NO_PRECHECK=1: off. */
typedef struct { int (*fn)(void *ctx, const GEOSGeometry *g); void *ctx; int armed; } PlPre;
extern _Thread_local PlPre pl_pre;

int pl_notch(Scene *s, const GeoList *polys, double gap);          /* gg3.notch(s, polys, gap=0.3) (only with PL.notch) */
/* gg3.up(s, polys, z, h, kind) -> *top (NUM_NONE = None). polys may hold NULL (None) entries. */
int pl_up(Scene *s, const GeoList *polys, Num z, Num h, Kind *kind, Num *top);
int pl_mound(Scene *s, const GeoList *polys, Num z, Num h, const Tier *tiers, int nt, Kind *kind);   /* gg3.mound */
int pl_cut(Scene *s, const GeoList *polys, Num z, Num depth, Kind *kind);                            /* gg3.cut */
int pl_topcut(Scene *s, const GeoList *polys, Num ztop, Num depth, Kind *kind);                      /* gg3.topcut (kind "seam") */
int pl_drape(Scene *s, Geo *g, Num h, Kind *kind);                                                   /* gg3.drape */

/* gg3.cluster(s, rng, n, make_one, cx, cy, spread, gap=0.25) -> out. make_one is either a details shape
   (details._one(shape, srng): has_shape, sh, srng; sh_rock / sh_round carry a cspec: those are made by scatter.c
   with rng, as cstone.cluster does, when PL.crocks and n <= 64) or a callback fn(ctx, x, y, err). */
typedef struct {
    int has_shape; LfShape sh; PyRandom *srng;
    Geo *(*fn)(void *ctx, double x, double y, int *err); void *ctx;
} PlOne;
int pl_cluster(Scene *s, PyRandom *rng, int n, const PlOne *one, double cx, double cy, double spread, double gap, GeoList *out);

/* gg3.place_group(s, rng, shapes_at, tries=120, gap=0.4, ground=False, region=DISC): shapes_at(ctx, cx, cy, out)
   appends the shapes (none = None / []), returns 0 or SC_ERR. Returns 1 (ps_out, *z), 0 = (None, None), SC_ERR. */
typedef int (*PlShapesAtFn)(void *ctx, double cx, double cy, GeoList *out);
int pl_place_group(Scene *s, PyRandom *rng, PlShapesAtFn shapes_at, void *ctx, int tries, double gap, int ground,
                   Geo *region, GeoList *ps_out, Num *z);

/* gg3.crack_plate(s, rng, reg, z, h, kind, npts=420, w=0.2) */
int pl_crack_plate(Scene *s, PyRandom *rng, Geo *reg, Num z, Num h, Kind *kind, int npts, double w);
#endif
