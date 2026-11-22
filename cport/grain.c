/* grain.c: see grain.h. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "grain.h"

#define FADE 0.6          /* mm: the move grows from 0 at a flat area's edge to full over this */
#define MAXROUNDS 16

/* ---------------------------------------------------------------- edge -> midpoint map */
typedef struct { uint64_t *k; int *m; size_t cap, n; } Emap;
static uint64_t ekey(int a, int b) { return a < b ? ((uint64_t)(uint32_t)a << 32) | (uint32_t)b : ((uint64_t)(uint32_t)b << 32) | (uint32_t)a; }
static size_t eslot(uint64_t k, size_t cap) { k *= 0x9e3779b97f4a7c15ULL; return (size_t)(k ^ (k >> 29)) & (cap - 1); }
static int em_init(Emap *e, size_t want) {
    e->cap = 64; while (e->cap < want * 2) e->cap <<= 1; e->n = 0;
    e->k = (uint64_t *)malloc(sizeof(uint64_t) * e->cap); e->m = (int *)malloc(sizeof(int) * e->cap);
    if (!e->k || !e->m) { free(e->k); free(e->m); return -1; }
    for (size_t i = 0; i < e->cap; i++) e->k[i] = UINT64_MAX;
    return 0;
}
static int *em_find(Emap *e, uint64_t k) { size_t s = eslot(k, e->cap); while (e->k[s] != UINT64_MAX) { if (e->k[s] == k) return &e->m[s]; s = (s + 1) & (e->cap - 1); } return NULL; }
static void em_put(Emap *e, uint64_t k) { size_t s = eslot(k, e->cap); while (e->k[s] != UINT64_MAX) { if (e->k[s] == k) return; s = (s + 1) & (e->cap - 1); } e->k[s] = k; e->m[s] = -1; e->n++; }
static void em_free(Emap *e) { free(e->k); free(e->m); }

/* ---------------------------------------------------------------- the mesh */
static int is_flat(const double *v, const int *t) {          /* a horizontal face whose outside is up */
    const double *a = v + 3 * t[0], *b = v + 3 * t[1], *c = v + 3 * t[2];
    double ux = b[0] - a[0], uy = b[1] - a[1], uz = b[2] - a[2], wx = c[0] - a[0], wy = c[1] - a[1], wz = c[2] - a[2];
    double nx = uy * wz - uz * wy, ny = uz * wx - ux * wz, nz = ux * wy - uy * wx, l = sqrt(nx * nx + ny * ny + nz * nz);
    return l > 1e-12 && nz / l > 0.99995;
}
static double d2(const double *v, int a, int b) { double x = v[3 * a] - v[3 * b], y = v[3 * a + 1] - v[3 * b + 1], z = v[3 * a + 2] - v[3 * b + 2]; return x * x + y * y + z * z; }

/* ---------------------------------------------------------------- value noise */
static uint32_t SEED;
static double lat(int x, int y) {
    uint32_t h = (uint32_t)x * 0x8da6b343u ^ (uint32_t)y * 0xd8163841u ^ SEED * 0xcb1ab31fu;
    h ^= h >> 13; h *= 0x5bd1e995u; h ^= h >> 15;
    return (h & 0xffffff) / (double)0x7fffff - 1.0;          /* -1 .. 1 */
}
static double vnoise(double x, double y) {
    int ix = (int)floor(x), iy = (int)floor(y); double fx = x - ix, fy = y - iy;
    double sx = fx * fx * (3 - 2 * fx), sy = fy * fy * (3 - 2 * fy);
    double a = lat(ix, iy), b = lat(ix + 1, iy), c = lat(ix, iy + 1), d = lat(ix + 1, iy + 1);
    return (a + (b - a) * sx) + ((c + (d - c) * sx) - (a + (b - a) * sx)) * sy;
}

/* ---------------------------------------------------------------- distance to an area's edge (Dijkstra over the edges) */
typedef struct { double d; int v; } Hn;
static void hpush(Hn *h, int *n, double d, int v) { int i = (*n)++; while (i > 0) { int p = (i - 1) / 2; if (h[p].d <= d) break; h[i] = h[p]; i = p; } h[i].d = d; h[i].v = v; }
static Hn hpop(Hn *h, int *n) {
    Hn top = h[0], last = h[--(*n)]; int i = 0;
    for (;;) { int c = 2 * i + 1; if (c >= *n) break; if (c + 1 < *n && h[c + 1].d < h[c].d) c++; if (h[c].d >= last.d) break; h[i] = h[c]; i = c; }
    if (*n > 0) h[i] = last;
    return top;
}

int grain_apply(double **vp, int *nvp, int **fp, int *ntp, double amp, uint32_t seed) {
    double *v = *vp; int nv = *nvp, *f = *fp, nt = *ntp, moved = 0;
    double area = 0, L;
    if (amp <= 0 || nt <= 0) return 0;
    for (int t = 0; t < nt; t++) if (is_flat(v, f + 3 * t)) {
        const double *a = v + 3 * f[3 * t], *b = v + 3 * f[3 * t + 1], *c = v + 3 * f[3 * t + 2];
        area += 0.5 * fabs((b[0] - a[0]) * (c[1] - a[1]) - (b[1] - a[1]) * (c[0] - a[0]));
    }
    L = fmax(0.4, sqrt(area / 40000.0));          /* a big base: coarser, so its file stays a sensible size */

    /* split: each flat face's longest edge while it is longer than L; a face beside a split edge takes the same point */
    for (int round = 0; round < MAXROUNDS; round++) {
        Emap e; int marks = 0;
        if (em_init(&e, (size_t)nt + 16) < 0) { *vp = v; *nvp = nv; *fp = f; *ntp = nt; return -1; }
        for (int t = 0; t < nt; t++) {
            int *q = f + 3 * t, k, best = -1; double bl = L * L;
            if (!is_flat(v, q)) continue;
            for (k = 0; k < 3; k++) { double l = d2(v, q[k], q[(k + 1) % 3]); if (l > bl) { bl = l; best = k; } }
            if (best >= 0) { if (2 * (e.n + 1) > e.cap) break; em_put(&e, ekey(q[best], q[(best + 1) % 3])); marks++; }
        }
        if (!marks) { em_free(&e); break; }
        /* the new points */
        { double *nvv = (double *)realloc(v, sizeof(double) * 3 * (size_t)(nv + e.n)); if (!nvv) { em_free(&e); return -1; } v = nvv; }
        for (size_t s = 0; s < e.cap; s++) if (e.k[s] != UINT64_MAX) {
            int a = (int)(e.k[s] >> 32), b = (int)(uint32_t)e.k[s];
            for (int c = 0; c < 3; c++) v[3 * nv + c] = 0.5 * (v[3 * a + c] + v[3 * b + c]);
            e.m[s] = nv++;
        }
        /* the new faces: a face with 1, 2 or 3 split edges becomes 2, 3 or 4 */
        {
            int *nf = (int *)malloc(sizeof(int) * 3 * (size_t)nt * 4), n2 = 0;
            if (!nf) { em_free(&e); *vp = v; *nvp = nv; return -1; }
            for (int t = 0; t < nt; t++) {
                int *q = f + 3 * t, m[3], cnt = 0;
                for (int k = 0; k < 3; k++) { int *p = em_find(&e, ekey(q[k], q[(k + 1) % 3])); m[k] = p ? *p : -1; cnt += p != NULL; }
#define TRI(A, B, C) do { nf[3 * n2] = (A); nf[3 * n2 + 1] = (B); nf[3 * n2 + 2] = (C); n2++; } while (0)
                if (cnt == 0) TRI(q[0], q[1], q[2]);
                else if (cnt == 1) {
                    int k = m[0] >= 0 ? 0 : m[1] >= 0 ? 1 : 2, a = q[k], b = q[(k + 1) % 3], c = q[(k + 2) % 3];
                    TRI(a, m[k], c); TRI(m[k], b, c);
                } else if (cnt == 2) {
                    int k = m[0] < 0 ? 0 : m[1] < 0 ? 1 : 2, a = q[k], b = q[(k + 1) % 3], c = q[(k + 2) % 3];
                    int m1 = m[(k + 1) % 3], m2 = m[(k + 2) % 3];          /* m1 on b-c, m2 on c-a */
                    TRI(m1, c, m2);
                    if (d2(v, a, m1) <= d2(v, b, m2)) { TRI(a, b, m1); TRI(a, m1, m2); }
                    else { TRI(a, b, m2); TRI(b, m1, m2); }
                } else { TRI(q[0], m[0], m[2]); TRI(m[0], q[1], m[1]); TRI(m[2], m[1], q[2]); TRI(m[0], m[1], m[2]); }
#undef TRI
            }
            free(f); f = nf; nt = n2;
        }
        em_free(&e);
    }

    /* move the points inside the flat areas */
    {
        unsigned char *flat = (unsigned char *)malloc((size_t)nv), *edge = (unsigned char *)calloc((size_t)nv, 1);
        double *dist = (double *)malloc(sizeof(double) * (size_t)nv);
        int *deg = (int *)calloc((size_t)nv + 1, sizeof(int)), *adj = NULL, hn = 0; Hn *heap = NULL;
        if (!flat || !edge || !dist || !deg) { free(flat); free(edge); free(dist); free(deg); *vp = v; *nvp = nv; *fp = f; *ntp = nt; return -1; }
        memset(flat, 0, (size_t)nv);
        for (int t = 0; t < nt; t++) { int fl = is_flat(v, f + 3 * t); for (int k = 0; k < 3; k++) { if (fl) flat[f[3 * t + k]] = 1; else edge[f[3 * t + k]] = 1; } }
        /* neighbours (each face edge both ways), as a packed list */
        for (int t = 0; t < 3 * nt; t++) deg[f[t] + 1] += 2;
        for (int i = 0; i < nv; i++) deg[i + 1] += deg[i];
        adj = (int *)malloc(sizeof(int) * (size_t)(deg[nv] ? deg[nv] : 1)); heap = (Hn *)malloc(sizeof(Hn) * (size_t)(deg[nv] + nv + 1));
        if (!adj || !heap) { free(flat); free(edge); free(dist); free(deg); free(adj); free(heap); *vp = v; *nvp = nv; *fp = f; *ntp = nt; return -1; }
        {
            int *fill = (int *)malloc(sizeof(int) * (size_t)nv);
            if (!fill) { free(flat); free(edge); free(dist); free(deg); free(adj); free(heap); *vp = v; *nvp = nv; *fp = f; *ntp = nt; return -1; }
            memcpy(fill, deg, sizeof(int) * (size_t)nv);
            for (int t = 0; t < nt; t++) for (int k = 0; k < 3; k++) { int a = f[3 * t + k], b = f[3 * t + (k + 1) % 3]; adj[fill[a]++] = b; adj[fill[b]++] = a; }
            free(fill);
        }
        for (int i = 0; i < nv; i++) { dist[i] = 1e300; if (flat[i] && edge[i]) { dist[i] = 0; hpush(heap, &hn, 0, i); } }
        while (hn) {
            Hn h = hpop(heap, &hn);
            if (h.d > dist[h.v] || h.d >= FADE) continue;
            for (int j = deg[h.v]; j < deg[h.v + 1]; j++) {
                int w = adj[j]; double nd;
                if (!flat[w] || edge[w]) continue;
                nd = h.d + sqrt(d2(v, h.v, w));
                if (nd < dist[w]) { dist[w] = nd; hpush(heap, &hn, nd, w); }
            }
        }
        SEED = seed;
        {
            double w1 = fmax(2.2, 7 * L), w2 = fmax(1.1, 3.5 * L), w3 = fmax(0.6, 2 * L);
            for (int i = 0; i < nv; i++) {
                double x, y, s, n;
                if (!flat[i] || edge[i]) continue;
                s = dist[i] >= FADE ? 1.0 : dist[i] / FADE; s = s * s * (3 - 2 * s);
                x = v[3 * i]; y = v[3 * i + 1];
                n = 0.55 * vnoise(x / w1, y / w1) + 0.30 * vnoise(x / w2 + 17.3, y / w2 - 5.1) + 0.15 * vnoise(x / w3 - 31.7, y / w3 + 9.4);
                v[3 * i + 2] += amp * s * n; moved++;
            }
        }
        free(flat); free(edge); free(dist); free(deg); free(adj); free(heap);
    }
    *vp = v; *nvp = nv; *fp = f; *ntp = nt;
    return moved;
}
