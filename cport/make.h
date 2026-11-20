/* make.h: gg3.make(style, n), the top of making one base (make.c): the n > 20 recursion, the layout (layouts.h
   ly_ground: ORIG[style], mixr, FMIX, SCENE_SKIP), T_init or the flat style's objs / keepout, base_ops, the tier
   rng, the tier pick (RND: mixr.sample), LATE last, the installs (features / details / scenes / style_details:
   their colours and scenes._tag_all), then scenes.compose (cp_compose) and scenes.release in its finally - or, with
   scenes off, landmarks / set pieces / the tier loop / details.apply as make does them.
   The base comes back as Python leaves it: the Scene and what compose put on it (CpOut).
   Proof: work\cscene\compose (crec.py records gg3.make runs; the replay starts from the recorded layout).
   Call scene_config(), pl_config(), solids_init(), ly_config(), kinds_init(), cp_config_default() / cp_config(),
   mk_config() first. */
#ifndef MAKE_H
#define MAKE_H
#include "compose.h"
#include "layouts.h"

/* the style's detail functions, from the builder parts: bld_add_any("gg3", "style_fns", "MkStyleFn", fn) */
typedef struct {
    ScFnList tiers[3];                    /* gg3.TIERS[style][0..2] (tier 2, 3, 4) */
    ScFnList late, sparse;                /* the functions of gg3.LATE / gg3.SPARSE (membership by identity) */
    ScFnList details;                     /* details.DETAILS[style] (with features.NEW and style_details' twenty in it) */
    ScFnList new20;                       /* style_details.NEW20[style] */
    FtLmEntry *landmarks; int nlandmarks; /* features.LANDMARKS[style] (scenes off) */
} MkStyleFns;
typedef int (*MkStyleFn)(const char *style, MkStyleFns *out);          /* fills out (references); 0, SC_ERR */
void mk_style_fns_free(MkStyleFns *f);
/* scenes off only: details.anchors(s, style, rng) and details.apply(s, style, rng, t) ("details", "anchors" /
   "apply", "MkDetailsFn"): 0 / SC_ERR */
typedef int (*MkDetailsFn)(Scene *s, const char *style, PyRandom *rng, int t);

typedef struct {
    const char *salt, *reseed;            /* os.environ SALT / RESEED (make adds "x<n>" to SALT for n > 20, and leaves it so) */
    int random_mix, scenes, landmarks, set_pieces, extra_details;      /* CFG (defaults True, False, True, True, True) */
    MkStyleFn style_fns;                  /* NULL: bld_any("gg3", "style_fns", "MkStyleFn") */
    /* the layout: ly_ground unless set (a proof replays a recorded one). Must give the scene and mixr as
       gg3.make has them right after the rename that follows ORIG[style](nb). */
    Scene *(*layout)(const char *style, int n, const char *salt, const char *reseed, PyRandom *mixr, int *err);
    /* scenes off: features.landmarks(s, style, mixr) (never raises: each landmark's error is caught). NULL: ft_landmarks
       over the style's LANDMARKS (MkStyleFns.landmarks). */
    int (*landmarks_fn)(Scene *s, const char *style, PyRandom *rng, const MkStyleFns *F, FtS3dList *s3d);
} MkCfg;
extern MkCfg MKC;
void mk_config(void);                     /* the defaults above */

/* gg3.make(style, n): the base (NULL with *err = SC_ERR / CP_EXIT when Python raises out of make; the partly made
   scene is then dropped, as Python loses it). out gets what compose leaves on s (empty with scenes off). */
Scene *mk_make(const char *style, int n, CpOut *out, int *err);
#endif
