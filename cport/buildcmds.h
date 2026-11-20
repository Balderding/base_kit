/* buildcmds.h: gg.build_cmds(s) in C (buildcmds.c): the finished scene -> the command groups mesher.field_from_cmds /
   field_ops (and the SolidCraft build) read, with every helper it reaches: _prune, final_gcuts (gg.Scene and
   gg2.Scene), _group_ops, fit_for_taper, _separate, _coincide, RIM_K, _clip, _rim, _wall_r, op_k, clip_z, op_clip,
   _prism_groups, piece_group, sk, loops, _arc_loop, magnet_centres, mag_sketch, sk_circle, _surface_z, and
   build_cmds_trim_last (CFG base_first = false). gg.py is not rewritten there by sizefix (all of these are in
   sizefix.SKIP), so this is the plain source. Proof: work\cscene\buildcmds (capture.py, prove.py, bctest.c).

   Python semantics kept:
   - numbers carry their Python type (BcNum.t), because the type decides round() (numpy.float64 rounds as numpy
     does) and shows in the commands (an int offset 0 stays an int). Codes as scene.h's Num: 0 float, 1 int,
     2 gg3.Z, 3 bool, 4 numpy.float64.
   - kinds are objects: a plain str equals any kind with the same text, a gg.LK equals another LK only when it is
     the same object (BcKind pointer), exactly as LK.__eq__.
   - ops are reference counted objects (BcOp): the same op may sit in s.ops twice or in a chop's ops, and the
     identity decides RIM_K (op_k) and _group_ops' ("keep", id(op)) key, as in Python.
   - build_cmds changes the scene as Python does (s.ops, s.gcuts, s.chops rebuilt by _prune, _group_ops,
     _separate; every op's rk = RIM_K slot afterwards, 0 for ops not in s.ops).
   Geometries: every GEOSGeometry * in the scene is owned by it. shp_init() must have been called. */
#ifndef BUILDCMDS_H
#define BUILDCMDS_H
#include "geos_dyn.h"

#define BC_ERR (-1)                       /* Python would raise here */

enum { BC_FLOAT = 0, BC_INT = 1, BC_Z = 2, BC_BOOL = 3, BC_NP = 4 };
typedef struct { double v; unsigned char t; } BcNum;

typedef struct BcKind {                   /* a material name: str, or gg.LK (lk = 1) */
    int idx;                              /* its index in BcScene.kinds (for dumps) */
    int lk; char *s;
    int has_loft; BcNum loft_chain, loft_z;          /* LK.loft = (chain, z), when set */
    int has_ghost;                        /* getattr(kind, "ghost", None) is not None */
} BcKind;

enum { BC_PRISM = 0, BC_TAPER = 1 };
/* ("prism", [P], z0, top, kind): n[0] = z0, n[1] = top;  ("taper", [P], z, h, angle, kind): n = z, h, angle */
typedef struct BcOp { int rc; int type; BcKind *kind; BcNum n[3]; GEOSGeometry **polys; int np; int rk; } BcOp;
typedef struct { GEOSGeometry **polys; int np; BcNum depth; BcKind *kind; } BcGcut;          /* ([P], depth, kind) */
typedef struct { GEOSGeometry **polys; int np; BcNum z0, z1; BcKind *kind; } BcTcut;         /* ([P], z0, z1, kind) */
typedef struct BcChop {                   /* {"kind", "poly", "z0", "top", "run", "drop", "ops"[, "iso"]} */
    BcKind *kind; GEOSGeometry *poly; BcNum z0, top, run, drop; int has_iso; BcNum iso; BcOp **ops; int nops;
} BcChop;

typedef struct {                          /* the gg globals and CFG keys build_cmds reads */
    double R0, H, RIM, base_mm;           /* gg.R0, gg.H, gg.RIM, gg.BASE_MM */
    double sketch_tol;                    /* gg.SKETCH_TOL as it stands (gg3.smooth_isolated sets it to 0) */
    double rim_gap;                       /* float(CFG.get("rim_gap", 0.02)) */
    int base_first, group_pieces, split_cuts, chamfer_rocks, chamfer_grates;   /* CFG, each default True */
    int noarc;                            /* os.environ NOARC set */
    int gg2;                              /* s is a gg2.Scene (its final_gcuts) */
    int has_cutgap; BcNum cutgap;         /* s.cutgap (gg2.Scene.final_gcuts reads getattr(s, "cutgap", 0.05)) */
    int true_circle_rim;                  /* CFG true_circle_rim (default True; only build_cmds_trim_last reads it) */
    /* gg.CHAMFER_PRISMS and gg.PIECES as they stand when build_cmds runs: scenes.kfix and ground_big ADD names to
       CHAMFER_PRISMS while the scene is made ("rock_s"...). NULL = the sets as gg.py defines them. Owned (freed by
       bc_scene_free). */
    char **chamfer_prisms; int nchamfer;
    char **pieces; int npieces;
} BcCfg;

typedef struct BcScene {
    BcCfg cfg;
    BcKind **kinds; int nkinds;           /* every kind object the scene uses (owned) */
    BcOp **ops; int nops;                 /* s.ops (one reference each) */
    BcGcut *gcuts; int ngcuts;
    BcTcut *tcuts; int ntcuts;
    GEOSGeometry **feet; int nfeet;
    BcChop **chops; int nchops;
} BcScene;

BcOp *bc_op_new(int type, BcKind *kind, const BcNum *n, GEOSGeometry **polys, int np);      /* takes the polys array over; rc = 1 */
void bc_op_unref(BcOp *o);
void bc_scene_free(BcScene *s);           /* frees everything in it (not s itself) */

/* ---- the result: command groups ---- */
/* a value of a command, as the Python dict / list holds it */
enum { BCV_NONE = 0, BCV_NUM, BCV_STR, BCV_LIST, BCV_DICT, BCV_PTS };
typedef struct BcVal BcVal;
struct BcVal {
    int t;
    BcNum num;                            /* BCV_NUM (a bool is a BcNum with t = BC_BOOL) */
    char *s;                              /* BCV_STR */
    int n, cap; BcVal *v; const char **k; /* BCV_LIST: n items v; BCV_DICT: n keys k with values v, in insertion order */
    double *pts; int dim;                 /* BCV_PTS: a list of n points, each a list of dim Python floats ([[x, y], ...]) */
};
const BcVal *bc_get(const BcVal *dict, const char *key);         /* dict[key] or NULL */

/* one group of build_cmds' list: a plain list of commands, or a gg.Group with its attributes. Every command is a
   dict {"command": str, "params": dict}. */
typedef struct BcGroup BcGroup;
struct BcGroup {
    int is_group;                         /* isinstance(g, gg.Group) */
    BcVal *cmds; int n, cap;
    BcVal piece;                          /* Group.piece: {"body", "top", "dist"} or BCV_NONE */
    int polish;                           /* Group.polish */
    BcGroup *fallback; int nfb, capfb;    /* Group.fallback (plain lists) */
    BcGroup *alts; int nalts, capalts;    /* Group.alts */
};
typedef struct { BcGroup *g; int n, cap; } BcGroups;
void bc_groups_free(BcGroups *gs);

/* gg.build_cmds(s): the groups go to out (which starts empty). 0 = done, BC_ERR = Python raises (out and s are then
   as far as Python got). */
int bc_build_cmds(BcScene *s, BcGroups *out);
/* s.final_gcuts() (gg.Scene / gg2.Scene) of the scene as it stands: a new list (owned; bc_gcuts_free). For the 2D drawing. */
int bc_final_gcuts(const BcScene *s, BcGcut **out, int *nout);
void bc_gcuts_free(BcGcut *g, int n);
/* how many times a _group_ops key matched more than one earlier key (only possible when a str and LK kinds with the
   same text are mixed: CPython's dict then picks by its hash table, the C takes the first). 0 in every proof. */
extern int bc_ambiguous_keys;
#endif
