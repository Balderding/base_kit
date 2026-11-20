/* layouts_new.c: layout(n) of the styles kept one per file (style_ashveil, style_blight, style_ferrum, style_obelis,
   style_sundered, style_thornwild) with their own ground helpers. Their kit names (floor, banks, j_grid, meander ...)
   come from styles2 / gg3 through _take(G): here layouts_s2.c and leaf.c. The module side effects of the layouts
   (blight / obelis _register: kit sets told about new kinds, s.ob_v) do not touch the scene's dumped state; see
   layouts.h for what the integrating code must still do (obelis: "obfloor" in gg3.BRIDGE, s.ob_v). */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "layouts.h"

#define H (SC.H)
#define R0 (SC.R0)
#define U(a, b) py_uniform(rng, (a), (b))
#define RI(a, b) ((int)py_randint(rng, (a), (b)))
#define RND() py_random(rng)
#define TI(f, a) { { (f), NUM_FLOAT, 0 }, { (a), NUM_INT, 0 } }
#define COS py_cos
#define SIN py_sin

static const Tier T2550[2] = { TI(0.5, 25), TI(0.5, 50) };
static Geo *dr(double r) { return g_ptbuf(0, 0, r, 16); }
static void rocks_on(Scene *s, PyRandom *rng, int n0, int n1, double rmin, double rmax, double h0, double h1, const char *kind) {
    GeoList *sc = S_scatter(s, RI(n0, n1), rmin, rmax, NULL, 0.5, 0, 0); int i;
    for (i = 0; i < sc->n; i++) { double h = U(h0, h1); S_stack(s, L1(sc->v[i]), NF(h), S2_ROCKY, 2, kind, NONE, NULL); }
}

/* ================================================================ style_ashveil (the file of 2026-11-19 22:19) */
/* style_ashveil._ripples(s, rng, phi, D, region=None, n=(2, 3), step=(2.2, 2.8), h=(0.22, 0.3)) */
static Geo *av_ripples(Scene *s, PyRandom *rng, double phi, Geo *region, int n0, int n1) {
    Geo *reg = !region ? dr(R0 - 0.8) : g_inter(region, dr(R0 - 0.8)); double E, st; GeoList *out = LL(); int64_t k, m;
    if (g_isempty(reg)) return g_empty();
    E = 15.5 * s2_KK(); st = U(2.2, 2.8);
    m = s2_NN(RI(n0, n1));
    for (k = 0; k < m; k++) {
        double cx, cy, r, a0, *cs; int nc, i; Geo *patch, *pin;
        lf_rp(rng, 0, 11, &cx, &cy); r = U(3.0, 4.8);
        patch = g_inter(LAG(lf_blob(cx, cy, r, rng, 5)), reg);
        if (g_isempty(patch)) continue;
        pin = NULL;
        a0 = -E + U(0, st);
        nc = ly_arange(a0, E, st, &cs);
        for (i = 0; i < nc; i++) {
            double ph = phi + U(-0.1, 0.1), off = cs[i] / (SC.K > 1.0 ? SC.K : 1.0), amp = U(0.3, 0.6), w; Geo *ln = LAG(lf_meander(rng, ph, off, amp, 1.0, 40)), *g;
            w = U(0.24, 0.3);
            if (!pin) pin = g_buf(patch, -0.3);
            g = g_inter(g_buf(ln, w), pin);
            if (!g_isempty(g) && g_area(g) > 0.4) LP(out, g);
        }
    }
    if (out->n) { double h = U(0.22, 0.3); S_gcut(s, L1(g_uu(out)), NF(h), "joint"); }
    return out->n ? g_uu(out) : g_empty();
}
/* style_ashveil._shards(s, rng, n, kind="ashscrap", r=(0.7, 1.2), h=(0.35, 0.6)) */
static void av_shards(Scene *s, PyRandom *rng, int n) {
    GeoList *sc = S_scatter(s, n, 0.7, 1.2, NULL, 0.4, 0, 0); int i;
    for (i = 0; i < sc->n; i++) {
        double a = U(0, 180); Geo *g = g_rot(g_scale(sc->v[i], 1.5, 0.7, SHP_ORIGIN_CENTER, 0, 0), a, SHP_ORIGIN_CENTROID, 0, 0);
        if (S_free(s, g, 0.3) && g_contains(dr(R0 - 1.0), g)) { double h = U(0.35, 0.6); PR(s, L1(g), H + h, "ashscrap"); }
    }
}
Scene *new_ashveil(int n) {
    Scene *s = ly_mk("ashveil", "Ashveil Wastes", n, "#8a8580"); PyRandom *rng = &s->rng; int v = RI(0, 3), i; Geo *D = s2_D(); S2Fr fr = s2_fr(rng);
    static const Tier TH[] = { TI(0.8, 6), TI(0.2, 30) }, TD[] = { TI(0.35, 18), TI(0.35, 35), TI(0.3, 55) };
    if (v == 0) {
        double c = U(-6, 0) * SC.K; Geo *reg = g_inter(s2_R(&fr, -30, 30, c, 30), D), *rest;
        s2_floor(s, reg, 0.42, "flag", s2_j_voronoi(rng, RI(14, 20), 0.26, 0.45), 0.34, 0.5);
        rest = g_diff(dr(R0 - 0.8), g_buf(reg, 0.6));
        av_ripples(s, rng, fr.phi, rest, 1, 2);
        s2_banks(s, rng, RI(1, 2), 1.6, 2.4, 0.45, 0.7, "ashdrift", T2550, 2, rest);
    } else if (v == 1) {
        s2_floor(s, D, 0.42, "flag", s2_j_voronoi(rng, RI(14, 20), 0.26, 0.45), 0.34, 0.5);
        s2_banks(s, rng, RI(2, 4), 1.5, 2.6, 0.35, 0.6, "ashdrift", T2550, 2, NULL);
    } else if (v == 2) {
        GeoList *sc;
        av_ripples(s, rng, fr.phi, NULL, 2, 3);
        sc = S_scatter(s, RI(2, 3), 1.4, 2.0, NULL, 0.6, 0, 0);
        for (i = 0; i < sc->n; i++) {
            double a = U(0, 180); Geo *g = g_rot(g_scale(sc->v[i], 1.8, 0.55, SHP_ORIGIN_CENTER, 0, 0), a, SHP_ORIGIN_CENTROID, 0, 0);
            if (S_free(s, g, 0.4) && g_contains(dr(R0 - 1.0), g)) { double h = U(0.7, 1.1); S_stack(s, L1(g), NF(h), TH, 2, "ashhull", NONE, NULL); }
        }
        s2_banks(s, rng, RI(1, 3), 1.6, 2.6, 0.45, 0.75, "ashdrift", T2550, 2, NULL);
        s2_cracks(s, rng, RI(2, 3), 0.35, "crack", 0.22);
    } else {
        double c = U(6, 9) * SC.K; Geo *dune = g_inter(s2_R(&fr, -30, 30, c, 30), dr(R0 - 0.6));
        if (!g_isempty(dune)) { double h = U(0.5, 0.75); S_stack(s, L1(dune), NF(h), TD, 3, "ashdrift", NONE, NULL); }
        s2_floor(s, g_inter(s2_R(&fr, -30, 30, -30, c - 1.5), D), 0.4, "flag", s2_j_voronoi(rng, RI(14, 20), 0.26, 0.45), 0.32, 0.5);
    }
    rocks_on(s, rng, 3, 6, 0.8, 1.4, 0.5, 0.9, "ashcinder");
    av_shards(s, rng, RI(2, 4));
    s2_grit(s, rng, 10, 18, "ashcinder");
    return s;
}

/* ================================================================ style_blight (the file of 2026-11-19 22:21) */
static const Tier SPIKE[2] = { TI(0.75, 4), TI(0.25, 30) };
/* BLIGHT_RUNES: strokes (p, q) in a box from -1 to 1 */
static const double RUNES[8][5][4] = {
    { { 0, -1, 0, 1 }, { 0, 1, 0.6, 0.55 }, { 0.6, 0.55, 0.6, 0.1 } },
    { { -0.6, 1, 0.25, 0.35 }, { 0.25, 0.35, -0.25, -0.35 }, { -0.25, -0.35, 0.6, -1 } },
    { { 0, -1, 0, 1 }, { 0, 0.45, 0.55, 0.85 }, { 0, -0.15, -0.55, 0.2 } },
    { { -0.6, -0.9, 0, 0.9 }, { 0, 0.9, 0.15, -0.2 }, { 0.15, -0.2, 0.6, 0.3 } },
    { { 0, 0.2, 0, 1 }, { -0.42, -0.25, 0, -0.67 }, { 0, -0.67, 0.42, -0.25 }, { 0.42, -0.25, 0, 0.17 }, { 0, 0.17, -0.42, -0.25 } },
    { { -0.3, -1, -0.3, 1 }, { -0.3, 0.5, 0.45, 0.15 }, { -0.3, -0.25, 0.45, -0.6 } },
    { { 0.5, -1, -0.2, -0.2 }, { -0.2, -0.2, 0.3, 0.4 }, { 0.3, 0.4, -0.4, 1 } },
    { { -0.55, -0.8, 0.55, -0.8 }, { -0.35, -0.8, -0.1, 0.9 }, { 0.35, -0.8, 0.5, 0.3 } },
};
static const int RUNE_N[8] = { 3, 3, 3, 3, 5, 3, 3, 3 };
/* style_blight.rune(f2, k, s, w) */
static Geo *bl_rune(const Frame *f2, int64_t k, double s, double w) {
    GeoList *l = LL(); int i; const double (*st)[4] = RUNES[ly_mod(k, 8)];
    for (i = 0; i < RUNE_N[ly_mod(k, 8)]; i++) {
        double xy[4];
        lf_at(f2, st[i][0] * s * 0.5, st[i][1] * s * 0.5, &xy[0], &xy[1]); lf_at(f2, st[i][2] * s * 0.5, st[i][3] * s * 0.5, &xy[2], &xy[3]);
        LP(l, g_linebuf(xy, 2, w / 2, SHP_CAP_ROUND, 3));
    }
    return g_uu(l);
}
/* style_blight._thorn_clumps(s, rng, n, r=(0.7, 1.05), h=(1.4, 2.3), region=None) */
static void bl_thorn_clumps(Scene *s, PyRandom *rng, int n, Geo *region) {
    GeoList *sc = S_scatter(s, n, 1.4, 1.8, region, 0.6, 0, 0); int i, k, nk;
    for (i = 0; i < sc->n; i++) {
        Geo *c = g_centroid(sc->v[i]); double cx = g_x(c), cy = g_y(c), r, asp; int nv; GeoList *ps = LL(), *rest;
        r = U(0.7, 1.05); asp = U(0.6, 0.85); nv = py_choice(rng, 2) ? 4 : 3;
        LP(ps, g_stone(cx, cy, r, rng, asp, nv));
        nk = RI(1, 3);
        for (k = 0; k < nk; k++) {
            double t = U(0, 6.283), d = U(1.3, 1.9), rr = U(0.5, 0.7); Geo *q = g_stone(cx + d * COS(t), cy + d * SIN(t), rr, rng, 0.7, 3); int ok = 1, o;
            for (o = 0; o < ps->n; o++) if (!(g_dist(q, ps->v[o]) > 0.25)) { ok = 0; break; }
            if (ok && g_contains(dr(R0 - 0.8), q) && S_free(s, q, 0.3) && (!region || g_contains(region, q))) LP(ps, q);
        }
        { double h = U(1.4, 2.3); S_stack(s, L1(ps->v[0]), NF(h), SPIKE, 2, "thorn", NONE, NULL); }
        if (ps->n > 1) {
            double h = U(0.9, 1.4); rest = LL(); for (k = 1; k < ps->n; k++) LP(rest, ps->v[k]);
            S_stack(s, rest, NF(h), SPIKE, 2, "thorn", NONE, NULL);
        }
    }
}
/* the point and the outward normal angle of a rift's edge at u along ln (p, t) */
static void rift_at(PyRandom *rng, Geo *ln, double L, double u, Geo **p, double *t) {
    Geo *q; double ch;
    *p = g_interp(ln, u); q = g_interp(ln, L < u + 0.3 ? L : u + 0.3);
    ch = py_choice(rng, 2) ? 1 : -1;
    *t = py_atan2(g_y(q) - g_y(*p), g_x(q) - g_x(*p)) + ch * PI / 2;
}
/* style_blight._rift(s, rng, phi, w=(1.5, 2.4), depth=0.85, keep=None, lip_h=(0.5, 1.0)) */
static Geo *bl_rift(Scene *s, PyRandom *rng, double phi, double w0, double w1, double depth, Geo *keep, double lh0, double lh1) {
    double wd = U(w0, w1), off, amp, L; Geo *ln, *g, *disc; GeoList *teeth = LL(), *cr = LL(), *lips = LL(), *all; int64_t i, m; int k;
    off = U(-3.5, 3.5); amp = U(1.8, 3.2);
    ln = LAG(lf_meander(rng, phi, off, amp, wd, 40));
    g = g_bufx(ln, wd / 2, 16, SHP_CAP_FLAT, SHP_JOIN_ROUND, 5.0);
    L = g_len(ln);
    m = ly_int(L / 1.1);
    for (i = 0; i < m; i++) {
        double u = (i + U(0.2, 0.8)) * 1.1, t, d, r; Geo *p;
        if (u >= L - 0.5) break;
        rift_at(rng, ln, L, u, &p, &t);
        d = wd / 2 + U(-0.1, 0.25); r = U(0.35, 0.6);
        LP(teeth, g_stone(g_x(p) + d * COS(t), g_y(p) + d * SIN(t), r, rng, 0.7, 3));
        if (RND() < 0.16) {
            double a = t + U(-0.5, 0.5), L2 = U(2.5, 5.0), bx = g_x(p) + (d + L2) * COS(a), by = g_y(p) + (d + L2) * SIN(a);
            LP(cr, g_bufx(g_wigline(g_x(p), g_y(p), bx, by, rng, 0.5, 4), 0.22, 3, SHP_CAP_ROUND, SHP_JOIN_ROUND, 5.0));
        }
    }
    disc = g_ptbuf(0, 0, R0 - 0.9, 96);
    all = LL(); LP(all, g); gl_extend(all, teeth);
    g = g_simp(g_inter(g_uu(all), disc), 0.04);
    if (g_isempty(g)) return g_empty();
    S_gcut(s, g_polys(g), NF(depth), "rift");
    if (cr->n) S_gcut(s, g_polys(g_inter(g_diff(g_uu(cr), g), disc)), NF(0.45), "rift");
    m = ly_int(L / 1.6);
    for (i = 0; i < m; i++) {
        double u = (i + U(0, 1)) * 1.6, t, d, r, asp; int nv, ok; Geo *p, *b;
        if (u >= L) break;
        rift_at(rng, ln, L, u, &p, &t);
        d = wd / 2 + U(1.0, 1.5); r = U(0.7, 1.05); asp = U(0.55, 0.8); nv = RI(4, 6);
        b = g_stone(g_x(p) + d * COS(t), g_y(p) + d * SIN(t), r, rng, asp, nv);
        ok = g_dist(b, g) > 0.25 && g_contains(g_buf(disc, -0.4), b);
        if (ok) for (k = 0; k < lips->n; k++) if (!(g_dist(b, lips->v[k]) > 0.3)) { ok = 0; break; }
        if (ok && keep && g_intersects(b, keep)) ok = 0;
        if (ok) LP(lips, b);
    }
    for (k = 0; k < lips->n; k++) { double h = U(lh0, lh1); S_stack(s, L1(lips->v[k]), NF(h), S2_ROCKY, 2, "blightrock", NONE, NULL); }
    return g;
}
/* style_blight._theme_here(s): the theme this base will get (THEME, or scenes.theme_order(NAME, SALT)) */
static const char *BL_THEMES[5] = { "spike garden", "bone altar", "boiling pool", "rune circle", "rift in the ground" };
static const char *bl_theme_here(Scene *s) {
    static char th[256]; const char *e = getenv("THEME"); const char *us; long n; int64_t idx[5]; int i; PyRandom r; char seed[1200];
    if (e) {          /* os.environ.get("THEME", "").strip().lower() */
        size_t a = 0, b = strlen(e), j;
        while (a < b && (e[a] == ' ' || e[a] == '\t' || e[a] == '\n' || e[a] == '\r')) a++;
        while (b > a && (e[b - 1] == ' ' || e[b - 1] == '\t' || e[b - 1] == '\n' || e[b - 1] == '\r')) b--;
        if (b > a) { for (j = 0; j < b - a && j < sizeof th - 1; j++) th[j] = (char)((e[a + j] >= 'A' && e[a + j] <= 'Z') ? e[a + j] + 32 : e[a + j]); th[j] = 0; return th; }
    }
    us = strrchr(s->name, '_');
    if (!us) return "";
    n = strtol(us + 1, NULL, 10);
    for (i = 0; i < 5; i++) idx[i] = i;          /* scenes.theme_order: Random(f"themes{style}{salt}").shuffle(range(5)) */
    snprintf(seed, sizeof seed, "themesblight%s", LYMK->salt ? LYMK->salt : "");
    py_seed_str(&r, seed); py_shuffle_idx(&r, idx, 5);
    return BL_THEMES[idx[ly_mod(ly_mod(n - 1, 5) + (n - 1 >= 0 ? (n - 1) / 5 : -((-(n - 1) + 4) / 5)), 5)]];
}
/* style_blight._figure_ground(s, rng, theme, keep, rg, floored=False) */
static void bl_figure_ground(Scene *s, PyRandom *rng, const char *theme, Geo *keep, Geo *rg, int floored) {
    static const Tier TBONE[3] = { TI(0.45, 12), TI(0.3, 38), TI(0.25, 62) };
    Geo *D = g_ptbuf(0, 0, R0 - 0.9, 96), *busy, *web; double A = g_area(rg) / 200.0, rk, k2, b[4]; GeoList *mk = LL(), *sc; int i, j, nr;
    if (s->gcuts.n || s->feet.n) {
        GeoList *l = LL(); for (i = 0; i < s->gcuts.n; i++) gl_extend(l, &s->gcuts.v[i].polys); gl_extend(l, &s->feet);
        busy = g_buf(g_uu(l), 0.35);
    } else busy = g_empty();
    web = floored ? g_empty() : g_diff(g_inter(s2_j_voronoi(rng, RI(16, 24), 0.2, 0.5), D), busy);
    if (!g_isempty(web)) S_gcut(s, g_polys(web), NF(0.32), "rift");
    busy = !g_isempty(web) ? g_uu2(busy, g_buf(web, 0.3)) : busy;
    g_bounds(keep, b); rk = (b[2] - b[0]) / 2;
    nr = RI(4, 6);
    for (i = 0; i < nr; i++) {
        for (j = 0; j < 20; j++) {
            double t_ = U(0, 6.283), d_ = U(0.25, 0.85) * rk, cx = d_ * COS(t_), cy = d_ * SIN(t_), a, sz; int64_t k; Frame f2; Geo *g; int ok, o;
            a = U(0, 6.283); f2 = lf_frame(cx, cy, a); k = py_randbelow(rng, 8); sz = U(1.1, 1.5);
            g = bl_rune(&f2, k, sz, 0.26);
            ok = !g_intersects(g_buf(g, 0.3), busy);
            if (ok) for (o = 0; o < mk->n; o++) if (!(g_dist(g, mk->v[o]) > 0.5)) { ok = 0; break; }
            if (ok) { LP(mk, g); break; }
        }
    }
    if (mk->n) {
        GeoList *q = LL(); for (i = 0; i < mk->n; i++) gl_extend(q, g_polys(mk->v[i]));
        if (floored) S_tcut(s, q, NF(H + 0.2), NF(H + 0.9), "runecut"); else S_gcut(s, q, NF(0.3), "runecut");
    }
    if (!strcmp(theme, "rune circle")) {
        double Rr = rk + 1.6, a0; int n_ = RI(9, 12); GeoList *rr = LL();
        a0 = U(0, 6.283);
        LP(rr, LAG(lf_ring(0, 0, Rr + 1.4, Rr + 1.05, 64)));
        for (i = 0; i < n_; i++) {
            double an = a0 + 6.283 * i / n_; Frame f2 = lf_frame(Rr * COS(an), Rr * SIN(an), an + PI / 2); int64_t k = py_randbelow(rng, 8);
            LP(rr, bl_rune(&f2, k, 1.4, 0.3));
        }
        S_gcut(s, g_polys(g_inter(g_uu(rr), D)), NF(0.35), "runecut");
    }
    k2 = A > 0.6 ? A : 0.6;          /* max(0.6, A) */
    { int r = RI(3, 5); bl_thorn_clumps(s, rng, (int)ly_rint(r * k2 * (!strcmp(theme, "spike garden") ? 1.8 : 1.0)), rg); }
    { int r = RI(2, 3); s2_banks(s, rng, ly_rint(r * k2), 1.4, 2.4, 0.6, 1.1, "fleshrock", S2_SOFTT, 2, rg); }
    { int r = RI(3, 5); sc = S_scatter(s, (int)ly_rint(r * k2), 0.9, 1.6, rg, 0.5, 0, 0); for (i = 0; i < sc->n; i++) { double h = U(0.6, 1.1); S_stack(s, L1(sc->v[i]), NF(h), S2_ROCKY, 2, "blightrock", NONE, NULL); } }
    { int r = RI(3, 5); sc = S_scatter(s, (int)ly_rint(r * k2), 0.5, 0.8, rg, 0.4, 1, 0); for (i = 0; i < sc->n; i++) { double h = U(0.6, 0.9); S_stack(s, L1(sc->v[i]), NF(h), S2_SOFTT, 2, "blister", NONE, NULL); } }
    if (!strcmp(theme, "bone altar") || !strcmp(theme, "spike garden") || !strcmp(theme, "rift in the ground")) {
        int r = RI(3, 5); sc = S_scatter(s, (int)ly_rint(r * k2), 0.6, 0.8, rg, 0.4, 1, 0);
        for (i = 0; i < sc->n; i++) {
            Geo *c = g_centroid(sc->v[i]), *bn; double a_ = U(0, PI), L_ = U(1.6, 2.4), xy[4], px, py; Frame ff = lf_frame(g_x(c), g_y(c), a_); GeoList *l = LL();
            lf_at(&ff, -L_ / 2, 0, &xy[0], &xy[1]); lf_at(&ff, L_ / 2, 0, &xy[2], &xy[3]);
            LP(l, g_linebuf(xy, 2, 0.3, SHP_CAP_ROUND, 16));
            lf_at(&ff, -L_ / 2, 0, &px, &py); LP(l, g_ptbuf(px, py, 0.42, 10));
            lf_at(&ff, L_ / 2, 0, &px, &py); LP(l, g_ptbuf(px, py, 0.42, 10));
            bn = g_uu(l);
            if (g_contains(rg, bn) && S_free(s, bn, 0.3)) { double h = U(0.45, 0.6); S_stack(s, L1(bn), NF(h), TBONE, 3, "bone", NONE, NULL); }
        }
    }
    if (!strcmp(theme, "boiling pool")) s2_pools(s, rng, RI(2, 3), 1.5, 2.4, 0.6, "ichor", g_buf(keep, 0.5));
}
Scene *new_blight(int n) {
    Scene *s = ly_mk("blight", "The Blight", n, "#6a6152"); PyRandom *rng = &s->rng; int v = RI(0, 4), fig; Geo *D = s2_D(), *keep = NULL, *ring_ = NULL; S2Fr fr = s2_fr(rng);
    const char *ks = getenv("KIT_SCALE"), *kf = getenv("KIT_FINAL_MM"), *theme = ""; double ts = (ks && *ks) ? strtod(ks, NULL) : 1;
    fig = ts != 1.0;
    if (fig) theme = bl_theme_here(s);
    if (fig) {
        double rv = (kf && *kf) ? strtod(kf, NULL) : 2 * R0 * ts; int64_t ch;
        rv = rv / (2 * ts); rv = rv < R0 ? rv : R0;          /* min(R0, ...) */
        keep = g_ptbuf(0, 0, 0.5 * rv, 48);
        ring_ = g_diff(g_ptbuf(0, 0, (rv - 0.5 < R0 - 0.9 ? rv - 0.5 : R0 - 0.9), 96), g_buf(keep, 0.4));
        ch = py_choice(rng, 4);          /* the .get default is drawn whatever the theme */
        v = !strcmp(theme, "rift in the ground") ? 1 : !strcmp(theme, "boiling pool") ? 2 : !strcmp(theme, "spike garden") ? 3 : (int)((const int[]){ 0, 1, 3, 4 })[ch];
    }
    if (v == 1 && fig) bl_rift(s, rng, fr.phi, 2.4, 3.2, 0.95, g_buf(keep, 0.3), 0.8, 1.4);
    else if (v == 0) { s2_floor(s, D, 0.5, "blightrock", s2_j_voronoi(rng, RI(14, 22), 0.3, 0.6), 0.45, 0.5); bl_thorn_clumps(s, rng, RI(1, 3), ring_); }
    else if (v == 1) { bl_rift(s, rng, fr.phi, 1.5, 2.4, 0.85, NULL, 0.5, 1.0); if (RND() < 0.5) bl_thorn_clumps(s, rng, RI(1, 2), NULL); }
    else if (v == 2) {
        s2_pools(s, rng, RI(2, 3), 1.8, 3.0, 0.6, "ichor", keep);
        s2_banks(s, rng, RI(2, 3), 1.5, 2.4, 0.6, 1.1, "fleshrock", S2_SOFTT, 2, ring_);
        s2_cracks(s, rng, RI(1, 2), 0.4, "rift", 0.22);
    } else if (v == 3) {
        s2_cracks(s, rng, RI(3, 4), 0.45, "rift", 0.22);
        bl_thorn_clumps(s, rng, RI(2, 4), ring_);
        s2_banks(s, rng, 1, 1.4, 2.0, 0.5, 0.9, "fleshrock", S2_SOFTT, 2, ring_);
    } else s2_floor(s, D, 0.55, "fleshrock", s2_j_voronoi(rng, RI(9, 14), 0.34, 0.9), 0.5, 0.5);
    if (fig) bl_figure_ground(s, rng, theme, keep, ring_, v == 0 || v == 4);
    else rocks_on(s, rng, 1, 3, 1.0, 1.7, 0.7, 1.2, "blightrock");
    s2_grit(s, rng, 10, 20, "grit");
    return s;
}

/* ================================================================ style_ferrum */
/* style_ferrum._rivets(phi, sx, sy, R, inset_=0.75) (NULL = None) */
static Geo *fe_rivets(double phi, double sx, double sy, double R) {
    double E0 = 22 * s2_KK(), ins = 0.75, lim = ly_pow(R + 1, 2.0); int64_t i0, j0, i, j, i1, j1; GeoList *pts = LL(); int k;
    i0 = ly_int((E0 - R - sx) / sx); if (i0 < 0) i0 = 0;
    j0 = ly_int((E0 - R - sy) / sy); if (j0 < 0) j0 = 0;
    i1 = ly_int((E0 + R) / sx) + 2; j1 = ly_int((E0 + R) / sy) + 2;
    for (i = i0; i < i1; i++) for (j = j0; j < j1; j++) {
        double x = -E0 + i * sx, y = -E0 + j * sy, UV[12] = { ins, ins, sx - ins, ins, ins, sy - ins, sx - ins, sy - ins, sx / 2, ins, sx / 2, sy - ins };
        for (k = 0; k < 6; k++) { double px = x + UV[2 * k], py = y + UV[2 * k + 1]; if (px * px + py * py < lim) LP(pts, g_ptbuf(px, py, 0.24, 6)); }
    }
    return pts->n ? g_rot(g_uu(pts), LF_DEGREES(phi), SHP_ORIGIN_POINT, 0, 0) : NULL;
}
static Geo *fe_grid(double phi, double sx, double sy, double R) {
    Geo *j = s2_j_grid(phi, sx, sy, 0.22), *r = fe_rivets(phi, sx, sy, R);
    return r ? g_union(j, r) : j;
}
Scene *new_ferrum(int n) {
    Scene *s = ly_mk("ferrum", "Ferrum Works", n, "#8a3f26"); PyRandom *rng = &s->rng; int v; Geo *D; S2Fr fr; double sx, sy, Rb;
    { static const double W[4] = { 35, 30, 15, 20 }; int64_t ix; if (py_choices_w(rng, 4, W, 1, &ix) != 0) ly_raise(); v = (int)ix; }
    D = s2_D(); fr = s2_fr(rng);
    sx = U(5.5, 6.5); sy = U(3.6, 4.4); Rb = R0 + 2;
    if (v == 0) {
        double c = U(-3, 2) * SC.K; Geo *reg = g_inter(s2_R(&fr, -30, 30, c, 30), D);
        s2_floor(s, reg, 0.5, "plate", fe_grid(fr.phi, sx, sy, Rb), 0.32, 0.5);
        s2_pools(s, rng, RI(1, 2), 1.4, 2.2, 0.4, "tar", g_buf(reg, 1.0));
        s2_banks(s, rng, RI(1, 2), 1.4, 2.2, 0.4, 0.7, "dust", T2550, 2, g_diff(dr(R0 - 0.8), g_buf(reg, 0.6)));
    } else if (v == 1) {
        double E0 = 22 * s2_KK(), c; GeoList *cells = LL(); int k, nk = RI(1, 3) + (SC.K > 1.5); Geo *gone, *reg, *tr;
        for (k = 0; k < nk; k++) {
            double x0, y0, uv[8];
            x0 = -E0 + sx * ly_rint((E0 + U(-0.7, 0.7) * R0) / sx);
            y0 = -E0 + sy * ly_rint((E0 + U(-0.7, 0.7) * R0) / sy);
            uv[0] = x0; uv[1] = y0; uv[2] = x0 + sx; uv[3] = y0; uv[4] = x0 + sx; uv[5] = y0 + sy; uv[6] = x0; uv[7] = y0 + sy;
            LP(cells, g_frpoly(&fr.f, uv, 4));
        }
        gone = g_bufx(g_uu(cells), 0.25, 16, SHP_CAP_ROUND, SHP_JOIN_MITRE, 5.0);
        reg = g_diff(D, gone);
        c = U(-4, 4) * SC.K; tr = g_inter(s2_R(&fr, -30, 30, c - 0.6, c + 0.6), D);
        s2_floor(s, g_diff(reg, g_buf(tr, 0.3)), 0.5, "plate", fe_grid(fr.phi, sx, sy, Rb), 0.32, 0.5);
        S_gcut(s, L1(tr), NF(0.4), "slot");
    } else if (v == 2) {
        s2_banks(s, rng, RI(2, 4), 1.6, 2.8, 0.4, 0.8, "dust", T2550, 2, NULL);
        s2_pools(s, rng, RI(1, 3), 1.2, 2.0, 0.35, "tar", NULL);
        s2_cracks(s, rng, RI(1, 3), 0.35, "crack", 0.22);
    } else {
        double c = U(-4, 1) * SC.K; Geo *reg = g_inter(s2_R(&fr, -30, 30, c, 30), D);
        s2_floor(s, reg, 0.45, "basalt", s2_j_voronoi(rng, RI(12, 18), 0.3, 0.5), 0.4, 0.5);
        S_gcut(s, L1(g_inter(s2_R(&fr, -30, 30, c - 2.2, c - 0.7), D)), NF(0.4), "tar");
    }
    rocks_on(s, rng, 2, 4, 0.9, 1.6, 0.6, 1.1, "slag");
    s2_grit(s, rng, 10, 20, "grit");
    return s;
}

/* ================================================================ style_obelis */
static const double OBQ[4] = { -0.4, -0.133, 0.133, 0.4 };          /* _Q */
/* style_obelis._stroke(pts, w) */
static Geo *ob_stroke(const double *xy, int n, double w) { return LA(shp_line_buffer(xy, n, w / 2, SHP_CAP_FLAT, SHP_JOIN_MITRE, 16, 4.0)); }
typedef struct { double cx, cy, c, sn, w; } ObP;
static Geo *ob_L(const ObP *P, const double *uv, int n) {          /* L(pts): _stroke([P(u, v) ...], w) */
    double xy[32]; int i;
    for (i = 0; i < n; i++) { xy[2 * i] = P->cx + uv[2 * i] * P->c - uv[2 * i + 1] * P->sn; xy[2 * i + 1] = P->cy + uv[2 * i] * P->sn + uv[2 * i + 1] * P->c; }
    return ob_stroke(xy, n, P->w);
}
/* style_obelis.ob_glyph(cx, cy, s, a, rng, w=0.25) */
static Geo *ob_glyph(double cx, double cy, double s, double a, PyRandom *rng, double w) {
    double q[4], lo, hi; ObP P; GeoList *g = LL(); int i;
    w = 0.14 * s < w ? 0.14 * s : w;          /* min(w, 0.14 * s) */
    for (i = 0; i < 4; i++) q[i] = OBQ[i] * s;
    P.cx = cx; P.cy = cy; P.c = COS(a); P.sn = SIN(a); P.w = w;
    lo = q[0]; hi = q[3];
    if (RND() < 0.6) {          /* the bracket: three sides of the square (seven when k == 3: the slices overlap) */
        double sides[4][4] = { { lo, hi, lo, lo }, { lo, lo, hi, lo }, { hi, lo, hi, hi }, { hi, hi, lo, hi } }, uv[32]; int k = RI(0, 3), run[8], nr = 0, j, np = 0;
        for (j = (k + 1) % 4; j < 4; j++) run[nr++] = j;
        for (j = 0; j < k; j++) run[nr++] = j;
        uv[np++] = sides[run[0]][0]; uv[np++] = sides[run[0]][1];
        for (j = 0; j < nr; j++) { uv[np++] = sides[run[j]][2]; uv[np++] = sides[run[j]][3]; }
        LP(g, ob_L(&P, uv, np / 2));
    } else {
        double uv[10] = { lo, lo, hi, lo, hi, hi, lo, hi, lo, lo };
        LP(g, ob_L(&P, uv, 5));
        if (RND() < 0.5) { double m = s * 0.18, u2[10] = { -m, -m, m, -m, m, m, -m, m, -m, -m }; LP(g, ob_L(&P, u2, 5)); }
    }
    {
        int64_t marks[2]; int nm = RI(1, 2), j;
        if (py_sample_e(rng, 6, nm, marks) != 0) ly_raise();
        for (j = 0; j < nm; j++) {
            int k = (int)marks[j];
            if (k == 0) { double v = py_choice(rng, 2) ? q[2] : q[1], e, uv[4]; e = py_choice(rng, 2) ? hi : q[2]; uv[0] = lo; uv[1] = v; uv[2] = e; uv[3] = v; LP(g, ob_L(&P, uv, 2)); }
            else if (k == 1) { double u = py_choice(rng, 2) ? q[2] : q[1], e, uv[4]; e = py_choice(rng, 2) ? q[2] : q[1]; uv[0] = u; uv[1] = lo; uv[2] = u; uv[3] = e; LP(g, ob_L(&P, uv, 2)); }
            else if (k == 2) { double uv[12] = { lo, lo, q[1], lo, q[1], q[1], q[2], q[1], q[2], q[2], hi, q[2] }; LP(g, ob_L(&P, uv, 6)); }
            else if (k == 3) {
                int sv = py_choice(rng, 2) ? 1 : -1;
                if (RND() < 0.5) { double uv[6] = { q[1], sv * q[2], 0, -sv * q[1] * 0.5, q[2], sv * q[2] }; LP(g, ob_L(&P, uv, 3)); }
                else { double uv[6] = { lo * 0.6, sv * q[2], 0, -sv * q[1], hi * 0.6, sv * q[2] }; LP(g, ob_L(&P, uv, 3)); }
            } else if (k == 4) {
                double u = py_choice(rng, 2) ? q[2] : q[1], v, d = s * 0.09, uv[8], xy[8]; int t;
                v = py_choice(rng, 2) ? q[2] : q[1];
                uv[0] = u - d; uv[1] = v - d; uv[2] = u + d; uv[3] = v - d; uv[4] = u + d; uv[5] = v + d; uv[6] = u - d; uv[7] = v + d;
                for (t = 0; t < 4; t++) { xy[2 * t] = cx + uv[2 * t] * P.c - uv[2 * t + 1] * P.sn; xy[2 * t + 1] = cy + uv[2 * t] * P.sn + uv[2 * t + 1] * P.c; }
                LP(g, g_poly(xy, 4));
            } else { double uv[6] = { q[1], q[1], q[2], q[1], q[2], q[2] }; LP(g, ob_L(&P, uv, 3)); }
        }
    }
    return g_uu(g);
}
/* style_obelis.ob_band(cx, cy, a, L, cell, rng, w=0.25, border=True) */
static Geo *ob_band(double cx, double cy, double a, double L, double cell, PyRandom *rng, double w) {
    double c = COS(a), sn = SIN(a); int64_t n = ly_int(L / (cell * 1.12)), i; GeoList *g = LL(); int sv;
    if (n < 1) n = 1;
    for (i = 0; i < n; i++) { double u = (i - (n - 1) / 2.0) * cell * 1.12; LP(g, ob_glyph(cx + u * c, cy + u * sn, cell, a, rng, w)); }
    for (sv = -1; sv <= 1; sv += 2) {
        double v = sv * cell * 0.72, h = n * cell * 1.12 / 2, xy[4] = { cx - h * c - v * sn, cy - h * sn + v * c, cx + h * c - v * sn, cy + h * sn + v * c };
        LP(g, ob_stroke(xy, 2, w));
    }
    return g_uu(g);
}
/* style_obelis._chamfer(g, c) */
static Geo *ob_chamfer(Geo *g, double c) { return g_bufx(g_bufx(g, -c, 16, SHP_CAP_ROUND, SHP_JOIN_MITRE, 5.0), c, 16, SHP_CAP_ROUND, SHP_JOIN_BEVEL, 5.0); }
/* style_obelis._inlay(s, rng, sl, z, phi, n=2, cell=1.9) */
static void ob_inlay(Scene *s, PyRandom *rng, Geo *sl, double z, double phi, int64_t n) {
    Geo *keep; GeoList *gs = LL(); int64_t i;
    if (!sl || g_isempty(sl)) return;
    keep = g_inset(sl, 0.7);
    for (i = 0; i < n; i++) {
        double cx, cy, a, L; Geo *g;
        lf_rp(rng, 0, 9, &cx, &cy);
        a = phi + (py_choice(rng, 2) ? PI / 2 : 0.0);
        L = U(6, 10) * ly_pow(s2_KK(), 0.5);
        g = g_inter(ob_band(cx, cy, a, L, 1.9, rng, 0.3), keep);
        if (!g_isempty(g)) LP(gs, g);
    }
    if (gs->n) S_tcut(s, L1(g_uu(gs)), NF(z - 0.3), NF(z), "slot");
}
/* style_obelis._grid(phi, sx, sy=None, jw=0.22): the joints as separate pieces */
static GeoList *ob_grid(double phi, double sx, double sy, double jw) {
    double E = 22 * s2_KK(), *ys, *xs; int ny, nx, i, j; GeoList *hs = LL(), *out = LL(); Geo *hu;
    if (sy == 0) sy = sx;
    ny = ly_arange(-E, E, sy, &ys);
    for (i = 0; i < ny; i++) LP(hs, g_box(-E, ys[i] - jw, E, ys[i] + jw));
    hu = g_buf(g_uu(hs), 0.01);
    for (i = 0; i < hs->n; i++) LP(out, g_rot(hs->v[i], LF_DEGREES(phi), SHP_ORIGIN_POINT, 0, 0));
    nx = ly_arange(-E, E, sx, &xs);
    for (i = 0; i < nx; i++) { GeoList *p = g_polys(g_diff(g_box(xs[i] - jw, -E, xs[i] + jw, E), hu)); for (j = 0; j < p->n; j++) LP(out, g_rot(p->v[j], LF_DEGREES(phi), SHP_ORIGIN_POINT, 0, 0)); }
    return out;
}
/* style_obelis._jfloor(s, region, h, kind, joints, depth=0.38, edge=0.5) */
static Geo *ob_jfloor(Scene *s, Geo *region, double h, const char *kind, GeoList *joints) {
    Geo *sl = s2_floor(s, region, h, kind, NULL, 0.38, 0.5), *keep; GeoList *ps = LL(); int i, j;
    if (g_isempty(sl)) return sl;
    keep = g_buf(sl, -0.5);
    for (i = 0; i < joints->n; i++) { GeoList *q = g_polys(g_inter(joints->v[i], keep)); for (j = 0; j < q->n; j++) if (g_area(q->v[j]) > 0.05) LP(ps, q->v[j]); }
    if (ps->n) S_tcut(s, ps, NF(H + h - 0.38), NF(H + h), "joint");
    return sl;
}
static void ob_drifts(Scene *s, PyRandom *rng, int n, Geo *region) { s2_banks(s, rng, n, 1.8, 3.0, 0.4, 0.8, "sand", T2550, 2, region); }
/* style_obelis._rim_sand(rng, D, n) */
static Geo *ob_rim_sand(PyRandom *rng, Geo *D, int n) {
    GeoList *gs = LL(); int i;
    for (i = 0; i < n; i++) {
        double t = U(0, 6.283), d = R0 * U(0.85, 1.0), r = U(3.0, 4.5) * ly_pow(s2_KK(), 0.7);
        LP(gs, LAG(lf_blob(d * COS(t), d * SIN(t), r, rng, 4)));
    }
    return g_inter(g_uu(gs), D);
}
/* style_obelis._lay_sand(s, rng, sand) */
static void ob_lay_sand(Scene *s, PyRandom *rng, Geo *sand) {
    GeoList *p = g_polys(g_inter(sand, dr(R0 - 0.6))), *ps = LL(); int i;
    for (i = 0; i < p->n; i++) if (g_area(p->v[i]) > 2.0) LP(ps, p->v[i]);
    if (ps->n) { double h = U(0.5, 0.9); S_stack(s, ps, NF(h), T2550, 2, "sand", NONE, NULL); }
}
Scene *new_obelis(int n) {
    Scene *s = ly_mk("obelis", "Obelis Tombs", n, "#a89270"); PyRandom *rng = &s->rng; int v = RI(0, 4), i; Geo *D = s2_D(); S2Fr fr = s2_fr(rng); double phi = fr.phi;
    s->has_phi = 1; s->phi = NF(phi);          /* s.phi = phi (s.ob_v = v is not part of the scene dump) */
    if (v == 0) {
        Geo *sand = ob_rim_sand(rng, D, RI(1, 2) + (SC.K > 2)), *sl; double step = U(6.0, 7.5); int64_t k;
        sl = ob_jfloor(s, g_diff(D, g_buf(sand, 0.3)), 0.55, "obfloor", ob_grid(phi, step, 0, 0.22));
        k = SC.K > 1.2 ? RI(1, 2) + ly_int(0.8 * ly_pow(SC.K, 2.0)) : RI(1, 2);
        ob_inlay(s, rng, sl, H + 0.55, phi, k);
        ob_lay_sand(s, rng, sand);
    } else if (v == 1) {
        double c = U(-3, 3) * SC.K, w = U(4.2, 5.2) * ly_pow(s2_KK(), 0.5), sx, fx, fy; Geo *band = g_inter(s2_R(&fr, -30, 30, c - w, c + w), D), *sl, *g = NULL;
        sx = U(4.4, 5.4);
        sl = ob_jfloor(s, band, 0.6, "obfloor", ob_grid(phi, sx, 400, 0.22));
        lf_at(&fr.f, 0, c, &fx, &fy);
        if (!g_isempty(sl)) g = g_inter(ob_band(fx, fy, phi, 60 * s2_KK(), 1.9, rng, 0.3), g_inset(sl, 0.6));
        if (g && !g_isempty(g)) S_tcut(s, L1(g), NF(H + 0.3), NF(H + 0.6), "slot");
        ob_drifts(s, rng, RI(2, 3), g_diff(dr(R0 - 0.8), g_buf(band, 0.6)));
    } else if (v == 2) {
        double a = U(-3, 2) * SC.K, sx; Geo *reg = g_inter(s2_R(&fr, -30, 30, a, 30), D), *sl;
        sx = U(4.6, 5.4);
        sl = ob_jfloor(s, reg, 0.55, "obfloor", ob_grid(phi + 0.785, sx, 0, 0.22));
        if (SC.K > 1.2) ob_inlay(s, rng, sl, H + 0.55, phi + 0.785, ly_int(0.4 * ly_pow(SC.K, 2.0)));
        ob_drifts(s, rng, RI(2, 3), g_diff(dr(R0 - 0.8), g_buf(reg, 0.5)));
        s2_cracks(s, rng, 2, 0.35, "crack", 0.22);
    } else if (v == 3) {
        Geo *sand = ob_rim_sand(rng, D, RI(1, 2)), *sl, *keep, *sq; double cx, cy, st, r; int k = 0; GeoList *gs = LL(), *parts = LL(), *rings = LL();
        sl = s2_floor(s, g_diff(D, g_buf(sand, 0.3)), 0.55, "obfloor", NULL, 0.38, 0.5);
        lf_rp(rng, 0, 3, &cx, &cy);
        st = 3.0 * ly_pow(s2_KK(), 0.5); r = st;
        while (r < R0 * 1.3) {
            Geo *q = LAG(lf_rect(cx, cy, 2 * r, 2 * r, phi + (k % 2 ? PI / 4 : 0.0)));
            LP(gs, g_bufx(g_ext(q), 0.2, 16, SHP_CAP_FLAT, SHP_JOIN_MITRE, 5.0)); r += st; k++;
        }
        for (i = 0; i < 4; i++) {
            double xy[4] = { cx + st * COS(phi + i * PI / 2), cy + st * SIN(phi + i * PI / 2), cx + 3 * R0 * COS(phi + i * PI / 2), cy + 3 * R0 * SIN(phi + i * PI / 2) };
            LP(gs, ob_stroke(xy, 2, 0.4));
        }
        keep = g_buf(sl, -0.6);
        for (i = 0; i < k; i++) LP(rings, gs->v[i]);
        sq = g_uu(rings);
        for (i = 0; i < k; i++) gl_extend(parts, g_polys(g_inter(gs->v[i], keep)));
        for (i = k; i < gs->n; i++) gl_extend(parts, g_polys(g_inter(g_diff(gs->v[i], sq), keep)));
        S_tcut(s, parts, NF(H + 0.2), NF(H + 0.55), "joint");
        if (SC.K > 1.2) ob_inlay(s, rng, g_diff(sl, g_buf(sq, 0.5)), H + 0.55, phi, ly_int(0.4 * ly_pow(SC.K, 2.0)));
        ob_lay_sand(s, rng, sand);
    } else {
        int64_t m = s2_NN(RI(3, 5)), t; GeoList *got = LL(), *rl = LL(), *bl = LL(); double ga[256]; Geo *rip, *patch; double *ys; int ny;
        for (t = 0; t < m * 25; t++) {
            double cx, cy, a, l, w; Geo *q; int ok;
            if (got->n >= m) break;
            lf_rp(rng, 0, 11, &cx, &cy);
            a = phi + U(-0.25, 0.25);
            a = a + (py_choice(rng, 2) ? PI / 2 : 0);
            l = U(4.0, 6.0); w = U(3.0, 4.2);
            q = ob_chamfer(LAG(lf_rect(cx, cy, l, w, a)), 0.35);
            ok = g_contains(dr(R0 - 1.0), q);
            if (ok) for (i = 0; i < got->n; i++) if (!(g_dist(q, got->v[i]) > 1.0)) { ok = 0; break; }
            if (ok) { if (got->n >= 256) ly_raise(); ga[got->n] = a; LP(got, q); }
        }
        for (i = 0; i < got->n; i++) {
            Geo *q = got->v[i]; double h = U(0.4, 0.6); GeoList *top = PR(s, L1(q), H + h, "obfloor");
            if (top->n && RND() < 0.7) {
                Geo *c = g_centroid(q), *g; double sz = sqrt(g_area(q)) * 0.6;
                g = g_inter(ob_glyph(g_x(c), g_y(c), 2.6 < sz ? 2.6 : sz, ga[i], rng, 0.3), g_inset(q, 0.4));
                if (!g_isempty(g)) S_tcut(s, L1(g), NF(H + h - 0.3), NF(H + h), "slot");
            }
        }
        { double st = U(1.4, 1.8); ny = ly_arange(-30 * s2_KK(), 30 * s2_KK(), st, &ys); }
        for (i = 0; i < ny; i++) { double xy[4]; lf_at(&fr.f, -40, ys[i], &xy[0], &xy[1]); lf_at(&fr.f, 40, ys[i], &xy[2], &xy[3]); LP(rl, g_linebuf(xy, 2, 0.18, SHP_CAP_ROUND, 16)); }
        rip = g_uu(rl);
        { double px, py, r; lf_rp(rng, 2, 8, &px, &py); r = U(4.0, 6.0) * ly_pow(s2_KK(), 0.6); patch = g_inter(g_ptbuf(px, py, r, 16), dr(R0 - 1.0)); }
        for (i = 0; i < got->n; i++) LP(bl, g_buf(got->v[i], 0.6));
        rip = g_diff(g_inter(rip, patch), got->n ? g_uu(bl) : g_empty());
        if (!g_isempty(rip)) S_gcut(s, L1(rip), NF(0.25), "rut");
        ob_drifts(s, rng, RI(2, 3), NULL);
    }
    s2_grit(s, rng, 4, 10, "grit");
    return s;
}

/* ================================================================ style_sundered */
/* style_sundered.su_crater(s, rng, cx, cy, r, depth=0.75) */
static Geo *su_crater(Scene *s, PyRandom *rng, double cx, double cy, double r, double depth) {
    static const Tier TR[] = { TI(0.65, 8), TI(0.35, 40) };
    Geo *hole; GeoList *lip = LL(), *cr = LL(), *ps, *rocks = LL(); int k, nk; Geo *dk = dr(R0 - 0.3);
    { double asp = U(0.8, 0.95); hole = g_stone(cx, cy, r, rng, asp, 10); }
    nk = RI(7, 11);
    for (k = 0; k < nk; k++) { double t = U(0, 6.283), dd = r + U(0.35, 1.0), rr = U(0.55, 0.95); LP(lip, g_stone(cx + dd * COS(t), cy + dd * SIN(t), rr, rng, 0.8, 5)); }
    nk = RI(4, 6);
    for (k = 0; k < nk; k++) {
        double t = U(0, 6.283), L = U(2.0, 4.0);
        LP(cr, g_bufx(g_wigline(cx + r * 0.8 * COS(t), cy + r * 0.8 * SIN(t), cx + (r + L) * COS(t), cy + (r + L) * SIN(t), rng, 0.3, 3), 0.22, 3, SHP_CAP_ROUND, SHP_JOIN_ROUND, 5.0));
    }
    ps = g_polys(g_uu(lip));
    for (k = 0; k < ps->n; k++) if (g_contains(dk, ps->v[k])) LP(rocks, ps->v[k]);
    { double h = U(0.55, 0.85); S_stack(s, rocks, NF(h), TR, 2, "rubble", NONE, NULL); }
    S_gcut(s, L1(hole), NF(depth), "pothole"); S_gcut(s, L1(g_uu(cr)), NF(0.4), "crack");
    return hole;
}
/* style_sundered.su_lifted(s, rng, sl, n) */
static void su_lifted(Scene *s, PyRandom *rng, Geo *sl, int n) {
    Geo *reg, *q; double b[4]; int got = 0, i; int64_t t, m; GeoList *cs = LL();
    if (g_isempty(sl)) return;
    reg = g_inset(sl, 1.2); g_bounds(sl, b);
    for (i = 0; i < n * 20; i++) {
        double x, y, l, w, a; Geo *g;
        if (got >= n || g_isempty(reg)) break;
        x = U(b[0], b[2]); y = U(b[1], b[3]); l = U(2.4, 3.6); w = U(2.0, 3.0); a = U(0, 3.14);
        g = LAG(lf_rect(x, y, l, w, a));
        if (g_contains(reg, g)) { S_tcut(s, L1(g), NF(H + 0.12), NF(H + 0.6), "pothole"); got++; }
    }
    m = ly_rint(2 * s2_KK()); if (m < 2) m = 2;
    for (t = 0; t < m; t++) {
        double ax, ay, tt, L;
        lf_rp(rng, 0, 11, &ax, &ay); tt = U(0, 6.283); L = U(6, 14);
        LP(cs, g_bufx(g_wigline(ax, ay, ax + L * COS(tt), ay + L * SIN(tt), rng, 0.6, 6), 0.22, 3, SHP_CAP_ROUND, SHP_JOIN_ROUND, 5.0));
    }
    q = g_inter(g_uu(cs), g_inset(sl, 0.5));
    if (!g_isempty(q)) S_tcut(s, L1(q), NF(H + 0.2), NF(H + 0.6), "crack");
}
Scene *new_sundered(int n) {
    Scene *s = ly_mk("sundered", "Sundered City", n, "#5a5249"); PyRandom *rng = &s->rng; int v = RI(0, 3); Geo *D = s2_D(); S2Fr fr = s2_fr(rng); double phi = fr.phi;
    static const Tier TS[] = { TI(0.55, 20), TI(0.45, 45) };
    if (v == 0) {
        double w = U(3.4, 4.2), L = U(5.0, 6.5); Geo *sl = s2_floor(s, D, 0.55, "flag", s2_j_planks(phi, w, L, 0.24), 0.38, 0.5);
        su_lifted(s, rng, sl, RI(1, 3));
    } else if (v == 1) {
        double c = U(-3, 2) * SC.K, a, b; Geo *sl, *ln; GeoList *kb;
        a = U(3.0, 3.6); b = U(4.2, 5.0);
        sl = s2_floor(s, g_inter(s2_R(&fr, -30, 30, c + 1.0, 30), D), 0.5, "flag", s2_j_grid(phi, a, b, 0.22), 0.38, 0.5);
        kb = PR(s, L1(g_inter(s2_R(&fr, -30, 30, c - 0.1, c + 1.1), D)), H + 0.8, "ashlar");
        if (kb->n) { double sy = U(3.4, 4.4); S_tcut(s, L1(g_inter(g_inter(s2_j_grid(phi, 30, sy, 0.22), s2_R(&fr, -30, 30, c - 0.5, c + 1.5)), dr(R0 - 0.6))), NF(H + 0.5), NF(H + 0.8), "joint"); }
        su_lifted(s, rng, sl, 1);
        { double off = c - U(3.5, 5.5); ln = LAG(lf_meander(rng, phi, off, 0.8, 1.0, 40)); }
        S_gcut(s, L1(g_bufx(ln, 0.5, 16, SHP_CAP_FLAT, SHP_JOIN_ROUND, 5.0)), NF(0.35), "rut");
        s2_cracks(s, rng, RI(2, 3), 0.4, "crack", 0.22);
    } else if (v == 2) {
        double a = U(-2, 4) * SC.K; Geo *reg = g_inter(s2_R(&fr, -30, 30, a, 30), D); int64_t k, m;
        s2_floor(s, reg, 0.5, "flag", s2_j_voronoi(rng, RI(16, 24), 0.24, 0.25), 0.38, 0.5);
        m = s2_NN(RI(1, 2));
        for (k = 0; k < m; k++) {
            int t;
            for (t = 0; t < 40; t++) {
                double cx, cy, r;
                lf_rp(rng, 0, 8, &cx, &cy); r = U(1.9, 2.7);
                if (g_contains(dr(R0 - 1.5), g_ptbuf(cx, cy, r + 1.2, 16)) && S_free(s, g_ptbuf(cx, cy, r + 1.5, 16), 0.3)) { su_crater(s, rng, cx, cy, r, 0.75); break; }
            }
        }
        s2_cracks(s, rng, 2, 0.4, "crack", 0.22);
    } else {
        double a = U(-1, 4) * SC.K; Geo *reg = g_inter(s2_R(&fr, -30, 30, a, 30), D);
        s2_floor(s, reg, 0.5, "flag", s2_j_voronoi(rng, RI(18, 26), 0.22, 0.2), 0.38, 0.5);
        s2_banks(s, rng, RI(2, 3), 1.8, 2.8, 0.5, 0.9, "spoil", TS, 2, g_diff(dr(R0 - 0.8), g_buf(reg, 0.5)));
        s2_cracks(s, rng, 2, 0.4, "crack", 0.22);
    }
    rocks_on(s, rng, 2, 4, 0.7, 1.2, 0.45, 0.8, "rubble");
    s2_grit(s, rng, 8, 16, "grit");
    return s;
}

/* ================================================================ style_thornwild */
Scene *new_thornwild(int n) {
    Scene *s = ly_mk("thornwild", "Thornwild", n, "#3c4629"); PyRandom *rng = &s->rng; int v = RI(0, 3); double K = s2_KK();
    static const Tier TR[] = { TI(0.45, 18), TI(0.35, 40), TI(0.2, 62) }, TN[] = { TI(0.5, 20), TI(0.5, 50) };
    if (v == 0) {
        double a0 = U(0, 6.28), cx = 14.0 * SC.K * COS(a0), cy = 14.0 * SC.K * SIN(a0); static const double DA[5] = { -0.75, -0.35, 0.05, 0.45, 0.8 }; GeoList *roots = LL(); int nd = RI(3, 5), d;
        for (d = 0; d < nd; d++) {
            double L = (17 + U(0, 9)) * SC.K, ex = cx - L * COS(a0 + DA[d]), ey = cy - L * SIN(a0 + DA[d]), pts[16]; int np = lf_wiggle(cx, cy, ex, ey, rng, 1.4, 7, pts), i; GeoList *seg = LL();
            for (i = 0; i < np - 1; i++) { double w = U(0.9, 1.3) * (1.0 - 0.45 * i / np) * ly_pow(K, 0.5); LP(seg, g_linebuf(&pts[2 * i], 2, w, SHP_CAP_ROUND, 4)); }
            LP(roots, g_uu(seg));
        }
        S_stack(s, L1(g_uu(roots)), NF(1.1), TR, 3, "root", NONE, NULL);
    } else if (v == 1) s2_channel(s, rng, U(2.6, 3.6), 0.5, "bile", 4.0);
    else if (v == 2) s2_pools(s, rng, RI(1, 3), 1.6, 2.6, 0.5, "bile", NULL);
    else {
        Geo *net = g_inter(s2_j_voronoi(rng, RI(7, 11), 0.42, 0.9), dr(R0 - 1.2));
        if (!g_isempty(net)) S_stack(s, L1(net), NF(0.5), TN, 2, "root", NONE, NULL);
    }
    s2_banks(s, rng, RI(1, 3), 1.5, 2.4, 0.4, 0.8, "moss", NULL, 0, NULL);
    rocks_on(s, rng, 1, 3, 0.9, 1.5, 0.6, 1.0, "rock");
    s2_grit(s, rng, 10, 22, "grit");
    return s;
}
