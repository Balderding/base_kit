/* bld_sd_rx_forest.c: sd_r_x_forest.py in C (see bld_sd_rx.h). */
#include "bld_sd_rx.h"

static double sgn(int i) { return i ? 1 : -1; }

/* x_bark(pts, r, top, n=4, w=0.1): one cut (U(lines), top) */
static GG *x_bark(const P *pts, int np, double r, int n, double w) {
    double vs[16]; GL l = { 0 }; P q[64];
    LS(vs, -r * 0.55, r * 0.55, n);
    for (int k = 0; k < n; k++) {
        for (int i = 0; i + 1 < np; i++) q[i] = POLp(pts[i], vs[k], ATAN2(pts[i + 1].y - pts[i].y, pts[i + 1].x - pts[i].x) + 1.5708);
        GA(&l, BUF(LSTR(q, np - 1), w / 2, 16));
    }
    return UL(&l);
}

/* snag */
typedef struct { double cx, cy, h, sq, t0; int nf; double prof[10]; } Snag;
static double snag_zr(const Snag *s, double z) {
    for (int i = 0; i < 4; i++) {
        double r0 = s->prof[2 * i], z0 = s->prof[2 * i + 1], r1 = s->prof[2 * i + 2], z1 = s->prof[2 * i + 3];
        if (z <= z1) return r0 + (r1 - r0) * (z - z0) / MX(1e-6, z1 - z0);
    }
    return s->prof[8];
}
static int snag_sect(void *c, double z, GEOSGeometry **out) {
    Snag *s = (Snag *)c; GEOSGeometry *st;
    *out = NULL;
    if (!(z < s->h)) return SO_OK;
    st = shp_star(s->cx, s->cy, snag_zr(s, z), snag_zr(s, z) - 0.13, s->nf, s->t0);
    if (!st) return SO_ERR;
    *out = shp_scale(st, 1.0, s->sq, SHP_ORIGIN_CENTER, 0, 0); shp_free(st);
    return *out ? SO_OK : SO_ERR;
}
static int flare_rf(void *c, double s, double *out) { (void)c; *out = 0.42 - 0.18 * s; return SO_OK; }
static void snag(double cx, double cy, double a, PyRandom *rng, SoObj *o, GG **foot) {
    double r = u(0.95, 1.1), h = u(4.6, 5.4), sq = u(0.88, 1.0), t0 = u(0, 6.28);
    Snag *sn = (Snag *)malloc(sizeof *sn); double pr[10] = { r * 1.42, 0.0, r * 1.3, h * 0.05, r * 1.08, h * 0.16, r, h * 0.3, r * 0.94, h };
    sn->cx = cx; sn->cy = cy; sn->h = h; sn->sq = sq; sn->t0 = t0; memcpy(sn->prof, pr, sizeof pr);
    sn->nf = (int)RI(rng, 13, 17);          /* (an error here would leak sn: randint(13, 17) can not raise) */
    S *ps = SN(); { Solid *so = sol_custom(snag_sect, sn, free, h); sol_push(ps, so); sol_unref(so); }
    for (int k = 0; k < 5; k++) {
        double t = t0 + 6.2832 * k / 5 + u(-0.3, 0.3); P p = POL(cx, cy, r * 0.9, t); double d = u(-0.2, 0.2); P q = POL(cx, cy, r * 1.55, t + d);
        P pq[2] = { p, q }; ROD(ps, pq, 2, 0, flare_rf, NULL, 1.6, 0, 0);
    }
    C cuts[8]; int nc = 0; int sp = (int)RI(rng, 3, 4);
    for (int k = 0; k < sp; k++) {
        double t = t0 + 6.2832 * (k + 0.5) / sp + u(-0.2, 0.2); P q[3]; q[0] = PT(cx, cy);
        double d = u(0.35, 0.55); q[1] = POL(cx, cy, r * 1.6, t - d); d = u(0.3, 0.45); q[2] = POL(cx, cy, r * 1.6, t + d);
        cuts[nc].q = POLY(q, 3); cuts[nc].fl = h * u(0.72, 0.9); nc++;
    }
    cuts[nc].q = DISC(cx, cy, r * 0.5, 10); cuts[nc].fl = h * u(0.45, 0.6); nc++;
    { double d = u(1, 2); P wp = POL(cx, cy, r * 0.82, t0 + d); cuts[nc].q = DISCp(wp, 0.24, 8); cuts[nc].fl = h * 0.25; nc++; }
    if (rnd() < 0.5) {
        double tb = t0 + u(2.5, 4); P q[4] = { POL(cx, cy, r * 0.7, tb - 0.4), POL(cx, cy, r * 1.3, tb - 0.5), POL(cx, cy, r * 1.3, tb + 0.5), POL(cx, cy, r * 0.7, tb + 0.4) };
        cuts[nc].q = POLY(q, 4); cuts[nc].fl = h * u(0.3, 0.5); nc++;
    }
    so_obj_init(o, "stump"); ST(o, ps, NULL, -1, cuts, nc, DISC(cx, cy, r * 1.6, 20));
    int n = (int)RI(rng, 1, 2);
    for (int i = 0; i < n; i++) {
        double t = t0 + u(0, 6.28); P c = POL(cx, cy, r * 1.15, t); S *s = SN();
        double h1 = h * u(0.12, 0.22); EXT(s, ELL(c.x, c.y, 0.62, 0.36, t + 1.5708, 12), h1, 0);
        P c2 = POLp(c, 0.12, t); double h2 = h * u(0.12, 0.22) + 0.15; EXT(s, ELL(c2.x, c2.y, 0.42, 0.24, t + 1.5708, 10), h2, 0);
        ST(o, s, "fungus", -1, NULL, 0, DISCp(c, 0.6, 20));
    }
    *foot = OUT(o);
}

/* hollow_log */
static int log_rf(void *c, double s, double *out) { double r = *(double *)c; int e = 0; double p = PWs((s - 0.3) / 0.06, 2, &e); if (e) return SO_ERR; *out = r * (1.05 - 0.12 * s) + 0.08 * py_exp(-p); return SO_OK; }
static int stub2_rf(void *c, double s, double *out) { (void)c; *out = 0.42 - 0.1 * s; return SO_OK; }
static void hollow_log(double cx, double cy, double a, PyRandom *rng, SoObj *o, GG **foot) {
    F ff = FR(cx, cy, a); so_obj_init(o, "log"); double L = u(3.9, 4.2), r = u(1.05, 1.2), x0 = -L / 2 + 0.15;
    double *rp = (double *)xr_mem(sizeof(double)); *rp = r;
    double ss[7]; P pts[7]; LS(ss, 0, 0.97, 7); for (int i = 0; i < 7; i++) pts[i] = FF(&ff, x0 + L * ss[i], 0.15 * SIN(3 * ss[i]));
    S *ps = SN(); ROD(ps, pts, 7, 0, log_rf, rp, 1.9, 0, 0);
    P p = pts[4]; double t = CH2(rng); t = a + t * u(1.0, 1.5); { P pq[2] = { p, POLp(p, r + 0.6, t) }; ROD(ps, pq, 2, 0, stub2_rf, NULL, 2.0, 0, 0); }
    GG *hol;
    if (rnd() < 0.6) { double d = u(0.05, 0.12); hol = ELL(PXY(FF(&ff, x0 + L * d, 0)), r * 0.62, r * 0.5, a, 14); }
    else { double d = u(-0.5, 0.5); hol = ELL(PXY(FF(&ff, d, 0)), L * 0.16, r * 0.35, a, 16); }
    C cuts[3]; cuts[0].q = hol; cuts[0].fl = r * 0.7;
    { double v1 = u(-0.2, 0.2); P p1 = FF(&ff, x0 + L * 0.2, v1); double d = u(0.55, 0.8), v2 = u(-0.2, 0.2); cuts[1].q = BARL(p1, FF(&ff, x0 + L * d, v2), 0.16); cuts[1].fl = r * 1.4; }
    cuts[2].q = x_bark(pts, 7, r, 5, 0.12); cuts[2].fl = r * 1.9 - 0.16;
    if (rnd() < 0.7) { double x = u(0.0, L * 0.3), sv = CH2(rng); P c = FF(&ff, x, sv * r * 0.3); double rr = u(1.0, 1.3); DOME(ps, c.x, c.y, rr, r * 1.95 + 0.12, 0, 0.6, a, 20); }
    { GL h = { 0 }; P pts7[7]; memcpy(pts7, pts, sizeof pts7); GA(&h, BUF(LSTR(pts7, 7), r, 16)); GA(&h, DISCp(POLp(p, r + 0.6, t), 0.4, 20)); ST(o, ps, NULL, -1, cuts, 3, HULL(&h)); }
    double sv = CH2(rng); int n = (int)RI(rng, 2, 4);
    for (int i = 0; i < n; i++) {
        double rr = u(0.32, 0.45), hh = u(0.6, 0.9), x = u(-L * 0.3, L * 0.3), d = u(0.2, 0.35); P c = FF(&ff, x, sv * (r * 1.15 + rr + d));
        double pr[10] = { 0.18, 0.0, 0.18, hh * 0.6, rr, hh * 0.7, rr * 0.8, hh * 0.9, 0.2, hh };
        S *s = SN(); TURN(s, c.x, c.y, pr, 5, 10, 1.0, 0.0, 0.0); ST(o, s, "fungus", -1, NULL, 0, DISCp(c, rr, 20));
    }
    *foot = OUT(o);
}

/* fallen_branch */
typedef struct { double r, q; } Br;
static int br_rf(void *c, double s, double *out) { double r = *(double *)c; *out = r * (1.0 - 0.45 * s); return SO_OK; }
static int twig_rf(void *c, double s, double *out) { Br *b = (Br *)c; *out = b->r * (0.7 - 0.4 * b->q) * (1 - 0.4 * s); return SO_OK; }
static void fallen_branch(double cx, double cy, double a, PyRandom *rng, SoObj *o, GG **foot) {
    F ff = FR(cx, cy, a); so_obj_init(o, "twig"); double L = u(6.3, 6.8), r = u(0.42, 0.5);
    double *rp = (double *)xr_mem(sizeof(double)); *rp = r;
    P pts[4]; pts[0] = FF(&ff, -L / 2, 0); { double d = u(-0.4, 0.4); pts[1] = FF(&ff, -L * 0.15, d); d = u(-0.5, 0.5); pts[2] = FF(&ff, L * 0.2, d); d = u(-0.7, 0.7); pts[3] = FF(&ff, L / 2 - 0.5, d); }
    GG *ln = LSTR(pts, 4);
    S *ps = SN(); ROD(ps, pts, 4, 0, br_rf, rp, 1.9, 0, 0);
    { double vs[2] = { -0.15, 0.15 }; for (int i = 0; i < 2; i++) { P q = POLp(pts[0], 0.15, a + 1.57 + vs[i] * 6); CONE(ps, q.x, q.y, 0.26, r * 2.0 + 0.2, 0.08, 0, 5, 1.0, 0.0); } }
    GL leaves = { 0 }, fl = { 0 }; GA(&fl, BUF(ln, r + 0.2, 16));
    static const double QS[5] = { 0.3, 0.45, 0.6, 0.75, 0.88 };
    int k = (int)RI(rng, 3, 4); int64_t ix[5]; xr_ok(py_sample_e(rng, 5, k, ix));
    double qs[5]; for (int i = 0; i < k; i++) qs[i] = QS[ix[i]];
    for (int i = 1; i < k; i++) for (int j = i; j > 0 && qs[j] < qs[j - 1]; j--) { double t = qs[j]; qs[j] = qs[j - 1]; qs[j - 1] = t; }
    for (int i = 0; i < k; i++) {
        double q = qs[i]; GG *pg = xr_keep(shp_interpolate(ln, q, 1)); P p = PT(shp_x(pg), shp_y(pg));
        double sv = CH2(rng); double t = a + sv * u(0.5, 0.9); double d = u(1.4, 2.2); P e = POLp(p, d, t);
        Br *bc = (Br *)xr_mem(sizeof *bc); bc->r = r; bc->q = q;
        { P pq[2] = { p, e }; ROD(ps, pq, 2, 0, twig_rf, bc, 1.9, 0, 0); }
        int nk = (int)RI(rng, 1, 2);
        for (int j = 0; j < nk; j++) {
            double d1 = u(0.6, 1.0), s2 = CH2(rng); double tt = t + s2 * u(0.4, 0.8); P e2 = POLp(e, d1, tt);
            { P pq[2] = { e, e2 }; ROD(ps, pq, 2, 0.3, NULL, NULL, 1.6, 0, 0); }
            double ro = u(0.6, 0.8); int nn = (int)RI(rng, 5, 7); double rot = u(0, 1);
            GA(&leaves, STAR(e2.x, e2.y, ro, 0.32, nn, rot)); GA(&fl, DISCp(e2, 0.6, 20));
        }
    }
    C cu[1] = { { x_bark(pts, 4, r, 3, 0.1), r * 1.9 - 0.12 } };
    ST(o, ps, NULL, -1, cu, 1, HULL(&fl));
    if (leaves.n) { S *s = SN(); EXT(s, UL(&leaves), 0.5, 0); ST(o, s, "leaf", -1, NULL, 0, UL(&leaves)); }
    *foot = OUT(o);
}

static const XrDef DEFS[] = {
    { "forest", "growth", "stump", "snag", 10.5, 0, 1, snag },
    { "forest", "made", "log", "hollow_log", 10.5, 0, 1, hollow_log },
    { "forest", "debris", "twig", "fallen_branch", 10.5, 0, 1, fallen_branch },
};
void bld_sdrx_forest(void) { xr_register(DEFS, (int)(sizeof DEFS / sizeof DEFS[0])); }
