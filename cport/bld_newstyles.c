/* bld_newstyles.c: group G10 of the C port, the newer styles (style_sundered / ashveil / ferrum / thornwild /
   obelis / blight .py and their sd_r_x_<style>.py details): the shared helpers of bld_ns.h, sd_r_x_frontier.x_cutfix
   (as these styles' x_d uses it), the registration (bld_register_newstyles) and the FtCall argument helpers.
   Each style's builders are in bld_ns_<style>.c, its tables in bld_ns_tables.c.
   Proof: work\cscene\bld_newstyles (nscap.py records real calls in real bases and makes random cases; nstest.dll
   replays them; the dumps are compared byte for byte). */
#include "bld_ns.h"

/* ================================================================== the arena */
typedef struct { void *p; void (*f)(void *); } NsItem;
static NsItem *AR; static int nar, car;
NsFrame *ns_top;

static void gfree(void *p) { shp_free((G *)p); }
void ns_keep(void *p, void (*fr)(void *)) {
    if (!p) return;
    if (nar == car) { car = car ? 2 * car : 1024; AR = (NsItem *)realloc(AR, sizeof(NsItem) * (size_t)car); }
    AR[nar].p = p; AR[nar].f = fr; nar++;
}
void ns_enter(NsFrame *f) { f->mark = nar; f->up = ns_top; ns_top = f; }
void ns_leave(NsFrame *f) {
    while (nar > f->mark) { nar--; AR[nar].f(AR[nar].p); }
    ns_top = f->up;
}
_Noreturn void ns_raise(void) { longjmp(ns_top->jb, 1); }
G *T(G *g) { if (!g) ns_raise(); ns_keep(g, gfree); return g; }
G *TN(G *g) { if (g) ns_keep(g, gfree); return g; }

static void glfree(void *p) { GL *l = (GL *)p; free(l->v); free(l); }
GL *gl_new(void) { GL *l = (GL *)calloc(1, sizeof *l); ns_keep(l, glfree); return l; }
void gl_add(GL *l, G *g) { if (l->n == l->cap) { l->cap = l->cap ? 2 * l->cap : 16; l->v = (G **)realloc(l->v, sizeof(G *) * (size_t)l->cap); } l->v[l->n++] = g; }
static void plfree(void *p) { PTS *l = (PTS *)p; free(l->v); free(l); }
PTS *pl_new(void) { PTS *l = (PTS *)calloc(1, sizeof *l); ns_keep(l, plfree); return l; }
void pl_add(PTS *l, double x, double y) { if (l->n == l->cap) { l->cap = l->cap ? 2 * l->cap : 16; l->v = (double *)realloc(l->v, sizeof(double) * 2 * (size_t)l->cap); } l->v[2 * l->n] = x; l->v[2 * l->n + 1] = y; l->n++; }

/* ================================================================== maths */
double ns_pow(double x, double y) {
    double re, im;
    if (px_pow(x, y, &re, &im) != 0 || im != 0.0) ns_raise();          /* a complex result: the callers here would raise on it */
    return re;
}
double ns_mdist(double x0, double y0, double x1, double y1) { double p[2] = { x0, y0 }, q[2] = { x1, y1 }; return px_dist(p, q, 2); }
double ns_hypot(double x, double y) { return py_hypot(x, y); }
double ns_choice_d(PyRandom *r, const double *v, int n) { return v[py_choice(r, n)]; }

/* ================================================================== geometry */
G *P(double x, double y) { return T(shp_point(x, y)); }
G *PB(double x, double y, double r, int q) { return T(shp_point_buffer(x, y, r, q)); }
G *buf(const G *g, double d, int q) { return T(shp_buffer_q(g, d, q)); }
G *bufx(const G *g, double d, int q, int cap, int join, double mitre) { return T(shp_buffer(g, d, q, cap, join, mitre, 0)); }
G *dif(const G *a, const G *b) { return T(shp_difference(a, b)); }
G *inter(const G *a, const G *b) { return T(shp_intersection(a, b)); }
G *uni(const G *a, const G *b) { return T(shp_union(a, b)); }
G *uu(GL *l) { return T(shp_union_all((const G *const *)l->v, l->n)); }
G *U(GL *l) { return T(sd_U((const G *const *)l->v, l->n)); }
G *u_(GL *l) { return T(so_u((const G *const *)l->v, l->n)); }
G *hull(GL *l) { return T(sd_hull((const G *const *)l->v, l->n)); }
G *chull(const G *g) { return T(shp_convex_hull(g)); }
G *poly(PTS *p) { return T(shp_polygon(p->v, p->n)); }
G *polyv(const double *xy, int n) { return T(shp_polygon(xy, n)); }
G *line(PTS *p) { return T(shp_linestring(p->v, p->n)); }
G *simp(const G *g, double tol) { return T(shp_simplify(g, tol, 1)); }
G *rect(double cx, double cy, double l, double w, double a) { return T(shp_rect(cx, cy, l, w, a)); }
G *star(double cx, double cy, double ro, double ri, int n, double r_) { return T(shp_star(cx, cy, ro, ri, n, r_)); }
static G *steal(Geo *h) { G *g; if (!h) ns_raise(); g = shp_clone(h->g); geo_unref(h); return T(g); }
G *stone(double cx, double cy, double r, PyRandom *rng, double aspect, int nv) { return steal(lf_stone(cx, cy, r, rng, aspect, nv, 0)); }
G *blob(double cx, double cy, double r, PyRandom *rng, int k) { return steal(lf_blob(cx, cy, r, rng, k)); }
G *inset(const G *g, double d) { return T(shp_inset(g, d)); }
G *ring(double cx, double cy, double ro, double ri, int res) { return T(shp_ring(cx, cy, ro, ri, res)); }
G *disc(double x, double y, double r, int n) { return T(sd_disc(x, y, r, n)); }
G *ann(double x, double y, double ro, double ri, int n) { return T(sd_ann(x, y, ro, ri, n)); }
G *capl(double px, double py, double qx, double qy, double w) { double p[2] = { px, py }, q[2] = { qx, qy }; return T(sd_capl(p, q, w)); }
G *barl(double px, double py, double qx, double qy, double w) { double p[2] = { px, py }, q[2] = { qx, qy }; return T(sd_barl(p, q, w)); }
G *lineb(PTS *p, double w) { return T(sd_line(p->v, p->n, w)); }
G *ell(double x, double y, double rx, double ry, double a, int n) { return T(sd_ell(x, y, rx, ry, a, n)); }
G *ngon(double x, double y, double r, int n, double a) { return T(sd_ngon(x, y, r, n, a)); }
G *tri(double px, double py, double qx, double qy, double w) { double p[2] = { px, py }, q[2] = { qx, qy }; return T(sd_tri(p, q, w)); }
G *rrect(double x, double y, double L, double W, double a, double r) { return T(sd_rrect(x, y, L, W, a, r)); }
G *wavy(double px, double py, double qx, double qy, PyRandom *rng, double amp, int n, double w) { double p[2] = { px, py }, q[2] = { qx, qy }; return T(sd_wavy(p, q, rng, amp, n, w)); }
G *rot(const G *g, double ang, int origin, double ox, double oy, int rad) { return T(shp_rotate(g, ang, origin, ox, oy, rad)); }
G *scl(const G *g, double fx_, double fy_, int origin, double ox, double oy) { return T(shp_scale(g, fx_, fy_, origin, ox, oy)); }
G *mrr(const G *g) { return T(shp_min_rot_rect(g)); }
G *cen(const G *g) { return T(shp_centroid(g)); }
G *rpt(const G *g) { return T(shp_representative_point(g)); }
G *ext_ring(const G *g) { const G *e = shp_exterior(g); if (!e) ns_raise(); return T(shp_clone(e)); }
G *interp(const G *ln, double d) { return T(shp_interpolate(ln, d, 0)); }
G *clone(const G *g) { return T(shp_clone(g)); }
GL *polys(const G *g) {
    ShpList s = { 0 }; GL *l = gl_new(); int i;
    shp_polys_of(g, &s);
    for (i = 0; i < s.n; i++) { gl_add(l, T(s.g[i])); s.g[i] = NULL; }
    free(s.g);
    return l;
}
double area(const G *g) { return shp_area(g); }
double len_(const G *g) { return shp_length(g); }
double dist(const G *a, const G *b) { return shp_distance(a, b); }
int contains(const G *a, const G *b) { int r = shp_contains(a, b); if (r < 0) ns_raise(); return r; }
int intersects(const G *a, const G *b) { int r = shp_intersects(a, b); if (r < 0) ns_raise(); return r; }
void bounds(const G *g, double b[4]) { shp_bounds(g, b); }
double gx(const G *pt) { return shp_x(pt); }
double gy(const G *pt) { return shp_y(pt); }
int wide(const G *p, double w) { int r = shp_wide_enough(p, w, SC.ts); if (r < 0) ns_raise(); return r; }
PTS *wiggle(double ax, double ay, double bx, double by, PyRandom *rng, double amp, int n) {
    PTS *l = pl_new(); int m;
    l->v = (double *)malloc(sizeof(double) * 2 * (size_t)(n + 2)); l->cap = n + 2;
    m = lf_wiggle(ax, ay, bx, by, rng, amp, n, l->v); l->n = m;
    return l;
}
int ns_linspace(double a, double b, int n, double *out) { return np_linspace(a, b, n, 1, out) < 0 ? (ns_raise(), 0) : n; }
int ns_arange(double a, double b, double st, double *out, int cap) { int64_t n = np_arange(a, b, st, out, cap); if (n < 0) ns_raise(); return (int)n; }

/* ================================================================== solids */
static void slfree(void *p) { sol_free((SoSolids *)p); free(p); }
SoSolids *sl_new(void) { SoSolids *s = (SoSolids *)calloc(1, sizeof *s); ns_keep(s, slfree); return s; }
void sl_add(SoSolids *dst, const SoSolids *src) { sol_extend(dst, src); }
void sl_custom(SoSolids *dst, NsSectFn fn, const void *ctx, size_t size, double top) {
    void *c = malloc(size ? size : 1); Solid *s;
    memcpy(c, ctx, size);
    s = sol_custom(fn, c, free, top);
    if (!s) { free(c); ns_raise(); }
    sol_push(dst, s); sol_unref(s);
}
void cut_add(NsCuts *c, const G *q, double fl) { if (c->n >= 64) ns_raise(); c->v[c->n].q = q; c->v[c->n].fl = fl; c->n++; }

/* ================================================================== layers */
int ns_count(NsOut *o) { return o->lay->n; }
void ns_up(NsOut *o, const G *g, double h, const char *kind) { lyl_push_own(o->lay, ly_up(shp_clone(g), h, kind)); }
void ns_on(NsOut *o, const G *g, int64_t par, double h, const char *kind) { lyl_push_own(o->lay, ly_on(shp_clone(g), par, h, kind)); }
void ns_tcut(NsOut *o, const G *g, int64_t par, double d) { lyl_push_own(o->lay, ly_tcut(shp_clone(g), par, d)); }
void ns_gcut(NsOut *o, const G *g, double d, const char *kind) { lyl_push_own(o->lay, ly_gcut(shp_clone(g), d, kind)); }
void ns_pad(NsOut *o, const G *g, const char *kind) {
    SoLayer *l = ly_new(3); l->v[0] = lv_str("pad"); l->v[1] = lv_geom(shp_clone(g)); l->v[2] = lv_str(kind); lyl_push_own(o->lay, l);
}
void ns_mound(NsOut *o, const G *g, double h, const char *kind, const FtTiers *t) {
    SoLayer *l = ly_new(5); l->v[0] = lv_str("mound"); l->v[1] = lv_geom(shp_clone(g)); l->v[2] = lv_float(h); l->v[3] = lv_str(kind);
    l->v[4].t = LV_PTR; l->v[4].p = (void *)t; lyl_push_own(o->lay, l);
}
void ns_st(NsOut *o, const SoSolids *s, const char *kind, int has_par, int64_t par, const NsCuts *c) {
    SoLayer *l = NULL;
    if (sd_ST(s, kind, has_par, par, c ? c->v : NULL, c ? c->n : 0, &l) != SO_OK) ns_raise();
    lyl_push_own(o->lay, l);
}
static void objfree(void *p) { so_obj_free((SoObj *)p); free(p); }
SoObj *obj_new(const char *kind) { SoObj *o = (SoObj *)calloc(1, sizeof *o); so_obj_init(o, kind); ns_keep(o, objfree); return o; }
void ns_obj_out(NsOut *o, SoObj *ob, GL *extra, G **foot) {
    G *h = NULL; int i;
    if (so_obj_out(ob, extra ? (const G *const *)extra->v : NULL, extra ? extra->n : 0, &h) != SO_OK) ns_raise();
    *foot = T(h);
    for (i = 0; i < ob->L.n; i++) lyl_push(o->lay, ob->L.v[i]);
}
int ns_done(Geo **foot_out, const G *foot) { *foot_out = foot ? geo_new(shp_clone(foot)) : NULL; return 0; }

const FtTiers NS_SOFT_T = { 2, { { { 0.55, NUM_FLOAT, 0 }, { 25, NUM_INT, 0 } }, { { 0.45, NUM_FLOAT, 0 }, { 50, NUM_INT, 0 } } } };
const FtTiers NS_ROCK_T = { 2, { { { 0.7, NUM_FLOAT, 0 }, { 6, NUM_INT, 0 } }, { { 0.3, NUM_FLOAT, 0 }, { 40, NUM_INT, 0 } } } };
const FtTiers NS_PEB_T = { 2, { { { 0.7, NUM_FLOAT, 0 }, { 10, NUM_INT, 0 } }, { { 0.3, NUM_FLOAT, 0 }, { 45, NUM_INT, 0 } } } };
const FtTiers NS_SPIKE_T = { 2, { { { 0.75, NUM_FLOAT, 0 }, { 4, NUM_INT, 0 } }, { { 0.25, NUM_FLOAT, 0 }, { 30, NUM_INT, 0 } } } };
const FtTiers NS_ROUND_T = { 3, { { { 0.45, NUM_FLOAT, 0 }, { 12, NUM_INT, 0 } }, { { 0.3, NUM_FLOAT, 0 }, { 38, NUM_INT, 0 } }, { { 0.25, NUM_FLOAT, 0 }, { 62, NUM_INT, 0 } } } };

/* ================================================================== sd_r_x_frontier.x_cutfix */
/* x_cutfix(out, style, name, cls): a cut into a top is dropped by the kit when no part of it is 0.2 mm wide as
   laid; every line cut too thin is widened just enough to stay, at the size this detail is laid. lay is changed in
   place (a new tuple for each tcut). SC_ERR if a shapely call raised. */
int ns_cutfix(Geo *foot, SoLayers *lay, const char *style, const char *name, const char *cls) {
    double k, wmin; int i, j, t;
    if (!foot) return 0;
    k = sdr_real_factor(style, name, cls); if (k == 0.0) k = 1.0;          /* real_factor(...) or 1.0 */
    wmin = 0.205 / k;
    for (i = 0; i < lay->n; i++) {
        SoLayer *L = lay->v[i], *N; ShpList ps = { 0 }, qs = { 0 };
        if (!L || L->v[0].t != LV_STR || strcmp(L->v[0].s, "tcut") || L->n < 2 || L->v[1].t != LV_GEOM || !L->v[1].g) continue;
        t = shp_is_empty(L->v[1].g); if (t < 0) return SC_ERR;
        if (t) continue;
        shp_polys_of(L->v[1].g, &qs);
        for (j = 0; j < qs.n; j++) {
            G *q = qs.g[j]; int it;
            qs.g[j] = NULL;
            for (it = 0; it < 8; it++) {
                G *b = shp_buffer_q(q, -wmin / 2, 16), *q2; int e;
                if (!b) { shp_free(q); shp_list_free(&qs); shp_list_free(&ps); return SC_ERR; }
                e = shp_is_empty(b); shp_free(b);
                if (e < 0) { shp_free(q); shp_list_free(&qs); shp_list_free(&ps); return SC_ERR; }
                if (!e) break;
                q2 = shp_buffer_q(q, 0.015, 3); shp_free(q);
                if (!q2) { shp_list_free(&qs); shp_list_free(&ps); return SC_ERR; }
                q = q2;
            }
            shp_list_push(&ps, q);
        }
        shp_list_free(&qs);
        N = ly_new(L->n); N->v[0] = lv_str("tcut");
        N->v[1] = lv_geom(sd_U((const G *const *)ps.g, ps.n));
        shp_list_free(&ps);
        if (!N->v[1].g) { ly_unref(N); return SC_ERR; }
        for (j = 2; j < L->n; j++) N->v[j] = L->v[j];          /* the parent and the depth: plain numbers */
        lay->v[i] = N; ly_unref(L);
    }
    return 0;
}

/* ================================================================== sd_reg.d / x_d */
static const NsSd **SDS; static int nsds, csds;
int ns_sd_call(void *ctx, double cx, double cy, double a, PyRandom *rng, int has_scale, double scale, Geo **foot, SoLayers *layers) {
    const NsSd *d = (const NsSd *)ctx; SoLayers lay = { 0 }; NsOut o = { &lay }; G *f = NULL; int r, i;
    *foot = NULL;
    if (has_scale) return SC_ERR;          /* f takes four arguments: TypeError */
    {
        NsFrame fr_; ns_enter(&fr_);
        if (setjmp(fr_.jb)) { ns_leave(&fr_); lyl_free(&lay); return SC_ERR; }
        r = d->body(cx, cy, a, rng, &o, &f);
        if (r == 0) *foot = f ? geo_new(shp_clone(f)) : NULL;
        ns_leave(&fr_);
    }
    if (r == 1) { lyl_free(&lay); return 1; }
    if (d->cutfix && ns_cutfix(*foot, &lay, d->style, d->name, d->cls) < 0) { geo_unref(*foot); *foot = NULL; lyl_free(&lay); return SC_ERR; }
    for (i = 0; i < lay.n; i++) lyl_push(layers, lay.v[i]);
    lyl_free(&lay);
    return 0;
}
void ns_sd_reg(const NsSd *d, double r, int ground) {
    SdrEntry e; int i;
    e.f0 = ns_sd_call; e.ctx = (void *)d; e.style = d->style; e.cls = d->cls; e.kind = d->kind; e.name = d->name; e.r = r; e.ground = ground;
    sdr_register(&e);
    for (i = 0; i < nsds; i++) if (!strcmp(SDS[i]->style, d->style) && !strcmp(SDS[i]->name, d->name)) { SDS[i] = d; return; }
    if (nsds == csds) { csds = csds ? 2 * csds : 256; SDS = (const NsSd **)realloc(SDS, sizeof(*SDS) * (size_t)csds); }
    SDS[nsds++] = d;
}
const NsSd *ns_sd_find(const char *style, const char *name) {
    int i; for (i = 0; i < nsds; i++) if (!strcmp(SDS[i]->style, style) && !strcmp(SDS[i]->name, name)) return SDS[i];
    return NULL;
}

/* ================================================================== FtCall arguments */
const FtArg *ns_arg(const FtCall *c, int j, const char *kw) {
    int i, k = 0;
    if (kw) for (i = 0; i < c->nargs; i++) if (c->a[i].kw && !strcmp(c->a[i].kw, kw)) return &c->a[i];
    for (i = 0; i < c->nargs; i++) { if (c->a[i].kw) continue; if (k == j) return &c->a[i]; k++; }
    return NULL;
}
const char *ns_arg_s(const FtCall *c, int j, const char *kw, const char *dflt) { const FtArg *a = ns_arg(c, j, kw); return a && a->t == 's' ? a->s : dflt; }
int ns_arg_pair(const FtCall *c, int j, const char *kw, double d0, double d1, double *x, double *y) {
    const FtArg *a = ns_arg(c, j, kw);
    if (!a) { *x = d0; *y = d1; return 0; }
    if (a->t != 'T' || a->n != 2) return SC_ERR;
    *x = a->v[0]; *y = a->v[1]; return 0;
}

/* ================================================================== registration */
/* sd_reg.REG is filled in style_details._parts() order: sd_a .. sd_e, sd_r, then sorted(glob("sd_r_*.py")); of the
   newer styles' files that is ashveil, blight, ferrum, obelis, sundered, thornwild (each registers only its own style,
   so their place among the other sd_r_x files does not matter). The style files' own builders go into builders.c. */
void bld_register_newstyles(void) {
    ns_reg_ashveil(); ns_reg_blight(); ns_reg_ferrum(); ns_reg_obelis(); ns_reg_sundered(); ns_reg_thornwild();
    ns_bld_blight(); ns_bld_ferrum(); ns_bld_obelis(); ns_bld_tiers();
}

/* ================================================================== more solids, Obj methods, small helpers */
void sl_custom_f(SoSolids *dst, NsSectFn fn, const void *ctx, size_t size, double top, void (*fr)(void *)) {
    void *c = malloc(size ? size : 1); Solid *s;
    memcpy(c, ctx, size);
    s = sol_custom(fn, c, fr, top);
    if (!s) { fr(c); ns_raise(); }
    sol_push(dst, s); sol_unref(s);
}
#define OK_(e) do { if ((e) != SO_OK) ns_raise(); } while (0)
void S_ext(SoSolids *sol, const G *g, double h, double lift) { OK_(sd_ext(sol, g, h, lift)); }
void S_box(SoSolids *sol, const G *poly, double h, double lift) { OK_(so_box(sol, poly, h, lift)); }
void S_lie(SoSolids *sol, double x, double y, double a, double L, double r, double lift) { OK_(sd_lie(sol, x, y, a, L, r, lift)); }
void S_cyl(SoSolids *sol, double x, double y, double r, double h, double lift, int n) { OK_(sd_cyl(sol, x, y, r, h, lift, n)); }
void S_dome(SoSolids *sol, double x, double y, double r, double h, double lift, double sq, double a, int n) { OK_(sd_dome(sol, x, y, r, h, lift, sq, a, n)); }
void S_cone(SoSolids *sol, double x, double y, double r, double h, double tip, double lift, int n, double sq, double a) { OK_(sd_cone(sol, x, y, r, h, tip, lift, n, sq, a)); }
void S_turn(SoSolids *sol, double x, double y, const double *prof, int np, int n, double sq, double a, double lift) { OK_(sd_turn(sol, x, y, prof, np, n, sq, a, lift)); }
void S_rod(SoSolids *sol, PTS *p, double r, double hk, double lift, double arch) { OK_(sd_rod(sol, p->v, p->n, r, NULL, NULL, hk, lift, arch)); }
void S_rodf(SoSolids *sol, PTS *p, SoNumFn f, void *ctx, double hk, double lift, double arch) { OK_(sd_rod(sol, p->v, p->n, 0.0, f, ctx, hk, lift, arch)); }
void S_spin(SoSolids *sol, double x, double y, double a, double L, SoNumFn f, void *ctx, double hk, double lift, const char *prof) { OK_(sd_spin(sol, x, y, a, L, f, ctx, hk, lift, prof)); }
void S_ridge(SoSolids *sol, double x, double y, double a, double L, double W, double h, double lift) { OK_(sd_ridge(sol, x, y, a, L, W, h, lift)); }
void S_loaf(SoSolids *sol, double x, double y, double a, double L, double W, double h, double lift) { OK_(sd_loaf(sol, x, y, a, L, W, h, lift)); }
void S_rock(SoSolids *sol, double x, double y, double r, double h, PyRandom *rng, double sq, double lift, int n) { OK_(sd_rock(sol, x, y, r, h, rng, sq, lift, n)); }
void S_fluted(SoSolids *sol, double x, double y, double r, double h, int n, double a, double deep, int m, double twist, double lift) { OK_(sd_fluted(sol, x, y, r, h, n, a, deep, m, twist, lift)); }
void S_pot(SoSolids *sol, double x, double y, double r, double h, double neck, double lip, double lift, int n) { OK_(sd_pot(sol, x, y, r, h, neck, lip, lift, n)); }
void S_hang(SoSolids *sol, const G *poly, double z0, double z1) { OK_(sd_hang(sol, poly, z0, z1)); }
void S_vault(SoSolids *sol, double cx, double cy, double a, double L, double W, double h, const char *prof, double lift, int under) { OK_(so_vault(sol, cx, cy, a, L, W, h, prof, lift, under, 0, 0.0)); }
void S_lathe(SoSolids *sol, double cx, double cy, const double *prof, int np, int seg, double sq, double a, double lift) { OK_(so_lathe(sol, cx, cy, prof, np, seg, sq, a, lift, NULL, 0)); }
int crv(double r0, double r1, double z0, double z1, int out_, int n, double *prof) { OK_(so_curve(r0, r1, z0, z1, n, out_, prof)); return n + 1; }
SoSolids *ext1(const G *g, double h) { SoSolids *s = sl_new(); S_ext(s, g, h, 0.0); return s; }

#define PAR(p) ((p) >= 0), ((p) >= 0 ? (p) : 0)
static int okix(int r) { if (r < 0) ns_raise(); return r; }
int ob_up(SoObj *o, const G *g, double h, const char *kind) { return okix(so_obj_up(o, g, h, kind)); }
int ob_on(SoObj *o, const G *g, int64_t par, double h, const char *kind) { return okix(so_obj_on(o, g, PAR(par), h, kind)); }
void ob_cut(SoObj *o, const G *g, int64_t par, double d) { okix(so_obj_cut(o, g, PAR(par), d)); }
int ob_st(SoObj *o, const SoSolids *s, const char *kind, int64_t par, const NsCuts *c, const G *foot) { return okix(so_obj_st(o, s, kind, PAR(par), c ? c->v : NULL, c ? c->n : 0, foot)); }
void ob_gcut(SoObj *o, const G *g, double d, const char *kind) { okix(so_obj_gcut(o, g, d, kind)); }
int ob_lid(SoObj *o, const G *g, int64_t par, double h, double edge, const char *kind) { return okix(so_obj_lid(o, g, PAR(par), h, edge, kind)); }
void ob_groove(SoObj *o, const G *g, int64_t par, double w, double d) { okix(so_obj_groove(o, g, PAR(par), w, d)); }
void ob_seams(SoObj *o, const FF *f, double L, double W, int64_t par, double step, int across, double w, double d, const G *skip) { okix(so_obj_seams(o, f, L, W, PAR(par), step, across, w, d, skip)); }
int ob_studs(SoObj *o, PTS *p, int64_t par, double r, double h, const char *kind, int n) { return okix(so_obj_studs(o, p->v, p->n, PAR(par), r, h, kind, n)); }
int ob_handle(SoObj *o, double px, double py, double qx, double qy, int64_t par, double w, double h, const char *kind) { double p[2] = { px, py }, q[2] = { qx, qy }; return okix(so_obj_handle(o, p, q, PAR(par), w, h, kind)); }
int ob_tabs(SoObj *o, const FF *f, const double *xs, int nx, double v, int64_t par, double a, double l, double w, double h, const char *kind) { return okix(so_obj_tabs(o, f, xs, nx, v, PAR(par), a, l, w, h, kind)); }
int ob_ribs(SoObj *o, const FF *f, double L, double W, int64_t par, int n, double a, double w, double h, int across, const char *kind) { return okix(so_obj_ribs(o, f, L, W, PAR(par), n, a, w, h, across, kind)); }
void ob_vents(SoObj *o, const FF *f, double x, double v, int64_t par, int n, double l, double a, double step, double w, double d) { okix(so_obj_vents(o, f, x, v, PAR(par), n, l, a, step, w, d)); }
int ob_corners(SoObj *o, const FF *f, double L, double W, int64_t par, double a, double size, double h, const char *kind) { return okix(so_obj_corners(o, f, L, W, PAR(par), a, size, h, kind)); }
int ob_straps(SoObj *o, const FF *f, const double *xs, int nx, double W, int64_t par, double a, double w, double h, const char *kind) { return okix(so_obj_straps(o, f, xs, nx, W, PAR(par), a, w, h, kind)); }
static void ob_addf(SoObj *o, const G *g) {
    if (o->F.n == o->F.cap) { o->F.cap = o->F.cap ? 2 * o->F.cap : 16; o->F.v = (GEOSGeometry **)realloc(o->F.v, sizeof(GEOSGeometry *) * (size_t)o->F.cap); }
    o->F.v[o->F.n++] = g ? shp_clone(g) : NULL;
}
void ob_raw_mound(SoObj *o, const G *g, double h, const char *kind, const FtTiers *t) {
    SoLayer *l = ly_new(5); l->v[0] = lv_str("mound"); l->v[1] = lv_geom(shp_clone(g)); l->v[2] = lv_float(h); l->v[3] = lv_str(kind);
    l->v[4].t = LV_PTR; l->v[4].p = (void *)t; lyl_push_own(&o->L, l); ob_addf(o, g);
}
void ob_raw(SoObj *o, SoLayer *l, const G *foot) { lyl_push_own(&o->L, l); if (foot) ob_addf(o, foot); }
G *box_(double x0, double y0, double x1, double y1) { return T(shp_box(x0, y0, x1, y1, 1)); }
double ns_round(double x) { if (!isfinite(x)) ns_raise(); return py_rint(x); }
int64_t ns_int(double x) { int64_t v; if (px_int(x, &v) != 0) ns_raise(); return v; }

/* ================================================================== the data tables (bld_ns_tables.c) */
const NsV *ns_kid(const NsV *v, int j) { return j >= 0 && j < v->n ? &NS_V[NS_KID[v->a + j]] : NULL; }
const NsV *ns_get(const NsV *d, const char *key) {
    int j; for (j = 0; j < d->n; j++) { const NsV *k = ns_kid(d, j); if (k->t == 'k' && k->s && !strcmp(k->s, key)) return ns_kid(k, 0); }
    return NULL;
}
static void bp(Buf *b, const char *s, size_t n) {
    if (b->n + n + 1 > b->cap) { b->cap = (b->n + n) * 2 + 4096; b->b = (unsigned char *)realloc(b->b, b->cap); }
    memcpy(b->b + b->n, s, n); b->n += n;
}
static void bs(Buf *b, const char *s) { bp(b, s, strlen(s)); }
static void vtext(Buf *b, const NsV *v) {
    char t[64]; int j;
    switch (v->t) {
    case 'N': bs(b, "N"); return;
    case 'A': bs(b, "A"); return;
    case 'i': case 'b': case 'E': snprintf(t, sizeof t, "%c%lld", v->t, (long long)v->v); bs(b, t); return;
    case 'f': { uint64_t u; memcpy(&u, &v->v, 8); snprintf(t, sizeof t, "f%016llx", (unsigned long long)u); bs(b, t); return; }
    case 's': snprintf(t, sizeof t, "s%d:", (int)strlen(v->s)); bs(b, t); bs(b, v->s); return;
    case 'X': bs(b, "X"); bs(b, v->s); return;
    case 'k': bs(b, v->s); bs(b, "="); break;
    case 'F': bs(b, "F<"); bs(b, v->s); bs(b, ">("); break;
    default: t[0] = v->t; t[1] = '('; t[2] = 0; bs(b, t); break;
    }
    for (j = 0; j < v->n; j++) { if (j) bs(b, ","); vtext(b, ns_kid(v, j)); }
    if (v->t != 'k') bs(b, ")");
}
int ns_tables_text(Buf *out) {
    int i; char t[96];
    out->n = 0;
    for (i = 0; i < NS_NENTRY; i++) { snprintf(t, sizeof t, "E%d %s ", i, NS_ENTRY[i].style); bs(out, t); vtext(out, &NS_V[NS_ENTRY[i].v]); bs(out, "\n"); }
    for (i = 0; i < NS_NSTYLE; i++) { snprintf(t, sizeof t, "S %s ", NS_STYLE[i].style); bs(out, t); vtext(out, &NS_V[NS_STYLE[i].v]); bs(out, "\n"); }
    bs(out, "K "); vtext(out, &NS_V[NS_KIT]); bs(out, "\n");
    return (int)out->n;
}
