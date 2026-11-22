/* place.c: gg3.py's placement helpers in C (see place.h). Each function names the gg3 routine it copies; the
   geometry is cport\shp.c (shapely 2.2.0's own GEOS calls), the scene writers cport\scene.c, the leaves cport\leaf.c,
   the rock clusters cport\scatter.c. gg3.py is not rewritten by sizefix, so this is the source as written. */
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "shp.h"
#include "pyx.h"
#include "pyexact.h"
#include "leaf.h"
#include "scene.h"
#include "scatter.h"
#include "place.h"

#define TRY(x) do { if ((x) < 0) return SC_ERR; } while (0)
static Num NF(double v) { return num_f(v); }

PlCfg PL = { 1.0, 0, 1, NULL, { 0 }, { 0 }, 0.97, 0.3, 0.9 };

int pl_config(double hboost, int notch, int crocks, const double *csx, const double *csy) {
    PL.hboost = hboost; PL.notch = notch; PL.crocks = crocks;
    if (csx) memcpy(PL.csx, csx, sizeof PL.csx);
    if (csy) memcpy(PL.csy, csy, sizeof PL.csy);
    geo_unref(PL.disc);
    PL.disc = geo_new(shp_point_buffer(0, 0, SC.R0 - 0.7, 128));          /* DISC = Point(0, 0).buffer(R0 - 0.7, 128) */
    return PL.disc != NULL;
}

/* SOFT = {"grit"}; BRIDGE = {...}: membership as Python tests it (an LK equals a str by its text) */
static int is_soft(const Kind *k) { return kind_is(k, "grit"); }
static const char *BRIDGE[] = { "plate", "tflag", "basalt", "alien", "ice", "duck", "plank", "tatami", "flag", "fleshfloor", "board", "shelf",
                                "obfloor" };          /* + the floors the newer styles add at load time (style_obelis FLOORS); one shared kind table comes with the compose port */
static int is_bridge(const Kind *k) { size_t i; for (i = 0; i < sizeof BRIDGE / sizeof *BRIDGE; i++) if (kind_is(k, BRIDGE[i])) return 1; return 0; }
static int allowed(const PlAllow *a, const Kind *k) { int i; if (!a) return 0; for (i = 0; i < a->n; i++) if (kind_is(k, a->v[i])) return 1; return 0; }
static int is_bridge_z(Num z) { return z.t == NUM_Z && z.bridge; }          /* getattr(z, "bridge", False) */

int pl_terr(const Scene *s) { return s->has_terr; }
int (*pl_terrk_fn)(const Kind *k);          /* the raised ground kinds (scenes.TERRK: bedrock, dune), set by pipeline.c */

/* ---------------- small list helpers ---------------- */
static void grow(void **v, int *cap, int n, size_t sz) { if (n >= *cap) { *cap = *cap ? *cap * 2 : 8; *v = realloc(*v, sz * (size_t)*cap); } }
static const GEOSGeometry **raw(const GeoList *l) {          /* the GEOS pointers of a list (NULL for None entries) */
    const GEOSGeometry **v = (const GEOSGeometry **)malloc(sizeof *v * (size_t)(l->n ? l->n : 1)); int i;
    for (i = 0; i < l->n; i++) v[i] = l->v[i] ? l->v[i]->g : NULL;
    return v;
}
static GEOSGeometry *union_list(const GeoList *l) { const GEOSGeometry **v = raw(l); GEOSGeometry *u = shp_union_all(v, l->n); free(v); return u; }
static void tcl_free(TcutList *l) { int i; for (i = 0; i < l->n; i++) { gl_free(&l->v[i].polys); kind_unref(l->v[i].kind); } free(l->v); l->v = NULL; l->n = l->cap = 0; }
static void gcl_free(GcutList *l) { int i; for (i = 0; i < l->n; i++) { gl_free(&l->v[i].polys); kind_unref(l->v[i].kind); } free(l->v); l->v = NULL; l->n = l->cap = 0; }
/* (op[0], ps) + tuple(op[2:]): a new op with other pieces */
static Op *op_with(const Op *o, const GeoList *ps) { return op_like(o, ps); }

/* a set of ids: sorted pointers */
typedef struct { const void **v; int n, cap; } PSet;
static void ps_add(PSet *s, const void *p) { grow((void **)&s->v, &s->cap, s->n, sizeof(void *)); s->v[s->n++] = p; }
static int pcmp(const void *a, const void *b) { const char *x = *(const char *const *)a, *y = *(const char *const *)b; return x < y ? -1 : x > y; }
static void ps_done(PSet *s) { if (s->n) qsort(s->v, (size_t)s->n, sizeof(void *), pcmp); }
static int ps_has(const PSet *s, const void *p) { return s->n && bsearch(&p, s->v, (size_t)s->n, sizeof(void *), pcmp) != NULL; }
static void ps_free(PSet *s) { free(s->v); s->v = NULL; s->n = s->cap = 0; }

/* ---------------- _Hits ---------------- */
/* _Hits.__init__: self.polys = polys; self.tree = STRtree(polys) if polys else None */
int pl_hits_init(PlHits *h, const GeoList *polys) {
    memset(h, 0, sizeof *h); gl_copy(&h->polys, polys);
    if (!polys->n) return 0;
    h->gs = raw(&h->polys);
    h->tree = shp_tree_new(h->gs, h->polys.n, 10);
    return h->tree ? 0 : SC_ERR;
}
void pl_hits_free(PlHits *h) { shp_tree_free(h->tree); free(h->gs); gl_free(&h->polys); memset(h, 0, sizeof *h); }
/* len(self.tree.query(g, predicate="intersects")) > 0 */
int pl_hits_hit(PlHits *h, const GEOSGeometry *g) {
    ShpIdx x = { 0 }; int r;
    if (!h->tree) return 0;
    r = shp_tree_query(h->tree, g, SHP_PRED_INTERSECTS, &x);
    shp_idx_free(&x);
    return r < 0 ? SC_ERR : r > 0;
}
/* len(self.tree.query(pt, predicate="dwithin", distance=d)) > 0 */
int pl_hits_near(PlHits *h, const GEOSGeometry *pt, double d) {
    ShpIdx x = { 0 }; int r;
    if (!h->tree) return 0;
    r = shp_tree_query_dwithin(h->tree, pt, d, &x);
    shp_idx_free(&x);
    return r < 0 ? SC_ERR : r > 0;
}
/* _soft_hits(s): _Hits([p for op in s.ops if op[-1] in SOFT for p in op[1]]) */
int pl_soft_hits(const Scene *s, PlHits *h) {
    GeoList ps = { 0 }; int i, r;
    for (i = 0; i < s->ops.n; i++) if (is_soft(s->ops.v[i]->kind)) gl_extend(&ps, &s->ops.v[i]->polys);
    r = pl_hits_init(h, &ps); gl_free(&ps);
    return r;
}
/* _soft_cut(kind, p): kind in ("crack", "joint") or (kind == "pit" and p.area < 2.5) */
int pl_soft_cut(const Kind *k, const Geo *p) { return kind_is(k, "crack") || kind_is(k, "joint") || (kind_is(k, "pit") && shp_area(p->g) < 2.5); }

/* ---------------- hard ---------------- */
int pl_hard(Scene *s, const PlAllow *allow, GeoList *out) {
    GeoList both = { 0 }; const GeoList *base; int i, j;
    if (pl_terr(s)) { gl_extend(&both, &s->objs); gl_extend(&both, &s->keepout); base = &both; }          /* (s.objs + s.keepout) if terr(s) else s.feet */
    else base = &s->feet;
    if (base->n) {
        char *small = (char *)calloc((size_t)base->n, 1), *drop = (char *)calloc((size_t)base->n, 1); int any = 0, r = 0;
        for (i = 0; i < base->n; i++) { small[i] = shp_area(base->v[i]->g) < 2.0; any |= small[i]; }          /* shapely.area(array) < 2.0 */
        if (any) {
            PlHits su;
            r = pl_soft_hits(s, &su);
            if (r >= 0 && su.tree) {          /* su.tree.query(array of the small pieces, predicate="intersects")[0] */
                int ni = 0; const GEOSGeometry **q = (const GEOSGeometry **)malloc(sizeof *q * (size_t)base->n); int *idx = (int *)malloc(sizeof(int) * (size_t)base->n);
                ShpIdx qi = { 0 }, ti = { 0 };
                for (i = 0; i < base->n; i++) if (small[i]) { idx[ni] = i; q[ni++] = base->v[i]->g; }
                if (shp_tree_query_bulk(su.tree, q, ni, SHP_PRED_INTERSECTS, &qi, &ti) < 0) r = SC_ERR;
                else for (j = 0; j < qi.n; j++) drop[idx[qi.i[j]]] = 1;
                shp_idx_free(&qi); shp_idx_free(&ti); free(q); free(idx);
            }
            pl_hits_free(&su);
        }
        if (r >= 0) for (i = 0; i < base->n; i++) if (!drop[i]) gl_push(out, base->v[i]);
        free(small); free(drop);
        if (r < 0) { gl_free(&both); return SC_ERR; }
    }
    gl_free(&both);
    for (i = 0; i < s->gcuts.n; i++) {          /* out += [p for p in ps if not _soft_cut(k, p) and k not in allow] */
        Gcut *c = &s->gcuts.v[i];
        for (j = 0; j < c->polys.n; j++) if (!pl_soft_cut(c->kind, c->polys.v[j]) && !allowed(allow, c->kind)) gl_push(out, c->polys.v[j]);
    }
    for (i = 0; i < s->tcuts.n; i++) {
        Tcut *c = &s->tcuts.v[i];
        for (j = 0; j < c->polys.n; j++) if (!pl_soft_cut(c->kind, c->polys.v[j]) && !allowed(allow, c->kind)) gl_push(out, c->polys.v[j]);
    }
    return 0;
}

/* ---------------- evict ---------------- */
typedef struct { Scene *s; const ShpPrep *gp; PlHits su; int made; } Keep;
/* keep(p): not (p.area < 2.0 and gp.intersects(p)) or not su.hit(p), su = _soft_hits(s) made on first need */
static int keep_p(Keep *k, const Geo *p) {
    int x;
    if (!(shp_area(p->g) < 2.0)) return 1;
    x = shp_prep_intersects(k->gp, p->g); TRY(x);
    if (!x) return 1;
    if (!k->made) { k->made = 1; TRY(pl_soft_hits(k->s, &k->su)); }          /* the soft pieces as they were before this call */
    x = pl_hits_hit(&k->su, p->g); TRY(x);
    return !x;
}
int pl_evict(Scene *s, Geo *g) {
    GEOSGeometry *gb = shp_buffer_q(g->g, 0.3, 16); ShpPrep gp; OpList ops = { 0 }; GeoList feet = { 0 }, objs = { 0 };
    Keep kp; int i, j, r = 0, terr = pl_terr(s);
    if (!gb) return SC_ERR;
    gp = shp_prep(gb);
    memset(&kp, 0, sizeof kp); kp.s = s; kp.gp = &gp;
    for (i = 0; r >= 0 && i < s->ops.n; i++) {
        Op *o = s->ops.v[i];
        if (is_soft(o->kind)) {
            GeoList ps = { 0 };
            for (j = 0; j < o->polys.n; j++) { int x = shp_prep_intersects(&gp, o->polys.v[j]->g); if (x < 0) { r = SC_ERR; break; } if (!x) gl_push(&ps, o->polys.v[j]); }
            if (r >= 0 && ps.n) { if (ps.n != o->polys.n) ol_push_own(&ops, op_with(o, &ps)); else ol_push(&ops, o); }
            gl_free(&ps);
        } else ol_push(&ops, o);
    }
    for (i = 0; r >= 0 && i < s->feet.n; i++) { int k = keep_p(&kp, s->feet.v[i]); if (k < 0) r = SC_ERR; else if (k) gl_push(&feet, s->feet.v[i]); }
    if (terr) for (i = 0; r >= 0 && i < s->objs.n; i++) { int k = keep_p(&kp, s->objs.v[i]); if (k < 0) r = SC_ERR; else if (k) gl_push(&objs, s->objs.v[i]); }
    if (kp.made) pl_hits_free(&kp.su);
    if (r < 0) { ol_free(&ops); gl_free(&feet); gl_free(&objs); shp_prep_free(&gp); shp_free(gb); return SC_ERR; }
    ol_free(&s->ops); s->ops = ops;          /* s.ops = ops; s.feet = feet; s.objs = objs */
    gl_free(&s->feet); s->feet = feet;
    if (terr) { gl_free(&s->objs); s->objs = objs; } else gl_free(&objs);
    {   /* s.tcuts: joints lose the part under gb, soft cuts under gb go */
        TcutList nt = { 0 };
        for (i = 0; r >= 0 && i < s->tcuts.n; i++) {
            Tcut *c = &s->tcuts.v[i]; GeoList ps2 = { 0 }; int joint = kind_is(c->kind, "joint");
            for (j = 0; j < c->polys.n; j++) {
                Geo *p = c->polys.v[j]; int x;
                if (joint) {
                    x = shp_prep_intersects(&gp, p->g);
                    if (x < 0) { r = SC_ERR; break; }
                    if (x) { GEOSGeometry *d = shp_difference(p->g, gb); if (!d) { r = SC_ERR; break; } lf_polys_of_new(d, &ps2); }
                    else gl_push(&ps2, p);
                } else {
                    int keep = 1;
                    if (pl_soft_cut(c->kind, p)) { x = shp_prep_intersects(&gp, p->g); if (x < 0) { r = SC_ERR; break; } keep = !x; }
                    if (keep) gl_push(&ps2, p);
                }
            }
            if (r >= 0 && ps2.n) tcl_push(&nt, &ps2, c->z0, c->z1, c->kind);
            gl_free(&ps2);
        }
        if (r < 0) { tcl_free(&nt); shp_prep_free(&gp); shp_free(gb); return SC_ERR; }
        tcl_free(&s->tcuts); s->tcuts = nt;
    }
    {   /* s.gcuts: small pits under gb go */
        GcutList ng = { 0 };
        for (i = 0; r >= 0 && i < s->gcuts.n; i++) {
            Gcut *c = &s->gcuts.v[i]; GeoList ps2 = { 0 }; int pit = kind_is(c->kind, "pit");
            for (j = 0; j < c->polys.n; j++) {
                Geo *p = c->polys.v[j]; int drop = 0;
                if (pit && shp_area(p->g) < 2.5) { int x = shp_prep_intersects(&gp, p->g); if (x < 0) { r = SC_ERR; break; } drop = x; }
                if (!drop) gl_push(&ps2, p);
            }
            if (r >= 0 && ps2.n) gcl_push(&ng, &ps2, c->depth, c->kind);
            gl_free(&ps2);
        }
        if (r < 0) { gcl_free(&ng); shp_prep_free(&gp); shp_free(gb); return SC_ERR; }
        gcl_free(&s->gcuts); s->gcuts = ng;
    }
    shp_prep_free(&gp); shp_free(gb);
    return 0;
}

/* ---------------- surf ---------------- */
int pl_surf(Scene *s, Geo *g, Num *z) {
    Num zz = NF(SC.H); int br = 0, i, j, nb = 0; double ba[256], bt[256], ga = shp_area(g->g), cov = 0.0; const GEOSGeometry *bp[256];          /* the plates under g: area, top */
    *z = num_none();
    if (pl_terr(s)) {          /* lo, hi = T_zr(s, g); hi if hi - lo <= 1.0 else None */
        Num lo, hi;
        TRY(T_zr(s, g, &lo, &hi));
        if (num_sub(hi, lo).v > PL.step) return 0;          /* was 1.0: a detail across a step of up to 1 mm */
        if (pl_terrk_fn && PL.cover > 0) {          /* a raised slab of the ground (bedrock, dune): all on it or all off it */
            double ga = shp_area(g->g), cov = 0.0, top = hi.v; const GEOSGeometry *sl[256]; int ns = 0;
            for (i = 0; i < s->ops.n; i++) { Op *o = s->ops.v[i]; if (o->type != OP_PRISM || !o->kind || !pl_terrk_fn(o->kind)) continue;
                for (j = 0; j < o->polys.n; j++) { const GEOSGeometry *p = o->polys.v[j]->g; GEOSGeometry *q; double a; int x = shp_intersects(p, g->g); TRY(x); if (!x) continue;
                    q = shp_intersection(p, g->g); if (!q) return SC_ERR; a = shp_area(q); shp_free(q); if (a < 1e-3) continue;
                    cov += a; if (ns < 256) sl[ns++] = p; if (o->n[1].v > top) top = o->n[1].v; } }
            if (cov > 0.03 * ga && cov < PL.cover * ga && sc_uncovered(g->g, sl, ns) > (1.0 - PL.cover) * ga) return 0;          /* None: across a slab's edge */
            if (cov >= PL.cover * ga && top > hi.v) { *z = num_f(top); return 0; }          /* on the slab */
        }
        *z = hi;
        return 0;
    }
    for (i = 0; i < s->ops.n; i++) {
        Op *o = s->ops.v[i];
        if (is_soft(o->kind)) continue;
        for (j = 0; j < o->polys.n; j++) {
            const GEOSGeometry *p = o->polys.v[j]->g; int x; GEOSGeometry *q; double a;
            x = shp_intersects(p, g->g); TRY(x);
            if (!x) continue;
            q = shp_intersection(p, g->g); if (!q) return SC_ERR;
            a = shp_area(q); shp_free(q);
            if (a < 1e-3) continue;
            if (o->type == OP_PRISM) {          /* p.buffer(-0.3).contains(g) */
                GEOSGeometry *b = shp_buffer_q(p, -0.3, 16); int c;
                if (!b) return SC_ERR;
                c = shp_contains(b, g->g); shp_free(b); TRY(c);
                if (c) { zz = num_max2(zz, o->n[1]); continue; }
            }
            if (o->type == OP_PRISM && is_bridge(o->kind)) { zz = num_max2(zz, o->n[1]); br = 1; if (nb < 256) { ba[nb] = a; bt[nb] = o->n[1].v; bp[nb] = p; nb++; } }
            else return 0;          /* None */
        }
    }
    if (br) {          /* it must stand on the plates at that level, not hang over a lower surface */
        const GEOSGeometry *lv[256]; int nl = 0;
        for (i = 0; i < nb; i++) if (bt[i] > zz.v - 0.1) { cov += ba[i]; lv[nl++] = bp[i]; }
        if (cov < PL.cover * ga && sc_uncovered(g->g, lv, nl) > (1.0 - PL.cover) * ga) return 0;          /* None: it straddles a plate's edge (a joint between tiles is fine) */
    }
    *z = num_z(zz.v, br);          /* z = Z(z); z.bridge = br */
    return 0;
}

/* ---------------- find ---------------- */
/* one try of find: would g be taken? 1 yes (*z), 0 no, SC_ERR. Pure: it reads the scene, changes nothing, draws no
   random numbers, so asking it early (pl_pre) and again in the loop gives the same answer. */
typedef struct { Scene *s; Geo *region; PlHits *hb; GeoList *hl; PSet *hold, *brid; double gap; int ground, terr; } PlTry;
static int pl_try(const PlTry *T, Geo *g, Num *zout) {
    Scene *s = T->s; int x, i; GEOSGeometry *gb; Num z = num_none();
    *zout = num_none();
    x = shp_is_empty(g->g);
    if (x == 0) { x = shp_contains(T->region->g, g->g); x = x < 0 ? -1 : !x; }          /* g.is_empty or not region.contains(g) */
    if (x) return x < 0 ? SC_ERR : 0;
    gb = shp_buffer_q(g->g, T->gap, 16);
    if (!gb) return SC_ERR;
    x = pl_hits_hit(T->hb, gb);
    if (x < 0) { shp_free(gb); return SC_ERR; }
    if (!x) {
        if (!T->terr) z = NF(SC.H);
        else if (pl_surf(s, g, &z) < 0) { shp_free(gb); return SC_ERR; }
    } else {
        ShpIdx q = { 0 }; int bad = 0;
        if (T->terr) { shp_free(gb); return 0; }
        if (shp_tree_query(T->hb->tree, gb, SHP_PRED_NONE, &q) < 0) bad = -1;
        for (i = 0; !bad && i < q.n; i++) {          /* any(p.intersects(gb) and not (id(p) in brid or (id(p) in hold and p.buffer(-0.3).contains(g)))) */
            Geo *p = T->hl->v[q.i[i]]; int y = shp_intersects(p->g, gb);
            if (y < 0) { bad = -1; break; }
            if (!y || ps_has(T->brid, p)) continue;
            if (ps_has(T->hold, p)) {
                GEOSGeometry *b = shp_buffer_q(p->g, -0.3, 16); int c;
                if (!b) { bad = -1; break; }
                c = shp_contains(b, g->g); shp_free(b);
                if (c < 0) { bad = -1; break; }
                if (c) continue;
            }
            bad = 1;
        }
        shp_idx_free(&q);
        if (bad < 0) { shp_free(gb); return SC_ERR; }
        if (bad) { shp_free(gb); return 0; }
        if (pl_surf(s, g, &z) < 0) { shp_free(gb); return SC_ERR; }
    }
    shp_free(gb);
    if (z.t == NUM_NONE || (T->ground && z.v != SC.H)) return 0;
    *zout = z; return 1;
}
_Thread_local PlPre pl_pre;
static int pl_pre_fn(void *c, const GEOSGeometry *g) {          /* pl_pre.fn: the try's test on a footprint made early */
    Geo *gg; Num z; int r; GEOSGeometry *k = shp_clone(g);
    if (!k) return SC_ERR;
    gg = geo_new(k); r = pl_try((const PlTry *)c, gg, &z); geo_unref(gg);
    return r;
}
static int pre_on(void) { static int on = -1; if (on < 0) { const char *e = getenv("BBB_NO_PRECHECK"); on = !(e && e[0] == '1'); } return on; }

int pl_find(Scene *s, PlMakeFn make, void *mctx, int tries, double gap, Geo *region, int ground, const PlAllow *allow,
            PlAtFn at, void *actx, Geo **gout, Num *zout) {
    GeoList hl = { 0 }, blk = { 0 }; PlHits hb, taken; PSet hold = { 0 }, brid = { 0 }; ShpPrep regp; int i, j, t, r = 0, found = 0, terr = pl_terr(s);
    memset(&taken, 0, sizeof taken); memset(&regp, 0, sizeof regp);
    *gout = NULL; *zout = num_none();
    if (!region) region = PL.disc;
    if (pl_hard(s, allow, &hl) < 0) { gl_free(&hl); return SC_ERR; }
    if (pl_hits_init(&hb, &hl) < 0) { pl_hits_free(&hb); gl_free(&hl); return SC_ERR; }
    for (i = 0; i < s->ops.n; i++) {          /* hold / brid: ids of the pieces of solid prisms / of deck plates */
        Op *o = s->ops.v[i];
        if (o->type != OP_PRISM) continue;
        if (!is_soft(o->kind)) for (j = 0; j < o->polys.n; j++) ps_add(&hold, o->polys.v[j]);
        if (is_bridge(o->kind)) for (j = 0; j < o->polys.n; j++) ps_add(&brid, o->polys.v[j]);
    }
    ps_done(&hold); ps_done(&brid);
    if (at) {
        double asum, ra;
        for (i = 0; i < hl.n; i++) if (!ps_has(&hold, hl.v[i]) && !ps_has(&brid, hl.v[i])) gl_push(&blk, hl.v[i]);
        if (pl_hits_init(&taken, &blk) < 0) r = SC_ERR;
        regp = shp_prep(region->g);
        if (r >= 0 && blk.n) {
            double *ar = (double *)malloc(sizeof(double) * (size_t)blk.n);
            for (i = 0; i < blk.n; i++) ar[i] = shp_area(blk.v[i]->g);
            asum = px_sum(ar, blk.n); free(ar);          /* sum(p.area for p in blk) */
            ra = shp_area(region->g);
            if (asum > 0.5 * ra) {          /* crowded: worth working out exactly how much room is left */
                GEOSGeometry *u = union_list(&blk), *ub = u ? shp_buffer_q(u, gap, 16) : NULL, *d = ub ? shp_difference(region->g, ub) : NULL;
                if (!d) r = SC_ERR;
                else if (shp_area(d) < 0.02 * shp_area(region->g)) r = 1;          /* no room left */
                shp_free(u); shp_free(ub); shp_free(d);
            }
        }
    }
    PlTry T = { s, region, &hb, &hl, &hold, &brid, gap, ground, terr };
    for (t = 0; r == 0 && t < tries; t++) {
        Geo *g = NULL; int err = 0, x; Num z;
        if (at) {
            double cx, cy; GEOSGeometry *pt;
            if (at(actx, &cx, &cy) < 0) { r = SC_ERR; break; }
            pt = shp_point(cx, cy);
            x = shp_prep_contains(&regp, pt);
            if (x == 1) { int nr = pl_hits_near(&taken, pt, gap); x = nr < 0 ? -1 : !nr; }          /* not regp.contains(pt) or taken.near(pt, gap) */
            shp_free(pt);
            if (x < 0) { r = SC_ERR; break; }
            if (!x) continue;
            if (pre_on()) { pl_pre.fn = pl_pre_fn; pl_pre.ctx = &T; pl_pre.armed = 1; }          /* speed plan: the try's test offered to make() */
            g = make(mctx, 1, cx, cy, &err);
            pl_pre.armed = 0;
        } else g = make(mctx, 0, 0.0, 0.0, &err);
        if (err) { geo_unref(g); r = SC_ERR; break; }
        if (!g) continue;
        x = pl_try(&T, g, &z);
        if (x < 0) { geo_unref(g); r = SC_ERR; break; }
        if (!x) { geo_unref(g); continue; }
        if (pl_evict(s, g) < 0) { geo_unref(g); r = SC_ERR; break; }
        *gout = g; *zout = z; found = 1;
        break;
    }
    if (at) { pl_hits_free(&taken); shp_prep_free(&regp); }
    pl_hits_free(&hb); gl_free(&hl); gl_free(&blk); ps_free(&hold); ps_free(&brid);
    if (r < 0) return SC_ERR;
    return found;
}

/* ---------------- notch ---------------- */
int pl_notch(Scene *s, const GeoList *polys, double gap) {
    GEOSGeometry *u, *g; PSet old = { 0 }; GeoList nw = { 0 }; int i, j, k, r = 0;
    if (!PL.notch) return 0;          /* off: in real builds the cut-outs made plate joins fail */
    u = union_list(polys); g = u ? shp_buffer_q(u, gap, 16) : NULL; shp_free(u);
    if (!g) return SC_ERR;
    for (i = 0; r >= 0 && i < s->ops.n; i++) {
        Op *o = s->ops.v[i]; GeoList ps = { 0 };
        if (o->type != OP_PRISM || !is_bridge(o->kind)) continue;
        for (j = 0; j < o->polys.n; j++) {
            Geo *p = o->polys.v[j]; int x = shp_intersects(p->g, g);
            if (x < 0) { r = SC_ERR; break; }
            if (!x) { gl_push(&ps, p); continue; }
            ps_add(&old, p);
            {   /* [r.simplify(0.03) for r in polys_of(p.difference(g).buffer(-0.45, join_style=2).buffer(0.45, join_style=2)) if wide_enough(r) and r.area > 0.6] */
                GEOSGeometry *d = shp_difference(p->g, g), *b1 = d ? shp_buffer(d, -0.45, 16, SHP_CAP_ROUND, SHP_JOIN_MITRE, 5.0, 0) : NULL,
                             *b2 = b1 ? shp_buffer(b1, 0.45, 16, SHP_CAP_ROUND, SHP_JOIN_MITRE, 5.0, 0) : NULL;
                GeoList rs = { 0 };
                shp_free(d); shp_free(b1);
                if (!b2) { r = SC_ERR; break; }
                lf_polys_of_new(b2, &rs);
                for (k = 0; k < rs.n; k++) {
                    int w = lf_wide_enough(rs.v[k], SC.MINW); GEOSGeometry *sm;
                    if (w < 0) { r = SC_ERR; break; }
                    if (!w || !(shp_area(rs.v[k]->g) > 0.6)) continue;
                    sm = shp_simplify(rs.v[k]->g, 0.03, 1);
                    if (!sm) { r = SC_ERR; break; }
                    { Geo *q = geo_new(sm); gl_push(&ps, q); gl_push(&nw, q); geo_unref(q); }
                }
                gl_free(&rs);
                if (r < 0) break;
            }
        }
        if (r >= 0) { Op *n2 = op_with(o, &ps); op_unref(s->ops.v[i]); s->ops.v[i] = n2; }          /* s.ops[i] = (op[0], ps) + tuple(op[2:]) */
        gl_free(&ps);
    }
    if (r >= 0) {          /* s.feet = [p for p in s.feet if id(p) not in old] + new */
        GeoList feet = { 0 };
        ps_done(&old);
        for (i = 0; i < s->feet.n; i++) if (!ps_has(&old, s->feet.v[i])) gl_push(&feet, s->feet.v[i]);
        gl_extend(&feet, &nw);
        gl_free(&s->feet); s->feet = feet;
    }
    if (r >= 0) {          /* s.tcuts: every cut loses the part under g */
        TcutList nt = { 0 };
        for (i = 0; r >= 0 && i < s->tcuts.n; i++) {
            Tcut *c = &s->tcuts.v[i]; GeoList ps2 = { 0 };
            for (j = 0; j < c->polys.n; j++) {
                GEOSGeometry *d = shp_difference(c->polys.v[j]->g, g); GeoList rs = { 0 };
                if (!d) { r = SC_ERR; break; }
                lf_polys_of_new(d, &rs);
                for (k = 0; k < rs.n; k++) {
                    GEOSGeometry *b = shp_buffer_q(rs.v[k]->g, -0.1, 16);
                    if (!b) { r = SC_ERR; break; }
                    if (!shp_is_empty(b)) gl_push(&ps2, rs.v[k]);
                    shp_free(b);
                }
                gl_free(&rs);
                if (r < 0) break;
            }
            if (r >= 0 && ps2.n) tcl_push(&nt, &ps2, c->z0, c->z1, c->kind);
            gl_free(&ps2);
        }
        if (r >= 0) { tcl_free(&s->tcuts); s->tcuts = nt; } else tcl_free(&nt);
    }
    ps_free(&old); gl_free(&nw); shp_free(g);
    return r < 0 ? SC_ERR : 0;
}

/* ---------------- up / mound / cut / topcut ---------------- */
/* review plan B2 "edge wear on slabs" (2026-11-22): a stone slab, flagstone, headstone, tomb or step gets a few small
   bites out of its outline (worn, chipped edges); its own hash, so the layout's random numbers are not touched.
   Bites about 0.9 mm (FDM) or 0.6 mm (resin) across, give or take; a piece smaller than 4 mm2 is left as it is.
   settings "edge_wear": false turns it off. */
int cp_edge_wear = 1;          /* scene.c sc_prism calls these for every prism */
int wear_kind(const Kind *k) {
    static const char *W[] = { "slab", "flag", "paving", "headstone", "tomb", "grave", "step", "plinth", "altar", NULL };
    if (!k || !k->s) return 0;
    for (int i = 0; W[i]; i++) if (strstr(k->s, W[i])) return 1;
    return 0;
}
void wear_polys(GeoList *ps) {
    extern double kit_min_detail; double bite = kit_min_detail < 2.5 ? 0.6 : 0.9; GeoList out = { 0 };
    for (int i = 0; i < ps->n; i++) {
        GEOSGeometry *g = ps->v[i]->g, *w; const GEOSGeometry *L; double area = shp_area(g), len, bb[4]; uint32_t h; int n;
        if (area < 4.0 || !(L = shp_exterior(g))) { gl_push(&out, ps->v[i]); continue; }
        len = shp_length(L); shp_bounds(g, bb);
        h = (uint32_t)(int64_t)floor(bb[0] * 997.0) * 2654435761u ^ (uint32_t)(int64_t)floor(bb[1] * 991.0) * 40503u ^ (uint32_t)(int64_t)floor(area * 100.0);
#define WRND() (h = h * 1664525u + 1013904223u, (double)(h >> 8) / 16777216.0)
        n = 3 + (int)(WRND() * 3); w = shp_clone(g);
        for (int k = 0; k < n && w; k++) {
            GEOSGeometry *pt = shp_interpolate(L, WRND() * len, 0), *c, *d;
            if (!pt) continue;
            c = shp_point_buffer(shp_x(pt), shp_y(pt), bite * (0.6 + 0.8 * WRND()), 1); shp_free(pt);          /* a square turned 45 degrees: an angular chip, not a round bite */
            d = c ? shp_difference(w, c) : NULL; shp_free(c);
            if (d && !shp_is_empty(d) && shp_area(d) > 0.8 * area) { shp_free(w); w = d; } else shp_free(d);
        }
#undef WRND
        if (w && !shp_is_empty(w)) { ShpList l = { 0 }; shp_polys_of(w, &l); for (int k = 0; k < l.n; k++) { gl_push_own(&out, geo_new(l.g[k])); l.g[k] = NULL; } shp_list_free(&l); }
        else gl_push(&out, ps->v[i]);
        shp_free(w);
    }
    gl_free(ps); *ps = out;
}
int pl_up(Scene *s, const GeoList *polys, Num z, Num h, Kind *kind, Num *top) {
    GeoList ps = { 0 }, got = { 0 }; int i, r; Num t, z0;
    *top = num_none();
    for (i = 0; i < polys->n; i++) if (polys->v[i] && !shp_is_empty(polys->v[i]->g)) gl_push(&ps, polys->v[i]);
    if (!ps.n) { gl_free(&ps); return 0; }
    if (h.v >= 1.0) h = num_mul(h, NF(PL.hboost));          /* h *= HBOOST[0] */
    if (pl_terr(s)) {          /* got = tprism(s, polys, h, kind); got[0][1] if got else None */
        TPiece *tp = NULL; int np_ = 0;
        r = tprism(s, &ps, h, kind, &tp, &np_);
        if (r >= 0 && np_) *top = tp[0].top;
        for (i = 0; i < np_; i++) geo_unref(tp[i].g);
        free(tp); gl_free(&ps);
        return r < 0 ? SC_ERR : 0;
    }
    t = num_min2(num_add(z, h), NF(SC.CAP));          /* top = min(z + h, CAPZ) */
    { extern double cp_prop_sink; if (cp_prop_sink > 0 && z.v <= SC.H + 1e-9 && !is_bridge_z(z) && h.v > 0.5) t = num_min2(num_add(z, NF(h.v - fmin(cp_prop_sink, 0.15 * h.v))), NF(SC.CAP)); }          /* review N7 (2026-11-22): a thing on the ground sits in it, min(0.4 mm, 15 %) lower */
    if (num_sub(t, z).v < 0.25) { gl_free(&ps); return 0; }
    if (is_bridge_z(z) && pl_notch(s, &ps, 0.3) < 0) { gl_free(&ps); return SC_ERR; }
    z0 = (z.v <= SC.H || is_bridge_z(z)) ? NF(SC.H - SC.SINK) : num_sub(z, NF(0.15));
    r = sc_prism(s, &ps, t, kind, z0, SC.MINW, 1, &got);
    if (r >= 0 && got.n) *top = t;
    gl_free(&ps); gl_free(&got);
    return r < 0 ? SC_ERR : 0;
}

int pl_mound(Scene *s, const GeoList *polys, Num z, Num h, const Tier *tiers, int nt, Kind *kind) {
    GeoList tops = { 0 }; Num top, z0; int r;
    if (h.v >= 1.0) h = num_mul(h, NF(PL.hboost));
    if (pl_terr(s)) return tstack(s, polys, h, tiers, nt, kind) < 0 ? SC_ERR : 0;
    h = num_min2(h, num_sub(num_sub(NF(SC.CAP), z), NF(0.05)));          /* h = min(h, CAPZ - z - 0.05) */
    if (h.v < 0.25) return 0;
    if (is_bridge_z(z)) { TRY(pl_notch(s, polys, 0.3)); h = num_add(h, num_sub(z, NF(SC.H))); z = NF(SC.H); }
    z0 = z.v <= SC.H ? num_none() : num_add(num_sub(z, NF(0.15)), NF(SC.SINK));
    r = sc_stack(s, polys, h, tiers, nt, kind, z0, &tops, &top);
    gl_free(&tops);
    return r < 0 ? SC_ERR : 0;
}

int pl_cut(Scene *s, const GeoList *polys, Num z, Num depth, Kind *kind) {
    if (pl_terr(s)) return tcut_flat(s, polys, depth, kind) < 0 ? SC_ERR : 0;
    if (z.v <= SC.H) return sc_gcut(s, polys, depth, kind) < 0 ? SC_ERR : 0;
    return sc_tcut(s, polys, num_sub(z, depth), num_add(z, NF(0.3)), kind) < 0 ? SC_ERR : 0;
}

int pl_topcut(Scene *s, const GeoList *polys, Num ztop, Num depth, Kind *kind) {
    return sc_tcut(s, polys, num_sub(ztop, depth), num_add(ztop, NF(0.5)), kind ? kind : kind_str("seam")) < 0 ? SC_ERR : 0;
}

/* ---------------- drape ---------------- */
typedef struct { Num z; GeoList ps; } Level;
static int level_of(Level **lv, int *n, int *cap, Num z) {          /* levels.setdefault(z, []): the first key kept */
    int i;
    for (i = 0; i < *n; i++) if ((*lv)[i].z.v == z.v) return i;
    grow((void **)lv, cap, *n, sizeof(Level)); memset(&(*lv)[*n], 0, sizeof(Level)); (*lv)[*n].z = z;
    return (*n)++;
}
/* [p for p in ps if wide_enough(p) and p.area > 0.6] */
static int good_pieces(const GeoList *ps, GeoList *out) {
    int i;
    for (i = 0; i < ps->n; i++) { int w = lf_wide_enough(ps->v[i], SC.MINW); TRY(w); if (w && shp_area(ps->v[i]->g) > 0.6) gl_push(out, ps->v[i]); }
    return 0;
}
typedef struct { Op *o; double key; int i; } PrismKey;
static int pk_cmp(const void *a, const void *b) {          /* sorted(..., key=lambda o: -o[3]): stable */
    const PrismKey *x = (const PrismKey *)a, *y = (const PrismKey *)b;
    if (x->key < y->key) return -1;
    if (y->key < x->key) return 1;
    return x->i - y->i;
}
int pl_drape(Scene *s, Geo *g0, Num h, Kind *kind) {
    GEOSGeometry *gi = shp_intersection(g0->g, PL.disc->g); Geo *g; int i, j, r = 0;
    if (!gi) return SC_ERR;
    g = geo_new(gi);
    if (pl_terr(s)) {
        GeoList hl = { 0 }, pieces = { 0 }; GEOSGeometry *b, *gd; double E, xs[4096]; int64_t nx;
        if (pl_hard(s, NULL, &hl) < 0) { gl_free(&hl); geo_unref(g); return SC_ERR; }
        if (hl.n) { GEOSGeometry *u = union_list(&hl); b = u ? shp_buffer_q(u, 0.35, 16) : NULL; shp_free(u); }          /* unary_union(hard(s)).buffer(0.35) */
        else b = shp_polygon(NULL, 0);          /* Polygon() */
        gl_free(&hl);
        gd = b ? shp_difference(g->g, b) : NULL; shp_free(b);
        geo_unref(g);
        if (!gd) return SC_ERR;
        g = geo_new(gd);
        E = 16 * (SC.K > 1.0 ? SC.K : 1.0);          /* 16 * max(1.0, gg.K) */
        nx = np_arange(-E, E, 3.0, xs, 4096);
        for (i = 0; r >= 0 && i < nx; i++) {          /* [q for x in xs for q in polys_of(g.intersection(box(x, -E, x + 3.0, E))) if wide_enough(q)] */
            GEOSGeometry *bx = shp_box(xs[i], -E, xs[i] + 3.0, E, 1), *q = bx ? shp_intersection(g->g, bx) : NULL; GeoList qs = { 0 };
            shp_free(bx);
            if (!q) { r = SC_ERR; break; }
            lf_polys_of_new(q, &qs);
            for (j = 0; j < qs.n; j++) { int w = lf_wide_enough(qs.v[j], SC.MINW); if (w < 0) { r = SC_ERR; break; } if (w) gl_push(&pieces, qs.v[j]); }
            gl_free(&qs);
        }
        if (r >= 0 && pieces.n) {
            r = pl_evict(s, g);
            if (r >= 0) { TPiece *tp = NULL; int np_ = 0; r = tprism(s, &pieces, h, kind, &tp, &np_); for (i = 0; i < np_; i++) geo_unref(tp[i].g); free(tp); }
        }
        gl_free(&pieces); geo_unref(g);
        return r < 0 ? SC_ERR : 0;
    }
    {   /* tapers and tool cuts: keep 0.35 off them */
        GeoList tap = { 0 };
        for (i = 0; i < s->ops.n; i++) if (s->ops.v[i]->type == OP_TAPER && !is_soft(s->ops.v[i]->kind)) gl_extend(&tap, &s->ops.v[i]->polys);
        for (i = 0; i < s->tcuts.n; i++) gl_extend(&tap, &s->tcuts.v[i].polys);
        if (tap.n) {
            GEOSGeometry *u = union_list(&tap), *b = u ? shp_buffer_q(u, 0.35, 16) : NULL, *d = b ? shp_difference(g->g, b) : NULL;
            shp_free(u); shp_free(b);
            if (!d) { gl_free(&tap); geo_unref(g); return SC_ERR; }
            geo_unref(g); g = geo_new(d);
        }
        gl_free(&tap);
    }
    if (pl_evict(s, g) < 0) { geo_unref(g); return SC_ERR; }
    {
        Geo *rest = geo_ref(g); Level *lv = NULL; int nl = 0, cl = 0; PrismKey *pk; int npk = 0; Num zg; int nbt = 0; Num bt = num_none();
        GeoList allp = { 0 };
        pk = (PrismKey *)malloc(sizeof(PrismKey) * (size_t)(s->ops.n + 1));
        for (i = 0; i < s->ops.n; i++) {
            Op *o = s->ops.v[i];
            if (o->type == OP_PRISM && !is_soft(o->kind) && !is_bridge(o->kind)) { pk[npk].o = op_ref(o); pk[npk].key = -o->n[1].v; pk[npk].i = npk; npk++; }
        }
        qsort(pk, (size_t)npk, sizeof *pk, pk_cmp);
        for (i = 0; r >= 0 && i < npk; i++) {
            Op *o = pk[i].o;
            for (j = 0; j < o->polys.n; j++) {
                Geo *p = o->polys.v[j]; Geo *ins = lf_inset(p, 0.3); GEOSGeometry *piece, *pb, *nr;
                if (!ins) { r = SC_ERR; break; }
                piece = shp_intersection(rest->g, ins->g); geo_unref(ins);
                if (!piece) { r = SC_ERR; break; }
                if (!shp_is_empty(piece)) { int k = level_of(&lv, &nl, &cl, o->n[1]); lf_polys_of_new(piece, &lv[k].ps); } else shp_free(piece);
                pb = shp_buffer_q(p->g, 0.3, 16); nr = pb ? shp_difference(rest->g, pb) : NULL; shp_free(pb);
                if (!nr) { r = SC_ERR; break; }
                geo_unref(rest); rest = geo_new(nr);
            }
        }
        for (i = 0; i < npk; i++) op_unref(pk[i].o);
        free(pk);
        for (i = 0; r >= 0 && i < s->ops.n; i++) {          /* bt = [op[3] for bridge prisms for p in op[1] if p.intersects(rest)] */
            Op *o = s->ops.v[i];
            if (o->type != OP_PRISM || !is_bridge(o->kind)) continue;
            for (j = 0; j < o->polys.n; j++) {
                int x = shp_intersects(o->polys.v[j]->g, rest->g);
                if (x < 0) { r = SC_ERR; break; }
                if (x) { bt = nbt ? num_max2(bt, o->n[1]) : o->n[1]; nbt++; }
            }
        }
        if (r >= 0) {
            int k;
            zg = num_z(nbt ? bt.v : SC.H, nbt > 0);          /* zg = Z(max(bt) if bt else H); zg.bridge = bool(bt) */
            k = level_of(&lv, &nl, &cl, zg);
            lf_polys_of(rest, &lv[k].ps);
            for (i = 0; r >= 0 && i < nl; i++) r = good_pieces(&lv[i].ps, &allp);
            if (r >= 0 && nbt && allp.n) r = pl_notch(s, &allp, 0.3);
            for (i = 0; r >= 0 && i < nl; i++) {
                GeoList ps = { 0 }; Num top;
                r = good_pieces(&lv[i].ps, &ps);
                if (r >= 0 && ps.n) r = pl_up(s, &ps, lv[i].z, h, kind, &top);
                gl_free(&ps);
            }
        }
        for (i = 0; i < nl; i++) gl_free(&lv[i].ps);
        free(lv); gl_free(&allp); geo_unref(rest);
    }
    geo_unref(g);
    return r < 0 ? SC_ERR : 0;
}

/* ---------------- cluster / place_group ---------------- */
int pl_cluster(Scene *s, PyRandom *rng, int n, const PlOne *one, double cx, double cy, double spread, double gap, GeoList *out) {
    int t, i;
    (void)s;
    if (one->has_shape && (one->sh.kind == SH_ROCK || one->sh.kind == SH_ROUND) && PL.crocks && n <= 64) {          /* cstone.cluster(rng, sp, ...) */
        static ScatterOut so; ScatterSpec sp; PyRandom loc = *rng; int k;
        memset(&sp, 0, sizeof sp);
        sp.round_ = one->sh.kind == SH_ROUND; sp.r0 = one->sh.a[0]; sp.r1 = one->sh.a[1];
        if (!sp.round_) { sp.nv_lo = (int)one->sh.a[2]; sp.nv_hi = (int)one->sh.a[3]; }          /* cspec = (0, r0, r1, nv[0], nv[1], 0, 0.0) / (1, r0, r1, 0, 0, 0, 0.0) */
        sp.csx = PL.csx; sp.csy = PL.csy;
        k = scatter_cluster(&loc, &sp, n, cx, cy, spread, gap, &so);
        if (k >= 0 && so.ncoord <= 8192) {
            int j = 0;
            *rng = loc;
            for (i = 0; i < k; i++) {          /* Polygon([(xy[2t], xy[2t+1]) for t in range(j, j + m)]) */
                GEOSGeometry *p = shp_polygon(so.xy + 2 * j, so.len[i]);
                if (!p) return SC_ERR;
                gl_push_own(out, geo_new(p)); j += so.len[i];
            }
            return 0;
        }
    }
    for (t = 0; t < n * 30; t++) {
        double x, y; Geo *g; int err = 0, ok = 1;
        if (out->n >= n) break;
        x = cx + py_uniform(rng, -spread, spread);
        y = cy + py_uniform(rng, -spread, spread);
        g = one->has_shape ? lf_shape(&one->sh, x, y, one->srng) : one->fn(one->ctx, x, y, &err);
        if (err || !g) { geo_unref(g); return SC_ERR; }
        for (i = 0; i < out->n; i++) if (!(shp_distance(g->g, out->v[i]->g) > gap)) { ok = 0; break; }          /* all(g.distance(q) > gap for q in got) */
        if (ok) gl_push_own(out, g); else geo_unref(g);
    }
    return 0;
}

typedef struct { PlShapesAtFn fn; void *ctx; GeoList holder; PyRandom *rng; } PG;
static Geo *pg_make(void *c, int has_c, double cx, double cy, int *err) {          /* mk(cx, cy) */
    PG *pg = (PG *)c; GeoList ps = { 0 }; GEOSGeometry *u;
    (void)has_c;
    if (pg->fn(pg->ctx, cx, cy, &ps) < 0) { gl_free(&ps); *err = 1; return NULL; }
    if (!ps.n) { gl_free(&ps); return NULL; }          /* if not ps: return None */
    gl_free(&pg->holder); pg->holder = ps;          /* holder["ps"] = ps */
    u = union_list(&ps);
    if (!u) { *err = 1; return NULL; }
    return geo_new(u);
}
static int pg_at(void *c, double *x, double *y) { lf_rp(((PG *)c)->rng, 0, 12, x, y); return 0; }          /* lambda: rp(rng, 0, 12) */
int pl_place_group(Scene *s, PyRandom *rng, PlShapesAtFn shapes_at, void *ctx, int tries, double gap, int ground,
                   Geo *region, GeoList *ps_out, Num *z) {
    PG pg; Geo *g = NULL; int r;
    memset(&pg, 0, sizeof pg); pg.fn = shapes_at; pg.ctx = ctx; pg.rng = rng;
    *z = num_none();
    r = pl_find(s, pg_make, &pg, tries, gap, region, ground, NULL, pg_at, &pg, &g, z);
    if (r == 1) { gl_extend(ps_out, &pg.holder); geo_unref(g); }
    else *z = num_none();
    gl_free(&pg.holder);
    return r;
}

/* ---------------- crack_plate ---------------- */
int pl_crack_plate(Scene *s, PyRandom *rng, Geo *reg, Num z, Num h, Kind *kind, int npts, double w) {
    GeoList one = { 0 }, segs = { 0 }; Num top; double m, E, *pts, bx[4]; int64_t np2; int i, j, r = 0, err = 0;
    GEOSGeometry *mp, *env, *vor; PySetF *edges; Geo *inner; GEOSGeometry *nb; ShpPrep near;
    gl_push(&one, reg);
    r = pl_up(s, &one, z, h, kind, &top); gl_free(&one);
    if (r < 0) return SC_ERR;
    if (top.t == NUM_NONE) return 0;
    m = SC.K > 1.0 ? SC.K : 1.0;          /* max(1.0, gg.K) */
    E = 17 * m;
    np2 = (int64_t)py_rint((double)npts * lf_fpow(m, 2.0, &err));          /* round(npts * max(1.0, gg.K) ** 2) */
    pts = (double *)malloc(sizeof(double) * 2 * (size_t)(np2 > 0 ? np2 : 1));
    for (i = 0; i < np2; i++) { pts[2 * i] = py_uniform(rng, -E, E); pts[2 * i + 1] = py_uniform(rng, -E, E); }
    mp = shp_multipoint(pts, (int)np2); free(pts);
    env = shp_box(-E - 3, -E - 3, E + 3, E + 3, 1);
    vor = (mp && env) ? shp_voronoi(mp, env, 0.0, 0) : NULL;
    shp_free(mp); shp_free(env);
    if (!vor) return SC_ERR;
    edges = pyset_new(2, 2);
    {   /* edges.add(tuple(sorted((a_, b_)))) for each side of each cell, the corners rounded to 3 places */
        int ng = shp_num_geoms(vor), cap = 0; double *xy = NULL;
        for (i = 0; i < ng; i++) {
            const GEOSGeometry *cell = shp_geom_n(vor, i), *ext = shp_exterior(cell); int nc;
            if (!ext) continue;
            nc = G.GEOSGetNumCoordinates_r(gctx, ext);
            if (nc > cap) { cap = nc * 2; xy = (double *)realloc(xy, sizeof(double) * 2 * (size_t)cap); }
            nc = shp_coords(ext, xy, cap);
            for (j = 0; j < 2 * nc; j++) xy[j] = px_round(xy[j], 3);
            for (j = 0; j + 1 < nc; j++) {
                double key[4]; const double *a = xy + 2 * j, *b = xy + 2 * j + 2;
                if (px_tuple_lt(b, a, 2)) { const double *t = a; a = b; b = t; }
                key[0] = a[0]; key[1] = a[1]; key[2] = b[0]; key[3] = b[1];
                pyset_add(edges, key);
            }
        }
        free(xy);
    }
    shp_free(vor);
    inner = lf_inset(reg, 0.35);
    nb = inner ? shp_buffer_q(inner->g, w + 0.6, 16) : NULL;
    if (!nb) { geo_unref(inner); pyset_free(edges); return SC_ERR; }
    near = shp_prep(nb); shp_bounds(nb, bx);
    {
        int64_t pos = 0; const double *k;
        while ((k = pyset_next(edges, &pos))) {
            double p4[8]; GEOSGeometry *ln; int x;
            lf_wiggle(k[0], k[1], k[2], k[3], rng, 0.2, 3, p4);          /* always drawn, so the same seed gives the same cracks */
            if ((k[0] > k[2] ? k[0] : k[2]) < bx[0] - 1 || (k[0] < k[2] ? k[0] : k[2]) > bx[2] + 1 ||
                (k[1] > k[3] ? k[1] : k[3]) < bx[1] - 1 || (k[1] < k[3] ? k[1] : k[3]) > bx[3] + 1) continue;
            ln = shp_linestring(p4, 4);
            if (!ln) { r = SC_ERR; break; }
            x = shp_prep_intersects(&near, ln);
            if (x < 0) { shp_free(ln); r = SC_ERR; break; }
            if (x) {          /* only cracks that reach this plate */
                GEOSGeometry *b = shp_buffer(ln, w, 4, SHP_CAP_ROUND, SHP_JOIN_ROUND, 5.0, 0);
                if (!b) { shp_free(ln); r = SC_ERR; break; }
                gl_push_own(&segs, geo_new(b));
            }
            shp_free(ln);
        }
    }
    shp_prep_free(&near); shp_free(nb); pyset_free(edges);
    if (r >= 0 && segs.n) {
        GEOSGeometry *net = union_list(&segs), *q = net ? shp_intersection(net, inner->g) : NULL; GeoList qs = { 0 };
        shp_free(net);
        if (!q) r = SC_ERR;
        else { gl_push_own(&qs, geo_new(q)); r = pl_topcut(s, &qs, top, NF(0.3), kind_str("mudcrack")); gl_free(&qs); }
    }
    gl_free(&segs); geo_unref(inner);
    return r < 0 ? SC_ERR : 0;
}
