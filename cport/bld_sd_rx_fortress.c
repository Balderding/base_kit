/* bld_sd_rx_fortress.c: sd_r_x_fortress.py in C (see bld_sd_rx.h). */
#include "bld_sd_rx.h"

/* torii_beam */
typedef struct { double h0, up, add; } ToriiZ;
static double torii_z(void *c, double s) { ToriiZ *t = (ToriiZ *)c; return t->h0 + t->add + t->up * PW(fabs(2 * s - 1), 2.5); }
static double torii_z0(void *c, double s) { ToriiZ *t = (ToriiZ *)c; return t->h0 + t->up * PW(fabs(2 * s - 1), 2.5); }
static void torii_beam(double cx, double cy, double a, PyRandom *rng, SoObj *o, GG **foot) {
    F ff = FR(cx, cy, a); so_obj_init(o, "lacquer");
    double L = u(8.6, 9.0), W = u(1.1, 1.25), h0 = u(1.0, 1.1), up = u(0.5, 0.8);
    ToriiZ z0 = { h0, up, 0 }, z1 = { h0, up, 0.22 };
    S *s = SN();
    { P l[2] = { FF(&ff, -L / 2, 0), FF(&ff, L / 2, 0) }; XWALL(s, l, 2, W, torii_z0, &z0); }
    { P l[2] = { FF(&ff, -L / 2 + 0.3, 0), FF(&ff, L / 2 - 0.3, 0) }; XWALL(s, l, 2, W - 0.5, torii_z, &z1); }
    ST(o, s, NULL, -1, NULL, 0, RECT(cx, cy, L, W, a));
    double hh; GG *g = RECTp(FF(&ff, 0, -W / 2 - 0.3), L * 0.86, 0.85, a); hh = u(0.75, 0.85); int sm = UP(o, g, hh, NULL);
    { GL l = { 0 }; double xs[2] = { -L * 0.3, L * 0.3 }; for (int i = 0; i < 2; i++) GA(&l, BARL(FF(&ff, xs[i], -W), FF(&ff, xs[i], -W / 2 - 0.85), 0.22)); CUT(o, UL(&l), sm, 0.1); }
    double v = u(2.3, 2.6); v = -W / 2 - v;
    double t = a + u(-0.08, 0.08); double x1 = u(-0.4, 0.4); P c0 = FF(&ff, x1, v); F f2 = FR(c0.x, c0.y, t); double Ln = L * u(0.72, 0.8);
    g = RECTp(FF(&f2, 0, 0), Ln, 0.9, t); hh = u(0.55, 0.65); int nk = UP(o, g, hh, NULL);
    for (int i = 0; i < 2; i++) { double sx = i ? 1 : -1; if (rnd() < 0.8) ON(o, RECTp(FF(&f2, sx * Ln * 0.3, 0), 0.35, 0.9, t), nk, 0.25, "wood"); }
    x1 = u(-1.2, 1.2); P pc = FF(&ff, x1, (v - W / 2 - 0.42) / 2 - 0.3);
    double tp = a + u(-0.3, 0.3); { static const double ch[2] = { 0.0, PI }; tp = tp + CH(rng, ch, 2); }
    F f3 = FR(pc.x, pc.y, tp);
    GG *pg = RECT(pc.x, pc.y, 2.0, 1.3, tp); int pb = UP(o, pg, 0.42, "wood"); ON(o, DIF(pg, INS(pg, 0.22)), pb, 0.16, "lacquer");
    GL st = { 0 }; int n = (int)RI(rng, 2, 3);
    for (int j = 0; j < n; j++) {
        double x = -0.55 + j * 0.55;
        GA(&st, BARL(FF(&f3, x - 0.12, -0.32), FF(&f3, x + 0.12, -0.32), 0.18)); GA(&st, BARL(FF(&f3, x, -0.38), FF(&f3, x, 0.3), 0.18)); GA(&st, BARL(FF(&f3, x - 0.15, 0.08), FF(&f3, x + 0.15, 0.12), 0.18));
    }
    CUT(o, INT(UL(&st), INS(pg, 0.3)), pb, 0.1);
    if (rnd() < 0.5) {
        double sx = CH2(rng); P c = FF(&ff, sx * L * 0.33, W / 2 + 0.9); double pr[4] = { 0.85, 0.0, 0.85, 0 }; pr[3] = u(0.9, 1.3);
        s = SN(); TURN(s, c.x, c.y, pr, 2, 18, 1.0, 0.0, 0.0);
        P tr[3] = { POLp(c, 1.0, a), POLp(c, 1.0, a + 1.6), POLp(c, 1.0, a + 3.2) };
        C cu[1] = { { POLY(tr, 3), 0.7 } }; ST(o, s, NULL, -1, cu, 1, DISCp(c, 0.85, 20));
    }
    *foot = OUT(o);
}

/* bamboo_fence */
typedef struct { double Lx, rh; } Nodes;
static double nodes_f(void *c, double s) { Nodes *n = (Nodes *)c; double m; xr_ok(px_mod_d(s * n->Lx / 0.9, 1.0, &m)); return m < 0.1 ? (n->rh + 0.05) : n->rh; }
static double post_r(void *c, double s_) { double rp = *(double *)c; return s_ < 0.92 ? rp : rp * (1 - (s_ - 0.92) / 0.1); }
static void bamboo_fence(double cx, double cy, double a, PyRandom *rng, SoObj *o, GG **foot) {
    F ff = FR(cx, cy, a); so_obj_init(o, "bamboo");
    double L = u(5.3, 5.5), W = u(3.4, 3.8), rh = 0.24, rv = 0.24;
    Nodes nL = { L, rh }, nW = { W, rh };
    int nv = (int)RI(rng, 2, 3); double vs[3]; LS(vs, -W * 0.36, W * 0.36, nv);
    S *ps = SN(); C cuts[2]; int nc = 0;
    for (int i = 0; i < nv; i++) { P p = FF(&ff, -L / 2, vs[i]); double d = u(-0.1, 0.1); TBAR(ps, p, FF(&ff, L / 2, vs[i] + d), 0, nodes_f, &nL, 0.4, 1, rh, rh); }
    int nx = (int)RI(rng, 4, 6); double xs[6]; LS(xs, -L / 2 + 1.1, L / 2 - 1.1, nx);
    int brk = -1; if (rnd() < 0.5) brk = (int)RI(rng, 0, nx - 1);
    for (int i = 0; i < nx; i++) {
        double x = xs[i];
        if (i == brk) { P p = FF(&ff, x, -W / 2 + 0.2); double d = u(0.3, 0.6); P q = POLp(p, W * 0.55, a + 1.5708 + d); TBARc(ps, p, q, rv, 1.0); continue; }
        { P p = FF(&ff, x, -W / 2); double d = u(-0.1, 0.1); TBAR(ps, p, FF(&ff, x + d, W / 2), 0, nodes_f, &nW, 0.4, 1, 2 * rh + rv * 0.6, 2 * rh + rv * 0.6); }
        for (int j = 0; j < nv; j++) { P c = FF(&ff, x, vs[j]); DOME(ps, c.x, c.y, 0.36, 2 * rh + rv * 1.6 + 0.2, 0.0, 0.8, a + 0.785, 8); }
    }
    static double rp = 0.42;
    for (int i = 0; i < 2; i++) {
        double sx = i ? 1 : -1, x = sx * (L / 2 - 0.25);
        TBAR(ps, FF(&ff, x, -W / 2 - 0.3), FF(&ff, x, W / 2 + 0.3), 0, post_r, &rp, 1.0, 0, 0, 0);
        cuts[nc].q = DISCp(FF(&ff, x, -W / 2 - 0.3), 0.22, 8); cuts[nc].fl = rp; nc++;
    }
    ST(o, ps, NULL, -1, cuts, nc, RECT(cx, cy, L + 0.4, W + 0.6, a));
    *foot = OUT(o);
}

/* rice_bales */
typedef struct { double L, r; } Bale;
static double bale_rf(void *c, double s) {
    Bale *b = (Bale *)c; double L = b->L, r = b->r, x = s * L, band;
    if (x < 0.25 || x > L - 0.25) return r * 0.92;
    if (x < 0.4 || x > L - 0.4) return r * 0.8;
    band = MN(MN(fabs(x - L * 0.25), fabs(x - L * 0.5)), fabs(x - L * 0.75));
    return r * (0.9 + 0.1 * SIN(PI * (x - 0.4) / (L - 0.8))) + (band < 0.12 ? 0.08 : 0.0);
}
static void bale(SoObj *o, Bale *bc, P c, double b, double z) {
    F f2 = FR(c.x, c.y, b); double L = bc->L, r = bc->r; S *s = SN(); GL l1 = { 0 }, l2 = { 0 };
    TBAR(s, FF(&f2, -L / 2, 0), FF(&f2, L / 2, 0), 0, bale_rf, bc, 0.4, 1, z + r * 0.95, z + r * 0.95);
    double vv[4] = { -r * 0.4, -r * 0.13, r * 0.13, r * 0.4 }, qq[3] = { 0.25, 0.5, 0.75 };
    for (int i = 0; i < 4; i++) GA(&l1, BARL(FF(&f2, -L / 2 + 0.45, vv[i]), FF(&f2, L / 2 - 0.45, vv[i]), 0.12));
    for (int i = 0; i < 3; i++) GA(&l2, RECTp(FF(&f2, L * qq[i] - L / 2, 0), 0.3, 2 * r, b));
    C cu[1] = { { DIF(UL(&l1), UL(&l2)), z + r * 1.85 } };
    ST(o, s, NULL, -1, cu, 1, RECT(c.x, c.y, L, 2 * r, b));
}
static void rice_bales(double cx, double cy, double a, PyRandom *rng, SoObj *o, GG **foot) {
    F ff = FR(cx, cy, a); double L = u(3.1, 3.4), r = u(0.88, 0.95); int n = (int)RI(rng, 2, 3); so_obj_init(o, "sack");
    Bale *bc = (Bale *)xr_mem(sizeof *bc); bc->L = L; bc->r = r;
    for (int i = 0; i < n; i++) { double x = u(-0.12, 0.12); P c = FF(&ff, x, (i - (n - 1) / 2.0) * (2 * r + 0.05)); double b = a + u(-0.05, 0.05); bale(o, bc, c, b, 0.0); }
    if (rnd() < 0.75) {
        double x = u(-0.15, 0.15), k = n == 2 ? 0 : CH2(rng); P c = FF(&ff, x, (r + 0.03) * k); double b = a + u(-0.06, 0.06); bale(o, bc, c, b, r * 1.5);
    }
    *foot = OUT(o);
}

static const XrDef DEFS[] = {
    { "fortress", "made", "lacquer", "torii_beam", 10.5, 0, 1, torii_beam },
    { "fortress", "made", "bamboo", "bamboo_fence", 10.5, 0, 1, bamboo_fence },
    { "fortress", "made", "sack", "rice_bales", 10.5, 0, 1, rice_bales },
};
void bld_sdrx_fortress(void) { xr_register(DEFS, (int)(sizeof DEFS / sizeof DEFS[0])); }
