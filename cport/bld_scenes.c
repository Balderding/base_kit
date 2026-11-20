/* bld_scenes.c: scenes.py's builders, subjects and props in C (see bld_scenes.h). Each C function names the Python
   it copies. Random numbers are drawn in exactly the Python's order: every draw is its own statement (C does not order
   the arguments of a call), conditions before the values they guard, list items left to right. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdarg.h>
#include "bld_scenes.h"
#include "bld_details.h"

#define UNI(a, b) py_uniform(rng, (a), (b))
#define RND() py_random(rng)
#define COS py_cos
#define SIN py_sin
#define SUP() (SO.supports)

/* ================================================================ temporaries freed with the builder's frame */
typedef struct { void *p; void (*f)(void *); } Tmp;
static Tmp *TMPS; static int ntmp, captmp;
static void tmp_push(void *p, void (*f)(void *)) {
    if (ntmp == captmp) { captmp = captmp ? 2 * captmp : 256; TMPS = (Tmp *)realloc(TMPS, sizeof(Tmp) * (size_t)captmp); }
    TMPS[ntmp].p = p; TMPS[ntmp].f = f; ntmp++;
}
static void tmp_pop(int mark) { while (ntmp > mark) { ntmp--; TMPS[ntmp].f(TMPS[ntmp].p); } }
static void solf(void *p) { sol_free((SoSolids *)p); }
static void lylf(void *p) { lyl_free((SoLayers *)p); }
static void geof(void *p) { geo_unref((Geo *)p); }
static SoSolids *SL(void) { SoSolids *s = (SoSolids *)LM(sizeof *s); tmp_push(s, solf); return s; }
static SoLayers *LYS(void) { SoLayers *l = (SoLayers *)LM(sizeof *l); tmp_push(l, lylf); return l; }
static Geo *own(Geo *g) { if (g) tmp_push(g, geof); return g; }          /* a NEW reference that may be None */

/* ================================================================ small helpers */
static double kk_(double k, double e) { return bs_pow(k > 1.0 ? k : 1.0, e); }          /* _kk(k, e) = max(1.0, k) ** e */
static double mx(double a, double b) { return b > a ? b : a; }                         /* max(a, b): the first of equal */
static double mn(double a, double b) { return b < a ? b : a; }                         /* min(a, b) */
static const char *I_(const char *s) { const char *r; if (!s) return NULL; r = so_intern(s); if (!r) ly_raise(); return r; }
static const char *KF(const char *k) { Kind *r; if (!k) return NULL; r = sm_kfix(kind_str(k)); return I_(r->s); }          /* kfix */
static const char *OR2(const char *a, const char *b) { return a && *a ? a : b; }        /* a or b */
static Geo *rect_(double cx, double cy, double l, double w, double a) { return LAG(lf_rect(cx, cy, l, w, a)); }
static Geo *rectf(const Frame *f, double u, double v, double l, double w, double a) { double x, y; lf_at(f, u, v, &x, &y); return rect_(x, y, l, w, a); }
static Geo *disc_(double x, double y, double r, int n) { return LA(so_disc(x, y, r, n)); }
static Geo *ann_(double x, double y, double ro, double ri, int n) { return LA(so_ann(x, y, ro, ri, n)); }
static Geo *bar_(const double *p, const double *q, double w) { return LA(so_bar(p, q, w)); }
static Geo *cap_(const double *p, const double *q, double w) { return LA(so_cap(p, q, w)); }
static Geo *barf(const Frame *f, double u0, double v0, double u1, double v1, double w) { double p[2], q[2]; lf_at(f, u0, v0, &p[0], &p[1]); lf_at(f, u1, v1, &q[0], &q[1]); return bar_(p, q, w); }
static Geo *capf(const Frame *f, double u0, double v0, double u1, double v1, double w) { double p[2], q[2]; lf_at(f, u0, v0, &p[0], &p[1]); lf_at(f, u1, v1, &q[0], &q[1]); return cap_(p, q, w); }
static Geo *U_(GeoList *l) { const GEOSGeometry **v = (const GEOSGeometry **)LM(sizeof(void *) * (size_t)(l->n + 1)); int i; for (i = 0; i < l->n; i++) v[i] = l->v[i] ? l->v[i]->g : NULL; return LA(so_u(v, l->n)); }   /* _u */
static Geo *U2_(Geo *a, Geo *b) { return U_(L2(a, b)); }
static Geo *hull_(Geo *g) { return g_hull(g); }
static Geo *stone_(double cx, double cy, double r, PyRandom *rng, double asp, int nv) { return g_stone(cx, cy, r, rng, asp, nv); }
static void FF(const Frame *f, double u, double v, double *p) { lf_at(f, u, v, &p[0], &p[1]); }
static SoFrame sof(double cx, double cy, double a) { return so_frame(cx, cy, a); }
static double *AR(int n) { return (double *)LM(sizeof(double) * (size_t)(n > 0 ? n : 1)); }
static int arange_(double a, double b, double st, double **out) { return ly_arange(a, b, st, out); }

/* layers (each geometry a new object, as features' builders make them) */
static SoVal GV(Geo *g) { GEOSGeometry *c; if (!g) return lv_none(); c = shp_clone(g->g); if (!c) ly_raise(); return lv_geom(c); }
static void Lup(SoLayers *L, Geo *g, double h, const char *k) { SoLayer *y = ly_new(4); y->v[0] = lv_str("up"); y->v[1] = GV(g); y->v[2] = lv_float(h); y->v[3] = lv_str(k); lyl_push_own(L, y); }
static void Lon(SoLayers *L, Geo *g, int64_t par, double h, const char *k) { SoLayer *y = ly_new(5); y->v[0] = lv_str("on"); y->v[1] = GV(g); y->v[2] = lv_int(par); y->v[3] = lv_float(h); y->v[4] = lv_str(k); lyl_push_own(L, y); }
static void Ltcut(SoLayers *L, Geo *g, int64_t par, double d) { SoLayer *y = ly_new(4); y->v[0] = lv_str("tcut"); y->v[1] = GV(g); y->v[2] = lv_int(par); y->v[3] = lv_float(d); lyl_push_own(L, y); }
static void Lgcut(SoLayers *L, Geo *g, double d, const char *k) { SoLayer *y = ly_new(4); y->v[0] = lv_str("gcut"); y->v[1] = GV(g); y->v[2] = lv_float(d); y->v[3] = lv_str(k); lyl_push_own(L, y); }
static void Lmound(SoLayers *L, Geo *g, double h, const char *k, const FtTiers *t) {
    SoLayer *y = ly_new(5); y->v[0] = lv_str("mound"); y->v[1] = GV(g); y->v[2] = lv_float(h); y->v[3] = lv_str(k); y->v[4] = lv_none(); y->v[4].t = LV_PTR; y->v[4].p = (void *)t; lyl_push_own(L, y);
}
/* _steps(solids, kind, parent, cuts) appended */
static void Lsteps(SoLayers *L, const SoSolids *sol, const char *kind, int has_par, int64_t par, const SoCut *cuts, int nc) {
    SoLayer *y = NULL;
    if (so_steps(sol, kind, has_par, par, cuts, nc, &y) != SO_OK || !y) ly_raise();
    lyl_push_own(L, y);
}
/* L[:2] + (L[2] + n0,) + L[3:] for an "on" / "tcut" layer, else L itself: appended to lay (_join, _several) */
static void Lshift(SoLayers *lay, const SoLayers *ly, int64_t n0) {
    int i, j;
    for (i = 0; i < ly->n; i++) {
        SoLayer *l = ly->v[i];
        if (l && l->n > 2 && l->v[0].t == LV_STR && (!strcmp(l->v[0].s, "on") || !strcmp(l->v[0].s, "tcut"))) {
            SoLayer *y = ly_new(l->n);
            for (j = 0; j < l->n; j++) {
                SoVal v = l->v[j];
                if (v.t == LV_GEOM) { v = lv_geom(v.g ? shp_clone(v.g) : NULL); if (l->v[j].g && !v.g) { ly_unref(y); ly_raise(); } }
                else if (v.t == LV_LATER) v = lv_later(so_later_ref(v.later));
                y->v[j] = v;
            }
            if (y->v[2].t == LV_INT) y->v[2].i += n0; else if (y->v[2].t == LV_FLOAT) y->v[2].f += (double)n0; else { ly_unref(y); ly_raise(); }
            lyl_push_own(lay, y);
        } else lyl_push(lay, l);
    }
}
static void Ljoin(SoLayers *lay, const SoLayers *more) { Lshift(lay, more, lay->n); }          /* _join(lay, more) */

/* solids */
static void VAULT(SoSolids *o, double cx, double cy, double a, double L, double W, double h, const char *prof, double lift, int under, int has_ax, double ax) {
    if (so_vault(o, cx, cy, a, L, W, h, prof, lift, under, has_ax, ax) != SO_OK) ly_raise();
}
static void LATHE(SoSolids *o, double cx, double cy, const double *prof, int np, int seg, double squash, double a, double lift, const double *real, int nreal) {
    if (so_lathe(o, cx, cy, prof, np, seg, squash, a, lift, real, nreal) != SO_OK) ly_raise();
}
static void LOFT(SoSolids *o, double cx, double cy, double a, double L, SoNumFn f, void *c, double hk, const char *prof, double lift, int under) {
    if (so_loft(o, cx, cy, a, L, f, c, hk, prof, lift, under) != SO_OK) ly_raise();
}
static void TUBE(SoSolids *o, const double *pts, int n, double r, SoNumFn f, void *c, double hk, double lift, int under, double arch) {
    if (so_tube(o, pts, n, r, f, c, hk, lift, under, arch) != SO_OK) ly_raise();
}
static void SLAB(SoSolids *o, Geo *p, double z0, double z1) { if (so_slab(o, p->g, z0, z1) != SO_OK) ly_raise(); }
static void BOX(SoSolids *o, Geo *p, double h) { if (so_box(o, p->g, h, 0.0) != SO_OK) ly_raise(); }
static int CURVE(double r0, double r1, double z0, double z1, int n, int out_, double *prof) { if (so_curve(r0, r1, z0, z1, n, out_, prof) != SO_OK) ly_raise(); return n + 1; }
/* x ** y inside a lambda handed to loft / tube (no raise there: an error goes back as SO_ERR) */
#define PW(x, y, out) do { double re_ = 0, im_ = 0; if (px_pow((x), (y), &re_, &im_) < 0) return SO_ERR; (out) = re_; } while (0)

/* scenes.py's tiers */
#define TN(f, a) { { (f), NUM_FLOAT, 0 }, { (a), NUM_INT, 0 } }
static const FtTiers BAG_T = { 2, { TN(0.7, 8), TN(0.3, 40) } };          /* BAG_T */
#define SOFT_T (&FT_SOFT_T)
#define ROCK_T (&FT_ROCK_T)

/* ================================================================ the builder frame */
/* a builder body: fills *foot (an arena geometry or NULL) and lay; *none = 1 for a None result */
typedef void (*BsBody)(void *c, double cx, double cy, double a, PyRandom *rng, double k, Geo **foot, SoLayers *lay, int *none);
enum { K_NONE = 0, K_OPT = 1, K_REQ = 2 };
static int bs_run(BsBody body, int kmode, void *c, double cx, double cy, double a, PyRandom *rng, int hs, double k, Geo **foot, SoLayers *L) {
    BsFrame F; SoLayers lay = { 0 }; Geo *ft = NULL; int none = 0, tm = ntmp, i;
    *foot = NULL;
    if (kmode == K_NONE && hs) return SC_ERR;          /* f(cx, cy, a, rng) given a k: TypeError */
    if (kmode == K_REQ && !hs) return SC_ERR;
    if (!hs) k = 1.0;
    bs_begin(&F);
    if (setjmp(F.jb)) { tmp_pop(tm); bs_end(&F); lyl_free(&lay); return SC_ERR; }
    body(c, cx, cy, a, rng, k, &ft, &lay, &none);
    if (!none && ft) { GEOSGeometry *g = shp_clone(ft->g); if (!g) ly_raise(); *foot = geo_new(g); }
    tmp_pop(tm); bs_end(&F);
    if (none) { lyl_free(&lay); geo_unref(*foot); *foot = NULL; return 1; }
    for (i = 0; i < lay.n; i++) lyl_push(L, lay.v[i]);
    lyl_free(&lay);
    return 0;
}
/* calling another builder from inside a body: its foot (arena, may be NULL) and its layers (a temporary list) */
static int CALLB(const BsBuild *b, double cx, double cy, double a, PyRandom *rng, int hs, double k, Geo **foot, SoLayers **lay) {
    Geo *f = NULL; SoLayers *l = LYS(); int r = b->fn(b->ctx, cx, cy, a, rng, hs, k, &f, l);
    if (r < 0) { geo_unref(f); ly_raise(); }
    *foot = f ? LAG(f) : NULL; *lay = l;
    return r;          /* 1 = None */
}

/* ctx: strings and numbers a builder keeps from its factory */
typedef struct { const char *s[6]; double v[4]; BsBuild b[2]; int i[2]; } BC;
static BC *bc_new(void) { BC *c = (BC *)calloc(1, sizeof *c); bld_keep(c, free); return c; }
#define BUILDER(nm, km) \
    static void nm##_(BC *c, double cx, double cy, double a, PyRandom *rng, double k, Geo **foot, SoLayers *lay, int *none); \
    static int nm(void *c, double cx, double cy, double a, PyRandom *rng, int hs, double k, Geo **foot, SoLayers *L) { \
        return bs_run((BsBody)nm##_, km, c, cx, cy, a, rng, hs, k, foot, L); } \
    static void nm##_(BC *c, double cx, double cy, double a, PyRandom *rng, double k, Geo **foot, SoLayers *lay, int *none)

/* ================================================================ features' builders (through the table) */
static FtArg AS(const char *s) { FtArg a; memset(&a, 0, sizeof a); a.t = 's'; a.s = s; return a; }
static FtArg AF(double v) { FtArg a; memset(&a, 0, sizeof a); a.t = 'f'; a.n = 1; a.v[0] = v; return a; }
static FtArg AI(double v) { FtArg a; memset(&a, 0, sizeof a); a.t = 'i'; a.n = 1; a.v[0] = v; a.imask = 1; return a; }
static FtArg AT2(double x, double y, unsigned im) { FtArg a; memset(&a, 0, sizeof a); a.t = 'T'; a.n = 2; a.v[0] = x; a.v[1] = y; a.imask = im; return a; }
/* F.<name>(args...) -> a builder (raises if it is not there or its factory raised) */
static BsBuild FEAT(const char *name, int n, const FtArg *args) {
    FtCall c; BsBuild b; int r, i; char tag[128]; const char *kind = "";
    memset(&c, 0, sizeof c); memset(&b, 0, sizeof b);
    c.mod = "features"; c.name = name; c.nargs = n; for (i = 0; i < n && i < 6; i++) c.a[i] = args[i];
    r = bld_make(&c, &b.fn, &b.ctx);
    if (r != 0) ly_raise();
    if (!strncmp(name, "b_", 2)) {          /* features._tag_builders: .tag / .kind (scenes' b_skull / b_ribs keep their own: the same) */
        FtCall s2 = c; s2.mod = NULL; s2.name = name + 2; tag[0] = 0;
        ft_tag_kind(&s2, tag, sizeof tag, &kind); b.tag = I_(tag); b.kind = I_(kind ? kind : "");
    }
    return b;
}
static BsBuild FEAT1(const char *name, const char *s0) { FtArg a[1]; a[0] = AS(s0); return FEAT(name, 1, a); }
static BsBuild FEAT2(const char *name, const char *s0, const char *s1) { FtArg a[2]; a[0] = AS(s0); a[1] = AS(s1); return FEAT(name, 2, a); }

/* ================================================================ subjects: S_* */
/* S_portal(p): st = kfix(p["stone"]), fl = p["floor"] or p["stone"]; s[2] = p["pit"] */
BUILDER(S_portal, K_REQ) {
    Frame ff = lf_frame(cx, cy, a); double W, T, Dp, far, th, Rr; Geo *sill, *dark; GeoList *ft = LL(); int sv, i, n;
    const char *st = c->s[0], *fl = c->s[1];
    W = UNI(6.0, 7.4) * kk_(k, 0.8); T = 2.6 * kk_(k, 0.5); Dp = 3.6 * kk_(k, 0.5); far = 14 * kk_(k, 0.5);
    sill = rectf(&ff, Dp / 2 + 0.7, 0, 1.8, W + 1.4, a);
    { double uv[8] = { -far, -W / 2 + 0.15, Dp / 2 - 0.5, -W / 2 + 0.15, Dp / 2 - 0.5, W / 2 - 0.15, -far, W / 2 - 0.15 }; dark = g_frpoly(&ff, uv, 4); }
    Lgcut(lay, dark, 0.85, c->s[2]); Lup(lay, sill, 0.45, fl); LP(ft, sill);
    if (SUP()) {
        double hj = UNI(2.9, 3.4); Geo *js[2], *lin, *key; SoSolids *sol = SL();
        for (i = 0; i < 2; i++) { sv = i ? 1 : -1; js[i] = rectf(&ff, 0, sv * (W / 2 + T / 2), Dp, T, a); LP(ft, js[i]); }
        lin = rectf(&ff, 0, 0, Dp * 0.92, W + 2 * T + 0.6, a); key = rectf(&ff, 0, 0, Dp * 1.04, T * 0.7, a);
        BOX(sol, js[0], hj); BOX(sol, js[1], hj); SLAB(sol, lin, hj - 0.25, hj + 1.0); SLAB(sol, key, hj - 0.55, hj + 1.35);
        Lsteps(lay, sol, st, 0, 0, NULL, 0);
    } else {
        for (sv = -1; sv <= 1; sv += 2) {
            Geo *j = rectf(&ff, 0, sv * (W / 2 + T / 2), Dp, T, a); double h = UNI(2.3, 2.9); LP(ft, j);
            Lup(lay, j, h, st); Ltcut(lay, barf(&ff, 0, sv * W / 2, 0, sv * (W / 2 + T), 0.24), lay->n - 1, 0.22);
        }
    }
    th = py_atan2(cy, cx); Rr = SC.R0;
    for (sv = -1; sv <= 1; sv += 2) {
        double u = (W / 2 + T + 0.5) / Rr;
        n = (int)bs_randint(rng, 3, 4); n += (int)bs_int(1.5 * k);
        for (i = 0; i < n; i++) {
            double bl = UNI(2.8, 4.4), t = th + sv * (u + bl / 2 / Rr); Geo *q; double h;
            u += (bl + 0.1) / Rr;
            if (RND() < 0.15) continue;
            q = rect_((Rr - 1.6) * COS(t), (Rr - 1.6) * SIN(t), 3.2, bl - 0.15, t); LP(ft, q);
            h = UNI(1.0, 2.3) * (1 - 0.1 * i); Lup(lay, q, h, st);
        }
    }
    *foot = g_buf(g_buf(U_(ft), 0.9), -0.6);
}
/* S_ruin(p): st = kfix(p["stone"]), fl = p["floor"] or p["stone"] */
static void ruin_run(BC *c, PyRandom *rng, SoLayers *lay, double p0x, double p0y, double p1x, double p1y, double t) {          /* run(p0, p1) */
    double p0[2] = { p0x, p0y }, p1[2] = { p1x, p1y }, length = px_dist(p0, p1, 2), ang = py_atan2(p1[1] - p0[1], p1[0] - p0[0]), u = 0.0; Frame f2 = lf_frame(p0[0], p0[1], ang);
    while (u < length - 1.2) {
        double bl = mn(UNI(2.2, 3.8), length - u);
        if (RND() > 0.12) { Geo *r = rectf(&f2, u + bl / 2, 0, bl - 0.1, t - 0.35, ang); double h = UNI(0.7, 2.6); Lon(lay, r, 0, h, c->s[0]); }
        u += bl;
    }
}
BUILDER(S_ruin, K_REQ) {
    double L, W, t = 1.7, js, e, *us, x0, y0, x1, y1; Frame ff; Geo *flr; GeoList *bars = LL(); int i, n, sv;
    L = UNI(10, 12.5) * k * 0.85; W = UNI(9, 11.5) * k * 0.85; ff = lf_frame(cx, cy, a); flr = rect_(cx, cy, L, W, a);
    js = 2.2 * kk_(k, 0.3);
    n = arange_(-L / 2 + js, L / 2 - 0.5, js, &us); for (i = 0; i < n; i++) LP(bars, barf(&ff, us[i], -W, us[i], W, 0.26));
    n = arange_(-W / 2 + js, W / 2 - 0.5, js, &us); for (i = 0; i < n; i++) LP(bars, barf(&ff, -L, us[i], L, us[i], 0.26));
    Lup(lay, flr, 0.35, c->s[1]); Ltcut(lay, g_inter(U_(bars), g_inset(flr, t + 0.3)), 0, 0.25);
    e = t / 2;
    lf_at(&ff, -L / 2 + e, -W / 2 + e, &x0, &y0); lf_at(&ff, -L / 2 + e, W / 2 - e, &x1, &y1); ruin_run(c, rng, lay, x0, y0, x1, y1, t);
    for (sv = -1; sv <= 1; sv += 2) {
        if (RND() < 0.8) { double q; lf_at(&ff, -L / 2 + e, sv * (W / 2 - e), &x0, &y0); q = UNI(-0.2, 1.0); lf_at(&ff, L / 2 * q - e, sv * (W / 2 - e), &x1, &y1); ruin_run(c, rng, lay, x0, y0, x1, y1, t); }
        if (RND() < 0.7) { double q; lf_at(&ff, L / 2 - e, sv * (W / 2 - e), &x0, &y0); q = UNI(1.9, 2.6); lf_at(&ff, L / 2 - e, sv * q, &x1, &y1); ruin_run(c, rng, lay, x0, y0, x1, y1, t); }
    }
    *foot = flr;
}
/* S_shaft(p): wd = kfix(p["wood"]); s[1] = p["pit"] */
BUILDER(S_shaft, K_REQ) {
    Frame ff = lf_frame(cx, cy, a), lf; double S, t, sv, *us, x, y; Geo *outer, *hole, *ring_, *lad; GeoList *l = LL(); int su, sw, i, n; const char *wd = c->s[0];
    S = UNI(5.0, 6.2) * kk_(k, 0.8); t = 1.3 * kk_(k, 0.4);
    outer = rect_(cx, cy, S + 2 * t, S + 2 * t, a); hole = rect_(cx, cy, S, S, a);
    for (su = -1; su <= 1; su += 2) for (sw = -1; sw <= 1; sw += 2) LP(l, barf(&ff, su * S / 2, sw * S / 2, su * (S / 2 + t + 0.3), sw * (S / 2 + t + 0.3), 0.24));
    ring_ = g_diff(outer, hole);
    Lgcut(lay, g_inset(hole, 0.15), 0.9, c->s[1]); Lup(lay, ring_, 0.9, wd); Ltcut(lay, U_(l), 1, 0.25);
    for (sw = -1; sw <= 1; sw += 2) Lon(lay, rectf(&ff, -S * 0.15, sw * (S / 2 + t / 2), 1.25, 1.25, a), 1, 1.6, wd);
    Lup(lay, barf(&ff, -S * 0.15, -S / 2 - 0.1, -S * 0.15, S / 2 + 0.1, 1.2), 1.9, wd);
    sv = bs_choice_pm1(rng); lf_at(&ff, S * 0.2, sv * (S / 2 - 1.6), &x, &y); lf = lf_frame(x, y, a + sv * PI / 2);
    l = LL();
    LP(l, barf(&lf, 0, -0.95, 4.6 + t, -0.95, 0.92)); LP(l, barf(&lf, 0, 0.95, 4.6 + t, 0.95, 0.92));
    n = arange_(0.5, 4.4 + t, 1.6, &us); for (i = 0; i < n; i++) LP(l, barf(&lf, us[i], -1.0, us[i], 1.0, 0.92));
    lad = U_(l);
    Lup(lay, g_diff(lad, g_buf(g_diff(outer, hole), 0.3)), 0.5, wd);
    *foot = U2_(g_buf(outer, 0.4), hull_(lad));
}
/* S_camp(p): s[0] = p["rock"] or p["stone"], s[1] = p["wood"], s[2] = p["iron"] */
BUILDER(S_camp, K_REQ) {
    BsBuild fire = FEAT2("b_campfire", c->s[0], c->s[1]); Geo *foot0, *lg, *kd; SoLayers *fl; GeoList *ft = LL(); int i, n; double d, sd, t, lx, ly, kx, ky;
    if (CALLB(&fire, cx, cy, a, rng, 0, 1.0, &foot0, &fl) == 1) ly_raise();          /* foot0, lay = None: a TypeError */
    for (i = 0; i < fl->n; i++) lyl_push(lay, fl->v[i]);
    LP(ft, foot0);
    n = (int)bs_randint(rng, 2, 3) + (k > 2.2); d = 5.6 + 1.0 * kk_(k, 0.5);
    for (i = 0; i < n; i++) {
        double t0 = a + PI + (i - (n - 1) / 2.0) * 1.0 + UNI(-0.12, 0.12), bx = cx + d * COS(t0), by = cy + d * SIN(t0), b = t0 + PI / 2 + UNI(-0.3, 0.3);
        Frame bf = lf_frame(bx, by, b); SoSolids *sol = SL(); int64_t i0;
        LP(ft, g_buf(rect_(bx, by, 4.6, 1.9, b), 0.3));
        VAULT(sol, bx, by, b, 5.0, 2.4, 0.6, "loaf", 0, 0, 0, 0); Lsteps(lay, sol, "sack", 0, 0, NULL, 0); i0 = lay->n - 1;
        Lon(lay, rectf(&bf, 1.7, 0, 1.3, 1.7, b), i0, 0.35, "sack"); Ltcut(lay, barf(&bf, -0.6, -1.5, -0.6, 1.5, 0.24), i0, 0.2);
    }
    sd = bs_choice_pm1(rng); t = a + sd * UNI(1.2, 1.5); lx = cx + (d - 0.8) * COS(t); ly = cy + (d - 0.8) * SIN(t);
    { double p[2] = { lx - 2.6 * SIN(t), ly + 2.6 * COS(t) }, q[2] = { lx + 2.6 * SIN(t), ly - 2.6 * COS(t) }; lg = cap_(p, q, 1.4); LP(ft, lg); }
    { SoSolids *sol = SL(); VAULT(sol, lx, ly, t + PI / 2, 5.4, 1.7, 0.95, "round", 0, 0, 0, 0); Lsteps(lay, sol, c->s[1], 0, 0, NULL, 0); }
    t = a - sd * 0.6; kx = cx + 4.0 * COS(t); ky = cy + 4.0 * SIN(t); kd = disc_(kx, ky, 0.85, 16); LP(ft, kd);
    { double pr[2 * 8] = { 0.95, 0.0, 0.95, 0.45 }; SoSolids *sol = SL(); int np = 2 + CURVE(0.95, 0.5, 0.45, 0.9, 5, 1, pr + 4); int64_t n0 = lay->n;
      LATHE(sol, kx, ky, pr, np, 16, 1.0, 0.0, 0.0, NULL, 0); Lsteps(lay, sol, c->s[2], 0, 0, NULL, 0); Lon(lay, disc_(kx, ky, 0.34, 10), n0, 0.25, c->s[2]); }
    *foot = g_buf(hull_(U_(ft)), 0.3);
}
/* S_depot(p): wd = kfix(p["wood"]); s[1] = p["wood"], s[2] = p["crate"] */
BUILDER(S_depot, K_REQ) {
    Frame ff = lf_frame(cx, cy, a); int nx = 2 + (k > 1.9), ny = 2 + (int)bs_int(k + 0.4), i, j, n; double cw = 2.7, L, W, *vs; Geo *pal; GeoList *l = LL();
    L = nx * cw + 0.8; W = ny * cw + 0.8; pal = rect_(cx, cy, L, W, a);
    n = arange_(-W / 2 + 1.2, W / 2 - 0.4, 1.2, &vs); for (i = 0; i < n; i++) LP(l, barf(&ff, -L, vs[i], L, vs[i], 0.24));
    Lup(lay, pal, 0.35, c->s[0]); Ltcut(lay, U_(l), 0, 0.22);
    for (i = 0; i < nx; i++) for (j = 0; j < ny; j++) {
        double p[2], hi;
        if (RND() < 0.16) continue;
        FF(&ff, (i - (nx - 1) / 2.0) * cw, (j - (ny - 1) / 2.0) * cw, p); hi = i == 0 ? 1.0 : 0.0;
        if (RND() < 0.3) {
            double hb = 1.7 + hi, pr[2 * 9] = { 1.3, 0.0, 1.3, hb * 0.5 }; int np = 2 + CURVE(1.3, 1.08, hb * 0.5, hb, 6, 1, pr + 4); SoSolids *sol = SL();
            LATHE(sol, p[0], p[1], pr, np, 28, 1.0, 0.0, 0.0, NULL, 0); Lsteps(lay, sol, c->s[1], 1, 0, NULL, 0);
            Lon(lay, ann_(p[0], p[1], 1.25, 0.5, 24), lay->n - 1, 0.22, c->s[1]);
        } else {
            double b = a + UNI(-0.07, 0.07); SoFrame f2 = sof(p[0], p[1], b); Geo *gq = rect_(p[0], p[1], 2.4, 2.4, b); int64_t n0 = lay->n;
            if (so_crate_layers(gq->g, &f2, 2.4, 2.4, b, 1.3 + hi, c->s[2], rng, 1, 0, n0, lay) != SO_OK) ly_raise();
        }
    }
    *foot = pal;
}
/* S_graves(p): st = kfix(p["stone"]), ea = p["soft"] or "earth"; s[2] = p["pit"] */
BUILDER(S_graves, K_REQ) {
    Frame ff = lf_frame(cx, cy, a); int n = 2 + (int)bs_int(k + 0.5), i; double gap = 4.1; int64_t op; GeoList *ft = LL();
    if (py_randrange1_e(rng, n, &op) != 0) ly_raise();
    for (i = 0; i < n; i++) {
        double v = (i - (n - 1) / 2.0) * gap, b = a + UNI(-0.05, 0.05); Geo *m = g_bufq(rectf(&ff, 0.6, v, 5.6, 2.2, b), 0.45, 4), *hs = rectf(&ff, -3.9, v, 1.05, 2.3, b);
        LP(ft, m); LP(ft, hs);
        if (i == op) {
            double p[2]; Geo *sp; Lgcut(lay, g_inset(m, 0.3), 0.85, c->s[2]);
            { double dv = UNI(-0.6, 0.6); FF(&ff, 5.6, v + dv, p); } sp = LAG(lf_blob(p[0], p[1], 1.5, rng, 4)); LP(ft, sp);
            Lmound(lay, sp, 1.1, c->s[1], SOFT_T);
        } else Lmound(lay, m, 0.7, c->s[1], SOFT_T);
        { double h = UNI(1.8, 2.6); Lup(lay, hs, h, c->s[0]); }
    }
    *foot = g_buf(hull_(U_(ft)), 0.3);
}
/* S_wagon(p): wd = kfix(p["wood"]); s[1] = p["wood"], s[2] = p["iron"] */
BUILDER(S_wagon, K_REQ) {
    double b, L, W, gx, gy, wx, wy, wa, p[2], *vs; Frame ff; Geo *bed, *tg, *wf; int gone, ci, su, sv, i, n; SoLayers *wl; BsBuild wh;
    static const int CS[4][2] = { { -1, -1 }, { -1, 1 }, { 1, -1 }, { 1, 1 } };
    b = a + PI / 2 + UNI(-0.25, 0.25); ff = lf_frame(cx, cy, b); L = UNI(9, 11) * kk_(k, 0.7); W = UNI(4.6, 5.4) * kk_(k, 0.6);
    bed = rect_(cx, cy, L, W, b); gone = (int)py_choice(rng, 4); gx = CS[gone][0]; gy = CS[gone][1];
    if (SUP()) {
        double Rw = 1.75, ax = Rw * 0.95; SoSolids *sol = SL();
        for (ci = 0; ci < 4; ci++) {
            if (ci == gone) continue;
            su = CS[ci][0]; sv = CS[ci][1]; FF(&ff, su * (L / 2 - 1.9), sv * (W / 2 + 0.6), p);
            VAULT(sol, p[0], p[1], b + PI / 2, 0.9, 2 * Rw, Rw, "round", 0, 1, 1, ax);
            FF(&ff, su * (L / 2 - 1.9), sv * (W / 2 + 0.6), p);
            VAULT(sol, p[0], p[1], b + PI / 2, 1.5, 0.9, 0.5, "round", 0, 1, 1, ax);
        }
        for (su = -1; su <= 1; su += 2) { FF(&ff, su * (L / 2 - 1.9), 0, p); VAULT(sol, p[0], p[1], b + PI / 2, W + 1.0, 0.7, 0.4, "round", 0, 1, 1, ax); }
        SLAB(sol, bed, ax + 0.25, ax + 0.95); SLAB(sol, g_diff(bed, g_inset(bed, 0.8)), ax + 0.9, ax + 1.9);
        BOX(sol, rectf(&ff, gx * (L / 2 - 1.9), gy * (W / 2 - 0.9), 1.3, 1.3, b), ax + 0.3);
        tg = barf(&ff, L / 2, 0, L / 2 + 5.0 * kk_(k, 0.5), 0, 0.95);
        { double q = UNI(0, 1.5); FF(&ff, gx * (L / 2 - 1.0 + q), gy * (W / 2 + 4.2), p); wx = p[0]; wy = p[1]; }
        wh = FEAT2("b_wheel", c->s[1], c->s[2]); wa = UNI(0, 3);
        if (CALLB(&wh, wx, wy, wa, rng, 0, 1.0, &wf, &wl) == 1) ly_raise();
        SLAB(sol, tg, ax - 0.1, ax + 0.4);
        Lsteps(lay, sol, c->s[0], 0, 0, NULL, 0); Ljoin(lay, wl);
        { GeoList *l = LL(); LP(l, g_buf(bed, 0.9)); LP(l, tg); LP(l, wf); *foot = hull_(U_(l)); }
        return;
    }
    {
        SoSolids *sol = SL(); GeoList *l = LL();
        for (ci = 0; ci < 4; ci++) {
            if (ci == gone) continue;
            su = CS[ci][0]; sv = CS[ci][1]; FF(&ff, su * (L / 2 - 1.9), sv * (W / 2 + 0.6), p);
            VAULT(sol, p[0], p[1], b + PI / 2, 1.05, 3.4, 2.2, "round", 0, 0, 0, 0);
            VAULT(sol, p[0], p[1], b + PI / 2, 1.6, 0.95, 1.2, "round", 0, 0, 0, 0);
        }
        Lsteps(lay, sol, c->s[0], 0, 0, NULL, 0); Lup(lay, bed, 1.0, c->s[0]);
        n = arange_(-W / 2 + 1.1, W / 2 - 0.4, 1.1, &vs); for (i = 0; i < n; i++) LP(l, barf(&ff, -L, vs[i], L, vs[i], 0.24));
        Ltcut(lay, U_(l), 1, 0.22); Lon(lay, g_diff(bed, g_inset(bed, 0.95)), 1, 0.9, c->s[0]);
        tg = barf(&ff, L / 2, 0, L / 2 + 5.0 * kk_(k, 0.5), 0, 0.95); Lup(lay, tg, 0.5, c->s[0]);
        { double q = UNI(0, 1.5); FF(&ff, gx * (L / 2 - 1.0 + q), gy * (W / 2 + 4.2), p); wx = p[0]; wy = p[1]; }
        wh = FEAT2("b_wheel", c->s[1], c->s[2]); wa = UNI(0, 3);
        if (CALLB(&wh, wx, wy, wa, rng, 0, 1.0, &wf, &wl) == 1) ly_raise();
        Ljoin(lay, wl);
        l = LL(); LP(l, g_buf(bed, 0.9)); LP(l, tg); LP(l, wf); *foot = hull_(U_(l));
    }
}
/* S_corral(p): wd = kfix(p["wood"]) */
BUILDER(S_corral, K_REQ) {
    double R, *pts, t; int n, i; GeoList *posts = LL(), *rails = LL(); Geo *pg, *rg, *tr;
    R = UNI(7.0, 8.5) * kk_(k, 0.8); n = (int)bs_int(2 * PI * R / 4.2); if (n < 7) n = 7;
    pts = AR(2 * n);
    for (i = 0; i < n; i++) { pts[2 * i] = cx + R * COS(a + 6.283 * (i + 0.5) / n); pts[2 * i + 1] = cy + R * SIN(a + 6.283 * (i + 0.5) / n); }
    for (i = 0; i < n; i++) LP(posts, rect_(pts[2 * i], pts[2 * i + 1], 1.15, 1.15, a + 6.283 * (i + 0.5) / n));
    pg = U_(posts);
    for (i = 0; i < n - 1; i++) if (RND() > 0.08) LP(rails, bar_(&pts[2 * i], &pts[2 * i + 2], 0.92));
    rg = U_(rails);
    Lup(lay, g_diff(rg, g_buf(pg, 0.12)), 0.9, c->s[0]); Lup(lay, pg, 1.9, c->s[0]);
    t = a + PI + UNI(-0.9, 0.9); tr = rect_(cx + (R - 2.0) * COS(t), cy + (R - 2.0) * SIN(t), 1.7, 4.2, t);
    Lup(lay, tr, 0.95, c->s[0]); Ltcut(lay, g_inset(tr, 0.45), lay->n - 1, 0.5);
    *foot = disc_(cx, cy, R + 0.8, 40);
}
/* S_machine(p): m = kfix(p["iron"]) */
BUILDER(S_machine, K_REQ) {
    Frame ff = lf_frame(cx, cy, a), f2; double Dp, W, hc[2], hw, hl, tc[2], tr, *vs; Geo *base; GeoList *l = LL(), *ft = LL(); int i, n, sv; const char *m = c->s[0];
    Dp = UNI(6.0, 7.5) * kk_(k, 0.8); W = UNI(8.0, 10.0) * kk_(k, 0.8); base = rect_(cx, cy, Dp, W, a);
    Lup(lay, base, 0.6, m); Ltcut(lay, g_buf(g_ext(g_inset(base, 0.45)), 0.14), 0, 0.22);
    FF(&ff, -Dp * 0.12, -W * 0.2, hc); hw = Dp * 0.6; hl = W * 0.46; Lon(lay, rect_(hc[0], hc[1], hw, hl, a), 0, 1.8, m); f2 = lf_frame(hc[0], hc[1], a);
    n = arange_(-hl * 0.3, hl * 0.31, 0.8, &vs); for (i = 0; i < n; i++) LP(l, barf(&f2, -hw * 0.32, vs[i], hw * 0.32, vs[i], 0.36));
    Ltcut(lay, U_(l), lay->n - 1, 0.3);
    FF(&ff, -Dp * 0.05, W * 0.27, tc); tr = mn(Dp, W) * 0.2 + 0.4; Lon(lay, disc_(tc[0], tc[1], tr, 28), 0, 2.3, m); Ltcut(lay, ann_(tc[0], tc[1], tr * 0.62, tr * 0.62 - 0.3, 24), lay->n - 1, 0.25);
    Lon(lay, rectf(&ff, Dp * 0.33, -W * 0.28, 1.8, 2.6, a), 0, 1.1, m);
    LP(ft, g_buf(base, 0.4));
    for (sv = -1; sv <= 1; sv += 2) {
        double u = UNI(-Dp * 0.25, Dp * 0.2); Geo *st = barf(&ff, u, sv * (W / 2 - 0.2), u, sv * (W / 2 + 2.8), 1.5), *fl = rectf(&ff, u, sv * (W / 2 + 2.6), 2.2, 0.95, a);
        LP(ft, st); LP(ft, fl);
        Lup(lay, g_diff(g_diff(st, g_buf(base, 0.15)), g_buf(fl, 0.15)), 0.9, "pipe"); Lup(lay, fl, 0.98, "pipe");
    }
    *foot = hull_(U_(ft));
}
/* S_emplacement(p): s[0] = p["iron"], s[1] = p["wood"] (kfix'd when the builder runs) */
BUILDER(S_emplacement, K_REQ) {
    Frame ff = lf_frame(cx, cy, a), sc; double R, *ts, sv, p[2]; int i, n, j; const char *iw; GeoList *l;
    R = UNI(6.3, 7.3) * kk_(k, 0.8);
    n = arange_(0.8, 6.283 - 0.8, 2.95 / R, &ts);
    for (i = 0; i < n; i++) {
        double t = ts[i]; Geo *q = g_bufq(rect_(cx + R * COS(a + t), cy + R * SIN(a + t), 1.7, 2.6, a + t), 0.3, 3);
        Lmound(lay, q, i % 2 ? 1.5 : 1.05, "sandbag", &BAG_T);
    }
    iw = KF(c->s[0]);
    l = LL(); for (j = -1; j <= 1; j += 2) LP(l, rectf(&ff, -0.7, j * 2.0, 2.7, 1.0, a));
    Lup(lay, U_(l), 1.5, KF(c->s[1])); Lup(lay, barf(&ff, -1.0, 0, -R * 0.8, 0, 1.05), 1.3, iw);
    Lup(lay, rectf(&ff, 0.3, 0, 2.2, 1.9, a), 1.6, iw); Lup(lay, barf(&ff, 1.6, 0, 4.4, 0, 1.0), 0.6, iw);
    sv = bs_choice_pm1(rng); FF(&ff, 1.0, sv * R * 0.55, p); { double q = UNI(-0.3, 0.3); sc = lf_frame(p[0], p[1], a + q); }
    l = LL(); { static const double V3[3] = { -1.0, 0, 1.0 }; for (j = 0; j < 3; j++) LP(l, capf(&sc, -1.1, V3[j], 1.1, V3[j], 0.9)); }
    Lup(lay, g_buf(U_(l), 0.05), 0.7, c->s[0]);
    *foot = disc_(cx, cy, R + 1.6, 32);
}
/* S_tent(p): s[0] = p["wood"] */
BUILDER(S_tent, K_REQ) {
    Frame ff = lf_frame(cx, cy, a); double L, W; Geo *body; GeoList *ft = LL(); SoSolids *sol = SL(); int su, sv;
    L = UNI(8, 10) * kk_(k, 0.7); W = UNI(6.0, 7.0) * kk_(k, 0.5); body = rect_(cx, cy, L, W, a);
    VAULT(sol, cx, cy, a, L, W, W * 0.46, "ridge", 0, 0, 0, 0); VAULT(sol, cx, cy, a, L + 0.9, 0.55, W * 0.46 + 0.2, "loaf", 0, 0, 0, 0);
    Lsteps(lay, sol, "sail", 0, 0, NULL, 0); LP(ft, body);
    for (sv = -1; sv <= 1; sv += 2) { double uv[6] = { L / 2 + 0.4, sv * 0.45, L / 2 + 2.3, sv * 2.3, L / 2 + 0.4, sv * 2.6 }; Lup(lay, g_frpoly(&ff, uv, 3), 0.45, "sail"); }
    for (su = -1; su <= 1; su += 2) for (sv = -1; sv <= 1; sv += 2) {
        double c0[2], c1[2], c2[2];
        FF(&ff, su * (L / 2 + 0.4), sv * (W / 2 + 0.4), c0); FF(&ff, su * (L / 2 + 1.9), sv * (W / 2 + 1.9), c1); FF(&ff, su * (L / 2 + 2.5), sv * (W / 2 + 2.5), c2);
        Lup(lay, bar_(c0, c1, 0.92), 0.35, "rope"); Lup(lay, rect_(c2[0], c2[1], 1.0, 1.0, a + 0.785), 0.8, c->s[0]); LP(ft, disc_(c2[0], c2[1], 0.8, 20));
    }
    *foot = hull_(U_(ft));
}
/* S_log(p): wd = kfix(p["wood"]); s[1] = p["pit"] */
typedef struct { double W, L; } LogR;
static int log_r(void *c, double u, double *out) { LogR *r = (LogR *)c; *out = r->W / 2 - r->W * 0.14 * (u / r->L + 0.5); return SO_OK; }          /* lambda u: W / 2 - W * 0.14 * (u / L + 0.5) */
BUILDER(S_log, K_REQ) {
    double b, L, W, rot, rc[2], hc[2]; Frame ff; Geo *trunk, *rp_, *hole; GeoList *l = LL(), *ft = LL(); int i, n, j; SoSolids *sol = SL(); LogR lr;
    b = a + PI / 2 + UNI(-0.3, 0.3); ff = lf_frame(cx, cy, b); L = UNI(16, 21) * kk_(k, 0.8); W = UNI(2.7, 3.3) * kk_(k, 0.5);
    { double uv[8] = { -L / 2, -W / 2, L / 2, -W * 0.36, L / 2, W * 0.36, -L / 2, W / 2 }; trunk = g_frpoly(&ff, uv, 4); }
    { double V3[3] = { -W * 0.22, 0.05, W * 0.24 };
      for (j = 0; j < 3; j++) { double p[2], q[2], xy[14]; int m; FF(&ff, -L / 2 + 0.6, V3[j], p); FF(&ff, L / 2 - 0.6, V3[j] * 0.75, q); m = lf_wiggle(p[0], p[1], q[0], q[1], rng, 0.25, 6, xy); LP(l, g_buf(g_line(xy, m), 0.13)); } }
    lr.W = W; lr.L = L; LOFT(sol, cx, cy, b, L, log_r, &lr, 1.15, "round", 0.0, 0);
    Lsteps(lay, sol, c->s[0], 0, 0, NULL, 0); Ltcut(lay, U_(l), 0, 0.25);
    FF(&ff, -L / 2 - 0.5, 0, rc); rot = UNI(0, 1); rp_ = LAG(lf_star(rc[0], rc[1], W * 1.1, W * 0.68, 7, rot)); Lup(lay, g_diff(rp_, g_buf(trunk, 0.15)), 2.3, KF("root"));
    LP(ft, trunk); LP(ft, rp_);
    n = (int)bs_randint(rng, 2, 4);
    for (i = 0; i < n; i++) {
        double u = UNI(-L * 0.25, L * 0.42), sv = bs_choice_pm1(rng), du, dv; Geo *br;
        du = UNI(0.8, 1.6); dv = UNI(1.8, 3.2);
        br = barf(&ff, u, sv * W * 0.3, u + du, sv * (W / 2 + dv), 1.0);
        Lup(lay, g_diff(br, g_buf(trunk, 0.15)), 0.8, c->s[0]); LP(ft, br);
    }
    FF(&ff, -L / 2 - W * 1.1 - 1.6, 0, hc); hole = LAG(lf_blob(hc[0], hc[1], 1.7, rng, 4)); Lgcut(lay, hole, 0.5, c->s[1]); LP(ft, hole);
    *foot = g_buf(hull_(U_(ft)), 0.2);
}
/* S_well(p): st = kfix(p["stone"]); s[1] = p["water"], s[2] = p["wood"] */
BUILDER(S_well, K_REQ) {
    Frame ff = lf_frame(cx, cy, a); double R, sv, bc[2], rcc[2]; int n, i; GeoList *l = LL(); int64_t n0;
    R = UNI(3.6, 4.4) * kk_(k, 0.6); n = (int)bs_randint(rng, 9, 12);
    for (i = 0; i < n; i++) {
        double p[2] = { cx + (R - 1.5) * COS(a + 6.283 * i / n), cy + (R - 1.5) * SIN(a + 6.283 * i / n) }, q[2] = { cx + (R + 0.2) * COS(a + 6.283 * i / n), cy + (R + 0.2) * SIN(a + 6.283 * i / n) };
        LP(l, bar_(p, q, 0.26));
    }
    Lup(lay, ann_(cx, cy, R, R - 1.3, 2 * n), 1.5, c->s[0]); Ltcut(lay, U_(l), 0, 0.25); Lgcut(lay, disc_(cx, cy, R - 1.7, 24), 0.85, c->s[1]);
    sv = bs_choice_pm1(rng); FF(&ff, R * 0.45, sv * (R + 1.5), bc); FF(&ff, -R * 0.3, sv * (R + 1.9), rcc);
    n0 = lay->n;
    Lup(lay, disc_(bc[0], bc[1], 0.95, 16), 0.98, c->s[2]); Ltcut(lay, disc_(bc[0], bc[1], 0.55, 12), n0, 0.5); Lup(lay, ann_(rcc[0], rcc[1], 1.35, 0.45, 18), 0.45, "rope");
    l = LL(); LP(l, disc_(cx, cy, R + 0.3, 32)); LP(l, disc_(bc[0], bc[1], 1.2, 20)); LP(l, disc_(rcc[0], rcc[1], 1.5, 20));
    *foot = hull_(U_(l));
}
/* S_statue(p): st = kfix(p["stone"]) */
typedef struct { double w, ln; } DrumR;
static int drum_r(void *c, double u, double *out) { DrumR *r = (DrumR *)c; *out = r->w / 2 * (0.82 + 0.18 * py_cos(u / r->ln * 2.4)); return SO_OK; }
BUILDER(S_statue, K_REQ) {
    double P, s_, sv, b, u, hc[2]; Geo *pl, *hd; Frame bf; GeoList *ft = LL(); int j; const char *st = c->s[0];
    P = UNI(4.4, 5.2) * kk_(k, 0.6); pl = rect_(cx, cy, P, P, a); s_ = kk_(k, 0.5);
    Lup(lay, pl, 0.7, st); Lon(lay, g_inset(pl, 0.75), 0, 0.9, st);
    { double pr[6] = { P * 0.27, 0.0, P * 0.22, 0.7, P * 0.2, 1.0 }; SoSolids *sol = SL(); LATHE(sol, cx, cy, pr, 3, 12, 1.0, 0.0, 0.0, NULL, 0); Lsteps(lay, sol, st, 1, 1, NULL, 0); }
    Ltcut(lay, g_buf(g_ext(g_inset(pl, 0.4)), 0.13), 0, 0.22);
    sv = bs_choice_pm1(rng); b = a + sv * UNI(0.75, 1.05); bf = lf_frame(cx, cy, b); u = P / 2 + 1.6; LP(ft, g_buf(pl, 0.4));
    for (j = 0; j < 2; j++) {
        double ln = j ? 3.2 * s_ : 3.8 * s_, w = j ? 2.0 * s_ : 2.5 * s_, qc[2], qa, dv; Geo *q; SoSolids *sol = SL(); DrumR *dr = (DrumR *)LM(sizeof *dr);
        dv = UNI(-0.3, 0.3); FF(&bf, u + ln / 2, dv, qc); qa = b + UNI(-0.25, 0.25); q = rect_(qc[0], qc[1], ln, w, qa); LP(ft, q); u += ln + 0.7;
        dr->w = w; dr->ln = ln; LOFT(sol, qc[0], qc[1], qa, ln, drum_r, dr, 0.95, "round", 0.0, 0); Lsteps(lay, sol, st, 0, 0, NULL, 0);
    }
    { double dv = UNI(-0.8, 0.8); FF(&bf, u + 1.2 * s_, dv, hc); } hd = disc_(hc[0], hc[1], 1.15 * s_, 16); LP(ft, hd);
    { double pr[16]; int np = CURVE(1.15 * s_, 0.35, 0.0, 1.5 * s_, 7, 1, pr); SoSolids *sol = SL(); LATHE(sol, hc[0], hc[1], pr, np, 16, 1.0, 0.0, 0.0, NULL, 0); Lsteps(lay, sol, st, 0, 0, NULL, 0); }
    *foot = hull_(U_(ft));
}
/* S_organ(p): s[0] = p["soft"] or "flesh", s[1] = p["vein"] */
BUILDER(S_organ, K_REQ) {
    double R; Geo *body; GeoList *parts = LL(); int n, i, j, m = 6;
    R = UNI(4.4, 5.4) * kk_(k, 0.8); body = g_buf(hull_(LAG(lf_blob(cx, cy, R, rng, 5))), 0.3); Lmound(lay, body, 2.4, c->s[0], SOFT_T);
    n = (int)bs_randint(rng, 5, 7);
    for (i = 0; i < n; i++) {
        double t = a + 6.283 * (i + 0.5) / n + UNI(-0.25, 0.25), Ln = UNI(5, 8) * kk_(k, 0.6), pts[2 * 7];
        lf_wiggle(cx + (R * 0.7) * COS(t), cy + (R * 0.7) * SIN(t), cx + (R + Ln) * COS(t), cy + (R + Ln) * SIN(t), rng, 0.8, m, pts);
        for (j = 0; j < m; j++) LP(parts, cap_(&pts[2 * j], &pts[2 * j + 2], 1.7 - 0.75 * j / (m - 1)));
    }
    Lup(lay, g_diff(U_(parts), g_inset(body, 1.2)), 0.6, c->s[1]);
    *foot = g_buf(body, 1.2);
}

/* ================================================================ the made things: b_* */
/* b_crate(kind="crate", wood="wood") */
BUILDER(B_crate, K_NONE) {
    double L, W, h; Frame ff; Geo *g; const char *kind = c->s[0];
    L = UNI(3.4, 4.2); W = UNI(2.9, 3.4); ff = lf_frame(cx, cy, a); g = rect_(cx, cy, L, W, a); h = UNI(1.2, 1.7);
    if (RND() < 0.3) {
        double du, b2, b3, p[2]; Frame lf; Geo *lid; GeoList *l = LL(); int64_t n0; static const double V3[3] = { -0.6, 0, 0.6 }; int j;
        Lup(lay, g, h, kind); Ltcut(lay, g_inset(g, 0.92), 0, 0.9);
        du = UNI(-0.5, 0.5); FF(&ff, du, W / 2 + 1.55, p); b2 = a + UNI(-0.35, 0.35); lf = lf_frame(p[0], p[1], b2);
        FF(&lf, 0, 0, p); b3 = a + UNI(-0.3, 0.3); lid = rect_(p[0], p[1], L * 0.95, W * 0.8, b3);
        n0 = lay->n; Lup(lay, lid, 0.4, kind);
        for (j = 0; j < 3; j++) LP(l, barf(&lf, -L, V3[j], L, V3[j], 0.22));
        Ltcut(lay, U_(l), n0, 0.18);
        *foot = hull_(U2_(g, lid)); return;
    }
    { SoFrame f = sof(cx, cy, a); if (so_crate_layers(g->g, &f, L, W, a, h, kind, rng, 0, 0, 0, lay) != SO_OK) ly_raise(); }
    *foot = g;
}
/* b_crate_stack(kind="crate", wood="wood") */
BUILDER(B_crate_stack, K_NONE) {
    double L, W, h, b, L2, p[2]; Frame ff; SoFrame sf; Geo *g, *g2; GeoList *ft = LL(); const char *kind = c->s[0];
    L = UNI(3.4, 4.0); W = UNI(2.9, 3.3); ff = lf_frame(cx, cy, a); g = rect_(cx, cy, L, W, a); h = UNI(1.3, 1.6);
    sf = sof(cx, cy, a); if (so_crate_layers(g->g, &sf, L, W, a, h, kind, rng, 0, 0, 0, lay) != SO_OK) ly_raise();
    LP(ft, g);
    b = a + UNI(-0.3, 0.3); { double du = UNI(-0.4, 0.4); FF(&ff, du, W + 0.5, p); } sf = sof(p[0], p[1], b); L2 = L * UNI(0.8, 0.95); g2 = rect_(p[0], p[1], L2, W, b);
    h = UNI(1.1, 1.4); if (so_crate_layers(g2->g, &sf, L2, W, b, h, kind, rng, 0, 0, lay->n, lay) != SO_OK) ly_raise();
    LP(ft, g2);
    if (RND() < 0.6) {
        double b3 = a + UNI(0.2, 0.6), du, dv; Geo *g3; int64_t n0;
        du = UNI(-0.3, 0.3); dv = UNI(-0.2, 0.2); g3 = rectf(&ff, du, dv, 2.1, 1.9, b3); n0 = lay->n;
        Lon(lay, g3, 0, 1.0, kind); Ltcut(lay, g_buf(g_ext(g_inset(g3, 0.35)), 0.11), n0, 0.18);
    }
    *foot = hull_(U_(ft));
}
/* b_keg(kind="wood", iron="iron") */
BUILDER(B_keg, K_NONE) {
    double r, h, rb, pr[2 * 10], real[2 * 11], *ts; int np, j, n; SoSolids *sol = SL(); GeoList *l = LL(); Geo *staves; const char *kind = c->s[0];
    r = UNI(1.3, 1.6); h = UNI(1.9, 2.5); rb = r * 1.2;
    pr[0] = rb; pr[1] = 0.05; pr[2] = rb; pr[3] = h * 0.5; np = 2 + CURVE(rb, r, h * 0.5, h, 7, 1, pr + 4);
    real[0] = r * 0.98; real[1] = 0.0;
    for (j = 1; j < 10; j++) { real[2 * j] = r + (rb - r) * bs_pow(SIN(PI * j / 10), 0.8); real[2 * j + 1] = h * j / 10; }
    real[20] = r; real[21] = h;
    LATHE(sol, cx, cy, pr, np, 28, 1.0, 0.0, 0.0, real, 11);
    n = arange_(0, 6.28, 6.283 / 9, &ts);
    for (j = 0; j < n; j++) { double t = ts[j], p[2] = { cx + 0.3 * COS(a + t), cy + 0.3 * SIN(a + t) }, q[2] = { cx + r * COS(a + t), cy + r * SIN(a + t) }; LP(l, bar_(p, q, 0.2)); }
    staves = g_inter(U_(l), disc_(cx, cy, r - 0.5, 24));
    Lsteps(lay, sol, kind, 0, 0, NULL, 0); Lon(lay, ann_(cx, cy, r + 0.2, r - 0.6, 28), 0, 0.22, kind);
    if (!g_isempty(staves)) Ltcut(lay, staves, 0, 0.16);
    if (RND() < 0.6) Lon(lay, disc_(cx + r * 0.45 * COS(a + 1), cy + r * 0.45 * SIN(a + 1), 0.28, 10), 0, 0.2, kind);
    *foot = disc_(cx, cy, rb + 0.2, 24);
}
/* _barrel_side(cx, cy, a, Ln, r, lift): loft(..., lambda u: r * (0.84 + 0.16 * math.cos(u / (Ln / 2) * 1.35)) + hoop(u), 1.2, lift=lift, under=True) */
typedef struct { double Ln, r; } BarrelR;
static int barrel_r(void *c, double u, double *out) {
    BarrelR *b = (BarrelR *)c; double x = fabs(fabs(u) - b->Ln * 0.29) / 0.34, p, hoop;
    PW(x, 2.0, p); hoop = 0.13 * mx(0.0, 1 - p);
    *out = b->r * (0.84 + 0.16 * py_cos(u / (b->Ln / 2) * 1.35)) + hoop; return SO_OK;
}
static void barrel_side(SoSolids *sol, double cx, double cy, double a, double Ln, double r, double lift) {
    BarrelR *b = (BarrelR *)LM(sizeof *b); b->Ln = Ln; b->r = r; LOFT(sol, cx, cy, a, Ln, barrel_r, b, 1.2, "round", lift, 1);
}
/* b_barrel(kind="wood", iron="iron") */
BUILDER(B_barrel, K_NONE) {
    double Ln, r; Frame ff; SoSolids *sol = SL(); const char *kind = c->s[0];
    Ln = UNI(3.6, 4.4); r = UNI(1.15, 1.4); ff = lf_frame(cx, cy, a);
    barrel_side(sol, cx, cy, a, Ln, r, 0.0); Lsteps(lay, sol, kind, 0, 0, NULL, 0);
    if (RND() < 0.7) { double du = UNI(-0.8, 0.8), sv = bs_choice_pm1(rng), b = a + UNI(-0.3, 0.3); Lup(lay, rectf(&ff, du, (r + 0.5) * sv, 1.4, 0.95, b), 0.5, kind); }
    *foot = rect_(cx, cy, Ln, 2 * r + 0.3, a);
}
/* b_barrels(kind="wood", iron="iron") */
BUILDER(B_barrels, K_NONE) {
    double Ln, r, p[2]; Frame ff; SoSolids *sol = SL();
    Ln = UNI(3.6, 4.2); r = UNI(1.15, 1.35); ff = lf_frame(cx, cy, a);
    FF(&ff, 0, -r - 0.2, p); barrel_side(sol, p[0], p[1], a, Ln, r, 0.0);
    { double du = UNI(-0.2, 0.2); FF(&ff, du, r + 0.2, p); } barrel_side(sol, p[0], p[1], a, Ln * 0.96, r, 0.0);
    { double du = UNI(-0.3, 0.3); FF(&ff, du, 0, p); } barrel_side(sol, p[0], p[1], a, Ln, r, r * 1.25 * 0.8);
    Lsteps(lay, sol, c->s[0], 0, 0, NULL, 0);
    *foot = rect_(cx, cy, Ln + 0.4, 4 * r + 0.8, a);
}
/* _sack(cx, cy, a, L, W, h, lift) */
typedef struct { double L, W; } SackR;
static int sack_r(void *c, double u, double *out) {          /* lambda u: W / 2 * full(u) * (1.0 if u < L * 0.2 else 1 - 0.5 * ((u - L * 0.2) / (L * 0.3)) ** 2) */
    SackR *s = (SackR *)c; double x = fabs(2 * u / s->L), full, p, tail;
    if (x < 1) { PW(x, 3.2, p); PW(1 - p, 0.45, full); } else full = 0.0;
    if (u < s->L * 0.2) tail = 1.0; else { PW((u - s->L * 0.2) / (s->L * 0.3), 2.0, p); tail = 1 - 0.5 * p; }
    *out = s->W / 2 * full * tail; return SO_OK;
}
static int neck_r(void *c, double u, double *out) { (void)c; *out = 0.34 + 0.14 * u / 0.45; return SO_OK; }          /* lambda u: 0.34 + 0.14 * u / 0.45 */
static void sack(SoSolids *sol, double cx, double cy, double a, double L, double W, double h, double lift) {
    SackR *s = (SackR *)LM(sizeof *s); Frame f = lf_frame(cx, cy, a); double p[2];
    s->L = L; s->W = W; LOFT(sol, cx, cy, a, L, sack_r, s, 2 * h / W, "round", lift, 0);
    FF(&f, L / 2 + 0.25, 0, p); LOFT(sol, p[0], p[1], a, 0.9, neck_r, NULL, 1.0 * h / W + 0.6, "round", lift, 0);
}
/* b_sacks(kind="sack") */
BUILDER(B_sacks, K_NONE) {
    Frame ff = lf_frame(cx, cy, a); SoSolids *sol = SL(); GeoList *ft = LL(); int n, i; double L, W, h, p[2];
    n = (int)bs_randint(rng, 2, 3); L = UNI(3.2, 3.8); W = UNI(2.1, 2.5); h = UNI(0.85, 1.0);
    for (i = 0; i < n; i++) {
        double du = UNI(-0.3, 0.3), b; FF(&ff, du, (i - (n - 1) / 2.0) * (W - 0.1), p);
        b = a + UNI(-0.2, 0.2); b = b + PI * (double)bs_randint(rng, 0, 1);
        sack(sol, p[0], p[1], b, L, W, h, 0.0); LP(ft, rect_(p[0], p[1], L + 1.0, W, b));
    }
    if (n == 3 || RND() < 0.5) { double du = UNI(-0.3, 0.3), dv = UNI(-0.2, 0.2), b; FF(&ff, du, dv, p); b = a + PI / 2 + UNI(-0.4, 0.4); sack(sol, p[0], p[1], b, L * 0.92, W, h, h * 0.72); }
    *foot = g_buf(hull_(U_(ft)), 0.2);
    Lsteps(lay, sol, c->s[0], 0, 0, NULL, 0);
}
/* b_sack(kind="sack") */
BUILDER(B_sack, K_NONE) {
    double L, W, h; SoSolids *sol = SL();
    L = UNI(3.2, 3.8); W = UNI(2.1, 2.5);
    *foot = rect_(cx, cy, L + 1.0, W, a);
    h = UNI(0.85, 1.0); sack(sol, cx, cy, a, L, W, h, 0.0); Lsteps(lay, sol, c->s[0], 0, 0, NULL, 0);
}
/* b_log(kind="wood"): lambda u: r * (1.0 - 0.16 * u / L) + 0.14 * math.exp(-((u - kn) / 0.5) ** 2) */
typedef struct { double r, L, kn; } FLogR;
static int flog_r(void *c, double u, double *out) { FLogR *f = (FLogR *)c; double p; PW((u - f->kn) / 0.5, 2.0, p); *out = f->r * (1.0 - 0.16 * u / f->L) + 0.14 * py_exp(-p); return SO_OK; }
BUILDER(B_log, K_NONE) {
    double L, r, u, sv, b, p[2]; Frame ff; SoSolids *sol = SL(); FLogR *fr = (FLogR *)LM(sizeof *fr);
    L = UNI(6.0, 8.0); r = UNI(0.95, 1.25); ff = lf_frame(cx, cy, a);
    fr->r = r; fr->L = L; fr->kn = UNI(-L * 0.3, L * 0.3); LOFT(sol, cx, cy, a, L, flog_r, fr, 1.15, "round", 0.0, 1);
    u = UNI(-L * 0.2, L * 0.25); sv = bs_choice_pm1(rng); b = a + sv * UNI(0.9, 1.3); FF(&ff, u, sv * (r + 0.5), p);
    VAULT(sol, p[0], p[1], b, 1.7, 1.0, 0.7, "round", 0, 0, 0, 0);
    *foot = rect_(cx, cy, L, 2 * r + 0.4, a); Lsteps(lay, sol, c->s[0], 0, 0, NULL, 0);
}
/* b_logpile(kind="wood") */
BUILDER(B_logpile, K_NONE) {
    double L, r, p[2], V[3]; Frame ff; SoSolids *sol = SL(); int i, m;
    L = UNI(4.6, 5.8); r = UNI(0.75, 0.9); ff = lf_frame(cx, cy, a); V[0] = -2 * r; V[1] = 0; V[2] = 2 * r;
    for (i = 0; i < 3; i++) { double du = UNI(-0.35, 0.35), q; FF(&ff, du, V[i], p); q = UNI(0.9, 1.0); VAULT(sol, p[0], p[1], a, L * q, 2 * r, r * 1.15, "round", 0, 1, 0, 0); }
    m = (int)bs_randint(rng, 1, 2); V[0] = -r; V[1] = r;
    for (i = 0; i < m; i++) { double du = UNI(-0.35, 0.35), q; FF(&ff, du, V[i], p); q = UNI(0.85, 1.0); VAULT(sol, p[0], p[1], a, L * q, 2 * r, r * 1.15, "round", r * 1.6, 1, 0, 0); }
    *foot = rect_(cx, cy, L + 0.5, 6 * r + 0.4, a); Lsteps(lay, sol, c->s[0], 0, 0, NULL, 0);
}
/* b_urn(kind="rock") */
BUILDER(B_urn, K_NONE) {
    double r, h, pr[2 * 13]; int np; SoSolids *sol = SL(); SoCut cut;
    r = UNI(1.2, 1.55); h = UNI(2.0, 2.7);
    pr[0] = r * 0.62; pr[1] = 0.05; pr[2] = r * 0.62; pr[3] = h * 0.08; pr[4] = r; pr[5] = h * 0.42;
    np = 3 + CURVE(r, r * 0.5, h * 0.42, h * 0.84, 7, 1, pr + 6);
    pr[2 * np] = r * 0.66; pr[2 * np + 1] = h * 0.9; pr[2 * np + 2] = r * 0.66; pr[2 * np + 3] = h; np += 2;
    *foot = disc_(cx, cy, r + 0.2, 20);
    LATHE(sol, cx, cy, pr, np, 28, 1.0, 0.0, 0.0, NULL, 0);
    cut.q = disc_(cx, cy, r * 0.66 - 0.3, 16)->g; cut.fl = h - 0.55;
    Lsteps(lay, sol, c->s[0], 0, 0, &cut, 1);
}
/* b_urn_fallen(kind="rock"): lambda u: r * (0.5 + 0.5 * math.sin(math.pi * min(1.0, (u + L / 2) / L * 1.25))) */
typedef struct { double r, L; } UrnR;
static int urn_r(void *c, double u, double *out) { UrnR *f = (UrnR *)c; *out = f->r * (0.5 + 0.5 * py_sin(PI * mn(1.0, (u + f->L / 2) / f->L * 1.25))); return SO_OK; }
BUILDER(B_urn_fallen, K_NONE) {
    double r, L = 2.6, p[2]; Frame ff; SoSolids *sol = SL(); UrnR *ur = (UrnR *)LM(sizeof *ur); int i, n;
    r = UNI(1.15, 1.4); ff = lf_frame(cx, cy, a);
    ur->r = r; ur->L = L; LOFT(sol, cx, cy, a, L, urn_r, ur, 1.2, "round", 0.0, 0);
    Lsteps(lay, sol, c->s[0], 0, 0, NULL, 0);
    n = (int)bs_randint(rng, 2, 4);
    for (i = 0; i < n; i++) { double du = UNI(L / 2 + 0.5, L / 2 + 1.6), dv = UNI(-1.3, 1.3), rr; FF(&ff, du, dv, p); rr = UNI(0.55, 0.75); Lup(lay, stone_(p[0], p[1], rr, rng, 0.7, 4), 0.3, c->s[0]); }
    *foot = rectf(&ff, 0.6, 0, L + 2.2, 2 * r + 0.8, a);
}
/* b_drum_can(kind="iron") */
BUILDER(B_drum_can, K_NONE) {
    double r, h; SoSolids *sol = SL(); const char *kind = c->s[0];
    r = UNI(1.3, 1.55); h = UNI(2.1, 2.6);
    { double pr[8] = { r + 0.1, 0.0, r + 0.1, h * 0.72, r, h * 0.8, r, h }; LATHE(sol, cx, cy, pr, 4, 28, 1.0, 0.0, 0.0, NULL, 0); }
    Lsteps(lay, sol, kind, 0, 0, NULL, 0); Lon(lay, ann_(cx, cy, r + 0.2, r - 0.62, 28), 0, 0.2, kind);
    Lon(lay, disc_(cx + r * 0.4 * COS(a), cy + r * 0.4 * SIN(a), 0.3, 10), 0, 0.18, kind);
    *foot = disc_(cx, cy, r + 0.25, 24);
}
/* b_tank(kind="iron"): lambda u: r * dome(u) + (0.14 if abs(abs(u) - L * 0.24) < 0.27 else 0.0) */
typedef struct { double L, r, e; } TankR;
static int tank_r(void *c, double u, double *out) {
    TankR *t = (TankR *)c; double dome, p;
    if (fabs(u) < t->L / 2 - t->e) dome = 1.0; else { PW((fabs(u) - t->L / 2 + t->e) / t->e, 2.0, p); dome = sqrt(mx(0.0, 1 - p)); }
    *out = t->r * dome + (fabs(fabs(u) - t->L * 0.24) < 0.27 ? 0.14 : 0.0); return SO_OK;
}
BUILDER(B_tank, K_NONE) {
    double L, r, p[2]; Frame ff; SoSolids *sol = SL(); TankR *tr = (TankR *)LM(sizeof *tr); GeoList *l = LL(); const char *kind = c->s[0];
    L = UNI(6.4, 7.8); r = UNI(1.4, 1.7); ff = lf_frame(cx, cy, a);
    tr->L = L; tr->r = r; tr->e = r * 0.75; LOFT(sol, cx, cy, a, L, tank_r, tr, 1.2, "round", 0.0, 1);
    { double du = UNI(-0.5, 0.5); FF(&ff, du, 0, p); } BOX(sol, disc_(p[0], p[1], 0.5, 12), r * 1.2 + 0.3);
    LP(l, rectf(&ff, -L * 0.24, 0, 0.95, 2 * r + 1.0, a)); LP(l, rectf(&ff, L * 0.24, 0, 0.95, 2 * r + 1.0, a));
    Lsteps(lay, sol, kind, 0, 0, NULL, 0); Lup(lay, U_(l), 0.5, kind);
    *foot = rect_(cx, cy, L + 0.4, 2 * r + 1.0, a);
}
/* b_chest(kind="crate", iron="iron") */
BUILDER(B_chest, K_NONE) {
    double L, W, h, p[2]; Frame ff; SoSolids *sol = SL(); int i;
    L = UNI(3.6, 4.4); W = UNI(2.4, 2.8); h = UNI(1.0, 1.3); ff = lf_frame(cx, cy, a);
    BOX(sol, rect_(cx, cy, L, W, a), h); VAULT(sol, cx, cy, a, L, W, W * 0.42, "loaf", h, 0, 0, 0);
    for (i = 0; i < 2; i++) { double u = i ? L * 0.3 : -L * 0.3; BOX(sol, rectf(&ff, u, 0, 0.5, W + 0.24, a), h); FF(&ff, u, 0, p); VAULT(sol, p[0], p[1], a, 0.5, W + 0.24, W * 0.42 + 0.14, "loaf", h, 0, 0, 0); }
    Lsteps(lay, sol, c->s[0], 0, 0, NULL, 0); Lup(lay, rectf(&ff, 0, -W / 2 - 0.2, 0.95, 0.95, a), h * 0.8, c->s[1]);
    *foot = rect_(cx, cy, L + 0.3, W + 0.8, a);
}
/* b_stump(kind="stump"): roots lambda u: 0.55 * (1 - (u + Lr / 2) / Lr * 0.8) */
static int root_r(void *c, double u, double *out) { double Lr = *(double *)c; *out = 0.55 * (1 - (u + Lr / 2) / Lr * 0.8); return SO_OK; }
BUILDER(B_stump, K_NONE) {
    double r, h, pr[2 * 8]; int np, i, n; SoSolids *sol = SL(); GeoList *l = LL(); Geo *rings;
    r = UNI(1.4, 1.9); h = UNI(1.2, 1.8);
    np = CURVE(r * 1.45, r, 0.05, h * 0.7, 6, 0, pr); pr[2 * np] = r; pr[2 * np + 1] = h; np++;
    LATHE(sol, cx, cy, pr, np, 14, 1.0, 0.0, 0.0, NULL, 0);
    n = (int)bs_randint(rng, 3, 5);
    for (i = 0; i < n; i++) {
        double b = a + i * 6.283 / 4.3 + UNI(-0.3, 0.3), *Lr = (double *)LM(sizeof(double)), d;
        *Lr = r * UNI(1.0, 1.5); d = r * 0.8 + *Lr / 2;
        LOFT(sol, cx + d * COS(b), cy + d * SIN(b), b, *Lr, root_r, Lr, h * 0.55 / 0.55, "ridge", 0.0, 0);
    }
    LP(l, ann_(cx, cy, r * 0.75, r * 0.75 - 0.2, 14)); LP(l, ann_(cx, cy, r * 0.45, r * 0.45 - 0.2, 14));
    { double p[2] = { cx, cy }, q[2] = { cx + r * COS(a), cy + r * SIN(a) }; LP(l, bar_(p, q, 0.22)); }
    rings = g_inter(U_(l), disc_(cx, cy, r - 0.3, 14));
    *foot = disc_(cx, cy, r * 2.2, 14);
    Lsteps(lay, sol, c->s[0], 0, 0, NULL, 0); Ltcut(lay, rings, 0, 0.16);
}
/* b_wheel_up(kind="wood", iron="iron") */
BUILDER(B_wheel_up, K_NONE) {
    double R; SoSolids *sol = SL();
    R = UNI(2.2, 2.7);
    VAULT(sol, cx, cy, a + PI / 2, 0.95, 2 * R, R * 0.92, "round", 0, 0, 0, 0); VAULT(sol, cx, cy, a + PI / 2, 1.5, 0.9, R * 0.5, "round", 0, 0, 0, 0);
    *foot = rect_(cx, cy, 2 * R, 1.6, a); Lsteps(lay, sol, c->s[0], 0, 0, NULL, 0);
}
/* b_tent(kind="sail", wood="wood"): f(cx, cy, a, rng, k=1.0) */
BUILDER(B_tent, K_OPT) {
    double L, W, h; Frame ff; SoSolids *sol = SL(); GeoList *l = LL(); int i, sv;
    L = UNI(6.5, 8.0) * k; W = UNI(5.2, 6.2) * k; h = W * 0.5; ff = lf_frame(cx, cy, a);
    VAULT(sol, cx, cy, a, L, W, h, "ridge", 0, 0, 0, 0); VAULT(sol, cx, cy, a, L + 0.9, 0.5, h + 0.2, "loaf", 0, 0, 0, 0);
    { double U3[3] = { -L * 0.35, 0, L * 0.35 }; for (i = 0; i < 3; i++) for (sv = -1; sv <= 1; sv += 2) { double p[2]; FF(&ff, U3[i], sv * (W / 2 + 0.75), p); LP(l, disc_(p[0], p[1], 0.5, 8)); } }
    Lsteps(lay, sol, c->s[0], 0, 0, NULL, 0); Lup(lay, U_(l), 0.45, c->s[1]);
    *foot = rect_(cx, cy, L + 1.2, W + 2.2, a);
}
/* b_signpost(post="wood", board="wood") */
BUILDER(B_signpost, K_NONE) {
    double h, b; Frame f2; Geo *arrow; SoSolids *sol = SL();
    h = UNI(3.4, 4.2); b = a + UNI(-0.4, 0.4); f2 = lf_frame(cx, cy, b);
    { double uv[10] = { -1.2, -0.3, 1.7, -0.3, 2.4, 0, 1.7, 0.3, -1.2, 0.3 }; arrow = g_frpoly(&f2, uv, 5); }
    *foot = disc_(cx, cy, 2.6, 12);
    BOX(sol, disc_(cx, cy, 0.42, 10), h); SLAB(sol, arrow, h - 1.3, h - 0.25);
    Lsteps(lay, sol, c->s[0], 0, 0, NULL, 0); Lmound(lay, disc_(cx, cy, 0.9, 10), 0.35, "earth", ROCK_T);
}
/* b_shrooms(kind="shroom", R=(2.4, 3.1)): f(cx, cy, a, rng, k=1.0) */
BUILDER(B_shrooms, K_OPT) {
    double Rr; int n, i, any = 0; SoSolids *sol = SL();
    Rr = UNI(c->v[0], c->v[1]) * k; n = (int)bs_int((double)bs_randint(rng, 6, 9) * k);
    for (i = 0; i < n; i++) {
        double x, y, rc, h, pr[2 * 6], real[2 * 9]; int np, nr;
        if (RND() < 0.12) continue;
        x = cx + Rr * COS(a + 6.283 * i / n) + UNI(-0.15, 0.15); y = cy + Rr * SIN(a + 6.283 * i / n) + UNI(-0.15, 0.15);
        rc = UNI(0.5, 0.8); h = UNI(0.9, 1.6);
        np = CURVE(rc, 0.25, 0.0, h, 5, 1, pr);
        real[0] = 0.2; real[1] = 0.0; real[2] = 0.17; real[3] = h * 0.55; real[4] = rc; real[5] = h * 0.58; nr = 3 + CURVE(rc, 0.24, h * 0.62, h, 5, 1, real + 6);
        LATHE(sol, x, y, pr, np, 12, 1.0, 0.0, 0.0, real, nr); any = 1;
    }
    *foot = ann_(cx, cy, Rr + 0.9, mx(0.1, Rr - 0.9), 28);
    if (any) Lsteps(lay, sol, c->s[0], 0, 0, NULL, 0); else lyl_push_own(lay, NULL);          /* (foot, None): one None layer (see bld_scenes.h) */
}
/* b_table_rock(kind="rock"): f(cx, cy, a, rng, k=1.0) */
BUILDER(B_table_rock, K_OPT) {
    double r, h; SoSolids *sol = SL();
    r = UNI(1.5, 2.1) * k; h = UNI(2.6, 3.6) * mx(1.0, bs_pow(k, 0.5));
    { double hf[10] = { r * 0.75, 0.0, r * 0.62, h * 0.5, r * 0.9, h * 0.62, r, h * 0.8, r * 0.7, h },
             real[12] = { r * 0.72, 0.0, r * 0.5, h * 0.25, r * 0.42, h * 0.5, r * 0.98, h * 0.64, r, h * 0.8, r * 0.72, h };
      *foot = disc_(cx, cy, r + 0.3, 12);
      LATHE(sol, cx, cy, hf, 5, 9, 0.85, a, 0.0, real, 6); }
    Lsteps(lay, sol, c->s[0], 0, 0, NULL, 0);
}

/* ---------------------------------------------------------------- bones */
/* _knuckle(r0, ends=0.75, w=0.11): lambda t: r0 * (1 + ends * math.exp(-(t / w) ** 2) + ends * math.exp(-((1 - t) / w) ** 2)) */
typedef struct { double r0, ends, w; } Knuckle;
static int knuckle_r(void *c, double t, double *out) {
    Knuckle *k = (Knuckle *)c; double p1, p2; PW(t / k->w, 2.0, p1); PW((1 - t) / k->w, 2.0, p2);
    *out = k->r0 * (1 + k->ends * py_exp(-p1) + k->ends * py_exp(-p2)); return SO_OK;
}
static Knuckle *knuckle(double r0) { Knuckle *k = (Knuckle *)LM(sizeof *k); k->r0 = r0; k->ends = 0.75; k->w = 0.11; return k; }
/* the skull's lambdas */
typedef struct { double k, Lf; } FaceR;
static int face_r(void *c, double u, double *out) {          /* k * (1.02 - 0.2 * ((u + Lf / 2) / Lf) - 0.26 * max(0.0, (u + Lf / 2) / Lf - 0.45) ** 1.3 / 0.55 ** 1.3) */
    FaceR *f = (FaceR *)c; double p1, p2; PW(mx(0.0, (u + f->Lf / 2) / f->Lf - 0.45), 1.3, p1); PW(0.55, 1.3, p2);
    *out = f->k * (1.02 - 0.2 * ((u + f->Lf / 2) / f->Lf) - 0.26 * p1 / p2); return SO_OK;
}
static int teeth_r(void *c, double t, double *out) { double k = *(double *)c, p; PW(mx(0.0, py_cos(2 * PI * 7 * t)), 2.0, p); *out = 0.15 * k * (1 + 0.5 * p); return SO_OK; }          /* nt = 7 */
static int jaw_r(void *c, double t, double *out) { double k = *(double *)c; *out = 0.2 * k * (1.25 - fabs(t - 0.5)); return SO_OK; }
/* _skull_parts(cx, cy, a, k, rng, jaw=True) -> (foot, solids, cuts) */
typedef struct { Geo *foot; SoSolids *ps; SoCut cuts[3]; int nc; } Skull;
static void skull_parts(double cx, double cy, double a, double k, PyRandom *rng, Skull *S) {
    Frame ff = lf_frame(cx, cy, a); double cc[2], fc[2], bc[2], Lf = 2.1 * k, pr[2 * 9], *kp; int np, sv, j; FaceR *fr; SoSolids *ps = S->ps;
    FF(&ff, -0.72 * k, 0, cc);
    np = CURVE(1.38 * k, 0.3 * k, 0.0, 1.3 * k, 8, 1, pr); LATHE(ps, cc[0], cc[1], pr, np, 24, 1.16 / 1.38, a, 0.0, NULL, 0);
    FF(&ff, 1.1 * k, 0, fc); fr = (FaceR *)LM(sizeof *fr); fr->k = k; fr->Lf = Lf; LOFT(ps, fc[0], fc[1], a, Lf, face_r, fr, 1.02, "round", 0.0, 0);
    FF(&ff, 0.42 * k, 0, bc); VAULT(ps, bc[0], bc[1], a + PI / 2, 1.95 * k, 0.55 * k, 1.18 * k, "round", 0, 0, 0, 0);
    for (sv = -1; sv <= 1; sv += 2) { double pts[6]; FF(&ff, 0.75 * k, sv * 0.78 * k, pts); FF(&ff, 1.05 * k, sv * 1.02 * k, pts + 2); FF(&ff, 1.4 * k, sv * 0.8 * k, pts + 4); TUBE(ps, pts, 3, 0.2 * k, NULL, NULL, 2.6, 0.0, 0, 0.0); }
    { double pts[4]; FF(&ff, 0.55 * k, 0, pts); FF(&ff, 1.0 * k, 0, pts + 2); TUBE(ps, pts, 2, 0.17 * k, NULL, NULL, 5.2, 0.0, 0, 0.0); }
    S->nc = 0;
    for (sv = -1; sv <= 1; sv += 2) {
        double o[2]; Geo *d;
        FF(&ff, 0.82 * k, sv * 0.5 * k, o); d = disc_(o[0], o[1], 0.36 * k, 14);
        d = LA(shp_scale(d->g, 1.0, 0.85, SHP_ORIGIN_POINT, o[0], o[1]));
        d = LA(shp_rotate(d->g, a + sv * 0.25, SHP_ORIGIN_POINT, o[0], o[1], 1));
        S->cuts[S->nc].q = d->g; S->cuts[S->nc].fl = 0.28 * k; S->nc++;
    }
    { double uv[8] = { 1.08 * k, 0, 1.5 * k, -0.24 * k, 1.42 * k, 0, 1.5 * k, 0.24 * k }; Geo *h = g_buf(g_frpoly(&ff, uv, 4), 0.04); S->cuts[S->nc].q = h->g; S->cuts[S->nc].fl = 0.3 * k; S->nc++; }
    { double pts[2 * 13]; for (j = 0; j < 13; j++) { double t = -1.25 + 2.5 * j / 12; FF(&ff, 1.72 * k + 0.36 * k * COS(t), 0.62 * k * SIN(t), pts + 2 * j); }
      kp = (double *)LM(sizeof(double)); *kp = k; TUBE(ps, pts, 13, 0, teeth_r, kp, 4.2, 0.0, 0, 0.0); }
    { GeoList *l = LL(); LP(l, disc_(cc[0], cc[1], 1.45 * k, 16)); LP(l, rect_(fc[0], fc[1], Lf + 0.5, 2.3 * k, a)); S->foot = U_(l); }
    if (RND() < 0.55) {
        double jc[2], du, dv, ja, pts[10]; Frame jf;
        du = UNI(0, 0.8 * k); dv = UNI(-1.2, 1.2); FF(&ff, 2.6 * k + du, dv * k, jc); ja = a + UNI(-0.9, 0.9); jf = lf_frame(jc[0], jc[1], ja);
        FF(&jf, -0.8 * k, -0.7 * k, pts); FF(&jf, 0.1 * k, -0.62 * k, pts + 2); FF(&jf, 0.62 * k, 0, pts + 4); FF(&jf, 0.1 * k, 0.62 * k, pts + 6); FF(&jf, -0.8 * k, 0.7 * k, pts + 8);
        TUBE(ps, pts, 5, 0, jaw_r, kp, 1.5, 0.0, 0, 0.0);
        S->foot = U2_(S->foot, disc_(jc[0], jc[1], 1.0 * k, 12));
    }
}
/* b_skull(kind="bone", big=1.0) */
BUILDER(B_skull, K_NONE) {
    Skull S; double kq;
    S.ps = SL(); kq = c->v[0] * UNI(0.9, 1.15); skull_parts(cx, cy, a, kq, rng, &S);
    *foot = hull_(S.foot); Lsteps(lay, S.ps, c->s[0], 0, 0, S.cuts, S.nc);
}
/* _ribcage(cx, cy, a, Ln, kk, reach, rng) */
typedef struct { double kk; int nv; } SpineR;
static int spine_r(void *c, double t, double *out) { SpineR *s = (SpineR *)c; double p; PW(mx(0.0, py_cos(2 * PI * s->nv * t)), 3.0, p); *out = 0.44 * s->kk * (1 + 0.42 * p); return SO_OK; }
typedef struct { double kk, st; } RibR;
static int rib_r(void *c, double t, double *out) { RibR *r = (RibR *)c; *out = r->kk * (0.42 - 0.17 * t * r->st); return SO_OK; }
static void ribcage(SoSolids *ps, double cx, double cy, double a, double Ln, double kk, double reach, PyRandom *rng) {
    Frame ff = lf_frame(cx, cy, a); double bend, pts[2 * 17], sw; int i, n, sv, j; SpineR *sr = (SpineR *)LM(sizeof *sr);
    bend = UNI(-0.06, 0.06) * Ln;
    sr->kk = kk; sr->nv = (int)bs_int(Ln / (1.25 * kk)); if (sr->nv < 6) sr->nv = 6;
    for (i = 0; i < 17; i++) { double u = -Ln / 2 + Ln * i / 16; FF(&ff, u, bend * SIN(PI * (u / Ln + 0.5)), pts + 2 * i); }
    TUBE(ps, pts, 17, 0, spine_r, sr, 1.5, 0.0, 0, 0.0);
    n = (int)bs_int(Ln * 0.78 / (1.45 * kk)); if (n < 4) n = 4; sw = UNI(0.5, 1.1) * kk;
    for (i = 0; i < n; i++) {
        double u = -Ln * 0.36 + (i + 0.5) * Ln * 0.72 / n, bell = bs_pow(SIN(PI * (i + 0.5) / n), 0.7);
        for (sv = -1; sv <= 1; sv += 2) {
            double q = RND(), stop, rr_, x0, y0, path[16]; RibR *rr;
            if (q < 0.1) continue;
            stop = q < 0.24 ? UNI(0.35, 0.6) : 1.0;
            rr_ = reach * (0.5 + 0.62 * bell) * UNI(0.92, 1.06); x0 = u; y0 = bend * SIN(PI * (u / Ln + 0.5));
            for (j = 0; j < 8; j++) { double s = stop * j / 7; FF(&ff, x0 + sw * bs_pow(s, 1.6), y0 + sv * rr_ * SIN(2.05 * s) / 0.89, path + 2 * j); }
            rr = (RibR *)LM(sizeof *rr); rr->kk = kk; rr->st = stop;
            TUBE(ps, path, 8, 0, rib_r, rr, 1.7, 0.0, 0, rr_ * 0.55 * stop);
        }
    }
}
/* b_ribs(kind="bone", L=(5.5, 7.5)): f(cx, cy, a, rng, k=1.0) */
BUILDER(B_ribs, K_OPT) {
    double Ln, kk, reach; SoSolids *ps = SL();
    Ln = UNI(c->v[0], c->v[1]) * k; kk = mx(1.0, bs_pow(k, 0.5)); reach = 2.3 * mx(1.0, bs_pow(k, 0.7));
    ribcage(ps, cx, cy, a, Ln, kk, reach, rng);
    *foot = rect_(cx, cy, Ln, 2 * reach * 1.15, a); Lsteps(lay, ps, c->s[0], 0, 0, NULL, 0);
}
/* L_skeleton(kind="bone"): f(cx, cy, a, rng, k) */
BUILDER(L_skeleton, K_REQ) {
    double k9 = k * 0.9, Ln, kk, reach, tv, tail[10], hx[2], ks, sa; Frame ff; SoSolids *ps = SL(); GeoList *ft = LL(), *tl = LL(); int sv, j, i; Skull S;
    static const double LEGS[4][2] = { { -0.34, -1 }, { -0.34, 1 }, { 0.3, -1 }, { 0.3, 1 } };
    Ln = UNI(13, 17) * k9; kk = mx(1.0, bs_pow(k9, 0.5)); reach = 2.3 * mx(1.0, bs_pow(k9, 0.7)); ff = lf_frame(cx, cy, a);
    ribcage(ps, cx, cy, a, Ln, kk, reach, rng); LP(ft, rect_(cx, cy, Ln, 2 * reach * 1.15, a));
    for (sv = -1; sv <= 1; sv += 2) { double pc[2], pr[2 * 7]; int np; FF(&ff, -Ln / 2 - 0.2 * kk, sv * 1.25 * kk, pc); np = CURVE(1.45 * kk, 0.4 * kk, 0.0, 0.95 * kk, 6, 1, pr); LATHE(ps, pc[0], pc[1], pr, np, 14, 0.62, a + sv * 0.55, 0.0, NULL, 0); }
    for (j = 1; j < 5; j++) (void)bs_choice_pm1(rng);          /* the first tail: (... ) * rng.choice((-1, 1)) if j else 0, thrown away */
    tv = bs_choice_pm1(rng);
    for (j = 0; j < 5; j++) FF(&ff, -Ln / 2 - 1.3 * kk - j * 1.2 * kk, tv * 0.16 * kk * j * j, tail + 2 * j);
    for (j = 0; j < 5; j++) { double pr[2 * 5]; int np = CURVE((0.5 - 0.07 * j) * kk, 0.22 * kk, 0.0, (0.55 - 0.07 * j) * kk, 4, 1, pr); LATHE(ps, tail[2 * j], tail[2 * j + 1], pr, np, 10, 1.0, 0.0, 0.0, NULL, 0); }
    for (j = 0; j < 5; j++) LP(tl, g_ptbuf(tail[2 * j], tail[2 * j + 1], 0.6 * kk, 16));
    LP(ft, U_(tl));
    for (i = 0; i < 4; i++) {
        double su = LEGS[i][0], s2 = LEGS[i][1], b, c0[2], c1[2], Lb, du, dv, pts[4];
        if (RND() < 0.25) continue;
        b = a + s2 * UNI(0.9, 1.5); b = b + UNI(-0.3, 0.3);
        du = UNI(-0.6, 0.6); dv = UNI(0.2, 0.9); FF(&ff, su * Ln + du * kk, s2 * (reach * 0.95 + dv * kk), c0); Lb = UNI(3.0, 4.2) * kk;
        c1[0] = c0[0] + Lb * COS(b); c1[1] = c0[1] + Lb * SIN(b);
        pts[0] = c0[0]; pts[1] = c0[1]; pts[2] = c1[0]; pts[3] = c1[1];
        TUBE(ps, pts, 2, 0, knuckle_r, knuckle(0.3 * kk), 1.5, 0.0, 1, 0.0); LP(ft, bar_(c0, c1, 1.2 * kk));
    }
    FF(&ff, Ln / 2 + 2.0 * bs_pow(k, 0.6), 0, hx); ks = 1.9 * mx(1.0, bs_pow(k, 0.6)) * UNI(0.9, 1.1);
    sa = a + UNI(-0.35, 0.35); S.ps = SL(); skull_parts(hx[0], hx[1], sa, ks, rng, &S); LP(ft, S.foot);
    sol_extend(ps, S.ps);
    *foot = hull_(U_(ft)); Lsteps(lay, ps, c->s[0], 0, 0, S.cuts, S.nc);
}
/* b_bone(kind="bone"): f(cx, cy, a, rng, k=1.0) */
BUILDER(B_bone, K_OPT) {
    double L, pts[4]; Frame ff; SoSolids *ps = SL();
    L = UNI(2.6, 3.8) * k; ff = lf_frame(cx, cy, a);
    *foot = rect_(cx, cy, L + 1.0, 1.4 * k, a);
    FF(&ff, -L / 2, 0, pts); FF(&ff, L / 2, 0, pts + 2);
    TUBE(ps, pts, 2, 0, knuckle_r, knuckle(0.3 * mx(1.0, bs_pow(k, 0.5))), 1.4, 0.0, 1, 0.0);
    Lsteps(lay, ps, c->s[0], 0, 0, NULL, 0);
}

/* ---------------------------------------------------------------- _several, _either, the wrappers */
/* _several(build, n=(2, 4), spread=2.5) */
BUILDER(B_several, K_NONE) {
    int n, i, t, j; double sp = c->v[0]; GeoList *ft = LL();
    n = (int)bs_randint(rng, c->i[0], c->i[1]);
    for (i = 0; i < n; i++) for (t = 0; t < 12; t++) {
        double x = cx + UNI(-sp, sp), y = cy + UNI(-sp, sp), ang = UNI(0, 6.283); Geo *f; SoLayers *ly; int ok = 1;
        if (CALLB(&c->b[0], x, y, ang, rng, 0, 1.0, &f, &ly) == 1 || !f) ly_raise();          /* ft, ly = None: TypeError */
        for (j = 0; j < ft->n; j++) if (!(g_dist(f, ft->v[j]) > 0.3)) { ok = 0; break; }
        if (ok) { LP(ft, f); Lshift(lay, ly, lay->n); break; }
    }
    if (!ft->n) { *none = 1; return; }
    *foot = hull_(U_(ft));
}
/* _either(f1, f2, share=0.5) */
static int B_either(void *c_, double cx, double cy, double a, PyRandom *rng, int hs, double k, Geo **foot, SoLayers *L) {
    BC *c = (BC *)c_; const BsBuild *b = py_random(rng) < c->v[0] ? &c->b[1] : &c->b[0];
    return b->fn(b->ctx, cx, cy, a, rng, hs, k, foot, L);
}
/* _turn(build, da): lambda cx, cy, a, rng, k: build(cx, cy, a + da, rng, k) */
static int W_turn(void *c_, double cx, double cy, double a, PyRandom *rng, int hs, double k, Geo **foot, SoLayers *L) {
    BC *c = (BC *)c_; if (!hs) return SC_ERR;
    return c->b[0].fn(c->b[0].ctx, cx, cy, a + c->v[0], rng, 1, k, foot, L);
}
/* _grow(mk, m=1.3): lambda cx, cy, a, rng, k: b(cx, cy, a, rng, k * m) */
static int W_grow(void *c_, double cx, double cy, double a, PyRandom *rng, int hs, double k, Geo **foot, SoLayers *L) {
    BC *c = (BC *)c_; if (!hs) return SC_ERR;
    return c->b[0].fn(c->b[0].ctx, cx, cy, a, rng, 1, k * c->v[0], foot, L);
}
/* _worn(mk): got = b(cx, cy, a, rng, k); got if got is None or got[0] is None else (got[0], _weather(got[1], rng)) */
static int W_worn(void *c_, double cx, double cy, double a, PyRandom *rng, int hs, double k, Geo **foot, SoLayers *L) {
    BC *c = (BC *)c_; SoLayers got = { 0 }, out = { 0 }; int r, i;
    if (!hs) return SC_ERR;
    r = c->b[0].fn(c->b[0].ctx, cx, cy, a, rng, 1, k, foot, &got);
    if (r != 0 || !*foot) { for (i = 0; i < got.n; i++) lyl_push(L, got.v[i]); lyl_free(&got); return r; }
    if (so_weather(&got, rng, 0.6, &out) != SO_OK) { lyl_free(&got); lyl_free(&out); geo_unref(*foot); *foot = NULL; return SC_ERR; }
    for (i = 0; i < out.n; i++) lyl_push(L, out.v[i]);
    lyl_free(&got); lyl_free(&out);
    return 0;
}
/* "carcass": C(lambda cx, cy, a, rng: F.L_skeleton(p["bone"])(cx, cy, a, rng, 0.45)) */
static int B_carcass(void *c_, double cx, double cy, double a, PyRandom *rng, int hs, double k, Geo **foot, SoLayers *L) {
    BC *c = (BC *)c_; BsFrame F; BsBuild b; int r;
    if (hs) return SC_ERR;
    bs_begin(&F);
    if (setjmp(F.jb)) { bs_end(&F); *foot = NULL; return SC_ERR; }
    b = FEAT1("L_skeleton", c->s[0]);
    bs_end(&F);
    return b.fn(b.ctx, cx, cy, a, rng, 1, 0.45, foot, L);
}

static BsBuild mkb(FtBuildFn fn, BC *c, const char *tag, const char *kind) { BsBuild b; b.fn = fn; b.ctx = c; b.tag = tag ? I_(tag) : NULL; b.kind = kind ? I_(kind) : NULL; return b; }
static BsBuild W1(FtBuildFn fn, BsBuild inner, double v) { BC *c = bc_new(); c->b[0] = inner; c->v[0] = v; return mkb(fn, c, NULL, NULL); }

/* ================================================================ the table of b_* and their arguments */
/* an FtCall argument by position or keyword; Python's default when it is not given */
static const FtArg *arg_of(const FtCall *call, int pos, const char *kw) {
    int i, np = 0;
    for (i = 0; i < call->nargs; i++) { const FtArg *a = &call->a[i]; if (a->kw && kw && !strcmp(a->kw, kw)) return a; }
    for (i = 0; i < call->nargs; i++) { const FtArg *a = &call->a[i]; if (!a->kw) { if (np == pos) return a; np++; } }
    return NULL;
}
static const char *as_s(const FtCall *call, int pos, const char *kw, const char *dflt) { const FtArg *a = arg_of(call, pos, kw); return I_(a ? (a->t == 's' ? a->s : NULL) : dflt); }
static double as_f(const FtCall *call, int pos, const char *kw, double dflt) { const FtArg *a = arg_of(call, pos, kw); return a && a->n ? a->v[0] : dflt; }
static void as_t2(const FtCall *call, int pos, const char *kw, double d0, double d1, double *v) { const FtArg *a = arg_of(call, pos, kw); if (a && a->n >= 2) { v[0] = a->v[0]; v[1] = a->v[1]; } else { v[0] = d0; v[1] = d1; } }

typedef struct { const char *name; FtBuildFn fn; int ns; const char *kw[2], *dflt[2]; const char *tag; int tagkind; } BDef;
/* tagkind: which string argument is f.kind (0 = the first) */
static const BDef BDEFS[] = {
    { "b_crate", B_crate, 2, { "kind", "wood" }, { "crate", "wood" }, "crate", 0 },
    { "b_crate_stack", B_crate_stack, 2, { "kind", "wood" }, { "crate", "wood" }, "crates", 0 },
    { "b_keg", B_keg, 2, { "kind", "iron" }, { "wood", "iron" }, "keg", 0 },
    { "b_barrel", B_barrel, 2, { "kind", "iron" }, { "wood", "iron" }, "barrel", 0 },
    { "b_barrels", B_barrels, 2, { "kind", "iron" }, { "wood", "iron" }, "barrels", 0 },
    { "b_sacks", B_sacks, 1, { "kind", NULL }, { "sack", NULL }, "sacks", 0 },
    { "b_sack", B_sack, 1, { "kind", NULL }, { "sack", NULL }, "sack", 0 },
    { "b_log", B_log, 1, { "kind", NULL }, { "wood", NULL }, "log", 0 },
    { "b_logpile", B_logpile, 1, { "kind", NULL }, { "wood", NULL }, "logpile", 0 },
    { "b_urn", B_urn, 1, { "kind", NULL }, { "rock", NULL }, "urn", 0 },
    { "b_urn_fallen", B_urn_fallen, 1, { "kind", NULL }, { "rock", NULL }, "urn", 0 },
    { "b_drum_can", B_drum_can, 1, { "kind", NULL }, { "iron", NULL }, "drum", 0 },
    { "b_tank", B_tank, 1, { "kind", NULL }, { "iron", NULL }, "tank", 0 },
    { "b_chest", B_chest, 2, { "kind", "iron" }, { "crate", "iron" }, "chest", 0 },
    { "b_stump", B_stump, 1, { "kind", NULL }, { "stump", NULL }, "stump", 0 },
    { "b_wheel_up", B_wheel_up, 2, { "kind", "iron" }, { "wood", "iron" }, "wheel", 0 },
    { "b_tent", B_tent, 2, { "kind", "wood" }, { "sail", "wood" }, "tent", 0 },
    { "b_signpost", B_signpost, 2, { "post", "board" }, { "wood", "wood" }, "sign", 0 },
    { "b_shrooms", B_shrooms, 1, { "kind", NULL }, { "shroom", NULL }, "fairy_ring", 0 },
    { "b_table_rock", B_table_rock, 1, { "kind", NULL }, { "rock", NULL }, "hoodoo", 0 },
    { "b_skull", B_skull, 1, { "kind", NULL }, { "bone", NULL }, "skull", 0 },
    { "b_ribs", B_ribs, 1, { "kind", NULL }, { "bone", NULL }, "ribs", 0 },
    { "b_bone", B_bone, 1, { "kind", NULL }, { "bone", NULL }, "bone", 0 },
    { "L_skeleton", L_skeleton, 1, { "kind", NULL }, { "bone", NULL }, NULL, 0 },
};
#define NBDEFS ((int)(sizeof BDEFS / sizeof BDEFS[0]))
static int bdef_make(const BDef *d, const FtCall *call, BsBuild *out) {
    BC *c = bc_new(); int i;
    for (i = 0; i < d->ns; i++) c->s[i] = as_s(call, i, d->kw[i], d->dflt[i]);
    if (d->fn == B_shrooms) as_t2(call, 1, "R", 2.4, 3.1, c->v);
    if (d->fn == B_skull) c->v[0] = as_f(call, 1, "big", 1.0);
    if (d->fn == B_ribs) as_t2(call, 1, "L", 5.5, 7.5, c->v);
    *out = mkb(d->fn, c, d->tag, d->tag ? c->s[d->tagkind] : NULL);
    return 0;
}
static const BDef *bdef_find(const char *name) {
    int i;
    for (i = 0; i < NBDEFS; i++) if (!strcmp(BDEFS[i].name, name) || (BDEFS[i].name[0] == 'b' && !strcmp(BDEFS[i].name + 2, name)) || (BDEFS[i].name[0] == 'L' && !strcmp(name, "skeleton"))) return &BDEFS[i];
    return NULL;
}
int bs_make(const FtCall *call, BsBuild *out) {
    const BDef *d;
    if (!strcmp(call->name, "_carcass")) {          /* the "carcass" prop's lambda (made here for the proof) */
        BC *c = bc_new(); const FtArg *a = arg_of(call, 0, NULL); c->s[0] = I_(a && a->t == 's' ? a->s : NULL); *out = mkb(B_carcass, c, NULL, NULL); return 0;
    }
    d = bdef_find(call->name);
    if (!d) return -2;
    return bdef_make(d, call, out);
}
static int bld_fac(const FtCall *call, FtBuildFn *fn, void **ctx) { BsBuild b; int r = bs_make(call, &b); if (r == 0) { *fn = b.fn; *ctx = b.ctx; } return r; }
int bs_several(const BsBuild *inner, int n0, int n1, double spread, BsBuild *out) {
    BC *c = bc_new(); c->b[0] = *inner; c->i[0] = n0; c->i[1] = n1; c->v[0] = spread;
    *out = mkb(B_several, c, inner->tag ? inner->tag : "", inner->kind ? inner->kind : ""); return 0;
}
int bs_either(const BsBuild *f1, const BsBuild *f2, double share, BsBuild *out) {
    BC *c = bc_new(); c->b[0] = *f1; c->b[1] = *f2; c->v[0] = share;
    *out = mkb(B_either, c, f1->tag ? f1->tag : "", f1->kind ? f1->kind : ""); return 0;
}
static BsBuild BMK(const char *name, const char *s0, const char *s1) {          /* scenes.<name>(s0, s1) */
    FtCall c; BsBuild b; memset(&c, 0, sizeof c); c.mod = "scenes"; c.name = name;
    if (s0) c.a[c.nargs++] = AS(s0);
    if (s1) c.a[c.nargs++] = AS(s1);
    if (bs_make(&c, &b) != 0) ly_raise();
    return b;
}

/* ================================================================ SUBJECTS */
const char *const BS_SUBJECTS[] = { "wall", "dais_brazier", "dais_column", "dais_altar", "skeleton", "crater", "colonnade", "hull", "vehicle", "tree", "circle",
    "ruin", "pool", "nest", "spires", "tentacles", "portal", "shaft", "camp", "depot", "graves", "wagon", "corral", "machine", "emplacement", "tent",
    "log", "well", "statue", "organ" };
const int BS_NSUBJECTS = (int)(sizeof BS_SUBJECTS / sizeof BS_SUBJECTS[0]);
static BsBuild SB(FtBuildFn fn, const char *s0, const char *s1, const char *s2) { BC *c = bc_new(); c->s[0] = s0; c->s[1] = s1; c->s[2] = s2; return mkb(fn, c, NULL, NULL); }
static BsBuild subject_(const char *name, const ScPal *p) {
    const char *fl = OR2(p->floor, p->stone);
#define IS(x) (!strcmp(name, x))
    if (IS("wall")) return W1(W_worn, W1(W_turn, FEAT1("L_wall", KF(p->stone)), PI / 2), 0);
    if (IS("dais_brazier")) { FtArg a[4]; a[0] = AS(fl); a[1] = AS("brazier"); a[2] = AS("fire"); a[3] = AS(p->iron); return W1(W_turn, FEAT("L_dais", 4, a), -PI / 7); }
    if (IS("dais_column")) return W1(W_turn, FEAT2("L_dais", fl, "column"), -PI / 7);
    if (IS("dais_altar")) return W1(W_turn, FEAT2("L_dais", fl, "altar"), -PI / 7);
    if (IS("skeleton")) return W1(W_turn, FEAT1("L_skeleton", p->bone), PI / 2);
    if (IS("crater")) return FEAT2("L_crater", OR2(p->rock, p->stone), p->pit);
    if (IS("colonnade")) { const char *st = KF(p->stone); return W1(W_turn, FEAT2("L_colonnade", st, fl), PI / 2); }
    if (IS("hull")) { const char *w1 = KF(p->wood), *w2 = KF(p->wood); return W1(W_turn, FEAT2("L_hull", w1, w2), PI / 2); }
    if (IS("vehicle")) { const char *b = KF(OR2(p->wood, "iron")), *t = KF(OR2(p->iron, "iron")); return W1(W_turn, FEAT2("L_vehicle", b, t), PI / 2); }          /* a wrecked tank or truck */
    if (IS("tree")) return FEAT2("L_tree", "stump", "root");
    if (IS("circle")) return FEAT2("L_circle", "glyph", KF(p->stone));
    if (IS("ruin")) return W1(W_worn, SB(S_ruin, KF(p->stone), fl, NULL), 0);
    if (IS("pool")) return FEAT2("L_pool", OR2(p->rock, p->stone), p->water);
    if (IS("nest")) return FEAT2("L_nest", p->twig, "egg");
    if (IS("spires")) return FEAT1("L_spires", OR2(OR2(p->gem, p->rock), p->stone));
    if (IS("tentacles")) return FEAT2("L_tentacles", (strcmp(p->vein ? p->vein : "", "vein") || (p->soft && *p->soft)) ? p->vein : "tendril", p->pit);
    if (IS("portal")) return W1(W_worn, SB(S_portal, KF(p->stone), fl, p->pit), 0);
    if (IS("shaft")) return W1(W_grow, SB(S_shaft, KF(p->wood), p->pit, NULL), 1.3);
    if (IS("camp")) return W1(W_grow, SB(S_camp, OR2(p->rock, p->stone), p->wood, p->iron), 1.3);
    if (IS("depot")) return W1(W_grow, SB(S_depot, KF(p->wood), p->wood, p->crate), 1.3);
    if (IS("graves")) { const char *st = KF(p->stone); return W1(W_grow, SB(S_graves, st, OR2(p->soft, "earth"), p->pit), 1.3); }
    if (IS("wagon")) return W1(W_grow, SB(S_wagon, KF(p->wood), p->wood, p->iron), 1.3);
    if (IS("corral")) return SB(S_corral, KF(p->wood), NULL, NULL);
    if (IS("machine")) return W1(W_grow, SB(S_machine, KF(p->iron), NULL, NULL), 1.3);
    if (IS("emplacement")) return SB(S_emplacement, p->iron, p->wood, NULL);
    if (IS("tent")) return W1(W_grow, SB(S_tent, p->wood, NULL, NULL), 1.3);
    if (IS("log")) return SB(S_log, KF(p->wood), p->pit, NULL);
    if (IS("well")) return W1(W_grow, SB(S_well, KF(p->stone), p->water, p->wood), 1.3);
    if (IS("statue")) return W1(W_worn, W1(W_grow, SB(S_statue, KF(p->stone), NULL, NULL), 1.3), 0);
    if (IS("organ")) return SB(S_organ, OR2(p->soft, "flesh"), p->vein, NULL);
#undef IS
    ly_raise();          /* SUBJECTS[name]: KeyError */
    { BsBuild z; memset(&z, 0, sizeof z); return z; }
}
int bs_subject(const char *name, const ScPal *pal, FtBuildFn *fn, void **ctx) {
    BsFrame F; BsBuild b;
    bs_begin(&F);
    if (setjmp(F.jb)) { bs_end(&F); return SC_ERR; }
    b = subject_(name, pal);
    bs_end(&F);
    *fn = b.fn; *ctx = b.ctx;
    return 0;
}

/* ================================================================ the props */
/* A prop is a recipe (what _props() writes for it), read twice: by bs_prop_fn to make the detail function, by
   bs_prop_desc to say what it is made of (the proof). */
enum { P_NONE = 0, P_COMP, P_CALL, P_REF, P_ORE };
enum { S_FEAT = 0, S_SCENES, S_EITHER, S_SEVERAL, S_CARCASS };
typedef struct BSpec { int t; const char *name; FtCall call; const struct BSpec *a, *b; double share, spread; int n0, n1; } BSpec;
typedef struct { int t; BSpec b[3]; int top; BsCall call; const char *ref, *rock, *gem; } Prop;
FtS3dList *bs_s3d = NULL;
static FtS3dList S3D_OWN;

static FtCall CALLV(const char *mod, const char *name, int n, const FtArg *a) { FtCall c; int i; memset(&c, 0, sizeof c); c.mod = mod; c.name = name; c.nargs = n; for (i = 0; i < n && i < 6; i++) c.a[i] = a[i]; return c; }
static FtArg ASH(const char *nm, int n, const double *v, unsigned im) { FtArg a; int i; memset(&a, 0, sizeof a); a.t = 'S'; a.s = nm; a.n = n; for (i = 0; i < n; i++) a.v[i] = v[i]; a.imask = im; return a; }
static FtArg APAT(const char *nm, double v) { FtArg a; memset(&a, 0, sizeof a); a.t = 'P'; a.s = nm; a.n = 1; a.v[0] = v; return a; }
static FtArg AR2(double f0, double a0, double f1, double a1) { FtArg a; memset(&a, 0, sizeof a); a.t = 'R'; a.n = 4; a.v[0] = f0; a.v[1] = a0; a.v[2] = f1; a.v[3] = a1; a.imask = 2 | 8; return a; }
static FtArg AB(int b) { FtArg a; memset(&a, 0, sizeof a); a.t = 'b'; a.n = 1; a.v[0] = b; a.imask = 1; return a; }
static FtArg KW(FtArg a, const char *kw) { a.kw = kw; return a; }
static FtArg ANONE(void) { FtArg a; memset(&a, 0, sizeof a); a.t = 'N'; return a; }
static FtArg ASN(const char *s) { return s ? AS(s) : ANONE(); }

static BSpec BF(const char *name, int n, const FtArg *a) { BSpec s; memset(&s, 0, sizeof s); s.t = S_FEAT; s.name = name; s.call = CALLV("features", name, n, a); return s; }
static BSpec BS(const char *name, int n, const FtArg *a) { BSpec s; memset(&s, 0, sizeof s); s.t = S_SCENES; s.name = name; s.call = CALLV("scenes", name, n, a); return s; }
static void comp1(Prop *P, BSpec b) { P->t = P_COMP; P->b[0] = b; P->top = 0; }
static BsCall BCALL(const char *mod, const char *name, int n, const FtArg *a) { BsCall c; int i; memset(&c, 0, sizeof c); c.mod = mod; c.name = name; c.nargs = n; for (i = 0; i < n && i < 10; i++) c.a[i] = a[i]; return c; }
static void call1(Prop *P, const char *mod, const char *name, int n, const FtArg *a) { P->t = P_CALL; P->call = BCALL(mod, name, n, a); }
#define A(...) ((const FtArg[]){ __VA_ARGS__ })
#define NA(...) (int)(sizeof((FtArg[]){ __VA_ARGS__ }) / sizeof(FtArg))
#define FEATC(P, nm, ...) comp1(P, BF(nm, NA(__VA_ARGS__), A(__VA_ARGS__)))
#define SCENC(P, nm, ...) comp1(P, BS(nm, NA(__VA_ARGS__), A(__VA_ARGS__)))
#define DCALL(P, nm, ...) call1(P, "details", nm, NA(__VA_ARGS__), A(__VA_ARGS__))
#define S2CALL(P, nm, ...) call1(P, "styles2", nm, NA(__VA_ARGS__), A(__VA_ARGS__))
static FtArg SH(const char *nm, double a0, double a1, double a2, double a3, int n, unsigned im) { double v[4] = { a0, a1, a2, a3 }; return ASH(nm, n, v, im); }

const char *const BS_PROP_NAMES[] = { "pick", "shovel", "axe", "sword", "spear", "rifle", "anchor", "wrench", "wheel", "ladder", "cart", "chest", "sign",
    "barrel", "keg", "crate", "crates", "timbers", "planks", "beam", "campfire", "brazier", "wellhead", "skull", "ribs", "bones", "carcass", "grate",
    "gear", "lid", "tiles", "slab", "steps", "drum", "stub", "runes", "crystals", "stump", "puddle", "nest", "fairy", "tentacle", "pothole", "posts",
    "headstone", "coffin", "lantern", "teeth", "sandbags", "trapdoor", "vent", "sacks", "sack", "barrels", "log", "logpile", "urn", "pot", "oildrum",
    "tank", "rope", "chain", "cables", "casings", "coins", "eggs", "pustules", "orepile", "holes", "rocks", "boulder", "pebbles", "rubble", "shrubs",
    "bush", "crack", "prints" };
const int BS_NPROPS = (int)(sizeof BS_PROP_NAMES / sizeof BS_PROP_NAMES[0]);

/* _props()[name](p): the recipe; 0 = no such prop (PROPS.get(name) is None), 1 = a recipe (P_NONE: the lambda gave None) */
static int prop_recipe(const char *name, const ScPal *p, Prop *P) {
    const char *R = OR2(p->rock, p->stone);
    static const char *const TOOLS[] = { "pick", "shovel", "axe", "sword", "spear", "rifle", "anchor", "wrench" };
    int i;
    memset(P, 0, sizeof *P);
#define IS(x) (!strcmp(name, x))
    for (i = 0; i < 8; i++) if (IS(TOOLS[i])) { FEATC(P, "b_tool", AS(TOOLS[i]), ASN(p->iron), ASN(p->wood)); return 1; }
    if (IS("wheel")) {
        P->t = P_COMP; P->b[1] = BF("b_wheel", 2, A(ASN(p->wood), ASN(p->iron))); P->b[2] = BS("b_wheel_up", 2, A(ASN(p->wood), ASN(p->iron)));
        P->b[0].t = S_EITHER; P->b[0].name = "_either"; P->b[0].a = &P->b[1]; P->b[0].b = &P->b[2]; P->b[0].share = 0.4; return 1;
    }
    if (IS("ladder")) { FEATC(P, "b_ladder", ASN(p->wood)); return 1; }
    if (IS("cart")) { FEATC(P, "b_cart", ASN(p->wood), ASN(p->iron)); return 1; }
    if (IS("chest")) { SCENC(P, "b_chest", ASN(p->crate), ASN(p->iron)); return 1; }
    if (IS("sign")) { if (SUP()) SCENC(P, "b_signpost", ASN(p->wood), ASN(p->wood)); else FEATC(P, "b_sign", ASN(p->wood), ASN(p->wood)); return 1; }
    if (IS("barrel")) { SCENC(P, "b_barrel", ASN(p->wood), ASN(p->iron)); return 1; }
    if (IS("keg")) { SCENC(P, "b_keg", ASN(p->wood), ASN(p->iron)); return 1; }
    if (IS("crate")) { SCENC(P, "b_crate", ASN(p->crate), ASN(p->wood)); return 1; }
    if (IS("crates")) { SCENC(P, "b_crate_stack", ASN(p->crate), ASN(p->wood)); return 1; }
    if (IS("timbers")) { FEATC(P, "b_pile", ASN(p->wood), AT2(4.2, 5.4, 0), AF(1.0)); return 1; }
    if (IS("planks")) { DCALL(P, "scatter", SH("caps", 2.6, 3.6, 0.6, 0, 3, 0), AT2(2, 3, 3), AT2(0.5, 0.7, 0), ASN(p->wood), AS("up"), AF(1.8)); return 1; }
    if (IS("beam")) { DCALL(P, "single", SH("caps", 5.0, 7.0, 1.0, 0, 3, 0), AT2(0.6, 0.8, 0), ASN(p->wood), AS("up"), APAT("seams", 1.3)); return 1; }
    if (IS("campfire")) { FEATC(P, "b_campfire", ASN(R), ASN(p->wood)); return 1; }
    if (IS("brazier")) { FEATC(P, "b_brazier", ASN(p->iron), AS("fire")); return 1; }
    if (IS("wellhead")) { FEATC(P, "b_well", ASN(R), ASN(p->water)); return 1; }
    if (IS("skull")) { FEATC(P, "b_skull", ASN(p->bone), AF(1.2)); return 1; }
    if (IS("ribs")) { FEATC(P, "b_ribs", ASN(p->bone)); return 1; }
    if (IS("bones")) {
        P->t = P_COMP; P->b[1] = BS("b_bone", 1, A(ASN(p->bone)));
        P->b[0].t = S_SEVERAL; P->b[0].name = "_several"; P->b[0].a = &P->b[1]; P->b[0].n0 = 2; P->b[0].n1 = 4; P->b[0].spread = 2.6; return 1;
    }
    if (IS("carcass")) { P->t = P_COMP; P->b[0].t = S_CARCASS; P->b[0].name = "carcass"; P->b[0].call = CALLV("features", "L_skeleton", 1, A(ASN(p->bone))); return 1; }
    if (IS("grate")) { FEATC(P, "b_grate", ASN(p->iron)); return 1; }
    if (IS("gear")) { FEATC(P, "b_gear", ASN(p->iron)); return 1; }
    if (IS("lid")) { FEATC(P, "b_boss", ASN(p->iron), ASN(p->iron)); return 1; }
    if (IS("tiles")) { FEATC(P, "b_tiles", ASN(OR2(p->floor, p->stone))); return 1; }
    if (IS("slab")) { FEATC(P, "b_cracked_slab", ASN(p->stone)); return 1; }
    if (IS("steps")) { FEATC(P, "b_stairs", ASN(OR2(p->floor, p->stone))); return 1; }
    if (IS("drum")) { FEATC(P, "b_fallen_column", ASN(p->stone)); return 1; }
    if (IS("stub")) { S2CALL(P, "f_column", ASN(p->stone)); return 1; }
    if (IS("runes")) { FEATC(P, "b_rune_circle", AS("glyph")); return 1; }
    if (IS("crystals")) { if (p->gem && *p->gem) FEATC(P, "b_crystals", AS(p->gem)); return 1; }
    if (IS("stump")) { SCENC(P, "b_stump", AS("stump")); return 1; }
    if (IS("puddle")) { DCALL(P, "sunk", SH("blob", 1.3, 1.9, 0, 0, 2, 0), AT2(0.3, 0.45, 0), ASN(p->water)); return 1; }
    if (IS("nest")) { FEATC(P, "b_nest", ASN(p->twig), AS("egg")); return 1; }
    if (IS("fairy")) { if (SUP()) SCENC(P, "b_shrooms", AS("shroom")); else FEATC(P, "b_fairy_ring", AS("shroom")); return 1; }
    if (IS("tentacle")) { FEATC(P, "b_tentacle", AS(p->vein && strcmp(p->vein, "vein") ? p->vein : "tendril")); return 1; }
    if (IS("pothole")) { FEATC(P, "b_crater", ASN(R), ASN(p->pit)); return 1; }
    if (IS("posts")) { S2CALL(P, "f_post_row", ASN(p->wood), AT2(1.8, 2.3, 0), AB(1), AT2(2, 3, 3), AF(4.2)); return 1; }
    { static const char *const REFS[] = { "headstone", "coffin", "lantern", "teeth", "sandbags", "trapdoor" };
      for (i = 0; i < 6; i++) if (IS(REFS[i])) { P->t = P_REF; P->call = BCALL("styles2", NULL, -1, NULL); P->ref = REFS[i]; return 1; } }
    if (IS("vent")) { S2CALL(P, "f_vent", ASN(p->pit), ASN(p->stone)); return 1; }
    if (IS("sacks")) { SCENC(P, "b_sacks", AS("sack")); return 1; }
    if (IS("sack")) { SCENC(P, "b_sack", AS("sack")); return 1; }
    if (IS("barrels")) { SCENC(P, "b_barrels", ASN(p->wood), ASN(p->iron)); return 1; }
    if (IS("log")) { SCENC(P, "b_log", ASN(p->wood)); return 1; }
    if (IS("logpile")) { SCENC(P, "b_logpile", ASN(p->wood)); return 1; }
    if (IS("urn")) { SCENC(P, "b_urn", AS("clay")); return 1; }
    if (IS("pot")) { SCENC(P, "b_urn_fallen", AS("clay")); return 1; }
    if (IS("oildrum")) { SCENC(P, "b_drum_can", ASN(p->iron)); return 1; }
    if (IS("tank")) { SCENC(P, "b_tank", ASN(p->iron)); return 1; }
    if (IS("rope")) { DCALL(P, "path", AF(0.5), AT2(6, 10, 3), AF(0.4), AS("rope"), AS("up"), AF(0.9)); return 1; }
    if (IS("chain")) { DCALL(P, "path", AF(0.6), AT2(7, 11, 3), AF(0.4), AS("chain"), AS("up"), AF(1.0)); return 1; }
    if (IS("cables")) { DCALL(P, "path", AF(0.6), AT2(7, 11, 3), AF(0.4), AS("cable"), AS("up"), AF(0.9)); return 1; }
    if (IS("casings")) { DCALL(P, "scatter", SH("caps", 0.7, 1.0, 0.3, 0, 3, 0), AT2(5, 8, 3), AT2(0.3, 0.35, 0), ASN(p->iron), AS("up"), AF(1.4)); return 1; }
    if (IS("coins")) { DCALL(P, "scatter", SH("disc", 0.4, 0.5, 12, 0, 3, 4), AT2(5, 9, 3), AT2(0.3, 0.35, 0), AS("gold"), AS("up"), AF(1.2)); return 1; }
    if (IS("eggs")) { DCALL(P, "scatter", SH("ell", 0.6, 0.8, 0.9, 1.2, 4, 0), AT2(3, 6, 3), AT2(0.7, 1.0, 0), AS("egg"), AS("mound"), AF(1.6), AR2(0.55, 25, 0.45, 50)); return 1; }
    if (IS("pustules")) { DCALL(P, "scatter", SH("disc", 0.6, 0.9, 16, 0, 3, 4), AT2(3, 6, 3), AT2(0.7, 1.0, 0), AS("pustule"), AS("mound"), AF(1.6), AR2(0.55, 25, 0.45, 50)); return 1; }
    if (IS("orepile")) { P->t = P_ORE; P->rock = R; P->gem = p->gem; return 1; }
    if (IS("holes")) { DCALL(P, "sunk", SH("disc", 0.4, 0.55, 10, 0, 3, 4), AT2(0.6, 0.8, 0), AS("pit"), AT2(4, 6, 3), AF(1.4)); return 1; }
    if (IS("rocks")) { if (p->rock && *p->rock) DCALL(P, "scatter", SH("rock", 0.9, 1.4, 0, 0, 2, 0), AT2(3, 5, 3), AT2(0.6, 1.0, 0), ASN(R)); return 1; }
    if (IS("boulder")) { if (SUP()) SCENC(P, "b_table_rock", ASN(R)); else DCALL(P, "single", SH("rock", 1.8, 2.5, 0, 0, 2, 0), AT2(1.1, 1.7, 0), ASN(R), AS("mound"), KW(AR2(0.7, 6, 0.3, 40), "tiers")); return 1; }
    if (IS("pebbles")) { DCALL(P, "scatter", SH("round", 0.5, 0.75, 0, 0, 2, 0), AT2(6, 10, 3), AT2(0.35, 0.5, 0), AS("pebble"), AS("mound"), AF(1.6), AR2(0.7, 10, 0.3, 45)); return 1; }
    if (IS("rubble")) { DCALL(P, "scatter", SH("tri", 0.7, 1.1, 0, 0, 2, 0), AT2(4, 7, 3), AT2(0.4, 0.7, 0), ASN(R), AS("mound"), AF(1.8), AR2(0.7, 6, 0.3, 40)); return 1; }
    if (IS("shrubs")) { if (p->plant && *p->plant) DCALL(P, "scatter", SH("star", 0.9, 1.3, 5, 8, 4, 12), AT2(2, 4, 3), AT2(0.8, 1.2, 0), AS(p->plant), AS("mound"), AF(2.0), AR2(0.55, 25, 0.45, 50)); return 1; }
    if (IS("bush")) { if (p->plant && *p->plant) DCALL(P, "single", SH("star", 1.5, 2.1, 6, 9, 4, 12), AT2(1.0, 1.4, 0), AS(p->plant), AS("mound"), KW(AR2(0.55, 25, 0.45, 50), "tiers")); return 1; }
    if (IS("crack")) { DCALL(P, "sunk", SH("wig", 5, 8, 0.45, 0.9, 4, 3), AT2(0.4, 0.5, 0), AS("crack")); return 1; }
    if (IS("prints")) { DCALL(P, "trail", AF(1.3), AT2(5, 8, 3)); return 1; }
#undef IS
    return 0;
}

/* ---- making it */
static BsBuild spec_make(const BSpec *s) {
    BsBuild b, x, y;
    switch (s->t) {
    case S_FEAT: return FEAT(s->name, s->call.nargs, s->call.a);
    case S_SCENES: if (bs_make(&s->call, &b) != 0) ly_raise(); return b;
    case S_EITHER: x = spec_make(s->a); y = spec_make(s->b); bs_either(&x, &y, s->share, &b); return b;
    case S_SEVERAL: x = spec_make(s->a); bs_several(&x, s->n0, s->n1, s->spread, &b); return b;
    case S_CARCASS: { BC *c = bc_new(); c->s[0] = I_(s->call.a[0].s); return mkb(B_carcass, c, NULL, NULL); }
    }
    ly_raise(); memset(&b, 0, sizeof b); return b;
}
static ScFn *fn_new(const char *qual, int (*run)(ScFn *, Scene *, PyRandom *, int), void *ctx, void (*ctx_free)(void *)) {
    ScFn *f = (ScFn *)calloc(1, sizeof *f); f->rc = 1; f->qualname = qual; f->run = run; f->ctx = ctx; f->ctx_free = ctx_free; return f;
}
static int run_comp(ScFn *f, Scene *s, PyRandom *rng, int lvl) {
    FtComp *c = (FtComp *)f->ctx; int r; (void)lvl;
    r = ft_comp_run(c, s, rng, 1.0, bs_s3d ? bs_s3d : &S3D_OWN);
    if (!bs_s3d) ft_s3d_free(&S3D_OWN);
    return r < 0 ? SC_ERR : 0;
}
/* _orepile(rock, gem).fn(s, rng, lvl=0) */
typedef struct { const char *rock, *gem; } Ore;
typedef struct { Scene *s; PyRandom *rng; } OreSa;
static Geo *ore_one(void *c, double x, double y) { OreSa *m = (OreSa *)c; double r = py_uniform(m->rng, 0.6, 1.0); return g_stone(x, y, r, m->rng, .7, 5); }
static void ore_sa(void *c, double cx, double cy, GeoList *out) { OreSa *m = (OreSa *)c; bs_cluster(m->s, m->rng, 7, ore_one, m, cx, cy, 1.6, 0.15, out); }
static int run_ore(ScFn *f, Scene *s, PyRandom *rng, int lvl) {
    Ore *o = (Ore *)f->ctx; BsFrame F; OreSa m = { s, rng }; Num z; GeoList *ps; int i; static const Tier RT[2] = { TN(0.7, 6), TN(0.3, 40) };
    (void)lvl;
    bs_begin(&F);
    if (setjmp(F.jb)) { bs_end(&F); return SC_ERR; }
    ps = bs_place_group(s, rng, ore_sa, &m, 120, 0.4, 0, NULL, &z);
    if (ps && ps->n) for (i = 0; i < ps->n; i++) bs_mound(s, L1(ps->v[i]), z, num_f(0.7), RT, 2, (i % 2 && o->gem && *o->gem) ? o->gem : o->rock);
    bs_end(&F);
    return 0;
}
/* ---- details.py / styles2.py details: made by bld-features' constructors (bld_details.h), fetched from the table */
typedef DtFn *(*DtScatterMk)(const LfShape *, double, double, double, double, const char *, const char *, double, const Tier *, int);
typedef DtFn *(*DtSingleMk)(const LfShape *, double, double, const char *, const char *, const LfPat *, const Tier *, int);
typedef DtFn *(*DtSunkMk)(const LfShape *, double, double, const char *, double, double, double);
typedef DtFn *(*DtTrailMk)(double, double, double, const LfShape *, Num, const char *);
typedef DtFn *(*DtPathMk)(double, double, double, Num, const char *, const char *, double);
typedef DtFn *(*DtPostMk)(const char *, double, double, int, double, double, double);
typedef DtFn *(*DtColMk)(const char *);
typedef DtFn *(*DtVentMk)(const char *, const char *);
typedef int (*DtRunFn)(const DtFn *, Scene *, PyRandom *, double);
typedef int (*TierRef)(Scene *, PyRandom *, int);
static const FtArg *barg(const BsCall *c, int pos, const char *kw) {
    int i, np = 0;
    for (i = 0; i < c->nargs; i++) if (c->a[i].kw && kw && !strcmp(c->a[i].kw, kw)) return &c->a[i];
    for (i = 0; i < c->nargs; i++) if (!c->a[i].kw) { if (np == pos) return &c->a[i]; np++; }
    return NULL;
}
static void bt2(const BsCall *c, int pos, const char *kw, double d0, double d1, double *a, double *b) { const FtArg *x = barg(c, pos, kw); if (x && x->n >= 2) { *a = x->v[0]; *b = x->v[1]; } else { *a = d0; *b = d1; } }
static double bf1(const BsCall *c, int pos, const char *kw, double d) { const FtArg *x = barg(c, pos, kw); return x && x->n ? x->v[0] : d; }
static const char *bs1(const BsCall *c, int pos, const char *kw, const char *d) { const FtArg *x = barg(c, pos, kw); return x ? (x->t == 's' ? x->s : NULL) : d; }
static int btiers(const BsCall *c, int pos, const char *kw, const double *dflt, Tier *t) {
    const FtArg *x = barg(c, pos, kw); int i, n;
    if (!x) { for (i = 0; i < 2; i++) { t[i].frac = num_f(dflt[2 * i]); t[i].a = num_i(dflt[2 * i + 1]); } return 2; }
    n = x->n / 2;
    for (i = 0; i < n && i < 4; i++) {
        t[i].frac = (x->imask >> (2 * i)) & 1 ? num_i(x->v[2 * i]) : num_f(x->v[2 * i]);
        t[i].a = (x->imask >> (2 * i + 1)) & 1 ? num_i(x->v[2 * i + 1]) : num_f(x->v[2 * i + 1]);
    }
    return n;
}
/* a details sh_* argument ('S') as leaf.h's LfShape, the shape's own defaults filled in */
static int bshape(const FtArg *x, LfShape *sh) {
    int k; double v[5] = { 0 };
    if (!x || x->t != 'S') return 0;
    for (k = 0; k < SH_COUNT; k++) if (!strcmp(lf_shape_tag(k), x->s)) break;
    if (k == SH_COUNT) ly_raise();
    for (int i = 0; i < x->n && i < 5; i++) v[i] = x->v[i];
    if (k == SH_ROCK && x->n < 3) { v[2] = 5; v[3] = 8; }          /* sh_rock(r0, r1, nv=(5, 8)) */
    if (k == SH_DISC && x->n < 3) v[2] = 20;                       /* sh_disc(r0, r1, res=20) */
    if (k == SH_WIG && x->n < 4) v[3] = 0.5;                       /* sh_wig(l0, l1, w, amp=0.5) */
    *sh = lf_sh(k, v[0], v[1], v[2], v[3], v[4]);
    return 1;
}
static int bpat(const FtArg *x, LfPat *pt) {
    static const char *const NM[PAT_COUNT] = { "slots", "seams", "ring", "cross", "dots", "hole", "rune" };
    static const double D0[PAT_COUNT][2] = { { 4, 0.35 }, { 0.9, 0 }, { 0.3, 0 }, { 0, 0 }, { 0.25, 0 }, { 0.5, 0.8 }, { 0, 0 } };
    int k;
    if (!x || x->t != 'P') return 0;
    for (k = 0; k < PAT_COUNT; k++) if (!strcmp(NM[k], x->s)) break;
    if (k == PAT_COUNT) ly_raise();
    pt->kind = k; pt->a[0] = x->n > 0 ? x->v[0] : D0[k][0]; pt->a[1] = x->n > 1 ? x->v[1] : D0[k][1];
    return 1;
}
static int run_dt(ScFn *f, Scene *s, PyRandom *rng, int lvl) {
    DtRunFn run = (DtRunFn)bld_any("details", "dt_run", "dt_run");
    if (!run) return SC_ERR;
    return run((const DtFn *)f->ctx, s, rng, (double)lvl) < 0 ? SC_ERR : 0;
}
static int run_ref(ScFn *f, Scene *s, PyRandom *rng, int lvl) { return ((TierRef)f->ctx)(s, rng, lvl) < 0 ? SC_ERR : 0; }
static void nofree(void *p) { (void)p; }
/* *out NULL when the maker is not in the table (not ported yet) */
static void make_call(const BsCall *c, const char *ref, ScFn **out) {
    LfShape sh; LfPat pt; Tier t[4]; int nt; double a, b, d, e; DtFn *f = NULL; char q[96];
    static const double ROCKY[4] = { 0.7, 6, 0.3, 40 }, SINGLE_T[4] = { 0.75, 5, 0.25, 40 };
    *out = NULL;
    if (ref) {          /* styles2.f_headstone ...: the function itself (_tag_all tagged it: TAGS) */
        char nm[64]; TierRef fn; SmTag tg;
        snprintf(nm, sizeof nm, "f_%s", ref); fn = (TierRef)bld_any("styles2", nm, "tier");
        if (!fn) return;
        *out = fn_new(I_(nm), run_ref, (void *)fn, nofree);
        if (sm_tag_lookup(nm, &tg)) (*out)->tag = tg;
        return;
    }
    if (!strcmp(c->mod, "details") && !strcmp(c->name, "scatter")) {
        DtScatterMk mk = (DtScatterMk)bld_any("details", "scatter", "dt_new"); int hs = bshape(barg(c, 0, "shape"), &sh);
        bt2(c, 1, "n", 3, 6, &a, &b); bt2(c, 2, "h", 0.5, 0.9, &d, &e); nt = btiers(c, 6, "tiers", ROCKY, t);
        if (mk) f = mk(hs ? &sh : NULL, a, b, d, e, I_(bs1(c, 3, "kind", "rock")), I_(bs1(c, 4, "how", "mound")), bf1(c, 5, "spread", 1.6), t, nt);
    } else if (!strcmp(c->mod, "details") && !strcmp(c->name, "single")) {
        DtSingleMk mk = (DtSingleMk)bld_any("details", "single", "dt_new"); int hs = bshape(barg(c, 0, "shape"), &sh), hp = bpat(barg(c, 4, "top"), &pt);
        bt2(c, 1, "h", 0.8, 1.4, &a, &b); nt = btiers(c, 5, "tiers", SINGLE_T, t);
        if (mk) f = mk(hs ? &sh : NULL, a, b, I_(bs1(c, 2, "kind", "crate")), I_(bs1(c, 3, "how", "up")), hp ? &pt : NULL, t, nt);
    } else if (!strcmp(c->mod, "details") && !strcmp(c->name, "sunk")) {
        DtSunkMk mk = (DtSunkMk)bld_any("details", "sunk", "dt_new"); int hs = bshape(barg(c, 0, "shape"), &sh);
        bt2(c, 1, "depth", 0.3, 0.6, &a, &b); bt2(c, 3, "n", 1, 1, &d, &e);
        if (mk) f = mk(hs ? &sh : NULL, a, b, I_(bs1(c, 2, "kind", "pit")), d, e, bf1(c, 4, "spread", 0.0));
    } else if (!strcmp(c->mod, "details") && !strcmp(c->name, "trail")) {
        DtTrailMk mk = (DtTrailMk)bld_any("details", "trail", "dt_new"); int hs = bshape(barg(c, 2, "shape"), &sh); const FtArg *dp = barg(c, 3, "depth");
        Num depth = dp ? ((dp->imask & 1) ? num_i(dp->v[0]) : num_f(dp->v[0])) : num_f(0.35);
        bt2(c, 1, "n", 5, 8, &a, &b);
        if (mk) f = mk(bf1(c, 0, "step", 1.3), a, b, hs ? &sh : NULL, depth, I_(bs1(c, 4, "kind", "pit")));
    } else if (!strcmp(c->mod, "details") && !strcmp(c->name, "path")) {
        DtPathMk mk = (DtPathMk)bld_any("details", "path", "dt_new"); const FtArg *hx = barg(c, 2, "h");
        Num h = hx ? ((hx->imask & 1) ? num_i(hx->v[0]) : num_f(hx->v[0])) : num_f(0.4);
        bt2(c, 1, "l", 6, 10, &a, &b);
        if (mk) f = mk(bf1(c, 0, "w", 0.7), a, b, h, I_(bs1(c, 3, "kind", "cable")), I_(bs1(c, 4, "how", "up")), bf1(c, 5, "amp", 0.6));
    } else if (!strcmp(c->mod, "styles2") && !strcmp(c->name, "f_post_row")) {
        DtPostMk mk = (DtPostMk)bld_any("styles2", "f_post_row", "dt_new"); const FtArg *rl = barg(c, 2, "rail");
        bt2(c, 1, "h", 1.8, 2.4, &a, &b); bt2(c, 3, "n", 2, 3, &d, &e);
        if (mk) f = mk(I_(bs1(c, 0, "kind", NULL)), a, b, rl ? rl->v[0] != 0 : 1, d, e, bf1(c, 4, "gap", 4.2));
    } else if (!strcmp(c->mod, "styles2") && !strcmp(c->name, "f_column")) {
        DtColMk mk = (DtColMk)bld_any("styles2", "f_column", "dt_new");
        if (mk) f = mk(I_(bs1(c, 0, "kind", "tcolumn")));
    } else if (!strcmp(c->mod, "styles2") && !strcmp(c->name, "f_vent")) {
        DtVentMk mk = (DtVentMk)bld_any("styles2", "f_vent", "dt_new");
        if (mk) f = mk(I_(bs1(c, 0, "kind_cut", NULL)), I_(bs1(c, 1, "kind_rim", NULL)));
    } else ly_raise();
    if (!f) return;
    snprintf(q, sizeof q, "%s.<locals>.fn", c->name);
    *out = fn_new(I_(q), run_dt, f, nofree);          /* the DtFn is the table's (bld_free_all) */
    (*out)->tag.tmpl = f->tmpl; (*out)->tag.kind = f->kind; (*out)->tag.shape = f->shape; (*out)->tag.cls = NULL;
}

int bs_prop_fn(const char *name, const ScPal *pal, ScFn **out) {
    Prop P; BsFrame F; int r;
    *out = NULL;
    bs_begin(&F);
    if (setjmp(F.jb)) { bs_end(&F); *out = NULL; return 0; }          /* try: return mk(pal) except Exception: return None */
    r = prop_recipe(name, pal, &P);
    if (!r || P.t == P_NONE) { bs_end(&F); return 0; }
    if (P.t == P_COMP) {
        BsBuild b = spec_make(&P.b[P.top]); FtComp *c = (FtComp *)calloc(1, sizeof *c);
        *c = ft_comp(b.fn, b.ctx, 10.5, 0.4, 0, 60); c->kind = b.kind ? b.kind : ""; c->shape = b.tag ? b.tag : "";
        *out = fn_new("comp.<locals>.fn", run_comp, c, free);
        (*out)->tag.tmpl = "comp"; (*out)->tag.kind = c->kind; (*out)->tag.shape = c->shape; (*out)->tag.cls = NULL;
    } else if (P.t == P_ORE) {
        Ore *o = (Ore *)calloc(1, sizeof *o); o->rock = I_(P.rock); o->gem = I_(P.gem);
        *out = fn_new("_orepile.<locals>.fn", run_ore, o, free);
        (*out)->tag.tmpl = "scatter"; (*out)->tag.kind = o->rock; (*out)->tag.shape = "rock"; (*out)->tag.cls = NULL;
    } else make_call(&P.call, P.t == P_REF ? P.ref : NULL, out);
    bs_end(&F);
    return 0;
}

/* ---- describing it */
typedef struct { char *b; int n, cap; } Sb;
static void sp(Sb *s, const char *fmt, ...) __attribute__((format(printf, 2, 3)));
static void sp(Sb *s, const char *fmt, ...) {
    va_list ap; int k; char tmp[512];
    va_start(ap, fmt); k = vsnprintf(tmp, sizeof tmp, fmt, ap); va_end(ap);
    if (k < 0) return;
    if (s->b && s->n + k < s->cap) memcpy(s->b + s->n, tmp, (size_t)k + 1);
    s->n += k;
}
static void num_(Sb *s, double v, int isint) { if (isint) sp(s, "i:%lld", (long long)v); else sp(s, "f:%.17g", v); }
static void arg_(Sb *s, const FtArg *a) {
    int j;
    if (a->kw) sp(s, "%s=", a->kw);
    switch (a->t) {
    case 's': sp(s, "s:%s", a->s); break;
    case 'N': sp(s, "~"); break;
    case 'f': case 'i': num_(s, a->v[0], a->t == 'i' || (a->imask & 1)); break;
    case 'b': sp(s, "b:%d", (int)a->v[0]); break;
    case 'T': sp(s, "("); for (j = 0; j < a->n; j++) { if (j) sp(s, ","); num_(s, a->v[j], (a->imask >> j) & 1); } sp(s, ")"); break;
    case 'R': sp(s, "("); for (j = 0; j < a->n / 2; j++) { if (j) sp(s, ","); sp(s, "("); num_(s, a->v[2 * j], (a->imask >> (2 * j)) & 1); sp(s, ","); num_(s, a->v[2 * j + 1], (a->imask >> (2 * j + 1)) & 1); sp(s, ")"); } sp(s, ")"); break;
    case 'S': case 'P': sp(s, "%c:%s(", a->t, a->s); for (j = 0; j < a->n; j++) { if (j) sp(s, ","); num_(s, a->v[j], (a->imask >> j) & 1); } sp(s, ")"); break;
    default: sp(s, "?");
    }
}
static void args_(Sb *s, const char *mod, const char *name, int n, const FtArg *a) { int i; sp(s, "%s.%s(", mod, name); for (i = 0; i < n; i++) { if (i) sp(s, ","); arg_(s, &a[i]); } sp(s, ")"); }
static void call_(Sb *s, const FtCall *c) { args_(s, c->mod, c->name, c->nargs, c->a); }
static void bcall_(Sb *s, const BsCall *c) { args_(s, c->mod, c->name, c->nargs, c->a); }
static void spec_(Sb *s, const BSpec *b) {
    switch (b->t) {
    case S_FEAT: case S_SCENES: call_(s, &b->call); break;
    case S_EITHER: sp(s, "scenes._either("); spec_(s, b->a); sp(s, ","); spec_(s, b->b); sp(s, ","); num_(s, b->share, 0); sp(s, ")"); break;
    case S_SEVERAL: sp(s, "scenes._several("); spec_(s, b->a); sp(s, ",(i:%d,i:%d),", b->n0, b->n1); num_(s, b->spread, 0); sp(s, ")"); break;
    case S_CARCASS: sp(s, "carcass("); arg_(s, &b->call.a[0]); sp(s, ")"); break;
    }
}
static const char *T_(const char *x) { return x ? x : "~"; }
int bs_prop_desc(const char *name, const ScPal *pal, char *out, int cap) {
    Prop P; Sb s = { out, 0, cap }; BsFrame F; ScFn *f = NULL;
    if (out && cap) out[0] = 0;
    bs_begin(&F);
    if (setjmp(F.jb)) { bs_end(&F); sp(&s, "raised"); return s.n; }
    if (!prop_recipe(name, pal, &P) || P.t == P_NONE) { bs_end(&F); sp(&s, "None"); return s.n; }
    if (P.t == P_COMP) { sp(&s, "comp("); spec_(&s, &P.b[P.top]); sp(&s, ")"); }
    else if (P.t == P_ORE) { sp(&s, "scenes._orepile(s:%s,", P.rock); if (P.gem) sp(&s, "s:%s)", P.gem); else sp(&s, "~)"); }
    else if (P.t == P_REF) sp(&s, "styles2.f_%s", P.ref);
    else bcall_(&s, &P.call);
    bs_end(&F);
    bs_prop_fn(name, pal, &f);          /* the tags of what prop_fn really makes */
    if (!f) sp(&s, "|?");
    else { sp(&s, "|%s|%s|%s|%s", T_(f->tag.tmpl), T_(f->tag.kind), T_(f->tag.shape), T_(f->tag.cls)); if (f->ctx_free) f->ctx_free(f->ctx); free(f); }
    return s.n;
}

void bld_register_scenes(void) {
    int i;
    for (i = 0; i < NBDEFS; i++) {
        bld_add("scenes", BDEFS[i].name, bld_fac);
        if (BDEFS[i].name[0] == 'b') bld_add("scenes", BDEFS[i].name + 2, bld_fac);
    }
    bld_add("scenes", "skeleton", bld_fac);
    bld_add_any("scenes", "SUBJECTS", "CpSubjectFn", (void *)bs_subject);
    bld_add_any("scenes", "prop_fn", "CpPropFn", (void *)bs_prop_fn);
}
