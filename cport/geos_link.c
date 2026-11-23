/* geos_link.c: the static-mode twin of geos_dyn.c. Link this file INSTEAD of geos_dyn.c, plus the GEOS 3.14.1 static
   libraries (built by cport\geos_static\build_geos.cmd), and the exe carries GEOS inside itself: no geos_c.dll.
       clang %CLANG_FLAGS% ... -static ... geos_link.c -Lgeos_static\lib -lgeos_c -lgeos -lstdc++ -lwinpthread
   (-static: libstdc++, libgcc and winpthread go into the exe too; it then needs only Windows' own DLLs + the UCRT.)
   It fills the same GeosApi table G and context gctx_tl (geos_dyn.h), so every C file that uses G.<fn>(gctx_tl, ...) works
   unchanged. The numbers are those of shapely's geos_c.dll (proven: work\cscene\geosstatic).

   The functions are declared here from the GEOS_FNS list itself (C links by name; the opaque pointer types of
   geos_dyn.h have the same ABI as geos_c.h's), so the list in geos_dyn.h stays the one place to add a function. */
#include <stdio.h>
#include "geos_dyn.h"

GeosApi G;
_Thread_local GEOSContextHandle_t gctx_tl;          /* PLAN-threads step 1: one per thread */
_Thread_local char geos_err[512];
static volatile long geos_table;          /* the function table: filled once */

#define GEOS_DECL(ret, name, args) extern ret name args;
GEOS_FNS(GEOS_DECL)
#undef GEOS_DECL

static void on_err(const char *msg, void *u) { (void)u; snprintf(geos_err, sizeof geos_err, "%s", msg); }

int geos_load(void) {
    if (gctx_tl) return 1;
    if (!geos_table) {
#define GEOS_SET(ret, name, args) G.name = name;
        GEOS_FNS(GEOS_SET)
#undef GEOS_SET
        geos_table = 1;
    }
    gctx_tl = G.GEOS_init_r();
    if (!gctx_tl) { fprintf(stderr, "GEOS_init_r failed\n"); return 0; }
    G.GEOSContext_setErrorMessageHandler_r(gctx_tl, on_err, NULL);
    return 1;
}
void geos_thread_end(void) { if (gctx_tl) { G.GEOS_finish_r(gctx_tl); gctx_tl = NULL; } }
GEOSContextHandle_t geos_ctx(void) { if (!gctx_tl) geos_load(); return gctx_tl; }          /* a thread's first GEOS call */
GEOSContextHandle_t geos_new_ctx(void) { GEOSContextHandle_t c; geos_ctx(); c = G.GEOS_init_r(); if (c) G.GEOSContext_setErrorMessageHandler_r(c, on_err, NULL); return c; }          /* a further context (mt.c's workers) */
