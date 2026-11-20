/* bld_sd_rx_corridor.c: sd_r_x_corridor.py in C (see bld_sd_rx.h; xc_bar is XBAR with step 0.1). */
#include "bld_sd_rx.h"

static double sgn(int i) { return i ? 1 : -1; }
/* xc_hose(o, pts, r=0.26, kind="cable") */
static void xc_hose(SoObj *o, const P *pts, int n, double r, const char *kind) { S *s = SN(); ROD(s, pts, n, r, NULL, NULL, 1.1, 0, 0); ST(o, s, kind, -1, NULL, 0, BUF(LSTR(pts, n), r + 0.15, 16)); }

/* terminal */
static void terminal(double cx, double cy, double a, PyRandom *rng, SoObj *o, GG **foot) {
    F ff = FR(cx, cy, a); double L = u(3.3, 3.9), W = u(2.5, 3.0); so_obj_init(o, "metal");
    double Hd = u(3.6, 4.1); int b0 = UP(o, RRECT(cx, cy, L, W, a, 0.2), 0.45, NULL);
    int body = ON(o, RRECT(cx, cy, L - 0.3, W - 0.3, a, 0.15), b0, Hd - 0.45, NULL);
    double xs = -L * 0.08;
    GROOVE(o, RECTp(FF(&ff, L * 0.22, 0), L * 0.4, W - 1.0, a), body, 0.16, 0.12);
    { GL l = { 0 }; double vs[64]; int nv = AR(vs, 64, -W / 2 + 0.6, W * 0.12, 0.42); for (int i = 0; i < 3; i++) for (int j = 0; j < nv; j++) GA(&l, RECTp(FF(&ff, L * 0.08 + i * 0.44, vs[j]), 0.32, 0.32, a)); ON(o, UL(&l), body, 0.12, NULL); }
    ON(o, DISCp(FF(&ff, L * 0.3, W * 0.3), 0.36, 12), body, 0.22, NULL); { P sp[1] = { FF(&ff, L * 0.1, W * 0.3) }; STUDS(o, sp, 1, body, 0.24, 0.16, NULL, 8); }
    GG *g = RECTp(FF(&ff, xs - L * 0.12, 0), L * 0.26, W - 0.6, a); double hh = u(0.8, 1.0); int sc = ON(o, g, body, hh, NULL);
    GG *scr = RECTp(FF(&ff, xs - L * 0.12, -W * 0.04), L * 0.26 - 0.45, W - 1.25, a); CUT(o, scr, sc, 0.16);
    if (rnd() < 0.4) {
        double d1 = u(-0.2, 0.2), d2 = u(-0.5, 0.4); P c = FF(&ff, xs - L * 0.12 + d1, d2); GL l = { 0 }; int n = (int)RI(rng, 3, 5);
        for (int i = 0; i < n; i++) { double r = u(0.5, 1.2), t = u(0, 6.28); GA(&l, BARL(c, POLp(c, r, t), 0.12)); }
        CUT(o, INT(UL(&l), scr), sc, 0.3);
    }
    g = RECTp(FF(&ff, -L / 2 + 0.55, 0), 0.8, W - 0.6, a); hh = u(1.6, 1.9); int bk = ON(o, g, body, hh, NULL);
    { GL l = { 0 }; double vs[64]; int nv = AR(vs, 64, -W / 2 + 0.6, W * 0.1, 0.3); for (int j = 0; j < nv; j++) GA(&l, BARL(FF(&ff, -L / 2 + 0.33, vs[j]), FF(&ff, -L / 2 + 0.77, vs[j]), 0.13)); CUT(o, UL(&l), bk, 0.14); }
    { P sp[3]; for (int i = 0; i < 3; i++) sp[i] = FF(&ff, -L / 2 + 0.55, W / 2 - 0.65 - 0.36 * i); STUDS(o, sp, 3, bk, 0.14, 0.14, NULL, 8); }
    GROOVE(o, INT(RECTp(FF(&ff, L * 0.2, 0), L * 0.3, W - 0.3, a), INS(RRECT(cx, cy, L - 0.3, W - 0.3, a, 0.15), 0.05)), b0, 0.12, 0.1);
    *foot = OUT(o);
}

/* deck_hatch */
static void deck_hatch(double cx, double cy, double a, PyRandom *rng, SoObj *o, GG **foot) {
    F ff = FR(cx, cy, a); double Sz = u(3.8, 4.5), R = Sz * 0.35; so_obj_init(o, "plate"); GG *g = RRECT(cx, cy, Sz, Sz, a, 0.35);
    int b = UP(o, g, 0.4, NULL); int rd = rnd() < 0.7;
    GG *hole = rd ? DISC(cx, cy, R + 0.1, 28) : RRECT(cx, cy, Sz * 0.68, Sz * 0.68, a, 0.3);
    DIF(g, BUF(hole, 0.25, 16));
    { GL l = { 0 }; double ts[64]; int nt = AR(ts, 64, -Sz, Sz, 0.55); for (int i = 0; i < nt; i++) GA(&l, BARL(FF(&ff, ts[i] - Sz, -Sz), FF(&ff, ts[i] + Sz, Sz), 0.22));
      CUT(o, DIF(INT(UL(&l), INS(g, 0.25)), BUF(hole, 0.45, 16)), b, 0.1); }
    { P sp[7]; int n = 0; for (int i = 0; i < 2; i++) for (int j = 0; j < 2; j++) sp[n++] = FF(&ff, sgn(i) * (Sz / 2 - 0.32), sgn(j) * (Sz / 2 - 0.32));
      for (int i = 0; i < 2; i++) sp[n++] = FF(&ff, sgn(i) * (Sz / 2 - 0.3), 0); sp[n++] = FF(&ff, 0, Sz / 2 - 0.3); STUDS(o, sp, n, b, 0.22, 0.13, NULL, 8); }
    double hy = -Sz / 2 + 0.3; { GL l = { 0 }; double xs[2] = { -Sz * 0.2, Sz * 0.2 }; for (int i = 0; i < 2; i++) GA(&l, RECTp(FF(&ff, xs[i], hy), 0.7, 0.5, a)); ON(o, UL(&l), b, 0.32, NULL); }
    ON(o, RECTp(FF(&ff, 0, hy), Sz * 0.62, 0.3, a), b, 0.2, NULL);
    int cov = ON(o, BUF(hole, -0.12, 16), b, 0.28, NULL);
    if (rd) {
        GROOVE(o, DISC(cx, cy, R - 0.32, 28), cov, 0.14, 0.1);
        int nd = (int)RI(rng, 6, 8); double t0 = u(0, 6.28);
        { GL l = { 0 }; for (int k = 0; k < nd; k++) { double t = t0 + 6.2832 * k / nd; GA(&l, RECTp(POL(cx, cy, R - 0.05, t), 0.55, 0.36, t)); } ON(o, UL(&l), b, 0.38, NULL); }
        static const double n34[2] = { 3, 4 }; int n = (int)CH(rng, n34, 2); double rw = R * 0.6, t1 = u(0, 6.28);
        { GL l = { 0 }; GA(&l, ANN(cx, cy, rw, rw - 0.36, 24)); for (int k = 0; k < n; k++) GA(&l, CAPL(PT(cx, cy), POL(cx, cy, rw - 0.18, t1 + 6.2832 * k / n), 0.36)); ON(o, UL(&l), cov, 0.3, NULL); }
        ON(o, DISC(cx, cy, 0.42, 10), cov, 0.5, NULL);
    } else {
        static const double ch[2] = { 0, 1.5708 }; double t0 = a + CH(rng, ch, 2);
        ON(o, CAPL(POL(cx, cy, 0.0, t0), POL(cx, cy, Sz * 0.26, t0), 0.42), cov, 0.28, NULL); ON(o, DISC(cx, cy, 0.45, 10), cov, 0.42, NULL);
        GROOVE(o, RRECT(cx, cy, Sz * 0.68 - 0.75, Sz * 0.68 - 0.75, a, 0.2), cov, 0.14, 0.1);
        HANDLE(o, FF(&ff, -Sz * 0.14, Sz * 0.2), FF(&ff, Sz * 0.14, Sz * 0.2), cov, 0.36, 0.3, NULL);
    }
    P c = FF(&ff, Sz / 2 - 0.55, Sz / 2 - 0.55); int k = ON(o, DISCp(c, 0.3, 10), b, 0.2, NULL); CUT(o, DISCp(c, 0.13, 8), k, 0.1);
    *foot = OUT(o);
}

/* hover_pallet */
static void hover_pallet(double cx, double cy, double a, PyRandom *rng, SoObj *o, GG **foot) {
    F ff = FR(cx, cy, a); double L = u(4.8, 5.6), W = u(3.2, 3.7); so_obj_init(o, "metal");
    P pods[4]; { int n = 0; for (int i = 0; i < 2; i++) for (int j = 0; j < 2; j++) pods[n++] = FF(&ff, sgn(i) * (L / 2 - 0.75), sgn(j) * (W / 2 - 0.55)); }
    int pd; { GL l = { 0 }; for (int i = 0; i < 4; i++) GA(&l, DISCp(pods[i], 0.72, 14)); pd = UP(o, UL(&l), 0.75, NULL); }
    { GL l = { 0 }; for (int i = 0; i < 4; i++) GA(&l, ANN(pods[i].x, pods[i].y, 0.5, 0.32, 10)); CUT(o, UL(&l), pd, 0.14); }
    GG *dk = RRECTp(FF(&ff, 0.1, 0), L - 0.4, W - 0.2, a, 0.4); double hh = u(1.05, 1.2); int b = UP(o, dk, hh, NULL);
    GROOVE(o, INS(dk, 0.35), b, 0.16, 0.12);
    { GL l = { 0 }; double xs[3] = { -L * 0.3, 0.0, L * 0.3 }; for (int i = 0; i < 3; i++) for (int j = 0; j < 2; j++) { P q = FF(&ff, xs[i], sgn(j) * (W / 2 - 0.42)); GA(&l, ANN(q.x, q.y, 0.24, 0.1, 8)); } CUT(o, UL(&l), b, 0.12); }
    GG *tg = RECTp(FF(&ff, -L / 2 + 0.05, 0), 0.6, 1.0, a); hh = u(1.9, 2.2); int tb = UP(o, tg, hh, NULL); ON(o, RECTp(FF(&ff, -L / 2 + 0.05, 0), 0.4, 0.75, a), tb, 0.2, NULL);
    double c0 = rnd();
    if (c0 < 0.6) {
        int n = (int)RI(rng, 1, 2);
        for (int i = 0; i < n; i++) {
            double cl = n == 2 ? u(1.7, 2.1) : u(2.6, 3.2); double cw = u(2.0, 2.4); double d1 = u(-0.1, 0.1); double x = 0.25 + (i - (n - 1) / 2.0) * 2.25 + d1; double y = u(-0.2, 0.2); P c = FF(&ff, x, y);
            double t = a + u(-0.12, 0.12); F f2 = FR(c.x, c.y, t);
            GG *cg = RECTp(c, cl, cw, t); double ch = u(2.0, 2.4); int cs = ON(o, cg, b, ch, NULL); int ld = LID(o, cg, cs, 0.14, 0.16, NULL);
            CORNERS(o, &f2, cl - 0.32, cw - 0.32, ld, t, 0.45, 0.1, NULL);
            { GL l = { 0 }; double xs[64]; int nx = AR(xs, 64, -cl * 0.25, cl * 0.25, 0.26); for (int j = 0; j < nx; j++) GA(&l, BARL(FF(&f2, xs[j], -0.18), FF(&f2, xs[j], 0.18), 0.12)); CUT(o, UL(&l), ld, 0.1); }
            ON(o, RECTp(c, cl - 0.7, 0.42, t + 1.5708 * 0), ld, 0.1, NULL);
            HANDLE(o, FF(&f2, -cl * 0.2, cw / 2 - 0.45), FF(&f2, cl * 0.2, cw / 2 - 0.45), ld, 0.3, 0.28, NULL);
        }
    } else if (c0 < 0.8) {
        double y = u(-0.2, 0.2); P c = FF(&ff, 0.2, y); double r = u(1.05, 1.25); GG *dg = DISCp(c, r, 22); double dh = u(2.4, 2.8); int dr = ON(o, dg, b, dh, NULL); ON(o, ANN(c.x, c.y, r - 0.13, r - 0.45, 22), dr, 0.12, NULL);
        double t = a + u(0, 6.28); CUT(o, DISCp(POLp(c, r * 0.5, t), 0.22, 8), dr, 0.12); { P sp[1] = { POLp(c, r * 0.45, a + 2.0) }; STUDS(o, sp, 1, dr, 0.2, 0.15, NULL, 8); }
    } else {
        GL l = { 0 }; double xs[64]; int nx = AR(xs, 64, -L / 2 + 1.3, L / 2 - 0.9, 0.5);
        for (int i = 0; i < nx; i++) { P q[3] = { FF(&ff, xs[i] - 0.35, -W * 0.28), FF(&ff, xs[i], 0), FF(&ff, xs[i] - 0.35, W * 0.28) }; GA(&l, LINE(q, 3, 0.16)); }
        CUT(o, UL(&l), b, 0.12);
    }
    *foot = OUT(o);
}

/* welding_set */
typedef struct { double Lb, r; } Weld;
static int weld_rf(void *c, double t, double *out) {
    Weld *w = (Weld *)c; double Lb = w->Lb, r = w->r, x = t + Lb / 2;
    if (x < 0.08) *out = r * 0.95;
    else if (x < Lb - 0.75) *out = r + (fabs(x - Lb * 0.55) < 0.12 ? 0.05 : 0.0);
    else if (x < Lb - 0.45) *out = r * MX(0.52, COS((x - (Lb - 0.75)) / 0.3 * 1.15));
    else *out = x < Lb - 0.2 ? 0.33 : 0.26;
    return SO_OK;
}
static double torch1(void *c, double s) { (void)c; return s < 0.25 ? 0.3 : 0.24; }
static double torch2(void *c, double s) { (void)c; return 0.2 - 0.06 * s; }
static void welding_set(double cx, double cy, double a, PyRandom *rng, SoObj *o, GG **foot) {
    F ff = FR(cx, cy, a); double L = u(3.6, 4.4), r = 0.62; so_obj_init(o, "metal");
    UP(o, RECTp(FF(&ff, -L / 2 - 0.05, 0), 0.35, 4 * r + 0.6, a), 0.7, NULL);
    S *ps = SN(); double svs[2] = { -1, 1 }, fs[2] = { 1.0, 0.92 };
    for (int i = 0; i < 2; i++) {
        double sv = svs[i], Lb = L * fs[i], xm = -L / 2 + 0.1 + Lb / 2; Weld *w = (Weld *)xr_mem(sizeof *w); w->Lb = Lb; w->r = r;
        P q = FF(&ff, xm, sv * (r + 0.02)); SPIN(ps, q.x, q.y, a, Lb, weld_rf, w, 1.25, 0, "round");
        P rg = FF(&ff, -L / 2 + 0.1 + Lb + 0.25, sv * r); CYL(ps, rg.x, rg.y, 0.34, 1.0, 0.0, 10);
        for (int k = -1; k <= 1; k += 2) { P z = POLp(rg, 0.36, a + 1.5708 * k + 0.6); CYL(ps, z.x, z.y, 0.24, 0.8, 0.0, 8); }
    }
    { P q = FF(&ff, -L * 0.05, 0); LIE(ps, q.x, q.y, a + 1.5708, 0.3, 2 * r + 0.15, 0.0); }
    ST(o, ps, NULL, -1, NULL, 0, RECTp(FF(&ff, 0.2, 0), L + 0.8, 4 * r + 0.3, a));
    { S *s = SN(); for (int i = 0; i < 2; i++) { P q = FF(&ff, -L / 2 + 0.35, sgn(i) * (2 * r + 0.5)); LIE(s, q.x, q.y, a + 1.5708, 0.36, 0.62, 0.0); } ST(o, s, "tar", -1, NULL, 0, RECTp(FF(&ff, -L / 2 + 0.35, 0), 1.3, 4 * r + 1.4, a)); }
    double d1 = u(1.5, 2.5), d2 = u(-2.1, 2.1); P e = FF(&ff, L / 2 + d1, d2); P h0 = FF(&ff, L / 2 + 0.5, 0); P h[8], hb[8];
    int nh = lf_wiggle(h0.x, h0.y, e.x, e.y, rng, 0.6, 4, &h[0].x);
    for (int i = 0; i < nh; i++) { hb[i].x = h[i].x + 0.18 * COS(a + 1.5708); hb[i].y = h[i].y + 0.18 * SIN(a + 1.5708); }
    { S *s = SN(); ROD(s, h, nh, 0.22, NULL, NULL, 1.0, 0, 0); ROD(s, hb, nh, 0.22, NULL, NULL, 1.0, 0, 0); ST(o, s, "cable", -1, NULL, 0, BUF(LSTR(h, nh), 0.5, 16)); }
    double t = ATAN2(e.y - h[nh - 2].y, e.x - h[nh - 2].x); P q = POLp(e, 1.2, t);
    { S *s = SN(); XBAR(s, POLp(e, -0.2, t), POLp(e, 0.7, t), 0, torch1, NULL, 0.6, 0.1); XBAR(s, POLp(e, 0.7, t), q, 0, torch2, NULL, 0.6, 0.1);
      P z = POLp(e, 0.1, t + 1.5708); CYL(s, z.x, z.y, 0.18, 0.62, 0.0, 6); ST(o, s, NULL, -1, NULL, 0, DISCp(e, 0.8, 20)); }
    *foot = OUT(o);
}

/* gas_cylinder */
typedef struct { double Lb, r; } Gas;
static double gas_rf(void *c, double s) {
    Gas *g = (Gas *)c; double Lb = g->Lb, r = g->r, x = s * Lb;
    if (x < 0.12) return r * 1.04;
    if (x < 0.2) return r * 0.97;
    if (x < Lb - r) return r + (fabs(x - (Lb - r - 0.45)) < 0.1 ? 0.05 : 0.0);
    return MX(0.4, r * sqrt(MX(0.0, 1 - PW((x - (Lb - r)) / r, 2))));
}
static double cap_rf(void *c, double s) { (void)c; return s < 0.8 ? 0.55 : 0.45; }
static void gas_cylinder(double cx, double cy, double a, PyRandom *rng, SoObj *o, GG **foot) {
    F ff = FR(cx, cy, a); double r = u(0.75, 0.9); so_obj_init(o, "metal"); int capd = rnd() < 0.5; double L = capd ? u(5.2, 5.8) : u(4.0, 4.8);
    double Lb = L - (capd ? 0.9 : 0.6), xb = -L / 2; Gas *gc = (Gas *)xr_mem(sizeof *gc); gc->Lb = Lb; gc->r = r;
    S *ps = SN(); XBAR(ps, FF(&ff, xb, 0), FF(&ff, xb + Lb, 0), 0, gas_rf, gc, 0.45, 0.1); double top = 1.45 * r;
    C cuts[3]; int nc = 0;
    if (capd) {
        XBAR(ps, FF(&ff, xb + Lb - 0.05, 0), FF(&ff, xb + Lb + 0.85, 0), 0, cap_rf, NULL, 0.45, 0.1);
        cuts[nc].q = ELL(PXY(FF(&ff, xb + Lb + 0.4, 0)), 0.22, 0.14, a, 8); cuts[nc].fl = 0.4; nc++;
    } else {
        XBAR(ps, FF(&ff, xb + Lb - 0.05, 0), FF(&ff, xb + Lb + 0.55, 0), 0.3, NULL, NULL, 0.45, 0.1);
        P q = FF(&ff, xb + Lb + 0.3, 0); LIE(ps, q.x, q.y, a + 1.5708, 0.2, 0.5, 0.0);
    }
    { GL l = { 0 }; double xs[64]; int nx = AR(xs, 64, xb + Lb * 0.3, xb + Lb * 0.62, 0.3); for (int i = 0; i < nx; i++) GA(&l, RECTp(FF(&ff, xs[i], 0), 0.14, 0.42, a)); cuts[nc].q = UL(&l); cuts[nc].fl = top - 0.12; nc++; }
    if (rnd() < 0.35) { double d = u(0.2, 0.5); cuts[nc].q = ELL(PXY(FF(&ff, xb + Lb * d, r * 0.4)), 0.5, 0.35, a, 10); cuts[nc].fl = top - 0.18; nc++; }
    ST(o, ps, NULL, -1, cuts, nc, RECT(cx, cy, L, 2 * r + 0.3, a));
    P h[3]; int nh;
    if (!capd) {
        h[0] = FF(&ff, L / 2 + 0.1, 0); double d = u(-1, 1); h[1] = FF(&ff, L / 2 + 1.0, d); double x = u(0.4, 1.6), y = u(1.4, 2.2); y = y * CH2(rng); h[2] = FF(&ff, L / 2 + x, y); nh = 3;
        xc_hose(o, h, 3, 0.24, "cable");
    } else { h[0] = FF(&ff, L / 2 + 0.15, 0.4); h[1] = FF(&ff, L / 2 + 0.5, 0.4); nh = 2; }
    GG *ex[1] = { BUF(LSTR(h, nh), 0.4, 16) };
    *foot = OUTX(o, ex, 1);
}

/* plate_stack */
static void plate_stack(double cx, double cy, double a, PyRandom *rng, SoObj *o, GG **foot) {
    double L = u(3.0, 3.5), W = u(2.4, 2.8); so_obj_init(o, "plate"); int n = (int)RI(rng, 4, 6); double th = u(0.22, 0.26); int par = -1;
    double ds = u(0, 6.28), st = u(0.13, 0.17); P p0 = POL(cx, cy, -st * (n - 1) / 2, ds); double x = p0.x, y = p0.y, t = a;
    for (int i = 0; i < n; i++) {
        GG *g = RECT(x, y, L, W, t); par = UP(o, g, th * (i + 1) + 0.05, NULL);
        if (i < n - 1) { double d = u(-0.5, 0.5); P q = POL(x, y, st, ds + d); x = q.x; y = q.y; t = a + u(-0.04, 0.04); }
    }
    F ff = FR(x, y, t);
    { GL l = { 0 }; for (int i = 0; i < 2; i++) for (int j = 0; j < 2; j++) GA(&l, DISCp(FF(&ff, sgn(i) * (L / 2 - 0.4), sgn(j) * (W / 2 - 0.4)), 0.17, 6)); GA(&l, CAPL(FF(&ff, -0.35, W / 2 - 0.42), FF(&ff, 0.35, W / 2 - 0.42), 0.24)); CUT(o, UL(&l), par, 0.16); }
    { GL l = { 0 }; int k = 0; for (int i = -5; i < 6; i++) for (int j = -4; j < 5; j++) { int jm = ((j % 2) + 2) % 2; P q = FF(&ff, i * 0.5 + jm * 0.25, j * 0.4); GA(&l, ELL(q.x, q.y, 0.2, 0.08, t + ((k % 2) ? 0.6 : -0.6), 6)); k++; }
      CUT(o, INT(UL(&l), RECT(x, y, L - 1.3, W - 1.3, t)), par, 0.1); }
    if (rnd() < 0.6) {
        GL l = { 0 };
        if (rnd() < 0.5) { for (int i = 0; i < 2; i++) GA(&l, RECTp(FF(&ff, sgn(i) * L * 0.3, 0), 0.4, W + 0.2, t)); }
        else GA(&l, RECTp(FF(&ff, 1 * L * 0.3, 0), 0.4, W + 0.2, t));
        ON(o, UL(&l), par, 0.08, NULL);
    }
    int m = (int)RI(rng, 1, 3); F f0 = FR(cx, cy, a);
    for (int i = 0; i < m; i++) { double x1 = u(-L * 0.35, L * 0.35), sv = CH2(rng); double y1 = sv * (W / 2 + u(0.55, 0.8)); P p = FF(&f0, x1, y1); double rot = u(0, 1); UP(o, NGON(p.x, p.y, 0.3, 6, rot), 0.3, NULL); }
    *foot = OUT(o);
}

static const XrDef DEFS[] = {
    { "corridor", "made", "metal", "terminal", 10.5, 0, 0, terminal },
    { "corridor", "made", "plate", "deck_hatch", 10.5, 0, 0, deck_hatch },
    { "corridor", "made", "metal", "hover_pallet", 10.5, 0, 0, hover_pallet },
    { "corridor", "made", "metal", "welding_set", 10.5, 0, 0, welding_set },
    { "corridor", "made", "metal", "gas_cylinder", 10.5, 0, 0, gas_cylinder },
    { "corridor", "made", "plate", "plate_stack", 10.5, 0, 0, plate_stack },
};
void bld_sdrx_corridor(void) { xr_register(DEFS, (int)(sizeof DEFS / sizeof DEFS[0])); }
