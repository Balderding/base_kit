/* bld_ns_thornwild.c: the 27 style-only details of sd_r_x_thornwild.py and its shared drawing (tw_wedge, tw_jaws,
   tw_leaves, tw_vine, tw_curl, tw_cap), copied routine by routine (each names the Python it copies). See bld_ns.h.
   (style_thornwild.py's _sd tier features look these up; see bld_ns_tiers.c.) */
#include "bld_ns.h"

#define BODY(nm) static int nm(double cx, double cy, double a, PyRandom *rng, NsOut *o, G **foot)
#define OUT_ ns_obj_out(o, ob, NULL, foot); return 0
static G *lnb(PTS *p, double d) { return buf(line(p), d, 16); }
typedef struct { double a, b; } Lin;          /* a - b * t */
static int lin_rf(void *c, double t, double *out) { const Lin *k = (const Lin *)c; *out = k->a - k->b * t; return 0; }
static void rodl(SoSolids *s, PTS *p, double a, double b, double hk) { Lin l = { a, b }; S_rodf(s, p, lin_rf, &l, hk, 0.0, 0.0); }
static PTS *pts2(double ax, double ay, double bx, double by) { PTS *p = pl_new(); pl_add(p, ax, ay); pl_add(p, bx, by); return p; }

/* tw_wedge(poly, hinge, run, z0, z1, n=6) */
typedef struct { G *poly, *hinge; double run, z0, z1; } Wedge;
static void wedge_free(void *c) { Wedge *w = (Wedge *)c; shp_free(w->poly); shp_free(w->hinge); free(w); }
static int wedge_sect(void *c, double z, G **out) {
    const Wedge *w = (const Wedge *)c; G *b, *g; int e;
    *out = NULL;
    if (z >= w->z1) return SO_OK;
    if (z <= w->z0) { *out = shp_clone(w->poly); return *out ? SO_OK : SO_ERR; }
    if (w->z1 - w->z0 == 0.0) return SO_ERR;
    b = shp_buffer_q(w->hinge, w->run * (z - w->z0) / (w->z1 - w->z0), 6); if (!b) return SO_ERR;
    g = shp_difference(w->poly, b); shp_free(b); if (!g) return SO_ERR;
    e = shp_is_empty(g); if (e < 0) { shp_free(g); return SO_ERR; }
    if (e) { shp_free(g); return SO_OK; }
    *out = g; return SO_OK;
}
static void tw_wedge(SoSolids *sol, const G *poly, const G *hinge, double run, double z0, double z1) {
    Wedge w; w.poly = shp_clone(poly); w.hinge = shp_clone(hinge); w.run = run; w.z0 = z0; w.z1 = z1;
    sl_custom_f(sol, wedge_sect, &w, sizeof w, z1, wedge_free);
}
/* tw_jaws(o, p, t, L, W, z0, z1, teeth=True, par=None) */
static int tw_jaws(SoObj *ob, double px, double py, double t, double L, double W, double z0, double z1, int teeth) {
    FF f = ff_(px, py, t); G *hinge; SoSolids *lobes = sl_new(), *s2 = sl_new(); int sv, i;
    hinge = line(pts2(FX(f, -L / 2, 0), FX(f, L / 2, 0)));
    for (sv = -1; sv <= 1; sv += 2) {
        double xy[8] = { FX(f, -L, 0.12 * sv), FX(f, L, 0.12 * sv), FX(f, L, W * 2 * sv), FX(f, -L, W * 2 * sv) };
        G *half = inter(ell(px, py, L / 2, W, t, 20), polyv(xy, 4));
        tw_wedge(lobes, half, hinge, W * 0.85, z0, z1);
    }
    i = ob_st(ob, lobes, "maw", -1, NULL, ell(px, py, L / 2, W, t, 20));
    S_rod(s2, pts2(FX(f, -L / 2 + 0.1, 0), FX(f, L / 2 - 0.1, 0)), 0.3, 1.2, 0.0, 0.0); S_cyl(s2, px, py, 0.55, z0 * 0.9, 0.0, 20); ob_st(ob, s2, "creeper", -1, NULL, NULL);
    if (teeth) {
        SoSolids *ts = sl_new(); int m = (int)ns_int(L / 0.62), k; if (m < 3) m = 3;
        for (sv = -1; sv <= 1; sv += 2) for (k = 0; k < m; k++) {
            double q = -0.82 + 1.64 * (k + 0.5) / m, x = q * L / 2, y = sv * W * sqrt(pmax(0.0, 1 - q * q)) * 0.98;
            S_cone(ts, FX(f, x, y), 0.3, z1 + 0.55, 0.07, 0.0, 8, 1.0, 0.0);
        }
        ob_st(ob, ts, "tooth", -1, NULL, ell(px, py, L / 2 + 0.3, W + 0.3, t, 16));
    }
    return i;
}
/* tw_leaves(o, p, rng, n, L0, L1, w=0.62, kind="creeper", t0=None) */
typedef struct { double w, L; } Leaf;
static int leaf_rf(void *c, double q, double *out) { const Leaf *k = (const Leaf *)c; int err = 0; double m = pmax(0.05, pmin(0.95, (q + k->L / 2) / k->L)), p = lf_fpow(m, 0.8, &err); if (err) return -1; *out = k->w * ns_sin(p * PI) + 0.05; return 0; }
static void tw_leaves(SoObj *ob, double px, double py, PyRandom *rng, int n, double L0, double L1, double w, const char *kind) {
    SoSolids *ps = sl_new(); GL *feet = gl_new(); int k; double t0 = U_(rng, 0, 6.28);
    for (k = 0; k < n; k++) {
        double t = t0 + 6.2832 * k / n + U_(rng, -0.25, 0.25), L = U_(rng, L0, L1), c0 = polx(px, L / 2 + 0.3, t), c1 = poly_(py, L / 2 + 0.3, t); Leaf lf = { w, L };
        S_spin(ps, c0, c1, t, L, leaf_rf, &lf, 0.55, 0.0, "ridge");
        gl_add(feet, disc(POL(px, py, L + 0.3, t), 0.3, 20));
    }
    gl_add(feet, disc(px, py, 0.6, 20));
    ob_st(ob, ps, kind, -1, NULL, hull(feet));
}
/* tw_vine(pts, r, rng, thorns=True, step=0.75, th=0.9) -> (rod solids, thorn solids), appended */
static void tw_vine(PTS *pts, double r, PyRandom *rng, double step, double th, SoSolids *ps, SoSolids *ts) {
    G *ln = line(pts); double L = len_(ln), us[256]; int n, k = 0, i;
    S_rod(ps, pts, r, 1.2, 0.0, 0.0);
    n = ns_arange(0.4, L - 0.3, step, us, 256);
    for (i = 0; i < n; i++) {
        G *pa = interp(ln, us[i]), *pb = interp(ln, pmin(L, us[i] + 0.05)); double t = ns_atan2(gy(pb) - gy(pa), gx(pb) - gx(pa)) + (k % 2 ? 1.5708 : -1.5708), q0 = polx(gx(pa), r * 0.75, t), q1 = poly_(gy(pa), r * 0.75, t), h;
        h = r * 1.2 + th * U_(rng, 0.8, 1.15); S_cone(ts, q0, q1, 0.32, h, 0.06, 0.0, 8, 1.0, 0.0); k++;
    }
}
/* tw_curl(p, t, L, bend, n=7) */
static PTS *tw_curl(double x, double y, double t, double L, double bend, int n) {
    PTS *p = pl_new(); int i;
    pl_add(p, x, y);
    for (i = 0; i < n; i++) { t += bend * (0.4 + 1.2 * i / n); x += L / n * ns_cos(t); y += L / n * ns_sin(t); pl_add(p, x, y); }
    return p;
}
/* tw_cap(x, y, r, h, stalk=0.45) */
static void tw_cap(SoSolids *s, double x, double y, double r, double h, double stalk) {
    double prof[18] = { stalk * 1.25, 0.0, stalk, h * 0.35, r, h * 0.62 };
    crv(r, 0.2, h * 0.62, h, 1, 5, prof + 6);
    S_turn(s, x, y, prof, 9, 14, 1.0, 0.0, 0.0);
}

typedef struct { double a, b, lo, hi; } Step;          /* a + b * (|q - 0.5| > hi) style: a + b * (cond) */
static int bone_rf(void *c, double q, double *out) { (void)c; *out = 0.32 + 0.12 * (fabs(q - 0.5) > 0.38); return 0; }
BODY(tw_maw_flower) {
    SoObj *ob = obj_new("maw"); double L = U_(rng, 3.0, 3.6), W = U_(rng, 1.25, 1.5); SoSolids *d = sl_new();
    { int n = (int)RI(rng, 5, 7); tw_leaves(ob, cx, cy, rng, n, 2.6, 3.4, 0.7, "creeper"); }
    S_dome(d, cx, cy, 1.2, 0.9, 0.0, 1.0, 0.0, 16); ob_st(ob, d, "creeper", -1, NULL, NULL);
    tw_jaws(ob, cx, cy, a, L, W, 0.85, 1.85, 1);
    if (RR(rng) < 0.5) {
        double t = a + U_(rng, 1.2, 1.9), b0x = polx(cx, W + 1.3, t), b0y = poly_(cy, W + 1.3, t), t2 = t + U_(rng, 1.2, 1.9), b1x = polx(b0x, 1.8, t2), b1y = poly_(b0y, 1.8, t2); SoSolids *s = sl_new();
        S_rodf(s, pts2(b0x, b0y, b1x, b1y), bone_rf, NULL, 1.1, 0.0, 0.0); ob_st(ob, s, "bone", -1, NULL, capl(b0x, b0y, b1x, b1y, 0.7));
    }
    OUT_;
}
BODY(tw_snap_traps) {
    SoObj *ob = obj_new("maw"); int n = (int)RI(rng, 3, 4), k;
    { int m = (int)RI(rng, 4, 6); tw_leaves(ob, cx, cy, rng, m, 1.4, 2.0, 0.45, "creeper"); }
    for (k = 0; k < n; k++) {
        double t = a + 6.2832 * k / n + U_(rng, -0.3, 0.3), d = U_(rng, 1.3, 1.7), px = polx(cx, d, t), py = poly_(cy, d, t), tt = t + U_(rng, -0.5, 0.5) + 1.5708, L = U_(rng, 1.5, 1.8), W = U_(rng, 0.62, 0.75);
        tw_jaws(ob, px, py, tt, L, W, 0.45, 1.0, 1);
    }
    OUT_;
}
BODY(tw_pitcher_cluster) {
    SoObj *ob = obj_new("pitcher"); int n = (int)RI(rng, 2, 4), k, j, nf = 0; double fx_[4], fy_[4];
    { int m = (int)RI(rng, 3, 5); tw_leaves(ob, cx, cy, rng, m, 1.6, 2.3, 0.5, "creeper"); }
    for (k = 0; k < n; k++) {
        double px = cx, py = cy, r, h; int hit = 0; SoSolids *s; NsCuts cu = { 0 };
        if (k) { double d = U_(rng, 1.5, 1.9), t = a + 6.2832 * k / (n - 1 > 1 ? n - 1 : 1); px = polx(cx, d, t); py = poly_(cy, d, t); }
        r = U_(rng, 0.75, 0.95) * (k == 0 ? 1.0 : 0.85); h = U_(rng, 2.2, 3.0) * (k == 0 ? 1.0 : 0.8);
        for (j = 0; j < nf && !hit; j++) hit = ns_mdist(px, py, fx_[j], fy_[j]) < r + 0.9;
        if (hit) continue;
        fx_[nf] = px; fy_[nf] = py; nf++;
        { double prof[12] = { r * 1.1, 0.0, r * 1.2, h * 0.25, r, h * 0.6, r * 0.82, h * 0.82, r * 1.08, h * 0.95, r * 1.08, h }; s = sl_new(); S_turn(s, px, py, prof, 6, 18, 1.0, 0.0, 0.0); }
        cut_add(&cu, disc(px, py, r * 0.6, 14), h * 0.55); cut_add(&cu, ann(px, py, r * 0.98, r * 0.82, 18), h * 0.93);
        ob_st(ob, s, NULL, -1, &cu, disc(px, py, r * 1.2, 20));
    }
    OUT_;
}
static int const_rf(void *c, double q, double *out) { (void)q; *out = *(const double *)c; return 0; }
BODY(tw_sundew_rosette) {
    SoObj *ob = obj_new("sundew"); int n = (int)RI(rng, 5, 7), k, j; SoSolids *pads = sl_new(), *ts = sl_new(); GL *feet = gl_new();
    for (k = 0; k < n; k++) {
        double t = a + 6.2832 * k / n + U_(rng, -0.2, 0.2), L = U_(rng, 1.6, 2.2), c0 = polx(cx, L, t), c1 = poly_(cy, L, t), rr; int m;
        S_rod(pads, pts2(cx, cy, c0, c1), 0.2, 1.0, 0.0, 0.0); rr = U_(rng, 0.65, 0.8); S_ext(pads, disc(c0, c1, rr, 12), 0.3, 0.0); gl_add(feet, disc(c0, c1, 0.85, 20));
        m = (int)RI(rng, 3, 4);
        for (j = 0; j < m; j++) { double d = U_(rng, 0.15, 0.5), tt = U_(rng, 0, 6.28), q0 = polx(c0, d, tt), q1 = poly_(c1, d, tt), h = U_(rng, 0.65, 0.85); S_cone(ts, q0, q1, 0.25, h, 0.12, 0.0, 6, 1.0, 0.0); }
    }
    S_dome(pads, cx, cy, 0.5, 0.45, 0.0, 1.0, 0.0, 10); ob_st(ob, pads, "creeper", -1, NULL, hull(feet));
    ob_st(ob, ts, "sundew", -1, NULL, NULL);
    if (RR(rng) < 0.5) { double t = a + U_(rng, 0, 6.28), d = U_(rng, 0.9, 1.3), q0 = polx(cx, d, t), q1 = poly_(cy, d, t), c = 0.42; SoSolids *s = sl_new(); S_spin(s, q0, q1, t, 1.3, const_rf, &c, 1.2, 0.0, "round"); ob_st(ob, s, "chitin", -1, NULL, disc(q0, q1, 0.7, 20)); }
    OUT_;
}
BODY(tw_spore_pods) {
    SoObj *ob = obj_new("sporepod"); int n = (int)RI(rng, 3, 5), k, j, nf = 0; double fpx[8], fpy[8], fr[8]; SoSolids *ps = sl_new(); NsCuts cu = { 0 }; GL *fl = gl_new();
    for (k = 0; k < n * 4; k++) {
        double r, px = cx, py = cy, h; int hit = 0;
        if (nf >= n) break;
        r = U_(rng, 0.8, 1.15);
        if (nf) { double d = U_(rng, 1.4, 2.4), t = U_(rng, 0, 6.28); px = polx(cx, d, t); py = poly_(cy, d, t); }
        for (j = 0; j < nf && !hit; j++) hit = ns_mdist(px, py, fpx[j], fpy[j]) < r + fr[j] + 0.2;
        if (hit) continue;
        h = r * U_(rng, 1.5, 1.9); fpx[nf] = px; fpy[nf] = py; fr[nf] = r; nf++;
        { double prof[10] = { r * 0.85, 0.0, r, h * 0.35, r * 0.9, h * 0.62, r * 0.5, h * 0.86, r * 0.25, h }; S_turn(ps, px, py, prof, 5, 16, 1.0, 0.0, 0.0); }
        cut_add(&cu, disc(px, py, 0.24, 8), h * 0.72); cut_add(&cu, ann(px, py, r * 0.6, r * 0.44, 14), h * 0.8);
    }
    for (j = 0; j < nf; j++) gl_add(fl, disc(fpx[j], fpy[j], fr[j], 20));
    ob_st(ob, ps, NULL, -1, &cu, hull(fl));
    if (nf > 1) { SoSolids *rs = sl_new(); for (j = 0; j + 1 < nf; j++) S_rod(rs, wiggle(fpx[j], fpy[j], fpx[j + 1], fpy[j + 1], rng, 0.3, 3), 0.32, 1.1, 0.0, 0.0); ob_st(ob, rs, "root", -1, NULL, NULL); }
    OUT_;
}
static int petal_rf(void *c, double q, double *out) { (void)c; *out = 0.55 * ns_sin(pmax(0.05, pmin(0.95, (q + 0.95) / 1.9)) * PI) + 0.08; return 0; }
BODY(tw_burst_pod) {
    SoObj *ob = obj_new("sporepod"); double r = U_(rng, 1.1, 1.35), h; int n, k; SoSolids *body = sl_new(), *sp = sl_new(); NsCuts cu = { 0 };
    h = r * U_(rng, 1.0, 1.2); n = (int)RI(rng, 4, 5);
    { double prof[6] = { r * 0.9, 0.0, r, h * 0.5, r * 0.95, h }; S_turn(body, cx, cy, prof, 3, 18, 1.0, 0.0, 0.0); }
    for (k = 0; k < n; k++) { double t = a + 6.2832 * k / n + U_(rng, -0.2, 0.2), c0 = polx(cx, r + 0.75, t), c1 = poly_(cy, r + 0.75, t); S_spin(body, c0, c1, t, 1.9, petal_rf, NULL, 0.6, 0.0, "ridge"); }
    cut_add(&cu, disc(cx, cy, r * 0.62, 14), h * 0.3); ob_st(ob, body, NULL, -1, &cu, disc(cx, cy, r + 1.7, 20));
    n = (int)RI(rng, 6, 9);
    for (k = 0; k < n; k++) { double d = U_(rng, r + 0.9, r + 2.3), t = U_(rng, 0, 6.28), q0 = polx(cx, d, t), q1 = poly_(cy, d, t), rr = U_(rng, 0.28, 0.38); S_dome(sp, q0, q1, rr, 0.4, 0.0, 1.0, 0.0, 8); }
    ob_st(ob, sp, "spore", -1, NULL, disc(cx, cy, r + 2.5, 20));
    OUT_;
}
BODY(tw_spore_stalks) {
    SoObj *ob = obj_new("spore"); int n = (int)RI(rng, 3, 6), k, j, nf = 0; double fpx[8], fpy[8], fr[8]; SoSolids *ps = sl_new(), *st = sl_new(); NsCuts cu = { 0 }; GL *fl = gl_new();
    for (k = 0; k < n * 4; k++) {
        double px = cx, py = cy, r, h; int hit = 0;
        if (nf >= n) break;
        if (nf) { double d = U_(rng, 1.0, 2.0), t = U_(rng, 0, 6.28); px = polx(cx, d, t); py = poly_(cy, d, t); }
        r = U_(rng, 0.5, 0.7);
        for (j = 0; j < nf && !hit; j++) hit = ns_mdist(px, py, fpx[j], fpy[j]) < r + fr[j] + 0.15;
        if (hit) continue;
        fpx[nf] = px; fpy[nf] = py; fr[nf] = r; nf++; h = U_(rng, 1.7, 2.8);
        S_cone(st, px, py, r * 1.15, h * 0.7, r * 0.6, 0.0, 12, 1.0, 0.0);
        { double prof[14] = { r * 0.7, h * 0.55, r, h * 0.75 }; crv(r, 0.2, h * 0.75, h, 1, 4, prof + 4); S_turn(ps, px, py, prof, 7, 14, 1.0, 0.0, 0.0); }
        { double t = U_(rng, 0, 6.28); cut_add(&cu, disc(POL(px, py, r * 0.45, t), 0.13, 6), h * 0.85); }
    }
    for (j = 0; j < nf; j++) gl_add(fl, disc(fpx[j], fpy[j], fr[j] * 1.15, 20));
    ob_st(ob, st, "sporepod", -1, NULL, hull(fl));
    ob_st(ob, ps, NULL, -1, &cu, NULL);
    OUT_;
}
BODY(tw_seed_burrs) {
    SoSolids *ps = sl_new(); int k, j, n = (int)RI(rng, 3, 6), nf = 0; double fpx[8], fpy[8]; GL *fl = gl_new();
    for (k = 0; k < n; k++) {
        double d = k ? U_(rng, 0.9, 2.0) : 0.0, t = U_(rng, 0, 6.28), px = polx(cx, d, t), py = poly_(cy, d, t), r = U_(rng, 0.45, 0.6); int hit = 0;
        for (j = 0; j < nf && !hit; j++) hit = ns_mdist(px, py, fpx[j], fpy[j]) < r + 0.95;
        if (hit) continue;
        fpx[nf] = px; fpy[nf] = py; nf++; S_dome(ps, px, py, r, r * 1.2, 0.0, 1.0, 0.0, 10);
        for (j = 0; j < 6; j++) { double tt = U_(rng, 0, 0.3) + j * 1.0472, q0 = polx(px, r * 0.85, tt), q1 = poly_(py, r * 0.85, tt); S_cone(ps, q0, q1, 0.26, r * 1.25 + 0.35, 0.06, 0.0, 6, 1.0, 0.0); }
    }
    for (j = 0; j < nf; j++) gl_add(fl, disc(fpx[j], fpy[j], 0.9, 20));
    *foot = hull(fl); ns_st(o, ps, "seedburr", 0, 0, NULL); return 0;
}
BODY(tw_glowcap_ring) {
    double R = U_(rng, 2.2, 2.8); int n = (int)RI(rng, 7, 10), k; SoSolids *ps = sl_new(), *small = sl_new(); NsCuts cu = { 0 };
    for (k = 0; k < n; k++) {
        double t, d, px, py, r, h;
        if (RR(rng) < 0.12) continue;
        t = a + 6.2832 * k / n + U_(rng, -0.15, 0.15); d = R + U_(rng, -0.2, 0.2); px = polx(cx, d, t); py = poly_(cy, d, t); r = U_(rng, 0.5, 0.75); h = U_(rng, 0.9, 1.4);
        tw_cap(ps, px, py, r, h, r * 0.55);
        if (r > 0.68) cut_add(&cu, ann(px, py, r * 0.8, r * 0.62, 12), h * 0.72);
        if (RR(rng) < 0.5) { double d2 = R + U_(rng, -0.6, 0.6); S_dome(small, POL(cx, cy, d2, t + 3.1416 / n), 0.3, 0.42, 0.0, 1.0, 0.0, 8); }
    }
    *foot = ann(cx, cy, R + 1.0, pmax(0.2, R - 1.0), 24);
    ns_st(o, ps, "glowcap", 0, 0, &cu); ns_st(o, small, "glowcap", 0, 0, NULL); return 0;
}
BODY(tw_glow_stalks) {
    SoSolids *ps = sl_new(); NsCuts cu = { 0 }; int k, j, nf = 0, n; double fpx[8], fpy[8], fr[8]; GL *fl = gl_new();
    n = (int)RI(rng, 3, 5) * 3;
    for (k = 0; k < n; k++) {
        double px = cx, py = cy, r, h; int hit = 0;
        if (nf >= 5) break;
        if (nf) { double d = U_(rng, 2.0, 2.8), t = U_(rng, 0, 6.28); px = polx(cx, d, t); py = poly_(cy, d, t); }
        r = nf ? U_(rng, 0.6, 0.9) : U_(rng, 0.95, 1.3);
        for (j = 0; j < nf && !hit; j++) hit = ns_mdist(px, py, fpx[j], fpy[j]) < r + fr[j] + 0.1;
        if (hit) continue;
        h = nf ? U_(rng, 1.3, 2.2) : U_(rng, 2.4, 3.2); fpx[nf] = px; fpy[nf] = py; fr[nf] = r; nf++;
        tw_cap(ps, px, py, r, h, pmax(0.45, r * 0.45));
        cut_add(&cu, ann(px, py, r * 0.82, r * 0.66, 14), h * 0.74);
    }
    for (j = 0; j < nf; j++) gl_add(fl, disc(fpx[j], fpy[j], fr[j], 20));
    *foot = hull(fl); ns_st(o, ps, "glowcap", 0, 0, &cu); return 0;
}
typedef struct { double r; } RootR;
static int rootr_rf(void *c, double q, double *out) { *out = ((const RootR *)c)->r * (1.05 - 0.3 * q); return 0; }
BODY(tw_shelf_fungus_root) {
    FF f = ff_(cx, cy, a); double L = U_(rng, 4.6, 5.6), r = U_(rng, 0.75, 0.9), ss[6], sv; SoObj *ob = obj_new("rootbark"); PTS *pts = pl_new(); int i, n; SoSolids *s = sl_new(), *br = sl_new(); NsCuts cu = { 0 }; RootR rr = { 0 };
    ns_linspace(0, 1, 6, ss);
    for (i = 0; i < 6; i++) { double v = 0.3 * ns_sin(4 * ss[i] + U_(rng, 0, 1)); pl_add(pts, FX(f, -L / 2 + L * ss[i], v)); }
    rr.r = r; S_rodf(s, pts, rootr_rf, &rr, 1.3, 0.0, 0.0);
    cut_add(&cu, barl(FX(f, -L / 2, -0.3), FX(f, L / 2, -0.3), 0.14), r * 1.15); cut_add(&cu, barl(FX(f, -L / 2, 0.3), FX(f, L / 2, 0.3), 0.14), r * 1.15);
    ob_st(ob, s, NULL, -1, &cu, lnb(pts, r));
    sv = CH(rng, -1, 1); n = (int)RI(rng, 3, 5);
    for (i = 0; i < n; i++) {
        double x = -L * 0.35 + L * 0.7 * i / 4 + U_(rng, -0.2, 0.2), c0 = fx(&f, x, sv * (r + 0.15)), c1 = fy(&f, x, sv * (r + 0.15)), q = U_(rng, 0.55, 0.8), h; G *half = dif(ell(c0, c1, q, q * 0.7, a, 20), lnb(pts, r * 0.6));
        h = r * 1.3 + U_(rng, 0.0, 0.3); S_ext(br, half, h, 0.0);
    }
    ob_st(ob, br, "glowcap", -1, NULL, NULL);
    OUT_;
}
typedef struct { double r0; } Knot;
static int knot_rf(void *c, double s, double *out) { *out = ((const Knot *)c)->r0 * (0.55 + 0.45 * ns_sin(pmax(0.0, pmin(1.0, s)) * PI)) + 0.15; return 0; }
BODY(tw_root_knot) {
    SoSolids *ps = sl_new(); GL *feet = gl_new(); int k, n = (int)RI(rng, 3, 4);
    for (k = 0; k < n; k++) {
        double t = a + k * U_(rng, 0.6, 1.0), L = U_(rng, 5.0, 6.5), r0; PTS *pts = wiggle(POL(cx, cy, L / 2, t + 3.1416), POL(cx, cy, L / 2, t), rng, 0.6, 5); Knot kn;
        r0 = U_(rng, 0.7, 0.95); kn.r0 = r0;
        S_rodf(ps, pts, knot_rf, &kn, 1.5, 0.0, 0.0); gl_add(feet, lnb(pts, r0));
    }
    { double t = a + U_(rng, 0, 6.28), d = U_(rng, 2.4, 3.0), ex = polx(cx, d, t), ey = poly_(cy, d, t), ch = CH(rng, -1, 1); rodl(ps, tw_curl(ex, ey, t, 2.2, ch * 0.5, 6), 0.45, 0.2, 1.2); }
    *foot = hull(feet); ns_st(o, ps, "root", 0, 0, NULL); return 0;
}
typedef struct { double W; } Butt;
static int butt_rf(void *c, double s, double *out) { *out = ((const Butt *)c)->W * (1.0 - 0.6 * s) + 0.1; return 0; }
BODY(tw_buttress_root) {
    FF f = ff_(cx, cy, a); double L = U_(rng, 6.5, 8.0), W = U_(rng, 1.0, 1.3), ss[9], vs[2] = { -0.35, 0.35 }; PTS *pts = pl_new(); SoSolids *ps = sl_new(); GL *feet = gl_new(); int i, n; Butt bt; NsCuts cu = { 0 };
    ns_linspace(0, 1, 9, ss); for (i = 0; i < 9; i++) pl_add(pts, FX(f, -L / 2 + L * ss[i], 0.9 * ns_sin(2.2 * ss[i])));
    bt.W = W; S_rodf(ps, pts, butt_rf, &bt, 2.4, 0.0, 0.0); gl_add(feet, lnb(pts, W + 0.3));
    n = (int)RI(rng, 2, 3);
    for (i = 0; i < n; i++) {
        int j = (int)RI(rng, 1, 5); double px = pts->v[2 * j], py = pts->v[2 * j + 1], ch = CH(rng, -1, 1), t = a + ch * U_(rng, 0.9, 1.6), L2 = U_(rng, 1.8, 2.6), b = U_(rng, -0.4, 0.4); PTS *sp = tw_curl(px, py, t, L2, b, 4);
        rodl(ps, sp, 0.5, 0.2, 1.2); gl_add(feet, lnb(sp, 0.6));
    }
    for (i = 0; i < 2; i++) cut_add(&cu, lnb(pts2(FX(f, -L / 2, vs[i]), FX(f, L / 2, vs[i] + 0.2)), 0.08), W * 1.6);
    *foot = hull(feet); ns_st(o, ps, "root", 0, 0, &cu); return 0;
}
BODY(tw_thorn_vine) {
    double px = polx(cx, 3.2, a + 3.1416), py = poly_(cy, 3.2, a + 3.1416), t0 = a + U_(rng, -0.3, 0.3), L = U_(rng, 6.0, 7.0), ch = CH(rng, -1, 1), b = ch * U_(rng, 0.15, 0.3), r; PTS *pts = tw_curl(px, py, t0, L, b, 9); SoSolids *ps = sl_new(), *ts = sl_new(); int k, n;
    r = U_(rng, 0.38, 0.45); tw_vine(pts, r, rng, 0.75, 0.9, ps, ts);
    n = (int)RI(rng, 1, 2);
    for (k = 0; k < n; k++) {
        int j = (int)RI(rng, 2, 6); double qx = pts->v[2 * j], qy = pts->v[2 * j + 1], c2 = CH(rng, -1, 1), t = a + c2 * U_(rng, 0.8, 1.3), L2 = U_(rng, 1.8, 2.4), bb = U_(rng, -0.6, 0.6); PTS *sh = tw_curl(qx, qy, t, L2, bb, 4);
        tw_vine(sh, 0.32, rng, 0.8, 0.7, ps, ts);
    }
    *foot = lnb(pts, 1.6); ns_st(o, ps, "creeper", 0, 0, NULL); ns_st(o, ts, "spikethorn", 0, 0, NULL); return 0;
}
BODY(tw_thorn_bramble) {
    SoSolids *ps = sl_new(), *ts = sl_new(); GL *feet = gl_new(); int k, n;
    S_dome(ps, cx, cy, 0.9, 0.9, 0.0, 1.0, 0.0, 12); gl_add(feet, disc(cx, cy, 1.0, 20));
    n = (int)RI(rng, 5, 6);
    for (k = 0; k < n; k++) {
        double t = a + k * 1.15 + U_(rng, -0.3, 0.3), L = U_(rng, 2.6, 3.6), ch = CH(rng, -1, 1), b = ch * U_(rng, 0.3, 0.6); PTS *pts = tw_curl(cx, cy, t, L, b, 5);
        tw_vine(pts, 0.36, rng, 0.65, 0.8, ps, ts); gl_add(feet, lnb(pts, 0.6));
    }
    *foot = hull(feet); ns_st(o, ps, "creeper", 0, 0, NULL); ns_st(o, ts, "spikethorn", 0, 0, NULL); return 0;
}
BODY(tw_strangler_coil) {
    double R = U_(rng, 1.4, 1.8), hr = U_(rng, 1.5, 2.0), xy[50], r = 0.36; SoObj *ob = obj_new("rock"); SoSolids *rk = sl_new(), *ps = sl_new(), *ts = sl_new(); int i; PTS *tail;
    S_rock(rk, cx, cy, R, hr, rng, 0.9, 0.0, 9); ob_st(ob, rk, NULL, -1, NULL, NULL);
    if (sd_spiral(cx, cy, R + 0.15, R * 0.35, 1.4, a, 24, xy) != SO_OK) ns_raise();
    for (i = 0; i < 24; i++) {
        double d0 = ns_mdist(xy[2 * i], xy[2 * i + 1], cx, cy), z = hr * (1 - ns_pow(d0 / (R + 0.2), 2.0)) * 0.95;
        S_rod(ps, pts2(xy[2 * i], xy[2 * i + 1], xy[2 * i + 2], xy[2 * i + 3]), r, 1.2, pmax(0.0, z - r * 0.6), 0.0);
        if (i % 3 == 1) S_cone(ts, xy[2 * i], xy[2 * i + 1], 0.3, z + r * 1.2 + 0.6, 0.06, 0.0, 6, 1.0, 0.0);
    }
    tail = tw_curl(xy[0], xy[1], a - 1.5708, 2.2, 0.3, 4); tw_vine(tail, 0.36, rng, 0.75, 0.9, ps, ts);
    ob_st(ob, ps, "creeper", -1, NULL, lnb(tail, 0.5)); ob_st(ob, ts, "spikethorn", -1, NULL, NULL);
    OUT_;
}
BODY(tw_grasping_tendrils) {
    double r = U_(rng, 0.9, 1.2); SoSolids *ps = sl_new(); NsCuts cu = { 0 }; GL *feet = gl_new(); int k, n;
    gl_add(feet, disc(cx, cy, r + 0.5, 20));
    n = (int)RI(rng, 4, 6);
    for (k = 0; k < n; k++) {
        double t = a + 6.2832 * k / 5 + U_(rng, -0.3, 0.3), px = polx(cx, r * 0.8, t), py = poly_(cy, r * 0.8, t), L = U_(rng, 2.6, 3.6), ch = CH(rng, -1, 1), b = ch * U_(rng, 0.35, 0.6); PTS *pts = tw_curl(px, py, t, L, b, 7);
        rodl(ps, pts, 0.55, 0.28, 1.2); gl_add(feet, lnb(pts, 0.5));
        cut_add(&cu, disc(pts->v[4], pts->v[5], 0.12, 6), 0.42); cut_add(&cu, disc(pts->v[8], pts->v[9], 0.12, 6), 0.42);
    }
    *foot = hull(feet);
    ns_gcut(o, disc(cx, cy, r, 14), 0.6, "pit"); ns_st(o, ps, "creeper", 0, 0, &cu); return 0;
}
BODY(tw_rooted_ribcage) {
    FF f = ff_(cx, cy, a); double L = U_(rng, 3.4, 4.0), s0, s1; SoObj *ob = obj_new("bone"); SoSolids *sp = sl_new(), *rt = sl_new(), *d = sl_new(); int k, sv, n; NsCuts cu = { 0 };
    S_rod(sp, pts2(FX(f, -L / 2, 0), FX(f, L / 2, 0)), 0.32, 1.0, 0.0, 0.0);
    for (k = 0; k < 5; k++) {
        double x = -L * 0.35 + k * L * 0.16;
        for (sv = -1; sv <= 1; sv += 2) { PTS *p = pl_new(); pl_add(p, FX(f, x, 0)); pl_add(p, FX(f, x + 0.45, sv * 1.0)); pl_add(p, FX(f, x + 0.25, sv * 1.65)); rodl(sp, p, 0.36, 0.06, 1.3); }
    }
    ob_st(ob, sp, NULL, -1, NULL, rect(cx, cy, L + 0.4, 3.8, a));
    { double u = L / 2 + U_(rng, 0.9, 1.3), ch = CH(rng, -1, 1), v = ch * U_(rng, 0.4, 1.0); s0 = fx(&f, u, v); s1 = fy(&f, u, v); }
    S_dome(d, s0, s1, 0.78, 1.0, 0.0, 0.85, a, 14);
    for (sv = -1; sv <= 1; sv += 2) cut_add(&cu, disc(POL(s0, s1, 0.35, a + sv * 0.5), 0.2, 8), 0.55);
    ob_st(ob, d, NULL, -1, &cu, disc(s0, s1, 0.8, 20));
    n = (int)RI(rng, 2, 3);
    for (k = 0; k < n; k++) { double u1 = U_(rng, -L / 2, L / 2), px = fx(&f, u1, -2.4), py = fy(&f, u1, -2.4), u2 = U_(rng, -L / 2, L / 2); S_rod(rt, wiggle(px, py, FX(f, u2, 2.4), rng, 0.4, 4), 0.36, 1.5, 0.0, 0.0); }
    ob_st(ob, rt, "root", -1, NULL, NULL);
    OUT_;
}
BODY(tw_lost_helmet) {
    FF f = ff_(cx, cy, a); double r = U_(rng, 1.1, 1.3); SoObj *ob = obj_new("metal"); SoSolids *s = sl_new(), *ps = sl_new(), *ts = sl_new(); NsCuts cu = { 0 }; PTS *pts;
    S_dome(s, cx, cy, r, r * 1.05, 0.0, 0.88, a, 18); S_ext(s, ell(FX(f, r * 0.55, 0), r * 0.5, r * 0.95, a, 12), 0.4, 0.0);
    cut_add(&cu, rect(FX(f, r * 0.55, 0), 0.3, r * 1.2, a), r * 0.55); cut_add(&cu, disc(FX(f, -r * 0.3, r * 0.3), 0.3, 8), r * 0.8);
    ob_st(ob, s, NULL, -1, &cu, NULL);
    pts = tw_curl(FX(f, r * 1.6, -r), a + 2.2, 4.0, 0.4, 7); tw_vine(pts, 0.34, rng, 0.9, 0.6, ps, ts);
    ob_st(ob, ps, "creeper", -1, NULL, lnb(pts, 0.5)); ob_st(ob, ts, "spikethorn", -1, NULL, NULL);
    OUT_;
}
BODY(tw_dropped_pack) {
    FF f = ff_(cx, cy, a); double L = U_(rng, 2.6, 3.0), W = U_(rng, 1.9, 2.2), h, xs[2]; SoObj *ob = obj_new("leather"); int b, sv; SoSolids *s = sl_new(), *ps = sl_new(), *ts = sl_new(); PTS *pts;
    { G *g = rrect(cx, cy, L, W, a, 0.5); h = U_(rng, 0.9, 1.1); b = ob_up(ob, g, h, NULL); }
    ob_lid(ob, rrect(FX(f, L * 0.22, 0), L * 0.5, W, a, 0.45), b, 0.18, 0.16, NULL);
    for (sv = -1; sv <= 1; sv += 2) { G *g = rrect(FX(f, -L * 0.1, sv * (W / 2 + 0.3)), L * 0.42, 0.6, a, 0.22); h = U_(rng, 0.6, 0.75); ob_up(ob, g, h, NULL); }
    xs[0] = -L * 0.25; xs[1] = L * 0.12; ob_straps(ob, &f, xs, 2, W, b, a, 0.32, 0.12, "leather");
    S_lie(s, FX(f, -L / 2 - 0.4, 0), a + 1.5708, W + 0.4, 0.55, 0.0); ob_st(ob, s, "cloth", -1, NULL, rect(FX(f, -L / 2 - 0.4, 0), 1.2, W + 0.4, a));
    ob_cut(ob, barl(FX(f, L * 0.05, -W / 2 + 0.25), FX(f, L * 0.35, -W / 2 + 0.45), 0.24), b, 0.3);
    pts = tw_curl(FX(f, L / 2 + 1.2, W / 2), a + 3.6, 3.6, 0.45, 6); tw_vine(pts, 0.3, rng, 1.0, 0.5, ps, ts);
    ob_st(ob, ps, "creeper", -1, NULL, lnb(pts, 0.4)); ob_st(ob, ts, "spikethorn", -1, NULL, NULL);
    OUT_;
}
BODY(tw_overgrown_rifle) {
    FF f = ff_(cx, cy, a); double L = U_(rng, 5.6, 6.4), xs[10]; SoObj *ob = obj_new("metal"); int bd, i; SoSolids *s = sl_new(), *s2 = sl_new(), *ps = sl_new(), *ts = sl_new(); PTS *pts = pl_new();
    { double xy[8] = { FX(f, -L / 2, -0.55), FX(f, -L / 2 + 1.5, -0.35), FX(f, -L / 2 + 1.5, 0.35), FX(f, -L / 2, 0.55) }; ob_up(ob, polyv(xy, 4), 0.6, "wood"); }
    bd = ob_up(ob, rect(FX(f, -L / 2 + 2.3, 0), 1.7, 0.8, a), 0.75, NULL);
    ob_on(ob, rect(FX(f, -L / 2 + 2.4, 0), 0.8, 0.5, a), bd, 0.3, NULL);
    ob_up(ob, rect(FX(f, -L / 2 + 2.0, -0.6), 0.6, 0.5, a), 0.35, NULL);
    S_rod(s, pts2(FX(f, -L / 2 + 3.1, 0), FX(f, L / 2, 0)), 0.27, 1.6, 0.0, 0.0); ob_st(ob, s, NULL, -1, NULL, rect(FX(f, 0.9, 0), L - 3.1, 0.55, a));
    S_rod(s2, wiggle(FX(f, -L / 2 + 0.4, 0.5), FX(f, -L / 2 + 3.4, 0.5), rng, 0.5, 4), 0.25, 0.8, 0.0, 0.0); ob_st(ob, s2, "leather", -1, NULL, NULL);
    ns_linspace(-0.2, L / 2 - 0.4, 10, xs); for (i = 0; i < 10; i++) pl_add(pts, FX(f, xs[i], 0.9 * ns_sin(xs[i] * 2.4)));
    tw_vine(pts, 0.3, rng, 1.1, 0.5, ps, ts);
    ob_st(ob, ps, "creeper", -1, NULL, lnb(pts, 0.4)); ob_st(ob, ts, "spikethorn", -1, NULL, NULL);
    OUT_;
}
BODY(tw_machete) {
    FF f = ff_(cx, cy, a); double L = U_(rng, 3.6, 4.2); SoObj *ob = obj_new("metal"); int g, k, n; GL *l = gl_new(); SoSolids *ps = sl_new(); PTS *bp = pl_new();
    pl_add(bp, FX(f, -L / 2 + 1.2, -0.3)); pl_add(bp, FX(f, L / 2 - 0.6, -0.42)); pl_add(bp, FX(f, L / 2, 0.1)); pl_add(bp, FX(f, L / 2 - 0.3, 0.55)); pl_add(bp, FX(f, -L / 2 + 1.2, 0.3));
    ob_up(ob, poly(bp), 0.3, NULL);
    ob_up(ob, rect(FX(f, -L / 2 + 1.15, 0), 0.3, 1.0, a), 0.45, NULL);
    g = ob_up(ob, rect(FX(f, -L / 2 + 0.5, 0), 1.0, 0.5, a), 0.5, "leather");
    for (k = 0; k < 3; k++) gl_add(l, barl(FX(f, -L / 2 + 0.2 + k * 0.3, -0.4), FX(f, -L / 2 + 0.3 + k * 0.3, 0.4), 0.14));
    ob_cut(ob, U(l), g, 0.12);
    n = (int)RI(rng, 2, 3);
    for (k = 0; k < n; k++) { double u = U_(rng, -L / 2, L / 2), ch = CH(rng, -1, 1), v = ch * U_(rng, 1.0, 1.5), p0 = fx(&f, u, v), p1 = fy(&f, u, v), t = a + U_(rng, -0.6, 0.6); S_rod(ps, pts2(POL(p0, p1, -0.7, t), POL(p0, p1, 0.7, t)), 0.3, 1.2, 0.0, 0.0); }
    ob_st(ob, ps, "creeper", -1, NULL, rect(cx, cy, L, 3.2, a));
    OUT_;
}
BODY(tw_field_lantern) {
    FF f = ff_(cx, cy, a); double L = U_(rng, 2.0, 2.4), r = U_(rng, 0.62, 0.72), xs[3]; SoObj *ob = obj_new("metal"); SoSolids *s1 = sl_new(), *s2 = sl_new(), *s3 = sl_new(), *sp = sl_new(); NsCuts cu = { 0 }; GL *l = gl_new(); int i, n; PTS *p = pl_new();
    S_lie(s1, cx, cy, a, L, r, 0.0); xs[0] = -L * 0.22; xs[1] = 0.0; xs[2] = L * 0.22;
    for (i = 0; i < 3; i++) gl_add(l, barl(FX(f, xs[i], -r), FX(f, xs[i], r), 0.2));
    cut_add(&cu, U(l), r * 1.6); ob_st(ob, s1, NULL, -1, &cu, rect(cx, cy, L, 2 * r, a));
    S_lie(s2, FX(f, -L / 2 - 0.2, 0), a, 0.4, r * 1.05, 0.0); ob_st(ob, s2, NULL, -1, NULL, rect(FX(f, -L / 2 - 0.2, 0), 0.4, 2 * r, a));
    pl_add(p, FX(f, -L / 2 - 0.4, -0.45)); pl_add(p, FX(f, -L / 2 - 1.0, 0)); pl_add(p, FX(f, -L / 2 - 0.4, 0.45)); S_rod(s3, p, 0.2, 1.0, 0.0, 0.0); ob_st(ob, s3, NULL, -1, NULL, NULL);
    n = (int)RI(rng, 3, 5);
    for (i = 0; i < n; i++) { double u = U_(rng, -L / 2, L / 2 + 0.8), ch = CH(rng, -1, 1), v = ch * U_(rng, r + 0.5, r + 1.2); S_dome(sp, FX(f, u, v), 0.28, 0.35, 0.0, 1.0, 0.0, 8); }
    ob_st(ob, sp, "glowcap", -1, NULL, rect(cx, cy, L + 1.2, 2 * r + 2.4, a));
    OUT_;
}
static int bootbone_rf(void *c, double q, double *out) { (void)c; *out = 0.36 + 0.1 * (q > 0.85); return 0; }
BODY(tw_boot_and_bones) {
    FF f = ff_(cx, cy, a); SoObj *ob = obj_new("leather"); int b, i; GL *l = gl_new(); SoSolids *bn = sl_new(), *cl = sl_new(); PTS *sp = pl_new(); double xs[3] = { 0.2, 0.6, 1.0 };
    pl_add(sp, FX(f, -1.2, -0.55)); pl_add(sp, FX(f, 1.0, -0.6)); pl_add(sp, FX(f, 1.5, -0.3)); pl_add(sp, FX(f, 1.55, 0.25)); pl_add(sp, FX(f, 1.0, 0.55)); pl_add(sp, FX(f, -1.2, 0.5));
    b = ob_up(ob, poly(sp), 0.85, NULL);
    ob_on(ob, rrect(FX(f, -0.75, 0), 0.9, 1.2, a, 0.3), b, 0.55, NULL);
    for (i = 0; i < 3; i++) gl_add(l, barl(FX(f, xs[i], -0.8), FX(f, xs[i], 0.8), 0.18));
    ob_cut(ob, U(l), b, 0.14);
    { double v = U_(rng, -0.4, 0.4); S_rodf(bn, pts2(FX(f, -1.0, 0.0), FX(f, -3.2, v)), bootbone_rf, NULL, 1.0, 0.0, 0.0); }
    ob_st(ob, bn, "bone", -1, NULL, capl(FX(f, -1.0, 0), FX(f, -3.2, 0), 0.8));
    S_rod(cl, wiggle(FX(f, 0.2, 0.6), FX(f, 0.6, 1.8), rng, 0.3, 3), 0.2, 0.9, 0.0, 0.0); ob_st(ob, cl, "cloth", -1, NULL, NULL);
    OUT_;
}
BODY(tw_warning_stake) {
    FF f = ff_(cx, cy, a); SoObj *ob = obj_new("wood"); SoSolids *s1 = sl_new(), *s2 = sl_new(), *d = sl_new(), *ps = sl_new(), *ts = sl_new(); NsCuts cu = { 0 }; int sv; PTS *pts; double s0, s1_;
    { double h = U_(rng, 2.6, 3.2); S_cone(s1, cx, cy, 0.62, h, 0.4, 0.0, 8, 1.0, 0.0); ob_st(ob, s1, NULL, -1, NULL, disc(cx, cy, 0.62, 20)); }
    { double L = U_(rng, 2.4, 2.8); S_lie(s2, FX(f, 0, 0), a + 1.5708, L, 0.35, 0.0); ob_st(ob, s2, NULL, -1, NULL, rect(cx, cy, 0.7, 2.8, a)); }
    { double u = U_(rng, 1.0, 1.3); s0 = fx(&f, u, 0); s1_ = fy(&f, u, 0); }
    S_dome(d, s0, s1_, 0.62, 0.8, 0.0, 0.85, a, 12);
    for (sv = -1; sv <= 1; sv += 2) cut_add(&cu, disc(POL(s0, s1_, 0.3, a + sv * 0.5), 0.16, 6), 0.45);
    ob_st(ob, d, "bone", -1, &cu, disc(s0, s1_, 0.62, 20));
    pts = tw_curl(FX(f, -1.6, -1.0), a + 0.8, 3.8, 0.5, 6); tw_vine(pts, 0.32, rng, 0.9, 0.6, ps, ts);
    ob_st(ob, ps, "creeper", -1, NULL, lnb(pts, 0.4)); ob_st(ob, ts, "spikethorn", -1, NULL, NULL);
    OUT_;
}
BODY(tw_drag_gouges) {
    FF f = ff_(cx, cy, a); double L = U_(rng, 4.0, 5.0), vs[4]; GL *l = gl_new(), *hl = gl_new(); G *g, *hole; int i, n, sv; SoSolids *rt = sl_new();
    n = (int)RI(rng, 3, 4); ns_linspace(-0.7, 0.7, n, vs);
    for (i = 0; i < n; i++) gl_add(l, buf(line(wiggle(FX(f, -L / 2, vs[i]), FX(f, L / 2 - 0.6, vs[i] * 0.6), rng, 0.15, 3)), 0.2, 3));
    g = U(l);
    { double r = U_(rng, 0.75, 0.95); hole = disc(FX(f, L / 2, 0), r, 12); }
    for (sv = -1; sv <= 1; sv += 2) { PTS *p = pl_new(); pl_add(p, FX(f, L / 2 - 0.4, sv * 1.1)); pl_add(p, FX(f, L / 2 + 0.6, sv * 0.9)); pl_add(p, FX(f, L / 2 + 1.3, sv * 0.2)); rodl(rt, p, 0.4, 0.15, 1.2); }
    gl_add(hl, g); gl_add(hl, buf(hole, 0.4, 16)); *foot = hull(hl);
    ns_gcut(o, g, 0.3, "crack"); ns_gcut(o, hole, 0.7, "pit"); ns_st(o, rt, "root", 0, 0, NULL); return 0;
}
static const FtTiers SEEP_T = { 2, { { { 0.6, NUM_FLOAT, 0 }, { 20, NUM_INT, 0 } }, { { 0.4, NUM_FLOAT, 0 }, { 50, NUM_INT, 0 } } } };
BODY(tw_sap_seep) {
    G *p, *rim; double r = U_(rng, 1.3, 1.8);
    p = blob(cx, cy, r, rng, 4); rim = dif(buf(p, 0.45, 16), buf(p, 0.05, 16));
    *foot = buf(p, 0.5, 16); ns_gcut(o, p, 0.45, "bile"); ns_mound(o, rim, 0.25, "moss", &SEEP_T); return 0;
}

#define SD(fn, cls, kind, name) static const NsSd D_##fn = { fn, "thornwild", cls, kind, name, 1 };
SD(tw_maw_flower, "growth", "maw", "maw_flower") SD(tw_snap_traps, "growth", "maw", "snap_traps") SD(tw_pitcher_cluster, "growth", "pitcher", "pitcher_cluster")
SD(tw_sundew_rosette, "growth", "sundew", "sundew_rosette") SD(tw_spore_pods, "growth", "sporepod", "spore_pods") SD(tw_burst_pod, "growth", "sporepod", "burst_pod")
SD(tw_spore_stalks, "growth", "spore", "spore_stalks") SD(tw_seed_burrs, "debris", "seedburr", "seed_burrs") SD(tw_glowcap_ring, "growth", "glowcap", "glowcap_ring")
SD(tw_glow_stalks, "growth", "glowcap", "glow_stalks") SD(tw_shelf_fungus_root, "growth", "glowcap", "shelf_fungus_root") SD(tw_root_knot, "growth", "root", "root_knot")
SD(tw_buttress_root, "growth", "root", "buttress_root") SD(tw_thorn_vine, "growth", "creeper", "thorn_vine") SD(tw_thorn_bramble, "growth", "spikethorn", "thorn_bramble")
SD(tw_strangler_coil, "growth", "creeper", "strangler_coil") SD(tw_grasping_tendrils, "growth", "creeper", "grasping_tendrils") SD(tw_rooted_ribcage, "made", "bone", "rooted_ribcage")
SD(tw_lost_helmet, "made", "metal", "lost_helmet") SD(tw_dropped_pack, "made", "leather", "dropped_pack") SD(tw_overgrown_rifle, "made", "metal", "overgrown_rifle")
SD(tw_machete, "made", "metal", "machete") SD(tw_field_lantern, "made", "metal", "field_lantern") SD(tw_boot_and_bones, "made", "leather", "boot_and_bones")
SD(tw_warning_stake, "made", "wood", "warning_stake") SD(tw_drag_gouges, "marks", "pit", "drag_gouges") SD(tw_sap_seep, "marks", "bile", "sap_seep")
void ns_reg_thornwild(void) {
    const NsSd *all[] = { &D_tw_maw_flower, &D_tw_snap_traps, &D_tw_pitcher_cluster, &D_tw_sundew_rosette, &D_tw_spore_pods, &D_tw_burst_pod, &D_tw_spore_stalks,
        &D_tw_seed_burrs, &D_tw_glowcap_ring, &D_tw_glow_stalks, &D_tw_shelf_fungus_root, &D_tw_root_knot, &D_tw_buttress_root, &D_tw_thorn_vine, &D_tw_thorn_bramble,
        &D_tw_strangler_coil, &D_tw_grasping_tendrils, &D_tw_rooted_ribcage, &D_tw_lost_helmet, &D_tw_dropped_pack, &D_tw_overgrown_rifle, &D_tw_machete,
        &D_tw_field_lantern, &D_tw_boot_and_bones, &D_tw_warning_stake, &D_tw_drag_gouges, &D_tw_sap_seep };
    int i; for (i = 0; i < (int)(sizeof all / sizeof *all); i++) ns_sd_reg(all[i], 10.5, all[i]->body == tw_drag_gouges || all[i]->body == tw_sap_seep);
}
