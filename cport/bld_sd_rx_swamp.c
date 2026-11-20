/* bld_sd_rx_swamp.c: sd_r_x_swamp.py in C (see bld_sd_rx.h). */
#include "bld_sd_rx.h"

static double sgn(int i) { return i ? 1 : -1; }

/* xs_groove(ff, x0, x1, v, rf, hk, n=6, w=0.17, dz=0.12, lift=0.0): appends the cuts */
static int xs_groove(C *out, const F *ff, double x0, double x1, double v, XrNum rf, void *ctx, double hk, int n, double w, double dz, double lift) {
    int k = 0;
    for (int i = 0; i < n; i++) {
        double a_ = x0 + (x1 - x0) * i / n, b_ = x0 + (x1 - x0) * (i + 1) / n, r = rf(ctx, (a_ + b_) / 2);
        if (fabs(v) < r * 0.85) { out[k].q = BARL(FF(ff, a_, v), FF(ff, b_, v), w); out[k].fl = hk * 0.93 * sqrt(MX(0.0, r * r - PW(fabs(v) + w / 2, 2))) - dz + lift; k++; }
    }
    return k;
}

/* dugout */
typedef struct { double W, L; } Dug;
static double dug_r(const Dug *d, double t) { return d->W / 2 * PW(MX(0.0, 1 - PW(fabs(2 * t / d->L), 2.4)), 0.55) + 0.04; }
static int dug_rf(void *c, double t, double *out) { *out = dug_r((Dug *)c, t); return SO_OK; }
static void dugout(double cx, double cy, double a, PyRandom *rng, SoObj *o, GG **foot) {
    F ff = FR(cx, cy, a); double L = u(7.0, 8.4), W = u(1.3, 1.5), hk = 1.2; so_obj_init(o, "log");
    Dug *dg = (Dug *)xr_mem(sizeof *dg); dg->W = W; dg->L = L;
    P hq[28]; { double ts[14]; LS(ts, -L * 0.4, L * 0.4, 14); for (int i = 0; i < 14; i++) hq[i] = FF(&ff, ts[i], dug_r(dg, ts[i]) - 0.3); LS(ts, L * 0.4, -L * 0.4, 14); for (int i = 0; i < 14; i++) hq[14 + i] = FF(&ff, ts[i], -dug_r(dg, ts[i]) + 0.3); }
    GG *hol = POLY(hq, 28);
    { int k = (int)RI(rng, 1, 2); int64_t ix[2]; double pop[3] = { -L * 0.2, L * 0.05, L * 0.22 }; xr_ok(py_sample_e(rng, 3, k, ix)); GL l = { 0 }; for (int i = 0; i < k; i++) { P q = FF(&ff, pop[ix[i]], 0); GA(&l, RECTp(q, 0.32, W, a)); } hol = DIF(hol, UL(&l)); }
    GG *adz; { GL l = { 0 }; double xs[64]; int nx = AR(xs, 64, -L * 0.36, L * 0.36, 0.42); for (int i = 0; i < nx; i++) GA(&l, BARL(FF(&ff, xs[i], -W), FF(&ff, xs[i] + 0.1, W), 0.16)); adz = INT(UL(&l), INS(hol, 0.12)); }
    { S *s = SN(); SPIN(s, cx, cy, a, L, dug_rf, dg, hk, 0.25, "round"); C cu[4] = { { hol, 0.55 }, { adz, 0.47 } }; for (int i = 0; i < 2; i++) { cu[2 + i].q = DISCp(FF(&ff, sgn(i) * (L / 2 - 0.45), 0), 0.12, 6); cu[2 + i].fl = 0.0; }
      ST(o, s, NULL, -1, cu, 4, RECT(cx, cy, L, W, a)); }
    for (int i = 0; i < 2; i++) { double sx = sgn(i); S *s = SN(); EXT(s, RECTp(FF(&ff, sx * (L / 2 - 0.55), 0), 0.5, 0.5, a), dug_r(dg, sx * (L / 2 - 0.55)) * hk * 0.93 + 0.37, 0); ST(o, s, NULL, -1, NULL, 0, DISCp(FF(&ff, sx * (L / 2 - 0.55), 0), 0.3, 20)); }
    double x = u(-1, 1); P pc = FF(&ff, x, W / 2 + 0.7); double pt = a + u(-0.2, 0.2); F pd = FR(pc.x, pc.y, pt);
    { S *s = SN(); P l[2] = { FF(&pd, -2.2, 0), FF(&pd, 0.8, 0) }; ROD(s, l, 2, 0.25, NULL, NULL, 1.1, 0, 0); EXT(s, RECTp(FF(&pd, -2.35, 0), 0.25, 0.8, a), 0.3, 0); ST(o, s, NULL, -1, NULL, 0, RECTp(FF(&pd, -0.7, 0), 3.2, 0.8, a)); }
    GG *bl = ELL(PXY(FF(&pd, 1.75, 0)), 1.0, 0.45, a, 16); int bd = UP(o, bl, 0.22, NULL); XON(o, RECTp(FF(&pd, 1.55, 0), 1.5, 0.22, a), bd, 0.1, NULL);
    double sx = CH2(rng); P p0 = FF(&ff, sx * (L / 2 - 0.45), 0); double d1 = u(1.4, 2.2), d2 = u(1.0, 1.4); P sk = FF(&ff, sx * (L / 2 - d1), -(W / 2 + d2));
    P w0 = POLp(p0, 0.3, ATAN2(sk.y - p0.y, sk.x - p0.x)), w1 = POLp(sk, 0.3, ATAN2(p0.y - sk.y, p0.x - sk.x)); P pts[8]; int np = lf_wiggle(w0.x, w0.y, w1.x, w1.y, rng, 0.3, 4, &pts[0].x);
    { S *s = SN(); ROD(s, pts, np, 0.25, NULL, NULL, 1.0, 0, 0); ST(o, s, "rope", -1, NULL, 0, BUF(LSTR(pts, np), 0.3, 16)); }
    { double pr[6] = { 0.3, 0.0, 0.28, 0, 0.12, 0 }; pr[3] = u(1.3, 1.6); pr[5] = u(1.7, 1.9); S *s = SN(); TURN(s, sk.x, sk.y, pr, 3, 10, 1.0, 0.0, 0.0); ST(o, s, NULL, -1, NULL, 0, DISCp(sk, 0.3, 20)); }
    if (rnd() < 0.4) {
        double x1 = u(-1, 1); P c = FF(&ff, x1, -(W / 2 + 0.55)); double t = a + u(-0.15, 0.15); F f3 = FR(c.x, c.y, t); double Ls = u(4.5, 5.5);
        S *s = SN(); P l[2] = { FF(&f3, -Ls / 2, 0), FF(&f3, Ls / 2 - 0.4, 0) }; ROD(s, l, 2, 0.25, NULL, NULL, 1.0, 0, 0);
        GL g = { 0 }; double vs[3] = { -0.3, 0.0, 0.3 }; for (int i = 0; i < 3; i++) GA(&g, TRI(FF(&f3, Ls / 2 - 0.5, vs[i]), FF(&f3, Ls / 2 + 0.4, vs[i] * 1.4), 0.3)); GA(&g, BARL(FF(&f3, Ls / 2 - 0.5, -0.35), FF(&f3, Ls / 2 - 0.5, 0.35), 0.3));
        EXT(s, UL(&g), 0.25, 0); ST(o, s, "iron", -1, NULL, 0, RECTp(FF(&f3, 0, 0), Ls, 0.8, a));
    }
    { double x1 = u(-L / 4, L / 4), y1 = u(-0.3, 0.3); P c = FF(&ff, x1, y1); GCUT(o, DIF(BLOB(c.x, c.y, W * 1.1, rng, 4), RECT(cx, cy, L, W + 0.4, a)), 0.18, "water"); }
    *foot = OUT(o);
}

/* pilings */
static void pilings(double cx, double cy, double a, PyRandom *rng, SoObj *o, GG **foot) {
    F ff = FR(cx, cy, a); int n = (int)RI(rng, 3, 4); double sp = u(1.55, 1.8); so_obj_init(o, "log"); P cc[4]; double cr[4], ch[4];
    for (int i = 0; i < n; i++) {
        double y = u(-0.35, 0.35); P c = FF(&ff, (i - (n - 1) / 2.0) * sp, y); double r = u(0.5, 0.62), h = u(1.5, 3.8); cc[i] = c; cr[i] = r; ch[i] = h; double t = u(0, 6.28);
        C cuts[6]; int nc = 0; double rrs[2] = { r * 0.42, r * 0.72 };
        for (int k = 0; k < 2; k++) { cuts[nc].q = ANN(c.x, c.y, rrs[k] + 0.08, rrs[k] - 0.08, 16); cuts[nc].fl = h - 0.07; nc++; }
        { int m = (int)RI(rng, 1, 2); for (int k = 0; k < m; k++) { cuts[nc].q = BARL(c, POLp(c, r, t + k * 2.2), 0.14); cuts[nc].fl = h - 0.1; nc++; } }
        if (rnd() < 0.55) {
            P q[3] = { POLp(c, r * 1.2, t + 1.0), POLp(c, r * 1.2, t + 2.6), POLp(c, 0.1, t + 1.8) }; cuts[nc].q = POLY(q, 3); cuts[nc].fl = h - u(0.3, 0.6); nc++;
            cuts[nc].q = DISCp(POLp(c, r * 0.15, t), r * 0.4, 8); cuts[nc].fl = h - u(0.3, 0.5); nc++;
        }
        S *s = SN(); double pr[8] = { r * 1.08, 0.0, r, 0.5, r * 0.97, h * 0.7, r * 1.0, h }; TURN(s, c.x, c.y, pr, 4, 12, 1.0, 0.0, 0.0);
        int m = (int)RI(rng, 3, 5); for (int k = 0; k < m; k++) { double d = u(0, 0.5); P q = POLp(c, r * 0.9, t + k * 1.4 + d); double dr = u(0.3, 0.42), dh = u(0.3, 0.5); DOME(s, q.x, q.y, dr, dh, 0, 1.0, 0.0, 10); }
        ST(o, s, NULL, -1, cuts, nc, DISCp(c, r + 0.3, 20));
    }
    int64_t i; xr_ok(py_randrange1_e(rng, n - 1, &i)); P c0 = cc[i], c1 = cc[i + 1]; P m = { (c0.x + c1.x) / 2, (c0.y + c1.y) / 2 }; double t = ATAN2(c1.y - c0.y, c1.x - c0.x) + u(-0.3, 0.3);
    double d = u(0.9, 1.2); double sv = CH2(rng); m = POLp(m, d, t + sv * 1.5708); F f2 = FR(m.x, m.y, t); double Lb = sp * u(1.5, 1.9);
    int bb = UP(o, RECTp(m, Lb, 0.7, t), 0.62, NULL); { GL l = { 0 }; double vs[2] = { -0.12, 0.16 }; for (int k = 0; k < 2; k++) GA(&l, BARL(FF(&f2, -Lb / 2, vs[k]), FF(&f2, Lb / 2, vs[k] + 0.03), 0.12)); CUT(o, UL(&l), bb, 0.08); }
    { double x = Lb * u(0.15, 0.35); x = x * CH2(rng); P bp = FF(&f2, x, 0); XON(o, DISCp(bp, 0.17, 8), bb, 0.45, "iron"); }
    CUT(o, DISCp(FF(&f2, -Lb * 0.3, 0), 0.14, 8), bb, 0.3);
    { int64_t j; xr_ok(py_randrange1_e(rng, n, &j)); P c = cc[j]; double r = cr[j]; double dd = u(1.6, 2.4); double s1 = CH2(rng); double ang = a + s1 * u(1.2, 1.9); P e = POLp(c, r + dd, ang);
      P w0 = POLp(c, r + 0.1, ATAN2(e.y - c.y, e.x - c.x)); P pts[8]; int np = lf_wiggle(w0.x, w0.y, e.x, e.y, rng, 0.35, 4, &pts[0].x);
      S *s = SN(); ROD(s, pts, np, 0.25, NULL, NULL, 1.0, 0, 0); ST(o, s, "rope", -1, NULL, 0, BUF(LSTR(pts, np), 0.3, 16)); }
    { GL l = { 0 }; for (int k = 0; k < n; k++) { double dr = u(0.6, 0.9); GA(&l, DISCp(cc[k], cr[k] + dr, 14)); } GCUT(o, BUF(BUF(UL(&l), 0.3, 16), -0.3, 16), 0.15, "water"); }
    (void)ch;
    *foot = OUT(o);
}

/* totem_stake */
typedef struct { double L, r, x0, hl; } Totem;
static double totem_r(const Totem *k, double t) {
    double L = k->L, r = k->r, s, m;
    if (t < -L / 2 + 1.1) return r * MX(0.25, (t + L / 2) / 1.1);
    if (t > L / 2 - 1.0) return r * 0.85;
    xr_ok(px_mod_d(t - k->x0, k->hl, &m)); s = m / k->hl;
    return r * (s < 0.1 ? 1.12 : 1.0 + 0.1 * py_exp(-PW((s - 0.32) / 0.07, 2)));
}
static int totem_rf(void *c, double t, double *out) { *out = totem_r((Totem *)c, t); return SO_OK; }
static double totem_hz(const Totem *k, double lf, double hk, double v, double t) { double rt = totem_r(k, t); return lf * MN(1.0, rt / k->r) + hk * 0.93 * sqrt(MX(0.0, PW(rt, 2) - v * v)); }
static void totem_stake(double cx, double cy, double a, PyRandom *rng, SoObj *o, GG **foot) {
    F ff = FR(cx, cy, a); double L = u(6.6, 7.6), r = u(0.62, 0.72), hk = 1.1, lf = r * 0.8; int nh = (int)RI(rng, 2, 3); double x0 = -L / 2 + 1.3, hl = (L - 2.4) / nh;
    Totem *tk = (Totem *)xr_mem(sizeof *tk); tk->L = L; tk->r = r; tk->x0 = x0; tk->hl = hl;
    S *ps = SN(); SPIN(ps, cx, cy, a, L, totem_rf, tk, hk, lf, "round"); C cuts[16]; int nc = 0;
    for (int k = 0; k < nh; k++) {
        double xe = x0 + k * hl + hl * 0.45, xm = x0 + k * hl + hl * 0.8;
        for (int i = 0; i < 2; i++) { cuts[nc].q = DISCp(FF(&ff, xe, sgn(i) * r * 0.38), 0.2, 10); cuts[nc].fl = totem_hz(tk, lf, hk, r * 0.38, xe) - 0.16; nc++; }
        cuts[nc].q = RECTp(FF(&ff, xm, 0), 0.2, r * 1.0, a); cuts[nc].fl = totem_hz(tk, lf, hk, r * 0.5, xm) - 0.12; nc++;
        EXT(ps, RECTp(FF(&ff, xe + hl * 0.17, 0), hl * 0.3, 0.36, a), totem_hz(tk, lf, hk, 0, xe) + 0.12, 0);
    }
    { double vs[2] = { -0.2, 0.15 }; for (int i = 0; i < 2; i++) { cuts[nc].q = BARL(FF(&ff, -L / 2 + 0.3, vs[i]), FF(&ff, -L / 2 + 1.0, vs[i] + 0.05), 0.16); cuts[nc].fl = totem_hz(tk, lf, hk, fabs(vs[i]), -L / 2 + 0.8) - 0.1; nc++; } }
    { P p = FF(&ff, L / 2 - 0.4, 0); double d = u(-0.15, 0.15); EXT(ps, TRI(p, FF(&ff, L / 2 + 0.6, d), 0.6), r * 1.0 + lf, 0); }
    for (int i = 0; i < 2; i++) { cuts[nc].q = DISCp(FF(&ff, L / 2 - 0.55, sgn(i) * r * 0.42), 0.16, 8); cuts[nc].fl = totem_hz(tk, lf, hk, r * 0.42, L / 2 - 0.55) - 0.14; nc++; }
    so_obj_init(o, "log"); ST(o, ps, NULL, -1, cuts, nc, RECT(cx, cy, L, 2 * r, a));
    double xc = x0 + (double)RI(rng, 0, nh - 1) * hl + 0.05; int m = (int)RI(rng, 2, 3);
    for (int k = 0; k < m; k++) {
        double sv = CH2(rng); P p = FF(&ff, xc + k * 0.32, sv * r * 0.95); double d1 = u(-0.6, 0.8), d2 = u(1.0, 1.6); P e = FF(&ff, xc + d1, sv * (r + d2));
        P pts[2] = { p, e }; { S *s = SN(); ROD(s, pts, 2, 0.25, NULL, NULL, 1.0, 0, 0); ST(o, s, "rope", -1, NULL, 0, BUF(LSTR(pts, 2), 0.3, 16)); }
        double t = ATAN2(e.y - p.y, e.x - p.x); P e2 = POLp(e, 0.7, t);
        S *s = SN(); P l[2] = { e, e2 }; ROD(s, l, 2, 0.25, NULL, NULL, 1.0, 0, 0); DOME(s, e.x, e.y, 0.25, 0.4, 0, 1.0, 0.0, 8); { P q = POLp(e, 0.7, t); DOME(s, q.x, q.y, 0.25, 0.4, 0, 1.0, 0.0, 8); }
        ST(o, s, "bone", -1, NULL, 0, DISCp(e, 0.9, 20));
    }
    *foot = OUT(o);
}

/* mossy_log */
typedef struct { double r, L; } Moss;
static double moss_r(void *c, double t) { Moss *m = (Moss *)c; return m->r * (1 - 0.12 * (t + m->L / 2) / m->L); }
static int moss_rf(void *c, double t, double *out) { *out = moss_r(c, t); return SO_OK; }
static void mossy_log(double cx, double cy, double a, PyRandom *rng, SoObj *o, GG **foot) {
    F ff = FR(cx, cy, a); double L = u(5.0, 5.8), r = u(0.8, 0.95), hk = 1.05, lf = r * 0.75; so_obj_init(o, "moss");
    Moss *mc = (Moss *)xr_mem(sizeof *mc); mc->r = r; mc->L = L;
    C cuts[40]; int nc = 0; double vs[4] = { -0.55 * r, -0.2 * r, 0.25 * r, 0.6 * r };
    for (int i = 0; i < 4; i++) { double d0 = u(0.2, 0.8), d1 = u(0.3, 1.2), dv = u(-0.05, 0.05); nc += xs_groove(cuts + nc, &ff, -L / 2 + d0, L / 2 - d1, vs[i] + dv, moss_r, mc, hk, 8, 0.17, 0.12, lf); }
    { double d = u(-0.1, 0.1); cuts[nc].q = DISCp(FF(&ff, L / 2 - 0.55, d), r * 0.5, 12); cuts[nc].fl = hk * r * 0.5 + lf; nc++; }
    S *ps = SN(); SPIN(ps, cx, cy, a, L, moss_rf, mc, hk, lf, "round"); double sv = CH2(rng), bx = u(-L * 0.25, L * 0.15);
    { P l[2]; l[0] = FF(&ff, bx, sv * r * 0.5); double d1 = u(0.5, 0.9), d2 = u(0.8, 1.1); l[1] = FF(&ff, bx + d1, sv * (r + d2)); ROD(ps, l, 2, 0.34, NULL, NULL, 1.1, lf * 0.6, 0); }
    { GL l = { 0 }; double vv[4]; LS(vv, -r * 0.6, r * 0.6, 4); for (int i = 0; i < 4; i++) { P p = FF(&ff, L / 2 - 0.1, vv[i]); double d1 = u(0.5, 0.9), d2 = u(-0.1, 0.1); GA(&l, TRI(p, FF(&ff, L / 2 + d1, vv[i] + d2), 0.34)); } EXT(ps, UL(&l), r * 0.9 + lf, 0); }
    int n = (int)RI(rng, 3, 5);
    for (int i = 0; i < n; i++) {
        double x = u(-L / 2 + 0.6, L / 2 - 1.0), y = u(-r * 0.4, r * 0.4); P c = FF(&ff, x, y); double q = u(0.45, 0.7), z = lf + hk * 0.93 * sqrt(MX(0.0, PW(moss_r(mc, 0), 2) - 0.1));
        double aa = u(0, 3); DOME(ps, c.x, c.y, q, z + q * 0.45, 0, 0.85, aa, 12);
        for (int k = 0; k < 3; k++) { double d = u(0, 1); P p = POLp(c, q * 0.55, k * 2.1 + d); DOME(ps, p.x, p.y, q * 0.4, z + q * 0.62, 0, 1.0, 0.0, 8); }
    }
    n = (int)RI(rng, 1, 3);
    for (int i = 0; i < n; i++) {
        double s1 = CH2(rng), x = u(-L / 2 + 0.6, L / 2 - 0.6); P c = FF(&ff, x, s1 * (r - 0.05)); double t = a + s1 * 1.5708;
        double dr = u(0.5, 0.65); GG *g = INT(DISCp(c, dr, 12), RECTp(POLp(c, 0.5, t), 1.0, 1.4, t)); double hh = lf + r * u(0.5, 0.8); EXT(ps, g, hh, 0);
    }
    ST(o, ps, NULL, -1, cuts, nc, RECTp(FF(&ff, 0.4, 0), L + 0.8, 2 * r, a));
    n = (int)RI(rng, 1, 3);
    for (int i = 0; i < n; i++) {
        double x = u(-L / 2, -L / 4), s1 = CH2(rng); double y = s1 * (r + u(1.0, 1.3)); P c = FF(&ff, x, y); double ro = u(0.6, 0.8); int np = (int)RI(rng, 5, 7); double rot = u(0, 1);
        S *s = SN(); EXT(s, STAR(c.x, c.y, ro, 0.28, np, rot), 0.3, 0); ST(o, s, "grass", -1, NULL, 0, DISCp(c, 0.6, 20));
    }
    *foot = OUT(o);
}

/* reed_bundle */
static double const_r(void *c, double q) { (void)q; return *(double *)c; }
static int tip_rf(void *c, double q, double *out) { (void)c; int e = 0; double p = PWs(2 * q / 0.9, 2, &e); if (e) return SO_ERR; *out = 0.25 * sqrt(MX(0.1, 1 - p)); return SO_OK; }
static void bundle(SoObj *o, PyRandom *rng, double x, double y, double t, double Lb, double r, double lift) {
    F f2 = FR(x, y, t); double hk = 1.0; double *rp = (double *)xr_mem(sizeof(double)); *rp = r; lift = lift + r * 0.7;
    C cuts[32]; int nc = 0; { double vs[5]; LS(vs, -r * 0.7, r * 0.7, 5); for (int i = 0; i < 5; i++) nc += xs_groove(cuts + nc, &f2, -Lb / 2 + 0.05, Lb / 2 - 0.05, vs[i], const_r, rp, hk, 4, 0.15, 0.1, 0.0); }
    for (int i = 0; i < nc; i++) cuts[i].fl = cuts[i].fl + lift;
    double xbs[2] = { -Lb * 0.22, Lb * 0.25 }; GL bd = { 0 }; for (int i = 0; i < 2; i++) GA(&bd, RECTp(FF(&f2, xbs[i], 0), 0.4, 2 * r + 0.3, t));
    { S *s = SN(); LIE(s, x, y, t, Lb, r * 0.87, lift); ST(o, s, NULL, -1, cuts, nc, RECT(x, y, Lb, 2 * r, t)); }
    { S *s = SN(); for (int i = 0; i < 2; i++) { P q = FF(&f2, xbs[i], 0); LIE(s, q.x, q.y, t, 0.4, r * 0.87 + 0.1, lift); }
      C cu[2]; for (int i = 0; i < 2; i++) { cu[i].q = BARL(FF(&f2, xbs[i] - 0.25, -r), FF(&f2, xbs[i] + 0.25, r), 0.15); cu[i].fl = lift + r * 0.9; } ST(o, s, "twig", -1, cu, 2, UL(&bd)); }
    S *tips = SN(); int n = (int)RI(rng, 5, 7);
    for (int k = 0; k < n; k++) {
        double v = (k - 3) * r * 0.3, d1 = u(0.8, 1.4), d2 = u(1.6, 2.4); P e = FF(&f2, Lb / 2 + d1, v * d2); P p = FF(&f2, Lb / 2 - 0.2, v * 0.8); double tt = ATAN2(e.y - p.y, e.x - p.x);
        P l[2] = { p, e }; ROD(tips, l, 2, 0.25, NULL, NULL, 1.0, lift * 0.8, 0); P q = POLp(e, 0.35, tt); SPIN(tips, q.x, q.y, tt, 0.9, tip_rf, NULL, 1.2, lift * 0.8, "round");
    }
    ST(o, tips, NULL, -1, NULL, 0, RECTp(FF(&f2, Lb / 2 + 0.9, 0), 2.0, 2 * r + 1.4, t));
}
static void reed_bundle(double cx, double cy, double a, PyRandom *rng, SoObj *o, GG **foot) {
    so_obj_init(o, "reed"); F ff = FR(cx, cy, a); double Lb = u(4.0, 4.6), r = u(0.55, 0.65); bundle(o, rng, cx, cy, a, Lb, r, 0.0);
    if (rnd() < 0.4) { double x = u(-0.6, 0.0), y = u(-0.1, 0.1); P c = FF(&ff, x, y); double t = a + u(-0.12, 0.12); bundle(o, rng, c.x, c.y, t, Lb * 0.75, r * 0.7, r * 1.25); }
    *foot = OUT(o);
}

/* fish_trap */
typedef struct { double L, r; } Trap;
static double trap_r(const Trap *k, double t) { double s = (t + k->L / 2) / k->L; return s > 0.0 ? k->r * (1 - 0.58 * (1 - s)) : k->r * 0.42; }
static int trap_rf(void *c, double t, double *out) { *out = trap_r((Trap *)c, t); return SO_OK; }
static int fish_rf(void *c, double q, double *out) { double Lf = *(double *)c; int e = 0; double p = PWs(2 * q / Lf, 2, &e); if (e) return SO_ERR; *out = 0.28 * sqrt(MX(0.15, 1 - p)) + 0.02; return SO_OK; }
static void fish_trap(double cx, double cy, double a, PyRandom *rng, SoObj *o, GG **foot) {
    F ff = FR(cx, cy, a); double L = u(3.0, 3.5), r = u(0.8, 0.95), hk = 1.1, lf = r * 0.6; so_obj_init(o, "twig");
    Trap *tp = (Trap *)xr_mem(sizeof *tp); tp->L = L; tp->r = r;
    C cuts[4]; { double vs[2] = { -0.3, 0.3 }; for (int i = 0; i < 2; i++) { cuts[i].q = BARL(FF(&ff, -L / 2 + 0.25, vs[i] * r), FF(&ff, L / 2 - 0.5, vs[i] * r * 1.5), 0.15); cuts[i].fl = lf + hk * 0.93 * r * 0.75; } }
    cuts[2].q = ELL(PXY(FF(&ff, L / 2 - 0.25, 0)), 0.28, r * 0.55, a, 14); cuts[2].fl = hk * r * 0.55 + lf; cuts[3].q = DISCp(FF(&ff, L / 2 - 0.3, 0), 0.16, 8); cuts[3].fl = lf * 0.5;
    { S *s = SN(); SPIN(s, cx, cy, a, L, trap_rf, tp, hk, lf, "round"); ST(o, s, NULL, -1, cuts, 4, RECT(cx, cy, L, 2 * r, a)); }
    { S *s = SN(); for (int k = 1; k < 6; k++) { P q = FF(&ff, -L / 2 + L * k / 6, 0); LIE(s, q.x, q.y, a, 0.3, trap_r(tp, -L / 2 + L * k / 6) * 1.02 + 0.06, lf * 0.95); }
      C cu[1] = { { ELL(PXY(FF(&ff, L / 2 - 0.25, 0)), 0.28, r * 0.55, a, 14), hk * r * 0.55 + lf } }; ST(o, s, NULL, -1, cu, 1, RECT(cx, cy, L, 2 * r, a)); }
    { P q = FF(&ff, -L / 2 - 0.1, 0); S *s = SN(); LIE(s, q.x, q.y, a, 0.4, r * 0.48, lf * 0.4); ST(o, s, "rope", -1, NULL, 0, DISCp(q, 0.45, 20)); }
    double sv = CH2(rng); double d1 = u(0.0, 0.5), d2 = u(0.5, 0.8); P e = FF(&ff, -L / 2 + d1, sv * (r + d2)); P pts[3] = { FF(&ff, -L / 2 - 0.3, 0), FF(&ff, -L / 2 - 0.5, sv * 0.4), e };
    { S *s = SN(); ROD(s, pts, 3, 0.25, NULL, NULL, 1.0, 0, 0); ST(o, s, "rope", -1, NULL, 0, BUF(LSTR(pts, 3), 0.3, 16)); }
    { double rr = u(0.32, 0.4), hh = u(0.3, 0.4); S *s = SN(); ROCK(s, e.x, e.y, rr, hh, rng, 0, 0.0, 9); ST(o, s, "rock", -1, NULL, 0, DISCp(e, 0.4, 20)); }
    int n = (int)RI(rng, 1, 2);
    for (int i = 0; i < n; i++) {
        double x = u(-0.6, 0.9), y = u(0.55, 0.7); P c = FF(&ff, x, -sv * (r + y + i * 0.7)); double t = a + u(-0.4, 0.4); t = t + PI * (double)RI(rng, 0, 1); F f2 = FR(c.x, c.y, t); double Lf = u(1.5, 1.9);
        double *lp = (double *)xr_mem(sizeof(double)); *lp = Lf;
        S *s = SN(); SPIN(s, c.x, c.y, t, Lf, fish_rf, lp, 1.2, 0, "round"); { P q[3] = { FF(&f2, -Lf / 2 + 0.1, 0), FF(&f2, -Lf / 2 - 0.45, -0.32), FF(&f2, -Lf / 2 - 0.45, 0.32) }; EXT(s, POLY(q, 3), 0.16, 0); }
        C cu[2] = { { DISCp(FF(&f2, Lf * 0.3, 0.06), 0.09, 6), 0.22 }, { BARL(FF(&f2, Lf * 0.18, -0.3), FF(&f2, Lf * 0.2, 0.3), 0.15), 0.26 } };
        ST(o, s, "hide", -1, cu, 2, RECTp(c, Lf + 0.5, 0.6, t));
    }
    *foot = OUT(o);
}

/* shed_skin */
static void shed_skin(double cx, double cy, double a, PyRandom *rng, SoObj *o, GG **foot) {
    F ff = FR(cx, cy, a); double L = u(6.0, 9.0); so_obj_init(o, "hide"); double ph = u(0, 6.28), A = u(0.7, 1.0);
    double xs[30]; P pts[30]; LS(xs, -L / 2, L / 2, 30); for (int i = 0; i < 30; i++) pts[i] = FF(&ff, xs[i], A * SIN(xs[i] * 1.3 + ph));
    GG *ln = LSTR(pts, 30); int m = 29; GL segs = { 0 };
    for (int i = 0; i < m; i++) { double q = (i + 0.5) / m, w = 0.35 + 0.5 * PW(SIN(MX(0.06, q) * PI * 0.92), 0.6); GA(&segs, CAPL(pts[i], pts[i + 1], w)); }
    GG *body = UL(&segs); double hh = u(0.24, 0.3); int b = UP(o, body, hh, NULL);
    { GL wr = { 0 }; double ll = LEN(ln), ss[64]; int ns = AR(ss, 64, 0.35, ll - 0.3, 0.32);
      for (int i = 0; i < ns; i++) { GG *p = xr_keep(shp_interpolate(ln, ss[i], 0)), *q = xr_keep(shp_interpolate(ln, MN(ll, ss[i] + 0.05), 0)); double t = ATAN2(shp_y(q) - shp_y(p), shp_x(q) - shp_x(p)) + 1.5708 + u(-0.25, 0.25);
          GA(&wr, BARL(POL(shp_x(p), shp_y(p), 0.6, t), POL(shp_x(p), shp_y(p), 0.6, t + PI), 0.12)); }
      CUT(o, INT(UL(&wr), body), b, 0.09); }
    int n = (int)RI(rng, 1, 2);
    for (int k = 0; k < n; k++) {
        double s = u(0.25, 0.7) * LEN(ln); GG *p = xr_keep(shp_interpolate(ln, s, 0)); double d = u(0.5, 0.8); GG *q = xr_keep(shp_interpolate(ln, s + d, 0));
        XON(o, INT(CAPL(PT(shp_x(p), shp_y(p)), PT(shp_x(q), shp_y(q)), 0.7), BUF(body, 0.1, 16)), b, 0.16, NULL);
    }
    P e = pts[29]; double t = ATAN2(pts[29].y - pts[28].y, pts[29].x - pts[28].x);
    CUT(o, TRI(POLp(e, 0.3, t), POLp(e, -0.6, t), 0.3), b, 0.2);
    { GL l = { 0 }; for (int i = 0; i < 2; i++) { P q = POLp(POLp(e, -0.45, t), 0.22, t + sgn(i) * 1.5708); GA(&l, DISCp(q, 0.15, 8)); } XON(o, UL(&l), b, 0.08, NULL); }
    GG *ex[1] = { BUF(ln, 0.8, 16) };
    *foot = OUTX(o, ex, 1);
}

static const XrDef DEFS[] = {
    { "swamp", "made", "log", "dugout", 10.5, 0, 0, dugout },
    { "swamp", "made", "log", "pilings", 10.5, 0, 0, pilings },
    { "swamp", "made", "log", "totem_stake", 10.5, 0, 0, totem_stake },
    { "swamp", "growth", "moss", "mossy_log", 10.5, 0, 0, mossy_log },
    { "swamp", "made", "reed", "reed_bundle", 10.5, 0, 0, reed_bundle },
    { "swamp", "made", "twig", "fish_trap", 10.5, 0, 0, fish_trap },
    { "swamp", "made", "hide", "shed_skin", 10.5, 0, 0, shed_skin },
};
void bld_sdrx_swamp(void) { xr_register(DEFS, (int)(sizeof DEFS / sizeof DEFS[0])); }
