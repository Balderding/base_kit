/* bld_sd_rx_temple.c: sd_r_x_temple.py in C (see bld_sd_rx.h). */
#include "bld_sd_rx.h"

static double sgn(int i) { return i ? 1 : -1; }

/* x_glyph(c, t, s, rng) */
static GG *x_glyph(P c, double t, double s, PyRandom *rng) {
    F f2 = FR(c.x, c.y, t); GG *g = RRECTp(c, s, s, t, s * 0.22); GL gs = { 0 }; GA(&gs, DIF(g, INS(g, 0.13)));
    int k = (int)RI(rng, 0, 3);
    if (k == 0) {
        for (int i = 0; i < 2; i++) GA(&gs, DISCp(FF(&f2, sgn(i) * s * 0.2, s * 0.12), s * 0.13, 8));
        GA(&gs, BARL(FF(&f2, -s * 0.22, -s * 0.2), FF(&f2, s * 0.22, -s * 0.2), 0.13));
    } else if (k == 1) {
        int nd = (int)RI(rng, 1, 3);
        for (int j = 0; j < nd; j++) GA(&gs, DISCp(FF(&f2, (j - (nd - 1) / 2.0) * s * 0.26, s * 0.18), s * 0.12, 8));
        GA(&gs, BARL(FF(&f2, -s * 0.3, -s * 0.12), FF(&f2, s * 0.3, -s * 0.12), 0.14));
    } else if (k == 2) {
        double sp[42]; xr_ok(sd_spiral(c.x, c.y, s * 0.32, s * 0.06, 1.6, t, 20, sp)); GA(&gs, LINE((const P *)sp, 21, 0.12));
    } else {
        GA(&gs, BARL(FF(&f2, -s * 0.3, -s * 0.3), FF(&f2, s * 0.3, s * 0.3), 0.12)); GA(&gs, BARL(FF(&f2, -s * 0.3, s * 0.3), FF(&f2, s * 0.3, -s * 0.3), 0.12));
        GA(&gs, ANN(c.x, c.y, s * 0.36, s * 0.27, 12));
    }
    return UL(&gs);
}

/* calendar_stone */
static void calendar_stone(double cx, double cy, double a, PyRandom *rng, SoObj *o, GG **foot) {
    double R = u(2.6, 2.8), h = u(1.7, 2.0); so_obj_init(o, "tcolumn");
    GG *g = DISC(cx, cy, R, 40);
    if (rnd() < 0.35) {
        double tb = u(0, 6.28); P q[3]; q[0] = POL(cx, cy, R * 0.55, tb); double d = u(0.3, 0.5); q[1] = POL(cx, cy, R * 1.3, tb - d); d = u(0.4, 0.6); q[2] = POL(cx, cy, R * 1.3, tb + d);
        g = DIF(g, POLY(q, 3));
    }
    int b = UP(o, g, h, NULL);
    { GL l = { 0 }; for (int k = 0; k < 40; k++) GA(&l, DISCp(POL(cx, cy, R - 0.28, a + 6.2832 * k / 40), 0.12, 6)); CUT(o, UL(&l), b, 0.1); }
    GROOVE(o, DISC(cx, cy, R - 0.55, 36), b, 0.14, 0.12);
    { GL l = { 0 }; for (int k = 0; k < 8; k++) GA(&l, TRI(POL(cx, cy, R * 0.55, a + 6.2832 * (k + 0.5) / 8), POL(cx, cy, R - 0.62, a + 6.2832 * (k + 0.5) / 8), 0.9)); ON(o, UL(&l), b, 0.25, NULL); }
    int rd = ON(o, ANN(cx, cy, R * 0.56, R * 0.38, 32), b, 0.3, NULL);
    { GL l = { 0 }; for (int k = 0; k < 20; k++) GA(&l, BARL(POL(cx, cy, R * 0.37, a + 6.2832 * k / 20), POL(cx, cy, R * 0.57, a + 6.2832 * k / 20), 0.1)); CUT(o, UL(&l), rd, 0.1); }
    { GL l = { 0 }; for (int k = 0; k < 20; k++) GA(&l, DISCp(POL(cx, cy, R * 0.47, a + 6.2832 * (k + 0.5) / 20), 0.14, 6)); CUT(o, UL(&l), rd, 0.08); }
    for (int k = 0; k < 4; k++) {
        double t = a + 0.785 + k * 1.5708; P c = POL(cx, cy, R * 0.25, t); int pn = ON(o, RRECTp(c, R * 0.17, R * 0.15, t, 0.06), b, 0.3, NULL);
        CUT(o, DISCp(c, 0.12, 6), pn, 0.08);
    }
    int fc = ON(o, DISC(cx, cy, R * 0.16, 20), b, 0.5, NULL); F f2 = FR(cx, cy, a);
    { GL l = { 0 }; for (int i = 0; i < 2; i++) GA(&l, DISCp(FF(&f2, R * 0.05, sgn(i) * R * 0.06), 0.13, 8)); CUT(o, UL(&l), fc, 0.1); }
    ON(o, TRI(FF(&f2, -R * 0.02, 0), FF(&f2, -R * 0.2, 0), 0.48), fc, 0.12, NULL);
    *foot = OUT(o);
}

/* glyph_lintel */
static void glyph_lintel(double cx, double cy, double a, PyRandom *rng, SoObj *o, GG **foot) {
    F ff = FR(cx, cy, a); double L = u(8.6, 9.0), W = u(1.8, 2.0), h = u(1.6, 1.85); so_obj_init(o, "tcolumn");
    double xb = u(-0.25, 0.25) * L, gap = u(0.3, 0.6);
    double vs[7]; P jp[9]; LS(vs, -W, W, 7); for (int i = 0; i < 7; i++) { double d = u(-0.25, 0.25); jp[i] = FF(&ff, xb + d, vs[i]); }
    jp[7] = FF(&ff, xb + L, W); jp[8] = FF(&ff, xb + L, -W);
    GG *jag = POLY(jp, 9), *whole = RECT(cx, cy, L, W, a);
    GG *pg[2]; double pdx[2], pdb[2];
    pg[0] = DIF(whole, jag); pdx[0] = 0.0; pdb[0] = 0.0;
    pg[1] = INT(whole, jag); pdx[1] = gap; pdb[1] = u(-0.12, 0.12);
    double s = W - 0.95;
    for (int i = 0; i < 2; i++) {
        GG *g = pg[i]; double dx = pdx[i], db = pdb[i];
        if (EMPTY(g)) continue;          /* the centroid of an empty piece is nan; it stays empty and is skipped */
        P c0 = CENT(g); g = ROT(TRANS(g, dx * COS(a), dx * SIN(a)), db, SHP_ORIGIN_POINT, c0.x, c0.y, 1);
        if (EMPTY(g)) continue;
        double t = a + db; P gc = CENT(g); F f2 = FR(gc.x, gc.y, t); double k = dx == 0 ? 1.0 : u(0.92, 1.0); int b = UP(o, g, h * k, NULL);
        GG *inner = INS(g, 0.4); GL gl = { 0 }; GA(&gl, DIF(INS(g, 0.2), INS(g, 0.32)));
        double xs[256]; int nx = AR(xs, 256, -L, L, s + 0.18);
        for (int j = 0; j < nx; j++) { P c = FF(&f2, xs[j], 0); if (CONTAINS(inner, RRECTp(c, s, s, t, 0.2))) GA(&gl, x_glyph(c, t, s, rng)); }
        CUT(o, UL(&gl), b, 0.12);
    }
    int n = (int)RI(rng, 2, 3);
    for (int i = 0; i < n; i++) {
        double d = u(-0.3, 0.3), sv = CH2(rng); sv = sv * u(W / 2 + 0.3, W / 2 + 0.8); P c = FF(&ff, xb + gap / 2 + d, sv); P q[5];
        for (int k = 0; k < 5; k++) { double r = u(0.28, 0.45), e = u(-0.2, 0.2); q[k] = POLp(c, r, k * 1.25 + e); }
        GG *g = POLY(q, 5); double hh = u(0.4, 0.6); UP(o, g, hh, NULL);
    }
    *foot = OUT(o);
}

/* lotus_basin */
static void lotus_basin(double cx, double cy, double a, PyRandom *rng, SoObj *o, GG **foot) {
    double R = u(2.0, 2.15); int n = (int)RI(rng, 8, 10); double t0 = u(0, 1); so_obj_init(o, "tcolumn");
    GG *g = NGON(cx, cy, R * 1.05, 16, t0); double hh = u(0.3, 0.36); int pl = UP(o, g, hh, NULL); GROOVE(o, NGON(cx, cy, R * 1.05 - 0.2, 16, t0), pl, 0.1, 0.08);
    GL p1 = { 0 }; for (int k = 0; k < n; k++) { double t = t0 + 6.2832 * k / n; P c = POL(cx, cy, R * 0.62, t); GA(&p1, ELL(c.x, c.y, R * 0.4, R * 0.28, t, 12)); } GA(&p1, DISC(cx, cy, R * 0.66, 24));
    GG *pet1 = UL(&p1); hh = u(1.35, 1.5); int b1 = UP(o, pet1, hh, NULL);
    { GL l = { 0 }; for (int k = 0; k < n; k++) { double t = t0 + 6.2832 * k / n; GA(&l, BARL(POL(cx, cy, R * 0.68, t), POL(cx, cy, R * 0.98, t), 0.12)); } CUT(o, UL(&l), b1, 0.1); }
    GL p2 = { 0 }; for (int k = 0; k < n; k++) { double t = t0 + 6.2832 * (k + 0.5) / n; P c = POL(cx, cy, R * 0.55, t); GA(&p2, ELL(c.x, c.y, R * 0.3, R * 0.22, t, 12)); } GA(&p2, DISC(cx, cy, R * 0.56, 24));
    int b2 = ON(o, UL(&p2), b1, 0.5, NULL);
    { GL l = { 0 }; for (int k = 0; k < n; k++) { double t = t0 + 6.2832 * (k + 0.5) / n; GA(&l, BARL(POL(cx, cy, R * 0.6, t), POL(cx, cy, R * 0.8, t), 0.12)); } CUT(o, UL(&l), b2, 0.1); }
    int rim = ON(o, DISC(cx, cy, R * 0.56, 28), b2, 0.25, NULL);
    { GG *d = DISC(cx, cy, R * 0.44, 24); double dd = u(1.0, 1.3); CUT(o, d, rim, dd); }
    *foot = OUT(o);
}

static const XrDef DEFS[] = {
    { "temple", "made", "tcolumn", "calendar_stone", 10.5, 0, 1, calendar_stone },
    { "temple", "made", "tcolumn", "glyph_lintel", 10.5, 0, 1, glyph_lintel },
    { "temple", "made", "tcolumn", "lotus_basin", 10.5, 0, 1, lotus_basin },
};
void bld_sdrx_temple(void) { xr_register(DEFS, (int)(sizeof DEFS / sizeof DEFS[0])); }
