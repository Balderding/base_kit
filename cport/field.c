/* field.c: see field.h. Every Python step is named in a comment (mesher.py, class Field and friends). */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "field.h"
#include "pyx.h"
#include "mt.h"

#define GRID 1e-4
typedef GEOSContextHandle_t Cx;
enum { OP_PRISM = 1, OP_STEP = 2, OP_TAPER = 3, OP_CUT = 4, OP_STAND = 5 };

/* ------------------------------------------------------------------------------------------------ small parts */
typedef struct { GEOSGeometry **a; int n, cap; } GV;
static void gv_push(GV *v, GEOSGeometry *g) { if (v->n == v->cap) { v->cap = v->cap ? v->cap * 2 : 8; v->a = realloc(v->a, sizeof(*v->a) * v->cap); } v->a[v->n++] = g; }
typedef struct { GEOSGeometry *g; int rule; char fresh; } Cell;          /* fresh: made by this step (freed if the step fails) */
typedef struct { Cell *a; int n, cap; double box[4]; } Bucket;
static void cl_push(Bucket *b, GEOSGeometry *g, int rule, char fresh) { if (b->n == b->cap) { b->cap = b->cap ? b->cap * 2 : 8; b->a = realloc(b->a, sizeof(Cell) * b->cap); } b->a[b->n].g = g; b->a[b->n].rule = rule; b->a[b->n].fresh = fresh; b->n++; }
typedef struct { Rule *a; int n, cap; } Rules;
static int r_add(Rules *R, Rule r) { if (R->n == R->cap) { R->cap = R->cap ? R->cap * 2 : 256; R->a = realloc(R->a, sizeof(Rule) * R->cap); } R->a[R->n] = r; return R->n++; }
static Rule flat(double z) { Rule r; memset(&r, 0, sizeof r); r.kind = R_FLAT; r.z = py_round(z, 4); return r; }          /* Flat(z): round(z, 4) */
static double r_lo(const Rule *r) { return r->kind == R_SLOPE ? r->z0 : r->kind == R_BAND ? r->lo : r->z; }
static double r_hi(const Rule *r) { return r->kind == R_SLOPE ? r->zmax : r->kind == R_BAND ? r->hi : r->z; }

static int empty(Cx c, const GEOSGeometry *g) { return !g || G.GEOSisEmpty_r(c, g) != 0; }
static double area(Cx c, const GEOSGeometry *g) { double a = 0; G.GEOSArea_r(c, g, &a); return a; }
/* P(g): on the 1e-4 grid; takes g (returns g itself when empty, as Python does); NULL = GEOS failed */
static GEOSGeometry *Pc(Cx c, GEOSGeometry *g, int *err) {
    if (!g) { *err = 1; return NULL; }
    if (G.GEOSisEmpty_r(c, g)) return g;
    GEOSGeometry *q = G.GEOSGeom_setPrecision_r(c, g, GRID, 0); G.GEOSGeom_destroy_r(c, g);
    if (!q) *err = 1;
    return q;
}
/* polys(g): copies of its polygons over 1e-6 mm2 */
static void polys_c(Cx c, const GEOSGeometry *g, GV *out) {
    if (!g || G.GEOSisEmpty_r(c, g)) return;
    int t = G.GEOSGeomTypeId_r(c, g);
    if (t == GEOS_POLYGON) { if (area(c, g) > 1e-6) gv_push(out, G.GEOSGeom_clone_r(c, g)); return; }
    if (t == GEOS_MULTIPOINT || t == GEOS_MULTILINESTRING || t == GEOS_MULTIPOLYGON || t == GEOS_GEOMETRYCOLLECTION) {
        int n = G.GEOSGetNumGeometries_r(c, g);
        for (int i = 0; i < n; i++) polys_c(c, G.GEOSGetGeometryN_r(c, g, i), out);
    }
}
static void bounds(Cx c, const GEOSGeometry *g, double b[4]) { G.GEOSGeom_getXMin_r(c, g, &b[0]); G.GEOSGeom_getYMin_r(c, g, &b[1]); G.GEOSGeom_getXMax_r(c, g, &b[2]); G.GEOSGeom_getYMax_r(c, g, &b[3]); }
static int overlap(const double *r, const double *b) { return b[0] <= r[2] && r[0] <= b[2] && b[1] <= r[3] && r[1] <= b[3]; }          /* Field._touched */

/* Slope.above / Band.above: a new geometry, or NULL for None */
static GEOSGeometry *above(Cx c, const Rule *r, double z, int *err) {
    if (r->kind == R_SLOPE) {
        if (z <= r->z0) return G.GEOSGeom_clone_r(c, r->poly);
        if (z >= r->zmax) return NULL;
        GEOSGeometry *b = G.GEOSBufferWithStyle_r(c, r->poly, -(z - r->z0) * r->tan, 16, 1, 2, 5.0); if (!b) *err = 1; return b;          /* buffer(..., join_style=2) */
    }
    if (r->kind == R_BAND) return z < (r->lo + r->hi) / 2 ? G.GEOSGeom_clone_r(c, r->poly) : NULL;
    return NULL;
}
/* Slope.at / Band.at */
static double at(Cx c, const Rule *r, double x, double y) {
    if (r->kind == R_FLAT || r->kind == R_STEP) return r->z;
    GEOSGeometry *p = G.GEOSGeom_createPointFromXY_r(c, x, y); double v;
    if (r->kind == R_SLOPE) { double d = 0; G.GEOSDistance_r(c, r->ring, p, &d); double a = r->z0 + d * r->cot; v = py_round(a < r->zmax ? a : r->zmax, 4); }
    else {
        double d0 = 0, d1 = 0; G.GEOSDistance_r(c, r->ring, p, &d0); G.GEOSDistance_r(c, r->inner, p, &d1);
        double t1 = d0 + d1 > 1e-9 ? d0 / (d0 + d1) : 0.0, q = d0 / r->w, t2 = q < 1.0 ? q : 1.0, t = t2 > t1 ? t2 : t1;
        v = py_round(r->lo + (r->hi - r->lo) * t, 4);
    }
    G.GEOSGeom_destroy_r(c, p); return v;
}

/* ------------------------------------------------------------------------------------------------ the steps */
typedef struct {
    int kind; char name[64];
    GEOSGeometry *in;               /* the outline as Python sent it */
    GEOSGeometry *reg; double box[4]; int noreg;          /* _region(): P(outline & disc); noreg = empty (the step does nothing) */
    double a, b, c; int chain;
    int f, sl, ftop; GEOSGeometry *top;          /* the rules this step lays down; taper: its flat top outline */
    int bprev, bnew;                /* join_step: the chain's last piece becomes this band (-1 = none) */
    int failed;
} Op;
typedef struct { GEOSGeometry *g; int rule; } Item;

/* new_for(inter, rule) of join_prism / join_step / join_taper / cut: up to two (outline, rule) */
static int new_for(Cx c, const Op *o, const Rules *R, GEOSGeometry *inter, int ri, Item *it, int *ni, int *err) {
    const Rule *r = &R->a[ri]; *ni = 0;
    if (o->kind == OP_PRISM || o->kind == OP_STEP) {
        double z1 = o->a;
        if (r_hi(r) <= z1) { it[(*ni)++] = (Item){ G.GEOSGeom_clone_r(c, inter), o->f }; return 1; }
        if (r_lo(r) >= z1) return 1;
        GEOSGeometry *ab = above(c, r, z1, err); if (*err) return 0;
        GEOSGeometry *g = (ab && !G.GEOSisEmpty_r(c, ab)) ? G.GEOSDifference_r(c, inter, ab) : G.GEOSGeom_clone_r(c, inter);
        if (ab) G.GEOSGeom_destroy_r(c, ab);
        if (!g) { *err = 1; return 0; }
        it[(*ni)++] = (Item){ g, o->f }; return 1;
    }
    if (o->kind == OP_CUT) {
        double zb = o->a;
        if (r_lo(r) >= zb) { it[(*ni)++] = (Item){ G.GEOSGeom_clone_r(c, inter), o->f }; return 1; }
        if (r_hi(r) <= zb) return 1;
        GEOSGeometry *ab = above(c, r, zb, err); if (*err) return 0;
        if (ab && !G.GEOSisEmpty_r(c, ab)) { GEOSGeometry *g = G.GEOSIntersection_r(c, inter, ab); G.GEOSGeom_destroy_r(c, ab); if (!g) { *err = 1; return 0; } it[(*ni)++] = (Item){ g, o->f }; }
        else if (ab) G.GEOSGeom_destroy_r(c, ab);
        return 1;
    }
    /* join_taper */
    double ztop = o->a + o->b, zc;
    if (r->kind == R_FLAT || r->kind == R_STEP) zc = r->z;
    else {
        GEOSGeometry *pt = G.GEOSPointOnSurface_r(c, inter); if (!pt) { *err = 1; return 0; }
        double x = 0, y = 0; G.GEOSCoordSeq_getXY_r(c, G.GEOSGeom_getCoordSeq_r(c, pt), 0, &x, &y); G.GEOSGeom_destroy_r(c, pt);
        zc = at(c, r, x, y);
    }
    if (zc >= ztop) return 1;
    GEOSGeometry *vis = above(c, &R->a[o->sl], zc, err); if (*err) return 0;
    if (!vis) return 1;
    if (G.GEOSisEmpty_r(c, vis)) { G.GEOSGeom_destroy_r(c, vis); return 1; }
    GEOSGeometry *v = G.GEOSIntersection_r(c, inter, vis); G.GEOSGeom_destroy_r(c, vis); if (!v) { *err = 1; return 0; }
    if (!empty(c, o->top)) {
        GEOSGeometry *t = G.GEOSIntersection_r(c, v, o->top); if (!t) { G.GEOSGeom_destroy_r(c, v); *err = 1; return 0; }
        if (!G.GEOSisEmpty_r(c, t)) it[(*ni)++] = (Item){ t, o->ftop }; else G.GEOSGeom_destroy_r(c, t);
        GEOSGeometry *v2 = G.GEOSDifference_r(c, v, o->top); G.GEOSGeom_destroy_r(c, v); if (!v2) { *err = 1; return 0; } v = v2;
    }
    if (!G.GEOSisEmpty_r(c, v)) it[(*ni)++] = (Item){ v, o->sl }; else G.GEOSGeom_destroy_r(c, v);
    return 1;
}

/* Field._apply_bucket: one tile's cells after one step; 0 = GEOS failed (the tile is then left as it was) */
static int apply_bucket(Cx c, Bucket *b, const Op *o, const Rules *R) {
    Bucket out; memset(&out, 0, sizeof out); memcpy(out.box, b->box, sizeof out.box); int err = 0;
    GEOSGeometry **gone = malloc(sizeof(GEOSGeometry *) * (b->n ? b->n : 1)); int ngone = 0;
    for (int i = 0; i < b->n && !err; i++) {
        GEOSGeometry *cp = b->a[i].g; int rule = b->a[i].rule;
        char hit = G.GEOSIntersects_r(c, cp, o->reg);
        if (hit == 2) { err = 1; break; }
        if (!hit) { cl_push(&out, cp, rule, 0); continue; }
        GEOSGeometry *inter = G.GEOSIntersection_r(c, cp, o->reg); if (!inter) { err = 1; break; }
        GV taken = { 0 };
        int *trule = NULL, ntr = 0, captr = 0;
        if (!G.GEOSisEmpty_r(c, inter) && area(c, inter) > 1e-7) {
            Item it[2]; int ni = 0;
            if (!new_for(c, o, R, inter, rule, it, &ni, &err)) { G.GEOSGeom_destroy_r(c, inter); break; }
            for (int k = 0; k < ni && !err; k++) {
                GEOSGeometry *ps = Pc(c, it[k].g, &err); if (err) break;
                int before = taken.n; polys_c(c, ps, &taken); G.GEOSGeom_destroy_r(c, ps);
                for (int q = before; q < taken.n; q++) { if (ntr == captr) { captr = captr ? captr * 2 : 4; trule = realloc(trule, sizeof(int) * captr); } trule[ntr++] = it[k].rule; }
            }
            if (err) { for (int k = 0; k < taken.n; k++) G.GEOSGeom_destroy_r(c, taken.a[k]); free(taken.a); free(trule); G.GEOSGeom_destroy_r(c, inter); break; }
        }
        G.GEOSGeom_destroy_r(c, inter);
        if (!taken.n) { cl_push(&out, cp, rule, 0); free(taken.a); free(trule); continue; }
        GEOSGeometry **cl = malloc(sizeof(GEOSGeometry *) * taken.n); for (int k = 0; k < taken.n; k++) cl[k] = G.GEOSGeom_clone_r(c, taken.a[k]);
        GEOSGeometry *coll = G.GEOSGeom_createCollection_r(c, GEOS_GEOMETRYCOLLECTION, cl, (unsigned)taken.n); free(cl);          /* unary_union([q for q, _ in taken]) */
        GEOSGeometry *U = coll ? G.GEOSUnaryUnion_r(c, coll) : NULL; if (coll) G.GEOSGeom_destroy_r(c, coll);
        GEOSGeometry *rest = U ? G.GEOSDifference_r(c, cp, U) : NULL; if (U) G.GEOSGeom_destroy_r(c, U);
        GEOSGeometry *pr = rest ? Pc(c, rest, &err) : NULL; if (!rest) err = 1;
        if (err) { for (int k = 0; k < taken.n; k++) G.GEOSGeom_destroy_r(c, taken.a[k]); free(taken.a); free(trule); break; }
        GV rp = { 0 }; polys_c(c, pr, &rp); G.GEOSGeom_destroy_r(c, pr);
        for (int k = 0; k < rp.n; k++) cl_push(&out, rp.a[k], rule, 1);
        for (int k = 0; k < taken.n; k++) cl_push(&out, taken.a[k], trule[k], 1);
        free(rp.a); free(taken.a); free(trule);
        gone[ngone++] = cp;
    }
    if (err) {          /* the step failed here: this tile stays as it was */
        for (int i = 0; i < out.n; i++) if (out.a[i].fresh) G.GEOSGeom_destroy_r(c, out.a[i].g);
        free(out.a); free(gone); return 0;
    }
    for (int i = 0; i < ngone; i++) G.GEOSGeom_destroy_r(c, gone[i]);
    for (int i = 0; i < out.n; i++) out.a[i].fresh = 0;
    free(gone); free(b->a); *b = out; return 1;
}
static void remap(Bucket *b, int from, int to) { for (int i = 0; i < b->n; i++) if (b->a[i].rule == from) b->a[i].rule = to; }

/* ------------------------------------------------------------------------------------------------ threads */
typedef struct { Op *ops; int nops; const GEOSGeometry *disc; } RegJob;
static void reg_one(int i, Cx c, void *u) {          /* Field._region */
    RegJob *J = u; Op *o = &J->ops[i]; int err = 0;
    if (o->kind == OP_STAND || !o->in) { o->noreg = 1; return; }
    GEOSGeometry *r = Pc(c, G.GEOSIntersection_r(c, o->in, J->disc), &err);
    if (err || !r) { o->noreg = 1; o->failed = 1; return; }
    if (G.GEOSisEmpty_r(c, r)) { G.GEOSGeom_destroy_r(c, r); o->noreg = 1; return; }
    o->reg = r; bounds(c, r, o->box);
}
typedef struct { Bucket *bk; Op *ops; int nops; const Rules *R; char *fail; } TileJob;          /* fail[k * nops + i]: step i failed in tile k */
static void tile_one(int k, Cx c, void *u) {
    TileJob *J = u; Bucket *b = &J->bk[k];
    for (int i = 0; i < J->nops; i++) {
        Op *o = &J->ops[i];
        if (o->kind == OP_STAND) continue;
        if (o->bprev >= 0) remap(b, o->bprev, o->bnew);
        if (o->noreg || !overlap(o->box, b->box)) continue;
        if (!apply_bucket(c, b, o, J->R)) J->fail[(size_t)k * J->nops + i] = 1;
    }
}

/* ------------------------------------------------------------------------------------------------ the input */
typedef struct { const unsigned char *p, *e; int bad; } Rd;
static double rd_d(Rd *r) { double v = 0; if (r->p + 8 > r->e) { r->bad = 1; return 0; } memcpy(&v, r->p, 8); r->p += 8; return v; }
static uint32_t rd_u(Rd *r) { uint32_t v = 0; if (r->p + 4 > r->e) { r->bad = 1; return 0; } memcpy(&v, r->p, 4); r->p += 4; return v; }
static int32_t rd_i(Rd *r) { int32_t v = 0; if (r->p + 4 > r->e) { r->bad = 1; return 0; } memcpy(&v, r->p, 4); r->p += 4; return v; }
static int rd_b(Rd *r) { if (r->p + 1 > r->e) { r->bad = 1; return 0; } return *r->p++; }
static GEOSGeometry *rd_g(Rd *r) {
    uint32_t n = rd_u(r); if (r->bad || r->p + n > r->e) { r->bad = 1; return NULL; }
    GEOSGeometry *g = n ? G.GEOSGeomFromWKB_buf_r(gctx, r->p, n) : NULL; r->p += n;
    double pr = rd_d(r);
    if (g && pr > 0) { GEOSGeometry *q = G.GEOSGeom_setPrecision_r(gctx, g, pr, 1); if (q) { G.GEOSGeom_destroy_r(gctx, g); g = q; } }          /* the grid back, pointwise */
    return g;
}

static void sk_add(char *sk, int len, const char *line) {          /* skipped: one line per failed step, no repeats */
    size_t L = strlen(line); const char *p = sk;
    while ((p = strstr(p, line))) { if ((p == sk || p[-1] == '\n') && p[L] == '\n') return; p += L; }
    if (strlen(sk) + L + 2 < (size_t)len) { strcat(sk, line); strcat(sk, "\n"); }
}

/* one step on the tiles it reaches, one after another (Field._apply); 0 = it failed somewhere (the tiles before it keep it) */
static int run_serial(Bucket *bk, int nb, Op *o, const Rules *R) {
    if (o->bprev >= 0) for (int k = 0; k < nb; k++) remap(&bk[k], o->bprev, o->bnew);
    if (o->noreg) return !o->failed;
    for (int k = 0; k < nb; k++) if (overlap(o->box, bk[k].box) && !apply_bucket(gctx, &bk[k], o, R)) return 0;
    return 1;
}

int field_build(const unsigned char *buf, size_t n, int nthreads, MeshIn *in, char *skipped, int sklen, char *err, int errlen) {
    Rd r = { buf, buf + n, 0 }; memset(in, 0, sizeof *in); skipped[0] = 0;
    if (n < 4 || memcmp(buf, "BBF1", 4)) { snprintf(err, errlen, "not a BBF1 stream"); return 0; }
    r.p += 4;
    in->R0 = rd_d(&r); in->H = rd_d(&r); in->rim_tan = rd_d(&r);
    if ((in->has_mag = rd_b(&r))) { in->rm = rd_d(&r); in->dm = rd_d(&r); in->ncens = (int)rd_u(&r); in->cens = malloc(sizeof(double) * 2 * (in->ncens ? in->ncens : 1)); for (int i = 0; i < 2 * in->ncens; i++) in->cens[i] = rd_d(&r); }
    in->disc = rd_g(&r);
    if ((in->has_oval = rd_b(&r))) {
        in->ntr = (int)rd_u(&r); in->tr = malloc(sizeof(double) * 2 * (in->ntr ? in->ntr : 1)); for (int i = 0; i < 2 * in->ntr; i++) in->tr[i] = rd_d(&r);
        in->nou = (int)rd_u(&r); in->ou = malloc(sizeof(double) * 2 * (in->nou ? in->nou : 1)); for (int i = 0; i < 2 * in->nou; i++) in->ou[i] = rd_d(&r);
    }
    GEOSGeometry *guard = rd_g(&r); double size = rd_d(&r);
    int nops = (int)rd_u(&r); if (r.bad || !in->disc || !guard || nops < 0) { snprintf(err, errlen, "input not readable"); return 0; }
    Op *ops = calloc(nops ? nops : 1, sizeof(Op));
    for (int i = 0; i < nops && !r.bad; i++) {
        Op *o = &ops[i]; o->kind = rd_b(&r); int nl = rd_b(&r); if (r.p + nl > r.e) { r.bad = 1; break; }
        memcpy(o->name, r.p, nl < 63 ? nl : 63); r.p += nl;
        o->in = rd_g(&r); o->a = rd_d(&r); o->b = rd_d(&r); o->c = rd_d(&r); o->chain = rd_i(&r); o->bprev = o->bnew = -1; o->f = o->sl = o->ftop = -1;
    }
    if (r.bad) { snprintf(err, errlen, "steps not readable"); return 0; }
    double H = in->H;

    /* Field.tile(guard, H): the rim ring, then the ground squares */
    Rules R = { 0 }; int nb = 0, capb = 64; Bucket *bk = calloc(capb, sizeof(Bucket)); int e0 = 0;
    {
        double gb[4]; bounds(gctx, guard, gb);
        GEOSGeometry *ring = Pc(gctx, G.GEOSDifference_r(gctx, in->disc, guard), &e0);
        if (ring && !G.GEOSisEmpty_r(gctx, ring)) { cl_push(&bk[0], ring, r_add(&R, flat(H)), 0); bounds(gctx, ring, bk[0].box); } else if (ring) G.GEOSGeom_destroy_r(gctx, ring);
        nb = 1;
        int nx = (int)ceil((gb[2] - gb[0]) / size), ny = (int)ceil((gb[3] - gb[1]) / size); if (nx < 1) nx = 1; if (ny < 1) ny = 1;
        for (int i = 0; i < nx; i++) for (int j = 0; j < ny; j++) {
            double x0 = gb[0] + i * size, y0 = gb[1] + j * size, x1 = gb[0] + (i + 1) * size, y1 = gb[1] + (j + 1) * size;
            double sq[10] = { x1, y0, x1, y1, x0, y1, x0, y0, x1, y0 };          /* shapely box(): counter-clockwise from (maxx, miny) */
            GEOSGeometry *bx = G.GEOSGeom_createPolygon_r(gctx, G.GEOSGeom_createLinearRing_r(gctx, G.GEOSCoordSeq_copyFromBuffer_r(gctx, sq, 5, 0, 0)), NULL, 0);
            GEOSGeometry *q = Pc(gctx, G.GEOSIntersection_r(gctx, guard, bx), &e0); G.GEOSGeom_destroy_r(gctx, bx);
            GV got = { 0 }; polys_c(gctx, q, &got); if (q) G.GEOSGeom_destroy_r(gctx, q);
            if (!got.n) { free(got.a); continue; }
            if (nb == capb) { bk = realloc(bk, sizeof(Bucket) * capb * 2); memset(bk + capb, 0, sizeof(Bucket) * capb); capb *= 2; }
            for (int k = 0; k < got.n; k++) cl_push(&bk[nb], got.a[k], r_add(&R, flat(H)), 0);
            bk[nb].box[0] = x0; bk[nb].box[1] = y0; bk[nb].box[2] = x1; bk[nb].box[3] = y1; nb++; free(got.a);
        }
    }
    if (e0) { snprintf(err, errlen, "tiles: GEOS failed"); return 0; }

    /* the rules each step lays down, in step order (one thread: a step's rules never depend on the cells) */
    int maxch = -1; for (int i = 0; i < nops; i++) if (ops[i].chain > maxch) maxch = ops[i].chain;
    int *chains = malloc(sizeof(int) * (maxch + 2)); for (int i = 0; i <= maxch + 1; i++) chains[i] = -1;
    for (int i = 0; i < nops; i++) {
        Op *o = &ops[i]; int e = 0;
        if (o->kind == OP_PRISM || o->kind == OP_CUT) o->f = r_add(&R, flat(o->a));
        else if (o->kind == OP_STEP) {          /* join_step(poly, z1, chain, zb) */
            int prev = o->chain >= 0 ? chains[o->chain] : -1;
            if (prev >= 0 && R.a[prev].z > R.a[prev].zb + 1e-4 && !empty(gctx, o->in)) {          /* Band(prev.poly, poly, prev.zb, prev.z) */
                Rule bd; memset(&bd, 0, sizeof bd); bd.kind = R_BAND; bd.poly = G.GEOSGeom_clone_r(gctx, R.a[prev].poly); bd.ring = G.GEOSBoundary_r(gctx, bd.poly); bd.inner = G.GEOSBoundary_r(gctx, o->in);
                bd.lo = R.a[prev].zb; bd.hi = R.a[prev].z;
                GV pp = { 0 }; polys_c(gctx, o->in, &pp); double w = 0.0; int any = 0;
                for (int k = 0; k < pp.n; k++) {
                    const GEOSCoordSequence *cs = G.GEOSGeom_getCoordSeq_r(gctx, G.GEOSGetExteriorRing_r(gctx, pp.a[k])); unsigned int m = 0; G.GEOSCoordSeq_getSize_r(gctx, cs, &m);
                    for (unsigned int t = 0; t < m; t += 2) { double x, y, d = 0; G.GEOSCoordSeq_getXY_r(gctx, cs, t, &x, &y); GEOSGeometry *pt = G.GEOSGeom_createPointFromXY_r(gctx, x, y); G.GEOSDistance_r(gctx, bd.ring, pt, &d); G.GEOSGeom_destroy_r(gctx, pt); if (!any || d > w) w = d; any = 1; }
                    G.GEOSGeom_destroy_r(gctx, pp.a[k]);
                }
                free(pp.a); bd.w = w > 0.05 ? w : 0.05;          /* max(max(ds, default=0.0), 0.05) */
                o->bprev = prev; o->bnew = r_add(&R, bd);
            }
            Rule st = flat(o->a); st.kind = R_STEP; st.poly = G.GEOSGeom_clone_r(gctx, o->in); st.zb = o->b < o->a ? o->b : o->a;          /* Step(z1); f.zb = min(zb, z1) */
            o->f = r_add(&R, st); if (o->chain >= 0) chains[o->chain] = o->f;
        } else if (o->kind == OP_TAPER) {          /* join_taper(poly, z0, h, tan) */
            double z0 = o->a, h = o->b, tn = o->c, ztop = z0 + h;
            o->top = G.GEOSBufferWithStyle_r(gctx, o->in, -h * tn, 16, 1, 2, 5.0); if (!o->top) e = 1;
            Rule sl; memset(&sl, 0, sizeof sl); sl.kind = R_SLOPE; sl.poly = G.GEOSGeom_clone_r(gctx, o->in); sl.ring = G.GEOSBoundary_r(gctx, sl.poly); sl.z0 = z0; sl.tan = tn; sl.cot = 1.0 / tn; sl.zmax = ztop;
            o->sl = r_add(&R, sl); o->ftop = r_add(&R, flat(ztop));
        }
        if (e) { o->failed = 1; o->noreg = 1; }
    }
    free(chains);

    /* the regions: P(outline & disc), each step on its own (threads when there are) */
    RegJob RJ = { ops, nops, in->disc };
    if (nthreads > 1) mt_run(nops, nthreads, 50, reg_one, &RJ); else for (int i = 0; i < nops; i++) if (!ops[i].failed) reg_one(i, gctx, &RJ);

    /* the steps */
    int nsteps = 0; while (nsteps < nops && ops[nsteps].kind != OP_STAND) nsteps++;
    if (nthreads > 1) {          /* every tile its own steps, on all threads */
        char *fail = calloc((size_t)nb * (nsteps ? nsteps : 1), 1); TileJob TJ = { bk, ops, nsteps, &R, fail };
        mt_run(nb, nthreads, 8, tile_one, &TJ);
        for (int i = 0; i < nsteps; i++) { int f = ops[i].failed; for (int k = 0; k < nb && !f; k++) f = fail[(size_t)k * nsteps + i]; if (f) { char l[96]; snprintf(l, sizeof l, "%s: GEOSException", ops[i].name); sk_add(skipped, sklen, l); } }
        free(fail);
    } else for (int i = 0; i < nsteps; i++) if (ops[i].failed || !run_serial(bk, nb, &ops[i], &R)) { char l[96]; snprintf(l, sizeof l, "%s: GEOSException", ops[i].name); sk_add(skipped, sklen, l); }

    /* stand_flat(F, pads, H, sc): the miniature's standing spots, level with the ground most of their area has */
    for (int i = nsteps; i < nops; i++) {
        Op *o = &ops[i]; if (o->kind != OP_STAND || !o->in) continue;
        int e = 0; GEOSGeometry *pads = Pc(gctx, G.GEOSIntersection_r(gctx, o->in, guard), &e);
        if (e || !pads) { sk_add(skipped, sklen, "standing place: GEOSException"); continue; }
        if (G.GEOSisEmpty_r(gctx, pads)) { G.GEOSGeom_destroy_r(gctx, pads); continue; }
        double Hs = o->a, sc = o->b;
        double zv[512], av[512]; int nz = 0;
        for (int k = 0; k < nb && !e; k++) for (int j = 0; j < bk[k].n && !e; j++) {
            const Rule *ru = &R.a[bk[k].a[j].rule];
            if (Hs - 1e-6 <= r_lo(ru) && r_hi(ru) <= Hs + 0.9 * sc + 1e-6) {
                char hit = G.GEOSIntersects_r(gctx, bk[k].a[j].g, pads); if (hit == 2) { e = 1; break; } if (!hit) continue;
                GEOSGeometry *x = G.GEOSIntersection_r(gctx, bk[k].a[j].g, pads); if (!x) { e = 1; break; }
                double z = py_round(r_hi(ru), 2), a = area(gctx, x); G.GEOSGeom_destroy_r(gctx, x); int t;
                for (t = 0; t < nz; t++) if (zv[t] == z) break;
                if (t == nz && nz < 512) { zv[nz] = z; av[nz] = 0; nz++; }
                if (t < nz) av[t] += a;
            }
        }
        if (e) { G.GEOSGeom_destroy_r(gctx, pads); sk_add(skipped, sklen, "standing place: GEOSException"); continue; }
        double level = Hs; if (nz) { int bi = 0; for (int t = 1; t < nz; t++) if (av[t] > av[bi]) bi = t; level = zv[bi]; }          /* max(area, key=area.get): the first of the largest */
        Op two[2]; memset(two, 0, sizeof two);
        for (int t = 0; t < 2; t++) {          /* F.join_prism(pads, level); F.cut(pads, level) */
            two[t].kind = t ? OP_CUT : OP_PRISM; two[t].a = level; two[t].f = r_add(&R, flat(level)); two[t].bprev = two[t].bnew = -1; two[t].in = pads;
            RegJob rj = { &two[t], 1, in->disc }; reg_one(0, gctx, &rj);
            if (two[t].failed || !run_serial(bk, nb, &two[t], &R)) { sk_add(skipped, sklen, "standing place: GEOSException"); if (two[t].reg) G.GEOSGeom_destroy_r(gctx, two[t].reg); break; }
            if (two[t].reg) G.GEOSGeom_destroy_r(gctx, two[t].reg);
        }
        G.GEOSGeom_destroy_r(gctx, pads);
    }

    /* F.cells, one bucket after the other, for mesh_from_field */
    int nc = 0; for (int k = 0; k < nb; k++) nc += bk[k].n;
    in->ncells = nc; in->cell = malloc(sizeof(GEOSGeometry *) * (nc ? nc : 1)); in->cell_rule = malloc(sizeof(int) * (nc ? nc : 1)); nc = 0;
    for (int k = 0; k < nb; k++) { for (int j = 0; j < bk[k].n; j++) { in->cell[nc] = bk[k].a[j].g; in->cell_rule[nc] = bk[k].a[j].rule; nc++; } free(bk[k].a); }
    free(bk);
    in->nrules = R.n; in->rules = R.a;          /* Step rules keep their poly: mesh_in_free frees it */
    for (int i = 0; i < nops; i++) { if (ops[i].in) G.GEOSGeom_destroy_r(gctx, ops[i].in); if (ops[i].reg) G.GEOSGeom_destroy_r(gctx, ops[i].reg); if (ops[i].top) G.GEOSGeom_destroy_r(gctx, ops[i].top); }
    free(ops); G.GEOSGeom_destroy_r(gctx, guard);
    return 1;
}
