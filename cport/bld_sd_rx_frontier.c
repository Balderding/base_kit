/* bld_sd_rx_frontier.c: sd_r_x_frontier.py in C (see bld_sd_rx.h; x_word, x_chain, x_d live in bld_sd_rx.c). */
#include "bld_sd_rx.h"

static double sgn(int i) { return i ? 1 : -1; }

/* shop_sign */
static void shop_sign(double cx, double cy, double a, PyRandom *rng, SoObj *o, GG **foot) {
    static const char *WORDS[] = { "SALOON", "BANK", "HOTEL", "STORE", "ASSAY", "BARBER", "JAIL", "GUNS", "FEED", "SHERIFF", "LIVERY", "GOLD", "TAILOR" };
    F ff = FR(cx, cy, a); double L = u(4.9, 5.3), W = u(1.7, 2.0); so_obj_init(o, "board");
    const char *word = WORDS[CHI(rng, 13)];
    int k = (int)RI(rng, 0, 2); GG *g;
    if (k == 0) g = RRECT(cx, cy, L, W, a, 0.25);
    else if (k == 1) { GL l = { 0 }; for (int i = 0; i < 2; i++) for (int j = 0; j < 2; j++) GA(&l, DISCp(FF(&ff, sgn(i) * L / 2, sgn(j) * W / 2), 0.42, 10)); g = DIF(RECT(cx, cy, L, W, a), UL(&l)); }
    else g = U2(RECTp(FF(&ff, 0, -W * 0.08), L, W * 0.84, a), ELL(PXY(FF(&ff, 0, W * 0.2)), L * 0.34, W * 0.3, a, 24));
    if (rnd() < 0.3) {
        double sx = CH2(rng); P q[3]; q[0] = FF(&ff, sx * L / 2 + sx, -W);
        double d = u(0.6, 1.0); q[1] = FF(&ff, sx * (L / 2 - d), -W);
        d = u(-0.2, 0.3); q[2] = FF(&ff, sx * L / 2 + sx, d);
        g = DIF(g, POLY(q, 3));
    }
    double hh = u(0.32, 0.36); int b = UP(o, g, hh, NULL); GG *inner = INS(g, 0.4);
    S *s = SN(); hh = u(0.28, 0.32); EXT(s, DIF(g, inner), hh, 0); ST(o, s, NULL, b, NULL, 0, NULL);
    { double v1 = u(-0.3, -0.18); P p1 = FF(&ff, -L, W * v1); double v2 = u(-0.3, -0.18); CUT(o, INT(BARL(p1, FF(&ff, L, W * v2), 0.12), inner), b, 0.08); }
    P wc = FF(&ff, 0, W * 0.04); GG *wd = XWORD(word, wc.x, wc.y, a, W * 0.5, 0.16, 1, L - 1.1);
    if (wd) CUT(o, INT(wd, INS(inner, 0.08)), b, 0.13);
    if (rnd() < 0.5) {
        GL l = { 0 }; int n = (int)RI(rng, 2, 4);
        for (int i = 0; i < n; i++) { double x = u(-L * 0.35, L * 0.35), y = u(-W * 0.25, W * 0.25); GA(&l, DISCp(FF(&ff, x, y), 0.17, 8)); }
        CUT(o, UL(&l), b, 0.2);
    }
    { P st[4]; int n = 0; for (int i = 0; i < 2; i++) for (int j = 0; j < 2; j++) st[n++] = FF(&ff, sgn(i) * (L / 2 - 0.25), sgn(j) * (W * 0.3)); STUDS(o, st, 4, b, 0.21, 0.08, "iron", 8); }
    double top = k != 2 ? W / 2 : W * 0.48;
    for (int i = 0; i < 2; i++) {
        double sx = sgn(i);
        if (rnd() < 0.85) {
            P e = FF(&ff, sx * L * 0.3, top + 0.2); s = SN(); EXT(s, ANN(e.x, e.y, 0.3, 0.12, 10), 0.3, 0); ST(o, s, "iron", -1, NULL, 0, DISCp(e, 0.3, 20));
            if (rnd() < 0.8) { P p1 = FF(&ff, sx * L * 0.3 + sx * 0.1, top + 0.55); double d1 = u(0.22, 0.42), d2 = u(1.2, 1.7); XCHAIN(o, p1, FF(&ff, sx * L * d1, top + d2), "iron", 0.2, 0.52); }
        }
    }
    if (rnd() < 0.5) {
        double v = top + u(1.9, 2.2); P p = FF(&ff, -L * 0.42, v), q = FF(&ff, L * 0.3, v), sc = FF(&ff, L * 0.3, v - 0.35); double sp[2 * 19];
        xr_ok(sd_spiral(sc.x, sc.y, 0.35, 0.08, 1.1, a + 1.57, 18, sp));
        GL l = { 0 }; GA(&l, BARL(p, q, 0.26)); GA(&l, LINE((const P *)sp, 19, 0.2)); GA(&l, RECTp(FF(&ff, -L * 0.42, v), 0.3, 0.7, a));
        s = SN(); EXT(s, UL(&l), 0.3, 0); ST(o, s, "iron", -1, NULL, 0, RECTp(FF(&ff, 0, v), L * 0.8, 0.8, a));
    }
    *foot = OUT(o);
}

/* hay_bale */
static int hay_one(SoObj *o, PyRandom *rng, double h, P c, double b, double L, double W, int par, GG **gout) {
    F f2 = FR(c.x, c.y, b); int m = 9; P pts[20]; int np = 0;
    for (int e = 0; e < 2; e++) {
        double sx = e ? -1 : 1;
        for (int j = 0; j <= m; j++) { double d = u(-0.06, 0.06); pts[np++] = FF(&f2, sx * (L / 2 + (j % 2 ? 0.14 : -0.02) + d), sx * (-W / 2 + W * j / m)); }
    }
    GG *g = BUF(POLY(pts, np), 0.0, 16);
    int i = par < 0 ? UP(o, g, h, NULL) : ON(o, g, par, h, NULL);
    GL gs = { 0 }; double vs[64], xs[64]; int nv = AR(vs, 64, -W / 2 + 0.25, W / 2 - 0.2, 0.2);
    for (int iv = 0; iv < nv; iv++) {
        double x00 = u(0.1, 0.5); int nx = AR(xs, 64, -L / 2 + x00, L / 2 - 0.6, 1.25);
        for (int ix = 0; ix < nx; ix++) { double d1 = u(0.5, 1.3), d2 = u(-0.06, 0.06); GA(&gs, BARL(FF(&f2, xs[ix], vs[iv]), FF(&f2, xs[ix] + d1, vs[iv] + d2), 0.13)); }
    }
    double tw[2] = { -0.26 * L, 0.26 * L };
    GL r1 = { 0 }, r2 = { 0 };
    for (int k = 0; k < 2; k++) GA(&r1, RECTp(FF(&f2, tw[k], 0), 0.4, W, b));
    CUT(o, INT(DIF(UL(&gs), UL(&r1)), INS(g, 0.2)), i, 0.12);
    for (int k = 0; k < 2; k++) GA(&r2, RECTp(FF(&f2, tw[k], 0), 0.17, W + 1, b));
    CUT(o, INT(UL(&r2), g), i, 0.14);
    if (gout) *gout = g;
    return i;
}
static double fork_r(void *c, double s) { (void)c; return s < 0.97 ? 0.22 : 0.3; }
static void hay_bale(double cx, double cy, double a, PyRandom *rng, SoObj *o, GG **foot) {
    F ff = FR(cx, cy, a); double L = u(3.7, 4.1), W = u(1.8, 2.0), h = u(1.4, 1.55); so_obj_init(o, "sack");
    int b0 = hay_one(o, rng, h, PT(cx, cy), a, L, W, -1, NULL);
    if (rnd() < 0.45) {
        double x = u(-0.2, 0.2), y = u(-0.1, 0.1); P c = FF(&ff, x, y);
        static const double ch[3] = { 0.0, 0.0, 1.5708 }; double t = a + CH(rng, ch, 3); t = t + u(-0.12, 0.12);
        double L2 = rnd() < 0.5 ? L * 0.9 : W * 0.95;
        hay_one(o, rng, h, c, t, L2, W * 0.95, b0, NULL);
    } else if (rnd() < 0.5) {
        double x = u(-0.6, 0.6), y = u(0.6, 0.9); P c = FF(&ff, x, -W / 2 - y); double r = u(1.0, 1.3), hh = u(0.5, 0.7);
        S *s = SN(); DOME(s, c.x, c.y, r, hh, 0, 0.6, a, 20); ST(o, s, "sack", -1, NULL, 0, DISCp(c, 1.2, 20));
    }
    GL st = { 0 };
    for (int i = 0; i < 2; i++) for (int k = 0; k < 3; k++) {
        double sv = sgn(i), x = u(-L / 2, L / 2), y = u(W / 2 + 0.3, W / 2 + 1.0); P p = FF(&ff, x, sv * y);
        double r = u(0.5, 1.0), t = u(0, 6.28); GA(&st, BARL(p, POLp(p, r, t), 0.22));
    }
    { S *s = SN(); EXT(s, UL(&st), 0.12, 0); ST(o, s, "sack", -1, NULL, 0, UL(&st)); }
    double x = u(-0.8, 0.8), y = u(0.9, 1.2); P fc = FF(&ff, x, W / 2 + y); double ft = a + u(-0.3, 0.3); F f2 = FR(fc.x, fc.y, ft);
    double Lp = u(6.0, 6.8), x1 = Lp / 2 - 1.3;
    S *ps = SN(); TBAR(ps, FF(&f2, -Lp / 2, 0), FF(&f2, x1, 0), 0, fork_r, NULL, 0.5, 0, 0, 0);
    GL l = { 0 }; GA(&l, BARL(FF(&f2, x1, -0.62), FF(&f2, x1, 0.62), 0.3)); GA(&l, RECTp(FF(&f2, x1 - 0.2, 0), 0.5, 0.5, a));
    { static const double v3[3] = { -0.55, 0, 0.55 }, v4[4] = { -0.6, -0.2, 0.2, 0.6 }; int three = rnd() < 0.6; const double *vv = three ? v3 : v4;
      for (int i = 0; i < (three ? 3 : 4); i++) GA(&l, TRI(FF(&f2, x1, vv[i]), FF(&f2, Lp / 2, vv[i] * 1.15), 0.24)); }
    EXT(ps, UL(&l), 0.28, 0);
    ST(o, ps, "wood", -1, NULL, 0, RECTp(FF(&f2, 0, 0), Lp, 1.4, a));
    *foot = OUT(o);
}

/* busted_safe */
static void busted_safe(double cx, double cy, double a, PyRandom *rng, SoObj *o, GG **foot) {
    F ff = FR(cx, cy, a); double Hs = u(3.1, 3.4), Sz = u(2.5, 2.8), dp = u(2.5, 2.8); so_obj_init(o, "iron");
    double xb = -1.25; GG *g = RRECTp(FF(&ff, xb, 0), Hs, Sz, a, 0.15); int b = UP(o, g, dp, NULL);
    P fc = FF(&ff, xb, 0); F f2 = FR(fc.x, fc.y, a);
    { GL l = { 0 }; for (int i = 0; i < 2; i++) GA(&l, RECTp(FF(&f2, -Hs / 2 - 0.18, sgn(i) * (Sz / 2 - 0.4)), 0.4, 0.6, a));
      S *s = SN(); EXT(s, UL(&l), 0.6, 0); ST(o, s, NULL, -1, NULL, 0, RECTp(FF(&f2, -Hs / 2 - 0.2, 0), 0.45, Sz, a)); }
    double hw = Sz - 0.9, hl = Hs - 0.9, sh = u(-0.1, 0.25);
    GG *up_ = RECTp(FF(&f2, hl / 4 + sh / 2 + 0.1, 0), hl / 2 - sh - 0.2 + 0.0, hw, a), *lo = RECTp(FF(&f2, -hl / 4 + sh / 2 - 0.1, 0), hl / 2 + sh - 0.2, hw, a);
    CUT(o, up_, b, dp * 0.42); CUT(o, lo, b, dp * 0.62);
    GROOVE(o, RECTp(FF(&f2, 0, 0), hl + 0.35, hw + 0.35, a), b, 0.12, 0.1);
    { GL l = { 0 }; double xs[2] = { -hl * 0.3, hl * 0.3 }; for (int i = 0; i < 2; i++) GA(&l, RECTp(FF(&f2, xs[i], -Sz / 2 + 0.22), 0.36, 0.3, a)); ON(o, UL(&l), b, 0.22, NULL); }
    { GL l = { 0 }; double xs[4]; LS(xs, -hl * 0.38, hl * 0.38, 4); for (int i = 0; i < 4; i++) GA(&l, DISCp(FF(&f2, xs[i], Sz / 2 - 0.28), 0.1, 6)); CUT(o, UL(&l), b, 0.25); }
    CUT(o, RECTp(FF(&f2, -Hs / 2 + 0.22, 0), 0.14, Sz - 0.3, a), b, 0.12);
    double y = u(-0.2, 0.2); P dcp = FF(&ff, 1.7, y); double dl_ = u(2.1, 2.3), dt = a + u(-0.25, 0.25);
    GG *dg = RECTp(dcp, dl_, Sz - 0.15, dt); P dc = CENT(dg); double t = a + u(-0.25, 0.25); F f3 = FR(dc.x, dc.y, t);
    if (rnd() < 0.6) { P q[5] = { FF(&f3, 1.3, -1.6), FF(&f3, 0.4, -1.6), FF(&f3, 0.7, -1.15), FF(&f3, 0.55, -0.9), FF(&f3, 1.3, -0.6) }; dg = DIF(dg, POLY(q, 5)); }
    int dd = UP(o, dg, 0.5, NULL); int pn = ON(o, INS(dg, 0.3), dd, 0.12, NULL); GROOVE(o, INS(dg, 0.55), pn, 0.1, 0.08);
    { GL l = { 0 }; double vs[3] = { -0.7, 0.0, 0.7 }; for (int i = 0; i < 3; i++) GA(&l, RECTp(FF(&f3, -1.15 - 0.05, vs[i]), 0.3, 0.26, t));
      S *s = SN(); EXT(s, UL(&l), 0.3, 0); ST(o, s, NULL, -1, NULL, 0, RECTp(FF(&f3, -1.2, 0), 0.35, 1.8, t)); }
    P dl = FF(&f3, -0.25, 0.35); int di = ON(o, DISCp(dl, 0.42, 16), pn, 0.2, NULL); CUT(o, ANN(dl.x, dl.y, 0.36, 0.25, 16), di, 0.06);
    ON(o, DISCp(dl, 0.18, 8), di, 0.14, NULL);
    { P hc = FF(&f3, 0.35, -0.4); GL l = { 0 }; GA(&l, ANN(hc.x, hc.y, 0.42, 0.26, 12)); for (int k = 0; k < 3; k++) GA(&l, BARL(hc, POLp(hc, 0.38, k * 2.094 + t), 0.12)); ON(o, UL(&l), pn, 0.16, NULL); }
    ON(o, ELL(PXY(FF(&f3, 0.45, 0.45)), 0.14, 0.2, t, 8), pn, 0.08, NULL);
    GL sp = { 0 }; int n = (int)RI(rng, 4, 8);
    for (int i = 0; i < n; i++) {
        double x = u(-0.2, 2.8), sv = CH2(rng); sv = sv * u(1.5, 2.2); P c = FF(&ff, x, sv);
        if (rnd() < 0.7) GA(&sp, DISCp(c, 0.24, 10)); else { double r = u(0, 3); GA(&sp, RECTp(c, 0.7, 0.36, r)); }
    }
    { S *s = SN(); EXT(s, UL(&sp), 0.1, 0); ST(o, s, "gold", -1, NULL, 0, UL(&sp)); }
    *foot = OUT(o);
}

/* potbelly_stove */
static double stove_rf(void *c, double s) {
    double Rb = *(double *)c;
    if (s < 0.07) return Rb * 0.86;
    if (s < 0.12) return Rb * 0.76;
    if (s < 0.56) return Rb * (0.78 + 0.22 * SIN(PI * (s - 0.12) / 0.44));
    if (s < 0.6) return Rb * 0.86;
    if (s < 0.74) return Rb * 0.72;
    if (s < 0.8) return Rb * 0.8;
    if (s < 0.9) return Rb * (0.7 - 2.6 * (s - 0.8));
    return Rb * 0.26;
}
static double pipe_r(void *c, double s) { double rp = *(double *)c; return rp + (s < 0.08 ? 0.06 : 0.0); }
static void potbelly_stove(double cx, double cy, double a, PyRandom *rng, SoObj *o, GG **foot) {
    F ff = FR(cx, cy, a); so_obj_init(o, "iron"); double Rb = u(1.15, 1.3); double *rbp = (double *)xr_mem(sizeof(double)); *rbp = Rb;
    P e0; double t, bud;
    if (rnd() < 0.5) {
        double L = u(4.2, 4.6), zc = Rb * 0.98; S *ps = SN();
        TBAR(ps, FF(&ff, -L / 2, 0), FF(&ff, L / 2, 0), 0, stove_rf, rbp, 0.4, 1, zc, zc);
        double vz[3][2] = { { -Rb * 0.55, 0.35 }, { Rb * 0.55, 0.35 }, { 0.0, Rb * 1.4 } };
        for (int i = 0; i < 3; i++) { double v = vz[i][0], z = vz[i][1]; TBARz(ps, FF(&ff, -L / 2 + 0.05, v), FF(&ff, -L / 2 - 0.55, v * 1.2), 0.22, z, MX(0.25, z - 0.2)); }
        double dx = -L * 0.16; GG *dg = RECTp(FF(&ff, dx, 0), 1.05, 1.1, a);
        EXT(ps, dg, zc + Rb + 0.12, 0);
        GL l = { 0 }; double vs[3] = { -0.25, 0.0, 0.25 }; for (int i = 0; i < 3; i++) GA(&l, BARL(FF(&ff, dx - 0.25, vs[i]), FF(&ff, dx + 0.3, vs[i]), 0.12));
        C cu[1] = { { UL(&l), zc + Rb + 0.03 } }; ST(o, ps, NULL, -1, cu, 1, RECT(cx, cy, L + 1.0, 2 * Rb, a));
        double sv = CH2(rng); e0 = FF(&ff, -L / 2 + 0.3, sv * (Rb + 0.6)); t = a + u(-0.12, 0.12); bud = L - 0.6;
    } else {
        double hz = u(4.1, 4.5); P c = FF(&ff, -2.1, 0);
        double prof[64] = { Rb * 0.88, 0.45, Rb * 0.88, 0.7, Rb * 0.8, 0.78, Rb, 1.6, Rb, 2.1, Rb * 0.82, 2.6, Rb * 0.86, 2.66, Rb * 0.86, 2.8, Rb * 0.7, 2.86, Rb * 0.7, 3.3, Rb * 0.78, 3.36, Rb * 0.78, 3.5 };
        int np = 24; np += CRV(prof + np, Rb * 0.7, 0.32, 3.5, hz - 0.3, 1, 5);
        prof[np++] = 0.22; prof[np++] = hz - 0.3; prof[np++] = 0.22; prof[np++] = hz;
        S *ps = SN(); TURN(ps, c.x, c.y, prof, np / 2, 28, 1.0, 0.0, 0.0);
        for (int k = 0; k < 3; k++) { P q = POLp(c, Rb * 0.98, a + 0.5 + k * 2.094); CYL(ps, q.x, q.y, 0.3, 0.55, 0.0, 8); }
        P dp_ = POLp(c, Rb - 0.1, a + PI); EXT(ps, RECT(dp_.x, dp_.y, 0.7, 1.1, a), 2.0, 0);
        C cu[1] = { { DISCp(c, 0.38, 10), hz - 0.85 } }; ST(o, ps, NULL, -1, cu, 1, DISCp(c, Rb + 0.25, 20));
        double y = u(-0.5, 0.5); e0 = FF(&ff, -0.55, y); t = a + u(-0.2, 0.2); bud = u(3.4, 3.8);
    }
    static double rp = 0.36; int n = (int)RI(rng, 1, 2); P p = e0; int el = rnd() < 0.6; bud -= el ? 0.9 : 0.0;
    for (int i = 0; i < n; i++) {
        double Lp = bud / n - 0.25; P q = POLp(p, Lp, t); P pq[2] = { p, q };
        S *s = SN(); TBAR(s, p, q, 0, pipe_r, &rp, 1.0, 0, 0, 0); ST(o, s, NULL, -1, NULL, 0, BUF(LSTR(pq, 2), rp, 16));
        double d = u(-0.3, 0.3); p = POLp(q, 0.25, t + d); t += u(-0.5, 0.5);
    }
    if (el) {
        double tt = CH2(rng); tt = t + tt * 1.5708; P q = POLp(p, 0.8, t); P pq[2] = { p, q };
        S *s = SN(); TBARc(s, p, q, rp, 1.0); TBARc(s, q, POLp(q, 0.9, tt), rp, 1.0); ST(o, s, NULL, -1, NULL, 0, BUF(LSTR(pq, 2), 0.6, 16));
    }
    *foot = OUT(o);
}

/* batwing_door */
typedef struct { double hs, W, L, drop; } Utop;
static double utop(const Utop *t, double v) { double q = (t->hs * v + t->W / 2) / t->W; return t->L / 2 - t->drop * (SIN(q * PI) * 0.6 + q * 0.5); }
static void batwing_door(double cx, double cy, double a, PyRandom *rng, SoObj *o, GG **foot) {
    F ff = FR(cx, cy, a); double L = u(4.5, 4.8), W = u(2.4, 2.7); so_obj_init(o, "board"); double hs = CH2(rng);
    int m = 12; double drop = u(0.6, 0.9); Utop ut = { hs, W, L, drop };
    P pts[16]; int np = 0; double vs[16];
    pts[np++] = FF(&ff, -L / 2, -W / 2); pts[np++] = FF(&ff, -L / 2, W / 2);
    LS(vs, W / 2, -W / 2, m); for (int i = 0; i < m; i++) pts[np++] = FF(&ff, utop(&ut, vs[i]), vs[i]);
    GG *g = BUF(POLY(pts, np), 0.0, 16); double hh = u(0.3, 0.34); int b = UP(o, g, hh, NULL); double st = 0.42;
    double xm = -L / 2 + L * u(0.52, 0.58);
    GG *lower = RECTp(FF(&ff, (-L / 2 + 0.55 + xm - 0.2) / 2, 0), xm - 0.2 - (-L / 2 + 0.55), W - 2 * st, a);
    np = 0; pts[np++] = FF(&ff, xm + 0.2, -W / 2 + st); pts[np++] = FF(&ff, xm + 0.2, W / 2 - st);
    LS(vs, W / 2 - st, -W / 2 + st, m); for (int i = 0; i < m; i++) pts[np++] = FF(&ff, utop(&ut, vs[i]) - st, vs[i]);
    GG *upper = BUF(POLY(pts, np), 0.0, 16);
    GG *fr = DIF(g, U2(lower, upper));
    { S *s = SN(); EXT(s, fr, 0.15, 0); ST(o, s, NULL, b, NULL, 0, NULL); }
    { GL l = { 0 }; double xs[64]; int nx = AR(xs, 64, -L / 2 + 0.75, xm - 0.25, 0.3); for (int i = 0; i < nx; i++) GA(&l, BARL(FF(&ff, xs[i], -W), FF(&ff, xs[i], W), 0.14));
      CUT(o, INT(UL(&l), INS(lower, 0.05)), b, 0.12); }
    int ns = (int)RI(rng, 4, 6); double v2[8]; LS(v2, -W / 2 + st, W / 2 - st, ns + 1);
    { GL l = { 0 }; for (int i = 0; i < ns; i++) GA(&l, RECTp(FF(&ff, xm + 0.2 + L, (v2[i] + v2[i + 1]) / 2), 2 * L, (v2[i + 1] - v2[i]) - 0.3, a)); CUT(o, INT(UL(&l), INS(upper, 0.12)), b, 0.2); }
    { GL l = { 0 }; for (int i = 1; i < ns; i++) for (int j = 0; j < 2; j++) GA(&l, ELL(PXY(FF(&ff, xm + 0.2 + 0.25 + (j * 0.55), v2[i])), 0.08, 0.16, a, 6)); CUT(o, UL(&l), b, 0.06); }
    double hv = hs * (W / 2 - 0.05); double xx[2] = { -L * 0.32, L * 0.22 };
    { GL l = { 0 }; for (int i = 0; i < 2; i++) GA(&l, RECTp(FF(&ff, xx[i], hv - hs * 0.4), 0.42, 0.85, a)); S *s = SN(); EXT(s, UL(&l), 0.12, 0); ST(o, s, "iron", b, NULL, 0, NULL); }
    { P sp[4]; int n = 0; for (int i = 0; i < 2; i++) for (int j = 0; j < 2; j++) sp[n++] = FF(&ff, xx[i] + (j ? 0.1 : -0.1), hv - hs * 0.55); STUDS(o, sp, 4, b, 0.2, 0.08, "iron", 8); }
    if (rnd() < 0.5) {
        GL l = { 0 }; int n = (int)RI(rng, 2, 4);
        for (int i = 0; i < n; i++) { double x = u(-L * 0.3, L * 0.25), y = u(-W * 0.3, W * 0.3); GA(&l, DISCp(FF(&ff, x, y), 0.17, 8)); }
        CUT(o, UL(&l), b, 0.3);
    }
    *foot = OUT(o);
}

/* church_bell */
static double bell_rf(void *c, double s) {
    double Rm = *(double *)c, r = Rm * (0.5 + 0.08 * s + 0.42 * PW(s, 3.2));
    if (s > 0.86) r = s < 0.97 ? Rm * (1.0 + 0.06 * SIN((s - 0.86) / 0.14 * PI)) : Rm * 0.97;
    if (fabs(s - 0.2) < 0.02 || fabs(s - 0.3) < 0.02 || fabs(s - 0.72) < 0.02) r += 0.09;
    return r;
}
static double clap_r(void *c, double s) { (void)c; return s < 0.85 ? 0.24 : 0.32; }
static void church_bell(double cx, double cy, double a, PyRandom *rng, SoObj *o, GG **foot) {
    F ff = FR(cx, cy, a); double Lb = u(3.7, 4.0), Rm = u(1.75, 1.95); so_obj_init(o, "brass"); double x0 = -2.0;
    double *rmp = (double *)xr_mem(sizeof(double)); *rmp = Rm;
    P p = FF(&ff, x0, 0), q = FF(&ff, x0 + Lb, 0);
    C cuts[1]; int nc = 0;
    if (rnd() < 0.45) {
        double y1 = u(-0.3, 0.3); P a1 = FF(&ff, x0 + Lb, y1); double xk = u(0.45, 0.6), y2 = u(-0.6, 0.6); P a2 = FF(&ff, x0 + Lb * xk, y2);
        cuts[0].q = WAVY(a1, a2, rng, 0.25, 4, 0.16); cuts[0].fl = Rm * 1.5; nc = 1;
    }
    S *s = SN(); TBAR(s, p, q, 0, bell_rf, rmp, 0.4, 1, Rm * 0.52, Rm * 0.98); ST(o, s, NULL, -1, cuts, nc, RECTp(FF(&ff, x0 + Lb / 2, 0), Lb, 2 * Rm, a));
    s = SN(); EXT(s, BUF(EXTR(ELL(PXY(FF(&ff, x0 - 0.35, 0)), 0.4, 0.55, a, 12)), 0.14, 16), Rm * 1.05, 0); ST(o, s, "brass", -1, NULL, 0, DISCp(FF(&ff, x0 - 0.35, 0), 0.6, 20));
    if (rnd() < 0.8) {
        double hx = x0 - 0.75, t = a + 1.5708 + u(-0.15, 0.15), hl = u(3.4, 3.9); P hc = FF(&ff, hx, 0); F f2 = FR(hc.x, hc.y, t);
        GG *hg = RECTp(FF(&ff, hx, 0), hl, 0.95, t); double hh = u(1.1, 1.25); int hb = UP(o, hg, hh, "wood");
        { GL l = { 0 }; double vs[2] = { -0.22, 0.2 }; for (int i = 0; i < 2; i++) GA(&l, BARL(FF(&f2, -hl, vs[i]), FF(&f2, hl, vs[i]), 0.1)); CUT(o, UL(&l), hb, 0.08); }
        { GL l = { 0 }; double xs[4] = { -0.6, 0.6, -hl * 0.38, hl * 0.38 }; for (int i = 0; i < 4; i++) GA(&l, RECTp(FF(&f2, xs[i], 0), 0.3, 0.95, t)); ON(o, UL(&l), hb, 0.1, "iron"); }
        { P sp[4]; int n = 0; double xs[2] = { -0.6, 0.6 }, vs[2] = { -0.25, 0.25 }; for (int i = 0; i < 2; i++) for (int j = 0; j < 2; j++) sp[n++] = FF(&f2, xs[i], vs[j]); STUDS(o, sp, 4, hb, 0.2, 0.1, "iron", 8); }
        s = SN(); for (int i = 0; i < 2; i++) { double sv = sgn(i); TBARz(s, FF(&f2, sv * (hl / 2 - 0.25), 0), FF(&f2, sv * (hl / 2 + 0.4), 0), 0.24, 0.6, 0.6); }
        ST(o, s, "iron", -1, NULL, 0, RECTp(FF(&ff, hx, 0), hl + 0.8, 0.5, t));
    }
    double d1 = u(0.85, 1.0), d2 = u(-0.9, 0.9); P c = FF(&ff, x0 + Lb + d1, d2);
    double t = a + u(-0.6, 0.6); { static const double ch[2] = { 1.5708, -1.5708 }; t = t + CH(rng, ch, 2); } F f3 = FR(c.x, c.y, t);
    s = SN(); { P dc = FF(&f3, 0.0, 0); DOME(s, dc.x, dc.y, 0.5, 0.75, 0, 1.0, 0.0, 20); } TBAR(s, FF(&f3, 0.2, 0), FF(&f3, 1.5, 0), 0, clap_r, NULL, 0.6, 0, 0, 0);
    ST(o, s, "iron", -1, NULL, 0, DISCp(c, 0.6, 20));
    *foot = OUT(o);
}

static const XrDef DEFS[] = {
    { "frontier", "made", "board", "shop_sign", 10.5, 0, 1, shop_sign },
    { "frontier", "made", "sack", "hay_bale", 10.5, 0, 1, hay_bale },
    { "frontier", "made", "iron", "busted_safe", 10.5, 0, 1, busted_safe },
    { "frontier", "made", "iron", "potbelly_stove", 10.5, 0, 1, potbelly_stove },
    { "frontier", "made", "board", "batwing_door", 10.5, 0, 1, batwing_door },
    { "frontier", "made", "brass", "church_bell", 10.5, 0, 1, church_bell },
};
void bld_sdrx_frontier(void) { xr_register(DEFS, (int)(sizeof DEFS / sizeof DEFS[0])); }
