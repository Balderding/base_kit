/* grain.h: fine grain on the flat tops of the height-field mesh (plan B step 2, 2026-11-21), so no open ground is
   ever smooth. Flat, upward faces are split until no edge is longer than about L mm (the faces beside them get the
   same points, so the mesh stays closed), then the points inside each flat area move up or down by value noise.
   The move fades out over 0.6 mm towards the area's edge: edges, walls and rims stay exactly where they were.
   2026-11-22 (review P2-1, P2-2, N1):
   - mask: a grid over the base (mw x mh cells of mstep mm from mx0,my0); a face whose middle is on a GT_SMOOTH cell is
     not grained (water, the tops of objects); it then counts as an edge, so the fade still works. NULL = all ground.
   - coarse: for FDM printers: points about 0.5 mm apart, two octaves 3 and 1.2 mm across (resin: 0.4 mm, three
     octaves down to 0.6 mm, as before).
   amp: the largest move in mm. seed: per base.
   Returns how many points moved, -1 when out of memory (the mesh is then still whole, maybe not grained). */
#ifndef GRAIN_H
#define GRAIN_H
#include <stdint.h>
/* review N2 (2026-11-22): textures. mask cells hold one of these (0 = the base's own, deftex); GT_SMOOTH = no grain.
   ang (may be NULL): per cell, a plank's direction in degrees 0..179. */
enum { GT_BASE = 0, GT_SOIL = 1, GT_MUD, GT_SAND, GT_GRAVEL, GT_ASH, GT_PLANK, GT_STONE, GT_SMOOTH = 255 };
typedef struct { double amp; int coarse; const unsigned char *mask; int mw, mh; double mx0, my0, mstep; int deftex; const unsigned char *ang; } GrainOpt;
int grain_apply(double **v, int *nv, int **f, int *nt, const GrainOpt *o, uint32_t seed);
#endif
