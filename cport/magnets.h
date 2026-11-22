/* magnets.h (2026-11-22): where the 6x2 magnet holes go, ONE copy for everyone (the CAD commands in buildcmds.c, the
   mesh builder in meshglue.c, the runner's checks in runner.c had three copies of gg.magnet_centres that could drift).
   Round base: 1 hole up to 40 mm, 2 up to 60, 3 up to 100, else 4 on a ring at a quarter of the size plus one in the
   middle. Oval (long side A): 1 up to 60.5, 2 up to 105.5, else 3 along the long side. xy gets x,y pairs; returns how many. */
#ifndef MAGNETS_H
#define MAGNETS_H
#include "pyx.h"
static inline int mag_centres(double dia, double *xy) {
    const double pi = 3.141592653589793;
    int n = dia <= 40 ? 1 : dia <= 60 ? 2 : dia <= 100 ? 3 : 4, k = 0;
    double r = dia * 0.25;
    if (n <= 1) { xy[0] = 0.0; xy[1] = 0.0; return 1; }
    for (int i = 0; i < n; i++) { double a = pi / 2 + 2 * pi * i / n; xy[2 * k] = py_round(r * py_cos(a), 4); xy[2 * k + 1] = py_round(r * py_sin(a), 4); k++; }
    if (dia > 100) { xy[2 * k] = 0.0; xy[2 * k + 1] = 0.0; k++; }
    return k;
}
static inline int mag_oval_centres(double A, double *xy) {
    int n = A <= 60.5 ? 1 : A <= 105.5 ? 2 : 3; double x = py_round(A * 0.25, 4);
    if (n == 1) { xy[0] = 0.0; xy[1] = 0.0; return 1; }
    if (n == 2) { xy[0] = -x; xy[1] = 0.0; xy[2] = x; xy[3] = 0.0; return 2; }
    xy[0] = -x; xy[1] = 0.0; xy[2] = 0.0; xy[3] = 0.0; xy[4] = x; xy[5] = 0.0; return 3;
}
#endif
