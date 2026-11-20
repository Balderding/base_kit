/* bld_sd_rx_maahl.c: sd_r_x_maahl.py in C (see bld_sd_rx.h; xm_bar is XBAR with step 0.11). */
#include "bld_sd_rx.h"

static double sgn(int i) { return i ? 1 : -1; }

/* xm_glyphs(ff, xs, vs, a, rng, keep=0.8): NULL = None */
static GG *xm_glyphs(const F *ff, const double *xs, int nx, const double *vs, int nv, double a, PyRandom *rng, double keep) {
    GL gs = { 0 };
    for (int i = 0; i < nx; i++) for (int j = 0; j < nv; j++) {
        double x = xs[i], v = vs[j];
        if (rnd() > keep) continue;
        int k = (int)RI(rng, 0, 3); P c = FF(ff, x, v);
        if (k == 0) { static const double ch[3] = { 0.0, 0.6, -0.6 }; double t = a + CH(rng, ch, 3); GA(&gs, RECTp(c, 0.26, 0.6, t)); }
        else if (k == 1) { GA(&gs, RECTp(FF(ff, x - 0.2, v), 0.22, 0.56, a)); GA(&gs, RECTp(FF(ff, x + 0.2, v), 0.22, 0.56, a)); }
        else if (k == 2) GA(&gs, ANN(c.x, c.y, 0.32, 0.1, 8));
        else { P l[3] = { FF(ff, x - 0.22, v - 0.25), FF(ff, x + 0.2, v - 0.25), FF(ff, x + 0.2, v + 0.25) }; GA(&gs, LINE(l, 3, 0.22)); }
    }
    return gs.n ? UL(&gs) : NULL;
}
static GG *STONE(double x, double y, double r, PyRandom *rng, double aspect, int nv) { Geo *g = lf_stone(x, y, r, rng, aspect, nv, 0); GG *c; if (!g) xr_fail(); c = shp_clone(g->g); geo_unref(g); return xr_keep(c); }

/* buried_head */
static void buried_head(double cx, double cy, double a, PyRandom *rng, SoObj *o, GG **foot) {
    F ff = FR(cx, cy, a); so_obj_init(o, "ruin"); double s = u(1.05, 1.3);
    P hq[6] = { FF(&ff, -2.3 * s, -1.4 * s), FF(&ff, 0.3 * s, -2.1 * s), FF(&ff, 1.9 * s, -1.9 * s), FF(&ff, 1.9 * s, 1.9 * s), FF(&ff, 0.3 * s, 2.1 * s), FF(&ff, -2.3 * s, 1.4 * s) };
    GG *hd = BUF(BUF(POLY(hq, 6), -0.3, 16), 0.3, 6);
    GG *fc = ELL(PXY(FF(&ff, 0.15 * s, 0)), 1.6 * s, 1.12 * s, a, 28); double hh = u(1.0, 1.2); int b = UP(o, hd, hh, NULL);
    { GL l = { 0 }; double ks[3] = { 0.42, 0.84, 1.26 }; for (int i = 0; i < 3; i++) GA(&l, BUF(EXTR(BUF(fc, ks[i], 16)), 0.12, 16)); CUT(o, INT(UL(&l), INS(hd, 0.1)), b, 0.2); }
    hh = u(0.6, 0.75); int f = ON(o, fc, b, hh, NULL);
    { GL l = { 0 }; for (int i = 0; i < 2; i++) GA(&l, ELL(PXY(FF(&ff, -0.3 * s, sgn(i) * 0.58 * s)), 0.27 * s, 0.4 * s, a, 12)); if (rnd() < 0.4) GA(&l, ELL(PXY(FF(&ff, -1.05 * s, 0)), 0.3 * s, 0.22 * s, a, 12)); CUT(o, UL(&l), f, 0.35); }
    { GL l = { 0 }; for (int i = 0; i < 2; i++) { double sv = sgn(i); GA(&l, RECTp(FF(&ff, -0.72 * s, sv * 0.6 * s), 0.46, 0.95 * s, a + sv * 0.15)); } ON(o, UL(&l), f, 0.2, NULL); }
    { P q[4] = { FF(&ff, -0.6 * s, -0.23), FF(&ff, 0.4 * s, -0.38), FF(&ff, 0.4 * s, 0.38), FF(&ff, -0.6 * s, 0.23) }; GG *g = POLY(q, 4); hh = u(0.45, 0.55); ON(o, g, f, hh, NULL); }
    int lp = ON(o, ELL(PXY(FF(&ff, 0.95 * s, 0)), 0.3 * s, 0.6 * s, a, 12), f, 0.2, NULL); CUT(o, RECTp(FF(&ff, 0.95 * s, 0), 0.16, 0.8 * s, a), lp, 0.15);
    { GG *g = ELL(PXY(FF(&ff, -1.45 * s, 0)), 0.32, 0.26, a, 10); ON(o, g, b, rnd() < 0.7 ? 0.75 : 0.4, NULL); }
    GG *bg = RECTp(FF(&ff, 1.75 * s, 0), 0.9 * s, 0.8 * s, a); hh = u(1.1, 1.3); int bd = UP(o, bg, hh, NULL);
    { GL l = { 0 }; double xs[3] = { -0.25 * s, 0.0, 0.25 * s }; for (int i = 0; i < 3; i++) GA(&l, BARL(FF(&ff, 1.75 * s + xs[i], -1), FF(&ff, 1.75 * s + xs[i], 1), 0.12)); CUT(o, UL(&l), bd, 0.12); }
    if (rnd() < 0.3) { double d = u(-1, 1); P p = FF(&ff, d, -2.2 * s); d = u(-1, 1); P q = FF(&ff, d, 2.2 * s); CUT(o, WAVY(p, q, rng, 0.4, 4, 0.3), f, 0.35); }
    double x = u(2.0, 2.4) * s, y = u(-0.8, 0.8); P c = FF(&ff, x, y); double r = u(1.5, 2.0), h = u(1.1, 1.4), sq = u(0.6, 0.8), t = a + 1.57 + u(-0.4, 0.4);
    S *ss = SN(); DOME(ss, c.x, c.y, r, h, 0, sq, t, 20); ST(o, ss, "sand", -1, NULL, 0, DISCp(c, 1.5, 20));
    *foot = OUT(o);
}

/* obelisk_tip */
static void obelisk_tip(double cx, double cy, double a, PyRandom *rng, SoObj *o, GG **foot) {
    F ff = FR(cx, cy, a); double L = u(5.5, 7.0), w0 = u(1.9, 2.3), w1 = w0 * 0.74, Lt = u(1.5, 1.9), h = w1 * u(0.9, 1.0); so_obj_init(o, "obelisk"); double x0 = -L / 2, x1 = L / 2 - Lt;
    P sp[7]; { double vs[5]; LS(vs, -w0 / 2, w0 / 2, 5); for (int i = 0; i < 5; i++) { double d = u(-0.3, 0.3); sp[i] = FF(&ff, x0 + d, vs[i]); } } sp[5] = FF(&ff, x1, w1 / 2); sp[6] = FF(&ff, x1, -w1 / 2);
    int b = UP(o, POLY(sp, 7), h, NULL);
    double Lp = x1 - x0 - 1.2, xm = (x0 + x1) / 2 + 0.15; GROOVE(o, RECTp(FF(&ff, xm, 0), Lp, w1 - 0.7, a), b, 0.2, 0.18);
    { double xs[64], v0[1] = { 0.0 }; int nx = AR(xs, 64, xm - Lp / 2 + 0.6, xm + Lp / 2 - 1.5, 0.85); CUT(o, xm_glyphs(&ff, xs, nx, v0, 1, a, rng, 0.9), b, 0.2); }
    CUT(o, BUF(EXTR(RRECTp(FF(&ff, xm + Lp / 2 - 0.75, 0), 1.0, w1 - 1.05, a, 0.25)), 0.08, 16), b, 0.15);
    { double sv = CH2(rng); P q[3]; double d = u(0.6, 1.4); q[0] = FF(&ff, x0 + d, sv * w0); d = u(1.6, 2.4); q[1] = FF(&ff, x0 + d, sv * w0); q[2] = FF(&ff, x0 + 1.5, sv * (w0 / 2 - 0.45)); GG *g = POLY(q, 3); d = u(0.4, 0.6); CUT(o, g, b, d); }
    S *ps = SN(); double FH[4][2] = { { 1.0, 0.36 }, { 0.68, 0.62 }, { 0.4, 0.84 }, { 0.16, 1.0 } };
    for (int i = 0; i < 4; i++) { double f = FH[i][0]; P q[3] = { FF(&ff, x1 + 0.06, -w1 / 2 * f), FF(&ff, x1 + 0.06 + Lt * (0.3 + 0.7 * f), 0), FF(&ff, x1 + 0.06, w1 / 2 * f) }; EXT(ps, POLY(q, 3), h * FH[i][1], 0); }
    { P q[3] = { FF(&ff, x1, -w1 / 2), FF(&ff, L / 2, 0), FF(&ff, x1, w1 / 2) }; ST(o, ps, NULL, -1, NULL, 0, POLY(q, 3)); }
    if (rnd() < 0.97) {
        double l2 = u(1.4, 2.4), d1 = u(0.6, 1.0), d2 = u(-0.4, 0.4); P c = FF(&ff, x0 - l2 / 2 - d1, d2); double t = a + u(-0.3, 0.3); F f2 = FR(c.x, c.y, t); double w2 = w0 * 1.04;
        P q[8]; double vs[4]; LS(vs, -w2 / 2, w2 / 2, 4); for (int i = 0; i < 4; i++) { double d = u(-0.25, 0.25); q[i] = FF(&f2, -l2 / 2 + d, vs[i]); }
        LS(vs, w2 / 2, -w2 / 2, 4); for (int i = 0; i < 4; i++) { double d = u(-0.25, 0.25); q[4 + i] = FF(&f2, l2 / 2 + d, vs[i]); }
        GG *g = POLY(q, 8); double hh = h * u(0.9, 1.0); int b2 = UP(o, g, hh, NULL);
        { GL l = { 0 }; for (int i = 0; i < 2; i++) { double sv = sgn(i); GA(&l, BARL(FF(&f2, -l2, sv * (w2 / 2 - 0.45)), FF(&f2, l2, sv * (w2 / 2 - 0.45)), 0.2)); } CUT(o, UL(&l), b2, 0.18); }
        { double xs[64], v0[1] = { 0.0 }; int nx = AR(xs, 64, -l2 / 2 + 0.6, l2 / 2 - 0.4, 0.85); CUT(o, xm_glyphs(&f2, xs, nx, v0, 1, t, rng, 0.9), b2, 0.2); }
    }
    *foot = OUT(o);
}

/* sundial */
static void sundial(double cx, double cy, double a, PyRandom *rng, SoObj *o, GG **foot) {
    double R = u(2.0, 2.6); so_obj_init(o, "ruin");
    GG *g = NGON(cx, cy, R + 0.55, 8, a + 0.39); double hh = u(0.55, 0.7); int b0 = UP(o, g, hh, NULL);
    g = NGON(cx, cy, R + 0.2, 8, a + 0.39); hh = u(0.5, 0.6); int b = ON(o, g, b0, hh, NULL);
    int dl = ON(o, DISC(cx, cy, R - 0.1, 32), b, 0.3, NULL);
    { GL l = { 0 }; GA(&l, ANN(cx, cy, R * 0.6, R * 0.6 - 0.2, 24)); for (int k = 0; k < 11; k++) { double t = a + 0.95 + k * (6.2832 - 1.9) / 10; GA(&l, BARL(POL(cx, cy, R * (k % 2 == 0 ? 0.66 : 0.78), t), POL(cx, cy, R * 0.92, t), 0.24)); }
      CUT(o, DIF(UL(&l), RECTp(POL(cx, cy, R * 0.45, a), R, 0.9, a)), dl, 0.2); }
    if (rnd() < 0.35) CUT(o, DIF(WAVY(POL(cx, cy, R * 1.1, a + 2.0), POL(cx, cy, R * 1.1, a + 4.6), rng, 0.4, 4, 0.3), DISC(cx, cy, 0.5, 20)), dl, 0.3);
    g = RECTp(POL(cx, cy, R * 0.43, a), R * 0.82, 0.5, a); hh = u(0.45, 0.55); ON(o, g, dl, hh, NULL);
    g = RECTp(POL(cx, cy, R * 0.3, a), R * 0.56, 0.5, a); hh = u(0.95, 1.1); ON(o, g, dl, hh, NULL);
    g = RECTp(POL(cx, cy, R * 0.17, a), R * 0.3, 0.5, a); hh = u(1.35, 1.5); ON(o, g, dl, hh, NULL);
    if (rnd() < 0.4) { double t = a + u(1.5, 4.7); P c = POL(cx, cy, R + 0.6, t); double r = u(1.2, 1.6), h = u(0.7, 1.0); S *s = SN(); DOME(s, c.x, c.y, r, h, 0, 0.6, t + 1.57, 20); ST(o, s, "sand", -1, NULL, 0, DISCp(c, 0.9, 20)); }
    *foot = OUT(o);
}

/* amphorae */
static double amph_rf(void *c, double s) {
    double r = *(double *)c, ring = (fabs(s - 0.3) < 0.012 || fabs(s - 0.4) < 0.012 || fabs(s - 0.5) < 0.012) ? -0.04 : 0.0;
    if (s < 0.07) return 0.3;
    if (s < 0.6) return 0.3 + (r - 0.3) * PW(SIN((s - 0.07) / 0.53 * 1.5708), 0.8) + ring;
    if (s < 0.76) return r - (r - 0.36) * PW((s - 0.6) / 0.16, 1.4);
    return s < 0.9 ? 0.36 : 0.55;
}
static void amphorae(double cx, double cy, double a, PyRandom *rng, SoObj *o, GG **foot) {
    so_obj_init(o, "clay"); int n = (int)RI(rng, 1, 3); int stand = rnd() < 0.35;
    for (int i = 0; i < n; i++) {
        P c = POL(cx, cy, n == 1 ? 0.0 : 1.25, a + 1.57 + i * 6.2832 / n); double b = a + u(-0.5, 0.5); b = b + (rnd() < 0.3 ? PI : 0); double L = u(3.8, 4.6), r = u(0.85, 1.0); F f2 = FR(c.x, c.y, b);
        if (stand && i == n - 1) {
            double hb = u(1.0, 1.2), hn = hb + r * 1.1;
            { double sq = u(0.75, 0.95); S *s = SN(); DOME(s, c.x, c.y, r + 1.0, hb * 0.75, 0, sq, b, 20); ST(o, s, "sand", -1, NULL, 0, DISCp(c, r + 0.8, 20)); }
            double pr[32] = { r, 0.0, r, hb }; int np = 4; np += CRV(pr + np, r, 0.36, hb, hn, 1, 6); pr[np++] = 0.36; pr[np++] = hn + 0.6; pr[np++] = 0.52; pr[np++] = hn + 0.65; pr[np++] = 0.52; pr[np++] = hn + 0.85;
            S *s = SN(); TURN(s, c.x, c.y, pr, np / 2, 24, 1.0, 0.0, 0.0);
            for (int j = 0; j < 2; j++) { double sv = sgn(j); EXT(s, CAPL(FF(&f2, 0, sv * 0.4), FF(&f2, 0, sv * (r * 0.7 + 0.15)), 0.36), hn + 0.4, 0); }
            C cu[1] = { { DISCp(c, 0.3, 10), hn + 0.45 } }; ST(o, s, NULL, -1, cu, 1, DISCp(c, r, 20));
            continue;
        }
        double *rp = (double *)xr_mem(sizeof(double)); *rp = r;
        { GL l = { 0 }; for (int j = 0; j < 2; j++) { double sv = sgn(j); P q[4] = { FF(&f2, L * 0.12, sv * r * 0.5), FF(&f2, L * 0.2, sv * (r * 0.5 + 0.5)), FF(&f2, L * 0.36, sv * (r * 0.5 + 0.4)), FF(&f2, L * 0.4, sv * 0.3) }; GA(&l, LINE(q, 4, 0.42)); }
          S *s = SN(); EXT(s, UL(&l), r * 1.25, 0); ST(o, s, NULL, -1, NULL, 0, RECTp(c, L, 2 * r + 0.6, b)); }
        { S *s = SN(); XBAR(s, FF(&f2, -L / 2, 0), FF(&f2, L / 2, 0), 0, amph_rf, rp, 0.75, 0.11); C cu[1] = { { DISCp(FF(&f2, L / 2 - 0.08, 0), 0.25, 8), 0.5 } }; ST(o, s, NULL, -1, cu, 1, RECTp(c, L, 2 * r, b)); }
    }
    int m = (int)RI(rng, 2, 4);
    for (int i = 0; i < m; i++) { double rr = u(2.6, 3.4); double t = a + u(-0.6, 0.6) + (i % 2) * PI; P p = POL(cx, cy, rr, t); double r = u(0.45, 0.65); GG *g = STONE(p.x, p.y, r, rng, 0.7, 4); S *s = SN(); EXT(s, g, 0.32, 0); ST(o, s, NULL, -1, NULL, 0, DISCp(p, 0.6, 20)); }
    *foot = OUT(o);
}

/* arch_fragment */
static void arch_fragment(double cx, double cy, double a, PyRandom *rng, SoObj *o, GG **foot) {
    double R = u(3.2, 4.4), w = u(1.4, 1.7), span = u(1.4, 2.2); int n = (int)PINT(span * R / 1.45); if (n < 3) n = 3; so_obj_init(o, "ruin"); int kq = n / 2; double a0 = a - span / 2;
    for (int k = 0; k < n; k++) {
        if (k != kq && n > 3 && rnd() < 0.12) continue;
        double t0 = a0 + span * k / n + 0.04, t1 = a0 + span * (k + 1) / n - 0.04, dr = k != kq ? u(-0.2, 0.2) : 0.0, ex = k == kq ? 0.3 : 0.0;
        double rr = u(0, 0.15), tt = u(0, 6.28); P c = POL(cx, cy, rr, tt);
        P q[10]; xr_ok(sd_arc(c.x, c.y, R + ex + dr, t0, t1, 4, &q[0].x)); xr_ok(sd_arc(c.x, c.y, R - w + dr, t1, t0, 4, &q[5].x));
        GG *g = POLY(q, 10); double hh = u(1.15, 1.4) + ex * 0.8; int b = UP(o, g, hh, NULL);
        int pn = ON(o, INS(g, 0.2), b, 0.14, NULL);
        { GL l = { 0 }; double fs[2] = { 0.3, 0.7 }; for (int i = 0; i < 2; i++) { P ar[6]; xr_ok(sd_arc(c.x, c.y, R - w * fs[i] + dr, t0 - 0.1, t1 + 0.1, 5, &ar[0].x)); GA(&l, LINE(ar, 6, 0.2)); } CUT(o, UL(&l), pn, 0.16); }
        if (k == kq) { P m = POLp(c, R - w * 0.5, (t0 + t1) / 2); ON(o, DISCp(m, 0.32, 10), pn, 0.22, NULL); }
        else if (rnd() < 0.3) { P z[3]; z[0] = POLp(c, R + dr + 0.3, t0 - 0.05); double d = u(0.4, 0.7); z[1] = POLp(c, R + dr - d, t0 - 0.05); d = u(0.1, 0.2); z[2] = POLp(c, R + dr + 0.3, t0 + d); GG *cg = POLY(z, 3); d = u(0.3, 0.5); CUT(o, cg, b, d); }
    }
    int m = (int)RI(rng, 0, 2);
    for (int i = 0; i < m; i++) { double d = u(0.8, 1.6); double t = a + u(-span / 2, span / 2); P p = POL(cx, cy, R - w - d, t); double r = u(0.4, 0.55), h = u(0.45, 0.6); S *s = SN(); ROCK(s, p.x, p.y, r, h, rng, 0, 0.0, 9); ST(o, s, NULL, -1, NULL, 0, DISCp(p, 0.5, 20)); }
    *foot = OUT(o);
}

/* mosaic */
static void mosaic(double cx, double cy, double a, PyRandom *rng, SoObj *o, GG **foot) {
    F ff = FR(cx, cy, a); double L = u(4.4, 5.6), W = L * u(0.75, 1.0); so_obj_init(o, "pavers"); GG *g0 = RECT(cx, cy, L, W, a);
    double sx = CH2(rng), sy = CH2(rng); P bc = FF(&ff, sx * L / 2, sy * W / 2); double br = u(1.2, 1.8); GG *bite = BLOB(bc.x, bc.y, br, rng, 4); GG *g = DIF(g0, bite);
    if (shp_type(g) != GEOS_POLYGON) { GL ps = { 0 }; POLYS(g, &ps); if (!ps.n) xr_fail(); int bi = 0; for (int i = 1; i < ps.n; i++) if (AREA(ps.v[i]) > AREA(ps.v[bi])) bi = i; g = ps.v[bi]; }
    double hh = u(0.42, 0.5); int b = UP(o, g, hh, NULL); double Rm = MN(L, W) / 2 - 0.75; static const double ks[3] = { 4, 8, 24 }; int k = (int)CH(rng, ks, 3);
    double t0 = a + ((k == 4 && rnd() < 0.5) ? 0.785 : 0.0);
    GL fig = { 0 }; GA(&fig, BUF(EXTR(INS(g0, 0.42)), 0.11, 16)); { double fs[2] = { 1.0, 0.5 }; for (int i = 0; i < 2; i++) GA(&fig, BUF(EXTR(NGON(cx, cy, Rm * fs[i], k, t0)), 0.11, 16)); }
    for (int j = 0; j < 8; j++) { double t = t0 + 6.2832 * j / 8 + 0.39; GA(&fig, BARL(POL(cx, cy, Rm * 0.56, t), POL(cx, cy, Rm * 0.86, t), 0.22)); }
    CUT(o, UL(&fig), b, 0.17); ON(o, DISC(cx, cy, Rm * 0.2, 12), b, 0.14, NULL);
    GG *field = DIF(INS(g0, 0.62), BUF(NGON(cx, cy, Rm * 1.0, k, t0), 0.15, 16));
    { double st = 0.36; GL l = { 0 }; double xs[64]; int nx = AR(xs, 64, -L / 2, L / 2, st); for (int i = 0; i < nx; i++) GA(&l, BARL(FF(&ff, xs[i], -W), FF(&ff, xs[i], W), 0.1)); nx = AR(xs, 64, -W / 2, W / 2, st); for (int i = 0; i < nx; i++) GA(&l, BARL(FF(&ff, -L, xs[i]), FF(&ff, L, xs[i]), 0.1));
      CUT(o, INT(UL(&l), field), b, 0.08); }
    int m = (int)RI(rng, 3, 6);
    for (int i = 0; i < m; i++) { double d1 = u(-0.3, 1.2), d2 = u(-0.3, 1.2); P p = FF(&ff, sx * (L / 2 - d1), sy * (W / 2 - d2)); double rot = u(0, 1.5); GG *r = RECTp(p, 0.5, 0.5, rot); double h = u(0.28, 0.36); S *s = SN(); EXT(s, r, h, 0); ST(o, s, NULL, -1, NULL, 0, DISCp(p, 0.5, 20)); }
    *foot = OUT(o);
}

/* sphinx_paw */
static void sphinx_paw(double cx, double cy, double a, PyRandom *rng, SoObj *o, GG **foot) {
    F ff = FR(cx, cy, a); double L = u(4.0, 4.8), W = u(2.9, 3.5), h = u(1.7, 2.0); so_obj_init(o, "ruin"); double tw = W / 4;
    P q[7]; { double vs[5]; LS(vs, -W / 2 - 0.5, W / 2 + 0.5, 5); for (int i = 0; i < 5; i++) { double d = u(-0.3, 0.3); q[i] = FF(&ff, -L / 2 - 0.2 + d, vs[i]); } } q[5] = FF(&ff, L / 2 + 1.8, W / 2 + 0.5); q[6] = FF(&ff, L / 2 + 1.8, -W / 2 - 0.5);
    GG *pg = POLY(q, 7); double hh = u(0.5, 0.6); int pl = UP(o, pg, hh, NULL);
    CUT(o, INT(BARL(FF(&ff, L / 2 + 1.55, -W), FF(&ff, L / 2 + 1.55, W), 0.14), pg), pl, 0.12);
    GG *ag = INT(RRECTp(FF(&ff, -0.75, 0), L - 1.5, W - 0.1, a, 0.5), INS(pg, 0.2)); int arm = ON(o, ag, pl, h, NULL);
    ON(o, RECTp(FF(&ff, -L * 0.28, 0), 0.62, W - 0.5, a), arm, 0.2, NULL);
    { GL l = { 0 }; for (int i = 0; i < 3; i++) GA(&l, BARL(FF(&ff, -L * 0.05, (i - 1) * tw), FF(&ff, L / 2, (i - 1) * tw), 0.26)); CUT(o, UL(&l), arm, 0.35); }
    { S *s = SN(); for (int i = 0; i < 4; i++) { P c = FF(&ff, L / 2 - 0.3, (i - 1.5) * tw); double dh = h * u(0.85, 0.95); DOME(s, c.x, c.y, 1.05, dh, 0, (tw / 2 - 0.02) / 1.05, a, 20); } ST(o, s, NULL, pl, NULL, 0, NULL); }
    { GL l = { 0 }; for (int i = 0; i < 4; i++) { P z[4] = { FF(&ff, L / 2 + 0.9, (i - 1.5) * tw - 0.27), FF(&ff, L / 2 + 1.5, (i - 1.5) * tw - 0.2), FF(&ff, L / 2 + 1.5, (i - 1.5) * tw + 0.2), FF(&ff, L / 2 + 0.9, (i - 1.5) * tw + 0.27) }; GA(&l, POLY(z, 4)); } ON(o, UL(&l), pl, 0.45, NULL); }
    *foot = OUT(o);
}

/* stele */
static void stele(double cx, double cy, double a, PyRandom *rng, SoObj *o, GG **foot) {
    F ff = FR(cx, cy, a); double L = u(4.6, 5.8), W = u(2.6, 3.2), h = u(1.05, 1.3); so_obj_init(o, "ruin"); double xt = L / 2 - W / 2;
    GG *g = U2(RECTp(FF(&ff, -W / 4, 0), L - W / 2, W, a), DISCp(FF(&ff, xt, 0), W / 2, 24)); P ct = FF(&ff, xt, 0); double rs = W * 0.17;
    GL dec = { 0 }; GA(&dec, BUF(EXTR(INS(g, 0.36)), 0.11, 16));
    { double ts[5]; LS(ts, -1.2, 1.2, 5); for (int i = 0; i < 5; i++) GA(&dec, BARL(POLp(ct, rs + 0.25, a + ts[i]), POLp(ct, rs + 0.55, a + ts[i]), 0.2)); }
    GA(&dec, BARL(FF(&ff, xt - rs - 0.5, -W / 2 + 0.6), FF(&ff, xt - rs - 0.5, W / 2 - 0.6), 0.2));
    GG *gl; { double xs[64], vs[64]; int nx = AR(xs, 64, -L / 2 + 0.95, xt - rs - 0.9, 0.85), nv = AR(vs, 64, -W / 2 + 0.95, W / 2 - 0.7, 0.85); gl = xm_glyphs(&ff, xs, nx, vs, nv, a, rng, 0.85); }
    GG *wings; { GL l = { 0 }; GA(&l, DISCp(ct, rs, 14)); for (int i = 0; i < 2; i++) { double sv = sgn(i); P z[4] = { POLp(ct, rs * 0.6, a + sv * 1.5708 + 0.5 * sv), POLp(ct, W * 0.42, a + sv * 1.5708 + 0.25 * sv), POLp(ct, W * 0.4, a + sv * 1.5708 - 0.15 * sv), POLp(ct, rs * 0.6, a + sv * 1.5708 - 0.5 * sv) }; GA(&l, POLY(z, 4)); } wings = UL(&l); }
    GG *parts[2] = { g, NULL }; int np = 1;
    if (rnd() < 0.4) {
        double xs = u(-L * 0.25, L * 0.05); double d1 = u(-0.4, 0.4); P p = FF(&ff, xs + d1, -W); double d2 = u(-0.4, 0.4); P q = FF(&ff, xs + d2, W);
        GL ps = { 0 }; POLYS(DIF(g, WAVY(p, q, rng, 0.3, 4, 0.1)), &ps);
        int ord[64]; int n = ps.n < 64 ? ps.n : 64; for (int i = 0; i < n; i++) ord[i] = i;
        for (int i = 1; i < n; i++) for (int j = i; j > 0 && -AREA(ps.v[ord[j]]) < -AREA(ps.v[ord[j - 1]]); j--) { int t = ord[j]; ord[j] = ord[j - 1]; ord[j - 1] = t; }
        np = n < 2 ? n : 2; for (int i = 0; i < np; i++) parts[i] = ps.v[ord[i]];
        if (np == 2) {
            double dd = u(0.5, 0.9); P dv = POL(0, 0, dd, a + PI); GG *ctp = PTG(ct.x, ct.y);
            P c1 = CENT(parts[1]), c0 = CENT(parts[0]);
            if (DISTG(PTG(c1.x, c1.y), ctp) > DISTG(PTG(c0.x, c0.y), ctp)) { double rot = u(-0.12, 0.12); parts[1] = ROT(TRANS(parts[1], dv.x, dv.y), rot, SHP_ORIGIN_POINT, c1.x, c1.y, 1); }
        }
    }
    for (int i = 0; i < np; i++) {
        GG *q = parts[i]; int b = UP(o, q, h, NULL);
        { GL l = { 0 }; for (int j = 0; j < dec.n; j++) GA(&l, dec.v[j]); if (gl) GA(&l, gl); CUT(o, UL(&l), b, 0.2); }
        if (CONTAINS(q, PTG(ct.x, ct.y))) {
            int w_ = ON(o, INT(wings, INS(q, 0.4)), b, 0.18, NULL);
            GL l = { 0 }; for (int j = 0; j < 2; j++) { double sv = sgn(j); GA(&l, BARL(POLp(ct, rs + 0.1, a + sv * 1.5708), POLp(ct, W * 0.38, a + sv * 1.5708), 0.1)); } CUT(o, UL(&l), w_, 0.08);
        }
    }
    *foot = OUT(o);
}

/* fallen_capital */
static double cap_rf(void *c, double s) { double r = *(double *)c; return r * (s < 0.3 ? 1.0 : (s < 0.4 ? 1.16 : 1.0 + 0.62 * PW((s - 0.4) / 0.6, 1.5))); }
static void fallen_capital(double cx, double cy, double a, PyRandom *rng, SoObj *o, GG **foot) {
    F ff = FR(cx, cy, a); double L = u(3.2, 4.0), r = u(0.95, 1.15); so_obj_init(o, "column"); double *rp = (double *)xr_mem(sizeof(double)); *rp = r;
    double x0 = -L / 2, x4 = x0 + (L - 0.3) * 0.45; C cuts[6]; int nc = 0;
    { double vs[3] = { -r * 0.5, 0.0, r * 0.5 }; for (int i = 0; i < 3; i++) { cuts[nc].q = BARL(FF(&ff, -L / 2 - 0.2, vs[i]), FF(&ff, -L / 2 + (L - 0.3) * 0.28, vs[i]), 0.24); cuts[nc].fl = r * 1.12; nc++; } }
    { double vs[3] = { -r * 0.45, 0.0, r * 0.45 }; for (int i = 0; i < 3; i++) { double v = vs[i]; P q[4] = { FF(&ff, x4, v - 0.12), FF(&ff, L / 2 - 0.4, v * 1.5 - 0.15), FF(&ff, L / 2 - 0.4, v * 1.5 + 0.15), FF(&ff, x4, v + 0.12) }; cuts[nc].q = POLY(q, 4); cuts[nc].fl = r * 1.3; nc++; } }
    { S *s = SN(); XBAR(s, FF(&ff, -L / 2, 0), FF(&ff, L / 2 - 0.3, 0), 0, cap_rf, rp, 0.4, 0.11); ST(o, s, NULL, -1, cuts, nc, RECT(cx, cy, L, 3.3 * r, a)); }
    int ab = UP(o, RECTp(FF(&ff, L / 2 + 0.15, 0), 0.9, r * 3.5, a), r * 2.3, NULL); CUT(o, BARL(FF(&ff, L / 2 + 0.15, -r * 2), FF(&ff, L / 2 + 0.15, r * 2), 0.22), ab, 0.16);
    if (rnd() < 0.55) {
        double d1 = u(1.6, 2.3), d2 = u(-1.0, 1.0); P c = FF(&ff, -L / 2 - d1, d2); double rot = u(0, 1); GG *sg = STAR(c.x, c.y, r * 1.04, r * 0.9, 10, rot); double hh = u(1.7, 2.3); int dr = UP(o, sg, hh, NULL);
        CUT(o, U2(DISCp(c, 0.28, 8), ANN(c.x, c.y, r * 0.66, r * 0.66 - 0.2, 16)), dr, 0.18);
    }
    *foot = OUT(o);
}

/* worm_maw: returns (ell, [ST(lip), ST(gum), ST(th)]) */
static void worm_maw(double cx, double cy, double a, PyRandom *rng, SoObj *o, GG **foot) {
    double R = u(2.2, 2.9); int n = (int)RI(rng, 7, 10); double t0 = u(0, 6.28), sq = u(0.85, 1.0); so_obj_init(o, "chitin");
    S *lip = SN(); { double pr[8] = { R + 1.0, 0.0, R + 0.6, 0.7, R + 0.25, 1.15, R - 0.1, 1.25 }; TURN(lip, cx, cy, pr, 4, 28, sq, a, 0.0); }
    S *gum = SN(); GG *el = ELL(cx, cy, R - 0.05, (R - 0.05) * sq, a, 20);
    for (int k = 0; k < 14; k++) { double t = t0 + 6.2832 * k / 14 + 0.2; EXT(gum, INT(RECTp(POL(cx, cy, R - 0.35, t), 0.7, 0.3, t), el), 0.75, 0); }
    S *th = SN();
    for (int k = 0; k < n; k++) {
        double t = t0 + 6.2832 * k / n + u(-0.08, 0.08), l = R * u(0.5, 0.66); P p = POL(cx, cy, (R - 0.35) * (fabs(SIN(t - a)) > 0.7 ? sq : 1.0), t);
        double FH[3][2] = { { 1.0, 0.6 }, { 0.68, 1.05 }, { 0.38, 1.45 } };
        for (int i = 0; i < 3; i++) { double f = FH[i][0]; EXT(th, TRI(p, POLp(p, l * f, t + PI), 0.85 * (0.55 + 0.45 * f)), FH[i][1], 0); }
        double t2 = t + 3.1416 / n; P p2 = POL(cx, cy, (R - 0.3) * 0.92, t2);
        EXT(th, TRI(p2, POLp(p2, l * 0.42, t2 + PI), 0.5), 0.7, 0);
    }
    GG *fo = ELL(cx, cy, R + 1.1, (R + 1.1) * sq, a, 20);
    { SoLayer *y; C cu[1] = { { ELL(cx, cy, R - 0.3, (R - 0.3) * sq, a, 20), 0.14 } }; xr_ok(sd_ST(lip, "sand", 0, 0, cu, 1, &y)); lyl_push_own(&o->L, y); }
    { SoLayer *y; xr_ok(sd_ST(gum, "flesh", 0, 0, NULL, 0, &y)); lyl_push_own(&o->L, y); }
    { SoLayer *y; xr_ok(sd_ST(th, "tooth", 0, 0, NULL, 0, &y)); lyl_push_own(&o->L, y); }
    *foot = fo;
}

static const XrDef DEFS[] = {
    { "maahl", "made", "ruin", "buried_head", 10.5, 0, 0, buried_head },
    { "maahl", "made", "obelisk", "obelisk_tip", 10.5, 0, 0, obelisk_tip },
    { "maahl", "made", "ruin", "sundial", 10.5, 0, 0, sundial },
    { "maahl", "made", "clay", "amphorae", 10.5, 0, 0, amphorae },
    { "maahl", "made", "ruin", "arch_fragment", 10.5, 0, 0, arch_fragment },
    { "maahl", "made", "pavers", "mosaic", 10.5, 0, 0, mosaic },
    { "maahl", "made", "ruin", "sphinx_paw", 10.5, 0, 0, sphinx_paw },
    { "maahl", "made", "ruin", "stele", 10.5, 0, 0, stele },
    { "maahl", "made", "column", "fallen_capital", 10.5, 0, 0, fallen_capital },
    { "maahl", "debris", "chitin", "worm_maw", 10.5, 1, 0, worm_maw },
};
void bld_sdrx_maahl(void) { xr_register(DEFS, (int)(sizeof DEFS / sizeof DEFS[0])); }
