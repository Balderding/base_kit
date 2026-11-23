/* scene.c: the scene object and its writers (see scene.h). Each function names the Python it copies:
   gg.Scene (gg.py) and gg2.Scene (gg2.py) methods, gg.LK, gg3.Z, the gg2 terrain functions. sizefix does not rewrite
   any of them (the Scene classes are skipped whole, the T_* functions are in sizefix.SKIP), so this is the source as
   written. The geometry is cport\shp.c (shapely 2.2.0's own GEOS calls); the leaves are cport\leaf.c. */
#include <stdlib.h>
#include <stdatomic.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include "shp.h"
#include "pyx.h"
#include "pyexact.h"
#include "leaf.h"
#include "scene.h"

#define PI 3.141592653589793          /* math.pi */
#define TRY(x) do { if ((x) < 0) return SC_ERR; } while (0)

SceneCfg SC;

/* gg / gg2 module globals: R0, H, RIM = BASE_MM / 2.0, 4.0, 12.0; K = R0 / 15.0; CLIP = R0 + 1.0 (gg2 sets it to
   R0 + 0.15 at import); BLANK = R0 + 0.3; SINK = 0.2; MINW = 0.88; LEDGE / LAP = 0 / 0 with the mesh builder, else
   0.05 / 0.03; gg2: CUTR = R0 + 1.0, CAP = H + gg.max_height() */
int scene_config(double base_mm, int mesh, double ts, double max_h, int chamfer_rocks, int base_first, int gg2) {
    SC.base_mm = base_mm; SC.R0 = base_mm / 2.0; SC.H = 4.0; SC.K = SC.R0 / 15.0;
    SC.CLIP = gg2 ? SC.R0 + 0.15 : SC.R0 + 1.0; SC.BLANK = SC.R0 + 0.3; SC.SINK = 0.2; SC.MINW = 0.88;
    SC.mesh = mesh; SC.LEDGE = mesh ? 0.0 : 0.05; SC.LAP = mesh ? 0.0 : 0.03; SC.ts = ts;
    SC.CUTR = SC.R0 + 1.0; SC.CAP = SC.H + max_h;
    SC.chamfer_rocks = chamfer_rocks; SC.base_first = base_first; SC.layout = 0;
    return shp_init() && px_init() == 1;
}

/* ======================= handles and lists ======================= */
static _Atomic unsigned long long geo_uids;          /* PLAN-threads A: every Geo gets a new id, never reused */
Geo *geo_new(GEOSGeometry *g) { Geo *h; if (!g) return NULL; h = (Geo *)malloc(sizeof *h); h->g = g; h->rc = 1; h->uid = ++geo_uids; return h; }
Geo *geo_ref(Geo *g) { if (g) g->rc++; return g; }
void geo_unref(Geo *g) { if (g && --g->rc == 0) { shp_free(g->g); free(g); } }
static void grow(void **v, int *cap, int n, size_t sz) { if (n >= *cap) { *cap = *cap ? *cap * 2 : 8; *v = realloc(*v, sz * (size_t)*cap); } }
void gl_push_own(GeoList *l, Geo *g) { grow((void **)&l->v, &l->cap, l->n, sizeof(Geo *)); l->v[l->n++] = g; }
void gl_push(GeoList *l, Geo *g) { gl_push_own(l, geo_ref(g)); }
void gl_extend(GeoList *l, const GeoList *o) { int i, n = o->n; for (i = 0; i < n; i++) gl_push(l, o->v[i]); }
void gl_clear(GeoList *l) { int i; for (i = 0; i < l->n; i++) geo_unref(l->v[i]); l->n = 0; }
void gl_free(GeoList *l) { gl_clear(l); free(l->v); l->v = NULL; l->cap = 0; }
void gl_copy(GeoList *dst, const GeoList *src) { dst->v = NULL; dst->n = dst->cap = 0; gl_extend(dst, src); }
int gl_index(const GeoList *l, const Geo *g) { int i; for (i = 0; i < l->n; i++) if (l->v[i] == g) return i; return -1; }

/* ======================= numbers ======================= */
/* the type of a + b / a - b / a * b in Python: int op int -> int; numpy.float64 wins over int / float (its reflected
   method runs first, as a float subclass) but not over a gg3.Z on the left (float.__add__ takes it); else float */
static unsigned char rtype(Num a, Num b) {
    int ai = a.t == NUM_INT || a.t == NUM_BOOL, bi = b.t == NUM_INT || b.t == NUM_BOOL;
    if (ai && bi) return NUM_INT;
    if (a.t == NUM_NP) return NUM_NP;
    if (b.t == NUM_NP && a.t != NUM_Z) return NUM_NP;
    return NUM_FLOAT;
}
Num num_add(Num a, Num b) { Num r = { a.v + b.v, rtype(a, b), 0 }; return r; }
Num num_sub(Num a, Num b) { Num r = { a.v - b.v, rtype(a, b), 0 }; return r; }
Num num_mul(Num a, Num b) { Num r = { a.v * b.v, rtype(a, b), 0 }; return r; }
Num num_round(Num a, int nd) {
    Num r = a;
    if (a.t == NUM_INT || a.t == NUM_BOOL) { r.t = NUM_INT; return r; }          /* round(int, n) -> int */
    if (a.t == NUM_NP) {          /* numpy: rint(x * 10 ** n) / 10 ** n */
        double f = 1.0; int i; for (i = 0; i < nd; i++) f *= 10.0;
        r.v = py_rint(a.v * f) / f; return r;
    }
    r.v = px_round(a.v, nd); r.t = NUM_FLOAT; r.bridge = 0;          /* float.__round__ (a Z gives a plain float) */
    return r;
}
static Num NF(double v) { return num_f(v); }

/* ======================= kinds ======================= */
static Kind **interned; static int n_int, cap_int;
Kind *kind_str(const char *s) {
    int i; Kind *k;
    for (i = 0; i < n_int; i++) if (strcmp(interned[i]->s, s) == 0) return interned[i];
    k = (Kind *)calloc(1, sizeof *k); k->rc = 1 << 30; k->s = _strdup(s);
    grow((void **)&interned, &cap_int, n_int, sizeof(Kind *)); interned[n_int++] = k;
    return k;
}
Kind *kind_lk(const char *s) { Kind *k = (Kind *)calloc(1, sizeof *k); k->rc = 1; k->lk = 1; k->s = _strdup(s); return k; }
Kind *kind_ref(Kind *k) { if (k && k->lk) k->rc++; return k; }
void kind_unref(Kind *k) { if (k && k->lk && --k->rc == 0) { free(k->s); free(k); } }
/* LK.__eq__: two LKs are equal only when they are the same object; an LK and a str compare as text */
int kind_eq(const Kind *a, const Kind *b) { if (a->lk && b->lk) return a == b; return strcmp(a->s, b->s) == 0; }

/* ======================= ops and the other entries ======================= */
int sc_feat; const char *sc_fname; static int feat_n, feat_depth;
int sc_feat_push(const char *name) { int saved = sc_feat; if (feat_depth++ == 0) { sc_feat = ++feat_n; sc_fname = name; } return saved; }
void sc_feat_pop(int saved) { if (--feat_depth == 0) { sc_feat = saved; if (!saved) sc_fname = NULL; } }
static Op *op_new(int type, const GeoList *polys, Kind *kind) { Op *o = (Op *)calloc(1, sizeof *o); o->rc = 1; o->type = type; gl_copy(&o->polys, polys); o->kind = kind_ref(kind); o->feat = sc_feat; o->fname = sc_fname; return o; }
Op *op_like(const Op *o, const GeoList *ps) { Op *n = o->type == OP_PRISM ? op_prism(ps, o->n[0], o->n[1], o->kind) : op_taper(ps, o->n[0], o->n[1], o->n[2], o->kind); n->feat = o->feat; n->fname = o->fname; return n; }
Op *op_prism(const GeoList *polys, Num z0, Num top, Kind *kind) { Op *o = op_new(OP_PRISM, polys, kind); o->n[0] = z0; o->n[1] = top; o->n[2] = num_none(); return o; }
Op *op_taper(const GeoList *polys, Num z, Num h, Num angle, Kind *kind) { Op *o = op_new(OP_TAPER, polys, kind); o->n[0] = z; o->n[1] = h; o->n[2] = angle; return o; }
Op *op_ref(Op *o) { if (o) o->rc++; return o; }
void op_unref(Op *o) { if (o && --o->rc == 0) { gl_free(&o->polys); kind_unref(o->kind); free(o); } }
void ol_push_own(OpList *l, Op *o) { grow((void **)&l->v, &l->cap, l->n, sizeof(Op *)); l->v[l->n++] = o; }
void ol_push(OpList *l, Op *o) { ol_push_own(l, op_ref(o)); }
void ol_free(OpList *l) { int i; for (i = 0; i < l->n; i++) op_unref(l->v[i]); free(l->v); l->v = NULL; l->n = l->cap = 0; }
void gcl_push(GcutList *l, const GeoList *polys, Num depth, Kind *kind) {
    Gcut *c; grow((void **)&l->v, &l->cap, l->n, sizeof(Gcut)); c = &l->v[l->n++];
    gl_copy(&c->polys, polys); c->depth = depth; c->kind = kind_ref(kind);
}
void tcl_push(TcutList *l, const GeoList *polys, Num z0, Num z1, Kind *kind) {
    Tcut *c; grow((void **)&l->v, &l->cap, l->n, sizeof(Tcut)); c = &l->v[l->n++];
    gl_copy(&c->polys, polys); c->z0 = z0; c->z1 = z1; c->kind = kind_ref(kind);
}
void trl_push(TerrList *l, Geo *fp, Num zb, Num zt, Geo *top) {
    Terr *t; grow((void **)&l->v, &l->cap, l->n, sizeof(Terr)); t = &l->v[l->n++];
    t->fp = geo_ref(fp); t->zb = zb; t->zt = zt; t->top = geo_ref(top);
}
void chl_push_own(ChopList *l, Chop *c) { grow((void **)&l->v, &l->cap, l->n, sizeof(Chop *)); l->v[l->n++] = c; }
static void chop_unref(Chop *c) { if (c && --c->rc == 0) { kind_unref(c->kind); geo_unref(c->poly); ol_free(&c->ops); free(c); } }

/* ======================= the scene ======================= */
static GEOSGeometry *disc_at(double r) { return shp_point_buffer(0, 0, r, 256); }

/* gg.Scene.__init__(name, title, seed, ground); gg2.Scene.__init__ adds cdisc */
Scene *scene_new(const char *name, const char *title, const char *seed, const char *ground, int gg2) {
    Scene *s = (Scene *)calloc(1, sizeof *s);
    s->name = _strdup(name); s->title = _strdup(title); s->ground = _strdup(ground ? ground : "");
    py_seed_str(&s->rng, seed);
    s->disc = geo_new(disc_at(SC.CLIP));
    s->gg2 = gg2;
    if (gg2) s->cdisc = geo_new(disc_at(SC.CUTR));
    return s;
}
/* gg2.mk(style, title, n, ground): Scene(f"{style}_{n:02d}", f"{title} #{n}", f"{style}{n}", ground) */
Scene *scene_mk(const char *style, const char *title, int n, const char *ground) {
    char a[256], b[256], c[256];
    snprintf(a, sizeof a, "%s_%02d", style, n); snprintf(b, sizeof b, "%s #%d", title, n); snprintf(c, sizeof c, "%s%d", style, n);
    return scene_new(a, b, c, ground, 1);
}
void scene_free(Scene *s) {
    int i;
    if (!s) return;
    free(s->name); free(s->title); free(s->ground);
    geo_unref(s->disc); geo_unref(s->cdisc); geo_unref(s->wash);
    ol_free(&s->ops); ol_free(&s->base_ops);
    for (i = 0; i < s->gcuts.n; i++) { gl_free(&s->gcuts.v[i].polys); kind_unref(s->gcuts.v[i].kind); }
    free(s->gcuts.v);
    for (i = 0; i < s->tcuts.n; i++) { gl_free(&s->tcuts.v[i].polys); kind_unref(s->tcuts.v[i].kind); }
    free(s->tcuts.v);
    for (i = 0; i < s->chops.n; i++) chop_unref(s->chops.v[i]);
    free(s->chops.v);
    for (i = 0; i < s->terr.n; i++) { geo_unref(s->terr.v[i].fp); geo_unref(s->terr.v[i].top); }
    free(s->terr.v);
    gl_free(&s->feet); gl_free(&s->objs); gl_free(&s->keepout); gl_free(&s->held);
    free(s);
}

/* [g for p in polys for g in polys_of(p.intersection(disc)) if g.area > 0.08] */
static int clip_to(const GeoList *polys, const GEOSGeometry *disc, GeoList *out) {
    int i, j;
    for (i = 0; i < polys->n; i++) {
        GeoList ps = { 0 }; GEOSGeometry *x = shp_intersection(polys->v[i]->g, disc);
        if (!x) return SC_ERR;
        lf_polys_of_new(x, &ps);
        for (j = 0; j < ps.n; j++) if (shp_area(ps.v[j]->g) > 0.08) gl_push(out, ps.v[j]);
        gl_free(&ps);
    }
    return 0;
}
/* gg.Scene.clip: s.disc. gg2.Scene.clip: s.kc = getattr(s, "kc", -1) + 1; Point(0, 0).buffer(R0 + 0.15 - 0.15 * (kc % 4), 256) */
int sc_clip(Scene *s, const GeoList *polys, GeoList *out) {
    static GEOSGeometry *d4[4]; static double d4r = -1;
    int m;
    if (!s->gg2) return clip_to(polys, s->disc->g, out);
    s->kc = (s->has_kc ? s->kc : -1) + 1; s->has_kc = 1;
    if (d4r != SC.R0) { for (m = 0; m < 4; m++) { shp_free(d4[m]); d4[m] = disc_at(SC.R0 + 0.15 - 0.15 * m); } d4r = SC.R0; }          /* the same disc each time: made once */
    m = (int)(s->kc % 4);
    if (!d4[m]) return SC_ERR;
    return clip_to(polys, d4[m], out);
}
int sc_cclip(Scene *s, const GeoList *polys, GeoList *out) { return clip_to(polys, s->cdisc->g, out); }

void (*sc_piece_hook)(char kind, const GeoList *polys);          /* live.hook_layout (scene.h) */

/* gg.Scene.gcut: s.gcuts.append((s.clip(polys), depth, kind)); gg2: s.cclip */
static void widen_cuts(GeoList *ps);
static int sc_gcut_(Scene *s, const GeoList *polys, Num depth, Kind *kind) {
    GeoList ps = { 0 }; int r = s->gg2 ? sc_cclip(s, polys, &ps) : sc_clip(s, polys, &ps);
    if (r >= 0 && kind && !(kd_wet_hook_fn && kd_wet_hook_fn(kind))) widen_cuts(&ps);          /* water, tar, lava: as they are */
    if (r >= 0) gcl_push(&s->gcuts, &ps, depth, kind);
    gl_free(&ps);
    return r;
}
/* gg.Scene.tcut / gg2.Scene.tcut: ps = [p for p in clip(polys) if not p.buffer(-0.1).is_empty]; if ps: append */
static int sc_tcut_(Scene *s, const GeoList *polys, Num z0, Num z1, Kind *kind) {
    GeoList cl = { 0 }, ps = { 0 }; int i, r = s->gg2 ? sc_cclip(s, polys, &cl) : sc_clip(s, polys, &cl);
    for (i = 0; r >= 0 && i < cl.n; i++) {
        GEOSGeometry *b = shp_buffer_q(cl.v[i]->g, -0.1, 16);
        if (!b) { r = SC_ERR; break; }
        if (!shp_is_empty(b)) gl_push(&ps, cl.v[i]);
        shp_free(b);
    }
    if (r >= 0 && ps.n) { widen_cuts(&ps); tcl_push(&s->tcuts, &ps, z0, z1, kind); }
    gl_free(&cl); gl_free(&ps);
    return r < 0 ? SC_ERR : 0;
}

int (*kd_wet_hook_fn)(const Kind *k);
double sc_uncovered(const GEOSGeometry *g, const GEOSGeometry *const *plates, int n) {
    GEOSGeometry *u = n ? shp_union_all(plates, n) : NULL, *d = u ? shp_difference(g, u) : shp_clone(g), *a, *b; double r;
    shp_free(u); if (!d) return 0.0;
    a = shp_buffer(d, -0.35, 8, 2, 2, 5.0, 0); b = a ? shp_buffer(a, 0.35, 8, 2, 2, 5.0, 0) : NULL;          /* a joint is gone after this */
    r = b ? shp_area(b) : 0.0; shp_free(a); shp_free(b); shp_free(d); return r;
}
double sc_min_part = 0.5, sc_min_cut = 0.3;          /* readable: no detail part thinner, no cut narrower (mm; 0 = off) */
static GEOSGeometry *mitre(const GEOSGeometry *g, double d) { return shp_buffer(g, d, 8, 2, 2, 5.0, 0); }          /* flat caps, mitred joins: corners stay sharp */
/* a detail piece with its parts thinner than sc_min_part trimmed off; the piece itself when that would take most of it */
static void push_readable(GeoList *out, Geo *p) {
    GEOSGeometry *a, *b; double a0, a1;
    if (!sc_feat || sc_min_part <= 0) { gl_push(out, p); return; }
    a = mitre(p->g, -sc_min_part / 2); b = a ? mitre(a, sc_min_part / 2) : NULL; shp_free(a);
    a0 = shp_area(p->g); a1 = b ? shp_area(b) : 0.0;
    if (b && a0 - a1 >= 0.02 && a1 < 0.6 * a0) {          /* thin all over (a spiky shrub, a root, a rope): thickened a little, then trimmed */
        GEOSGeometry *w = mitre(p->g, 0.15), *c = w ? mitre(w, -sc_min_part / 2) : NULL, *d = c ? mitre(c, sc_min_part / 2) : NULL; double aw = w ? shp_area(w) : 0.0;
        shp_free(c);
        if (d && shp_area(d) >= 0.6 * aw) {
            GeoList ps = { 0 }; int j; shp_free(b); shp_free(w);
            lf_polys_of_new(d, &ps); for (j = 0; j < ps.n; j++) if (shp_area(ps.v[j]->g) > 0.05) gl_push(out, ps.v[j]); gl_free(&ps); return;
        }
        shp_free(w); shp_free(d);
    }
    if (!b || a0 - a1 < 0.02 || a1 < 0.6 * a0) { shp_free(b); gl_push(out, p); return; }          /* nothing thin (or nothing to be done): as it was */
    { GEOSGeometry *c = shp_intersection(b, p->g); GeoList ps = { 0 }; int j; shp_free(b); if (!c) { gl_push(out, p); return; }
      lf_polys_of_new(c, &ps); for (j = 0; j < ps.n; j++) if (shp_area(ps.v[j]->g) > 0.05) gl_push(out, ps.v[j]); gl_free(&ps); }
}
/* a cut narrower than sc_min_cut, widened to it */
static void widen_cuts(GeoList *ps) {
    int i;
    if (sc_min_cut <= 0) return;
    for (i = 0; i < ps->n; i++) {
        GEOSGeometry *a = mitre(ps->v[i]->g, -sc_min_cut / 2), *w; double a0 = shp_area(ps->v[i]->g), a1 = a ? shp_area(a) : 0.0; shp_free(a);
        if (a1 > 0.3 * a0) continue;          /* wide enough */
        w = mitre(ps->v[i]->g, 0.1); if (!w) continue;          /* a 0.2 mm joint becomes 0.4 */
        geo_unref(ps->v[i]); ps->v[i] = geo_new(w);
    }
}
/* [p for p in s.clip(polys) if wide_enough(p, w)] */
static int clip_wide(Scene *s, const GeoList *polys, double w, GeoList *out) {
    GeoList cl = { 0 }; int i, r = sc_clip(s, polys, &cl);
    for (i = 0; r >= 0 && i < cl.n; i++) { int k = lf_wide_enough(cl.v[i], w); if (k < 0) r = SC_ERR; else if (k) push_readable(out, cl.v[i]); }
    gl_free(&cl);
    return r < 0 ? SC_ERR : 0;
}

/* gg.Scene.prism(polys, top, kind, z0=None, minw=MINW, foot=True) */
static int sc_prism_(Scene *s, const GeoList *polys, Num top, Kind *kind, Num z0, double minw, int foot, GeoList *out) {
    GeoList ps = { 0 }; Num z;
    if (clip_wide(s, polys, minw, &ps) < 0) { gl_free(&ps); return SC_ERR; }
    if (!ps.n) { gl_free(&ps); return 0; }
    z = z0.t == NUM_NONE ? NF(SC.H - SC.SINK) : z0;
    ol_push_own(&s->ops, op_prism(&ps, z, top, kind));
    if (foot && (z0.t == NUM_NONE || z0.v <= SC.H)) gl_extend(&s->feet, &ps);
    gl_extend(out, &ps); gl_free(&ps);
    return 0;
}

static double tan_deg(double a) { return py_tan(LF_RADIANS(a)); }          /* math.tan(math.radians(a)) */

/* [g for q in qs for g in polys_of(inset(q, d)) if g.area > amin] -> out */
static int inset_all(const GeoList *qs, double d, double amin, GeoList *out) {
    int i, j;
    for (i = 0; i < qs->n; i++) {
        GeoList ps = { 0 }; GEOSGeometry *x = shp_inset(qs->v[i]->g, d);
        if (!x) return SC_ERR;
        lf_polys_of_new(x, &ps);
        for (j = 0; j < ps.n; j++) if (shp_area(ps.v[j]->g) > amin) gl_push(out, ps.v[j]);
        gl_free(&ps);
    }
    return 0;
}
/* [q for q in (fit_for_taper(p, d) for p in profs) if q is not None] */
static int fit_all(const GeoList *profs, double d, GeoList *ok) {
    int i, err;
    for (i = 0; i < profs->n; i++) {
        Geo *q = lf_fit_for_taper(profs->v[i], d, &err);
        if (err) return SC_ERR;
        if (q) gl_push_own(ok, q);
    }
    return 0;
}

/* the tier loop shared by gg.Scene._tiers and gg2.tstack: first adds `first_add`, later tiers `lap`; z steps by hh - zstep */
static int tier_loop(GeoList *profs_io, Num *z, Num h, const Tier *tiers, int nt, Kind *kind, Num first_add, double lap, double ledge, double zstep, OpList *ops) {
    int t, first = 1;
    for (t = 0; t < nt; t++) {
        Num hh = num_add(num_mul(tiers[t].frac, h), first ? first_add : NF(lap));
        double d = hh.v * tan_deg(tiers[t].a.v) + 0.03;
        GeoList ok = { 0 }, np_ = { 0 };
        if (fit_all(profs_io, d, &ok) < 0) { gl_free(&ok); return SC_ERR; }
        if (!ok.n) { gl_free(&ok); break; }
        ol_push_own(ops, op_taper(&ok, num_round(*z, 4), num_round(hh, 4), tiers[t].a, kind));
        if (inset_all(&ok, hh.v * tan_deg(tiers[t].a.v) + ledge, 0.1, &np_) < 0) { gl_free(&ok); gl_free(&np_); return SC_ERR; }
        gl_free(profs_io); *profs_io = np_; gl_free(&ok);
        *z = num_add(*z, num_sub(hh, NF(zstep))); first = 0;
    }
    return 0;
}

/* gg.Scene._tiers(profs, z, h, tiers, kind) -> (ops, profs, z) (the legacy stacked tiers; the second definition) */
int sc_tiers(Scene *s, const GeoList *profs, Num z, Num h, const Tier *tiers, int nt, Kind *kind, OpList *ops_out, GeoList *tops, Num *zt) {
    GeoList pr; int r;
    (void)s;
    gl_copy(&pr, profs);
    r = tier_loop(&pr, &z, h, tiers, nt, kind, NF(SC.SINK), SC.LAP, SC.LEDGE, SC.LAP, ops_out);
    if (r >= 0) { gl_extend(tops, &pr); *zt = z; }
    gl_free(&pr);
    return r;
}

/* gg.Scene._solids(profs, z, top, hb, tiers, kind): one solid per piece; returns the top profiles */
int sc_solids(Scene *s, const GeoList *profs, Num z, Num top, Num hb, const Tier *tiers, int nt, Kind *kind, GeoList *tops) {
    Num hh = num_sub(top, z), a_eff, drop, run, amax;
    double *terms = (double *)malloc(sizeof(double) * (size_t)(nt + 1)), ins; int i;
    for (i = 0; i < nt; i++) terms[i] = num_mul(tiers[i].frac, hb).v * tan_deg(tiers[i].a.v);          /* fr * hb * math.tan(math.radians(a)) */
    if (hb.t == NUM_NP) { ins = 0.0; for (i = 0; i < nt; i++) ins = ins + terms[i]; }          /* sum() of numpy.float64 terms: plain additions */
    else ins = lf_fsum(terms, nt);          /* sum() of floats: Neumaier */
    free(terms);
    a_eff = hh.v > 0 ? NF(LF_DEGREES(py_atan(ins / hh.v))) : num_i(0);
    drop = nt > 1 ? num_min2(num_mul(tiers[nt - 1].frac, hb), num_mul(NF(0.6), hb)) : num_mul(NF(0.45), hb);
    if (nt == 0) return SC_ERR;          /* tiers[-1]: IndexError */
    amax = num_max2(tiers[nt - 1].a, num_i(30));
    run = num_mul(drop, NF(tan_deg(amax.v)));
    for (i = 0; i < profs->n; i++) {
        Geo *p = profs->v[i], *q; Op *single; GeoList one = { 0 }; int err = 0, concave; double pa, ha;
        GEOSGeometry *hl;
        if (a_eff.v > 0.5) { q = lf_fit_for_taper(p, hh.v * tan_deg(a_eff.v) + 0.03, &err); if (err) return SC_ERR; }
        else q = geo_ref(p);
        if (q) { gl_push(&one, q); single = op_taper(&one, num_round(z, 4), num_round(hh, 4), num_round(a_eff, 3), kind); }
        else { gl_push(&one, p); single = op_prism(&one, num_round(z, 4), num_round(top, 4), kind); }
        geo_unref(q); gl_free(&one);
        if (inset_all(&(GeoList){ &p, 1, 1 }, ins + 0.05, 0.1, tops) < 0) { op_unref(single); return SC_ERR; }
        hl = shp_convex_hull(p->g);
        if (!hl) { op_unref(single); return SC_ERR; }
        ha = shp_area(hl); shp_free(hl); pa = shp_area(p->g);
        concave = ha - pa > 0.04 * ha;
        if (SC.mesh && tiers[0].a.v >= 12 && pa >= 0.9 && !kind_is(kind, "grit") && !kind_is(kind, "pebble") && ins > 0.15) {
            /* the soft heap: m fine sloped bands, steep at the foot and easing to the crown */
            double inr = 2 * pa / shp_length(p->g), ins_ = ins, dd; int m, k; GeoList cur = { 0 }; Num zz = z;
            if (0.6 * inr > ins_) ins_ = 0.6 * inr;
            if (0.95 * inr < ins_) ins_ = 0.95 * inr;
            m = (int)(hh.v / 0.1); if (m > 12) m = 12; if (m < 5) m = 5;
            gl_push(&cur, p);
            for (k = 0; k < m; k++) {
                double u0 = (double)k / m, u1 = (double)(k + 1) / m; int pe = 0;
                double f1 = lf_fpow(1 - u1, 1.8, &pe), f0 = lf_fpow(1 - u0, 1.8, &pe);
                Num hk = num_mul(hh, NF((1 - f1) - (1 - f0))); GeoList nx = { 0 };
                dd = ins_ / m;
                if (pe) { gl_free(&cur); op_unref(single); return SC_ERR; }
                if (hk.v < 0.02 || !cur.n) break;
                ol_push_own(&s->ops, op_taper(&cur, num_round(zz, 4), num_round(hk, 4), num_round(NF(LF_DEGREES(py_atan(dd / hk.v))), 3), kind));
                zz = num_add(zz, hk);
                if (inset_all(&cur, dd, 0.3, &nx) < 0) { gl_free(&cur); gl_free(&nx); op_unref(single); return SC_ERR; }
                gl_free(&cur); cur = nx;
            }
            gl_free(&cur); op_unref(single);
            continue;
        }
        if (kind_is(kind, "grit") || kind_is(kind, "pebble") || pa < 1.2 || run.v < 0.04 || concave) { ol_push_own(&s->ops, single); continue; }
        {
            Chop *c = (Chop *)calloc(1, sizeof *c); Op *rk;
            c->rc = 1; c->kind = kind_ref(kind); c->poly = geo_ref(p); c->z0 = num_round(z, 4); c->top = num_round(top, 4);
            c->run = num_round(run, 4); c->drop = num_round(drop, 4); ol_push(&c->ops, single);
            chl_push_own(&s->chops, c);
            rk = single->type == OP_TAPER ? op_taper(&single->polys, single->n[0], single->n[1], single->n[2], kind_str("rockc"))
                                          : op_prism(&single->polys, single->n[0], single->n[1], kind_str("rockc"));          /* single[:-1] + ("rockc",) */
            ol_push_own(&s->ops, rk); op_unref(single);
        }
    }
    return 0;
}

/* gg.Scene.stack(polys, h, tiers, kind, z0=None) -> (top profiles, top z) */
static int sc_stack_(Scene *s, const GeoList *polys, Num h, const Tier *tiers, int nt, Kind *kind, Num z0, GeoList *tops, Num *top) {
    GeoList profs = { 0 }; Num z; int r;
    if (clip_wide(s, polys, SC.MINW, &profs) < 0) { gl_free(&profs); return SC_ERR; }
    if (!profs.n) { gl_free(&profs); *top = NF(SC.H); return 0; }
    if (z0.t == NUM_NONE) gl_extend(&s->feet, &profs);
    z = num_sub(z0.t == NUM_NONE ? NF(SC.H) : z0, NF(SC.SINK));
    if (SC.chamfer_rocks && SC.base_first) {
        *top = num_add(num_add(z, NF(SC.SINK)), h);
        r = sc_solids(s, &profs, z, *top, h, tiers, nt, kind, tops);
    } else {
        OpList ops = { 0 }; Num zt; int i;
        r = sc_tiers(s, &profs, z, h, tiers, nt, kind, &ops, tops, &zt);
        for (i = 0; r >= 0 && i < ops.n; i++) ol_push(&s->ops, ops.v[i]);
        ol_free(&ops);
        if (r >= 0) *top = num_add(zt, NF(0.03));
    }
    gl_free(&profs);
    return r;
}

static GEOSGeometry *disc_at0(void) { return shp_point_buffer(0, 0, SC.R0 - 0.4, 128); }          /* Point(0, 0).buffer(R0 - 0.4, 128) */
/* the scatter loop shared by gg.Scene.scatter and gg2.scatter_any (busy and the radius differ) */
static int scatter_loop(Scene *s, int n, double rmin, double rmax, Geo *region, double gap, int round_, int tries, GEOSGeometry *busy, double rad, GeoList *out) {
    GEOSGeometry *base = region ? NULL : disc_at0(), *reg; int it, cnt, r = 0;
    reg = shp_buffer_q(region ? region->g : base, -0.2, 16); shp_free(base);
    if (!reg) return SC_ERR;
    n = lf_area_count(n, region);
    cnt = tries ? tries : n * 60;
    for (it = 0; it < cnt; it++) {
        double rr, a, d; Geo *p; int ok, i;
        if (out->n >= n) break;
        rr = py_uniform(&s->rng, rmin, rmax); a = py_uniform(&s->rng, 0, 2 * PI); d = sqrt(py_random(&s->rng)) * rad;
        p = lf_stone(d * py_cos(a), d * py_sin(a), rr, &s->rng, 0.0, 0, round_);
        if (!p) { r = SC_ERR; break; }
        ok = shp_contains(reg, p->g);
        if (ok == 1) { int x = shp_intersects(p->g, busy); ok = x < 0 ? -1 : !x; }
        for (i = 0; ok == 1 && i < out->n; i++) { double dd = shp_distance(p->g, out->v[i]->g); if (!(dd > gap)) ok = 0; }
        if (ok < 0) { geo_unref(p); r = SC_ERR; break; }
        if (ok) gl_push_own(out, p); else geo_unref(p);
    }
    shp_free(reg);
    return r;
}
/* the busy area: unary_union(gs).buffer(gap) if gs else Polygon() */
static GEOSGeometry *busy_of(GEOSGeometry *const *gs, int n, double gap) {
    GEOSGeometry *u, *b;
    if (!n) return G.GEOSGeom_createEmptyPolygon_r(gctx);
    u = shp_union_all((const GEOSGeometry *const *)gs, n);
    if (!u) return NULL;
    b = shp_buffer_q(u, gap, 16); shp_free(u);
    return b;
}
static GEOSGeometry *busy_list(const GeoList *a, const GeoList *b, double gap) {
    int n = a->n + (b ? b->n : 0), i; GEOSGeometry **v = (GEOSGeometry **)malloc(sizeof(GEOSGeometry *) * (size_t)(n + 1)), *r;
    for (i = 0; i < a->n; i++) v[i] = a->v[i]->g;
    for (i = 0; b && i < b->n; i++) v[a->n + i] = b->v[i]->g;
    r = busy_of(v, n, gap); free(v);
    return r;
}
/* gg.Scene.scatter(n, rmin, rmax, region=None, gap=0.3, round_=True, tries=None) */
int sc_scatter(Scene *s, int n, double rmin, double rmax, Geo *region, double gap, int round_, int tries, GeoList *out) {
    GEOSGeometry *busy; int r;
    /* reg is made first in Python, then busy: neither draws random numbers, so the order does not show */
    busy = busy_list(&s->feet, NULL, gap);
    if (!busy) return SC_ERR;
    r = scatter_loop(s, n, rmin, rmax, region, gap, round_, tries, busy, SC.CLIP, out);
    shp_free(busy);
    return r;
}

/* gg.Scene.final_gcuts / gg2.Scene.final_gcuts */
int sc_final_gcuts(Scene *s, GcutList *out) {
    double gap = s->gg2 ? num_max2(s->has_cutgap ? s->cutgap : NF(0.05), NF(0.35)).v : (SC.base_first ? 0.35 : 0.05);
    GEOSGeometry *feet = busy_list(&s->feet, NULL, gap); int i, j;
    if (!feet) return SC_ERR;
    for (i = 0; i < s->gcuts.n; i++) {
        Gcut *c = &s->gcuts.v[i]; GEOSGeometry **v = (GEOSGeometry **)malloc(sizeof(GEOSGeometry *) * (size_t)(c->polys.n + 1)), *u, *g;
        GeoList pl = { 0 }, ps = { 0 };
        for (j = 0; j < c->polys.n; j++) v[j] = c->polys.v[j]->g;
        u = shp_union_all((const GEOSGeometry *const *)v, c->polys.n); free(v);
        if (!u) { shp_free(feet); return SC_ERR; }
        g = shp_difference(u, feet); shp_free(u);
        if (!g) { shp_free(feet); return SC_ERR; }
        lf_polys_of_new(g, &pl);
        for (j = 0; j < pl.n; j++) {
            GEOSGeometry *b, *sm;
            if (!(shp_area(pl.v[j]->g) > 0.1)) continue;
            b = shp_buffer_q(pl.v[j]->g, -0.1, 16);
            if (!b) { gl_free(&pl); gl_free(&ps); shp_free(feet); return SC_ERR; }
            if (!shp_is_empty(b)) {
                sm = shp_simplify(pl.v[j]->g, 0.01, 1);
                if (!sm) { shp_free(b); gl_free(&pl); gl_free(&ps); shp_free(feet); return SC_ERR; }
                gl_push_own(&ps, geo_new(sm));
            }
            shp_free(b);
        }
        if (ps.n) gcl_push(out, &ps, c->depth, c->kind);
        gl_free(&pl); gl_free(&ps);
    }
    shp_free(feet);
    return 0;
}

/* gg.Scene.top: max([prism tops] + [taper z + h] + [H]) - H */
Num sc_top(Scene *s) {
    Num m = num_none(); int i, first = 1;
    for (i = 0; i < s->ops.n; i++) if (s->ops.v[i]->type == OP_PRISM) { Num v = s->ops.v[i]->n[1]; if (first || v.v > m.v) m = v; first = 0; }
    for (i = 0; i < s->ops.n; i++) if (s->ops.v[i]->type == OP_TAPER) { Num v = num_add(s->ops.v[i]->n[0], s->ops.v[i]->n[1]); if (first || v.v > m.v) m = v; first = 0; }
    if (first || SC.H > m.v) m = NF(SC.H);
    return num_sub(m, NF(SC.H));
}

/* not lst or not p.buffer(gap).intersects(unary_union(lst)) */
static int free_of(const GeoList *lst, Geo *p, double gap) {
    GEOSGeometry **v, *u, *b; int i, r;
    if (!lst->n) return 1;
    b = shp_buffer_q(p->g, gap, 16);
    if (!b) return SC_ERR;
    v = (GEOSGeometry **)malloc(sizeof(GEOSGeometry *) * (size_t)lst->n);
    for (i = 0; i < lst->n; i++) v[i] = lst->v[i]->g;
    u = shp_union_all((const GEOSGeometry *const *)v, lst->n); free(v);
    if (!u) { shp_free(b); return SC_ERR; }
    r = shp_intersects(b, u); shp_free(b); shp_free(u);
    return r < 0 ? SC_ERR : !r;
}
int sc_free(Scene *s, Geo *p, double gap) { return free_of(&s->feet, p, gap); }          /* gg2.Scene.free */

/* ======================= gg2 terrain ======================= */
/* T_init: if not hasattr(s, "terr"): s.terr, s.objs, s.keepout = [], [], [] */
void T_init(Scene *s) {
    if (s->has_terr) return;
    s->has_terr = 1; s->terr.n = 0;
    gl_free(&s->objs); gl_free(&s->keepout); s->has_objs = 1;
}
void T_add(Scene *s, Geo *fp, Num zb, Num zt, Geo *top) { T_init(s); trl_push(&s->terr, fp, zb, zt, top); }

/* fp.intersects(p) and fp.intersection(p).area > 1e-3 */
static int overlaps_some(const GEOSGeometry *fp, const GEOSGeometry *p) {
    int x = shp_intersects(fp, p); GEOSGeometry *i; double a;
    if (x <= 0) return x;
    i = shp_intersection(fp, p);
    if (!i) return SC_ERR;
    a = shp_area(i); shp_free(i);
    return a > 1e-3;
}
/* T_zr(s, p) -> (round(lo, 3), round(hi, 3)) */
int T_zr(Scene *s, Geo *p, Num *lo, Num *hi) {
    Num l = NF(SC.H), h = NF(SC.H); int i;
    T_init(s);
    for (i = 0; i < s->terr.n; i++) {
        Terr *t = &s->terr.v[i]; int o = overlaps_some(t->fp->g, p->g), c;
        if (o < 0) return SC_ERR;
        if (o) h = num_max2(h, t->zt);
        c = shp_contains(t->top->g, p->g);
        if (c < 0) return SC_ERR;
        if (c) l = num_max2(l, t->zt);
        else { c = shp_contains(t->fp->g, p->g); if (c < 0) return SC_ERR; if (c) l = num_max2(l, t->zb); }
    }
    *lo = num_round(l, 3); *hi = num_round(h, 3);
    return 0;
}
/* T_flat(s, p): H, the top it stands on, or None */
int T_flat(Scene *s, Geo *p, Num *z) {
    Num hi = NF(SC.H); GeoList own = { 0 }; int i, r = 0, any = 0;
    T_init(s);
    *z = num_none();
    for (i = 0; i < s->keepout.n; i++) { int x = shp_intersects(s->keepout.v[i]->g, p->g); if (x < 0) return SC_ERR; if (x) return 0; }
    for (i = 0; i < s->terr.n; i++) {
        Terr *t = &s->terr.v[i]; int o = overlaps_some(t->fp->g, p->g);
        if (o < 0) { gl_free(&own); return SC_ERR; }
        if (o) {
            if (t->zt.v > hi.v + 1e-6) { hi = t->zt; gl_clear(&own); gl_push(&own, t->top); }
            else if (fabs(t->zt.v - hi.v) < 1e-6) gl_push(&own, t->top);
        }
    }
    if (hi.v == SC.H) { gl_free(&own); *z = NF(SC.H); return 0; }
    for (i = 0; i < own.n; i++) { int c = shp_contains(own.v[i]->g, p->g); if (c < 0) { r = SC_ERR; break; } if (c) { any = 1; break; } }
    gl_free(&own);
    if (r < 0) return r;
    if (any) *z = hi;
    return 0;
}
int T_free(Scene *s, Geo *p, double gap) { T_init(s); return free_of(&s->objs, p, gap); }

/* T_group: {(lo, hi): [p...]} in insertion order (keys equal as Python compares floats) */
int T_group(Scene *s, const GeoList *polys, TGroup **groups, int *ng) {
    int i, j, cap = 0; TGroup *g = NULL; *ng = 0;
    for (i = 0; i < polys->n; i++) {
        Num lo, hi;
        if (T_zr(s, polys->v[i], &lo, &hi) < 0) { *groups = g; return SC_ERR; }
        for (j = 0; j < *ng; j++) if (g[j].lo.v == lo.v && g[j].hi.v == hi.v) break;
        if (j == *ng) { grow((void **)&g, &cap, *ng, sizeof(TGroup)); memset(&g[j], 0, sizeof(TGroup)); g[j].lo = lo; g[j].hi = hi; (*ng)++; }
        gl_push(&g[j].ps, polys->v[i]);
    }
    *groups = g;
    return 0;
}
void tgroups_free(TGroup *g, int ng) { int i; for (i = 0; i < ng; i++) gl_free(&g[i].ps); free(g); }

/* tprism(s, polys, h, kind) -> [(g, top)] */
int tprism(Scene *s, const GeoList *polys, Num h, Kind *kind, TPiece **out, int *nout) {
    TGroup *g; int ng, i, j, cap = 0, r;
    *out = NULL; *nout = 0;
    r = T_group(s, polys, &g, &ng);
    for (i = 0; r >= 0 && i < ng; i++) {
        Num top = num_min2(num_add(g[i].hi, h), NF(SC.CAP)); GeoList got = { 0 };
        if (num_sub(top, g[i].hi).v < 0.25) continue;
        r = sc_prism(s, &g[i].ps, top, kind, num_sub(g[i].lo, NF(SC.SINK)), SC.MINW, 0, &got);
        if (r >= 0) {
            gl_extend(&s->feet, &got); gl_extend(&s->objs, &got);
            for (j = 0; j < got.n; j++) { grow((void **)out, &cap, *nout, sizeof(TPiece)); (*out)[*nout].g = geo_ref(got.v[j]); (*out)[*nout].top = top; (*nout)++; }
        }
        gl_free(&got);
    }
    tgroups_free(g, ng);
    return r;
}

/* tstack(s, polys, h, tiers, kind) */
int tstack(Scene *s, const GeoList *polys, Num h, const Tier *tiers, int nt, Kind *kind) {
    TGroup *g; int ng, i, r;
    r = T_group(s, polys, &g, &ng);
    for (i = 0; r >= 0 && i < ng; i++) {
        Num hh_tot = num_min2(h, num_sub(num_sub(NF(SC.CAP), g[i].hi), NF(0.05))), z; GeoList profs = { 0 };
        if (hh_tot.v < 0.25) continue;
        r = clip_wide(s, &g[i].ps, SC.MINW, &profs);
        if (r < 0 || !profs.n) { gl_free(&profs); continue; }
        gl_extend(&s->feet, &profs); gl_extend(&s->objs, &profs);
        z = num_sub(g[i].lo, NF(SC.SINK));
        if (SC.chamfer_rocks && SC.base_first) {
            GeoList tops = { 0 };
            r = sc_solids(s, &profs, z, num_add(g[i].hi, hh_tot), hh_tot, tiers, nt, kind, &tops);
            gl_free(&tops); gl_free(&profs);
            continue;
        }
        r = tier_loop(&profs, &z, hh_tot, tiers, nt, kind, num_add(num_sub(g[i].hi, g[i].lo), NF(SC.SINK)), 0.03, 0.05, 0.03, &s->ops);
        gl_free(&profs);
    }
    tgroups_free(g, ng);
    return r;
}

/* tcut_flat(s, polys, depth, kind="pit") */
int tcut_flat(Scene *s, const GeoList *polys, Num depth, Kind *kind) {
    GEOSGeometry *block; int i, j, ng = 0, cap = 0, r = 0; struct { Num z; GeoList ps; } *g = NULL;
    T_init(s);
    block = busy_list(&s->objs, NULL, 0.3);
    if (!block) return SC_ERR;
    for (i = 0; i < polys->n; i++) {
        Num z; int x = shp_intersects(polys->v[i]->g, block);
        if (x < 0) { r = SC_ERR; break; }
        if (x) continue;
        if (T_flat(s, polys->v[i], &z) < 0) { r = SC_ERR; break; }
        if (z.t == NUM_NONE) continue;
        for (j = 0; j < ng; j++) if (g[j].z.v == z.v) break;
        if (j == ng) { grow((void **)&g, &cap, ng, sizeof *g); memset(&g[j], 0, sizeof *g); g[j].z = z; ng++; }
        gl_push(&g[j].ps, polys->v[i]);
    }
    shp_free(block);
    for (j = 0; r >= 0 && j < ng; j++) r = sc_tcut(s, &g[j].ps, num_sub(g[j].z, depth), num_add(g[j].z, NF(0.3)), kind);
    for (j = 0; j < ng; j++) gl_free(&g[j].ps);
    free(g);
    return r;
}

/* scatter_any(s, n, rmin, rmax, region=None, gap=0.3, round_=True, tries=None): avoids features, not terrain */
int scatter_any(Scene *s, int n, double rmin, double rmax, Geo *region, double gap, int round_, int tries, GeoList *out) {
    GEOSGeometry *busy; int r;
    T_init(s);
    busy = busy_list(&s->objs, &s->keepout, gap);
    if (!busy) return SC_ERR;
    r = scatter_loop(s, n, rmin, rmax, region, gap, round_, tries, busy, SC.R0, out);
    shp_free(busy);
    return r;
}

/* ======================= the dump ======================= */
struct DumpCtx { Buf *out; const void **gk; int ng, gcap; const void **kk; int nk, kcap; GEOSWKBWriter *w; };
static void bput(Buf *b, const void *p, size_t n) {
    if (b->n + n > b->cap) { b->cap = (b->n + n) * 2 + 4096; b->b = (unsigned char *)realloc(b->b, b->cap); }
    memcpy(b->b + b->n, p, n); b->n += n;
}
static void put1(DumpCtx *d, int c) { unsigned char x = (unsigned char)c; bput(d->out, &x, 1); }
static void put4(DumpCtx *d, int32_t v) { bput(d->out, &v, 4); }
static void putstr(DumpCtx *d, const char *s) { int32_t n = (int32_t)strlen(s); put4(d, n); bput(d->out, s, (size_t)n); }
static int table(const void ***t, int *n, int *cap, const void *p) {          /* index of p, or -1 after adding it */
    int i; for (i = 0; i < *n; i++) if ((*t)[i] == p) return i;
    grow((void **)t, cap, *n, sizeof(void *)); (*t)[(*n)++] = p; return -1;
}
DumpCtx *dump_begin(Buf *out) {
    DumpCtx *d = (DumpCtx *)calloc(1, sizeof *d); d->out = out;
    d->w = G.GEOSWKBWriter_create_r(gctx);
    G.GEOSWKBWriter_setOutputDimension_r(gctx, d->w, 4); G.GEOSWKBWriter_setByteOrder_r(gctx, d->w, 1);
    G.GEOSWKBWriter_setFlavor_r(gctx, d->w, 1); G.GEOSWKBWriter_setIncludeSRID_r(gctx, d->w, 0);
    return d;
}
void dump_end(DumpCtx *d) { G.GEOSWKBWriter_destroy_r(gctx, d->w); free(d->gk); free(d->kk); free(d); }
void dump_none(DumpCtx *d) { put1(d, 'N'); }
void dump_raw(DumpCtx *d, int tag, int count) { put1(d, tag); put4(d, count); }
void dump_num(DumpCtx *d, Num n) {
    if (n.t == NUM_NONE) { dump_none(d); return; }
    put1(d, 'n'); put1(d, n.t); bput(d->out, &n.v, 8);
    if (n.t == NUM_Z) put1(d, n.bridge);
}
void dump_geo(DumpCtx *d, const Geo *g) {
    int i; size_t n; unsigned char *w;
    if (!g) { dump_none(d); return; }
    put1(d, 'g'); i = table(&d->gk, &d->ng, &d->gcap, g);
    put4(d, i < 0 ? d->ng - 1 : i);
    if (i >= 0) return;
    w = G.GEOSWKBWriter_write_r(gctx, d->w, g->g, &n);
    put4(d, (int32_t)n); bput(d->out, w, n); G.GEOSFree_r(gctx, w);
}
void dump_geolist(DumpCtx *d, const GeoList *l) { int i; dump_raw(d, 'l', l->n); for (i = 0; i < l->n; i++) dump_geo(d, l->v[i]); }
static void dump_str(DumpCtx *d, const char *s) { put1(d, 's'); putstr(d, s); }
void dump_kind(DumpCtx *d, const Kind *k) {
    int i;
    if (!k->lk) { dump_str(d, k->s); return; }
    put1(d, 'k'); i = table(&d->kk, &d->nk, &d->kcap, k);
    put4(d, i < 0 ? d->nk - 1 : i);
    if (i >= 0) return;
    putstr(d, k->s);
    if (k->has_loft) { dump_raw(d, 't', 2); dump_num(d, k->loft_chain); dump_num(d, k->loft_z); } else dump_none(d);
    if (k->has_ghost) dump_num(d, k->ghost); else dump_none(d);
}
void dump_op(DumpCtx *d, const Op *o) {
    int i, nn = o->type == OP_PRISM ? 2 : 3;
    dump_raw(d, 't', 3 + nn); dump_str(d, o->type == OP_PRISM ? "prism" : "taper"); dump_geolist(d, &o->polys);
    for (i = 0; i < nn; i++) dump_num(d, o->n[i]);
    dump_kind(d, o->kind);
}
static void key(DumpCtx *d, const char *k) { putstr(d, k); }
static void dump_ops(DumpCtx *d, const OpList *l) { int i; dump_raw(d, 'l', l->n); for (i = 0; i < l->n; i++) dump_op(d, l->v[i]); }
void dump_scene(DumpCtx *d, const Scene *s) {
    /* the keys present, in this order (sstate.py writes the same) */
    int i, j, has_ok = s->has_objs, nk = 5 + (s->cdisc != NULL) + s->has_kc + 5 + s->has_terr + 2 * has_ok + s->has_held + s->has_cutgap + s->has_phi + (s->wash != NULL);
    dump_raw(d, 'd', nk);
    key(d, "name"); dump_str(d, s->name); key(d, "title"); dump_str(d, s->title); key(d, "ground"); dump_str(d, s->ground);
    key(d, "rng"); put1(d, 'r'); bput(d->out, s->rng.mt, 624 * 4); put4(d, s->rng.index);
    key(d, "disc"); dump_geo(d, s->disc);
    if (s->cdisc) { key(d, "cdisc"); dump_geo(d, s->cdisc); }
    if (s->has_kc) { key(d, "kc"); dump_num(d, num_i((double)s->kc)); }
    key(d, "ops"); dump_ops(d, &s->ops);
    key(d, "gcuts"); dump_raw(d, 'l', s->gcuts.n);
    for (i = 0; i < s->gcuts.n; i++) { dump_raw(d, 't', 3); dump_geolist(d, &s->gcuts.v[i].polys); dump_num(d, s->gcuts.v[i].depth); dump_kind(d, s->gcuts.v[i].kind); }
    key(d, "tcuts"); dump_raw(d, 'l', s->tcuts.n);
    for (i = 0; i < s->tcuts.n; i++) { dump_raw(d, 't', 4); dump_geolist(d, &s->tcuts.v[i].polys); dump_num(d, s->tcuts.v[i].z0); dump_num(d, s->tcuts.v[i].z1); dump_kind(d, s->tcuts.v[i].kind); }
    key(d, "feet"); dump_geolist(d, &s->feet);
    key(d, "chops"); dump_raw(d, 'l', s->chops.n);
    for (i = 0; i < s->chops.n; i++) {
        const Chop *c = s->chops.v[i];
        dump_raw(d, 'd', 7 + c->has_iso);
        key(d, "kind"); dump_kind(d, c->kind); key(d, "poly"); dump_geo(d, c->poly);
        key(d, "z0"); dump_num(d, c->z0); key(d, "top"); dump_num(d, c->top); key(d, "run"); dump_num(d, c->run); key(d, "drop"); dump_num(d, c->drop);
        key(d, "ops"); dump_ops(d, &c->ops);
        if (c->has_iso) { key(d, "iso"); dump_num(d, c->iso); }
    }
    if (s->has_terr) {
        key(d, "terr"); dump_raw(d, 'l', s->terr.n);
        for (i = 0; i < s->terr.n; i++) { dump_raw(d, 't', 4); dump_geo(d, s->terr.v[i].fp); dump_num(d, s->terr.v[i].zb); dump_num(d, s->terr.v[i].zt); dump_geo(d, s->terr.v[i].top); }
    }
    if (has_ok) { key(d, "objs"); dump_geolist(d, &s->objs); key(d, "keepout"); dump_geolist(d, &s->keepout); }
    /* s.base_ops is not dumped: it holds id()s, which Python reuses for later ops (and nothing reads it) */
    if (s->has_held) { key(d, "held"); dump_geolist(d, &s->held); }
    if (s->has_cutgap) { key(d, "cutgap"); dump_num(d, s->cutgap); }
    if (s->has_phi) { key(d, "phi"); dump_num(d, s->phi); }
    if (s->wash) { key(d, "wash"); dump_geo(d, s->wash); }
}
int sc_gcut(Scene *s, const GeoList *polys, Num depth, Kind *kind) { int r = sc_gcut_(s, polys, depth, kind); if (r >= 0 && sc_piece_hook) sc_piece_hook('c', polys); return r; }
int sc_tcut(Scene *s, const GeoList *polys, Num z0, Num z1, Kind *kind) { int r = sc_tcut_(s, polys, z0, z1, kind); if (r >= 0 && sc_piece_hook) sc_piece_hook('c', polys); return r; }
int sc_prism(Scene *s, const GeoList *polys, Num top, Kind *kind, Num z0, double minw, int foot, GeoList *out) {
    extern int cp_edge_wear; extern int wear_kind(const Kind *k); extern void wear_polys(GeoList *ps);
    if (cp_edge_wear && wear_kind(kind)) {          /* plan B2 (2026-11-22): worn, chipped edges on stone slabs (place.c) */
        GeoList w = { 0 }; int r; for (int i = 0; i < polys->n; i++) if (polys->v[i]) gl_push(&w, polys->v[i]);
        wear_polys(&w); r = sc_prism_(s, &w, top, kind, z0, minw, foot, out); if (r >= 0 && sc_piece_hook) sc_piece_hook('r', &w); gl_free(&w); return r;
    }
    { int r = sc_prism_(s, polys, top, kind, z0, minw, foot, out); if (r >= 0 && sc_piece_hook) sc_piece_hook('r', polys); return r; }
}
int sc_stack(Scene *s, const GeoList *polys, Num h, const Tier *tiers, int nt, Kind *kind, Num z0, GeoList *tops, Num *top) { int r = sc_stack_(s, polys, h, tiers, nt, kind, z0, tops, top); if (r >= 0 && sc_piece_hook) sc_piece_hook('r', polys); return r; }
