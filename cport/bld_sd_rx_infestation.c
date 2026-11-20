/* bld_sd_rx_infestation.c: sd_r_x_infestation.py in C (see bld_sd_rx.h; _pl / _lean / _curl of sd_r_infestation.py
   are XPL / LEAN / CURL in bld_sd_rx.c). */
#include "bld_sd_rx.h"

static double sgn(int i) { return i ? 1 : -1; }

/* x_veins(p, prof, t, n=8, w=0.12): 4 cuts into out */
static int x_veins(C *out, P p, const double *prof, int np, double t, int n, double w) {
    int i = 0; for (int k = 1; k < np; k++) if (prof[2 * k] > prof[2 * i]) i = k;          /* max(range, key=r): the first largest */
    const double *top = prof + 2 * i; int nt = np - i; double rmax = top[0];
    for (int k = 0; k < 4; k++) {
        double r0 = rmax * (0.25 + 0.19 * k), r1 = rmax * (0.25 + 0.19 * (k + 1)) + 0.05; GL l = { 0 };
        for (int j = 0; j < n; j++) GA(&l, BARL(POLp(p, r0, t + 6.2832 * j / n + 0.04 * k), POLp(p, r1, t + 6.2832 * j / n + 0.04 * (k + 1)), w));
        double rr = MN(r1, rmax), z = top[2 * nt - 1];
        for (int m = 0; m + 1 < nt; m++) { double a0 = top[2 * m], z0 = top[2 * m + 1], a1 = top[2 * m + 2], z1 = top[2 * m + 3]; if (a1 <= rr && rr <= a0) { z = z0 + (z1 - z0) * (a0 - rr) / MX(1e-6, a0 - a1); break; } }
        out[k].q = UL(&l); out[k].fl = z - 0.1;
    }
    return 4;
}
/* x_egg(o, p, r, h, t, par=None, opened=False, kind="egg") */
static int x_egg(SoObj *o, P p, double r, double h, double t, int par, int opened, const char *kind) {
    double prof[14] = { r * 0.8, 0.0, r * 0.98, h * 0.25, r, h * 0.42, r * 0.92, h * 0.62, r * 0.72, h * 0.8, r * 0.42, h * 0.93, r * 0.18, h };
    S *ps = SN(); TURN(ps, p.x, p.y, prof, 7, 20, 1.0, 0.0, 0.0);
    C cuts[5]; C veins[4]; x_veins(veins, p, prof, 7, t + 0.4, 8, 0.12);
    if (opened) { cuts[0].q = STAR(p.x, p.y, r * 0.7, 0.24, 4, t); cuts[0].fl = h * 0.45; }
    else { GL l = { 0 }; for (int j = 0; j < 2; j++) GA(&l, BARL(POLp(p, r * 0.45, t + j * 1.5708), POLp(p, r * 0.45, t + j * 1.5708 + PI), 0.16)); cuts[0].q = UL(&l); cuts[0].fl = h * 0.84; }
    for (int k = 0; k < 4; k++) cuts[1 + k] = veins[k];
    return ST(o, ps, kind, par, cuts, 5, DISCp(p, r, 20));
}

/* egg_ledge */
static void egg_ledge(double cx, double cy, double a, PyRandom *rng, SoObj *o, GG **foot) {
    double R = u(3.2, 4.2); int n = (int)RI(rng, 3, 4); double span = n * u(2.15, 2.35) / R; so_obj_init(o, "resin");
    double ox = cx - R * COS(a), oy = cy - R * SIN(a);
    double ar[30]; xr_ok(sd_arc(ox, oy, R, a - span / 2, a + span / 2, 14, ar));
    GL gl = { 0 }; GA(&gl, LINE((const P *)ar, 15, 2.6));
    { double ts[256]; int nt = AR(ts, 256, a - span / 2, a + span / 2, 0.85 / R); for (int i = 0; i < nt; i++) { P c = POL(ox, oy, R + 1.25, ts[i]); double r = u(0.4, 0.62); GA(&gl, DISCp(c, r, 10)); } }
    GG *g = UL(&gl); double hh = u(0.55, 0.75); int b = UP(o, g, hh, NULL);
    { GL l = { 0 }; for (int k = 0; k < n - 1; k++) { double t = a - span / 2 + span * (k + 1) / n; GA(&l, BARL(POL(ox, oy, R - 2, t), POL(ox, oy, R + 2, t), 0.28)); } CUT(o, UL(&l), b, 0.2); }
    { GL l = { 0 }; double ts[256]; int nt = AR(ts, 256, a - span / 2 + 0.2 / R, a + span / 2, 0.55 / R); for (int i = 0; i < nt; i++) GA(&l, BARL(POL(ox, oy, R + 0.5, ts[i]), POL(ox, oy, R + 1.4, ts[i] + 0.05), 0.12)); CUT(o, UL(&l), b, 0.1); }
    int op = -1; if (rnd() < 0.4) op = (int)RI(rng, 0, n - 1);
    for (int k = 0; k < n; k++) {
        double d = u(-0.15, 0.15); P p = POL(ox, oy, R - 0.1 + d, a - span / 2 + span * (k + 0.5) / n); double r = u(0.9, 1.02), h = u(2.4, 2.9), t = u(0, 1.5);
        x_egg(o, p, r, h, t, b, k == op, "egg");
    }
    *foot = OUT(o);
}

/* burst_egg: the petals are _lean spikes shifted up: sect(z - z0) if z >= z0 else sect(0.0) of a box */
typedef struct { GEOSGeometry *g; double h, z0; } Shifted;
static void shifted_free(void *c) { Shifted *s = (Shifted *)c; shp_free(s->g); free(s); }
static int shifted_sect(void *c, double z, GEOSGeometry **out) {
    Shifted *s = (Shifted *)c; double zz = z >= s->z0 ? z - s->z0 : 0.0;
    if (zz < 0.0 + s->h) { *out = shp_clone(s->g); return *out ? SO_OK : SO_ERR; }
    *out = NULL; return SO_OK;
}
static int root_rf(void *c, double q, double *out) { (void)c; *out = 0.46 - 0.2 * q; return SO_OK; }
static void burst_egg(double cx, double cy, double a, PyRandom *rng, SoObj *o, GG **foot) {
    double r = u(1.4, 1.6), h = r * u(2.0, 2.2); static const double n3[3] = { 4, 4, 5 }; int n = (int)CH(rng, n3, 3); so_obj_init(o, "egg");
    double prof[10] = { r * 0.82, 0.0, r, h * 0.3, r * 0.98, h * 0.52, r * 0.88, h * 0.74, r * 0.76, h * 0.86 };
    S *ps = SN(); TURN(ps, cx, cy, prof, 5, 24, 1.0, 0.0, 0.0);
    for (int k = 0; k < n; k++) {
        double t = a + 6.2832 * k / n + u(-0.2, 0.2); P p = POL(cx, cy, r * 0.66, t); double lean = u(0.5, 0.75);
        double hl = h * u(0.5, 0.6), dx = lean * COS(t), dy = lean * SIN(t), z0 = h * 0.7; int m = 8; double tip = 0.26, sq = 0.55, aa = t + 1.5708, pw = 0.6;
        for (int j = 0; j < m; j++) {          /* _lean(p[0], p[1], r * 0.62, hl, dx, dy, 8, 0.26, 0.55, t + 1.5708, 0.6) */
            double f = (double)j / (double)(m - 1), bb = PW(1 - f, pw), rr = r * 0.62;
            GG *e = ELL(p.x + dx * j / (m - 1), p.y + dy * j / (m - 1), MX(tip, rr * bb), MX(tip, rr * bb * sq), aa, 12);
            double top = 0.0 + hl * (j + 1) / m;          /* box: lift + h */
            Shifted *sh = (Shifted *)malloc(sizeof *sh); sh->g = shp_clone(e); sh->h = hl * (j + 1) / m; sh->z0 = z0;
            Solid *so = sol_custom(shifted_sect, sh, shifted_free, z0 + top); sol_push(ps, so); sol_unref(so);
        }
    }
    C cuts[5]; cuts[0].q = DISC(cx, cy, r * 0.5, 16); cuts[0].fl = h * 0.4; x_veins(cuts + 1, PT(cx, cy), prof, 5, a + 0.3, 7, 0.12);
    ST(o, ps, NULL, -1, cuts, 5, DISC(cx, cy, r + 0.3, 20));
    S *rs = SN(); int nr = (int)RI(rng, 4, 6);
    for (int k = 0; k < nr; k++) {
        double t = a + 6.2832 * k / 5 + u(-0.3, 0.3); double L = r * 0.3 + u(1.0, 1.5); double bend = u(-0.9, 0.9); P pts[8];
        int np = CURL(pts, POL(cx, cy, r * 0.75, t), t, L, bend, 6); ROD(rs, pts, np, 0, root_rf, NULL, 0.9, 0, 0);
    }
    ST(o, rs, NULL, -1, NULL, 0, DISC(cx, cy, r + 1.4, 20));
    if (rnd() < 0.6) { double t = u(0, 6.28); P c = POL(cx, cy, r + 1.0, t); double rr = u(0.6, 0.9); GCUT(o, BLOB(c.x, c.y, rr, rng, 3), 0.26, "acid"); }
    *foot = OUT(o);
}

/* molt */
static const double MOLT_ENV[14] = { 0.0, 0.3, 0.12, 0.5, 0.6, 0.86, 0.72, 0.8, 0.78, 1.0, 0.93, 0.9, 1.0, 0.25 };
static const double COC_ENV[18] = { 0.0, 0.0, 0.03, 0.4, 0.3, 0.72, 0.66, 1.0, 0.76, 0.9, 0.81, 0.56, 0.87, 0.66, 0.95, 0.56, 1.0, 0.0 };
typedef struct { double L, W; int ns; } Molt;
static int molt_rf(void *c, double t, double *out) {
    Molt *m = (Molt *)c; double s = (t + m->L / 2) / m->L, sc, md; int e = 0;
    if (s > 0.75) sc = 1.0;
    else { if (px_mod_d(s / 0.75 * m->ns, 1.0, &md) != 0) return SO_ERR; sc = 0.78 + 0.22 * PWs(1 - md, 0.7, &e); if (e) return SO_ERR; }
    *out = m->W * XPL(MOLT_ENV, 7, s) * sc + 0.03; return SO_OK;
}
static int leg_rf(void *c, double q, double *out) { (void)c; *out = 0.32 - 0.07 * q; return SO_OK; }
static void molt(double cx, double cy, double a, PyRandom *rng, SoObj *o, GG **foot) {
    F ff = FR(cx, cy, a); double L = u(4.8, 6.0), W = u(1.05, 1.25); int ns = (int)RI(rng, 6, 8); so_obj_init(o, "carapace"); double hk = u(1.15, 1.3);
    Molt *mc = (Molt *)xr_mem(sizeof *mc); mc->L = L; mc->W = W; mc->ns = ns;
    S *legs = SN();
    for (int k = 0; k < 3; k++) for (int i = 0; i < 2; i++) {
        double sv = sgn(i);
        if (rnd() < 0.2) continue;
        double x = L * (0.02 + 0.13 * k); P pp[3]; pp[0] = FF(&ff, x, sv * W * 0.4);
        double d1 = u(-0.2, 0.3), d2 = u(0.6, 1.0); pp[1] = FF(&ff, x + d1, sv * (W + d2));
        d1 = u(0.5, 1.0); d2 = u(1.2, 1.9); pp[2] = FF(&ff, x - d1, sv * (W + d2));
        ROD(legs, pp, 3, 0, leg_rf, NULL, 1.4, 0, 0);
    }
    for (int i = 0; i < 2; i++) { double sv = sgn(i); P pp[2]; pp[0] = FF(&ff, -L / 2 + 0.3, sv * 0.2); double d1 = u(0.6, 1.1), d2 = u(0.4, 0.8); pp[1] = FF(&ff, -L / 2 - d1, sv * d2); ROD(legs, pp, 2, 0, leg_rf, NULL, 1.4, 0, 0); }
    ST(o, legs, NULL, -1, NULL, 0, RECT(cx, cy, L + 2.0, 2 * W + 3.6, a));
    P sq[6] = { FF(&ff, -L * 0.12, 0), FF(&ff, L * 0.1, -0.38), FF(&ff, L * 0.42, -0.32), FF(&ff, L * 0.47, 0), FF(&ff, L * 0.42, 0.32), FF(&ff, L * 0.1, 0.38) };
    GG *split = POLY(sq, 6);
    GL el = { 0 }; for (int i = 0; i < 2; i++) { double sv = sgn(i); GA(&el, ELL(PXY(FF(&ff, L * 0.38, sv * W * 0.45)), 0.24, 0.16, a + sv * 0.4, 8)); } GG *eyes = UL(&el);
    S *body = SN(); SPIN(body, cx, cy, a, L, molt_rf, mc, hk, 0, "round");
    { int m = (int)RI(rng, 3, 4); double xs[4]; LS(xs, -L * 0.4, -L * 0.15, m); for (int i = 0; i < m; i++) { P q = FF(&ff, xs[i], 0); CONE(body, q.x, q.y, 0.3, W * 0.55 * hk + 0.5, 0.06, 0, 6, 1.0, 0.0); } }
    C cu[2] = { { split, 0.3 }, { eyes, W * hk * 0.5 } }; ST(o, body, NULL, -1, cu, 2, RECT(cx, cy, L, 2 * W, a));
    *foot = OUT(o);
}

/* cocoon */
typedef struct { double L, r; int nb; } Coc;
static int coc_rf(void *c, double t, double *out) {
    Coc *m = (Coc *)c; double s = (t + m->L / 2) / m->L, k, md; int e = 0;
    if (s > 0.79) k = 1.0;
    else { if (px_mod_d(s / 0.79 * m->nb, 1.0, &md) != 0) return SO_ERR; k = 0.86 + 0.14 * PWs(1 - md, 0.6, &e); if (e) return SO_ERR; }
    *out = m->r * XPL(COC_ENV, 9, s) * k + 0.03; return SO_OK;
}
static int strand_rf(void *c, double q, double *out) { (void)c; *out = 0.4 - 0.14 * q; return SO_OK; }
static void cocoon(double cx, double cy, double a, PyRandom *rng, SoObj *o, GG **foot) {
    F ff = FR(cx, cy, a); double L = u(5.4, 6.4), r = u(1.2, 1.4); so_obj_init(o, "web");
    int nb = (int)RI(rng, 6, 8); double sl = CH2(rng);
    Coc *cc = (Coc *)xr_mem(sizeof *cc); cc->L = L; cc->r = r; cc->nb = nb;
    UP(o, BUF(BUF(RECT(cx, cy, L * 0.85, 2 * r + 0.8, a), 0.4, 16), -0.2, 4), 0.2, "resin");
    C cuts[32]; int nc = 0; double xs[32]; int nx = AR(xs, 32, -L * 0.42, L * 0.25, 0.75);
    for (int i = 0; i < nx; i++) { double x = xs[i]; cuts[nc].q = sl > 0 ? BARL(FF(&ff, x - 0.6, -r * 1.2), FF(&ff, x + 0.6, r * 1.2), 0.16) : BARL(FF(&ff, x + 0.6, -r * 1.2), FF(&ff, x - 0.6, r * 1.2), 0.16); cuts[nc].fl = r * 0.9; nc++; }
    { S *s = SN(); SPIN(s, cx, cy, a, L, coc_rf, cc, 1.15, 0, "round"); ST(o, s, NULL, -1, cuts, nc, RECT(cx, cy, L, 2 * r, a)); }
    S *ps = SN(); int n = (int)RI(rng, 3, 5);
    for (int i = 0; i < n; i++) {
        double x = u(-L * 0.4, L * 0.35), sv = CH2(rng); P p = FF(&ff, x, sv * r * 0.3); double d1 = u(-1.2, 1.2), d2 = u(1.3, 2.0); P e = FF(&ff, x + d1, sv * (r + d2));
        P pq[2] = { p, e }; ROD(ps, pq, 2, 0, strand_rf, NULL, 1.1, 0, 0); FPUSH(o, DISCp(e, 0.3, 20));
    }
    ST(o, ps, NULL, -1, NULL, 0, RECT(cx, cy, L, 2 * r, a));
    double k = rnd();
    if (k < 0.45) {
        double y = u(-0.2, 0.2); P c = FF(&ff, -L / 2 - 0.55, y); double t = a + u(-0.4, 0.4); F f2 = FR(c.x, c.y, t); int b = UP(o, RRECTp(c, 1.8, 0.98, t, 0.3), 0.8, "metal");
        { GL l = { 0 }; double xs3[3] = { -0.5, -0.15, 0.2 }; for (int i = 0; i < 3; i++) GA(&l, BARL(FF(&f2, xs3[i], -1), FF(&f2, xs3[i], 1), 0.2)); CUT(o, UL(&l), b, 0.14); }
        ON(o, RRECTp(FF(&f2, 0.5, 0), 0.6, 0.8, t, 0.2), b, 0.2, "metal");
    } else if (k < 0.7) {
        double sv = CH2(rng); P c = FF(&ff, -L * 0.1, sv * (r + 0.5)); double t = ATAN2(c.y - cy, c.x - cx); S *hs = SN(); DOME(hs, c.x, c.y, 0.5, 0.55, 0, 1.0, 0.0, 20);
        for (int j = 0; j < 4; j++) { P p = POLp(c, 0.3, t + (j - 1.5) * 0.4); double d = u(0.7, 1.0); P pq[2] = { p, POLp(p, d, t + (j - 1.5) * 0.3) }; ROD(hs, pq, 2, 0.24, NULL, NULL, 1.3, 0, 0); }
        ST(o, hs, "flesh", -1, NULL, 0, DISCp(c, 1.3, 20));
    }
    *foot = OUT(o);
}

static const XrDef DEFS[] = {
    { "infestation", "growth", "egg", "egg_ledge", 10.5, 0, 1, egg_ledge },
    { "infestation", "growth", "egg", "burst_egg", 10.5, 0, 1, burst_egg },
    { "infestation", "made", "carapace", "molt", 10.5, 0, 1, molt },
    { "infestation", "growth", "web", "cocoon", 10.5, 0, 1, cocoon },
};
void bld_sdrx_infestation(void) { xr_register(DEFS, (int)(sizeof DEFS / sizeof DEFS[0])); }
