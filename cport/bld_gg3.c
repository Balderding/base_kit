/* bld_gg3.c: the tier functions of gg3.py (b_* badlands, h_* highland, m_* maahl, c_* corridor, i_* infestation,
   u_* urban, n_* mine: the TIERS table), copied routine by routine; see bld_gg3.h. Each C function names the Python
   function it copies; every random number is drawn in the Python's order (each rng call its own statement: C does
   not order the arguments of a call). The shared toolkit (bs_find ...) is at the top. */
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include "bld_gg3.h"

#define UNI(a, b) py_uniform(rng, (a), (b))
#define RND() py_random(rng)
#define COS py_cos
#define SIN py_sin
#define H (SC.H)
#define CAPZ (SC.CAP)
#define DISC (PL.disc)

/* ================================================================ the toolkit */
double bs_pow(double x, double y) { double re = 0, im = 0; int r = px_pow(x, y, &re, &im); if (r != 0) ly_raise(); return re; }
double bs_rpow(double x, double y) { double re = 0, im = 0; int r = px_pow(x, y, &re, &im); if (r < 0) ly_raise(); return re; }
int64_t bs_int(double x) { return ly_int(x); }
int64_t bs_randint(PyRandom *rng, int64_t a, int64_t b) { int64_t v; if (py_randint_e(rng, a, b, &v) != 0) ly_raise(); return v; }
double bs_choice_pm1(PyRandom *rng) { return py_choice(rng, 2) ? 1.0 : -1.0; }

/* the callbacks place.h calls: each runs its body in its own raise frame */
typedef struct { BsMkBody mk; BsSaBody sa; BsOneBody one; void *ctx; } BsCb;
static Geo *cb_make(void *c, int has_c, double cx, double cy, int *err) {
    BsCb *b = (BsCb *)c; jmp_buf jb, *old = ly_jb; Geo *g; (void)has_c; (void)cx; (void)cy;
    ly_jb = &jb;
    if (setjmp(jb)) { ly_jb = old; *err = 1; return NULL; }
    g = b->mk(b->ctx);
    ly_jb = old;
    return geo_ref(g);
}
static int cb_sa(void *c, double cx, double cy, GeoList *out) {
    BsCb *b = (BsCb *)c; jmp_buf jb, *old = ly_jb;
    ly_jb = &jb;
    if (setjmp(jb)) { ly_jb = old; return SC_ERR; }
    b->sa(b->ctx, cx, cy, out);
    ly_jb = old;
    return 0;
}
static Geo *cb_one(void *c, double x, double y, int *err) {
    BsCb *b = (BsCb *)c; jmp_buf jb, *old = ly_jb; Geo *g;
    ly_jb = &jb;
    if (setjmp(jb)) { ly_jb = old; *err = 1; return NULL; }
    g = b->one(b->ctx, x, y);
    ly_jb = old;
    return geo_ref(g);
}
Geo *bs_find(Scene *s, BsMkBody mk, void *ctx, int tries, double gap, Geo *region, int ground, const char *allow1, Num *z) {
    BsCb cb = { mk, NULL, NULL, ctx }; Geo *g = NULL; const char *al[1] = { allow1 }; PlAllow allow = { allow1 ? 1 : 0, al };
    int r = pl_find(s, cb_make, &cb, tries, gap, region, ground, &allow, NULL, NULL, &g, z);
    LC(r);
    if (r == 0) { *z = num_none(); return NULL; }
    return LAG(g);
}
GeoList *bs_place_group(Scene *s, PyRandom *rng, BsSaBody sa, void *ctx, int tries, double gap, int ground, Geo *region, Num *z) {
    BsCb cb = { NULL, sa, NULL, ctx }; GeoList *out = LL();
    int r = pl_place_group(s, rng, cb_sa, &cb, tries, gap, ground, region, out, z);
    LC(r);
    if (r == 0) { *z = num_none(); return NULL; }
    return out;
}
void bs_cluster(Scene *s, PyRandom *rng, int n, BsOneBody one, void *ctx, double cx, double cy, double spread, double gap, GeoList *out) {
    BsCb cb = { NULL, NULL, one, ctx }; PlOne po; memset(&po, 0, sizeof po); po.fn = cb_one; po.ctx = &cb;
    LC(pl_cluster(s, rng, n, &po, cx, cy, spread, gap, out));
}
Num bs_up(Scene *s, const GeoList *polys, Num z, Num h, const char *kind) { Num t; LC(pl_up(s, polys, z, h, KS(kind), &t)); return t; }
void bs_mound(Scene *s, const GeoList *polys, Num z, Num h, const Tier *t, int nt, const char *kind) { LC(pl_mound(s, polys, z, h, t, nt, KS(kind))); }
void bs_cut(Scene *s, const GeoList *polys, Num z, Num depth, const char *kind) { LC(pl_cut(s, polys, z, depth, KS(kind))); }
void bs_topcut(Scene *s, const GeoList *polys, Num ztop, Num depth, const char *kind) { LC(pl_topcut(s, polys, ztop, depth, KS(kind))); }
void bs_drape(Scene *s, Geo *g, Num h, const char *kind) { LC(pl_drape(s, g, h, KS(kind))); }
GeoList *bs_hard(Scene *s) { GeoList *l = LL(); LC(pl_hard(s, NULL, l)); return l; }
void bs_evict(Scene *s, Geo *g) { LC(pl_evict(s, g)); }
Num bs_surf(Scene *s, Geo *g) { Num z; LC(pl_surf(s, g, &z)); return z; }

/* shorthands */
#define NUMH num_f(H)
static Geo *bar_(double px, double py, double qx, double qy, double w) { double p[2] = { px, py }, q[2] = { qx, qy }; return LA(so_bar(p, q, w)); }
static Geo *rect_(double cx, double cy, double l, double w, double a) { return LAG(lf_rect(cx, cy, l, w, a)); }
static Geo *star_(double cx, double cy, double ro, double ri, int n, double rot) { return LAG(lf_star(cx, cy, ro, ri, n, rot)); }
static Geo *blob_(double cx, double cy, double r, PyRandom *rng, int k) { return LAG(lf_blob(cx, cy, r, rng, k)); }
static Geo *ring_(double cx, double cy, double ro, double ri, int res) { return LAG(lf_ring(cx, cy, ro, ri, res)); }
static Geo *hexa_(double cx, double cy, double r) { return LAG(lf_hexa(cx, cy, r, 0.0)); }
static Geo *meander_(PyRandom *rng, double phi, double off, double amp, double w, int n) { return LAG(lf_meander(rng, phi, off, amp, w, n)); }
static Geo *radial_(double cx, double cy, double r0, double r1, int n, PyRandom *rng, double w) { return LAG(lf_radial(cx, cy, r0, r1, n, rng, w)); }
static Geo *seg_(double ax, double ay, double bx, double by) { double xy[4] = { ax, ay, bx, by }; return g_line(xy, 2); }
static Geo *wigl_(double ax, double ay, double bx, double by, PyRandom *rng, double amp) { return g_wigline(ax, ay, bx, by, rng, amp, 3); }
static Geo *frpoly_(const Frame *f, const double *uv, int n) { return g_frpoly(f, uv, n); }
static Frame fr_(double cx, double cy, double a) { return lf_frame(cx, cy, a); }
static void at_(const Frame *f, double u, double v, double *x, double *y) { lf_at(f, u, v, x, y); }
static Geo *rectf_(const Frame *f, double u, double v, double l, double w, double a) { double x, y; lf_at(f, u, v, &x, &y); return rect_(x, y, l, w, a); }
/* g.minimum_rotated_rectangle.exterior.coords */
static int mrr_coords(Geo *g, double **xy) { Geo *m = LA(shp_min_rot_rect(g->g)); return g_coords(g_ext(m), xy); }
static double cx_(Geo *g) { return g_x(g_centroid(g)); }
static double cy_(Geo *g) { return g_y(g_centroid(g)); }
static GeoList *ops_polys(Scene *s, int type, const char *kind) {          /* [p for op in s.ops if op[0] == type and op[-1] == kind for p in op[1]] */
    GeoList *l = LL(); int i, j;
    for (i = 0; i < s->ops.n; i++) { Op *o = s->ops.v[i]; if (o->type == type && kind_is(o->kind, kind)) for (j = 0; j < o->polys.n; j++) LP(l, o->polys.v[j]); }
    return l;
}
static int in_soft(const Kind *k) { return sm_set_has(SM.soft, k->s); }
static int in_bridge(const Kind *k) { return sm_set_has(SM.bridge, k->s); }
static const Tier *TS2(Tier *t, double f0, double a0, double f1, double a1) { t[0] = TT(f0, a0); t[1] = TT(f1, a1); return t; }
static void tcl_free_(TcutList *l) { int i; for (i = 0; i < l->n; i++) { gl_free(&l->v[i].polys); kind_unref(l->v[i].kind); } free(l->v); l->v = NULL; l->n = l->cap = 0; }

/* ================================================================ gg2.crater_t (layouts_gg2.c keeps its own static copy) */
void bs_crater_t(Scene *s, double cx, double cy, double r) {
    PyRandom *rng = &s->rng; Geo *hole; GeoList *lip = LL(), *cracks = LL(), *ps, *keep = LL(); int k, nk; Geo *dk = g_ptbuf(0, 0, SC.R0 - 0.3, 16);
    Tier tr[2];
    { double asp = UNI(0.75, 0.95); hole = g_stone(cx, cy, r, rng, asp, 9); }
    S_gcut(s, L1(hole), num_f(py_hypot(cx, cy) > 4 ? 0.8 : 0.7), "pit");
    gl_push(&s->keepout, g_buf(hole, 0.2));
    nk = (int)bs_randint(rng, 7, 11);
    for (k = 0; k < nk; k++) { double a = UNI(0, 2 * PI), d = r + UNI(0.3, 0.9), rr = UNI(0.6, 1.0); LP(lip, g_stone(cx + d * COS(a), cy + d * SIN(a), rr, rng, 0.8, 5)); }
    nk = (int)bs_randint(rng, 3, 5);
    for (k = 0; k < nk; k++) {
        double a = UNI(0, 2 * PI), p0x = cx + r * 0.8 * COS(a), p0y = cy + r * 0.8 * SIN(a), p1x = cx + (r + 1.5) * COS(a), p1y = cy + (r + 1.5) * SIN(a);
        LP(cracks, g_bufx(wigl_(p0x, p0y, p1x, p1y, rng, 0.2), 0.22, 4, SHP_CAP_ROUND, SHP_JOIN_ROUND, 5.0));
    }
    ps = g_polys(g_uu(lip));
    for (k = 0; k < ps->n; k++) if (g_contains(dk, ps->v[k])) LP(keep, ps->v[k]);
    S_tstack(s, keep, num_f(0.75), TS2(tr, 0.65, 8, 0.35, 40), 2, "rock");
    S_gcut(s, L1(g_uu(cracks)), num_f(0.4), "crack");
}

/* ================================================================ BADLANDS */
/* b_wash */
static void b_wash_(Scene *s, PyRandom *rng, int lvl) {
    double phi = UNI(0, PI), off = UNI(-6, 6), amp = UNI(1.0, 2.2); Geo *ln = meander_(rng, phi, off, amp, 1.6, 40); double w;
    (void)lvl;
    w = UNI(0.7, 0.95);
    S_gcut(s, L1(g_bufx(ln, w, 16, SHP_CAP_FLAT, SHP_JOIN_ROUND, 5.0)), num_f(0.5), "wash");
    geo_unref(s->wash); s->wash = geo_ref(ln);
}
/* b_hoodoo: lambda: stone(*rp(rng, 2, 12), r, rng, .85, 8) */
typedef struct { PyRandom *rng; double r, d0, d1, asp; int nv; } StoneMk;
static Geo *mk_stone(void *c) { StoneMk *m = (StoneMk *)c; double x, y; bs_rp(m->rng, m->d0, m->d1, &x, &y); return g_stone(x, y, m->r, m->rng, m->asp, m->nv); }
static void b_hoodoo_(Scene *s, PyRandom *rng, int lvl) {
    int k, n = (int)bs_randint(rng, 1, 2) + (lvl >= 3); Tier t3[3] = { TT(0.4, 4), TT(0.3, 7), TT(0.3, 10) };
    for (k = 0; k < n; k++) {
        StoneMk m = { rng, UNI(1.5, 1.9), 2, 12, .85, 8 }; Num z, zt; Geo *g; GeoList *profs;
        g = bs_find(s, mk_stone, &m, 120, 0.5, NULL, 0, NULL, &z);
        if (!g) continue;
        if (pl_terr(s)) continue;
        profs = S_stack(s, L1(g), num_f(2.3), t3, 3, "spire", num_none(), &zt);
        if (profs->n) S_prism(s, L1(g_buf(g_max_area(profs), 0.35)), num_min2(num_add(zt, num_f(0.45)), num_f(CAPZ)), "rock", num_sub(zt, num_f(0.15)), SC.MINW, 0);
    }
}
/* b_mudcrack: lambda: blob(*rp(rng, 0, 9), rng.uniform(3.0, 4.0), rng) */
typedef struct { PyRandom *rng; double d0, d1, r0, r1; int k; } BlobMk;
static Geo *mk_blob(void *c) { BlobMk *m = (BlobMk *)c; double x, y, r; bs_rp(m->rng, m->d0, m->d1, &x, &y); r = py_uniform(m->rng, m->r0, m->r1); return blob_(x, y, r, m->rng, m->k); }
static void b_mudcrack_(Scene *s, PyRandom *rng, int lvl) {
    BlobMk m = { rng, 0, 9, 3.0, 4.0, 4 }; Num z; Geo *g = bs_find(s, mk_blob, &m, 120, 0.5, NULL, 1, NULL, &z); (void)lvl;
    if (g) LC(pl_crack_plate(s, rng, g, z, num_f(0.3), KS("pan"), 420, 0.2));
}
/* b_agave: lambda: star(*rp(rng, 1, 12), ro, ro * 0.45, 8, rng.uniform(0, 1)) */
typedef struct { PyRandom *rng; double d0, d1, ro, ri; int n; } StarMk;
static Geo *mk_star(void *c) { StarMk *m = (StarMk *)c; double x, y, rot; bs_rp(m->rng, m->d0, m->d1, &x, &y); rot = py_uniform(m->rng, 0, 1); return star_(x, y, m->ro, m->ri, m->n, rot); }
static void b_agave_(Scene *s, PyRandom *rng, int lvl) {
    int k, n = (int)bs_randint(rng, 2, 3); Tier t[2]; (void)lvl;
    for (k = 0; k < n; k++) {
        double ro = UNI(1.3, 1.8); StarMk m = { rng, 1, 12, ro, ro * 0.45, 8 }; Num z; Geo *g = bs_find(s, mk_star, &m, 120, 0.4, NULL, 0, NULL, &z);
        if (g) bs_mound(s, L1(g), z, num_f(1.4), TS2(t, 0.5, 20, 0.5, 45), 2, "agave");
    }
}
/* b_washbed: lambda: stone(*s.wash.interpolate(rng.uniform(0, s.wash.length)).coords[0], rng.uniform(0.5, 0.7), rng, .8, 7) */
typedef struct { Scene *s; PyRandom *rng; } WashMk;
static Geo *mk_washbed(void *c) {
    WashMk *m = (WashMk *)c; double d = py_uniform(m->rng, 0, g_len(m->s->wash)); Geo *p = g_interp(m->s->wash, d); double x = g_x(p), y = g_y(p), r = py_uniform(m->rng, 0.5, 0.7);
    return g_stone(x, y, r, m->rng, .8, 7);
}
static void b_washbed_(Scene *s, PyRandom *rng, int lvl) {
    Geo *band; int k, n; Tier t[2]; (void)lvl;
    if (!s->wash) return;
    band = g_buf(s->wash, 1.1); n = (int)bs_randint(rng, 10, 16);
    for (k = 0; k < n; k++) {
        WashMk m = { s, rng }; Num z; Geo *g = bs_find(s, mk_washbed, &m, 30, 0.15, g_inter(band, DISC), 0, "wash", &z);
        if (g) S_stack(s, L1(g), num_f(0.4), TS2(t, 0.7, 10, 0.3, 45), 2, "pebble", num_none(), NULL);
    }
}
/* b_spiral */
static void b_spiral_(Scene *s, PyRandom *rng, int lvl) {
    int i, j, best = -1; Num bz = num_none(); Geo *bp = NULL, *q, *c, *sp; double ba = 0; (void)lvl;
    for (i = 0; i < s->ops.n; i++) {          /* max(tops, key=(t[0], t[1].area)): the first of equal keys */
        Op *o = s->ops.v[i];
        if (o->type != OP_PRISM || !kind_is(o->kind, "rock")) continue;
        for (j = 0; j < o->polys.n; j++) {
            Geo *p = o->polys.v[j]; double a;
            if (!g_wide(p, 4.0)) continue;
            a = g_area(p);
            if (best < 0 || o->n[1].v > bz.v || (o->n[1].v == bz.v && a > ba)) { best = 1; bz = o->n[1]; bp = p; ba = a; }
        }
    }
    if (best < 0) return;
    q = g_inset(bp, 1.4); c = g_reppt(!g_isempty(q) ? q : bp);
    { double r = UNI(1.2, 1.7); sp = g_inter(LAG(lf_spiral(g_x(c), g_y(c), r, 2.2, 0.4)), g_inset(bp, 0.4)); }
    bs_topcut(s, L1(sp), bz, num_f(0.3), "glyph");
}
/* b_split */
static void b_split_(Scene *s, PyRandom *rng, int lvl) {
    StoneMk m = { rng, UNI(2.4, 2.9), 2, 11, .8, 8 }; Num z; Geo *g, *slot; GeoList *halves; int i; double a; (void)lvl;
    g = bs_find(s, mk_stone, &m, 120, 0.5, NULL, 1, NULL, &z);
    if (!g) return;
    a = UNI(0, PI);
    slot = rect_(cx_(g), cy_(g), 8, 0.6, a);
    halves = g_polys(g_diff(g, slot));
    for (i = 0; i < halves->n && i < 2; i++) bs_up(s, L1(halves->v[i]), z, num_f(2.2 - 0.4 * i), "spire");
}
/* b_scree: lambda: stone(b.x + d * math.cos(a), b.y + d * math.sin(a), rng.uniform(0.5, 0.8), rng, .7, 5, round_=False) */
typedef struct { PyRandom *rng; double x, y, r0, r1, asp; int nv; } StoneAt;
static Geo *mk_stone_at(void *c) { StoneAt *m = (StoneAt *)c; double r = py_uniform(m->rng, m->r0, m->r1); return g_stone(m->x, m->y, r, m->rng, m->asp, m->nv); }
static void b_scree_(Scene *s, PyRandom *rng, int lvl) {
    GeoList *rocks = LL(); int i, j, k, n; Geo *big, *ext; double bcx, bcy; Tier t[2]; (void)lvl;
    for (i = 0; i < s->ops.n; i++) { Op *o = s->ops.v[i]; if (o->type == OP_PRISM && kind_is(o->kind, "rock") && o->n[0].v <= H) for (j = 0; j < o->polys.n; j++) LP(rocks, o->polys.v[j]); }
    if (!rocks->n) return;
    big = g_max_area(rocks); bcx = cx_(big); bcy = cy_(big); ext = g_ext(big);
    n = (int)bs_randint(rng, 16, 22);
    for (k = 0; k < n; k++) {
        double a = py_atan2(-bcy, -bcx) + UNI(-0.9, 0.9), d = UNI(0.6, 4.0); Geo *pt = g_pt(bcx + 20 * COS(a), bcy + 20 * SIN(a)), *b, *g; Num z;
        b = LA(shp_interpolate(ext->g, shp_project(ext->g, pt->g, 0), 0));
        { StoneAt m = { rng, g_x(b) + d * COS(a), g_y(b) + d * SIN(a), 0.5, 0.8, .7, 5 }; g = bs_find(s, mk_stone_at, &m, 20, 0.12, NULL, 0, NULL, &z); }
        if (g) S_stack(s, L1(g), num_f(0.5), TS2(t, 0.7, 6, 0.3, 40), 2, "rock", num_none(), NULL);
    }
}

/* ================================================================ HIGHLAND */
/* h_bog: reeds: lambda: star(p.x + rng.uniform(-.9, .9), p.y + rng.uniform(-.9, .9), 1.25, 0.55, 5, rng.uniform(0, 1)) */
typedef struct { PyRandom *rng; double x, y; } ReedMk;
static Geo *mk_reed(void *c) { ReedMk *m = (ReedMk *)c; double x = m->x + py_uniform(m->rng, -.9, .9), y = m->y + py_uniform(m->rng, -.9, .9), rot = py_uniform(m->rng, 0, 1); return star_(x, y, 1.25, 0.55, 5, rot); }
static void h_bog_(Scene *s, PyRandom *rng, int lvl) {
    BlobMk m = { rng, 0, 9, 2.4, 3.2, 4 }; Num z; Geo *g = bs_find(s, mk_blob, &m, 120, 1.2, NULL, 1, NULL, &z), *ext; int k, n; Tier t[2]; (void)lvl;
    if (!g) return;
    S_gcut(s, L1(g), num_f(0.6), "water");
    n = (int)bs_randint(rng, 4, 7);
    for (k = 0; k < n; k++) {
        Geo *p, *r_; Num z_; ReedMk rm;
        ext = g_ext(g); p = g_interp(ext, UNI(0, g_len(ext)));
        rm.rng = rng; rm.x = g_x(p); rm.y = g_y(p);
        r_ = bs_find(s, mk_reed, &rm, 15, 0.15, NULL, 0, NULL, &z_);
        if (r_) bs_mound(s, L1(r_), z_, num_f(1.6), TS2(t, 0.6, 15, 0.4, 40), 2, "reed");
    }
}
/* h_steps: lambda: stone(p.x, p.y, rng.uniform(1.0, 1.3), rng, .8, 9) */
static void h_steps_(Scene *s, PyRandom *rng, int lvl) {
    double a = UNI(0, PI), off = UNI(-4, 4), *us; Geo *ln = meander_(rng, a, off, 1.2, 1, 40); int i, n; (void)lvl;
    { double u0 = UNI(0, 3); n = ly_arange(u0, g_len(ln), 3.0, &us); }
    for (i = 0; i < n; i++) {
        Geo *p = g_interp(ln, us[i]), *g; Num z; StoneAt m = { rng, g_x(p), g_y(p), 1.0, 1.3, .8, 9 };
        g = bs_find(s, mk_stone_at, &m, 3, 0.3, NULL, 0, NULL, &z);
        if (g) bs_up(s, L1(g), z, num_f(0.5), "slate");
    }
}
/* h_wall: lambda: stone(d * math.cos(a), d * math.sin(a), rng.uniform(1.0, 1.25), rng, .7, 6, round_=False) */
static void h_wall_(Scene *s, PyRandom *rng, int lvl) {
    double a0 = UNI(0, 2 * PI), *ts; int n = 0, i, nt; (void)lvl;
    { double e = UNI(1.0, 1.5); nt = ly_arange(0, e, 0.15, &ts); }
    for (i = 0; i < nt; i++) {
        double a = a0 + ts[i], d = 12.2 + UNI(-.3, .3); Geo *g; Num z; StoneAt m = { rng, d * COS(a), d * SIN(a), 1.0, 1.25, .7, 6 };
        g = bs_find(s, mk_stone_at, &m, 4, 0.12, NULL, 0, NULL, &z);
        if (!g) continue;
        bs_up(s, L1(g), z, num_f(n % 2 ? 1.2 : 1.8), "slate"); n++;
    }
}
/* h_moss: lambda: blob(*rp(rng, 0, 12), rng.uniform(1.4, 2.4), rng, 5) */
static void h_moss_(Scene *s, PyRandom *rng, int lvl) {
    int k, n = (int)bs_randint(rng, 2, 4); (void)lvl;
    for (k = 0; k < n; k++) {
        BlobMk m = { rng, 0, 12, 1.4, 2.4, 5 }; Num z; Geo *g = bs_find(s, mk_blob, &m, 120, 0.3, NULL, 0, NULL, &z);
        if (g) bs_up(s, L1(g), z, num_f(0.25), "moss");
    }
}
/* h_heather: cluster(s, rng, 8, lambda x, y: Point(x, y).buffer(0.48, 8), cx, cy, 1.4, 0.15) */
typedef struct { Scene *s; PyRandom *rng; int n; double spread, gap, r; int q; double r0, r1; } DotSa;
static Geo *one_dot(void *c, double x, double y) { DotSa *m = (DotSa *)c; return g_ptbuf(x, y, m->r, m->q); }
static void sa_dots(void *c, double cx, double cy, GeoList *out) { DotSa *m = (DotSa *)c; bs_cluster(m->s, m->rng, m->n, one_dot, m, cx, cy, m->spread, m->gap, out); }
static void h_heather_(Scene *s, PyRandom *rng, int lvl) {
    int k, n = (int)bs_randint(rng, 2, 3); Tier t[1] = { TT(1.0, 25) }; (void)lvl;
    for (k = 0; k < n; k++) {
        DotSa m = { s, rng, 8, 1.4, 0.15, 0.48, 8, 0, 0 }; Num z; GeoList *ps = bs_place_group(s, rng, sa_dots, &m, 120, 0.3, 0, NULL, &z);
        if (ps && ps->n) bs_mound(s, ps, z, num_f(0.7), t, 1, "heather");
    }
}
/* h_standing: lambda: rect(*rp(rng, 3, 10), rng.uniform(3.4, 4.2), rng.uniform(2.6, 3.0), rng.uniform(0, math.pi)).buffer(0.3).buffer(-0.3) */
static Geo *mk_standing(void *c) {
    PyRandom *rng = (PyRandom *)c; double x, y, l, w, a; bs_rp(rng, 3, 10, &x, &y); l = UNI(3.4, 4.2); w = UNI(2.6, 3.0); a = UNI(0, PI);
    return g_buf(g_buf(rect_(x, y, l, w, a), 0.3), -0.3);
}
static void h_standing_(Scene *s, PyRandom *rng, int lvl) {
    Num z, top; Geo *g = bs_find(s, mk_standing, rng, 300, 0.5, NULL, 1, NULL, &z); (void)lvl;
    if (!g) return;
    top = bs_up(s, L1(g), z, num_f(2.9), "standing");
    if (bs_truthy(top)) { double x = cx_(g), y = cy_(g); bs_topcut(s, L2(ring_(x, y, 1.05, 0.7, 32), g_ptbuf(x, y, 0.35, 10)), top, num_f(0.3), "crack"); }
}
/* h_stream: lambda: Point(*ln.interpolate(rng.uniform(0, ln.length)).coords[0]).buffer(0.5, 8) */
typedef struct { PyRandom *rng; Geo *ln; } LnMk;
static Geo *mk_pebble(void *c) { LnMk *m = (LnMk *)c; Geo *p = g_interp(m->ln, py_uniform(m->rng, 0, g_len(m->ln))); return g_ptbuf(g_x(p), g_y(p), 0.5, 8); }
static void h_stream_(Scene *s, PyRandom *rng, int lvl) {
    double phi = UNI(0, PI), off = UNI(-7, 7); Geo *ln = meander_(rng, phi, off, 1.4, 1.4, 40), *band; int k; Tier t[2]; (void)lvl;
    S_gcut(s, L1(g_bufx(ln, 0.75, 16, SHP_CAP_FLAT, SHP_JOIN_ROUND, 5.0)), num_f(0.6), "water");
    band = g_inter(g_buf(ln, 0.9), DISC);
    for (k = 0; k < 12; k++) {
        LnMk m = { rng, ln }; Num z; Geo *g = bs_find(s, mk_pebble, &m, 10, 0.15, band, 0, "water", &z);
        if (g) S_stack(s, L1(g), num_f(0.35), TS2(t, 0.7, 10, 0.3, 45), 2, "pebble", num_none(), NULL);
    }
}
/* h_fern: mk() */
static Geo *mk_fern(void *c) {
    PyRandom *rng = (PyRandom *)c; double cx, cy, a, L, *us; Frame f; GeoList *parts = LL(); int i, n; double x0, y0, x1, y1;
    bs_rp(rng, 1, 11, &cx, &cy); a = UNI(0, 2 * PI); f = fr_(cx, cy, a); L = UNI(3.5, 4.5);
    at_(&f, 0, 0, &x0, &y0); at_(&f, L, 0, &x1, &y1);
    { double xy[4] = { x0, y0, x1, y1 }; LP(parts, g_linebuf(xy, 2, 0.45, SHP_CAP_ROUND, 16)); }
    n = ly_linspace(0.6, L - 0.5, 5, &us);
    for (i = 0; i < n; i++) {
        double u = us[i], w = 1.4 * (1 - u / L) + 0.5; int sg;
        for (sg = -1; sg <= 1; sg += 2) { double xy[4]; at_(&f, u, 0, &xy[0], &xy[1]); at_(&f, u + 0.6, sg * w, &xy[2], &xy[3]); LP(parts, g_linebuf(xy, 2, 0.45, SHP_CAP_ROUND, 16)); }
    }
    return g_uu(parts);
}
static void h_fern_(Scene *s, PyRandom *rng, int lvl) {
    int k, n = (int)bs_randint(rng, 2, 3); (void)lvl;
    for (k = 0; k < n; k++) { Num z; Geo *g = bs_find(s, mk_fern, rng, 120, 0.3, NULL, 0, NULL, &z); if (g) bs_up(s, L1(g), z, num_f(0.55), "fern"); }
}

/* ================================================================ DESERTS OF MAAHL */
/* m_ruin: at(cx, cy) */
static void sa_ruin(void *c, double cx, double cy, GeoList *out) {
    PyRandom *rng = (PyRandom *)c; double a = UNI(0, PI); Frame f = fr_(cx, cy, a); int i, n = (int)bs_randint(rng, 3, 4);
    for (i = 0; i < n; i++) { double u = i * 2.6 + UNI(-.3, .3), v = UNI(-.4, .4), b = a + UNI(-.15, .15); LP(out, rectf_(&f, u, v, 2.3, 2.0, b)); }
}
static void m_ruin_(Scene *s, PyRandom *rng, int lvl) {
    Num z; GeoList *ps = bs_place_group(s, rng, sa_ruin, rng, 120, 0.5, 0, NULL, &z); int i; (void)lvl;
    if (!ps || !ps->n) return;
    for (i = 0; i < ps->n; i++) {
        Geo *p = ps->v[i]; Num t = bs_up(s, L1(p), z, num_f(UNI(0.6, 1.2)), "ruin");
        if (bs_truthy(t)) { double x = cx_(p), y = cy_(p), a = UNI(0, PI); bs_topcut(s, L1(rect_(x, y, 1.6, 0.35, a)), t, num_f(0.25), "glyph"); }
    }
}
/* m_shrub */
static void m_shrub_(Scene *s, PyRandom *rng, int lvl) {
    int k, n = (int)bs_randint(rng, 2, 4); Tier t[2]; (void)lvl;
    for (k = 0; k < n; k++) {
        double ro = UNI(1.0, 1.5); StarMk m = { rng, 0, 12, ro, ro * 0.55, 9 }; Num z; Geo *g = bs_find(s, mk_star, &m, 120, 0.4, NULL, 0, NULL, &z);
        if (g) bs_mound(s, L1(g), z, num_f(0.9), TS2(t, 0.6, 20, 0.4, 45), 2, "shrub");
    }
}
/* m_sinkhole */
static void m_sinkhole_(Scene *s, PyRandom *rng, int lvl) {
    int it, k; (void)lvl;
    for (it = 0; it < 300; it++) {
        double cx, cy, r; Geo *g; Num zf;
        bs_rp(rng, 0, 10, &cx, &cy); r = UNI(1.5, 2.0); g = g_ptbuf(cx, cy, r + 1.4, 32);
        if (!g_contains(DISC, g)) continue;
        if (g_intersects(g_uu(bs_hard(s)), g)) continue;
        zf = S_tflat(s, g);
        if (zf.t == NUM_NONE || zf.v != H) continue;
        bs_evict(s, g);
        { static const double D[3] = { 0.3, 0.6, 0.9 }; for (k = 0; k < 3; k++) S_gcut(s, L1(g_ptbuf(cx, cy, r - k * 0.6, 32)), num_f(D[k]), k < 2 ? "hollow" : "pit"); }
        S_gcut(s, L1(radial_(cx, cy, r, r + 1.3, 6, rng, 0.22)), num_f(0.35), "crack"); gl_push(&s->keepout, g); return;
    }
}
/* m_crystal */
static void sa_crystal(void *c, double cx, double cy, GeoList *out) {
    PyRandom *rng = (PyRandom *)c; double a0 = UNI(0, 2 * PI); GeoList *l = LL(), *ps; int k, n = (int)bs_randint(rng, 3, 5);
    for (k = 0; k < n; k++) {
        double a = a0 + UNI(-0.8, 0.8), L = UNI(1.8, 3.0); Frame f = fr_(cx, cy, a); double uv[10] = { 0.3, -0.5, L, -0.5, L + 0.6, 0, L, 0.5, 0.3, 0.5 };
        LP(l, frpoly_(&f, uv, 5));
    }
    ps = g_polys(g_uu(l));
    for (k = 0; k < ps->n; k++) LP(out, ps->v[k]);
}
static void m_crystal_(Scene *s, PyRandom *rng, int lvl) {
    Num z; GeoList *ps = bs_place_group(s, rng, sa_crystal, rng, 120, 0.4, 0, NULL, &z); (void)lvl;
    if (ps && ps->n) bs_up(s, ps, z, num_f(UNI(1.6, 2.2)), "crystal");
}
/* m_salt */
static void m_salt_(Scene *s, PyRandom *rng, int lvl) {
    int it; (void)lvl;
    for (it = 0; it < 60; it++) {
        double x, y, r; Geo *reg; Num zf; GeoList *top; Num *tops;
        bs_rp(rng, 0, 9, &x, &y); r = UNI(2.8, 3.6); reg = blob_(x, y, r, rng, 4);
        if (!g_contains(DISC, reg)) continue;
        if (g_intersects(g_buf(g_uu(bs_hard(s)), 0.4), reg)) continue;
        zf = S_tflat(s, reg);
        if (zf.t == NUM_NONE) continue;
        bs_evict(s, reg); (void)S_tflat(s, reg);
        top = S_tprism(s, L1(reg), num_f(0.25), "salt", &tops);
        if (top->n) {
            Geo *net = ly_vor_net(rng, -17, 17, 520, -20, 20, 0, 0, 0.2, 3);
            bs_topcut(s, L1(g_inter(net, g_inset(reg, 0.3))), tops[0], num_f(0.2), "crack");
        }
        return;
    }
}
/* m_obelisk: mk() */
static Geo *mk_obelisk(void *c) {
    PyRandom *rng = (PyRandom *)c; double x, y, a; Frame f; double uv[10] = { -5, -1.0, 4.0, -1.0, 5, 0, 4.0, 1.0, -5, 1.0 };
    bs_rp(rng, 0, 6, &x, &y); a = UNI(0, PI); f = fr_(x, y, a); return frpoly_(&f, uv, 5);
}
static void m_obelisk_(Scene *s, PyRandom *rng, int lvl) {
    Num z, *tops; Geo *g = bs_find(s, mk_obelisk, rng, 200, 0.5, NULL, 0, NULL, &z); GeoList *got; (void)lvl;
    if (!g) return;
    got = S_tprism(s, L1(g), num_f(1.3), "obelisk", &tops);
    if (got->n) {
        double *c, dx, dy, a, *us; Frame f; GeoList *glyphs = LL(), *keep = LL(); int i, n; Geo *in;
        mrr_coords(g, &c); dx = c[2] - c[0]; dy = c[3] - c[1];
        a = py_hypot(dx, dy) > 3 ? py_atan2(dy, dx) : py_atan2(c[5] - c[3], c[4] - c[2]);
        f = fr_(cx_(g), cy_(g), a);
        n = ly_arange(-3.6, 3.4, 0.95, &us);
        for (i = 0; i < n; i++) LP(glyphs, rectf_(&f, us[i], 0, 0.5, i % 3 ? 0.9 : 0.5, a));
        for (i = 0; i < glyphs->n; i++) { in = g_inset(g, 0.3); if (g_contains(in, glyphs->v[i])) LP(keep, glyphs->v[i]); }
        bs_topcut(s, keep, tops[0], num_f(0.3), "glyph");
    }
}
/* m_column: lambda: Point(*rp(rng, 2, 11), ).buffer(r, 48) */
typedef struct { PyRandom *rng; double d0, d1, r; int q; } PtMk;
static Geo *mk_ptbuf(void *c) { PtMk *m = (PtMk *)c; double x, y; bs_rp(m->rng, m->d0, m->d1, &x, &y); return g_ptbuf(x, y, m->r, m->q); }
static void m_column_(Scene *s, PyRandom *rng, int lvl) {
    double r = UNI(1.8, 2.1); PtMk m = { rng, 2, 11, r, 48 }; Num z, *tops; Geo *g = bs_find(s, mk_ptbuf, &m, 120, 0.5, NULL, 0, NULL, &z); GeoList *got; (void)lvl;
    if (!g) return;
    got = S_tprism(s, L1(g), num_f(1.8), "obelisk", &tops);
    if (got->n) {
        double x = cx_(g), y = cy_(g); Num zt = tops[0]; GeoList *fl = LL(); int k;
        for (k = 0; k < 12; k++) LP(fl, g_ptbuf(x + r * COS(k * PI / 6), y + r * SIN(k * PI / 6), 0.32, 8));
        bs_topcut(s, L1(g_inter(g_uu(fl), g)), zt, num_f(0.6), "glyph");
        bs_topcut(s, L1(ring_(x, y, 1.0, 0.75, 32)), zt, num_f(0.25), "crack");
    }
}
/* m_pavers */
static void sa_pavers(void *c, double cx, double cy, GeoList *out) {
    PyRandom *rng = (PyRandom *)c; double a = UNI(0, PI); Frame f = fr_(cx, cy, a); int i, j;
    for (i = -1; i < 2; i++) for (j = -1; j < 2; j++) if (RND() > 0.3) LP(out, rectf_(&f, i * 2.3, j * 2.3, 1.95, 1.95, a));
}
static void m_pavers_(Scene *s, PyRandom *rng, int lvl) {
    Num z; GeoList *ps = bs_place_group(s, rng, sa_pavers, rng, 120, 0.4, 0, NULL, &z); int i; (void)lvl;
    if (ps && ps->n) for (i = 0; i < ps->n; i++) bs_up(s, L1(ps->v[i]), z, num_f(UNI(0.25, 0.45)), "ruin");
}

/* ================================================================ CORRIDOR */
/* c_vent: mk(): rect(cx, cy, 4.4, 3.2, getattr(s, "phi", rng.choice([0, math.pi / 2]))) */
typedef struct { Scene *s; PyRandom *rng; } SRMk;
static Geo *mk_vent(void *c) {
    SRMk *m = (SRMk *)c; PyRandom *rng = m->rng; double x, y, a;
    bs_rp(rng, 0, 9, &x, &y);
    a = py_choice(rng, 2) ? PI / 2 : 0;          /* getattr's default is drawn even when s.phi is there */
    if (m->s->has_phi) a = m->s->phi.v;
    return rect_(x, y, 4.4, 3.2, a);
}
static void c_vent_(Scene *s, PyRandom *rng, int lvl) {
    int k, n = 2 + (lvl >= 3);
    for (k = 0; k < n; k++) {
        SRMk m = { s, rng }; Num z, top; Geo *g = bs_find(s, mk_vent, &m, 120, 0.4, NULL, 0, NULL, &z);
        if (!g) continue;
        top = bs_up(s, L1(g), z, num_f(0.3), "metal");
        if (bs_truthy(top)) {
            double x = cx_(g), y = cy_(g), *mr, a, *vs; Frame f; GeoList *l = LL(); int i, nv;
            mrr_coords(g, &mr); a = py_atan2(mr[3] - mr[1], mr[2] - mr[0]);
            f = fr_(x, y, a);
            nv = ly_arange(-1.0, 1.01, 0.66, &vs);
            for (i = 0; i < nv; i++) LP(l, rectf_(&f, 0, vs[i], 3.4, 0.4, a));
            bs_topcut(s, l, top, num_f(0.5), "slot");
        }
    }
}
/* c_chevron */
static void sa_chevron(void *c, double cx, double cy, GeoList *out) {
    PyRandom *rng = (PyRandom *)c; double a = UNI(0, PI), *us; Frame f = fr_(cx, cy, a); int i, n = ly_arange(-3.6, 2.6, 2.4, &us);
    for (i = 0; i < n; i++) { double u = us[i]; double uv[12] = { u, -1.1, u + 1.45, -1.1, u + 2.55, 0, u + 1.45, 1.1, u, 1.1, u + 1.1, 0 }; LP(out, frpoly_(&f, uv, 6)); }
}
static void c_chevron_(Scene *s, PyRandom *rng, int lvl) {
    Num z; GeoList *ps = bs_place_group(s, rng, sa_chevron, rng, 300, 0.25, 0, NULL, &z); (void)lvl;
    if (ps && ps->n) bs_up(s, ps, z, num_f(0.25), "paint");
}
/* c_cable */
static void c_cable_(Scene *s, PyRandom *rng, int lvl) {
    int k, n = (int)bs_randint(rng, 1, 2); (void)lvl;
    for (k = 0; k < n; k++) {
        double phi = UNI(0, PI), off = UNI(-8, 8), amp = UNI(0.8, 2.0); Geo *ln = meander_(rng, phi, off, amp, 1.0, 30);
        bs_drape(s, g_bufx(ln, 0.5, 16, SHP_CAP_ROUND, SHP_JOIN_ROUND, 5.0), num_f(0.7), "cable");
    }
}
/* c_panel: lambda: rect(*rp(rng, 0, 9), 4.6, 4.6, rng.choice([0, 0.78, getattr(s, 'phi', 0)])) */
static Geo *mk_panel(void *c) {
    SRMk *m = (SRMk *)c; PyRandom *rng = m->rng; double x, y, a; int64_t i;
    bs_rp(rng, 0, 9, &x, &y); i = py_choice(rng, 3);
    a = i == 0 ? 0 : i == 1 ? 0.78 : (m->s->has_phi ? m->s->phi.v : 0);
    return rect_(x, y, 4.6, 4.6, a);
}
static void c_panel_(Scene *s, PyRandom *rng, int lvl) {
    SRMk m = { s, rng }; Num z, top; Geo *g = bs_find(s, mk_panel, &m, 120, 0.4, NULL, 0, NULL, &z); (void)lvl;
    if (!g) return;
    top = bs_up(s, L1(g), z, num_f(0.35), "metal");
    if (bs_truthy(top)) {
        double *xy; int i, n; GeoList *l = LL();
        bs_topcut(s, L1(g_diff(g_inset(g, 0.5), g_inset(g, 0.85))), top, num_f(0.25), "slot");
        n = g_coords(g_ext(g_inset(g, 0.25)), &xy);
        for (i = 0; i < n && i < 4; i++) LP(l, g_ptbuf(xy[2 * i], xy[2 * i + 1], 0.3, 8));
        bs_topcut(s, l, top, num_f(0.4), "slot");
    }
}
/* c_spill */
static void c_spill_(Scene *s, PyRandom *rng, int lvl) {
    BlobMk m = { rng, 0, 10, 1.6, 2.4, 5 }; Num z; Geo *g = bs_find(s, mk_blob, &m, 120, 0.3, NULL, 0, NULL, &z); (void)lvl;
    if (g) bs_cut(s, L1(g), z, num_f(0.3), "coolant");
}
/* c_hexdeck: lambda: Point(*rp(rng, 0, 9)).buffer(rng.uniform(2.6, 3.2), 32) */
static Geo *mk_hexdeck(void *c) { PyRandom *rng = (PyRandom *)c; double x, y, r; bs_rp(rng, 0, 9, &x, &y); r = UNI(2.6, 3.2); return g_ptbuf(x, y, r, 32); }
static void c_hexdeck_(Scene *s, PyRandom *rng, int lvl) {
    Num z; Geo *g = bs_find(s, mk_hexdeck, rng, 300, 0.2, NULL, 0, NULL, &z), *net; double x, y, r = 1.2; GeoList *cells = LL(), *rims = LL(); int i, j; (void)lvl;
    if (!g) return;
    x = cx_(g); y = cy_(g);
    for (i = -4; i < 5; i++) for (j = -4; j < 5; j++) LP(cells, hexa_(x + i * r * 1.5, y + (j + 0.5 * ly_mod(i, 2)) * r * sqrt(3), r));
    for (i = 0; i < cells->n; i++) LP(rims, g_buf(g_ext(cells->v[i]), 0.18));
    net = g_inter(g_uu(rims), g_inset(g, 0.3));
    bs_cut(s, L1(net), z, num_f(0.3), "slot");
    bs_cut(s, L1(g_buf(g_ext(g), 0.25)), z, num_f(0.3), "slot");
}
/* c_scorch: lambda: Point(*rp(rng, 0, 9)).buffer(2.3, 24) */
static void c_scorch_(Scene *s, PyRandom *rng, int lvl) {
    PtMk m = { rng, 0, 9, 2.3, 24 }; Num z; Geo *g = bs_find(s, mk_ptbuf, &m, 120, 0.2, NULL, 0, NULL, &z); double x, y; (void)lvl;
    if (!g) return;
    x = cx_(g); y = cy_(g); bs_cut(s, L1(blob_(x, y, 1.4, rng, 4)), z, num_f(0.25), "scorch");
    bs_cut(s, L1(radial_(x, y, 1.2, 2.3, 7, rng, 0.2)), z, num_f(0.3), "crack");
}
/* c_lights: a = getattr(s, "phi", rng.uniform(0, math.pi)) + rng.choice([0, math.pi / 2]) */
static void sa_lights(void *c, double cx, double cy, GeoList *out) {
    SRMk *m = (SRMk *)c; PyRandom *rng = m->rng; double a, d; Frame f; static const double U3[3] = { -3.6, 0, 3.6 }; int i;
    a = m->s->has_phi ? m->s->phi.v : UNI(0, PI);
    if (m->s->has_phi) (void)UNI(0, PI);          /* the default is drawn even when s.phi is there */
    d = py_choice(rng, 2) ? PI / 2 : 0; a = a + d; f = fr_(cx, cy, a);
    for (i = 0; i < 3; i++) LP(out, rectf_(&f, U3[i], 0, 3.0, 0.95, a));
}
static void c_lights_(Scene *s, PyRandom *rng, int lvl) {
    SRMk m = { s, rng }; Num z; GeoList *ps = bs_place_group(s, rng, sa_lights, &m, 120, 0.35, 0, NULL, &z); (void)lvl;
    if (ps && ps->n) bs_up(s, ps, z, num_f(0.3), "lens");
}

/* ================================================================ INFESTATION */
/* i_eggs: cluster(s, rng, rng.randint(4, 6), lambda x, y: affinity.rotate(affinity.scale(Point(x, y).buffer(1.1, 16), 1.0, 1.4), rng.uniform(0, 180)), cx, cy, 2.0, 0.2) */
typedef struct { Scene *s; PyRandom *rng; } EggSa;
static Geo *one_egg(void *c, double x, double y) { EggSa *m = (EggSa *)c; Geo *e = g_scale(g_ptbuf(x, y, 1.1, 16), 1.0, 1.4, SHP_ORIGIN_CENTER, 0, 0); double d = py_uniform(m->rng, 0, 180); return g_rot(e, d, SHP_ORIGIN_CENTER, 0, 0); }
static void sa_eggs(void *c, double cx, double cy, GeoList *out) { EggSa *m = (EggSa *)c; int n = (int)bs_randint(m->rng, 4, 6); bs_cluster(m->s, m->rng, n, one_egg, m, cx, cy, 2.0, 0.2, out); }
static void i_eggs_(Scene *s, PyRandom *rng, int lvl) {
    int k, n = 1 + (lvl >= 3); Tier t[2];
    for (k = 0; k < n; k++) {
        EggSa m = { s, rng }; Num z; GeoList *ps = bs_place_group(s, rng, sa_eggs, &m, 200, 0.3, 0, NULL, &z);
        if (ps && ps->n) bs_mound(s, ps, z, num_f(1.5), TS2(t, 0.6, 10, 0.4, 35), 2, "egg");
    }
}
/* i_acid */
static void i_acid_(Scene *s, PyRandom *rng, int lvl) {
    int k, n = (int)bs_randint(rng, 1, 2); (void)lvl;
    for (k = 0; k < n; k++) {
        BlobMk m = { rng, 0, 10, 1.5, 2.2, 4 }; Num z; Geo *g = bs_find(s, mk_blob, &m, 120, 0.4, NULL, 0, NULL, &z);
        if (!g) continue;
        if (bs_truthy(bs_up(s, L1(g_diff(g_buf(g, 0.95), g)), z, num_f(0.3), "acidrim"))) bs_cut(s, L1(g), z, num_f(0.35), "acid");
    }
}
/* i_veins */
static void i_veins_(Scene *s, PyRandom *rng, int lvl) {
    GeoList *mass = LL(), *lines = LL(), *bufs = LL(); int i, j, k, n; double ccx, ccy; (void)lvl;
    for (i = 0; i < s->ops.n; i++) { Op *o = s->ops.v[i]; if (kind_is(o->kind, "flesh") && o->type == OP_TAPER) for (j = 0; j < o->polys.n; j++) LP(mass, o->polys.v[j]); }
    if (mass->n) { Geo *c = g_centroid(g_uu(mass)); ccx = g_x(c); ccy = g_y(c); } else { ccx = 0; ccy = 0; }
    n = (int)bs_randint(rng, 3, 4);
    for (k = 0; k < n; k++) {
        double a = py_atan2(-ccy, -ccx) + UNI(-1.4, 1.4), x = ccx, y = ccy, pts[22]; int np = 0;
        pts[np++] = x; pts[np++] = y;
        for (i = 0; i < 10; i++) {
            a += UNI(-.35, .35); x += 1.6 * COS(a); y += 1.6 * SIN(a); pts[np++] = x; pts[np++] = y;
            if (i == 5 && RND() < 0.7) { double b = a + (py_choice(rng, 2) ? 1.0 : -1.0) * 0.8; LP(lines, seg_(x, y, x + 4 * COS(b), y + 4 * SIN(b))); }
        }
        LP(lines, g_line(pts, np / 2));
    }
    for (i = 0; i < lines->n; i++) LP(bufs, g_bufx(lines->v[i], 0.46, 16, SHP_CAP_ROUND, SHP_JOIN_ROUND, 5.0));
    bs_drape(s, g_uu(bufs), num_f(0.35), "vein");
}
/* i_spores: cluster(s, rng, rng.randint(3, 5), lambda x, y: Point(x, y).buffer(rng.uniform(0.55, 0.95), 14), cx, cy, 1.2, 0.15) */
static Geo *one_spore(void *c, double x, double y) { EggSa *m = (EggSa *)c; double r = py_uniform(m->rng, 0.55, 0.95); return g_ptbuf(x, y, r, 14); }
static void sa_spores(void *c, double cx, double cy, GeoList *out) { EggSa *m = (EggSa *)c; int n = (int)bs_randint(m->rng, 3, 5); bs_cluster(m->s, m->rng, n, one_spore, m, cx, cy, 1.2, 0.15, out); }
static void i_spores_(Scene *s, PyRandom *rng, int lvl) {
    int k, n = (int)bs_randint(rng, 2, 3); Tier t[2]; (void)lvl;
    for (k = 0; k < n; k++) {
        EggSa m = { s, rng }; Num z; GeoList *ps = bs_place_group(s, rng, sa_spores, &m, 120, 0.3, 0, NULL, &z);
        if (ps && ps->n) bs_mound(s, ps, z, num_f(1.1), TS2(t, 0.5, 15, 0.5, 50), 2, "spore");
    }
}
/* i_hatched: mk() */
static Geo *mk_hatched(void *c) { PyRandom *rng = (PyRandom *)c; double x, y, rot; bs_rp(rng, 1, 11, &x, &y); { Geo *d = g_ptbuf(x, y, 1.7, 20); rot = UNI(0, 1); return g_diff(d, star_(x, y, 1.15, 0.6, 7, rot)); } }
static void i_hatched_(Scene *s, PyRandom *rng, int lvl) {
    int k, n = (int)bs_randint(rng, 2, 3); (void)lvl;
    for (k = 0; k < n; k++) { Num z; Geo *g = bs_find(s, mk_hatched, rng, 120, 0.35, NULL, 0, NULL, &z); if (g) bs_up(s, L1(g), z, num_f(1.3), "shell"); }
}
/* i_torn */
static void i_torn_(Scene *s, PyRandom *rng, int lvl) {
    int i, j, k, np = 0; Geo **pp; Num *pz; int64_t *ix; (void)lvl;
    for (i = 0; i < s->ops.n; i++) { Op *o = s->ops.v[i]; if (o->type == OP_PRISM && kind_is(o->kind, "plate")) for (j = 0; j < o->polys.n; j++) if (g_area(o->polys.v[j]) > 12) np++; }
    pp = (Geo **)LM(sizeof(Geo *) * (size_t)(np + 1)); pz = (Num *)LM(sizeof(Num) * (size_t)(np + 1)); ix = (int64_t *)LM(sizeof(int64_t) * (size_t)(np + 1)); np = 0;
    for (i = 0; i < s->ops.n; i++) { Op *o = s->ops.v[i]; if (o->type == OP_PRISM && kind_is(o->kind, "plate")) for (j = 0; j < o->polys.n; j++) if (g_area(o->polys.v[j]) > 12) { pp[np] = o->polys.v[j]; pz[np] = o->n[1]; ix[np] = np; np++; } }
    py_shuffle_idx(rng, ix, np);
    for (k = 0; k < np && k < 3; k++) {
        Geo *p = pp[ix[k]], *crk, *sh; Num zt = pz[ix[k]]; int hit = 0; double ccx, ccy, a; GeoList *shard;
        for (i = 0; !hit && i < s->tcuts.n; i++) for (j = 0; !hit && j < s->tcuts.v[i].polys.n; j++) if (g_intersects(p, s->tcuts.v[i].polys.v[j])) hit = 1;
        for (i = 0; !hit && i < s->ops.n; i++) {
            Op *o = s->ops.v[i];
            if (kind_is(o->kind, "plate") || in_soft(o->kind)) continue;
            for (j = 0; !hit && j < o->polys.n; j++) if (g_intersects(p, o->polys.v[j])) hit = 1;
        }
        if (hit) continue;
        ccx = cx_(p); ccy = cy_(p); a = UNI(0, PI);
        crk = g_buf(wigl_(ccx - 4 * COS(a), ccy - 4 * SIN(a), ccx + 4 * COS(a), ccy + 4 * SIN(a), rng, 0.4), 0.25);
        shard = g_polys(g_diff(g_inset(p, 0.4), g_buf(crk, 0.35)));
        if (shard->n < 2) continue;
        sh = shard->v[0]; { double ba = g_area(sh); for (i = 1; i < shard->n; i++) { double a2 = g_area(shard->v[i]); if (a2 < ba) { ba = a2; sh = shard->v[i]; } } }
        bs_topcut(s, L1(g_inter(crk, g_inset(p, 0.05))), zt, num_f(0.45), "slot");
        S_prism(s, L1(g_inset(sh, 0.15)), num_add(zt, num_f(0.4)), "plate", num_sub(zt, num_f(0.15)), SC.MINW, 0); return;
    }
}
/* i_ribs */
static void sa_ribs(void *c, double cx, double cy, GeoList *out) {
    PyRandom *rng = (PyRandom *)c; double a0 = UNI(0, 2 * PI), R_ = UNI(4, 5.5), ox = cx - R_ * COS(a0), oy = cy - R_ * SIN(a0), *ts; int i, n = ly_linspace(-0.45, 0.45, 5, &ts);
    for (i = 0; i < n; i++) { double t = ts[i]; LP(out, rect_(ox + R_ * COS(a0 + t), oy + R_ * SIN(a0 + t), 3.2, 0.95, a0 + t)); }
}
static void i_ribs_(Scene *s, PyRandom *rng, int lvl) {
    Num z; GeoList *ps = bs_place_group(s, rng, sa_ribs, rng, 120, 0.4, 0, NULL, &z); Tier t[2]; (void)lvl;
    if (ps && ps->n) bs_mound(s, ps, z, num_f(1.6), TS2(t, 0.6, 6, 0.4, 30), 2, "carapace");
}

/* ================================================================ URBAN */
/* u_bricks: cluster(s, rng, rng.randint(6, 10), lambda x, y: rect(x, y, 1.8, 0.95, rng.uniform(0, math.pi)), cx, cy, 1.8, 0.12) */
static Geo *one_brick(void *c, double x, double y) { EggSa *m = (EggSa *)c; double a = py_uniform(m->rng, 0, PI); return rect_(x, y, 1.8, 0.95, a); }
static void sa_bricks(void *c, double cx, double cy, GeoList *out) { EggSa *m = (EggSa *)c; int n = (int)bs_randint(m->rng, 6, 10); bs_cluster(m->s, m->rng, n, one_brick, m, cx, cy, 1.8, 0.12, out); }
static void u_bricks_(Scene *s, PyRandom *rng, int lvl) {
    int k, i, n = (int)bs_randint(rng, 1, 2) + (lvl >= 3);
    for (k = 0; k < n; k++) {
        EggSa m = { s, rng }; Num z; GeoList *ps = bs_place_group(s, rng, sa_bricks, &m, 120, 0.35, 0, NULL, &z);
        if (ps && ps->n) for (i = 0; i < ps->n; i++) bs_up(s, L1(ps->v[i]), z, num_f(UNI(0.45, 0.8)), "brick");
    }
}
/* u_bullets: cluster(s, rng, rng.randint(5, 8), lambda x, y: Point(x, y).buffer(rng.uniform(0.3, 0.45), 8), cx, cy, 1.5, 0.3) */
static Geo *one_bullet(void *c, double x, double y) { EggSa *m = (EggSa *)c; double r = py_uniform(m->rng, 0.3, 0.45); return g_ptbuf(x, y, r, 8); }
static void sa_bullets(void *c, double cx, double cy, GeoList *out) { EggSa *m = (EggSa *)c; int n = (int)bs_randint(m->rng, 5, 8); bs_cluster(m->s, m->rng, n, one_bullet, m, cx, cy, 1.5, 0.3, out); }
static void u_bullets_(Scene *s, PyRandom *rng, int lvl) {
    int k, n = (int)bs_randint(rng, 2, 3); (void)lvl;
    for (k = 0; k < n; k++) {
        EggSa m = { s, rng }; Num z; GeoList *ps = bs_place_group(s, rng, sa_bullets, &m, 120, 0.25, 0, NULL, &z);
        if (ps && ps->n) bs_cut(s, ps, z, num_f(0.4), "pothole");
    }
}
/* u_wall: mk() */
static Geo *mk_uwall(void *c) {
    PyRandom *rng = (PyRandom *)c; double x, y, a, l, w; Geo *r;
    bs_rp(rng, 2, 10, &x, &y); a = UNI(0, PI); l = UNI(5.5, 6.5); w = UNI(3.0, 3.6); r = rect_(x, y, l, w, a);
    return g_inter(r, g_buf(g_stone(x, y, 3.4, rng, .8, 9), 0.8));
}
static void u_wall_(Scene *s, PyRandom *rng, int lvl) {
    Num z, top; Geo *g = bs_find(s, mk_uwall, rng, 120, 0.4, NULL, 0, NULL, &z); double *mr, a, *vs, *us; Frame f; GeoList *cs = LL(); int i, j, nv, nu; (void)lvl;
    if (!g) return;
    top = bs_up(s, L1(g), z, num_f(1.0), "brick");
    if (!bs_truthy(top)) return;
    mrr_coords(g, &mr); a = py_atan2(mr[3] - mr[1], mr[2] - mr[0]); f = fr_(cx_(g), cy_(g), a);
    nv = ly_arange(-2, 2.1, 1.0, &vs);
    for (i = 0; i < nv; i++) LP(cs, rectf_(&f, 0, vs[i], 9, 0.3, a));
    nv = ly_arange(-2, 2, 1.0, &vs);
    for (i = 0; i < nv; i++) {
        double v = vs[i]; nu = ly_arange(-4.5, 4.5, 1.8, &us);
        for (j = 0; j < nu; j++) LP(cs, rectf_(&f, us[j] + (ly_mod(ly_rint(v), 2) ? 0.9 : 0), v + 0.5, 0.3, 0.7, a));
    }
    bs_topcut(s, L1(g_inter(g_uu(cs), g_inset(g, 0.3))), top, num_f(0.3), "slot");
}
/* u_dashes */
static void sa_dashes(void *c, double cx, double cy, GeoList *out) {
    PyRandom *rng = (PyRandom *)c; double a = UNI(0, PI); Frame f = fr_(cx, cy, a); static const double U3[3] = { -4.4, 0, 4.4 }; int i;
    for (i = 0; i < 3; i++) LP(out, rectf_(&f, U3[i], 0, 2.6, 0.95, a));
}
static void u_dashes_(Scene *s, PyRandom *rng, int lvl) {
    Num z; GeoList *ps = bs_place_group(s, rng, sa_dashes, rng, 120, 0.35, 1, NULL, &z); (void)lvl;
    if (ps && ps->n) bs_up(s, ps, z, num_f(0.25), "paint");
}
/* u_scorch */
static void u_scorch_(Scene *s, PyRandom *rng, int lvl) {
    int k, n = (int)bs_randint(rng, 1, 2); (void)lvl;
    for (k = 0; k < n; k++) { BlobMk m = { rng, 0, 10, 1.6, 2.4, 5 }; Num z; Geo *g = bs_find(s, mk_blob, &m, 120, 0.3, NULL, 0, NULL, &z); if (g) bs_cut(s, L1(g), z, num_f(0.2), "scorch"); }
}
/* u_rebar */
static void sa_rebar(void *c, double cx, double cy, GeoList *out) {
    PyRandom *rng = (PyRandom *)c; double a = UNI(0, PI); Frame f = fr_(cx, cy, a); static const double V2[2] = { -0.9, 0.8 }; int i;
    double uv[14] = { -1.8, -1.5, 1.4, -1.5, 1.9, -0.4, 1.2, 0.3, 1.7, 1.1, 1.0, 1.5, -1.8, 1.5 };
    LP(out, frpoly_(&f, uv, 7));
    for (i = 0; i < 2; i++) { double b = a + UNI(-.2, .2); LP(out, rectf_(&f, 1.9, V2[i], 2.2, 0.9, b)); }
}
static void u_rebar_(Scene *s, PyRandom *rng, int lvl) {
    Num z, top; GeoList *ps = bs_place_group(s, rng, sa_rebar, rng, 500, 0.3, 0, NULL, &z); int i; (void)lvl;
    if (!ps || !ps->n) return;
    top = bs_up(s, L1(ps->v[0]), z, num_f(0.8), "concrete");
    if (bs_truthy(top))
        for (i = 1; i < ps->n; i++) S_prism(s, L1(ps->v[i]), num_min2(num_add(top, num_f(0.2)), num_f(CAPZ)), "iron", z.v <= H ? num_f(H - SC.SINK) : num_sub(z, num_f(0.15)), SC.MINW, 1);
}
/* u_tilt: lambda: rect(*rp(rng, 0, 12), 2.9, 2.9, rng.uniform(0, math.pi)) */
static Geo *mk_tilt(void *c) { PyRandom *rng = (PyRandom *)c; double x, y, a; bs_rp(rng, 0, 12, &x, &y); a = UNI(0, PI); return rect_(x, y, 2.9, 2.9, a); }
static void u_tilt_(Scene *s, PyRandom *rng, int lvl) {
    int i, j, k, n, best = -1; Geo *sl = NULL; double ba = 0; (void)lvl;
    for (i = 0; i < s->ops.n; i++) {          /* max(slabs, key=lambda t: t[1].area) */
        Op *o = s->ops.v[i];
        if (o->type != OP_PRISM || !kind_is(o->kind, "slab")) continue;
        for (j = 0; j < o->polys.n; j++) { double a = g_area(o->polys.v[j]); if (best < 0 || a > ba) { best = 1; ba = a; sl = o->polys.v[j]; } }
    }
    if (best < 0) return;
    n = (int)bs_randint(rng, 2, 3);
    for (k = 0; k < n; k++) {
        Num z; Geo *g = bs_find(s, mk_tilt, rng, 200, 0.35, g_inter(g_inset(sl, 0.6), DISC), 0, "slot", &z), *gb; TcutList nt = { 0 };
        if (!g) continue;
        gb = g_buf(g, 0.15);
        for (i = 0; i < s->tcuts.n; i++) {
            Tcut *c = &s->tcuts.v[i]; GeoList *ps2 = LL(); int a2, b2;
            for (a2 = 0; a2 < c->polys.n; a2++) { GeoList *qs = g_polys(g_diff(c->polys.v[a2], gb)); for (b2 = 0; b2 < qs->n; b2++) if (!g_isempty(g_buf(qs->v[b2], -0.1))) LP(ps2, qs->v[b2]); }
            if (ps2->n) tcl_push(&nt, ps2, c->z0, c->z1, c->kind);
        }
        tcl_free_(&s->tcuts); s->tcuts = nt;
        bs_up(s, L1(g), z, num_f(0.4), "paver");
    }
}
/* u_crater */
static void u_crater_(Scene *s, PyRandom *rng, int lvl) {
    int it; (void)lvl;
    for (it = 0; it < 80; it++) {
        double cx, cy, r; Geo *g; Num zs;
        bs_rp(rng, 0, 9, &cx, &cy); r = UNI(1.8, 2.3); g = g_ptbuf(cx, cy, r + 2.4, 16);
        if (!g_contains(DISC, g)) continue;
        zs = bs_surf(s, g);
        if (zs.t == NUM_NONE || zs.v != H) continue;
        if (g_intersects(g_uu(bs_hard(s)), g)) continue;
        bs_evict(s, g); if (!gg2_crater(s, cx, cy, r, 0.7)) ly_raise();
        S_gcut(s, L1(radial_(cx, cy, r + 1.2, r + 2.6, 6, rng, 0.22)), num_f(0.4), "crack"); return;
    }
}

/* ================================================================ MINE */
/* n_posts: lambda: (lambda b: rect(d * math.cos(b), d * math.sin(b), 1.9, 1.9, b))(a + rng.uniform(-0.6, 0.6)) */
typedef struct { PyRandom *rng; double a, d; } PostMk;
static Geo *mk_post(void *c) { PostMk *m = (PostMk *)c; double b = m->a + py_uniform(m->rng, -0.6, 0.6); return rect_(m->d * COS(b), m->d * SIN(b), 1.9, 1.9, b); }
static void n_posts_(Scene *s, PyRandom *rng, int lvl) {
    double a0 = UNI(0, 2 * PI); int k, n = 2 + (lvl >= 3);
    for (k = 0; k < n; k++) {
        PostMk m = { rng, a0 + k * 2.1, 12.3 }; Num z, *tops; Geo *g = bs_find(s, mk_post, &m, 40, 0.3, NULL, 0, NULL, &z); GeoList *got;
        if (!g) continue;
        got = S_tprism(s, L1(g), num_f(2.6), "post", &tops);
        if (got->n) bs_topcut(s, L1(ring_(cx_(g), cy_(g), 0.7, 0.45, 12)), tops[0], num_f(0.25), "crack");
    }
}
/* n_ore: lambda: stone(p.x + rng.uniform(-.5, .5), p.y + rng.uniform(-.5, .5), rng.uniform(0.6, 0.9), rng, .7, 5, round_=False) */
typedef struct { PyRandom *rng; double x, y; } OreMk;
static Geo *mk_ore(void *c) { OreMk *m = (OreMk *)c; double x = m->x + py_uniform(m->rng, -.5, .5), y = m->y + py_uniform(m->rng, -.5, .5), r = py_uniform(m->rng, 0.6, 0.9); return g_stone(x, y, r, m->rng, .7, 5); }
static void n_ore_(Scene *s, PyRandom *rng, int lvl) {
    double phi = UNI(0, PI), off = UNI(-8, 8), *us; Geo *ln = meander_(rng, phi, off, 1.0, 1, 30); int i, n = ly_arange(0, g_len(ln), 1.0, &us); (void)lvl;
    for (i = 0; i < n; i++) {
        Geo *p = g_interp(ln, us[i]), *g; Num z, *tops; OreMk m = { rng, g_x(p), g_y(p) };
        g = bs_find(s, mk_ore, &m, 8, 0.12, NULL, 0, NULL, &z);
        if (g) S_tprism(s, L1(g), num_f(0.35), "quartz", &tops);
    }
}
/* n_timbers */
static void sa_timbers(void *c, double cx, double cy, GeoList *out) {
    PyRandom *rng = (PyRandom *)c; double a = UNI(0, 2 * PI), b; Frame f = fr_(cx, cy, a); b = UNI(0.5, 0.8);
    LP(out, rectf_(&f, 3.2, -0.4, 6.2, 1.3, a - b / 2)); LP(out, rectf_(&f, 3.2, 1.9, 6.2, 1.3, a + b / 2));
}
static void n_timbers_(Scene *s, PyRandom *rng, int lvl) {
    Num z; GeoList *ps = bs_place_group(s, rng, sa_timbers, rng, 120, 0.4, 0, NULL, &z); int i, k; (void)lvl;
    if (!ps || !ps->n) return;
    ps = g_polys(g_uu(ps));
    for (k = 0; k < ps->n; k++) {
        Geo *p = ps->v[k]; Num *tops; GeoList *got = S_tprism(s, L1(p), num_f(0.9), "wood", &tops);
        if (got->n) {
            double x = cx_(p), y = cy_(p), *mr, bl = 0, ba = 0; int best = -1;
            mrr_coords(p, &mr);
            for (i = 0; i < 2; i++) {          /* max([(length, angle) for i in range(2)])[1]: tuples compared, the first of equal ones */
                double l = py_hypot(mr[2 * i + 2] - mr[2 * i], mr[2 * i + 3] - mr[2 * i + 1]), an = py_atan2(mr[2 * i + 3] - mr[2 * i + 1], mr[2 * i + 2] - mr[2 * i]);
                if (best < 0 || l > bl || (l == bl && an > ba)) { best = i; bl = l; ba = an; }
            }
            bs_topcut(s, L1(rect_(x, y, 5.0, 0.3, ba)), tops[0], num_f(0.25), "crack");
        }
    }
}
/* n_seep */
static void n_seep_(Scene *s, PyRandom *rng, int lvl) {
    int it, i; (void)lvl;
    for (it = 0; it < 80; it++) {
        double cx, cy, r, *ts; Geo *g; Num zf; GeoList *dr; int n;
        bs_rp(rng, 0, 10, &cx, &cy); r = UNI(1.6, 2.2); g = blob_(cx, cy, r, rng, 4);
        if (!g_contains(DISC, g)) continue;
        zf = S_tflat(s, g_buf(g, 1.0));
        if (zf.t == NUM_NONE) continue;
        if (g_intersects(g_uu(bs_hard(s)), g_buf(g, 1.0))) continue;
        bs_evict(s, g_buf(g, 1.0)); S_tcutflat(s, L1(g), num_f(0.4), "water");
        n = ly_linspace(0, 6.28, 6, &ts); dr = LL();
        for (i = 0; i < n - 1; i++) LP(dr, g_ptbuf(cx + 2.6 * COS(ts[i]), cy + 2.6 * SIN(ts[i]), 0.35, 8));
        S_tcutflat(s, dr, num_f(0.4), "pit");
        return;
    }
}
/* n_orepile: cluster(s, rng, 7, lambda x, y: stone(x, y, rng.uniform(0.6, 1.0), rng, .7, 5, round_=False), cx, cy, 1.6, 0.15) */
static Geo *one_ore(void *c, double x, double y) { EggSa *m = (EggSa *)c; double r = py_uniform(m->rng, 0.6, 1.0); return g_stone(x, y, r, m->rng, .7, 5); }
static void sa_orepile(void *c, double cx, double cy, GeoList *out) { EggSa *m = (EggSa *)c; bs_cluster(m->s, m->rng, 7, one_ore, m, cx, cy, 1.6, 0.15, out); }
static void n_orepile_(Scene *s, PyRandom *rng, int lvl) {
    EggSa m = { s, rng }; Num z; GeoList *ps = bs_place_group(s, rng, sa_orepile, &m, 120, 0.4, 0, NULL, &z); int i; Tier t[2]; (void)lvl;
    if (ps && ps->n) for (i = 0; i < ps->n; i++) S_tstack(s, L1(ps->v[i]), num_f(0.7), TS2(t, 0.7, 6, 0.3, 40), 2, i % 2 ? "quartz" : "rock");
}
/* n_shaft */
static void n_shaft_(Scene *s, PyRandom *rng, int lvl) {
    int it; (void)lvl;
    for (it = 0; it < 80; it++) {
        double cx, cy, r; Geo *g;
        bs_rp(rng, 0, 8, &cx, &cy); r = UNI(2.5, 2.9); g = g_ptbuf(cx, cy, r + 2.2, 16);
        if (!g_contains(DISC, g)) continue;
        if (g_intersects(g_uu(bs_hard(s)), g)) continue;
        bs_evict(s, g); bs_crater_t(s, cx, cy, r); S_gcut(s, L1(radial_(cx, cy, r + 0.8, r + 3.0, 7, rng, 0.22)), num_f(0.4), "crack"); return;
    }
}
/* n_barrel */
static void sa_barrel(void *c, double cx, double cy, GeoList *out) {
    PyRandom *rng = (PyRandom *)c; Geo *rg = ring_(cx, cy, 1.9, 0.85, 32); double a0 = UNI(0, 1); GeoList *cuts = LL(), *ps; int k;
    for (k = 0; k < 8; k++) LP(cuts, rect_(cx + 1.3 * COS(a0 + k * 0.785), cy + 1.3 * SIN(a0 + k * 0.785), 1.2, 0.25, a0 + k * 0.785));
    ps = g_polys(g_diff(rg, g_uu(cuts)));
    for (k = 0; k < ps->n; k++) if (k % 4 != 1) LP(out, ps->v[k]);
    { double y = cy + UNI(-1, 1), a = UNI(0, 3); LP(out, rect_(cx + 2.8, y, 2.4, 0.95, a)); }
}
static void n_barrel_(Scene *s, PyRandom *rng, int lvl) {
    Num z, *tops; GeoList *ps = bs_place_group(s, rng, sa_barrel, rng, 300, 0.3, 0, NULL, &z), *ok = LL(); int i; (void)lvl;
    if (!ps || !ps->n) return;
    for (i = 0; i < ps->n; i++) if (g_wide(ps->v[i], SC.MINW)) LP(ok, ps->v[i]);
    S_tprism(s, ok, num_f(1.0), "wood", &tops);
}
/* n_ladder */
static void sa_ladder(void *c, double cx, double cy, GeoList *out) {
    PyRandom *rng = (PyRandom *)c; double a = UNI(0, PI), *us; Frame f = fr_(cx, cy, a); GeoList *l = LL(), *ps; int i, n;
    LP(l, rectf_(&f, 0, -1.1, 7.5, 0.9, a)); LP(l, rectf_(&f, 0, 1.1, 7.5, 0.9, a));
    n = ly_arange(-3, 3.1, 1.5, &us);
    for (i = 0; i < n; i++) LP(l, rectf_(&f, us[i], 0, 0.9, 1.5, a));
    ps = g_polys(g_uu(l));
    for (i = 0; i < ps->n; i++) LP(out, ps->v[i]);
}
static void n_ladder_(Scene *s, PyRandom *rng, int lvl) {
    Num z, *tops; GeoList *ps = bs_place_group(s, rng, sa_ladder, rng, 300, 0.3, 0, NULL, &z); (void)lvl;
    if (ps && ps->n) S_tprism(s, ps, num_f(0.6), "wood", &tops);
}

/* ================================================================ the entries: each in its own raise frame */
#define ENTRY(nm) static int nm(Scene *s, PyRandom *rng, int lvl) { BsFrame F; bs_begin(&F); if (setjmp(F.jb)) { bs_end(&F); return SC_ERR; } nm##_(s, rng, lvl); bs_end(&F); return 0; }
ENTRY(b_wash) ENTRY(b_hoodoo) ENTRY(b_mudcrack) ENTRY(b_agave) ENTRY(b_washbed) ENTRY(b_spiral) ENTRY(b_split) ENTRY(b_scree)
ENTRY(h_bog) ENTRY(h_steps) ENTRY(h_wall) ENTRY(h_moss) ENTRY(h_heather) ENTRY(h_standing) ENTRY(h_stream) ENTRY(h_fern)
ENTRY(m_ruin) ENTRY(m_shrub) ENTRY(m_sinkhole) ENTRY(m_crystal) ENTRY(m_salt) ENTRY(m_obelisk) ENTRY(m_column) ENTRY(m_pavers)
ENTRY(c_vent) ENTRY(c_chevron) ENTRY(c_cable) ENTRY(c_panel) ENTRY(c_spill) ENTRY(c_hexdeck) ENTRY(c_scorch) ENTRY(c_lights)
ENTRY(i_eggs) ENTRY(i_acid) ENTRY(i_veins) ENTRY(i_spores) ENTRY(i_hatched) ENTRY(i_torn) ENTRY(i_ribs)
ENTRY(u_bricks) ENTRY(u_bullets) ENTRY(u_wall) ENTRY(u_dashes) ENTRY(u_scorch) ENTRY(u_rebar) ENTRY(u_tilt) ENTRY(u_crater)
ENTRY(n_posts) ENTRY(n_ore) ENTRY(n_timbers) ENTRY(n_seep) ENTRY(n_orepile) ENTRY(n_shaft) ENTRY(n_barrel) ENTRY(n_ladder)

#define T(st, t, nm) { st, t, #nm, nm }
const BsTier BS_TIERS[] = {
    T("badlands", 1, b_wash), T("badlands", 1, b_hoodoo), T("badlands", 2, b_mudcrack), T("badlands", 2, b_agave), T("badlands", 2, b_washbed),
    T("badlands", 3, b_split), T("badlands", 3, b_scree), T("badlands", 3, b_spiral),
    T("highland", 1, h_bog), T("highland", 1, h_steps), T("highland", 2, h_wall), T("highland", 2, h_moss), T("highland", 2, h_heather),
    T("highland", 3, h_stream), T("highland", 3, h_standing), T("highland", 3, h_fern),
    T("maahl", 1, m_ruin), T("maahl", 1, m_shrub), T("maahl", 2, m_sinkhole), T("maahl", 2, m_crystal), T("maahl", 2, m_salt),
    T("maahl", 3, m_obelisk), T("maahl", 3, m_column), T("maahl", 3, m_pavers),
    T("corridor", 1, c_vent), T("corridor", 1, c_chevron), T("corridor", 2, c_cable), T("corridor", 2, c_panel), T("corridor", 2, c_spill),
    T("corridor", 3, c_hexdeck), T("corridor", 3, c_scorch), T("corridor", 3, c_lights),
    T("infestation", 1, i_eggs), T("infestation", 1, i_acid), T("infestation", 2, i_veins), T("infestation", 2, i_spores),
    T("infestation", 3, i_hatched), T("infestation", 3, i_torn), T("infestation", 3, i_ribs),
    T("urban", 1, u_bricks), T("urban", 1, u_bullets), T("urban", 2, u_wall), T("urban", 2, u_dashes), T("urban", 2, u_scorch),
    T("urban", 3, u_rebar), T("urban", 3, u_tilt), T("urban", 3, u_crater),
    T("mine", 1, n_posts), T("mine", 1, n_ore), T("mine", 2, n_timbers), T("mine", 2, n_seep), T("mine", 2, n_orepile),
    T("mine", 3, n_shaft), T("mine", 3, n_barrel), T("mine", 3, n_ladder),
};
const int BS_NTIERS = (int)(sizeof BS_TIERS / sizeof BS_TIERS[0]);
BsTierFn bs_tier_fn(const char *name) { int i; for (i = 0; i < BS_NTIERS; i++) if (!strcmp(BS_TIERS[i].name, name)) return BS_TIERS[i].fn; return NULL; }
int bs_is_late(const char *n) { return !strcmp(n, "b_washbed") || !strcmp(n, "b_scree") || !strcmp(n, "b_spiral"); }
int bs_is_sparse(const char *n) { return !strcmp(n, "c_cable") || !strcmp(n, "c_lights") || !strcmp(n, "c_chevron"); }
void bld_register_gg3(void) { int i; for (i = 0; i < BS_NTIERS; i++) bld_add_any("gg3", BS_TIERS[i].name, "tier", (void *)BS_TIERS[i].fn); }
