/* bld_sd_rx_highland.c: sd_r_x_highland.py in C (see bld_sd_rx.h); scenes._barrel_side copied here. */
#include "bld_sd_rx.h"

static double sgn(int i) { return i ? 1 : -1; }

/* xh_bar(p, q, r, z0, z1) */
typedef struct { int m; double px, py, ux, uy, L, r, z0, z1; } XhBar;
static int xh_sect(void *c, double z, GEOSGeometry **out) {
    XhBar *b = (XhBar *)c; int n = b->m + 1, nrun = 0; double *rs = (double *)malloc(sizeof(double) * (size_t)n), *rw = (double *)malloc(sizeof(double) * (size_t)n), *xy = (double *)malloc(sizeof(double) * 4 * (size_t)n);
    int bad = 0;
    *out = NULL;
    for (int i = 0; i < n; i++) {
        double s = (double)i / (double)b->m, cc = b->z0 + (b->z1 - b->z0) * s, w;
        if (z <= cc) w = b->r;
        else if (z < cc + b->r) { int e = 0; double d2 = PWs(z - cc, 2.0, &e), v = b->r * b->r - d2; if (e) { bad = 1; break; } w = sqrt(v > 0.0 ? v : 0.0); }
        else w = 0.0;
        if (w >= 0.16) { rs[nrun] = s; rw[nrun] = w; nrun++; }
    }
    if (!bad && nrun >= 2) {
        int k = 0;
        for (int j = 0; j < nrun; j++) { xy[k++] = b->px + b->ux * rs[j] * b->L - b->uy * rw[j]; xy[k++] = b->py + b->uy * rs[j] * b->L + b->ux * rw[j]; }
        for (int j = nrun - 1; j >= 0; j--) { xy[k++] = b->px + b->ux * rs[j] * b->L - b->uy * -rw[j]; xy[k++] = b->py + b->uy * rs[j] * b->L + b->ux * -rw[j]; }
        GEOSGeometry *g = shp_polygon(xy, 2 * nrun);
        if (!g) bad = 1;
        else {
            int v = shp_is_valid(g);
            if (v < 0) bad = 1;
            else if (v && shp_area(g) > 0.05) { *out = g; g = NULL; }
            else { *out = shp_buffer_q(g, 0, 16); if (!*out) bad = 1; }
            shp_free(g);
        }
    }
    free(rs); free(rw); free(xy);
    return bad ? SO_ERR : SO_OK;
}
static void xh_bar(S *s, P p, P q, double r, double z0, double z1) {
    double L = DIST(p, q); int m; XhBar *b;
    if (L == 0.0) L = 1.0;
    m = (int)PINT(L / 0.12); if (m < 8) m = 8;
    b = (XhBar *)malloc(sizeof *b); b->m = m; b->px = p.x; b->py = p.y; b->ux = (q.x - p.x) / L; b->uy = (q.y - p.y) / L; b->L = L; b->r = r; b->z0 = z0; b->z1 = z1;
    Solid *so = sol_custom(xh_sect, b, free, MX(z0, z1) + r); sol_push(s, so); sol_unref(so);
}
/* xh_rough(cx, cy, L, W, a, rng, j=0.12) */
static GG *xh_rough(double cx, double cy, double L, double W, double a, PyRandom *rng, double j) {
    F f = FR(cx, cy, a); P pts[12]; int n = 0; static const int E[4][4] = { { -1, -1, 1, -1 }, { 1, -1, 1, 1 }, { 1, 1, -1, 1 }, { -1, 1, -1, -1 } };
    for (int e = 0; e < 4; e++) for (int k = 0; k < 3; k++) {
        double s = k / 3.0, x0 = E[e][0], y0 = E[e][1], x1 = E[e][2], y1 = E[e][3];
        double x = (x0 + (x1 - x0) * s) * L / 2 + u(-j, j), y = (y0 + (y1 - y0) * s) * W / 2 + u(-j, j); pts[n++] = FF(&f, x, y);
    }
    return BUF(POLY(pts, n), 0, 16);
}
/* scenes._barrel_side(cx, cy, a, Ln, r, lift=0.0) */
typedef struct { double Ln, r; } Brl;
static int barrel_rf(void *c, double u_, double *out) {
    Brl *b = (Brl *)c; int e = 0; double p = PWs(fabs(fabs(u_) - b->Ln * 0.29) / 0.34, 2, &e); if (e) return SO_ERR;
    double hoop = 0.13 * MX(0.0, 1 - p); *out = b->r * (0.84 + 0.16 * COS(u_ / (b->Ln / 2) * 1.35)) + hoop; return SO_OK;
}
static void barrel_side(S *s, double cx, double cy, double a, double Ln, double r, double lift) {
    Brl *b = (Brl *)xr_mem(sizeof *b); b->Ln = Ln; b->r = r; xr_ok(so_loft(s, cx, cy, a, Ln, barrel_rf, b, 1.2, "round", lift, 1));
}

/* beacon_pyre */
static void beacon_pyre(double cx, double cy, double a, PyRandom *rng, SoObj *o, GG **foot) {
    double r = u(1.8, 2.3), Sz = r * u(1.3, 1.4), rl = u(0.26, 0.3); int n = (int)RI(rng, 5, 6); so_obj_init(o, "wood"); double dz = rl * 1.55;
    S *allg = SN();
    for (int k = 0; k < n; k++) {
        double t = a + (k % 2) * 1.5708 + u(-0.05, 0.05); double fx = cx + u(-0.06, 0.06), fy = cy + u(-0.06, 0.06); F f2 = FR(fx, fy, t); double Lk = Sz + 0.5 - k * 0.1, vv = Sz / 2 - rl - k * 0.06;
        for (int i = 0; i < 2; i++) { double d = u(-0.15, 0.15); P q = FF(&f2, d, sgn(i) * vv); double tt = t + u(-0.04, 0.04); LIE(allg, q.x, q.y, tt, Lk, rl, k * dz); }
        if (k >= 2 && rnd() < 0.6) { double d1 = u(-0.2, 0.2), d2 = u(-0.3, 0.3); P q = FF(&f2, d1, d2); double tt = t + u(-0.1, 0.1); LIE(allg, q.x, q.y, tt, Lk * 0.8, rl * 0.9, k * dz); }
    }
    double ht = MN(3.3, n * dz + u(0.3, 0.55));
    for (int i = 0; i < 7; i++) { double tt = i * 1.05 + u(0, 0.4); P q = POL(cx, cy, i == 0 ? 0.0 : Sz * 0.22, tt); double rr = Sz * u(0.2, 0.26); double hh = ht * (i == 0 ? 1.0 : u(0.85, 0.95)); DOME(allg, q.x, q.y, rr, hh, 0, 1.0, 0.0, 10); }
    ST(o, allg, NULL, -1, NULL, 0, RECT(cx, cy, Sz + 0.7, Sz + 0.7, a));
    int m = (int)RI(rng, 7, 10); double rp[40]; xr_ok(sd_ringpts(cx, cy, r + 1.0, m, a, 0.1, rng, rp));
    for (int i = 0; i < m; i++) { double rr = u(0.4, 0.55), hh = u(0.35, 0.55); S *s = SN(); ROCK(s, rp[2 * i], rp[2 * i + 1], rr, hh, rng, 0, 0.0, 9); ST(o, s, "slate", -1, NULL, 0, DISC(rp[2 * i], rp[2 * i + 1], 0.55, 20)); }
    FPUSH(o, DISC(cx, cy, r + 1.5, 20)); *foot = OUT(o);
}

/* fallen_cross */
static void fallen_cross(double cx, double cy, double a, PyRandom *rng, SoObj *o, GG **foot) {
    F ff = FR(cx, cy, a); double L = u(4.9, 5.9), w = u(0.85, 1.0), th = u(0.6, 0.7); so_obj_init(o, "slate"); double xc = L * 0.2, A = L * 0.55, R = L * 0.19;
    GG *g; { GL l = { 0 }; GA(&l, RECTp(FF(&ff, -L / 2 + (xc + A / 2 + L / 2) / 2, 0), xc + A / 2 + L / 2, w, a)); GA(&l, RECTp(FF(&ff, xc, 0), w, A, a)); { P q = FF(&ff, xc, 0); GA(&l, ANN(q.x, q.y, R, R - 0.45, 24)); }
      GA(&l, RECTp(FF(&ff, xc + A / 2 - 0.25, 0), 0.5, w * 1.3, a)); GA(&l, RECTp(FF(&ff, xc, A / 2 - 0.25), w * 1.3, 0.5, a)); GA(&l, RECTp(FF(&ff, xc, -A / 2 + 0.25), w * 1.3, 0.5, a)); g = UL(&l); }
    GG *holes = DIF(DISCp(FF(&ff, xc, 0), R - 0.45, 20), U2(RECTp(FF(&ff, xc, 0), w + 0.1, A, a), RECTp(FF(&ff, xc, 0), A, w + 0.1, a)));
    g = DIF(g, holes);
    if (rnd() < 0.35) { P q[7] = { FF(&ff, -L * 0.18, -w), FF(&ff, -L * 0.14, -w * 0.1), FF(&ff, -L * 0.2, w * 0.2), FF(&ff, -L * 0.15, w), FF(&ff, -L * 0.08, w), FF(&ff, -L * 0.1, 0), FF(&ff, -L * 0.06, -w) }; g = DIF(g, POLY(q, 7)); }
    int b = UP(o, g, th, NULL); GROOVE(o, RECTp(FF(&ff, -L * 0.12 - xc * 0.2, 0), L * 0.62, w - 0.32, a), b, 0.13, 0.08);
    GG *pan = RECTp(FF(&ff, -L * 0.12 - xc * 0.2, 0), L * 0.5, w - 0.6, a); double sp = 0.34;
    { GL l = { 0 }; double xs[128]; int nx = AR(xs, 128, -L, L, sp); for (int i = 0; i < nx; i++) GA(&l, BARL(FF(&ff, xs[i] - w, -w), FF(&ff, xs[i] + w, w), 0.12)); for (int i = 0; i < nx; i++) GA(&l, BARL(FF(&ff, xs[i] - w, w), FF(&ff, xs[i] + w, -w), 0.12)); CUT(o, INT(UL(&l), pan), b, 0.09); }
    { P q = FF(&ff, xc, 0); CUT(o, U1(ANN(q.x, q.y, R - 0.12, R - 0.3, 24)), b, 0.07); S *s = SN(); DOME(s, q.x, q.y, w * 0.36, 0.3, 0, 1.0, 0.0, 14); ST(o, s, NULL, b, NULL, 0, NULL); }
    { GL l = { 0 }; for (int i = 0; i < 2; i++) { double sv = sgn(i); GA(&l, BARL(FF(&ff, xc, sv * (w / 2 + 0.15)), FF(&ff, xc, sv * (A / 2 - 0.55)), 0.12)); } GA(&l, BARL(FF(&ff, xc + w / 2 + 0.15, 0), FF(&ff, xc + A / 2 - 0.55, 0), 0.12)); CUT(o, UL(&l), b, 0.08); }
    double sv = CH2(rng); double x = u(0.4, 0.9), y = u(1.0, 1.2); P c = FF(&ff, -L / 2 + x, sv * (w / 2 + y)); double t = a + u(-0.4, 0.4);
    GG *rg = xh_rough(c.x, c.y, 1.5, 1.3, t, rng, 0.08); double hh = u(0.9, 1.1); int bs = UP(o, rg, hh, NULL); int s2 = XON(o, INS(RECTp(c, 1.5, 1.3, t), 0.22), bs, 0.16, NULL);
    CUT(o, RECTp(c, w * 0.6, 0.45, t), s2, 0.45);
    *foot = OUT(o);
}

/* caber_and_stone */
typedef struct { double r0, r1, L; } Caber;
static int caber_rf(void *c, double t, double *out) { Caber *k = (Caber *)c; *out = k->r0 + (k->r1 - k->r0) * (t + k->L / 2) / k->L; return SO_OK; }
static void caber_and_stone(double cx, double cy, double a, PyRandom *rng, SoObj *o, GG **foot) {
    F ff = FR(cx, cy, a); double L = u(7.4, 8.6), r0 = u(0.46, 0.52), r1 = r0 * u(0.68, 0.75); so_obj_init(o, "wood");
    double lf = r0 * 0.8; Caber *ck = (Caber *)xr_mem(sizeof *ck); ck->r0 = r0; ck->r1 = r1; ck->L = L;
#define HZ(t_) (lf + (r0 + (r1 - r0) * ((t_) + L / 2) / L) * 1.1 * 0.93)
    int nk = (int)RI(rng, 3, 5); double kn[5]; for (int i = 0; i < nk; i++) kn[i] = u(-L * 0.4, L * 0.4);
    C cuts[8]; int nc = 0; int ng = (int)RI(rng, 2, 3);
    for (int i = 0; i < ng; i++) { double x0 = u(-L / 2, L / 2 - 1.6), v = u(-0.1, 0.1); P p = FF(&ff, x0, v); double d1 = u(0.8, 1.6), d2 = u(-0.05, 0.05); cuts[nc].q = BARL(p, FF(&ff, x0 + d1, v + d2), 0.16); cuts[nc].fl = HZ(x0) - 0.1; nc++; }
    for (int i = 0; i < nk; i++) { double y = u(-0.12, 0.12); P q = FF(&ff, kn[i], y); cuts[nc].q = ANN(q.x, q.y, 0.2, 0.08, 8); cuts[nc].fl = HZ(kn[i]) - 0.08; nc++; }
#undef HZ
    { S *s = SN(); SPIN(s, cx, cy, a, L, caber_rf, ck, 1.1, lf, "round"); ST(o, s, NULL, -1, cuts, nc, RECT(cx, cy, L, 2 * r0, a)); }
    { double x = u(-2, 2), y = u(1.7, 2.0); P c = FF(&ff, x, y); double rs = u(0.7, 0.85); double sq = u(0.8, 0.9), aa = u(0, 3); S *s = SN(); DOME(s, c.x, c.y, rs, rs * 1.15, 0, sq, aa, 20); ST(o, s, "slate", -1, NULL, 0, DISCp(c, rs, 20)); }
    double sv = -1, t = a + u(-0.15, 0.15); double x = u(-1.2, 1.2), y = sv * u(1.5, 1.8); P hc = FF(&ff, x, y); F f2 = FR(hc.x, hc.y, t); double Lh = u(3.0, 3.5);
    { S *s = SN(); P l[2] = { FF(&f2, -Lh / 2, 0), FF(&f2, Lh / 2, 0) }; ROD(s, l, 2, 0.25, NULL, NULL, 1.2, 0, 0); ST(o, s, NULL, -1, NULL, 0, RECTp(hc, Lh, 0.4, t)); }
    { S *s = SN(); P q = FF(&f2, Lh / 2 + 0.25, 0); DOME(s, q.x, q.y, 0.38, 0.62, 0, 1.0, 0.0, 14); ST(o, s, "iron", -1, NULL, 0, DISCp(q, 0.4, 20)); }
    if (rnd() < 0.6) {
        double x1 = u(-1.5, 1.5), y1 = u(2.9, 3.2); P tc = FF(&ff, x1, y1); double l = u(2.8, 3.2), tt = a + u(-0.08, 0.08); int tb = UP(o, RECTp(tc, l, 0.45, tt), 0.32, NULL);
        CUT(o, BARL(POLp(tc, 1.3, a), POLp(tc, 1.3, a + PI), 0.12), tb, 0.08);
    }
    *foot = OUT(o);
}

/* hurdle */
static void hurdle_one(SoObj *o, double L, double W, double x0, double y0, double t, double z) {
    F ff = FR(x0, y0, t); int nz = (int)PINT(L / 0.62); double xs[64]; for (int i = 0; i <= nz; i++) xs[i] = (-L / 2 + 0.25) + i * (L - 0.5) / nz;
    double vs[64]; int nv = AR(vs, 64, -W / 2 + 0.2, W / 2 - 0.1, 0.36);
    GG *hole = RECTp(FF(&ff, 0, W * 0.18), 0.9, 0.42, t);
    GL l1 = { 0 }, l2 = { 0 }, l3 = { 0 }, l4 = { 0 }, l5 = { 0 };
    for (int j = 0; j < nv; j++) GA(&l1, BARL(FF(&ff, -L / 2 + 0.2, vs[j]), FF(&ff, L / 2 - 0.2, vs[j]), 0.3));
    GG *lo = DIF(UL(&l1), hole);
    for (int j = 0; j < nv; j++) for (int i = 0; i < nz; i++) if ((i + j) % 2 == 0) GA(&l2, BARL(FF(&ff, xs[i], vs[j]), FF(&ff, xs[i + 1], vs[j]), 0.3));
    GG *hi = DIF(UL(&l2), hole);
    for (int i = 1; i < nz; i++) GA(&l3, BARL(FF(&ff, xs[i], -W / 2 - 0.05), FF(&ff, xs[i], W / 2), 0.3));
    for (int i = 1; i < nz; i++) GA(&l3, TRI(FF(&ff, xs[i], -W / 2 - 0.05), FF(&ff, xs[i], -W / 2 - 0.5), 0.3));
    GG *zl = UL(&l3);
    for (int i = 0; i < 2; i++) { double sx = sgn(i); GA(&l4, BARL(FF(&ff, sx * (L / 2 - 0.15), -W / 2 - 0.3), FF(&ff, sx * (L / 2 - 0.15), W / 2 + 0.1), 0.42)); }
    GG *sails = UL(&l4);
    for (int i = 0; i < 2; i++) for (int j = 0; j < nv; j += 2) { P q = FF(&ff, sgn(i) * (L / 2 - 0.15), vs[j]); GA(&l5, ANN(q.x, q.y, 0.32, 0.12, 8)); }
    GG *tw = UL(&l5);
    UP(o, zl, z + 0.34, NULL); UP(o, lo, z + 0.3, NULL); UP(o, hi, z + 0.44, NULL); UP(o, sails, z + 0.48, NULL); UP(o, tw, z + 0.52, NULL);
}
static void hurdle(double cx, double cy, double a, PyRandom *rng, SoObj *o, GG **foot) {
    so_obj_init(o, "wood"); double L = u(5.3, 6.2), W = u(3.0, 3.4);
    hurdle_one(o, L, W, cx, cy, a, 0.0);
    if (rnd() < 0.35) { double x = cx + u(-0.25, 0.25), y = cy + u(-0.25, 0.25), t = a + u(-0.05, 0.05); hurdle_one(o, L, W, x, y, t, 0.4); }
    GG *ex[1] = { RECT(cx, cy, L, W + 1.0, a) };
    *foot = OUTX(o, ex, 1);
}

/* burial_kist */
static void burial_kist(double cx, double cy, double a, PyRandom *rng, SoObj *o, GG **foot) {
    F ff = FR(cx, cy, a); double L = u(3.4, 4.0), W = u(2.0, 2.4), t = u(0.42, 0.52); so_obj_init(o, "slate");
    GCUT(o, RECT(cx, cy, L, W, a), 0.7, "pit"); int64_t tip = -1; if (rnd() < 0.4) xr_ok(py_randrange1_e(rng, 4, &tip));
    P sc[4] = { FF(&ff, 0, -(W / 2 + t / 2)), FF(&ff, 0, W / 2 + t / 2), FF(&ff, -(L / 2 + t / 2), 0), FF(&ff, L / 2 + t / 2, 0) };
    double sl[4] = { L + 2 * t, L + 2 * t, t, t }, sw[4] = { t, t, W, W };
    for (int i = 0; i < 4; i++) {
        P c = sc[i]; double l = sl[i], w = sw[i];
        if (i == tip) { c.x = c.x + (cx - c.x) * 0.25; c.y = c.y + (cy - c.y) * 0.25; double ta = a + u(-0.1, 0.1); GG *g = xh_rough(c.x, c.y, l * 0.9, w * 1.6, ta, rng, 0.08); double hh = u(0.25, 0.35); UP(o, g, hh, NULL); continue; }
        GG *g = xh_rough(c.x, c.y, l, w, a, rng, 0.06); double hh = u(0.7, 1.0); int b = UP(o, g, hh, NULL);
        double ml = MX(l, w); double d1 = u(0.2, 0.45); static const double ch[2] = { 0.0, PI }; double c1 = CH(rng, ch, 2); double t1 = a + c1 + u(-0.2, 0.2);
        P q[3]; q[0] = POLp(c, ml * d1, t1); double t2 = a + u(-0.4, 0.4); q[1] = POLp(c, ml, t2); q[2] = POLp(c, ml, a + 1.57);
        GG *cg = INT(POLY(q, 3), g); double d = u(0.15, 0.3); CUT(o, cg, b, d);
    }
    double x = u(1.9, 2.6), y = u(-0.6, 0.6); P cc = FF(&ff, x, y); double ct = a + u(-0.4, 0.4); GG *cg = xh_rough(cc.x, cc.y, L * 0.85, W + 1.0, ct, rng, 0.15);
    double hh = u(0.55, 0.65); int cp = UP(o, cg, hh, NULL);
    { GL l = { 0 }; int n = (int)RI(rng, 3, 6); for (int i = 0; i < n; i++) { double rr = u(0, 1.0), tt = u(0, 6.28); P q = POLp(cc, rr, tt); double r = u(0.18, 0.24); GA(&l, DISCp(q, r, 10)); } CUT(o, UL(&l), cp, 0.12); }
    if (rnd() < 0.5) { P c0 = POLp(cc, 0.2, ct); CUT(o, U2(ANN(c0.x, c0.y, 0.55, 0.42, 16), DISCp(c0, 0.2, 8)), cp, 0.1); }
    int n = (int)RI(rng, 4, 7);
    for (int i = 0; i < n; i++) { double rr = u(L * 0.75, L * 0.95), tt = u(0, 6.28); P p = POL(cx, cy, rr, tt); double r1 = u(0.3, 0.45), h1 = u(0.25, 0.45); S *s = SN(); ROCK(s, p.x, p.y, r1, h1, rng, 0, 0.0, 9); ST(o, s, NULL, -1, NULL, 0, DISCp(p, 0.45, 20)); }
    *foot = OUT(o);
}

/* pot_still */
static void pot_still(double cx, double cy, double a, PyRandom *rng, SoObj *o, GG **foot) {
    F ff = FR(cx, cy, a); so_obj_init(o, "brass"); double r = u(1.25, 1.45), h = u(2.4, 2.8);
    double pr[32] = { r * 0.72, 0.0, r * 0.92, h * 0.18, r, h * 0.38 }; int np = 6; np += CRV(pr + np, r, 0.5, h * 0.38, h * 0.82, 1, 6);
    pr[np++] = 0.46; pr[np++] = h * 0.9; pr[np++] = 0.56; pr[np++] = h * 0.92; pr[np++] = 0.56; pr[np++] = h; int npp = np / 2;
    C cuts[3]; double rrs[2] = { r * 0.8, r * 0.6 };
    for (int k = 0; k < 2; k++) {
        double rr = rrs[k], z = h * 0.4;
        for (int i = 3; i + 1 < npp; i++) { double r0 = pr[2 * i], z0 = pr[2 * i + 1], r1 = pr[2 * i + 2], z1 = pr[2 * i + 3]; if (MN(r0, r1) <= rr && rr <= MX(r0, r1)) { double dd = r1 - r0; z = z0 + (z1 - z0) * (rr - r0) / (dd != 0.0 ? dd : 1); break; } }
        cuts[k].q = ANN(cx, cy, rr + 0.08, rr - 0.08, 24); cuts[k].fl = z - 0.12;
    }
    cuts[2].q = DISC(cx, cy, 0.3, 10); cuts[2].fl = h - 0.25;
    { S *s = SN(); TURN(s, cx, cy, pr, npp, 24, 1.0, 0.0, 0.0); ST(o, s, NULL, -1, cuts, 3, DISC(cx, cy, r, 20)); }
    { int m = (int)RI(rng, 8, 10); double rp[40]; xr_ok(sd_ringpts(cx, cy, r + 0.35, m, a, 0.12, rng, rp));
      for (int i = 0; i < m; i++) { double r1 = u(0.36, 0.46), h1 = u(0.4, 0.6); S *s = SN(); ROCK(s, rp[2 * i], rp[2 * i + 1], r1, h1, rng, 0, 0.0, 9); ST(o, s, "slate", -1, NULL, 0, DISC(rp[2 * i], rp[2 * i + 1], 0.45, 20)); } }
    double x = u(1.25, 1.5), y = u(-0.4, 0.4); P tc = FF(&ff, r + x, y); double rt = u(1.0, 1.15), ht = u(2.6, 3.0);
    int tb = UP(o, ANN(tc.x, tc.y, rt, rt - 0.24, 24), ht, "wood");
    { GL l = { 0 }; for (int k = 0; k < 12; k++) GA(&l, BARL(POLp(tc, rt - 0.3, k * 0.52), POLp(tc, rt + 0.1, k * 0.52), 0.12)); CUT(o, UL(&l), tb, 0.1); }
    GG *wd = DISCp(tc, rt - 0.22, 20); double wh = ht - u(0.35, 0.5); int wt = UP(o, wd, wh, "water");
    XON(o, DIF(ANN(tc.x, tc.y, rt * 0.62, rt * 0.62 - 0.3, 20), RECTp(POLp(tc, rt * 0.6, a + PI), 0.6, 0.6, a)), wt, 0.12, "brass");
    double t = ATAN2(tc.y - cy, tc.x - cx); P p0 = POL(cx, cy, 0.2, t), p1 = POLp(tc, rt * 0.45, t + PI);
    { S *s = SN(); P pq[2] = { p0, p1 }; xh_bar(s, p0, p1, 0.27, h - 0.05, ht + 0.25); ST(o, s, NULL, -1, NULL, 0, BUF(LSTR(pq, 2), 0.3, 16)); }
    double s1 = CH2(rng); P sp = POLp(tc, rt + 0.3, t + s1 * 1.3); UP(o, RECTp(sp, 0.6, 0.3, t), 0.55, "brass");
    double s2 = CH2(rng); P kc = POLp(sp, 0.75, t + s2 * 0.6); { S *s = SN(); barrel_side(s, kc.x, kc.y, t + 1.57, 1.2, 0.5, 0.0); ST(o, s, "wood", -1, NULL, 0, RECTp(kc, 1.2, 1.0, t + 1.57)); }
    int n = (int)RI(rng, 2, 3);
    for (int i = 0; i < n; i++) { double d = u(0.9, 1.4); double tt = t + PI + u(-1.2, 1.2); P q = POL(cx, cy, r + d, tt); double rot = u(0, 3); UP(o, RECTp(q, 0.7, 0.42, rot), 0.32, "earth"); }
    *foot = OUT(o);
}

/* crook_and_plaid */
static int crook_rf(void *c, double q, double *out) { (void)c; double rs = 0.26; *out = rs * (q < 0.03 ? 1.25 : 1.0) + ((fabs(q - 0.3) < 0.015 || fabs(q - 0.58) < 0.015) ? 0.06 : 0.0); return SO_OK; }
static void crook_and_plaid(double cx, double cy, double a, PyRandom *rng, SoObj *o, GG **foot) {
    F ff = FR(cx, cy, a); double L = u(6.0, 7.4); so_obj_init(o, "wood");
    { S *s = SN(); P l[2] = { FF(&ff, -L / 2, 0), FF(&ff, L / 2, 0) }; ROD(s, l, 2, 0, crook_rf, NULL, 1.1, 0, 0); ST(o, s, NULL, -1, NULL, 0, RECT(cx, cy, L, 0.5, a)); }
    P h0 = FF(&ff, L / 2 + 0.05, 0.7); P pts[13]; xr_ok(sd_arc(h0.x, h0.y, 0.7, a - 1.5708, a + 2.4, 12, &pts[0].x));
    { S *s = SN(); ROD(s, pts, 13, 0.27, NULL, NULL, 1.1, 0, 0); P l[2] = { pts[12], POLp(pts[12], 0.5, a + 2.4 + 1.9) }; ROD(s, l, 2, 0.25, NULL, NULL, 1.1, 0, 0);
      P q = FF(&ff, L / 2 - 0.15, 0); LIE(s, q.x, q.y, a, 0.3, 0.3, 0.0); ST(o, s, "bone", -1, NULL, 0, DISCp(h0, 1.0, 20)); }
    double x = u(-1.6, 1.2), y = u(1.9, 2.2); P pc = FF(&ff, x, -y); double t = a + u(-0.3, 0.3); F f2 = FR(pc.x, pc.y, t); double Lp = u(2.6, 3.0), Wp = u(1.8, 2.1);
    P tq[4]; tq[0] = FF(&f2, Lp / 2 - 0.2, -Wp / 2 + 0.1); { double d1 = u(1.0, 1.4), d2 = u(-0.2, 0.2); tq[1] = FF(&f2, Lp / 2 + d1, -Wp * 0.4 + d2); d1 = u(1.0, 1.4); d2 = u(-0.2, 0.2); tq[2] = FF(&f2, Lp / 2 + d1, Wp * 0.3 + d2); } tq[3] = FF(&f2, Lp / 2 - 0.2, Wp / 2 - 0.2);
    GG *tail = BUF(POLY(tq, 4), 0.15, 16); int tl = UP(o, tail, 0.18, "cloth"); double te = Lp / 2 + 1.0;
    { GL l = { 0 }; double vs[64]; int nv = AR(vs, 64, -Wp * 0.35, Wp * 0.28, 0.2); for (int i = 0; i < nv; i++) GA(&l, BARL(FF(&f2, te, vs[i]), FF(&f2, te + 0.6, vs[i] + 0.02), 0.1)); CUT(o, UL(&l), tl, 0.1); }
    int b1 = UP(o, RRECTp(pc, Lp, Wp, t, 0.25), 0.26, "cloth");
    double d1 = u(-0.1, 0.1), d2 = u(-0.08, 0.08); int b2 = XON(o, RRECTp(FF(&f2, d1, d2), Lp - 0.25, Wp - 0.25, t, 0.25), b1, 0.24, "cloth");
    d1 = u(-0.1, 0.1); d2 = u(-0.08, 0.08); int b3 = XON(o, RRECTp(FF(&f2, d1, d2), Lp - 0.5, Wp - 0.5, t, 0.25), b2, 0.24, "cloth");
    double s0 = u(0, 0.4); GL st = { 0 };
    { double xs[64]; int nx = AR(xs, 64, -Lp / 2 + s0, Lp / 2, 0.7); for (int i = 0; i < nx; i++) { double xx[2] = { xs[i], xs[i] + 0.2 }; for (int k = 0; k < 2; k++) GA(&st, BARL(FF(&f2, xx[k], -Wp), FF(&f2, xx[k], Wp), 0.11)); } }
    { double vs[64]; int nv = AR(vs, 64, -Wp / 2 + s0 * 0.6, Wp / 2, 0.7); for (int i = 0; i < nv; i++) { double vv[2] = { vs[i], vs[i] + 0.2 }; for (int k = 0; k < 2; k++) GA(&st, BARL(FF(&f2, -Lp, vv[k]), FF(&f2, Lp, vv[k]), 0.11)); } }
    CUT(o, INT(UL(&st), RRECTp(pc, Lp - 0.5, Wp - 0.5, t, 0.25)), b3, 0.06);
    CUT(o, U1(BARL(FF(&f2, -Lp / 2 + 0.3, -Wp / 2 + 0.38), FF(&f2, Lp / 2 - 0.3, -Wp / 2 + 0.38), 0.1)), b2, 0.05);
    d1 = u(-0.6, 0.6); d2 = u(-0.3, 0.3); P bc = FF(&f2, d1, d2); XON(o, ANN(bc.x, bc.y, 0.34, 0.16, 14), b3, 0.1, "brass");
    *foot = OUT(o);
}

static const XrDef DEFS[] = {
    { "highland", "made", "wood", "beacon_pyre", 10.5, 0, 0, beacon_pyre },
    { "highland", "made", "slate", "fallen_cross", 10.5, 0, 0, fallen_cross },
    { "highland", "made", "wood", "caber_and_stone", 10.5, 0, 0, caber_and_stone },
    { "highland", "made", "wood", "hurdle", 10.5, 0, 0, hurdle },
    { "highland", "made", "slate", "burial_kist", 10.5, 1, 0, burial_kist },
    { "highland", "made", "brass", "pot_still", 10.5, 0, 0, pot_still },
    { "highland", "made", "wood", "crook_and_plaid", 10.5, 0, 0, crook_and_plaid },
};
void bld_sdrx_highland(void) { xr_register(DEFS, (int)(sizeof DEFS / sizeof DEFS[0])); }
