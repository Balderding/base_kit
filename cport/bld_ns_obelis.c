/* bld_ns_obelis.c: style_obelis.py's glyphs (ob_glyph, ob_band, _stroke, _chamfer, _oct), its own builders (b_ob_slab,
   b_ob_casket, b_ob_pylon, b_ob_panel, L_obelisk, L_pylons) and the 25 style-only details of sd_r_x_obelis.py, copied
   routine by routine (each names the Python it copies). See bld_ns.h. */
#include "bld_ns.h"

/* _stroke(pts, w): LineString(pts).buffer(w / 2, cap_style=2, join_style=2, mitre_limit=4.0) */
G *ob_stroke(PTS *p, double w) { return bufx(line(p), w / 2, 16, SHP_CAP_FLAT, SHP_JOIN_MITRE, 4.0); }
/* _chamfer(g, c): g.buffer(-c, join_style=2).buffer(c, join_style=3) */
G *ob_chamfer(const G *g, double c) { return bufx(bufx(g, -c, 16, SHP_CAP_ROUND, SHP_JOIN_MITRE, 5.0), c, 16, SHP_CAP_ROUND, SHP_JOIN_BEVEL, 5.0); }
/* _oct(x, y, r, a=0.0) */
G *ob_oct(double x, double y, double r, double a) {
    double xy[16]; int k;
    for (k = 0; k < 8; k++) { double t = a + PI / 8 + k * PI / 4; xy[2 * k] = x + r * ns_cos(t); xy[2 * k + 1] = y + r * ns_sin(t); }
    return polyv(xy, 8);
}

/* ob_glyph(cx, cy, s, a, rng, w=0.25) */
typedef struct { double cx, cy, c, sn; } GP;
static void gp(PTS *l, const GP *p, double u, double v) { pl_add(l, p->cx + u * p->c - v * p->sn, p->cy + u * p->sn + v * p->c); }
static G *gL(const GP *p, const double *uv, int n, double w) { PTS *l = pl_new(); int i; for (i = 0; i < n; i++) gp(l, p, uv[2 * i], uv[2 * i + 1]); return ob_stroke(l, w); }
G *ob_glyph(double cx, double cy, double s, double a, PyRandom *rng, double w) {
    static const double Q[4] = { -0.4, -0.133, 0.133, 0.4 };
    double q[4], lo, hi; GL *g = gl_new(); GP p; int i, nm; int64_t marks[2];
    w = w < 0.14 * s ? w : 0.14 * s;          /* min(w, 0.14 * s) */
    for (i = 0; i < 4; i++) q[i] = Q[i] * s;
    p.cx = cx; p.cy = cy; p.c = ns_cos(a); p.sn = ns_sin(a);
    lo = q[0]; hi = q[3];
    if (RR(rng) < 0.6) {          /* the bracket: three sides of the square */
        double sides[4][4] = { { lo, hi, lo, lo }, { lo, lo, hi, lo }, { hi, lo, hi, hi }, { hi, hi, lo, hi } }, uv[32]; int k = (int)RI(rng, 0, 3), run[8], nr = 0, j;
        for (j = (k + 1) % 4; j < 4; j++) run[nr++] = j;
        for (j = 0; j < k; j++) run[nr++] = j;
        uv[0] = sides[run[0]][0]; uv[1] = sides[run[0]][1];
        for (j = 0; j < nr; j++) { uv[2 + 2 * j] = sides[run[j]][2]; uv[3 + 2 * j] = sides[run[j]][3]; }
        gl_add(g, gL(&p, uv, nr + 1, w));
    } else {
        double uv[10] = { lo, lo, hi, lo, hi, hi, lo, hi, lo, lo };
        gl_add(g, gL(&p, uv, 5, w));
        if (RR(rng) < 0.5) { double m = s * 0.18, u2[10] = { -m, -m, m, -m, m, m, -m, m, -m, -m }; gl_add(g, gL(&p, u2, 5, w)); }
    }
    nm = (int)RI(rng, 1, 2);
    if (py_sample_e(rng, 6, nm, marks) != 0) ns_raise();
    for (i = 0; i < nm; i++) {
        int k = (int)marks[i];
        if (k == 0) { double v = CH(rng, q[1], q[2]), e = CH(rng, q[2], hi), uv[4] = { lo, v, e, v }; gl_add(g, gL(&p, uv, 2, w)); }
        else if (k == 1) { double u = CH(rng, q[1], q[2]), e = CH(rng, q[1], q[2]), uv[4] = { u, lo, u, e }; gl_add(g, gL(&p, uv, 2, w)); }
        else if (k == 2) { double uv[12] = { lo, lo, q[1], lo, q[1], q[1], q[2], q[1], q[2], q[2], hi, q[2] }; gl_add(g, gL(&p, uv, 6, w)); }
        else if (k == 3) {
            double sv = CH(rng, -1, 1);
            if (RR(rng) < 0.5) { double uv[6] = { q[1], sv * q[2], 0, -sv * q[1] * 0.5, q[2], sv * q[2] }; gl_add(g, gL(&p, uv, 3, w)); }
            else { double uv[6] = { lo * 0.6, sv * q[2], 0, -sv * q[1], hi * 0.6, sv * q[2] }; gl_add(g, gL(&p, uv, 3, w)); }
        } else if (k == 4) {
            double u = CH(rng, q[1], q[2]), v = CH(rng, q[1], q[2]), d = s * 0.09; PTS *l = pl_new();
            gp(l, &p, u - d, v - d); gp(l, &p, u + d, v - d); gp(l, &p, u + d, v + d); gp(l, &p, u - d, v + d);
            gl_add(g, poly(l));
        } else { double uv[6] = { q[1], q[1], q[2], q[1], q[2], q[2] }; gl_add(g, gL(&p, uv, 3, w)); }
    }
    return uu(g);
}
/* ob_band(cx, cy, a, L, cell, rng, w=0.25, border=True) */
G *ob_band(double cx, double cy, double a, double L, double cell, PyRandom *rng, double w, int border) {
    double c = ns_cos(a), sn = ns_sin(a); int n = (int)trunc(L / (cell * 1.12)), i, sv; GL *g = gl_new();
    if (n < 1) n = 1;
    for (i = 0; i < n; i++) { double u = (i - (n - 1) / 2.0) * cell * 1.12; gl_add(g, ob_glyph(cx + u * c, cy + u * sn, cell, a, rng, w)); }
    if (border)
        for (sv = -1; sv <= 1; sv += 2) {
            double v = sv * cell * 0.72, h = n * cell * 1.12 / 2; PTS *l = pl_new();
            pl_add(l, cx - h * c - v * sn, cy - h * sn + v * c); pl_add(l, cx + h * c - v * sn, cy + h * sn + v * c);
            gl_add(g, ob_stroke(l, w));
        }
    return uu(g);
}

/* ================================================================== style_obelis.py builders */
typedef struct { const char *kind, *floor; int which; } ObCtx;
/* b_ob_slab(kind="monolith").f(cx, cy, a, rng) */
static int ob_slab(const ObCtx *c, double cx, double cy, double a, PyRandom *rng, double k, NsOut *o, G **foot) {
    double P_ = U_(rng, 2.6, 3.2), h; G *g;
    { double v = P_ * U_(rng, 0.8, 1.0); g = ob_chamfer(rect(cx, cy, P_, v, a), 0.2); }
    *foot = g;
    h = U_(rng, 0.45, 0.6); ns_up(o, g, h, c->kind);
    ns_tcut(o, ob_glyph(cx, cy, P_ * 0.75, a, rng, 0.3), 0, 0.2);
    return 0;
}
/* b_ob_casket(kind="casket").f */
static int ob_casket(const ObCtx *c, double cx, double cy, double a, PyRandom *rng, double k, NsOut *o, G **foot) {
    double L = U_(rng, 4.2, 5.0), W = U_(rng, 1.8, 2.2), h; G *g = ob_chamfer(rect(cx, cy, L, W, a), 0.45), *lid = inset(g, 0.25), *crest;
    crest = ob_chamfer(rect(cx, cy, L * 0.35, W * 0.35, a), 0.2);
    *foot = g;
    h = U_(rng, 0.8, 1.0); ns_up(o, g, h, c->kind); ns_on(o, lid, 0, 0.25, c->kind); ns_on(o, crest, 1, 0.2, c->kind);
    ns_tcut(o, dif(inset(lid, 0.25), inset(lid, 0.47)), 1, 0.15);
    return 0;
}
/* b_ob_pylon(kind="monolith").f */
static int ob_pylon(const ObCtx *c, double cx, double cy, double a, PyRandom *rng, double k, NsOut *o, G **foot) {
    double r = U_(rng, 1.2, 1.5), h; G *g = ob_oct(cx, cy, r, a), *sh = ob_oct(cx, cy, r * 0.66, a);
    *foot = g;
    ns_up(o, g, 0.5, c->kind); h = U_(rng, 1.4, 2.2); ns_on(o, sh, 0, h, c->kind); ns_tcut(o, rect(cx, cy, 2 * r, 0.32, a), 1, 0.25);
    return 0;
}
/* b_ob_panel(kind="obmetal").f */
static int ob_panel(const ObCtx *c, double cx, double cy, double a, PyRandom *rng, double k, NsOut *o, G **foot) {
    FF f = ff_(cx, cy, a); double L = U_(rng, 3.0, 3.8), W = U_(rng, 1.8, 2.3), xs[64], u; PTS *pl = pl_new(); G *g; GL *l = gl_new(); int i, n;
    pl_add(pl, FX(f, -L / 2, -W / 2)); pl_add(pl, FX(f, L / 2, -W / 2)); u = L / 2 - U_(rng, 0.4, 1.2); pl_add(pl, FX(f, u, W / 2)); pl_add(pl, FX(f, -L / 2, W / 2));
    g = poly(pl);
    n = ns_arange(-L / 2 + 0.7, L / 2 - 1.0, 0.5, xs, 64);
    for (i = 0; i < n; i++) gl_add(l, rect(FX(f, xs[i], 0), 0.25, W * 0.5, a));
    *foot = g;
    ns_up(o, g, 0.35, c->kind); ns_tcut(o, uu(l), 0, 0.15);
    return 0;
}
/* L_obelisk(kind="monolith", floor="obfloor").f(cx, cy, a, rng, k) */
static int L_obelisk(const ObCtx *c, double cx, double cy, double a, PyRandom *rng, double k, NsOut *o, G **foot) {
    double P_ = U_(rng, 5.0, 5.8) * fmax(1.0, ns_pow(k, 0.6)); G *g1 = ob_chamfer(rect(cx, cy, P_, P_, a), 0.3), *g2 = inset(g1, P_ * 0.13), *g3 = inset(g2, P_ * 0.13), *sl;
    sl = ob_chamfer(rect(cx, cy, P_ * 0.22, P_ * 0.42, a), 0.15);
    ns_up(o, g1, 0.45, c->floor); ns_on(o, g2, 0, 0.45, c->kind); ns_on(o, g3, 1, 0.45, c->kind); ns_on(o, sl, 2, 2.0, c->kind);
    ns_tcut(o, dif(inset(g1, 0.12), inset(g1, 0.32)), 0, 0.15); ns_tcut(o, dif(inset(g2, 0.12), inset(g2, 0.32)), 1, 0.15);
    *foot = buf(g1, 0.3, 16);
    return 0;
}
/* L_pylons(kind="monolith", floor="obmetal").f */
static int L_pylons(const ObCtx *c, double cx, double cy, double a, PyRandom *rng, double k, NsOut *o, G **foot) {
    double R = U_(rng, 3.6, 4.2) * fmax(1.0, ns_pow(k, 0.6)); int n = (int)CH(rng, 4, 6), i; G *dais = PB(cx, cy, R * 0.55, 32); GL *ft = gl_new();
    ns_up(o, dais, 0.4, c->floor); ns_tcut(o, PB(cx, cy, R * 0.25, 24), 0, 0.25); gl_add(ft, dais);
    for (i = 0; i < n; i++) {
        double t = a + 6.2832 * i / n, h; G *p = ob_chamfer(rect(cx + R * ns_cos(t), cy + R * ns_sin(t), 1.3, 1.3, t), 0.2); int j;
        gl_add(ft, p);
        j = ns_count(o); ns_up(o, bufx(p, 0.3, 16, SHP_CAP_ROUND, SHP_JOIN_MITRE, 5.0), 0.4, c->kind);
        h = U_(rng, 1.4, 2.4); ns_on(o, p, j, h, c->kind);
    }
    *foot = chull(uu(ft));
    return 0;
}
typedef int (*ObBody)(const ObCtx *c, double cx, double cy, double a, PyRandom *rng, double k, NsOut *o, G **foot);
static const ObBody OB[6] = { ob_slab, ob_casket, ob_pylon, ob_panel, L_obelisk, L_pylons };
static int ob_call(void *ctx, double cx, double cy, double a, PyRandom *rng, int has_scale, double scale, Geo **foot, SoLayers *layers) {
    const ObCtx *c = (const ObCtx *)ctx; SoLayers lay = { 0 }; NsOut o = { &lay }; G *f = NULL; int r, i, lm = c->which >= 4;
    *foot = NULL;
    if (lm != (has_scale != 0)) return SC_ERR;          /* a b_ takes four arguments, an L_ five: TypeError */
    {
        NsFrame fr_; ns_enter(&fr_);
        if (setjmp(fr_.jb)) { ns_leave(&fr_); lyl_free(&lay); return SC_ERR; }
        r = OB[c->which](c, cx, cy, a, rng, scale, &o, &f);
        *foot = f ? geo_new(shp_clone(f)) : NULL;
        ns_leave(&fr_);
    }
    for (i = 0; i < lay.n; i++) lyl_push(layers, lay.v[i]);
    lyl_free(&lay);
    return r;
}
static int ob_make(const FtCall *call, FtBuildFn *fn, void **ctx, int which, const char *k0, const char *f0) {
    ObCtx *c = (ObCtx *)calloc(1, sizeof *c);
    c->which = which; c->kind = so_intern(ns_arg_s(call, 0, "kind", k0)); c->floor = f0 ? so_intern(ns_arg_s(call, 1, "floor", f0)) : NULL;
    bld_keep(c, free); *fn = ob_call; *ctx = c;
    return 0;
}
static int f_ob_slab(const FtCall *c, FtBuildFn *fn, void **x) { return ob_make(c, fn, x, 0, "monolith", NULL); }
static int f_ob_casket(const FtCall *c, FtBuildFn *fn, void **x) { return ob_make(c, fn, x, 1, "casket", NULL); }
static int f_ob_pylon(const FtCall *c, FtBuildFn *fn, void **x) { return ob_make(c, fn, x, 2, "monolith", NULL); }
static int f_ob_panel(const FtCall *c, FtBuildFn *fn, void **x) { return ob_make(c, fn, x, 3, "obmetal", NULL); }
static int f_L_obelisk(const FtCall *c, FtBuildFn *fn, void **x) { return ob_make(c, fn, x, 4, "monolith", "obfloor"); }
static int f_L_pylons(const FtCall *c, FtBuildFn *fn, void **x) { return ob_make(c, fn, x, 5, "monolith", "obmetal"); }
void ns_bld_obelis(void) {
    bld_add("style_obelis", "b_ob_slab", f_ob_slab); bld_add("style_obelis", "b_ob_casket", f_ob_casket); bld_add("style_obelis", "b_ob_pylon", f_ob_pylon);
    bld_add("style_obelis", "b_ob_panel", f_ob_panel); bld_add("style_obelis", "L_obelisk", f_L_obelisk); bld_add("style_obelis", "L_pylons", f_L_pylons);
    bld_add("style_obelis", "ob_slab", f_ob_slab); bld_add("style_obelis", "ob_casket", f_ob_casket); bld_add("style_obelis", "ob_pylon", f_ob_pylon);          /* FT_NEW / FT_LMS names */
    bld_add("style_obelis", "ob_panel", f_ob_panel); bld_add("style_obelis", "obelisk", f_L_obelisk); bld_add("style_obelis", "pylons", f_L_pylons);
}

/* ================================================================== sd_r_x_obelis.py */
#define BODY(nm) static int nm(double cx, double cy, double a, PyRandom *rng, NsOut *o, G **foot)
#define OUT_ ns_obj_out(o, ob, NULL, foot); return 0

/* xo_drift(o, x, y, r, h, rng) */
static void xo_drift(SoObj *ob, double x, double y, double r, double h, PyRandom *rng) {
    SoSolids *s = sl_new(); double sq = U_(rng, 0.5, 0.75), aa = U_(rng, 0, 3.14);
    S_dome(s, x, y, r, h, 0.0, sq, aa, 20); ob_st(ob, s, "sand", -1, NULL, disc(x, y, r * 0.8, 20));
}
/* xo_chips(o, ff, xs, vs, rng, kind="obsidian", n=(2, 3)) */
static void xo_chips(SoObj *ob, const FF *f, double x0, double x1, double v0, double v1, PyRandom *rng, const char *kind, int n0, int n1) {
    int n = (int)RI(rng, n0, n1), i, k;
    for (i = 0; i < n; i++) {
        double u = U_(rng, x0, x1), v = U_(rng, v0, v1), c0 = fx(f, u, v), c1 = fy(f, u, v), r = U_(rng, 0.6, 0.85), h; PTS *p = pl_new(); G *g;
        for (k = 0; k < 4; k++) { double rr = r * U_(rng, 0.8, 1.15), t = k * 1.57 + U_(rng, -0.35, 0.35); pl_add(p, POL(c0, c1, rr, t)); }
        g = poly(p); h = U_(rng, 0.35, 0.5); ob_up(ob, g, h, kind);
    }
}
static PTS *corners4o(const FF *f, double hx, double hy) { PTS *p = pl_new(); int sx, sy; for (sx = -1; sx <= 1; sx += 2) for (sy = -1; sy <= 1; sy += 2) pl_add(p, fx(f, sx * hx, sy * hy), fy(f, sx * hx, sy * hy)); return p; }
static G *tri3(double ax, double ay, double bx, double by, double cx_, double cy_) { double xy[6] = { ax, ay, bx, by, cx_, cy_ }; return polyv(xy, 3); }
static G *quad4(const double *xy) { return polyv(xy, 4); }

BODY(xo_fallen_monolith) {
    FF f = ff_(cx, cy, a); double L = U_(rng, 8.4, 9.2), W = U_(rng, 2.0, 2.4), h = U_(rng, 1.3, 1.6), xb, gap, tb, db2; SoObj *ob = obj_new("monolith"); G *cutl, *whole, *gs[2]; int j;
    xb = U_(rng, -0.2, 0.2) * L; gap = U_(rng, 0.35, 0.6); tb = U_(rng, -0.25, 0.25);
    { double xy[8] = { FX(f, xb - W * py_tan(tb) / 2, -W), FX(f, xb + W * py_tan(tb) / 2, W), FX(f, L, W), FX(f, L, -W) }; cutl = quad4(xy); }
    whole = ob_chamfer(rect(cx, cy, L, W, a), 0.3);
    gs[0] = dif(whole, cutl); gs[1] = inter(whole, cutl); db2 = U_(rng, -0.1, 0.1);
    for (j = 0; j < 2; j++) {
        double dx = j ? gap : 0.0, db = j ? db2 : 0.0, t, hh, Lp; G *g = gs[j], *c0, *c; int b, tp;
        if (empty(g)) continue;
        c0 = cen(g); g = rot(T(shp_translate(g, dx * ns_cos(a), dx * ns_sin(a))), db, SHP_ORIGIN_POINT, gx(c0), gy(c0), 1);
        t = a + db; c = cen(g); hh = h * (dx == 0 ? 1.0 : U_(rng, 0.9, 1.0)); b = ob_up(ob, g, hh, NULL); tp = ob_on(ob, inset(g, 0.28), b, 0.2, NULL);
        Lp = sqrt(area(g) / W) * 1.6;
        ob_cut(ob, ob_band(gx(c), gy(c), t, Lp, 1.2, rng, 0.24, 1), tp, 0.14);
    }
    xo_chips(ob, &f, xb - 0.3, xb + 0.6, -W / 2 - 0.7, W / 2 + 0.7, rng, "obsidian", 2, 3);
    if (RR(rng) < 0.7) { double ch = CH(rng, -1, 1), u = ch * (L / 2 + U_(rng, -0.2, 0.3)), v = U_(rng, -0.4, 0.4), e0 = fx(&f, u, v), e1 = fy(&f, u, v), r = U_(rng, 1.5, 2.0), hh = U_(rng, 0.7, 0.95); xo_drift(ob, e0, e1, r, hh, rng); }
    OUT_;
}
BODY(xo_standing_stub) {
    FF f = ff_(cx, cy, a); double P_ = U_(rng, 3.4, 3.9), sw, sl, h; SoObj *ob = obj_new("monolith"); G *g = ob_chamfer(rect(cx, cy, P_, P_, a), 0.25), *slab; int b, s2, lo, n, i, k;
    b = ob_up(ob, g, 0.4, NULL); ob_groove(ob, inset(g, 0.2), b, 0.15, 0.12); s2 = ob_on(ob, inset(g, 0.5), b, 0.35, NULL); ob_groove(ob, inset(g, 0.7), s2, 0.15, 0.12);
    sw = U_(rng, 1.1, 1.3); sl = U_(rng, 2.0, 2.4); slab = ob_chamfer(rect(cx, cy, sw, sl, a), 0.18); h = U_(rng, 1.1, 1.4); lo = ob_on(ob, slab, s2, h, NULL);
    { G *q = inter(slab, rect(FX(f, 0, sl / 4), sw * 2, sl / 2, a)); h = U_(rng, 0.35, 0.6); ob_on(ob, q, lo, h, NULL); }
    ob_cut(ob, rect(FX(f, 0, -sl / 4), sw * 0.5, 0.22, a), lo, 0.12);
    n = (int)RI(rng, 1, 2);
    for (i = 0; i < n; i++) {
        double ch = CH(rng, -1, 1), u = ch * U_(rng, 1.0, 1.3), v = U_(rng, -1.2, 1.2), c0 = fx(&f, u, v), c1 = fy(&f, u, v); PTS *p = pl_new();
        for (k = 0; k < 3; k++) { double t = k * 2.1 + U_(rng, -0.3, 0.3); pl_add(p, POL(c0, c1, 0.35, t)); }
        ob_on(ob, poly(p), s2, 0.25, "obsidian");
    }
    OUT_;
}
BODY(xo_glyph_stele) {
    FF f = ff_(cx, cy, a); double P_ = U_(rng, 4.2, 4.8), cell, h; SoObj *ob = obj_new("monolith"); G *g; int b, i, j; GL *gl = gl_new();
    { double v = P_ * U_(rng, 0.9, 1.0); g = ob_chamfer(rect(cx, cy, P_, v, a), 0.25); }
    if (RR(rng) < 0.35) {
        double sx = CH(rng, -1, 1), sy = CH(rng, -1, 1), u1, v2, xy[6];
        xy[0] = fx(&f, sx * P_ / 2, sy * P_ / 2); xy[1] = fy(&f, sx * P_ / 2, sy * P_ / 2);
        u1 = sx * (P_ / 2 - U_(rng, 0.8, 1.2)); xy[2] = fx(&f, u1, sy * P_ / 2); xy[3] = fy(&f, u1, sy * P_ / 2);
        v2 = sy * (P_ / 2 - U_(rng, 0.8, 1.2)); xy[4] = fx(&f, sx * P_ / 2, v2); xy[5] = fy(&f, sx * P_ / 2, v2);
        g = dif(g, buf(polyv(xy, 3), 0.05, 16));
    }
    h = U_(rng, 0.6, 0.8); b = ob_up(ob, g, h, NULL); ob_groove(ob, inset(g, 0.3), b, 0.2, 0.15);
    cell = P_ * 0.34;
    for (i = -1; i <= 1; i += 2) for (j = -1; j <= 1; j += 2) gl_add(gl, ob_glyph(FX(f, i * cell * 0.6, j * cell * 0.6), cell * 0.95, a, rng, 0.26));
    ob_cut(ob, inter(U(gl), inset(g, 0.55)), b, 0.18);
    OUT_;
}
BODY(xo_capstone) {
    double P_ = U_(rng, 3.4, 3.8), st; SoObj *ob = obj_new("monolith"); G *g; int b, i, k;
    { double aa = a + U_(rng, -0.1, 0.1); g = ob_chamfer(rect(cx, cy, P_, P_, aa), 0.15); }
    st = P_ * 0.13;
    b = ob_up(ob, g, 0.5, NULL); ob_groove(ob, inset(g, 0.22), b, 0.16, 0.12); i = b;
    for (k = 1; k < 4; k++) i = ob_on(ob, inset(g, st * k), i, 0.45, NULL);
    ob_cut(ob, rect(cx, cy, P_ * 0.25, 0.22, a), i, 0.15);
    OUT_;
}
BODY(xo_obelisk_tip) {
    FF f = ff_(cx, cy, a); double L = U_(rng, 6.6, 7.4), W = U_(rng, 1.9, 2.2), h, xs[64]; SoObj *ob = obj_new("monolith"); PTS *p = pl_new(), *q = pl_new(); G *g; int b, i, n; GL *gl = gl_new();
    pl_add(p, FX(f, -L / 2, -W / 2)); pl_add(p, FX(f, L * 0.22, -W * 0.42)); pl_add(p, FX(f, L / 2, 0)); pl_add(p, FX(f, L * 0.22, W * 0.42)); pl_add(p, FX(f, -L / 2, W / 2)); g = poly(p);
    h = U_(rng, 0.9, 1.1); b = ob_up(ob, g, h, NULL);
    pl_add(q, FX(f, -L / 2 + 0.2, -W * 0.2)); pl_add(q, FX(f, L * 0.2, -W * 0.14)); pl_add(q, FX(f, L / 2 - 0.6, 0)); pl_add(q, FX(f, L * 0.2, W * 0.14)); pl_add(q, FX(f, -L / 2 + 0.2, W * 0.2));
    ob_on(ob, poly(q), b, 0.3, NULL);
    n = ns_arange(-L / 2 + 0.8, L * 0.1, 1.05, xs, 64);
    for (i = 0; i < n; i++) gl_add(gl, ob_glyph(FX(f, xs[i], -W * 0.32), 0.95, a, rng, 0.2));
    ob_cut(ob, inter(U(gl), inset(g, 0.2)), b, 0.12);
    xo_chips(ob, &f, -L / 2 - 0.8, -L / 2 - 0.3, -W / 2, W / 2, rng, "obsidian", 2, 3);
    OUT_;
}
BODY(xo_half_pylon) {
    double r = U_(rng, 1.5, 1.8), h; SoObj *ob = obj_new("monolith"); G *g = ob_oct(cx, cy, r, a); int b, sh;
    h = U_(rng, 0.5, 0.6); b = ob_up(ob, g, h, NULL); ob_groove(ob, ob_oct(cx, cy, r - 0.25, a), b, 0.15, 0.12);
    { G *q = ob_oct(cx, cy, r * 0.66, a); h = U_(rng, 1.6, 2.2); sh = ob_on(ob, q, b, h, NULL); }
    ob_cut(ob, rect(cx, cy, r * 1.6, 0.3, a), sh, 0.25);
    ob_groove(ob, ob_oct(cx, cy, r * 0.66 - 0.22, a), sh, 0.14, 0.1);
    { double d = r + U_(rng, 0.6, 0.9), t = a + U_(rng, 0, 6.28), rr = U_(rng, 1.4, 1.9), hh = U_(rng, 0.6, 0.9); xo_drift(ob, POL(cx, cy, d, t), rr, hh, rng); }
    OUT_;
}
BODY(xo_toppled_pylon) {
    FF f = ff_(cx, cy, a); double L = U_(rng, 7.4, 8.4), W = U_(rng, 1.5, 1.8), h, xs[2]; SoObj *ob = obj_new("monolith"); G *g = ob_chamfer(rect(cx, cy, L, W, a), 0.35); int b, tp, i; GL *l = gl_new();
    h = U_(rng, 1.0, 1.2); b = ob_up(ob, g, h, NULL); tp = ob_on(ob, inset(g, 0.3), b, 0.15, NULL);
    xs[0] = -L * 0.2; xs[1] = L * 0.08;
    for (i = 0; i < 2; i++) { G *q = ob_chamfer(rect(FX(f, xs[i], 0), 0.55, W + 0.5, a), 0.12); h = U_(rng, 1.25, 1.4); ob_up(ob, q, h, NULL); }
    gl_add(l, rect(FX(f, L / 2 - 0.6, -W * 0.18), 0.8, 0.22, a)); gl_add(l, rect(FX(f, L / 2 - 0.6, W * 0.18), 0.8, 0.22, a)); ob_cut(ob, U(l), tp, 0.12);
    { double u = -L / 2 - U_(rng, 1.4, 1.8), v = U_(rng, -0.5, 0.5), c0 = fx(&f, u, v), c1 = fy(&f, u, v), r = U_(rng, 1.1, 1.3); G *so = ob_oct(c0, c1, r, a); int sb; h = U_(rng, 0.5, 0.7); sb = ob_up(ob, so, h, NULL); ob_cut(ob, ob_oct(c0, c1, 0.6, a), sb, 0.3); }
    OUT_;
}
BODY(xo_pylon_ring) {
    double R = U_(rng, 2.6, 3.0); int n = (int)CH(rng, 4, 5, 6), i, dc; SoObj *ob = obj_new("monolith");
    dc = ob_up(ob, disc(cx, cy, R * 0.55, 32), 0.3, "obmetal"); ob_groove(ob, disc(cx, cy, R * 0.42, 28), dc, 0.14, 0.1); ob_cut(ob, disc(cx, cy, R * 0.2, 20), dc, 0.15);
    for (i = 0; i < n; i++) {
        double t = a + 6.2832 * i / n, c0 = polx(cx, R, t), c1 = poly_(cy, R, t), h; G *g = ob_chamfer(rect(c0, c1, 1.15, 1.15, t), 0.15); int b, tp;
        h = U_(rng, 0.9, 1.4); b = ob_up(ob, g, h, NULL); tp = ob_on(ob, inset(g, 0.22), b, 0.15, NULL); ob_cut(ob, rect(c0, c1, 0.7, 0.22, t), tp, 0.1);
    }
    OUT_;
}
/* _casket(o, cx, cy, a, L, W, rng, h=0.95) -> (g, b) */
static G *xo_casket(SoObj *ob, double cx, double cy, double a, double L, double W, double h, int *b) {
    G *g = ob_chamfer(rect(cx, cy, L, W, a), 0.45); int pl = ob_up(ob, bufx(g, 0.25, 16, SHP_CAP_ROUND, SHP_JOIN_MITRE, 5.0), 0.25, NULL);
    *b = ob_on(ob, g, pl, h, NULL); return g;
}
BODY(xo_casket_) {
    FF f = ff_(cx, cy, a); double L = U_(rng, 5.2, 5.8), W = U_(rng, 2.0, 2.3); SoObj *ob = obj_new("casket"); int b, lid, c1, i, sv; G *g = xo_casket(ob, cx, cy, a, L, W, 0.95, &b); GL *l = gl_new(); double xs[2];
    lid = ob_on(ob, inset(g, 0.2), b, 0.22, NULL); ob_groove(ob, inset(g, 0.42), lid, 0.14, 0.1);
    { double xy[8] = { FX(f, -L * 0.3, 0), FX(f, 0, -W * 0.22), FX(f, L * 0.3, 0), FX(f, 0, W * 0.22) }; c1 = ob_on(ob, quad4(xy), lid, 0.2, NULL); }
    { double xy[8] = { FX(f, -L * 0.15, 0), FX(f, 0, -W * 0.11), FX(f, L * 0.15, 0), FX(f, 0, W * 0.11) }; ob_on(ob, quad4(xy), c1, 0.18, NULL); }
    xs[0] = -L * 0.32; xs[1] = L * 0.32;
    for (i = 0; i < 2; i++) for (sv = -1; sv <= 1; sv += 2) gl_add(l, rect(FX(f, xs[i], sv * W * 0.32), 0.3, 0.3, a));
    ob_cut(ob, U(l), lid, 0.1);
    OUT_;
}
BODY(xo_open_casket) {
    FF f = ff_(cx, cy, a); double L = U_(rng, 5.0, 5.6), W = U_(rng, 2.0, 2.3), sv, c0, c1, t; SoObj *ob = obj_new("casket"); int b, ld; G *g = xo_casket(ob, cx, cy, a, L, W, 0.95, &b), *lg;
    ob_cut(ob, inset(g, 0.35), b, 0.2); ob_cut(ob, inset(g, 0.6), b, 0.75);
    sv = CH(rng, -1, 1); { double u = U_(rng, -0.5, 0.5), v = sv * (W + U_(rng, 0.4, 0.7)); c0 = fx(&f, u, v); c1 = fy(&f, u, v); }
    t = a + U_(rng, -0.35, 0.35); lg = ob_chamfer(rect(c0, c1, L - 0.3, W - 0.2, t), 0.4);
    ld = ob_up(ob, lg, 0.35, NULL); ob_groove(ob, inset(lg, 0.25), ld, 0.14, 0.1);
    ob_cut(ob, wavy(POL(c0, c1, L * 0.25, t + 1.4), POL(c0, c1, L * 0.25, t - 1.8), rng, 0.25, 3, 0.22), ld, 0.2);
    OUT_;
}
BODY(xo_vessel_row) {
    FF f = ff_(cx, cy, a); int n = (int)RI(rng, 3, 4), fall, i; SoObj *ob = obj_new("casket"); double s;
    s = U_(rng, 1.05, 1.2); fall = (int)RB(rng, n);
    for (i = 0; i < n; i++) {
        double u = (i - (n - 1) / 2.0) * (s + 0.3), c0 = fx(&f, u, 0), c1 = fy(&f, u, 0), h; G *g; int b, l1, l2;
        if (i == fall) {
            double aa = a + 1.5708 + U_(rng, -0.3, 0.3);
            g = ob_chamfer(rect(c0, c1, s * 1.5, s, aa), 0.12); b = ob_up(ob, g, s * 0.8, NULL); ob_cut(ob, ob_chamfer(rect(c0, c1, s * 1.1, s * 0.5, a + 1.5708), 0.1), b, 0.12);
            { double v = -s - U_(rng, 0.4, 0.7), q0 = fx(&f, u, v), q1 = fy(&f, u, v), a2 = a + U_(rng, -0.5, 0.5); ob_up(ob, ob_chamfer(rect(q0, q1, s * 0.9, s * 0.9, a2), 0.12), 0.3, NULL); }
            continue;
        }
        g = ob_chamfer(rect(c0, c1, s, s, a), 0.12); h = U_(rng, 1.3, 1.6); b = ob_up(ob, g, h, NULL); l1 = ob_on(ob, inset(g, 0.12), b, 0.15, NULL); l2 = ob_on(ob, inset(g, 0.32), l1, 0.2, NULL);
        ob_cut(ob, rect(c0, c1, s * 0.4, 0.2, a), l2, 0.1);
    }
    OUT_;
}
BODY(xo_metal_shards) {
    SoObj *ob = obj_new("obmetal"); GL *gs = gl_new(); int n = (int)RI(rng, 2, 3), i, j;
    for (i = 0; i < n; i++) {
        double t = a + i * 2.1 + U_(rng, -0.3, 0.3), d = i ? U_(rng, 0.6, 1.6) : 0.0, c0 = polx(cx, d, t + 1.0), c1 = poly_(cy, d, t + 1.0), L, W, u1, u2, xs[64], h; FF f2 = ff_(c0, c1, t); double xy[8]; G *g; int hit = 0, b, k, nx; GL *dl = gl_new();
        L = U_(rng, 3.0, 3.8); W = U_(rng, 1.8, 2.3);
        xy[0] = fx(&f2, -L / 2, -W / 2); xy[1] = fy(&f2, -L / 2, -W / 2); xy[2] = fx(&f2, L / 2, -W / 2); xy[3] = fy(&f2, L / 2, -W / 2);
        u1 = L / 2 - U_(rng, 0.5, 1.4); xy[4] = fx(&f2, u1, W / 2); xy[5] = fy(&f2, u1, W / 2);
        u2 = -L / 2 + U_(rng, 0, 0.6); xy[6] = fx(&f2, u2, W / 2); xy[7] = fy(&f2, u2, W / 2);
        g = quad4(xy);
        for (j = 0; j < gs->n && !hit; j++) hit = dist(g, gs->v[j]) < 0.35;
        if (hit) continue;
        gl_add(gs, g); h = U_(rng, 0.45, 0.55); b = ob_up(ob, g, h, NULL);
        ob_cut(ob, barl(FX(f2, -L / 2 + 0.2, -W / 2 + 0.45), FX(f2, L / 2 - 0.2, -W / 2 + 0.45), 0.16), b, 0.1);
        nx = ns_arange(-L / 2 + 0.6, L / 2 - 1.2, 0.55, xs, 64);
        for (k = 0; k < nx; k++) gl_add(dl, disc(FX(f2, xs[k], W / 2 - 0.5), 0.13, 6));
        ob_cut(ob, inter(U(dl), inset(g, 0.25)), b, 0.1);
    }
    OUT_;
}
BODY(xo_vent_panel) {
    FF f = ff_(cx, cy, a); double L = U_(rng, 3.6, 4.2), W = U_(rng, 2.6, 3.0), h, sx, sy; SoObj *ob = obj_new("obmetal"); G *g = ob_chamfer(rect(cx, cy, L, W, a), 0.2); int b, i, j; GL *l = gl_new();
    h = U_(rng, 0.4, 0.5); b = ob_up(ob, g, h, NULL); ob_groove(ob, inset(g, 0.3), b, 0.15, 0.1); ob_vents(ob, &f, 0, 0, b, 5, L * 0.55, a, 0.42, 0.2, 0.2);
    for (i = -1; i <= 1; i += 2) for (j = -1; j <= 1; j += 2) gl_add(l, rect(FX(f, i * (L / 2 - 0.45), j * (W / 2 - 0.45)), 0.4, 0.4, a));
    ob_on(ob, U(l), b, 0.15, NULL);
    sx = CH(rng, -1, 1); sy = CH(rng, -1, 1);
    ob_on(ob, inter(tri3(FX(f, sx * L / 2, sy * W / 2), FX(f, sx * (L / 2 - 1.2), sy * W / 2), FX(f, sx * L / 2, sy * (W / 2 - 1.2))), inset(g, 0.13)), b, 0.3, NULL);
    OUT_;
}
static G *arcpoly(double ox, double oy, double r0, double a0, double a1, double r1, double b0, double b1, int n) {
    double *xy = (double *)malloc(sizeof(double) * 4 * (size_t)(n + 1)); G *g;
    ns_keep(xy, free);
    if (sd_arc(ox, oy, r0, a0, a1, n, xy) != SO_OK || sd_arc(ox, oy, r1, b0, b1, n, xy + 2 * (n + 1)) != SO_OK) ns_raise();
    g = polyv(xy, 2 * (n + 1)); return g;
}
BODY(xo_ring_segment) {
    double R = U_(rng, 4.0, 4.6), wd = U_(rng, 1.0, 1.2), sp = U_(rng, 1.3, 1.8), ox, oy, h, ts[6]; SoObj *ob = obj_new("obmetal"); G *band, *rb; int b, i; GL *l = gl_new();
    ox = polx(cx, -R, a); oy = poly_(cy, -R, a);
    band = arcpoly(ox, oy, R + wd / 2, a - sp / 2, a + sp / 2, R - wd / 2, a + sp / 2, a - sp / 2, 20);
    h = U_(rng, 0.45, 0.55); b = ob_up(ob, band, h, NULL);
    rb = arcpoly(ox, oy, R + 0.15, a - sp / 2 + 0.05, a + sp / 2 - 0.05, R - 0.15, a + sp / 2 - 0.05, a - sp / 2 + 0.05, 20);
    ob_on(ob, rb, b, 0.2, NULL);
    ns_linspace(-sp / 2 + 0.15, sp / 2 - 0.15, 6, ts);
    for (i = 0; i < 6; i++) gl_add(l, rect(POL(ox, oy, R + wd * 0.32, a + ts[i]), 0.32, 0.32, a + ts[i]));
    ob_cut(ob, U(l), b, 0.15);
    OUT_;
}
BODY(xo_conduit_run) {
    FF f = ff_(cx, cy, a); double L = U_(rng, 6.0, 7.0); SoObj *ob = obj_new("obmetal"); int hs, bx; G *jb;
    hs = ob_up(ob, ob_chamfer(rect(cx, cy, L, 0.95, a), 0.12), 0.6, NULL); ob_cut(ob, rect(cx, cy, L - 0.8, 0.36, a), hs, 0.3);
    jb = ob_chamfer(rect(FX(f, L / 2 + 0.6, 0), 1.5, 1.6, a), 0.2); bx = ob_up(ob, jb, 0.9, NULL); ob_groove(ob, inset(jb, 0.25), bx, 0.14, 0.1);
    { double u = U_(rng, -1.0, 0.5), ch = CH(rng, -1, 1), v = ch * U_(rng, 1.1, 1.4), c0 = fx(&f, u, v), c1 = fy(&f, u, v), t = a + U_(rng, 0.25, 0.5), Ls = U_(rng, 2.6, 3.2); int s2;
      s2 = ob_up(ob, ob_chamfer(rect(c0, c1, Ls, 0.95, t), 0.12), 0.6, NULL); ob_cut(ob, rect(c0, c1, 2.0, 0.36, t), s2, 0.3); }
    OUT_;
}
BODY(xo_floor_hatch) {
    FF f = ff_(cx, cy, a); double P_ = U_(rng, 3.4, 3.8); SoObj *ob = obj_new("obmetal"); G *g = ob_chamfer(rect(cx, cy, P_, P_, a), 0.2); int b;
    b = ob_up(ob, g, 0.35, NULL); ob_groove(ob, inset(g, 0.3), b, 0.16, 0.12); ob_groove(ob, inset(g, 0.75), b, 0.16, 0.12);
    ob_cut(ob, barl(FX(f, 0, -P_ / 2 + 0.4), FX(f, 0, P_ / 2 - 0.4), 0.18), b, 0.15); ob_cut(ob, rect(FX(f, 0.45, 0), 0.22, 0.7, a), b, 0.15);
    ob_studs(ob, corners4o(&f, P_ / 2 - 0.5, P_ / 2 - 0.5), b, 0.2, 0.15, NULL, 8);
    OUT_;
}
BODY(xo_star_dial) {
    double R = U_(rng, 2.3, 2.6), h; SoObj *ob = obj_new("obmetal"); int b, bs, k; GL *l = gl_new();
    { G *g = ngon(cx, cy, R, 24, a); h = U_(rng, 0.45, 0.55); b = ob_up(ob, g, h, NULL); }
    ob_groove(ob, ngon(cx, cy, R - 0.35, 24, a), b, 0.14, 0.1); ob_groove(ob, ngon(cx, cy, R * 0.55, 16, a), b, 0.14, 0.1);
    for (k = 0; k < 12; k++) gl_add(l, barl(POL(cx, cy, R * 0.62, a + 6.2832 * k / 12), POL(cx, cy, R - 0.55, a + 6.2832 * k / 12), 0.16));
    ob_cut(ob, U(l), b, 0.1);
    bs = ob_on(ob, ob_chamfer(rect(cx, cy, R * 0.45, R * 0.45, a + 0.785), 0.1), b, 0.25, NULL); ob_cut(ob, rect(cx, cy, R * 0.25, 0.2, a), bs, 0.1);
    { double d = R * U_(rng, 0.8, 1.0), t = a + U_(rng, 0, 6.28), r = U_(rng, 1.5, 1.9), hh = U_(rng, 0.7, 0.9); xo_drift(ob, POL(cx, cy, d, t), r, hh, rng); }
    OUT_;
}
BODY(xo_offering_dais) {
    FF f = ff_(cx, cy, a); double P_ = U_(rng, 4.0, 4.4); SoObj *ob = obj_new("monolith"); G *g = ob_chamfer(rect(cx, cy, P_, P_, a), 0.2); int b, s2, s3;
    b = ob_up(ob, g, 0.4, NULL); s2 = ob_on(ob, inset(g, 0.6), b, 0.4, NULL); s3 = ob_on(ob, inset(g, 1.15), s2, 0.4, NULL);
    ob_cut(ob, inset(g, 1.6), s3, 0.25); ob_groove(ob, inset(g, 0.25), b, 0.14, 0.1);
    ob_studs(ob, corners4o(&f, P_ / 2 - 1.3, P_ / 2 - 1.3), s3, 0.18, 0.12, NULL, 8);
    OUT_;
}
BODY(xo_glyph_cubes) {
    FF f = ff_(cx, cy, a); int n = (int)RI(rng, 2, 3), i, nt = 0, pick; SoObj *ob = obj_new("monolith"); double s; struct { int b; double c0, c1, t; G *g; } tops[4];
    s = U_(rng, 1.7, 1.9);
    for (i = 0; i < n; i++) {
        double u = (i - (n - 1) / 2.0) * (s + 0.35), v = U_(rng, -0.25, 0.25), c0 = fx(&f, u, v), c1 = fy(&f, u, v), t = a + U_(rng, -0.2, 0.2), h; G *g = ob_chamfer(rect(c0, c1, s, s, t), 0.12);
        h = U_(rng, 1.1, 1.3); tops[nt].b = ob_up(ob, g, h, NULL); tops[nt].c0 = c0; tops[nt].c1 = c1; tops[nt].t = t; tops[nt].g = g; nt++;
    }
    pick = (int)RB(rng, n);
    if (RR(rng) < 0.6) {
        double aa = tops[pick].t + U_(rng, -0.4, 0.4), h; G *g2 = ob_chamfer(rect(tops[pick].c0, tops[pick].c1, s * 0.75, s * 0.75, aa), 0.1); int b2;
        h = U_(rng, 0.8, 1.0); b2 = ob_on(ob, g2, tops[pick].b, h, NULL);
        tops[nt] = tops[pick]; tops[nt].b = b2; tops[nt].g = g2; nt++;
    }
    for (i = 0; i < nt; i++) ob_cut(ob, inter(ob_glyph(tops[i].c0, tops[i].c1, sqrt(area(tops[i].g)) * 0.72, tops[i].t, rng, 0.2), inset(tops[i].g, 0.2)), tops[i].b, 0.12);
    OUT_;
}
BODY(xo_lens_pedestal) {
    double r = U_(rng, 1.4, 1.6), h; SoObj *ob = obj_new("monolith"); G *g = ob_oct(cx, cy, r, a); int b, s2;
    b = ob_up(ob, g, 0.5, NULL); { G *q = ob_oct(cx, cy, r * 0.75, a); h = U_(rng, 0.9, 1.1); s2 = ob_on(ob, q, b, h, NULL); }
    ob_groove(ob, ob_oct(cx, cy, r * 0.75 - 0.22, a), s2, 0.14, 0.1);
    ob_on(ob, ngon(cx, cy, r * 0.42, 6, a), s2, 0.45, "obglow");
    OUT_;
}
BODY(xo_plinth_steps) {
    FF f = ff_(cx, cy, a); double W = U_(rng, 3.6, 4.2), td = U_(rng, 1.1, 1.3), rise; SoObj *ob = obj_new("obfloor"); int i;
    rise = U_(rng, 0.45, 0.55);
    for (i = 0; i < 3; i++) {
        double x = (i - 1) * td; G *g = rect(FX(f, x, 0), td + 0.02, W, a); int b = ob_up(ob, g, rise * (i + 1), NULL);
        ob_cut(ob, barl(FX(f, x - td / 2 + 0.3, -W / 2 + 0.3), FX(f, x - td / 2 + 0.3, W / 2 - 0.3), 0.16), b, 0.12);
        if (i) ob_cut(ob, ob_glyph(FX(f, x + 0.15, 0), td * 0.75, a, rng, 0.18), b, 0.12);
    }
    { double u = -1.5 * td - U_(rng, 0.3, 0.6), v = U_(rng, -W * 0.3, W * 0.3), c0 = fx(&f, u, v), c1 = fy(&f, u, v), r = U_(rng, 1.6, 2.1), h = U_(rng, 0.8, 1.0); xo_drift(ob, c0, c1, r, h, rng); }
    OUT_;
}
BODY(xo_sand_drift) {
    SoObj *ob = obj_new("sand"); FF f = ff_(cx, cy, a);
    { double r = U_(rng, 2.0, 2.6), h = U_(rng, 0.7, 1.0); xo_drift(ob, cx, cy, r, h, rng); }
    { double u = U_(rng, 1.5, 2.2), v = U_(rng, -1.2, 1.2), c0 = fx(&f, u, v), c1 = fy(&f, u, v), r = U_(rng, 1.1, 1.5), h = U_(rng, 0.45, 0.6); xo_drift(ob, c0, c1, r, h, rng); }
    OUT_;
}
BODY(xo_stone_chips) {
    SoObj *ob = obj_new("obsidian"); FF f = ff_(cx, cy, a);
    xo_chips(ob, &f, -1.8, 1.8, -1.2, 1.2, rng, "obsidian", 4, 6);
    OUT_;
}
BODY(xo_glyph_marks) {
    SoObj *ob = obj_new("slot"); double L = U_(rng, 5.0, 7.0), c = U_(rng, 1.3, 1.5); G *g = ob_band(cx, cy, a, L, c, rng, 0.25, 1);
    ob_gcut(ob, g, 0.3, "slot");
    OUT_;
}
BODY(xo_wind_ripples) {
    SoObj *ob = obj_new("rut"); FF f = ff_(cx, cy, a); int n = (int)RI(rng, 4, 6), i; double st = U_(rng, 0.8, 1.0); GL *gs = gl_new();
    for (i = 0; i < n; i++) {
        double v = (i - (n - 1) / 2.0) * st, L = U_(rng, 3.0, 4.2), w; PTS *p = pl_new();
        pl_add(p, FX(f, -L / 2, v)); w = v + U_(rng, 0.2, 0.4); pl_add(p, FX(f, 0, w)); pl_add(p, FX(f, L / 2, v));
        gl_add(gs, bufx(line(p), 0.17, 16, SHP_CAP_ROUND, SHP_JOIN_ROUND, 5.0));
    }
    ob_gcut(ob, U(gs), 0.22, "rut");
    OUT_;
}

#define SD(fn, cls, kind, name) static const NsSd D_##fn = { fn, "obelis", cls, kind, name, 1 };
SD(xo_fallen_monolith, "made", "monolith", "fallen_monolith") SD(xo_standing_stub, "made", "monolith", "standing_stub") SD(xo_glyph_stele, "made", "monolith", "glyph_stele")
SD(xo_capstone, "made", "monolith", "capstone") SD(xo_obelisk_tip, "made", "monolith", "obelisk_tip") SD(xo_half_pylon, "made", "monolith", "half_pylon")
SD(xo_toppled_pylon, "made", "monolith", "toppled_pylon") SD(xo_pylon_ring, "made", "monolith", "pylon_ring") SD(xo_casket_, "made", "casket", "casket")
SD(xo_open_casket, "made", "casket", "open_casket") SD(xo_vessel_row, "made", "casket", "vessel_row") SD(xo_metal_shards, "made", "obmetal", "metal_shards")
SD(xo_vent_panel, "made", "obmetal", "vent_panel") SD(xo_ring_segment, "made", "obmetal", "ring_segment") SD(xo_conduit_run, "made", "obmetal", "conduit_run")
SD(xo_floor_hatch, "made", "obmetal", "floor_hatch") SD(xo_star_dial, "made", "obmetal", "star_dial") SD(xo_offering_dais, "made", "monolith", "offering_dais")
SD(xo_glyph_cubes, "made", "monolith", "glyph_cubes") SD(xo_lens_pedestal, "made", "monolith", "lens_pedestal") SD(xo_plinth_steps, "made", "monolith", "plinth_steps")
SD(xo_sand_drift, "debris", "sand", "sand_drift") SD(xo_stone_chips, "debris", "obsidian", "stone_chips") SD(xo_glyph_marks, "marks", "slot", "glyph_marks")
SD(xo_wind_ripples, "marks", "rut", "wind_ripples")
void ns_reg_obelis(void) {
    const NsSd *all[] = { &D_xo_fallen_monolith, &D_xo_standing_stub, &D_xo_glyph_stele, &D_xo_capstone, &D_xo_obelisk_tip, &D_xo_half_pylon, &D_xo_toppled_pylon,
        &D_xo_pylon_ring, &D_xo_casket_, &D_xo_open_casket, &D_xo_vessel_row, &D_xo_metal_shards, &D_xo_vent_panel, &D_xo_ring_segment, &D_xo_conduit_run,
        &D_xo_floor_hatch, &D_xo_star_dial, &D_xo_offering_dais, &D_xo_glyph_cubes, &D_xo_lens_pedestal, &D_xo_plinth_steps, &D_xo_sand_drift, &D_xo_stone_chips,
        &D_xo_glyph_marks, &D_xo_wind_ripples };
    int i; for (i = 0; i < (int)(sizeof all / sizeof *all); i++) ns_sd_reg(all[i], 10.5, all[i]->body == xo_glyph_marks || all[i]->body == xo_wind_ripples);
}
