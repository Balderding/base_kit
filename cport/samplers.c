/* samplers.c: survey group G8: the samplers and the removal helpers of scenes.py (see samplers.h). Each function
   names the Python routine it copies; the Python text is quoted where the order of operations matters. */
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include "samplers.h"
#include "shp.h"
#include "pyx.h"

#define PI 3.141592653589793

/* ======================= the kind sets ======================= */
SmCfg SM;
SmStrSet SM_BRIDGE0, SM_TERRK0, SM_SOFT0, SM_PIECES0, SM_CHAMFER0, SM_DEBRIS0, SM_GROWTH0, SM_CUTK0;
int sm_set_has(const SmStrSet *s, const char *k) { int i; if (!s || !k) return 0; for (i = 0; i < s->n; i++) if (strcmp(s->v[i], k) == 0) return 1; return 0; }
void sm_set_add(SmStrSet *s, const char *k) {
    if (sm_set_has(s, k)) return;
    if (s->n >= s->cap) { s->cap = s->cap ? 2 * s->cap : 16; s->v = (const char **)realloc((void *)s->v, sizeof(char *) * (size_t)s->cap); }
    s->v[s->n++] = _strdup(k);
}
static void fill(SmStrSet *s, const char *words) {          /* words separated by spaces */
    char buf[64]; const char *p = words;
    while (*p) {
        int n = 0;
        while (*p == ' ') p++;
        while (*p && *p != ' ' && n < 63) buf[n++] = *p++;
        buf[n] = 0; if (n) sm_set_add(s, buf);
    }
}
static const char DEBRIS[] = "rock pebble sandstone slate basalt obsidian shelf garden rubble ice quartz brick concrete flag tcolumn crystal salt snow ember glass spire";
static const char GROWTH[] = "shrub grass flower fern moss heather reed shroom leaf weed agave flesh pustule egg tendril vein spore root lily stump mud earth tooth spine carapace shell";
static const char CUTK[] = "crack pit water rut scorch lava bile acid pothole slot glyph coolant seam pan wash tar hollow";
void sm_config_default(void) {
    static int done;
    if (!done) {
        done = 1;
        fill(&SM_BRIDGE0, "plate tflag basalt alien ice duck plank tatami flag fleshfloor board shelf");          /* gg3.BRIDGE */
        fill(&SM_TERRK0, "dune bedrock");                                                                         /* scenes.TERRK */
        fill(&SM_SOFT0, "grit");                                                                                  /* gg3.SOFT */
        fill(&SM_PIECES0, "rock pebble grit rubble crate pustule obelisk flower spire quartz lens post");          /* gg.PIECES */
        fill(&SM_CHAMFER0, "slate bedrock rock brick concrete sandstone rubble");                                 /* gg.CHAMFER_PRISMS */
        fill(&SM_DEBRIS0, DEBRIS); fill(&SM_GROWTH0, GROWTH); fill(&SM_CUTK0, CUTK);                              /* scenes.DEBRIS / GROWTH / CUTK */
    }
    SM.bridge = &SM_BRIDGE0; SM.terrk = &SM_TERRK0; SM.soft = &SM_SOFT0; SM.pieces = &SM_PIECES0; SM.chamfer = &SM_CHAMFER0;
    SM.debris = &SM_DEBRIS0; SM.growth = &SM_GROWTH0; SM.cutk = &SM_CUTK0;
    SM.col_setdefault = NULL; SM.col_ctx = NULL;
}
static int in_set(const SmStrSet *s, const Kind *k) { return k && sm_set_has(s, k->s); }
static int is_bridge(const Kind *k) { return in_set(SM.bridge, k); }
static int is_terrk(const Kind *k) { return in_set(SM.terrk, k); }
static int is_soft(const Kind *k) { return in_set(SM.soft, k); }

/* ======================= small list helpers ======================= */
static void tcl_free_(TcutList *l) { int i; for (i = 0; i < l->n; i++) { gl_free(&l->v[i].polys); kind_unref(l->v[i].kind); } free(l->v); l->v = NULL; l->n = l->cap = 0; }
static void gcl_free_(GcutList *l) { int i; for (i = 0; i < l->n; i++) { gl_free(&l->v[i].polys); kind_unref(l->v[i].kind); } free(l->v); l->v = NULL; l->n = l->cap = 0; }
static void trl_free_(TerrList *l) { int i; for (i = 0; i < l->n; i++) { geo_unref(l->v[i].fp); geo_unref(l->v[i].top); } free(l->v); l->v = NULL; l->n = l->cap = 0; }
static void chop_unref_(Chop *c) { if (c && --c->rc == 0) { kind_unref(c->kind); geo_unref(c->poly); ol_free(&c->ops); free(c); } }
static Op *op_with(const Op *o, const GeoList *ps) {          /* (op[0], ps) + tuple(op[2:]) */
    return o->type == OP_PRISM ? op_prism(ps, o->n[0], o->n[1], o->kind) : op_taper(ps, o->n[0], o->n[1], o->n[2], o->kind);
}
static int is_empty(const Geo *g) { return shp_is_empty(g->g); }
static double area(const Geo *g) { return shp_area(g->g); }

/* ======================= id sets ======================= */
static uint64_t hp(const void *p) { uint64_t x = (uint64_t)(uintptr_t)p; x ^= x >> 33; x *= 0xff51afd7ed558ccdULL; x ^= x >> 33; return x; }
void sm_ids_add(SmIdSet *s, const void *p) {
    int64_t i;
    if (!p) return;
    if (!s->t || 2 * (s->n + 1) > s->mask + 1) {
        int64_t nm = s->t ? 2 * (s->mask + 1) - 1 : 63, j; const void **nt = (const void **)calloc((size_t)(nm + 1), sizeof(void *));
        for (j = 0; s->t && j <= s->mask; j++) if (s->t[j]) { int64_t k = (int64_t)(hp(s->t[j]) & (uint64_t)nm); while (nt[k]) k = (k + 1) & nm; nt[k] = s->t[j]; }
        free((void *)s->t); s->t = nt; s->mask = nm;
    }
    i = (int64_t)(hp(p) & (uint64_t)s->mask);
    while (s->t[i]) { if (s->t[i] == p) return; i = (i + 1) & s->mask; }
    s->t[i] = p; s->n++;
}
int sm_ids_has(const SmIdSet *s, const void *p) {
    int64_t i;
    if (!s->t || !p) return 0;
    i = (int64_t)(hp(p) & (uint64_t)s->mask);
    while (s->t[i]) { if (s->t[i] == p) return 1; i = (i + 1) & s->mask; }
    return 0;
}
void sm_ids_free(SmIdSet *s) { free((void *)s->t); memset(s, 0, sizeof *s); }

/* _cells(s): {id(e[0]) for e in getattr(s, "terr", [])} */
void sm_cells(const Scene *s, SmIdSet *out) { int i; if (s->has_terr) for (i = 0; i < s->terr.n; i++) sm_ids_add(out, s->terr.v[i].fp); }

/* ======================= samplers ======================= */
struct SmAt {
    int rc, kind;
    GEOSGeometry **rings; double *w; int nr; int side; double c[2], front[2];          /* s_near */
    double d0, d1;                          /* s_rim */
    Geo *g; double b[4];                    /* s_in (g), s_far / s_not (g = avoid) */
    double R, d;                            /* s_far (R, d), s_not (d), sm_ring (R) */
    SmAt **fs; double *ws; int nf;          /* s_mix */
    SmAt *f;                                /* s_not */
    double x, y, lim;                       /* sm_jit */
    SmAtFn fn; void *ctx;                   /* sm_fn */
};
static SmAt *at_new(int kind) { SmAt *f = (SmAt *)calloc(1, sizeof *f); f->rc = 1; f->kind = kind; return f; }
SmAt *sm_at_ref(SmAt *f) { if (f) f->rc++; return f; }
void sm_at_unref(SmAt *f) {
    int i;
    if (!f || --f->rc) return;
    for (i = 0; i < f->nr; i++) shp_free(f->rings[i]);
    free(f->rings); free(f->w);
    geo_unref(f->g);
    for (i = 0; i < f->nf; i++) sm_at_unref(f->fs[i]);
    free(f->fs); free(f->ws);
    sm_at_unref(f->f);
    free(f);
}
int sm_at_kind(const SmAt *f) { return f->kind; }

/* s_near(g, d0, d1, side=None, c=None, front=None, n=4):
       if g is None or g.is_empty: return None
       rings = [p.exterior for d in np.linspace(d0, d1, n) for p in polys_of(g.buffer(d, 6))]
       if not rings: return None
       w = [r.length for r in rings] */
int sm_near(Geo *g, double d0, double d1, int side, const double *c, const double *front, int n, SmAt **out) {
    double *ds; int i, j, e; ShpList ps = { 0 }; SmAt *f;
    *out = NULL;
    if (!g) return 0;
    if ((e = is_empty(g)) < 0) return SC_ERR;
    if (e) return 0;
    if (n < 0) return SC_ERR;          /* linspace: Number of samples must be non-negative */
    ds = (double *)malloc(sizeof(double) * (size_t)(n ? n : 1));
    np_linspace(d0, d1, n, 1, ds);
    f = at_new(SMK_NEAR);
    for (i = 0; i < n; i++) {
        GEOSGeometry *b = shp_buffer_q(g->g, ds[i], 6);
        if (!b) { free(ds); sm_at_unref(f); return SC_ERR; }
        ps.n = 0; shp_polys_of(b, &ps);
        for (j = 0; j < ps.n; j++) {
            f->rings = (GEOSGeometry **)realloc(f->rings, sizeof(GEOSGeometry *) * (size_t)(f->nr + 1));
            f->rings[f->nr++] = shp_clone(shp_exterior(ps.g[j]));
            shp_free(ps.g[j]);
        }
        shp_free(b);
    }
    free(ds); free(ps.g);
    if (!f->nr) { sm_at_unref(f); return 0; }
    f->w = (double *)malloc(sizeof(double) * (size_t)f->nr);
    for (i = 0; i < f->nr; i++) f->w[i] = shp_length(f->rings[i]);
    f->side = side;
    if (side) { f->c[0] = c[0]; f->c[1] = c[1]; f->front[0] = front[0]; f->front[1] = front[1]; }
    *out = f;
    return 0;
}
/* def f(rng):
       for _ in range(16):
           r = rng.choices(rings, w)[0]; pt = r.interpolate(rng.random() * r.length)
           if side and ((pt.x - c[0]) * front[0] + (pt.y - c[1]) * front[1] > 0) != (side == "front"): continue
           break
       return pt.x, pt.y */
static int near_call(SmAt *f, PyRandom *rng, double *x, double *y) {
    int k; int64_t i;
    for (k = 0; k < 16; k++) {
        GEOSGeometry *pt; double u;
        if (py_choices_w(rng, f->nr, f->w, 1, &i) != PX_OK) return SC_ERR;
        u = py_random(rng) * shp_length(f->rings[i]);
        pt = shp_interpolate(f->rings[i], u, 0);
        if (!pt) return SC_ERR;
        *x = shp_x(pt); *y = shp_y(pt); shp_free(pt);
        if (f->side && (((*x - f->c[0]) * f->front[0] + (*y - f->c[1]) * f->front[1] > 0) != (f->side == SM_FRONT))) continue;
        break;
    }
    return 0;
}
/* s_rim(d0, d1): lambda rng: (lambda a, r: (r * math.cos(a), r * math.sin(a)))(rng.uniform(0, 6.2832), gg.R0 - rng.uniform(d0, d1)) */
SmAt *sm_rim(double d0, double d1) { SmAt *f = at_new(SMK_RIM); f->d0 = d0; f->d1 = d1; return f; }
static int rim_call(SmAt *f, PyRandom *rng, double *x, double *y) {
    double a = py_uniform(rng, 0, 6.2832), r = SC.R0 - py_uniform(rng, f->d0, f->d1);
    *x = r * py_cos(a); *y = r * py_sin(a); return 0;
}
/* s_in(g): x0, y0, x1, y1 = g.bounds
       for _ in range(30):
           x, y = rng.uniform(x0, x1), rng.uniform(y0, y1)
           if g.contains(Point(x, y)): break
       return x, y */
int sm_in(Geo *g, SmAt **out) {
    SmAt *f;
    *out = NULL;
    if (!g) return SC_ERR;          /* None.bounds */
    f = at_new(SMK_IN); f->g = geo_ref(g); shp_bounds(g->g, f->b);
    *out = f; return 0;
}
static int in_call(SmAt *f, PyRandom *rng, double *x, double *y) {
    int k;
    for (k = 0; k < 30; k++) {
        GEOSGeometry *pt; int c;
        *x = py_uniform(rng, f->b[0], f->b[2]); *y = py_uniform(rng, f->b[1], f->b[3]);
        pt = shp_point(*x, *y); c = pt ? shp_contains(f->g->g, pt) : -1; shp_free(pt);
        if (c < 0) return SC_ERR;
        if (c) break;
    }
    return 0;
}
/* s_far(avoid, d): R = gg.R0 - 2.0
       for _ in range(30):
           a = rng.uniform(0, 6.2832); r = R * math.sqrt(rng.random()); x, y = r * math.cos(a), r * math.sin(a)
           if avoid.is_empty or avoid.distance(Point(x, y)) > d: break
       return x, y */
SmAt *sm_far(Geo *avoid, double d) { SmAt *f = at_new(SMK_FAR); f->g = geo_ref(avoid); f->R = SC.R0 - 2.0; f->d = d; return f; }
static int far_call(SmAt *f, PyRandom *rng, double *x, double *y) {
    int k;
    for (k = 0; k < 30; k++) {
        double a = py_uniform(rng, 0, 6.2832), r = f->R * sqrt(py_random(rng)); int e;
        *x = r * py_cos(a); *y = r * py_sin(a);
        if (!f->g) return SC_ERR;          /* None.is_empty */
        if ((e = is_empty(f->g)) < 0) return SC_ERR;
        if (e) break;
        { GEOSGeometry *pt = shp_point(*x, *y); double dd = shp_distance(f->g->g, pt); shp_free(pt); if (dd > f->d) break; }
    }
    return 0;
}
/* s_mix(ws): ws = [(w, f) for w, f in ws if f is not None and w > 0]; if not ws: return None
       return lambda rng: rng.choices([f for w, f in ws], [w for w, f in ws])[0](rng) */
SmAt *sm_mix(const double *w, SmAt *const *fs, int n) {
    SmAt *f; int i;
    for (i = 0; i < n; i++) if (fs[i] && w[i] > 0) break;
    if (i == n) return NULL;
    f = at_new(SMK_MIX);
    f->fs = (SmAt **)malloc(sizeof(SmAt *) * (size_t)n); f->ws = (double *)malloc(sizeof(double) * (size_t)n);
    for (i = 0; i < n; i++) if (fs[i] && w[i] > 0) { f->fs[f->nf] = sm_at_ref(fs[i]); f->ws[f->nf++] = w[i]; }
    return f;
}
static int mix_call(SmAt *f, PyRandom *rng, double *x, double *y) {
    int64_t i;
    if (py_choices_w(rng, f->nf, f->ws, 1, &i) != PX_OK) return SC_ERR;
    return sm_at_call(f->fs[i], rng, x, y);
}
/* s_not(f, avoid, d): if f is None or avoid is None or avoid.is_empty: return f
       def g(rng):
           for _ in range(12):
               x, y = f(rng)
               if avoid.distance(Point(x, y)) > d: break
           return x, y */
SmAt *sm_not(SmAt *f, Geo *avoid, double d) {
    SmAt *g;
    if (!f || !avoid || is_empty(avoid) != 0) return sm_at_ref(f);          /* an is_empty error is not met here */
    g = at_new(SMK_NOT); g->f = sm_at_ref(f); g->g = geo_ref(avoid); g->d = d;
    return g;
}
static int not_call(SmAt *f, PyRandom *rng, double *x, double *y) {
    int k;
    for (k = 0; k < 12; k++) {
        GEOSGeometry *pt; double dd;
        if (sm_at_call(f->f, rng, x, y) < 0) return SC_ERR;
        pt = shp_point(*x, *y); dd = shp_distance(f->g->g, pt); shp_free(pt);
        if (dd > f->d) break;
    }
    return 0;
}
/* no_bare: at = lambda rng, x=x, y=y: (x + rng.uniform(-1, 1) * lim * 0.35, y + rng.uniform(-1, 1) * lim * 0.35) */
SmAt *sm_jit(double x, double y, double lim) { SmAt *f = at_new(SMK_JIT); f->x = x; f->y = y; f->lim = lim; return f; }
static int jit_call(SmAt *f, PyRandom *rng, double *x, double *y) {
    *x = f->x + py_uniform(rng, -1, 1) * f->lim * 0.35;
    *y = f->y + py_uniform(rng, -1, 1) * f->lim * 0.35;
    return 0;
}
/* small_scene: ring_ = lambda rng: (lambda a, r: (r * math.cos(a), r * math.sin(a)))(rng.uniform(0, 6.2832), R * rng.uniform(0.25, 0.5)) */
SmAt *sm_ring(double R) { SmAt *f = at_new(SMK_RING); f->R = R; return f; }
static int ring_call(SmAt *f, PyRandom *rng, double *x, double *y) {
    double a = py_uniform(rng, 0, 6.2832), r = f->R * py_uniform(rng, 0.25, 0.5);
    *x = r * py_cos(a); *y = r * py_sin(a); return 0;
}
SmAt *sm_fn(SmAtFn fn, void *ctx) { SmAt *f = at_new(SMK_FN); f->fn = fn; f->ctx = ctx; return f; }

int sm_at_call(SmAt *f, PyRandom *rng, double *x, double *y) {
    if (!f) return SC_ERR;          /* None(rng) */
    switch (f->kind) {
    case SMK_NEAR: return near_call(f, rng, x, y);
    case SMK_RIM: return rim_call(f, rng, x, y);
    case SMK_IN: return in_call(f, rng, x, y);
    case SMK_FAR: return far_call(f, rng, x, y);
    case SMK_MIX: return mix_call(f, rng, x, y);
    case SMK_NOT: return not_call(f, rng, x, y);
    case SMK_JIT: return jit_call(f, rng, x, y);
    case SMK_RING: return ring_call(f, rng, x, y);
    case SMK_FN: return f->fn(f->ctx, rng, x, y);
    }
    return SC_ERR;
}

/* ======================= aims ======================= */
enum { SMA_PAR, SMA_TO, SMA_RIM, SMA_FN };
struct SmAim { int rc, kind; GEOSGeometry **ext; int ne; double jit, c[2]; SmAimFn fn; void *ctx; };
static SmAim *aim_new(int kind) { SmAim *a = (SmAim *)calloc(1, sizeof *a); a->rc = 1; a->kind = kind; return a; }
SmAim *sm_aim_ref(SmAim *a) { if (a) a->rc++; return a; }
void sm_aim_unref(SmAim *a) { int i; if (!a || --a->rc) return; for (i = 0; i < a->ne; i++) shp_free(a->ext[i]); free(a->ext); free(a); }
/* a_par(g, jit=0.12): ext = [p.exterior for p in polys_of(g)] */
int sm_a_par(Geo *g, double jit, SmAim **out) {
    SmAim *a = aim_new(SMA_PAR); ShpList ps = { 0 }; int i;
    a->jit = jit;
    if (g) shp_polys_of(g->g, &ps);
    a->ext = (GEOSGeometry **)malloc(sizeof(GEOSGeometry *) * (size_t)(ps.n ? ps.n : 1));
    for (i = 0; i < ps.n; i++) a->ext[a->ne++] = shp_clone(shp_exterior(ps.g[i]));
    shp_list_free(&ps);
    *out = a; return 0;
}
/*     def f(rng, x, y, a):
           if not ext: return a
           pt = Point(x, y); r = min(ext, key=lambda e: e.distance(pt)); u = r.project(pt)
           p0 = r.interpolate((u - 0.7) % r.length); p1 = r.interpolate((u + 0.7) % r.length)
           return math.atan2(p1.y - p0.y, p1.x - p0.x) + rng.uniform(-jit, jit) */
static int par_call(SmAim *f, PyRandom *rng, double x, double y, double a, double *out) {
    GEOSGeometry *pt, *p0, *p1; int i, best = 0; double bd, u, L, m0, m1;
    if (!f->ne) { *out = a; return 0; }
    pt = shp_point(x, y);
    bd = shp_distance(f->ext[0], pt);
    for (i = 1; i < f->ne; i++) { double di = shp_distance(f->ext[i], pt); if (di < bd) { bd = di; best = i; } }
    u = shp_project(f->ext[best], pt, 0); shp_free(pt);
    if (u < 0) return SC_ERR;          /* GEOSProject's -1: shapely raises GEOSException (a NaN point...) */
    L = shp_length(f->ext[best]);
    if (px_mod_d(u - 0.7, L, &m0) != PX_OK) return SC_ERR;
    p0 = shp_interpolate(f->ext[best], m0, 0);
    if (px_mod_d(u + 0.7, L, &m1) != PX_OK) { shp_free(p0); return SC_ERR; }
    p1 = shp_interpolate(f->ext[best], m1, 0);
    if (!p0 || !p1) { shp_free(p0); shp_free(p1); return SC_ERR; }
    *out = py_atan2(shp_y(p1) - shp_y(p0), shp_x(p1) - shp_x(p0)) + py_uniform(rng, -f->jit, f->jit);
    shp_free(p0); shp_free(p1);
    return 0;
}
/* a_to(c, jit=0.15): lambda rng, x, y, a: math.atan2(c[1] - y, c[0] - x) + rng.uniform(-jit, jit) */
SmAim *sm_a_to(double cx, double cy, double jit) { SmAim *a = aim_new(SMA_TO); a->c[0] = cx; a->c[1] = cy; a->jit = jit; return a; }
/* a_rim(rng, x, y, a): math.atan2(y, x) + math.pi / 2 + rng.uniform(-0.15, 0.15) */
SmAim *sm_a_rim(void) { return aim_new(SMA_RIM); }
SmAim *sm_aim_fn(SmAimFn fn, void *ctx) { SmAim *a = aim_new(SMA_FN); a->fn = fn; a->ctx = ctx; return a; }
int sm_aim_call(SmAim *f, PyRandom *rng, double x, double y, double a, double *out) {
    if (!f) return SC_ERR;
    switch (f->kind) {
    case SMA_PAR: return par_call(f, rng, x, y, a, out);
    case SMA_TO: *out = py_atan2(f->c[1] - y, f->c[0] - x) + py_uniform(rng, -f->jit, f->jit); return 0;
    case SMA_RIM: *out = py_atan2(y, x) + PI / 2 + py_uniform(rng, -0.15, 0.15); return 0;
    case SMA_FN: return f->fn(f->ctx, rng, x, y, a, out);
    }
    return SC_ERR;
}

/* ======================= focus ======================= */
int sm_hook_err;
static void focus_hook(void *ctx, PyRandom *rng, double *x, double *y) { if (sm_at_call((SmAt *)ctx, rng, x, y) < 0) { sm_hook_err = 1; *x = *y = 0; } }
static double aim_hook(void *ctx, PyRandom *rng, double x, double y, double a) { double r = a; if (sm_aim_call((SmAim *)ctx, rng, x, y, a, &r) < 0) { sm_hook_err = 1; r = a; } return r; }
/* focus.__enter__: self.keep = (FOCUS[0], AIM[0]); FOCUS[0], AIM[0] = self.at, self.aim */
void sm_focus_enter(SmFocus *keep, SmAt *at, SmAim *aim) {
    keep->f = lf_focus; keep->fc = lf_focus_ctx; keep->a = lf_aimfn; keep->ac = lf_aim_ctx;
    keep->at = sm_at_ref(at); keep->aim = sm_aim_ref(aim);
    lf_focus = at ? focus_hook : NULL; lf_focus_ctx = at;
    lf_aimfn = aim ? aim_hook : NULL; lf_aim_ctx = aim;
}
/* focus.__exit__: FOCUS[0], AIM[0] = self.keep */
void sm_focus_exit(SmFocus *keep) {
    lf_focus = keep->f; lf_focus_ctx = keep->fc; lf_aimfn = keep->a; lf_aim_ctx = keep->ac;
    sm_at_unref(keep->at); sm_aim_unref(keep->aim); keep->at = NULL; keep->aim = NULL;
}

/* ======================= removing things ======================= */
/* _remove(s, dead):
       if not dead: return
       ids = {id(p) for p in dead}; tree = _Hits(dead)
       def rider(p, z0): return z0 > H + 0.05 and tree.hit(p.representative_point())
       ops = []
       for op in s.ops:
           ps = []
           for p in op[1]:
               if id(p) in ids: continue
               if op[-1] not in BRIDGE and op[-1] not in TERRK and rider(p, op[2]): ids.add(id(p)); continue
               ps.append(p)
           if ps: ops.append(op if len(ps) == len(op[1]) else (op[0], ps) + tuple(op[2:]))
       s.ops = ops
       s.chops = [c for c in getattr(s, "chops", []) if id(c["poly"]) not in ids]
       s.feet = [p for p in s.feet if id(p) not in ids]
       if hasattr(s, "objs"): s.objs = [p for p in s.objs if id(p) not in ids]
       if hasattr(s, "terr"): s.terr = [e for e in s.terr if id(e[0]) not in ids]
       s.tcuts = [(ps2, z0, z1, k) for ps, z0, z1, k in s.tcuts for ps2 in [[p for p in ps if not (z1 > H + 0.35 and tree.hit(p.representative_point()))]] if ps2] */
static int hit_rep(PlHits *t, const Geo *p) {          /* tree.hit(p.representative_point()) */
    GEOSGeometry *rp; int h;
    if (!p) return SC_ERR;          /* None.representative_point */
    rp = shp_representative_point(p->g);
    if (!rp) return SC_ERR;
    h = pl_hits_hit(t, rp); shp_free(rp);
    return h;
}
int sm_remove(Scene *s, const GeoList *dead) {
    SmIdSet ids = { 0 }; PlHits tree; OpList ops = { 0 }; int i, j, r = 0;
    if (!dead->n) return 0;
    for (i = 0; i < dead->n; i++) sm_ids_add(&ids, dead->v[i]);
    if (pl_hits_init(&tree, dead) < 0) { sm_ids_free(&ids); pl_hits_free(&tree); return SC_ERR; }
    for (i = 0; r >= 0 && i < s->ops.n; i++) {
        Op *o = s->ops.v[i]; GeoList ps = { 0 }; int open = !is_bridge(o->kind) && !is_terrk(o->kind);
        for (j = 0; j < o->polys.n; j++) {
            Geo *p = o->polys.v[j];
            if (p ? sm_ids_has(&ids, p) : 0) continue;
            if (open && o->n[0].v > SC.H + 0.05) {
                int h = hit_rep(&tree, p);
                if (h < 0) { r = SC_ERR; break; }
                if (h) { sm_ids_add(&ids, p); continue; }
            }
            gl_push(&ps, p);
        }
        if (r >= 0 && ps.n) { if (ps.n == o->polys.n) ol_push(&ops, o); else ol_push_own(&ops, op_with(o, &ps)); }
        gl_free(&ps);
    }
    if (r < 0) { ol_free(&ops); pl_hits_free(&tree); sm_ids_free(&ids); return SC_ERR; }
    ol_free(&s->ops); s->ops = ops;
    {   ChopList nc = { 0 };
        for (i = 0; i < s->chops.n; i++) { Chop *c = s->chops.v[i]; if (sm_ids_has(&ids, c->poly)) chop_unref_(c); else chl_push_own(&nc, c); }
        free(s->chops.v); s->chops = nc; }
    {   GeoList nf = { 0 };
        for (i = 0; i < s->feet.n; i++) if (!sm_ids_has(&ids, s->feet.v[i])) gl_push(&nf, s->feet.v[i]);
        gl_free(&s->feet); s->feet = nf; }
    if (s->has_objs) {
        GeoList no = { 0 };
        for (i = 0; i < s->objs.n; i++) if (!sm_ids_has(&ids, s->objs.v[i])) gl_push(&no, s->objs.v[i]);
        gl_free(&s->objs); s->objs = no;
    }
    if (s->has_terr) {
        TerrList nt = { 0 };
        for (i = 0; i < s->terr.n; i++) { Terr *e = &s->terr.v[i]; if (!sm_ids_has(&ids, e->fp)) trl_push(&nt, e->fp, e->zb, e->zt, e->top); }
        trl_free_(&s->terr); s->terr = nt;
    }
    {   TcutList nt = { 0 };
        for (i = 0; r >= 0 && i < s->tcuts.n; i++) {
            Tcut *c = &s->tcuts.v[i]; GeoList ps2 = { 0 }; int high = c->z1.v > SC.H + 0.35;
            for (j = 0; j < c->polys.n; j++) {
                Geo *p = c->polys.v[j];
                if (high) { int h = hit_rep(&tree, p); if (h < 0) { r = SC_ERR; break; } if (h) continue; }
                gl_push(&ps2, p);
            }
            if (r >= 0 && ps2.n) tcl_push(&nt, &ps2, c->z0, c->z1, c->kind);
            gl_free(&ps2);
        }
        if (r >= 0) { tcl_free_(&s->tcuts); s->tcuts = nt; } else tcl_free_(&nt);
    }
    pl_hits_free(&tree); sm_ids_free(&ids);
    return r;
}

/* clear_small(s, g):
       gp = prep(g); cells = _cells(s)
       dead = [p for op in s.ops if op[-1] not in BRIDGE and op[-1] not in TERRK for p in op[1] if p.area < 10 and id(p) not in cells and gp.intersects(p)]
       _remove(s, dead)
       s.gcuts = [(ps2, d, k) for ps, d, k in s.gcuts for ps2 in [[p for p in ps if not (k == "pit" and p.area < 2.5 and gp.intersects(p))]] if ps2] */
int sm_clear_small(Scene *s, Geo *g) {
    ShpPrep gp; SmIdSet cells = { 0 }; GeoList dead = { 0 }; int i, j, r = 0;
    if (!g) return SC_ERR;
    gp = shp_prep(g->g); sm_cells(s, &cells);
    for (i = 0; r >= 0 && i < s->ops.n; i++) {
        Op *o = s->ops.v[i];
        if (is_bridge(o->kind) || is_terrk(o->kind)) continue;
        for (j = 0; j < o->polys.n; j++) {
            Geo *p = o->polys.v[j]; int x;
            if (!p) { r = SC_ERR; break; }
            if (!(area(p) < 10) || sm_ids_has(&cells, p)) continue;
            x = shp_prep_intersects(&gp, p->g);
            if (x < 0) { r = SC_ERR; break; }
            if (x) gl_push(&dead, p);
        }
    }
    sm_ids_free(&cells);
    if (r >= 0) r = sm_remove(s, &dead);
    gl_free(&dead);
    if (r >= 0) {
        GcutList ng = { 0 };
        for (i = 0; r >= 0 && i < s->gcuts.n; i++) {
            Gcut *c = &s->gcuts.v[i]; GeoList ps2 = { 0 }; int pit = kind_is(c->kind, "pit");
            for (j = 0; j < c->polys.n; j++) {
                Geo *p = c->polys.v[j];
                if (pit) {
                    if (!p) { r = SC_ERR; break; }
                    if (area(p) < 2.5) { int x = shp_prep_intersects(&gp, p->g); if (x < 0) { r = SC_ERR; break; } if (x) continue; }
                }
                gl_push(&ps2, p);
            }
            if (r >= 0 && ps2.n) gcl_push(&ng, &ps2, c->depth, c->kind);
            gl_free(&ps2);
        }
        if (r >= 0) { gcl_free_(&s->gcuts); s->gcuts = ng; } else gcl_free_(&ng);
    }
    shp_prep_free(&gp);
    return r;
}

/* clear_terrain(s, g):
       if not terr(s) or not s.terr: return
       inop = {id(p) for op in s.ops if op[0] == "prism" and op[-1] in TERRK for p in op[1]}
       dead = []; swap = {}; gb = None
       for e in s.terr:
           fp = e[0]
           if id(fp) not in inop or not fp.intersects(g): continue
           a = fp.intersection(g).area
           if a < 1e-3: continue
           if a <= 0.15 * fp.area:
               gb = gb or g.buffer(0.35)
               q = [x for x in polys_of(fp.difference(gb)) if x.area > 3 and wide_enough(x, 1.6)]
               if len(q) == 1: swap[id(fp)] = q[0].simplify(0.03); continue
           dead.append(fp)
       if swap:
           s.ops = [(op[0], [swap.get(id(p), p) for p in op[1]]) + tuple(op[2:]) for op in s.ops]
           s.feet = [swap.get(id(p), p) for p in s.feet]
           s.terr = [(swap[id(e[0])], e[1], e[2], inset(swap[id(e[0])], 0.4)) if id(e[0]) in swap else e for e in s.terr]
       if dead:
           loose = [p for op in s.ops if op[-1] not in BRIDGE and op[-1] not in TERRK for p in op[1] if p.area < 10 and op[2] > H + 0.05 and any(d.contains(p.representative_point()) for d in dead)]
           _remove(s, dead + loose) */
typedef struct { const Geo *from; Geo *to; } Swap;
static Geo *swap_get(const Swap *sw, int n, Geo *p) { int i; for (i = n - 1; i >= 0; i--) if (sw[i].from == p) return sw[i].to; return p; }          /* the last one set wins (a dict) */
int sm_clear_terrain(Scene *s, Geo *g) {
    SmIdSet inop = { 0 }; GeoList dead = { 0 }; Swap *sw = NULL; int nsw = 0, i, j, r = 0; GEOSGeometry *gb = NULL;
    if (!pl_terr(s) || !s->terr.n) return 0;
    if (!g) return SC_ERR;
    for (i = 0; i < s->ops.n; i++) { Op *o = s->ops.v[i]; if (o->type == OP_PRISM && is_terrk(o->kind)) for (j = 0; j < o->polys.n; j++) sm_ids_add(&inop, o->polys.v[j]); }
    for (i = 0; r >= 0 && i < s->terr.n; i++) {
        Geo *fp = s->terr.v[i].fp; int x; GEOSGeometry *it; double a;
        if (!sm_ids_has(&inop, fp)) continue;
        x = shp_intersects(fp->g, g->g);
        if (x < 0) { r = SC_ERR; break; }
        if (!x) continue;
        it = shp_intersection(fp->g, g->g);
        if (!it) { r = SC_ERR; break; }
        a = shp_area(it); shp_free(it);
        if (a < 1e-3) continue;
        if (a <= 0.15 * area(fp)) {
            GEOSGeometry *df; GeoList ps = { 0 }, q = { 0 };
            if (!gb || shp_is_empty(gb)) { shp_free(gb); gb = shp_buffer_q(g->g, 0.35, 16); if (!gb) { r = SC_ERR; break; } }
            df = shp_difference(fp->g, gb);
            if (!df) { r = SC_ERR; break; }
            lf_polys_of_new(df, &ps);
            for (j = 0; j < ps.n; j++) {
                if (!(area(ps.v[j]) > 3)) continue;
                x = lf_wide_enough(ps.v[j], 1.6);
                if (x < 0) { r = SC_ERR; break; }
                if (x) gl_push(&q, ps.v[j]);
            }
            gl_free(&ps);
            if (r >= 0 && q.n == 1) {
                GEOSGeometry *sp = shp_simplify(q.v[0]->g, 0.03, 1);
                gl_free(&q);
                if (!sp) { r = SC_ERR; break; }
                sw = (Swap *)realloc(sw, sizeof(Swap) * (size_t)(nsw + 1)); sw[nsw].from = fp; sw[nsw].to = geo_new(sp); nsw++;
                continue;
            }
            gl_free(&q);
            if (r < 0) break;
        }
        gl_push(&dead, fp);
    }
    shp_free(gb); sm_ids_free(&inop);
    if (r < 0) { for (i = 0; i < nsw; i++) geo_unref(sw[i].to); free(sw); gl_free(&dead); return SC_ERR; }
    if (nsw) {
        OpList ops = { 0 }; GeoList feet = { 0 }; TerrList nt = { 0 };
        for (i = 0; i < s->ops.n; i++) {
            Op *o = s->ops.v[i]; GeoList ps = { 0 };
            for (j = 0; j < o->polys.n; j++) gl_push(&ps, swap_get(sw, nsw, o->polys.v[j]));
            ol_push_own(&ops, op_with(o, &ps)); gl_free(&ps);
        }
        ol_free(&s->ops); s->ops = ops;
        for (i = 0; i < s->feet.n; i++) gl_push(&feet, swap_get(sw, nsw, s->feet.v[i]));
        gl_free(&s->feet); s->feet = feet;
        for (i = 0; r >= 0 && i < s->terr.n; i++) {
            Terr *e = &s->terr.v[i]; Geo *to = swap_get(sw, nsw, e->fp);
            if (to != e->fp) {
                Geo *top = lf_inset(to, 0.4);
                if (!top) { r = SC_ERR; break; }
                trl_push(&nt, to, e->zb, e->zt, top); geo_unref(top);
            } else trl_push(&nt, e->fp, e->zb, e->zt, e->top);
        }
        if (r < 0) { trl_free_(&nt); for (i = 0; i < nsw; i++) geo_unref(sw[i].to); free(sw); gl_free(&dead); return SC_ERR; }
        trl_free_(&s->terr); s->terr = nt;
    }
    for (i = 0; i < nsw; i++) geo_unref(sw[i].to);
    free(sw);
    if (dead.n) {
        GeoList all = { 0 };
        gl_copy(&all, &dead);
        for (i = 0; r >= 0 && i < s->ops.n; i++) {
            Op *o = s->ops.v[i];
            if (is_bridge(o->kind) || is_terrk(o->kind)) continue;
            for (j = 0; j < o->polys.n; j++) {
                Geo *p = o->polys.v[j]; GEOSGeometry *rp; int k, hit = 0;
                if (!p) { r = SC_ERR; break; }
                if (!(area(p) < 10) || !(o->n[0].v > SC.H + 0.05)) continue;
                rp = shp_representative_point(p->g);
                if (!rp) { r = SC_ERR; break; }
                for (k = 0; k < dead.n; k++) { int c = shp_contains(dead.v[k]->g, rp); if (c < 0) { r = SC_ERR; break; } if (c) { hit = 1; break; } }
                shp_free(rp);
                if (r < 0) break;
                if (hit) gl_push(&all, p);
            }
        }
        if (r >= 0) r = sm_remove(s, &all);
        gl_free(&all);
    }
    gl_free(&dead);
    return r;
}

/* _bigs(s): cells = _cells(s)
       return [p for op in s.ops if op[-1] not in SOFT and op[-1] not in BRIDGE and op[-1] not in TERRK for p in op[1] if p.area >= 10 and id(p) not in cells and op[2] <= H + 0.05] */
int sm_bigs(const Scene *s, GeoList *out) {
    SmIdSet cells = { 0 }; int i, j;
    sm_cells(s, &cells);
    for (i = 0; i < s->ops.n; i++) {
        Op *o = s->ops.v[i];
        if (is_soft(o->kind) || is_bridge(o->kind) || is_terrk(o->kind)) continue;
        for (j = 0; j < o->polys.n; j++) {
            Geo *p = o->polys.v[j];
            if (!p) { sm_ids_free(&cells); return SC_ERR; }
            if (area(p) >= 10 && !sm_ids_has(&cells, p) && o->n[0].v <= SC.H + 0.05) gl_push(out, p);
        }
    }
    sm_ids_free(&cells);
    return 0;
}

/* _top_at(s, x, y):
       pt = Point(x, y); best = None
       for op in s.ops:
           top = op[3] if op[0] == "prism" else op[2] + op[3] if op[-1] == "rockc" else None
           if top is None or top <= H + 0.05 or (best and top <= best[0]): continue
           for p in op[1]:
               if p.contains(pt) and p.area > 4.0: best = (top, p); break
       return best */
int sm_top_at(const Scene *s, double x, double y, Num *top, Geo **p) {
    GEOSGeometry *pt = shp_point(x, y); int i, j, found = 0; Num bt = num_none(); Geo *bp = NULL;
    if (!pt) return SC_ERR;
    for (i = 0; i < s->ops.n; i++) {
        Op *o = s->ops.v[i]; Num t;
        if (o->type == OP_PRISM) t = o->n[1];
        else if (kind_is(o->kind, "rockc")) t = num_add(o->n[0], o->n[1]);
        else continue;
        if (t.t == NUM_NONE || t.v <= SC.H + 0.05 || (found && t.v <= bt.v)) continue;
        for (j = 0; j < o->polys.n; j++) {
            Geo *q = o->polys.v[j]; int c;
            if (!q) { shp_free(pt); return SC_ERR; }
            c = shp_contains(q->g, pt);
            if (c < 0) { shp_free(pt); return SC_ERR; }
            if (c && area(q) > 4.0) { bt = t; bp = q; found = 1; break; }
        }
    }
    shp_free(pt);
    if (!found) return 0;
    *top = bt; *p = geo_ref(bp);
    return 1;
}

/* _plates(s):
       lv = {}
       for op in s.ops:
           if op[0] == "prism" and op[-1] in BRIDGE: lv.setdefault(round(op[3], 3), []).extend(op[1])
       return [(z, unary_union(ps)) for z, ps in sorted(lv.items(), reverse=True)]
   The dict keys are numbers: an int and a float of the same value are one key, the first one kept. */
typedef struct { Num z; GeoList ps; } Lv;
static int lv_desc(const void *a, const void *b) { double x = ((const Lv *)a)->z.v, y = ((const Lv *)b)->z.v; return x > y ? -1 : x < y ? 1 : 0; }
int sm_plates(const Scene *s, SmPlate **out, int *n) {
    Lv *lv = NULL; int nl = 0, i, j, r = 0;
    *out = NULL; *n = 0;
    for (i = 0; i < s->ops.n; i++) {
        Op *o = s->ops.v[i]; Num z;
        if (o->type != OP_PRISM || !is_bridge(o->kind)) continue;
        z = num_round(o->n[1], 3);
        for (j = 0; j < nl; j++) if (lv[j].z.v == z.v) break;
        if (j == nl) { lv = (Lv *)realloc(lv, sizeof(Lv) * (size_t)(nl + 1)); memset(&lv[nl], 0, sizeof(Lv)); lv[nl].z = z; nl++; }
        gl_extend(&lv[j].ps, &o->polys);
    }
    qsort(lv, (size_t)nl, sizeof(Lv), lv_desc);          /* distinct keys: the order is unique */
    *out = (SmPlate *)calloc((size_t)(nl ? nl : 1), sizeof(SmPlate));
    for (i = 0; i < nl; i++) {
        if (r >= 0) {
            const GEOSGeometry **gs = (const GEOSGeometry **)malloc(sizeof(*gs) * (size_t)(lv[i].ps.n ? lv[i].ps.n : 1)); GEOSGeometry *u;
            for (j = 0; j < lv[i].ps.n; j++) gs[j] = lv[i].ps.v[j] ? lv[i].ps.v[j]->g : NULL;
            u = shp_union_all(gs, lv[i].ps.n); free(gs);
            if (!u) r = SC_ERR;
            else { (*out)[*n].z = lv[i].z; (*out)[*n].g = geo_new(u); (*n)++; }
        }
        gl_free(&lv[i].ps);
    }
    free(lv);
    if (r < 0) { sm_plates_free(*out, *n); *out = NULL; *n = 0; }
    return r;
}
void sm_plates_free(SmPlate *p, int n) { int i; for (i = 0; i < n; i++) geo_unref(p[i].g); free(p); }

/* _strips(g, step=3.0):
       if g.is_empty: return []
       x0, y0, x1, y1 = g.bounds; out = []
       for x in np.arange(x0, x1, step):
           for y in np.arange(y0, y1, step): out += polys_of(g.intersection(box(x, y, x + step, y + step)))
   In scenes.py `box` is scenes.box(poly, h, lift=0.0), the solid, not shapely's box: the call raises TypeError (4
   arguments for 2..3) the first time the inner loop runs. sm_strips does exactly that; sm_strips_box is the routine
   as meant (shapely.geometry.box), proven against a copy of it, for the day the Python is fixed. */
static int strips(Geo *g, double step, GeoList *out, int as_kit) {
    double b[4], *xs, *ys; int64_t nx, ny, i, j; int e;
    if (!g) return SC_ERR;
    if ((e = is_empty(g)) < 0) return SC_ERR;
    if (e) return 0;
    shp_bounds(g->g, b);
    nx = np_arange(b[0], b[2], step, NULL, 0); ny = np_arange(b[1], b[3], step, NULL, 0);
    if (nx < 0 || ny < 0) return SC_ERR;
    xs = (double *)malloc(sizeof(double) * (size_t)(nx + 1)); ys = (double *)malloc(sizeof(double) * (size_t)(ny + 1));
    np_arange(b[0], b[2], step, xs, nx); np_arange(b[1], b[3], step, ys, ny);
    for (i = 0; i < nx; i++)
        for (j = 0; j < ny; j++) {
            if (as_kit) { free(xs); free(ys); return SC_ERR; }          /* box(x, y, x + step, y + step): TypeError */
            GEOSGeometry *bx = shp_box(xs[i], ys[j], xs[i] + step, ys[j] + step, 1), *it = bx ? shp_intersection(g->g, bx) : NULL;
            shp_free(bx);
            if (!it) { free(xs); free(ys); return SC_ERR; }
            lf_polys_of_new(it, out);
        }
    free(xs); free(ys);
    return 0;
}
int sm_strips(Geo *g, double step, GeoList *out) { return strips(g, step, out, 1); }
int sm_strips_box(Geo *g, double step, GeoList *out) { return strips(g, step, out, 0); }

/* ======================= path helpers ======================= */
/* _at(line, u):
       u = max(0.0, min(line.length, u)); p = line.interpolate(u); q = line.interpolate(min(line.length, u + 0.3)); p0 = line.interpolate(max(0.0, u - 0.3))
       return p.x, p.y, math.atan2(q.y - p0.y, q.x - p0.x) */
static double pmin(double a, double b) { return b < a ? b : a; }          /* min(a, b): a unless b < a */
static double pmax(double a, double b) { return b > a ? b : a; }
int sm_at_line(Geo *line, double u, double *x, double *y, double *a) {
    double L; GEOSGeometry *p, *q, *p0;
    if (!line) return SC_ERR;
    L = shp_length(line->g);
    u = pmax(0.0, pmin(L, u));
    p = shp_interpolate(line->g, u, 0); q = shp_interpolate(line->g, pmin(L, u + 0.3), 0); p0 = shp_interpolate(line->g, pmax(0.0, u - 0.3), 0);
    if (!p || !q || !p0) { shp_free(p); shp_free(q); shp_free(p0); return SC_ERR; }
    *x = shp_x(p); *y = shp_y(p); *a = py_atan2(shp_y(q) - shp_y(p0), shp_x(q) - shp_x(p0));
    shp_free(p); shp_free(q); shp_free(p0);
    return 0;
}
/* _side(line, d, w): line.offset_curve(d).buffer(w / 2, cap_style=2). LineString.offset_curve defaults to quad_segs=16 (shapely.offset_curve to 8) */
Geo *sm_side(Geo *line, double d, double w) {
    GEOSGeometry *oc, *b;
    if (!line) return NULL;
    oc = shp_offset_curve(line->g, d, 16, SHP_JOIN_ROUND, 5.0);
    if (!oc) return NULL;
    b = shp_buffer(oc, w / 2, 16, SHP_CAP_FLAT, SHP_JOIN_ROUND, 5.0, 0); shp_free(oc);
    return geo_new(b);
}
/* _var(line, rng, w0, w1, step=0.9):
       g = unary_union([_disc(*_at(line, u)[:2], rng.uniform(w0, w1) / 2, 10) for u in np.arange(0, line.length + step, step)])
       return g.buffer(0.5).buffer(-0.5).simplify(0.04) */
Geo *sm_var(Geo *line, PyRandom *rng, double w0, double w1, double step) {
    double L, *us; int64_t n, i; GEOSGeometry **ds, *u, *b1, *b2, *sp;
    if (!line) return NULL;
    L = shp_length(line->g);
    n = np_arange(0.0, L + step, step, NULL, 0);
    if (n < 0) return NULL;
    us = (double *)malloc(sizeof(double) * (size_t)(n + 1)); ds = (GEOSGeometry **)calloc((size_t)(n + 1), sizeof(GEOSGeometry *));
    np_arange(0.0, L + step, step, us, n);
    for (i = 0; i < n; i++) {
        double x, y, a;
        if (sm_at_line(line, us[i], &x, &y, &a) < 0) break;
        ds[i] = so_disc(x, y, py_uniform(rng, w0, w1) / 2, 10);
        if (!ds[i]) break;
    }
    u = i == n ? shp_union_all((const GEOSGeometry *const *)ds, (int)n) : NULL;
    for (i = 0; i < n; i++) shp_free(ds[i]);
    free(ds); free(us);
    if (!u) return NULL;
    b1 = shp_buffer_q(u, 0.5, 16); shp_free(u);
    b2 = b1 ? shp_buffer_q(b1, -0.5, 16) : NULL; shp_free(b1);
    sp = b2 ? shp_simplify(b2, 0.04, 1) : NULL; shp_free(b2);
    return geo_new(sp);
}

/* ======================= _u _disc _ann _cap _bar ======================= */
Geo *sm_u(const GeoList *gs) {
    const GEOSGeometry **v = (const GEOSGeometry **)malloc(sizeof(*v) * (size_t)(gs->n ? gs->n : 1)); int i; GEOSGeometry *r;
    for (i = 0; i < gs->n; i++) v[i] = gs->v[i] ? gs->v[i]->g : NULL;
    r = so_u(v, gs->n); free(v);
    return geo_new(r);
}
Geo *sm_disc(double x, double y, double r, int n) { return geo_new(so_disc(x, y, r, n)); }
Geo *sm_ann(double x, double y, double ro, double ri, int n) { return geo_new(so_ann(x, y, ro, ri, n)); }
Geo *sm_cap(const double *p, const double *q, double w) { return geo_new(so_cap(p, q, w)); }
Geo *sm_bar(const double *p, const double *q, double w) { return geo_new(so_bar(p, q, w)); }

/* ======================= kfix, klass, TAGS ======================= */
/* kfix(kind):
       if kind in BRIDGE: k2 = kind + "_p"
       elif kind in gg.PIECES: k2 = kind + "_s"
       else: return kind
       gg.COL.setdefault(k2, gg.COL.get(kind, "#999999"))
       if kind in gg.CHAMFER_PRISMS: gg.CHAMFER_PRISMS.add(k2)
       return k2 */
Kind *sm_kfix(Kind *kind) {
    char buf[256]; const char *suf;
    if (!kind) return NULL;
    if (is_bridge(kind)) suf = "_p";
    else if (in_set(SM.pieces, kind)) suf = "_s";
    else return kind;
    snprintf(buf, sizeof buf, "%s%s", kind->s, suf);
    if (SM.col_setdefault) SM.col_setdefault(SM.col_ctx, buf, kind->s);
    if (in_set(SM.chamfer, kind)) sm_set_add(SM.chamfer, buf);
    return kind_str(buf);
}

static int eq(const char *a, const char *b) { return strcmp(a, b) == 0; }
/* klass(fn):
       c = getattr(fn, "cls", None)
       if c: return c
       tmpl, kind, shape = getattr(fn, "tmpl", ""), getattr(fn, "kind", ""), getattr(fn, "shape", "")
       if tmpl == "comp":
           b = shape.split(":")[0]
           return {"crystals": "debris", "fairy_ring": "growth", "roots": "growth", "tentacle": "growth", "skull": "bones", "ribs": "bones"}.get(b, "made")
       if kind == "bone": return "bones"
       if tmpl in ("sunk", "trail") or (tmpl == "path" and kind in CUTK): return "marks"
       if kind in DEBRIS: return "abstract" if shape in ("ring", "cross") else "debris"
       if kind in GROWTH: return "abstract" if shape in ("ring", "cross", "hex") else "growth"
       if tmpl in ("single", "scatter") and shape in ABSTRACT: return "abstract"
       return "made" */
const char *sm_klass(const SmTag *t) {
    const char *tmpl = t->tmpl ? t->tmpl : "", *kind = t->kind ? t->kind : "", *shape = t->shape ? t->shape : "";
    if (t->cls && *t->cls) return t->cls;
    if (eq(tmpl, "comp")) {
        char b[128]; size_t n = strcspn(shape, ":"); if (n > 127) n = 127; memcpy(b, shape, n); b[n] = 0;
        if (eq(b, "crystals")) return "debris";
        if (eq(b, "fairy_ring") || eq(b, "roots") || eq(b, "tentacle")) return "growth";
        if (eq(b, "skull") || eq(b, "ribs")) return "bones";
        return "made";
    }
    if (eq(kind, "bone")) return "bones";
    if (eq(tmpl, "sunk") || eq(tmpl, "trail") || (eq(tmpl, "path") && sm_set_has(SM.cutk, kind))) return "marks";
    if (sm_set_has(SM.debris, kind)) return eq(shape, "ring") || eq(shape, "cross") ? "abstract" : "debris";
    if (sm_set_has(SM.growth, kind)) return eq(shape, "ring") || eq(shape, "cross") || eq(shape, "hex") ? "abstract" : "growth";
    if ((eq(tmpl, "single") || eq(tmpl, "scatter")) && (eq(shape, "ring") || eq(shape, "cross") || eq(shape, "hex") || eq(shape, "crescent") || eq(shape, "star"))) return "abstract";
    return "made";
}

static const char *const TAGS[][5] = {          /* scenes.TAGS: name, template, kind, shape, class */
    { "b_wash", "line", "wash", "channel", "line" }, { "b_hoodoo", "single", "spire", "rock", "debris" }, { "b_mudcrack", "sunk", "pan", "blob", "marks" },
    { "b_agave", "scatter", "agave", "star", "growth" }, { "b_washbed", "scatter", "pebble", "round", "late" }, { "b_spiral", "sunk", "glyph", "spiral", "late" },
    { "b_split", "single", "spire", "rock", "debris" }, { "b_scree", "scatter", "rock", "rock", "late" },
    { "h_bog", "sunk", "water", "blob", "marks" }, { "h_steps", "line", "slate", "stones", "line" }, { "h_wall", "line", "slate", "wall", "line" }, { "h_moss", "scatter", "moss", "blob", "growth" },
    { "h_heather", "scatter", "heather", "disc", "growth" }, { "h_standing", "single", "standing", "rect", "made" }, { "h_stream", "line", "water", "channel", "line" },
    { "h_fern", "scatter", "fern", "frond", "growth" },
    { "m_ruin", "scatter", "ruin", "rect", "made" }, { "m_shrub", "scatter", "shrub", "star", "growth" }, { "m_sinkhole", "sunk", "pit", "disc", "marks" },
    { "m_crystal", "scatter", "crystal", "shard", "debris" }, { "m_salt", "sunk", "salt", "blob", "marks" }, { "m_obelisk", "single", "obelisk", "bar", "made" },
    { "m_column", "single", "obelisk", "disc", "made" }, { "m_pavers", "scatter", "ruin", "rect", "made" },
    { "c_vent", "single", "metal", "rect", "made" }, { "c_chevron", "scatter", "paint", "chevron", "made" }, { "c_cable", "line", "cable", "cable", "line" },
    { "c_panel", "single", "metal", "rect", "made" }, { "c_spill", "sunk", "coolant", "blob", "marks" }, { "c_hexdeck", "sunk", "slot", "hexnet", "marks" },
    { "c_scorch", "sunk", "scorch", "blob", "marks" }, { "c_lights", "scatter", "lens", "rect", "made" },
    { "i_eggs", "scatter", "egg", "ell", "growth" }, { "i_acid", "sunk", "acid", "blob", "marks" }, { "i_veins", "line", "vein", "veins", "line" },
    { "i_spores", "scatter", "spore", "disc", "growth" }, { "i_hatched", "scatter", "shell", "disc", "growth" }, { "i_torn", "single", "plate", "shard", "late" },
    { "i_ribs", "scatter", "carapace", "rect", "growth" },
    { "u_bricks", "scatter", "brick", "rect", "debris" }, { "u_bullets", "sunk", "pothole", "disc", "marks" }, { "u_wall", "single", "brick", "rect", "debris" },
    { "u_dashes", "scatter", "paint", "rect", "line" }, { "u_scorch", "sunk", "scorch", "blob", "marks" }, { "u_rebar", "single", "concrete", "slab", "debris" },
    { "u_tilt", "scatter", "paver", "rect", "late" }, { "u_crater", "sunk", "pothole", "crater", "marks" },
    { "n_posts", "line", "post", "rect", "line" }, { "n_ore", "line", "quartz", "rock", "line" }, { "n_timbers", "scatter", "wood", "rect", "made" },
    { "n_seep", "sunk", "water", "blob", "marks" }, { "n_orepile", "scatter", "quartz", "rock", "made" }, { "n_shaft", "sunk", "pit", "crater", "marks" },
    { "n_barrel", "single", "wood", "ring", "made" }, { "n_ladder", "single", "wood", "ladder", "made" },
    { "f_post_row", "scatter", "post", "rect", "made" }, { "f_headstone", "single", "headstone", "rect", "made" }, { "f_lantern", "single", "lantern", "hex", "made" },
    { "f_column", "single", "column", "disc", "made" }, { "f_stump", "single", "stump", "rock", "growth" }, { "f_teeth", "scatter", "tooth", "tri", "growth" },
    { "f_sandbags", "scatter", "sandbag", "rect", "made" }, { "f_coffin", "single", "coffin", "coffin", "made" }, { "f_trapdoor", "single", "trap", "rect", "made" },
    { "f_lily", "scatter", "lily", "disc", "late" }, { "f_vent", "single", "vent", "disc", "debris" },
};
int sm_tag_lookup(const char *name, SmTag *out) {
    size_t i;
    for (i = 0; i < sizeof TAGS / sizeof *TAGS; i++)
        if (strcmp(TAGS[i][0], name) == 0) { out->tmpl = TAGS[i][1]; out->kind = TAGS[i][2]; out->shape = TAGS[i][3]; out->cls = TAGS[i][4]; return 1; }
    return 0;
}
/* _tag_all: for fn in [every tier function] + [every detail]: once each (by id); nm = fn.__qualname__.split(".")[0];
   if nm in TAGS: fn.tmpl, fn.kind, fn.shape, fn.cls = TAGS[nm] */
void sm_tag_all(SmTag *fns, const char *const *names, const int *same, int n) {
    int i;
    for (i = 0; i < n; i++) {
        char nm[128]; size_t k;
        if (same && same[i] != i) continue;
        k = strcspn(names[i], "."); if (k > 127) k = 127; memcpy(nm, names[i], k); nm[k] = 0;
        sm_tag_lookup(nm, &fns[i]);
    }
}

/* ======================= _bare_grid ======================= */
/* _bare_grid(sc, lim):
       s = sc.s; big = 3.1416 * lim * lim * 1.5; small, rings = [], []
       for ps in [op[1] for op in s.ops] + [g[0] for g in s.gcuts] + [c[0] for c in s.tcuts]:
           for q in ps:
               if q is None or q.is_empty: continue
               if q.area < big * 0.5 or q.buffer(-lim * 0.9, 4).is_empty: small.append(q)
               else: rings.append(q.exterior)
       r = gg.R0 - 1.0; st = lim / 3.0; xs = np.arange(-r + st / 2, r, st)
       gx, gy = np.meshgrid(xs, xs); keep = gx * gx + gy * gy <= r * r; P = np.column_stack([gx[keep], gy[keep]])
       if not len(P): return P, np.zeros(0)
       pts = shapely.points(P); d = np.full(len(P), np.inf)
       for geoms, off in ((small, 0.0), (rings, 0.5)):
           if not geoms: continue
           tree = shapely.STRtree(geoms); (ia, ib), dist = tree.query_nearest(pts, return_distance=True, all_matches=False)
           np.minimum.at(d, ia, dist - off)
       return P, d */
typedef struct { const GEOSGeometry **v; int n, cap; } GPtrs;
static void gp_push(GPtrs *l, const GEOSGeometry *g) { if (l->n >= l->cap) { l->cap = l->cap ? 2 * l->cap : 256; l->v = (const GEOSGeometry **)realloc((void *)l->v, sizeof(*l->v) * (size_t)l->cap); } l->v[l->n++] = g; }
static int bare_one(const GeoList *ps, double lim, double big, GPtrs *small, GPtrs *rings) {
    int j;
    for (j = 0; j < ps->n; j++) {
        Geo *q = ps->v[j]; int e;
        if (!q) continue;
        if ((e = is_empty(q)) < 0) return SC_ERR;
        if (e) continue;
        if (area(q) < big * 0.5) { gp_push(small, q->g); continue; }
        {   GEOSGeometry *b = shp_buffer(q->g, -lim * 0.9, 4, SHP_CAP_ROUND, SHP_JOIN_ROUND, 5.0, 0);
            if (!b) return SC_ERR;
            e = shp_is_empty(b); shp_free(b);
            if (e) gp_push(small, q->g);
            else {
                const GEOSGeometry *ex = shp_exterior(q->g);
                if (!ex) return SC_ERR;          /* .exterior of a non-polygon */
                gp_push(rings, ex);
            }
        }
    }
    return 0;
}
int sm_bare_grid(const Scene *s, double lim, double **P_, double **d_, int64_t *n_) {
    double big = 3.1416 * lim * lim * 1.5, r, st, *P, *d; GPtrs small = { 0 }, rings = { 0 }; int i, k, rc = 0; int64_t n, j;
    GEOSGeometry **pts;
    *P_ = *d_ = NULL; *n_ = 0;
    for (i = 0; rc >= 0 && i < s->ops.n; i++) rc = bare_one(&s->ops.v[i]->polys, lim, big, &small, &rings);
    for (i = 0; rc >= 0 && i < s->gcuts.n; i++) rc = bare_one(&s->gcuts.v[i].polys, lim, big, &small, &rings);
    for (i = 0; rc >= 0 && i < s->tcuts.n; i++) rc = bare_one(&s->tcuts.v[i].polys, lim, big, &small, &rings);
    if (rc < 0) { free((void *)small.v); free((void *)rings.v); return SC_ERR; }
    r = SC.R0 - 1.0; st = lim / 3.0;
    n = np_bare_grid(r, st, NULL, 0);
    if (n <= 0) { free((void *)small.v); free((void *)rings.v); return n < 0 ? SC_ERR : 0; }
    P = (double *)malloc(sizeof(double) * 2 * (size_t)n); d = (double *)malloc(sizeof(double) * (size_t)n);
    n = np_bare_grid(r, st, P, n);
    pts = (GEOSGeometry **)malloc(sizeof(GEOSGeometry *) * (size_t)n);
    for (j = 0; j < n; j++) { pts[j] = shp_point(P[2 * j], P[2 * j + 1]); d[j] = INFINITY; }
    for (k = 0; rc >= 0 && k < 2; k++) {
        const GPtrs *gs = k ? &rings : &small; double off = k ? 0.5 : 0.0; ShpTree *t;
        if (!gs->n) continue;
        t = shp_tree_new(gs->v, gs->n, 10);
        if (!t) { rc = SC_ERR; break; }
        for (j = 0; j < n; j++) {
            ShpIdx x = { 0 }; int m = shp_tree_query_nearest(t, pts[j], 0, 0, &x), q;
            if (m < 0) { rc = SC_ERR; shp_idx_free(&x); break; }
            for (q = 0; q < m; q++) d[j] = np_minimum(d[j], x.d[q] - off);          /* np.minimum.at(d, ia, dist - off): ia == j */
            shp_idx_free(&x);
        }
        shp_tree_free(t);
    }
    for (j = 0; j < n; j++) shp_free(pts[j]);
    free(pts); free((void *)small.v); free((void *)rings.v);
    if (rc < 0) { free(P); free(d); return SC_ERR; }
    *P_ = P; *d_ = d; *n_ = n;
    return 0;
}

/* ======================= mini_type ======================= */
static const char *const MINIS[] = { "biped", "wide", "walker", "tripod", "quad", "many", "scorpion", "crawler", "tracked", "hover", "none" };
static const char *const MINI_ALIAS[][2] = {
    { "fat", "wide" }, { "broad", "wide" }, { "two", "biped" }, { "2", "biped" }, { "human", "biped" }, { "robot", "walker" }, { "mech", "walker" },
    { "three", "tripod" }, { "3", "tripod" }, { "four", "quad" }, { "4", "quad" }, { "beast", "quad" }, { "horse", "quad" }, { "spider", "many" },
    { "insect", "many" }, { "crab", "many" }, { "legs", "many" }, { "crocodile", "crawler" }, { "croc", "crawler" }, { "snake", "crawler" },
    { "lizard", "crawler" }, { "slug", "crawler" }, { "tank", "tracked" }, { "wheels", "tracked" }, { "flying", "hover" }, { "fly", "hover" },
    { "flyer", "hover" }, { "hovering", "hover" }, { "off", "none" },
};
/* mini_type(rng):
       k = str(os.environ.get("KIT_MINI") or CFG.get("miniature", "biped")).strip().lower(); k = MINI_ALIAS.get(k, k)
       if k in ("mix", "random", "any"): return (rng or _random).choice([m for m in MINIS if m != "none"])
       if k not in MINIS: raise SystemExit(...)
       return k
   raw = the str before strip / lower; ASCII whitespace and case only (the kit's names are ASCII). */
int sm_mini_type(const char *raw, PyRandom *rng, const char **out) {
    char k[128]; size_t a = 0, b = strlen(raw ? raw : ""), i, n; const char *kk;
    if (!raw) raw = "biped";
    while (a < b && isspace((unsigned char)raw[a])) a++;
    while (b > a && isspace((unsigned char)raw[b - 1])) b--;
    n = b - a; if (n > 127) n = 127;
    for (i = 0; i < n; i++) k[i] = (char)tolower((unsigned char)raw[a + i]);
    k[n] = 0; kk = k;
    for (i = 0; i < sizeof MINI_ALIAS / sizeof *MINI_ALIAS; i++) if (eq(MINI_ALIAS[i][0], k)) { kk = MINI_ALIAS[i][1]; break; }
    if (eq(kk, "mix") || eq(kk, "random") || eq(kk, "any")) {
        if (!rng) return SC_ERR;          /* the module random: not this port's */
        *out = MINIS[py_randbelow(rng, 10)]; return 0;
    }
    for (i = 0; i < sizeof MINIS / sizeof *MINIS; i++) if (eq(MINIS[i], kk)) { *out = MINIS[i]; return 0; }
    return SC_ERR;
}

/* ======================= Sc ======================= */
/* Sc.__init__(s, style, mixr, rng, t):
       self.s, self.style, self.mixr, self.rng, self.t = s, style, mixr, rng, t
       self.mm = gg.BASE_MM; self.K = gg.K; self.R = gg.R0; self.pal = PAL[style]
       self.wide = Point(0, 0).buffer(gg.R0 + 1.6, 128); self.disc = Point(0, 0).buffer(gg.R0 - 0.7, 128)
       self.core = None; self.subj = None; self.solid = Polygon(); self.c = (0.0, 0.0); self.front = (1.0, 0.0); self.p1 = (0.0, 0.0)
       self.mini = mini_type(mixr); self.wet = None
       self.pathg = None; self.line = None; self.start = None; self.yard = None; self.also = []; self.log = []; self.taken = []; self.focal = None; self.marks_later = [] */
int sm_sc_init(Sc *sc, Scene *s, const char *style, PyRandom *mixr, PyRandom *rng, int t, const ScPal *pal, const char *mini_raw) {
    memset(sc, 0, sizeof *sc);
    sc->s = s; sc->style = style; sc->mixr = mixr; sc->rng = rng; sc->t = t;
    sc->mm = SC.base_mm; sc->K = SC.K; sc->R = SC.R0;
    if (pal) sc->pal = *pal;
    sc->wide = geo_new(shp_point_buffer(0, 0, SC.R0 + 1.6, 128)); sc->disc = geo_new(shp_point_buffer(0, 0, SC.R0 - 0.7, 128));
    sc->solid = geo_new(shp_polygon(NULL, 0));
    sc->c[0] = 0.0; sc->c[1] = 0.0; sc->front[0] = 1.0; sc->front[1] = 0.0; sc->p1[0] = 0.0; sc->p1[1] = 0.0;
    if (!sc->wide || !sc->disc || !sc->solid) return SC_ERR;
    return sm_mini_type(mini_raw, mixr, &sc->mini);
}
void sm_sc_free(Sc *sc) {
    int i;
    geo_unref(sc->wide); geo_unref(sc->disc); geo_unref(sc->core); geo_unref(sc->subj); geo_unref(sc->solid);
    geo_unref(sc->wet); geo_unref(sc->lline); geo_unref(sc->pathg); geo_unref(sc->line); geo_unref(sc->yard); geo_unref(sc->focal);
    geo_unref(sc->floor); geo_unref(sc->floor_edge); geo_unref(sc->waters); geo_unref(sc->lines);
    gl_free(&sc->also); gl_free(&sc->taken); gl_free(&sc->bigs); gl_free(&sc->anchors); gl_free(&sc->tall_cells); gl_free(&sc->rim); gl_free(&sc->stand_pads);
    for (i = 0; i < sc->log.n; i++) free(sc->log.v[i]);
    free(sc->log.v); free(sc->marks_later.v); free(sc->mark_pool.v);
    memset(sc, 0, sizeof *sc);
}
