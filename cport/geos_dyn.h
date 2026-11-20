/* geos_dyn.h: the GEOS C API (reentrant, GEOS 3.14), loaded at run time from the very geos_c.dll that shapely ships,
   so every outline operation gives exactly what the Python kit gets. Call geos_load() once; then use G.<fn>(gctx, ...).

   Where the dll is looked for: BBB_GEOS (full path of geos_c*.dll), else geos\geos_c.dll beside the exe, else
   shapely.libs\geos_c-*.dll in the Python user site-packages. */
#ifndef GEOS_DYN_H
#define GEOS_DYN_H
#include <stddef.h>

typedef void *GEOSContextHandle_t;
typedef struct GEOSGeom_t GEOSGeometry;
typedef struct GEOSCoordSeq_t GEOSCoordSequence;
typedef struct GEOSSTRtree_t GEOSSTRtree;
typedef struct GEOSPrepGeom_t GEOSPreparedGeometry;
typedef void (*GEOSQueryCallback)(void *item, void *userdata);
typedef int (*GEOSDistanceCallback)(const void *item1, const void *item2, double *distance, void *userdata);
typedef void (*GEOSMessageHandler_r)(const char *message, void *userdata);

enum { GEOS_POINT, GEOS_LINESTRING, GEOS_LINEARRING, GEOS_POLYGON, GEOS_MULTIPOINT, GEOS_MULTILINESTRING, GEOS_MULTIPOLYGON, GEOS_GEOMETRYCOLLECTION };

#define GEOS_FNS(X) \
  X(GEOSContextHandle_t, GEOS_init_r, (void)) \
  X(void, GEOS_finish_r, (GEOSContextHandle_t)) \
  X(GEOSMessageHandler_r, GEOSContext_setErrorMessageHandler_r, (GEOSContextHandle_t, GEOSMessageHandler_r, void *)) \
  X(const char *, GEOSversion, (void)) \
  X(GEOSGeometry *, GEOSGeomFromWKB_buf_r, (GEOSContextHandle_t, const unsigned char *, size_t)) \
  X(void, GEOSGeom_destroy_r, (GEOSContextHandle_t, GEOSGeometry *)) \
  X(GEOSGeometry *, GEOSGeom_clone_r, (GEOSContextHandle_t, const GEOSGeometry *)) \
  X(GEOSGeometry *, GEOSGeom_setPrecision_r, (GEOSContextHandle_t, const GEOSGeometry *, double, int)) \
  X(char, GEOSisEmpty_r, (GEOSContextHandle_t, const GEOSGeometry *)) \
  X(char, GEOSisValid_r, (GEOSContextHandle_t, const GEOSGeometry *)) \
  X(int, GEOSArea_r, (GEOSContextHandle_t, const GEOSGeometry *, double *)) \
  X(int, GEOSDistance_r, (GEOSContextHandle_t, const GEOSGeometry *, const GEOSGeometry *, double *)) \
  X(char, GEOSContains_r, (GEOSContextHandle_t, const GEOSGeometry *, const GEOSGeometry *)) \
  X(GEOSGeometry *, GEOSBoundary_r, (GEOSContextHandle_t, const GEOSGeometry *)) \
  X(GEOSGeometry *, GEOSUnaryUnion_r, (GEOSContextHandle_t, const GEOSGeometry *)) \
  X(GEOSGeometry *, GEOSPolygonize_r, (GEOSContextHandle_t, const GEOSGeometry *const *, unsigned int)) \
  X(GEOSGeometry *, GEOSPointOnSurface_r, (GEOSContextHandle_t, const GEOSGeometry *)) \
  X(GEOSGeometry *, GEOSConstrainedDelaunayTriangulation_r, (GEOSContextHandle_t, const GEOSGeometry *)) \
  X(int, GEOSOrientPolygons_r, (GEOSContextHandle_t, GEOSGeometry *, int)) \
  X(int, GEOSGeomTypeId_r, (GEOSContextHandle_t, const GEOSGeometry *)) \
  X(int, GEOSGetNumGeometries_r, (GEOSContextHandle_t, const GEOSGeometry *)) \
  X(const GEOSGeometry *, GEOSGetGeometryN_r, (GEOSContextHandle_t, const GEOSGeometry *, int)) \
  X(const GEOSGeometry *, GEOSGetExteriorRing_r, (GEOSContextHandle_t, const GEOSGeometry *)) \
  X(int, GEOSGetNumInteriorRings_r, (GEOSContextHandle_t, const GEOSGeometry *)) \
  X(const GEOSGeometry *, GEOSGetInteriorRingN_r, (GEOSContextHandle_t, const GEOSGeometry *, int)) \
  X(const GEOSCoordSequence *, GEOSGeom_getCoordSeq_r, (GEOSContextHandle_t, const GEOSGeometry *)) \
  X(int, GEOSCoordSeq_getSize_r, (GEOSContextHandle_t, const GEOSCoordSequence *, unsigned int *)) \
  X(int, GEOSCoordSeq_getXY_r, (GEOSContextHandle_t, const GEOSCoordSequence *, unsigned int, double *, double *)) \
  X(GEOSCoordSequence *, GEOSCoordSeq_copyFromBuffer_r, (GEOSContextHandle_t, const double *, unsigned int, int, int)) \
  X(GEOSGeometry *, GEOSGeom_createLinearRing_r, (GEOSContextHandle_t, GEOSCoordSequence *)) \
  X(GEOSGeometry *, GEOSGeom_createPolygon_r, (GEOSContextHandle_t, GEOSGeometry *, GEOSGeometry **, unsigned int)) \
  X(GEOSGeometry *, GEOSGeom_createCollection_r, (GEOSContextHandle_t, int, GEOSGeometry **, unsigned int)) \
  X(GEOSGeometry *, GEOSGeom_createPointFromXY_r, (GEOSContextHandle_t, double, double)) \
  X(GEOSSTRtree *, GEOSSTRtree_create_r, (GEOSContextHandle_t, size_t)) \
  X(void, GEOSSTRtree_insert_r, (GEOSContextHandle_t, GEOSSTRtree *, const GEOSGeometry *, void *)) \
  X(void, GEOSSTRtree_query_r, (GEOSContextHandle_t, GEOSSTRtree *, const GEOSGeometry *, GEOSQueryCallback, void *)) \
  X(const void *, GEOSSTRtree_nearest_generic_r, (GEOSContextHandle_t, GEOSSTRtree *, const void *, const GEOSGeometry *, GEOSDistanceCallback, void *)) \
  X(void, GEOSSTRtree_destroy_r, (GEOSContextHandle_t, GEOSSTRtree *)) \
  X(GEOSGeometry *, GEOSGeom_createLineString_r, (GEOSContextHandle_t, GEOSCoordSequence *)) \
  X(const GEOSPreparedGeometry *, GEOSPrepare_r, (GEOSContextHandle_t, const GEOSGeometry *)) \
  X(int, GEOSPreparedDistance_r, (GEOSContextHandle_t, const GEOSPreparedGeometry *, const GEOSGeometry *, double *)) \
  X(void, GEOSPreparedGeom_destroy_r, (GEOSContextHandle_t, const GEOSPreparedGeometry *)) \
  X(GEOSGeometry *, GEOSIntersection_r, (GEOSContextHandle_t, const GEOSGeometry *, const GEOSGeometry *)) \
  X(GEOSGeometry *, GEOSDifference_r, (GEOSContextHandle_t, const GEOSGeometry *, const GEOSGeometry *)) \
  X(char, GEOSIntersects_r, (GEOSContextHandle_t, const GEOSGeometry *, const GEOSGeometry *)) \
  X(GEOSGeometry *, GEOSBufferWithStyle_r, (GEOSContextHandle_t, const GEOSGeometry *, double, int, int, int, double)) \
  X(int, GEOSGeom_getXMin_r, (GEOSContextHandle_t, const GEOSGeometry *, double *)) \
  X(int, GEOSGeom_getYMin_r, (GEOSContextHandle_t, const GEOSGeometry *, double *)) \
  X(int, GEOSGeom_getXMax_r, (GEOSContextHandle_t, const GEOSGeometry *, double *)) \
  X(int, GEOSGeom_getYMax_r, (GEOSContextHandle_t, const GEOSGeometry *, double *)) \
  X(GEOSGeometry *, GEOSConvexHull_r, (GEOSContextHandle_t, const GEOSGeometry *)) \
  X(GEOSGeometry *, GEOSTopologyPreserveSimplify_r, (GEOSContextHandle_t, const GEOSGeometry *, double)) \
  SHP_GEOS_FNS(X)

/* the functions the shapely layer (shp.c) needs on top of the ones above */
typedef struct GEOSBufParams_t GEOSBufferParams;
typedef struct GEOSWKBWriter_t GEOSWKBWriter;
#define SHP_GEOS_FNS(X) \
  X(GEOSBufferParams *, GEOSBufferParams_create_r, (GEOSContextHandle_t)) \
  X(void, GEOSBufferParams_destroy_r, (GEOSContextHandle_t, GEOSBufferParams *)) \
  X(int, GEOSBufferParams_setEndCapStyle_r, (GEOSContextHandle_t, GEOSBufferParams *, int)) \
  X(int, GEOSBufferParams_setJoinStyle_r, (GEOSContextHandle_t, GEOSBufferParams *, int)) \
  X(int, GEOSBufferParams_setMitreLimit_r, (GEOSContextHandle_t, GEOSBufferParams *, double)) \
  X(int, GEOSBufferParams_setQuadrantSegments_r, (GEOSContextHandle_t, GEOSBufferParams *, int)) \
  X(int, GEOSBufferParams_setSingleSided_r, (GEOSContextHandle_t, GEOSBufferParams *, int)) \
  X(GEOSGeometry *, GEOSBufferWithParams_r, (GEOSContextHandle_t, const GEOSGeometry *, const GEOSBufferParams *, double)) \
  X(GEOSGeometry *, GEOSUnion_r, (GEOSContextHandle_t, const GEOSGeometry *, const GEOSGeometry *)) \
  X(GEOSGeometry *, GEOSSymDifference_r, (GEOSContextHandle_t, const GEOSGeometry *, const GEOSGeometry *)) \
  X(GEOSGeometry *, GEOSGetCentroid_r, (GEOSContextHandle_t, const GEOSGeometry *)) \
  X(GEOSGeometry *, GEOSEnvelope_r, (GEOSContextHandle_t, const GEOSGeometry *)) \
  X(GEOSGeometry *, GEOSSimplify_r, (GEOSContextHandle_t, const GEOSGeometry *, double)) \
  X(GEOSGeometry *, GEOSMinimumRotatedRectangle_r, (GEOSContextHandle_t, const GEOSGeometry *)) \
  X(GEOSGeometry *, GEOSInterpolate_r, (GEOSContextHandle_t, const GEOSGeometry *, double)) \
  X(GEOSGeometry *, GEOSInterpolateNormalized_r, (GEOSContextHandle_t, const GEOSGeometry *, double)) \
  X(double, GEOSProject_r, (GEOSContextHandle_t, const GEOSGeometry *, const GEOSGeometry *)) \
  X(double, GEOSProjectNormalized_r, (GEOSContextHandle_t, const GEOSGeometry *, const GEOSGeometry *)) \
  X(GEOSGeometry *, GEOSOffsetCurve_r, (GEOSContextHandle_t, const GEOSGeometry *, double, int, int, double)) \
  X(GEOSGeometry *, GEOSSnap_r, (GEOSContextHandle_t, const GEOSGeometry *, const GEOSGeometry *, double)) \
  X(GEOSGeometry *, GEOSVoronoiDiagram_r, (GEOSContextHandle_t, const GEOSGeometry *, const GEOSGeometry *, double, int)) \
  X(int, GEOSLength_r, (GEOSContextHandle_t, const GEOSGeometry *, double *)) \
  X(int, GEOSGeom_getExtent_r, (GEOSContextHandle_t, const GEOSGeometry *, double *, double *, double *, double *)) \
  X(int, GEOSGeomGetX_r, (GEOSContextHandle_t, const GEOSGeometry *, double *)) \
  X(int, GEOSGeomGetY_r, (GEOSContextHandle_t, const GEOSGeometry *, double *)) \
  X(char, GEOSWithin_r, (GEOSContextHandle_t, const GEOSGeometry *, const GEOSGeometry *)) \
  X(char, GEOSTouches_r, (GEOSContextHandle_t, const GEOSGeometry *, const GEOSGeometry *)) \
  X(char, GEOSCrosses_r, (GEOSContextHandle_t, const GEOSGeometry *, const GEOSGeometry *)) \
  X(char, GEOSOverlaps_r, (GEOSContextHandle_t, const GEOSGeometry *, const GEOSGeometry *)) \
  X(char, GEOSCovers_r, (GEOSContextHandle_t, const GEOSGeometry *, const GEOSGeometry *)) \
  X(char, GEOSCoveredBy_r, (GEOSContextHandle_t, const GEOSGeometry *, const GEOSGeometry *)) \
  X(char, GEOSDisjoint_r, (GEOSContextHandle_t, const GEOSGeometry *, const GEOSGeometry *)) \
  X(char, GEOSEquals_r, (GEOSContextHandle_t, const GEOSGeometry *, const GEOSGeometry *)) \
  X(char, GEOSDistanceWithin_r, (GEOSContextHandle_t, const GEOSGeometry *, const GEOSGeometry *, double)) \
  X(char, GEOSPreparedContains_r, (GEOSContextHandle_t, const GEOSPreparedGeometry *, const GEOSGeometry *)) \
  X(char, GEOSPreparedContainsProperly_r, (GEOSContextHandle_t, const GEOSPreparedGeometry *, const GEOSGeometry *)) \
  X(char, GEOSPreparedIntersects_r, (GEOSContextHandle_t, const GEOSPreparedGeometry *, const GEOSGeometry *)) \
  X(char, GEOSPreparedWithin_r, (GEOSContextHandle_t, const GEOSPreparedGeometry *, const GEOSGeometry *)) \
  X(char, GEOSPreparedTouches_r, (GEOSContextHandle_t, const GEOSPreparedGeometry *, const GEOSGeometry *)) \
  X(char, GEOSPreparedCrosses_r, (GEOSContextHandle_t, const GEOSPreparedGeometry *, const GEOSGeometry *)) \
  X(char, GEOSPreparedOverlaps_r, (GEOSContextHandle_t, const GEOSPreparedGeometry *, const GEOSGeometry *)) \
  X(char, GEOSPreparedCovers_r, (GEOSContextHandle_t, const GEOSPreparedGeometry *, const GEOSGeometry *)) \
  X(char, GEOSPreparedCoveredBy_r, (GEOSContextHandle_t, const GEOSPreparedGeometry *, const GEOSGeometry *)) \
  X(char, GEOSPreparedDistanceWithin_r, (GEOSContextHandle_t, const GEOSPreparedGeometry *, const GEOSGeometry *, double)) \
  X(GEOSGeometry *, GEOSGeom_createEmptyPoint_r, (GEOSContextHandle_t)) \
  X(GEOSGeometry *, GEOSGeom_createEmptyLineString_r, (GEOSContextHandle_t)) \
  X(GEOSGeometry *, GEOSGeom_createEmptyPolygon_r, (GEOSContextHandle_t)) \
  X(GEOSGeometry *, GEOSGeom_createEmptyCollection_r, (GEOSContextHandle_t, int)) \
  X(GEOSCoordSequence *, GEOSCoordSeq_create_r, (GEOSContextHandle_t, unsigned int, unsigned int)) \
  X(int, GEOSCoordSeq_setXY_r, (GEOSContextHandle_t, GEOSCoordSequence *, unsigned int, double, double)) \
  X(int, GEOSCoordSeq_copyToBuffer_r, (GEOSContextHandle_t, const GEOSCoordSequence *, double *, int, int)) \
  X(void, GEOSCoordSeq_destroy_r, (GEOSContextHandle_t, GEOSCoordSequence *)) \
  X(int, GEOSGetNumCoordinates_r, (GEOSContextHandle_t, const GEOSGeometry *)) \
  X(char, GEOSHasZ_r, (GEOSContextHandle_t, const GEOSGeometry *)) \
  X(GEOSWKBWriter *, GEOSWKBWriter_create_r, (GEOSContextHandle_t)) \
  X(void, GEOSWKBWriter_destroy_r, (GEOSContextHandle_t, GEOSWKBWriter *)) \
  X(void, GEOSWKBWriter_setOutputDimension_r, (GEOSContextHandle_t, GEOSWKBWriter *, int)) \
  X(void, GEOSWKBWriter_setByteOrder_r, (GEOSContextHandle_t, GEOSWKBWriter *, int)) \
  X(void, GEOSWKBWriter_setFlavor_r, (GEOSContextHandle_t, GEOSWKBWriter *, int)) \
  X(void, GEOSWKBWriter_setIncludeSRID_r, (GEOSContextHandle_t, GEOSWKBWriter *, char)) \
  X(unsigned char *, GEOSWKBWriter_write_r, (GEOSContextHandle_t, GEOSWKBWriter *, const GEOSGeometry *, size_t *)) \
  X(GEOSGeometry *, GEOSReverse_r, (GEOSContextHandle_t, const GEOSGeometry *)) \
  X(void, GEOSFree_r, (GEOSContextHandle_t, void *))

#define GEOS_PTR(ret, name, args) ret (*name) args;
typedef struct { GEOS_FNS(GEOS_PTR) } GeosApi;
#undef GEOS_PTR

extern GeosApi G;
extern GEOSContextHandle_t gctx;
extern char geos_err[512];          /* the last GEOS error message (shapely would raise GEOSException with it) */
int geos_load(void);                /* 1 = ready; 0 = not found (message on stderr) */
#endif
