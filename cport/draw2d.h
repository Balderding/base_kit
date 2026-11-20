/* draw2d.h: mesher.draw2d (mesher._draw2d_pil, mesher.py) in C: the base seen from straight above as a colour drawing,
   pixel for pixel what PIL 13.0 draws (its polygon, line and ellipse rasterisers, masked paste and LANCZOS resize are
   copied from Pillow's libImaging: Draw.c, Paste.c, Resample.c, Convert.c), saved as a PNG.

   The scene data it draws from (D2Scene) is what _draw2d_pil reads off the scene: the ground colour, s.final_gcuts(),
   s.ops, s.tcuts, gg.R0 / H / SINK / BASE_MM, gg.oval() and mesher.scale_plan(s)[:4]. Until the scene is in C it comes
   over from Python as a "BBD1" blob (work\cscene\runner\capture.py, dump()):
       "BBD1", f64 R0 H SINK BASE_MM, u8 has_sp f64 sc fin tx ty, u8 has_oval f64 A B, str ground,
       u32 n x gcut {kind, u32 np, np x poly}, u32 n x op {u8 0 prism / 1 taper, f64 z0, f64 top (prism) or h (taper),
       kind, u32 np, np x poly}, u32 n x tcut {kind, u32 np, np x poly}
       str = u32 len + utf-8 bytes; kind = str + u8 (1 = a gg.LK) + str (its colour, "#rrggbb": the colour tables grow
       at import and per scene, so the colour comes with the scene);
       poly = u8 type (0 none or empty, 1 Polygon, 2 something else), type 1: u32 rings, each u32 n + n x 2 f64
       (shapely's ring coords, closed, exterior first). */
#ifndef DRAW2D_H
#define DRAW2D_H
#include <stddef.h>

typedef struct { int type; int nr; int *len; double *xy; } D2Poly;          /* type: 0 none/empty, 1 Polygon, 2 not a Polygon */
typedef struct { char *kind; int is_lk; char *col; int optype; double z0, b; int np; D2Poly *p; } D2Group;          /* col: gg.COL / gg.CUTC .get(kind, default) as the drawing looks it up; optype/z0/b: ops only */
typedef struct {
    double R0, H, SINK, BASE_MM;
    int has_sp; double sc, fin, tx, ty;          /* mesher.scale_plan(s)[:4] */
    int has_oval; double A, B;                   /* gg.oval() */
    char *ground;
    int ngc, nop, ntc; D2Group *gc, *op, *tc;    /* s.final_gcuts(), s.ops, s.tcuts */
} D2Scene;

int d2_read(const unsigned char *buf, size_t n, D2Scene *s);          /* 1 = ok */
void d2_scene_free(D2Scene *s);

/* the drawing as pixels: px x px, channels 4 (RGBA, transparent=1) or 3 (RGB), rows top down; *pix is malloc'd.
   win: mesher.PLAN_WIN (what the drawing shows, in the built base's mm). 1 = ok; 0 = Python would have raised
   (and fallen back to its matplotlib drawing): err says why. */
int d2_render(const D2Scene *s, int px, int transparent, unsigned char **pix, int *channels, double win[4], char *err, int errlen);
/* a PNG of 8-bit RGB (channels 3) or RGBA (4) pixels, deflate-compressed by our own small compressor. 1 = ok */
int d2_png_write(const char *path, const unsigned char *pix, int w, int h, int channels);
/* d2_render + d2_png_write: mesher.draw2d(s, png, px, transparent) */
int d2_draw(const D2Scene *s, const char *png, int px, int transparent, double win[4], char *err, int errlen);

/* ---- the PIL image steps the runner needs (tools\gui_job.py see_through), pixel-identical to PIL 13.0 ---- */
/* Image.resize((nw, nh), LANCZOS) of 8-bit RGB (ch 3) or RGBA (ch 4; premultiplied while resized, as PIL does): malloc'd */
unsigned char *d2_resize(const unsigned char *pix, int w, int h, int ch, int nw, int nh);
/* a PNG (8-bit gray, RGB, RGBA, gray+alpha; no palette, no interlace) as RGBA: malloc'd, NULL if it cannot */
unsigned char *d2_png_read_rgba(const char *path, int *w, int *h);
/* gui_job.see_through(src, dst, px): white round the picture made see-through (floodfill from the white corners,
   thresh 6), cropped to what is left (getbbox), thumbnail((px, px), LANCZOS), saved. 1 = ok; 0 = err says why */
int d2_see_through(const char *src, const char *dst, int px, char *err, int errlen);
#endif
