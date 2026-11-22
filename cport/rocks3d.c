/* rocks3d.c: organic rocks, the shells (see rocks3d.h). */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "shp.h"
#include "rocks3d.h"

int ork_on = 0;
double kit_strength = 1.0;          /* style strength (settings "style_strength", --strength; pipeline.c sets it): 1 = the standard look */

/* ---------------- the kinds ---------------- */
static const char *const ORK_KINDS0[] = { "rock", "pebble", "pebbles", "rubble", "sandstone", "slate", "stone", "stones", "boulder", "cobble",
                                          "fleshrock", "blightrock", "ashstone", NULL };
static char **ORK_K; static int ORK_NK = -1;
void ork_set_kinds(const char *const *kinds, int n) {
    for (int i = 0; i < ORK_NK; i++) free(ORK_K[i]);
    free(ORK_K); ORK_K = NULL; ORK_NK = -1;
    if (n < 0) return;
    ORK_K = (char **)malloc(sizeof(char *) * (size_t)(n ? n : 1)); ORK_NK = n;
    for (int i = 0; i < n; i++) ORK_K[i] = _strdup(kinds[i]);
}
int ork_kind(const char *k) {
    char b[128]; size_t L;
    if (!k || !strncmp(k, "gb_", 3)) return 0;          /* the ground's own big shapes stay terrain */
    snprintf(b, sizeof b, "%s", k); L = strlen(b);
    if (L >= 2 && (!strcmp(b + L - 2, "_p") || !strcmp(b + L - 2, "_s"))) b[L - 2] = 0;          /* a kind's piece / side variants */
    if (ORK_NK >= 0) { for (int i = 0; i < ORK_NK; i++) if (!strcmp(ORK_K[i], b)) return 1; return 0; }
    for (int i = 0; ORK_KINDS0[i]; i++) if (!strcmp(ORK_KINDS0[i], b)) return 1;
    return 0;
}

/* ---------------- the pieces build_cmds hands over ---------------- */
typedef struct { GEOSGeometry *p; double z0, top, area, bb[4]; } OrkPiece;
static OrkPiece *PC; static int NPC, CPC;
void ork_clear(void) { for (int i = 0; i < NPC; i++) shp_free(PC[i].p); NPC = 0; }
void ork_take(double z0, double top, GEOSGeometry *const *polys, int np) {
    for (int i = 0; i < np; i++) {
        ShpList l = { 0 };
        if (!polys[i]) continue;
        shp_polys_of(polys[i], &l);
        for (int k = 0; k < l.n; k++) {
            double a = shp_area(l.g[k]);
            if (a < 0.03) continue;
            if (NPC == CPC) { CPC = CPC ? 2 * CPC : 64; PC = (OrkPiece *)realloc(PC, sizeof(OrkPiece) * (size_t)CPC); }
            PC[NPC].p = l.g[k]; l.g[k] = NULL; PC[NPC].z0 = z0; PC[NPC].top = top; PC[NPC].area = a; shp_bounds(PC[NPC].p, PC[NPC].bb); NPC++;
        }
        shp_list_free(&l);
    }
}

/* ---------------- the surface ---------------- */
static uint32_t ork_h(int x, int y, uint32_t s) {
    uint32_t h = (uint32_t)x * 374761393u + (uint32_t)y * 668265263u + s * 2246822519u;
    h = (h ^ (h >> 13)) * 1274126177u; return h ^ (h >> 16);
}
static double ork_lat(int x, int y, uint32_t s) { return (double)(ork_h(x, y, s) & 0xffffff) / 8388607.5 - 1.0; }
static double ork_noise(double x, double y, uint32_t s) {          /* value noise, smooth, in -1..1 */
    int xi = (int)floor(x), yi = (int)floor(y); double fx = x - xi, fy = y - yi, a, b, c, d;
    fx = fx * fx * (3 - 2 * fx); fy = fy * fy * (3 - 2 * fy);
    a = ork_lat(xi, yi, s); b = ork_lat(xi + 1, yi, s); c = ork_lat(xi, yi + 1, s); d = ork_lat(xi + 1, yi + 1, s);
    return (a + (b - a) * fx) + ((c + (d - c) * fx) - (a + (b - a) * fx)) * fy;
}
/* a polygon's rings as segments, for the distance inside it */
typedef struct { double *s; int n; } OrkSeg;          /* n segments: x0 y0 x1 y1 */
static void ork_segs(const GEOSGeometry *p, OrkSeg *o) {
    int cap = 4096, nh = shp_num_interiors(p); double *xy = (double *)malloc(sizeof(double) * 2 * (size_t)cap);
    o->s = NULL; o->n = 0;
    for (int r = -1; r < nh; r++) {
        const GEOSGeometry *ring = r < 0 ? shp_exterior(p) : shp_interior(p, r); int n;
        while ((n = shp_coords(ring, xy, cap)) < 0 && cap < (1 << 22)) { cap *= 4; xy = (double *)realloc(xy, sizeof(double) * 2 * (size_t)cap); }
        if (n < 2) continue;
        o->s = (double *)realloc(o->s, sizeof(double) * 4 * (size_t)(o->n + n - 1));
        for (int k = 0; k + 1 < n; k++) { double *q = o->s + 4 * (o->n + k); q[0] = xy[2 * k]; q[1] = xy[2 * k + 1]; q[2] = xy[2 * k + 2]; q[3] = xy[2 * k + 3]; }
        o->n += n - 1;
    }
    free(xy);
}
/* the distance from (x, y) to the polygon's edge: + inside it (even-odd over all its rings), - outside */
static double ork_din(const OrkSeg *o, double x, double y) {
    int in = 0; double d2 = 1e300;
    for (int k = 0; k < o->n; k++) {
        const double *q = o->s + 4 * k; double ex = q[2] - q[0], ey = q[3] - q[1], ll = ex * ex + ey * ey, t, dx, dy;
        if ((q[1] > y) != (q[3] > y) && x < q[0] + (y - q[1]) * ex / ey) in = !in;
        t = ll > 0 ? ((x - q[0]) * ex + (y - q[1]) * ey) / ll : 0; if (t < 0) t = 0; if (t > 1) t = 1;
        dx = q[0] + t * ex - x; dy = q[1] + t * ey - y; if (dx * dx + dy * dy < d2) d2 = dx * dx + dy * dy;
    }
    return in ? sqrt(d2) : -sqrt(d2);
}

static int *UF;
static int uf_f(int x) { while (UF[x] != x) { UF[x] = UF[UF[x]]; x = UF[x]; } return x; }
static int cmp_area(const void *a, const void *b) { const OrkPiece *p = (const OrkPiece *)a, *q = (const OrkPiece *)b; return p->area > q->area ? -1 : p->area < q->area; }

/* one rock: pieces idx[0..n) (biggest first) */
static int ork_one(const int *idx, int n, const SmShellXf *xf, double Hdeck, const GEOSGeometry *rim, MgMesh *out, char *err, int errcap) {
    double M = xf->on ? xf->f : 1.0, bx = xf->on ? xf->tx : 0, by = xf->on ? xf->ty : 0, fb[4], g, x0, y0, z0min = 1e30, tmax = -1e30, zg, V, *Z = NULL, *D = NULL;
    int nx, ny, rc = -1, *par = NULL; GEOSGeometry *foot = NULL; OrkSeg *sg = NULL; double *w = NULL, *inr = NULL, *dt = NULL;
    const GEOSGeometry **gs = (const GEOSGeometry **)malloc(sizeof(GEOSGeometry *) * (size_t)n);
    memset(out, 0, sizeof *out); err[0] = 0;
    for (int i = 0; i < n; i++) {          /* the corners rounded (a stone's outline is a few straight cuts: worn round) */
        OrkPiece *p = &PC[idx[i]]; double rr = 0.3 / M, inr0 = 2 * p->area / (shp_length(p->p) > 0 ? shp_length(p->p) : 1);
        if (rr > 0.4 * inr0) rr = 0.4 * inr0;
        if (rr > 0.01) {
            GEOSGeometry *a = shp_buffer_q(p->p, -rr, 8), *b = a ? shp_buffer_q(a, rr, 8) : NULL; shp_free(a);
            if (b && !shp_is_empty(b) && shp_area(b) > 0.5 * p->area) { ShpList l = { 0 }; shp_polys_of(b, &l); if (l.n == 1) { shp_free(p->p); p->p = l.g[0]; l.g[0] = NULL; p->area = shp_area(p->p); shp_bounds(p->p, p->bb); } shp_list_free(&l); }
            shp_free(b);
        }
    }
    for (int i = 0; i < n; i++) { const OrkPiece *p = &PC[idx[i]]; gs[i] = p->p; if (p->z0 < z0min) z0min = p->z0; if (p->top > tmax) tmax = p->top; }
    zg = z0min + 0.15 > Hdeck ? z0min + 0.15 : Hdeck;          /* where it meets the ground (build_cmds' prism_groups: max(z0 + 0.15, H)) */
    V = tmax - zg;
    if (V < 0.05) { free(gs); return 1; }          /* flat: nothing to shape (left out: a stone this flat is a mark on the ground) */
    foot = shp_union_all(gs, n); free(gs);
    if (!foot) { snprintf(err, (size_t)errcap, "GEOS: the outline"); return -1; }
    shp_bounds(foot, fb);
    g = 0.05 / M;
    { double ww = fb[2] - fb[0], hh = fb[3] - fb[1]; while ((ww / g + 5) * (hh / g + 5) > 250000.0) g *= 1.25; nx = (int)ceil(ww / g) + 5; ny = (int)ceil(hh / g) + 5; x0 = fb[0] - 2 * g; y0 = fb[1] - 2 * g; }
    /* each piece's parent: the lowest-topped... the highest top below its own among the pieces it stands in */
    par = (int *)malloc(sizeof(int) * (size_t)n); dt = (double *)malloc(sizeof(double) * (size_t)n);
    for (int i = 0; i < n; i++) {
        const OrkPiece *p = &PC[idx[i]]; double best = -1e30; par[i] = -1;
        for (int j = 0; j < n; j++) {
            const OrkPiece *q = &PC[idx[j]]; GEOSGeometry *x; double a;
            if (j == i || !(q->top < p->top - 1e-6) || q->top <= best) continue;
            if (q->bb[0] > p->bb[2] || q->bb[2] < p->bb[0] || q->bb[1] > p->bb[3] || q->bb[3] < p->bb[1]) continue;
            x = shp_intersection(p->p, q->p); a = x ? shp_area(x) : 0; shp_free(x);
            if (a > 0.5 * p->area) { best = q->top; par[i] = j; }
        }
        dt[i] = p->top - (par[i] >= 0 ? PC[idx[par[i]]].top : zg);
        if (dt[i] < 0) dt[i] = 0;
    }
    /* the distances inside each piece, its widest inside distance and its blend width */
    sg = (OrkSeg *)calloc((size_t)n, sizeof(OrkSeg)); D = (double *)malloc(sizeof(double) * (size_t)n * (size_t)nx * (size_t)ny);
    w = (double *)malloc(sizeof(double) * (size_t)n); inr = (double *)calloc((size_t)n, sizeof(double));
    for (int i = 0; i < n; i++) {
        const double *pb = PC[idx[i]].bb;
        ork_segs(PC[idx[i]].p, &sg[i]);
        for (int j = 0; j < ny; j++) for (int k = 0; k < nx; k++) {
            double x = x0 + k * g, y = y0 + j * g, m3 = 3.5 * g, d = x < pb[0] - m3 || x > pb[2] + m3 || y < pb[1] - m3 || y > pb[3] + m3 ? -1e9 : ork_din(&sg[i], x, y);
            D[((size_t)i * ny + j) * nx + k] = d; if (d > inr[i]) inr[i] = d;
        }
        w[i] = (par[i] < 0 ? 1.1 : 1.3) * dt[i];
        if (w[i] < 0.12) w[i] = 0.12;
        if (w[i] > 0.95 * inr[i]) w[i] = 0.95 * inr[i];
        if (w[i] < 1e-3) w[i] = 1e-3;
    }
    Z = (double *)malloc(sizeof(double) * (size_t)nx * (size_t)ny);
    {
        double cx = (fb[0] + fb[2]) / 2, cy = (fb[1] + fb[3]) / 2, fr = M / 0.9, hmax = 0;          /* lumps about 0.9 mm across on the finished base */
        uint32_t seed = (uint32_t)(int64_t)floor(cx * 1000.0) * 2654435761u ^ (uint32_t)(int64_t)floor(cy * 1000.0) * 40503u;
        for (int j = 0; j < ny; j++) for (int k = 0; k < nx; k++) {
            size_t q = (size_t)j * nx + k; double h = 0, base = 0; int any = 0;
            /* just outside a rock's edge the foot's surface goes on (its slope carried out), so the straight cut at the
               outline meets a smooth surface: no saw teeth along the top of the low side */
            double hb = -1e30;
            for (int i = 0; i < n; i++) {
                double d = D[((size_t)i * ny + j) * nx + k], s, r;
                if (par[i] < 0) {
                    if (d < -3.2 * g) continue;
                    any = 1; s = d / w[i]; if (s > 1) s = 1;
                    r = s >= 0 ? 0.2 + 0.8 * (1 - (1 - s) * (1 - s)) : 0.2 + 1.6 * s;          /* the foot: a low straight side, then rounding over */
                    if (s > base) base = s;
                    if (dt[i] * r > hb) hb = dt[i] * r;          /* side by side feet: the highest */
                    continue;
                }
                if (d <= 0) continue;
                s = d / w[i]; if (s > 1) s = 1;
                h += dt[i] * s * s * (3 - 2 * s);          /* a tier above: blended in */
            }
            if (any) h += hb;
            if (!any) { Z[q] = -1e30; continue; }
            {   /* the lumps: none at the foot, so it stays sitting on the ground */
                double x = x0 + k * g, y = y0 + j * g, nn = ork_noise(x * fr, y * fr, seed) + 0.45 * ork_noise(x * fr * 2.3 + 17.3, y * fr * 2.3 - 5.1, seed ^ 0x9e3779b9u);
                h += 0.13 * kit_strength * V * nn * sqrt(base);
                if (h < -0.5 * V) h = -0.5 * V;
            }
            Z[q] = h; if (h > hmax) hmax = h;
        }
        for (size_t q = 0; q < (size_t)nx * (size_t)ny; q++) if (Z[q] > -1e29) Z[q] = zg + Z[q] * (hmax > 0 ? V / hmax : 1.0);          /* no higher than the pieces were */
        {   /* plan B step 4 (2026-11-21): each rock one of four shapes, with chips and wear. 0 rounded, 1 chipped (broken
               flat faces), 2 layered (soft ledges, like sandstone), 3 split (a crack across it). All of them get a few
               chips off their sides and fine pitting; nothing goes below its foot, so it still sits on the ground. */
            uint32_t sh = seed * 2246822519u ^ 0x27d4eb2fu; int shape;
            double rw = fmax(fb[2] - fb[0], fb[3] - fb[1]), zf = zg + 0.18 * V;
            sh ^= sh >> 15; sh *= 0x85ebca6bu; sh ^= sh >> 13; shape = (int)(sh % 4u);
#define ORND() (sh = sh * 1664525u + 1013904223u, (double)(sh >> 8) / 16777216.0)
            int nch = shape == 1 ? 3 + (int)(ORND() * 3) : 1 + (int)(ORND() * 2);
            double ca[6], cr[6], ch[6], ck[6], la = ORND() * 6.2832, per = V / (3.0 + floor(ORND() * 3)), sa = ORND() * 3.1416, wc = fmax(0.12 / M, 0.04 * rw);
            for (int c = 0; c < nch; c++) { ca[c] = ORND() * 6.2832; cr[c] = (0.12 + 0.28 * ORND()) * rw; ch[c] = 1.0 - (0.45 - 0.35 * ORND()) * fmin(1.6, kit_strength); ck[c] = (1.0 + 1.5 * ORND()) * kit_strength; }
            (void)la;
            for (int j = 0; j < ny; j++) for (int k = 0; k < nx; k++) {
                size_t q = (size_t)j * nx + k; double x = x0 + k * g - cx, y = y0 + j * g - cy, z = Z[q], t;
                if (z < -1e29 || z <= zf) continue;
                for (int c = 0; c < nch; c++) {          /* a chip: the surface past a line cut down to a tilted plane */
                    double al = x * cos(ca[c]) + y * sin(ca[c]), pz = zg + V * ch[c] - V * ck[c] * (al - cr[c]) / (0.5 * rw);
                    if (pz < z) z = pz;
                }
                if (shape == 2) {          /* layers: flat ledges with short steps between */
                    double u = (z - zg) / per, fl = floor(u), fr2 = u - fl, s2 = fr2 < 0.7 ? 0 : (fr2 - 0.7) / 0.3;
                    { double lw = fmin(0.9, 0.6 * kit_strength); z = zg + per * (fl + s2 * s2 * (3 - 2 * s2)) * lw + (z - zg) * (1 - lw); }
                }
                if (shape == 3) {          /* split: a crack through the middle */
                    double d = fabs(-x * sin(sa) + y * cos(sa)) + 0.3 * wc * ork_noise(x * fr * 3, y * fr * 3, seed ^ 0x51ed27u);
                    if (d < wc) { double e = 1 - d / wc; z -= fmin(0.7, 0.35 * kit_strength) * V * e * e; }
                }
                t = ork_noise(x * fr * 5.1 - 3.3, y * fr * 5.1 + 8.8, seed ^ 0xa5a5a5u);          /* wear: fine pits */
                if (t > 0.35) z -= 0.05 * kit_strength * V * (t - 0.35) / 0.65;
                if (z < zf) z = zf;
                Z[q] = z;
            }
#undef ORND
        }
    }
    {
        ManifoldManifold *m = sm_grid_cut(Z, nx, ny, x0, y0, g, (z0min < Hdeck ? z0min : Hdeck) - 0.3, foot, M, bx, by, xf, rim, err, errcap);          /* down below the deck: a rock on a ledge has no air under it */
        if (!m) { rc = err[0] ? -1 : 1; goto done; }
        ManifoldMeshGL64 *o = manifold_get_meshgl64(manifold_alloc_meshgl64(), m); manifold_delete_manifold(m);
        size_t ov = manifold_meshgl64_num_vert(o), ot = manifold_meshgl64_num_tri(o), np = manifold_meshgl64_num_prop(o);
        if (!ot) { manifold_delete_meshgl64(o); rc = 1; goto done; }
        double *pv = manifold_meshgl64_vert_properties(malloc(sizeof(double) * ov * np), o);
        uint64_t *tv = manifold_meshgl64_tri_verts(malloc(sizeof(uint64_t) * 3 * ot), o);
        manifold_delete_meshgl64(o);
        out->nv = (int)ov; out->nt = (int)ot; out->v = (double *)malloc(sizeof(double) * 3 * ov); out->f = (int *)malloc(sizeof(int) * 3 * ot);
        for (size_t i = 0; i < ov; i++) memcpy(out->v + 3 * i, pv + np * i, 24);
        for (size_t i = 0; i < 3 * ot; i++) out->f[i] = (int)tv[i];
        free(pv); free(tv);
    }
    rc = 0;
done:
    for (int i = 0; sg && i < n; i++) free(sg[i].s);
    free(sg); free(D); free(w); free(inr); free(Z); free(par); free(dt); shp_free(foot);
    return rc;
}

void ork_build(const SmShellXf *xf, double H, int has_oval, double A, double B, double rlim, S3Shells *out, MgStrs *skipped, int *nrocks) {
    GEOSGeometry *rim; int *idx;
    *nrocks = 0;
    if (!NPC) return;
    { GEOSGeometry *d = shp_point_buffer(0, 0, has_oval ? 1.0 : rlim, 32); rim = has_oval && d ? shp_scale(d, A / 2.0 - 0.97, B / 2.0 - 0.97, SHP_ORIGIN_POINT, 0, 0) : d; if (has_oval) shp_free(d); }
    qsort(PC, (size_t)NPC, sizeof(OrkPiece), cmp_area);
    /* the rocks: a piece joins the one it mostly stands in, when it is higher (a tier, a heap's band, a lump on top) */
    UF = (int *)malloc(sizeof(int) * (size_t)NPC); for (int i = 0; i < NPC; i++) UF[i] = i;
    for (int i = 0; i < NPC; i++) for (int j = i + 1; j < NPC; j++) {
        const OrkPiece *a = &PC[i], *b = &PC[j]; GEOSGeometry *x; double ar;          /* b is the smaller */
        if (a->bb[0] > b->bb[2] || a->bb[2] < b->bb[0] || a->bb[1] > b->bb[3] || a->bb[3] < b->bb[1]) continue;
        if (fabs(a->top - b->top) < 1e-6) continue;
        x = shp_intersection(a->p, b->p); ar = x ? shp_area(x) : 0; shp_free(x);
        if (ar > 0.5 * b->area) { int ra = uf_f(i), rb = uf_f(j); if (ra != rb) UF[rb] = ra; }
    }
    idx = (int *)malloc(sizeof(int) * (size_t)NPC);
    for (int r = 0; r < NPC; r++) {
        int n = 0; MgMesh m; char err[200], line[300]; int rc;
        if (uf_f(r) != r) continue;
        for (int i = 0; i < NPC; i++) if (uf_f(i) == r) idx[n++] = i;
        rc = ork_one(idx, n, xf, H, rim, &m, err, sizeof err);
        if (rc == 0) {
            if (out->n == out->cap) { out->cap = out->cap ? 2 * out->cap : 8; out->v = (MgMesh *)realloc(out->v, sizeof(MgMesh) * (size_t)out->cap); }
            out->v[out->n++] = m; (*nrocks)++;
        } else if (rc < 0) {
            snprintf(line, sizeof line, "rock: %s", err); mg_strs_push(skipped, line);
            mg_log_caught("organic rock dropped", "RuntimeError", err, "cport/rocks3d.c");
        }
    }
    free(idx); free(UF); UF = NULL; shp_free(rim);
}
