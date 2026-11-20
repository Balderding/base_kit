/* bld_sd_rx_dungeon.c: sd_r_x_dungeon.py in C (see bld_sd_rx.h; x_wall is XWALL in bld_sd_rx.c). */
#include "bld_sd_rx.h"

static double sgn(int i) { return i ? 1 : -1; }

/* gibbet_cage */
typedef struct { double x0, L, r, th; } Cage;
static double cage_R(const Cage *c, double x) {
    double s = (x - c->x0) / c->L;
    if (s < 0.16) return 0.62 * PW(SIN(PI * MX(0.05, s) / 0.16), 0.5) + 0.2;
    if (s < 0.2) return 0.6;
    if (s < 0.3) return 0.6 + 0.5 * (s - 0.2) / 0.1;
    if (s < 0.6) return 1.1 - 0.12 * (s - 0.3) / 0.3;
    return MX(0.55, 0.98 - 0.4 * (s - 0.6) / 0.4);
}
static double hoop_z(void *p, double s) { Cage *c = (Cage *)p; double v = 1 - PW(2 * s - 1, 2); return c->r * (0.7 + sqrt(MX(0.0, v))); }
static double strap_z(void *p, double s) { Cage *c = (Cage *)p; return cage_R(c, c->x0 + 0.15 + s * (c->L - 0.25)) * (0.7 + 0.97 * SIN(c->th)); }
static double leg_r(void *p, double s) { (void)p; return (s < 0.08 || s > 0.92) ? 0.26 : 0.2; }
static void gibbet_cage(double cx, double cy, double a, PyRandom *rng, SoObj *o, GG **foot) {
    F ff = FR(cx, cy, a); double L = u(5.4, 5.7), x0 = -L / 2 + 0.3; so_obj_init(o, "iron");
    S *ps = SN(); static const double HS[9] = { 0.04, 0.12, 0.25, 0.36, 0.47, 0.58, 0.7, 0.82, 0.95 }, TH[5] = { 0.35, 0.95, 1.5708, 2.2, 2.8 };
    Cage base = { x0, L, 0, 0 };
    for (int i = 0; i < 9; i++) {
        double x = x0 + L * HS[i]; Cage *c = (Cage *)xr_mem(sizeof *c); *c = base; c->r = cage_R(&base, x);
        P l[2] = { FF(&ff, x, -c->r), FF(&ff, x, c->r) }; XWALL(ps, l, 2, 0.28, hoop_z, c);
    }
    for (int i = 0; i < 5; i++) {
        double xs[30]; P l[30]; LS(xs, x0 + 0.15, x0 + L - 0.1, 30); Cage *c = (Cage *)xr_mem(sizeof *c); *c = base; c->th = TH[i];
        for (int j = 0; j < 30; j++) l[j] = FF(&ff, xs[j], cage_R(&base, xs[j]) * COS(TH[i]));
        XWALL(ps, l, 30, 0.26, strap_z, c);
    }
    P rg = FF(&ff, x0 - 0.45, 0); EXT(ps, ANN(rg.x, rg.y, 0.5, 0.22, 12), 0.5, 0);
    ST(o, ps, NULL, -1, NULL, 0, RECTp(FF(&ff, 0, 0), L + 0.6, 2.4, a));
    double y = u(-0.1, 0.1); P sk = FF(&ff, x0 + L * 0.08, y); S *bo = SN(); DOME(bo, sk.x, sk.y, 0.46, 0.95, 0, 0.85, a, 20);
    { double xs[64]; int n = AR(xs, 64, x0 + L * 0.27, x0 + L * 0.45, 0.32); for (int i = 0; i < n; i++) TBARz(bo, FF(&ff, xs[i], -0.75), FF(&ff, xs[i] + 0.12, 0.75), 0.2, 0.8, 0.8); }
    TBARz(bo, FF(&ff, x0 + L * 0.22, 0), FF(&ff, x0 + L * 0.5, 0), 0.2, 0.45, 0.45);
    for (int i = 0; i < 2; i++) { double sv = sgn(i); P p = FF(&ff, x0 + L * 0.55, sv * 0.35); double d = u(0.2, 0.4); TBAR(bo, p, FF(&ff, x0 + L * 0.93, sv * d), 0, leg_r, NULL, 0.4, 1, 0.32, 0.32); }
    { GL l = { 0 }; for (int i = 0; i < 2; i++) GA(&l, DISCp(POLp(sk, 0.2, a + sgn(i) * 1.0), 0.14, 8)); C cu[1] = { { UL(&l), 0.45 } }; ST(o, bo, "bone", -1, cu, 1, DISCp(sk, 0.5, 20)); }
    if (rnd() < 0.8) { P p = FF(&ff, x0 - 0.95, 0); double d1 = u(0.3, 0.6), d2 = u(-1.4, 1.4); XCHAIN(o, p, FF(&ff, x0 - 0.95 - d1, d2), "iron", 0.22, 0.56); }
    *foot = OUT(o);
}

/* pressure_plate */
static void pressure_plate(double cx, double cy, double a, PyRandom *rng, SoObj *o, GG **foot) {
    F ff = FR(cx, cy, a); so_obj_init(o, "flag"); double Sz = u(2.1, 2.3), cell = Sz + 0.5, fw = cell * 0.55;
    double xs[2] = { -(cell + fw) / 2 - 0.05, (cell + fw) / 2 + 0.05 }, vs[2] = { -cell * 0.25, cell * 0.25 };
    for (int i = 0; i < 2; i++) for (int j = 0; j < 2; j++) {
        double x = xs[i], v = vs[j], d1 = u(-0.04, 0.04), d2 = u(-0.04, 0.04); P c = FF(&ff, x + d1, v + d2); double t = a + u(-0.03, 0.03);
        GG *g = BUF(BUFX(RECTp(c, fw - 0.14, cell / 2 - 0.14, t), -0.08, 16, SHP_CAP_ROUND, SHP_JOIN_MITRE, 5.0), 0.08, 4);
        if (rnd() < 0.15) continue;
        double hh = u(0.3, 0.38); int b = UP(o, g, hh, NULL);
        if (rnd() < 0.4) { double e1 = u(-0.4, 0.4); P p = FF(&ff, x - fw * 0.4, v + e1); double e2 = u(-0.4, 0.4); P q = FF(&ff, x + fw * 0.4, v + e2); CUT(o, WAVY(p, q, rng, 0.25, 3, 0.12), b, 0.1); }
    }
    GG *fr = RECT(cx, cy, cell * 0.98, cell * 0.98, a); int b = UP(o, fr, 0.32, NULL);
    GG *pg = RECT(cx, cy, Sz, Sz, a); CUT(o, BUFX(EXTR(pg), 0.12, 16, SHP_CAP_ROUND, SHP_JOIN_MITRE, 5.0), b, 0.22);
    { GL l = { 0 }; for (int i = 0; i < 2; i++) GA(&l, RECTp(FF(&ff, 0, sgn(i) * (cell / 2 - 0.17)), 1.4, 0.16, a)); CUT(o, UL(&l), b, 0.25); }
    int pl = ON(o, INS(pg, 0.16), b, 0.12, NULL);
    { GL l = { 0 }; for (int i = 0; i < 2; i++) GA(&l, DISCp(FF(&ff, -0.25, sgn(i) * 0.36), 0.28, 10)); GA(&l, TRI(FF(&ff, 0.15, 0), FF(&ff, 0.45, 0), 0.32)); CUT(o, UL(&l), pl, 0.12); }
    GROOVE(o, ELL(cx, cy, Sz * 0.33, Sz * 0.3, a, 20), pl, 0.12, 0.08);
    { GL l = { 0 }; double v4[4] = { -0.3, -0.1, 0.1, 0.3 }; for (int i = 0; i < 4; i++) GA(&l, BARL(FF(&ff, 0.62, v4[i]), FF(&ff, 0.88, v4[i]), 0.12)); CUT(o, UL(&l), pl, 0.08); }
    int n = (int)RI(rng, 2, 3);
    for (int i = 0; i < n; i++) {
        double sx = CH2(rng); sx = sx * u(cell * 0.55, cell * 0.8); double y = u(-cell * 0.4, cell * 0.4); P p = FF(&ff, sx, y);
        double t = a + 1.5708 + u(-0.6, 0.6); double r = u(1.0, 1.2); P q = POLp(p, r, t); P pq[2] = { p, q };
        GL l = { 0 }; GA(&l, BARL(p, q, 0.22)); GA(&l, TRI(q, POLp(q, 0.35, t), 0.3)); GA(&l, TRI(POLp(p, 0.3, t), POLp(p, -0.15, t), 0.6));
        S *s = SN(); EXT(s, UL(&l), 0.22, 0); ST(o, s, "iron", -1, NULL, 0, BUF(LSTR(pq, 2), 0.4, 16));
    }
    *foot = OUT(o);
}

/* stocks */
static void stocks(double cx, double cy, double a, PyRandom *rng, SoObj *o, GG **foot) {
    F ff = FR(cx, cy, a); double L = u(5.0, 5.3), Wb = u(0.95, 1.05), th = u(0.45, 0.5); so_obj_init(o, "wood");
    double op = 0.0; if (rnd() < 0.4) op = u(0.15, 0.6);
    GG *holes; { GL l = { 0 }; GA(&l, DISCp(FF(&ff, 0, 0), 0.62, 16)); for (int i = 0; i < 2; i++) GA(&l, DISCp(FF(&ff, sgn(i) * L * 0.3, 0), 0.38, 12)); holes = UL(&l); }
    for (int i = 0; i < 2; i++) {
        double sv = sgn(i), opv = sv > 0 ? op : 0.0;
        P q = FF(&ff, 0, opv);
        GG *g = DIF(RECTp(FF(&ff, 0, sv * (Wb / 2 + 0.04) + opv), L, Wb, a), TRANS(holes, q.x - cx, q.y - cy));
        int b = UP(o, g, th, NULL);
        GL l = { 0 }; double ks[2] = { 0.3, 0.7 };
        for (int k = 0; k < 2; k++) GA(&l, BARL(FF(&ff, -L, sv * Wb * ks[k] + opv), FF(&ff, L, sv * Wb * ks[k] + opv), 0.11));
        CUT(o, DIF(INT(UL(&l), INS(g, 0.25)), BUF(holes, 0.15, 16)), b, 0.08);
        ON(o, U1(RECTp(FF(&ff, -L / 2 + 0.3, sv * (Wb / 2 + 0.04) + opv), 0.36, Wb - 0.1, a)), b, 0.1, "iron");
    }
    { P sp[2]; for (int i = 0; i < 2; i++) { double sv = sgn(i); sp[i] = FF(&ff, -L / 2 + 0.3, sv * 0.5 + (sv > 0 ? op : 0)); } STUDS(o, sp, 2, 0, 0.2, 0.16, "iron", 8); }
    P hp = FF(&ff, L / 2 - 0.35, 0.05 + op / 2); S *s = SN(); EXT(s, RECT(hp.x, hp.y, 0.4, 1.1 + op, a), th + 0.12, 0); EXT(s, DISCp(FF(&ff, L / 2 + 0.25, op / 2), 0.36, 10), th + 0.05, 0);
    ST(o, s, "iron", -1, NULL, 0, DISCp(hp, 0.6, 20));
    s = SN(); { P c1 = FF(&ff, L / 2 + 0.6, op / 2); EXT(s, U2(ANN(c1.x, c1.y, 0.32, 0.14, 10), RRECTp(FF(&ff, L / 2 + 0.95, op / 2), 0.6, 0.75, a, 0.15)), 0.5, 0); }
    ST(o, s, "iron", -1, NULL, 0, DISCp(FF(&ff, L / 2 + 0.8, op / 2), 0.6, 20));
    double vb = -(Wb + 0.08), pl = u(3.0, 3.4);
    for (int i = 0; i < 2; i++) {
        double sx = sgn(i), x = sx * (L / 2 - 0.35); GG *pg = RECTp(FF(&ff, x, vb - pl / 2 + 0.3), 0.62, pl, a); int p = UP(o, pg, 0.62, NULL);
        CUT(o, BARL(FF(&ff, x - 0.1, vb - 0.3), FF(&ff, x + 0.05, vb - pl + 0.6), 0.1), p, 0.08);
        GG *fg = RECTp(FF(&ff, x + sx * 0.0, vb - pl + 0.1), 1.6, 0.6, a); double hh = u(1.6, 1.8); int f = UP(o, fg, hh, NULL);
        CUT(o, BARL(FF(&ff, x - 0.8, vb - pl + 0.1), FF(&ff, x + 0.8, vb - pl + 0.1), 0.1), f, 0.08);
    }
    *foot = OUT(o);
}

/* portcullis_piece */
static double torn_z(void *c, double s) { (void)c; return 0.42 + 1.1 * PW(s, 1.5); }
static void portcullis_piece(double cx, double cy, double a, PyRandom *rng, SoObj *o, GG **foot) {
    F ff = FR(cx, cy, a); so_obj_init(o, "iron"); int nx = (int)RI(rng, 3, 4), ny = (int)RI(rng, 3, 4); double s = u(1.15, 1.3), w = 0.42;
    double X[4], Y[4]; for (int i = 0; i < nx; i++) X[i] = (i - (nx - 1) / 2.0) * s; for (int j = 0; j < ny; j++) Y[j] = (j - (ny - 1) / 2.0) * s;
    double y0 = Y[0] - 0.6, y1 = Y[ny - 1] + 0.5;
    GL ups = { 0 };
    for (int i = 0; i < nx; i++) {
        double x = X[i], top = y1 + (rnd() < 0.5 ? u(0.0, 0.5) : 0.0);
        GA(&ups, U2(BARL(FF(&ff, x, y0), FF(&ff, x, top), w), TRI(FF(&ff, x, y0 + 0.05), FF(&ff, x, y0 - 0.9), w + 0.12)));
    }
    int b = UP(o, UL(&ups), 0.42, NULL); (void)b;
    GL cl = { 0 };
    for (int j = 0; j < ny; j++) { double e = rnd() < 0.6 ? 0.45 : -s * 0.5; GA(&cl, BARL(FF(&ff, X[0] - 0.45, Y[j]), FF(&ff, X[nx - 1] + e, Y[j]), w)); }
    GG *cr = UL(&cl); int c = UP(o, cr, 0.84, NULL);
    { GG *hl = CHULL(cr); GL l = { 0 }; for (int i = 0; i < nx; i++) for (int j = 0; j < ny; j++) { P q = FF(&ff, X[i], Y[j]); if (CONTAINS(hl, PTG(q.x, q.y))) GA(&l, RECTp(q, 0.6, 0.6, a)); } ON(o, UL(&l), c, 0.1, NULL); }
    { P sp[16]; int n = 0; for (int i = 0; i < nx; i++) for (int j = 0; j < ny; j++) sp[n++] = FF(&ff, X[i], Y[j]); STUDS(o, sp, n, c, 0.2, 0.22, NULL, 8); }
    if (rnd() < 0.7) {
        double x = CH(rng, X, nx); P l[2] = { FF(&ff, x, y1 - 0.1), FF(&ff, x, y1 + 1.4) }; S *ps = SN(); XWALL(ps, l, 2, w, torn_z, NULL);
        ST(o, ps, NULL, -1, NULL, 0, RECTp(FF(&ff, x, y1 + 0.7), w, 1.5, a));
    }
    { S *ps = SN(); int n = (int)RI(rng, 2, 4); for (int i = 0; i < n; i++) { double x = u(-s * nx / 2, s * nx / 2), y = u(y0, y1); P q = FF(&ff, x, y); DOME(ps, q.x, q.y, 0.3, 0.6, 0, 0.8, 0.0, 20); }
      ST(o, ps, "rust", -1, NULL, 0, RECT(cx, cy, 1, 1, a)); }
    int n = (int)RI(rng, 1, 2);
    for (int i = 0; i < n; i++) {
        double sx = CH2(rng); sx = sx * (s * nx / 2 + u(0.6, 1.0)); double y = u(y0, y1); P c2 = FF(&ff, sx, y); double t = u(0, 3.14); P q[5];
        for (int k = 0; k < 5; k++) { double r = u(0.5, 0.8), d = u(-0.2, 0.2); q[k] = POLp(c2, r, t + k * 1.3 + d); }
        GG *g = POLY(q, 5); double hh = u(0.6, 0.9); int bs = UP(o, g, hh, "flag"); ON(o, INS(g, 0.15), bs, 0.1, "flag");
    }
    *foot = OUT(o);
}

/* mimic */
typedef struct { double zb, zf; } LidZ;
static double lid_z(void *c, double s) { LidZ *l = (LidZ *)c; return l->zb + (l->zf - l->zb) * s; }
static double tongue1(void *c, double s) { (void)c; (void)s; return 0.5; }
static double tongue2(void *c, double s) { (void)c; return 0.5 - 0.15 * s; }
static void mimic(double cx, double cy, double a, PyRandom *rng, SoObj *o, GG **foot) {
    F ff = FR(cx, cy, a); double L = u(4.9, 5.2), W = u(2.5, 2.8), h = u(2.3, 2.6); so_obj_init(o, "wood"); GG *g = RECT(cx, cy, L, W, a);
    int b = UP(o, g, h, NULL); GG *mo = RECTp(FF(&ff, 0, -W * 0.08), L - 0.7, W * 0.42, a);
    CUT(o, mo, b, h * 0.6);
    SEAMS(o, &ff, L, W, b, 0.55, 0, 0.1, 0.06, BUF(mo, 0.1, 16));
    double bands[2] = { -L * 0.32, L * 0.32 };
    { GL l = { 0 }; for (int i = 0; i < 2; i++) GA(&l, RECTp(FF(&ff, bands[i], 0), 0.4, W, a)); for (int i = 0; i < 2; i++) GA(&l, RECTp(FF(&ff, sgn(i) * (L / 2 - 0.25), 0), 0.5, W, a)); ON(o, DIF(UL(&l), BUF(mo, 0.05, 16)), b, 0.1, "iron"); }
    { P sp[4]; int n = 0; for (int i = 0; i < 2; i++) { sp[n++] = FF(&ff, bands[i], -W / 2 + 0.25); sp[n++] = FF(&ff, bands[i], W / 2 - 0.25); } STUDS(o, sp, 4, b, 0.2, 0.2, "iron", 8); }
    { S *s = SN(); double xs[64]; int n = AR(xs, 64, -L / 2 + 0.65, L / 2 - 0.5, 0.55); for (int i = 0; i < n; i++) { P q = FF(&ff, xs[i], -W * 0.08 - W * 0.21 + 0.22); CONE(s, q.x, q.y, 0.3, h + 0.65, 0.06, 0, 6, 1.0, 0.0); }
      ST(o, s, "bone", -1, NULL, 0, RECTp(FF(&ff, 0, -W * 0.29), L - 0.8, 0.5, a)); }
    double lv0 = W * 0.13, lv1 = W / 2, zb = h + 0.1, zf = h + u(1.0, 1.3);
    GG *lg = RECTp(FF(&ff, 0, (lv0 + lv1) / 2), L, lv1 - lv0, a);
    LidZ *lz = (LidZ *)xr_mem(sizeof *lz); lz->zb = zb; lz->zf = zf;
    { S *s = SN(); P l[2] = { FF(&ff, 0, lv1), FF(&ff, 0, lv0) }; XWALL(s, l, 2, L, lid_z, lz);
      GL r = { 0 }; for (int i = 0; i < 2; i++) GA(&r, RECTp(FF(&ff, bands[i], (lv0 + lv1) / 2), 0.36, W, a)); C cu[1] = { { UL(&r), zf + 0.5 } }; ST(o, s, NULL, -1, cu, 1, lg); }
    P ey[2]; for (int i = 0; i < 2; i++) ey[i] = FF(&ff, sgn(i) * L * 0.18, lv0 + 0.4);
    { S *s = SN(); for (int i = 0; i < 2; i++) DOME(s, ey[i].x, ey[i].y, 0.32, zf + 0.3, 0, 0.8, a, 20);
      double xs[64]; int n = AR(xs, 64, -L / 2 + 0.6, L / 2 - 0.4, 0.55); for (int i = 0; i < n; i++) { P q = FF(&ff, xs[i], lv0 + 0.16); CONE(s, q.x, q.y, 0.3, zf + 0.5, 0.06, 0, 6, 1.0, 0.0); }
      GL l = { 0 }; for (int i = 0; i < 2; i++) GA(&l, DISCp(ey[i], 0.12, 6)); C cu[1] = { { UL(&l), zf + 0.1 } };
      ST(o, s, "bone", -1, cu, 1, RECTp(FF(&ff, 0, lv0 + 0.3), L, 0.6, a)); }
    double ex = u(-0.8, 0.8), eyv = u(1.4, 2.0); P e = FF(&ff, ex, -W / 2 - eyv);
    P tp[3]; { double d = u(-0.3, 0.3); tp[0] = FF(&ff, d, -W * 0.1); d = u(-0.3, 0.3); tp[1] = FF(&ff, d, -W / 2 - 0.1); tp[2] = e; }
    { S *s = SN(); TBAR(s, tp[0], tp[1], 0, tongue1, NULL, 0.4, 1, h * 0.55, h + 0.05); TBAR(s, tp[1], tp[2], 0, tongue2, NULL, 0.4, 1, h * 0.9, 0.3);
      C cu[1] = { { BARL(tp[1], tp[2], 0.12), 0.45 } }; ST(o, s, "flesh", -1, cu, 1, BUF(LSTR(tp, 3), 0.5, 16)); }
    P lk = FF(&ff, 0, -W / 2 - 0.15); { S *s = SN(); EXT(s, RECT(lk.x, lk.y, 0.8, 0.3, a), h * 0.8, 0); ST(o, s, "iron", -1, NULL, 0, RECT(lk.x, lk.y, 0.8, 0.3, a)); }
    *foot = OUT(o);
}

/* keystone */
static GG *wedge(PyRandom *rng, P c, double t, double w0, double w1, double l, F *f2o) {
    F f2 = FR(c.x, c.y, t); P q[4] = { FF(&f2, -w0 / 2, -l / 2), FF(&f2, w0 / 2, -l / 2), FF(&f2, w1 / 2, l / 2), FF(&f2, -w1 / 2, l / 2) }; GG *g = POLY(q, 4);
    int n = (int)RI(rng, 0, 2);
    for (int i = 0; i < n; i++) { double s1 = CH2(rng), s2 = CH2(rng); P d = FF(&f2, s1 * w1 / 2, s2 * l / 2); double r = u(0.2, 0.4); g = DIF(g, DISCp(d, r, 6)); }
    *f2o = f2; return g;
}
static void keystone(double cx, double cy, double a, PyRandom *rng, SoObj *o, GG **foot) {
    F ff = FR(cx, cy, a); so_obj_init(o, "flag"); double h = u(1.45, 1.6); F f2;
    double t = a + u(-0.2, 0.2); GG *ks = wedge(rng, PT(cx, cy), t, 1.4, 1.95, 2.2, &f2); int b = UP(o, ks, h + 0.25, NULL); int fc = ON(o, INS(ks, 0.22), b, 0.12, NULL);
    int k = (int)RI(rng, 0, 2);
    if (k == 0) {
        { GL l = { 0 }; for (int i = 0; i < 2; i++) GA(&l, ELL(PXY(FF(&f2, sgn(i) * 0.32, 0.35)), 0.32, 0.16, 0, 8)); ON(o, UL(&l), fc, 0.12, NULL); }
        { GL l = { 0 }; for (int i = 0; i < 2; i++) GA(&l, DISCp(FF(&f2, sgn(i) * 0.32, 0.12), 0.18, 8)); CUT(o, UL(&l), fc, 0.14); }
        { P q[3] = { FF(&f2, 0, 0.25), FF(&f2, 0.18, -0.25), FF(&f2, -0.18, -0.25) }; ON(o, POLY(q, 3), fc, 0.14, NULL); }
        CUT(o, ELL(PXY(FF(&f2, 0, -0.55)), 0.34, 0.11, 0, 10), fc, 0.12);
    } else if (k == 1) {
        int bs = ON(o, DISCp(FF(&f2, 0, 0), 0.6, 16), fc, 0.14, NULL);
        CUT(o, U2(BARL(FF(&f2, 0, -0.42), FF(&f2, 0, 0.42), 0.14), BARL(FF(&f2, -0.42, 0), FF(&f2, 0.42, 0), 0.14)), bs, 0.08);
    } else {
        P l3[3] = { FF(&f2, -0.3, -0.5), FF(&f2, 0, 0.5), FF(&f2, 0.3, -0.5) };
        CUT(o, U2(LINE(l3, 3, 0.14), BARL(FF(&f2, -0.35, 0.0), FF(&f2, 0.35, 0.0), 0.14)), fc, 0.12);
    }
    double sxs[3]; int ns = 2; { int64_t ix[2]; static const double pm[2] = { -1, 1 }; xr_ok(py_sample_e(rng, 2, 2, ix)); sxs[0] = pm[ix[0]]; sxs[1] = pm[ix[1]]; }
    if (rnd() < 0.5) sxs[ns++] = CH2(rng);
    for (int i = 0; i < ns; i++) {
        double sx = sxs[i], x = sx * u(1.6, 1.75) * (i < 2 ? 1.0 : 0.0);
        if (i < 2) x = x + 0; else x = x + u(-0.4, 0.4);
        double y = i < 2 ? 0.0 : CH2(rng) * 1.9; y = y + u(-0.2, 0.2);
        P c = FF(&ff, x, y); double tt = a + u(-0.4, 0.4) + (i == 2 ? 1.57 : 0.0); F f3;
        GG *g = wedge(rng, c, tt, 1.0, 1.3, 1.8, &f3); double hh = u(1.25, 1.4); int vb = UP(o, g, hh, NULL); ON(o, INS(g, 0.2), vb, 0.1, NULL);
    }
    *foot = OUT(o);
}

static const XrDef DEFS[] = {
    { "dungeon", "made", "iron", "gibbet_cage", 10.5, 0, 1, gibbet_cage },
    { "dungeon", "made", "flag", "pressure_plate", 10.5, 0, 1, pressure_plate },
    { "dungeon", "made", "wood", "stocks", 10.5, 0, 1, stocks },
    { "dungeon", "made", "iron", "portcullis_piece", 10.5, 0, 1, portcullis_piece },
    { "dungeon", "made", "wood", "mimic", 10.5, 0, 1, mimic },
    { "dungeon", "made", "flag", "keystone", 10.5, 0, 1, keystone },
};
void bld_sdrx_dungeon(void) { xr_register(DEFS, (int)(sizeof DEFS / sizeof DEFS[0])); }
