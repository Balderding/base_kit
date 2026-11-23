/* mesh.c: mesher.mesh_from_field (mesher.py lines 415-678) in C, routine for routine, for bit-identical output.
   Every Python step is named in a comment; dict and set orders are kept where the output depends on them
   (OMap keeps insertion order like a dict). Numbers: pyx.h. Outlines: GEOS from shapely's own dll. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "mesh.h"
#include "pyx.h"
#include "omap.h"
#include "mt.h"

#define KG 1e-6
#define GRID 1e-4
#include <windows.h>
#undef near
#undef far
static double t_now(void) { LARGE_INTEGER f, c; QueryPerformanceFrequency(&f); QueryPerformanceCounter(&c); return (double)c.QuadPart / (double)f.QuadPart; }
static int t_on = -1; static double t_last;
static void T_(const char *what) { if (t_on < 0) t_on = getenv("MESH_TIMES") != NULL; if (!t_on) return; double t = t_now(); if (what) fprintf(stderr, "T %-12s %.3f s\n", what, t - t_last); t_last = t; }

/* ------------------------------------------------------------------------------------------------ small helpers */
typedef struct { double *a; int n, cap; } DV;          /* growable doubles */
typedef struct { int *a; int n, cap; } IV;
static void dv_push(DV *v, double x) { if (v->n == v->cap) { v->cap = v->cap ? v->cap * 2 : 4; v->a = realloc(v->a, sizeof(double) * v->cap); } v->a[v->n++] = x; }
static void iv_push(IV *v, int x) { if (v->n == v->cap) { v->cap = v->cap ? v->cap * 2 : 4; v->a = realloc(v->a, sizeof(int) * v->cap); } v->a[v->n++] = x; }
static int dv_has(const DV *v, double x) { for (int i = 0; i < v->n; i++) if (v->a[i] == x) return 1; return 0; }
static void dv_add(DV *v, double x) { if (!dv_has(v, x)) dv_push(v, x); }          /* a Python set of floats */
static int cmp_d(const void *a, const void *b) { double x = *(const double *)a, y = *(const double *)b; return x < y ? -1 : x > y; }

typedef struct { double *xy; int n; } Ring;            /* coords, closed as in Python (last == first) */
typedef struct { Ring *r; int n; } Rings;              /* one face: outline then holes */
typedef struct { Rings *a; int n, cap; } RingsV;       /* rings_of */

static void ring_push(Ring *r, double x, double y, int *cap) { if (r->n == *cap) { *cap = *cap ? *cap * 2 : 16; r->xy = realloc(r->xy, sizeof(double) * 2 * *cap); } r->xy[2 * r->n] = x; r->xy[2 * r->n + 1] = y; r->n++; }
static Ring ring_copy(Ring s) { Ring r = { malloc(sizeof(double) * 2 * (s.n ? s.n : 1)), s.n }; memcpy(r.xy, s.xy, sizeof(double) * 2 * s.n); return r; }
static Rings rings_copy(Rings s) { Rings r = { malloc(sizeof(Ring) * (s.n ? s.n : 1)), s.n }; for (int i = 0; i < s.n; i++) r.r[i] = ring_copy(s.r[i]); return r; }
static void rings_free(Rings *s) { for (int i = 0; i < s->n; i++) free(s->r[i].xy); free(s->r); s->r = NULL; s->n = 0; }
static void rv_push(RingsV *v, Rings x) { if (v->n == v->cap) { v->cap = v->cap ? v->cap * 2 : 64; v->a = realloc(v->a, sizeof(Rings) * v->cap); } v->a[v->n++] = x; }
static RingsV rv_copy(const RingsV *s) { RingsV r = { malloc(sizeof(Rings) * (s->n ? s->n : 1)), s->n, s->n }; for (int i = 0; i < s->n; i++) r.a[i] = rings_copy(s->a[i]); return r; }
static void rv_free(RingsV *v) { for (int i = 0; i < v->n; i++) rings_free(&v->a[i]); free(v->a); v->a = NULL; v->n = v->cap = 0; }

/* coords of a GEOS ring or line */
static Ring ring_of_c(GEOSContextHandle_t cx, const GEOSGeometry *g) {
    const GEOSCoordSequence *cs = G.GEOSGeom_getCoordSeq_r(cx, g); unsigned int n = 0; G.GEOSCoordSeq_getSize_r(cx, cs, &n);
    Ring r = { malloc(sizeof(double) * 2 * (n ? n : 1)), (int)n };
    for (unsigned int i = 0; i < n; i++) G.GEOSCoordSeq_getXY_r(cx, cs, i, &r.xy[2 * i], &r.xy[2 * i + 1]);
    return r;
}
static Ring ring_of(const GEOSGeometry *g) { return ring_of_c(gctx, g); }
static Rings rings_of_poly(const GEOSGeometry *f) {          /* [list(f.exterior.coords)] + [list(i.coords) for i in f.interiors] */
    int nh = G.GEOSGetNumInteriorRings_r(gctx, f); Rings r = { malloc(sizeof(Ring) * (nh + 1)), nh + 1 };
    r.r[0] = ring_of(G.GEOSGetExteriorRing_r(gctx, f));
    for (int i = 0; i < nh; i++) r.r[i + 1] = ring_of(G.GEOSGetInteriorRingN_r(gctx, f, i));
    return r;
}
static double area_of(const GEOSGeometry *g) { double a = 0; G.GEOSArea_r(gctx, g, &a); return a; }

/* shapely.LinearRing(coords): closed if it is not, at least 4 coords (else shapely raises ValueError) */
static GEOSGeometry *mk_ring_c(GEOSContextHandle_t cx, const double *xy, int n, int *err) {
    int close = n > 0 && (xy[0] != xy[2 * (n - 1)] || xy[1] != xy[2 * (n - 1) + 1]); int m = n + close;
    if (m < 4) { *err = 1; return NULL; }
    double *b = malloc(sizeof(double) * 2 * m); memcpy(b, xy, sizeof(double) * 2 * n); if (close) { b[2 * n] = xy[0]; b[2 * n + 1] = xy[1]; }
    GEOSCoordSequence *cs = G.GEOSCoordSeq_copyFromBuffer_r(cx, b, (unsigned)m, 0, 0); free(b);
    return G.GEOSGeom_createLinearRing_r(cx, cs);
}
static GEOSGeometry *mk_ring(const double *xy, int n, int *err) { return mk_ring_c(gctx, xy, n, err); }
/* shapely.Polygon(shell, holes) */
static GEOSGeometry *mk_poly_c(GEOSContextHandle_t cx, const Ring *shell, const Ring *holes, int nh, int *err) {
    GEOSGeometry *s = mk_ring_c(cx, shell->xy, shell->n, err); if (!s) return NULL;
    GEOSGeometry **hs = malloc(sizeof(GEOSGeometry *) * (nh ? nh : 1));
    for (int i = 0; i < nh; i++) { hs[i] = mk_ring_c(cx, holes[i].xy, holes[i].n, err); if (!hs[i]) { G.GEOSGeom_destroy_r(cx, s); for (int j = 0; j < i; j++) G.GEOSGeom_destroy_r(cx, hs[j]); free(hs); return NULL; } }
    GEOSGeometry *p = G.GEOSGeom_createPolygon_r(cx, s, hs, (unsigned)nh); free(hs); return p;
}
static GEOSGeometry *mk_poly(const Ring *shell, const Ring *holes, int nh, int *err) { return mk_poly_c(gctx, shell, holes, nh, err);
}

/* _earclip(pts): pts without the closing coord */
static void earclip(const double *pts0, int n, DV *out) {
    double *pts = malloc(sizeof(double) * 2 * (n ? n : 1)); memcpy(pts, pts0, sizeof(double) * 2 * n);
    double sum = 0.0;          /* sum(...) starts from the int 0: adding floats to it gives the same as from 0.0 */
    for (int i = 0; i < n; i++) { int j = (i + 1) % n; sum += pts[2 * i] * pts[2 * j + 1] - pts[2 * j] * pts[2 * i + 1]; }
    if (sum < 0) for (int i = 0; i < n / 2; i++) { double tx = pts[2 * i], ty = pts[2 * i + 1]; pts[2 * i] = pts[2 * (n - 1 - i)]; pts[2 * i + 1] = pts[2 * (n - 1 - i) + 1]; pts[2 * (n - 1 - i)] = tx; pts[2 * (n - 1 - i) + 1] = ty; }
    int *idx = malloc(sizeof(int) * (n ? n : 1)), m = n; for (int i = 0; i < n; i++) idx[i] = i;
#define CR(o, a, b) ((pts[2*(a)] - pts[2*(o)]) * (pts[2*(b)+1] - pts[2*(o)+1]) - (pts[2*(a)+1] - pts[2*(o)+1]) * (pts[2*(b)] - pts[2*(o)]))
#define PUT3(i0, i1, i2) do { dv_push(out, pts[2*(i0)]); dv_push(out, pts[2*(i0)+1]); dv_push(out, pts[2*(i1)]); dv_push(out, pts[2*(i1)+1]); dv_push(out, pts[2*(i2)]); dv_push(out, pts[2*(i2)+1]); } while (0)
    int guard = 0;
    while (m > 3 && guard < 20000) {
        guard++; int cut = 0;
        for (int k = 0; k < m; k++) {
            int i0 = idx[(k - 1 + m) % m], i1 = idx[k], i2 = idx[(k + 1) % m];
            if (CR(i0, i1, i2) <= 0) continue;
            int inside = 0;
            for (int q = 0; q < m && !inside; q++) { int j = idx[q]; if (j != i0 && j != i1 && j != i2 && CR(i0, i1, j) >= 0 && CR(i1, i2, j) >= 0 && CR(i2, i0, j) >= 0) inside = 1; }
            if (inside) continue;
            PUT3(i0, i1, i2); memmove(idx + k, idx + k + 1, sizeof(int) * (m - k - 1)); m--; cut = 1; break;
        }
        if (!cut) { for (int i = 1; i < m - 1; i++) PUT3(idx[0], idx[i], idx[i + 1]); m = 0; }
    }
    if (m == 3) PUT3(idx[0], idx[1], idx[2]);
#undef CR
#undef PUT3
    free(idx); free(pts);
}

/* _tri2d(poly): triangles (x0,y0,x1,y1,x2,y2) into out. 0 = Python would raise (a polygon with holes GEOS could not triangulate) */
static int tri2d_c(GEOSContextHandle_t cx, const GEOSGeometry *poly, DV *out) {
    GEOSGeometry *t = G.GEOSConstrainedDelaunayTriangulation_r(cx, poly);
    if (!t) {
        if (G.GEOSGetNumInteriorRings_r(cx, poly) > 0) return 0;
        Ring e = ring_of_c(cx, G.GEOSGetExteriorRing_r(cx, poly)); earclip(e.xy, e.n - 1, out); free(e.xy); return 1;
    }
    int n = G.GEOSGetNumGeometries_r(cx, t);
    for (int i = 0; i < n; i++) {
        const GEOSGeometry *tr = G.GEOSGetGeometryN_r(cx, t, i);
        if (G.GEOSisEmpty_r(cx, tr)) continue;
        Ring e = ring_of_c(cx, G.GEOSGetExteriorRing_r(cx, tr));
        for (int j = 0; j < 3; j++) { dv_push(out, e.xy[2 * j]); dv_push(out, e.xy[2 * j + 1]); }
        free(e.xy);
    }
    G.GEOSGeom_destroy_r(cx, t); return 1;
}
static int tri2d(const GEOSGeometry *poly, DV *out) { return tri2d_c(gctx, poly, out); }

/* ------------------------------------------------------------------------------------------------ the state of one run */
typedef struct { GEOSGeometry *f; int rule; } Face;
typedef struct { DV z, c, h; } KH;          /* per corner: canon pairs z -> c (heights()), hts values */
typedef struct {
    const MeshIn *in;
    Rule *rules; int nrules, caprules;
    Face *keep; int nkeep, capkeep;
    RingsV rings;                               /* rings_of */
    OMap hc; DV hcv;                            /* _hc: (rule, kx, ky) -> height, first call wins */
    OMap kmap; KH *kh; int capkh;               /* heights(): canon and hts per corner */
    OMap forced; DV fz;                         /* forced: corner -> z */
    OMap edges; DV *ez; IV *es; int capedge;    /* edges: (a, b) -> [(z at a, z at b)], side: [+1/-1] */
    OMap V; DV verts; IV tris;                  /* vid() and tri() */
    int err; char msg[200];
} M;

static inline void key2(double x, double y, int64_t k[2]) { k[0] = (int64_t)py_rint(x / KG); k[1] = (int64_t)py_rint(y / KG); }
static inline int klt(const int64_t *a, const int64_t *b) { return a[0] < b[0] || (a[0] == b[0] && a[1] < b[1]); }

static int add_rule(M *m, Rule r) {
    if (m->nrules == m->caprules) { m->caprules = m->caprules ? m->caprules * 2 : 64; m->rules = realloc(m->rules, sizeof(Rule) * m->caprules); }
    m->rules[m->nrules] = r; return m->nrules++;
}
static void add_face(M *m, GEOSGeometry *f, int rule) {
    if (m->nkeep == m->capkeep) { m->capkeep = m->capkeep ? m->capkeep * 2 : 256; m->keep = realloc(m->keep, sizeof(Face) * m->capkeep); }
    m->keep[m->nkeep].f = f; m->keep[m->nkeep].rule = rule; m->nkeep++;
}

/* Slope.at / Band.at / Flat.at */
static double rule_at(const Rule *r, double x, double y) {
    if (r->kind == R_FLAT || r->kind == R_STEP) return r->z;
    GEOSGeometry *p = G.GEOSGeom_createPointFromXY_r(gctx, x, y); double v;
    if (r->kind == R_SLOPE) {
        double d = 0; G.GEOSDistance_r(gctx, r->ring, p, &d);
        double a = r->z0 + d * r->cot; v = py_round(a < r->zmax ? a : r->zmax, 4);          /* min(zmax, a) */
    } else {
        double d0 = 0, d1 = 0; G.GEOSDistance_r(gctx, r->ring, p, &d0); G.GEOSDistance_r(gctx, r->inner, p, &d1);
        double t1 = d0 + d1 > 1e-9 ? d0 / (d0 + d1) : 0.0, q = d0 / r->w, t2 = q < 1.0 ? q : 1.0, t = t2 > t1 ? t2 : t1;          /* max(t1, min(1.0, q)) */
        v = py_round(r->lo + (r->hi - r->lo) * t, 4);
    }
    G.GEOSGeom_destroy_r(gctx, p); return v;
}
/* rat(r, x, y, k): a rule's height at a corner, worked out once (Flat itself is never cached) */
static double rat(M *m, int ri, double x, double y, const int64_t k[2]) {
    const Rule *r = &m->rules[ri];
    if (r->kind == R_FLAT) return r->z;
    int64_t kk[3] = { ri, k[0], k[1] }; int isnew, i = om_put(&m->hc, kk, &isnew);
    if (isnew) dv_push(&m->hcv, rule_at(r, x, y));
    return m->hcv.a[i];
}
static int is_flat(const M *m, int ri) { return m->rules[ri].kind == R_FLAT || m->rules[ri].kind == R_STEP; }          /* isinstance(r, Flat) */

/* heights(rings_of): canon + hts; zat() reads them */
static void kh_reset(M *m) {
    for (int i = 0; i < m->kmap.n; i++) { free(m->kh[i].z.a); free(m->kh[i].c.a); free(m->kh[i].h.a); memset(&m->kh[i], 0, sizeof(KH)); }
    om_clear(&m->kmap);
}
static KH *kh_at(M *m, int i) {
    if (i >= m->capkh) { int nc = m->capkh ? m->capkh : 1024; while (nc <= i) nc *= 2; m->kh = realloc(m->kh, sizeof(KH) * nc); memset(m->kh + m->capkh, 0, sizeof(KH) * (nc - m->capkh)); m->capkh = nc; }
    return &m->kh[i];
}
static void heights(M *m) {
    kh_reset(m);
    OMap raw; om_init(&raw, 2); DV *rz = NULL, *rf = NULL; int cap = 0;
    for (int fi = 0; fi < m->nkeep; fi++) {
        Rings *rs = &m->rings.a[fi]; int ri = m->keep[fi].rule;
        for (int q = 0; q < rs->n; q++) for (int j = 0; j < rs->r[q].n; j++) {
            double x = rs->r[q].xy[2 * j], y = rs->r[q].xy[2 * j + 1]; int64_t k[2]; key2(x, y, k);
            if (om_find(&m->forced, k) >= 0) continue;
            double z = rat(m, ri, x, y, k); int i = om_put(&raw, k, NULL);
            if (i >= cap) { int nc = cap ? cap * 2 : 1024; while (nc <= i) nc *= 2; rz = realloc(rz, sizeof(DV) * nc); rf = realloc(rf, sizeof(DV) * nc); memset(rz + cap, 0, sizeof(DV) * (nc - cap)); memset(rf + cap, 0, sizeof(DV) * (nc - cap)); cap = nc; }
            dv_add(&rz[i], z);
            if (is_flat(m, ri)) dv_add(&rf[i], z);
        }
    }
    for (int i = 0; i < raw.n; i++) {
        int ki = om_put(&m->kmap, om_key(&raw, i), NULL); KH *h = kh_at(m, ki);
        DV zs = rz[i]; qsort(zs.a, zs.n, sizeof(double), cmp_d);
        int g0 = 0;
        while (g0 < zs.n) {          /* groups of heights a hair apart: z - last of the group < 2.5e-3 */
            int g1 = g0 + 1; while (g1 < zs.n && zs.a[g1] - zs.a[g1 - 1] < 2.5e-3) g1++;
            double c = zs.a[g0]; for (int t = g0; t < g1; t++) if (dv_has(&rf[i], zs.a[t])) { c = zs.a[t]; break; }
            for (int t = g0; t < g1; t++) { dv_push(&h->z, zs.a[t]); dv_push(&h->c, c); }
            dv_add(&h->h, c); g0 = g1;
        }
        free(rz[i].a); free(rf[i].a);
    }
    free(rz); free(rf); om_free(&raw);
    for (int i = 0; i < m->forced.n; i++) {          /* for k, z in forced.items(): hts[k] = {z} */
        int ki = om_put(&m->kmap, om_key(&m->forced, i), NULL); KH *h = kh_at(m, ki); h->h.n = 0; dv_push(&h->h, m->fz.a[i]);
    }
}
static double zat(M *m, int ri, double x, double y) {
    int64_t k[2]; key2(x, y, k); int fi = om_find(&m->forced, k);
    if (fi >= 0) return m->fz.a[fi];
    int ki = om_find(&m->kmap, k); double z = rat(m, ri, x, y, k);
    if (ki >= 0) { KH *h = &m->kh[ki]; for (int t = 0; t < h->z.n; t++) if (h->z.a[t] == z) return h->c.a[t]; }
    if (!m->err) { m->err = 1; snprintf(m->msg, sizeof m->msg, "KeyError in zat at (%.6f, %.6f)", x, y); }
    return z;
}

/* edge_table(rings_of, zat) */
static void edges_reset(M *m) { for (int i = 0; i < m->edges.n; i++) { free(m->ez[i].a); free(m->es[i].a); memset(&m->ez[i], 0, sizeof(DV)); memset(&m->es[i], 0, sizeof(IV)); } om_clear(&m->edges); }
static int edge_slot(M *m, const int64_t *a, const int64_t *b) {
    int64_t k4[4] = { a[0], a[1], b[0], b[1] }; int i = om_put(&m->edges, k4, NULL);
    if (i >= m->capedge) { int nc = m->capedge ? m->capedge * 2 : 1024; while (nc <= i) nc *= 2; m->ez = realloc(m->ez, sizeof(DV) * nc); m->es = realloc(m->es, sizeof(IV) * nc); memset(m->ez + m->capedge, 0, sizeof(DV) * (nc - m->capedge)); memset(m->es + m->capedge, 0, sizeof(IV) * (nc - m->capedge)); m->capedge = nc; }
    return i;
}
static void edge_table(M *m) {
    edges_reset(m);
    for (int fi = 0; fi < m->nkeep; fi++) {
        Rings *rs = &m->rings.a[fi]; int ri = m->keep[fi].rule;
        for (int q = 0; q < rs->n; q++) {
            Ring *cs = &rs->r[q]; double *zs = malloc(sizeof(double) * (cs->n ? cs->n : 1));
            for (int j = 0; j < cs->n; j++) zs[j] = zat(m, ri, cs->xy[2 * j], cs->xy[2 * j + 1]);
            for (int j = 0; j + 1 < cs->n; j++) {
                int64_t a[2], b[2]; key2(cs->xy[2 * j], cs->xy[2 * j + 1], a); key2(cs->xy[2 * j + 2], cs->xy[2 * j + 3], b);
                if (a[0] == b[0] && a[1] == b[1]) continue;
                if (klt(a, b)) { int e = edge_slot(m, a, b); dv_push(&m->ez[e], zs[j]); dv_push(&m->ez[e], zs[j + 1]); iv_push(&m->es[e], 1); }
                else { int e = edge_slot(m, b, a); dv_push(&m->ez[e], zs[j + 1]); dv_push(&m->ez[e], zs[j]); iv_push(&m->es[e], -1); }
            }
            free(zs);
        }
    }
}

/* settle(): heights, edges, and a corner wherever two neighbours swap which is higher along their shared edge */
static void settle(M *m) {
    for (int pass = 0; pass < 3; pass++) {
        heights(m); edge_table(m);
        OMap split; om_init(&split, 4); DV sp = { 0 };
        for (int e = 0; e < m->edges.n; e++) {
            if (m->ez[e].n != 4) continue;          /* len(lst) == 2 */
            double ap = m->ez[e].a[0], aq = m->ez[e].a[1], bp = m->ez[e].a[2], bq = m->ez[e].a[3], dp = ap - bp, dq = aq - bq;
            if (dp * dq < 0) {
                const int64_t *k4 = om_key(&m->edges, e); double u = dp / (dp - dq);
                double x = ((double)k4[0] + (double)(k4[2] - k4[0]) * u) * KG, y = ((double)k4[1] + (double)(k4[3] - k4[1]) * u) * KG;
                int64_t k[2]; key2(x, y, k);
                if ((k[0] == k4[0] && k[1] == k4[1]) || (k[0] == k4[2] && k[1] == k4[3])) continue;
                int isnew, si = om_put(&split, k4, &isnew);
                if (isnew) { dv_push(&sp, (double)k[0] * KG); dv_push(&sp, (double)k[1] * KG); } else { sp.a[2 * si] = (double)k[0] * KG; sp.a[2 * si + 1] = (double)k[1] * KG; }
                int fi = om_put(&m->forced, k, &isnew); double fzv = py_round(ap + (aq - ap) * u, 4);
                if (isnew) dv_push(&m->fz, fzv); else m->fz.a[fi] = fzv;
            }
        }
        if (!split.n) { om_free(&split); free(sp.a); break; }
        for (int fi = 0; fi < m->nkeep; fi++) {
            Rings *rs = &m->rings.a[fi];
            for (int q = 0; q < rs->n; q++) {
                Ring *cs = &rs->r[q]; Ring out = { NULL, 0 }; int cap = 0;
                if (cs->n) ring_push(&out, cs->xy[0], cs->xy[1], &cap);
                for (int j = 0; j + 1 < cs->n; j++) {
                    int64_t a[2], b[2]; key2(cs->xy[2 * j], cs->xy[2 * j + 1], a); key2(cs->xy[2 * j + 2], cs->xy[2 * j + 3], b);
                    int64_t k4[4]; if (klt(a, b)) { k4[0] = a[0]; k4[1] = a[1]; k4[2] = b[0]; k4[3] = b[1]; } else { k4[0] = b[0]; k4[1] = b[1]; k4[2] = a[0]; k4[3] = a[1]; }
                    int si = om_find(&split, k4);
                    if (si >= 0 && !(a[0] == b[0] && a[1] == b[1])) ring_push(&out, sp.a[2 * si], sp.a[2 * si + 1], &cap);
                    ring_push(&out, cs->xy[2 * j + 2], cs->xy[2 * j + 3], &cap);
                }
                free(cs->xy); *cs = out;
            }
        }
        om_free(&split); free(sp.a);
    }
}

/* pinches(): corners where more than two walls would share one upright line */
typedef struct { double z; int d; } Ev;
static int cmp_ev(const void *a, const void *b) { const Ev *x = a, *y = b; if (x->z != y->z) return x->z < y->z ? -1 : 1; return (x->d > y->d) - (x->d < y->d); }
static void pinches(M *m, OMap *out) {
    OMap at; om_init(&at, 2); DV *iv = NULL; int cap = 0;
    for (int e = 0; e < m->edges.n; e++) {
        if (m->ez[e].n != 4) continue;
        const int64_t *k4 = om_key(&m->edges, e); double ap = m->ez[e].a[0], aq = m->ez[e].a[1], bp = m->ez[e].a[2], bq = m->ez[e].a[3];
        for (int s = 0; s < 2; s++) {
            double u = s ? aq : ap, v = s ? bq : bp;
            if (fabs(u - v) > GRID / 2) {
                int i = om_put(&at, k4 + 2 * s, NULL);
                if (i >= cap) { int nc = cap ? cap * 2 : 256; while (nc <= i) nc *= 2; iv = realloc(iv, sizeof(DV) * nc); memset(iv + cap, 0, sizeof(DV) * (nc - cap)); cap = nc; }
                dv_push(&iv[i], u < v ? u : v); dv_push(&iv[i], u < v ? v : u);
            }
        }
    }
    for (int i = 0; i < at.n; i++) {
        int n = iv[i].n / 2;
        if (n >= 4) {
            Ev *ev = malloc(sizeof(Ev) * 2 * n);
            for (int j = 0; j < n; j++) { ev[2 * j].z = iv[i].a[2 * j]; ev[2 * j].d = 1; ev[2 * j + 1].z = iv[i].a[2 * j + 1]; ev[2 * j + 1].d = -1; }
            qsort(ev, 2 * n, sizeof(Ev), cmp_ev); int c = 0;
            for (int j = 0; j < 2 * n; j++) { c += ev[j].d; if (c > 2) { om_put(out, om_key(&at, i), NULL); break; } }
            free(ev);
        }
        free(iv[i].a);
    }
    free(iv); om_free(&at);
}

/* cut_corners(fix): returns 0 = done; 1 = _BadCorner (bad: the corners to leave alone); 2 = another exception */
static void cutpt(const int64_t *v, const int64_t *o, double e, double *px, double *py) {
    double d = py_hypot((double)(o[0] - v[0]), (double)(o[1] - v[1])) * KG;
    *px = py_rint(((double)v[0] * KG + (double)(o[0] - v[0]) * KG * e / d) / KG) * KG;
    *py = py_rint(((double)v[1] * KG + (double)(o[1] - v[1]) * KG * e / d) / KG) * KG;
}
typedef struct { double x, y, a; } AP;
static int cmp_ap(const void *a, const void *b) { const AP *x = a, *y = b; return x->a < y->a ? -1 : x->a > y->a; }
static int cut_corners(M *m, const OMap *fix, OMap *bad) {
    OMap nb; om_init(&nb, 2); OMap *nbs = NULL; int cap = 0;          /* nb: corner -> set of neighbour corners */
    for (int fi = 0; fi < m->nkeep; fi++) for (int q = 0; q < m->rings.a[fi].n; q++) {
        Ring *cs = &m->rings.a[fi].r[q];
        for (int j = 0; j + 1 < cs->n; j++) {
            int64_t a[2], b[2]; key2(cs->xy[2 * j], cs->xy[2 * j + 1], a); key2(cs->xy[2 * j + 2], cs->xy[2 * j + 3], b);
            for (int s = 0; s < 2; s++) {
                const int64_t *v = s ? b : a, *o = s ? a : b;
                if (om_find(fix, v) < 0) continue;
                int i = om_put(&nb, v, NULL);
                if (i >= cap) { int nc = cap ? cap * 2 : 64; while (nc <= i) nc *= 2; nbs = realloc(nbs, sizeof(OMap) * nc); for (int t = cap; t < nc; t++) om_init(&nbs[t], 2); cap = nc; }
                om_put(&nbs[i], o, NULL);
            }
        }
    }
    double *eps = malloc(sizeof(double) * (nb.n ? nb.n : 1));
    for (int i = 0; i < nb.n; i++) {
        const int64_t *v = om_key(&nb, i); double mn = INFINITY;
        for (int j = 0; j < nbs[i].n; j++) { const int64_t *o = om_key(&nbs[i], j); double d = py_hypot((double)(o[0] - v[0]), (double)(o[1] - v[1])) * KG; if (d < mn) mn = d; }
        double x = 0.3 * mn; eps[i] = x < 0.02 ? x : 0.02;          /* min(0.02, 0.3 * min(...)) */
    }
    DV *zmid = calloc(nb.n ? nb.n : 1, sizeof(DV));
    for (int fi = 0; fi < m->nkeep; fi++) for (int q = 0; q < m->rings.a[fi].n; q++) {
        Ring *cs = &m->rings.a[fi].r[q];
        for (int j = 0; j + 1 < cs->n; j++) {
            int64_t k[2]; key2(cs->xy[2 * j], cs->xy[2 * j + 1], k); int i = om_find(&nb, k);
            if (i >= 0) dv_push(&zmid[i], rat(m, m->keep[fi].rule, cs->xy[2 * j], cs->xy[2 * j + 1], k));
        }
    }
    int res = 0; RingsV nr_all = { 0 };
    for (int fi = 0; fi < m->nkeep && !res; fi++) {
        Rings *rs = &m->rings.a[fi]; Rings nr = { malloc(sizeof(Ring) * (rs->n ? rs->n : 1)), rs->n };
        for (int q = 0; q < rs->n; q++) {
            Ring *cs = &rs->r[q]; int n = cs->n - 1, cp = 0; Ring out = { NULL, 0 };
            for (int j = 0; j < n; j++) {
                int64_t k[2]; key2(cs->xy[2 * j], cs->xy[2 * j + 1], k); int i = om_find(&nb, k);
                if (i >= 0 && eps[i] > 5e-5) {
                    int jp = (j - 1 + n) % n, jn = (j + 1) % n; int64_t kp[2], kn[2]; double x, y;
                    key2(cs->xy[2 * jp], cs->xy[2 * jp + 1], kp); key2(cs->xy[2 * jn], cs->xy[2 * jn + 1], kn);
                    cutpt(k, kp, eps[i], &x, &y); ring_push(&out, x, y, &cp);
                    cutpt(k, kn, eps[i], &x, &y); ring_push(&out, x, y, &cp);
                } else ring_push(&out, cs->xy[2 * j], cs->xy[2 * j + 1], &cp);
            }
            if (out.n) ring_push(&out, out.xy[0], out.xy[1], &cp);          /* out + [out[0]] */
            nr.r[q] = out;
        }
        int bad_face = nr.r[0].n < 4;
        if (!bad_face) {
            int perr = 0; GEOSGeometry *p = mk_poly(&nr.r[0], nr.r + 1, nr.n - 1, &perr);
            if (!p) { res = 2; rings_free(&nr); break; }          /* shapely raises ValueError: a hole of under 4 coords */
            bad_face = !G.GEOSisValid_r(gctx, p); G.GEOSGeom_destroy_r(gctx, p);
        }
        if (bad_face) {          /* raise _BadCorner({key(x, y) for cs in rings for x, y in cs} & set(eps)) */
            for (int q = 0; q < rs->n; q++) for (int j = 0; j < rs->r[q].n; j++) { int64_t k[2]; key2(rs->r[q].xy[2 * j], rs->r[q].xy[2 * j + 1], k); if (om_find(&nb, k) >= 0) om_put(bad, k, NULL); }
            res = 1; rings_free(&nr); break;
        }
        rv_push(&nr_all, nr);
    }
    if (!res) {
        rv_free(&m->rings); m->rings = nr_all; nr_all.a = NULL; nr_all.n = 0;
        for (int i = 0; i < nb.n; i++) {
            if (eps[i] <= 5e-5) continue;
            const int64_t *v = om_key(&nb, i); AP *pts = malloc(sizeof(AP) * (nbs[i].n ? nbs[i].n : 1)); int np = 0;
            for (int j = 0; j < nbs[i].n; j++) {          /* the set {cutpt(v, o) for o in os_} */
                double x, y; cutpt(v, om_key(&nbs[i], j), eps[i], &x, &y); int dup = 0;
                for (int t = 0; t < np; t++) if (pts[t].x == x && pts[t].y == y) dup = 1;
                if (!dup) { pts[np].x = x; pts[np].y = y; pts[np].a = py_atan2(y - (double)v[1] * KG, x - (double)v[0] * KG); np++; }
            }
            for (int s = 1; s < np; s++) { AP t = pts[s]; int u = s - 1; while (u >= 0 && cmp_ap(&pts[u], &t) > 0) { pts[u + 1] = pts[u]; u--; } pts[u + 1] = t; }          /* sorted (stable) */
            if (np < 3) { free(pts); continue; }
            DV zs = zmid[i]; qsort(zs.a, zs.n, sizeof(double), cmp_d);
            if (!zs.n) { res = 2; free(pts); break; }          /* KeyError */
            Ring ring = { NULL, 0 }; int cp = 0;
            for (int t = 0; t < np; t++) ring_push(&ring, pts[t].x, pts[t].y, &cp);
            ring_push(&ring, pts[0].x, pts[0].y, &cp);
            int perr = 0; GEOSGeometry *tile = mk_poly(&ring, NULL, 0, &perr);
            if (!tile) { res = 2; free(ring.xy); free(pts); break; }
            Rule r = { 0 }; r.kind = R_FLAT; r.z = py_round(zs.a[zs.n / 2], 4);
            add_face(m, tile, add_rule(m, r));
            Rings one = { malloc(sizeof(Ring)), 1 }; one.r[0] = ring; rv_push(&m->rings, one);
            free(pts);
        }
    }
    rv_free(&nr_all);
    for (int i = 0; i < nb.n; i++) { om_free(&nbs[i]); free(zmid[i].a); }
    for (int i = nb.n; i < cap; i++) om_free(&nbs[i]);
    free(nbs); free(zmid); free(eps); om_free(&nb);
    return res;
}

/* ------------------------------------------------------------------------------------------------ vid, tri */
static int vid(M *m, double x, double y, double z) {
    int64_t k[3] = { (int64_t)py_rint(x / 1e-6), (int64_t)py_rint(y / 1e-6), (int64_t)py_rint(z / GRID) }; int isnew, i = om_put(&m->V, k, &isnew);
    if (isnew) { dv_push(&m->verts, (double)k[0] * 1e-6); dv_push(&m->verts, (double)k[1] * 1e-6); dv_push(&m->verts, (double)k[2] * GRID); }
    return i;
}
static void tri(M *m, int a, int b, int c) { if (a != b && b != c && a != c) { iv_push(&m->tris, a); iv_push(&m->tris, b); iv_push(&m->tris, c); } }
static void cross3(const double *A, const double *B, const double *C, double n[3]) {          /* np.cross(B - A, C - A) */
    double a0 = B[0] - A[0], a1 = B[1] - A[1], a2 = B[2] - A[2], b0 = C[0] - A[0], b1 = C[1] - A[1], b2 = C[2] - A[2];
    n[0] = a1 * b2 - a2 * b1; n[1] = a2 * b0 - a0 * b2; n[2] = a0 * b1 - a1 * b0;
}

/* STRtree helpers: items are cell index + 1 */
typedef struct { int *a; int n, cap; } Hits;
static void q_cb(void *item, void *u) { Hits *h = u; if (h->n == h->cap) { h->cap = h->cap ? h->cap * 2 : 8; h->a = realloc(h->a, sizeof(int) * h->cap); } h->a[h->n++] = (int)(intptr_t)item - 1; }
typedef struct { GEOSGeometry **cells; const GEOSGeometry *q; } NearU;
#define NEAR_Q ((void *)(intptr_t)-7)
static int near_cb(const void *i1, const void *i2, double *d, void *u) {
    NearU *n = u; const GEOSGeometry *g1 = i1 == NEAR_Q ? n->q : n->cells[(int)(intptr_t)i1 - 1], *g2 = i2 == NEAR_Q ? n->q : n->cells[(int)(intptr_t)i2 - 1];
    return G.GEOSDistance_r(gctx, g1, g2, d);
}

/* ------------------------------------------------------------------------------------------------ threads
   Only with KIT_MESH_THREADS > 1 (mt.h). Not bit-identical to Python: the same surface, noded tile by tile.
   Step 1 (one network of all cell outlines): every cell lies inside one 12 mm ground tile (Field.tile; later steps
   only split cells), so the outlines are joined per tile, on all threads. Lines well inside their tile (MT_MARGIN from
   its square and from the rim) can meet nothing of another tile and go straight through; the rest, with the rim
   ring, is joined once more, together. */
#define MT_MARGIN 5e-4
typedef struct {
    const Rings *cr; int nc;                  /* every cell's rings */
    const int *tile_of; int ntile; const int *tcells, *tstart;          /* cells of tile k: tcells[tstart[k] .. tstart[k+1]) */
    const int *ti, *tj; double X0, Y0, TS;    /* tile k's square */
    const Rings *gr; int ngr;                 /* the cells in no one tile (the rim ring) */
    DV *in_xy; IV *in_n; DV *gl_xy; IV *gl_n; int *bad;          /* per tile out: lines inside, lines near the border */
} LinesJob;

static GEOSGeometry *mls_c(GEOSContextHandle_t cx, const Rings *rs, const int *which, int nw) {          /* the outlines of these cells as one MultiLineString */
    int cnt = 0; for (int i = 0; i < nw; i++) cnt += rs[which ? which[i] : i].n;
    GEOSGeometry **ls = malloc(sizeof(GEOSGeometry *) * (cnt ? cnt : 1)); int k = 0;
    for (int i = 0; i < nw; i++) { const Rings *r = &rs[which ? which[i] : i]; for (int j = 0; j < r->n; j++) { GEOSCoordSequence *cs = G.GEOSCoordSeq_copyFromBuffer_r(cx, r->r[j].xy, (unsigned)r->r[j].n, 0, 0); ls[k++] = G.GEOSGeom_createLineString_r(cx, cs); } }
    GEOSGeometry *c = G.GEOSGeom_createCollection_r(cx, GEOS_MULTILINESTRING, ls, (unsigned)k); free(ls); return c;
}
static GEOSGeometry *union_grid_c(GEOSContextHandle_t cx, GEOSGeometry *c) {          /* join lines with no rounding: no corner moves, so the joins of the tiles and of the borders meet exactly (one core gives the same lines either way) */
    GEOSGeometry *u = G.GEOSUnaryUnion_r(cx, c); G.GEOSGeom_destroy_r(cx, c); return u;
}
static void lines_tile(int k, GEOSContextHandle_t cx, void *user) {
    LinesJob *J = user; int a = J->tstart[k], b = J->tstart[k + 1];
    GEOSGeometry *u = union_grid_c(cx, mls_c(cx, J->cr, J->tcells + a, b - a));
    if (!u) { J->bad[k] = 1; return; }
    double bx0 = J->X0 + J->ti[k] * J->TS + MT_MARGIN, by0 = J->Y0 + J->tj[k] * J->TS + MT_MARGIN, bx1 = bx0 + J->TS - 2 * MT_MARGIN, by1 = by0 + J->TS - 2 * MT_MARGIN;
    const GEOSPreparedGeometry *rim = NULL; GEOSGeometry *rimg = NULL;
    if (J->ngr) {          /* near the rim: lines that close to the rim ring go to the common join too */
        rimg = mls_c(cx, J->gr, NULL, J->ngr);
        double sq[10] = { bx0, by0, bx1, by0, bx1, by1, bx0, by1, bx0, by0 }; int e = 0; GEOSGeometry *box = mk_poly_c(cx, &(Ring){ sq, 5 }, NULL, 0, &e); double d = 1;
        if (box) { G.GEOSDistance_r(cx, rimg, box, &d); G.GEOSGeom_destroy_r(cx, box); }
        if (d < 2 * MT_MARGIN) rim = G.GEOSPrepare_r(cx, rimg);
    }
    int t = G.GEOSGeomTypeId_r(cx, u), np = (t == GEOS_LINESTRING) ? 1 : G.GEOSGetNumGeometries_r(cx, u);
    for (int i = 0; i < np; i++) {
        const GEOSGeometry *g = t == GEOS_LINESTRING ? u : G.GEOSGetGeometryN_r(cx, u, i);
        if (G.GEOSGeomTypeId_r(cx, g) != GEOS_LINESTRING) continue;
        Ring r = ring_of_c(cx, g); int inside = r.n >= 2;
        for (int j = 0; j < r.n && inside; j++) { double x = r.xy[2 * j], y = r.xy[2 * j + 1]; if (!(x > bx0 && x < bx1 && y > by0 && y < by1)) inside = 0; }
        if (inside && rim) for (int j = 0; j < r.n && inside; j++) { GEOSGeometry *p = G.GEOSGeom_createPointFromXY_r(cx, r.xy[2 * j], r.xy[2 * j + 1]); double d = 0; G.GEOSPreparedDistance_r(cx, rim, p, &d); G.GEOSGeom_destroy_r(cx, p); if (d < MT_MARGIN) inside = 0; }
        DV *xy = inside ? &J->in_xy[k] : &J->gl_xy[k]; IV *n = inside ? &J->in_n[k] : &J->gl_n[k];
        for (int j = 0; j < 2 * r.n; j++) dv_push(xy, r.xy[j]);
        iv_push(n, r.n); free(r.xy);
    }
    if (rim) G.GEOSPreparedGeom_destroy_r(cx, rim);
    if (rimg) G.GEOSGeom_destroy_r(cx, rimg);
    G.GEOSGeom_destroy_r(cx, u);
}
/* the joined outlines of all cells, as GEOSUnaryUnion of their boundaries gives them (noded); NULL = do it on one core */
static GEOSGeometry *mt_lines(GEOSGeometry **cells, int nc, int nthreads) {
    char tb[32]; double TS = GetEnvironmentVariableA("MESH_TILE", tb, sizeof tb) ? atof(tb) : 12.0; if (!(TS > 0)) TS = 12.0;
    Rings *cr = malloc(sizeof(Rings) * (nc ? nc : 1)); int *tile_of = malloc(sizeof(int) * (nc ? nc : 1)); double X0 = 1e300, Y0 = 1e300;
    double *cx = malloc(sizeof(double) * 2 * (nc ? nc : 1));
    for (int i = 0; i < nc; i++) {
        cr[i] = rings_of_poly(cells[i]); const Ring *s = &cr[i].r[0]; double x0 = 1e300, y0 = 1e300, x1 = -1e300, y1 = -1e300;
        for (int j = 0; j < s->n; j++) { double x = s->xy[2 * j], y = s->xy[2 * j + 1]; if (x < x0) x0 = x; if (x > x1) x1 = x; if (y < y0) y0 = y; if (y > y1) y1 = y; }
        tile_of[i] = (x1 - x0 > TS + 1e-6 || y1 - y0 > TS + 1e-6) ? -1 : 0; cx[2 * i] = (x0 + x1) / 2; cx[2 * i + 1] = (y0 + y1) / 2;
        if (tile_of[i] == 0) { if (x0 < X0) X0 = x0; if (y0 < Y0) Y0 = y0; }
    }
    /* tiles: by the middle of each cell's box, on the grid the ground tiles were cut on */
    OMap tm; om_init(&tm, 2); IV ti = { 0 }, tj = { 0 }; int ngr = 0;
    for (int i = 0; i < nc; i++) {
        if (tile_of[i] < 0) { ngr++; continue; }
        int64_t k[2] = { (int64_t)floor((cx[2 * i] - X0) / TS), (int64_t)floor((cx[2 * i + 1] - Y0) / TS) }; int isnew, s = om_put(&tm, k, &isnew);
        if (isnew) { iv_push(&ti, (int)k[0]); iv_push(&tj, (int)k[1]); }
        tile_of[i] = s;
    }
    int nt = tm.n; GEOSGeometry *res = NULL;
    if (nt >= 2) {
        int *tstart = calloc(nt + 1, sizeof(int)), *tcells = malloc(sizeof(int) * (nc ? nc : 1)), *fill = calloc(nt, sizeof(int));
        for (int i = 0; i < nc; i++) if (tile_of[i] >= 0) tstart[tile_of[i] + 1]++;
        for (int k = 0; k < nt; k++) tstart[k + 1] += tstart[k];
        for (int i = 0; i < nc; i++) if (tile_of[i] >= 0) { int k = tile_of[i]; tcells[tstart[k] + fill[k]++] = i; }
        Rings *gr = malloc(sizeof(Rings) * (ngr ? ngr : 1)); int g = 0; for (int i = 0; i < nc; i++) if (tile_of[i] < 0) gr[g++] = cr[i];
        LinesJob J = { cr, nc, tile_of, nt, tcells, tstart, ti.a, tj.a, X0, Y0, TS, gr, ngr, calloc(nt, sizeof(DV)), calloc(nt, sizeof(IV)), calloc(nt, sizeof(DV)), calloc(nt, sizeof(IV)), calloc(nt, sizeof(int)) };
        mt_run(nt, nthreads, 8, lines_tile, &J);
        int bad = 0; for (int k = 0; k < nt; k++) bad |= J.bad[k];
        if (!bad) {          /* the lines near tile borders and the rim ring: joined together, once */
            int cnt = ngr ? 0 : 0; for (int i = 0; i < ngr; i++) cnt += gr[i].n;
            for (int k = 0; k < nt; k++) cnt += J.gl_n[k].n;
            GEOSGeometry **ls = malloc(sizeof(GEOSGeometry *) * (cnt ? cnt : 1)); int q = 0;
            for (int k = 0; k < nt; k++) { int o = 0; for (int p = 0; p < J.gl_n[k].n; p++) { int n = J.gl_n[k].a[p]; ls[q++] = G.GEOSGeom_createLineString_r(gctx, G.GEOSCoordSeq_copyFromBuffer_r(gctx, J.gl_xy[k].a + 2 * o, (unsigned)n, 0, 0)); o += n; } }
            for (int i = 0; i < ngr; i++) for (int j = 0; j < gr[i].n; j++) ls[q++] = G.GEOSGeom_createLineString_r(gctx, G.GEOSCoordSeq_copyFromBuffer_r(gctx, gr[i].r[j].xy, (unsigned)gr[i].r[j].n, 0, 0));
            GEOSGeometry *gu = union_grid_c(gctx, G.GEOSGeom_createCollection_r(gctx, GEOS_MULTILINESTRING, ls, (unsigned)q)); free(ls);
            if (gu) {
                /* the border lines go in one segment each: the join may have merged two border pieces through the point
                   where an inside line ends, and the face finder links lines only at their ends */
                int gt = G.GEOSGeomTypeId_r(gctx, gu), gn = gt == GEOS_LINESTRING ? 1 : G.GEOSGetNumGeometries_r(gctx, gu), tot = 0;
                Ring *gp = malloc(sizeof(Ring) * (gn ? gn : 1));
                for (int i = 0; i < gn; i++) { const GEOSGeometry *pg = gt == GEOS_LINESTRING ? gu : G.GEOSGetGeometryN_r(gctx, gu, i); gp[i] = G.GEOSGeomTypeId_r(gctx, pg) == GEOS_LINESTRING ? ring_of(pg) : (Ring){ NULL, 0 }; if (gp[i].n > 1) tot += gp[i].n - 1; }
                for (int k = 0; k < nt; k++) tot += J.in_n[k].n;
                GEOSGeometry **all = malloc(sizeof(GEOSGeometry *) * (tot ? tot : 1)); int z = 0;
                for (int k = 0; k < nt; k++) { int o = 0; for (int p = 0; p < J.in_n[k].n; p++) { int n = J.in_n[k].a[p]; all[z++] = G.GEOSGeom_createLineString_r(gctx, G.GEOSCoordSeq_copyFromBuffer_r(gctx, J.in_xy[k].a + 2 * o, (unsigned)n, 0, 0)); o += n; } }
                for (int i = 0; i < gn; i++) { for (int j = 0; j + 1 < gp[i].n; j++) all[z++] = G.GEOSGeom_createLineString_r(gctx, G.GEOSCoordSeq_copyFromBuffer_r(gctx, gp[i].xy + 2 * j, 2, 0, 0)); free(gp[i].xy); }
                free(gp);
                res = G.GEOSGeom_createCollection_r(gctx, GEOS_MULTILINESTRING, all, (unsigned)z); free(all); G.GEOSGeom_destroy_r(gctx, gu);
            }
        }
        for (int k = 0; k < nt; k++) { free(J.in_xy[k].a); free(J.in_n[k].a); free(J.gl_xy[k].a); free(J.gl_n[k].a); }
        free(J.in_xy); free(J.in_n); free(J.gl_xy); free(J.gl_n); free(J.bad); free(gr); free(tstart); free(tcells); free(fill);
    }
    for (int i = 0; i < nc; i++) rings_free(&cr[i]);
    free(cr); free(tile_of); free(cx); free(ti.a); free(tj.a); om_free(&tm);
    return res;
}

/* the top: each face's triangles worked out on all threads (only the triangulation; heights and vertex numbers are
   then given out in face order, on one thread, as before) */
typedef struct { const RingsV *rings; DV *tl; char *state; } TopJob;          /* state: 0 done, 1 do it on one core (outline not valid), 2 error */
static void top_face(int fi, GEOSContextHandle_t cx, void *user) {
    TopJob *J = user; const Rings *rs = &J->rings->a[fi]; int perr = 0;
    GEOSGeometry *poly = mk_poly_c(cx, &rs->r[0], rs->r + 1, rs->n - 1, &perr);
    if (!poly) { J->state[fi] = 2; return; }
    if (!G.GEOSisValid_r(cx, poly)) { G.GEOSGeom_destroy_r(cx, poly); J->state[fi] = 1; return; }
    double a = 0; G.GEOSArea_r(cx, poly, &a);
    if (a > 0 && !tri2d_c(cx, poly, &J->tl[fi])) J->state[fi] = 2;
    G.GEOSGeom_destroy_r(cx, poly);
}

/* everything one run made (the input stays the caller's) */
static void m_free(M *m, GEOSGeometry **cells, int nc) {
    for (int i = 0; i < nc; i++) G.GEOSGeom_destroy_r(gctx, cells[i]);
    free(cells);
    for (int i = 0; i < m->nkeep; i++) G.GEOSGeom_destroy_r(gctx, m->keep[i].f);
    free(m->keep); rv_free(&m->rings); free(m->rules);
    om_free(&m->hc); free(m->hcv.a); kh_reset(m); om_free(&m->kmap); free(m->kh);
    om_free(&m->forced); free(m->fz.a); edges_reset(m); om_free(&m->edges); free(m->ez); free(m->es);
    om_free(&m->V); free(m->verts.a); free(m->tris.a);
}

/* ------------------------------------------------------------------------------------------------ mesh_from_field */
static int dg_swaps;          /* PLAN-next step 1: top faces swapped back (one build a process: the --core-build child) */
int mesh_from_field(const MeshIn *in, MeshOut *out) {
    M mm; M *m = &mm; memset(m, 0, sizeof mm); m->in = in; T_(NULL);
    om_init(&m->hc, 3); om_init(&m->kmap, 2); om_init(&m->forced, 2); om_init(&m->edges, 4); om_init(&m->V, 3);
    for (int i = 0; i < in->nrules; i++) add_rule(m, in->rules[i]);
    /* 1. one shared network of edges for all cells */
    int nc = 0; GEOSGeometry **cells = malloc(sizeof(GEOSGeometry *) * (in->ncells ? in->ncells : 1)); int *crule = malloc(sizeof(int) * (in->ncells ? in->ncells : 1));
    for (int i = 0; i < in->ncells; i++) {
        const GEOSGeometry *cp = in->cell[i];
        if (G.GEOSisEmpty_r(gctx, cp)) continue;
        GEOSGeometry *p = G.GEOSGeom_setPrecision_r(gctx, cp, GRID, 0);          /* P(cp) */
        if (!p || G.GEOSisEmpty_r(gctx, p) || !(area_of(p) > 1e-7)) { if (p) G.GEOSGeom_destroy_r(gctx, p); continue; }
        cells[nc] = p; crule[nc] = in->cell_rule[i]; nc++;
    }
    T_("precision");
    int nth = mt_threads(); GEOSGeometry *lines = nth ? mt_lines(cells, nc, nth) : NULL;          /* threads: joined tile by tile */
    if (!lines) {
        GEOSGeometry **bs = malloc(sizeof(GEOSGeometry *) * (nc ? nc : 1));
        for (int i = 0; i < nc; i++) bs[i] = G.GEOSBoundary_r(gctx, cells[i]);
        GEOSGeometry *coll = G.GEOSGeom_createCollection_r(gctx, GEOS_GEOMETRYCOLLECTION, bs, (unsigned)nc); free(bs);
        lines = G.GEOSUnaryUnion_r(gctx, coll); G.GEOSGeom_destroy_r(gctx, coll);
    }
    T_("union");
    if (!lines) { snprintf(out->err, sizeof out->err, "unary_union: %s", geos_err); return 0; }
    int lt = G.GEOSGeomTypeId_r(gctx, lines), nparts = (lt == GEOS_MULTILINESTRING || lt == GEOS_GEOMETRYCOLLECTION || lt == GEOS_MULTIPOLYGON || lt == GEOS_MULTIPOINT) ? G.GEOSGetNumGeometries_r(gctx, lines) : 0;
    const GEOSGeometry **obs = malloc(sizeof(GEOSGeometry *) * (nparts ? nparts : 1)); int nobs;
    if (nparts) { for (int i = 0; i < nparts; i++) obs[i] = G.GEOSGetGeometryN_r(gctx, lines, i); nobs = nparts; }
    else { obs[0] = lines; nobs = 1; }
    GEOSGeometry *pg = G.GEOSPolygonize_r(gctx, (const GEOSGeometry *const *)obs, (unsigned)nobs); free(obs);
    if (!pg) { snprintf(out->err, sizeof out->err, "polygonize: %s", geos_err); return 0; }
    T_("polygonize"); GEOSSTRtree *tree = G.GEOSSTRtree_create_r(gctx, 10);
    for (int i = 0; i < nc; i++) G.GEOSSTRtree_insert_r(gctx, tree, cells[i], (void *)(intptr_t)(i + 1));
    int nf = G.GEOSGetNumGeometries_r(gctx, pg);
    for (int fi = 0; fi < nf; fi++) {
        const GEOSGeometry *f = G.GEOSGetGeometryN_r(gctx, pg, fi);
        if (!(area_of(f) > 0)) continue;
        GEOSGeometry *pt = G.GEOSPointOnSurface_r(gctx, f); int r = -1; Hits h = { 0 };
        G.GEOSSTRtree_query_r(gctx, tree, pt, q_cb, &h);
        for (int j = 0; j < h.n; j++) if (G.GEOSContains_r(gctx, cells[h.a[j]], pt) == 1) { r = crule[h.a[j]]; break; }
        free(h.a);
        if (r < 0) {          /* a hairline gap between two cells: belongs to the nearest */
            NearU nu = { cells, pt }; const void *it = G.GEOSSTRtree_nearest_generic_r(gctx, tree, NEAR_Q, pt, near_cb, &nu);
            if (it && it != NEAR_Q) r = crule[(int)(intptr_t)it - 1];
        }
        G.GEOSGeom_destroy_r(gctx, pt);
        if (r < 0) continue;
        /* _real(f): a face that is only a line walked there and back is no face */
        Ring e = ring_of(G.GEOSGetExteriorRing_r(gctx, f)); OMap ks; om_init(&ks, 2);
        for (int j = 0; j < e.n; j++) { int64_t k[2] = { (int64_t)py_rint(e.xy[2 * j] / 1e-6), (int64_t)py_rint(e.xy[2 * j + 1] / 1e-6) }; om_put(&ks, k, NULL); }
        int real = ks.n >= 3; om_free(&ks); free(e.xy);
        if (!real) continue;
        GEOSGeometry *o = G.GEOSGeom_clone_r(gctx, f); G.GEOSOrientPolygons_r(gctx, o, 0);          /* orient(f, 1.0) */
        add_face(m, o, r);
    }
    G.GEOSSTRtree_destroy_r(gctx, tree); G.GEOSGeom_destroy_r(gctx, pg); G.GEOSGeom_destroy_r(gctx, lines);
    {   /* the sliver mend (3b, 2026-11-22; only when the kit mends a base that failed on a mesh fault, KIT_MEND_THIN=1):
           a face thinner than 0.02 mm on average (2 * area / perimeter) takes the rule of the neighbour it shares the longest
           border with. No point moves, so no new gaps; the wall between it and that neighbour goes. */
        const char *mt = getenv("KIT_MEND_THIN"), *dp = getenv("BBB_MESH_DIAG"); const char *nm = getenv("KIT_DIAG_NAME"); int nthin = 0, nmoved = 0;
        if (mt && mt[0] == '1') for (int i = 0; i < m->nkeep; i++) {
            const GEOSGeometry *f = m->keep[i].f; double A = area_of(f), P = 0; G.GEOSLength_r(gctx, f, &P);
            if (!(P > 0) || 2 * A / P >= 0.02) continue;
            nthin++;
            GEOSGeometry *bf = G.GEOSBoundary_r(gctx, f); double best = 0; int bj = -1;
            for (int j = 0; j < m->nkeep; j++) {
                if (j == i || m->keep[j].rule == m->keep[i].rule || G.GEOSIntersects_r(gctx, f, m->keep[j].f) != 1) continue;
                GEOSGeometry *bg = G.GEOSBoundary_r(gctx, m->keep[j].f), *x = bg ? G.GEOSIntersection_r(gctx, bf, bg) : NULL; double L = 0;
                if (x) G.GEOSLength_r(gctx, x, &L);
                if (L > best) { best = L; bj = j; }
                if (x) G.GEOSGeom_destroy_r(gctx, x);
                if (bg) G.GEOSGeom_destroy_r(gctx, bg);
            }
            G.GEOSGeom_destroy_r(gctx, bf);
            if (bj >= 0) {
                if (dp && *dp) { FILE *df = fopen(dp, "a"); double cx = 0, cy = 0; GEOSGeometry *c = G.GEOSGetCentroid_r(gctx, f); if (c) { G.GEOSGeomGetX_r(gctx, c, &cx); G.GEOSGeomGetY_r(gctx, c, &cy); G.GEOSGeom_destroy_r(gctx, c); }
                    if (df) { fprintf(df, "%s mend: thin face at (%.4f %.4f), %.4f mm wide, area %.6f, rule %d -> %d (border %.4f mm)" "\n", nm ? nm : "?", cx, cy, 2 * A / P, A, m->keep[i].rule, m->keep[bj].rule, best); fclose(df); } }
                m->keep[i].rule = m->keep[bj].rule; nmoved++;
            }
        }
        if (mt && mt[0] == '1' && dp && *dp) { FILE *df = fopen(dp, "a"); if (df) { fprintf(df, "%s mend: %d thin faces, %d given their neighbour's rule" "\n", nm ? nm : "?", nthin, nmoved); fclose(df); } }
    }
    for (int i = 0; i < m->nkeep; i++) rv_push(&m->rings, rings_of_poly(m->keep[i].f));
    /* 2. heights at every corner, then the corners where four walls would meet */
    T_("face rules"); settle(m); T_("settle");
    OMap pins; om_init(&pins, 2); pinches(m, &pins);
    int dg_first = pins.n, dg_tries = 0; const char *dg_exit = pins.n ? "6 tries" : "none";          /* PLAN-next step 1: how the corner loop ended */
    for (int tr = 0; tr < 6; tr++) {
        if (!pins.n) { if (tr) dg_exit = "bad corners taken out, none left"; break; }
        dg_tries = tr + 1;
        RingsV saved = rv_copy(&m->rings); int savedk = m->nkeep; OMap sf = m->forced; DV sfz = m->fz;
        sf.keys = malloc(sizeof(int64_t) * 2 * (sf.cap ? sf.cap : 1)); memcpy(sf.keys, m->forced.keys, sizeof(int64_t) * 2 * sf.n);
        sf.slot = m->forced.slot ? malloc(sizeof(int32_t) * (m->forced.smask + 1)) : NULL; if (sf.slot) memcpy(sf.slot, m->forced.slot, sizeof(int32_t) * (m->forced.smask + 1));
        sfz.a = malloc(sizeof(double) * (sfz.cap ? sfz.cap : 1)); memcpy(sfz.a, m->fz.a, sizeof(double) * sfz.n);
        OMap bad; om_init(&bad, 2);
        int res = cut_corners(m, &pins, &bad);
        if (res == 0) { settle(m); rv_free(&saved); om_free(&sf); free(sfz.a); om_free(&bad); dg_exit = "done"; break; }
        rv_free(&m->rings); m->rings = saved;
        for (int i = savedk; i < m->nkeep; i++) G.GEOSGeom_destroy_r(gctx, m->keep[i].f);
        m->nkeep = savedk; om_free(&m->forced); free(m->fz.a); m->forced = sf; m->fz = sfz;
        if (res == 1) {          /* pins = pins - e.args[0] if e.args[0] else set() */
            OMap np; om_init(&np, 2);
            if (bad.n) for (int i = 0; i < pins.n; i++) if (om_find(&bad, om_key(&pins, i)) < 0) om_put(&np, om_key(&pins, i), NULL);
            om_free(&pins); pins = np; om_free(&bad); settle(m);
        } else { om_free(&bad); settle(m); dg_exit = "exception (a ring under 4 coords / a key)"; break; }
    }
    {   const char *dp = getenv("BBB_MESH_DIAG");          /* PLAN-next step 1: the corners left as they are, measured again */
        if (dp && *dp) {
            OMap left; om_init(&left, 2); pinches(m, &left); int e4 = 0, nshort = 0; FILE *df = fopen(dp, "a"); const char *nm = getenv("KIT_DIAG_NAME");
            for (int e = 0; e < m->edges.n; e++) if (m->ez[e].n > 4) e4++;
            if (df) {
                fprintf(df, "%s mesh: pins first %d, loop %s after %d tries, pins left %d", nm ? nm : "?", dg_first, dg_exit, dg_tries, left.n);
                for (int i = 0; i < left.n && i < 40; i++) { const int64_t *k = om_key(&left, i); fprintf(df, " [%.6f %.6f]", (double)k[0] * KG, (double)k[1] * KG); }
                fprintf(df, "; 2D edges in more than two faces %d; short edges (< 1e-4 mm):", e4);
                for (int fi = 0; fi < m->nkeep; fi++) { Rings *rs = &m->rings.a[fi]; for (int q = 0; q < rs->n; q++) { Ring *cs = &rs->r[q]; for (int j = 0; j + 1 < cs->n; j++) {
                    double dx = cs->xy[2 * j + 2] - cs->xy[2 * j], dy = cs->xy[2 * j + 3] - cs->xy[2 * j + 1], L = sqrt(dx * dx + dy * dy);
                    if (L > 0 && L < 1e-4) { if (nshort < 40) fprintf(df, " [%.6f %.6f %.2g]", cs->xy[2 * j], cs->xy[2 * j + 1], L); nshort++; } } } }
                fprintf(df, " (%d)" "\n", nshort); fclose(df);
            }
            om_free(&left);
        }
    }
    om_free(&pins); T_("pins");
    /* the top: every face's triangles */
    TopJob TJ = { &m->rings, NULL, NULL };
    if (nth && m->nkeep > 1) { TJ.tl = calloc(m->nkeep, sizeof(DV)); TJ.state = calloc(m->nkeep, 1); mt_run(m->nkeep, nth, 200, top_face, &TJ); }
    for (int fi = 0; fi < m->nkeep; fi++) {
        Rings *rs = &m->rings.a[fi]; int ri = m->keep[fi].rule, perr = 0;
        DV tl = { 0 };
        if (TJ.tl && TJ.state[fi] == 0) tl = TJ.tl[fi];          /* worked out on the threads */
        else {
            if (TJ.tl) free(TJ.tl[fi].a);
            GEOSGeometry *poly = mk_poly(&rs->r[0], rs->r + 1, rs->n - 1, &perr); int own = 1;
            if (!poly) { snprintf(out->err, sizeof out->err, "ValueError: a face ring under 4 coords"); return 0; }
            if (!G.GEOSisValid_r(gctx, poly)) { G.GEOSGeom_destroy_r(gctx, poly); poly = m->keep[fi].f; own = 0; dg_swaps++; }          /* step 1: counted */
            if (area_of(poly) > 0 && !tri2d(poly, &tl)) { snprintf(out->err, sizeof out->err, "GEOSException: triangulation of a face with holes"); return 0; }
            if (own) G.GEOSGeom_destroy_r(gctx, poly);
        }
        if (!tl.n && rs->n == 1) { Ring *cs = &rs->r[0]; int n = cs->n - 1; for (int i = 1; i + 1 < n; i++) { dv_push(&tl, cs->xy[0]); dv_push(&tl, cs->xy[1]); dv_push(&tl, cs->xy[2 * i]); dv_push(&tl, cs->xy[2 * i + 1]); dv_push(&tl, cs->xy[2 * i + 2]); dv_push(&tl, cs->xy[2 * i + 3]); } }
        for (int t = 0; t + 5 < tl.n; t += 6) {
            double *p = tl.a + t, x0 = p[0], y0 = p[1], x1 = p[2], y1 = p[3], x2 = p[4], y2 = p[5];
            if ((x1 - x0) * (y2 - y0) - (x2 - x0) * (y1 - y0) < 0) { double tx = x1, ty = y1; x1 = x2; y1 = y2; x2 = tx; y2 = ty; }
            int a = vid(m, x0, y0, zat(m, ri, x0, y0)), b = vid(m, x1, y1, zat(m, ri, x1, y1)), c = vid(m, x2, y2, zat(m, ri, x2, y2)); tri(m, a, b, c);
        }
        free(tl.a);
    }
    if (TJ.tl) { free(TJ.tl); free(TJ.state); }
    T_("top tris");
    { const char *dp = getenv("BBB_MESH_DIAG"); if (dp && *dp) { FILE *df = fopen(dp, "a"); const char *nm = getenv("KIT_DIAG_NAME"); if (df) { fprintf(df, "%s mesh: top faces swapped for the original (not valid) %d" "\n", nm ? nm : "?", dg_swaps); fclose(df); } } }
    if (m->err) { snprintf(out->err, sizeof out->err, "%s", m->msg); return 0; }
    /* 3. walls between neighbours of different height */
    const MeshIn *I = in; double R0 = I->R0, H = I->H;
    for (int e = 0; e < m->edges.n; e++) {
        if (m->ez[e].n != 4) continue;
        double ap = m->ez[e].a[0], aq = m->ez[e].a[1], bp = m->ez[e].a[2], bq = m->ez[e].a[3];
        if (fabs(ap - bp) < GRID / 2 && fabs(aq - bq) < GRID / 2) continue;
        const int64_t *k4 = om_key(&m->edges, e); double pa0 = (double)k4[0] * KG, pa1 = (double)k4[1] * KG, pb0 = (double)k4[2] * KG, pb1 = (double)k4[3] * KG;
        double L = py_hypot(pb0 - pa0, pb1 - pa1);
        double lo_p = ap < bp ? ap : bp, hi_p = ap < bp ? bp : ap, lo_q = aq < bq ? aq : bq, hi_q = aq < bq ? bq : aq;
        int ia = om_find(&m->kmap, k4), ib = om_find(&m->kmap, k4 + 2);
        DV mp = { 0 }, mq = { 0 };
        if (ia >= 0) for (int t = 0; t < m->kh[ia].h.n; t++) { double z = m->kh[ia].h.a[t]; if (lo_p < z && z < hi_p) dv_push(&mp, z); }
        if (ib >= 0) for (int t = 0; t < m->kh[ib].h.n; t++) { double z = m->kh[ib].h.a[t]; if (lo_q < z && z < hi_q) dv_push(&mq, z); }
        qsort(mp.a, mp.n, sizeof(double), cmp_d); qsort(mq.a, mq.n, sizeof(double), cmp_d);
        DV ring = { 0 };
#define RP(u, z) do { dv_push(&ring, (u)); dv_push(&ring, (z)); } while (0)
        RP(0.0, lo_p); RP(L, lo_q); for (int t = 0; t < mq.n; t++) RP(L, mq.a[t]); RP(L, hi_q); RP(0.0, hi_p); for (int t = mp.n - 1; t >= 0; t--) RP(0.0, mp.a[t]);
#undef RP
        free(mp.a); free(mq.a);
        Ring ded = { NULL, 0 }; int cp = 0;
        for (int t = 0; t < ring.n; t += 2) if (!ded.n || ring.a[t] != ded.xy[2 * ded.n - 2] || ring.a[t + 1] != ded.xy[2 * ded.n - 1]) ring_push(&ded, ring.a[t], ring.a[t + 1], &cp);
        free(ring.a);
        if (ded.n > 1 && ded.xy[0] == ded.xy[2 * ded.n - 2] && ded.xy[1] == ded.xy[2 * ded.n - 1]) ded.n--;
        if (ded.n < 3) { free(ded.xy); continue; }
        int perr = 0; GEOSGeometry *wp = mk_poly(&ded, NULL, 0, &perr); free(ded.xy);
        if (!wp) continue;
        if (!G.GEOSisValid_r(gctx, wp) || area_of(wp) < 1e-12) { G.GEOSGeom_destroy_r(gctx, wp); continue; }
        int first_higher = (ap + aq) > (bp + bq), left_higher = m->es[e].a[0] == 1 ? first_higher : !first_higher;
        double rx = pb1 - pa1, ry = -(pb0 - pa0);
        DV tl = { 0 }; tri2d(wp, &tl); G.GEOSGeom_destroy_r(gctx, wp);
        for (int t = 0; t + 5 < tl.n; t += 6) {
            double P3[3][3];
            for (int c = 0; c < 3; c++) { double u = tl.a[t + 2 * c], z = tl.a[t + 2 * c + 1]; int near = u < L / 2; P3[c][0] = near ? pa0 : pb0; P3[c][1] = near ? pa1 : pb1; P3[c][2] = z; }
            double n[3]; cross3(P3[0], P3[1], P3[2], n);
            int o1 = 1, o2 = 2; if ((n[0] * rx + n[1] * ry > 0) != left_higher) { o1 = 2; o2 = 1; }
            int a = vid(m, P3[0][0], P3[0][1], P3[0][2]), b = vid(m, P3[o1][0], P3[o1][1], P3[o1][2]), c = vid(m, P3[o2][0], P3[o2][1], P3[o2][2]); tri(m, a, b, c);
        }
        free(tl.a);
    }
    T_("walls");
    /* 4. rim (12 degree cone), bottom, magnet pocket */
    int n_wall = m->tris.n / 3;
    Ring rimr = ring_of(G.GEOSGetExteriorRing_r(gctx, I->disc)); int NS = rimr.n - 1;
    double *ang = malloc(sizeof(double) * NS), *outer = malloc(sizeof(double) * 2 * NS); int *top = malloc(sizeof(int) * NS), *bot = malloc(sizeof(int) * NS);
    for (int i = 0; i < NS; i++) { double x = rimr.xy[2 * i], y = rimr.xy[2 * i + 1]; ang[i] = py_atan2(y, x); top[i] = vid(m, x, y, H); }
    if (I->has_oval) {
        for (int i = 0; i < NS; i++) {
            double x = rimr.xy[2 * i], y = rimr.xy[2 * i + 1], best = 0; int bi = 0;
            for (int t = 0; t < I->ntr; t++) { double dx = I->tr[2 * t] - x, dy = I->tr[2 * t + 1] - y, d = dx * dx + dy * dy; if (t == 0 || d < best) { best = d; bi = t; } }
            outer[2 * i] = I->ou[2 * bi]; outer[2 * i + 1] = I->ou[2 * bi + 1]; bot[i] = vid(m, outer[2 * i], outer[2 * i + 1], 0.0);
        }
    } else for (int i = 0; i < NS; i++) bot[i] = vid(m, R0 * py_cos(ang[i]), R0 * py_sin(ang[i]), 0.0);
    for (int i = 0; i < NS; i++) { int j = (i + 1) % NS; tri(m, top[i], bot[i], bot[j]); tri(m, top[i], bot[j], top[j]); }
    if (I->has_mag) {
        int MS = 72; double ma[72]; for (int i = 0; i < MS; i++) ma[i] = 2 * M_PI * i / MS;
        if (!I->has_oval) for (int i = 0; i < NS; i++) { outer[2 * i] = R0 * py_cos(ang[i]); outer[2 * i + 1] = R0 * py_sin(ang[i]); }
        Ring sh = { outer, NS }; Ring *holes = malloc(sizeof(Ring) * (I->ncens ? I->ncens : 1));
        for (int c = 0; c < I->ncens; c++) { holes[c].xy = malloc(sizeof(double) * 2 * MS); holes[c].n = MS; for (int i = 0; i < MS; i++) { holes[c].xy[2 * i] = I->cens[2 * c] + I->rm * py_cos(ma[i]); holes[c].xy[2 * i + 1] = I->cens[2 * c + 1] + I->rm * py_sin(ma[i]); } }
        int perr = 0; GEOSGeometry *bp = mk_poly(&sh, holes, I->ncens, &perr); DV tl = { 0 };
        if (!bp || !tri2d(bp, &tl)) { snprintf(out->err, sizeof out->err, "bottom: %s", geos_err); return 0; }
        G.GEOSGeom_destroy_r(gctx, bp);
        for (int t = 0; t + 5 < tl.n; t += 6) { int a = vid(m, tl.a[t], tl.a[t + 1], 0.0), b = vid(m, tl.a[t + 2], tl.a[t + 3], 0.0), c = vid(m, tl.a[t + 4], tl.a[t + 5], 0.0); tri(m, a, b, c); }
        free(tl.a);
        for (int c = 0; c < I->ncens; c++) {
            int m0[72], m1[72];
            for (int i = 0; i < MS; i++) m0[i] = vid(m, holes[c].xy[2 * i], holes[c].xy[2 * i + 1], 0.0);
            for (int i = 0; i < MS; i++) m1[i] = vid(m, holes[c].xy[2 * i], holes[c].xy[2 * i + 1], I->dm);
            int cc = vid(m, I->cens[2 * c], I->cens[2 * c + 1], I->dm);
            for (int i = 0; i < MS; i++) { int j = (i + 1) % MS; tri(m, m0[i], m1[i], m1[j]); tri(m, m0[i], m1[j], m0[j]); tri(m, m1[i], cc, m1[j]); }
            free(holes[c].xy);
        }
        free(holes);
    } else { int c = vid(m, 0.0, 0.0, 0.0); for (int i = 0; i < NS; i++) tri(m, bot[i], c, bot[(i + 1) % NS]); }
    free(rimr.xy); free(ang); free(outer); free(top); free(bot);
    /* rim faces out, bottom faces down, pocket wall faces the middle, pocket roof faces down */
    double *vv = m->verts.a; int *T = m->tris.a, nt = m->tris.n / 3;
    for (int i = n_wall; i < nt; i++) {
        int a = T[3 * i], b = T[3 * i + 1], c = T[3 * i + 2]; double *A = vv + 3 * a, *B = vv + 3 * b, *C = vv + 3 * c, n[3], g[3]; cross3(A, B, C, n);
        for (int q = 0; q < 3; q++) g[q] = (A[q] + B[q] + C[q]) / 3;
        int want, inpocket = 0;
        if (fabs(n[2]) > 5 * py_hypot(n[0], n[1])) want = n[2] < 0;
        else {
            if (I->has_mag) for (int q = 0; q < I->ncens && !inpocket; q++) if (py_hypot(g[0] - I->cens[2 * q], g[1] - I->cens[2 * q + 1]) < I->rm + 1e-6) inpocket = 1;
            if (inpocket) {
                int bq = 0; double bd = 0;
                for (int q = 0; q < I->ncens; q++) { double d = py_hypot(g[0] - I->cens[2 * q], g[1] - I->cens[2 * q + 1]); if (q == 0 || d < bd) { bd = d; bq = q; } }
                double cx = I->cens[2 * bq], cy = I->cens[2 * bq + 1]; want = n[0] * (g[0] - cx) + n[1] * (g[1] - cy) < 0;
            } else want = n[0] * g[0] + n[1] * g[1] > 0;
        }
        if (!want) { T[3 * i + 1] = c; T[3 * i + 2] = b; }
    }
    T_("rim+bottom");
    out->nv = m->verts.n / 3; out->nt = nt; out->v = m->verts.a; out->t = m->tris.a; m->verts.a = NULL; m->tris.a = NULL;
    free(crule); m_free(m, cells, nc);
    return 1;
}

void mesh_in_free(MeshIn *in) {
    for (int i = 0; i < in->ncells; i++) if (in->cell[i]) G.GEOSGeom_destroy_r(gctx, in->cell[i]);
    for (int i = 0; i < in->nrules; i++) { Rule *r = &in->rules[i]; if (r->poly) G.GEOSGeom_destroy_r(gctx, r->poly); if (r->ring) G.GEOSGeom_destroy_r(gctx, r->ring); if (r->inner) G.GEOSGeom_destroy_r(gctx, r->inner); }
    if (in->disc) G.GEOSGeom_destroy_r(gctx, in->disc);
    free(in->cell); free(in->cell_rule); free(in->rules); free(in->cens); free(in->tr); free(in->ou); memset(in, 0, sizeof *in);
}

/* ------------------------------------------------------------------------------------------------ the .min reader */
typedef struct { const unsigned char *p, *e; int bad; } Rd;
static double rd_d(Rd *r) { double v = 0; if (r->p + 8 > r->e) { r->bad = 1; return 0; } memcpy(&v, r->p, 8); r->p += 8; return v; }
static uint32_t rd_u(Rd *r) { uint32_t v = 0; if (r->p + 4 > r->e) { r->bad = 1; return 0; } memcpy(&v, r->p, 4); r->p += 4; return v; }
static int rd_b(Rd *r) { if (r->p + 1 > r->e) { r->bad = 1; return 0; } return *r->p++; }
static GEOSGeometry *rd_g(Rd *r) {
    uint32_t n = rd_u(r); if (r->bad || r->p + n > r->e) { r->bad = 1; return NULL; }
    GEOSGeometry *g = G.GEOSGeomFromWKB_buf_r(gctx, r->p, n); r->p += n;
    double pr = rd_d(r);
    if (g && pr > 0) { GEOSGeometry *q = G.GEOSGeom_setPrecision_r(gctx, g, pr, 1); if (q) { G.GEOSGeom_destroy_r(gctx, g); g = q; } }          /* the grid back, pointwise: the geometry unchanged */
    return g;
}
int mesh_read(const char *path, MeshIn *in) {
    FILE *f = fopen(path, "rb"); if (!f) return 0;
    fseek(f, 0, SEEK_END); long n = ftell(f); fseek(f, 0, SEEK_SET); unsigned char *buf = malloc(n); if (fread(buf, 1, n, f) != (size_t)n) { fclose(f); free(buf); return 0; } fclose(f);
    int ok = mesh_read_buf(buf, (size_t)n, in); free(buf); return ok;
}
int mesh_read_buf(const unsigned char *buf, size_t n, MeshIn *in) {
    Rd r = { buf, buf + n, 0 }; memset(in, 0, sizeof *in);
    if (n < 4 || memcmp(buf, "BBM1", 4)) return 0;
    r.p += 4;
    in->R0 = rd_d(&r); in->H = rd_d(&r); in->rim_tan = rd_d(&r);
    if ((in->has_mag = rd_b(&r))) { in->rm = rd_d(&r); in->dm = rd_d(&r); in->ncens = (int)rd_u(&r); in->cens = malloc(sizeof(double) * 2 * (in->ncens ? in->ncens : 1)); for (int i = 0; i < 2 * in->ncens; i++) in->cens[i] = rd_d(&r); }
    in->disc = rd_g(&r);
    if ((in->has_oval = rd_b(&r))) {
        in->ntr = (int)rd_u(&r); in->tr = malloc(sizeof(double) * 2 * (in->ntr ? in->ntr : 1)); for (int i = 0; i < 2 * in->ntr; i++) in->tr[i] = rd_d(&r);
        in->nou = (int)rd_u(&r); in->ou = malloc(sizeof(double) * 2 * (in->nou ? in->nou : 1)); for (int i = 0; i < 2 * in->nou; i++) in->ou[i] = rd_d(&r);
    }
    in->ncells = (int)rd_u(&r); in->cell = malloc(sizeof(GEOSGeometry *) * (in->ncells ? in->ncells : 1)); in->cell_rule = malloc(sizeof(int) * (in->ncells ? in->ncells : 1));
    Rule *tmp = malloc(sizeof(Rule) * (in->ncells ? in->ncells : 1));
    for (int i = 0; i < in->ncells && !r.bad; i++) {
        in->cell[i] = rd_g(&r); Rule *q = &tmp[i]; memset(q, 0, sizeof *q); q->kind = rd_b(&r);
        if (q->kind == R_FLAT) q->z = rd_d(&r);
        else if (q->kind == R_STEP) { q->z = rd_d(&r); if (rd_b(&r)) q->poly = rd_g(&r); rd_b(&r); rd_d(&r); }
        else if (q->kind == R_SLOPE) { q->poly = rd_g(&r); q->z0 = rd_d(&r); q->tan = rd_d(&r); q->zmax = rd_d(&r); q->cot = 1.0 / q->tan; q->ring = G.GEOSBoundary_r(gctx, q->poly); }
        else if (q->kind == R_BAND) { q->poly = rd_g(&r); q->inner = rd_g(&r); q->lo = rd_d(&r); q->hi = rd_d(&r); q->w = rd_d(&r); q->ring = G.GEOSBoundary_r(gctx, q->poly); }
        else r.bad = 1;
    }
    uint32_t ni = rd_u(&r); int maxr = -1; uint32_t *idx = malloc(sizeof(uint32_t) * (ni ? ni : 1));
    for (uint32_t i = 0; i < ni; i++) { idx[i] = rd_u(&r); if ((int)idx[i] > maxr) maxr = (int)idx[i]; }
    in->nrules = maxr + 1; in->rules = calloc(in->nrules ? in->nrules : 1, sizeof(Rule));
    char *got = calloc(in->nrules ? in->nrules : 1, 1);
    for (int i = 0; i < in->ncells && i < (int)ni; i++) {          /* cells sharing one rule object: the same rule (its first copy; the others go) */
        in->cell_rule[i] = (int)idx[i];
        if (!got[idx[i]]) { in->rules[idx[i]] = tmp[i]; got[idx[i]] = 1; }
        else { Rule *q = &tmp[i]; if (q->poly) G.GEOSGeom_destroy_r(gctx, q->poly); if (q->ring) G.GEOSGeom_destroy_r(gctx, q->ring); if (q->inner) G.GEOSGeom_destroy_r(gctx, q->inner); }
    }
    free(got);
    free(idx); free(tmp);
    return !r.bad;
}
