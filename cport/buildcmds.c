/* buildcmds.c: gg.build_cmds(s) and every helper it reaches, in C (see buildcmds.h). Each function names the Python
   routine it copies; the order of every GEOS call and every float operation is the Python one. */
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>
#include "buildcmds.h"
#include "shp.h"
#include "pyx.h"

#define DEEP 0.3                          /* gg.DEEP */
#define CAP_LAP 0.04                      /* gg.CAP_LAP */
#define DEG2RAD (3.141592653589793 / 180.0)          /* CPython's degToRad = Py_MATH_PI / 180.0 */
int bc_ambiguous_keys;

static double radians(double x) { return x * DEG2RAD; }            /* math.radians */
static double tan_deg(double a) { return py_tan(radians(a)); }     /* math.tan(math.radians(a)) */

/* ======================= numbers with their Python type ======================= */
static BcNum NUM(double v, int t) { BcNum n; n.v = v; n.t = (unsigned char)t; return n; }
static BcNum F(double v) { return NUM(v, BC_FLOAT); }
static BcNum I(double v) { return NUM(v, BC_INT); }
static int is_fl(int t) { return t == BC_FLOAT || t == BC_Z; }
/* the type of a + b, a - b, a * b: numpy.float64 wins (but Z.__add__ is float's, which answers first and gives a
   float), else any float gives a float, else int */
static int res_t(BcNum a, BcNum b) {
    if (a.t == BC_NP) return BC_NP;
    if (b.t == BC_NP) return a.t == BC_Z ? BC_FLOAT : BC_NP;
    if (is_fl(a.t) || is_fl(b.t)) return BC_FLOAT;
    return BC_INT;
}
static BcNum fix_int(BcNum n) { if (n.t == BC_INT) n.v += 0.0; return n; }          /* no -0.0 for an int */
static BcNum n_add(BcNum a, BcNum b) { return fix_int(NUM(a.v + b.v, res_t(a, b))); }
static BcNum n_sub(BcNum a, BcNum b) { return fix_int(NUM(a.v - b.v, res_t(a, b))); }
static BcNum n_mul(BcNum a, BcNum b) { return fix_int(NUM(a.v * b.v, res_t(a, b))); }
static BcNum n_neg(BcNum a) { return fix_int(NUM(-a.v, a.t == BC_NP ? BC_NP : is_fl(a.t) ? BC_FLOAT : BC_INT)); }
static BcNum n_max(BcNum a, BcNum b) { return b.v > a.v ? b : a; }          /* max(a, b): the first of equal values */
static BcNum n_min(BcNum a, BcNum b) { return b.v < a.v ? b : a; }          /* min(a, b) */
static double d_max(double a, double b) { return b > a ? b : a; }
/* round(x, nd): an int stays an int; a float is correctly rounded (float.__round__); numpy.float64.__round__ is
   numpy's rint(x * 10**nd) / 10**nd */
static BcNum n_round(BcNum a, int nd) {
    if (a.t == BC_INT || a.t == BC_BOOL) return I(a.v);
    if (a.t == BC_NP) { double f = nd == 3 ? 1000.0 : 10000.0; return NUM(rint(a.v * f) / f, BC_NP); }
    return F(py_round(a.v, nd));
}
static double rnd4(double x) { return py_round(x, 4); }

/* ======================= command values ======================= */
static BcVal v_none(void) { BcVal v; memset(&v, 0, sizeof v); return v; }
static BcVal v_num(BcNum n) { BcVal v = v_none(); v.t = BCV_NUM; v.num = n; return v; }
static BcVal v_f(double x) { return v_num(F(x)); }
static BcVal v_bool(int b) { return v_num(NUM(b ? 1 : 0, BC_BOOL)); }
static BcVal v_str(const char *s) { BcVal v = v_none(); v.t = BCV_STR; v.s = strdup(s); return v; }
static BcVal v_list(void) { BcVal v = v_none(); v.t = BCV_LIST; return v; }
static BcVal v_dict(void) { BcVal v = v_none(); v.t = BCV_DICT; return v; }
static BcVal v_pts(const double *xy, int n) {
    BcVal v = v_none(); v.t = BCV_PTS; v.n = n; v.dim = 2;
    v.pts = (double *)malloc(sizeof(double) * 2 * (size_t)(n ? n : 1)); memcpy(v.pts, xy, sizeof(double) * 2 * (size_t)n);
    return v;
}
static void v_grow(BcVal *l) {
    if (l->n < l->cap) return;
    l->cap = l->cap ? 2 * l->cap : 8;
    l->v = (BcVal *)realloc(l->v, sizeof(BcVal) * (size_t)l->cap);
    if (l->t == BCV_DICT) l->k = (const char **)realloc((void *)l->k, sizeof(char *) * (size_t)l->cap);
}
static void v_push(BcVal *l, BcVal x) { v_grow(l); l->v[l->n++] = x; }
static void v_free(BcVal *a);
static void d_set(BcVal *d, const char *key, BcVal x) {          /* d[key] = x (an existing key keeps its place) */
    for (int i = 0; i < d->n; i++) if (!strcmp(d->k[i], key)) { v_free(&d->v[i]); d->v[i] = x; return; }
    v_grow(d); d->k[d->n] = key; d->v[d->n++] = x;
}
static BcVal v_xy(double x, double y) { BcVal l = v_list(); v_push(&l, v_f(x)); v_push(&l, v_f(y)); return l; }
static BcVal v_strs1(const char *s) { BcVal l = v_list(); v_push(&l, v_str(s)); return l; }
static BcVal v_copy(const BcVal *a) {
    BcVal v = *a;
    if (a->t == BCV_STR) v.s = strdup(a->s);
    else if (a->t == BCV_PTS) { v.pts = (double *)malloc(sizeof(double) * (size_t)(a->n * a->dim + 1)); memcpy(v.pts, a->pts, sizeof(double) * (size_t)(a->n * a->dim)); }
    else if (a->t == BCV_LIST || a->t == BCV_DICT) {
        v.cap = a->n; v.v = a->n ? (BcVal *)malloc(sizeof(BcVal) * (size_t)a->n) : NULL; v.k = NULL;
        if (a->t == BCV_DICT && a->n) { v.k = (const char **)malloc(sizeof(char *) * (size_t)a->n); memcpy((void *)v.k, a->k, sizeof(char *) * (size_t)a->n); }
        for (int i = 0; i < a->n; i++) v.v[i] = v_copy(&a->v[i]);
    }
    return v;
}
static void v_free(BcVal *a) {
    if (a->t == BCV_STR) free(a->s);
    else if (a->t == BCV_PTS) free(a->pts);
    else if (a->t == BCV_LIST || a->t == BCV_DICT) { for (int i = 0; i < a->n; i++) v_free(&a->v[i]); free(a->v); free((void *)a->k); }
    *a = v_none();
}
const BcVal *bc_get(const BcVal *d, const char *key) {
    if (!d || d->t != BCV_DICT) return NULL;
    for (int i = 0; i < d->n; i++) if (!strcmp(d->k[i], key)) return &d->v[i];
    return NULL;
}
static BcVal cmd(const char *command, BcVal params) {         /* {"command": command, "params": params} */
    BcVal d = v_dict(); d_set(&d, "command", v_str(command)); d_set(&d, "params", params); return d;
}

/* ======================= command groups ======================= */
static BcGroup g_new(int is_group) { BcGroup g; memset(&g, 0, sizeof g); g.is_group = is_group; return g; }
static void g_push(BcGroup *g, BcVal c) {
    if (g->n == g->cap) { g->cap = g->cap ? 2 * g->cap : 8; g->cmds = (BcVal *)realloc(g->cmds, sizeof(BcVal) * (size_t)g->cap); }
    g->cmds[g->n++] = c;
}
static void g_free(BcGroup *g) {
    for (int i = 0; i < g->n; i++) v_free(&g->cmds[i]);
    free(g->cmds); v_free(&g->piece);
    for (int i = 0; i < g->nfb; i++) g_free(&g->fallback[i]);
    free(g->fallback);
    for (int i = 0; i < g->nalts; i++) g_free(&g->alts[i]);
    free(g->alts);
    memset(g, 0, sizeof *g);
}
static BcGroup g_copy(const BcGroup *a, int is_group) {      /* a new group holding copies of a's commands */
    BcGroup g = g_new(is_group);
    for (int i = 0; i < a->n; i++) g_push(&g, v_copy(&a->cmds[i]));
    return g;
}
static void gl_push(BcGroup **v, int *n, int *cap, BcGroup g) {
    if (*n == *cap) { *cap = *cap ? 2 * *cap : 4; *v = (BcGroup *)realloc(*v, sizeof(BcGroup) * (size_t)*cap); }
    (*v)[(*n)++] = g;
}
static void G_push(BcGroups *G, BcGroup g) { gl_push(&G->g, &G->n, &G->cap, g); }
void bc_groups_free(BcGroups *gs) { for (int i = 0; i < gs->n; i++) g_free(&gs->g[i]); free(gs->g); memset(gs, 0, sizeof *gs); }

/* ======================= scene objects ======================= */
BcOp *bc_op_new(int type, BcKind *kind, const BcNum *n, GEOSGeometry **polys, int np) {
    BcOp *o = (BcOp *)calloc(1, sizeof(BcOp));
    o->rc = 1; o->type = type; o->kind = kind; o->n[0] = n[0]; o->n[1] = n[1]; o->n[2] = type == BC_TAPER ? n[2] : F(0);
    o->polys = polys; o->np = np;
    return o;
}
void bc_op_unref(BcOp *o) {
    if (!o || --o->rc > 0) return;
    for (int i = 0; i < o->np; i++) shp_free(o->polys[i]);
    free(o->polys); free(o);
}
static void chop_free(BcChop *c) {
    if (!c) return;
    shp_free(c->poly);
    for (int i = 0; i < c->nops; i++) bc_op_unref(c->ops[i]);
    free(c->ops); free(c);
}
static void polys_free(GEOSGeometry **p, int n) { for (int i = 0; i < n; i++) shp_free(p[i]); free(p); }
void bc_scene_free(BcScene *s) {
    for (int i = 0; i < s->nops; i++) bc_op_unref(s->ops[i]);
    free(s->ops);
    for (int i = 0; i < s->ngcuts; i++) polys_free(s->gcuts[i].polys, s->gcuts[i].np);
    free(s->gcuts);
    for (int i = 0; i < s->ntcuts; i++) polys_free(s->tcuts[i].polys, s->tcuts[i].np);
    free(s->tcuts);
    polys_free(s->feet, s->nfeet);
    for (int i = 0; i < s->nchops; i++) chop_free(s->chops[i]);
    free(s->chops);
    for (int i = 0; i < s->nkinds; i++) { free(s->kinds[i]->s); free(s->kinds[i]); }
    free(s->kinds);
    for (int i = 0; i < s->cfg.nchamfer; i++) free(s->cfg.chamfer_prisms[i]);
    free(s->cfg.chamfer_prisms);
    for (int i = 0; i < s->cfg.npieces; i++) free(s->cfg.pieces[i]);
    free(s->cfg.pieces);
    memset(s, 0, sizeof *s);
}

/* kind == "text" (a str or an LK compares by text with a plain str) */
static int kind_is(const BcKind *k, const char *s) { return !strcmp(k->s, s); }
/* a == b for two kind objects: LK.__eq__ (identity between two LKs), else str equality */
static int kind_eq(const BcKind *a, const BcKind *b) { return a == b || (!(a->lk && b->lk) && !strcmp(a->s, b->s)); }
static const char *const PIECES0[] = { "rock", "pebble", "grit", "rubble", "crate", "pustule", "obelisk", "flower", "spire", "quartz", "lens", "post", NULL };
static const char *const CHAMFER_PRISMS0[] = { "slate", "bedrock", "rock", "brick", "concrete", "sandstone", "rubble", NULL };
/* kind in gg.PIECES / gg.CHAMFER_PRISMS (an LK is found by its text, as str's set lookup compares it) */
static int in_list(const BcKind *k, const char *const *set, int n) { for (int i = 0; n < 0 ? set[i] != NULL : i < n; i++) if (kind_is(k, set[i])) return 1; return 0; }
static int in_pieces(const BcCfg *c, const BcKind *k) { return c->pieces ? in_list(k, (const char *const *)c->pieces, c->npieces) : in_list(k, PIECES0, -1); }
static int in_chamfer(const BcCfg *c, const BcKind *k) { return c->chamfer_prisms ? in_list(k, (const char *const *)c->chamfer_prisms, c->nchamfer) : in_list(k, CHAMFER_PRISMS0, -1); }

/* ======================= geometry helpers ======================= */
/* list(ring.coords) as x, y pairs (malloc'd); returns the count, -1 if GEOS refuses */
static int ring_xy(const GEOSGeometry *ring, double **xy) {
    const GEOSCoordSequence *cs; unsigned int n = 0;
    *xy = NULL;
    if (!ring || !(cs = G.GEOSGeom_getCoordSeq_r(gctx, ring)) || !G.GEOSCoordSeq_getSize_r(gctx, cs, &n)) return -1;
    *xy = (double *)malloc(sizeof(double) * 2 * (n ? n : 1));
    for (unsigned int i = 0; i < n; i++) G.GEOSCoordSeq_getXY_r(gctx, cs, i, &(*xy)[2 * i], &(*xy)[2 * i + 1]);
    return (int)n;
}
static int ext_xy(const GEOSGeometry *p, double **xy) { const GEOSGeometry *r = shp_exterior(p); return r ? ring_xy(r, xy) : (*xy = NULL, -1); }
static int ncoords(const GEOSGeometry *p) { double *xy; int n = ext_xy(p, &xy); free(xy); return n; }
/* max(polys_of(g), key=lambda x: x.area): a clone of the first largest; NULL if there is none (ValueError) */
static GEOSGeometry *largest(const GEOSGeometry *g) {
    ShpList l = { 0 }; GEOSGeometry *best = NULL; double ba = 0;
    shp_polys_of(g, &l);
    for (int i = 0; i < l.n; i++) { double a = shp_area(l.g[i]); if (!best || a > ba) { best = l.g[i]; ba = a; } }
    if (best) best = shp_clone(best);
    shp_list_free(&l);
    return best;
}
static GEOSGeometry *empty_polygon(void) { return shp_polygon(NULL, 0); }          /* Polygon() */
static GEOSGeometry **clone_all(GEOSGeometry *const *p, int n) {
    GEOSGeometry **o = (GEOSGeometry **)malloc(sizeof(GEOSGeometry *) * (size_t)(n ? n : 1));
    for (int i = 0; i < n; i++) o[i] = shp_clone(p[i]);
    return o;
}
static GEOSGeometry **take_list(ShpList *l, int *n) {          /* the list's array, taken over */
    GEOSGeometry **o = l->g ? l->g : (GEOSGeometry **)malloc(sizeof(GEOSGeometry *)); *n = l->n;
    l->g = NULL; l->n = l->cap = 0;
    return o;
}

/* ======================= gg: wall, rim, clip ======================= */
/* gg._wall_r(z): radius of the finished 12 deg wall at height z */
static double wall_r(const BcCfg *c, double z) { return c->R0 - (0 > z ? 0.0 : z) * tan_deg(c->RIM); }
/* gg._rim(z, k) */
static double rim_r(const BcCfg *c, double z, int k) { return wall_r(c, z) - c->rim_gap - 0.03 * k; }
/* gg._clip(polys, z, need, k): appended to out. BC_ERR when shapely raises. */
static int clip(const BcCfg *c, GEOSGeometry *const *polys, int n, double z, double need, int k, ShpList *out) {
    GEOSGeometry *disc = shp_point_buffer(0, 0, rim_r(c, z, k), 96);
    if (!disc) return BC_ERR;
    for (int i = 0; i < n; i++) {
        GEOSGeometry *q = shp_intersection(polys[i], disc); ShpList parts = { 0 };
        if (!q) { shp_free(disc); return BC_ERR; }
        shp_polys_of(q, &parts); shp_free(q);
        for (int j = 0; j < parts.n; j++) {
            GEOSGeometry *x = parts.g[j]; int keep = shp_area(x) > 1e-3;
            if (keep && !(need <= 0)) {
                GEOSGeometry *b = shp_buffer_q(x, -(need + 0.03), 16);
                if (!b) { shp_list_free(&parts); shp_free(disc); return BC_ERR; }
                keep = !shp_is_empty(b); shp_free(b);
            }
            if (keep) { shp_list_push(out, x); parts.g[j] = NULL; }
        }
        shp_list_free(&parts);
    }
    shp_free(disc);
    return 0;
}
/* gg.clip_z(op) */
static double clip_z(const BcCfg *c, const BcOp *op) { return op->type == BC_PRISM ? d_max(op->n[1].v, c->H) : d_max(op->n[0].v, c->H); }
/* gg.op_clip(op) (op_k(op) = op->rk) */
static int op_clip(const BcCfg *c, const BcOp *op, ShpList *out) {
    double need = op->type == BC_PRISM ? 0.0 : op->n[1].v * tan_deg(op->n[2].v);
    return clip(c, op->polys, op->np, clip_z(c, op), need, op->rk, out);
}

/* ======================= gg: sketches ======================= */
static void push_rounded_pts(BcGroup *g, const double *xy, int n, int closed) {        /* sketch.line of rounded points */
    double *r = (double *)malloc(sizeof(double) * 2 * (size_t)(n ? n : 1));
    for (int i = 0; i < 2 * n; i++) r[i] = rnd4(xy[i]);
    BcVal p = v_dict(); d_set(&p, "points", v_pts(r, n)); d_set(&p, "closed", v_bool(closed));
    g_push(g, cmd("sketch.line", p)); free(r);
}
static int cmp_xy(const void *a, const void *b) {
    const double *p = (const double *)a, *q = (const double *)b;
    if (p[0] < q[0]) return -1;
    if (p[0] > q[0]) return 1;
    return p[1] < q[1] ? -1 : p[1] > q[1] ? 1 : 0;
}
static int n_distinct(const double *xy, int n) {          /* len(set(coords)) */
    double *c = (double *)malloc(sizeof(double) * 2 * (size_t)(n ? n : 1)); int k = 0;
    memcpy(c, xy, sizeof(double) * 2 * (size_t)n);
    qsort(c, (size_t)n, 2 * sizeof(double), cmp_xy);
    for (int i = 0; i < n; i++) if (i == 0 || !(c[2 * i] == c[2 * i - 2] && c[2 * i + 1] == c[2 * i - 1])) k++;
    free(c);
    return k;
}
typedef struct { double *xy; int n, cap; } Pts;
static void pts_push(Pts *p, double x, double y) {
    if (p->n == p->cap) { p->cap = p->cap ? 2 * p->cap : 16; p->xy = (double *)realloc(p->xy, sizeof(double) * 2 * (size_t)p->cap); }
    p->xy[2 * p->n] = x; p->xy[2 * p->n + 1] = y; p->n++;
}
typedef struct { int arc; Pts line; double a[2], m[2], b[2]; } Seg;
static void proj(const double *q, double rim, double *o) {          /* _arc_loop.proj */
    double r = py_hypot(q[0], q[1]);
    if (r == 0) r = 1;
    o[0] = q[0] * rim / r; o[1] = q[1] * rim / r;
}
/* gg._arc_loop(coords, rim): commands appended to out; returns how many (0 = None or an empty list), BC_ERR */
static int arc_loop(BcGroup *out, const double *coords, int nc, double rim, double tol, int noarc) {
    int n = nc > 0 ? nc - 1 : 0, k0, i, j, ns = 0, cs = 0, any_on = 0, all_on = 1, has_arc = 0, added = 0, rc = 0;
    double *P; char *on, *ON; Seg *seg = NULL; Pts cur = { 0 };
    if (noarc) return 0;
    on = (char *)malloc((size_t)n + 1);
    for (i = 0; i < n; i++) { on[i] = fabs(py_hypot(coords[2 * i], coords[2 * i + 1]) - rim) < 0.006; any_on |= on[i]; all_on &= on[i]; }
    if (n < 4 || all_on || !any_on) { free(on); return 0; }
    for (k0 = 0; on[k0]; k0++) {}
    P = (double *)malloc(sizeof(double) * 2 * (size_t)n); ON = (char *)malloc((size_t)n);
    for (i = 0; i < n; i++) { int s = (k0 + i) % n; P[2 * i] = coords[2 * s]; P[2 * i + 1] = coords[2 * s + 1]; ON[i] = on[s]; }
#define SEG_NEW() (ns == cs ? (cs = cs ? 2 * cs : 8, seg = (Seg *)realloc(seg, sizeof(Seg) * (size_t)cs)) : seg, memset(&seg[ns], 0, sizeof(Seg)), &seg[ns++])
    i = 0;
    while (i < n) {
        if (ON[i]) {
            j = i;
            while (j + 1 < n && ON[j + 1]) j++;
            if (j - i >= 2) {
                double a[2], m[2], b[2]; Seg *s;
                proj(&P[2 * i], rim, a); proj(&P[2 * ((i + j) / 2)], rim, m); proj(&P[2 * j], rim, b);
                if (cur.n) { pts_push(&cur, a[0], a[1]); s = SEG_NEW(); s->line = cur; memset(&cur, 0, sizeof cur); }
                else {
                    double st[2] = { a[0], a[1] };
                    if (ns && seg[ns - 1].arc) { st[0] = seg[ns - 1].b[0]; st[1] = seg[ns - 1].b[1]; }
                    s = SEG_NEW(); pts_push(&s->line, st[0], st[1]); pts_push(&s->line, a[0], a[1]);
                }
                s = SEG_NEW(); s->arc = 1; memcpy(s->a, a, sizeof a); memcpy(s->m, m, sizeof m); memcpy(s->b, b, sizeof b);
                memset(&cur, 0, sizeof cur); pts_push(&cur, b[0], b[1]); has_arc = 1;
                i = j + 1; continue;
            }
            for (int t = i; t <= j; t++) pts_push(&cur, P[2 * t], P[2 * t + 1]);
            i = j + 1; continue;
        }
        pts_push(&cur, P[2 * i], P[2 * i + 1]); i++;
    }
    if (!has_arc) goto done;
    if (cur.n) {
        if (!seg[0].arc) pts_push(&cur, seg[0].line.xy[0], seg[0].line.xy[1]);
        else pts_push(&cur, seg[0].a[0], seg[0].a[1]);
        Seg *s = SEG_NEW(); s->line = cur; memset(&cur, 0, sizeof cur);
    }
#undef SEG_NEW
    for (int q = 0; q < ns; q++) {
        if (!seg[q].arc) {
            GEOSGeometry *ln = shp_linestring(seg[q].line.xy, seg[q].line.n), *sl;
            double *xy; int m;
            if (!ln) { rc = BC_ERR; goto done; }
            if (tol) { sl = shp_simplify(ln, tol, 1); shp_free(ln); ln = sl; if (!ln) { rc = BC_ERR; goto done; } }
            m = ring_xy(ln, &xy); shp_free(ln);
            if (m < 0) { free(xy); rc = BC_ERR; goto done; }
            for (int t = 0; t < 2 * m; t++) xy[t] = rnd4(xy[t]);
            if (m >= 2 && (xy[0] != xy[2 * m - 2] || xy[1] != xy[2 * m - 1])) {
                BcVal p = v_dict(); d_set(&p, "points", v_pts(xy, m)); d_set(&p, "closed", v_bool(0));
                g_push(out, cmd("sketch.line", p)); added++;
            }
            free(xy);
        } else {
            BcVal p = v_dict();
            d_set(&p, "start", v_xy(rnd4(seg[q].a[0]), rnd4(seg[q].a[1])));
            d_set(&p, "through", v_xy(rnd4(seg[q].m[0]), rnd4(seg[q].m[1])));
            d_set(&p, "end", v_xy(rnd4(seg[q].b[0]), rnd4(seg[q].b[1])));
            g_push(out, cmd("sketch.arc.three_point", p)); added++;
        }
    }
    rc = added;
done:
    for (int q = 0; q < ns; q++) free(seg[q].line.xy);
    free(seg); free(cur.xy); free(P); free(ON); free(on);
    return rc;
}
/* gg.loops(p): the rings' points, rounded, without the closing point, as closed sketch lines */
static int loops(BcGroup *g, const GEOSGeometry *p) {
    double *xy; int n = ext_xy(p, &xy), nh;
    if (n < 0) { free(xy); return BC_ERR; }
    push_rounded_pts(g, xy, n > 0 ? n - 1 : 0, 1); free(xy);
    nh = shp_num_interiors(p);
    for (int i = 0; i < nh; i++) {
        n = ring_xy(shp_interior(p, i), &xy);
        if (n < 0) { free(xy); return BC_ERR; }
        push_rounded_pts(g, xy, n > 0 ? n - 1 : 0, 1); free(xy);
    }
    return 0;
}
static BcVal create_params(BcNum offset, const char *name) {
    BcVal p = v_dict(); d_set(&p, "plane", v_str("XY")); d_set(&p, "offset", v_num(offset)); d_set(&p, "name", v_str(name));
    return p;
}
/* gg.sk(name, z, polys, rim) with gg.SKETCH_TOL = tol; rim NAN = None. Appended to g. */
static int sk(BcGroup *g, const BcCfg *c, const char *name, BcNum z, GEOSGeometry *const *polys, int n, double rim, double tol) {
    g_push(g, cmd("sketch.create", create_params(n_round(z, 4), name)));
    for (int i = 0; i < n; i++) {
        const GEOSGeometry *p = polys[i]; GEOSGeometry *q = NULL; double *xy; int m, r;
        if (shp_area(p) < 0.01) continue;
        m = ext_xy(p, &xy);
        if (m < 0) { free(xy); return BC_ERR; }
        if (n_distinct(xy, m) < 4) { free(xy); continue; }
        if (!isnan(rim) && rim != 0 && shp_num_interiors(p) == 0) {
            r = arc_loop(g, xy, m, rim, tol, c->noarc);
            if (r < 0) { free(xy); return BC_ERR; }
            if (r > 0) { free(xy); continue; }
        }
        free(xy);
        if (tol) {
            q = shp_simplify(p, tol, 1);
            if (!q) return BC_ERR;
            if (shp_is_empty(q) || shp_type(q) != GEOS_POLYGON) { shp_free(q); q = NULL; }
        }
        r = loops(g, q ? q : p); shp_free(q);
        if (r < 0) return BC_ERR;
    }
    g_push(g, cmd("sketch.finish", v_dict()));
    return 0;
}
/* gg.sk_circle(name, z, r) */
static void sk_circle(BcGroup *g, const char *name, BcNum z, double r) {
    BcVal p = v_dict(), ctr = v_list();
    g_push(g, cmd("sketch.create", create_params(n_round(z, 4), name)));
    v_push(&ctr, v_num(I(0))); v_push(&ctr, v_num(I(0)));
    d_set(&p, "center", ctr); d_set(&p, "radius", v_f(rnd4(r)));
    g_push(g, cmd("sketch.circle.center", p));
    g_push(g, cmd("sketch.finish", v_dict()));
}
/* gg.magnet_centres(dia) */
static int magnet_centres(double dia, double *xy) {
    int n = dia <= 40 ? 1 : dia <= 60 ? 2 : dia <= 100 ? 3 : 4, k = 0;
    const double pi = 3.141592653589793;
    if (n <= 1) { xy[0] = 0.0; xy[1] = 0.0; return 1; }
    double r = dia * 0.25;
    for (int i = 0; i < n; i++) {
        double a = pi / 2 + 2 * pi * i / n;
        xy[2 * k] = rnd4(r * py_cos(a)); xy[2 * k + 1] = rnd4(r * py_sin(a)); k++;
    }
    if (dia > 100) { xy[2 * k] = 0.0; xy[2 * k + 1] = 0.0; k++; }
    return k;
}
/* gg.mag_sketch() + the MagCut extrude */
static void mag_group(BcGroup *g, const BcCfg *c) {
    double xy[10]; int n = magnet_centres(c->base_mm, xy);
    g_push(g, cmd("sketch.create", create_params(F(2.1), "Mag")));
    for (int i = 0; i < n; i++) { BcVal p = v_dict(); d_set(&p, "center", v_xy(xy[2 * i], xy[2 * i + 1])); d_set(&p, "radius", v_f(3.1)); g_push(g, cmd("sketch.circle.center", p)); }
    g_push(g, cmd("sketch.finish", v_dict()));
    BcVal e = v_dict();
    d_set(&e, "sketch", v_str("Mag")); d_set(&e, "distance", v_f(2.2)); d_set(&e, "direction", v_str("negative"));
    d_set(&e, "operation", v_str("cut")); d_set(&e, "targets", v_strs1("Base")); d_set(&e, "name", v_str("MagCut"));
    g_push(g, cmd("solid.extrude", e));
}
/* {"sketch", "distance", ["taper"], ["direction": "negative"], "operation", ["targets": [target]], "name", ["body_name"]} */
static BcVal extrude(const char *sketch, BcNum distance, const BcNum *taper, int negative, const char *operation, const char *target, const char *name, const char *body) {
    BcVal e = v_dict();
    d_set(&e, "sketch", v_str(sketch)); d_set(&e, "distance", v_num(distance));
    if (taper) d_set(&e, "taper", v_num(*taper));
    if (negative) d_set(&e, "direction", v_str("negative"));
    d_set(&e, "operation", v_str(operation));
    if (target) d_set(&e, "targets", v_strs1(target));
    d_set(&e, "name", v_str(name));
    if (body) d_set(&e, "body_name", v_str(body));
    return cmd("solid.extrude", e);
}

/* ======================= gg.piece_group / _prism_groups ======================= */
/* gg.piece_group(body, sketch_cmds, distance, top, d, fallback, poly): skc and the fallback groups are taken over */
static int piece_group(BcGroups *Gs, const char *body, BcGroup *skc, BcNum distance, BcNum top, BcNum d, BcGroup *fb, int nfb, const GEOSGeometry *poly) {
    const BcVal *p0 = bc_get(&skc->cmds[0], "params");
    char sk_name[64], nm[80], nmc[80], skc_name[72];
    BcGroup grp;
    snprintf(sk_name, sizeof sk_name, "%s", bc_get(p0, "name")->s);
    if (poly) {
        double pa = shp_area(poly);
        while (d.v >= 0.05) {
            GEOSGeometry *q = shp_inset(poly, d.v + 0.06); int e;
            if (!q) return BC_ERR;
            e = shp_is_empty(q);
            if (!e && !(shp_area(q) < 0.25 * pa)) { shp_free(q); break; }
            shp_free(q);
            d = n_mul(d, F(0.6));
        }
    }
    snprintf(nm, sizeof nm, "F%s", body);
    if (!poly || d.v < 0.05 || n_sub(distance, d).v < 0.1) {
        grp = *skc; grp.is_group = 1;
        g_push(&grp, extrude(sk_name, n_round(distance, 4), NULL, 0, "new", NULL, nm, body));
    } else {
        BcGroup cap = g_copy(skc, 0); BcVal np = v_copy(p0);
        snprintf(skc_name, sizeof skc_name, "%sc", sk_name); snprintf(nmc, sizeof nmc, "F%sc", body);
        d_set(&np, "name", v_str(skc_name));
        d_set(&np, "offset", v_num(n_round(n_sub(n_sub(top, d), F(CAP_LAP)), 4)));
        v_free(&cap.cmds[0]); cap.cmds[0] = cmd("sketch.create", np);
        grp = *skc; grp.is_group = 1;
        g_push(&grp, extrude(sk_name, n_round(n_sub(distance, d), 4), NULL, 0, "new", NULL, nm, body));
        for (int i = 0; i < cap.n; i++) g_push(&grp, cap.cmds[i]);
        free(cap.cmds);
        BcNum t45 = F(-45.0); BcVal e = v_dict();
        d_set(&e, "sketch", v_str(skc_name)); d_set(&e, "distance", v_num(n_round(n_add(d, F(CAP_LAP)), 4))); d_set(&e, "taper", v_num(t45));
        d_set(&e, "operation", v_str("join")); d_set(&e, "targets", v_strs1(body)); d_set(&e, "name", v_str(nmc));
        g_push(&grp, cmd("solid.extrude", e));
    }
    memset(skc, 0, sizeof *skc);
    grp.piece = v_dict();
    d_set(&grp.piece, "body", v_str(body)); d_set(&grp.piece, "top", v_num(n_round(top, 4))); d_set(&grp.piece, "dist", v_list());
    grp.fallback = fb; grp.nfb = grp.capfb = nfb;
    G_push(Gs, grp);
    return 0;
}

/* gg._prism_groups(i, op) */
static int prism_groups(BcGroups *Gs, const BcCfg *c, int i, const BcOp *op) {
    BcNum vis = n_sub(op->n[1], n_max(n_add(op->n[0], F(0.15)), F(c->H)));
    int rk = op->rk, rc = 0; ShpList parts = { 0 }; double cz = clip_z(c, op);
    if (clip(c, op->polys, op->np, cz, 0.0, rk, &parts) < 0) { shp_list_free(&parts); return BC_ERR; }
    for (int k = 0; k < parts.n; k++) {
        GEOSGeometry *p = parts.g[k]; char nm[64], fnm[64], pnm[64]; double pa = shp_area(p), rimv, mx = 0; double *xy; int m;
        BcGroup plain, sk2;
        if (pa < 0.01) continue;
        rimv = rim_r(c, cz, rk);
        snprintf(nm, sizeof nm, "P%d_%d", i, k); snprintf(fnm, sizeof fnm, "FP%d_%d", i, k); snprintf(pnm, sizeof pnm, "PP%d_%d", i, k);
        plain = g_new(0);
        if (sk(&plain, c, nm, n_sub(op->n[0], F(DEEP)), &p, 1, rimv, c->sketch_tol) < 0) { g_free(&plain); rc = BC_ERR; break; }
        g_push(&plain, extrude(nm, n_round(n_add(n_sub(op->n[1], op->n[0]), F(DEEP)), 4), NULL, 0, "join", "Base", fnm, NULL));
        m = ext_xy(p, &xy);
        for (int t = 0; t < m; t++) { double h = py_hypot(xy[2 * t], xy[2 * t + 1]); if (t == 0 || h > mx) mx = h; }
        free(xy);
        if (vis.v < 0.6 || pa < 0.8 || mx > rimv - 0.01) { plain.is_group = 1; G_push(Gs, plain); continue; }
        BcNum d = n_round(n_min(F(0.3), n_mul(F(0.35), vis)), 4);
        sk2 = g_new(0);
        if (sk(&sk2, c, nm, n_sub(op->n[0], F(DEEP)), &p, 1, rimv, c->sketch_tol) < 0) { g_free(&sk2); g_free(&plain); rc = BC_ERR; break; }
        BcGroup *fb = (BcGroup *)malloc(sizeof(BcGroup)); fb[0] = plain;
        if (piece_group(Gs, pnm, &sk2, n_add(n_sub(op->n[1], op->n[0]), F(DEEP)), op->n[1], d, fb, 1, p) < 0) { rc = BC_ERR; break; }
    }
    shp_list_free(&parts);
    return rc;
}

/* ======================= Scene.final_gcuts ======================= */
static void gcuts_free(BcGcut *g, int n) { for (int i = 0; i < n; i++) polys_free(g[i].polys, g[i].np); free(g); }
/* gg.Scene.final_gcuts / gg2.Scene.final_gcuts: a new list of cuts (owned) */
static int final_gcuts(const BcScene *s, BcGcut **out, int *nout) {
    const BcCfg *c = &s->cfg; GEOSGeometry *feet; int n = 0;
    double gap = c->gg2 ? d_max(c->has_cutgap ? c->cutgap.v : 0.05, 0.35) : (c->base_first ? 0.35 : 0.05);
    *out = (BcGcut *)calloc((size_t)(s->ngcuts ? s->ngcuts : 1), sizeof(BcGcut)); *nout = 0;
    if (s->nfeet) {
        GEOSGeometry *u = shp_union_all((const GEOSGeometry *const *)s->feet, s->nfeet);
        if (!u) return BC_ERR;
        feet = shp_buffer_q(u, gap, 16); shp_free(u);
    } else feet = empty_polygon();
    if (!feet) return BC_ERR;
    for (int i = 0; i < s->ngcuts; i++) {
        GEOSGeometry *u = shp_union_all((const GEOSGeometry *const *)s->gcuts[i].polys, s->gcuts[i].np), *g;
        ShpList parts = { 0 }, ps = { 0 };
        if (!u) goto fail;
        g = shp_difference(u, feet); shp_free(u);
        if (!g) goto fail;
        shp_polys_of(g, &parts); shp_free(g);
        for (int j = 0; j < parts.n; j++) {
            GEOSGeometry *p = parts.g[j], *b;
            if (!(shp_area(p) > 0.1)) continue;
            b = shp_buffer_q(p, -0.1, 16);
            if (!b) { shp_list_free(&parts); shp_list_free(&ps); goto fail; }
            int e = shp_is_empty(b); shp_free(b);
            if (e) continue;
            GEOSGeometry *q = shp_simplify(p, 0.01, 1);
            if (!q) { shp_list_free(&parts); shp_list_free(&ps); goto fail; }
            shp_list_push(&ps, q);
        }
        shp_list_free(&parts);
        if (ps.n) { BcGcut *o = &(*out)[n++]; o->polys = take_list(&ps, &o->np); o->depth = s->gcuts[i].depth; o->kind = s->gcuts[i].kind; *nout = n; }
    }
    shp_free(feet);
    return 0;
fail:
    shp_free(feet);
    return BC_ERR;
}

/* ======================= gg._prune ======================= */
/* bad(p, kind, z0, top) inside _prune: 1 / 0, BC_ERR */
static int prune_bad(const BcScene *s, const GEOSGeometry *p, const BcKind *kind, double z0, double top, const GEOSGeometry *gcu) {
    const BcCfg *c = &s->cfg; int r;
    if (!in_pieces(c, kind) || shp_area(p) >= 12) return 0;
    GEOSGeometry *disc = shp_point_buffer(0, 0, wall_r(c, d_max(top, c->H)) - 0.3, 64);
    if (!disc) return BC_ERR;
    r = shp_contains(disc, p); shp_free(disc);
    if (r < 0) return BC_ERR;
    if (!r) return 1;
    if (z0 <= c->H + 0.05) { r = shp_intersects(p, gcu); if (r) return r; }
    for (int i = 0; i < s->ntcuts; i++) {
        double a = s->tcuts[i].z0.v, b = s->tcuts[i].z1.v;
        if (!(b > z0 + 0.05 && a < z0 + 0.05)) continue;
        for (int j = 0; j < s->tcuts[i].np; j++) { r = shp_intersects(s->tcuts[i].polys[j], p); if (r) return r; }
    }
    return 0;
}
static int prune(BcScene *s) {
    BcGcut *fg; int nfg, rc = BC_ERR, nd = 0, nnew = 0, nkeep = 0;
    ShpList gc = { 0 }; GEOSGeometry *gcu = NULL;
    const GEOSGeometry **dropped = NULL; BcOp **nw = NULL; BcChop **keep = NULL;
    if (final_gcuts(s, &fg, &nfg) < 0) return BC_ERR;
    for (int i = 0; i < nfg; i++) for (int j = 0; j < fg[i].np; j++) { shp_list_push(&gc, fg[i].polys[j]); fg[i].polys[j] = NULL; }
    if (gc.n) { GEOSGeometry *u = shp_union_all((const GEOSGeometry *const *)gc.g, gc.n); if (u) { gcu = shp_buffer_q(u, 0.1, 16); shp_free(u); } }
    else gcu = empty_polygon();
    if (!gcu) goto out;
    int cap = 16; for (int i = 0; i < s->nops; i++) cap += s->ops[i]->np;
    cap += s->nchops;
    dropped = (const GEOSGeometry **)malloc(sizeof(GEOSGeometry *) * (size_t)cap);
    keep = (BcChop **)malloc(sizeof(BcChop *) * (size_t)(s->nchops + 1));
    for (int i = 0; i < s->nchops; i++) {
        BcChop *c = s->chops[i];
        int b = prune_bad(s, c->poly, c->kind, c->z0.v, c->top.v, gcu);
        if (b < 0) goto out;
        if (b) dropped[nd++] = c->poly; else keep[nkeep++] = c;
    }
    nw = (BcOp **)malloc(sizeof(BcOp *) * (size_t)(s->nops + 1));
    for (int i = 0; i < s->nops; i++) {
        BcOp *op = s->ops[i]; const BcKind *kind = op->kind;
        if (kind_is(kind, "rockc")) { op->rc++; nw[nnew++] = op; continue; }
        double z0 = op->n[0].v, top = op->type == BC_PRISM ? op->n[1].v : n_add(op->n[0], op->n[1]).v;
        GEOSGeometry **ok = (GEOSGeometry **)malloc(sizeof(GEOSGeometry *) * (size_t)(op->np + 1)); int nok = 0;
        for (int j = 0; j < op->np; j++) {
            GEOSGeometry *p = op->polys[j], *pt = shp_representative_point(p); int skip = 0, b;
            if (!pt) { free(ok); goto out; }
            if (in_pieces(&s->cfg, kind))
                for (int t = 0; t < nd && !skip; t++) { int r = shp_contains(dropped[t], pt); if (r < 0) { shp_free(pt); free(ok); goto out; } skip = r; }
            shp_free(pt);
            if (skip) continue;
            b = prune_bad(s, p, kind, z0, top, gcu);
            if (b < 0) { free(ok); goto out; }
            if (b) { dropped[nd++] = p; continue; }
            ok[nok++] = shp_clone(p);
        }
        if (nok) nw[nnew++] = bc_op_new(op->type, op->kind, op->n, ok, nok); else free(ok);
    }
    for (int i = 0; i < s->nops; i++) bc_op_unref(s->ops[i]);
    free(s->ops); s->ops = nw; s->nops = nnew; nw = NULL;
    /* iso: the chop touches no raised piece and no other chop */
    for (int i = 0; i < nkeep; i++) {
        BcChop *c = keep[i]; GEOSGeometry *b = shp_buffer_q(c->poly, 0.05, 16); int hit = 0;
        if (!b) goto out;
        for (int k = 0; k < s->nops && !hit; k++) {
            const BcOp *op = s->ops[k];
            if (kind_is(op->kind, "rockc") || kind_is(op->kind, "grit")) continue;
            for (int j = 0; j < op->np && !hit; j++) { int r = shp_intersects(b, op->polys[j]); if (r < 0) { shp_free(b); goto out; } hit = r; }
        }
        for (int k = 0; k < nkeep && !hit; k++) {
            if (keep[k] == c) continue;
            int r = shp_intersects(b, keep[k]->poly); if (r < 0) { shp_free(b); goto out; } hit = r;
        }
        shp_free(b);
        c->has_iso = 1; c->iso = NUM(!hit, BC_BOOL);
    }
    for (int i = 0; i < s->nchops; i++) {
        int kept = 0;
        for (int k = 0; k < nkeep; k++) kept |= keep[k] == s->chops[i];
        if (!kept) chop_free(s->chops[i]);
    }
    free(s->chops); s->chops = keep; s->nchops = nkeep; keep = NULL;
    rc = 0;
out:
    if (nw) { for (int i = 0; i < nnew; i++) bc_op_unref(nw[i]); free(nw); }
    free(keep); free((void *)dropped);
    shp_free(gcu); shp_list_free(&gc); gcuts_free(fg, nfg);
    return rc;
}

/* ======================= gg.fit_for_taper ======================= */
/* gg.edges_survive(p, d): 1 / 0, BC_ERR */
static int edges_survive(const GEOSGeometry *p, double d) {
    GEOSGeometry *q = shp_inset(p, d); int r;
    if (!q) return BC_ERR;
    r = !shp_is_empty(q) && shp_type(q) == GEOS_POLYGON && ncoords(q) == ncoords(p);
    shp_free(q);
    return r;
}
/* gg.fit_for_taper(p, d): *res = the fitted polygon (new) or NULL for None */
static int fit_for_taper(const GEOSGeometry *p0, double d, GEOSGeometry **res) {
    GEOSGeometry *p, *q;
    *res = NULL;
    if (!p0 || shp_is_empty(p0) || shp_type(p0) != GEOS_POLYGON) return 0;
    q = shp_polygon_from_ring(shp_exterior(p0));
    if (!q) return BC_ERR;
    p = shp_simplify(q, 0.05, 1); shp_free(q);
    if (!p) return BC_ERR;
    for (int it = 0; it < 400; it++) {
        int es = edges_survive(p, d), n, k = 0, del, v; double *c, best = 0;
        if (es < 0) { shp_free(p); return BC_ERR; }
        if (es) { *res = p; return 0; }
        n = ext_xy(p, &c) - 1;
        if (n <= 4) { free(c); shp_free(p); return 0; }
        for (int i = 0; i < n; i++) {
            int j = (i + 1) % n; double e = py_hypot(c[2 * i] - c[2 * j], c[2 * i + 1] - c[2 * j + 1]);
            if (i == 0 || e < best) { best = e; k = i; }
        }
        del = (k + 1) % n;
        memmove(&c[2 * del], &c[2 * del + 2], sizeof(double) * 2 * (size_t)(n - del - 1));
        shp_free(p);
        p = shp_polygon(c, n - 1); free(c);
        if (!p) return BC_ERR;
        v = shp_is_valid(p);
        if (v < 0) { shp_free(p); return BC_ERR; }
        if (!v) { shp_free(p); return 0; }
    }
    shp_free(p);
    return 0;
}

/* ======================= gg._group_ops ======================= */
typedef struct { int keep; BcOp *op; int type; BcNum a, b, c; BcKind *kind; BcOp **ops; int n, cap; } Key;
static int key_eq(const Key *x, const Key *y) {
    if (x->keep || y->keep) return x->keep && y->keep && x->op == y->op;
    return x->type == y->type && x->a.v == y->a.v && x->b.v == y->b.v && x->c.v == y->c.v && kind_eq(x->kind, y->kind);
}
static int group_ops(BcScene *s) {
    const BcCfg *c = &s->cfg; Key *keys; int nk = 0, nn = 0, rc = BC_ERR;
    BcOp **nw;
    if (!c->group_pieces) return 0;
    keys = (Key *)calloc((size_t)(s->nops + 1), sizeof(Key));
    for (int i = 0; i < s->nops; i++) {
        BcOp *op = s->ops[i]; Key k; int found = -1, matches = 0;
        memset(&k, 0, sizeof k);
        if (kind_is(op->kind, "rockc")) { k.keep = 1; k.op = op; }
        else { k.type = op->type; k.a = n_round(op->n[0], 3); k.b = n_round(op->n[1], 3); k.c = op->type == BC_TAPER ? n_round(op->n[2], 3) : I(0); k.kind = op->kind; }
        for (int j = 0; j < nk; j++) if (key_eq(&keys[j], &k)) { if (found < 0) found = j; matches++; }
        if (matches > 1) bc_ambiguous_keys++;
        if (found < 0) { found = nk++; keys[found] = k; }
        Key *g = &keys[found];
        if (g->n == g->cap) { g->cap = g->cap ? 2 * g->cap : 4; g->ops = (BcOp **)realloc(g->ops, sizeof(BcOp *) * (size_t)g->cap); }
        g->ops[g->n++] = op;
    }
    nw = (BcOp **)malloc(sizeof(BcOp *) * (size_t)(s->nops + 1));
    for (int q = 0; q < nk; q++) {
        Key *k = &keys[q]; BcOp *o0 = k->ops[0]; ShpList bufs = { 0 }, ps = { 0 }, parts = { 0 }; GEOSGeometry *u, *fused;
        if (k->keep || (k->n == 1 && o0->np == 1)) { for (int j = 0; j < k->n; j++) { k->ops[j]->rc++; nw[nn++] = k->ops[j]; } continue; }
        for (int j = 0; j < k->n; j++) for (int t = 0; t < k->ops[j]->np; t++) {
            GEOSGeometry *b = shp_buffer(k->ops[j]->polys[t], 0.06, 16, SHP_CAP_ROUND, SHP_JOIN_MITRE, 3.0, 0);
            if (!b) { shp_list_free(&bufs); goto out; }
            shp_list_push(&bufs, b);
        }
        u = shp_union_all((const GEOSGeometry *const *)bufs.g, bufs.n); shp_list_free(&bufs);
        if (!u) goto out;
        fused = shp_buffer(u, -0.06, 16, SHP_CAP_ROUND, SHP_JOIN_MITRE, 3.0, 0); shp_free(u);
        if (!fused) goto out;
        shp_polys_of(fused, &parts); shp_free(fused);
        for (int j = 0; j < parts.n; j++) if (shp_area(parts.g[j]) > 0.05) { shp_list_push(&ps, parts.g[j]); parts.g[j] = NULL; }
        shp_list_free(&parts);
        if (!ps.n) continue;
        if (o0->type == BC_TAPER) {
            ShpList fit = { 0 };
            for (int j = 0; j < ps.n; j++) {
                GEOSGeometry *f;
                if (fit_for_taper(ps.g[j], o0->n[1].v * tan_deg(o0->n[2].v) + 0.03, &f) < 0) { shp_list_free(&fit); shp_list_free(&ps); goto out; }
                if (f) shp_list_push(&fit, f);
            }
            if (fit.n) { shp_list_free(&ps); ps = fit; }
        }
        int np; GEOSGeometry **arr = take_list(&ps, &np);
        nw[nn++] = bc_op_new(o0->type, o0->kind, o0->n, arr, np);
    }
    for (int i = 0; i < s->nops; i++) bc_op_unref(s->ops[i]);
    free(s->ops); s->ops = nw; s->nops = nn; nw = NULL;
    rc = 0;
out:
    if (nw) { for (int i = 0; i < nn; i++) bc_op_unref(nw[i]); free(nw); }
    for (int q = 0; q < nk; q++) free(keys[q].ops);
    free(keys);
    return rc;
}

/* ======================= gg._coincide / _separate ======================= */
/* gg._coincide(a, b, tol=0.005): 1 / 0, BC_ERR */
static int coincide(const GEOSGeometry *a, const GEOSGeometry *b) {
    const double tol = 0.005;
    GEOSGeometry *ba = shp_boundary(a), *bb = shp_boundary(b), *bi = NULL; int r = BC_ERR;
    if (!ba || !bb) goto out;
    bi = shp_intersection(ba, bb);
    if (!bi) goto out;
    if (!shp_is_empty(bi) && shp_length(bi) > 1e-3) { r = 1; goto out; }
    for (int w = 0; w < 2; w++) {
        const GEOSGeometry *p = w ? b : a, *qb = w ? ba : bb;
        double *xy; int n = ext_xy(p, &xy);
        if (n < 0) { free(xy); goto out; }
        for (int i = 0; i < n; i++) {
            GEOSGeometry *pt = shp_point(xy[2 * i], xy[2 * i + 1]); double d = shp_distance(qb, pt); shp_free(pt);
            if (d < tol) { free(xy); r = 1; goto out; }
        }
        free(xy);
    }
    r = 0;
out:
    shp_free(ba); shp_free(bb); shp_free(bi);
    return r;
}
typedef struct { int where, i, j; GEOSGeometry *g; int own; } Item;          /* where: 0 op, 1 chop, 2 gcut */
static void item_set(Item *it, GEOSGeometry *g) { if (it->own) shp_free(it->g); it->g = g; it->own = 1; }
/* keeps the polygons of a list that are not empty and larger than lim (clones) */
static GEOSGeometry **keep_big(GEOSGeometry *const *p, int n, double lim, int *nout) {
    GEOSGeometry **o = (GEOSGeometry **)malloc(sizeof(GEOSGeometry *) * (size_t)(n ? n : 1)); int k = 0;
    for (int i = 0; i < n; i++) if (!shp_is_empty(p[i]) && shp_area(p[i]) > lim) o[k++] = shp_clone(p[i]);
    *nout = k;
    return o;
}
/* gg._separate(s, gap=0.12, touch=0.08, sliver=0.02): returns fixed, BC_ERR */
static int separate(BcScene *s) {
    const double gap = 0.12, touch = 0.08, sliver = 0.02;
    Item *it; int n = 0, cap = 16, fixed = 0, rc = BC_ERR; const GEOSGeometry **geoms; ShpTree *tree;
    for (int i = 0; i < s->nops; i++) cap += s->ops[i]->np;
    for (int i = 0; i < s->ngcuts; i++) cap += s->gcuts[i].np;
    cap += s->nchops;
    it = (Item *)calloc((size_t)cap, sizeof(Item));
    for (int i = 0; i < s->nops; i++) {
        if (kind_is(s->ops[i]->kind, "rockc")) continue;
        for (int j = 0; j < s->ops[i]->np; j++) { Item t = { 0, i, j, s->ops[i]->polys[j], 0 }; it[n++] = t; }
    }
    for (int i = 0; i < s->nchops; i++) { Item t = { 1, i, 0, s->chops[i]->poly, 0 }; it[n++] = t; }
    for (int i = 0; i < s->ngcuts; i++) for (int j = 0; j < s->gcuts[i].np; j++) { Item t = { 2, i, j, s->gcuts[i].polys[j], 0 }; it[n++] = t; }
    if (n < 2) { free(it); return 0; }
    geoms = (const GEOSGeometry **)malloc(sizeof(GEOSGeometry *) * (size_t)n);
    for (int a = 0; a < n; a++) geoms[a] = it[a].g;
    tree = shp_tree_new(geoms, n, 10);
    for (int a = 0; a < n; a++) {
        GEOSGeometry *pa = it[a].g, *buf = shp_buffer_q(pa, touch, 16); ShpIdx hits = { 0 }; int nh;
        if (!buf) goto out;
        nh = shp_tree_query(tree, buf, SHP_PRED_NONE, &hits); shp_free(buf);
        if (nh < 0) { shp_idx_free(&hits); goto out; }
        for (int h = 0; h < hits.n; h++) {
            int b = hits.i[h], lo, hi, r; GEOSGeometry *pb, *x, *q; double ia;
            if (b <= a || (it[b].where == it[a].where && it[b].i == it[a].i)) continue;
            if (it[a].where == 0 && it[b].where == 0 && s->ops[it[a].i]->kind->has_loft && s->ops[it[b].i]->kind->has_loft) continue;
            pb = it[b].g;
            if (shp_is_empty(pa) || shp_is_empty(pb) || shp_distance(pa, pb) >= touch) continue;
            if (shp_area(pa) <= shp_area(pb)) { lo = a; hi = b; } else { lo = b; hi = a; }
            x = shp_intersection(pa, pb);
            if (!x) { shp_idx_free(&hits); goto out; }
            ia = shp_area(x); shp_free(x);
            if (ia >= sliver) {
                r = coincide(pa, pb);
                if (r < 0) { shp_idx_free(&hits); goto out; }
                if (!r) continue;
                q = shp_buffer(it[lo].g, -gap, 16, SHP_CAP_ROUND, SHP_JOIN_MITRE, 5.0, 0);
                if (!q) { shp_idx_free(&hits); goto out; }
                if (shp_is_empty(q) || shp_area(q) < 0.3) { shp_free(q); continue; }
                x = largest(q); shp_free(q);
                if (!x) { shp_idx_free(&hits); goto out; }
                item_set(&it[lo], x); pa = it[a].g; fixed++; continue;
            }
            GEOSGeometry *hb = shp_buffer_q(it[hi].g, gap, 8);
            if (!hb) { shp_idx_free(&hits); goto out; }
            q = shp_difference(it[lo].g, hb); shp_free(hb);
            if (!q) { shp_idx_free(&hits); goto out; }
            if (!shp_is_empty(q)) { x = largest(q); shp_free(q); q = x; if (!q) { shp_idx_free(&hits); goto out; } }
            item_set(&it[lo], q); pa = it[a].g; fixed++;
        }
        shp_idx_free(&hits);
    }
    shp_tree_free(tree); tree = NULL;
    /* write the items back: new ops and cuts (only non-empty pieces over 0.05 mm2), the chops' polygons in place */
    {
        BcOp **nw = (BcOp **)malloc(sizeof(BcOp *) * (size_t)(s->nops + 1)); int nn = 0;
        BcGcut *ng = (BcGcut *)calloc((size_t)(s->ngcuts + 1), sizeof(BcGcut)); int ngc = 0;
        GEOSGeometry ***opp = (GEOSGeometry ***)malloc(sizeof(void *) * (size_t)(s->nops + 1));
        GEOSGeometry ***gcp = (GEOSGeometry ***)malloc(sizeof(void *) * (size_t)(s->ngcuts + 1));
        for (int i = 0; i < s->nops; i++) { opp[i] = (GEOSGeometry **)malloc(sizeof(void *) * (size_t)(s->ops[i]->np + 1)); memcpy(opp[i], s->ops[i]->polys, sizeof(void *) * (size_t)s->ops[i]->np); }
        for (int i = 0; i < s->ngcuts; i++) { gcp[i] = (GEOSGeometry **)malloc(sizeof(void *) * (size_t)(s->gcuts[i].np + 1)); memcpy(gcp[i], s->gcuts[i].polys, sizeof(void *) * (size_t)s->gcuts[i].np); }
        for (int k = 0; k < n; k++) {
            if (it[k].where == 0) opp[it[k].i][it[k].j] = it[k].g;
            else if (it[k].where == 2) gcp[it[k].i][it[k].j] = it[k].g;
            else if (it[k].g != s->chops[it[k].i]->poly) { shp_free(s->chops[it[k].i]->poly); s->chops[it[k].i]->poly = it[k].g; it[k].own = 0; }
        }
        for (int i = 0; i < s->nops; i++) {
            int np; GEOSGeometry **ps = keep_big(opp[i], s->ops[i]->np, 0.05, &np);
            if (np) nw[nn++] = bc_op_new(s->ops[i]->type, s->ops[i]->kind, s->ops[i]->n, ps, np); else free(ps);
            free(opp[i]);
        }
        for (int i = 0; i < s->ngcuts; i++) {
            int np; GEOSGeometry **ps = keep_big(gcp[i], s->gcuts[i].np, 0.05, &np);
            if (np) { ng[ngc].polys = ps; ng[ngc].np = np; ng[ngc].depth = s->gcuts[i].depth; ng[ngc].kind = s->gcuts[i].kind; ngc++; } else free(ps);
            free(gcp[i]);
        }
        free(opp); free(gcp);
        for (int k = 0; k < n; k++) if (it[k].own) { shp_free(it[k].g); it[k].own = 0; }
        for (int i = 0; i < s->nops; i++) bc_op_unref(s->ops[i]);
        free(s->ops); s->ops = nw; s->nops = nn;
        gcuts_free(s->gcuts, s->ngcuts); s->gcuts = ng; s->ngcuts = ngc;
        int nc = 0;
        for (int i = 0; i < s->nchops; i++) {
            BcChop *c = s->chops[i];
            if (!shp_is_empty(c->poly) && shp_area(c->poly) > 0.5) s->chops[nc++] = c; else chop_free(c);
        }
        s->nchops = nc;
    }
    rc = fixed;
out:
    shp_tree_free(tree); free((void *)geoms);
    if (rc < 0) for (int k = 0; k < n; k++) if (it[k].own) shp_free(it[k].g);
    free(it);
    return rc;
}

/* ======================= gg._surface_z ======================= */
/* the highest op top under the slot inside (z0 + 0.05, z1 + 1e-6]: 1 = found (*best), 0 = None, BC_ERR */
static int surface_z(const BcScene *s, const GEOSGeometry *p, double z0, double z1, BcNum *best) {
    GEOSGeometry *pt = shp_representative_point(p); int has = 0;
    if (!pt) return BC_ERR;
    for (int i = 0; i < s->nops; i++) {
        const BcOp *op = s->ops[i]; BcNum top; int in = 0;
        if (kind_is(op->kind, "rockc")) continue;
        top = op->type == BC_PRISM ? op->n[1] : n_add(op->n[0], op->n[1]);
        if (!(z0 + 0.05 < top.v && top.v <= z1 + 1e-6)) continue;
        for (int j = 0; j < op->np && !in; j++) { int r = shp_contains(op->polys[j], pt); if (r < 0) { shp_free(pt); return BC_ERR; } in = r; }
        if (in && (!has || top.v > best->v)) { *best = top; has = 1; }
    }
    shp_free(pt);
    return has;
}

/* ======================= gg.build_cmds_trim_last ======================= */
static int build_cmds_trim_last(BcScene *s, BcGroups *Gs, int true_circle_rim) {
    const BcCfg *c = &s->cfg; BcGcut *fg; int nfg; char nm[64], fnm[64];
    BcGroup g = g_new(0); BcNum H = F(c->H), mR = F(-c->RIM);
    sk_circle(&g, "Blank", I(0), c->R0 + 0.3);
    g_push(&g, extrude("Blank", H, NULL, 0, "new", NULL, "Base", "Base")); G_push(Gs, g);
    if (final_gcuts(s, &fg, &nfg) < 0) return BC_ERR;
    for (int i = 0; i < nfg; i++) {
        snprintf(nm, sizeof nm, "G%d", i); snprintf(fnm, sizeof fnm, "FG%d", i);
        g = g_new(0);
        if (sk(&g, c, nm, H, fg[i].polys, fg[i].np, NAN, c->sketch_tol) < 0) { g_free(&g); gcuts_free(fg, nfg); return BC_ERR; }
        g_push(&g, extrude(nm, fg[i].depth, NULL, 1, "cut", "Base", fnm, NULL)); G_push(Gs, g);
    }
    gcuts_free(fg, nfg);
    for (int i = 0; i < s->nops; i++) {
        const BcOp *op = s->ops[i]; BcNum t;
        snprintf(nm, sizeof nm, "J%d", i); snprintf(fnm, sizeof fnm, "FJ%d", i);
        g = g_new(0);
        if (sk(&g, c, nm, op->n[0], op->polys, op->np, NAN, c->sketch_tol) < 0) { g_free(&g); return BC_ERR; }
        if (op->type == BC_PRISM) g_push(&g, extrude(nm, n_round(n_sub(op->n[1], op->n[0]), 4), NULL, 0, "join", "Base", fnm, NULL));
        else { t = n_neg(op->n[2]); g_push(&g, extrude(nm, op->n[1], &t, 0, "join", "Base", fnm, NULL)); }
        G_push(Gs, g);
    }
    for (int i = 0; i < s->ntcuts; i++) {
        const BcTcut *tc = &s->tcuts[i];
        if (!tc->np) continue;
        snprintf(nm, sizeof nm, "T%d", i); snprintf(fnm, sizeof fnm, "FT%d", i);
        g = g_new(0);
        if (sk(&g, c, nm, tc->z1, tc->polys, tc->np, NAN, c->sketch_tol) < 0) { g_free(&g); return BC_ERR; }
        g_push(&g, extrude(nm, n_round(n_sub(tc->z1, tc->z0), 4), NULL, 1, "cut", "Base", fnm, NULL)); G_push(Gs, g);
    }
    g = g_new(0);
    if (true_circle_rim) sk_circle(&g, "Trim", I(0), c->R0);
    else {
        GEOSGeometry *d = shp_point_buffer(0, 0, c->R0, 96);
        if (!d || sk(&g, c, "Trim", I(0), &d, 1, NAN, c->sketch_tol) < 0) { shp_free(d); g_free(&g); return BC_ERR; }
        shp_free(d);
    }
    g_push(&g, extrude("Trim", F(8.0), &mR, 0, "new", NULL, "TrimF", "Trim"));
    BcVal p = v_dict();
    d_set(&p, "target", v_str("Base")); d_set(&p, "tools", v_strs1("Trim")); d_set(&p, "operation", v_str("intersect")); d_set(&p, "keep_tools", v_bool(0));
    g_push(&g, cmd("solid.combine", p)); G_push(Gs, g);
    g = g_new(0); mag_group(&g, c); G_push(Gs, g);
    return 0;
}

/* ======================= gg.build_cmds ======================= */
int bc_build_cmds(BcScene *s, BcGroups *Gs) {
    const BcCfg *c = &s->cfg; BcGcut *fg = NULL; int nfg = 0; char nm[64], fnm[64];
    BcNum H = F(c->H), mR = F(-c->RIM);
    BcGroup g;
    if (!c->base_first) return build_cmds_trim_last(s, Gs, c->true_circle_rim);
    if (prune(s) < 0 || group_ops(s) < 0 || separate(s) < 0) return BC_ERR;
    /* RIM_K: each op of s.ops its own rim slot (the last place wins for an op that is there twice); others 0 */
    for (int i = 0; i < s->nchops; i++) for (int j = 0; j < s->chops[i]->nops; j++) s->chops[i]->ops[j]->rk = 0;
    for (int k = 0; k < s->nops; k++) s->ops[k]->rk = 0;
    for (int k = 0; k < s->nops; k++) s->ops[k]->rk = 1 + k % 6;

    g = g_new(0);
    sk_circle(&g, "Blank", I(0), c->R0);
    g_push(&g, extrude("Blank", H, &mR, 0, "new", NULL, "Base", "Base")); G_push(Gs, g);
    g = g_new(0); mag_group(&g, c); G_push(Gs, g);

    /* ground cuts */
    if (final_gcuts(s, &fg, &nfg) < 0) return BC_ERR;
    for (int i = 0; i < nfg; i++) {
        ShpList ps = { 0 };
        if (clip(c, fg[i].polys, fg[i].np, c->H, 0.0, 0, &ps) < 0) { shp_list_free(&ps); goto fail; }
        if (!ps.n) continue;
        int split = c->split_cuts && 1 < ps.n && ps.n <= 12, nparts = split ? ps.n : 1;
        for (int k = 0; k < nparts; k++) {
            int j = (i * 7 + k) % 23 + 1;
            double zt = c->H + 0.2 + 0.004 * j;
            BcNum dd = n_round(n_add(n_add(fg[i].depth, F(zt - c->H)), F(0.003 * j)), 4);
            snprintf(nm, sizeof nm, "G%d_%d", i, k); snprintf(fnm, sizeof fnm, "FG%d_%d", i, k);
            g = g_new(0);
            if (sk(&g, c, nm, F(zt), split ? &ps.g[k] : ps.g, split ? 1 : ps.n, rim_r(c, c->H, 0), c->sketch_tol) < 0) { g_free(&g); shp_list_free(&ps); goto fail; }
            g_push(&g, extrude(nm, dd, NULL, 1, "cut", "Base", fnm, NULL)); G_push(Gs, g);
        }
        shp_list_free(&ps);
    }
    gcuts_free(fg, nfg); fg = NULL;

    /* raised ops */
    for (int i = 0; i < s->nops; i++) {
        const BcOp *op = s->ops[i]; const BcKind *kd = op->kind; ShpList polys = { 0 }; BcNum z0; BcVal e;
        if (kind_is(kd, "rockc")) continue;
        if (kd->has_ghost) continue;
        if (op->type == BC_PRISM && !kd->has_loft && in_chamfer(c, kd) && c->chamfer_rocks) {
            if (prism_groups(Gs, c, i, op) < 0) return BC_ERR;
            continue;
        }
        if (op_clip(c, op, &polys) < 0) { shp_list_free(&polys); return BC_ERR; }
        if (!polys.n) continue;
        z0 = n_sub(op->n[0], F(DEEP));
        snprintf(nm, sizeof nm, "J%d", i); snprintf(fnm, sizeof fnm, "FJ%d", i);
        if (op->type == BC_PRISM) {
            e = extrude(nm, n_round(n_sub(op->n[1], z0), 4), NULL, 0, "join", "Base", fnm, NULL);
            if (kd->has_loft) {
                BcVal lf = v_list(); v_push(&lf, v_num(kd->loft_chain)); v_push(&lf, v_num(n_round(kd->loft_z, 4)));
                d_set((BcVal *)bc_get(&e, "params"), "loft", lf);
            }
        } else {
            double grow = DEEP * tan_deg(op->n[2].v); BcNum t = n_neg(op->n[2]);
            if (grow > 1e-3) {
                ShpList grown = { 0 };
                for (int k = 0; k < polys.n; k++) {
                    GEOSGeometry *b = shp_buffer(polys.g[k], grow, 16, SHP_CAP_ROUND, SHP_JOIN_MITRE, 3.0, 0), *d, *x;
                    d = shp_point_buffer(0, 0, rim_r(c, d_max(z0.v, c->H), op->rk), 96);
                    if (!b || !d) { shp_free(b); shp_free(d); shp_list_free(&grown); shp_list_free(&polys); return BC_ERR; }
                    x = shp_intersection(b, d); shp_free(b); shp_free(d);
                    if (!x) { shp_list_free(&grown); shp_list_free(&polys); return BC_ERR; }
                    shp_polys_of(x, &grown); shp_free(x);
                }
                shp_list_free(&polys); polys = grown;
            }
            if (!polys.n) { shp_list_free(&polys); continue; }
            e = extrude(nm, n_round(n_add(op->n[1], F(DEEP)), 4), &t, 0, "join", "Base", fnm, NULL);
        }
        g = g_new(0);
        if (sk(&g, c, nm, z0, polys.g, polys.n, op->type == BC_PRISM ? rim_r(c, clip_z(c, op), op->rk) : NAN, kd->has_loft ? 0.0 : c->sketch_tol) < 0) {
            g_free(&g); v_free(&e); shp_list_free(&polys); return BC_ERR;
        }
        g_push(&g, e); G_push(Gs, g);
        shp_list_free(&polys);
    }

    /* chamfered pieces */
    for (int i = 0; i < s->nchops; i++) {
        const BcChop *ch = s->chops[i]; ShpList q = { 0 }; BcGroup *fb = NULL; int nfb = 0, capfb = 0;
        if (clip(c, &ch->poly, 1, d_max(ch->top.v, c->H), 0.0, 0, &q) < 0) { shp_list_free(&q); return BC_ERR; }
        for (int j = 0; j < ch->nops; j++) {
            const BcOp *op = ch->ops[j]; ShpList pp = { 0 }; BcVal e = v_dict(), x; BcNum t;
            if (op_clip(c, op, &pp) < 0) { shp_list_free(&pp); shp_list_free(&q); v_free(&e); return BC_ERR; }
            if (!pp.n) { v_free(&e); continue; }
            if (op->type == BC_PRISM) d_set(&e, "distance", v_num(n_round(n_sub(op->n[1], op->n[0]), 4)));
            else { t = n_neg(op->n[2]); d_set(&e, "distance", v_num(op->n[1])); d_set(&e, "taper", v_num(t)); }
            snprintf(nm, sizeof nm, "CF%d_%d", i, j); snprintf(fnm, sizeof fnm, "FCF%d_%d", i, j);
            d_set(&e, "sketch", v_str(nm)); d_set(&e, "operation", v_str("join")); d_set(&e, "targets", v_strs1("Base")); d_set(&e, "name", v_str(fnm));
            g = g_new(0);
            if (sk(&g, c, nm, op->n[0], pp.g, pp.n, op->type == BC_PRISM ? rim_r(c, clip_z(c, op), op->rk) : NAN, c->sketch_tol) < 0) {
                g_free(&g); v_free(&e); shp_list_free(&pp); shp_list_free(&q); return BC_ERR;
            }
            x = cmd("solid.extrude", e); g_push(&g, x);
            gl_push(&fb, &nfb, &capfb, g);
            shp_list_free(&pp);
        }
        int iso = ch->has_iso ? ch->iso.v != 0 : 1;
        if (q.n != 1 || shp_area(q.g[0]) < 0.98 * shp_area(ch->poly) || !iso) {
            for (int j = 0; j < nfb; j++) G_push(Gs, fb[j]);
            free(fb); shp_list_free(&q); continue;
        }
        GEOSGeometry *p = q.g[0];
        snprintf(nm, sizeof nm, "C%d", i); snprintf(fnm, sizeof fnm, "PC%d", i);
        g = g_new(0);
        if (sk(&g, c, nm, n_sub(ch->z0, F(DEEP)), &p, 1, NAN, c->sketch_tol) < 0) { g_free(&g); shp_list_free(&q); return BC_ERR; }
        if (piece_group(Gs, fnm, &g, n_add(n_sub(ch->top, ch->z0), F(DEEP)), ch->top, n_min(ch->run, ch->drop), fb, nfb, p) < 0) { shp_list_free(&q); return BC_ERR; }
        shp_list_free(&q);
    }

    /* tool cuts (and the grate chamfers) */
    for (int i = 0; i < s->ntcuts; i++) {
        const BcTcut *tc = &s->tcuts[i]; ShpList ps = { 0 }; double rimv;
        if (!tc->np) continue;
        if (clip(c, tc->polys, tc->np, d_max(tc->z0.v, c->H), 0.0, 0, &ps) < 0) { shp_list_free(&ps); return BC_ERR; }
        if (!ps.n) continue;
        rimv = rim_r(c, d_max(tc->z0.v, c->H), 0);
        if (c->split_cuts && 1 < ps.n && ps.n <= 12) {
            for (int k = 0; k < ps.n; k++) {
                int j = (i * 7 + k) % 23 + 1;
                snprintf(nm, sizeof nm, "T%d_%d", i, k); snprintf(fnm, sizeof fnm, "FT%d_%d", i, k);
                g = g_new(0);
                if (sk(&g, c, nm, n_add(tc->z1, F(0.004 * j)), &ps.g[k], 1, rimv, c->sketch_tol) < 0) { g_free(&g); shp_list_free(&ps); return BC_ERR; }
                g_push(&g, extrude(nm, n_round(n_add(n_sub(tc->z1, tc->z0), F(0.007 * j)), 4), NULL, 1, "cut", "Base", fnm, NULL)); G_push(Gs, g);
            }
        } else {
            snprintf(nm, sizeof nm, "T%d", i); snprintf(fnm, sizeof fnm, "FT%d", i);
            g = g_new(0);
            if (sk(&g, c, nm, tc->z1, ps.g, ps.n, rimv, c->sketch_tol) < 0) { g_free(&g); shp_list_free(&ps); return BC_ERR; }
            g_push(&g, extrude(nm, n_round(n_sub(tc->z1, tc->z0), 4), NULL, 1, "cut", "Base", fnm, NULL)); G_push(Gs, g);
        }
        if (kind_is(tc->kind, "slot") && c->chamfer_grates) {
            for (int k = 0; k < ps.n; k++) {
                GEOSGeometry *p = ps.g[k], *hull; BcNum top; double *xy, pa, mind = 0; int has, m, skip;
                has = surface_z(s, p, tc->z0.v, tc->z1.v, &top);
                if (has < 0) { shp_list_free(&ps); return BC_ERR; }
                m = ext_xy(p, &xy);
                pa = shp_area(p);
                skip = !has || pa < 0.5 || shp_num_interiors(p) || m - 1 > 8;
                if (!skip) {
                    hull = shp_convex_hull(p);
                    if (!hull) { free(xy); shp_list_free(&ps); return BC_ERR; }
                    skip = shp_area(hull) - pa > 0.02 * pa; shp_free(hull);
                }
                if (!skip) {
                    for (int t = 0; t + 1 < m; t++) { double dd = py_hypot(xy[2 * t] - xy[2 * t + 2], xy[2 * t + 1] - xy[2 * t + 3]); if (t == 0 || dd < mind) mind = dd; }
                    skip = mind < 0.45;
                }
                if (skip) { free(xy); continue; }
                BcVal edges = v_list();
                for (int t = 0; t + 1 < m; t++) {
                    BcVal ed = v_list();
                    v_push(&ed, v_f(rnd4((xy[2 * t] + xy[2 * t + 2]) / 2))); v_push(&ed, v_f(rnd4((xy[2 * t + 1] + xy[2 * t + 3]) / 2)));
                    v_push(&ed, v_num(n_round(top, 4)));
                    v_push(&edges, ed);
                }
                free(xy);
                BcVal p1 = v_dict(), p2 = v_dict();
                d_set(&p2, "edges", v_copy(&edges)); d_set(&p2, "distance", v_f(0.06));
                d_set(&p1, "edges", edges); d_set(&p1, "distance", v_f(0.1));
                g = g_new(1); g_push(&g, cmd("solid.chamfer", p1)); g.polish = 1;
                BcGroup alt = g_new(1); g_push(&alt, cmd("solid.chamfer", p2));
                gl_push(&g.alts, &g.nalts, &g.capalts, alt);
                G_push(Gs, g);
            }
        }
        shp_list_free(&ps);
    }
    return 0;
fail:
    gcuts_free(fg, nfg);
    return BC_ERR;
}

/* ======================= for the 2D drawing (integrate, 2026-11-20) ======================= */
/* s.final_gcuts() of the scene as it stands (mesher._draw2d_pil draws these): a new list, free with bc_gcuts_free */
int bc_final_gcuts(const BcScene *s, BcGcut **out, int *nout) { return final_gcuts(s, out, nout); }
void bc_gcuts_free(BcGcut *g, int n) { if (g) gcuts_free(g, n); }
