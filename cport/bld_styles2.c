/* bld_styles2.c: styles2.py's signature features (f_*) and the TIERS2 / DETAILS2 tables in C (see bld_details.h).
   Each function names the Python routine it copies. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "bld_details.h"
#include "shp.h"
#include "pyexact.h"

#define PI 3.141592653589793
static Num NF(double v) { return num_f(v); }
static Kind *KS(const char *s) { return kind_str(s ? s : "None"); }
static GeoList one_list(Geo *g) { GeoList l = { 0 }; gl_push(&l, g); return l; }
/* s.prism([g], top, kind, z0=z0, foot=False) with g taken over */
static int prism1(Scene *s, Geo *g, Num top, const char *kind, Num z0, int foot) {
    GeoList l = { 0 }, out = { 0 }; int r;
    if (!g) return SC_ERR;
    gl_push_own(&l, g);
    r = sc_prism(s, &l, top, KS(kind), z0, SC.MINW, foot, &out);
    gl_free(&l); gl_free(&out);
    return r < 0 ? SC_ERR : 0;
}
static int truthy(Num n) { return n.t != NUM_NONE && n.v != 0; }
static Num capz(Num t) { return num_min2(t, NF(SC.CAP)); }          /* min(t, CAPZ) */

/* ---------------- the make / at callbacks ---------------- */
typedef struct { const DtFn *f; PyRandom *rng; } Mk;
/* f_post_row's at(cx, cy) */
static int post_sa(void *c, double cx, double cy, GeoList *out) {
    Mk *m = (Mk *)c; const DtFn *f = m->f; double a = py_uniform(m->rng, 0, PI); int k = (int)py_randint(m->rng, (int64_t)f->n0, (int64_t)f->n1); Frame ff = lf_frame(cx, cy, a);
    for (int i = 0; i < k; i++) { double x, y; Geo *g; lf_at(&ff, (i - (k - 1) / 2.0) * f->x[0], 0, &x, &y); g = lf_rect(x, y, 0.95, 0.95, a); if (!g) return SC_ERR; gl_push_own(out, g); }
    if (f->flag) { Geo *g = lf_rect(cx, cy, (k - 1) * f->x[0], 0.5, a); if (!g) return SC_ERR; gl_push_own(out, g); }
    return 0;
}
/* f_teeth's at(cx, cy) */
static int teeth_sa(void *c, double cx, double cy, GeoList *out) {
    Mk *m = (Mk *)c; double a0 = py_uniform(m->rng, 0, 2 * PI), R_ = py_uniform(m->rng, 4, 6), ox = cx - R_ * py_cos(a0), oy = cy - R_ * py_sin(a0), ts[16]; int n = (int)py_randint(m->rng, 4, 6);
    if (lf_linspace(-0.5, 0.5, n, ts) != n) return SC_ERR;
    for (int i = 0; i < n; i++) {
        double r = py_uniform(m->rng, 0.6, 0.85); Geo *g = lf_stone(ox + R_ * py_cos(a0 + ts[i]), oy + R_ * py_sin(a0 + ts[i]), r, m->rng, 0.8, 3, 0);
        if (!g) return SC_ERR;
        gl_push_own(out, g);
    }
    return 0;
}
/* f_sandbags' at(cx, cy) */
static int sand_sa(void *c, double cx, double cy, GeoList *out) {
    Mk *m = (Mk *)c; double a = py_uniform(m->rng, 0, PI); Frame ff = lf_frame(cx, cy, a);
    for (int i = 0; i < 3; i++) {
        double x, y; GEOSGeometry *r, *b;
        lf_at(&ff, (i - 1) * 3.0, 0, &x, &y); r = shp_rect(x, y, 2.7, 1.5, a); if (!r) return SC_ERR;
        b = shp_buffer_q(r, 0.3, 16); shp_free(r); if (!b) return SC_ERR;
        gl_push_own(out, geo_new(b));
    }
    return 0;
}
/* the find makes of the plain functions: make() (no arguments) */
static Geo *mk_none(void *c, int has_c, double cx, double cy, int *err) {
    Mk *m = (Mk *)c; PyRandom *rng = m->rng; Geo *g = NULL; double x, y;
    (void)cx; (void)cy;
    if (has_c) { *err = 1; return NULL; }
    switch (m->f->t) {
    case DT_HEADSTONE: {          /* mkst().buffer(0.45, join_style=2) */
        double a, l; GEOSGeometry *r, *b;
        lf_rp(rng, 1, 10, &x, &y); a = py_uniform(rng, 0, PI); l = py_uniform(rng, 2.6, 3.2);
        r = shp_rect(x, y, l, 1.0, a); if (!r) break;
        b = shp_buffer(r, 0.45, 16, SHP_CAP_ROUND, SHP_JOIN_MITRE, 5.0, 0); shp_free(r);
        g = geo_new(b); break;
    }
    case DT_LANTERN: lf_rp(rng, 2, 10, &x, &y); g = geo_new(shp_point_buffer(x, y, 1.5, 6)); break;
    case DT_COLUMN: lf_rp(rng, 2, 10, &x, &y); g = geo_new(shp_point_buffer(x, y, 1.9, 4)); break;
    case DT_STUMP: { double r; lf_rp(rng, 1, 9, &x, &y); r = py_uniform(rng, 1.5, 2.1); g = lf_stone(x, y, r, rng, 0.9, 9, 0); break; }
    case DT_COFFIN: {
        double a, xy[12]; Frame ff;
        lf_rp(rng, 0, 8, &x, &y); a = py_uniform(rng, 0, PI); ff = lf_frame(x, y, a);
        lf_at(&ff, -3.2, -0.8, &xy[0], &xy[1]); lf_at(&ff, -1.6, -1.3, &xy[2], &xy[3]); lf_at(&ff, 3.2, -0.9, &xy[4], &xy[5]);
        lf_at(&ff, 3.2, 0.9, &xy[6], &xy[7]); lf_at(&ff, -1.6, 1.3, &xy[8], &xy[9]); lf_at(&ff, -3.2, 0.8, &xy[10], &xy[11]);
        g = geo_new(shp_polygon(xy, 6)); break;
    }
    case DT_TRAPDOOR: lf_rp(rng, 0, 8, &x, &y); g = lf_rect(x, y, 5.0, 5.0, py_uniform(rng, 0, PI)); break;
    case DT_VENT: { double r; lf_rp(rng, 1, 9, &x, &y); r = py_uniform(rng, 1.5, 2.0); g = geo_new(shp_point_buffer(x, y, r, 9)); break; }
    }
    if (!g) *err = 1;
    return g;
}

/* ---------------- the features ---------------- */
/* f_post_row(kind, h, rail, n, gap).fn */
static int f_post_row(const DtFn *f, Scene *s, PyRandom *rng) {
    Mk m = { f, rng }; GeoList ps = { 0 }, posts = { 0 }; Num z, top; double hh; int r;
    r = pl_place_group(s, rng, post_sa, &m, 120, 0.4, 0, NULL, &ps, &z);
    if (r < 0) goto fail;
    if (r == 0 || !ps.n) { gl_free(&ps); return DT_NONE; }
    hh = py_uniform(rng, f->h0, f->h1);
    for (int i = 0; i < (f->flag ? ps.n - 1 : ps.n); i++) gl_push(&posts, ps.v[i]);
    if (pl_up(s, &posts, z, NF(hh), KS(f->k1), &top) < 0) goto fail;
    if (f->flag && truthy(top)) {
        Num z0 = z.v <= SC.H ? NF(SC.H - SC.SINK) : num_sub(z, NF(0.15));
        if (prism1(s, geo_ref(ps.v[ps.n - 1]), capz(num_sub(top, NF(0.3))), f->k1, z0, 1) < 0) goto fail;
    }
    gl_free(&ps); gl_free(&posts); return DT_NONE;
fail: gl_free(&ps); gl_free(&posts); return SC_ERR;
}
/* f_headstone(s, rng, lvl) */
static int f_headstone(const DtFn *f, Scene *s, PyRandom *rng, int lvl) {
    Mk m = { f, rng }; int n = (int)py_randint(rng, 1, 2) + (lvl >= 3);
    for (int i = 0; i < n; i++) {
        Geo *g = NULL; Num z, top; GeoList l; int r = pl_find(s, mk_none, &m, 120, 0.4, NULL, 0, NULL, NULL, NULL, &g, &z);
        if (r < 0) return SC_ERR;
        if (r == 0) continue;
        l = one_list(g); r = pl_up(s, &l, z, NF(0.4), KS("headstone"), &top); gl_free(&l);
        if (r < 0) { geo_unref(g); return SC_ERR; }
        if (truthy(top)) {
            Num t2 = num_add(top, NF(py_uniform(rng, 1.8, 2.5)));
            r = prism1(s, lf_inset(g, 0.45), capz(t2), "headstone", num_sub(top, NF(0.15)), 0);
            if (r < 0) { geo_unref(g); return SC_ERR; }
        }
        geo_unref(g);
    }
    return DT_NONE;
}
/* f_lantern(s, rng, lvl) */
static int f_lantern(const DtFn *f, Scene *s, PyRandom *rng) {
    Mk m = { f, rng }; Geo *g = NULL, *c = NULL; Num z, top; GeoList l; double x, y; int r = pl_find(s, mk_none, &m, 120, 0.4, NULL, 0, NULL, NULL, NULL, &g, &z);
    if (r < 0) return SC_ERR;
    if (r == 0) return DT_NONE;
    c = geo_new(shp_centroid(g->g)); if (!c) { geo_unref(g); return SC_ERR; }
    x = shp_x(c->g); y = shp_y(c->g); geo_unref(c);
    l = one_list(g); r = pl_up(s, &l, z, NF(0.5), KS("lantern"), &top); gl_free(&l); geo_unref(g);
    if (r < 0) return SC_ERR;
    if (!truthy(top)) return DT_NONE;
    if (prism1(s, geo_new(shp_point_buffer(x, y, 0.7, 16)), capz(num_add(top, NF(1.2))), "lantern", num_sub(top, NF(0.15)), 0) < 0) return SC_ERR;
    if (prism1(s, geo_new(shp_point_buffer(x, y, 1.25, 6)), capz(num_add(top, NF(1.9))), "lantern", num_add(top, NF(1.05)), 0) < 0) return SC_ERR;
    return DT_NONE;
}
/* f_column(kind, broken).fn */
static int f_column(const DtFn *f, Scene *s, PyRandom *rng) {
    Mk m = { f, rng }; Geo *g = NULL, *c = NULL, *g2; Num z, top; GeoList l; double x, y; int r = pl_find(s, mk_none, &m, 120, 0.4, NULL, 0, NULL, NULL, NULL, &g, &z);
    if (r < 0) return SC_ERR;
    if (r == 0) return DT_NONE;
    c = geo_new(shp_centroid(g->g)); if (!c) { geo_unref(g); return SC_ERR; }
    x = shp_x(c->g); y = shp_y(c->g); geo_unref(c);
    g2 = geo_new(shp_rotate(g->g, py_uniform(rng, 0, 90), SHP_ORIGIN_POINT, x, y, 0)); geo_unref(g);
    if (!g2) return SC_ERR;
    l = one_list(g2); r = pl_up(s, &l, z, NF(0.5), KS(f->k1), &top); gl_free(&l); geo_unref(g2);
    if (r < 0) return SC_ERR;
    if (truthy(top) && prism1(s, geo_new(shp_point_buffer(x, y, 1.25, 20)), capz(num_add(top, NF(py_uniform(rng, 1.4, 2.4)))), f->k1, num_sub(top, NF(0.15)), 0) < 0) return SC_ERR;
    return DT_NONE;
}
/* f_stump(s, rng, lvl) */
static int f_stump(const DtFn *f, Scene *s, PyRandom *rng) {
    Mk m = { f, rng }; Geo *g = NULL, *c; Num z, top; GeoList l; double x, y; int r = pl_find(s, mk_none, &m, 120, 0.4, NULL, 0, NULL, NULL, NULL, &g, &z);
    if (r < 0) return SC_ERR;
    if (r == 0) return DT_NONE;
    c = geo_new(shp_centroid(g->g)); if (!c) { geo_unref(g); return SC_ERR; }
    x = shp_x(c->g); y = shp_y(c->g); geo_unref(c);
    l = one_list(g); r = pl_up(s, &l, z, NF(py_uniform(rng, 1.0, 1.6)), KS("stump"), &top); gl_free(&l);
    if (r < 0) { geo_unref(g); return SC_ERR; }
    if (truthy(top)) {
        GEOSGeometry *a = shp_ring(x, y, 0.7, 0.7 - 0.25, 20), *b = shp_ring(x, y, 1.2, 1.2 - 0.25, 20), *u = NULL, *in = NULL, *q = NULL;
        if (a && b) { const GEOSGeometry *v[2] = { a, b }; u = shp_union_all(v, 2); }
        if (u) in = shp_inset(g->g, 0.3);
        if (in) q = shp_intersection(u, in);
        shp_free(a); shp_free(b); shp_free(u); shp_free(in);
        if (!q) { geo_unref(g); return SC_ERR; }
        l = one_list(geo_new(q)); geo_unref(l.v[0]);
        r = pl_topcut(s, &l, top, NF(0.25), KS("slot")); gl_free(&l);
        if (r < 0) { geo_unref(g); return SC_ERR; }
    }
    geo_unref(g);
    return DT_NONE;
}
/* f_teeth(s, rng, lvl) */
static const Tier TEETH_T[2] = { { { 0.6, NUM_FLOAT, 0 }, { 6, NUM_INT, 0 } }, { { 0.4, NUM_FLOAT, 0 }, { 28, NUM_INT, 0 } } };
static const Tier SAND_T[2] = { { { 0.7, NUM_FLOAT, 0 }, { 8, NUM_INT, 0 } }, { { 0.3, NUM_FLOAT, 0 }, { 40, NUM_INT, 0 } } };
static int f_group(const DtFn *f, Scene *s, PyRandom *rng) {          /* f_teeth / f_sandbags */
    Mk m = { f, rng }; GeoList ps = { 0 }; Num z; int r;
    r = pl_place_group(s, rng, f->t == DT_TEETH ? teeth_sa : sand_sa, &m, 120, 0.4, 0, NULL, &ps, &z);
    if (r == 1 && ps.n) {
        if (f->t == DT_TEETH) r = pl_mound(s, &ps, z, NF(py_uniform(rng, 1.4, 2.0)), TEETH_T, 2, KS("tooth"));
        else r = pl_mound(s, &ps, z, NF(0.9), SAND_T, 2, KS("sandbag"));
    }
    gl_free(&ps);
    return r < 0 ? SC_ERR : DT_NONE;
}
/* f_coffin(s, rng, lvl) */
static int f_coffin(const DtFn *f, Scene *s, PyRandom *rng) {
    Mk m = { f, rng }; Geo *g = NULL; Num z, top; GeoList l; int r = pl_find(s, mk_none, &m, 120, 0.4, NULL, 0, NULL, NULL, NULL, &g, &z);
    if (r < 0) return SC_ERR;
    if (r == 0) return DT_NONE;
    l = one_list(g); r = pl_up(s, &l, z, NF(py_uniform(rng, 0.6, 0.9)), KS("coffin"), &top); gl_free(&l);
    if (r >= 0 && truthy(top)) {
        GEOSGeometry *a = shp_centroid(g->g), *b = NULL, *c = NULL, *q = NULL;
        if (a) b = shp_inset(g->g, 0.4);
        if (b) c = shp_inset(g->g, 0.7);
        if (c) q = shp_difference(b, c);
        shp_free(a); shp_free(b); shp_free(c);
        if (!q) r = SC_ERR;
        else { l = one_list(geo_new(q)); geo_unref(l.v[0]); r = pl_topcut(s, &l, top, NF(0.25), KS("slot")); gl_free(&l); }
    }
    geo_unref(g);
    return r < 0 ? SC_ERR : DT_NONE;
}
/* f_trapdoor(s, rng, lvl) */
static int f_trapdoor(const DtFn *f, Scene *s, PyRandom *rng) {
    Mk m = { f, rng }; Geo *g = NULL; Num z, top; GeoList l; int r = pl_find(s, mk_none, &m, 120, 0.4, NULL, 0, NULL, NULL, NULL, &g, &z);
    if (r < 0) return SC_ERR;
    if (r == 0) return DT_NONE;
    l = one_list(g); r = pl_up(s, &l, z, NF(0.35), KS("trap"), &top); gl_free(&l);
    if (r >= 0 && truthy(top)) {
        GEOSGeometry *mr = shp_min_rot_rect(g->g), *cen = NULL, *v[3] = { 0 }, *u = NULL, *in = NULL, *q = NULL; double xy[64], a = 0, x, y; int n = -1; Frame ff;
        r = SC_ERR;
        if (mr && shp_type(mr) == GEOS_POLYGON) n = shp_coords(shp_exterior(mr), xy, 32);
        shp_free(mr);
        if (n >= 2 && (cen = shp_centroid(g->g))) {
            static const double V[3] = { -1.25, 0, 1.25 }; int ok = 1;
            a = py_atan2(xy[3] - xy[1], xy[2] - xy[0]); ff = lf_frame(shp_x(cen), shp_y(cen), a);
            for (int i = 0; i < 3; i++) { lf_at(&ff, 0, V[i], &x, &y); v[i] = shp_rect(x, y, 6, 0.3, a); if (!v[i]) ok = 0; }
            if (ok) u = shp_union_all((const GEOSGeometry *const *)v, 3);
            if (u) in = shp_inset(g->g, 0.3);
            if (in) q = shp_intersection(u, in);
            if (q) {
                l = one_list(geo_new(q)); geo_unref(l.v[0]); r = pl_topcut(s, &l, top, NF(0.25), KS("slot")); gl_free(&l);
                if (r >= 0) { lf_at(&ff, 1.6, 0, &x, &y); r = prism1(s, lf_ring(x, y, 0.75, 0.35, 16), capz(num_add(top, NF(0.3))), "iron", num_sub(top, NF(0.15)), 0); }
            }
        }
        for (int i = 0; i < 3; i++) shp_free(v[i]);
        shp_free(cen); shp_free(u); shp_free(in);
    }
    geo_unref(g);
    return r < 0 ? SC_ERR : DT_NONE;
}
/* f_lily(s, rng, lvl) */
static int f_lily(Scene *s, PyRandom *rng) {
    GeoList water = { 0 }, pads = { 0 }; GEOSGeometry *u = NULL, *reg = NULL; double b[4]; int e, r = DT_NONE; Kind *wk = kind_str("water");
    for (int i = 0; i < s->gcuts.n; i++) if (kind_eq(s->gcuts.v[i].kind, wk)) gl_extend(&water, &s->gcuts.v[i].polys);
    if (!water.n) { gl_free(&water); return DT_NONE; }
    { const GEOSGeometry **v = (const GEOSGeometry **)malloc(sizeof(*v) * (size_t)water.n); for (int i = 0; i < water.n; i++) v[i] = water.v[i]->g; u = shp_union_all(v, water.n); free(v); }
    if (u) reg = shp_buffer_q(u, -0.9, 16);
    shp_free(u); gl_free(&water);
    if (!reg) return SC_ERR;
    e = shp_is_empty(reg);
    if (e) { shp_free(reg); return e < 0 ? SC_ERR : DT_NONE; }
    shp_bounds(reg, b);
    for (int it = 0; it < 200; it++) {
        double cx, cy, rr, xy[6]; GEOSGeometry *cb, *tri, *p, *cb2; int in, ok = 1;
        if (pads.n >= py_randint(rng, 3, 6)) break;
        cx = py_uniform(rng, b[0], b[2]); cy = py_uniform(rng, b[1], b[3]); rr = py_uniform(rng, 0.7, 1.0);
        xy[0] = cx; xy[1] = cy; xy[2] = cx + 2 * rr; xy[3] = cy - 0.5 * rr; xy[4] = cx + 2 * rr; xy[5] = cy + 0.5 * rr;
        cb = shp_point_buffer(cx, cy, rr, 16); tri = shp_polygon(xy, 3); p = cb && tri ? shp_difference(cb, tri) : NULL;
        shp_free(cb); shp_free(tri);
        cb2 = p ? shp_point_buffer(cx, cy, rr, 16) : NULL;
        if (!cb2) { shp_free(p); r = SC_ERR; break; }
        in = shp_contains(reg, cb2); shp_free(cb2);
        if (in < 0) { shp_free(p); r = SC_ERR; break; }
        if (in) for (int j = 0; j < pads.n; j++) if (!(shp_distance(p, pads.v[j]->g) > 0.4)) { ok = 0; break; }
        if (in && ok) gl_push_own(&pads, geo_new(p)); else shp_free(p);
    }
    shp_free(reg);
    if (r == DT_NONE && pads.n) {
        GeoList out = { 0 };
        if (sc_prism(s, &pads, NF(SC.H - 0.2), KS("lily"), NF(SC.H - 0.75), SC.MINW, 0, &out) < 0) r = SC_ERR;
        gl_free(&out);
    }
    gl_free(&pads);
    return r;
}
/* f_vent(kind_cut, kind_rim).fn */
static int f_vent(const DtFn *f, Scene *s, PyRandom *rng) {
    Mk m = { f, rng }; Geo *g = NULL; Num z, top; GeoList l; int r = pl_find(s, mk_none, &m, 120, 0.4, NULL, 0, NULL, NULL, NULL, &g, &z);
    if (r < 0) return SC_ERR;
    if (r == 0) return DT_NONE;
    l = one_list(g); r = pl_up(s, &l, z, NF(py_uniform(rng, 0.7, 1.1)), KS(f->k2), &top); gl_free(&l);
    if (r >= 0 && truthy(top)) { Geo *q = lf_inset(g, 0.55); if (!q) r = SC_ERR; else { l = one_list(q); geo_unref(q); r = pl_topcut(s, &l, top, NF(0.5), KS(f->k1)); gl_free(&l); } }
    geo_unref(g);
    return r < 0 ? SC_ERR : DT_NONE;
}

int dt_styles2_run(const DtFn *f, Scene *s, PyRandom *rng, double arg) {
    switch (f->t) {
    case DT_POST_ROW: return f_post_row(f, s, rng);
    case DT_COLUMN: return f_column(f, s, rng);
    case DT_VENT: return f_vent(f, s, rng);
    case DT_HEADSTONE: return f_headstone(f, s, rng, (int)arg);
    case DT_LANTERN: return f_lantern(f, s, rng);
    case DT_STUMP: return f_stump(f, s, rng);
    case DT_TEETH: case DT_SANDBAGS: return f_group(f, s, rng);
    case DT_COFFIN: return f_coffin(f, s, rng);
    case DT_TRAPDOOR: return f_trapdoor(f, s, rng);
    case DT_LILY: return f_lily(s, rng);
    }
    return SC_ERR;
}

/* ---------------- the plain functions, the constructors ---------------- */
#define PL_(T, Q) { .t = T, .qual = Q, .num = { 0, NUM_NONE, 0 } }
static const DtFn PLAIN[8] = {
    PL_(DT_HEADSTONE, "f_headstone"), PL_(DT_LANTERN, "f_lantern"), PL_(DT_STUMP, "f_stump"), PL_(DT_TEETH, "f_teeth"), PL_(DT_SANDBAGS, "f_sandbags"), PL_(DT_COFFIN, "f_coffin"), PL_(DT_TRAPDOOR, "f_trapdoor"), PL_(DT_LILY, "f_lily"),
};
const DtFn *dt_plain(int t) { return t >= DT_HEADSTONE && t <= DT_LILY ? &PLAIN[t - DT_HEADSTONE] : NULL; }
static int tier_headstone(Scene *s, PyRandom *rng, int lvl) { return dt_run(&PLAIN[0], s, rng, lvl); }
static int tier_lantern(Scene *s, PyRandom *rng, int lvl) { return dt_run(&PLAIN[1], s, rng, lvl); }
static int tier_stump(Scene *s, PyRandom *rng, int lvl) { return dt_run(&PLAIN[2], s, rng, lvl); }
static int tier_teeth(Scene *s, PyRandom *rng, int lvl) { return dt_run(&PLAIN[3], s, rng, lvl); }
static int tier_sandbags(Scene *s, PyRandom *rng, int lvl) { return dt_run(&PLAIN[4], s, rng, lvl); }
static int tier_coffin(Scene *s, PyRandom *rng, int lvl) { return dt_run(&PLAIN[5], s, rng, lvl); }
static int tier_trapdoor(Scene *s, PyRandom *rng, int lvl) { return dt_run(&PLAIN[6], s, rng, lvl); }
static int tier_lily(Scene *s, PyRandom *rng, int lvl) { return dt_run(&PLAIN[7], s, rng, lvl); }
static DtFn *nw(int t, const char *qual) { DtFn *f = (DtFn *)calloc(1, sizeof *f); bld_keep(f, free); f->t = t; f->qual = qual; f->num = num_none(); return f; }
DtFn *dt_post_row(const char *kind, double h0, double h1, int rail, double n0, double n1, double gap) {
    DtFn *f = nw(DT_POST_ROW, "f_post_row"); f->k1 = so_intern(kind); f->h0 = h0; f->h1 = h1; f->flag = rail; f->n0 = n0; f->n1 = n1; f->x[0] = gap; return f;
}
DtFn *dt_column(const char *kind) { DtFn *f = nw(DT_COLUMN, "f_column"); f->k1 = so_intern(kind); f->flag = 1; return f; }
DtFn *dt_vent(const char *kind_cut, const char *kind_rim) { DtFn *f = nw(DT_VENT, "f_vent"); f->k1 = so_intern(kind_cut); f->k2 = so_intern(kind_rim); return f; }

static const DtList *lfind(const DtList *t, int n, const char *style) { for (int i = 0; i < n; i++) if (!strcmp(t[i].style, style)) return &t[i]; return NULL; }
const DtList *dt_details2(const char *style) { return lfind(DT_DETAILS2, DT_NDETAILS2, style); }
const DtTiers *dt_tiers2(const char *style) { for (int i = 0; i < DT_NTIERS2; i++) if (!strcmp(DT_TIERS2[i].style, style)) return &DT_TIERS2[i]; return NULL; }

void bld_register_styles2(void) {
    bld_add_any("styles2", "DETAILS2", "dt_list", (void *)dt_details2);
    bld_add_any("styles2", "TIERS2", "dt_tiers", (void *)dt_tiers2);
    bld_add_any("styles2", "f_post_row", "dt_new", (void *)dt_post_row);
    bld_add_any("styles2", "f_column", "dt_new", (void *)dt_column);
    bld_add_any("styles2", "f_vent", "dt_new", (void *)dt_vent);
    bld_add_any("styles2", "f_headstone", "tier", (void *)tier_headstone);
    bld_add_any("styles2", "f_lantern", "tier", (void *)tier_lantern);
    bld_add_any("styles2", "f_stump", "tier", (void *)tier_stump);
    bld_add_any("styles2", "f_teeth", "tier", (void *)tier_teeth);
    bld_add_any("styles2", "f_sandbags", "tier", (void *)tier_sandbags);
    bld_add_any("styles2", "f_coffin", "tier", (void *)tier_coffin);
    bld_add_any("styles2", "f_trapdoor", "tier", (void *)tier_trapdoor);
    bld_add_any("styles2", "f_lily", "tier", (void *)tier_lily);
}

/* ---- generated by work\cscene\bld_features\gen_dtables.py from styles2.TIERS2 / DETAILS2 ---- */
/* @@DTABLES@@ */
static const DtFn S0 = { DT_SINGLE, "single", "single", "log", "caps", "log", NULL, "up", 1, { 5, { 5.0, 7.5, 1.3, 0.0, 0.0 } }, 1, { 1, { 1.6, 0.0 } }, 0.0, 0.0, 0.8, 1.1, { 0.0, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 2, { { { 0.75, 0, 0 }, { 5.0, 1, 0 } }, { { 0.25, 0, 0 }, { 40.0, 1, 0 } } }, 0 };
static const DtFn S1 = { DT_SCATTER, "scatter", "scatter", "reed", "disc", "reed", NULL, "up", 1, { 2, { 0.42, 0.52, 8.0, 0.0, 0.0 } }, 0, { 0, { 0.0, 0.0 } }, 6.0, 10.0, 1.4, 2.2, { 1.6, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 2, { { { 0.7, 0, 0 }, { 6.0, 1, 0 } }, { { 0.3, 0, 0 }, { 40.0, 1, 0 } } }, 0 };
static const DtFn S2 = { DT_PATH, "path", "path", "root", "", "root", NULL, "up", 0, { 0, { 0.0, 0.0, 0.0, 0.0, 0.0 } }, 0, { 0, { 0.0, 0.0 } }, 0.0, 0.0, 0.0, 0.0, { 0.9, 6.0, 10.0, 1.2 }, { 0.6, NUM_FLOAT, 0 }, 0, { { { 0 } } }, 0 };
static const DtFn S3 = { DT_COLUMN, "f_column", NULL, NULL, NULL, "tflag", NULL, NULL, 0, { 0, { 0.0, 0.0, 0.0, 0.0, 0.0 } }, 0, { 0, { 0.0, 0.0 } }, 0.0, 0.0, 0.0, 0.0, { 0.0, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 0, { { { 0 } } }, 1 };
static const DtFn S4 = { DT_SINGLE, "single", "single", "tflag", "hex", "tflag", NULL, "up", 1, { 9, { 1.4, 1.8, 0.0, 0.0, 0.0 } }, 1, { 6, { 0.0, 0.0 } }, 0.0, 0.0, 0.8, 1.1, { 0.0, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 2, { { { 0.75, 0, 0 }, { 5.0, 1, 0 } }, { { 0.25, 0, 0 }, { 40.0, 1, 0 } } }, 0 };
static const DtFn S5 = { DT_POST_ROW, "f_post_row", NULL, NULL, NULL, "wood", NULL, NULL, 0, { 0, { 0.0, 0.0, 0.0, 0.0, 0.0 } }, 0, { 0, { 0.0, 0.0 } }, 2.0, 3.0, 1.6, 2.2, { 4.2, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 0, { { { 0 } } }, 0 };
static const DtFn S6 = { DT_VENT, "f_vent", NULL, NULL, NULL, "lava", "basalt", NULL, 0, { 0, { 0.0, 0.0, 0.0, 0.0, 0.0 } }, 0, { 0, { 0.0, 0.0 } }, 0.0, 0.0, 0.0, 0.0, { 0.0, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 0, { { { 0 } } }, 0 };
static const DtFn S7 = { DT_SCATTER, "scatter", "scatter", "obsidian", "tri", "obsidian", NULL, "mound", 1, { 10, { 0.7, 1.0, 0.0, 0.0, 0.0 } }, 0, { 0, { 0.0, 0.0 } }, 4.0, 6.0, 1.0, 1.6, { 1.6, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 2, { { { 0.6, 0, 0 }, { 4.0, 1, 0 } }, { { 0.4, 0, 0 }, { 30.0, 1, 0 } } }, 0 };
static const DtFn S8 = { DT_SINGLE, "single", "single", "obsidian", "tri", "obsidian", NULL, "mound", 1, { 10, { 1.5, 2.0, 0.0, 0.0, 0.0 } }, 0, { 0, { 0.0, 0.0 } }, 0.0, 0.0, 1.8, 2.6, { 0.0, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 2, { { { 0.6, 0, 0 }, { 4.0, 1, 0 } }, { { 0.4, 0, 0 }, { 25.0, 1, 0 } } }, 0 };
static const DtFn S9 = { DT_SUNK, "sunk", "sunk", "lava", "star", "lava", NULL, NULL, 1, { 7, { 1.4, 1.9, 5.0, 7.0, 0.0 } }, 0, { 0, { 0.0, 0.0 } }, 1.0, 1.0, 0.4, 0.5, { 0.0, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 0, { { { 0 } } }, 0 };
static const DtFn S10 = { DT_PATH, "path", "path", "lava", "", "lava", NULL, "cut", 0, { 0, { 0.0, 0.0, 0.0, 0.0, 0.0 } }, 0, { 0, { 0.0, 0.0 } }, 0.0, 0.0, 0.0, 0.0, { 0.5, 6.0, 10.0, 1.0 }, { 0.45, NUM_FLOAT, 0 }, 0, { { { 0 } } }, 0 };
static const DtFn S11 = { DT_COLUMN, "f_column", NULL, NULL, NULL, "basalt", NULL, NULL, 0, { 0, { 0.0, 0.0, 0.0, 0.0, 0.0 } }, 0, { 0, { 0.0, 0.0 } }, 0.0, 0.0, 0.0, 0.0, { 0.0, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 0, { { { 0 } } }, 1 };
static const DtFn S12 = { DT_SINGLE, "single", "single", "basalt", "ring", "basalt", NULL, "up", 1, { 12, { 1.6, 2.0, 0.7, 0.0, 0.0 } }, 0, { 0, { 0.0, 0.0 } }, 0.0, 0.0, 0.8, 1.0, { 0.0, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 2, { { { 0.75, 0, 0 }, { 5.0, 1, 0 } }, { { 0.25, 0, 0 }, { 40.0, 1, 0 } } }, 0 };
static const DtFn S13 = { DT_POST_ROW, "f_post_row", NULL, NULL, NULL, "iron", NULL, NULL, 0, { 0, { 0.0, 0.0, 0.0, 0.0, 0.0 } }, 0, { 0, { 0.0, 0.0 } }, 2.0, 2.0, 1.4, 2.0, { 4.5, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 0, { { { 0 } } }, 1 };
static const DtFn S14 = { DT_SCATTER, "scatter", "scatter", "crystal", "hex", "crystal", NULL, "up", 1, { 9, { 0.7, 1.0, 0.0, 0.0, 0.0 } }, 0, { 0, { 0.0, 0.0 } }, 3.0, 5.0, 0.9, 1.5, { 1.6, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 2, { { { 0.7, 0, 0 }, { 6.0, 1, 0 } }, { { 0.3, 0, 0 }, { 40.0, 1, 0 } } }, 0 };
static const DtFn S15 = { DT_SINGLE, "single", "single", "alien", "rect", "alien", NULL, "up", 1, { 4, { 2.6, 3.4, 1.6, 2.0, 0.0 } }, 1, { 0, { 3.0, 0.35 } }, 0.0, 0.0, 0.8, 1.1, { 0.0, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 2, { { { 0.75, 0, 0 }, { 5.0, 1, 0 } }, { { 0.25, 0, 0 }, { 40.0, 1, 0 } } }, 0 };
static const DtFn S16 = { DT_SINGLE, "single", "single", "alien", "disc", "alien", NULL, "up", 1, { 2, { 1.3, 1.6, 24.0, 0.0, 0.0 } }, 1, { 2, { 0.3, 0.0 } }, 0.0, 0.0, 0.6, 0.9, { 0.0, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 2, { { { 0.75, 0, 0 }, { 5.0, 1, 0 } }, { { 0.25, 0, 0 }, { 40.0, 1, 0 } } }, 0 };
static const DtFn S17 = { DT_PATH, "path", "path", "cable", "", "cable", NULL, "up", 0, { 0, { 0.0, 0.0, 0.0, 0.0, 0.0 } }, 0, { 0, { 0.0, 0.0 } }, 0.0, 0.0, 0.0, 0.0, { 0.6, 7.0, 11.0, 0.6 }, { 0.4, NUM_FLOAT, 0 }, 0, { { { 0 } } }, 0 };
static const DtFn S18 = { DT_SUNK, "sunk", "sunk", "slot", "ring", "slot", NULL, NULL, 1, { 12, { 1.4, 1.8, 0.4, 0.0, 0.0 } }, 0, { 0, { 0.0, 0.0 } }, 1.0, 1.0, 0.35, 0.45, { 0.0, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 0, { { { 0 } } }, 0 };
static const DtFn S19 = { DT_COLUMN, "f_column", NULL, NULL, NULL, "alien", NULL, NULL, 0, { 0, { 0.0, 0.0, 0.0, 0.0, 0.0 } }, 0, { 0, { 0.0, 0.0 } }, 0.0, 0.0, 0.0, 0.0, { 0.0, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 0, { { { 0 } } }, 1 };
static const DtFn S20 = { DT_SINGLE, "single", "single", "crystal", "hex", "crystal", NULL, "up", 1, { 9, { 1.5, 1.9, 0.0, 0.0, 0.0 } }, 0, { 0, { 0.0, 0.0 } }, 0.0, 0.0, 1.8, 2.6, { 0.0, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 2, { { { 0.75, 0, 0 }, { 5.0, 1, 0 } }, { { 0.25, 0, 0 }, { 40.0, 1, 0 } } }, 0 };
static const DtFn S21 = { DT_VENT, "f_vent", NULL, NULL, NULL, "slot", "alien", NULL, 0, { 0, { 0.0, 0.0, 0.0, 0.0, 0.0 } }, 0, { 0, { 0.0, 0.0 } }, 0.0, 0.0, 0.0, 0.0, { 0.0, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 0, { { { 0 } } }, 0 };
static const DtFn S22 = { DT_SCATTER, "scatter", "scatter", "iron", "caps", "iron", NULL, "up", 1, { 5, { 0.7, 1.0, 0.3, 0.0, 0.0 } }, 0, { 0, { 0.0, 0.0 } }, 6.0, 10.0, 0.3, 0.35, { 1.4, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 2, { { { 0.7, 0, 0 }, { 6.0, 1, 0 } }, { { 0.3, 0, 0 }, { 40.0, 1, 0 } } }, 0 };
static const DtFn S23 = { DT_POST_ROW, "f_post_row", NULL, NULL, NULL, "wire", NULL, NULL, 0, { 0, { 0.0, 0.0, 0.0, 0.0, 0.0 } }, 0, { 0, { 0.0, 0.0 } }, 2.0, 3.0, 1.4, 1.9, { 4.0, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 0, { { { 0 } } }, 1 };
static const DtFn S24 = { DT_SINGLE, "single", "single", "crate", "rect", "crate", NULL, "up", 1, { 4, { 2.6, 3.2, 1.6, 2.0, 0.0 } }, 1, { 1, { 0.8, 0.0 } }, 0.0, 0.0, 1.0, 1.3, { 0.0, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 2, { { { 0.75, 0, 0 }, { 5.0, 1, 0 } }, { { 0.25, 0, 0 }, { 40.0, 1, 0 } } }, 0 };
static const DtFn S25 = { DT_SUNK, "sunk", "sunk", "water", "blob", "water", NULL, NULL, 1, { 13, { 1.5, 2.1, 0.0, 0.0, 0.0 } }, 0, { 0, { 0.0, 0.0 } }, 1.0, 1.0, 0.5, 0.7, { 0.0, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 0, { { { 0 } } }, 0 };
static const DtFn S26 = { DT_SINGLE, "single", "single", "iron", "disc", "iron", NULL, "up", 1, { 2, { 0.9, 1.1, 20.0, 0.0, 0.0 } }, 1, { 2, { 0.25, 0.0 } }, 0.0, 0.0, 1.6, 2.0, { 0.0, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 2, { { { 0.75, 0, 0 }, { 5.0, 1, 0 } }, { { 0.25, 0, 0 }, { 40.0, 1, 0 } } }, 0 };
static const DtFn S27 = { DT_PATH, "path", "path", "wire", "", "wire", NULL, "up", 0, { 0, { 0.0, 0.0, 0.0, 0.0, 0.0 } }, 0, { 0, { 0.0, 0.0 } }, 0.0, 0.0, 0.0, 0.0, { 0.45, 8.0, 12.0, 1.3 }, { 0.45, NUM_FLOAT, 0 }, 0, { { { 0 } } }, 0 };
static const DtFn S28 = { DT_SCATTER, "scatter", "scatter", "garden", "round", "garden", NULL, "mound", 1, { 1, { 0.9, 1.4, 0.0, 0.0, 0.0 } }, 0, { 0, { 0.0, 0.0 } }, 3.0, 5.0, 0.5, 0.8, { 2.2, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 2, { { { 0.7, 0, 0 }, { 6.0, 1, 0 } }, { { 0.3, 0, 0 }, { 40.0, 1, 0 } } }, 0 };
static const DtFn S29 = { DT_SINGLE, "single", "single", "plank", "rect", "plank", NULL, "up", 1, { 4, { 3.0, 3.8, 1.2, 1.5, 0.0 } }, 1, { 1, { 0.9, 0.0 } }, 0.0, 0.0, 0.8, 1.0, { 0.0, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 2, { { { 0.75, 0, 0 }, { 5.0, 1, 0 } }, { { 0.25, 0, 0 }, { 40.0, 1, 0 } } }, 0 };
static const DtFn S30 = { DT_SINGLE, "single", "single", "wood", "disc", "wood", NULL, "up", 1, { 2, { 1.0, 1.3, 24.0, 0.0, 0.0 } }, 1, { 2, { 0.25, 0.0 } }, 0.0, 0.0, 1.2, 1.6, { 0.0, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 2, { { { 0.75, 0, 0 }, { 5.0, 1, 0 } }, { { 0.25, 0, 0 }, { 40.0, 1, 0 } } }, 0 };
static const DtFn S31 = { DT_POST_ROW, "f_post_row", NULL, NULL, NULL, "wood", NULL, NULL, 0, { 0, { 0.0, 0.0, 0.0, 0.0, 0.0 } }, 0, { 0, { 0.0, 0.0 } }, 2.0, 2.0, 1.8, 2.4, { 5.0, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 0, { { { 0 } } }, 1 };
static const DtFn S32 = { DT_SINGLE, "single", "single", "tatami", "rect", "tatami", NULL, "up", 1, { 4, { 2.2, 2.8, 2.2, 2.8, 0.0 } }, 1, { 3, { 0.0, 0.0 } }, 0.0, 0.0, 0.5, 0.6, { 0.0, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 2, { { { 0.75, 0, 0 }, { 5.0, 1, 0 } }, { { 0.25, 0, 0 }, { 40.0, 1, 0 } } }, 0 };
static const DtFn S33 = { DT_SCATTER, "scatter", "scatter", "iron", "caps", "iron", NULL, "up", 1, { 5, { 3.0, 4.0, 0.4, 0.0, 0.0 } }, 0, { 0, { 0.0, 0.0 } }, 1.0, 2.0, 0.4, 0.5, { 1.5, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 2, { { { 0.7, 0, 0 }, { 6.0, 1, 0 } }, { { 0.3, 0, 0 }, { 40.0, 1, 0 } } }, 0 };
static const DtFn S34 = { DT_PATH, "path", "path", "vein", "", "vein", NULL, "up", 0, { 0, { 0.0, 0.0, 0.0, 0.0, 0.0 } }, 0, { 0, { 0.0, 0.0 } }, 0.0, 0.0, 0.0, 0.0, { 0.7, 7.0, 11.0, 1.3 }, { 0.6, NUM_FLOAT, 0 }, 0, { { { 0 } } }, 0 };
static const DtFn S35 = { DT_SCATTER, "scatter", "scatter", "pustule", "disc", "pustule", NULL, "mound", 1, { 2, { 0.6, 0.9, 16.0, 0.0, 0.0 } }, 0, { 0, { 0.0, 0.0 } }, 3.0, 6.0, 0.7, 1.0, { 1.6, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 2, { { { 0.55, 0, 0 }, { 25.0, 1, 0 } }, { { 0.45, 0, 0 }, { 50.0, 1, 0 } } }, 0 };
static const DtFn S36 = { DT_SUNK, "sunk", "sunk", "bile", "blob", "bile", NULL, NULL, 1, { 13, { 1.4, 2.0, 0.0, 0.0, 0.0 } }, 0, { 0, { 0.0, 0.0 } }, 1.0, 1.0, 0.45, 0.6, { 0.0, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 0, { { { 0 } } }, 0 };
static const DtFn S37 = { DT_PATH, "path", "path", "vein", "", "vein", NULL, "up", 0, { 0, { 0.0, 0.0, 0.0, 0.0, 0.0 } }, 0, { 0, { 0.0, 0.0 } }, 0.0, 0.0, 0.0, 0.0, { 0.5, 8.0, 12.0, 1.5 }, { 0.5, NUM_FLOAT, 0 }, 0, { { { 0 } } }, 0 };
static const DtFn S38 = { DT_VENT, "f_vent", NULL, NULL, NULL, "bile", "flesh", NULL, 0, { 0, { 0.0, 0.0, 0.0, 0.0, 0.0 } }, 0, { 0, { 0.0, 0.0 } }, 0.0, 0.0, 0.0, 0.0, { 0.0, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 0, { { { 0 } } }, 0 };
static const DtFn S39 = { DT_SCATTER, "scatter", "scatter", "bone", "bone", "bone", NULL, "up", 1, { 6, { 1.6, 2.2, 0.0, 0.0, 0.0 } }, 0, { 0, { 0.0, 0.0 } }, 2.0, 4.0, 0.4, 0.5, { 1.6, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 2, { { { 0.7, 0, 0 }, { 6.0, 1, 0 } }, { { 0.3, 0, 0 }, { 40.0, 1, 0 } } }, 0 };
static const DtFn S40 = { DT_SINGLE, "single", "single", "barrel", "disc", "barrel", NULL, "up", 1, { 2, { 1.0, 1.25, 24.0, 0.0, 0.0 } }, 1, { 2, { 0.25, 0.0 } }, 0.0, 0.0, 1.5, 1.9, { 0.0, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 2, { { { 0.75, 0, 0 }, { 5.0, 1, 0 } }, { { 0.25, 0, 0 }, { 40.0, 1, 0 } } }, 0 };
static const DtFn S41 = { DT_POST_ROW, "f_post_row", NULL, NULL, NULL, "post", NULL, NULL, 0, { 0, { 0.0, 0.0, 0.0, 0.0, 0.0 } }, 0, { 0, { 0.0, 0.0 } }, 2.0, 2.0, 1.8, 2.3, { 5.0, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 0, { { { 0 } } }, 1 };
static const DtFn S42 = { DT_SINGLE, "single", "single", "crate", "rect", "crate", NULL, "up", 1, { 4, { 2.6, 3.4, 1.8, 2.2, 0.0 } }, 1, { 1, { 0.8, 0.0 } }, 0.0, 0.0, 1.1, 1.4, { 0.0, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 2, { { { 0.75, 0, 0 }, { 5.0, 1, 0 } }, { { 0.25, 0, 0 }, { 40.0, 1, 0 } } }, 0 };
static const DtFn S43 = { DT_TRAIL, "trail", "trail", "pit", "crescent", "pit", NULL, NULL, 1, { 8, { 0.45, 0.55, 0.0, 0.0, 0.0 } }, 0, { 0, { 0.0, 0.0 } }, 5.0, 8.0, 0.0, 0.0, { 1.5, 0.0, 0.0, 0.0 }, { 0.3, NUM_FLOAT, 0 }, 0, { { { 0 } } }, 0 };
static const DtFn S44 = { DT_SCATTER, "scatter", "scatter", "board", "caps", "board", NULL, "up", 1, { 5, { 2.4, 3.2, 0.5, 0.0, 0.0 } }, 0, { 0, { 0.0, 0.0 } }, 2.0, 3.0, 0.5, 0.6, { 1.6, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 2, { { { 0.7, 0, 0 }, { 6.0, 1, 0 } }, { { 0.3, 0, 0 }, { 40.0, 1, 0 } } }, 0 };
static const DtFn S45 = { DT_SINGLE, "single", "single", "iron", "ring", "iron", NULL, "up", 1, { 12, { 1.8, 2.2, 0.5, 0.0, 0.0 } }, 0, { 0, { 0.0, 0.0 } }, 0.0, 0.0, 0.5, 0.6, { 0.0, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 2, { { { 0.75, 0, 0 }, { 5.0, 1, 0 } }, { { 0.25, 0, 0 }, { 40.0, 1, 0 } } }, 0 };
static const DtFn S46 = { DT_SCATTER, "scatter", "scatter", "barrel", "disc", "barrel", NULL, "up", 1, { 2, { 0.9, 1.1, 20.0, 0.0, 0.0 } }, 0, { 0, { 0.0, 0.0 } }, 2.0, 3.0, 1.4, 1.8, { 2.2, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 2, { { { 0.7, 0, 0 }, { 6.0, 1, 0 } }, { { 0.3, 0, 0 }, { 40.0, 1, 0 } } }, 0 };
static const DtFn S47 = { DT_SINGLE, "single", "single", "board", "rect", "board", NULL, "up", 1, { 4, { 3.4, 4.2, 1.2, 1.5, 0.0 } }, 1, { 1, { 1.0, 0.0 } }, 0.0, 0.0, 0.9, 1.1, { 0.0, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 2, { { { 0.75, 0, 0 }, { 5.0, 1, 0 } }, { { 0.25, 0, 0 }, { 40.0, 1, 0 } } }, 0 };
static const DtFn S48 = { DT_SCATTER, "scatter", "scatter", "bone", "bone", "bone", NULL, "up", 1, { 6, { 1.5, 2.1, 0.0, 0.0, 0.0 } }, 0, { 0, { 0.0, 0.0 } }, 2.0, 3.0, 0.4, 0.5, { 1.6, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 2, { { { 0.7, 0, 0 }, { 6.0, 1, 0 } }, { { 0.3, 0, 0 }, { 40.0, 1, 0 } } }, 0 };
static const DtFn S49 = { DT_POST_ROW, "f_post_row", NULL, NULL, NULL, "fence", NULL, NULL, 0, { 0, { 0.0, 0.0, 0.0, 0.0, 0.0 } }, 0, { 0, { 0.0, 0.0 } }, 2.0, 3.0, 1.8, 2.4, { 4.0, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 0, { { { 0 } } }, 1 };
static const DtFn S50 = { DT_SINGLE, "single", "single", "headstone", "cross", "headstone", NULL, "up", 1, { 11, { 2.6, 3.2, 0.8, 0.0, 0.0 } }, 0, { 0, { 0.0, 0.0 } }, 0.0, 0.0, 0.5, 0.7, { 0.0, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 2, { { { 0.75, 0, 0 }, { 5.0, 1, 0 } }, { { 0.25, 0, 0 }, { 40.0, 1, 0 } } }, 0 };
static const DtFn S51 = { DT_SINGLE, "single", "single", "earth", "blob", "earth", NULL, "mound", 1, { 13, { 1.8, 2.4, 0.0, 0.0, 0.0 } }, 0, { 0, { 0.0, 0.0 } }, 0.0, 0.0, 0.7, 1.0, { 0.0, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 2, { { { 0.55, 0, 0 }, { 25.0, 1, 0 } }, { { 0.45, 0, 0 }, { 50.0, 1, 0 } } }, 0 };
static const DtFn S52 = { DT_SINGLE, "single", "single", "bone", "ell", "bone", NULL, "mound", 1, { 3, { 1.1, 1.4, 0.8, 1.0, 0.0 } }, 1, { 4, { 0.22, 0.0 } }, 0.0, 0.0, 0.7, 0.9, { 0.0, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 2, { { { 0.75, 0, 0 }, { 5.0, 1, 0 } }, { { 0.25, 0, 0 }, { 40.0, 1, 0 } } }, 0 };
static const DtFn S53 = { DT_SCATTER, "scatter", "scatter", "shroom", "disc", "shroom", NULL, "mound", 1, { 2, { 0.55, 0.8, 14.0, 0.0, 0.0 } }, 0, { 0, { 0.0, 0.0 } }, 3.0, 6.0, 0.6, 0.9, { 1.5, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 2, { { { 0.5, 0, 0 }, { 5.0, 1, 0 } }, { { 0.5, 0, 0 }, { 50.0, 1, 0 } } }, 0 };
static const DtFn S54 = { DT_SCATTER, "scatter", "scatter", "shrub", "star", "shrub", NULL, "mound", 1, { 7, { 1.0, 1.4, 6.0, 9.0, 0.0 } }, 0, { 0, { 0.0, 0.0 } }, 2.0, 4.0, 0.9, 1.3, { 2.0, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 2, { { { 0.55, 0, 0 }, { 25.0, 1, 0 } }, { { 0.45, 0, 0 }, { 50.0, 1, 0 } } }, 0 };
static const DtFn S55 = { DT_SINGLE, "single", "single", "moss", "blob", "moss", NULL, "mound", 1, { 13, { 1.6, 2.2, 0.0, 0.0, 0.0 } }, 0, { 0, { 0.0, 0.0 } }, 0.0, 0.0, 0.6, 0.9, { 0.0, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 2, { { { 0.55, 0, 0 }, { 25.0, 1, 0 } }, { { 0.45, 0, 0 }, { 50.0, 1, 0 } } }, 0 };
static const DtFn S56 = { DT_SCATTER, "scatter", "scatter", "leaf", "ell", "leaf", NULL, "up", 1, { 3, { 0.5, 0.7, 0.9, 1.2, 0.0 } }, 0, { 0, { 0.0, 0.0 } }, 5.0, 8.0, 0.3, 0.35, { 1.8, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 2, { { { 0.7, 0, 0 }, { 6.0, 1, 0 } }, { { 0.3, 0, 0 }, { 40.0, 1, 0 } } }, 0 };
static const DtFn S57 = { DT_SCATTER, "scatter", "scatter", "bone", "bone", "bone", NULL, "up", 1, { 6, { 1.5, 2.1, 0.0, 0.0, 0.0 } }, 0, { 0, { 0.0, 0.0 } }, 2.0, 4.0, 0.4, 0.5, { 1.6, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 2, { { { 0.7, 0, 0 }, { 6.0, 1, 0 } }, { { 0.3, 0, 0 }, { 40.0, 1, 0 } } }, 0 };
static const DtFn S58 = { DT_SINGLE, "single", "single", "iron", "disc", "iron", NULL, "up", 1, { 2, { 1.4, 1.7, 24.0, 0.0, 0.0 } }, 1, { 0, { 4.0, 0.3 } }, 0.0, 0.0, 0.4, 0.5, { 0.0, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 2, { { { 0.75, 0, 0 }, { 5.0, 1, 0 } }, { { 0.25, 0, 0 }, { 40.0, 1, 0 } } }, 0 };
static const DtFn S59 = { DT_PATH, "path", "path", "chain", "", "chain", NULL, "up", 0, { 0, { 0.0, 0.0, 0.0, 0.0, 0.0 } }, 0, { 0, { 0.0, 0.0 } }, 0.0, 0.0, 0.0, 0.0, { 0.6, 7.0, 11.0, 1.0 }, { 0.4, NUM_FLOAT, 0 }, 0, { { { 0 } } }, 0 };
static const DtFn S60 = { DT_SINGLE, "single", "single", "crate", "rect", "crate", NULL, "up", 1, { 4, { 2.6, 3.2, 1.6, 2.0, 0.0 } }, 1, { 1, { 0.8, 0.0 } }, 0.0, 0.0, 1.0, 1.3, { 0.0, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 2, { { { 0.75, 0, 0 }, { 5.0, 1, 0 } }, { { 0.25, 0, 0 }, { 40.0, 1, 0 } } }, 0 };
static const DtFn S61 = { DT_COLUMN, "f_column", NULL, NULL, NULL, "flag", NULL, NULL, 0, { 0, { 0.0, 0.0, 0.0, 0.0, 0.0 } }, 0, { 0, { 0.0, 0.0 } }, 0.0, 0.0, 0.0, 0.0, { 0.0, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 0, { { { 0 } } }, 1 };
static const DtFn S62 = { DT_SINGLE, "single", "single", "iron", "ring", "iron", NULL, "up", 1, { 12, { 1.3, 1.6, 0.45, 0.0, 0.0 } }, 0, { 0, { 0.0, 0.0 } }, 0.0, 0.0, 0.5, 0.6, { 0.0, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 2, { { { 0.75, 0, 0 }, { 5.0, 1, 0 } }, { { 0.25, 0, 0 }, { 40.0, 1, 0 } } }, 0 };
static const DtFn S63 = { DT_SUNK, "sunk", "sunk", "water", "blob", "water", NULL, NULL, 1, { 13, { 1.4, 2.0, 0.0, 0.0, 0.0 } }, 0, { 0, { 0.0, 0.0 } }, 1.0, 1.0, 0.3, 0.4, { 0.0, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 0, { { { 0 } } }, 0 };
static const DtFn S64 = { DT_COLUMN, "f_column", NULL, NULL, NULL, "tcolumn", NULL, NULL, 0, { 0, { 0.0, 0.0, 0.0, 0.0, 0.0 } }, 0, { 0, { 0.0, 0.0 } }, 0.0, 0.0, 0.0, 0.0, { 0.0, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 0, { { { 0 } } }, 1 };
static const DtFn S65 = { DT_SCATTER, "scatter", "scatter", "tcolumn", "rect", "tcolumn", NULL, "up", 1, { 4, { 1.4, 2.0, 1.0, 1.4, 0.0 } }, 0, { 0, { 0.0, 0.0 } }, 2.0, 4.0, 0.6, 0.9, { 2.0, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 2, { { { 0.7, 0, 0 }, { 6.0, 1, 0 } }, { { 0.3, 0, 0 }, { 40.0, 1, 0 } } }, 0 };
static const DtFn S66 = { DT_SINGLE, "single", "single", "tflag", "hex", "tflag", NULL, "up", 1, { 9, { 1.5, 1.9, 0.0, 0.0, 0.0 } }, 1, { 6, { 0.0, 0.0 } }, 0.0, 0.0, 0.7, 0.9, { 0.0, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 2, { { { 0.75, 0, 0 }, { 5.0, 1, 0 } }, { { 0.25, 0, 0 }, { 40.0, 1, 0 } } }, 0 };
static const DtFn S67 = { DT_SINGLE, "single", "single", "tcolumn", "caps", "tcolumn", NULL, "up", 1, { 5, { 4.5, 6.0, 1.9, 0.0, 0.0 } }, 1, { 1, { 1.5, 0.0 } }, 0.0, 0.0, 1.0, 1.3, { 0.0, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 2, { { { 0.75, 0, 0 }, { 5.0, 1, 0 } }, { { 0.25, 0, 0 }, { 40.0, 1, 0 } } }, 0 };
static const DtFn S68 = { DT_SUNK, "sunk", "sunk", "glyph", "ring", "glyph", NULL, NULL, 1, { 12, { 1.6, 2.0, 0.4, 0.0, 0.0 } }, 0, { 0, { 0.0, 0.0 } }, 1.0, 1.0, 0.35, 0.45, { 0.0, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 0, { { { 0 } } }, 0 };
static const DtFn S69 = { DT_COLUMN, "f_column", NULL, NULL, NULL, "tcolumn", NULL, NULL, 0, { 0, { 0.0, 0.0, 0.0, 0.0, 0.0 } }, 0, { 0, { 0.0, 0.0 } }, 0.0, 0.0, 0.0, 0.0, { 0.0, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 0, { { { 0 } } }, 1 };
static const DtFn S70 = { DT_VENT, "f_vent", NULL, NULL, NULL, "pit", "tflag", NULL, 0, { 0, { 0.0, 0.0, 0.0, 0.0, 0.0 } }, 0, { 0, { 0.0, 0.0 } }, 0.0, 0.0, 0.0, 0.0, { 0.0, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 0, { { { 0 } } }, 0 };
static const DtFn S71 = { DT_SCATTER, "scatter", "scatter", "gold", "disc", "gold", NULL, "up", 1, { 2, { 0.4, 0.5, 12.0, 0.0, 0.0 } }, 0, { 0, { 0.0, 0.0 } }, 5.0, 9.0, 0.3, 0.35, { 1.2, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 2, { { { 0.7, 0, 0 }, { 6.0, 1, 0 } }, { { 0.3, 0, 0 }, { 40.0, 1, 0 } } }, 0 };
static const DtFn S72 = { DT_SCATTER, "scatter", "scatter", "shell", "crescent", "shell", NULL, "up", 1, { 8, { 0.6, 0.9, 0.0, 0.0, 0.0 } }, 0, { 0, { 0.0, 0.0 } }, 3.0, 6.0, 0.35, 0.45, { 1.6, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 2, { { { 0.7, 0, 0 }, { 6.0, 1, 0 } }, { { 0.3, 0, 0 }, { 40.0, 1, 0 } } }, 0 };
static const DtFn S73 = { DT_SINGLE, "single", "single", "drift", "caps", "drift", NULL, "up", 1, { 5, { 5.0, 7.0, 1.0, 0.0, 0.0 } }, 1, { 1, { 1.8, 0.0 } }, 0.0, 0.0, 0.6, 0.8, { 0.0, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 2, { { { 0.75, 0, 0 }, { 5.0, 1, 0 } }, { { 0.25, 0, 0 }, { 40.0, 1, 0 } } }, 0 };
static const DtFn S74 = { DT_PATH, "path", "path", "rope", "", "rope", NULL, "up", 0, { 0, { 0.0, 0.0, 0.0, 0.0, 0.0 } }, 0, { 0, { 0.0, 0.0 } }, 0.0, 0.0, 0.0, 0.0, { 0.6, 7.0, 11.0, 1.2 }, { 0.45, NUM_FLOAT, 0 }, 0, { { { 0 } } }, 0 };
static const DtFn S75 = { DT_SCATTER, "scatter", "scatter", "shell", "star", "shell", NULL, "mound", 1, { 7, { 0.8, 1.1, 5.0, 5.0, 0.0 } }, 0, { 0, { 0.0, 0.0 } }, 1.0, 3.0, 0.4, 0.5, { 2.0, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 2, { { { 0.5, 0, 0 }, { 15.0, 1, 0 } }, { { 0.5, 0, 0 }, { 45.0, 1, 0 } } }, 0 };
static const DtFn S76 = { DT_SUNK, "sunk", "sunk", "water", "blob", "water", NULL, NULL, 1, { 13, { 1.4, 2.0, 0.0, 0.0, 0.0 } }, 0, { 0, { 0.0, 0.0 } }, 1.0, 1.0, 0.4, 0.55, { 0.0, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 0, { { { 0 } } }, 0 };
static const DtFn S77 = { DT_SINGLE, "single", "single", "iron", "ring", "iron", NULL, "up", 1, { 12, { 1.5, 1.9, 0.6, 0.0, 0.0 } }, 0, { 0, { 0.0, 0.0 } }, 0.0, 0.0, 0.6, 0.8, { 0.0, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 2, { { { 0.75, 0, 0 }, { 5.0, 1, 0 } }, { { 0.25, 0, 0 }, { 40.0, 1, 0 } } }, 0 };
static const DtFn S78 = { DT_POST_ROW, "f_post_row", NULL, NULL, NULL, "drift", NULL, NULL, 0, { 0, { 0.0, 0.0, 0.0, 0.0, 0.0 } }, 0, { 0, { 0.0, 0.0 } }, 2.0, 3.0, 1.6, 2.2, { 3.2, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 0, { { { 0 } } }, 0 };
static const DtFn S79 = { DT_SCATTER, "scatter", "scatter", "weed", "wig", "weed", NULL, "up", 1, { 14, { 2.5, 3.5, 0.5, 0.6, 0.0 } }, 0, { 0, { 0.0, 0.0 } }, 2.0, 4.0, 0.4, 0.5, { 1.8, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 2, { { { 0.7, 0, 0 }, { 6.0, 1, 0 } }, { { 0.3, 0, 0 }, { 40.0, 1, 0 } } }, 0 };
static const DtFn S80 = { DT_SCATTER, "scatter", "scatter", "rock", "rock", "rock", NULL, "mound", 1, { 0, { 0.9, 1.4, 5.0, 8.0, 0.0 } }, 0, { 0, { 0.0, 0.0 } }, 3.0, 5.0, 0.6, 1.0, { 1.6, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 2, { { { 0.7, 0, 0 }, { 6.0, 1, 0 } }, { { 0.3, 0, 0 }, { 40.0, 1, 0 } } }, 0 };
static const DtFn S81 = { DT_SCATTER, "scatter", "scatter", "pebble", "round", "pebble", NULL, "mound", 1, { 1, { 0.5, 0.75, 0.0, 0.0, 0.0 } }, 0, { 0, { 0.0, 0.0 } }, 6.0, 10.0, 0.35, 0.5, { 1.6, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 2, { { { 0.7, 0, 0 }, { 10.0, 1, 0 } }, { { 0.3, 0, 0 }, { 45.0, 1, 0 } } }, 0 };
static const DtFn S82 = { DT_SINGLE, "single", "single", "rock", "rock", "rock", NULL, "mound", 1, { 0, { 1.8, 2.5, 5.0, 8.0, 0.0 } }, 0, { 0, { 0.0, 0.0 } }, 0.0, 0.0, 1.1, 1.7, { 0.0, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 2, { { { 0.7, 0, 0 }, { 6.0, 1, 0 } }, { { 0.3, 0, 0 }, { 40.0, 1, 0 } } }, 0 };
static const DtFn S83 = { DT_SCATTER, "scatter", "scatter", "rock", "tri", "rock", NULL, "mound", 1, { 10, { 0.7, 1.1, 0.0, 0.0, 0.0 } }, 0, { 0, { 0.0, 0.0 } }, 3.0, 6.0, 0.5, 0.8, { 1.8, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 2, { { { 0.7, 0, 0 }, { 6.0, 1, 0 } }, { { 0.3, 0, 0 }, { 40.0, 1, 0 } } }, 0 };
static const DtFn S84 = { DT_SUNK, "sunk", "sunk", "water", "blob", "water", NULL, NULL, 1, { 13, { 1.3, 1.9, 0.0, 0.0, 0.0 } }, 0, { 0, { 0.0, 0.0 } }, 1.0, 1.0, 0.3, 0.45, { 0.0, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 0, { { { 0 } } }, 0 };
static const DtFn S85 = { DT_SUNK, "sunk", "sunk", "crack", "wig", "crack", NULL, NULL, 1, { 14, { 5.0, 8.0, 0.45, 0.9, 0.0 } }, 0, { 0, { 0.0, 0.0 } }, 1.0, 1.0, 0.4, 0.5, { 0.0, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 0, { { { 0 } } }, 0 };
static const DtFn S86 = { DT_TRAIL, "trail", "trail", "pit", "ell", "pit", NULL, NULL, 1, { 3, { 0.35, 0.45, 0.6, 0.75, 0.0 } }, 0, { 0, { 0.0, 0.0 } }, 5.0, 8.0, 0.0, 0.0, { 1.3, 0.0, 0.0, 0.0 }, { 0.35, NUM_FLOAT, 0 }, 0, { { { 0 } } }, 0 };
static const DtFn S87 = { DT_SUNK, "sunk", "sunk", "pit", "disc", "pit", NULL, NULL, 1, { 2, { 0.4, 0.55, 10.0, 0.0, 0.0 } }, 0, { 0, { 0.0, 0.0 } }, 4.0, 7.0, 0.3, 0.4, { 1.5, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 0, { { { 0 } } }, 0 };
static const DtFn S88 = { DT_SCATTER, "scatter", "scatter", "bone", "bone", "bone", NULL, "up", 1, { 6, { 1.5, 2.2, 0.0, 0.0, 0.0 } }, 0, { 0, { 0.0, 0.0 } }, 2.0, 4.0, 0.4, 0.5, { 1.6, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 2, { { { 0.7, 0, 0 }, { 6.0, 1, 0 } }, { { 0.3, 0, 0 }, { 40.0, 1, 0 } } }, 0 };
static const DtFn S89 = { DT_SINGLE, "single", "single", "bone", "ell", "bone", NULL, "mound", 1, { 3, { 1.1, 1.4, 0.7, 0.9, 0.0 } }, 1, { 4, { 0.22, 0.0 } }, 0.0, 0.0, 0.6, 0.8, { 0.0, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 2, { { { 0.75, 0, 0 }, { 5.0, 1, 0 } }, { { 0.25, 0, 0 }, { 40.0, 1, 0 } } }, 0 };
static const DtFn S90 = { DT_SCATTER, "scatter", "scatter", "reed", "star", "reed", NULL, "mound", 1, { 7, { 0.9, 1.3, 5.0, 8.0, 0.0 } }, 0, { 0, { 0.0, 0.0 } }, 2.0, 4.0, 0.8, 1.2, { 2.0, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 2, { { { 0.55, 0, 0 }, { 25.0, 1, 0 } }, { { 0.45, 0, 0 }, { 50.0, 1, 0 } } }, 0 };
static const DtFn S91 = { DT_SINGLE, "single", "single", "reed", "star", "reed", NULL, "mound", 1, { 7, { 1.5, 2.1, 6.0, 9.0, 0.0 } }, 0, { 0, { 0.0, 0.0 } }, 0.0, 0.0, 1.0, 1.4, { 0.0, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 2, { { { 0.55, 0, 0 }, { 25.0, 1, 0 } }, { { 0.45, 0, 0 }, { 50.0, 1, 0 } } }, 0 };
static const DtFn S92 = { DT_SINGLE, "single", "single", "crate", "rect", "crate", NULL, "up", 1, { 4, { 2.4, 3.2, 1.6, 2.0, 0.0 } }, 1, { 1, { 0.8, 0.0 } }, 0.0, 0.0, 1.0, 1.4, { 0.0, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 2, { { { 0.75, 0, 0 }, { 5.0, 1, 0 } }, { { 0.25, 0, 0 }, { 40.0, 1, 0 } } }, 0 };
static const DtFn S93 = { DT_SCATTER, "scatter", "scatter", "crate", "rect", "crate", NULL, "up", 1, { 4, { 1.2, 1.6, 1.0, 1.3, 0.0 } }, 0, { 0, { 0.0, 0.0 } }, 2.0, 3.0, 0.8, 1.1, { 1.8, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 2, { { { 0.7, 0, 0 }, { 6.0, 1, 0 } }, { { 0.3, 0, 0 }, { 40.0, 1, 0 } } }, 0 };
static const DtFn S94 = { DT_SINGLE, "single", "single", "log", "disc", "log", NULL, "up", 1, { 2, { 0.9, 1.2, 24.0, 0.0, 0.0 } }, 1, { 2, { 0.25, 0.0 } }, 0.0, 0.0, 1.4, 1.8, { 0.0, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 2, { { { 0.75, 0, 0 }, { 5.0, 1, 0 } }, { { 0.25, 0, 0 }, { 40.0, 1, 0 } } }, 0 };
static const DtFn S95 = { DT_SCATTER, "scatter", "scatter", "log", "caps", "log", NULL, "up", 1, { 5, { 2.4, 3.4, 0.55, 0.0, 0.0 } }, 0, { 0, { 0.0, 0.0 } }, 2.0, 3.0, 0.5, 0.6, { 1.8, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 2, { { { 0.7, 0, 0 }, { 6.0, 1, 0 } }, { { 0.3, 0, 0 }, { 40.0, 1, 0 } } }, 0 };
static const DtFn S96 = { DT_SINGLE, "single", "single", "log", "caps", "log", NULL, "up", 1, { 5, { 4.5, 6.5, 0.95, 0.0, 0.0 } }, 1, { 1, { 1.3, 0.0 } }, 0.0, 0.0, 0.6, 0.8, { 0.0, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 2, { { { 0.75, 0, 0 }, { 5.0, 1, 0 } }, { { 0.25, 0, 0 }, { 40.0, 1, 0 } } }, 0 };
static const DtFn S97 = { DT_SCATTER, "scatter", "scatter", "shroom", "disc", "shroom", NULL, "mound", 1, { 2, { 0.5, 0.7, 12.0, 0.0, 0.0 } }, 0, { 0, { 0.0, 0.0 } }, 4.0, 7.0, 0.5, 0.7, { 1.4, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 2, { { { 0.5, 0, 0 }, { 5.0, 1, 0 } }, { { 0.5, 0, 0 }, { 50.0, 1, 0 } } }, 0 };
static const DtFn S98 = { DT_SUNK, "sunk", "sunk", "glyph", "ring", "glyph", NULL, NULL, 1, { 12, { 1.3, 1.7, 0.4, 0.0, 0.0 } }, 0, { 0, { 0.0, 0.0 } }, 1.0, 1.0, 0.3, 0.4, { 0.0, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 0, { { { 0 } } }, 0 };
static const DtFn S99 = { DT_SINGLE, "single", "single", "mud", "ell", "mud", NULL, "mound", 1, { 3, { 1.5, 1.9, 0.9, 1.1, 0.0 } }, 0, { 0, { 0.0, 0.0 } }, 0.0, 0.0, 0.8, 1.1, { 0.0, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 2, { { { 0.55, 0, 0 }, { 25.0, 1, 0 } }, { { 0.45, 0, 0 }, { 50.0, 1, 0 } } }, 0 };
static const DtFn S100 = { DT_SUNK, "sunk", "sunk", "pit", "crescent", "pit", NULL, NULL, 1, { 8, { 1.0, 1.4, 0.0, 0.0, 0.0 } }, 0, { 0, { 0.0, 0.0 } }, 1.0, 1.0, 0.3, 0.4, { 0.0, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 0, { { { 0 } } }, 0 };
static const DtFn S101 = { DT_SCATTER, "scatter", "scatter", "rock", "ell", "rock", NULL, "mound", 1, { 3, { 0.6, 0.8, 0.9, 1.2, 0.0 } }, 0, { 0, { 0.0, 0.0 } }, 3.0, 5.0, 0.5, 0.7, { 1.6, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 2, { { { 0.7, 0, 0 }, { 6.0, 1, 0 } }, { { 0.3, 0, 0 }, { 40.0, 1, 0 } } }, 0 };
static const DtFn S102 = { DT_SCATTER, "scatter", "scatter", "basalt", "rock", "basalt", NULL, "mound", 1, { 0, { 0.9, 1.4, 5.0, 8.0, 0.0 } }, 0, { 0, { 0.0, 0.0 } }, 3.0, 5.0, 0.6, 1.0, { 1.6, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 2, { { { 0.7, 0, 0 }, { 6.0, 1, 0 } }, { { 0.3, 0, 0 }, { 40.0, 1, 0 } } }, 0 };
static const DtFn S103 = { DT_SCATTER, "scatter", "scatter", "pebble", "round", "pebble", NULL, "mound", 1, { 1, { 0.5, 0.75, 0.0, 0.0, 0.0 } }, 0, { 0, { 0.0, 0.0 } }, 6.0, 10.0, 0.35, 0.5, { 1.6, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 2, { { { 0.7, 0, 0 }, { 10.0, 1, 0 } }, { { 0.3, 0, 0 }, { 45.0, 1, 0 } } }, 0 };
static const DtFn S104 = { DT_SINGLE, "single", "single", "basalt", "rock", "basalt", NULL, "mound", 1, { 0, { 1.8, 2.5, 5.0, 8.0, 0.0 } }, 0, { 0, { 0.0, 0.0 } }, 0.0, 0.0, 1.1, 1.7, { 0.0, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 2, { { { 0.7, 0, 0 }, { 6.0, 1, 0 } }, { { 0.3, 0, 0 }, { 40.0, 1, 0 } } }, 0 };
static const DtFn S105 = { DT_SCATTER, "scatter", "scatter", "basalt", "tri", "basalt", NULL, "mound", 1, { 10, { 0.7, 1.1, 0.0, 0.0, 0.0 } }, 0, { 0, { 0.0, 0.0 } }, 3.0, 6.0, 0.5, 0.8, { 1.8, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 2, { { { 0.7, 0, 0 }, { 6.0, 1, 0 } }, { { 0.3, 0, 0 }, { 40.0, 1, 0 } } }, 0 };
static const DtFn S106 = { DT_SUNK, "sunk", "sunk", "lava", "blob", "lava", NULL, NULL, 1, { 13, { 1.3, 1.9, 0.0, 0.0, 0.0 } }, 0, { 0, { 0.0, 0.0 } }, 1.0, 1.0, 0.3, 0.45, { 0.0, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 0, { { { 0 } } }, 0 };
static const DtFn S107 = { DT_SUNK, "sunk", "sunk", "crack", "wig", "crack", NULL, NULL, 1, { 14, { 5.0, 8.0, 0.45, 0.9, 0.0 } }, 0, { 0, { 0.0, 0.0 } }, 1.0, 1.0, 0.4, 0.5, { 0.0, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 0, { { { 0 } } }, 0 };
static const DtFn S108 = { DT_TRAIL, "trail", "trail", "pit", "ell", "pit", NULL, NULL, 1, { 3, { 0.35, 0.45, 0.6, 0.75, 0.0 } }, 0, { 0, { 0.0, 0.0 } }, 5.0, 8.0, 0.0, 0.0, { 1.3, 0.0, 0.0, 0.0 }, { 0.35, NUM_FLOAT, 0 }, 0, { { { 0 } } }, 0 };
static const DtFn S109 = { DT_SUNK, "sunk", "sunk", "pit", "disc", "pit", NULL, NULL, 1, { 2, { 0.4, 0.55, 10.0, 0.0, 0.0 } }, 0, { 0, { 0.0, 0.0 } }, 4.0, 7.0, 0.3, 0.4, { 1.5, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 0, { { { 0 } } }, 0 };
static const DtFn S110 = { DT_SCATTER, "scatter", "scatter", "bone", "bone", "bone", NULL, "up", 1, { 6, { 1.5, 2.2, 0.0, 0.0, 0.0 } }, 0, { 0, { 0.0, 0.0 } }, 2.0, 4.0, 0.4, 0.5, { 1.6, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 2, { { { 0.7, 0, 0 }, { 6.0, 1, 0 } }, { { 0.3, 0, 0 }, { 40.0, 1, 0 } } }, 0 };
static const DtFn S111 = { DT_SINGLE, "single", "single", "bone", "ell", "bone", NULL, "mound", 1, { 3, { 1.1, 1.4, 0.7, 0.9, 0.0 } }, 1, { 4, { 0.22, 0.0 } }, 0.0, 0.0, 0.6, 0.8, { 0.0, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 2, { { { 0.75, 0, 0 }, { 5.0, 1, 0 } }, { { 0.25, 0, 0 }, { 40.0, 1, 0 } } }, 0 };
static const DtFn S112 = { DT_SINGLE, "single", "single", "crate", "rect", "crate", NULL, "up", 1, { 4, { 2.4, 3.2, 1.6, 2.0, 0.0 } }, 1, { 1, { 0.8, 0.0 } }, 0.0, 0.0, 1.0, 1.4, { 0.0, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 2, { { { 0.75, 0, 0 }, { 5.0, 1, 0 } }, { { 0.25, 0, 0 }, { 40.0, 1, 0 } } }, 0 };
static const DtFn S113 = { DT_SCATTER, "scatter", "scatter", "crate", "rect", "crate", NULL, "up", 1, { 4, { 1.2, 1.6, 1.0, 1.3, 0.0 } }, 0, { 0, { 0.0, 0.0 } }, 2.0, 3.0, 0.8, 1.1, { 1.8, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 2, { { { 0.7, 0, 0 }, { 6.0, 1, 0 } }, { { 0.3, 0, 0 }, { 40.0, 1, 0 } } }, 0 };
static const DtFn S114 = { DT_SINGLE, "single", "single", "wood", "disc", "wood", NULL, "up", 1, { 2, { 0.9, 1.2, 24.0, 0.0, 0.0 } }, 1, { 2, { 0.25, 0.0 } }, 0.0, 0.0, 1.4, 1.8, { 0.0, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 2, { { { 0.75, 0, 0 }, { 5.0, 1, 0 } }, { { 0.25, 0, 0 }, { 40.0, 1, 0 } } }, 0 };
static const DtFn S115 = { DT_SCATTER, "scatter", "scatter", "wood", "caps", "wood", NULL, "up", 1, { 5, { 2.4, 3.4, 0.55, 0.0, 0.0 } }, 0, { 0, { 0.0, 0.0 } }, 2.0, 3.0, 0.5, 0.6, { 1.8, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 2, { { { 0.7, 0, 0 }, { 6.0, 1, 0 } }, { { 0.3, 0, 0 }, { 40.0, 1, 0 } } }, 0 };
static const DtFn S116 = { DT_SCATTER, "scatter", "scatter", "obsidian", "hex", "obsidian", NULL, "up", 1, { 9, { 0.6, 0.9, 0.0, 0.0, 0.0 } }, 0, { 0, { 0.0, 0.0 } }, 3.0, 5.0, 0.7, 1.1, { 1.6, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 2, { { { 0.7, 0, 0 }, { 6.0, 1, 0 } }, { { 0.3, 0, 0 }, { 40.0, 1, 0 } } }, 0 };
static const DtFn S117 = { DT_SUNK, "sunk", "sunk", "lava", "disc", "lava", NULL, NULL, 1, { 2, { 0.5, 0.7, 10.0, 0.0, 0.0 } }, 0, { 0, { 0.0, 0.0 } }, 3.0, 5.0, 0.4, 0.5, { 1.6, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 0, { { { 0 } } }, 0 };
static const DtFn S118 = { DT_SCATTER, "scatter", "scatter", "ember", "round", "ember", NULL, "mound", 1, { 1, { 0.5, 0.7, 0.0, 0.0, 0.0 } }, 0, { 0, { 0.0, 0.0 } }, 5.0, 8.0, 0.4, 0.5, { 1.4, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 2, { { { 0.6, 0, 0 }, { 15.0, 1, 0 } }, { { 0.4, 0, 0 }, { 45.0, 1, 0 } } }, 0 };
static const DtFn S119 = { DT_SINGLE, "single", "single", "basalt", "blob", "basalt", NULL, "mound", 1, { 13, { 1.6, 2.2, 0.0, 0.0, 0.0 } }, 0, { 0, { 0.0, 0.0 } }, 0.0, 0.0, 0.7, 1.0, { 0.0, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 2, { { { 0.7, 0, 0 }, { 6.0, 1, 0 } }, { { 0.3, 0, 0 }, { 40.0, 1, 0 } } }, 0 };
static const DtFn S120 = { DT_SUNK, "sunk", "sunk", "pit", "crescent", "pit", NULL, NULL, 1, { 8, { 1.0, 1.4, 0.0, 0.0, 0.0 } }, 0, { 0, { 0.0, 0.0 } }, 1.0, 1.0, 0.3, 0.4, { 0.0, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 0, { { { 0 } } }, 0 };
static const DtFn S121 = { DT_SCATTER, "scatter", "scatter", "rock", "ell", "rock", NULL, "mound", 1, { 3, { 0.6, 0.8, 0.9, 1.2, 0.0 } }, 0, { 0, { 0.0, 0.0 } }, 3.0, 5.0, 0.5, 0.7, { 1.6, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 2, { { { 0.7, 0, 0 }, { 6.0, 1, 0 } }, { { 0.3, 0, 0 }, { 40.0, 1, 0 } } }, 0 };
static const DtFn S122 = { DT_SUNK, "sunk", "sunk", "crack", "star", "crack", NULL, NULL, 1, { 7, { 1.2, 1.6, 5.0, 6.0, 0.0 } }, 0, { 0, { 0.0, 0.0 } }, 1.0, 1.0, 0.3, 0.4, { 0.0, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 0, { { { 0 } } }, 0 };
static const DtFn S123 = { DT_SINGLE, "single", "single", "rock", "hex", "rock", NULL, "up", 1, { 9, { 1.1, 1.4, 0.0, 0.0, 0.0 } }, 1, { 3, { 0.0, 0.0 } }, 0.0, 0.0, 0.7, 0.9, { 0.0, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 2, { { { 0.75, 0, 0 }, { 5.0, 1, 0 } }, { { 0.25, 0, 0 }, { 40.0, 1, 0 } } }, 0 };
static const DtFn S124 = { DT_SCATTER, "scatter", "scatter", "ice", "rock", "ice", NULL, "mound", 1, { 0, { 0.9, 1.4, 5.0, 8.0, 0.0 } }, 0, { 0, { 0.0, 0.0 } }, 3.0, 5.0, 0.6, 1.0, { 1.6, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 2, { { { 0.7, 0, 0 }, { 6.0, 1, 0 } }, { { 0.3, 0, 0 }, { 40.0, 1, 0 } } }, 0 };
static const DtFn S125 = { DT_SCATTER, "scatter", "scatter", "snow", "round", "snow", NULL, "mound", 1, { 1, { 0.5, 0.75, 0.0, 0.0, 0.0 } }, 0, { 0, { 0.0, 0.0 } }, 6.0, 10.0, 0.35, 0.5, { 1.6, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 2, { { { 0.7, 0, 0 }, { 10.0, 1, 0 } }, { { 0.3, 0, 0 }, { 45.0, 1, 0 } } }, 0 };
static const DtFn S126 = { DT_SINGLE, "single", "single", "ice", "rock", "ice", NULL, "mound", 1, { 0, { 1.8, 2.5, 5.0, 8.0, 0.0 } }, 0, { 0, { 0.0, 0.0 } }, 0.0, 0.0, 1.1, 1.7, { 0.0, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 2, { { { 0.7, 0, 0 }, { 6.0, 1, 0 } }, { { 0.3, 0, 0 }, { 40.0, 1, 0 } } }, 0 };
static const DtFn S127 = { DT_SCATTER, "scatter", "scatter", "ice", "tri", "ice", NULL, "mound", 1, { 10, { 0.7, 1.1, 0.0, 0.0, 0.0 } }, 0, { 0, { 0.0, 0.0 } }, 3.0, 6.0, 0.5, 0.8, { 1.8, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 2, { { { 0.7, 0, 0 }, { 6.0, 1, 0 } }, { { 0.3, 0, 0 }, { 40.0, 1, 0 } } }, 0 };
static const DtFn S128 = { DT_SUNK, "sunk", "sunk", "water", "blob", "water", NULL, NULL, 1, { 13, { 1.3, 1.9, 0.0, 0.0, 0.0 } }, 0, { 0, { 0.0, 0.0 } }, 1.0, 1.0, 0.3, 0.45, { 0.0, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 0, { { { 0 } } }, 0 };
static const DtFn S129 = { DT_SUNK, "sunk", "sunk", "crack", "wig", "crack", NULL, NULL, 1, { 14, { 5.0, 8.0, 0.45, 0.9, 0.0 } }, 0, { 0, { 0.0, 0.0 } }, 1.0, 1.0, 0.4, 0.5, { 0.0, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 0, { { { 0 } } }, 0 };
static const DtFn S130 = { DT_TRAIL, "trail", "trail", "pit", "ell", "pit", NULL, NULL, 1, { 3, { 0.35, 0.45, 0.6, 0.75, 0.0 } }, 0, { 0, { 0.0, 0.0 } }, 5.0, 8.0, 0.0, 0.0, { 1.3, 0.0, 0.0, 0.0 }, { 0.35, NUM_FLOAT, 0 }, 0, { { { 0 } } }, 0 };
static const DtFn S131 = { DT_SUNK, "sunk", "sunk", "pit", "disc", "pit", NULL, NULL, 1, { 2, { 0.4, 0.55, 10.0, 0.0, 0.0 } }, 0, { 0, { 0.0, 0.0 } }, 4.0, 7.0, 0.3, 0.4, { 1.5, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 0, { { { 0 } } }, 0 };
static const DtFn S132 = { DT_SCATTER, "scatter", "scatter", "bone", "bone", "bone", NULL, "up", 1, { 6, { 1.5, 2.2, 0.0, 0.0, 0.0 } }, 0, { 0, { 0.0, 0.0 } }, 2.0, 4.0, 0.4, 0.5, { 1.6, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 2, { { { 0.7, 0, 0 }, { 6.0, 1, 0 } }, { { 0.3, 0, 0 }, { 40.0, 1, 0 } } }, 0 };
static const DtFn S133 = { DT_SINGLE, "single", "single", "bone", "ell", "bone", NULL, "mound", 1, { 3, { 1.1, 1.4, 0.7, 0.9, 0.0 } }, 1, { 4, { 0.22, 0.0 } }, 0.0, 0.0, 0.6, 0.8, { 0.0, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 2, { { { 0.75, 0, 0 }, { 5.0, 1, 0 } }, { { 0.25, 0, 0 }, { 40.0, 1, 0 } } }, 0 };
static const DtFn S134 = { DT_SINGLE, "single", "single", "crate", "rect", "crate", NULL, "up", 1, { 4, { 2.4, 3.2, 1.6, 2.0, 0.0 } }, 1, { 1, { 0.8, 0.0 } }, 0.0, 0.0, 1.0, 1.4, { 0.0, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 2, { { { 0.75, 0, 0 }, { 5.0, 1, 0 } }, { { 0.25, 0, 0 }, { 40.0, 1, 0 } } }, 0 };
static const DtFn S135 = { DT_SCATTER, "scatter", "scatter", "crate", "rect", "crate", NULL, "up", 1, { 4, { 1.2, 1.6, 1.0, 1.3, 0.0 } }, 0, { 0, { 0.0, 0.0 } }, 2.0, 3.0, 0.8, 1.1, { 1.8, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 2, { { { 0.7, 0, 0 }, { 6.0, 1, 0 } }, { { 0.3, 0, 0 }, { 40.0, 1, 0 } } }, 0 };
static const DtFn S136 = { DT_SINGLE, "single", "single", "alien", "disc", "alien", NULL, "up", 1, { 2, { 0.9, 1.2, 24.0, 0.0, 0.0 } }, 1, { 2, { 0.25, 0.0 } }, 0.0, 0.0, 1.4, 1.8, { 0.0, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 2, { { { 0.75, 0, 0 }, { 5.0, 1, 0 } }, { { 0.25, 0, 0 }, { 40.0, 1, 0 } } }, 0 };
static const DtFn S137 = { DT_SCATTER, "scatter", "scatter", "alien", "caps", "alien", NULL, "up", 1, { 5, { 2.4, 3.4, 0.55, 0.0, 0.0 } }, 0, { 0, { 0.0, 0.0 } }, 2.0, 3.0, 0.5, 0.6, { 1.8, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 2, { { { 0.7, 0, 0 }, { 6.0, 1, 0 } }, { { 0.3, 0, 0 }, { 40.0, 1, 0 } } }, 0 };
static const DtFn S138 = { DT_SINGLE, "single", "single", "alien", "caps", "alien", NULL, "up", 1, { 5, { 4.5, 6.5, 0.95, 0.0, 0.0 } }, 1, { 1, { 1.3, 0.0 } }, 0.0, 0.0, 0.6, 0.8, { 0.0, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 2, { { { 0.75, 0, 0 }, { 5.0, 1, 0 } }, { { 0.25, 0, 0 }, { 40.0, 1, 0 } } }, 0 };
static const DtFn S139 = { DT_SCATTER, "scatter", "scatter", "ice", "tri", "ice", NULL, "mound", 1, { 10, { 0.7, 1.0, 0.0, 0.0, 0.0 } }, 0, { 0, { 0.0, 0.0 } }, 4.0, 6.0, 0.9, 1.4, { 1.6, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 2, { { { 0.6, 0, 0 }, { 4.0, 1, 0 } }, { { 0.4, 0, 0 }, { 30.0, 1, 0 } } }, 0 };
static const DtFn S140 = { DT_SINGLE, "single", "single", "snow", "blob", "snow", NULL, "mound", 1, { 13, { 1.8, 2.4, 0.0, 0.0, 0.0 } }, 0, { 0, { 0.0, 0.0 } }, 0.0, 0.0, 0.5, 0.8, { 0.0, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 2, { { { 0.5, 0, 0 }, { 30.0, 1, 0 } }, { { 0.5, 0, 0 }, { 55.0, 1, 0 } } }, 0 };
static const DtFn S141 = { DT_SINGLE, "single", "single", "alien", "rect", "alien", NULL, "up", 1, { 4, { 1.8, 2.2, 1.8, 2.2, 0.0 } }, 1, { 4, { 0.25, 0.0 } }, 0.0, 0.0, 0.4, 0.5, { 0.0, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 2, { { { 0.75, 0, 0 }, { 5.0, 1, 0 } }, { { 0.25, 0, 0 }, { 40.0, 1, 0 } } }, 0 };
static const DtFn S142 = { DT_SUNK, "sunk", "sunk", "slot", "hex", "slot", NULL, NULL, 1, { 9, { 1.0, 1.3, 0.0, 0.0, 0.0 } }, 0, { 0, { 0.0, 0.0 } }, 1.0, 1.0, 0.4, 0.5, { 0.0, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 0, { { { 0 } } }, 0 };
static const DtFn S143 = { DT_SUNK, "sunk", "sunk", "pit", "crescent", "pit", NULL, NULL, 1, { 8, { 1.0, 1.4, 0.0, 0.0, 0.0 } }, 0, { 0, { 0.0, 0.0 } }, 1.0, 1.0, 0.3, 0.4, { 0.0, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 0, { { { 0 } } }, 0 };
static const DtFn S144 = { DT_SCATTER, "scatter", "scatter", "rock", "ell", "rock", NULL, "mound", 1, { 3, { 0.6, 0.8, 0.9, 1.2, 0.0 } }, 0, { 0, { 0.0, 0.0 } }, 3.0, 5.0, 0.5, 0.7, { 1.6, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 2, { { { 0.7, 0, 0 }, { 6.0, 1, 0 } }, { { 0.3, 0, 0 }, { 40.0, 1, 0 } } }, 0 };
static const DtFn S145 = { DT_SUNK, "sunk", "sunk", "crack", "star", "crack", NULL, NULL, 1, { 7, { 1.2, 1.6, 5.0, 6.0, 0.0 } }, 0, { 0, { 0.0, 0.0 } }, 1.0, 1.0, 0.3, 0.4, { 0.0, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 0, { { { 0 } } }, 0 };
static const DtFn S146 = { DT_SCATTER, "scatter", "scatter", "rock", "rock", "rock", NULL, "mound", 1, { 0, { 0.9, 1.4, 5.0, 8.0, 0.0 } }, 0, { 0, { 0.0, 0.0 } }, 3.0, 5.0, 0.6, 1.0, { 1.6, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 2, { { { 0.7, 0, 0 }, { 6.0, 1, 0 } }, { { 0.3, 0, 0 }, { 40.0, 1, 0 } } }, 0 };
static const DtFn S147 = { DT_SCATTER, "scatter", "scatter", "pebble", "round", "pebble", NULL, "mound", 1, { 1, { 0.5, 0.75, 0.0, 0.0, 0.0 } }, 0, { 0, { 0.0, 0.0 } }, 6.0, 10.0, 0.35, 0.5, { 1.6, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 2, { { { 0.7, 0, 0 }, { 10.0, 1, 0 } }, { { 0.3, 0, 0 }, { 45.0, 1, 0 } } }, 0 };
static const DtFn S148 = { DT_SINGLE, "single", "single", "rock", "rock", "rock", NULL, "mound", 1, { 0, { 1.8, 2.5, 5.0, 8.0, 0.0 } }, 0, { 0, { 0.0, 0.0 } }, 0.0, 0.0, 1.1, 1.7, { 0.0, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 2, { { { 0.7, 0, 0 }, { 6.0, 1, 0 } }, { { 0.3, 0, 0 }, { 40.0, 1, 0 } } }, 0 };
static const DtFn S149 = { DT_SCATTER, "scatter", "scatter", "rock", "tri", "rock", NULL, "mound", 1, { 10, { 0.7, 1.1, 0.0, 0.0, 0.0 } }, 0, { 0, { 0.0, 0.0 } }, 3.0, 6.0, 0.5, 0.8, { 1.8, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 2, { { { 0.7, 0, 0 }, { 6.0, 1, 0 } }, { { 0.3, 0, 0 }, { 40.0, 1, 0 } } }, 0 };
static const DtFn S150 = { DT_SUNK, "sunk", "sunk", "water", "blob", "water", NULL, NULL, 1, { 13, { 1.3, 1.9, 0.0, 0.0, 0.0 } }, 0, { 0, { 0.0, 0.0 } }, 1.0, 1.0, 0.3, 0.45, { 0.0, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 0, { { { 0 } } }, 0 };
static const DtFn S151 = { DT_SUNK, "sunk", "sunk", "crack", "wig", "crack", NULL, NULL, 1, { 14, { 5.0, 8.0, 0.45, 0.9, 0.0 } }, 0, { 0, { 0.0, 0.0 } }, 1.0, 1.0, 0.4, 0.5, { 0.0, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 0, { { { 0 } } }, 0 };
static const DtFn S152 = { DT_TRAIL, "trail", "trail", "pit", "ell", "pit", NULL, NULL, 1, { 3, { 0.35, 0.45, 0.6, 0.75, 0.0 } }, 0, { 0, { 0.0, 0.0 } }, 5.0, 8.0, 0.0, 0.0, { 1.3, 0.0, 0.0, 0.0 }, { 0.35, NUM_FLOAT, 0 }, 0, { { { 0 } } }, 0 };
static const DtFn S153 = { DT_SUNK, "sunk", "sunk", "pit", "disc", "pit", NULL, NULL, 1, { 2, { 0.4, 0.55, 10.0, 0.0, 0.0 } }, 0, { 0, { 0.0, 0.0 } }, 4.0, 7.0, 0.3, 0.4, { 1.5, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 0, { { { 0 } } }, 0 };
static const DtFn S154 = { DT_SCATTER, "scatter", "scatter", "bone", "bone", "bone", NULL, "up", 1, { 6, { 1.5, 2.2, 0.0, 0.0, 0.0 } }, 0, { 0, { 0.0, 0.0 } }, 2.0, 4.0, 0.4, 0.5, { 1.6, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 2, { { { 0.7, 0, 0 }, { 6.0, 1, 0 } }, { { 0.3, 0, 0 }, { 40.0, 1, 0 } } }, 0 };
static const DtFn S155 = { DT_SINGLE, "single", "single", "bone", "ell", "bone", NULL, "mound", 1, { 3, { 1.1, 1.4, 0.7, 0.9, 0.0 } }, 1, { 4, { 0.22, 0.0 } }, 0.0, 0.0, 0.6, 0.8, { 0.0, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 2, { { { 0.75, 0, 0 }, { 5.0, 1, 0 } }, { { 0.25, 0, 0 }, { 40.0, 1, 0 } } }, 0 };
static const DtFn S156 = { DT_SINGLE, "single", "single", "crate", "rect", "crate", NULL, "up", 1, { 4, { 2.4, 3.2, 1.6, 2.0, 0.0 } }, 1, { 1, { 0.8, 0.0 } }, 0.0, 0.0, 1.0, 1.4, { 0.0, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 2, { { { 0.75, 0, 0 }, { 5.0, 1, 0 } }, { { 0.25, 0, 0 }, { 40.0, 1, 0 } } }, 0 };
static const DtFn S157 = { DT_SCATTER, "scatter", "scatter", "crate", "rect", "crate", NULL, "up", 1, { 4, { 1.2, 1.6, 1.0, 1.3, 0.0 } }, 0, { 0, { 0.0, 0.0 } }, 2.0, 3.0, 0.8, 1.1, { 1.8, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 2, { { { 0.7, 0, 0 }, { 6.0, 1, 0 } }, { { 0.3, 0, 0 }, { 40.0, 1, 0 } } }, 0 };
static const DtFn S158 = { DT_SINGLE, "single", "single", "wood", "disc", "wood", NULL, "up", 1, { 2, { 0.9, 1.2, 24.0, 0.0, 0.0 } }, 1, { 2, { 0.25, 0.0 } }, 0.0, 0.0, 1.4, 1.8, { 0.0, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 2, { { { 0.75, 0, 0 }, { 5.0, 1, 0 } }, { { 0.25, 0, 0 }, { 40.0, 1, 0 } } }, 0 };
static const DtFn S159 = { DT_SCATTER, "scatter", "scatter", "wood", "caps", "wood", NULL, "up", 1, { 5, { 2.4, 3.4, 0.55, 0.0, 0.0 } }, 0, { 0, { 0.0, 0.0 } }, 2.0, 3.0, 0.5, 0.6, { 1.8, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 2, { { { 0.7, 0, 0 }, { 6.0, 1, 0 } }, { { 0.3, 0, 0 }, { 40.0, 1, 0 } } }, 0 };
static const DtFn S160 = { DT_SINGLE, "single", "single", "wood", "caps", "wood", NULL, "up", 1, { 5, { 4.5, 6.5, 0.95, 0.0, 0.0 } }, 1, { 1, { 1.3, 0.0 } }, 0.0, 0.0, 0.6, 0.8, { 0.0, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 2, { { { 0.75, 0, 0 }, { 5.0, 1, 0 } }, { { 0.25, 0, 0 }, { 40.0, 1, 0 } } }, 0 };
static const DtFn S161 = { DT_SCATTER, "scatter", "scatter", "iron", "caps", "iron", NULL, "up", 1, { 5, { 0.7, 1.0, 0.3, 0.0, 0.0 } }, 0, { 0, { 0.0, 0.0 } }, 5.0, 8.0, 0.3, 0.35, { 1.4, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 2, { { { 0.7, 0, 0 }, { 6.0, 1, 0 } }, { { 0.3, 0, 0 }, { 40.0, 1, 0 } } }, 0 };
static const DtFn S162 = { DT_SINGLE, "single", "single", "iron", "ring", "iron", NULL, "up", 1, { 12, { 1.2, 1.5, 0.5, 0.0, 0.0 } }, 0, { 0, { 0.0, 0.0 } }, 0.0, 0.0, 0.5, 0.7, { 0.0, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 2, { { { 0.75, 0, 0 }, { 5.0, 1, 0 } }, { { 0.25, 0, 0 }, { 40.0, 1, 0 } } }, 0 };
static const DtFn S163 = { DT_PATH, "path", "path", "rope", "", "rope", NULL, "up", 0, { 0, { 0.0, 0.0, 0.0, 0.0, 0.0 } }, 0, { 0, { 0.0, 0.0 } }, 0.0, 0.0, 0.0, 0.0, { 0.5, 6.0, 10.0, 0.9 }, { 0.4, NUM_FLOAT, 0 }, 0, { { { 0 } } }, 0 };
static const DtFn S164 = { DT_SCATTER, "scatter", "scatter", "duck", "rect", "duck", NULL, "up", 1, { 4, { 1.2, 1.6, 0.5, 0.7, 0.0 } }, 0, { 0, { 0.0, 0.0 } }, 3.0, 5.0, 0.4, 0.5, { 1.6, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 2, { { { 0.7, 0, 0 }, { 6.0, 1, 0 } }, { { 0.3, 0, 0 }, { 40.0, 1, 0 } } }, 0 };
static const DtFn S165 = { DT_SINGLE, "single", "single", "wood", "cross", "wood", NULL, "up", 1, { 11, { 2.6, 3.2, 0.6, 0.0, 0.0 } }, 0, { 0, { 0.0, 0.0 } }, 0.0, 0.0, 0.6, 0.8, { 0.0, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 2, { { { 0.75, 0, 0 }, { 5.0, 1, 0 } }, { { 0.25, 0, 0 }, { 40.0, 1, 0 } } }, 0 };
static const DtFn S166 = { DT_SUNK, "sunk", "sunk", "pit", "blob", "pit", NULL, NULL, 1, { 13, { 1.8, 2.4, 0.0, 0.0, 0.0 } }, 0, { 0, { 0.0, 0.0 } }, 1.0, 1.0, 0.6, 0.8, { 0.0, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 0, { { { 0 } } }, 0 };
static const DtFn S167 = { DT_SCATTER, "scatter", "scatter", "iron", "disc", "iron", NULL, "mound", 1, { 2, { 0.55, 0.7, 14.0, 0.0, 0.0 } }, 0, { 0, { 0.0, 0.0 } }, 2.0, 3.0, 0.5, 0.6, { 1.6, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 2, { { { 0.5, 0, 0 }, { 10.0, 1, 0 } }, { { 0.5, 0, 0 }, { 45.0, 1, 0 } } }, 0 };
static const DtFn S168 = { DT_SINGLE, "single", "single", "crate", "rect", "crate", NULL, "up", 1, { 4, { 2.4, 3.2, 1.6, 2.0, 0.0 } }, 1, { 1, { 0.8, 0.0 } }, 0.0, 0.0, 1.0, 1.4, { 0.0, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 2, { { { 0.75, 0, 0 }, { 5.0, 1, 0 } }, { { 0.25, 0, 0 }, { 40.0, 1, 0 } } }, 0 };
static const DtFn S169 = { DT_SCATTER, "scatter", "scatter", "crate", "rect", "crate", NULL, "up", 1, { 4, { 1.2, 1.6, 1.0, 1.3, 0.0 } }, 0, { 0, { 0.0, 0.0 } }, 2.0, 3.0, 0.8, 1.1, { 1.8, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 2, { { { 0.7, 0, 0 }, { 6.0, 1, 0 } }, { { 0.3, 0, 0 }, { 40.0, 1, 0 } } }, 0 };
static const DtFn S170 = { DT_SINGLE, "single", "single", "wood", "disc", "wood", NULL, "up", 1, { 2, { 0.9, 1.2, 24.0, 0.0, 0.0 } }, 1, { 2, { 0.25, 0.0 } }, 0.0, 0.0, 1.4, 1.8, { 0.0, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 2, { { { 0.75, 0, 0 }, { 5.0, 1, 0 } }, { { 0.25, 0, 0 }, { 40.0, 1, 0 } } }, 0 };
static const DtFn S171 = { DT_SCATTER, "scatter", "scatter", "wood", "caps", "wood", NULL, "up", 1, { 5, { 2.4, 3.4, 0.55, 0.0, 0.0 } }, 0, { 0, { 0.0, 0.0 } }, 2.0, 3.0, 0.5, 0.6, { 1.8, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 2, { { { 0.7, 0, 0 }, { 6.0, 1, 0 } }, { { 0.3, 0, 0 }, { 40.0, 1, 0 } } }, 0 };
static const DtFn S172 = { DT_SINGLE, "single", "single", "wood", "caps", "wood", NULL, "up", 1, { 5, { 4.5, 6.5, 0.95, 0.0, 0.0 } }, 1, { 1, { 1.3, 0.0 } }, 0.0, 0.0, 0.6, 0.8, { 0.0, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 2, { { { 0.75, 0, 0 }, { 5.0, 1, 0 } }, { { 0.25, 0, 0 }, { 40.0, 1, 0 } } }, 0 };
static const DtFn S173 = { DT_SCATTER, "scatter", "scatter", "iron", "caps", "iron", NULL, "up", 1, { 5, { 0.7, 1.0, 0.3, 0.0, 0.0 } }, 0, { 0, { 0.0, 0.0 } }, 5.0, 8.0, 0.3, 0.35, { 1.4, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 2, { { { 0.7, 0, 0 }, { 6.0, 1, 0 } }, { { 0.3, 0, 0 }, { 40.0, 1, 0 } } }, 0 };
static const DtFn S174 = { DT_SINGLE, "single", "single", "iron", "ring", "iron", NULL, "up", 1, { 12, { 1.2, 1.5, 0.5, 0.0, 0.0 } }, 0, { 0, { 0.0, 0.0 } }, 0.0, 0.0, 0.5, 0.7, { 0.0, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 2, { { { 0.75, 0, 0 }, { 5.0, 1, 0 } }, { { 0.25, 0, 0 }, { 40.0, 1, 0 } } }, 0 };
static const DtFn S175 = { DT_PATH, "path", "path", "rope", "", "rope", NULL, "up", 0, { 0, { 0.0, 0.0, 0.0, 0.0, 0.0 } }, 0, { 0, { 0.0, 0.0 } }, 0.0, 0.0, 0.0, 0.0, { 0.5, 6.0, 10.0, 0.9 }, { 0.4, NUM_FLOAT, 0 }, 0, { { { 0 } } }, 0 };
static const DtFn S176 = { DT_SCATTER, "scatter", "scatter", "garden", "rock", "garden", NULL, "mound", 1, { 0, { 0.9, 1.4, 5.0, 8.0, 0.0 } }, 0, { 0, { 0.0, 0.0 } }, 3.0, 5.0, 0.6, 1.0, { 1.6, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 2, { { { 0.7, 0, 0 }, { 6.0, 1, 0 } }, { { 0.3, 0, 0 }, { 40.0, 1, 0 } } }, 0 };
static const DtFn S177 = { DT_SCATTER, "scatter", "scatter", "pebble", "round", "pebble", NULL, "mound", 1, { 1, { 0.5, 0.75, 0.0, 0.0, 0.0 } }, 0, { 0, { 0.0, 0.0 } }, 6.0, 10.0, 0.35, 0.5, { 1.6, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 2, { { { 0.7, 0, 0 }, { 10.0, 1, 0 } }, { { 0.3, 0, 0 }, { 45.0, 1, 0 } } }, 0 };
static const DtFn S178 = { DT_SINGLE, "single", "single", "garden", "rock", "garden", NULL, "mound", 1, { 0, { 1.8, 2.5, 5.0, 8.0, 0.0 } }, 0, { 0, { 0.0, 0.0 } }, 0.0, 0.0, 1.1, 1.7, { 0.0, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 2, { { { 0.7, 0, 0 }, { 6.0, 1, 0 } }, { { 0.3, 0, 0 }, { 40.0, 1, 0 } } }, 0 };
static const DtFn S179 = { DT_SCATTER, "scatter", "scatter", "garden", "tri", "garden", NULL, "mound", 1, { 10, { 0.7, 1.1, 0.0, 0.0, 0.0 } }, 0, { 0, { 0.0, 0.0 } }, 3.0, 6.0, 0.5, 0.8, { 1.8, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 2, { { { 0.7, 0, 0 }, { 6.0, 1, 0 } }, { { 0.3, 0, 0 }, { 40.0, 1, 0 } } }, 0 };
static const DtFn S180 = { DT_SUNK, "sunk", "sunk", "water", "blob", "water", NULL, NULL, 1, { 13, { 1.3, 1.9, 0.0, 0.0, 0.0 } }, 0, { 0, { 0.0, 0.0 } }, 1.0, 1.0, 0.3, 0.45, { 0.0, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 0, { { { 0 } } }, 0 };
static const DtFn S181 = { DT_SUNK, "sunk", "sunk", "crack", "wig", "crack", NULL, NULL, 1, { 14, { 5.0, 8.0, 0.45, 0.9, 0.0 } }, 0, { 0, { 0.0, 0.0 } }, 1.0, 1.0, 0.4, 0.5, { 0.0, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 0, { { { 0 } } }, 0 };
static const DtFn S182 = { DT_TRAIL, "trail", "trail", "pit", "ell", "pit", NULL, NULL, 1, { 3, { 0.35, 0.45, 0.6, 0.75, 0.0 } }, 0, { 0, { 0.0, 0.0 } }, 5.0, 8.0, 0.0, 0.0, { 1.3, 0.0, 0.0, 0.0 }, { 0.35, NUM_FLOAT, 0 }, 0, { { { 0 } } }, 0 };
static const DtFn S183 = { DT_SUNK, "sunk", "sunk", "pit", "disc", "pit", NULL, NULL, 1, { 2, { 0.4, 0.55, 10.0, 0.0, 0.0 } }, 0, { 0, { 0.0, 0.0 } }, 4.0, 7.0, 0.3, 0.4, { 1.5, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 0, { { { 0 } } }, 0 };
static const DtFn S184 = { DT_SCATTER, "scatter", "scatter", "bone", "bone", "bone", NULL, "up", 1, { 6, { 1.5, 2.2, 0.0, 0.0, 0.0 } }, 0, { 0, { 0.0, 0.0 } }, 2.0, 4.0, 0.4, 0.5, { 1.6, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 2, { { { 0.7, 0, 0 }, { 6.0, 1, 0 } }, { { 0.3, 0, 0 }, { 40.0, 1, 0 } } }, 0 };
static const DtFn S185 = { DT_SCATTER, "scatter", "scatter", "wood", "rect", "wood", NULL, "up", 1, { 4, { 1.6, 2.0, 0.4, 0.5, 0.0 } }, 0, { 0, { 0.0, 0.0 } }, 2.0, 4.0, 0.35, 0.45, { 1.6, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 2, { { { 0.7, 0, 0 }, { 6.0, 1, 0 } }, { { 0.3, 0, 0 }, { 40.0, 1, 0 } } }, 0 };
static const DtFn S186 = { DT_SINGLE, "single", "single", "wood", "disc", "wood", NULL, "up", 1, { 2, { 1.2, 1.5, 24.0, 0.0, 0.0 } }, 1, { 3, { 0.0, 0.0 } }, 0.0, 0.0, 0.5, 0.6, { 0.0, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 2, { { { 0.75, 0, 0 }, { 5.0, 1, 0 } }, { { 0.25, 0, 0 }, { 40.0, 1, 0 } } }, 0 };
static const DtFn S187 = { DT_SINGLE, "single", "single", "lantern", "hex", "lantern", NULL, "up", 1, { 9, { 1.1, 1.4, 0.0, 0.0, 0.0 } }, 1, { 5, { 0.35, 0.5 } }, 0.0, 0.0, 0.9, 1.2, { 0.0, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 2, { { { 0.75, 0, 0 }, { 5.0, 1, 0 } }, { { 0.25, 0, 0 }, { 40.0, 1, 0 } } }, 0 };
static const DtFn S188 = { DT_SCATTER, "scatter", "scatter", "wood", "disc", "wood", NULL, "up", 1, { 2, { 0.5, 0.65, 14.0, 0.0, 0.0 } }, 0, { 0, { 0.0, 0.0 } }, 3.0, 5.0, 0.6, 0.8, { 1.5, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 2, { { { 0.7, 0, 0 }, { 6.0, 1, 0 } }, { { 0.3, 0, 0 }, { 40.0, 1, 0 } } }, 0 };
static const DtFn S189 = { DT_SUNK, "sunk", "sunk", "slot", "rect", "slot", NULL, NULL, 1, { 4, { 2.2, 2.8, 1.4, 1.8, 0.0 } }, 0, { 0, { 0.0, 0.0 } }, 1.0, 1.0, 0.3, 0.4, { 0.0, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 0, { { { 0 } } }, 0 };
static const DtFn S190 = { DT_SCATTER, "scatter", "scatter", "flesh", "rock", "flesh", NULL, "mound", 1, { 0, { 0.9, 1.4, 5.0, 8.0, 0.0 } }, 0, { 0, { 0.0, 0.0 } }, 3.0, 5.0, 0.6, 1.0, { 1.6, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 2, { { { 0.7, 0, 0 }, { 6.0, 1, 0 } }, { { 0.3, 0, 0 }, { 40.0, 1, 0 } } }, 0 };
static const DtFn S191 = { DT_SCATTER, "scatter", "scatter", "pustule", "round", "pustule", NULL, "mound", 1, { 1, { 0.5, 0.75, 0.0, 0.0, 0.0 } }, 0, { 0, { 0.0, 0.0 } }, 6.0, 10.0, 0.35, 0.5, { 1.6, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 2, { { { 0.7, 0, 0 }, { 10.0, 1, 0 } }, { { 0.3, 0, 0 }, { 45.0, 1, 0 } } }, 0 };
static const DtFn S192 = { DT_SINGLE, "single", "single", "flesh", "rock", "flesh", NULL, "mound", 1, { 0, { 1.8, 2.5, 5.0, 8.0, 0.0 } }, 0, { 0, { 0.0, 0.0 } }, 0.0, 0.0, 1.1, 1.7, { 0.0, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 2, { { { 0.7, 0, 0 }, { 6.0, 1, 0 } }, { { 0.3, 0, 0 }, { 40.0, 1, 0 } } }, 0 };
static const DtFn S193 = { DT_SCATTER, "scatter", "scatter", "flesh", "tri", "flesh", NULL, "mound", 1, { 10, { 0.7, 1.1, 0.0, 0.0, 0.0 } }, 0, { 0, { 0.0, 0.0 } }, 3.0, 6.0, 0.5, 0.8, { 1.8, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 2, { { { 0.7, 0, 0 }, { 6.0, 1, 0 } }, { { 0.3, 0, 0 }, { 40.0, 1, 0 } } }, 0 };
static const DtFn S194 = { DT_SUNK, "sunk", "sunk", "bile", "blob", "bile", NULL, NULL, 1, { 13, { 1.3, 1.9, 0.0, 0.0, 0.0 } }, 0, { 0, { 0.0, 0.0 } }, 1.0, 1.0, 0.3, 0.45, { 0.0, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 0, { { { 0 } } }, 0 };
static const DtFn S195 = { DT_SUNK, "sunk", "sunk", "crack", "wig", "crack", NULL, NULL, 1, { 14, { 5.0, 8.0, 0.45, 0.9, 0.0 } }, 0, { 0, { 0.0, 0.0 } }, 1.0, 1.0, 0.4, 0.5, { 0.0, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 0, { { { 0 } } }, 0 };
static const DtFn S196 = { DT_TRAIL, "trail", "trail", "pit", "ell", "pit", NULL, NULL, 1, { 3, { 0.35, 0.45, 0.6, 0.75, 0.0 } }, 0, { 0, { 0.0, 0.0 } }, 5.0, 8.0, 0.0, 0.0, { 1.3, 0.0, 0.0, 0.0 }, { 0.35, NUM_FLOAT, 0 }, 0, { { { 0 } } }, 0 };
static const DtFn S197 = { DT_SUNK, "sunk", "sunk", "pit", "disc", "pit", NULL, NULL, 1, { 2, { 0.4, 0.55, 10.0, 0.0, 0.0 } }, 0, { 0, { 0.0, 0.0 } }, 4.0, 7.0, 0.3, 0.4, { 1.5, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 0, { { { 0 } } }, 0 };
static const DtFn S198 = { DT_SCATTER, "scatter", "scatter", "egg", "ell", "egg", NULL, "mound", 1, { 3, { 0.6, 0.8, 0.9, 1.2, 0.0 } }, 0, { 0, { 0.0, 0.0 } }, 3.0, 6.0, 0.7, 1.0, { 1.6, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 2, { { { 0.55, 0, 0 }, { 25.0, 1, 0 } }, { { 0.45, 0, 0 }, { 50.0, 1, 0 } } }, 0 };
static const DtFn S199 = { DT_SINGLE, "single", "single", "flesh", "blob", "flesh", NULL, "mound", 1, { 13, { 1.8, 2.4, 0.0, 0.0, 0.0 } }, 0, { 0, { 0.0, 0.0 } }, 0.0, 0.0, 1.0, 1.4, { 0.0, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 2, { { { 0.55, 0, 0 }, { 25.0, 1, 0 } }, { { 0.45, 0, 0 }, { 50.0, 1, 0 } } }, 0 };
static const DtFn S200 = { DT_SUNK, "sunk", "sunk", "bile", "disc", "bile", NULL, NULL, 1, { 2, { 0.4, 0.6, 12.0, 0.0, 0.0 } }, 0, { 0, { 0.0, 0.0 } }, 4.0, 7.0, 0.4, 0.5, { 1.6, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 0, { { { 0 } } }, 0 };
static const DtFn S201 = { DT_SUNK, "sunk", "sunk", "pit", "ring", "pit", NULL, NULL, 1, { 12, { 1.4, 1.8, 0.45, 0.0, 0.0 } }, 0, { 0, { 0.0, 0.0 } }, 1.0, 1.0, 0.4, 0.5, { 0.0, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 0, { { { 0 } } }, 0 };
static const DtFn S202 = { DT_SCATTER, "scatter", "scatter", "tooth", "crescent", "tooth", NULL, "up", 1, { 8, { 0.8, 1.1, 0.0, 0.0, 0.0 } }, 0, { 0, { 0.0, 0.0 } }, 2.0, 4.0, 0.5, 0.7, { 1.6, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 2, { { { 0.7, 0, 0 }, { 6.0, 1, 0 } }, { { 0.3, 0, 0 }, { 40.0, 1, 0 } } }, 0 };
static const DtFn S203 = { DT_SCATTER, "scatter", "scatter", "vein", "caps", "vein", NULL, "up", 1, { 5, { 1.6, 2.4, 0.5, 0.0, 0.0 } }, 0, { 0, { 0.0, 0.0 } }, 3.0, 5.0, 0.5, 0.7, { 1.6, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 2, { { { 0.7, 0, 0 }, { 6.0, 1, 0 } }, { { 0.3, 0, 0 }, { 40.0, 1, 0 } } }, 0 };
static const DtFn S204 = { DT_SINGLE, "single", "single", "flesh", "ring", "flesh", NULL, "up", 1, { 12, { 1.4, 1.8, 0.7, 0.0, 0.0 } }, 0, { 0, { 0.0, 0.0 } }, 0.0, 0.0, 0.8, 1.0, { 0.0, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 2, { { { 0.75, 0, 0 }, { 5.0, 1, 0 } }, { { 0.25, 0, 0 }, { 40.0, 1, 0 } } }, 0 };
static const DtFn S205 = { DT_SINGLE, "single", "single", "egg", "ell", "egg", NULL, "mound", 1, { 3, { 1.4, 1.9, 0.9, 1.2, 0.0 } }, 0, { 0, { 0.0, 0.0 } }, 0.0, 0.0, 1.0, 1.4, { 0.0, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 2, { { { 0.55, 0, 0 }, { 25.0, 1, 0 } }, { { 0.45, 0, 0 }, { 50.0, 1, 0 } } }, 0 };
static const DtFn S206 = { DT_SCATTER, "scatter", "scatter", "tooth", "tri", "tooth", NULL, "mound", 1, { 10, { 0.6, 0.9, 0.0, 0.0, 0.0 } }, 0, { 0, { 0.0, 0.0 } }, 4.0, 6.0, 0.8, 1.2, { 1.6, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 2, { { { 0.6, 0, 0 }, { 4.0, 1, 0 } }, { { 0.4, 0, 0 }, { 35.0, 1, 0 } } }, 0 };
static const DtFn S207 = { DT_SINGLE, "single", "single", "flesh", "disc", "flesh", NULL, "up", 1, { 2, { 1.4, 1.8, 24.0, 0.0, 0.0 } }, 1, { 5, { 0.6, 0.9 } }, 0.0, 0.0, 0.5, 0.7, { 0.0, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 2, { { { 0.75, 0, 0 }, { 5.0, 1, 0 } }, { { 0.25, 0, 0 }, { 40.0, 1, 0 } } }, 0 };
static const DtFn S208 = { DT_SINGLE, "single", "single", "iron", "rect", "iron", NULL, "up", 1, { 4, { 2.0, 2.6, 1.2, 1.5, 0.0 } }, 1, { 0, { 2.0, 0.3 } }, 0.0, 0.0, 0.7, 0.9, { 0.0, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 2, { { { 0.75, 0, 0 }, { 5.0, 1, 0 } }, { { 0.25, 0, 0 }, { 40.0, 1, 0 } } }, 0 };
static const DtFn S209 = { DT_SCATTER, "scatter", "scatter", "iron", "caps", "iron", NULL, "up", 1, { 5, { 0.7, 1.0, 0.3, 0.0, 0.0 } }, 0, { 0, { 0.0, 0.0 } }, 4.0, 7.0, 0.3, 0.35, { 1.4, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 2, { { { 0.7, 0, 0 }, { 6.0, 1, 0 } }, { { 0.3, 0, 0 }, { 40.0, 1, 0 } } }, 0 };
static const DtFn S210 = { DT_SUNK, "sunk", "sunk", "pit", "crescent", "pit", NULL, NULL, 1, { 8, { 1.0, 1.4, 0.0, 0.0, 0.0 } }, 0, { 0, { 0.0, 0.0 } }, 1.0, 1.0, 0.3, 0.4, { 0.0, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 0, { { { 0 } } }, 0 };
static const DtFn S211 = { DT_SCATTER, "scatter", "scatter", "rock", "ell", "rock", NULL, "mound", 1, { 3, { 0.6, 0.8, 0.9, 1.2, 0.0 } }, 0, { 0, { 0.0, 0.0 } }, 3.0, 5.0, 0.5, 0.7, { 1.6, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 2, { { { 0.7, 0, 0 }, { 6.0, 1, 0 } }, { { 0.3, 0, 0 }, { 40.0, 1, 0 } } }, 0 };
static const DtFn S212 = { DT_SINGLE, "single", "single", "crate", "rect", "crate", NULL, "up", 1, { 4, { 2.4, 3.2, 1.6, 2.0, 0.0 } }, 1, { 1, { 0.8, 0.0 } }, 0.0, 0.0, 1.0, 1.4, { 0.0, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 2, { { { 0.75, 0, 0 }, { 5.0, 1, 0 } }, { { 0.25, 0, 0 }, { 40.0, 1, 0 } } }, 0 };
static const DtFn S213 = { DT_SCATTER, "scatter", "scatter", "crate", "rect", "crate", NULL, "up", 1, { 4, { 1.2, 1.6, 1.0, 1.3, 0.0 } }, 0, { 0, { 0.0, 0.0 } }, 2.0, 3.0, 0.8, 1.1, { 1.8, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 2, { { { 0.7, 0, 0 }, { 6.0, 1, 0 } }, { { 0.3, 0, 0 }, { 40.0, 1, 0 } } }, 0 };
static const DtFn S214 = { DT_SINGLE, "single", "single", "board", "disc", "board", NULL, "up", 1, { 2, { 0.9, 1.2, 24.0, 0.0, 0.0 } }, 1, { 2, { 0.25, 0.0 } }, 0.0, 0.0, 1.4, 1.8, { 0.0, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 2, { { { 0.75, 0, 0 }, { 5.0, 1, 0 } }, { { 0.25, 0, 0 }, { 40.0, 1, 0 } } }, 0 };
static const DtFn S215 = { DT_SCATTER, "scatter", "scatter", "board", "caps", "board", NULL, "up", 1, { 5, { 2.4, 3.4, 0.55, 0.0, 0.0 } }, 0, { 0, { 0.0, 0.0 } }, 2.0, 3.0, 0.5, 0.6, { 1.8, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 2, { { { 0.7, 0, 0 }, { 6.0, 1, 0 } }, { { 0.3, 0, 0 }, { 40.0, 1, 0 } } }, 0 };
static const DtFn S216 = { DT_SINGLE, "single", "single", "board", "caps", "board", NULL, "up", 1, { 5, { 4.5, 6.5, 0.95, 0.0, 0.0 } }, 1, { 1, { 1.3, 0.0 } }, 0.0, 0.0, 0.6, 0.8, { 0.0, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 2, { { { 0.75, 0, 0 }, { 5.0, 1, 0 } }, { { 0.25, 0, 0 }, { 40.0, 1, 0 } } }, 0 };
static const DtFn S217 = { DT_SCATTER, "scatter", "scatter", "iron", "caps", "iron", NULL, "up", 1, { 5, { 0.7, 1.0, 0.3, 0.0, 0.0 } }, 0, { 0, { 0.0, 0.0 } }, 5.0, 8.0, 0.3, 0.35, { 1.4, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 2, { { { 0.7, 0, 0 }, { 6.0, 1, 0 } }, { { 0.3, 0, 0 }, { 40.0, 1, 0 } } }, 0 };
static const DtFn S218 = { DT_SINGLE, "single", "single", "iron", "ring", "iron", NULL, "up", 1, { 12, { 1.2, 1.5, 0.5, 0.0, 0.0 } }, 0, { 0, { 0.0, 0.0 } }, 0.0, 0.0, 0.5, 0.7, { 0.0, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 2, { { { 0.75, 0, 0 }, { 5.0, 1, 0 } }, { { 0.25, 0, 0 }, { 40.0, 1, 0 } } }, 0 };
static const DtFn S219 = { DT_PATH, "path", "path", "rope", "", "rope", NULL, "up", 0, { 0, { 0.0, 0.0, 0.0, 0.0, 0.0 } }, 0, { 0, { 0.0, 0.0 } }, 0.0, 0.0, 0.0, 0.0, { 0.5, 6.0, 10.0, 0.9 }, { 0.4, NUM_FLOAT, 0 }, 0, { { { 0 } } }, 0 };
static const DtFn S220 = { DT_SCATTER, "scatter", "scatter", "rock", "rock", "rock", NULL, "mound", 1, { 0, { 0.9, 1.4, 5.0, 8.0, 0.0 } }, 0, { 0, { 0.0, 0.0 } }, 3.0, 5.0, 0.6, 1.0, { 1.6, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 2, { { { 0.7, 0, 0 }, { 6.0, 1, 0 } }, { { 0.3, 0, 0 }, { 40.0, 1, 0 } } }, 0 };
static const DtFn S221 = { DT_SCATTER, "scatter", "scatter", "pebble", "round", "pebble", NULL, "mound", 1, { 1, { 0.5, 0.75, 0.0, 0.0, 0.0 } }, 0, { 0, { 0.0, 0.0 } }, 6.0, 10.0, 0.35, 0.5, { 1.6, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 2, { { { 0.7, 0, 0 }, { 10.0, 1, 0 } }, { { 0.3, 0, 0 }, { 45.0, 1, 0 } } }, 0 };
static const DtFn S222 = { DT_SINGLE, "single", "single", "rock", "rock", "rock", NULL, "mound", 1, { 0, { 1.8, 2.5, 5.0, 8.0, 0.0 } }, 0, { 0, { 0.0, 0.0 } }, 0.0, 0.0, 1.1, 1.7, { 0.0, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 2, { { { 0.7, 0, 0 }, { 6.0, 1, 0 } }, { { 0.3, 0, 0 }, { 40.0, 1, 0 } } }, 0 };
static const DtFn S223 = { DT_SCATTER, "scatter", "scatter", "rock", "tri", "rock", NULL, "mound", 1, { 10, { 0.7, 1.1, 0.0, 0.0, 0.0 } }, 0, { 0, { 0.0, 0.0 } }, 3.0, 6.0, 0.5, 0.8, { 1.8, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 2, { { { 0.7, 0, 0 }, { 6.0, 1, 0 } }, { { 0.3, 0, 0 }, { 40.0, 1, 0 } } }, 0 };
static const DtFn S224 = { DT_SUNK, "sunk", "sunk", "water", "blob", "water", NULL, NULL, 1, { 13, { 1.3, 1.9, 0.0, 0.0, 0.0 } }, 0, { 0, { 0.0, 0.0 } }, 1.0, 1.0, 0.3, 0.45, { 0.0, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 0, { { { 0 } } }, 0 };
static const DtFn S225 = { DT_SUNK, "sunk", "sunk", "crack", "wig", "crack", NULL, NULL, 1, { 14, { 5.0, 8.0, 0.45, 0.9, 0.0 } }, 0, { 0, { 0.0, 0.0 } }, 1.0, 1.0, 0.4, 0.5, { 0.0, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 0, { { { 0 } } }, 0 };
static const DtFn S226 = { DT_TRAIL, "trail", "trail", "pit", "ell", "pit", NULL, NULL, 1, { 3, { 0.35, 0.45, 0.6, 0.75, 0.0 } }, 0, { 0, { 0.0, 0.0 } }, 5.0, 8.0, 0.0, 0.0, { 1.3, 0.0, 0.0, 0.0 }, { 0.35, NUM_FLOAT, 0 }, 0, { { { 0 } } }, 0 };
static const DtFn S227 = { DT_SUNK, "sunk", "sunk", "pit", "disc", "pit", NULL, NULL, 1, { 2, { 0.4, 0.55, 10.0, 0.0, 0.0 } }, 0, { 0, { 0.0, 0.0 } }, 4.0, 7.0, 0.3, 0.4, { 1.5, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 0, { { { 0 } } }, 0 };
static const DtFn S228 = { DT_SCATTER, "scatter", "scatter", "bone", "bone", "bone", NULL, "up", 1, { 6, { 1.5, 2.2, 0.0, 0.0, 0.0 } }, 0, { 0, { 0.0, 0.0 } }, 2.0, 4.0, 0.4, 0.5, { 1.6, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 2, { { { 0.7, 0, 0 }, { 6.0, 1, 0 } }, { { 0.3, 0, 0 }, { 40.0, 1, 0 } } }, 0 };
static const DtFn S229 = { DT_SINGLE, "single", "single", "bone", "ell", "bone", NULL, "mound", 1, { 3, { 1.1, 1.4, 0.7, 0.9, 0.0 } }, 1, { 4, { 0.22, 0.0 } }, 0.0, 0.0, 0.6, 0.8, { 0.0, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 2, { { { 0.75, 0, 0 }, { 5.0, 1, 0 } }, { { 0.25, 0, 0 }, { 40.0, 1, 0 } } }, 0 };
static const DtFn S230 = { DT_SCATTER, "scatter", "scatter", "shrub", "star", "shrub", NULL, "mound", 1, { 7, { 0.9, 1.3, 5.0, 8.0, 0.0 } }, 0, { 0, { 0.0, 0.0 } }, 2.0, 4.0, 0.8, 1.2, { 2.0, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 2, { { { 0.55, 0, 0 }, { 25.0, 1, 0 } }, { { 0.45, 0, 0 }, { 50.0, 1, 0 } } }, 0 };
static const DtFn S231 = { DT_SINGLE, "single", "single", "shrub", "star", "shrub", NULL, "mound", 1, { 7, { 1.5, 2.1, 6.0, 9.0, 0.0 } }, 0, { 0, { 0.0, 0.0 } }, 0.0, 0.0, 1.0, 1.4, { 0.0, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 2, { { { 0.55, 0, 0 }, { 25.0, 1, 0 } }, { { 0.45, 0, 0 }, { 50.0, 1, 0 } } }, 0 };
static const DtFn S232 = { DT_SINGLE, "single", "single", "post", "rect", "post", NULL, "up", 1, { 4, { 1.0, 1.3, 1.0, 1.3, 0.0 } }, 1, { 3, { 0.0, 0.0 } }, 0.0, 0.0, 2.0, 2.6, { 0.0, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 2, { { { 0.75, 0, 0 }, { 5.0, 1, 0 } }, { { 0.25, 0, 0 }, { 40.0, 1, 0 } } }, 0 };
static const DtFn S233 = { DT_SCATTER, "scatter", "scatter", "iron", "crescent", "iron", NULL, "up", 1, { 8, { 0.5, 0.65, 0.0, 0.0, 0.0 } }, 0, { 0, { 0.0, 0.0 } }, 3.0, 6.0, 0.3, 0.35, { 1.5, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 2, { { { 0.7, 0, 0 }, { 6.0, 1, 0 } }, { { 0.3, 0, 0 }, { 40.0, 1, 0 } } }, 0 };
static const DtFn S234 = { DT_SINGLE, "single", "single", "board", "cross", "board", NULL, "up", 1, { 11, { 2.4, 3.0, 0.6, 0.0, 0.0 } }, 0, { 0, { 0.0, 0.0 } }, 0.0, 0.0, 0.5, 0.7, { 0.0, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 2, { { { 0.75, 0, 0 }, { 5.0, 1, 0 } }, { { 0.25, 0, 0 }, { 40.0, 1, 0 } } }, 0 };
static const DtFn S235 = { DT_SCATTER, "scatter", "scatter", "rock", "rock", "rock", NULL, "mound", 1, { 0, { 0.9, 1.4, 5.0, 8.0, 0.0 } }, 0, { 0, { 0.0, 0.0 } }, 3.0, 5.0, 0.6, 1.0, { 1.6, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 2, { { { 0.7, 0, 0 }, { 6.0, 1, 0 } }, { { 0.3, 0, 0 }, { 40.0, 1, 0 } } }, 0 };
static const DtFn S236 = { DT_SCATTER, "scatter", "scatter", "pebble", "round", "pebble", NULL, "mound", 1, { 1, { 0.5, 0.75, 0.0, 0.0, 0.0 } }, 0, { 0, { 0.0, 0.0 } }, 6.0, 10.0, 0.35, 0.5, { 1.6, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 2, { { { 0.7, 0, 0 }, { 10.0, 1, 0 } }, { { 0.3, 0, 0 }, { 45.0, 1, 0 } } }, 0 };
static const DtFn S237 = { DT_SINGLE, "single", "single", "rock", "rock", "rock", NULL, "mound", 1, { 0, { 1.8, 2.5, 5.0, 8.0, 0.0 } }, 0, { 0, { 0.0, 0.0 } }, 0.0, 0.0, 1.1, 1.7, { 0.0, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 2, { { { 0.7, 0, 0 }, { 6.0, 1, 0 } }, { { 0.3, 0, 0 }, { 40.0, 1, 0 } } }, 0 };
static const DtFn S238 = { DT_SCATTER, "scatter", "scatter", "rock", "tri", "rock", NULL, "mound", 1, { 10, { 0.7, 1.1, 0.0, 0.0, 0.0 } }, 0, { 0, { 0.0, 0.0 } }, 3.0, 6.0, 0.5, 0.8, { 1.8, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 2, { { { 0.7, 0, 0 }, { 6.0, 1, 0 } }, { { 0.3, 0, 0 }, { 40.0, 1, 0 } } }, 0 };
static const DtFn S239 = { DT_SUNK, "sunk", "sunk", "water", "blob", "water", NULL, NULL, 1, { 13, { 1.3, 1.9, 0.0, 0.0, 0.0 } }, 0, { 0, { 0.0, 0.0 } }, 1.0, 1.0, 0.3, 0.45, { 0.0, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 0, { { { 0 } } }, 0 };
static const DtFn S240 = { DT_SUNK, "sunk", "sunk", "crack", "wig", "crack", NULL, NULL, 1, { 14, { 5.0, 8.0, 0.45, 0.9, 0.0 } }, 0, { 0, { 0.0, 0.0 } }, 1.0, 1.0, 0.4, 0.5, { 0.0, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 0, { { { 0 } } }, 0 };
static const DtFn S241 = { DT_TRAIL, "trail", "trail", "pit", "ell", "pit", NULL, NULL, 1, { 3, { 0.35, 0.45, 0.6, 0.75, 0.0 } }, 0, { 0, { 0.0, 0.0 } }, 5.0, 8.0, 0.0, 0.0, { 1.3, 0.0, 0.0, 0.0 }, { 0.35, NUM_FLOAT, 0 }, 0, { { { 0 } } }, 0 };
static const DtFn S242 = { DT_SUNK, "sunk", "sunk", "pit", "disc", "pit", NULL, NULL, 1, { 2, { 0.4, 0.55, 10.0, 0.0, 0.0 } }, 0, { 0, { 0.0, 0.0 } }, 4.0, 7.0, 0.3, 0.4, { 1.5, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 0, { { { 0 } } }, 0 };
static const DtFn S243 = { DT_SCATTER, "scatter", "scatter", "bone", "bone", "bone", NULL, "up", 1, { 6, { 1.5, 2.2, 0.0, 0.0, 0.0 } }, 0, { 0, { 0.0, 0.0 } }, 2.0, 4.0, 0.4, 0.5, { 1.6, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 2, { { { 0.7, 0, 0 }, { 6.0, 1, 0 } }, { { 0.3, 0, 0 }, { 40.0, 1, 0 } } }, 0 };
static const DtFn S244 = { DT_SINGLE, "single", "single", "bone", "ell", "bone", NULL, "mound", 1, { 3, { 1.1, 1.4, 0.7, 0.9, 0.0 } }, 1, { 4, { 0.22, 0.0 } }, 0.0, 0.0, 0.6, 0.8, { 0.0, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 2, { { { 0.75, 0, 0 }, { 5.0, 1, 0 } }, { { 0.25, 0, 0 }, { 40.0, 1, 0 } } }, 0 };
static const DtFn S245 = { DT_SCATTER, "scatter", "scatter", "shrub", "star", "shrub", NULL, "mound", 1, { 7, { 0.9, 1.3, 5.0, 8.0, 0.0 } }, 0, { 0, { 0.0, 0.0 } }, 2.0, 4.0, 0.8, 1.2, { 2.0, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 2, { { { 0.55, 0, 0 }, { 25.0, 1, 0 } }, { { 0.45, 0, 0 }, { 50.0, 1, 0 } } }, 0 };
static const DtFn S246 = { DT_SINGLE, "single", "single", "shrub", "star", "shrub", NULL, "mound", 1, { 7, { 1.5, 2.1, 6.0, 9.0, 0.0 } }, 0, { 0, { 0.0, 0.0 } }, 0.0, 0.0, 1.0, 1.4, { 0.0, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 2, { { { 0.55, 0, 0 }, { 25.0, 1, 0 } }, { { 0.45, 0, 0 }, { 50.0, 1, 0 } } }, 0 };
static const DtFn S247 = { DT_SINGLE, "single", "single", "coffin", "rect", "coffin", NULL, "up", 1, { 4, { 2.4, 3.2, 1.6, 2.0, 0.0 } }, 1, { 1, { 0.8, 0.0 } }, 0.0, 0.0, 1.0, 1.4, { 0.0, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 2, { { { 0.75, 0, 0 }, { 5.0, 1, 0 } }, { { 0.25, 0, 0 }, { 40.0, 1, 0 } } }, 0 };
static const DtFn S248 = { DT_SCATTER, "scatter", "scatter", "coffin", "rect", "coffin", NULL, "up", 1, { 4, { 1.2, 1.6, 1.0, 1.3, 0.0 } }, 0, { 0, { 0.0, 0.0 } }, 2.0, 3.0, 0.8, 1.1, { 1.8, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 2, { { { 0.7, 0, 0 }, { 6.0, 1, 0 } }, { { 0.3, 0, 0 }, { 40.0, 1, 0 } } }, 0 };
static const DtFn S249 = { DT_SINGLE, "single", "single", "wood", "disc", "wood", NULL, "up", 1, { 2, { 0.9, 1.2, 24.0, 0.0, 0.0 } }, 1, { 2, { 0.25, 0.0 } }, 0.0, 0.0, 1.4, 1.8, { 0.0, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 2, { { { 0.75, 0, 0 }, { 5.0, 1, 0 } }, { { 0.25, 0, 0 }, { 40.0, 1, 0 } } }, 0 };
static const DtFn S250 = { DT_SCATTER, "scatter", "scatter", "wood", "caps", "wood", NULL, "up", 1, { 5, { 2.4, 3.4, 0.55, 0.0, 0.0 } }, 0, { 0, { 0.0, 0.0 } }, 2.0, 3.0, 0.5, 0.6, { 1.8, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 2, { { { 0.7, 0, 0 }, { 6.0, 1, 0 } }, { { 0.3, 0, 0 }, { 40.0, 1, 0 } } }, 0 };
static const DtFn S251 = { DT_SINGLE, "single", "single", "wood", "caps", "wood", NULL, "up", 1, { 5, { 4.5, 6.5, 0.95, 0.0, 0.0 } }, 1, { 1, { 1.3, 0.0 } }, 0.0, 0.0, 0.6, 0.8, { 0.0, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 2, { { { 0.75, 0, 0 }, { 5.0, 1, 0 } }, { { 0.25, 0, 0 }, { 40.0, 1, 0 } } }, 0 };
static const DtFn S252 = { DT_SINGLE, "single", "single", "headstone", "rect", "headstone", NULL, "up", 1, { 4, { 2.4, 3.0, 1.6, 2.0, 0.0 } }, 1, { 6, { 0.0, 0.0 } }, 0.0, 0.0, 0.4, 0.5, { 0.0, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 2, { { { 0.75, 0, 0 }, { 5.0, 1, 0 } }, { { 0.25, 0, 0 }, { 40.0, 1, 0 } } }, 0 };
static const DtFn S253 = { DT_SUNK, "sunk", "sunk", "pit", "rect", "pit", NULL, NULL, 1, { 4, { 2.4, 3.2, 1.4, 1.8, 0.0 } }, 0, { 0, { 0.0, 0.0 } }, 1.0, 1.0, 0.6, 0.8, { 0.0, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 0, { { { 0 } } }, 0 };
static const DtFn S254 = { DT_SCATTER, "scatter", "scatter", "shroom", "disc", "shroom", NULL, "mound", 1, { 2, { 0.5, 0.65, 12.0, 0.0, 0.0 } }, 0, { 0, { 0.0, 0.0 } }, 3.0, 5.0, 0.5, 0.7, { 1.4, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 2, { { { 0.5, 0, 0 }, { 5.0, 1, 0 } }, { { 0.5, 0, 0 }, { 50.0, 1, 0 } } }, 0 };
static const DtFn S255 = { DT_SINGLE, "single", "single", "headstone", "disc", "headstone", NULL, "up", 1, { 2, { 0.9, 1.1, 16.0, 0.0, 0.0 } }, 1, { 3, { 0.0, 0.0 } }, 0.0, 0.0, 1.6, 2.2, { 0.0, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 2, { { { 0.75, 0, 0 }, { 5.0, 1, 0 } }, { { 0.25, 0, 0 }, { 40.0, 1, 0 } } }, 0 };
static const DtFn S256 = { DT_SUNK, "sunk", "sunk", "pit", "crescent", "pit", NULL, NULL, 1, { 8, { 1.0, 1.4, 0.0, 0.0, 0.0 } }, 0, { 0, { 0.0, 0.0 } }, 1.0, 1.0, 0.3, 0.4, { 0.0, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 0, { { { 0 } } }, 0 };
static const DtFn S257 = { DT_SCATTER, "scatter", "scatter", "rock", "rock", "rock", NULL, "mound", 1, { 0, { 0.9, 1.4, 5.0, 8.0, 0.0 } }, 0, { 0, { 0.0, 0.0 } }, 3.0, 5.0, 0.6, 1.0, { 1.6, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 2, { { { 0.7, 0, 0 }, { 6.0, 1, 0 } }, { { 0.3, 0, 0 }, { 40.0, 1, 0 } } }, 0 };
static const DtFn S258 = { DT_SCATTER, "scatter", "scatter", "pebble", "round", "pebble", NULL, "mound", 1, { 1, { 0.5, 0.75, 0.0, 0.0, 0.0 } }, 0, { 0, { 0.0, 0.0 } }, 6.0, 10.0, 0.35, 0.5, { 1.6, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 2, { { { 0.7, 0, 0 }, { 10.0, 1, 0 } }, { { 0.3, 0, 0 }, { 45.0, 1, 0 } } }, 0 };
static const DtFn S259 = { DT_SINGLE, "single", "single", "rock", "rock", "rock", NULL, "mound", 1, { 0, { 1.8, 2.5, 5.0, 8.0, 0.0 } }, 0, { 0, { 0.0, 0.0 } }, 0.0, 0.0, 1.1, 1.7, { 0.0, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 2, { { { 0.7, 0, 0 }, { 6.0, 1, 0 } }, { { 0.3, 0, 0 }, { 40.0, 1, 0 } } }, 0 };
static const DtFn S260 = { DT_SCATTER, "scatter", "scatter", "rock", "tri", "rock", NULL, "mound", 1, { 10, { 0.7, 1.1, 0.0, 0.0, 0.0 } }, 0, { 0, { 0.0, 0.0 } }, 3.0, 6.0, 0.5, 0.8, { 1.8, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 2, { { { 0.7, 0, 0 }, { 6.0, 1, 0 } }, { { 0.3, 0, 0 }, { 40.0, 1, 0 } } }, 0 };
static const DtFn S261 = { DT_SUNK, "sunk", "sunk", "water", "blob", "water", NULL, NULL, 1, { 13, { 1.3, 1.9, 0.0, 0.0, 0.0 } }, 0, { 0, { 0.0, 0.0 } }, 1.0, 1.0, 0.3, 0.45, { 0.0, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 0, { { { 0 } } }, 0 };
static const DtFn S262 = { DT_SUNK, "sunk", "sunk", "crack", "wig", "crack", NULL, NULL, 1, { 14, { 5.0, 8.0, 0.45, 0.9, 0.0 } }, 0, { 0, { 0.0, 0.0 } }, 1.0, 1.0, 0.4, 0.5, { 0.0, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 0, { { { 0 } } }, 0 };
static const DtFn S263 = { DT_TRAIL, "trail", "trail", "pit", "ell", "pit", NULL, NULL, 1, { 3, { 0.35, 0.45, 0.6, 0.75, 0.0 } }, 0, { 0, { 0.0, 0.0 } }, 5.0, 8.0, 0.0, 0.0, { 1.3, 0.0, 0.0, 0.0 }, { 0.35, NUM_FLOAT, 0 }, 0, { { { 0 } } }, 0 };
static const DtFn S264 = { DT_SUNK, "sunk", "sunk", "pit", "disc", "pit", NULL, NULL, 1, { 2, { 0.4, 0.55, 10.0, 0.0, 0.0 } }, 0, { 0, { 0.0, 0.0 } }, 4.0, 7.0, 0.3, 0.4, { 1.5, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 0, { { { 0 } } }, 0 };
static const DtFn S265 = { DT_SCATTER, "scatter", "scatter", "bone", "bone", "bone", NULL, "up", 1, { 6, { 1.5, 2.2, 0.0, 0.0, 0.0 } }, 0, { 0, { 0.0, 0.0 } }, 2.0, 4.0, 0.4, 0.5, { 1.6, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 2, { { { 0.7, 0, 0 }, { 6.0, 1, 0 } }, { { 0.3, 0, 0 }, { 40.0, 1, 0 } } }, 0 };
static const DtFn S266 = { DT_SINGLE, "single", "single", "bone", "ell", "bone", NULL, "mound", 1, { 3, { 1.1, 1.4, 0.7, 0.9, 0.0 } }, 1, { 4, { 0.22, 0.0 } }, 0.0, 0.0, 0.6, 0.8, { 0.0, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 2, { { { 0.75, 0, 0 }, { 5.0, 1, 0 } }, { { 0.25, 0, 0 }, { 40.0, 1, 0 } } }, 0 };
static const DtFn S267 = { DT_SCATTER, "scatter", "scatter", "shrub", "star", "shrub", NULL, "mound", 1, { 7, { 0.9, 1.3, 5.0, 8.0, 0.0 } }, 0, { 0, { 0.0, 0.0 } }, 2.0, 4.0, 0.8, 1.2, { 2.0, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 2, { { { 0.55, 0, 0 }, { 25.0, 1, 0 } }, { { 0.45, 0, 0 }, { 50.0, 1, 0 } } }, 0 };
static const DtFn S268 = { DT_SINGLE, "single", "single", "shrub", "star", "shrub", NULL, "mound", 1, { 7, { 1.5, 2.1, 6.0, 9.0, 0.0 } }, 0, { 0, { 0.0, 0.0 } }, 0.0, 0.0, 1.0, 1.4, { 0.0, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 2, { { { 0.55, 0, 0 }, { 25.0, 1, 0 } }, { { 0.45, 0, 0 }, { 50.0, 1, 0 } } }, 0 };
static const DtFn S269 = { DT_SCATTER, "scatter", "scatter", "log", "caps", "log", NULL, "up", 1, { 5, { 2.2, 3.2, 0.5, 0.0, 0.0 } }, 0, { 0, { 0.0, 0.0 } }, 2.0, 4.0, 0.45, 0.55, { 1.8, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 2, { { { 0.7, 0, 0 }, { 6.0, 1, 0 } }, { { 0.3, 0, 0 }, { 40.0, 1, 0 } } }, 0 };
static const DtFn S270 = { DT_SCATTER, "scatter", "scatter", "grass", "star", "grass", NULL, "mound", 1, { 7, { 0.6, 0.8, 5.0, 6.0, 0.0 } }, 0, { 0, { 0.0, 0.0 } }, 4.0, 7.0, 0.5, 0.7, { 1.6, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 2, { { { 0.55, 0, 0 }, { 25.0, 1, 0 } }, { { 0.45, 0, 0 }, { 50.0, 1, 0 } } }, 0 };
static const DtFn S271 = { DT_SINGLE, "single", "single", "shroom", "disc", "shroom", NULL, "mound", 1, { 2, { 1.0, 1.3, 16.0, 0.0, 0.0 } }, 0, { 0, { 0.0, 0.0 } }, 0.0, 0.0, 0.8, 1.1, { 0.0, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 2, { { { 0.5, 0, 0 }, { 5.0, 1, 0 } }, { { 0.5, 0, 0 }, { 50.0, 1, 0 } } }, 0 };
static const DtFn S272 = { DT_SCATTER, "scatter", "scatter", "moss", "blob", "moss", NULL, "mound", 1, { 13, { 0.7, 1.0, 0.0, 0.0, 0.0 } }, 0, { 0, { 0.0, 0.0 } }, 2.0, 4.0, 0.5, 0.7, { 1.8, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 2, { { { 0.55, 0, 0 }, { 25.0, 1, 0 } }, { { 0.45, 0, 0 }, { 50.0, 1, 0 } } }, 0 };
static const DtFn S273 = { DT_PATH, "path", "path", "root", "", "root", NULL, "up", 0, { 0, { 0.0, 0.0, 0.0, 0.0, 0.0 } }, 0, { 0, { 0.0, 0.0 } }, 0.0, 0.0, 0.0, 0.0, { 0.6, 6.0, 9.0, 1.4 }, { 0.45, NUM_FLOAT, 0 }, 0, { { { 0 } } }, 0 };
static const DtFn S274 = { DT_SCATTER, "scatter", "scatter", "pebble", "disc", "pebble", NULL, "mound", 1, { 2, { 0.35, 0.45, 10.0, 0.0, 0.0 } }, 0, { 0, { 0.0, 0.0 } }, 5.0, 9.0, 0.3, 0.4, { 1.3, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 2, { { { 0.6, 0, 0 }, { 15.0, 1, 0 } }, { { 0.4, 0, 0 }, { 45.0, 1, 0 } } }, 0 };
static const DtFn S275 = { DT_SINGLE, "single", "single", "log", "ring", "log", NULL, "up", 1, { 12, { 1.3, 1.7, 0.6, 0.0, 0.0 } }, 0, { 0, { 0.0, 0.0 } }, 0.0, 0.0, 0.6, 0.8, { 0.0, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 2, { { { 0.75, 0, 0 }, { 5.0, 1, 0 } }, { { 0.25, 0, 0 }, { 40.0, 1, 0 } } }, 0 };
static const DtFn S276 = { DT_TRAIL, "trail", "trail", "pit", "crescent", "pit", NULL, NULL, 1, { 8, { 0.35, 0.45, 0.0, 0.0, 0.0 } }, 0, { 0, { 0.0, 0.0 } }, 5.0, 8.0, 0.0, 0.0, { 1.2, 0.0, 0.0, 0.0 }, { 0.3, NUM_FLOAT, 0 }, 0, { { { 0 } } }, 0 };
static const DtFn S277 = { DT_SUNK, "sunk", "sunk", "pit", "crescent", "pit", NULL, NULL, 1, { 8, { 1.0, 1.4, 0.0, 0.0, 0.0 } }, 0, { 0, { 0.0, 0.0 } }, 1.0, 1.0, 0.3, 0.4, { 0.0, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 0, { { { 0 } } }, 0 };
static const DtFn S278 = { DT_SCATTER, "scatter", "scatter", "rock", "ell", "rock", NULL, "mound", 1, { 3, { 0.6, 0.8, 0.9, 1.2, 0.0 } }, 0, { 0, { 0.0, 0.0 } }, 3.0, 5.0, 0.5, 0.7, { 1.6, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 2, { { { 0.7, 0, 0 }, { 6.0, 1, 0 } }, { { 0.3, 0, 0 }, { 40.0, 1, 0 } } }, 0 };
static const DtFn S279 = { DT_SINGLE, "single", "single", "crate", "rect", "crate", NULL, "up", 1, { 4, { 2.4, 3.2, 1.6, 2.0, 0.0 } }, 1, { 1, { 0.8, 0.0 } }, 0.0, 0.0, 1.0, 1.4, { 0.0, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 2, { { { 0.75, 0, 0 }, { 5.0, 1, 0 } }, { { 0.25, 0, 0 }, { 40.0, 1, 0 } } }, 0 };
static const DtFn S280 = { DT_SCATTER, "scatter", "scatter", "crate", "rect", "crate", NULL, "up", 1, { 4, { 1.2, 1.6, 1.0, 1.3, 0.0 } }, 0, { 0, { 0.0, 0.0 } }, 2.0, 3.0, 0.8, 1.1, { 1.8, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 2, { { { 0.7, 0, 0 }, { 6.0, 1, 0 } }, { { 0.3, 0, 0 }, { 40.0, 1, 0 } } }, 0 };
static const DtFn S281 = { DT_SINGLE, "single", "single", "wood", "disc", "wood", NULL, "up", 1, { 2, { 0.9, 1.2, 24.0, 0.0, 0.0 } }, 1, { 2, { 0.25, 0.0 } }, 0.0, 0.0, 1.4, 1.8, { 0.0, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 2, { { { 0.75, 0, 0 }, { 5.0, 1, 0 } }, { { 0.25, 0, 0 }, { 40.0, 1, 0 } } }, 0 };
static const DtFn S282 = { DT_SCATTER, "scatter", "scatter", "wood", "caps", "wood", NULL, "up", 1, { 5, { 2.4, 3.4, 0.55, 0.0, 0.0 } }, 0, { 0, { 0.0, 0.0 } }, 2.0, 3.0, 0.5, 0.6, { 1.8, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 2, { { { 0.7, 0, 0 }, { 6.0, 1, 0 } }, { { 0.3, 0, 0 }, { 40.0, 1, 0 } } }, 0 };
static const DtFn S283 = { DT_SINGLE, "single", "single", "wood", "caps", "wood", NULL, "up", 1, { 5, { 4.5, 6.5, 0.95, 0.0, 0.0 } }, 1, { 1, { 1.3, 0.0 } }, 0.0, 0.0, 0.6, 0.8, { 0.0, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 2, { { { 0.75, 0, 0 }, { 5.0, 1, 0 } }, { { 0.25, 0, 0 }, { 40.0, 1, 0 } } }, 0 };
static const DtFn S284 = { DT_SCATTER, "scatter", "scatter", "iron", "caps", "iron", NULL, "up", 1, { 5, { 0.7, 1.0, 0.3, 0.0, 0.0 } }, 0, { 0, { 0.0, 0.0 } }, 5.0, 8.0, 0.3, 0.35, { 1.4, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 2, { { { 0.7, 0, 0 }, { 6.0, 1, 0 } }, { { 0.3, 0, 0 }, { 40.0, 1, 0 } } }, 0 };
static const DtFn S285 = { DT_SINGLE, "single", "single", "iron", "ring", "iron", NULL, "up", 1, { 12, { 1.2, 1.5, 0.5, 0.0, 0.0 } }, 0, { 0, { 0.0, 0.0 } }, 0.0, 0.0, 0.5, 0.7, { 0.0, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 2, { { { 0.75, 0, 0 }, { 5.0, 1, 0 } }, { { 0.25, 0, 0 }, { 40.0, 1, 0 } } }, 0 };
static const DtFn S286 = { DT_PATH, "path", "path", "rope", "", "rope", NULL, "up", 0, { 0, { 0.0, 0.0, 0.0, 0.0, 0.0 } }, 0, { 0, { 0.0, 0.0 } }, 0.0, 0.0, 0.0, 0.0, { 0.5, 6.0, 10.0, 0.9 }, { 0.4, NUM_FLOAT, 0 }, 0, { { { 0 } } }, 0 };
static const DtFn S287 = { DT_SCATTER, "scatter", "scatter", "flag", "rock", "flag", NULL, "mound", 1, { 0, { 0.9, 1.4, 5.0, 8.0, 0.0 } }, 0, { 0, { 0.0, 0.0 } }, 3.0, 5.0, 0.6, 1.0, { 1.6, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 2, { { { 0.7, 0, 0 }, { 6.0, 1, 0 } }, { { 0.3, 0, 0 }, { 40.0, 1, 0 } } }, 0 };
static const DtFn S288 = { DT_SCATTER, "scatter", "scatter", "pebble", "round", "pebble", NULL, "mound", 1, { 1, { 0.5, 0.75, 0.0, 0.0, 0.0 } }, 0, { 0, { 0.0, 0.0 } }, 6.0, 10.0, 0.35, 0.5, { 1.6, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 2, { { { 0.7, 0, 0 }, { 10.0, 1, 0 } }, { { 0.3, 0, 0 }, { 45.0, 1, 0 } } }, 0 };
static const DtFn S289 = { DT_SINGLE, "single", "single", "flag", "rock", "flag", NULL, "mound", 1, { 0, { 1.8, 2.5, 5.0, 8.0, 0.0 } }, 0, { 0, { 0.0, 0.0 } }, 0.0, 0.0, 1.1, 1.7, { 0.0, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 2, { { { 0.7, 0, 0 }, { 6.0, 1, 0 } }, { { 0.3, 0, 0 }, { 40.0, 1, 0 } } }, 0 };
static const DtFn S290 = { DT_SCATTER, "scatter", "scatter", "flag", "tri", "flag", NULL, "mound", 1, { 10, { 0.7, 1.1, 0.0, 0.0, 0.0 } }, 0, { 0, { 0.0, 0.0 } }, 3.0, 6.0, 0.5, 0.8, { 1.8, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 2, { { { 0.7, 0, 0 }, { 6.0, 1, 0 } }, { { 0.3, 0, 0 }, { 40.0, 1, 0 } } }, 0 };
static const DtFn S291 = { DT_SUNK, "sunk", "sunk", "water", "blob", "water", NULL, NULL, 1, { 13, { 1.3, 1.9, 0.0, 0.0, 0.0 } }, 0, { 0, { 0.0, 0.0 } }, 1.0, 1.0, 0.3, 0.45, { 0.0, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 0, { { { 0 } } }, 0 };
static const DtFn S292 = { DT_SUNK, "sunk", "sunk", "crack", "wig", "crack", NULL, NULL, 1, { 14, { 5.0, 8.0, 0.45, 0.9, 0.0 } }, 0, { 0, { 0.0, 0.0 } }, 1.0, 1.0, 0.4, 0.5, { 0.0, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 0, { { { 0 } } }, 0 };
static const DtFn S293 = { DT_TRAIL, "trail", "trail", "pit", "ell", "pit", NULL, NULL, 1, { 3, { 0.35, 0.45, 0.6, 0.75, 0.0 } }, 0, { 0, { 0.0, 0.0 } }, 5.0, 8.0, 0.0, 0.0, { 1.3, 0.0, 0.0, 0.0 }, { 0.35, NUM_FLOAT, 0 }, 0, { { { 0 } } }, 0 };
static const DtFn S294 = { DT_SUNK, "sunk", "sunk", "pit", "disc", "pit", NULL, NULL, 1, { 2, { 0.4, 0.55, 10.0, 0.0, 0.0 } }, 0, { 0, { 0.0, 0.0 } }, 4.0, 7.0, 0.3, 0.4, { 1.5, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 0, { { { 0 } } }, 0 };
static const DtFn S295 = { DT_SCATTER, "scatter", "scatter", "bone", "bone", "bone", NULL, "up", 1, { 6, { 1.5, 2.2, 0.0, 0.0, 0.0 } }, 0, { 0, { 0.0, 0.0 } }, 2.0, 4.0, 0.4, 0.5, { 1.6, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 2, { { { 0.7, 0, 0 }, { 6.0, 1, 0 } }, { { 0.3, 0, 0 }, { 40.0, 1, 0 } } }, 0 };
static const DtFn S296 = { DT_SINGLE, "single", "single", "bone", "ell", "bone", NULL, "mound", 1, { 3, { 1.1, 1.4, 0.7, 0.9, 0.0 } }, 1, { 4, { 0.22, 0.0 } }, 0.0, 0.0, 0.6, 0.8, { 0.0, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 2, { { { 0.75, 0, 0 }, { 5.0, 1, 0 } }, { { 0.25, 0, 0 }, { 40.0, 1, 0 } } }, 0 };
static const DtFn S297 = { DT_SINGLE, "single", "single", "flag", "rect", "flag", NULL, "up", 1, { 4, { 1.2, 1.5, 1.2, 1.5, 0.0 } }, 1, { 3, { 0.0, 0.0 } }, 0.0, 0.0, 1.8, 2.4, { 0.0, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 2, { { { 0.75, 0, 0 }, { 5.0, 1, 0 } }, { { 0.25, 0, 0 }, { 40.0, 1, 0 } } }, 0 };
static const DtFn S298 = { DT_SCATTER, "scatter", "scatter", "iron", "ring", "iron", NULL, "up", 1, { 12, { 0.7, 0.9, 0.3, 0.0, 0.0 } }, 0, { 0, { 0.0, 0.0 } }, 2.0, 4.0, 0.3, 0.35, { 1.6, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 2, { { { 0.7, 0, 0 }, { 6.0, 1, 0 } }, { { 0.3, 0, 0 }, { 40.0, 1, 0 } } }, 0 };
static const DtFn S299 = { DT_SUNK, "sunk", "sunk", "pit", "rect", "pit", NULL, NULL, 1, { 4, { 2.2, 2.8, 2.2, 2.8, 0.0 } }, 0, { 0, { 0.0, 0.0 } }, 1.0, 1.0, 0.5, 0.7, { 0.0, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 0, { { { 0 } } }, 0 };
static const DtFn S300 = { DT_SINGLE, "single", "single", "wood", "disc", "wood", NULL, "up", 1, { 2, { 1.0, 1.2, 20.0, 0.0, 0.0 } }, 1, { 2, { 0.25, 0.0 } }, 0.0, 0.0, 1.2, 1.6, { 0.0, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 2, { { { 0.75, 0, 0 }, { 5.0, 1, 0 } }, { { 0.25, 0, 0 }, { 40.0, 1, 0 } } }, 0 };
static const DtFn S301 = { DT_SCATTER, "scatter", "scatter", "tcolumn", "rock", "tcolumn", NULL, "mound", 1, { 0, { 0.9, 1.4, 5.0, 8.0, 0.0 } }, 0, { 0, { 0.0, 0.0 } }, 3.0, 5.0, 0.6, 1.0, { 1.6, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 2, { { { 0.7, 0, 0 }, { 6.0, 1, 0 } }, { { 0.3, 0, 0 }, { 40.0, 1, 0 } } }, 0 };
static const DtFn S302 = { DT_SCATTER, "scatter", "scatter", "pebble", "round", "pebble", NULL, "mound", 1, { 1, { 0.5, 0.75, 0.0, 0.0, 0.0 } }, 0, { 0, { 0.0, 0.0 } }, 6.0, 10.0, 0.35, 0.5, { 1.6, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 2, { { { 0.7, 0, 0 }, { 10.0, 1, 0 } }, { { 0.3, 0, 0 }, { 45.0, 1, 0 } } }, 0 };
static const DtFn S303 = { DT_SINGLE, "single", "single", "tcolumn", "rock", "tcolumn", NULL, "mound", 1, { 0, { 1.8, 2.5, 5.0, 8.0, 0.0 } }, 0, { 0, { 0.0, 0.0 } }, 0.0, 0.0, 1.1, 1.7, { 0.0, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 2, { { { 0.7, 0, 0 }, { 6.0, 1, 0 } }, { { 0.3, 0, 0 }, { 40.0, 1, 0 } } }, 0 };
static const DtFn S304 = { DT_SCATTER, "scatter", "scatter", "tcolumn", "tri", "tcolumn", NULL, "mound", 1, { 10, { 0.7, 1.1, 0.0, 0.0, 0.0 } }, 0, { 0, { 0.0, 0.0 } }, 3.0, 6.0, 0.5, 0.8, { 1.8, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 2, { { { 0.7, 0, 0 }, { 6.0, 1, 0 } }, { { 0.3, 0, 0 }, { 40.0, 1, 0 } } }, 0 };
static const DtFn S305 = { DT_SUNK, "sunk", "sunk", "water", "blob", "water", NULL, NULL, 1, { 13, { 1.3, 1.9, 0.0, 0.0, 0.0 } }, 0, { 0, { 0.0, 0.0 } }, 1.0, 1.0, 0.3, 0.45, { 0.0, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 0, { { { 0 } } }, 0 };
static const DtFn S306 = { DT_SUNK, "sunk", "sunk", "crack", "wig", "crack", NULL, NULL, 1, { 14, { 5.0, 8.0, 0.45, 0.9, 0.0 } }, 0, { 0, { 0.0, 0.0 } }, 1.0, 1.0, 0.4, 0.5, { 0.0, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 0, { { { 0 } } }, 0 };
static const DtFn S307 = { DT_TRAIL, "trail", "trail", "pit", "ell", "pit", NULL, NULL, 1, { 3, { 0.35, 0.45, 0.6, 0.75, 0.0 } }, 0, { 0, { 0.0, 0.0 } }, 5.0, 8.0, 0.0, 0.0, { 1.3, 0.0, 0.0, 0.0 }, { 0.35, NUM_FLOAT, 0 }, 0, { { { 0 } } }, 0 };
static const DtFn S308 = { DT_SUNK, "sunk", "sunk", "pit", "disc", "pit", NULL, NULL, 1, { 2, { 0.4, 0.55, 10.0, 0.0, 0.0 } }, 0, { 0, { 0.0, 0.0 } }, 4.0, 7.0, 0.3, 0.4, { 1.5, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 0, { { { 0 } } }, 0 };
static const DtFn S309 = { DT_SCATTER, "scatter", "scatter", "bone", "bone", "bone", NULL, "up", 1, { 6, { 1.5, 2.2, 0.0, 0.0, 0.0 } }, 0, { 0, { 0.0, 0.0 } }, 2.0, 4.0, 0.4, 0.5, { 1.6, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 2, { { { 0.7, 0, 0 }, { 6.0, 1, 0 } }, { { 0.3, 0, 0 }, { 40.0, 1, 0 } } }, 0 };
static const DtFn S310 = { DT_SINGLE, "single", "single", "bone", "ell", "bone", NULL, "mound", 1, { 3, { 1.1, 1.4, 0.7, 0.9, 0.0 } }, 1, { 4, { 0.22, 0.0 } }, 0.0, 0.0, 0.6, 0.8, { 0.0, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 2, { { { 0.75, 0, 0 }, { 5.0, 1, 0 } }, { { 0.25, 0, 0 }, { 40.0, 1, 0 } } }, 0 };
static const DtFn S311 = { DT_SCATTER, "scatter", "scatter", "shrub", "star", "shrub", NULL, "mound", 1, { 7, { 0.9, 1.3, 5.0, 8.0, 0.0 } }, 0, { 0, { 0.0, 0.0 } }, 2.0, 4.0, 0.8, 1.2, { 2.0, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 2, { { { 0.55, 0, 0 }, { 25.0, 1, 0 } }, { { 0.45, 0, 0 }, { 50.0, 1, 0 } } }, 0 };
static const DtFn S312 = { DT_SINGLE, "single", "single", "shrub", "star", "shrub", NULL, "mound", 1, { 7, { 1.5, 2.1, 6.0, 9.0, 0.0 } }, 0, { 0, { 0.0, 0.0 } }, 0.0, 0.0, 1.0, 1.4, { 0.0, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 2, { { { 0.55, 0, 0 }, { 25.0, 1, 0 } }, { { 0.45, 0, 0 }, { 50.0, 1, 0 } } }, 0 };
static const DtFn S313 = { DT_SINGLE, "single", "single", "crate", "rect", "crate", NULL, "up", 1, { 4, { 2.4, 3.2, 1.6, 2.0, 0.0 } }, 1, { 1, { 0.8, 0.0 } }, 0.0, 0.0, 1.0, 1.4, { 0.0, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 2, { { { 0.75, 0, 0 }, { 5.0, 1, 0 } }, { { 0.25, 0, 0 }, { 40.0, 1, 0 } } }, 0 };
static const DtFn S314 = { DT_SCATTER, "scatter", "scatter", "crate", "rect", "crate", NULL, "up", 1, { 4, { 1.2, 1.6, 1.0, 1.3, 0.0 } }, 0, { 0, { 0.0, 0.0 } }, 2.0, 3.0, 0.8, 1.1, { 1.8, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 2, { { { 0.7, 0, 0 }, { 6.0, 1, 0 } }, { { 0.3, 0, 0 }, { 40.0, 1, 0 } } }, 0 };
static const DtFn S315 = { DT_SINGLE, "single", "single", "wood", "disc", "wood", NULL, "up", 1, { 2, { 0.9, 1.2, 24.0, 0.0, 0.0 } }, 1, { 2, { 0.25, 0.0 } }, 0.0, 0.0, 1.4, 1.8, { 0.0, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 2, { { { 0.75, 0, 0 }, { 5.0, 1, 0 } }, { { 0.25, 0, 0 }, { 40.0, 1, 0 } } }, 0 };
static const DtFn S316 = { DT_SCATTER, "scatter", "scatter", "wood", "caps", "wood", NULL, "up", 1, { 5, { 2.4, 3.4, 0.55, 0.0, 0.0 } }, 0, { 0, { 0.0, 0.0 } }, 2.0, 3.0, 0.5, 0.6, { 1.8, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 2, { { { 0.7, 0, 0 }, { 6.0, 1, 0 } }, { { 0.3, 0, 0 }, { 40.0, 1, 0 } } }, 0 };
static const DtFn S317 = { DT_SINGLE, "single", "single", "tflag", "rect", "tflag", NULL, "up", 1, { 4, { 2.8, 3.6, 2.8, 3.6, 0.0 } }, 1, { 6, { 0.0, 0.0 } }, 0.0, 0.0, 0.4, 0.5, { 0.0, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 2, { { { 0.75, 0, 0 }, { 5.0, 1, 0 } }, { { 0.25, 0, 0 }, { 40.0, 1, 0 } } }, 0 };
static const DtFn S318 = { DT_SINGLE, "single", "single", "tflag", "disc", "tflag", NULL, "up", 1, { 2, { 1.5, 1.9, 24.0, 0.0, 0.0 } }, 1, { 6, { 0.0, 0.0 } }, 0.0, 0.0, 0.5, 0.7, { 0.0, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 2, { { { 0.75, 0, 0 }, { 5.0, 1, 0 } }, { { 0.25, 0, 0 }, { 40.0, 1, 0 } } }, 0 };
static const DtFn S319 = { DT_SCATTER, "scatter", "scatter", "tcolumn", "hex", "tcolumn", NULL, "up", 1, { 9, { 0.6, 0.85, 0.0, 0.0, 0.0 } }, 0, { 0, { 0.0, 0.0 } }, 3.0, 5.0, 0.5, 0.7, { 1.6, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 2, { { { 0.7, 0, 0 }, { 6.0, 1, 0 } }, { { 0.3, 0, 0 }, { 40.0, 1, 0 } } }, 0 };
static const DtFn S320 = { DT_SUNK, "sunk", "sunk", "glyph", "star", "glyph", NULL, NULL, 1, { 7, { 1.4, 1.9, 6.0, 8.0, 0.0 } }, 0, { 0, { 0.0, 0.0 } }, 1.0, 1.0, 0.3, 0.4, { 0.0, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 0, { { { 0 } } }, 0 };
static const DtFn S321 = { DT_SINGLE, "single", "single", "tcolumn", "tri", "tcolumn", NULL, "up", 1, { 10, { 1.6, 2.0, 0.0, 0.0, 0.0 } }, 1, { 6, { 0.0, 0.0 } }, 0.0, 0.0, 1.4, 1.9, { 0.0, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 2, { { { 0.75, 0, 0 }, { 5.0, 1, 0 } }, { { 0.25, 0, 0 }, { 40.0, 1, 0 } } }, 0 };
static const DtFn S322 = { DT_SUNK, "sunk", "sunk", "pit", "crescent", "pit", NULL, NULL, 1, { 8, { 1.0, 1.4, 0.0, 0.0, 0.0 } }, 0, { 0, { 0.0, 0.0 } }, 1.0, 1.0, 0.3, 0.4, { 0.0, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 0, { { { 0 } } }, 0 };
static const DtFn S323 = { DT_SCATTER, "scatter", "scatter", "shelf", "rock", "shelf", NULL, "mound", 1, { 0, { 0.9, 1.4, 5.0, 8.0, 0.0 } }, 0, { 0, { 0.0, 0.0 } }, 3.0, 5.0, 0.6, 1.0, { 1.6, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 2, { { { 0.7, 0, 0 }, { 6.0, 1, 0 } }, { { 0.3, 0, 0 }, { 40.0, 1, 0 } } }, 0 };
static const DtFn S324 = { DT_SCATTER, "scatter", "scatter", "pebble", "round", "pebble", NULL, "mound", 1, { 1, { 0.5, 0.75, 0.0, 0.0, 0.0 } }, 0, { 0, { 0.0, 0.0 } }, 6.0, 10.0, 0.35, 0.5, { 1.6, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 2, { { { 0.7, 0, 0 }, { 10.0, 1, 0 } }, { { 0.3, 0, 0 }, { 45.0, 1, 0 } } }, 0 };
static const DtFn S325 = { DT_SINGLE, "single", "single", "shelf", "rock", "shelf", NULL, "mound", 1, { 0, { 1.8, 2.5, 5.0, 8.0, 0.0 } }, 0, { 0, { 0.0, 0.0 } }, 0.0, 0.0, 1.1, 1.7, { 0.0, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 2, { { { 0.7, 0, 0 }, { 6.0, 1, 0 } }, { { 0.3, 0, 0 }, { 40.0, 1, 0 } } }, 0 };
static const DtFn S326 = { DT_SCATTER, "scatter", "scatter", "shelf", "tri", "shelf", NULL, "mound", 1, { 10, { 0.7, 1.1, 0.0, 0.0, 0.0 } }, 0, { 0, { 0.0, 0.0 } }, 3.0, 6.0, 0.5, 0.8, { 1.8, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 2, { { { 0.7, 0, 0 }, { 6.0, 1, 0 } }, { { 0.3, 0, 0 }, { 40.0, 1, 0 } } }, 0 };
static const DtFn S327 = { DT_SUNK, "sunk", "sunk", "water", "blob", "water", NULL, NULL, 1, { 13, { 1.3, 1.9, 0.0, 0.0, 0.0 } }, 0, { 0, { 0.0, 0.0 } }, 1.0, 1.0, 0.3, 0.45, { 0.0, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 0, { { { 0 } } }, 0 };
static const DtFn S328 = { DT_SUNK, "sunk", "sunk", "crack", "wig", "crack", NULL, NULL, 1, { 14, { 5.0, 8.0, 0.45, 0.9, 0.0 } }, 0, { 0, { 0.0, 0.0 } }, 1.0, 1.0, 0.4, 0.5, { 0.0, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 0, { { { 0 } } }, 0 };
static const DtFn S329 = { DT_TRAIL, "trail", "trail", "pit", "ell", "pit", NULL, NULL, 1, { 3, { 0.35, 0.45, 0.6, 0.75, 0.0 } }, 0, { 0, { 0.0, 0.0 } }, 5.0, 8.0, 0.0, 0.0, { 1.3, 0.0, 0.0, 0.0 }, { 0.35, NUM_FLOAT, 0 }, 0, { { { 0 } } }, 0 };
static const DtFn S330 = { DT_SUNK, "sunk", "sunk", "pit", "disc", "pit", NULL, NULL, 1, { 2, { 0.4, 0.55, 10.0, 0.0, 0.0 } }, 0, { 0, { 0.0, 0.0 } }, 4.0, 7.0, 0.3, 0.4, { 1.5, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 0, { { { 0 } } }, 0 };
static const DtFn S331 = { DT_SCATTER, "scatter", "scatter", "bone", "bone", "bone", NULL, "up", 1, { 6, { 1.5, 2.2, 0.0, 0.0, 0.0 } }, 0, { 0, { 0.0, 0.0 } }, 2.0, 4.0, 0.4, 0.5, { 1.6, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 2, { { { 0.7, 0, 0 }, { 6.0, 1, 0 } }, { { 0.3, 0, 0 }, { 40.0, 1, 0 } } }, 0 };
static const DtFn S332 = { DT_SINGLE, "single", "single", "bone", "ell", "bone", NULL, "mound", 1, { 3, { 1.1, 1.4, 0.7, 0.9, 0.0 } }, 1, { 4, { 0.22, 0.0 } }, 0.0, 0.0, 0.6, 0.8, { 0.0, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 2, { { { 0.75, 0, 0 }, { 5.0, 1, 0 } }, { { 0.25, 0, 0 }, { 40.0, 1, 0 } } }, 0 };
static const DtFn S333 = { DT_SCATTER, "scatter", "scatter", "weed", "star", "weed", NULL, "mound", 1, { 7, { 0.9, 1.3, 5.0, 8.0, 0.0 } }, 0, { 0, { 0.0, 0.0 } }, 2.0, 4.0, 0.8, 1.2, { 2.0, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 2, { { { 0.55, 0, 0 }, { 25.0, 1, 0 } }, { { 0.45, 0, 0 }, { 50.0, 1, 0 } } }, 0 };
static const DtFn S334 = { DT_SINGLE, "single", "single", "weed", "star", "weed", NULL, "mound", 1, { 7, { 1.5, 2.1, 6.0, 9.0, 0.0 } }, 0, { 0, { 0.0, 0.0 } }, 0.0, 0.0, 1.0, 1.4, { 0.0, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 2, { { { 0.55, 0, 0 }, { 25.0, 1, 0 } }, { { 0.45, 0, 0 }, { 50.0, 1, 0 } } }, 0 };
static const DtFn S335 = { DT_SINGLE, "single", "single", "crate", "rect", "crate", NULL, "up", 1, { 4, { 2.4, 3.2, 1.6, 2.0, 0.0 } }, 1, { 1, { 0.8, 0.0 } }, 0.0, 0.0, 1.0, 1.4, { 0.0, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 2, { { { 0.75, 0, 0 }, { 5.0, 1, 0 } }, { { 0.25, 0, 0 }, { 40.0, 1, 0 } } }, 0 };
static const DtFn S336 = { DT_SCATTER, "scatter", "scatter", "crate", "rect", "crate", NULL, "up", 1, { 4, { 1.2, 1.6, 1.0, 1.3, 0.0 } }, 0, { 0, { 0.0, 0.0 } }, 2.0, 3.0, 0.8, 1.1, { 1.8, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 2, { { { 0.7, 0, 0 }, { 6.0, 1, 0 } }, { { 0.3, 0, 0 }, { 40.0, 1, 0 } } }, 0 };
static const DtFn S337 = { DT_SINGLE, "single", "single", "drift", "disc", "drift", NULL, "up", 1, { 2, { 0.9, 1.2, 24.0, 0.0, 0.0 } }, 1, { 2, { 0.25, 0.0 } }, 0.0, 0.0, 1.4, 1.8, { 0.0, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 2, { { { 0.75, 0, 0 }, { 5.0, 1, 0 } }, { { 0.25, 0, 0 }, { 40.0, 1, 0 } } }, 0 };
static const DtFn S338 = { DT_SCATTER, "scatter", "scatter", "drift", "caps", "drift", NULL, "up", 1, { 5, { 2.4, 3.4, 0.55, 0.0, 0.0 } }, 0, { 0, { 0.0, 0.0 } }, 2.0, 3.0, 0.5, 0.6, { 1.8, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 2, { { { 0.7, 0, 0 }, { 6.0, 1, 0 } }, { { 0.3, 0, 0 }, { 40.0, 1, 0 } } }, 0 };
static const DtFn S339 = { DT_SINGLE, "single", "single", "drift", "caps", "drift", NULL, "up", 1, { 5, { 4.5, 6.5, 0.95, 0.0, 0.0 } }, 1, { 1, { 1.3, 0.0 } }, 0.0, 0.0, 0.6, 0.8, { 0.0, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 2, { { { 0.75, 0, 0 }, { 5.0, 1, 0 } }, { { 0.25, 0, 0 }, { 40.0, 1, 0 } } }, 0 };
static const DtFn S340 = { DT_SCATTER, "scatter", "scatter", "iron", "caps", "iron", NULL, "up", 1, { 5, { 0.7, 1.0, 0.3, 0.0, 0.0 } }, 0, { 0, { 0.0, 0.0 } }, 5.0, 8.0, 0.3, 0.35, { 1.4, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 2, { { { 0.7, 0, 0 }, { 6.0, 1, 0 } }, { { 0.3, 0, 0 }, { 40.0, 1, 0 } } }, 0 };
static const DtFn S341 = { DT_SCATTER, "scatter", "scatter", "shell", "disc", "shell", NULL, "mound", 1, { 2, { 0.4, 0.55, 12.0, 0.0, 0.0 } }, 0, { 0, { 0.0, 0.0 } }, 5.0, 9.0, 0.3, 0.4, { 1.3, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 2, { { { 0.6, 0, 0 }, { 15.0, 1, 0 } }, { { 0.4, 0, 0 }, { 45.0, 1, 0 } } }, 0 };
static const DtFn S342 = { DT_SINGLE, "single", "single", "shell", "ell", "shell", NULL, "mound", 1, { 3, { 1.2, 1.5, 0.8, 1.0, 0.0 } }, 1, { 1, { 0.5, 0.0 } }, 0.0, 0.0, 0.6, 0.8, { 0.0, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 2, { { { 0.5, 0, 0 }, { 15.0, 1, 0 } }, { { 0.5, 0, 0 }, { 45.0, 1, 0 } } }, 0 };
static const DtFn S343 = { DT_SUNK, "sunk", "sunk", "water", "disc", "water", NULL, NULL, 1, { 2, { 1.2, 1.7, 20.0, 0.0, 0.0 } }, 0, { 0, { 0.0, 0.0 } }, 1.0, 1.0, 0.4, 0.6, { 0.0, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 0, { { { 0 } } }, 0 };
static const DtFn S344 = { DT_SCATTER, "scatter", "scatter", "drift", "caps", "drift", NULL, "up", 1, { 5, { 1.8, 2.6, 0.45, 0.0, 0.0 } }, 0, { 0, { 0.0, 0.0 } }, 3.0, 5.0, 0.4, 0.5, { 1.8, 0.0, 0.0, 0.0 }, { 0, NUM_NONE, 0 }, 2, { { { 0.7, 0, 0 }, { 6.0, 1, 0 } }, { { 0.3, 0, 0 }, { 40.0, 1, 0 } } }, 0 };
static const DtFn *const TL_swamp_0[] = { &S0, &S1 };
static const DtFn *const TL_swamp_1[] = { &PLAIN[7], &S2, &PLAIN[2] };
static const DtFn *const TL_swamp_2[] = { &S3, &S4, &S5 };
static const DtFn *const TL_cynder_0[] = { &S6, &S7 };
static const DtFn *const TL_cynder_1[] = { &S8, &S9, &S10 };
static const DtFn *const TL_cynder_2[] = { &S11, &S12, &S13 };
static const DtFn *const TL_targa_0[] = { &S14, &S15 };
static const DtFn *const TL_targa_1[] = { &S16, &S17, &S18 };
static const DtFn *const TL_targa_2[] = { &S19, &S20, &S21 };
static const DtFn *const TL_trederra_0[] = { &PLAIN[4], &S22 };
static const DtFn *const TL_trederra_1[] = { &S23, &S24, &S25 };
static const DtFn *const TL_trederra_2[] = { &PLAIN[4], &S26, &S27 };
static const DtFn *const TL_fortress_0[] = { &PLAIN[1], &S28 };
static const DtFn *const TL_fortress_1[] = { &S29, &S30, &S31 };
static const DtFn *const TL_fortress_2[] = { &PLAIN[1], &S32, &S33 };
static const DtFn *const TL_beast_0[] = { &PLAIN[3], &S34 };
static const DtFn *const TL_beast_1[] = { &S35, &S36, &S37 };
static const DtFn *const TL_beast_2[] = { &PLAIN[3], &S38, &S39 };
static const DtFn *const TL_frontier_0[] = { &S40, &S41 };
static const DtFn *const TL_frontier_1[] = { &S42, &S43, &S44 };
static const DtFn *const TL_frontier_2[] = { &S45, &S46, &S47 };
static const DtFn *const TL_graveyard_0[] = { &PLAIN[0], &S48 };
static const DtFn *const TL_graveyard_1[] = { &PLAIN[5], &S49, &S50 };
static const DtFn *const TL_graveyard_2[] = { &PLAIN[0], &S51, &S52 };
static const DtFn *const TL_forest_0[] = { &PLAIN[2], &S53 };
static const DtFn *const TL_forest_1[] = { &S0, &S2, &S54 };
static const DtFn *const TL_forest_2[] = { &PLAIN[2], &S55, &S56 };
static const DtFn *const TL_dungeon_0[] = { &PLAIN[6], &S57 };
static const DtFn *const TL_dungeon_1[] = { &S58, &S59, &S60 };
static const DtFn *const TL_dungeon_2[] = { &S61, &S62, &S63 };
static const DtFn *const TL_temple_0[] = { &S64, &S65 };
static const DtFn *const TL_temple_1[] = { &S66, &S67, &S68 };
static const DtFn *const TL_temple_2[] = { &S69, &S70, &S71 };
static const DtFn *const TL_coast_0[] = { &S72, &S73 };
static const DtFn *const TL_coast_1[] = { &S74, &S75, &S76 };
static const DtFn *const TL_coast_2[] = { &S77, &S78, &S79 };
static const DtFn *const D2_swamp[] = { &S80, &S81, &S82, &S83, &S84, &S85, &S86, &S87, &S88, &S89, &S90, &S91, &S92, &S93, &S94, &S95, &S96, &S0, &S1, &PLAIN[7], &S2, &PLAIN[2], &S3, &S4, &S5, &S97, &S98, &S99, &S100, &S101 };
static const DtFn *const D2_cynder[] = { &S102, &S103, &S104, &S105, &S106, &S107, &S108, &S109, &S110, &S111, &S6, &S7, &S8, &S9, &S10, &S11, &S12, &S13, &S112, &S113, &S114, &S115, &S116, &S117, &S118, &S119, &S120, &S121, &S122, &S123 };
static const DtFn *const D2_targa[] = { &S124, &S125, &S126, &S127, &S128, &S129, &S130, &S131, &S132, &S133, &S14, &S15, &S16, &S17, &S18, &S19, &S20, &S21, &S134, &S135, &S136, &S137, &S138, &S139, &S140, &S141, &S142, &S143, &S144, &S145 };
static const DtFn *const D2_trederra[] = { &S146, &S147, &S148, &S149, &S150, &S151, &S152, &S153, &S154, &S155, &S156, &S157, &S158, &S159, &S160, &S161, &S162, &S163, &PLAIN[4], &S22, &S23, &S24, &S25, &PLAIN[4], &S26, &S27, &S164, &S165, &S166, &S167 };
static const DtFn *const D2_fortress[] = { &PLAIN[1], &S28, &S29, &S30, &S31, &PLAIN[1], &S32, &S33, &S168, &S169, &S170, &S171, &S172, &S173, &S174, &S175, &S176, &S177, &S178, &S179, &S180, &S181, &S182, &S183, &S184, &S185, &S186, &S187, &S188, &S189 };
static const DtFn *const D2_beast[] = { &PLAIN[3], &S34, &S35, &S36, &S37, &PLAIN[3], &S38, &S39, &S190, &S191, &S192, &S193, &S194, &S195, &S196, &S197, &S198, &S199, &S200, &S201, &S202, &S203, &S204, &S205, &S206, &S207, &S208, &S209, &S210, &S211 };
static const DtFn *const D2_frontier[] = { &S40, &S41, &S42, &S43, &S44, &S45, &S46, &S47, &S212, &S213, &S214, &S215, &S216, &S217, &S218, &S219, &S220, &S221, &S222, &S223, &S224, &S225, &S226, &S227, &S228, &S229, &S230, &S231, &S232, &S233, &S234 };
static const DtFn *const D2_graveyard[] = { &PLAIN[0], &S48, &PLAIN[5], &S49, &S50, &PLAIN[0], &S51, &S52, &S235, &S236, &S237, &S238, &S239, &S240, &S241, &S242, &S243, &S244, &S245, &S246, &S247, &S248, &S249, &S250, &S251, &S252, &S253, &S254, &S255, &S256 };
static const DtFn *const D2_forest[] = { &PLAIN[2], &S53, &S0, &S2, &S54, &PLAIN[2], &S55, &S56, &S257, &S258, &S259, &S260, &S261, &S262, &S263, &S264, &S265, &S266, &S267, &S268, &S269, &S270, &S271, &S272, &S273, &S274, &S275, &S276, &S277, &S278 };
static const DtFn *const D2_dungeon[] = { &PLAIN[6], &S57, &S58, &S59, &S60, &S61, &S62, &S63, &S279, &S280, &S281, &S282, &S283, &S284, &S285, &S286, &S287, &S288, &S289, &S290, &S291, &S292, &S293, &S294, &S295, &S296, &S297, &S298, &S299, &S300 };
static const DtFn *const D2_temple[] = { &S64, &S65, &S66, &S67, &S68, &S69, &S70, &S71, &S301, &S302, &S303, &S304, &S305, &S306, &S307, &S308, &S309, &S310, &S311, &S312, &S313, &S314, &S315, &S316, &S317, &S318, &S319, &S320, &S321, &S322 };
static const DtFn *const D2_coast[] = { &S72, &S73, &S74, &S75, &S76, &S77, &S78, &S79, &S323, &S324, &S325, &S326, &S327, &S328, &S329, &S330, &S331, &S332, &S333, &S334, &S335, &S336, &S337, &S338, &S339, &S340, &S341, &S342, &S343, &S344 };
const DtTiers DT_TIERS2[] = {
    { "swamp", { { "swamp", 2, TL_swamp_0 }, { "swamp", 3, TL_swamp_1 }, { "swamp", 3, TL_swamp_2 } } },
    { "cynder", { { "cynder", 2, TL_cynder_0 }, { "cynder", 3, TL_cynder_1 }, { "cynder", 3, TL_cynder_2 } } },
    { "targa", { { "targa", 2, TL_targa_0 }, { "targa", 3, TL_targa_1 }, { "targa", 3, TL_targa_2 } } },
    { "trederra", { { "trederra", 2, TL_trederra_0 }, { "trederra", 3, TL_trederra_1 }, { "trederra", 3, TL_trederra_2 } } },
    { "fortress", { { "fortress", 2, TL_fortress_0 }, { "fortress", 3, TL_fortress_1 }, { "fortress", 3, TL_fortress_2 } } },
    { "beast", { { "beast", 2, TL_beast_0 }, { "beast", 3, TL_beast_1 }, { "beast", 3, TL_beast_2 } } },
    { "frontier", { { "frontier", 2, TL_frontier_0 }, { "frontier", 3, TL_frontier_1 }, { "frontier", 3, TL_frontier_2 } } },
    { "graveyard", { { "graveyard", 2, TL_graveyard_0 }, { "graveyard", 3, TL_graveyard_1 }, { "graveyard", 3, TL_graveyard_2 } } },
    { "forest", { { "forest", 2, TL_forest_0 }, { "forest", 3, TL_forest_1 }, { "forest", 3, TL_forest_2 } } },
    { "dungeon", { { "dungeon", 2, TL_dungeon_0 }, { "dungeon", 3, TL_dungeon_1 }, { "dungeon", 3, TL_dungeon_2 } } },
    { "temple", { { "temple", 2, TL_temple_0 }, { "temple", 3, TL_temple_1 }, { "temple", 3, TL_temple_2 } } },
    { "coast", { { "coast", 2, TL_coast_0 }, { "coast", 3, TL_coast_1 }, { "coast", 3, TL_coast_2 } } },
};
const int DT_NTIERS2 = 12;
const DtList DT_DETAILS2[] = {
    { "swamp", 30, D2_swamp },
    { "cynder", 30, D2_cynder },
    { "targa", 30, D2_targa },
    { "trederra", 30, D2_trederra },
    { "fortress", 30, D2_fortress },
    { "beast", 30, D2_beast },
    { "frontier", 31, D2_frontier },
    { "graveyard", 30, D2_graveyard },
    { "forest", 30, D2_forest },
    { "dungeon", 30, D2_dungeon },
    { "temple", 30, D2_temple },
    { "coast", 30, D2_coast },
};
const int DT_NDETAILS2 = 12;
/* @@END@@ */
