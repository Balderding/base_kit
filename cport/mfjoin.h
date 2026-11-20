/* mfjoin.h: supports on, joined into one solid. With supports allowed the kit adds each object as its own closed shell
   sunk into the base; mf_join makes the base and every shell that reaches it one solid with Manifold (a union), so
   the STL is one clean piece. A shell that touches nothing after the join (a piece hanging in the air) and the
   zero-thickness slivers a join can leave are dropped and counted. The mesh is replaced in place (v: nv x 3 doubles,
   f: nt x 3 ints, both malloc'd). 0 = ok (r says what happened); -1 = Manifold could not join them (err says why;
   the mesh is left as it was). */
#ifndef MFJOIN_H
#define MFJOIN_H
#include <stddef.h>
typedef struct { int joined, parts_in, not_closed, floating, slivers, unpinched; } MfJoin;   /* unpinched: parts where pieces touching only along an edge were split apart first */
int mf_join(double **v, int *nv, int **f, int *nt, MfJoin *r, char *err, size_t errcap);

/* smaller files: the mesh simplified by Manifold within tol mm (flat areas with fewer, bigger triangles; the shape
   moves no more than tol). Replaced in place. 0 = ok; -1 = left as it was (err says why) */
int mf_simplify(double **v, int *nv, int **f, int *nt, double tol, char *err, size_t errcap);
/* the strict print check: Manifold's own test of a closed, valid solid. 0 = fine, else Manifold's status
   (mf_status_text says it in words) */
int mf_check(const double *v, int nv, const int *f, int nt);
const char *mf_status_text(int st);
#endif
