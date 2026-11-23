/* draw2d.c: mesher._draw2d_pil (mesher.py) in C, pixel-identical to PIL 13.0.
   The rasterisers are Pillow's own (libImaging, HPND licence: Copyright (c) 1997-2006 by Secret Labs AB, Copyright (c)
   1995-2006 by Fredrik Lundh, Copyright (c) 2010 by Alex Clark and contributors; "permission to use, copy, modify and
   distribute this software ... is hereby granted without fee, provided that the above copyright notice appear in all
   copies"), cut down to the image modes the drawing uses: L (1 byte a pixel) and RGB / RGBA (4 bytes a pixel, as Pillow
   stores them). Each function names the Pillow routine it copies. The float arithmetic is the same expression for
   expression (float where Pillow uses float), and sin / hypot are the UCRT's, the ones Pillow's _imaging.pyd calls.
   Build with -ffp-contract=off and no fast-math. */
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <math.h>
#include "draw2d.h"

/* ---------------------------------------------------------------- the UCRT's sin and hypot (as _imaging.pyd) */
typedef double (*f1_t)(double); typedef double (*f2_t)(double, double);
static f1_t u_sin; static f2_t u_hypot;
static int ucrt_load(void) {
    if (u_sin && u_hypot) return 1;
    HMODULE h = LoadLibraryA("ucrtbase.dll");
    if (!h) return 0;
    u_sin = (f1_t)(void *)GetProcAddress(h, "sin"); u_hypot = (f2_t)(void *)GetProcAddress(h, "_hypot");
    if (!u_hypot) u_hypot = (f2_t)(void *)GetProcAddress(h, "hypot");
    return u_sin && u_hypot;
}

/* ---------------------------------------------------------------- images */
typedef struct { int w, h, ps; unsigned char *p; } Img;          /* ps: 1 (L) or 4 (RGB, RGBA) bytes a pixel */
static Img *img_new(int w, int h, int ps, const unsigned char ink[4]) {          /* Image.new(mode, size, colour) */
    Img *m = malloc(sizeof *m); m->w = w; m->h = h; m->ps = ps; m->p = malloc((size_t)w * h * ps + 1);
    if (ps == 1) memset(m->p, ink[0], (size_t)w * h);
    else for (size_t i = 0; i < (size_t)w * h; i++) memcpy(m->p + 4 * i, ink, 4);
    return m;
}
static void img_free(Img *m) { if (m) { free(m->p); free(m); } }
static unsigned char *row8(const Img *m, int y) { return m->p + (size_t)y * m->w * m->ps; }

/* ImagingUtils.h */
#define SHIFTFORDIV255(a) ((((a) >> 8) + a) >> 8)
#define MULDIV255(a, b, tmp) (tmp = (a) * (b) + 128, SHIFTFORDIV255(tmp))
#define DIV255(a, tmp) (tmp = (a) + 128, SHIFTFORDIV255(tmp))
#define BLEND(mask, in1, in2, tmp1) DIV255(in1 * (255 - mask) + in2 * mask, tmp1)
#define CLIP8(v) ((v) <= 0 ? 0 : (v) < 256 ? (v) : 255)

/* ---------------------------------------------------------------- Draw.c */
#define ROUND_UP(f) ((int)((f) >= 0.0 ? floor((f) + 0.5F) : -floor(fabs(f) + 0.5F)))
#define ROUND_DOWN(f) ((int)((f) >= 0.0 ? ceil((f) - 0.5F) : -ceil(fabs(f) - 0.5F)))

typedef struct { int d; int x0, y0; int xmin, ymin, xmax, ymax; float dx; } Edge;

static void point(Img *im, int x, int y, uint32_t ink) {          /* point8 / point32 */
    if (x >= 0 && x < im->w && y >= 0 && y < im->h) {
        if (im->ps == 1) row8(im, y)[x] = (unsigned char)ink;
        else memcpy(row8(im, y) + 4 * x, &ink, 4);
    }
}
static void hline(Img *im, int x0, int y0, int x1, uint32_t ink) {          /* hline8 / hline32, no mask */
    if (y0 >= 0 && y0 < im->h) {
        if (x0 < 0) x0 = 0;
        else if (x0 >= im->w) return;
        if (x1 < 0) return;
        else if (x1 >= im->w) x1 = im->w - 1;
        if (x0 <= x1) {
            unsigned char *p = row8(im, y0);
            if (im->ps == 1) memset(p + x0, (unsigned char)ink, (size_t)(x1 - x0 + 1));
            else for (; x0 <= x1; x0++) memcpy(p + 4 * x0, &ink, 4);
        }
    }
}

static void line(Img *im, int x0, int y0, int x1, int y1, uint32_t ink) {          /* Draw.c line() */
    int i, n, e, dx, dy, xs, ys;
    dy = y1 - y0;
    if (dy < 0) dy = -dy, ys = -1;
    else if (dy == 0) {
        if (x1 > x0) hline(im, x0, y0, x1 - 1, ink);
        else if (x0 > x1) hline(im, x1 + 1, y0, x0, ink);
        return;
    } else ys = 1;
    dx = x1 - x0;
    if (dx < 0) dx = -dx, xs = -1;
    else xs = 1;
    n = (dx > dy) ? dx : dy; (void)n;
    if (dx == 0) {
        for (i = 0; i < dy; i++) { point(im, x0, y0, ink); y0 += ys; }
    } else if (dx > dy) {
        n = dx; dy += dy; e = dy - dx; dx += dx;
        for (i = 0; i < n; i++) { point(im, x0, y0, ink); if (e >= 0) { y0 += ys; e -= dx; } e += dy; x0 += xs; }
    } else {
        n = dy; dx += dx; e = dx - dy; dy += dy;
        for (i = 0; i < n; i++) { point(im, x0, y0, ink); if (e >= 0) { x0 += xs; e -= dy; } e += dx; y0 += ys; }
    }
}

static int x_cmp(const void *x0, const void *x1) {
    float diff = *((const float *)x0) - *((const float *)x1);
    return diff < 0 ? -1 : diff > 0 ? 1 : 0;
}

/* Draw.c polygon_generic, the path without alpha blending (draw32 / draw8: what an RGB or L image gets) */
static int polygon_generic(Img *im, int n, Edge *e, uint32_t ink) {
    Edge **edge_table; float *xx; int edge_count = 0; int ymin = im->h - 1; int ymax = 0; int i, j, k;
    float adjacent_line_x, adjacent_line_x_other_edge;
    if (n <= 0) return 0;
    edge_table = calloc(n, sizeof(Edge *));
    if (!edge_table) return -1;
    for (i = 0; i < n; i++) {
        if (ymin > e[i].ymin) ymin = e[i].ymin;
        if (ymax < e[i].ymax) ymax = e[i].ymax;
        if (e[i].ymin == e[i].ymax) { hline(im, e[i].xmin, e[i].ymin, e[i].xmax, ink); continue; }
        edge_table[edge_count++] = (e + i);
    }
    if (ymin < 0) ymin = 0;
    if (ymax > im->h) ymax = im->h;
    xx = calloc(edge_count * 2 + 1, sizeof(float));
    if (!xx) { free(edge_table); return -1; }
    for (; ymin <= ymax; ymin++) {
        j = 0;
        for (i = 0; i < edge_count; i++) {
            Edge *current = edge_table[i];
            if (ymin >= current->ymin && ymin <= current->ymax) {
                xx[j++] = (ymin - current->y0) * current->dx + current->x0;
                if (ymin == current->ymax && ymin < ymax) {
                    xx[j] = xx[j - 1]; j++;
                } else if ((ymin == current->ymin || ymin == current->ymax) && current->dx != 0) {
                    for (k = 0; k < i; k++) {
                        Edge *other_edge = edge_table[k];
                        if ((ymin != other_edge->ymin && ymin != other_edge->ymax) || other_edge->dx == 0) continue;
                        if (roundf(xx[j - 1]) == roundf((ymin - other_edge->y0) * other_edge->dx + other_edge->x0)) {
                            int offset = ymin == current->ymax ? -1 : 1;
                            adjacent_line_x = (ymin + offset - current->y0) * current->dx + current->x0;
                            if (ymin + offset >= other_edge->ymin && ymin + offset <= other_edge->ymax) {
                                adjacent_line_x_other_edge = (ymin + offset - other_edge->y0) * other_edge->dx + other_edge->x0;
                                if (xx[j - 1] > adjacent_line_x + 1 && xx[j - 1] > adjacent_line_x_other_edge + 1)
                                    xx[j - 1] = roundf(fmax(adjacent_line_x, adjacent_line_x_other_edge)) + 1;
                                else if (xx[j - 1] < adjacent_line_x - 1 && xx[j - 1] < adjacent_line_x_other_edge - 1)
                                    xx[j - 1] = roundf(fmin(adjacent_line_x, adjacent_line_x_other_edge)) - 1;
                                break;
                            }
                        }
                    }
                }
            }
        }
        qsort(xx, j, sizeof(float), x_cmp);
        for (i = 1; i < j; i += 2) hline(im, ROUND_UP(xx[i - 1]), ymin, ROUND_DOWN(xx[i]), ink);
    }
    free(xx); free(edge_table);
    return 0;
}

static void add_edge(Edge *e, int x0, int y0, int x1, int y1) {          /* Draw.c add_edge */
    if (x0 <= x1) e->xmin = x0, e->xmax = x1; else e->xmin = x1, e->xmax = x0;
    if (y0 <= y1) e->ymin = y0, e->ymax = y1; else e->ymin = y1, e->ymax = y0;
    if (y0 == y1) { e->d = 0; e->dx = 0.0; }
    else { e->dx = ((float)(x1 - x0)) / (y1 - y0); e->d = y0 == e->ymin ? 1 : -1; }
    e->x0 = x0; e->y0 = y0;
}

/* ImageDraw.polygon(xy, fill=ink): _imaging.c _draw_polygon ((int) of every coordinate) + ImagingDrawPolygon, fill */
static int draw_polygon_fill(Img *im, int count, const double *dxy, uint32_t ink) {
    int i, n, x0, y0, x1, y1;
    if (count < 2) return -1;          /* "coordinate list must contain at least 2 coordinates" */
    int *xy = malloc(sizeof(int) * 2 * count);
    for (i = 0; i < 2 * count; i++) xy[i] = (int)dxy[i];
    Edge *e = calloc(count, sizeof(Edge));
    for (i = n = 0; i < count - 1; i++) {
        x0 = xy[i * 2]; y0 = xy[i * 2 + 1]; x1 = xy[i * 2 + 2]; y1 = xy[i * 2 + 3];
        if (y0 == y1 && i != 0 && y0 == xy[i * 2 - 1]) {
            Edge *last_e = &e[n - 1];
            if (x1 > x0 && x0 > xy[i * 2 - 2]) { last_e->xmax = x1; continue; }
            else if (x1 < x0 && x0 < xy[i * 2 - 2]) { last_e->xmin = x1; continue; }
        }
        add_edge(&e[n++], x0, y0, x1, y1);
    }
    if (xy[i * 2] != xy[0] || xy[i * 2 + 1] != xy[1]) add_edge(&e[n++], xy[i * 2], xy[i * 2 + 1], xy[0], xy[1]);
    int r = polygon_generic(im, n, e, ink);
    free(e); free(xy);
    return r;
}

/* ImagingDrawWideLine */
static void wide_line(Img *im, int x0, int y0, int x1, int y1, uint32_t ink, int width) {
    int dx, dy; double big_hypotenuse, small_hypotenuse, ratio_max, ratio_min; int dxmin, dxmax, dymin, dymax; Edge e[4];
    dx = x1 - x0; dy = y1 - y0;
    if (dx == 0 && dy == 0) { point(im, x0, y0, ink); return; }
    big_hypotenuse = u_hypot(dx, dy);
    small_hypotenuse = (width - 1) / 2.0;
    ratio_max = ROUND_UP(small_hypotenuse) / big_hypotenuse;
    ratio_min = ROUND_DOWN(small_hypotenuse) / big_hypotenuse;
    dxmin = ROUND_DOWN(ratio_min * dy); dxmax = ROUND_DOWN(ratio_max * dy);
    dymin = ROUND_DOWN(ratio_min * dx); dymax = ROUND_DOWN(ratio_max * dx);
    int v[4][2] = { {x0 - dxmin, y0 + dymax}, {x1 - dxmin, y1 + dymax}, {x1 + dxmax, y1 - dymin}, {x0 + dxmax, y0 - dymin} };
    add_edge(e + 0, v[0][0], v[0][1], v[1][0], v[1][1]);
    add_edge(e + 1, v[1][0], v[1][1], v[2][0], v[2][1]);
    add_edge(e + 2, v[2][0], v[2][1], v[3][0], v[3][1]);
    add_edge(e + 3, v[3][0], v[3][1], v[0][0], v[0][1]);
    polygon_generic(im, 4, e, ink);
}

/* ImageDraw.line(xy, fill=ink, width): _imaging.c _draw_lines */
static void draw_lines(Img *im, int n, const double *xy, uint32_t ink, int width) {
    int i;
    if (width == 1) {
        const double *p = NULL;
        for (i = 0; i < n - 1; i++) { p = &xy[i + i]; line(im, (int)p[0], (int)p[1], (int)p[2], (int)p[3], ink); }
        if (p) point(im, (int)p[2], (int)p[3], ink);
    } else {
        for (i = 0; i < n - 1; i++) { const double *p = &xy[i + i]; wide_line(im, (int)p[0], (int)p[1], (int)p[2], (int)p[3], ink, width); }
    }
}

/* Draw.c quarter_* / ellipse_* / ellipseNew */
typedef struct { int32_t a, b, cx, cy, ex, ey; int64_t a2, b2, a2b2; int8_t finished; } quarter_state;
static void quarter_init(quarter_state *s, int32_t a, int32_t b) {
    if (a < 0 || b < 0) s->finished = 1;
    else { s->a = a; s->b = b; s->cx = a; s->cy = b % 2; s->ex = a % 2; s->ey = b; s->a2 = (int64_t)a * a; s->b2 = (int64_t)b * b; s->a2b2 = s->a2 * s->b2; s->finished = 0; }
}
static int64_t quarter_delta(quarter_state *s, int64_t x, int64_t y) { return llabs(s->a2 * y * y + s->b2 * x * x - s->a2b2); }
static int8_t quarter_next(quarter_state *s, int32_t *ret_x, int32_t *ret_y) {
    if (s->finished) return -1;
    *ret_x = s->cx; *ret_y = s->cy;
    if (s->cx == s->ex && s->cy == s->ey) s->finished = 1;
    else {
        int32_t nx = s->cx; int32_t ny = s->cy + 2; int64_t ndelta = quarter_delta(s, nx, ny);
        if (nx > 1) {
            int64_t newdelta = quarter_delta(s, s->cx - 2, s->cy + 2);
            if (ndelta > newdelta) { nx = s->cx - 2; ny = s->cy + 2; ndelta = newdelta; }
            newdelta = quarter_delta(s, s->cx - 2, s->cy);
            if (ndelta > newdelta) { nx = s->cx - 2; ny = s->cy; }
        }
        s->cx = nx; s->cy = ny;
    }
    return 0;
}
typedef struct { quarter_state st_o, st_i; int32_t py, pl, pr; int32_t cy[4], cl[4], cr[4]; int8_t bufcnt; int8_t finished; int8_t leftmost; } ellipse_state;
static void ellipse_init(ellipse_state *s, int32_t a, int32_t b, int32_t w) {
    s->bufcnt = 0; s->leftmost = a % 2;
    quarter_init(&s->st_o, a, b);
    if (w < 1 || quarter_next(&s->st_o, &s->pr, &s->py) == -1) s->finished = 1;
    else { s->finished = 0; quarter_init(&s->st_i, a - 2 * (w - 1), b - 2 * (w - 1)); s->pl = s->leftmost; }
}
static int8_t ellipse_next(ellipse_state *s, int32_t *ret_x0, int32_t *ret_y, int32_t *ret_x1) {
    if (s->bufcnt == 0) {
        if (s->finished) return -1;
        int32_t y = s->py; int32_t l = s->pl; int32_t r = s->pr; int32_t cx = 0, cy = 0; int8_t next_ret;
        while ((next_ret = quarter_next(&s->st_o, &cx, &cy)) != -1 && cy <= y) {}
        if (next_ret == -1) s->finished = 1;
        else { s->pr = cx; s->py = cy; }
        while ((next_ret = quarter_next(&s->st_i, &cx, &cy)) != -1 && cy <= y) l = cx;
        s->pl = next_ret == -1 ? s->leftmost : cx;
        if ((l > 0 || l < r) && y > 0) { s->cl[s->bufcnt] = l == 0 ? 2 : l; s->cy[s->bufcnt] = y; s->cr[s->bufcnt] = r; ++s->bufcnt; }
        if (y > 0) { s->cl[s->bufcnt] = -r; s->cy[s->bufcnt] = y; s->cr[s->bufcnt] = -l; ++s->bufcnt; }
        if (l > 0 || l < r) { s->cl[s->bufcnt] = l == 0 ? 2 : l; s->cy[s->bufcnt] = -y; s->cr[s->bufcnt] = r; ++s->bufcnt; }
        s->cl[s->bufcnt] = -r; s->cy[s->bufcnt] = -y; s->cr[s->bufcnt] = -l; ++s->bufcnt;
    }
    --s->bufcnt;
    *ret_x0 = s->cl[s->bufcnt]; *ret_y = s->cy[s->bufcnt]; *ret_x1 = s->cr[s->bufcnt];
    return 0;
}
/* ImageDraw.ellipse(box, fill=ink) (fill=1) or ImageDraw.ellipse(box, outline=ink, width=w) (fill=0): _draw_ellipse + ellipseNew */
static void draw_ellipse(Img *im, const double box[4], uint32_t ink, int fill, int width) {
    int x0 = (int)box[0], y0 = (int)box[1], x1 = (int)box[2], y1 = (int)box[3];
    int a = x1 - x0, b = y1 - y0;
    if (a < 0 || b < 0) return;
    if (fill) width = a + b;
    ellipse_state st; ellipse_init(&st, a, b, width);
    int32_t X0, Y, X1;
    while (ellipse_next(&st, &X0, &Y, &X1) != -1) hline(im, x0 + (X0 + a) / 2, y0 + (Y + b) / 2, x0 + (X1 + a) / 2, ink);
}

/* a square or rectangle base (2026-11-22): its box filled, or its outline width px wide; the picture is too small for
   the 1.5 mm corners to matter */
static void draw_box(Img *im, const double box[4], uint32_t ink, int fill, int width) {
    int x0 = (int)box[0], y0 = (int)box[1], x1 = (int)box[2], y1 = (int)box[3];
    for (int y = y0; y <= y1; y++) {
        if (fill || y < y0 + width || y > y1 - width) hline(im, x0, y, x1, ink);
        else { hline(im, x0, y, x0 + width - 1, ink); hline(im, x1 - width + 1, y, x1, ink); }
    }
}
#define SHAPE(im, e, ink, fill, width) do { if (s->has_oval == 2 && ring) draw_box(im, e, ink, fill, width); else draw_ellipse(im, e, ink, fill, width); } while (0)

/* ---------------------------------------------------------------- Paste.c */
/* Image.paste(colour, (x0, y0, x0 + mask.w, y0 + mask.h), mask) with an L mask: ImagingFill2 + fill_mask_L */
static void fill_mask(Img *out, const unsigned char ink[4], const Img *mask, int dx0, int dy0) {
    int64_t xsize = mask->w, ysize = mask->h; int sx0 = 0, sy0 = 0; unsigned int tmp1;
    if (dx0 < 0) { xsize += dx0; if (xsize <= 0) return; sx0 = -dx0, dx0 = 0; }
    if (dx0 + xsize > out->w) xsize = out->w - dx0;
    if (dy0 < 0) { ysize += dy0; if (ysize <= 0) return; sy0 = -dy0, dy0 = 0; }
    if (dy0 + ysize > out->h) ysize = out->h - dy0;
    if (xsize <= 0 || ysize <= 0) return;
    for (int y = 0; y < ysize; y++) {
        unsigned char *o = row8(out, y + dy0) + dx0 * out->ps; const unsigned char *m = row8(mask, y + sy0) + sx0;
        for (int x = 0; x < xsize; x++) {
            for (int i = 0; i < out->ps; i++) o[i] = (unsigned char)BLEND(*m, o[i], ink[i], tmp1);
            o += out->ps; m++;
        }
    }
}
/* Image.paste(im, (dx0, dy0), mask) (mask NULL: no mask): ImagingPaste + paste / paste_mask_L (4 bytes a pixel) */
static void paste(Img *out, const Img *in, const Img *mask, int dx0, int dy0) {
    int64_t xsize = in->w, ysize = in->h; int sx0 = 0, sy0 = 0; unsigned int tmp1;
    if (dx0 < 0) xsize += dx0, sx0 = -dx0, dx0 = 0;
    if (dx0 + xsize > out->w) xsize = out->w - dx0;
    if (dy0 < 0) ysize += dy0, sy0 = -dy0, dy0 = 0;
    if (dy0 + ysize > out->h) ysize = out->h - dy0;
    if (xsize <= 0 || ysize <= 0) return;
    for (int y = 0; y < ysize; y++) {
        unsigned char *o = row8(out, y + dy0) + dx0 * 4; const unsigned char *s = row8(in, y + sy0) + sx0 * 4;
        if (!mask) { memcpy(o, s, (size_t)xsize * 4); continue; }
        const unsigned char *m = row8(mask, y + sy0) + sx0;
        for (int x = 0; x < xsize; x++) {
            unsigned char a = m[0];
            o[0] = (unsigned char)BLEND(a, o[0], s[0], tmp1); o[1] = (unsigned char)BLEND(a, o[1], s[1], tmp1);
            o[2] = (unsigned char)BLEND(a, o[2], s[2], tmp1); o[3] = (unsigned char)BLEND(a, o[3], s[3], tmp1);
            o += 4; s += 4; m++;
        }
    }
}
static Img *crop(const Img *im, int x0, int y0, int x1, int y1) {          /* Image.crop, inside the image */
    unsigned char z[4] = { 0, 0, 0, 0 }; Img *c = img_new(x1 - x0, y1 - y0, im->ps, z);
    for (int y = y0; y < y1; y++) memcpy(row8(c, y - y0), row8(im, y) + x0 * im->ps, (size_t)(x1 - x0) * im->ps);
    return c;
}

/* ---------------------------------------------------------------- Resample.c (LANCZOS, 8 bits a channel) */
#define PRECISION_BITS (32 - 8 - 2)
static double sinc_filter(double x) { if (x == 0.0) return 1.0; x = x * M_PI; return u_sin(x) / x; }
static double lanczos_filter(double x) { if (-3.0 <= x && x < 3.0) return sinc_filter(x) * sinc_filter(x / 3); return 0.0; }
static unsigned char clip8(int in) { int v = in >> PRECISION_BITS; return (unsigned char)(v < 0 ? 0 : v > 255 ? 255 : v); }

static int precompute_coeffs(int inSize, float in0, float in1, int outSize, int **boundsp, double **kkp) {
    double support, scale, filterscale; int xx, x, ksize, xmin, xmax; int *bounds; double *kk, *k;
    filterscale = scale = (double)(in1 - in0) / outSize;
    if (filterscale < 1.0) filterscale = 1.0;
    support = 3.0 * filterscale;
    ksize = (int)ceil(support) * 2 + 1;
    kk = malloc(outSize * ksize * sizeof(double)); bounds = malloc(outSize * 2 * sizeof(int));
    double inv_filterscale = 1.0 / filterscale;
    for (xx = 0; xx < outSize; xx++) {
        double center = in0 + (xx + 0.5) * scale; double ww = 0.0;
        xmin = (int)(center - support + 0.5); if (xmin < 0) xmin = 0;
        xmax = (int)(center + support + 0.5); if (xmax > inSize) xmax = inSize;
        xmax -= xmin;
        k = &kk[xx * ksize];
        for (x = 0; x < xmax; x++) { double w = lanczos_filter((x + xmin - center + 0.5) * inv_filterscale); k[x] = w; ww += w; }
        if (ww != 0.0) for (x = 0; x < xmax; x++) k[x] /= ww;
        for (; x < ksize; x++) k[x] = 0;
        bounds[xx * 2 + 0] = xmin; bounds[xx * 2 + 1] = xmax;
    }
    *boundsp = bounds; *kkp = kk;
    return ksize;
}
static void normalize_coeffs_8bpc(int outSize, int ksize, double *prekk) {
    int32_t *kk = (int32_t *)prekk;
    for (int x = 0; x < outSize * ksize; x++) {
        if (prekk[x] < 0) kk[x] = (int)(-0.5 + prekk[x] * (1 << PRECISION_BITS));
        else kk[x] = (int)(0.5 + prekk[x] * (1 << PRECISION_BITS));
    }
}
/* bands: 3 (RGB: channel 3 comes out 0) or 4 */
static void resample_h(Img *out, const Img *in, int offset, int ksize, int *bounds, double *prekk, int bands) {
    int32_t *kk = (int32_t *)prekk; normalize_coeffs_8bpc(out->w, ksize, prekk);
    for (int yy = 0; yy < out->h; yy++) {
        const unsigned char *li = row8(in, yy + offset); unsigned char *lo = row8(out, yy);
        for (int xx = 0; xx < out->w; xx++) {
            int xmin = bounds[xx * 2], xmax = bounds[xx * 2 + 1]; int32_t *k = &kk[xx * ksize];
            int ss0, ss1, ss2, ss3; ss0 = ss1 = ss2 = ss3 = 1 << (PRECISION_BITS - 1);
            for (int x = 0; x < xmax; x++) {
                ss0 += li[(x + xmin) * 4 + 0] * k[x]; ss1 += li[(x + xmin) * 4 + 1] * k[x]; ss2 += li[(x + xmin) * 4 + 2] * k[x];
                if (bands == 4) ss3 += li[(x + xmin) * 4 + 3] * k[x];
            }
            lo[xx * 4 + 0] = clip8(ss0); lo[xx * 4 + 1] = clip8(ss1); lo[xx * 4 + 2] = clip8(ss2); lo[xx * 4 + 3] = bands == 4 ? clip8(ss3) : 0;
        }
    }
}
static void resample_v(Img *out, const Img *in, int offset, int ksize, int *bounds, double *prekk, int bands) {
    int32_t *kk = (int32_t *)prekk; normalize_coeffs_8bpc(out->h, ksize, prekk); (void)offset;
    for (int yy = 0; yy < out->h; yy++) {
        unsigned char *lo = row8(out, yy); int32_t *k = &kk[yy * ksize]; int ymin = bounds[yy * 2], ymax = bounds[yy * 2 + 1];
        for (int xx = 0; xx < out->w; xx++) {
            int ss0, ss1, ss2, ss3; ss0 = ss1 = ss2 = ss3 = 1 << (PRECISION_BITS - 1);
            for (int y = 0; y < ymax; y++) {
                const unsigned char *li = row8(in, y + ymin);
                ss0 += li[xx * 4 + 0] * k[y]; ss1 += li[xx * 4 + 1] * k[y]; ss2 += li[xx * 4 + 2] * k[y];
                if (bands == 4) ss3 += li[xx * 4 + 3] * k[y];
            }
            lo[xx * 4 + 0] = clip8(ss0); lo[xx * 4 + 1] = clip8(ss1); lo[xx * 4 + 2] = clip8(ss2); lo[xx * 4 + 3] = bands == 4 ? clip8(ss3) : 0;
        }
    }
}
/* Image.resize((xsize, ysize), LANCZOS) of a 4-byte image: ImagingResampleInner with box (0, 0, w, h) */
static Img *resize_lanczos(const Img *in, int xsize, int ysize, int bands) {
    unsigned char z[4] = { 0, 0, 0, 0 };
    float box[4] = { 0, 0, (float)in->w, (float)in->h };
    int need_h = xsize != in->w || box[0] || box[2] != xsize, need_v = ysize != in->h || box[1] || box[3] != ysize;
    int horizontal_first = !((in->h - ysize) > 0 && (in->h - ysize) > (in->w - xsize) * 2);
    int ksize_h = 0, ksize_v = 0, *bounds_h = NULL, *bounds_v = NULL, ybox_first = 0, ybox_last = 0; double *kk_h = NULL, *kk_v = NULL;
    if ((need_h && horizontal_first) || need_v) ksize_v = precompute_coeffs(in->h, box[1], box[3], ysize, &bounds_v, &kk_v);
    if (need_h) {
        if (horizontal_first) {
            ybox_first = bounds_v[0]; ybox_last = bounds_v[ysize * 2 - 2] + bounds_v[ysize * 2 - 1];
            if (ybox_first != 0) for (int i = 0; i < ysize; i++) bounds_v[i * 2] -= ybox_first;
        }
        ksize_h = precompute_coeffs(in->w, box[0], box[2], xsize, &bounds_h, &kk_h);
    }
    const Img *cur = in; Img *tmp = NULL, *nxt;
    if (horizontal_first) {
        if (need_h) { nxt = img_new(xsize, ybox_last - ybox_first, 4, z); resample_h(nxt, cur, ybox_first, ksize_h, bounds_h, kk_h, bands); tmp = nxt; cur = nxt; }
        if (need_v) { nxt = img_new(xsize, ysize, 4, z); resample_v(nxt, cur, 0, ksize_v, bounds_v, kk_v, bands); img_free(tmp); tmp = nxt; cur = nxt; }
    } else {
        if (need_v) { nxt = img_new(cur->w, ysize, 4, z); resample_v(nxt, cur, 0, ksize_v, bounds_v, kk_v, bands); tmp = nxt; cur = nxt; }
        if (need_h) { nxt = img_new(xsize, ysize, 4, z); resample_h(nxt, cur, 0, ksize_h, bounds_h, kk_h, bands); img_free(tmp); tmp = nxt; cur = nxt; }
    }
    free(bounds_h); free(kk_h); free(bounds_v); free(kk_v);
    if (!tmp) { tmp = img_new(in->w, in->h, 4, z); memcpy(tmp->p, in->p, (size_t)in->w * in->h * 4); }          /* same size: Image.copy() */
    return tmp;
}

/* ---------------------------------------------------------------- the scene: reading the BBD1 blob */
typedef struct { const unsigned char *p, *e; int bad; } Rd;
static void rd(Rd *r, void *dst, size_t n) { if (r->bad || (size_t)(r->e - r->p) < n) { r->bad = 1; memset(dst, 0, n); return; } memcpy(dst, r->p, n); r->p += n; }
static double rdf(Rd *r) { double v; rd(r, &v, 8); return v; }
static uint32_t rdu(Rd *r) { uint32_t v; rd(r, &v, 4); return v; }
static int rdb(Rd *r) { unsigned char v; rd(r, &v, 1); return v; }
static char *rds(Rd *r) { uint32_t n = rdu(r); if (r->bad || n > (size_t)(r->e - r->p)) { r->bad = 1; return calloc(1, 1); } char *s = malloc(n + 1); rd(r, s, n); s[n] = 0; return s; }
static void rdpoly(Rd *r, D2Poly *p) {
    memset(p, 0, sizeof *p); p->type = rdb(r);
    if (p->type != 1) return;
    p->nr = (int)rdu(r); if (r->bad || p->nr > 1000000) { r->bad = 1; p->nr = 0; return; }
    p->len = calloc(p->nr + 1, sizeof(int)); size_t tot = 0; const unsigned char *save;
    int *off = calloc(p->nr + 1, sizeof(int)); (void)off; free(off);
    save = r->p;
    for (int i = 0; i < p->nr; i++) { uint32_t n = rdu(r); if (r->bad || (size_t)(r->e - r->p) < 16 * (size_t)n) { r->bad = 1; return; } p->len[i] = (int)n; tot += n; r->p += 16 * (size_t)n; }
    r->p = save; p->xy = malloc(16 * (tot + 1)); size_t at = 0;
    for (int i = 0; i < p->nr; i++) { uint32_t n = rdu(r); rd(r, p->xy + 2 * at, 16 * (size_t)n); at += n; }
}
static void rdgroup(Rd *r, D2Group *g, int is_op) {
    memset(g, 0, sizeof *g);
    if (is_op) { g->optype = rdb(r); g->z0 = rdf(r); g->b = rdf(r); }
    g->kind = rds(r); g->is_lk = rdb(r); g->col = rds(r); g->np = (int)rdu(r);
    if (r->bad || g->np > 10000000) { r->bad = 1; g->np = 0; return; }
    g->p = calloc(g->np + 1, sizeof(D2Poly));
    for (int i = 0; i < g->np && !r->bad; i++) rdpoly(r, &g->p[i]);
}
static D2Group *rdgroups(Rd *r, int *n, int is_op) {
    *n = (int)rdu(r); if (r->bad || *n > 10000000) { r->bad = 1; *n = 0; return NULL; }
    D2Group *g = calloc(*n + 1, sizeof(D2Group));
    for (int i = 0; i < *n && !r->bad; i++) rdgroup(r, &g[i], is_op);
    return g;
}
int d2_read(const unsigned char *buf, size_t n, D2Scene *s) {
    Rd r = { buf, buf + n, 0 }; memset(s, 0, sizeof *s);
    if (n < 4 || memcmp(buf, "BBD1", 4)) return 0;
    r.p += 4;
    s->R0 = rdf(&r); s->H = rdf(&r); s->SINK = rdf(&r); s->BASE_MM = rdf(&r);
    s->has_sp = rdb(&r); s->sc = rdf(&r); s->fin = rdf(&r); s->tx = rdf(&r); s->ty = rdf(&r);
    s->has_oval = rdb(&r); s->A = rdf(&r); s->B = rdf(&r);
    s->ground = rds(&r);
    s->gc = rdgroups(&r, &s->ngc, 0); s->op = rdgroups(&r, &s->nop, 1); s->tc = rdgroups(&r, &s->ntc, 0);
    return !r.bad;
}
static void free_groups(D2Group *g, int n) {
    for (int i = 0; i < n; i++) { for (int j = 0; j < g[i].np; j++) { free(g[i].p[j].len); free(g[i].p[j].xy); } free(g[i].p); free(g[i].kind); free(g[i].col); }
    free(g);
}
void d2_scene_free(D2Scene *s) { free_groups(s->gc, s->ngc); free_groups(s->op, s->nop); free_groups(s->tc, s->ntc); free(s->ground); memset(s, 0, sizeof *s); }

/* ---------------------------------------------------------------- colours: gg.shade */
static int hexv(int c) { return c >= '0' && c <= '9' ? c - '0' : c >= 'a' && c <= 'f' ? c - 'a' + 10 : c >= 'A' && c <= 'F' ? c - 'A' + 10 : -1; }
static int parse_rgb(const char *h, int rgb[3]) {          /* ImageColor.getrgb for "#rrggbb" / "#rgb" (the kit only uses these) */
    size_t n = strlen(h);
    if (h[0] != '#' || (n != 7 && n != 4)) return 0;
    for (size_t i = 1; i < n; i++) if (hexv(h[i]) < 0) return 0;
    for (int i = 0; i < 3; i++) rgb[i] = n == 7 ? hexv(h[1 + 2 * i]) * 16 + hexv(h[2 + 2 * i]) : hexv(h[1 + i]) * 17;
    return 1;
}
static void shade(const int rgb[3], double k, unsigned char ink[4]) {          /* gg.shade(hexc, k), as the ink PIL makes of it */
    for (int i = 0; i < 3; i++) { int v = (int)(rgb[i] * k); ink[i] = (unsigned char)(v < 0 ? 0 : v > 255 ? 255 : v); }
    ink[3] = 255;
}
static uint32_t ink32(const unsigned char ink[4]) { uint32_t v; memcpy(&v, ink, 4); return v; }

/* ---------------------------------------------------------------- mesher._draw2d_pil */
typedef struct { Img *lay; int W; double cx, cy, half, k; } Ctx;
static double X(const Ctx *c, double x) { return (x - (c->cx - c->half)) * c->k; }
static double Y(const Ctx *c, double y) { return ((c->cy + c->half) - y) * c->k; }

/* rings(p) in pixels, offset by (ox, oy) (shadow) and less (sx, sy) (the mask's corner); ring i of p */
static double *ring_px(const Ctx *c, const D2Poly *p, int i, double ox, double oy, int sx, int sy, int use_off) {
    int at = 0; for (int j = 0; j < i; j++) at += p->len[j];
    double *o = malloc(16 * (p->len[i] + 1));
    for (int j = 0; j < p->len[i]; j++) {
        double x = X(c, p->xy[2 * (at + j)]), y = Y(c, p->xy[2 * (at + j) + 1]);
        if (use_off) { x = x + ox; y = y + oy; }
        o[2 * j] = x - sx; o[2 * j + 1] = y - sy;
    }
    return o;
}

/* fill(p, colour) and shadow(p, dx, dy) of _draw2d_pil; shadow != 0: the shadow form */
static void fill_or_shadow(Ctx *c, const D2Poly *p, const unsigned char ink[4], int is_shadow, double dx, double dy) {
    int W = c->W; double ox = 0, oy = 0;
    if (is_shadow) { ox = dx * c->k; oy = -dy * c->k; }
    if (p->nr < 1) return;
    int n0 = p->len[0];
    if (n0 < 3) return;
    double *r0 = ring_px(c, p, 0, ox, oy, 0, 0, is_shadow);
    if (!is_shadow && p->nr == 1) { draw_polygon_fill(c->lay, n0, r0, ink32(ink)); free(r0); return; }
    double mnx = r0[0], mxx = r0[0], mny = r0[1], mxy = r0[1];
    for (int j = 1; j < n0; j++) {          /* min() / max(): the first of equals; only the value matters here */
        if (r0[2 * j] < mnx) mnx = r0[2 * j];
        if (r0[2 * j] > mxx) mxx = r0[2 * j];
        if (r0[2 * j + 1] < mny) mny = r0[2 * j + 1];
        if (r0[2 * j + 1] > mxy) mxy = r0[2 * j + 1];
    }
    free(r0);
    int x0 = (int)(mnx > 0 ? mnx : 0), y0 = (int)(mny > 0 ? mny : 0);
    double ex = mxx + 2, ey = mxy + 2;
    int x1 = ex < W ? (int)ex : W, y1 = ey < W ? (int)ey : W;
    if (x1 <= x0 || y1 <= y0) return;
    unsigned char zero[4] = { 0, 0, 0, 0 }; Img *m = img_new(x1 - x0, y1 - y0, 1, zero);
    double *q = ring_px(c, p, 0, ox, oy, x0, y0, is_shadow);
    draw_polygon_fill(m, n0, q, is_shadow ? 89 : 255); free(q);
    for (int i = 1; i < p->nr; i++) {
        if (p->len[i] >= 3) { q = ring_px(c, p, i, ox, oy, x0, y0, is_shadow); draw_polygon_fill(m, p->len[i], q, 0); free(q); }
    }
    if (!is_shadow) fill_mask(c->lay, ink, m, x0, y0);
    else {          /* lay.paste(Image.composite(black, lay.crop(box), m), box) */
        Img *cr = crop(c->lay, x0, y0, x1, y1); unsigned char blk[4] = { 0, 0, 0, 255 }; Img *b = img_new(cr->w, cr->h, 4, blk);
        paste(cr, b, m, 0, 0); paste(c->lay, cr, NULL, x0, y0); img_free(cr); img_free(b);
    }
    img_free(m);
}

int d2_test_full = 0;          /* proof runs only: 1 = hand back the drawing at twice the size, before the resize */
int d2_render(const D2Scene *s, int px, int transparent, unsigned char **pix, int *channels, double win[4], char *err, int errlen) {
    *pix = NULL; *channels = transparent ? 4 : 3;
    if (!ucrt_load()) { snprintf(err, errlen, "the UCRT maths could not be loaded"); return 0; }
    int ground[3], tmp[3];
    if (!parse_rgb(s->ground, ground)) { snprintf(err, errlen, "ground colour %s", s->ground); return 0; }
    for (int g = 0; g < s->ngc + s->nop + s->ntc; g++) {          /* what Python's drawing would fail on (it then draws with matplotlib) */
        const D2Group *G = g < s->ngc ? &s->gc[g] : g < s->ngc + s->nop ? &s->op[g - s->ngc] : &s->tc[g - s->ngc - s->nop];
        if (!parse_rgb(G->col, tmp)) { snprintf(err, errlen, "colour %s", G->col); return 0; }
        for (int i = 0; i < G->np; i++) if (G->p[i].type == 2 || (G->p[i].type == 0 && (g < s->ngc || g >= s->ngc + s->nop))) { snprintf(err, errlen, "a piece that is not a Polygon"); return 0; }
    }
    const int SS = 2; int W = px * SS; double R0 = s->R0;
    double cx = 0.0, cy = 0.0, half = R0 + 1.0; double clip[4] = { 0.0, 0.0, R0, R0 }; int ring = 0;          /* clip: centre x, y, half widths */
    if (s->has_sp && s->has_oval) {
        double sc = s->sc, tx = s->tx, ty = s->ty, A = s->A, B = s->B, ocx = -tx / sc, ocy = -ty / sc, w = A / sc, h = B / sc;
        clip[0] = ocx; clip[1] = ocy; clip[2] = w / 2; clip[3] = h / 2; ring = 1; cx = ocx; cy = ocy; half = w / 2 * 1.03;
    } else if (s->has_sp && s->fin / s->sc < s->BASE_MM - 0.05) {
        double sc = s->sc, fin = s->fin, tx = s->tx, ty = s->ty, r = fin / sc / 2.0, ocx = -tx / sc, ocy = -ty / sc;
        clip[0] = ocx; clip[1] = ocy; clip[2] = r; clip[3] = r; ring = 1; cx = ocx; cy = ocy; half = r * 1.03;
    }
    double k = W / (2 * half);
    {          /* PLAN_WIN = _to_field(s, box) */
        double b[4] = { cx - half, cy - half, cx + half, cy + half };
        if (!s->has_sp || (s->has_oval && fabs(s->sc - 1) < 1e-6 && !s->tx && !s->ty)) memcpy(win, b, sizeof b);
        else { double f = s->sc; win[0] = b[0] * f + s->tx; win[1] = b[1] * f + s->ty; win[2] = b[2] * f + s->tx; win[3] = b[3] * f + s->ty; }
    }
    Ctx C = { NULL, W, cx, cy, half, k };
#define ELL(c0, c1, c2, c3) { X(&C, (c0) - (c2)), Y(&C, (c1) + (c3)), X(&C, (c0) + (c2)), Y(&C, (c1) - (c3)) }
    unsigned char white[4] = { 255, 255, 255, 255 }, ink[4], gnd[4] = { (unsigned char)ground[0], (unsigned char)ground[1], (unsigned char)ground[2], 255 };
    Img *img = img_new(W, W, 4, white);
    { double e[4] = ELL(0.0, 0.0, R0 + 0.6, R0 + 0.6); parse_rgb("#1d1b1a", tmp); shade(tmp, 1.0, ink); draw_ellipse(img, e, ink32(ink), 1, 0); }
    C.lay = img_new(W, W, 4, gnd);
    for (int g = 0; g < s->ngc; g++) {          /* s.final_gcuts() */
        parse_rgb(s->gc[g].col, tmp); shade(tmp, 1.0, ink);
        for (int i = 0; i < s->gc[g].np; i++) fill_or_shadow(&C, &s->gc[g].p[i], ink, 0, 0, 0);
    }
    double pxd = px;          /* px / 3 is a true division in Python */
    int lw = (int)rint(0.3 / 72 * (pxd / 3) * SS); if (lw < 1) lw = 1;          /* max(1, int(round(...))): round() ties to even, as rint */
    for (int g = 0; g < s->nop; g++) {          /* s.ops */
        const D2Group *G = &s->op[g]; double z0 = G->z0, top; int tier;
        if (G->optype == 0) { top = G->b; tier = z0 > s->H; }
        else { top = z0 + G->b; tier = z0 > s->H - s->SINK + 0.01; }
        double kk = 0.78 + 0.13 * (top - s->H); int col[3]; unsigned char fi[4], li[4];
        parse_rgb(G->col, col); shade(col, kk, fi); shade(col, .55, li);
        for (int i = 0; i < G->np; i++) {
            const D2Poly *p = &G->p[i];
            if (p->type == 0) continue;
            if (!tier) fill_or_shadow(&C, p, fi, 1, .2 + .12 * (top - s->H), -.2 - .12 * (top - s->H));
            fill_or_shadow(&C, p, fi, 0, 0, 0);
            for (int r = 0; r < p->nr; r++) { double *q = ring_px(&C, p, r, 0, 0, 0, 0, 0); draw_lines(C.lay, p->len[r], q, ink32(li), lw); free(q); }
        }
    }
    for (int g = 0; g < s->ntc; g++) {          /* s.tcuts */
        parse_rgb(s->tc[g].col, tmp); shade(tmp, 1.0, ink);
        for (int i = 0; i < s->tc[g].np; i++) fill_or_shadow(&C, &s->tc[g].p[i], ink, 0, 0, 0);
    }
    unsigned char zero[4] = { 0, 0, 0, 0 };
    Img *m = img_new(W, W, 1, zero);
    { double e[4] = ELL(clip[0], clip[1], clip[2], clip[3]); SHAPE(m, e, 255, 1, 0); }
    paste(img, C.lay, m, 0, 0); img_free(C.lay); img_free(m);
    if (ring) {
        int w = (int)(3.0 / 72 * (pxd / 3) * SS); if (w < 2) w = 2;
        double e[4] = ELL(clip[0], clip[1], clip[2], clip[3]); parse_rgb("#1a1a1a", tmp); shade(tmp, 1.0, ink); SHAPE(img, e, ink32(ink), 0, w);
    }
    Img *out;
    if (d2_test_full) {
        int ch = *channels; unsigned char *o = malloc((size_t)W * W * ch);
        for (size_t i = 0; i < (size_t)W * W; i++) { memcpy(o + ch * i, img->p + 4 * i, ch); if (ch == 4) o[4 * i + 3] = 255; }
        img_free(img); *pix = o; return 1;
    }
    if (transparent) {
        Img *a = img_new(W, W, 1, zero);
        if (!ring) { double e[4] = ELL(0.0, 0.0, R0 + 0.6, R0 + 0.6); draw_ellipse(a, e, 255, 1, 0); }
        else { double e[4] = ELL(clip[0], clip[1], clip[2] * 1.01, clip[3] * 1.01); SHAPE(a, e, 255, 1, 0); }
        unsigned int t;
        for (size_t i = 0; i < (size_t)W * W; i++) {          /* convert("RGBA") + putalpha(a), then the premultiplied RGBa resize needs */
            unsigned char *q = img->p + 4 * i; unsigned int al = a->p[i];
            q[3] = (unsigned char)al;
            q[0] = (unsigned char)MULDIV255(q[0], al, t); q[1] = (unsigned char)MULDIV255(q[1], al, t); q[2] = (unsigned char)MULDIV255(q[2], al, t);
        }
        img_free(a);
        out = resize_lanczos(img, px, px, 4);
        for (size_t i = 0; i < (size_t)px * px; i++) {          /* RGBa -> RGBA (Convert.c rgba2rgbA) */
            unsigned char *q = out->p + 4 * i; unsigned int al = q[3];
            if (al != 255 && al != 0) { for (int c = 0; c < 3; c++) { unsigned int v = (255 * q[c]) / al; q[c] = (unsigned char)CLIP8(v); } }
        }
    } else out = resize_lanczos(img, px, px, 3);
    img_free(img);
    int ch = *channels; unsigned char *o = malloc((size_t)px * px * ch);
    for (size_t i = 0; i < (size_t)px * px; i++) memcpy(o + ch * i, out->p + 4 * i, ch);
    img_free(out);
    *pix = o;
    return 1;
#undef ELL
}

/* ---------------------------------------------------------------- PNG: our own deflate (fixed Huffman codes, LZ77) */
typedef struct { unsigned char *b; size_t n, cap; uint32_t acc; int nb; } Bits;
static void put_byte(Bits *o, unsigned char c) { if (o->n == o->cap) { o->cap = o->cap ? o->cap * 2 : 65536; o->b = realloc(o->b, o->cap); } o->b[o->n++] = c; }
static void put_bits(Bits *o, uint32_t v, int n) { o->acc |= v << o->nb; o->nb += n; while (o->nb >= 8) { put_byte(o, (unsigned char)o->acc); o->acc >>= 8; o->nb -= 8; } }
static void put_rev(Bits *o, uint32_t code, int n) { uint32_t r = 0; for (int i = 0; i < n; i++) r |= ((code >> i) & 1) << (n - 1 - i); put_bits(o, r, n); }          /* Huffman codes go most significant bit first */
static void lit(Bits *o, int c) {          /* fixed literal/length code */
    if (c < 144) put_rev(o, 0x30 + c, 8); else if (c < 256) put_rev(o, 0x190 + c - 144, 9);
    else if (c < 280) put_rev(o, c - 256, 7); else put_rev(o, 0xc0 + c - 280, 8);
}
static const int LBASE[29] = { 3, 4, 5, 6, 7, 8, 9, 10, 11, 13, 15, 17, 19, 23, 27, 31, 35, 43, 51, 59, 67, 83, 99, 115, 131, 163, 195, 227, 258 };
static const int LEXT[29] = { 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2, 2, 3, 3, 3, 3, 4, 4, 4, 4, 5, 5, 5, 5, 0 };
static const int DBASE[30] = { 1, 2, 3, 4, 5, 7, 9, 13, 17, 25, 33, 49, 65, 97, 129, 193, 257, 385, 513, 769, 1025, 1537, 2049, 3073, 4097, 6145, 8193, 12289, 16385, 24577 };
static const int DEXT[30] = { 0, 0, 0, 0, 1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6, 6, 7, 7, 8, 8, 9, 9, 10, 10, 11, 11, 12, 12, 13, 13 };
static void match(Bits *o, int len, int dist) {
    int i = 28; while (LBASE[i] > len) i--;
    lit(o, 257 + i); if (LEXT[i]) put_bits(o, (uint32_t)(len - LBASE[i]), LEXT[i]);
    int d = 29; while (DBASE[d] > dist) d--;
    put_rev(o, (uint32_t)d, 5); if (DEXT[d]) put_bits(o, (uint32_t)(dist - DBASE[d]), DEXT[d]);
}
static uint32_t adler32(const unsigned char *p, size_t n) { uint32_t a = 1, b = 0; for (size_t i = 0; i < n; i++) { a = (a + p[i]) % 65521; b = (b + a) % 65521; } return (b << 16) | a; }
static void zlib_compress(const unsigned char *in, size_t n, Bits *o) {
    put_byte(o, 0x78); put_byte(o, 0x01);
    put_bits(o, 1, 1); put_bits(o, 1, 2);          /* one final block, fixed codes */
    enum { HB = 15, WIN = 32768, CHAIN = 24 };
    int *head = malloc(sizeof(int) << HB), *prev = malloc(sizeof(int) * WIN);
    for (int i = 0; i < (1 << HB); i++) head[i] = -1;
    size_t i = 0;
    while (i < n) {
        int best = 0, bd = 0;
        if (i + 3 <= n) {
            uint32_t h = ((in[i] << 10) ^ (in[i + 1] << 5) ^ in[i + 2]) & ((1 << HB) - 1);
            int c = head[h], tries = CHAIN; size_t maxl = n - i < 258 ? n - i : 258;
            while (c >= 0 && i - (size_t)c <= WIN - 1 && tries--) {
                if (in[c + best] == in[i + best]) { size_t l = 0; while (l < maxl && in[c + l] == in[i + l]) l++; if ((int)l > best) { best = (int)l; bd = (int)(i - c); if (l == maxl) break; } }
                c = prev[c & (WIN - 1)];
            }
            prev[i & (WIN - 1)] = head[h]; head[h] = (int)i;
        }
        if (best >= 3) {
            match(o, best, bd);
            for (size_t j = i + 1; j < i + best && j + 3 <= n; j++) { uint32_t h = ((in[j] << 10) ^ (in[j + 1] << 5) ^ in[j + 2]) & ((1 << HB) - 1); prev[j & (WIN - 1)] = head[h]; head[h] = (int)j; }
            i += best;
        } else { lit(o, in[i]); i++; }
    }
    lit(o, 256); if (o->nb) put_bits(o, 0, 8 - o->nb);
    uint32_t ad = adler32(in, n); put_byte(o, ad >> 24); put_byte(o, ad >> 16); put_byte(o, ad >> 8); put_byte(o, ad);
    free(head); free(prev);
}
static const uint32_t crc_tab[256] = {          /* PNG's CRC-32 table, precomputed (it was filled on first use: a race between worker threads) */
    0x00000000u, 0x77073096u, 0xee0e612cu, 0x990951bau, 0x076dc419u, 0x706af48fu, 0xe963a535u, 0x9e6495a3u,
    0x0edb8832u, 0x79dcb8a4u, 0xe0d5e91eu, 0x97d2d988u, 0x09b64c2bu, 0x7eb17cbdu, 0xe7b82d07u, 0x90bf1d91u,
    0x1db71064u, 0x6ab020f2u, 0xf3b97148u, 0x84be41deu, 0x1adad47du, 0x6ddde4ebu, 0xf4d4b551u, 0x83d385c7u,
    0x136c9856u, 0x646ba8c0u, 0xfd62f97au, 0x8a65c9ecu, 0x14015c4fu, 0x63066cd9u, 0xfa0f3d63u, 0x8d080df5u,
    0x3b6e20c8u, 0x4c69105eu, 0xd56041e4u, 0xa2677172u, 0x3c03e4d1u, 0x4b04d447u, 0xd20d85fdu, 0xa50ab56bu,
    0x35b5a8fau, 0x42b2986cu, 0xdbbbc9d6u, 0xacbcf940u, 0x32d86ce3u, 0x45df5c75u, 0xdcd60dcfu, 0xabd13d59u,
    0x26d930acu, 0x51de003au, 0xc8d75180u, 0xbfd06116u, 0x21b4f4b5u, 0x56b3c423u, 0xcfba9599u, 0xb8bda50fu,
    0x2802b89eu, 0x5f058808u, 0xc60cd9b2u, 0xb10be924u, 0x2f6f7c87u, 0x58684c11u, 0xc1611dabu, 0xb6662d3du,
    0x76dc4190u, 0x01db7106u, 0x98d220bcu, 0xefd5102au, 0x71b18589u, 0x06b6b51fu, 0x9fbfe4a5u, 0xe8b8d433u,
    0x7807c9a2u, 0x0f00f934u, 0x9609a88eu, 0xe10e9818u, 0x7f6a0dbbu, 0x086d3d2du, 0x91646c97u, 0xe6635c01u,
    0x6b6b51f4u, 0x1c6c6162u, 0x856530d8u, 0xf262004eu, 0x6c0695edu, 0x1b01a57bu, 0x8208f4c1u, 0xf50fc457u,
    0x65b0d9c6u, 0x12b7e950u, 0x8bbeb8eau, 0xfcb9887cu, 0x62dd1ddfu, 0x15da2d49u, 0x8cd37cf3u, 0xfbd44c65u,
    0x4db26158u, 0x3ab551ceu, 0xa3bc0074u, 0xd4bb30e2u, 0x4adfa541u, 0x3dd895d7u, 0xa4d1c46du, 0xd3d6f4fbu,
    0x4369e96au, 0x346ed9fcu, 0xad678846u, 0xda60b8d0u, 0x44042d73u, 0x33031de5u, 0xaa0a4c5fu, 0xdd0d7cc9u,
    0x5005713cu, 0x270241aau, 0xbe0b1010u, 0xc90c2086u, 0x5768b525u, 0x206f85b3u, 0xb966d409u, 0xce61e49fu,
    0x5edef90eu, 0x29d9c998u, 0xb0d09822u, 0xc7d7a8b4u, 0x59b33d17u, 0x2eb40d81u, 0xb7bd5c3bu, 0xc0ba6cadu,
    0xedb88320u, 0x9abfb3b6u, 0x03b6e20cu, 0x74b1d29au, 0xead54739u, 0x9dd277afu, 0x04db2615u, 0x73dc1683u,
    0xe3630b12u, 0x94643b84u, 0x0d6d6a3eu, 0x7a6a5aa8u, 0xe40ecf0bu, 0x9309ff9du, 0x0a00ae27u, 0x7d079eb1u,
    0xf00f9344u, 0x8708a3d2u, 0x1e01f268u, 0x6906c2feu, 0xf762575du, 0x806567cbu, 0x196c3671u, 0x6e6b06e7u,
    0xfed41b76u, 0x89d32be0u, 0x10da7a5au, 0x67dd4accu, 0xf9b9df6fu, 0x8ebeeff9u, 0x17b7be43u, 0x60b08ed5u,
    0xd6d6a3e8u, 0xa1d1937eu, 0x38d8c2c4u, 0x4fdff252u, 0xd1bb67f1u, 0xa6bc5767u, 0x3fb506ddu, 0x48b2364bu,
    0xd80d2bdau, 0xaf0a1b4cu, 0x36034af6u, 0x41047a60u, 0xdf60efc3u, 0xa867df55u, 0x316e8eefu, 0x4669be79u,
    0xcb61b38cu, 0xbc66831au, 0x256fd2a0u, 0x5268e236u, 0xcc0c7795u, 0xbb0b4703u, 0x220216b9u, 0x5505262fu,
    0xc5ba3bbeu, 0xb2bd0b28u, 0x2bb45a92u, 0x5cb36a04u, 0xc2d7ffa7u, 0xb5d0cf31u, 0x2cd99e8bu, 0x5bdeae1du,
    0x9b64c2b0u, 0xec63f226u, 0x756aa39cu, 0x026d930au, 0x9c0906a9u, 0xeb0e363fu, 0x72076785u, 0x05005713u,
    0x95bf4a82u, 0xe2b87a14u, 0x7bb12baeu, 0x0cb61b38u, 0x92d28e9bu, 0xe5d5be0du, 0x7cdcefb7u, 0x0bdbdf21u,
    0x86d3d2d4u, 0xf1d4e242u, 0x68ddb3f8u, 0x1fda836eu, 0x81be16cdu, 0xf6b9265bu, 0x6fb077e1u, 0x18b74777u,
    0x88085ae6u, 0xff0f6a70u, 0x66063bcau, 0x11010b5cu, 0x8f659effu, 0xf862ae69u, 0x616bffd3u, 0x166ccf45u,
    0xa00ae278u, 0xd70dd2eeu, 0x4e048354u, 0x3903b3c2u, 0xa7672661u, 0xd06016f7u, 0x4969474du, 0x3e6e77dbu,
    0xaed16a4au, 0xd9d65adcu, 0x40df0b66u, 0x37d83bf0u, 0xa9bcae53u, 0xdebb9ec5u, 0x47b2cf7fu, 0x30b5ffe9u,
    0xbdbdf21cu, 0xcabac28au, 0x53b39330u, 0x24b4a3a6u, 0xbad03605u, 0xcdd70693u, 0x54de5729u, 0x23d967bfu,
    0xb3667a2eu, 0xc4614ab8u, 0x5d681b02u, 0x2a6f2b94u, 0xb40bbe37u, 0xc30c8ea1u, 0x5a05df1bu, 0x2d02ef8du
};
static uint32_t crc32_of(const unsigned char *p, size_t n, uint32_t c) {
    c = ~c; for (size_t i = 0; i < n; i++) c = crc_tab[(c ^ p[i]) & 255] ^ (c >> 8); return ~c;
}
static void chunk(FILE *f, const char *type, const unsigned char *d, size_t n) {
    unsigned char hd[8] = { (unsigned char)(n >> 24), (unsigned char)(n >> 16), (unsigned char)(n >> 8), (unsigned char)n, (unsigned char)type[0], (unsigned char)type[1], (unsigned char)type[2], (unsigned char)type[3] };
    fwrite(hd, 1, 8, f); if (n) fwrite(d, 1, n, f);
    uint32_t c = crc32_of(hd + 4, 4, 0); c = crc32_of(d, n, c);
    unsigned char cc[4] = { (unsigned char)(c >> 24), (unsigned char)(c >> 16), (unsigned char)(c >> 8), (unsigned char)c }; fwrite(cc, 1, 4, f);
}
static int paeth(int a, int b, int c) { int p = a + b - c, pa = abs(p - a), pb = abs(p - b), pc = abs(p - c); return pa <= pb && pa <= pc ? a : pb <= pc ? b : c; }
int d2_png_write(const char *path, const unsigned char *pix, int w, int h, int ch) {
    size_t rl = (size_t)w * ch; unsigned char *raw = malloc((rl + 1) * h), *cand = malloc(rl * 5);
    for (int y = 0; y < h; y++) {          /* each row gets the filter with the smallest sum of |bytes| (libpng's heuristic) */
        const unsigned char *r = pix + y * rl, *u = y ? pix + (y - 1) * rl : NULL; long best = -1; int bf = 0;
        for (int f = 0; f < 5; f++) {
            long sum = 0; unsigned char *c = cand + f * rl;
            for (size_t x = 0; x < rl; x++) {
                int a = x >= (size_t)ch ? r[x - ch] : 0, b = u ? u[x] : 0, cc = (u && x >= (size_t)ch) ? u[x - ch] : 0, v;
                v = f == 0 ? r[x] : f == 1 ? r[x] - a : f == 2 ? r[x] - b : f == 3 ? r[x] - ((a + b) >> 1) : r[x] - paeth(a, b, cc);
                c[x] = (unsigned char)v; sum += (signed char)c[x] < 0 ? -(signed char)c[x] : (signed char)c[x];
            }
            if (best < 0 || sum < best) { best = sum; bf = f; }
        }
        raw[y * (rl + 1)] = (unsigned char)bf; memcpy(raw + y * (rl + 1) + 1, cand + bf * rl, rl);
    }
    free(cand);
    Bits z = { 0 }; zlib_compress(raw, (rl + 1) * h, &z); free(raw);
    FILE *f = fopen(path, "wb");
    if (!f) { free(z.b); return 0; }
    static const unsigned char sig[8] = { 137, 80, 78, 71, 13, 10, 26, 10 }; fwrite(sig, 1, 8, f);
    unsigned char ih[13] = { (unsigned char)(w >> 24), (unsigned char)(w >> 16), (unsigned char)(w >> 8), (unsigned char)w, (unsigned char)(h >> 24), (unsigned char)(h >> 16), (unsigned char)(h >> 8), (unsigned char)h, 8, (unsigned char)(ch == 4 ? 6 : 2), 0, 0, 0 };
    chunk(f, "IHDR", ih, 13); chunk(f, "IDAT", z.b, z.n); chunk(f, "IEND", NULL, 0);
    int ok = !ferror(f); fclose(f); free(z.b);
    return ok;
}

int d2_draw(const D2Scene *s, const char *png, int px, int transparent, double win[4], char *err, int errlen) {
    unsigned char *pix; int ch;
    if (!d2_render(s, px, transparent, &pix, &ch, win, err, errlen)) return 0;
    int ok = d2_png_write(png, pix, px, px, ch); free(pix);
    if (!ok) snprintf(err, errlen, "cannot write %s", png);
    return ok;
}

/* ================================================================ PIL steps for the runner (gui_job.see_through) */
unsigned char *d2_resize(const unsigned char *pix, int w, int h, int ch, int nw, int nh) {          /* Image.resize(..., LANCZOS) */
    unsigned char z[4] = { 0, 0, 0, 0 }; unsigned int t;
    if (!ucrt_load()) return NULL;
    Img *in = img_new(w, h, 4, z);
    for (size_t i = 0; i < (size_t)w * h; i++) {
        unsigned char *q = in->p + 4 * i; memcpy(q, pix + ch * i, ch);
        if (ch == 4) { unsigned int al = q[3]; q[0] = (unsigned char)MULDIV255(q[0], al, t); q[1] = (unsigned char)MULDIV255(q[1], al, t); q[2] = (unsigned char)MULDIV255(q[2], al, t); }          /* RGBA -> RGBa */
    }
    Img *out = resize_lanczos(in, nw, nh, ch == 4 ? 4 : 3); img_free(in);
    unsigned char *o = malloc((size_t)nw * nh * ch);
    for (size_t i = 0; i < (size_t)nw * nh; i++) {
        unsigned char *q = out->p + 4 * i;
        if (ch == 4) { unsigned int al = q[3]; if (al != 255 && al != 0) for (int c = 0; c < 3; c++) { unsigned int v = (255 * q[c]) / al; q[c] = (unsigned char)CLIP8(v); } }          /* RGBa -> RGBA */
        memcpy(o + ch * i, q, ch);
    }
    img_free(out); return o;
}

/* ---- inflate (RFC 1951): stored, fixed and dynamic blocks ---- */
typedef struct { const unsigned char *p, *e; uint32_t bb; int bn; int bad; } In;
static int ibit(In *s, int n) { while (s->bn < n) { if (s->p >= s->e) { s->bad = 1; return 0; } s->bb |= (uint32_t)*s->p++ << s->bn; s->bn += 8; } int v = (int)(s->bb & ((1u << n) - 1)); s->bb >>= n; s->bn -= n; return v; }
typedef struct { short cnt[16], sym[320]; } Huff;
static void hbuild(Huff *h, const unsigned char *len, int n) {
    short off[16]; memset(h->cnt, 0, sizeof h->cnt);
    for (int i = 0; i < n; i++) h->cnt[len[i]]++;
    h->cnt[0] = 0; off[1] = 0; for (int i = 1; i < 15; i++) off[i + 1] = off[i] + h->cnt[i];
    for (int i = 0; i < n; i++) if (len[i]) h->sym[off[len[i]]++] = (short)i;
}
static int hdecode(In *s, const Huff *h) {
    int code = 0, first = 0, index = 0;
    for (int l = 1; l < 16; l++) { code |= ibit(s, 1); int c = h->cnt[l]; if (code - c < first) return h->sym[index + (code - first)]; index += c; first += c; first <<= 1; code <<= 1; if (s->bad) return -1; }
    s->bad = 1; return -1;
}
static unsigned char *inflate_z(const unsigned char *z, size_t n, size_t *outn) {
    In s = { z + 2, z + n, 0, 0, 0 };
    size_t cap = 1 << 20, len = 0; unsigned char *o = malloc(cap); int last;
#define OUTB(c) do { if (len == cap) { cap *= 2; o = realloc(o, cap); } o[len++] = (unsigned char)(c); } while (0)
    do {
        last = ibit(&s, 1); int type = ibit(&s, 2);
        if (type == 0) {
            s.bb = 0; s.bn = 0; if (s.e - s.p < 4) { s.bad = 1; break; }
            unsigned L = s.p[0] | (s.p[1] << 8); s.p += 4; if ((size_t)(s.e - s.p) < L) { s.bad = 1; break; }
            for (unsigned i = 0; i < L; i++) OUTB(s.p[i]);
            s.p += L; continue;
        }
        Huff lh, dh; unsigned char ln[320];
        if (type == 1) {
            for (int i = 0; i < 288; i++) ln[i] = i < 144 ? 8 : i < 256 ? 9 : i < 280 ? 7 : 8;
            hbuild(&lh, ln, 288);
            for (int i = 0; i < 30; i++) ln[i] = 5;
            hbuild(&dh, ln, 30);
        } else if (type == 2) {
            static const unsigned char ord[19] = { 16, 17, 18, 0, 8, 7, 9, 6, 10, 5, 11, 4, 12, 3, 13, 2, 14, 1, 15 };
            int nlen = ibit(&s, 5) + 257, ndist = ibit(&s, 5) + 1, ncode = ibit(&s, 4) + 4; unsigned char cl[19] = { 0 }; Huff chh;
            for (int i = 0; i < ncode; i++) cl[ord[i]] = (unsigned char)ibit(&s, 3);
            hbuild(&chh, cl, 19);
            for (int i = 0; i < nlen + ndist && !s.bad;) {
                int sym = hdecode(&s, &chh);
                if (sym < 0) break;
                if (sym < 16) ln[i++] = (unsigned char)sym;
                else { int rep, v = 0; if (sym == 16) { if (!i) { s.bad = 1; break; } v = ln[i - 1]; rep = 3 + ibit(&s, 2); } else if (sym == 17) rep = 3 + ibit(&s, 3); else rep = 11 + ibit(&s, 7); while (rep-- && i < nlen + ndist) ln[i++] = (unsigned char)v; }
            }
            hbuild(&lh, ln, nlen); hbuild(&dh, ln + nlen, ndist);
        } else { s.bad = 1; break; }
        for (;;) {
            int sym = hdecode(&s, &lh); if (s.bad) break;
            if (sym < 256) OUTB(sym);
            else if (sym == 256) break;
            else {
                sym -= 257; if (sym >= 29) { s.bad = 1; break; }
                int L = LBASE[sym] + ibit(&s, LEXT[sym]); int d = hdecode(&s, &dh); if (d < 0 || d >= 30) { s.bad = 1; break; }
                size_t D = (size_t)(DBASE[d] + ibit(&s, DEXT[d])); if (D > len) { s.bad = 1; break; }
                for (int i = 0; i < L; i++) { unsigned char cc = o[len - D]; OUTB(cc); }
            }
        }
    } while (!last && !s.bad);
#undef OUTB
    if (s.bad) { free(o); return NULL; }
    *outn = len; return o;
}
unsigned char *d2_png_read_rgba(const char *path, int *w, int *h) {
    WCHAR wp[MAX_PATH * 2]; MultiByteToWideChar(CP_UTF8, 0, path, -1, wp, MAX_PATH * 2);
    FILE *f = _wfopen(wp, L"rb"); if (!f) return NULL;
    fseek(f, 0, SEEK_END); long n = ftell(f); fseek(f, 0, SEEK_SET); unsigned char *b = malloc(n > 0 ? n : 1); size_t got = fread(b, 1, n, f); fclose(f);
    if (got < 33 || memcmp(b + 1, "PNG", 3)) { free(b); return NULL; }
    size_t at = 8, zl = 0; unsigned char *z = NULL; int W = 0, H = 0, depth = 0, ct = 0, il = 0;
    while (at + 8 <= got) {
        uint32_t L = ((uint32_t)b[at] << 24) | (b[at + 1] << 16) | (b[at + 2] << 8) | b[at + 3]; const unsigned char *t = b + at + 4, *d = b + at + 8;
        if (at + 12 + L > got) break;
        if (!memcmp(t, "IHDR", 4)) { W = (d[0] << 24) | (d[1] << 16) | (d[2] << 8) | d[3]; H = (d[4] << 24) | (d[5] << 16) | (d[6] << 8) | d[7]; depth = d[8]; ct = d[9]; il = d[12]; }
        else if (!memcmp(t, "IDAT", 4)) { z = realloc(z, zl + L + 1); memcpy(z + zl, d, L); zl += L; }
        else if (!memcmp(t, "IEND", 4)) break;
        at += 12 + L;
    }
    free(b);
    int ch = ct == 0 ? 1 : ct == 2 ? 3 : ct == 4 ? 2 : ct == 6 ? 4 : 0;
    if (!z || depth != 8 || !ch || il || W <= 0 || H <= 0) { free(z); return NULL; }
    size_t rn; unsigned char *raw = inflate_z(z, zl, &rn); free(z);
    size_t rl = (size_t)W * ch;
    if (!raw || rn < (rl + 1) * H) { free(raw); return NULL; }
    unsigned char *px = malloc(rl * H);
    for (int y = 0; y < H; y++) {
        int ft = raw[y * (rl + 1)]; const unsigned char *s = raw + y * (rl + 1) + 1; unsigned char *r = px + y * rl, *u = y ? px + (y - 1) * rl : NULL;
        for (size_t x = 0; x < rl; x++) {
            int a = x >= (size_t)ch ? r[x - ch] : 0, bb = u ? u[x] : 0, c = (u && x >= (size_t)ch) ? u[x - ch] : 0;
            int v = ft == 0 ? 0 : ft == 1 ? a : ft == 2 ? bb : ft == 3 ? (a + bb) >> 1 : paeth(a, bb, c);
            r[x] = (unsigned char)(s[x] + v);
        }
    }
    free(raw);
    unsigned char *o = malloc((size_t)W * H * 4);          /* Image.convert("RGBA") */
    for (size_t i = 0; i < (size_t)W * H; i++) {
        const unsigned char *q = px + ch * i; unsigned char *d = o + 4 * i;
        if (ch == 1) { d[0] = d[1] = d[2] = q[0]; d[3] = 255; } else if (ch == 2) { d[0] = d[1] = d[2] = q[0]; d[3] = q[1]; }
        else if (ch == 3) { memcpy(d, q, 3); d[3] = 255; } else memcpy(d, q, 4);
    }
    free(px); *w = W; *h = H; return o;
}

/* ImageDraw.floodfill(im, xy, (255, 255, 255, 0), thresh=6) on RGBA: the 4-connected region whose colour is within
   thresh (sum of |differences| over the four channels) of the seed's colour */
static void floodfill(unsigned char *p, int w, int h, int sx, int sy) {
    const unsigned char val[4] = { 255, 255, 255, 0 }; unsigned char bg[4]; memcpy(bg, p + 4 * ((size_t)sy * w + sx), 4);
    int d = 0; for (int i = 0; i < 4; i++) d += abs(val[i] - bg[i]);
    if (d <= 6) return;
    int *st = malloc(sizeof(int) * 2 * ((size_t)w * h + 1)); size_t ns = 0;
    memcpy(p + 4 * ((size_t)sy * w + sx), val, 4); st[ns++] = sx; st[ns++] = sy;
    while (ns) {
        int y = st[--ns], x = st[--ns];
        static const int dx[4] = { 1, -1, 0, 0 }, dy[4] = { 0, 0, 1, -1 };
        for (int k = 0; k < 4; k++) {
            int s = x + dx[k], t = y + dy[k];
            if (s < 0 || t < 0 || s >= w || t >= h) continue;
            unsigned char *q = p + 4 * ((size_t)t * w + s); int e = 0; for (int i = 0; i < 4; i++) e += abs(q[i] - bg[i]);
            if (e <= 6) { memcpy(q, val, 4); st[ns++] = s; st[ns++] = t; }
        }
    }
    free(st);
}
static int round_aspect(double number, double aspect, double other, int by_x) {          /* Image.thumbnail's round_aspect: the closer of floor / ceil (floor on a tie), at least 1 */
    int f = (int)floor(number), c = (int)ceil(number); double kf, kc;
    if (by_x) { kf = fabs(aspect - f / other); kc = fabs(aspect - c / other); }
    else { kf = f == 0 ? 0 : fabs(aspect - other / f); kc = c == 0 ? 0 : fabs(aspect - other / c); }
    int r = kc < kf ? c : f; return r > 1 ? r : 1;
}
int d2_see_through(const char *src, const char *dst, int px, char *err, int errlen) {
    int w, h; unsigned char *p = d2_png_read_rgba(src, &w, &h);
    if (!p) { snprintf(err, errlen, "cannot read %s", src); return 0; }
    int cx[4] = { 0, w - 1, 0, w - 1 }, cy[4] = { 0, 0, h - 1, h - 1 };
    for (int k = 0; k < 4; k++) { unsigned char *q = p + 4 * ((size_t)cy[k] * w + cx[k]); if (q[0] == 255 && q[1] == 255 && q[2] == 255) floodfill(p, w, h, cx[k], cy[k]); }
    int x0 = w, y0 = h, x1 = -1, y1 = -1;          /* getbbox(): where alpha is not 0 */
    for (int y = 0; y < h; y++) for (int x = 0; x < w; x++) if (p[4 * ((size_t)y * w + x) + 3]) { if (x < x0) x0 = x; if (x > x1) x1 = x; if (y < y0) y0 = y; if (y > y1) y1 = y; }
    if (x1 >= 0) {
        int nw = x1 + 1 - x0, nh = y1 + 1 - y0; unsigned char *c = malloc((size_t)nw * nh * 4);
        for (int y = 0; y < nh; y++) memcpy(c + (size_t)y * nw * 4, p + 4 * ((size_t)(y + y0) * w + x0), (size_t)nw * 4);
        free(p); p = c; w = nw; h = nh;
    }
    if (!(px >= w && px >= h)) {          /* thumbnail((px, px), LANCZOS) */
        double aspect = (double)w / h; int x = px, y = px;
        if ((double)x / y >= aspect) x = round_aspect(y * aspect, aspect, y, 1);
        else y = round_aspect(x / aspect, aspect, x, 0);
        int fx = (int)((double)w / x / 2.0), fy = (int)((double)h / y / 2.0);          /* reducing_gap 2.0 */
        if ((fx ? fx : 1) > 1 || (fy ? fy : 1) > 1) { free(p); snprintf(err, errlen, "a thumbnail that needs Image.reduce (not in C yet)"); return 0; }
        if (x != w || y != h) { unsigned char *r = d2_resize(p, w, h, 4, x, y); free(p); p = r; w = x; h = y; }
    }
    int ok = d2_png_write(dst, p, w, h, 4); free(p);
    if (!ok) snprintf(err, errlen, "cannot write %s", dst);
    return ok;
}
