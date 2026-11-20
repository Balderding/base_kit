/* features.h: group G7 of the C port: features.py (_k, _build, comp, lm, landmarks, true_scale, size_factor, the
   NEW / LANDMARKS tables and extend) and sd_reg.py (d(...).f, scaled, real_factor) in C (features.c).
   Copied routine by routine; proof: work\cscene\features\fcapture.py (real calls recorded while real bases are made,
   replayed in C, the scene dumps compared byte for byte; plus random cases).

   Python semantics kept:
   - a builder (features b_* / L_* and the sd_* `_` functions) is a callback FtBuildFn; it hands back the footprint
     (a NEW Geo reference, NULL = None) and the layers (SoLayers of solids.h), or says None / raised.
   - layer tuples are SoLayer (solids.h). Their items as Python has them; two items carry C payloads as LV_PTR:
       a "mound" layer's tiers (L[4]):        LV_PTR to a const FtTiers (borrowed: keep it alive, e.g. static)
       a "steps" layer's pieces (L[2]):       LV_LATER (a solids.h SoLater, from so_steps / so_fine / sd_ST), or
                                              LV_PTR to an FtSteps (a plain list of pieces, a _Later made here, or
                                              one whose pieces come from a callback).
   - _Later is lazy: the pieces are cut the first time anything looks at them, and sculpt numbers its chain from
     so_chain_counter at THAT moment; ft_build looks at the layers exactly in Python's order.
   - every routine returns a status; SC_ERR (-1) = Python would raise here (the caller's try/except decides).
     ft_build catches per layer as Python does (each caught error is counted in ft_caught).
   - the module globals read: gg3.H = SC.H, gg3.CAPZ = SC.CAP, gg3.HBOOST[0] = PL.hboost, gg3.DISC = PL.disc,
     gg3.BRIDGE (with style_obelis' "obfloor"), and the environment (ft_set_env).
   Call scene_config(), pl_config() and solids_init() first. */
#ifndef FEATURES_H
#define FEATURES_H
#include "place.h"
#include "solids.h"

/* ---------------- the environment the Python reads ---------------- */
/* os.environ KIT_SCALE, KIT_FINAL_MM, KIT_NO_REAL (NULL = not set). Copied. */
void ft_set_env(const char *kit_scale, const char *kit_final_mm, const char *kit_no_real);
/* Python's float(str): 0 = ok, SC_ERR = ValueError */
int ft_pyfloat(const char *s, double *out);
double ft_true_scale(void);                   /* features.true_scale() */
double ft_size_factor(double mm);             /* features.size_factor(mm) */

/* ---------------- small helpers ---------------- */
const char *ft_k(const char *kind);           /* features._k: kind + "_p" for a BRIDGE kind (interned; NULL = None stays NULL) */
int ft_in_bridge(const char *kind);           /* kind in gg3.BRIDGE */
extern int64_t ft_caught;                     /* kitlog.caught calls made by ft_build / ft_landmarks (a count) */

/* ---------------- layer payloads ---------------- */
typedef struct { int n; Tier t[8]; } FtTiers;
extern const FtTiers FT_ROCK_T, FT_SOFT_T, FT_SPIKE_T, FT_PEB_T, FT_ROUND_T;   /* features.ROCK_T ... ROUND_T */

/* FtSteps: the pieces of a steps layer that are not a solids.h SoLater. Reference counted; LV_PTR does not hold a
   reference, so every FtSteps is also kept in a list here until ft_steps_release() (call it when a base is done). */
typedef struct FtSteps FtSteps;
/* a callback maker: fills out (a NEW list) and returns SO_OK, or SO_ERR (it raised: the list stays empty for good) */
typedef int (*FtFillFn)(void *ctx, SoPieces *out);
FtSteps *ft_steps_list(const SoPieces *p);    /* a plain list of pieces (copied) */
FtSteps *ft_steps_later(FtFillFn fill, void *ctx, void (*ctx_free)(void *));   /* a _Later(make) */
FtSteps *ft_steps_ready(const SoPieces *p);   /* a _Later already filled (copied) */
void ft_steps_release(void);
/* the pieces a steps layer holds (looking at them: a _Later is filled now). NULL = it raised while being filled
   (the Python exception), *empty pieces after that. */
const SoPieces *ft_pieces(const SoVal *v);
int ft_is_later(const SoVal *v);              /* hasattr(L[2], "_make"): a _Later (filled or not) */

/* scenes.TOP3.get(c): the C sculpt's table (so_top3) unless set to something else (the proof replays) */
extern int (*ft_top3)(int64_t c, double *top3);

/* s.solids3d (--supports on): what ft_build appends to it */
typedef struct { int64_t c; double zb, zs; } FtS3d;
typedef struct { FtS3d *v; int n, cap; } FtS3dList;
void ft_s3d_free(FtS3dList *l);

/* ---------------- features._build(s, z, layers) ---------------- */
/* 1 / 0 = the done Python returns; SC_ERR = it raised (outside the per-layer try: a None layer, a short tuple) */
int ft_build(Scene *s, Num z, const SoLayers *layers, FtS3dList *s3d);

/* ---------------- builders, comp, lm ---------------- */
/* a builder: build(cx, cy, a, rng) / build(cx, cy, a, rng, scale) (has_scale) / L_x(cx, cy, a, rng, k).
   Returns 0 with *foot (NEW reference, NULL = None) and the layers appended to *layers; 1 = the builder gave None;
   SC_ERR = it raised. */
typedef int (*FtBuildFn)(void *ctx, double cx, double cy, double a, PyRandom *rng, int has_scale, double scale, Geo **foot, SoLayers *layers);
/* features.comp(build, r, gap, ground, tries).fn; tmpl "comp", kind = build.kind, shape = build.tag (style_details sets
   cls, kind, shape, sd) */
typedef struct { FtBuildFn build; void *ctx; double r, gap; int ground, tries; const char *kind, *shape, *cls; int sd; } FtComp;
FtComp ft_comp(FtBuildFn build, void *ctx, double r, double gap, int ground, int tries);    /* defaults 10.5, 0.4, 0, 60 */
/* fn(s, rng, lvl=0, scale=1.0): 1 = placed and built something (True), 0 = False, SC_ERR = raised */
int ft_comp_run(const FtComp *c, Scene *s, PyRandom *rng, double scale, FtS3dList *s3d);
/* features.lm(build, r_frac=0.55, gap=0.6, ground=False).fn(s, rng, k) */
typedef struct { FtBuildFn build; void *ctx; double r_frac, gap; int ground; } FtLm;
int ft_lm_run(const FtLm *l, Scene *s, PyRandom *rng, double k, FtS3dList *s3d);
/* features.landmarks(s, style, rng) over a pool (LANDMARKS[style]: (smallest mm, lm) pairs, in order); mm = gg.BASE_MM.
   Returns the count placed (it never raises: each landmark's error is caught) */
typedef struct { double lo; FtLm lm; } FtLmEntry;
int ft_landmarks(Scene *s, const FtLmEntry *pool, int n, PyRandom *rng, double mm, FtS3dList *s3d);

/* ---------------- the tables of features.py (_new, _landmarks, the style files' feature_tables) ---------------- */
/* a builder call as written in the tables: b_wheel("wood"), b_fairy_ring("flower", h=(0.45, 0.6), tiers=((1.0, 30),)) */
/* t: 's' str, 'f' float, 'i' int, 'b' bool, 'T' tuple of n numbers, 'R' tuple of n/2 pairs; bit j of imask: v[j] was an int */
typedef struct { const char *kw; char t; const char *s; int n; double v[8]; unsigned imask; } FtArg;
/* name without "b_" / "L_"; mod: NULL for features.py's own builders, else the style file that defines it ("style_blight") */
typedef struct { const char *mod, *name; int nargs; FtArg a[6]; } FtCall;
/* C(b_x(...), r=, gap=, ground=, tries=); shape / kind: what the detail function carries (fn.shape, fn.kind) */
typedef struct { const char *style; FtCall b; double r, gap; int ground, tries; const char *shape, *kind; } FtNewSpec;
typedef struct { const char *style; double lo; FtCall b; double r_frac, gap; int ground; } FtLmSpec;   /* (lo, lm(L_x(...), r_frac, gap, ground)) */
extern const FtNewSpec FT_NEW[]; extern const int FT_NNEW;          /* in NEW's order: style by style, as dict order */
extern const FtLmSpec FT_LMS[]; extern const int FT_NLMS;
/* _tag_builders: the .tag and .kind of one of features.py's own b_* builders (comp copies them to fn.shape / fn.kind) */
void ft_tag_kind(const FtCall *b, char *tag, int ntag, const char **kind);
/* extend(details): add each style's NEW list to DETAILS[style] when DETAILS has the style (once: a second call does
   nothing). has(ctx, style) = style in DETAILS; add(ctx, style, spec) appends one entry. */
int ft_extend(void *ctx, int (*has)(void *, const char *), void (*add)(void *, const char *, const FtNewSpec *));

/* ---------------- sd_reg.py ---------------- */
/* the scale tables (scale\real.json, drawn.json) as text: one line per detail "style\tname\treal\tlong\n" (repr floats,
   0 for a missing value: what float(... or 0) gives). NULL = the copy built into features.c (written by
   work\csceneeatures\gen_tables.py from scale\*.json); real_factor loads that one by itself the first time it needs
   tables, as Python reads the files the first time. Returns the line count, SC_ERR. */
int sdr_set_tables(const char *text);
void sdr_cache_clear(void);                   /* _REAL.clear() (the "tables" entry stays: the tables never change) */
void sdr_cache_put(const char *style, const char *name, double k);          /* _REAL[(style, name)] = k */
int sdr_cache_get(const char *style, const char *name, double *k);          /* 1 = cached */
/* SCALE (KIT_SCALE_FILE: {"style|name": factor}); a "*" key in that file is a 1-tuple in Python and never matches */
void sdr_scale_clear(void);
void sdr_scale_put(const char *style, const char *name, double k);
void sdr_scale_star(int has, double k);       /* SCALE["*"] set by code (the file can not) */
double sdr_real_factor(const char *style, const char *name, const char *cls);           /* real_factor (cached) */
/* scaled(out, cx, cy, k): the footprint (NEW reference) and the layers made k times larger (out appended) */
int sdr_scaled(Geo *foot, const SoLayers *layers, double cx, double cy, double k, Geo **foot_out, SoLayers *out);
/* the register: d(style, cls, kind, name, r=10.5, ground=False)(f0) */
typedef struct { FtBuildFn f0; void *ctx; const char *style, *cls, *kind, *name; double r; int ground; } SdrEntry;
/* d(...).w: REG[style] gets the entry, a later one of the same name taking the earlier one's place */
void sdr_register(const SdrEntry *e);
int sdr_reg_count(const char *style);
const SdrEntry *sdr_reg_get(const char *style, int i);
void sdr_reg_clear(void);
/* d(...).f(cx, cy, a, rng): an FtBuildFn with ctx = the SdrEntry (has_scale must be 0: f takes four arguments) */
int sdr_f(void *entry, double cx, double cy, double a, PyRandom *rng, int has_scale, double scale, Geo **foot, SoLayers *layers);
#endif
