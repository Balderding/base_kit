/* smooth3d.h: smooth details (2.1, settings "smooth_details", --smooth; not in the Python kit). The rounded solids
   sculpt registered for a chain (solids.h so_smooth3d) become one smooth closed shell that cannot overhang:
     - the envelope height H(x, y) of the group (the highest point of any of its solids above that spot: so_hraster)
       sampled on a fine grid, as a surface z = H(x, y) (every face of it looks up),
     - cut to the group's footprint (union of sect(0)) by a straight prism (Manifold intersection): vertical sides,
     - down into the ground (below the lowest ground under it), flat at the bottom.
   The shells go to the runner in the "BBS1" blob (solids3d.h s3_blob) and are joined into the base by mf_join, so the
   STL is one solid. Scaled bases (KIT_FINAL_MM / KIT_SCALE) get the same map as mesher.rescale gives the base.
   Global names start with sm_shell_ (samplers.h has sm_). */
#ifndef SMOOTH3D_H
#define SMOOTH3D_H
#include <stdint.h>
#include "solids.h"
#include "meshglue.h"
#include "features.h"
#include "solids3d.h"
#include "manifold/manifoldc.h"

/* mesher.rescale's map of a scaled base (meshglue rs_P / rs_Z): x * f + tx, y * f + ty, z by rs_Z; on = 0: none */
typedef struct { int on; double f, tx, ty, H, cap, floor_; } SmShellXf;
/* the grid step on the finished base, mm (settings "smooth_grid_mm", default 0.08) */
extern double sm_shell_grid_mm;
/* one shell for one s.solids3d entry of a smooth chain; 0 = built (out filled), 1 = nothing of it on the base, -1 = not (err says why) */
/* region: what the layout kept of the object's marker (the union of its ghost ops' outlines, laid out): the shell is
   cut to it (and to the rim), so it never covers what the layout gave to something else */
int sm_shell_build(const FtS3d *e, const SoSolids *sol, const GEOSGeometry *foot, const GEOSGeometry *region, const SmShellXf *xf,
                   int has_oval, double A, double B, double rlim, MgMesh *out, char *err, int errcap);
/* a surface z = Z[j * nx + i] over the grid (x0 + i g, y0 + j g), layout units (-1e30: not covered; Z is changed),
   down to zbot, cut straight down at foot, mapped onto the finished base by M * p + (bx, by) and xf's heights, kept
   inside rim (finished base; NULL = no rim): a NEW Manifold, or NULL (err[0] != 0: an error; else nothing left) */
ManifoldManifold *sm_grid_cut(double *Z, int nx, int ny, double x0, double y0, double g, double zbot, const GEOSGeometry *foot,
                              double M, double bx, double by, const SmShellXf *xf, const GEOSGeometry *rim, char *err, int errcap);
/* every smooth entry of l whose chain is alive[k] (the ghost kinds of the ops still on the base; region[k] their
   outlines): built and appended to out; a failure adds "smooth {cid}: why" to skipped */
void sm_shell_add_all(const FtS3dList *l, const int64_t *alive, const GEOSGeometry *const *region, int nalive, const SmShellXf *xf,
                      int has_oval, double A, double B, double rlim, S3Shells *out, MgStrs *skipped);
#endif
