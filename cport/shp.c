/* shp.c: the shapely layer (see shp.h). Each function names the shapely 2.2.0 call it copies and says how shapely
   does it: which GEOS function with which parameters (shapely's lib.*.pyd imports, read from the binary), and what
   shapely itself does in Python / numpy around the call (that arithmetic is redone here operation by operation;
   built with -ffp-contract=off and no fast-math, so nothing is fused or reordered). Proof: work\cscene\shp\prove.py. */
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <math.h>
#include "geos_dyn.h"
#include "pyx.h"
#include "shp.h"

#define PI 3.141592653589793          /* math.pi */

int shp_init(void) {
    static int ready = 0;
    if (!ready) ready = geos_load() && pyx_load();
    return ready;
}
void shp_free(GEOSGeometry *g) { if (g) G.GEOSGeom_destroy_r(gctx, g); }
GEOSGeometry *shp_clone(const GEOSGeometry *g) { return g ? G.GEOSGeom_clone_r(gctx, g) : NULL; }

/* ======================= constructors ======================= */

/* Point(x, y): shapely.points -> GEOSGeom_createPointFromXY */
GEOSGeometry *shp_point(double x, double y) { return G.GEOSGeom_createPointFromXY_r(gctx, x, y); }
GEOSGeometry *shp_point_empty(void) { return G.GEOSGeom_createEmptyPoint_r(gctx); }

static GEOSCoordSequence *seq_of(const double *xy, int n, int close) {          /* an XY sequence, closed when asked */
    GEOSCoordSequence *s = G.GEOSCoordSeq_create_r(gctx, (unsigned)(n + (close ? 1 : 0)), 2);
    if (!s) return NULL;
    for (int i = 0; i < n; i++) G.GEOSCoordSeq_setXY_r(gctx, s, (unsigned)i, xy[2 * i], xy[2 * i + 1]);
    if (close) G.GEOSCoordSeq_setXY_r(gctx, s, (unsigned)n, xy[0], xy[1]);
    return s;
}

/* LineString(pts): shapely.linestrings (no closing); [] -> LINESTRING EMPTY; one point -> GEOS refuses (shapely raises) */
GEOSGeometry *shp_linestring(const double *xy, int n) {
    GEOSCoordSequence *s;
    if (n == 0) return G.GEOSGeom_createEmptyLineString_r(gctx);
    s = seq_of(xy, n, 0);
    return s ? G.GEOSGeom_createLineString_r(gctx, s) : NULL;          /* the sequence is taken over, also on failure */
}

/* LinearRing(coords): shapely.linearrings closes the ring when the first and last coordinates differ (compared with !=),
   and ALWAYS when there are exactly 3 (so [a, b, a] becomes a b a a); fewer than 3 raises ("A linearring requires at
   least 4 coordinates") */
GEOSGeometry *shp_linearring(const double *xy, int n) {
    GEOSCoordSequence *s; int close;
    if (n == 0) { GEOSCoordSequence *e = G.GEOSCoordSeq_create_r(gctx, 0, 2); return e ? G.GEOSGeom_createLinearRing_r(gctx, e) : NULL; }
    if (n < 3) return NULL;
    close = n == 3 || xy[0] != xy[2 * (n - 1)] || xy[1] != xy[2 * (n - 1) + 1];
    s = seq_of(xy, n, close);
    return s ? G.GEOSGeom_createLinearRing_r(gctx, s) : NULL;
}

/* Polygon(coords): LinearRing(coords) then shapely.polygons(shell); no coordinates -> POLYGON EMPTY */
GEOSGeometry *shp_polygon(const double *xy, int n) { return shp_polygon_holes(xy, n, NULL, NULL, 0); }

/* Polygon(shell, holes): every ring closed as LinearRing() closes it; holes=[] is the same as no holes */
GEOSGeometry *shp_polygon_holes(const double *xy, int n, const double *const *hxy, const int *hn, int nh) {
    GEOSGeometry *sh, **hs = NULL, *r; int i;
    if (n == 0 && nh == 0) return G.GEOSGeom_createEmptyPolygon_r(gctx);
    sh = shp_linearring(xy, n);
    if (!sh) return NULL;
    if (nh > 0) {
        hs = (GEOSGeometry **)malloc(sizeof(GEOSGeometry *) * (size_t)nh);
        for (i = 0; i < nh; i++) {
            hs[i] = shp_linearring(hxy[i], hn[i]);
            if (!hs[i]) { while (i--) shp_free(hs[i]); shp_free(sh); free(hs); return NULL; }
        }
    }
    r = G.GEOSGeom_createPolygon_r(gctx, sh, hs, (unsigned)nh);          /* takes the rings over */
    free(hs);
    return r;
}

/* Polygon(ring): a LinearRing is used as it is; a LineString must be valid, then its coordinates make the ring;
   a Polygon comes back as it is */
GEOSGeometry *shp_polygon_from_ring(const GEOSGeometry *ring) {
    int t = G.GEOSGeomTypeId_r(gctx, ring);
    if (t == GEOS_POLYGON) return shp_clone(ring);
    if (t == GEOS_LINEARRING) {
        GEOSGeometry *sh = shp_clone(ring);
        return sh ? G.GEOSGeom_createPolygon_r(gctx, sh, NULL, 0) : NULL;
    }
    if (t == GEOS_LINESTRING) {
        int m = G.GEOSGetNumCoordinates_r(gctx, ring); double *xy; GEOSGeometry *r;
        if (G.GEOSisValid_r(gctx, ring) != 1) return NULL;          /* TopologicalError("An input LineString must be valid.") */
        if (m == 0) return G.GEOSGeom_createEmptyPolygon_r(gctx);    /* LinearRing of no coords is LINEARRING EMPTY -> POLYGON EMPTY */
        xy = (double *)malloc(sizeof(double) * 2 * (size_t)m);
        shp_coords(ring, xy, m);
        r = shp_polygon(xy, m);
        free(xy);
        return r;
    }
    return NULL;
}

/* shapely.geometry.box: Polygon([(maxx, miny), (maxx, maxy), (minx, maxy), (minx, miny)]), reversed when not ccw */
GEOSGeometry *shp_box(double minx, double miny, double maxx, double maxy, int ccw) {
    double c[8] = { maxx, miny, maxx, maxy, minx, maxy, minx, miny }, r[8];
    if (ccw) return shp_polygon(c, 4);
    for (int i = 0; i < 4; i++) { r[2 * i] = c[2 * (3 - i)]; r[2 * i + 1] = c[2 * (3 - i) + 1]; }
    return shp_polygon(r, 4);
}

/* MultiPoint(pts) == shapely.multipoints(shapely.points(pts)) */
GEOSGeometry *shp_multipoint(const double *xy, int n) {
    GEOSGeometry **p, *r;
    if (n == 0) return G.GEOSGeom_createEmptyCollection_r(gctx, GEOS_MULTIPOINT);
    p = (GEOSGeometry **)malloc(sizeof(GEOSGeometry *) * (size_t)n);
    for (int i = 0; i < n; i++) p[i] = shp_point(xy[2 * i], xy[2 * i + 1]);
    r = G.GEOSGeom_createCollection_r(gctx, GEOS_MULTIPOINT, p, (unsigned)n);
    free(p);
    return r;
}

/* a collection of clones of the parts (shapely.multipolygons / geometrycollections / lib.create_collection) */
GEOSGeometry *shp_collection(int type, const GEOSGeometry *const *gs, int n) {
    GEOSGeometry **p, *r; int m = 0;
    if (n == 0) return G.GEOSGeom_createEmptyCollection_r(gctx, type);
    p = (GEOSGeometry **)malloc(sizeof(GEOSGeometry *) * (size_t)n);
    for (int i = 0; i < n; i++) if (gs[i]) p[m++] = shp_clone(gs[i]);          /* None is left out */
    r = m ? G.GEOSGeom_createCollection_r(gctx, type, p, (unsigned)m) : G.GEOSGeom_createEmptyCollection_r(gctx, type);
    free(p);
    return r;
}

/* ======================= buffer ======================= */

/* BaseGeometry.buffer -> shapely.buffer -> lib.buffer: GEOSBufferParams with the four styles and single_sided, then
   GEOSBufferWithParams. quad_segs goes through np.intc (an int). Python raises for mitre_limit == 0 and a distance
   that is not finite. */
GEOSGeometry *shp_buffer(const GEOSGeometry *g, double distance, int quad_segs, int cap, int join, double mitre_limit, int single_sided) {
    GEOSBufferParams *bp; GEOSGeometry *r;
    if (mitre_limit == 0.0 || !isfinite(distance)) return NULL;
    bp = G.GEOSBufferParams_create_r(gctx);
    if (!bp) return NULL;
    if (!G.GEOSBufferParams_setEndCapStyle_r(gctx, bp, cap) || !G.GEOSBufferParams_setJoinStyle_r(gctx, bp, join) ||
        !G.GEOSBufferParams_setMitreLimit_r(gctx, bp, mitre_limit) || !G.GEOSBufferParams_setQuadrantSegments_r(gctx, bp, quad_segs) ||
        !G.GEOSBufferParams_setSingleSided_r(gctx, bp, single_sided ? 1 : 0)) { G.GEOSBufferParams_destroy_r(gctx, bp); return NULL; }
    r = G.GEOSBufferWithParams_r(gctx, g, bp, distance);
    G.GEOSBufferParams_destroy_r(gctx, bp);
    return r;
}
GEOSGeometry *shp_buffer_q(const GEOSGeometry *g, double distance, int quad_segs) {
    return shp_buffer(g, distance, quad_segs, SHP_CAP_ROUND, SHP_JOIN_ROUND, 5.0, 0);
}
GEOSGeometry *shp_point_buffer(double x, double y, double r, int quad_segs) {
    GEOSGeometry *p = shp_point(x, y), *b = p ? shp_buffer_q(p, r, quad_segs) : NULL;
    shp_free(p);
    return b;
}
GEOSGeometry *shp_line_buffer(const double *xy, int n, double w, int cap, int join, int quad_segs, double mitre_limit) {
    GEOSGeometry *l = shp_linestring(xy, n), *b = l ? shp_buffer(l, w, quad_segs, cap, join, mitre_limit, 0) : NULL;
    shp_free(l);
    return b;
}

/* ======================= set operations ======================= */

/* unary_union(geoms) == shapely.union_all(geoms): lib.create_collection(geoms, GEOMETRYCOLLECTION) (None left out,
   everything else cloned in order, empties included), then GEOSUnaryUnion */
GEOSGeometry *shp_union_all(const GEOSGeometry *const *gs, int n) {
    GEOSGeometry *c = shp_collection(GEOS_GEOMETRYCOLLECTION, gs, n), *r;
    if (!c) return NULL;
    r = G.GEOSUnaryUnion_r(gctx, c);
    shp_free(c);
    return r;
}
GEOSGeometry *shp_union(const GEOSGeometry *a, const GEOSGeometry *b) { return G.GEOSUnion_r(gctx, a, b); }
GEOSGeometry *shp_difference(const GEOSGeometry *a, const GEOSGeometry *b) { return G.GEOSDifference_r(gctx, a, b); }
GEOSGeometry *shp_intersection(const GEOSGeometry *a, const GEOSGeometry *b) { return G.GEOSIntersection_r(gctx, a, b); }
GEOSGeometry *shp_symdiff(const GEOSGeometry *a, const GEOSGeometry *b) { return G.GEOSSymDifference_r(gctx, a, b); }

/* ======================= predicates ======================= */
static int b3(char c) { return c == 2 ? -1 : c; }          /* GEOS: 1 true, 0 false, 2 exception */
int shp_contains(const GEOSGeometry *a, const GEOSGeometry *b) { return b3(G.GEOSContains_r(gctx, a, b)); }
int shp_intersects(const GEOSGeometry *a, const GEOSGeometry *b) { return b3(G.GEOSIntersects_r(gctx, a, b)); }
int shp_within(const GEOSGeometry *a, const GEOSGeometry *b) { return b3(G.GEOSWithin_r(gctx, a, b)); }
int shp_touches(const GEOSGeometry *a, const GEOSGeometry *b) { return b3(G.GEOSTouches_r(gctx, a, b)); }
int shp_crosses(const GEOSGeometry *a, const GEOSGeometry *b) { return b3(G.GEOSCrosses_r(gctx, a, b)); }
int shp_overlaps(const GEOSGeometry *a, const GEOSGeometry *b) { return b3(G.GEOSOverlaps_r(gctx, a, b)); }
int shp_covers(const GEOSGeometry *a, const GEOSGeometry *b) { return b3(G.GEOSCovers_r(gctx, a, b)); }
int shp_covered_by(const GEOSGeometry *a, const GEOSGeometry *b) { return b3(G.GEOSCoveredBy_r(gctx, a, b)); }
int shp_disjoint(const GEOSGeometry *a, const GEOSGeometry *b) { return b3(G.GEOSDisjoint_r(gctx, a, b)); }
int shp_equals(const GEOSGeometry *a, const GEOSGeometry *b) { return b3(G.GEOSEquals_r(gctx, a, b)); }
int shp_dwithin(const GEOSGeometry *a, const GEOSGeometry *b, double d) { return b3(G.GEOSDistanceWithin_r(gctx, a, b, d)); }
int shp_is_empty(const GEOSGeometry *g) { return g ? b3(G.GEOSisEmpty_r(gctx, g)) : 1; }
int shp_is_valid(const GEOSGeometry *g) { return b3(G.GEOSisValid_r(gctx, g)); }

/* ======================= measurement ======================= */

/* distance_scalar: nan when either side is empty, else GEOSDistance */
double shp_distance(const GEOSGeometry *a, const GEOSGeometry *b) {
    double d;
    if (G.GEOSisEmpty_r(gctx, a) || G.GEOSisEmpty_r(gctx, b)) return NAN;
    return G.GEOSDistance_r(gctx, a, b, &d) ? d : NAN;
}
double shp_area(const GEOSGeometry *g) { double a; return G.GEOSArea_r(gctx, g, &a) ? a : NAN; }
double shp_length(const GEOSGeometry *g) { double a; return G.GEOSLength_r(gctx, g, &a) ? a : NAN; }
/* shapely.bounds: nan x4 for an empty, else the extent */
void shp_bounds(const GEOSGeometry *g, double b[4]) {
    if (!g || G.GEOSisEmpty_r(gctx, g) || !G.GEOSGeom_getExtent_r(gctx, g, &b[0], &b[1], &b[2], &b[3])) b[0] = b[1] = b[2] = b[3] = NAN;
}
/* line.project(pt) == shapely.line_locate_point: nan for an empty line or point, else GEOSProject(Normalized) */
double shp_project(const GEOSGeometry *line, const GEOSGeometry *pt, int normalized) {
    if (G.GEOSisEmpty_r(gctx, line) || G.GEOSisEmpty_r(gctx, pt)) return NAN;
    return normalized ? G.GEOSProjectNormalized_r(gctx, line, pt) : G.GEOSProject_r(gctx, line, pt);
}

/* ======================= constructive ======================= */
GEOSGeometry *shp_centroid(const GEOSGeometry *g) { return G.GEOSGetCentroid_r(gctx, g); }
GEOSGeometry *shp_representative_point(const GEOSGeometry *g) { return G.GEOSPointOnSurface_r(gctx, g); }
/* g.simplify(tol, preserve_topology): GEOSTopologyPreserveSimplify, or GEOSSimplify (Douglas-Peucker) */
GEOSGeometry *shp_simplify(const GEOSGeometry *g, double tolerance, int preserve_topology) {
    return preserve_topology ? G.GEOSTopologyPreserveSimplify_r(gctx, g, tolerance) : G.GEOSSimplify_r(gctx, g, tolerance);
}
GEOSGeometry *shp_convex_hull(const GEOSGeometry *g) { return G.GEOSConvexHull_r(gctx, g); }
GEOSGeometry *shp_envelope(const GEOSGeometry *g) { return G.GEOSEnvelope_r(gctx, g); }
/* g.boundary: None (NULL here) for a GeometryCollection, else GEOSBoundary */
GEOSGeometry *shp_boundary(const GEOSGeometry *g) {
    if (G.GEOSGeomTypeId_r(gctx, g) == GEOS_GEOMETRYCOLLECTION) return NULL;
    return G.GEOSBoundary_r(gctx, g);
}
GEOSGeometry *shp_min_rot_rect(const GEOSGeometry *g) { return G.GEOSMinimumRotatedRectangle_r(gctx, g); }
/* line.interpolate(d, normalized): POINT EMPTY for an empty line, else GEOSInterpolate(Normalized) */
GEOSGeometry *shp_interpolate(const GEOSGeometry *line, double distance, int normalized) {
    if (G.GEOSisEmpty_r(gctx, line)) return G.GEOSGeom_createEmptyPoint_r(gctx);
    return normalized ? G.GEOSInterpolateNormalized_r(gctx, line, distance) : G.GEOSInterpolate_r(gctx, line, distance);
}
GEOSGeometry *shp_offset_curve(const GEOSGeometry *line, double distance, int quad_segs, int join, double mitre_limit) {
    return G.GEOSOffsetCurve_r(gctx, line, distance, quad_segs, join, mitre_limit);
}
GEOSGeometry *shp_snap(const GEOSGeometry *a, const GEOSGeometry *b, double tolerance) { return G.GEOSSnap_r(gctx, a, b, tolerance); }
/* orient(g, sign) -> lib.orient_polygons_scalar(g, int(sign < 0.0)): a clone with GEOSOrientPolygons on it */
GEOSGeometry *shp_orient(const GEOSGeometry *g, double sign) {
    GEOSGeometry *r = shp_clone(g);
    if (r && G.GEOSOrientPolygons_r(gctx, r, sign < 0.0 ? 1 : 0) != 0) { shp_free(r); return NULL; }
    return r;
}
/* shapely.ops.voronoi_diagram(g, envelope, tolerance, edges) -> voronoi_polygons(g, tolerance, extend_to=envelope,
   only_edges=edges, ordered=False) -> GEOSVoronoiDiagram (flags: 1 only edges); a result that is not a
   GeometryCollection is wrapped in one */
GEOSGeometry *shp_voronoi(const GEOSGeometry *g, const GEOSGeometry *envelope, double tolerance, int edges) {
    GEOSGeometry *r = G.GEOSVoronoiDiagram_r(gctx, g, envelope, tolerance, edges ? 1 : 0), *w;
    if (!r || G.GEOSGeomTypeId_r(gctx, r) == GEOS_GEOMETRYCOLLECTION) return r;
    w = G.GEOSGeom_createCollection_r(gctx, GEOS_GEOMETRYCOLLECTION, &r, 1);
    return w;
}
GEOSGeometry *shp_constrained_delaunay(const GEOSGeometry *g) { return G.GEOSConstrainedDelaunayTriangulation_r(gctx, g); }
GEOSGeometry *shp_reverse(const GEOSGeometry *g) { return G.GEOSReverse_r(gctx, g); }

/* ======================= access ======================= */
int shp_type(const GEOSGeometry *g) { return G.GEOSGeomTypeId_r(gctx, g); }
int shp_num_geoms(const GEOSGeometry *g) { return G.GEOSGetNumGeometries_r(gctx, g); }
const GEOSGeometry *shp_geom_n(const GEOSGeometry *g, int i) { return G.GEOSGetGeometryN_r(gctx, g, i); }
const GEOSGeometry *shp_exterior(const GEOSGeometry *p) { return G.GEOSGetExteriorRing_r(gctx, p); }
int shp_num_interiors(const GEOSGeometry *p) { return G.GEOSisEmpty_r(gctx, p) ? 0 : G.GEOSGetNumInteriorRings_r(gctx, p); }
const GEOSGeometry *shp_interior(const GEOSGeometry *p, int i) { return G.GEOSGetInteriorRingN_r(gctx, p, i); }
int shp_coords(const GEOSGeometry *g, double *xy, int cap) {
    const GEOSCoordSequence *cs; unsigned m = 0;
    if (G.GEOSisEmpty_r(gctx, g)) return 0;
    cs = G.GEOSGeom_getCoordSeq_r(gctx, g);
    if (!cs || !G.GEOSCoordSeq_getSize_r(gctx, cs, &m) || (int)m > cap) return -1;
    if (m && !G.GEOSCoordSeq_copyToBuffer_r(gctx, cs, xy, 0, 0)) return -1;
    return (int)m;
}
double shp_x(const GEOSGeometry *pt) { double v; return G.GEOSGeomGetX_r(gctx, pt, &v) ? v : NAN; }
double shp_y(const GEOSGeometry *pt) { double v; return G.GEOSGeomGetY_r(gctx, pt, &v) ? v : NAN; }
int shp_has_z(const GEOSGeometry *g) { return b3(G.GEOSHasZ_r(gctx, g)); }

/* ======================= affinity ======================= */
/* affinity.affine_transform -> shapely.transform: get_coordinates, then numpy, element by element and in this order:
     xp = a * x + b * y + xoff;  yp = d * x + e * y + yoff
   then set_coordinates on a copy (same structure, every coordinate replaced, ring closings too). */
typedef struct { double a, b, d, e, xoff, yoff; } Aff;
static GEOSCoordSequence *aff_seq(const GEOSCoordSequence *cs, const Aff *m) {
    unsigned n = 0; double *xy; GEOSCoordSequence *r;
    if (!G.GEOSCoordSeq_getSize_r(gctx, cs, &n)) return NULL;
    xy = (double *)malloc(sizeof(double) * 2 * (n ? n : 1));
    if (n && !G.GEOSCoordSeq_copyToBuffer_r(gctx, cs, xy, 0, 0)) { free(xy); return NULL; }
    for (unsigned i = 0; i < n; i++) {
        double x = xy[2 * i], y = xy[2 * i + 1];
        xy[2 * i] = m->a * x + m->b * y + m->xoff;
        xy[2 * i + 1] = m->d * x + m->e * y + m->yoff;
    }
    r = G.GEOSCoordSeq_copyFromBuffer_r(gctx, xy, n, 0, 0);
    free(xy);
    return r;
}
static GEOSGeometry *aff_geom(const GEOSGeometry *g, const Aff *m) {
    int t = G.GEOSGeomTypeId_r(gctx, g);
    if (G.GEOSisEmpty_r(gctx, g)) return shp_clone(g);
    switch (t) {
    case GEOS_POINT: {
        double x, y;
        if (!G.GEOSGeomGetX_r(gctx, g, &x) || !G.GEOSGeomGetY_r(gctx, g, &y)) return NULL;
        return G.GEOSGeom_createPointFromXY_r(gctx, m->a * x + m->b * y + m->xoff, m->d * x + m->e * y + m->yoff);
    }
    case GEOS_LINESTRING: case GEOS_LINEARRING: {
        GEOSCoordSequence *s = aff_seq(G.GEOSGeom_getCoordSeq_r(gctx, g), m);
        if (!s) return NULL;
        return t == GEOS_LINESTRING ? G.GEOSGeom_createLineString_r(gctx, s) : G.GEOSGeom_createLinearRing_r(gctx, s);
    }
    case GEOS_POLYGON: {
        int nh = G.GEOSGetNumInteriorRings_r(gctx, g), i; GEOSGeometry *sh = aff_geom(G.GEOSGetExteriorRing_r(gctx, g), m), **hs, *r;
        if (!sh) return NULL;
        hs = (GEOSGeometry **)malloc(sizeof(GEOSGeometry *) * (size_t)(nh ? nh : 1));
        for (i = 0; i < nh; i++) hs[i] = aff_geom(G.GEOSGetInteriorRingN_r(gctx, g, i), m);
        r = G.GEOSGeom_createPolygon_r(gctx, sh, hs, (unsigned)nh);
        free(hs);
        return r;
    }
    default: {
        int n = G.GEOSGetNumGeometries_r(gctx, g), i; GEOSGeometry **p = (GEOSGeometry **)malloc(sizeof(GEOSGeometry *) * (size_t)(n ? n : 1)), *r;
        for (i = 0; i < n; i++) p[i] = aff_geom(G.GEOSGetGeometryN_r(gctx, g, i), m);
        r = G.GEOSGeom_createCollection_r(gctx, t, p, (unsigned)n);
        free(p);
        return r;
    }
    }
}
GEOSGeometry *shp_affine(const GEOSGeometry *g, double a, double b, double d, double e, double xoff, double yoff) {
    Aff m = { a, b, d, e, xoff, yoff };
    return aff_geom(g, &m);
}

/* affinity.interpret_origin: 'center' = the bounds' middle ((maxx + minx) / 2.0, (maxy + miny) / 2.0),
   'centroid' = geom.centroid.coords[0], else the given point */
static int origin_of(const GEOSGeometry *g, int origin, double ox, double oy, double *x0, double *y0) {
    if (origin == SHP_ORIGIN_CENTER) {
        double b[4]; shp_bounds(g, b);
        *x0 = (b[2] + b[0]) / 2.0; *y0 = (b[3] + b[1]) / 2.0;
    } else if (origin == SHP_ORIGIN_CENTROID) {
        GEOSGeometry *c = shp_centroid(g);
        if (!c) return 0;
        *x0 = shp_x(c); *y0 = shp_y(c); shp_free(c);
    } else { *x0 = ox; *y0 = oy; }
    return 1;
}

/* affinity.rotate(geom, angle, origin, use_radians): an empty geometry comes back as it is; degrees -> angle * pi / 180.0;
   cos / sin of the UCRT (math.cos / math.sin), each set to 0.0 below 2.5e-16 in size; matrix
   (cosp, -sinp, sinp, cosp, x0 - x0 * cosp + y0 * sinp, y0 - x0 * sinp - y0 * cosp) */
GEOSGeometry *shp_rotate(const GEOSGeometry *g, double angle, int origin, double ox, double oy, int use_radians) {
    double cosp, sinp, x0, y0;
    if (G.GEOSisEmpty_r(gctx, g)) return shp_clone(g);
    if (!use_radians) angle = angle * PI / 180.0;
    cosp = py_cos(angle); sinp = py_sin(angle);
    if (fabs(cosp) < 2.5e-16) cosp = 0.0;
    if (fabs(sinp) < 2.5e-16) sinp = 0.0;
    if (!origin_of(g, origin, ox, oy, &x0, &y0)) return NULL;
    return shp_affine(g, cosp, -sinp, sinp, cosp, x0 - x0 * cosp + y0 * sinp, y0 - x0 * sinp - y0 * cosp);
}

/* affinity.scale(geom, xfact, yfact, origin): matrix (xfact, 0.0, 0.0, yfact, x0 - x0 * xfact, y0 - y0 * yfact) */
GEOSGeometry *shp_scale(const GEOSGeometry *g, double xfact, double yfact, int origin, double ox, double oy) {
    double x0, y0;
    if (G.GEOSisEmpty_r(gctx, g)) return shp_clone(g);
    if (!origin_of(g, origin, ox, oy, &x0, &y0)) return NULL;
    return shp_affine(g, xfact, 0.0, 0.0, yfact, x0 - x0 * xfact, y0 - y0 * yfact);
}

/* affinity.translate(geom, xoff, yoff): matrix (1.0, 0.0, 0.0, 1.0, xoff, yoff) */
GEOSGeometry *shp_translate(const GEOSGeometry *g, double xoff, double yoff) {
    if (G.GEOSisEmpty_r(gctx, g)) return shp_clone(g);
    return shp_affine(g, 1.0, 0.0, 0.0, 1.0, xoff, yoff);
}

/* ======================= prepared ======================= */
/* shapely.prepared.prep(g): shapely.prepare(g) (GEOSPrepare); its predicates then run GEOSPrepared* */
ShpPrep shp_prep(const GEOSGeometry *g) { ShpPrep p; p.g = g; p.p = G.GEOSPrepare_r(gctx, g); return p; }
void shp_prep_free(ShpPrep *p) { if (p->p) G.GEOSPreparedGeom_destroy_r(gctx, p->p); p->p = NULL; }
int shp_prep_contains(const ShpPrep *p, const GEOSGeometry *o) { return b3(G.GEOSPreparedContains_r(gctx, p->p, o)); }
int shp_prep_intersects(const ShpPrep *p, const GEOSGeometry *o) { return b3(G.GEOSPreparedIntersects_r(gctx, p->p, o)); }
int shp_prep_covers(const ShpPrep *p, const GEOSGeometry *o) { return b3(G.GEOSPreparedCovers_r(gctx, p->p, o)); }
int shp_prep_within(const ShpPrep *p, const GEOSGeometry *o) { return b3(G.GEOSPreparedWithin_r(gctx, p->p, o)); }
int shp_prep_touches(const ShpPrep *p, const GEOSGeometry *o) { return b3(G.GEOSPreparedTouches_r(gctx, p->p, o)); }
int shp_prep_contains_properly(const ShpPrep *p, const GEOSGeometry *o) { return b3(G.GEOSPreparedContainsProperly_r(gctx, p->p, o)); }

/* ======================= lists ======================= */
void shp_list_push(ShpList *l, GEOSGeometry *g) {
    if (l->n == l->cap) { l->cap = l->cap ? 2 * l->cap : 16; l->g = (GEOSGeometry **)realloc(l->g, sizeof(GEOSGeometry *) * (size_t)l->cap); }
    l->g[l->n++] = g;
}
void shp_list_free(ShpList *l) { for (int i = 0; i < l->n; i++) shp_free(l->g[i]); free(l->g); l->g = NULL; l->n = l->cap = 0; }
static void idx_push(ShpIdx *x, int i, double d) {
    if (x->n == x->cap) {
        x->cap = x->cap ? 2 * x->cap : 16;
        x->i = (int *)realloc(x->i, sizeof(int) * (size_t)x->cap); x->d = (double *)realloc(x->d, sizeof(double) * (size_t)x->cap);
    }
    x->i[x->n] = i; x->d[x->n] = d; x->n++;
}
void shp_idx_free(ShpIdx *x) { free(x->i); free(x->d); x->i = NULL; x->d = NULL; x->n = x->cap = 0; }

/* ======================= STRtree ======================= */
/* shapely.STRtree(geoms, node_capacity=10): lib.STRtree makes GEOSSTRtree_create(node_capacity) and inserts every
   geometry that is neither None nor empty, in order, with its index as the item. The GEOS tree's shape depends only on
   the envelopes and their order, so it is the very tree shapely queries. */
struct ShpTree { GEOSSTRtree *t; const GEOSGeometry **g; int n; };
ShpTree *shp_tree_new(const GEOSGeometry *const *gs, int n, int node_capacity) {
    ShpTree *t = (ShpTree *)calloc(1, sizeof(ShpTree));
    t->t = G.GEOSSTRtree_create_r(gctx, (size_t)node_capacity);
    t->g = (const GEOSGeometry **)malloc(sizeof(GEOSGeometry *) * (size_t)(n ? n : 1)); t->n = n;
    for (int i = 0; i < n; i++) {
        t->g[i] = gs[i];
        if (gs[i] && !G.GEOSisEmpty_r(gctx, gs[i])) G.GEOSSTRtree_insert_r(gctx, t->t, gs[i], (void *)(intptr_t)i);
    }
    return t;
}
void shp_tree_free(ShpTree *t) { if (!t) return; G.GEOSSTRtree_destroy_r(gctx, t->t); free(t->g); free(t); }

typedef struct { ShpIdx *out; } QCtx;
static void q_cb(void *item, void *ud) { idx_push(((QCtx *)ud)->out, (int)(intptr_t)item, 0.0); }

/* the prepared predicate shapely's tree query runs: predicate(geometry, tree_geometry) */
static int pred_prep(int predicate, const GEOSPreparedGeometry *p, const GEOSGeometry *o) {
    switch (predicate) {
    case SHP_PRED_INTERSECTS: return b3(G.GEOSPreparedIntersects_r(gctx, p, o));
    case SHP_PRED_WITHIN: return b3(G.GEOSPreparedWithin_r(gctx, p, o));
    case SHP_PRED_CONTAINS: return b3(G.GEOSPreparedContains_r(gctx, p, o));
    case SHP_PRED_OVERLAPS: return b3(G.GEOSPreparedOverlaps_r(gctx, p, o));
    case SHP_PRED_CROSSES: return b3(G.GEOSPreparedCrosses_r(gctx, p, o));
    case SHP_PRED_TOUCHES: return b3(G.GEOSPreparedTouches_r(gctx, p, o));
    case SHP_PRED_COVERS: return b3(G.GEOSPreparedCovers_r(gctx, p, o));
    case SHP_PRED_COVERED_BY: return b3(G.GEOSPreparedCoveredBy_r(gctx, p, o));
    case SHP_PRED_CONTAINS_PROPERLY: return b3(G.GEOSPreparedContainsProperly_r(gctx, p, o));
    }
    return -1;
}

/* tree.query(g, predicate): the tree's envelope hits in the order GEOS visits them, then (with a predicate) only those
   where the prepared predicate holds; an empty g finds nothing */
int shp_tree_query(ShpTree *t, const GEOSGeometry *g, int predicate, ShpIdx *out) {
    ShpIdx hits = { 0 }; QCtx q = { &hits }; int i, k = 0;
    if (!g || G.GEOSisEmpty_r(gctx, g)) return 0;
    G.GEOSSTRtree_query_r(gctx, t->t, g, q_cb, &q);
    if (predicate == SHP_PRED_NONE) {
        for (i = 0; i < hits.n; i++) idx_push(out, hits.i[i], 0.0);
        k = hits.n;
    } else {
        const GEOSPreparedGeometry *p = G.GEOSPrepare_r(gctx, g);
        for (i = 0; i < hits.n; i++) {
            int r = pred_prep(predicate, p, t->g[hits.i[i]]);
            if (r < 0) { k = -1; break; }
            if (r) { idx_push(out, hits.i[i], 0.0); k++; }
        }
        G.GEOSPreparedGeom_destroy_r(gctx, p);
    }
    shp_idx_free(&hits);
    return k;
}

int shp_tree_query_bulk(ShpTree *t, const GEOSGeometry *const *gs, int n, int predicate, ShpIdx *qi, ShpIdx *ti) {
    int k = 0;
    for (int j = 0; j < n; j++) {
        int before = ti->n, r = shp_tree_query(t, gs[j], predicate, ti);
        if (r < 0) return -1;
        for (int i = before; i < ti->n; i++) idx_push(qi, j, 0.0);
        k += r;
    }
    return k;
}

/* tree.query(g, predicate="dwithin", distance=d): the envelope grown by d, then GEOSPreparedDistanceWithin. (The
   order is the tree's own leaf order, which a wider query box does not change.) */
int shp_tree_query_dwithin(ShpTree *t, const GEOSGeometry *g, double distance, ShpIdx *out) {
    ShpIdx hits = { 0 }; QCtx q = { &hits }; int i, k = 0; double b[4]; GEOSGeometry *env; const GEOSPreparedGeometry *p;
    if (!g || G.GEOSisEmpty_r(gctx, g)) return 0;
    shp_bounds(g, b);
    env = shp_box(b[0] - distance, b[1] - distance, b[2] + distance, b[3] + distance, 1);
    G.GEOSSTRtree_query_r(gctx, t->t, env, q_cb, &q);
    shp_free(env);
    p = G.GEOSPrepare_r(gctx, g);
    for (i = 0; i < hits.n; i++) {
        int r = b3(G.GEOSPreparedDistanceWithin_r(gctx, p, t->g[hits.i[i]], distance));
        if (r < 0) { k = -1; break; }
        if (r) { idx_push(out, hits.i[i], 0.0); k++; }
    }
    G.GEOSPreparedGeom_destroy_r(gctx, p);
    shp_idx_free(&hits);
    return k;
}

/* tree.query_nearest(g, return_distance=True, exclusive, all_matches): GEOSSTRtree_nearest_generic with g as the item
   and its envelope, and shapely's distance callback: GEOSDistance between g and the tree geometry (DBL_MAX for an equal
   one when exclusive); a distance below the best so far starts the list again, an equal one joins it when all_matches;
   without all_matches the first one found at the least distance is the answer. */
typedef struct { ShpTree *t; const GEOSGeometry *g; int exclusive, all_matches; double min; ShpIdx pairs; int fail; } NCtx;
static int n_cb(const void *item1, const void *item2, double *distance, void *ud) {
    NCtx *c = (NCtx *)ud; int i = (int)(intptr_t)item1; const GEOSGeometry *tg = c->t->g[i]; double d;
    (void)item2;
    if (c->exclusive && G.GEOSEquals_r(gctx, c->g, tg) == 1) { *distance = 1.7976931348623157e308; return 1; }
    if (!G.GEOSDistance_r(gctx, c->g, tg, &d)) { c->fail = 1; return 0; }
    if (d < c->min) { c->pairs.n = 0; c->min = d; idx_push(&c->pairs, i, d); }
    else if (d == c->min && c->all_matches) idx_push(&c->pairs, i, d);
    *distance = c->all_matches ? nextafter(d, INFINITY) : d;          /* all_matches: a hair more, so GEOS keeps searching for ties */
    return 1;
}
int shp_tree_query_nearest(ShpTree *t, const GEOSGeometry *g, int exclusive, int all_matches, ShpIdx *out) {
    NCtx c; int i;
    if (!g || G.GEOSisEmpty_r(gctx, g)) return 0;
    memset(&c, 0, sizeof c); c.t = t; c.g = g; c.exclusive = exclusive; c.all_matches = all_matches; c.min = 1.7976931348623157e308;
    G.GEOSSTRtree_nearest_generic_r(gctx, t->t, g, g, n_cb, &c);
    if (c.fail) { shp_idx_free(&c.pairs); return -1; }
    for (i = 0; i < c.pairs.n; i++) idx_push(out, c.pairs.i[i], c.pairs.d[i]);
    i = c.pairs.n; shp_idx_free(&c.pairs);
    return i;
}

/* ======================= the kit's helpers ======================= */
/* gg.polys_of(g): [] for None / empty, [g] for a Polygon, the parts' polygons depth first for a collection, else [] */
void shp_polys_of(const GEOSGeometry *g, ShpList *out) {
    int t;
    if (!g || G.GEOSisEmpty_r(gctx, g)) return;
    t = G.GEOSGeomTypeId_r(gctx, g);
    if (t == GEOS_POLYGON) { shp_list_push(out, shp_clone(g)); return; }
    if (t >= GEOS_MULTIPOINT) for (int i = 0, n = G.GEOSGetNumGeometries_r(gctx, g); i < n; i++) shp_polys_of(G.GEOSGetGeometryN_r(gctx, g, i), out);
}
/* gg.inset(p, d) */
GEOSGeometry *shp_inset(const GEOSGeometry *p, double d) { return shp_buffer(p, -d, 16, SHP_CAP_ROUND, SHP_JOIN_MITRE, 10.0, 0); }
/* gg.wide_enough(p, w): not p.buffer(-(w / _TS) / 2 + 0.02 / _TS).is_empty */
int shp_wide_enough(const GEOSGeometry *p, double w, double ts) {
    GEOSGeometry *b = shp_buffer_q(p, -(w / ts) / 2 + 0.02 / ts, 16); int r;
    if (!b) return -1;
    r = !G.GEOSisEmpty_r(gctx, b); shp_free(b);
    return r;
}
/* gg3.rect(cx, cy, l, w, a): f = gg.frame(cx, cy, a) (c, s = math.cos(a), math.sin(a); f(u, v) = (cx + u * c - v * s,
   cy + u * s + v * c)); Polygon([f(-l / 2, -w / 2), f(l / 2, -w / 2), f(l / 2, w / 2), f(-l / 2, w / 2)]) */
GEOSGeometry *shp_rect(double cx, double cy, double l, double w, double a) {
    double c = py_cos(a), s = py_sin(a), uv[8] = { -l / 2, -w / 2, l / 2, -w / 2, l / 2, w / 2, -l / 2, w / 2 }, xy[8];
    for (int i = 0; i < 4; i++) { double u = uv[2 * i], v = uv[2 * i + 1]; xy[2 * i] = cx + u * c - v * s; xy[2 * i + 1] = cy + u * s + v * c; }
    return shp_polygon(xy, 4);
}
/* gg.star(cx, cy, ro, ri, n, rot): 2n points, radius ro on even i, ri on odd, angle rot + i * math.pi / n */
GEOSGeometry *shp_star(double cx, double cy, double ro, double ri, int n, double rot) {
    double *xy = (double *)malloc(sizeof(double) * 4 * (size_t)(n > 0 ? n : 1)); GEOSGeometry *r;
    for (int i = 0; i < 2 * n; i++) {
        double rr = i % 2 == 0 ? ro : ri, t = rot + (double)i * PI / n;
        xy[2 * i] = cx + rr * py_cos(t); xy[2 * i + 1] = cy + rr * py_sin(t);
    }
    r = shp_polygon(xy, 2 * n);
    free(xy);
    return r;
}
/* gg3.hexa(cx, cy, r, rot): Polygon of (cx + r * math.cos(rot + k * math.pi / 3), ...) for k in range(6) */
GEOSGeometry *shp_hexa(double cx, double cy, double r, double rot) {
    double xy[12];
    for (int k = 0; k < 6; k++) { double t = rot + (double)k * PI / 3; xy[2 * k] = cx + r * py_cos(t); xy[2 * k + 1] = cy + r * py_sin(t); }
    return shp_polygon(xy, 6);
}
/* gg3.ring(cx, cy, ro, ri, res=32): Point(cx, cy).buffer(ro, res).difference(Point(cx, cy).buffer(ri, res)) */
GEOSGeometry *shp_ring(double cx, double cy, double ro, double ri, int res) {
    GEOSGeometry *a = shp_point_buffer(cx, cy, ro, res), *b = shp_point_buffer(cx, cy, ri, res), *r = NULL;
    if (a && b) r = shp_difference(a, b);
    shp_free(a); shp_free(b);
    return r;
}
