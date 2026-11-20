/* bld_ns_ferrum.c: style_ferrum.py's own builders (b_sunk_gear, b_vent_cowl, b_pipe_stub) and the 28 style-only
   details of sd_r_x_ferrum.py, copied routine by routine (each names the Python it copies). See bld_ns.h. */
#include "bld_ns.h"

/* style_ferrum._gear_poly(cx, cy, R, n, a, td=0.6) */
static G *gear_poly(double cx, double cy, double R, int n, double a, double td) {
    double tw = 3.1416 * R / n; GL *l = gl_new(); int k;
    tw = tw < 0.85 ? tw : 0.85; tw = tw > 0.55 ? tw : 0.55;          /* max(0.55, min(0.85, ...)) */
    gl_add(l, PB(cx, cy, R - td * 0.75, 3 * n));
    for (k = 0; k < n; k++) {
        double t = a + 6.2832 * k / n;
        gl_add(l, rect(cx + (R - td * 0.45) * ns_cos(t), cy + (R - td * 0.45) * ns_sin(t), td * 1.1, tw, t));
    }
    return uu(l);
}

typedef struct { const char *kind; double r0, r1; int which; } FeCtx;
static const FtTiers DUST_T = { 2, { { { 0.55, NUM_FLOAT, 0 }, { 25, NUM_INT, 0 } }, { { 0.45, NUM_FLOAT, 0 }, { 50, NUM_INT, 0 } } } };

/* b_sunk_gear(kind="iron", R=(1.8, 2.5)).f(cx, cy, a, rng) */
static int sunk_gear(const FeCtx *c, double cx, double cy, double a, PyRandom *rng, NsOut *o, G **foot) {
    double Rr = U_(rng, c->r0, c->r1), t, dx, dy, h; int n = (int)RI(rng, 11, 15); G *g = gear_poly(cx, cy, Rr, n, a, 0.6), *dust, *rg;
    t = a + U_(rng, 0, 6.28); dx = cx + Rr * 0.65 * ns_cos(t); dy = cy + Rr * 0.65 * ns_sin(t);
    dust = PB(dx, dy, Rr * 0.85, 16);
    rg = dif(PB(cx, cy, Rr - 0.9, 24), PB(cx, cy, Rr - 1.2, 24));
    *foot = uni(PB(cx, cy, Rr + 0.3, 16), dust);
    h = U_(rng, 0.5, 0.65); ns_up(o, g, h, c->kind);
    ns_tcut(o, uni(rg, PB(cx, cy, 0.45, 10)), 0, 0.3);
    h = U_(rng, 0.9, 1.2); ns_mound(o, dust, h, "dust", &DUST_T);
    return 0;
}
/* b_vent_cowl(kind="iron").f */
static int vent_cowl(const FeCtx *c, double cx, double cy, double a, PyRandom *rng, NsOut *o, G **foot) {
    double r = U_(rng, 0.9, 1.2), h;
    *foot = PB(cx, cy, r + 0.5, 16);
    ns_up(o, PB(cx, cy, r + 0.4, 20), 0.35, c->kind);
    { G *g = PB(cx, cy, r, 20); h = U_(rng, 1.6, 2.4); ns_on(o, g, 0, h, c->kind); }
    ns_tcut(o, PB(cx, cy, r - 0.42, 16), 1, 0.6);
    return 0;
}
/* b_pipe_stub(kind="pipe").f */
static int pipe_stub(const FeCtx *c, double cx, double cy, double a, PyRandom *rng, NsOut *o, G **foot) {
    double L = U_(rng, 4.5, 6.0), w = U_(rng, 1.0, 1.3); FF f = ff_(cx, cy, a); G *pipe, *fl, *b1, *b2; GL *all = gl_new(), *bl = gl_new();
    pipe = rect(cx, cy, L, w, a); fl = rect(FX(f, L / 2 - 0.3, 0), 0.5, w + 0.5, a);
    b1 = rect(FX(f, -L * 0.3, 0), 0.9, w + 0.7, a); b2 = rect(FX(f, L * 0.25, 0), 0.9, w + 0.7, a);
    gl_add(all, pipe); gl_add(all, fl); gl_add(all, b1); gl_add(all, b2); gl_add(bl, b1); gl_add(bl, b2);
    *foot = chull(uu(all));
    ns_up(o, uu(bl), 0.55, "iron"); ns_up(o, pipe, w * 0.9, c->kind); ns_up(o, fl, w * 0.9 + 0.15, c->kind);
    return 0;
}
static int fe_call(void *ctx, double cx, double cy, double a, PyRandom *rng, int has_scale, double scale, Geo **foot, SoLayers *layers) {
    const FeCtx *c = (const FeCtx *)ctx; SoLayers lay = { 0 }; NsOut o = { &lay }; G *f = NULL; int r, i;
    *foot = NULL;
    if (has_scale) return SC_ERR;          /* f(cx, cy, a, rng): TypeError */
    {
        NsFrame fr_; ns_enter(&fr_);
        if (setjmp(fr_.jb)) { ns_leave(&fr_); lyl_free(&lay); return SC_ERR; }
        r = c->which == 0 ? sunk_gear(c, cx, cy, a, rng, &o, &f) : c->which == 1 ? vent_cowl(c, cx, cy, a, rng, &o, &f) : pipe_stub(c, cx, cy, a, rng, &o, &f);
        *foot = f ? geo_new(shp_clone(f)) : NULL;
        ns_leave(&fr_);
    }
    for (i = 0; i < lay.n; i++) lyl_push(layers, lay.v[i]);
    lyl_free(&lay);
    return r;
}
static int fe_make(const FtCall *call, FtBuildFn *fn, void **ctx, int which, const char *k0) {
    FeCtx *c = (FeCtx *)calloc(1, sizeof *c);
    c->which = which; c->kind = so_intern(ns_arg_s(call, 0, "kind", k0));
    if (which == 0 && ns_arg_pair(call, 1, "R", 1.8, 2.5, &c->r0, &c->r1) < 0) { free(c); return SC_ERR; }
    bld_keep(c, free); *fn = fe_call; *ctx = c;
    return 0;
}
static int f_sunk_gear(const FtCall *c, FtBuildFn *fn, void **x) { return fe_make(c, fn, x, 0, "iron"); }
static int f_vent_cowl(const FtCall *c, FtBuildFn *fn, void **x) { return fe_make(c, fn, x, 1, "iron"); }
static int f_pipe_stub(const FtCall *c, FtBuildFn *fn, void **x) { return fe_make(c, fn, x, 2, "pipe"); }
void ns_bld_ferrum(void) {
    bld_add("style_ferrum", "b_sunk_gear", f_sunk_gear); bld_add("style_ferrum", "b_vent_cowl", f_vent_cowl); bld_add("style_ferrum", "b_pipe_stub", f_pipe_stub);
    bld_add("style_ferrum", "sunk_gear", f_sunk_gear); bld_add("style_ferrum", "vent_cowl", f_vent_cowl); bld_add("style_ferrum", "pipe_stub", f_pipe_stub);          /* FT_NEW's names */
}

/* ================================================================== sd_r_x_ferrum.py */
#define BODY(nm) static int nm(double cx, double cy, double a, PyRandom *rng, NsOut *o, G **foot)
static const FtTiers XF_SOFT = { 2, { { { 0.55, NUM_FLOAT, 0 }, { 25, NUM_INT, 0 } }, { { 0.45, NUM_FLOAT, 0 }, { 50, NUM_INT, 0 } } } };

/* xf_gear(cx, cy, R, n, a, td=0.6, tw=None) */
static G *xf_gear(double cx, double cy, double R, int n, double a, double td, double tw) {
    GL *l = gl_new(); int k;
    if (tw == 0.0) { tw = pmax(0.5, pmin(0.85, 3.1416 * R / n)); }
    gl_add(l, disc(cx, cy, R - td * 0.75, 3 * n > 24 ? 3 * n : 24));
    for (k = 0; k < n; k++) { double t = a + 6.2832 * k / n; gl_add(l, rect(POL(cx, cy, R - td * 0.45, t), td * 1.1, tw, t)); }
    return U(l);
}
/* xf_edge(cx, cy, a, prof, T): an upright plate on its edge (one solid) */
typedef struct { G *prof; FF f; double x0, x1, top, T; } Edge;
static void edge_free(void *p) { Edge *e = (Edge *)p; shp_free(e->prof); free(e); }
static int edge_sect(void *c, double z, G **out) {
    const Edge *e = (const Edge *)c; GL *ps, *l; int i; G *r = NULL;
    *out = NULL;
    if (z >= e->top) return SO_OK;
    { NS_BEGIN
      l = gl_new(); ps = polys(inter(e->prof, box_(e->x0 - 1, z, e->x1 + 1, z + 0.002)));
      for (i = 0; i < ps->n; i++) {
          double b[4]; bounds(ps->v[i], b);
          if (b[2] - b[0] >= 0.25) { PTS *p = pl_new(); pl_add(p, FX(e->f, b[0], -e->T / 2)); pl_add(p, FX(e->f, b[2], -e->T / 2)); pl_add(p, FX(e->f, b[2], e->T / 2)); pl_add(p, FX(e->f, b[0], e->T / 2)); gl_add(l, poly(p)); }
      }
      if (l->n) r = U(l);
      *out = r ? shp_clone(r) : NULL;
      NS_END(SO_OK); }
}
static void xf_edge(SoSolids *sol, double cx, double cy, double a, const G *prof, double T) {
    Edge e; double b[4];
    bounds(prof, b);
    e.prof = shp_clone(prof); e.f = ff_(cx, cy, a); e.x0 = b[0]; e.x1 = b[2]; e.top = b[3]; e.T = T;
    sl_custom_f(sol, edge_sect, &e, sizeof e, e.top, edge_free);
}
/* xf_dust(o, x, y, r, h, rng, sq=0.7, a=0.0) */
static void xf_dust(SoObj *ob, double x, double y, double r, double h, PyRandom *rng, double sq, double a) {
    G *g = buf(ell(x, y, r, r * sq, a, 16), 0.0, 16); GL *l = gl_new(); double t = a + U_(rng, 0, 6.28);
    gl_add(l, g); gl_add(l, blob(POL(x, y, r * 0.5, t), r * 0.55, rng, 4));
    ob_raw_mound(ob, U(l), h, "dust", &XF_SOFT);
}
/* xf_chain(pts, w=0.38, step=0.62): solids for ST() */
static SoSolids *xf_chain(PTS *pts, double w, double step) {
    G *ln = line(pts); SoSolids *out = sl_new(); double L = len_(ln), s = 0.0; int k = 0;
    while (s + step <= L + 1e-6) {
        G *p = interp(ln, s), *q = interp(ln, pmin(L, s + step)); double t = ns_atan2(gy(q) - gy(p), gx(q) - gx(p)), mx = (gx(p) + gx(q)) / 2, my = (gy(p) + gy(q)) / 2;
        if (k % 2 == 0) S_ext(out, dif(ell(mx, my, step * 0.85, 0.42, t, 14), ell(mx, my, step * 0.85 - w, 0.42 - w * 0.75, t, 12)), 0.3, 0.0);
        else S_ext(out, barl(POL(mx, my, -step * 0.75, t), POL(mx, my, step * 0.75, t), w), 0.52, 0.0);
        s += step; k++;
    }
    return out;
}
static G *lnbuf(PTS *p, double d) { return buf(line(p), d, 16); }
static PTS *corners4(const FF *f, double hx, double hy) { PTS *p = pl_new(); int sx, sy; for (sx = -1; sx <= 1; sx += 2) for (sy = -1; sy <= 1; sy += 2) pl_add(p, fx(f, sx * hx, sy * hy), fy(f, sx * hx, sy * hy)); return p; }

BODY(xf_half_gear) {
    double R = U_(rng, 2.6, 3.3), h, t0, t, d, rr, hh; int n = (int)RI(rng, 14, 18), k, nk, b, nh, hb; SoObj *ob = obj_new("iron"); G *g; GL *l;
    g = xf_gear(cx, cy, R, n, a, 0.6, 0.0);
    nk = (int)RI(rng, 0, 2);
    for (k = 0; k < nk; k++) { double tt = a + 6.2832 * (RB(rng, n) + 0.0) / n; g = dif(g, disc(POL(cx, cy, R + 0.1, tt), 0.5, 8)); }
    h = U_(rng, 0.55, 0.7); b = ob_up(ob, g, h, NULL);
    ob_cut(ob, ann(cx, cy, R - 0.8, R - 1.12, 28), b, 0.18);
    nh = (int)CH(rng, 4, 5, 6); t0 = U_(rng, 0, 6.28);
    l = gl_new(); for (k = 0; k < nh; k++) gl_add(l, disc(POL(cx, cy, R * 0.55, t0 + 6.2832 * k / nh), R * 0.16, 12));
    ob_cut(ob, U(l), b, 0.35);
    hb = ob_on(ob, disc(cx, cy, R * 0.3 + 0.25, 18), b, 0.35, NULL);
    l = gl_new(); gl_add(l, disc(cx, cy, 0.38, 12)); gl_add(l, rect(POL(cx, cy, 0.42, t0), 0.3, 0.3, t0)); ob_cut(ob, U(l), hb, 0.3);
    t = a + U_(rng, 0, 6.28); d = R * U_(rng, 0.55, 0.8);
    rr = R * U_(rng, 0.85, 1.05); hh = U_(rng, 1.0, 1.3);
    xf_dust(ob, POL(cx, cy, d, t), rr, hh, rng, 0.6, t + 1.5708);
    ns_obj_out(o, ob, NULL, foot); return 0;
}
BODY(xf_gear_on_edge) {
    double R = U_(rng, 1.8, 2.3), T, zc, a2; int n = (int)RI(rng, 12, 16), sv; SoObj *ob = obj_new("iron"); FF f = ff_(cx, cy, a); G *prof; SoSolids *sol = sl_new();
    T = U_(rng, 0.8, 0.95); zc = R * U_(rng, 0.15, 0.35);
    a2 = U_(rng, 0, 1); prof = xf_gear(0.0, zc, R, n, a2, 0.6, 0.0);
    xf_edge(sol, cx, cy, a, prof, T); xf_edge(sol, cx, cy, a, disc(0.0, zc, 0.8, 16), T + 0.8);
    ob_st(ob, sol, NULL, -1, NULL, rect(cx, cy, 2 * R + 0.4, T + 0.9, a));
    for (sv = -1; sv <= 1; sv += 2) {
        double rr = R * U_(rng, 0.6, 0.8), u = U_(rng, -R * 0.3, R * 0.3), v = sv * (T / 2 + rr * 0.38), h;
        h = U_(rng, 0.6, 0.85); xf_dust(ob, FX(f, u, v), rr, h, rng, 0.45, a);
    }
    ns_obj_out(o, ob, NULL, foot); return 0;
}
BODY(xf_gear_pair) {
    FF f = ff_(cx, cy, a); double R1 = U_(rng, 1.5, 1.8), R2 = U_(rng, 0.9, 1.15), c1x, c1y, c2x, c2y; SoObj *ob = obj_new("iron"); int n1 = (int)RI(rng, 13, 16), n2, b, g1, g2, sv, i; GL *l;
    n2 = (int)ns_round(n1 * R2 / R1); if (n2 < 8) n2 = 8;
    c1x = fx(&f, -R2 * 0.55, 0); c1y = fy(&f, -R2 * 0.55, 0); c2x = fx(&f, R1 + R2 - 0.5 - R2 * 0.55, 0); c2y = fy(&f, R1 + R2 - 0.5 - R2 * 0.55, 0);
    l = gl_new(); gl_add(l, disc(c1x, c1y, R1 + 0.35, 20)); gl_add(l, disc(c2x, c2y, R2 + 0.35, 16)); b = ob_up(ob, hull(l), 0.35, NULL);
    l = gl_new(); for (sv = -1; sv <= 1; sv += 2) gl_add(l, disc(FX(f, -R2 * 0.55 - R1 - 0.05, sv * 0.5), 0.2, 8)); ob_cut(ob, U(l), b, 0.2);
    g1 = ob_on(ob, xf_gear(c1x, c1y, R1, n1, a, 0.5, 0.0), b, 0.4, NULL); g2 = ob_on(ob, xf_gear(c2x, c2y, R2, n2, a + 3.1416 / n2, 0.5, 0.0), b, 0.4, NULL);
    for (i = 0; i < 2; i++) {
        double x = i ? c2x : c1x, y = i ? c2y : c1y, r = i ? R2 : R1; int g = i ? g2 : g1, k;
        k = ob_on(ob, disc(x, y, r * 0.32 + 0.15, 12), g, 0.3, NULL); ob_cut(ob, ngon(x, y, 0.3, 6, a), k, 0.15);
    }
    ns_obj_out(o, ob, NULL, foot); return 0;
}
BODY(xf_cable_bundle) {
    FF f = ff_(cx, cy, a); double L = U_(rng, 7.0, 9.5), r, A, fq, ph0, xs[41], Px[41], Py[41]; int k = (int)RI(rng, 3, 5), m = 40, i, j, nc; SoObj *ob = obj_new("cable"); PTS *lp = pl_new(); G *ln; SoSolids *sols = sl_new(); int64_t cl[3];
    r = U_(rng, 0.3, 0.36); A = U_(rng, 0.5, 1.1); fq = U_(rng, 0.5, 0.9) * 6.2832 / L; ph0 = U_(rng, 0, 6.28);
    ns_linspace(-L / 2, L / 2, 41, xs);
    for (i = 0; i < 41; i++) pl_add(lp, FX(f, xs[i], A * ns_sin(fq * xs[i] + ph0)));
    ln = line(lp);
    for (i = 0; i <= m; i++) { G *p = T(shp_interpolate(ln, (double)i / m, 1)); Px[i] = gx(p); Py[i] = gy(p); }
    nc = (int)RI(rng, 2, 3);
    if (py_sample_e(rng, (m - 6) - 7, nc, cl) != 0) ns_raise();
    for (i = 0; i < nc; i++) cl[i] += 7;
    for (i = 1; i < nc; i++) { int64_t x = cl[i]; j = i - 1; while (j >= 0 && cl[j] > x) { cl[j + 1] = cl[j]; j--; } cl[j + 1] = x; }
#define NRM(i_, nx_, ny_) do { int a_ = (i_) - 1 > 0 ? (i_) - 1 : 0, b_ = (i_) + 1 < m ? (i_) + 1 : m; double dx_ = Px[b_] - Px[a_], dy_ = Py[b_] - Py[a_], l_ = ns_hypot(dx_, dy_); if (l_ == 0.0) l_ = 1; nx_ = -dy_ / l_; ny_ = dx_ / l_; } while (0)
    for (j = 0; j < k; j++) {
        double off0 = (j - (k - 1) / 2.0) * 2 * r * 1.08, ph = U_(rng, 0, 6.28); PTS *pts = pl_new();
        for (i = 0; i <= m; i++) {
            int64_t dm = -1; int c; double sp, nx, ny;
            for (c = 0; c < nc; c++) { int64_t d = i - cl[c] < 0 ? cl[c] - i : i - cl[c]; if (dm < 0 || d < dm) dm = d; }
            sp = 1.0 + (i < m - 2 ? 0.3 * pmin(1.0, dm / 5.0) * (0.6 + 0.4 * ns_sin(i * 0.45 + ph)) : 1.3 + 0.25 * (i - m + 2));
            NRM(i, nx, ny); pl_add(pts, Px[i] + nx * off0 * sp, Py[i] + ny * off0 * sp);
        }
        S_rod(sols, pts, r, 1.5, 0.0, 0.0);
    }
    for (i = 0; i < nc; i++) { double nx, ny, t; int c = (int)cl[i]; NRM(c, nx, ny); t = ns_atan2(ny, nx); S_ext(sols, rect(Px[c], Py[c], k * 2 * r * 1.08 + 0.55, 0.6, t), 2 * r * 1.5 * 0.5 + 0.22, 0.0); }
#undef NRM
    ob_st(ob, sols, NULL, -1, NULL, buf(ln, k * r * 1.6 + 0.4, 16));
    ns_obj_out(o, ob, NULL, foot); return 0;
}
BODY(xf_cable_drum) {
    FF f = ff_(cx, cy, a); double Rf = U_(rng, 1.6, 1.9), Wd = U_(rng, 2.0, 2.4), ex, ey; SoObj *ob = obj_new("cable"); SoSolids *sols = sl_new(); int sx, i, n; PTS *pts; GL *l = gl_new();
    for (sx = -1; sx <= 1; sx += 2) S_lie(sols, FX(f, sx * (Wd / 2 - 0.2), 0), a, 0.4, Rf, 0.0);
    n = (int)ns_int((Wd - 0.8) / 0.42);
    for (i = 0; i < n; i++) S_lie(sols, FX(f, -Wd / 2 + 0.6 + i * 0.42, 0), a, 0.36, Rf * 0.72, 0.0);
    { double u = U_(rng, -1, 2), v = Rf + U_(rng, 2.5, 3.5); ex = fx(&f, u, v); ey = fy(&f, u, v); }
    pts = wiggle(FX(f, Wd * 0.1, Rf * 0.6), ex, ey, rng, 0.6, 4);
    S_rod(sols, pts, 0.3, 1.5, 0.0, 0.0);
    gl_add(l, rect(cx, cy, Wd, 2 * Rf, a)); gl_add(l, lnbuf(pts, 0.4));
    ob_st(ob, sols, NULL, -1, NULL, U(l));
    ns_obj_out(o, ob, NULL, foot); return 0;
}
BODY(xf_pipe_run) {
    FF f = ff_(cx, cy, a); double L = U_(rng, 7.0, 9.0), r = U_(rng, 0.55, 0.7), vs[2], Wt, xf, xs[2]; SoObj *ob = obj_new("pipe"); int two, nv, i; SoSolids *sols = sl_new();
    two = RR(rng) < 0.5;
    if (two) { vs[0] = -(r + 0.12); vs[1] = r + 0.12; nv = 2; } else { vs[0] = 0.0; nv = 1; }
    Wt = two ? 4 * r + 0.24 : 2 * r;
    xs[0] = -L * 0.32; xs[1] = L * 0.32;
    for (i = 0; i < 2; i++) S_ext(sols, rect(FX(f, xs[i], 0), 0.8, Wt + 0.8, a), 2 * r * 0.85, 0.0);
    xf = U_(rng, -L * 0.15, L * 0.15);
    for (i = 0; i < nv; i++) { S_lie(sols, FX(f, 0, vs[i]), a, L, r, 0.0); S_lie(sols, FX(f, xf, vs[i]), a, 0.45, r + 0.22, 0.0); }
    ob_st(ob, sols, NULL, -1, NULL, rect(cx, cy, L, Wt + 0.8, a));
    if (RR(rng) < 0.55) {
        double ch = CH(rng, -1, 1), u = xf + ch * U_(rng, 1.4, 2.2), c0 = fx(&f, u, vs[0]), c1 = fy(&f, u, vs[0]); int vb, k; GL *l = gl_new(); PTS *p = pl_new();
        vb = ob_up(ob, rect(c0, c1, 1.25, 1.25, a), 2 * r * 1.15 + 0.45, NULL);
        gl_add(l, ann(c0, c1, 0.6, 0.38, 16)); for (k = 0; k < 4; k++) gl_add(l, barl(c0, c1, POL(c0, c1, 0.5, a + 0.8 + 1.5708 * k), 0.28));
        ob_on(ob, U(l), vb, 0.28, NULL); pl_add(p, c0, c1); ob_studs(ob, p, vb, 0.22, 0.45, NULL, 8);
    }
    ns_obj_out(o, ob, NULL, foot); return 0;
}
BODY(xf_pipe_elbow) {
    FF f = ff_(cx, cy, a); double r = U_(rng, 0.6, 0.75), L1 = U_(rng, 3.0, 4.0), L2 = U_(rng, 1.8, 2.8), rb = 1.3, ts[7]; SoObj *ob = obj_new("pipe"); PTS *pts = pl_new(), *p2 = pl_new(); SoSolids *sols = sl_new(); int i, n;
    pl_add(pts, FX(f, -L1, 0)); ns_linspace(0, 1.5708, 7, ts);
    for (i = 0; i < 7; i++) pl_add(pts, FX(f, -rb + rb * ns_sin(ts[i]), rb - rb * ns_cos(ts[i])));
    pl_add(pts, FX(f, 0, L2));
    S_rod(sols, pts, r, 1.6, 0.0, 0.0);
    pl_add(p2, FX(f, -L1 - 0.05, 0)); pl_add(p2, FX(f, -L1 + 0.4, 0)); S_rod(sols, p2, r + 0.3, 1.6 * r / (r + 0.3), 0.0, 0.0);
    ob_st(ob, sols, NULL, -1, NULL, lnbuf(pts, r + 0.35));
    n = (int)RI(rng, 2, 3);
    for (i = 0; i < n; i++) {
        double u = U_(rng, -1.0, 1.2), v = L2 + U_(rng, 0.6, 1.4), c0 = fx(&f, u, v), c1 = fy(&f, u, v), t = U_(rng, 0, 6.28), xy[6], h; G *g;
        xy[0] = polx(c0, 0.55, t); xy[1] = poly_(c1, 0.55, t); xy[2] = polx(c0, 0.5, t + 2.3); xy[3] = poly_(c1, 0.5, t + 2.3); xy[4] = polx(c0, 0.45, t + 4.0); xy[5] = poly_(c1, 0.45, t + 4.0);
        g = polyv(xy, 3); h = U_(rng, 0.25, 0.4); ob_st(ob, ext1(g, h), "rust", -1, NULL, NULL);
    }
    ns_obj_out(o, ob, NULL, foot); return 0;
}
BODY(xf_piston_ram) {
    FF f = ff_(cx, cy, a); double Lc = U_(rng, 3.0, 3.8), Lr = U_(rng, 1.8, 2.6), R = U_(rng, 0.6, 0.72), x0; SoObj *ob = obj_new("iron"); SoSolids *sols = sl_new(); PTS *hs = pl_new(); int i; GL *ex = gl_new();
    x0 = -(Lc + Lr) / 2;
    S_lie(sols, FX(f, x0 + Lc / 2, 0), a, Lc, R, 0.0); S_lie(sols, FX(f, x0 + 0.2, 0), a, 0.4, R + 0.12, 0.0); S_lie(sols, FX(f, x0 + Lc - 0.2, 0), a, 0.4, R + 0.12, 0.0);
    S_lie(sols, FX(f, x0 + Lc + Lr / 2, 0), a, Lr, 0.3, 0.0);
    pl_add(hs, FX(f, x0 + Lc * 0.7, R * 0.7)); pl_add(hs, FX(f, x0 + Lc * 0.5, R + 1.2));
    { double u = x0 + Lc * U_(rng, 0.1, 0.3), v = R + U_(rng, 1.6, 2.2); pl_add(hs, FX(f, u, v)); }
    S_rod(sols, hs, 0.24, 1.5, 0.0, 0.0);
    ob_st(ob, sols, NULL, -1, NULL, rect(cx, cy, Lc + Lr, 2 * R + 0.3, a));
    for (i = 0; i < 2; i++) {
        double x = i ? x0 + Lc + Lr + 0.4 : x0 - 0.45, sx = i ? 1 : -1, c0 = fx(&f, x, 0), c1 = fy(&f, x, 0); GL *l = gl_new(); int e;
        gl_add(l, disc(c0, c1, 0.62, 14)); gl_add(l, rect(FX(f, x - sx * 0.45, 0), 0.6, 0.8, a));
        e = ob_st(ob, ext1(U(l), 0.75), NULL, -1, NULL, NULL); ob_cut(ob, disc(c0, c1, 0.26, 10), e, 0.3);
    }
    gl_add(ex, lnbuf(hs, 0.35));
    ns_obj_out(o, ob, ex, foot); return 0;
}
BODY(xf_vent_stack) {
    FF f = ff_(cx, cy, a); double R = U_(rng, 0.95, 1.2), Hh = U_(rng, 3.8, 4.8), S, v, h; SoObj *ob = obj_new("iron"); int b, fl, s1, s2, bx;
    S = 2 * R + 1.3;
    b = ob_up(ob, rect(cx, cy, S, S, a), 0.4, NULL);
    ob_studs(ob, corners4(&f, S / 2 - 0.35, S / 2 - 0.35), b, 0.24, 0.15, NULL, 8);
    fl = ob_on(ob, disc(cx, cy, R + 0.35, 24), b, 0.3, NULL); s1 = ob_on(ob, disc(cx, cy, R, 24), fl, Hh * 0.55, NULL);
    s2 = ob_on(ob, disc(cx, cy, R - 0.14, 24), s1, Hh * 0.4, NULL); ob_cut(ob, disc(cx, cy, R - 0.5, 20), s2, 0.8);
    v = U_(rng, -0.3, 0.3); h = U_(rng, 1.4, 1.8); bx = ob_up(ob, rect(FX(f, S / 2 + 0.85, v), 1.6, 1.9, a), h, NULL);
    ob_vents(ob, &f, S / 2 + 0.85, 0, bx, 3, 1.1, a, 0.42, 0.2, 0.25);
    if (RR(rng) < 0.45) {
        double v2 = U_(rng, -0.5, 0.5), c0 = fx(&f, -S / 2 - 0.9, v2), c1 = fy(&f, -S / 2 - 0.9, v2), r2 = U_(rng, 0.55, 0.7), h2; int p2, t2;
        p2 = ob_up(ob, disc(c0, c1, r2 + 0.2, 16), 0.3, NULL); { G *g = disc(c0, c1, r2, 16); h2 = Hh * U_(rng, 0.45, 0.6); t2 = ob_on(ob, g, p2, h2, NULL); }
        ob_cut(ob, disc(c0, c1, r2 - 0.3, 12), t2, 0.5);
    }
    ns_obj_out(o, ob, NULL, foot); return 0;
}
BODY(xf_blower_hood) {
    FF f = ff_(cx, cy, a); double L = U_(rng, 3.0, 3.6), W = U_(rng, 2.4, 2.8), R, c0, c1, h; SoObj *ob = obj_new("iron"); G *g = rrect(cx, cy, L, W, a, 0.25); int b, lid, fr, du, k; GL *l = gl_new();
    h = U_(rng, 1.4, 1.7); b = ob_up(ob, g, h, NULL); lid = ob_lid(ob, g, b, 0.15, 0.16, NULL);
    R = pmin(L, W) * 0.36; c0 = fx(&f, -L * 0.08, 0); c1 = fy(&f, -L * 0.08, 0); fr = ob_on(ob, disc(c0, c1, R + 0.2, 24), lid, 0.18, NULL);
    gl_add(l, disc(c0, c1, 0.35, 10)); for (k = 0; k < 3; k++) gl_add(l, barl(POL(c0, c1, -R, a + k * 1.0472), POL(c0, c1, R, a + k * 1.0472), 0.24));
    ob_cut(ob, dif(disc(c0, c1, R - 0.05, 24), U(l)), fr, 0.3);
    ob_vents(ob, &f, L * 0.36, 0, lid, 4, 0.7, a + 1.5708, 0.36, 0.18, 0.2);
    ob_studs(ob, corners4(&f, L / 2 - 0.35, W / 2 - 0.35), lid, 0.2, 0.12, NULL, 8);
    h = U_(rng, 0.9, 1.1); du = ob_up(ob, rect(FX(f, L / 2 + 0.55, 0), 1.1, W * 0.5, a), h, NULL); ob_cut(ob, rect(FX(f, L / 2 + 0.75, 0), 0.6, W * 0.5 - 0.5, a), du, 0.5);
    ns_obj_out(o, ob, NULL, foot); return 0;
}
BODY(xf_gauge_panel) {
    FF f = ff_(cx, cy, a); double L = U_(rng, 3.2, 3.8), W = U_(rng, 2.0, 2.4), h, xs[2]; SoObj *ob = obj_new("iron"); G *g = rrect(cx, cy, L, W, a, 0.2); int b, i, n; PTS *p = pl_new(), *st = pl_new();
    h = U_(rng, 1.0, 1.2); b = ob_up(ob, g, h, NULL); ob_groove(ob, rrect(cx, cy, L - 0.6, W - 0.6, a, 0.15), b, 0.26, 0.14);
    xs[0] = -L * 0.22; xs[1] = L * 0.08;
    for (i = 0; i < 2; i++) {
        double c0 = fx(&f, xs[i], W * 0.12), c1 = fy(&f, xs[i], W * 0.12), t; int gz = ob_on(ob, disc(c0, c1, 0.62, 18), b, 0.2, NULL);
        ob_cut(ob, disc(c0, c1, 0.45, 16), gz, 0.15); t = U_(rng, 0.4, 2.6);
        ob_on(ob, barl(c0, c1, POL(c0, c1, 0.38, a + t), 0.16), gz, 0.05, NULL);
    }
    n = (int)RI(rng, 3, 4); for (i = 0; i < n; i++) pl_add(p, FX(f, -L * 0.3 + i * 0.4, -W * 0.28));
    ob_studs(ob, p, b, 0.17, 0.22, NULL, 8);
    ob_cut(ob, rect(FX(f, L * 0.33, 0), 0.32, W * 0.55, a), b, 0.25);
    { double v = U_(rng, -0.4, 0.4); ob_on(ob, rect(FX(f, L * 0.33, v), 0.5, 0.5, a), b, 0.55, NULL); }
    pl_add(st, FX(f, -L / 2, 0)); { double v = U_(rng, -0.6, 0.6); pl_add(st, FX(f, -L / 2 - 0.8, v)); }
    { double u = -L / 2 - U_(rng, 1.4, 2.0), v = U_(rng, -1.0, 1.0); pl_add(st, FX(f, u, v)); }
    { SoSolids *s = sl_new(); S_rod(s, st, 0.3, 1.5, 0.0, 0.0); ob_st(ob, s, "cable", -1, NULL, lnbuf(st, 0.4)); }
    ns_obj_out(o, ob, NULL, foot); return 0;
}
BODY(xf_walker_foot) {
    FF f = ff_(cx, cy, a); double s = U_(rng, 1.0, 1.2), hx, hy, ts[3] = { -0.62, 0.0, 0.62 }, h; SoObj *ob = obj_new("iron"); G *heel; GL *l = gl_new(), *c = gl_new(); int b, i, j, an, sh; PTS *p = pl_new(), *hs = pl_new();
    hx = fx(&f, -0.6 * s, 0); hy = fy(&f, -0.6 * s, 0);
    heel = rrect(hx, hy, 2.4 * s, 2.3 * s, a, 0.35); gl_add(l, heel);
    for (i = 0; i < 3; i++) { GL *t2 = gl_new(); gl_add(t2, disc(POL(hx, hy, 0.6 * s, a + ts[i]), 0.62 * s, 12)); gl_add(t2, disc(POL(hx, hy, 3.2 * s, a + ts[i]), 0.36 * s, 10)); gl_add(l, hull(t2)); }
    h = U_(rng, 0.6, 0.75); b = ob_up(ob, U(l), h, NULL);
    for (i = 0; i < 3; i++) gl_add(c, barl(POL(hx, hy, 1.4 * s, a + ts[i] - 0.35), POL(hx, hy, 1.4 * s, a + ts[i] + 0.35), 0.16));
    ob_cut(ob, buf(U(c), 0.06, 16), b, 0.14);
    for (i = 0; i < 3; i++) for (j = 0; j < 2; j++) { double r_ = j ? 2.6 : 2.1; pl_add(p, POL(hx, hy, r_ * s, a + ts[i])); }
    ob_studs(ob, p, b, 0.18, 0.12, NULL, 8);
    an = ob_on(ob, disc(hx, hy, 0.95 * s, 18), b, 0.7, NULL);
    { double aa = a + U_(rng, -0.3, 0.3); G *g = rect(hx, hy, 1.15 * s, 1.3 * s, aa); h = U_(rng, 1.2, 2.0); sh = ob_on(ob, g, an, h, NULL); }
    { double xy[6] = { POL(hx, hy, 1.0, a + 0.3), POL(hx, hy, 0.2, a + 1.9), POL(hx, hy, 1.0, a + 3.4) }; G *g = polyv(xy, 3); h = U_(rng, 0.3, 0.6); ob_cut(ob, g, sh, h); }
    pl_add(hs, FX(f, -1.4 * s, 0.5)); { double v = U_(rng, 0.8, 1.6); pl_add(hs, FX(f, -2.4 * s, v)); }
    { double u = -U_(rng, 3.0, 3.8) * s, v = U_(rng, -0.5, 1.5); pl_add(hs, FX(f, u, v)); }
    { SoSolids *sl = sl_new(); S_rod(sl, hs, 0.26, 1.5, 0.0, 0.0); ob_st(ob, sl, "cable", -1, NULL, lnbuf(hs, 0.35)); }
    ns_obj_out(o, ob, NULL, foot); return 0;
}
BODY(xf_walker_leg) {
    double s = U_(rng, 0.9, 1.05), kb, hipx, hipy, ankx, anky, ftx, fty, h; SoObj *ob = obj_new("iron"); int th, sn, kn, hp, sv, k, n; GL *l = gl_new(), *fl = gl_new(); PTS *p = pl_new(); SoSolids *sols = sl_new();
    kb = U_(rng, -0.5, 0.5);
    hipx = polx(cx, -4.2 * s, a); hipy = poly_(cy, -4.2 * s, a); ankx = polx(cx, 3.8 * s, a + kb); anky = poly_(cy, 3.8 * s, a + kb); ftx = polx(cx, 4.9 * s, a + kb); fty = poly_(cy, 4.9 * s, a + kb);
    { G *g = barl(hipx, hipy, cx, cy, 1.7 * s); h = U_(rng, 1.0, 1.15); th = ob_up(ob, g, h, NULL); }
    for (sv = -1; sv <= 1; sv += 2) {
        double ax = polx(hipx, 1.1, a), ay = poly_(hipy, 1.1, a), bx = polx(cx, -0.9, a), by = poly_(cy, -0.9, a);
        gl_add(l, barl(POL(ax, ay, sv * 0.55 * s, a + 1.5708), POL(bx, by, sv * 0.55 * s, a + 1.5708), 0.26));
    }
    ob_cut(ob, U(l), th, 0.14);
    ob_on(ob, barl(POL(hipx, hipy, 0.9, a), POL(cx, cy, -1.0, a), 0.5), th, 0.3, NULL);
    { G *g = barl(cx, cy, ankx, anky, 1.35 * s); h = U_(rng, 0.85, 0.95); sn = ob_up(ob, g, h, NULL); }
    { double ds[3] = { 1.2, 2.0, 2.8 }; int i; for (i = 0; i < 3; i++) pl_add(p, POL(cx, cy, ds[i] * s, a + kb)); }
    ob_studs(ob, p, sn, 0.2, 0.12, NULL, 8);
    { G *g = disc(cx, cy, 1.05 * s, 18); h = U_(rng, 1.35, 1.55); kn = ob_up(ob, g, h, NULL); }
    ob_cut(ob, ann(cx, cy, 0.7 * s, 0.45 * s, 16), kn, 0.2);
    ob_up(ob, rrect(POL(ftx, fty, 0.0, 0.0), 1.6 * s, 2.1 * s, a + kb, 0.3), 0.55, NULL);
    { G *g = disc(hipx, hipy, 1.25 * s, 18); h = U_(rng, 1.2, 1.4); hp = ob_up(ob, g, h, NULL); }
    ob_cut(ob, disc(hipx, hipy, 0.8 * s, 14), hp, 0.5);
    n = (int)RI(rng, 2, 3);
    for (k = 0; k < n; k++) {
        double t = a + 3.1416 + U_(rng, -0.9, 0.9), d = U_(rng, 2.2, 3.2); PTS *w = wiggle(POL(hipx, hipy, 1.2, t), POL(hipx, hipy, d, t), rng, 0.4, 3);
        S_rod(sols, w, 0.24, 1.5, 0.0, 0.0); gl_add(fl, lnbuf(w, 0.32));
    }
    ob_st(ob, sols, "cable", -1, NULL, U(fl));
    ns_obj_out(o, ob, NULL, foot); return 0;
}
static int xf_lin(void *c, double t, double *out) { const double *k = (const double *)c; *out = k[0] - k[1] * t; return 0; }
BODY(xf_crane_hook) {
    FF f = ff_(cx, cy, a); double L = U_(rng, 2.0, 2.4), W = U_(rng, 1.5, 1.8), h, hcx, hcy, arcxy[22], kk[2] = { 0.36, 0.12 }, ex, ey; SoObj *ob = obj_new("iron"); G *g = rrect(cx, cy, L, W, a, 0.3); int b, sv, i; GL *l = gl_new(); PTS *p = pl_new(), *pts = pl_new(), *cp; SoSolids *sols = sl_new();
    h = U_(rng, 1.0, 1.2); b = ob_up(ob, g, h, NULL);
    for (sv = -1; sv <= 1; sv += 2) gl_add(l, rect(FX(f, -0.15, sv * 0.35), L * 0.6, 0.24, a)); ob_cut(ob, U(l), b, 0.3);
    pl_add(p, FX(f, -0.15, 0)); ob_studs(ob, p, b, 0.3, 0.2, NULL, 8);
    hcx = fx(&f, L / 2 + 0.95, 0.15); hcy = fy(&f, L / 2 + 0.95, 0.15);
    pl_add(pts, FX(f, L / 2, 0)); if (sd_arc(hcx, hcy, 0.9, a + 3.1416, a + 3.1416 + 4.6, 10, arcxy) != SO_OK) ns_raise();
    for (i = 0; i <= 10; i++) pl_add(pts, arcxy[2 * i], arcxy[2 * i + 1]);
    S_rodf(sols, pts, xf_lin, kk, 1.5, 0.0, 0.0);
    { double u = -L / 2 - U_(rng, 3.5, 5.0), v = U_(rng, -2.0, 2.0); ex = fx(&f, u, v); ey = fy(&f, u, v); }
    cp = wiggle(FX(f, -L / 2 - 0.1, 0), ex, ey, rng, 0.8, 4);
    ob_st(ob, sols, NULL, -1, NULL, lnbuf(pts, 0.45)); ob_st(ob, xf_chain(cp, 0.38, 0.62), NULL, -1, NULL, lnbuf(cp, 0.5));
    ns_obj_out(o, ob, NULL, foot); return 0;
}
static G *jib_seg(const FF *f, double x0, double x1, double W) {
    GL *gs = gl_new(); int sv, i, n; double st;
    for (sv = -1; sv <= 1; sv += 2) gl_add(gs, barl(fx(f, x0, sv * W / 2), fy(f, x0, sv * W / 2), fx(f, x1, sv * W / 2), fy(f, x1, sv * W / 2), 0.6));
    n = (int)ns_int((x1 - x0) / 1.4); if (n < 1) n = 1; st = (x1 - x0) / n;
    for (i = 0; i < n; i++) { double s0 = (i % 2 ? -1 : 1) * W / 2, s1 = ((i + 1) % 2 ? -1 : 1) * W / 2; gl_add(gs, barl(fx(f, x0 + i * st, s0), fy(f, x0 + i * st, s0), fx(f, x0 + (i + 1) * st, s1), fy(f, x0 + (i + 1) * st, s1), 0.45)); }
    return U(gs);
}
BODY(xf_crane_jib) {
    double L = U_(rng, 7.5, 9.5), W = U_(rng, 1.9, 2.3), kink, xk, xs[64]; SoObj *ob = obj_new("girder"); FF f1, f2; G *body, *gus; GL *l = gl_new(), *gl = gl_new(); int i, n, sv;
    kink = U_(rng, -0.35, 0.35); xk = L * U_(rng, 0.15, 0.3);
    f1 = ff_(cx, cy, a); f2 = ff_(FX(f1, xk, 0), a + kink);
    gl_add(l, jib_seg(&f1, -L / 2, xk, W)); gl_add(l, jib_seg(&f2, 0, L / 2 - xk, W)); body = U(l);
    ob_st(ob, ext1(body, 0.6), NULL, -1, NULL, body);
    n = ns_arange(-L / 2 + 1.4, xk, 2.8, xs, 64);
    for (i = 0; i < n; i++) for (sv = -1; sv <= 1; sv += 2) gl_add(gl, disc(FX(f1, xs[i], sv * W / 2), 0.42, 10));
    gus = U(gl); ob_st(ob, ext1(gus, 0.8), NULL, -1, NULL, gus);
    ns_obj_out(o, ob, NULL, foot); return 0;
}
BODY(xf_girder_stub) {
    double L = U_(rng, 5.0, 6.5), W = U_(rng, 1.6, 1.9), kink, xk, xs[64], h; SoObj *ob = obj_new("girder"); FF f1, f2; GL *l = gl_new(), *fl = gl_new(); G *web, *flg; int b, sv, i, n; PTS *p = pl_new();
    kink = CH(rng, -1, 1); kink *= U_(rng, 0.15, 0.45); xk = L * U_(rng, 0.1, 0.3);
    f1 = ff_(cx, cy, a); f2 = ff_(FX(f1, xk, 0), a + kink);
    gl_add(l, rect(FX(f1, (xk - L / 2) / 2, 0), xk + L / 2, W, a)); gl_add(l, rect(FX(f2, (L / 2 - xk) / 2, 0), L / 2 - xk, W, a + kink)); web = U(l);
    b = ob_up(ob, web, 0.45, NULL);
    for (sv = -1; sv <= 1; sv += 2) gl_add(fl, barl(FX(f1, -L / 2, sv * (W / 2 - 0.25)), FX(f1, xk + 0.1, sv * (W / 2 - 0.25)), 0.5));
    for (sv = -1; sv <= 1; sv += 2) gl_add(fl, barl(FX(f2, -0.1, sv * (W / 2 - 0.25)), FX(f2, L / 2 - xk, sv * (W / 2 - 0.25)), 0.5));
    flg = U(fl); h = U_(rng, 1.0, 1.2); ob_st(ob, ext1(flg, h), NULL, -1, NULL, flg);
    n = ns_arange(-L / 2 + 0.5, xk - 0.3, 0.6, xs, 64); for (i = 0; i < n; i++) pl_add(p, FX(f1, xs[i], 0));
    n = ns_arange(0.4, L / 2 - xk - 0.3, 0.6, xs, 64); for (i = 0; i < n; i++) pl_add(p, FX(f2, xs[i], 0));
    ob_studs(ob, p, b, 0.2, 0.14, NULL, 8);
    ns_obj_out(o, ob, NULL, foot); return 0;
}
BODY(xf_bolt_head) {
    FF f = ff_(cx, cy, a); SoObj *ob = obj_new("iron"); int w, nt, th; double h;
    { double aa = a + U_(rng, -0.3, 0.3); w = ob_up(ob, rect(cx, cy, 2.0, 2.0, aa), 0.3, NULL); }
    nt = ob_on(ob, ngon(cx, cy, 0.85, 6, a), w, 0.6, NULL); { G *g = disc(cx, cy, 0.45, 12); h = U_(rng, 0.6, 1.0); th = ob_on(ob, g, nt, h, NULL); }
    ob_cut(ob, barl(POL(cx, cy, 0.5, a + 0.5), POL(cx, cy, -0.5, a + 0.5), 0.26), th, 0.12);
    if (RR(rng) < 0.6) {
        double u = U_(rng, 1.8, 2.4), v = U_(rng, -1.0, 1.0), c0 = fx(&f, u, v), c1 = fy(&f, u, v), r_ = U_(rng, 0, 1); NsCuts cu = { 0 };
        G *g = ngon(c0, c1, 0.75, 6, r_); cut_add(&cu, disc(c0, c1, 0.32, 10), 0.15); ob_st(ob, ext1(g, 0.5), NULL, -1, &cu, NULL);
    }
    ns_obj_out(o, ob, NULL, foot); return 0;
}
BODY(xf_valve_wheel) {
    double R = U_(rng, 1.2, 1.5), t, c0, c1; int n = (int)CH(rng, 3, 4, 5), k; SoObj *ob = obj_new("iron"); GL *l = gl_new(); NsCuts cu = { 0 }; SoSolids *s = sl_new();
    gl_add(l, ann(cx, cy, R, R - 0.45, 28)); for (k = 0; k < n; k++) gl_add(l, barl(cx, cy, POL(cx, cy, R - 0.2, a + 6.2832 * k / n), 0.42)); gl_add(l, disc(cx, cy, 0.55, 12));
    cut_add(&cu, rect(cx, cy, 0.38, 0.38, a), 0.15); ob_st(ob, ext1(U(l), 0.4), NULL, -1, &cu, disc(cx, cy, R, 20));
    { double tt = a + U_(rng, 0, 6.28); c0 = polx(cx, R + 0.9, tt); c1 = poly_(cy, R + 0.9, tt); }
    t = U_(rng, 0, 6.28); S_lie(s, c0, c1, t, 1.4, 0.26, 0.0); ob_st(ob, s, NULL, -1, NULL, rect(c0, c1, 1.4, 0.6, t));
    ns_obj_out(o, ob, NULL, foot); return 0;
}
BODY(xf_rivets_and_nuts) {
    SoObj *ob = obj_new("iron"); GL *taken = gl_new(); int want = (int)RI(rng, 5, 8), i, j;
    for (i = 0; i < 60; i++) {
        double d, t, c0, c1, k; G *g; int hit = 0;
        if (taken->n >= want) break;
        d = U_(rng, 0, 2.4); t = U_(rng, 0, 6.28); c0 = polx(cx, d, t); c1 = poly_(cy, d, t); g = disc(c0, c1, 0.62, 8);
        for (j = 0; j < taken->n && !hit; j++) hit = intersects(g, taken->v[j]);
        if (hit) continue;
        gl_add(taken, g); k = RR(rng);
        if (k < 0.45) { double r_ = U_(rng, 0, 1); NsCuts cu = { 0 }; G *q = ngon(c0, c1, 0.52, 6, r_); cut_add(&cu, disc(c0, c1, 0.22, 8), 0.12); ob_st(ob, ext1(q, 0.38), NULL, -1, &cu, g); }
        else if (k < 0.8) { SoSolids *s = sl_new(); S_cyl(s, c0, c1, 0.28, 0.5, 0.0, 8); S_dome(s, c0, c1, 0.5, 0.3, 0.0, 1.0, 0.0, 10); ob_st(ob, s, NULL, -1, NULL, g); }
        else ob_st(ob, ext1(ann(c0, c1, 0.6, 0.22, 12), 0.22), NULL, -1, NULL, g);
    }
    *foot = taken->n ? hull(taken) : disc(cx, cy, 0.7, 20);
    for (i = 0; i < ob->L.n; i++) lyl_push(o->lay, ob->L.v[i]);
    return 0;
}
BODY(xf_torn_plate) {
    FF f = ff_(cx, cy, a); double L = U_(rng, 3.6, 4.4), W = U_(rng, 2.4, 2.9), xs[64], t; SoObj *ob = obj_new("rust"); G *g, *r0; GL *ps, *l = gl_new(); int b, i, n, sv; PTS *w;
    r0 = rect(cx, cy, L, W, a);
    { double u1 = L / 2 - U_(rng, 0.6, 1.1); double ax = fx(&f, u1, -W), ay = fy(&f, u1, -W), u2 = L / 2 - U_(rng, 0.3, 0.9); w = wiggle(ax, ay, FX(f, u2, W), rng, 0.5, 6); }
    g = dif(r0, inter(buf(line(w), 1.2, 16), rect(FX(f, L / 2, 0), 2.4, W + 1, a)));
    ps = polys(g); if (!ps->n) ns_raise();
    { int bi = 0; double ba = area(ps->v[0]); for (i = 1; i < ps->n; i++) if (area(ps->v[i]) > ba) { ba = area(ps->v[i]); bi = i; } g = ps->v[bi]; }
    b = ob_up(ob, g, 0.32, NULL);
    n = ns_arange(-L / 2 + 0.55, L / 2 - 0.9, 0.6, xs, 64);
    for (i = 0; i < n; i++) for (sv = -1; sv <= 1; sv += 2) gl_add(l, disc(FX(f, xs[i], sv * (W / 2 - 0.55)), 0.22, 8));
    n = ns_arange(-W / 2 + 1.15, W / 2 - 1.0, 0.6, xs, 64);
    for (i = 0; i < n; i++) gl_add(l, disc(FX(f, -L / 2 + 0.55, xs[i]), 0.22, 8));
    ob_cut(ob, inter(U(l), inset(g, 0.35)), b, 0.2);
    t = a + U_(rng, 0.5, 1.1); { double u = U_(rng, -0.6, 0.2); ob_on(ob, rect(FX(f, u, 0), 0.95, W + 2, t), b, 0.28, NULL); }
    ns_obj_out(o, ob, NULL, foot); return 0;
}
BODY(xf_ingot_stack) {
    FF f = ff_(cx, cy, a); double L = U_(rng, 2.6, 3.0), bw, t; int nb = 4, b, c2, c3, k, i; SoObj *ob = obj_new("iron"); GL *l;
    bw = L / nb;
    b = ob_up(ob, rect(cx, cy, L, L, a), 0.5, NULL);
    l = gl_new(); for (i = 1; i < nb; i++) gl_add(l, barl(FX(f, -L, -L / 2 + i * bw), FX(f, L, -L / 2 + i * bw), 0.28)); ob_cut(ob, U(l), b, 0.2);
    c2 = ob_on(ob, rect(cx, cy, L, L, a), b, 0.5, NULL);
    l = gl_new(); for (i = 1; i < nb; i++) gl_add(l, barl(FX(f, -L / 2 + i * bw, -L), FX(f, -L / 2 + i * bw, L), 0.28)); ob_cut(ob, U(l), c2, 0.2);
    k = (int)RI(rng, 1, 3); c3 = ob_on(ob, rect(FX(f, 0, -L / 2 + k * bw / 2), L, k * bw, a), c2, 0.5, NULL);
    if (k > 1) { l = gl_new(); for (i = 1; i < k; i++) gl_add(l, barl(FX(f, -L, -L / 2 + i * bw), FX(f, L, -L / 2 + i * bw), 0.28)); ob_cut(ob, U(l), c3, 0.2); }
    t = a + U_(rng, 0.2, 0.6); { double v = U_(rng, -0.6, 0.6); ob_up(ob, rect(FX(f, L / 2 + 0.9, v), bw * 0.95, L, t), 0.45, NULL); }
    ns_obj_out(o, ob, NULL, foot); return 0;
}
typedef struct { double r0, r1, L; } Lad;
static int ladle(void *c, double t, double *out) { const Lad *k = (const Lad *)c; double q; if (k->L == 0.0) return -1; q = (t + k->L / 2) / k->L; *out = k->r0 + (k->r1 - k->r0) * (t + k->L / 2) / k->L + (q > 0.92 ? 0.12 : 0.0); return 0; }
BODY(xf_crucible) {
    FF f = ff_(cx, cy, a); double L = U_(rng, 2.8, 3.4), r0 = U_(rng, 0.95, 1.1), r1; SoObj *ob = obj_new("iron"); SoSolids *sols = sl_new(); int sv, s0, i; NsCuts cu = { 0 }; GL *l = gl_new(), *rl = gl_new(); G *sp; Lad ld;
    r1 = r0 * 1.25; ld.r0 = r0; ld.r1 = r1; ld.L = L;
    S_spin(sols, cx, cy, a, L, ladle, &ld, 1.15, 0.0, "round");
    for (sv = -1; sv <= 1; sv += 2) S_lie(sols, FX(f, 0, sv * (r0 + 0.35)), a + 1.5708, 0.55, 0.33, 0.0);
    cut_add(&cu, ell(FX(f, L / 2 - 0.15, 0), 0.35, r1 * 0.75, a, 14), r1 * 0.6);
    ob_st(ob, sols, NULL, -1, &cu, rect(cx, cy, L, 2 * r1 + 0.8, a));
    gl_add(l, ell(FX(f, L / 2 + 1.5, 0), 1.6, 1.0, a, 16)); { double v = U_(rng, -0.6, 0.6); gl_add(l, blob(FX(f, L / 2 + 2.6, v), 0.9, rng, 4)); }
    sp = U(l); s0 = ob_up(ob, sp, 0.35, "slag");
    { double rs[3] = { 1.4, 2.0, 2.6 }; for (i = 0; i < 3; i++) gl_add(rl, ann(FX(f, L / 2 + 0.3, 0), rs[i], rs[i] - 0.18, 20)); }
    ob_cut(ob, inter(buf(U(rl), 0.06, 16), inset(sp, 0.2)), s0, 0.14);
    ns_obj_out(o, ob, NULL, foot); return 0;
}
BODY(xf_chain_heap) {
    SoObj *ob = obj_new("iron"); int n = (int)RI(rng, 2, 3), np_, i; double *sxy, ex, ey; PTS *pts = pl_new(), *tail, *all = pl_new(); GL *l = gl_new();
    np_ = 14 * n; sxy = (double *)malloc(sizeof(double) * 2 * (size_t)(np_ + 1)); ns_keep(sxy, free);
    if (sd_spiral(cx, cy, 0.6, 0.6 + 0.65 * n, n, a, np_, sxy) != SO_OK) ns_raise();
    for (i = 0; i <= np_; i++) pl_add(pts, sxy[2 * i], sxy[2 * i + 1]);
    { double d = U_(rng, 3.8, 4.8), t = a + U_(rng, 0, 6.28); ex = polx(cx, d, t); ey = poly_(cy, d, t); }
    tail = wiggle(pts->v[2 * (pts->n - 1)], pts->v[2 * (pts->n - 1) + 1], ex, ey, rng, 0.5, 3);
    for (i = 0; i < pts->n; i++) pl_add(all, pts->v[2 * i], pts->v[2 * i + 1]);
    for (i = 1; i < tail->n; i++) pl_add(all, tail->v[2 * i], tail->v[2 * i + 1]);
    gl_add(l, disc(cx, cy, 0.9 + 0.65 * n, 20)); gl_add(l, lnbuf(tail, 0.5));
    ob_st(ob, xf_chain(all, 0.38, 0.62), NULL, -1, NULL, U(l));
    ns_obj_out(o, ob, NULL, foot); return 0;
}
BODY(xf_slag_lumps) {
    SoObj *ob = obj_new("slag"); GL *taken = gl_new(); int i, n = (int)RI(rng, 3, 6), j;
    for (i = 0; i < n; i++) {
        double d = i == 0 ? 0.0 : U_(rng, 1.2, 2.4), t = U_(rng, 0, 6.28), c0 = polx(cx, d, t), c1 = poly_(cy, d, t), r = U_(rng, 0.6, 1.1) * (i == 0 ? 1.3 : 1.0), h; G *g = disc(c0, c1, r + 0.1, 10); int hit = 0, nk, k; NsCuts cu = { 0 }; SoSolids *s = sl_new();
        for (j = 0; j < taken->n && !hit; j++) hit = intersects(g, taken->v[j]);
        if (hit) continue;
        gl_add(taken, g); h = r * U_(rng, 0.8, 1.2);
        nk = (int)RI(rng, 1, 3);
        for (k = 0; k < nk; k++) { double dd = r * U_(rng, 0, 0.45), tt = U_(rng, 0, 6.28), rr = U_(rng, 0.12, 0.2); cut_add(&cu, disc(POL(c0, c1, dd, tt), rr, 6), h * 0.55); }
        S_rock(s, c0, c1, r, h, rng, 0.0, 0.0, 9); ob_st(ob, s, NULL, -1, &cu, g);
    }
    *foot = hull(taken);
    for (i = 0; i < ob->L.n; i++) lyl_push(o->lay, ob->L.v[i]);
    return 0;
}
BODY(xf_slag_tongue) {
    FF f = ff_(cx, cy, a); SoObj *ob = obj_new("slag"); GL *l = gl_new(), *rl = gl_new(); G *g; int b, i, n; double h, rs[4] = { 1.5, 2.2, 2.9, 3.6 };
    gl_add(l, ell(FX(f, -1.0, 0), 1.6, 1.1, a, 16)); { double v = U_(rng, -0.3, 0.3); gl_add(l, ell(FX(f, 1.0, v), 1.4, 0.9, a, 14)); }
    { double v = U_(rng, -0.5, 0.5); gl_add(l, blob(FX(f, 2.2, v), 0.8, rng, 4)); }
    g = buf(buf(U(l), 0.2, 16), -0.2, 16);
    h = U_(rng, 0.35, 0.45); b = ob_up(ob, g, h, NULL);
    for (i = 0; i < 4; i++) gl_add(rl, ann(FX(f, -2.6, 0), rs[i], rs[i] - 0.28, 24));
    ob_cut(ob, inter(U(rl), inset(g, 0.25)), b, 0.14);
    n = (int)RI(rng, 1, 2);
    for (i = 0; i < n; i++) { double u = U_(rng, -1.4, 1.2), v = U_(rng, -0.4, 0.4), r; PTS *p = pl_new(); pl_add(p, FX(f, u, v)); r = U_(rng, 0.35, 0.45); ob_studs(ob, p, b, r, 0.35, NULL, 8); }
    ns_obj_out(o, ob, NULL, foot); return 0;
}
BODY(xf_oil_slick) {
    FF f = ff_(cx, cy, a); GL *l = gl_new(), *dl = gl_new(), *hl = gl_new(); G *g, *dr; int i, n; double d;
    { double r = U_(rng, 1.3, 1.8); gl_add(l, blob(cx, cy, r, rng, 4)); }
    { double u = U_(rng, 1.2, 1.8), v = U_(rng, -0.5, 0.5), r = U_(rng, 0.7, 1.0); gl_add(l, blob(FX(f, u, v), r, rng, 3)); }
    g = U(l);
    n = (int)RI(rng, 2, 4);
    for (i = 0; i < n; i++) { double u = U_(rng, -2.8, 3.2), v = U_(rng, -2.0, 2.0), r = U_(rng, 0.3, 0.45); gl_add(dl, disc(FX(f, u, v), r, 8)); }
    dr = dif(U(dl), buf(g, 0.3, 16));
    if (!empty(dr)) { gl_add(hl, g); gl_add(hl, dr); *foot = hull(hl); } else *foot = buf(g, 0.2, 16);
    d = U_(rng, 0.3, 0.4); ns_gcut(o, g, d, "tar");
    if (!empty(dr)) ns_gcut(o, dr, 0.25, "tar");
    return 0;
}
BODY(xf_floor_vent) {
    FF f = ff_(cx, cy, a); double L = U_(rng, 3.2, 3.8), W = U_(rng, 2.4, 2.8), xs[64]; SoObj *ob = obj_new("iron"); G *g = rect(cx, cy, L, W, a), *hole; int fr, i, n; GL *l = gl_new();
    fr = ob_up(ob, g, 0.35, NULL); hole = rect(cx, cy, L - 1.2, W - 1.2, a);
    n = ns_arange(-(L - 1.2) / 2 + 0.3, (L - 1.2) / 2 - 0.15, 0.78, xs, 64);
    for (i = 0; i < n; i++) gl_add(l, rect(FX(f, xs[i], 0), 0.42, W - 1.2, a));
    ob_cut(ob, inter(U(l), hole), fr, 0.3);
    ob_studs(ob, corners4(&f, L / 2 - 0.35, W / 2 - 0.35), fr, 0.2, 0.12, NULL, 8);
    ns_obj_out(o, ob, NULL, foot); return 0;
}
BODY(xf_dust_drift) {
    FF f = ff_(cx, cy, a); SoObj *ob = obj_new("dust"); GL *l = gl_new(); G *g; double h;
    { double rx = U_(rng, 2.2, 2.8), ry = U_(rng, 1.0, 1.3); gl_add(l, ell(cx, cy, rx, ry, a, 18)); }
    { double u = U_(rng, 1.0, 1.6), v = U_(rng, -0.5, 0.5), r = U_(rng, 0.8, 1.1); gl_add(l, blob(FX(f, u, v), r, rng, 4)); }
    g = U(l); h = U_(rng, 0.6, 0.85); ob_raw_mound(ob, g, h, "dust", &XF_SOFT);
    { double u = -U_(rng, 2.5, 3.0), v = U_(rng, -0.6, 0.6), c0 = fx(&f, u, v), c1 = fy(&f, u, v), r = U_(rng, 0.55, 0.75), hh = U_(rng, 0.5, 0.7); SoSolids *s = sl_new();
      S_rock(s, c0, c1, r, hh, rng, 0.0, 0.0, 9); ob_st(ob, s, "slag", -1, NULL, disc(c0, c1, 0.8, 20)); }
    ns_obj_out(o, ob, NULL, foot); return 0;
}

#define SD(fn, cls, kind, name) static const NsSd D_##fn = { fn, "ferrum", cls, kind, name, 0 };
SD(xf_half_gear, "made", "iron", "half_gear") SD(xf_gear_on_edge, "made", "iron", "gear_on_edge") SD(xf_gear_pair, "made", "iron", "gear_pair")
SD(xf_cable_bundle, "made", "cable", "cable_bundle") SD(xf_cable_drum, "made", "cable", "cable_drum") SD(xf_pipe_run, "made", "pipe", "pipe_run")
SD(xf_pipe_elbow, "made", "pipe", "pipe_elbow") SD(xf_piston_ram, "made", "iron", "piston_ram") SD(xf_vent_stack, "made", "iron", "vent_stack")
SD(xf_blower_hood, "made", "iron", "blower_hood") SD(xf_gauge_panel, "made", "iron", "gauge_panel") SD(xf_walker_foot, "made", "iron", "walker_foot")
SD(xf_walker_leg, "made", "iron", "walker_leg") SD(xf_crane_hook, "made", "iron", "crane_hook") SD(xf_crane_jib, "made", "girder", "crane_jib")
SD(xf_girder_stub, "made", "girder", "girder_stub") SD(xf_bolt_head, "made", "iron", "bolt_head") SD(xf_valve_wheel, "made", "iron", "valve_wheel")
SD(xf_rivets_and_nuts, "debris", "iron", "rivets_and_nuts") SD(xf_torn_plate, "made", "rust", "torn_plate") SD(xf_ingot_stack, "made", "iron", "ingot_stack")
SD(xf_crucible, "made", "iron", "crucible") SD(xf_chain_heap, "made", "iron", "chain_heap") SD(xf_slag_lumps, "debris", "slag", "slag_lumps")
SD(xf_slag_tongue, "debris", "slag", "slag_tongue") SD(xf_oil_slick, "marks", "tar", "oil_slick") SD(xf_floor_vent, "made", "iron", "floor_vent")
SD(xf_dust_drift, "debris", "dust", "dust_drift")
void ns_reg_ferrum(void) {
    const NsSd *all[] = { &D_xf_half_gear, &D_xf_gear_on_edge, &D_xf_gear_pair, &D_xf_cable_bundle, &D_xf_cable_drum, &D_xf_pipe_run, &D_xf_pipe_elbow, &D_xf_piston_ram,
        &D_xf_vent_stack, &D_xf_blower_hood, &D_xf_gauge_panel, &D_xf_walker_foot, &D_xf_walker_leg, &D_xf_crane_hook, &D_xf_crane_jib, &D_xf_girder_stub, &D_xf_bolt_head,
        &D_xf_valve_wheel, &D_xf_rivets_and_nuts, &D_xf_torn_plate, &D_xf_ingot_stack, &D_xf_crucible, &D_xf_chain_heap, &D_xf_slag_lumps, &D_xf_slag_tongue,
        &D_xf_oil_slick, &D_xf_floor_vent, &D_xf_dust_drift };
    int i; for (i = 0; i < (int)(sizeof all / sizeof *all); i++) ns_sd_reg(all[i], 10.5, all[i]->body == xf_oil_slick);
}
