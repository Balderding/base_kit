/* builders.h: the one table of the kit's builders in C (2026-11-19, main session).
   A builder is anything the scene code calls by name to make a detail: features.py b_* and L_*, scenes.py S_* and
   b_* and the subjects, details.py templates (scatter, single, sunk, trail, path ...), gg3.py tier functions, styles2.py
   f_*, the style-only details of sd_a..sd_e / sd_r* / sd_r_x_* (registered with sd_reg via sdr_register in
   features.h), and the newer styles' own builders in style_<name>.py.
   Each builder file registers its builders once, in its own bld_register_<part>() (listed in builders.c), keyed by
   the Python module and function name exactly as written in the kit (FtCall.mod / FtCall.name in features.h), so a
   call recorded as data (FtCall: name, positional and keyword arguments) is turned into a working builder:
       FtBuildFn fn; void *ctx;
       if (bld_make(call, &fn, &ctx) == 0) ... fn(ctx, cx, cy, a, rng, has_scale, scale, &foot, &layers) ...
   A factory reads the call's arguments (Python's defaults where an argument is left out) and returns a builder with
   its own ctx (owned by the table: freed by bld_free_all at the end of a run). Kinds of callable that are not an
   FtBuildFn (a tier function fn(s, rng, lvl), a theme subject, a sampler) are registered with bld_add_any and
   fetched with bld_any, each with the signature its caller documents. */
#ifndef BUILDERS_H
#define BUILDERS_H
#include "features.h"

typedef int (*BldFactory)(const FtCall *call, FtBuildFn *fn, void **ctx);          /* 0 = made; SC_ERR = Python would raise */
void bld_add(const char *mod, const char *name, BldFactory f);                      /* a later one of the same mod+name takes the earlier one's place */
BldFactory bld_get(const char *mod, const char *name);                              /* NULL = not ported yet */
int bld_make(const FtCall *call, FtBuildFn *fn, void **ctx);                        /* bld_get + run it; -2 = not ported yet; -3 = a call with no module (FT_NEW / FT_LMS data): use bld_ft_make in bld_features.h */
void bld_keep(void *ctx, void (*free_fn)(void *));                                 /* hand a ctx to the table to free at the end */
void bld_free_all(void);

void bld_add_any(const char *mod, const char *name, const char *sig, void *fn);    /* any other callable: sig names its C signature */
void *bld_any(const char *mod, const char *name, const char *sig);                  /* NULL = not there or a different signature */

void bld_register_all(void);                                                        /* every part's bld_register_<part>(), in a fixed order */
int bld_count(void);
#endif
