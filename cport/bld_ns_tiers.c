/* bld_ns_tiers.c: the newer styles' own tier features (style_<name>.py tables(G)): fn(s, rng, lvl) on the scene,
   copied routine by routine (each names the Python it copies). See bld_ns.h for the list and how they register.
   Built on place.h (find, place_group, up, mound, cut, topcut, cluster) and scene.h (s.prism). */
#include "bld_ns.h"
#include "layouts.h"

FtS3dList *ns_s3d;

/* ---------------- glue: arena geometries <-> scene handles ---------------- */
static void geofree(void *p) { geo_unref((Geo *)p); }
static void glfree2(void *p) { GeoList *l = (GeoList *)p; gl_free(l); free(l); }
static GeoList *gls(void) { GeoList *l = (GeoList *)calloc(1, sizeof *l); ns_keep(l, glfree2); return l; }
static Geo *H(const G *g) { Geo *h = geo_new(shp_clone(g)); if (!h) ns_raise(); ns_keep(geo_ref(h), geofree); return h; }   /* a handle the arena also holds */
static GeoList *gl1(const G *g) { GeoList *l = gls(); gl_push(l, H(g)); return l; }
static int truthy(Num n) { return n.t != NUM_NONE && n.v != 0.0; }
static Kind *K(const char *s) { return kind_str(s); }
#define CK(e) do { if ((e) < 0) ns_raise(); } while (0)
static Num up_(Scene *s, GeoList *ps, Num z, double h, const char *kind) { Num top = num_none(); CK(pl_up(s, ps, z, num_f(h), K(kind), &top)); return top; }
static void topcut_(Scene *s, const G *g, Num top, double d, const char *kind) { CK(pl_topcut(s, gl1(g), top, num_f(d), K(kind))); }
static void mound_(Scene *s, GeoList *ps, Num z, double h, const FtTiers *t, const char *kind) { CK(pl_mound(s, ps, z, num_f(h), t->t, t->n, K(kind))); }
static int prism_(Scene *s, GeoList *ps, Num top, const char *kind, Num z0) { GeoList *out = gls(); CK(sc_prism(s, ps, top, K(kind), z0, SC.MINW, 0, out)); return out->n > 0; }
static Num capz(void) { return num_f(SC.CAP); }

/* find(s, mk, gap) with a make() that draws its own spot: the body runs in its own raise frame */
typedef G *(*MkBody)(void *c);
typedef struct { MkBody body; void *c; } MkCtx;
static Geo *mk_cb(void *c, int has_c, double cx, double cy, int *err) {
    MkCtx *m = (MkCtx *)c; Geo *r = NULL; G *g;
    NsFrame fr; ns_enter(&fr);
    if (setjmp(fr.jb)) { ns_leave(&fr); *err = 1; return NULL; }
    g = m->body(m->c); r = g ? geo_new(shp_clone(g)) : NULL;
    ns_leave(&fr); return r;
}
static Geo *find_(Scene *s, MkBody body, void *c, double gap, Num *z) {
    MkCtx m = { body, c }; Geo *g = NULL; int r = pl_find(s, mk_cb, &m, 120, gap, NULL, 0, NULL, NULL, NULL, &g, z);
    if (r < 0) ns_raise();
    if (g) ns_keep(g, geofree);
    return r == 1 ? g : NULL;
}
/* place_group(s, rng, shapes_at, gap): shapes_at fills a GL of arena geometries */
typedef void (*AtBody)(void *c, double cx, double cy, GL *out);
typedef struct { AtBody body; void *c; } AtCtx;
static int at_cb(void *c, double cx, double cy, GeoList *out) {
    AtCtx *m = (AtCtx *)c; GL *l; int i;
    NsFrame fr; ns_enter(&fr);
    if (setjmp(fr.jb)) { ns_leave(&fr); return SC_ERR; }
    l = gl_new(); m->body(m->c, cx, cy, l);
    for (i = 0; i < l->n; i++) gl_push_own(out, geo_new(shp_clone(l->v[i])));
    ns_leave(&fr); return 0;
}
static GeoList *group_(Scene *s, PyRandom *rng, AtBody body, void *c, double gap, Num *z) {
    AtCtx m = { body, c }; GeoList *ps = gls(); int r = pl_place_group(s, rng, at_cb, &m, 120, gap, 0, NULL, ps, z);
    if (r < 0) ns_raise();
    return r == 1 ? ps : NULL;
}
static GeoList *glsub(GeoList *l, int from, int to) { GeoList *o = gls(); int i; for (i = from; i < to && i < l->n; i++) gl_push(o, l->v[i]); return o; }
#define TIER_BEGIN NsFrame fr_; ns_enter(&fr_); if (setjmp(fr_.jb)) { ns_leave(&fr_); return SC_ERR; }
#define TIER_END(r) do { ns_leave(&fr_); return (r); } while (0)

/* ================================================================== style_sundered.py */
typedef struct { PyRandom *rng; } R1;
/* sd_blocks: at(cx, cy) -> [g for g in polys_of(unary_union(at(cx, cy)))] */
static void sdb_at(void *c, double cx, double cy, GL *out) {
    PyRandom *rng = ((R1 *)c)->rng; int i, n = (int)RI(rng, 2, 3); GL *l = gl_new(), *ps;
    for (i = 0; i < n; i++) { double x = cx + U_(rng, -2.2, 2.2), y = cy + U_(rng, -1.6, 1.6), L = U_(rng, 2.0, 2.6), W = U_(rng, 1.2, 1.5), a = U_(rng, 0, PI); gl_add(l, rect(x, y, L, W, a)); }
    ps = polys(uu(l)); for (i = 0; i < ps->n; i++) gl_add(out, ps->v[i]);
}
static int t_sd_blocks(Scene *s, PyRandom *rng, int lvl) {
    TIER_BEGIN
    R1 c = { rng }; Num z; GeoList *ps = group_(s, rng, sdb_at, &c, 0.35, &z); int i;
    for (i = 0; ps && i < ps->n; i++) {
        GeoList *one = glsub(ps, i, i + 1); Num top = up_(s, one, z, U_(rng, 0.7, 1.0), "ashlar");
        if (truthy(top)) { G *p = ps->v[i]->g, *cc = cen(p); double a = U_(rng, 0, PI); topcut_(s, inter(rect(gx(cc), gy(cc), 0.25, 6, a), inset(p, 0.3)), top, 0.18, "slot"); }
    }
    TIER_END(0);
}
static void sdf_at(void *c, double cx, double cy, GL *out) {
    PyRandom *rng = ((R1 *)c)->rng; double a = U_(rng, 0, PI); FF f = ff_(cx, cy, a); int i, j;
    for (i = -1; i < 2; i++) for (j = -1; j < 1; j++) if (RR(rng) > 0.3) { double aa = a + U_(rng, -0.12, 0.12); gl_add(out, rect(FX(f, i * 2.6, j * 2.6), 2.25, 2.25, aa)); }
}
static int t_sd_flags(Scene *s, PyRandom *rng, int lvl) {
    TIER_BEGIN
    R1 c = { rng }; Num z; GeoList *ps = group_(s, rng, sdf_at, &c, 0.4, &z); int i;
    for (i = 0; ps && i < ps->n; i++) { double h = U_(rng, 0.25, 0.6); up_(s, glsub(ps, i, i + 1), z, h, "ashlar"); }
    TIER_END(0);
}
static void sdw_at(void *c, double cx, double cy, GL *out) {
    PyRandom *rng = ((R1 *)c)->rng; double a = U_(rng, 0, PI), L; FF f = ff_(cx, cy, a);
    L = U_(rng, 6.0, 8.0); gl_add(out, rect(FX(f, 0, 0), L, 1.8, a)); gl_add(out, rect(FX(f, L / 2 + 0.8, 0), 1.2, 1.8, a));
}
static int t_sd_wallstub(Scene *s, PyRandom *rng, int lvl) {
    TIER_BEGIN
    R1 c = { rng }; Num z, top; GeoList *ps = group_(s, rng, sdw_at, &c, 0.4, &z);
    if (!ps || !ps->n) TIER_END(0);
    top = up_(s, glsub(ps, 0, 1), z, U_(rng, 1.2, 1.8), "ashlar");
    if (truthy(top)) {
        G *p = ps->v[0]->g, *cc = cen(p); double xy[20], a, us[8]; int n, i; GL *l = gl_new(); FF f;
        n = shp_coords(ext_ring(mrr(p)), xy, 10); if (n < 2) ns_raise();
        a = ns_atan2(xy[3] - xy[1], xy[2] - xy[0]); f = ff_(gx(cc), gy(cc), a);
        n = ns_arange(-3.5, 3.6, 2.2, us, 8); for (i = 0; i < n; i++) gl_add(l, rect(FX(f, us[i], 0), 0.25, 4, a));
        topcut_(s, inter(uu(l), inset(p, 0.25)), top, 0.2, "slot");
    }
    if (ps->n > 1) up_(s, glsub(ps, 1, 2), z, U_(rng, 2.0, 2.6), "ashlar");
    TIER_END(0);
}

/* ================================================================== style_ashveil.py */
static G *apl_mk(void *c) {
    PyRandom *rng = ((R1 *)c)->rng; double x, y, a, L, W, u1, u2, u3; FF f; PTS *p = pl_new();
    lf_rp(rng, 1, 9, &x, &y); a = U_(rng, 0, PI); f = ff_(x, y, a); L = U_(rng, 2.6, 3.6); W = U_(rng, 1.0, 1.4);
    pl_add(p, FX(f, -L / 2, -W / 2)); u1 = L / 2 * U_(rng, 0.6, 1.0); pl_add(p, FX(f, u1, -W / 2));
    u2 = W / 2 * U_(rng, -0.2, 0.6); pl_add(p, FX(f, L / 2, u2)); u3 = L / 2 * U_(rng, 0.2, 0.7); pl_add(p, FX(f, u3, W / 2)); pl_add(p, FX(f, -L / 2, W / 2));
    return buf(poly(p), 0, 16);
}
static int t_av_plate(Scene *s, PyRandom *rng, int lvl) {
    TIER_BEGIN
    R1 c = { rng }; Num z, top; Geo *g = find_(s, apl_mk, &c, 0.4, &z); G *in;
    if (!g) TIER_END(0);
    top = up_(s, gl1(g->g), z, U_(rng, 0.9, 1.4), "ashhull");
    if (truthy(top)) {
        GL *holes = gl_new(); in = inset(g->g, 0.4);
        if (!empty(in)) {
            double xy[512]; int n, i; if (shp_type(in) != GEOS_POLYGON) ns_raise();
            n = shp_coords(shp_exterior(in), xy, 256); if (n < 0) ns_raise();
            for (i = 0; i < n - 1; i += 2) gl_add(holes, PB(xy[2 * i], xy[2 * i + 1], 0.17, 6));
        }
        if (holes->n) topcut_(s, uu(holes), top, 0.25, "slot");
    }
    TIER_END(0);
}
static void ast_at(void *c, double cx, double cy, GL *out) {
    PyRandom *rng = ((R1 *)c)->rng; double a = U_(rng, 0, PI), gap; int k = (int)RI(rng, 2, 3), i; FF f;
    gap = U_(rng, 4.0, 4.8); f = ff_(cx, cy, a);
    for (i = 0; i < k; i++) gl_add(out, rect(FX(f, (i - (k - 1) / 2.0) * gap, 0), 1.0, 1.0, a));
}
static int t_av_stilts(Scene *s, PyRandom *rng, int lvl) {
    TIER_BEGIN
    R1 c = { rng }; Num z, top; GeoList *ps = group_(s, rng, ast_at, &c, 0.4, &z); GL *l = gl_new(); int i;
    if (!ps || !ps->n) TIER_END(0);
    top = up_(s, ps, z, U_(rng, 1.7, 2.3), "ashpipe");
    if (truthy(top)) { for (i = 0; i < ps->n; i++) gl_add(l, buf(cen(ps->v[i]->g), 0.32, 8)); topcut_(s, uu(l), top, 0.35, "slot"); }
    TIER_END(0);
}
static Geo *drum_one(void *c, double x, double y, int *err) {
    PyRandom *rng = ((R1 *)c)->rng; G *g = shp_point_buffer(x, y, U_(rng, 0.85, 1.0), 20);
    if (!g) { *err = 1; return NULL; }
    return geo_new(g);
}
static void adr_at(void *c, double cx, double cy, GL *out) {
    PyRandom *rng = ((R1 *)c)->rng; GeoList *l = gls(); PlOne one; int i, n = (int)RI(rng, 2, 3);
    memset(&one, 0, sizeof one); one.fn = drum_one; one.ctx = c;
    CK(pl_cluster(NULL, rng, n, &one, cx, cy, 2.2, 0.3, l));
    for (i = 0; i < l->n; i++) gl_add(out, clone(l->v[i]->g));
}
typedef struct { PyRandom *rng; Scene *s; } R2;
static void adr_at2(void *c, double cx, double cy, GL *out) {
    R2 *r = (R2 *)c; GeoList *l = gls(); PlOne one; int i, n = (int)RI(r->rng, 2, 3); R1 r1 = { r->rng };
    memset(&one, 0, sizeof one); one.fn = drum_one; one.ctx = &r1;
    CK(pl_cluster(r->s, r->rng, n, &one, cx, cy, 2.2, 0.3, l));
    for (i = 0; i < l->n; i++) gl_add(out, clone(l->v[i]->g));
}
static int t_av_drums(Scene *s, PyRandom *rng, int lvl) {
    TIER_BEGIN
    R2 c = { rng, s }; Num z, top; GeoList *ps = group_(s, rng, adr_at2, &c, 0.4, &z); GL *l = gl_new(); int i;
    if (!ps || !ps->n) TIER_END(0);
    top = up_(s, ps, z, U_(rng, 1.4, 1.8), "rust");
    if (truthy(top)) { for (i = 0; i < ps->n; i++) { G *cc = cen(ps->v[i]->g); gl_add(l, dif(buf(cc, 0.55, 16), buf(cc, 0.32, 16))); } topcut_(s, uu(l), top, 0.2, "slot"); }
    TIER_END(0);
}
/* _sd(name, kind, n).w: the detail of sd_r_x_ashveil.py named name, as comp(f, r, ground), placed randint(*n) times */
static const SdrEntry *sd_lookup(const char *style, const char *name) {
    int i, n = sdr_reg_count(style); for (i = 0; i < n; i++) { const SdrEntry *e = sdr_reg_get(style, i); if (!strcmp(e->name, name)) return e; }
    return NULL;
}
static int av_sd_tier(const char *name, int n0, int n1, Scene *s, PyRandom *rng, int lvl) {
    const SdrEntry *e = sd_lookup("ashveil", name); FtComp c; int got = 0, i, n, r;
    if (!e) return 0;
    c = ft_comp(sdr_f, (void *)e, e->r, 0.4, e->ground, 60);
    n = (int)RI(rng, n0, n1);
    for (i = 0; i < n; i++) { r = ft_comp_run(&c, s, rng, 1.0, ns_s3d); if (r < 0) return SC_ERR; got = r > 0 || got; }
    return got;
}
/* style_thornwild._sd(name, cls, kind).fn: the first of style_details.NEW20["thornwild"] whose shape is name, run once */
static int tw_sd_tier(const char *name, Scene *s, PyRandom *rng, int lvl) {
    const SdrEntry *e = sd_lookup("thornwild", name); FtComp c;
    if (!e) return 0;
    c = ft_comp(sdr_f, (void *)e, e->r, 0.4, e->ground, 60);
    return ft_comp_run(&c, s, rng, 1.0, ns_s3d);
}

/* ================================================================== style_obelis.py */
typedef struct { PyRandom *rng; double a, r, L, W, P; } Hold;
static G *om_mk(void *c) {          /* f_monolith.mk */
    Hold *h = (Hold *)c; double x, y, a, P_; lf_rp(h->rng, 2, 10, &x, &y); a = U_(h->rng, 0, PI); P_ = U_(h->rng, 3.0, 3.6); h->a = a;
    return ob_chamfer(rect(x, y, P_, P_, a), 0.25);
}
static int t_ob_monolith(Scene *s, PyRandom *rng, int lvl) {
    TIER_BEGIN
    Hold h = { rng }; Num z, top, t2, t3; Geo *g = find_(s, om_mk, &h, 0.4, &z); G *cc, *sl; double a;
    if (!g) TIER_END(0);
    a = h.a; cc = cen(g->g); top = up_(s, gl1(g->g), z, 0.45, "monolith");
    if (!truthy(top)) TIER_END(0);
    t2 = num_min2(num_add(top, num_f(0.35)), capz());
    if (!prism_(s, gl1(inset(g->g, 0.45)), t2, "monolith", num_sub(top, num_f(0.15)))) TIER_END(0);
    topcut_(s, dif(inset(g->g, 0.12), inset(g->g, 0.3)), top, 0.15, "slot");
    { double l1 = U_(rng, 1.1, 1.4), l2 = U_(rng, 2.2, 2.7); sl = ob_chamfer(rect(gx(cc), gy(cc), l1, l2, a), 0.2); }
    t3 = num_min2(num_add(t2, num_f(U_(rng, 1.6, 2.6))), capz());
    if (prism_(s, gl1(sl), t3, "monolith", num_sub(t2, num_f(0.15)))) topcut_(s, dif(inset(sl, 0.25), inset(sl, 0.45)), t3, 0.15, "slot");
    TIER_END(0);
}
static G *op_mk(void *c) {          /* f_pylon.mk */
    Hold *h = (Hold *)c; double x, y, r, a; lf_rp(h->rng, 1, 10, &x, &y); r = U_(h->rng, 1.25, 1.55); a = U_(h->rng, 0, PI); h->a = a; h->r = r;
    return ob_oct(x, y, r, a);
}
static int t_ob_pylon(Scene *s, PyRandom *rng, int lvl) {
    TIER_BEGIN
    Hold h = { rng }; Num z, top, t2; Geo *g = find_(s, op_mk, &h, 0.4, &z); G *cc, *sh; double a, r;
    if (!g) TIER_END(0);
    a = h.a; r = h.r; cc = cen(g->g); top = up_(s, gl1(g->g), z, U_(rng, 0.4, 0.55), "monolith");
    if (!truthy(top)) TIER_END(0);
    sh = ob_oct(gx(cc), gy(cc), r * 0.68, a); t2 = num_min2(num_add(top, num_f(U_(rng, 1.4, 2.4))), capz());
    if (prism_(s, gl1(sh), t2, "monolith", num_sub(top, num_f(0.15)))) topcut_(s, rect(gx(cc), gy(cc), 2 * r, 0.32, a), t2, 0.25, "slot");
    topcut_(s, dif(inset(g->g, 0.12), inset(g->g, 0.3)), top, 0.15, "slot");
    TIER_END(0);
}
static G *oc_mk(void *c) {          /* f_casket.mk */
    Hold *h = (Hold *)c; double x, y, a; lf_rp(h->rng, 0, 9, &x, &y); a = U_(h->rng, 0, PI); h->a = a; h->L = U_(h->rng, 4.2, 5.0); h->W = U_(h->rng, 1.8, 2.2);
    return ob_chamfer(rect(x, y, h->L, h->W, a), 0.45);
}
static int t_ob_casket(Scene *s, PyRandom *rng, int lvl) {
    TIER_BEGIN
    Hold h = { rng }; Num z, top, t2; Geo *g = find_(s, oc_mk, &h, 0.4, &z); G *cc, *lid; double a, L, W;
    if (!g) TIER_END(0);
    a = h.a; L = h.L; W = h.W; cc = cen(g->g); top = up_(s, gl1(g->g), z, U_(rng, 0.8, 1.0), "casket");
    if (!truthy(top)) TIER_END(0);
    lid = inset(g->g, 0.25); t2 = num_min2(num_add(top, num_f(0.25)), capz());
    if (prism_(s, gl1(lid), t2, "casket", num_sub(top, num_f(0.15)))) {
        FF f = ff_(gx(cc), gy(cc), a); GL *l = gl_new(); double xs[3] = { -L * 0.25, 0.0, L * 0.25 }; int i;
        gl_add(l, dif(inset(lid, 0.3), inset(lid, 0.52)));
        for (i = 0; i < 3; i++) gl_add(l, rect(FX(f, xs[i], 0), 0.26, W * 0.4, a));
        topcut_(s, inter(uu(l), lid), t2, 0.18, "slot");
    }
    TIER_END(0);
}
static void opa_at(void *c, double cx, double cy, GL *out) {          /* f_panels.at */
    PyRandom *rng = ((R1 *)c)->rng; double a0 = U_(rng, 0, PI); int i, j, n = (int)RI(rng, 2, 3); GL *l = gl_new();
    for (i = 0; i < n; i++) {
        FF f = ff_(cx, cy, a0 + i * 1.9), cc; double u = U_(rng, -1.2, 1.2), v = U_(rng, -1.2, 1.2), x = fx(&f, u, v), y = fy(&f, u, v), a = a0 + i * 1.9 + U_(rng, -0.3, 0.3), L, W; G *q, *cutl; PTS *p = pl_new();
        L = U_(rng, 2.2, 3.0); W = U_(rng, 1.4, 1.9); q = rect(x, y, L, W, a); cc = ff_(x, y, a);
        { double u1 = U_(rng, -0.3, 0.5); pl_add(p, FX(cc, u1, -2)); } pl_add(p, FX(cc, 2, -2)); pl_add(p, FX(cc, 2, 2)); { double u2 = U_(rng, 0.2, 1.2); pl_add(p, FX(cc, u2, 2)); }
        cutl = poly(p);
        if (RR(rng) < 0.7) q = dif(q, cutl);
        if (!empty(q) && area(q) > 1.2) { GL *ps = polys(q); int bi = 0; double ba; if (!ps->n) ns_raise(); ba = area(ps->v[0]); for (j = 1; j < ps->n; j++) if (area(ps->v[j]) > ba) { ba = area(ps->v[j]); bi = j; } gl_add(l, ps->v[bi]); }
    }
    for (i = 0; i < l->n; i++) { int ok = 1; for (j = 0; j < l->n && ok; j++) if (j != i && !(l->v[j] == l->v[i])) ok = dist(l->v[i], l->v[j]) > 0.35; if (ok) gl_add(out, l->v[i]); }
}
static int t_ob_panels(Scene *s, PyRandom *rng, int lvl) {
    TIER_BEGIN
    R1 c = { rng }; Num z, top; GeoList *ps = group_(s, rng, opa_at, &c, 0.4, &z); GL *l = gl_new(); int i;
    if (!ps || !ps->n) TIER_END(0);
    top = up_(s, ps, z, U_(rng, 0.3, 0.4), "obmetal");
    if (truthy(top)) { for (i = 0; i < ps->n; i++) gl_add(l, dif(inset(ps->v[i]->g, 0.3), inset(ps->v[i]->g, 0.5))); topcut_(s, uu(l), top, 0.12, "slot"); }
    TIER_END(0);
}
static G *og_mk(void *c) {          /* f_glyphslab.mk */
    Hold *h = (Hold *)c; double x, y, a, P_; lf_rp(h->rng, 0, 10, &x, &y); a = U_(h->rng, 0, PI); P_ = U_(h->rng, 2.8, 3.4); h->a = a; h->P = P_;
    return ob_chamfer(rect(x, y, P_, P_, a), 0.2);
}
static int t_ob_glyphslab(Scene *s, PyRandom *rng, int lvl) {
    TIER_BEGIN
    Hold h = { rng }; Num z, top; Geo *g = find_(s, og_mk, &h, 0.4, &z); G *cc;
    if (!g) TIER_END(0);
    cc = cen(g->g); top = up_(s, gl1(g->g), z, U_(rng, 0.4, 0.6), "monolith");
    if (truthy(top)) { G *gl = inter(ob_glyph(gx(cc), gy(cc), h.P * 0.78, h.a, rng, 0.3), inset(g->g, 0.3)); if (!empty(gl)) topcut_(s, gl, top, 0.2, "slot"); }
    TIER_END(0);
}
static G *or_mk(void *c) {          /* f_ring.mk */
    Hold *h = (Hold *)c; double x, y, R_, w; lf_rp(h->rng, 1, 10, &x, &y); R_ = U_(h->rng, 1.8, 2.3); h->r = R_; h->a = U_(h->rng, 0, 1);
    w = R_ - U_(h->rng, 0.6, 0.75);
    return dif(PB(x, y, R_, 32), PB(x, y, w, 32));
}
static int t_ob_ring(Scene *s, PyRandom *rng, int lvl) {
    TIER_BEGIN
    Hold h = { rng }; Num z, top; Geo *g = find_(s, or_mk, &h, 0.4, &z); G *cc;
    if (!g) TIER_END(0);
    cc = cen(g->g); top = up_(s, gl1(g->g), z, U_(rng, 0.4, 0.55), "obmetal");
    if (truthy(top)) {
        int n = (int)CH(rng, 6, 8), k; GL *l = gl_new();
        for (k = 0; k < n; k++) { double t = h.a + 6.2832 * k / n; gl_add(l, rect(gx(cc) + h.r * ns_cos(t), gy(cc) + h.r * ns_sin(t), 0.6, 0.3, t)); }
        topcut_(s, inter(uu(l), g->g), top, 0.2, "slot");
    }
    TIER_END(0);
}
static int t_ob_drift(Scene *s, PyRandom *rng, int lvl) {          /* _drifts(s, rng, 1, big=(1.6, 2.4)) */
    static const Tier T2[2] = { { { 0.5, NUM_FLOAT, 0 }, { 25, NUM_INT, 0 } }, { { 0.5, NUM_FLOAT, 0 }, { 50, NUM_INT, 0 } } };
    TIER_BEGIN
    s2_banks(s, rng, 1, 1.6, 2.4, 0.4, 0.8, "sand", T2, 2, NULL);
    TIER_END(0);
}

/* ================================================================== style_blight.py */
static const FtTiers SPIKE = { 2, { { { 0.75, NUM_FLOAT, 0 }, { 4, NUM_INT, 0 } }, { { 0.25, NUM_FLOAT, 0 }, { 30, NUM_INT, 0 } } } };
static const FtTiers BONE_T = { 3, { { { 0.45, NUM_FLOAT, 0 }, { 12, NUM_INT, 0 } }, { { 0.3, NUM_FLOAT, 0 }, { 38, NUM_INT, 0 } }, { { 0.25, NUM_FLOAT, 0 }, { 62, NUM_INT, 0 } } } };
static void bt_at(void *c, double cx, double cy, GL *out) {          /* f_thorns.at */
    PyRandom *rng = ((R1 *)c)->rng; int i, j, n; GL *ps = gl_new();
    { double r = U_(rng, 0.75, 1.05), asp = U_(rng, 0.6, 0.85); int nv = (int)CH(rng, 3, 4); gl_add(ps, stone(cx, cy, r, rng, asp, nv)); }
    n = (int)RI(rng, 2, 4);
    for (i = 0; i < n; i++) {
        double t = U_(rng, 0, 6.283), d = U_(rng, 1.4, 2.0), r = U_(rng, 0.5, 0.7); G *q = stone(cx + d * ns_cos(t), cy + d * ns_sin(t), r, rng, 0.7, 3); int ok = 1;
        for (j = 0; j < ps->n && ok; j++) ok = dist(q, ps->v[j]) > 0.25;
        if (ok) gl_add(ps, q);
    }
    for (i = 0; i < ps->n; i++) gl_add(out, ps->v[i]);
}
static int t_bl_thorns(Scene *s, PyRandom *rng, int lvl) {
    TIER_BEGIN
    R1 c = { rng }; Num z; GeoList *ps = group_(s, rng, bt_at, &c, 0.4, &z);
    if (!ps || !ps->n) TIER_END(0);
    mound_(s, glsub(ps, 0, 1), z, U_(rng, 1.8, 2.6), &SPIKE, "thorn");
    if (ps->n > 1) mound_(s, glsub(ps, 1, ps->n), z, U_(rng, 1.0, 1.5), &SPIKE, "thorn");
    TIER_END(0);
}
static G *bb_mk(void *c) { PyRandom *rng = ((R1 *)c)->rng; double x, y, r; lf_rp(rng, 1, 9, &x, &y); r = U_(rng, 1.5, 2.1); return buf(blob(x, y, r, rng, 4), 1.0, 16); }
static int t_bl_boil(Scene *s, PyRandom *rng, int lvl) {
    TIER_BEGIN
    R1 c = { rng }; Num z; Geo *g = find_(s, bb_mk, &c, 0.4, &z); G *pool; GL *rim, *bl = gl_new(), *bub = gl_new(); int i, j, k; double b[4];
    if (!g || z.v > SC.H + 1e-6 || pl_terr(s)) TIER_END(0);
    pool = inset(g->g, 1.0);
    if (empty(pool)) TIER_END(0);
    CK(pl_cut(s, gl1(pool), z, num_f(0.6), K("ichor")));
    { GL *ps = polys(dif(g->g, buf(pool, 0.25, 16))); rim = gl_new(); for (i = 0; i < ps->n; i++) if (area(ps->v[i]) > 0.3) gl_add(rim, ps->v[i]); }
    for (i = 0; i < rim->n; i++) for (k = 0; k < 5; k++) {
        G *cc = rpt(rim->v[i]); double x = gx(cc) + U_(rng, -0.6, 0.6), y = gy(cc) + U_(rng, -0.6, 0.6), r = U_(rng, 0.45, 0.7); G *bb = inter(stone(x, y, r, rng, 0.85, 7), rim->v[i]); int ok;
        ok = !empty(bb) && area(bb) > 0.3;
        for (j = 0; j < bl->n && ok; j++) ok = dist(bb, bl->v[j]) > 0.2;
        if (ok) gl_add(bl, bb);
    }
    if (bl->n) {
        GeoList *ps = gls(); for (i = 0; i < bl->n; i++) { GL *pp = polys(bl->v[i]); for (j = 0; j < pp->n; j++) if (wide(pp->v[j], 0.7)) gl_push(ps, H(pp->v[j])); }
        mound_(s, ps, z, U_(rng, 0.4, 0.65), &NS_SOFT_T, "blister");
    }
    bounds(pool, b);
    for (i = 0; i < 80; i++) {
        double x, y, r; G *bb; int ok;
        if (bub->n >= RI(rng, 2, 4)) break;
        x = U_(rng, b[0], b[2]); y = U_(rng, b[1], b[3]); r = U_(rng, 0.5, 0.75); bb = PB(x, y, r, 14);
        ok = contains(inset(pool, 0.3), bb);
        for (j = 0; j < bub->n && ok; j++) ok = dist(bb, bub->v[j]) > 0.35;
        if (ok) gl_add(bub, bb);
    }
    if (bub->n) { GeoList *ps = gls(); for (i = 0; i < bub->n; i++) gl_push(ps, H(bub->v[i])); prism_(s, ps, num_f(SC.H - 0.15), "blister", num_f(SC.H - 0.75)); }
    TIER_END(0);
}
static G *br_mk(void *c) { PyRandom *rng = ((R1 *)c)->rng; double x, y, r; lf_rp(rng, 1, 8, &x, &y); r = U_(rng, 2.6, 3.2); return PB(x, y, r, 24); }
static int t_bl_runes(Scene *s, PyRandom *rng, int lvl) {
    TIER_BEGIN
    R1 c = { rng }; Num z; Geo *g = find_(s, br_mk, &c, 0.3, &z); G *cc, *g2, *cr; double b[4], Rr, a0, ca; int n, i; GL *marks = gl_new(); GeoList *p1 = gls(), *p2 = gls(); GL *ps;
    if (!g) TIER_END(0);
    cc = cen(g->g); bounds(g->g, b); Rr = (b[2] - b[0]) / 2 - 0.25; a0 = U_(rng, 0, 6.283); n = (int)RI(rng, 5, 7);
    gl_add(marks, ring(gx(cc), gy(cc), Rr, Rr - 0.35, 40));
    for (i = 0; i < n; i++) { double t = a0 + 6.283 * i / n; FF f = ff_(gx(cc) + Rr * 0.6 * ns_cos(t), gy(cc) + Rr * 0.6 * ns_sin(t), t + PI / 2); gl_add(marks, ns_bl_rune(&f, RB(rng, 8), 1.1, 0.28)); }
    g2 = uu(marks);
    ca = U_(rng, 0, 6.283);
    cr = bufx(line(wiggle(gx(cc) - (Rr + 0.6) * ns_cos(ca), gy(cc) - (Rr + 0.6) * ns_sin(ca), gx(cc) + (Rr + 0.6) * ns_cos(ca), gy(cc) + (Rr + 0.6) * ns_sin(ca), rng, 0.5, 5)), 0.25, 3, SHP_CAP_ROUND, SHP_JOIN_ROUND, 5.0);
    ps = polys(dif(g2, buf(cr, 0.3, 16))); for (i = 0; i < ps->n; i++) gl_push(p1, H(ps->v[i]));
    CK(pl_cut(s, p1, z, num_f(0.35), K("runecut")));
    ps = polys(cr); for (i = 0; i < ps->n; i++) gl_push(p2, H(ps->v[i]));
    CK(pl_cut(s, p2, z, num_f(0.5), K("rift")));
    TIER_END(0);
}
static void bo_at(void *c, double cx, double cy, GL *out) {          /* f_bones.at */
    PyRandom *rng = ((R1 *)c)->rng; int i, n; double r = U_(rng, 1.3, 1.7);
    gl_add(out, blob(cx, cy, r, rng, 4));
    n = (int)RI(rng, 2, 3);
    for (i = 0; i < n; i++) {
        double t = U_(rng, 0, PI), L = U_(rng, 2.6, 3.4), ox = cx + U_(rng, -0.6, 0.6), oy = cy + U_(rng, -0.6, 0.6); FF f = ff_(ox, oy, t); GL *l = gl_new(); PTS *p = pl_new();
        pl_add(p, FX(f, -L / 2, 0)); pl_add(p, FX(f, L / 2, 0));
        gl_add(l, bufx(line(p), 0.32, 16, SHP_CAP_ROUND, SHP_JOIN_ROUND, 5.0)); gl_add(l, PB(FX(f, -L / 2, 0), 0.5, 10)); gl_add(l, PB(FX(f, L / 2, 0), 0.5, 10));
        gl_add(out, uu(l));
    }
}
static int t_bl_bones(Scene *s, PyRandom *rng, int lvl) {
    TIER_BEGIN
    R1 c = { rng }; Num z; GeoList *ps = group_(s, rng, bo_at, &c, 0.4, &z); int i;
    if (!ps || !ps->n) TIER_END(0);
    mound_(s, glsub(ps, 0, 1), z, U_(rng, 0.5, 0.7), &NS_SOFT_T, "bone");
    for (i = 1; i < ps->n; i++) mound_(s, glsub(ps, i, i + 1), z, U_(rng, 0.7, 0.95), &BONE_T, "bone");
    TIER_END(0);
}
/* pat_brune().f(cx, cy, g, rng) */
static Geo *pat_brune(double cx, double cy, const Geo *g, PyRandom *rng, int *err) {
    Geo *r = NULL; NsFrame fr; ns_enter(&fr);
    if (setjmp(fr.jb)) { ns_leave(&fr); *err = 1; return NULL; }
    { double t = U_(rng, 0, 6.283); FF f = ff_(cx, cy, t); int64_t k = RB(rng, 8); r = geo_new(shp_clone(ns_bl_rune(&f, k, 1.7, 0.32))); }
    ns_leave(&fr); return r;
}

void ns_bld_tiers(void) {
    bld_add_any("style_sundered", "sd_blocks", "tier", (void *)t_sd_blocks); bld_add_any("style_sundered", "sd_flags", "tier", (void *)t_sd_flags);
    bld_add_any("style_sundered", "sd_wallstub", "tier", (void *)t_sd_wallstub);
    bld_add_any("style_ashveil", "_plate", "tier", (void *)t_av_plate); bld_add_any("style_ashveil", "_stilts", "tier", (void *)t_av_stilts);
    bld_add_any("style_ashveil", "_drums", "tier", (void *)t_av_drums); bld_add_any("style_ashveil", "_sd", "ns_sd_tier", (void *)av_sd_tier);
    bld_add_any("style_thornwild", "_sd", "ns_sd_tier1", (void *)tw_sd_tier);
    bld_add_any("style_obelis", "f_monolith", "tier", (void *)t_ob_monolith); bld_add_any("style_obelis", "f_pylon", "tier", (void *)t_ob_pylon);
    bld_add_any("style_obelis", "f_casket", "tier", (void *)t_ob_casket); bld_add_any("style_obelis", "f_panels", "tier", (void *)t_ob_panels);
    bld_add_any("style_obelis", "f_glyphslab", "tier", (void *)t_ob_glyphslab); bld_add_any("style_obelis", "f_ring", "tier", (void *)t_ob_ring);
    bld_add_any("style_obelis", "f_drift", "tier", (void *)t_ob_drift);
    bld_add_any("style_blight", "f_thorns", "tier", (void *)t_bl_thorns); bld_add_any("style_blight", "f_boil", "tier", (void *)t_bl_boil);
    bld_add_any("style_blight", "f_runes", "tier", (void *)t_bl_runes); bld_add_any("style_blight", "f_bones", "tier", (void *)t_bl_bones);
    bld_add_any("style_blight", "pat_brune", "ns_pat", (void *)pat_brune);
    (void)adr_at;
}
