/* meshglue.c: the steps between the finished scene and the C height field, in C (see meshglue.h). Each function
   names the Python routine it copies; GEOS calls and float operations are made in the Python order. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include <windows.h>
#include "meshglue.h"
#include "shp.h"
#include "pyx.h"
#include "magnets.h"

#define PI 3.141592653589793
#define DEG2RAD (PI / 180.0)              /* CPython's degToRad */
#define GRID 1e-4                         /* mesher.GRID */
#define NSEG 360                          /* mesher.NSEG */
#define NSEG_OVAL 480                     /* mesher.NSEG_OVAL */
char mg_err[256];

static int fail_geos(const char *what) { snprintf(mg_err, sizeof mg_err, "GEOSException: %s%s%s", geos_err, what ? " in " : "", what ? what : ""); return MG_ERR; }
static int fail(const char *msg) { snprintf(mg_err, sizeof mg_err, "%s", msg); return MG_ERR; }

/* ======================= small containers ======================= */
void mg_buf_add(MgBuf *o, const void *p, size_t k) {
    if (o->n + k + 1 > o->cap) { o->cap = (o->n + k + 1) * 2 + 64; o->b = (unsigned char *)realloc(o->b, o->cap); }
    if (k) memcpy(o->b + o->n, p, k);
    o->n += k; o->b[o->n] = 0;
}
static void buf_s(MgBuf *o, const char *s) { mg_buf_add(o, s, strlen(s)); }
static void buf_u8(MgBuf *o, unsigned v) { unsigned char c = (unsigned char)v; mg_buf_add(o, &c, 1); }
static void buf_u32(MgBuf *o, uint32_t v) { mg_buf_add(o, &v, 4); }
static void buf_i32(MgBuf *o, int32_t v) { mg_buf_add(o, &v, 4); }
static void buf_f64(MgBuf *o, double v) { mg_buf_add(o, &v, 8); }

void mg_strs_push(MgStrs *l, const char *s) {
    if (l->n == l->cap) { l->cap = l->cap ? 2 * l->cap : 8; l->s = (char **)realloc(l->s, sizeof(char *) * (size_t)l->cap); }
    l->s[l->n++] = strdup(s);
}
void mg_strs_free(MgStrs *l) { for (int i = 0; i < l->n; i++) free(l->s[i]); free(l->s); memset(l, 0, sizeof *l); }

/* ======================= Python's float repr ======================= */
int mg_float_repr(double x, char *out, int cap) {
    char t[64], dig[32], b[64]; int nd = 0, e, k = 0;
    if (isnan(x)) return snprintf(out, cap, "nan");
    if (isinf(x)) return snprintf(out, cap, x > 0 ? "inf" : "-inf");
    int neg = signbit(x) != 0; double ax = fabs(x);
    if (ax == 0) return snprintf(out, cap, neg ? "-0.0" : "0.0");
    for (int p = 1; p <= 17; p++) { snprintf(t, sizeof t, "%.*e", p - 1, ax); if (strtod(t, NULL) == ax) break; }
    for (char *c = t; *c && *c != 'e'; c++) if (*c >= '0' && *c <= '9') dig[nd++] = *c;
    e = atoi(strchr(t, 'e') + 1);
    while (nd > 1 && dig[nd - 1] == '0') nd--;
    if (neg) b[k++] = '-';
    int decpt = e + 1;
    if (e >= -4 && e < 16) {              /* repr: fixed notation when -4 <= exponent < 16 */
        if (decpt <= 0) { b[k++] = '0'; b[k++] = '.'; for (int i = 0; i < -decpt; i++) b[k++] = '0'; for (int i = 0; i < nd; i++) b[k++] = dig[i]; }
        else if (decpt >= nd) { for (int i = 0; i < nd; i++) b[k++] = dig[i]; for (int i = nd; i < decpt; i++) b[k++] = '0'; b[k++] = '.'; b[k++] = '0'; }
        else { for (int i = 0; i < decpt; i++) b[k++] = dig[i]; b[k++] = '.'; for (int i = decpt; i < nd; i++) b[k++] = dig[i]; }
        b[k] = 0;
    } else {
        b[k++] = dig[0];
        if (nd > 1) { b[k++] = '.'; for (int i = 1; i < nd; i++) b[k++] = dig[i]; }
        snprintf(b + k, sizeof b - (size_t)k, "e%c%02d", e < 0 ? '-' : '+', e < 0 ? -e : e);
    }
    return snprintf(out, cap, "%s", b);
}
static void buf_float(MgBuf *o, double x) { char t[64]; mg_float_repr(x, t, sizeof t); buf_s(o, t); }
static void fmt_g(char *t, size_t n, double v) { snprintf(t, n, "%g", v); }          /* f"{v:g}" */

/* ======================= md5 (hashlib.md5) ======================= */
static const uint32_t MD5K[64] = {
    0xd76aa478, 0xe8c7b756, 0x242070db, 0xc1bdceee, 0xf57c0faf, 0x4787c62a, 0xa8304613, 0xfd469501, 0x698098d8, 0x8b44f7af,
    0xffff5bb1, 0x895cd7be, 0x6b901122, 0xfd987193, 0xa679438e, 0x49b40821, 0xf61e2562, 0xc040b340, 0x265e5a51, 0xe9b6c7aa,
    0xd62f105d, 0x02441453, 0xd8a1e681, 0xe7d3fbc8, 0x21e1cde6, 0xc33707d6, 0xf4d50d87, 0x455a14ed, 0xa9e3e905, 0xfcefa3f8,
    0x676f02d9, 0x8d2a4c8a, 0xfffa3942, 0x8771f681, 0x6d9d6122, 0xfde5380c, 0xa4beea44, 0x4bdecfa9, 0xf6bb4b60, 0xbebfbc70,
    0x289b7ec6, 0xeaa127fa, 0xd4ef3085, 0x04881d05, 0xd9d4d039, 0xe6db99e5, 0x1fa27cf8, 0xc4ac5665, 0xf4292244, 0x432aff97,
    0xab9423a7, 0xfc93a039, 0x655b59c3, 0x8f0ccc92, 0xffeff47d, 0x85845dd1, 0x6fa87e4f, 0xfe2ce6e0, 0xa3014314, 0x4e0811a1,
    0xf7537e82, 0xbd3af235, 0x2ad7d2bb, 0xeb86d391 };
static const int MD5R[64] = { 7, 12, 17, 22, 7, 12, 17, 22, 7, 12, 17, 22, 7, 12, 17, 22, 5, 9, 14, 20, 5, 9, 14, 20, 5, 9, 14, 20, 5, 9, 14, 20,
    4, 11, 16, 23, 4, 11, 16, 23, 4, 11, 16, 23, 4, 11, 16, 23, 6, 10, 15, 21, 6, 10, 15, 21, 6, 10, 15, 21, 6, 10, 15, 21 };
static void md5_block(uint32_t h[4], const unsigned char *p) {
    uint32_t w[16], a = h[0], b = h[1], c = h[2], d = h[3];
    for (int i = 0; i < 16; i++) w[i] = (uint32_t)p[4 * i] | (uint32_t)p[4 * i + 1] << 8 | (uint32_t)p[4 * i + 2] << 16 | (uint32_t)p[4 * i + 3] << 24;
    for (int i = 0; i < 64; i++) {
        uint32_t f; int g;
        if (i < 16) { f = (b & c) | (~b & d); g = i; }
        else if (i < 32) { f = (d & b) | (~d & c); g = (5 * i + 1) & 15; }
        else if (i < 48) { f = b ^ c ^ d; g = (3 * i + 5) & 15; }
        else { f = c ^ (b | ~d); g = (7 * i) & 15; }
        uint32_t t = d; d = c; c = b;
        uint32_t x = a + f + MD5K[i] + w[g];
        b = b + ((x << MD5R[i]) | (x >> (32 - MD5R[i]))); a = t;
    }
    h[0] += a; h[1] += b; h[2] += c; h[3] += d;
}
void mg_md5_hex(const void *data, size_t n, char out[33]) {
    uint32_t h[4] = { 0x67452301, 0xefcdab89, 0x98badcfe, 0x10325476 };
    const unsigned char *p = (const unsigned char *)data; unsigned char tail[128]; size_t i = 0, r;
    for (; i + 64 <= n; i += 64) md5_block(h, p + i);
    r = n - i; memset(tail, 0, sizeof tail); memcpy(tail, p + i, r); tail[r] = 0x80;
    size_t tl = r + 1 + 8 <= 64 ? 64 : 128; uint64_t bits = (uint64_t)n * 8;
    for (int k = 0; k < 8; k++) tail[tl - 8 + k] = (unsigned char)(bits >> (8 * k));
    md5_block(h, tail); if (tl == 128) md5_block(h, tail + 64);
    for (int k = 0; k < 16; k++) snprintf(out + 2 * k, 3, "%02x", (unsigned)((h[k / 4] >> (8 * (k % 4))) & 0xff));
}

/* ======================= geometry helpers ======================= */
/* mesher.P: shapely.set_precision(g, GRID) (None / empty pass through) */
static GEOSGeometry *P(const GEOSGeometry *g) {
    if (shp_is_empty(g)) return shp_clone(g);
    return G.GEOSGeom_setPrecision_r(gctx, g, GRID, 0);
}
/* mesher.polys(g): every Polygon of area > 1e-6, depth first (clones) */
static void polys(const GEOSGeometry *g, ShpList *out) {
    if (!g || shp_is_empty(g)) return;
    int t = shp_type(g);
    if (t == GEOS_POLYGON) { if (shp_area(g) > 1e-6) shp_list_push(out, shp_clone(g)); return; }
    if (t >= GEOS_MULTIPOINT) for (int i = 0; i < shp_num_geoms(g); i++) polys(shp_geom_n(g, i), out);
}
static double py_max(double a, double b) { return b > a ? b : a; }          /* max(a, b) */
static double py_min(double a, double b) { return b < a ? b : a; }          /* min(a, b) */

/* gg.ellipse_ring(a, b, n) -> points (2 * 4q doubles) and T (4q); returns the count */
static int ellipse_ring(double a, double b, int n, double **pts, double **Tout) {
    int q = n / 4 > 4 ? n / 4 : 4, M = 4000;
    double *ts = (double *)malloc(sizeof(double) * (M + 1)), *L = (double *)malloc(sizeof(double) * (M + 1)), *quarter = (double *)malloc(sizeof(double) * (size_t)q);
    for (int i = 0; i <= M; i++) ts[i] = 0.5 * PI * i / M;
    L[0] = 0.0;
    for (int i = 1; i <= M; i++) L[i] = L[i - 1] + py_hypot(a * (py_cos(ts[i]) - py_cos(ts[i - 1])), b * (py_sin(ts[i]) - py_sin(ts[i - 1])));
    int j = 0;
    for (int k = 0; k < q; k++) {
        double want = L[M] * k / q;
        while (L[j + 1] < want) j++;
        quarter[k] = ts[j] + (ts[j + 1] - ts[j]) * ((want - L[j]) / py_max(L[j + 1] - L[j], 1e-15));
    }
    int N = 4 * q, m = 0; double *T = (double *)malloc(sizeof(double) * (size_t)N), *xy = (double *)malloc(sizeof(double) * 2 * (size_t)N);
    for (int k = 0; k < q; k++) T[m++] = quarter[k];
    T[m++] = PI - 0.5 * PI; for (int k = q - 1; k >= 1; k--) T[m++] = PI - quarter[k];
    for (int k = 0; k < q; k++) T[m++] = PI + quarter[k];
    T[m++] = 2 * PI - 0.5 * PI; for (int k = q - 1; k >= 1; k--) T[m++] = 2 * PI - quarter[k];
    for (int k = 0; k < N; k++) { xy[2 * k] = a * py_cos(T[k]); xy[2 * k + 1] = b * py_sin(T[k]); }
    free(ts); free(L); free(quarter);
    *pts = xy; *Tout = T;
    return N;
}
/* gg.ellipse_offset(a, b, T, d) */
static double *ellipse_offset(double a, double b, const double *T, int n, double d) {
    double *o = (double *)malloc(sizeof(double) * 2 * (size_t)(n ? n : 1));
    for (int i = 0; i < n; i++) {
        double t = T[i], x = a * py_cos(t), y = b * py_sin(t), nx = b * py_cos(t), ny = a * py_sin(t), L = py_hypot(nx, ny);
        o[2 * i] = x - d * nx / L; o[2 * i + 1] = y - d * ny / L;
    }
    return o;
}
/* gg.magnet_centres(dia) */
static int magnet_centres(double dia, double *xy) { return mag_centres(dia, xy); }          /* magnets.h: the one copy */
/* gg.oval_magnet_centres(A) */
static int oval_magnet_centres(double A, double *xy) { return mag_oval_centres(A, xy); }

/* the exterior points of every polygon of polys(g), for the max(hypot(...)) of scale_plan: -1 = empty (max() raises) */
static int max_ext_r(const GEOSGeometry *U, double cx, double cy, double *rp) {
    ShpList ps = { 0 }; int any = 0; double best = 0;
    polys(U, &ps);
    for (int i = 0; i < ps.n; i++) {
        const GEOSGeometry *ring = shp_exterior(ps.g[i]); int n = G.GEOSGetNumCoordinates_r(gctx, ring);
        double *xy = (double *)malloc(sizeof(double) * 2 * (size_t)(n + 1)); n = shp_coords(ring, xy, n + 1);
        for (int k = 0; k < n; k++) { double h = py_hypot(xy[2 * k] - cx, xy[2 * k + 1] - cy); if (!any || h > best) best = h; any = 1; }
        free(xy);
    }
    shp_list_free(&ps);
    if (!any) return -1;
    *rp = best; return 0;
}

/* ======================= mesher.scale_plan / _oval_plan ======================= */
static GEOSGeometry *pads_union(const GEOSGeometry *const *pads, int npads) {
    const GEOSGeometry **v = (const GEOSGeometry **)calloc((size_t)npads + 1, sizeof(void *)); int m = 0;
    for (int i = 0; i < npads; i++) if (pads[i] && !shp_is_empty(pads[i])) v[m++] = pads[i];
    GEOSGeometry *U = shp_union_all(v, m); free(v);
    return U;
}
/* mesher._oval_plan(s, ov, sc) */
static int oval_plan(const MgEnv *e, const GEOSGeometry *const *pads, int npads, MgPlan *out) {
    double A = e->oval[0], B = e->oval[1], a = A / 2.0, b = B / 2.0, fin = A, sc = e->scale, tx = 0.0, ty = 0.0;
    double *ring, *T; int nr = ellipse_ring(a, b, 240, &ring, &T);
    double *off = ellipse_offset(a, b, T, nr, 2.4);
    GEOSGeometry *inner = shp_polygon(off, nr), *padsg = NULL, *U = NULL, *c = NULL, *Q = NULL;
    free(off);
    out->sc = sc; out->fin = fin; out->pads = NULL;
    if (npads > 0 && inner) {          /* the try: any failure leaves pads None */
        U = pads_union(pads, npads); if (!U) goto done;
        c = shp_centroid(U); if (!c) goto done;
        double cx = shp_x(c), cy = shp_y(c), k = fin / e->base_mm;
        for (int it = 0; it < 30; it++) {
            shp_free(Q); Q = shp_scale(U, k, k, SHP_ORIGIN_POINT, cx, cy); if (!Q) goto done;
            GEOSGeometry *t = shp_translate(Q, -cx, -cy); if (!t) goto done;
            int r = shp_contains(inner, t); shp_free(t);
            if (r < 0) goto done;
            if (r) break;
            k *= 0.9;
        }
#define FITS(X, Y, R) do { GEOSGeometry *t_ = shp_translate(Q, (X) - cx, (Y) - cy); if (!t_) goto done; R = shp_contains(inner, t_); shp_free(t_); if (R < 0) goto done; } while (0)
        double px = cx * sc, py = cy * sc, qx, qy; int r;
        FITS(px, py, r);
        if (r) { qx = px; qy = py; }
        else {
            double lo = 0.0, hi = 1.0;
            for (int it = 0; it < 24; it++) {
                double mid = (lo + hi) / 2;
                FITS(px * mid, py * mid, r);
                if (r) lo = mid; else hi = mid;
            }
            qx = px * lo; qy = py * lo;
        }
#undef FITS
        double dx = qx - px, dy = qy - py, RL = e->base_mm * sc / 2.0 + 0.1, t = 0.0;
#define COVERS(TT, R) do { R = 1; for (int i_ = 0; i_ < nr; i_++) if (!(py_hypot(ring[2 * i_] - (TT) * dx, ring[2 * i_ + 1] - (TT) * dy) <= RL)) { R = 0; break; } } while (0)
        int cv0; COVERS(0.0, cv0);
        if (sc > 1.0 + 1e-6 && (dx != 0 || dy != 0) && cv0) {
            int cv1; COVERS(1.0, cv1);
            if (cv1) t = 1.0;
            else {
                double lo = 0.0, hi = 1.0;
                for (int it = 0; it < 24; it++) { double mid = (lo + hi) / 2; int cm; COVERS(mid, cm); if (cm) lo = mid; else hi = mid; }
                t = lo;
            }
        }
#undef COVERS
        tx = dx * t; ty = dy * t;
        padsg = shp_translate(Q, qx - cx, qy - cy);
    }
done:
    out->tx = tx; out->ty = ty; out->pads = padsg;
    shp_free(inner); shp_free(U); shp_free(c); shp_free(Q); free(ring); free(T);
    return 1;
}
int mg_scale_plan(const MgEnv *e, const GEOSGeometry *const *pads, int npads, MgPlan *out) {
    double fin = e->final_mm, sc = e->scale, tx = 0.0, ty = 0.0;
    memset(out, 0, sizeof *out);
    if (e->has_oval) return oval_plan(e, pads, npads, out);
    if (!fin || (fabs(sc - 1) < 1e-6 && fabs(fin - e->base_mm) < 1e-6)) return 0;
    GEOSGeometry *U = NULL, *c = NULL, *pg = NULL;
    out->sc = sc; out->fin = fin;
    if (npads > 0) {
        double rp, cx, cy;
        U = pads_union(pads, npads); if (!U) goto done;
        c = shp_centroid(U); if (!c) goto done;
        cx = shp_x(c); cy = shp_y(c);
        if (max_ext_r(U, cx, cy, &rp) < 0) goto done;          /* max() of nothing: ValueError, caught */
        rp = rp * fin / e->base_mm;
        double px = cx * sc, py = cy * sc, lim = py_max(0.0, fin / 2 - rp - 2.0), h = py_hypot(px, py), qx, qy;
        if (h > lim) { qx = px * lim / h; qy = py * lim / h; } else { qx = px; qy = py; }
        double slack = py_max(0.0, (e->base_mm * sc - fin) / 2), dx = qx - px, dy = qy - py, dh = py_hypot(dx, dy);
        if (dh > slack && dh > 0) { dx = dx * slack / dh; dy = dy * slack / dh; }
        tx = dx; ty = dy;
        GEOSGeometry *s1 = shp_scale(U, fin / e->base_mm, fin / e->base_mm, SHP_ORIGIN_POINT, cx, cy);
        if (s1) { pg = shp_translate(s1, qx - cx, qy - cy); shp_free(s1); }
    }
done:
    out->tx = tx; out->ty = ty; out->pads = pg;
    shp_free(U); shp_free(c);
    return 1;
}
void mg_plan_free(MgPlan *p) { shp_free(p->pads); p->pads = NULL; }

GEOSGeometry *mg_stand(const MgPlan *p) {
    GEOSGeometry *g = shp_buffer_q(p->pads, 1.2, 6);
    if (!g) fail_geos("the standing place");
    return g;
}

/* ======================= command values (as buildcmds.c holds them) ======================= */
static BcNum NUM(double v, int t) { BcNum n; n.v = v; n.t = (unsigned char)t; return n; }
static int is_fl(int t) { return t == BC_FLOAT || t == BC_Z; }
static int res_t(BcNum a, BcNum b) {          /* the type of a + b, a - b, a * b (buildcmds.c res_t) */
    if (a.t == BC_NP) return BC_NP;
    if (b.t == BC_NP) return a.t == BC_Z ? BC_FLOAT : BC_NP;
    if (is_fl(a.t) || is_fl(b.t)) return BC_FLOAT;
    return BC_INT;
}
static BcNum n_sub(BcNum a, BcNum b) { BcNum r = NUM(a.v - b.v, res_t(a, b)); if (r.t == BC_INT) r.v += 0.0; return r; }
static BcNum n_add(BcNum a, BcNum b) { BcNum r = NUM(a.v + b.v, res_t(a, b)); if (r.t == BC_INT) r.v += 0.0; return r; }
static BcNum n_mul(BcNum a, BcNum b) { BcNum r = NUM(a.v * b.v, res_t(a, b)); if (r.t == BC_INT) r.v += 0.0; return r; }
static BcNum n_min(BcNum a, BcNum b) { return b.v < a.v ? b : a; }
static BcNum n_max(BcNum a, BcNum b) { return b.v > a.v ? b : a; }

static BcVal v_none(void) { BcVal v; memset(&v, 0, sizeof v); return v; }
static BcVal v_num(BcNum n) { BcVal v = v_none(); v.t = BCV_NUM; v.num = n; return v; }
static BcVal v_f(double x) { return v_num(NUM(x, BC_FLOAT)); }
static BcVal v_str(const char *s) { BcVal v = v_none(); v.t = BCV_STR; v.s = strdup(s); return v; }
static BcVal v_list(void) { BcVal v = v_none(); v.t = BCV_LIST; return v; }
static BcVal v_dict(void) { BcVal v = v_none(); v.t = BCV_DICT; return v; }
static void v_grow(BcVal *l) {
    if (l->n < l->cap) return;
    l->cap = l->cap ? 2 * l->cap : 8;
    l->v = (BcVal *)realloc(l->v, sizeof(BcVal) * (size_t)l->cap);
    if (l->t == BCV_DICT) l->k = (const char **)realloc((void *)l->k, sizeof(char *) * (size_t)l->cap);
}
static void v_push(BcVal *l, BcVal x) { v_grow(l); l->v[l->n++] = x; }
static void v_free(BcVal *a) {
    if (a->t == BCV_STR) free(a->s);
    else if (a->t == BCV_PTS) free(a->pts);
    else if (a->t == BCV_LIST || a->t == BCV_DICT) { for (int i = 0; i < a->n; i++) v_free(&a->v[i]); free(a->v); free((void *)a->k); }
    *a = v_none();
}
static void d_set(BcVal *d, const char *key, BcVal x) {
    for (int i = 0; i < d->n; i++) if (!strcmp(d->k[i], key)) { v_free(&d->v[i]); d->v[i] = x; return; }
    v_grow(d); d->k[d->n] = key; d->v[d->n++] = x;
}
static BcVal v_copy(const BcVal *a) {
    BcVal v = *a;
    if (a->t == BCV_STR) v.s = strdup(a->s);
    else if (a->t == BCV_PTS) { v.pts = (double *)malloc(sizeof(double) * (size_t)(a->n * a->dim + 1)); memcpy(v.pts, a->pts, sizeof(double) * (size_t)(a->n * a->dim)); }
    else if (a->t == BCV_LIST || a->t == BCV_DICT) {
        v.cap = a->n; v.v = a->n ? (BcVal *)malloc(sizeof(BcVal) * (size_t)a->n) : NULL; v.k = NULL;
        if (a->t == BCV_DICT && a->n) { v.k = (const char **)malloc(sizeof(char *) * (size_t)a->n); memcpy((void *)v.k, a->k, sizeof(char *) * (size_t)a->n); }
        for (int i = 0; i < a->n; i++) v.v[i] = v_copy(&a->v[i]);
    }
    return v;
}
static BcVal v_xy(double x, double y) { BcVal l = v_list(); v_push(&l, v_f(x)); v_push(&l, v_f(y)); return l; }
static BcVal cmd_new(const char *command, BcVal params) { BcVal d = v_dict(); d_set(&d, "command", v_str(command)); d_set(&d, "params", params); return d; }
static const char *v_cmd(const BcVal *c) { const BcVal *k = bc_get(c, "command"); return k && k->t == BCV_STR ? k->s : ""; }
static const BcVal *v_params(const BcVal *c) { return bc_get(c, "params"); }
static int v_isnum(const BcVal *v) { return v && v->t == BCV_NUM; }
static double v_d(const BcVal *v, double dflt) { return v_isnum(v) ? v->num.v : dflt; }
static int v_truthy(const BcVal *v) {
    if (!v) return 0;
    switch (v->t) { case BCV_NONE: return 0; case BCV_NUM: return v->num.v != 0; case BCV_STR: return v->s[0] != 0; default: return v->n != 0; }
}
static int v_streq(const BcVal *v, const char *s) { return v && v->t == BCV_STR && !strcmp(v->s, s); }
static void v_str_of(const BcVal *v, char *out, size_t n) {          /* str(params.get("name", "")) */
    if (!v) { out[0] = 0; return; }
    if (v->t == BCV_STR) snprintf(out, n, "%s", v->s);
    else if (v->t == BCV_NUM && v->num.t == BC_INT) snprintf(out, n, "%lld", (long long)v->num.v);
    else if (v->t == BCV_NUM) mg_float_repr(v->num.v, out, (int)n);
    else snprintf(out, n, "?");
}
/* a point value: [x, y] (a list of numbers) or one point of a points list */
static int v_pt(const BcVal *v, double *x, double *y) {
    if (!v) return 0;
    if (v->t == BCV_LIST && v->n >= 2 && v_isnum(&v->v[0]) && v_isnum(&v->v[1])) { *x = v->v[0].num.v; *y = v->v[1].num.v; return 1; }
    if (v->t == BCV_PTS && v->n == 1) { *x = v->pts[0]; *y = v->pts[1]; return 1; }
    return 0;
}
/* a points value -> malloc'd x, y pairs; the count, -1 if it is not one */
static int v_pts(const BcVal *v, double **xy) {
    if (!v) return -1;
    if (v->t == BCV_PTS) {
        *xy = (double *)malloc(sizeof(double) * 2 * (size_t)(v->n + 1));
        for (int i = 0; i < v->n; i++) { (*xy)[2 * i] = v->pts[i * v->dim]; (*xy)[2 * i + 1] = v->pts[i * v->dim + 1]; }
        return v->n;
    }
    if (v->t == BCV_LIST) {
        *xy = (double *)malloc(sizeof(double) * 2 * (size_t)(v->n + 1));
        for (int i = 0; i < v->n; i++) if (!v_pt(&v->v[i], &(*xy)[2 * i], &(*xy)[2 * i + 1])) { free(*xy); return -1; }
        return v->n;
    }
    return -1;
}
static BcGroup grp_new(void) { BcGroup g; memset(&g, 0, sizeof g); return g; }
static void grp_push(BcGroup *g, BcVal c) {
    if (g->n == g->cap) { g->cap = g->cap ? 2 * g->cap : 8; g->cmds = (BcVal *)realloc(g->cmds, sizeof(BcVal) * (size_t)g->cap); }
    g->cmds[g->n++] = c;
}
static void groups_push(BcGroups *gs, BcGroup g) {
    if (gs->n == gs->cap) { gs->cap = gs->cap ? 2 * gs->cap : 16; gs->g = (BcGroup *)realloc(gs->g, sizeof(BcGroup) * (size_t)gs->cap); }
    gs->g[gs->n++] = g;
}

/* ======================= mesher.rescale ======================= */
typedef struct { double H, f, cap, floor_, tx, ty; } Rs;
static BcNum rs_Z(const Rs *r, BcNum z) {          /* Z(z) */
    BcNum H = NUM(r->H, BC_FLOAT), f = NUM(r->f, BC_FLOAT);
    if (z.v >= r->H) return n_min(n_add(H, n_mul(n_sub(z, H), f)), NUM(r->H + r->cap, BC_FLOAT));
    return n_max(n_sub(H, n_mul(n_sub(H, z), f)), n_min(NUM(r->floor_, BC_FLOAT), z));
}
static BcVal rs_P(const Rs *r, const BcVal *q) {          /* P(q) = [q[0] * f + tx, q[1] * f + ty] */
    double x = 0, y = 0; v_pt(q, &x, &y);
    return v_xy(x * r->f + r->tx, y * r->f + r->ty);
}
int mg_rescale(const BcGroups *in, double f, double mm, double cap, double floor_, double tx, double ty, const GEOSGeometry *pads, BcGroups *out) {
    Rs r; r.f = f; r.cap = cap; r.floor_ = floor_; r.tx = tx; r.ty = ty; r.H = 4.0;
    BcNum Hn = NUM(4.0, BC_FLOAT); int found = 0;
    for (int gi = 0; gi < in->n && !found; gi++)
        for (int i = 0; i < in->g[gi].n; i++) {
            const BcVal *c = &in->g[gi].cmds[i], *p = v_params(c);
            if (!strcmp(v_cmd(c), "solid.extrude") && v_streq(bc_get(p, "name"), "Base")) { const BcVal *d = bc_get(p, "distance"); if (!v_isnum(d)) return fail("KeyError: 'distance'"); Hn = d->num; found = 1; break; }
        }
    r.H = Hn.v;
    for (int gi = 0; gi < in->n; gi++) {
        const BcGroup *g = &in->g[gi]; BcGroup ng = grp_new(); int is_base = 0, is_mag = 0, mag_done = 0; BcNum off = NUM(0.0, BC_FLOAT);
        for (int i = 0; i < g->n; i++) {
            if (strcmp(v_cmd(&g->cmds[i]), "solid.extrude")) continue;
            char nm[128]; v_str_of(bc_get(v_params(&g->cmds[i]), "name"), nm, sizeof nm);
            if (!strcmp(nm, "Base")) is_base = 1;
            if (!strcmp(nm, "MagCut")) is_mag = 1;
        }
        for (int i = 0; i < g->n; i++) {
            const char *k = v_cmd(&g->cmds[i]); const BcVal *p0 = v_params(&g->cmds[i]);
            BcVal p = p0 ? v_copy(p0) : v_dict();
            if (is_base) {
                if (!strcmp(k, "sketch.circle.center")) d_set(&p, "radius", v_f(mm / 2.0));
            } else if (is_mag) {
                if (!strcmp(k, "sketch.circle.center")) {
                    if (mag_done) { v_free(&p); continue; }
                    mag_done = 1; double xy[10]; int n = magnet_centres(mm, xy);
                    for (int m = 0; m < n; m++) { BcVal q = v_copy(&p); d_set(&q, "center", v_xy(xy[2 * m], xy[2 * m + 1])); grp_push(&ng, cmd_new(k, q)); }
                    v_free(&p); continue;
                }
            } else if (!strcmp(k, "sketch.create")) {
                const BcVal *o = bc_get(&p, "offset"); off = v_isnum(o) ? o->num : NUM(0.0, BC_FLOAT);
                d_set(&p, "offset", v_num(rs_Z(&r, off)));
            } else if (!strcmp(k, "sketch.circle.center")) {
                BcVal np = rs_P(&r, bc_get(&p, "center")); d_set(&p, "center", np);
                const BcVal *rad = bc_get(&p, "radius"); d_set(&p, "radius", v_num(n_mul(rad ? rad->num : NUM(0, BC_FLOAT), NUM(f, BC_FLOAT))));
            } else if (!strcmp(k, "sketch.line")) {
                const BcVal *pts = bc_get(&p, "points"); double *xy; int n = v_pts(pts, &xy);
                if (n < 0) { v_free(&p); bc_groups_free(out); return fail("TypeError: points"); }
                BcVal l = v_list();
                for (int m = 0; m < n; m++) v_push(&l, v_xy(xy[2 * m] * f + tx, xy[2 * m + 1] * f + ty));
                free(xy); d_set(&p, "points", l);
            } else if (!strcmp(k, "sketch.arc.three_point")) {
                d_set(&p, "start", rs_P(&r, bc_get(&p, "start"))); d_set(&p, "through", rs_P(&r, bc_get(&p, "through"))); d_set(&p, "end", rs_P(&r, bc_get(&p, "end")));
            } else if (!strcmp(k, "solid.extrude")) {
                const BcVal *dv = bc_get(&p, "distance"); BcNum d = dv ? dv->num : NUM(0, BC_FLOAT);
                int cut = v_streq(bc_get(&p, "operation"), "cut");
                if (cut && v_streq(bc_get(&p, "direction"), "negative")) d_set(&p, "distance", v_num(n_max(n_sub(rs_Z(&r, off), rs_Z(&r, n_sub(off, d))), NUM(0.01, BC_FLOAT))));
                else if (!cut) d_set(&p, "distance", v_num(n_max(n_sub(rs_Z(&r, n_add(off, d)), rs_Z(&r, off)), NUM(0.01, BC_FLOAT))));
                const BcVal *lf = bc_get(&p, "loft");
                if (v_truthy(lf) && lf->t == BCV_LIST && lf->n >= 2) {
                    BcVal nl = v_list(); v_push(&nl, v_copy(&lf->v[0])); v_push(&nl, v_num(rs_Z(&r, lf->v[1].num))); d_set(&p, "loft", nl);
                }
            }
            grp_push(&ng, cmd_new(k, p));
        }
        groups_push(out, ng);
    }
    if (pads && !shp_is_empty(pads)) {          /* where the miniature stands: flat at deck height, at the miniature's real size */
        BcGroup g = grp_new(); BcVal p = v_dict();
        d_set(&p, "plane", v_str("XY")); d_set(&p, "offset", v_f(r.H + cap + 1.0)); d_set(&p, "name", v_str("Stand"));
        grp_push(&g, cmd_new("sketch.create", p));
        GEOSGeometry *b = shp_buffer_q(pads, 1.2, 6); ShpList ps = { 0 };
        if (!b) { bc_groups_free(out); return fail_geos("rescale"); }
        polys(b, &ps); shp_free(b);
        for (int i = 0; i < ps.n; i++) {
            const GEOSGeometry *ring = shp_exterior(ps.g[i]); int n = G.GEOSGetNumCoordinates_r(gctx, ring);
            double *xy = (double *)malloc(sizeof(double) * 2 * (size_t)(n + 1)); n = shp_coords(ring, xy, n + 1);
            BcVal l = v_list();
            for (int m = 0; m < n - 1; m++) v_push(&l, v_xy(py_round(xy[2 * m], 4), py_round(xy[2 * m + 1], 4)));
            free(xy);
            BcVal q = v_dict(); d_set(&q, "points", l); d_set(&q, "closed", v_num(NUM(1, BC_BOOL)));
            grp_push(&g, cmd_new("sketch.line", q));
        }
        shp_list_free(&ps);
        grp_push(&g, cmd_new("sketch.finish", v_dict()));
        BcVal e = v_dict(), tg = v_list(); v_push(&tg, v_str("Base"));
        d_set(&e, "sketch", v_str("Stand")); d_set(&e, "distance", v_f(cap + 1.0)); d_set(&e, "direction", v_str("negative"));
        d_set(&e, "operation", v_str("cut")); d_set(&e, "targets", tg); d_set(&e, "name", v_str("FStand"));
        grp_push(&g, cmd_new("solid.extrude", e));
        groups_push(out, g);
    }
    return 0;
}

/* ======================= mesher.sketch_region ======================= */
typedef struct { double *xy; int n, cap; } Run;
static void run_add(Run *r, double x, double y) {
    if (r->n == r->cap) { r->cap = r->cap ? 2 * r->cap : 64; r->xy = (double *)realloc(r->xy, sizeof(double) * 2 * (size_t)r->cap); }
    r->xy[2 * r->n] = x; r->xy[2 * r->n + 1] = y; r->n++;
}
static double unwrap(double t, double ref, int sign) { while (sign * (t - ref) < 0) t += sign * 2 * PI; return t; }
/* mesher._arc_pts(a, m, b, step=0.12): appended to the run */
static void arc_pts(const double *a, const double *m, const double *b, Run *run) {
    double ax = a[0], ay = a[1], bx = b[0], by = b[1], mx = m[0], my = m[1], step = 0.12;
    double d = 2 * (ax * (by - my) + bx * (my - ay) + mx * (ay - by));
    if (fabs(d) < 1e-12) { run_add(run, ax, ay); run_add(run, mx, my); run_add(run, bx, by); return; }
    double ux = ((ax * ax + ay * ay) * (by - my) + (bx * bx + by * by) * (my - ay) + (mx * mx + my * my) * (ay - by)) / d;
    double uy = ((ax * ax + ay * ay) * (mx - bx) + (bx * bx + by * by) * (ax - mx) + (mx * mx + my * my) * (bx - ax)) / d;
    double r = py_hypot(ax - ux, ay - uy);
    double t0 = py_atan2(ay - uy, ax - ux), t1 = py_atan2(my - uy, mx - ux), t2 = py_atan2(by - uy, bx - ux);
    int sign = 1;
    if (!(unwrap(t1, t0, 1) <= unwrap(t2, t0, 1))) sign = -1;
    double t2u = unwrap(t2, t0, sign);
    if (fabs(t2u - t0) * r > 2.5 * (py_hypot(mx - ax, my - ay) + py_hypot(bx - mx, by - my)) + 1e-9) { run_add(run, ax, ay); run_add(run, mx, my); run_add(run, bx, by); return; }
    double nn = fabs(t2u - t0) * r / step; int n = (int)nn; if (n < 2) n = 2;
    for (int i = 0; i <= n; i++) run_add(run, ux + r * py_cos(t0 + (t2u - t0) * i / n), uy + r * py_sin(t0 + (t2u - t0) * i / n));
}
/* sketch_region(cmds): the filled region (NULL with *none = 1 for None); MG_ERR through *err */
static GEOSGeometry *sketch_region(const BcVal *const *cmds, int nc, int *none, int *err) {
    ShpList loops = { 0 }; Run run = { 0 }; GEOSGeometry *reg = NULL;
    *none = 0; *err = 0;
    for (int i = 0; i < nc; i++) {
        const char *k = v_cmd(cmds[i]); const BcVal *p = v_params(cmds[i]);
        if (!strcmp(k, "sketch.circle.center")) {
            double x, y; v_pt(bc_get(p, "center"), &x, &y);
            GEOSGeometry *c = shp_point_buffer(x, y, v_d(bc_get(p, "radius"), 0), NSEG / 4);
            if (!c) goto gerr;
            shp_list_push(&loops, c);
        } else if (!strcmp(k, "sketch.line") && v_truthy(bc_get(p, "closed"))) {
            double *xy; int n = v_pts(bc_get(p, "points"), &xy);
            if (n < 0) goto terr;
            if (n >= 3) { GEOSGeometry *q = shp_polygon(xy, n); free(xy); if (!q) goto gerr; shp_list_push(&loops, q); }
            else free(xy);
        } else if (!strcmp(k, "sketch.line")) {
            double *xy; int n = v_pts(bc_get(p, "points"), &xy);
            if (n < 0) goto terr;
            for (int m = 0; m < n; m++) run_add(&run, xy[2 * m], xy[2 * m + 1]);
            free(xy);
        } else if (!strcmp(k, "sketch.arc.three_point")) {
            double a[2], m[2], b[2];
            if (!v_pt(bc_get(p, "start"), &a[0], &a[1]) || !v_pt(bc_get(p, "through"), &m[0], &m[1]) || !v_pt(bc_get(p, "end"), &b[0], &b[1])) goto terr;
            arc_pts(a, m, b, &run);
        }
        if (run.n >= 4 && py_hypot(run.xy[2 * (run.n - 1)] - run.xy[0], run.xy[2 * (run.n - 1) + 1] - run.xy[1]) < 2e-3) {
            GEOSGeometry *q = shp_polygon(run.xy, run.n); if (!q) goto gerr;
            shp_list_push(&loops, q); run.n = 0;
        }
    }
    if (run.n >= 3) { GEOSGeometry *q = shp_polygon(run.xy, run.n); if (!q) goto gerr; shp_list_push(&loops, q); }
    {
        ShpList lp = { 0 }; double *area;
        for (int i = 0; i < loops.n; i++) {
            GEOSGeometry *b = shp_buffer_q(loops.g[i], 0, 16);
            if (!b) { shp_list_free(&lp); goto gerr; }
            if (!shp_is_empty(b) && shp_area(b) > 1e-5) shp_list_push(&lp, b); else shp_free(b);
        }
        area = (double *)malloc(sizeof(double) * (size_t)(lp.n + 1));
        for (int i = 0; i < lp.n; i++) area[i] = -shp_area(lp.g[i]);
        for (int i = 1; i < lp.n; i++) {          /* sorted(key=-area): a stable insertion sort */
            double ka = area[i]; GEOSGeometry *g = lp.g[i]; int j = i - 1;
            while (j >= 0 && area[j] > ka) { area[j + 1] = area[j]; lp.g[j + 1] = lp.g[j]; j--; }
            area[j + 1] = ka; lp.g[j + 1] = g;
        }
        free(area);
        for (int i = 0; i < lp.n; i++) {
            if (!reg) { reg = shp_clone(lp.g[i]); continue; }
            int c = shp_contains(reg, lp.g[i]); GEOSGeometry *nr;
            if (c < 0) { shp_list_free(&lp); goto gerr; }
            nr = c ? shp_difference(reg, lp.g[i]) : shp_union(reg, lp.g[i]);
            shp_free(reg); reg = nr;
            if (!reg) { shp_list_free(&lp); goto gerr; }
        }
        shp_list_free(&lp);
    }
    shp_list_free(&loops); free(run.xy);
    if (!reg) *none = 1;
    return reg;
gerr:
    fail_geos("sketch_region"); goto out;
terr:
    fail("TypeError: a sketch command's points");
out:
    shp_list_free(&loops); free(run.xy); shp_free(reg); *err = 1;
    return NULL;
}

/* ======================= mesher.field_ops ======================= */
static void step_push(MgSteps *s, int kind, const char *name, GEOSGeometry *g, double prec, double a, double b, double c, int chain) {
    if (s->n == s->cap) { s->cap = s->cap ? 2 * s->cap : 32; s->v = (MgStep *)realloc(s->v, sizeof(MgStep) * (size_t)s->cap); }
    MgStep *t = &s->v[s->n++]; memset(t, 0, sizeof *t);
    t->kind = kind; t->nlen = (int)strlen(name); if (t->nlen > 60) t->nlen = 60;
    memcpy(t->name, name, (size_t)t->nlen); t->g.g = g; t->g.prec = prec; t->a = a; t->b = b; t->c = c; t->chain = chain;
}
void mg_steps_free(MgSteps *s) { for (int i = 0; i < s->n; i++) shp_free(s->v[i].g.g); free(s->v); memset(s, 0, sizeof *s); }
void mg_base_free(MgBase *b) { shp_free(b->disc.g); shp_free(b->guard.g); free(b->TR); free(b->OU); free(b->cens); memset(b, 0, sizeof *b); }

int mg_field_ops(const MgEnv *e, const BcGroups *groups, MgBase *base, int *has_base, MgSteps *steps, MgStrs *skipped) {
    double *chains = NULL; int nchains = 0, rc = 0, has_mag = 0;
    double mag_r = 0, mag_off = 0, *mag_c = NULL; int mag_n = 0;
    const BcVal **cur = NULL; int ncur = 0, capcur = 0;
    memset(base, 0, sizeof *base); *has_base = 0;
    for (int gi = 0; gi < groups->n; gi++) {
        const BcGroup *g = &groups->g[gi]; int any = 0; double off = 0.0;
        for (int i = 0; i < g->n; i++) if (!strcmp(v_cmd(&g->cmds[i]), "solid.extrude")) { any = 1; break; }
        if (!any) continue;
        ncur = 0;
        for (int i = 0; i < g->n; i++) {
            const char *k = v_cmd(&g->cmds[i]); const BcVal *p = v_params(&g->cmds[i]);
            if (!strcmp(k, "sketch.create")) { ncur = 0; off = v_d(bc_get(p, "offset"), 0.0); }
            else if (!strncmp(k, "sketch.", 7) && strcmp(k, "sketch.finish")) {
                if (ncur == capcur) { capcur = capcur ? 2 * capcur : 32; cur = (const BcVal **)realloc((void *)cur, sizeof(void *) * (size_t)capcur); }
                cur[ncur++] = &g->cmds[i];
            } else if (!strcmp(k, "solid.extrude")) {
                char name[256]; v_str_of(bc_get(p, "name"), name, sizeof name);
                int cut = v_streq(bc_get(p, "operation"), "cut");
                const BcVal *dv = bc_get(p, "distance"); if (!v_isnum(dv)) { rc = fail("KeyError: 'distance'"); goto done; }
                double dist = dv->num.v, tap = fabs(v_d(bc_get(p, "taper"), 0.0));
                int none, err; GEOSGeometry *reg = sketch_region(cur, ncur, &none, &err), *r2;
                if (err) { rc = MG_ERR; goto done; }
                if (none || shp_is_empty(reg)) { shp_free(reg); continue; }
                if (!strcmp(name, "Base")) {
                    shp_free(reg);
                    double R0 = v_d(bc_get(v_params(cur[0]), "radius"), 0), H = off + dist, rim_tan = py_tan(tap * DEG2RAD), rt = R0 - H * rim_tan;
                    GEOSGeometry *poly; double ref_area = 0; int has_ref = 0;
                    mg_base_free(base);
                    if (e->has_oval) {
                        double oa = e->oval[0] / 2.0, ob = e->oval[1] / 2.0, *ring, *T; int n = ellipse_ring(oa, ob, NSEG_OVAL, &ring, &T);
                        double *topr = ellipse_offset(oa, ob, T, n, H * rim_tan);
                        poly = shp_polygon(topr, n); free(T);
                        base->has_oval = 1; base->TR = topr; base->nTR = n; base->OU = ring; base->nOU = n;
                        ref_area = PI * py_pow(rt - 0.02, 2); has_ref = 1;
                    } else {
                        double xy[2 * NSEG];
                        for (int a = 0; a < NSEG; a++) { double an = 2 * PI * a / NSEG; xy[2 * a] = rt * py_cos(an); xy[2 * a + 1] = rt * py_sin(an); }
                        poly = shp_polygon(xy, NSEG);
                    }
                    if (!poly) { rc = fail_geos("the base"); goto done; }
                    base->disc.g = P(poly); base->disc.prec = GRID; shp_free(poly);
                    if (!base->disc.g) { rc = fail_geos("the base"); goto done; }
                    GEOSGeometry *gb = shp_buffer(base->disc.g, -0.02, 16, SHP_CAP_ROUND, SHP_JOIN_MITRE, 5.0, 0);
                    if (!gb) { rc = fail_geos("the base"); goto done; }
                    base->guard.g = P(gb); base->guard.prec = GRID; shp_free(gb);
                    if (!base->guard.g) { rc = fail_geos("the base"); goto done; }
                    base->R0 = R0; base->H = H; base->rim_tan = rim_tan;
                    base->ref_area = (has_ref && ref_area != 0.0) ? ref_area : shp_area(base->guard.g);
                    *has_base = 1;
                    continue;
                }
                if (!strcmp(name, "MagCut")) {
                    shp_free(reg);
                    has_mag = 1; mag_r = v_d(bc_get(v_params(cur[0]), "radius"), 0); mag_off = off; free(mag_c);
                    if (e->has_oval) { mag_c = (double *)malloc(sizeof(double) * 6); mag_n = oval_magnet_centres(e->oval[0], mag_c); }
                    else {
                        mag_c = (double *)malloc(sizeof(double) * 2 * (size_t)(ncur + 1)); mag_n = ncur;
                        for (int m = 0; m < ncur; m++) if (!v_pt(bc_get(v_params(cur[m]), "center"), &mag_c[2 * m], &mag_c[2 * m + 1])) { rc = fail("KeyError: 'center'"); goto done; }
                    }
                    continue;
                }
                if (!*has_base) { shp_free(reg); continue; }
                if (cut && shp_area(reg) > 1.5 * base->ref_area) {
                    char t[400]; snprintf(t, sizeof t, "%s: a cut far larger than the base, left out", name); mg_strs_push(skipped, t);
                    shp_free(reg); continue;
                }
                r2 = shp_intersection(reg, base->guard.g); shp_free(reg);
                if (!r2) { rc = fail_geos("field_ops"); goto done; }
                if (shp_is_empty(r2)) { shp_free(r2); continue; }
                /* the try: a GEOS error here skips this step */
                if (cut) {
                    if (v_streq(bc_get(p, "direction"), "negative")) step_push(steps, 4, name, r2, 0.0, off - dist, off, 0.0, -1);
                    else step_push(steps, 4, name, r2, 0.0, off, off + dist, 0.0, -1);
                    r2 = NULL;
                } else if (tap > 0.01) {
                    ShpList ps = { 0 }; double tn = py_tan(tap * DEG2RAD);
                    polys(r2, &ps);
                    for (int m = 0; m < ps.n; m++) { step_push(steps, 3, name, ps.g[m], 0.0, off, dist, tn, -1); ps.g[m] = NULL; }
                    shp_list_free(&ps);
                } else if (v_truthy(bc_get(p, "loft"))) {
                    const BcVal *lf = bc_get(p, "loft");
                    GEOSGeometry *pr = P(r2);
                    if (!pr) { char t[400]; snprintf(t, sizeof t, "%s: GEOSException", name); mg_strs_push(skipped, t); }
                    else {
                        double ck = lf->v[0].num.v; int ch = -1;
                        for (int m = 0; m < nchains; m++) if (chains[m] == ck) { ch = m; break; }
                        if (ch < 0) { chains = (double *)realloc(chains, sizeof(double) * (size_t)(nchains + 1)); chains[nchains] = ck; ch = nchains++; }
                        step_push(steps, 2, name, pr, GRID, off + dist, lf->v[1].num.v, 0.0, ch);
                    }
                } else { step_push(steps, 1, name, r2, 0.0, off + dist, 0.0, 0.0, -1); r2 = NULL; }
                shp_free(r2);
            }
        }
    }
    if (*has_base && has_mag) { base->has_mag = 1; base->rm = mag_r; base->dm = mag_off; base->cens = mag_c; base->ncen = mag_n; mag_c = NULL; }
done:
    free(chains); free(mag_c); free((void *)cur);
    return rc;
}

/* ======================= cmesh.pack_ops ======================= */
static GEOSWKBWriter *wkbw;
/* cmesh._geom: u32 len + to_wkb(g, output_dimension=2, byte_order=1, include_srid=False) + f64 precision */
static int put_geom(MgBuf *o, const GEOSGeometry *g, double prec) {
    if (!wkbw) {
        wkbw = G.GEOSWKBWriter_create_r(gctx);
        G.GEOSWKBWriter_setOutputDimension_r(gctx, wkbw, 2); G.GEOSWKBWriter_setByteOrder_r(gctx, wkbw, 1);
        G.GEOSWKBWriter_setFlavor_r(gctx, wkbw, 1); G.GEOSWKBWriter_setIncludeSRID_r(gctx, wkbw, 0);
    }
    size_t n = 0; unsigned char *b = G.GEOSWKBWriter_write_r(gctx, wkbw, g, &n);
    if (!b) return fail_geos("to_wkb");
    buf_u32(o, (uint32_t)n); mg_buf_add(o, b, n); buf_f64(o, prec); G.GEOSFree_r(gctx, b);
    return 0;
}
int mg_pack_ops(const MgEnv *e, const MgBase *base, const MgSteps *steps, const GEOSGeometry *stand, double stand_H, double stand_sc, MgBuf *o) {
    mg_buf_add(o, "BBF1", 4); buf_f64(o, base->R0); buf_f64(o, base->H); buf_f64(o, base->rim_tan);
    if (base->has_mag) {
        buf_u8(o, 1); buf_f64(o, base->rm); buf_f64(o, base->dm); buf_u32(o, (uint32_t)base->ncen);
        for (int i = 0; i < base->ncen; i++) { buf_f64(o, base->cens[2 * i]); buf_f64(o, base->cens[2 * i + 1]); }
    } else buf_u8(o, 0);
    if (put_geom(o, base->disc.g, base->disc.prec)) return MG_ERR;
    if (base->has_oval) {
        buf_u8(o, 1); buf_u32(o, (uint32_t)base->nTR); mg_buf_add(o, base->TR, sizeof(double) * 2 * (size_t)base->nTR);
        buf_u32(o, (uint32_t)base->nOU); mg_buf_add(o, base->OU, sizeof(double) * 2 * (size_t)base->nOU);
    } else buf_u8(o, 0);
    if (put_geom(o, base->guard.g, base->guard.prec)) return MG_ERR;
    buf_f64(o, e->tile);
    buf_u32(o, (uint32_t)(steps->n + (stand ? 1 : 0)));
    for (int i = 0; i < steps->n; i++) {
        const MgStep *s = &steps->v[i];
        buf_u8(o, (unsigned)s->kind); buf_u8(o, (unsigned)s->nlen); mg_buf_add(o, s->name, (size_t)s->nlen);
        if (put_geom(o, s->g.g, s->g.prec)) return MG_ERR;
        buf_f64(o, s->a); buf_f64(o, s->b); buf_f64(o, s->c); buf_i32(o, s->chain);
    }
    if (stand) {
        buf_u8(o, 5); buf_u8(o, 5); mg_buf_add(o, "Stand", 5);
        if (put_geom(o, stand, 0.0)) return MG_ERR;
        buf_f64(o, stand_H); buf_f64(o, stand_sc); buf_f64(o, 0.0); buf_i32(o, -1);
    }
    return 0;
}

/* ======================= the sig ======================= */
static void repr_str(MgBuf *o, const char *s) {          /* repr(str) */
    int sq = strchr(s, '\'') != NULL, dq = strchr(s, '"') != NULL; char q = (sq && !dq) ? '"' : '\'', t[8];
    buf_u8(o, (unsigned char)q);
    for (const unsigned char *p = (const unsigned char *)s; *p; p++) {
        if (*p == '\\') buf_s(o, "\\\\");
        else if (*p == (unsigned char)q) { buf_u8(o, '\\'); buf_u8(o, q); }
        else if (*p == '\t') buf_s(o, "\\t");
        else if (*p == '\n') buf_s(o, "\\n");
        else if (*p == '\r') buf_s(o, "\\r");
        else if (*p < 0x20 || *p == 0x7f) { snprintf(t, sizeof t, "\\x%02x", *p); buf_s(o, t); }
        else buf_u8(o, *p);
    }
    buf_u8(o, (unsigned char)q);
}
static void repr_round2(MgBuf *o, BcNum n) {          /* repr(round(op[2], 2)) */
    char t[64];
    if (n.t == BC_INT || n.t == BC_BOOL) { snprintf(t, sizeof t, "%lld", (long long)n.v); buf_s(o, t); return; }
    if (n.t == BC_NP) { buf_s(o, "np.float64("); buf_float(o, rint(n.v * 100.0) / 100.0); buf_s(o, ")"); return; }
    buf_float(o, py_round(n.v, 2));
}
int mg_sig(const BcScene *s, const char *style, const MgEnv *e, char *out, int cap) {
    MgBuf b = { 0 }; char h[33], t1[40], t2[40];
    buf_s(&b, "[");
    for (int i = 0; i < s->nops; i++) {
        const BcOp *op = s->ops[i];
        if (i) buf_s(&b, ", ");
        buf_s(&b, op->type == BC_PRISM ? "('prism', " : "('taper', "); repr_str(&b, op->kind->s); buf_s(&b, ", ");
        repr_round2(&b, op->n[0]); buf_s(&b, ", [");
        for (int j = 0; j < op->np; j++) {
            const GEOSGeometry *p = op->polys[j];
            if (shp_type(p) != GEOS_POLYGON) { free(b.b); return fail("AttributeError: object has no attribute 'exterior'"); }
            if (j) buf_s(&b, ", ");
            buf_s(&b, "[");
            if (!shp_is_empty(p)) {
                const GEOSGeometry *ring = shp_exterior(p); int n = G.GEOSGetNumCoordinates_r(gctx, ring);
                double *xy = (double *)malloc(sizeof(double) * 2 * (size_t)(n + 1)); n = shp_coords(ring, xy, n + 1);
                if (shp_has_z(ring)) { free(xy); free(b.b); return fail("sig: a 3D outline"); }
                for (int k = 0; k < 2 * n; k++) { if (k) buf_s(&b, ", "); buf_float(&b, py_round(xy[k], 1)); }
                free(xy);
            }
            buf_s(&b, "]");
        }
        buf_s(&b, "])");
    }
    buf_s(&b, "]");
    mg_md5_hex(b.b, b.n, h); free(b.b); h[16] = 0;
    fmt_g(t1, sizeof t1, e->base_mm);
    if (e->has_oval) { fmt_g(t2, sizeof t2, e->oval[1]); snprintf(out, (size_t)cap, "%s%so%s%s", style, t1, t2, h); }
    else snprintf(out, (size_t)cap, "%s%s%s", style, t1, h);
    return 0;
}
double mg_want_mm(const BcScene *s, double H, const MgEnv *e, int has_plan, const MgPlan *plan) {
    double mx = H; int any = 0, fin = e->final_mm != 0;
    for (int i = 0; i < s->nops; i++) if (s->ops[i]->type == BC_PRISM) { double v = s->ops[i]->n[1].v; if (!any || v > mx) mx = v; any = 1; }
    double want = py_round(py_min(25.0, (mx - H) * (fin ? e->scale : 1.0)), 1);
    if (has_plan && (e->has_oval || plan->fin / plan->sc < e->base_mm - 0.05)) want = 0.0;
    return want;
}

/* ======================= gg3.smooth_isolated ======================= */
/* gg.Scene.final_gcuts / gg2.Scene.final_gcuts (as buildcmds.c final_gcuts): the polygons of the cuts appended to out */
static int final_gcut_polys(const BcScene *s, ShpList *out) {
    const BcCfg *c = &s->cfg; GEOSGeometry *feet;
    double gap = c->gg2 ? py_max(c->has_cutgap ? c->cutgap.v : 0.05, 0.35) : (c->base_first ? 0.35 : 0.05);
    if (s->nfeet) {
        GEOSGeometry *u = shp_union_all((const GEOSGeometry *const *)s->feet, s->nfeet);
        if (!u) return fail_geos("final_gcuts");
        feet = shp_buffer_q(u, gap, 16); shp_free(u);
    } else feet = shp_polygon(NULL, 0);
    if (!feet) return fail_geos("final_gcuts");
    for (int i = 0; i < s->ngcuts; i++) {
        GEOSGeometry *u = shp_union_all((const GEOSGeometry *const *)s->gcuts[i].polys, s->gcuts[i].np), *g;
        ShpList parts = { 0 };
        if (!u) goto gfail;
        g = shp_difference(u, feet); shp_free(u);
        if (!g) goto gfail;
        shp_polys_of(g, &parts); shp_free(g);
        for (int j = 0; j < parts.n; j++) {
            GEOSGeometry *p = parts.g[j], *b;
            if (!(shp_area(p) > 0.1)) continue;
            b = shp_buffer_q(p, -0.1, 16);
            if (!b) { shp_list_free(&parts); goto gfail; }
            int em = shp_is_empty(b); shp_free(b);
            if (em) continue;
            GEOSGeometry *q = shp_simplify(p, 0.01, 1);
            if (!q) { shp_list_free(&parts); goto gfail; }
            shp_list_push(out, q);
        }
        shp_list_free(&parts);
    }
    shp_free(feet);
    return 0;
gfail:
    shp_free(feet);
    return fail_geos("final_gcuts");
}
int mg_smooth_isolated(BcScene *s, double tol, double touch_tol) {
    s->cfg.sketch_tol = 0;
    if (!tol) return 0;
    int nitems = 0, rc = 0;
    for (int i = 0; i < s->nops; i++) nitems += s->ops[i]->np;
    ShpList others = { 0 };
    if (final_gcut_polys(s, &others) < 0) { shp_list_free(&others); return MG_ERR; }
    for (int i = 0; i < s->ntcuts; i++) for (int j = 0; j < s->tcuts[i].np; j++) shp_list_push(&others, shp_clone(s->tcuts[i].polys[j]));
    int ng = nitems + others.n;
    const GEOSGeometry **geoms = (const GEOSGeometry **)calloc((size_t)ng + 1, sizeof(void *));
    int *owner = (int *)calloc((size_t)ng + 1, sizeof(int)), *it_j = (int *)calloc((size_t)nitems + 1, sizeof(int)), m = 0;
    for (int i = 0; i < s->nops; i++) for (int j = 0; j < s->ops[i]->np; j++) { geoms[m] = s->ops[i]->polys[j]; owner[m] = i; it_j[m] = j; m++; }
    for (int k = 0; k < others.n; k++) { geoms[m] = others.g[k]; owner[m] = -1; m++; }
    ShpTree *tree = shp_tree_new(geoms, ng, 10); double gap = 2 * tol + 0.01;
    /* new = {i: list(op[1])}: the smoothed polygons per op (NULL = the old one) */
    GEOSGeometry **nw = (GEOSGeometry **)calloc((size_t)nitems + 1, sizeof(void *));
    for (int idx = 0; idx < nitems && rc == 0; idx++) {
        const GEOSGeometry *p = geoms[idx]; int i = owner[idx], touching = 0;
        GEOSGeometry *pb = shp_buffer_q(p, gap, 16), *bd = NULL; ShpIdx nb = { 0 };
        if (!pb) { rc = fail_geos("smooth_isolated"); break; }
        if (shp_tree_query(tree, pb, SHP_PRED_NONE, &nb) < 0) { shp_free(pb); rc = fail_geos("smooth_isolated"); break; }
        shp_free(pb);
        for (int q = 0; q < nb.n && !touching; q++) {
            int k = nb.i[q];
            if (k == idx) continue;
            if (!bd) { bd = shp_boundary(p); if (!bd) { rc = fail_geos("smooth_isolated"); break; } }
            GEOSGeometry *kb = shp_boundary(geoms[k]);
            if (!kb) { rc = fail_geos("smooth_isolated"); break; }
            double d = shp_distance(bd, kb); shp_free(kb);
            if (!(d < gap)) continue;
            if (owner[k] != i) { touching = 1; break; }
            int r = shp_intersects(geoms[k], p);
            if (r < 0) { rc = fail_geos("smooth_isolated"); break; }
            if (r) touching = 1;
        }
        shp_free(bd); shp_idx_free(&nb);
        if (rc) break;
        GEOSGeometry *q = shp_simplify(p, touching ? touch_tol : tol, 1);
        if (!q) { rc = fail_geos("smooth_isolated"); break; }
        if (shp_type(q) == GEOS_POLYGON && !shp_is_empty(q)) nw[idx] = q; else shp_free(q);
    }
    shp_tree_free(tree);
    if (rc == 0) {          /* s.ops = [(op[0], new[i]) + tuple(op[2:])]: new op objects */
        int base = 0;
        for (int i = 0; i < s->nops; i++) {
            BcOp *o = s->ops[i]; GEOSGeometry **ps = (GEOSGeometry **)calloc((size_t)o->np + 1, sizeof(void *));
            for (int j = 0; j < o->np; j++) { ps[j] = nw[base + j] ? nw[base + j] : shp_clone(o->polys[j]); nw[base + j] = NULL; }
            BcOp *n = bc_op_new(o->type, o->kind, o->n, ps, o->np);
            base += o->np; bc_op_unref(o); s->ops[i] = n;
        }
    }
    for (int k = 0; k < nitems; k++) shp_free(nw[k]);
    free(nw); free((void *)geoms); free(owner); free(it_j); shp_list_free(&others);
    return rc;
}

/* ======================= json.dumps (sort_keys) ======================= */
MgJ mgj_null(void) { MgJ v; memset(&v, 0, sizeof v); v.t = MGJ_NULL; return v; }
MgJ mgj_bool(int b) { MgJ v = mgj_null(); v.t = b ? MGJ_TRUE : MGJ_FALSE; return v; }
MgJ mgj_int(int64_t i) { MgJ v = mgj_null(); v.t = MGJ_INT; v.i = i; return v; }
MgJ mgj_float(double d) { MgJ v = mgj_null(); v.t = MGJ_FLOAT; v.d = d; return v; }
MgJ mgj_str(const char *s) { MgJ v = mgj_null(); v.t = MGJ_STR; v.s = strdup(s); return v; }
MgJ mgj_list(void) { MgJ v = mgj_null(); v.t = MGJ_LIST; return v; }
MgJ mgj_dict(void) { MgJ v = mgj_null(); v.t = MGJ_DICT; return v; }
static void mgj_grow(MgJ *l) {
    if (l->n < l->cap) return;
    l->cap = l->cap ? 2 * l->cap : 8; l->v = (MgJ *)realloc(l->v, sizeof(MgJ) * (size_t)l->cap);
    if (l->t == MGJ_DICT) l->k = (char **)realloc(l->k, sizeof(char *) * (size_t)l->cap);
}
void mgj_push(MgJ *l, MgJ v) { mgj_grow(l); l->v[l->n++] = v; }
void mgj_set(MgJ *d, const char *key, MgJ v) {
    for (int i = 0; i < d->n; i++) if (!strcmp(d->k[i], key)) { mgj_free(&d->v[i]); d->v[i] = v; return; }
    mgj_grow(d); d->k[d->n] = strdup(key); d->v[d->n++] = v;
}
void mgj_free(MgJ *v) {
    if (v->t == MGJ_STR) free(v->s);
    if (v->t == MGJ_LIST || v->t == MGJ_DICT) {
        for (int i = 0; i < v->n; i++) { mgj_free(&v->v[i]); if (v->t == MGJ_DICT) free(v->k[i]); }
        free(v->v); free(v->k);
    }
    *v = mgj_null();
}
static MgJ mgj_copy(const MgJ *a) {
    MgJ v = *a;
    if (a->t == MGJ_STR) v.s = strdup(a->s);
    if (a->t == MGJ_LIST || a->t == MGJ_DICT) {
        v.cap = a->n; v.v = a->n ? (MgJ *)malloc(sizeof(MgJ) * (size_t)a->n) : NULL; v.k = NULL;
        if (a->t == MGJ_DICT && a->n) v.k = (char **)malloc(sizeof(char *) * (size_t)a->n);
        for (int i = 0; i < a->n; i++) { v.v[i] = mgj_copy(&a->v[i]); if (a->t == MGJ_DICT) v.k[i] = strdup(a->k[i]); }
    }
    return v;
}
static uint32_t utf8_next(const unsigned char **p) {
    const unsigned char *s = *p; uint32_t c = *s++;
    if (c >= 0xf0 && s[0] && s[1] && s[2]) { c = ((c & 7) << 18) | ((uint32_t)(s[0] & 63) << 12) | ((uint32_t)(s[1] & 63) << 6) | (s[2] & 63); s += 3; }
    else if (c >= 0xe0 && s[0] && s[1]) { c = ((c & 15) << 12) | ((uint32_t)(s[0] & 63) << 6) | (s[1] & 63); s += 2; }
    else if (c >= 0xc0 && s[0]) { c = ((c & 31) << 6) | (s[0] & 63); s += 1; }
    *p = s; return c;
}
static void json_str(MgBuf *o, const char *s) {          /* json's py_encode_basestring_ascii */
    char t[16]; const unsigned char *p = (const unsigned char *)s;
    buf_u8(o, '"');
    while (*p) {
        uint32_t c = utf8_next(&p);
        if (c == '"') buf_s(o, "\\\"");
        else if (c == '\\') buf_s(o, "\\\\");
        else if (c == '\n') buf_s(o, "\\n");
        else if (c == '\r') buf_s(o, "\\r");
        else if (c == '\t') buf_s(o, "\\t");
        else if (c == '\b') buf_s(o, "\\b");
        else if (c == '\f') buf_s(o, "\\f");
        else if (c < 0x20 || c > 0x7e) {
            if (c >= 0x10000) { uint32_t v = c - 0x10000; snprintf(t, sizeof t, "\\u%04x\\u%04x", 0xd800 | (v >> 10), 0xdc00 | (v & 0x3ff)); }
            else snprintf(t, sizeof t, "\\u%04x", c);
            buf_s(o, t);
        } else buf_u8(o, (unsigned char)c);
    }
    buf_u8(o, '"');
}
/* Python's sorted() of str keys: by code points (UTF-8 byte order is the same order) */
static int key_cmp(const char *a, const char *b) { return strcmp(a, b); }
void mgj_dump(const MgJ *v, int sort, MgBuf *o) {
    char t[64];
    switch (v->t) {
    case MGJ_NULL: buf_s(o, "null"); break;
    case MGJ_FALSE: buf_s(o, "false"); break;
    case MGJ_TRUE: buf_s(o, "true"); break;
    case MGJ_INT: snprintf(t, sizeof t, "%lld", (long long)v->i); buf_s(o, t); break;
    case MGJ_FLOAT:
        if (isnan(v->d)) buf_s(o, "NaN");
        else if (isinf(v->d)) buf_s(o, v->d > 0 ? "Infinity" : "-Infinity");
        else buf_float(o, v->d);
        break;
    case MGJ_STR: json_str(o, v->s); break;
    case MGJ_LIST:
        buf_s(o, "[");
        for (int i = 0; i < v->n; i++) { if (i) buf_s(o, ", "); mgj_dump(&v->v[i], sort, o); }
        buf_s(o, "]"); break;
    case MGJ_DICT: {
        int *ord = (int *)malloc(sizeof(int) * (size_t)(v->n + 1));
        for (int i = 0; i < v->n; i++) ord[i] = i;
        if (sort) for (int i = 1; i < v->n; i++) { int x = ord[i], j = i - 1; while (j >= 0 && key_cmp(v->k[ord[j]], v->k[x]) > 0) { ord[j + 1] = ord[j]; j--; } ord[j + 1] = x; }
        buf_s(o, "{");
        for (int i = 0; i < v->n; i++) { if (i) buf_s(o, ", "); json_str(o, v->k[ord[i]]); buf_s(o, ": "); mgj_dump(&v->v[ord[i]], sort, o); }
        buf_s(o, "}"); free(ord); break;
    }
    }
}
void mg_scene_report(const char *theme, const MgJ *plan, const char *plan_line, const char *const *scene_log, int nlog, MgJ *out) {
    *out = mgj_dict();
    if (!plan || (plan->t == MGJ_DICT && plan->n == 0) || plan->t == MGJ_NULL) return;          /* not getattr(s, "plan", None) */
    MgJ p = mgj_dict(); MgBuf b = { 0 }; char h[33];
    for (int i = 0; i < plan->n; i++) if (plan->k[i][0] != '_') mgj_set(&p, plan->k[i], mgj_copy(&plan->v[i]));
    mgj_dump(&p, 1, &b); mg_md5_hex(b.b, b.n, h); h[10] = 0; free(b.b);
    mgj_set(out, "theme", mgj_str(theme ? theme : ""));
    mgj_set(out, "plan", p);
    mgj_set(out, "plan_line", mgj_str(plan_line ? plan_line : ""));
    mgj_set(out, "plan_hash", mgj_str(h));
    MgJ lg = mgj_list();
    for (int i = 0; i < nlog && i < 8; i++) mgj_push(&lg, mgj_str(scene_log[i]));
    mgj_set(out, "scene_log", lg);
}

/* ======================= kitlog ======================= */
static MgStrs LOGREC;                     /* this process's records: their one-line forms (kitlog._REC) */
static char LOGBASE[256], LOGDIR[MAX_PATH];
void mg_log_dir(const char *dir) { snprintf(LOGDIR, sizeof LOGDIR, "%s", dir ? dir : ""); }
void mg_log_base(const char *name) { snprintf(LOGBASE, sizeof LOGBASE, "%s", name ? name : ""); }
int mg_log_mark(void) { return LOGREC.n; }
static void cut_chars(char *s, int nchars) {          /* str[:n] on code points */
    unsigned char *p = (unsigned char *)s; int k = 0;
    while (*p && k < nchars) { p++; while ((*p & 0xc0) == 0x80) p++; k++; }
    *p = 0;
}
static void log_write(const char *where, const char *kind, const char *text, const char *at, const char *tb) {
    char who[300], tm[32], path[MAX_PATH * 2]; time_t now = time(NULL); struct tm lt;
    if (LOGBASE[0]) snprintf(who, sizeof who, "%s", LOGBASE);
    else { const char *salt = getenv("SALT"); if (salt && salt[0]) snprintf(who, sizeof who, "seed %s", salt); else snprintf(who, sizeof who, "pid %lu", (unsigned long)GetCurrentProcessId()); }
    lt = *localtime(&now); strftime(tm, sizeof tm, "%Y-%m-%d %H:%M:%S", &lt);
    const char *d = getenv("KIT_ERRORS_DIR"); if (!d || !d[0]) d = LOGDIR[0] ? LOGDIR : "out";
    CreateDirectoryA(d, NULL);
    snprintf(path, sizeof path, "%s\\kit_errors.log", d);
    FILE *f = fopen(path, "ab");
    if (f) {
        fprintf(f, "%s  %s  [%s]  %s: %s", tm, who, where, kind, text); if (at && at[0]) fprintf(f, "  (%s)", at); fprintf(f, "\n");
        if (tb && tb[0]) {          /* the traceback block: "".join("    " + l + "\n" for l in tb.rstrip().splitlines()) */
            size_t n = strlen(tb); while (n && (tb[n - 1] == ' ' || tb[n - 1] == '\n' || tb[n - 1] == '\r' || tb[n - 1] == '\t')) n--;
            size_t s = 0;
            while (s < n) {
                size_t e = s; while (e < n && tb[e] != '\n' && tb[e] != '\r') e++;
                fprintf(f, "    %.*s\n", (int)(e - s), tb + s);
                if (e < n && tb[e] == '\r' && e + 1 < n && tb[e + 1] == '\n') e++;
                s = e + 1;
            }
        }
        fclose(f);
    }
    if (getenv("KIT_ERRORS_PRINT")) { fprintf(stderr, "! %s  %s  [%s]  %s: %s", tm, who, where, kind, text); if (at && at[0]) fprintf(stderr, "  (%s)", at); fprintf(stderr, "\n"); }
}
const char *mg_log_caught(const char *where, const char *kind, const char *text, const char *at) {
    char *t4 = strdup(text ? text : ""), *t8, line[1200];
    cut_chars(t4, 400); t8 = strdup(t4); cut_chars(t8, 80);
    {          /* the traceback of an exception C has no frames for: format_exception's last line, "Kind: text" */
        size_t n = strlen(kind) + strlen(text ? text : "") + 4; char *tb = (char *)malloc(n);
        snprintf(tb, n, (text && text[0]) ? "%s: %s" : "%s", kind, text ? text : "");
        log_write(where, kind, t4, at, tb); free(tb);
    }
    snprintf(line, sizeof line, "%s: %s %s", where, kind, t8);
    if (at && at[0]) { size_t n = strlen(line); snprintf(line + n, sizeof line - n, " (%s)", at); }
    free(t4); free(t8);
    mg_strs_push(&LOGREC, line);
    return LOGREC.s[LOGREC.n - 1];
}
const char *mg_log_note(const char *where, const char *text) {
    char *t4 = strdup(text ? text : ""), *line; size_t n;
    cut_chars(t4, 400); log_write(where, "note", t4, "", NULL);
    n = strlen(where) + strlen(text ? text : "") + 4; line = (char *)malloc(n);
    snprintf(line, n, "%s: %s", where, text ? text : "");
    mg_strs_push(&LOGREC, line); free(line); free(t4);
    return LOGREC.s[LOGREC.n - 1];
}
void mg_log_since(int mark, MgStrs *out) { for (int i = mark; i < LOGREC.n; i++) mg_strs_push(out, LOGREC.s[i]); }

/* ======================= layout_bridge.layout_build, after the layout ======================= */
int mg_build_input(BcScene *s, const char *style, const MgEnv *e, const GEOSGeometry *const *pads, int npads, double tol, double touch_tol, MgBuilt *out) {
    BcGroups groups = { 0 }, scaled = { 0 }; const BcGroups *use = &groups; MgBase base; MgSteps steps = { 0 }; GEOSGeometry *stand = NULL;
    int has_base = 0, rc = MG_ERR;
    memset(out, 0, sizeof *out); memset(&base, 0, sizeof base);
    if (mg_sig(s, style, e, out->sig, sizeof out->sig) < 0) return MG_ERR;
    if (mg_smooth_isolated(s, tol, touch_tol) < 0) return MG_ERR;
    if (bc_build_cmds(s, &groups) < 0) { fail("build_cmds raised"); goto done; }
    out->steps = groups.n;
    out->has_plan = mg_scale_plan(e, pads, npads, &out->plan);
    if (out->has_plan && !(e->has_oval && fabs(out->plan.sc - 1) < 1e-6 && !out->plan.tx && !out->plan.ty)) {
        if (mg_rescale(&groups, out->plan.sc, out->plan.fin, 25.0, 2.9, out->plan.tx, out->plan.ty, NULL, &scaled) < 0) goto done;
        use = &scaled;
    }
    if (mg_field_ops(e, use, &base, &has_base, &steps, &out->skipped) < 0) goto done;
    if (!has_base) { fail("RuntimeError: no base in the command groups"); goto done; }
    if (out->has_plan && out->plan.pads) {
        stand = shp_buffer_q(out->plan.pads, 1.2, 6);
        if (!stand) mg_strs_push(&out->skipped, "standing place: GEOSException");
    }
    if (mg_pack_ops(e, &base, &steps, stand, base.H, out->plan.sc, &out->bbf) < 0) goto done;
    out->H = base.H; out->R0 = base.R0; out->rim_tan = base.rim_tan; out->nsteps = steps.n;
    out->want_mm = mg_want_mm(s, base.H, e, out->has_plan, &out->plan);
    rc = 0;
done:
    shp_free(stand); mg_steps_free(&steps); mg_base_free(&base); bc_groups_free(&groups); bc_groups_free(&scaled);
    return rc;
}
void mg_built_free(MgBuilt *b) { free(b->bbf.b); mg_strs_free(&b->skipped); mg_plan_free(&b->plan); memset(b, 0, sizeof *b); }

/* ======================= --supports on: the true 3D solids ======================= */
void mg_mesh_free(MgMesh *m) { free(m->v); free(m->f); memset(m, 0, sizeof *m); }
typedef struct { MgMesh *m; int capv, capf; } Mb;
static int mb_v(Mb *b, double x, double y, double z) {
    MgMesh *m = b->m;
    if (m->nv == b->capv) { b->capv = b->capv ? 2 * b->capv : 64; m->v = (double *)realloc(m->v, sizeof(double) * 3 * (size_t)b->capv); }
    m->v[3 * m->nv] = x; m->v[3 * m->nv + 1] = y; m->v[3 * m->nv + 2] = z; return m->nv++;
}
static void mb_f(Mb *b, int a, int c, int d) {
    MgMesh *m = b->m;
    if (m->nt == b->capf) { b->capf = b->capf ? 2 * b->capf : 64; m->f = (int *)realloc(m->f, sizeof(int) * 3 * (size_t)b->capf); }
    m->f[3 * m->nt] = a; m->f[3 * m->nt + 1] = c; m->f[3 * m->nt + 2] = d; m->nt++;
}
/* _extrude's ix dict: (round(x, 5), round(y, 5), z) -> index; keys equal as Python floats are (0.0 == -0.0) */
typedef struct { double k[3]; int i; } Vk;
typedef struct { Vk *s; size_t cap, n; } Vmap;
static uint64_t dbits(double x) { uint64_t u; if (x == 0) x = 0.0; memcpy(&u, &x, 8); return u; }
static size_t vk_slot(const double *k, size_t cap) {
    uint64_t h = dbits(k[0]) * 0x9e3779b97f4a7c15ULL ^ dbits(k[1]) * 0xc2b2ae3d27d4eb4fULL ^ dbits(k[2]) * 0x165667b19e3779f9ULL;
    return (size_t)(h ^ (h >> 31)) & (cap - 1);
}
static int vmap_get(Vmap *m, Mb *b, double x, double y, double z) {
    double k[3] = { py_round(x, 5), py_round(y, 5), z };
    if (2 * (m->n + 1) > m->cap) {
        size_t nc = m->cap ? 2 * m->cap : 256; Vk *ns = (Vk *)malloc(sizeof(Vk) * nc);
        for (size_t i = 0; i < nc; i++) ns[i].i = -1;
        for (size_t i = 0; i < m->cap; i++) if (m->s[i].i >= 0) { size_t s = vk_slot(m->s[i].k, nc); while (ns[s].i >= 0) s = (s + 1) & (nc - 1); ns[s] = m->s[i]; }
        free(m->s); m->s = ns; m->cap = nc;
    }
    size_t s = vk_slot(k, m->cap);
    while (m->s[s].i >= 0) {
        if (m->s[s].k[0] == k[0] && m->s[s].k[1] == k[1] && m->s[s].k[2] == k[2]) return m->s[s].i;
        s = (s + 1) & (m->cap - 1);
    }
    m->s[s].k[0] = k[0]; m->s[s].k[1] = k[1]; m->s[s].k[2] = k[2]; m->s[s].i = mb_v(b, x, y, z); m->n++;
    return m->s[s].i;
}
int mg_extrude3d(const GEOSGeometry *poly, double z0, double z1, MgMesh *out) {
    Mb b = { out, 0, 0 }; Vmap ix = { 0 }; ShpList ps = { 0 }; int rc = 0;
    memset(out, 0, sizeof *out);
    shp_polys_of(poly, &ps);
    for (int i = 0; i < ps.n && !rc; i++) {
        GEOSGeometry *g = shp_orient(ps.g[i], 1.0), *tr;
        if (!g) { rc = fail_geos("_extrude"); break; }
        tr = shp_constrained_delaunay(g);
        if (!tr) { shp_free(g); rc = fail_geos("_extrude"); break; }
        for (int k = 0; k < shp_num_geoms(tr); k++) {
            double xy[8]; const GEOSGeometry *t = shp_geom_n(tr, k);
            if (shp_coords(shp_exterior(t), xy, 4) < 3) continue;
            double ax = xy[0], ay = xy[1], bx = xy[2], by = xy[3], cx = xy[4], cy = xy[5];
            if ((bx - ax) * (cy - ay) - (by - ay) * (cx - ax) < 0) { double tx = bx, ty = by; bx = cx; by = cy; cx = tx; cy = ty; }
            int a1 = vmap_get(&ix, &b, ax, ay, z1), b1 = vmap_get(&ix, &b, bx, by, z1), c1 = vmap_get(&ix, &b, cx, cy, z1); mb_f(&b, a1, b1, c1);
            int a0 = vmap_get(&ix, &b, ax, ay, z0), c0 = vmap_get(&ix, &b, cx, cy, z0), b0 = vmap_get(&ix, &b, bx, by, z0); mb_f(&b, a0, c0, b0);
        }
        shp_free(tr);
        for (int r = -1; r < shp_num_interiors(g); r++) {
            const GEOSGeometry *ring = r < 0 ? shp_exterior(g) : shp_interior(g, r); int n = G.GEOSGetNumCoordinates_r(gctx, ring);
            double *xy = (double *)malloc(sizeof(double) * 2 * (size_t)(n + 1)); n = shp_coords(ring, xy, n + 1);
            for (int k = 0; k + 1 < n; k++) {
                double ax = xy[2 * k], ay = xy[2 * k + 1], bx = xy[2 * k + 2], by = xy[2 * k + 3];
                if (ax == bx && ay == by) continue;
                int p = vmap_get(&ix, &b, ax, ay, z0), q = vmap_get(&ix, &b, bx, by, z0), w = vmap_get(&ix, &b, bx, by, z1); mb_f(&b, p, q, w);
                p = vmap_get(&ix, &b, ax, ay, z0); q = vmap_get(&ix, &b, bx, by, z1); w = vmap_get(&ix, &b, ax, ay, z1); mb_f(&b, p, q, w);
            }
            free(xy);
        }
        shp_free(g);
    }
    shp_list_free(&ps); free(ix.s);
    return rc;
}
int mg_revolve3d(double cx, double cy, const double *prof0, int n0, int seg, double squash, double a, double zb, double sink, MgMesh *out) {
    Mb b = { out, 0, 0 }; int n = n0 + 1; double *prof = (double *)malloc(sizeof(double) * 2 * (size_t)n), ca = py_cos(a), sa = py_sin(a);
    memset(out, 0, sizeof *out);
    prof[0] = prof0[0]; prof[1] = prof0[1] - sink; memcpy(prof + 2, prof0, sizeof(double) * 2 * (size_t)n0);
    for (int i = 0; i < n; i++) {
        double r = prof[2 * i], z = prof[2 * i + 1];
        for (int k = 0; k < seg; k++) {
            double t = 6.283185 * k / seg, u = r * py_cos(t), v = r * squash * py_sin(t);
            mb_v(&b, cx + u * ca - v * sa, cy + u * sa + v * ca, zb + z);
        }
    }
    for (int i = 0; i < n - 1; i++)
        for (int k = 0; k < seg; k++) {
            int p0 = i * seg + k, p1 = i * seg + (k + 1) % seg, q0 = (i + 1) * seg + k, q1 = (i + 1) * seg + (k + 1) % seg;
            mb_f(&b, p0, p1, q1); mb_f(&b, p0, q1, q0);
        }
    int b0 = mb_v(&b, cx, cy, zb + prof[1]), t0 = mb_v(&b, cx, cy, zb + prof[2 * (n - 1) + 1]);
    for (int k = 0; k < seg; k++) { mb_f(&b, b0, (k + 1) % seg, k); mb_f(&b, t0, (n - 1) * seg + k, (n - 1) * seg + (k + 1) % seg); }
    free(prof);
    return 0;
}
/* CPython's sum() of floats (start 0): 0 + the first, then Neumaier's compensated sum (bltinmodule.c builtin_sum_impl) */
static double py_sum(const double *x, int n) {
    if (n == 0) return 0.0;
    double f = 0 + x[0], c = 0.0;
    for (int i = 1; i < n; i++) {
        double t = f + x[i];
        if (fabs(f) >= fabs(x[i])) c += (f - t) + x[i]; else c += (x[i] - t) + f;
        f = t;
    }
    if (c && isfinite(c)) f += c;
    return f;
}
int mg_sweep3d(const double *pts, const double *radii, int n, int seg, double squash, MgMesh *out) {
    Mb b = { out, 0, 0 };
    memset(out, 0, sizeof *out);
    for (int i = 0; i < n; i++) {
        const double *p = pts + 3 * i, *a_ = pts + 3 * (i - 1 > 0 ? i - 1 : 0), *b_ = pts + 3 * (i + 1 < n - 1 ? i + 1 : n - 1); double r = radii[i];
        double T[3] = { b_[0] - a_[0], b_[1] - a_[1], b_[2] - a_[2] };
        double sq[3] = { T[0] * T[0], T[1] * T[1], T[2] * T[2] }, L = sqrt(py_sum(sq, 3)); if (L == 0) L = 1.0;          /* math.sqrt(sum(c * c for c in T)) or 1.0 */
        T[0] = T[0] / L; T[1] = T[1] / L; T[2] = T[2] / L;
        double N[3] = { -T[1], T[0], 0.0 }, Ln = py_hypot(N[0], N[1]);
        if (Ln < 1e-6) { N[0] = 1.0; N[1] = 0.0; N[2] = 0.0; } else { N[0] = N[0] / Ln; N[1] = N[1] / Ln; N[2] = 0.0; }
        double B[3] = { T[1] * N[2] - T[2] * N[1], T[2] * N[0] - T[0] * N[2], T[0] * N[1] - T[1] * N[0] };
        if (B[2] < 0) { B[0] = -B[0]; B[1] = -B[1]; B[2] = -B[2]; }
        for (int k = 0; k < seg; k++) {
            double t = 6.283185 * k / seg, c_ = r * py_cos(t), s_ = r * squash * py_sin(t);
            mb_v(&b, p[0] + c_ * N[0] + s_ * B[0], p[1] + c_ * N[1] + s_ * B[1], p[2] + c_ * N[2] + s_ * B[2]);
        }
    }
    for (int i = 0; i < n - 1; i++)
        for (int k = 0; k < seg; k++) {
            int p0 = i * seg + k, p1 = i * seg + (k + 1) % seg, q0 = (i + 1) * seg + k, q1 = (i + 1) * seg + (k + 1) % seg;
            mb_f(&b, p0, q1, p1); mb_f(&b, p0, q0, q1);
        }
    int b0 = mb_v(&b, pts[0], pts[1], pts[2]), t0 = mb_v(&b, pts[3 * (n - 1)], pts[3 * (n - 1) + 1], pts[3 * (n - 1) + 2]);
    for (int k = 0; k < seg; k++) { mb_f(&b, b0, k, (k + 1) % seg); mb_f(&b, t0, (n - 1) * seg + (k + 1) % seg, (n - 1) * seg + k); }
    return 0;
}
int mg_slab3d(const GEOSGeometry *poly, double z0, double z1, double zb, double zs, double R0, double H, double rim, MgMesh *out) {
    GEOSGeometry *d = shp_point_buffer(0, 0, R0 - H * py_tan(rim * DEG2RAD) - 0.15, 96), *q;
    memset(out, 0, sizeof *out);
    if (!d) return fail_geos("slab");
    q = shp_intersection(poly, d); shp_free(d);
    if (!q) return fail_geos("slab");
    int rc = mg_extrude3d(q, zb + z0 * zs, zb + z1 * zs, out); shp_free(q);
    return rc;
}
/* numpy's pairwise_sum of a contiguous float64 row (numpy/_core/src/umath/loops_utils.h.src) */
static double np_pairwise(const double *a, int64_t n) {
    if (n < 8) { double r = -0.0; for (int64_t i = 0; i < n; i++) r += a[i]; return r; }
    if (n <= 128) {
        double r[8], res; int64_t i;
        for (int k = 0; k < 8; k++) r[k] = a[k];
        for (i = 8; i < n - (n % 8); i += 8) for (int k = 0; k < 8; k++) r[k] += a[i + k];
        res = ((r[0] + r[1]) + (r[2] + r[3])) + ((r[4] + r[5]) + (r[6] + r[7]));
        for (; i < n; i++) res += a[i];
        return res;
    }
    int64_t n2 = n / 2; n2 -= n2 % 8;
    return np_pairwise(a, n2) + np_pairwise(a + n2, n - n2);
}
double mg_volume(const MgMesh *m) {
    double *ig = (double *)malloc(sizeof(double) * (size_t)(m->nt ? m->nt : 1));
    for (int i = 0; i < m->nt; i++) {
        const double *a = m->v + 3 * m->f[3 * i], *b = m->v + 3 * m->f[3 * i + 1], *c = m->v + 3 * m->f[3 * i + 2];
        double u1 = b[1] - a[1], u2 = b[2] - a[2], w1 = c[1] - b[1], w2 = c[2] - b[2];          /* triangles.cross: np.cross(v1 - v0, v2 - v1) */
        ig[i] = (u1 * w2 - u2 * w1) * (a[0] + b[0] + c[0]);
    }
    double v = (0.0 + (m->nt ? np_pairwise(ig, m->nt) : 0.0)) / 6.0;          /* add.reduce: the identity 0.0, then the pairwise sum */
    free(ig);
    return v;
}
static double (*ucrt_hypot)(double, double);
static double np_hypot(double x, double y) {          /* np.hypot: the CRT's hypot */
    if (!ucrt_hypot) { HMODULE h = LoadLibraryA("ucrtbase.dll"); if (h) ucrt_hypot = (double (*)(double, double))(void *)GetProcAddress(h, "_hypot"); if (!ucrt_hypot) ucrt_hypot = hypot; }
    return ucrt_hypot(x, y);
}
int mg_solid_fit(MgMesh *t, int has_oval, double A, double B, double rlim) {
    if (t->nt == 0) return fail("IndexError: too many indices for array");          /* trimesh's volume of no faces raises */
    if (mg_volume(t) < 0) for (int i = 0; i < t->nt; i++) { int x = t->f[3 * i]; t->f[3 * i] = t->f[3 * i + 2]; t->f[3 * i + 2] = x; }          /* invert: faces reversed */
    if (has_oval) {
        double oa = A / 2.0 - 0.97, ob = B / 2.0 - 0.97;
        for (int i = 0; i < t->nv; i++) { double d = np_hypot(t->v[3 * i] / oa, t->v[3 * i + 1] / ob); if (d > 1.0) { t->v[3 * i] /= d; t->v[3 * i + 1] /= d; } }
    } else
        for (int i = 0; i < t->nv; i++) { double d = np_hypot(t->v[3 * i], t->v[3 * i + 1]); if (d > rlim) { double k = rlim / d; t->v[3 * i] *= k; t->v[3 * i + 1] *= k; } }
    return 0;
}
void mg_concat(MgMesh *m, const MgMesh *extra, int n) {
    int nv = m->nv, nt = m->nt;
    for (int i = 0; i < n; i++) { nv += extra[i].nv; nt += extra[i].nt; }
    m->v = (double *)realloc(m->v, sizeof(double) * 3 * (size_t)(nv ? nv : 1)); m->f = (int *)realloc(m->f, sizeof(int) * 3 * (size_t)(nt ? nt : 1));
    for (int i = 0; i < n; i++) {
        memcpy(m->v + 3 * m->nv, extra[i].v, sizeof(double) * 3 * (size_t)extra[i].nv);
        for (int k = 0; k < 3 * extra[i].nt; k++) m->f[3 * m->nt + k] = extra[i].f[k] + m->nv;
        m->nv += extra[i].nv; m->nt += extra[i].nt;
    }
}

/* ======================= with_figure: trimesh's cylinder and icosphere ======================= */
/* trimesh's merge_vertices: keys round(v * 1e8) as int64 (exact row equality: hashable_rows packs only rows within
   +-2^20, else compares the raw rows), unique_rows(keep_order=True): first occurrence among referenced vertices */
typedef struct { int64_t k[3]; int i; } Mk;
void mg_merge_vertices(MgMesh *m) {
    int nv = m->nv; size_t cap = 16; while (cap < (size_t)nv * 2 + 2) cap <<= 1;
    Mk *slot = (Mk *)malloc(sizeof(Mk) * cap); for (size_t i = 0; i < cap; i++) slot[i].i = -1;
    unsigned char *ref = (unsigned char *)calloc((size_t)nv + 1, 1); int *newi = (int *)malloc(sizeof(int) * (size_t)(nv + 1)), nu = 0;
    double *nvv = (double *)malloc(sizeof(double) * 3 * (size_t)(nv + 1));
    for (int i = 0; i < 3 * m->nt; i++) ref[m->f[i]] = 1;
    for (int i = 0; i < nv; i++) {
        if (!ref[i]) { newi[i] = 0; continue; }          /* inverse is 0 for an unreferenced vertex (never used) */
        int64_t k[3]; for (int c = 0; c < 3; c++) k[c] = (int64_t)rint(m->v[3 * i + c] * 100000000.0);
        uint64_t h = (uint64_t)k[0] * 0x9e3779b97f4a7c15ULL ^ (uint64_t)k[1] * 0xc2b2ae3d27d4eb4fULL ^ (uint64_t)k[2] * 0x165667b19e3779f9ULL;
        size_t s = (size_t)(h ^ (h >> 29)) & (cap - 1);
        while (slot[s].i >= 0 && !(slot[s].k[0] == k[0] && slot[s].k[1] == k[1] && slot[s].k[2] == k[2])) s = (s + 1) & (cap - 1);
        if (slot[s].i < 0) { memcpy(slot[s].k, k, sizeof k); slot[s].i = nu; memcpy(nvv + 3 * nu, m->v + 3 * i, 24); nu++; }
        newi[i] = slot[s].i;
    }
    for (int i = 0; i < 3 * m->nt; i++) m->f[i] = newi[m->f[i]];
    free(m->v); m->v = nvv; m->nv = nu;
    free(slot); free(ref); free(newi);
}
/* creation.revolve(linestring, sections=n) for a profile of per points, closed (angle 2 pi), then Trimesh(process=True) */
static int revolve_closed(const double *ls, int per, int sections, MgMesh *out) {
    int ns = sections + 1, slices = ns - 1, nv = slices * per;
    double *th = (double *)malloc(sizeof(double) * (size_t)ns), step = (PI * 2.0 - 0.0) / (ns - 1);
    for (int j = 0; j < ns; j++) th[j] = j * step + 0.0;          /* np.linspace(0, 2 pi, ns) */
    th[ns - 1] = PI * 2.0;
    memset(out, 0, sizeof *out);
    out->v = (double *)malloc(sizeof(double) * 3 * (size_t)nv); out->nv = nv;
    for (int j = 0; j < slices; j++) {          /* vertices[:-per]: the last section (2 pi) dropped */
        double c = py_cos(th[j]), s = py_sin(th[j]);
        for (int i = 0; i < per; i++) { double *v = out->v + 3 * (j * per + i); v[0] = c * ls[2 * i]; v[1] = s * ls[2 * i]; v[2] = ls[2 * i + 1]; }
    }
    /* single = the quads (i, per + i, i + 1), (i + 1, per + i, per + i + 1) of the first slice, kept where their area
       > tol.merge (on the first slice's vertices: a triangle of two equal points has area 0) */
    int single[64][3], nsg = 0;
    for (int i = 0; i < per - 1 && nsg < 62; i++) {
        int t[2][3] = { { i, per + i, i + 1 }, { i + 1, per + i, per + i + 1 } };
        for (int q = 0; q < 2; q++) {
            double p[3][3];
            for (int k = 0; k < 3; k++) {
                int vi = t[q][k], j = vi / per, ii = vi % per; double c = py_cos(th[j]), s = py_sin(th[j]);
                p[k][0] = c * ls[2 * ii]; p[k][1] = s * ls[2 * ii]; p[k][2] = ls[2 * ii + 1];
            }
            double a[3] = { p[1][0] - p[0][0], p[1][1] - p[0][1], p[1][2] - p[0][2] }, b[3] = { p[2][0] - p[1][0], p[2][1] - p[1][1], p[2][2] - p[1][2] };
            double cx = a[1] * b[2] - a[2] * b[1], cy = a[2] * b[0] - a[0] * b[2], cz = a[0] * b[1] - a[1] * b[0];
            double area = sqrt(cx * cx + cy * cy + cz * cz) / 2.0;
            if (area > 1e-8) { memcpy(single[nsg], t[q], sizeof t[q]); nsg++; }
        }
    }
    free(th);
    out->nt = slices * nsg; out->f = (int *)malloc(sizeof(int) * 3 * (size_t)(out->nt ? out->nt : 1));
    for (int s = 0; s < slices; s++) for (int q = 0; q < nsg; q++) for (int k = 0; k < 3; k++) out->f[3 * (s * nsg + q) + k] = (single[q][k] + s * per) % nv;
    mg_merge_vertices(out);          /* Trimesh(vertices, faces) processes: merge_vertices */
    return 0;
}
int mg_cylinder(double radius, double height, int sections, MgMesh *out) {
    double half = fabs(height) / 2.0, ls[8] = { 0, -half, radius, -half, radius, half, 0, half };
    return revolve_closed(ls, 4, sections, out);
}
static const char *const ICO_V[36] = {
    "-0x1.0d2ca0da1530dp-1", "0x1.b38880b4603e5p-1", "0x0.0p+0", "0x1.0d2ca0da1530dp-1", "0x1.b38880b4603e5p-1", "0x0.0p+0",
    "-0x1.0d2ca0da1530dp-1", "-0x1.b38880b4603e5p-1", "0x0.0p+0", "0x1.0d2ca0da1530dp-1", "-0x1.b38880b4603e5p-1", "0x0.0p+0",
    "0x0.0p+0", "-0x1.0d2ca0da1530dp-1", "0x1.b38880b4603e5p-1", "0x0.0p+0", "0x1.0d2ca0da1530dp-1", "0x1.b38880b4603e5p-1",
    "0x0.0p+0", "-0x1.0d2ca0da1530dp-1", "-0x1.b38880b4603e5p-1", "0x0.0p+0", "0x1.0d2ca0da1530dp-1", "-0x1.b38880b4603e5p-1",
    "0x1.b38880b4603e5p-1", "0x0.0p+0", "-0x1.0d2ca0da1530dp-1", "0x1.b38880b4603e5p-1", "0x0.0p+0", "0x1.0d2ca0da1530dp-1",
    "-0x1.b38880b4603e5p-1", "0x0.0p+0", "-0x1.0d2ca0da1530dp-1", "-0x1.b38880b4603e5p-1", "0x0.0p+0", "0x1.0d2ca0da1530dp-1" };
static const int ICO_F[60] = { 0, 11, 5, 0, 5, 1, 0, 1, 7, 0, 7, 10, 0, 10, 11, 1, 5, 9, 5, 11, 4, 11, 10, 2, 10, 7, 6, 7, 1, 8, 3, 9, 4, 3, 4, 2,
    3, 2, 6, 3, 6, 8, 3, 8, 9, 4, 9, 5, 2, 4, 11, 6, 2, 10, 8, 6, 7, 9, 8, 1 };          /* trimesh's resources: icosahedron */
typedef struct { int64_t key; int idx; } Ek;
static int ek_cmp(const void *a, const void *b) { const Ek *x = (const Ek *)a, *y = (const Ek *)b; return x->key < y->key ? -1 : x->key > y->key ? 1 : x->idx - y->idx; }
/* remesh.subdivide(vertices, faces): every face split in four at its edge midpoints */
static void subdivide(MgMesh *m) {
    int ne = 3 * m->nt; Ek *e = (Ek *)malloc(sizeof(Ek) * (size_t)ne); int (*ed)[2] = malloc(sizeof(int) * 2 * (size_t)ne), *inv = (int *)malloc(sizeof(int) * (size_t)ne);
    for (int i = 0; i < m->nt; i++) for (int k = 0; k < 3; k++) {          /* faces_to_edges: (a, b), (b, c), (c, a), sorted per row */
        int a = m->f[3 * i + k], b = m->f[3 * i + (k + 1) % 3], lo = a < b ? a : b, hi = a < b ? b : a, j = 3 * i + k;
        ed[j][0] = lo; ed[j][1] = hi; e[j].key = ((int64_t)hi << 32) | (int64_t)lo; e[j].idx = j;          /* hashable_rows: column 1 in the high bits */
    }
    qsort(e, (size_t)ne, sizeof(Ek), ek_cmp);          /* np.unique: sorted keys; return_index = first occurrence */
    int nu = 0; int *first = (int *)malloc(sizeof(int) * (size_t)ne);
    for (int q = 0; q < ne; q++) { if (q == 0 || e[q].key != e[q - 1].key) first[nu++] = e[q].idx; inv[e[q].idx] = nu - 1; }
    int nv0 = m->nv; m->v = (double *)realloc(m->v, sizeof(double) * 3 * (size_t)(nv0 + nu));
    for (int u = 0; u < nu; u++) for (int c = 0; c < 3; c++) {          /* vertices[edges[unique]].mean(axis=1): (0 + a + b) / 2 */
        double a = m->v[3 * ed[first[u]][0] + c], b = m->v[3 * ed[first[u]][1] + c];
        m->v[3 * (nv0 + u) + c] = ((0.0 + a) + b) / 2.0;
    }
    m->nv = nv0 + nu;
    int *nf = (int *)malloc(sizeof(int) * 12 * (size_t)m->nt);
    for (int i = 0; i < m->nt; i++) {
        int f0 = m->f[3 * i], f1 = m->f[3 * i + 1], f2 = m->f[3 * i + 2], m0 = inv[3 * i] + nv0, m1 = inv[3 * i + 1] + nv0, m2 = inv[3 * i + 2] + nv0;
        int t[12] = { f0, m0, m2, m0, f1, m1, m2, m1, f2, m0, m1, m2 }; memcpy(nf + 12 * i, t, sizeof t);
    }
    free(m->f); m->f = nf; m->nt *= 4;
    free(e); free(ed); free(inv); free(first);
}
static void ico_radius(MgMesh *m, double radius) {          /* vertices += unit * (radius - |v|) */
    for (int i = 0; i < m->nv; i++) {
        double *v = m->v + 3 * i, x2 = v[0] * v[0], y2 = v[1] * v[1], z2 = v[2] * v[2], sc = sqrt((x2 + y2) + z2);
        for (int c = 0; c < 3; c++) { double u = v[c] / sc; v[c] = v[c] + u * (radius - sc); }
    }
}
int mg_icosphere(int subdivisions, double radius, MgMesh *out) {
    memset(out, 0, sizeof *out);
    out->nv = 12; out->nt = 20; out->v = (double *)malloc(sizeof(double) * 36); out->f = (int *)malloc(sizeof(int) * 60);
    for (int i = 0; i < 36; i++) out->v[i] = strtod(ICO_V[i], NULL);
    memcpy(out->f, ICO_F, sizeof ICO_F);
    for (int s = 0; s < subdivisions; s++) { subdivide(out); ico_radius(out, radius); }
    if (subdivisions <= 0) ico_radius(out, radius);
    return 0;
}
static void translate(MgMesh *m, double x, double y, double z) {          /* apply_translation (a pure translation: v + t) */
    for (int i = 0; i < m->nv; i++) { m->v[3 * i] += x; m->v[3 * i + 1] += y; m->v[3 * i + 2] += z; }
}
/* the model's space on the picture (2026-11-20): a low wall w wide from z0 to z1 along a closed outline of n points */
int mg_outline(const double *xy, int n, double w, double z0, double z1, int dash, MgMesh *out) {
    memset(out, 0, sizeof *out); if (n < 2) return -1;
    out->v = (double *)malloc(sizeof(double) * 24 * (size_t)n); out->f = (int *)malloc(sizeof(int) * 36 * (size_t)n); if (!out->v || !out->f) return -1;
    for (int i = 0; i < n; i++) {
        double ax = xy[2 * i], ay = xy[2 * i + 1], bx = xy[2 * ((i + 1) % n)], by = xy[2 * ((i + 1) % n) + 1], ex = bx - ax, ey = by - ay, L = sqrt(ex * ex + ey * ey), nx, ny;
        if (L < 1e-9 || (dash > 0 && (i / dash) % 2)) continue;          /* dashed: every other run of dash segments */
        nx = -ey / L * w / 2; ny = ex / L * w / 2; ax -= ex / L * w / 2; ay -= ey / L * w / 2; bx += ex / L * w / 2; by += ey / L * w / 2;          /* a little past each end: closed corners */
        double c[4][2] = { { ax - nx, ay - ny }, { bx - nx, by - ny }, { bx + nx, by + ny }, { ax + nx, ay + ny } }; int b0 = out->nv;          /* anticlockwise from above */
        static const int F[12][3] = { { 0, 2, 1 }, { 0, 3, 2 }, { 4, 5, 6 }, { 4, 6, 7 }, { 0, 1, 5 }, { 0, 5, 4 }, { 1, 2, 6 }, { 1, 6, 5 }, { 2, 3, 7 }, { 2, 7, 6 }, { 3, 0, 4 }, { 3, 4, 7 } };
        for (int k = 0; k < 8; k++) { double *p = out->v + 3 * (size_t)(out->nv++); p[0] = c[k % 4][0]; p[1] = c[k % 4][1]; p[2] = k < 4 ? z0 : z1; }
        for (int k = 0; k < 12; k++) { int *t = out->f + 3 * (size_t)(out->nt++); t[0] = b0 + F[k][0]; t[1] = b0 + F[k][1]; t[2] = b0 + F[k][2]; }
    }
    return 0;
}
int mg_figure(double sc, double r, MgMesh *out) {
    double h = 32.0 * sc / 3.0, w = py_max(3.0, h * 0.28); MgMesh ico;
    mg_cylinder(w / 2, h * 0.82, 20, out); translate(out, 0, 0, h * 0.41);
    mg_icosphere(2, w * 0.45, &ico); translate(&ico, 0, 0, h * 0.82 + w * 0.4);
    mg_concat(out, &ico, 1); mg_mesh_free(&ico);
    double d = r * 0.71 + w + 2; translate(out, d, -d, 0);
    return 0;
}

/* ======================= PIL 13: thumbnail with Image.reduce (libImaging Reduce.c, Resample.c) ======================= */
#define PB (32 - 8 - 2)                   /* Resample.c PRECISION_BITS */
#define MULDIV255(a, b, tmp) (tmp = (a) * (b) + 128, ((((tmp) >> 8) + (tmp)) >> 8))          /* ImagingUtils.h */
static UINT32 div_u32(int divider) {          /* Reduce.c division_UINT32(divider, 8): float arithmetic */
    UINT32 max_dividend = (UINT32)(1 << 8) * (UINT32)divider; float max_int = (float)((1 << 30) * 4.0);
    return (UINT32)(max_int / (float)max_dividend);
}
/* one output pixel of ImagingReduce: the average over [x0, x1) x [y0, y1) of every band. The special cases of
   Reduce.c (1x2, 2x1, 2x2, 4x4) shift by log2(n) where the rest multiply by division_UINT32(n): (s + n / 2) >> k equals
   ((s + n / 2) * (2^24 / n)) >> 24 for n a power of two, so one formula per case is used as Reduce.c has it. */
static void reduce_px(const unsigned char *pix, int w, int x0, int x1, int y0, int y1, int shift, unsigned char *o) {
    int n = (x1 - x0) * (y1 - y0); UINT32 amend = (UINT32)(n / 2), mult = div_u32(n);
    for (int c = 0; c < 4; c++) {
        UINT32 ss = 0;
        for (int y = y0; y < y1; y++) for (int x = x0; x < x1; x++) ss += pix[4 * ((size_t)y * w + x) + c];
        o[c] = (unsigned char)(shift >= 0 ? (ss + amend) >> shift : ((ss + amend) * mult) >> 24);
    }
}
unsigned char *mg_reduce4(const unsigned char *pix, int w, int h, int fx, int fy, const int box0[4], int *ow, int *oh) {
    int box[4] = { box0[0], box0[1], box0[2] - box0[0], box0[3] - box0[1] };          /* (left, top, width, height) */
    int W = (box[2] - 1) / fx + 1, H = (box[3] - 1) / fy + 1; unsigned char *o = (unsigned char *)calloc((size_t)W * H * 4, 1);
    (void)h;
    int shift = -1;          /* the special cases that shift: 1x2, 2x1 (>> 1), 2x2 (>> 2), 4x4 (>> 4) */
    if ((fx == 1 && fy == 2) || (fx == 2 && fy == 1)) shift = 1; else if (fx == 2 && fy == 2) shift = 2; else if (fx == 4 && fy == 4) shift = 4;
    for (int y = 0; y < box[3] / fy; y++) for (int x = 0; x < box[2] / fx; x++) {
        int xx = box[0] + x * fx, yy = box[1] + y * fy;
        reduce_px(pix, w, xx, xx + fx, yy, yy + fy, shift, o + 4 * ((size_t)y * W + x));
    }
    /* ImagingReduceCorners: the last column, the last row, the corner (always the multiplier form) */
    if (box[2] % fx) { int x = box[2] / fx; for (int y = 0; y < box[3] / fy; y++) reduce_px(pix, w, box[0] + x * fx, box[0] + box[2], box[1] + y * fy, box[1] + y * fy + fy, -1, o + 4 * ((size_t)y * W + x)); }
    if (box[3] % fy) { int y = box[3] / fy; for (int x = 0; x < box[2] / fx; x++) reduce_px(pix, w, box[0] + x * fx, box[0] + x * fx + fx, box[1] + y * fy, box[1] + box[3], -1, o + 4 * ((size_t)y * W + x)); }
    if (box[2] % fx && box[3] % fy) { int x = box[2] / fx, y = box[3] / fy; reduce_px(pix, w, box[0] + x * fx, box[0] + box[2], box[1] + y * fy, box[1] + box[3], -1, o + 4 * ((size_t)y * W + x)); }
    *ow = W; *oh = H;
    return o;
}
static double rs_sinc(double x) { if (x == 0.0) return 1.0; x = x * 3.14159265358979323846; return py_sin(x) / x; }
static double rs_lanczos(double x) { if (-3.0 <= x && x < 3.0) return rs_sinc(x) * rs_sinc(x / 3); return 0.0; }
static unsigned char rs_clip8(int in) { int v = in >> PB; return (unsigned char)(v < 0 ? 0 : v > 255 ? 255 : v); }
static int rs_coeffs(int inSize, float in0, float in1, int outSize, int **boundsp, double **kkp) {          /* precompute_coeffs */
    double support, scale, filterscale; int ksize, xmin, xmax; int *bounds; double *kk, *k;
    filterscale = scale = (double)(in1 - in0) / outSize;
    if (filterscale < 1.0) filterscale = 1.0;
    support = 3.0 * filterscale;
    ksize = (int)ceil(support) * 2 + 1;
    kk = (double *)malloc((size_t)outSize * ksize * sizeof(double)); bounds = (int *)malloc((size_t)outSize * 2 * sizeof(int));
    double inv_filterscale = 1.0 / filterscale;
    for (int xx = 0; xx < outSize; xx++) {
        double center = in0 + (xx + 0.5) * scale, ww = 0.0; int x;
        xmin = (int)(center - support + 0.5); if (xmin < 0) xmin = 0;
        xmax = (int)(center + support + 0.5); if (xmax > inSize) xmax = inSize;
        xmax -= xmin;
        k = &kk[xx * ksize];
        for (x = 0; x < xmax; x++) { double wgt = rs_lanczos((x + xmin - center + 0.5) * inv_filterscale); k[x] = wgt; ww += wgt; }
        if (ww != 0.0) for (x = 0; x < xmax; x++) k[x] /= ww;
        for (; x < ksize; x++) k[x] = 0;
        bounds[xx * 2] = xmin; bounds[xx * 2 + 1] = xmax;
    }
    *boundsp = bounds; *kkp = kk;
    return ksize;
}
static void rs_norm(int outSize, int ksize, double *prekk) {          /* normalize_coeffs_8bpc */
    int32_t *kk = (int32_t *)prekk;
    for (int x = 0; x < outSize * ksize; x++) kk[x] = prekk[x] < 0 ? (int)(-0.5 + prekk[x] * (1 << PB)) : (int)(0.5 + prekk[x] * (1 << PB));
}
static void rs_h(unsigned char *out, int ow, int oh, const unsigned char *in, int iw, int offset, int ksize, int *bounds, double *prekk) {
    int32_t *kk = (int32_t *)prekk; rs_norm(ow, ksize, prekk);
    for (int yy = 0; yy < oh; yy++) {
        const unsigned char *li = in + 4 * (size_t)iw * (yy + offset); unsigned char *lo = out + 4 * (size_t)ow * yy;
        for (int xx = 0; xx < ow; xx++) {
            int xmin = bounds[xx * 2], xmax = bounds[xx * 2 + 1]; int32_t *k = &kk[xx * ksize]; int ss[4];
            for (int c = 0; c < 4; c++) ss[c] = 1 << (PB - 1);
            for (int x = 0; x < xmax; x++) for (int c = 0; c < 4; c++) ss[c] += li[(x + xmin) * 4 + c] * k[x];
            for (int c = 0; c < 4; c++) lo[xx * 4 + c] = rs_clip8(ss[c]);
        }
    }
}
static void rs_v(unsigned char *out, int ow, int oh, const unsigned char *in, int iw, int ksize, int *bounds, double *prekk) {
    int32_t *kk = (int32_t *)prekk; rs_norm(oh, ksize, prekk);
    for (int yy = 0; yy < oh; yy++) {
        unsigned char *lo = out + 4 * (size_t)ow * yy; int32_t *k = &kk[yy * ksize]; int ymin = bounds[yy * 2], ymax = bounds[yy * 2 + 1];
        for (int xx = 0; xx < ow; xx++) {
            int ss[4]; for (int c = 0; c < 4; c++) ss[c] = 1 << (PB - 1);
            for (int y = 0; y < ymax; y++) { const unsigned char *li = in + 4 * (size_t)iw * (y + ymin); for (int c = 0; c < 4; c++) ss[c] += li[xx * 4 + c] * k[y]; }
            for (int c = 0; c < 4; c++) lo[xx * 4 + c] = rs_clip8(ss[c]);
        }
    }
}
unsigned char *mg_resample4(const unsigned char *pix, int w, int h, int xsize, int ysize, const float box[4]) {
    /* _imaging.c _resize: integer box of the asked size = a crop */
    if (box[0] - (int)box[0] == 0 && box[2] - box[0] == xsize && box[1] - (int)box[1] == 0 && box[3] - box[1] == ysize) {
        unsigned char *o = (unsigned char *)malloc((size_t)xsize * ysize * 4); int x0 = (int)box[0], y0 = (int)box[1];
        for (int y = 0; y < ysize; y++) memcpy(o + 4 * (size_t)xsize * y, pix + 4 * ((size_t)(y + y0) * w + x0), 4 * (size_t)xsize);
        return o;
    }
    int need_h = xsize != w || box[0] || box[2] != xsize, need_v = ysize != h || box[1] || box[3] != ysize;
    int horizontal_first = !((h - ysize) > 0 && (h - ysize) > (w - xsize) * 2);
    int ksize_h = 0, ksize_v = 0, *bounds_h = NULL, *bounds_v = NULL, ybox_first = 0, ybox_last = 0; double *kk_h = NULL, *kk_v = NULL;
    if ((need_h && horizontal_first) || need_v) ksize_v = rs_coeffs(h, box[1], box[3], ysize, &bounds_v, &kk_v);
    if (need_h) {
        if (horizontal_first) {
            ybox_first = bounds_v[0]; ybox_last = bounds_v[ysize * 2 - 2] + bounds_v[ysize * 2 - 1];
            if (ybox_first != 0) for (int i = 0; i < ysize; i++) bounds_v[i * 2] -= ybox_first;
        }
        ksize_h = rs_coeffs(w, box[0], box[2], xsize, &bounds_h, &kk_h);
    }
    const unsigned char *cur = pix; int cw = w; unsigned char *tmp = NULL, *nxt;
    if (horizontal_first) {
        if (need_h) { nxt = (unsigned char *)malloc((size_t)xsize * (ybox_last - ybox_first) * 4 + 4); rs_h(nxt, xsize, ybox_last - ybox_first, cur, cw, ybox_first, ksize_h, bounds_h, kk_h); tmp = nxt; cur = nxt; cw = xsize; }
        if (need_v) { nxt = (unsigned char *)malloc((size_t)xsize * ysize * 4 + 4); rs_v(nxt, xsize, ysize, cur, cw, ksize_v, bounds_v, kk_v); free(tmp); tmp = nxt; cur = nxt; }
    } else {
        if (need_v) { nxt = (unsigned char *)malloc((size_t)cw * ysize * 4 + 4); rs_v(nxt, cw, ysize, cur, cw, ksize_v, bounds_v, kk_v); tmp = nxt; cur = nxt; }
        if (need_h) { nxt = (unsigned char *)malloc((size_t)xsize * ysize * 4 + 4); rs_h(nxt, xsize, ysize, cur, cw, 0, ksize_h, bounds_h, kk_h); free(tmp); tmp = nxt; cur = nxt; cw = xsize; }
    }
    free(bounds_h); free(kk_h); free(bounds_v); free(kk_v);
    if (!tmp) { tmp = (unsigned char *)malloc((size_t)w * h * 4); memcpy(tmp, pix, (size_t)w * h * 4); }
    return tmp;
}
static int th_round_aspect(double number, double aspect, double other, int by_x) {          /* thumbnail's round_aspect */
    int f = (int)floor(number), c = (int)ceil(number); double kf, kc;
    if (by_x) { kf = fabs(aspect - f / other); kc = fabs(aspect - c / other); }
    else { kf = f == 0 ? 0 : fabs(aspect - other / f); kc = c == 0 ? 0 : fabs(aspect - other / c); }
    int r = kc < kf ? c : f; return r > 1 ? r : 1;
}
int mg_thumbnail_rgba(const unsigned char *pix, int w, int h, int px, unsigned char **out, int *ow, int *oh) {
    *out = NULL; *ow = w; *oh = h;
    if (px >= w && px >= h) return 0;
    double aspect = (double)w / h; int x = px, y = px;
    if ((double)x / y >= aspect) x = th_round_aspect(y * aspect, aspect, y, 1);
    else y = th_round_aspect(x / aspect, aspect, x, 0);
    if (x == w && y == h) return 0;
    /* Image.resize(size, LANCZOS, reducing_gap=2.0) of RGBA: as RGBa */
    unsigned char *a = (unsigned char *)malloc((size_t)w * h * 4); unsigned int t;
    for (size_t i = 0; i < (size_t)w * h; i++) {
        const unsigned char *q = pix + 4 * i; unsigned char *d = a + 4 * i; unsigned int al = q[3];
        d[0] = (unsigned char)MULDIV255(q[0], al, t); d[1] = (unsigned char)MULDIV255(q[1], al, t); d[2] = (unsigned char)MULDIV255(q[2], al, t); d[3] = q[3];
    }
    float box[4] = { 0, 0, (float)w, (float)h }; const unsigned char *src = a; unsigned char *red = NULL; int sw = w, sh = h;
    int fx = (int)((double)w / x / 2.0), fy = (int)((double)h / y / 2.0); if (!fx) fx = 1; if (!fy) fy = 1;
    if (fx > 1 || fy > 1) {
        double sx = (double)w / x, sy = (double)h / y, spx = 2.5 * sx, spy = 2.5 * sy;          /* _get_safe_box: LANCZOS support 3 - 0.5 */
        int rb[4] = { (int)(0 - spx) > 0 ? (int)(0 - spx) : 0, (int)(0 - spy) > 0 ? (int)(0 - spy) : 0,
                      (int)ceil(w + spx) < w ? (int)ceil(w + spx) : w, (int)ceil(h + spy) < h ? (int)ceil(h + spy) : h };
        red = mg_reduce4(a, w, h, fx, fy, rb, &sw, &sh); src = red;
        box[0] = (float)((0.0 - rb[0]) / fx); box[1] = (float)((0.0 - rb[1]) / fy); box[2] = (float)((double)(w - rb[0]) / fx); box[3] = (float)((double)(h - rb[1]) / fy);
    }
    unsigned char *r = mg_resample4(src, sw, sh, x, y, box);
    free(a); free(red);
    for (size_t i = 0; i < (size_t)x * y; i++) {          /* RGBa -> RGBA */
        unsigned char *q = r + 4 * i; unsigned int al = q[3];
        if (al != 255 && al != 0) for (int c = 0; c < 3; c++) { unsigned int v = (255 * q[c]) / al; q[c] = (unsigned char)(v > 255 ? 255 : v); }
    }
    *out = r; *ow = x; *oh = y;
    return 1;
}

/* ======================= run_set.sheet ======================= */
/* FONT BEGIN: generated by work\cscene\meshglue\make_font.py (DejaVu Sans, 10 pt at 70 dpi). Per glyph: width, height,
   x offset, y offset from the baseline (down positive), advance in 1/64 px, then width x height coverage bytes. */
#define FONT_ASC 10
#define FONT_DESC 3
static const short FONT_M[95][5] = {
    {3,0,0,0,192},{4,7,0,-7,256},{5,7,0,-7,320},{8,7,0,-7,512},{6,8,0,-7,384},{10,7,0,-7,640},{8,7,0,-7,512},{3,7,0,-7,192},{4,9,0,-8,256},{4,9,0,-8,256},{5,7,0,-7,320},{8,7,0,-7,512},{3,2,0,-1,192},{4,3,0,-3,256},{3,1,0,-1,192},{4,8,0,-7,192},{6,7,0,-7,384},{6,7,0,-7,384},{6,7,0,-7,384},{6,7,0,-7,384},{6,7,0,-7,384},{6,7,0,-7,384},{6,7,0,-7,384},{6,7,0,-7,384},{6,7,0,-7,384},{6,7,0,-7,384},{3,5,0,-5,192},{3,6,0,-5,192},{8,6,0,-6,512},{8,5,0,-5,512},{8,6,0,-6,512},{5,7,0,-7,320},{10,9,0,-7,640},{7,7,0,-7,448},{7,7,0,-7,448},{7,7,0,-7,448},{8,7,0,-7,512},{6,7,0,-7,384},{6,7,0,-7,384},{8,7,0,-7,512},{8,7,0,-7,512},{3,7,0,-7,192},{4,9,-1,-7,192},{7,7,0,-7,448},{6,7,0,-7,384},{9,7,0,-7,576},{7,7,0,-7,448},{8,7,0,-7,512},{6,7,0,-7,384},{8,8,0,-7,512},{7,7,0,-7,448},{6,7,0,-7,384},{8,7,-1,-7,384},{7,7,0,-7,448},{7,7,0,-7,448},{10,7,0,-7,640},{7,7,0,-7,448},{8,7,-1,-7,384},{7,7,0,-7,448},{4,9,0,-8,256},{4,8,0,-7,192},{4,9,0,-8,256},{8,7,0,-7,512},{7,2,-1,0,320},{5,8,0,-8,320},{6,5,0,-5,384},{6,8,0,-8,384},{6,5,0,-5,384},{6,8,0,-8,384},{6,5,0,-5,384},{4,8,0,-8,256},{6,7,0,-5,384},{6,8,0,-8,384},{3,8,0,-8,192},{4,10,-1,-8,192},{6,8,0,-8,384},{3,8,0,-8,192},{10,5,0,-5,640},{6,5,0,-5,384},{6,5,0,-5,384},{6,7,0,-5,384},{6,7,0,-5,384},{5,5,0,-5,256},{5,5,0,-5,320},{4,7,0,-7,256},{6,5,0,-5,384},{6,5,0,-5,384},{8,5,0,-5,512},{6,5,0,-5,384},{6,7,0,-5,384},{5,5,0,-5,320},{6,9,0,-8,384},{3,10,0,-8,192},{6,9,0,-8,384},{8,5,0,-5,512} };
static const char *const FONT_B[95] = {
    "",
    "007c8000007c8000007c800000787b00006b6e0000000000007c8000",
    "08cc30a40008cc30a40008cc30a4000000000000000000000000000000000000000000",
    "000000893f527700000000c00793370000acffffffffff9c00004e7b19af00003cffffffffffff0c0000c5049d2e00000014b600c8010000",
    "00001c6100000082edffff181ddb2f60000008d1988d140000095fb9d43b00001c6892782bf7f3f7b91600001c600000",
    "0ec6ef570000a522000060881cd000567100000061881cd013b1020000000fc8ef599d35c1f060000000004b7c589018d50000000eb405598f18d40000009533000bc4f06000",
    "001fcbef7d000000007e950f800f00000046cb130000000008b541bd2700d80a50a2001ec87b9e0032eb3b0a62ff58000069e0f7b239e125",
    "08cc0008cc0008cc00000000000000000000000000",
    "000dbe020077640000d314000de200001fd400000de2000000d3130000776400000dbe02",
    "0abf030000805a00002fb8000002ed000000ed060002ed00002eb800007f5b000abf0400",
    "763e903e76078fde8f07078ede8e07763e903e76000000000000000000000000000000",
    "00000038980000000000003898000000000000389800000000f0ffffffffff50000000389800000000000038980000000000003898000000",
    "00d72d16b400",
    "84ffff200000000000000000",
    "00f018",
    "00009d360003cf01004093000092420000cf0400359f0000864d0000cb080000",
    "0046daed8b0109e94216cc503dc30000659751ad00004eac3dc3000065980aea4216cc510049dbed8d02",
    "00e8ffd40000000024d40000000024d40000000024d40000000024d40000000024d4000000c4ffffff70",
    "047be7e9840033821826e33c00000000ca2c0000007c8b0000018e87000007a97600000044ffffffff5c",
    "005ee0ed930203981c19ce4d00000016c9390000fcffa40200000015ae69356a0f17b0730589edea9c09",
    "000008c9c40000008f6ec40000516d38c4001d9a0038c40082ffffffffcc00000038c40000000038c400",
    "00ecfffff40000e80000000000ebf1e576000000012ddd41000000008d700000012cdd423cfffee27400",
    "001cb3f2ff4400c79610000027f00900000044ccb4f6c11f30f5300988a004d83008889e003bd4f5bc1c",
    "2cffffffff7100000002da180000004cab00000000b74400000023da010000008c7600000009e7150000",
    "0070e7f3b00d15f0270db1700be0260db1550068fcffb8042ed91e0b94823dda1e0b9494018debf5bc1b",
    "0089ebe87d0043d21911ba4246d21911ba8a0190eee49e9f000000009d820000044cf22600e8fcd44c00",
    "00d43400000000000000000000d434",
    "00d43400000000000000000000d72d16b400",
    "000000001061b642000d5cb1bd6e1a0000e4d33400000000000e5db2bc6d1a00000000001162b7420000000000000000",
    "00f0ffffffffff50000000000000000000f0ffffffffff5000000000000000000000000000000000",
    "00be822b0000000000074ea3c57d2600000000000e9bff4400074da2c67e270000be832c000000000000000000000000",
    "48fffdd42f0000088f8a000033d0240000dc2600000ee0000000000000000018e80000",
    "00002cabeef6c75a0000003cd75d160737bb810003c81b0000000000b11737850086f2c7b814743f525e04de201edb34c41438830089f3c8b0ca420004d120000000000000000042dc5b181667940000000031b2f1edb6350000",
    "000057fd2e00000000bb729400000023b502cd090000884a00725f0005e8ffffffc500549d000000c52bb9490000006f91",
    "04fffffbd43d0004f8000464c50004f8000464b40004fffffff4430004f8000232e90b04f8000233f81404fffffde26b00",
    "001aa5e6eb9e1005d38919125f5e46cd000000000066a5000000000046cd000000000005d38919125f5e001aa5e7ec9d10",
    "04fffff8d983070004f8000b37c68f0004f800000022f20004f800000000f91104f800000021f20004f8000a35c48e0004fffffada840700",
    "04ffffffff9804f80000000004f80000000004ffffffff7004f80000000004f80000000004ffffffffac",
    "04ffffffff2c04f80000000004f80000000004ffffffdc0004f80000000004f80000000004f800000000",
    "001ca4e4efb62b0006d58a1b0e468f0046cd00000000000066a50000a8ffec0046cd0000000cec0006d58a1b0634eb00001ca4e5f1c03e00",
    "04f800000074880004f800000074880004f800000074880004ffffffffff880004f800000074880004f800000074880004f8000000748800",
    "04f80004f80004f80004f80004f80004f80004f800",
    "0004f8000004f8000004f8000004f8000004f8000004f8000008f200004bd10083e34b00",
    "04f8000055cf1e04f8006cc4120004f886b30a000004fded3800000004f833dd3a000004f8002ddd410004f8000028db47",
    "04f80000000004f80000000004f80000000004f80000000004f80000000004f80000000004ffffffff84",
    "04ffa200000bf3a80004f0cc0c005ec7a80004f07a5f00bb6ba80004f01dbb1eb950a80004f000bb985d50a80004f0005ff30b50a80004f00000000050a800",
    "04ff960000748004f3d42700748004f050b000748004f000c23e748004f00036c9758004f00000a8cb8004f0000021f980",
    "001cacece69d110005d58414199eba0046ce00000006e92666a500000000c64547ce00000005e92606d78413199eba00001eaeede79d1100",
    "04fffff5bf1e04f8000a999704f80009979704fffff6c11f04f80000000004f80000000004f800000000",
    "001cacece79e130005d58414199ebd0046ce00000006e92866a500000000c64746ce00000005e82705d58413199eb300001dacecffb10c000000000033db3300",
    "04fffff6c3220004f80009959a0004f8000996900004ffffffd40f0004f8001dc4550004f800002cda0404f8000000a965",
    "0086e8ec920b40df27106a5032ca09000000004ca0a2850f000000006aa2477b1c0e8fb50781e3f4c22b",
    "08ffffffffffff24000000708c000000000000708c000000000000708c000000000000708c000000000000708c000000000000708c000000",
    "20dc0000008c7420dc0000008c7420dc0000008c7420dc0000008c7412ea0000009b6600d863092fe72e0032c7f5df6800",
    "ba4d000000779054b2000000d92b05e41a003fc50000887b00a45f000024dc12e70a000000bdb0940000000057ff2e0000",
    "8b740000bda00000926e48b70008c5c90000d42b0bee05428ca62516e60000c23b844b666758a500007f7dc20e25a99a6200003cccc40000cfdc1f000005f4880000a4dd0000",
    "1adb1a001dd9180057b703bf4d000000a6c3940000000046ff2d0000000dd570c6030000a16e00917d0058b6010009d234",
    "00aa65000049c2020012d82413d92000000044c6ae5e000000000098b4000000000000708c000000000000708c000000000000708c000000",
    "70ffffffffff42000000005f990000000043b2040000002cc00d00000019c51a0000000cc02d0000000082ffffffffff68",
    "24fff00024c4000024c4000024c4000024c4000024c4000024c4000024c4000024fff000",
    "cb080000864d0000359f000000cf030000924200004093000003cf0100009d37",
    "08ffff0c0000dc0c0000dc0c0000dc0c0000dc0c0000dc0c0000dc0c0000dc0c08ffff0c",
    "000004b8ec2c000000009d8e3cd51a00007f8c00003ac30e0000000000000000000000000000000000000000000000000000000000000000",
    "0000000000000018ffffffffff18",
    "04ba2b00000016b10500000000000000000000000000000000000000000000000000000000000000",
    "00ffffe668000000001cd11211b4f2ffff345ab61521d83817caf4b8bd38",
    "18d00000000018d00000000018d00000000018d4a2f1bd1718fb450b8a9718da00002ac218fb430a889718d4a3f1be17",
    "006ce0fee00039d92a020000698c0000000039da2a010000006fe2fee000",
    "0000000074700000000074700000000074700087efd19b703fd21d1bce706a8600007f703fd01c1acd700087efd19b70",
    "006ce1f2a70837c81d0a8f7168ffffffff9d39c31f0100000069defdff68",
    "0053eeb800cb2b0000e50000c4ffff7800e8000000e8000000e8000000e80000",
    "008aefd19b7040d01c19cb706a8600007e7040d01c19cb70008aefd1a56200000023d43000c8ffe87a00",
    "18d00000000018d00000000018d00000000018d49df1c00e18f9400aa56318d20000687b18d00000687c18d00000687c",
    "10d80000000000000010d80010d80010d80010d80010d800",
    "0010d80000000000000000000010d8000010d8000010d8000010d8000010d7000032c60030f15b00",
    "18d00000000018d00000000018d00000000018d00039c12a18d065b0100018ebe61c000018d035cb2b0018d00021c841",
    "10d80010d80010d80010d80010d80010d80010d80010d800",
    "18d5a7f2b147e2eb4d0018f83d0cc0ae1142cb0018d200008c5e0004e30018d000008c5c0004e40018d000008c5c0004e400",
    "18d49df1c00e18f9400aa56318d20000687b18d00000687c18d00000687c",
    "007be8ed93023dd62117c05d6a8a00006b893ed51f16c05e007ee9ee9603",
    "18d4a2f1bd1718fb450b8a9718da00002ac218fb430a889718d4a3f1be1718d00000000018d000000000",
    "0087efd19b703fd21d1bce706a8600007f703fd01c1acd700087efd19b70000000007470000000007470",
    "18d49ef31c18fb44020018d500000018d000000018d0000000",
    "18c5f9ff6c5ea50500000e99c3a4270000026da874fffcd841",
    "14d4000014d40000bcffffb014d4000014d400000be3090000a5f8b0",
    "28c00000787028c00000787027c000007a7011e61b1ac9700082f2cc9b70",
    "80730000896a1bd60106dd0d00ae435a97000045aac22e000001d7c40000",
    "70740095c300499e27be00c8bb11915500d836a17358d60f0095c3572ac3c100004cfc1000e07800",
    "2cd01217d3240052bbc148000001cdb8000000808fad600050ba030ccc37",
    "7a7700008d6511d6040cd9080091566e7c000020c3d112000000a9920000000ecb22000034fb8c000000",
    "74ffffffc30000058a1e00068b1c00078c1a000083ffffffd0",
    "000005bcf71c000030c6080000003cab000000026e99000000c0f636000000037e99000000003eab0000000030c60700000005bef81c",
    "00bc1800bc1800bc1800bc1800bc1800bc1800bc1800bc1800bc1800bc18",
    "00bde03800000001718c000000005098000000003ec10a00000004cdff1c00003ece0f0000004f9a000000016f8c000000bde13a0000",
    "00000000000000010061e3da620c604a00701335b4f1a30b00000000000000000000000000000000",
};
/* FONT END */

void mg_sheet_title(const char *base, const char *detail, int ok, int variant, char *out, int cap) {
    snprintf(out, (size_t)cap, "%s  detail %s  %s%s", base, detail ? detail : "", ok ? "OK" : "FLAGGED", variant ? " (variant)" : "");
}
/* GridSpecBase.get_grid_positions for equal ratios: cell edges in figure fractions */
static void grid_pos(int nrows, int ncols, double left, double right, double bottom, double top, double wspace, double hspace,
                     double *tops, double *bottoms, double *lefts, double *rights) {
    double tot_w = right - left, tot_h = top - bottom;
    double cell_h = tot_h / (nrows + hspace * (nrows - 1)), sep_h = hspace * cell_h, acc = 0;
    for (int r = 0; r < nrows; r++) { acc += r ? sep_h : 0; tops[r] = top - acc; acc += cell_h; bottoms[r] = top - acc; }
    double cell_w = tot_w / (ncols + wspace * (ncols - 1)), sep_w = wspace * cell_w; acc = 0;
    for (int c = 0; c < ncols; c++) { acc += c ? sep_w : 0; lefts[c] = left + acc; acc += cell_w; rights[c] = left + acc; }
}
/* the axes box of an image of aspect a (rows / columns) in a cell: adjustable "box", anchored in the middle */
static void fit_box(double x0, double y0, double x1, double y1, double a, double Win, double Hin, double *b) {
    double cw = (x1 - x0) * Win, ch = (y1 - y0) * Hin, w = cw, hh = cw * a;
    if (hh > ch) { hh = ch; w = ch / a; }
    double cx = (x0 + x1) / 2, cy = (y0 + y1) / 2;
    b[0] = cx - w / Win / 2; b[2] = cx + w / Win / 2; b[1] = cy - hh / Hin / 2; b[3] = cy + hh / Hin / 2;
}
static double title_top(const char *t) {          /* the title's top above the axes top, inches (measured on matplotlib 3.11) */
    return strpbrk(t, "_()[]{}|$@QJjgpqy,;") ? 0.19333333333333 : 0.18885633680556;
}
static int text_w64(const char *s) { int w = 0; for (; *s; s++) { int c = (unsigned char)*s; if (c >= 32 && c < 127) w += FONT_M[c - 32][4]; } return w; }
static void draw_text(unsigned char *im, int W, int H, const char *s, double xc, double base, unsigned rgb) {
    double x = xc - text_w64(s) / 64.0 / 2; int bx = (int)floor(x + 0.5), by = (int)floor(base + 0.5);
    unsigned char col[3] = { (unsigned char)(rgb >> 16), (unsigned char)(rgb >> 8), (unsigned char)rgb };
    for (; *s; s++) {
        int c = (unsigned char)*s; if (c < 32 || c >= 127) continue;
        const short *m = FONT_M[c - 32]; const char *hex = FONT_B[c - 32];
        for (int y = 0; y < m[1]; y++) for (int xx = 0; xx < m[0]; xx++) {
            int px = bx + m[2] + xx, py = by + m[3] + y; if (px < 0 || py < 0 || px >= W || py >= H) continue;
            char hb[3] = { hex[2 * (y * m[0] + xx)], hex[2 * (y * m[0] + xx) + 1], 0 }; int a = (int)strtol(hb, NULL, 16);
            unsigned char *q = im + 4 * ((size_t)py * W + px);
            for (int k = 0; k < 3; k++) q[k] = (unsigned char)((q[k] * (255 - a) + col[k] * a + 127) / 255);
        }
        bx += m[4] / 64;
    }
}
unsigned char *mg_sheet(const MgSheetItem *items, int n, int *Wp, int *Hp, double *axes_out) {
    int rows = (n + 4) / 5, cols = 5;
    if (rows > 64) rows = 64;
    if (rows < 1) rows = 1;
    int na = rows * cols;
    double Win = 20.0, Hin = 4.2 * rows, pad = 1.08 * 10.0 / 72.0;
    double tops[64], bots[64], lefts[5], rights[5];
    /* tight_layout's one pass: the default subplot params; every axes' tight box (the axes after its aspect, and the
       title above it, only as wide as the axes) against its cell */
    grid_pos(rows, cols, 0.125, 0.9, 0.11, 0.88, 0.2, 0.2, tops, bots, lefts, rights);
    double *vs = (double *)calloc((size_t)(rows + 1) * cols, sizeof(double)), *hs = (double *)calloc((size_t)rows * (cols + 1), sizeof(double));
    for (int i = 0; i < na; i++) {
        int r = i / cols, c = i % cols; double b[4] = { lefts[c], bots[r], rights[c], tops[r] }, t[4];
        const MgSheetItem *it = i < n ? &items[i] : NULL;
        if (it && it->rgba) fit_box(lefts[c], bots[r], rights[c], tops[r], (double)it->h / it->w, Win, Hin, b);
        memcpy(t, b, sizeof t);
        if (it && it->title && it->title[0]) t[3] = b[3] + title_top(it->title) / Hin;
        hs[r * (cols + 1) + c] += lefts[c] - t[0]; hs[r * (cols + 1) + c + 1] += t[2] - rights[c];
        vs[r * cols + c] += t[3] - tops[r]; vs[(r + 1) * cols + c] += bots[r] - t[1];
    }
    double mx;
    mx = -1e9; for (int r = 0; r < rows; r++) if (hs[r * (cols + 1)] > mx) mx = hs[r * (cols + 1)];
    double ml = (mx > 0 ? mx : 0) + pad / Win;
    mx = -1e9; for (int r = 0; r < rows; r++) if (hs[r * (cols + 1) + cols] > mx) mx = hs[r * (cols + 1) + cols];
    double mr = (mx > 0 ? mx : 0) + pad / Win;
    mx = -1e9; for (int c = 0; c < cols; c++) if (vs[c] > mx) mx = vs[c];
    double mt = (mx > 0 ? mx : 0) + pad / Hin;
    mx = -1e9; for (int c = 0; c < cols; c++) if (vs[rows * cols + c] > mx) mx = vs[rows * cols + c];
    double mb = (mx > 0 ? mx : 0) + pad / Hin;
    mx = -1e9; for (int r = 0; r < rows; r++) for (int c = 1; c < cols; c++) if (hs[r * (cols + 1) + c] > mx) mx = hs[r * (cols + 1) + c];
    double hsp = mx + pad / Win, h_axes = (1 - mr - ml - hsp * (cols - 1)) / cols, wspace = hsp / h_axes, hspace = 0.2;
    if (rows > 1) {
        mx = -1e9; for (int r = 1; r < rows; r++) for (int c = 0; c < cols; c++) if (vs[r * cols + c] > mx) mx = vs[r * cols + c];
        double vsp = mx + pad / Hin, v_axes = (1 - mt - mb - vsp * (rows - 1)) / rows; hspace = vsp / v_axes;
    }
    free(vs); free(hs);
    grid_pos(rows, cols, ml, 1 - mr, mb, 1 - mt, wspace, hspace, tops, bots, lefts, rights);          /* fig.subplots_adjust(**kwargs) */
    /* the picture at 70 dpi on a white figure */
    int W = (int)(Win * 70 + 0.5), H = (int)(Hin * 70 + 0.5);
    unsigned char *im = (unsigned char *)malloc((size_t)W * H * 4); memset(im, 255, (size_t)W * H * 4);
    for (int i = 0; i < na; i++) {
        int r = i / cols, c = i % cols; double b[4] = { lefts[c], bots[r], rights[c], tops[r] };
        const MgSheetItem *it = i < n ? &items[i] : NULL;
        if (it && it->rgba) fit_box(lefts[c], bots[r], rights[c], tops[r], (double)it->h / it->w, Win, Hin, b);
        if (axes_out && i < n) memcpy(axes_out + 4 * i, b, sizeof b);
        if (!it) continue;
        if (it->rgba) {
            int x0 = (int)floor(b[0] * W + 0.5), x1 = (int)floor(b[2] * W + 0.5), y0 = (int)floor((1 - b[3]) * H + 0.5), y1 = (int)floor((1 - b[1]) * H + 0.5);
            if (x1 > x0 && y1 > y0) {
                unsigned char *src = (unsigned char *)malloc((size_t)it->w * it->h * 4); unsigned int t;
                for (size_t k = 0; k < (size_t)it->w * it->h; k++) {          /* over the white figure */
                    const unsigned char *q = it->rgba + 4 * k; unsigned int a = q[3];
                    for (int ch = 0; ch < 3; ch++) src[4 * k + ch] = (unsigned char)(MULDIV255(q[ch], a, t) + MULDIV255(255, 255 - a, t));
                    src[4 * k + 3] = 255;
                }
                float fb[4] = { 0, 0, (float)it->w, (float)it->h };
                unsigned char *rs = mg_resample4(src, it->w, it->h, x1 - x0, y1 - y0, fb); free(src);
                for (int y = y0; y < y1; y++) if (y >= 0 && y < H) for (int x = x0; x < x1; x++) if (x >= 0 && x < W) memcpy(im + 4 * ((size_t)y * W + x), rs + 4 * ((size_t)(y - y0) * (x1 - x0) + (x - x0)), 4);
                free(rs);
            }
        }
        if (it->title && it->title[0]) draw_text(im, W, H, it->title, (b[0] + b[2]) / 2 * W, (1 - b[3]) * H - 6.0 / 72.0 * 70.0, it->ok ? MG_SHEET_OK_RGB : MG_SHEET_FLAG_RGB);
    }
    *Wp = W; *Hp = H;
    return im;
}

/* ======================= run_set.drop_specks ======================= */
/* trimesh.util.unitize(crosses, check_valid=True) per face: the unit normal, or 0 when |cross| is not above 1e-13
   (triangles.normals of triangles.cross: np.cross(v1 - v0, v2 - v1)); returns valid */
static int face_normal(const double *a, const double *b, const double *c, double *n) {
    double u0 = b[0] - a[0], u1 = b[1] - a[1], u2 = b[2] - a[2], w0 = c[0] - b[0], w1 = c[1] - b[1], w2 = c[2] - b[2];
    double cx = u1 * w2 - u2 * w1, cy = u2 * w0 - u0 * w2, cz = u0 * w1 - u1 * w0, nn = sqrt(cx * cx + cy * cy + cz * cz);
    if (!(nn > 1e-13)) { n[0] = n[1] = n[2] = 0; return 0; }
    double r = 1.0 / nn; n[0] = cx * r; n[1] = cy * r; n[2] = cz * r; return 1;
}
/* the face_normals setter: the given normals are kept only if finite, not all ~0 (ptp >= 1e-8) and the first 20 match
   the computed ones (np.allclose: |a - b| <= 1e-8 + 1e-5 |b|). f: nt faces (3 indices), v: the vertices */
static int normals_ok(const double *v, const int *f, int nt, const double *fn) {
    double lo = INFINITY, hi = -INFINITY;
    for (int i = 0; i < 3 * nt; i++) { if (!isfinite(fn[i])) return 0; if (fn[i] < lo) lo = fn[i]; if (fn[i] > hi) hi = fn[i]; }
    if (nt == 0 || hi - lo < 1e-8) return 0;
    for (int i = 0; i < nt && i < 20; i++) {
        double c[3]; face_normal(v + 3 * f[3 * i], v + 3 * f[3 * i + 1], v + 3 * f[3 * i + 2], c);
        for (int k = 0; k < 3; k++) if (!(fabs(c[k] - fn[3 * i + k]) <= 1e-8 + 1e-5 * fabs(fn[3 * i + k]))) return 0;
    }
    return 1;
}
/* a CPython set of non-negative ints (hash(i) == i): set_add_entry / set_table_resize / set_insert_clean of
   Objects/setobject.c with no deletions, so the iteration order (the table order) is Python's */
typedef struct { int64_t *t; uint64_t mask; int64_t fill; } ISet;          /* t[i] = -1 empty */
static void iset_init(ISet *s) { s->mask = 7; s->fill = 0; s->t = (int64_t *)malloc(sizeof(int64_t) * 8); for (int i = 0; i < 8; i++) s->t[i] = -1; }
static void iset_clean(int64_t *t, uint64_t mask, int64_t key) {
    uint64_t perturb = (uint64_t)key, i = (uint64_t)key & mask;
    for (;;) {
        if (t[i] < 0) { t[i] = key; return; }
        if (i + 9 <= mask) for (int j = 1; j <= 9; j++) if (t[i + j] < 0) { t[i + j] = key; return; }
        perturb >>= 5; i = (i * 5 + 1 + perturb) & mask;
    }
}
static void iset_add(ISet *s, int64_t key) {          /* the key is known not to be there */
    uint64_t mask = s->mask, i = (uint64_t)key & mask, perturb = (uint64_t)key;
    for (;;) {
        int probes = (i + 9 <= mask) ? 9 : 0; uint64_t j = i;
        do { if (s->t[j] < 0) goto found; j++; } while (probes--);
        perturb >>= 5; i = (i * 5 + 1 + perturb) & mask;
        continue;
found:
        s->t[j] = key; s->fill++;
        if ((uint64_t)s->fill * 5 < mask * 3) return;
        uint64_t ns = 8; int64_t minused = s->fill > 50000 ? s->fill * 2 : s->fill * 4;
        while (ns <= (uint64_t)minused) ns <<= 1;
        int64_t *nt = (int64_t *)malloc(sizeof(int64_t) * ns); for (uint64_t k = 0; k < ns; k++) nt[k] = -1;
        for (uint64_t k = 0; k <= mask; k++) if (s->t[k] >= 0) iset_clean(nt, ns - 1, s->t[k]);
        free(s->t); s->t = nt; s->mask = ns - 1;
        return;
    }
}
/* an nx.Graph built by from_edgelist: nodes in insertion order, each node's neighbours in insertion order (no repeats) */
typedef struct { int n, *order, *pos; int **nb, *nn, *cap; } NxG;
static void nx_node(NxG *g, int u) { if (g->pos[u] < 0) { g->pos[u] = g->n; g->order[g->n++] = u; } }
static void nx_link(NxG *g, int u, int v) {
    for (int k = 0; k < g->nn[u]; k++) if (g->nb[u][k] == v) return;
    if (g->nn[u] == g->cap[u]) { g->cap[u] = g->cap[u] ? 2 * g->cap[u] : 4; g->nb[u] = (int *)realloc(g->nb[u], sizeof(int) * (size_t)g->cap[u]); }
    g->nb[u][g->nn[u]++] = v;
}
static void nx_edge(NxG *g, int u, int v) { nx_node(g, u); nx_node(g, v); nx_link(g, u, v); if (u != v) nx_link(g, v, u); }
static void nx_new(NxG *g, int count) {
    g->n = 0; g->order = (int *)malloc(sizeof(int) * (size_t)(count + 1)); g->pos = (int *)malloc(sizeof(int) * (size_t)(count + 1));
    g->nb = (int **)calloc((size_t)count + 1, sizeof(int *)); g->nn = (int *)calloc((size_t)count + 1, sizeof(int)); g->cap = (int *)calloc((size_t)count + 1, sizeof(int));
    for (int i = 0; i <= count; i++) g->pos[i] = -1;
}
static void nx_free(NxG *g, int count) { for (int i = 0; i <= count; i++) free(g->nb[i]); free(g->nb); free(g->nn); free(g->cap); free(g->order); free(g->pos); }
/* an edge of a face list: the face (or edge) index and its key (hashable_rows of the sorted pair: hi in the high half) */
typedef struct { int64_t key; int idx; } Ek2;
static int ek2_cmp(const void *a, const void *b) { const Ek2 *x = (const Ek2 *)a, *y = (const Ek2 *)b; return x->key < y->key ? -1 : x->key > y->key ? 1 : x->idx - y->idx; }
static int64_t pair_key(int a, int b) { int lo = a < b ? a : b, hi = a < b ? b : a; return ((int64_t)hi << 32) | (int64_t)lo; }
/* the edges (sorted keys) of nt faces; *ne = 3 nt; edge j belongs to face j / 3 */
static Ek2 *edges_sorted(const int *f, int nt) {
    Ek2 *e = (Ek2 *)malloc(sizeof(Ek2) * (size_t)(3 * nt + 1));
    for (int i = 0; i < nt; i++) for (int k = 0; k < 3; k++) { e[3 * i + k].key = pair_key(f[3 * i + k], f[3 * i + (k + 1) % 3]); e[3 * i + k].idx = 3 * i + k; }
    qsort(e, (size_t)(3 * nt), sizeof(Ek2), ek2_cmp);
    return e;
}
static void used_add(int **used, int *nused, int *capu, int x, int y) {
    for (int q = 0; q < nused[x]; q++) if (used[x][q] == y) return;
    if (nused[x] == capu[x]) { capu[x] = capu[x] ? 2 * capu[x] : 4; used[x] = (int *)realloc(used[x], sizeof(int) * (size_t)capu[x]); }
    used[x][nused[x]++] = y;
}
/* repair.fill_holes(part, use_fan=False): every boundary cycle of 3 or 4 edges (nx.cycle_basis of the boundary edges)
   closed with faces wound against the boundary; the faces are appended to *f (*nt grows). mesh.extend_faces then drops
   every new face of zero area (triangles.normals' valid: |cross| above 1e-13) and keeps the part's cached normals,
   the new faces' own normals added after them. Returns 1 if faces were added */
static int fill_holes(int **fp, int *ntp, int nv, const double *v) {
    int nt = *ntp, *f = *fp, ne = 3 * nt;
    if (nt < 3) return 0;
    Ek2 *e = edges_sorted(f, nt); int nb = 0, all2 = 1;
    int *bidx = (int *)malloc(sizeof(int) * (size_t)(ne + 1));
    for (int i = 0; i < ne;) { int j = i; while (j < ne && e[j].key == e[i].key) j++; if (j - i == 1) bidx[nb++] = e[i].idx; if (j - i != 2) all2 = 0; i = j; }
    free(e);
    if (all2 || nb < 3) { free(bidx); return 0; }          /* watertight (or too little boundary): nothing to fill */
    /* boundary = mesh.edges[boundary_groups] (directed as in the faces), in hashable order */
    int *bd = (int *)malloc(sizeof(int) * 2 * (size_t)nb);
    for (int k = 0; k < nb; k++) { int fi = bidx[k] / 3, c = bidx[k] % 3; bd[2 * k] = f[3 * fi + c]; bd[2 * k + 1] = f[3 * fi + (c + 1) % 3]; }
    free(bidx);
    NxG g; nx_new(&g, nv);
    for (int k = 0; k < nb; k++) nx_edge(&g, bd[2 * k], bd[2 * k + 1]);
    /* networkx.cycle_basis(G): gnodes = dict.fromkeys(G); popitem() takes the LAST node left */
    unsigned char *gone = (unsigned char *)calloc((size_t)nv + 1, 1); int top = g.n;
    int *pred = (int *)malloc(sizeof(int) * (size_t)(nv + 1)), *stack = (int *)malloc(sizeof(int) * (size_t)(nb * 2 + 2));
    int **used = (int **)calloc((size_t)nv + 1, sizeof(int *)), *nused = (int *)calloc((size_t)nv + 1, sizeof(int)), *capu = (int *)calloc((size_t)nv + 1, sizeof(int));
    unsigned char *inused = (unsigned char *)calloc((size_t)nv + 1, 1);
    int *visited = (int *)malloc(sizeof(int) * (size_t)(nv + 1)), nvis;
    int (*cyc)[4] = malloc(sizeof(int) * 4 * (size_t)(nb + 1)); int *clen = (int *)malloc(sizeof(int) * (size_t)(nb + 1)), ncyc = 0, nbig = 0;
    for (int i = 0; i <= nv; i++) pred[i] = -1;
/* used[x].add(y) */
    for (;;) {
        while (top > 0 && gone[g.order[top - 1]]) top--;
        if (top == 0) break;
        int root = g.order[--top]; gone[root] = 1;
        int sp = 0; stack[sp++] = root; pred[root] = root; inused[root] = 1; nused[root] = 0; nvis = 0; visited[nvis++] = root;
        while (sp) {
            int z = stack[--sp];
            for (int k = 0; k < g.nn[z]; k++) {
                int nbr = g.nb[z][k];
                if (!inused[nbr]) { pred[nbr] = z; stack[sp++] = nbr; inused[nbr] = 1; nused[nbr] = 0; used_add(used, nused, capu, nbr, z); visited[nvis++] = nbr; }
                else if (nbr == z) { if (ncyc <= nb) { cyc[ncyc][0] = z; clen[ncyc++] = 1; } }
                else {
                    int inz = 0; for (int q = 0; q < nused[z]; q++) if (used[z][q] == nbr) { inz = 1; break; }
                    if (inz) continue;
                    int c[4], cl = 0, big = 0; c[cl++] = nbr; c[cl++] = z; int p = pred[z];
                    for (;;) {
                        int inpn = 0; for (int q = 0; q < nused[nbr]; q++) if (used[nbr][q] == p) { inpn = 1; break; }
                        if (inpn) break;
                        if (cl < 4) c[cl] = p; cl++; p = pred[p];
                        if (cl > 4) big = 1;
                        if (cl > nv + 2) break;
                    }
                    if (cl < 4) c[cl] = p; cl++;
                    if (cl > 4) big = 1;
                    if (big) nbig++;
                    else { memcpy(cyc[ncyc], c, sizeof c); clen[ncyc++] = cl; }
                    used_add(used, nused, capu, nbr, z);
                }
            }
        }
        for (int k = 0; k < nvis; k++) { gone[visited[k]] = 1; inused[visited[k]] = 0; }          /* for node in pred: gnodes.pop(node) */
    }
    /* triangulate_quads(holes, use_fan=False): all 3 long -> as they are; all 4 long -> [0 1 2] then [2 3 0] of each;
       mixed -> the triangles, then the quads' first halves, then their second halves; longer cycles are not filled */
    int n3 = 0, n4 = 0;
    for (int k = 0; k < ncyc; k++) { if (clen[k] == 3) n3++; else if (clen[k] == 4) n4++; }
    int nn_ = n3 + 2 * n4, *nf = (int *)malloc(sizeof(int) * 3 * (size_t)(nn_ + 1)), m = 0;
    int ragged = (n3 && n4) || nbig || (ncyc && (n3 + n4) != ncyc);
    if (!ragged && ncyc && clen[0] != 3 && clen[0] != 4) nn_ = 0;
    if (nn_) {
        for (int k = 0; k < ncyc; k++) if (clen[k] == 3) { memcpy(nf + 3 * m, cyc[k], sizeof(int) * 3); m++; }
        for (int k = 0; k < ncyc; k++) if (clen[k] == 4) { nf[3 * m] = cyc[k][0]; nf[3 * m + 1] = cyc[k][1]; nf[3 * m + 2] = cyc[k][2]; m++; }
        for (int k = 0; k < ncyc; k++) if (clen[k] == 4) { nf[3 * m] = cyc[k][2]; nf[3 * m + 1] = cyc[k][3]; nf[3 * m + 2] = cyc[k][0]; m++; }
        /* a new face with an edge running the same way as a boundary edge is turned over */
        for (int k = 0; k < m; k++) {
            int rev = 0;
            for (int c = 0; c < 3 && !rev; c++) { int a = nf[3 * k + c], b = nf[3 * k + (c + 1) % 3]; for (int q = 0; q < nb; q++) if (bd[2 * q] == a && bd[2 * q + 1] == b) { rev = 1; break; } }
            if (rev) { int t = nf[3 * k]; nf[3 * k] = nf[3 * k + 2]; nf[3 * k + 2] = t; }
        }
        { int w = 0; double dn[3];          /* extend_faces: new_faces[valid] */
          for (int k = 0; k < m; k++) if (face_normal(v + 3 * nf[3 * k], v + 3 * nf[3 * k + 1], v + 3 * nf[3 * k + 2], dn)) { if (w != k) memcpy(nf + 3 * w, nf + 3 * k, sizeof(int) * 3); w++; }
          m = w; if (!m) nn_ = 0; }
    }
    if (nn_) {
        f = (int *)realloc(f, sizeof(int) * 3 * (size_t)(nt + m)); memcpy(f + 3 * nt, nf, sizeof(int) * 3 * (size_t)m);
        *fp = f; *ntp = nt + m;
    }
    free(nf); free(cyc); free(clen); free(visited); free(inused); free(gone); free(pred); free(stack); free(bd);
    for (int i = 0; i <= nv; i++) free(used[i]);
    free(used); free(nused); free(capu); nx_free(&g, nv);
    return nn_ > 0;
}
int mg_drop_specks(const unsigned char *stl, size_t n, double max_mm3, int allow_supports, MgBuf *out, int *removed) {
    *removed = 0;
    if (allow_supports) return 0;          /* the separate shells are the objects' true solids, not dust */
    if (n < 84) return fail("HeaderError: Binary STL shorter than a fixed header!");
    uint32_t nt; memcpy(&nt, stl + 80, 4);
    if (84 + 50 * (size_t)nt != n) return fail("HeaderError: Binary STL has incorrect length in header");
    MgMesh m; memset(&m, 0, sizeof m); m.nt = (int)nt; m.nv = 3 * (int)nt;
    m.v = (double *)malloc(sizeof(double) * 9 * (size_t)(nt + 1)); m.f = (int *)malloc(sizeof(int) * 3 * (size_t)(nt + 1));
    double *fn = (double *)malloc(sizeof(double) * 3 * (size_t)(nt + 1));
    for (uint32_t i = 0; i < nt; i++) {
        float c[12]; memcpy(c, stl + 84 + 50 * (size_t)i, 48);
        for (int k = 0; k < 3; k++) fn[3 * i + k] = c[k];
        for (int k = 0; k < 9; k++) m.v[9 * (size_t)i + k] = c[3 + k];
        for (int k = 0; k < 3; k++) m.f[3 * i + k] = 3 * (int)i + k;
    }
    int keep_fn = normals_ok(m.v, m.f, m.nt, fn);          /* Trimesh(vertices, faces, face_normals): the setter's check */
    mg_merge_vertices(&m);                                 /* process(): merge_vertices (the normals stay cached) */
    /* face_adjacency: the two faces of every edge in exactly two faces, edges in hashable order, each pair sorted */
    Ek2 *e = edges_sorted(m.f, m.nt); int ne = 3 * m.nt;
    NxG g; nx_new(&g, m.nt);
    for (int i = 0; i < ne;) {
        int j = i; while (j < ne && e[j].key == e[i].key) j++;
        if (j - i == 2) { int a = e[i].idx / 3, b = e[i + 1].idx / 3; if (a != b) nx_edge(&g, a < b ? a : b, a < b ? b : a); }
        i = j;
    }
    free(e);
    for (int i = 0; i < m.nt; i++) nx_node(&g, i);          /* graph.add_nodes_from(nodes) */
    /* nx.connected_components: for v in G (insertion order), the BFS set of every unseen node; list(set) order */
    unsigned char *seen = (unsigned char *)calloc((size_t)m.nt + 1, 1);
    int np_ = 0, **pf = NULL, *pn = NULL, rc = 0; int *lvl = (int *)malloc(sizeof(int) * (size_t)(m.nt + 1)), *nxt = (int *)malloc(sizeof(int) * (size_t)(m.nt + 1));
    for (int oi = 0; oi < g.n; oi++) {
        int v = g.order[oi]; if (seen[v]) continue;
        ISet s; iset_init(&s); iset_add(&s, v); seen[v] = 1;
        int nl = 0, nn2 = 0; lvl[nl++] = v;
        while (nl) {
            nn2 = 0;
            for (int a = 0; a < nl; a++) for (int k = 0; k < g.nn[lvl[a]]; k++) { int w = g.nb[lvl[a]][k]; if (!seen[w]) { seen[w] = 1; iset_add(&s, w); nxt[nn2++] = w; } }
            memcpy(lvl, nxt, sizeof(int) * (size_t)nn2); nl = nn2;
        }
        pf = (int **)realloc(pf, sizeof(int *) * (size_t)(np_ + 1)); pn = (int *)realloc(pn, sizeof(int) * (size_t)(np_ + 1));
        pf[np_] = (int *)malloc(sizeof(int) * (size_t)s.fill); pn[np_] = 0;
        for (uint64_t k = 0; k <= s.mask; k++) if (s.t[k] >= 0) pf[np_][pn[np_]++] = (int)s.t[k];
        free(s.t); np_++;
    }
    free(seen); free(lvl); free(nxt); nx_free(&g, m.nt);
    if (np_ >= 2) {
        /* submesh(repair=True): each part as its own mesh (faces in that order, the file's normals offered), then
           fill_holes on every part of 4 faces or more; a part that got new faces loses its cached normals */
        int **F = (int **)calloc((size_t)np_, sizeof(int *)), *Fn = (int *)calloc((size_t)np_, sizeof(int)), *pfn = (int *)calloc((size_t)np_, sizeof(int));
        double *vol = (double *)calloc((size_t)np_, sizeof(double)), big = 0;
        for (int q = 0; q < np_; q++) {
            Fn[q] = pn[q]; F[q] = (int *)malloc(sizeof(int) * 3 * (size_t)(pn[q] + 1));
            double *pn3 = (double *)malloc(sizeof(double) * 3 * (size_t)(pn[q] + 1));
            for (int k = 0; k < pn[q]; k++) { memcpy(F[q] + 3 * k, m.f + 3 * pf[q][k], sizeof(int) * 3); memcpy(pn3 + 3 * k, fn + 3 * pf[q][k], 24); }
            if (keep_fn) pfn[q] = normals_ok(m.v, F[q], Fn[q], pn3);
            else pfn[q] = 0;          /* mesh.face_normals computed: the part's check of computed normals passes, computed either way */
            free(pn3);
            if (Fn[q] >= 4) fill_holes(&F[q], &Fn[q], m.nv, m.v);          /* the part keeps its cached normals (pfn); its new faces' are computed */
            MgMesh t; t.v = m.v; t.nv = m.nv; t.f = F[q]; t.nt = Fn[q];
            vol[q] = fabs(mg_volume(&t));
            if (q == 0 || vol[q] > big) big = vol[q];
        }
        int nkeep = 0; for (int q = 0; q < np_; q++) if (vol[q] == big || vol[q] >= max_mm3) nkeep++;
        if (nkeep != np_) {
            /* concatenate(keep).export(path). The normals: one part kept is concatenate's copy() (no cache: computed);
               several: if any part still has cached normals, every part's normals (cached, else computed) stacked and
               offered to the new mesh (its check on the first 20 faces); else computed */
            uint32_t cnt = 0; int anyc = 0;
            for (int q = 0; q < np_; q++) if (vol[q] == big || vol[q] >= max_mm3) { cnt += (uint32_t)Fn[q]; anyc |= pfn[q]; }
            int *af = (int *)malloc(sizeof(int) * 3 * (size_t)(cnt + 1)); double *an = (double *)malloc(sizeof(double) * 3 * (size_t)(cnt + 1)); int at = 0;
            for (int q = 0; q < np_; q++) {
                if (!(vol[q] == big || vol[q] >= max_mm3)) continue;
                for (int k = 0; k < Fn[q]; k++, at++) {
                    memcpy(af + 3 * at, F[q] + 3 * k, sizeof(int) * 3);
                    if (pfn[q] && k < pn[q]) memcpy(an + 3 * at, fn + 3 * pf[q][k], 24);
                    else face_normal(m.v + 3 * af[3 * at], m.v + 3 * af[3 * at + 1], m.v + 3 * af[3 * at + 2], an + 3 * at);
                }
            }
            int use_given = nkeep >= 2 && anyc && normals_ok(m.v, af, (int)cnt, an);
            unsigned char hd[84] = { 0 }; memcpy(hd + 80, &cnt, 4); mg_buf_add(out, hd, 84);
            for (int k = 0; k < (int)cnt; k++) {
                float o[12]; unsigned char rec[50]; double nrm[3];
                const double *a = m.v + 3 * af[3 * k], *b = m.v + 3 * af[3 * k + 1], *c = m.v + 3 * af[3 * k + 2];
                if (use_given) memcpy(nrm, an + 3 * k, 24); else face_normal(a, b, c, nrm);
                for (int z = 0; z < 3; z++) { o[z] = (float)nrm[z]; o[3 + z] = (float)a[z]; o[6 + z] = (float)b[z]; o[9 + z] = (float)c[z]; }
                memcpy(rec, o, 48); rec[48] = rec[49] = 0; mg_buf_add(out, rec, 50);
            }
            free(af); free(an);
            *removed = np_ - nkeep;
        }
        for (int q = 0; q < np_; q++) free(F[q]);
        free(F); free(Fn); free(pfn); free(vol);
    }
    for (int q = 0; q < np_; q++) free(pf[q]);
    free(pf); free(pn); free(fn); mg_mesh_free(&m);
    return rc;
}

/* ======================= run_set.close_seams ======================= */
/* numpy's ddot of two 3-vectors (a @ b, np.linalg.norm of a vector): OpenBLAS's loop with FMA (measured) */
__attribute__((target("fma"))) static double dot3(const double *a, const double *b) { return __builtin_fma(a[2], b[2], __builtin_fma(a[1], b[1], a[0] * b[0])); }
/* (n, 3) @ (3,): OpenBLAS dgemv. Its kernel's order is not known: the FMA loop is the nearest (about 1 row in 5 differs
   in the last bit from numpy) */
__attribute__((target("fma"))) static double gemv3(const double *a, const double *b) { return __builtin_fma(a[2], b[2], __builtin_fma(a[1], b[1], a[0] * b[0])); }
static double norm3_rows(const double *d) { return sqrt((d[0] * d[0] + d[1] * d[1]) + d[2] * d[2]); }          /* np.linalg.norm(axis=...) */
static void cross3(const double *p, const double *q, double *o) { o[0] = p[1] * q[2] - p[2] * q[1]; o[1] = p[2] * q[0] - p[0] * q[2]; o[2] = p[0] * q[1] - p[1] * q[0]; }
typedef struct { double *v; int nv, capv; int *f; int nt, capf; } Sm;          /* a mesh being repaired */
static void sm_faces_push(Sm *m, int a, int b, int c) {
    if (m->nt == m->capf) { m->capf = m->capf ? 2 * m->capf : 64; m->f = (int *)realloc(m->f, sizeof(int) * 3 * (size_t)m->capf); }
    m->f[3 * m->nt] = a; m->f[3 * m->nt + 1] = b; m->f[3 * m->nt + 2] = c; m->nt++;
}
/* run_set._edge_counts(m)[0]: the edges in an odd number of faces */
static int odd_edges(const Sm *m) {
    Ek2 *e = edges_sorted(m->f, m->nt); int ne = 3 * m->nt, odd = 0;
    for (int i = 0; i < ne;) { int j = i; while (j < ne && e[j].key == e[i].key) j++; if ((j - i) % 2 == 1) odd++; i = j; }
    free(e); return odd;
}
/* the edges in exactly 1 face (require_count=1) in hashable order: their (face, corner) positions */
static int boundary_edges(const Sm *m, int **pos) {
    Ek2 *e = edges_sorted(m->f, m->nt); int ne = 3 * m->nt, nb = 0; *pos = (int *)malloc(sizeof(int) * (size_t)(ne + 1));
    for (int i = 0; i < ne;) { int j = i; while (j < ne && e[j].key == e[i].key) j++; if (j - i == 1) (*pos)[nb++] = e[i].idx; i = j; }
    free(e); return nb;
}
/* run_set._drop_slivers */
static void drop_slivers(Sm *m) {
    for (int it = 0; it < 10; it++) {
        Ek2 *e = edges_sorted(m->f, m->nt); int ne = 3 * m->nt; unsigned char *badf = (unsigned char *)calloc((size_t)m->nt + 1, 1); int anybad = 0;
        for (int i = 0; i < ne;) {          /* bad: the edges in exactly 1 or exactly 3 faces; a face with one of them is hit */
            int j = i; while (j < ne && e[j].key == e[i].key) j++;
            if (j - i == 1 || j - i == 3) { anybad = 1; for (int k = i; k < j; k++) badf[e[k].idx / 3] = 1; }
            i = j;
        }
        free(e);
        if (!anybad) { free(badf); return; }
        int nk = 0, w = 0;
        for (int i = 0; i < m->nt; i++) {
            const double *p0 = m->v + 3 * m->f[3 * i], *p1 = m->v + 3 * m->f[3 * i + 1], *p2 = m->v + 3 * m->f[3 * i + 2];
            double d0[3] = { p0[0] - p2[0], p0[1] - p2[1], p0[2] - p2[2] }, d1[3] = { p1[0] - p0[0], p1[1] - p0[1], p1[2] - p0[2] }, d2[3] = { p2[0] - p1[0], p2[1] - p1[1], p2[2] - p1[2] };
            double el = norm3_rows(d0), x = norm3_rows(d1), y = norm3_rows(d2);          /* norm(tri - roll(tri, 1), axis=2).max(axis=1) */
            if (x > el || isnan(x)) el = x;
            if (y > el || isnan(y)) el = y;
            double u[3] = { p1[0] - p0[0], p1[1] - p0[1], p1[2] - p0[2] }, v[3] = { p2[0] - p1[0], p2[1] - p1[1], p2[2] - p1[2] }, c[3];
            cross3(u, v, c);
            double area = sqrt((c[0] * c[0] + c[1] * c[1]) + c[2] * c[2]) / 2.0, h = 2 * area / (el > 1e-12 ? el : 1e-12);
            if (badf[i] && h < 1e-4) { badf[i] = 2; nk++; }
        }
        if (!nk) { free(badf); return; }
        for (int i = 0; i < m->nt; i++) if (badf[i] != 2) { memmove(m->f + 3 * w, m->f + 3 * i, sizeof(int) * 3); w++; }          /* update_faces(~kill) */
        m->nt = w; free(badf);
    }
}
/* Trimesh.remove_unreferenced_vertices: the referenced vertices, in order */
static void remove_unreferenced(Sm *m) {
    int *inv = (int *)malloc(sizeof(int) * (size_t)(m->nv + 1)), n = 0; unsigned char *ref = (unsigned char *)calloc((size_t)m->nv + 1, 1);
    for (int i = 0; i < 3 * m->nt; i++) ref[m->f[i]] = 1;
    for (int i = 0; i < m->nv; i++) if (ref[i]) { inv[i] = n; memmove(m->v + 3 * n, m->v + 3 * i, 24); n++; } else inv[i] = 0;
    for (int i = 0; i < 3 * m->nt; i++) m->f[i] = inv[m->f[i]];
    m->nv = n; free(inv); free(ref);
}
typedef struct { double t; int v; } Tv;
static int tv_cmp(const void *a, const void *b) { const Tv *x = (const Tv *)a, *y = (const Tv *)b; return x->t < y->t ? -1 : x->t > y->t ? 1 : x->v - y->v; }
static int int_cmp(const void *a, const void *b) { int x = *(const int *)a, y = *(const int *)b; return (x > y) - (x < y); }
/* run_set._fix_tjunctions(m, tol=2e-4): 1 = closed, 0 = not */
static int fix_tjunctions(Sm *m, double tol) {
    for (int it = 0; it < 12; it++) {
        int nt = m->nt, ne = 3 * nt;
        /* e = vstack([F[:, [0, 1]], F[:, [1, 2]], F[:, [2, 0]]]): edge k is (corner k / nt of face k % nt) */
        int *ea = (int *)malloc(sizeof(int) * (size_t)(ne + 1)), *eb = (int *)malloc(sizeof(int) * (size_t)(ne + 1));
        for (int c = 0; c < 3; c++) for (int i = 0; i < nt; i++) { ea[c * nt + i] = m->f[3 * i + c]; eb[c * nt + i] = m->f[3 * i + (c + 1) % 3]; }
        Ek2 *s = (Ek2 *)malloc(sizeof(Ek2) * (size_t)(ne + 1));
        for (int k = 0; k < ne; k++) { s[k].key = pair_key(ea[k], eb[k]); s[k].idx = k; }
        qsort(s, (size_t)ne, sizeof(Ek2), ek2_cmp);
        unsigned char *isodd = (unsigned char *)calloc((size_t)ne + 1, 1); int nop = 0;
        for (int i = 0; i < ne;) { int j = i; while (j < ne && s[j].key == s[i].key) j++; if ((j - i) % 2 == 1) for (int k = i; k < j; k++) { isodd[s[k].idx] = 1; nop++; } i = j; }
        free(s);
        if (!nop) { free(ea); free(eb); free(isodd); return 1; }
        int *V = (int *)malloc(sizeof(int) * 2 * (size_t)nop), nV = 0;          /* np.unique(e[op]) */
        for (int k = 0; k < ne; k++) if (isodd[k]) { V[nV++] = ea[k]; V[nV++] = eb[k]; }
        qsort(V, (size_t)nV, sizeof(int), int_cmp);
        int u = 0; for (int k = 0; k < nV; k++) if (!k || V[k] != V[k - 1]) V[u++] = V[k];
        nV = u;
        int *newf = (int *)malloc(sizeof(int) * (size_t)(nt + 1)), nnew = 0; int **nfaces = (int **)calloc((size_t)nt + 1, sizeof(int *)); int *nfn = (int *)calloc((size_t)nt + 1, sizeof(int));
        unsigned char *innew = (unsigned char *)calloc((size_t)nt + 1, 1); int changed = 0;
        Tv *on = (Tv *)malloc(sizeof(Tv) * (size_t)(nV + 1));
        for (int k = 0; k < ne; k++) {
            if (!isodd[k]) continue;
            int f = k % nt; if (innew[f]) continue;
            int a = ea[k], b = eb[k]; const double *Pa = m->v + 3 * a, *Pb = m->v + 3 * b;
            double d[3] = { Pb[0] - Pa[0], Pb[1] - Pa[1], Pb[2] - Pa[2] }, L2 = dot3(d, d);
            if (L2 < 1e-12) continue;
            int non = 0;
            for (int i = 0; i < nV; i++) {
                const double *Q = m->v + 3 * V[i]; double w[3] = { Q[0] - Pa[0], Q[1] - Pa[1], Q[2] - Pa[2] }, t = gemv3(w, d) / L2;
                double r[3] = { Q[0] - (Pa[0] + t * d[0]), Q[1] - (Pa[1] + t * d[1]), Q[2] - (Pa[2] + t * d[2]) }, dist = norm3_rows(r);
                if (t > 1e-6 && t < 1 - 1e-6 && dist < tol && V[i] != a && V[i] != b) { on[non].t = t; on[non].v = V[i]; non++; }
            }
            if (!non) continue;
            int x = -1, nx_ = 0; for (int c = 0; c < 3; c++) { int vv = m->f[3 * f + c]; if (vv != a && vv != b) { x = vv; nx_++; } }
            if (nx_ != 1) continue;
            qsort(on, (size_t)non, sizeof(Tv), tv_cmp);
            int *ch = (int *)malloc(sizeof(int) * (size_t)(non + 2)), nc = 0; ch[nc++] = a; for (int i = 0; i < non; i++) ch[nc++] = on[i].v; ch[nc++] = b;
            nfaces[nnew] = (int *)malloc(sizeof(int) * 3 * (size_t)(nc - 1)); nfn[nnew] = nc - 1;
            for (int i = 0; i < nc - 1; i++) { nfaces[nnew][3 * i] = ch[i]; nfaces[nnew][3 * i + 1] = ch[i + 1]; nfaces[nnew][3 * i + 2] = x; }
            free(ch); newf[nnew++] = f; innew[f] = 1; changed = 1;
        }
        free(on); free(ea); free(eb); free(isodd); free(V);
        if (!changed) { free(newf); free(nfaces); free(nfn); free(innew); return 0; }
        int *F = (int *)malloc(sizeof(int) * 3 * (size_t)(nt + 1)), w = 0; memcpy(F, m->f, sizeof(int) * 3 * (size_t)nt);
        for (int i = 0; i < nt; i++) if (!innew[i]) { memcpy(m->f + 3 * w, F + 3 * i, sizeof(int) * 3); w++; }
        m->nt = w; free(F);
        for (int q = 0; q < nnew; q++) { for (int i = 0; i < nfn[q]; i++) sm_faces_push(m, nfaces[q][3 * i], nfaces[q][3 * i + 1], nfaces[q][3 * i + 2]); free(nfaces[q]); }
        free(newf); free(nfaces); free(nfn); free(innew);
    }
    return odd_edges(m) == 0;
}
/* the boundary edges as Python's dict-of-lists: out.setdefault(a, []).append(b), keys in first-seen order */
typedef struct { int *key, *pos; int nk; int **lst, *ln, *cap; } Out;
static void out_build(Out *o, const Sm *m, const int *bpos, int nb) {
    o->key = (int *)malloc(sizeof(int) * (size_t)(nb + 1)); o->pos = (int *)malloc(sizeof(int) * (size_t)(m->nv + 1)); o->nk = 0;
    o->lst = (int **)calloc((size_t)nb + 1, sizeof(int *)); o->ln = (int *)calloc((size_t)nb + 1, sizeof(int)); o->cap = (int *)calloc((size_t)nb + 1, sizeof(int));
    for (int i = 0; i < m->nv; i++) o->pos[i] = -1;
    for (int k = 0; k < nb; k++) {
        int fi = bpos[k] / 3, c = bpos[k] % 3, a = m->f[3 * fi + c], b = m->f[3 * fi + (c + 1) % 3];
        if (o->pos[a] < 0) { o->pos[a] = o->nk; o->key[o->nk++] = a; }
        int p = o->pos[a]; if (o->ln[p] == o->cap[p]) { o->cap[p] = o->cap[p] ? 2 * o->cap[p] : 2; o->lst[p] = (int *)realloc(o->lst[p], sizeof(int) * (size_t)o->cap[p]); }
        o->lst[p][o->ln[p]++] = b;
    }
}
static void out_free(Out *o) { for (int i = 0; i < o->nk; i++) free(o->lst[i]); free(o->lst); free(o->ln); free(o->cap); free(o->key); free(o->pos); }
static int out_has(const Out *o, int a) { return o->pos[a] >= 0 && o->ln[o->pos[a]] > 0; }          /* out.get(a) (a non-empty list) */
static int out_pop(Out *o, int a) { int p = o->pos[a]; return o->lst[p][--o->ln[p]]; }
static double cr2(const double *o, const double *a, const double *b) { return (a[0] - o[0]) * (b[1] - o[1]) - (a[1] - o[1]) * (b[0] - o[0]); }
/* run_set._fill_planar_holes(m, tol=2e-3, max_n=400) */
static void fill_planar_holes(Sm *m, double tol, int max_n) {
    int *bpos, nb = boundary_edges(m, &bpos); Out o; out_build(&o, m, bpos, nb); free(bpos);
    int nt0 = m->nt; int *add = NULL, nadd = 0, capadd = 0;
#define ADDF(a, b, c) do { if (nadd == capadd) { capadd = capadd ? 2 * capadd : 64; add = (int *)realloc(add, sizeof(int) * 3 * (size_t)capadd); } add[3 * nadd] = (a); add[3 * nadd + 1] = (b); add[3 * nadd + 2] = (c); nadd++; } while (0)
    int *loop = (int *)malloc(sizeof(int) * (size_t)(max_n + 3));
    for (int si = 0; si < o.nk; si++) {
        int start = o.key[si];
        while (out_has(&o, start)) {
            int nl = 0; loop[nl++] = start; int cur = out_pop(&o, start);
            while (cur != start && nl <= max_n && out_has(&o, cur)) { loop[nl++] = cur; cur = out_pop(&o, cur); }
            if (cur != start || nl < 3) continue;
            int n = nl, *ring = (int *)malloc(sizeof(int) * (size_t)n); for (int i = 0; i < n; i++) ring[i] = loop[n - 1 - i];
            double nrm[3] = { 0, 0, 0 };
            for (int i = 0; i < n; i++) { double c[3]; cross3(m->v + 3 * ring[i], m->v + 3 * ring[(i + 1) % n], c); nrm[0] += c[0]; nrm[1] += c[1]; nrm[2] += c[2]; }
            double L = sqrt(dot3(nrm, nrm));
            if (L < 1e-9) { free(ring); continue; }
            nrm[0] /= L; nrm[1] /= L; nrm[2] /= L;
            double mean[3] = { 0, 0, 0 };
            for (int i = 0; i < n; i++) for (int c = 0; c < 3; c++) mean[c] = i ? mean[c] + m->v[3 * ring[i] + c] : m->v[3 * ring[i] + c];
            for (int c = 0; c < 3; c++) mean[c] /= n;
            double worst = 0; int flat = 1;
            for (int i = 0; i < n; i++) { const double *p = m->v + 3 * ring[i]; double d[3] = { p[0] - mean[0], p[1] - mean[1], p[2] - mean[2] }, x = fabs(gemv3(d, nrm)); if (x > worst || isnan(x)) worst = x; }
            if (worst > tol) flat = 0;
            if (!flat) { free(ring); continue; }
            double e0[3] = { 1, 0, 0 }, e1[3] = { 0, 1, 0 }, ax[3], ay[3];
            cross3(nrm, fabs(nrm[0]) < 0.9 ? e0 : e1, ax); double la = sqrt(dot3(ax, ax)); ax[0] /= la; ax[1] /= la; ax[2] /= la; cross3(nrm, ax, ay);
            double *xy = (double *)malloc(sizeof(double) * 2 * (size_t)n); int *idx = (int *)malloc(sizeof(int) * (size_t)n), ni = n;
            for (int i = 0; i < n; i++) { xy[2 * i] = gemv3(m->v + 3 * ring[i], ax); xy[2 * i + 1] = gemv3(m->v + 3 * ring[i], ay); idx[i] = i; }
            int guard = 0;
            while (ni > 3 && guard < 5000) {
                guard++; int done = 0;
                for (int k = 0; k < ni; k++) {
                    int i0 = idx[(k - 1 + ni) % ni], i1 = idx[k], i2 = idx[(k + 1) % ni];
                    if (cr2(xy + 2 * i0, xy + 2 * i1, xy + 2 * i2) <= 1e-12) continue;
                    int inside = 0;
                    for (int q = 0; q < ni && !inside; q++) { int j = idx[q]; if (j != i0 && j != i1 && j != i2 && cr2(xy + 2 * i0, xy + 2 * i1, xy + 2 * j) > 0 && cr2(xy + 2 * i1, xy + 2 * i2, xy + 2 * j) > 0 && cr2(xy + 2 * i2, xy + 2 * i0, xy + 2 * j) > 0) inside = 1; }
                    if (inside) continue;
                    ADDF(ring[i0], ring[i1], ring[i2]); memmove(idx + k, idx + k + 1, sizeof(int) * (size_t)(ni - k - 1)); ni--; done = 1; break;
                }
                if (!done) {          /* degenerate (a collinear run): the flattest corner goes (min: the first of equal) */
                    int best = 0; double bv = 0;
                    for (int k = 0; k < ni; k++) { double v = fabs(cr2(xy + 2 * idx[(k - 1 + ni) % ni], xy + 2 * idx[k], xy + 2 * idx[(k + 1) % ni])); if (k == 0 || v < bv) { bv = v; best = k; } }
                    ADDF(ring[idx[(best - 1 + ni) % ni]], ring[idx[best]], ring[idx[(best + 1) % ni]]); memmove(idx + best, idx + best + 1, sizeof(int) * (size_t)(ni - best - 1)); ni--;
                }
            }
            if (ni == 3) ADDF(ring[idx[0]], ring[idx[1]], ring[idx[2]]);
            free(xy); free(idx); free(ring);
        }
    }
#undef ADDF
    (void)nt0;
    for (int k = 0; k < nadd; k++) sm_faces_push(m, add[3 * k], add[3 * k + 1], add[3 * k + 2]);
    free(add); free(loop); out_free(&o);
}
/* run_set._fill_small_loops(m, max_n=8, max_len=4.0) */
static void fill_small_loops(Sm *m, int max_n, double max_len) {
    int *bpos, nb = boundary_edges(m, &bpos);
    int *nx = (int *)malloc(sizeof(int) * (size_t)(m->nv + 1)), *keys = (int *)malloc(sizeof(int) * (size_t)(nb + 1)), nk = 0;
    for (int i = 0; i < m->nv; i++) nx[i] = -1;
    for (int k = 0; k < nb; k++) {          /* nxt = {a: b}: a later edge from a replaces the value, the key keeps its place */
        int fi = bpos[k] / 3, c = bpos[k] % 3, a = m->f[3 * fi + c], b = m->f[3 * fi + (c + 1) % 3];
        if (nx[a] < 0) keys[nk++] = a;
        nx[a] = b;
    }
    free(bpos);
    unsigned char *seen = (unsigned char *)calloc((size_t)m->nv + 1, 1); int *loop = (int *)malloc(sizeof(int) * (size_t)(max_n + 3));
    int *add = NULL, nadd = 0;
    for (int ki = 0; ki < nk; ki++) {
        int u = keys[ki]; if (seen[u]) continue;
        int nl = 0; loop[nl++] = u;
        while (nx[loop[nl - 1]] >= 0 && nx[loop[nl - 1]] != u && nl <= max_n) { loop[nl] = nx[loop[nl - 1]]; nl++; }
        for (int i = 0; i < nl; i++) seen[loop[i]] = 1;
        if (nx[loop[nl - 1]] != u || nl < 3 || nl > max_n) continue;
        double per = 0;          /* sum(np.linalg.norm(...)): np.float64 values added one by one from 0 */
        for (int i = 0; i < nl; i++) { const double *p = m->v + 3 * loop[i], *q = m->v + 3 * loop[(i + 1) % nl]; double d[3] = { p[0] - q[0], p[1] - q[1], p[2] - q[2] }; per = per + sqrt(dot3(d, d)); }
        if (per > max_len) continue;
        add = (int *)realloc(add, sizeof(int) * 3 * (size_t)(nadd + nl));
        for (int i = 1; i < nl - 1; i++) { int r0 = loop[nl - 1], ri = loop[nl - 1 - i], rj = loop[nl - 2 - i]; add[3 * nadd] = r0; add[3 * nadd + 1] = ri; add[3 * nadd + 2] = rj; nadd++; }
    }
    for (int k = 0; k < nadd; k++) sm_faces_push(m, add[3 * k], add[3 * k + 1], add[3 * k + 2]);
    free(add); free(nx); free(keys); free(seen); free(loop);
}
int mg_close_seams(const unsigned char *stl, size_t n, MgBuf *out, int *repaired) {
    *repaired = 0;
    if (n < 84) return fail("HeaderError: Binary STL shorter than a fixed header!");
    uint32_t nt; memcpy(&nt, stl + 80, 4);
    if (84 + 50 * (size_t)nt != n) return fail("HeaderError: Binary STL has incorrect length in header");
    MgMesh mm; memset(&mm, 0, sizeof mm); mm.nt = (int)nt; mm.nv = 3 * (int)nt;
    mm.v = (double *)malloc(sizeof(double) * 9 * (size_t)(nt + 1)); mm.f = (int *)malloc(sizeof(int) * 3 * (size_t)(nt + 1));
    for (uint32_t i = 0; i < nt; i++) {
        float c[12]; memcpy(c, stl + 84 + 50 * (size_t)i, 48);
        for (int k = 0; k < 9; k++) mm.v[9 * (size_t)i + k] = c[3 + k];
        for (int k = 0; k < 3; k++) mm.f[3 * i + k] = 3 * (int)i + k;
    }
    mg_merge_vertices(&mm); mg_merge_vertices(&mm);          /* trimesh.load (process) and m.merge_vertices() */
    Sm m; m.v = mm.v; m.nv = m.capv = mm.nv; m.f = mm.f; m.nt = m.capf = mm.nt;
    if (odd_edges(&m) == 0) { free(m.v); free(m.f); return 0; }
    drop_slivers(&m); remove_unreferenced(&m);
    if (odd_edges(&m)) { fix_tjunctions(&m, 2e-4); drop_slivers(&m); }
    if (odd_edges(&m)) fix_tjunctions(&m, 2e-4);
    if (odd_edges(&m)) fill_planar_holes(&m, 2e-3, 400);
    if (odd_edges(&m)) fill_small_loops(&m, 8, 4.0);
    if (odd_edges(&m)) { free(m.v); free(m.f); return 0; }
    /* Trimesh(vertices, faces, process=False).export(path): the normals computed */
    unsigned char hd[84] = { 0 }; uint32_t cnt = (uint32_t)m.nt; memcpy(hd + 80, &cnt, 4); mg_buf_add(out, hd, 84);
    for (int k = 0; k < m.nt; k++) {
        float o[12]; unsigned char rec[50]; double nrm[3];
        const double *a = m.v + 3 * m.f[3 * k], *b = m.v + 3 * m.f[3 * k + 1], *c = m.v + 3 * m.f[3 * k + 2];
        face_normal(a, b, c, nrm);
        for (int z = 0; z < 3; z++) { o[z] = (float)nrm[z]; o[3 + z] = (float)a[z]; o[6 + z] = (float)b[z]; o[9 + z] = (float)c[z]; }
        memcpy(rec, o, 48); rec[48] = rec[49] = 0; mg_buf_add(out, rec, 50);
    }
    free(m.v); free(m.f); *repaired = 1;
    return 0;
}
