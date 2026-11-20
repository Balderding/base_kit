/* meshglue.h: the build-path steps between the finished scene and the C height field (cport\field.c), in C: what
   work\cscene\runner\layout_bridge.py still did in Python (marked NOT PORTED YET there):

     mesher.scale_plan + _oval_plan      mg_scale_plan
     mesher.rescale                      mg_rescale
     mesher.field_ops (+ sketch_region, _arc_pts, gg.ellipse_ring / ellipse_offset, gg.magnet_centres /
                       oval_magnet_centres)            mg_field_ops
     the standing spots (sp[4].buffer(1.2, 6))         mg_stand
     cmesh.pack_ops (the "BBF1" stream bbb_kit.exe --core-build reads)   mg_pack_ops
     the build record's sig (md5 of the ops' repr) and want_mm           mg_sig, mg_want_mm
     gg3.smooth_isolated                 mg_smooth_isolated
     gg3.scene_report                    mg_scene_report (plan_hash: json.dumps(plan, sort_keys=True) + md5)
     kitlog (the per-base error list and out\kit_errors.log lines)       mg_log_*
   and mg_build_input(): the whole of layout_bridge.layout_build after build_cmds, in Python's order.

   Proof: work\cscene\meshglue (capture.py + prove.py + mgtest.dll): the BBF1 bytes, the sig, the skipped list, the
   want_mm, the plan floats and pads WKB, and every op's polygons after smooth_isolated, compared byte for byte with
   the Python kit on captured real scenes (round, oval, scenery scale, minis, many styles).

   Rules as in shp.h: geometries returned are new and owned by the caller; shp_init() must have been called.
   Returns: 0 = done, MG_ERR = Python would raise there (the message in mg_err). */
#ifndef MESHGLUE_H
#define MESHGLUE_H
#include <stddef.h>
#include <stdint.h>
#include "geos_dyn.h"
#include "buildcmds.h"

#define MG_ERR (-1)
extern char mg_err[256];                  /* what Python would raise: "GEOSException: ...", "ValueError: ..." */

/* a growing byte buffer (malloc'd; free(b.b)) */
typedef struct { unsigned char *b; size_t n, cap; } MgBuf;
void mg_buf_add(MgBuf *o, const void *p, size_t k);

/* a list of strings (the skipped list, the error list) */
typedef struct { char **s; int n, cap; } MgStrs;
void mg_strs_push(MgStrs *l, const char *s);
void mg_strs_free(MgStrs *l);

/* ---------------- the settings these steps read (os.environ / gg / CFG) ---------------- */
typedef struct {
    double base_mm;                       /* gg.BASE_MM (the layout size) */
    double final_mm;                      /* float(os.environ.get("KIT_FINAL_MM", 0) or 0) */
    double scale;                         /* float(os.environ.get("KIT_SCALE", 1) or 1) */
    int has_oval; double oval[2];         /* gg.oval(): (long, short) mm */
    double tile;                          /* mesher.TILE (MESH_TILE, default 12.0) */
} MgEnv;

/* ---------------- mesher.scale_plan ---------------- */
typedef struct {
    double sc, fin, tx, ty;               /* (scale, base mm, shift x, shift y, ...) */
    GEOSGeometry *pads;                   /* the standing spots on the finished base, or NULL (None); owned */
} MgPlan;
/* pads: s.stand["pads"] (NULL entries = None), npads = 0 when s has no stand / no pads. Returns 1 = a plan,
   0 = None (no scenery scale, round base), MG_ERR never (Python's try catches everything: pads = None). */
int mg_scale_plan(const MgEnv *e, const GEOSGeometry *const *pads, int npads, MgPlan *out);
void mg_plan_free(MgPlan *p);

/* ---------------- mesher.rescale(groups, f, mm, cap=25.0, floor=2.9, tx, ty, pads) ---------------- */
/* out starts empty; pads may be NULL. Commands keep Python's value types (an untouched number keeps its type). */
int mg_rescale(const BcGroups *in, double f, double mm, double cap, double floor_, double tx, double ty,
               const GEOSGeometry *pads, BcGroups *out);

/* ---------------- mesher.field_ops ---------------- */
typedef struct { GEOSGeometry *g; double prec; } MgGeom;          /* a geometry with its shapely.get_precision */
typedef struct {
    int kind;                             /* 1 prism, 2 step, 3 taper, 4 cut, 5 the standing spots */
    char name[64]; int nlen;              /* name.encode("utf-8")[:60] */
    MgGeom g; double a, b, c; int chain;
} MgStep;
typedef struct { MgStep *v; int n, cap; } MgSteps;
typedef struct {
    double R0, H, rim_tan, ref_area;
    MgGeom disc, guard;
    int has_oval; double *TR, *OU; int nTR, nOU;          /* oval: (oa, ob, topr, ring): topr and ring as x, y pairs */
    int has_mag; double rm, dm; double *cens; int ncen;   /* base["mag"] = (radius, depth, [(x, y)]) */
} MgBase;
/* field_ops(groups): *has_base = 0 when no Base extrude was met (Python: base None). skipped strings appended. */
int mg_field_ops(const MgEnv *e, const BcGroups *groups, MgBase *base, int *has_base, MgSteps *steps, MgStrs *skipped);
void mg_base_free(MgBase *b);
void mg_steps_free(MgSteps *s);

/* stand = (sp[4].buffer(1.2, 6), base["H"], sp[0]): the standing spots step's outline (NULL = MG_ERR) */
GEOSGeometry *mg_stand(const MgPlan *p);
/* cmesh.pack_ops(base, steps, stand): the "BBF1" stream appended to out. stand may be NULL (None); stand_H and
   stand_sc are the tuple's other two members. */
int mg_pack_ops(const MgEnv *e, const MgBase *base, const MgSteps *steps, const GEOSGeometry *stand, double stand_H,
                double stand_sc, MgBuf *out);

/* ---------------- the build record ---------------- */
/* sig: f"{style}{BASE_MM:g}" + (f"o{oval[1]:g}" if oval) + md5(repr([(op[0], op[-1], round(op[2], 2), [[round(v, 1) for
   c in p.exterior.coords for v in c] for p in op[1]]) for op in s.ops])).hexdigest()[:16], on the scene as gg3.make
   leaves it (before smooth_isolated). out: at least 200 bytes. */
int mg_sig(const BcScene *s, const char *style, const MgEnv *e, char *out, int cap);
/* want_mm: round(min(25.0, (max(prism tops, default=H) - H) * (sc if fin else 1.0)), 1), 0.0 when only a part of the
   layout is on the base. has_plan / plan: what scale_plan gave. */
double mg_want_mm(const BcScene *s, double H, const MgEnv *e, int has_plan, const MgPlan *plan);

/* ---------------- gg3.smooth_isolated(s, tol) ---------------- */
/* tol = CFG sketch_tolerance_mm (0.03), touch_tol = CFG touching_tolerance_mm (0.005). Sets s->cfg.sketch_tol = 0
   (gg.SKETCH_TOL = 0). s->ops become NEW ops (as Python makes new tuples: a chop keeps its old op objects). */
int mg_smooth_isolated(BcScene *s, double tol, double touch_tol);

/* ---------------- json.dumps + md5 (gg3.scene_report's plan_hash) ---------------- */
enum { MGJ_NULL, MGJ_FALSE, MGJ_TRUE, MGJ_INT, MGJ_FLOAT, MGJ_STR, MGJ_LIST, MGJ_DICT };
typedef struct MgJ { int t; int64_t i; double d; char *s; int n, cap; struct MgJ *v; char **k; } MgJ;   /* dict: n keys k, values v, insertion order */
MgJ mgj_null(void); MgJ mgj_bool(int b); MgJ mgj_int(int64_t i); MgJ mgj_float(double d); MgJ mgj_str(const char *s);
MgJ mgj_list(void); MgJ mgj_dict(void);
void mgj_push(MgJ *list, MgJ v);          /* list.append(v) (takes v over) */
void mgj_set(MgJ *dict, const char *key, MgJ v);          /* dict[key] = v (takes v over; an existing key keeps its place) */
void mgj_free(MgJ *v);
/* json.dumps(v, sort_keys=sort) with Python's defaults (", ", ": ", ensure_ascii, NaN/Infinity, float repr) */
void mgj_dump(const MgJ *v, int sort, MgBuf *out);
/* md5 of bytes as 32 hex digits + 0 */
void mg_md5_hex(const void *p, size_t n, char out[33]);
/* gg3.scene_report(s): {} when there is no plan (*out = an empty dict); else {"theme", "plan" (keys not starting
   with "_"), "plan_line", "plan_hash", "scene_log"[:8]}. plan / scene_log are read, not taken over. */
void mg_scene_report(const char *theme, const MgJ *plan, const char *plan_line, const char *const *scene_log, int nlog, MgJ *out);

/* ---------------- kitlog (one error log for the whole kit: out\kit_errors.log) ---------------- */
/* the records of this process, as kitlog keeps them. caught: kind = the exception's type name, text = str(e), at =
   "file:line in func" or "". Returns the one-line form kitlog.caught returns (for the error list). The log file is
   KIT_ERRORS_DIR\kit_errors.log, else <dir>\kit_errors.log (mg_log_dir). */
void mg_log_dir(const char *dir);
void mg_log_base(const char *name);
int mg_log_mark(void);
const char *mg_log_caught(const char *where, const char *kind, const char *text, const char *at);
const char *mg_log_note(const char *where, const char *text);
void mg_log_since(int mark, MgStrs *out);           /* kitlog.since(m): the one-line forms */

/* ---------------- layout_bridge.layout_build after the layout ---------------- */
typedef struct {
    MgBuf bbf;                            /* the "BBF1" field input */
    char sig[220];                        /* the full sig ("style60" + "o52" + 16 hex) */
    MgStrs skipped;
    int steps;                            /* len(groups) */
    int nsteps;                           /* len(steps) */
    double H, want_mm;
    int has_plan; MgPlan plan;
    double R0, rim_tan;                   /* the field's base (with H): add_solids' rlim = R0 - H * rim_tan - 0.12 */
} MgBuilt;
/* s: the scene as gg3.make leaves it; style; e; tol / touch_tol: as mg_smooth_isolated. Runs sig, smooth_isolated,
   build_cmds, scale_plan, rescale, field_ops, the standing spots, pack_ops and want_mm, in mesher.build's order.
   Returns 0, MG_ERR (Python raises: "no base in the command groups" or a GEOS error, in mg_err). */
int mg_build_input(BcScene *s, const char *style, const MgEnv *e, const GEOSGeometry *const *pads, int npads,
                   double tol, double touch_tol, MgBuilt *out);
void mg_built_free(MgBuilt *b);

/* repr(x) of a float as Python writes it (shortest round trip) */
int mg_float_repr(double x, char *out, int cap);

/* ======================= --supports on: mesher.add_solids and scenes' true 3D builders ======================= */
/* a mesh as trimesh holds it: nv vertices (x, y, z doubles), nt faces (3 vertex indices) */
typedef struct { double *v; int nv; int *f; int nt; } MgMesh;
void mg_mesh_free(MgMesh *m);
/* scenes._extrude(poly, z0, z1): a closed block (vertices shared by (round(x, 5), round(y, 5), z)) */
int mg_extrude3d(const GEOSGeometry *poly, double z0, double z1, MgMesh *out);
/* scenes._revolve(cx, cy, prof, seg, squash, a, zb, sink=0.3): prof = n (r, z) pairs */
int mg_revolve3d(double cx, double cy, const double *prof, int n, int seg, double squash, double a, double zb, double sink, MgMesh *out);
/* scenes._sweep(pts, radii, seg=12, squash=1.0): pts = n (x, y, z) */
int mg_sweep3d(const double *pts, const double *radii, int n, int seg, double squash, MgMesh *out);
/* slab's builder: _extrude(poly.intersection(Point(0, 0).buffer(R0 - H * tan(radians(rim)) - 0.15, 96)), zb + z0 * zs, zb + z1 * zs) */
int mg_slab3d(const GEOSGeometry *poly, double z0, double z1, double zb, double zs, double R0, double H, double rim, MgMesh *out);
/* trimesh.Trimesh.volume (mass_properties' integral, numpy's pairwise sum) */
double mg_volume(const MgMesh *m);
/* add_solids' handling of ONE shell (after mk(zb, zs) gave it): faces turned outwards if its volume is negative, then
   pressed back inside the base's top: an oval (has_oval: A, B the oval's sizes) or the round top of radius rlim
   (build passes R0 - H * rim_tan - 0.12). MG_ERR for a shell with no faces (Python: IndexError, add_solids skips it:
   skipped += f"solid {cid}: IndexError" and kitlog.caught(f"solid {cid} dropped", e)). */
int mg_solid_fit(MgMesh *t3, int has_oval, double A, double B, double rlim);
/* trimesh.util.concatenate([m] + extra): vertices stacked, faces offset; m grows */
void mg_concat(MgMesh *m, const MgMesh *extra, int n);

/* ======================= tools\gui_job.with_figure: the figure standing beside the base ======================= */
/* trimesh.creation.cylinder(radius, height, sections) (revolve + Trimesh(process=True)'s merge_vertices) */
int mg_cylinder(double radius, double height, int sections, MgMesh *out);
/* trimesh.creation.icosphere(subdivisions, radius) */
int mg_icosphere(int subdivisions, double radius, MgMesh *out);
/* the figure of with_figure for scenery scale sc (> 1.01) beside a base whose largest x is r (m.bounds[1][0]):
   concatenate([cylinder(w/2, h*0.82, 20) + [0, 0, h*0.41], icosphere(2, w*0.45) + [0, 0, h*0.82 + w*0.4]]) moved by
   [r*0.71 + w + 2, -(r*0.71 + w + 2), 0], with h = 32*sc/3, w = max(3, h*0.28). The picture is then
   mesher.shot(concatenate([m, figure]), W, H, 34, -60): mg_concat(base, &figure, 1) and the C shot. */
int mg_figure(double sc, double r, MgMesh *out);
/* trimesh's merge_vertices() (tol.merge: round(v * 1e8), first occurrence kept, unreferenced vertices dropped) */
void mg_merge_vertices(MgMesh *m);

/* ======================= PIL 13: Image.thumbnail((px, px), LANCZOS) of an RGBA image, reduce included ======================= */
/* what tools\gui_job.see_through does after the crop: thumbnail with reducing_gap 2.0, i.e. RGBA -> RGBa, Image.reduce
   by (int(w / x / 2) or 1, ...) when either is > 1 (ImagingReduce, every special case), the LANCZOS resample with the
   fractional box that leaves (ImagingResampleInner), RGBa -> RGBA. pix: w x h RGBA. *out (malloc'd RGBA) is *ow x *oh;
   returns 0 when no resize is needed (px >= w and px >= h: *out = NULL), 1 when done. */
int mg_thumbnail_rgba(const unsigned char *pix, int w, int h, int px, unsigned char **out, int *ow, int *oh);
/* Image.reduce((fx, fy), box) of a 4-band 8-bit image (box = PIL's (x0, y0, x1, y1)): malloc'd, *ow x *oh */
unsigned char *mg_reduce4(const unsigned char *pix, int w, int h, int fx, int fy, const int box[4], int *ow, int *oh);
/* ImagingResample(LANCZOS) of a 4-band 8-bit image with a float box (x0, y0, x1, y1): malloc'd ow x oh */
unsigned char *mg_resample4(const unsigned char *pix, int w, int h, int ow, int oh, const float box[4]);

/* ======================= run_set.sheet: the <style>_sheet.png contact sheet ======================= */
/* matplotlib's layout redone (subplots(rows, 5, figsize=(20, 4.2 rows)), every axis off, imshow, set_title(fontsize=10),
   tight_layout() with its one pass of _auto_adjust_subplotpars, savefig(dpi=70)): the same axes and title places.
   The pixels are our own (not matplotlib's Agg): the picture resampled LANCZOS into its axes box, the title drawn
   with DejaVu Sans glyphs (matplotlib's font) rendered by FreeType. A flagged title is not red as matplotlib's
   "#c00" (balderding cannot read red): MG_SHEET_FLAG_RGB. */
#define MG_SHEET_OK_RGB 0x222222
#define MG_SHEET_FLAG_RGB 0x1f3a93
typedef struct {
    const unsigned char *rgba; int w, h;  /* the base's _iso.png as RGBA, or NULL (not ok / no picture: an empty axis) */
    const char *title;                    /* f"{base}  detail {detail}  {note}" (mg_sheet_title) */
    int ok;
} MgSheetItem;
/* f"{base}  detail {detail}  {note}", note = "OK" | "FLAGGED", + " (variant)" when the last try was a fresh variant */
void mg_sheet_title(const char *base, const char *detail, int ok, int variant, char *out, int cap);
/* the sheet as opaque RGBA, *W x *H (1400 x 294 rows); malloc'd. axes (optional, 4 doubles per item): each axes box
   in figure fractions (x0, y0, x1, y1), as matplotlib places it, for the proof */
unsigned char *mg_sheet(const MgSheetItem *items, int n, int *W, int *H, double *axes);

/* ======================= run_set.drop_specks(path, max_mm3=1.5) ======================= */
/* on the STL's bytes (binary): trimesh.load (the file's normals kept as trimesh keeps them, merge_vertices), split into
   parts across edges in exactly two faces, every part smaller than max_mm3 dropped unless it is the biggest. *removed =
   how many were dropped; when > 0 the new STL (trimesh's export: 80 zero bytes, count, normal + corners as float32,
   u16 0) is appended to out. allow_supports (CFG): nothing is done. Every step as trimesh 5.2 / networkx 3.7 does it
   with no scipy installed: the parts are networkx connected_components (BFS sets, faces in CPython set order), each
   part of 4 faces or more gets repair.fill_holes (3- and 4-edge holes closed, nx.cycle_basis). */
int mg_drop_specks(const unsigned char *stl, size_t n, double max_mm3, int allow_supports, MgBuf *out, int *removed);

/* ======================= run_set.close_seams(path) ======================= */
/* the repair before "not watertight", on the STL's bytes: merge, _drop_slivers, remove_unreferenced_vertices,
   _fix_tjunctions, _fill_planar_holes, _fill_small_loops in run_set's order; *repaired = 1 and the new STL (normals
   computed, as trimesh exports a fresh Trimesh) appended to out when the mesh ends up closed (Python saves it then),
   0 when it was closed already or stays open. Not bit-exact in one place: three products numpy hands to OpenBLAS dgemv
   ((n, 3) @ (3,): the points' parameter along an open edge, the flatness of a hole, a hole's 2D coordinates) use an FMA
   loop here (the dgemv kernel's order is not known; about 1 in 5 such values differs in the last bit). They only meet
   tolerances (1e-6, 2e-4, 2e-3) and the ear clipper's signs, so the faces come out the same except in a tie. */
int mg_close_seams(const unsigned char *stl, size_t n, MgBuf *out, int *repaired);
#endif
