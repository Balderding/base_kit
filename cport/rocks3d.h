/* rocks3d.h: organic rocks (2.1, settings "organic_rocks", --rocks; KIT_ROCKS; not in the Python kit). The rocks and
   stones of a base (the raised pieces of the rock kinds: scattered stones, boulders, heaps, the chamfered pieces) are
   not made as flat-topped stacked pieces in the height field any more but as smooth, lumpy, natural shapes:
     - build_cmds hands every such piece over (after its pruning and clipping) instead of raising it,
     - the pieces of one rock (a stack of tiers, a heap's bands: each sitting inside the one below) make one rock,
     - its top: a rounded foot standing a little up from the ground, then each tier's rise blended in over a width
       like its height, a few soft lumps of noise (seeded by the rock's place, the same every build), no higher than
       the pieces were,
     - as a surface z = H(x, y) cut straight down at the rock's outline and sunk into the ground: nothing overhangs.
   The shells go to the runner with the smooth details' (the "BBS1" blob) and are joined into the base by mf_join.
   Global names start with ork_. */
#ifndef ROCKS3D_H
#define ROCKS3D_H
#include "smooth3d.h"

extern int ork_on;                         /* the switch (pipeline.c sets it) */
int ork_kind(const char *kind);            /* 1 = a rock kind made organic (settings "organic_rock_kinds" may name them) */
void ork_set_kinds(const char *const *kinds, int n);          /* n < 0: the built-in list */
void ork_clear(void);                      /* forget the pieces handed over (before each build_cmds) */
void ork_take(double z0, double top, GEOSGeometry *const *polys, int np);   /* build_cmds hands one piece over (polys copied) */
/* every rock of the pieces handed over: built and appended to out (H: the deck height); a failure adds
   "rock: why" to skipped */
void ork_build(const SmShellXf *xf, double H, int has_oval, double A, double B, double rlim, S3Shells *out, MgStrs *skipped, int *nrocks);
#endif
