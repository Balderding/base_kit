/* bld_scenes.h: group G10 of the C port, part "bld-scenes": the builders of scenes.py in C (bld_scenes.c).
   scenes.py   S_portal S_ruin S_shaft S_camp S_depot S_graves S_wagon S_corral S_machine S_emplacement S_tent S_log
               S_well S_statue S_organ and the SUBJECTS table with its wrappers _worn / _grow / _turn;
               b_crate b_crate_stack b_keg b_barrel b_barrels b_sacks b_sack b_log b_logpile b_urn b_urn_fallen
               b_drum_can b_tank b_chest b_stump b_wheel_up b_tent b_signpost b_shrooms b_table_rock b_skull b_ribs
               b_bone L_skeleton, _several, _either, _skull_parts, _ribcage, _knuckle, _barrel_side, _sack;
               the props: _orepile, _props (the PROPS table), prop_fn, PROP_NAMES.
   (_weather and _crate_layers are solids.h's so_weather / so_crate_layers; the gg3 tier functions are bld_gg3.h.)
   Proof: work\cscene\bld_scenes (bcapture.py: real calls inside real bases of every style, classic and figure scale,
   ovals, supports on, scenes on and off, plus random cases; replayed on bldtest.dll, byte for byte).

   ---- the palette: subjects and props take PAL[style] as samplers.h's ScPal (p["wood"] = pal->wood ...; p[x] for a
        name not among the fields is looked up in extra_k / extra_v). NULL = None.

   ---- what is registered (bld_register_scenes, builders.h):
     bld_add("scenes", "b_crate" ...)        every b_* above and "L_skeleton": BldFactory from an FtCall (mod
                                             "scenes", the Python name with its prefix; the short name without "b_"
                                             is registered too). Arguments as Python takes them, positional or by
                                             keyword, Python's defaults where left out (b_ribs L=: a 'T' tuple).
     bld_add_any("scenes", "SUBJECTS", "CpSubjectFn", bs_subject)     compose.h's CpSubjectFn
     bld_add_any("scenes", "prop_fn", "CpPropFn", bs_prop_fn)         compose.h's CpPropFn
   A builder is an FtBuildFn (features.h): b_* take (cx, cy, a, rng) or (cx, cy, a, rng, k=1.0) as the Python does (a
   k given to one that takes none is a TypeError: SC_ERR before any draw); subjects and L_skeleton need k
   (has_scale = 1, scale = k).

   ---- what is asked of the table (the other parts' builders and details):
     features.py's builders inside ours (F.b_campfire, F.b_wheel, F.L_wall, F.L_dais ... in the subjects and props):
         bld_make on an FtCall { "features", "<b_ or L_ name>", positional args as the Python passes them }.
     the props that are details.py templates (D.scatter / single / sunk / trail / path) or styles2.py details
     (S2.f_column / f_post_row / f_vent): bld-features' constructors bld_any(mod, name, "dt_new") (bld_details.h's
     dt_scatter ... dt_vent signatures), run by bld_any("details", "dt_run", "dt_run"); the plain functions
     (S2.f_headstone ...): bld_any("styles2", "f_headstone", "tier"), tagged from TAGS as scenes._tag_all leaves them.
     A prop whose maker is not in the table yet gives None (prop_fn's own except: path).

   ---- solids3d: the comp details made here (props) append to *bs_s3d (NULL = a list of their own, dropped). */
#ifndef BLD_SCENES_H
#define BLD_SCENES_H
#include "bld_gg3.h"
#include "compose.h"

/* the subjects (compose.h CpSubjectFn): SUBJECTS[name](pal); 0 = made (*ctx kept by the table), SC_ERR = Python
   raised (KeyError for a name not in SUBJECTS, or the factory raised) */
int bs_subject(const char *name, const ScPal *pal, FtBuildFn *fn, void **ctx);
extern const char *const BS_SUBJECTS[];          /* SUBJECTS' keys in dict order */
extern const int BS_NSUBJECTS;

/* a builder made here, with the attributes Python sets on it (f.tag, f.kind; NULL = not there) */
typedef struct { FtBuildFn fn; void *ctx; const char *tag, *kind; } BsBuild;
/* scenes.py's b_* / L_skeleton / _several / _either by name and FtCall (the same as the table's factories) */
int bs_make(const FtCall *call, BsBuild *out);   /* 0 made, -2 not one of ours, SC_ERR raised */
int bs_several(const BsBuild *inner, int n0, int n1, double spread, BsBuild *out);       /* _several(build, n=(n0, n1), spread) */
int bs_either(const BsBuild *f1, const BsBuild *f2, double share, BsBuild *out);           /* _either(f1, f2, share) */

/* the props (compose.h CpPropFn): prop_fn(name, pal): *out a NEW ScFn, NULL = None */
int bs_prop_fn(const char *name, const ScPal *pal, ScFn **out);
extern const char *const BS_PROP_NAMES[];        /* PROP_NAMES in order */
extern const int BS_NPROPS;
/* a call as _props() writes it: positional arguments first, then keyword ones (FtArg.kw); up to 10 (FtCall holds 6) */
typedef struct { const char *mod, *name; int nargs; FtArg a[10]; } BsCall;
/* what a prop is made of, as text (the proof compares it with the Python's); NULL out = only the length */
int bs_prop_desc(const char *name, const ScPal *pal, char *out, int cap);
extern FtS3dList *bs_s3d;

void bld_register_scenes(void);
#endif
