/* layouts_s2.c: styles2.py in C: the twelve layouts (swamp ... coast) and their helpers (_D, KK, NN, _fr, j_planks,
   j_grid, j_voronoi, j_hex, floor, channel, pools, cracks, banks, grit). styles2 is not rewritten by sizefix (it is
   imported plainly): its own KK() / e() do the size following. The gg3 helpers it gets by install() (rp, blob, rect,
   ring, radial, meander, crater) are cport\leaf.c's lf_* and gg2_crater. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "layouts.h"

#define H (SC.H)
#define R0 (SC.R0)
#define SINK (SC.SINK)
#define U(a, b) py_uniform(rng, (a), (b))
#define RI(a, b) ((int)py_randint(rng, (a), (b)))
#define RND() py_random(rng)
#define TI(f, a) { { (f), NUM_FLOAT, 0 }, { (a), NUM_INT, 0 } }
#define COS py_cos
#define SIN py_sin

const Tier S2_ROCKY[2] = { TI(0.7, 6), TI(0.3, 40) }, S2_SOFTT[2] = { TI(0.55, 25), TI(0.45, 50) };
static const Tier BANKT[2] = { TI(0.6, 25), TI(0.4, 50) }, GRITT[1] = { TI(1.0, 20) };

Geo *s2_D(void) { return g_ptbuf(0, 0, R0 + 1.0, 128); }                         /* styles2._D */
double s2_KK(void) { return SC.K > 1.0 ? SC.K : 1.0; }                            /* styles2.KK: max(1.0, gg.K) */
int64_t s2_NN(int64_t n) {                                                        /* styles2.NN */
    double k2, m; int64_t r;
    if (!(SC.K > 1)) return n;
    k2 = ly_pow(SC.K, 2.0); m = k2 < 12.0 ? k2 : 12.0;
    r = ly_rint(n * m);
    return r > 1 ? r : 1;
}
static double e_(double x) { return fabs(x) >= 20 ? x * s2_KK() : x; }            /* _fr's e: "past the rim" extents follow the base */
S2Fr s2_fr(PyRandom *rng) { S2Fr r; r.phi = U(0, 2 * PI); r.f = lf_frame(0, 0, r.phi); return r; }
Geo *s2_R(const S2Fr *fr, double u0, double u1, double v0, double v1) { return g_R(&fr->f, e_(u0), e_(u1), e_(v0), e_(v1)); }

/* styles2.j_planks(phi, w, L=None, jw=0.22) */
Geo *s2_j_planks(double phi, double w, double L, double jw) {
    double E = 22 * s2_KK(); int64_t j, i, nj = ly_int(2 * E / w) + 1; GeoList *g = LL();
    for (j = 0; j < nj; j++) LP(g, g_box(-E, -E + j * w - jw, E, -E + j * w + jw));
    if (L != 0) {
        int64_t ni = ly_int(2 * E / L) + 2;
        for (j = 0; j < nj; j++) for (i = 0; i < ni; i++)
            LP(g, g_box(-E + i * L + ly_mod(j, 2) * L / 2 - jw, -E + j * w, -E + i * L + ly_mod(j, 2) * L / 2 + jw, -E + (j + 1) * w));
    }
    return g_rot(g_uu(g), LF_DEGREES(phi), SHP_ORIGIN_POINT, 0, 0);
}
/* styles2.j_grid(phi, sx, sy=None, jw=0.22) */
Geo *s2_j_grid(double phi, double sx, double sy, double jw) {
    double E = 22 * s2_KK(), *ys, *xs; int ny, nx, i; GeoList *g = LL();
    if (sy == 0) sy = sx;          /* sy or sx */
    ny = ly_arange(-E, E, sy, &ys); nx = ly_arange(-E, E, sx, &xs);
    for (i = 0; i < ny; i++) LP(g, g_box(-E, ys[i] - jw, E, ys[i] + jw));
    for (i = 0; i < nx; i++) LP(g, g_box(xs[i] - jw, -E, xs[i] + jw, E));
    return g_rot(g_uu(g), LF_DEGREES(phi), SHP_ORIGIN_POINT, 0, 0);
}
/* styles2.j_voronoi(rng, n=26, jw=0.24, amp=0.0) */
Geo *s2_j_voronoi(PyRandom *rng, double n, double jw, double amp) {
    double E = 17 * s2_KK(); int64_t m = ly_rint(n * ly_pow(s2_KK(), 2.0));
    return ly_vor_net(rng, -E, E, m, -E - 3, E + 3, amp != 0, amp, jw, 3);
}
/* styles2.j_hex(r, phi=0.0, jw=0.22) */
Geo *s2_j_hex(double r, double phi, double jw) {
    int64_t m = ly_int(12 * s2_KK()) + 1, i, j; GeoList *segs = LL(); int k;
    for (i = -m; i < m + 1; i++) for (j = -m; j < m + 1; j++) {
        double cx = 1.5 * r * i, cy = sqrt(3.0) * r * (j + 0.5 * ly_mod(i, 2)), hp[8];
        if (fabs(cx) > 19 * s2_KK() || fabs(cy) > 19 * s2_KK()) continue;
        for (k = 0; k < 4; k++) { hp[2 * k] = cx + r * COS(k * PI / 3); hp[2 * k + 1] = cy + r * SIN(k * PI / 3); }
        LP(segs, g_linebuf(hp, 4, jw, SHP_CAP_ROUND, 3));
    }
    return g_rot(g_uu(segs), LF_DEGREES(phi), SHP_ORIGIN_POINT, 0, 0);
}

/* styles2.floor(s, region, h, kind, joints=None, depth=0.38, edge=0.5) -> the slab polygon */
Geo *s2_floor(Scene *s, Geo *region, double h, const char *kind, Geo *joints, double depth, double edge) {
    GeoList *got = PR(s, g_polys(region), H + h, kind); Geo *sl;
    if (!got->n) return g_empty();
    sl = g_uu(got);
    if (joints) { Geo *j = g_inter(joints, g_buf(sl, -edge)); if (!g_isempty(j)) S_tcut(s, L1(j), NF(H + h - depth), NF(H + h), "joint"); }
    return sl;
}
/* styles2.channel(s, rng, w, depth, kind, amp=3.0) */
Geo *s2_channel(Scene *s, PyRandom *rng, double w, double depth, const char *kind, double amp) {
    double phi = U(0, PI), off = U(-5, 5); Geo *ln = LAG(lf_meander(rng, phi, off, amp, w, 40)), *g;
    g = g_simp(g_bufx(ln, w / 2, 16, SHP_CAP_FLAT, SHP_JOIN_ROUND, 5.0), 0.05);
    S_gcut(s, L1(g), NF(depth), kind);
    return g;
}
static Geo *blob_rp(PyRandom *rng, double dmax, double r0, double r1) {          /* blob(*rp(rng, 0, dmax), rng.uniform(r0, r1), rng, rng.randint(3, 5)) */
    double x, y, r; int k;
    lf_rp(rng, 0, dmax, &x, &y); r = U(r0, r1); k = RI(3, 5);
    return LAG(lf_blob(x, y, r, rng, k));
}
/* styles2.pools(s, rng, n, r0, r1, depth, kind, avoid=None) */
Geo *s2_pools(Scene *s, PyRandom *rng, int64_t n, double r0, double r1, double depth, const char *kind, Geo *avoid) {
    GeoList *out = LL(); int64_t t, m; Geo *dk = g_ptbuf(0, 0, R0 - 1.0, 16); int i;
    if (SC.K > 1.3) n = s2_NN(n);
    m = n * 30;
    for (t = 0; t < m; t++) {
        Geo *p; int ok;
        if (out->n >= n) break;
        p = blob_rp(rng, 10, r0, r1);
        ok = g_contains(dk, p) && S_free(s, p, 0.5);
        if (ok) for (i = 0; i < out->n; i++) if (!(g_dist(p, out->v[i]) > 0.8)) { ok = 0; break; }
        if (ok && avoid && g_intersects(p, avoid)) ok = 0;
        if (ok) LP(out, p);
    }
    if (out->n) S_gcut(s, out, NF(depth), kind);
    return out->n ? g_uu(out) : g_empty();
}
/* styles2.cracks(s, rng, n, depth=0.4, kind="crack", w=0.22) */
void s2_cracks(Scene *s, PyRandom *rng, int64_t n, double depth, const char *kind, double w) {
    GeoList *cs = LL(); int64_t t, m = ly_rint(n * s2_KK());
    if (m < n) m = n;          /* max(n, round(n * KK())) */
    for (t = 0; t < m; t++) {
        double ax, ay, tt, L, bx, by;
        lf_rp(rng, 0, 12, &ax, &ay); tt = U(0, 6.283); L = U(8, 20); bx = ax + L * COS(tt); by = ay + L * SIN(tt);
        LP(cs, g_bufx(g_wigline(ax, ay, bx, by, rng, 0.7, 7), w, 3, SHP_CAP_ROUND, SHP_JOIN_ROUND, 5.0));
    }
    S_gcut(s, L1(g_uu(cs)), NF(depth), kind);
}
/* styles2.banks(s, rng, n, r0, r1, h, kind, tiers=((0.6, 25), (0.4, 50)), region=None) */
void s2_banks(Scene *s, PyRandom *rng, int64_t n, double r0, double r1, double h0, double h1, const char *kind, const Tier *t, int nt, Geo *region) {
    Geo *reg = region ? region : g_ptbuf(0, 0, R0 - 0.8, 16); int64_t i, m;
    if (!t) { t = BANKT; nt = 2; }
    n = s2_NN(n); m = n * 25;
    for (i = 0; i < m; i++) {
        Geo *p;
        if (n <= 0) break;
        p = blob_rp(rng, 11, r0, r1);
        if (g_contains(reg, p) && S_free(s, p, 0.5)) { double h = U(h0, h1); S_stack(s, L1(p), NF(h), t, nt, kind, NONE, NULL); n--; }
    }
}
/* styles2.grit(s, rng, n0, n1, kind="grit") */
void s2_grit(Scene *s, PyRandom *rng, int n0, int n1, const char *kind) {
    int k = RI(n0, n1);
    S_stack(s, S_scatter(s, k, 0.47, 0.65, NULL, 0.35, 1, 0), NF(0.28), GRITT, 1, kind, NONE, NULL);
}
/* for p in s.scatter(rng.randint(n0, n1), rmin, rmax, gap=0.5, round_=False): s.stack([p], rng.uniform(h0, h1), ROCKY, kind) */
static void rocks_on(Scene *s, PyRandom *rng, int n0, int n1, double rmin, double rmax, double h0, double h1, const char *kind) {
    GeoList *sc = S_scatter(s, RI(n0, n1), rmin, rmax, NULL, 0.5, 0, 0); int i;
    for (i = 0; i < sc->n; i++) { double h = U(h0, h1); S_stack(s, L1(sc->v[i]), NF(h), S2_ROCKY, 2, kind, NONE, NULL); }
}
static Geo *dr(double r) { return g_ptbuf(0, 0, r, 16); }

/* ------------------------------------------------------------------------------------------------ the layouts */
Scene *s2_swamp(int n) {
    Scene *s = ly_mk("swamp", "Swamps of Jargono", n, "#4f4a30"); PyRandom *rng = &s->rng; int v = RI(0, 3); Geo *D = s2_D();
    if (v == 0) s2_channel(s, rng, U(4, 6), 0.6, "water", 4.0);
    else if (v == 1) s2_pools(s, rng, RI(2, 4), 2.2, 3.6, 0.55, "water", NULL);
    else if (v == 2) {
        S2Fr fr = s2_fr(rng); double c = U(-3, 3) * SC.K, w = 4.5 * ly_pow(s2_KK(), 0.6);
        S_gcut(s, L1(g_diff(D, g_buf(s2_R(&fr, -30, 30, c - w, c + w), 1.5))), NF(0.55), "water");
    } else {
        S2Fr fr = s2_fr(rng); double a = U(-2, 3); Geo *reg = g_inter(s2_R(&fr, -30, 30, a, 30), D);
        s2_floor(s, reg, 0.55, "tflag", s2_j_grid(fr.phi, U(4.2, 5.0), 0, 0.22), 0.38, 0.5);
        s2_pools(s, rng, 2, 1.8, 2.6, 0.5, "water", g_buf(reg, 1.0));
    }
    s2_banks(s, rng, RI(1, 3), 1.6, 2.6, 0.5, 0.9, "mud", NULL, 0, NULL);
    s2_grit(s, rng, 10, 20, "grit");
    return s;
}

Scene *s2_cynder(int n) {
    Scene *s = ly_mk("cynder", "Caverns of Cynder", n, "#3a3230"); PyRandom *rng = &s->rng; int v = RI(0, 3); Geo *D = s2_D();
    if (v == 0) s2_channel(s, rng, U(3.5, 5.5), 0.7, "lava", 4.0);
    else if (v == 1) s2_pools(s, rng, RI(2, 3), 2.0, 3.2, 0.65, "lava", NULL);
    else if (v == 2) s2_floor(s, D, 0.55, "basalt", s2_j_voronoi(rng, RI(14, 22), 0.32, 0.3), 0.5, 0.5);
    else {
        S2Fr fr = s2_fr(rng); double c = U(-4, 2) * SC.K; Geo *reg;
        S_gcut(s, L1(g_inter(s2_R(&fr, -30, 30, -30, c), D)), NF(0.7), "lava");
        reg = g_inter(s2_R(&fr, -30, 30, c + 0.8, 30), D);
        s2_floor(s, reg, 0.5, "basalt", s2_j_voronoi(rng, 16, 0.28, 0.3), 0.45, 0.5);
    }
    if (v < 2) s2_cracks(s, rng, RI(2, 4), 0.45, "lava", 0.22);
    rocks_on(s, rng, 2, 4, 1.1, 1.9, 0.8, 1.4, "basalt");
    s2_grit(s, rng, 10, 20, "grit");
    return s;
}

Scene *s2_targa(int n) {
    Scene *s = ly_mk("targa", "Targa Plateau", n, "#8fa9b3"); PyRandom *rng = &s->rng; int v = RI(0, 3); Geo *D = s2_D(); S2Fr fr = s2_fr(rng);
    static const Tier TS[] = { TI(0.5, 30), TI(0.5, 55) };
    if (v == 0) s2_floor(s, D, 0.5, "alien", s2_j_hex(U(3.4, 4.2), fr.phi, 0.22), 0.38, 0.5);
    else if (v == 1) { double c = U(-3, 3) * SC.K; s2_floor(s, g_inter(s2_R(&fr, -30, 30, c - 5, c + 5), D), 0.6, "alien", s2_j_grid(fr.phi, 5.0, 3.3, 0.22), 0.38, 0.5); }
    else if (v == 2) s2_floor(s, D, 0.45, "ice", s2_j_voronoi(rng, RI(8, 13), 0.2, 0.5), 0.3, 0.5);
    else {
        double a = U(-3, 2); Geo *reg = g_inter(s2_R(&fr, -30, 30, a, 30), D);
        s2_floor(s, reg, 0.55, "alien", s2_j_grid(fr.phi + 0.785, 4.6, 0, 0.22), 0.38, 0.5);
        s2_cracks(s, rng, 2, 0.35, "crack", 0.22);
    }
    s2_banks(s, rng, RI(1, 3), 1.8, 3.0, 0.4, 0.8, "snow", TS, 2, NULL);
    return s;
}

Scene *s2_trederra(int n) {
    Scene *s = ly_mk("trederra", "Trederra", n, "#5a5340"); PyRandom *rng = &s->rng; int v = RI(0, 3), i; Geo *D = s2_D(); S2Fr fr = s2_fr(rng);
    static const Tier TB[] = { TI(0.7, 8), TI(0.3, 40) };
    if (v == 0 || v == 1) {
        double c = U(-4, 4) * SC.K, w = U(5.5, 7.0); GeoList *l = LL();
        s2_floor(s, g_inter(s2_R(&fr, -30, 30, c - w / 2, c + w / 2), D), 0.55, "duck", s2_j_planks(fr.phi + PI / 2, 1.5, 0, 0.25), 0.45, 0.5);
        LP(l, g_inter(s2_R(&fr, -30, 30, c + (-w / 2 + 0.9) - 0.5, c + (-w / 2 + 0.9) + 0.5), D));
        LP(l, g_inter(s2_R(&fr, -30, 30, c + (w / 2 - 0.9) - 0.5, c + (w / 2 - 0.9) + 0.5), D));
        S_prism(s, l, NF(H + 0.85), "wood", NF(H + 0.4), SC.MINW, 0);
        if (v == 1) s2_pools(s, rng, 2, 1.6, 2.4, 0.5, "water", s2_R(&fr, -30, 30, c - w / 2 - 1, c + w / 2 + 1));
    } else if (v == 2) {
        int k = RI(1, 2);
        for (i = 0; i < k; i++) {
            double cx, cy, r;
            lf_rp(rng, 0, 7, &cx, &cy); r = U(2.0, 2.8);
            if (S_free(s, g_ptbuf(cx, cy, r + 2, 16), 0.3)) gg2_crater(s, cx, cy, r, 0.75);
        }
    } else {
        double c = U(-2, 2) * SC.K, half = U(4.0, 5.0), *us; int sg, nu = ly_arange(-20 * s2_KK(), 20 * s2_KK(), 3.7, &us);
        for (sg = -1; sg <= 1; sg += 2) {
            GeoList *bags = LL();
            for (i = 0; i < nu; i++) {
                double u = us[i], a; Geo *b = g_buf(s2_R(&fr, u, u + 3.4, sg * (half + 0.2), sg * (half + 2.4)), 0.35);
                a = U(-3, 3);
                LP(bags, g_inter(g_rot(b, a, SHP_ORIGIN_CENTROID, 0, 0), D));
            }
            S_stack(s, bags, NF(1.2), TB, 2, "sandbag", NONE, NULL);
        }
        S_gcut(s, L1(g_inter(s2_R(&fr, -30, 30, c - 1.0, c + 1.0), D)), NF(0.35), "water");
    }
    s2_cracks(s, rng, RI(1, 2), 0.4, "rut", 0.35);
    s2_grit(s, rng, 10, 22, "grit");
    return s;
}

Scene *s2_fortress(int n) {
    Scene *s = ly_mk("fortress", "Forbidden Fortress", n, "#6a6658"); PyRandom *rng = &s->rng; int v = RI(0, 3); Geo *D = s2_D(); S2Fr fr = s2_fr(rng);
    if (v == 0) { double w = U(2.0, 2.6), L = U(9, 12); s2_floor(s, D, 0.55, "plank", s2_j_planks(fr.phi, w, L, 0.22), 0.38, 0.5); }
    else if (v == 1) s2_floor(s, D, 0.5, "tatami", s2_j_grid(fr.phi, 9.0, 4.5, 0.3), 0.3, 0.5);
    else if (v == 2) {
        double c = U(-2, 4) * SC.K; GeoList *l = LL(); int64_t k, m = ly_int(12 * s2_KK());
        s2_floor(s, g_inter(s2_R(&fr, -30, 30, c, 30), D), 0.7, "plank", s2_j_planks(fr.phi, 2.2, 10, 0.22), 0.38, 0.5);
        for (k = 1; k < m; k++) LP(l, s2_R(&fr, -30, 30, c - 1.5 - k * 1.5, c - 1.5 - k * 1.5 + 0.5));
        S_gcut(s, L1(g_inter(g_uu(l), dr(R0 - 0.6))), NF(0.3), "rut");
    } else s2_floor(s, D, 0.5, "flag", s2_j_voronoi(rng, RI(16, 24), 0.22, 0.0), 0.38, 0.5);
    return s;
}

Scene *s2_beast(int n) {
    Scene *s = ly_mk("beast", "Belly of the Beast", n, "#7a3038"); PyRandom *rng = &s->rng; int v = RI(0, 3); Geo *D = s2_D();
    static const Tier TF[] = { TI(0.5, 20), TI(0.5, 50) };
    if (v == 0) s2_floor(s, D, 0.55, "fleshfloor", s2_j_voronoi(rng, RI(9, 14), 0.3, 0.9), 0.45, 0.5);
    else if (v == 1) s2_pools(s, rng, RI(2, 4), 2.0, 3.2, 0.6, "bile", NULL);
    else if (v == 2) s2_channel(s, rng, U(3.5, 5.0), 0.6, "bile", 4.5);
    else {
        S2Fr fr = s2_fr(rng); double a0 = -14 * s2_KK() + U(0, 3), st = U(6.5, 8.5), *ys; int ny = ly_arange(a0, 16 * s2_KK(), st, &ys), i; GeoList *r = LL();
        for (i = 0; i < ny; i++) LP(r, g_inter(s2_R(&fr, -30, 30, ys[i], ys[i] + 1.7), D));
        S_stack(s, r, NF(0.7), TF, 2, "flesh", NONE, NULL);
    }
    s2_banks(s, rng, RI(1, 3), 1.6, 2.8, 0.6, 1.2, "flesh", NULL, 0, NULL);
    return s;
}

Scene *s2_frontier(int n) {
    Scene *s = ly_mk("frontier", "Frontier Town", n, "#8a6f4c"); PyRandom *rng = &s->rng; int v = RI(0, 3); Geo *D = s2_D(); S2Fr fr = s2_fr(rng);
    if (v == 0) {
        double c = U(-3, 3) * SC.K; GeoList *ruts = LL(); static const double O[2] = { 4.5, 8.5 }; int i;
        s2_floor(s, g_inter(s2_R(&fr, -30, 30, c, 30), D), 0.85, "board", s2_j_planks(fr.phi + PI / 2, U(1.8, 2.3), 0, 0.22), 0.38, 0.5);
        PR(s, L1(g_inter(s2_R(&fr, -30, 30, c - 0.9, c + 0.1), D)), H + 1.0, "wood");
        for (i = 0; i < 2; i++) { Geo *m = LAG(lf_meander(rng, fr.phi, c - O[i], 0.6, 1.0, 40)); LP(ruts, g_bufx(m, 0.5, 16, SHP_CAP_FLAT, SHP_JOIN_ROUND, 5.0)); }
        S_gcut(s, L1(g_uu(ruts)), NF(0.35), "rut");
    } else if (v == 1) { double w = U(1.9, 2.5), L = U(10, 13); s2_floor(s, D, 0.6, "board", s2_j_planks(fr.phi, w, L, 0.22), 0.38, 0.5); }
    else if (v == 2) {
        double o = U(-3, 3); GeoList *l = LL(); static const double DD[2] = { -2.6, 2.6 }; int i;
        for (i = 0; i < 2; i++) { Geo *m = LAG(lf_meander(rng, fr.phi, o + DD[i], 0.8, 1.0, 40)); LP(l, g_bufx(m, 0.55, 16, SHP_CAP_FLAT, SHP_JOIN_ROUND, 5.0)); }
        S_gcut(s, L1(g_uu(l)), NF(0.4), "rut");
        s2_cracks(s, rng, 2, 0.3, "crack", 0.22);
    } else {
        double a = U(-4, 1), b = U(-4, 1);
        s2_floor(s, g_inter(s2_R(&fr, a, 30, b, 30), D), 0.8, "board", s2_j_planks(fr.phi, 2.1, 0, 0.22), 0.38, 0.5);
    }
    s2_grit(s, rng, 12, 24, "grit");
    return s;
}

Scene *s2_graveyard(int n) {
    Scene *s = ly_mk("graveyard", "Graveyard", n, "#4c4a3a"); PyRandom *rng = &s->rng; int v = RI(0, 3); Geo *D = s2_D(); S2Fr fr = s2_fr(rng);
    if (v == 0) {
        double cx, cy; Geo *g;
        lf_rp(rng, 0, 4, &cx, &cy); g = LAG(lf_rect(cx, cy, 9.0, 4.2, fr.phi));
        S_gcut(s, L1(g), NF(0.9), "pit");
        s2_banks(s, rng, 2, 1.8, 2.6, 0.7, 1.1, "earth", NULL, 0, g_diff(dr(R0 - 0.8), g_buf(g, 0.6)));
    } else if (v == 1) { double c = U(-3, 3) * SC.K; s2_floor(s, g_inter(s2_R(&fr, -30, 30, c - 3, c + 3), D), 0.45, "flag", s2_j_voronoi(rng, 26, 0.2, 0.0), 0.38, 0.5); }
    else if (v == 2) {
        double cx, cy; Geo *g; GeoList *top;
        lf_rp(rng, 0, 4, &cx, &cy); g = LAG(lf_rect(cx, cy, 9.5, 4.6, fr.phi));
        top = PR(s, L1(g), H + 0.8, "headstone");
        if (top->n) S_tcut(s, L1(g_diff(g_inset(g, 0.6), g_inset(g, 0.95))), NF(H + 0.5), NF(H + 0.8), "slot");
    } else s2_cracks(s, rng, 3, 0.35, "crack", 0.22);
    rocks_on(s, rng, 1, 3, 1.0, 1.6, 0.6, 1.0, "rock");
    s2_grit(s, rng, 10, 20, "grit");
    return s;
}

Scene *s2_forest(int n) {
    Scene *s = ly_mk("forest", "Forest Floor", n, "#4a4630"); PyRandom *rng = &s->rng; int v = RI(0, 3);
    static const Tier TR[] = { TI(0.5, 20), TI(0.5, 50) };
    if (v == 0) {
        double a0 = U(0, 6.28), cx = 13.5 * SC.K * COS(a0), cy = 13.5 * SC.K * SIN(a0); static const double DA[4] = { -0.7, -0.3, 0.1, 0.5 }; GeoList *roots = LL(); int i;
        for (i = 0; i < 4; i++) {
            double bx = cx - (16 + U(0, 8)) * SC.K * COS(a0 + DA[i]), by, w; Geo *ln;
            by = cy - (16 + U(0, 8)) * SC.K * SIN(a0 + DA[i]);
            ln = g_wigline(cx, cy, bx, by, rng, 1.2, 7);
            w = U(0.7, 1.1);
            LP(roots, g_bufx(ln, w, 4, SHP_CAP_ROUND, SHP_JOIN_ROUND, 5.0));
        }
        S_stack(s, L1(g_uu(roots)), NF(1.0), TR, 2, "root", NONE, NULL);
    } else if (v == 1) s2_channel(s, rng, U(2.5, 3.5), 0.5, "water", 4.0);
    else if (v == 2) s2_pools(s, rng, RI(1, 2), 1.8, 2.8, 0.45, "water", NULL);
    else {
        double phi = U(0, PI), off = U(-3, 3); Geo *m = LAG(lf_meander(rng, phi, off, 2.5, 2.4, 40));
        S_gcut(s, L1(g_bufx(m, 1.2, 16, SHP_CAP_FLAT, SHP_JOIN_ROUND, 5.0)), NF(0.3), "rut");
    }
    rocks_on(s, rng, 2, 4, 1.0, 1.7, 0.6, 1.1, "rock");
    s2_grit(s, rng, 12, 24, "grit");
    return s;
}

Scene *s2_dungeon(int n) {
    Scene *s = ly_mk("dungeon", "Dungeon", n, "#55524e"); PyRandom *rng = &s->rng; int v = RI(0, 3); Geo *D = s2_D(); S2Fr fr = s2_fr(rng);
    if (v == 0) { double a = U(4.6, 5.6), b = U(4.6, 5.6); s2_floor(s, D, 0.55, "flag", s2_j_grid(fr.phi, a, b, 0.22), 0.38, 0.5); }
    else if (v == 1) s2_floor(s, D, 0.55, "flag", s2_j_voronoi(rng, RI(18, 28), 0.24, 0.0), 0.38, 0.5);
    else if (v == 2) { double w = U(3.0, 3.6), L = U(5.5, 7.0); s2_floor(s, D, 0.55, "flag", s2_j_planks(fr.phi, w, L, 0.22), 0.38, 0.5); }
    else {
        double c = U(-4, 4) * SC.K; Geo *r1 = s2_R(&fr, -30, 30, c + 0.9, 30), *r2 = s2_R(&fr, -30, 30, -30, c - 0.9);
        s2_floor(s, g_inter(r1, D), 0.55, "flag", s2_j_grid(fr.phi, 5.0, 0, 0.22), 0.38, 0.5);
        s2_floor(s, g_inter(r2, D), 0.55, "flag", s2_j_grid(fr.phi, 5.0, 0, 0.22), 0.38, 0.5);
        S_gcut(s, L1(g_inter(s2_R(&fr, -30, 30, c - 0.55, c + 0.55), D)), NF(0.4), "water");
    }
    return s;
}

Scene *s2_temple(int n) {
    Scene *s = ly_mk("temple", "Ancient Temple", n, "#9c8d6e"); PyRandom *rng = &s->rng; int v = RI(0, 3); Geo *D = s2_D(); S2Fr fr = s2_fr(rng);
    static const Tier TS[] = { TI(0.5, 25), TI(0.5, 50) };
    if (v == 0) s2_floor(s, D, 0.55, "tflag", s2_j_grid(fr.phi, U(6.5, 8.0), 0, 0.22), 0.38, 0.5);
    else if (v == 1) {
        double c = U(-6, -2) * SC.K; int k;
        for (k = 0; k < 3; k++) s2_floor(s, g_inter(s2_R(&fr, -30, 30, c + k * 4.0, 30), D), 0.5 + 0.55 * k, "tflag", k ? s2_j_grid(fr.phi, 30, 30, 0.22) : NULL, 0.38, 0.5);
        S_tcut(s, L1(g_inter(g_inter(s2_j_grid(fr.phi, 6.0, 40, 0.22), s2_R(&fr, -30, 30, c + 8.6, 30)), dr(R0 - 0.6))), NF(H + 1.6 - 0.35), NF(H + 1.6), "joint");
    } else if (v == 2) {
        Geo *sl = s2_floor(s, D, 0.55, "tflag", NULL, 0.38, 0.5); double cx, cy; GeoList *l = LL(); static const double RR[3] = { 3.0, 6.0, 9.0 }; int i;
        lf_rp(rng, 0, 3, &cx, &cy);
        for (i = 0; i < 3; i++) LP(l, LAG(lf_ring(cx, cy, RR[i], RR[i] - 0.4, 48)));
        LP(l, LAG(lf_radial(cx, cy, 3.0, 9.0, 8, rng, 0.2)));
        S_tcut(s, L1(g_inter(g_uu(l), g_buf(sl, -0.6))), NF(H + 0.2), NF(H + 0.55), "joint");
    } else {
        double a = U(-3, 3); Geo *reg = g_inter(s2_R(&fr, -30, 30, a, 30), D);
        s2_floor(s, reg, 0.55, "tflag", s2_j_grid(fr.phi + 0.4, 5.5, 0, 0.22), 0.38, 0.5);
        s2_banks(s, rng, 2, 2.0, 3.0, 0.4, 0.7, "snow", TS, 2, g_diff(dr(R0 - 0.8), g_buf(reg, 0.5)));
    }
    return s;
}

Scene *s2_coast(int n) {
    Scene *s = ly_mk("coast", "Rocky Coast", n, "#7a705e"); PyRandom *rng = &s->rng; int v = RI(0, 3); Geo *D = s2_D();
    if (v == 0) s2_pools(s, rng, RI(2, 4), 1.8, 3.0, 0.6, "water", NULL);
    else if (v == 1) s2_floor(s, D, 0.6, "shelf", s2_j_voronoi(rng, RI(7, 11), 0.35, 0.8), 0.5, 0.5);
    else if (v == 2) {
        S2Fr fr = s2_fr(rng); double off = U(-4, 2), y;
        LAG(lf_meander(rng, fr.phi, off, 1.5, 1.0, 40));          /* ln (drawn, then not used: the "if False" branch) */
        y = U(-6, -1);
        S_gcut(s, L1(g_inter(s2_R(&fr, -30, 30, -30, y), D)), NF(0.55), "water");
    } else s2_channel(s, rng, U(2.5, 4.0), 0.55, "water", 3.5);
    rocks_on(s, rng, 2, 5, 1.2, 2.2, 0.7, 1.4, "shelf");
    s2_grit(s, rng, 14, 26, "pebble");
    return s;
}
