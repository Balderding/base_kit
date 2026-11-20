/* bld_sd_rx_coast.c: sd_r_x_coast.py in C (see bld_sd_rx.h). */
#include "bld_sd_rx.h"

static double sgn(int i) { return i ? 1 : -1; }

/* x_ball(x, y, r, lift=0.0, n=16) */
static void x_ball(S *s, double x, double y, double r, double lift, int n) {
    double pr[32] = { r * 0.75, 0.0, r, r * 0.55 }; int np = 4;
    np += CRV(pr + np, r, 0.12, r * 0.55 + 0.01, r * 1.9, 1, 6);
    TURN(s, x, y, pr, np / 2, n, 1.0, 0.0, lift);
}
static double half_round(void *c, double s) { double k0 = ((double *)c)[0], k1 = ((double *)c)[1]; double v = 1 - PW(2 * s - 1, 2); return k0 + k1 * sqrt(MX(0.0, v)); }

/* oars */
static double oar_rf(void *c, double s) {
    double L = *(double *)c, x = s * (L - 2.6 - 0.1);
    if (x < 0.25) return 0.3;
    if (x < 1.6) return 0.22;
    if (2.7 < x && x < 3.5) return 0.33;
    return 0.27 - 0.04 * x / (L - 2.6);
}
static double blade_rf(void *c, double s) { (void)c; return 0.24 - 0.04 * s; }
static void oars(double cx, double cy, double a, PyRandom *rng, SoObj *o, GG **foot) {
    so_obj_init(o, "drift"); int n = rnd() < 0.6 ? 2 : 1;
    for (int i = 0; i < n; i++) {
        double b = a; if (i != 0) { double sv = CH2(rng); b = a + sv * u(0.12, 0.3); } else b = a + 0.0;
        double rr = i == 0 ? 0.0 : u(0.8, 1.1); P fc = POL(cx, cy, rr, a + 1.5708); F f2 = FR(fc.x, fc.y, b);
        double L = u(8.4, 8.8), x0 = -L / 2, xb = L / 2 - 2.6; double *lp = (double *)xr_mem(sizeof(double)); *lp = L;
        S *s = SN(); TBAR(s, FF(&f2, x0, 0), FF(&f2, xb + 0.1, 0), 0, oar_rf, lp, 0.95, 0, 0, 0);
        GL l = { 0 }; for (int k = 0; k < 4; k++) GA(&l, BARL(FF(&f2, x0 + 2.75 + k * 0.2, -0.5), FF(&f2, x0 + 2.75 + k * 0.2, 0.5), 0.08));
        C cu[1] = { { UL(&l), 0.5 } }; ST(o, s, NULL, -1, cu, 1, RECTp(FF(&f2, -1.3, 0), L - 2.6, 0.7, b));
        P tp[8] = { FF(&f2, xb - 0.1, -0.22), FF(&f2, xb + 1.0, -0.6), FF(&f2, L / 2 - 0.1, -0.62), FF(&f2, L / 2, -0.4), FF(&f2, L / 2, 0.45), FF(&f2, L / 2 - 0.15, 0.62), FF(&f2, xb + 1.0, 0.6), FF(&f2, xb - 0.1, 0.22) };
        GG *tip = POLY(tp, 8);
        if (rnd() < 0.35) { double v1 = u(-0.3, 0.3); P p = FF(&f2, L / 2 + 0.05, v1); double d = u(0.7, 1.1), v2 = u(-0.1, 0.1); tip = DIF(tip, TRI(p, FF(&f2, L / 2 - d, v2), 0.24)); }
        UP(o, tip, 0.24, NULL); s = SN(); TBAR(s, FF(&f2, xb - 0.1, 0), FF(&f2, L / 2 - 0.5, 0), 0, blade_rf, NULL, 0.4, 0, 0, 0); ST(o, s, NULL, -1, NULL, 0, RECTp(FF(&f2, xb + 1, 0), 2.2, 0.5, b));
        RECTp(FF(&f2, 0, 0), L, 1.3, b);
    }
    double rr = u(1.4, 1.8); P c = POL(cx, cy, rr, a - 1.5708); double t = a + u(-0.5, 0.5); F f3 = FR(c.x, c.y, t);
    P ac = FF(&f3, 0.35, 0); double ar[18]; xr_ok(sd_arc(ac.x, ac.y, 0.42, -1.5708, 1.5708, 8, ar));
    S *s = SN(); EXT(s, U2(BUF(LSTR((const P *)ar, 9), 0.13, 16), BARL(FF(&f3, 0.35, 0), FF(&f3, -0.75, 0), 0.26)), 0.3, 0);
    ST(o, s, "iron", -1, NULL, 0, DISCp(c, 0.8, 20));
    *foot = OUT(o);
}

/* cannon */
typedef struct { double L, r; } Gun;
static double gun_rf(void *c, double s) {
    Gun *g = (Gun *)c; double L = g->L, r = g->r, x = s * (L - 0.9);
    if (x < 0.25) return r * 0.55;
    if (x < 0.5) return r * 1.02;
    if (x < 2.0) return r * 1.0;
    if (x < 2.3) return r * 1.08;
    if (x < 3.2) return r * 0.94;
    if (x < 3.4) return r * 1.0;
    if (x < L - 1.8) return r * (0.9 - 0.18 * (x - 3.4) / (L - 5.2));
    if (x < L - 1.4) return r * 0.8;
    return r * (0.72 + 0.16 * SIN(MN(1.0, (x - L + 1.4) / 0.5) * 1.5708));
}
static double knob_rf(void *c, double s) { (void)c; return s < 0.6 ? 0.35 : 0.22; }
static double rammer_rf(void *c, double s) { (void)c; return s > 0.92 ? 0.36 : 0.2; }
static void cannon(double cx, double cy, double a, PyRandom *rng, SoObj *o, GG **foot) {
    F ff = FR(cx, cy, a); so_obj_init(o, "iron"); double L = u(7.4, 7.8), r = u(0.8, 0.9), xb = -L / 2 + 0.5;
    Gun *gp = (Gun *)xr_mem(sizeof *gp); gp->L = L; gp->r = r; P pile; double sv;
    static double hr[2] = { 0.1, 1.2 };
    if (rnd() < 0.65) {
        double zc = u(2.3, 2.5), Lc = u(4.6, 5.0), xc = -L / 2 + 0.6 + Lc / 2 - 0.3, Wc = 2.2;
        for (int i = 0; i < 2; i++) {
            double s1 = sgn(i), xlh[3][3] = { { xc + Lc * 0.3, Lc * 0.4, zc + 0.1 }, { xc - Lc * 0.05, Lc * 0.3 + 0.3, zc - 0.25 }, { xc - Lc * 0.35, Lc * 0.3 + 0.3, zc - 0.65 } };
            for (int k = 0; k < 3; k++) {
                double x = xlh[k][0], l = xlh[k][1], h = xlh[k][2];
                int ch = UP(o, RECTp(FF(&ff, x, s1 * (Wc / 2 - 0.25)), l, 0.5, a), h, "wood");
                if (k == 0) { ON(o, RECTp(FF(&ff, x + 0.3, s1 * (Wc / 2 - 0.25)), 0.9, 0.5, a), ch, 0.14, NULL); P sp[2] = { FF(&ff, x + 0.3 + -0.3, s1 * (Wc / 2 - 0.25)), FF(&ff, x + 0.3 + 0.3, s1 * (Wc / 2 - 0.25)) }; STUDS(o, sp, 2, ch, 0.2, 0.24, NULL, 8); }
            }
        }
        double xs2[2] = { xc + Lc * 0.38, xc - Lc * 0.38 };
        for (int i = 0; i < 2; i++) {
            double x = xs2[i]; UP(o, RECTp(FF(&ff, x, 0), 0.7, Wc + 1.2, a), 0.75, "wood");
            for (int j = 0; j < 2; j++) { double s1 = sgn(j); P l[2] = { FF(&ff, x - 0.65, s1 * (Wc / 2 + 0.32)), FF(&ff, x + 0.65, s1 * (Wc / 2 + 0.32)) }; S *s = SN(); XWALL(s, l, 2, 0.42, half_round, hr);
                ST(o, s, "wood", -1, NULL, 0, RECTp(FF(&ff, x, s1 * (Wc / 2 + 0.32)), 1.3, 0.45, a)); }
        }
        UP(o, RECTp(FF(&ff, xc - Lc * 0.38, 0), 1.1, Wc - 1.0, a), zc - r * 0.75, "wood");
        S *s = SN(); TBAR(s, FF(&ff, xb, 0), FF(&ff, L / 2, 0), 0, gun_rf, gp, 0.4, 1, zc, zc); TBAR(s, FF(&ff, xb - 0.5, 0), FF(&ff, xb + 0.05, 0), 0, knob_rf, NULL, 0.4, 1, zc, zc);
        for (int j = 0; j < 2; j++) { double s1 = sgn(j); TBARz(s, FF(&ff, xc + Lc * 0.3 + 0.3, s1 * 0.6), FF(&ff, xc + Lc * 0.3 + 0.3, s1 * (Wc / 2 + 0.1)), 0.3, zc + 0.05, zc + 0.05); }
        C cu[1] = { { DISCp(FF(&ff, L / 2 - 0.25, 0), r * 0.42, 10), zc + 0.2 } }; ST(o, s, NULL, -1, cu, 1, RECT(cx, cy, L, 2 * r, a));
        sv = CH2(rng); double x = u(-1.0, 1.0), y = u(2.8, 3.1); pile = FF(&ff, x, sv * y);
    } else {
        double t = a + u(-0.15, 0.15); F f2 = FR(cx, cy, t);
        S *s = SN(); TBAR(s, FF(&f2, xb, 0), FF(&f2, L / 2, 0), 0, gun_rf, gp, 0.4, 1, r * 0.85, r * 0.7); TBAR(s, FF(&f2, xb - 0.5, 0), FF(&f2, xb + 0.05, 0), 0, knob_rf, NULL, 0.4, 1, r * 0.85, r * 0.85);
        C cu[1] = { { DISCp(FF(&f2, L / 2 - 0.25, 0), r * 0.42, 10), r * 0.9 } }; ST(o, s, NULL, -1, cu, 1, RECT(cx, cy, L, 2 * r, t));
        for (int j = 0; j < 2; j++) { double s1 = sgn(j); s = SN(); TBARz(s, FF(&f2, -L * 0.08, s1 * r * 0.8), FF(&f2, -L * 0.08, s1 * (r + 0.35)), 0.3, r * 0.8, r * 0.8); ST(o, s, NULL, -1, NULL, 0, DISCp(FF(&f2, -L * 0.08, s1 * r), 0.4, 20)); }
        sv = CH2(rng); double x = u(-1.0, 1.0), y = u(1.8, 2.2); pile = FF(&ff, x, sv * y);
    }
    double rb = u(0.42, 0.48); static const double n34[2] = { 3, 4 }; int n = (int)CH(rng, n34, 2); S *balls = SN();
    for (int k = 0; k < n; k++) { P q = POLp(pile, rb * 1.05 * (n == 3 ? 1 : 1.42), a + 6.2832 * k / n); x_ball(balls, q.x, q.y, rb, 0.0, 16); }
    x_ball(balls, pile.x, pile.y, rb, rb * 0.85, 16);
    ST(o, balls, NULL, -1, NULL, 0, DISCp(pile, rb * 2.3, 20));
    if (rnd() < 0.6) {
        double d1 = u(2.0, 2.4); P p = FF(&ff, -L * 0.35, sv * d1 * 1.0); double d2 = u(2.0, 2.4); P q = FF(&ff, L * 0.3, sv * d2);
        if (DIST(p, pile) > 1.6 && DIST(q, pile) > 1.6) { P pq[2] = { p, q }; S *s = SN(); TBAR(s, p, q, 0, rammer_rf, NULL, 0.6, 0, 0, 0); ST(o, s, "wood", -1, NULL, 0, BUF(LSTR(pq, 2), 0.4, 16)); }
    }
    *foot = OUT(o);
}

/* capstan */
static double bar_rf(void *c, double s) { (void)c; return s < 0.85 ? 0.26 : 0.32; }
static void capstan(double cx, double cy, double a, PyRandom *rng, SoObj *o, GG **foot) {
    so_obj_init(o, "drift"); double r = u(1.05, 1.2), h = u(3.5, 3.8); int n = 8; double t0 = u(0, 1);
    int base = UP(o, NGON(cx, cy, r + 0.75, 12, t0), 0.35, NULL);
    { P sp[6]; for (int k = 0; k < 6; k++) sp[k] = POL(cx, cy, r + 0.45, t0 + 6.2832 * k / 6); STUDS(o, sp, 6, base, 0.22, 0.2, "iron", 8); }
    GG *wh = STAR(cx, cy, r + 0.12, r - 0.08, n, t0 + 0.2);
    S *s = SN(); EXT(s, wh, h * 0.72, 0);
    { double pr[10] = { r - 0.06, 0.0, r - 0.06, h * 0.72, r + 0.25, h * 0.74, r + 0.25, h - 0.08, r * 0.8, h }; TURN(s, cx, cy, pr, 5, 24, 1.0, 0.0, 0.0); }
    EXT(s, ANN(cx, cy, r + 0.3, r - 0.1, 24), h * 0.36, 0);
    { GL l = { 0 }; for (int k = 0; k < 6; k++) GA(&l, RECTp(POL(cx, cy, r + 0.1, t0 + 6.2832 * k / 6), 0.5, 0.36, t0 + 6.2832 * k / 6)); C cu[1] = { { UL(&l), h * 0.8 } }; ST(o, s, NULL, base, cu, 1, DISC(cx, cy, r + 0.3, 20)); }
    s = SN(); EXT(s, DISC(cx, cy, r * 0.62, 18), h - 0.5 + 0.35 + 0.18, 0);
    for (int k = 0; k < 6; k++) { P q = POL(cx, cy, r * 0.42, t0 + 6.2832 * k / 6); CYL(s, q.x, q.y, 0.2, h - 0.5 + 0.35 + 0.3, 0.0, 6); }
    ST(o, s, "iron", -1, NULL, 0, DISC(cx, cy, r * 0.62, 20));
    int nb = (int)RI(rng, 1, 3);
    for (int i = 0; i < nb; i++) {
        double t = a + i * 2.1 + u(-0.3, 0.3); P p = POL(cx, cy, r + 1.0, t + 0.5); double d = u(2.9, 3.2); P q = POLp(p, d, t + 1.5708 + 0.5); P pq[2] = { p, q };
        s = SN(); TBAR(s, p, q, 0, bar_rf, NULL, 0.7, 0, 0, 0); ST(o, s, "drift", -1, NULL, 0, BUF(LSTR(pq, 2), 0.4, 16));
    }
    *foot = OUT(o);
}

/* driftwood */
typedef struct { double r; } DW;
static int trunk_rf(void *c, double s, double *out) { double r = ((DW *)c)->r; *out = r * (1.12 - 0.45 * s) * (1.0 + 0.06 * SIN(s * 23)); return SO_OK; }
static int stub_rf(void *c, double s, double *out) { (void)c; *out = 0.34 - 0.08 * s; return SO_OK; }
static int root_rf(void *c, double s, double *out) { (void)c; *out = 0.4 - 0.14 * s; return SO_OK; }
static void driftwood(double cx, double cy, double a, PyRandom *rng, SoObj *o, GG **foot) {
    F ff = FR(cx, cy, a); so_obj_init(o, "drift"); double L = u(6.9, 7.4), r = u(0.55, 0.65), bend = u(-0.6, 0.6);
    DW *dw = (DW *)xr_mem(sizeof *dw); dw->r = r;
    double x0 = -L / 2 + 1.0, ss[8]; P pts[8]; LS(ss, 0, 1, 8); for (int i = 0; i < 8; i++) pts[i] = FF(&ff, x0 + L * ss[i], bend * SIN(PI * ss[i]));
    S *ps = SN(); ROD(ps, pts, 8, 0, trunk_rf, dw, 1.85, 0, 0);
    int nk = (int)RI(rng, 1, 3);
    for (int k = 0; k < nk; k++) {
        double s = u(0.3, 0.85); int i = (int)PINT(s * 7); P p = pts[i]; double sv = CH2(rng); double t = a + sv * u(0.6, 1.2); double d = u(0.6, 1.0); P q = POLp(p, r + d, t);
        P pq[2] = { p, q }; ROD(ps, pq, 2, 0, stub_rf, NULL, 1.6 * r / 0.34, 0, 0);
    }
    P rp = pts[0]; DOME(ps, rp.x, rp.y, r * 1.5, r * 2.2, 0, 0.85, a, 12);
    nk = (int)RI(rng, 3, 5);
    for (int k = 0; k < nk; k++) { double t = a + PI + u(-1.4, 1.4); double d = u(0.6, 1.1); P q = POLp(rp, r * 1.5 + d, t); P pq[2] = { POLp(rp, r * 0.8, t), q }; ROD(ps, pq, 2, 0, root_rf, NULL, 2.0, 0, 0); }
    GL gl = { 0 }; double vs[4] = { -r * 0.5, -r * 0.15, r * 0.25, r * 0.55 };
    for (int j = 0; j < 4; j++) { P l[6]; for (int i = 1; i < 7; i++) l[i - 1] = POLp(pts[i], vs[j], a + 1.5708); GA(&gl, BUF(LSTR(l, 6), 0.06, 16)); }
    C cuts[2] = { { UL(&gl), r * 1.75 } }; int nc = 1;
    if (rnd() < 0.6) { P l[4]; for (int i = 2; i < 6; i++) { double d = u(-0.1, 0.1); l[i - 2] = POLp(pts[i], d, a + 1.5708); } cuts[1].q = BUF(LSTR(l, 4), 0.1, 16); cuts[1].fl = r * 1.4; nc = 2; }
    { GL h = { 0 }; GA(&h, BUF(LSTR(pts, 8), r + 0.6, 16)); GA(&h, DISCp(rp, r * 1.5 + 1.0, 20)); ST(o, ps, NULL, -1, cuts, nc, HULL(&h)); }
    if (rnd() < 0.4) { P p = pts[4]; S *s = SN(); TBARz(s, POLp(p, r + 0.2, a + 1.5708), POLp(p, r + 0.2, a - 1.5708), 0.22, r * 2.0, r * 2.0); ST(o, s, "rope", -1, NULL, 0, DISCp(p, r + 0.3, 20)); }
    *foot = OUT(o);
}

/* net_floats */
static void net_floats(double cx, double cy, double a, PyRandom *rng, SoObj *o, GG **foot) {
    F ff = FR(cx, cy, a); so_obj_init(o, "rope"); double L = u(4.8, 5.2), W = u(3.4, 3.8), m = u(0.62, 0.7), t = a + u(-0.1, 0.1); (void)t;
    double e = u(0.0, 0.3); GG *edge = BUF(BUF(RECT(cx, cy, L, W, a), -0.4, 16), 0.4 + e, 3);
    GL gs = { 0 }; double xs[64]; int nx = AR(xs, 64, -L / 2 + 0.2, L / 2, m);
    for (int i = 0; i < nx; i++) { double d1 = u(-0.1, 0.1); P p = FF(&ff, xs[i] + d1, -W); double d2 = u(-0.3, 0.3); GA(&gs, BARL(p, FF(&ff, xs[i] + d2, W), 0.36)); }
    nx = AR(xs, 64, -W / 2 + 0.2, W / 2, m);
    for (int i = 0; i < nx; i++) { double d1 = u(-0.1, 0.1); P p = FF(&ff, -L, xs[i] + d1); double d2 = u(-0.3, 0.3); GA(&gs, BARL(p, FF(&ff, L, xs[i] + d2), 0.36)); }
    UP(o, INT(UL(&gs), edge), 0.22, "twine");
    double hx[12]; P hr[12]; LS(hx, -L / 2 - 0.1, L / 2 + 0.1, 12); for (int i = 0; i < 12; i++) hr[i] = FF(&ff, hx[i], W / 2 - 0.15 + 0.2 * SIN(hx[i] * 1.3));
    { S *s = SN(); ROD(s, hr, 12, 0.24, NULL, NULL, 1.0, 0, 0); ST(o, s, "rope", -1, NULL, 0, BUF(LSTR(hr, 12), 0.3, 16)); }
    nx = AR(xs, 64, -L / 2 + 0.5, L / 2 - 0.3, 1.3);
    for (int i = 0; i < nx; i++) { P p = FF(&ff, xs[i], W / 2 - 0.15 + 0.2 * SIN(xs[i] * 1.3)); S *s = SN(); EXT(s, RRECT(p.x, p.y, 0.62, 0.5, a, 0.12), 0.4, 0); ST(o, s, "wood", -1, NULL, 0, DISCp(p, 0.4, 20)); }
    P cs[8]; double rs[8]; int ncs = 0; int nn = (int)RI(rng, 2, 4);
    for (int i = 0; i < nn; i++) {
        double r = 0; P c = { 0, 0 };
        for (int k = 0; k < 12; k++) {
            r = u(0.62, 0.8); double x = u(-L / 2 + 0.8, L / 2 - 0.8), y = u(-W / 2 + 0.8, W / 2 - 1.0); c = FF(&ff, x, y);
            int ok = 1; for (int j = 0; j < ncs; j++) if (!(DIST(c, cs[j]) > r + rs[j] + 0.3)) { ok = 0; break; }
            if (ok) break;
        }
        cs[ncs] = c; rs[ncs] = r; ncs++;
        S *s = SN(); x_ball(s, c.x, c.y, r, 0.0, 16); CYL(s, c.x, c.y, 0.24, r * 1.9 + 0.22, 0.0, 8); ST(o, s, "glass", -1, NULL, 0, DISCp(c, r, 20));
        s = SN(); EXT(s, ANN(c.x, c.y, r + 0.12, r - 0.12, 18), r * 0.62, 0); ST(o, s, "rope", -1, NULL, 0, DISCp(c, r + 0.1, 20));
    }
    *foot = OUT(o);
}

/* lobster_pot */
static int lob_rf(void *c, double s, double *out) { (void)c; int e = 0; double v = 1 - PWs(2 * s / 1.6, 2, &e); if (e) return SO_ERR; *out = 0.36 * sqrt(MX(0.05, v)); return SO_OK; }
static double hoop_q(void *c, double q) { double R = *(double *)c; double v = 1 - PW(2 * q - 1, 2); return 0.25 + R * 1.1 * sqrt(MX(0.0, v)); }
static double lath_z(void *c, double q) { (void)q; return *(double *)c + 0.06; }
static void lobster_pot(double cx, double cy, double a, PyRandom *rng, SoObj *o, GG **foot) {
    F ff = FR(cx, cy, a); so_obj_init(o, "wood"); double L = u(3.7, 4.0), W = u(2.5, 2.7), R = W / 2, x0 = -L / 2;
    double *Rp = (double *)xr_mem(sizeof(double)); *Rp = R;
    int b = UP(o, RECT(cx, cy, L, W, a), 0.26, NULL);
    { GL l = { 0 }; double vs[2] = { -W / 6, W / 6 }; for (int i = 0; i < 2; i++) GA(&l, BARL(FF(&ff, -L, vs[i]), FF(&ff, L, vs[i]), 0.1)); CUT(o, UL(&l), b, 0.08); }
    if (rnd() < 0.6) {
        double x = u(-0.6, 0.2), y = u(-0.3, 0.3); P c = FF(&ff, x, y); double t = a + u(-0.4, 0.4); F f2 = FR(c.x, c.y, t);
        S *s = SN(); SPIN(s, c.x, c.y, t, 1.6, lob_rf, NULL, 1.1, 0, "round");
        for (int i = 0; i < 2; i++) { P q = FF(&f2, 1.1, sgn(i) * 0.4); DOME(s, q.x, q.y, 0.3, 0.45, 0, 0.6, t, 20); }
        ST(o, s, "hide", b, NULL, 0, DISCp(c, 1.0, 20));
    }
    S *ps = SN(); int nh = (int)RI(rng, 4, 5); double ss[5]; LS(ss, 0.03, 0.97, nh);
    for (int i = 0; i < nh; i++) { double x = x0 + L * ss[i]; P l[2] = { FF(&ff, x, -R + 0.05), FF(&ff, x, R - 0.05) }; XWALL(ps, l, 2, 0.3, hoop_q, Rp); }
    int nl = (int)RI(rng, 5, 6);
    for (int k = 0; k < nl; k++) {
        double th = PI * (k + 0.5) / nl, v = R * 0.95 * COS(th); double *z = (double *)xr_mem(sizeof(double)); *z = 0.25 + R * 1.1 * SIN(th);
        P l[2] = { FF(&ff, x0 + 0.05, v), FF(&ff, -x0 - 0.05, v) }; XWALL(ps, l, 2, 0.3, lath_z, z);
    }
    for (int i = 0; i < 2; i++) { P q = FF(&ff, sgn(i) * (L / 2 - 0.1), 0); EXT(ps, ANN(q.x, q.y, 0.55, 0.32, 12), 0.7, 0); }
    ST(o, ps, "wood", -1, NULL, 0, RECT(cx, cy, L, W, a));
    double ex = u(2.9, 3.3), ey = u(-1.4, 1.4); P e = FF(&ff, L / 2 + ex, ey);
    double d = u(-0.5, 0.5); P p0 = FF(&ff, L / 2 + 0.2, d); P p1 = POLp(e, 0.7, a + PI); P pts[8];
    int np = lf_wiggle(p0.x, p0.y, p1.x, p1.y, rng, 0.6, 4, &pts[0].x);
    { S *s = SN(); ROD(s, pts, np, 0.22, NULL, NULL, 1.0, 0, 0); ST(o, s, "rope", -1, NULL, 0, BUF(LSTR(pts, np), 0.3, 16)); }
    { S *s = SN(); double pr[10] = { 0.62, 0.0, 0.7, 0.4, 0.62, 0.8, 0.3, 0.9, 0.22, 1.5 }; TURN(s, e.x, e.y, pr, 5, 14, 1.0, 0.0, 0.0); ST(o, s, "wood", -1, NULL, 0, DISCp(e, 0.7, 20)); }
    *foot = OUT(o);
}

static const XrDef DEFS[] = {
    { "coast", "made", "drift", "oars", 10.5, 0, 1, oars },
    { "coast", "made", "iron", "cannon", 10.5, 0, 1, cannon },
    { "coast", "made", "drift", "capstan", 10.5, 0, 1, capstan },
    { "coast", "debris", "drift", "driftwood", 10.5, 0, 1, driftwood },
    { "coast", "made", "glass", "net_floats", 10.5, 0, 1, net_floats },
    { "coast", "made", "twig", "lobster_pot", 10.5, 0, 1, lobster_pot },
};
void bld_sdrx_coast(void) { xr_register(DEFS, (int)(sizeof DEFS / sizeof DEFS[0])); }
