/* smooth3d.c: smooth details, the shells (see smooth3d.h). */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "shp.h"
#include "smooth3d.h"
extern GEOSGeometry *mg_rrect_poly(double A, double B, double d);          /* meshglue.c: a square or rectangle base inset by d */
#include "manifold/manifoldc.h"

double sm_shell_grid_mm = 0.08;

static double sm_Z(const SmShellXf *x, double z) {          /* meshglue rs_Z */
    if (!x->on) return z;
    if (z >= x->H) { double a = x->H + (z - x->H) * x->f, c = x->H + x->cap; return a < c ? a : c; }
    { double a = x->H - (x->H - z) * x->f, b = x->floor_ < z ? x->floor_ : z; return a > b ? a : b; }
}
static void sm_fail(char *err, int cap, const char *t) { snprintf(err, (size_t)cap, "%s", t); }

/* the rings of the footprint as Manifold polygons (outer rings counter-clockwise, holes clockwise) */
static ManifoldPolygons *sm_polys(const GEOSGeometry *g) {
    ShpList ps = { 0 }; int nr = 0, cap = 1 << 14; double *xy = (double *)malloc(sizeof(double) * 2 * (size_t)cap);
    ManifoldSimplePolygon **sp = NULL; ManifoldPolygons *out;
    shp_polys_of(g, &ps);
    for (int i = 0; i < ps.n; i++) {
        int nh = shp_num_interiors(ps.g[i]);
        for (int r = -1; r < nh; r++) {
            const GEOSGeometry *ring = r < 0 ? shp_exterior(ps.g[i]) : shp_interior(ps.g[i], r);
            int n;
            while ((n = shp_coords(ring, xy, cap)) < 0 && cap < (1 << 24)) { cap *= 4; xy = (double *)realloc(xy, sizeof(double) * 2 * (size_t)cap); }
            if (n < 4) continue;
            n--;          /* the closing point */
            double a = 0; for (int k = 0; k < n; k++) { int q = (k + 1) % n; a += xy[2 * k] * xy[2 * q + 1] - xy[2 * q] * xy[2 * k + 1]; }
            int want_ccw = r < 0;
            ManifoldVec2 *pv = (ManifoldVec2 *)malloc(sizeof(ManifoldVec2) * (size_t)n);
            for (int k = 0; k < n; k++) { int s = ((a > 0) == want_ccw) ? k : n - 1 - k; pv[k].x = xy[2 * s]; pv[k].y = xy[2 * s + 1]; }
            sp = (ManifoldSimplePolygon **)realloc(sp, sizeof(*sp) * (size_t)(nr + 1));
            sp[nr++] = manifold_simple_polygon(manifold_alloc_simple_polygon(), pv, (size_t)n);
            free(pv);
        }
    }
    shp_list_free(&ps); free(xy);
    if (!nr) { free(sp); return NULL; }
    out = manifold_polygons(manifold_alloc_polygons(), sp, (size_t)nr);
    for (int i = 0; i < nr; i++) manifold_delete_simple_polygon(sp[i]);
    free(sp);
    return out;
}

/* the map of one shell: layout point p -> finished base M * p + (bx, by), z by sm_Z */
typedef struct { double M, bx, by; const SmShellXf *xf; ManifoldManifoldVec *vec; int bad; } SmMap;
static ManifoldManifold *sm_solid(const double *v, int nv, const uint64_t *f, int nt) {
    ManifoldMeshGL64 *mg = manifold_meshgl64(manifold_alloc_meshgl64(), (double *)v, (size_t)nv, 3, (uint64_t *)f, (size_t)nt);
    ManifoldMeshGL64 *mm = manifold_meshgl64_merge(manifold_alloc_meshgl64(), mg);
    ManifoldManifold *m = manifold_of_meshgl64(manifold_alloc_manifold(), mm);
    manifold_delete_meshgl64(mg); manifold_delete_meshgl64(mm);
    if (manifold_status(m) != MANIFOLD_NO_ERROR) { manifold_delete_manifold(m); return NULL; }
    return m;
}
/* so_envelope's pieces: mapped onto the finished base, into the list to join */
static void sm_take(void *u, const double *v, int nv, const int *f, int nt) {
    SmMap *q = (SmMap *)u; double *w = (double *)malloc(sizeof(double) * 3 * (size_t)nv); uint64_t *t = (uint64_t *)malloc(sizeof(uint64_t) * 3 * (size_t)nt); ManifoldManifold *m;
    for (int i = 0; i < nv; i++) { w[3 * i] = q->M * v[3 * i] + q->bx; w[3 * i + 1] = q->M * v[3 * i + 1] + q->by; w[3 * i + 2] = sm_Z(q->xf, v[3 * i + 2]); }
    for (int i = 0; i < 3 * nt; i++) t[i] = (uint64_t)f[i];
    m = sm_solid(w, nv, t, nt); free(w); free(t);
    if (!m) { q->bad++; return; }
    manifold_manifold_vec_push_back(q->vec, m); manifold_delete_manifold(m);
}
/* a surface z = Z[j * nx + i] over the grid (x0 + i g, y0 + j g) (layout units and heights; nodes at -1e30 are not
   covered: they take the highest neighbour's height, 3 nodes out, so the straight cut at the footprint meets a surface
   that goes on, not one falling to the ground between two nodes), down to zbot, cut straight down at foot: mapped
   onto the finished base (M, bx, by, xf) and kept inside rim (finished base, may be NULL). A NEW Manifold, or NULL
   (err says why; err[0] = 0: nothing left, not an error) */
ManifoldManifold *sm_grid_cut(double *Z, int nx, int ny, double x0, double y0, double g, double zbot, const GEOSGeometry *foot,
                              double M, double bx, double by, const SmShellXf *xf, const GEOSGeometry *rim, char *err, int errcap) {
    double ztop = -1e30, *V = NULL, zb = sm_Z(xf, zbot); uint64_t *T = NULL;
    GEOSGeometry *fs = NULL, *fm = NULL, *fk = NULL; ManifoldPolygons *pp = NULL; ManifoldManifold *pr = NULL, *pr2 = NULL, *blk = NULL, *res = NULL;
    err[0] = 0;
    {
        double *Z2 = (double *)malloc(sizeof(double) * (size_t)nx * (size_t)ny);
        for (int it = 0; it < 3; it++) {
            memcpy(Z2, Z, sizeof(double) * (size_t)nx * (size_t)ny);
            for (int j = 0; j < ny; j++) for (int i = 0; i < nx; i++) {
                size_t k = (size_t)j * nx + i; double b = -1e30;
                if (Z2[k] > -1e29) continue;
                for (int dj = -1; dj <= 1; dj++) for (int di = -1; di <= 1; di++) {
                    int a = i + di, c = j + dj; if (a < 0 || c < 0 || a >= nx || c >= ny) continue;
                    if (Z2[(size_t)c * nx + a] > b) b = Z2[(size_t)c * nx + a];
                }
                Z[k] = b;
            }
        }
        free(Z2);
    }
    {   /* the block: z = Z on top, flat bottom, straight sides */
        size_t nn = (size_t)nx * (size_t)ny, nt = 0, ntmax = 4 * nn + 4 * (size_t)(nx + ny);
        V = (double *)malloc(sizeof(double) * 3 * 2 * nn); T = (uint64_t *)malloc(sizeof(uint64_t) * 3 * ntmax);
        for (int j = 0; j < ny; j++) for (int i = 0; i < nx; i++) {
            size_t k = (size_t)j * nx + i; double z = Z[k] < -1e29 ? zb + 0.05 : sm_Z(xf, Z[k]);
            double X = M * (x0 + i * g) + bx, Y = M * (y0 + j * g) + by;
            if (z < zb + 0.05) z = zb + 0.05;
            if (z > ztop) ztop = z;
            V[3 * k] = X; V[3 * k + 1] = Y; V[3 * k + 2] = z;
            V[3 * (nn + k)] = X; V[3 * (nn + k) + 1] = Y; V[3 * (nn + k) + 2] = zb;
        }
#define TRI(a_, b_, c_) do { T[3 * nt] = (a_); T[3 * nt + 1] = (b_); T[3 * nt + 2] = (c_); nt++; } while (0)
        for (int j = 0; j + 1 < ny; j++) for (int i = 0; i + 1 < nx; i++) {
            uint64_t a = (uint64_t)j * nx + i, b = a + 1, c = a + 1 + nx, d = a + nx;
            if (fabs(V[3 * a + 2] - V[3 * c + 2]) <= fabs(V[3 * b + 2] - V[3 * d + 2])) { TRI(a, b, c); TRI(a, c, d); TRI(nn + a, nn + c, nn + b); TRI(nn + a, nn + d, nn + c); }
            else { TRI(a, b, d); TRI(b, c, d); TRI(nn + a, nn + d, nn + b); TRI(nn + b, nn + d, nn + c); }
        }
        {   /* the sides: the boundary walked counter-clockwise seen from above */
            int L = 2 * (nx - 1) + 2 * (ny - 1), m = 0; uint64_t *loop = (uint64_t *)malloc(sizeof(uint64_t) * (size_t)L);
            for (int i = 0; i < nx - 1; i++) loop[m++] = (uint64_t)i;
            for (int j = 0; j < ny - 1; j++) loop[m++] = (uint64_t)j * nx + (nx - 1);
            for (int i = nx - 1; i > 0; i--) loop[m++] = (uint64_t)(ny - 1) * nx + i;
            for (int j = ny - 1; j > 0; j--) loop[m++] = (uint64_t)j * nx;
            for (int s = 0; s < L; s++) { uint64_t P = loop[s], Q = loop[(s + 1) % L]; TRI(nn + P, nn + Q, Q); TRI(nn + P, Q, P); }
            free(loop);
        }
#undef TRI
        if (!(blk = sm_solid(V, (int)(2 * nn), T, (int)nt))) { sm_fail(err, errcap, "the height block is not a solid"); goto done; }
    }
    fs = shp_scale(foot, M, M, SHP_ORIGIN_POINT, 0.0, 0.0); fm = fs ? shp_translate(fs, bx, by) : NULL;
    if (!fm) { sm_fail(err, errcap, "GEOS: the footprint"); goto done; }
    fk = rim ? shp_intersection(fm, rim) : shp_clone(fm);
    if (!fk) { sm_fail(err, errcap, "GEOS: the footprint inside the rim"); goto done; }
    if (shp_is_empty(fk) || shp_area(fk) < 0.005) goto done;          /* nothing left inside the rim: not an error */
    if (!(pp = sm_polys(fk))) goto done;
    pr = manifold_extrude(manifold_alloc_manifold(), pp, ztop - zb + 1.5, 0, 0.0, 1.0, 1.0);
    pr2 = manifold_translate(manifold_alloc_manifold(), pr, 0.0, 0.0, zb - 0.3);
    res = manifold_intersection(manifold_alloc_manifold(), blk, pr2);
    if (manifold_status(res) != MANIFOLD_NO_ERROR) { snprintf(err, (size_t)errcap, "the cut failed (status %d)", (int)manifold_status(res)); manifold_delete_manifold(res); res = NULL; goto done; }
    if (manifold_is_empty(res)) { manifold_delete_manifold(res); res = NULL; }
done:
    free(V); free(T); shp_free(fs); shp_free(fm); shp_free(fk);
    if (pp) manifold_delete_polygons(pp);
    if (pr) manifold_delete_manifold(pr);
    if (pr2) manifold_delete_manifold(pr2);
    if (blk) manifold_delete_manifold(blk);
    return res;
}
/* a custom solid (a model): its top above a fine grid (so_hraster), the surface cut straight down at its own footprint
   (sect(0)). zbot: layout height */
static int sm_raster(const Solid *so, const FtS3d *e, double zbot, SmMap *q, char *err, int errcap) {
    double fb[4], g, x0, y0, *H; int nx, ny, rc = -1; GEOSGeometry *foot = NULL; ManifoldManifold *res;
    if (sol_sect(so, 0.0, &foot) != SO_OK || !foot || shp_is_empty(foot)) { shp_free(foot); return 0; }          /* nothing on the ground: nothing to make */
    shp_bounds(foot, fb);
    g = sm_shell_grid_mm * 0.625 / q->M;          /* finer than the exact shells' curves: a model's sides are steep */
    { double w = fb[2] - fb[0], h = fb[3] - fb[1]; while ((w / g + 5) * (h / g + 5) > 360000.0) g *= 1.25; nx = (int)ceil(w / g) + 5; ny = (int)ceil(h / g) + 5; x0 = fb[0] - 2 * g; y0 = fb[1] - 2 * g; }
    H = (double *)malloc(sizeof(double) * (size_t)nx * (size_t)ny);
    for (size_t i = 0; i < (size_t)nx * (size_t)ny; i++) H[i] = -1e30;
    if (so_hraster(so, x0, y0, g, nx, ny, H) != SO_OK) { sm_fail(err, errcap, "a height raster failed"); goto done; }
    for (size_t i = 0; i < (size_t)nx * (size_t)ny; i++) if (H[i] > -1e29) H[i] = e->zb + (H[i] < -0.05 ? -0.05 : H[i]) * e->zs;
    res = sm_grid_cut(H, nx, ny, x0, y0, g, zbot, foot, q->M, q->bx, q->by, q->xf, NULL, err, errcap);
    if (!res && err[0]) goto done;
    if (res) { manifold_manifold_vec_push_back(q->vec, res); manifold_delete_manifold(res); }
    rc = 0;
done:
    free(H); shp_free(foot);
    return rc;
}

int sm_shell_build(const FtS3d *e, const SoSolids *sol, const GEOSGeometry *foot, const GEOSGeometry *region, const SmShellXf *xf,
                   int has_oval, double A, double B, double rlim, MgMesh *out, char *err, int errcap) {
    double fb[4], k = 1.0, ox = 0, oy = 0, zbot; int rc = -1; SmMap q; ManifoldManifold *u = NULL;
    memset(out, 0, sizeof *out); err[0] = 0; memset(&q, 0, sizeof q);
    if (!foot || shp_is_empty(foot)) { sm_fail(err, errcap, "no footprint"); return -1; }
    shp_bounds(foot, fb);
    if (!(fb[2] > fb[0]) || !(fb[3] > fb[1])) { sm_fail(err, errcap, "a flat footprint"); return -1; }
    if (e->hasbb) {          /* the marker as laid: scaled about some point (features scale_pieces), the same map for the shell */
        double kx = (e->bb[2] - e->bb[0]) / (fb[2] - fb[0]), ky = (e->bb[3] - e->bb[1]) / (fb[3] - fb[1]);
        k = (fb[2] - fb[0]) >= (fb[3] - fb[1]) ? kx : ky;
        if (!(k > 0) || fabs(kx - ky) > 0.02 * (kx > ky ? kx : ky) + 1e-9) { sm_fail(err, errcap, "the marker was not only scaled"); return -1; }
        ox = e->bb[0] - k * fb[0]; oy = e->bb[1] - k * fb[1];
    }
    /* layout point p (the solids' own units) -> finished base M * p + (bx, by); heights by sm_Z */
    q.M = k * (xf->on ? xf->f : 1.0); q.bx = xf->on ? ox * xf->f + xf->tx : ox; q.by = xf->on ? oy * xf->f + xf->ty : oy; q.xf = xf;
    q.vec = manifold_manifold_vec(manifold_alloc_manifold_vec(), 0);
    zbot = (e->zlo < e->zb ? e->zlo : e->zb) - 0.4;          /* under the lowest ground below it */
    for (int i = 0; i < sol->n; i++) {
        const Solid *so = sol->v[i];
        if (so_envelope(so, e->zb, e->zs, zbot, 0.06 / q.M, sm_take, &q) == SO_OK) continue;
        if (sm_raster(so, e, zbot, &q, err, errcap) != 0) goto done;
    }
    if (!manifold_manifold_vec_length(q.vec)) { sm_fail(err, errcap, q.bad ? "the pieces were not solids" : "nothing to make"); goto done; }
    u = manifold_batch_boolean(manifold_alloc_manifold(), q.vec, MANIFOLD_ADD);
    if (manifold_status(u) != MANIFOLD_NO_ERROR) { snprintf(err, (size_t)errcap, "the join failed (status %d)", (int)manifold_status(u)); goto done; }
    {   /* cut straight down to what the layout kept of the marker (its ghost ops, laid out, on the finished base) and to
           the rim */
        ManifoldBox *bb = manifold_bounding_box(manifold_alloc_box(), u); ManifoldVec3 lo = manifold_box_min(bb), hi = manifold_box_max(bb);
        GEOSGeometry *d = has_oval == 2 ? NULL : shp_point_buffer(0, 0, has_oval ? 1.0 : rlim, 32), *rim = has_oval == 2 ? mg_rrect_poly(A, B, 0.97) : has_oval && d ? shp_scale(d, A / 2.0 - 0.97, B / 2.0 - 0.97, SHP_ORIGIN_POINT, 0, 0) : d;          /* 2: a square or rectangle base */
        GEOSGeometry *r1 = NULL, *r2 = NULL, *keep = NULL; ManifoldPolygons *pp = NULL; ManifoldManifold *pr, *pr2, *cut;
        manifold_delete_box(bb);
        if (has_oval) shp_free(d);          /* NULL for a rectangle: fine */
        if (region && !shp_is_empty(region)) {
            r1 = xf->on ? shp_scale(region, xf->f, xf->f, SHP_ORIGIN_POINT, 0.0, 0.0) : shp_clone(region);
            r2 = r1 && xf->on ? shp_translate(r1, xf->tx, xf->ty) : (r1 ? shp_clone(r1) : NULL);
            keep = r2 && rim ? shp_intersection(r2, rim) : NULL;
        } else if (rim) keep = shp_clone(rim);
        shp_free(rim); shp_free(r1); shp_free(r2);
        if (!keep) { sm_fail(err, errcap, "GEOS: the kept outline"); goto done; }
        if (shp_is_empty(keep) || shp_area(keep) < 0.01) { shp_free(keep); err[0] = 0; rc = 1; goto done; }          /* off the base (an oval's ends, past the rim): nothing to make, as the steps would show nothing */
        pp = sm_polys(keep); shp_free(keep);
        if (!pp) { sm_fail(err, errcap, "the kept outline has no rings"); goto done; }
        pr = manifold_extrude(manifold_alloc_manifold(), pp, hi.z - lo.z + 2.0, 0, 0.0, 1.0, 1.0); manifold_delete_polygons(pp);
        pr2 = manifold_translate(manifold_alloc_manifold(), pr, 0.0, 0.0, lo.z - 1.0); manifold_delete_manifold(pr);
        cut = manifold_intersection(manifold_alloc_manifold(), u, pr2); manifold_delete_manifold(pr2);
        manifold_delete_manifold(u); u = cut;
        if (manifold_status(u) != MANIFOLD_NO_ERROR) { sm_fail(err, errcap, "the cut to the kept outline failed"); goto done; }
        if (manifold_is_empty(u)) { err[0] = 0; rc = 1; goto done; }
    }
    {
        ManifoldMeshGL64 *o = manifold_get_meshgl64(manifold_alloc_meshgl64(), u);
        size_t ov = manifold_meshgl64_num_vert(o), ot = manifold_meshgl64_num_tri(o), np = manifold_meshgl64_num_prop(o);
        if (!ot) { manifold_delete_meshgl64(o); sm_fail(err, errcap, "nothing left"); goto done; }
        double *pv = manifold_meshgl64_vert_properties(malloc(sizeof(double) * ov * np), o);
        uint64_t *tv = manifold_meshgl64_tri_verts(malloc(sizeof(uint64_t) * 3 * ot), o);
        manifold_delete_meshgl64(o);
        out->nv = (int)ov; out->nt = (int)ot; out->v = (double *)malloc(sizeof(double) * 3 * ov); out->f = (int *)malloc(sizeof(int) * 3 * ot);
        for (size_t i = 0; i < ov; i++) memcpy(out->v + 3 * i, pv + np * i, 24);
        for (size_t i = 0; i < 3 * ot; i++) out->f[i] = (int)tv[i];
        free(pv); free(tv);
    }
    rc = 0;
done:
    if (u) manifold_delete_manifold(u);
    manifold_delete_manifold_vec(q.vec);
    return rc;
}

void sm_shell_add_all(const FtS3dList *l, const int64_t *alive, const GEOSGeometry *const *region, int nalive, const SmShellXf *xf,
                      int has_oval, double A, double B, double rlim, S3Shells *out, MgStrs *skipped) {
    for (int i = 0; i < l->n; i++) {
        int64_t cid = l->v[i].c; int live = -1; const SoSolids *sol; const GEOSGeometry *foot; MgMesh m; char err[200], line[300];
        for (int k = 0; k < nalive && live < 0; k++) if (alive[k] == cid) live = k;
        if (live < 0 || !so_smooth3d(cid, &sol, &foot)) continue;
        int rc = sm_shell_build(&l->v[i], sol, foot, region ? region[live] : NULL, xf, has_oval, A, B, rlim, &m, err, sizeof err);
        if (rc == 0) {
            if (out->n == out->cap) { out->cap = out->cap ? 2 * out->cap : 8; out->v = (MgMesh *)realloc(out->v, sizeof(MgMesh) * (size_t)out->cap); }
            out->v[out->n++] = m;
        } else if (rc < 0) {
            snprintf(line, sizeof line, "smooth %lld: %s", (long long)cid, err); mg_strs_push(skipped, line);
            snprintf(line, sizeof line, "smooth shell %lld dropped", (long long)cid); mg_log_caught(line, "RuntimeError", err, "cport/smooth3d.c");
        }
    }
}
