/* layouts.c: the ground layouts' API (see layouts.h): gg3.make's first part (ly_ground), ORIG[style] (ly_orig), the
   gg2.mk / pickF that gg3.make installs, and the helpers every layout file uses (an arena that holds the
   geometries a layout makes, geometry and Scene calls that raise as Python raises, the voronoi joint net). */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "layouts.h"

LyCfg LYC = { 1, 0, 0 };
void ly_config(int random_mix, int scenes, int outcrop_cracks) { LYC.random_mix = random_mix; LYC.scenes = scenes; LYC.outcrop_cracks = outcrop_cracks; }

/* ================================================================ errors and the arena */
jmp_buf *ly_jb = NULL;
void ly_raise(void) { longjmp(*ly_jb, 1); }

enum { AR_GEO, AR_LIST, AR_MEM };
typedef struct { int t; void *p; } ArEnt;
static ArEnt *AR = NULL; static int ARN = 0, ARCAP = 0, ARDEPTH = 0;
static void ar_push(int t, void *p) {
    if (ARN >= ARCAP) { ARCAP = ARCAP ? 2 * ARCAP : 4096; AR = (ArEnt *)realloc(AR, sizeof *AR * (size_t)ARCAP); }
    AR[ARN].t = t; AR[ARN].p = p; ARN++;
}
static void ar_free(ArEnt *e) {
    if (e->t == AR_GEO) geo_unref((Geo *)e->p);
    else if (e->t == AR_LIST) { gl_free((GeoList *)e->p); free(e->p); }
    else free(e->p);
}
void ly_arena_begin(void) { ARDEPTH++; }
int ly_mark(void) { return ARN; }
void ly_pop(int mark) { while (ARN > mark) ar_free(&AR[--ARN]); }
void ly_arena_end(void) { if (--ARDEPTH <= 0) { ly_pop(0); ARDEPTH = 0; } }
Geo *LA(GEOSGeometry *g) { Geo *h; if (!g) ly_raise(); h = geo_new(g); ar_push(AR_GEO, h); return h; }
Geo *LAG(Geo *h) { if (!h) ly_raise(); ar_push(AR_GEO, h); return h; }
GeoList *LL(void) { GeoList *l = (GeoList *)calloc(1, sizeof *l); ar_push(AR_LIST, l); return l; }
GeoList *L1(Geo *g) { GeoList *l = LL(); gl_push(l, g); return l; }
GeoList *L2(Geo *a, Geo *b) { GeoList *l = LL(); gl_push(l, a); gl_push(l, b); return l; }
void *LM(size_t n) { void *p = calloc(1, n ? n : 1); ar_push(AR_MEM, p); return p; }

/* ================================================================ numbers */
double ly_pow(double x, double y) { double re = 0, im = 0; if (px_pow(x, y, &re, &im) != 0) ly_raise(); return re; }
int64_t ly_rint(double x) { int64_t v; if (!isfinite(x)) ly_raise(); v = (int64_t)py_rint(x); return v; }
double ly_round(double x, int nd) { return px_round(x, nd); }
int64_t ly_int(double x) { int64_t v; if (px_int(x, &v) != 0) ly_raise(); return v; }
int64_t KA_(double v) { double k = SC.K > 1.0 ? SC.K : 1.0; return ly_rint(v * ly_pow(k, 2.0)); }
int ly_arange(double a, double b, double st, double **out) {
    int64_t n = np_arange(a, b, st, NULL, 0);
    if (n < 0) ly_raise();
    *out = (double *)LM(sizeof(double) * (size_t)(n + 1));
    if (n) np_arange(a, b, st, *out, n);
    return (int)n;
}
int ly_linspace(double a, double b, int n, double **out) {
    if (n < 0) ly_raise();
    *out = (double *)LM(sizeof(double) * (size_t)(n + 1));
    if (np_linspace(a, b, n, 1, *out) < 0) ly_raise();
    return n;
}

/* ================================================================ geometry */
Geo *g_empty(void) { return LA(shp_polygon(NULL, 0)); }
Geo *g_pt(double x, double y) { return LA(shp_point(x, y)); }
Geo *g_ptbuf(double x, double y, double r, int q) { return LA(shp_point_buffer(x, y, r, q)); }
Geo *g_buf(Geo *g, double d) { return LA(shp_buffer_q(g->g, d, 16)); }
Geo *g_bufq(Geo *g, double d, int q) { return LA(shp_buffer_q(g->g, d, q)); }
Geo *g_bufx(Geo *g, double d, int q, int cap, int join, double mitre) { return LA(shp_buffer(g->g, d, q, cap, join, mitre, 0)); }
Geo *g_poly(const double *xy, int n) { return LA(shp_polygon(xy, n)); }
Geo *g_box(double x0, double y0, double x1, double y1) { return LA(shp_box(x0, y0, x1, y1, 1)); }
Geo *g_line(const double *xy, int n) { return LA(shp_linestring(xy, n)); }
Geo *g_linebuf(const double *xy, int n, double w, int cap, int q) { return LA(shp_line_buffer(xy, n, w, cap, SHP_JOIN_ROUND, q, 5.0)); }
Geo *g_inter(Geo *a, Geo *b) { return LA(shp_intersection(a->g, b->g)); }
Geo *g_diff(Geo *a, Geo *b) { return LA(shp_difference(a->g, b->g)); }
Geo *g_union(Geo *a, Geo *b) { return LA(shp_union(a->g, b->g)); }
Geo *g_uu(const GeoList *l) {
    const GEOSGeometry **gs = (const GEOSGeometry **)malloc(sizeof *gs * (size_t)(l->n ? l->n : 1)); int i; GEOSGeometry *r;
    for (i = 0; i < l->n; i++) gs[i] = l->v[i] ? l->v[i]->g : NULL;
    r = shp_union_all(gs, l->n); free(gs);
    return LA(r);
}
Geo *g_uu2(Geo *a, Geo *b) { return g_uu(L2(a, b)); }
Geo *g_simp(Geo *g, double t) { return LA(shp_simplify(g->g, t, 1)); }
Geo *g_hull(Geo *g) { return LA(shp_convex_hull(g->g)); }
Geo *g_centroid(Geo *g) { return LA(shp_centroid(g->g)); }
Geo *g_reppt(Geo *g) { return LA(shp_representative_point(g->g)); }
Geo *g_rot(Geo *g, double deg, int origin, double ox, double oy) { return LA(shp_rotate(g->g, deg, origin, ox, oy, 0)); }
Geo *g_scale(Geo *g, double fx, double fy, int origin, double ox, double oy) { return LA(shp_scale(g->g, fx, fy, origin, ox, oy)); }
Geo *g_trans(Geo *g, double dx, double dy) { return LA(shp_translate(g->g, dx, dy)); }
Geo *g_inset(Geo *p, double d) { return LA(shp_inset(p->g, d)); }
Geo *g_ext(Geo *p) { const GEOSGeometry *r = shp_exterior(p->g); if (!r) ly_raise(); return LA(shp_clone(r)); }
Geo *g_interp(Geo *line, double d) { return LA(shp_interpolate(line->g, d, 0)); }
GeoList *g_polys(Geo *g) { GeoList *l = LL(); lf_polys_of(g, l); return l; }
Geo *g_max_area(const GeoList *l) {
    int i, b = 0; double ba;
    if (!l->n) ly_raise();
    ba = g_area(l->v[0]);
    for (i = 1; i < l->n; i++) { double a = g_area(l->v[i]); if (a > ba) { ba = a; b = i; } }
    return l->v[b];
}
double g_area(Geo *g) { return shp_area(g->g); }
double g_len(Geo *g) { return shp_length(g->g); }
double g_dist(Geo *a, Geo *b) { return shp_distance(a->g, b->g); }
void g_bounds(Geo *g, double b[4]) { shp_bounds(g->g, b); }
double g_x(Geo *p) { return shp_x(p->g); }
double g_y(Geo *p) { return shp_y(p->g); }
int g_isempty(Geo *g) { return LC(shp_is_empty(g->g)); }
int g_contains(Geo *a, Geo *b) { return LC(shp_contains(a->g, b->g)); }
int g_intersects(Geo *a, Geo *b) { return LC(shp_intersects(a->g, b->g)); }
int g_valid(Geo *g) { return LC(shp_is_valid(g->g)); }
int g_type(Geo *g) { return shp_type(g->g); }
int g_wide(Geo *p, double w) { return LC(lf_wide_enough(p, w)); }
Geo *g_fit(Geo *p, double d) { int err = 0; Geo *q = lf_fit_for_taper(p, d, &err); if (err) ly_raise(); if (!q) return NULL; return LAG(q); }
Geo *g_stone_r(double cx, double cy, double r, PyRandom *rng, double aspect, int nv, int round_) { return LAG(lf_stone(cx, cy, r, rng, aspect, nv, round_)); }
Geo *g_stone(double cx, double cy, double r, PyRandom *rng, double aspect, int nv) { return g_stone_r(cx, cy, r, rng, aspect, nv, 0); }
Geo *g_wigline(double ax, double ay, double bx, double by, PyRandom *rng, double amp, int n) {
    double *xy = (double *)LM(sizeof(double) * 2 * (size_t)(n + 1)); int k = lf_wiggle(ax, ay, bx, by, rng, amp, n, xy);
    return g_line(xy, k);
}
int g_coords(Geo *g, double **xy) {
    int n = shp_coords(g->g, NULL, 0), cap = 64;
    for (;;) { *xy = (double *)LM(sizeof(double) * 2 * (size_t)cap); n = shp_coords(g->g, *xy, cap); if (n >= 0) return n; if (cap > (1 << 24)) ly_raise(); cap *= 4; }
}
Geo *g_frpoly(const Frame *f, const double *uv, int n) {
    double *xy = (double *)LM(sizeof(double) * 2 * (size_t)n); int i;
    for (i = 0; i < n; i++) lf_at(f, uv[2 * i], uv[2 * i + 1], &xy[2 * i], &xy[2 * i + 1]);
    return g_poly(xy, n);
}
Geo *g_R(const Frame *f, double u0, double u1, double v0, double v1) { double uv[8] = { u0, v0, u1, v0, u1, v1, u0, v1 }; return g_frpoly(f, uv, 4); }
Geo *g_linecut(double ax, double ay, double bx, double by, double w) { return LAG(lf_line_cut(ax, ay, bx, by, w)); }

/* ================================================================ the Scene */
Kind *KS(const char *s) { return kind_str(s); }
GeoList *S_prismk(Scene *s, const GeoList *polys, Num top, Kind *kind, Num z0, double minw, int foot) {
    GeoList *out = LL(); LC(sc_prism(s, polys, top, kind, z0, minw, foot, out)); return out;
}
GeoList *S_prism(Scene *s, const GeoList *polys, Num top, const char *kind, Num z0, double minw, int foot) { return S_prismk(s, polys, top, KS(kind), z0, minw, foot); }
GeoList *S_stack(Scene *s, const GeoList *polys, Num h, const Tier *t, int nt, const char *kind, Num z0, Num *top) {
    GeoList *out = LL(); Num tp; LC(sc_stack(s, polys, h, t, nt, KS(kind), z0, out, &tp)); if (top) *top = tp; return out;
}
void S_gcut(Scene *s, const GeoList *polys, Num depth, const char *kind) { LC(sc_gcut(s, polys, depth, KS(kind))); }
void S_tcut(Scene *s, const GeoList *polys, Num z0, Num z1, const char *kind) { LC(sc_tcut(s, polys, z0, z1, KS(kind))); }
GeoList *S_scatter(Scene *s, int n, double rmin, double rmax, Geo *region, double gap, int round_, int tries) {
    GeoList *out = LL(); LC(sc_scatter(s, n, rmin, rmax, region, gap, round_, tries, out)); return out;
}
int S_free(Scene *s, Geo *p, double gap) { return LC(sc_free(s, p, gap)); }
void S_taper(Scene *s, const GeoList *polys, Num z, Num h, Num a, const char *kind) { ol_push_own(&s->ops, op_taper(polys, z, h, a, KS(kind))); }
void S_tadd(Scene *s, Geo *fp, Num zb, Num zt, Geo *top) { T_add(s, fp, zb, zt, top); }
void S_tzr(Scene *s, Geo *p, Num *lo, Num *hi) { LC(T_zr(s, p, lo, hi)); }
int S_tfree(Scene *s, Geo *p, double gap) { return LC(T_free(s, p, gap)); }
Num S_tflat(Scene *s, Geo *p) { Num z; LC(T_flat(s, p, &z)); return z; }
GeoList *S_tprism(Scene *s, const GeoList *polys, Num h, const char *kind, Num **tops) {
    TPiece *tp = NULL; int n = 0, i, r = tprism(s, polys, h, KS(kind), &tp, &n); GeoList *out = LL(); Num *t;
    t = (Num *)LM(sizeof(Num) * (size_t)(n + 1));
    for (i = 0; i < n; i++) { gl_push_own(out, tp[i].g); t[i] = tp[i].top; }
    free(tp);
    LC(r);
    if (tops) *tops = t;
    return out;
}
void S_tstack(Scene *s, const GeoList *polys, Num h, const Tier *t, int nt, const char *kind) { LC(tstack(s, polys, h, t, nt, KS(kind))); }
void S_tcutflat(Scene *s, const GeoList *polys, Num depth, const char *kind) { LC(tcut_flat(s, polys, depth, KS(kind))); }
GeoList *S_scatter_any(Scene *s, int n, double rmin, double rmax, Geo *region, double gap, int round_, int tries) {
    GeoList *out = LL(); LC(scatter_any(s, n, rmin, rmax, region, gap, round_, tries, out)); return out;
}
void S_extend_feet(Scene *s, const GeoList *l) { gl_extend(&s->feet, l); }
void S_extend_objs(Scene *s, const GeoList *l) { gl_extend(&s->objs, l); }

/* ================================================================ the voronoi net (gg2.crack_net, the hardpan and
   cobble nets of gg2.maahl / urban_x, styles2.j_voronoi) */
static int tuple_lt2(const double *a, const double *b) { return px_tuple_lt(a, b, 2); }
Geo *ly_vor_net(PyRandom *rng, double lo, double hi, int64_t n, double e0, double e1, int wig, double amp, double w, int q) {
    int mk = ly_mark(); GeoList *segs = LL(); double *xy; int64_t i; PySetF *edges; GEOSGeometry *pts, *env, *vd; int nc, c;
    Geo *net;
    if (n < 0) n = 0;
    xy = (double *)LM(sizeof(double) * 2 * (size_t)(n + 1));
    for (i = 0; i < n; i++) { xy[2 * i] = py_uniform(rng, lo, hi); xy[2 * i + 1] = py_uniform(rng, lo, hi); }
    pts = shp_multipoint(xy, (int)n); env = shp_box(e0, e0, e1, e1, 1);
    vd = (pts && env) ? shp_voronoi(pts, env, 0.0, 0) : NULL;
    shp_free(pts); shp_free(env);
    LA(vd);
    edges = pyset_new(2, 2);
    nc = shp_num_geoms(vd);
    for (c = 0; c < nc; c++) {
        const GEOSGeometry *cell = shp_geom_n(vd, c), *ext = shp_exterior(cell); int m, k; double *cc;
        if (!ext) { pyset_free(edges); ly_raise(); }
        m = shp_coords(ext, NULL, 0);
        if (m < 0) { m = 0; }
        cc = (double *)malloc(sizeof(double) * 2 * (size_t)(G.GEOSGetNumCoordinates_r(gctx, ext) + 1));
        m = shp_coords(ext, cc, G.GEOSGetNumCoordinates_r(gctx, ext));
        for (k = 0; k < 2 * m; k++) cc[k] = px_round(cc[k], 3);          /* tuple(round(v, 3) for v in xy) */
        for (k = 0; k + 1 < m; k++) {
            double key[4]; const double *a = &cc[2 * k], *b = &cc[2 * k + 2];
            if (tuple_lt2(b, a)) { key[0] = b[0]; key[1] = b[1]; key[2] = a[0]; key[3] = a[1]; }          /* tuple(sorted((a, b))) */
            else { key[0] = a[0]; key[1] = a[1]; key[2] = b[0]; key[3] = b[1]; }
            pyset_add(edges, key);
        }
        free(cc);
    }
    {
        int64_t pos = 0; const double *key;
        while ((key = pyset_next(edges, &pos))) {
            Geo *ln;
            if (wig) { double *p = (double *)malloc(sizeof(double) * 8); int k = lf_wiggle(key[0], key[1], key[2], key[3], rng, amp, 3, p);
                       GEOSGeometry *b = shp_line_buffer(p, k, w, SHP_CAP_ROUND, SHP_JOIN_ROUND, q, 5.0); free(p); ln = LA(b); }
            else { double p[4] = { key[0], key[1], key[2], key[3] }; ln = LA(shp_line_buffer(p, 2, w, SHP_CAP_ROUND, SHP_JOIN_ROUND, q, 5.0)); }
            gl_push(segs, ln);
        }
    }
    pyset_free(edges);
    net = g_uu(segs);
    geo_ref(net); ly_pop(mk); ar_push(AR_GEO, net);          /* keep only the net */
    return net;
}

/* ================================================================ gg2.mk and pickF as gg3.make installs them */
const LyMk *LYMK = NULL;
Scene *ly_mk(const char *style, const char *title, int k, const char *ground) {
    char a[512], b[512], c[1024];
    snprintf(a, sizeof a, "%s_%02d", style, LYMK->n); snprintf(b, sizeof b, "%s #%d", title, LYMK->n);
    snprintf(c, sizeof c, "%s%d%s", style, k, LYMK->suf ? LYMK->suf : "");
    return scene_new(a, b, c, ground, 1);
}
static int cmpstr(const void *a, const void *b) { return strcmp(*(const char *const *)a, *(const char *const *)b); }
int ly_pickF(const char *const *table, int k0, int cnt, int n, const char **out, int cap) {
    static char buf[64][32]; static int bi = 0;
    int nout = 0, i;
    if (!LYMK->fmix) {          /* table[n].split() */
        const char *t;
        if (n < k0 || n >= k0 + cnt) ly_raise();          /* KeyError */
        t = table[n - k0];
        while (*t) {
            int L = 0;
            while (*t == ' ') t++;
            if (!*t) break;
            while (t[L] && t[L] != ' ') L++;
            if (nout < cap) { char *d = buf[bi]; bi = (bi + 1) % 64; memcpy(d, t, (size_t)L); d[L] = 0; out[nout++] = d; }
            t += L;
        }
        return nout;
    }
    {          /* sorted({w for v in table.values() for w in v.split()} - SCENE_SKIP); FMIX.sample(pool, min(len(pool), FMIX.randint(2, 4))) */
        char *pool[64]; int np = 0, j, k; int64_t want, idx[64];
        for (i = 0; i < cnt; i++) {
            const char *t = table[i];
            while (*t) {
                int L = 0, dup = 0; char w[32];
                while (*t == ' ') t++;
                if (!*t) break;
                while (t[L] && t[L] != ' ') L++;
                memcpy(w, t, (size_t)L); w[L] = 0; t += L;
                if (LYMK->scene_skip && (!strcmp(w, "track") || !strcmp(w, "ruts") || !strcmp(w, "planks") || !strcmp(w, "trail"))) continue;
                for (j = 0; j < np; j++) if (!strcmp(pool[j], w)) { dup = 1; break; }
                if (!dup) { pool[np] = buf[bi]; bi = (bi + 1) % 64; strcpy(pool[np], w); np++; }
            }
        }
        qsort(pool, (size_t)np, sizeof pool[0], cmpstr);
        want = py_randint(LYMK->fmix, 2, 4);
        k = (int)(np < want ? np : want);
        if (py_sample_e(LYMK->fmix, np, k, idx) != 0) ly_raise();
        for (j = 0; j < k && nout < cap; j++) out[nout++] = pool[idx[j]];
        return nout;
    }
}
int ly_count(const char *const *F, int nF, const char *w) { int i, c = 0; for (i = 0; i < nF; i++) if (!strcmp(F[i], w)) c++; return c; }

/* ================================================================ ORIG */
typedef struct { const char *name; LyFn fn; int s2; } LyStyle;
static const LyStyle ORIG[] = {
    /* gg2.STYLES */
    { "badlands", gg2_badlands, 0 }, { "highland", gg2_highland, 0 }, { "maahl", gg2_maahl, 0 }, { "corridor", gg2_corridor, 0 },
    { "infestation", gg2_infestation, 0 }, { "urban", gg2_urban, 0 }, { "mine", gg2_mine, 0 }, { "mine_v3", gg2_mine_v3, 0 },
    { "maahl_v3", gg2_maahl_v3, 0 },
    /* styles2.STYLES2 */
    { "swamp", s2_swamp, 1 }, { "cynder", s2_cynder, 1 }, { "targa", s2_targa, 1 }, { "trederra", s2_trederra, 1 },
    { "fortress", s2_fortress, 1 }, { "beast", s2_beast, 1 }, { "frontier", s2_frontier, 1 }, { "graveyard", s2_graveyard, 1 },
    { "forest", s2_forest, 1 }, { "dungeon", s2_dungeon, 1 }, { "temple", s2_temple, 1 }, { "coast", s2_coast, 1 },
    /* newstyles (style_<name>.py, in name order) */
    { "ashveil", new_ashveil, 0 }, { "blight", new_blight, 0 }, { "ferrum", new_ferrum, 0 }, { "obelis", new_obelis, 0 },
    { "sundered", new_sundered, 0 }, { "thornwild", new_thornwild, 0 },
};
static const LyStyle *find_style(const char *st) { size_t i; for (i = 0; i < sizeof ORIG / sizeof ORIG[0]; i++) if (!strcmp(ORIG[i].name, st)) return &ORIG[i]; return NULL; }
int ly_has_style(const char *style) { return find_style(style) != NULL; }

Scene *ly_orig(const char *style, int nb, const LyMk *mk, int *err) {
    const LyStyle *st = find_style(style); jmp_buf jb, *prev = ly_jb; const LyMk *pmk = LYMK; Scene *volatile s = NULL;
    *err = 0;
    if (!st) { *err = 1; return NULL; }          /* KeyError */
    ly_arena_begin();
    ly_jb = &jb; LYMK = mk;
    if (setjmp(jb) == 0) s = st->fn(nb);
    else { *err = 1; s = NULL; }
    ly_jb = prev; LYMK = pmk;
    ly_arena_end();
    return s;
}

static char SALT_AFTER[1024];
Scene *ly_ground(const char *style, int n, const char *salt, const char *reseed, PyRandom *mixr, const char **salt_after, int *err) {
    const LyStyle *st; char saltc[1024], seed[1200], suf[1200]; int nb, RND, layout0; LyMk mk; Scene *s;
    salt = salt ? salt : ""; reseed = reseed ? reseed : "";
    if (salt_after) { snprintf(SALT_AFTER, sizeof SALT_AFTER, "%s", salt); *salt_after = SALT_AFTER; }
    if (n > 20) {          /* 21+: same 4 detail tiers in turn, each with its own seed */
        int k = n; char s2[1024], t[512]; char *h;
        n = (n - 1) % 20 + 1;
        snprintf(s2, sizeof s2, "%sx%d", salt, k);
        if (salt_after) snprintf(SALT_AFTER, sizeof SALT_AFTER, "%s", s2);
        s = ly_ground(style, n, s2, reseed, mixr, NULL, err);
        if (!s) return NULL;
        free(s->name); snprintf(t, sizeof t, "%s_%02d", style, k); s->name = _strdup(t);
        h = strrchr(s->title, '#'); if (h) *h = 0;
        snprintf(t, sizeof t, "%s#%d", s->title, k); free(s->title); s->title = _strdup(t);
        return s;
    }
    st = find_style(style);
    if (!st) { *err = 1; return NULL; }
    snprintf(saltc, sizeof saltc, "%s%s", salt, reseed);
    RND = saltc[0] && LYC.random_mix;
    snprintf(seed, sizeof seed, "%s%dmix%s", style, n, saltc); py_seed_str(mixr, seed);
    if (st->s2) { nb = n; suf[0] = 0; }
    else if (!strcmp(style, "corridor") || !strcmp(style, "urban")) { nb = n; suf[0] = 0; }
    else { nb = (n - 1) % 10 + 1; snprintf(suf, sizeof suf, "%s", n <= 10 ? "" : "b"); }
    mk.fmix = NULL; mk.scene_skip = 0;
    if (RND) {
        size_t L;
        nb = st->s2 ? n : (!strcmp(style, "corridor") || !strcmp(style, "urban")) ? (int)py_randint(mixr, 1, 20) : (int)py_randint(mixr, 1, 10);
        L = strlen(suf); snprintf(suf + L, sizeof suf - L, "n%d", n);
        mk.fmix = mixr; mk.scene_skip = LYC.scenes;
    }
    { size_t L = strlen(suf); snprintf(suf + L, sizeof suf - L, "%s%s", reseed, salt); }
    mk.n = n; mk.suf = suf; mk.salt = salt; mk.reseed = reseed;
    layout0 = SC.layout; SC.layout = 1;
    s = ly_orig(style, nb, &mk, err);
    SC.layout = layout0;
    if (!s) return NULL;
    {          /* s.name, s.title = f"{style}_{n:02d}", s.title.rsplit("#", 1)[0] + f"#{n}" */
        char t[512]; char *h;
        free(s->name); snprintf(t, sizeof t, "%s_%02d", style, n); s->name = _strdup(t);
        h = strrchr(s->title, '#'); if (h) *h = 0;
        snprintf(t, sizeof t, "%s#%d", s->title, n); free(s->title); s->title = _strdup(t);
    }
    return s;
}
