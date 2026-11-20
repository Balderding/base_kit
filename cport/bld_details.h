/* bld_details.h: the detail functions of details.py and styles2.py in C (bld_details.c, bld_styles2.c; agent
   "bld-features", group G10).
   details.py   the templates scatter, single, sunk, trail, path (the sh_* shapes and pat_* patterns are leaf.h's LfShape /
                LfPat), _raise, the set pieces a_outcrop, a_heap, a_block, a_ring, a_pit, apply(), anchors(), and the
                DETAILS / ANCHORS tables as data
   styles2.py   the signature features f_post_row, f_column, f_vent (factories) and f_headstone, f_lantern, f_stump,
                f_teeth, f_sandbags, f_coffin, f_trapdoor, f_lily (plain functions), and TIERS2 / DETAILS2 as data (the
                _nature / _gear pools and pad() already applied: the tables hold what they made)

   A detail function of these files is a DtFn: the factory's arguments as data, run by dt_run(fn, s, rng, arg):
       templates and f_*:   fn(s, rng, lvl)  -> None           (arg = lvl, an int as a double)
       a_* set pieces:      fn(s, rng, g_)   -> True / False   (arg = g_)
   dt_run returns DT_NONE (None), 1 (True) or 0 (False), or SC_ERR (Python raised: the caller's try / except decides; the
   scene and rng are then as Python leaves them at that point).
   Identity is the pointer: the tables share one DtFn wherever the Python shares one function object (styles2's log,
   reeds, roots, f_sandbags ... sit in TIERS2 and DETAILS2, pad() repeats its fill objects), so sample / set / `in`
   work on the pointers as Python's do on the objects.
   Module globals read: gg.H / SINK / K / MINW (SC), gg3.CAPZ (SC.CAP), gg3.FOCUS / AIM (leaf.h hooks), gg3.DISC,
   cstone.enabled() (PL), os.environ KIT_SCALE (anchors: features.h ft_true_scale).
   Registered in the builder table (builders.h, bld_add_any):
       ("details", "dt_run", "dt_run", dt_run)                    ("details", "DETAILS" / "ANCHORS", "dt_list", dt_details / dt_anchors)
       ("styles2", "DETAILS2", "dt_list", dt_details2)            ("styles2", "TIERS2", "dt_tiers", dt_tiers2)
       ("details", "scatter" / "single" / "sunk" / "trail" / "path", "dt_new", dt_scatter ...); the five a_* names all give dt_a
       ("details", "anchors", "dt_anchors_run", ...)  ("details", "apply", "dt_apply", ...)  ("details", "_raise", "dt_raise", ...)
       ("styles2", "f_post_row" / "f_column" / "f_vent", "dt_new", ...)   ("styles2", "f_headstone" ..., "tier", int fn(Scene *, PyRandom *, int lvl))
   Proof: work\cscene\bld_features (bfprove.py: real calls inside real bases, replayed at once, + random cases). */
#ifndef BLD_DETAILS_H
#define BLD_DETAILS_H
#include "builders.h"
#include "place.h"
#include "leaf.h"

enum { DT_SCATTER, DT_SINGLE, DT_SUNK, DT_TRAIL, DT_PATH,                       /* details.py templates */
       DT_OUTCROP, DT_HEAP, DT_BLOCK, DT_RING, DT_PIT,                          /* details.py a_* */
       DT_POST_ROW, DT_COLUMN, DT_VENT,                                         /* styles2.py factories */
       DT_HEADSTONE, DT_LANTERN, DT_STUMP, DT_TEETH, DT_SANDBAGS, DT_COFFIN, DT_TRAPDOOR, DT_LILY,   /* styles2.py functions */
       DT_COUNT };
#define DT_NONE 2

/* the arguments, by type (Python's defaults written where the tables leave them out):
   DT_SCATTER  scatter(shape, n=(3, 6), h=(0.5, 0.9), kind="rock", how="mound", spread=1.6, tiers=((0.7, 6), (0.3, 40)))
               sh, n0 n1, h0 h1, k1, how, x[0] = spread, tiers
   DT_SINGLE   single(shape, h=(0.8, 1.4), kind="crate", how="up", top=None, tiers=((0.75, 5), (0.25, 40)))   sh, h0 h1, k1, how, pat, tiers
   DT_SUNK     sunk(shape, depth=(0.3, 0.6), kind="pit", n=(1, 1), spread=0.0)       sh, h0 h1 = depth, k1, n0 n1, x[0] = spread
   DT_TRAIL    trail(step=1.3, n=(5, 8), shape=sh_ell(0.35, 0.45, 0.6, 0.75), depth=0.35, kind="pit")   x[0] = step, n0 n1, sh, num = depth, k1
   DT_PATH     path(w=0.7, l=(6, 10), h=0.4, kind="cable", how="up", amp=0.6)       x[0] = w, x[1] x[2] = l, num = h, k1, how, x[3] = amp
   DT_OUTCROP  a_outcrop(kind, tiers=2)                                             k1
   DT_HEAP     a_heap(kind, tiers=((0.45, 18), (0.35, 35), (0.2, 52)), h=(1.6, 2.4))    k1, tiers, h0 h1
   DT_BLOCK    a_block(kind, top_kind=None, pattern=None, h=(1.4, 2.2))             k1, k2, pat, h0 h1
   DT_RING     a_ring(kind, n=(6, 9), h=(1.2, 2.0))                                 k1, n0 n1, h0 h1
   DT_PIT      a_pit(kind="pit", depth=(0.6, 0.85))                                 k1, h0 h1 = depth
   DT_POST_ROW f_post_row(kind, h=(1.8, 2.4), rail=True, n=(2, 3), gap=4.2)          k1, h0 h1, flag = rail, n0 n1, x[0] = gap
   DT_COLUMN   f_column(kind="tcolumn", broken=True)                                k1 (broken is not used)
   DT_VENT     f_vent(kind_cut, kind_rim)                                           k1 = kind_cut, k2 = kind_rim
   the plain functions take nothing. */
typedef struct DtFn {
    int t;
    const char *qual;                 /* the Python __qualname__ before the first ".": "scatter", "a_block", "f_post_row", "f_lily" (scenes._tag_all's key) */
    const char *tmpl, *kind, *shape;  /* fn.tmpl / fn.kind / fn.shape as the Python file sets them (NULL = no such attribute: the a_* and f_*) */
    const char *k1, *k2, *how;        /* materials (NULL = None) and how ("up" / "mound" / "cut") */
    int has_shape; LfShape sh;
    int has_pat; LfPat pat;
    double n0, n1, h0, h1;
    double x[4];
    Num num;                          /* trail depth / path h (a Python number with its type) */
    int nt; Tier tiers[4];
    int flag;
} DtFn;
int dt_run(const DtFn *f, Scene *s, PyRandom *rng, double arg);

/* the constructors (a new DtFn, owned by the builder table: freed by bld_free_all); sh / pat NULL = None */
DtFn *dt_scatter(const LfShape *sh, double n0, double n1, double h0, double h1, const char *kind, const char *how, double spread, const Tier *tiers, int nt);
DtFn *dt_single(const LfShape *sh, double h0, double h1, const char *kind, const char *how, const LfPat *top, const Tier *tiers, int nt);
DtFn *dt_sunk(const LfShape *sh, double d0, double d1, const char *kind, double n0, double n1, double spread);
DtFn *dt_trail(double step, double n0, double n1, const LfShape *sh, Num depth, const char *kind);      /* sh NULL: the default sh_ell */
DtFn *dt_path(double w, double l0, double l1, Num h, const char *kind, const char *how, double amp);
DtFn *dt_a(int t, const char *kind, const char *k2, const LfPat *pat, double n0, double n1, double h0, double h1, const Tier *tiers, int nt);   /* DT_OUTCROP .. DT_PIT */
DtFn *dt_post_row(const char *kind, double h0, double h1, int rail, double n0, double n1, double gap);
DtFn *dt_column(const char *kind);
DtFn *dt_vent(const char *kind_cut, const char *kind_rim);
const DtFn *dt_plain(int t);          /* the one DtFn of f_headstone ... f_lily (DT_HEADSTONE .. DT_LILY) */

/* ---------------- the tables ---------------- */
typedef struct { const char *style; int n; const DtFn *const *v; } DtList;
typedef struct { const char *style; DtList tier[3]; } DtTiers;
extern const DtList DT_DETAILS[]; extern const int DT_NDETAILS;          /* details.DETAILS as details.py defines it (infestation cut to 30) */
extern const DtList DT_ANCHORS[]; extern const int DT_NANCHORS;          /* details.ANCHORS */
extern const DtList DT_DETAILS2[]; extern const int DT_NDETAILS2;        /* styles2.DETAILS2 */
extern const DtTiers DT_TIERS2[]; extern const int DT_NTIERS2;           /* styles2.TIERS2 */
const DtList *dt_details(const char *style);     /* NULL = not there */
const DtList *dt_anchors(const char *style);
const DtList *dt_details2(const char *style);
const DtTiers *dt_tiers2(const char *style);

/* details.anchors(s, style, rng) over a pool (ANCHORS[style]); the count placed (the Python returns None). Each set piece's error is caught. */
int dt_anchors_run(Scene *s, const DtList *pool, PyRandom *rng);
/* details.apply(s, style, rng, t) over any pool of n detail functions (DETAILS[style] as the kit has it then: its own
   entries, features' and style_details' additions): run(ctx, i, s, rng, t) calls pool[i] (an error it reports is caught,
   as Python's try / except does; the value is not looked at). rng.sample / choice work on positions, so a pool that
   holds one function twice behaves as Python's does. Returns 0. */
int dt_apply(Scene *s, int n, int (*run)(void *ctx, int i, Scene *s, PyRandom *rng, int t), void *ctx, PyRandom *rng, int t);

void bld_register_details(void);
void bld_register_styles2(void);
/* bld_styles2.c's part of dt_run (DT_POST_ROW .. DT_LILY) */
int dt_styles2_run(const DtFn *f, Scene *s, PyRandom *rng, double arg);
/* the helpers both files use */
int dt_raise(Scene *s, const GeoList *ps, Num z, double h, const char *kind, const char *how, const Tier *tiers, int nt, Num *top);   /* details._raise */
#endif
