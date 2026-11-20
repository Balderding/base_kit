/* bld_ns_blight.c: style_blight.py's own builders (b_thorn_cluster, b_bone_heap, b_boiling_pool, b_rune_ring,
   b_torn_rift, b_flesh_rock and the landmarks L_thorns, L_rift, L_rune_ring, L_boil, L_bones) and the 25 style-only
   details of sd_r_x_blight.py, copied routine by routine (each names the Python it copies). See bld_ns.h for how
   they are written; nscap.py is the proof. */
#include "bld_ns.h"

/* style_blight.BLIGHT_RUNES: each rune a few strokes ((x0, y0), (x1, y1)) in a box from -1 to 1 */
typedef struct { int n; double s[5][4]; } Rune;
static const Rune RUNES[8] = {
    { 3, { { 0, -1, 0, 1 }, { 0, 1, 0.6, 0.55 }, { 0.6, 0.55, 0.6, 0.1 } } },
    { 3, { { -0.6, 1, 0.25, 0.35 }, { 0.25, 0.35, -0.25, -0.35 }, { -0.25, -0.35, 0.6, -1 } } },
    { 3, { { 0, -1, 0, 1 }, { 0, 0.45, 0.55, 0.85 }, { 0, -0.15, -0.55, 0.2 } } },
    { 3, { { -0.6, -0.9, 0, 0.9 }, { 0, 0.9, 0.15, -0.2 }, { 0.15, -0.2, 0.6, 0.3 } } },
    { 5, { { 0, 0.2, 0, 1 }, { -0.42, -0.25, 0, -0.67 }, { 0, -0.67, 0.42, -0.25 }, { 0.42, -0.25, 0, 0.17 }, { 0, 0.17, -0.42, -0.25 } } },
    { 3, { { -0.3, -1, -0.3, 1 }, { -0.3, 0.5, 0.45, 0.15 }, { -0.3, -0.25, 0.45, -0.6 } } },
    { 3, { { 0.5, -1, -0.2, -0.2 }, { -0.2, -0.2, 0.3, 0.4 }, { 0.3, 0.4, -0.4, 1 } } },
    { 3, { { -0.55, -0.8, 0.55, -0.8 }, { -0.35, -0.8, -0.1, 0.9 }, { 0.35, -0.8, 0.5, 0.3 } } },
};
#define NRUNES 8

/* style_blight.rune(f2, k, s, w): unary_union of the strokes, each LineString.buffer(w / 2, cap_style=1, resolution=3) */
static G *rune(const FF *f2, int64_t k, double s, double w) {
    const Rune *r = &RUNES[((k % NRUNES) + NRUNES) % NRUNES]; GL *l = gl_new(); int i;
    for (i = 0; i < r->n; i++) {
        const double *q = r->s[i]; double xy[4];
        xy[0] = fx(f2, q[0] * s * 0.5, q[1] * s * 0.5); xy[1] = fy(f2, q[0] * s * 0.5, q[1] * s * 0.5);
        xy[2] = fx(f2, q[2] * s * 0.5, q[3] * s * 0.5); xy[3] = fy(f2, q[2] * s * 0.5, q[3] * s * 0.5);
        gl_add(l, bufx(T(shp_linestring(xy, 2)), w / 2, 3, SHP_CAP_ROUND, SHP_JOIN_ROUND, 5.0));
    }
    return uu(l);
}
/* sd_r_x_blight.bl_runes(f2, k, s, w): U of capl strokes */
static G *bl_runes(const FF *f2, int64_t k, double s, double w) {
    const Rune *r = &RUNES[((k % NRUNES) + NRUNES) % NRUNES]; GL *l = gl_new(); int i;
    for (i = 0; i < r->n; i++) {
        const double *q = r->s[i];
        gl_add(l, capl(fx(f2, q[0] * s * 0.5, q[1] * s * 0.5), fy(f2, q[0] * s * 0.5, q[1] * s * 0.5), fx(f2, q[2] * s * 0.5, q[3] * s * 0.5), fy(f2, q[2] * s * 0.5, q[3] * s * 0.5), w));
    }
    return U(l);
}

/* style_blight._spike_solid / sd_r_x_blight.bl_spike: (sect, h), one twisted thorn */
typedef struct { double cx, cy, r, h, lean, la, twist, a0, inner; int pts; } Spike;
static int spike_sect(void *c, double z, G **out) {
    const Spike *s = (const Spike *)c; double t, rr, pw; int err = 0;
    *out = NULL;
    if (s->h == 0.0) return SO_ERR;          /* ZeroDivisionError */
    t = z / s->h;
    if (t >= 1) return SO_OK;
    pw = lf_fpow(1 - t, 0.9, &err); if (err) return SO_ERR;
    rr = s->r * pw;
    if (rr < 0.24) return SO_OK;
    *out = shp_star(s->cx + s->lean * t * ns_cos(s->la), s->cy + s->lean * t * ns_sin(s->la), rr, rr * s->inner, s->pts, s->a0 + s->twist * t);
    return *out ? SO_OK : SO_ERR;
}
static void spike(SoSolids *sol, double cx, double cy, double r, double h, double lean, double la, double twist, double a0, int pts, double inner) {
    Spike s = { cx, cy, r, h, lean, la, twist, a0, inner, pts };
    sl_custom(sol, spike_sect, &s, sizeof s, h);
}

/* rfun lambdas */
typedef struct { double a, b, lo, hi, r; } Knuckle;          /* a if t < lo or t > hi else b */
static int knuckle(void *c, double t, double *out) { const Knuckle *k = (const Knuckle *)c; *out = (t < k->lo || t > k->hi) ? k->a : k->b; return 0; }
typedef struct { double a, b; } Lin;                         /* a - b * t */
static int lin(void *c, double t, double *out) { const Lin *k = (const Lin *)c; *out = k->a - k->b * t; return 0; }
static void rod2(SoSolids *sol, const double *pts, int n, SoNumFn f, void *ctx, double r, double hk, double lift) {
    if (sd_rod(sol, pts, n, r, f, ctx, hk, lift, 0.0) != SO_OK) ns_raise();
}

/* ================================================================== style_blight.py builders */
typedef struct { const char *kind, *k2; double n0, n1; int lm; } BlCtx;

/* b_thorn_cluster(kind="thorn", n=(3, 6)).f(cx, cy, a, rng, k=1.0) */
static int thorn_cluster(const BlCtx *c, double cx, double cy, double a, PyRandom *rng, double k, NsOut *o, G **foot) {
    double kk = fmax(1.0, ns_pow(k, 0.6)), k35 = fmax(1.0, ns_pow(k, 0.35)), r0, h0, lean, la, tw, a0; int pts, i, n; SoSolids *sol = sl_new(); GL *ft = gl_new();
    r0 = U_(rng, 1.0, 1.3) * kk;
    { double cap = SC.CAP - SC.H - 0.1, v = U_(rng, 2.6, 3.2) * k35; h0 = v < cap ? v : cap; }
    lean = r0 * 0.45; la = U_(rng, 0, 6.283); tw = U_(rng, 0.8, 1.6); tw *= CH(rng, -1, 1); a0 = U_(rng, 0, 3); pts = (int)CH(rng, 4, 5);
    spike(sol, cx, cy, r0, h0, lean, la, tw, a0, pts, 0.62); gl_add(ft, PB(cx, cy, r0, 16));
    n = (int)RI(rng, (int64_t)c->n0, (int64_t)c->n1) + (int)trunc(2 * (k - 1));
    for (i = 0; i < n; i++) {
        double t = a + U_(rng, 0, 6.283), d = U_(rng, 1.6, 2.4) * kk, r = U_(rng, 0.55, 0.85) * kk, x = cx + d * ns_cos(t), y = cy + d * ns_sin(t); G *g = PB(x, y, r, 16); int ok = 1, j;
        for (j = 0; j < ft->n && ok; j++) ok = dist(g, ft->v[j]) > 0.2;
        if (ok) {
            double cap = SC.CAP - SC.H - 0.3, v, h, la2, tw2, a2; int p2;
            gl_add(ft, g);
            v = U_(rng, 1.4, 2.6) * k35; h = v < cap ? v : cap;
            la2 = t + U_(rng, -0.5, 0.5); tw2 = U_(rng, 0.8, 2.0); tw2 *= CH(rng, -1, 1); a2 = U_(rng, 0, 3); p2 = (int)CH(rng, 3, 4);
            spike(sol, x, y, r, h, r * 0.5, la2, tw2, a2, p2, 0.62);
        }
    }
    *foot = buf(chull(u_(ft)), 0.2, 16);
    ns_st(o, sol, c->kind, 0, 0, NULL);
    return 0;
}

/* b_bone_heap(kind="bone").f */
static int bone_heap(const BlCtx *c, double cx, double cy, double a, PyRandom *rng, double k, NsOut *o, G **foot) {
    double kk = fmax(1.0, ns_pow(k, 0.7)), R_ = U_(rng, 1.6, 2.0) * kk, sq, sx, sy; int i, n; SoSolids *sol = sl_new(), *l1 = sl_new(); NsCuts cu = { 0 };
    double prof[8] = { R_, 0.0, R_ * 0.8, 0.35, R_ * 0.45, 0.6, 0.3, 0.7 }, pc[16];
    sq = U_(rng, 0.7, 0.9);
    if (so_lathe(l1, cx, cy, prof, 4, 14, sq, a, 0.0, NULL, 0) != SO_OK || l1->n < 1) ns_raise();
    sol_push(sol, l1->v[0]);
    n = (int)RI(rng, 3, 5) + (int)trunc(2 * (k - 1));
    for (i = 0; i < n; i++) {
        double t = U_(rng, 0, PI), L = U_(rng, 2.6, 3.6) * kk, ox = cx + U_(rng, -0.7, 0.7) * kk, oy = cy + U_(rng, -0.7, 0.7) * kk, lift; FF f = ff_(ox, oy, t);
        double pts[4] = { FX(f, -L / 2, 0), FX(f, L / 2, 0) }; Knuckle kn = { 0.42, 0.28, 0.1, 0.9 };
        lift = U_(rng, 0.0, 0.3);
        if (so_tube(sol, pts, 2, 0.0, knuckle, &kn, 1.2, lift, 0, 0.0) != SO_OK) ns_raise();
    }
    sx = cx + U_(rng, -0.5, 0.5); sy = cy + U_(rng, -0.5, 0.5);
    so_curve(0.75, 0.22, 0.0, 0.9, 6, 1, pc);
    if (so_lathe(sol, sx, sy, pc, 7, 16, 0.8, a + 1.2, 0.45, NULL, 0) != SO_OK) ns_raise();
    for (i = -1; i <= 1; i += 2) cut_add(&cu, PB(sx + 0.25 * ns_cos(a + 1.2 + i * 0.6), sy + 0.25 * ns_sin(a + 1.2 + i * 0.6), 0.17, 6), 1.0);
    *foot = PB(cx, cy, R_ + 1.6 * kk, 16);
    ns_st(o, sol, c->kind, 0, 0, &cu);
    return 0;
}

/* b_boiling_pool(water="ichor", rim="blister").f */
static int boiling_pool(const BlCtx *c, double cx, double cy, double a, PyRandom *rng, double k, NsOut *o, G **foot) {
    double kk = fmax(1.0, ns_pow(k, 0.8)), k3 = fmax(1.0, ns_pow(k, 0.3)), R_, b[4]; G *pool; GL *bub = gl_new(), *out = gl_new(), *all = gl_new(); int i, n;
    R_ = U_(rng, 2.0, 2.6) * kk; pool = blob(cx, cy, R_, rng, 5);
    { double d = U_(rng, 0.6, 0.8); ns_gcut(o, pool, d, c->kind); }
    bounds(pool, b);
    for (i = 0; i < 120; i++) {
        double x, y, r; G *bb; int ok, j;
        if (bub->n >= RI(rng, 3, 5) + (int64_t)trunc(3 * (k - 1))) break;
        x = U_(rng, b[0], b[2]); y = U_(rng, b[1], b[3]); r = U_(rng, 0.5, 0.8) * k3; bb = buf(P(x, y), r, 14);
        ok = contains(inset(pool, 0.35), bb);
        for (j = 0; j < bub->n && ok; j++) ok = dist(bb, bub->v[j]) > 0.35;
        if (ok) gl_add(bub, bb);
    }
    for (i = 0; i < bub->n; i++) ns_pad(o, bub->v[i], c->k2);
    n = (int)RI(rng, 6, 9) + (int)trunc(5 * (k - 1));
    for (i = 0; i < n; i++) {
        double t = a + 6.283 * i / n + U_(rng, -0.2, 0.2), d = R_ + U_(rng, 0.5, 1.0) * kk, rr; G *q; int ok, j;
        rr = U_(rng, 0.55, 0.85) * kk;
        q = stone(cx + d * ns_cos(t), cy + d * ns_sin(t), rr, rng, 0.85, 7);
        ok = dist(q, pool) > 0.2;
        for (j = 0; j < out->n && ok; j++) ok = dist(q, out->v[j]) > 0.2;
        if (ok) { double h; gl_add(out, q); h = U_(rng, 0.45, 0.8); ns_mound(o, q, h, c->k2, &NS_SOFT_T); }
    }
    gl_add(all, pool); for (i = 0; i < out->n; i++) gl_add(all, out->v[i]);
    *foot = buf(chull(u_(all)), 0.3, 16);
    return 0;
}

/* b_rune_ring(kind="runecut", crack="rift", R=(3.0, 3.8)).f */
static int rune_ring(const BlCtx *c, double cx, double cy, double a, PyRandom *rng, double k, NsOut *o, G **foot) {
    double Rr = U_(rng, c->n0, c->n1) * k, w = 0.38, s_; GL *g = gl_new(), *cr = gl_new(); int i, n; G *crr, *marks;
    gl_add(g, T(so_ann(cx, cy, Rr, Rr - w, 48))); gl_add(g, T(so_ann(cx, cy, Rr * 0.62, Rr * 0.62 - w * 0.85, 40)));
    n = (int)RI(rng, 6, 8) + (int)trunc(4 * (k - 1));
    s_ = fmin(1.5, (Rr * 0.38 - 0.1) * 1.7); s_ = ((Rr * 0.38 - 0.1) * 1.7 < 1.5 ? (Rr * 0.38 - 0.1) * 1.7 : 1.5) * 1.0;
    for (i = 0; i < n; i++) {
        double t = a + 6.283 * i / n, rm = Rr * 0.81; FF f = ff_(cx + rm * ns_cos(t), cy + rm * ns_sin(t), t + PI / 2);
        gl_add(g, rune(&f, RB(rng, NRUNES), s_, 0.3));
    }
    { FF f = ff_(cx, cy, a); gl_add(g, rune(&f, RB(rng, NRUNES), Rr * 0.7, 0.36)); }
    n = (int)RI(rng, 1, 2);
    for (i = 0; i < n; i++) {
        double ca = a + U_(rng, 0, 6.283), L = Rr + 1.2, u1, u2; PTS *w_;
        u1 = U_(rng, 0.2, 1.0); { double ax = cx - L * ns_cos(ca) * u1; u2 = U_(rng, 0.2, 1.0);
        w_ = wiggle(ax, cy - L * ns_sin(ca) * u2, cx + L * ns_cos(ca), cy + L * ns_sin(ca), rng, 0.6, 6); }
        gl_add(cr, bufx(line(w_), 0.26, 3, SHP_CAP_ROUND, SHP_JOIN_ROUND, 5.0));
    }
    crr = u_(cr); marks = dif(u_(g), buf(crr, 0.35, 16));
    ns_gcut(o, marks, 0.35, c->kind); ns_gcut(o, crr, 0.55, c->k2);
    n = (int)RI(rng, 0, 2);
    for (i = 0; i < n; i++) {
        G *p = rpt(crr), *q; double x = gx(p) + U_(rng, -1, 1), y = gy(p) + U_(rng, -1, 1), r = U_(rng, 0.5, 0.7);
        q = stone(x, y, r, rng, 0.7, 4);
        if (dist(q, crr) > 0.2) { double h = U_(rng, 0.4, 0.6); ns_mound(o, q, h, "blightrock", &NS_PEB_T); }
    }
    *foot = T(so_disc(cx, cy, Rr + 0.5, 32));
    return 0;
}

/* b_torn_rift(kind="rift", lip="blightrock", L=(7.0, 9.5)).f */
static double rift_half(double W, double u, double Ln) {
    double q = 1 - ns_pow(2 * u / Ln, 2.0);
    return W * ns_pow(q > 0.0 ? q : 0.0, 0.7);
}
static int torn_rift(const BlCtx *c, double cx, double cy, double a, PyRandom *rng, double k, NsOut *o, G **foot) {
    double Ln = U_(rng, c->n0, c->n1) * k, W = U_(rng, 1.2, 1.7) * fmax(1.0, ns_pow(k, 0.6)), us[15]; FF f = ff_(cx, cy, a); int m = 14, i, n, sx;
    PTS *top = pl_new(), *bot = pl_new(), *all = pl_new(); G *slot, *ends; GL *el = gl_new(), *ft = gl_new();
    for (i = 0; i <= m; i++) us[i] = -Ln / 2 + Ln * i / m;
    for (i = 0; i <= m; i++) { double v = W * ns_pow(fmax(0.0, 1 - ns_pow(2 * us[i] / Ln, 2.0)), 0.7), u1, u2; u1 = U_(rng, 0.6, 1.0); u2 = U_(rng, -0.15, 0.15); v = v * u1 + u2; pl_add(top, FX(f, us[i], v)); }
    for (i = 0; i <= m; i++) { double v = -W * ns_pow(fmax(0.0, 1 - ns_pow(2 * us[i] / Ln, 2.0)), 0.7), u1, u2; u1 = U_(rng, 0.6, 1.0); u2 = U_(rng, -0.15, 0.15); v = v * u1 + u2; pl_add(bot, FX(f, us[i], v)); }
    for (i = 0; i < top->n; i++) pl_add(all, top->v[2 * i], top->v[2 * i + 1]);
    for (i = bot->n - 1; i >= 0; i--) pl_add(all, bot->v[2 * i], bot->v[2 * i + 1]);
    slot = buf(poly(all), 0, 16);
    if (empty(slot) || area(slot) < 2) return 1;
    for (sx = -1; sx <= 1; sx += 2) {
        double u1, u2; PTS *w_;
        u1 = U_(rng, 1.5, 3.0); { double bu = sx * (Ln / 2 + u1 * k); u2 = U_(rng, -1, 1);
        w_ = wiggle(FX(f, sx * Ln / 2 - sx * 0.3, 0), FX(f, bu, u2), rng, 0.4, 3); }
        gl_add(el, bufx(line(w_), 0.24, 3, SHP_CAP_ROUND, SHP_JOIN_ROUND, 5.0));
    }
    ends = u_(el);
    { double d = U_(rng, 0.85, 1.0); ns_gcut(o, slot, d, c->kind); }
    ns_gcut(o, dif(ends, slot), 0.5, c->kind);
    gl_add(ft, slot); gl_add(ft, ends);
    n = (int)RI(rng, 5, 8) + (int)trunc(4 * (k - 1));
    for (i = 0; i < n; i++) {
        double u = U_(rng, -Ln * 0.4, Ln * 0.4), sv = CH(rng, -1, 1), half = rift_half(W, u, Ln), v, r, asp; int nv, ok, j; G *q;
        v = sv * (half + U_(rng, 0.9, 1.3)); r = U_(rng, 0.65, 0.95) * fmax(1.0, ns_pow(k, 0.4)); asp = U_(rng, 0.55, 0.8); nv = (int)RI(rng, 4, 5);
        q = stone(FX(f, u, v), r, rng, asp, nv);
        ok = dist(q, slot) > 0.25;
        for (j = 2; j < ft->n && ok; j++) ok = dist(q, ft->v[j]) > 0.25;
        if (ok) { double h; gl_add(ft, q); h = U_(rng, 0.6, 1.1); ns_mound(o, q, h, c->k2, &NS_ROCK_T); }
    }
    *foot = buf(chull(u_(ft)), 0.3, 16);
    return 0;
}

/* b_flesh_rock(kind="fleshrock", pock="blister").f */
static int flesh_rock(const BlCtx *c, double cx, double cy, double a, PyRandom *rng, double k, NsOut *o, G **foot) {
    double kk = fmax(1.0, ns_pow(k, 0.6)); SoSolids *sol = sl_new(); GL *ft = gl_new(); int i, n;
    n = (int)RI(rng, 3, 4);
    for (i = 0; i < n; i++) {
        double t = U_(rng, 0, 6.283), d, r, x, y, h, sq, pc[16];
        d = i == 0 ? 0.0 : U_(rng, 1.0, 1.6) * kk; r = U_(rng, 1.0, 1.4) * kk * (i == 0 ? 1.0 : 0.8); x = cx + d * ns_cos(t); y = cy + d * ns_sin(t);
        h = U_(rng, 1.0, 1.5) * (i == 0 ? 1.0 : 0.75);
        so_curve(r, 0.25, 0.0, h, 7, 1, pc); sq = U_(rng, 0.75, 0.95);
        if (so_lathe(sol, x, y, pc, 8, 16, sq, t, 0.0, NULL, 0) != SO_OK) ns_raise();
        gl_add(ft, PB(x, y, r, 16));
    }
    ns_st(o, sol, c->kind, 0, 0, NULL);
    n = (int)RI(rng, 1, 2);
    for (i = 0; i < n; i++) {
        double t = U_(rng, 0, 6.283), d = U_(rng, 1.9, 2.6) * kk, x = cx + d * ns_cos(t), y = cy + d * ns_sin(t), r = U_(rng, 0.45, 0.6); G *q = PB(x, y, r, 12);
        if (dist(q, u_(ft)) > 0.2) { double h; gl_add(ft, q); h = U_(rng, 0.45, 0.6); ns_mound(o, q, h, c->k2, &NS_SOFT_T); }
    }
    *foot = buf(chull(u_(ft)), 0.2, 16);
    return 0;
}

typedef int (*BlBody)(const BlCtx *c, double cx, double cy, double a, PyRandom *rng, double k, NsOut *o, G **foot);
typedef struct { BlCtx c; BlBody body; double kmul, kpow; int lm; } BlFn;          /* lm: L_x(...)(cx, cy, a, rng, k) -> body(k * kmul (k ** kpow when kpow)) */
static int bl_call(void *ctx, double cx, double cy, double a, PyRandom *rng, int has_scale, double scale, Geo **foot, SoLayers *layers) {
    const BlFn *b = (const BlFn *)ctx; SoLayers lay = { 0 }; NsOut o = { &lay }; G *f = NULL; int r, i; double k;
    *foot = NULL;
    if (b->lm && !has_scale) return SC_ERR;          /* f(cx, cy, a, rng, k): k is required */
    {
        NsFrame fr_; ns_enter(&fr_);
        if (setjmp(fr_.jb)) { ns_leave(&fr_); lyl_free(&lay); return SC_ERR; }
        k = has_scale ? scale : 1.0;
        if (b->lm && b->kpow) k = b->kmul * fmax(1.0, ns_pow(k, b->kpow));
        else if (b->lm) k = b->kmul * k;
        r = b->body(&b->c, cx, cy, a, rng, k, &o, &f);
        if (r == 0) *foot = f ? geo_new(shp_clone(f)) : NULL;
        ns_leave(&fr_);
    }
    if (r == 1) { lyl_free(&lay); return 1; }
    for (i = 0; i < lay.n; i++) lyl_push(layers, lay.v[i]);
    lyl_free(&lay);
    return 0;
}
static int bl_make(const FtCall *call, FtBuildFn *fn, void **ctx, BlBody body, const char *k0, const char *k1d, int has_pair, double p0, double p1,
                   int lm, double kmul, double kpow) {
    BlFn *b = (BlFn *)calloc(1, sizeof *b);
    b->body = body; b->lm = lm; b->kmul = kmul; b->kpow = kpow;
    b->c.kind = so_intern(ns_arg_s(call, 0, lm ? NULL : "kind", k0));
    b->c.k2 = k1d ? so_intern(ns_arg_s(call, 1, NULL, k1d)) : NULL;
    b->c.n0 = p0; b->c.n1 = p1;
    if (has_pair && !lm && ns_arg_pair(call, k1d ? 2 : 1, NULL, p0, p1, &b->c.n0, &b->c.n1) < 0) { free(b); return SC_ERR; }
    bld_keep(b, free); *fn = bl_call; *ctx = b;
    return 0;
}
/* the kw names of the factories: thorn_cluster(kind, n), bone_heap(kind), boiling_pool(water, rim), rune_ring(kind, crack, R),
   torn_rift(kind, lip, L), flesh_rock(kind, pock); landmarks pass the pair themselves */
static int f_thorn_cluster(const FtCall *c, FtBuildFn *fn, void **x) { return bl_make(c, fn, x, thorn_cluster, "thorn", NULL, 1, 3, 6, 0, 1, 0); }
static int f_bone_heap(const FtCall *c, FtBuildFn *fn, void **x) { return bl_make(c, fn, x, bone_heap, "bone", NULL, 0, 0, 0, 0, 1, 0); }
static int f_boiling_pool(const FtCall *c, FtBuildFn *fn, void **x) { return bl_make(c, fn, x, boiling_pool, "ichor", "blister", 0, 0, 0, 0, 1, 0); }
static int f_rune_ring(const FtCall *c, FtBuildFn *fn, void **x) { return bl_make(c, fn, x, rune_ring, "runecut", "rift", 1, 3.0, 3.8, 0, 1, 0); }
static int f_torn_rift(const FtCall *c, FtBuildFn *fn, void **x) { return bl_make(c, fn, x, torn_rift, "rift", "blightrock", 1, 7.0, 9.5, 0, 1, 0); }
static int f_flesh_rock(const FtCall *c, FtBuildFn *fn, void **x) { return bl_make(c, fn, x, flesh_rock, "fleshrock", "blister", 0, 0, 0, 0, 1, 0); }
/* L_thorns(kind): b_thorn_cluster(kind, (6, 9))(..., 1.8 * max(1.0, k ** 0.6)); L_rift: b_torn_rift(kind, lip, (13.0, 17.0))(..., k);
   L_rune_ring: b_rune_ring(kind, crack, (5.5, 7.0))(..., k); L_boil: b_boiling_pool(water, rim)(..., 1.7 * k); L_bones: b_bone_heap(kind)(..., 1.6 * k) */
static int f_L_thorns(const FtCall *c, FtBuildFn *fn, void **x) { return bl_make(c, fn, x, thorn_cluster, "thorn", NULL, 1, 6, 9, 1, 1.8, 0.6); }
static int f_L_rift(const FtCall *c, FtBuildFn *fn, void **x) { return bl_make(c, fn, x, torn_rift, "rift", "blightrock", 1, 13.0, 17.0, 1, 1, 0); }
static int f_L_rune_ring(const FtCall *c, FtBuildFn *fn, void **x) { return bl_make(c, fn, x, rune_ring, "runecut", "rift", 1, 5.5, 7.0, 1, 1, 0); }
static int f_L_boil(const FtCall *c, FtBuildFn *fn, void **x) { return bl_make(c, fn, x, boiling_pool, "ichor", "blister", 0, 0, 0, 1, 1.7, 0); }
static int f_L_bones(const FtCall *c, FtBuildFn *fn, void **x) { return bl_make(c, fn, x, bone_heap, "bone", NULL, 0, 0, 0, 1, 1.6, 0); }

void ns_bld_blight(void) {
    bld_add("style_blight", "b_thorn_cluster", f_thorn_cluster); bld_add("style_blight", "b_bone_heap", f_bone_heap);
    bld_add("style_blight", "b_boiling_pool", f_boiling_pool); bld_add("style_blight", "b_rune_ring", f_rune_ring);
    bld_add("style_blight", "b_torn_rift", f_torn_rift); bld_add("style_blight", "b_flesh_rock", f_flesh_rock);
    bld_add("style_blight", "L_thorns", f_L_thorns); bld_add("style_blight", "L_rift", f_L_rift); bld_add("style_blight", "L_rune_ring", f_L_rune_ring);
    bld_add("style_blight", "L_boil", f_L_boil); bld_add("style_blight", "L_bones", f_L_bones);
    /* the same under the names FT_NEW / FT_LMS write (no "b_" / "L_"): every b_ name, and the L_ names that do not clash
       ("rune_ring" is both b_rune_ring and L_rune_ring: that name gives b_rune_ring; L_rune_ring needs its full name) */
    bld_add("style_blight", "thorn_cluster", f_thorn_cluster); bld_add("style_blight", "bone_heap", f_bone_heap); bld_add("style_blight", "boiling_pool", f_boiling_pool);
    bld_add("style_blight", "rune_ring", f_rune_ring); bld_add("style_blight", "torn_rift", f_torn_rift); bld_add("style_blight", "flesh_rock", f_flesh_rock);
    bld_add("style_blight", "thorns", f_L_thorns); bld_add("style_blight", "rift", f_L_rift); bld_add("style_blight", "boil", f_L_boil); bld_add("style_blight", "bones", f_L_bones);
}

/* ================================================================== sd_r_x_blight.py */
#define BODY(nm) static int nm(double cx, double cy, double a, PyRandom *rng, NsOut *o, G **foot)

/* bl_skull(x, y, r, a, lift=0.0): (solids, cuts) */
static void bl_skull(SoSolids *sol, NsCuts *cu, double x, double y, double r, double a, double lift) {
    FF f = ff_(x, y, a); double h = r * 1.05; int sv;
    if (sd_dome(sol, x, y, r, h, lift, 0.82, a, 20) != SO_OK) ns_raise();
    if (sd_dome(sol, FX(f, r * 0.55, 0), r * 0.62, h * 0.7, lift, 0.8, a, 20) != SO_OK) ns_raise();
    for (sv = -1; sv <= 1; sv += 2) cut_add(cu, disc(FX(f, r * 0.55, sv * r * 0.36), r * 0.24, 8), lift + h * 0.45);
    cut_add(cu, tri(FX(f, r * 0.95, 0), FX(f, r * 0.72, 0), r * 0.26), lift + h * 0.3);
}
/* bl_bone(p, q, r=0.26): rod([p, q], lambda t: r * (1.55 if t < 0.08 or t > 0.92 else 1.0), 1.1) */
static void bl_bone(SoSolids *sol, double px, double py, double qx, double qy, double r) {
    double pts[4] = { px, py, qx, qy }; Knuckle k = { r * 1.55, r * 1.0, 0.08, 0.92 };
    rod2(sol, pts, 2, knuckle, &k, 0.0, 1.1, 0.0);
}

BODY(sd_twisted_spike) {
    double r = U_(rng, 1.0, 1.3), h, la, tw, a0; int pts, i, n; SoSolids *sol = sl_new(); GL *ft = gl_new();
    h = U_(rng, 2.7, 3.2); la = U_(rng, 0, 6.28); tw = U_(rng, 1.0, 1.8); tw *= CH(rng, -1, 1); a0 = U_(rng, 0, 3); pts = (int)CH(rng, 4, 5);
    spike(sol, cx, cy, r, h, r * 0.5, la, tw, a0, pts, 0.62); gl_add(ft, disc(cx, cy, r, 20));
    n = (int)RI(rng, 2, 3);
    for (i = 0; i < n; i++) {
        double t = a + i * 2.2 + U_(rng, -0.4, 0.4), d = r + U_(rng, 0.8, 1.2), x = cx + d * ns_cos(t), y = cy + d * ns_sin(t), rr = U_(rng, 0.5, 0.7), hh, tw2, a2;
        hh = U_(rng, 1.1, 1.7); tw2 = U_(rng, 1.0, 2.0); a2 = U_(rng, 0, 3);
        spike(sol, x, y, rr, hh, rr * 0.5, t, tw2, a2, 3, 0.62); gl_add(ft, disc(x, y, rr, 20));
    }
    *foot = buf(hull(ft), 0.2, 16); ns_st(o, sol, "thorn", 0, 0, NULL); return 0;
}
BODY(sd_thorn_ring) {
    double R = U_(rng, 1.9, 2.4); int n = (int)RI(rng, 5, 7), i; SoSolids *sol = sl_new(); GL *ft = gl_new();
    for (i = 0; i < n; i++) {
        double t = a + 6.2832 * i / n + U_(rng, -0.2, 0.2), x = cx + R * ns_cos(t), y = cy + R * ns_sin(t), rr = U_(rng, 0.55, 0.8), h, tw, a0; int p;
        h = U_(rng, 1.3, 2.3); tw = U_(rng, 0.6, 1.4); a0 = U_(rng, 0, 3); p = (int)CH(rng, 3, 4);
        spike(sol, x, y, rr, h, rr * 0.55, t + PI, tw, a0, p, 0.62); gl_add(ft, disc(x, y, rr, 20));
    }
    *foot = buf(hull(ft), 0.2, 16); ns_st(o, sol, "thorn", 0, 0, NULL); return 0;
}
BODY(sd_leaning_thorns) {
    FF f = ff_(cx, cy, a); int n = (int)RI(rng, 3, 5), i; SoSolids *sol = sl_new(); GL *ft = gl_new(); double lean = a + PI / 2 + U_(rng, -0.3, 0.3);
    for (i = 0; i < n; i++) {
        double v = U_(rng, -0.3, 0.3), u = (i - (n - 1) / 2.0) * 1.5, x = fx(&f, u, v), y = fy(&f, u, v), rr = U_(rng, 0.55, 0.8), h, tw, a0; int p;
        h = U_(rng, 1.4, 2.4); tw = U_(rng, 0.4, 1.0); a0 = U_(rng, 0, 3); p = (int)CH(rng, 3, 4);
        spike(sol, x, y, rr, h, rr * 0.6, lean, tw, a0, p, 0.62); gl_add(ft, disc(x, y, rr, 20));
    }
    *foot = buf(hull(ft), 0.2, 16); ns_st(o, sol, "thorn", 0, 0, NULL); return 0;
}
typedef struct { double L; } Shard;          /* lambda q, L=L: 0.5 * (0.5 - q / L) + 0.12 */
static int shard(void *c, double q, double *out) { const Shard *s = (const Shard *)c; if (s->L == 0.0) return -1; *out = 0.5 * (0.5 - q / s->L) + 0.12; return 0; }
BODY(sd_thorn_shards) {
    double r = U_(rng, 0.8, 1.0), h, tw, a0; int i, n; SoSolids *sol = sl_new(); GL *ft = gl_new();
    h = U_(rng, 1.0, 1.3); tw = U_(rng, 0.6, 1.2); a0 = U_(rng, 0, 3);
    spike(sol, cx, cy, r, h, 0.0, 0.0, tw, a0, 4, 0.66); gl_add(ft, disc(cx, cy, r, 20));
    n = (int)RI(rng, 2, 3);
    for (i = 0; i < n; i++) {
        double t = a + i * 2.0 + U_(rng, -0.3, 0.3), d = r + U_(rng, 1.3, 1.9), x = cx + d * ns_cos(t), y = cy + d * ns_sin(t), L = U_(rng, 1.8, 2.6), b = t + U_(rng, -1, 1); Shard s = { L };
        if (sd_spin(sol, x, y, b, L, shard, &s, 1.1, 0.0, "ridge") != SO_OK) ns_raise();
        gl_add(ft, rect(x, y, L, 1.0, b));
    }
    *foot = buf(hull(ft), 0.2, 16); ns_st(o, sol, "thorn", 0, 0, NULL); return 0;
}
BODY(sd_impaled_skull) {
    FF f = ff_(cx, cy, a); double r = U_(rng, 0.9, 1.1), h, la, tw, a0; int i, n; SoSolids *sol = sl_new(), *sk = sl_new(), *bones = sl_new(); NsCuts cu = { 0 };
    h = U_(rng, 2.8, 3.2); la = U_(rng, 0, 6.28); tw = U_(rng, 0.6, 1.2); a0 = U_(rng, 0, 3);
    spike(sol, cx, cy, r, h, r * 0.4, la, tw, a0, 4, 0.62);
    bl_skull(sk, &cu, FX(f, r + 0.55, 0), 0.75, a, 0.0);
    n = (int)RI(rng, 2, 3);
    for (i = 0; i < n; i++) {
        double t = a + U_(rng, 1.2, 5.0), px = cx + (r + 0.4) * ns_cos(t), py = cy + (r + 0.4) * ns_sin(t), d = r + U_(rng, 2.0, 2.8), t2 = t + U_(rng, -0.3, 0.3);
        bl_bone(bones, px, py, cx + d * ns_cos(t2), cy + d * ns_sin(t2), 0.22);
    }
    *foot = disc(cx, cy, r + 2.6, 20);
    ns_st(o, sol, "thorn", 0, 0, NULL); ns_st(o, sk, "bone", 0, 0, &cu); ns_st(o, bones, "bone", 0, 0, NULL); return 0;
}
BODY(sd_bone_pile) {
    double R = U_(rng, 1.5, 1.9), sq, prof[8]; int i, n; SoSolids *sol = sl_new(), *sk = sl_new(); GL *ft = gl_new(); NsCuts cu = { 0 };
    prof[0] = R; prof[1] = 0.0; prof[2] = R * 0.8; prof[3] = 0.3; prof[4] = R * 0.45; prof[5] = 0.55; prof[6] = 0.3; prof[7] = 0.62;
    sq = U_(rng, 0.7, 0.9);
    if (sd_turn(sol, cx, cy, prof, 4, 14, sq, a, 0.0) != SO_OK) ns_raise();
    gl_add(ft, disc(cx, cy, R, 20));
    n = (int)RI(rng, 3, 5);
    for (i = 0; i < n; i++) {
        double t = U_(rng, 0, 3.14), L = U_(rng, 2.6, 3.4), ox = cx + U_(rng, -0.6, 0.6), oy = cy + U_(rng, -0.6, 0.6), rr; FF f = ff_(ox, oy, t);
        rr = U_(rng, 0.22, 0.28);
        bl_bone(sol, FX(f, -L / 2, 0), FX(f, L / 2, 0), rr); gl_add(ft, disc(FX(f, L / 2, 0), 0.4, 20)); gl_add(ft, disc(FX(f, -L / 2, 0), 0.4, 20));
    }
    { double x = cx + U_(rng, -0.4, 0.4), y = cy + U_(rng, -0.4, 0.4), aa = a + U_(rng, -1, 1); bl_skull(sk, &cu, x, y, 0.7, aa, 0.35); }
    sl_add(sol, sk);
    *foot = buf(hull(ft), 0.2, 16); ns_st(o, sol, "bone", 0, 0, &cu); return 0;
}
BODY(sd_skull_heap) {
    SoSolids *sol = sl_new(); NsCuts cu = { 0 }; GL *ft = gl_new(); int i, n = (int)RI(rng, 3, 5);
    for (i = 0; i < n; i++) {
        double x = cx, y = cy, r, aa; int j, bad = 0; G *pt;
        if (i) { double d = U_(rng, 1.25, 1.5), t = a + i * 1.3 + U_(rng, -0.2, 0.2); x = cx + d * ns_cos(t); y = cy + d * ns_sin(t); }
        r = i ? U_(rng, 0.65, 0.8) : 0.8;
        if (ft->n) { pt = P(x, y); for (j = 0; j < ft->n && !bad; j++) bad = dist(pt, cen(ft->v[j])) < r * 1.55; }
        if (bad) continue;
        aa = U_(rng, 0, 6.28); bl_skull(sol, &cu, x, y, r, aa, 0.0); gl_add(ft, disc(x, y, r * 1.3, 20));
    }
    *foot = buf(hull(ft), 0.2, 16); ns_st(o, sol, "bone", 0, 0, &cu); return 0;
}
BODY(sd_ribcage) {
    FF f = ff_(cx, cy, a); double L = U_(rng, 4.2, 5.0); int n = (int)RI(rng, 5, 6), i; SoSolids *sol = sl_new(); GL *ft = gl_new();
    { double pts[4] = { FX(f, -L / 2, 0), FX(f, L / 2, 0) }; rod2(sol, pts, 2, NULL, NULL, 0.3, 1.1, 0.0); }
    gl_add(ft, rect(cx, cy, L, 0.8, a));
    for (i = 0; i < n; i++) {
        double x = -L / 2 + 0.5 + i * (L - 1.0) / (n - 1), ln = U_(rng, 1.8, 2.4) * (1 - 0.25 * fabs(i - n / 2.0) / n), pts[6]; Lin lf = { 0.22, 0.06 };
        ln *= RR(rng) < 0.2 ? 0.55 : 1.0;
        pts[0] = fx(&f, x, 0.2); pts[1] = fy(&f, x, 0.2); pts[2] = fx(&f, x + 0.3, ln * 0.5); pts[3] = fy(&f, x + 0.3, ln * 0.5); pts[4] = fx(&f, x + 0.1, ln); pts[5] = fy(&f, x + 0.1, ln);
        rod2(sol, pts, 3, lin, &lf, 0.0, 1.1, 0.0); gl_add(ft, disc(pts[4], pts[5], 0.3, 20));
    }
    *foot = buf(hull(ft), 0.2, 16); ns_st(o, sol, "bone", 0, 0, NULL); return 0;
}
BODY(sd_horned_skull) {
    FF f = ff_(cx, cy, a); SoSolids *sk = sl_new(); NsCuts cu = { 0 }; GL *ft = gl_new(); int sv;
    bl_skull(sk, &cu, cx, cy, 1.0, a, 0.0);
    for (sv = -1; sv <= 1; sv += 2) {
        double pts[8], v2, v3; Lin lf = { 0.42, 0.26 };
        v2 = sv * U_(rng, 1.4, 1.8); v3 = sv * U_(rng, 0.6, 1.0);
        pts[0] = fx(&f, -0.3, sv * 0.6); pts[1] = fy(&f, -0.3, sv * 0.6); pts[2] = fx(&f, -1.1, sv * 1.4); pts[3] = fy(&f, -1.1, sv * 1.4);
        pts[4] = fx(&f, -2.2, v2); pts[5] = fy(&f, -2.2, v2); pts[6] = fx(&f, -2.7, v3); pts[7] = fy(&f, -2.7, v3);
        rod2(sk, pts, 4, lin, &lf, 0.0, 1.0, 0.0);
    }
    gl_add(ft, disc(cx, cy, 1.2, 20)); gl_add(ft, disc(FX(f, -2.5, 1.5), 0.6, 20)); gl_add(ft, disc(FX(f, -2.5, -1.5), 0.6, 20));
    *foot = buf(hull(ft), 0.2, 16); ns_st(o, sk, "bone", 0, 0, &cu); return 0;
}
BODY(sd_bubbling_pool) {
    double R = U_(rng, 1.7, 2.2), b[4]; G *pool = blob(cx, cy, R, rng, 4); GL *bub = gl_new(), *ft = gl_new(); int i, n;
    { double d = U_(rng, 0.55, 0.7); ns_gcut(o, pool, d, "ichor"); }
    bounds(pool, b);
    for (i = 0; i < 80; i++) {
        double x, y, r; G *bb; int ok, j;
        if (bub->n >= RI(rng, 2, 4)) break;
        x = U_(rng, b[0], b[2]); y = U_(rng, b[1], b[3]); r = U_(rng, 0.5, 0.72); bb = disc(x, y, r, 14);
        ok = contains(inset(pool, 0.3), bb);
        for (j = 0; j < bub->n && ok; j++) ok = dist(bb, bub->v[j]) > 0.35;
        if (ok) gl_add(bub, bb);
    }
    for (i = 0; i < bub->n; i++) ns_pad(o, bub->v[i], "blister");
    gl_add(ft, pool);
    n = (int)RI(rng, 3, 5);
    for (i = 0; i < n; i++) {
        double t = a + i * 1.4 + U_(rng, -0.3, 0.3), d = R + U_(rng, 0.5, 0.8), r = U_(rng, 0.45, 0.6); G *q = disc(cx + d * ns_cos(t), cy + d * ns_sin(t), r, 12); int ok, j;
        ok = dist(q, pool) > 0.2;
        for (j = 1; j < ft->n && ok; j++) ok = dist(q, ft->v[j]) > 0.2;
        if (ok) { double h; gl_add(ft, q); h = U_(rng, 0.4, 0.6); ns_mound(o, q, h, "blister", &NS_SOFT_T); }
    }
    *foot = buf(hull(ft), 0.2, 16); return 0;
}
BODY(sd_ichor_seep) {
    FF f = ff_(cx, cy, a); double L = U_(rng, 5, 7), v, d; G *cr, *pool; GL *l = gl_new();
    v = U_(rng, -0.5, 0.5);
    cr = wavy(FX(f, -L / 2, 0), FX(f, L / 2 - 1.0, v), rng, 0.5, 5, 0.45);
    { double r = U_(rng, 1.0, 1.3); pool = blob(FX(f, L / 2, 0), r, rng, 3); }
    gl_add(l, cr); gl_add(l, pool);
    *foot = buf(hull(l), 0.2, 16);
    { G *uu_ = U(l); d = U_(rng, 0.5, 0.6); ns_gcut(o, uu_, d, "ichor"); }
    ns_pad(o, disc(FX(f, L / 2, 0), 0.5, 12), "blister");
    return 0;
}
BODY(sd_blisters) {
    SoSolids *ps = sl_new(); NsCuts cu = { 0 }; GL *ft = gl_new(); int i, n = (int)RI(rng, 4, 7);
    for (i = 0; i < n; i++) {
        double x = cx, y = cy, r, h; int j, bad = 0;
        if (i) { double d = U_(rng, 0.9, 1.9), t = a + i * 1.1; x = cx + d * ns_cos(t); y = cy + d * ns_sin(t); }
        r = U_(rng, 0.45, 0.8);
        if (ft->n) { G *dd = disc(x, y, r, 20); for (j = 0; j < ft->n && !bad; j++) bad = dist(dd, ft->v[j]) < 0.1; }
        if (bad) continue;
        h = r * U_(rng, 0.8, 1.1);
        if (sd_dome(ps, x, y, r, h, 0.0, 1.0, 0.0, 14) != SO_OK) ns_raise();
        gl_add(ft, disc(x, y, r, 20));
        if (r > 0.6 && RR(rng) < 0.5) cut_add(&cu, disc(x, y, r * 0.42, 10), h * 0.5);
    }
    *foot = buf(hull(ft), 0.2, 16); ns_st(o, ps, "blister", 0, 0, &cu); return 0;
}
BODY(sd_rune_stone) {
    FF f = ff_(cx, cy, a); double L = U_(rng, 2.4, 2.9), W = U_(rng, 2.0, 2.4), xy[8], u1, u2, u3, u4, h, ra, s; SoObj *ob = obj_new("runestone"); G *g; int b; int64_t k;
    u1 = U_(rng, 0, 0.4); xy[0] = fx(&f, -L / 2 + u1, -W / 2); xy[1] = fy(&f, -L / 2 + u1, -W / 2);
    u2 = U_(rng, 0, 0.4); xy[2] = fx(&f, L / 2, -W / 2 + u2); xy[3] = fy(&f, L / 2, -W / 2 + u2);
    u3 = U_(rng, 0, 0.4); xy[4] = fx(&f, L / 2 - u3, W / 2); xy[5] = fy(&f, L / 2 - u3, W / 2);
    u4 = U_(rng, 0, 0.4); xy[6] = fx(&f, -L / 2, W / 2 - u4); xy[7] = fy(&f, -L / 2, W / 2 - u4);
    g = polyv(xy, 4);
    h = U_(rng, 1.1, 1.5); b = so_obj_up(ob, g, h, NULL); if (b < 0) ns_raise();
    ra = a + U_(rng, -0.3, 0.3); k = RB(rng, 8); s = (L < W ? L : W) * 0.62;
    { FF f2 = ff_(cx, cy, ra); if (so_obj_cut(ob, bl_runes(&f2, k, s, 0.3), 1, b, 0.3) < 0) ns_raise(); }
    { double v1 = U_(rng, -0.8, 0.8), v2; double px = fx(&f, -L / 2 - 0.3, v1), py = fy(&f, -L / 2 - 0.3, v1); v2 = U_(rng, -0.8, 0.8);
      if (so_obj_cut(ob, wavy(px, py, FX(f, L / 2 + 0.3, v2), rng, 0.3, 4, 0.22), 1, b, 0.4) < 0) ns_raise(); }
    ns_obj_out(o, ob, NULL, foot); return 0;
}
BODY(sd_cracked_rune_ring) {
    double R = U_(rng, 2.2, 2.7), t, t2; int n = (int)RI(rng, 5, 6), i; GL *g = gl_new(); G *cr;
    gl_add(g, ann(cx, cy, R, R - 0.34, 36));
    for (i = 0; i < n; i++) {
        double t_ = a + 6.2832 * i / n; FF f = ff_(cx + R * 0.6 * ns_cos(t_), cy + R * 0.6 * ns_sin(t_), a + 6.2832 * i / n + PI / 2);
        gl_add(g, bl_runes(&f, RB(rng, 8), 1.0, 0.28));
    }
    t = U_(rng, 0, 6.28); t2 = t + PI + U_(rng, -0.4, 0.4);
    cr = wavy(POL(cx, cy, R + 0.6, t), POL(cx, cy, R + 0.6, t2), rng, 0.5, 5, 0.42);
    *foot = disc(cx, cy, R + 0.7, 20);
    ns_gcut(o, dif(U(g), buf(cr, 0.3, 16)), 0.35, "runecut"); ns_gcut(o, cr, 0.5, "rift"); return 0;
}
BODY(sd_rune_slab) {
    FF f = ff_(cx, cy, a); double L = U_(rng, 4.2, 5.0), W = U_(rng, 2.4, 2.8), gap, br; SoObj *ob = obj_new("runestone"); int sx;
    gap = U_(rng, 0.35, 0.5); br = U_(rng, -0.4, 0.4);
    for (sx = -1; sx <= 1; sx += 2) {
        double xy[8], u1, u2, h, h2; G *g, *top; int b; GL *rl = gl_new(); FF f2;
        xy[0] = fx(&f, sx * L / 2, -W / 2); xy[1] = fy(&f, sx * L / 2, -W / 2);
        u1 = br + sx * gap / 2 + U_(rng, -0.2, 0.2); xy[2] = fx(&f, u1, -W / 2); xy[3] = fy(&f, u1, -W / 2);
        u2 = br + sx * gap / 2 + U_(rng, -0.3, 0.3); xy[4] = fx(&f, u2, W / 2); xy[5] = fy(&f, u2, W / 2);
        xy[6] = fx(&f, sx * L / 2, W / 2); xy[7] = fy(&f, sx * L / 2, W / 2);
        g = buf(polyv(xy, 4), 0, 16);
        h = U_(rng, 0.5, 0.65); b = so_obj_up(ob, g, h, NULL); if (b < 0) ns_raise();
        top = inter(inset(g, 0.9), rect(FX(f, br + sx * (gap / 2 + 0.8), 0), 1.6, W, a));
        h2 = U_(rng, 0.2, 0.3); if (so_obj_on(ob, top, 1, b, h2, NULL) < 0) ns_raise();
        f2 = ff_(FX(f, sx * L * 0.27, 0), a);
        { double vs[2] = { -W * 0.24, W * 0.24 }; int j; for (j = 0; j < 2; j++) { FF f3 = ff_(FX(f2, 0, vs[j]), a + PI / 2); gl_add(rl, bl_runes(&f3, RB(rng, 8), 0.9, 0.26)); } }
        if (so_obj_cut(ob, inter(U(rl), inset(g, 0.3)), 1, b, 0.25) < 0) ns_raise();
    }
    ns_obj_out(o, ob, NULL, foot); return 0;
}
BODY(sd_blood_stone) {
    double R = U_(rng, 1.6, 1.9), h; SoObj *ob = obj_new("runestone"); G *g; int b, n, i, nv; GL *l = gl_new();
    nv = (int)CH(rng, 5, 6, 7); g = ngon(cx, cy, R, nv, a);
    h = U_(rng, 0.9, 1.2); b = so_obj_up(ob, g, h, NULL); if (b < 0) ns_raise();
    n = (int)RI(rng, 3, 4);
    gl_add(l, disc(cx, cy, 0.55, 14));
    for (i = 0; i < n; i++) gl_add(l, barl(cx, cy, POL(cx, cy, R + 0.3, a + 6.2832 * i / n + 0.3), 0.3));
    if (so_obj_cut(ob, U(l), 1, b, 0.35) < 0) ns_raise();
    ns_obj_out(o, ob, NULL, foot); return 0;
}
static double tear_wf(double W, double q, double L) { return W * ns_pow(fmax(0.0, 1 - ns_pow(2 * q / L, 2.0)), 0.7); }
BODY(sd_rift_tear) {
    FF f = ff_(cx, cy, a); double L = U_(rng, 6.5, 8.0), W = U_(rng, 1.0, 1.4), us[13]; int m = 12, i, n; PTS *pl = pl_new(); G *slot; GL *ft = gl_new();
    for (i = 0; i <= m; i++) us[i] = -L / 2 + L * i / m;
    for (i = 0; i <= m; i++) { double v = tear_wf(W, us[i], L) * U_(rng, 0.6, 1.0); pl_add(pl, FX(f, us[i], v)); }
    for (i = m; i >= 0; i--) { double v = -tear_wf(W, us[i], L) * U_(rng, 0.6, 1.0); pl_add(pl, FX(f, us[i], v)); }
    slot = buf(poly(pl), 0, 16);
    { double d = U_(rng, 0.8, 0.95); ns_gcut(o, slot, d, "rift"); }
    gl_add(ft, slot);
    n = (int)RI(rng, 4, 6);
    for (i = 0; i < n; i++) {
        double q = U_(rng, -L * 0.38, L * 0.38), sv = CH(rng, -1, 1), v, r; int nv, ok, j; G *p;
        v = sv * (tear_wf(W, q, L) + U_(rng, 0.8, 1.1)); r = U_(rng, 0.55, 0.8); nv = (int)RI(rng, 4, 5);
        p = stone(FX(f, q, v), r, rng, 0.7, nv);
        ok = dist(p, slot) > 0.22;
        for (j = 1; j < ft->n && ok; j++) ok = dist(p, ft->v[j]) > 0.2;
        if (ok) { double h; gl_add(ft, p); h = U_(rng, 0.5, 0.9); ns_mound(o, p, h, "blightrock", &NS_ROCK_T); }
    }
    *foot = buf(hull(ft), 0.2, 16); return 0;
}
BODY(sd_rift_mouth) {
    double R = U_(rng, 1.0, 1.3); G *mouth = stone(cx, cy, R, rng, 0.85, 7), *sp; int n = (int)RI(rng, 4, 6), i; GL *l = gl_new(), *ft = gl_new();
    for (i = 0; i < n; i++) {
        double t1 = a + 6.2832 * i / n + U_(rng, -0.3, 0.3), d2 = R + U_(rng, 1.6, 2.6), t2 = a + 6.2832 * i / n + U_(rng, -0.3, 0.3);
        gl_add(l, wavy(POL(cx, cy, R * 0.6, t1), POL(cx, cy, d2, t2), rng, 0.3, 3, 0.38));
    }
    sp = U(l);
    { double d = U_(rng, 0.85, 1.0); ns_gcut(o, mouth, d, "rift"); }
    ns_gcut(o, dif(sp, mouth), 0.5, "rift");
    gl_add(ft, mouth); gl_add(ft, sp);
    n = (int)RI(rng, 2, 4);
    for (i = 0; i < n; i++) {
        double d = R + U_(rng, 1.0, 2.0), t = U_(rng, 0, 6.28), r = U_(rng, 0.4, 0.55); G *p = stone(POL(cx, cy, d, t), r, rng, 0.7, 4); int ok, j;
        ok = dist(p, sp) > 0.2;
        for (j = 2; j < ft->n && ok; j++) ok = dist(p, ft->v[j]) > 0.2;
        if (ok) { double h; gl_add(ft, p); h = U_(rng, 0.4, 0.55); ns_mound(o, p, h, "blightrock", &NS_PEB_T); }
    }
    *foot = buf(hull(ft), 0.2, 16); return 0;
}
BODY(sd_flesh_lump) {
    SoSolids *ps = sl_new(); GL *ft = gl_new(); int i, n = (int)RI(rng, 3, 4);
    for (i = 0; i < n; i++) {
        double x = cx, y = cy, r, h, sq, aa;
        if (i) { double d = U_(rng, 1.0, 1.5), t = a + i * 2.0 + U_(rng, -0.4, 0.4); x = cx + d * ns_cos(t); y = cy + d * ns_sin(t); }
        r = U_(rng, 1.0, 1.3) * (i == 0 ? 1 : 0.8);
        h = U_(rng, 1.0, 1.5) * (i == 0 ? 1 : 0.75); sq = U_(rng, 0.75, 0.95); aa = U_(rng, 0, 3);
        if (sd_dome(ps, x, y, r, h, 0.0, sq, aa, 20) != SO_OK) ns_raise();
        gl_add(ft, disc(x, y, r, 20));
    }
    *foot = buf(hull(ft), 0.2, 16); ns_st(o, ps, "fleshrock", 0, 0, NULL); return 0;
}
BODY(sd_veined_rock) {
    double r = U_(rng, 1.4, 1.8), h = U_(rng, 1.0, 1.3), sq; SoSolids *ps = sl_new(), *vs = sl_new(); GL *ft = gl_new(); int i, n;
    sq = U_(rng, 0.75, 0.9); if (sd_dome(ps, cx, cy, r, h, 0.0, sq, a, 20) != SO_OK) ns_raise();
    gl_add(ft, disc(cx, cy, r, 20));
    n = (int)RI(rng, 3, 4);
    for (i = 0; i < n; i++) {
        double t = a + i * 1.6 + U_(rng, -0.3, 0.3), pts[6], t2, d3, t3; Lin lf = { 0.26, 0.08 };
        pts[0] = cx + r * 0.3 * ns_cos(t); pts[1] = cy + r * 0.3 * ns_sin(t);
        t2 = t + U_(rng, -0.3, 0.3); pts[2] = cx + r * 0.9 * ns_cos(t2); pts[3] = cy + r * 0.9 * ns_sin(t2);
        d3 = r + U_(rng, 1.2, 2.0); t3 = t + U_(rng, -0.4, 0.4); pts[4] = cx + d3 * ns_cos(t3); pts[5] = cy + d3 * ns_sin(t3);
        rod2(vs, pts, 3, lin, &lf, 0.0, 1.0, 0.0); gl_add(ft, disc(pts[4], pts[5], 0.3, 20));
    }
    *foot = buf(hull(ft), 0.2, 16); ns_st(o, ps, "fleshrock", 0, 0, NULL); ns_st(o, vs, "tendril", 0, 0, NULL); return 0;
}
BODY(sd_eye_stone) {
    FF f = ff_(cx, cy, a); double r = U_(rng, 1.6, 1.9), h = U_(rng, 1.1, 1.4), ex, ey; SoSolids *lump = sl_new(), *eye = sl_new(); G *lids; NsCuts c1 = { 0 }, c2 = { 0 };
    if (sd_dome(lump, cx, cy, r, h, 0.0, 0.8, a, 20) != SO_OK) ns_raise();
    ex = fx(&f, r * 0.15, 0); ey = fy(&f, r * 0.15, 0); lids = ell(ex, ey, 1.0, 0.62, a, 18);
    if (sd_dome(eye, ex, ey, 0.58, 0.45, h * 0.62, 0.8, a, 20) != SO_OK) ns_raise();
    *foot = disc(cx, cy, r, 20);
    cut_add(&c1, lids, h * 0.62); ns_st(o, lump, "fleshrock", 0, 0, &c1);
    cut_add(&c2, rect(ex, ey, 0.9, 0.22, a + PI / 2), h * 0.62 + 0.25); ns_st(o, eye, "blister", 0, 0, &c2);
    return 0;
}
BODY(sd_tendril_hole) {
    double R = U_(rng, 0.8, 1.0); G *hole = stone(cx, cy, R, rng, 0.85, 8); SoSolids *vs = sl_new(); GL *ft = gl_new(); int i, n;
    gl_add(ft, hole);
    n = (int)RI(rng, 3, 4);
    for (i = 0; i < n; i++) {
        double t = a + i * 6.2832 / 3.5 + U_(rng, -0.3, 0.3), L = U_(rng, 2.6, 3.6), b = t + U_(rng, -0.7, 0.7), pts[6], p0x, p0y; Lin lf = { 0.42, 0.22 };
        pts[0] = cx + R * 0.9 * ns_cos(t); pts[1] = cy + R * 0.9 * ns_sin(t);
        p0x = cx + R * 0.9 * ns_cos(t); p0y = cy + R * 0.9 * ns_sin(t);
        pts[2] = polx(p0x, L * 0.5, t + (b - t) * 0.5); pts[3] = poly_(p0y, L * 0.5, t + (b - t) * 0.5);
        pts[4] = polx(p0x, L, b); pts[5] = poly_(p0y, L, b);
        rod2(vs, pts, 3, lin, &lf, 0.0, 1.0, 0.0); gl_add(ft, disc(pts[4], pts[5], 0.3, 20));
    }
    *foot = buf(hull(ft), 0.2, 16);
    { double d = U_(rng, 0.8, 0.95); ns_gcut(o, hole, d, "rift"); }
    ns_st(o, vs, "tendril", 0, 0, NULL); return 0;
}
BODY(sd_ritual_candles) {
    int n = (int)RI(rng, 4, 6), i; double R = U_(rng, 1.6, 2.0); SoSolids *sol = sl_new(), *sk = sl_new(); GL *ft = gl_new(); NsCuts cu = { 0 };
    for (i = 0; i < n; i++) {
        double t = a + 6.2832 * i / n, x = cx + R * ns_cos(t), y = cy + R * ns_sin(t), r = U_(rng, 0.32, 0.42), h;
        if (sd_cyl(sol, x, y, r + 0.18, 0.18, 0.0, 12) != SO_OK) ns_raise();
        h = U_(rng, 0.7, 1.4); if (sd_cyl(sol, x, y, r, h, 0.0, 12) != SO_OK) ns_raise();
        gl_add(ft, disc(x, y, r + 0.18, 20));
    }
    { double aa = a + U_(rng, -1, 1); bl_skull(sk, &cu, cx, cy, 0.78, aa, 0.0); }
    *foot = buf(hull(ft), 0.2, 16); ns_st(o, sol, "wax", 0, 0, NULL); ns_st(o, sk, "bone", 0, 0, &cu); return 0;
}
BODY(sd_offering_bowl) {
    double r = U_(rng, 1.1, 1.3), h = U_(rng, 0.9, 1.1), prof[20]; SoSolids *sol = sl_new(); NsCuts cu = { 0 }; int i;
    prof[0] = r * 0.6; prof[1] = 0.0; prof[2] = r * 0.7; prof[3] = h * 0.2;
    so_curve(r * 0.7, r, h * 0.2, h * 0.75, 6, 1, prof + 4);
    prof[18] = r; prof[19] = h;
    if (sd_turn(sol, cx, cy, prof, 10, 20, 1.0, 0.0, 0.0) != SO_OK) ns_raise();
    for (i = 0; i < 3; i++) if (sd_cyl(sol, POL(cx, cy, r * 0.62, a + i * 2.094), 0.3, 0.25, 0.0, 8) != SO_OK) ns_raise();
    *foot = disc(cx, cy, r + 0.1, 20);
    cut_add(&cu, disc(cx, cy, r - 0.28, 20), h * 0.55); ns_st(o, sol, "iron", 0, 0, &cu); return 0;
}
BODY(sd_ritual_knife) {
    FF f = ff_(cx, cy, a); double Lb = U_(rng, 2.6, 3.2), xs[7], ar[8]; SoObj *ob = obj_new("iron"); PTS *pl = pl_new(); G *blade, *gd, *hg, *pm; int i, hd, na; GL *l = gl_new();
    pl_add(pl, FX(f, -0.2, 0.55));
    ns_linspace(0, Lb, 7, xs);
    for (i = 0; i < 7; i++) { double x = xs[i]; pl_add(pl, FX(f, x, 0.5 * (1 - x / Lb) + 0.12 + 0.1 * ns_sin(x * 4))); }
    ns_linspace(Lb, 0, 7, xs);
    for (i = 0; i < 7; i++) { double x = xs[i]; pl_add(pl, FX(f, x, -0.5 * (1 - x / Lb) - 0.12 + 0.1 * ns_sin(x * 4))); }
    pl_add(pl, FX(f, -0.2, -0.55));
    blade = buf(poly(pl), 0, 16);
    { SoSolids *s = sl_new(); if (sd_ext(s, blade, 0.32, 0.0) != SO_OK || so_obj_st(ob, s, NULL, 0, 0, NULL, 0, blade) < 0) ns_raise(); }
    { double aa = a + U_(rng, -0.2, 0.2); gd = rect(FX(f, -0.4, 0), 0.5, 1.7, aa); }
    { SoSolids *s = sl_new(); if (sd_ext(s, gd, 0.45, 0.0) != SO_OK || so_obj_st(ob, s, NULL, 0, 0, NULL, 0, gd) < 0) ns_raise(); }
    hg = rect(FX(f, -1.2, 0), 1.2, 0.62, a);
    { SoSolids *s = sl_new(); if (sd_ext(s, hg, 0.48, 0.0) != SO_OK || (hd = so_obj_st(ob, s, "leather", 0, 0, NULL, 0, hg)) < 0) ns_raise(); }
    na = ns_arange(-1.7, -0.7, 0.25, ar, 8);
    for (i = 0; i < na; i++) gl_add(l, barl(FX(f, ar[i], -0.45), FX(f, ar[i] + 0.12, 0.45), 0.12));
    if (so_obj_cut(ob, U(l), 1, hd, 0.12) < 0) ns_raise();
    pm = disc(FX(f, -1.95, 0), 0.42, 10);
    { SoSolids *s = sl_new(); if (sd_ext(s, pm, 0.52, 0.0) != SO_OK || so_obj_st(ob, s, NULL, 0, 0, NULL, 0, pm) < 0) ns_raise(); }
    ns_obj_out(o, ob, NULL, foot); return 0;
}

/* the register, in the file's order: d(style, cls, kind, name, r=10.5, ground=False) */
#define SD(fn, cls, kind, name) static const NsSd D_##fn = { fn, "blight", cls, kind, name, 0 };
SD(sd_twisted_spike, "debris", "thorn", "twisted_spike") SD(sd_thorn_ring, "debris", "thorn", "thorn_ring") SD(sd_leaning_thorns, "debris", "thorn", "leaning_thorns")
SD(sd_thorn_shards, "debris", "thorn", "thorn_shards") SD(sd_impaled_skull, "made", "thorn", "impaled_skull") SD(sd_bone_pile, "debris", "bone", "bone_pile")
SD(sd_skull_heap, "debris", "bone", "skull_heap") SD(sd_ribcage, "debris", "bone", "ribcage") SD(sd_horned_skull, "debris", "bone", "horned_skull")
SD(sd_bubbling_pool, "marks", "ichor", "bubbling_pool") SD(sd_ichor_seep, "marks", "ichor", "ichor_seep") SD(sd_blisters, "growth", "blister", "blisters")
SD(sd_rune_stone, "made", "runestone", "rune_stone") SD(sd_cracked_rune_ring, "marks", "runecut", "cracked_rune_ring") SD(sd_rune_slab, "made", "runestone", "rune_slab")
SD(sd_blood_stone, "made", "runestone", "blood_stone") SD(sd_rift_tear, "marks", "rift", "rift_tear") SD(sd_rift_mouth, "marks", "rift", "rift_mouth")
SD(sd_flesh_lump, "growth", "fleshrock", "flesh_lump") SD(sd_veined_rock, "growth", "fleshrock", "veined_rock") SD(sd_eye_stone, "growth", "fleshrock", "eye_stone")
SD(sd_tendril_hole, "growth", "tendril", "tendril_hole") SD(sd_ritual_candles, "made", "wax", "ritual_candles") SD(sd_offering_bowl, "made", "iron", "offering_bowl")
SD(sd_ritual_knife, "made", "iron", "ritual_knife")
void ns_reg_blight(void) {
    ns_sd_reg(&D_sd_twisted_spike, 10.5, 0); ns_sd_reg(&D_sd_thorn_ring, 10.5, 0); ns_sd_reg(&D_sd_leaning_thorns, 10.5, 0); ns_sd_reg(&D_sd_thorn_shards, 10.5, 0);
    ns_sd_reg(&D_sd_impaled_skull, 10.5, 0); ns_sd_reg(&D_sd_bone_pile, 10.5, 0); ns_sd_reg(&D_sd_skull_heap, 10.5, 0); ns_sd_reg(&D_sd_ribcage, 10.5, 0);
    ns_sd_reg(&D_sd_horned_skull, 10.5, 0); ns_sd_reg(&D_sd_bubbling_pool, 10.5, 1); ns_sd_reg(&D_sd_ichor_seep, 10.5, 1); ns_sd_reg(&D_sd_blisters, 10.5, 0);
    ns_sd_reg(&D_sd_rune_stone, 10.5, 0); ns_sd_reg(&D_sd_cracked_rune_ring, 10.5, 1); ns_sd_reg(&D_sd_rune_slab, 10.5, 0); ns_sd_reg(&D_sd_blood_stone, 10.5, 0);
    ns_sd_reg(&D_sd_rift_tear, 10.5, 1); ns_sd_reg(&D_sd_rift_mouth, 10.5, 1); ns_sd_reg(&D_sd_flesh_lump, 10.5, 0); ns_sd_reg(&D_sd_veined_rock, 10.5, 0);
    ns_sd_reg(&D_sd_eye_stone, 10.5, 0); ns_sd_reg(&D_sd_tendril_hole, 10.5, 1); ns_sd_reg(&D_sd_ritual_candles, 10.5, 0); ns_sd_reg(&D_sd_offering_bowl, 10.5, 0);
    ns_sd_reg(&D_sd_ritual_knife, 10.5, 0);
}
/* style_blight.rune, for bld_ns_tiers.c (f_runes, pat_brune) */
G *ns_bl_rune(const FF *f2, int64_t k, double s, double w) { return rune(f2, k, s, w); }
