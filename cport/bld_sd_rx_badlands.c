/* bld_sd_rx_badlands.c: sd_r_x_badlands.py in C (see bld_sd_rx.h; xon / xstuds are XON / XSTUDS there). */
#include "bld_sd_rx.h"

static double sgn(int i) { return i ? 1 : -1; }

/* xb_pad(cx, cy, t, Lp, Hp, th, z0) */
static void xb_pad(S *ps, double cx, double cy, double t, double Lp, double Hp, double th, double z0) {
    int n = 15; F f = FR(cx, cy, t);
    for (int i = 0; i < n; i++) {
        double s0 = -Lp / 2 + Lp * i / n, s1 = s0 + Lp / n, s = (s0 + s1) / 2, e = sqrt(MX(0.0, 1 - PW(2 * s / Lp, 2)));
        EXT(ps, RECTp(FF(&f, s, 0), s1 - s0 + 0.03, MX(0.32, th * (0.7 + 0.3 * e)), t), z0 + Hp * MX(0.25, PW(e, 0.85)), 0);
    }
}

/* saguaro */
typedef struct { double L, r; } Sag;
static int sag_rf(void *c, double t, double *out) {
    Sag *s = (Sag *)c; double L = s->L, r = s->r, k; int e = 0;
    if (t < L / 2 - r) k = 1.0; else { double p = PWs((t - L / 2 + r) / r, 2, &e); k = sqrt(MX(0.03, 1 - p)); }
    if (e) return SO_ERR;
    *out = r * k * (t < -L / 2 + 0.3 ? 0.9 : 1.0); return SO_OK;
}
static void saguaro(double cx, double cy, double a, PyRandom *rng, SoObj *o, GG **foot) {
    so_obj_init(o, "cactus"); F ff = FR(cx, cy, a); double L = u(4.6, 5.6), r = u(0.62, 0.76), hk = 1.1;
    Sag *sc = (Sag *)xr_mem(sizeof *sc); sc->L = L; sc->r = r;
    double lf = r * 0.7; S *ps = SN(); SPIN(ps, cx, cy, a, L, sag_rf, sc, hk, lf, "round");
    C cuts[4]; double vs[4] = { -0.62 * r, -0.21 * r, 0.21 * r, 0.62 * r };
    for (int i = 0; i < 4; i++) { double v = vs[i], w = fabs(v) + 0.09; cuts[i].q = BARL(FF(&ff, -L / 2 + 0.25, v), FF(&ff, L / 2 - r * 0.9, v), 0.17); cuts[i].fl = lf + hk * 0.93 * sqrt(MX(0.0, r * r - w * w)) - 0.12; }
    { int k = (int)RI(rng, 1, 2); int64_t ix[2]; static const double pm[2] = { -1, 1 }; xr_ok(py_sample_e(rng, 2, k, ix));
      for (int i = 0; i < k; i++) {
          double sv = pm[ix[i]], x = u(-L * 0.15, L * 0.25); P p = FF(&ff, x, sv * r * 0.5); double d = u(1.4, 2.0); double t = a + sv * u(0.7, 1.2); P e = POLp(p, d, t); double ra = r * 0.66;
          P pq[2] = { p, e }; ROD(ps, pq, 2, ra, NULL, NULL, 1.05, ra * 0.6, 0); DOME(ps, e.x, e.y, ra, ra * 1.55, 0, 1.0, 0.0, 20);
      } }
    ST(o, ps, NULL, -1, cuts, 4, RECT(cx, cy, L, 2 * r, a));
    { GL l = { 0 }; double vv[4]; LS(vv, -r * 0.7, r * 0.7, 4);
      for (int i = 0; i < 4; i++) { P p = FF(&ff, -L / 2 + 0.2, vv[i]); double d1 = u(0.6, 1.1), d2 = u(-0.15, 0.15); GA(&l, BARL(p, FF(&ff, -L / 2 - d1, vv[i] + d2), 0.3)); }
      S *s = SN(); EXT(s, UL(&l), r * 1.3, 0); ST(o, s, "wood", -1, NULL, 0, RECTp(FF(&ff, -L / 2 - 0.5, 0), 1.2, 2 * r, a)); }
    if (rnd() < 0.45) {
        double x = u(0.3, 1.0), sv = CH2(rng); double y = sv * (r + u(0.9, 1.2)); P c = FF(&ff, -L / 2 + x, y); double ry = u(0.45, 0.55), hy = u(2.4, 3.2); int n = (int)RI(rng, 9, 11);
        S *s = SN(); EXT(s, STAR(c.x, c.y, ry, ry * 0.78, n, a), hy - ry * 0.8, 0); FLUTED(s, c.x, c.y, ry, ry * 0.8, n, a, 0.78, 5, 0.0, hy - ry * 0.8); ST(o, s, NULL, -1, NULL, 0, DISCp(c, ry, 20));
        s = SN(); int m = (int)RI(rng, 2, 4); for (int k = 0; k < m; k++) { double d = u(0, 1); P q = POLp(c, ry * 0.55, k * 1.9 + d); DOME(s, q.x, q.y, 0.17, hy + 0.12, 0, 1.0, 0.0, 8); }
        ST(o, s, "flower", -1, NULL, 0, DISCp(c, ry, 20));
    }
    int n = (int)RI(rng, 1, 3);
    for (int i = 0; i < n; i++) { double x = u(0.4, 1.0), y = u(-1.0, 1.0); P c = FF(&ff, -L / 2 - x, y); double rr = u(0.3, 0.42), hh = u(0.28, 0.45); S *s = SN(); ROCK(s, c.x, c.y, rr, hh, rng, 0, 0.0, 9); ST(o, s, NULL, -1, NULL, 0, NULL); }
    *foot = OUT(o);
}

/* water_trough */
static void water_trough(double cx, double cy, double a, PyRandom *rng, SoObj *o, GG **foot) {
    F ff = FR(cx, cy, a); double L = u(4.4, 5.4), W = u(1.7, 2.0), h = u(1.4, 1.65); so_obj_init(o, "wood"); GG *g = RECT(cx, cy, L, W, a); double t = 0.34;
    int sk; { GL l = { 0 }; for (int i = 0; i < 2; i++) GA(&l, RECTp(FF(&ff, sgn(i) * L * 0.3, 0), 0.62, W + 1.0, a)); sk = UP(o, UL(&l), 0.42, NULL); }
    { GL l = { 0 }; for (int i = 0; i < 2; i++) for (int j = 0; j < 2; j++) { double sx = sgn(i), sv = sgn(j); GA(&l, BARL(FF(&ff, sx * L * 0.3 - 0.4, sv * (W / 2 + 0.3)), FF(&ff, sx * L * 0.3 + 0.4, sv * (W / 2 + 0.3)), 0.12)); } CUT(o, UL(&l), sk, 0.08); }
    GG *rim = DIF(g, INS(g, t)); int wl = UP(o, rim, h, NULL);
    { double x1 = -L * u(0.1, 0.2), x2 = L * u(0.15, 0.3); double xs[2] = { x1, x2 }; GL l = { 0 };
      for (int i = 0; i < 2; i++) GA(&l, BARL(FF(&ff, xs[i], -W), FF(&ff, xs[i] + 0.01, W), 0.13));
      for (int i = 0; i < 2; i++) GA(&l, BARL(FF(&ff, sgn(i) * (L / 2 - t), -W), FF(&ff, sgn(i) * (L / 2 - t), W), 0.13));
      CUT(o, UL(&l), wl, 0.09); }
    double hh = h - u(0.3, 0.45); int wa = UP(o, BUF(INS(g, t), 0.02, 16), hh, "water"); double x = u(-L * 0.2, L * 0.2); P cw = FF(&ff, x, 0);
    { GL l = { 0 }; GA(&l, ANN(cw.x, cw.y, 0.4, 0.4 - 0.12, 20)); GA(&l, ANN(cw.x, cw.y, 0.75, 0.75 - 0.12, 20)); CUT(o, INT(UL(&l), INS(g, t + 0.15)), wa, 0.06); }
    double xb[3] = { -L * 0.36, 0.0, L * 0.36 }; int bd;
    { GL l = { 0 }; for (int i = 0; i < 3; i++) GA(&l, RECTp(FF(&ff, xb[i], 0), 0.4, W + 0.1, a)); bd = XON(o, INT(UL(&l), DIF(g, INS(g, t))), wl, 0.09, "iron"); }
    { P sp[6]; int n = 0; for (int i = 0; i < 3; i++) for (int j = 0; j < 2; j++) sp[n++] = FF(&ff, xb[i], sgn(j) * (W / 2 - t / 2)); XSTUDS(o, sp, 6, bd, 0.13, 0.06, "iron", 6); }
    { GL l = { 0 }; for (int i = 0; i < 2; i++) for (int j = 0; j < 2; j++) { double sx = sgn(i), sv = sgn(j);
        P q[6] = { FF(&ff, sx * L / 2, sv * W / 2), FF(&ff, sx * (L / 2 - 0.6), sv * W / 2), FF(&ff, sx * (L / 2 - 0.6), sv * (W / 2 - t)), FF(&ff, sx * (L / 2 - t), sv * (W / 2 - t)), FF(&ff, sx * (L / 2 - t), sv * (W / 2 - 0.6)), FF(&ff, sx * L / 2, sv * (W / 2 - 0.6)) };
        GA(&l, POLY(q, 6)); } XON(o, UL(&l), wl, 0.08, "iron"); }
    double d1 = u(1.1, 1.5), d2 = u(-0.7, 0.7); P c = FF(&ff, L / 2 + d1, d2); double br = 0.62; double bh = u(1.0, 1.15);
    { GG *an = ANN(c.x, c.y, br, br - 0.16, 16); UP(o, an, bh, NULL); } UP(o, DISCp(c, br - 0.14, 14), 0.5, "water");
    { double ar[22]; xr_ok(sd_arc(c.x, c.y, br + 0.28, a + 0.3, a + PI - 0.3, 10, ar)); S *s = SN(); EXT(s, LINE((const P *)ar, 11, 0.3), 0.22, 0); ST(o, s, "iron", -1, NULL, 0, DISCp(c, br + 0.5, 20)); }
    if (rnd() < 0.5) {
        double y = u(-0.3, 0.3); P p = FF(&ff, -L / 2 - 0.45, y); double ph = u(2.3, 2.6); int pb = UP(o, DISCp(p, 0.42, 14), ph, "iron");
        { S *s = SN(); DOME(s, p.x, p.y, 0.36, 0.3, 0, 1.0, 0.0, 20); ST(o, s, "iron", pb, NULL, 0, NULL); }
        double p_v = (p.x - cx) * -SIN(a) + (p.y - cy) * COS(a); UP(o, RECTp(FF(&ff, -L / 2 - 0.1, p_v), 0.8, 0.32, a), h + 0.35, "iron");
        double d = u(-0.3, 0.3); P hd = FF(&ff, -L / 2 - 1.3, p_v + d);
        S *s = SN(); EXT(s, CAPL(POLp(p, 0.3, ATAN2(hd.y - p.y, hd.x - p.x)), hd, 0.3), 0.3, 0); ST(o, s, "iron", -1, NULL, 0, DISCp(hd, 0.5, 20));
    } else {
        double y = u(-0.5, 0.5); P q = FF(&ff, -L / 2 - 0.8, y); double r = u(0.8, 1.05); GCUT(o, BLOB(q.x, q.y, r, rng, 3), 0.25, "water");
    }
    GL hl = { 0 };
    for (int i = 0; i < 2; i++) { double sv = sgn(i); int k = (int)RI(rng, 1, 2); for (int j = 0; j < k; j++) { double x1 = u(-L / 2, L / 2), y1 = u(0.5, 0.9); P q = FF(&ff, x1, sv * (W / 2 + y1)); GA(&hl, DIF(ANN(q.x, q.y, 0.3, 0.14, 10), RECTp(POLp(q, 0.25, a + 1.57), 0.3, 0.3, a))); } }
    GCUT(o, UL(&hl), 0.12, "earth");
    *foot = OUT(o);
}

/* hitching_rail */
static void hitching_rail(double cx, double cy, double a, PyRandom *rng, SoObj *o, GG **foot) {
    F ff = FR(cx, cy, a); double L = u(5.0, 6.3); so_obj_init(o, "wood");
    for (int i = 0; i < 2; i++) {
        P p = FF(&ff, sgn(i) * L / 2, 0); double t = a + u(-0.15, 0.15); GG *g = RECTp(p, 0.62, 0.62, t); double hh = u(2.6, 3.0); int b = UP(o, g, hh, NULL); XON(o, RECTp(p, 0.42, 0.42, t), b, 0.12, NULL);
        CUT(o, BARL(POLp(p, 0.5, t + PI), POLp(p, 0.5, t), 0.3), b, 0.22);
        GL l = { 0 }; double ks[3] = { 0.8, 2.6, 4.4 }; for (int k = 0; k < 3; k++) GA(&l, BARL(POLp(p, 0.05, t + ks[k]), POLp(p, 0.3, t + ks[k]), 0.1)); CUT(o, UL(&l), b, 0.1);
    }
    double sv = CH2(rng); double x = u(-0.4, 0.4), y = sv * u(1.0, 1.3); P c = FF(&ff, x, y); double t = a + u(-0.2, 0.2); F f2 = FR(c.x, c.y, t); double Lr = L * u(0.82, 0.95), rr = 0.36;
    { S *s = SN(); LIE(s, c.x, c.y, t, Lr, rr, 0.0); C cu[2]; for (int i = 0; i < 2; i++) { double s2 = sgn(i); cu[i].q = BARL(FF(&f2, -Lr * 0.42, s2 * 0.12), FF(&f2, Lr * 0.42, s2 * 0.1), 0.16); cu[i].fl = rr * 0.95; } ST(o, s, NULL, -1, cu, 2, RECTp(c, Lr, 2 * rr, t)); }
    P rings[2] = { FF(&f2, -Lr * 0.25, -sv * (rr + 0.5)), FF(&f2, Lr * 0.22, -sv * (rr + 0.5)) };
    { GL l = { 0 }, fl = { 0 }; double xx[2] = { -Lr * 0.25, Lr * 0.22 }; for (int i = 0; i < 2; i++) GA(&l, ANN(rings[i].x, rings[i].y, 0.42, 0.2, 12)); for (int i = 0; i < 2; i++) GA(&l, RECTp(FF(&f2, xx[i], -sv * rr * 0.9), 0.3, 0.42, t));
      for (int i = 0; i < 2; i++) GA(&fl, DISCp(rings[i], 0.45, 20)); S *s = SN(); EXT(s, UL(&l), 0.26, 0); ST(o, s, "iron", -1, NULL, 0, UL(&fl)); }
    P q = rings[RI(rng, 0, 1)]; double d = u(1.6, 2.4); double ta = a - sv * 1.57 + u(-0.6, 0.6); P e = POLp(q, d, ta); P q0 = POLp(q, 0.4, a - sv * 1.57); P pts[8];
    int np = lf_wiggle(q0.x, q0.y, e.x, e.y, rng, 0.4, 3, &pts[0].x);
    { S *s = SN(); ROD(s, pts, np, 0.25, NULL, NULL, 1.1, 0, 0); ST(o, s, "rope", -1, NULL, 0, BUF(LSTR(pts, np), 0.3, 16)); }
    { double s1 = CH2(rng); double px = s1 * L / 2 + u(-0.3, 0.3); double py = -sv * u(1.4, 1.8); P p = FF(&ff, px, py); double ang = u(0, 6.28), sp[70]; xr_ok(sd_spiral(p.x, p.y, 0.3, 0.95, 2.4, ang, 34, sp));
      S *s = SN(); ROD(s, (const P *)sp, 35, 0.25, NULL, NULL, 1.0, 0, 0); ST(o, s, "rope", -1, NULL, 0, DISCp(p, 1.1, 20)); }
    int n = (int)RI(rng, 0, 2);
    for (int i = 0; i < n; i++) { double x1 = u(-L / 3, L / 3), y1 = -sv * u(0.6, 1.6); P qq = FF(&ff, x1, y1); S *s = SN(); for (int k = 0; k < 3; k++) { P z = POLp(qq, 0.22, k * 2.1); DOME(s, z.x, z.y, 0.22, 0.24, 0, 1.0, 0.0, 20); } ST(o, s, "earth", -1, NULL, 0, DISCp(qq, 0.4, 20)); }
    GG *ex[1] = { RECT(cx, cy, L + 1.4, 4.4, a) };
    *foot = OUTX(o, ex, 1);
}

/* prickly_pear */
typedef struct { P c; double t, Lp, Hp, z0; } Pad;
static void prickly_pear(double cx, double cy, double a, PyRandom *rng, SoObj *o, GG **foot) {
    so_obj_init(o, "cactus"); double th = u(0.4, 0.46);
    { S *s = SN(); CYL(s, cx, cy, 0.36, 0.7, 0.0, 8); ST(o, s, "wood", -1, NULL, 0, DISC(cx, cy, 0.4, 20)); }
    Pad pads[32]; int np = 0; double t0 = u(0, 6.28); int n = (int)RI(rng, 3, 4);
    for (int k = 0; k < n; k++) {
        double t = t0 + k * 6.2832 / 3.5 + u(-0.3, 0.3); double d = u(0.55, 0.85); P c = POL(cx, cy, d, t); double Lp = u(1.2, 1.5), Hp = u(1.5, 1.8);
        double tt = t + 1.5708 + u(-0.5, 0.5); pads[np].c = c; pads[np].t = tt; pads[np].Lp = Lp; pads[np].Hp = Hp; pads[np].z0 = 0.0; np++;
    }
    for (int tier = 1; tier <= 2; tier++) {
        int base[32], nb = 0; for (int i = 0; i < np; i++) if ((tier == 1 && pads[i].z0 == 0.0) || (tier == 2 && pads[i].z0 > 0.0)) base[nb++] = i;
        int k = tier == 1 ? (int)RI(rng, 1, 3) : (int)RI(rng, 0, 2); if (nb < k) k = nb;
        int64_t ix[32]; xr_ok(py_sample_e(rng, nb, k, ix));
        for (int j = 0; j < k; j++) {
            Pad p = pads[base[ix[j]]]; double s = u(-0.25, 0.25) * p.Lp, e = sqrt(MX(0.0, 1 - PW(2 * s / p.Lp, 2))), zt = p.z0 + p.Hp * MX(0.25, PW(e, 0.85));
            P q = POLp(p.c, s, p.t); double d = u(0.15, 0.35); double sv = CH2(rng); P c = POLp(q, d, p.t + sv * 1.5708);
            double tt = p.t + u(-0.7, 0.7), Lp = p.Lp * u(0.75, 0.9), Hp = p.Hp * u(0.75, 0.9), z0 = zt - u(0.35, 0.5);
            pads[np].c = c; pads[np].t = tt; pads[np].Lp = Lp; pads[np].Hp = Hp; pads[np].z0 = z0; np++;
        }
    }
    for (int i = 0; i < np; i++) { Pad p = pads[i]; S *s = SN(); xb_pad(s, p.c.x, p.c.y, p.t, p.Lp, p.Hp, th, p.z0); ST(o, s, NULL, -1, NULL, 0, RECTp(p.c, p.Lp, th, p.t)); }
    int ord[32]; for (int i = 0; i < np; i++) ord[i] = i;
    for (int i = 1; i < np; i++) for (int j = i; j > 0; j--) { double kj = -(pads[ord[j]].z0 + pads[ord[j]].Hp), kp = -(pads[ord[j - 1]].z0 + pads[ord[j - 1]].Hp); if (kj < kp) { int tmp = ord[j]; ord[j] = ord[j - 1]; ord[j - 1] = tmp; } else break; }
    for (int m = 0; m < 2 && m < np; m++) {
        Pad p = pads[ord[m]]; P fr[3]; int nf = 0; double ss[3] = { -0.22, 0.05, 0.25 };
        for (int i = 0; i < 3; i++) if (rnd() < 0.75) fr[nf++] = POLp(p.c, ss[i] * p.Lp, p.t);
        if (nf) {
            S *s = SN(); GL fl = { 0 };
            for (int i = 0; i < nf; i++) {
                double w = 2 * ((fr[i].x - p.c.x) * COS(p.t) + (fr[i].y - p.c.y) * SIN(p.t)) / p.Lp; double z = p.z0 + p.Hp * MX(0.25, PW(sqrt(MX(0.0, 1 - PW(w, 2))), 0.85)) + 0.3;
                DOME(s, fr[i].x, fr[i].y, 0.22, z, 0, 1.0, 0.0, 10);
            }
            for (int i = 0; i < nf; i++) GA(&fl, DISCp(fr[i], 0.22, 20));
            ST(o, s, "flower", -1, NULL, 0, UL(&fl));
        }
    }
    double d = u(2.0, 2.4), tt = t0 + u(2.5, 3.5); P c = POL(cx, cy, d, tt); double t = u(0, 3.14);
    S *s = SN(); DOME(s, c.x, c.y, 0.7, 0.34, 0, 0.65, t, 20); C cu[2]; for (int k = 0; k < 2; k++) { cu[k].q = DISCp(POLp(c, 0.3, t + k * 3.14), 0.1, 6); cu[k].fl = 0.24; }
    ST(o, s, NULL, -1, cu, 2, DISCp(c, 0.7, 20));
    *foot = OUT(o);
}

/* ox_yoke */
static void ox_yoke(double cx, double cy, double a, PyRandom *rng, SoObj *o, GG **foot) {
    F ff = FR(cx, cy, a); so_obj_init(o, "wood"); double X = u(1.4, 1.6);
    P pts[9] = { FF(&ff, -2 * X, 0.35), FF(&ff, -1.5 * X, 0.1), FF(&ff, -X, -0.25), FF(&ff, -0.5 * X, 0.0), FF(&ff, 0, 0.15), FF(&ff, 0.5 * X, 0.0), FF(&ff, X, -0.25), FF(&ff, 1.5 * X, 0.1), FF(&ff, 2 * X, 0.35) };
    GG *bg = LINE(pts, 9, 1.0); double hh = u(0.5, 0.6); int b = UP(o, bg, hh, NULL); int cr = XON(o, INS(bg, 0.2), b, 0.14, NULL); XON(o, INS(bg, 0.36), cr, 0.08, NULL);
    CUT(o, INT(U1(BARL(FF(&ff, -1.9 * X, 0.08), FF(&ff, 1.9 * X, 0.08 + 0.02), 0.11)), INS(bg, 0.1)), cr, 0.08);
    double gone = 0; if (rnd() < 0.3) gone = CH2(rng);
    for (int i = 0; i < 2; i++) {
        double sx = sgn(i);
        if (sx == gone) { GL l = { 0 }; for (int k = -1; k <= 1; k += 2) GA(&l, DISCp(FF(&ff, sx * X + k * 0.75, -0.2), 0.16, 8)); CUT(o, UL(&l), cr, 0.3); continue; }
        P c = FF(&ff, sx * X, -0.45); double ar[30]; xr_ok(sd_arc(c.x, c.y, 0.78, a + PI + 0.05, a + 2 * PI - 0.05, 14, ar));
        S *s = SN(); ROD(s, (const P *)ar, 15, 0.26, NULL, NULL, 1.2, 0, 0); ST(o, s, NULL, -1, NULL, 0, DISCp(c, 1.1, 20));
        GL l = { 0 }; for (int k = -1; k <= 1; k += 2) GA(&l, RECTp(FF(&ff, sx * X + k * 0.75, -0.2), 0.5, 0.2, a)); XON(o, UL(&l), cr, 0.16, "iron");
    }
    XON(o, RECTp(FF(&ff, 0, 0.3), 0.5, 0.36, a), cr, 0.1, "iron"); P c = FF(&ff, 0, 1.12);
    { S *s = SN(); EXT(s, ANN(c.x, c.y, 0.58, 0.3, 14), 0.3, 0); ST(o, s, "iron", -1, NULL, 0, DISCp(c, 0.6, 20)); }
    double d = u(1.5, 2.0); static const double c2[2] = { 0.0, PI }; double t1 = CH(rng, c2, 2), s1 = CH2(rng); double ang = a + t1 + s1 * u(0.3, 0.6); P e = POLp(c, d, ang);
    int n = 5; GL ls = { 0 };
    for (int k = 0; k < n; k++) {
        P p = { c.x + (e.x - c.x) * (k + 0.8) / n, c.y + (e.y - c.y) * (k + 0.8) / n }; double tt = ATAN2(e.y - c.y, e.x - c.x);
        if (k % 2 == 0) GA(&ls, BUF(EXTR(ELL(p.x, p.y, 0.3, 0.2, tt, 10)), 0.08, 16)); else GA(&ls, BARL(POLp(p, 0.24, tt), POLp(p, 0.24, tt + PI), 0.2));
    }
    { S *s = SN(); P ce[2] = { c, e }; EXT(s, UL(&ls), 0.22, 0); ST(o, s, "iron", -1, NULL, 0, BUF(LSTR(ce, 2), 0.4, 16)); }
    *foot = OUT(o);
}

/* bedroll_canteen */
static void bedroll_canteen(double cx, double cy, double a, PyRandom *rng, SoObj *o, GG **foot) {
    F ff = FR(cx, cy, a); double L = u(3.0, 3.6), r = u(0.75, 0.9); so_obj_init(o, "cloth");
    S *ps = SN(); LIE(ps, cx, cy, a, L, r, 0.0); { double xs[2] = { -L * 0.28, L * 0.28 }; for (int i = 0; i < 2; i++) { P q = FF(&ff, xs[i], 0); LIE(ps, q.x, q.y, a, 0.42, r + 0.1, 0.0); } }
    C cuts[5]; int nc = 0;
    { double v = r * 0.42; cuts[nc].q = BARL(FF(&ff, -L / 2 + 0.1, r * 0.42), FF(&ff, L / 2 - 0.1, r * 0.42), 0.16); cuts[nc].fl = 1.06 * sqrt(MX(0.0, r * r - v * v)) - 0.12; nc++; }
    for (int i = 0; i < 2; i++) { double xs[2] = { L / 2 - 0.25, L / 2 - 0.5 }; for (int j = 0; j < 2; j++) { cuts[nc].q = BARL(FF(&ff, sgn(i) * xs[j], -r), FF(&ff, sgn(i) * xs[j], r), 0.16); cuts[nc].fl = 1.06 * sqrt(MX(0.0, r * r - 0.0 * 0.0)) - 0.1; nc++; } }
    ST(o, ps, NULL, -1, cuts, nc, RECT(cx, cy, L, 2 * r, a));
    { GL l = { 0 }; double xs[2] = { -L * 0.28, L * 0.28 }; for (int i = 0; i < 2; i++) GA(&l, RECTp(FF(&ff, xs[i], 0), 0.5, 0.42, a)); S *s = SN(); EXT(s, UL(&l), r * 1.2 + 0.16, 0); ST(o, s, "brass", -1, NULL, 0, DISC(cx, cy, 0.5, 20)); }
    double sv = CH2(rng); double x = u(-0.6, 0.6), y = sv * (r + u(0.75, 0.9)); P c = FF(&ff, x, y); double R = u(0.6, 0.7), t = u(0, 6.28);
    int cn = UP(o, DISCp(c, R, 20), 0.42, "tin"); int fc = XON(o, DISCp(c, R - 0.12, 20), cn, 0.06, "sack");
    { GL l = { 0 }; for (int k = 0; k < 2; k++) GA(&l, BARL(POLp(c, R, t + k * 1.5708), POLp(c, R, t + k * 1.5708 + PI), 0.12)); CUT(o, UL(&l), fc, 0.05); }
    P sp = POLp(c, R + 0.2, t + 0.785); { S *s = SN(); CYL(s, sp.x, sp.y, 0.22, 0.32, 0.0, 10); ST(o, s, "tin", -1, NULL, 0, DISCp(sp, 0.25, 20)); }
    { P pts[16]; pts[0] = POLp(c, R, t + 0.785 + 0.6); double rr = R + u(0.35, 0.5); xr_ok(sd_arc(c.x, c.y, rr, t + 0.785 + 0.8, t + 0.785 + 5.4, 12, &pts[1].x)); pts[14] = POLp(c, R, t + 0.785 - 0.6);
      S *s = SN(); EXT(s, LINE(pts, 15, 0.28), 0.14, 0); ST(o, s, "leather", -1, NULL, 0, BUF(LSTR(pts, 15), 0.3, 16)); }
    x = u(-0.9, 0.9); y = u(0.5, 0.6); P q = FF(&ff, x, -sv * (r + y)); UP(o, ANN(q.x, q.y, 0.36, 0.22, 14), 0.48, "tin"); UP(o, DISCp(q, 0.24, 12), 0.08, "tin");
    { P z = POLp(q, 0.48, t); S *s = SN(); EXT(s, ANN(z.x, z.y, 0.2, 0.06, 8), 0.34, 0); ST(o, s, "tin", -1, NULL, 0, DISCp(q, 0.6, 20)); }
    *foot = OUT(o);
}

/* saddle */
static int seat_rf(void *c, double t, double *out) { double Ls = *(double *)c; int e = 0; double p = PWs(t / (Ls * 0.41), 2, &e); if (e) return SO_ERR; *out = 0.85 * sqrt(MX(0.05, 1 - p)); return SO_OK; }
static void saddle(double cx, double cy, double a, PyRandom *rng, SoObj *o, GG **foot) {
    F ff = FR(cx, cy, a); so_obj_init(o, "leather"); double Ls = u(3.2, 3.6), Ws = u(2.5, 2.8);
    double *lp = (double *)xr_mem(sizeof(double)); *lp = Ls;
    double x = u(-0.1, 0.1); P bc = FF(&ff, x, 0); double bl_ = Ls + u(0.5, 0.8); double bt = a + u(-0.08, 0.08); GG *bl = RECTp(bc, bl_, Ws + 0.5, bt); int bb = UP(o, bl, 0.16, "cloth");
    { GL l = { 0 }; double xs[2] = { -Ls / 2 - 0.1, Ls / 2 + 0.1 }; for (int i = 0; i < 2; i++) GA(&l, BARL(FF(&ff, xs[i], -Ws), FF(&ff, xs[i] + 0.01, Ws), 0.14)); CUT(o, UL(&l), bb, 0.08); }
    GG *g = RRECT(cx, cy, Ls, Ws, a, 0.9); int sk = UP(o, g, 0.4, NULL); GROOVE(o, INS(g, 0.26), sk, 0.13, 0.08);
    { P sp[4]; int n = 0; for (int i = 0; i < 2; i++) for (int j = 0; j < 2; j++) sp[n++] = FF(&ff, sgn(i) * (Ls / 2 - 0.5), sgn(j) * (Ws / 2 - 0.45)); XSTUDS(o, sp, 4, sk, 0.2, 0.08, "brass", 10); }
    S *body = SN(); SPIN(body, cx, cy, a, Ls * 0.82, seat_rf, lp, 1.0, 0, "round");
    { P q = FF(&ff, Ls * 0.2, 0); DOME(body, q.x, q.y, 0.75, 1.5, 0, 0.9, a, 20); }
    { P c = FF(&ff, -Ls * 0.12, 0); double ar[22]; xr_ok(sd_arc(c.x, c.y, 0.8, a + 2.1, a + 4.18, 10, ar)); EXT(body, LINE((const P *)ar, 11, 0.42), 1.6, 0); }
    { C cu[1] = { { ELL(PXY(FF(&ff, -Ls * 0.02, 0)), 0.55, 0.42, a, 14), 0.62 } }; ST(o, body, NULL, sk, cu, 1, NULL); }
    { P hx = FF(&ff, Ls * 0.3, 0); double pr[10] = { 0.4, 0.0, 0.3, 1.5, 0.27, 1.95, 0.36, 2.0, 0.36, 2.2 }; S *s = SN(); TURN(s, hx.x, hx.y, pr, 5, 12, 1.0, 0.0, 0.0); ST(o, s, NULL, sk, NULL, 0, NULL); }
    for (int i = 0; i < 2; i++) {
        double sv = sgn(i), x1 = u(-0.1, 0.3); P fc = FF(&ff, x1, sv * (Ws / 2 + 0.8)); double ft = a + sv * u(-0.25, 0.25); F f2 = FR(fc.x, fc.y, ft);
        GG *fg = RRECTp(FF(&f2, 0, 0), 1.3, 1.9, a, 0.3); int fd = UP(o, fg, 0.22, NULL); GROOVE(o, INS(fg, 0.2), fd, 0.12, 0.07);
        double x2 = u(-0.2, 0.2); P sc = FF(&f2, x2, sv * 1.55); GG *sg = RRECTp(sc, 1.0, 0.75, a, 0.15);
        UP(o, DIF(sg, RRECTp(sc, 0.55, 0.32, a, 0.1)), 0.42, "wood");
    }
    double y = u(-0.5, 0.5); P rc = FF(&ff, -Ls / 2 - 0.3, y); S *s = SN(); EXT(s, ANN(rc.x, rc.y, 0.4, 0.2, 12), 0.22, 0); ST(o, s, "iron", -1, NULL, 0, DISCp(rc, 0.45, 20));
    *foot = OUT(o);
}

static const XrDef DEFS[] = {
    { "badlands", "growth", "cactus", "saguaro", 10.5, 0, 0, saguaro },
    { "badlands", "made", "wood", "water_trough", 10.5, 0, 0, water_trough },
    { "badlands", "made", "wood", "hitching_rail", 10.5, 0, 0, hitching_rail },
    { "badlands", "growth", "cactus", "prickly_pear", 10.5, 0, 0, prickly_pear },
    { "badlands", "made", "wood", "ox_yoke", 10.5, 0, 0, ox_yoke },
    { "badlands", "made", "cloth", "bedroll_canteen", 10.5, 0, 0, bedroll_canteen },
    { "badlands", "made", "leather", "saddle", 10.5, 0, 0, saddle },
};
void bld_sdrx_badlands(void) { xr_register(DEFS, (int)(sizeof DEFS / sizeof DEFS[0])); }
