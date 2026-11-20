/* bld_ns_ashveil.c: the 29 style-only details of sd_r_x_ashveil.py (style_ashveil.py's own tier features are in
   bld_ns_tiers.c), copied routine by routine (each names the Python it copies). See bld_ns.h. */
#include "bld_ns.h"

#define BODY(nm) static int nm(double cx, double cy, double a, PyRandom *rng, NsOut *o, G **foot)
#define OUT_ ns_obj_out(o, ob, NULL, foot); return 0

/* xa_bar(p, q, rf, k=0.45): a round bar lying on the ground, solid underneath; rf a number or a function of 0..1 */
typedef struct { double px, py, ux, uy, L; int m; double *S, *R, *C; } XaBar;
static void xabar_free(void *c) { XaBar *b = (XaBar *)c; free(b->S); free(b->R); free(b->C); free(b); }
static int xabar_sect(void *c, double z, G **out) {
    const XaBar *b = (const XaBar *)c; int i, nr = 0, err = 0; double *rs = NULL, *rw = NULL; ShpList ps = { 0 };
    *out = NULL;
    rs = (double *)malloc(sizeof(double) * (size_t)(b->m + 2)); rw = (double *)malloc(sizeof(double) * (size_t)(b->m + 2));
    for (i = 0; i <= b->m + 1; i++) {
        double s = i <= b->m ? b->S[i] : 0.0, r = i <= b->m ? b->R[i] : 0.0, cc = i <= b->m ? b->C[i] : 0.0, w;
        if (z <= cc) w = r;
        else if (z < cc + r) { double q = lf_fpow(z - cc, 2.0, &err); w = sqrt(pmax(0.0, r * r - q)); }
        else w = 0.0;
        if (w >= 0.2) { rs[nr] = s; rw[nr] = w; nr++; continue; }
        if (nr >= 2 && (rs[nr - 1] - rs[0]) * b->L >= 0.3) {
            double *xy = (double *)malloc(sizeof(double) * 4 * (size_t)nr); int j, k = 0; G *g;
            for (j = 0; j < nr; j++) { xy[k++] = b->px + b->ux * rs[j] * b->L - b->uy * rw[j]; xy[k++] = b->py + b->uy * rs[j] * b->L + b->ux * rw[j]; }
            for (j = nr - 1; j >= 0; j--) { xy[k++] = b->px + b->ux * rs[j] * b->L - b->uy * -rw[j]; xy[k++] = b->py + b->uy * rs[j] * b->L + b->ux * -rw[j]; }
            g = shp_polygon(xy, 2 * nr); free(xy);
            if (!g) { err = 1; break; }
            { int v = shp_is_valid(g); if (v < 0) { shp_free(g); err = 1; break; } if (v && shp_area(g) > 0.05) shp_list_push(&ps, g); else shp_free(g); }
        }
        nr = 0;
    }
    free(rs); free(rw);
    if (err) { shp_list_free(&ps); return SO_ERR; }
    if (ps.n) { *out = sd_U((const G *const *)ps.g, ps.n); shp_list_free(&ps); return *out ? SO_OK : SO_ERR; }
    return SO_OK;
}
typedef double (*XaRf)(const void *ctx, double s);
static SoSolids *xa_bar(double px, double py, double qx, double qy, XaRf rf, const void *rctx, double rconst, double k) {
    XaBar b; int i; double top = 0.0; SoSolids *sol = sl_new();
    b.L = ns_mdist(px, py, qx, qy); if (b.L == 0.0) b.L = 1.0;
    b.m = (int)ns_int(b.L / 0.11); if (b.m < 8) b.m = 8;
    b.px = px; b.py = py; b.ux = (qx - px) / b.L; b.uy = (qy - py) / b.L;
    b.S = (double *)malloc(sizeof(double) * (size_t)(b.m + 1)); b.R = (double *)malloc(sizeof(double) * (size_t)(b.m + 1)); b.C = (double *)malloc(sizeof(double) * (size_t)(b.m + 1));
    for (i = 0; i <= b.m; i++) { b.S[i] = (double)i / b.m; b.R[i] = pmax(0.0, rf ? rf(rctx, b.S[i]) : rconst); b.C[i] = k * b.R[i]; }
    for (i = 0; i <= b.m; i++) { double v = b.C[i] + b.R[i]; if (i == 0 || v > top) top = v; }
    sl_custom_f(sol, xabar_sect, &b, sizeof b, top, xabar_free);
    return sol;
}
/* xa_drift(o, x, y, r, h, a, rng, sq=None) */
static void xa_drift(SoObj *ob, double x, double y, double r, double h, double a, PyRandom *rng, double sq) {
    SoSolids *s = sl_new();
    if (sq == 0.0) sq = U_(rng, 0.55, 0.75);
    S_dome(s, x, y, r, h, 0.0, sq, a, 20); ob_st(ob, s, "ashdrift", -1, NULL, ell(x, y, r, r * 0.7, a, 16));
}
/* xa_up(o, g, h, kind=None): o.st(ext(g, h), kind, foot=g) */
static int xa_up(SoObj *ob, const G *g, double h, const char *kind) { return ob_st(ob, ext1(g, h), kind, -1, NULL, g); }
/* xa_lattice(o, ff, x0, x1, w0, w1, a, kind, h=0.5, step=1.3) */
static int xa_lattice(SoObj *ob, const FF *f, double x0, double x1, double w0, double w1, double a, const char *kind, double h, double step) {
    GL *rl = gl_new(), *br = gl_new(); double xs[256]; int sv, i, n, b; G *ub;
#define WV(x) ((w0 + (w1 - w0) * ((x) - x0) / (x1 - x0)) / 2)
    for (sv = -1; sv <= 1; sv += 2) gl_add(rl, barl(fx(f, x0, sv * WV(x0)), fy(f, x0, sv * WV(x0)), fx(f, x1, sv * WV(x1)), fy(f, x1, sv * WV(x1)), 0.55));
    n = ns_arange(x0, x1, step, xs, 255); xs[n++] = x1;
    for (i = 0; i + 1 < n; i++) { double xa = xs[i], xb = xs[i + 1]; int s2 = i % 2 ? 1 : -1; gl_add(br, barl(fx(f, xa, s2 * WV(xa)), fy(f, xa, s2 * WV(xa)), fx(f, xb, -s2 * WV(xb)), fy(f, xb, -s2 * WV(xb)), 0.48)); }
#undef WV
    b = xa_up(ob, U(rl), h, kind); ub = U(br); ob_st(ob, ext1(ub, h * 0.8), kind, -1, NULL, ub);
    return b;
}
static G *lnb(PTS *p, double d) { return buf(line(p), d, 16); }
static G *seg2(double ax, double ay, double bx, double by, double d) { PTS *p = pl_new(); pl_add(p, ax, ay); pl_add(p, bx, by); return lnb(p, d); }

BODY(xa_buried_cab) {
    FF f = ff_(cx, cy, a); double L = U_(rng, 4.2, 5.0), W = U_(rng, 3.0, 3.4), h = U_(rng, 1.5, 1.8), sv, tcx, tcy, tr; SoObj *ob = obj_new("ashhull"); G *body; int b, rf_, hp, i; GL *tl = gl_new(); NsCuts cu = { 0 };
    double fs[4] = { -0.6, -0.2, 0.2, 0.6 };
    body = rrect(cx, cy, L, W, a, 0.35); b = ob_up(ob, body, h, NULL);
    ob_cut(ob, rect(FX(f, L * 0.22, 0), 0.9, W - 0.8, a), b, 0.35);
    rf_ = ob_on(ob, rrect(FX(f, -L * 0.12, 0), L * 0.5, W - 0.6, a, 0.25), b, 0.22, NULL);
    { double v = U_(rng, -0.3, 0.3), h0 = fx(&f, -L * 0.12, v), h1 = fy(&f, -L * 0.12, v); hp = ob_on(ob, disc(h0, h1, 0.6, 16), rf_, 0.14, NULL); ob_cut(ob, barl(POL(h0, h1, 0.45, a + 1.57), POL(h0, h1, 0.45, a - 1.57), 0.12), hp, 0.1); }
    ob_vents(ob, &f, L / 2 - 0.45, 0, b, 4, W * 0.55, a + 1.5708, 0.38, 0.16, 0.2);
    sv = CH(rng, -1, 1); { double u = U_(rng, -0.2, 0.6); tcx = fx(&f, u, sv * (W / 2 + 0.75)); tcy = fy(&f, u, sv * (W / 2 + 0.75)); } tr = U_(rng, 1.0, 1.2);
    for (i = 0; i < 4; i++) { double px = polx(tcx, tr * fs[i], a), py = poly_(tcy, tr * fs[i], a); gl_add(tl, barl(POL(px, py, 0.8, a + 1.5708), POL(px, py, 0.8, a - 1.5708), 0.16)); }
    cut_add(&cu, U(tl), 0.75);
    ob_st(ob, xa_bar(POL(tcx, tcy, tr, a + PI), POL(tcx, tcy, tr, a), NULL, NULL, 0.62, 0.55), "rust", -1, &cu, rect(tcx, tcy, 2 * tr, 1.3, a));
    { double v = U_(rng, -0.4, 0.4), x = fx(&f, -L * 0.45, v), y = fy(&f, -L * 0.45, v), r = U_(rng, 2.2, 2.8), hh = U_(rng, 1.2, 1.6), aa = a + U_(rng, -0.3, 0.3); xa_drift(ob, x, y, r, hh, aa, rng, 0.0); }
    OUT_;
}
BODY(xa_track_links) {
    SoObj *ob = obj_new("ashhull"); int n = (int)RI(rng, 5, 7), i; double st = U_(rng, 0.95, 1.05), W = U_(rng, 2.2, 2.6), bend = U_(rng, -0.06, 0.06), t = a, px, py, lx, ly;
    px = cx - ns_cos(a) * n * st / 2; py = cy - ns_sin(a) * n * st / 2; lx = px; ly = py;
    for (i = 0; i < n; i++) {
        FF f2 = ff_(px, py, t); G *g = rect(px, py, st - 0.12, W, t); int b; double h; GL *l = gl_new(); int sv;
        h = U_(rng, 0.38, 0.45); b = ob_up(ob, g, h, NULL);
        ob_on(ob, rect(FX(f2, 0, 0), 0.32, W - 0.4, t), b, 0.22, NULL);
        for (sv = -1; sv <= 1; sv += 2) gl_add(l, disc(FX(f2, st / 2 - 0.06, sv * (W / 2 - 0.35)), 0.24, 8));
        ob_on(ob, U(l), b, 0.14, NULL);
        lx = px; ly = py; t += bend; { double nx = polx(px, st, t), ny = poly_(py, st, t); px = nx; py = ny; }
    }
    { double r = U_(rng, 1.6, 2.0), h = U_(rng, 0.8, 1.0); xa_drift(ob, lx, ly, r, h, t + 1.5708, rng, 0.0); }
    OUT_;
}
BODY(xa_hub_wheel) {
    double R = U_(rng, 1.7, 2.1), h; SoObj *ob = obj_new("rust"); int ty, rim, hb, k; GL *l = gl_new(); PTS *p = pl_new();
    { G *g = ann(cx, cy, R, R * 0.6, 32); h = U_(rng, 0.75, 0.9); ty = xa_up(ob, g, h, NULL); }
    for (k = 0; k < 16; k++) gl_add(l, barl(POL(cx, cy, R * 0.86, a + k * 0.393), POL(cx, cy, R + 0.2, a + k * 0.393), 0.2));
    ob_cut(ob, U(l), ty, 0.2);
    rim = ob_up(ob, disc(cx, cy, R * 0.63, 28), 0.45, NULL); hb = ob_on(ob, disc(cx, cy, R * 0.3, 16), rim, 0.3, NULL);
    ob_cut(ob, disc(cx, cy, 0.22, 8), hb, 0.3);
    for (k = 0; k < 8; k++) pl_add(p, POL(cx, cy, R * 0.43, a + k * 6.2832 / 8));
    ob_studs(ob, p, rim, 0.17, 0.14, NULL, 8);
    if (RR(rng) < 0.6) { double t = a + U_(rng, 0, 6.28), x = polx(cx, R * 0.8, t), y = poly_(cy, R * 0.8, t), hh = U_(rng, 0.8, 1.0); xa_drift(ob, x, y, R * 0.75, hh, t + 1.57, rng, 0.0); }
    OUT_;
}
BODY(xa_cargo_container) {
    FF f = ff_(cx, cy, a); double L = U_(rng, 6.5, 7.5), W = U_(rng, 2.4, 2.7), h = U_(rng, 2.0, 2.3), xs[64], vs[4], sv; SoObj *ob = obj_new("ashcargo"); G *g = rect(cx, cy, L, W, a); int b, i, n; GL *l = gl_new(), *l2 = gl_new();
    b = ob_up(ob, g, h, NULL);
    n = ns_arange(-L / 2 + 0.9, L / 2 - 0.9, 0.5, xs, 64); for (i = 0; i < n; i++) gl_add(l, barl(FX(f, xs[i], -W), FX(f, xs[i], W), 0.2));
    ob_cut(ob, inter(U(l), inset(g, 0.35)), b, 0.12);
    ob_corners(ob, &f, L, W, b, a, 0.55, 0.12, NULL);
    ob_cut(ob, barl(FX(f, L / 2 - 0.18, -W), FX(f, L / 2 - 0.18, W), 0.12), b, 0.2);
    vs[0] = -W * 0.36; vs[1] = -W * 0.12; vs[2] = W * 0.12; vs[3] = W * 0.36;
    for (i = 0; i < 4; i++) gl_add(l2, rect(FX(f, L / 2 - 0.55, vs[i]), 0.5, 0.5, a));
    ob_on(ob, U(l2), b, 0.12, NULL);
    sv = CH(rng, -1, 1);
    { double v = U_(rng, -0.5, 0.5), x = fx(&f, -sv * L * 0.42, v), y = fy(&f, -sv * L * 0.42, v), r = U_(rng, 2.4, 3.0), hh = h * U_(rng, 0.75, 0.95), aa = a + 1.5708 + U_(rng, -0.3, 0.3); xa_drift(ob, x, y, r, hh, aa, rng, 0.0); }
    OUT_;
}
BODY(xa_buried_hatch) {
    double R = U_(rng, 1.3, 1.6); SoObj *ob = obj_new("ashhull"); int ld, k; GL *l = gl_new();
    ob_gcut(ob, disc(cx, cy, R + 0.9, 24), 0.25, "hollow");
    xa_up(ob, ann(cx, cy, R, R - 0.5, 28), 0.6, NULL); ld = ob_up(ob, disc(cx, cy, R - 0.45, 24), 0.4, NULL);
    ob_on(ob, ann(cx, cy, R * 0.55, R * 0.55 - 0.45, 20), ld, 0.3, NULL);
    for (k = 0; k < 3; k++) gl_add(l, barl(POL(cx, cy, R * 0.5, a + k * 2.094), POL(cx, cy, 0.0, a), 0.45));
    ob_on(ob, U(l), ld, 0.3, NULL);
    ob_up(ob, rect(POL(cx, cy, R + 0.1, a + PI), 0.8, 1.2, a), 0.75, NULL);
    OUT_;
}
typedef struct { double r; } StiltRf;
static double stilt_rf(const void *c, double s) { double r = ((const StiltRf *)c)->r; return r * (fabs(s - 0.42) < 0.035 ? 1.22 : 1.0) * (s > 0.97 ? 0.75 : 1.0); }
BODY(xa_stilt_pipe) {
    FF f = ff_(cx, cy, a); SoObj *ob = obj_new("ashpipe"); int k = (int)RI(rng, 2, 3), i; double gap = U_(rng, 3.2, 3.8), hs = U_(rng, 2.2, 2.7), Lp, r, px, py, qx, qy; StiltRf sr;
    for (i = 0; i < k; i++) {
        double u = (i - (k - 1) / 2.0) * gap, c0 = fx(&f, u, -1.2), c1 = fy(&f, u, -1.2), aa = a + U_(rng, -0.05, 0.05), h; G *g = rect(c0, c1, 0.95, 0.95, aa); int sp;
        h = hs * U_(rng, 0.85, 1.0); sp = ob_up(ob, g, h, NULL);
        ob_cut(ob, rect(c0, c1, 1.4, 0.42, a + 1.5708), sp, 0.3);
        ob_up(ob, rect(c0, c1, 1.6, 1.6, a), 0.3, NULL);
    }
    Lp = gap * (k - 1) + U_(rng, 1.5, 2.5); r = U_(rng, 0.6, 0.72); sr.r = r;
    px = fx(&f, -Lp / 2, 1.0); py = fy(&f, -Lp / 2, 1.0); { double v = 1.0 + U_(rng, -0.3, 0.3); qx = fx(&f, Lp / 2, v); qy = fy(&f, Lp / 2, v); }
    ob_st(ob, xa_bar(px, py, qx, qy, stilt_rf, &sr, 0, 0.5), NULL, -1, NULL, seg2(px, py, qx, qy, r));
    { double d = U_(rng, 0.5, 0.8), t = a + U_(rng, -0.4, 0.4); GL *l = gl_new(); gl_add(l, tri(POL(qx, qy, 0, a), POL(qx, qy, d, t), 0.5)); ob_st(ob, ext1(U(l), r * 1.2), NULL, -1, NULL, disc(qx, qy, 0.8, 20)); }
    if (RR(rng) < 0.6) ob_gcut(ob, ell(FX(f, Lp / 2 + 0.8, 1.2), 1.0, 0.7, a, 14), 0.3, "tar");
    OUT_;
}
BODY(xa_valve_head) {
    SoObj *ob = obj_new("ashpipe"); double r = U_(rng, 0.7, 0.85); int fl, rs, wl, k; PTS *p = pl_new(); GL *l = gl_new(); SoSolids *s1 = sl_new(), *s2 = sl_new();
    fl = ob_up(ob, disc(cx, cy, r + 0.45, 20), 0.35, NULL);
    for (k = 0; k < 6; k++) pl_add(p, POL(cx, cy, r + 0.22, a + k * 1.0472));
    ob_studs(ob, p, fl, 0.17, 0.12, NULL, 8);
    { double pr[12] = { r, 0.0, r, 1.1, r + 0.25, 1.15, r + 0.25, 1.4, r * 0.6, 1.5, r * 0.6, 1.9 }; S_turn(s1, cx, cy, pr, 6, 20, 1.0, 0.0, 0.0); rs = ob_st(ob, s1, NULL, fl, NULL, NULL); }
    { double pr[6] = { r + 0.75, 1.9, r + 0.75, 2.15, 0.3, 2.2 }; S_turn(s2, cx, cy, pr, 3, 24, 1.0, 0.0, 0.0); wl = ob_st(ob, s2, NULL, rs, NULL, NULL); }
    for (k = 0; k < 4; k++) gl_add(l, disc(POL(cx, cy, (r + 0.75) * 0.55, a + 0.785 + k * 1.5708), 0.25, 8));
    ob_cut(ob, U(l), wl, 0.2);
    if (RR(rng) < 0.6) { double t = a + U_(rng, 0, 6.28); xa_drift(ob, POL(cx, cy, r + 1.0, t), 1.1, 0.7, t + 1.57, rng, 0.0); }
    OUT_;
}
typedef struct { double r; } HoopRf;
static double hoop_rf(const void *c, double s) { double r = ((const HoopRf *)c)->r; return r * ((fabs(s - 0.33) < 0.035 || fabs(s - 0.67) < 0.035) ? 1.08 : 1.0); }
BODY(xa_toppled_drums) {
    FF f = ff_(cx, cy, a); SoObj *ob = obj_new("rust"); double r = U_(rng, 0.85, 0.95), c0, c1, h; int dr, sv, i, n; GL *l = gl_new(); HoopRf hr;
    { double rr = U_(rng, 1.6, 2.0); ob_gcut(ob, blob(FX(f, 0.6, 0.4), rr, rng, 5), 0.25, "tar"); }
    c0 = fx(&f, -1.4, 0); c1 = fy(&f, -1.4, 0); { G *g = disc(c0, c1, r, 22); h = U_(rng, 1.6, 1.8); dr = ob_up(ob, g, h, NULL); }
    ob_cut(ob, ann(c0, c1, r - 0.12, r - 0.3, 20), dr, 0.15);
    for (sv = -1; sv <= 1; sv += 2) gl_add(l, disc(POL(c0, c1, r * 0.5, a + sv * 1.2), 0.18, 8));
    ob_cut(ob, U(l), dr, 0.2);
    n = (int)RI(rng, 1, 2); hr.r = r;
    for (i = 0; i < n; i++) {
        double t = a + U_(rng, -0.5, 0.5) + 1.5708, p0 = fx(&f, 0.6 + 0.2 * i, -0.6 + 2.0 * i), p1 = fy(&f, 0.6 + 0.2 * i, -0.6 + 2.0 * i), L = 2.4; FF f2 = ff_(p0, p1, t);
        ob_st(ob, xa_bar(FX(f2, -L / 2, 0), FX(f2, L / 2, 0), hoop_rf, &hr, 0, 0.4), NULL, -1, NULL, rect(p0, p1, L, 2 * r, t));
    }
    OUT_;
}
BODY(xa_fallen_pylon) {
    FF f = ff_(cx, cy, a); double L = U_(rng, 7.5, 9.0), x0, x1, h; SoObj *ob = obj_new("ashpylon"); G *hd; int bh, ft;
    x0 = -L / 2; x1 = L / 2 - 1.4;
    h = U_(rng, 0.5, 0.6); xa_lattice(ob, &f, x0 + 0.6, x1, 2.0, 1.1, a, "ashpylon", h, 1.3);
    hd = rect(FX(f, L / 2 - 0.8, 0), 1.6, 1.5, a); h = U_(rng, 1.1, 1.3); bh = ob_up(ob, hd, h, NULL); ob_vents(ob, &f, L / 2 - 0.8, 0, bh, 3, 1.1, a, 0.38, 0.16, 0.25);
    xa_up(ob, ann(FX(f, L / 2 + 0.35, 0), 0.8, 0.3, 16), 0.9, NULL);
    { double r = U_(rng, 1.2, 1.4); G *g = stone(FX(f, x0 + 0.2, 0), r, rng, 0.8, 7); h = U_(rng, 0.6, 0.8); ft = ob_up(ob, g, h, "ashstone"); }
    ob_on(ob, rect(FX(f, x0 + 0.3, 0), 1.3, 1.6, a), ft, 0.15, NULL);
    OUT_;
}
BODY(xa_pylon_stub) {
    FF f = ff_(cx, cy, a); double S = U_(rng, 2.6, 3.0); SoObj *ob = obj_new("ashstone"); int b0, b1, sx, sy;
    b0 = ob_up(ob, rect(cx, cy, S + 0.8, S + 0.8, a), 0.5, NULL); b1 = ob_on(ob, rect(cx, cy, S, S, a), b0, 0.45, NULL);
    for (sx = -1; sx <= 1; sx += 2) for (sy = -1; sy <= 1; sy += 2) {
        double c0 = fx(&f, sx * (S / 2 - 0.55), sy * (S / 2 - 0.55)), c1 = fy(&f, sx * (S / 2 - 0.55), sy * (S / 2 - 0.55)), h; int pl = ob_on(ob, rect(c0, c1, 0.8, 0.8, a), b1, 0.12, "ashpylon");
        { G *g = rect(c0, c1, 0.5, 0.5, a); h = U_(rng, 0.6, 1.4); ob_on(ob, g, pl, h, "ashpylon"); }
    }
    ob_cut(ob, rect(FX(f, S / 2 + 0.2, 0), 0.25, 1.0, a), b0, 0.12);
    OUT_;
}
BODY(xa_relay_mast) {
    FF f = ff_(cx, cy, a); double L = U_(rng, 7.0, 8.5), c0, c1, R; SoObj *ob = obj_new("rust"); int sv;
    xa_lattice(ob, &f, -L / 2, L / 2 - 2.0, 1.2, 1.2, a, "rust", 0.5, 1.1);
    c0 = fx(&f, L / 2 - 0.6, 0); c1 = fy(&f, L / 2 - 0.6, 0); R = U_(rng, 1.4, 1.7);
    xa_up(ob, ann(c0, c1, R, R - 0.5, 24), 0.7, "ashpylon"); ob_up(ob, disc(c0, c1, R - 0.45, 20), 0.3, "ashpylon");
    ob_up(ob, disc(c0, c1, 0.35, 10), 0.9, "ashpylon");
    for (sv = -1; sv <= 1; sv += 2) { double u = U_(rng, -L / 2, 0), v = sv * U_(rng, 1.8, 2.4); xa_up(ob, disc(FX(f, u, v), 0.45, 10), 0.4, NULL); }
    OUT_;
}
BODY(xa_warning_sign) {
    FF f = ff_(cx, cy, a); double Ls = U_(rng, 4.4, 5.2), c0, c1, B, t, vs[3] = { -0.18, 0.0, 0.18 }; SoObj *ob = obj_new("ashscrap"); GL *l = gl_new(), *cl = gl_new(); G *g; int b, i; FF f2; PTS *sp = pl_new();
    gl_add(l, rect(FX(f, -0.4, 0), Ls - 0.8, 0.65, a));
    for (i = 0; i < 3; i++) { double u = Ls / 2 - 0.85 + U_(rng, 0.3, 0.6); gl_add(l, tri(FX(f, Ls / 2 - 0.85, vs[i]), FX(f, u, vs[i] * 0.6), 0.24)); }
    xa_up(ob, U(l), 0.4, NULL);
    c0 = fx(&f, -Ls / 2 + 1.2, 0); c1 = fy(&f, -Ls / 2 + 1.2, 0); B = U_(rng, 2.2, 2.6); t = a + U_(rng, -0.2, 0.2); f2 = ff_(c0, c1, t);
    { double xy[6] = { FX(f2, B / 2 - 0.6, B / 2 + 0.1), FX(f2, B / 2 + 0.1, B / 2 + 0.1), FX(f2, B / 2 + 0.1, B / 2 - 0.6) }; g = dif(rect(c0, c1, B, B, t), polyv(xy, 3)); }
    b = ob_up(ob, g, 0.55, "ashpaint"); ob_groove(ob, inset(g, 0.25), b, 0.16, 0.12);
    { double dxs[2] = { -0.1, 0.5 }; for (i = 0; i < 2; i++) { PTS *p = pl_new(); pl_add(p, FX(f2, -0.5 + dxs[i], -0.55)); pl_add(p, FX(f2, 0.05 + dxs[i], 0)); pl_add(p, FX(f2, -0.5 + dxs[i], 0.55)); gl_add(cl, lineb(p, 0.24)); } }
    ob_cut(ob, U(cl), b, 0.14);
    pl_add(sp, FX(f2, 0, -B * 0.32)); pl_add(sp, FX(f2, 0, B * 0.32));
    ob_studs(ob, sp, b, 0.18, 0.08, "rust", 8);
    OUT_;
}
BODY(xa_cell_panel) {
    FF f = ff_(cx, cy, a); double L = U_(rng, 3.2, 3.8), W = U_(rng, 2.0, 2.4), h, gxs[3], gys[2], cw, chh; SoObj *ob = obj_new("ashpylon"); G *g; int b, sv, i, nx = 4, ny = 3, n; GL *l = gl_new();
    for (sv = -1; sv <= 1; sv += 2) xa_up(ob, rect(FX(f, 0, sv * (W / 2 - 0.3)), L - 0.4, 0.6, a), 0.35, "rust");
    g = rect(cx, cy, L, W, a); h = U_(rng, 0.75, 0.9); b = ob_up(ob, g, h, NULL);
    for (i = 1; i < nx; i++) gxs[i - 1] = -L / 2 + 0.25 + (L - 0.5) * i / nx;
    for (i = 1; i < ny; i++) gys[i - 1] = -W / 2 + 0.25 + (W - 0.5) * i / ny;
    for (i = 0; i < nx - 1; i++) gl_add(l, barl(FX(f, gxs[i], -W / 2 + 0.3), FX(f, gxs[i], W / 2 - 0.3), 0.22));
    for (i = 0; i < ny - 1; i++) gl_add(l, barl(FX(f, -L / 2 + 0.3, gys[i]), FX(f, L / 2 - 0.3, gys[i]), 0.22));
    ob_cut(ob, U(l), b, 0.18);
    n = (int)RI(rng, 1, 2);
    for (i = 0; i < n; i++) {
        int ii = (int)RI(rng, 0, nx - 1), jj = (int)RI(rng, 0, ny - 1);
        cw = (L - 0.5) / nx; chh = (W - 0.5) / ny;
        ob_cut(ob, rect(FX(f, -L / 2 + 0.25 + cw * (ii + 0.5), -W / 2 + 0.25 + chh * (jj + 0.5)), cw - 0.3, chh - 0.3, a), b, 0.35);
    }
    OUT_;
}
BODY(xa_lean_to) {
    FF f = ff_(cx, cy, a); double L = U_(rng, 4.2, 5.0), W = U_(rng, 2.6, 3.0), h, xs[2]; SoObj *ob = obj_new("ashtarp"); SoSolids *s = sl_new(); NsCuts cu = { 0 }; GL *l = gl_new(); int sx, i, n;
    h = U_(rng, 1.4, 1.7); S_loaf(s, cx, cy, a, L, W, h, 0.0);
    xs[0] = -L * 0.2; xs[1] = L * 0.2; for (i = 0; i < 2; i++) gl_add(l, barl(FX(f, xs[i], -W), FX(f, xs[i], W), 0.16));
    cut_add(&cu, U(l), 1.0); ob_st(ob, s, NULL, -1, &cu, rect(cx, cy, L, W, a));
    for (sx = -1; sx <= 1; sx += 2) { G *g = disc(FX(f, sx * (L / 2 + 0.25), 0), 0.35, 8); h = U_(rng, 1.8, 2.1); xa_up(ob, g, h, "ashscrap"); }
    n = (int)RI(rng, 2, 4);
    for (i = 0; i < n; i++) { double u = U_(rng, -L / 2, L / 2), p0 = fx(&f, u, -W / 2 - 0.3), p1 = fy(&f, u, -W / 2 - 0.3), r = U_(rng, 0.4, 0.55), hh = U_(rng, 0.4, 0.55); SoSolids *rk = sl_new(); S_rock(rk, p0, p1, r, hh, rng, 0.0, 0.0, 9); ob_st(ob, rk, "ashcinder", -1, NULL, disc(p0, p1, 0.5, 20)); }
    { double u = L / 2 + U_(rng, 1.2, 1.8), v = U_(rng, -0.8, 0.8), e0 = fx(&f, u, v), e1 = fy(&f, u, v); PTS *p = pl_new(); SoSolids *rd = sl_new();
      pl_add(p, FX(f, L / 2 + 0.25, 0)); pl_add(p, e0, e1); S_rod(rd, p, 0.16, 1.0, 0.0, 0.0); ob_st(ob, rd, "rope", -1, NULL, disc(e0, e1, 0.3, 20)); ob_up(ob, disc(e0, e1, 0.3, 8), 0.5, "rust"); }
    OUT_;
}
BODY(xa_ash_still) {
    SoObj *ob = obj_new("rust"); double r = U_(rng, 0.9, 1.0), h, t, cn0, cn1; int dr, cb, k; PTS *pts = pl_new(); SoSolids *s = sl_new(), *rd = sl_new();
    { G *g = disc(cx, cy, r, 22); h = U_(rng, 1.2, 1.4); dr = ob_up(ob, g, h, NULL); }
    S_cone(s, cx, cy, r + 0.15, 0.9, 0.25, 0.0, 20, 1.0, 0.0); ob_st(ob, s, "ashscrap", dr, NULL, NULL);
    t = a + U_(rng, -0.4, 0.4); cn0 = polx(cx, r + 1.3, t); cn1 = poly_(cy, r + 1.3, t);
    for (k = 0; k < 7; k++) pl_add(pts, POL(cx, cy, r + 0.25, t + 1.2 * k / 6));
    pl_add(pts, cn0, cn1);
    S_rod(rd, pts, 0.2, 1.0, 0.0, 0.0); ob_st(ob, rd, "ashpipe", -1, NULL, lnb(pts, 0.3));
    cb = ob_up(ob, disc(cn0, cn1, 0.55, 14), 0.8, NULL); ob_cut(ob, ann(cn0, cn1, 0.45, 0.25, 12), cb, 0.1);
    OUT_;
}
BODY(xa_salvage_sled) {
    FF f = ff_(cx, cy, a); double L = U_(rng, 4.6, 5.4), W = U_(rng, 2.2, 2.5), xs[64]; SoObj *ob = obj_new("ashscrap"); int sv, dk, bx, i, n; GL *l = gl_new(), *gl = gl_new();
    for (sv = -1; sv <= 1; sv += 2) { GL *q = gl_new(); gl_add(q, rect(FX(f, 0, sv * (W / 2 - 0.3)), L, 0.6, a)); gl_add(q, disc(FX(f, L / 2, sv * (W / 2 - 0.3)), 0.3, 8)); xa_up(ob, U(q), 0.35, "rust"); }
    dk = ob_up(ob, rect(FX(f, -0.2, 0), L - 0.9, W, a), 0.55, NULL);
    n = ns_arange(-L / 2 + 1.0, L / 2 - 1.0, 1.1, xs, 64); for (i = 0; i < n; i++) gl_add(l, barl(FX(f, xs[i], -W), FX(f, xs[i], W), 0.14));
    ob_cut(ob, U(l), dk, 0.1);
    bx = ob_on(ob, rect(FX(f, -0.9, 0), 1.5, 1.4, a), dk, 0.8, "ashcargo"); ob_lid(ob, rect(FX(f, -0.9, 0), 1.5, 1.4, a), bx, 0.12, 0.15, "ashcargo");
    ob_st(ob, xa_bar(FX(f, 0.4, -0.75), FX(f, 0.4, 0.75), NULL, NULL, 0.68, 0.45), "rust", dk, NULL, rect(FX(f, 0.4, 0), 1.4, 1.5, a));
    { double u = L / 2 + U_(rng, 2.0, 3.0), v = U_(rng, -0.6, 0.6), e0 = fx(&f, u, v), e1 = fy(&f, u, v); SoSolids *rd = sl_new(); PTS *w = wiggle(FX(f, L / 2, 0), e0, e1, rng, 0.3, 4);
      S_rod(rd, w, 0.17, 1.0, 0.0, 0.0); ob_st(ob, rd, "rope", -1, NULL, disc(e0, e1, 0.3, 20)); }
    for (sv = -1; sv <= 1; sv += 2) gl_add(gl, barl(FX(f, L / 2 + 0.6, sv * (W / 2 - 0.25)), FX(f, L / 2 + 3.0, sv * (W / 2 - 0.25)), 0.4));
    ob_gcut(ob, U(gl), 0.15, "rut");
    OUT_;
}
BODY(xa_breath_mask) {
    FF f = ff_(cx, cy, a); SoObj *ob = obj_new("rust"); G *fc = ell(cx, cy, 1.25, 1.0, a, 20); int b, sv; GL *l = gl_new();
    b = ob_up(ob, fc, 0.5, NULL);
    for (sv = -1; sv <= 1; sv += 2) gl_add(l, disc(FX(f, 0.35, sv * 0.45), 0.32, 12));
    ob_cut(ob, U(l), b, 0.25);
    for (sv = -1; sv <= 1; sv += 2) ob_up(ob, disc(FX(f, -0.75, sv * 0.75), 0.48, 14), 0.75, "ashscrap");
    { double u = U_(rng, 1.2, 2.0), v = U_(rng, 1.6, 2.2); PTS *w = wiggle(FX(f, 0.2, 1.0), FX(f, u, v), rng, 0.3, 4); SoSolids *rd = sl_new(); S_rod(rd, w, 0.17, 1.0, 0.0, 0.0); ob_st(ob, rd, "ashtarp", -1, NULL, disc(FX(f, 1.4, 1.8), 0.3, 20)); }
    OUT_;
}
BODY(xa_visor_helmet) {
    FF f = ff_(cx, cy, a); SoObj *ob = obj_new("ashscrap"); double r = U_(rng, 1.05, 1.2); SoSolids *s = sl_new(); NsCuts cu = { 0 };
    S_dome(s, cx, cy, r, r * 0.95, 0.0, 0.88, a, 20); S_ext(s, rect(cx, cy, 2 * r * 0.95, 0.5, a), r * 1.0, 0.0);
    cut_add(&cu, rect(FX(f, r * 0.55, 0), 0.4, r * 1.3, a), r * 0.45);
    ob_st(ob, s, NULL, -1, &cu, disc(cx, cy, r, 20));
    ob_up(ob, disc(FX(f, r + 0.2, 0), 0.38, 10), 0.4, "rust");
    { double v = U_(rng, -0.3, 0.3); xa_drift(ob, FX(f, -r * 0.9, v), r * 1.1, r * 0.6, a + 1.57, rng, 0.0); }
    OUT_;
}
static int grazer_rf(void *c, double s, double *out) { (void)c; *out = 0.55 + 0.55 * ns_sin(pmin(1.0, s * 1.6) * 1.5708) * (1.0 - 0.45 * pmax(0.0, s - 0.6) / 0.4); return 0; }
BODY(xa_grazer_skull) {
    FF f = ff_(cx, cy, a); SoObj *ob = obj_new("bone"); double L = U_(rng, 3.2, 3.8); SoSolids *sk = sl_new(); NsCuts cu = { 0 }; GL *l1 = gl_new(), *l2 = gl_new(); int sv;
    S_spin(sk, FX(f, 0, 0), a, L, grazer_rf, NULL, 1.0, 0.0, "round");
    for (sv = -1; sv <= 1; sv += 2) gl_add(l1, disc(FX(f, -L * 0.12, sv * 0.55), 0.3, 10));
    for (sv = -1; sv <= 1; sv += 2) gl_add(l2, barl(FX(f, L * 0.2, sv * 0.35), FX(f, L * 0.45, sv * 0.28), 0.12));
    cut_add(&cu, U(l1), 0.35); cut_add(&cu, U(l2), 0.45);
    ob_st(ob, sk, NULL, -1, &cu, rect(cx, cy, L, 2.2, a));
    for (sv = -1; sv <= 1; sv += 2) {
        PTS *p = pl_new(); SoSolids *rd = sl_new(); double v3, v4;
        pl_add(p, FX(f, -L * 0.3, sv * 0.8)); pl_add(p, FX(f, -L * 0.5, sv * 1.6));
        v3 = sv * U_(rng, 1.8, 2.1); pl_add(p, FX(f, -L * 0.8, v3)); v4 = sv * U_(rng, 1.3, 1.6); pl_add(p, FX(f, -L * 0.95, v4));
        S_rod(rd, p, 0.34, 1.0, 0.0, 0.0); ob_st(ob, rd, NULL, -1, NULL, lnb(p, 0.4));
    }
    OUT_;
}
BODY(xa_rib_arches) {
    FF f = ff_(cx, cy, a); SoObj *ob = obj_new("bone"); double L = U_(rng, 5.0, 6.0); int n = (int)RI(rng, 4, 6), i, sv;
    for (i = 0; i < n + 2; i++) { double x = -L / 2 + L * i / (n + 1); SoSolids *s = sl_new(); S_dome(s, FX(f, x, 0), 0.5, 0.65, 0.0, 0.8, a, 20); ob_st(ob, s, NULL, -1, NULL, disc(FX(f, x, 0), 0.5, 20)); }
    for (i = 0; i < n; i++) {
        double x = -L / 2 + L * (i + 1) / (n + 1);
        for (sv = -1; sv <= 1; sv += 2) {
            PTS *p; SoSolids *rd; double v3, v4;
            if (RR(rng) < 0.15) continue;
            p = pl_new(); rd = sl_new();
            pl_add(p, FX(f, x, sv * 0.4)); pl_add(p, FX(f, x + 0.3, sv * 1.3));
            v3 = sv * U_(rng, 2.0, 2.4); pl_add(p, FX(f, x + 0.2, v3)); v4 = sv * U_(rng, 2.6, 3.0); pl_add(p, FX(f, x - 0.2, v4));
            S_rod(rd, p, 0.26, 1.0, 0.0, 0.0); ob_st(ob, rd, NULL, -1, NULL, lnb(p, 0.3));
        }
    }
    { double ch = CH(rng, -1, 1), v = ch * U_(rng, 0.5, 1.2), x = fx(&f, L / 2 - 0.2, v), y = fy(&f, L / 2 - 0.2, v), r = U_(rng, 1.2, 1.6), h = U_(rng, 0.6, 0.8); xa_drift(ob, x, y, r, h, a + 1.57, rng, 0.0); }
    OUT_;
}
static const char *SCRAPK[3] = { "ashscrap", "ashhull", "rust" };
BODY(xa_scrap_heap) {
    SoObj *ob = obj_new("ashscrap"); int i, n = (int)RI(rng, 3, 5), k;
    for (i = 0; i < n; i++) {
        double d = U_(rng, 0.2, 1.6), tt = a + U_(rng, 0, 6.28), p0 = polx(cx, d, tt), p1 = poly_(cy, d, tt), t = U_(rng, 0, 3.14); PTS *p = pl_new(); G *g; int v;
        for (k = 0; k < 4; k++) { double r = U_(rng, 0.9, 1.4), aa = t + k * 1.5708 + U_(rng, -0.3, 0.3); pl_add(p, POL(p0, p1, r, aa)); }
        g = buf(poly(p), 0, 16); v = shp_is_valid(g); if (v < 0) ns_raise();
        if (v && area(g) > 0.8) { double h = U_(rng, 0.6, 1.6); int c = (int)RB(rng, 3); ob_up(ob, g, h, SCRAPK[c]); }
    }
    { double p0 = polx(cx, 1.6, a + 1.57), p1 = poly_(cy, 1.6, a + 1.57), q0 = polx(cx, 1.6, a - 1.57), q1 = poly_(cy, 1.6, a - 1.57);
      ob_st(ob, xa_bar(POL(p0, p1, 1.0, a), POL(q0, q1, 1.0, a), NULL, NULL, 0.45, 0.5), "ashpipe", -1, NULL, seg2(p0, p1, q0, q1, 0.5)); }
    { double r = U_(rng, 1.8, 2.2), h = U_(rng, 0.9, 1.2); xa_drift(ob, POL(cx, cy, 1.8, a + PI), r, h, a + 1.57, rng, 0.0); }
    OUT_;
}
BODY(xa_rebar_slab) {
    FF f = ff_(cx, cy, a); SoObj *ob = obj_new("ashstone"); G *g; int b, i; double h, vs[3] = { -0.6, 0.0, 0.6 };
    { double r = U_(rng, 1.6, 2.0); g = stone(cx, cy, r, rng, 0.75, 7); } h = U_(rng, 0.6, 0.8); b = ob_up(ob, g, h, NULL);
    ob_cut(ob, inter(wavy(POL(cx, cy, 2.2, a + 0.4), POL(cx, cy, 2.2, a + 3.4), rng, 0.3, 4, 0.16), inset(g, 0.2)), b, 0.2);
    for (i = 0; i < 3; i++) {
        double p0 = fx(&f, 1.2, vs[i]), p1 = fy(&f, 1.2, vs[i]), u = U_(rng, 2.4, 3.2), v = vs[i] + U_(rng, -0.6, 0.6), q0 = fx(&f, u, v), q1 = fy(&f, u, v); PTS *p = pl_new(); SoSolids *rd = sl_new();
        pl_add(p, p0, p1); pl_add(p, q0, q1); S_rod(rd, p, 0.3, 1.0, 0.0, 0.0); ob_st(ob, rd, "rust", -1, NULL, lnb(p, 0.32));
    }
    OUT_;
}
BODY(xa_cinder_rocks) {
    SoObj *ob = obj_new("ashcinder"); int i, n;
    { double r = U_(rng, 0.9, 1.2), h = U_(rng, 0.8, 1.1); SoSolids *s = sl_new(); S_rock(s, cx, cy, r, h, rng, 0.0, 0.0, 9); ob_st(ob, s, NULL, -1, NULL, disc(cx, cy, 1.1, 20)); }
    n = (int)RI(rng, 3, 5);
    for (i = 0; i < n; i++) { double d = U_(rng, 1.4, 2.4), t = a + U_(rng, 0, 6.28), p0 = polx(cx, d, t), p1 = poly_(cy, d, t), r = U_(rng, 0.4, 0.6), h = U_(rng, 0.35, 0.55); SoSolids *s = sl_new(); S_rock(s, p0, p1, r, h, rng, 0.0, 0.0, 9); ob_st(ob, s, NULL, -1, NULL, disc(p0, p1, 0.6, 20)); }
    OUT_;
}
BODY(xa_ash_dune) {
    FF f = ff_(cx, cy, a); double L = U_(rng, 3.0, 4.0), h = U_(rng, 0.8, 1.1), vs[2] = { -0.9, 0.9 }; SoObj *ob = obj_new("ashdrift"); int i;
    { double sq = U_(rng, 0.45, 0.6); SoSolids *s = sl_new(); S_dome(s, cx, cy, L / 2, h, 0.0, sq, a, 20); ob_st(ob, s, NULL, -1, NULL, ell(cx, cy, L / 2, L / 4, a, 16)); }
    for (i = 0; i < 2; i++) { double u = U_(rng, -0.3, 0.3), x = fx(&f, u, vs[i] * L * 0.3), y = fy(&f, u, vs[i] * L * 0.3), aa = a + U_(rng, -0.1, 0.1); SoSolids *s = sl_new();
        S_loaf(s, x, y, aa, L * 0.45, 0.7, h * 0.45, 0.0); ob_st(ob, s, NULL, -1, NULL, rect(FX(f, 0, vs[i] * L * 0.3), L * 0.45, 0.7, a)); }
    OUT_;
}
BODY(xa_ripple_patch) {
    FF f = ff_(cx, cy, a); double L = U_(rng, 3.4, 4.4); int n = (int)RI(rng, 4, 5), i; SoSolids *sols = sl_new();
    for (i = 0; i < n; i++) {
        double v = (i - (n - 1) / 2.0) * 0.9, l = L * (1.0 - 0.25 * fabs(i - (n - 1) / 2.0) / n), u = U_(rng, -0.3, 0.3), x = fx(&f, u, v), y = fy(&f, u, v), aa = a + U_(rng, -0.08, 0.08), h = U_(rng, 0.35, 0.45);
        S_loaf(sols, x, y, aa, l, 0.85, h, 0.0);
    }
    *foot = rect(cx, cy, L, n * 0.9, a);
    ns_st(o, sols, "ashdrift", 0, 0, NULL);
    return 0;
}
BODY(xa_hardpan_cracks) {
    double R = U_(rng, 1.6, 2.2), px[8], py[8]; int n = (int)RI(rng, 5, 7), k; GL *cs = gl_new(); G *g;
    for (k = 0; k < n; k++) { double r = R * U_(rng, 0.8, 1.1), t = a + k * 6.2832 / n; px[k] = polx(cx, r, t); py[k] = poly_(cy, r, t); }
    for (k = 0; k < n; k++) { int j = (k + 1) % n; gl_add(cs, buf(line(wiggle(px[k], py[k], px[j], py[j], rng, 0.25, 3)), 0.13, 4)); }
    for (k = 0; k < n; k++) {
        if (RR(rng) < 0.6) { double r = R * U_(rng, 1.8, 2.4), t = ns_atan2(py[k] - cy, px[k] - cx) + U_(rng, -0.3, 0.3); gl_add(cs, buf(line(wiggle(px[k], py[k], POL(cx, cy, r, t), rng, 0.3, 3)), 0.13, 4)); }
    }
    g = U(cs); *foot = chull(g); ns_gcut(o, g, 0.3, "crack"); return 0;
}
BODY(xa_tread_marks) {
    FF f = ff_(cx, cy, a); double L = U_(rng, 5.0, 6.5), w = U_(rng, 0.9, 1.1), xs[64]; GL *gs = gl_new(); G *g; int sv, i, n;
    for (sv = -1; sv <= 1; sv += 2) {
        double v = sv * U_(rng, 1.0, 1.2); G *band = rect(FX(f, 0, v), L, w, a), *gap2; GL *gl = gl_new();
        n = ns_arange(-L / 2 + 0.4, L / 2, 0.75, xs, 64); for (i = 0; i < n; i++) gl_add(gl, rect(FX(f, xs[i], v), 0.3, w + 0.2, a));
        if (RR(rng) < 0.6) { double u = U_(rng, -L / 3, L / 3), l = U_(rng, 0.8, 1.4); gap2 = rect(FX(f, u, v), l, w + 0.2, a); } else gap2 = polyv(NULL, 0);
        gl_add(gs, dif(dif(band, U(gl)), gap2));
    }
    g = U(gs); *foot = chull(g); ns_gcut(o, g, 0.22, "rut"); return 0;
}
static double wreck_rf(const void *c, double s) { (void)c; return s < 0.88 ? 0.6 : 0.78; }
BODY(xa_drift_wreck) {
    FF f = ff_(cx, cy, a); SoObj *ob = obj_new("ashdrift"); double L = U_(rng, 3.0, 3.8); int k = (int)RI(rng, 0, 2), i, n;
    if (k == 0) {
        double xy[8] = { FX(f, L * 0.1, -1.1), FX(f, L * 0.55, -0.9), FX(f, L * 0.62, 0.7), FX(f, L * 0.2, 1.0) }; G *g = buf(polyv(xy, 4), 0, 16); double h = U_(rng, 1.0, 1.4);
        ob_up(ob, g, h, "ashhull");
    } else if (k == 1) {
        double p0 = fx(&f, -0.2, 0), p1 = fy(&f, -0.2, 0), v = U_(rng, -0.3, 0.3), q0 = fx(&f, L * 0.62, v), q1 = fy(&f, L * 0.62, v);
        ob_st(ob, xa_bar(p0, p1, q0, q1, wreck_rf, NULL, 0, 0.5), "ashpipe", -1, NULL, seg2(p0, p1, q0, q1, 0.6));
    } else {
        double c0 = fx(&f, L * 0.35, 0), c1 = fy(&f, L * 0.35, 0); G *g = ann(c0, c1, 1.15, 0.6, 20); double h = U_(rng, 0.8, 1.0); ob_st(ob, ext1(g, h), "rust", -1, NULL, disc(c0, c1, 1.15, 20));
    }
    { double h = U_(rng, 0.8, 1.1), sq = U_(rng, 0.55, 0.7); xa_drift(ob, FX(f, -L * 0.12, 0), L / 2, h, a, rng, sq); }
    n = (int)RI(rng, 1, 3);
    for (i = 0; i < n; i++) { double u = U_(rng, -L * 0.4, L * 0.5), ch = CH(rng, -1, 1), v = ch * U_(rng, 1.2, 1.7), p0 = fx(&f, u, v), p1 = fy(&f, u, v), r = U_(rng, 0.35, 0.5), h = U_(rng, 0.3, 0.45); SoSolids *s = sl_new();
        S_rock(s, p0, p1, r, h, rng, 0.0, 0.0, 9); ob_st(ob, s, "ashcinder", -1, NULL, disc(p0, p1, 0.5, 20)); }
    OUT_;
}
BODY(xa_scrap_litter) {
    SoObj *ob = obj_new("ashscrap"); int i, n = (int)RI(rng, 3, 5), k;
    for (i = 0; i < n; i++) {
        double d = U_(rng, 0.3, 2.2), tt = U_(rng, 0, 6.28), p0 = polx(cx, d, tt), p1 = poly_(cy, d, tt), t0 = U_(rng, 0, 3.14), r = U_(rng, 0.6, 0.95); PTS *p = pl_new(); G *g; int v;
        for (k = 0; k < 4; k++) { double rr = r * U_(rng, 0.7, 1.2), aa = t0 + k * 1.5708 + U_(rng, -0.35, 0.35); pl_add(p, POL(p0, p1, rr, aa)); }
        g = buf(poly(p), 0, 16); v = shp_is_valid(g); if (v < 0) ns_raise();
        if (v && area(g) > 0.6) { double h = U_(rng, 0.35, 0.7); int c = (int)RB(rng, 3); ob_up(ob, g, h, SCRAPK[c]); }
    }
    n = (int)RI(rng, 1, 2);
    for (i = 0; i < n; i++) {
        double d = U_(rng, 0.5, 1.8), t = U_(rng, 0, 6.28), p0 = polx(cx, d, t), p1 = poly_(cy, d, t), d2 = U_(rng, 1.6, 2.4), t2 = U_(rng, 0, 6.28), q0 = polx(p0, d2, t2), q1 = poly_(p1, d2, t2); SoSolids *rd = sl_new();
        S_rod(rd, wiggle(p0, p1, q0, q1, rng, 0.25, 3), 0.26, 1.0, 0.0, 0.0); ob_st(ob, rd, "rust", -1, NULL, seg2(p0, p1, q0, q1, 0.3));
    }
    n = (int)RI(rng, 2, 3);
    for (i = 0; i < n; i++) { double d = U_(rng, 1.0, 2.6), t = U_(rng, 0, 6.28), p0 = polx(cx, d, t), p1 = poly_(cy, d, t), r = U_(rng, 0.35, 0.5), h = U_(rng, 0.3, 0.45); SoSolids *s = sl_new();
        S_rock(s, p0, p1, r, h, rng, 0.0, 0.0, 9); ob_st(ob, s, "ashcinder", -1, NULL, disc(p0, p1, 0.5, 20)); }
    OUT_;
}

#define SD(fn, cls, kind, name) static const NsSd D_##fn = { fn, "ashveil", cls, kind, name, 0 };
SD(xa_buried_cab, "made", "ashhull", "buried_cab") SD(xa_track_links, "made", "ashhull", "track_links") SD(xa_hub_wheel, "made", "ashhull", "hub_wheel")
SD(xa_cargo_container, "made", "ashhull", "cargo_container") SD(xa_buried_hatch, "made", "ashhull", "buried_hatch") SD(xa_stilt_pipe, "made", "ashpipe", "stilt_pipe")
SD(xa_valve_head, "made", "ashpipe", "valve_head") SD(xa_toppled_drums, "made", "ashpipe", "toppled_drums") SD(xa_fallen_pylon, "made", "ashpylon", "fallen_pylon")
SD(xa_pylon_stub, "made", "ashpylon", "pylon_stub") SD(xa_relay_mast, "made", "ashpylon", "relay_mast") SD(xa_warning_sign, "made", "ashpylon", "warning_sign")
SD(xa_cell_panel, "made", "ashpylon", "cell_panel") SD(xa_lean_to, "made", "ashtarp", "lean_to") SD(xa_ash_still, "made", "ashtarp", "ash_still")
SD(xa_salvage_sled, "made", "ashtarp", "salvage_sled") SD(xa_breath_mask, "made", "ashtarp", "breath_mask") SD(xa_visor_helmet, "made", "ashtarp", "visor_helmet")
SD(xa_grazer_skull, "made", "bone", "grazer_skull") SD(xa_rib_arches, "made", "bone", "rib_arches") SD(xa_scrap_heap, "made", "ashscrap", "scrap_heap")
SD(xa_rebar_slab, "debris", "ashstone", "rebar_slab") SD(xa_cinder_rocks, "debris", "ashcinder", "cinder_rocks") SD(xa_ash_dune, "debris", "ashdrift", "ash_dune")
SD(xa_ripple_patch, "debris", "ashdrift", "ripple_patch") SD(xa_hardpan_cracks, "marks", "crack", "hardpan_cracks") SD(xa_tread_marks, "marks", "rut", "tread_marks")
SD(xa_drift_wreck, "debris", "ashdrift", "drift_wreck") SD(xa_scrap_litter, "debris", "ashscrap", "scrap_litter")
void ns_reg_ashveil(void) {
    const NsSd *all[] = { &D_xa_buried_cab, &D_xa_track_links, &D_xa_hub_wheel, &D_xa_cargo_container, &D_xa_buried_hatch, &D_xa_stilt_pipe, &D_xa_valve_head,
        &D_xa_toppled_drums, &D_xa_fallen_pylon, &D_xa_pylon_stub, &D_xa_relay_mast, &D_xa_warning_sign, &D_xa_cell_panel, &D_xa_lean_to, &D_xa_ash_still,
        &D_xa_salvage_sled, &D_xa_breath_mask, &D_xa_visor_helmet, &D_xa_grazer_skull, &D_xa_rib_arches, &D_xa_scrap_heap, &D_xa_rebar_slab, &D_xa_cinder_rocks,
        &D_xa_ash_dune, &D_xa_ripple_patch, &D_xa_hardpan_cracks, &D_xa_tread_marks, &D_xa_drift_wreck, &D_xa_scrap_litter };
    int i; for (i = 0; i < (int)(sizeof all / sizeof *all); i++) ns_sd_reg(all[i], 10.5, all[i]->body == xa_buried_hatch);
}
