/* bld_sd_rx_graveyard.c: sd_r_x_graveyard.py in C (see bld_sd_rx.h). */
#include "bld_sd_rx.h"

static double sgn(int i) { return i ? 1 : -1; }

/* _GLYPHS: (p0, q0, p1, q1) strokes */
typedef struct { char ch; int n; double s[4][4]; } GGlyph;
static const GGlyph GG_[] = {
    { 'H', 3, { { 0, 0, 0, 1 }, { 1, 0, 1, 1 }, { 0, .5, 1, .5 } } }, { 'I', 1, { { .5, 0, .5, 1 } } }, { 'L', 2, { { 0, 0, 0, 1 }, { 0, 0, 1, 0 } } },
    { 'T', 2, { { 0, 1, 1, 1 }, { .5, 0, .5, 1 } } }, { 'E', 4, { { 0, 0, 0, 1 }, { 0, 1, 1, 1 }, { 0, .5, .8, .5 }, { 0, 0, 1, 0 } } },
    { 'A', 3, { { 0, 0, .5, 1 }, { .5, 1, 1, 0 }, { .25, .45, .75, .45 } } }, { 'M', 4, { { 0, 0, 0, 1 }, { 0, 1, .5, .45 }, { .5, .45, 1, 1 }, { 1, 1, 1, 0 } } },
    { 'N', 3, { { 0, 0, 0, 1 }, { 0, 1, 1, 0 }, { 1, 0, 1, 1 } } }, { 'V', 2, { { 0, 1, .5, 0 }, { .5, 0, 1, 1 } } }, { '1', 2, { { .5, 0, .5, 1 }, { .2, .8, .5, 1 } } },
    { '7', 2, { { 0, 1, 1, 1 }, { 1, 1, .35, 0 } } }, { 'F', 3, { { 0, 0, 0, 1 }, { 0, 1, 1, 1 }, { 0, .5, .75, .5 } } }, { 'K', 3, { { 0, 0, 0, 1 }, { 0, .5, 1, 1 }, { .3, .65, 1, 0 } } },
};
/* xg_text(f2, x0, v0, n, size, w, rng, step=None, chars="HILTEAMNVFK17"); step 0 = None */
static GG *xg_text(const F *f2, double x0, double v0, int n, double size, double w, PyRandom *rng, double step, const char *chars) {
    GL gs = { 0 }; double cw = size * 0.68; int nc = (int)strlen(chars);
    if (step == 0) step = size * 0.95;
    for (int i = 0; i < n; i++) {
        char ch = chars[CHI(rng, nc)]; double x = x0 + i * step; const GGlyph *g = NULL;
        for (size_t j = 0; j < sizeof GG_ / sizeof GG_[0]; j++) if (GG_[j].ch == ch) g = &GG_[j];
        if (!g) xr_fail();
        for (int k = 0; k < g->n; k++) {
            double p0 = g->s[k][0], q0 = g->s[k][1], p1 = g->s[k][2], q1 = g->s[k][3];
            GA(&gs, BARL(FF(f2, x + p0 * cw, v0 + (q0 - 0.5) * size), FF(f2, x + p1 * cw + 1e-3, v0 + (q1 - 0.5) * size + 1e-3), w));
        }
    }
    return UL(&gs);
}
static const char *CHARS = "HILTEAMNVFK17";
static GG *xg_coffin(const F *f, double L, double W) {
    P q[6] = { FF(f, -L / 2, -W * 0.3), FF(f, L * 0.22, -W / 2), FF(f, L / 2, -W * 0.32), FF(f, L / 2, W * 0.32), FF(f, L * 0.22, W / 2), FF(f, -L / 2, W * 0.3) };
    return POLY(q, 6);
}
/* xg_grain(f2, L, W, rng, n=2, w=0.12) */
static GG *xg_grain(const F *f2, double L, double W, PyRandom *rng, int n, double w) {
    GL gs = { 0 };
    for (int k = 0; k < n; k++) {
        double v = (k + 0.5 - n / 2.0) * W / (n + 0.4) + u(-0.05, 0.05), x0 = u(-L / 2, -L * 0.1), x1 = u(L * 0.1, L / 2);
        P p = FF(f2, x0, v); double d = u(-0.06, 0.06); GA(&gs, BARL(p, FF(f2, x1, v + d), w));
    }
    return UL(&gs);
}

/* fallen_railing */
static void fallen_railing(double cx, double cy, double a, PyRandom *rng, SoObj *o, GG **foot) {
    F ff = FR(cx, cy, a); double L = u(5.3, 6.3), W = u(3.0, 3.6); so_obj_init(o, "fence"); double sp = u(0.62, 0.72);
    double cl = L * u(0.55, 0.85); double cx0 = u(-(L - cl) / 2, (L - cl) / 2), vb = -W / 2 - 0.05;
    P q[5]; q[0] = FF(&ff, cx0 - cl / 2, vb - 0.45); q[1] = FF(&ff, cx0 + cl / 2, vb - 0.45); { double d = u(-0.2, 0.1); q[2] = FF(&ff, cx0 + cl / 2 + d, vb + 0.1); d = u(0.1, 0.35); q[3] = FF(&ff, cx0 + cl / 2 - d, vb + 0.45); } q[4] = FF(&ff, cx0 - cl / 2, vb + 0.45);
    GG *cg = POLY(q, 5); double hh = u(0.8, 0.95); int cp = UP(o, cg, hh, "headstone"); int ct = XON(o, INS(cg, 0.16), cp, 0.12, "headstone");
    double xs[64]; int nx = AR(xs, 64, -L / 2 + 0.35, L / 2 - 0.2, sp); int gone[64];
    for (int i = 0; i < nx; i++) gone[i] = rnd() < 0.1;
    int64_t bent; xr_ok(py_randrange1_e(rng, nx, &bent));
    GL bars = { 0 }, heads = { 0 }, dogs = { 0 }; P plugs[64]; int npl = 0; double top = W / 2 - 0.45;
    for (int i = 0; i < nx; i++) {
        if (gone[i]) continue;
        double x = xs[i], b = i == bent ? u(-0.5, 0.5) : 0.0;
        { P l[3] = { FF(&ff, x, vb + 0.25), FF(&ff, x, 0.0), FF(&ff, x + b, top) }; GA(&bars, LINE(l, 3, 0.3)); }
        if (fabs(x - cx0) < cl / 2 - 0.2) plugs[npl++] = FF(&ff, x, vb + 0.05);
        if (rnd() < 0.88) { P l[4] = { FF(&ff, x + b - 0.3, top + 0.12), FF(&ff, x + b, top - 0.1), FF(&ff, x + b + 0.3, top + 0.12), FF(&ff, x + b, top + 0.3) }; GA(&heads, U2(TRI(FF(&ff, x + b, top - 0.05), FF(&ff, x + b, top + 0.62), 0.62), POLY(l, 4))); }
        if (i < nx - 1) GA(&dogs, U2(BARL(FF(&ff, x + sp / 2, vb + 0.3), FF(&ff, x + sp / 2, -W * 0.28), 0.26), TRI(FF(&ff, x + sp / 2, -W * 0.36), FF(&ff, x + sp / 2, -W * 0.16), 0.42)));
    }
    UP(o, UL(&dogs), 0.24, NULL); UP(o, UL(&bars), 0.3, NULL); UP(o, UL(&heads), 0.34, NULL);
    double rv[2] = { -W * 0.3, W * 0.2 }; int has_br = 0; double br = 0;
    if (rnd() < 0.4) { br = u(L * 0.25, L * 0.45); br = br * CH2(rng); has_br = 1; }
    GL rails = { 0 };
    for (int k = 0; k < 2; k++) { double v = rv[k]; if (!has_br || k == 0) GA(&rails, BARL(FF(&ff, -L / 2 + 0.15, v), FF(&ff, L / 2 - 0.05, v), 0.42)); else GA(&rails, BARL(FF(&ff, br < 0 ? -L / 2 + 0.15 : br, v), FF(&ff, br > 0 ? L / 2 - 0.05 : br, v), 0.42)); }
    int rl = UP(o, UL(&rails), 0.44, NULL);
    { P st[128]; int n = 0; for (int i = 0; i < nx; i++) if (!gone[i]) for (int k = 0; k < 2; k++) st[n++] = FF(&ff, xs[i], rv[k]); XSTUDS(o, st, n, rl, 0.17, 0.09, NULL, 6); }
    if (npl) { GL l = { 0 }; for (int i = 0; i < npl; i++) GA(&l, RECTp(plugs[i], 0.5, 0.5, a)); XON(o, UL(&l), ct, 0.07, "tin"); }
    { GL l = { 0 }; double fs[2] = { -0.2, 0.25 }; for (int i = 0; i < 2; i++) { if (!(rnd() < 0.7)) continue; P p = FF(&ff, cx0 + cl * fs[i], vb - 0.6); double d = u(-0.15, 0.15); GA(&l, BARL(p, FF(&ff, cx0 + cl * fs[i] + d, vb + 0.6), 0.15)); } CUT(o, UL(&l), ct, 0.16); }
    *foot = OUT(o);
}

/* open_coffin */
static void open_coffin(double cx, double cy, double a, PyRandom *rng, SoObj *o, GG **foot) {
    F ff = FR(cx, cy, a); double L = u(5.4, 6.4), W = u(2.0, 2.3), h = u(1.3, 1.5); so_obj_init(o, "coffin");
    GG *g = xg_coffin(&ff, L, W); int b = UP(o, DIF(g, INS(g, 0.3)), h, NULL); UP(o, BUF(INS(g, 0.3), 0.02, 16), 0.32, NULL);
    { GL l = { 0 }; double xs[2] = { -L * 0.1, L * 0.3 }; for (int i = 0; i < 2; i++) GA(&l, BARL(FF(&ff, xs[i], -W), FF(&ff, xs[i] + 0.01, W), 0.12)); CUT(o, UL(&l), b, 0.08); }
    { GL l = { 0 }; double xs[3] = { -L * 0.25, L * 0.12, L * 0.34 }; for (int i = 0; i < 3; i++) for (int j = 0; j < 2; j++) GA(&l, RECTp(FF(&ff, xs[i], sgn(j) * (xs[i] > L * 0.05 ? W / 2 - 0.02 : W * 0.33)), 0.55, 0.5, a)); UP(o, UL(&l), h * 0.62, "brass"); }
    double fl = 0.32;
    if (rnd() < 0.65) {
        double hx = L / 2 - 0.62, y = u(-0.08, 0.08); P q = FF(&ff, hx, y); S *s = SN(); DOME(s, q.x, q.y, 0.42, fl + 0.48, 0, 0.82, a, 20);
        C cu[2]; for (int i = 0; i < 2; i++) { cu[i].q = DISCp(FF(&ff, hx - 0.12, sgn(i) * 0.17), 0.12, 6); cu[i].fl = fl + 0.3; } ST(o, s, "bone", -1, cu, 2, NULL);
        double sx = L / 2 - 1.15; GL bones = { 0 }; GA(&bones, BARL(FF(&ff, sx, 0), FF(&ff, -L * 0.12, 0), 0.24));
        int nk = (int)RI(rng, 4, 5);
        for (int k = 0; k < nk; k++) { double x = sx - 0.12 - k * 0.3; P l1[3] = { FF(&ff, x, 0.08), FF(&ff, x - 0.08, 0.42), FF(&ff, x - 0.3, 0.58) }, l2[3] = { FF(&ff, x, -0.08), FF(&ff, x - 0.08, -0.42), FF(&ff, x - 0.3, -0.58) }; GA(&bones, LINE(l1, 3, 0.18)); GA(&bones, LINE(l2, 3, 0.18)); }
        double pv = -L * 0.14; for (int i = 0; i < 2; i++) GA(&bones, ELL(PXY(FF(&ff, pv, sgn(i) * 0.3)), 0.3, 0.24, a, 10));
        for (int i = 0; i < 2; i++) {
            double sv = sgn(i), d = u(0.18, 0.3); P k0 = FF(&ff, pv - 1.2, sv * d); GA(&bones, CAPL(FF(&ff, pv - 0.15, sv * 0.3), k0, 0.24)); d = u(0.12, 0.3); GA(&bones, CAPL(k0, FF(&ff, -L / 2 + 0.45, sv * d), 0.22));
            P e = FF(&ff, sx - 0.8, sv * 0.78); GA(&bones, CAPL(FF(&ff, sx + 0.1, sv * 0.65), e, 0.2)); double d1 = u(-0.2, 0.3), d2 = u(0.35, 0.7); GA(&bones, CAPL(e, FF(&ff, pv + d1, sv * d2), 0.2));
        }
        s = SN(); EXT(s, INT(UL(&bones), INS(g, 0.32)), fl + 0.24, 0); ST(o, s, "bone", -1, NULL, 0, NULL);
    } else {
        P c = FF(&ff, -0.1, 0); S *s = SN(); XSACK(s, c.x, c.y, a, L * 0.78, W * 0.62, 0.55, fl);
        C cu[3]; double xs[3] = { L * 0.24, -L * 0.06, -L * 0.32 }; for (int i = 0; i < 3; i++) { cu[i].q = RECTp(FF(&ff, xs[i], 0), 0.18, W, a); cu[i].fl = fl + 0.42; } ST(o, s, "cloth", -1, cu, 3, NULL);
    }
    double lt = a + u(-0.25, 0.25); double x = u(-0.6, 0.6), y = u(0.25, 0.6); P lc = FF(&ff, x, W + y); F f2 = FR(lc.x, lc.y, lt); GG *lg = xg_coffin(&f2, L, W);
    int ld = UP(o, lg, 0.4, NULL); int pn = XON(o, INS(lg, 0.32), ld, 0.12, NULL);
    XON(o, U2(BARL(FF(&f2, -L * 0.28, 0), FF(&f2, L * 0.3, 0), 0.36), BARL(FF(&f2, L * 0.14, -W * 0.27), FF(&f2, L * 0.14, W * 0.27), 0.36)), pn, 0.13, "brass");
    int pl = XON(o, RECTp(FF(&f2, -L * 0.12, 0), 0.75, 0.6, lt), pn, 0.08, "brass"); CUT(o, xg_text(&f2, -L * 0.12 - 0.24, 0.0, 2, 0.34, 0.1, rng, 0, CHARS), pl, 0.07);
    { P st[64]; int n = 0; double xs[64]; int nx = AR(xs, 64, -L / 2 + 0.4, L / 2 - 0.2, 0.8);
      for (int i = 0; i < nx; i++) for (int j = 0; j < 2; j++) st[n++] = FF(&f2, xs[i], sgn(j) * (W * 0.3 + (W * 0.2 - fabs(xs[i] - L * 0.22) / L * 0.0) - 0.2));
      XSTUDS(o, st, n, ld, 0.13, 0.08, "iron", 6); }
    if (rnd() < 0.35) { P l[3]; double d = u(-0.3, 0.3); l[0] = FF(&f2, -L * 0.4, d); d = u(-0.2, 0.2); l[1] = FF(&f2, -L * 0.1, d); d = u(-0.3, 0.3); l[2] = FF(&f2, L * 0.3, d); CUT(o, LINE(l, 3, 0.16), ld, 0.3); }
    *foot = OUT(o);
}

/* sunken_grave */
static void sunken_grave(double cx, double cy, double a, PyRandom *rng, SoObj *o, GG **foot) {
    F ff = FR(cx, cy, a); double L = u(5.4, 6.0), W = u(2.2, 2.5); so_obj_init(o, "headstone"); double k = 0.42;
    GG *inner = RECT(cx, cy, L, W, a);
    { GG *g = BUF(inner, -0.05, 16); double d = u(0.28, 0.36); GCUT(o, g, d, "pit"); }
    { double x = u(-0.6, 0.6); P c = FF(&ff, x, 0); GG *g = INT(BLOB(c.x, c.y, W * 0.33, rng, 3), BUF(inner, -0.45, 16)); double d = u(0.6, 0.7); GCUT(o, g, d, "pit"); }
    { GL l = { 0 }; double xs[64]; int nx = AR(xs, 64, -L / 2 + 0.5, L / 2 - 1.2, 1.4);
      for (int i = 0; i < nx; i++) for (int j = 0; j < 2; j++) { double sv = sgn(j); if (!(rnd() < 0.6)) continue; P p = FF(&ff, xs[i], sv * (W / 2 - 0.32)); double d1 = u(0.6, 1.2), d2 = u(0.35, 0.6); GA(&l, BARL(p, FF(&ff, xs[i] + d1, sv * (W / 2 - d2)), 0.15)); }
      GCUT(o, INT(UL(&l), inner), 0.12, "pit"); }
    int drop = -1; if (rnd() < 0.6) drop = (int)CHI(rng, 4);
    P pq[4][2] = { { FF(&ff, -L / 2, -W / 2 - k / 2), FF(&ff, L / 2, -W / 2 - k / 2) }, { FF(&ff, -L / 2, W / 2 + k / 2), FF(&ff, L / 2, W / 2 + k / 2) },
                   { FF(&ff, -L / 2 - k / 2, -W / 2), FF(&ff, -L / 2 - k / 2, W / 2) }, { FF(&ff, L / 2 + k / 2, -W / 2), FF(&ff, L / 2 + k / 2, W / 2) } };
    GL sides = { 0 };
    for (int i = 0; i < 4; i++) {
        P p = pq[i][0], q = pq[i][1];
        if (i == drop) {
            P m = { (p.x + q.x) / 2, (p.y + q.y) / 2 }; double t = ATAN2(q.y - p.y, q.x - p.x) + u(-0.12, 0.12); P c = { m.x + (cx - m.x) * 0.18, m.y + (cy - m.y) * 0.18 }; double n_ = DIST(p, q) * 0.85;
            int kb = UP(o, RECTp(c, n_, k, t), 0.3, NULL);
            double d1 = u(0.1, 0.3); P a1 = POLp(c, n_ * d1, t); double d2 = u(0.1, 0.3); double ex = POLp(c, n_ * d2, t).x; double d3 = u(0.1, 0.3); double ey = POLp(c, n_ * d3, t).y;
            CUT(o, BARL(a1, POL(ex, ey, 0.6, t + 1.57), 0.14), kb, 0.2);
        } else GA(&sides, BARL(p, q, k));
    }
    { GG *g = UL(&sides); double hh = u(0.5, 0.6); int kb = UP(o, g, hh, NULL); GL l = { 0 }; double xs[2] = { -L * 0.2, L * 0.2 }; for (int i = 0; i < 2; i++) GA(&l, BARL(FF(&ff, xs[i], -W), FF(&ff, xs[i] + 0.02, W), 0.13)); CUT(o, UL(&l), kb, 0.1); }
    P cs[4]; { int n = 0; for (int i = 0; i < 2; i++) for (int j = 0; j < 2; j++) cs[n++] = FF(&ff, sgn(i) * (L / 2 + k / 2), sgn(j) * (W / 2 + k / 2)); }
    GL l1 = { 0 }, l2 = { 0 }, l3 = { 0 }; for (int i = 0; i < 4; i++) { GA(&l1, RECTp(cs[i], 0.7, 0.7, a)); }
    int cb = UP(o, UL(&l1), 0.72, NULL); for (int i = 0; i < 4; i++) GA(&l2, RECTp(cs[i], 0.42, 0.42, a)); int c2 = XON(o, UL(&l2), cb, 0.12, NULL);
    for (int i = 0; i < 4; i++) GA(&l3, RECTp(cs[i], 0.22, 0.22, a)); XON(o, UL(&l3), c2, 0.1, NULL);
    *foot = OUT(o);
}

/* mortsafe */
static void mortsafe(double cx, double cy, double a, PyRandom *rng, SoObj *o, GG **foot) {
    F ff = FR(cx, cy, a); double L = u(6.3, 7.1), W = u(2.6, 3.0); so_obj_init(o, "fence"); GG *R = RECT(cx, cy, L, W, a);
    int kb = UP(o, R, 0.5, "headstone"); int sl = XON(o, INS(R, 0.5), kb, 0.25, "headstone"); GROOVE(o, INS(R, 0.7), sl, 0.14, 0.08);
    int nb = (int)RI(rng, 3, 4); double xb[4]; for (int i = 0; i < nb; i++) xb[i] = (i + 0.5) / nb * (L - 1.6) - (L - 1.6) / 2;
    int bd; { GL l = { 0 }; for (int i = 0; i < nb; i++) GA(&l, RECTp(FF(&ff, xb[i], 0), 0.5, W - 0.9, a)); bd = XON(o, UL(&l), sl, 0.18, "iron"); }
    { P st[8]; int n = 0; for (int i = 0; i < nb; i++) for (int j = 0; j < 2; j++) st[n++] = FF(&ff, xb[i], sgn(j) * (W / 2 - 0.62)); XSTUDS(o, st, n, bd, 0.16, 0.08, "iron", 6); }
    int hp = XON(o, RECTp(FF(&ff, -L / 2 + 0.95, 0), 0.5, 0.8, a), sl, 0.12, "iron"); XON(o, DISCp(FF(&ff, -L / 2 + 0.95, 0), 0.27, 10), hp, 0.12, "iron");
    if (rnd() < 0.6) { int n = (int)RI(rng, 3, 4); GG *t = xg_text(&ff, -L * 0.1, 0.0, n, 0.42, 0.11, rng, 0.52, CHARS); GL l = { 0 }; for (int i = 0; i < nb; i++) GA(&l, RECTp(FF(&ff, xb[i], 0), 0.62, W, a)); CUT(o, DIF(t, UL(&l)), sl, 0.08); }
    P ps[64]; int np = 0; double e = 0.62, vs2[2] = { -W / 2 + 0.22, W / 2 - 0.22 }, xs2[2] = { -L / 2 + 0.22, L / 2 - 0.22 };
    for (int i = 0; i < 2; i++) { int m = (int)PINT((L - 2 * e) / 0.85) + 1; if (m < 3) m = 3; double xs[32]; LS(xs, -L / 2 + e, L / 2 - e, m); for (int j = 0; j < m; j++) ps[np++] = FF(&ff, xs[j], vs2[i]); }
    for (int i = 0; i < 2; i++) { int m = (int)PINT((W - 2 * e) / 0.85) + 1; if (m < 2) m = 2; double vs[32]; LS(vs, -W / 2 + e, W / 2 - e, m); for (int j = 0; j < m; j++) ps[np++] = FF(&ff, xs2[i], vs[j]); }
    int lean[64]; for (int i = 0; i < np; i++) lean[i] = rnd() < 0.08;
    { int k = 0; for (int i = 0; i < np; i++) if (!lean[i]) ps[k++] = ps[i]; np = k; }
    double hp_ = u(1.6, 1.9); int po;
    { GL l = { 0 }; for (int i = 0; i < np; i++) GA(&l, RECTp(ps[i], 0.32, 0.32, a + 0.785)); po = XON(o, UL(&l), kb, hp_, "iron"); }
    { GL l = { 0 }; for (int i = 0; i < np; i++) GA(&l, RECTp(ps[i], 0.18, 0.18, a + 0.785)); XON(o, UL(&l), po, 0.22, "iron"); }
    P cs[4]; { int n = 0; for (int i = 0; i < 2; i++) for (int j = 0; j < 2; j++) cs[n++] = FF(&ff, sgn(i) * (L / 2 - 0.3), sgn(j) * (W / 2 - 0.3)); }
    int cp; { GL l = { 0 }; for (int i = 0; i < 4; i++) GA(&l, RECTp(cs[i], 0.56, 0.56, a)); cp = XON(o, UL(&l), kb, hp_ + 0.3, "iron"); }
    for (int i = 0; i < 4; i++) { S *s = SN(); DOME(s, cs[i].x, cs[i].y, 0.28, 0.34, 0, 1.0, 0.0, 20); ST(o, s, "iron", cp, NULL, 0, NULL); }
    *foot = OUT(o);
}

/* effigy_slab */
typedef struct { double bl, hw; } Eff;
static int eff_rf(void *c, double t, double *out) { Eff *e = (Eff *)c; double s = (t + e->bl / 2) / e->bl; *out = e->hw * (0.62 + 0.38 * MN(1.0, s / 0.75)) * (s < 0.93 ? 1.0 : MX(0.55, 1 - (s - 0.93) * 6)); return SO_OK; }
static int hands_rf(void *c, double t, double *out) { (void)c; *out = 0.2 * (1 - fabs(t) / 0.6); return SO_OK; }
static void effigy_slab(double cx, double cy, double a, PyRandom *rng, SoObj *o, GG **foot) {
    F ff = FR(cx, cy, a); double L = u(6.0, 7.0), W = u(2.6, 3.0); so_obj_init(o, "headstone"); GG *g = RECT(cx, cy, L, W, a);
    double hh = u(0.7, 0.85); int sb = UP(o, g, hh, NULL); int ch = XON(o, INS(g, 0.18), sb, 0.12, NULL); GROOVE(o, INS(g, 0.42), ch, 0.13, 0.08);
    for (int i = 0; i < 2; i++) { double sv = sgn(i); if (rnd() < 0.8) { double d = u(0.7, 1.2); int n = (int)RI(rng, 7, 10); CUT(o, xg_text(&ff, -L / 2 + d, sv * (W / 2 - 0.62), n, 0.3, 0.1, rng, 0.36, CHARS), ch, 0.08); } }
    if (rnd() < 0.3) { P l[3]; double d = u(-1.5, 1.5); l[0] = FF(&ff, d, -W); d = u(-0.6, 0.6); l[1] = FF(&ff, d, 0); d = u(-1.5, 1.5); l[2] = FF(&ff, d, W); CUT(o, LINE(l, 3, 0.16), ch, 0.5); }
    double bl = L * 0.6, x0 = -L * 0.07, hw = W * 0.24, hx = x0 + bl / 2 + 0.45;
    int cu = XON(o, RECTp(FF(&ff, hx + 0.05, 0), 0.95, hw * 2 * 0.95, a), ch, 0.22, NULL);
    { P st[4]; int n = 0; for (int i = 0; i < 2; i++) for (int j = 0; j < 2; j++) st[n++] = FF(&ff, hx + 0.05 + sgn(i) * 0.4, sgn(j) * hw * 0.88); XSTUDS(o, st, 4, cu, 0.13, 0.06, NULL, 6); }
    Eff *ec = (Eff *)xr_mem(sizeof *ec); ec->bl = bl; ec->hw = hw;
    S *ps = SN(); { P q = FF(&ff, x0, 0); SPIN(ps, q.x, q.y, a, bl, eff_rf, ec, 1.0, 0, "round"); }
    C folds[3]; { double vs[3] = { -hw * 0.45, 0.0, hw * 0.45 }; for (int i = 0; i < 3; i++) { folds[i].q = BARL(FF(&ff, x0 - bl / 2 + 0.1, vs[i]), FF(&ff, x0 + bl * 0.05, vs[i] * 0.8), 0.17); folds[i].fl = hw * 0.78; } }
    { P q = FF(&ff, x0 + bl * 0.22, 0); SPIN(ps, q.x, q.y, a, 0.9, hands_rf, NULL, 1.2, hw * 0.75, "round"); }
    for (int i = 0; i < 2; i++) { P q = FF(&ff, x0 - bl / 2 + 0.25, sgn(i) * hw * 0.32); DOME(ps, q.x, q.y, 0.24, hw * 0.7, 0, 0.8, a, 20); }
    if (rnd() < 0.4) EXT(ps, U2(BARL(FF(&ff, x0 - bl * 0.38, hw * 0.4), FF(&ff, x0 + bl * 0.12, hw * 0.4), 0.2), BARL(FF(&ff, x0 + bl * 0.06, hw * 0.12), FF(&ff, x0 + bl * 0.06, hw * 0.68), 0.2)), hw * 0.95, 0);
    if (rnd() < 0.75) { P q = FF(&ff, hx, 0); DOME(ps, q.x, q.y, hw * 0.62, hw * 1.0 + 0.22, 0, 0.85, a, 20); }
    ST(o, ps, NULL, ch, folds, 3, NULL);
    double dx = x0 - bl / 2 - 0.5; S *s = SN(); { P q = FF(&ff, dx, 0); LOAF(s, q.x, q.y, a + 1.57, hw * 1.4, 0.6, 0.45, 0.0); } { P q = FF(&ff, dx, hw * 0.55); DOME(s, q.x, q.y, 0.26, 0.55, 0, 1.0, 0.0, 20); }
    ST(o, s, NULL, ch, NULL, 0, NULL);
    *foot = OUT(o);
}

/* broken_obelisk */
static void broken_obelisk(double cx, double cy, double a, PyRandom *rng, SoObj *o, GG **foot) {
    F ff = FR(cx, cy, a); double Sz = u(2.0, 2.4); so_obj_init(o, "headstone");
    int b1 = UP(o, RECT(cx, cy, Sz, Sz, a), 0.45, NULL); int b2 = XON(o, RECT(cx, cy, Sz * 0.84, Sz * 0.84, a), b1, 0.42, NULL);
    double hh = u(1.3, 1.6); int dd = XON(o, RECT(cx, cy, Sz * 0.66, Sz * 0.66, a), b2, hh, NULL); GROOVE(o, RECT(cx, cy, Sz * 0.66 - 0.34, Sz * 0.66 - 0.34, a), dd, 0.12, 0.07);
    int pl = XON(o, RECT(cx, cy, Sz * 0.5, Sz * 0.5, a), dd, 0.2, NULL); double sw = Sz * 0.38; double ta = a + u(-0.04, 0.04); GG *sg = RECT(cx, cy, sw, sw, ta); hh = u(0.7, 1.0); int st = XON(o, sg, pl, hh, NULL);
    { P q[4]; q[0] = FF(&ff, -sw, -sw * 0.1); double d = u(0.0, 0.4); q[1] = FF(&ff, sw * d, sw); q[2] = FF(&ff, sw, sw); q[3] = FF(&ff, sw, -sw); GG *g = POLY(q, 4); d = u(0.35, 0.55); CUT(o, g, st, d); }
    { P p = FF(&ff, -sw, sw * 0.3); double d = u(0.1, 0.4); CUT(o, TRI(p, FF(&ff, sw * 0.1, sw * d), sw * 0.5), st, 0.2); }
    double d1 = u(2.2, 2.7), d2 = u(-0.6, 0.6); P c = FF(&ff, Sz / 2 + d1, d2); double b = a + u(-0.5, 0.5); F f2 = FR(c.x, c.y, b); double Lf = u(2.6, 3.4), w0 = sw * 0.95, w1 = sw * 0.72, tp = 0.55;
    int n = 8; S *ps = SN(); double xs[9]; LS(xs, -Lf / 2, Lf / 2 - tp, n + 1);
    for (int i = 0; i < n; i++) { double s = (i + 0.5) / n, w = w0 + (w1 - w0) * s; EXT(ps, RECTp(FF(&f2, (xs[i] + xs[i + 1]) / 2, 0), xs[i + 1] - xs[i] + 0.02, w, b), w * 0.95, 0); }
    { P q[3] = { FF(&f2, Lf / 2 - tp, -w1 / 2), FF(&f2, Lf / 2, 0), FF(&f2, Lf / 2 - tp, w1 / 2) }; EXT(ps, POLY(q, 3), w1 * 0.55, 0); }
    { P q = FF(&f2, Lf / 2 - tp / 2, 0); RIDGE(ps, q.x, q.y, b, tp, w1, w1 * 0.95, 0.0); }
    P jq[5]; jq[0] = FF(&f2, -Lf / 2 - 0.05, -w0); { double d = u(0.15, 0.4); jq[1] = FF(&f2, -Lf / 2 + d, -w0 * 0.2); d = u(0.0, 0.2); jq[2] = FF(&f2, -Lf / 2 + d, w0 * 0.15); d = u(0.2, 0.45); jq[3] = FF(&f2, -Lf / 2 + d, w0); } jq[4] = FF(&f2, -Lf / 2 - 0.05, w0);
    C cuts[128]; int nc = 0; cuts[nc].q = POLY(jq, 5); cuts[nc].fl = w0 * 0.45; nc++;
    { int m = (int)RI(rng, 3, 4); GG *lt = xg_text(&f2, -Lf / 2 + 0.5, 0.0, m, 0.5, 0.16, rng, 0.56, CHARS); GL qs = { 0 }; POLYS(lt, &qs);
      for (int i = 0; i < qs.n && nc < 128; i++) { P qc = CENT(qs.v[i]); cuts[nc].q = qs.v[i]; cuts[nc].fl = (w0 + (w1 - w0) * ((qc.x - c.x) * COS(b) + (qc.y - c.y) * SIN(b) + Lf / 2) / Lf) * 0.95 - 0.08; nc++; }
      if (qs.n >= 128) xr_fail(); }
    ST(o, ps, NULL, -1, cuts, nc, RECTp(c, Lf, w0, b));
    int m = (int)RI(rng, 2, 4);
    for (int i = 0; i < m; i++) { double x = u(0.3, 0.9), y = u(-1.0, 1.0); P p = FF(&f2, -Lf / 2 - x, y); double r = u(0.22, 0.35), h = u(0.2, 0.35); S *s = SN(); ROCK(s, p.x, p.y, r, h, rng, 0, 0.0, 9); ST(o, s, NULL, -1, NULL, 0, NULL); }
    *foot = OUT(o);
}

/* footstones */
static void footstones(double cx, double cy, double a, PyRandom *rng, SoObj *o, GG **foot) {
    F ff = FR(cx, cy, a); int n = (int)RI(rng, 3, 5); so_obj_init(o, "headstone"); int any_ = 0;
    for (int i = 0; i < n; i++) {
        if (rnd() < 0.12) continue;
        any_ = 1; double x = (i - (n - 1) / 2.0) * 2.0 + u(-0.15, 0.15), y = u(-0.2, 0.2); P c = FF(&ff, x, y); double w = u(1.15, 1.45), h = u(1.6, 2.2), t = a + u(-0.2, 0.2); F f2 = FR(c.x, c.y, t);
        if (rnd() < 0.3) {
            double sv = CH2(rng); double tr = t + sv * 1.5708; F f3 = FR(c.x, c.y, tr);
            GG *g = U2(RECTp(FF(&f3, -0.2, 0), h - 0.4, w, tr), ELL(PXY(FF(&f3, h / 2 - 0.42, 0)), w * 0.5, 0.42, tr, 16));
            int sb = UP(o, g, 0.42, NULL); GROOVE(o, INS(g, 0.2), sb, 0.12, 0.07);
            P fc = FF(&f3, 0.1, 0); F ft = FR(fc.x, fc.y, t + sv * 1.5708 - 1.5708);
            GG *t1 = xg_text(&ft, -0.42, 0.22, 2, 0.4, 0.11, rng, 0.48, "HILTEAMNVFK"); GG *t2 = xg_text(&ft, -0.5, -0.35, 4, 0.3, 0.1, rng, 0.3, "17");
            CUT(o, U2(t1, t2), sb, 0.08);
            continue;
        }
        { S *s = SN(); DOME(s, c.x, c.y, w * 0.62, 0.22, 0, 0.6, t, 20); ST(o, s, "grass", -1, NULL, 0, DISCp(c, w * 0.6, 20)); }
        double th = u(0.45, 0.55); int gable = rnd() < 0.35;
        S *ps = SN(); EXT(ps, RECTp(c, w, th, t), h * 0.78, 0);
        if (gable) { EXT(ps, RECTp(c, w * 0.62, th, t), h * 0.9, 0); EXT(ps, RECTp(c, w * 0.3, th, t), h, 0); }
        else { EXT(ps, RECTp(c, w * 0.85, th, t), h * 0.9, 0); EXT(ps, RECTp(c, w * 0.58, th, t), h, 0); }
        C cu[1]; int nc = 0; if (rnd() < 0.5) { cu[0].q = BARL(FF(&f2, -w * 0.3, 0), FF(&f2, w * 0.3, 0), 0.16); cu[0].fl = h * 0.86; nc = 1; }
        ST(o, ps, NULL, -1, cu, nc, RECTp(c, w, th, t));
    }
    if (!any_) xr_none();
    GG *ex[1] = { RECT(cx, cy, n * 2.0 + 0.4, 1.8, a) };
    *foot = OUTX(o, ex, 1);
}

/* wooden_cross */
static void wooden_cross(double cx, double cy, double a, PyRandom *rng, SoObj *o, GG **foot) {
    F ff = FR(cx, cy, a); double L = u(3.2, 3.7), W = u(1.6, 2.0); so_obj_init(o, "wood"); double th = u(0.34, 0.4), pw = 0.52;
    double xc = L / 2 - L * 0.3; int loose = rnd() < 0.2;
    GG *post = U2(RECTp(FF(&ff, -0.1, 0), L - 0.4, pw, a), TRI(FF(&ff, -L / 2 + 0.25, 0), FF(&ff, -L / 2 - 0.2, 0), pw));
    int p = UP(o, post, th, NULL); CUT(o, xg_grain(&ff, L * 0.8, pw, rng, 1, 0.11), p, 0.1);
    if (loose) {
        double t2 = u(0.9, 1.3); t2 = a + t2 * CH2(rng); double x = u(-0.6, 0.2), sv = CH2(rng); double y = sv * u(0.9, 1.2); P c = FF(&ff, x, y); F f2 = FR(c.x, c.y, t2);
        CUT(o, U2(BARL(FF(&ff, xc, -pw), FF(&ff, xc, pw), 0.12), BARL(FF(&ff, xc + pw, -pw), FF(&ff, xc + pw, pw), 0.12)), p, 0.14);
        int ar = UP(o, RECTp(c, W, pw, t2), th, NULL); GG *gr = xg_grain(&f2, W * 0.8, pw, rng, 1, 0.11); CUT(o, U2(gr, RECTp(c, pw, pw - 0.16, t2)), ar, 0.1);
    } else {
        int ar = UP(o, RECTp(FF(&ff, xc, 0), pw, W, a), th + 0.02, NULL); CUT(o, U2(BARL(FF(&ff, xc - pw / 2, -W), FF(&ff, xc - pw / 2, W), 0.11), BARL(FF(&ff, xc + pw / 2, -W), FF(&ff, xc + pw / 2, W), 0.11)), ar, 0.1);
        int nb = XON(o, RECTp(FF(&ff, xc, 0), pw * 1.1, W * 0.46, a), ar, 0.1, NULL); P fc = FF(&ff, xc, 0); F f3 = FR(fc.x, fc.y, a + 1.5708); CUT(o, xg_text(&f3, -W * 0.17, 0.0, 3, 0.26, 0.09, rng, 0.17, CHARS), nb, 0.07);
        P st[2] = { FF(&ff, xc, -1 * (W / 2 - 0.15)), FF(&ff, xc, 1 * (W / 2 - 0.15)) }; XSTUDS(o, st, 2, ar, 0.11, 0.06, "iron", 6);
    }
    int cp = XON(o, RECTp(FF(&ff, L / 2 - 0.55, 0), 0.42, pw + 0.04, a), p, 0.12, NULL); CUT(o, BARL(FF(&ff, L / 2 - 0.55, -pw), FF(&ff, L / 2 - 0.55, pw), 0.1), cp, 0.08);
    *foot = OUT(o);
}

/* grave_bell */
static int bell_rf2(void *c, double q, double *out) { double bl = *(double *)c; *out = 0.3 + 0.28 * px_real((q + bl / 2) / bl, 1.6) + (q > bl / 2 - 0.12 ? 0.06 : 0.0); return SO_OK; }
static void grave_bell(double cx, double cy, double a, PyRandom *rng, SoObj *o, GG **foot) {
    F ff = FR(cx, cy, a); so_obj_init(o, "iron");
    double y = u(-0.2, 0.2); P pc = FF(&ff, -1.35, y); int fl = UP(o, DISCp(pc, 0.55, 16), 0.14, NULL); double hh = u(1.1, 1.4); int pp = XON(o, DISCp(pc, 0.32, 14), fl, hh, NULL); CUT(o, DISCp(pc, 0.17, 10), pp, 0.5);
    y = u(0.55, 0.7); P sc = FF(&ff, -1.0, y); hh = u(0.9, 1.2); int sb = UP(o, RECTp(sc, 0.52, 0.52, a), hh, "wood");
    { P p = FF(&ff, -1.25, 0.6); double d = u(0.5, 0.8); CUT(o, TRI(p, FF(&ff, -0.75, d), 0.4), sb, 0.3); }
    double t = a + u(-0.35, 0.35); P fc = FF(&ff, -0.65, 0.6); F f2 = FR(fc.x, fc.y, t); double Lp = u(1.9, 2.3);
    P q[5]; { double d = u(0.0, 0.2); q[0] = FF(&f2, d, -0.25); q[1] = FF(&f2, Lp, -0.25); q[2] = FF(&f2, Lp, 0.25); d = u(0.1, 0.3); q[3] = FF(&f2, d, 0.25); d = u(0.25, 0.45); q[4] = FF(&f2, d, 0.0); }
    int po = UP(o, POLY(q, 5), 0.46, "wood"); { P gc = FF(&f2, Lp / 2, 0); F fg = FR(gc.x, gc.y, t); CUT(o, xg_grain(&fg, Lp * 0.8, 0.5, rng, 1, 0.1), po, 0.1); }
    double sv = CH2(rng); int arm = UP(o, U2(RECTp(FF(&f2, Lp - 0.15, sv * 0.55), 0.3, 1.1, t), RECTp(FF(&f2, Lp - 0.35, sv * 0.28), 0.6, 0.28, t + 0.785)), 0.3, NULL);
    { P st[1] = { FF(&f2, Lp - 0.15, sv * 0.12) }; XSTUDS(o, st, 1, arm, 0.11, 0.07, NULL, 6); }
    double d = u(-0.3, 0.1); P bc = FF(&f2, Lp + d, sv * 1.35); double bt = t + sv * u(1.2, 1.9); F fb = FR(bc.x, bc.y, bt); static double bl = 1.3;
    { S *s = SN(); SPIN(s, bc.x, bc.y, bt, bl, bell_rf2, &bl, 1.0, 0, "round"); P z = FF(&fb, -bl / 2 - 0.12, 0); EXT(s, ANN(z.x, z.y, 0.22, 0.09, 8), 0.24, 0);
      C cu[1] = { { ELL(PXY(FF(&fb, bl / 2 - 0.14, 0)), 0.14, 0.36, bt, 10), 0.4 } }; ST(o, s, "brass", -1, cu, 1, DISCp(bc, 0.75, 20)); }
    P w0 = FF(&fb, -bl / 2 - 0.3, 0), w1 = POLp(pc, 0.38, ATAN2(bc.y - pc.y, bc.x - pc.x)); P pts[8]; int np = lf_wiggle(w0.x, w0.y, w1.x, w1.y, rng, 0.35, 4, &pts[0].x);
    { S *s = SN(); ROD(s, pts, np, 0.25, NULL, NULL, 1.1, 0, 0); ST(o, s, "rope", -1, NULL, 0, BUF(LSTR(pts, np), 0.3, 16)); }
    *foot = OUT(o);
}

/* robbers_hole */
static void robbers_hole(double cx, double cy, double a, PyRandom *rng, SoObj *o, GG **foot) {
    F ff = FR(cx, cy, a); so_obj_init(o, "iron");
    double r = u(1.5, 1.9); GG *p = BLOB(cx, cy, r, rng, 4); double d = u(0.6, 0.85); GCUT(o, p, d, "pit"); { GG *g = INS(p, 0.55); d = u(1.1, 1.4); GCUT(o, g, d, "pit"); }
    double x = u(2.8, 3.4), y = u(-1, 1); P mc = FF(&ff, x, y); double rm = u(1.3, 1.6), hm = u(1.4, 1.9), t0 = u(0, 6.28);
    S *hp = SN(); { double sq = u(0.75, 0.9); DOME(hp, mc.x, mc.y, rm, hm, 0, sq, t0, 20); }
    for (int k = 0; k < 3; k++) { double e = u(-0.4, 0.4); P q = POLp(mc, rm * 0.75, t0 + k * 2.1 + e); double r1 = rm * u(0.45, 0.6), h1 = hm * u(0.4, 0.6); DOME(hp, q.x, q.y, r1, h1, 0, 1.0, 0.0, 20); }
    ST(o, hp, "earth", -1, NULL, 0, DISCp(mc, rm * 1.3, 20));
    int n = (int)RI(rng, 3, 5);
    for (int i = 0; i < n; i++) {
        double rr = u(0.4, 1.4), tt = u(0, 6.28); P q = POLp(mc, rr, tt); double r1 = u(0.3, 0.45), h1 = u(0.25, 0.4); double lift = hm * MX(0.0, 1 - PW(DIST(q, mc) / rm, 2)) * 0.85;
        S *s = SN(); ROCK(s, q.x, q.y, r1, h1, rng, 0, lift, 9); ST(o, s, "earth", -1, NULL, 0, DISCp(q, 0.4, 20));
    }
    x = u(-1, 1); P fc = FF(&ff, x, -3.0); double ft = a + u(-0.4, 0.4); F f2 = FR(fc.x, fc.y, ft); double L = u(5.0, 6.5);
    { S *s = SN(); P l[3] = { FF(&f2, -L / 2, 0), FF(&f2, L / 2 - 0.6, 0), FF(&f2, L / 2, 0.55) }; ROD(s, l, 3, 0.27, NULL, NULL, 1.15, 0, 0);
      EXT(s, U2(TRI(FF(&f2, -L / 2 + 0.05, 0), FF(&f2, -L / 2 - 0.45, 0.2), 0.5), TRI(FF(&f2, -L / 2 + 0.05, 0), FF(&f2, -L / 2 - 0.45, -0.2), 0.5)), 0.22, 0);
      ST(o, s, NULL, -1, NULL, 0, RECTp(FF(&f2, 0, 0), L, 1.2, a)); }
    if (rnd() < 0.75) {
        double t = a + u(1.2, 1.9); double rr = u(1.2, 1.8), ta = a + u(2.2, 4.0); P c = POL(cx, cy, rr, ta); F fb = FR(c.x, c.y, t);
        int m = (int)RI(rng, 1, 2);
        for (int k = 0; k < m; k++) {
            double Lb = u(1.8, 2.4), v = k * 0.78, ty = t + u(-0.15, 0.15); P q[6]; q[0] = FF(&fb, -Lb / 2, v - 0.34); q[1] = FF(&fb, Lb / 2, v - 0.34);
            double e = u(0.1, 0.5); q[2] = FF(&fb, Lb / 2 + e, v - 0.1); e = u(0.0, 0.3); q[3] = FF(&fb, Lb / 2 - e, v + 0.05); e = u(0.1, 0.4); q[4] = FF(&fb, Lb / 2 + e, v + 0.34); q[5] = FF(&fb, -Lb / 2, v + 0.34);
            GG *g = POLY(q, 6); int bb = UP(o, ROT(g, ty - t, SHP_ORIGIN_POINT, c.x, c.y, 1), 0.32, "coffin");
            P gc = FF(&fb, 0, v); F fg = FR(gc.x, gc.y, ty); CUT(o, xg_grain(&fg, Lb * 0.7, 0.68, rng, 1, 0.1), bb, 0.1);
        }
    }
    if (rnd() < 0.6) {
        double rr = u(1.7, 2.1), tt = u(0, 6.28); P lc = POLp(mc, rr, tt); int ln = UP(o, DISCp(lc, 0.42, 14), 1.0, "tin");
        { S *s = SN(); CONE(s, lc.x, lc.y, 0.42, 0.36, 0.14, 0, 16, 1.0, 0.0); ST(o, s, "tin", ln, NULL, 0, NULL); }
        XON(o, RECTp(POLp(lc, 0.32, a), 0.3, 0.42, a), ln, 0.08, "glass");
    } else if (rnd() < 0.6) {
        double x1 = u(-0.5, 0.5), y1 = u(1.8, 2.3); P rc = FF(&ff, x1, y1); double ang = u(0, 6.28), sp[62]; xr_ok(sd_spiral(rc.x, rc.y, 0.25, 0.85, 2.2, ang, 30, sp));
        S *s = SN(); ROD(s, (const P *)sp, 31, 0.25, NULL, NULL, 1.0, 0, 0); ST(o, s, "rope", -1, NULL, 0, DISCp(rc, 1.0, 20));
    }
    *foot = OUT(o);
}

static const XrDef DEFS[] = {
    { "graveyard", "made", "fence", "fallen_railing", 10.5, 0, 0, fallen_railing },
    { "graveyard", "made", "coffin", "open_coffin", 10.5, 0, 0, open_coffin },
    { "graveyard", "marks", "pit", "sunken_grave", 10.5, 1, 0, sunken_grave },
    { "graveyard", "made", "fence", "mortsafe", 10.5, 0, 0, mortsafe },
    { "graveyard", "made", "headstone", "effigy_slab", 10.5, 0, 0, effigy_slab },
    { "graveyard", "made", "headstone", "broken_obelisk", 10.5, 0, 0, broken_obelisk },
    { "graveyard", "made", "headstone", "footstones", 10.5, 0, 0, footstones },
    { "graveyard", "made", "wood", "wooden_cross", 10.5, 0, 0, wooden_cross },
    { "graveyard", "made", "brass", "grave_bell", 10.5, 1, 0, grave_bell },
    { "graveyard", "made", "iron", "robbers_hole", 10.5, 1, 0, robbers_hole },
};
void bld_sdrx_graveyard(void) { xr_register(DEFS, (int)(sizeof DEFS / sizeof DEFS[0])); }
