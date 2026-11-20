/* groundbig.c: ground_big.py in C: the ground of a big base at figure scale (class Ground, STYLE, _kinds, _floored,
   apply). scenes.compose calls apply(sc) once, after the miniature's place is held, on finished bases of 49.9 mm and
   up with a scenery scale. The scenes.py helpers it needs (clear_small, _remove, _cells, clear_terrain, _bigs,
   _plates, _strips, lay_cut) are copied here as static functions (sc_* below, each naming its Python routine) until
   the G8 / G9 port has them; gg3.evict is cport\place.c's pl_evict, gg2.T_add / tstack / tcut_flat cport\scene.c.
   Each Python try/except of the file is kept (a feature that raises is dropped, as in Python, and counted in
   sc->nlog). */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "layouts.h"

/* gg.COL, the colour table (pictures only): the names this file adds go through these two hooks, which the integration
   points at kinds.c's table (cport\pipeline.c); NULL (a proof build without kinds.c): not kept */
const char *(*ly_col_get)(const char *k);
void (*ly_col_put)(const char *k, const char *v, int overwrite);
static const char *col_get(const char *k) { return ly_col_get ? ly_col_get(k) : NULL; }
static void col_default(const char *k, const char *v) { if (ly_col_put) ly_col_put(k, v, 0); }
static void col_set(const char *k, const char *v) { if (ly_col_put) ly_col_put(k, v, 1); }

#define COS py_cos
#define SIN py_sin
#define TI(f, a) { { (f), NUM_FLOAT, 0 }, { (a), NUM_INT, 0 } }

/* ================================================================ the scenes.py sets */
LyStrSet LY_TERRK, LY_WETK, LY_BRIDGE, LY_SOFT, LY_CHAMFER;
int lyset_has(const LyStrSet *s, const char *k) { int i; for (i = 0; i < s->n; i++) if (!strcmp(s->v[i], k)) return 1; return 0; }
void lyset_add(LyStrSet *s, const char *k) {
    if (lyset_has(s, k)) return;
    if (s->n >= s->cap) { s->cap = s->cap ? 2 * s->cap : 32; s->v = (const char **)realloc(s->v, sizeof(char *) * (size_t)s->cap); }
    s->v[s->n++] = kind_str(k)->s;
}
void ly_sets_default(void) {
    static const char *T[] = { "dune", "bedrock" }, *W[] = { "water", "lava", "bile", "acid", "wash", "tar", "coolant", "ichor" },
        *B[] = { "plate", "tflag", "basalt", "alien", "ice", "duck", "plank", "tatami", "flag", "fleshfloor", "board", "shelf", "obfloor" },
        *C[] = { "slate", "bedrock", "rock", "brick", "concrete", "sandstone", "rubble" };
    size_t i;
    LY_TERRK.n = LY_WETK.n = LY_BRIDGE.n = LY_SOFT.n = LY_CHAMFER.n = 0;
    for (i = 0; i < sizeof T / sizeof *T; i++) lyset_add(&LY_TERRK, T[i]);          /* scenes.TERRK */
    for (i = 0; i < sizeof W / sizeof *W; i++) lyset_add(&LY_WETK, W[i]);           /* scenes.WETK (+ "ichor": style_blight._register) */
    for (i = 0; i < sizeof B / sizeof *B; i++) lyset_add(&LY_BRIDGE, B[i]);         /* gg3.BRIDGE (+ "obfloor": style_obelis._register) */
    lyset_add(&LY_SOFT, "grit");                                                     /* gg3.SOFT */
    for (i = 0; i < sizeof C / sizeof *C; i++) lyset_add(&LY_CHAMFER, C[i]);        /* gg.CHAMFER_PRISMS */
}
static int in_set(const LyStrSet *s, const Kind *k) { return lyset_has(s, k->s); }

/* ================================================================ small helpers */
#define HB (SC.H)
static Geo *uu_nonempty(GeoList *l) {          /* ground_big._u / scenes._u: unary_union of the non-empty ones, else Polygon() */
    GeoList *k = LL(); int i;
    for (i = 0; i < l->n; i++) if (l->v[i] && !g_isempty(l->v[i])) LP(k, l->v[i]);
    return k->n ? g_uu(k) : g_empty();
}
static Geo *biggest(Geo *g) { GeoList *p = g_polys(g); return p->n ? g_max_area(p) : NULL; }          /* _biggest */
static int in_ids(Geo *const *v, int n, const Geo *g) { int i; for (i = 0; i < n; i++) if (v[i] == g) return 1; return 0; }
typedef struct { Geo **v; int n, cap; } Ids;
static void ids_add(Ids *x, Geo *g) { if (x->n >= x->cap) { x->cap = x->cap ? 2 * x->cap : 64; x->v = (Geo **)realloc(x->v, sizeof(Geo *) * (size_t)x->cap); } x->v[x->n++] = g; }
static Op *op_with(const Op *o, const GeoList *ps) {          /* (op[0], ps) + tuple(op[2:]) */
    return o->type == OP_PRISM ? op_prism(ps, o->n[0], o->n[1], o->kind) : op_taper(ps, o->n[0], o->n[1], o->n[2], o->kind);
}
static void chop_drop(Chop *c) { if (c && --c->rc == 0) { kind_unref(c->kind); geo_unref(c->poly); ol_free(&c->ops); free(c); } }
static void set_ops(Scene *s, OpList *ops) { ol_free(&s->ops); s->ops = *ops; }
static void set_list(GeoList *dst, GeoList *src) { gl_free(dst); *dst = *src; }
static void set_tcuts(Scene *s, TcutList *t) {
    int i; for (i = 0; i < s->tcuts.n; i++) { gl_free(&s->tcuts.v[i].polys); kind_unref(s->tcuts.v[i].kind); }
    free(s->tcuts.v); s->tcuts = *t;
}
static void set_gcuts(Scene *s, GcutList *t) {
    int i; for (i = 0; i < s->gcuts.n; i++) { gl_free(&s->gcuts.v[i].polys); kind_unref(s->gcuts.v[i].kind); }
    free(s->gcuts.v); s->gcuts = *t;
}

/* ================================================================ copies of scenes.py routines */
/* scenes._remove(s, dead) */
static void sc_remove(Scene *s, GeoList *dead) {
    Ids ids = { 0 }; PlHits tree; OpList ops = { 0 }; int i, j; GeoList feet = { 0 }; ChopList chops = { 0 }; TcutList tc = { 0 };
    if (!dead->n) return;
    for (i = 0; i < dead->n; i++) ids_add(&ids, dead->v[i]);
    if (pl_hits_init(&tree, dead) < 0) { free(ids.v); ly_raise(); }
    for (i = 0; i < s->ops.n; i++) {
        Op *op = s->ops.v[i]; GeoList ps = { 0 }; int ride_ok = !in_set(&LY_BRIDGE, op->kind) && !in_set(&LY_TERRK, op->kind);
        for (j = 0; j < op->polys.n; j++) {
            Geo *p = op->polys.v[j];
            if (in_ids(ids.v, ids.n, p)) continue;
            if (ride_ok && op->n[0].v > HB + 0.05) {          /* rider(p, op[2]) */
                GEOSGeometry *rp = shp_representative_point(p->g); int hit = rp ? pl_hits_hit(&tree, rp) : -1;
                shp_free(rp);
                if (hit < 0) ly_raise();
                if (hit) { ids_add(&ids, p); continue; }
            }
            gl_push(&ps, p);
        }
        if (ps.n) ol_push_own(&ops, ps.n == op->polys.n ? op_ref(op) : op_with(op, &ps));
        gl_free(&ps);
    }
    set_ops(s, &ops);
    for (i = 0; i < s->chops.n; i++) { if (!in_ids(ids.v, ids.n, s->chops.v[i]->poly)) chl_push_own(&chops, s->chops.v[i]); else chop_drop(s->chops.v[i]); }
    free(s->chops.v); s->chops = chops;
    for (i = 0; i < s->feet.n; i++) if (!in_ids(ids.v, ids.n, s->feet.v[i])) gl_push(&feet, s->feet.v[i]);
    set_list(&s->feet, &feet);
    if (s->has_objs) { GeoList o = { 0 }; for (i = 0; i < s->objs.n; i++) if (!in_ids(ids.v, ids.n, s->objs.v[i])) gl_push(&o, s->objs.v[i]); set_list(&s->objs, &o); }
    if (s->has_terr) {
        TerrList t = { 0 };
        for (i = 0; i < s->terr.n; i++) { Terr *e = &s->terr.v[i]; if (!in_ids(ids.v, ids.n, e->fp)) trl_push(&t, e->fp, e->zb, e->zt, e->top); }
        for (i = 0; i < s->terr.n; i++) { geo_unref(s->terr.v[i].fp); geo_unref(s->terr.v[i].top); }
        free(s->terr.v); s->terr = t;
    }
    for (i = 0; i < s->tcuts.n; i++) {
        Tcut *c = &s->tcuts.v[i]; GeoList ps2 = { 0 };
        for (j = 0; j < c->polys.n; j++) {
            Geo *p = c->polys.v[j]; int drop = 0;
            if (c->z1.v > HB + 0.35) { GEOSGeometry *rp = shp_representative_point(p->g); int hit = rp ? pl_hits_hit(&tree, rp) : -1; shp_free(rp); if (hit < 0) ly_raise(); drop = hit; }
            if (!drop) gl_push(&ps2, p);
        }
        if (ps2.n) tcl_push(&tc, &ps2, c->z0, c->z1, c->kind);
        gl_free(&ps2);
    }
    set_tcuts(s, &tc);
    pl_hits_free(&tree); free(ids.v);
}
static int is_cell(Scene *s, Geo *p) { int i; if (!s->has_terr) return 0; for (i = 0; i < s->terr.n; i++) if (s->terr.v[i].fp == p) return 1; return 0; }          /* id(p) in scenes._cells(s) */
/* scenes.clear_small(s, g) */
static void sc_clear_small(Scene *s, Geo *g) {
    ShpPrep gp = shp_prep(g->g); GeoList *dead = LL(); int i, j; GcutList gc = { 0 };
    for (i = 0; i < s->ops.n; i++) {
        Op *op = s->ops.v[i];
        if (in_set(&LY_BRIDGE, op->kind) || in_set(&LY_TERRK, op->kind)) continue;
        for (j = 0; j < op->polys.n; j++) {
            Geo *p = op->polys.v[j]; int hit;
            if (!(g_area(p) < 10) || is_cell(s, p)) continue;
            hit = shp_prep_intersects(&gp, p->g); if (hit < 0) { shp_prep_free(&gp); ly_raise(); }
            if (hit) LP(dead, p);
        }
    }
    sc_remove(s, dead);
    for (i = 0; i < s->gcuts.n; i++) {
        Gcut *c = &s->gcuts.v[i]; GeoList ps2 = { 0 };
        for (j = 0; j < c->polys.n; j++) {
            Geo *p = c->polys.v[j]; int drop = 0;
            if (kind_is(c->kind, "pit") && g_area(p) < 2.5) { drop = shp_prep_intersects(&gp, p->g); if (drop < 0) { shp_prep_free(&gp); ly_raise(); } }
            if (!drop) gl_push(&ps2, p);
        }
        if (ps2.n) gcl_push(&gc, &ps2, c->depth, c->kind);
        gl_free(&ps2);
    }
    set_gcuts(s, &gc);
    shp_prep_free(&gp);
}
/* scenes.clear_terrain(s, g) */
static void sc_clear_terrain(Scene *s, Geo *g) {
    Ids inop = { 0 }; GeoList *dead = LL(); Geo **swk = NULL, **swv = NULL; int nsw = 0, i, j; Geo *gb = NULL;
    if (!s->has_terr || !s->terr.n) return;
    for (i = 0; i < s->ops.n; i++) if (s->ops.v[i]->type == OP_PRISM && in_set(&LY_TERRK, s->ops.v[i]->kind)) for (j = 0; j < s->ops.v[i]->polys.n; j++) ids_add(&inop, s->ops.v[i]->polys.v[j]);
    swk = (Geo **)LM(sizeof(Geo *) * (size_t)(s->terr.n + 1)); swv = (Geo **)LM(sizeof(Geo *) * (size_t)(s->terr.n + 1));
    for (i = 0; i < s->terr.n; i++) {
        Geo *fp = s->terr.v[i].fp; double a;
        if (!in_ids(inop.v, inop.n, fp) || !g_intersects(fp, g)) continue;
        a = g_area(g_inter(fp, g));
        if (a < 1e-3) continue;
        if (a <= 0.15 * g_area(fp)) {
            GeoList *q = LL(), *pp; int k;
            if (!gb || g_isempty(gb)) gb = g_buf(g, 0.35);          /* gb = gb or g.buffer(0.35) */
            pp = g_polys(g_diff(fp, gb));
            for (k = 0; k < pp->n; k++) if (g_area(pp->v[k]) > 3 && g_wide(pp->v[k], 1.6)) LP(q, pp->v[k]);
            if (q->n == 1) {
                int f; for (f = 0; f < nsw; f++) if (swk[f] == fp) break;
                swv[f] = g_simp(q->v[0], 0.03); if (f == nsw) { swk[nsw] = fp; nsw++; }
                continue;
            }
        }
        LP(dead, fp);
    }
    free(inop.v);
#define SWAP(p) ({ Geo *_p = (p), *_r = _p; int _f; for (_f = 0; _f < nsw; _f++) if (swk[_f] == _p) { _r = swv[_f]; break; } _r; })
    if (nsw) {
        OpList ops = { 0 }; GeoList feet = { 0 }; TerrList t = { 0 };
        for (i = 0; i < s->ops.n; i++) { GeoList ps = { 0 }; for (j = 0; j < s->ops.v[i]->polys.n; j++) gl_push(&ps, SWAP(s->ops.v[i]->polys.v[j])); ol_push_own(&ops, op_with(s->ops.v[i], &ps)); gl_free(&ps); }
        set_ops(s, &ops);
        for (i = 0; i < s->feet.n; i++) gl_push(&feet, SWAP(s->feet.v[i]));
        set_list(&s->feet, &feet);
        for (i = 0; i < s->terr.n; i++) {
            Terr *e = &s->terr.v[i]; Geo *w = SWAP(e->fp);
            if (w != e->fp) trl_push(&t, w, e->zb, e->zt, g_inset(w, 0.4)); else trl_push(&t, e->fp, e->zb, e->zt, e->top);
        }
        for (i = 0; i < s->terr.n; i++) { geo_unref(s->terr.v[i].fp); geo_unref(s->terr.v[i].top); }
        free(s->terr.v); s->terr = t;
    }
#undef SWAP
    if (dead->n) {
        GeoList *all = LL(); gl_extend(all, dead);
        for (i = 0; i < s->ops.n; i++) {
            Op *op = s->ops.v[i];
            if (in_set(&LY_BRIDGE, op->kind) || in_set(&LY_TERRK, op->kind)) continue;
            for (j = 0; j < op->polys.n; j++) {
                Geo *p = op->polys.v[j], *rp; int k, hit = 0;
                if (!(g_area(p) < 10) || !(op->n[0].v > HB + 0.05)) continue;
                rp = g_reppt(p);
                for (k = 0; k < dead->n; k++) if (g_contains(dead->v[k], rp)) { hit = 1; break; }
                if (hit) LP(all, p);
            }
        }
        sc_remove(s, all);
    }
}
/* scenes._bigs(s) */
static GeoList *sc_bigs(Scene *s) {
    GeoList *out = LL(); int i, j;
    for (i = 0; i < s->ops.n; i++) {
        Op *op = s->ops.v[i];
        if (in_set(&LY_SOFT, op->kind) || in_set(&LY_BRIDGE, op->kind) || in_set(&LY_TERRK, op->kind)) continue;
        for (j = 0; j < op->polys.n; j++) { Geo *p = op->polys.v[j]; if (g_area(p) >= 10 && !is_cell(s, p) && op->n[0].v <= HB + 0.05) LP(out, p); }
    }
    return out;
}
/* scenes._plates(s): [(top z, union)], highest first. z keeps its Python type (round(op[3], 3)). */
typedef struct { Num z; Geo *u; } Plate;
static int sc_plates(Scene *s, Plate **out) {
    Num *zs = (Num *)LM(sizeof(Num) * (size_t)(s->ops.n + 1)); GeoList **ls = (GeoList **)LM(sizeof(GeoList *) * (size_t)(s->ops.n + 1)); int nz = 0, i, j, k;
    Plate *p;
    for (i = 0; i < s->ops.n; i++) {
        Op *op = s->ops.v[i]; Num z;
        if (op->type != OP_PRISM || !in_set(&LY_BRIDGE, op->kind)) continue;
        z = num_round(op->n[1], 3);
        for (j = 0; j < nz; j++) if (zs[j].v == z.v) break;
        if (j == nz) { zs[nz] = z; ls[nz] = LL(); nz++; }
        gl_extend(ls[j], &op->polys);
    }
    for (i = 0; i < nz; i++) for (j = i + 1; j < nz; j++) if (zs[j].v > zs[i].v) { Num t = zs[i]; GeoList *tl = ls[i]; zs[i] = zs[j]; ls[i] = ls[j]; zs[j] = t; ls[j] = tl; }
    p = (Plate *)LM(sizeof(Plate) * (size_t)(nz + 1));
    for (k = 0; k < nz; k++) { p[k].z = zs[k]; p[k].u = g_uu(ls[k]); }
    *out = p;
    return nz;
}
/* scenes._strips(g, step=3.0) */
static GeoList *sc_strips(Geo *g, double step) {
    GeoList *out = LL(); double b[4], *xs, *ys; int nx, ny, i, j;
    if (g_isempty(g)) return out;
    g_bounds(g, b);
    nx = ly_arange(b[0], b[2], step, &xs); ny = ly_arange(b[1], b[3], step, &ys);
    for (i = 0; i < nx; i++) for (j = 0; j < ny; j++) gl_extend(out, g_polys(g_inter(g, g_box(xs[i], ys[j], xs[i] + step, ys[j] + step))));
    return out;
}
/* scenes.lay_cut(sc, g, depth, kind, meet=False) */
static Geo *sc_lay_cut(LyGbSc *sc, Geo *g, double depth, const char *kind, int meet) {
    Scene *s = sc->s; GeoList *ob = LL(), *bg; Geo *obu; int i;
    g = g_inter(g, sc->wide);
    bg = sc_bigs(s);
    for (i = 0; i < bg->n; i++) if (g_intersects(bg->v[i], g)) LP(ob, bg->v[i]);
    if (sc->core && !meet) LP(ob, sc->core);
    obu = uu_nonempty(ob);
    if (!g_isempty(obu)) g = g_diff(g, g_buf(obu, 0.35));
    if (g_isempty(g)) return g;
    if (pl_terr(s)) sc_clear_terrain(s, g_buf(g, 0.4));
    sc_clear_small(s, g_buf(g, 0.15)); LC(pl_evict(s, g));
    if (pl_terr(s)) {
        GeoList *tl = LL(); Geo *tu, *flat;
        for (i = 0; i < s->terr.n; i++) if (g_intersects(s->terr.v[i].fp, g)) LP(tl, s->terr.v[i].fp);
        tu = uu_nonempty(tl);
        flat = !g_isempty(tu) ? g_diff(g, tu) : g;
        if (!g_isempty(flat)) S_gcut(s, g_polys(flat), NF(depth), kind);
        if (!g_isempty(tu)) S_tcutflat(s, sc_strips(g_inter(g, tu), 3.0), NF(depth), kind);
        return g;
    }
    {
        Geo *rest = g; Plate *pl; int np = sc_plates(s, &pl);
        for (i = 0; i < np; i++) {
            GeoList *part = g_polys(g_inter(rest, pl[i].u));
            if (part->n) S_tcut(s, part, num_sub(pl[i].z, NF(depth)), num_add(pl[i].z, NF(0.3)), kind);
            rest = g_diff(rest, pl[i].u);
        }
        if (!g_isempty(rest)) S_gcut(s, g_polys(rest), NF(depth), kind);
    }
    return g;
}

/* ================================================================ class Ground */
typedef struct { Geo *poly; double h, run, e, zb; } Rise;
typedef struct { int has; int n; double r0, r1, h0, h1; const char *kind; } Rocks;          /* the (n, r0, r1, h0, h1, kind) tuples */
typedef struct {
    LyGbSc *sc; Scene *s; PyRandom rng;
    double R, HS, H_, SINK, LAP, A, rk; int terr, made, on_floors;
    Geo *free_, *body, *wet_all, *wet, *disc, *floors;
    Plate *plates; int nplates;
    GeoList *mine; Rise *rises; int nr, cr;
} Gb;
static const Tier ROCK_T[2] = { TI(0.7, 6), TI(0.3, 40) }, PEB_T[2] = { TI(0.7, 10), TI(0.3, 45) }, PEB1[1] = { TI(1.0, 30) };
#define RNG (&g->rng)
#define U(a, b) py_uniform(RNG, (a), (b))
#define RI(a, b) ((int)py_randint(RNG, (a), (b)))
#define RND() py_random(RNG)

/* the per-call try of the Python: run body; on a raise restore the jump target and report 0 */
#define TRY(body) do { jmp_buf _jb, *_prev = ly_jb; ly_jb = &_jb; if (setjmp(_jb) == 0) { body; ly_jb = _prev; } else { ly_jb = _prev; g->sc->nlog++; } } while (0)

static double fz_max(double a, double b) { return b > a ? b : a; }          /* max(a, b) */
static double fz_min(double a, double b) { return b < a ? b : a; }          /* min(a, b) */

/* Ground.floor_z(g) */
static double gb_floor_z(Gb *g, Geo *x) {
    int i;
    for (i = 0; i < g->nplates; i++) {
        GEOSGeometry *q = shp_intersection(g->plates[i].u->g, x->g); double a;
        if (!q) { g->sc->nlog++; continue; }          /* except Exception: logged, the next plate */
        a = shp_area(q); shp_free(q);
        if (a > 0.3 * g_area(x)) return fz_max(g->H_, g->plates[i].z.v);
    }
    return g->H_;
}
/* Ground._hmax(u) */
static double gb__hmax(double u) {
    static const double P[6][2] = { { 0.0, 0.45 }, { 0.5, 0.5 }, { 0.62, 0.9 }, { 0.72, 1.5 }, { 0.82, 2.5 }, { 0.92, 4.0 } }; int i;
    for (i = 0; i < 5; i++) { double u0 = P[i][0], h0 = P[i][1], u1 = P[i + 1][0], h1 = P[i + 1][1]; if (u <= u1) return u >= u0 ? h0 + (h1 - h0) * (u - u0) / (u1 - u0) : h0; }
    return 4.0;
}
/* Ground.hmax(g) */
static double gb_hmax(Gb *g, Geo *x) {
    double u = g_dist(x, g_pt(0, 0)) / g->R;
    if (!g_isempty(g->body) && g_intersects(x, g->body)) return 0.7 * g->HS;
    return g->HS * gb__hmax(u);
}
/* Ground.ok(g, gap=0.8, wet=True, inside=0.55): wet 0 = False, 1 = True, 2 = "all" */
static int gb_ok(Gb *g, Geo *x, double gap, int wet, double inside) {
    Geo *w; double a; int i;
    if (!x || g_isempty(x) || !g_valid(x)) return 0;
    if (g_intersects(x, g->free_)) return 0;
    w = wet == 2 ? g->wet_all : g->wet;
    if (wet && !g_isempty(w) && g_intersects(x, g_buf(w, 0.5))) return 0;
    a = g_area(x);
    if (a <= 0 || g_area(g_inter(x, g->disc)) < inside * a) return 0;
    if (!g->on_floors && !g_isempty(g->floors) && g_area(g_inter(x, g->floors)) > 0.05 * a) return 0;
    for (i = 0; i < g->mine->n; i++) if (g_dist(x, g->mine->v[i]) < gap) return 0;
    return 1;
}
/* Ground.spot(u0, u1) */
static void gb_spot(Gb *g, double u0, double u1, double *x, double *y, double *t) {
    double lo = fz_max(u0 * g->R, g->rk + 1.0), hi = fz_max(lo + 0.5, u1 * g->R), d;
    *t = U(0, 2 * PI);
    d = sqrt(U(lo * lo, hi * hi));
    *x = d * COS(*t); *y = d * SIN(*t);
}
/* what the make() closures return: got[0] is the foot */
typedef struct { Geo *g[4]; double d[4]; GeoList *l; } Got;
typedef int (*MkFn)(Gb *g, const void *kw, double x, double y, double t, Got *got);
/* Ground.try_place(make, u0, u1, tries=40, **kw) */
static int gb_try_place(Gb *g, MkFn mk, const void *kw, double u0, double u1, double gap, int wet, double inside, Got *got) {
    int t;
    for (t = 0; t < 40; t++) {
        double x, y, a;
        gb_spot(g, u0, u1, &x, &y, &a);
        memset(got, 0, sizeof *got);
        if (!mk(g, kw, x, y, a, got)) continue;
        if (gb_ok(g, got->g[0], gap, wet, inside)) return 1;
    }
    return 0;
}
/* Ground.clear_under(g) */
static void gb_clear_under(Gb *g, Geo *x) {
    Scene *s = g->s; Geo *gb; TcutList out = { 0 }; int i, j;
    TRY({ sc_clear_small(s, g_buf(x, 0.3)); LC(pl_evict(s, x)); });
    gb = g_buf(x, 0.3);
    for (i = 0; i < s->tcuts.n; i++) {
        Tcut *c = &s->tcuts.v[i]; GeoList qs = { 0 };
        for (j = 0; j < c->polys.n; j++) {
            Geo *p = c->polys.v[j];
            if (g_intersects(p, gb)) { GeoList *q = g_polys(g_diff(p, gb)); int k; for (k = 0; k < q->n; k++) if (g_area(q->v[k]) > 0.02) gl_push(&qs, q->v[k]); }
            else if (g_area(p) > 0.02) gl_push(&qs, p);
        }
        if (qs.n) tcl_push(&out, &qs, c->z0, c->z1, c->kind);
        gl_free(&qs);
    }
    set_tcuts(s, &out);
}
static void gb_take(Gb *g, Geo *x) { LP(g->mine, x); g->made++; }
/* Ground.K(kind): this file's own name for a material (counted as terrain) */
static const char *gb_K(const char *kind) {
    char base[256], name[300]; size_t L = strlen(kind);
    if (!strncmp(kind, "gb_", 3)) return kind;
    snprintf(base, sizeof base, "%s", kind);
    if (L >= 2 && (!strcmp(kind + L - 2, "_p") || !strcmp(kind + L - 2, "_s"))) base[L - 2] = 0;
    snprintf(name, sizeof name, "gb_%s", base);
    { const char *c = col_get(base); if (!c) c = col_get(kind); col_default(name, c ? c : "#8a8a8a"); }          /* gg.COL.setdefault(name, ...) */
    if (lyset_has(&LY_CHAMFER, base)) lyset_add(&LY_CHAMFER, name);
    lyset_add(&LY_TERRK, name);
    return kind_str(name)->s;
}
/* Ground.blob(cx, cy, rx, ry, a, lumps=6, rough=0.25) (NULL = None) */
static Geo *gb_blob(Gb *g, double cx, double cy, double rx, double ry, double a, int lumps, double rough) {
    double c = COS(a), s_ = SIN(a), mn = fz_min(rx, ry); GeoList *gs = LL(); int i;
    for (i = 0; i < lumps; i++) {
        double u = U(-0.55, 0.55) * rx, v = U(-0.5, 0.5) * ry, r = mn * U(0.55, 0.85), fx, fy, ang; Geo *e = g_ptbuf(0, 0, 1.0, 20);
        fx = r * rx / mn * U(0.6, 0.9); fy = r * U(0.9, 1.1);
        e = g_scale(e, fx, fy, SHP_ORIGIN_CENTER, 0, 0);
        ang = LF_DEGREES(a) + U(-15, 15);
        e = g_rot(e, ang, SHP_ORIGIN_POINT, 0, 0);
        LP(gs, g_trans(e, cx + u * c - v * s_, cy + u * s_ + v * c));
    }
    return biggest(g_simp(g_bufq(g_bufq(uu_nonempty(gs), rough * mn, 8), -rough * mn, 8), 0.1));
}
static Geo *gb_path(Gb *g, double ax, double ay, double bx, double by, double amp, int n) { return g_wigline(ax, ay, bx, by, RNG, amp, n); }
/* Ground.ribbon(line, w0, w1, step=0.6, jit=0.15) */
static Geo *gb_ribbon(Gb *g, Geo *line, double w0, double w1) {
    double L = g_len(line); int64_t n = ly_int(L / 0.6), i; GeoList *gs = LL();
    if (n < 2) n = 2;
    for (i = 0; i < n + 1; i++) {
        double f = (double)i / n, r; Geo *p = g_interp(line, f * L);
        r = (w0 + (w1 - w0) * f) / 2 * (1 + U(-0.15, 0.15));
        LP(gs, g_ptbuf(g_x(p), g_y(p), fz_max(0.2, r), 12));
    }
    return g_simp(g_bufq(g_bufq(uu_nonempty(gs), 0.25, 8), -0.25, 8), 0.04);
}
static double gb_prof(double u, double e) { return 1 - ly_pow(1 - u, e); }
/* Ground.z_at(g) */
static double gb_z_at(Gb *g, Geo *x) {
    double z = g->H_; GeoList *pts = LL(); int i, j;
    if (g_type(x) == GEOS_POLYGON) { double *xy; int n = g_coords(g_ext(x), &xy); for (i = 0; i < n; i++) LP(pts, g_pt(xy[2 * i], xy[2 * i + 1])); }
    else { Geo *c = g_centroid(x); LP(pts, g_pt(g_x(c), g_y(c))); }
    for (i = 0; i < g->nr; i++) {
        Rise *r = &g->rises[i]; double lo = 0; Geo *ext;
        if (!g_intersects(r->poly, x)) continue;
        ext = g_ext(r->poly);
        for (j = 0; j < pts->n; j++) {
            double v = g_contains(r->poly, pts->v[j]) ? r->h * gb_prof(fz_min(1.0, g_dist(ext, pts->v[j]) / r->run), r->e) : 0.0;
            if (j == 0 || v < lo) lo = v;          /* min(...): the first of equal values */
        }
        z = fz_max(z, r->zb + lo);
    }
    return z;
}
static void gb_add_rise(Gb *g, Geo *poly, double h, double run, double e, double zb) {
    if (g->nr >= g->cr) { g->cr = g->cr ? 2 * g->cr : 32; g->rises = (Rise *)realloc(g->rises, sizeof(Rise) * (size_t)g->cr); }
    g->rises[g->nr].poly = poly; g->rises[g->nr].h = h; g->rises[g->nr].run = run; g->rises[g->nr].e = e; g->rises[g->nr].zb = zb; g->nr++;
}
/* Ground.rise(poly, h, kind, run=None, e=1.7, m=None); run < 0 = None */
static int gb_rise(Gb *g, Geo *poly, double h, const char *kind, double run, double e) {
    Scene *s = g->s; double inr, run0 = run, zb, lift; Geo *crown; int m, k, first = 1, built = 0; GeoList *cur = LL();
    if (!poly || g_isempty(poly)) return 0;
    if (g_type(poly) != GEOS_POLYGON) ly_raise();          /* .exterior of a non-polygon raises */
    poly = g_simp(LA(shp_polygon_from_ring(shp_exterior(poly->g))), 0.04);
    if (!g_valid(poly) || g_area(poly) < 1.0) return 0;
    kind = gb_K(kind);
    inr = 2 * g_area(poly) / g_len(poly);
    run = fz_min(run0 >= 0 ? run0 : 2.6 * h + 0.8, 0.92 * inr);
    crown = g_inset(poly, run * 0.7); zb = gb_floor_z(g, poly); lift = zb - g->H_;
    h = fz_min(h + lift * 0.6, fz_max(0.3, gb_hmax(g, !g_isempty(crown) ? crown : poly) - lift));
    run = fz_min(run0 >= 0 ? run0 : 2.6 * h + 0.8, 0.92 * inr);
    if (h < 0.15 + lift * 0.3 || run < 0.3) return 0;
    { int64_t mm = ly_int(h / 0.3) + 3; m = (int)(mm < 6 ? mm : 6); if (m < 4) m = 4; }
    LP(cur, g_simp(poly, 0.12));
    for (k = 0; k < m; k++) {
        double z0, z1, hh, dd, sink; GeoList *ok = LL(), *tops = LL(); int i;
        if (!cur->n) break;
        z0 = h * gb_prof((double)k / m, e); z1 = h * gb_prof((double)(k + 1) / m, e); hh = z1 - z0; dd = run / m;
        if (hh < 0.015) break;
        for (i = 0; i < cur->n; i++) { Geo *q = g_fit(cur->v[i], dd + 0.03); if (q) LP(ok, q); }
        if (!ok->n) break;
        sink = first ? g->SINK : g->LAP;
        S_taper(s, ok, NF(ly_round(zb + z0 - sink, 4)), NF(ly_round(hh + sink, 4)), NF(ly_round(LF_DEGREES(py_atan(dd / hh)), 3)), kind);
        for (i = 0; i < ok->n; i++) { GeoList *t = g_polys(g_inset(ok->v[i], dd)); int j; for (j = 0; j < t->n; j++) if (g_area(t->v[j]) > 0.25) LP(tops, g_simp(t->v[j], 0.1)); }
        if (g->terr) S_tadd(s, uu_nonempty(ok), NF(zb + z0), NF(zb + z1), uu_nonempty(tops));
        if (first) { S_extend_feet(s, ok); first = 0; }
        built = 1; cur = tops;
    }
    if (built) gb_add_rise(g, poly, h, run, e, zb);
    return built;
}
/* LineString(wiggle(a, b, rng, amp, n)).buffer(w, q) (cap round) */
static Geo *wig_buf(Gb *g, double ax, double ay, double bx, double by, double amp, int n, double w, int q) {
    return g_bufq(g_wigline(ax, ay, bx, by, RNG, amp, n), w, q);
}
/* Ground.shelf(poly, h, kind, steps=2, drift=(0.0, 0.0), cracks=True) */
static int gb_shelf(Gb *g, Geo *poly, double h, const char *kind, int steps, double dx, double dy) {
    Scene *s = g->s; const char *kd; Geo *cur = poly; double z = g->H_, inr; int built = 0, k, nm = 0, i; Geo **mc; double *mz;
    if (h < 0.3) return 0;
    kd = gb_K(kind); inr = 2 * g_area(poly) / g_len(poly);
    mc = (Geo **)LM(sizeof(Geo *) * (size_t)(steps + 1)); mz = (double *)LM(sizeof(double) * (size_t)(steps + 1));
    for (k = 0; k < steps; k++) {
        double zt = g->H_ + h * (k + 1) / steps; GeoList *ps;
        if (k) {
            double d = inr * U(0.22, 0.34);
            cur = biggest(g_inset(g_trans(cur, dx * d, dy * d), d));
            if (!cur || g_area(cur) < 1.5) break;
            cur = g_simp(g_bufq(g_bufq(cur, 0.3, 6), -0.3, 6), 0.05);
        }
        zt = fz_min(zt, g->H_ + gb_hmax(g, cur));
        if (zt <= z + 0.25) break;
        ps = S_prism(s, L1(cur), NF(zt), kd, NF(k == 0 ? g->H_ - g->SINK : z - 0.15), SC.MINW, k == 0);
        if (!ps->n) break;
        if (g->terr) { Geo *u = uu_nonempty(ps); S_tadd(s, u, NF(g->H_), NF(zt), g_inset(uu_nonempty(ps), 0.4)); }
        built = 1; mc[nm] = cur; mz[nm] = zt; nm++; z = zt;
    }
    for (i = 0; i < nm; i++) {
        Geo *c0 = mc[i], *gg_; double zt = mz[i]; GeoList *cs = LL(); int t, nt;
        if (g_area(c0) < 6) continue;
        nt = RI(1, 3);
        for (t = 0; t < nt; t++) {
            Geo *c = g_reppt(c0); double a = U(0, PI), L = sqrt(g_area(c0)) * U(0.6, 1.1), cx = g_x(c), cy = g_y(c);
            LP(cs, wig_buf(g, cx - L / 2 * COS(a), cy - L / 2 * SIN(a), cx + L / 2 * COS(a), cy + L / 2 * SIN(a), 0.4, 5, 0.17, 2));
        }
        gg_ = g_inter(uu_nonempty(cs), g_inset(c0, 0.45));
        if (i + 1 < nm) gg_ = g_diff(gg_, g_buf(mc[i + 1], 0.4));
        if (!g_isempty(gg_)) S_tcut(s, g_polys(gg_), NF(zt - 0.3), NF(zt + 0.3), "crack");
    }
    if (built) gb_add_rise(g, poly, h * 0.5, 0.3, 1.0, g->H_);
    return built;
}
/* Ground.rocks(region, n, r0, r1, h0, h1, kind, gap=0.25, tiers=ROCK_T, tries=None) */
static int gb_rocks(Gb *g, Geo *region, int n, double r0, double r1, double h0, double h1, const char *kind, const Tier *tiers, int nt) {
    Scene *s = g->s; GeoList *got = LL(); double b[4]; const char *kd; int64_t t, m = (int64_t)n * 25; int i;
    if (!region || g_isempty(region)) return 0;
    if (!tiers) { tiers = ROCK_T; nt = 2; }
    g_bounds(region, b); kd = gb_K(kind);
    for (t = 0; t < m; t++) {
        double r, x, y, asp; int nv, ok; Geo *p;
        if (got->n >= n) break;
        r = U(r0, r1); x = U(b[0], b[2]); y = U(b[1], b[3]); asp = U(0.6, 0.95); nv = RI(5, 8);
        p = g_stone(x, y, r, RNG, asp, nv);
        ok = g_contains(region, p) && !g_intersects(p, g->free_);
        if (ok) for (i = 0; i < got->n; i++) if (g_dist(p, got->v[i]) < 0.25) { ok = 0; break; }
        if (!ok) continue;
        if (!g_isempty(g->wet_all) && g_intersects(p, g->wet_all)) continue;
        LP(got, p);
    }
    for (i = 0; i < got->n; i++) {
        Geo *p = got->v[i]; double z = gb_z_at(g, p), h = U(h0, h1);
        h = fz_min(h, fz_max(0.25, gb_hmax(g, p) + 0.4 - (z - g->H_)));
        if (g->terr) { S_tstack(s, L1(p), NF(h), tiers, nt, kd); continue; }
        if (z <= g->H_ + 1e-6) S_stack(s, L1(p), NF(h), tiers, nt, kd, NONE, NULL);
        else S_stack(s, L1(p), NF(h), tiers, nt, kd, NF(z), NULL);
    }
    return got->n;
}
static int gb_rocks_s(Gb *g, Geo *region, const Rocks *r, const Tier *tiers, int nt) { return gb_rocks(g, region, r->n, r->r0, r->r1, r->h0, r->h1, r->kind, tiers, nt); }
/* Ground.cut(g, depth, kind) */
static void gb_cut(Gb *g, Geo *x, double depth, const char *kind) {
    if (!x || g_isempty(x)) return;
    x = g_diff(x, g_buf(g->free_, 0.3));
    if (g_isempty(x)) return;
    {          /* try: lay_cut(sc, g, depth, kind, meet=True) except Exception: s.gcut(polys_of(g), depth, kind) */
        jmp_buf jb, *prev = ly_jb; int failed = 0;
        ly_jb = &jb;
        if (setjmp(jb) == 0) sc_lay_cut(g->sc, x, depth, kind, 1); else failed = 1;
        ly_jb = prev;
        if (failed) S_gcut(g->s, g_polys(x), NF(depth), kind);
    }
}
static Geo *gb_hills(Gb *g) { GeoList *l = LL(); int i; for (i = 0; i < g->nr; i++) LP(l, g_buf(g->rises[i].poly, 0.45)); return uu_nonempty(l); }
/* Ground._strips(g, step) */
static GeoList *gb_strips(Geo *x, double step) {
    double b[4], xx, yy; GeoList *out = LL();
    g_bounds(x, b);
    for (xx = b[0]; xx < b[2]; xx += step) for (yy = b[1]; yy < b[3]; yy += step) gl_extend(out, g_polys(g_inter(x, g_box(xx, yy, xx + step, yy + step))));
    return out;
}
/* Ground.carve(g, depth, kind) -> the part cut into flat ground */
static Geo *gb_carve(Gb *g, Geo *x, double depth, const char *kind) {
    Geo *hill, *on, *flat;
    if (!x || g_isempty(x)) return g_empty();
    x = g_diff(x, g_buf(g->free_, 0.3));
    if (g_isempty(x)) return g_empty();
    hill = gb_hills(g);
    on = !g_isempty(hill) ? g_inter(x, hill) : g_empty();
    flat = !g_isempty(hill) ? g_diff(x, hill) : x;
    if (!g_isempty(flat)) gb_cut(g, flat, depth, kind);
    if (!g_isempty(on)) {
        GeoList *st = gb_strips(on, 2.0); double keys[4096]; GeoList *grp[4096]; int ng = 0, i, j;
        for (i = 0; i < st->n; i++) {
            Geo *q = st->v[i]; double z0, key;
            if (g_area(q) < 0.05) continue;
            z0 = fz_max(g->H_ - 0.37, gb_z_at(g, q) - depth);
            key = fz_max(g->H_ - 0.37, ly_rint(z0 / 0.12) * 0.12);
            for (j = 0; j < ng; j++) if (keys[j] == key) break;
            if (j == ng) { if (ng >= 4096) ly_raise(); keys[ng] = key; grp[ng] = LL(); ng++; }
            LP(grp[j], q);
        }
        for (j = 0; j < ng; j++) {
            GeoList *b = LL(), *qs = LL(), *p; int k;
            for (k = 0; k < grp[j]->n; k++) LP(b, g_buf(grp[j]->v[k], 0.01));
            p = g_polys(uu_nonempty(b));
            for (k = 0; k < p->n; k++) LP(qs, g_simp(p->v[k], 0.05));
            S_tcut(g->s, qs, NF(keys[j]), NF(keys[j] + depth + 6.0), kind);
        }
    }
    return flat;
}
static int any_plate_hit(Gb *g, Geo *x) { Plate *pl; int n = sc_plates(g->s, &pl), i; for (i = 0; i < n; i++) if (g_intersects(x, pl[i].u)) return 1; return 0; }
/* Ground.ripples(pool, kind, n=3) */
static void gb_ripples(Gb *g, Geo *pool, const char *kind, int n) {
    GeoList *rings = LL(); int k; char kp[300];
    if (g->terr || (g->H_ > 0 && any_plate_hit(g, pool))) return;
    for (k = 1; k < n + 1; k++) {
        Geo *a = g_inset(pool, 0.55 * k), *b = g_inset(pool, 0.55 * k + 0.32); GeoList *q; int i;
        if (g_isempty(a) || g_isempty(b)) break;
        q = g_polys(g_diff(a, b));
        for (i = 0; i < q->n; i++) if (g_area(q->v[i]) > 0.3) LP(rings, q->v[i]);
    }
    snprintf(kp, sizeof kp, "%s_pad", kind);
    if (rings->n) { const char *c = col_get(kind); col_default(kp, c ? c : "#406070"); }
    if (rings->n) S_prism(g->s, rings, NF(g->H_ - 0.2), kp, NF(g->H_ - 0.8), SC.MINW, 0);
}
/* Ground.slabs(region, h0, h1, kind, n=6, tilt=None, gap=0.3, joints=True) */
static int gb_slabs(Gb *g, Geo *region, double h0, double h1, const char *kind, int n, int has_tilt, double tx, double ty, double ta) {
    Scene *s = g->s; double b[4], *xy, zf, lift, span; const char *kd; int built = 0, i, nc; GEOSGeometry *pts, *env, *vd;
    g_bounds(region, b);
    xy = (double *)LM(sizeof(double) * 2 * (size_t)(n + 1));
    for (i = 0; i < n; i++) { xy[2 * i] = U(b[0], b[2]); xy[2 * i + 1] = U(b[1], b[3]); }
    pts = shp_multipoint(xy, n);
    kd = gb_K(kind); zf = gb_floor_z(g, region); lift = zf - g->H_;
    env = shp_box(b[0] - 2, b[1] - 2, b[2] + 2, b[3] + 2, 1);
    vd = (pts && env) ? shp_voronoi(pts, env, 0.0, 0) : NULL; shp_free(pts); shp_free(env); LA(vd);
    span = fz_max(b[2] - b[0], b[3] - b[1]) / 2;
    nc = shp_num_geoms(vd);
    for (i = 0; i < nc; i++) {
        Geo *c = LA(shp_clone(shp_geom_n(vd, i))), *q = biggest(g_inset(g_inter(c, region), 0.3 / 2)); double h; GeoList *ps;
        if (!q || g_area(q) < 1.2 || !g_wide(q, 1.2)) continue;
        if (has_tilt) {
            Geo *qc = g_centroid(q); double d = fabs(-(g_x(qc) - tx) * SIN(ta) + (g_y(qc) - ty) * COS(ta));
            h = h0 + (h1 - h0) * fz_max(0.0, 1 - d / fz_max(0.5, span * 0.8)) + U(-0.08, 0.08);
        } else h = U(h0, h1);
        h = fz_min(h + lift, fz_max(lift + 0.3, gb_hmax(g, q)));
        if (h < 0.2 + lift) continue;
        ps = PR(s, L1(q), g->H_ + h, kd);
        if (!ps->n) continue;
        built++;
        if (g->terr) { Geo *u = uu_nonempty(ps); S_tadd(s, u, NF(g->H_), NF(g->H_ + h), g_inset(uu_nonempty(ps), 0.4)); }
        if (g_area(q) > 5 && RND() < 0.6) {
            Geo *cc = g_reppt(q), *x; double a = U(0, PI), L = sqrt(g_area(q)), cx = g_x(cc), cy = g_y(cc);
            x = g_inter(wig_buf(g, cx - L * COS(a), cy - L * SIN(a), cx + L * COS(a), cy + L * SIN(a), 0.3, 4, 0.16, 2), g_inset(q, 0.35));
            if (!g_isempty(x)) S_tcut(s, g_polys(x), NF(g->H_ + h - 0.3), NF(g->H_ + h + 0.3), "crack");
        }
    }
    if (built) gb_add_rise(g, region, fz_min(h0, h1), 0.3, 1.0, zf);
    return built;
}
/* Ground.crack_patch(region, n, depth=0.3, kind="crack", w=0.2) */
static void gb_crack_patch(Gb *g, Geo *region, int n, double depth, const char *kind, double w) {
    double b[4], *xy; GEOSGeometry *pts, *env, *vd; GeoList *segs = LL(); int i, nc; Geo *x;
    g_bounds(region, b);
    xy = (double *)LM(sizeof(double) * 2 * (size_t)(n + 1));
    for (i = 0; i < n; i++) { xy[2 * i] = U(b[0], b[2]); xy[2 * i + 1] = U(b[1], b[3]); }
    pts = shp_multipoint(xy, n); env = shp_box(b[0] - 1, b[1] - 1, b[2] + 1, b[3] + 1, 1);
    vd = (pts && env) ? shp_voronoi(pts, env, 0.0, 0) : NULL; shp_free(pts); shp_free(env); LA(vd);
    nc = shp_num_geoms(vd);
    for (i = 0; i < nc; i++) {
        Geo *cell = LA(shp_clone(shp_geom_n(vd, i))); double *c; int m = g_coords(g_ext(cell), &c), k;
        for (k = 0; k + 1 < m; k++) if (RND() < 0.8) LP(segs, wig_buf(g, c[2 * k], c[2 * k + 1], c[2 * k + 2], c[2 * k + 3], 0.25, 3, w, 2));
    }
    x = g_inter(uu_nonempty(segs), g_inset(region, 0.3));
    if (!g_isempty(x)) gb_carve(g, x, depth, kind);
}
/* LineString(g.coords) of line.offset_curve(d) (ground_big._one_line) */
static Geo *one_line(Geo *line, double d) {
    Geo *o = LA(shp_offset_curve(line->g, d, 16, SHP_JOIN_ROUND, 5.0)          /* LineString.offset_curve: quad_segs=16 (the method; shapely.offset_curve has 8) */); double *xy; int n;
    if (g_type(o) == GEOS_MULTILINESTRING) {
        int i, k = shp_num_geoms(o->g), b = 0; double bl = -1;
        for (i = 0; i < k; i++) { double L = shp_length(shp_geom_n(o->g, i)); if (i == 0 || L > bl) { bl = L; b = i; } }
        o = LA(shp_clone(shp_geom_n(o->g, b)));
    }
    n = g_coords(o, &xy);
    return g_line(xy, n);
}

/* ================================================================ the features: keyword arguments */
typedef struct {
    const char *kind, *bed, *bank, *soft, *stone, *rim, *pit, *water, *wall, *pipe, *hub;
    double r[2], h[2], u[2], elong[2], L[2], w[2], big[2], rim_h[2], bank_h[2];
    int steps[2], n[2], ripples, has_ripples, tilt_off, tang_off, has_run, has_e; double run, e;
    Rocks rocks, scree, pebbles, ejecta;
} Kw;
#define P2(v, a, b) (v[0] || v[1] ? v[0] : (a)), (v[0] || v[1] ? v[1] : (b))
static void pair(const double *v, double a, double b, double *o) { if (v[0] || v[1]) { o[0] = v[0]; o[1] = v[1]; } else { o[0] = a; o[1] = b; } }
static void ipair(const int *v, int a, int b, int *o) { if (v[0] || v[1]) { o[0] = v[0]; o[1] = v[1]; } else { o[0] = a; o[1] = b; } }

/* f_rise */
static int mk_rise(Gb *g, const void *kw_, double x, double y, double t, Got *got) {
    const Kw *kw = (const Kw *)kw_; double r[2], el[2], rx, a, e1, e2;
    pair(kw->r, 0.12, 0.22, r); pair(kw->elong, 1.0, 2.2, el);
    rx = g->R * U(r[0], r[1]); a = t + PI / 2 + U(-0.6, 0.6);
    e1 = ly_pow(U(el[0], el[1]), 0.5); e2 = ly_pow(U(el[0], el[1]), 0.5);
    got->g[0] = gb_blob(g, x, y, rx * e1, rx / e2, a, 6, 0.25);
    return got->g[0] != NULL;
}
static int f_rise(Gb *g, const Kw *kw) {
    double u[2], h[2]; Got got; Geo *x;
    pair(kw->u, 0.5, 0.95, u); pair(kw->h, 0.4, 1.0, h);
    if (!gb_try_place(g, mk_rise, kw, u[0], u[1], 0.8, 1, 0.6, &got)) return 0;
    x = got.g[0]; gb_clear_under(g, x);
    { double hh = U(h[0], h[1]); if (!gb_rise(g, x, hh, kw->kind, -1, 1.7)) return 0; }
    gb_take(g, x);
    if (kw->rocks.has) gb_rocks_s(g, g_buf(x, -0.4), &kw->rocks, NULL, 0);
    return 1;
}
/* f_ridge */
static int mk_ridge(Gb *g, const void *kw_, double x, double y, double t, Got *got) {
    const Kw *kw = (const Kw *)kw_; double L[2], w[2], a, Ln, ax, ay, bx, by, ww, w0, w1; Geo *line, *rb;
    pair(kw->L, 0.35, 0.6, L); pair(kw->w, 2.0, 3.5, w);
    a = t + (kw->tang_off ? 0 : PI / 2) + U(-0.5, 0.5); Ln = g->R * U(L[0], L[1]);
    ax = x - Ln / 2 * COS(a); ay = y - Ln / 2 * SIN(a); bx = x + Ln / 2 * COS(a); by = y + Ln / 2 * SIN(a);
    ww = U(w[0], w[1]); line = gb_path(g, ax, ay, bx, by, Ln * 0.08, 6);
    w0 = ww * U(0.5, 0.8); w1 = ww * U(0.6, 1.0);
    rb = gb_ribbon(g, line, w0, w1);
    if (g_isempty(rb)) return 0;
    got->g[0] = biggest(rb);
    return 1;
}
static int f_ridge(Gb *g, const Kw *kw) {
    double u[2], h[2]; Got got; Geo *x;
    pair(kw->u, 0.6, 0.95, u); pair(kw->h, 0.6, 1.4, h);
    if (!gb_try_place(g, mk_ridge, kw, u[0], u[1], 0.8, 1, 0.55, &got) || !got.g[0]) return 0;
    x = got.g[0]; gb_clear_under(g, x);
    { double hh = U(h[0], h[1]); if (!gb_rise(g, x, hh, kw->kind, kw->has_run ? kw->run : -1, kw->has_e ? kw->e : 1.7)) return 0; }
    gb_take(g, x);
    if (kw->rocks.has) gb_rocks_s(g, g_buf(x, -0.3), &kw->rocks, NULL, 0);
    return 1;
}
/* f_mesa */
static int mk_mesa(Gb *g, const void *kw_, double x, double y, double t, Got *got) {
    const Kw *kw = (const Kw *)kw_; double r[2], rx, a1, a2; Geo *b;
    pair(kw->r, 0.13, 0.2, r);
    rx = g->R * U(r[0], r[1]); a1 = U(1.0, 1.6); a2 = U(0.7, 1.0);
    b = gb_blob(g, x, y, rx * a1, rx * a2, t + PI / 2, 7, 0.12);
    if (!b) return 0;
    got->g[0] = g_simp(b, 0.25); got->d[0] = t;
    return 1;
}
static int f_mesa(Gb *g, const Kw *kw) {
    double u[2], h[2], t, dx, dy; int st[2]; Got got; Geo *x;
    pair(kw->u, 0.8, 1.05, u); pair(kw->h, 2.0, 3.8, h); ipair(kw->steps, 2, 3, st);
    if (!gb_try_place(g, mk_mesa, kw, u[0], u[1], 0.8, 1, 0.4, &got)) return 0;
    x = got.g[0]; t = got.d[0]; gb_clear_under(g, x);
    if (RND() < 0.75) { dx = COS(t) * 0.5; dy = SIN(t) * 0.5; } else { dx = -SIN(t) * 0.5; dy = COS(t) * 0.5; }
    { double hh = U(h[0], h[1]); int ns = RI(st[0], st[1]); if (!gb_shelf(g, x, hh, kw->kind, ns, dx, dy)) return 0; }
    gb_take(g, x);
    if (kw->scree.has) {
        Geo *c = g_centroid(x), *foot = g_inter(g_diff(g_buf(x, 2.2), g_buf(x, 0.3)), g_ptbuf(g_x(c) * 0.8, g_y(c) * 0.8, sqrt(g_area(x)) * 0.9, 16));
        gb_rocks_s(g, g_diff(foot, g->free_), &kw->scree, NULL, 0);
    }
    return 1;
}
/* f_outcrop */
static int mk_outcrop(Gb *g, const void *kw_, double x, double y, double t, Got *got) {
    const Kw *kw = (const Kw *)kw_; double r[2], rr;
    pair(kw->r, 0.07, 0.12, r);
    rr = g->R * U(r[0], r[1]);
    got->g[0] = g_ptbuf(x, y, rr * 1.9, 16); got->d[0] = x; got->d[1] = y; got->d[2] = rr; got->d[3] = t;
    return 1;
}
static int f_outcrop(Gb *g, const Kw *kw) {
    double u[2], h[2], x, y, rr, t; int n[2], k, i, made = 0; Got got; Geo *ft; const char *kd;
    pair(kw->u, 0.75, 1.0, u); pair(kw->h, 1.2, 3.2, h); ipair(kw->n, 3, 6, n);
    if (!gb_try_place(g, mk_outcrop, kw, u[0], u[1], 0.8, 1, 0.5, &got)) return 0;
    ft = got.g[0]; x = got.d[0]; y = got.d[1]; rr = got.d[2]; t = got.d[3]; gb_clear_under(g, ft);
    if (kw->soft) gb_rise(g, gb_blob(g, x, y, rr * 1.9, rr * 1.4, t + PI / 2, 6, 0.25), 0.5, kw->soft, -1, 1.7);
    kd = gb_K(kw->kind); k = RI(n[0], n[1]);
    for (i = 0; i < k; i++) {
        double b = U(0, 2 * PI), d = rr * U(0.0, 1.1) * (i > 0), cx = x + d * COS(b), cy = y + d * SIN(b), r, asp, back, z, hh; int nv; Geo *p;
        r = rr * U(0.45, 0.8) * (i == 0 ? 1.0 : 0.8); asp = U(0.6, 0.9); nv = RI(5, 8);
        p = g_stone(cx, cy, r, RNG, asp, nv);
        if (g_intersects(p, g->free_)) continue;
        back = py_hypot(cx, cy) / g->R;
        z = gb_z_at(g, p);
        hh = fz_min(U(h[0], h[1]) * (0.6 + 0.5 * back), gb_hmax(g, p) - (z - g->H_));
        if (hh < 0.4) continue;
        if (g->terr) S_tstack(g->s, L1(p), NF(hh), ROCK_T, 2, kd);
        else if (z > g->H_ + 1e-6) S_stack(g->s, L1(p), NF(hh), ROCK_T, 2, kd, NF(z), NULL);
        else S_stack(g->s, L1(p), NF(hh), ROCK_T, 2, kd, NONE, NULL);
        made++;
    }
    if (made) gb_take(g, ft);
    return made > 0;
}
/* f_gully */
static int mk_gully(Gb *g, const void *kw_, double x, double y, double t, Got *got) {
    const Kw *kw = (const Kw *)kw_; double L[2], w[2], r0 = g->R * 1.04, p0x = r0 * COS(t), p0y = r0 * SIN(t), a, Ln, p1x, p1y, ww; Geo *line, *ch; GeoList *ban = LL(), *all = LL(); int sg;
    pair(kw->L, 0.35, 0.55, L); pair(kw->w, 1.6, 2.6, w);
    a = t + PI + U(-0.9, 0.9); Ln = g->R * U(L[0], L[1]);
    p1x = p0x + Ln * COS(a); p1y = p0y + Ln * SIN(a);
    line = gb_path(g, p0x, p0y, p1x, p1y, Ln * 0.12, 8); ww = U(w[0], w[1]);
    ch = gb_ribbon(g, line, ww, ww * 0.35);
    { GeoList *b0 = LL(); for (sg = -1; sg <= 1; sg += 2) LP(b0, gb_ribbon(g, one_line(line, sg * (ww * 0.5 + 1.3)), 2.0, 1.0));
      for (sg = 0; sg < b0->n; sg++) if (!g_isempty(b0->v[sg])) LP(ban, g_diff(b0->v[sg], g_buf(ch, 0.45))); }
    LP(all, ch); gl_extend(all, ban);
    got->g[0] = uu_nonempty(all); got->g[1] = ch; got->l = ban;
    return 1;
}
static int f_gully(Gb *g, const Kw *kw) {
    double u[2], bh[2]; Got got; Geo *foot, *ch, *flat; int i, j;
    pair(kw->u, 0.55, 1.05, u); pair(kw->bank_h, 0.5, 1.1, bh);
    if (!gb_try_place(g, mk_gully, kw, u[0], u[1], 0.8, 1, 0.5, &got)) return 0;
    foot = got.g[0]; ch = got.g[1]; gb_clear_under(g, foot);
    for (i = 0; i < got.l->n; i++) { GeoList *q = g_polys(got.l->v[i]); for (j = 0; j < q->n; j++) if (g_area(q->v[j]) > 2.0) { double hh = U(bh[0], bh[1]); gb_rise(g, q->v[j], hh, kw->bank, -1, 1.4); } }
    flat = gb_carve(g, ch, 0.36, kw->bed);
    if (kw->pebbles.has && !g_isempty(flat)) gb_rocks_s(g, g_inset(flat, 0.25), &kw->pebbles, PEB1, 1);
    gb_take(g, foot);
    return 1;
}
/* f_pool */
static int mk_pool(Gb *g, const void *kw_, double x, double y, double t, Got *got) {
    const Kw *kw = (const Kw *)kw_; double r[2], rx, f1, a; Geo *b;
    pair(kw->r, 0.08, 0.15, r);
    rx = g->R * U(r[0], r[1]); f1 = U(1.0, 1.5); a = U(0, PI);
    b = gb_blob(g, x, y, rx * f1, rx, a, 6, 0.25);
    if (!b) return 0;
    got->g[0] = kw->rim ? g_buf(b, 1.6) : b; got->g[1] = b;
    return 1;
}
static int f_pool(Gb *g, const Kw *kw) {
    double u[2], rh[2]; Got got; Geo *foot, *pool, *flat; int ripples = kw->has_ripples ? kw->ripples : 2, i;
    pair(kw->u, 0.52, 0.9, u); pair(kw->rim_h, 0.3, 0.6, rh);
    if (!gb_try_place(g, mk_pool, kw, u[0], u[1], 0.8, 2, 0.55, &got)) return 0;
    foot = got.g[0]; pool = got.g[1]; gb_clear_under(g, foot);
    if (kw->rim) {
        GeoList *lumps = LL(); Geo *ring_ = g_diff(g_buf(pool, 1.7), g_buf(pool, 0.45)), *ext = g_ext(pool); int k, nk = RI(4, 7);
        for (k = 0; k < nk; k++) {
            Geo *p = g_interp(ext, U(0, g_len(ext))), *q; double rx = U(1.0, 1.8), ry = U(0.7, 1.1), a = U(0, PI); int ok;
            q = gb_blob(g, g_x(p), g_y(p), rx, ry, a, 3, 0.25);
            if (!q) continue;
            q = biggest(g_inter(q, ring_));
            ok = q && g_area(q) > 1.0;
            if (ok) for (i = 0; i < lumps->n; i++) if (!(g_dist(q, lumps->v[i]) > 0.2)) { ok = 0; break; }
            if (ok) LP(lumps, q);
        }
        for (i = 0; i < lumps->n; i++) { double hh = U(rh[0], rh[1]); gb_rise(g, lumps->v[i], hh, kw->rim, -1, 1.5); }
    }
    flat = gb_carve(g, pool, 0.36, kw->kind);
    if (ripples && !g_isempty(flat) && g_area(flat) > 0.9 * g_area(pool)) gb_ripples(g, pool, kw->kind, ripples);
    g->wet = uu_nonempty(L2(g->wet, pool)); g->wet_all = uu_nonempty(L2(g->wet_all, pool));
    gb_take(g, foot);
    return 1;
}
/* f_rubble */
static int mk_rubble(Gb *g, const void *kw_, double x, double y, double t, Got *got) {
    const Kw *kw = (const Kw *)kw_; double r[2], rx, a1, a2; Geo *b;
    pair(kw->r, 0.1, 0.16, r);
    rx = g->R * U(r[0], r[1]); a1 = U(1.0, 1.5); a2 = U(0.7, 1.0);
    b = gb_blob(g, x, y, rx * a1, rx * a2, t + PI / 2, 6, 0.25);
    if (!b) return 0;
    got->g[0] = g_buf(b, 1.0); got->g[1] = b;
    return 1;
}
static int f_rubble(Gb *g, const Kw *kw) {
    double u[2], h[2], big[2]; int n[2]; Got got; Geo *foot, *x;
    pair(kw->u, 0.62, 1.0, u); pair(kw->h, 0.6, 1.6, h); pair(kw->big, 0.8, 1.6, big); ipair(kw->n, 10, 18, n);
    if (!gb_try_place(g, mk_rubble, kw, u[0], u[1], 0.8, 1, 0.55, &got)) return 0;
    foot = got.g[0]; x = got.g[1]; gb_clear_under(g, foot);
    { double hh = U(h[0], h[1]); gb_rise(g, x, hh, kw->soft, -1, 1.4); }
    { int k = RI(n[0], n[1]); gb_rocks(g, g_buf(x, 0.8), k, big[0], big[1], 0.4, 1.0, kw->stone, ROCK_T, 2); }
    { int k = RI(n[0], n[1]); gb_rocks(g, g_buf(x, 1.0), k, 0.4, 0.7, 0.25, 0.45, kw->stone, PEB_T, 2); }
    gb_take(g, foot);
    return 1;
}
/* f_strands */
static int mk_strands(Gb *g, const void *kw_, double x, double y, double t, Got *got) {
    const Kw *kw = (const Kw *)kw_; double u[2], L[2], w[2], hx, hy; int n[2], k, i; GeoList *parts = LL(); Geo *x2;
    pair(kw->u, 0.85, 1.05, u); pair(kw->L, 0.35, 0.6, L); pair(kw->w, 1.6, 2.6, w); ipair(kw->n, 3, 6, n);
    hx = g->R * U(u[0], u[1]) * COS(t); hy = g->R * U(u[0], u[1]) * SIN(t);
    k = RI(n[0], n[1]);
    for (i = 0; i < k; i++) {
        double a = t + PI + U(-1.1, 1.1), Ln = g->R * U(L[0], L[1]), ww; Geo *line = gb_path(g, hx, hy, hx + Ln * COS(a), hy + Ln * SIN(a), Ln * 0.1, 7);
        ww = U(w[0], w[1]);
        LP(parts, gb_ribbon(g, line, ww, 0.5));
    }
    x2 = g_diff(uu_nonempty(parts), g_buf(g->free_, 0.8));
    got->g[0] = g_buf(x2, 0.5); got->g[1] = x2; got->d[0] = hx; got->d[1] = hy; got->d[2] = t;
    return 1;
}
static int f_strands(Gb *g, const Kw *kw) {
    double h[2], hh, hx, hy, t; Got got; Geo *foot, *x; GeoList *ps = LL(), *q; const char *kd; int i;
    pair(kw->h, 0.7, 1.2, h);
    if (!gb_try_place(g, mk_strands, kw, 0.9, 1.0, 0.8, 1, 0.4, &got)) return 0;
    foot = got.g[0]; x = got.g[1]; hx = got.d[0]; hy = got.d[1]; t = got.d[2]; gb_clear_under(g, foot);
    kd = gb_K(kw->kind);
    q = g_polys(x);
    for (i = 0; i < q->n; i++) if (g_area(q->v[i]) > 0.6) LP(ps, q->v[i]);
    if (!ps->n) return 0;
    hh = U(h[0], h[1]);
    for (i = 0; i < ps->n; i++) gb_rise(g, ps->v[i], fz_min(hh, gb_hmax(g, ps->v[i]) + 0.3), kd, -1, 1.3);
    if (kw->hub) { Geo *hb = gb_blob(g, hx, hy, g->R * 0.09, g->R * 0.07, t, 4, 0.25); if (hb) gb_rise(g, hb, 2.4, kw->hub, -1, 1.2); }
    gb_take(g, foot);
    return 1;
}
/* f_slabs */
static int mk_slabs(Gb *g, const void *kw_, double x, double y, double t, Got *got) {
    const Kw *kw = (const Kw *)kw_; double r[2], rx, a1; Geo *b;
    pair(kw->r, 0.12, 0.2, r);
    rx = g->R * U(r[0], r[1]); a1 = U(1.1, 1.6);
    b = gb_blob(g, x, y, rx * a1, rx, t + PI / 2, 6, 0.1);
    if (!b) return 0;
    got->g[0] = b; got->d[0] = x; got->d[1] = y; got->d[2] = t;
    return 1;
}
static int f_slabs(Gb *g, const Kw *kw) {
    double u[2], h[2]; int n[2], k; Got got; Geo *x;
    pair(kw->u, 0.6, 1.0, u); pair(kw->h, 0.3, 1.4, h); ipair(kw->n, 5, 9, n);
    if (!gb_try_place(g, mk_slabs, kw, u[0], u[1], 0.8, 1, 0.55, &got)) return 0;
    x = got.g[0]; gb_clear_under(g, x);
    k = RI(n[0], n[1]);
    if (!kw->tilt_off) { double ta = got.d[2] + PI / 2 + U(-0.4, 0.4); if (!gb_slabs(g, x, h[0], h[1], kw->kind, k, 1, got.d[0], got.d[1], ta)) return 0; }
    else if (!gb_slabs(g, x, h[0], h[1], kw->kind, k, 0, 0, 0, 0)) return 0;
    gb_take(g, x);
    return 1;
}
/* f_crater */
static int mk_crater(Gb *g, const void *kw_, double x, double y, double t, Got *got) {
    const Kw *kw = (const Kw *)kw_; double r[2], rr;
    pair(kw->r, 0.09, 0.14, r);
    rr = g->R * U(r[0], r[1]);
    got->g[0] = g_ptbuf(x, y, rr + 2.6, 24); got->d[0] = x; got->d[1] = y; got->d[2] = rr;
    return 1;
}
static int f_crater(Gb *g, const Kw *kw) {
    double u[2], rh[2], x, y, rr, a0; Got got; Geo *foot, *hole, *ring_; int n, k, i;
    pair(kw->u, 0.6, 0.95, u); pair(kw->rim_h, 0.6, 1.2, rh);
    if (!gb_try_place(g, mk_crater, kw, u[0], u[1], 0.8, 1, 0.55, &got)) return 0;
    foot = got.g[0]; x = got.d[0]; y = got.d[1]; rr = got.d[2]; gb_clear_under(g, foot);
    { double a = U(0, PI); hole = gb_blob(g, x, y, rr, rr * 0.85, a, 5, 0.2); }
    if (!hole) return 0;
    ring_ = g_diff(g_buf(hole, 2.2), g_buf(hole, 0.35));
    a0 = U(0, 2 * PI); n = RI(3, 4);
    for (k = 0; k < n; k++) {
        double a = a0 + k * 2 * PI / n + 0.12, b = a0 + (k + 1) * 2 * PI / n - 0.12, xy[20]; Geo *arc;
        xy[0] = x; xy[1] = y;
        for (i = 0; i < 9; i++) { xy[2 + 2 * i] = x + (rr + 5) * COS(a + (b - a) * i / 8); xy[3 + 2 * i] = y + (rr + 5) * SIN(a + (b - a) * i / 8); }
        arc = biggest(g_inter(ring_, g_poly(xy, 10)));
        if (arc && g_area(arc) > 1.0) { double hh = U(rh[0], rh[1]); gb_rise(g, arc, hh, kw->rim, -1, 1.3); }
    }
    gb_carve(g, hole, 0.37, kw->pit);
    if (kw->water) gb_carve(g, g_inset(hole, 0.8), 0.37, kw->water);
    if (kw->ejecta.has) gb_rocks_s(g, g_diff(foot, g_buf(hole, 0.3)), &kw->ejecta, NULL, 0);
    g->wet = uu_nonempty(L2(g->wet, hole)); g->wet_all = uu_nonempty(L2(g->wet_all, hole)); gb_take(g, foot);
    return 1;
}
/* f_trench */
static int mk_trench(Gb *g, const void *kw_, double x, double y, double t, Got *got) {
    const Kw *kw = (const Kw *)kw_; double w[2], L[2], a, Ln, ww, xy[4]; Geo *line, *ch;
    pair(kw->w, 1.6, 2.2, w); pair(kw->L, 0.5, 0.8, L);
    a = t + PI / 2 + U(-0.25, 0.25); Ln = g->R * U(L[0], L[1]); ww = U(w[0], w[1]);
    xy[0] = x - Ln / 2 * COS(a); xy[1] = y - Ln / 2 * SIN(a); xy[2] = x + Ln / 2 * COS(a); xy[3] = y + Ln / 2 * SIN(a);
    line = g_line(xy, 2); ch = g_bufx(line, ww / 2, 16, SHP_CAP_FLAT, SHP_JOIN_ROUND, 5.0);
    got->g[0] = g_bufx(ch, 1.4, 16, SHP_CAP_FLAT, SHP_JOIN_ROUND, 5.0); got->g[1] = line; got->g[2] = ch; got->d[0] = ww;
    return 1;
}
static int f_trench(Gb *g, const Kw *kw) {
    double u[2], ww; Got got; Geo *foot, *line, *ch; int sg;
    pair(kw->u, 0.6, 0.8, u);
    if (!gb_try_place(g, mk_trench, kw, u[0], u[1], 0.8, 1, 0.5, &got)) return 0;
    foot = got.g[0]; line = got.g[1]; ch = got.g[2]; ww = got.d[0]; gb_clear_under(g, foot);
    gb_carve(g, ch, 0.36, kw->bed);
    if (kw->wall) {
        const char *kd = gb_K(kw->wall);
        for (sg = -1; sg <= 1; sg += 2) {
            Geo *lip = g_bufx(one_line(line, sg * (ww / 2 + 0.55)), 0.45, 16, SHP_CAP_FLAT, SHP_JOIN_ROUND, 5.0);
            PR(g->s, L1(g_diff(lip, g_buf(g->free_, 0.3))), g->H_ + 0.45, kd);
        }
    }
    if (kw->pipe && !g->terr && !any_plate_hit(g, ch)) {
        double d = U(-0.2, 0.2) * ww; Geo *pp = g_inter(g_bufx(one_line(line, d), fz_min(0.5, ww * 0.25), 16, SHP_CAP_FLAT, SHP_JOIN_ROUND, 5.0), g_inset(ch, 0.25));
        if (!g_isempty(pp)) { char kp[300]; snprintf(kp, sizeof kp, "%s_pad", kw->pipe); { const char *c = col_get(kw->pipe); col_default(kp, c ? c : "#6f747c"); } S_prism(g->s, g_polys(pp), NF(g->H_ - 0.05), kp, NF(g->H_ - 0.8), SC.MINW, 0); }
    }
    gb_take(g, foot);
    return 1;
}
/* f_cracked_flat */
static int mk_cracked(Gb *g, const void *kw_, double x, double y, double t, Got *got) {
    const Kw *kw = (const Kw *)kw_; double r[2], rx;
    pair(kw->r, 0.14, 0.22, r);
    rx = g->R * U(r[0], r[1]);
    got->g[0] = gb_blob(g, x, y, rx * 1.3, rx, t + PI / 2, 6, 0.2);
    return 1;
}
static int f_cracked_flat(Gb *g, const Kw *kw) {
    double u[2]; int n[2]; Got got;
    pair(kw->u, 0.5, 0.9, u); ipair(kw->n, 10, 16, n);
    if (!gb_try_place(g, mk_cracked, kw, u[0], u[1], 0.3, 0, 0.55, &got) || !got.g[0]) return 0;
    { int k = RI(n[0], n[1]); gb_crack_patch(g, got.g[0], k, 0.3, kw->kind ? kw->kind : "crack", 0.2); }
    gb_take(g, got.g[0]);
    return 1;
}
/* Ground.field(kind, h=(0.3, 1.4), r=(0.19, 0.3), cover=0.7, n_max=22, rocks=None, elong=(1.2, 2.4)) */
static int gb_field(Gb *g, const char *kind, double h0, double h1, double cover, const Rocks *rocks) {
    Geo *ring = g_diff(g->disc, g_buf(g->free_, 0.6)), *done, *block; double want, cx[64], cy[64]; int nc = 0, made = 0, it, i; GeoList *bl = LL();
    if (g_isempty(ring)) return 0;
    want = cover * g_area(ring); done = g_empty();
    for (i = 0; i < g->mine->n; i++) LP(bl, g_buf(g->mine->v[i], 0.3));
    if (!g_isempty(g->wet)) LP(bl, g_buf(g->wet, 0.7));
    if (!g_isempty(g->floors)) LP(bl, g->floors);
    block = uu_nonempty(bl);
    for (it = 0; it < 22; it++) {
        int has = 0, j; double bd = 0, bx = 0, by = 0, bt = 0, rx, el, a, uu, hh; Geo *x;
        for (j = 0; j < 14; j++) {
            double t = U(0, 2 * PI), d = fz_max(g->rk + 1.5, g->R * U(0.5, 0.88)), x0 = d * COS(t), y0 = d * SIN(t), dm = 1e9;
            if (g_contains(done, g_pt(x0, y0)) && RND() < 0.8) continue;
            for (i = 0; i < nc; i++) { double q = py_hypot(x0 - cx[i], y0 - cy[i]); if (i == 0 || q < dm) dm = q; }
            if (nc) dm = fz_min(dm, 1e9);          /* min([...] + [1e9]) */
            if (!has || dm > bd) { has = 1; bd = dm; bx = x0; by = y0; bt = t; }
        }
        if (!has) continue;
        rx = g->R * U(0.19, 0.3); el = ly_pow(U(1.2, 2.4), 0.5); a = bt + PI / 2 + U(-0.5, 0.5);
        x = gb_blob(g, bx, by, rx * el, rx / el, a, 7, 0.3);
        if (!x) continue;
        x = g_diff(x, g_buf(g->free_, 0.7));
        if (!g_isempty(block)) x = g_diff(x, block);
        x = !g_isempty(x) ? biggest(g_bufq(g_bufq(x, -0.4, 8), 0.4, 8)) : NULL;
        if (!x || g_area(x) < 6 || !g_wide(x, 2.0)) continue;
        if (nc >= 64) ly_raise();
        cx[nc] = bx; cy[nc] = by; nc++;
        uu = fz_min(1.0, fz_max(0.0, (py_hypot(bx, by) / g->R - 0.45) / 0.55));
        hh = (h0 + (h1 - h0) * uu) * U(0.75, 1.15);
        gb_clear_under(g, x);
        if (gb_rise(g, x, hh, kind, -1, 1.6)) {
            made++; done = uu_nonempty(L2(done, x));
            if (rocks && rocks->has && RND() < 0.4) gb_rocks_s(g, g_inset(x, 0.6), rocks, NULL, 0);
        }
        if (g_area(done) >= want) break;
    }
    g->made += made;
    return made;
}

/* ================================================================ the styles (ground_big.STYLE + each style file's ground_big) */
enum { F_RISE, F_RIDGE, F_MESA, F_OUTCROP, F_GULLY, F_POOL, F_RUBBLE, F_STRANDS, F_SLABS, F_CRATER, F_TRENCH, F_CRACKED };
typedef struct { int f, n; Kw kw; } Feat;
typedef struct { int has_field; const char *fkind; double fh0, fh1, fcover; Rocks rocks; Feat feats[8]; int nf; int has_floored; Feat ffeats[4]; int nff; } Style;
#define RK(n, a, b, c, d, k) { 1, n, a, b, c, d, k }
#define SCREE(k) RK(5, 0.5, 1.0, 0.3, 0.7, k)
#define PEBBLES RK(6, 0.35, 0.6, 0.2, 0.35, "pebble")
#define E_ "gb_rise"
#define B_ "gb_bank"
#define P_ "gb_heap"
static const Style STY[] = {
    /* 0 badlands */ { 1, E_, 0.3, 1.9, 0.65, RK(2, 0.6, 1.1, 0.4, 0.8, "sandstone"), { { F_MESA, 2, { .kind = "sandstone", .scree = SCREE("sandstone") } }, { F_GULLY, 2, { .bed = "wash", .bank = B_, .pebbles = PEBBLES } },
                       { F_CRACKED, 2, { .kind = "mudcrack" } }, { F_OUTCROP, 1, { .kind = "sandstone", .soft = B_ } } }, 4 },
    /* 1 highland */ { 1, "moss", 0.4, 2.0, 0.7, RK(3, 0.5, 0.9, 0.3, 0.6, "slate"), { { F_MESA, 1, { .kind = "slate", .steps = { 3, 4 }, .h = { 1.6, 3.0 }, .scree = SCREE("slate") } }, { F_OUTCROP, 2, { .kind = "slate", .soft = "moss" } },
                       { F_POOL, 1, { .kind = "water", .rim = "moss" } }, { F_GULLY, 1, { .bed = "water", .bank = B_, .pebbles = PEBBLES } } }, 4 },
    /* 2 maahl */ { 1, E_, 0.3, 2.0, 0.6, { 0 }, { { F_RIDGE, 3, { .kind = E_, .h = { 0.8, 2.2 }, .w = { 3.0, 5.0 }, .L = { 0.4, 0.7 } } }, { F_MESA, 1, { .kind = "ruin", .steps = { 1, 2 }, .h = { 1.2, 2.2 } } },
                       { F_CRACKED, 1, { .kind = "mudcrack" } } }, 3 },
    /* 3 corridor */ { 0, NULL, 0, 0, 0, { 0 }, { { F_SLABS, 4, { .kind = "metal", .h = { 0.4, 1.6 }, .r = { 0.15, 0.25 }, .n = { 6, 10 } } }, { F_TRENCH, 2, { .bed = "slot", .wall = "metal", .pipe = "pipe" } },
                       { F_RUBBLE, 2, { .soft = P_, .stone = "metal", .h = { 0.5, 1.2 }, .big = { 0.6, 1.2 } } }, { F_RIDGE, 3, { .kind = "metal", .h = { 1.0, 1.8 }, .w = { 2.6, 3.6 }, .has_e = 1, .e = 1.0 } },
                       { F_CRATER, 1, { .rim = "metal", .pit = "slot", .rim_h = { 0.5, 1.0 }, .ejecta = RK(8, 0.5, 0.9, 0.3, 0.6, "metal") } } }, 5 },
    /* 4 infestation */ { 1, "flesh", 0.4, 1.8, 0.6, RK(5, 0.45, 0.8, 0.4, 0.8, "pustule"), { { F_STRANDS, 2, { .kind = "tendril", .hub = "flesh" } }, { F_POOL, 2, { .kind = "acid", .rim = "flesh" } },
                       { F_SLABS, 1, { .kind = "metal", .h = { 0.3, 1.1 } } } }, 3 },
    /* 5 urban */ { 1, P_, 0.2, 0.9, 0.45, { 0 }, { { F_RUBBLE, 3, { .soft = P_, .stone = "concrete" } }, { F_SLABS, 2, { .kind = "concrete", .h = { 0.3, 1.2 } } },
                       { F_CRATER, 1, { .rim = B_, .pit = "pothole", .ejecta = RK(8, 0.5, 1.0, 0.3, 0.7, "rubble") } }, { F_POOL, 2, { .kind = "water", .r = { 0.05, 0.09 } } } }, 4 },
    /* 6 mine */ { 1, E_, 0.3, 1.6, 0.55, RK(3, 0.5, 1.0, 0.4, 0.8, "rock"), { { F_RUBBLE, 2, { .soft = P_, .stone = "rock" } }, { F_MESA, 2, { .kind = "rock", .steps = { 2, 3 }, .h = { 1.6, 3.4 }, .scree = SCREE("rock") } },
                       { F_POOL, 2, { .kind = "water", .r = { 0.05, 0.1 } } } }, 3 },
    /* 7 swamp */ { 1, "mud", 0.25, 1.0, 0.6, RK(2, 0.5, 0.9, 0.3, 0.6, "rock"), { { F_POOL, 3, { .kind = "water", .rim = "mud", .ripples = 3, .has_ripples = 1 } }, { F_STRANDS, 2, { .kind = "root", .hub = "stump" } },
                       { F_RISE, 4, { .kind = "moss", .h = { 0.5, 1.2 }, .r = { 0.07, 0.13 } } } }, 3 },
    /* 8 cynder */ { 1, "ash", 0.3, 1.8, 0.55, { 0 }, { { F_MESA, 2, { .kind = "basalt", .steps = { 2, 3 }, .h = { 1.6, 3.6 }, .scree = SCREE("basalt") } },
                       { F_GULLY, 2, { .bed = "lava", .bank = "ash", .bank_h = { 0.4, 0.9 } } }, { F_OUTCROP, 1, { .kind = "obsidian", .h = { 1.4, 3.0 } } } }, 3 },
    /* 9 targa */ { 1, "snow", 0.3, 1.9, 0.6, { 0 }, { { F_RIDGE, 2, { .kind = "snow", .h = { 0.6, 1.8 }, .w = { 2.6, 4.2 } } }, { F_OUTCROP, 2, { .kind = "ice", .h = { 1.6, 3.6 }, .soft = "snow" } },
                       { F_SLABS, 2, { .kind = "alien", .h = { 0.3, 1.3 } } }, { F_GULLY, 1, { .bed = "slot", .bank = "snow", .w = { 0.9, 1.3 } } } }, 4 },
    /* 10 trederra */ { 1, "mud", 0.3, 1.3, 0.6, { 0 }, { { F_CRATER, 3, { .rim = "mud", .pit = "pit", .water = "water", .ejecta = RK(6, 0.4, 0.8, 0.3, 0.6, "rock") } },
                       { F_POOL, 2, { .kind = "water", .rim = "mud", .ripples = 2, .has_ripples = 1 } } }, 2 },
    /* 11 fortress */ { 1, "moss", 0.25, 1.0, 0.5, { 0 }, { { F_POOL, 1, { .kind = "water", .r = { 0.1, 0.15 }, .ripples = 3, .has_ripples = 1, .rim = "moss" } },
                       { F_OUTCROP, 2, { .kind = "garden", .h = { 1.0, 2.6 }, .n = { 2, 4 }, .soft = "moss" } }, { F_RIDGE, 1, { .kind = B_, .h = { 0.4, 0.9 } } } }, 3,
                     1, { { F_SLABS, 3, { .kind = "plank", .h = { 0.2, 0.9 }, .tilt_off = 1 } }, { F_RUBBLE, 2, { .soft = P_, .stone = "garden" } }, { F_OUTCROP, 1, { .kind = "garden", .h = { 1.0, 2.4 }, .n = { 2, 4 } } } }, 3 },
    /* 12 beast */ { 1, "flesh", 0.4, 2.0, 0.7, { 0 }, { { F_RIDGE, 3, { .kind = "flesh", .h = { 0.6, 1.6 }, .w = { 2.0, 3.5 } } }, { F_POOL, 2, { .kind = "bile", .rim = "flesh" } },
                       { F_STRANDS, 2, { .kind = "vein", .hub = "flesh" } }, { F_OUTCROP, 1, { .kind = "tooth", .h = { 1.4, 3.0 }, .n = { 3, 5 } } } }, 4 },
    /* 13 frontier */ { 1, E_, 0.25, 1.3, 0.55, RK(2, 0.5, 0.9, 0.3, 0.6, "rock"), { { F_GULLY, 1, { .bed = "rut", .bank = B_, .pebbles = PEBBLES } }, { F_POOL, 2, { .kind = "water", .r = { 0.05, 0.09 } } },
                       { F_OUTCROP, 1, { .kind = "rock", .soft = B_ } } }, 3,
                     1, { { F_SLABS, 2, { .kind = "board", .h = { 0.2, 0.8 }, .tilt_off = 1 } }, { F_RUBBLE, 1, { .soft = P_, .stone = "wood" } }, { F_RISE, 3, { .kind = E_, .h = { 0.3, 0.8 } } } }, 3 },
    /* 14 graveyard */ { 1, "earth", 0.25, 1.0, 0.5, { 0 }, { { F_RIDGE, 4, { .kind = "earth", .h = { 0.4, 0.8 }, .w = { 2.2, 3.0 }, .L = { 0.14, 0.2 }, .tang_off = 1 } },
                       { F_STRANDS, 1, { .kind = "root", .hub = "stump" } }, { F_POOL, 1, { .kind = "water", .r = { 0.05, 0.08 } } } }, 3 },
    /* 15 forest */ { 1, "moss", 0.35, 1.8, 0.65, RK(2, 0.6, 1.0, 0.4, 0.7, "rock"), { { F_STRANDS, 2, { .kind = "root", .hub = "stump", .n = { 4, 7 } } }, { F_RIDGE, 2, { .kind = "leaf", .h = { 0.3, 0.7 } } },
                       { F_POOL, 1, { .kind = "water" } }, { F_OUTCROP, 1, { .kind = "rock", .soft = "moss" } } }, 4 },
    /* 16 dungeon */ { 0, NULL, 0, 0, 0, { 0 }, { { F_SLABS, 4, { .kind = "flag", .h = { 0.3, 1.2 }, .r = { 0.15, 0.24 }, .n = { 6, 10 }, .tilt_off = 1 } }, { F_RUBBLE, 2, { .soft = P_, .stone = "flag" } },
                       { F_POOL, 2, { .kind = "water", .r = { 0.05, 0.1 }, .ripples = 1, .has_ripples = 1 } }, { F_CRACKED, 1, { 0 } } }, 4 },
    /* 17 temple */ { 1, E_, 0.3, 1.5, 0.5, { 0 }, { { F_RIDGE, 2, { .kind = E_, .h = { 0.5, 1.4 }, .w = { 2.6, 4.2 } } }, { F_RUBBLE, 2, { .soft = P_, .stone = "tcolumn", .big = { 1.0, 1.8 } } },
                       { F_SLABS, 2, { .kind = "tflag", .h = { 0.3, 1.1 } } }, { F_POOL, 1, { .kind = "water", .r = { 0.06, 0.1 } } } }, 4 },
    /* 18 coast */ { 1, E_, 0.2, 1.2, 0.5, { 0 }, { { F_MESA, 2, { .kind = "shelf", .steps = { 2, 4 }, .h = { 1.4, 3.0 } } }, { F_POOL, 3, { .kind = "water", .ripples = 2, .has_ripples = 1, .r = { 0.07, 0.12 } } },
                       { F_RIDGE, 2, { .kind = E_, .h = { 0.3, 0.8 }, .w = { 2.0, 3.0 } } }, { F_OUTCROP, 1, { .kind = "shelf" } } }, 4 },
    /* 19 ashveil (style_ashveil.ground_big) */ { 1, "ashdrift", 0.3, 1.6, 0.45, RK(2, 0.5, 1.0, 0.3, 0.7, "ashcinder"), { { F_RIDGE, 2, { .kind = "ashdrift", .h = { 0.6, 1.8 }, .w = { 2.6, 4.2 } } },
                       { F_CRACKED, 2, { .kind = "crack" } }, { F_CRATER, 1, { .rim = B_, .pit = "pit", .ejecta = RK(8, 0.5, 1.0, 0.3, 0.7, "ashcinder") } },
                       { F_SLABS, 1, { .kind = "ashhull", .h = { 0.3, 1.2 } } }, { F_OUTCROP, 1, { .kind = "ashcinder", .h = { 1.2, 2.6 }, .soft = "ashdrift" } } }, 5 },
    /* 20 blight */ { 1, "fleshrock", 0.3, 1.2, 0.3, RK(3, 0.5, 0.9, 0.3, 0.6, "blightrock"), { { F_CRATER, 2, { .rim = "blightrock", .pit = "rift", .rim_h = { 0.5, 1.0 }, .ejecta = RK(6, 0.4, 0.8, 0.3, 0.6, "thorn") } },
                       { F_POOL, 2, { .kind = "ichor", .rim = "blister", .ripples = 2, .has_ripples = 1 } }, { F_RUBBLE, 1, { .soft = P_, .stone = "blightrock" } }, { F_CRACKED, 1, { .kind = "rift" } } }, 4 },
    /* 21 ferrum */ { 1, "dust", 0.3, 1.5, 0.55, RK(3, 0.5, 1.0, 0.4, 0.8, "slag"), { { F_RUBBLE, 2, { .soft = P_, .stone = "slag", .h = { 0.5, 1.3 }, .big = { 0.6, 1.3 } } },
                       { F_SLABS, 3, { .kind = "plate", .h = { 0.3, 1.3 }, .r = { 0.15, 0.25 }, .n = { 5, 9 } } }, { F_POOL, 2, { .kind = "tar", .rim = "dust", .r = { 0.05, 0.09 } } },
                       { F_TRENCH, 1, { .bed = "slot", .wall = "girder", .pipe = "pipe" } }, { F_OUTCROP, 1, { .kind = "slag", .h = { 1.2, 2.6 }, .soft = "dust" } },
                       { F_RIDGE, 2, { .kind = "dust", .h = { 0.5, 1.4 }, .w = { 2.4, 3.8 } } } }, 6,
                     1, { { F_SLABS, 3, { .kind = "plate", .h = { 0.2, 0.9 }, .tilt_off = 1 } }, { F_RUBBLE, 2, { .soft = P_, .stone = "slag" } }, { F_TRENCH, 1, { .bed = "slot", .wall = "girder", .pipe = "pipe" } } }, 3 },
    /* 22 obelis */ { 1, "sand", 0.3, 1.4, 0.5, { 0 }, { { F_SLABS, 3, { .kind = "obfloor", .h = { 0.3, 1.1 } } }, { F_SLABS, 2, { .kind = "monolith", .h = { 0.6, 1.5 }, .tilt_off = 1 } },
                       { F_RIDGE, 2, { .kind = "sand", .h = { 0.5, 1.4 }, .w = { 2.6, 4.2 } } } }, 3 },
    /* 23 sundered */ { 1, P_, 0.2, 1.0, 0.45, RK(4, 0.5, 1.0, 0.3, 0.7, "rubble"), { { F_RUBBLE, 3, { .soft = P_, .stone = "ashlar", .big = { 1.0, 1.8 } } }, { F_SLABS, 2, { .kind = "ashlar", .h = { 0.3, 1.2 } } },
                       { F_CRATER, 2, { .rim = B_, .pit = "pothole", .ejecta = RK(8, 0.5, 1.0, 0.3, 0.7, "rubble") } }, { F_RIDGE, 1, { .kind = P_, .h = { 0.5, 1.2 }, .w = { 2.4, 3.6 } } } }, 4,
                     1, { { F_SLABS, 3, { .kind = "flag", .h = { 0.2, 0.8 }, .tilt_off = 1 } }, { F_RUBBLE, 2, { .soft = P_, .stone = "ashlar" } } }, 2 },
    /* 24 thornwild */ { 1, "moss", 0.35, 1.8, 0.65, RK(2, 0.6, 1.0, 0.4, 0.7, "rock"), { { F_STRANDS, 3, { .kind = "root", .hub = "stump", .n = { 4, 7 } } }, { F_POOL, 1, { .kind = "bile", .rim = "moss", .ripples = 2, .has_ripples = 1 } },
                       { F_RISE, 3, { .kind = "moss", .h = { 0.5, 1.2 }, .r = { 0.07, 0.13 } } }, { F_OUTCROP, 1, { .kind = "rock", .soft = "moss" } } }, 4 },
    /* 25 default */ { 1, E_, 0.3, 1.2, 0.5, { 0 }, { { F_OUTCROP, 1, { .kind = "rock" } } }, 1 },
};
static const char *STY_NAMES[] = { "badlands", "highland", "maahl", "corridor", "infestation", "urban", "mine", "swamp", "cynder", "targa", "trederra", "fortress",
    "beast", "frontier", "graveyard", "forest", "dungeon", "temple", "coast", "ashveil", "blight", "ferrum", "obelis", "sundered", "thornwild" };
static const Style *find_sty(const char *st) { int i; for (i = 0; i < (int)(sizeof STY_NAMES / sizeof *STY_NAMES); i++) if (!strcmp(STY_NAMES[i], st)) return &STY[i]; return &STY[25]; }
static int is_tall(int f) { return f == F_MESA || f == F_OUTCROP || f == F_STRANDS; }
static int run_feat(Gb *g, const Feat *ft) {
    switch (ft->f) {
    case F_RISE: return f_rise(g, &ft->kw); case F_RIDGE: return f_ridge(g, &ft->kw); case F_MESA: return f_mesa(g, &ft->kw);
    case F_OUTCROP: return f_outcrop(g, &ft->kw); case F_GULLY: return f_gully(g, &ft->kw); case F_POOL: return f_pool(g, &ft->kw);
    case F_RUBBLE: return f_rubble(g, &ft->kw); case F_STRANDS: return f_strands(g, &ft->kw); case F_SLABS: return f_slabs(g, &ft->kw);
    case F_CRATER: return f_crater(g, &ft->kw); case F_TRENCH: return f_trench(g, &ft->kw); default: return f_cracked_flat(g, &ft->kw);
    }
}

/* _shade(hexcol, f): a colour a little lighter (f > 1) or darker (f < 1) */
static void gb_shade(const char *hexcol, double f, char out[16]) {
    const char *h = hexcol; int v[3], i;
    while (*h == '#') h++;
    for (i = 0; i < 3; i++) {
        char t[3]; char *e; size_t L = strlen(h);
        if ((size_t)(2 * i) >= L) { snprintf(out, 16, "#888888"); return; }          /* int("", 16): ValueError */
        t[0] = h[2 * i]; t[1] = (size_t)(2 * i + 1) < L ? h[2 * i + 1] : 0; t[2] = 0;
        v[i] = (int)strtol(t, &e, 16);
        if (*e || !t[0]) { snprintf(out, 16, "#888888"); return; }
    }
    for (i = 0; i < 3; i++) { double x = v[i] * f; int k = (int)x; v[i] = k < 0 ? 0 : k > 255 ? 255 : k; }          /* max(0, min(255, int(v * f))) */
    snprintf(out, 16, "#%02x%02x%02x", v[0], v[1], v[2]);
}
/* Ground.__init__(sc) */
static void gb_init(Gb *g, LyGbSc *sc) {
    Scene *s = sc->s; double fin = sc->final_mm, ts = sc->true_scale, big; char seed[1200]; GeoList *keep = LL(), *fl = LL(), *wet = LL(), *wsm = LL(); int i, j;
    memset(g, 0, sizeof *g); g->sc = sc; g->s = s;
    g->R = SC.R0;
    if (fin > 0 && ts > 0) g->R = fz_min(g->R, fin / (2 * ts));
    g->HS = (fin >= 89.9 || fin <= 0) ? 1.0 : fz_max(0.4, ly_pow(fin / 90.0, 1.3));
    g->H_ = SC.H; g->SINK = SC.SINK; g->LAP = SC.LAP;
    g->terr = pl_terr(s);
    snprintf(seed, sizeof seed, "ground_big%s%s%s%s", sc->style, s->name, sc->salt ? sc->salt : "", sc->reseed ? sc->reseed : "");
    py_seed_str(&g->rng, seed);
    g->A = fz_max(0.3, ly_pow(g->R / 26.67, 2.0));
    gl_extend(keep, &s->keepout);
    {
        GeoList *f = LL(); gl_extend(f, keep);
        for (i = 0; i < sc->stand_pads.n; i++) if (sc->stand_pads.v[i] && !g_isempty(sc->stand_pads.v[i])) LP(f, g_buf(sc->stand_pads.v[i], 1.6));
        if (sc->stand_body && !g_isempty(sc->stand_body)) LP(f, g_buf(sc->stand_body, 0.4));
        g->free_ = uu_nonempty(f);
    }
    g->body = (sc->stand_body && !g_isempty(sc->stand_body)) ? g_buf(sc->stand_body, 2.5) : g_empty();
    g->rk = 0.0;
    { double best = 0; int has = 0; for (i = 0; i < keep->n; i++) { double v = sqrt(g_area(keep->v[i]) / PI); if (!has || v > best) { best = v; has = 1; } } g->rk = has ? fz_max(best, 0.0) : 0.0; }
    for (i = 0; i < s->gcuts.n; i++) {
        Gcut *c = &s->gcuts.v[i];
        if (!(in_set(&LY_WETK, c->kind) || kind_is(c->kind, "pit") || kind_is(c->kind, "hollow") || kind_is(c->kind, "pothole"))) continue;
        for (j = 0; j < c->polys.n; j++) if (g_area(c->polys.v[j]) > 2.0) LP(wet, c->polys.v[j]);
    }
    g->wet_all = uu_nonempty(wet); big = 0.2 * PI * g->R * g->R;
    for (i = 0; i < wet->n; i++) if (g_area(wet->v[i]) < big) LP(wsm, wet->v[i]);
    g->wet = uu_nonempty(wsm);
    {
        jmp_buf jb, *prev = ly_jb; ly_jb = &jb;          /* try: self.plates = [(float(z), u) ...] except Exception: [] */
        if (setjmp(jb) == 0) { g->nplates = sc_plates(s, &g->plates); for (i = 0; i < g->nplates; i++) g->plates[i].z = NF(g->plates[i].z.v); }
        else { g->nplates = 0; g->plates = NULL; }
        ly_jb = prev;
    }
    g->mine = LL(); g->disc = g_ptbuf(0, 0, g->R - 0.6, 96);
    for (i = 0; i < g->nplates; i++) LP(fl, g_bufq(g->plates[i].u, 0.5, 8));
    g->floors = uu_nonempty(fl);
}
/* ground_big._floored(g) */
static int gb_floored(Gb *g) {
    int r = 0; jmp_buf jb, *prev = ly_jb; ly_jb = &jb;
    if (setjmp(jb) == 0) {
        Plate *pl; int n = sc_plates(g->s, &pl), i; GeoList *l = LL(); Geo *u, *ring;
        for (i = 0; i < n; i++) LP(l, pl[i].u);
        u = uu_nonempty(l); ring = g_diff(g->disc, g->free_);
        r = !g_isempty(u) && g_area(g_inter(u, ring)) > 0.5 * g_area(ring);
    } else r = 0;
    ly_jb = prev;
    return r;
}

static int s_nops(Scene *s) { return s->ops.n; }
/* ground_big.apply(sc) */
int ly_ground_big(LyGbSc *sc) {
    jmp_buf jb, *prev = ly_jb; volatile int made = 0, failed = 0;
    if (sc->final_mm < 49.9 || sc->true_scale == 1.0) return 0;
    if (!LY_TERRK.n) ly_sets_default();
    ly_arena_begin();
    ly_jb = &jb;
    if (setjmp(jb) == 0) {
        Gb gg_, *g = &gg_; const Style *st = find_sty(sc->style); const Feat *feats; int nf, i, k, nt = 0, ntd = 0; Feat *todo; int64_t *ord; Feat *tall, *rest; int nr = 0;
        int field = st->has_field;
        gb_init(g, sc);
        {   /* _kinds(gg, scenes, ground): gg.COL[name] = _shade(ground, f), scenes.TERRK.add(name); then COL.setdefault of ash, pipe, slot */
            static const char *const NM[3] = { "gb_rise", "gb_bank", "gb_heap" }; static const double F[3] = { 1.12, 0.92, 1.05 };
            const char *gr = sc->s->ground && sc->s->ground[0] ? sc->s->ground : "#777777";
            for (i = 0; i < 3; i++) { char c[16]; gb_shade(gr, F[i], c); col_set(NM[i], c); lyset_add(&LY_TERRK, NM[i]); }
            col_default("ash", "#5c5552"); col_default("pipe", "#6f747c"); col_default("slot", "#0d1014");
        }
        feats = st->feats; nf = st->nf;
        if (st->has_floored && gb_floored(g)) { feats = st->ffeats; nf = st->nff; field = 0; g->on_floors = 1; }
        todo = (Feat *)LM(sizeof(Feat) * 64);
        for (i = 0; i < nf; i++) {
            double m = feats[i].n * g->A; int64_t mi = ly_int(m), c; double r = RND();
            mi = mi + (r < m - (double)ly_int(m) ? 1 : 0);
            c = feats[i].n >= 2 ? 1 : 0; if (mi > c) c = mi;
            for (k = 0; k < c && ntd < 64; k++) todo[ntd++] = feats[i];
        }
        tall = (Feat *)LM(sizeof(Feat) * 64); rest = (Feat *)LM(sizeof(Feat) * 64);
        for (i = 0; i < ntd; i++) { if (is_tall(todo[i].f)) tall[nt++] = todo[i]; else rest[nr++] = todo[i]; }
        ord = (int64_t *)LM(sizeof(int64_t) * (size_t)(nr + 1)); for (i = 0; i < nr; i++) ord[i] = i;
        py_shuffle_idx(&g->rng, ord, nr);
        {
            static const char *FN[] = { "rise", "ridge", "mesa", "outcrop", "gully", "pool", "rubble", "strands", "slabs", "crater", "trench", "cracked_flat" };
            int dbg = getenv("GB_DEBUG") != NULL, ok;
            for (i = 0; i < nt; i++) { ok = -1; TRY(ok = run_feat(g, &tall[i])); if (dbg) fprintf(stderr, "  ground_big %s %d (C, ops %d)\n", FN[tall[i].f], ok, s_nops(sc->s)); }
            if (field) { TRY(gb_field(g, st->fkind, st->fh0, st->fh1, st->fcover * fz_min(1.0, 0.75 + 0.25 * g->A), &st->rocks)); if (dbg) fprintf(stderr, "  field ops=%d\n", s_nops(sc->s)); }
            for (i = 0; i < nr; i++) { ok = -1; TRY(ok = run_feat(g, &rest[ord[i]])); if (dbg) fprintf(stderr, "  ground_big %s %d (C, ops %d)\n", FN[rest[ord[i]].f], ok, s_nops(sc->s)); }
        }
        made = g->made;
        free(g->rises);
    } else failed = 1;
    ly_jb = prev;
    ly_arena_end();
    return failed ? SC_ERR : made;
}
