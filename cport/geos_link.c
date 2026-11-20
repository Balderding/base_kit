/* geos_link.c: the static-mode twin of geos_dyn.c. Link this file INSTEAD of geos_dyn.c, plus the GEOS 3.14.1 static
   libraries (built by cport\geos_static\build_geos.cmd), and the exe carries GEOS inside itself: no geos_c.dll.
       clang %CLANG_FLAGS% ... -static ... geos_link.c -Lgeos_static\lib -lgeos_c -lgeos -lstdc++ -lwinpthread
   (-static: libstdc++, libgcc and winpthread go into the exe too; it then needs only Windows' own DLLs + the UCRT.)
   It fills the same GeosApi table G and context gctx (geos_dyn.h), so every C file that uses G.<fn>(gctx, ...) works
   unchanged. The numbers are those of shapely's geos_c.dll (proven: work\cscene\geosstatic).

   The functions are declared here from the GEOS_FNS list itself (C links by name; the opaque pointer types of
   geos_dyn.h have the same ABI as geos_c.h's), so the list in geos_dyn.h stays the one place to add a function. */
#include <stdio.h>
#include "geos_dyn.h"

GeosApi G;
GEOSContextHandle_t gctx;
char geos_err[512];

#define GEOS_DECL(ret, name, args) extern ret name args;
GEOS_FNS(GEOS_DECL)
#undef GEOS_DECL

static void on_err(const char *msg, void *u) { (void)u; snprintf(geos_err, sizeof geos_err, "%s", msg); }

int geos_load(void) {
    if (gctx) return 1;
#define GEOS_SET(ret, name, args) G.name = name;
    GEOS_FNS(GEOS_SET)
#undef GEOS_SET
    gctx = G.GEOS_init_r();
    if (!gctx) { fprintf(stderr, "GEOS_init_r failed\n"); return 0; }
    G.GEOSContext_setErrorMessageHandler_r(gctx, on_err, NULL);
    return 1;
}
