/* scatter.c: gg3.cluster() with gg.stone() shapes, in C: the routine that makes the small rocks of a scatter detail
   (details.scatter with sh_rock / sh_round, and the fixed rock clusters of gg3 and scenes). Routine by routine from
   the Python, bit for bit:
     gg3.cluster(s, rng, n, make_one, cx, cy, spread, gap)     n shapes near (cx, cy) that keep gap from each other
     the make_one lambdas (details.sh_rock, sh_round; gg3 / scenes stone(x, y, rng.uniform(0.6, 1.0), rng, .7, 5))
     gg.stone(cx, cy, r, rng, aspect, nv, round_)              one rock outline
     gg._hull(pts)                                             Andrew's monotone chain, as the Python does it
   The random numbers come from the caller's random.Random state (624 words + index, random.getstate()), handed back
   advanced exactly as Python would leave it. GEOS is shapely's own geos_c.dll, so the hull, the simplify and the
   distances are the very same calls shapely makes. */
#include <stdlib.h>
#include <string.h>
#include "geos_dyn.h"
#include "pyx.h"
#include "pyrandom.h"
#include "scatter.h"

#define PI 3.141592653589793          /* math.pi */
typedef struct { double x, y; } P2;
static int cmp_p2(const void *a, const void *b) {          /* Python's tuple order: x, then y */
    const P2 *p = (const P2 *)a, *q = (const P2 *)b;
    if (p->x < q->x) return -1;
    if (p->x > q->x) return 1;
    return p->y < q->y ? -1 : p->y > q->y;
}
static int cmp_d(const void *a, const void *b) { double x = *(const double *)a, y = *(const double *)b; return x < y ? -1 : x > y; }

/* gg._hull: P = sorted(set(pts)); lo, up = half(P), half(reversed(P)); lo[:-1] + up[:-1]. Returns the count, -1 if < 3 */
static int hull(const P2 *pts, int n, P2 *out) {
    P2 P[SC_MAXNV], h[SC_MAXNV + 1]; int m = 0, i, k, nh = 0;
    memcpy(P, pts, sizeof(P2) * n); qsort(P, n, sizeof(P2), cmp_p2);
    for (i = 0; i < n; i++) if (!m || P[i].x != P[m - 1].x || P[i].y != P[m - 1].y) P[m++] = P[i];          /* set(): exact duplicates go */
    if (m < 3) return -1;          /* Python hands back the points and Polygon() fails: the caller redoes it in Python */
    for (int pass = 0; pass < 2; pass++) {          /* half(P), then half(reversed(P)) */
        k = 0;
        for (int t = 0; t < m; t++) {
            P2 p = pass ? P[m - 1 - t] : P[t];
            while (k >= 2 && (h[k - 1].x - h[k - 2].x) * (p.y - h[k - 2].y) - (h[k - 1].y - h[k - 2].y) * (p.x - h[k - 2].x) <= 0) k--;
            h[k++] = p;
        }
        for (i = 0; i < k - 1; i++) out[nh++] = h[i];
    }
    return nh;
}

static GEOSGeometry *poly_of(const P2 *c, int n) {          /* Polygon(coords): the ring closed as shapely closes it */
    double b[2 * 260]; GEOSCoordSequence *cs; GEOSGeometry *r;
    memcpy(b, c, sizeof(double) * 2 * n); b[2 * n] = c[0].x; b[2 * n + 1] = c[0].y;
    cs = G.GEOSCoordSeq_copyFromBuffer_r(gctx, b, (unsigned)(n + 1), 0, 0);
    if (!cs) return NULL;
    r = G.GEOSGeom_createLinearRing_r(gctx, cs);
    return r ? G.GEOSGeom_createPolygon_r(gctx, r, NULL, 0) : NULL;
}

/* ---------- the round rock's outline without GEOS: exact copies of what GEOS 3.14.1 does for this one case ----------
   convex_hull (algorithm/ConvexHull.cpp): the Graham scan's result is the hull's corners only (cleanRing drops points
   on a straight edge), clockwise, starting at the lowest point (the smallest x of the lowest). Its orientation test is
   CGAlgorithmsDD::orientationIndex: the double filter below, then double-double when the filter cannot tell; we
   hand such a rock to GEOS instead. simplify(0.04) (TopologyPreservingSimplifier, TaggedLineStringSimplifier): on one
   convex ring its topology checks never refuse (a chord of a convex ring crosses nothing, and there is no other
   component to jump), so it is Douglas-Peucker with its minimum size (4 points) and its ring-endpoint step. */
static int orient_filter(double pax, double pay, double pbx, double pby, double pcx, double pcy) {          /* 1 left, -1 right, 0 straight, 2 unsure */
    double detleft = (pax - pcx) * (pby - pcy), detright = (pay - pcy) * (pbx - pcx), det = detleft - detright;
    double error = fabs(detleft + detright) * 3.3306690621773724e-16;
    if (fabs(det) >= error) return (det > 0) - (det < 0);
    return 2;
}
static double pt_dist(P2 p, P2 a) { double dx = p.x - a.x, dy = p.y - a.y; return sqrt(dx * dx + dy * dy); }
static double pt_seg(P2 p, P2 A, P2 B) {          /* Distance::pointToSegment */
    double r, s;
    if (A.x == B.x && A.y == B.y) return pt_dist(p, A);
    r = ((p.x - A.x) * (B.x - A.x) + (p.y - A.y) * (B.y - A.y)) / ((B.x - A.x) * (B.x - A.x) + (B.y - A.y) * (B.y - A.y));
    if (r <= 0.0) return pt_dist(p, A);
    if (r >= 1.0) return pt_dist(p, B);
    s = ((A.y - p.y) * (B.x - A.x) - (A.x - p.x) * (B.y - A.y)) / ((B.x - A.x) * (B.x - A.x) + (B.y - A.y) * (B.y - A.y));
    return fabs(s) * sqrt(((B.x - A.x) * (B.x - A.x) + (B.y - A.y) * (B.y - A.y)));
}
/* the hull's corners, clockwise from the lowest point, closed (first == last). Returns the count, -1 = unsure (use GEOS) */
static int hull_cw(const P2 *in, int n, P2 *out) {
    P2 P[128], H[260]; int m = 0, k = 0, i, t, lo, nh, s0;
    memcpy(P, in, sizeof(P2) * n); qsort(P, n, sizeof(P2), cmp_p2);
    for (i = 0; i < n; i++) if (!m || P[i].x != P[m - 1].x || P[i].y != P[m - 1].y) P[m++] = P[i];
    if (m < 3) return -1;
    for (i = 0; i < m; i++) {          /* Andrew's chain, counter-clockwise, corners only */
        while (k >= 2) { int o = orient_filter(H[k - 2].x, H[k - 2].y, H[k - 1].x, H[k - 1].y, P[i].x, P[i].y); if (o == 2) return -1; if (o > 0) break; k--; }
        H[k++] = P[i];
    }
    for (i = m - 2, t = k + 1; i >= 0; i--) {
        while (k >= t) { int o = orient_filter(H[k - 2].x, H[k - 2].y, H[k - 1].x, H[k - 1].y, P[i].x, P[i].y); if (o == 2) return -1; if (o > 0) break; k--; }
        H[k++] = P[i];
    }
    nh = k - 1;          /* H[nh] == H[0] */
    if (nh < 3) return -1;
    for (lo = 0, i = 1; i < nh; i++) if (H[i].y < H[lo].y || (H[i].y == H[lo].y && H[i].x < H[lo].x)) lo = i;
    for (i = 0, s0 = lo; i < nh; i++) out[i] = H[((s0 - i) % nh + nh) % nh];          /* reversed: clockwise, from the lowest */
    out[nh] = out[0];
    return nh + 1;
}
typedef struct { const P2 *pts; int seg[260][2]; int nseg; double tol; } DP;
static void dp_section(DP *d, int i, int j, int depth) {          /* TaggedLineStringSimplifier::simplifySection */
    int valid = 1, k, far = i; double maxd = -1.0;
    depth += 1;
    if (i + 1 == j) { d->seg[d->nseg][0] = i; d->seg[d->nseg][1] = j; d->nseg++; return; }
    if ((d->nseg ? d->nseg + 1 : 0) < 4 && depth + 1 < 4) valid = 0;          /* getResultSize() < minimumSize and worst case too small */
    for (k = i + 1; k < j; k++) { double dd = pt_seg(d->pts[k], d->pts[i], d->pts[j]); if (dd > maxd) { maxd = dd; far = k; } }
    if (maxd < 0) { for (k = i; k < j; k++) { d->seg[d->nseg][0] = k; d->seg[d->nseg][1] = k + 1; d->nseg++; } return; }
    if (maxd > d->tol) valid = 0;
    if (valid) { d->seg[d->nseg][0] = i; d->seg[d->nseg][1] = j; d->nseg++; return; }          /* flatten (topology always valid here) */
    dp_section(d, i, far, depth);
    dp_section(d, far, j, depth);
}
/* simplify(tol) of a closed convex ring (n points, first == last) -> out (closed), the count */
static int simplify_ring(const P2 *ring, int n, double tol, P2 *out) {
    static DP d; int i, m = 0, p0f;
    d.pts = ring; d.nseg = 0; d.tol = tol;
    dp_section(&d, 0, n - 1, 0);
    p0f = d.seg[0][0];
    if (d.nseg + 1 > 4) {          /* simplifyRingEndpoint */
        P2 a = ring[d.seg[d.nseg - 1][0]], b = ring[d.seg[0][1]];
        if (pt_seg(ring[d.seg[0][0]], a, b) <= tol) { p0f = d.seg[d.nseg - 1][0]; d.nseg--; }
    }
    for (i = 0; i < d.nseg; i++) out[m++] = ring[i == 0 ? p0f : d.seg[i][0]];
    out[m++] = d.nseg == 1 ? ring[d.seg[0][1]] : ring[d.seg[d.nseg - 1][1]];
    if (d.nseg >= 1 && p0f != d.seg[0][0]) out[m - 1] = ring[p0f];          /* the ring still closes on its (new) first point */
    return m;
}
/* the round rock from its 120 points: 1 = done (coords in out, *no closed count), 0 = unsure (use GEOS) */
static int round_fast(const P2 *pts, P2 *out, int *no) {
    P2 h[130]; int nh = hull_cw(pts, 120, h);
    if (nh < 0) return 0;
    *no = simplify_ring(h, nh, 0.04, out);
    return *no >= 4;
}
int scatter_fast = 1;          /* 0: always GEOS (the proof compares the two) */

/* gg.stone(cx, cy, r, rng, aspect, nv, round_); *bad = 1 when Python would raise (the caller redoes it in Python) */
static GEOSGeometry *stone(PyRandom *rng, const ScatterSpec *sp, double cx, double cy, double r, int nv, int *bad) {
    if (sp->round_) {          /* the hull of three overlapping circles, 40 points each, simplified by 0.04 */
        double c3[3][3]; GEOSGeometry *pts[120], *mp, *hl, *sm; int i, j;
        for (i = 0; i < 3; i++) {
            double x0 = cx + py_uniform(rng, -.4, .4) * r, y0 = cy + py_uniform(rng, -.4, .4) * r;
            c3[i][0] = x0; c3[i][1] = y0; c3[i][2] = r * py_uniform(rng, .55, .8);
        }
        P2 pp[120], o[130]; int no;
        for (i = 0; i < 3; i++) for (j = 0; j < 40; j++) {          /* numpy: x0 + rr * _CSX, two separate operations */
            pp[i * 40 + j].x = c3[i][0] + c3[i][2] * sp->csx[j]; pp[i * 40 + j].y = c3[i][1] + c3[i][2] * sp->csy[j];
        }
        if (scatter_fast && round_fast(pp, o, &no)) { GEOSGeometry *g = poly_of(o, no - 1); if (!g) *bad = 1; return g; }
        for (i = 0; i < 120; i++) pts[i] = G.GEOSGeom_createPointFromXY_r(gctx, pp[i].x, pp[i].y);
        mp = G.GEOSGeom_createCollection_r(gctx, 4 /* GEOS_MULTIPOINT */, pts, 120);
        if (!mp) { *bad = 1; return NULL; }
        hl = G.GEOSConvexHull_r(gctx, mp); G.GEOSGeom_destroy_r(gctx, mp);
        if (!hl) { *bad = 1; return NULL; }
        sm = G.GEOSTopologyPreserveSimplify_r(gctx, hl, 0.04); G.GEOSGeom_destroy_r(gctx, hl);
        if (!sm || G.GEOSGeomTypeId_r(gctx, sm) != 3 /* GEOS_POLYGON */) { if (sm) G.GEOSGeom_destroy_r(gctx, sm); *bad = 1; return NULL; }
        return sm;
    }
    {
        int n = nv ? nv : (int)py_randint(rng, 5, 8), i, nh;          /* n = nv or rng.randint(5, 8) */
        double aspect = sp->aspect ? sp->aspect : py_uniform(rng, .6, .95), rot = py_uniform(rng, 0, PI), ts[SC_MAXNV], cr, sr;
        P2 pts[SC_MAXNV], hp[2 * SC_MAXNV];
        GEOSGeometry *g;
        if (n < 1 || n > SC_MAXNV) { *bad = 1; return NULL; }
        for (i = 0; i < n; i++) ts[i] = py_uniform(rng, 0, 2 * PI);
        qsort(ts, n, sizeof(double), cmp_d);
        for (i = 0; i < n; i++) ts[i] = (double)(i * 2) * PI / n * 0.6 + ts[i] * 0.4;          /* i * 2 * math.pi / n * 0.6 + t * 0.4 */
        cr = py_cos(rot); sr = py_sin(rot);
        for (i = 0; i < n; i++) {
            double rr = r * py_uniform(rng, .82, 1.1), x = rr * py_cos(ts[i]), y = rr * py_sin(ts[i]) * aspect;
            pts[i].x = cx + x * cr - y * sr; pts[i].y = cy + x * sr + y * cr;
        }
        nh = hull(pts, n, hp);
        if (nh < 3) { *bad = 1; return NULL; }
        g = poly_of(hp, nh);
        if (!g) *bad = 1;
        return g;
    }
}

int scatter_cluster(PyRandom *rng, const ScatterSpec *sp, int n, double cx, double cy, double spread, double gap, ScatterOut *out) {
    GEOSGeometry *got[SC_MAXGOT]; int ng = 0, it, bad = 0, i, ok = 1;
    out->n = 0; out->ncoord = 0;
    if (n > SC_MAXGOT) return -1;
    for (it = 0; it < n * 30; it++) {
        double x, y, r, d; int nv = sp->nv_const; GEOSGeometry *g; int far = 1;
        if (ng >= n) break;
        x = cx + py_uniform(rng, -spread, spread); y = cy + py_uniform(rng, -spread, spread);          /* make_one(cx + U, cy + U) */
        r = py_uniform(rng, sp->r0, sp->r1);          /* the lambda: stone(x, y, rng.uniform(r0, r1), ...) */
        if (sp->nv_lo) nv = (int)py_randint(rng, sp->nv_lo, sp->nv_hi);          /* sh_rock: rng.randint(*nv), after the radius */
        g = stone(rng, sp, x, y, r, nv, &bad);
        if (bad) { ok = 0; break; }
        for (i = 0; i < ng; i++) {          /* all(g.distance(q) > gap for q in got) */
            if (!G.GEOSDistance_r(gctx, g, got[i], &d)) { ok = 0; break; }
            if (!(d > gap)) { far = 0; break; }
        }
        if (!ok) { G.GEOSGeom_destroy_r(gctx, g); break; }
        if (far) got[ng++] = g; else G.GEOSGeom_destroy_r(gctx, g);
    }
    for (i = 0; ok && i < ng; i++) {          /* the outlines, each ring as shapely holds it (closed) */
        const GEOSGeometry *ring = G.GEOSGetExteriorRing_r(gctx, got[i]); const GEOSCoordSequence *cs = ring ? G.GEOSGeom_getCoordSeq_r(gctx, ring) : NULL; unsigned m = 0, j;
        if (!cs || !G.GEOSCoordSeq_getSize_r(gctx, cs, &m) || out->ncoord + (int)m > SC_MAXCOORD) { ok = 0; break; }
        out->len[out->n++] = (int)m;
        for (j = 0; j < m; j++) { G.GEOSCoordSeq_getXY_r(gctx, cs, j, &out->xy[2 * out->ncoord], &out->xy[2 * out->ncoord + 1]); out->ncoord++; }
    }
    for (i = 0; i < ng; i++) G.GEOSGeom_destroy_r(gctx, got[i]);
    return ok ? out->n : -1;
}

/* the proof of round_fast: random rocks made both ways (GEOS and the copies above), every coordinate compared.
   Returns the number that differ; *unsure = how many the fast path handed to GEOS. */
__declspec(dllexport) int bbb_scatter_selftest(int n, unsigned seed, int *unsure, int *done) {
    PyRandom rng; int bad = 0, i, j, c;
    if (!geos_load() || !pyx_load()) return -1;
    py_seed_u64(&rng, seed); *unsure = 0; *done = 0;
    for (c = 0; c < n; c++) {
        double cx = py_uniform(&rng, -60, 60), cy = py_uniform(&rng, -60, 60), r = py_uniform(&rng, 0.05, 4.0), c3[3][3], cs[40], sn[40];
        P2 pp[120], o[130]; int no, m = 0; GEOSGeometry *pts[120], *mp, *hl, *sm; const GEOSCoordSequence *q; unsigned qn = 0;
        for (j = 0; j < 40; j++) { cs[j] = cos(j * (2 * PI / 40)); sn[j] = sin(j * (2 * PI / 40)); }
        for (i = 0; i < 3; i++) { c3[i][0] = cx + py_uniform(&rng, -.4, .4) * r; c3[i][1] = cy + py_uniform(&rng, -.4, .4) * r; c3[i][2] = r * py_uniform(&rng, .55, .8); }
        for (i = 0; i < 3; i++) for (j = 0; j < 40; j++) { pp[i * 40 + j].x = c3[i][0] + c3[i][2] * cs[j]; pp[i * 40 + j].y = c3[i][1] + c3[i][2] * sn[j]; }
        if (!round_fast(pp, o, &no)) { (*unsure)++; continue; }
        for (i = 0; i < 120; i++) pts[i] = G.GEOSGeom_createPointFromXY_r(gctx, pp[i].x, pp[i].y);
        mp = G.GEOSGeom_createCollection_r(gctx, 4, pts, 120); hl = G.GEOSConvexHull_r(gctx, mp); sm = G.GEOSTopologyPreserveSimplify_r(gctx, hl, 0.04);
        q = G.GEOSGeom_getCoordSeq_r(gctx, G.GEOSGetExteriorRing_r(gctx, sm)); G.GEOSCoordSeq_getSize_r(gctx, q, &qn);
        if ((int)qn != no) bad++;
        else for (m = 0; m < no; m++) { double x, y; G.GEOSCoordSeq_getXY_r(gctx, q, (unsigned)m, &x, &y); if (memcmp(&x, &o[m].x, 8) || memcmp(&y, &o[m].y, 8)) { bad++; break; } }
        G.GEOSGeom_destroy_r(gctx, mp); G.GEOSGeom_destroy_r(gctx, hl); G.GEOSGeom_destroy_r(gctx, sm);
        (*done)++;
    }
    return bad;
}
