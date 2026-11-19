/* bbbcore.dll: the kit's C parts, called from Python (ctypes) while the port is under way.
     int  bbb_mesh(const unsigned char *blob, size_t n, double **v, int *nv, int **t, int *nt, char *err, int errlen)
          blob = the mesher's input in the .min format (cport/dump_mesh.py); 1 = done (free v and t with bbb_free),
          0 = an error (err says what; the caller falls back to the Python mesher)
     void bbb_free(void *p)
   GEOS is the geos_c.dll shapely itself uses (already loaded in the Python process). */
#include <windows.h>
#include <stdio.h>
#include <string.h>
#include "mesh.h"
#include "pyx.h"

static int ready = -1;
static int init(void) { if (ready < 0) ready = geos_load() && pyx_load(); return ready; }

__declspec(dllexport) int bbb_mesh(const unsigned char *blob, size_t n, double **v, int *nv, int **t, int *nt, char *err, int errlen) {
    MeshIn in; MeshOut out; memset(&out, 0, sizeof out); *v = NULL; *t = NULL; *nv = *nt = 0;
    if (!init()) { snprintf(err, errlen, "GEOS or the UCRT math could not be loaded"); return 0; }
    if (!mesh_read_buf(blob, n, &in)) { snprintf(err, errlen, "input not readable"); mesh_in_free(&in); return 0; }
    int ok = mesh_from_field(&in, &out);
    mesh_in_free(&in);
    if (!ok) { snprintf(err, errlen, "%s", out.err); free(out.v); free(out.t); return 0; }
    *v = out.v; *nv = out.nv; *t = out.t; *nt = out.nt; return 1;
}
__declspec(dllexport) void bbb_free(void *p) { free(p); }
__declspec(dllexport) const char *bbb_version(void) { return "bbbcore 1 (mesher)"; }
