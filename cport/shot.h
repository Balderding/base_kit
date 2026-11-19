/* shot.h: mesher.shot() (the shaded 3D picture of a base) drawn by Direct3D 11: on the video card, or on Windows' own
   software renderer (WARP, all CPU cores) when there is no usable card. The same camera, light and shading as the
   Python one; only the filling in of the pixels moves. */
#ifndef SHOT_H
#define SHOT_H
/* V: nv x 3 doubles, F: nt x 3 ints. rgb: w*h*3 bytes out (rows top down). mode: 1 = video card (WARP if it fails),
   2 = WARP only. dev: which one drew it ("video card <name>" / "software (WARP)"). 1 = ok; 0 = failed (err says why). */
int shot_render(const double *V, int nv, const int *F, int nt, int w, int h, double elev, double azim, int mode,
                unsigned char *rgb, char *dev, int devlen, char *err, int errlen);
#endif
