/* bld_sd_rx_targa.c: sd_r_x_targa.py in C (see bld_sd_rx.h; xt_bar is XBAR with step 0.11). */
#include "bld_sd_rx.h"

static double sgn(int i) { return i ? 1 : -1; }

/* xt_drift(o, x, y, r, h, rng) */
static void xt_drift(SoObj *o, double x, double y, double r, double h, PyRandom *rng) {
    double sq = u(0.55, 0.8), t = u(0, 3.14); S *s = SN(); DOME(s, x, y, r, h, 0, sq, t, 20); ST(o, s, "snow", -1, NULL, 0, DISC(x, y, r * 0.8, 20));
}

/* pod_lid */
static GG *pod_cap(const F *ff, double l, double w, double dx) {
    GL h = { 0 }; GA(&h, DISCp(FF(ff, dx - (l - w) / 2, 0), w / 2, 16)); GA(&h, DISCp(FF(ff, dx + (l - w) / 2 + w * 0.12, 0), w * 0.38, 16)); return HULL(&h);
}
static void pod_lid(double cx, double cy, double a, PyRandom *rng, SoObj *o, GG **foot) {
    F ff = FR(cx, cy, a); double L = u(5.0, 6.2), W = u(2.6, 3.2); so_obj_init(o, "alien");
    GG *g = pod_cap(&ff, L, W, 0.0); double sv = CH2(rng); double xs[2] = { -L * 0.22, L * 0.14 };
    int kn; { GL l = { 0 }; for (int i = 0; i < 2; i++) GA(&l, RECTp(FF(&ff, xs[i], sv * (W / 2 - 0.05)), 0.9, 0.9, a)); kn = UP(o, UL(&l), 0.85, NULL); }
    { GL l = { 0 }; for (int i = 0; i < 2; i++) GA(&l, BARL(FF(&ff, xs[i] - 0.45, sv * (W / 2 - 0.05)), FF(&ff, xs[i] + 0.45, sv * (W / 2 - 0.05)), 0.12)); CUT(o, UL(&l), kn, 0.08); }
    int b = UP(o, g, 0.5, NULL);
    double hh = u(0.38, 0.45); int sh = ON(o, INS(g, 0.4), b, hh, NULL); hh = u(0.35, 0.42); int cr = ON(o, INS(g, 0.8), sh, hh, NULL);
    GG *wn = pod_cap(&ff, L * 0.46, W * 0.36, L * 0.1);
    CUT(o, DIF(wn, RECTp(FF(&ff, L * 0.1, 0), 0.45, W, a)), cr, 0.28); GROOVE(o, BUF(wn, 0.25, 16), cr, 0.16, 0.12);
    { P st[3]; double vs[3] = { -0.45, 0.0, 0.45 }; for (int i = 0; i < 3; i++) st[i] = FF(&ff, -L * 0.33, vs[i]); STUDS(o, st, 3, cr, 0.2, 0.14, NULL, 8); }
    { GL l = { 0 }; double x2[2] = { -L * 0.2, L * 0.33 }; for (int i = 0; i < 2; i++) GA(&l, BARL(FF(&ff, x2[i], -W), FF(&ff, x2[i], W), 0.14)); CUT(o, UL(&l), sh, 0.12); }
    ON(o, RECTp(FF(&ff, -L * 0.05, -sv * (W / 2 - 0.3)), 1.0, 0.5, a), b, 0.35, NULL);
    if (rnd() < 0.4) { double d = u(-L * 0.3, 0); P p = FF(&ff, d, -W); d = u(-L * 0.3, 0); P q = FF(&ff, d, W * 0.2); CUT(o, WAVY(p, q, rng, 0.3, 4, 0.22), cr, 0.3); }
    if (rnd() < 0.5) { double x = u(-L * 0.4, L * 0.4); P c = FF(&ff, x, -sv * (W / 2 + 0.6)); double r = u(1.2, 1.7), h = u(0.6, 0.85); xt_drift(o, c.x, c.y, r, h, rng); }
    *foot = OUT(o);
}

/* sled */
static void sled(double cx, double cy, double a, PyRandom *rng, SoObj *o, GG **foot) {
    F ff = FR(cx, cy, a); double L = u(6.0, 7.5), W = u(2.4, 2.9); so_obj_init(o, "wood");
    { GL l = { 0 }; for (int i = 0; i < 2; i++) { double sv = sgn(i); P q[4] = { FF(&ff, -L / 2, sv * W / 2), FF(&ff, L / 2 - 1.2, sv * W / 2), FF(&ff, L / 2 - 0.3, sv * (W / 2 - 0.45)), FF(&ff, L / 2, sv * (W / 2 - 0.9)) }; GA(&l, LINE(q, 4, 0.55)); }
      GA(&l, RECTp(FF(&ff, L / 2 - 0.05, 0), 0.5, W - 1.4, a)); S *s = SN(); EXT(s, UL(&l), 0.5, 0); ST(o, s, NULL, -1, NULL, 0, RECT(cx, cy, L, W + 0.6, a)); }
    { GL l = { 0 }; for (int i = 0; i < 2; i++) { double sv = sgn(i); GA(&l, RECTp(FF(&ff, L / 2 - 0.55, sv * (W / 2 - 0.35)), 0.6, 0.5, a + sv * 0.5)); } GA(&l, RECTp(FF(&ff, L / 2 - 0.05, 0), 0.5, W - 1.4, a)); UP(o, UL(&l), 0.95, NULL); }
    GG *dk = RECTp(FF(&ff, -0.6, 0), L - 2.2, W - 0.7, a); int b = UP(o, dk, 0.8, NULL);
    { double Ls = L - 2.2, Ws = W - 0.7, step = 0.62, vs[64]; int nv = AR(vs, 64, -Ws / 2 + step, Ws / 2 - step * 0.5, step); GL l = { 0 };          /* seams(lambda x, v: ff(x - 0.6, v), ...) */
      for (int i = 0; i < nv; i++) GA(&l, BARL(FF(&ff, -Ls - 0.6, vs[i]), FF(&ff, Ls - 0.6, vs[i]), 0.16)); CUT(o, UL(&l), b, 0.14); }
    { GL l = { 0 }; double xs[4]; LS(xs, -(L - 2.2) / 2 + 0.3, (L - 2.2) / 2 - 0.3, 4); for (int i = 0; i < 4; i++) for (int j = 0; j < 2; j++) GA(&l, RECTp(FF(&ff, -0.6 + xs[i], sgn(j) * (W / 2 - 0.3)), 0.4, 0.4, a)); UP(o, UL(&l), 1.45, NULL); }
    if (rnd() < 0.85) {
        double x = -(L - 2.2) / 2 - 0.6 + 0.25; int n = (int)RI(rng, 1, 2);
        for (int i = 0; i < n; i++) {
            double cl = u(1.5, 1.9), cw = W - 1.25, y = u(-0.1, 0.1); P c = FF(&ff, x + cl / 2, y); double t = a + u(-0.08, 0.08); F f2 = FR(c.x, c.y, t); x += cl + 0.15;
            GG *cg = RECTp(c, cl, cw, t); double ch = u(1.0, 1.3); int cs = ON(o, cg, b, ch, "wood"); int ld = LID(o, cg, cs, 0.12, 0.14, NULL); CORNERS(o, &f2, cl - 0.28, cw - 0.28, ld, t, 0.4, 0.1, "metal");
            ON(o, RECTp(c, 0.4, cw - 0.4, t), ld, 0.12, "rope");
        }
        if (x < (L - 2.2) / 2 - 0.6 - 1.0) {
            double lr = (L - 2.2) / 2 - 0.6 - x - 0.1; P c = FF(&ff, x + lr / 2, 0); GG *g = RRECTp(c, lr, W - 1.3, a, 0.4); double th = u(0.7, 0.9); int tr = ON(o, g, b, th, "cloth");
            GL l = { 0 }; double fs[2] = { 0.33, 0.66 }; for (int i = 0; i < 2; i++) GA(&l, BARL(FF(&ff, x + lr * fs[i], -W), FF(&ff, x + lr * fs[i], W), 0.14)); CUT(o, UL(&l), tr, 0.12);
        }
    } else { GL l = { 0 }; for (int i = 0; i < 2; i++) GA(&l, RECTp(FF(&ff, -0.6 + sgn(i) * (L - 2.2) * 0.36, 0), 0.5, W - 0.9, a)); ON(o, UL(&l), b, 0.14, NULL); }
    double d1 = u(1.5, 2.4), d2 = u(-1.2, 1.2); P e = FF(&ff, L / 2 + d1, d2); P pts[12]; pts[0] = FF(&ff, L / 2 + 0.3, -0.3); P w0 = FF(&ff, L / 2 + 0.6, 0);
    int np = 1 + lf_wiggle(w0.x, w0.y, e.x, e.y, rng, 0.5, 3, &pts[1].x); pts[np++] = FF(&ff, L / 2 + 0.9, 0.5); pts[np++] = FF(&ff, L / 2 + 0.3, 0.3);
    { S *s = SN(); ROD(s, pts, np, 0.25, NULL, NULL, 1.0, 0, 0); ST(o, s, "rope", -1, NULL, 0, BUF(LSTR(pts, np), 0.4, 16)); }
    *foot = OUT(o);
}

/* ice_blocks */
static GG *STONE(double x, double y, double r, PyRandom *rng, double aspect, int nv) { Geo *g = lf_stone(x, y, r, rng, aspect, nv, 0); GG *c; if (!g) xr_fail(); c = shp_clone(g->g); geo_unref(g); return xr_keep(c); }
static void ice_blocks(double cx, double cy, double a, PyRandom *rng, SoObj *o, GG **foot) {
    F ff = FR(cx, cy, a); int n = (int)RI(rng, 2, 4); so_obj_init(o, "ice"); double bl = u(1.7, 2.0), bw = u(1.3, 1.5); int tb[4]; P tc[4]; double tt[4];
    for (int i = 0; i < n; i++) {
        double x = (i - (n - 1) / 2.0) * (bl + 0.14) + u(-0.05, 0.05), y = u(-0.12, 0.12); P c = FF(&ff, x, y); double t = a + u(-0.06, 0.06); F f2 = FR(c.x, c.y, t);
        GG *g = RRECTp(c, bl, bw, t, 0.12); double hh = u(1.25, 1.4); int b = UP(o, g, hh, NULL);
        GL l = { 0 }; double vs[3] = { -bw * 0.2, bw * 0.05, bw * 0.24 }; for (int k = 0; k < 3; k++) GA(&l, BARL(FF(&f2, -bl, vs[k]), FF(&f2, bl, vs[k]), 0.14)); CUT(o, UL(&l), b, 0.1);
        tb[i] = b; tc[i] = c; tt[i] = t;
    }
    for (int i = 0; i < n; i++) if (rnd() < 0.45) { double t2 = tt[i] + u(-0.15, 0.15); GG *g = RRECTp(tc[i], bl - 0.4, bw - 0.4, t2, 0.12); double hh = u(1.1, 1.25); int k = ON(o, g, tb[i], hh, NULL); F f2 = FR(tc[i].x, tc[i].y, tt[i]); CUT(o, BARL(FF(&f2, -bl, 0), FF(&f2, bl, 0), 0.14), k, 0.1); }
    int m = (int)RI(rng, 2, 4);
    for (int i = 0; i < m; i++) { double x = u(-n, n) * 0.9, y = u(0.35, 0.7); P p = FF(&ff, x, bw / 2 + y); double r = u(0.3, 0.45); GG *g = STONE(p.x, p.y, r, rng, 0.7, 5); double hh = u(0.3, 0.45); UP(o, g, hh, NULL); }
    double x = u(-1, 1), y = u(1.2, 1.7); P c = FF(&ff, x, -bw / 2 - y); double t = a + u(-0.3, 0.3); F f2 = FR(c.x, c.y, t); double Ls = u(3.6, 4.4);
    P bq[17]; bq[0] = FF(&f2, -Ls / 2, 0.3); for (int k = 0; k < 15; k++) bq[1 + k] = FF(&f2, -Ls / 2 + Ls * k / 14, k % 2 ? -0.3 : -0.02); bq[16] = FF(&f2, Ls / 2, 0.3);
    { GG *blade = POLY(bq, 17); P q = FF(&f2, Ls / 2 + 0.55, 0.1); S *s = SN(); EXT(s, U2(BUF(blade, 0.12, 2), ANN(q.x, q.y, 0.7, 0.25, 10)), 0.3, 0); ST(o, s, "metal", -1, NULL, 0, RECTp(c, Ls + 2.0, 1.4, t)); }
    if (rnd() < 0.6) {
        double x1 = u(-0.8, 0.8), y1 = u(1.1, 1.3); P p = FF(&ff, x1, bw / 2 + y1); double t2 = a + u(-0.3, 0.3); F f3 = FR(p.x, p.y, t2);
        GL l = { 0 }; for (int i = 0; i < 2; i++) { double sv = sgn(i); P q[4] = { FF(&f3, -0.9, sv * 0.5), FF(&f3, 0.3, -sv * 0.15), FF(&f3, 0.8, -sv * 0.45), FF(&f3, 0.95, -sv * 0.2) }; GA(&l, LINE(q, 4, 0.3)); } GA(&l, DISCp(FF(&f3, 0.0, 0.0), 0.24, 8));
        S *s = SN(); EXT(s, UL(&l), 0.32, 0); ST(o, s, "metal", -1, NULL, 0, DISCp(p, 1.0, 20));
    }
    *foot = OUT(o);
}

/* buried_steps */
static void buried_steps(double cx, double cy, double a, PyRandom *rng, SoObj *o, GG **foot) {
    F ff = FR(cx, cy, a); double W = u(3.2, 4.0); int n = (int)RI(rng, 3, 4); double td = u(1.0, 1.2); so_obj_init(o, "alien"); double rise = u(0.55, 0.65);
    for (int i = 0; i < n; i++) {
        double x = (i - (n - 1) / 2.0) * td; int b = UP(o, RECTp(FF(&ff, x, 0), td + 0.02, W, a), rise * (i + 1), NULL); CUT(o, BARL(FF(&ff, x - td / 2 + 0.3, -W), FF(&ff, x - td / 2 + 0.3, W), 0.16), b, 0.12);
        { GL l = { 0 }; double vs[64]; int nv = AR(vs, 64, -W / 2 + 0.6, W / 2 - 0.4, 0.55); for (int k = 0; k < nv; k++) if (rnd() < 0.7) GA(&l, RECTp(FF(&ff, x + 0.05, vs[k]), 0.24, 0.24, a + 0.785)); CUT(o, UL(&l), b, 0.1); }
        if (rnd() < 0.3) { double sv = CH2(rng); P q[3]; q[0] = FF(&ff, x - td / 2, sv * W / 2); double d = u(0.4, 0.7); q[1] = FF(&ff, x - td / 2 + d, sv * W / 2); d = u(0.5, 0.9); q[2] = FF(&ff, x - td / 2, sv * (W / 2 - d)); CUT(o, POLY(q, 3), b, rise * 0.6); }
    }
    for (int i = 0; i < 2; i++) {
        double sv = sgn(i); int wl = UP(o, RECTp(FF(&ff, 0.15, sv * (W / 2 + 0.4)), n * td + 0.5, 0.9, a), rise * n + 0.45, NULL); int cp = ON(o, RECTp(FF(&ff, 0.15, sv * (W / 2 + 0.4)), n * td + 0.5, 0.62, a), wl, 0.14, NULL);
        CUT(o, BARL(FF(&ff, -n * td, sv * (W / 2 + 0.4)), FF(&ff, n * td, sv * (W / 2 + 0.4)), 0.14), cp, 0.1); ON(o, DISCp(FF(&ff, n * td / 2, sv * (W / 2 + 0.4)), 0.3, 10), cp, 0.25, NULL);
    }
    { double x = u(0.2, 0.7), y = u(-W * 0.3, W * 0.3); P c = FF(&ff, -n * td / 2 - x, y); double r = u(1.7, 2.3), h = u(1.0, 1.3); xt_drift(o, c.x, c.y, r, h, rng); }
    if (rnd() < 0.5) { double x = u(-0.5, 1.0), sv = CH2(rng); P c = FF(&ff, x, sv * (W / 2 + 1.2)); double r = u(1.2, 1.6), h = u(0.7, 1.0); xt_drift(o, c.x, c.y, r, h, rng); }
    *foot = OUT(o);
}

/* tusk: returns ln.buffer(r0 + 0.2), o.L */
static int tusk_rf(void *c, double q, double *out) { double r0 = *(double *)c; int e = 0; double p = PWs(q, 1.2, &e); if (e) return SO_ERR; *out = r0 * (1 - 0.7 * p); return SO_OK; }
static void tusk(double cx, double cy, double a, PyRandom *rng, SoObj *o, GG **foot) {
    double R = u(3.0, 4.5), span = u(1.3, 2.0); so_obj_init(o, "bone"); P oc = { cx - R * COS(a), cy - R * SIN(a) };
    P pts[15]; xr_ok(sd_arc(oc.x, oc.y, R, a - span / 2, a + span / 2, 14, &pts[0].x)); double r0 = u(0.55, 0.65), hk = 1.45, top = r0 * hk; double *rp = (double *)xr_mem(sizeof(double)); *rp = r0;
    GG *ln = LSTR(pts, 15); double fs[3] = { 0.06, 0.12, 0.18 }; P rings[3]; for (int i = 0; i < 3; i++) { GG *p = xr_keep(shp_interpolate(ln, fs[i], 1)); rings[i] = PT(shp_x(p), shp_y(p)); }
    C cuts[3]; int nc = 0; { GL l = { 0 }; for (int i = 0; i < 3; i++) { double t = a - span / 2 + fs[i] * span; GA(&l, BARL(POLp(rings[i], 1.0, t), POLp(rings[i], -1.0, t), 0.12)); } cuts[nc].q = UL(&l); cuts[nc].fl = top - 0.12; nc++; }
    cuts[nc].q = DISCp(pts[0], r0 * 0.55, 10); cuts[nc].fl = top * 0.45; nc++;
    if (rnd() < 0.6) { cuts[nc].q = BUF(LSTR(pts + 2, 7), 0.06, 16); cuts[nc].fl = top - 0.15; nc++; }
    { S *s = SN(); ROD(s, pts, 15, 0, tusk_rf, rp, hk, 0, 0); ST(o, s, NULL, -1, cuts, nc, BUF(ln, r0 + 0.2, 16)); }
    if (rnd() < 0.55) { P e = rnd() < 0.5 ? pts[14] : pts[0]; double t = a + u(-1, 1); P q = POLp(e, 0.3, t); double r = u(1.1, 1.4), h = u(0.55, 0.75); xt_drift(o, q.x, q.y, r, h, rng); }
    *foot = BUF(ln, r0 + 0.2, 16);
}

/* disc_wreck: returns disc(cx, cy, R + 0.2), o.L */
static void disc_wreck(double cx, double cy, double a, PyRandom *rng, SoObj *o, GG **foot) {
    double R = u(2.2, 3.0); so_obj_init(o, "alien"); double tb = a + u(-0.3, 0.3);
    P q[3]; q[0] = POL(cx, cy, R * 0.35, tb); { double d = u(0.3, 0.6); q[1] = POL(cx, cy, 2 * R, tb - d); d = u(0.3, 0.6); q[2] = POL(cx, cy, 2 * R, tb + d); } GG *bite = POLY(q, 3);
    int rim = UP(o, DIF(DISC(cx, cy, R, 28), bite), 0.38, NULL);
    { GL l = { 0 }; for (int k = 0; k < 8; k++) GA(&l, BARL(POL(cx, cy, R * 0.78, tb + 0.5 + k * 0.7), POL(cx, cy, R * 1.1, tb + 0.5 + k * 0.7), 0.12)); CUT(o, UL(&l), rim, 0.1); }
    int hl = ON(o, DIF(DISC(cx, cy, R * 0.74, 28), bite), rim, 0.36, NULL);
    { GL l = { 0 }; for (int k = 0; k < 9; k++) GA(&l, DISCp(POL(cx, cy, R * 0.6, tb + 0.8 + k * 0.62), 0.18, 8)); CUT(o, DIF(UL(&l), BUF(bite, 0.2, 16)), hl, 0.2); }
    { double pr[32] = { R * 0.45, 0.0, R * 0.45, 0.12 }; int np = 4; np += CRV(pr + np, R * 0.45, 0.3, 0.12, 0.62, 1, 5); S *s = SN(); TURN(s, cx, cy, pr, np / 2, 24, 1.0, 0.0, 0.0);
      C cu[2] = { { bite, 0.0 }, { ANN(cx, cy, R * 0.36, R * 0.28, 20), 0.3 } }; ST(o, s, "alien", hl, cu, 2, NULL); }
    ON(o, DISCp(POL(cx, cy, R * 0.16, tb + PI), 0.22, 8), hl, 0.95, NULL);
    for (int i = 0; i < 2; i++) {
        double sv = sgn(i), d = u(0.55, 0.85); double t = tb + sv * u(0.3, 0.45); P p = POL(cx, cy, R * d, t);
        P z[3] = { POLp(p, 0.35, tb + sv * 1.2), POLp(p, 0.35, tb + sv * 1.2 + 2.2), POLp(p, 0.35, tb + sv * 1.2 + 4.0) }; GG *g = BUF(POLY(z, 3), 0.05, 16); double hh = u(0.6, 0.85); UP(o, g, hh, NULL);
    }
    *foot = DISC(cx, cy, R + 0.2, 20);
}

/* supply_drop */
static void supply_drop(double cx, double cy, double a, PyRandom *rng, SoObj *o, GG **foot) {
    F ff = FR(cx, cy, a); double L = u(4.0, 4.8), W = u(2.3, 2.7), h = u(1.9, 2.2); so_obj_init(o, "metal"); GG *g = RRECT(cx, cy, L, W, a, 0.35);
    for (int i = 0; i < 2; i++) { double sx = sgn(i); P c = FF(&ff, sx * (L / 2 + 0.05), 0); P ar[9]; xr_ok(sd_arc(c.x, c.y, 0.7, sx > 0 ? a - 1.5708 : a + 1.5708, sx > 0 ? a + 1.5708 : a + 4.7124, 8, &ar[0].x)); S *s = SN(); EXT(s, LINE(ar, 9, 0.45), h * 0.75, 0); ST(o, s, NULL, -1, NULL, 0, DISCp(c, 0.9, 20)); }
    int b = UP(o, g, h, NULL); int lid = LID(o, g, b, 0.18, 0.16, NULL); RIBS(o, &ff, L - 1.0, W - 0.5, lid, 2, a, 0.45, 0.15, 1, NULL);
    GG *pn = RECT(cx, cy, L * 0.3, W * 0.5, a); CUT(o, pn, lid, 0.16); CUT(o, U2(BARL(FF(&ff, -L * 0.1, -W * 0.18), FF(&ff, L * 0.1, W * 0.18), 0.22), BARL(FF(&ff, -L * 0.1, W * 0.18), FF(&ff, L * 0.1, -W * 0.18), 0.22)), lid, 0.3);
    { GL l = { 0 }; double xs[64]; int nx = AR(xs, 64, -L * 0.3, L * 0.05, 0.3); for (int i = 0; i < nx; i++) GA(&l, RECTp(FF(&ff, xs[i], -W * 0.36), 0.14, 0.34, a)); CUT(o, UL(&l), lid, 0.1); }
    { double xs[2] = { -L * 0.3, L * 0.3 }; TABS(o, &ff, xs, 2, W / 2 - 0.45, lid, a, 0.5, 0.55, 0.18, NULL); }
    double sv = CH2(rng); { double x = u(-L * 0.3, L * 0.3); P c = FF(&ff, x, sv * (W / 2 + 0.7)); double r = u(1.8, 2.4), hh = u(1.1, 1.5); xt_drift(o, c.x, c.y, r, hh, rng); }
    if (rnd() < 0.5) {
        double s1 = CH2(rng); double y = sv * u(0.2, 0.9); P c = FF(&ff, s1 * (L / 2 + 0.9), y);
        if (rnd() < 0.5) { double r = u(1.1, 1.5), hh = u(0.6, 0.85); xt_drift(o, c.x, c.y, r, hh, rng); }
        else { double t = a + u(-0.4, 0.4); S *s = SN(); for (int k = 0; k < 3; k++) { P q = POLp(c, (k - 1) * 0.55, t + 1.5708); double l = u(1.6, 2.1), hh = u(0.45, 0.65); LOAF(s, q.x, q.y, t, l, 0.75, hh, 0.0); } ST(o, s, "cloth", -1, NULL, 0, DISCp(c, 1.0, 20)); }
    }
    *foot = OUT(o);
}

/* pack_and_snowshoes */
static double roll_rf(void *c, double s) { (void)c; return 0.6 + 0.07 * (double)(fabs(s - 0.25) < 0.06 || fabs(s - 0.75) < 0.06); }
static void pack_and_snowshoes(double cx, double cy, double a, PyRandom *rng, SoObj *o, GG **foot) {
    F ff = FR(cx, cy, a); double L = u(3.0, 3.6), W = u(2.1, 2.5); so_obj_init(o, "cloth"); GG *g = RRECT(cx, cy, L, W, a, 0.45);
    double hh = u(1.25, 1.45); int b = UP(o, g, hh, NULL); int fl = ON(o, RRECTp(FF(&ff, L * 0.2, 0), L * 0.55, W - 0.25, a, 0.4), b, 0.22, NULL); CUT(o, INT(BARL(FF(&ff, L * 0.2, -W), FF(&ff, L * 0.2, W), 0.12), INS(g, 0.5)), fl, 0.08);
    { GL l = { 0 }; for (int i = 0; i < 2; i++) GA(&l, RECTp(FF(&ff, L * 0.05, sgn(i) * W * 0.24), L * 0.8, 0.46, a)); ON(o, UL(&l), b, 0.32, "leather"); }
    { P st[2]; for (int i = 0; i < 2; i++) st[i] = FF(&ff, -L * 0.12, sgn(i) * W * 0.24); STUDS(o, st, 2, b, 0.28, 0.45, "metal", 8); }
    { double sv = CH2(rng); GG *sg = RRECTp(FF(&ff, -L * 0.1, sv * (W / 2 + 0.3)), 1.3, 1.0, a, 0.25); double h2 = u(0.85, 1.0); UP(o, sg, h2, NULL); }
    { S *s = SN(); XBAR(s, FF(&ff, L / 2 + 0.6, -W * 0.55), FF(&ff, L / 2 + 0.6, W * 0.55), 0, roll_rf, NULL, 0.7, 0.11); ST(o, s, "sack", -1, NULL, 0, RECTp(FF(&ff, L / 2 + 0.6, 0), 1.2, W * 1.1, a)); }
    int n = (int)RI(rng, 1, 2);
    for (int i = 0; i < n; i++) {
        P c;
        if (i == 0) { double x = u(-0.8, 0.8) - L * 0.1; double y = (i == 0 ? 1 : -1) * (W / 2 + u(1.9, 2.3)); c = FF(&ff, x, y); }
        else { double x = u(1.6, 2.0), y = u(-0.5, 0.5); c = FF(&ff, -L / 2 - x, y); }
        double t = a + u(-0.4, 0.4) + (i ? 1.5708 : 0.0); F f2 = FR(c.x, c.y, t);
        GL hl = { 0 }; GA(&hl, ELL(PXY(FF(&f2, 0.5, 0)), 1.5, 0.95, t, 20)); GA(&hl, DISCp(FF(&f2, -1.9, 0), 0.3, 8)); GG *sh = HULL(&hl);
        GL fr = { 0 }; GA(&fr, DIF(sh, INS(sh, 0.42))); GA(&fr, RECTp(FF(&f2, -2.2, 0), 1.0, 0.45, t)); { double xs[2] = { -0.1, 1.1 }; for (int k = 0; k < 2; k++) GA(&fr, INT(RECTp(FF(&f2, xs[k], 0), 0.42, 2.2, t), sh)); }
        GL la = { 0 }; for (int k = 0; k < 2; k++) { double sv = sgn(k); GA(&la, BARL(FF(&f2, -0.1, sv * 0.75), FF(&f2, 1.1, -sv * 0.75), 0.26)); } { double vs[2] = { -0.35, 0.35 }; for (int k = 0; k < 2; k++) GA(&la, BARL(FF(&f2, -0.1, vs[k]), FF(&f2, 1.1, vs[k]), 0.2)); }
        GG *lace = INT(UL(&la), sh);
        S *s = SN(); EXT(s, UL(&fr), 0.42, 0); EXT(s, lace, 0.3, 0); ST(o, s, "wood", -1, NULL, 0, sh);
    }
    if (rnd() < 0.5) {
        double x = u(-L * 0.3, L * 0.1), sv = CH2(rng); double y = -sv * (W / 2 + u(0.6, 0.9)); P p = FF(&ff, x, y); double t = a + u(-0.3, 0.3); F f3 = FR(p.x, p.y, t);
        P q[4] = { FF(&f3, 0.85, -0.9), FF(&f3, 1.15, -0.2), FF(&f3, 1.15, 0.5), FF(&f3, 0.85, 0.3) };
        S *s = SN(); EXT(s, U2(BARL(FF(&f3, -1.4, 0), FF(&f3, 1.0, 0), 0.36), POLY(q, 4)), 0.36, 0); ST(o, s, "metal", -1, NULL, 0, RECTp(p, 2.6, 1.0, t));
    }
    *foot = OUT(o);
}

/* skis */
static void skis(double cx, double cy, double a, PyRandom *rng, SoObj *o, GG **foot) {
    so_obj_init(o, "wood"); double L = u(8.0, 9.5);
    for (int i = 0; i < 2; i++) {
        double sv = sgn(i), d = u(0.55, 0.8); P c = POL(cx, cy, d, a + sv * 1.5708); double t = a + u(-0.08, 0.08); t = t + sv * u(0.0, 0.1); F f2 = FR(c.x, c.y, t);
        P q[5] = { FF(&f2, -L / 2, -0.42), FF(&f2, L / 2 - 1.2, -0.46), FF(&f2, L / 2, 0), FF(&f2, L / 2 - 1.2, 0.46), FF(&f2, -L / 2, 0.42) }; int b = UP(o, POLY(q, 5), 0.3, NULL);
        int pl = ON(o, RECTp(FF(&f2, -0.2, 0), 1.6, 0.62, t), b, 0.14, "metal");
        { GL l = { 0 }; for (int k = 0; k < 2; k++) GA(&l, RECTp(FF(&f2, 0.45, sgn(k) * 0.25), 0.42, 0.18, t)); GA(&l, RECTp(FF(&f2, 0.6, 0), 0.18, 0.68, t)); ON(o, UL(&l), pl, 0.22, "metal"); }
        { P l[5] = { FF(&f2, 0.45, 0.3), FF(&f2, -0.9, 0.28), FF(&f2, -0.95, 0), FF(&f2, -0.9, -0.28), FF(&f2, 0.45, -0.3) }; ON(o, LINE(l, 5, 0.15), pl, 0.08, "metal"); }
        { P z[3] = { FF(&f2, L / 2 - 1.3, -0.33), FF(&f2, L / 2 - 0.15, 0), FF(&f2, L / 2 - 1.3, 0.33) }; ON(o, POLY(z, 3), b, 0.18, NULL); }
        { P z[3] = { FF(&f2, L / 2 - 0.95, -0.2), FF(&f2, L / 2 - 0.3, 0), FF(&f2, L / 2 - 0.95, 0.2) }; ON(o, POLY(z, 3), b, 0.4, NULL); }
    }
    int n = (int)RI(rng, 1, 2);
    for (int i = 0; i < n; i++) {
        double d = u(1.9, 2.5); P c = POL(cx, cy, d, a + (i == 0 ? 1.5708 : -1.5708)); double t = a + u(-0.35, 0.35); F f2 = FR(c.x, c.y, t); double Lp = L * 0.62;
        GL l = { 0 }; GA(&l, BARL(FF(&f2, -Lp / 2, 0), FF(&f2, Lp / 2, 0), 0.4)); { P q = FF(&f2, Lp / 2 - 0.8, 0); GA(&l, ANN(q.x, q.y, 0.7, 0.22, 10)); }
        { P z[4]; for (int k = 0; k < 4; k++) z[k] = FF(&f2, Lp / 2 - 0.8 + 0.7 * COS(k * 1.5708 + 0.785) * 0.9, 0.7 * SIN(k * 1.5708 + 0.785) * 0.9); GA(&l, BUF(EXTR(POLY(z, 4)), 0.08, 16)); }
        S *s = SN(); EXT(s, UL(&l), 0.32, 0); EXT(s, U1(RECTp(FF(&f2, -Lp / 2 + 0.5, 0), 1.0, 0.55, t)), 0.45, 0); ST(o, s, NULL, -1, NULL, 0, RECTp(c, Lp, 1.4, t));
        { P q = FF(&f2, -Lp / 2 + 0.2, 0.55); S *s2 = SN(); EXT(s2, ANN(q.x, q.y, 0.45, 0.22, 10), 0.24, 0); ST(o, s2, "leather", -1, NULL, 0, DISCp(q, 0.45, 20)); }
    }
    *foot = OUT(o);
}

/* conduit_node */
static void conduit_node(double cx, double cy, double a, PyRandom *rng, SoObj *o, GG **foot) {
    double R = u(1.5, 1.9); int n = (int)RI(rng, 2, 4); GL gs = { 0 }; so_obj_init(o, "alien"); P ends[4], runs[4][3];
    for (int k = 0; k < n; k++) {
        double t = a + 6.2832 * k / n + u(-0.2, 0.2); P m = POL(cx, cy, R + 1.3, t); double d = u(1.2, 2.4); static const double ch[3] = { -0.5236, 0, 0.5236 }; double tt = t + CH(rng, ch, 3); P e = POLp(m, d, tt);
        runs[k][0] = POL(cx, cy, R + 0.2, t); runs[k][1] = m; runs[k][2] = e; GA(&gs, LINE(runs[k], 3, 0.75)); ends[k] = e;
    }
    GCUT(o, UL(&gs), 0.35, "slot");
    { S *s = SN(); for (int k = 0; k < n; k++) ROD(s, runs[k], 3, 0.28, NULL, NULL, 1.0, 0, 0); ST(o, s, "cable", -1, NULL, 0, UL(&gs)); }
    GL cl = { 0 };
    for (int k = 0; k < n; k++) {
        GG *ln = LSTR(runs[k], 3); double ll = LEN(ln), fs[64]; int nf = AR(fs, 64, 0.7, ll - 0.6, 1.1);
        for (int j = 0; j < nf; j++) { GG *q = xr_keep(shp_interpolate(ln, fs[j], 0)), *q2 = xr_keep(shp_interpolate(ln, MN(ll, fs[j] + 0.1), 0)); double tt = ATAN2(shp_y(q2) - shp_y(q), shp_x(q2) - shp_x(q)); GA(&cl, RECT(shp_x(q), shp_y(q), 0.3, 0.85, tt)); }
    }
    if (cl.n) { S *s = SN(); EXT(s, UL(&cl), 0.42, 0); ST(o, s, "metal", -1, NULL, 0, UL(&cl)); }
    GG *g = NGON(cx, cy, R, 6, a); double hh = u(0.65, 0.8); int b = UP(o, g, hh, NULL); ON(o, ANN(cx, cy, R * 0.72, R * 0.72 - 0.45, 24), b, 0.2, NULL);
    { P st[6]; for (int k = 0; k < 6; k++) st[k] = POL(cx, cy, R - 0.38, a + k * 1.0472); STUDS(o, st, 6, b, 0.22, 0.15, NULL, 8); }
    { GL l = { 0 }; for (int k = 0; k < 6; k++) GA(&l, BARL(POL(cx, cy, R * 0.78, a + 0.5236 + k * 1.0472 - 0.12), POL(cx, cy, R * 0.78, a + 0.5236 + k * 1.0472 + 0.12), 0.12)); CUT(o, UL(&l), b, 0.1); }
    { S *s = SN(); DOME(s, cx, cy, R * 0.38, R * 0.5, 0, 1.0, 0.0, 14); ST(o, s, NULL, b, NULL, 0, NULL); }
    for (int k = 0; k < n; k++) { P e = ends[k]; double t = ATAN2(e.y - cy, e.x - cx); P c = POLp(e, 0.8, t); int kk = UP(o, DISCp(c, 0.62, 12), 0.5, NULL); CUT(o, DISCp(c, 0.25, 8), kk, 0.18); }
    *foot = OUT(o);
}

static const XrDef DEFS[] = {
    { "targa", "made", "alien", "pod_lid", 10.5, 0, 0, pod_lid },
    { "targa", "made", "wood", "sled", 10.5, 0, 0, sled },
    { "targa", "made", "ice", "ice_blocks", 10.5, 0, 0, ice_blocks },
    { "targa", "made", "alien", "buried_steps", 10.5, 0, 0, buried_steps },
    { "targa", "made", "bone", "tusk", 10.5, 0, 0, tusk },
    { "targa", "made", "alien", "disc_wreck", 10.5, 0, 0, disc_wreck },
    { "targa", "made", "metal", "supply_drop", 10.5, 0, 0, supply_drop },
    { "targa", "made", "cloth", "pack_and_snowshoes", 10.5, 0, 0, pack_and_snowshoes },
    { "targa", "made", "wood", "skis", 10.5, 0, 0, skis },
    { "targa", "made", "alien", "conduit_node", 10.5, 1, 0, conduit_node },
};
void bld_sdrx_targa(void) { xr_register(DEFS, (int)(sizeof DEFS / sizeof DEFS[0])); }
