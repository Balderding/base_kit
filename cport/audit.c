/* audit.c: the "does this base make sense" check (audit.h). It reads a finished layout (a Scene after compose) and
   counts what would look wrong or not read once printed:
     straddle  a detail standing at a raised level it only partly stands on (part of it over a lower surface), or on
               terrain with a step of more than AU.step under it
     collide   two different details (or a detail and the ground's raised plates) cutting into each other: their
               pieces overlap in plan and in height, and neither stands on the other
     thin      a detail part narrower than AU.minw
     low       a whole detail standing less than AU.minh proud
     tiny      a whole detail smaller in plan than AU.mina
     cut_narrow / cut_shallow   a cut (groove, crack, mark) narrower than AU.cutw / shallower than AU.cutd
     wet       a detail standing in water, tar or lava (more than a fifth of it)
   Every op carries the detail that made it (scene.h Op.feat / fname); the ground laid out before the details
   (s->base_ops) is "ground". One JSON line per base: the counts and, per check, the details most often to blame. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdint.h>
#include "scene.h"
#include "shp.h"
#include "kinds.h"
#include "audit.h"

AuCfg AU = { 0.3, 0.5, 0.3, 0.6, 0.3, 0.2, 4.0 };

enum { C_STRADDLE, C_BURIED, C_COLLIDE, C_THIN, C_LOW, C_TINY, C_CUTN, C_CUTS, C_WET, C_TALL, NCHK };
static const char *CN[NCHK] = { "straddle", "buried", "collide", "thin", "low", "tiny", "cut_narrow", "cut_shallow", "wet", "tall" };

typedef struct { const char *who; int n; } Blame;
typedef struct { int n; Blame b[64]; int nb; } Chk;
static void blame(Chk *c, const char *who) {
    int i; c->n++;
    for (i = 0; i < c->nb; i++) if (!strcmp(c->b[i].who, who)) { c->b[i].n++; return; }
    if (c->nb < 64) { c->b[c->nb].who = who; c->b[c->nb].n = 1; c->nb++; }
}

typedef struct { const GEOSGeometry *g; double bb[4], z0, top, area; int feat, base, op; const Kind *k; const char *who; } Pc;

static int bb_hit(const double *a, const double *b) { return !(a[2] < b[0] || b[2] < a[0] || a[3] < b[1] || b[3] < a[1]); }
static double inter_area(const GEOSGeometry *a, const GEOSGeometry *b) { GEOSGeometry *x = shp_intersection(a, b); double r = x ? shp_area(x) : 0.0; shp_free(x); return r; }
static double opened_loss(const GEOSGeometry *g, double r) {          /* the area of g narrower than 2r */
    GEOSGeometry *a = shp_buffer(g, -r, 8, 2, 2, 5.0, 0), *b = a ? shp_buffer(a, r, 8, 2, 2, 5.0, 0) : NULL;          /* mitred: a sharp corner is not thin */ double l = shp_area(g) - (b ? shp_area(b) : 0.0);
    shp_free(a); shp_free(b); return l > 0 ? l : 0;
}
static int in_base(const Scene *s, const Op *o) { int i; for (i = 0; i < s->base_ops.n; i++) if (s->base_ops.v[i] == o) return 1; return 0; }

static char *who_of(const Op *o, int base) {          /* "detail name / material" */
    char b[160]; const char *f = base ? "ground" : o->fname ? o->fname : "laid with the scene";
    snprintf(b, sizeof b, "%s / %s", f, o->kind ? o->kind->s : "?"); return _strdup(b);
}
static void json_str(FILE *f, const char *s) { fputc('"', f); for (; *s; s++) { if (*s == '"' || *s == '\\') fputc('\\', f); fputc(*s, f); } fputc('"', f); }

static int au_groundk(const char *k) {          /* kinds the scene lays as ground (not standing details) */
    static const char *G[] = { "bedrock", "garden", "grass", "flower", "flesh", "moss", "pan", "mud", "sand", "ash", "dust", "spoil", "pavers", "slate", NULL }; int i; size_t n = strlen(k);
    for (i = 0; G[i]; i++) if (!strcmp(k, G[i])) return 1;
    if (n > 2 && !strcmp(k + n - 2, "_p")) return 1;          /* a paved path */
    return 0;
}
static void au_note(const char *what, const Pc *a, double v) {          /* BBB_AUDIT_PAIRS: where it is */
    const char *pf = getenv("BBB_AUDIT_PAIRS"); FILE *ff = pf && *pf ? fopen(pf, "a") : NULL;
    if (ff) { fprintf(ff, "%s %s [%.2f..%.2f] area %.2f value %.2f at %.1f,%.1f\n", what, a->who, a->z0, a->top, a->area, v, (a->bb[0] + a->bb[2]) / 2, (a->bb[1] + a->bb[3]) / 2); fclose(ff); }
}
int au_scene(Scene *s, const char *style, int n, double mm, const char *theme, FILE *out) {
    Pc *P = NULL; int np = 0, cap = 0, i, j, k; Chk C[NCHK]; char **names = NULL; int nn = 0;
    const double H = SC.H;
    memset(C, 0, sizeof C);
    for (i = 0; i < s->ops.n; i++) {
        Op *o = s->ops.v[i]; int base = in_base(s, o) || (!o->feat && o->kind && (kd_kind_in(KD.bridge, o->kind) || kd_kind_in(KD.terrk, o->kind) || au_groundk(o->kind->s))); char *w;
        { const char *pf = getenv("BBB_AUDIT_KINDS"); FILE *ff = pf && *pf && !o->feat && !in_base(s, o) ? fopen(pf, "a") : NULL; if (ff) { fprintf(ff, "%s %s bridge=%d terrk=%d ground=%d\n", style, o->kind ? o->kind->s : "?", o->kind && kd_kind_in(KD.bridge, o->kind), o->kind && kd_kind_in(KD.terrk, o->kind), base); fclose(ff); } }          /* the ground: laid out before the details, or with the scene (paths, floors, bedrock) */
        if (o->kind && kd_kind_in(KD.soft, o->kind)) continue;          /* grit: loose ground, stands on nothing */
        w = who_of(o, base); names = (char **)realloc(names, sizeof *names * (size_t)(nn + 1)); names[nn++] = w;
        for (j = 0; j < o->polys.n; j++) {
            const GEOSGeometry *g = o->polys.v[j] ? o->polys.v[j]->g : NULL; Pc *p;
            if (!g || shp_is_empty(g)) continue;
            if (np == cap) { cap = cap ? 2 * cap : 256; P = (Pc *)realloc(P, sizeof *P * (size_t)cap); }
            p = &P[np++]; p->g = g; shp_bounds(g, p->bb); p->area = shp_area(g);
            p->z0 = o->n[0].v; p->top = o->type == OP_PRISM ? o->n[1].v : o->n[0].v + o->n[1].v;
            { const char *pf = getenv("BBB_AUDIT_LIST"); FILE *ff = pf && *pf ? fopen(pf, "a") : NULL; if (ff) { fprintf(ff, "%s\t%s\t%.2f\t%.2f\n", w, o->kind ? o->kind->s : "?", p->area, p->top - p->z0); fclose(ff); } }          /* BBB_AUDIT_LIST: every piece, who, kind, area, height */
            p->feat = base ? -1 : o->feat ? o->feat : -1000;          /* laid with the scene: one detail per material (a path of pipe, a row of logs) */ p->base = base; p->op = i; p->k = o->kind; p->who = w;
        }
    }
    /* ---- per piece: collide, straddle, buried, thin, wet */
    double *fz = (double *)malloc(sizeof(double) * (size_t)(np ? np : 1)), *ftop = (double *)malloc(sizeof(double) * (size_t)(np ? np : 1));
    for (i = 0; i < np; i++) { fz[i] = P[i].z0; ftop[i] = P[i].top; for (j = 0; j < np; j++) if (P[j].feat == P[i].feat) { if (P[j].z0 < fz[i]) fz[i] = P[j].z0; if (P[j].top > ftop[i]) ftop[i] = P[j].top; } }
    for (i = 0; i < np; i++) {
        Pc *a = &P[i]; int hit = 0, foot = a->z0 <= fz[i] + 0.01;          /* foot: one of the detail's lowest pieces */
        if (a->base || a->area < 0.05) continue;
        for (j = 0; j < np && !hit; j++) {          /* collide: two different details in the same place, neither standing on the other */
            Pc *b = &P[j]; double x;
            if (j == i || b->base || b->feat == a->feat || !bb_hit(a->bb, b->bb)) continue;
            if ((b->feat == -1000 && strstr(a->who, "cart on track")) || (a->feat == -1000 && strstr(b->who, "cart on track"))) continue;          /* the cart stands on its rails on purpose */
            if (a->z0 >= b->top - 0.2 || b->z0 >= a->top - 0.2) continue;          /* one stands on the other (sunk in by less than 0.2 mm: settled) */
            if ((x = inter_area(a->g, b->g)) > 0.1) { hit = 1; blame(&C[C_COLLIDE], a->who);
                { const char *pf = getenv("BBB_AUDIT_PAIRS"); FILE *ff = pf && *pf ? fopen(pf, "a") : NULL; if (ff) { fprintf(ff, "COLLIDE %s [%.2f..%.2f] x %s [%.2f..%.2f] %.2f mm2 at %.1f,%.1f ops %d %d of %d\n", a->who, a->z0, a->top, b->who, b->z0, b->top, x, (a->bb[0] + a->bb[2]) / 2, (a->bb[1] + a->bb[3]) / 2, a->op, b->op, s->ops.n); fclose(ff); } } }
        }
        if (foot) {          /* straddle: partly on a raised plate of the ground, partly off it; buried: no higher than the plate it is on */
            double cov = 0.0, ptop = -1e9; GEOSGeometry *u = NULL; const char *pk = "";
            for (j = 0; j < np; j++) {
                Pc *b = &P[j]; GEOSGeometry *x, *v;
                if (!b->base || b->top <= H + 0.05 || !bb_hit(a->bb, b->bb)) continue;
                x = shp_intersection(a->g, b->g); if (!x) continue;
                if (shp_area(x) < 1e-3) { shp_free(x); continue; }
                if (b->top > ptop) { ptop = b->top; pk = b->who; }
                v = u ? shp_union(u, x) : x; if (u) { shp_free(u); shp_free(x); } u = v;
            }
            cov = u ? shp_area(u) : 0.0;
            if (cov > 0.3 && a->area - cov > (0.5 > 0.15 * a->area ? 0.5 : 0.15 * a->area)) {          /* off the plates by more than the joints between them? */
                const GEOSGeometry *one[1]; one[0] = u; if (sc_uncovered(a->g, one, 1) > (0.5 > 0.15 * a->area ? 0.5 : 0.15 * a->area)) { blame(&C[C_STRADDLE], a->who); au_note(pk, a, cov); }
            }
            shp_free(u);
            if (cov >= 0.85 * a->area && ftop[i] <= ptop + 0.15) blame(&C[C_BURIED], a->who);
            if (a->z0 > H + 0.05 && cov < 0.3) {          /* stood on another detail: hanging off it? */
                double sup = 0.0; GEOSGeometry *w = NULL;
                for (j = 0; j < np; j++) {
                    Pc *b = &P[j]; GEOSGeometry *x, *v;
                    if (b->base || b->feat == a->feat || fabs(b->top - a->z0) > 0.06 || !bb_hit(a->bb, b->bb)) continue;
                    x = shp_intersection(a->g, b->g); if (!x) continue;
                    v = w ? shp_union(w, x) : x; if (w) { shp_free(w); shp_free(x); } w = v;
                }
                sup = w ? shp_area(w) : 0.0; shp_free(w);
                if (sup > 0.05 && sup < 0.9 * a->area) { blame(&C[C_STRADDLE], a->who); au_note("STRADDLE hangs off", a, sup); }
            }
            if (s->has_terr && cov < 0.3 && a->z0 <= H + 1.5) {          /* on terrain: the step under it */
                Geo h = { (GEOSGeometry *)a->g, 1 << 30 }; Num lo, hi;
                if (T_zr(s, &h, &lo, &hi) >= 0 && hi.v - lo.v > AU.step) { blame(&C[C_STRADDLE], a->who); au_note("STRADDLE terrain step", a, hi.v - lo.v); }
            }
        }
        if (foot && opened_loss(a->g, AU.minw / 2) > (0.15 > 0.1 * a->area ? 0.15 : 0.1 * a->area)) blame(&C[C_THIN], a->who);          /* by its footprint (a mound's upper tiers taper on purpose) */
        {   double wet = 0.0;
            for (j = 0; j < s->gcuts.n; j++) {
                Gcut *c = &s->gcuts.v[j];
                if (!c->kind || !kd_kind_in(KD.wetk, c->kind)) continue;
                for (k = 0; k < c->polys.n; k++) if (c->polys.v[k] && c->polys.v[k]->g) { double bb[4]; shp_bounds(c->polys.v[k]->g, bb); if (bb_hit(a->bb, bb)) wet += inter_area(a->g, c->polys.v[k]->g); }
            }
            if (wet > 0.2 * a->area) blame(&C[C_WET], a->who);
        }
    }
    /* ---- per detail: low, tiny */
    for (i = 0; i < np; i++) {
        Pc *a = &P[i]; double zlo = a->z0, ztop = a->top; GEOSGeometry *u; int first = 1;
        if (a->base) continue;
        for (j = 0; j < i; j++) if (P[j].feat == a->feat) { first = 0; break; }
        if (!first) continue;
        u = shp_clone(a->g);
        for (j = i + 1; j < np; j++) if (P[j].feat == a->feat) {
            GEOSGeometry *v = shp_union(u, P[j].g); shp_free(u); u = v;
            if (P[j].z0 < zlo) zlo = P[j].z0;
            if (P[j].top > ztop) ztop = P[j].top;
        }
        if (ztop - zlo < AU.minh) blame(&C[C_LOW], a->who);
        if (ztop - H > AU.tall) blame(&C[C_TALL], a->who);          /* towers over the base (a figure stands about 25 mm tall) */
        if (u && shp_area(u) < AU.mina) blame(&C[C_TINY], a->who);
        shp_free(u);
    }
    /* ---- cuts */
    for (i = 0; i < s->tcuts.n + s->gcuts.n; i++) {
        const GeoList *ps; double depth; const Kind *kd; char b[96];
        if (i < s->tcuts.n) { Tcut *c = &s->tcuts.v[i]; ps = &c->polys; depth = c->z1.v - c->z0.v; kd = c->kind; }
        else { Gcut *c = &s->gcuts.v[i - s->tcuts.n]; ps = &c->polys; depth = c->depth.v; kd = c->kind; if (kd && kd_kind_in(KD.wetk, kd)) continue; }
        snprintf(b, sizeof b, "cut / %s", kd ? kd->s : "?");
        for (k = 0; k < ps->n; k++) {
            const GEOSGeometry *g = ps->v[k] ? ps->v[k]->g : NULL; double a;
            if (!g || shp_is_empty(g)) continue;
            a = shp_area(g); if (a < 0.02) continue;
            if (opened_loss(g, AU.cutw / 2) > 0.5 * a) { names = (char **)realloc(names, sizeof *names * (size_t)(nn + 1)); names[nn] = _strdup(b); blame(&C[C_CUTN], names[nn++]); }
            if (depth < AU.cutd) { names = (char **)realloc(names, sizeof *names * (size_t)(nn + 1)); names[nn] = _strdup(b); blame(&C[C_CUTS], names[nn++]); }
        }
    }
    /* ---- the line */
    fprintf(out, "{\"style\": "); json_str(out, style); fprintf(out, ", \"n\": %d, \"mm\": %g, \"theme\": ", n, mm); json_str(out, theme ? theme : "");
    fprintf(out, ", \"pieces\": %d", np);
    for (i = 0; i < NCHK; i++) {
        fprintf(out, ", \"%s\": %d, \"%s_who\": {", CN[i], C[i].n, CN[i]);
        for (j = 0; j < C[i].nb; j++) { if (j) fputs(", ", out); json_str(out, C[i].b[j].who); fprintf(out, ": %d", C[i].b[j].n); }
        fputc('}', out);
    }
    fputs("}\n", out); fflush(out);
    for (i = 0; i < nn; i++) free(names[i]);
    free(names); free(P); free(fz); free(ftop);
    return 0;
}
