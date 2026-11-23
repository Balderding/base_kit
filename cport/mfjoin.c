/* mfjoin.c: see mfjoin.h. The base and the objects' shells (supports on) joined into one solid by Manifold
   (github.com/elalish/manifold, Apache 2.0; cport\manifold_static). */
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdint.h>
#include <math.h>
#include "mfjoin.h"
#include "manifold/manifoldc.h"

static int uf_find(int *uf, int x) { while (uf[x] != x) { uf[x] = uf[uf[x]]; x = uf[x]; } return x; }          /* the array passed down: no shared state (worker threads join at once) */
static void uf_join(int *uf, int a, int b) { a = uf_find(uf, a); b = uf_find(uf, b); if (a != b) uf[b] = a; }

/* the closed parts of a mesh (corners joined by triangles): part[v] = 0..n-1; returns n */
static int parts_of(int nv, const int *f, int nt, int *part) {
    int *uf = (int *)malloc(sizeof(int) * (size_t)(nv ? nv : 1));
    for (int i = 0; i < nv; i++) uf[i] = i;
    for (int t = 0; t < nt; t++) { uf_join(uf, f[3 * t], f[3 * t + 1]); uf_join(uf, f[3 * t], f[3 * t + 2]); }
    int *id = (int *)malloc(sizeof(int) * (size_t)(nv ? nv : 1)), n = 0;
    for (int i = 0; i < nv; i++) id[i] = -1;
    for (int i = 0; i < nv; i++) { int r = uf_find(uf, i); if (id[r] < 0) id[r] = n++; part[i] = id[r]; }
    free(id); free(uf); return n;
}
/* one part as a Manifold (its corners merged by position first); NULL if Manifold does not take it as a closed solid */
static ManifoldManifold *part_solid(const double *v, int nv, const int *f, int nt, const int *part, int p, int *ntri) {
    int *map = (int *)malloc(sizeof(int) * (size_t)nv), pv = 0, pt = 0;
    double *pp = (double *)malloc(sizeof(double) * 3 * (size_t)nv); uint64_t *tri = (uint64_t *)malloc(sizeof(uint64_t) * 3 * (size_t)(nt ? nt : 1));
    for (int i = 0; i < nv; i++) map[i] = -1;
    for (int t = 0; t < nt; t++) if (part[f[3 * t]] == p) {
        for (int c = 0; c < 3; c++) { int k = f[3 * t + c]; if (map[k] < 0) { map[k] = pv; memcpy(pp + 3 * pv, v + 3 * k, 24); pv++; } tri[3 * pt + c] = (uint64_t)map[k]; }
        pt++;
    }
    *ntri = pt;
    ManifoldMeshGL64 *mg = manifold_meshgl64(manifold_alloc_meshgl64(), pp, (size_t)pv, 3, tri, (size_t)pt);
    ManifoldMeshGL64 *mm = manifold_meshgl64_merge(manifold_alloc_meshgl64(), mg);
    ManifoldManifold *m = manifold_of_meshgl64(manifold_alloc_manifold(), mm);
    manifold_delete_meshgl64(mg); manifold_delete_meshgl64(mm); free(map); free(pp); free(tri);
    if (manifold_status(m) != MANIFOLD_NO_ERROR) { manifold_delete_manifold(m); return NULL; }
    return m;
}

/* ---- unpinch: a part where two pieces only touch along an edge or at a corner (an edge with 4 faces) is not a
   manifold for Manifold. Each such edge's faces are paired the way the solids lie (around the edge, a face with the
   one next to it that closes the same wedge of solid), then every corner is split into one vertex per fan of faces
   joined through paired edges (same position, separate vertex). v grows; f is renumbered in place. 0 = done,
   -1 = an edge that cannot be paired (left as it was). */
typedef struct { int a, b, t; } MfHe;          /* half edge a -> b of face t */
static int he_cmp(const void *x, const void *y) {
    const MfHe *p = (const MfHe *)x, *q = (const MfHe *)y; int pa = p->a < p->b ? p->a : p->b, pb = p->a < p->b ? p->b : p->a, qa = q->a < q->b ? q->a : q->b, qb = q->a < q->b ? q->b : q->a;
    return pa != qa ? (pa < qa ? -1 : 1) : pb != qb ? (pb < qb ? -1 : 1) : 0;
}
static int cf_find(int *cf, int x) { while (cf[x] != x) { cf[x] = cf[cf[x]]; x = cf[x]; } return x; }          /* the union-find over corners (3 * t + c), passed down */
static void cf_join(int *cf, int a, int b) { a = cf_find(cf, a); b = cf_find(cf, b); if (a != b) cf[b] = a; }
static int corner_of(const int *f, int t, int v) { for (int c = 0; c < 3; c++) if (f[3 * t + c] == v) return 3 * t + c; return -1; }
static int mf_unpinch(double **vp, int *nvp, int *f, int nt) {
    int ne = 3 * nt, rc = 0; MfHe *h = (MfHe *)malloc(sizeof(MfHe) * (size_t)(ne ? ne : 1)); const double *v = *vp;
    for (int t = 0; t < nt; t++) for (int c = 0; c < 3; c++) { h[3 * t + c].a = f[3 * t + c]; h[3 * t + c].b = f[3 * t + (c + 1) % 3]; h[3 * t + c].t = t; }
    qsort(h, (size_t)ne, sizeof(MfHe), he_cmp);
    int *cf = (int *)malloc(sizeof(int) * (size_t)(ne ? ne : 1)); for (int i = 0; i < ne; i++) cf[i] = i;
    for (int i = 0; i < ne && !rc;) {
        int j = i; while (j < ne && he_cmp(&h[i], &h[j]) == 0) j++;
        int n = j - i;
        if (n == 2) {          /* the usual edge: its two faces' corners at both ends are one vertex */
            if (h[i].a == h[i + 1].a) { rc = -1; break; }          /* both the same way round: not a solid */
            int a = h[i].a, b = h[i].b;
            cf_join(cf, corner_of(f, h[i].t, a), corner_of(f, h[i + 1].t, a)); cf_join(cf, corner_of(f, h[i].t, b), corner_of(f, h[i + 1].t, b));
        } else if (n % 2 == 0 && n <= 16) {          /* faces around the edge by angle; pair each with its neighbour across solid */
            int a = h[i].a < h[i].b ? h[i].a : h[i].b, b = h[i].a < h[i].b ? h[i].b : h[i].a; double e[3], ex[3], ey[3], L = 0, ang[16]; int ord[16], fw[16];
            for (int k = 0; k < 3; k++) { e[k] = v[3 * b + k] - v[3 * a + k]; L += e[k] * e[k]; }
            L = sqrt(L); if (L <= 0) { rc = -1; break; }
            for (int k = 0; k < 3; k++) e[k] /= L;
            { double tmp[3] = { fabs(e[0]) < 0.9 ? 1.0 : 0.0, fabs(e[0]) < 0.9 ? 0.0 : 1.0, 0.0 }, d = tmp[0] * e[0] + tmp[1] * e[1];
              for (int k = 0; k < 3; k++) ex[k] = tmp[k] - d * e[k];
              double l2 = sqrt(ex[0] * ex[0] + ex[1] * ex[1] + ex[2] * ex[2]); for (int k = 0; k < 3; k++) ex[k] /= l2;
              ey[0] = e[1] * ex[2] - e[2] * ex[1]; ey[1] = e[2] * ex[0] - e[0] * ex[2]; ey[2] = e[0] * ex[1] - e[1] * ex[0]; }
            for (int k = 0; k < n; k++) {
                const MfHe *q = &h[i + k]; int t = q->t, w = f[3 * t] != a && f[3 * t] != b ? f[3 * t] : f[3 * t + 1] != a && f[3 * t + 1] != b ? f[3 * t + 1] : f[3 * t + 2];
                double d[3]; for (int c = 0; c < 3; c++) d[c] = v[3 * w + c] - v[3 * a + c];
                ang[k] = atan2(d[0] * ey[0] + d[1] * ey[1] + d[2] * ey[2], d[0] * ex[0] + d[1] * ex[1] + d[2] * ex[2]);
                fw[k] = q->a == a;          /* a -> b in this face */
                ord[k] = k;
            }
            for (int x = 1; x < n; x++) for (int y = x; y > 0 && ang[ord[y]] < ang[ord[y - 1]]; y--) { int s = ord[y]; ord[y] = ord[y - 1]; ord[y - 1] = s; }
            /* a face going a -> b faces towards increasing angle, so its solid lies towards decreasing angle; one going
               b -> a the other way. A wedge of solid runs from a b -> a face to the next face by angle, which must go
               a -> b: pair those two */
            {
                int used = 0;
                for (int x = 0; x < n && !rc; x++) {
                    int p = ord[x], q = ord[(x + 1) % n];
                    if (fw[p]) continue;
                    if (!fw[q]) { rc = -1; break; }
                    cf_join(cf, corner_of(f, h[i + p].t, a), corner_of(f, h[i + q].t, a)); cf_join(cf, corner_of(f, h[i + p].t, b), corner_of(f, h[i + q].t, b));
                    used++;
                }
                if (!rc && used * 2 != n) rc = -1;
                if (rc) break;
            }
        } else { rc = -1; break; }          /* an open edge or an odd one: not for here */
        i = j;
    }
    if (!rc) {          /* one vertex per class of corners */
        int *nid = (int *)malloc(sizeof(int) * (size_t)(ne ? ne : 1)), nn = 0; double *nv;
        for (int i = 0; i < ne; i++) nid[i] = -1;
        nv = (double *)malloc(sizeof(double) * 3 * (size_t)(ne ? ne : 1));
        for (int i = 0; i < ne; i++) { int r0 = cf_find(cf, i); if (nid[r0] < 0) { nid[r0] = nn; memcpy(nv + 3 * nn, *vp + 3 * f[i], 24); nn++; } }
        for (int i = 0; i < ne; i++) f[i] = nid[cf_find(cf, i)];
        free(*vp); *vp = nv; *nvp = nn; free(nid);
    }
    free(h); free(cf);
    return rc;
}
/* one part as a Manifold, unpinched first if Manifold does not take it as it is */
static ManifoldManifold *part_solid2(const double *v, int nv, const int *f, int nt, const int *part, int p, int *ntri, int *unpinched) {
    ManifoldManifold *m = part_solid(v, nv, f, nt, part, p, ntri);
    *unpinched = 0;
    if (m) return m;
    int *pf = (int *)malloc(sizeof(int) * 3 * (size_t)(nt ? nt : 1)), pt = 0, *zero; double *pv = (double *)malloc(sizeof(double) * 3 * (size_t)(nv ? nv : 1)); int pnv = nv;
    memcpy(pv, v, sizeof(double) * 3 * (size_t)nv);
    for (int t = 0; t < nt; t++) if (part[f[3 * t]] == p) { memcpy(pf + 3 * pt, f + 3 * t, 12); pt++; }
    if (mf_unpinch(&pv, &pnv, pf, pt) == 0) {
        zero = (int *)calloc((size_t)(pnv ? pnv : 1), sizeof(int));
        m = part_solid(pv, pnv, pf, pt, zero, 0, ntri);
        free(zero);
        if (m) *unpinched = 1;
    }
    free(pf); free(pv);
    return m;
}

int mf_join(double **vp, int *nvp, int **fp, int *ntp, MfJoin *r, char *err, size_t errcap) {
    memset(r, 0, sizeof *r); err[0] = 0;
    int nv = *nvp, nt = *ntp; const double *v = *vp; const int *f = *fp;
    int *part = (int *)malloc(sizeof(int) * (size_t)(nv ? nv : 1)); int np = parts_of(nv, f, nt, part);
    r->parts_in = np;
    if (np < 2) { free(part); return 0; }          /* already one solid */
    ManifoldManifoldVec *vec = manifold_manifold_vec(manifold_alloc_manifold_vec(), 0);
    int *pcount = (int *)calloc((size_t)np, sizeof(int)), big = 0;
    for (int t = 0; t < nt; t++) pcount[part[f[3 * t]]]++;
    for (int p = 1; p < np; p++) if (pcount[p] > pcount[big]) big = p;          /* the base: the part with the most faces */
    for (int p = 0; p < np; p++) {
        int ntri, un; ManifoldManifold *m = part_solid2(v, nv, f, nt, part, p, &ntri, &un);
        if (!m && p == big) {          /* the base itself is not a solid for Manifold: no join (it must never be dropped) */
            snprintf(err, errcap, "Manifold does not take the base as a solid: not joined");
            free(part); free(pcount); manifold_delete_manifold_vec(vec); return -1;
        }
        if (!m) { r->not_closed++; continue; }          /* not a closed solid: left out (it would be a loose piece anyway) */
        r->unpinched += un;
        manifold_manifold_vec_push_back(vec, m); manifold_delete_manifold(m);
    }
    free(part); free(pcount);
    ManifoldManifold *u = manifold_batch_boolean(manifold_alloc_manifold(), vec, MANIFOLD_ADD);
    manifold_delete_manifold_vec(vec);
    if (manifold_status(u) != MANIFOLD_NO_ERROR) { snprintf(err, errcap, "Manifold could not join the parts (status %d)", (int)manifold_status(u)); manifold_delete_manifold(u); return -1; }
    ManifoldMeshGL64 *out = manifold_get_meshgl64(manifold_alloc_meshgl64(), u); manifold_delete_manifold(u);
    size_t ov = manifold_meshgl64_num_vert(out), ot = manifold_meshgl64_num_tri(out), npr = manifold_meshgl64_num_prop(out);
    double *pr = manifold_meshgl64_vert_properties(malloc(sizeof(double) * (ov * npr ? ov * npr : 1)), out);
    uint64_t *tv = manifold_meshgl64_tri_verts(malloc(sizeof(uint64_t) * (3 * ot ? 3 * ot : 1)), out);
    manifold_delete_meshgl64(out);
    double *nvv = (double *)malloc(sizeof(double) * 3 * (ov ? ov : 1)); int *nf = (int *)malloc(sizeof(int) * 3 * (ot ? ot : 1));
    for (size_t i = 0; i < ov; i++) memcpy(nvv + 3 * i, pr + npr * i, 24);
    for (size_t i = 0; i < 3 * ot; i++) nf[i] = (int)tv[i];
    free(pr); free(tv);
    /* what is still apart after the join touches nothing: a piece in the air, or a sliver. Keep the biggest part (the
       base and all that joined it), drop the rest */
    int *op = (int *)malloc(sizeof(int) * (ov ? ov : 1)); int on = parts_of((int)ov, nf, (int)ot, op);
    double *vol = (double *)calloc((size_t)(on ? on : 1), sizeof(double));
    for (size_t t = 0; t < ot; t++) {
        const double *a = nvv + 3 * nf[3 * t], *b = nvv + 3 * nf[3 * t + 1], *c = nvv + 3 * nf[3 * t + 2];
        vol[op[nf[3 * t]]] += (a[0] * (b[1] * c[2] - b[2] * c[1]) - a[1] * (b[0] * c[2] - b[2] * c[0]) + a[2] * (b[0] * c[1] - b[1] * c[0])) / 6.0;
    }
    int keep = 0; for (int p = 1; p < on; p++) if (vol[p] > vol[keep]) keep = p;
    int *map = (int *)malloc(sizeof(int) * (ov ? ov : 1)), kv = 0, kt = 0;
    for (size_t i = 0; i < ov; i++) { map[i] = -1; if (op[i] == keep) { memmove(nvv + 3 * kv, nvv + 3 * i, 24); map[i] = kv++; } }
    for (size_t t = 0; t < ot; t++) if (op[nf[3 * t]] == keep) { for (int c = 0; c < 3; c++) nf[3 * kt + c] = map[nf[3 * t + c]]; kt++; }
    for (int p = 0; p < on; p++) if (p != keep) { if (vol[p] > 0.05) r->floating++; else r->slivers++; }
    free(op); free(vol); free(map);
    free(*vp); free(*fp); *vp = nvv; *fp = nf; *nvp = kv; *ntp = kt;
    r->joined = 1; return 0;
}

/* the whole mesh as one Manifold (every face; corners merged by position); NULL with *status if Manifold refuses it */
static ManifoldManifold *whole_solid(const double *v, int nv, const int *f, int nt, int *status) {
    int *zero = (int *)calloc((size_t)(nv ? nv : 1), sizeof(int)), ntri;
    ManifoldMeshGL64 *mg; ManifoldMeshGL64 *mm; ManifoldManifold *m;
    double *pp = (double *)malloc(sizeof(double) * 3 * (size_t)(nv ? nv : 1)); uint64_t *tri = (uint64_t *)malloc(sizeof(uint64_t) * 3 * (size_t)(nt ? nt : 1));
    memcpy(pp, v, sizeof(double) * 3 * (size_t)nv); for (int i = 0; i < 3 * nt; i++) tri[i] = (uint64_t)f[i];
    mg = manifold_meshgl64(manifold_alloc_meshgl64(), pp, (size_t)nv, 3, tri, (size_t)nt);
    mm = manifold_meshgl64_merge(manifold_alloc_meshgl64(), mg);
    m = manifold_of_meshgl64(manifold_alloc_manifold(), mm);
    manifold_delete_meshgl64(mg); manifold_delete_meshgl64(mm); free(pp); free(tri); free(zero); (void)ntri;
    *status = (int)manifold_status(m);
    if (*status != MANIFOLD_NO_ERROR) { manifold_delete_manifold(m); return NULL; }
    return m;
}
/* a Manifold's mesh back as malloc'd arrays */
static void to_arrays(ManifoldManifold *m, double **vp, int *nvp, int **fp, int *ntp) {
    ManifoldMeshGL64 *out = manifold_get_meshgl64(manifold_alloc_meshgl64(), m);
    size_t ov = manifold_meshgl64_num_vert(out), ot = manifold_meshgl64_num_tri(out), npr = manifold_meshgl64_num_prop(out);
    double *pr = manifold_meshgl64_vert_properties(malloc(sizeof(double) * (ov * npr ? ov * npr : 1)), out);
    uint64_t *tv = manifold_meshgl64_tri_verts(malloc(sizeof(uint64_t) * (3 * ot ? 3 * ot : 1)), out);
    manifold_delete_meshgl64(out);
    double *nvv = (double *)malloc(sizeof(double) * 3 * (ov ? ov : 1)); int *nf = (int *)malloc(sizeof(int) * 3 * (ot ? ot : 1));
    for (size_t i = 0; i < ov; i++) memcpy(nvv + 3 * i, pr + npr * i, 24);
    for (size_t i = 0; i < 3 * ot; i++) nf[i] = (int)tv[i];
    free(pr); free(tv);
    *vp = nvv; *nvp = (int)ov; *fp = nf; *ntp = (int)ot;
}
int mf_simplify(double **vp, int *nvp, int **fp, int *ntp, double tol, char *err, size_t errcap) {
    int st; ManifoldManifold *m = whole_solid(*vp, *nvp, *fp, *ntp, &st);
    if (!m) { snprintf(err, errcap, "Manifold does not take the mesh as a solid (status %d)", st); return -1; }
    ManifoldManifold *s = manifold_simplify(manifold_alloc_manifold(), m, tol); manifold_delete_manifold(m);
    if (manifold_status(s) != MANIFOLD_NO_ERROR) { snprintf(err, errcap, "Manifold could not simplify the mesh (status %d)", (int)manifold_status(s)); manifold_delete_manifold(s); return -1; }
    double *nv2; int nvn, *nf2, ntn; to_arrays(s, &nv2, &nvn, &nf2, &ntn); manifold_delete_manifold(s);
    free(*vp); free(*fp); *vp = nv2; *nvp = nvn; *fp = nf2; *ntp = ntn; return 0;
}
int mf_check(const double *v, int nv, const int *f, int nt) {
    int st; ManifoldManifold *m = whole_solid(v, nv, f, nt, &st);
    if (m) manifold_delete_manifold(m);
    return st;
}
const char *mf_status_text(int st) {
    switch (st) {
    case MANIFOLD_NO_ERROR: return "fine";
    case MANIFOLD_NON_FINITE_VERTEX: return "a corner is not a number";
    case MANIFOLD_NOT_MANIFOLD: return "not a closed solid (an open or doubled edge)";
    case MANIFOLD_VERTEX_INDEX_OUT_OF_BOUNDS: return "a triangle points at a missing corner";
    default: return "Manifold rejected it";
    }
}
