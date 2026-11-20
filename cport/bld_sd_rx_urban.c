/* bld_sd_rx_urban.c: sd_r_x_urban.py in C (see bld_sd_rx.h; xu_bar is XBAR with step 0.1). */
#include "bld_sd_rx.h"

static double sgn(int i) { return i ? 1 : -1; }
static GG *STONE(double x, double y, double r, PyRandom *rng, double aspect, int nv) { Geo *g = lf_stone(x, y, r, rng, aspect, nv, 0); GG *c; if (!g) xr_fail(); c = shp_clone(g->g); geo_unref(g); return xr_keep(c); }

/* xu_chunk(o, x, y, r, h, rng, kind="concrete") */
static int xu_chunk(SoObj *o, double x, double y, double r, double h, PyRandom *rng, const char *kind) {
    GG *g = STONE(x, y, r, rng, 0.8, 7); int b = UP(o, g, h, kind);
    double t = u(0, 3.14); CUT(o, INT(WAVY(POL(x, y, r, t), POL(x, y, r, t + PI), rng, 0.2, 3, 0.14), INS(g, 0.25)), b, 0.12);
    return b;
}

/* street_lamp: returns rect(...), o.L */
static double lamp_r1(void *c, double s) { (void)c; double Lb = 1.4, x = s * Lb, band = (fabs(x - 0.25) < 0.08 || fabs(x - 0.55) < 0.08 || fabs(x - 1.15) < 0.08) ? 0.06 : 0.0; return (s < 0.6 ? 0.62 : 0.62 - 0.25 * (s - 0.6) / 0.4) + band; }
static double lamp_r2(void *c, double s) { (void)c; return 0.3 - 0.08 * s + ((fabs(s - 0.38) < 0.03 || s > 0.93) ? 0.07 : 0.0); }
static double lamp_r3(void *c, double s) { (void)c; return s < 0.6 ? 0.24 : 0.3; }
static void street_lamp(double cx, double cy, double a, PyRandom *rng, SoObj *o, GG **foot) {
    F ff = FR(cx, cy, a); double L = u(8.0, 11.0); so_obj_init(o, "iron"); double x0 = -L / 2, xh = L / 2 - 0.9;
    { double y = u(-0.2, 0.2); P c = FF(&ff, x0 + 0.15, y); double r = u(0.95, 1.1), h = u(0.45, 0.6); xu_chunk(o, c.x, c.y, r, h, rng, "concrete"); }
    double Lb = 1.4;
    { S *s = SN(); XBAR(s, FF(&ff, x0, 0), FF(&ff, x0 + Lb, 0), 0, lamp_r1, NULL, 0.45, 0.1); ST(o, s, NULL, -1, NULL, 0, RECTp(FF(&ff, x0 + Lb / 2, 0), Lb, 1.4, a)); }
    double Ls = xh - (x0 + Lb) + 0.1;
    { S *s = SN(); XBAR(s, FF(&ff, x0 + Lb - 0.05, 0), FF(&ff, xh, 0), 0, lamp_r2, NULL, 0.45, 0.1); ST(o, s, NULL, -1, NULL, 0, RECTp(FF(&ff, x0 + Lb + Ls / 2, 0), Ls, 0.8, a)); }
    double xl = xh - u(0.7, 1.0); { S *s = SN(); EXT(s, BARL(FF(&ff, xl, -0.8), FF(&ff, xl, 0.8), 0.32), 0.32, 0); ST(o, s, NULL, -1, NULL, 0, RECTp(FF(&ff, xl, 0), 0.4, 1.7, a)); }
    P c = FF(&ff, xh + 0.75, 0); double t = a + u(-0.15, 0.15); F f2 = FR(c.x, c.y, t); double lw = u(1.25, 1.45);
    GG *lg = RECTp(c, 1.3, lw, t); double hh = u(1.2, 1.4); int lb = UP(o, lg, hh, NULL);
    GG *panes[2]; for (int i = 0; i < 2; i++) panes[i] = RECTp(FF(&f2, 0, sgn(i) * lw * 0.22), 0.85, lw * 0.32, t);
    int kp[2]; GL keep = { 0 }; for (int i = 0; i < 2; i++) { kp[i] = rnd() < 0.55; if (kp[i]) GA(&keep, panes[i]); }
    if (keep.n) CUT(o, UL(&keep), lb, 0.75);
    if (keep.n < 2) { GL l = { 0 }; for (int i = 0; i < 2; i++) if (!kp[i]) GA(&l, panes[i]); CUT(o, UL(&l), lb, 0.12); }
    { P q[4] = { FF(&f2, 0.62, -lw / 2 - 0.05), FF(&f2, 1.1, -lw * 0.2), FF(&f2, 1.1, lw * 0.2), FF(&f2, 0.62, lw / 2 + 0.05) }; int rf_ = UP(o, POLY(q, 4), 1.0, NULL); ON(o, RECTp(FF(&f2, 0.95, 0), 0.3, lw * 0.32, t), rf_, 0.15, NULL); }
    { S *s = SN(); XBAR(s, FF(&f2, 1.05, 0), FF(&f2, 1.55, 0), 0, lamp_r3, NULL, 0.6, 0.1); ST(o, s, NULL, -1, NULL, 0, DISCp(FF(&f2, 1.3, 0), 0.4, 20)); }
    int n = (int)RI(rng, 2, 4);
    for (int i = 0; i < n; i++) { double x = u(0.2, 1.6), sv = CH2(rng); double y = sv * u(1.0, 1.4); P p = FF(&ff, xh + x, y); double ro = u(0.32, 0.45), rot = u(0, 6.28); UP(o, STAR(p.x, p.y, ro, 0.16, 3, rot), 0.25, "glass"); }
    *foot = RECTp(FF(&ff, 0.8, 0), L + 3.2, 3.0, a);
}

/* park_bench */
static void park_bench(double cx, double cy, double a, PyRandom *rng, SoObj *o, GG **foot) {
    F ff = FR(cx, cy, a); double L = u(4.9, 5.8); so_obj_init(o, "wood"); double hs = u(1.3, 1.4), xe = L / 2 - 0.2;
    GG *sg = RECTp(FF(&ff, 0, 0.3), L - 0.5, 1.3, a); int s = UP(o, sg, hs, NULL);
    GL gv = { 0 }; double vs[3] = { 0.0, 0.32, 0.62 }; for (int i = 0; i < 3; i++) GA(&gv, BARL(FF(&ff, -L, vs[i]), FF(&ff, L, vs[i]), 0.13));
    int miss = rnd() < 0.3;
    CUT(o, UL(&gv), s, 0.12);
    if (miss) { double x1 = u(-L * 0.3, L * 0.1); P q = FF(&ff, x1, 0.78); double l = u(1.0, 2.0); CUT(o, RECTp(q, l, 0.34, a), s, 0.5); }
    static const double n34[3] = { 3, 3, 4 }; int nb = (int)CH(rng, n34, 3); double vb[4], tops[4]; for (int i = 0; i < nb; i++) vb[i] = -0.42 - 0.24 * i; double Hb = u(1.15, 1.3); for (int i = 0; i < nb; i++) tops[i] = hs + 0.3 + Hb * (i + 1) / nb;
    int last = -1; for (int i = 0; i < nb; i++) { double v = vb[i]; last = UP(o, RECTp(FF(&ff, 0, (v + vb[nb - 1] - 0.14) / 2), L - 0.5, v - vb[nb - 1] + 0.26, a), tops[i], NULL); }
    if (rnd() < 0.5) { double x = u(-0.4, 0.4); ON(o, RECTp(FF(&ff, x, vb[nb - 1] + 0.02), 0.8, 0.3, a), last, 0.08, "brass"); }
    double ha = u(1.85, 2.0);
    for (int i = 0; i < 2; i++) {
        double sx = sgn(i); GG *g = U2(RECTp(FF(&ff, sx * xe, (0.95 + vb[nb - 1] - 0.14) / 2), 0.38, 0.95 - vb[nb - 1] + 0.14, a), DISCp(FF(&ff, sx * xe, 0.95), 0.27, 10));
        int e = UP(o, g, ha, "iron"); ON(o, DISCp(FF(&ff, sx * xe, 0.95), 0.2, 8), e, 0.12, "iron");
        UP(o, RECTp(FF(&ff, sx * xe, vb[nb - 1] - 0.05), 0.38, 0.3, a), tops[nb - 1] + 0.2, "iron");
    }
    *foot = OUT(o);
}

/* bathtub: returns og.buffer(0.4), o.L */
static void bathtub(double cx, double cy, double a, PyRandom *rng, SoObj *o, GG **foot) {
    F ff = FR(cx, cy, a); double L = u(4.6, 5.6), W = u(2.2, 2.6), h = u(1.75, 1.95); so_obj_init(o, "iron");
    GG *og = U3(RECT(cx, cy, L - W, W, a), DISCp(FF(&ff, -(L - W) / 2, 0), W / 2, 24), DISCp(FF(&ff, (L - W) / 2, 0), W / 2, 24));
    for (int i = 0; i < 2; i++) for (int j = 0; j < 2; j++) {
        double sx = sgn(i), sy = sgn(j); P p = FF(&ff, sx * (L / 2 - 0.75), sy * (W / 2 - 0.05)); GG *fg = ELL(p.x, p.y, 0.42, 0.34, a, 10); int f = UP(o, fg, 0.55, NULL);
        GL l = { 0 }; double ks[2] = { -0.5, 0.5 }; for (int k = 0; k < 2; k++) GA(&l, BARL(POLp(p, 0.05, a + sy * 1.5708 + ks[k]), POLp(p, 0.5, a + sy * 1.5708 + ks[k]), 0.12)); CUT(o, UL(&l), f, 0.1);
    }
    int b = UP(o, og, h, NULL); ON(o, DIF(og, INS(og, 0.32)), b, 0.12, NULL);
    double sv = CH2(rng); GG *inner = U3(RECTp(FF(&ff, -sv * 0.25, 0), L - W - 0.5, W - 0.85, a), DISCp(FF(&ff, -sv * 0.25 - (L - W - 0.5) / 2, 0), W / 2 - 0.42, 20), DISCp(FF(&ff, -sv * 0.25 + (L - W - 0.5) / 2, 0), W / 2 - 0.42, 20));
    CUT(o, inner, b, h - 0.45);
    double tx = sv * (L / 2 - 0.42);
    ON(o, U2(DISCp(FF(&ff, tx, -0.5), 0.22, 8), DISCp(FF(&ff, tx, 0.5), 0.22, 8)), b, 0.38, NULL); ON(o, CAPL(FF(&ff, tx, 0), FF(&ff, tx - sv * 0.55, 0), 0.26), b, 0.3, NULL);
    ON(o, U2(BARL(FF(&ff, tx - 0.15, -0.5), FF(&ff, tx + 0.15, -0.5), 0.16), BARL(FF(&ff, tx - 0.15, 0.5), FF(&ff, tx + 0.15, 0.5), 0.16)), b, 0.48, NULL);
    if (rnd() < 0.3) { int n = (int)RI(rng, 2, 4); for (int i = 0; i < n; i++) { double x = u(-L * 0.25, L * 0.2), y = u(-0.4, 0.4); P p = FF(&ff, x, y); double r = u(0.4, 0.55); GG *g = STONE(p.x, p.y, r, rng, 0.7, 5); double hh = u(0.8, 1.1); UP(o, g, hh, "concrete"); } }
    *foot = BUF(og, 0.4, 16);
}

/* post_box: returns rect(...), o.L */
static double post_rf(void *c, double s) {
    double r = *(double *)c;
    if (s < 0.1) return r * 1.1;
    if (s < 0.8) return r;
    if (s < 0.86) return r * 1.1;
    return r * MX(0.45, sqrt(MX(0.0, 1 - PW((s - 0.86) / 0.16, 2))) * 1.1);
}
static void post_box(double cx, double cy, double a, PyRandom *rng, SoObj *o, GG **foot) {
    F ff = FR(cx, cy, a); double L = u(3.4, 4.2), r = u(0.92, 1.02); so_obj_init(o, "iron"); double k = 0.65, top = (1 + k) * r; double *rp = (double *)xr_mem(sizeof(double)); *rp = r;
    S *ps = SN(); XBAR(ps, FF(&ff, -L / 2, 0), FF(&ff, L / 2, 0), 0, post_rf, rp, k, 0.1);
    double xs = L * 0.25; EXT(ps, RECTp(FF(&ff, xs + 0.28, 0), 0.24, 1.15, a), top + 0.08, 0);
    EXT(ps, RECTp(FF(&ff, -L * 0.12 + L * 0.27, 0), 0.5, 0.7, a), top + 0.06, 0);
    GG *door = RECTp(FF(&ff, -L * 0.12, 0), L * 0.4, 1.0 * r, a);
    C cuts[4]; cuts[0].q = RECTp(FF(&ff, xs, 0), 0.26, 1.0, a); cuts[0].fl = top - 0.32; cuts[1].q = BUF(EXTR(door), 0.07, 16); cuts[1].fl = top - 0.24;
    { P q = FF(&ff, -L * 0.12, 0); cuts[2].q = ANN(q.x, q.y, 0.3, 0.17, 10); cuts[2].fl = top - 0.1; }
    cuts[3].q = U2(BARL(FF(&ff, -L * 0.4, -2), FF(&ff, -L * 0.4, 2), 0.14), BARL(FF(&ff, L * 0.3, -2), FF(&ff, L * 0.3, 2), 0.14)); cuts[3].fl = top - 0.3;
    ST(o, ps, NULL, -1, cuts, 4, RECT(cx, cy, L, 2 * r, a));
    { double y = u(-0.2, 0.2); P c = FF(&ff, -L / 2 + 0.1, y); double rr = u(0.95, 1.1), hh = u(0.5, 0.65); xu_chunk(o, c.x, c.y, rr, hh, rng, "concrete"); }
    int n = (int)RI(rng, 1, 3);
    for (int i = 0; i < n; i++) { double x = u(0.9, 1.8), y = u(-1.2, 1.2); P p = FF(&ff, L / 2 + x, y); double rot = u(0, 3); UP(o, RECTp(p, 1.2, 0.85, rot), 0.25, "paper"); }
    *foot = RECTp(FF(&ff, 0.8, 0), L + 3.6, 2 * r + 1.6, a);
}

/* kerbstones: returns rect(...), o.L */
static void kerbstones(double cx, double cy, double a, PyRandom *rng, SoObj *o, GG **foot) {
    F ff = FR(cx, cy, a); int n = (int)RI(rng, 3, 5); so_obj_init(o, "concrete");
    for (int i = 0; i < n; i++) {
        double y = u(-0.45, 0.45); P c = FF(&ff, (i - (n - 1) / 2.0) * 2.1, y); double t = a + u(-0.3, 0.3); F f2 = FR(c.x, c.y, t); double Lk = 1.9, Wk = u(0.95, 1.05), h = u(0.7, 0.85);
        double e0 = rnd() < 0.5 ? -Lk / 2 : Lk / 2; int brk = rnd() < 0.45;
        P pts[8] = { FF(&f2, -Lk / 2, -Wk / 2), FF(&f2, Lk / 2, -Wk / 2), FF(&f2, Lk / 2, Wk / 2), FF(&f2, -Lk / 2, Wk / 2) }; int np = 4;
        if (brk) {
            double sx = e0 > 0 ? 1 : -1, vs[4]; P jag[4]; LS(vs, Wk / 2, -Wk / 2, 4); for (int k = 0; k < 4; k++) { double d = u(0.0, 0.35); jag[k] = FF(&f2, sx * (Lk / 2 - d), vs[k]); }
            np = 0;
            if (sx > 0) { pts[np++] = FF(&f2, -Lk / 2, -Wk / 2); for (int k = 3; k >= 0; k--) pts[np++] = jag[k]; pts[np++] = FF(&f2, -Lk / 2, Wk / 2); }
            else { pts[np++] = FF(&f2, Lk / 2, -Wk / 2); pts[np++] = FF(&f2, Lk / 2, Wk / 2); for (int k = 0; k < 4; k++) pts[np++] = jag[k]; }
        }
        GG *g = BUF(POLY(pts, np), 0, 16);
        if (!VALID(g) || AREA(g) < 0.5) g = RECTp(c, Lk, Wk, t);
        double sv = CH2(rng); int b = UP(o, g, h - 0.2, NULL); int top = ON(o, INT(g, RECTp(FF(&f2, 0, -sv * 0.12), Lk + 1, Wk - 0.24, t)), b, 0.2, NULL);
        if (rnd() < 0.5) { double d = u(-0.5, 0.5); P p = FF(&f2, d, -Wk); d = u(-0.5, 0.5); P q = FF(&f2, d, Wk); CUT(o, WAVY(p, q, rng, 0.15, 3, 0.13), top, 0.14); }
        if (rnd() < 0.5) { double x = u(-0.5, 0.5); P p = FF(&f2, x, sv * (Wk / 2 + 0.15)); double r = u(0.3, 0.42); GG *sg = STONE(p.x, p.y, r, rng, 0.6, 5); double hh = u(0.3, 0.42); UP(o, sg, hh, "earth"); }
    }
    *foot = RECT(cx, cy, n * 2.1 + 0.4, 2.6, a);
}

/* window_frame: returns rect(...), o.L */
static void window_frame(double cx, double cy, double a, PyRandom *rng, SoObj *o, GG **foot) {
    F ff = FR(cx, cy, a); double L = u(3.6, 4.6), W = u(2.8, 3.4); GG *R = RECT(cx, cy, L, W, a); so_obj_init(o, "wood");
    double sv = CH2(rng); UP(o, RECTp(FF(&ff, 0, sv * (W / 2 + 0.05)), L + 0.6, 0.6, a), 0.55, NULL);
    int fr = UP(o, DIF(R, INS(R, 0.42)), 0.42, NULL); CUT(o, BUF(EXTR(INS(R, 0.21)), 0.06, 16), fr, 0.1);
    UP(o, RECT(cx, cy, L - 0.6, 0.42, a), 0.48, NULL);
    { GG *d = DISC(cx, cy, 0.24, 8); int k = UP(o, RECT(cx, cy, 0.62, 0.42, a), 0.48, NULL); ON(o, d, k, 0.12, "brass"); }
    static const double n12[2] = { 1, 2 }; int nb = (int)CH(rng, n12, 2); double xs[4]; LS(xs, -L / 2, L / 2, nb + 2);
    GL bars = { 0 }; for (int i = 1; i <= nb; i++) GA(&bars, RECTp(FF(&ff, xs[i], 0), 0.26, W - 0.6, a));
    UP(o, UL(&bars), 0.32, NULL);
    GL b2 = { 0 }; for (int i = 0; i < bars.n; i++) GA(&b2, bars.v[i]); GA(&b2, RECT(cx, cy, L, 0.42, a));
    GG *pane = DIF(INS(R, 0.42), BUF(UL(&b2), 0.01, 16));
    { GL qs = { 0 }; POLYS(pane, &qs);
      for (int i = 0; i < qs.n; i++) {
          GG *q = qs.v[i];
          if (rnd() < 0.4) {
              P m = CENT(q); double bx[4]; BOUNDS(q, bx); double c0 = rnd() < 0.5 ? bx[0] : bx[2]; double c1 = rnd() < 0.5 ? bx[1] : bx[3];
              P z[3]; z[0] = PT(c0, c1); double d1 = u(0.4, 0.8), d2 = u(0.1, 0.3); z[1] = PT(c0 + (m.x - c0) * d1, c1 + (m.y - c1) * d2); d1 = u(0.1, 0.3); d2 = u(0.4, 0.8); z[2] = PT(c0 + (m.x - c0) * d1, c1 + (m.y - c1) * d2);
              GG *sh = INT(BUF(POLY(z, 3), 0.12, 16), BUF(q, 0.1, 16));
              if (!EMPTY(sh) && AREA(sh) > 0.2) UP(o, sh, 0.25, "glass");
          }
      } }
    for (int i = 0; i < 2; i++) {
        double s2 = sgn(i);
        if (rnd() < 0.6) { double y = u(-W * 0.3, W * 0.3); P p = FF(&ff, s2 * L / 2, y); double x2 = u(0.4, 0.8), y2 = u(-W * 0.4, W * 0.4); P e = FF(&ff, s2 * (L / 2 + x2), y2); P pts[8]; int np = lf_wiggle(p.x, p.y, e.x, e.y, rng, 0.2, 3, &pts[0].x);
            S *s = SN(); ROD(s, pts, np, 0.18, NULL, NULL, 1.0, 0, 0); P pe[2] = { p, e }; ST(o, s, "rope", -1, NULL, 0, BUF(LSTR(pe, 2), 0.3, 16)); }
    }
    for (int i = 0; i < 2; i++) { double s2 = sgn(i); int n = (int)RI(rng, 0, 2); for (int k = 0; k < n; k++) { double x = u(-L, L) * 0.4, y = u(0.8, 1.4); P p = FF(&ff, x, s2 * (W / 2 + y)); double ro = u(0.35, 0.5), rot = u(0, 6.28); UP(o, STAR(p.x, p.y, ro, 0.17, 3, rot), 0.22, "glass"); } }
    *foot = RECT(cx, cy, L + 1.8, W + 3.6, a);
}

/* crossarm: returns hull(foot), o.L */
static double cross_rf(void *c, double s) { double r = *(double *)c; return r * (s < 0.85 ? 1.0 : 0.85); }
static void crossarm(double cx, double cy, double a, PyRandom *rng, SoObj *o, GG **foot) {
    F ff = FR(cx, cy, a); double L = u(5.0, 6.5); so_obj_init(o, "wood"); GL fo = { 0 }; GA(&fo, RECT(cx, cy, L, 1.2, a));
    if (rnd() < 0.65) {
        double sv = CH2(rng), lp = u(1.6, 2.2), r = u(0.5, 0.58); double *rp = (double *)xr_mem(sizeof(double)); *rp = r;
        P p = FF(&ff, 0, sv * 0.25); double d = u(-0.3, 0.3); P q = FF(&ff, d, sv * (0.25 + lp));
        { S *s = SN(); XBAR(s, p, q, 0, cross_rf, rp, 0.35, 0.1); ST(o, s, NULL, -1, NULL, 0, RECTp(FF(&ff, 0, sv * (0.25 + lp / 2)), 2 * r, lp, a)); }
        { GL l = { 0 }; for (int i = 0; i < 2; i++) { double sx = sgn(i); GA(&l, BARL(FF(&ff, sx * L * 0.28, sv * 0.2), FF(&ff, sx * 0.35, sv * (0.25 + lp * 0.6)), 0.3)); } S *s = SN(); EXT(s, UL(&l), 0.3, 0); ST(o, s, "iron", -1, NULL, 0, RECTp(FF(&ff, 0, sv * 0.8), L * 0.6, 1.4, a)); }
    }
    double ha = u(0.42, 0.48); int arm = UP(o, RECT(cx, cy, L, 0.62, a), ha, NULL);
    { P st[2] = { FF(&ff, -0.25, 0), FF(&ff, 0.25, 0) }; STUDS(o, st, 2, arm, 0.16, 0.1, "iron", 6); }
    double x4[4] = { -L * 0.42, -L * 0.17, L * 0.17, L * 0.42 }, x3[3] = { -L * 0.38, 0.0, L * 0.38 }; int four = rnd() < 0.6; const double *xs = four ? x4 : x3; int nx = four ? 4 : 3;
    for (int i = 0; i < nx; i++) {
        double x = xs[i]; P c = FF(&ff, x, 0);
        if (rnd() < 0.15) { STUDS(o, &c, 1, arm, 0.14, 0.2, "iron", 6); continue; }
        { double pr[32] = { 0.34, 0.0, 0.34, ha + 0.16, 0.24, ha + 0.22, 0.24, ha + 0.32, 0.28, ha + 0.36 }; int np = 10; np += CRV(pr + np, 0.28, 0.12, ha + 0.36, ha + 0.58, 1, 4); S *s = SN(); TURN(s, c.x, c.y, pr, np / 2, 12, 1.0, 0.0, 0.0); ST(o, s, "glass", -1, NULL, 0, DISCp(c, 0.34, 20)); }
        if (rnd() < 0.6) {
            double s2 = CH2(rng), d1 = u(-1.2, 1.2), d2 = u(2.0, 2.9); P e = FF(&ff, x + d1, s2 * d2); P w0 = FF(&ff, x, s2 * 0.35); P pts[8]; int np = lf_wiggle(w0.x, w0.y, e.x, e.y, rng, 0.5, 4, &pts[0].x);
            S *s = SN(); ROD(s, pts, np, 0.2, NULL, NULL, 1.0, 0, 0); ST(o, s, "wire", -1, NULL, 0, DISCp(e, 0.3, 20)); GA(&fo, DISCp(e, 0.3, 20));
        }
    }
    *foot = HULL(&fo);
}

/* chalk_outline: returns body.convex_hull, lay */
static void chalk_outline(double cx, double cy, double a, PyRandom *rng, SoObj *o, GG **foot) {
    F ff = FR(cx, cy, a); double k = u(0.85, 1.1), sv = CH2(rng); so_obj_init(o, "seam");
    GG *tor; { GL h = { 0 }; GA(&h, ELL(PXY(FF(&ff, 0.55 * k, 0)), 0.55 * k, 0.85 * k, a, 14)); GA(&h, ELL(PXY(FF(&ff, -0.75 * k, 0)), 0.5 * k, 0.65 * k, a, 14)); tor = HULL(&h); }
    double y = u(-0.25, 0.25); GG *hd = DISCp(FF(&ff, 1.9 * k, y), 0.6 * k, 14);
    double x1 = u(1.0, 1.5) * k; P e1 = FF(&ff, x1, sv * 1.6 * k); double x2 = u(1.9, 2.4) * k; double y2 = sv * u(2.0, 2.4) * k; P h1 = FF(&ff, x2, y2);
    GG *arm1 = U3(CAPL(FF(&ff, 1.0 * k, sv * 0.7 * k), e1, 0.48 * k), CAPL(e1, h1, 0.42 * k), DISCp(h1, 0.32 * k, 10));
    double x3 = u(0.3, 0.9) * k; double y3 = -sv * u(1.9, 2.3) * k; P h2 = FF(&ff, x3, y3); GG *arm2 = U2(CAPL(FF(&ff, 0.9 * k, -sv * 0.7 * k), h2, 0.48 * k), DISCp(h2, 0.32 * k, 10));
    GL bl = { 0 }; GA(&bl, tor); GA(&bl, hd); GA(&bl, CAPL(FF(&ff, 1.2 * k, 0), FF(&ff, 1.5 * k, 0), 0.4 * k)); GA(&bl, arm1); GA(&bl, arm2);
    for (int i = 0; i < 2; i++) {
        double s2 = sgn(i); double a1 = -u(1.8, 2.1) * k; double b1 = s2 * u(0.35, 0.8) * k; P kn = FF(&ff, a1, b1); double a2 = -u(2.9, 3.4) * k; double b2 = s2 * u(0.5, 1.3) * k; P ft = FF(&ff, a2, b2);
        GA(&bl, CAPL(FF(&ff, -1.0 * k, s2 * 0.3 * k), kn, 0.58 * k)); GA(&bl, CAPL(kn, ft, 0.5 * k)); { P q = POLp(ft, 0.15, a + s2 * 1.0); GA(&bl, ELL(q.x, q.y, 0.28 * k, 0.45 * k, a + s2 * 0.5, 10)); }
    }
    GG *body = UL(&bl);
    GG *g = DIF(BUF(body, 0.16, 16), BUF(body, -0.12, 16));
    lyl_push_own(&o->L, ly_gcut(shp_clone(g), 0.22, "seam"));
    double bx = u(0.9, 1.3); P bc = FF(&ff, 2.0 * k, -sv * bx * k); double br = u(0.6, 0.8); GG *st = BLOB(bc.x, bc.y, br, rng, 4);
    if (rnd() < 0.7) lyl_push_own(&o->L, ly_gcut(shp_clone(DIF(st, BUF(body, 0.3, 16))), 0.08, "seam"));
    int n = (int)RI(rng, 2, 3);
    for (int i = 0; i < n; i++) {
        double x = u(-2.4, 2.0) * k, s1 = CH2(rng); double yy = s1 * u(1.3, 1.7) * k; P p = FF(&ff, x, yy);
        if (CONTAINS(BUF(body, 0.5, 16), PTG(p.x, p.y))) continue;
        double t = a + u(0, 3.14); S *s = SN(); RIDGE(s, p.x, p.y, t, 0.6, 0.62, 0.52, 0.0); SoLayer *ly; xr_ok(sd_ST(s, "paper", 0, 0, NULL, 0, &ly)); lyl_push_own(&o->L, ly);
    }
    *foot = CHULL(body);
}

/* road_sign */
static void road_sign(double cx, double cy, double a, PyRandom *rng, SoObj *o, GG **foot) {
    F ff = FR(cx, cy, a); double L = u(4.6, 6.4); int k = (int)RI(rng, 0, 2); P c = FF(&ff, L / 2, 0); double R = u(1.4, 1.8); so_obj_init(o, "iron");
    double tp = k == 1 ? a + u(0, 1) : a + u(-0.4, 0.4);
    GG *plate = k == 0 ? NGON(c.x, c.y, R, 8, a + 0.39) : (k == 1 ? NGON(c.x, c.y, R * 1.15, 3, tp) : RECTp(c, R * 2.2, R * 1.2, tp));
    double y = u(-0.8, 0.8); P bend = FF(&ff, -L * 0.1, y); P pts[3] = { FF(&ff, -L / 2, 0), bend, FF(&ff, L / 2 - 0.3, 0) };
    { P q = FF(&ff, -L / 2 - 0.2, 0); double r = u(0.75, 0.9), h = u(0.85, 1.05); xu_chunk(o, q.x, q.y, r, h, rng, "concrete"); }
    { S *s = SN(); ROD(s, pts, 3, 0.24, NULL, NULL, 1.25, 0, 0); ST(o, s, NULL, -1, NULL, 0, BUF(LSTR(pts, 3), 0.3, 16)); }
    int b = UP(o, plate, 0.3, NULL);
    if (rnd() < 0.7) {
        ON(o, DIF(plate, INS(plate, 0.28)), b, 0.14, NULL);
        F f2 = FR(c.x, c.y, k != 2 ? a : tp);
        if (k == 0) { GL l = { 0 }; for (int i = 0; i < 4; i++) GA(&l, RECTp(FF(&f2, 0, (i - 1.5) * 0.52), 0.85, 0.34, a)); ON(o, UL(&l), b, 0.14, NULL); }
        else if (k == 1) { P tc = c; ON(o, U2(BARL(POLp(tc, 0.1, tp + PI), POLp(tc, 0.62, tp + PI), 0.28), DISCp(POLp(tc, 0.38, tp), 0.17, 8)), b, 0.14, NULL); }
        else { P q[3] = { FF(&f2, R * 0.25, -0.55), FF(&f2, R * 0.85, 0), FF(&f2, R * 0.25, 0.55) }; ON(o, U2(BARL(FF(&f2, -R * 0.75, 0), FF(&f2, R * 0.3, 0), 0.4), POLY(q, 3)), b, 0.14, NULL); }
    } else {
        F f2 = FR(c.x, c.y, a); GL l = { 0 }; double vs[2] = { -0.45, 0.45 }; for (int i = 0; i < 2; i++) GA(&l, INT(RECTp(FF(&f2, 0, vs[i]), R * 1.8, 0.32, a), INS(plate, 0.2))); ON(o, UL(&l), b, 0.2, NULL);
        P st[2] = { FF(&f2, 0, -0.45), FF(&f2, 0, 0.45) }; STUDS(o, st, 2, b, 0.2, 0.32, NULL, 8);
    }
    GG *ex[2] = { plate, RECT(cx, cy, L, 1.8, a) };
    *foot = OUTX(o, ex, 2);
}

/* dustbin: returns hull([...]), o.L */
typedef struct { double r; } Bin;
static double bin_rf(void *c, double s) { double r = ((Bin *)c)->r, hoop = (fabs(s - 0.18) < 0.035 || fabs(s - 0.5) < 0.035 || fabs(s - 0.82) < 0.035) ? 0.07 : 0.0; return r * (0.9 + 0.1 * s) + hoop + (s > 0.96 ? 0.06 : 0.0); }
static void dustbin(double cx, double cy, double a, PyRandom *rng, SoObj *o, GG **foot) {
    F ff = FR(cx, cy, a); double L = u(3.0, 3.6), r = u(1.05, 1.18); so_obj_init(o, "tin"); P c;
    Bin *bc = (Bin *)xr_mem(sizeof *bc); bc->r = r;
    if (rnd() < 0.65) {
        double x = u(1.4, 2.2), y = u(-1.5, 1.5); c = FF(&ff, L / 2 + x, y);
        S *ps = SN(); XBAR(ps, FF(&ff, -L / 2, 0), FF(&ff, L / 2, 0), 0, bin_rf, bc, 0.55, 0.1);
        for (int i = 0; i < 2; i++) { double sv = sgn(i); P q = FF(&ff, L * 0.15, sv * (r + 0.12)); EXT(ps, DIF(ANN(q.x, q.y, 0.34, 0.14, 10), RECTp(FF(&ff, L * 0.15, 0), 2, 2 * r, a)), r * 0.7, 0); }
        C cu[1] = { { U1(RECTp(FF(&ff, L / 2 - 0.35, 0), 0.9, 2 * r * 0.72, a)), r * 0.85 } }; ST(o, ps, NULL, -1, cu, 1, RECT(cx, cy, L, 2 * r + 0.3, a));
        int n = (int)RI(rng, 2, 4);
        for (int i = 0; i < n; i++) {
            double x1 = u(0.1, 1.0), y1 = u(-1.0, 1.0); P p = FF(&ff, L / 2 + x1, y1); double kk = rnd();
            if (kk < 0.4) { double l = u(0.7, 1.0), w = u(0.5, 0.7), rot = u(0, 3); UP(o, RECTp(p, l, w, rot), 0.22, "paper"); }
            else if (kk < 0.7) { double t = u(0, 6.28); S *s = SN(); XBAR(s, p, POLp(p, 0.75, t), 0.3, NULL, NULL, 0.6, 0.1); ST(o, s, NULL, -1, NULL, 0, DISCp(p, 0.8, 20)); }
            else { double rr = u(0.35, 0.5); GG *g = STONE(p.x, p.y, rr, rng, 0.7, 6); double hh = u(0.3, 0.45); UP(o, g, hh, "sack"); }
        }
    } else {
        P b0 = FF(&ff, -L * 0.55, 0); double x = u(-0.2, 0.2), y = u(-0.6, 0.6); c = FF(&ff, L * 0.58 + x, y); double hb = u(3.0, 3.3);
        int b = UP(o, DISCp(b0, r, 24), hb, NULL); ON(o, ANN(b0.x, b0.y, r - 0.05, r - 0.35, 24), b, 0.12, NULL); CUT(o, DISCp(b0, r - 0.42, 20), b, 0.8);
        int n = (int)RI(rng, 2, 3);
        for (int i = 0; i < n; i++) { double rr = u(r + 0.8, r + 1.8), t = a + u(-0.8, 0.8); P p = POLp(b0, rr, t); double l = u(0.6, 0.9), w = u(0.45, 0.6), rot = u(0, 3); UP(o, RECTp(p, l, w, rot), 0.22, "paper"); }
    }
    double lr = r + 0.12;
    { double pr[32] = { lr, 0.0, lr, 0.22, lr - 0.2, 0.26 }; int np = 6; np += CRV(pr + np, lr - 0.2, 0.55, 0.26, 0.6, 1, 4); S *s = SN(); TURN(s, c.x, c.y, pr, np / 2, 24, 1.0, 0.0, 0.0); ST(o, s, NULL, -1, NULL, 0, DISCp(c, lr, 20)); }
    double tl = a + u(0, 3.14);
    { GL l = { 0 }; GA(&l, BARL(POLp(c, 0.62, tl), POLp(c, 0.62, tl + PI), 0.3)); for (int k = 0; k < 2; k++) GA(&l, DISCp(POLp(c, 0.62, tl + k * PI), 0.22, 8)); S *s = SN(); EXT(s, UL(&l), 0.85, 0); ST(o, s, NULL, -1, NULL, 0, DISCp(c, 0.7, 20)); }
    GL h = { 0 }; GA(&h, RECT(cx, cy, L, 2 * r + 0.3, a)); GA(&h, DISCp(c, r + 0.3, 20)); *foot = HULL(&h);
}

/* hubcap_and_bumper: returns hull([...]), o.L */
static void hubcap_and_bumper(double cx, double cy, double a, PyRandom *rng, SoObj *o, GG **foot) {
    F ff = FR(cx, cy, a); double R = u(1.1, 1.4), x = u(-1.5, 1.5); P c = FF(&ff, x, 2.4); double L = u(5.0, 6.5); so_obj_init(o, "tin");
    double e1 = -0.7, e2 = -0.7 * u(0.2, 2.0); P path[4] = { FF(&ff, -L / 2, e1), FF(&ff, -L * 0.3, 0), FF(&ff, L * 0.3, 0), FF(&ff, L / 2, e2) };
    int b = UP(o, LINE(path, 4, 0.62), 0.6, NULL); CUT(o, BARL(FF(&ff, -L * 0.3, 0), FF(&ff, L * 0.3, 0), 0.14), b, 0.1);
    { GL l = { 0 }; for (int i = 0; i < 2; i++) GA(&l, RRECTp(FF(&ff, sgn(i) * L * 0.2, -0.05), 0.5, 0.85, a, 0.18)); UP(o, UL(&l), 0.82, NULL); }
    if (rnd() < 0.75) {
        double xp = u(-0.4, 0.4); GG *pg = RECTp(FF(&ff, xp, -0.85), 1.8, 0.62, a); int p = UP(o, pg, 0.3, "paper");
        GL l = { 0 }; for (int i = 0; i < 7; i++) if (i != 3) GA(&l, RECTp(FF(&ff, xp - 0.66 + 0.22 * i, -0.85), 0.14, 0.32, a)); ON(o, UL(&l), p, 0.08, NULL);
    }
    { GL l = { 0 }; for (int i = 0; i < 2; i++) GA(&l, DISCp(FF(&ff, sgn(i) * L * 0.42, 0), 0.12, 6)); CUT(o, UL(&l), b, 0.15); }
    int h = UP(o, DISCp(c, R, 28), 0.3, NULL); ON(o, DISCp(c, R - 0.2, 26), h, 0.12, NULL);
    int m = ON(o, DISCp(c, R * 0.48, 18), h, 0.32, NULL); CUT(o, ANN(c.x, c.y, R * 0.3, R * 0.3 - 0.12, 14), m, 0.08);
    { static const double ch[3] = { 6, 8, 10 }; double dv = CH(rng, ch, 3); double ts[16]; int nt = AR(ts, 16, 0, 6.2, 6.2832 / dv); GL l = { 0 }; for (int i = 0; i < nt; i++) GA(&l, BARL(POLp(c, R * 0.55, ts[i]), POLp(c, R * 0.82, ts[i]), 0.22)); CUT(o, UL(&l), h, 0.22); }
    if (rnd() < 0.4) { double t = u(0, 6.28); P q = POLp(c, R * 0.75, t); double rot = u(0, 3); CUT(o, ELL(q.x, q.y, 0.4, 0.25, rot, 8), h, 0.18); }
    GL hl = { 0 }; GA(&hl, RECT(cx, cy, L, 2.0, a)); GA(&hl, DISCp(c, R, 20)); *foot = HULL(&hl);
}

/* suitcase: returns hull([...]), o.L */
static int suit_case(SoObj *o, PyRandom *rng, double x, double y, double t, double L, double W, double h, int par) {
    F f2 = FR(x, y, t); GG *g = RRECT(x, y, L, W, t, 0.25);
    int b = par < 0 ? UP(o, g, h, NULL) : ON(o, g, par, h, NULL);
    ON(o, DIF(g, INS(g, 0.2)), b, 0.1, NULL); CORNERS(o, &f2, L - 0.1, W - 0.1, b, t, 0.5, 0.14, "brass");
    { double xs[2] = { -L * 0.3, L * 0.3 }; TABS(o, &f2, xs, 2, -W / 2 + 0.38, b, t, 0.36, 0.32, 0.16, "brass"); }
    if (rnd() < 0.55) { double xs[1]; xs[0] = L * u(-0.15, 0.15); STRAPS(o, &f2, xs, 1, W - 1.4, b, t, 0.36, 0.1, NULL); }
    if (rnd() < 0.6) { double x1 = u(-L * 0.25, L * 0.25), y1 = u(0.0, W * 0.2); P p = FF(&f2, x1, y1); double tt = t + u(-0.5, 0.5); ON(o, RRECTp(p, 0.55, 0.38, tt, 0.08), b, 0.08, "paper"); }
    return b;
}
static void suitcase(double cx, double cy, double a, PyRandom *rng, SoObj *o, GG **foot) {
    F ff = FR(cx, cy, a); double L = u(2.8, 3.4), W = u(1.9, 2.3), h = u(0.95, 1.1); so_obj_init(o, "leather");
    double x = u(-1, 1); P c = FF(&ff, x, W / 2 + 1.3); double bs = u(0, 6.28);
    suit_case(o, rng, cx, cy, a, L, W, h, -1);
    UP(o, U3(CAPL(FF(&ff, -0.42, -W / 2 - 0.22), FF(&ff, 0.42, -W / 2 - 0.22), 0.3), DISCp(FF(&ff, -0.42, -W / 2 - 0.1), 0.2, 8), DISCp(FF(&ff, 0.42, -W / 2 - 0.1), 0.2, 8)), h * 0.75, NULL);
    if (rnd() < 0.4) { double x1 = u(-0.15, 0.15), y1 = u(-0.1, 0.1); P q = FF(&ff, x1, y1); double t = a + u(-0.18, 0.18), hh = u(0.7, 0.85); suit_case(o, rng, q.x, q.y, t, L * 0.78, W * 0.78, hh, 0); }
    F f3 = FR(c.x, c.y, bs);
    GG *sole; { GL l = { 0 }; GA(&l, ELL(PXY(FF(&f3, 0.25, 0)), 0.6, 0.38, bs, 12)); GA(&l, ELL(PXY(FF(&f3, -0.55, 0)), 0.32, 0.3, bs, 10)); sole = HULL(&l); }
    int sb = UP(o, sole, 0.2, "tar"); int up_; { GL l = { 0 }; GA(&l, ELL(PXY(FF(&f3, 0.1, 0)), 0.5, 0.3, bs, 12)); GA(&l, DISCp(FF(&f3, -0.5, 0), 0.24, 10)); up_ = ON(o, HULL(&l), sb, 0.36, NULL); }
    CUT(o, ELL(PXY(FF(&f3, -0.25, 0)), 0.24, 0.16, bs, 8), up_, 0.2);
    GL hl = { 0 }; GA(&hl, RECT(cx, cy, L, W + 0.8, a)); GA(&hl, DISCp(c, 1.0, 20)); *foot = HULL(&hl);
}

/* storm_drain: returns rect(...), o.L */
static void storm_drain(double cx, double cy, double a, PyRandom *rng, SoObj *o, GG **foot) {
    F ff = FR(cx, cy, a); double L = u(4.0, 5.2); so_obj_init(o, "concrete");
    GCUT(o, RECTp(FF(&ff, 0, -0.3), L, 0.8, a), 0.08, "water");
    GG *kg = RECTp(FF(&ff, 0, 0.45), L, 1.1, a); double hh = u(0.7, 0.8); int k = UP(o, kg, hh, NULL); ON(o, RECTp(FF(&ff, 0, 0.6), L, 0.75, a), k, 0.14, NULL);
    { double l = L * u(0.5, 0.62); CUT(o, RECTp(FF(&ff, 0, 0.05), l, 0.42, a), k, 0.5); }
    { int m = (int)RI(rng, 4, 6); double xs[32]; int nx = AR(xs, 32, -L / 2 + 0.6, -L / 2 + 0.6 + 0.3 * m, 0.3); GL l = { 0 }; for (int i = 0; i < nx; i++) GA(&l, RECTp(FF(&ff, xs[i], 0.7), 0.14, 0.32, a)); CUT(o, UL(&l), k, 0.08); }
    double Lg = L * u(0.55, 0.68); GG *gg_ = RECTp(FF(&ff, 0, -1.05), Lg, 1.3, a);
    UP(o, DIF(gg_, INS(gg_, 0.28)), 0.25, "iron");
    GL sl = { 0 }; { double xs[32]; int nx = AR(xs, 32, -Lg / 2 + 0.5, Lg / 2 - 0.35, 0.48); for (int i = 0; i < nx; i++) GA(&sl, RECTp(FF(&ff, xs[i], -1.05), 0.26, 0.82, a)); }
    UP(o, DIF(INS(gg_, 0.28), BUF(UL(&sl), 0.01, 16)), 0.18 + 0.07, "iron");
    GCUT(o, UL(&sl), 0.7, "slot");
    UP(o, RECTp(FF(&ff, Lg / 2 - 0.1, -1.6), 0.5, 0.3, a), 0.28, "iron");
    int n = (int)RI(rng, 1, 3);
    for (int i = 0; i < n; i++) { double x = u(-Lg / 2, Lg / 2), y = u(-0.3, 0.3); P p = FF(&ff, x, -1.05 + y); double rot = u(0, 3); UP(o, ELL(p.x, p.y, 0.36, 0.2, rot, 8), 0.3, "grass"); }
    *foot = RECTp(FF(&ff, 0, -0.4), L, 2.9, a);
}

/* brick_pallet */
static int pyint_floordiv(double a, double b) { double q; xr_ok(px_floordiv_d(a, b, &q)); return (int)PINT(q); }
static void brick_pallet(double cx, double cy, double a, PyRandom *rng, SoObj *o, GG **foot) {
    F ff = FR(cx, cy, a); int n = (int)RI(rng, 3, 4), m = (int)RI(rng, 2, 3); so_obj_init(o, "wood");
    double Lp = n * 1.25 + 0.8, Wp = m * 0.75 + 0.8; GG *pg = RECT(cx, cy, Lp, Wp, a); int p = UP(o, pg, 0.6, NULL);
    double Bx = Lp - 0.5, By = Wp - 0.4, bl = 1.05, bw = 0.5, ch = 0.32, gap = 0.1;
    { GL l = { 0 }; double xs[64]; int nx = AR(xs, 64, -Lp / 2 + 0.45, Lp / 2, 0.55); for (int i = 0; i < nx; i++) GA(&l, BARL(FF(&ff, xs[i], -Wp), FF(&ff, xs[i], Wp), 0.12)); CUT(o, DIF(UL(&l), RECT(cx, cy, Bx, By, a)), p, 0.12); }
    int nc = (int)RI(rng, 4, 7); double xs = u(0.35, 0.65), sx = CH2(rng), sy = CH2(rng);
    for (int k = 0; k < nc; k++) {
        double top = 0.6 + ch * (k + 1); int left = nc - k; GG *keep;
        if (left == 2) keep = RECTp(FF(&ff, sx * Bx * (1 - xs) / 2, 0), Bx * xs, By, a);
        else if (left == 1) keep = RECTp(FF(&ff, sx * Bx * (1 - xs * 0.5) / 2, sy * By * 0.25), Bx * xs * 0.5, By * 0.5 + 0.1, a);
        else keep = RECT(cx, cy, Bx + 0.01, By + 0.01, a);
        GL br = { 0 };
        if (k % 2 == 0) {
            int nj = pyint_floordiv(By, bw), ni = pyint_floordiv(Bx, bl);
            for (int j = 0; j < nj; j++) for (int i = 0; i < ni; i++) GA(&br, RECTp(FF(&ff, -Bx / 2 + (i + 0.5) * (Bx / ni), -By / 2 + (j + 0.5) * (By / nj)), Bx / ni - gap, By / nj - gap, a));
        } else {
            int nj = pyint_floordiv(Bx, bw), ni0 = pyint_floordiv(By, bl), ni = ni0 > 1 ? ni0 : 1;
            for (int j = 0; j < nj; j++) for (int i = 0; i < ni; i++) GA(&br, RECTp(FF(&ff, -Bx / 2 + (j + 0.5) * (Bx / nj), -By / 2 + (i + 0.5) * (By / ni)), Bx / nj - gap, By / ni - gap, a));
        }
        GL got = { 0 };
        for (int i = 0; i < br.n; i++) { GG *ce = xr_keep(shp_centroid(br.v[i])); int w = shp_within(ce, keep); if (w < 0) xr_fail(); if (w && (left > 2 || rnd() > 0.12)) GA(&got, br.v[i]); }
        if (got.n) UP(o, UL(&got), top, "brick");
    }
    int nn = (int)RI(rng, 1, 3);
    for (int i = 0; i < nn; i++) {
        double x = u(-Lp * 0.35, Lp * 0.35), s1 = CH2(rng); double y = s1 * (Wp / 2 + u(0.45, 0.7)); P q = FF(&ff, x, y); double t = a + u(-0.6, 0.6);
        GG *g = RECTp(q, bl * (i == 1 ? 0.55 : 1.0), bw, t); double hh = rnd() < 0.6 ? ch + 0.02 : bw; UP(o, g, hh, "brick");
    }
    *foot = OUT(o);
}

static const XrDef DEFS[] = {
    { "urban", "made", "iron", "street_lamp", 10.5, 0, 0, street_lamp },
    { "urban", "made", "iron", "park_bench", 10.5, 0, 0, park_bench },
    { "urban", "made", "iron", "bathtub", 10.5, 0, 0, bathtub },
    { "urban", "made", "iron", "post_box", 10.5, 0, 0, post_box },
    { "urban", "debris", "concrete", "kerbstones", 10.5, 0, 0, kerbstones },
    { "urban", "made", "wood", "window_frame", 10.5, 0, 0, window_frame },
    { "urban", "made", "wood", "crossarm", 10.5, 0, 0, crossarm },
    { "urban", "marks", "seam", "chalk_outline", 10.5, 0, 0, chalk_outline },
    { "urban", "made", "iron", "road_sign", 10.5, 0, 0, road_sign },
    { "urban", "made", "tin", "dustbin", 10.5, 0, 0, dustbin },
    { "urban", "made", "tin", "hubcap_and_bumper", 10.5, 0, 0, hubcap_and_bumper },
    { "urban", "made", "leather", "suitcase", 10.5, 0, 0, suitcase },
    { "urban", "made", "concrete", "storm_drain", 10.5, 1, 0, storm_drain },
    { "urban", "made", "brick", "brick_pallet", 10.5, 0, 0, brick_pallet },
};
void bld_sdrx_urban(void) { xr_register(DEFS, (int)(sizeof DEFS / sizeof DEFS[0])); }
