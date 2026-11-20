/* bld_sd_rx_cynder.c: sd_r_x_cynder.py in C (see bld_sd_rx.h). */
#include "bld_sd_rx.h"

/* quench_trough */
static void quench_trough(double cx, double cy, double a, PyRandom *rng, SoObj *o, GG **foot) {
    F ff = FR(cx, cy, a); double L = u(4.6, 5.0), W = u(2.4, 2.7), h = u(2.5, 2.8);
    so_obj_init(o, "basalt");
    GG *g = RECT(cx, cy, L, W, a);
    for (int i = 0; i < 2; i++) {
        double sx = i ? 1 : -1;
        int bs = UP(o, RECTp(FF(&ff, sx * (L / 2 + 0.2), 0), 0.6, 1.1, a), h * 0.62, NULL);
        ON(o, RECTp(FF(&ff, sx * (L / 2 + 0.2), 0), 0.45, 0.8, a), bs, 0.12, NULL);
    }
    UP(o, RECT(cx, cy, L + 0.2, W + 0.2, a), 0.35, NULL);
    int b = UP(o, g, h, NULL); GG *inner = RRECT(cx, cy, L - 1.0, W - 1.0, a, 0.25);
    { double d = u(0.4, 0.55); CUT(o, inner, b, d); }
    GL tk = { 0 }; double xs[256]; int nx = AR(xs, 256, -L / 2 + 0.6, L / 2 - 0.4, 0.55);
    for (int i = 0; i < nx; i++) for (int j = 0; j < 2; j++) { double sy = j ? 1 : -1; GA(&tk, BARL(FF(&ff, xs[i], sy * (W / 2 - 0.5)), FF(&ff, xs[i] + 0.12, sy * (W / 2 + 0.1)), 0.24)); }
    nx = AR(xs, 256, -W / 2 + 0.6, W / 2 - 0.4, 0.55);
    for (int i = 0; i < nx; i++) for (int j = 0; j < 2; j++) { double sx = j ? 1 : -1; GA(&tk, BARL(FF(&ff, sx * (L / 2 - 0.5), xs[i]), FF(&ff, sx * (L / 2 + 0.1), xs[i] + 0.12), 0.24)); }
    for (int i = 0; i < 2; i++) for (int j = 0; j < 2; j++) {
        double sx = i ? 1 : -1, sy = j ? 1 : -1;
        if (!(rnd() < 0.4)) continue;
        P q[3]; q[0] = FF(&ff, sx * L / 2, sy * W / 2);
        double d1 = u(0.4, 0.7); q[1] = FF(&ff, sx * (L / 2 - d1), sy * W / 2);
        double d2 = u(0.4, 0.7); q[2] = FF(&ff, sx * L / 2, sy * (W / 2 - d2));
        GA(&tk, POLY(q, 3));
    }
    CUT(o, DIF(UL(&tk), BUF(inner, 0.08, 16)), b, 0.2);
    double sx = CH2(rng); S *s = SN();
    { double v = u(-0.5, 0.5) + 0.0; EXT(s, DISCp(FF(&ff, sx * (L / 2 + 0.25), v), 0.3, 10), 0.75, 0); }
    ST(o, s, "iron", -1, NULL, 0, DISCp(FF(&ff, sx * (L / 2 + 0.25), 0), 0.3, 20));
    GL sc = { 0 }; int n = (int)RI(rng, 1, 3);
    for (int i = 0; i < n; i++) { double x = u(-L * 0.3, L * 0.3), y = u(-W * 0.2, W * 0.2); P c = FF(&ff, x, y); double r = u(0.3, 0.5); GA(&sc, BLOB(c.x, c.y, r, rng, 3)); }
    s = SN(); EXT(s, INT(UL(&sc), INS(inner, 0.1)), h - 0.45 + 0.04, 0); ST(o, s, "ash", -1, NULL, 0, inner);
    double sv = CH2(rng); double x0 = u(-0.5, 0.5); double y0 = u(1.0, 1.3); P c = FF(&ff, x0, sv * (W / 2 + y0));
    double t = a + u(-0.2, 0.2); F f2 = FR(c.x, c.y, t); double Lb = u(3.4, 3.9);
    P bl[5] = { FF(&f2, -Lb / 2, -0.38), FF(&f2, Lb * 0.3, -0.38), FF(&f2, Lb / 2, 0), FF(&f2, Lb * 0.3, 0.38), FF(&f2, -Lb / 2, 0.38) };
    s = SN(); EXT(s, POLY(bl, 5), 0.32, 0); EXT(s, RECTp(FF(&f2, -Lb / 2 - 0.45, 0), 0.95, 0.5, t), 0.28, 0);
    { C cu[1] = { { BARL(FF(&f2, -Lb * 0.4, 0), FF(&f2, Lb * 0.2, 0), 0.24), 0.16 } }; ST(o, s, "iron", -1, cu, 1, RECT(c.x, c.y, Lb + 1.0, 0.8, t)); }
    if (rnd() < 0.6) {
        x0 = u(-0.6, 0.6); y0 = u(1.0, 1.2); c = FF(&ff, x0, -sv * (W / 2 + y0));
        double t1 = u(-0.3, 0.3); t = a + t1 + (rnd() < 0.5 ? PI : 0); f2 = FR(c.x, c.y, t); s = SN();
        for (int i = 0; i < 2; i++) {
            double s2 = i ? 1 : -1;
            P l[5] = { FF(&f2, -1.9, s2 * 0.42), FF(&f2, 0.4, -s2 * 0.05), FF(&f2, 0.9, -s2 * 0.62), FF(&f2, 1.4, -s2 * 0.7), FF(&f2, 1.7, -s2 * 0.45) };
            EXT(s, LINE(l, 5, 0.5), 0.36, 0);
        }
        ST(o, s, "iron", -1, NULL, 0, RECT(c.x, c.y, 3.8, 1.8, t));
        s = SN(); EXT(s, DISCp(FF(&f2, 0.4, 0), 0.42, 8), 0.56, 0); ST(o, s, "iron", -1, NULL, 0, DISCp(FF(&f2, 0.4, 0), 0.4, 20));
    }
    *foot = OUT(o);
}

static const XrDef DEFS[] = {
    { "cynder", "made", "basalt", "quench_trough", 10.5, 0, 1, quench_trough },
};
void bld_sdrx_cynder(void) { xr_register(DEFS, (int)(sizeof DEFS / sizeof DEFS[0])); }
