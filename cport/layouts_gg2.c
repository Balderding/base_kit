/* layouts_gg2.c: the ground layouts of gg2.py, AS SIZEFIX REWRITES THEM (work\cscene\layouts\gg2_fixed.py is the
   ast.unparse of the rewritten module: _KX / _KN / _KA there are KX_ / KN_ / KA_ here). Each function names the
   Python routine it copies; the random numbers are drawn in exactly the Python's order (one draw per C statement or
   argument list that has only one draw). */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "layouts.h"

#define H (SC.H)
#define R0 (SC.R0)
#define SINK (SC.SINK)
#define U(a, b) py_uniform(rng, (a), (b))
#define RI(a, b) ((int)py_randint(rng, (a), (b)))
#define RND() py_random(rng)
#define TI(f, a) { { (f), NUM_FLOAT, 0 }, { (a), NUM_INT, 0 } }
#define NT(t) ((int)(sizeof(t) / sizeof((t)[0])))
#define COS py_cos
#define SIN py_sin

static Geo *disc_r(double r) { return g_ptbuf(0, 0, r, 16); }          /* Point(0, 0).buffer(r) */

/* gg2.strata(s, plates, tops) */
static void strata(Scene *s, GeoList *plates, const double *tops, int ntops) {
    PyRandom *rng = &s->rng; GeoList *cl = LL(), *profs = LL(), *tp = LL(); OpList ops = { 0 }; Num zt; double h; int i;
    static const Tier T[] = { TI(0.82, 9), TI(0.18, 45) };
    LC(sc_clip(s, L1(g_uu(plates)), cl));
    for (i = 0; i < cl->n; i++) if (g_wide(cl->v[i], SC.MINW)) LP(profs, cl->v[i]);
    if (!profs->n) return;
    h = tops[ntops - 1];
    if (sc_tiers(s, profs, NF(H - SINK), NF(h), T, 2, KS("rock"), &ops, tp, &zt) < 0) { ol_free(&ops); ly_raise(); }
    if (!ops.n) { ol_free(&ops); return; }
    for (i = 0; i < ops.n; i++) ol_push(&s->ops, ops.v[i]);
    ol_free(&ops);
    gl_extend(&s->feet, profs);
    if (tp->n) {
        Geo *top = g_max_area(tp), *c = g_centroid(top); double a = U(0, PI), cx = g_x(c), cy = g_y(c);
        Geo *ln = g_inter(g_linecut(cx - 6 * COS(a), cy - 6 * SIN(a), cx + 6 * COS(a), cy + 6 * SIN(a), 0.5), g_inset(top, 0.3));
        if (!g_isempty(ln)) S_tcut(s, L1(ln), NF(H + h - 0.3), NF(H + h + 0.5), "crack");
    }
}

/* gg2.crack_net(s, npts, depth=0.45, region=None) */
static void crack_net(Scene *s, int npts, double depth, Geo *region) {
    PyRandom *rng = &s->rng;
    Geo *net = ly_vor_net(rng, KX_(-17), KX_(17), KA_(npts), KX_(-20), KX_(20), 1, 0.35, 0.23, 4);
    if (region) net = g_inter(net, region);
    S_gcut(s, L1(net), NF(depth), "crack");
}

/* gg2.badlands(n) */
Scene *gg2_badlands(int n) {
    Scene *s = ly_mk("badlands", "Badlands", n, "#a4553b"); PyRandom *rng = &s->rng; int v = n % 3, i; double th = U(0, 2 * PI);
    GeoList *pl = LL(), *sc;
    static const Tier T1[] = { TI(0.75, 5), TI(0.25, 40) }, T2[] = { TI(0.7, 10), TI(0.3, 45) };
    if (v == 0) {
        double cx = 7.5 * COS(th), cy = 7.5 * SIN(th);
        static const double tp[] = { 0.9, 1.75, 2.55 };
        LP(pl, g_stone(cx, cy, 4.2, rng, 0.7, 6));
        LP(pl, g_stone(cx + 4.3 * COS(th + 1.9), cy + 4.3 * SIN(th + 1.9), 3.0, rng, 0.75, 5));
        LP(pl, g_stone(cx + 4.0 * COS(th - 1.7), cy + 4.0 * SIN(th - 1.7), 2.4, rng, 0.8, 5));
        strata(s, pl, tp, 3);
    } else if (v == 1) {
        double as[2]; int k;
        static const double tp[] = { 0.8, 1.6 };
        as[0] = th; as[1] = th + PI + U(-0.5, 0.5);
        for (k = 0; k < 2; k++) {
            double a = as[k], cx = 8.5 * COS(a), cy = 8.5 * SIN(a); GeoList *l = LL();
            LP(l, g_stone(cx, cy, 3.0, rng, 0.7, 5));
            LP(l, g_stone(cx + 2.8 * COS(a + 1.6), cy + 2.8 * SIN(a + 1.6), 2.2, rng, 0.8, 5));
            strata(s, l, tp, 2);
        }
    } else {
        double d = U(3, 6), cx = d * COS(th), cy = d * SIN(th);
        static const double tp[] = { 0.7, 1.5, 2.3 };
        LP(pl, g_stone(cx, cy, 5.5, rng, 0.65, 7));
        strata(s, pl, tp, 3);
    }
    sc = S_scatter(s, RI(2, 6), 1.2, 1.9, NULL, 0.4, 0, 0);
    for (i = 0; i < sc->n; i++) S_stack(s, L1(sc->v[i]), NF(0.75), T1, 2, "rock", NONE, NULL);
    { int k = RI(18, 34); S_stack(s, S_scatter(s, k, 0.5, 0.75, NULL, 0.35, 1, 0), NF(0.35), T2, 2, "grit", NONE, NULL); }
    crack_net(s, RI(26, 42), 0.45, NULL);
    return s;
}

/* gg2.highland(n) */
Scene *gg2_highland(int n) {
    Scene *s = ly_mk("highland", "Highland", n, "#4c4a36"); PyRandom *rng = &s->rng; double th = U(0, 2 * PI);
    static const int cnt[4] = { 1, 2, 2, 3 }; int k, i, c, nc; GeoList *tus = LL(), *sc;
    static const Tier TG[] = { TI(0.55, 28), TI(0.45, 50) }, TF[] = { TI(1.0, 30) }, T2[] = { TI(0.7, 10), TI(0.3, 45) };
    for (k = 0; k < cnt[n % 4]; k++) {
        double a = th + k * 2.1, d = U(5.5, 9), r, h1, h2, aa; Geo *p, *q, *c0; GeoList *b, *u;
        r = k == 0 ? U(2.8, 4.8) : U(2.2, 3.2);
        p = g_stone(d * COS(a), d * SIN(a), r, rng, 0.5, 5);
        if (!S_free(s, p, 0.6)) continue;
        h1 = U(1.0, 1.5); h2 = U(1.8, 2.6);
        b = PR(s, L1(p), H + h1, "slate");
        if (!b->n) continue;
        { double tx = U(-0.7, 0.7), ty = U(-0.7, 0.7); q = g_trans(g_scale(b->v[0], 0.72, 0.8, SHP_ORIGIN_CENTER, 0, 0), tx, ty); }
        u = S_prism(s, g_polys(g_inter(q, g_inset(b->v[0], 0.4))), NF(H + h2), "slate", NF(H + h1 - 0.15), SC.MINW, 1);
        c0 = g_centroid(u->n ? u->v[0] : b->v[0]);
        aa = a + U(0.6, 1.2);
        if (LYC.outcrop_cracks) {
            double cx = g_x(c0), cy = g_y(c0);
            S_tcut(s, L1(g_inter(g_linecut(cx - 8 * COS(aa), cy - 8 * SIN(aa), cx + 8 * COS(aa), cy + 8 * SIN(aa), 0.5), b->v[0])), NF(H + h1 - 0.45), NF(H + 3.0), "crack");
        }
    }
    sc = S_scatter(s, RI(2, 6), 1.0, 1.6, NULL, 0.4, 0, 0);
    for (i = 0; i < sc->n; i++) PR(s, L1(sc->v[i]), H + 0.7, "slate");
    nc = RI(3, 6);
    { Geo *dk = disc_r(R0 - 0.5);
      for (c = 0; c < nc; c++) {
          double a = U(0, 2 * PI), d = U(2, 10), x = d * COS(a), y = d * SIN(a); int64_t j, m = KN_(14);
          for (j = 0; j < m; j++) {
              double ro = U(1.9, 2.8), px = x + U(-3, 3), py, rot; Geo *p; int ok;
              py = y + U(-3, 3); rot = U(0, 1);
              p = LAG(lf_star(px, py, ro, ro * 0.55, 7, rot));
              ok = g_contains(dk, p) && S_free(s, p, 0.3);
              if (ok) { Geo *pb = NULL; for (i = 0; i < tus->n; i++) { if (!pb) pb = g_buf(p, 0.3); if (!(!g_intersects(pb, tus->v[i]) || g_area(g_inter(p, tus->v[i])) > 1.0)) { ok = 0; break; } } }
              if (ok) LP(tus, p);
          }
      } }
    if (tus->n) S_stack(s, L1(g_uu(tus)), NF(1.5), TG, 2, "grass", NONE, NULL);
    nc = RI(1, 4);
    for (c = 0; c < nc; c++) {
        double a = U(0, 2 * PI), d = U(2, 11), x = d * COS(a), y = d * SIN(a); GeoList *fl = LL(); Geo *g;
        for (i = 0; i < 6; i++) { double px = x + U(-1.1, 1.1), py = y + U(-1.1, 1.1); LP(fl, g_ptbuf(px, py, 0.5, 8)); }
        g = g_uu(fl);
        if (S_free(s, g, 0.3)) S_stack(s, L1(g), NF(0.55), TF, 1, "flower", NONE, NULL);
    }
    { int k2 = RI(14, 26); S_stack(s, S_scatter(s, k2, 0.5, 0.75, NULL, 0.35, 1, 0), NF(0.35), T2, 2, "grit", NONE, NULL); }
    return s;
}

/* the deck plates of corridor_a / corridor_x: an octagon per cell (P, c, a, v0), cut by keep */
static Geo *octa(const Frame *f, double u, double vv, double a, double c) {
    double uv[16] = { -a + c, -a, a - c, -a, a, -a + c, a, a - c, a - c, a, -a + c, a, -a, a - c, -a, -a + c }; int i;
    for (i = 0; i < 8; i++) { uv[2 * i] = u + uv[2 * i]; uv[2 * i + 1] = vv + uv[2 * i + 1]; }
    return g_frpoly(f, uv, 8);
}
static Geo *ptf(const Frame *f, double u, double v, double r, int q) { double x, y; lf_at(f, u, v, &x, &y); return g_ptbuf(x, y, r, q); }

/* gg2.corridor_a(n) */
static Scene *corridor_a(int n) {
    Scene *s = ly_mk("corridor", "Spaceship Corridor", n, "#2f3540"); PyRandom *rng = &s->rng; int v = n % 3, i, k;
    double phi = U(0, PI), off, pvs[2], P, c, a, v0, *us; int npv, nu; Frame f = lf_frame(0, 0, phi); GeoList *keep = LL(), *plates = LL(), *vents = LL(), *treads = LL(), *l; Geo *kp;
    static const Tier TP[] = { TI(0.45, 6), TI(0.35, 30), TI(0.2, 55) }, TQ[] = { TI(0.6, 6), TI(0.4, 40) };
    static const double PC[3] = { 5.6, 6.0, 6.4 };
    off = U(-2, 2);
    if (v == 0 || v == 2) {
        PR(s, L2(g_R(&f, KX_(-20), KX_(20), off - 4.2, off - 3.1), g_R(&f, KX_(-20), KX_(20), off + 3.1, off + 4.2)), H + 0.9, "rail");
        l = LL(); nu = ly_arange(KX_(-19), KX_(19), 1.75, &us);
        for (i = 0; i < nu; i++) LP(l, g_R(&f, us[i], us[i] + 0.95, off - 3.4, off + 3.4));
        S_prism(s, l, NF(H + 0.55), "metal", NF(H - 0.8), SC.MINW, 0);
        S_gcut(s, L1(g_R(&f, KX_(-20), KX_(20), off - 3.15, off + 3.15)), NF(0.6), "slot");
        LP(keep, g_R(&f, KX_(-20), KX_(20), off - 4.2, off + 4.2));
    }
    {          /* {0: [off + rng.choice([-1, 1]) * 9.6], 1: [off + 7.5], 2: [off - 9.6, off + 9.6]}[v]: every value is made */
        int64_t ch = py_choice(rng, 2); double p0 = off + (ch ? 1 : -1) * 9.6;
        if (v == 0) { pvs[0] = p0; npv = 1; } else if (v == 1) { pvs[0] = off + 7.5; npv = 1; } else { pvs[0] = off - 9.6; pvs[1] = off + 9.6; npv = 2; }
    }
    for (k = 0; k < npv; k++) {
        double pv = pvs[k]; Geo *pipe = g_R(&f, KX_(-20), KX_(20), pv - 1.15, pv + 1.15);
        S_stack(s, L1(pipe), NF(1.5), TP, 3, "pipe", NONE, NULL);
        l = LL(); nu = ly_arange(KX_(-15) + U(0, 3), KX_(16), 6.5, &us);
        for (i = 0; i < nu; i++) LP(l, g_R(&f, us[i], us[i] + 1.3, pv - 1.6, pv + 1.6));
        S_stack(s, l, NF(1.9), TQ, 2, "pipe", NONE, NULL);
        LP(keep, g_buf(pipe, 0.45));
    }
    if (v == 1) {
        double hc = U(-3, 1); Geo *hatch = g_R(&f, hc - 4, hc + 4, off - 5.5, off + 2.5); GeoList *hp = PR(s, L1(hatch), H + 0.65, "plate");
        if (hp->n) {
            static const double X[2] = { -3.1, 3.1 }; int xi, yi;
            S_tcut(s, L2(g_R(&f, hc - 3, hc + 3, off - 1.75, off - 1.25), g_R(&f, hc - 0.25, hc + 0.25, off - 4.5, off + 1.5)), NF(H + 0.25), NF(H + 1.0), "slot");
            l = LL(); for (xi = 0; xi < 2; xi++) for (yi = 0; yi < 2; yi++) LP(l, ptf(&f, hc + X[xi], off - 1.5 + X[yi], 0.5, 8));
            S_prism(s, l, NF(H + 0.95), "metal", NF(H + 0.5), SC.MINW, 1);
        }
        LP(keep, g_buf(hatch, 0.5));
    }
    kp = keep->n ? g_uu(keep) : g_empty();
    P = PC[py_choice(rng, 3)]; c = n % 2 ? 1.7 : 0.7; a = P / 2 - 0.25; v0 = off + 4.8 + a;
    {
        int64_t i0 = KN_(-4), i1 = KN_(5), ii, jj; double kf = ly_pow(SC.K > 1.0 ? SC.K : 1.0, 1.3);
        for (ii = i0; ii < i1; ii++) for (jj = i0; jj < i1; jj++) {
            double u = ii * P, vv = v0 + jj * P; Geo *o = octa(&f, u, vv, a, c); GeoList *gs = g_polys(g_inter(g_diff(o, kp), s->disc)); int gi;
            for (gi = 0; gi < gs->n; gi++) {
                Geo *g = gs->v[gi];
                if (g_wide(g, SC.MINW) && g_area(g) > 1.2) {
                    LP(plates, g);
                    if (fabs(g_area(g) - g_area(o)) < 0.01) {
                        double kk = RND() * kf;
                        if (kk < 0.35) { static const double W[3] = { -1.45, -0.45, 0.55 }; int w; for (w = 0; w < 3; w++) LP(vents, g_R(&f, u - 1.6, u + 1.6, vv + W[w], vv + W[w] + 0.5)); }
                        else if (kk < 0.6) { static const double X[3] = { -1.3, 0, 1.3 }; int xi, yi; for (xi = 0; xi < 3; xi++) for (yi = 0; yi < 3; yi++) LP(treads, ptf(&f, u + X[xi], vv + X[yi], 0.47, 8)); }
                    }
                }
            }
        }
        PR(s, plates, H + 0.5, "plate");
        if (vents->n) S_tcut(s, vents, NF(H + 0.1), NF(H + 1.0), "slot");
        if (treads->n) S_prism(s, treads, NF(H + 0.8), "metal", NF(H + 0.3), SC.MINW, 1);
        if (c > 1) {
            GeoList *riv = LL(); Geo *dk = disc_r(R0 - 1.0);
            for (ii = i0; ii < i1; ii++) for (jj = i0; jj < i1; jj++) {
                Geo *p = ptf(&f, ii * P + P / 2, v0 + jj * P + P / 2, 0.5, 8); int ok, gi;
                ok = !g_intersects(g_buf(p, 0.2), kp) && g_contains(dk, p);
                if (ok) for (gi = 0; gi < plates->n; gi++) if (g_intersects(p, plates->v[gi])) { ok = 0; break; }
                if (ok) LP(riv, p);
            }
            PR(s, riv, H + 0.4, "metal");
        }
    }
    return s;
}

/* corridor_x's free(p): not keep or not p.buffer(0.4).intersects(unary_union(keep)) */
static int cx_free(GeoList *keep, Geo *p) { return !keep->n || !g_intersects(g_buf(p, 0.4), g_uu(keep)); }
/* corridor_x's place(w, l, tries=80): (cx, cy, p) or 0 */
static int cx_place(PyRandom *rng, GeoList *keep, double phi, double w, double l, double *cx, double *cy, Geo **p) {
    int t; Geo *dk = disc_r(R0 - 0.6);
    for (t = 0; t < 80; t++) {
        double a = U(0, 2 * PI), d = U(1, 9.5); Frame ff; double uv[8];
        *cx = d * COS(a); *cy = d * SIN(a); ff = lf_frame(*cx, *cy, phi);
        uv[0] = -l / 2; uv[1] = -w / 2; uv[2] = l / 2; uv[3] = -w / 2; uv[4] = l / 2; uv[5] = w / 2; uv[6] = -l / 2; uv[7] = w / 2;
        *p = g_frpoly(&ff, uv, 4);
        if (cx_free(keep, *p) && g_contains(dk, *p)) return 1;
    }
    return 0;
}

/* gg2.corridor_x(n) */
static Scene *corridor_x(int n) {
    static const char *const TAB[] = { "hatch cables hazard", "walk2 light", "drain cables junction", "hazard diamond pipe", "light hatch junction",
        "light cables diamond drain", "cables drain hazard", "junction light diamond pipe", "hatch hazard cables", "drain light pipe" };
    Scene *s = ly_mk("corridor", "Spaceship Corridor", n, "#2f3540"); PyRandom *rng = &s->rng; const char *F[16]; int nF, i, k;
    double phi, off, lanes[8], P, c, a, v0, *us; int li = 0, nu; Frame f; GeoList *keep = LL(), *l, *plates = LL(), *vents = LL(), *treads = LL(), *diam = LL(); Geo *kp;
    static const Tier TP[] = { TI(0.45, 6), TI(0.35, 30), TI(0.2, 55) }, TQ[] = { TI(0.6, 6), TI(0.4, 40) }, TC[] = { TI(0.6, 12), TI(0.4, 40) }, TJ[] = { TI(0.6, 10), TI(0.4, 45) };
    static const double PC[3] = { 5.6, 6.0, 6.4 };
    nF = ly_pickF(TAB, 11, 10, n, F, 12);
    phi = U(0, PI); f = lf_frame(0, 0, phi); off = U(-1.5, 1.5);
    { double L0[8] = { off - 8.5, off + 8.5, off - 2.0, off + 4.5, off - 12.5, off + 12.5, off - 15.5, off + 15.5 }; memcpy(lanes, L0, sizeof lanes); }
#define NEXTLANE() (li < 8 ? lanes[li++] : (ly_raise(), 0.0))
    if (ly_in(F, nF, "walk2")) {
        PR(s, L2(g_R(&f, KX_(-20), KX_(20), off - 4.2, off - 3.1), g_R(&f, KX_(-20), KX_(20), off + 3.1, off + 4.2)), H + 0.9, "rail");
        l = LL(); nu = ly_arange(KX_(-19), KX_(19), 1.75, &us);
        for (i = 0; i < nu; i++) LP(l, g_R(&f, us[i], us[i] + 0.95, off - 3.4, off + 3.4));
        S_prism(s, l, NF(H + 0.55), "metal", NF(H - 0.8), SC.MINW, 0);
        S_gcut(s, L1(g_R(&f, KX_(-20), KX_(20), off - 3.15, off + 3.15)), NF(0.6), "slot");
        LP(keep, g_R(&f, KX_(-20), KX_(20), off - 4.2, off + 4.2));
        { double L1v[8] = { off - 9.6, off + 9.6, off - 13.0, off + 13.0, off - 15.8, off + 15.8, off - 17.5, off + 17.5 }; memcpy(lanes, L1v, sizeof lanes); li = 0; }
        F[nF++] = "pipe"; F[nF++] = "pipe";
    }
    for (k = ly_count(F, nF, "pipe"); k > 0; k--) {
        double pv = NEXTLANE(); Geo *pipe = g_R(&f, KX_(-20), KX_(20), pv - 1.15, pv + 1.15);
        S_stack(s, L1(pipe), NF(1.5), TP, 3, "pipe", NONE, NULL);
        l = LL(); nu = ly_arange(KX_(-15) + U(0, 3), KX_(16), 6.5, &us);
        for (i = 0; i < nu; i++) LP(l, g_R(&f, us[i], us[i] + 1.3, pv - 1.6, pv + 1.6));
        S_stack(s, l, NF(1.9), TQ, 2, "pipe", NONE, NULL);
        LP(keep, g_buf(pipe, 0.45));
    }
    if (ly_in(F, nF, "cables")) {
        double cv = NEXTLANE(); static const double W[3] = { -1.15, 0.0, 1.15 }; int w;
        PR(s, L1(g_R(&f, KX_(-20), KX_(20), cv - 1.9, cv + 1.9)), H + 0.35, "metal");
        for (w = 0; w < 3; w++) S_stack(s, L1(g_R(&f, KX_(-20), KX_(20), cv + W[w] - 0.45, cv + W[w] + 0.45)), NF(0.6), TC, 2, "cable", NF(H + 0.35 - 0.15 + SINK), NULL);
        l = LL(); nu = ly_arange(KX_(-14), KX_(15), 5.0, &us);
        for (i = 0; i < nu; i++) LP(l, g_R(&f, us[i] - 0.3, us[i] + 0.3, cv - 2.0, cv + 2.0));
        S_tcut(s, l, NF(H + 0.75), NF(H + 1.6), "slot");
        LP(keep, g_R(&f, KX_(-20), KX_(20), cv - 1.9, cv + 1.9));
    }
    if (ly_in(F, nF, "hazard")) {
        double hv = NEXTLANE(); GeoList *band = PR(s, L1(g_R(&f, KX_(-20), KX_(20), hv - 1.4, hv + 1.4)), H + 0.6, "plate");
        if (band->n) {
            GeoList *st = LL(); nu = ly_arange(KX_(-24), KX_(20), 1.6, &us);
            for (i = 0; i < nu; i++) { double u = us[i], uv[8] = { u, hv - 2, u + 0.55, hv - 2, u + 0.55 + 4, hv + 2, u + 4, hv + 2 }; LP(st, g_frpoly(&f, uv, 4)); }
            S_tcut(s, L1(g_inter(g_uu(st), g_inset(band->v[0], 0.35))), NF(H + 0.3), NF(H + 1.0), "slot");
        }
        LP(keep, g_R(&f, KX_(-20), KX_(20), hv - 1.4, hv + 1.4));
    }
    if (ly_in(F, nF, "light")) {
        double lv = NEXTLANE();
        S_gcut(s, L1(g_R(&f, KX_(-20), KX_(20), lv - 0.7, lv + 0.7)), NF(0.55), "slot");
        l = LL(); nu = ly_arange(KX_(-18), KX_(18), 3.6, &us);
        for (i = 0; i < nu; i++) LP(l, g_R(&f, us[i], us[i] + 2.6, lv - 0.45, lv + 0.45));
        S_prism(s, l, NF(H + 0.15), "lens", NF(H - 0.75), SC.MINW, 0);
        LP(keep, g_R(&f, KX_(-20), KX_(20), lv - 0.7, lv + 0.7));
    }
#undef NEXTLANE
    if (ly_in(F, nF, "hatch")) {
        double cx, cy; Geo *hatch;
        if (cx_place(rng, keep, phi, 8.0, 8.0, &cx, &cy, &hatch)) {
            Frame ff = lf_frame(cx, cy, phi); GeoList *hp = PR(s, L1(hatch), H + 0.65, "plate");
            if (hp->n) {
                double u1[8] = { -3, -0.25, 3, -0.25, 3, 0.25, -3, 0.25 }, u2[8] = { -0.25, -3, 0.25, -3, 0.25, 3, -0.25, 3 }; static const double X[2] = { -3.1, 3.1 }; int xi, yi;
                S_tcut(s, L2(g_frpoly(&ff, u1, 4), g_frpoly(&ff, u2, 4)), NF(H + 0.25), NF(H + 1.0), "slot");
                l = LL(); for (xi = 0; xi < 2; xi++) for (yi = 0; yi < 2; yi++) LP(l, ptf(&ff, X[xi], X[yi], 0.5, 8));
                S_prism(s, l, NF(H + 0.95), "metal", NF(H + 0.5), SC.MINW, 1);
            }
            LP(keep, hatch);
        }
    }
    if (ly_in(F, nF, "drain")) {
        double cx, cy; Geo *pp;
        if (cx_place(rng, keep, phi, 6.6, 6.6, &cx, &cy, &pp)) {
            Geo *ringg = g_ptbuf(cx, cy, 3.3, 64); GeoList *rp = PR(s, L1(ringg), H + 0.6, "plate");
            if (rp->n) {
                GeoList *sl = LL(); nu = ly_arange(0, 2 * PI, PI / 6, &us);
                for (i = 0; i < nu; i++) { double aa = us[i]; LP(sl, g_linecut(cx + 1.3 * COS(aa), cy + 1.3 * SIN(aa), cx + 2.6 * COS(aa), cy + 2.6 * SIN(aa), 0.5)); }
                S_tcut(s, sl, NF(H - 0.2), NF(H + 1.0), "slot");
                S_prism(s, L1(g_ptbuf(cx, cy, 0.8, 24)), NF(H + 0.9), "metal", NF(H + 0.45), SC.MINW, 1);
            }
            LP(keep, ringg);
        }
    }
    if (ly_in(F, nF, "junction")) {
        double cx, cy; Geo *jb;
        if (cx_place(rng, keep, phi, 3.6, 4.6, &cx, &cy, &jb)) {
            Frame ff = lf_frame(cx, cy, phi); GeoList *jp = PR(s, L1(jb), H + 1.7, "metal");
            if (jp->n) {
                double u1[8] = { -2.6, -0.2, 2.6, -0.2, 2.6, 0.2, -2.6, 0.2 }; static const double X[2] = { -1.6, 1.6 }, Y[2] = { -1.1, 1.1 }; int xi, yi;
                double ang, b2x, b2y, ln[4]; Geo *cl;
                S_tcut(s, L1(g_frpoly(&ff, u1, 4)), NF(H + 1.4), NF(H + 2.2), "slot");
                l = LL(); for (xi = 0; xi < 2; xi++) for (yi = 0; yi < 2; yi++) LP(l, ptf(&ff, X[xi], Y[yi], 0.47, 8));
                S_prism(s, l, NF(H + 1.95), "metal", NF(H + 1.55), SC.MINW, 1);
                ang = py_atan2(cy, cx);
                b2x = 18 * COS(ang); b2y = 18 * SIN(ang);
                ln[0] = cx; ln[1] = cy; ln[2] = b2x; ln[3] = b2y;
                cl = g_linebuf(ln, 2, 0.7, SHP_CAP_FLAT, 16);
                if (!keep->n || !g_intersects(g_buf(g_diff(cl, jb), 0.4), g_uu(keep)))
                    S_stack(s, g_polys(g_diff(cl, g_buf(jb, -0.4))), NF(1.0), TJ, 2, "pipe", NONE, NULL);
                LP(keep, jb); LP(keep, g_linebuf(ln, 2, 0.7, SHP_CAP_FLAT, 16));
            }
        }
    }
    P = PC[py_choice(rng, 3)]; c = n % 2 ? 1.7 : 0.7; a = P / 2 - 0.25;
    kp = keep->n ? g_buf(g_uu(keep), 0.5) : g_empty();
    v0 = off + 4.8 + a;
    {
        int64_t i0 = KN_(-4), i1 = KN_(5), ii, jj; double kf = ly_pow(SC.K > 1.0 ? SC.K : 1.0, 1.3); int dia = ly_in(F, nF, "diamond");
        for (ii = i0; ii < i1; ii++) for (jj = i0; jj < i1; jj++) {
            double u = ii * P, vv = v0 + jj * P; Geo *o = octa(&f, u, vv, a, c); GeoList *gs = g_polys(g_inter(g_diff(o, kp), s->disc)); int gi;
            for (gi = 0; gi < gs->n; gi++) {
                Geo *g = gs->v[gi];
                if (g_wide(g, SC.MINW) && g_area(g) > 1.2) {
                    LP(plates, g);
                    if (fabs(g_area(g) - g_area(o)) < 0.01) {
                        double kk = RND() * kf;
                        if (dia && kk < 0.4) {
                            double *xs; int nx = ly_arange(-1.8, 1.9, 1.2, &xs), xi, yi;
                            for (xi = 0; xi < nx; xi++) for (yi = 0; yi < nx; yi++) {
                                if (ly_mod(ly_rint(xs[xi] / 1.2) + ly_rint(xs[yi] / 1.2), 2) == 0) {
                                    double px, py; lf_at(&f, u + xs[xi], vv + xs[yi], &px, &py);
                                    LP(diam, g_rot(g_ptbuf(px, py, 0.5, 4), 45, SHP_ORIGIN_POINT, px, py));
                                }
                            }
                        } else if (kk < 0.6) { static const double W[3] = { -1.45, -0.45, 0.55 }; int w; for (w = 0; w < 3; w++) LP(vents, g_R(&f, u - 1.6, u + 1.6, vv + W[w], vv + W[w] + 0.5)); }
                        else if (kk < 0.8) { static const double X[3] = { -1.3, 0, 1.3 }; int xi, yi; for (xi = 0; xi < 3; xi++) for (yi = 0; yi < 3; yi++) LP(treads, ptf(&f, u + X[xi], vv + X[yi], 0.47, 8)); }
                    }
                }
            }
        }
        PR(s, plates, H + 0.5, "plate");
        if (vents->n) S_tcut(s, vents, NF(H + 0.1), NF(H + 1.0), "slot");
        if (treads->n) S_prism(s, treads, NF(H + 0.8), "metal", NF(H + 0.3), SC.MINW, 1);
        if (diam->n) S_prism(s, diam, NF(H + 0.75), "metal", NF(H + 0.3), SC.MINW, 1);
        if (c > 1) {
            GeoList *riv = LL(); Geo *dk = disc_r(R0 - 1.0);
            for (ii = i0; ii < i1; ii++) for (jj = i0; jj < i1; jj++) {
                Geo *p = ptf(&f, ii * P + P / 2, v0 + jj * P + P / 2, 0.5, 8); int ok, gi;
                ok = !g_intersects(g_buf(p, 0.2), kp) && g_contains(dk, p);
                if (ok) for (gi = 0; gi < plates->n; gi++) if (g_intersects(p, plates->v[gi])) { ok = 0; break; }
                if (ok) LP(riv, p);
            }
            PR(s, riv, H + 0.4, "metal");
        }
    }
    return s;
}

/* gg2.corridor(n) */
Scene *gg2_corridor(int n) { return n <= 10 ? corridor_a(n) : corridor_x(n); }

/* gg2.infestation(n) */
Scene *gg2_infestation(int n) {
    Scene *s = ly_mk("infestation", "Alien Infestation", n, "#2b3140"); PyRandom *rng = &s->rng;
    double th = U(0, 2 * PI), d0 = U(6, 9), bx = d0 * COS(th), by = d0 * SIN(th), a6; GeoList *l = LL(), *tend = LL(), *pl = LL(), *slots = LL(), *t1, *ins, *profs;
    Geo *mass, *tg, *MB, *MBB; int i, k, nk, np_ = 0, cap = 0; Num ztop;
    typedef struct { Geo *q; double x, y; int whole; } Pl; Pl *plates = NULL;
    static const Tier TF[] = { TI(0.4, 14), TI(0.35, 32), TI(0.25, 52) }, TU[] = { TI(0.6, 20), TI(0.4, 55) };
    nk = RI(4, 7);
    for (i = 0; i < nk; i++) { double x = bx + U(-3, 3), y = by + U(-3, 3), r = U(2.0, 3.4); LP(l, g_ptbuf(x, y, r, 12)); }
    mass = g_uu(l);
    if (n % 2 == 0) {
        GeoList *m = LL();
        for (i = 0; i < 3; i++) { double x = -bx * 1.15 + U(-2, 2), y = -by * 1.15 + U(-2, 2), r = U(1.8, 2.6); LP(m, g_ptbuf(x, y, r, 12)); }
        mass = g_uu2(mass, g_uu(m));
    }
    mass = g_simp(g_buf(g_buf(mass, 1.0), -1.0), 0.05);
    nk = RI(3, 6);
    for (k = 0; k < nk; k++) {
        double a = th + PI + U(-1.3, 1.3), x = bx + 2.5 * COS(a), y = by + 2.5 * SIN(a), L, pts[18];
        pts[0] = x; pts[1] = y; L = U(6, 12);
        for (i = 0; i < 8; i++) { a += U(-0.5, 0.5); x += L / 8 * COS(a); y += L / 8 * SIN(a); pts[2 * i + 2] = x; pts[2 * i + 3] = y; }
        for (i = 0; i < 8; i++) LP(tend, g_linebuf(&pts[2 * i], 2, 0.75 - 0.03 * i, SHP_CAP_ROUND, 6));
        LP(tend, g_ptbuf(pts[16], pts[17], 0.85, 10));
    }
    tg = g_inter(g_diff(g_uu(tend), g_buf(mass, -0.6)), disc_r(R0 - 0.3));
    a6 = U(2.6, 3.4);
    MB = g_bufq(g_buf(mass, 0.3), 0, 16);
    { double b[4]; g_bounds(MB, b); MBB = g_buf(g_box(b[0], b[1], b[2], b[3]), 0.5); }
    {
        int64_t i0 = KN_(-7), i1 = KN_(8), ii, jj;
        for (ii = i0; ii < i1; ii++) for (jj = i0; jj < i1; jj++) {
            double x = ii * a6 * 1.5, y = (ly_mod(ii, 2) ? jj + 0.5 : (double)jj) * a6 * sqrt(3.0), hx[12], ha; Geo *h, *cutp; GeoList *qs; int kk, qi;
            if (py_hypot(x, y) > R0 + a6 + 1.5) continue;
            for (kk = 0; kk < 6; kk++) { hx[2 * kk] = x + (a6 - 0.3) * COS(kk * PI / 3); hx[2 * kk + 1] = y + (a6 - 0.3) * SIN(kk * PI / 3); }
            h = g_poly(hx, 6);
            {          /* try: cutp = h.difference(MB) if MBB.intersects(h) else h / except Exception: continue */
                int hit = shp_intersects(MBB->g, h->g);
                if (hit < 0) continue;
                if (hit) { GEOSGeometry *d = shp_difference(h->g, MB->g); if (!d) continue; cutp = LA(d); }
                else cutp = h;
            }
            qs = g_polys(g_inter(cutp, s->disc)); ha = g_area(h);
            for (qi = 0; qi < qs->n; qi++) {
                Geo *q = qs->v[qi];
                if (g_wide(q, SC.MINW) && g_area(q) > 1.2) {
                    if (np_ >= cap) { cap = cap ? 2 * cap : 64; plates = (Pl *)realloc(plates, sizeof *plates * (size_t)cap); }
                    plates[np_].q = q; plates[np_].x = x; plates[np_].y = y; plates[np_].whole = fabs(g_area(q) - ha) < 0.01; np_++;
                    LP(pl, q);
                }
            }
        }
    }
    { Pl *keep_ = (Pl *)LM(sizeof(Pl) * (size_t)(np_ + 1)); if (np_) memcpy(keep_, plates, sizeof(Pl) * (size_t)np_); free(plates); plates = keep_; }
    PR(s, pl, H + 0.45, "plate");
    for (i = 0; i < np_; i++) {
        if (plates[i].whole && a6 > 2.9 && RND() < 0.3 && !g_intersects(plates[i].q, tg)) {
            static const double W[3] = { -1.0, 0.0, 1.0 }; int w; double x = plates[i].x, y = plates[i].y;
            for (w = 0; w < 3; w++) LP(slots, g_box(x - 1.4, y + W[w], x + 1.4, y + W[w] + 0.5));
        }
    }
    if (slots->n) S_tcut(s, slots, NF(H + 0.05), NF(H + 1.0), "slot");
    { GeoList *tp = g_polys(tg), *big = LL(); for (i = 0; i < tp->n; i++) if (g_area(tp->v[i]) > 0.5) LP(big, tp->v[i]); t1 = PR(s, big, H + 0.75, "flesh"); }
    ins = LL(); for (i = 0; i < t1->n; i++) gl_extend(ins, g_polys(g_inset(t1->v[i], 0.3)));
    S_prism(s, ins, NF(H + 1.15), "flesh", NF(H + 0.6), SC.MINW, 1);
    profs = S_stack(s, g_polys(mass), NF(2.3), TF, 3, "flesh", NONE, &ztop);
    if (profs->n) {
        Geo *big = g_max_area(profs); GeoList *pus = LL(); int64_t t, m = KN_(60);
        for (t = 0; t < m; t++) {
            double b[4], x, y, r; Geo *p; int ok;
            if (pus->n >= RI(2, 5)) break;
            g_bounds(big, b);
            x = U(b[0], b[2]); y = U(b[1], b[3]); r = U(0.55, 0.75);
            p = g_ptbuf(x, y, r, 10);
            ok = g_contains(g_buf(big, -0.1), p);
            if (ok) for (i = 0; i < pus->n; i++) if (!(g_dist(p, pus->v[i]) > 0.3)) { ok = 0; break; }
            if (ok) LP(pus, p);
        }
        if (pus->n) S_stack(s, pus, NF(0.6), TU, 2, "pustule", num_add(num_sub(ztop, NF(0.15)), NF(SINK)), NULL);
    }
    return s;
}

/* gg2.urban_a(n) */
static Scene *urban_a(int n) {
    Scene *s = ly_mk("urban", "Urban Warfare", n, "#7e7b76"); PyRandom *rng = &s->rng; double phi = U(0, PI), d, *us; Frame f = lf_frame(0, 0, phi);
    GeoList *l = LL(), *slab, *sc; Geo *sl, *core, *g, *x, *road, *hole = NULL; int pv = n % 3, i, nu, nk, k;
    static const Tier TC[] = { TI(0.8, 3), TI(0.2, 40) }, TR[] = { TI(0.6, 4), TI(0.4, 30) }, TG[] = { TI(1.0, 20) };
    d = U(-5, 2.5) * SC.K;
    nu = ly_arange(KX_(-19.5) + U(0, 2), KX_(19), 4.5, &us);
    for (i = 0; i < nu; i++) LP(l, g_R(&f, us[i], us[i] + 4.0, d, d + 1.8));
    S_stack(s, l, NF(1.0), TC, 2, "curb", NONE, NULL);
    slab = PR(s, L1(g_R(&f, KX_(-20), KX_(20), d + 1.8, KX_(20))), H + 0.75, "slab");
    sl = slab->n ? g_uu(slab) : g_empty();
    core = g_diff(sl, g_R(&f, KX_(-20), KX_(20), d - 0.5, d + 2.3));
    if (pv == 2) {
        GeoList *a = LL(), *b = LL(); nu = ly_arange(KX_(-20), KX_(20), 3.6, &us);
        for (i = 0; i < nu; i++) LP(a, g_box(KX_(-20), us[i] - 0.22, KX_(20), us[i] + 0.22));
        g = g_rot(g_uu(a), LF_DEGREES(phi) + 45, SHP_ORIGIN_POINT, 0, 0);
        for (i = 0; i < nu; i++) LP(b, g_box(us[i] - 0.22, KX_(-20), us[i] + 0.22, KX_(20)));
        x = g_rot(g_uu(b), LF_DEGREES(phi) + 45, SHP_ORIGIN_POINT, 0, 0);
    } else {
        double L = pv == 0 ? 3.2 : 1.55, w = pv == 0 ? 1.55 : 3.2; GeoList *a = LL(), *b = LL();
        if (pv == 0) {
            int64_t j, ii;
            for (j = KN_(1); j < KN_(14); j++) LP(a, g_R(&f, KX_(-20), KX_(20), d + 2.3 + j * w - 0.22, d + 2.3 + j * w + 0.22));
            g = g_uu(a);
            for (j = KN_(0); j < KN_(14); j++) for (ii = KN_(-8); ii < KN_(9); ii++)
                LP(b, g_R(&f, ii * L + ly_mod(j, 2) * L / 2 - 0.22, ii * L + ly_mod(j, 2) * L / 2 + 0.22, d + 2.3 + j * w + 0.1, d + 2.3 + (j + 1) * w - 0.1));
            x = g_uu(b);
        } else {
            int64_t j;
            nu = ly_arange(KX_(-20), KX_(20), w, &us);
            for (i = 0; i < nu; i++) LP(a, g_R(&f, us[i] - 0.22, us[i] + 0.22, KX_(-20), KX_(20)));
            g = g_uu(a);
            for (k = 0; k < nu; k++) for (j = KN_(-1); j < KN_(6); j++)
                LP(b, g_R(&f, us[k] + 0.1, us[k] + w - 0.1, d + 2.3 + j * 3.2 + ly_mod(k, 2) * 1.6 - 0.22, d + 2.3 + j * 3.2 + ly_mod(k, 2) * 1.6 + 0.22));
            x = g_uu(b);
        }
    }
    S_tcut(s, L1(g_inter(g_uu2(g, x), core)), NF(H + 0.33), NF(H + 0.75), "joint");
    if (n % 2) {
        double u0 = U(-6, 2); GeoList *t = LL();
        PR(s, L1(g_R(&f, u0, u0 + 6.0, d - 2.7, d + 0.1)), H + 0.3, "metal");
        for (i = 0; i < 4; i++) LP(t, g_R(&f, u0 + 0.8 + i * 1.1, u0 + 1.3 + i * 1.1, d - 2.1, d - 0.5));
        S_tcut(s, t, NF(H - 0.5), NF(H + 1.0), "slot");
    }
    road = g_R(&f, KX_(-20), KX_(20), KX_(-20), d - 0.3);
    sc = S_scatter(s, RI(2, 5), 1.1, 1.9, road, 0.5, 0, 0);
    for (i = 0; i < sc->n; i++) { double h = U(0.8, 1.2); S_stack(s, L1(sc->v[i]), NF(h), TR, 2, "rubble", NONE, NULL); }
    if (n % 3 != 0) {
        int64_t t, m = KN_(60); Geo *dk = disc_r(R0 - 1.2);
        for (t = 0; t < m; t++) {
            double px, py, uu = U(-8, 8), vv = d - U(4, 9), r; Geo *cand;
            lf_at(&f, uu, vv, &px, &py); r = U(1.6, 2.4);
            cand = g_stone(px, py, r, rng, 0.8, 8);
            if (g_contains(dk, cand) && S_free(s, cand, 0.6)) { hole = cand; break; }
        }
    }
    if (hole) S_gcut(s, L1(hole), NF(0.7), "pothole");
    {
        GeoList *crk = LL(); double *ts; int nt = ly_linspace(0, 1, 9, &ts);
        nk = RI(1, 3);
        for (k = 0; k < nk; k++) {
            double x0, y0, tx, ty, *pts; int np2 = 0, ti;
            { double uu = U(-8, 8); lf_at(&f, uu, d, &x0, &y0); }
            if (k == 0 && hole) { Geo *c = g_centroid(hole); tx = g_x(c); ty = g_y(c); }
            else { double uu = U(KX_(-14), KX_(14)); lf_at(&f, uu, d - 30, &tx, &ty); }
            pts = (double *)LM(sizeof(double) * 2 * 10);
            pts[np2++] = x0; pts[np2++] = y0;
            for (ti = 1; ti < nt - 1; ti++) { double t = ts[ti], px = x0 + (tx - x0) * t + U(-0.8, 0.8), py = y0 + (ty - y0) * t + U(-0.8, 0.8); pts[np2++] = px; pts[np2++] = py; }
            pts[np2++] = tx; pts[np2++] = ty;
            LP(crk, g_linebuf(pts, np2 / 2, 0.22, SHP_CAP_ROUND, 4));
        }
        S_gcut(s, L1(g_inter(g_uu(crk), g_buf(road, 0.3))), NF(0.45), "crack");
    }
    {
        Geo *reg = hole ? g_diff(road, g_buf(hole, 0.3)) : road; int kk = RI(20, 40);
        S_stack(s, S_scatter(s, kk, 0.47, 0.62, reg, 0.35, 1, 0), NF(0.25), TG, 1, "grit", NONE, NULL);
    }
    return s;
}

/* gg2.crate(s, cx, cy, ang, w, l, h, z0=None): (got[0], zb + h) or 0 */
static int crate(Scene *s, double cx, double cy, double ang, double w, double l, double h, int has_z0, double z0, Geo **g0, double *ztop) {
    Frame f = lf_frame(cx, cy, ang); double uv[8] = { -l / 2, -w / 2, l / 2, -w / 2, l / 2, w / 2, -l / 2, w / 2 }, zb = has_z0 ? z0 : H; int64_t k, i;
    Geo *p = g_frpoly(&f, uv, 4); GeoList *got = S_prism(s, L1(p), NF(zb + h), "crate", has_z0 ? NF(z0 - 0.15) : NONE, SC.MINW, 1), *seams = LL();
    if (!got->n) return 0;
    k = ly_rint(l / 1.3); if (k < 2) k = 2;          /* max(2, int(round(l / 1.3))) */
    for (i = 1; i < k; i++) {
        double ax, ay, bx, by;
        lf_at(&f, -l / 2 + l * i / k, -w / 2 - 0.6, &ax, &ay); lf_at(&f, -l / 2 + l * i / k, w / 2 + 0.6, &bx, &by);
        LP(seams, g_linecut(ax, ay, bx, by, 0.35));
    }
    S_tcut(s, seams, NF(zb + h - 0.3), NF(zb + h + 0.5), "crack");
    *g0 = got->v[0]; *ztop = zb + h;
    return 1;
}

/* gg2.crater(s, cx, cy, r, depth=0.8) */
Geo *gg2_crater(Scene *s, double cx, double cy, double r, double depth) {
    PyRandom *rng = &s->rng; Geo *hole; GeoList *lip = LL(), *ends = LL(), *cracks = LL(), *all = LL(), *rocks = LL(), *ps; int k, nk; Geo *dk;
    static const Tier TR[] = { TI(0.65, 8), TI(0.35, 40) };
    { double asp = U(0.75, 0.95); hole = g_stone(cx, cy, r, rng, asp, 9); }
    nk = RI(6, 10);
    for (k = 0; k < nk; k++) { double a = U(0, 2 * PI), d = r + U(0.3, 0.9), rr = U(0.6, 1.0); LP(lip, g_stone(cx + d * COS(a), cy + d * SIN(a), rr, rng, 0.8, 5)); }
    nk = RI(3, 5);
    for (k = 0; k < nk; k++) {
        double a = U(0, 2 * PI), L = U(2.0, 3.8), p0x = cx + r * 0.8 * COS(a), p0y = cy + r * 0.8 * SIN(a), p1x, p1y, rr; Geo *ln;
        p1x = cx + (r + L) * COS(a + U(-0.2, 0.2)); p1y = cy + (r + L) * SIN(a + U(-0.2, 0.2));
        ln = g_wigline(p0x, p0y, p1x, p1y, rng, 0.25, 3);
        LP(cracks, g_bufx(ln, 0.22, 4, SHP_CAP_ROUND, SHP_JOIN_ROUND, 5.0));
        rr = U(0.55, 0.75); LP(ends, g_stone(p1x, p1y, rr, rng, 0.85, 5));
    }
    gl_extend(all, lip); gl_extend(all, ends);
    ps = g_polys(g_uu(all)); dk = disc_r(R0 - 0.3);
    for (k = 0; k < ps->n; k++) if (g_contains(dk, ps->v[k])) LP(rocks, ps->v[k]);
    { double h = U(0.6, 0.9); S_stack(s, rocks, NF(h), TR, 2, "rock", NONE, NULL); }
    S_gcut(s, L1(hole), NF(depth), "pit");
    S_gcut(s, L1(g_uu(cracks)), NF(0.4), "crack");
    return hole;
}

/* gg2.drill_holes(s, cx, cy, ang, rows, cols, z_top=None) */
static void drill_holes(Scene *s, double cx, double cy, double ang, int rows, int cols, int has_z, double z_top) {
    Frame f = lf_frame(cx, cy, ang); GeoList *holes = LL(); int i, j;
    for (i = 0; i < cols; i++) for (j = 0; j < rows; j++) LP(holes, ptf(&f, i * 1.8, j * 1.8, 0.5, 10));
    if (!has_z) S_gcut(s, holes, NF(0.9), "pit");
    else S_tcut(s, holes, NF(z_top - 0.9), NF(z_top + 0.5), "pit");
}

/* gg2.mine_v3(n) */
Scene *gg2_mine_v3(int n) {
    static const char *const TAB[] = { "crater ruts crates heap", "track crates bedrock", "bedrock crater ruts", "crater crater crates", "planks crates ruts crater",
        "track heap bedrock", "ruts ruts crates crater", "bedrock crater beam crates", "track crater crates", "heap ruts crates bedrock" };
    Scene *s = ly_mk("mine", "1800s Mine", n, "#5f4a38"); PyRandom *rng = &s->rng; const char *F[16]; int nF = ly_pickF(TAB, 1, 10, n, F, 16), i, k, nu;
    double phi = U(0, PI), *us; Frame f = lf_frame(0, 0, phi); GeoList *l;
    static const Tier TH[] = { TI(0.4, 10), TI(0.3, 25), TI(0.3, 48) }, TR[] = { TI(0.7, 6), TI(0.3, 40) }, TG[] = { TI(0.7, 10), TI(0.3, 45) };
    if (ly_in(F, nF, "bedrock")) {
        double a = U(0, 2 * PI), cx = 11.5 * COS(a), cy = 11.5 * SIN(a), h1, r; Geo *shelf; GeoList *b;
        r = U(5.5, 7.0); shelf = g_stone(cx, cy, r, rng, 0.6, 8);
        h1 = U(1.3, 1.8);
        b = PR(s, L1(shelf), H + h1, "bedrock");
        if (b->n) {
            Geo *core = g_inset(b->v[0], 0.5), *q, *m; GeoList *top2, *marks = LL(); int ng;
            q = g_inter(g_trans(g_scale(b->v[0], 0.7, 0.7, SHP_ORIGIN_CENTER, 0, 0), -1.2 * COS(a), -1.2 * SIN(a)), g_inset(b->v[0], 0.6));
            top2 = S_prism(s, g_polys(q), NF(H + h1 + 0.7), "bedrock", NF(H + h1 - 0.15), SC.MINW, 1);
            ng = RI(3, 5);
            for (i = 0; i < ng; i++) {
                double bb[4], px, py, aa; Frame ff; int nkk;
                g_bounds(core, bb);
                px = U(bb[0], bb[2]); py = U(bb[1], bb[3]); aa = U(0, PI); ff = lf_frame(px, py, aa);
                nkk = RI(3, 4);
                for (k = 0; k < nkk; k++) { double ax, ay, bx, by, jj; lf_at(&ff, -0.8, k * 0.9, &ax, &ay); jj = U(-0.15, 0.15); lf_at(&ff, 0.8, k * 0.9 + jj, &bx, &by); LP(marks, g_linecut(ax, ay, bx, by, 0.4)); }
            }
            m = g_inter(g_uu(marks), g_inset(b->v[0], 0.4));
            if (top2->n) m = g_diff(m, g_buf(top2->v[0], 0.3));
            S_tcut(s, L1(m), NF(H + h1 - 0.35), NF(H + h1 + 0.5), "crack");
            if (top2->n) {
                Geo *c = g_centroid(top2->v[0]);
                if (g_contains(g_buf(top2->v[0], -0.5), g_ptbuf(g_x(c), g_y(c), 2.4, 16))) drill_holes(s, g_x(c) - 0.9, g_y(c) - 0.9, a, 2, 2, 1, H + h1 + 0.7);
            }
        }
    }
    if (ly_in(F, nF, "track")) {
        double off = U(-4, 4), ue = U(-3, 4), g2 = 3.0; GeoList *tp = LL(), *tq = LL(); Geo *dk = disc_r(R0 - 0.2);
        PR(s, L1(g_R(&f, ue - 0.6, ue + 1.4, off - 4.8, off + 4.8)), H + 1.6, "wood");
        nu = ly_arange(ue + 3.0, KX_(18), 3.3, &us);
        for (i = 0; i < nu; i++) {
            double u = us[i], ang, ox, oy;
            if (!(RND() > 0.15)) continue;          /* the comprehension's filter runs first, then its element (the angle) */
            ang = U(-5, 5); lf_at(&f, u, off, &ox, &oy);
            LP(tp, g_rot(g_R(&f, u - 0.85, u + 0.85, off - 4.4, off + 4.4), ang, SHP_ORIGIN_POINT, ox, oy));
        }
        for (i = 0; i < tp->n; i++) if (g_contains(dk, tp->v[i])) LP(tq, tp->v[i]);
        PR(s, tq, H + 0.7, "wood");
        PR(s, L2(g_R(&f, ue + 0.3, KX_(20), off - g2 - 0.45, off - g2 + 0.45), g_R(&f, ue + 0.3, KX_(20), off + g2 - 0.45, off + g2 + 0.45)), H + 1.25, "iron");
    }
    if (ly_in(F, nF, "planks")) {
        double off = U(-8, -4); GeoList *fl = PR(s, L1(g_R(&f, KX_(-20), KX_(20), off, off + 6.6)), H + 0.6, "wood");
        if (fl->n) {
            GeoList *seams = LL(); int kk;
            for (kk = 1; kk <= 2; kk++) LP(seams, g_R(&f, KX_(-20), KX_(20), off + 2.2 * kk - 0.15, off + 2.2 * kk + 0.15));
            for (kk = 0; kk < 3; kk++) {
                double a0 = KX_(-18) + U(0, 6), st = U(7, 10);
                nu = ly_arange(a0, KX_(18), st, &us);
                for (i = 0; i < nu; i++) LP(seams, g_R(&f, us[i] - 0.15, us[i] + 0.15, off + 2.2 * kk + 0.1, off + 2.2 * (kk + 1) - 0.1));
            }
            S_tcut(s, L1(g_inter(g_uu(seams), g_inset(fl->v[0], 0.3))), NF(H + 0.25), NF(H + 0.6), "crack");
        }
    }
    if (ly_in(F, nF, "beam")) {
        double a = U(0, PI), d = U(-4, 4); Frame ff = lf_frame(d * COS(a + 1.57), d * SIN(a + 1.57), a); double uv[8] = { -6, -0.9, 6, -0.9, 6, 0.9, -6, 0.9 };
        Geo *bm = g_frpoly(&ff, uv, 4);
        if (S_free(s, bm, 0.5) && g_contains(disc_r(R0 - 0.3), bm)) {
            static const double V[2] = { -0.35, 0.35 }; GeoList *c2 = LL();
            PR(s, L1(bm), H + 1.2, "wood");
            for (i = 0; i < 2; i++) { double ax, ay, bx, by; lf_at(&ff, -5, V[i], &ax, &ay); lf_at(&ff, 5, V[i], &bx, &by); LP(c2, g_linecut(ax, ay, bx, by, 0.35)); }
            S_tcut(s, c2, NF(H + 0.9), NF(H + 1.7), "crack");
        }
    }
    for (k = ly_count(F, nF, "crater"); k > 0; k--) {
        int64_t t, m = KN_(80);
        for (t = 0; t < m; t++) {
            double r = U(1.6, 2.6), a = U(0, 2 * PI), d = U(0, 9), cx = d * COS(a), cy = d * SIN(a);
            if (S_free(s, g_ptbuf(cx, cy, r + 4.2, 16), 0.2)) { gg2_crater(s, cx, cy, r, d > 4 ? 0.8 : 0.7); break; }
        }
    }
    if (ly_in(F, nF, "heap")) {
        double a = U(0, 2 * PI), hx = 11 * COS(a), hy = 11 * SIN(a); GeoList *heap = LL(), *hq = LL(); int nh = RI(5, 8);
        for (i = 0; i < nh; i++) { double x = hx + U(-3, 3), y = hy + U(-3, 3), r = U(1.2, 2.2); LP(heap, g_stone(x, y, r, rng, 0.8, 6)); }
        for (i = 0; i < heap->n; i++) if (S_free(s, heap->v[i], 0.4)) LP(hq, heap->v[i]);
        if (hq->n) { double h = U(1.8, 2.6); S_stack(s, L1(g_inter(g_uu(hq), g_ptbuf(0, 0, R0 - 0.6, 128))), NF(h), TH, 3, "rock", NONE, NULL); }
    }
    {
        int nc = ly_count(F, nF, "crates") ? RI(1, 3) : 0;          /* range(F.count('crates') and rng.randint(1, 3)) */
        for (k = 0; k < nc; k++) {
            int64_t t, m = KN_(80);
            for (t = 0; t < m; t++) {
                double a = U(0, 2 * PI), d = U(3, 10), w = U(2.6, 3.4), ll = U(3.2, 4.4), cx = d * COS(a), cy = d * SIN(a), ang = U(0, PI); Geo *fp = g_ptbuf(cx, cy, py_hypot(w, ll) / 2, 16);
                if (S_free(s, fp, 0.3) && g_contains(disc_r(R0 - 0.5), fp)) {
                    double h = U(1.3, 1.6), zt; Geo *g0;
                    int c = crate(s, cx, cy, ang, w, ll, h, 0, 0, &g0, &zt);
                    if (c && k == 0 && RND() < 0.6) {
                        int i0 = s->tcuts.n - 1; double ox = cx + U(-0.3, 0.3), oy = cy + U(-0.3, 0.3), oa = ang + U(-0.4, 0.4), zt2; Geo *c2g;
                        if (crate(s, ox, oy, oa, w * 0.7, ll * 0.6, 1.2, 1, zt, &c2g, &zt2)) {
                            Tcut *tc; GeoList *keepl = LL(); Geo *cb = g_buf(c2g, 0.3); int pi;
                            if (i0 < 0) i0 += s->tcuts.n;          /* s.tcuts[-1] */
                            tc = &s->tcuts.v[i0];
                            for (pi = 0; pi < tc->polys.n; pi++) { GeoList *q = g_polys(g_diff(tc->polys.v[pi], cb)); int qi; for (qi = 0; qi < q->n; qi++) if (!g_isempty(g_buf(q->v[qi], -0.1))) LP(keepl, q->v[qi]); }
                            gl_clear(&tc->polys); gl_extend(&tc->polys, keepl);
                        }
                    }
                    break;
                }
            }
        }
    }
    for (k = ly_count(F, nF, "ruts"); k > 0; k--) {
        double a = U(0, PI), off = U(-6, 6), bend, *ls; Frame ff = lf_frame(0, 0, a); GeoList *ruts = LL(); int no, nl;
        bend = U(-0.03, 0.03);
        nl = ly_linspace(-18, 18, 24, &ls);
        for (no = 0; no < 2; no++) {
            double o = no ? 2.5 : -2.5, *pts = (double *)LM(sizeof(double) * 2 * (size_t)nl);
            for (i = 0; i < nl; i++) lf_at(&ff, ls[i], off + o + bend * ls[i] * ls[i], &pts[2 * i], &pts[2 * i + 1]);
            LP(ruts, g_linebuf(pts, nl, 0.3, SHP_CAP_FLAT, 16));
        }
        S_gcut(s, L1(g_uu(ruts)), NF(0.45), "rut");
    }
    l = S_scatter(s, RI(4, 8), 0.9, 1.6, NULL, 0.35, 0, 0);
    for (i = 0; i < l->n; i++) { double h = U(0.5, 0.9); S_stack(s, L1(l->v[i]), NF(h), TR, 2, "rock", NONE, NULL); }
    { int kk = RI(35, 55); S_stack(s, S_scatter(s, kk, 0.48, 0.72, NULL, 0.3, 1, 0), NF(0.35), TG, 2, "grit", NONE, NULL); }
    return s;
}

/* gg2.print_trail(s, phi, off, rng) */
static Geo *print_trail(Scene *s, double phi, double off, PyRandom *rng) {
    Frame f = lf_frame(0, 0, phi); GeoList *cuts = LL(); double *us; int nu = ly_arange(KX_(-16), KX_(16), 2.6, &us), i, t;
    static const double TT[3] = { -0.5, 0, 0.5 };
    (void)s; (void)rng;
    for (i = 0; i < nu; i++) {
        int side = i % 2 ? 1 : -1; double cx, cy, ax, ay, bx, by; Frame ff;
        lf_at(&f, us[i], off + side * 0.9, &cx, &cy); ff = lf_frame(cx, cy, phi);
        LP(cuts, ptf(&ff, 0, 0, 0.55, 10));
        for (t = 0; t < 3; t++) { lf_at(&ff, 0.4, 0, &ax, &ay); lf_at(&ff, 0.4 + 1.3 * COS(TT[t]), 1.3 * SIN(TT[t]), &bx, &by); LP(cuts, g_linecut(ax, ay, bx, by, 0.5)); }
    }
    return g_uu(cuts);
}

/* the rounded rock outlines of maahl / maahl_v3: unary_union of 4 discs, .buffer(0.8).buffer(-0.8).simplify(0.05) */
static Geo *sand_rock(PyRandom *rng, double cx, double cy, double r) {
    GeoList *l = LL(); int i;
    for (i = 0; i < 4; i++) { double x = cx + U(-r, r) * 0.6, y = cy + U(-r, r) * 0.6, rr = r * U(0.5, 0.8); LP(l, g_ptbuf(x, y, rr, 12)); }
    return g_simp(g_buf(g_buf(g_uu(l), 0.8), -0.8), 0.05);
}
static double rock_r(PyRandom *rng, const char *kind) {          /* {'outcrop': rng.uniform(3.0, 3.8), 'big': 3.6, 'small': 2.2}[kind]: the uniform is always drawn */
    double r = U(3.0, 3.8);
    return !strcmp(kind, "outcrop") ? r : !strcmp(kind, "big") ? 3.6 : 2.2;
}

/* gg2.maahl_v3(n) */
Scene *gg2_maahl_v3(int n) {
    static const char *const TAB[] = { "dune pits outcrop", "pavement outcrop2 trail", "hardpan dune pebbles", "pits pavement outcrop", "dune trail pebbles",
        "outcrop2 pits hardpan", "dune pavement pits", "trail outcrop pits", "hardpan pavement pebbles outcrop", "dune outcrop pits pebbles" };
    Scene *s = ly_mk("maahl", "Deserts of Maahl", n, "#c9a36c"); PyRandom *rng = &s->rng; const char *F[16]; int nF = ly_pickF(TAB, 1, 10, n, F, 16), i, k;
    double th = U(0, 2 * PI); const char *rocks[3]; int nr = 0;
    static const Tier TD[] = { TI(0.3, 12), TI(0.25, 18), TI(0.25, 26), TI(0.2, 40) }, TS[] = { TI(0.35, 8), TI(0.25, 20), TI(0.25, 38), TI(0.15, 60) },
        TP[] = { TI(0.6, 15), TI(0.4, 50) }, TV[] = { TI(0.75, 10), TI(0.25, 45) }, TB[] = { TI(0.7, 10), TI(0.3, 45) };
    if (ly_in(F, nF, "dune")) {
        double cx = 13 * COS(th), cy = 13 * SIN(th), r1 = U(7, 8.5), r2; Geo *a, *body;
        a = g_ptbuf(cx, cy, r1, 48); r2 = U(4.2, 4.8);
        body = g_diff(a, g_ptbuf(cx * 0.45, cy * 0.45, r2, 48));
        body = g_simp(g_buf(g_buf(body, -0.6), 0.6), 0.05);
        { double h = U(1.8, 2.4); S_stack(s, g_polys(body), NF(h), TD, 4, "dune", NONE, NULL); }
    }
    if (ly_in(F, nF, "outcrop")) rocks[nr++] = "outcrop";
    if (ly_in(F, nF, "outcrop2")) { rocks[nr++] = "big"; rocks[nr++] = "small"; }
    for (k = 0; k < nr; k++) {
        int64_t t, m = KN_(60);
        for (t = 0; t < m; t++) {
            double r = rock_r(rng, rocks[k]), a = th + PI + U(-1.4, 1.4), d = U(4, 9.5), cx = d * COS(a), cy = d * SIN(a); Geo *g = sand_rock(rng, cx, cy, r);
            if (S_free(s, g, 0.8) && g_contains(disc_r(R0 - 0.4), g)) { S_stack(s, L1(g), NF(strcmp(rocks[k], "small") ? 2.7 : 1.6), TS, 4, "sandstone", NONE, NULL); break; }
        }
    }
    if (ly_in(F, nF, "hardpan")) {
        int64_t t, m = KN_(60); Geo *reg = NULL; GeoList *pan;
        for (t = 0; t < m; t++) {
            double a = U(0, 2 * PI), d = U(2, 8), r = U(4, 5.5);
            reg = g_ptbuf(d * COS(a), d * SIN(a), r, 32);
            if (S_free(s, reg, 0.6) && g_contains(disc_r(R0 - 0.5), reg)) break;
        }
        if (!reg) ly_raise();          /* NameError when the loop never ran */
        pan = PR(s, L1(reg), H + 0.35, "pan");
        if (pan->n) {
            Geo *net = ly_vor_net(rng, KX_(-17), KX_(17), KA_(60), KX_(-20), KX_(20), 1, 0.25, 0.2, 4);
            S_tcut(s, L1(g_inter(net, g_buf(pan->v[0], 0.6))), NF(H + 0.05), NF(H + 0.8), "crack");
        }
    }
    if (ly_in(F, nF, "pebbles")) {
        int nc = RI(2, 3), c;
        for (c = 0; c < nc; c++) {
            double a = U(0, 2 * PI), d = U(1, 11); Geo *reg = g_ptbuf(d * COS(a), d * SIN(a), 2.2, 16); GeoList *peb;
            if (!S_free(s, reg, 0.4)) continue;
            peb = S_scatter(s, 9, 0.5, 0.75, reg, 0.2, 1, 2000);
            if (peb->n) S_stack(s, peb, NF(0.5), TP, 2, "pebble", NONE, NULL);
        }
    }
    if (ly_in(F, nF, "pavement")) {
        int64_t t, m = KN_(60);
        for (t = 0; t < m; t++) {
            double a = U(0, 2 * PI), d = U(2, 9), r = U(3.5, 4.8); Geo *reg = g_ptbuf(d * COS(a), d * SIN(a), r, 24);
            if (S_free(s, reg, 0.5) && g_contains(disc_r(R0 - 0.3), reg)) { S_stack(s, S_scatter(s, 40, 0.48, 0.7, reg, 0.18, 1, 4000), NF(0.3), TV, 2, "pebble", NONE, NULL); break; }
        }
    }
    if (ly_in(F, nF, "trail")) {
        double p1 = U(0, PI), p2 = U(-6, 6); Geo *tr = print_trail(s, p1, p2, rng);
        S_gcut(s, L1(tr), NF(0.4), "pit");
    }
    {
        GeoList *pits = LL(), *bl = LL(); int full = ly_in(F, nF, "pits"); int64_t t, m = KN_(3000); Geo *busy;
        gl_extend(bl, &s->feet);
        for (i = 0; i < s->gcuts.n; i++) gl_extend(bl, &s->gcuts.v[i].polys);
        for (i = 0; i < s->tcuts.n; i++) gl_extend(bl, &s->tcuts.v[i].polys);
        busy = g_buf(g_uu(bl), 0.5);
        for (t = 0; t < m; t++) {
            int want = full ? RI(55, 80) : RI(18, 28), mk2, ok; double a, d, r, sy; Geo *p;
            if (pits->n >= want) break;
            mk2 = ly_mark();
            a = U(0, 2 * PI); d = sqrt(RND()) * (R0 - 0.6);
            r = full ? U(0.45, 0.85) : U(0.45, 0.6);
            sy = U(0.75, 1.0);
            p = g_scale(g_ptbuf(d * COS(a), d * SIN(a), r, 10), 1.0, sy, SHP_ORIGIN_CENTER, 0, 0);
            ok = !g_intersects(p, busy);
            if (ok) for (i = 0; i < pits->n; i++) if (!(g_dist(p, pits->v[i]) > 0.45)) { ok = 0; break; }
            if (ok) LP(pits, p);
            ly_pop(mk2);
        }
        S_gcut(s, pits, NF(0.35), "pit");
    }
    { int kk = RI(10, 20); S_stack(s, S_scatter(s, kk, 0.5, 0.75, NULL, 0.4, 1, 0), NF(0.4), TB, 2, "pebble", NONE, NULL); }
    return s;
}

/* the voronoi cells of MultiPoint([(rng.uniform(lo, hi), rng.uniform(lo, hi)) for _ in range(n)]) in box(e0, e0, e1, e1) */
static GEOSGeometry *vor_cells(PyRandom *rng, double lo, double hi, int64_t n, double e0, double e1) {
    double *xy = (double *)LM(sizeof(double) * 2 * (size_t)(n + 1)); int64_t i; GEOSGeometry *pts, *env, *vd;
    for (i = 0; i < n; i++) { xy[2 * i] = U(lo, hi); xy[2 * i + 1] = U(lo, hi); }
    pts = shp_multipoint(xy, (int)n); env = shp_box(e0, e0, e1, e1, 1);
    vd = (pts && env) ? shp_voronoi(pts, env, 0.0, 0) : NULL;
    shp_free(pts); shp_free(env);
    LA(vd);
    return vd;
}

/* gg2.rock_floor(s, avoid, slope_dir, n_cells) (the levels it returns are not used by its caller) */
static void rock_floor(Scene *s, Geo *avoid, double slope_dir, int n_cells) {
    PyRandom *rng = &s->rng; GEOSGeometry *vd = vor_cells(rng, KX_(-17), KX_(17), KA_(n_cells), KX_(-20), KX_(20)); int nc = shp_num_geoms(vd), c, i, j;
    double lvs[64]; GeoList *lvl[64]; int nl = 0; Geo *dk = g_ptbuf(0, 0, R0 + 2.0, 64), *dk2 = g_ptbuf(0, 0, R0 - 0.45, 128);
    for (c = 0; c < nc; c++) {
        Geo *cell = LA(shp_clone(shp_geom_n(vd, c))); GeoList *gs = g_polys(g_inter(g_diff(g_inset(cell, 0.22), avoid), dk)); int gi;
        for (gi = 0; gi < gs->n; gi++) {
            Geo *g = gs->v[gi], *ce; double cx, cy, lv;
            if (g_area(g) < 3.0 || !g_wide(g, 1.6) || RND() < 0.2) continue;
            ce = g_centroid(g); cx = g_x(ce); cy = g_y(ce);
            lv = 0.75 + 0.45 * (cx * COS(slope_dir) + cy * SIN(slope_dir)) / 14 + U(-0.3, 0.3);
            lv = ly_rint(lv / 0.25) * 0.25;
            if (!(lv > 0.3)) lv = 0.3;          /* max(0.3, x) */
            if (!(lv < 1.5)) lv = 1.5;          /* min(1.5, y) */
            for (j = 0; j < nl; j++) if (lvs[j] == lv) break;
            if (j == nl) { if (nl >= 64) ly_raise(); lvs[nl] = lv; lvl[nl] = LL(); nl++; }
            LP(lvl[j], g_simp(g, 0.05));
        }
    }
    for (i = 0; i < nl; i++) for (j = i + 1; j < nl; j++) if (lvs[j] < lvs[i]) { double t = lvs[i]; GeoList *tl = lvl[i]; lvs[i] = lvs[j]; lvl[i] = lvl[j]; lvs[j] = t; lvl[j] = tl; }
    for (i = 0; i < nl; i++) {
        double lv = lvs[i]; GeoList *got = PR(s, lvl[i], H + lv, "bedrock"); int gi;
        for (gi = 0; gi < got->n; gi++) {
            Geo *g = got->v[gi]; double d = 0.35 * py_tan(LF_RADIANS(40)) + 0.03; Geo *q = g_fit(g_inset(g_inter(g, dk2), 0.15), d);
            if (SC.chamfer_rocks && SC.base_first) { S_tadd(s, g, NF(H), NF(H + lv), g_inset(g, 0.4)); continue; }
            if (q) {
                S_taper(s, L1(q), NF(ly_round(H + lv - 0.15, 4)), NF(0.35), NF(40.0), "bedrock");
                S_tadd(s, g, NF(H), NF(H + lv + 0.2), g_inset(q, 0.35 * py_tan(LF_RADIANS(40)) + 0.05));
            } else S_tadd(s, g, NF(H), NF(H + lv), g_inset(g, 0.05));
        }
    }
}

/* gg2.crater_t(s, cx, cy, r) */
static void crater_t(Scene *s, double cx, double cy, double r) {
    PyRandom *rng = &s->rng; Geo *hole; GeoList *lip = LL(), *cracks = LL(), *ps, *keep = LL(); int k, nk; Geo *dk = disc_r(R0 - 0.3);
    static const Tier TR[] = { TI(0.65, 8), TI(0.35, 40) };
    { double asp = U(0.75, 0.95); hole = g_stone(cx, cy, r, rng, asp, 9); }
    S_gcut(s, L1(hole), NF(py_hypot(cx, cy) > 4 ? 0.8 : 0.7), "pit");
    gl_push(&s->keepout, g_buf(hole, 0.2));
    nk = RI(7, 11);
    for (k = 0; k < nk; k++) { double a = U(0, 2 * PI), d = r + U(0.3, 0.9), rr = U(0.6, 1.0); LP(lip, g_stone(cx + d * COS(a), cy + d * SIN(a), rr, rng, 0.8, 5)); }
    nk = RI(3, 5);
    for (k = 0; k < nk; k++) {
        double a = U(0, 2 * PI), p0x = cx + r * 0.8 * COS(a), p0y = cy + r * 0.8 * SIN(a), p1x = cx + (r + 1.5) * COS(a), p1y = cy + (r + 1.5) * SIN(a);
        LP(cracks, g_bufx(g_wigline(p0x, p0y, p1x, p1y, rng, 0.2, 3), 0.22, 4, SHP_CAP_ROUND, SHP_JOIN_ROUND, 5.0));
    }
    ps = g_polys(g_uu(lip));
    for (k = 0; k < ps->n; k++) if (g_contains(dk, ps->v[k])) LP(keep, ps->v[k]);
    S_tstack(s, keep, NF(0.75), TR, 2, "rock");
    S_gcut(s, L1(g_uu(cracks)), NF(0.4), "crack");
}

/* gg2.mine(n) */
Scene *gg2_mine(int n) {
    static const char *const TAB[] = { "crater ruts crates heap", "track crates", "crater ruts marks", "crater crater crates", "planks crates ruts crater",
        "track heap marks", "ruts ruts crates crater", "crater beam crates marks", "track crater crates", "heap ruts crates marks" };
    Scene *s = ly_mk("mine", "1800s Mine", n, "#5f4a38"); PyRandom *rng = &s->rng; const char *F[16]; int nF, i, k, nu;
    double phi, off, bend = 0, *us, *ls; int nl; Frame f; Geo *lane = NULL, *avoid; GeoList *craters = LL(); double cr_r[64]; int ncr = 0;
    static const Tier TH[] = { TI(0.4, 10), TI(0.3, 25), TI(0.3, 48) }, TR[] = { TI(0.7, 6), TI(0.3, 40) }, TG[] = { TI(0.7, 10), TI(0.3, 45) };
    T_init(s);
    nF = ly_pickF(TAB, 1, 10, n, F, 16);
    phi = U(0, PI); f = lf_frame(0, 0, phi); off = U(-5, 5);
    nl = ly_linspace(-19, 19, 30, &ls);
    if (ly_in(F, nF, "track") || ly_in(F, nF, "ruts") || ly_in(F, nF, "planks")) {
        double lw = ly_in(F, nF, "track") ? 9.6 : 7.6, *pts = (double *)LM(sizeof(double) * 2 * (size_t)nl);
        bend = U(-0.025, 0.025);
        for (i = 0; i < nl; i++) lf_at(&f, ls[i], off + bend * ls[i] * ls[i], &pts[2 * i], &pts[2 * i + 1]);
        lane = g_linebuf(pts, nl, lw / 2, SHP_CAP_FLAT, 16);
        if (ly_count(F, nF, "ruts") == 2) {
            double *p2 = (double *)LM(sizeof(double) * 2 * (size_t)nl);
            for (i = 0; i < nl; i++) lf_at(&f, ls[i], off - 6 + 0.02 * ls[i] * ls[i], &p2[2 * i], &p2[2 * i + 1]);
            lane = g_uu2(lane, g_linebuf(p2, nl, 3.6, SHP_CAP_FLAT, 16));
        }
    }
    for (k = ly_count(F, nF, "crater"); k > 0; k--) {
        int64_t t, m = KN_(80);
        for (t = 0; t < m; t++) {
            double r = U(1.8, 2.6), a = U(0, 2 * PI), d = U(0, 9.5); Geo *c = g_pt(d * COS(a), d * SIN(a)); int ok = !lane || g_dist(c, lane) > r + 1.0, q;
            if (ok) for (q = 0; q < craters->n; q++) if (!(g_dist(c, craters->v[q]) > r + cr_r[q] + 2.5)) { ok = 0; break; }
            if (ok) { if (ncr >= 64) ly_raise(); LP(craters, c); cr_r[ncr++] = r; break; }
        }
    }
    if (lane || ncr) {
        GeoList *l = LL();
        if (lane) LP(l, g_buf(lane, 0.3));
        for (i = 0; i < ncr; i++) LP(l, g_buf(craters->v[i], cr_r[i] + 1.6));
        avoid = g_uu(l);
    } else avoid = g_empty();
    { double sd = U(0, 2 * PI); int nc = RI(26, 36); rock_floor(s, avoid, sd, nc); }
    if (lane) {
        if (ly_in(F, nF, "track")) {
            double ue = U(-3, 4), g2 = 3.0; GeoList *tp = LL(), *tq = LL(); Geo *dk = disc_r(R0 - 0.2);
            S_extend_objs(s, PR(s, L1(g_R(&f, ue - 0.6, ue + 1.4, off - 4.4, off + 4.4)), H + 1.6, "wood"));
            nu = ly_arange(ue + 3.0, KX_(18), 3.3, &us);
            for (i = 0; i < nu; i++) {
                double u = us[i], ang, ox, oy;
                if (!(RND() > 0.15)) continue;          /* the comprehension's filter runs first, then its element */
                ang = U(-5, 5); lf_at(&f, u, off, &ox, &oy);
                LP(tp, g_rot(g_R(&f, u - 0.85, u + 0.85, off - 4.2, off + 4.2), ang, SHP_ORIGIN_POINT, ox, oy));
            }
            for (i = 0; i < tp->n; i++) if (g_contains(dk, tp->v[i])) LP(tq, tp->v[i]);
            S_extend_objs(s, PR(s, tq, H + 0.7, "wood"));
            S_extend_objs(s, PR(s, L2(g_R(&f, ue + 0.3, KX_(20), off - g2 - 0.45, off - g2 + 0.45), g_R(&f, ue + 0.3, KX_(20), off + g2 - 0.45, off + g2 + 0.45)), H + 1.25, "iron"));
        }
        if (ly_in(F, nF, "planks")) {
            GeoList *fl = PR(s, L1(g_inter(g_R(&f, KX_(-20), KX_(20), off - 3.3, off + 3.3), g_buf(lane, -0.4))), H + 0.6, "wood");
            if (fl->n) {
                GeoList *seams = LL(); int kk;
                for (kk = 1; kk <= 2; kk++) LP(seams, g_R(&f, KX_(-20), KX_(20), off - 3.3 + 2.2 * kk - 0.15, off - 3.3 + 2.2 * kk + 0.15));
                for (kk = 0; kk < 3; kk++) {
                    double a0 = KX_(-18) + U(0, 6), st = U(7, 10);
                    nu = ly_arange(a0, KX_(18), st, &us);
                    for (i = 0; i < nu; i++) LP(seams, g_R(&f, us[i] - 0.15, us[i] + 0.15, off - 3.3 + 2.2 * kk + 0.1, off - 3.3 + 2.2 * (kk + 1) - 0.1));
                }
                S_tcut(s, L1(g_inter(g_uu(seams), g_inset(fl->v[0], 0.3))), NF(H + 0.25), NF(H + 0.6), "crack");
                S_extend_objs(s, fl);
            }
        }
        for (k = 0; k < ly_count(F, nF, "ruts"); k++) {
            double o2 = k == 0 ? off : off - 6, bb = k == 0 ? bend : 0.02; GeoList *ruts = LL(); int no;
            for (no = 0; no < 2; no++) {
                double o = no ? 2.5 : -2.5, *pts = (double *)LM(sizeof(double) * 2 * (size_t)nl);
                for (i = 0; i < nl; i++) lf_at(&f, ls[i], o2 + o + bb * ls[i] * ls[i], &pts[2 * i], &pts[2 * i + 1]);
                LP(ruts, g_linebuf(pts, nl, 0.3, SHP_CAP_FLAT, 16));
            }
            S_gcut(s, L1(g_inter(g_uu(ruts), g_buf(lane, -0.2))), NF(0.45), "rut");
        }
    }
    for (i = 0; i < ncr; i++) crater_t(s, g_x(craters->v[i]), g_y(craters->v[i]), cr_r[i]);
    if (ly_in(F, nF, "beam")) {
        int64_t t, m = KN_(40);
        for (t = 0; t < m; t++) {
            double a = U(0, PI), d = U(-6, 6); Frame ff = lf_frame(d * COS(a + 1.57), d * SIN(a + 1.57), a); double uv[8] = { -5.5, -0.9, 5.5, -0.9, 5.5, 0.9, -5.5, 0.9 };
            Geo *bm = g_frpoly(&ff, uv, 4);
            if (S_tfree(s, bm, 0.5) && g_contains(disc_r(R0 - 0.4), bm)) {
                Num *tops; GeoList *got = S_tprism(s, L1(bm), NF(1.0), "wood", &tops);
                if (got->n) {
                    static const double V[2] = { -0.35, 0.35 }; GeoList *c2 = LL();
                    for (i = 0; i < 2; i++) { double ax, ay, bx, by; lf_at(&ff, -5, V[i], &ax, &ay); lf_at(&ff, 5, V[i], &bx, &by); LP(c2, g_linecut(ax, ay, bx, by, 0.35)); }
                    S_tcut(s, c2, num_sub(tops[0], NF(0.3)), num_add(tops[0], NF(0.5)), "crack");
                }
                break;
            }
        }
    }
    if (ly_in(F, nF, "heap")) {
        double a = U(0, 2 * PI), hx = 11 * COS(a), hy = 11 * SIN(a); GeoList *heap = LL(), *hq = LL(); int nh = RI(5, 8);
        for (i = 0; i < nh; i++) { double x = hx + U(-3, 3), y = hy + U(-3, 3), r = U(1.2, 2.2); LP(heap, g_stone(x, y, r, rng, 0.8, 6)); }
        for (i = 0; i < heap->n; i++) if (S_tfree(s, heap->v[i], 0.4)) LP(hq, heap->v[i]);
        if (hq->n) S_tstack(s, L1(g_inter(g_uu(hq), g_ptbuf(0, 0, R0 - 0.6, 128))), NF(2.0), TH, 3, "rock");
    }
    {
        int nc = ly_count(F, nF, "crates") ? RI(1, 3) : 0;
        for (k = 0; k < nc; k++) {
            int64_t t, m = KN_(80);
            for (t = 0; t < m; t++) {
                double a = U(0, 2 * PI), d = U(2, 10), w = U(2.6, 3.4), ll = U(3.2, 4.4), cx = d * COS(a), cy = d * SIN(a), ang = U(0, PI);
                Frame ff = lf_frame(cx, cy, ang); double uv[8] = { -ll / 2, -w / 2, ll / 2, -w / 2, ll / 2, w / 2, -ll / 2, w / 2 }; Geo *fp = g_frpoly(&ff, uv, 4); Num lo, hi;
                S_tzr(s, fp, &lo, &hi);
                if (S_tfree(s, fp, 0.3) && g_contains(disc_r(R0 - 0.5), fp) && hi.v - lo.v <= 0.8 && hi.v <= H + 1.2) {
                    double h = U(1.3, 1.6); Num *tops; GeoList *got = S_tprism(s, L1(fp), NF(h), "crate", &tops);
                    if (got->n) {
                        int64_t kk = ly_rint(ll / 1.3), ii; GeoList *sm = LL(); Num top = tops[0];
                        if (kk < 2) kk = 2;
                        for (ii = 1; ii < kk; ii++) { double ax, ay, bx, by; lf_at(&ff, -ll / 2 + ll * ii / kk, -w / 2 - 0.6, &ax, &ay); lf_at(&ff, -ll / 2 + ll * ii / kk, w / 2 + 0.6, &bx, &by); LP(sm, g_linecut(ax, ay, bx, by, 0.35)); }
                        S_tcut(s, sm, num_sub(top, NF(0.3)), num_add(top, NF(0.5)), "crack");
                    }
                    break;
                }
            }
        }
    }
    { int kk = RI(4, 8); S_tstack(s, S_scatter_any(s, kk, 0.9, 1.6, NULL, 0.35, 0, 0), NF(0.7), TR, 2, "rock"); }
    { int kk = RI(35, 55); S_tstack(s, S_scatter_any(s, kk, 0.48, 0.72, NULL, 0.3, 1, 0), NF(0.35), TG, 2, "grit"); }
    if (ly_in(F, nF, "marks")) {
        int ne = 0, *idx; double *keys; int64_t *ord; GeoList *marks = LL(), *holes = LL(); int ti;
        idx = (int *)LM(sizeof(int) * (size_t)(s->terr.n + 1)); keys = (double *)LM(sizeof(double) * (size_t)(s->terr.n + 1)); ord = (int64_t *)LM(sizeof(int64_t) * (size_t)(s->terr.n + 1));
        for (i = 0; i < s->terr.n; i++) if (s->terr.v[i].zt.v > H + 1.0) { idx[ne] = i; keys[ne] = -shp_area(s->terr.v[i].top->g); ne++; }
        if (px_sorted_idx(keys, ne, 1, ord) != 0) ly_raise();
        for (ti = 0; ti < ne && ti < 3; ti++) {
            Terr e = s->terr.v[idx[ord[ti]]]; Geo *top = e.top, *core = g_inset(top, 0.3), *c; double b[4], px, py, aa; Frame ff; int i2, j2;
            if (g_isempty(core)) continue;
            g_bounds(core, b); px = U(b[0], b[2]); py = U(b[1], b[3]); aa = U(0, PI); ff = lf_frame(px, py, aa);
            for (k = 0; k < 3; k++) {
                double ax, ay, bx, by, jj; Geo *mm;
                lf_at(&ff, -0.8, k * 0.9, &ax, &ay); jj = U(-0.15, 0.15); lf_at(&ff, 0.8, k * 0.9 + jj, &bx, &by);
                mm = g_linecut(ax, ay, bx, by, 0.4);
                if (g_contains(core, mm)) LP(marks, mm);
            }
            c = g_centroid(top);
            for (i2 = 0; i2 < 2; i2++) for (j2 = 0; j2 < 2; j2++) {
                Geo *hh = g_ptbuf(g_x(c) + (i2 - 0.5) * 1.8, g_y(c) + (j2 - 0.5) * 1.8, 0.5, 10); int ok = g_contains(core, hh), mi;
                if (ok) for (mi = 0; mi < marks->n; mi++) if (g_intersects(hh, marks->v[mi])) { ok = 0; break; }
                if (ok) LP(holes, hh);
            }
        }
        S_tcutflat(s, marks, NF(0.35), "crack");
        S_tcutflat(s, holes, NF(0.9), "pit");
    }
    return s;
}

/* gg2.swells(s, blobs, dz=0.2, step=0.55): blobs (polygon, n) */
static void swells(Scene *s, GeoList *bl, const int *nts, double dz, double step) {
    GeoList *hulls = LL(), *cur = LL(), *ps; int nmax, i, k, m; double Ht, D; Geo *g;
    if (!bl->n) return;
    for (i = 0; i < bl->n; i++) LP(hulls, g_simp(g_bufq(g_hull(bl->v[i]), 0.5, 16), 0.03));
    nmax = nts[0]; for (i = 1; i < bl->n; i++) if (nts[i] > nmax) nmax = nts[i];
    Ht = nmax * dz; D = nmax * step;
    if (SC.mesh) m = 14; else { int64_t r = ly_rint(nmax / 1.5); m = (int)(r < 5 ? r : 5); if (m < 3) m = 3; }
    g = g_inter(g_uu(hulls), g_ptbuf(0, 0, R0 + 0.15, 256));
    ps = g_polys(g);
    for (i = 0; i < ps->n; i++) if (g_area(ps->v[i]) > 1.0 && g_wide(ps->v[i], 1.2)) LP(cur, g_simp(ps->v[i], 0.02));
    for (k = 0; k < m; k++) {
        double z0, z1, dd, hh, ang, sink, u0 = (double)k / m, u1 = (double)(k + 1) / m; GeoList *ok = LL(), *tops = LL(), *nc = LL();
        if (!cur->n) break;
        z0 = Ht * (1 - ly_pow(1 - u0, 1.8)); z1 = Ht * (1 - ly_pow(1 - u1, 1.8));
        dd = D / m; hh = z1 - z0;
        if (hh < (SC.mesh ? 0.02 : 0.1)) break;
        ang = LF_DEGREES(py_atan(dd / hh));
        for (i = 0; i < cur->n; i++) { Geo *q = g_fit(cur->v[i], dd + 0.03); if (q) LP(ok, q); }
        if (!ok->n) break;
        sink = k == 0 ? SINK : SC.LAP;
        S_taper(s, ok, NF(ly_round(H + z0 - sink, 4)), NF(ly_round(hh + sink, 4)), NF(ly_round(ang, 3)), "dune");
        for (i = 0; i < ok->n; i++) gl_extend(tops, g_polys(g_inset(ok->v[i], dd)));
        S_tadd(s, g_uu(ok), NF(H + z0), NF(H + z1), tops->n ? g_uu(tops) : g_empty());
        if (k == 0) S_extend_feet(s, ok);
        for (i = 0; i < ok->n; i++) { GeoList *q = g_polys(g_inset(ok->v[i], dd + SC.LEDGE)); int j; for (j = 0; j < q->n; j++) if (g_area(q->v[j]) > 1.0 && g_wide(q->v[j], 1.2)) LP(nc, q->v[j]); }
        cur = nc;
    }
}

/* gg2.hollow(s, poly) */
static void hollow(Scene *s, Geo *poly) {
    static const double DP[3] = { 0.2, 0.4, 0.6 }; int k;
    for (k = 0; k < 3; k++) { Geo *g = g_inset(poly, k * 0.6); if (!g_isempty(g) && g_wide(g, 1.2)) S_gcut(s, L1(g), NF(DP[k]), "hollow"); }
    gl_push(&s->keepout, g_buf(poly, 0.3));
}

static int any_keepout(Scene *s, Geo *g) { int i; for (i = 0; i < s->keepout.n; i++) if (g_intersects(g, s->keepout.v[i])) return 1; return 0; }
/* max(polys_of(g), key=area) if polys_of(g) else None */
static Geo *biggest(Geo *g) { GeoList *p = g_polys(g); return p->n ? g_max_area(p) : NULL; }

/* gg2.maahl(n) */
Scene *gg2_maahl(int n) {
    static const char *const TAB[] = { "dune pits outcrop", "pavement outcrop2 trail", "hardpan dune pebbles", "pits pavement outcrop hollow", "dune trail pebbles",
        "outcrop2 pits hardpan", "dune pavement pits hollow", "trail outcrop pits", "hardpan pavement pebbles outcrop hollow", "dune outcrop pits pebbles" };
    Scene *s = ly_mk("maahl", "Deserts of Maahl", n, "#c9a36c"); PyRandom *rng = &s->rng; const char *F[16]; int nF, i, k, nk;
    double th, tphi = 0, toff = 0; Geo *band = NULL, *hol = NULL; GeoList *blobs = LL(); int nts[16], nb = 0; const char *rocks[3]; int nr = 0;
    static const Tier TS[] = { TI(0.35, 8), TI(0.25, 20), TI(0.25, 38), TI(0.15, 60) }, TP[] = { TI(0.6, 15), TI(0.4, 50) }, TV[] = { TI(0.75, 10), TI(0.25, 45) }, TB[] = { TI(0.7, 10), TI(0.3, 45) };
    T_init(s);
    nF = ly_pickF(TAB, 1, 10, n, F, 16);
    th = U(0, 2 * PI);
    if (ly_in(F, nF, "trail")) {
        Frame ff; double xy[4];
        tphi = U(0, PI); toff = U(-6, 6); ff = lf_frame(0, 0, tphi);
        lf_at(&ff, -20, toff, &xy[0], &xy[1]); lf_at(&ff, 20, toff, &xy[2], &xy[3]);
        band = g_linebuf(xy, 2, 2.4, SHP_CAP_FLAT, 16);
    }
    if (ly_in(F, nF, "hollow")) {
        int64_t t, m = KN_(60);
        for (t = 0; t < m; t++) {
            double a = U(0, 2 * PI), d = U(1, 7), r = U(3.2, 4.2), sy; Geo *g;
            g = g_ptbuf(d * COS(a), d * SIN(a), r, 32); sy = U(0.7, 1.0);
            g = g_scale(g, 1.0, sy, SHP_ORIGIN_CENTER, 0, 0);
            if (!band || !g_intersects(g, g_buf(band, 0.5))) { hol = g; break; }
        }
    }
    nk = RI(2, 4);
    for (k = 0; k < nk; k++) {
        double a = th + k * 2 * PI / 3 + U(-0.5, 0.5), d = U(5, 13), cx = d * COS(a), cy = d * SIN(a); GeoList *l = LL(); Geo *b;
        for (i = 0; i < 3; i++) { double x = cx + U(-3, 3), y = cy + U(-3, 3), r = U(3.5, 6.0); LP(l, g_ptbuf(x, y, r, 32)); }
        b = g_buf(g_buf(g_uu(l), 1.0), -1.0);
        if (band) b = g_diff(b, g_buf(band, 1.0));
        if (hol) b = g_diff(b, g_buf(hol, 1.0));
        b = biggest(b);
        if (b && g_area(b) > 12) { Geo *bs = g_simp(b, 0.05); LP(blobs, bs); nts[nb++] = RI(4, 8); }
    }
    if (ly_in(F, nF, "dune")) {
        static const double TT[3] = { -0.45, 0, 0.45 }; int t;
        for (t = 0; t < 3; t++) {
            double a2 = th + PI + TT[t], cx = 11.5 * COS(a2), cy = 11.5 * SIN(a2), r = U(3.8, 4.6); Geo *g;
            g = g_rot(g_scale(g_ptbuf(cx, cy, r, 32), 1.0, 0.65, SHP_ORIGIN_CENTER, 0, 0), LF_DEGREES(a2) + 90, SHP_ORIGIN_POINT, cx, cy);
            if (band) g = g_diff(g, g_buf(band, 1.0));
            if (hol) g = g_diff(g, g_buf(hol, 1.0));
            g = biggest(g);
            if (g && g_area(g) > 8) { LP(blobs, g); nts[nb++] = RI(8, 10); }
        }
    }
    if (blobs->n) swells(s, blobs, nts, 0.2, 0.55);
    if (hol) hollow(s, hol);
    if (band) gl_push(&s->keepout, band);
    if (ly_in(F, nF, "outcrop")) rocks[nr++] = "outcrop";
    if (ly_in(F, nF, "outcrop2")) { rocks[nr++] = "big"; rocks[nr++] = "small"; }
    for (k = 0; k < nr; k++) {
        int64_t t, m = KN_(80);
        for (t = 0; t < m; t++) {
            double r = rock_r(rng, rocks[k]), a = U(0, 2 * PI), d = U(3, 9.5), cx = d * COS(a), cy = d * SIN(a); Geo *g = sand_rock(rng, cx, cy, r); Num lo, hi;
            S_tzr(s, g, &lo, &hi);
            if (S_tfree(s, g, 0.8) && !any_keepout(s, g) && g_contains(disc_r(R0 - 0.4), g) && hi.v - lo.v < 1.0) {
                S_tstack(s, L1(g), NF(strcmp(rocks[k], "small") ? 2.6 : 1.5), TS, 4, "sandstone"); break;
            }
        }
    }
    if (ly_in(F, nF, "hardpan")) {
        int64_t t, m = KN_(100);
        for (t = 0; t < m; t++) {
            double a = U(0, 2 * PI), d = U(1, 9), r = U(3.2, 4.6); Geo *reg = g_ptbuf(d * COS(a), d * SIN(a), r, 32); Num z = S_tflat(s, reg);
            if (z.t != NUM_NONE && S_tfree(s, reg, 0.6) && g_contains(disc_r(R0 - 0.5), reg)) {
                GeoList *pan = S_prism(s, L1(reg), num_add(z, NF(0.35)), "pan", num_sub(z, NF(SINK)), SC.MINW, 0);
                S_extend_feet(s, pan); S_extend_objs(s, pan);
                if (pan->n) {
                    Geo *net = ly_vor_net(rng, KX_(-17), KX_(17), KA_(60), KX_(-20), KX_(20), 1, 0.25, 0.2, 4);
                    S_tcut(s, L1(g_inter(net, g_buf(pan->v[0], 0.6))), num_add(z, NF(0.05)), num_add(z, NF(0.8)), "crack");
                }
                break;
            }
        }
    }
    if (ly_in(F, nF, "pebbles")) {
        int nc = RI(2, 3), c;
        for (c = 0; c < nc; c++) {
            double a = U(0, 2 * PI), d = U(1, 11); Geo *reg = g_ptbuf(d * COS(a), d * SIN(a), 2.2, 16);
            if (!S_tfree(s, reg, 0.4) || any_keepout(s, reg)) continue;
            S_tstack(s, S_scatter_any(s, 9, 0.5, 0.75, reg, 0.2, 1, 2000), NF(0.5), TP, 2, "pebble");
        }
    }
    if (ly_in(F, nF, "pavement")) {
        int64_t t, m = KN_(60);
        for (t = 0; t < m; t++) {
            double a = U(0, 2 * PI), d = U(2, 9), r = U(3.5, 4.8); Geo *reg = g_ptbuf(d * COS(a), d * SIN(a), r, 24);
            if (S_tfree(s, reg, 0.5) && !any_keepout(s, reg) && g_contains(disc_r(R0 - 0.3), reg)) {
                S_tstack(s, S_scatter_any(s, 40, 0.48, 0.7, reg, 0.18, 1, 4000), NF(0.3), TV, 2, "pebble"); break;
            }
        }
    }
    { int kk = RI(10, 20); S_tstack(s, S_scatter_any(s, kk, 0.5, 0.75, NULL, 0.4, 1, 0), NF(0.4), TB, 2, "pebble"); }
    if (band) {
        int ix = gl_index(&s->keepout, band);          /* s.keepout.remove(band): the first item that is band */
        if (ix < 0) ly_raise();
        geo_unref(s->keepout.v[ix]); memmove(&s->keepout.v[ix], &s->keepout.v[ix + 1], sizeof(Geo *) * (size_t)(s->keepout.n - ix - 1)); s->keepout.n--;
        S_gcut(s, L1(print_trail(s, tphi, toff, rng)), NF(0.4), "pit");
    }
    {
        GeoList *pits = LL(); int full = ly_in(F, nF, "pits"); int64_t t, m = KN_(4000);
        for (t = 0; t < m; t++) {
            int want = full ? RI(55, 80) : RI(18, 28), mk2, ok; double a, d, r, sy; Geo *p;
            if (pits->n >= want) break;
            mk2 = ly_mark();
            a = U(0, 2 * PI); d = sqrt(RND()) * (R0 - 0.6);
            r = full ? U(0.45, 0.85) : U(0.45, 0.6);
            sy = U(0.75, 1.0);
            p = g_scale(g_ptbuf(d * COS(a), d * SIN(a), r, 10), 1.0, sy, SHP_ORIGIN_CENTER, 0, 0);
            ok = 1;
            for (i = 0; i < pits->n; i++) if (!(g_dist(p, pits->v[i]) > 0.45)) { ok = 0; break; }
            if (ok && band && g_intersects(p, band)) ok = 0;
            if (ok) LP(pits, p);
            ly_pop(mk2);
        }
        S_tcutflat(s, pits, NF(0.35), "pit");
    }
    return s;
}

/* the sidewalk joints of urban_a / urban_x (pv = n % 3; w for pv 1 / urban_x: 3.2) -> unary_union([g, x]) */
static Geo *walk_joints(const Frame *f, double phi, double d, int pv, double w1) {
    GeoList *a = LL(), *b = LL(); double *us; int nu, i, k; Geo *g, *x;
    if (pv == 2) {
        nu = ly_arange(KX_(-20), KX_(20), 3.6, &us);
        for (i = 0; i < nu; i++) LP(a, g_box(KX_(-20), us[i] - 0.22, KX_(20), us[i] + 0.22));
        g = g_rot(g_uu(a), LF_DEGREES(phi) + 45, SHP_ORIGIN_POINT, 0, 0);
        for (i = 0; i < nu; i++) LP(b, g_box(us[i] - 0.22, KX_(-20), us[i] + 0.22, KX_(20)));
        x = g_rot(g_uu(b), LF_DEGREES(phi) + 45, SHP_ORIGIN_POINT, 0, 0);
    } else if (pv == 0) {
        double L = 3.2, w = 1.55; int64_t j, ii;
        for (j = KN_(1); j < KN_(14); j++) LP(a, g_R(f, KX_(-20), KX_(20), d + 2.3 + j * w - 0.22, d + 2.3 + j * w + 0.22));
        g = g_uu(a);
        for (j = KN_(0); j < KN_(14); j++) for (ii = KN_(-8); ii < KN_(9); ii++)
            LP(b, g_R(f, ii * L + ly_mod(j, 2) * L / 2 - 0.22, ii * L + ly_mod(j, 2) * L / 2 + 0.22, d + 2.3 + j * w + 0.1, d + 2.3 + (j + 1) * w - 0.1));
        x = g_uu(b);
    } else {
        double w = w1; int64_t j;
        nu = ly_arange(KX_(-20), KX_(20), w, &us);
        for (i = 0; i < nu; i++) LP(a, g_R(f, us[i] - 0.22, us[i] + 0.22, KX_(-20), KX_(20)));
        g = g_uu(a);
        for (k = 0; k < nu; k++) for (j = KN_(-1); j < KN_(6); j++)
            LP(b, g_R(f, us[k] + 0.1, us[k] + w - 0.1, d + 2.3 + j * 3.2 + ly_mod(k, 2) * 1.6 - 0.22, d + 2.3 + j * 3.2 + ly_mod(k, 2) * 1.6 + 0.22));
        x = g_uu(b);
    }
    return g_uu2(g, x);
}

/* urban_x's on_walk(w, l): (u, v, c) or 0 */
static int on_walk(PyRandom *rng, const Frame *f, double d, Geo *sl, GeoList *covers, double w, double l, double *uo, double *vo, Geo **co) {
    int64_t t, m = KN_(80);
    for (t = 0; t < m; t++) {
        double u = U(-9, 9), v = d + 2.3 + U(w / 2 + 0.6, 9); Geo *c = g_R(f, u - l / 2, u + l / 2, v - w / 2, v + w / 2); int ok, i;
        ok = g_contains(g_buf(sl, -0.6), c) && g_contains(disc_r(R0 - 0.8), c);
        if (ok) for (i = 0; i < covers->n; i++) if (g_intersects(g_buf(c, 0.6), covers->v[i])) { ok = 0; break; }
        if (ok) { *uo = u; *vo = v; *co = c; return 1; }
    }
    return 0;
}
/* urban_x's road_free(p, gap=0.6) */
static int road_free(Scene *s, Geo *road, GeoList *busy, Geo *p, double gap) {
    int i;
    if (!g_contains(g_buf(road, -0.4), p) || !g_contains(disc_r(R0 - 0.8), p)) return 0;
    for (i = 0; i < busy->n; i++) if (g_intersects(g_buf(p, gap), busy->v[i])) return 0;
    return S_free(s, p, gap);
}
/* the road cracks of urban_a / urban_x: pts from (x0, y0) to (tx, ty) through the 7 inner points of linspace(0, 1, 9) */
static Geo *crack_line(PyRandom *rng, double x0, double y0, double tx, double ty) {
    double *ts, *pts = (double *)LM(sizeof(double) * 20); int nt = ly_linspace(0, 1, 9, &ts), np2 = 0, ti;
    pts[np2++] = x0; pts[np2++] = y0;
    for (ti = 1; ti < nt - 1; ti++) { double t = ts[ti], px = x0 + (tx - x0) * t + U(-0.8, 0.8), py = y0 + (ty - y0) * t + U(-0.8, 0.8); pts[np2++] = px; pts[np2++] = py; }
    pts[np2++] = tx; pts[np2++] = ty;
    return g_linebuf(pts, np2 / 2, 0.22, SHP_CAP_ROUND, 4);
}

/* gg2.urban_x(n) */
static Scene *urban_x(int n) {
    static const char *const TAB[] = { "manhole crosswalk treegrate", "crater brokencurb utility", "tram gutter treegrate", "cobble manhole utility", "crater crosswalk drain",
        "tram repair brokencurb", "cobble crater treegrate", "manhole gutter repair utility", "crosswalk drain brokencurb", "tram crater utility", "manhole repair treegrate",
        "crater gutter utility", "cobble crosswalk brokencurb" };
    Scene *s = ly_mk("urban", "Urban Warfare", n, "#7e7b76"); PyRandom *rng = &s->rng; const char *F[16]; int nF, i, k, nu, has_gap = 0, pv = n % 3;
    double phi, d, *us, gap_u = 0; Frame f; GeoList *l = LL(), *slab, *covers = LL(), *busy = LL(), *sc; Geo *sl, *core, *road;
    static const Tier TC[] = { TI(0.8, 3), TI(0.2, 40) }, TR[] = { TI(0.6, 4), TI(0.4, 30) }, TG[] = { TI(1.0, 20) };
    nF = ly_pickF(TAB, 11, 13, n, F, 16);
    phi = U(0, PI); f = lf_frame(0, 0, phi);
    d = (!ly_in(F, nF, "tram") ? U(1.0, 4.0) : U(4.5, 6.0)) * SC.K;
    nu = ly_arange(KX_(-19.5) + U(0, 2), KX_(19), 4.5, &us);
    if (ly_in(F, nF, "brokencurb")) {
        double *mid = (double *)LM(sizeof(double) * (size_t)(nu + 1)); int nm = 0;
        for (i = 0; i < nu; i++) if (-8 < us[i] && us[i] < 6) mid[nm++] = us[i];
        if (nm) { gap_u = mid[py_choice(rng, nm)]; has_gap = 1; }
    }
    for (i = 0; i < nu; i++) if (!has_gap || us[i] != gap_u) LP(l, g_R(&f, us[i], us[i] + 4.0, d, d + 1.8));
    S_stack(s, l, NF(1.0), TC, 2, "curb", NONE, NULL);
    slab = PR(s, L1(g_R(&f, KX_(-20), KX_(20), d + 1.8, KX_(20))), H + 0.75, "slab");
    sl = slab->n ? g_uu(slab) : g_empty();
    if (ly_in(F, nF, "treegrate")) {
        double u, v; Geo *c;
        if (on_walk(rng, &f, d, sl, covers, 5.0, 5.0, &u, &v, &c)) {
            double *ys; int ny = ly_arange(-1.8, 1.9, 0.9, &ys); GeoList *t = LL();
            for (i = 0; i < ny; i++) LP(t, g_R(&f, u - 2.0, u + 2.0, v + ys[i] - 0.25, v + ys[i] + 0.25));
            S_tcut(s, t, NF(H + 0.15), NF(H + 1.2), "slot");
            LP(covers, c);
        }
    }
    if (ly_in(F, nF, "utility")) {
        double u, v; Geo *c;
        if (on_walk(rng, &f, d, sl, covers, 3.2, 3.2, &u, &v, &c)) {
            GeoList *t = LL();
            LP(t, g_diff(g_R(&f, u - 1.6, u + 1.6, v - 1.6, v + 1.6), g_R(&f, u - 1.15, u + 1.15, v - 1.15, v + 1.15)));
            LP(t, ptf(&f, u - 0.6, v, 0.3, 10)); LP(t, ptf(&f, u + 0.6, v, 0.3, 10));
            S_tcut(s, t, NF(H + 0.4), NF(H + 1.2), "slot");
            LP(covers, c);
        }
    }
    core = g_diff(g_diff(sl, g_R(&f, KX_(-20), KX_(20), d - 0.5, d + 2.3)), covers->n ? g_buf(g_uu(covers), 0.35) : g_empty());
    S_tcut(s, L1(g_inter(walk_joints(&f, phi, d, pv, 3.2), core)), NF(H + 0.33), NF(H + 0.75), "joint");
    road = g_R(&f, KX_(-20), KX_(20), KX_(-20), d - 0.3);
    if (ly_in(F, nF, "gutter") && !ly_in(F, nF, "drain")) {
        S_gcut(s, L1(g_R(&f, KX_(-20), KX_(20), d - 1.1, d - 0.3)), NF(0.3), "slot");
        LP(busy, g_R(&f, KX_(-20), KX_(20), d - 1.4, d));
    }
    if (ly_in(F, nF, "drain")) {
        double u0 = U(-6, 2); GeoList *t = LL();
        PR(s, L1(g_R(&f, u0, u0 + 6.0, d - 2.7, d + 0.1)), H + 0.3, "metal");
        for (i = 0; i < 4; i++) LP(t, g_R(&f, u0 + 0.8 + i * 1.1, u0 + 1.3 + i * 1.1, d - 2.1, d - 0.5));
        S_tcut(s, t, NF(H - 0.5), NF(H + 1.0), "slot");
        LP(busy, g_R(&f, u0, u0 + 6.0, d - 2.7, d + 0.1));
    }
    if (ly_in(F, nF, "tram")) {
        double tv = d - U(4.5, 5.5); static const double O[2] = { -2.4, 2.4 };
        for (i = 0; i < 2; i++) {
            double o = O[i];
            PR(s, L1(g_R(&f, KX_(-20), KX_(20), tv + o - 0.45, tv + o + 0.45)), H + 0.3, "iron");
            S_gcut(s, L1(g_R(&f, KX_(-20), KX_(20), tv + o + (o < 0 ? 0.5 : -1.0), tv + o + (o < 0 ? 1.0 : -0.5))), NF(0.5), "slot");
        }
        LP(busy, g_R(&f, KX_(-20), KX_(20), tv - 3.2, tv + 3.2));
    }
    if (ly_in(F, nF, "crosswalk")) {
        double u0 = U(-7, 3); GeoList *st = LL();
        for (k = 0; k < 3; k++) LP(st, g_R(&f, u0 + k * 2.4, u0 + k * 2.4 + 1.4, KX_(-20), d - 0.4));
        S_prism(s, st, NF(H + 0.25), "paint", NONE, 0.88, 1);
        LP(busy, g_R(&f, u0 - 0.5, u0 + 6.7, KX_(-20), d));
    }
    if (ly_in(F, nF, "cobble")) {
        double cv = U(-6, d - 4); GeoList *band = PR(s, L1(g_R(&f, KX_(-20), KX_(20), cv - 2.2, cv + 2.2)), H + 0.35, "cobble");
        if (band->n) {
            Geo *net = ly_vor_net(rng, KX_(-17), KX_(17), KA_(260), KX_(-20), KX_(20), 0, 0, 0.21, 3);
            S_tcut(s, L1(g_inter(net, g_inset(band->v[0], 0.3))), NF(H + 0.05), NF(H + 0.8), "slot");
        }
        LP(busy, g_R(&f, KX_(-20), KX_(20), cv - 2.6, cv + 2.6));
    }
    if (ly_in(F, nF, "manhole")) {
        int64_t t, m = KN_(80);
        for (t = 0; t < m; t++) {
            double uu = U(-8, 8), vv = U(-11, d - 3.5), cx, cy; Geo *c;
            lf_at(&f, uu, vv, &cx, &cy); c = g_ptbuf(cx, cy, 2.9, 48);
            if (road_free(s, road, busy, c, 0.6)) {
                Geo *rings = g_diff(g_ptbuf(cx, cy, 2.8, 48), g_ptbuf(cx, cy, 2.35, 48)), *inner = g_diff(g_ptbuf(cx, cy, 1.8, 48), g_ptbuf(cx, cy, 1.45, 48)), *hatch;
                GeoList *hl = LL(); static const double Y[3] = { -0.6, 0, 0.6 }; int yi;
                for (yi = 0; yi < 3; yi++) { double xy[4]; lf_at(&f, -3, Y[yi], &xy[0], &xy[1]); lf_at(&f, 3, Y[yi], &xy[2], &xy[3]); LP(hl, g_linebuf(xy, 2, 0.2, SHP_CAP_FLAT, 16)); }
                for (yi = 0; yi < 3; yi++) { double xy[4]; lf_at(&f, Y[yi], -3, &xy[0], &xy[1]); lf_at(&f, Y[yi], 3, &xy[2], &xy[3]); LP(hl, g_linebuf(xy, 2, 0.2, SHP_CAP_FLAT, 16)); }
                hatch = g_inter(g_trans(g_uu(hl), cx, cy), g_ptbuf(cx, cy, 1.1, 32));
                S_gcut(s, L2(rings, inner), NF(0.4), "slot");
                S_gcut(s, L1(hatch), NF(0.3), "slot");
                LP(busy, c);
                break;
            }
        }
    }
    if (ly_in(F, nF, "repair")) {
        int64_t t, m = KN_(80);
        for (t = 0; t < m; t++) {
            double uu = U(-8, 8), vv = U(-10, d - 3), cx, cy, a, xy[8]; Geo *p;
            lf_at(&f, uu, vv, &cx, &cy); a = U(-0.3, 0.3);
            xy[0] = cx - 3; xy[1] = cy - 1.8; xy[2] = cx + 3; xy[3] = cy - 1.8; xy[4] = cx + 3; xy[5] = cy + 1.8; xy[6] = cx - 3; xy[7] = cy + 1.8;
            p = g_rot(g_poly(xy, 4), LF_DEGREES(phi + a), SHP_ORIGIN_POINT, cx, cy);
            if (road_free(s, road, busy, p, 0.6)) { S_gcut(s, L1(p), NF(0.25), "patch"); LP(busy, p); break; }
        }
    }
    if (ly_in(F, nF, "crater")) {
        int64_t t, m = KN_(80);
        for (t = 0; t < m; t++) {
            double uu = U(-7, 7), vv = U(-10, d - 4.5), cx, cy, r;
            lf_at(&f, uu, vv, &cx, &cy); r = U(1.8, 2.5);
            if (road_free(s, road, busy, g_ptbuf(cx, cy, r + 2.2, 16), 0.3)) { gg2_crater(s, cx, cy, r, py_hypot(cx, cy) > 4 ? 0.8 : 0.7); LP(busy, g_ptbuf(cx, cy, r + 2.2, 16)); break; }
        }
    }
    if (has_gap) {
        GeoList *rub = LL(), *rq = LL();
        static const Tier TB2[] = { TI(0.6, 4), TI(0.4, 30) };
        for (i = 0; i < 3; i++) { double uu = gap_u + U(0.5, 3.5), vv = d + U(0.3, 1.5), x, y, r; lf_at(&f, uu, vv, &x, &y); r = U(0.6, 0.9); LP(rub, g_stone(x, y, r, rng, 0.8, 5)); }
        for (i = 0; i < 3; i++) if (S_free(s, rub->v[i], 0.3)) LP(rq, rub->v[i]);
        if (rq->n) S_stack(s, L1(g_uu(rq)), NF(0.8), TB2, 2, "rubble", NONE, NULL);
    }
    sc = S_scatter(s, RI(2, 4), 1.1, 1.8, road, 0.5, 0, 0);
    for (i = 0; i < sc->n; i++) {
        int hit = 0, q;
        for (q = 0; q < busy->n; q++) if (g_intersects(g_buf(sc->v[i], 0.5), busy->v[q])) { hit = 1; break; }
        if (!hit) { double h = U(0.8, 1.2); S_stack(s, L1(sc->v[i]), NF(h), TR, 2, "rubble", NONE, NULL); }
    }
    {
        GeoList *crk = LL(), *ends = LL(); int nk = 0;
        for (i = 0; i < busy->n; i++) if (g_area(busy->v[i]) < 60) LP(ends, g_centroid(busy->v[i]));
        nk = RI(1, 3);
        for (k = 0; k < nk; k++) {
            double x0, y0, tx, ty; Geo *e = ends->n ? ends->v[k % ends->n] : NULL;          /* [...] or [None] */
            { double uu = U(-8, 8); lf_at(&f, uu, d - 0.3, &x0, &y0); }
            if (e && k == 0) { tx = g_x(e); ty = g_y(e); }
            else { double uu = U(KX_(-14), KX_(14)); lf_at(&f, uu, d - 30, &tx, &ty); }
            LP(crk, crack_line(rng, x0, y0, tx, ty));
        }
        S_gcut(s, L1(g_inter(g_uu(crk), g_buf(road, 0.3))), NF(0.45), "crack");
    }
    {
        Geo *reg = busy->n ? g_diff(road, g_buf(g_uu(busy), 0.3)) : road; int kk = RI(20, 40);
        S_stack(s, S_scatter(s, kk, 0.47, 0.62, reg, 0.35, 1, 0), NF(0.25), TG, 1, "grit", NONE, NULL);
    }
    return s;
}

/* gg2.curb_blocks(walk, width=1.8, seg=4.5, disc=None) */
static GeoList *curb_blocks(Geo *walk, double width, double seg, Geo *disc) {
    Geo *band = g_diff(g_bufq(walk, width, 24), walk), *blocks; GeoList *gaps = LL(), *rings = g_polys(walk), *out = LL(), *bp; int ri, i;
    if (disc) band = g_inter(band, disc);
    for (ri = 0; ri < rings->n; ri++) {
        Geo *ext = g_ext(rings->v[ri]); double L = g_len(ext), q; int64_t k;
        if (px_floordiv_d(L, seg, &q) != 0) ly_raise();
        k = ly_int(q); if (k < 1) k = 1;
        for (i = 0; i < k; i++) {
            double t = i * L / k, tx, ty, n_, nx, ny, c0, c1, xy[8]; Geo *p = g_interp(ext, t), *qq = g_interp(ext, t + 0.05); static const double S1[2] = { -2.6, 2.6 }; int j;
            tx = g_x(qq) - g_x(p); ty = g_y(qq) - g_y(p);
            n_ = py_hypot(tx, ty); if (n_ == 0) n_ = 1;
            tx = tx / n_; ty = ty / n_; nx = -ty; ny = tx; c0 = g_x(p); c1 = g_y(p);
            for (j = 0; j < 2; j++) { xy[2 * j] = c0 + tx * 0.22 + nx * S1[j]; xy[2 * j + 1] = c1 + ty * 0.22 + ny * S1[j]; }
            for (j = 0; j < 2; j++) { xy[4 + 2 * j] = c0 - tx * 0.22 + nx * S1[1 - j]; xy[4 + 2 * j + 1] = c1 - ty * 0.22 + ny * S1[1 - j]; }
            LP(gaps, g_poly(xy, 4));
        }
    }
    blocks = gaps->n ? g_diff(band, g_uu(gaps)) : band;
    bp = g_polys(blocks);
    for (i = 0; i < bp->n; i++) if (g_area(bp->v[i]) > 1.5) LP(out, bp->v[i]);
    return out;
}

/* gg2.paving(s, walk, phi, kind, z=H + 0.75, step=3.6) */
static void paving(Scene *s, Geo *walk, double phi, const char *kind, double z, double step) {
    Geo *core = g_buf(walk, -0.6), *g, *x; GeoList *a = LL(), *b = LL();
    if (g_isempty(core)) return;
    if (!strcmp(kind, "flags")) {
        double *ys; int ny = ly_arange(KX_(-22), KX_(22), step, &ys), i;
        for (i = 0; i < ny; i++) LP(a, g_box(KX_(-22), ys[i] - 0.22, KX_(22), ys[i] + 0.22));
        g = g_uu(a);
        for (i = 0; i < ny; i++) LP(b, g_box(ys[i] - 0.22, KX_(-22), ys[i] + 0.22, KX_(22)));
        x = g_uu(b);
        g = g_rot(g, LF_DEGREES(phi) + 45, SHP_ORIGIN_POINT, 0, 0); x = g_rot(x, LF_DEGREES(phi) + 45, SHP_ORIGIN_POINT, 0, 0);
    } else {
        double L = 3.2, w = 1.55; int64_t j, i, nj = KN_(30), ni = KN_(15);
        for (j = 0; j < nj; j++) LP(a, g_box(KX_(-22), KX_(-22) + j * w - 0.22, KX_(22), KX_(-22) + j * w + 0.22));
        g = g_uu(a);
        for (j = 0; j < nj; j++) for (i = 0; i < ni; i++)
            LP(b, g_box(KX_(-22) + i * L + ly_mod(j, 2) * L / 2 - 0.22, KX_(-22) + j * w + 0.1, KX_(-22) + i * L + ly_mod(j, 2) * L / 2 + 0.22, KX_(-22) + (j + 1) * w - 0.1));
        x = g_uu(b);
        g = g_rot(g, LF_DEGREES(phi), SHP_ORIGIN_POINT, 0, 0); x = g_rot(x, LF_DEGREES(phi), SHP_ORIGIN_POINT, 0, 0);
    }
    S_tcut(s, L1(g_inter(g_uu2(g, x), core)), NF(z - 0.42), NF(z), "joint");
}

/* gg2.manhole(s, cx, cy, r=2.4, z=H) */
static void manhole(Scene *s, double cx, double cy, double r, double z) {
    Geo *ringg = g_diff(g_ptbuf(cx, cy, r, 48), g_ptbuf(cx, cy, r - 0.4, 48)), *hatch, *hd; GeoList *hl = LL(), *holes = LL(); double *ys; int ny = ly_arange(-r + 0.9, r - 0.5, 0.9, &ys), i;
    for (i = 0; i < ny; i++) LP(hl, g_box(cx - r, cy + ys[i] - 0.15, cx + r, cy + ys[i] + 0.15));
    hatch = g_inter(g_uu(hl), g_ptbuf(cx, cy, r - 0.75, 32));
    LP(holes, g_ptbuf(cx + -0.9, cy, 0.25, 10)); LP(holes, g_ptbuf(cx + 0.9, cy, 0.25, 10));
    if (z <= H) {
        S_gcut(s, L1(ringg), NF(0.45), "slot");
        hd = g_diff(hatch, g_buf(g_uu(holes), 0.3)); S_gcut(s, L1(hd), NF(0.2), "slot");
        S_gcut(s, holes, NF(0.6), "pit");
    } else {
        S_tcut(s, L1(ringg), NF(z - 0.45), NF(z + 0.3), "slot");
        hd = g_diff(hatch, g_buf(g_uu(holes), 0.3)); S_tcut(s, L1(hd), NF(z - 0.2), NF(z + 0.3), "slot");
        S_tcut(s, holes, NF(z - 0.6), NF(z + 0.3), "pit");
    }
}

/* gg2.road_detail(s, rng, road, f, hole_ok=True) */
static void road_detail(Scene *s, PyRandom *rng, Geo *road, const Frame *f) {
    Geo *road_in = g_inter(road, disc_r(R0 - 1.2)), *hole = NULL, *reg; GeoList *crk = LL(), *src, *sc; int k, nk, i;
    static const Tier TR[] = { TI(0.6, 4), TI(0.4, 30) }, TG[] = { TI(1.0, 20) };
    (void)f;
    if (g_isempty(road_in) || g_area(road_in) < 20) return;
    {
        double b[4]; int64_t t, m = KN_(80); Geo *rb = NULL;
        g_bounds(road_in, b);
        for (t = 0; t < m; t++) {
            double x = U(b[0], b[2]), y = U(b[1], b[3]), r = U(1.4, 2.2); Geo *cand = g_stone(x, y, r, rng, 0.8, 8);
            if (!rb) rb = g_buf(road_in, -0.5);
            if (g_contains(rb, cand) && S_free(s, cand, 0.6)) { hole = cand; break; }
        }
    }
    if (hole) S_gcut(s, L1(hole), NF(0.7), "pothole");
    src = g_polys(road_in);
    nk = RI(1, 3);
    for (k = 0; k < nk; k++) {
        Geo *P = g_max_area(src); double b[4], ax, ay, bx, by;
        g_bounds(P, b);
        ax = U(b[0], b[2]); ay = U(b[1], b[3]);
        if (k == 0 && hole) { Geo *c = g_centroid(hole); bx = g_x(c); by = g_y(c); }
        else { bx = U(b[0], b[2]); by = U(b[1], b[3]); }
        LP(crk, g_bufx(g_wigline(ax, ay, bx, by, rng, 0.6, 7), 0.22, 4, SHP_CAP_ROUND, SHP_JOIN_ROUND, 5.0));
    }
    S_gcut(s, L1(g_inter(g_uu(crk), g_buf(road, -0.3))), NF(0.45), "crack");
    reg = hole ? g_diff(road_in, g_buf(hole, 0.3)) : road_in;
    sc = S_scatter(s, RI(1, 4), 1.1, 1.8, reg, 0.5, 0, 0);
    for (i = 0; i < sc->n; i++) { double h = U(0.8, 1.2); S_stack(s, L1(sc->v[i]), NF(h), TR, 2, "rubble", NONE, NULL); }
    { int kk = RI(15, 30); S_stack(s, S_scatter(s, kk, 0.47, 0.62, reg, 0.35, 1, 0), NF(0.25), TG, 1, "grit", NONE, NULL); }
}

/* urban_layout's corner(a, b, rc, sx=1, sy=1) */
static Geo *ul_corner(const Frame *f, double a, double b, double rc, int sx, int sy) {
    double u0 = sx > 0 ? a : -40, u1 = sx > 0 ? 40 : a, v0 = sy > 0 ? b : -40, v1 = sy > 0 ? 40 : b, ui = a + sx * rc, vi = b + sy * rc;
    double X = sx * 40, Y = sy * 40;
    Geo *sq = g_R(f, u0, u1, v0, v1), *inner = g_bufq(g_R(f, X < ui ? X : ui, X > ui ? X : ui, Y < vi ? Y : vi, Y > vi ? Y : vi), rc, 32);
    return g_inter(sq, inner);
}

/* gg2.urban_layout(n, layout) */
static Scene *urban_layout(int n, const char *layout) {
    Scene *s = ly_mk("urban", "Urban Warfare", n, "#7e7b76"); PyRandom *rng = &s->rng; double phi = U(0, 2 * PI); Frame f = lf_frame(0, 0, phi);
    Geo *D = g_ptbuf(0, 0, R0 + 1.0, 128), *walk, *road, *sl, *covers; GeoList *walks = LL(), *slab; const char *extra = NULL; int i, k;
    static const Tier TC[] = { TI(0.8, 3), TI(0.2, 40) }, TR[] = { TI(0.6, 4), TI(0.4, 30) }, TGR[] = { TI(0.6, 20), TI(0.4, 45) };
    if (!strcmp(layout, "corner")) {
        double a = U(-4, 1) * SC.K, b = U(-4, 1) * SC.K, rc = U(2.5, 4.0);
        LP(walks, ul_corner(&f, a, b, rc, 1, 1));
    } else if (!strcmp(layout, "crossroads")) {
        double a = U(3.5, 5.5) * SC.K, b = U(3.5, 5.5) * SC.K, r1, r2; Geo *c1;
        r1 = U(2.0, 3.0); c1 = ul_corner(&f, a, b, r1, 1, 1); r2 = U(2.0, 3.0);
        LP(walks, c1); LP(walks, ul_corner(&f, -a, -b, r2, -1, -1));
        extra = "crosswalk";
    } else if (!strcmp(layout, "median")) {
        double c = U(-3, 3) * SC.K, w = U(4.0, 5.0) * ly_pow(SC.K, 0.6);
        LP(walks, g_R(&f, KX_(-40), KX_(40), c - w / 2, c + w / 2));
        extra = RND() < 0.6 ? "planter" : "bollards";
    } else if (!strcmp(layout, "alley")) {
        double c = U(-2, 2) * SC.K, half = U(3.8, 4.8) * SC.K;
        LP(walks, g_R(&f, KX_(-40), KX_(40), c + half, KX_(40))); LP(walks, g_R(&f, KX_(-40), KX_(40), KX_(-40), c - half));
        extra = RND() < 0.6 ? "manhole" : "drain";
    }
    if (!strcmp(layout, "plaza")) {
        double cx, cy, a0; GeoList *sc;
        slab = PR(s, L1(D), H + 0.6, "slab");
        sl = slab->n ? g_uu(slab) : g_empty();
        { double uu = U(-3, 3), vv = U(-3, 3); lf_at(&f, uu, vv, &cx, &cy); }
        { double r = U(2.2, 2.6); manhole(s, cx, cy, r, H + 0.6); }
        { Geo *core = g_diff(sl, g_ptbuf(cx, cy, 3.4, 16)); double st = U(4.2, 5.0); paving(s, core, phi, "flags", H + 0.6, st); }
        a0 = U(0, 2 * PI);
        { int nk = RI(2, 4); for (k = 0; k < nk; k++) { double a = a0 + k * 0.35, d = 9.0; S_prism(s, L1(g_ptbuf(d * COS(a), d * SIN(a), 0.75, 20)), NF(H + 0.6 + 1.6), "post", NF(H + 0.45), SC.MINW, 1); } }
        sc = S_scatter(s, RI(1, 3), 1.0, 1.6, g_buf(sl, -1.0), 0.6, 0, 0);
        for (i = 0; i < sc->n; i++) { double h = U(0.7, 1.0); S_stack(s, L1(sc->v[i]), NF(h), TR, 2, "rubble", NF(H + 0.6 - 0.15 + SINK), NULL); }
        return s;
    }
    walk = g_inter(g_uu(walks), D);
    road = g_diff(D, g_buf(walk, 1.8 + 0.3));
    S_stack(s, curb_blocks(walk, 1.8, 4.5, D), NF(1.0), TC, 2, "curb", NONE, NULL);
    slab = PR(s, g_polys(walk), H + 0.75, "slab");
    sl = slab->n ? g_uu(slab) : g_empty();
    covers = g_empty();
    if (extra && !strcmp(extra, "planter")) {
        Geo *c = g_centroid(sl), *bx; double cx = g_x(c), cy = g_y(c);
        bx = LA(shp_buffer(c->g, 1.9, 16, SHP_CAP_SQUARE, SHP_JOIN_ROUND, 5.0, 0));          /* Point(c.x, c.y).buffer(1.9, cap_style=3) */
        bx = g_rot(bx, LF_DEGREES(phi), SHP_ORIGIN_POINT, cx, cy);
        if (g_contains(g_buf(sl, -0.4), bx)) {
            S_prism(s, L1(g_diff(bx, g_buf(bx, -0.5))), NF(H + 0.75 + 1.1), "concrete", NF(H + 0.6), SC.MINW, 1);
            S_stack(s, L1(g_buf(bx, -0.55)), NF(0.9), TGR, 2, "grass", NF(H + 0.75 - 0.15 + SINK), NULL);
            covers = g_buf(bx, 0.4);
        }
    }
    if (extra && !strcmp(extra, "bollards")) {
        Geo *c = g_centroid(sl); double cx = g_x(c), cy = g_y(c);
        for (k = -1; k <= 1; k++) {
            double u = cx * COS(phi) + cy * SIN(phi), v = -cx * SIN(phi) + cy * COS(phi), x, y; Geo *p;
            lf_at(&f, u + k * 4.0, v, &x, &y);
            p = g_ptbuf(x, y, 0.7, 20);
            if (g_contains(g_buf(sl, -0.5), p)) { S_prism(s, L1(p), NF(H + 0.75 + 1.5), "post", NF(H + 0.6), SC.MINW, 1); covers = g_union(covers, g_buf(p, 0.4)); }
        }
    }
    { int64_t ch = py_choice(rng, 2); paving(s, g_diff(sl, covers), phi, ch ? "bond" : "flags", H + 0.75, 3.6); }
    if (extra && !strcmp(extra, "crosswalk")) {
        double u0 = U(-2, 2); GeoList *st = LL(), *q = LL(); Geo *arm;
        for (k = -3; k < 3; k++) LP(st, g_R(&f, KX_(-20), KX_(20), u0 + k * 1.6, u0 + k * 1.6 + 0.9));
        arm = g_inter(g_R(&f, 7.5, KX_(20), KX_(-20), KX_(20)), g_buf(road, -0.4));
        for (i = 0; i < st->n; i++) gl_extend(q, g_polys(g_inter(st->v[i], arm)));
        S_prism(s, q, NF(H + 0.25), "paint", NONE, 0.6, 1);
    }
    if (extra && !strcmp(extra, "manhole")) {
        double cx, cy, uu = U(-6, 6);
        lf_at(&f, uu, 0, &cx, &cy);
        if (g_contains(g_buf(road, -0.6), g_ptbuf(cx, cy, 2.4, 16))) manhole(s, cx, cy, 2.4, H);
    }
    if (extra && !strcmp(extra, "drain")) {
        double u0 = U(-6, 2), v = -U(3.8, 4.8) + 1.0; GeoList *t = LL();
        for (i = 0; i < 4; i++) LP(t, g_R(&f, u0 + i * 1.1, u0 + i * 1.1 + 0.5, v - 1.6, v));
        S_gcut(s, t, NF(0.8), "slot");
    }
    road_detail(s, rng, road, &f);
    return s;
}

/* gg2.urban(n) */
Scene *gg2_urban(int n) {
    static const char *KEYS[6] = { "straight", "corner", "crossroads", "median", "alley", "plaza" }; static const double W[6] = { 3, 3, 2, 1.5, 1.5, 1 };
    PyRandom r; char seed[1200]; int64_t ix;
    snprintf(seed, sizeof seed, "urbanlayout%d%s%s", n, LYMK->salt ? LYMK->salt : "", LYMK->reseed ? LYMK->reseed : "");
    py_seed_str(&r, seed);
    if (py_choices_w(&r, 6, W, 1, &ix) != 0) ly_raise();
    if (ix == 0) return n <= 10 ? urban_a(n) : urban_x(n);
    return urban_layout(n, KEYS[ix]);
}
