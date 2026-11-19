/* mt.h: threads for the C parts. KIT_MESH_THREADS = n (the window's CPU threads switch sets it through Python);
   0 or 1 = off, which is the proven one-core path, bit for bit the Python mesher. */
#ifndef MT_H
#define MT_H
#include "geos_dyn.h"
typedef void (*MtFn)(int item, GEOSContextHandle_t ctx, void *user);
int mt_threads(void);                                       /* how many threads to use; 0 = off */
void mt_run(int nitems, int nthreads, int grain, MtFn fn, void *user); /* the items handed out one at a time; every thread has its own GEOS
                                                                         context; at most one thread per `grain` items (as many as the work needs) */
#endif
