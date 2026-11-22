/* grain.h: fine grain on the flat tops of the height-field mesh (plan B step 2, 2026-11-21), so no open ground is
   ever smooth. Flat, upward faces are split until no edge is longer than about 0.4 mm (the faces beside them get the
   same points, so the mesh stays closed), then the points inside each flat area move up or down by three octaves of
   value noise. The move fades out over 0.6 mm towards the area's edge: edges, walls and rims stay exactly where they were.
   amp: the largest move in mm (0.12 standard: 0.24 mm from the lowest to the highest point). seed: per base.
   Returns how many points moved, -1 when out of memory (the mesh is then still whole, maybe not grained). */
#ifndef GRAIN_H
#define GRAIN_H
#include <stdint.h>
int grain_apply(double **v, int *nv, int **f, int *nt, double amp, uint32_t seed);
#endif
