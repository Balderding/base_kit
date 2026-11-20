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
#include "scatter.h"

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
__declspec(dllexport) const char *bbb_version(void) { return "bbbcore 2 (mesher, rock clusters)"; }

/* gg3.cluster of gg.stone rocks (scatter.c). state: random.getstate()[1] (624 words + the index), advanced in place
   only when it worked. Returns how many rocks (their closed rings: lens[i] points each, xy flat), -1 = do it in Python. */
__declspec(dllexport) int bbb_cluster(uint32_t *state, int round_, double r0, double r1, int nv_lo, int nv_hi, int nv_const, double aspect,
                                      const double *csx, const double *csy, int n, double cx, double cy, double spread, double gap,
                                      int *lens, double *xy, int cap) {
    static ScatterOut out; PyRandom rng; ScatterSpec sp; int k, i;
    if (!init()) return -1;
    memcpy(rng.mt, state, sizeof rng.mt); rng.index = (int)state[624];
    sp.round_ = round_; sp.r0 = r0; sp.r1 = r1; sp.nv_lo = nv_lo; sp.nv_hi = nv_hi; sp.nv_const = nv_const; sp.aspect = aspect; sp.csx = csx; sp.csy = csy;
    k = scatter_cluster(&rng, &sp, n, cx, cy, spread, gap, &out);
    if (k < 0 || out.ncoord > cap) return -1;
    for (i = 0; i < k; i++) lens[i] = out.len[i];
    memcpy(xy, out.xy, sizeof(double) * 2 * out.ncoord);
    memcpy(state, rng.mt, sizeof rng.mt); state[624] = (uint32_t)rng.index;
    return k;
}
