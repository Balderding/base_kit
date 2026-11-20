/* bld_ns_sundered.c: the 28 style-only details of sd_r_x_sundered.py and its shared drawing (su_lancet, su_jag,
   su_chip, su_drum, su_bars, su_bent), copied routine by routine (each names the Python it copies). See bld_ns.h.
   (style_sundered.py's own tier features are in bld_ns_tiers.c.) */
#include "bld_ns.h"

#define BODY(nm) static int nm(double cx, double cy, double a, PyRandom *rng, NsOut *o, G **foot)
#define OUT_ ns_obj_out(o, ob, NULL, foot); return 0
#define OUTL_(f) do { *foot = (f); { int i_; for (i_ = 0; i_ < ob->L.n; i_++) lyl_push(o->lay, ob->L.v[i_]); } return 0; } while (0)
static const char *ST_ = "ashlar", *SU_ = "statue";

/* su_lancet(ff, W, S, base=0.0) */
static G *su_lancet(const FF *f, double W, double S, double base) {
    PTS *p = pl_new(); double t[9]; int i;
    pl_add(p, fx(f, base, -W / 2), fy(f, base, -W / 2)); pl_add(p, fx(f, base, W / 2), fy(f, base, W / 2));
    ns_linspace(0, PI / 3, 9, t);
    for (i = 0; i < 9; i++) { double u = base + S + W * ns_sin(t[i]), v = -W / 2 + W * ns_cos(t[i]); pl_add(p, fx(f, u, v), fy(f, u, v)); }
    ns_linspace(PI / 3, 0, 9, t);
    for (i = 1; i < 9; i++) { double u = base + S + W * ns_sin(t[i]), v = W / 2 - W * ns_cos(t[i]); pl_add(p, fx(f, u, v), fy(f, u, v)); }
    return buf(poly(p), 0, 16);
}
/* su_jag(ff, u, W, rng, amp=0.3, n=6) */
static G *su_jag(const FF *f, double u, double W, PyRandom *rng, double amp, int n) {
    PTS *p = pl_new(); double vs[16]; int i;
    pl_add(p, fx(f, -50, -W), fy(f, -50, -W)); pl_add(p, fx(f, -50, W), fy(f, -50, W));
    ns_linspace(W, -W, n, vs);
    for (i = 0; i < n; i++) { double uu = u + U_(rng, -amp, amp); pl_add(p, fx(f, uu, vs[i]), fy(f, uu, vs[i])); }
    return poly(p);
}
/* su_chip(g, rng, r=(0.3, 0.6)) */
static G *su_chip(G *g, PyRandom *rng, double r0, double r1) {
    double xy[256]; int n, k, i, bi; GL *ps; G *q; double ba;
    if (shp_type(g) != GEOS_POLYGON || !shp_exterior(g)) ns_raise();          /* g.exterior: AttributeError on anything else */
    n = shp_coords(shp_exterior(g), xy, 128);
    if (n < 2) ns_raise();
    k = (int)RB(rng, n - 1);
    { double r = U_(rng, r0, r1); q = dif(g, stone(xy[2 * k], xy[2 * k + 1], r, rng, 0.8, 5)); }
    ps = polys(q);
    if (!ps->n) return g;
    bi = 0; ba = area(ps->v[0]);
    for (i = 1; i < ps->n; i++) if (area(ps->v[i]) > ba) { ba = area(ps->v[i]); bi = i; }
    return ps->v[bi];
}
/* su_drum(cx, cy, a, L, r, nfl=12, d=0.26, k=0.55, ends=None): (sect, c + r) */
typedef struct { double cx, cy, a, L, r, d, c; FF f; int nv; double vs[16]; int has_ends; double ends[2][5]; } Drum;
static double drum_hz(const Drum *D, double v) { return D->c + sqrt(pmax(0.0, D->r * D->r - v * v)); }
static int drum_sect(void *ctx, double z, G **out) {
    const Drum *D = (const Drum *)ctx; double w; int err = 0;
    *out = NULL;
    if (z <= D->c) w = D->r;
    else if (z < D->c + D->r) { double q = lf_fpow(z - D->c, 2.0, &err); if (err) return SO_ERR; w = sqrt(pmax(0.0, D->r * D->r - q)); }
    else return SO_OK;
    if (w < 0.2) return SO_OK;
    { NS_BEGIN
      G *g = rect(D->cx, D->cy, D->L, 2 * w, D->a); GL *gr = gl_new(); int i;
      if (D->has_ends) {
          GL *l = gl_new(); int s; gl_add(l, g);
          for (s = 0; s < 2; s++) {
              double sx = s ? 1 : -1, mx = D->ends[s][0], vs[5]; int j; PTS *p;
              for (j = 1; j < 5; j++) if (D->ends[s][j] > mx) mx = D->ends[s][j];
              if (!(mx > 0.05)) continue;
              p = pl_new(); pl_add(p, FX(D->f, sx * D->L / 2, -w)); pl_add(p, FX(D->f, sx * D->L / 2, w));
              ns_linspace(w, -w, 5, vs);
              for (j = 0; j < 5; j++) pl_add(p, FX(D->f, sx * (D->L / 2 + D->ends[s][j]), vs[j]));
              gl_add(l, buf(poly(p), 0, 16));
          }
          g = U(l);
      }
      for (i = 0; i < D->nv; i++) { double v = D->vs[i]; if (z > drum_hz(D, v) - D->d && fabs(v) < w - 0.2) gl_add(gr, barl(FX(D->f, -D->L, v), FX(D->f, D->L, v), 0.3)); }
      if (gr->n) g = dif(g, U(gr));
      *out = shp_clone(g);
      NS_END(SO_OK); }
}
static void su_drum(SoSolids *sol, double cx, double cy, double a, double L, double r, int nfl, const double *ends_m1, const double *ends_p1) {
    Drum D; int j;
    memset(&D, 0, sizeof D);
    D.cx = cx; D.cy = cy; D.a = a; D.L = L; D.r = r; D.d = 0.26; D.f = ff_(cx, cy, a); D.c = 0.55 * r;
    for (j = 0; j < nfl; j++) { double t = 2 * PI * (j + 0.5) / nfl - PI; if (fabs(t) < 1.25) D.vs[D.nv++] = r * ns_sin(t); }
    if (ends_m1) { D.has_ends = 1; memcpy(D.ends[0], ends_m1, sizeof D.ends[0]); memcpy(D.ends[1], ends_p1, sizeof D.ends[1]); }
    sl_custom(sol, drum_sect, &D, sizeof D, D.c + r);
}
/* su_bent(p0, ang, L, rng, n=4, bend=0.7) */
static PTS *su_bent(double x, double y, double ang, double L, PyRandom *rng, int n, double bend) {
    PTS *p = pl_new(); double t = ang, sv = CH(rng, -1, 1); int i;
    pl_add(p, x, y);
    for (i = 0; i < n; i++) { t += sv * bend * U_(rng, 0.3, 1.0) * (i > 0); x += L / n * ns_cos(t); y += L / n * ns_sin(t); pl_add(p, x, y); }
    return p;
}
/* su_bars(o, pts_list, r=0.27, kind="rust") */
static void su_bars(SoObj *ob, PTS **pl, int n, double r, const char *kind) {
    SoSolids *s = sl_new(); int i;
    for (i = 0; i < n; i++) S_rod(s, pl[i], r, 1.0, 0.0, 0.0);
    if (s->n) ob_st(ob, s, kind, -1, NULL, NULL);
}
static int ob_nf(SoObj *ob) { return ob->F.n; }
static G *lnb(PTS *p, double d) { return buf(line(p), d, 16); }

BODY(su_fluted_drum) {
    double L = U_(rng, 3.2, 4.0), r = U_(rng, 1.25, 1.45), e0[5] = { 0 }, e1[5]; FF f = ff_(cx, cy, a); SoObj *ob = obj_new(ST_); int nfl, i, n; SoSolids *s = sl_new();
    nfl = (int)CH(rng, 12, 16); for (i = 0; i < 5; i++) e1[i] = U_(rng, 0.0, 0.55);
    su_drum(s, cx, cy, a, L, r, nfl, e0, e1); ob_st(ob, s, NULL, -1, NULL, rect(cx, cy, L + 0.5, 2 * r, a));
    n = (int)RI(rng, 1, 3);
    for (i = 0; i < n; i++) {
        double u = U_(rng, -L / 2, L / 2), ch = CH(rng, -1, 1), v = ch * (r + U_(rng, 0.4, 0.9)), p0 = fx(&f, u, v), p1 = fy(&f, u, v), rr = U_(rng, 0.42, 0.6), h; G *g = stone(p0, p1, rr, rng, 0.7, 5);
        h = U_(rng, 0.35, 0.55); ob_up(ob, g, h, "rubble");
    }
    OUT_;
}
BODY(su_drum_row) {
    int n = (int)RI(rng, 2, 3), nfl, i; double r = U_(rng, 1.05, 1.2), x; SoObj *ob = obj_new(ST_); FF f = ff_(cx, cy, a);
    x = -(n * 2.9) / 2; nfl = (int)CH(rng, 12, 16);
    for (i = 0; i < n; i++) {
        double Li = i < n - 1 ? U_(rng, 2.3, 2.8) : U_(rng, 1.4, 2.0), v = U_(rng, -0.35, 0.35), c0 = fx(&f, x + Li / 2, v), c1 = fy(&f, x + Li / 2, v), t = a + U_(rng, -0.22, 0.22), rr = r * U_(rng, 0.95, 1.0); SoSolids *s = sl_new();
        su_drum(s, c0, c1, t, Li, rr, nfl, NULL, NULL); ob_st(ob, s, NULL, -1, NULL, rect(c0, c1, Li, 2 * r, t)); x += Li + U_(rng, 0.25, 0.6);
    }
    OUT_;
}
BODY(su_column_stump) {
    double P_ = U_(rng, 3.2, 3.6), r = U_(rng, 1.15, 1.3), h; SoObj *ob = obj_new(ST_); int n = (int)CH(rng, 12, 16), b, t1, s1, s2; G *pl, *sh, *br;
    pl = su_chip(rect(cx, cy, P_, P_, a), rng, 0.3, 0.6); h = U_(rng, 0.45, 0.6); b = ob_up(ob, pl, h, NULL);
    t1 = ob_on(ob, disc(cx, cy, r + 0.32, 28), b, 0.3, NULL);
    sh = star(cx, cy, r, r - 0.17, n, a);
    h = U_(rng, 1.5, 2.4); s1 = ob_on(ob, sh, t1, h * 0.6, NULL);
    { double x = cx + U_(rng, -0.3, 0.3), y = cy + U_(rng, -0.3, 0.3), rr = r * U_(rng, 0.7, 0.85); br = inter(stone(x, y, rr, rng, 0.8, 6), sh); }
    s2 = ob_on(ob, br, s1, h * 0.25, NULL);
    if (RR(rng) < 0.6) { double x = cx + U_(rng, -0.3, 0.3), y = cy + U_(rng, -0.3, 0.3); ob_on(ob, inter(stone(x, y, r * 0.45, rng, 0.8, 5), br), s2, h * 0.15, NULL); }
    { double t = a + U_(rng, 0.4, 1.2), p0 = polx(cx, P_ * 0.75, t), p1 = poly_(cy, P_ * 0.75, t), rr = U_(rng, 0.6, 0.8), hh; G *g = stone(p0, p1, rr, rng, 0.7, 6); hh = U_(rng, 0.5, 0.7); ob_up(ob, g, hh, "rubble"); }
    OUT_;
}
BODY(su_fallen_capital) {
    double A = U_(rng, 3.0, 3.4), h; SoObj *ob = obj_new(ST_); G *ab; int b, bell, nk, k; GL *lv = gl_new(), *l = gl_new();
    ab = su_chip(rect(cx, cy, A, A, a), rng, 0.3, 0.5); h = U_(rng, 0.5, 0.6); b = ob_up(ob, ab, h, NULL);
    bell = ob_on(ob, disc(cx, cy, A * 0.42, 24), b, 0.45, NULL);
    for (k = 0; k < 8; k++) { double t = a + 6.2832 * k / 8 + 0.39; gl_add(lv, ell(POL(cx, cy, A * 0.36, t), 0.42, 0.3, t, 10)); }
    ob_on(ob, U(lv), b, 0.6, NULL);
    for (k = 0; k < 6; k++) gl_add(l, disc(POL(cx, cy, A * 0.2, a + 6.2832 * k / 6), 0.3, 8));
    ob_on(ob, U(l), bell, 0.35, NULL);
    nk = ob_on(ob, disc(cx, cy, A * 0.16, 16), bell, 0.4, NULL); ob_cut(ob, ann(cx, cy, A * 0.14, A * 0.07, 12), nk, 0.12);
    OUTL_(buf(ab, 0.2, 16));
}
BODY(su_lancet_window) {
    double W = U_(rng, 4.0, 4.6), S = U_(rng, 3.4, 4.2), t = U_(rng, 0.95, 1.1), ey0, ey1, re, h; FF f = ff_(cx, cy, a), fw; G *out_, *inn, *fr, *tr; SoObj *ob; int b; GL *l = gl_new();
    fw = ff_(FX(f, -(S + W * 0.866) / 2, 0), a);
    out_ = su_lancet(&fw, W, S, 0.0); inn = su_lancet(&fw, W - 2 * t, S - 0.1, t * 0.7);
    if (RR(rng) < 0.5) { FF f2 = ff_(FX(f, -(S + W * 0.866) / 2, 0), a + PI); double u = -U_(rng, 0.6, 1.4); out_ = inter(out_, su_jag(&f2, u, W, rng, 0.3, 6)); }
    ob = obj_new(ST_); fr = dif(out_, inn); h = U_(rng, 0.65, 0.8); b = ob_up(ob, fr, h, NULL);
    ob_groove(ob, inset(out_, 0.35), b, 0.2, 0.12);
    ey0 = fx(&fw, S + W * 0.36, 0); ey1 = fy(&fw, S + W * 0.36, 0); re = W * 0.2;
    gl_add(l, barl(FX(fw, 0.1, 0), FX(fw, S + W * 0.36 - re, 0), 0.62)); gl_add(l, ann(ey0, ey1, re + 0.3, re - 0.2, 20));
    tr = inter(U(l), buf(inn, 0.1, 16));
    h = U_(rng, 0.55, 0.65); ob_st(ob, ext1(tr, h), NULL, -1, NULL, tr);
    OUTL_(buf(out_, 0.3, 16));
}
BODY(su_rose_window_shard) {
    double R = U_(rng, 4.2, 4.8), span = U_(rng, 0.9, 1.3), ox, oy, xy[32], h; SoObj *ob = obj_new(ST_); G *wedge, *fr; GL *ps, *l = gl_new(), *l2 = gl_new(); int i, b, k;
    ox = polx(cx, -R * 0.55, a); oy = poly_(cy, -R * 0.55, a);
    xy[0] = ox; xy[1] = oy; if (sd_arc(ox, oy, R, a - span / 2, a + span / 2, 14, xy + 2) != SO_OK) ns_raise();
    wedge = buf(polyv(xy, 16), 0, 16);
    { double rr = R * U_(rng, 0.25, 0.35); wedge = dif(wedge, disc(ox, oy, rr, 18)); }
    { double t = a + span * U_(rng, 0.2, 0.45), rr = U_(rng, 0.9, 1.4); wedge = dif(wedge, stone(POL(ox, oy, R * 0.7, t), rr, rng, 0.8, 6)); }
    ps = polys(wedge); if (!ps->n) ns_raise();
    { int bi = 0; double ba = area(ps->v[0]); for (i = 1; i < ps->n; i++) if (area(ps->v[i]) > ba) { ba = area(ps->v[i]); bi = i; } wedge = ps->v[bi]; }
    gl_add(l, ann(ox, oy, R, R - 0.7, 48)); gl_add(l, ann(ox, oy, R * 0.66 + 0.3, R * 0.66 - 0.3, 40));
    for (k = 0; k < 5; k++) gl_add(l, barl(ox, oy, POL(ox, oy, R, a + span * (k / 4.0 - 0.5)), 0.55));
    fr = inter(U(l), wedge);
    b = ob_up(ob, wedge, 0.35, NULL); h = U_(rng, 0.35, 0.45); ob_on(ob, fr, b, h, NULL);
    for (k = 0; k < 4; k++) gl_add(l2, ann(POL(ox, oy, R * 0.82, a + span * ((k + 0.5) / 4 - 0.5)), 0.5, 0.25, 12));
    ob_on(ob, inter(U(l2), wedge), b, 0.3, NULL);
    OUTL_(buf(wedge, 0.3, 16));
}
BODY(su_voussoirs) {
    double R = U_(rng, 4.0, 5.0), ox, oy, w; int n = (int)RI(rng, 4, 6), i; SoObj *ob = obj_new(ST_);
    ox = polx(cx, -R, a); oy = poly_(cy, -R, a); w = U_(rng, 1.4, 1.7);
    for (i = 0; i < n; i++) {
        double t = a + (i - (n - 1) / 2.0) * 0.32 + U_(rng, -0.05, 0.05), dr = U_(rng, -0.4, 0.5), xy[8], h; G *q, *g; int key = i == n / 2, b; PTS *p = pl_new();
        xy[0] = polx(ox, R + dr - w / 2, t - 0.13); xy[1] = poly_(oy, R + dr - w / 2, t - 0.13); xy[2] = polx(ox, R + dr + w / 2, t - 0.16); xy[3] = poly_(oy, R + dr + w / 2, t - 0.16);
        xy[4] = polx(ox, R + dr + w / 2, t + 0.16); xy[5] = poly_(oy, R + dr + w / 2, t + 0.16); xy[6] = polx(ox, R + dr - w / 2, t + 0.13); xy[7] = poly_(oy, R + dr - w / 2, t + 0.13);
        q = polyv(xy, 4);
        g = RR(rng) < 0.5 ? su_chip(q, rng, 0.2, 0.35) : q;
        h = U_(rng, 0.75, 0.95) * (key ? 1.15 : 1.0); b = ob_up(ob, g, h, NULL);
        pl_add(p, POL(ox, oy, R + dr - w / 2 + 0.45, t - 0.3)); pl_add(p, POL(ox, oy, R + dr - w / 2 + 0.45, t + 0.3));
        ob_cut(ob, lnb(p, 0.11), b, 0.12);
    }
    OUT_;
}
BODY(su_tracery_bits) {
    SoObj *ob = obj_new(ST_); int nfoot = 0, i, n = (int)RI(rng, 2, 3), j;
    for (i = 0; i < n; i++) {
        double d = U_(rng, 0.4, 2.4), p0 = polx(cx, d, a + i * 2.2), p1 = poly_(cy, d, a + i * 2.2), r = U_(rng, 0.95, 1.2), t; int k = (int)CH(rng, 3, 4); GL *lo = gl_new(), *li = gl_new(), *ps; G *fr;
        t = U_(rng, 0, 6.28);
        for (j = 0; j < k; j++) gl_add(lo, disc(POL(p0, p1, r * 0.5, t + 6.2832 * j / k), r * 0.55, 14));
        for (j = 0; j < k; j++) gl_add(li, disc(POL(p0, p1, r * 0.5, t + 6.2832 * j / k), r * 0.25, 12));
        fr = dif(buf(U(lo), 0.4, 16), U(li));
        if (RR(rng) < 0.4) fr = dif(fr, barl(POL(p0, p1, 2, t), POL(p0, p1, -2, t + 0.3), 0.3));
        ps = polys(fr);
        for (j = 0; j < ps->n; j++) if (area(ps->v[j]) > 0.8) { double h = U_(rng, 0.4, 0.55); ob_st(ob, ext1(ps->v[j], h), NULL, -1, NULL, ps->v[j]); nfoot++; }
    }
    if (!nfoot) return 1;
    OUT_;
}
typedef struct { double W, L; } Pin;
static int pin_rf(void *c, double q, double *out) { const Pin *p = (const Pin *)c; double D = p->L - 2.0; if (D == 0.0) return -1; *out = (p->W / 2) * (1 - (q + D / 2) / D) * 0.85 + 0.12; return 0; }
BODY(su_pinnacle) {
    double L = U_(rng, 5.6, 6.6), W = U_(rng, 1.5, 1.7); FF f = ff_(cx, cy, a); SoObj *ob = obj_new(ST_); int sh, j, sv, n; Pin pn; GL *l = gl_new(); G *cr; SoSolids *s = sl_new(), *d = sl_new();
    sh = ob_up(ob, rect(FX(f, -L / 2 + 1.0, 0), 2.0, W, a), W * 0.7, NULL);
    { double xy[6] = { FX(f, -L / 2 + 0.2, -W / 2), FX(f, -L / 2 + 1.8, -W / 2), FX(f, -L / 2 + 1.0, 0) }; ob_on(ob, buf(polyv(xy, 3), 0.0, 16), sh, 0.15, NULL); }
    pn.W = W; pn.L = L; S_spin(s, FX(f, 0.5, 0), a, L - 2.0, pin_rf, &pn, 1.0, 0.0, "ridge"); ob_st(ob, s, NULL, -1, NULL, NULL);
    n = (int)ns_int((L - 2.6) / 0.75);
    for (j = 0; j < n; j++) for (sv = -1; sv <= 1; sv += 2) gl_add(l, disc(FX(f, -L / 2 + 2.2 + j * 0.75, sv * (W / 2) * (1 - (j * 0.75 + 0.2) / (L - 2.0)) * 0.85), 0.32, 8));
    cr = U(l); ob_st(ob, ext1(cr, 0.55), NULL, -1, NULL, cr);
    S_dome(d, FX(f, L / 2 - 0.1, 0), 0.45, 0.7, 0.0, 1.0, 0.0, 20); ob_st(ob, d, NULL, -1, NULL, NULL);
    OUTL_(rect(FX(f, 0.2, 0), L + 0.6, W + 0.9, a));
}
typedef struct { double L, w; } Saint;
static int saint_rf(void *c, double q, double *out) {
    const Saint *k = (const Saint *)c; double s = (q + k->L / 2) / k->L, x; int err = 0;
    if (s < 0.62) { *out = k->w * (0.78 + 0.32 * s); return 0; }
    if (s < 0.72) { *out = k->w * (0.98 - 1.1 * (s - 0.62)); return 0; }
    if (s < 0.78) { *out = k->w * 0.5; return 0; }
    x = lf_fpow((s - 0.89) / 0.12, 2.0, &err); if (err) return -1;
    *out = k->w * 0.58 * sqrt(pmax(0.05, 1 - x)); return 0;
}
BODY(su_fallen_saint) {
    double L = U_(rng, 6.6, 7.6), w = U_(rng, 1.15, 1.3), vs[4] = { -0.6, -0.2, 0.25, 0.62 }; FF f = ff_(cx, cy, a); SoObj *ob = obj_new(SU_); Saint sk; SoSolids *body = sl_new(), *s1 = sl_new(), *s3 = sl_new(); NsCuts cu = { 0 }; int i, sx;
    sk.L = L; sk.w = w; S_spin(body, cx, cy, a, L, saint_rf, &sk, 1.05, 0.0, "round");
    for (i = 0; i < 4; i++) cut_add(&cu, barl(FX(f, -L / 2 - 0.2, vs[i]), FX(f, L * 0.1, vs[i] * 0.7), 0.22), w * 0.95);
    ob_st(ob, body, NULL, -1, &cu, rect(cx, cy, L, 2 * w, a));
    S_dome(s1, FX(f, L * 0.05, 0), w * 0.55, w * 1.25, 0.0, 1.0, 0.0, 20); ob_st(ob, s1, NULL, -1, NULL, NULL);
    ob_st(ob, ext1(rect(FX(f, L * 0.02, 0), 0.55, w * 1.3, a), w * 1.15), NULL, -1, NULL, NULL);
    S_dome(s3, FX(f, L * 0.39, 0), w * 0.66, w * 1.15, 0.0, 0.85, a, 20); ob_st(ob, s3, NULL, -1, NULL, NULL);
    for (sx = -1; sx <= 1; sx += 2) { double u = -L / 2 - U_(rng, 0.9, 1.4), p0 = fx(&f, u, sx * 0.45), p1 = fy(&f, u, sx * 0.45), h; G *g = ell(p0, p1, 0.62, 0.5, a, 10); h = U_(rng, 0.4, 0.6); ob_up(ob, g, h, SU_); }
    OUTL_(rect(FX(f, -0.6, 0), L + 2.6, 2 * w + 0.4, a));
}
BODY(su_statue_head) {
    double r = U_(rng, 1.55, 1.8), ts[6]; FF f = ff_(cx, cy, a); SoObj *ob = obj_new(SU_); SoSolids *s = sl_new(); NsCuts cu = { 0 }; GL *l = gl_new(), *l2 = gl_new(); int i;
    S_dome(s, cx, cy, r, r * 1.15, 0.0, 0.85, a, 20);
    ns_linspace(-1.2, 1.2, 6, ts); for (i = 0; i < 6; i++) gl_add(l, disc(POL(cx, cy, r * 0.8, a + PI + ts[i]), 0.3, 8));
    cut_add(&cu, U(l), r * 0.85); ob_st(ob, s, NULL, -1, &cu, NULL);
    gl_add(l2, ell(FX(f, r * 0.82, 0), 0.55, 0.45, a, 10)); gl_add(l2, rect(FX(f, r * 0.95, 0), 0.9, 0.55, a)); ob_st(ob, ext1(U(l2), r * 0.7), NULL, -1, NULL, NULL);
    ob_st(ob, ext1(stone(FX(f, -r * 0.2, -r * 0.95), 0.75, rng, 0.8, 6), 0.7), NULL, -1, NULL, NULL);
    OUTL_(disc(cx, cy, r * 1.3, 20));
}
BODY(su_statue_hand) {
    FF f = ff_(cx, cy, a); SoObj *ob = obj_new(SU_); double s = U_(rng, 0.9, 1.05), sv = CH(rng, -1, 1); G *palm; SoSolids *d = sl_new(); int j;
    palm = ell(FX(f, -0.6 * s, 0), 1.3 * s, 1.15 * s, a, 16); S_dome(d, FX(f, -0.6 * s, 0), 1.15 * s, 0.95 * s, 0.0, 0.88, a, 20); ob_st(ob, d, NULL, -1, NULL, palm);
    for (j = 0; j < 4; j++) { double v = (j - 1.5) * 0.52 * s, Lf = (1.7 - 0.25 * fabs(j - 1.3)) * s; PTS *p = pl_new(); SoSolids *r = sl_new(); pl_add(p, FX(f, 0.3 * s, v)); pl_add(p, FX(f, 0.3 * s + Lf, v * 1.05)); S_rod(r, p, 0.27 * s, 1.4, 0.0, 0.0); ob_st(ob, r, NULL, -1, NULL, NULL); }
    { PTS *p = pl_new(); SoSolids *r = sl_new(); pl_add(p, FX(f, -0.6 * s, sv * 1.0 * s)); pl_add(p, FX(f, 0.3 * s, sv * 1.55 * s)); S_rod(r, p, 0.3 * s, 1.4, 0.0, 0.0); ob_st(ob, r, NULL, -1, NULL, NULL); }
    ob_st(ob, ext1(stone(FX(f, -1.9 * s, 0), 0.75 * s, rng, 0.9, 6), 0.75 * s), NULL, -1, NULL, NULL);
    OUTL_(rect(FX(f, 0, 0), 4.6 * s, 3.6 * s, a));
}
BODY(su_empty_plinth) {
    double P_ = U_(rng, 3.6, 4.0); SoObj *ob = obj_new(ST_); FF f = ff_(cx, cy, a); int b0, b1, cap, sx;
    b0 = ob_up(ob, su_chip(rect(cx, cy, P_, P_, a), rng, 0.3, 0.6), 0.45, NULL); b1 = ob_on(ob, rect(cx, cy, P_ - 0.7, P_ - 0.7, a), b0, 0.9, NULL);
    ob_cut(ob, rect(FX(f, P_ / 2 - 0.55, 0), 0.25, P_ - 1.4, a), b1, 0.1);
    cap = ob_on(ob, rect(cx, cy, P_ - 0.5, P_ - 0.5, a), b1, 0.25, NULL);
    for (sx = -1; sx <= 1; sx += 2) { G *g = ell(FX(f, 0.15, sx * 0.42), 0.6, 0.32, a, 10); double h = U_(rng, 0.4, 0.6); ob_on(ob, g, cap, h, SU_); }
    { G *g = inter(ell(FX(f, -0.35, 0), 0.6, 0.9, a, 12), rect(FX(f, -0.6, 0), 0.6, 2, a)); double h = U_(rng, 0.7, 0.9); ob_on(ob, g, cap, h, SU_); }
    OUTL_(buf(rect(cx, cy, P_, P_, a), 0.3, 16));
}
BODY(su_ashlar_blocks) {
    SoObj *ob = obj_new(ST_); FF f = ff_(cx, cy, a); int last = -1, want = (int)RI(rng, 3, 4), i, j;
    for (i = 0; i < 30; i++) {
        double u, v, c0, c1, t, l, w; G *g; int hit = 0;
        if (ob_nf(ob) >= want) break;
        u = U_(rng, -2.4, 2.4); v = U_(rng, -1.6, 1.6); c0 = fx(&f, u, v); c1 = fy(&f, u, v); t = a + U_(rng, -0.8, 0.8);
        l = U_(rng, 1.9, 2.4); w = U_(rng, 1.1, 1.3); g = su_chip(rect(c0, c1, l, w, t), rng, 0.25, 0.45);
        for (j = 0; j < ob->F.n && !hit; j++) hit = dist(g, ob->F.v[j]) < 0.25;
        if (hit) continue;
        { double h = U_(rng, 0.75, 1.0); last = ob_up(ob, g, h, NULL); }
    }
    if (last >= 0 && RR(rng) < 0.5) { G *c = cen(ob->F.v[ob->F.n - 1]); double aa = a + U_(rng, 0, 3); ob_on(ob, rect(gx(c), gy(c), 1.3, 0.9, aa), last, 0.6, NULL); }
    if (!ob->F.n) return 1;
    OUT_;
}
BODY(su_masonry_heap) {
    double R = U_(rng, 2.2, 2.7); SoObj *ob = obj_new("rubble"); G *g; int i, n;
    g = buf(blob(cx, cy, R, rng, 6), 0.1, 16);
    { double h = U_(rng, 1.0, 1.3), sq = U_(rng, 0.75, 0.95); SoSolids *s = sl_new(); S_dome(s, cx, cy, R, h, 0.0, sq, a, 20); ob_st(ob, s, "spoil", -1, NULL, g); }
    n = (int)RI(rng, 3, 5);
    for (i = 0; i < n; i++) {
        double d = R * U_(rng, 0.55, 1.05), t = a + i * 1.4 + U_(rng, -0.3, 0.3), p0 = polx(cx, d, t), p1 = poly_(cy, d, t), l = U_(rng, 1.2, 1.6), w = U_(rng, 0.9, 1.1), aa = U_(rng, 0, 3), h; G *q;
        q = su_chip(rect(p0, p1, l, w, aa), rng, 0.2, 0.3); h = U_(rng, 0.6, 1.1); ob_up(ob, q, h, ST_);
    }
    { double x = cx + U_(rng, -0.4, 0.4), y = cy + U_(rng, -0.4, 0.4), aa = U_(rng, 0, 3), h; G *q = rect(x, y, 1.1, 0.9, aa); h = U_(rng, 1.4, 1.7); ob_st(ob, ext1(q, h), ST_, -1, NULL, NULL); }
    OUTL_(buf(g, 0.6, 16));
}
BODY(su_heaved_flags) {
    SoObj *ob = obj_new(ST_); FF f = ff_(cx, cy, a); double s = U_(rng, 1.9, 2.2); int i, j;
    for (i = -1; i < 2; i++) for (j = -1; j < 1; j++) {
        double u, v, c0, c1, t, h; G *g; int b;
        if (RR(rng) < 0.25) continue;
        u = i * (s + 0.25) + U_(rng, -0.15, 0.15); v = (j + 0.5) * (s + 0.25); c0 = fx(&f, u, v); c1 = fy(&f, u, v); t = a + U_(rng, -0.15, 0.15);
        { double w = s * U_(rng, 0.85, 1.0); g = rect(c0, c1, s, w, t); }
        if (RR(rng) < 0.35) g = su_chip(g, rng, 0.35, 0.6);
        h = U_(rng, 0.3, 0.75); b = ob_up(ob, g, h, NULL);
        if (RR(rng) < 0.5) { double t1 = t + U_(rng, 0.3, 1.2), px = polx(c0, s, t1), py = poly_(c1, s, t1), t2 = t + PI + U_(rng, 0.3, 1.2); ob_cut(ob, inter(wavy(px, py, POL(c0, c1, s, t2), rng, 0.2, 3, 0.2), inset(g, 0.2)), b, 0.15); }
    }
    if (!ob->F.n) return 1;
    OUT_;
}
BODY(su_rebar_slab) {
    FF f = ff_(cx, cy, a); double L = U_(rng, 3.8, 4.6), W = U_(rng, 2.8, 3.3), vs[5], h; SoObj *ob = obj_new("concrete"); PTS *p = pl_new(), *bars[3]; G *g; int b, i, nb;
    pl_add(p, FX(f, -L / 2, -W / 2)); { double u = L / 2 - U_(rng, 0, 0.6); pl_add(p, FX(f, u, -W / 2)); }
    ns_linspace(-W / 2 + 0.4, W / 2 - 0.4, 5, vs); for (i = 0; i < 5; i++) { double u = L / 2 + U_(rng, -0.5, 0.4); pl_add(p, FX(f, u, vs[i])); }
    { double u = L / 2 - U_(rng, 0, 0.6); pl_add(p, FX(f, u, W / 2)); } pl_add(p, FX(f, -L / 2, W / 2));
    g = buf(poly(p), 0, 16);
    { G *q = su_chip(g, rng, 0.4, 0.7); h = U_(rng, 0.6, 0.8); b = ob_up(ob, q, h, NULL); }
    { double u1 = U_(rng, -L / 3, 0), ax = fx(&f, u1, -W), ay = fy(&f, u1, -W), u2 = U_(rng, 0, L / 3); ob_cut(ob, inter(wavy(ax, ay, FX(f, u2, W), rng, 0.3, 4, 0.22), inset(g, 0.25)), b, 0.15); }
    nb = (int)RI(rng, 2, 3); ns_linspace(-W / 2 + 0.7, W / 2 - 0.7, nb, vs);
    for (i = 0; i < nb; i++) { double ang = a + U_(rng, -0.3, 0.3), Lb = U_(rng, 2.2, 3.2); bars[i] = su_bent(FX(f, L / 2 - 0.4, vs[i]), ang, Lb, rng, 4, 0.7); }
    su_bars(ob, bars, nb, 0.24, "rust");
    OUTL_(rect(FX(f, 1.0, 0), L + 3.0, W + 1.5, a));
}
BODY(su_rebar_tangle) {
    SoObj *ob = obj_new("rust"); PTS *bars[4]; int n = (int)RI(rng, 3, 4), i, j; GL *l = gl_new();
    for (i = 0; i < n; i++) { double d = U_(rng, 0, 1.2), t = U_(rng, 0, 6.28), p0 = polx(cx, d, t), p1 = poly_(cy, d, t), ang = a + U_(rng, -0.7, 0.7) + PI * (i % 2), Lb = U_(rng, 3.0, 4.2); bars[i] = su_bent(p0, p1, ang, Lb, rng, 4, 0.5); }
    su_bars(ob, bars, n, 0.25, "rust");
    for (i = 0; i < 2 && i < n; i++) if (RR(rng) < 0.6) { PTS *p = bars[i]; double x = p->v[2 * (p->n - 1)], y = p->v[2 * (p->n - 1) + 1], r = U_(rng, 0.55, 0.7), h; G *g = stone(x, y, r, rng, 0.8, 5); h = U_(rng, 0.45, 0.6); ob_up(ob, g, h, "concrete"); }
    for (i = 0; i < n; i++) for (j = 0; j < bars[i]->n; j++) gl_add(l, disc(bars[i]->v[2 * j], bars[i]->v[2 * j + 1], 0.5, 20));
    OUTL_(hull(l));
}
BODY(su_gothic_door) {
    double W = U_(rng, 3.2, 3.6), S = U_(rng, 3.0, 3.6), vs[4], xs[3], h; FF f = ff_(cx, cy, a), fw; SoObj *ob = obj_new("wood"); G *g; int b, i, j, rg; GL *l = gl_new(), *l2 = gl_new(), *l3 = gl_new();
    fw = ff_(FX(f, -(S + W * 0.866) / 2, 0), a);
    g = su_lancet(&fw, W, S, 0.0);
    if (RR(rng) < 0.35) {
        double v1 = U_(rng, -0.6, 0.6), ax = fx(&fw, -1, v1), ay = fy(&fw, -1, v1), v2 = U_(rng, -0.6, 0.6); GL *ps = polys(dif(g, barl(ax, ay, FX(fw, S + W, v2), 0.35))); int bi = 0; double ba;
        if (!ps->n) ns_raise();
        ba = area(ps->v[0]); for (i = 1; i < ps->n; i++) if (area(ps->v[i]) > ba) { ba = area(ps->v[i]); bi = i; } g = ps->v[bi];
    }
    h = U_(rng, 0.45, 0.55); b = ob_up(ob, g, h, NULL);
    ns_linspace(-W / 2 + W / 5, W / 2 - W / 5, 4, vs); for (i = 0; i < 4; i++) gl_add(l, barl(FX(fw, -1, vs[i]), FX(fw, S + W, vs[i]), 0.2));
    ob_cut(ob, U(l), b, 0.12);
    xs[0] = 0.7; xs[1] = S * 0.55; xs[2] = S + 0.3;
    for (i = 0; i < 3; i++) gl_add(l2, barl(FX(fw, xs[i], -W / 2), FX(fw, xs[i], W / 2 * 0.75), 0.5));
    ob_on(ob, inter(U(l2), inset(g, 0.15)), b, 0.15, "iron");
    ns_linspace(-W / 2 + 0.4, W / 2 - 0.5, 4, vs);
    for (i = 0; i < 3; i++) for (j = 0; j < 4; j++) gl_add(l3, disc(FX(fw, xs[i], vs[j]), 0.24, 6));
    ob_on(ob, inter(U(l3), inset(g, 0.2)), b, 0.25, "iron");
    { double r0 = fx(&fw, S * 0.55 - 0.9, W * 0.25), r1 = fy(&fw, S * 0.55 - 0.9, W * 0.25); rg = ob_on(ob, disc(r0, r1, 0.55, 12), b, 0.2, "iron"); ob_cut(ob, disc(r0, r1, 0.22, 8), rg, 0.12); }
    OUTL_(buf(g, 0.3, 16));
}
BODY(su_toppled_pew) {
    double L = U_(rng, 6.6, 7.6); FF f = ff_(cx, cy, a); SoObj *ob = obj_new("wood"); int bk, sx;
    ob_up(ob, rect(FX(f, 0, -0.7), L - 0.7, 1.25, a), 0.55, NULL); bk = ob_up(ob, rect(FX(f, 0, 0.75), L - 0.7, 1.3, a), 0.75, NULL);
    ob_cut(ob, barl(FX(f, -L, 0.75), FX(f, L, 0.75), 0.2), bk, 0.12);
    for (sx = -1; sx <= 1; sx += 2) {
        GL *l; int e;
        if (sx > 0 && RR(rng) < 0.3) { double v = U_(rng, -0.5, 0.5), c0 = fx(&f, L / 2 + 1.2, v), c1 = fy(&f, L / 2 + 1.2, v), aa = a + U_(rng, -0.5, 0.5); ob_up(ob, rect(c0, c1, 0.95, 2.6, aa), 0.5, NULL); continue; }
        l = gl_new(); gl_add(l, rect(FX(f, sx * (L / 2 - 0.4), 0), 0.95, 3.0, a)); gl_add(l, tri(FX(f, sx * (L / 2 - 0.4), 1.4), FX(f, sx * (L / 2 - 0.4), 2.3), 0.95));
        e = ob_up(ob, U(l), 1.0, NULL); ob_cut(ob, disc(FX(f, sx * (L / 2 - 0.4), -0.5), 0.22, 6), e, 0.1);
    }
    OUTL_(rect(FX(f, 0, 0.3), L + 0.6, 4.4, a));
}
typedef struct { FF f0, f1; double kink; } Rail;
static void rail_F(const Rail *r, double x, double v, double *ox, double *oy) {
    if (x <= r->kink) { *ox = fx(&r->f0, x, v); *oy = fy(&r->f0, x, v); }
    else { *ox = fx(&r->f1, x - r->kink, v); *oy = fy(&r->f1, x - r->kink, v); }
}
BODY(su_spiked_railing) {
    double L = U_(rng, 6.0, 7.2), xs[64], ls[9]; SoObj *ob = obj_new("iron"); Rail R; int i, n, k; GL *rl = gl_new(), *bl = gl_new(), *tl = gl_new(), *rb, *ht = gl_new(); G *rails, *bars, *tips, *rbu; double vs[2] = { -0.9, 0.9 };
    R.kink = U_(rng, -0.15, 0.15) * L; { double ch = CH(rng, -1, 1), bend = ch * U_(rng, 0.25, 0.5); double px, py; R.f0 = ff_(cx, cy, a); px = fx(&R.f0, R.kink, 0); py = fy(&R.f0, R.kink, 0); R.f1 = ff_(px, py, a + bend); }
    ns_linspace(-L / 2, L / 2, 9, ls);
    for (k = 0; k < 2; k++) { PTS *p = pl_new(); for (i = 0; i < 9; i++) { double x, y; rail_F(&R, ls[i], vs[k], &x, &y); pl_add(p, x, y); } gl_add(rl, lineb(p, 0.5)); }
    rails = U(rl);
    n = ns_arange(-L / 2 + 0.3, L / 2, 0.95, xs, 64);
    for (i = 0; i < n; i++) { double x0, y0, x1, y1; rail_F(&R, xs[i], -1.25, &x0, &y0); rail_F(&R, xs[i], 1.15, &x1, &y1); gl_add(bl, barl(x0, y0, x1, y1, 0.48)); }
    bars = U(bl);
    for (i = 0; i < n; i++) { double x0, y0, x1, y1; rail_F(&R, xs[i], 1.1, &x0, &y0); rail_F(&R, xs[i], 1.95, &x1, &y1); gl_add(tl, tri(x0, y0, x1, y1, 0.8)); }
    tips = U(tl);
    rb = gl_new(); gl_add(rb, rails); gl_add(rb, bars); rbu = U(rb);
    ob_st(ob, ext1(rbu, 0.42), NULL, -1, NULL, buf(rbu, 0.1, 16));
    ob_st(ob, ext1(tips, 0.32), NULL, -1, NULL, tips);
    gl_add(ht, rails); gl_add(ht, tips);
    OUTL_(buf(hull(ht), 0.2, 16));
}
typedef struct { double L, R; } Bell;
static int bell_rf(void *c, double q, double *out) {
    const Bell *b = (const Bell *)c; double s = (q + b->L / 2) / b->L;
    if (s < 0.97) { double x = 0.0; if (s > 0.72) { int err = 0; x = 0.45 * lf_fpow((s - 0.72) / 0.28, 1.6, &err); if (err) return -1; } *out = b->R * (0.45 + 0.25 * s + x); }
    else *out = b->R * 1.12;
    return 0;
}
BODY(su_fallen_bell) {
    double L = U_(rng, 3.8, 4.4), R = U_(rng, 1.6, 1.8); FF f = ff_(cx, cy, a); SoObj *ob = obj_new("brass"); Bell bl; SoSolids *s = sl_new(); NsCuts cu = { 0 }; GL *l = gl_new();
    bl.L = L; bl.R = R; S_spin(s, cx, cy, a, L, bell_rf, &bl, 1.0, 0.0, "round");
    cut_add(&cu, ell(FX(f, L / 2 - 0.25, 0), 0.45, R * 0.9, a, 14), R * 0.5); cut_add(&cu, barl(FX(f, L / 2 - 1.2, R * 0.3), FX(f, L / 2, R * 0.4), 0.22), R * 1.05);
    ob_st(ob, s, NULL, -1, &cu, rect(cx, cy, L, 2.2 * R, a));
    gl_add(l, rect(FX(f, -L / 2 - 0.35, 0), 0.8, 1.1, a)); ob_st(ob, ext1(U(l), R * 0.8), NULL, -1, NULL, NULL);
    { double u = U_(rng, -0.5, 0.5), ch = CH(rng, -1, 1), v = ch * (R + 1.0), p0 = fx(&f, u, v), p1 = fy(&f, u, v), ll = U_(rng, 3.6, 4.6), aa = a + U_(rng, -0.4, 0.4); ob_up(ob, rect(p0, p1, ll, 0.95, aa), 0.8, "wood"); }
    OUTL_(rect(FX(f, -0.2, 0), L + 1.2, 2 * R + 2.6, a));
}
typedef struct { double r, L; } Case;
static int case_rf(void *c, double q, double *out) { const Case *k = (const Case *)c; *out = k->r * (q < -k->L / 2 + 0.15 ? 1.12 : 1.0) * (q > k->L / 2 - 0.25 ? 0.92 : 1.0); return 0; }
BODY(su_shell_casings) {
    SoObj *ob = obj_new("brass"); GL *ft = gl_new(); int want = (int)RI(rng, 2, 4), i, j;
    for (i = 0; i < 30; i++) {
        double d, t0, p0, p1, t, L, r; G *g; int hit = 0; Case cs; SoSolids *s;
        if (ft->n >= want) break;
        d = U_(rng, 0.3, 2.0); t0 = U_(rng, 0, 6.28); p0 = polx(cx, d, t0); p1 = poly_(cy, d, t0); t = U_(rng, 0, 6.28); L = U_(rng, 2.2, 2.6); r = U_(rng, 0.42, 0.5);
        g = rect(p0, p1, L + 0.3, 2 * r + 0.3, t);
        for (j = 0; j < ft->n && !hit; j++) hit = intersects(g, ft->v[j]);
        if (hit) continue;
        gl_add(ft, g); cs.r = r; cs.L = L; s = sl_new(); S_spin(s, p0, p1, t, L, case_rf, &cs, 1.15, 0.0, "round"); ob_st(ob, s, NULL, -1, NULL, NULL);
    }
    if (RR(rng) < 0.6) {
        double d = U_(rng, 2.6, 3.0), t = U_(rng, 0, 6.28), p0 = polx(cx, d, t), p1 = poly_(cy, d, t), h; int b;
        { G *g = disc(p0, p1, 0.58, 16); h = U_(rng, 1.8, 2.1); b = ob_up(ob, g, h, NULL); }
        ob_cut(ob, ann(p0, p1, 0.34, 0.12, 10), b, 0.35); gl_add(ft, disc(p0, p1, 0.6, 20));
    }
    if (!ft->n) return 1;
    OUTL_(hull(ft));
}
typedef struct { double L, r; } Dud;
static int dud_rf(void *c, double q, double *out) {
    const Dud *k = (const Dud *)c; double s = (q + k->L / 2) / k->L;
    if (s < 0.42) { *out = k->r * sqrt(pmax(0.04, s / 0.42)) * 0.98 + 0.05; return 0; }
    *out = k->r * (0.82 < s && s < 0.88 ? 1.08 : 1.0); return 0;
}
BODY(su_dud_shell) {
    double L = U_(rng, 3.4, 4.0), r = U_(rng, 0.7, 0.8); FF f = ff_(cx, cy, a); SoObj *ob = obj_new("iron"); Dud dd; SoSolids *s = sl_new(), *d = sl_new();
    dd.L = L; dd.r = r; S_spin(s, cx, cy, a, L, dud_rf, &dd, 1.0, 0.0, "round"); ob_st(ob, s, NULL, -1, NULL, rect(cx, cy, L, 2 * r, a));
    ob_st(ob, ext1(rect(FX(f, L * 0.35, 0), 0.5, 2 * r * 1.08, a), r * 1.08), "brass", -1, NULL, NULL);
    S_dome(d, FX(f, -L / 2 + 0.1, 0), 1.3, 0.6, 0.0, 0.85, a, 20); ob_st(ob, d, "spoil", -1, NULL, NULL);
    OUTL_(rect(FX(f, -0.3, 0), L + 1.6, 2.8, a));
}
BODY(su_sandbag_line) {
    double R = U_(rng, 6.0, 8.0), ox, oy; int n = (int)RI(rng, 4, 5), i; SoObj *ob = obj_new("sack"); GL *ft = gl_new(); SoSolids *ps = sl_new();
    ox = polx(cx, -R, a); oy = poly_(cy, -R, a);
    for (i = 0; i < n; i++) {
        double t = a + (i - (n - 1) / 2.0) * 0.27, p0 = polx(ox, R, t), p1 = poly_(oy, R, t), h = U_(rng, 0.7, 0.85);
        S_loaf(ps, p0, p1, t + PI / 2, 1.75, 1.1, h, 0.0); gl_add(ft, rect(p0, p1, 1.8, 1.15, t + PI / 2));
        if (i < n - 1 && RR(rng) < 0.85) { double q0 = polx(ox, R, t + 0.135), q1 = poly_(oy, R, t + 0.135); S_loaf(ps, q0, q1, t + 0.135 + PI / 2, 1.6, 0.95, 0.75, 0.55); }
    }
    ob_st(ob, ps, NULL, -1, NULL, U(ft));
    if (RR(rng) < 0.5) { double t = a + U_(rng, -0.5, 0.5); SoSolids *d = sl_new(); S_dome(d, POL(ox, oy, R + 1.4, t), 0.9, 0.4, 0.0, 0.7, a, 20); ob_st(ob, d, NULL, -1, NULL, NULL); }
    OUTL_(buf(hull(ft), 0.3, 16));
}
BODY(su_barricade_beams) {
    FF f = ff_(cx, cy, a); SoObj *ob = obj_new("wood"); double L = U_(rng, 6.0, 7.0), xs[2]; int b1, sx, i, n; GL *l = gl_new();
    { double aa = a + U_(rng, -0.08, 0.08); b1 = ob_up(ob, rect(FX(f, 0, -0.6), L, 1.0, aa), 0.85, NULL); }
    xs[0] = -L / 3; xs[1] = L / 4; for (i = 0; i < 2; i++) gl_add(l, barl(FX(f, xs[i], -2), FX(f, xs[i], 1), 0.2));
    ob_cut(ob, U(l), b1, 0.12);
    { double aa = a + U_(rng, -0.1, 0.1); ob_up(ob, rect(FX(f, 0, 0.9), L * 0.8, 0.95, aa), 0.8, NULL); }
    for (sx = -1; sx <= 1; sx += 2) { double t = a + sx * U_(rng, 0.5, 0.8), c0 = fx(&f, sx * L * 0.22, 0.2), c1 = fy(&f, sx * L * 0.22, 0.2); ob_st(ob, ext1(rect(c0, c1, 4.2, 0.9, t), 1.3), NULL, -1, NULL, rect(c0, c1, 4.2, 0.9, t)); }
    n = (int)RI(rng, 2, 3);
    for (i = 0; i < n; i++) {
        double u = U_(rng, -L / 2, L / 2), ch = CH(rng, -1, 1), v = ch * U_(rng, 1.8, 2.4), p0 = fx(&f, u, v), p1 = fy(&f, u, v), aa = U_(rng, 0, 3), h; G *q = su_chip(rect(p0, p1, 1.4, 1.0, aa), rng, 0.2, 0.3);
        h = U_(rng, 0.7, 0.9); ob_up(ob, q, h, ST_);
    }
    OUTL_(rect(cx, cy, L + 1.2, 5.6, a));
}
BODY(su_shell_scar) {
    double r = U_(rng, 1.2, 1.6), ts[8], uu; G *hole = stone(cx, cy, r, rng, 0.85, 9), *cr, *pk; int n, i; GL *cl = gl_new(), *pl = gl_new(), *hl = gl_new();
    n = (int)RI(rng, 5, 7); if (np_linspace(0, 6.28, n, 0, ts) < 0) ns_raise(); uu = U_(rng, 0, 1);
    for (i = 0; i < n; i++) {
        double t = ts[i] + uu, d = r + U_(rng, 1.4, 2.4), t2 = t + U_(rng, -0.2, 0.2); PTS *w = wiggle(POL(cx, cy, r * 0.8, t), POL(cx, cy, d, t2), rng, 0.25, 3);
        gl_add(cl, bufx(line(w), 0.22, 3, SHP_CAP_ROUND, SHP_JOIN_ROUND, 5.0));
    }
    cr = U(cl);
    n = (int)RI(rng, 4, 7);
    for (i = 0; i < n; i++) { double d = r + U_(rng, 0.8, 2.2), t = U_(rng, 0, 6.28), rx = U_(rng, 0.3, 0.45), aa = U_(rng, 0, 3); gl_add(pl, ell(POL(cx, cy, d, t), rx, 0.24, aa, 8)); }
    pk = U(pl);
    gl_add(hl, buf(hole, r + 2.4, 16)); *foot = hull(hl);
    ns_gcut(o, hole, 0.55, "pothole"); ns_gcut(o, dif(cr, buf(hole, 0.1, 16)), 0.35, "crack"); ns_gcut(o, dif(dif(pk, buf(cr, 0.15, 16)), buf(hole, 0.2, 16)), 0.3, "pothole");
    return 0;
}
BODY(su_split_flags) {
    FF f = ff_(cx, cy, a); double L = U_(rng, 5, 8), b, qx, qy; PTS *pts; int j, ks[2], i; GL *l = gl_new();
    pts = wiggle(FX(f, -L / 2, 0), FX(f, L / 2, 0), rng, 0.5, 6);
    j = (int)RI(rng, 2, 4); { double ch = CH(rng, -1, 1); b = a + ch * U_(rng, 0.5, 1.0); }
    qx = pts->v[2 * j] + L * 0.35 * ns_cos(b); qy = pts->v[2 * j + 1] + L * 0.35 * ns_sin(b);
    gl_add(l, bufx(line(pts), 0.26, 3, SHP_CAP_ROUND, SHP_JOIN_ROUND, 5.0));
    gl_add(l, bufx(line(wiggle(pts->v[2 * j], pts->v[2 * j + 1], qx, qy, rng, 0.3, 3)), 0.22, 3, SHP_CAP_ROUND, SHP_JOIN_ROUND, 5.0));
    ks[0] = 1; ks[1] = pts->n - 2;
    for (i = 0; i < 2; i++) { double r = U_(rng, 0.35, 0.5); gl_add(l, disc(pts->v[2 * ks[i]], pts->v[2 * ks[i] + 1], r, 6)); }
    { G *g = U(l); *foot = buf(g, 0.6, 16); ns_gcut(o, g, 0.4, "crack"); }
    return 0;
}

#define SD(fn, cls, kind, name, cf) static const NsSd D_##fn = { fn, "sundered", cls, kind, name, cf };
SD(su_fluted_drum, "made", "ashlar", "fluted_drum", 1) SD(su_drum_row, "made", "ashlar", "drum_row", 1) SD(su_column_stump, "made", "ashlar", "column_stump", 1)
SD(su_fallen_capital, "made", "ashlar", "fallen_capital", 1) SD(su_lancet_window, "made", "ashlar", "lancet_window", 1) SD(su_rose_window_shard, "made", "ashlar", "rose_window_shard", 1)
SD(su_voussoirs, "debris", "ashlar", "voussoirs", 1) SD(su_tracery_bits, "debris", "ashlar", "tracery_bits", 1) SD(su_pinnacle, "made", "ashlar", "pinnacle", 1)
SD(su_fallen_saint, "made", "statue", "fallen_saint", 1) SD(su_statue_head, "made", "statue", "statue_head", 1) SD(su_statue_hand, "made", "statue", "statue_hand", 1)
SD(su_empty_plinth, "made", "ashlar", "empty_plinth", 1) SD(su_ashlar_blocks, "debris", "ashlar", "ashlar_blocks", 1) SD(su_masonry_heap, "debris", "rubble", "masonry_heap", 1)
SD(su_heaved_flags, "debris", "ashlar", "heaved_flags", 1) SD(su_rebar_slab, "debris", "concrete", "rebar_slab", 1) SD(su_rebar_tangle, "debris", "rust", "rebar_tangle", 1)
SD(su_gothic_door, "made", "wood", "gothic_door", 1) SD(su_toppled_pew, "made", "wood", "toppled_pew", 1) SD(su_spiked_railing, "made", "iron", "spiked_railing", 1)
SD(su_fallen_bell, "made", "brass", "fallen_bell", 1) SD(su_shell_casings, "made", "brass", "shell_casings", 1) SD(su_dud_shell, "made", "iron", "dud_shell", 1)
SD(su_sandbag_line, "made", "sack", "sandbag_line", 1) SD(su_barricade_beams, "made", "wood", "barricade_beams", 1) SD(su_shell_scar, "marks", "pothole", "shell_scar", 0)
SD(su_split_flags, "marks", "crack", "split_flags", 0)
void ns_reg_sundered(void) {
    const NsSd *all[] = { &D_su_fluted_drum, &D_su_drum_row, &D_su_column_stump, &D_su_fallen_capital, &D_su_lancet_window, &D_su_rose_window_shard, &D_su_voussoirs,
        &D_su_tracery_bits, &D_su_pinnacle, &D_su_fallen_saint, &D_su_statue_head, &D_su_statue_hand, &D_su_empty_plinth, &D_su_ashlar_blocks, &D_su_masonry_heap,
        &D_su_heaved_flags, &D_su_rebar_slab, &D_su_rebar_tangle, &D_su_gothic_door, &D_su_toppled_pew, &D_su_spiked_railing, &D_su_fallen_bell, &D_su_shell_casings,
        &D_su_dud_shell, &D_su_sandbag_line, &D_su_barricade_beams, &D_su_shell_scar, &D_su_split_flags };
    int i; for (i = 0; i < (int)(sizeof all / sizeof *all); i++) ns_sd_reg(all[i], 10.5, !all[i]->cutfix);
}
