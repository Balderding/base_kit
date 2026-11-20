/* scatter.h: gg3.cluster() of gg.stone() rocks in C (scatter.c). */
#ifndef SCATTER_H
#define SCATTER_H
#include "pyrandom.h"
#define SC_MAXNV 16
#define SC_MAXGOT 64
#define SC_MAXCOORD 8192
typedef struct {
    int round_;                  /* stone(..., round_=True): three circles' hull */
    double r0, r1;               /* the radius: rng.uniform(r0, r1), drawn by the make_one lambda */
    int nv_lo, nv_hi;            /* > 0: the lambda draws nv = rng.randint(nv_lo, nv_hi) after the radius (sh_rock) */
    int nv_const;                /* else this nv is passed (0 = stone draws randint(5, 8) itself) */
    double aspect;               /* 0 = stone draws uniform(.6, .95) */
    const double *csx, *csy;     /* gg._CSX, gg._CSY: numpy's own cos/sin of the 40 angles, handed over from Python */
} ScatterSpec;
typedef struct { int n, ncoord; int len[SC_MAXGOT]; double xy[2 * SC_MAXCOORD]; } ScatterOut;
int scatter_cluster(PyRandom *rng, const ScatterSpec *sp, int n, double cx, double cy, double spread, double gap, ScatterOut *out);          /* how many shapes, -1 = do it in Python */
#endif
