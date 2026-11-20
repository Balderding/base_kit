/* kinds.h: ONE shared table of every kind set, colour table and per-style table the scene code reads or extends at
   run time (kinds.c), and the small JSON-like value type the plans and theme tables are made of (KVal).
   Python names:
     gg3.BRIDGE  gg3.SOFT  scenes.TERRK  scenes.WETK  gg.CHAMFER_PRISMS     -> KD.bridge soft terrk wetk chamfer
         (their storage is layouts.h's LY_BRIDGE / LY_SOFT / LY_TERRK / LY_WETK / LY_CHAMFER, so ground_big adds to
         the very same sets; SmStrSet and LyStrSet have one layout)
     scenes.DEBRIS GROWTH CUTK TALL ALSO_OK BUILT ENCLOSED CLIFFS, gg.PIECES -> KD.debris growth cutk tall also_ok built enclosed cliffs pieces
     gg.COL, gg.CUTC (ordered str -> str: pictures only)                    -> KD.col, KD.cutc
     scenes.PAL PATH_OK STYLE_SAYS _EXTRA, styles2.TITLES, gg2 ground colour -> KD.styles[i] (KStyle)
     scenes.SUBJECTS (the names, in dict order) / SUBJECT_SAYS               -> KD.subjects / KD.subject_says
     scenes.MINIS (pads, reach, says), PATHS / PATH_W / PATH_SAYS, PROP_NAMES, _SPECIAL, RELATIONS, EDGE_OK, ABSTRACT, AMOUNT
     themes.THEMES                                                            -> KD.styles[i].themes (a KVal list of theme dicts)
   The values are what the .py files define (the KD_JSON block in kinds.c, written by
   work\cscene\compose\gen_kinds.py); the run-time additions are made by the hooks below, at the moments Python makes
   them, so a long run in one process accumulates exactly as one Python process does:
     kinds_init()                 import time: gg, gg3 (newstyles.hook_gg3: style colours, style_blight.tables() ->
                                  _register (CUTC only: scenes is not loaded yet), style_obelis.tables() -> FLOORS into
                                  BRIDGE), scenes (newstyles.hook_scenes), themes (hook_themes)
     kinds_layout_hook(style)     the first line of style_blight.layout / style_obelis.layout (_register / _register(True))
     kinds_install_scenes()       scenes.install (its colour defaults); kinds_install_style_details() likewise
     sm_kfix (samplers.c) adds to KD.chamfer and KD.col through SM; scenes.compose adds op kinds to KD.col.
   Call kinds_init() once, after scene_config(). It also points samplers.h's SM at these sets (kinds_point_parts). */
#ifndef KINDS_H
#define KINDS_H
#include <stdint.h>
#include "samplers.h"
#include "layouts.h"
#include "buildcmds.h"

/* ---------------- KVal: None / bool / int / float / str / list / dict (insertion ordered) ---------------- */
enum { KV_NONE = 0, KV_BOOL, KV_INT, KV_FLOAT, KV_STR, KV_LIST, KV_DICT };
typedef struct KVal { int t; int64_t i; double f; char *s; int n, cap; struct KVal **v; char **k; } KVal;
KVal *kv_none(void);
KVal *kv_bool(int b);
KVal *kv_int(int64_t i);
KVal *kv_float(double f);
KVal *kv_str(const char *s);                  /* copied; NULL -> None */
KVal *kv_list(void);
KVal *kv_dict(void);
void kv_push(KVal *l, KVal *x);               /* append (takes x over) */
void kv_set(KVal *d, const char *key, KVal *x);        /* d[key] = x (takes x over; an existing key keeps its place) */
KVal *kv_get(const KVal *d, const char *key);          /* d.get(key) (borrowed; NULL = missing) */
KVal *kv_copy(const KVal *x);                 /* a deep copy (json.loads(json.dumps(x)) for the JSON values) */
void kv_free(KVal *x);
int kv_is_num(const KVal *x);                 /* int / float / bool (Python numbers) */
double kv_num(const KVal *x);
const char *kv_s(const KVal *d, const char *key);      /* d[key] as a str, NULL if missing / not a str */
/* json.loads(text) (strict JSON, as Python reads it; NaN / Infinity accepted). NULL = it raised (ValueError). */
KVal *kv_parse_json(const char *text);
/* the same; err gets str(e) of the JSONDecodeError Python raises ("Expecting value: line 1 column 1 (char 0)") */
KVal *kv_parse_json_e(const char *text, char *err, size_t nerr);

/* ---------------- ordered string maps (gg.COL, gg.CUTC) ---------------- */
typedef struct { char **k, **v; int n, cap; } KMap;
const char *kmap_get(const KMap *m, const char *k);
const char *kmap_setdefault(KMap *m, const char *k, const char *v);   /* dict.setdefault: the value now there */
void kmap_set(KMap *m, const char *k, const char *v);                  /* m[k] = v */

/* ---------------- one style ---------------- */
typedef struct {
    const char *name, *title, *says, *ground;          /* says = STYLE_SAYS; title / ground for the new styles (NULL for the older ones) */
    ScPal pal;                                          /* PAL[style] (_P(...)) */
    const char *path_ok;                                /* PATH_OK[style] */
    const char *extra;                                  /* _EXTRA[style] ("" when the style has none) */
    KVal *themes;                                       /* themes.THEMES[style]: a list of theme dicts (NULL: the style has none) */
    int is_new;                                         /* one of the style_<name>.py styles */
} KStyle;

/* ---------------- the table ---------------- */
typedef struct {
    SmStrSet *bridge, *soft, *terrk, *wetk, *chamfer;  /* storage: LY_BRIDGE, LY_SOFT, LY_TERRK, LY_WETK, LY_CHAMFER */
    SmStrSet debris, growth, cutk, tall, also_ok, built, enclosed, cliffs, pieces, abstract, special;
    KMap col, cutc;
    KStyle *styles; int nstyles;                        /* PAL's order: the 19 older styles, then the new ones in file-name order */
    const char **subjects; int nsubjects, capsub;      /* SUBJECTS keys in dict order */
    KMap subject_says;
    const char **prop_names; int nprop_names;
    const char **paths; int npaths; double path_w[16]; const char *path_says[17];   /* PATHS order; path_says has "none" last */
    const char **relations; int nrelations;
    const char **edge_ok; int nedge_ok;
    double amount_debris, amount_growth, amount_marks;  /* scenes.AMOUNT */
    /* MINIS: pads (forward, sideways, r) or (forward, sideways, half length, half width) */
    struct KMini { const char *name, *says; double reach; int npads; int padn[12]; double pad[12][4]; } minis[16]; int nminis;
    int blight_done;                                    /* style_blight._DONE[0] */
    int scenes_loaded;
} Kinds;
extern Kinds KD;

void kinds_init(void);                        /* once; again does nothing */
void kinds_layout_hook(const char *style);    /* style_blight / style_obelis _register at their layout start */
void kinds_install_features(void);            /* features.install: its colours and the "<floor>_p" names (in the order the
                                                 recorded Python process iterated BRIDGE) */
void kinds_install_details(void);             /* details.install */
void kinds_install_scenes(void);              /* scenes.install: gg.COL.setdefault(...) of its own colours */
void kinds_install_style_details(void);       /* style_details.install likewise */
void kinds_point_parts(void);                 /* SM.bridge .. SM.cutk and SM.col_setdefault point at this table (kinds_init does it) */
/* gg.CHAMFER_PRISMS / gg.PIECES as they stand now, for buildcmds.h (BcCfg owns the copies) */
void kinds_bc(BcCfg *c);

const KStyle *kd_style(const char *name);     /* NULL = not a style */
int kd_has(const SmStrSet *s, const char *k);           /* k in s (NULL k: False) */
int kd_kind_in(const SmStrSet *s, const Kind *k);       /* op[-1] in s: an LK by its text, as Python's set finds it */
int kd_subject(const char *name);             /* name in SUBJECTS */
const struct KMini *kd_mini(const char *name);
int kd_path(const char *kind);                /* index in PATHS, -1 */
const char *kd_pal_get(const ScPal *p, const char *key, int *found);   /* p[key]; *found 0 = KeyError */
int kd_pal_set(ScPal *p, const char *key, const char *v);              /* p[key] = v (new keys go to the extras) */
void kd_pal_copy(ScPal *dst, const ScPal *src);         /* dict(src): the extras arrays are copied */
void kd_pal_free(ScPal *p);                   /* the extras of a copy */
#endif
