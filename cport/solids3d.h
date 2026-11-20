/* solids3d.h: --supports on, the true 3D solids (solids3d.c). Copied from the Python:
   scenes.py     the x[2] lambdas of slab, vault, lathe, loft and tube (what sculpt stores in TRUE3D[chain])
   mesher.py     add_solids (the shells of the objects still on the base, faced outwards, pressed inside the top)
   The shell makers themselves (_extrude, _revolve, _sweep, slab's cut, the volume sign, the pressing) are meshglue's
   mg_extrude3d / mg_revolve3d / mg_sweep3d / mg_slab3d / mg_solid_fit. Global names start with s3_.
   Proof: work\supports\prove_makers.py (each maker against Python), work\supports\e2e.py (whole jobs). */
#ifndef SOLIDS3D_H
#define SOLIDS3D_H
#include <stdint.h>
#include "solids.h"
#include "meshglue.h"
#include "features.h"

/* x[2](zb, zs) of one solid that has a 3D build: 0 with *out filled, or MG_ERR (Python raises) with the exception's
   type name in exc and str(e) in text */
int s3_build(const Solid *s, double zb, double zs, MgMesh *out, char *exc, int exccap, char *text, int textcap);

/* the shells add_solids keeps, in its order */
typedef struct { MgMesh *v; int n, cap; } S3Shells;
void s3_shells_free(S3Shells *s);
/* mesher.add_solids(s, m, skipped, rlim) without the concatenate: for every (cid, zb, zs) of s.solids3d whose cid is
   in alive (the .ghost of the ops' kinds), every TRUE3D[cid] maker is built and fitted (mg_solid_fit). A maker that
   raises adds "solid {cid}: {Exc}" to skipped and logs kitlog.caught(f"solid {cid} dropped", e). */
void s3_add_solids(const FtS3dList *l, const int64_t *alive, int nalive, int has_oval, double A, double B, double rlim,
                   S3Shells *out, MgStrs *skipped);
/* the shells as the "BBS1" blob: u32 count, then per shell u32 nv, u32 nt, nv * 3 doubles, nt * 3 int32 */
void s3_blob(const S3Shells *s, MgBuf *out);
#endif
