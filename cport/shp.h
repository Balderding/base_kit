/* shp.h: the shapely layer of the C port. Each function does exactly what one shapely 2.2.0 call does (GEOS 3.14.1,
   the very geos_c.dll shapely ships, loaded by geos_dyn.c), so the scene code can be ported call for call and give
   byte-identical geometry. Proven against shapely on random inputs: work\cscene\shp\prove.py.

   Rules:
   - every GEOSGeometry * returned is NEW and owned by the caller (free with shp_free); NULL = shapely would raise
     (GEOS error text in geos_err). Inputs are never changed or taken over.
   - doubles come back as shapely gives them (NaN where shapely gives nan, e.g. the bounds or distance of an empty).
   - predicates return 1 / 0, or -1 where shapely would raise.
   - coordinates are passed as flat x, y arrays (xy[2 * i], xy[2 * i + 1]).
   Call shp_init() once (loads GEOS and the UCRT maths). */
#ifndef SHP_H
#define SHP_H
#include "geos_dyn.h"

int shp_init(void);                                       /* 1 = ready */
void shp_free(GEOSGeometry *g);                           /* NULL is fine */
GEOSGeometry *shp_clone(const GEOSGeometry *g);

/* ---- constructors ---- */
GEOSGeometry *shp_point(double x, double y);                                /* Point(x, y) */
GEOSGeometry *shp_point_empty(void);                                        /* Point() */
GEOSGeometry *shp_linestring(const double *xy, int n);                      /* LineString(pts); n == 0: LINESTRING EMPTY; n == 1: NULL (raises) */
GEOSGeometry *shp_linearring(const double *xy, int n);                      /* LinearRing(coords): closed if not closed; < 4 after closing: NULL */
GEOSGeometry *shp_polygon(const double *xy, int n);                         /* Polygon(coords), the ring closed as shapely closes it; n == 0: POLYGON EMPTY */
GEOSGeometry *shp_polygon_holes(const double *xy, int n, const double *const *hxy, const int *hn, int nh);   /* Polygon(shell, holes) */
GEOSGeometry *shp_polygon_from_ring(const GEOSGeometry *ring);              /* Polygon(p.exterior) (LinearRing or LineString) */
GEOSGeometry *shp_box(double minx, double miny, double maxx, double maxy, int ccw);   /* shapely.geometry.box */
GEOSGeometry *shp_multipoint(const double *xy, int n);                      /* MultiPoint(pts) / shapely.multipoints(pts); n == 0: MULTIPOINT EMPTY */
GEOSGeometry *shp_collection(int type, const GEOSGeometry *const *gs, int n);   /* MultiPolygon(list) etc.: the parts cloned, in order */

/* ---- buffer ---- */
enum { SHP_CAP_ROUND = 1, SHP_CAP_FLAT = 2, SHP_CAP_SQUARE = 3 };          /* shapely.BufferCapStyle */
enum { SHP_JOIN_ROUND = 1, SHP_JOIN_MITRE = 2, SHP_JOIN_BEVEL = 3 };       /* shapely.BufferJoinStyle */
/* g.buffer(distance, quad_segs=16, cap_style, join_style, mitre_limit=5.0, single_sided=False) (resolution= is quad_segs) */
GEOSGeometry *shp_buffer(const GEOSGeometry *g, double distance, int quad_segs, int cap, int join, double mitre_limit, int single_sided);
GEOSGeometry *shp_buffer_q(const GEOSGeometry *g, double distance, int quad_segs);   /* g.buffer(d, quad_segs), the rest default */
GEOSGeometry *shp_point_buffer(double x, double y, double r, int quad_segs);         /* Point(x, y).buffer(r, quad_segs) */
GEOSGeometry *shp_line_buffer(const double *xy, int n, double w, int cap, int join, int quad_segs, double mitre_limit);   /* LineString(pts).buffer(w, ...) */

/* ---- set operations ---- */
GEOSGeometry *shp_union_all(const GEOSGeometry *const *gs, int n);         /* unary_union(list) == shapely.union_all; NULL entries skipped like None */
GEOSGeometry *shp_union(const GEOSGeometry *a, const GEOSGeometry *b);     /* a.union(b), a | b */
GEOSGeometry *shp_difference(const GEOSGeometry *a, const GEOSGeometry *b);
GEOSGeometry *shp_intersection(const GEOSGeometry *a, const GEOSGeometry *b);
GEOSGeometry *shp_symdiff(const GEOSGeometry *a, const GEOSGeometry *b);   /* a.symmetric_difference(b) */

/* ---- predicates (a.pred(b)) ---- */
int shp_contains(const GEOSGeometry *a, const GEOSGeometry *b);
int shp_intersects(const GEOSGeometry *a, const GEOSGeometry *b);
int shp_within(const GEOSGeometry *a, const GEOSGeometry *b);
int shp_touches(const GEOSGeometry *a, const GEOSGeometry *b);
int shp_crosses(const GEOSGeometry *a, const GEOSGeometry *b);
int shp_overlaps(const GEOSGeometry *a, const GEOSGeometry *b);
int shp_covers(const GEOSGeometry *a, const GEOSGeometry *b);
int shp_covered_by(const GEOSGeometry *a, const GEOSGeometry *b);
int shp_disjoint(const GEOSGeometry *a, const GEOSGeometry *b);
int shp_equals(const GEOSGeometry *a, const GEOSGeometry *b);
int shp_dwithin(const GEOSGeometry *a, const GEOSGeometry *b, double d);   /* shapely.dwithin(a, b, d) */
int shp_is_empty(const GEOSGeometry *g);
int shp_is_valid(const GEOSGeometry *g);

/* ---- measurement ---- */
double shp_distance(const GEOSGeometry *a, const GEOSGeometry *b);         /* nan if either is empty */
double shp_area(const GEOSGeometry *g);
double shp_length(const GEOSGeometry *g);
void shp_bounds(const GEOSGeometry *g, double b[4]);                       /* (minx, miny, maxx, maxy); nan x4 for an empty */
double shp_project(const GEOSGeometry *line, const GEOSGeometry *pt, int normalized);   /* line.project(pt) */

/* ---- constructive ---- */
GEOSGeometry *shp_centroid(const GEOSGeometry *g);
GEOSGeometry *shp_representative_point(const GEOSGeometry *g);             /* == point_on_surface */
GEOSGeometry *shp_simplify(const GEOSGeometry *g, double tolerance, int preserve_topology);   /* g.simplify(tol, preserve_topology=True) */
GEOSGeometry *shp_convex_hull(const GEOSGeometry *g);
GEOSGeometry *shp_envelope(const GEOSGeometry *g);
GEOSGeometry *shp_boundary(const GEOSGeometry *g);
GEOSGeometry *shp_min_rot_rect(const GEOSGeometry *g);                     /* g.minimum_rotated_rectangle == oriented_envelope */
GEOSGeometry *shp_interpolate(const GEOSGeometry *line, double distance, int normalized);    /* line.interpolate(d) */
GEOSGeometry *shp_offset_curve(const GEOSGeometry *line, double distance, int quad_segs, int join, double mitre_limit);   /* line.offset_curve(d), defaults 8, round, 5.0 */
GEOSGeometry *shp_snap(const GEOSGeometry *a, const GEOSGeometry *b, double tolerance);     /* shapely.ops.snap */
GEOSGeometry *shp_orient(const GEOSGeometry *g, double sign);              /* shapely.geometry.polygon.orient(g, sign) == shapely.ops.orient */
GEOSGeometry *shp_voronoi(const GEOSGeometry *g, const GEOSGeometry *envelope, double tolerance, int edges);   /* shapely.ops.voronoi_diagram */
GEOSGeometry *shp_constrained_delaunay(const GEOSGeometry *g);             /* shapely.constrained_delaunay_triangles */
GEOSGeometry *shp_reverse(const GEOSGeometry *g);                          /* g.reverse() */

/* ---- access ---- */
int shp_type(const GEOSGeometry *g);                                       /* GEOS_POINT ... GEOS_GEOMETRYCOLLECTION (geom_type) */
int shp_num_geoms(const GEOSGeometry *g);                                  /* len(g.geoms) */
const GEOSGeometry *shp_geom_n(const GEOSGeometry *g, int i);              /* g.geoms[i], BORROWED (lives as long as g) */
const GEOSGeometry *shp_exterior(const GEOSGeometry *p);                   /* p.exterior, BORROWED; shp_clone it to keep */
int shp_num_interiors(const GEOSGeometry *p);                              /* len(p.interiors); 0 for an empty */
const GEOSGeometry *shp_interior(const GEOSGeometry *p, int i);            /* p.interiors[i], BORROWED */
int shp_coords(const GEOSGeometry *g, double *xy, int cap);                /* list(g.coords) of a point / line / ring: count (-1 if over cap or not linear) */
double shp_x(const GEOSGeometry *pt);                                      /* pt.x */
double shp_y(const GEOSGeometry *pt);                                      /* pt.y */
int shp_has_z(const GEOSGeometry *g);                                      /* g.has_z */

/* ---- affinity ---- */
enum { SHP_ORIGIN_CENTER = 0, SHP_ORIGIN_CENTROID = 1, SHP_ORIGIN_POINT = 2 };   /* origin='center' | 'centroid' | (x0, y0) or a Point */
GEOSGeometry *shp_affine(const GEOSGeometry *g, double a, double b, double d, double e, double xoff, double yoff);   /* affine_transform(g, [a, b, d, e, xoff, yoff]) */
GEOSGeometry *shp_rotate(const GEOSGeometry *g, double angle, int origin, double ox, double oy, int use_radians);
GEOSGeometry *shp_scale(const GEOSGeometry *g, double xfact, double yfact, int origin, double ox, double oy);
GEOSGeometry *shp_translate(const GEOSGeometry *g, double xoff, double yoff);

/* ---- prepared geometry (shapely.prepared.prep): the same answers as the plain predicates, faster when reused ---- */
typedef struct { const GEOSGeometry *g; const GEOSPreparedGeometry *p; } ShpPrep;   /* g is BORROWED: keep it alive */
ShpPrep shp_prep(const GEOSGeometry *g);
void shp_prep_free(ShpPrep *p);
int shp_prep_contains(const ShpPrep *p, const GEOSGeometry *o);
int shp_prep_intersects(const ShpPrep *p, const GEOSGeometry *o);
int shp_prep_covers(const ShpPrep *p, const GEOSGeometry *o);
int shp_prep_within(const ShpPrep *p, const GEOSGeometry *o);
int shp_prep_touches(const ShpPrep *p, const GEOSGeometry *o);
int shp_prep_contains_properly(const ShpPrep *p, const GEOSGeometry *o);

/* ---- growing lists ---- */
typedef struct { GEOSGeometry **g; int n, cap; } ShpList;                  /* owned geometries */
void shp_list_push(ShpList *l, GEOSGeometry *g);                           /* takes g over */
void shp_list_free(ShpList *l);                                            /* frees every geometry and the array */
typedef struct { int *i; double *d; int n, cap; } ShpIdx;                  /* tree indices (and distances) */
void shp_idx_free(ShpIdx *x);

/* ---- STRtree (shapely.STRtree) ---- */
enum { SHP_PRED_NONE = 0, SHP_PRED_INTERSECTS = 1, SHP_PRED_WITHIN = 2, SHP_PRED_CONTAINS = 3, SHP_PRED_OVERLAPS = 4,
       SHP_PRED_CROSSES = 5, SHP_PRED_TOUCHES = 6, SHP_PRED_COVERS = 7, SHP_PRED_COVERED_BY = 8, SHP_PRED_CONTAINS_PROPERLY = 9 };
typedef struct ShpTree ShpTree;
ShpTree *shp_tree_new(const GEOSGeometry *const *gs, int n, int node_capacity);   /* STRtree(geoms, node_capacity=10); gs BORROWED (keep them alive) */
void shp_tree_free(ShpTree *t);
int shp_tree_query(ShpTree *t, const GEOSGeometry *g, int predicate, ShpIdx *out);   /* tree.query(g, predicate): indices appended, in shapely's order; -1 = error */
/* tree.query(array_of_geoms, predicate): the two rows, input index into qi, tree index into ti, input by input */
int shp_tree_query_bulk(ShpTree *t, const GEOSGeometry *const *gs, int n, int predicate, ShpIdx *qi, ShpIdx *ti);
int shp_tree_query_dwithin(ShpTree *t, const GEOSGeometry *g, double distance, ShpIdx *out);   /* tree.query(g, predicate="dwithin", distance=d) */
/* tree.query_nearest(g, return_distance=True, exclusive, all_matches): (index, distance) pairs appended to out (out->d
   gets the distances); an empty or missing g appends nothing. Returns the number appended, -1 = error. */
int shp_tree_query_nearest(ShpTree *t, const GEOSGeometry *g, int exclusive, int all_matches, ShpIdx *out);

/* ---- the kit's own helpers (gg.py / gg3.py) ---- */
void shp_polys_of(const GEOSGeometry *g, ShpList *out);                    /* gg.polys_of(g): clones of every polygon, depth first */
GEOSGeometry *shp_inset(const GEOSGeometry *p, double d);                  /* gg.inset: p.buffer(-d, join_style="mitre", mitre_limit=10.0) */
int shp_wide_enough(const GEOSGeometry *p, double w, double ts);           /* gg.wide_enough(p, w) with gg._TS = ts (1.0 unless KIT_SCALE) */
GEOSGeometry *shp_rect(double cx, double cy, double l, double w, double a);   /* gg3.rect */
GEOSGeometry *shp_star(double cx, double cy, double ro, double ri, int n, double rot);   /* gg.star */
GEOSGeometry *shp_hexa(double cx, double cy, double r, double rot);        /* gg3.hexa */
GEOSGeometry *shp_ring(double cx, double cy, double ro, double ri, int res);   /* gg3.ring */
#endif
