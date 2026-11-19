/* mesh.h: the C port of mesher.mesh_from_field (mesher.py), routine for routine. */
#ifndef MESH_H
#define MESH_H
#include "geos_dyn.h"

enum { R_FLAT = 0, R_STEP = 1, R_SLOPE = 2, R_BAND = 3 };
typedef struct {
    int kind;
    double z;                                 /* Flat, Step */
    GEOSGeometry *poly, *ring, *inner;        /* Slope: poly + its boundary; Band: poly, outer = its boundary, inner boundary */
    double z0, tan, cot, zmax;                /* Slope */
    double lo, hi, w;                         /* Band */
    double zb;                                /* Step: only the field builder (field.c) uses it */
} Rule;

typedef struct {
    double R0, H, rim_tan;
    int has_mag; double rm, dm; int ncens; double *cens;          /* magnet pockets */
    GEOSGeometry *disc;                                          /* F.disc */
    int has_oval; int ntr; double *tr; int nou; double *ou;      /* F.oval[2], F.oval[3] */
    int ncells; GEOSGeometry **cell; int *cell_rule;             /* F.cells: polygon, index of its rule */
    int nrules; Rule *rules;
} MeshIn;

typedef struct { int nv, nt; double *v; int *t; char err[256]; } MeshOut;

int mesh_read(const char *path, MeshIn *in);          /* reads a .min file (cport/dump_mesh.py); 1 = ok */
int mesh_read_buf(const unsigned char *buf, size_t n, MeshIn *in);          /* the same from memory */
int mesh_from_field(const MeshIn *in, MeshOut *out);  /* 1 = ok; 0 = what Python would have raised (out->err) */
void mesh_in_free(MeshIn *in);
#endif
