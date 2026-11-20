/* bld_sd_rx_mine.c: sd_r_x_mine.py in C (see bld_sd_rx.h). */
#include "bld_sd_rx.h"

static double sgn(int i) { return i ? 1 : -1; }

/* xm_grain(f2, L, W, rng, n=2, w=0.12) */
static GG *xm_grain(const F *f2, double L, double W, PyRandom *rng, int n, double w) {
    GL gs = { 0 };
    for (int k = 0; k < n; k++) {
        double v = (k + 0.5 - n / 2.0) * W / (n + 0.4) + u(-0.05, 0.05), x0 = u(-L / 2, -L * 0.1), x1 = u(L * 0.1, L / 2);
        P p = FF(f2, x0, v); double d = u(-0.06, 0.06); GA(&gs, BARL(p, FF(f2, x1, v + d), w));
    }
    return UL(&gs);
}
/* xm_ore(o, pts, rng, lift=0.0, kind="rock", r=(0.3, 0.45), avoid=()): o._ore is ORE (reset by every builder) */
static GL ORE;
static void xm_ore(SoObj *o, const P *pts, int n, PyRandom *rng, double lift, const char *kind, double r0, double r1, GG *const *avoid, int navoid) {
    for (int i = 0; i < n; i++) {
        double x = pts[i].x, y = pts[i].y, rr = u(r0, r1), h = rr * u(1.1, 1.5), t = u(0, 3.14); GG *g = DISC(x, y, rr + 0.1, 10); int hit = 0;
        for (int j = 0; j < ORE.n && !hit; j++) if (INTERSECTS(g, ORE.v[j])) hit = 1;
        for (int j = 0; j < navoid && !hit; j++) if (INTERSECTS(g, avoid[j])) hit = 1;
        if (hit) continue;
        GA(&ORE, g);
        C cu[1]; int nc = 0;
        if (rnd() < 0.35) { cu[0].q = BARL(POL(x, y, rr, t), POL(x, y, rr, t + PI), 0.15); cu[0].fl = lift + h * 0.7; nc = 1; }
        double pr[8] = { rr, 0.0, rr * 0.92, h * 0.55, rr * 0.6, h * 0.92, 0.18, h }; static const double seg[3] = { 5, 6, 7 };
        int sg = (int)CH(rng, seg, 3); double sq = u(0.7, 0.95); S *s = SN(); xr_ok(so_lathe(s, x, y, pr, 4, sg, sq, t, lift, NULL, 0));
        ST(o, s, i % 4 == 3 ? "quartz" : kind, -1, cu, nc, DISC(x, y, rr, 20));
    }
}

/* sluice_box */
static void sluice_box(double cx, double cy, double a, PyRandom *rng, SoObj *o, GG **foot) {
    memset(&ORE, 0, sizeof ORE);
    F ff = FR(cx, cy, a); double L = u(5.1, 6.0), W = u(1.7, 2.0), h = u(1.0, 1.2); so_obj_init(o, "wood"); double t = 0.3;
    int sk; { GL l = { 0 }; double xs[2] = { -L * 0.3, L * 0.32 }; for (int i = 0; i < 2; i++) GA(&l, RECTp(FF(&ff, xs[i], 0), 0.6, W + 1.0, a)); sk = UP(o, UL(&l), 0.36, NULL); }
    { P c = FF(&ff, -L * 0.3, 0); F f2 = FR(c.x, c.y, a + 1.5708); CUT(o, xm_grain(&f2, W, 0.6, rng, 1, 0.11), sk, 0.08); }
    GG *sides; { GL l = { 0 }; for (int i = 0; i < 2; i++) GA(&l, RECTp(FF(&ff, 0, sgn(i) * (W / 2 - t / 2)), L, t, a)); GA(&l, RECTp(FF(&ff, -L / 2 + t / 2, 0), t, W, a)); sides = UL(&l); }
    int wl = UP(o, sides, h, NULL); UP(o, RECTp(FF(&ff, t / 2, 0), L - t, W - 2 * t + 0.04, a), 0.3, NULL);
    { P st[64]; int n = 0; double xs[32]; int nx = AR(xs, 32, -L / 2 + 0.4, L / 2, 0.7); for (int i = 0; i < nx; i++) for (int j = 0; j < 2; j++) st[n++] = FF(&ff, xs[i], sgn(j) * (W / 2 - t / 2)); XSTUDS(o, st, n, wl, 0.12, 0.06, "iron", 6); }
    { GL l = { 0 }; double xs[2] = { -L * 0.15, L * 0.2 }; for (int i = 0; i < 2; i++) GA(&l, BARL(FF(&ff, xs[i], -W), FF(&ff, xs[i] + 0.01, W), 0.12)); CUT(o, UL(&l), wl, 0.08); }
    double xs[32]; int nx = AR(xs, 32, -L / 2 + 1.6, L / 2 - 0.4, 0.62);
    { GL l = { 0 }; for (int i = 0; i < nx; i++) GA(&l, RECTp(FF(&ff, xs[i], 0), 0.26, W - 2 * t + 0.04, a)); UP(o, UL(&l), 0.58, "iron"); }
    { int k = (int)RI(rng, 3, nx); if (k > nx) k = nx;
      for (int i = 0; i < k; i++) if (rnd() < 0.7) { double y = u(-W * 0.25, W * 0.25); P p = FF(&ff, xs[i] - 0.32, y); xm_ore(o, &p, 1, rng, 0.3, "rock", 0.17, 0.24, NULL, 0); } }
    { GL l = { 0 }; double vs[4]; LS(vs, -W / 2 + 0.45, W / 2 - 0.45, 4); for (int i = 0; i < 4; i++) GA(&l, RECTp(FF(&ff, -L / 2 + 0.75 + 0.0, vs[i]), 1.0, 0.24, a)); UP(o, UL(&l), h + 0.12, "iron"); }
    { GL l = { 0 }; double x3[3] = { -L * 0.32, 0.0, L * 0.32 }; for (int i = 0; i < 3; i++) GA(&l, RECTp(FF(&ff, x3[i], 0), 0.4, W + 0.12, a)); UP(o, DIF(UL(&l), RECTp(FF(&ff, t / 2, 0), L - t, W - 2 * t, a)), h + 0.08, "iron"); }
    { P q[4]; q[0] = FF(&ff, L / 2, -W / 2 + 0.2); double d = u(1.0, 1.4); q[1] = FF(&ff, L / 2 + d, -W * 0.7); d = u(1.2, 1.6); q[2] = FF(&ff, L / 2 + d, W * 0.6); q[3] = FF(&ff, L / 2, W / 2 - 0.2); GCUT(o, POLY(q, 4), 0.2, "water"); }
    { int n = (int)RI(rng, 2, 4); P pts[4]; for (int i = 0; i < n; i++) { double x = u(0.3, 1.2), y = u(-W * 0.4, W * 0.4); pts[i] = FF(&ff, L / 2 + x, y); } xm_ore(o, pts, n, rng, 0.0, "rock", 0.2, 0.3, NULL, 0); }
    GG *ex[1] = { RECT(cx, cy, L + 0.2, W + 1.0, a) };
    *foot = OUTX(o, ex, 1);
}

/* sorting_table */
static void sorting_table(double cx, double cy, double a, PyRandom *rng, SoObj *o, GG **foot) {
    memset(&ORE, 0, sizeof ORE);
    F ff = FR(cx, cy, a); double L = u(4.0, 4.6), W = u(2.3, 2.7), h = u(2.3, 2.6); so_obj_init(o, "wood"); GG *g = RECT(cx, cy, L, W, a);
    { GL l = { 0 }; for (int i = 0; i < 2; i++) GA(&l, RECTp(FF(&ff, sgn(i) * (L / 2 - 0.5), 0), 0.3, W + 0.2, a)); for (int i = 0; i < 2; i++) GA(&l, RECTp(FF(&ff, 0, sgn(i) * (W / 2 - 0.45)), L - 0.6, 0.3, a)); UP(o, UL(&l), 0.5, NULL); }
    { GL l = { 0 }; for (int i = 0; i < 2; i++) for (int j = 0; j < 2; j++) GA(&l, RECTp(FF(&ff, sgn(i) * (L / 2 - 0.3), sgn(j) * (W / 2 - 0.3)), 0.55, 0.55, a)); UP(o, UL(&l), h - 0.1, NULL); }
    int top = UP(o, INS(g, 0.42), h, NULL);
    { GL l = { 0 }; GA(&l, RECTp(FF(&ff, 0, W / 2 - 0.57), L - 0.84, 0.3, a)); GA(&l, RECTp(FF(&ff, -L / 2 + 0.57, 0), 0.3, W - 0.84, a)); GA(&l, RECTp(FF(&ff, L / 2 - 0.57, 0), 0.3, W - 0.84, a)); UP(o, UL(&l), h + 0.3, NULL); }
    SEAMS(o, &ff, L - 0.7, W - 0.7, top, 0.55, 0, 0.12, 0.08, NULL);
    P hp[2]; { double x = u(-L * 0.3, -L * 0.1), y = u(-0.2, 0.3); hp[0] = FF(&ff, x, y); x = u(L * 0.05, L * 0.3); y = u(-0.2, 0.3); hp[1] = FF(&ff, x, y); }
    for (int k = 0; k < 2; k++) { int n = (int)RI(rng, 3, 5); P pts[5]; for (int i = 0; i < n; i++) { double rr = u(0, 0.45), tt = u(0, 6.28); pts[i] = POLp(hp[k], rr, tt); } xm_ore(o, pts, n, rng, h, "rock", 0.2, 0.3, NULL, 0); }
    double x = u(-0.4, 0.4); P hc = FF(&ff, x, -W / 2 + 0.55); double t = a + u(-0.4, 0.4); F f2 = FR(hc.x, hc.y, t);
    { S *s = SN(); EXT(s, U2(BARL(FF(&f2, -0.9, 0), FF(&f2, 0.5, 0), 0.26), RECTp(FF(&f2, 0.6, 0), 0.3, 0.7, t)), 0.25, h); ST(o, s, "iron", -1, NULL, 0, DISCp(hc, 0.9, 20)); }
    { int n = (int)RI(rng, 3, 5); P pts[5]; for (int i = 0; i < n; i++) { double x1 = u(-L / 2, L / 2), y1 = u(0.35, 0.8); pts[i] = FF(&ff, x1, -W / 2 - y1); } xm_ore(o, pts, n, rng, 0.0, "rock", 0.25, 0.4, NULL, 0); }
    GG *ex[1] = { RECT(cx, cy, L + 0.2, W + 0.2, a) };
    *foot = OUTX(o, ex, 1);
}

/* kibble */
static void kibble(double cx, double cy, double a, PyRandom *rng, SoObj *o, GG **foot) {
    memset(&ORE, 0, sizeof ORE);
    so_obj_init(o, "iron"); double r = u(1.25, 1.45), h = u(2.9, 3.3); int full = rnd() < 0.55; double t0 = a + u(-0.3, 0.3);
    UP(o, ANN(cx, cy, r + 0.08, r - 0.3, 24), 0.32, NULL); int wl = UP(o, ANN(cx, cy, r, r - 0.32, 24), h, NULL);
    { GG *g = DISC(cx, cy, r - 0.3, 20); double hh = h - (full ? 0.55 : u(1.4, 1.8)); UP(o, g, hh, NULL); }
    { P st[16]; for (int k = 0; k < 16; k++) st[k] = POL(cx, cy, r - 0.16, k * 0.393 + t0); XSTUDS(o, st, 16, wl, 0.11, 0.07, NULL, 6); }
    { GL l = { 0 }; for (int k = 0; k < 3; k++) GA(&l, BARL(POL(cx, cy, r - 0.35, t0 + k * 2.1), POL(cx, cy, r + 0.1, t0 + k * 2.1), 0.12)); CUT(o, UL(&l), wl, 0.08); }
    if (full) { int n = (int)RI(rng, 4, 6); P pts[6]; for (int i = 0; i < n; i++) { double rr = u(0, r * 0.55), tt = u(0, 6.28); pts[i] = POL(cx, cy, rr, tt); } xm_ore(o, pts, n, rng, h - 0.6, "rock", 0.28, 0.38, NULL, 0); }
    for (int i = 0; i < 2; i++) { double sv = sgn(i); S *s = SN(); EXT(s, RECTp(POL(cx, cy, r + 0.1, t0 + sv * 1.5708), 0.4, 0.5, t0), h * 0.55, 0); ST(o, s, NULL, -1, NULL, 0, DISCp(POL(cx, cy, r + 0.12, t0 + sv * 1.5708), 0.3, 20)); }
    double side = CH2(rng); P p1 = POL(cx, cy, r + 0.35, t0 + 1.5708), p2 = POL(cx, cy, r + 0.35, t0 - 1.5708); double d = u(1.4, 1.8); P far = POL(cx, cy, r + d, t0 + side * 0.0 + (side > 0 ? 0 : PI));
    P bp[5] = { p1, POLp(far, r * 0.8, t0 + 1.5708), far, POLp(far, r * 0.8, t0 - 1.5708), p2 };
    { S *s = SN(); ROD(s, bp, 5, 0.27, NULL, NULL, 1.1, 0, 0); ST(o, s, NULL, -1, NULL, 0, BUF(LSTR(bp, 5), 0.35, 16)); }
    { P q = POLp(far, 0.45, ATAN2(far.y - cy, far.x - cx)); S *s = SN(); EXT(s, ANN(q.x, q.y, 0.42, 0.18, 12), 0.32, 0); ST(o, s, NULL, -1, NULL, 0, DISCp(far, 0.8, 20)); }
    { int n = (int)RI(rng, 2, 4); P pts[4]; for (int i = 0; i < n; i++) { double rr = r + u(0.6, 1.4); double tt = t0 + PI * (side > 0) + u(-1.2, 1.2); tt = tt + 1.5708 * CH2(rng); pts[i] = POL(cx, cy, rr, tt); }
      GG *av[3] = { DISC(cx, cy, r + 0.5, 20), BUF(LSTR(bp, 5), 0.45, 16), DISCp(far, 0.9, 20) }; xm_ore(o, pts, n, rng, 0.0, "rock", 0.3, 0.45, av, 3); }
    FPUSH(o, DISC(cx, cy, r + 1.9, 20)); *foot = OUT(o);
}

/* switch_lever */
static void switch_lever(double cx, double cy, double a, PyRandom *rng, SoObj *o, GG **foot) {
    memset(&ORE, 0, sizeof ORE);
    F ff = FR(cx, cy, a); so_obj_init(o, "iron");
    int sl; { GL l = { 0 }; double xs[2] = { -0.6, 0.6 }; for (int i = 0; i < 2; i++) GA(&l, RECTp(FF(&ff, xs[i], 0), 0.9, 3.4, a)); sl = UP(o, UL(&l), 0.5, "wood");
      GL g = { 0 }; for (int i = 0; i < 2; i++) { P c = FF(&ff, xs[i], 0); F f2 = FR(c.x, c.y, a + 1.5708); GA(&g, xm_grain(&f2, 3.2, 0.9, rng, 2, 0.11)); } CUT(o, UL(&g), sl, 0.08); }
    int bp = UP(o, RECTp(FF(&ff, 0, -0.4), 2.2, 1.5, a), 0.65, NULL);
    { P st[4]; int n = 0; for (int i = 0; i < 2; i++) for (int j = 0; j < 2; j++) st[n++] = FF(&ff, sgn(i) * 0.85, -0.4 + sgn(j) * 0.55); XSTUDS(o, st, 4, bp, 0.16, 0.12, NULL, 6); }
    int hs = XON(o, RECTp(FF(&ff, -0.2, -0.4), 1.0, 0.9, a), bp, 0.55, NULL); { P q = FF(&ff, -0.2, -0.4); S *s = SN(); DOME(s, q.x, q.y, 0.42, 0.35, 0, 1.0, 0.0, 20); ST(o, s, NULL, hs, NULL, 0, NULL); }
    double Ll = u(2.3, 2.7), d = u(-0.15, 0.15); P e = FF(&ff, 0.2 + Ll, -0.4 + d);
    { P l[2] = { FF(&ff, 0.25, -0.4), e }; S *s = SN(); ROD(s, l, 2, 0.25, NULL, NULL, 1.1, 0.15, 0); DOME(s, e.x, e.y, 0.42, 0.75, 0, 1.0, 0.0, 20); ST(o, s, NULL, -1, NULL, 0, BUF(LSTR(l, 2), 0.45, 16)); }
    { GL l = { 0 }; for (int i = 0; i < 2; i++) GA(&l, RECTp(POLp(e, 0.55, a + sgn(i) * 1.5708), 0.5, 0.25, a)); UP(o, UL(&l), 0.5, NULL); }
    double rv = 1.3; int rl = UP(o, RECTp(FF(&ff, 0, rv), 3.6, 0.55, a + 0.0), 0.32, NULL); XON(o, RECTp(FF(&ff, 0, rv), 3.6, 0.26, a), rl, 0.5, NULL);
    { P st[4]; int n = 0; double xs[2] = { -0.6, 0.6 }; for (int i = 0; i < 2; i++) for (int j = 0; j < 2; j++) st[n++] = FF(&ff, xs[i], rv + sgn(j) * 0.2); XSTUDS(o, st, 4, rl, 0.11, 0.06, NULL, 6); }
    { P l[2] = { FF(&ff, -0.2, 0.0), FF(&ff, -0.2, rv - 0.4) }; S *s = SN(); ROD(s, l, 2, 0.25, NULL, NULL, 1.0, 0.45, 0); ST(o, s, NULL, -1, NULL, 0, RECTp(FF(&ff, -0.2, 0.5), 0.5, 1.0, a)); }
    *foot = OUT(o);
}

/* spilled_ore */
static void spilled_ore(double cx, double cy, double a, PyRandom *rng, SoObj *o, GG **foot) {
    memset(&ORE, 0, sizeof ORE);
    F ff = FR(cx, cy, a); double L = u(3.0, 3.4), W = u(1.9, 2.2), h = u(1.05, 1.25); so_obj_init(o, "sack");
    S *sk = SN(); XSACK(sk, cx, cy, a, L, W, h, 0.0);
    static const double G0[4][4] = { { 0, 0, 0, 1 }, { 0, 1, 1, 1 }, { 1, 1, 1, 0 }, { 1, 0, 0, 0 } }, G1[5][4] = { { 0, 0, 0, 1 }, { 0, 1, 1, 1 }, { 1, 1, 1, .5 }, { 1, .5, 0, .5 }, { .3, .5, 1, 0 } }, G2[4][4] = { { 0, 0, 0, 1 }, { 0, 1, 1, 1 }, { 0, .5, .8, .5 }, { 0, 0, 1, 0 } };
    const double (*gl[3])[4] = { G0, G1, G2 }; int gn[3] = { 4, 5, 4 };
    P fc = FF(&ff, -L * 0.1, 0); F f2 = FR(fc.x, fc.y, a + 1.5708); GL lt = { 0 };
    for (int i = 0; i < 3; i++) for (int k = 0; k < gn[i]; k++) { double p0 = gl[i][k][0], q0 = gl[i][k][1], p1 = gl[i][k][2], q1 = gl[i][k][3];
        GA(&lt, BARL(FF(&f2, -0.75 + i * 0.55 + p0 * 0.38, -0.25 + q0 * 0.5), FF(&f2, -0.75 + i * 0.55 + p1 * 0.38 + 1e-3, -0.25 + q1 * 0.5 + 1e-3), 0.15)); }
    C cuts[64]; int nc = 0; { GL ps = { 0 }; POLYS(UL(&lt), &ps); if (ps.n > 60) xr_fail(); for (int i = 0; i < ps.n; i++) { cuts[nc].q = ps.v[i]; cuts[nc].fl = h * 0.78; nc++; } }
    cuts[nc].q = BARL(FF(&ff, -L / 2, W * 0.32), FF(&ff, L / 2, W * 0.32), 0.15); cuts[nc].fl = h * 0.55; nc++;
    { P p = FF(&ff, L / 2 - 0.1, 0); double d = u(-0.3, 0.3); cuts[nc].q = TRI(p, FF(&ff, L / 2 - 0.9, d), 0.6); cuts[nc].fl = h * 0.35; nc++; }
    ST(o, sk, NULL, -1, cuts, nc, RECT(cx, cy, L, W, a));
    P nk = FF(&ff, -L / 2 - 0.15, 0); { S *s = SN(); LIE(s, nk.x, nk.y, a, 0.5, 0.42, 0.0); ST(o, s, "rope", -1, NULL, 0, DISCp(nk, 0.45, 20)); }
    for (int i = 0; i < 2; i++) { double sv = sgn(i); P pts[2]; pts[0] = FF(&ff, -L / 2 - 0.2, sv * 0.2); double d1 = u(0.6, 1.0), d2 = u(0.4, 0.8); pts[1] = FF(&ff, -L / 2 - d1, sv * d2); S *s = SN(); ROD(s, pts, 2, 0.25, NULL, NULL, 1.0, 0, 0); ST(o, s, "rope", -1, NULL, 0, BUF(LSTR(pts, 2), 0.3, 16)); }
    { int n = (int)RI(rng, 10, 14); P pts[14]; for (int i = 0; i < n; i++) { double x = u(0.65, 2.5), y = u(-1.3, 1.3); pts[i] = FF(&ff, L / 2 + x, y); }
      GG *av[4] = { BUF(RECT(cx, cy, L, W, a), 0.1, 16), DISCp(nk, 0.6, 20), RECTp(FF(&ff, -L / 2 - 0.8, 0), 1.2, 2.0, a), RECTp(FF(&ff, 0.3, -(W / 2 + 0.6)), 6.0, 1.0, a) };
      xm_ore(o, pts, n, rng, 0.0, "rock", 0.28, 0.48, av, 4); }
    if (rnd() < 0.4) {
        double x = u(-0.5, 0.8), y = u(0.5, 0.7); P c = FF(&ff, x, -(W / 2 + y)); double t = a + u(-0.2, 0.2); F f3 = FR(c.x, c.y, t);
        { S *s = SN(); P l[2] = { FF(&f3, -2.0, 0), FF(&f3, 0.6, 0) }; ROD(s, l, 2, 0.25, NULL, NULL, 1.1, 0, 0); EXT(s, U1(RECTp(FF(&f3, -2.2, 0), 0.25, 0.7, a)), 0.3, 0); ST(o, s, "wood", -1, NULL, 0, RECTp(FF(&f3, -0.6, 0), 3.0, 0.8, a)); }
        P q[5] = { FF(&f3, 0.6, -0.5), FF(&f3, 1.7, -0.45), FF(&f3, 1.95, 0), FF(&f3, 1.7, 0.45), FF(&f3, 0.6, 0.5) }; UP(o, POLY(q, 5), 0.24, NULL);
    }
    *foot = OUT(o);
}

/* fallen_stull */
static int const_rf(void *c, double q, double *out) { (void)q; *out = *(double *)c; return SO_OK; }
static void fallen_stull(double cx, double cy, double a, PyRandom *rng, SoObj *o, GG **foot) {
    memset(&ORE, 0, sizeof ORE);
    F ff = FR(cx, cy, a); double L = u(3.0, 4.0), b = u(0.3, 0.8); b = b * CH2(rng); double r = u(0.6, 0.72); so_obj_init(o, "wood"); double lf = r * 0.8;
    double *rp = (double *)xr_mem(sizeof(double)); *rp = r;
    P c1 = FF(&ff, -L / 2, 0); P c2 = POLp(FF(&ff, 0.3, 0), L / 2, a + b);
    P cs[2] = { c1, c2 }; double ts[2] = { a, a + b }, Ls[2]; Ls[0] = L; Ls[1] = L * u(0.7, 1.0);
    for (int i = 0; i < 2; i++) {
        P c = cs[i]; double t = ts[i], Lx = Ls[i]; F f2 = FR(c.x, c.y, t); double hz = lf + r * 1.1 * 0.93; C cu[3]; double vs[2] = { -r * 0.3, r * 0.35 };
        for (int k = 0; k < 2; k++) { cu[k].q = BARL(FF(&f2, -Lx / 2 + 0.2, vs[k]), FF(&f2, Lx / 2 - 0.3, vs[k] + 0.02), 0.17); cu[k].fl = hz - 0.12 - fabs(vs[k]) * 0.3; }
        { double x = u(-Lx * 0.3, Lx * 0.3); P q = FF(&f2, x, 0); double l = u(0.8, 1.3); cu[2].q = RECTp(q, l, 2 * r, t); cu[2].fl = hz - 0.08; }
        S *s = SN(); SPIN(s, c.x, c.y, t, Lx, const_rf, rp, 1.1, lf, "round"); ST(o, s, NULL, -1, cu, 3, RECTp(c, Lx, 2 * r, t));
    }
    for (int i = 0; i < 2; i++) {
        double sv = sgn(i);
        if (rnd() < 0.75) { double x1 = u(-0.3, 0.3); P p = FF(&ff, x1, sv * 0.3); double x2 = u(-0.5, 0.9), y2 = u(1.5, 2.2); P q = FF(&ff, x2, sv * y2); S *s = SN(); EXT(s, TRI(p, q, 0.6), 0.45, 0); ST(o, s, NULL, -1, NULL, 0, DISCp(FF(&ff, 0, sv * 1.0), 1.0, 20)); }
    }
    { GL l = { 0 }; double vs[4]; LS(vs, -r * 0.6, r * 0.6, 4); for (int i = 0; i < 4; i++) { P p = FF(&ff, -0.05, vs[i]); double x = u(0.3, 0.7), y = u(-0.1, 0.1); GA(&l, TRI(p, FF(&ff, x, vs[i] + y), 0.32)); }
      S *s = SN(); EXT(s, UL(&l), lf + r * 0.8, 0); ST(o, s, NULL, -1, NULL, 0, DISCp(FF(&ff, 0.2, 0), r, 20)); }
    double x = u(0.3, 0.6); double sv = CH2(rng); double y = sv * (r + u(0.9, 1.2)); P cc = FF(&ff, -L * x, y); double t = a + u(-0.3, 0.3); F f2 = FR(cc.x, cc.y, t);
    { double l = u(2.4, 3.0); int cb = UP(o, RECTp(cc, l, 1.1, t), 0.42, NULL); CUT(o, xm_grain(&f2, 2.4, 1.1, rng, 2, 0.12), cb, 0.08); }
    { P q[4] = { FF(&f2, 1.6, -0.3), FF(&f2, 2.6, -0.3), FF(&f2, 2.6, 0.3), FF(&f2, 1.6, -0.1) }; UP(o, POLY(q, 4), 0.42, NULL); }
    { int n = (int)RI(rng, 4, 8); P pts[8]; for (int i = 0; i < n; i++) { double rr = u(1.2, 3.2), tt = u(0, 6.28); pts[i] = POL(cx, cy, rr, tt); }
      GG **av = (GG **)xr_mem(sizeof(GG *) * (size_t)(o->F.n + 1)); for (int i = 0; i < o->F.n; i++) av[i] = BUF(o->F.v[i], 0.15, 16); xm_ore(o, pts, n, rng, 0.0, "rock", 0.4, 0.7, av, o->F.n); }
    { GL h = { 0 }; GA(&h, RECTp(FF(&ff, -L / 2, 0), L, 2 * r, a)); GA(&h, RECTp(c2, L, 2 * r, a + b)); GG *ex[1] = { BUF(HULL(&h), 1.0, 16) }; *foot = OUTX(o, ex, 1); }
}

static const XrDef DEFS[] = {
    { "mine", "made", "wood", "sluice_box", 10.5, 0, 0, sluice_box },
    { "mine", "made", "wood", "sorting_table", 10.5, 0, 0, sorting_table },
    { "mine", "made", "iron", "kibble", 10.5, 0, 0, kibble },
    { "mine", "made", "iron", "switch_lever", 10.5, 0, 0, switch_lever },
    { "mine", "made", "sack", "spilled_ore", 10.5, 0, 0, spilled_ore },
    { "mine", "made", "wood", "fallen_stull", 10.5, 0, 0, fallen_stull },
};
void bld_sdrx_mine(void) { xr_register(DEFS, (int)(sizeof DEFS / sizeof DEFS[0])); }
