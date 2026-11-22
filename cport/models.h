/* models.h: the ready-made 3D models in C.
   1. loading a model file exactly as sd_mesh._load does it with trimesh 5.2.0 (process=False, skip_materials=True):
      OBJ through trimesh.exchange.obj.load_obj + Scene.to_mesh, GLB through trimesh.exchange.gltf.load_glb +
      Scene.to_mesh (node transforms, the scene-graph chain, OpenBLAS's sums), then y up -> z up;
   2. sd_mesh's posing (_rx/_ry/_rz, POSES, the 3x3 chains, v @ R.T): numpy's matmul goes through OpenBLAS, whose
      kernels on this PC sum k = 0..K-1 with fused multiply-adds starting from 0 (measured: 0 differences);
   3. sd_mesh.solid / _above / clip / join / ladder / sect on top of cport\shp.h;
   4. the model packs fetch of tools\get_models.py (WinHTTP + a zip reader and inflater of our own);
   5. the model files the kit uses packed into one compressed blob the exe carries (no models folder, no download).
   Every function names the Python routine it copies. */
#ifndef MODELS_H
#define MODELS_H
#include <stdint.h>
#include "shp.h"

enum { MDL_OK = 0, MDL_ERR_IO = -1, MDL_ERR_RAISE = -2, MDL_ERR_UNSUPPORTED = -3, MDL_ERR_MEM = -4 };
/* MDL_ERR_RAISE: trimesh / numpy would raise here; MDL_ERR_UNSUPPORTED: a path of trimesh this C does not copy
   (none of the kit's model files takes one; see the report in work\cscene\models). */

typedef struct { double *v; int64_t *f; int nv, nf; } MdlMesh;   /* v: nv x 3, f: nf x 3, row-major */
void mdl_free(MdlMesh *m);
int mdl_unsupported_line(void);                            /* the models.c line of the last MDL_ERR_UNSUPPORTED (for reports) */

/* trimesh.load(path, force="mesh" / force="scene" + to_mesh(), process=False, skip_materials=True): the vertices
   and faces as trimesh gives them (y up). .obj or .glb by the name's ending (case-insensitive). */
int mdl_trimesh_load(const char *path, MdlMesh *out);
int mdl_trimesh_obj(const unsigned char *data, size_t n, MdlMesh *out);
int mdl_trimesh_glb(const unsigned char *data, size_t n, MdlMesh *out);

/* sd_mesh._load(rel): (v z up, faces); rel is relative to root (sd_mesh.ROOT = <kit>\models). Cached per rel like
   the lru_cache (the returned mesh is borrowed and lives until mdl_cache_clear). NULL + *err on failure. */
const MdlMesh *mdl_load(const char *root, const char *rel, int *err);
void mdl_cache_clear(void);

/* ---- posing (sd_mesh) ---- */
void mdl_rx(double t, double R[9]);                       /* sd_mesh._rx */
void mdl_ry(double t, double R[9]);                       /* sd_mesh._ry */
void mdl_rz(double t, double R[9]);                       /* sd_mesh._rz */
int mdl_pose(char pose, double R[9]);                     /* sd_mesh.POSES[pose]; 0 = no such pose (KeyError) */
void mdl_mat3(const double A[9], const double B[9], double C[9]);   /* A @ B as numpy gives it (OpenBLAS dgemm) */
void mdl_vRT(const double *v, int n, const double R[9], double *out);   /* v @ R.T (n x 3), out may not be v */

/* ---- sd_mesh.solid ---- */
typedef struct MdlSolid MdlSolid;
/* sd_mesh.solid(rel, mm, cx, cy, yaw, pose, sink, mirror, tilt, tilt_dir, cap) for the loaded V, F (cap 0 = None).
   *foot gets the footprint (new geometry), *top the top; the MdlSolid is the sect closure. Returns MDL_OK or an
   error (a shapely call that would raise: MDL_ERR_RAISE). */
int mdl_solid(const MdlMesh *VF, double mm, double cx, double cy, double yaw, char pose, double sink, int mirror,
              double tilt, double tilt_dir, double cap, MdlSolid **out, GEOSGeometry **foot, double *top);
/* sect(z): the outline above z, a NEW geometry (the caller frees it), or NULL for None. *err = MDL_ERR_RAISE when
   Python would raise (both unions failed). Python hands back the same object for a built level; here it is a copy. */
/* resting (2026-11-21): mdl_settle_next(1) makes the next mdl_solid on this thread turn the posed model further so it
   lies flat on the stable face (of its convex hull, the centre of mass over it) nearest to straight down */
void mdl_settle_next(int on);
void mdl_rest_clear(void);                                /* the stable faces worked out per mesh (mdl_cache_clear too) */
GEOSGeometry *mdl_sect(MdlSolid *s, double z, int *err);
void mdl_solid_free(MdlSolid *s);
/* the posed arrays, for proofs: v (nv x 3) after the last shift, tri (nf x 3 x 3) */
void mdl_solid_arrays(const MdlSolid *s, const double **v, int *nv, const double **tri, int *nt);
/* smooth details: H[j * nx + i] = max(H, the model's top above (x0 + i g, y0 + j g)); 0 */
int mdl_hraster(const MdlSolid *s, double x0, double y0, double g, int nx, int ny, double *H);
/* sd_mesh._above(t, z): the part of triangle t above z, as xy points into out (up to 4 points); returns the count */
int mdl_above(const double t[9], double z, double out[8]);

/* ---- tools\get_models.py ---- */
typedef void (*MdlSay)(const char *line, void *ctx);
/* the kit's folder (the one with models\packs.json). say may be NULL (then stdout). */
int mdl_have(const char *kit, const char *pack);          /* have(p): 1 / 0, -1 no such pack */
int mdl_missing(const char *kit, int all_, char ***names); /* missing(all_): count; *names malloc'd (mdl_strs_free) */
void mdl_strs_free(char **s, int n);
void mdl_explain(const char *kit, char **names, int n, MdlSay say, void *ctx);   /* explain(packs) */
int mdl_fetch(const char *kit, const char *pack, MdlSay say, void *ctx);          /* fetch(p): 1 = here now */
int mdl_ensure(const char *kit, int all_, const char *only, MdlSay say, void *ctx);   /* ensure(): packs still missing */
void mdl_list(const char *kit, MdlSay say, void *ctx);    /* get_models.py --list */
int mdl_zip_here(const char *path);                       /* os.path.exists(z) and zipfile.is_zipfile(z) */
/* the pieces of fetch that need no network, for proofs: unpack a Kenney zip / a "zip" pack's OBJ folder, write a
   "files" pack's License.txt. date = "YYYY-MM-DD" (NULL = today). Return as the Python's branch: 1 ok, 0 refused. */
int mdl_unpack_kenney(const char *kit, const char *pack, const char *zip_path, MdlSay say, void *ctx);
int mdl_unpack_zip_pack(const char *kit, const char *pack, const char *zip_path, MdlSay say, void *ctx);
int mdl_write_files_licence(const char *kit, const char *pack, const char *const *seen, int nseen, const char *date);
/* HTTP GET with the kit's User-Agent (and Referer when not NULL): body malloc'd into *body; returns HTTP status or -1 */
int mdl_http_get(const char *url, const char *referer, int timeout_s, unsigned char **body, size_t *len);

/* ---- the model files packed into the exe (no models folder needed) ---- */
/* the packer, run once when the exe is built: the files rels (under root) into one compressed blob at out_path */
int mdl_pack(const char *root, const char *const *rels, int n, const char *out_path, size_t *raw_size, size_t *packed_size);
int mdl_blob_use(const unsigned char *blob, size_t n);    /* read models from this blob from now on (NULL: from the files again); clears the cache */
int mdl_blob_from_resource(void *module, const char *name);   /* the blob as an RCDATA resource of module (NULL = the exe) */
int mdl_removed(const char *rel);                         /* 1: the user took this model out (removed_models.txt beside the exe) */
int mdl_has(const char *root, const char *rel);           /* os.path.exists(sd_mesh.ROOT / rel), from the blob when one is in use */
#endif
