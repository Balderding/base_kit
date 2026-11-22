/* features.c: group G7 of the C port: features.py (_k, _build, comp, lm, landmarks, true_scale, size_factor, the
   NEW / LANDMARKS tables, extend) and sd_reg.py (real_factor, scaled, d(...).f and its register). See features.h.
   Each function names the Python routine it copies. */
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>
#include "features.h"
#include "pyx.h"
#include "pyexact.h"

#define NF(x) num_f(x)

/* ================================================================== the environment */
static char *ENV_SCALE, *ENV_FINAL, *ENV_NOREAL; static int HAS_SCALE, HAS_FINAL, HAS_NOREAL;
static void env_put(char **dst, int *has, const char *v) { free(*dst); *dst = v ? _strdup(v) : NULL; *has = v != NULL; }
void ft_set_env(const char *kit_scale, const char *kit_final_mm, const char *kit_no_real) {
    env_put(&ENV_SCALE, &HAS_SCALE, kit_scale); env_put(&ENV_FINAL, &HAS_FINAL, kit_final_mm); env_put(&ENV_NOREAL, &HAS_NOREAL, kit_no_real);
}

/* float(str) as CPython parses it: surrounding whitespace, an optional sign, digits with single underscores between
   them, a point, an exponent; or inf / infinity / nan in any case. Anything else is a ValueError. */
static int py_space(unsigned char c) { return c == ' ' || (c >= 9 && c <= 13); }          /* float() strips ASCII whitespace only */
static int ci_eq(const char *a, const char *b, size_t n) { size_t i; for (i = 0; i < n; i++) { char x = a[i]; if (x >= 'A' && x <= 'Z') x = (char)(x + 32); if (x != b[i]) return 0; } return 1; }
int ft_pyfloat(const char *s, double *out) {
    size_t n, i, j, k; char *buf; const char *p; int neg = 0, nd = 0, ok;
    if (!s) return SC_ERR;
    while (*s && py_space((unsigned char)*s)) s++;
    n = strlen(s); while (n && py_space((unsigned char)s[n - 1])) n--;
    if (!n) return SC_ERR;
    p = s; k = n;
    if (*p == '+' || *p == '-') { neg = *p == '-'; p++; k--; }
    if ((k == 3 && ci_eq(p, "inf", 3)) || (k == 8 && ci_eq(p, "infinity", 8))) { *out = neg ? -INFINITY : INFINITY; return 0; }
    if (k == 3 && ci_eq(p, "nan", 3)) { *out = neg ? -NAN : NAN; return 0; }
    /* underscores: only between two digits; then the plain grammar */
    buf = (char *)malloc(n + 1); j = 0; ok = 1;
    for (i = 0; i < n; i++) {
        if (s[i] == '_') {
            if (!(i > 0 && i + 1 < n && s[i - 1] >= '0' && s[i - 1] <= '9' && s[i + 1] >= '0' && s[i + 1] <= '9')) { ok = 0; break; }
            continue;
        }
        buf[j++] = s[i];
    }
    buf[j] = 0;
    if (ok) {          /* [sign] (digits [. [digits]] | . digits) [(e|E) [sign] digits] */
        const char *q = buf;
        if (*q == '+' || *q == '-') q++;
        while (*q >= '0' && *q <= '9') { q++; nd++; }
        if (*q == '.') { q++; while (*q >= '0' && *q <= '9') { q++; nd++; } }
        if (!nd) ok = 0;
        else if (*q == 'e' || *q == 'E') {
            int ne = 0; q++;
            if (*q == '+' || *q == '-') q++;
            while (*q >= '0' && *q <= '9') { q++; ne++; }
            if (!ne) ok = 0;
        }
        if (ok && *q) ok = 0;
    }
    if (ok) *out = strtod(buf, NULL);
    free(buf);
    return ok ? 0 : SC_ERR;
}
/* float(os.environ.get(name, dflt) or dflt): 0, or SC_ERR for a ValueError */
static int env_float(int has, const char *v, double dflt, double *out) {
    if (!has || !v[0]) { *out = dflt; return 0; }
    return ft_pyfloat(v, out);
}

/* features.true_scale: try float(os.environ.get("KIT_SCALE", 1) or 1) except ValueError: 1.0 */
double ft_true_scale(void) { double v; if (env_float(HAS_SCALE, ENV_SCALE, 1.0, &v) < 0) return 1.0; return v; }
/* the object size (KIT_OBJ, 2026-11-20): the base is laid out at its own size and each detail is brought to its real size
   (3 = true beside a 32 mm figure, 1:56); not set, or 1 and under: the classic sizes */
double ft_obj_scale(void) { const char *v = getenv("KIT_OBJ"); double o = (v && *v) ? strtod(v, NULL) : 0.0; return o > 1.0 ? o : 0.0; }
int ft_figure(void) { return ft_true_scale() != 1.0 || ft_obj_scale() > 0; }
/* the model's space on the base, kept clear (work\scale\RESEARCH.md): a model covers about 20 % of a 25-32 mm base, 40 % at
   40-50 mm, 50 % at 60-80 mm and 60 % from 90 mm up (times room). A tank or other wide model gets a long box, hull and
   tracks (1: half sizes *hx, *hy), any other model a disc (0: radius *rad). R is the base's radius. */
int ft_model_space(double mm, double R, const char *mini, double room, double *rad, double *hx, double *hy) {
    double a = (mm < 39.9 ? 0.2 : mm < 59.9 ? 0.4 : mm < 89.9 ? 0.5 : 0.6) * (room > 0 ? room : 1.0); int dflt = !mini || !*mini || !strcmp(mini, "default") || !strcmp(mini, "mix");
    int wide = mm >= 59.9 && ((mini && (!strcmp(mini, "tracked") || !strcmp(mini, "wide") || !strcmp(mini, "hover") || !strcmp(mini, "quad") || !strcmp(mini, "many"))) || (dflt && mm >= 89.9));
    if (a > 0.9) a = 0.9;
    if (wide) {
        *hx = 0.9 * R; *hy = a * 3.14159265 * R / 3.6;
        if (ft_obj_scale() > 0) {          /* true size (2026-11-21): a real main battle tank, 3.5-3.8 m wide and about 7.8 m hull: at 1:56 65 x 140 mm (times the size slider), cut to the base */
            double m = 6.0 * ft_obj_scale();          /* mm a metre */
            if (*hy < 1.8 * m) *hy = 1.8 * m;
            if (*hx > 3.9 * m) *hx = 3.9 * m;
        }
        if (*hy > 0.8 * R) *hy = 0.8 * R;
        *rad = 0; return 1;
    }
    *rad = sqrt(a) * R; *hx = *hy = 0; return 0;
}          /* a base for a model at true size: clear middle, no landmark */
/* features.size_factor */
double ft_size_factor(double mm) {
    if (ft_true_scale() != 1.0) return 1.6;
    return mm < 80 ? 1.6 : mm <= 100 ? 2.1 : mm <= 130 ? 2.5 : 2.9;
}

/* ================================================================== small helpers */
int64_t ft_caught;
/* gg3.BRIDGE, with the floor kinds style_obelis adds to it while gg3 loads */
static const char *BRIDGE[] = { "plate", "tflag", "basalt", "alien", "ice", "duck", "plank", "tatami", "flag", "fleshfloor", "board", "shelf", "obfloor" };
int ft_in_bridge(const char *kind) { size_t i; if (!kind) return 0; for (i = 0; i < sizeof BRIDGE / sizeof *BRIDGE; i++) if (!strcmp(kind, BRIDGE[i])) return 1; return 0; }
/* features._k(kind): kind + "_p" if kind in BRIDGE else kind */
const char *ft_k(const char *kind) {
    char b[256];
    if (!ft_in_bridge(kind)) return kind ? so_intern(kind) : NULL;
    snprintf(b, sizeof b, "%s_p", kind);
    return so_intern(b);
}
static const char *ROUND_KINDS[] = { "bone", "root", "rope", "vein", "tendril", "cable", "twig", "tooth", "chain", "sack", "egg", "pustule", "flesh", "moss" };
static int round_kind(const char *s) { size_t i; for (i = 0; i < sizeof ROUND_KINDS / sizeof *ROUND_KINDS; i++) if (!strcmp(s, ROUND_KINDS[i])) return 1; return 0; }

#define TI(f, a) { { f, NUM_FLOAT, 0 }, { a, NUM_INT, 0 } }
const FtTiers FT_SOFT_T = { 2, { TI(0.55, 25), TI(0.45, 50) } };
const FtTiers FT_ROCK_T = { 2, { TI(0.7, 6), TI(0.3, 40) } };
const FtTiers FT_SPIKE_T = { 2, { TI(0.75, 4), TI(0.25, 30) } };
const FtTiers FT_PEB_T = { 2, { TI(0.7, 10), TI(0.3, 45) } };
const FtTiers FT_ROUND_T = { 3, { TI(0.45, 12), TI(0.3, 38), TI(0.25, 62) } };
const FtTiers FT_GEM_T = { 2, { TI(0.5, 4), TI(0.5, 24) } };          /* an outcrop's crystal: half straight, half a long point */

/* ================================================================== steps payloads */
struct FtSteps { int rc, later, filled; SoPieces p; FtFillFn fill; void *ctx; void (*ctx_free)(void *); };
static FtSteps **ALLSTEPS; static int nall, capall;
static FtSteps *steps_new(int later) {
    FtSteps *f = (FtSteps *)calloc(1, sizeof *f); f->rc = 1; f->later = later;
    if (nall == capall) { capall = capall ? 2 * capall : 64; ALLSTEPS = (FtSteps **)realloc(ALLSTEPS, sizeof(FtSteps *) * (size_t)capall); }
    ALLSTEPS[nall++] = f;
    return f;
}
static void pieces_push(SoPieces *p, GEOSGeometry *g, double ztop, int64_t c, double z, int ghost) {
    if (p->n == p->cap) { p->cap = p->cap ? 2 * p->cap : 16; p->v = (SoPiece *)realloc(p->v, sizeof(SoPiece) * (size_t)p->cap); }
    p->v[p->n].g = g; p->v[p->n].ztop = ztop; p->v[p->n].c = c; p->v[p->n].z = z; p->v[p->n].ghost = ghost; p->n++;
}
static void pieces_copy(SoPieces *dst, const SoPieces *src) {
    int i; for (i = 0; i < src->n; i++) pieces_push(dst, src->v[i].g ? shp_clone(src->v[i].g) : NULL, src->v[i].ztop, src->v[i].c, src->v[i].z, src->v[i].ghost);
}
FtSteps *ft_steps_list(const SoPieces *p) { FtSteps *f = steps_new(0); f->filled = 1; if (p) pieces_copy(&f->p, p); return f; }
FtSteps *ft_steps_ready(const SoPieces *p) { FtSteps *f = steps_new(1); f->filled = 1; if (p) pieces_copy(&f->p, p); return f; }
FtSteps *ft_steps_later(FtFillFn fill, void *ctx, void (*ctx_free)(void *)) { FtSteps *f = steps_new(1); f->fill = fill; f->ctx = ctx; f->ctx_free = ctx_free; return f; }
void ft_steps_release(void) {
    int i;
    for (i = 0; i < nall; i++) { FtSteps *f = ALLSTEPS[i]; sop_free(&f->p); if (f->ctx_free) f->ctx_free(f->ctx); free(f); }
    free(ALLSTEPS); ALLSTEPS = NULL; nall = capall = 0;
}
/* iterating L[2]: a _Later fills itself first (_Later._fill drops its maker BEFORE running it, so one that raised
   stays empty) */
const SoPieces *ft_pieces(const SoVal *v) {
    if (v->t == LV_LATER) return so_later_pieces(v->later);
    if (v->t == LV_PTR && v->p) {
        FtSteps *f = (FtSteps *)v->p;
        if (!f->filled) {
            f->filled = 1;
            if (f->fill(f->ctx, &f->p) != SO_OK) { sop_free(&f->p); return NULL; }
        }
        return &f->p;
    }
    return NULL;          /* iterating None or a number: TypeError */
}
int ft_is_later(const SoVal *v) { return v->t == LV_LATER || (v->t == LV_PTR && v->p && ((FtSteps *)v->p)->later); }

int (*ft_top3)(int64_t c, double *top3) = so_top3;
void ft_s3d_free(FtS3dList *l) { free(l->v); l->v = NULL; l->n = l->cap = 0; }
static void s3d_push(FtS3dList *l, int64_t c, double zb, double zs, double zlo, const GEOSGeometry *g) {
    if (!l) return;
    if (l->n == l->cap) { l->cap = l->cap ? 2 * l->cap : 8; l->v = (FtS3d *)realloc(l->v, sizeof(FtS3d) * (size_t)l->cap); }
    l->v[l->n].c = c; l->v[l->n].zb = zb; l->v[l->n].zs = zs; l->v[l->n].zlo = zlo; l->v[l->n].hasbb = 0;
    if (g && !shp_is_empty(g)) { shp_bounds(g, l->v[l->n].bb); l->v[l->n].hasbb = 1; }
    l->n++;
}

/* ================================================================== values of a layer tuple */
static int vnum(const SoVal *v, Num *n) {          /* a Python number (int / float); else Python raises (TypeError) */
    if (v->t == LV_INT) { *n = num_i((double)v->i); return 0; }
    if (v->t == LV_FLOAT) { *n = num_f(v->f); return 0; }
    return SC_ERR;
}
static int truthy(Num n) { return n.t != NUM_NONE && n.v != 0.0; }
/* L[i] as a layer index (a dict key: an int, or a float equal to one); 0 = no such key possible */
static int vkey(const SoVal *v, int64_t *k) {
    if (v->t == LV_INT) { *k = v->i; return 1; }
    if (v->t == LV_FLOAT && v->f == floor(v->f) && fabs(v->f) < 9e15) { *k = (int64_t)v->f; return 1; }
    return 0;
}
static const char *vstr(const SoVal *v) { return v->t == LV_STR ? v->s : NULL; }
/* polys_of(L[1]) (None -> []); SC_ERR if L[1] is not a geometry */
static int vpolys(const SoVal *v, GeoList *out) {
    if (v->t == LV_NONE) return 0;
    if (v->t != LV_GEOM || !v->g) return SC_ERR;
    lf_polys_of_new(shp_clone(v->g), out);
    return 0;
}
static Kind *kd_of(const char *s) { return kind_str(s ? s : "None"); }          /* (the kinds are str here; None never reaches the scene) */
/* features._u(gs) */
static GEOSGeometry *u_list(const GeoList *l) {
    const GEOSGeometry **gs = (const GEOSGeometry **)malloc(sizeof(GEOSGeometry *) * (size_t)(l->n + 1)); int i, n = 0; GEOSGeometry *u;
    for (i = 0; i < l->n; i++) if (l->v[i] && !shp_is_empty(l->v[i]->g)) gs[n++] = l->v[i]->g;
    u = shp_union_all(gs, n); free(gs);
    return u;
}

/* ================================================================== features._build */
typedef struct { int set; Num top; GEOSGeometry *g; } Top;            /* tops[i] = (top, shape) */
typedef struct { int set; const GEOSGeometry *g; Num zb; double b; } Shp;   /* shape[i] = (L[1], zb, b) */

typedef struct { Scene *s; Num z; const SoLayers *L; Top *tops; Shp *shape; int n; int done; FtS3dList *s3d; } Bd;
static Top *top_get(Bd *B, const SoVal *key) { int64_t k; if (!vkey(key, &k) || k < 0 || k >= B->n || !B->tops[k].set) return NULL; return &B->tops[k]; }
static Shp *shape_get(Bd *B, const SoVal *key) { int64_t k; if (!vkey(key, &k) || k < 0 || k >= B->n || !B->shape[k].set) return NULL; return &B->shape[k]; }
static void top_put(Top *t, Num top, GEOSGeometry *g) { shp_free(t->g); t->set = 1; t->top = top; t->g = g; }

/* [q for q in ps if wide_enough(q, w)] (w < 0: the default MINW) */
static int keep_wide(GeoList *ps, double w, GeoList *out) {
    int i; for (i = 0; i < ps->n; i++) { int k = lf_wide_enough(ps->v[i], w < 0 ? SC.MINW : w); if (k < 0) return SC_ERR; if (k) gl_push(out, ps->v[i]); }
    return 0;
}
static void keep_area(GeoList *ps, double amin, GeoList *out) { int i; for (i = 0; i < ps->n; i++) if (shp_area(ps->v[i]->g) > amin) gl_push(out, ps->v[i]); }
/* polys_of(a.intersection(inset(b, d))) */
static int cut_in(const GEOSGeometry *a, const GEOSGeometry *b, double d, GeoList *out) {
    GEOSGeometry *in = shp_inset(b, d), *q;
    if (!in) return SC_ERR;
    q = shp_intersection(a, in); shp_free(in);
    if (!q) return SC_ERR;
    lf_polys_of_new(q, out);
    return 0;
}

#define TRY(x) do { if ((x) < 0) goto fail; } while (0)
static int layer_up(Bd *B, int i, const SoLayer *L, int carry) {
    GeoList all = { 0 }, ps = { 0 }; Num h, top; int r = SC_ERR; Scene *s = B->s;
    if (!carry) {          /* str(L[3]) in ROUND_KINDS and L[2] < 1.6 */
        const char *k3; char nb[64];
        if (L->n < 4) goto fail;
        k3 = vstr(&L->v[3]);
        if (!k3) { if (L->v[3].t == LV_NONE) k3 = "None"; else if (L->v[3].t == LV_INT) { snprintf(nb, sizeof nb, "%lld", (long long)L->v[3].i); k3 = nb; } else k3 = ""; }
        if (round_kind(k3)) {
            TRY(vnum(&L->v[2], &h));
            if (h.v < 1.6) {
                TRY(vpolys(&L->v[1], &all)); TRY(keep_wide(&all, 0.7, &ps));
                if (ps.n) { TRY(pl_mound(s, &ps, B->z, h, FT_ROUND_T.t, FT_ROUND_T.n, kd_of(ft_k(vstr(&L->v[3]))))); B->done = 1; }
                r = 0; goto fail;
            }
        }
    }
    if (L->n < 2) goto fail;
    TRY(vpolys(&L->v[1], &all)); TRY(keep_wide(&all, -1, &ps));
    if (ps.n) {
        if (L->n < 4) goto fail;
        TRY(vnum(&L->v[2], &h));
        TRY(pl_up(s, &ps, B->z, h, kd_of(ft_k(vstr(&L->v[3]))), &top));
        top_put(&B->tops[i], top, L->v[1].t == LV_GEOM ? shp_clone(L->v[1].g) : NULL);
        B->done = B->done || truthy(top);
    }
    r = 0;
fail:
    gl_free(&all); gl_free(&ps);
    return r;
}
static int layer_mound(Bd *B, const SoLayer *L) {
    GeoList all = { 0 }, ps = { 0 }; Num h; int r = SC_ERR; const FtTiers *T = &FT_ROCK_T;
    if (L->n < 2) goto fail;
    TRY(vpolys(&L->v[1], &all)); TRY(keep_wide(&all, 0.7, &ps));
    if (ps.n) {
        if (L->n < 3) goto fail;
        TRY(vnum(&L->v[2], &h));
        if (L->n > 4) { if (L->v[4].t != LV_PTR || !L->v[4].p) goto fail; T = (const FtTiers *)L->v[4].p; }
        if (L->n < 4) goto fail;
        TRY(pl_mound(B->s, &ps, B->z, h, T->t, T->n, kd_of(ft_k(vstr(&L->v[3])))));
        B->done = 1;
    }
    r = 0;
fail:
    gl_free(&all); gl_free(&ps);
    return r;
}
static int layer_steps(Bd *B, int i, const SoLayer *L) {
    Scene *s = B->s; Top *par = NULL; Num zb, zlow = num_none(), best_top = num_none(); GEOSGeometry *best_g = NULL; int has_best = 0, rough, k, r = SC_ERR, has_parent;
    double b = 1.0, hmax = 0.0; const SoPieces *P; Geo *foot = NULL;
    if (L->n < 3) goto fail;
    has_parent = L->n > 4 && L->v[4].t != LV_NONE;
    if (has_parent) { par = top_get(B, &L->v[4]); if (!par || !truthy(par->top)) return 0; }          /* continue */
    zb = par ? par->top : B->z;
    {   /* smooth details: a layer whose top gets cut (a pot's mouth, a groove) stays as steps */
        int cut = 0, j; int64_t pk;
        for (j = 0; j < B->n && !cut; j++) { const SoLayer *T = B->L->v[j]; if (T && T->n >= 3 && T->v[0].t == LV_STR && !strcmp(T->v[0].s, "tcut") && vkey(&T->v[2], &pk) && pk == i) cut = 1; }
        so_smooth_block = cut;
        P = ft_pieces(&L->v[2]);
        so_smooth_block = 0;
    }
    if (!P) goto fail;
    /* max((q[1] for q in L[2]), default=0.0) */
    if (P->n) { hmax = P->v[0].ztop; for (k = 1; k < P->n; k++) if (P->v[k].ztop > hmax) hmax = P->v[k].ztop; } else hmax = 0.0;
    b = hmax >= 1.0 ? PL.hboost : 1.0;
    rough = pl_terr(s) && !par;
    if (rough) {
        Num lo, hi;
        if (L->v[1].t != LV_GEOM) goto fail;
        foot = geo_new(shp_clone(L->v[1].g));
        TRY(T_zr(s, foot, &lo, &hi)); zlow = lo; zb = hi;
    }
    for (k = 0; k < P->n; k++) {
        const SoPiece *pc = &P->v[k]; Kind *kd; GeoList all = { 0 }, ps = { 0 }; Num top, got = num_none(); int e = 0;
        if (L->n < 4) goto fail;          /* kd = _k(L[3]) */
        kd = kind_lk(ft_k(vstr(&L->v[3])) ? ft_k(vstr(&L->v[3])) : "None");          /* gg.LK(_k(L[3])) */
        kd->has_loft = 1; kd->loft_chain = num_i((double)pc->c); kd->loft_z = num_add(zb, NF(pc->z * b));
        if (pc->ghost) {          /* the marker of true 3D solids */
            double t3;
            kd->has_ghost = 1; kd->ghost = num_i((double)pc->c);
            if (!ft_top3 || !ft_top3(pc->c, &t3)) t3 = pc->ztop;
            s3d_push(B->s3d, pc->c, zb.v, b * pc->ztop / (t3 > 0.01 ? t3 : 0.01), rough ? zlow.v : zb.v, pc->g);
        }
        if (par) e = cut_in(pc->g, par->g, 0.12, &all);
        else if (pc->g) lf_polys_of_new(shp_clone(pc->g), &all);
        if (e < 0) { kind_unref(kd); goto fail; }
        keep_area(&all, 0.12, &ps); gl_free(&all);
        if (!ps.n) { kind_unref(kd); continue; }
        top = num_add(zb, NF(pc->ztop * b));
        if (par || rough || (k && (has_best || P->v[0].ghost))) {
            if (num_sub(top, zb).v >= 0.1) {
                GeoList nw = { 0 }; Num z0 = rough ? num_sub(zlow, NF(0.2)) : num_sub(par ? zb : num_max2(zb, NF(SC.H)), NF(0.15));
                e = sc_prism(s, &ps, top, kd, z0, 0.45, 0, &nw);
                if (e >= 0 && nw.n) { got = top; if (rough && !has_best) { gl_extend(&s->feet, &nw); gl_extend(&s->objs, &nw); } }
                gl_free(&nw);
            }
        } else if (B->z.t == NUM_Z && B->z.bridge && !has_best) {          /* on a plank floor: the usual way (it notches the planks) */
            double b0 = PL.hboost; GeoList w = { 0 };
            PL.hboost = 1.0;
            e = keep_wide(&ps, -1, &w);
            if (e >= 0) e = pl_up(s, &w, zb, NF(pc->ztop * b), kd, &got);
            PL.hboost = b0; gl_free(&w);
        } else if (num_sub(top, zb).v >= 0.1) {
            GeoList nw = { 0 };
            e = sc_prism(s, &ps, top, kd, zb.v <= SC.H ? num_none() : num_sub(zb, NF(0.15)), 0.45, zb.v <= SC.H, &nw);
            if (e >= 0 && nw.n) got = top;
            gl_free(&nw);
        }
        kind_unref(kd);
        if (e < 0) { gl_free(&ps); goto fail; }
        if (truthy(got) && pc->ghost) { B->done = 1; gl_free(&ps); continue; }
        if (truthy(got) && (!has_best || got.v > best_top.v)) { shp_free(best_g); best_g = u_list(&ps); has_best = 1; best_top = got; if (!best_g) { gl_free(&ps); goto fail; } }
        gl_free(&ps);
    }
    if (has_best) { top_put(&B->tops[i], best_top, best_g); best_g = NULL; B->done = 1; B->shape[i].set = 1; B->shape[i].g = L->v[1].t == LV_GEOM ? L->v[1].g : NULL; B->shape[i].zb = zb; B->shape[i].b = b; }
    r = 0;
fail:
    shp_free(best_g); geo_unref(foot);
    return r;
}
static int layer_on(Bd *B, int i, const SoLayer *L) {
    Top *par; GeoList all = { 0 }, ps = { 0 }; int r = SC_ERR, j; Num h, top;
    if (L->n < 3) goto fail;
    par = top_get(B, &L->v[2]);
    if (!par || !truthy(par->top) || par->top.v + 0.3 > SC.CAP) return 0;
    if (L->n < 2) goto fail;
    if (L->v[1].t != LV_GEOM || !par->g) goto fail;
    TRY(cut_in(L->v[1].g, par->g, 0.12, &all));
    for (j = 0; j < all.n; j++) { int w = lf_wide_enough(all.v[j], 0.45); if (w < 0) goto fail; if (w && shp_area(all.v[j]->g) > 0.15) gl_push(&ps, all.v[j]); }
    if (ps.n) {
        GeoList nw = { 0 }; GEOSGeometry *u;
        if (L->n < 4) goto fail;
        TRY(vnum(&L->v[3], &h));
        top = num_add(par->top, num_mul(h, NF(h.v >= 1.0 ? PL.hboost : 1.0)));
        top = num_min2(top, NF(SC.CAP));
        if (L->n < 5) goto fail;
        if (sc_prism(B->s, &ps, top, kd_of(ft_k(vstr(&L->v[4]))), num_sub(par->top, NF(0.15)), SC.MINW, 0, &nw) < 0) { gl_free(&nw); goto fail; }
        gl_free(&nw);
        u = u_list(&ps); if (!u) goto fail;
        top_put(&B->tops[i], top, u);
    }
    r = 0;
fail:
    gl_free(&all); gl_free(&ps);
    return r;
}
static int layer_tcut(Bd *B, const SoLayer *L) {
    Top *par; Shp *sh; GeoList all = { 0 }, ps = { 0 }; int r = SC_ERR; Num dep, l3;
    if (L->n < 3) goto fail;
    par = top_get(B, &L->v[2]);
    if (!par || !truthy(par->top)) return 0;
    sh = shape_get(B, &L->v[2]);
    if (L->n < 2) goto fail;
    if (L->v[1].t != LV_GEOM) goto fail;
    if (sh ? !sh->g : !par->g) goto fail;
    TRY(cut_in(L->v[1].g, sh ? sh->g : par->g, 0.3, &all));
    keep_area(&all, 0.05, &ps);
    if (L->n < 4) goto fail;
    if (sh && L->n > 4 && L->v[4].t == LV_STR && !strcmp(L->v[4].s, "floor")) {
        TRY(vnum(&L->v[3], &l3));
        dep = num_sub(par->top, num_add(sh->zb, num_mul(l3, NF(sh->b))));
    } else TRY(vnum(&L->v[3], &dep));
    if (ps.n && dep.v > 0.05) TRY(pl_topcut(B->s, &ps, par->top, dep, kind_str("slot")));
    r = 0;
fail:
    gl_free(&all); gl_free(&ps);
    return r;
}
static int layer_gcut(Bd *B, const SoLayer *L) {
    GeoList all = { 0 }, ps = { 0 }; int r = SC_ERR; Num d;
    if (L->n < 2) goto fail;
    TRY(vpolys(&L->v[1], &all)); keep_area(&all, 0.08, &ps);
    if (ps.n) {
        if (L->n < 4) goto fail;
        TRY(vnum(&L->v[2], &d));
        TRY(pl_cut(B->s, &ps, B->z, d, kd_of(vstr(&L->v[3]))));
        B->done = 1;
    }
    r = 0;
fail:
    gl_free(&all); gl_free(&ps);
    return r;
}
/* gg.COL.setdefault(L[2] + "_pad", gg.COL.get(L[2], "#999999")): the pictures' colour table is kinds.c's; the integration
   points this hook at it (cport\pipeline.c). NULL (a proof build without kinds.c): not kept */
void (*ft_col_setdefault)(const char *k2, const char *kind);
static int layer_pad(Bd *B, const SoLayer *L) {
    GeoList all = { 0 }, ps = { 0 }; int r = SC_ERR; char kb[256];
    if (!(B->z.v <= SC.H + 1e-6) || pl_terr(B->s)) return 0;
    if (L->n < 2) goto fail;
    TRY(vpolys(&L->v[1], &all)); TRY(keep_wide(&all, 0.92, &ps));
    if (ps.n) {
        GeoList nw = { 0 };
        if (L->n < 3 || L->v[2].t != LV_STR) goto fail;          /* L[2] + "_pad" */
        snprintf(kb, sizeof kb, "%s_pad", L->v[2].s);
        if (ft_col_setdefault) ft_col_setdefault(kb, L->v[2].s);
        if (sc_prism(B->s, &ps, NF(SC.H - 0.12), kind_str(kb), NF(SC.H - 0.8), SC.MINW, 0, &nw) < 0) { gl_free(&nw); goto fail; }
        gl_free(&nw);
    }
    r = 0;
fail:
    gl_free(&all); gl_free(&ps);
    return r;
}

int ft_build(Scene *s, Num z, const SoLayers *layers, FtS3dList *s3d) {
    Bd B; int i, j, n = layers->n; char *carry;
    /* carry = {L[2] for L in layers if L[0] in ("on", "tcut")}: outside the try, so a None layer or a short "on" raises */
    for (i = 0; i < n; i++) {
        const SoLayer *L = layers->v[i];
        if (!L || L->n < 1) return SC_ERR;
        if (L->v[0].t == LV_STR && (!strcmp(L->v[0].s, "on") || !strcmp(L->v[0].s, "tcut")) && L->n < 3) return SC_ERR;
    }
    carry = (char *)calloc((size_t)n + 1, 1);
    for (i = 0; i < n; i++) {
        const SoLayer *L = layers->v[i]; int64_t k;
        if (L->v[0].t == LV_STR && (!strcmp(L->v[0].s, "on") || !strcmp(L->v[0].s, "tcut")) && vkey(&L->v[2], &k) && k >= 0 && k < n) carry[k] = 1;
    }
    B.s = s; B.z = z; B.L = layers; B.n = n; B.done = 0; B.s3d = s3d;
    B.tops = (Top *)calloc((size_t)n + 1, sizeof(Top)); B.shape = (Shp *)calloc((size_t)n + 1, sizeof(Shp));
    for (i = 0; i < n; i++) {
        const SoLayer *L = layers->v[i]; const char *t = vstr(&L->v[0]); int r = 0;
        if (!t) continue;
        if (!strcmp(t, "up")) r = layer_up(&B, i, L, carry[i]);
        else if (!strcmp(t, "mound")) r = layer_mound(&B, L);
        else if (!strcmp(t, "steps")) r = layer_steps(&B, i, L);
        else if (!strcmp(t, "on")) r = layer_on(&B, i, L);
        else if (!strcmp(t, "tcut")) r = layer_tcut(&B, L);
        else if (!strcmp(t, "gcut")) r = layer_gcut(&B, L);
        else if (!strcmp(t, "pad")) r = layer_pad(&B, L);
        if (r < 0) ft_caught++;          /* kitlog.caught(f"feature layer {t}", e) */
    }
    j = B.done;
    for (i = 0; i < n; i++) shp_free(B.tops[i].g);
    free(B.tops); free(B.shape); free(carry);
    return j;
}

/* ================================================================== comp, lm, landmarks */
typedef struct { FtBuildFn build; void *bctx; PyRandom *rng; int has_scale; double scale; int lm; double k; int have; Geo *foot; SoLayers lay; double r, Rr; } Mk;
static void hold_put(Mk *m, Geo *foot, SoLayers *lay) {          /* hold["g"] = got */
    geo_unref(m->foot); lyl_free(&m->lay);
    m->foot = foot; m->lay = *lay; memset(lay, 0, sizeof *lay); m->have = 1;
}
/* comp.fn.mk(cx, cy): got = build(cx, cy, aim(rng, cx, cy), rng[, scale]); None -> None; hold["g"] = got; got[0]
   lm.fn.mk(cx, cy):   got = build(cx, cy, rng.uniform(0, 2 * math.pi), rng, k); None or got[0] None -> None */
static Geo *mk_make(void *c, int has_c, double cx, double cy, int *err) {
    Mk *m = (Mk *)c; double a; Geo *foot = NULL; SoLayers lay = { 0 }; int r;
    if (!has_c) { *err = 1; return NULL; }          /* find always calls make(cx, cy) here (at is given) */
    a = m->lm ? py_uniform(m->rng, 0, 2 * 3.141592653589793) : lf_aim(m->rng, cx, cy, 2 * 3.141592653589793);
    r = m->lm ? m->build(m->bctx, cx, cy, a, m->rng, 1, m->k, &foot, &lay) : m->build(m->bctx, cx, cy, a, m->rng, m->has_scale, m->scale, &foot, &lay);
    if (r < 0) { geo_unref(foot); lyl_free(&lay); *err = 1; return NULL; }
    if (r == 1 || (m->lm && !foot)) { geo_unref(foot); lyl_free(&lay); return NULL; }
    hold_put(m, foot, &lay);
    return geo_ref(foot);
}
static int comp_at(void *c, double *x, double *y) { Mk *m = (Mk *)c; lf_rp(m->rng, 0, m->r, x, y); return 0; }          /* lambda: rp(rng, 0, r) */
/* lm's at: (lambda d, t: (d * cos(t), d * sin(t)))(Rr * sqrt(rng.random()), rng.uniform(0, 6.283)) */
static int lm_at(void *c, double *x, double *y) {
    Mk *m = (Mk *)c; double d = m->Rr * sqrt(py_random(m->rng)), t = py_uniform(m->rng, 0, 6.283);
    *x = d * py_cos(t); *y = d * py_sin(t);
    return 0;
}

FtComp ft_comp(FtBuildFn build, void *ctx, double r, double gap, int ground, int tries) {
    FtComp c; memset(&c, 0, sizeof c); c.build = build; c.ctx = ctx; c.r = r; c.gap = gap; c.ground = ground; c.tries = tries; c.kind = ""; c.shape = ""; c.cls = "";
    return c;
}
/* comp(build, r, gap, ground, tries).fn(s, rng, lvl=0, scale=1.0) */
int ft_comp_run(const FtComp *c, Scene *s, PyRandom *rng, double scale, FtS3dList *s3d) {
    Mk m = { c->build }; Geo *g = NULL; Num z = num_none(); int r;
    m.bctx = c->ctx; m.rng = rng; m.has_scale = scale != 1.0; m.scale = scale; m.r = c->r;
    r = pl_find(s, mk_make, &m, c->tries, c->gap, NULL, c->ground, NULL, comp_at, &m, &g, &z);
    geo_unref(g);
    if (r == 1) r = ft_build(s, z, &m.lay, s3d);
    else if (r == 0) r = 0;
    geo_unref(m.foot); lyl_free(&m.lay);
    return r < 0 ? SC_ERR : r;
}

/* p.intersects(gb) for every polygon of a list of cuts */
static int any_hits(const GeoList *ps, const GEOSGeometry *gb) { int j, h; for (j = 0; j < ps->n; j++) { h = shp_intersects(ps->v[j]->g, gb); if (h) return h; } return 0; }
/* the landmark's fallback: a spot where only small loose pieces are in the way, and those removed. 1 = found (z = H),
   0 = none in 40 tries, SC_ERR */
static int lm_clear(Mk *m, Scene *s, double gap) {
    int it, i, j;
    for (it = 0; it < 40; it++) {
        double x, y; int err = 0, bad = 0; Geo *g; GEOSGeometry *gb; GeoList blk = { 0 }, hd = { 0 };
        lm_at(m, &x, &y);
        g = mk_make(m, 1, x, y, &err);
        if (err) return SC_ERR;
        if (!g) continue;
        { int c = shp_contains(PL.disc->g, g->g); if (c < 0) { geo_unref(g); return SC_ERR; } if (!c) { geo_unref(g); continue; } }
        gb = shp_buffer_q(g->g, gap, 16); geo_unref(g);
        if (!gb) return SC_ERR;
        if (pl_hard(s, NULL, &hd) < 0) { shp_free(gb); gl_free(&hd); return SC_ERR; }
        for (i = 0; i < hd.n; i++) { int h = shp_intersects(hd.v[i]->g, gb); if (h < 0) { bad = -1; break; } if (h) gl_push(&blk, hd.v[i]); }
        gl_free(&hd);
        if (bad < 0) { shp_free(gb); gl_free(&blk); return SC_ERR; }
        for (i = 0; i < blk.n && !bad; i++) if (shp_area(blk.v[i]->g) >= 10) bad = 1;
        gl_free(&blk);
        for (i = 0; i < s->gcuts.n && !bad; i++) bad = any_hits(&s->gcuts.v[i].polys, gb);
        for (i = 0; i < s->tcuts.n && !bad; i++) bad = any_hits(&s->tcuts.v[i].polys, gb);
        for (i = 0; i < s->ops.n && !bad; i++) for (j = 0; j < s->ops.v[i]->polys.n && !bad; j++) {
            const GEOSGeometry *p = s->ops.v[i]->polys.v[j]->g;
            if (shp_area(p) >= 10) bad = shp_intersects(p, gb);
        }
        if (bad < 0) { shp_free(gb); return SC_ERR; }
        if (bad) { shp_free(gb); continue; }
        {          /* s.ops = [o for o in ((op[0], [p for p in op[1] if not p.intersects(gb)]) + tuple(op[2:]) for op in s.ops) if o[1]] */
            OpList no = { 0 }; GeoList nf = { 0 }; ChopList nc = { 0 };
            for (i = 0; i < s->ops.n; i++) {
                Op *o = s->ops.v[i]; GeoList keep = { 0 };
                for (j = 0; j < o->polys.n; j++) if (!shp_intersects(o->polys.v[j]->g, gb)) gl_push(&keep, o->polys.v[j]);
                if (keep.n) ol_push_own(&no, op_like(o, &keep));
                gl_free(&keep);
            }
            ol_free(&s->ops); s->ops = no;
            for (i = 0; i < s->feet.n; i++) if (!shp_intersects(s->feet.v[i]->g, gb)) gl_push(&nf, s->feet.v[i]);
            gl_free(&s->feet); s->feet = nf;
            for (i = 0; i < s->chops.n; i++) if (!shp_intersects(s->chops.v[i]->poly->g, gb)) { s->chops.v[i]->rc++; chl_push_own(&nc, s->chops.v[i]); }
            { ChopList old = s->chops; s->chops = nc; for (i = 0; i < old.n; i++) { Chop *c = old.v[i]; if (--c->rc == 0) { kind_unref(c->kind); geo_unref(c->poly); ol_free(&c->ops); free(c); } } free(old.v); }
        }
        shp_free(gb);
        return 1;
    }
    return 0;
}
/* lm(build, r_frac, gap, ground).fn(s, rng, k) */
int ft_lm_run(const FtLm *l, Scene *s, PyRandom *rng, double k, FtS3dList *s3d) {
    Mk m = { l->build }; Geo *g = NULL; Num z = num_none(); int r;
    m.bctx = l->ctx; m.rng = rng; m.lm = 1; m.k = k; m.Rr = SC.R0 * l->r_frac;
    r = pl_find(s, mk_make, &m, 60, l->gap, NULL, l->ground, NULL, lm_at, &m, &g, &z);
    geo_unref(g);
    if (r == 0 && !pl_terr(s)) { r = lm_clear(&m, s, l->gap); if (r == 1) z = NF(SC.H); }
    if (r == 1) r = ft_build(s, z, &m.lay, s3d);
    geo_unref(m.foot); lyl_free(&m.lay);
    return r < 0 ? SC_ERR : r;
}
/* features.landmarks(s, style, rng) */
int ft_landmarks(Scene *s, const FtLmEntry *pool0, int n, PyRandom *rng, double mm, FtS3dList *s3d) {
    int64_t *ix = (int64_t *)malloc(sizeof(int64_t) * (size_t)(n + 1)); int np = 0, i, want, placed = 0; double k, kk[3];
    for (i = 0; i < n; i++) if (mm >= pool0[i].lo - 0.01) ix[np++] = i;
    if (!np) { free(ix); return 0; }
    want = mm < 80 ? 1 : mm <= 100 ? 2 : mm <= 130 ? 3 : 4;
    k = ft_size_factor(mm); kk[0] = k; kk[1] = 0.8 * k; kk[2] = 0.62 * k;
    py_shuffle_i64(rng, ix, np);
    for (i = 0; i < np; i++) {
        int j;
        if (placed >= want) break;
        for (j = 0; j < 3; j++) {
            int r = ft_lm_run(&pool0[ix[i]].lm, s, rng, kk[j], s3d);
            if (r < 0) { ft_caught++; break; }
            if (r) { placed++; break; }
        }
    }
    free(ix);
    return placed;
}

/* ================================================================== the tables */
/* _tag_builders' wrapper: g.tag = n + (":" + str(a[0]) if n == "tool" and a else "");
   g.kind = (k.get("kind") or (a[0] if a and isinstance(a[0], str) and n != "tool" else "")) or
            (a[1] if n == "tool" and len(a) > 1 else "iron" if n == "tool" else "") */
static void arg_str(const FtArg *a, char *b, int nb) {          /* str(x) of a table argument */
    if (a->t == 's') snprintf(b, (size_t)nb, "%s", a->s);
    else if (a->t == 'i') snprintf(b, (size_t)nb, "%lld", (long long)a->v[0]);
    else if (a->t == 'b') snprintf(b, (size_t)nb, "%s", a->v[0] ? "True" : "False");
    else snprintf(b, (size_t)nb, "%.17g", a->v[0]);
}
void ft_tag_kind(const FtCall *b, char *tag, int ntag, const char **kind) {
    int i, npos = 0; const FtArg *pos[4]; const char *kw = NULL, *kd = "";
    for (i = 0; i < b->nargs; i++) { if (b->a[i].kw) { if (!strcmp(b->a[i].kw, "kind") && b->a[i].t == 's') kw = b->a[i].s; } else pos[npos++] = &b->a[i]; }
    int tool = !strcmp(b->name, "tool");
    if (tool && npos) { char s0[128]; arg_str(pos[0], s0, sizeof s0); snprintf(tag, (size_t)ntag, "%s:%s", b->name, s0); }
    else snprintf(tag, (size_t)ntag, "%s", b->name);
    if (kw && kw[0]) kd = kw;
    else if (npos && pos[0]->t == 's' && !tool && pos[0]->s[0]) kd = pos[0]->s;
    else if (tool && npos > 1) kd = pos[1]->t == 's' ? pos[1]->s : "";
    else if (tool) kd = "iron";
    *kind = so_intern(kd);
}
static int EXTENDED;
/* features.extend(details_mod) */
int ft_extend(void *ctx, int (*has)(void *, const char *), void (*add)(void *, const char *, const FtNewSpec *)) {
    int i;
    if (EXTENDED) return 0;
    EXTENDED = 1;
    for (i = 0; i < FT_NNEW; i++) if (has(ctx, FT_NEW[i].style)) add(ctx, FT_NEW[i].style, &FT_NEW[i]);
    return 1;
}

/* ---- generated by work\cscene\features\gen_tables.py from features._new / _landmarks and the style files' feature_tables ---- */
/* @@TABLES@@ */
/* features.NEW (style by style, as extend() fills it: _new() then the style files in name order) */
const FtNewSpec FT_NEW[] = {
    { "badlands", { 0, "wheel", 1, { { 0, 's', "wood", 0, { 0 }, 0 } } }, 10.5, 0.4, 0, 60, "wheel", "wood" },
    { "badlands", { 0, "skull", 2, { { 0, 's', "bone", 0, { 0 }, 0 }, { 0, 'f', 0, 1, { 1.2 }, 0 } } }, 10.5, 0.4, 0, 60, "skull", "bone" },
    { "badlands", { 0, "ribs", 1, { { 0, 's', "bone", 0, { 0 }, 0 } } }, 10.5, 0.4, 0, 60, "ribs", "bone" },
    { "badlands", { 0, "campfire", 2, { { 0, 's', "rock", 0, { 0 }, 0 }, { 0, 's', "wood", 0, { 0 }, 0 } } }, 10.5, 0.4, 0, 60, "campfire", "rock" },
    { "badlands", { 0, "tool", 1, { { 0, 's', "pick", 0, { 0 }, 0 } } }, 10.5, 0.4, 0, 60, "tool:pick", "iron" },
    { "badlands", { 0, "sign", 2, { { 0, 's', "wood", 0, { 0 }, 0 }, { 0, 's', "wood", 0, { 0 }, 0 } } }, 10.5, 0.4, 0, 60, "sign", "wood" },
    { "badlands", { 0, "crater", 2, { { 0, 's', "sandstone", 0, { 0 }, 0 }, { 0, 's', "pit", 0, { 0 }, 0 } } }, 10.5, 0.4, 0, 60, "crater", "sandstone" },
    { "badlands", { 0, "nest", 2, { { 0, 's', "shrub", 0, { 0 }, 0 }, { 0, 's', "bone", 0, { 0 }, 0 } } }, 10.5, 0.4, 0, 60, "nest", "shrub" },
    { "badlands", { 0, "roots", 2, { { 0, 's', "wood", 0, { 0 }, 0 }, { 0, 's', "wood", 0, { 0 }, 0 } } }, 10.5, 0.4, 0, 60, "roots", "wood" },
    { "badlands", { 0, "cracked_slab", 1, { { 0, 's', "sandstone", 0, { 0 }, 0 } } }, 10.5, 0.4, 0, 60, "cracked_slab", "sandstone" },
    { "highland", { 0, "campfire", 2, { { 0, 's', "slate", 0, { 0 }, 0 }, { 0, 's', "wood", 0, { 0 }, 0 } } }, 10.5, 0.4, 0, 60, "campfire", "slate" },
    { "highland", { 0, "well", 2, { { 0, 's', "slate", 0, { 0 }, 0 }, { 0, 's', "water", 0, { 0 }, 0 } } }, 10.5, 0.4, 0, 60, "well", "slate" },
    { "highland", { 0, "tool", 1, { { 0, 's', "sword", 0, { 0 }, 0 } } }, 10.5, 0.4, 0, 60, "tool:sword", "iron" },
    { "highland", { 0, "tool", 1, { { 0, 's', "axe", 0, { 0 }, 0 } } }, 10.5, 0.4, 0, 60, "tool:axe", "iron" },
    { "highland", { 0, "boss", 3, { { 0, 's', "wood", 0, { 0 }, 0 }, { 0, 's', "iron", 0, { 0 }, 0 }, { "cross", 'b', 0, 1, { 1 }, 0 } } }, 10.5, 0.4, 0, 60, "boss", "wood" },
    { "highland", { 0, "roots", 2, { { 0, 's', "wood", 0, { 0 }, 0 }, { 0, 's', "wood", 0, { 0 }, 0 } } }, 10.5, 0.4, 0, 60, "roots", "wood" },
    { "highland", { 0, "fairy_ring", 3, { { 0, 's', "flower", 0, { 0 }, 0 }, { "h", 'T', 0, 2, { 0.45, 0.6 }, 0 }, { "tiers", 'R', 0, 2, { 1.0, 30.0 }, 2 } } }, 10.5, 0.4, 0, 60, "fairy_ring", "flower" },
    { "highland", { 0, "rune_circle", 1, { { 0, 's', "glyph", 0, { 0 }, 0 } } }, 10.5, 0.4, 0, 60, "rune_circle", "glyph" },
    { "highland", { 0, "pile", 1, { { 0, 's', "wood", 0, { 0 }, 0 } } }, 10.5, 0.4, 0, 60, "pile", "wood" },
    { "highland", { 0, "stepping_pool", 2, { { 0, 's', "slate", 0, { 0 }, 0 }, { 0, 's', "water", 0, { 0 }, 0 } } }, 10.5, 0.4, 1, 60, "stepping_pool", "slate" },
    { "maahl", { 0, "crystals", 1, { { 0, 's', "crystal", 0, { 0 }, 0 } } }, 10.5, 0.4, 0, 60, "crystals", "crystal" },
    { "maahl", { 0, "fallen_column", 1, { { 0, 's', "column", 0, { 0 }, 0 } } }, 10.5, 0.4, 0, 60, "fallen_column", "column" },
    { "maahl", { 0, "rune_circle", 1, { { 0, 's', "glyph", 0, { 0 }, 0 } } }, 10.5, 0.4, 0, 60, "rune_circle", "glyph" },
    { "maahl", { 0, "stairs", 1, { { 0, 's', "ruin", 0, { 0 }, 0 } } }, 10.5, 0.4, 0, 60, "stairs", "ruin" },
    { "maahl", { 0, "skull", 2, { { 0, 's', "bone", 0, { 0 }, 0 }, { 0, 'f', 0, 1, { 1.3 }, 0 } } }, 10.5, 0.4, 0, 60, "skull", "bone" },
    { "maahl", { 0, "ribs", 1, { { 0, 's', "bone", 0, { 0 }, 0 } } }, 10.5, 0.4, 0, 60, "ribs", "bone" },
    { "maahl", { 0, "brazier", 2, { { 0, 's', "ruin", 0, { 0 }, 0 }, { 0, 's', "crystal", 0, { 0 }, 0 } } }, 10.5, 0.4, 0, 60, "brazier", "ruin" },
    { "maahl", { 0, "tiles", 1, { { 0, 's', "pavers", 0, { 0 }, 0 } } }, 10.5, 0.4, 0, 60, "tiles", "pavers" },
    { "maahl", { 0, "tool", 3, { { 0, 's', "spear", 0, { 0 }, 0 }, { 0, 's', "crystal", 0, { 0 }, 0 }, { 0, 's', "ruin", 0, { 0 }, 0 } } }, 10.5, 0.4, 0, 60, "tool:spear", "crystal" },
    { "maahl", { 0, "crater", 2, { { 0, 's', "rock", 0, { 0 }, 0 }, { 0, 's', "pit", 0, { 0 }, 0 } } }, 10.5, 0.4, 0, 60, "crater", "rock" },
    { "corridor", { 0, "gear", 1, { { 0, 's', "metal", 0, { 0 }, 0 } } }, 10.5, 0.4, 0, 60, "gear", "metal" },
    { "corridor", { 0, "tool", 2, { { 0, 's', "wrench", 0, { 0 }, 0 }, { 0, 's', "metal", 0, { 0 }, 0 } } }, 10.5, 0.4, 0, 60, "tool:wrench", "metal" },
    { "corridor", { 0, "grate", 2, { { 0, 's', "metal", 0, { 0 }, 0 }, { 0, 'b', 0, 1, { 1 }, 0 } } }, 10.5, 0.4, 0, 60, "grate", "metal" },
    { "corridor", { 0, "pile", 3, { { 0, 's', "pipe", 0, { 0 }, 0 }, { 0, 'T', 0, 2, { 3.6, 4.8 }, 0 }, { 0, 'f', 0, 1, { 0.9 }, 0 } } }, 10.5, 0.4, 0, 60, "pile", "pipe" },
    { "corridor", { 0, "hoops", 3, { { 0, 's', "metal", 0, { 0 }, 0 }, { 0, 'T', 0, 2, { 2.8, 3.4 }, 0 }, { 0, 'f', 0, 1, { 1.8 }, 0 } } }, 10.5, 0.4, 0, 60, "hoops", "metal" },
    { "corridor", { 0, "track", 3, { { 0, 's', "metal", 0, { 0 }, 0 }, { 0, 's', "metal", 0, { 0 }, 0 }, { 0, 'T', 0, 2, { 6.5, 9.0 }, 0 } } }, 10.5, 0.4, 0, 60, "track", "metal" },
    { "corridor", { 0, "boss", 3, { { 0, 's', "metal", 0, { 0 }, 0 }, { 0, 's', "metal", 0, { 0 }, 0 }, { "cross", 'b', 0, 1, { 1 }, 0 } } }, 10.5, 0.4, 0, 60, "boss", "metal" },
    { "corridor", { 0, "cart", 2, { { 0, 's', "crate", 0, { 0 }, 0 }, { 0, 's', "metal", 0, { 0 }, 0 } } }, 10.5, 0.4, 0, 60, "cart", "crate" },
    { "corridor", { 0, "tiles", 4, { { 0, 's', "plate", 0, { 0 }, 0 }, { 0, 'i', 0, 1, { 2 }, 1 }, { 0, 'i', 0, 1, { 2 }, 1 }, { 0, 'f', 0, 1, { 1.7 }, 0 } } }, 10.5, 0.4, 0, 60, "tiles", "plate" },
    { "corridor", { 0, "ladder", 1, { { 0, 's', "metal", 0, { 0 }, 0 } } }, 10.5, 0.4, 0, 60, "ladder", "metal" },
    { "infestation", { 0, "tentacle", 1, { { 0, 's', "tendril", 0, { 0 }, 0 } } }, 10.5, 0.4, 0, 60, "tentacle", "tendril" },
    { "infestation", { 0, "nest", 3, { { 0, 's', "flesh", 0, { 0 }, 0 }, { 0, 's', "egg", 0, { 0 }, 0 }, { 0, 'T', 0, 2, { 3.0, 5.0 }, 3 } } }, 10.5, 0.4, 0, 60, "nest", "flesh" },
    { "infestation", { 0, "skull", 2, { { 0, 's', "bone", 0, { 0 }, 0 }, { 0, 'f', 0, 1, { 1.1 }, 0 } } }, 10.5, 0.4, 0, 60, "skull", "bone" },
    { "infestation", { 0, "ribs", 1, { { 0, 's', "bone", 0, { 0 }, 0 } } }, 10.5, 0.4, 0, 60, "ribs", "bone" },
    { "infestation", { 0, "fairy_ring", 3, { { 0, 's', "pustule", 0, { 0 }, 0 }, { "h", 'T', 0, 2, { 0.6, 0.9 }, 0 }, { "tiers", 'R', 0, 4, { 0.55, 25.0, 0.45, 50.0 }, 10 } } }, 10.5, 0.4, 0, 60, "fairy_ring", "pustule" },
    { "infestation", { 0, "crater", 2, { { 0, 's', "flesh", 0, { 0 }, 0 }, { 0, 's', "acid", 0, { 0 }, 0 } } }, 10.5, 0.4, 0, 60, "crater", "flesh" },
    { "infestation", { 0, "tool", 3, { { 0, 's', "rifle", 0, { 0 }, 0 }, { 0, 's', "metal", 0, { 0 }, 0 }, { 0, 's', "metal", 0, { 0 }, 0 } } }, 10.5, 0.4, 0, 60, "tool:rifle", "metal" },
    { "infestation", { 0, "hoops", 3, { { 0, 's', "metal", 0, { 0 }, 0 }, { 0, 'T', 0, 2, { 2.8, 3.4 }, 0 }, { 0, 'f', 0, 1, { 1.8 }, 0 } } }, 10.5, 0.4, 0, 60, "hoops", "metal" },
    { "infestation", { 0, "crystals", 2, { { 0, 's', "spine", 0, { 0 }, 0 }, { 0, 'f', 0, 1, { 2.4 }, 0 } } }, 10.5, 0.4, 0, 60, "crystals", "spine" },
    { "infestation", { 0, "cracked_slab", 1, { { 0, 's', "carapace", 0, { 0 }, 0 } } }, 10.5, 0.4, 0, 60, "cracked_slab", "carapace" },
    { "urban", { 0, "boss", 4, { { 0, 's', "metal", 0, { 0 }, 0 }, { 0, 's', "metal", 0, { 0 }, 0 }, { 0, 'T', 0, 2, { 1.7, 2.1 }, 0 }, { 0, 'b', 0, 1, { 1 }, 0 } } }, 10.5, 0.4, 0, 60, "boss", "metal" },
    { "urban", { 0, "grate", 1, { { 0, 's', "iron", 0, { 0 }, 0 } } }, 10.5, 0.4, 0, 60, "grate", "iron" },
    { "urban", { 0, "sign", 2, { { 0, 's', "iron", 0, { 0 }, 0 }, { 0, 's', "metal", 0, { 0 }, 0 } } }, 10.5, 0.4, 0, 60, "sign", "iron" },
    { "urban", { 0, "tool", 2, { { 0, 's', "wrench", 0, { 0 }, 0 }, { 0, 's', "iron", 0, { 0 }, 0 } } }, 10.5, 0.4, 0, 60, "tool:wrench", "iron" },
    { "urban", { 0, "pile", 3, { { 0, 's', "concrete", 0, { 0 }, 0 }, { 0, 'T', 0, 2, { 3.6, 4.8 }, 0 }, { 0, 'f', 0, 1, { 0.95 }, 0 } } }, 10.5, 0.4, 0, 60, "pile", "concrete" },
    { "urban", { 0, "cracked_slab", 1, { { 0, 's', "concrete", 0, { 0 }, 0 } } }, 10.5, 0.4, 0, 60, "cracked_slab", "concrete" },
    { "urban", { 0, "wheel", 2, { { 0, 's', "rubble", 0, { 0 }, 0 }, { 0, 's', "metal", 0, { 0 }, 0 } } }, 10.5, 0.4, 0, 60, "wheel", "rubble" },
    { "urban", { 0, "tiles", 4, { { 0, 's', "concrete", 0, { 0 }, 0 }, { 0, 'i', 0, 1, { 3 }, 1 }, { 0, 'i', 0, 1, { 2 }, 1 }, { 0, 'f', 0, 1, { 1.4 }, 0 } } }, 10.5, 0.4, 0, 60, "tiles", "concrete" },
    { "urban", { 0, "hoops", 3, { { 0, 's', "metal", 0, { 0 }, 0 }, { 0, 'T', 0, 2, { 3.0, 3.6 }, 0 }, { 0, 'f', 0, 1, { 2.0 }, 0 } } }, 10.5, 0.4, 0, 60, "hoops", "metal" },
    { "urban", { 0, "crater", 2, { { 0, 's', "rubble", 0, { 0 }, 0 }, { 0, 's', "pothole", 0, { 0 }, 0 } } }, 10.5, 0.4, 0, 60, "crater", "rubble" },
    { "mine", { 0, "cart", 2, { { 0, 's', "wood", 0, { 0 }, 0 }, { 0, 's', "iron", 0, { 0 }, 0 } } }, 10.5, 0.4, 0, 60, "cart", "wood" },
    { "mine", { 0, "track", 2, { { 0, 's', "iron", 0, { 0 }, 0 }, { 0, 's', "wood", 0, { 0 }, 0 } } }, 10.5, 0.4, 0, 60, "track", "iron" },
    { "mine", { 0, "tool", 1, { { 0, 's', "pick", 0, { 0 }, 0 } } }, 10.5, 0.4, 0, 60, "tool:pick", "iron" },
    { "mine", { 0, "tool", 1, { { 0, 's', "shovel", 0, { 0 }, 0 } } }, 10.5, 0.4, 0, 60, "tool:shovel", "iron" },
    { "mine", { 0, "ladder", 1, { { 0, 's', "wood", 0, { 0 }, 0 } } }, 10.5, 0.4, 0, 60, "ladder", "wood" },
    { "mine", { 0, "pile", 3, { { 0, 's', "wood", 0, { 0 }, 0 }, { 0, 'T', 0, 2, { 4.2, 5.4 }, 0 }, { 0, 'f', 0, 1, { 1.0 }, 0 } } }, 10.5, 0.4, 0, 60, "pile", "wood" },
    { "mine", { 0, "crystals", 2, { { 0, 's', "quartz", 0, { 0 }, 0 }, { 0, 'f', 0, 1, { 2.4 }, 0 } } }, 10.5, 0.4, 0, 60, "crystals", "quartz" },
    { "mine", { 0, "wheel", 1, { { 0, 's', "wood", 0, { 0 }, 0 } } }, 10.5, 0.4, 0, 60, "wheel", "wood" },
    { "mine", { 0, "hoops", 3, { { 0, 's', "wood", 0, { 0 }, 0 }, { 0, 'T', 0, 2, { 3.0, 3.6 }, 0 }, { 0, 'f', 0, 1, { 2.0 }, 0 } } }, 10.5, 0.4, 0, 60, "hoops", "wood" },
    { "mine", { 0, "campfire", 2, { { 0, 's', "rock", 0, { 0 }, 0 }, { 0, 's', "wood", 0, { 0 }, 0 } } }, 10.5, 0.4, 0, 60, "campfire", "rock" },
    { "swamp", { 0, "boardwalk", 2, { { 0, 's', "log", 0, { 0 }, 0 }, { 0, 's', "log", 0, { 0 }, 0 } } }, 10.5, 0.4, 0, 60, "boardwalk", "log" },
    { "swamp", { 0, "stepping_pool", 2, { { 0, 's', "rock", 0, { 0 }, 0 }, { 0, 's', "water", 0, { 0 }, 0 } } }, 10.5, 0.4, 1, 60, "stepping_pool", "rock" },
    { "swamp", { 0, "roots", 2, { { 0, 's', "stump", 0, { 0 }, 0 }, { 0, 's', "root", 0, { 0 }, 0 } } }, 10.5, 0.4, 0, 60, "roots", "stump" },
    { "swamp", { 0, "tentacle", 1, { { 0, 's', "root", 0, { 0 }, 0 } } }, 10.5, 0.4, 0, 60, "tentacle", "root" },
    { "swamp", { 0, "skull", 2, { { 0, 's', "bone", 0, { 0 }, 0 }, { 0, 'f', 0, 1, { 1.2 }, 0 } } }, 10.5, 0.4, 0, 60, "skull", "bone" },
    { "swamp", { 0, "fairy_ring", 1, { { 0, 's', "shroom", 0, { 0 }, 0 } } }, 10.5, 0.4, 0, 60, "fairy_ring", "shroom" },
    { "swamp", { 0, "nest", 2, { { 0, 's', "reed", 0, { 0 }, 0 }, { 0, 's', "egg", 0, { 0 }, 0 } } }, 10.5, 0.4, 0, 60, "nest", "reed" },
    { "swamp", { 0, "tool", 3, { { 0, 's', "spear", 0, { 0 }, 0 }, { 0, 's', "iron", 0, { 0 }, 0 }, { 0, 's', "log", 0, { 0 }, 0 } } }, 10.5, 0.4, 0, 60, "tool:spear", "iron" },
    { "swamp", { 0, "fallen_column", 1, { { 0, 's', "tflag", 0, { 0 }, 0 } } }, 10.5, 0.4, 0, 60, "fallen_column", "tflag" },
    { "swamp", { 0, "ribs", 1, { { 0, 's', "bone", 0, { 0 }, 0 } } }, 10.5, 0.4, 0, 60, "ribs", "bone" },
    { "cynder", { 0, "crystals", 2, { { 0, 's', "obsidian", 0, { 0 }, 0 }, { 0, 'f', 0, 1, { 2.8 }, 0 } } }, 10.5, 0.4, 0, 60, "crystals", "obsidian" },
    { "cynder", { 0, "crater", 2, { { 0, 's', "basalt", 0, { 0 }, 0 }, { 0, 's', "lava", 0, { 0 }, 0 } } }, 10.5, 0.4, 0, 60, "crater", "basalt" },
    { "cynder", { 0, "brazier", 2, { { 0, 's', "basalt", 0, { 0 }, 0 }, { 0, 's', "fire", 0, { 0 }, 0 } } }, 10.5, 0.4, 0, 60, "brazier", "basalt" },
    { "cynder", { 0, "skull", 2, { { 0, 's', "bone", 0, { 0 }, 0 }, { 0, 'f', 0, 1, { 1.2 }, 0 } } }, 10.5, 0.4, 0, 60, "skull", "bone" },
    { "cynder", { 0, "ribs", 1, { { 0, 's', "bone", 0, { 0 }, 0 } } }, 10.5, 0.4, 0, 60, "ribs", "bone" },
    { "cynder", { 0, "tool", 1, { { 0, 's', "pick", 0, { 0 }, 0 } } }, 10.5, 0.4, 0, 60, "tool:pick", "iron" },
    { "cynder", { 0, "rune_circle", 2, { { 0, 's', "lava", 0, { 0 }, 0 }, { 0, 'T', 0, 2, { 2.6, 3.4 }, 0 } } }, 10.5, 0.4, 0, 60, "rune_circle", "lava" },
    { "cynder", { 0, "cracked_slab", 1, { { 0, 's', "basalt", 0, { 0 }, 0 } } }, 10.5, 0.4, 0, 60, "cracked_slab", "basalt" },
    { "cynder", { 0, "track", 3, { { 0, 's', "iron", 0, { 0 }, 0 }, { 0, 's', "basalt", 0, { 0 }, 0 }, { 0, 'T', 0, 2, { 6.5, 9.0 }, 0 } } }, 10.5, 0.4, 0, 60, "track", "iron" },
    { "cynder", { 0, "fairy_ring", 3, { { 0, 's', "basalt", 0, { 0 }, 0 }, { "h", 'T', 0, 2, { 0.7, 1.2 }, 0 }, { "tiers", 'R', 0, 4, { 0.6, 6.0, 0.4, 35.0 }, 10 } } }, 10.5, 0.4, 0, 60, "fairy_ring", "basalt" },
    { "targa", { 0, "crystals", 2, { { 0, 's', "ice", 0, { 0 }, 0 }, { 0, 'f', 0, 1, { 2.8 }, 0 } } }, 10.5, 0.4, 0, 60, "crystals", "ice" },
    { "targa", { 0, "gear", 1, { { 0, 's', "alien", 0, { 0 }, 0 } } }, 10.5, 0.4, 0, 60, "gear", "alien" },
    { "targa", { 0, "grate", 2, { { 0, 's', "alien", 0, { 0 }, 0 }, { 0, 'b', 0, 1, { 1 }, 0 } } }, 10.5, 0.4, 0, 60, "grate", "alien" },
    { "targa", { 0, "rune_circle", 1, { { 0, 's', "slot", 0, { 0 }, 0 } } }, 10.5, 0.4, 0, 60, "rune_circle", "slot" },
    { "targa", { 0, "fallen_column", 2, { { 0, 's', "alien", 0, { 0 }, 0 }, { 0, 'f', 0, 1, { 1.6 }, 0 } } }, 10.5, 0.4, 0, 60, "fallen_column", "alien" },
    { "targa", { 0, "boss", 3, { { 0, 's', "alien", 0, { 0 }, 0 }, { 0, 's', "crystal", 0, { 0 }, 0 }, { "cross", 'b', 0, 1, { 1 }, 0 } } }, 10.5, 0.4, 0, 60, "boss", "alien" },
    { "targa", { 0, "cracked_slab", 1, { { 0, 's', "ice", 0, { 0 }, 0 } } }, 10.5, 0.4, 0, 60, "cracked_slab", "ice" },
    { "targa", { 0, "stairs", 1, { { 0, 's', "alien", 0, { 0 }, 0 } } }, 10.5, 0.4, 0, 60, "stairs", "alien" },
    { "targa", { 0, "ribs", 1, { { 0, 's', "bone", 0, { 0 }, 0 } } }, 10.5, 0.4, 0, 60, "ribs", "bone" },
    { "targa", { 0, "crater", 2, { { 0, 's', "ice", 0, { 0 }, 0 }, { 0, 's', "pit", 0, { 0 }, 0 } } }, 10.5, 0.4, 0, 60, "crater", "ice" },
    { "trederra", { 0, "crater", 2, { { 0, 's', "mud", 0, { 0 }, 0 }, { 0, 's', "water", 0, { 0 }, 0 } } }, 10.5, 0.4, 0, 60, "crater", "mud" },
    { "trederra", { 0, "tool", 1, { { 0, 's', "rifle", 0, { 0 }, 0 } } }, 10.5, 0.4, 0, 60, "tool:rifle", "iron" },
    { "trederra", { 0, "tool", 1, { { 0, 's', "shovel", 0, { 0 }, 0 } } }, 10.5, 0.4, 0, 60, "tool:shovel", "iron" },
    { "trederra", { 0, "pile", 3, { { 0, 's', "iron", 0, { 0 }, 0 }, { 0, 'T', 0, 2, { 2.6, 3.4 }, 0 }, { 0, 'f', 0, 1, { 0.85 }, 0 } } }, 10.5, 0.4, 0, 60, "pile", "iron" },
    { "trederra", { 0, "boardwalk", 2, { { 0, 's', "duck", 0, { 0 }, 0 }, { 0, 's', "wood", 0, { 0 }, 0 } } }, 10.5, 0.4, 0, 60, "boardwalk", "duck" },
    { "trederra", { 0, "wheel", 1, { { 0, 's', "wood", 0, { 0 }, 0 } } }, 10.5, 0.4, 0, 60, "wheel", "wood" },
    { "trederra", { 0, "ladder", 1, { { 0, 's', "wood", 0, { 0 }, 0 } } }, 10.5, 0.4, 0, 60, "ladder", "wood" },
    { "trederra", { 0, "boss", 3, { { 0, 's', "iron", 0, { 0 }, 0 }, { 0, 's', "iron", 0, { 0 }, 0 }, { 0, 'T', 0, 2, { 1.2, 1.5 }, 0 } } }, 10.5, 0.4, 0, 60, "boss", "iron" },
    { "trederra", { 0, "cart", 2, { { 0, 's', "wood", 0, { 0 }, 0 }, { 0, 's', "iron", 0, { 0 }, 0 } } }, 10.5, 0.4, 0, 60, "cart", "wood" },
    { "trederra", { 0, "sign", 2, { { 0, 's', "wood", 0, { 0 }, 0 }, { 0, 's', "wood", 0, { 0 }, 0 } } }, 10.5, 0.4, 0, 60, "sign", "wood" },
    { "fortress", { 0, "tool", 1, { { 0, 's', "sword", 0, { 0 }, 0 } } }, 10.5, 0.4, 0, 60, "tool:sword", "iron" },
    { "fortress", { 0, "tool", 1, { { 0, 's', "spear", 0, { 0 }, 0 } } }, 10.5, 0.4, 0, 60, "tool:spear", "iron" },
    { "fortress", { 0, "stairs", 1, { { 0, 's', "plank", 0, { 0 }, 0 } } }, 10.5, 0.4, 0, 60, "stairs", "plank" },
    { "fortress", { 0, "well", 2, { { 0, 's', "garden", 0, { 0 }, 0 }, { 0, 's', "water", 0, { 0 }, 0 } } }, 10.5, 0.4, 0, 60, "well", "garden" },
    { "fortress", { 0, "brazier", 2, { { 0, 's', "iron", 0, { 0 }, 0 }, { 0, 's', "fire", 0, { 0 }, 0 } } }, 10.5, 0.4, 0, 60, "brazier", "iron" },
    { "fortress", { 0, "stepping_pool", 2, { { 0, 's', "garden", 0, { 0 }, 0 }, { 0, 's', "water", 0, { 0 }, 0 } } }, 10.5, 0.4, 1, 60, "stepping_pool", "garden" },
    { "fortress", { 0, "boss", 3, { { 0, 's', "wood", 0, { 0 }, 0 }, { 0, 's', "iron", 0, { 0 }, 0 }, { "cross", 'b', 0, 1, { 1 }, 0 } } }, 10.5, 0.4, 0, 60, "boss", "wood" },
    { "fortress", { 0, "tiles", 4, { { 0, 's', "tatami", 0, { 0 }, 0 }, { 0, 'i', 0, 1, { 2 }, 1 }, { 0, 'i', 0, 1, { 2 }, 1 }, { 0, 'f', 0, 1, { 1.8 }, 0 } } }, 10.5, 0.4, 0, 60, "tiles", "tatami" },
    { "fortress", { 0, "chest", 1, { { 0, 's', "crate", 0, { 0 }, 0 } } }, 10.5, 0.4, 0, 60, "chest", "crate" },
    { "fortress", { 0, "hoops", 3, { { 0, 's', "wood", 0, { 0 }, 0 }, { 0, 'T', 0, 2, { 3.0, 3.6 }, 0 }, { 0, 'f', 0, 1, { 2.0 }, 0 } } }, 10.5, 0.4, 0, 60, "hoops", "wood" },
    { "beast", { 0, "tentacle", 1, { { 0, 's', "vein", 0, { 0 }, 0 } } }, 10.5, 0.4, 0, 60, "tentacle", "vein" },
    { "beast", { 0, "ribs", 2, { { 0, 's', "bone", 0, { 0 }, 0 }, { 0, 'T', 0, 2, { 6.0, 8.5 }, 0 } } }, 10.5, 0.4, 0, 60, "ribs", "bone" },
    { "beast", { 0, "skull", 2, { { 0, 's', "bone", 0, { 0 }, 0 }, { 0, 'f', 0, 1, { 1.3 }, 0 } } }, 10.5, 0.4, 0, 60, "skull", "bone" },
    { "beast", { 0, "nest", 3, { { 0, 's', "flesh", 0, { 0 }, 0 }, { 0, 's', "egg", 0, { 0 }, 0 }, { 0, 'T', 0, 2, { 3.0, 5.0 }, 3 } } }, 10.5, 0.4, 0, 60, "nest", "flesh" },
    { "beast", { 0, "fairy_ring", 3, { { 0, 's', "pustule", 0, { 0 }, 0 }, { "h", 'T', 0, 2, { 0.6, 0.9 }, 0 }, { "tiers", 'R', 0, 4, { 0.55, 25.0, 0.45, 50.0 }, 10 } } }, 10.5, 0.4, 0, 60, "fairy_ring", "pustule" },
    { "beast", { 0, "crater", 2, { { 0, 's', "flesh", 0, { 0 }, 0 }, { 0, 's', "bile", 0, { 0 }, 0 } } }, 10.5, 0.4, 0, 60, "crater", "flesh" },
    { "beast", { 0, "tool", 3, { { 0, 's', "spear", 0, { 0 }, 0 }, { 0, 's', "bone", 0, { 0 }, 0 }, { 0, 's', "bone", 0, { 0 }, 0 } } }, 10.5, 0.4, 0, 60, "tool:spear", "bone" },
    { "beast", { 0, "crystals", 2, { { 0, 's', "tooth", 0, { 0 }, 0 }, { 0, 'f', 0, 1, { 2.4 }, 0 } } }, 10.5, 0.4, 0, 60, "crystals", "tooth" },
    { "beast", { 0, "tool", 1, { { 0, 's', "anchor", 0, { 0 }, 0 } } }, 10.5, 0.4, 0, 60, "tool:anchor", "iron" },
    { "beast", { 0, "chest", 1, { { 0, 's', "crate", 0, { 0 }, 0 } } }, 10.5, 0.4, 0, 60, "chest", "crate" },
    { "frontier", { 0, "wheel", 1, { { 0, 's', "wood", 0, { 0 }, 0 } } }, 10.5, 0.4, 0, 60, "wheel", "wood" },
    { "frontier", { 0, "hoops", 1, { { 0, 's', "barrel", 0, { 0 }, 0 } } }, 10.5, 0.4, 0, 60, "hoops", "barrel" },
    { "frontier", { 0, "well", 2, { { 0, 's', "rock", 0, { 0 }, 0 }, { 0, 's', "water", 0, { 0 }, 0 } } }, 10.5, 0.4, 0, 60, "well", "rock" },
    { "frontier", { 0, "sign", 2, { { 0, 's', "post", 0, { 0 }, 0 }, { 0, 's', "board", 0, { 0 }, 0 } } }, 10.5, 0.4, 0, 60, "sign", "post" },
    { "frontier", { 0, "campfire", 2, { { 0, 's', "rock", 0, { 0 }, 0 }, { 0, 's', "wood", 0, { 0 }, 0 } } }, 10.5, 0.4, 0, 60, "campfire", "rock" },
    { "frontier", { 0, "tool", 1, { { 0, 's', "rifle", 0, { 0 }, 0 } } }, 10.5, 0.4, 0, 60, "tool:rifle", "iron" },
    { "frontier", { 0, "boardwalk", 2, { { 0, 's', "board", 0, { 0 }, 0 }, { 0, 's', "post", 0, { 0 }, 0 } } }, 10.5, 0.4, 0, 60, "boardwalk", "board" },
    { "frontier", { 0, "cart", 2, { { 0, 's', "board", 0, { 0 }, 0 }, { 0, 's', "iron", 0, { 0 }, 0 } } }, 10.5, 0.4, 0, 60, "cart", "board" },
    { "frontier", { 0, "ladder", 1, { { 0, 's', "post", 0, { 0 }, 0 } } }, 10.5, 0.4, 0, 60, "ladder", "post" },
    { "frontier", { 0, "chest", 1, { { 0, 's', "crate", 0, { 0 }, 0 } } }, 10.5, 0.4, 0, 60, "chest", "crate" },
    { "graveyard", { 0, "skull", 2, { { 0, 's', "bone", 0, { 0 }, 0 }, { 0, 'f', 0, 1, { 1.2 }, 0 } } }, 10.5, 0.4, 0, 60, "skull", "bone" },
    { "graveyard", { 0, "ribs", 1, { { 0, 's', "bone", 0, { 0 }, 0 } } }, 10.5, 0.4, 0, 60, "ribs", "bone" },
    { "graveyard", { 0, "tool", 1, { { 0, 's', "shovel", 0, { 0 }, 0 } } }, 10.5, 0.4, 0, 60, "tool:shovel", "iron" },
    { "graveyard", { 0, "cracked_slab", 1, { { 0, 's', "headstone", 0, { 0 }, 0 } } }, 10.5, 0.4, 0, 60, "cracked_slab", "headstone" },
    { "graveyard", { 0, "fallen_column", 2, { { 0, 's', "headstone", 0, { 0 }, 0 }, { 0, 'f', 0, 1, { 1.5 }, 0 } } }, 10.5, 0.4, 0, 60, "fallen_column", "headstone" },
    { "graveyard", { 0, "well", 2, { { 0, 's', "headstone", 0, { 0 }, 0 }, { 0, 's', "pit", 0, { 0 }, 0 } } }, 10.5, 0.4, 0, 60, "well", "headstone" },
    { "graveyard", { 0, "roots", 2, { { 0, 's', "earth", 0, { 0 }, 0 }, { 0, 's', "earth", 0, { 0 }, 0 } } }, 10.5, 0.4, 0, 60, "roots", "earth" },
    { "graveyard", { 0, "fairy_ring", 3, { { 0, 's', "bone", 0, { 0 }, 0 }, { "h", 'T', 0, 2, { 0.4, 0.55 }, 0 }, { "tiers", 'R', 0, 4, { 0.7, 6.0, 0.3, 40.0 }, 10 } } }, 10.5, 0.4, 0, 60, "fairy_ring", "bone" },
    { "graveyard", { 0, "brazier", 2, { { 0, 's', "iron", 0, { 0 }, 0 }, { 0, 's', "fire", 0, { 0 }, 0 } } }, 10.5, 0.4, 0, 60, "brazier", "iron" },
    { "graveyard", { 0, "tiles", 4, { { 0, 's', "headstone", 0, { 0 }, 0 }, { 0, 'i', 0, 1, { 2 }, 1 }, { 0, 'i', 0, 1, { 2 }, 1 }, { 0, 'f', 0, 1, { 1.6 }, 0 } } }, 10.5, 0.4, 0, 60, "tiles", "headstone" },
    { "forest", { 0, "roots", 2, { { 0, 's', "stump", 0, { 0 }, 0 }, { 0, 's', "root", 0, { 0 }, 0 } } }, 10.5, 0.4, 0, 60, "roots", "stump" },
    { "forest", { 0, "fairy_ring", 1, { { 0, 's', "shroom", 0, { 0 }, 0 } } }, 10.5, 0.4, 0, 60, "fairy_ring", "shroom" },
    { "forest", { 0, "pile", 1, { { 0, 's', "log", 0, { 0 }, 0 } } }, 10.5, 0.4, 0, 60, "pile", "log" },
    { "forest", { 0, "campfire", 2, { { 0, 's', "rock", 0, { 0 }, 0 }, { 0, 's', "log", 0, { 0 }, 0 } } }, 10.5, 0.4, 0, 60, "campfire", "rock" },
    { "forest", { 0, "tool", 1, { { 0, 's', "axe", 0, { 0 }, 0 } } }, 10.5, 0.4, 0, 60, "tool:axe", "iron" },
    { "forest", { 0, "nest", 2, { { 0, 's', "leaf", 0, { 0 }, 0 }, { 0, 's', "shroom", 0, { 0 }, 0 } } }, 10.5, 0.4, 0, 60, "nest", "leaf" },
    { "forest", { 0, "stepping_pool", 2, { { 0, 's', "rock", 0, { 0 }, 0 }, { 0, 's', "water", 0, { 0 }, 0 } } }, 10.5, 0.4, 1, 60, "stepping_pool", "rock" },
    { "forest", { 0, "skull", 2, { { 0, 's', "bone", 0, { 0 }, 0 }, { 0, 'f', 0, 1, { 1.0 }, 0 } } }, 10.5, 0.4, 0, 60, "skull", "bone" },
    { "forest", { 0, "boardwalk", 2, { { 0, 's', "log", 0, { 0 }, 0 }, { 0, 's', "log", 0, { 0 }, 0 } } }, 10.5, 0.4, 0, 60, "boardwalk", "log" },
    { "forest", { 0, "well", 2, { { 0, 's', "rock", 0, { 0 }, 0 }, { 0, 's', "water", 0, { 0 }, 0 } } }, 10.5, 0.4, 0, 60, "well", "rock" },
    { "dungeon", { 0, "chest", 1, { { 0, 's', "crate", 0, { 0 }, 0 } } }, 10.5, 0.4, 0, 60, "chest", "crate" },
    { "dungeon", { 0, "tool", 1, { { 0, 's', "sword", 0, { 0 }, 0 } } }, 10.5, 0.4, 0, 60, "tool:sword", "iron" },
    { "dungeon", { 0, "skull", 2, { { 0, 's', "bone", 0, { 0 }, 0 }, { 0, 'f', 0, 1, { 1.1 }, 0 } } }, 10.5, 0.4, 0, 60, "skull", "bone" },
    { "dungeon", { 0, "grate", 1, { { 0, 's', "iron", 0, { 0 }, 0 } } }, 10.5, 0.4, 0, 60, "grate", "iron" },
    { "dungeon", { 0, "stairs", 1, { { 0, 's', "flag", 0, { 0 }, 0 } } }, 10.5, 0.4, 0, 60, "stairs", "flag" },
    { "dungeon", { 0, "brazier", 2, { { 0, 's', "iron", 0, { 0 }, 0 }, { 0, 's', "fire", 0, { 0 }, 0 } } }, 10.5, 0.4, 0, 60, "brazier", "iron" },
    { "dungeon", { 0, "rune_circle", 1, { { 0, 's', "glyph", 0, { 0 }, 0 } } }, 10.5, 0.4, 0, 60, "rune_circle", "glyph" },
    { "dungeon", { 0, "hoops", 1, { { 0, 's', "crate", 0, { 0 }, 0 } } }, 10.5, 0.4, 0, 60, "hoops", "crate" },
    { "dungeon", { 0, "boss", 3, { { 0, 's', "iron", 0, { 0 }, 0 }, { 0, 's', "iron", 0, { 0 }, 0 }, { "cross", 'b', 0, 1, { 1 }, 0 } } }, 10.5, 0.4, 0, 60, "boss", "iron" },
    { "dungeon", { 0, "ribs", 1, { { 0, 's', "bone", 0, { 0 }, 0 } } }, 10.5, 0.4, 0, 60, "ribs", "bone" },
    { "temple", { 0, "fallen_column", 1, { { 0, 's', "tcolumn", 0, { 0 }, 0 } } }, 10.5, 0.4, 0, 60, "fallen_column", "tcolumn" },
    { "temple", { 0, "stairs", 1, { { 0, 's', "tflag", 0, { 0 }, 0 } } }, 10.5, 0.4, 0, 60, "stairs", "tflag" },
    { "temple", { 0, "brazier", 2, { { 0, 's', "tcolumn", 0, { 0 }, 0 }, { 0, 's', "gold", 0, { 0 }, 0 } } }, 10.5, 0.4, 0, 60, "brazier", "tcolumn" },
    { "temple", { 0, "rune_circle", 1, { { 0, 's', "glyph", 0, { 0 }, 0 } } }, 10.5, 0.4, 0, 60, "rune_circle", "glyph" },
    { "temple", { 0, "tiles", 4, { { 0, 's', "tflag", 0, { 0 }, 0 }, { 0, 'i', 0, 1, { 3 }, 1 }, { 0, 'i', 0, 1, { 2 }, 1 }, { 0, 'f', 0, 1, { 1.6 }, 0 } } }, 10.5, 0.4, 0, 60, "tiles", "tflag" },
    { "temple", { 0, "boss", 3, { { 0, 's', "gold", 0, { 0 }, 0 }, { 0, 's', "gold", 0, { 0 }, 0 }, { "cross", 'b', 0, 1, { 1 }, 0 } } }, 10.5, 0.4, 0, 60, "boss", "gold" },
    { "temple", { 0, "well", 2, { { 0, 's', "tcolumn", 0, { 0 }, 0 }, { 0, 's', "water", 0, { 0 }, 0 } } }, 10.5, 0.4, 0, 60, "well", "tcolumn" },
    { "temple", { 0, "cracked_slab", 1, { { 0, 's', "tflag", 0, { 0 }, 0 } } }, 10.5, 0.4, 0, 60, "cracked_slab", "tflag" },
    { "temple", { 0, "tool", 3, { { 0, 's', "spear", 0, { 0 }, 0 }, { 0, 's', "gold", 0, { 0 }, 0 }, { 0, 's', "tcolumn", 0, { 0 }, 0 } } }, 10.5, 0.4, 0, 60, "tool:spear", "gold" },
    { "temple", { 0, "chest", 2, { { 0, 's', "tcolumn", 0, { 0 }, 0 }, { 0, 's', "gold", 0, { 0 }, 0 } } }, 10.5, 0.4, 0, 60, "chest", "tcolumn" },
    { "coast", { 0, "tool", 1, { { 0, 's', "anchor", 0, { 0 }, 0 } } }, 10.5, 0.4, 0, 60, "tool:anchor", "iron" },
    { "coast", { 0, "boardwalk", 2, { { 0, 's', "drift", 0, { 0 }, 0 }, { 0, 's', "drift", 0, { 0 }, 0 } } }, 10.5, 0.4, 0, 60, "boardwalk", "drift" },
    { "coast", { 0, "hoops", 1, { { 0, 's', "drift", 0, { 0 }, 0 } } }, 10.5, 0.4, 0, 60, "hoops", "drift" },
    { "coast", { 0, "chest", 1, { { 0, 's', "drift", 0, { 0 }, 0 } } }, 10.5, 0.4, 0, 60, "chest", "drift" },
    { "coast", { 0, "ribs", 1, { { 0, 's', "bone", 0, { 0 }, 0 } } }, 10.5, 0.4, 0, 60, "ribs", "bone" },
    { "coast", { 0, "stepping_pool", 2, { { 0, 's', "shelf", 0, { 0 }, 0 }, { 0, 's', "water", 0, { 0 }, 0 } } }, 10.5, 0.4, 1, 60, "stepping_pool", "shelf" },
    { "coast", { 0, "nest", 2, { { 0, 's', "weed", 0, { 0 }, 0 }, { 0, 's', "shell", 0, { 0 }, 0 } } }, 10.5, 0.4, 0, 60, "nest", "weed" },
    { "coast", { 0, "tentacle", 1, { { 0, 's', "weed", 0, { 0 }, 0 } } }, 10.5, 0.4, 0, 60, "tentacle", "weed" },
    { "coast", { 0, "wheel", 1, { { 0, 's', "drift", 0, { 0 }, 0 } } }, 10.5, 0.4, 0, 60, "wheel", "drift" },
    { "coast", { 0, "pile", 1, { { 0, 's', "drift", 0, { 0 }, 0 } } }, 10.5, 0.4, 0, 60, "pile", "drift" },
    { "ashveil", { 0, "wheel", 2, { { 0, 's', "ashscrap", 0, { 0 }, 0 }, { 0, 's', "rust", 0, { 0 }, 0 } } }, 10.5, 0.4, 0, 60, "wheel", "ashscrap" },
    { "ashveil", { 0, "skull", 2, { { 0, 's', "bone", 0, { 0 }, 0 }, { 0, 'f', 0, 1, { 1.2 }, 0 } } }, 10.5, 0.4, 0, 60, "skull", "bone" },
    { "ashveil", { 0, "ribs", 1, { { 0, 's', "bone", 0, { 0 }, 0 } } }, 10.5, 0.4, 0, 60, "ribs", "bone" },
    { "ashveil", { 0, "tool", 3, { { 0, 's', "wrench", 0, { 0 }, 0 }, { 0, 's', "rust", 0, { 0 }, 0 }, { 0, 's', "ashscrap", 0, { 0 }, 0 } } }, 10.5, 0.4, 0, 60, "tool:wrench", "rust" },
    { "ashveil", { 0, "pile", 3, { { 0, 's', "ashpipe", 0, { 0 }, 0 }, { 0, 'T', 0, 2, { 3.6, 4.8 }, 0 }, { 0, 'f', 0, 1, { 0.9 }, 0 } } }, 10.5, 0.4, 0, 60, "pile", "ashpipe" },
    { "ashveil", { 0, "cracked_slab", 1, { { 0, 's', "ashstone", 0, { 0 }, 0 } } }, 10.5, 0.4, 0, 60, "cracked_slab", "ashstone" },
    { "ashveil", { 0, "crater", 2, { { 0, 's', "ashcinder", 0, { 0 }, 0 }, { 0, 's', "pit", 0, { 0 }, 0 } } }, 10.5, 0.4, 0, 60, "crater", "ashcinder" },
    { "ashveil", { 0, "gear", 1, { { 0, 's', "rust", 0, { 0 }, 0 } } }, 10.5, 0.4, 0, 60, "gear", "rust" },
    { "ashveil", { 0, "hoops", 3, { { 0, 's', "rust", 0, { 0 }, 0 }, { 0, 'T', 0, 2, { 2.8, 3.4 }, 0 }, { 0, 'f', 0, 1, { 1.8 }, 0 } } }, 10.5, 0.4, 0, 60, "hoops", "rust" },
    { "ashveil", { 0, "sign", 2, { { 0, 's', "rust", 0, { 0 }, 0 }, { 0, 's', "ashscrap", 0, { 0 }, 0 } } }, 10.5, 0.4, 0, 60, "sign", "rust" },
    { "blight", { "style_blight", "thorn_cluster", 1, { { 0, 's', "thorn", 0, { 0 }, 0 } } }, 10.5, 0.4, 0, 60, "", "" },
    { "blight", { "style_blight", "bone_heap", 1, { { 0, 's', "bone", 0, { 0 }, 0 } } }, 10.5, 0.4, 0, 60, "", "" },
    { "blight", { "style_blight", "boiling_pool", 2, { { 0, 's', "ichor", 0, { 0 }, 0 }, { 0, 's', "blister", 0, { 0 }, 0 } } }, 10.5, 0.4, 1, 60, "", "" },
    { "blight", { "style_blight", "rune_ring", 2, { { 0, 's', "runecut", 0, { 0 }, 0 }, { 0, 's', "rift", 0, { 0 }, 0 } } }, 10.5, 0.4, 1, 60, "", "" },
    { "blight", { "style_blight", "torn_rift", 2, { { 0, 's', "rift", 0, { 0 }, 0 }, { 0, 's', "blightrock", 0, { 0 }, 0 } } }, 10.5, 0.4, 1, 60, "", "" },
    { "blight", { "style_blight", "flesh_rock", 2, { { 0, 's', "fleshrock", 0, { 0 }, 0 }, { 0, 's', "blister", 0, { 0 }, 0 } } }, 10.5, 0.4, 0, 60, "", "" },
    { "blight", { 0, "skull", 2, { { 0, 's', "bone", 0, { 0 }, 0 }, { 0, 'f', 0, 1, { 1.2 }, 0 } } }, 10.5, 0.4, 0, 60, "skull", "bone" },
    { "blight", { 0, "ribs", 1, { { 0, 's', "bone", 0, { 0 }, 0 } } }, 10.5, 0.4, 0, 60, "ribs", "bone" },
    { "blight", { 0, "crater", 2, { { 0, 's', "blightrock", 0, { 0 }, 0 }, { 0, 's', "rift", 0, { 0 }, 0 } } }, 10.5, 0.4, 0, 60, "crater", "blightrock" },
    { "blight", { 0, "tentacle", 1, { { 0, 's', "tendril", 0, { 0 }, 0 } } }, 10.5, 0.4, 0, 60, "tentacle", "tendril" },
    { "blight", { 0, "brazier", 2, { { 0, 's', "blightrock", 0, { 0 }, 0 }, { 0, 's', "fire", 0, { 0 }, 0 } } }, 10.5, 0.4, 0, 60, "brazier", "blightrock" },
    { "blight", { 0, "cracked_slab", 1, { { 0, 's', "runestone", 0, { 0 }, 0 } } }, 10.5, 0.4, 0, 60, "cracked_slab", "runestone" },
    { "ferrum", { 0, "gear", 1, { { 0, 's', "iron", 0, { 0 }, 0 } } }, 10.5, 0.4, 0, 60, "gear", "iron" },
    { "ferrum", { 0, "gear", 2, { { 0, 's', "rust", 0, { 0 }, 0 }, { 0, 'T', 0, 2, { 1.3, 1.8 }, 0 } } }, 10.5, 0.4, 0, 60, "gear", "rust" },
    { "ferrum", { 0, "grate", 2, { { 0, 's', "iron", 0, { 0 }, 0 }, { 0, 'b', 0, 1, { 1 }, 0 } } }, 10.5, 0.4, 0, 60, "grate", "iron" },
    { "ferrum", { 0, "pile", 3, { { 0, 's', "pipe", 0, { 0 }, 0 }, { 0, 'T', 0, 2, { 3.6, 4.8 }, 0 }, { 0, 'f', 0, 1, { 0.9 }, 0 } } }, 10.5, 0.4, 0, 60, "pile", "pipe" },
    { "ferrum", { 0, "hoops", 3, { { 0, 's', "rust", 0, { 0 }, 0 }, { 0, 'T', 0, 2, { 2.8, 3.4 }, 0 }, { 0, 'f', 0, 1, { 1.8 }, 0 } } }, 10.5, 0.4, 0, 60, "hoops", "rust" },
    { "ferrum", { 0, "track", 3, { { 0, 's', "iron", 0, { 0 }, 0 }, { 0, 's', "rust", 0, { 0 }, 0 }, { 0, 'T', 0, 2, { 6.5, 9.0 }, 0 } } }, 10.5, 0.4, 0, 60, "track", "iron" },
    { "ferrum", { 0, "boss", 3, { { 0, 's', "iron", 0, { 0 }, 0 }, { 0, 's', "iron", 0, { 0 }, 0 }, { "cross", 'b', 0, 1, { 1 }, 0 } } }, 10.5, 0.4, 0, 60, "boss", "iron" },
    { "ferrum", { 0, "cart", 2, { { 0, 's', "rust", 0, { 0 }, 0 }, { 0, 's', "iron", 0, { 0 }, 0 } } }, 10.5, 0.4, 0, 60, "cart", "rust" },
    { "ferrum", { 0, "tiles", 4, { { 0, 's', "plate", 0, { 0 }, 0 }, { 0, 'i', 0, 1, { 2 }, 1 }, { 0, 'i', 0, 1, { 2 }, 1 }, { 0, 'f', 0, 1, { 1.7 }, 0 } } }, 10.5, 0.4, 0, 60, "tiles", "plate" },
    { "ferrum", { 0, "ladder", 1, { { 0, 's', "rust", 0, { 0 }, 0 } } }, 10.5, 0.4, 0, 60, "ladder", "rust" },
    { "ferrum", { 0, "tool", 2, { { 0, 's', "wrench", 0, { 0 }, 0 }, { 0, 's', "iron", 0, { 0 }, 0 } } }, 10.5, 0.4, 0, 60, "tool:wrench", "iron" },
    { "ferrum", { "style_ferrum", "sunk_gear", 2, { { 0, 's', "rust", 0, { 0 }, 0 }, { 0, 'T', 0, 2, { 2.2, 3.0 }, 0 } } }, 10.5, 0.4, 0, 60, "sunk_gear", "rust" },
    { "obelis", { "style_obelis", "ob_slab", 0, { { 0 } } }, 10.5, 0.4, 0, 60, "ob_slab", "monolith" },
    { "obelis", { "style_obelis", "ob_casket", 0, { { 0 } } }, 10.5, 0.4, 0, 60, "ob_casket", "casket" },
    { "obelis", { "style_obelis", "ob_pylon", 0, { { 0 } } }, 10.5, 0.4, 0, 60, "ob_pylon", "monolith" },
    { "obelis", { "style_obelis", "ob_panel", 0, { { 0 } } }, 10.5, 0.4, 0, 60, "ob_panel", "obmetal" },
    { "obelis", { 0, "stairs", 1, { { 0, 's', "obfloor", 0, { 0 }, 0 } } }, 10.5, 0.4, 0, 60, "stairs", "obfloor" },
    { "obelis", { 0, "grate", 1, { { 0, 's', "obmetal", 0, { 0 }, 0 } } }, 10.5, 0.4, 0, 60, "grate", "obmetal" },
    { "obelis", { 0, "chest", 2, { { 0, 's', "casket", 0, { 0 }, 0 }, { 0, 's', "obmetal", 0, { 0 }, 0 } } }, 10.5, 0.4, 0, 60, "chest", "casket" },
    { "obelis", { 0, "tiles", 4, { { 0, 's', "obfloor", 0, { 0 }, 0 }, { 0, 'i', 0, 1, { 3 }, 1 }, { 0, 'i', 0, 1, { 2 }, 1 }, { 0, 'f', 0, 1, { 1.6 }, 0 } } }, 10.5, 0.4, 0, 60, "tiles", "obfloor" },
    { "obelis", { 0, "cracked_slab", 1, { { 0, 's', "monolith", 0, { 0 }, 0 } } }, 10.5, 0.4, 0, 60, "cracked_slab", "monolith" },
    { "obelis", { 0, "crystals", 2, { { 0, 's', "obglow", 0, { 0 }, 0 }, { 0, 'f', 0, 1, { 2.4 }, 0 } } }, 10.5, 0.4, 0, 60, "crystals", "obglow" },
    { "sundered", { 0, "fallen_column", 1, { { 0, 's', "ashlar", 0, { 0 }, 0 } } }, 10.5, 0.4, 0, 60, "fallen_column", "ashlar" },
    { "sundered", { 0, "cracked_slab", 1, { { 0, 's', "ashlar", 0, { 0 }, 0 } } }, 10.5, 0.4, 0, 60, "cracked_slab", "ashlar" },
    { "sundered", { 0, "tiles", 4, { { 0, 's', "flag", 0, { 0 }, 0 }, { 0, 'i', 0, 1, { 3 }, 1 }, { 0, 'i', 0, 1, { 2 }, 1 }, { 0, 'f', 0, 1, { 1.5 }, 0 } } }, 10.5, 0.4, 0, 60, "tiles", "flag" },
    { "sundered", { 0, "stairs", 1, { { 0, 's', "ashlar", 0, { 0 }, 0 } } }, 10.5, 0.4, 0, 60, "stairs", "ashlar" },
    { "sundered", { 0, "crater", 2, { { 0, 's', "rubble", 0, { 0 }, 0 }, { 0, 's', "pothole", 0, { 0 }, 0 } } }, 10.5, 0.4, 0, 60, "crater", "rubble" },
    { "sundered", { 0, "grate", 1, { { 0, 's', "iron", 0, { 0 }, 0 } } }, 10.5, 0.4, 0, 60, "grate", "iron" },
    { "sundered", { 0, "tool", 1, { { 0, 's', "rifle", 0, { 0 }, 0 } } }, 10.5, 0.4, 0, 60, "tool:rifle", "iron" },
    { "sundered", { 0, "pile", 3, { { 0, 's', "wood", 0, { 0 }, 0 }, { 0, 'T', 0, 2, { 4.2, 5.4 }, 0 }, { 0, 'f', 0, 1, { 1.0 }, 0 } } }, 10.5, 0.4, 0, 60, "pile", "wood" },
    { "sundered", { 0, "boss", 3, { { 0, 's', "iron", 0, { 0 }, 0 }, { 0, 's', "iron", 0, { 0 }, 0 }, { "cross", 'b', 0, 1, { 1 }, 0 } } }, 10.5, 0.4, 0, 60, "boss", "iron" },
    { "sundered", { 0, "brazier", 2, { { 0, 's', "iron", 0, { 0 }, 0 }, { 0, 's', "fire", 0, { 0 }, 0 } } }, 10.5, 0.4, 0, 60, "brazier", "iron" },
    { "thornwild", { 0, "roots", 2, { { 0, 's', "stump", 0, { 0 }, 0 }, { 0, 's', "root", 0, { 0 }, 0 } } }, 10.5, 0.4, 0, 60, "roots", "stump" },
    { "thornwild", { 0, "fairy_ring", 2, { { 0, 's', "glowcap", 0, { 0 }, 0 }, { "h", 'T', 0, 2, { 0.6, 0.95 }, 0 } } }, 10.5, 0.4, 0, 60, "fairy_ring", "glowcap" },
    { "thornwild", { 0, "tentacle", 1, { { 0, 's', "creeper", 0, { 0 }, 0 } } }, 10.5, 0.4, 0, 60, "tentacle", "creeper" },
    { "thornwild", { 0, "nest", 2, { { 0, 's', "root", 0, { 0 }, 0 }, { 0, 's', "sporepod", 0, { 0 }, 0 } } }, 10.5, 0.4, 0, 60, "nest", "root" },
    { "thornwild", { 0, "skull", 2, { { 0, 's', "bone", 0, { 0 }, 0 }, { 0, 'f', 0, 1, { 1.1 }, 0 } } }, 10.5, 0.4, 0, 60, "skull", "bone" },
    { "thornwild", { 0, "ribs", 1, { { 0, 's', "bone", 0, { 0 }, 0 } } }, 10.5, 0.4, 0, 60, "ribs", "bone" },
    { "thornwild", { 0, "tool", 1, { { 0, 's', "rifle", 0, { 0 }, 0 } } }, 10.5, 0.4, 0, 60, "tool:rifle", "iron" },
    { "thornwild", { 0, "crater", 2, { { 0, 's', "root", 0, { 0 }, 0 }, { 0, 's', "bile", 0, { 0 }, 0 } } }, 10.5, 0.4, 0, 60, "crater", "root" },
    { "thornwild", { 0, "crystals", 2, { { 0, 's', "spikethorn", 0, { 0 }, 0 }, { 0, 'f', 0, 1, { 2.4 }, 0 } } }, 10.5, 0.4, 0, 60, "crystals", "spikethorn" },
    { "thornwild", { 0, "pile", 3, { { 0, 's', "root", 0, { 0 }, 0 }, { 0, 'T', 0, 2, { 4.0, 5.2 }, 0 }, { 0, 'f', 0, 1, { 1.0 }, 0 } } }, 10.5, 0.4, 0, 60, "pile", "root" },
};
const int FT_NNEW = 254;
/* features.LANDMARKS: (smallest base mm, lm(L_x(...), r_frac, gap, ground)) */
const FtLmSpec FT_LMS[] = {
    { "badlands", 60.0, { 0, "skeleton", 1, { { 0, 's', "bone", 0, { 0 }, 0 } } }, 0.55, 0.6, 0 },
    { "badlands", 60.0, { 0, "crater", 2, { { 0, 's', "sandstone", 0, { 0 }, 0 }, { 0, 's', "pit", 0, { 0 }, 0 } } }, 0.55, 0.6, 0 },
    { "badlands", 60.0, { 0, "track", 2, { { 0, 's', "iron", 0, { 0 }, 0 }, { 0, 's', "wood", 0, { 0 }, 0 } } }, 0.35, 0.6, 0 },
    { "badlands", 100.0, { 0, "river", 2, { { 0, 's', "water", 0, { 0 }, 0 }, { 0, 's', "rock", 0, { 0 }, 0 } } }, 0.2, 0.6, 1 },
    { "badlands", 100.0, { 0, "hull", 2, { { 0, 's', "wood", 0, { 0 }, 0 }, { 0, 's', "wood", 0, { 0 }, 0 } } }, 0.55, 0.6, 0 },
    { "badlands", 130.0, { 0, "ruin", 2, { { 0, 's', "sandstone", 0, { 0 }, 0 }, { 0, 's', "sandstone", 0, { 0 }, 0 } } }, 0.55, 0.6, 0 },
    { "highland", 60.0, { 0, "circle", 2, { { 0, 's', "glyph", 0, { 0 }, 0 }, { 0, 's', "standing", 0, { 0 }, 0 } } }, 0.55, 0.6, 0 },
    { "highland", 60.0, { 0, "wall", 1, { { 0, 's', "slate", 0, { 0 }, 0 } } }, 0.55, 0.6, 0 },
    { "highland", 60.0, { 0, "tree", 2, { { 0, 's', "wood", 0, { 0 }, 0 }, { 0, 's', "wood", 0, { 0 }, 0 } } }, 0.55, 0.6, 0 },
    { "highland", 100.0, { 0, "river", 2, { { 0, 's', "water", 0, { 0 }, 0 }, { 0, 's', "slate", 0, { 0 }, 0 } } }, 0.2, 0.6, 1 },
    { "highland", 100.0, { 0, "ruin", 2, { { 0, 's', "slate", 0, { 0 }, 0 }, { 0, 's', "slate", 0, { 0 }, 0 } } }, 0.55, 0.6, 0 },
    { "highland", 130.0, { 0, "dais", 2, { { 0, 's', "slate", 0, { 0 }, 0 }, { 0, 's', "column", 0, { 0 }, 0 } } }, 0.55, 0.6, 0 },
    { "maahl", 60.0, { 0, "spires", 1, { { 0, 's', "crystal", 0, { 0 }, 0 } } }, 0.55, 0.6, 0 },
    { "maahl", 60.0, { 0, "wall", 1, { { 0, 's', "ruin", 0, { 0 }, 0 } } }, 0.55, 0.6, 0 },
    { "maahl", 60.0, { 0, "dais", 2, { { 0, 's', "ruin", 0, { 0 }, 0 }, { 0, 's', "altar", 0, { 0 }, 0 } } }, 0.55, 0.6, 0 },
    { "maahl", 100.0, { 0, "colonnade", 2, { { 0, 's', "column", 0, { 0 }, 0 }, { 0, 's', "pavers", 0, { 0 }, 0 } } }, 0.55, 0.6, 0 },
    { "maahl", 100.0, { 0, "skeleton", 1, { { 0, 's', "bone", 0, { 0 }, 0 } } }, 0.55, 0.6, 0 },
    { "maahl", 130.0, { 0, "ruin", 2, { { 0, 's', "ruin", 0, { 0 }, 0 }, { 0, 's', "pavers", 0, { 0 }, 0 } } }, 0.55, 0.6, 0 },
    { "corridor", 60.0, { 0, "track", 2, { { 0, 's', "metal", 0, { 0 }, 0 }, { 0, 's', "metal", 0, { 0 }, 0 } } }, 0.35, 0.6, 0 },
    { "corridor", 60.0, { 0, "wall", 2, { { 0, 's', "metal", 0, { 0 }, 0 }, { 0, 's', "pipe", 0, { 0 }, 0 } } }, 0.55, 0.6, 0 },
    { "corridor", 60.0, { 0, "dais", 2, { { 0, 's', "plate", 0, { 0 }, 0 }, { 0, 's', "column", 0, { 0 }, 0 } } }, 0.55, 0.6, 0 },
    { "corridor", 100.0, { 0, "vehicle", 2, { { 0, 's', "metal", 0, { 0 }, 0 }, { 0, 's', "metal", 0, { 0 }, 0 } } }, 0.55, 0.6, 0 },
    { "corridor", 100.0, { 0, "ruin", 2, { { 0, 's', "metal", 0, { 0 }, 0 }, { 0, 's', "plate", 0, { 0 }, 0 } } }, 0.55, 0.6, 0 },
    { "corridor", 130.0, { 0, "colonnade", 2, { { 0, 's', "metal", 0, { 0 }, 0 }, { 0, 's', "plate", 0, { 0 }, 0 } } }, 0.55, 0.6, 0 },
    { "infestation", 60.0, { 0, "tentacles", 2, { { 0, 's', "tendril", 0, { 0 }, 0 }, { 0, 's', "acid", 0, { 0 }, 0 } } }, 0.55, 0.6, 0 },
    { "infestation", 60.0, { 0, "nest", 2, { { 0, 's', "flesh", 0, { 0 }, 0 }, { 0, 's', "egg", 0, { 0 }, 0 } } }, 0.55, 0.6, 0 },
    { "infestation", 60.0, { 0, "skeleton", 1, { { 0, 's', "bone", 0, { 0 }, 0 } } }, 0.55, 0.6, 0 },
    { "infestation", 100.0, { 0, "river", 2, { { 0, 's', "acid", 0, { 0 }, 0 }, { 0, 's', "pustule", 0, { 0 }, 0 } } }, 0.2, 0.6, 1 },
    { "infestation", 100.0, { 0, "vehicle", 2, { { 0, 's', "carapace", 0, { 0 }, 0 }, { 0, 's', "spine", 0, { 0 }, 0 } } }, 0.55, 0.6, 0 },
    { "infestation", 130.0, { 0, "spires", 1, { { 0, 's', "spine", 0, { 0 }, 0 } } }, 0.55, 0.6, 0 },
    { "urban", 60.0, { 0, "wall", 2, { { 0, 's', "brick", 0, { 0 }, 0 }, { 0, 's', "concrete", 0, { 0 }, 0 } } }, 0.55, 0.6, 0 },
    { "urban", 60.0, { 0, "crater", 2, { { 0, 's', "rubble", 0, { 0 }, 0 }, { 0, 's', "pothole", 0, { 0 }, 0 } } }, 0.55, 0.6, 0 },
    { "urban", 60.0, { 0, "track", 2, { { 0, 's', "iron", 0, { 0 }, 0 }, { 0, 's', "concrete", 0, { 0 }, 0 } } }, 0.35, 0.6, 0 },
    { "urban", 100.0, { 0, "ruin", 2, { { 0, 's', "brick", 0, { 0 }, 0 }, { 0, 's', "concrete", 0, { 0 }, 0 } } }, 0.55, 0.6, 0 },
    { "urban", 100.0, { 0, "vehicle", 2, { { 0, 's', "metal", 0, { 0 }, 0 }, { 0, 's', "iron", 0, { 0 }, 0 } } }, 0.55, 0.6, 0 },
    { "urban", 130.0, { 0, "colonnade", 2, { { 0, 's', "concrete", 0, { 0 }, 0 }, { 0, 's', "concrete", 0, { 0 }, 0 } } }, 0.55, 0.6, 0 },
    { "mine", 60.0, { 0, "track", 2, { { 0, 's', "iron", 0, { 0 }, 0 }, { 0, 's', "wood", 0, { 0 }, 0 } } }, 0.35, 0.6, 0 },
    { "mine", 60.0, { 0, "spires", 1, { { 0, 's', "quartz", 0, { 0 }, 0 } } }, 0.55, 0.6, 0 },
    { "mine", 60.0, { 0, "boardwalk", 2, { { 0, 's', "wood", 0, { 0 }, 0 }, { 0, 's', "wood", 0, { 0 }, 0 } } }, 0.55, 0.6, 0 },
    { "mine", 100.0, { 0, "ruin", 2, { { 0, 's', "wood", 0, { 0 }, 0 }, { 0, 's', "wood", 0, { 0 }, 0 } } }, 0.55, 0.6, 0 },
    { "mine", 100.0, { 0, "river", 2, { { 0, 's', "water", 0, { 0 }, 0 }, { 0, 's', "rock", 0, { 0 }, 0 } } }, 0.2, 0.6, 1 },
    { "mine", 130.0, { 0, "skeleton", 1, { { 0, 's', "bone", 0, { 0 }, 0 } } }, 0.55, 0.6, 0 },
    { "swamp", 60.0, { 0, "boardwalk", 2, { { 0, 's', "log", 0, { 0 }, 0 }, { 0, 's', "log", 0, { 0 }, 0 } } }, 0.55, 0.6, 0 },
    { "swamp", 60.0, { 0, "tree", 2, { { 0, 's', "stump", 0, { 0 }, 0 }, { 0, 's', "root", 0, { 0 }, 0 } } }, 0.55, 0.6, 0 },
    { "swamp", 60.0, { 0, "pool", 2, { { 0, 's', "rock", 0, { 0 }, 0 }, { 0, 's', "water", 0, { 0 }, 0 } } }, 0.55, 0.6, 1 },
    { "swamp", 100.0, { 0, "hull", 2, { { 0, 's', "log", 0, { 0 }, 0 }, { 0, 's', "log", 0, { 0 }, 0 } } }, 0.55, 0.6, 0 },
    { "swamp", 100.0, { 0, "tentacles", 2, { { 0, 's', "root", 0, { 0 }, 0 }, { 0, 's', "water", 0, { 0 }, 0 } } }, 0.55, 0.6, 0 },
    { "swamp", 130.0, { 0, "ruin", 2, { { 0, 's', "tflag", 0, { 0 }, 0 }, { 0, 's', "tflag", 0, { 0 }, 0 } } }, 0.55, 0.6, 0 },
    { "cynder", 60.0, { 0, "crater", 2, { { 0, 's', "basalt", 0, { 0 }, 0 }, { 0, 's', "lava", 0, { 0 }, 0 } } }, 0.55, 0.6, 0 },
    { "cynder", 60.0, { 0, "spires", 1, { { 0, 's', "obsidian", 0, { 0 }, 0 } } }, 0.55, 0.6, 0 },
    { "cynder", 60.0, { 0, "circle", 2, { { 0, 's', "lava", 0, { 0 }, 0 }, { 0, 's', "basalt", 0, { 0 }, 0 } } }, 0.55, 0.6, 0 },
    { "cynder", 100.0, { 0, "river", 2, { { 0, 's', "lava", 0, { 0 }, 0 }, { 0, 's', "basalt", 0, { 0 }, 0 } } }, 0.2, 0.6, 1 },
    { "cynder", 100.0, { 0, "skeleton", 1, { { 0, 's', "bone", 0, { 0 }, 0 } } }, 0.55, 0.6, 0 },
    { "cynder", 130.0, { 0, "dais", 4, { { 0, 's', "basalt", 0, { 0 }, 0 }, { 0, 's', "brazier", 0, { 0 }, 0 }, { 0, 's', "fire", 0, { 0 }, 0 }, { 0, 's', "iron", 0, { 0 }, 0 } } }, 0.55, 0.6, 0 },
    { "targa", 60.0, { 0, "spires", 1, { { 0, 's', "ice", 0, { 0 }, 0 } } }, 0.55, 0.6, 0 },
    { "targa", 60.0, { 0, "circle", 2, { { 0, 's', "slot", 0, { 0 }, 0 }, { 0, 's', "alien", 0, { 0 }, 0 } } }, 0.55, 0.6, 0 },
    { "targa", 60.0, { 0, "wall", 2, { { 0, 's', "alien", 0, { 0 }, 0 }, { 0, 's', "crystal", 0, { 0 }, 0 } } }, 0.55, 0.6, 0 },
    { "targa", 100.0, { 0, "vehicle", 2, { { 0, 's', "alien", 0, { 0 }, 0 }, { 0, 's', "alien", 0, { 0 }, 0 } } }, 0.55, 0.6, 0 },
    { "targa", 100.0, { 0, "colonnade", 2, { { 0, 's', "alien", 0, { 0 }, 0 }, { 0, 's', "alien", 0, { 0 }, 0 } } }, 0.55, 0.6, 0 },
    { "targa", 130.0, { 0, "dais", 2, { { 0, 's', "alien", 0, { 0 }, 0 }, { 0, 's', "column", 0, { 0 }, 0 } } }, 0.55, 0.6, 0 },
    { "trederra", 60.0, { 0, "crater", 2, { { 0, 's', "mud", 0, { 0 }, 0 }, { 0, 's', "water", 0, { 0 }, 0 } } }, 0.55, 0.6, 0 },
    { "trederra", 60.0, { 0, "boardwalk", 2, { { 0, 's', "duck", 0, { 0 }, 0 }, { 0, 's', "wood", 0, { 0 }, 0 } } }, 0.55, 0.6, 0 },
    { "trederra", 60.0, { 0, "wall", 1, { { 0, 's', "sandbag", 0, { 0 }, 0 } } }, 0.55, 0.6, 0 },
    { "trederra", 100.0, { 0, "track", 2, { { 0, 's', "iron", 0, { 0 }, 0 }, { 0, 's', "wood", 0, { 0 }, 0 } } }, 0.35, 0.6, 0 },
    { "trederra", 100.0, { 0, "ruin", 2, { { 0, 's', "brick", 0, { 0 }, 0 }, { 0, 's', "duck", 0, { 0 }, 0 } } }, 0.55, 0.6, 0 },
    { "trederra", 130.0, { 0, "vehicle", 2, { { 0, 's', "iron", 0, { 0 }, 0 }, { 0, 's', "iron", 0, { 0 }, 0 } } }, 0.55, 0.6, 0 },
    { "fortress", 60.0, { 0, "dais", 4, { { 0, 's', "plank", 0, { 0 }, 0 }, { 0, 's', "brazier", 0, { 0 }, 0 }, { 0, 's', "fire", 0, { 0 }, 0 }, { 0, 's', "iron", 0, { 0 }, 0 } } }, 0.55, 0.6, 0 },
    { "fortress", 60.0, { 0, "pool", 2, { { 0, 's', "garden", 0, { 0 }, 0 }, { 0, 's', "water", 0, { 0 }, 0 } } }, 0.55, 0.6, 1 },
    { "fortress", 60.0, { 0, "wall", 1, { { 0, 's', "garden", 0, { 0 }, 0 } } }, 0.55, 0.6, 0 },
    { "fortress", 100.0, { 0, "ruin", 2, { { 0, 's', "wood", 0, { 0 }, 0 }, { 0, 's', "tatami", 0, { 0 }, 0 } } }, 0.55, 0.6, 0 },
    { "fortress", 100.0, { 0, "colonnade", 2, { { 0, 's', "wood", 0, { 0 }, 0 }, { 0, 's', "plank", 0, { 0 }, 0 } } }, 0.55, 0.6, 0 },
    { "fortress", 130.0, { 0, "river", 2, { { 0, 's', "water", 0, { 0 }, 0 }, { 0, 's', "garden", 0, { 0 }, 0 } } }, 0.2, 0.6, 1 },
    { "beast", 60.0, { 0, "skeleton", 1, { { 0, 's', "bone", 0, { 0 }, 0 } } }, 0.55, 0.6, 0 },
    { "beast", 60.0, { 0, "tentacles", 2, { { 0, 's', "vein", 0, { 0 }, 0 }, { 0, 's', "bile", 0, { 0 }, 0 } } }, 0.55, 0.6, 0 },
    { "beast", 60.0, { 0, "nest", 2, { { 0, 's', "flesh", 0, { 0 }, 0 }, { 0, 's', "egg", 0, { 0 }, 0 } } }, 0.55, 0.6, 0 },
    { "beast", 100.0, { 0, "river", 2, { { 0, 's', "bile", 0, { 0 }, 0 }, { 0, 's', "pustule", 0, { 0 }, 0 } } }, 0.2, 0.6, 1 },
    { "beast", 100.0, { 0, "hull", 2, { { 0, 's', "bone", 0, { 0 }, 0 }, { 0, 's', "bone", 0, { 0 }, 0 } } }, 0.55, 0.6, 0 },
    { "beast", 130.0, { 0, "spires", 1, { { 0, 's', "tooth", 0, { 0 }, 0 } } }, 0.55, 0.6, 0 },
    { "frontier", 60.0, { 0, "track", 2, { { 0, 's', "iron", 0, { 0 }, 0 }, { 0, 's', "post", 0, { 0 }, 0 } } }, 0.35, 0.6, 0 },
    { "frontier", 60.0, { 0, "boardwalk", 2, { { 0, 's', "board", 0, { 0 }, 0 }, { 0, 's', "post", 0, { 0 }, 0 } } }, 0.55, 0.6, 0 },
    { "frontier", 60.0, { 0, "hull", 2, { { 0, 's', "board", 0, { 0 }, 0 }, { 0, 's', "post", 0, { 0 }, 0 } } }, 0.55, 0.6, 0 },
    { "frontier", 100.0, { 0, "ruin", 2, { { 0, 's', "board", 0, { 0 }, 0 }, { 0, 's', "board", 0, { 0 }, 0 } } }, 0.55, 0.6, 0 },
    { "frontier", 100.0, { 0, "river", 2, { { 0, 's', "water", 0, { 0 }, 0 }, { 0, 's', "rock", 0, { 0 }, 0 } } }, 0.2, 0.6, 1 },
    { "frontier", 130.0, { 0, "skeleton", 1, { { 0, 's', "bone", 0, { 0 }, 0 } } }, 0.55, 0.6, 0 },
    { "graveyard", 60.0, { 0, "wall", 1, { { 0, 's', "headstone", 0, { 0 }, 0 } } }, 0.55, 0.6, 0 },
    { "graveyard", 60.0, { 0, "tree", 2, { { 0, 's', "earth", 0, { 0 }, 0 }, { 0, 's', "earth", 0, { 0 }, 0 } } }, 0.55, 0.6, 0 },
    { "graveyard", 60.0, { 0, "circle", 2, { { 0, 's', "glyph", 0, { 0 }, 0 }, { 0, 's', "headstone", 0, { 0 }, 0 } } }, 0.55, 0.6, 0 },
    { "graveyard", 100.0, { 0, "ruin", 2, { { 0, 's', "headstone", 0, { 0 }, 0 }, { 0, 's', "headstone", 0, { 0 }, 0 } } }, 0.55, 0.6, 0 },
    { "graveyard", 100.0, { 0, "skeleton", 1, { { 0, 's', "bone", 0, { 0 }, 0 } } }, 0.55, 0.6, 0 },
    { "graveyard", 130.0, { 0, "colonnade", 2, { { 0, 's', "headstone", 0, { 0 }, 0 }, { 0, 's', "headstone", 0, { 0 }, 0 } } }, 0.55, 0.6, 0 },
    { "forest", 60.0, { 0, "tree", 2, { { 0, 's', "stump", 0, { 0 }, 0 }, { 0, 's', "root", 0, { 0 }, 0 } } }, 0.55, 0.6, 0 },
    { "forest", 60.0, { 0, "pool", 2, { { 0, 's', "rock", 0, { 0 }, 0 }, { 0, 's', "water", 0, { 0 }, 0 } } }, 0.55, 0.6, 1 },
    { "forest", 60.0, { 0, "circle", 2, { { 0, 's', "glyph", 0, { 0 }, 0 }, { 0, 's', "rock", 0, { 0 }, 0 } } }, 0.55, 0.6, 0 },
    { "forest", 100.0, { 0, "river", 2, { { 0, 's', "water", 0, { 0 }, 0 }, { 0, 's', "rock", 0, { 0 }, 0 } } }, 0.2, 0.6, 1 },
    { "forest", 100.0, { 0, "boardwalk", 2, { { 0, 's', "log", 0, { 0 }, 0 }, { 0, 's', "log", 0, { 0 }, 0 } } }, 0.55, 0.6, 0 },
    { "forest", 130.0, { 0, "ruin", 2, { { 0, 's', "rock", 0, { 0 }, 0 }, { 0, 's', "rock", 0, { 0 }, 0 } } }, 0.55, 0.6, 0 },
    { "dungeon", 60.0, { 0, "circle", 2, { { 0, 's', "glyph", 0, { 0 }, 0 }, { 0, 's', "flag", 0, { 0 }, 0 } } }, 0.55, 0.6, 0 },
    { "dungeon", 60.0, { 0, "dais", 2, { { 0, 's', "flag", 0, { 0 }, 0 }, { 0, 's', "altar", 0, { 0 }, 0 } } }, 0.55, 0.6, 0 },
    { "dungeon", 60.0, { 0, "wall", 1, { { 0, 's', "flag", 0, { 0 }, 0 } } }, 0.55, 0.6, 0 },
    { "dungeon", 100.0, { 0, "colonnade", 2, { { 0, 's', "flag", 0, { 0 }, 0 }, { 0, 's', "flag", 0, { 0 }, 0 } } }, 0.55, 0.6, 0 },
    { "dungeon", 100.0, { 0, "skeleton", 1, { { 0, 's', "bone", 0, { 0 }, 0 } } }, 0.55, 0.6, 0 },
    { "dungeon", 130.0, { 0, "river", 2, { { 0, 's', "water", 0, { 0 }, 0 }, { 0, 's', "flag", 0, { 0 }, 0 } } }, 0.2, 0.6, 1 },
    { "temple", 60.0, { 0, "dais", 4, { { 0, 's', "tflag", 0, { 0 }, 0 }, { 0, 's', "brazier", 0, { 0 }, 0 }, { 0, 's', "gold", 0, { 0 }, 0 }, { 0, 's', "tcolumn", 0, { 0 }, 0 } } }, 0.55, 0.6, 0 },
    { "temple", 60.0, { 0, "colonnade", 2, { { 0, 's', "tcolumn", 0, { 0 }, 0 }, { 0, 's', "tflag", 0, { 0 }, 0 } } }, 0.55, 0.6, 0 },
    { "temple", 60.0, { 0, "circle", 2, { { 0, 's', "glyph", 0, { 0 }, 0 }, { 0, 's', "tcolumn", 0, { 0 }, 0 } } }, 0.55, 0.6, 0 },
    { "temple", 100.0, { 0, "ruin", 2, { { 0, 's', "tcolumn", 0, { 0 }, 0 }, { 0, 's', "tflag", 0, { 0 }, 0 } } }, 0.55, 0.6, 0 },
    { "temple", 100.0, { 0, "pool", 2, { { 0, 's', "tcolumn", 0, { 0 }, 0 }, { 0, 's', "water", 0, { 0 }, 0 } } }, 0.55, 0.6, 1 },
    { "temple", 130.0, { 0, "wall", 2, { { 0, 's', "tcolumn", 0, { 0 }, 0 }, { 0, 's', "gold", 0, { 0 }, 0 } } }, 0.55, 0.6, 0 },
    { "coast", 60.0, { 0, "hull", 2, { { 0, 's', "drift", 0, { 0 }, 0 }, { 0, 's', "drift", 0, { 0 }, 0 } } }, 0.55, 0.6, 0 },
    { "coast", 60.0, { 0, "pool", 2, { { 0, 's', "shelf", 0, { 0 }, 0 }, { 0, 's', "water", 0, { 0 }, 0 } } }, 0.55, 0.6, 1 },
    { "coast", 60.0, { 0, "boardwalk", 2, { { 0, 's', "drift", 0, { 0 }, 0 }, { 0, 's', "drift", 0, { 0 }, 0 } } }, 0.55, 0.6, 0 },
    { "coast", 100.0, { 0, "skeleton", 1, { { 0, 's', "bone", 0, { 0 }, 0 } } }, 0.55, 0.6, 0 },
    { "coast", 100.0, { 0, "river", 2, { { 0, 's', "water", 0, { 0 }, 0 }, { 0, 's', "shelf", 0, { 0 }, 0 } } }, 0.2, 0.6, 1 },
    { "coast", 130.0, { 0, "tentacles", 2, { { 0, 's', "weed", 0, { 0 }, 0 }, { 0, 's', "water", 0, { 0 }, 0 } } }, 0.55, 0.6, 0 },
    { "ashveil", 60.0, { 0, "vehicle", 2, { { 0, 's', "ashhull", 0, { 0 }, 0 }, { 0, 's', "ashscrap", 0, { 0 }, 0 } } }, 0.55, 0.6, 0 },
    { "ashveil", 60.0, { 0, "crater", 2, { { 0, 's', "ashcinder", 0, { 0 }, 0 }, { 0, 's', "pit", 0, { 0 }, 0 } } }, 0.55, 0.6, 0 },
    { "ashveil", 60.0, { 0, "skeleton", 1, { { 0, 's', "bone", 0, { 0 }, 0 } } }, 0.55, 0.6, 0 },
    { "ashveil", 100.0, { 0, "dais", 2, { { 0, 's', "ashstone", 0, { 0 }, 0 }, { 0, 's', "column", 0, { 0 }, 0 } } }, 0.55, 0.6, 0 },
    { "ashveil", 100.0, { 0, "track", 2, { { 0, 's', "rust", 0, { 0 }, 0 }, { 0, 's', "ashscrap", 0, { 0 }, 0 } } }, 0.35, 0.6, 0 },
    { "ashveil", 130.0, { 0, "wall", 1, { { 0, 's', "ashstone", 0, { 0 }, 0 } } }, 0.55, 0.6, 0 },
    { "blight", 60.0, { "style_blight", "thorns", 1, { { 0, 's', "thorn", 0, { 0 }, 0 } } }, 0.55, 0.6, 0 },
    { "blight", 60.0, { "style_blight", "rift", 2, { { 0, 's', "rift", 0, { 0 }, 0 }, { 0, 's', "blightrock", 0, { 0 }, 0 } } }, 0.35, 0.6, 1 },
    { "blight", 60.0, { "style_blight", "rune_ring", 2, { { 0, 's', "runecut", 0, { 0 }, 0 }, { 0, 's', "rift", 0, { 0 }, 0 } } }, 0.55, 0.6, 1 },
    { "blight", 100.0, { "style_blight", "boil", 2, { { 0, 's', "ichor", 0, { 0 }, 0 }, { 0, 's', "blister", 0, { 0 }, 0 } } }, 0.55, 0.6, 1 },
    { "blight", 100.0, { 0, "skeleton", 1, { { 0, 's', "bone", 0, { 0 }, 0 } } }, 0.55, 0.6, 0 },
    { "blight", 130.0, { "style_blight", "bones", 1, { { 0, 's', "bone", 0, { 0 }, 0 } } }, 0.55, 0.6, 0 },
    { "ferrum", 60.0, { 0, "track", 2, { { 0, 's', "iron", 0, { 0 }, 0 }, { 0, 's', "rust", 0, { 0 }, 0 } } }, 0.35, 0.6, 0 },
    { "ferrum", 60.0, { 0, "wall", 2, { { 0, 's', "girder", 0, { 0 }, 0 }, { 0, 's', "pipe", 0, { 0 }, 0 } } }, 0.55, 0.6, 0 },
    { "ferrum", 60.0, { 0, "crater", 2, { { 0, 's', "slag", 0, { 0 }, 0 }, { 0, 's', "tar", 0, { 0 }, 0 } } }, 0.55, 0.6, 0 },
    { "ferrum", 100.0, { 0, "vehicle", 2, { { 0, 's', "rust", 0, { 0 }, 0 }, { 0, 's', "rust", 0, { 0 }, 0 } } }, 0.55, 0.6, 0 },
    { "ferrum", 100.0, { 0, "spires", 1, { { 0, 's', "slag", 0, { 0 }, 0 } } }, 0.55, 0.6, 0 },
    { "ferrum", 130.0, { 0, "ruin", 2, { { 0, 's', "girder", 0, { 0 }, 0 }, { 0, 's', "plate", 0, { 0 }, 0 } } }, 0.55, 0.6, 0 },
    { "obelis", 60.0, { "style_obelis", "obelisk", 0, { { 0 } } }, 0.55, 0.6, 0 },
    { "obelis", 60.0, { "style_obelis", "pylons", 0, { { 0 } } }, 0.55, 0.6, 0 },
    { "obelis", 60.0, { 0, "colonnade", 2, { { 0, 's', "monolith", 0, { 0 }, 0 }, { 0, 's', "obfloor", 0, { 0 }, 0 } } }, 0.55, 0.6, 0 },
    { "obelis", 100.0, { 0, "ruin", 2, { { 0, 's', "monolith", 0, { 0 }, 0 }, { 0, 's', "obfloor", 0, { 0 }, 0 } } }, 0.55, 0.6, 0 },
    { "obelis", 100.0, { 0, "dais", 2, { { 0, 's', "obfloor", 0, { 0 }, 0 }, { 0, 's', "column", 0, { 0 }, 0 } } }, 0.55, 0.6, 0 },
    { "obelis", 130.0, { 0, "wall", 2, { { 0, 's', "monolith", 0, { 0 }, 0 }, { 0, 's', "obmetal", 0, { 0 }, 0 } } }, 0.55, 0.6, 0 },
    { "sundered", 60.0, { 0, "wall", 1, { { 0, 's', "ashlar", 0, { 0 }, 0 } } }, 0.55, 0.6, 0 },
    { "sundered", 60.0, { 0, "crater", 2, { { 0, 's', "rubble", 0, { 0 }, 0 }, { 0, 's', "pothole", 0, { 0 }, 0 } } }, 0.55, 0.6, 0 },
    { "sundered", 60.0, { 0, "colonnade", 2, { { 0, 's', "ashlar", 0, { 0 }, 0 }, { 0, 's', "flag", 0, { 0 }, 0 } } }, 0.55, 0.6, 0 },
    { "sundered", 100.0, { 0, "ruin", 2, { { 0, 's', "ashlar", 0, { 0 }, 0 }, { 0, 's', "flag", 0, { 0 }, 0 } } }, 0.55, 0.6, 0 },
    { "sundered", 100.0, { 0, "dais", 2, { { 0, 's', "flag", 0, { 0 }, 0 }, { 0, 's', "column", 0, { 0 }, 0 } } }, 0.55, 0.6, 0 },
    { "sundered", 130.0, { 0, "vehicle", 2, { { 0, 's', "iron", 0, { 0 }, 0 }, { 0, 's', "iron", 0, { 0 }, 0 } } }, 0.55, 0.6, 0 },
    { "thornwild", 60.0, { 0, "tree", 2, { { 0, 's', "stump", 0, { 0 }, 0 }, { 0, 's', "root", 0, { 0 }, 0 } } }, 0.55, 0.6, 0 },
    { "thornwild", 60.0, { 0, "tentacles", 2, { { 0, 's', "creeper", 0, { 0 }, 0 }, { 0, 's', "bile", 0, { 0 }, 0 } } }, 0.55, 0.6, 0 },
    { "thornwild", 60.0, { 0, "nest", 2, { { 0, 's', "root", 0, { 0 }, 0 }, { 0, 's', "sporepod", 0, { 0 }, 0 } } }, 0.55, 0.6, 0 },
    { "thornwild", 100.0, { 0, "circle", 2, { { 0, 's', "glyph", 0, { 0 }, 0 }, { 0, 's', "glowcap", 0, { 0 }, 0 } } }, 0.55, 0.6, 0 },
    { "thornwild", 100.0, { 0, "vehicle", 2, { { 0, 's', "hullplate", 0, { 0 }, 0 }, { 0, 's', "hullplate", 0, { 0 }, 0 } } }, 0.55, 0.6, 0 },
    { "thornwild", 130.0, { 0, "spires", 1, { { 0, 's', "spikethorn", 0, { 0 }, 0 } } }, 0.55, 0.6, 0 },
};
const int FT_NLMS = 150;
/* @@END@@ */

/* ================================================================== sd_reg.py */
typedef struct { char *style, *name; double v; } KV;
typedef struct { KV *v; int n, cap; } KVT;
static void kv_put(KVT *t, const char *a, const char *b, double v) {
    int i; for (i = 0; i < t->n; i++) if (!strcmp(t->v[i].style, a) && !strcmp(t->v[i].name, b)) { t->v[i].v = v; return; }
    if (t->n == t->cap) { t->cap = t->cap ? 2 * t->cap : 256; t->v = (KV *)realloc(t->v, sizeof(KV) * (size_t)t->cap); }
    t->v[t->n].style = _strdup(a); t->v[t->n].name = _strdup(b); t->v[t->n].v = v; t->n++;
}
static int kv_get(const KVT *t, const char *a, const char *b, double *v) {
    int i; for (i = 0; i < t->n; i++) if (!strcmp(t->v[i].style, a) && !strcmp(t->v[i].name, b)) { *v = t->v[i].v; return 1; }
    return 0;
}
static void kv_clear(KVT *t) { int i; for (i = 0; i < t->n; i++) { free(t->v[i].style); free(t->v[i].name); } t->n = 0; }
static KVT T_REAL, T_DRAWN, CACHE, SCALE; static int HAS_STAR; static double STAR;

/* @@SCALE@@ */
/* scale/real.json + drawn.json: style, name, real size (m), drawn length (mm); 857 details */
static const char SDR_BUILTIN[] =
    "ashveil\tash_dune\t0.0\t3.1\n"
    "ashveil\tash_still\t1.5\t3.8\n"
    "ashveil\tbreath_mask\t0.3\t3.7\n"
    "ashveil\tburied_cab\t3.0\t6.8\n"
    "ashveil\tburied_hatch\t1.4\t4.9\n"
    "ashveil\tcargo_container\t6.1\t8.4\n"
    "ashveil\tcell_panel\t1.8\t3.6\n"
    "ashveil\tcinder_rocks\t0.0\t3.9\n"
    "ashveil\tdrift_wreck\t0.0\t4.3\n"
    "ashveil\tfallen_pylon\t4.0\t9.9\n"
    "ashveil\tgrazer_skull\t0.6\t5.4\n"
    "ashveil\thardpan_cracks\t0.0\t5.9\n"
    "ashveil\thub_wheel\t1.05\t4.0\n"
    "ashveil\tlean_to\t3.5\t7.4\n"
    "ashveil\tpylon_stub\t1.5\t3.5\n"
    "ashveil\trebar_slab\t2.0\t4.4\n"
    "ashveil\trelay_mast\t4.0\t8.8\n"
    "ashveil\trib_arches\t3.2\t6.8\n"
    "ashveil\tripple_patch\t0.0\t3.7\n"
    "ashveil\tsalvage_sled\t3.0\t8.0\n"
    "ashveil\tscrap_heap\t2.6\t4.8\n"
    "ashveil\tscrap_litter\t0.0\t4.9\n"
    "ashveil\tstilt_pipe\t5.0\t8.3\n"
    "ashveil\ttoppled_drums\t2.2\t4.4\n"
    "ashveil\ttrack_links\t2.4\t7.4\n"
    "ashveil\ttread_marks\t0.0\t5.9\n"
    "ashveil\tvalve_head\t1.3\t3.7\n"
    "ashveil\tvisor_helmet\t0.4\t3.5\n"
    "ashveil\twarning_sign\t2.0\t4.7\n"
    "badlands\tanthill\t0.0\t4.5\n"
    "badlands\tantlers\t0.8\t5.1\n"
    "badlands\tarrows\t0.8\t5.2\n"
    "badlands\tbarrel_cactus\t0.8\t4.5\n"
    "badlands\tbedroll_canteen\t1.0\t4.8\n"
    "badlands\tcairn\t0.6\t3.8\n"
    "badlands\tgold_pan\t0.4\t5.9\n"
    "badlands\that\t0.44\t4.0\n"
    "badlands\thitching_rail\t2.5\t7.3\n"
    "badlands\thorseshoes\t0.15\t2.7\n"
    "badlands\tlonghorn_skull\t1.4\t7.6\n"
    "badlands\tm_barrel\t0.88\t6.8\n"
    "badlands\tm_bone\t0.45\t6.9\n"
    "badlands\tm_bone_knob\t0.3\t5.4\n"
    "badlands\tm_boot\t0.35\t6.3\n"
    "badlands\tm_campfire\t0.9\t7.5\n"
    "badlands\tm_cart\t2.5\t10.7\n"
    "badlands\tm_cartridge\t0.07\t5.5\n"
    "badlands\tm_horn\t0.5\t8.5\n"
    "badlands\tm_horseshoe\t0.13\t5.3\n"
    "badlands\tm_pickaxe\t0.9\t7.7\n"
    "badlands\tm_sand_rock\t0.0\t6.3\n"
    "badlands\tm_sand_rocks\t0.0\t6.4\n"
    "badlands\tm_sheriff_star\t0.07\t4.5\n"
    "badlands\tm_shovel\t1.2\t8.9\n"
    "badlands\tm_skull\t0.22\t4.5\n"
    "badlands\tm_wagon_wheel\t1.0\t8.1\n"
    "badlands\tm_whiskey_bottle\t0.3\t7.3\n"
    "badlands\tox_yoke\t1.5\t6.9\n"
    "badlands\tpetrified_log\t0.0\t7.9\n"
    "badlands\tprickly_pear\t1.5\t4.2\n"
    "badlands\trattlesnake\t1.2\t6.7\n"
    "badlands\tsaddle\t0.75\t7.8\n"
    "badlands\tsaguaro\t2.5\t6.3\n"
    "badlands\tsidewinder_tracks\t0.0\t8.5\n"
    "badlands\ttumbleweed\t0.65\t3.5\n"
    "badlands\twater_trough\t3.0\t8.4\n"
    "beast\tbarnacles\t0.0\t4.4\n"
    "beast\tbile_sump\t0.0\t7.1\n"
    "beast\tbone_spurs\t0.0\t9.3\n"
    "beast\tcilia\t0.0\t4.9\n"
    "beast\tdissolving_barrel\t1.2\t6.4\n"
    "beast\teaten_cuirass\t0.4\t4.0\n"
    "beast\tembedded_eye\t0.0\t4.2\n"
    "beast\tfish_bones\t0.4\t6.5\n"
    "beast\tgallstones\t0.0\t4.0\n"
    "beast\tgut_folds\t0.0\t7.0\n"
    "beast\tlamprey_mouth\t0.0\t5.2\n"
    "beast\tm_barrel\t0.88\t6.6\n"
    "beast\tm_bone\t0.4\t5.3\n"
    "beast\tm_bone_knob\t0.3\t5.5\n"
    "beast\tm_cannon\t2.0\t8.4\n"
    "beast\tm_chest\t0.8\t5.3\n"
    "beast\tm_crate\t0.8\t5.0\n"
    "beast\tm_fish\t0.5\t7.7\n"
    "beast\tm_fish_bone\t0.5\t8.9\n"
    "beast\tm_horn\t0.5\t8.3\n"
    "beast\tm_insect_leg\t1.0\t10.0\n"
    "beast\tm_long_bone\t0.45\t7.6\n"
    "beast\tm_rowboat\t3.0\t8.1\n"
    "beast\tm_ship_wheel\t1.0\t8.9\n"
    "beast\tm_skull\t0.22\t4.8\n"
    "beast\tm_tusk\t2\t10.5\n"
    "beast\tmolar\t0.0\t3.6\n"
    "beast\tnerve_cluster\t0.0\t9.0\n"
    "beast\tparasite_worm\t0.0\t6.2\n"
    "beast\tpolyps\t0.0\t3.2\n"
    "beast\tsplit_cyst\t0.0\t4.6\n"
    "beast\ttaste_buds\t0.0\t4.6\n"
    "beast\ttendon_cords\t0.0\t7.9\n"
    "beast\tulcer\t0.0\t6.4\n"
    "beast\tvertebrae\t0.5\t9.9\n"
    "blight\tblisters\t0.0\t3.7\n"
    "blight\tblood_stone\t0.9\t3.4\n"
    "blight\tbone_pile\t1.2\t4.3\n"
    "blight\tbubbling_pool\t0.0\t5.7\n"
    "blight\tcracked_rune_ring\t0.0\t6.2\n"
    "blight\teye_stone\t0.0\t3.6\n"
    "blight\tflesh_lump\t0.0\t4.5\n"
    "blight\thorned_skull\t1.0\t4.7\n"
    "blight\tichor_seep\t0.0\t8.0\n"
    "blight\timpaled_skull\t1.4\t7.1\n"
    "blight\tleaning_thorns\t0.0\t6.3\n"
    "blight\toffering_bowl\t0.45\t2.5\n"
    "blight\tribcage\t1.4\t5.1\n"
    "blight\trift_mouth\t0.0\t6.7\n"
    "blight\trift_tear\t0.0\t6.8\n"
    "blight\tritual_candles\t0.5\t4.5\n"
    "blight\tritual_knife\t0.35\t5.2\n"
    "blight\trune_slab\t1.4\t4.8\n"
    "blight\trune_stone\t0.6\t2.5\n"
    "blight\tskull_heap\t0.7\t3.9\n"
    "blight\ttendril_hole\t0.0\t8.2\n"
    "blight\tthorn_ring\t0.0\t5.6\n"
    "blight\tthorn_shards\t0.0\t6.9\n"
    "blight\ttwisted_spike\t0.0\t5.3\n"
    "blight\tveined_rock\t0.0\t6.1\n"
    "coast\tbollard\t1.0\t7.3\n"
    "coast\tcannon\t2.9\t7.7\n"
    "coast\tcapstan\t1.4\t5.9\n"
    "coast\tconch\t0.25\t3.6\n"
    "coast\tcrab\t0.2\t5.0\n"
    "coast\tcrab_burrows\t0.0\t7.0\n"
    "coast\tdriftwood\t2.2\t10.0\n"
    "coast\tfish_crate\t0.9\t4.9\n"
    "coast\tjellyfish\t0.45\t5.8\n"
    "coast\tkelp\t0.0\t8.0\n"
    "coast\tlobster_pot\t1\t7.8\n"
    "coast\tm_amphora\t1\t6.4\n"
    "coast\tm_barrel\t0.88\t5.1\n"
    "coast\tm_bottle\t0.3\t6.8\n"
    "coast\tm_cannon\t2.0\t8.6\n"
    "coast\tm_chest\t0.8\t5.7\n"
    "coast\tm_crate_bottles\t0.6\t6.0\n"
    "coast\tm_fish\t0.5\t7.4\n"
    "coast\tm_fish_bone\t0.5\t9.5\n"
    "coast\tm_keg\t0.7\t6.9\n"
    "coast\tm_paddle\t1.5\t9.5\n"
    "coast\tm_propeller\t0.4\t6.8\n"
    "coast\tm_rocks\t0.0\t8.0\n"
    "coast\tm_rope\t0.35\t7.0\n"
    "coast\tm_rowboat\t3.0\t7.9\n"
    "coast\tm_scallop\t0.1\t5.0\n"
    "coast\tm_seashell\t0.1\t5.1\n"
    "coast\tm_ship_wheel\t1.0\t9.0\n"
    "coast\tm_shovel\t1.2\t9.8\n"
    "coast\tnet_floats\t1.2\t5.7\n"
    "coast\toars\t2.4\t8.9\n"
    "coast\tsand_castle\t0.6\t6.9\n"
    "coast\tscallops\t0.25\t5.0\n"
    "coast\tship_wheel\t1.0\t6.8\n"
    "coast\tspyglass_compass\t0.4\t4.4\n"
    "coast\tstarfish\t0.25\t5.8\n"
    "coast\turchins\t0.25\t4.4\n"
    "coast\twhale_vertebra\t0.7\t6.7\n"
    "corridor\tcable_spool\t1.0\t5.4\n"
    "corridor\tcell_rack\t0.8\t6.3\n"
    "corridor\tcryo_canister\t0.5\t4.3\n"
    "corridor\tdead_drone\t0.6\t4.7\n"
    "corridor\tdeck_hatch\t1.0\t4.0\n"
    "corridor\tdeck_stencil\t0.0\t5.4\n"
    "corridor\tdrip_tray\t0.0\t4.5\n"
    "corridor\tgas_cylinder\t1.2\t6.1\n"
    "corridor\thelmet\t0.38\t5.0\n"
    "corridor\those_reel\t1.0\t7.0\n"
    "corridor\thover_pallet\t1.2\t5.3\n"
    "corridor\tjunction_box\t1.2\t7.5\n"
    "corridor\tm_beacon\t0.6\t7.6\n"
    "corridor\tm_bolt\t0.04\t5.5\n"
    "corridor\tm_cables\t1.5\t10.4\n"
    "corridor\tm_canisters\t0.8\t6.6\n"
    "corridor\tm_cog\t0.6\t6.8\n"
    "corridor\tm_container_flat\t1.2\t8.1\n"
    "corridor\tm_container_tall\t1.0\t7.4\n"
    "corridor\tm_crate\t0.8\t7.9\n"
    "corridor\tm_fan\t0.4\t6.2\n"
    "corridor\tm_gas_bottle\t0.6\t5.5\n"
    "corridor\tm_holo_table\t1.0\t6.1\n"
    "corridor\tm_lever\t0.4\t5.0\n"
    "corridor\tm_nut\t0.03\t5.0\n"
    "corridor\tm_pipe_bend\t0.8\t7.2\n"
    "corridor\tm_radio\t0.2\t7.5\n"
    "corridor\tm_spanner\t0.25\t9.0\n"
    "corridor\tm_spring\t0.09\t6.9\n"
    "corridor\tm_valve\t0.8\t5.3\n"
    "corridor\tm_vent_cover\t0.45\t8.1\n"
    "corridor\tmagboots\t0.35\t4.2\n"
    "corridor\tmess_tray\t0.4\t5.2\n"
    "corridor\tplate_stack\t0.3\t3.9\n"
    "corridor\tterminal\t1.2\t3.6\n"
    "corridor\ttoolbox\t0.65\t5.4\n"
    "corridor\tvalve_stub\t0.6\t4.4\n"
    "corridor\tvent_fan\t0.6\t4.9\n"
    "corridor\twelding_set\t1.5\t7.7\n"
    "cynder\tanvil\t0.75\t4.5\n"
    "cynder\tash_drift\t0.0\t9.8\n"
    "cynder\tbasalt_columns\t0.0\t5.6\n"
    "cynder\tcharred_hand\t0.6\t8.7\n"
    "cynder\tcinder_cone\t0.0\t8.4\n"
    "cynder\tcrucible\t1.2\t7.3\n"
    "cynder\tcrust_cracks\t0.0\t6.8\n"
    "cynder\tember_moat\t0.0\t6.4\n"
    "cynder\tforge_bellows\t1.5\t7.9\n"
    "cynder\tfumarole\t0.0\t5.8\n"
    "cynder\thorn_spur\t0.8\t6.5\n"
    "cynder\timp_idol\t0.6\t4.3\n"
    "cynder\tingots\t0.6\t5.6\n"
    "cynder\tlava_bomb\t0.0\t5.6\n"
    "cynder\tm_boulders\t0.0\t8.2\n"
    "cynder\tm_breastplate\t0.45\t6.4\n"
    "cynder\tm_crater\t0.0\t8.4\n"
    "cynder\tm_horn\t0.5\t8.4\n"
    "cynder\tm_mace\t0.6\t11.0\n"
    "cynder\tm_meteor\t0.0\t6.9\n"
    "cynder\tm_shards\t0.0\t7.5\n"
    "cynder\tm_skull\t0.22\t4.9\n"
    "cynder\tm_sword\t0.95\t13.3\n"
    "cynder\tm_tusk\t2\t9.6\n"
    "cynder\tobsidian_blades\t0.0\t6.3\n"
    "cynder\tquench_trough\t1.3\t6.7\n"
    "cynder\tropy_lava\t0.0\t6.1\n"
    "cynder\tshackles\t0.6\t8.9\n"
    "cynder\tskylight\t0.0\t6.6\n"
    "cynder\tslag_heap\t0.0\t5.2\n"
    "cynder\tsulfur_crust\t0.0\t4.0\n"
    "dungeon\tbear_trap\t0.8\t7.5\n"
    "dungeon\tblock_and_axe\t1.1\t6.2\n"
    "dungeon\tblood_drain\t0.0\t7.3\n"
    "dungeon\tcandelabra\t1.3\t7.8\n"
    "dungeon\tcrown_sceptre\t0.8\t7.1\n"
    "dungeon\tdice_and_cup\t0.15\t4.6\n"
    "dungeon\tgibbet_cage\t2.0\t7.4\n"
    "dungeon\tkey_ring\t0.25\t5.3\n"
    "dungeon\tkeystone\t1.0\t5.1\n"
    "dungeon\tm_barrel\t0.88\t3.2\n"
    "dungeon\tm_bottles\t0.3\t3.1\n"
    "dungeon\tm_breastplate\t0.4\t6.2\n"
    "dungeon\tm_candlestick\t0.3\t7.6\n"
    "dungeon\tm_chest\t0.8\t3.7\n"
    "dungeon\tm_chest_gold\t0.8\t4.9\n"
    "dungeon\tm_coins\t0.2\t6.0\n"
    "dungeon\tm_gem\t0.02\t4.7\n"
    "dungeon\tm_goblet\t0.18\t5.1\n"
    "dungeon\tm_key\t0.08\t5.5\n"
    "dungeon\tm_keyring\t0.25\t5.4\n"
    "dungeon\tm_mace\t0.6\t11.7\n"
    "dungeon\tm_padlock\t0.08\t4.9\n"
    "dungeon\tm_pot\t0.45\t3.5\n"
    "dungeon\tm_potion\t0.15\t2.5\n"
    "dungeon\tm_scroll\t0.35\t6.1\n"
    "dungeon\tm_shield\t0.8\t3.5\n"
    "dungeon\tm_skull\t0.22\t5.0\n"
    "dungeon\tm_sword\t0.95\t5.6\n"
    "dungeon\tm_trap\t0.8\t5.4\n"
    "dungeon\tm_trunk\t1.0\t5.2\n"
    "dungeon\tmace_and_shield\t1.0\t5.3\n"
    "dungeon\tmimic\t1.1\t5.3\n"
    "dungeon\tooze\t0.0\t7.0\n"
    "dungeon\tportcullis_piece\t1.2\t5.8\n"
    "dungeon\tpotions\t0.4\t9.4\n"
    "dungeon\tpressure_plate\t0.6\t6.0\n"
    "dungeon\tscrolls\t0.45\t7.0\n"
    "dungeon\tskull_candle\t0.25\t3.0\n"
    "dungeon\tspellbook\t0.6\t5.0\n"
    "dungeon\tstocks\t1.6\t6.5\n"
    "dungeon\ttorch\t0.6\t6.7\n"
    "ferrum\tblower_hood\t0.9\t4.5\n"
    "ferrum\tbolt_head\t0.5\t3.8\n"
    "ferrum\tcable_bundle\t2.5\t12.7\n"
    "ferrum\tcable_drum\t1.2\t6.9\n"
    "ferrum\tchain_heap\t1.0\t7.0\n"
    "ferrum\tcrane_hook\t1.5\t9.7\n"
    "ferrum\tcrane_jib\t3.0\t9.1\n"
    "ferrum\tcrucible\t1.2\t6.7\n"
    "ferrum\tdust_drift\t0.0\t6.2\n"
    "ferrum\tfloor_vent\t0.8\t3.7\n"
    "ferrum\tgauge_panel\t0.8\t5.6\n"
    "ferrum\tgear_on_edge\t0.9\t4.9\n"
    "ferrum\tgear_pair\t0.8\t5.4\n"
    "ferrum\tgirder_stub\t2.0\t5.6\n"
    "ferrum\thalf_gear\t1.2\t6.9\n"
    "ferrum\tingot_stack\t0.9\t4.4\n"
    "ferrum\toil_slick\t0.0\t5.0\n"
    "ferrum\tpipe_elbow\t1.2\t6.4\n"
    "ferrum\tpipe_run\t3.0\t7.6\n"
    "ferrum\tpiston_ram\t1.5\t7.2\n"
    "ferrum\trivets_and_nuts\t0.3\t4.9\n"
    "ferrum\tslag_lumps\t0.0\t2.9\n"
    "ferrum\tslag_tongue\t0.0\t5.5\n"
    "ferrum\ttorn_plate\t1.0\t2.8\n"
    "ferrum\tvalve_wheel\t0.5\t3.8\n"
    "ferrum\tvent_stack\t1.2\t6.7\n"
    "ferrum\twalker_foot\t1.4\t7.1\n"
    "ferrum\twalker_leg\t3.0\t12.7\n"
    "forest\tacorns_and_leaves\t0.25\t5.0\n"
    "forest\tamanita\t0.2\t3.3\n"
    "forest\tboar_wallow\t0.0\t11.0\n"
    "forest\tbrambles\t0.0\t7.4\n"
    "forest\tfallen_branch\t2.0\t8.5\n"
    "forest\tfeathers_pellets\t0.35\t6.0\n"
    "forest\tfern_fronds\t0.9\t6.1\n"
    "forest\tfox_den\t0.0\t6.6\n"
    "forest\thollow_log\t1.5\t6.1\n"
    "forest\tleaf_litter\t0.0\t5.4\n"
    "forest\tm_axe\t0.8\t7.3\n"
    "forest\tm_campfire\t0.9\t7.1\n"
    "forest\tm_fallen_tree\t3.0\t11.1\n"
    "forest\tm_fire_pit\t0.9\t6.8\n"
    "forest\tm_firewood\t0.35\t6.3\n"
    "forest\tm_log\t1.5\t9.4\n"
    "forest\tm_mallet\t0.35\t7.2\n"
    "forest\tm_mushroom\t0.1\t4.9\n"
    "forest\tm_rocks\t0.0\t8.4\n"
    "forest\tm_toadstool\t0.15\t4.9\n"
    "forest\tm_tree_log\t2.0\t10.6\n"
    "forest\tmoss_stones\t0.0\t5.8\n"
    "forest\tpine_cones\t0.25\t4.8\n"
    "forest\tpuffballs\t0.25\t3.8\n"
    "forest\tsap_bucket\t0.3\t4.8\n"
    "forest\tsnag\t0.7\t3.2\n"
    "forest\tsnare\t0.7\t7.5\n"
    "forest\tsquirrel_cache\t0.3\t5.9\n"
    "forest\twaymarker\t0.8\t4.2\n"
    "forest\twild_hive\t0.5\t3.8\n"
    "forest\twolf_tracks\t0.0\t7.0\n"
    "fortress\tbamboo_fence\t1.8\t6.0\n"
    "fortress\tbonsai\t0.6\t4.4\n"
    "fortress\tdeer_scarer\t1.1\t6.6\n"
    "fortress\tfan\t0.45\t5.5\n"
    "fortress\tgeta\t0.25\t4.2\n"
    "fortress\tgo_board\t0.45\t7.3\n"
    "fortress\tkabuto\t0.4\t4.8\n"
    "fortress\tkoro\t0.25\t3.2\n"
    "fortress\tm_barrel\t0.88\t3.8\n"
    "fortress\tm_barrels\t1.2\t3.9\n"
    "fortress\tm_bedroll\t1.9\t5.1\n"
    "fortress\tm_bolt\t0.8\t6.8\n"
    "fortress\tm_chest\t0.8\t3.3\n"
    "fortress\tm_cuirass\t0.45\t6.2\n"
    "fortress\tm_kunai\t0.25\t9.1\n"
    "fortress\tm_pot\t0.45\t3.3\n"
    "fortress\tm_rice_bowl\t0.15\t4.6\n"
    "fortress\tm_roof_tile\t0.3\t6.6\n"
    "fortress\tm_roof_tiles\t0.4\t7.4\n"
    "fortress\tm_sai\t0.5\t9.8\n"
    "fortress\tm_scroll\t0.35\t6.1\n"
    "fortress\tm_shuriken\t0.1\t3.9\n"
    "fortress\tm_spear\t2.2\t7.2\n"
    "fortress\tm_stone_lantern\t1.5\t9.3\n"
    "fortress\tm_teapot\t0.25\t4.6\n"
    "fortress\tmenpo\t0.2\t5.3\n"
    "fortress\tpaper_lantern\t0.6\t4.7\n"
    "fortress\traked_gravel\t0.0\t9.9\n"
    "fortress\trice_bales\t1.3\t5.9\n"
    "fortress\tsake_set\t0.3\t4.1\n"
    "fortress\tshamisen\t1.0\t7.4\n"
    "fortress\tshuriken\t0.2\t6.1\n"
    "fortress\tsuzu_bell\t0.3\t6.7\n"
    "fortress\tsword_stand\t1.0\t6.3\n"
    "fortress\ttaiko\t0.6\t7.0\n"
    "fortress\ttorii_beam\t3.0\t9.2\n"
    "fortress\ttsukubai\t0.7\t5.9\n"
    "frontier\tbatwing_door\t1.0\t4.7\n"
    "frontier\tboots_and_spurs\t0.4\t6.2\n"
    "frontier\tbranding_irons\t1.0\t9.3\n"
    "frontier\tbusted_safe\t1.3\t5.8\n"
    "frontier\tbutter_churn\t0.75\t2.7\n"
    "frontier\tchurch_bell\t1.2\t6.0\n"
    "frontier\tgunbelt\t0.8\t8.0\n"
    "frontier\thay_bale\t0.9\t6.8\n"
    "frontier\thorse_collar\t0.75\t5.9\n"
    "frontier\tlasso\t0.9\t7.3\n"
    "frontier\tm_axe\t0.8\t7.6\n"
    "frontier\tm_barrel\t0.88\t6.9\n"
    "frontier\tm_barrel_upright\t0.88\t4.9\n"
    "frontier\tm_bottle_crate\t0.6\t5.7\n"
    "frontier\tm_cart\t2.5\t10.3\n"
    "frontier\tm_chest\t0.8\t4.8\n"
    "frontier\tm_claw_hammer\t0.33\t8.0\n"
    "frontier\tm_horseshoe\t0.12\t5.3\n"
    "frontier\tm_padlock\t0.08\t5.0\n"
    "frontier\tm_pickaxe\t0.9\t7.8\n"
    "frontier\tm_sheriff_star\t0.07\t4.5\n"
    "frontier\tm_shovel\t1.2\t8.6\n"
    "frontier\tm_signpost\t2.0\t10.4\n"
    "frontier\tm_wagon_wheel\t1.0\t7.5\n"
    "frontier\tm_whiskey_bottle\t0.3\t7.3\n"
    "frontier\tnail_keg\t0.45\t5.7\n"
    "frontier\tpoker_hand\t0.2\t5.5\n"
    "frontier\tpotbelly_stove\t0.9\t6.9\n"
    "frontier\trut_puddles\t0.0\t9.1\n"
    "frontier\tshop_sign\t1.2\t5.2\n"
    "frontier\tsingletree\t0.9\t8.4\n"
    "frontier\tspittoon\t0.3\t3.8\n"
    "frontier\twanted_board\t0.8\t4.6\n"
    "frontier\twashtub\t0.8\t6.1\n"
    "frontier\twhiskey_jug\t0.35\t6.2\n"
    "graveyard\tangel_wing\t0.8\t4.8\n"
    "graveyard\tbroken_obelisk\t2.0\t6.7\n"
    "graveyard\tcandles\t0.3\t3.3\n"
    "graveyard\tchalice_rosary\t0.45\t6.4\n"
    "graveyard\tdead_bat\t0.4\t5.5\n"
    "graveyard\teffigy_slab\t2.0\t6.6\n"
    "graveyard\tfallen_railing\t1.8\t5.7\n"
    "graveyard\tfootstones\t2.2\t8.4\n"
    "graveyard\tgargoyle_head\t0.8\t4.8\n"
    "graveyard\tgrave_bell\t1.0\t4.4\n"
    "graveyard\tivy\t0.0\t7.7\n"
    "graveyard\tm_altar\t1.5\t5.6\n"
    "graveyard\tm_bench\t1.5\t5.3\n"
    "graveyard\tm_candles\t0.3\t2.4\n"
    "graveyard\tm_chalice\t0.22\t2.7\n"
    "graveyard\tm_coffin\t2.0\t6.9\n"
    "graveyard\tm_coffin_old\t2.0\t6.3\n"
    "graveyard\tm_fire_basket\t0.55\t2.9\n"
    "graveyard\tm_grave\t2.2\t6.7\n"
    "graveyard\tm_gravestone_bevel\t0.75\t2.9\n"
    "graveyard\tm_gravestone_broken\t0.75\t3.0\n"
    "graveyard\tm_gravestone_cross\t1.2\t4.4\n"
    "graveyard\tm_gravestone_fancy\t0.7\t2.8\n"
    "graveyard\tm_gravestone_roof\t0.7\t2.6\n"
    "graveyard\tm_gravestone_round\t0.7\t2.7\n"
    "graveyard\tm_gravestone_wide\t1.05\t4.0\n"
    "graveyard\tm_lantern\t0.31\t2.5\n"
    "graveyard\tm_lantern_glass\t0.25\t2.0\n"
    "graveyard\tm_mallet\t0.35\t7.2\n"
    "graveyard\tm_obelisk\t1.9\t4.2\n"
    "graveyard\tm_pumpkin\t0.3\t2.0\n"
    "graveyard\tm_shovel\t1.2\t3.5\n"
    "graveyard\tm_skull\t0.22\t4.7\n"
    "graveyard\tm_stone_cross\t1.3\t5.1\n"
    "graveyard\tm_tomb\t2.2\t6.8\n"
    "graveyard\tm_trowel\t0.28\t6.9\n"
    "graveyard\tm_urn\t0.37\t2.4\n"
    "graveyard\tm_urn_square\t0.29\t1.9\n"
    "graveyard\tm_wood_cross\t1.2\t4.9\n"
    "graveyard\tmortsafe\t2.1\t6.9\n"
    "graveyard\topen_coffin\t2.0\t6.4\n"
    "graveyard\traven\t0.6\t6.0\n"
    "graveyard\trobbers_hole\t1.6\t7.6\n"
    "graveyard\tskull_pile\t0.55\t4.3\n"
    "graveyard\tsunken_grave\t2.1\t6.9\n"
    "graveyard\ttoppled_vase\t0.75\t5.5\n"
    "graveyard\twooden_cross\t0.9\t3.6\n"
    "graveyard\twreath\t0.6\t6.2\n"
    "highland\tbagpipes\t1.0\t5.7\n"
    "highland\tbeacon_pyre\t3.0\t7.4\n"
    "highland\tburial_kist\t1.5\t7.9\n"
    "highland\tcaber_and_stone\t3.0\t8.1\n"
    "highland\tcreel\t0.65\t6.7\n"
    "highland\tcrook_and_plaid\t1.7\t8.6\n"
    "highland\tcup_and_ring\t0.0\t6.7\n"
    "highland\tdrystone_run\t0.0\t8.1\n"
    "highland\tfallen_cross\t2.0\t5.4\n"
    "highland\theather_hummock\t0.0\t4.6\n"
    "highland\thurdle\t1.9\t6.0\n"
    "highland\tm_arrowhead\t0.04\t4.4\n"
    "highland\tm_boulder\t0.0\t7.6\n"
    "highland\tm_bowl\t0.2\t4.1\n"
    "highland\tm_bronze_sword\t0.65\t11.6\n"
    "highland\tm_campfire\t0.9\t7.2\n"
    "highland\tm_crag\t0.0\t6.0\n"
    "highland\tm_crag_b\t0.0\t6.3\n"
    "highland\tm_horn\t0.5\t8.5\n"
    "highland\tm_leaning_stone\t2\t6.1\n"
    "highland\tm_outcrop\t0.0\t8.6\n"
    "highland\tm_round_shield\t0.75\t7.9\n"
    "highland\tm_skull\t0.22\t4.8\n"
    "highland\tm_spearhead\t0.25\t6.6\n"
    "highland\tm_standing_stone\t2\t4.6\n"
    "highland\tm_standing_stone_b\t2\t4.2\n"
    "highland\tm_standing_stone_c\t2\t4.7\n"
    "highland\tm_standing_stone_d\t2\t4.6\n"
    "highland\tpeat_cutting\t0.0\t5.5\n"
    "highland\tpeat_stack\t1.0\t6.7\n"
    "highland\tpot_still\t1.4\t6.5\n"
    "highland\tquern\t0.45\t5.1\n"
    "highland\tram_skull\t0.5\t4.4\n"
    "highland\tsplit_boulder\t0.0\t3.9\n"
    "highland\ttarge\t0.55\t4.1\n"
    "highland\tthistles\t0.7\t3.9\n"
    "highland\ttussocks\t0.0\t4.0\n"
    "infestation\tacid_holes\t0.0\t6.7\n"
    "infestation\tburst_egg\t1.2\t6.2\n"
    "infestation\tclaw_marks\t0.0\t6.3\n"
    "infestation\tcocoon\t2.0\t6.7\n"
    "infestation\tdead_crawler\t1.1\t6.8\n"
    "infestation\tegg_ledge\t2.8\t8.9\n"
    "infestation\tflamethrower\t1.5\t8.3\n"
    "infestation\thive_pores\t0.0\t6.7\n"
    "infestation\thive_vent\t0.0\t6.2\n"
    "infestation\tlarvae\t0.8\t6.8\n"
    "infestation\tm_insect_leg\t1\t9.9\n"
    "infestation\tm_spine\t0.6\t10.4\n"
    "infestation\tmandibles\t0.8\t6.3\n"
    "infestation\tmolt\t1.6\t7.1\n"
    "infestation\tresin_stump\t0.0\t6.1\n"
    "infestation\tsac_cluster\t0.0\t6.4\n"
    "infestation\tscanner_and_mags\t0.5\t6.1\n"
    "infestation\tslime_trail\t0.0\t8.2\n"
    "infestation\tspine_row\t0.0\t8.3\n"
    "infestation\tspore_chimneys\t0.0\t5.7\n"
    "infestation\ttendril_knot\t0.0\t7.9\n"
    "infestation\tvictim_bones\t1.0\t7.1\n"
    "infestation\tweb_anchor\t0.0\t8.8\n"
    "maahl\tamphorae\t1.6\t7.3\n"
    "maahl\tarch_fragment\t1.8\t5.7\n"
    "maahl\tbeast_tracks\t0.0\t12.5\n"
    "maahl\tburied_head\t3.0\t7.0\n"
    "maahl\tcanopic_jars\t0.5\t3.9\n"
    "maahl\tdesert_rose\t0.0\t3.7\n"
    "maahl\tfallen_capital\t1.2\t4.2\n"
    "maahl\tfulgurite\t0.6\t6.5\n"
    "maahl\tgeode\t0.0\t4.6\n"
    "maahl\tm_amphora\t1\t6.1\n"
    "maahl\tm_crater\t0.0\t9.5\n"
    "maahl\tm_crystal_rock\t0.0\t8.3\n"
    "maahl\tm_crystals\t0.0\t7.8\n"
    "maahl\tm_dish\t0.25\t5.2\n"
    "maahl\tm_horn\t0.5\t7.6\n"
    "maahl\tm_insect_leg\t1\t10.2\n"
    "maahl\tm_meteor\t0.0\t7.9\n"
    "maahl\tm_rocks\t0.0\t9.8\n"
    "maahl\tm_sand_rock\t0.0\t6.4\n"
    "maahl\tm_sand_rock_b\t0.0\t7.5\n"
    "maahl\tm_sand_rock_c\t0.0\t7.1\n"
    "maahl\tm_shield\t0.8\t7.7\n"
    "maahl\tm_spearhead\t0.25\t8.0\n"
    "maahl\tm_stones\t0.0\t7.1\n"
    "maahl\tmosaic\t1.5\t5.5\n"
    "maahl\tobelisk_tip\t3.5\t9.0\n"
    "maahl\toil_lamp\t0.15\t5.2\n"
    "maahl\tsaddlebags\t1.3\t8.1\n"
    "maahl\tsalt_plates\t0.0\t6.2\n"
    "maahl\tscarab\t1.0\t5.4\n"
    "maahl\tsphinx_paw\t2.0\t6.7\n"
    "maahl\tspore_pods\t1\t4.9\n"
    "maahl\tstele\t1.8\t5.4\n"
    "maahl\tsundial\t1.2\t5.7\n"
    "maahl\twater_skins\t0.75\t5.2\n"
    "maahl\twell_bucket\t0.4\t8.7\n"
    "maahl\twind_ripples\t0.0\t6.6\n"
    "maahl\tworm_maw\t2.0\t7.5\n"
    "mine\tassay_scale\t0.5\t5.8\n"
    "mine\tcanary_cage\t0.25\t2.8\n"
    "mine\tclaim_stake\t1.2\t4.0\n"
    "mine\tdrill_steels\t1.3\t8.0\n"
    "mine\tdynamite\t0.45\t5.8\n"
    "mine\tfallen_stull\t2.0\t9.9\n"
    "mine\tgold_seam\t0.0\t8.4\n"
    "mine\thand_pump\t1.0\t5.8\n"
    "mine\tkibble\t0.7\t6.6\n"
    "mine\tlunch_pail\t0.22\t4.9\n"
    "mine\tm_barrel\t0.88\t3.7\n"
    "mine\tm_crystals\t0.0\t6.0\n"
    "mine\tm_gear\t0.35\t6.0\n"
    "mine\tm_gem\t0.02\t5.3\n"
    "mine\tm_hammer\t0.35\t4.4\n"
    "mine\tm_ingot\t0.3\t6.6\n"
    "mine\tm_ladder\t2.5\t6.6\n"
    "mine\tm_ore\t0.0\t4.6\n"
    "mine\tm_pickaxe\t0.9\t5.5\n"
    "mine\tm_rail\t1.0\t12.3\n"
    "mine\tm_rocks\t0.0\t5.2\n"
    "mine\tm_shovel\t1.2\t7.6\n"
    "mine\tm_spoil_heap\t0.0\t9.0\n"
    "mine\tm_wheel\t1.0\t3.8\n"
    "mine\tminer_helmet\t0.35\t4.2\n"
    "mine\toil_lantern\t0.45\t4.6\n"
    "mine\tplunger\t0.5\t6.3\n"
    "mine\tpulley_block\t0.5\t7.5\n"
    "mine\tsluice_box\t3\t6.7\n"
    "mine\tsorting_table\t1.5\t4.6\n"
    "mine\tspilled_ore\t1.4\t6.6\n"
    "mine\tstalagmites\t0.0\t4.3\n"
    "mine\tswitch_lever\t1.0\t4.8\n"
    "mine\twheelset\t0.75\t5.1\n"
    "obelis\tcapstone\t1.4\t3.7\n"
    "obelis\tcasket\t2.6\t6.0\n"
    "obelis\tconduit_run\t3.4\t8.0\n"
    "obelis\tfallen_monolith\t4.5\t9.5\n"
    "obelis\tfloor_hatch\t1.5\t3.7\n"
    "obelis\tglyph_cubes\t1.8\t4.3\n"
    "obelis\tglyph_marks\t0.0\t4.8\n"
    "obelis\tglyph_stele\t1.8\t4.5\n"
    "obelis\thalf_pylon\t1.8\t5.4\n"
    "obelis\tlens_pedestal\t1.0\t2.9\n"
    "obelis\tmetal_shards\t1.6\t3.6\n"
    "obelis\tobelisk_tip\t3.2\t8.3\n"
    "obelis\toffering_dais\t2.2\t4.2\n"
    "obelis\topen_casket\t2.8\t5.9\n"
    "obelis\tplinth_steps\t2.4\t5.5\n"
    "obelis\tpylon_ring\t3.5\t6.6\n"
    "obelis\tring_segment\t3.0\t6.9\n"
    "obelis\tsand_drift\t0.0\t4.7\n"
    "obelis\tstanding_stub\t2.0\t3.7\n"
    "obelis\tstar_dial\t2.4\t6.0\n"
    "obelis\tstone_chips\t0.0\t3.9\n"
    "obelis\ttoppled_pylon\t4.5\t10.4\n"
    "obelis\tvent_panel\t1.5\t4.0\n"
    "obelis\tvessel_row\t1.8\t5.6\n"
    "obelis\twind_ripples\t0.0\t4.4\n"
    "sundered\tashlar_blocks\t1.3\t5.3\n"
    "sundered\tbarricade_beams\t2.4\t7.5\n"
    "sundered\tcolumn_stump\t1.5\t4.6\n"
    "sundered\tdrum_row\t2.4\t7.3\n"
    "sundered\tdud_shell\t0.6\t5.2\n"
    "sundered\tempty_plinth\t1.4\t4.4\n"
    "sundered\tfallen_bell\t1.7\t5.9\n"
    "sundered\tfallen_capital\t1.1\t3.6\n"
    "sundered\tfallen_saint\t2\t9.3\n"
    "sundered\tfluted_drum\t1.4\t4.6\n"
    "sundered\tgothic_door\t2.4\t6.9\n"
    "sundered\theaved_flags\t2.4\t6.8\n"
    "sundered\tlancet_window\t2.5\t7.5\n"
    "sundered\tmasonry_heap\t2.0\t6.6\n"
    "sundered\tpinnacle\t2.0\t6.6\n"
    "sundered\trebar_slab\t2.4\t7.5\n"
    "sundered\trebar_tangle\t2.4\t7.7\n"
    "sundered\trose_window_shard\t1.6\t4.9\n"
    "sundered\tsandbag_line\t3.2\t8.7\n"
    "sundered\tshell_casings\t1.1\t5.1\n"
    "sundered\tshell_scar\t0.0\t10.5\n"
    "sundered\tspiked_railing\t2.6\t8.1\n"
    "sundered\tsplit_flags\t0.0\t7.3\n"
    "sundered\tstatue_hand\t1\t4.4\n"
    "sundered\tstatue_head\t1.4\t4.4\n"
    "sundered\ttoppled_pew\t2.6\t7.7\n"
    "sundered\ttracery_bits\t1.6\t5.2\n"
    "sundered\tvoussoirs\t2.4\t7.3\n"
    "swamp\tbone_fetish\t0.6\t6.1\n"
    "swamp\tcroc_skull\t0.6\t5.5\n"
    "swamp\tcypress_knees\t0.8\t4.5\n"
    "swamp\tdart_gourds\t0.5\t7.3\n"
    "swamp\tdugout\t4.0\t7.6\n"
    "swamp\tfish_trap\t0.9\t4.0\n"
    "swamp\tfrog_idol\t0.5\t4.3\n"
    "swamp\tgator_slide\t0.0\t7.8\n"
    "swamp\tlily_pool\t1.0\t5.9\n"
    "swamp\tm_bone\t0.4\t7.0\n"
    "swamp\tm_bone_knob\t0.3\t5.5\n"
    "swamp\tm_boot\t0.4\t6.4\n"
    "swamp\tm_canoe\t4.5\t11.0\n"
    "swamp\tm_dead_tree\t3.0\t10.6\n"
    "swamp\tm_fish\t0.6\t8.1\n"
    "swamp\tm_fish_bone\t0.5\t8.3\n"
    "swamp\tm_jug\t0.35\t5.6\n"
    "swamp\tm_log\t1.5\t9.5\n"
    "swamp\tm_mushroom\t0.1\t4.8\n"
    "swamp\tm_paddle\t1.5\t9.2\n"
    "swamp\tm_rowboat\t3.0\t7.7\n"
    "swamp\tm_skull\t0.22\t4.6\n"
    "swamp\tm_toadstool\t0.15\t5.0\n"
    "swamp\tm_tree_log\t2.0\t10.5\n"
    "swamp\tmat_and_bowl\t0.9\t4.8\n"
    "swamp\tmossy_log\t1.5\t6.3\n"
    "swamp\tmud_boil\t0.0\t4.7\n"
    "swamp\tpilings\t1.8\t6.3\n"
    "swamp\treed_bundle\t1.5\t6.4\n"
    "swamp\troot_tangle\t0.0\t7.6\n"
    "swamp\tshaman_drum\t0.6\t6.0\n"
    "swamp\tshed_skin\t3\t9.3\n"
    "swamp\tshelf_stump\t0.9\t5.2\n"
    "swamp\ttotem_stake\t1.8\t6.8\n"
    "swamp\tturtle_shell\t0.5\t4.2\n"
    "targa\tburied_steps\t2.0\t6.8\n"
    "targa\tconduit_node\t2.4\t11.6\n"
    "targa\tdisc_wreck\t1.5\t5.7\n"
    "targa\tfallen_icicles\t0.0\t6.7\n"
    "targa\tfishing_hole\t0.6\t10.1\n"
    "targa\tfrost_flowers\t0.0\t4.8\n"
    "targa\tglyph_tile\t0.8\t4.9\n"
    "targa\tice_blocks\t2.0\t6.6\n"
    "targa\tice_spikes\t0.0\t4.5\n"
    "targa\tlattice_shard\t0.0\t4.6\n"
    "targa\tm_bolt\t0.05\t5.6\n"
    "targa\tm_canisters\t0.8\t6.5\n"
    "targa\tm_cargo\t1.0\t5.5\n"
    "targa\tm_crate\t0.8\t8.0\n"
    "targa\tm_fan\t0.35\t6.4\n"
    "targa\tm_gas_bottle\t0.6\t5.9\n"
    "targa\tm_gear\t0.35\t5.7\n"
    "targa\tm_ice_crystals\t0.0\t8.2\n"
    "targa\tm_ice_shard\t0.0\t3.5\n"
    "targa\tm_lights\t1.5\t8.4\n"
    "targa\tm_skip\t3\t8.3\n"
    "targa\tm_snow_rocks\t0.0\t7.3\n"
    "targa\tm_spanner\t0.25\t8.8\n"
    "targa\tm_tank\t1.5\t8.0\n"
    "targa\tm_valve_wheel\t0.5\t6.7\n"
    "targa\tm_vent\t0.45\t8.0\n"
    "targa\tpack_and_snowshoes\t1.3\t6.2\n"
    "targa\tpod_lid\t2.0\t5.5\n"
    "targa\tpower_cell\t0.75\t5.3\n"
    "targa\tpressure_ridge\t0.0\t9.5\n"
    "targa\tpylon_stub\t1.5\t6.0\n"
    "targa\tskis\t1.9\t9.2\n"
    "targa\tsled\t2.8\t9.0\n"
    "targa\tsnow_crescent\t0.0\t9.8\n"
    "targa\tsupply_drop\t1.6\t6.9\n"
    "targa\tsurvey_beacon\t1.0\t5.3\n"
    "targa\tthermal_grille\t1.2\t6.4\n"
    "targa\ttusk\t2.0\t7.2\n"
    "temple\tblack_mirror\t0.45\t5.9\n"
    "temple\tcalendar_stone\t3\t5.5\n"
    "temple\tcenser\t0.7\t6.4\n"
    "temple\tfigurines\t0.6\t8.3\n"
    "temple\tglyph_lintel\t2.2\t9.4\n"
    "temple\tgong\t1.0\t6.5\n"
    "temple\tidol\t0.3\t2.5\n"
    "temple\tlotus_basin\t1.0\t4.5\n"
    "temple\tm_altar\t1.5\t5.6\n"
    "temple\tm_amphora\t1\t6.1\n"
    "temple\tm_blocks\t1.0\t4.4\n"
    "temple\tm_brazier\t0.55\t2.8\n"
    "temple\tm_column_stub\t0.7\t3.3\n"
    "temple\tm_dish\t0.25\t5.3\n"
    "temple\tm_goblet\t0.18\t5.0\n"
    "temple\tm_gold_bar\t0.25\t6.3\n"
    "temple\tm_gold_chest\t0.8\t4.8\n"
    "temple\tm_jade_gem\t0.05\t4.9\n"
    "temple\tm_obelisk\t1.9\t4.3\n"
    "temple\tm_pillar_fallen\t2.5\t5.4\n"
    "temple\tm_pot\t0.45\t3.5\n"
    "temple\tm_shield\t0.8\t7.7\n"
    "temple\tm_spearhead\t0.25\t7.6\n"
    "temple\tm_statue\t1.8\t2.7\n"
    "temple\tm_steps\t1.2\t4.0\n"
    "temple\tm_urn\t0.27\t2.1\n"
    "temple\tmeander_relief\t1.2\t7.3\n"
    "temple\toffering_bowl\t0.4\t3.8\n"
    "temple\tprayer_beads\t0.35\t5.6\n"
    "temple\tprayer_wheel\t0.35\t6.6\n"
    "temple\tritual_dagger\t0.35\t6.5\n"
    "temple\tserpent_drum\t0.6\t5.8\n"
    "temple\tsinging_bowl\t0.25\t5.0\n"
    "temple\tsun_inlay\t0.0\t6.7\n"
    "temple\tsun_mask\t0.55\t6.1\n"
    "temple\tsun_staff\t2.0\t11.6\n"
    "temple\tvotive_tablets\t0.6\t6.4\n"
    "temple\twater_clock\t1.1\t6.2\n"
    "thornwild\tboot_and_bones\t0.8\t5.1\n"
    "thornwild\tburst_pod\t1.8\t7.6\n"
    "thornwild\tbuttress_root\t5.0\t10.1\n"
    "thornwild\tdrag_gouges\t0.0\t6.0\n"
    "thornwild\tdropped_pack\t0.7\t5.7\n"
    "thornwild\tfield_lantern\t0.35\t3.8\n"
    "thornwild\tglow_stalks\t1.2\t6.0\n"
    "thornwild\tglowcap_ring\t1.6\t7.1\n"
    "thornwild\tgrasping_tendrils\t1.8\t5.9\n"
    "thornwild\tlost_helmet\t0.3\t3.8\n"
    "thornwild\tmachete\t0.7\t3.7\n"
    "thornwild\tmaw_flower\t2.4\t7.0\n"
    "thornwild\tovergrown_rifle\t1.2\t5.9\n"
    "thornwild\tpitcher_cluster\t1.3\t4.7\n"
    "thornwild\troot_knot\t3.0\t7.2\n"
    "thornwild\trooted_ribcage\t1.6\t6.0\n"
    "thornwild\tsap_seep\t0.0\t4.1\n"
    "thornwild\tseed_burrs\t0.5\t3.5\n"
    "thornwild\tshelf_fungus_root\t2.0\t6.5\n"
    "thornwild\tsnap_traps\t0.5\t4.6\n"
    "thornwild\tspore_pods\t1\t6.3\n"
    "thornwild\tspore_stalks\t1.0\t4.4\n"
    "thornwild\tstrangler_coil\t1.4\t5.4\n"
    "thornwild\tsundew_rosette\t0.3\t5.5\n"
    "thornwild\tthorn_bramble\t1.8\t6.0\n"
    "thornwild\tthorn_vine\t2.5\t8.6\n"
    "thornwild\twarning_stake\t1.8\t5.2\n"
    "trederra\tammo_box\t0.47\t7.5\n"
    "trederra\tbinoculars_map\t0.7\t5.6\n"
    "trederra\tboot_puddles\t0.0\t8.8\n"
    "trederra\tbugle\t0.29\t5.1\n"
    "trederra\tduckboard\t1.8\t8.5\n"
    "trederra\tdud_shell\t0.6\t6.5\n"
    "trederra\tfield_telephone\t0.25\t7.5\n"
    "trederra\tgas_mask\t0.3\t7.6\n"
    "trederra\tm_ammo_crate\t0.55\t9.1\n"
    "trederra\tm_barrel\t0.88\t5.4\n"
    "trederra\tm_bedroll_packed\t0.6\t5.8\n"
    "trederra\tm_boot\t0.4\t6.4\n"
    "trederra\tm_campfire\t0.9\t5.4\n"
    "trederra\tm_chest\t0.8\t4.4\n"
    "trederra\tm_duckboard\t1.97\t7.3\n"
    "trederra\tm_gas_cylinder\t0.6\t5.9\n"
    "trederra\tm_grenade\t0.1\t4.3\n"
    "trederra\tm_jerrycan\t0.45\t5.7\n"
    "trederra\tm_oil_drum\t0.9\t5.2\n"
    "trederra\tm_pickaxe\t0.9\t7.2\n"
    "trederra\tm_shell\t0.5\t6.9\n"
    "trederra\tm_shovel\t1.2\t8.4\n"
    "trederra\tm_sledgehammer\t0.85\t10.5\n"
    "trederra\tm_tin_sheet\t1.8\t7.0\n"
    "trederra\tmachine_gun\t1.3\t7.4\n"
    "trederra\tmortar\t1.2\t5.6\n"
    "trederra\tperiscope\t1.0\t9.1\n"
    "trederra\tpoppies\t0.6\t6.6\n"
    "trederra\tration_tins\t0.3\t6.0\n"
    "trederra\trats\t0.7\t8.9\n"
    "trederra\trifle\t1.1\t8.6\n"
    "trederra\trum_jar\t0.45\t5.3\n"
    "trederra\tsandbags\t1.6\t7.3\n"
    "trederra\tscrew_pickets\t1.5\t9.4\n"
    "trederra\tshell_cases\t0.65\t8.4\n"
    "trederra\tsniper_plate\t0.75\t5.4\n"
    "trederra\tstick_grenades\t0.45\t7.5\n"
    "trederra\tstretcher\t2.3\t8.4\n"
    "trederra\ttin_hat\t0.31\t5.5\n"
    "trederra\ttrench_sign\t1.2\t5.8\n"
    "trederra\twire_coil\t1.0\t8.0\n"
    "urban\tbathtub\t1.9\t6.0\n"
    "urban\tbottles\t0.6\t6.5\n"
    "urban\tbrick_pallet\t1.2\t5.8\n"
    "urban\tchalk_outline\t1.8\t6.4\n"
    "urban\tcrossarm\t2.4\t5.7\n"
    "urban\tdustbin\t1\t6.7\n"
    "urban\thubcap_and_bumper\t1.3\t6.0\n"
    "urban\thydrant\t0.75\t3.4\n"
    "urban\tkerbstones\t2.0\t6.7\n"
    "urban\tm_axle\t1.5\t8.9\n"
    "urban\tm_bat\t0.85\t11.4\n"
    "urban\tm_bench\t1.8\t6.2\n"
    "urban\tm_bottle\t0.3\t7.0\n"
    "urban\tm_cinder_block\t0.4\t5.9\n"
    "urban\tm_concrete_barrier\t2.5\t8.4\n"
    "urban\tm_cone\t0.71\t3.3\n"
    "urban\tm_drain_grate\t0.45\t7.0\n"
    "urban\tm_dumpster\t1.8\t7.9\n"
    "urban\tm_dumpster_ro\t1.8\t8.3\n"
    "urban\tm_gas_bottle\t0.6\t5.4\n"
    "urban\tm_hacksaw\t0.4\t8.6\n"
    "urban\tm_hydrant\t0.8\t2.7\n"
    "urban\tm_jerrycan\t0.45\t5.3\n"
    "urban\tm_spanner\t0.25\t8.5\n"
    "urban\tm_stop_sign\t2.6\t9.8\n"
    "urban\tm_street_light\t5.0\t11.1\n"
    "urban\tm_tank\t2.0\t9.0\n"
    "urban\tm_tyre\t0.65\t4.6\n"
    "urban\tpark_bench\t1.8\t5.4\n"
    "urban\tpost_box\t1.63\t7.4\n"
    "urban\tradiator\t1.1\t6.8\n"
    "urban\trebar_chunk\t0.0\t5.7\n"
    "urban\troad_sign\t2.2\t8.2\n"
    "urban\tsetts\t0.85\t5.8\n"
    "urban\tstorm_drain\t1.0\t4.9\n"
    "urban\tstreet_lamp\t4.5\t13.5\n"
    "urban\tsuitcase\t0.7\t4.8\n"
    "urban\ttyres\t0.76\t4.7\n"
    "urban\twindow_frame\t2.0\t6.9\n"
/* IMPORT 2026-11-20 BEGIN: work\import\apply.py */
    "graveyard\tm_round_headstone\t1.0\t6.0\n"
    "frontier\tm_round_headstone\t1.0\t6.0\n"
    "graveyard\tm_ring_cross\t1.4\t8.4\n"
    "highland\tm_ring_cross\t1.4\t8.4\n"
    "graveyard\tm_tapered_obelisk\t2.2\t13.2\n"
    "obelis\tm_tapered_obelisk\t2.2\t13.2\n"
    "temple\tm_tapered_obelisk\t2.2\t13.2\n"
    "graveyard\tm_broken_column\t1.5\t9.0\n"
    "temple\tm_broken_column\t1.5\t9.0\n"
    "sundered\tm_broken_column\t1.5\t9.0\n"
    "graveyard\tm_gothic_headstone\t1.2\t7.2\n"
    "sundered\tm_gothic_headstone\t1.2\t7.2\n"
    "graveyard\tm_ledger_slab\t2.0\t12.0\n"
    "dungeon\tm_ledger_slab\t2.0\t12.0\n"
    "graveyard\tm_pillow_marker\t0.9\t5.4\n"
    "graveyard\tm_weeping_angel\t1.81\t10.86\n"
    "sundered\tm_weeping_angel\t1.81\t10.86\n"
    "graveyard\tm_draped_urn\t1.5\t9.0\n"
    "temple\tm_draped_urn\t1.5\t9.0\n"
    "graveyard\tm_chest_tomb\t2.0\t12.0\n"
    "temple\tm_chest_tomb\t2.0\t12.0\n"
    "dungeon\tm_chest_tomb\t2.0\t12.0\n"
    "graveyard\tm_leaning_slab\t1.12\t6.72\n"
    "swamp\tm_leaning_slab\t1.12\t6.72\n"
    "graveyard\tm_boulder_memorial\t1.0\t6.0\n"
    "highland\tm_boulder_memorial\t1.0\t6.0\n"
    "graveyard\tm_plank_cross\t1.2\t7.2\n"
    "frontier\tm_plank_cross\t1.2\t7.2\n"
    "badlands\tm_plank_cross\t1.2\t7.2\n"
    "graveyard\tm_iron_cross\t1.2\t7.2\n"
    "frontier\tm_iron_cross\t1.2\t7.2\n"
    "trederra\tm_iron_cross\t1.2\t7.2\n"
    "graveyard\tm_stump_marker\t1.0\t6.0\n"
    "forest\tm_stump_marker\t1.0\t6.0\n"
    "graveyard\tm_scroll_headstone\t1.0\t6.0\n"
    "graveyard\tm_stepped_cross\t1.6\t9.6\n"
    "sundered\tm_stepped_cross\t1.6\t9.6\n"
    "graveyard\tm_double_headstone\t1.4\t8.4\n"
    "graveyard\tm_bordered_slate\t1.1\t6.6\n"
    "graveyard\tm_deaths_head_stone\t0.85\t7.2\n"
    "sundered\tm_deaths_head_stone\t0.85\t7.2\n"
    "graveyard\tm_sarcophagus\t2.2\t13.2\n"
    "temple\tm_sarcophagus\t2.2\t13.2\n"
    "dungeon\tm_sarcophagus\t2.2\t13.2\n"
    "obelis\tm_sarcophagus\t2.2\t13.2\n"
    "graveyard\tm_toppled_stone\t1.8\t10.8\n"
    "sundered\tm_toppled_stone\t1.8\t10.8\n"
    "graveyard\tm_human_skull\t0.22\t1.32\n"
    "dungeon\tm_human_skull\t0.22\t1.32\n"
    "swamp\tm_human_skull\t0.22\t1.32\n"
    "badlands\tm_human_skull\t0.22\t1.32\n"
    "blight\tm_human_skull\t0.22\t1.32\n"
    "ashveil\tm_human_skull\t0.22\t1.32\n"
    "trederra\tm_human_skull\t0.22\t1.32\n"
    "sundered\tm_human_skull\t0.22\t1.32\n"
    "thornwild\tm_human_skull\t0.22\t1.32\n"
    "urban\tm_human_skull\t0.22\t1.32\n"
    "graveyard\tm_cranium\t0.2\t1.2\n"
    "dungeon\tm_cranium\t0.2\t1.2\n"
    "blight\tm_cranium\t0.2\t1.2\n"
    "dungeon\tm_cracked_skull\t0.22\t1.32\n"
    "badlands\tm_cracked_skull\t0.22\t1.32\n"
    "urban\tm_cracked_skull\t0.22\t1.32\n"
    "ashveil\tm_cracked_skull\t0.22\t1.32\n"
    "dungeon\tm_giant_skull\t0.5\t3.0\n"
    "badlands\tm_giant_skull\t0.5\t3.0\n"
    "maahl\tm_giant_skull\t0.5\t3.0\n"
    "temple\tm_demon_skull\t0.6\t3.6\n"
    "dungeon\tm_demon_skull\t0.6\t3.6\n"
    "blight\tm_demon_skull\t0.6\t3.6\n"
    "cynder\tm_demon_skull\t0.6\t3.6\n"
    "dungeon\tm_brute_skull\t0.35\t2.1\n"
    "badlands\tm_brute_skull\t0.35\t2.1\n"
    "blight\tm_brute_skull\t0.35\t2.1\n"
    "temple\tm_cyclops_skull\t0.5\t3.0\n"
    "coast\tm_cyclops_skull\t0.5\t3.0\n"
    "badlands\tm_cyclops_skull\t0.5\t3.0\n"
    "temple\tm_carved_skull\t0.22\t1.32\n"
    "swamp\tm_carved_skull\t0.22\t1.32\n"
    "thornwild\tm_carved_skull\t0.22\t1.32\n"
    "forest\tm_wolf_skull\t0.25\t1.5\n"
    "highland\tm_wolf_skull\t0.25\t1.5\n"
    "forest\tm_bear_skull\t0.4\t2.4\n"
    "highland\tm_bear_skull\t0.4\t2.4\n"
    "mine\tm_bear_skull\t0.4\t2.4\n"
    "frontier\tm_horse_skull\t0.55\t3.3\n"
    "badlands\tm_horse_skull\t0.55\t3.3\n"
    "maahl\tm_horse_skull\t0.55\t3.3\n"
    "frontier\tm_longhorn_skull\t1.2\t7.2\n"
    "badlands\tm_longhorn_skull\t1.2\t7.2\n"
    "highland\tm_ram_skull\t0.4\t2.4\n"
    "maahl\tm_ram_skull\t0.4\t2.4\n"
    "forest\tm_stag_skull\t0.9\t5.4\n"
    "highland\tm_stag_skull\t0.9\t5.4\n"
    "forest\tm_boar_skull\t0.35\t2.1\n"
    "swamp\tm_boar_skull\t0.35\t2.1\n"
    "badlands\tm_mammoth_skull\t1.2\t7.2\n"
    "highland\tm_mammoth_skull\t1.2\t7.2\n"
    "maahl\tm_mammoth_skull\t1.2\t7.2\n"
    "swamp\tm_serpent_skull\t0.4\t6.0\n"
    "thornwild\tm_serpent_skull\t0.4\t6.0\n"
    "temple\tm_serpent_skull\t0.4\t6.0\n"
    "badlands\tm_predator_skull\t1.5\t9.0\n"
    "thornwild\tm_predator_skull\t1.5\t9.0\n"
    "maahl\tm_predator_skull\t1.5\t9.0\n"
    "badlands\tm_frilled_skull\t2.0\t12.0\n"
    "thornwild\tm_frilled_skull\t2.0\t12.0\n"
    "dungeon\tm_dragon_skull\t1.8\t10.8\n"
    "highland\tm_dragon_skull\t1.8\t10.8\n"
    "cynder\tm_dragon_skull\t1.8\t10.8\n"
    "maahl\tm_dragon_skull\t1.8\t10.8\n"
    "ashveil\tm_dragon_skull\t1.8\t10.8\n"
    "badlands\tm_dragon_skull\t1.8\t10.8\n"
    "graveyard\tm_femur\t0.45\t2.7\n"
    "dungeon\tm_femur\t0.45\t2.7\n"
    "swamp\tm_femur\t0.45\t2.7\n"
    "badlands\tm_femur\t0.45\t2.7\n"
    "blight\tm_femur\t0.45\t2.7\n"
    "ashveil\tm_femur\t0.45\t2.7\n"
    "trederra\tm_femur\t0.45\t2.7\n"
    "sundered\tm_femur\t0.45\t2.7\n"
    "thornwild\tm_femur\t0.45\t2.7\n"
    "beast\tm_femur\t0.45\t2.7\n"
    "cynder\tm_femur\t0.45\t2.7\n"
    "badlands\tm_beast_ribcage\t3.0\t18.0\n"
    "ashveil\tm_beast_ribcage\t3.0\t18.0\n"
    "blight\tm_beast_ribcage\t3.0\t18.0\n"
    "infestation\tm_beast_ribcage\t3.0\t18.0\n"
    "beast\tm_beast_ribcage\t3.0\t18.0\n"
    "badlands\tm_vertebrae\t0.5\t3.0\n"
    "blight\tm_vertebrae\t0.5\t3.0\n"
    "infestation\tm_vertebrae\t0.5\t3.0\n"
    "badlands\tm_pelvis\t0.3\t1.8\n"
    "dungeon\tm_pelvis\t0.3\t1.8\n"
    "badlands\tm_jawbone\t0.3\t1.8\n"
    "dungeon\tm_jawbone\t0.3\t1.8\n"
    "coast\tm_jawbone\t0.3\t1.8\n"
    "graveyard\tm_crossed_bones\t0.5\t3.0\n"
    "dungeon\tm_crossed_bones\t0.5\t3.0\n"
    "coast\tm_crossed_bones\t0.5\t3.0\n"
    "dungeon\tm_bone_heap\t0.9\t6.0\n"
    "blight\tm_bone_heap\t0.9\t6.0\n"
    "infestation\tm_bone_heap\t0.9\t6.0\n"
    "ashveil\tm_bone_heap\t0.9\t6.0\n"
    "beast\tm_bone_heap\t0.9\t6.0\n"
    "dungeon\tm_skull_bone_heap\t1.0\t6.0\n"
    "graveyard\tm_skull_bone_heap\t1.0\t6.0\n"
    "blight\tm_skull_bone_heap\t1.0\t6.0\n"
    "badlands\tm_mammoth_tusk\t2.5\t15.0\n"
    "highland\tm_mammoth_tusk\t2.5\t15.0\n"
    "maahl\tm_mammoth_tusk\t2.5\t15.0\n"
    "forest\tm_shed_antler\t0.7\t4.2\n"
    "highland\tm_shed_antler\t0.7\t4.2\n"
    "coast\tm_ammonite\t0.2\t3.0\n"
    "badlands\tm_ammonite\t0.2\t3.0\n"
    "mine\tm_ammonite\t0.2\t3.0\n"
    "coast\tm_cut_ammonite\t0.35\t3.0\n"
    "badlands\tm_cut_ammonite\t0.35\t3.0\n"
    "mine\tm_cut_ammonite\t0.35\t3.0\n"
    "coast\tm_ammonite_cluster\t0.5\t3.0\n"
    "badlands\tm_ammonite_cluster\t0.5\t3.0\n"
    "coast\tm_trilobite\t0.4\t2.4\n"
    "badlands\tm_trilobite\t0.4\t2.4\n"
    "obelis\tm_trilobite\t0.4\t2.4\n"
    "infestation\tm_trilobite\t0.4\t2.4\n"
    "badlands\tm_petrified_log\t0.8\t4.8\n"
    "maahl\tm_petrified_log\t0.8\t4.8\n"
    "badlands\tm_fossil_fish\t0.3\t3.6\n"
    "coast\tm_fossil_fish\t0.3\t3.6\n"
    "badlands\tm_fossil_fern\t0.6\t3.6\n"
    "maahl\tm_fossil_fern\t0.6\t3.6\n"
    "coast\tm_sea_beast_jaw\t1.0\t6.0\n"
    "badlands\tm_sea_beast_jaw\t1.0\t6.0\n"
    "coast\tm_turtle_shell\t1.2\t7.2\n"
    "swamp\tm_turtle_shell\t1.2\t7.2\n"
    "highland\tm_great_maul\t1.4\t8.4\n"
    "badlands\tm_great_maul\t1.4\t8.4\n"
    "highland\tm_war_axe\t1.0\t6.0\n"
    "dungeon\tm_war_axe\t1.0\t6.0\n"
    "dungeon\tm_great_hammer\t1.1\t6.6\n"
    "highland\tm_great_hammer\t1.1\t6.6\n"
    "badlands\tm_bone_club\t1.0\t6.0\n"
    "swamp\tm_bone_club\t1.0\t6.0\n"
    "dungeon\tm_bone_club\t1.0\t6.0\n"
    "badlands\tm_brute_cleaver\t1.2\t7.2\n"
    "blight\tm_brute_cleaver\t1.2\t7.2\n"
    "dungeon\tm_brute_cleaver\t1.2\t7.2\n"
    "highland\tm_double_bit_axe\t0.95\t7.8\n"
    "dungeon\tm_double_bit_axe\t0.95\t7.8\n"
    "highland\tm_claymore\t1.4\t8.4\n"
    "graveyard\tm_claymore\t1.4\t8.4\n"
    "dungeon\tm_iron_bound_chest\t1.1\t4.8\n"
    "coast\tm_iron_bound_chest\t1.1\t4.8\n"
    "dungeon\tm_block_rubble\t1.0\t6.0\n"
    "temple\tm_block_rubble\t1.0\t6.0\n"
    "sundered\tm_block_rubble\t1.0\t6.0\n"
    "urban\tm_block_rubble\t1.0\t6.0\n"
    "fortress\tm_block_rubble\t1.0\t6.0\n"
    "dungeon\tm_plain_skull\t0.18\t1.08\n"
    "graveyard\tm_plain_skull\t0.18\t1.08\n"
    "temple\tm_square_pillar\t2.4\t14.4\n"
    "dungeon\tm_square_pillar\t2.4\t14.4\n"
    "temple\tm_tripod_brazier\t1.2\t7.2\n"
    "dungeon\tm_tripod_brazier\t1.2\t7.2\n"
    "frontier\tm_feed_trough\t1.2\t7.2\n"
    "frontier\tm_hay_bale_b\t0.81\t4.86\n"
    "highland\tm_hay_bale\t0.81\t4.86\n"
    "frontier\tm_haystack\t1.8\t7.8\n"
    "highland\tm_haystack\t1.8\t7.8\n"
    "frontier\tm_scarecrow\t1.9\t11.4\n"
    "graveyard\tm_scarecrow\t1.9\t11.4\n"
    "coast\tm_small_cask\t0.43\t2.58\n"
    "dungeon\tm_small_cask\t0.43\t2.58\n"
    "frontier\tm_small_cask\t0.43\t2.58\n"
    "frontier\tm_grain_sack\t0.75\t3.12\n"
    "fortress\tm_grain_sack\t0.75\t3.12\n"
    "temple\tm_stone_altar\t1.4\t8.4\n"
    "dungeon\tm_stone_altar\t1.4\t8.4\n"
    "mine\tm_anvil\t0.72\t4.32\n"
    "swamp\tm_cauldron\t0.68\t4.08\n"
    "dungeon\tm_cauldron\t0.68\t4.08\n"
    "dungeon\tm_plank_chest\t0.84\t5.04\n"
    "coast\tm_plank_chest\t0.84\t5.04\n"
    "forest\tm_woodpile\t1.0\t6.0\n"
    "frontier\tm_woodpile\t1.0\t6.0\n"
    "highland\tm_woodpile\t1.0\t6.0\n"
    "highland\tm_round_boulder\t1.5\t9.0\n"
    "forest\tm_round_boulder\t1.5\t9.0\n"
    "swamp\tm_round_boulder\t1.5\t9.0\n"
    "coast\tm_round_boulder\t1.5\t9.0\n"
    "badlands\tm_round_boulder\t1.5\t9.0\n"
    "mine\tm_round_boulder\t1.5\t9.0\n"
    "frontier\tm_round_boulder\t1.5\t9.0\n"
    "cynder\tm_round_boulder\t1.5\t9.0\n"
    "forest\tm_round_bush\t1.15\t6.9\n"
    "highland\tm_round_bush\t1.15\t6.9\n"
    "swamp\tm_round_bush\t1.15\t6.9\n"
    "forest\tm_branchy_log\t2.6\t15.6\n"
    "swamp\tm_branchy_log\t2.6\t15.6\n"
    "highland\tm_branchy_log\t2.6\t15.6\n"
    "forest\tm_mushroom_cluster\t0.35\t2.88\n"
    "swamp\tm_mushroom_cluster\t0.35\t2.88\n"
    "thornwild\tm_mushroom_cluster\t0.35\t2.88\n"
    "forest\tm_pebble_scatter\t0.6\t6.6\n"
    "highland\tm_pebble_scatter\t0.6\t6.6\n"
    "coast\tm_pebble_scatter\t0.6\t6.6\n"
    "badlands\tm_pebble_scatter\t0.6\t6.6\n"
    "mine\tm_pebble_scatter\t0.6\t6.6\n"
    "swamp\tm_pebble_scatter\t0.6\t6.6\n"
    "frontier\tm_pebble_scatter\t0.6\t6.6\n"
    "maahl\tm_pebble_scatter\t0.6\t6.6\n"
    "forest\tm_mossy_rock\t1.3\t7.8\n"
    "swamp\tm_mossy_rock\t1.3\t7.8\n"
    "highland\tm_mossy_rock\t1.3\t7.8\n"
    "highland\tm_stacked_rocks\t1.1\t6.6\n"
    "forest\tm_stacked_rocks\t1.1\t6.6\n"
    "coast\tm_stacked_rocks\t1.1\t6.6\n"
    "badlands\tm_stacked_rocks\t1.1\t6.6\n"
    "mine\tm_stacked_rocks\t1.1\t6.6\n"
    "frontier\tm_stacked_rocks\t1.1\t6.6\n"
    "highland\tm_flat_slab\t1.6\t9.6\n"
    "coast\tm_flat_slab\t1.6\t9.6\n"
    "badlands\tm_flat_slab\t1.6\t9.6\n"
    "swamp\tm_stepping_stones\t3.1\t18.6\n"
    "coast\tm_stepping_stones\t3.1\t18.6\n"
    "swamp\tm_stream_rock\t1.5\t9.0\n"
    "coast\tm_stream_rock\t1.5\t9.0\n"
    "forest\tm_root_stump\t1.6\t9.6\n"
    "swamp\tm_root_stump\t1.6\t9.6\n"
    "graveyard\tm_root_stump\t1.6\t9.6\n"
    "forest\tm_pine_sapling\t1.4\t8.4\n"
    "highland\tm_pine_sapling\t1.4\t8.4\n"
    "frontier\tm_stave_barrel\t0.78\t4.68\n"
    "dungeon\tm_stave_barrel\t0.78\t4.68\n"
    "coast\tm_stave_barrel\t0.78\t4.68\n"
    "mine\tm_stave_barrel\t0.78\t4.68\n"
    "frontier\tm_cart_wheel\t1.15\t5.04\n"
    "mine\tm_cart_wheel\t1.15\t5.04\n"
    "frontier\tm_small_crate\t0.53\t3.18\n"
    "coast\tm_small_crate\t0.53\t3.18\n"
    "dungeon\tm_small_crate\t0.53\t3.18\n"
    "frontier\tm_rail_fence\t2.5\t7.8\n"
    "highland\tm_rail_fence\t2.5\t7.8\n"
    "graveyard\tm_wooden_marker\t0.8\t4.8\n"
    "frontier\tm_wooden_marker\t0.8\t4.8\n"
    "coast\tm_rope_coil\t0.66\t3.96\n"
    "mine\tm_rope_coil\t0.66\t3.96\n"
    "frontier\tm_sack_b\t0.64\t3.84\n"
    "mine\tm_sack_b\t0.64\t3.84\n"
    "frontier\tm_fingerpost\t2.1\t12.6\n"
    "highland\tm_fingerpost\t2.1\t12.6\n"
    "graveyard\tm_plain_tombstone\t1.0\t6.0\n"
    "highland\tm_dry_stone_wall\t1.0\t6.0\n"
    "forest\tm_dry_stone_wall\t1.0\t6.0\n"
    "frontier\tm_plank_trough\t2\t7.8\n"
    "mine\tm_wheelbarrow\t1.5\t9.0\n"
    "frontier\tm_wheelbarrow\t1.5\t9.0\n"
    "urban\tm_timber_beam\t2.0\t12.0\n"
    "sundered\tm_timber_beam\t2.0\t12.0\n"
    "frontier\tm_timber_beam\t2.0\t12.0\n"
    "frontier\tm_chimney_stack\t3\t12.6\n"
    "highland\tm_chimney_stack\t3\t12.6\n"
    "forest\tm_log_fire\t0.55\t3.3\n"
    "frontier\tm_log_fire\t0.55\t3.3\n"
    "highland\tm_log_fire\t0.55\t3.3\n"
    "frontier\tm_roofed_well\t2.2\t13.2\n"
    "highland\tm_roofed_well\t2.2\t13.2\n"
    "highland\tm_targe\t0.65\t3.9\n"
    "highland\tm_plank_shield\t0.55\t3.3\n"
    "dungeon\tm_plank_shield\t0.55\t3.3\n"
    "dungeon\tm_buckler\t0.35\t2.1\n"
    "frontier\tm_hooped_barrel\t0.92\t5.52\n"
    "mine\tm_hooped_barrel\t0.92\t5.52\n"
    "badlands\tm_hooped_barrel\t0.92\t5.52\n"
    "frontier\tm_crate\t0.8\t6.0\n"
    "mine\tm_crate\t0.8\t6.0\n"
    "mine\tm_powder_keg\t0.34\t4.02\n"
    "frontier\tm_powder_keg\t0.34\t4.02\n"
    "frontier\tm_hay_block\t1.0\t6.0\n"
    "frontier\tm_water_trough\t1.6\t9.6\n"
    "frontier\tm_anvil_stump\t0.88\t5.28\n"
    "mine\tm_anvil_stump\t0.88\t5.28\n"
    "frontier\tm_spare_wheel\t1.0\t6.0\n"
    "badlands\tm_spare_wheel\t1.0\t6.0\n"
    "badlands\tm_desert_rocks\t1.7\t10.2\n"
    "maahl\tm_desert_rocks\t1.7\t10.2\n"
    "frontier\tm_desert_rocks\t1.7\t10.2\n"
    "badlands\tm_saguaro_b\t3\t11.4\n"
    "frontier\tm_saguaro\t3\t11.4\n"
    "badlands\tm_tall_saguaro\t3.1\t18.6\n"
    "frontier\tm_tall_saguaro\t3.1\t18.6\n"
    "badlands\tm_prickly_pear_b\t1.2\t5.28\n"
    "frontier\tm_prickly_pear\t1.2\t5.28\n"
    "mine\tm_ore_cart\t1.4\t8.4\n"
    "mine\tm_rail_track\t2.0\t12.0\n"
    "frontier\tm_hitching_post\t2.0\t12.0\n"
    "frontier\tm_fence_2m\t2.0\t12.0\n"
    "highland\tm_fence_2m\t2.0\t12.0\n"
    "frontier\tm_direction_sign\t2.4\t14.4\n"
    "badlands\tm_direction_sign\t2.4\t14.4\n"
    "mine\tm_barrow\t1.7\t10.2\n"
    "frontier\tm_barrow\t1.7\t10.2\n"
    "badlands\tm_cow_skull\t0.84\t5.04\n"
    "frontier\tm_cow_skull\t0.84\t5.04\n"
    "frontier\tm_town_well\t2.2\t13.2\n"
    "mine\tm_mine_entrance\t3.14\t18.84\n"
    "badlands\tm_mine_entrance\t3.14\t18.84\n"
    "urban\tm_fuel_drum\t0.9\t5.4\n"
    "ferrum\tm_fuel_drum\t0.9\t5.4\n"
    "trederra\tm_fuel_drum\t0.9\t5.4\n"
    "targa\tm_fuel_drum\t0.9\t5.4\n"
    "ashveil\tm_fuel_drum\t0.9\t5.4\n"
    "urban\tm_dented_drum\t0.9\t5.4\n"
    "ashveil\tm_dented_drum\t0.9\t5.4\n"
    "ferrum\tm_dented_drum\t0.9\t5.4\n"
    "trederra\tm_dented_drum\t0.9\t5.4\n"
    "ferrum\tm_drum_cluster\t1.5\t9.0\n"
    "urban\tm_drum_cluster\t1.5\t9.0\n"
    "corridor\tm_drum_cluster\t1.5\t9.0\n"
    "urban\tm_fuel_can\t0.45\t2.7\n"
    "trederra\tm_fuel_can\t0.45\t2.7\n"
    "targa\tm_fuel_can\t0.45\t2.7\n"
    "ashveil\tm_fuel_can\t0.45\t2.7\n"
    "mine\tm_ore_lump\t0.3\t2.82\n"
    "mine\tm_ore_scatter\t0.93\t5.58\n"
    "mine\tm_ingot_stack\t0.8\t4.8\n"
    "dungeon\tm_ingot_stack\t0.8\t4.8\n"
    "mine\tm_ingot_heap\t1.3\t7.8\n"
    "dungeon\tm_ingot_heap\t1.3\t7.8\n"
    "fortress\tm_cut_stone_stack\t1.6\t9.6\n"
    "urban\tm_cut_stone_stack\t1.6\t9.6\n"
    "sundered\tm_cut_stone_stack\t1.6\t9.6\n"
    "fortress\tm_small_stone_stack\t1.0\t6.0\n"
    "sundered\tm_small_stone_stack\t1.0\t6.0\n"
    "fortress\tm_cut_block\t0.75\t4.5\n"
    "sundered\tm_cut_block\t0.75\t4.5\n"
    "dungeon\tm_cut_block\t0.75\t4.5\n"
    "urban\tm_rubble_pile\t1.5\t9.0\n"
    "sundered\tm_rubble_pile\t1.5\t9.0\n"
    "dungeon\tm_rubble_pile\t1.5\t9.0\n"
    "temple\tm_rubble_pile\t1.5\t9.0\n"
    "mine\tm_rubble_pile\t1.5\t9.0\n"
    "urban\tm_small_rubble\t1.1\t6.6\n"
    "sundered\tm_small_rubble\t1.1\t6.6\n"
    "dungeon\tm_small_rubble\t1.1\t6.6\n"
    "temple\tm_small_rubble\t1.1\t6.6\n"
    "forest\tm_cut_log\t1.4\t8.4\n"
    "frontier\tm_cut_log\t1.4\t8.4\n"
    "highland\tm_cut_log\t1.4\t8.4\n"
    "forest\tm_log_pile\t1.7\t10.2\n"
    "frontier\tm_log_pile\t1.7\t10.2\n"
    "highland\tm_log_pile\t1.7\t10.2\n"
    "frontier\tm_plank_stack\t2.4\t10.2\n"
    "mine\tm_plank_stack\t2.4\t10.2\n"
    "frontier\tm_small_plank_stack\t2.4\t9.6\n"
    "mine\tm_small_plank_stack\t2.4\t9.6\n"
    "urban\tm_pallet\t1.2\t7.2\n"
    "ferrum\tm_pallet\t1.2\t7.2\n"
    "ferrum\tm_parts_heap\t1.7\t10.2\n"
    "targa\tm_parts_heap\t1.7\t10.2\n"
    "ashveil\tm_parts_heap\t1.7\t10.2\n"
    "urban\tm_parts_heap\t1.7\t10.2\n"
    "ferrum\tm_parts_crate\t1.5\t9.0\n"
    "corridor\tm_parts_crate\t1.5\t9.0\n"
    "ferrum\tm_gear_scatter\t1.2\t7.2\n"
    "targa\tm_gear_scatter\t1.2\t7.2\n"
    "corridor\tm_gear_scatter\t1.2\t7.2\n"
    "ferrum\tm_cog_wheel\t0.4\t2.4\n"
    "corridor\tm_cog_wheel\t0.4\t2.4\n"
    "mine\tm_smith_anvil\t0.8\t4.8\n"
    "frontier\tm_smith_anvil\t0.8\t4.8\n"
    "mine\tm_bucket\t0.35\t2.1\n"
    "frontier\tm_bucket\t0.35\t2.1\n"
    "coast\tm_bucket\t0.35\t2.1\n"
    "mine\tm_grindstone\t1.2\t7.2\n"
    "frontier\tm_grindstone\t1.2\t7.2\n"
    "mine\tm_lantern\t0.35\t2.1\n"
    "dungeon\tm_lantern\t0.35\t2.1\n"
    "frontier\tm_lantern\t0.35\t2.1\n"
    "mine\tm_pick\t1.0\t6.0\n"
    "mine\tm_rope_bundle\t1.0\t6.0\n"
    "coast\tm_rope_bundle\t1.0\t6.0\n"
    "mine\tm_coiled_rope\t0.8\t4.8\n"
    "coast\tm_coiled_rope\t0.8\t4.8\n"
    "mine\tm_spade\t1.2\t7.2\n"
    "graveyard\tm_spade\t1.2\t7.2\n"
    "trederra\tm_spade\t1.2\t7.2\n"
    "dungeon\tm_closed_book\t0.25\t1.5\n"
    "dungeon\tm_rolled_map\t0.5\t3.0\n"
    "highland\tm_battle_axe\t1.0\t6.0\n"
    "dungeon\tm_battle_axe\t1.0\t6.0\n"
    "highland\tm_hand_axe\t0.4\t6.0\n"
    "frontier\tm_hand_axe\t0.4\t6.0\n"
    "dungeon\tm_spiked_mace\t0.9\t5.4\n"
    "badlands\tm_spiked_mace\t0.9\t5.4\n"
    "badlands\tm_spiked_club\t0.9\t5.4\n"
    "swamp\tm_spiked_club\t0.9\t5.4\n"
    "blight\tm_spiked_club\t0.9\t5.4\n"
    "dungeon\tm_kite_shield\t0.9\t5.4\n"
    "highland\tm_kite_shield\t0.9\t5.4\n"
    "dungeon\tm_heater_shield\t0.9\t5.4\n"
    "graveyard\tm_heater_shield\t0.9\t5.4\n"
    "badlands\tm_spiked_shield\t1.0\t6.0\n"
    "blight\tm_spiked_shield\t1.0\t6.0\n"
    "dungeon\tm_spiked_shield\t1.0\t6.0\n"
    "graveyard\tm_broadsword\t1.0\t6.0\n"
    "highland\tm_broadsword\t1.0\t6.0\n"
    "graveyard\tm_longsword\t1.0\t6.0\n"
    "dungeon\tm_longsword\t1.0\t6.0\n"
    "urban\tm_steel_drum\t0.9\t5.4\n"
    "ferrum\tm_steel_drum\t0.9\t5.4\n"
    "trederra\tm_steel_drum\t0.9\t5.4\n"
    "targa\tm_steel_drum\t0.9\t5.4\n"
    "urban\tm_plastic_drum\t0.9\t5.4\n"
    "ashveil\tm_plastic_drum\t0.9\t5.4\n"
    "ferrum\tm_gas_cylinder\t0.7\t4.2\n"
    "corridor\tm_gas_cylinder_b\t0.7\t4.2\n"
    "urban\tm_gas_cylinder\t0.7\t4.2\n"
    "urban\tm_petrol_can\t0.45\t2.7\n"
    "trederra\tm_petrol_can\t0.45\t2.7\n"
    "ferrum\tm_power_cabinet\t1.2\t7.2\n"
    "corridor\tm_power_cabinet\t1.2\t7.2\n"
    "urban\tm_power_cabinet\t1.2\t7.2\n"
    "ferrum\tm_fuse_cabinet\t1.0\t6.0\n"
    "corridor\tm_fuse_cabinet\t1.0\t6.0\n"
    "ferrum\tm_cable_reel\t1.0\t6.0\n"
    "urban\tm_cable_reel\t1.0\t6.0\n"
    "ferrum\tm_generator\t0.9\t5.4\n"
    "urban\tm_generator\t0.9\t5.4\n"
    "targa\tm_generator\t0.9\t5.4\n"
    "corridor\tm_locker\t1.8\t10.8\n"
    "urban\tm_locker\t1.8\t10.8\n"
    "corridor\tm_extinguisher\t0.6\t3.6\n"
    "graveyard\tm_dead_tree_small\t2.5\t15.0\n"
    "swamp\tm_dead_tree_small\t2.5\t15.0\n"
    "blight\tm_dead_tree_small\t2.5\t15.0\n"
    "ashveil\tm_dead_tree_small\t2.5\t15.0\n"
    "badlands\tm_dead_tree_small\t2.5\t15.0\n"
    "graveyard\tm_dead_tree_forked\t3.0\t18.0\n"
    "swamp\tm_dead_tree_forked\t3.0\t18.0\n"
    "blight\tm_dead_tree_forked\t3.0\t18.0\n"
    "forest\tm_dead_tree_forked\t3.0\t18.0\n"
    "graveyard\tm_dead_tree_wide\t3.0\t18.0\n"
    "swamp\tm_dead_tree_wide\t3.0\t18.0\n"
    "ashveil\tm_dead_tree_wide\t3.0\t18.0\n"
    "highland\tm_boulder_group\t1.8\t10.8\n"
    "forest\tm_boulder_group\t1.8\t10.8\n"
    "coast\tm_boulder_group\t1.8\t10.8\n"
    "badlands\tm_boulder_group\t1.8\t10.8\n"
    "mine\tm_boulder_group\t1.8\t10.8\n"
    "swamp\tm_boulder_group\t1.8\t10.8\n"
    "cynder\tm_boulder_group\t1.8\t10.8\n"
    "blight\tm_boulder_group\t1.8\t10.8\n"
    "highland\tm_long_flat_rock\t1.0\t6.0\n"
    "coast\tm_long_flat_rock\t1.0\t6.0\n"
    "badlands\tm_long_flat_rock\t1.0\t6.0\n"
    "forest\tm_boulder_b\t0.8\t4.8\n"
    "highland\tm_boulder_b\t0.8\t4.8\n"
    "mine\tm_boulder_b\t0.8\t4.8\n"
    "swamp\tm_boulder_b\t0.8\t4.8\n"
    "coast\tm_boulder_b\t0.8\t4.8\n"
    "forest\tm_round_rock\t0.4\t2.4\n"
    "highland\tm_round_rock\t0.4\t2.4\n"
    "mine\tm_round_rock\t0.4\t2.4\n"
    "swamp\tm_round_rock\t0.4\t2.4\n"
    "coast\tm_round_rock\t0.4\t2.4\n"
    "badlands\tm_round_rock\t0.4\t2.4\n"
    "forest\tm_low_rock\t0.9\t5.4\n"
    "highland\tm_low_rock\t0.9\t5.4\n"
    "coast\tm_low_rock\t0.9\t5.4\n"
    "badlands\tm_low_rock\t0.9\t5.4\n"
    "mine\tm_low_rock\t0.9\t5.4\n"
    "forest\tm_slab_rock\t0.8\t4.8\n"
    "highland\tm_slab_rock\t0.8\t4.8\n"
    "coast\tm_slab_rock\t0.8\t4.8\n"
    "badlands\tm_slab_rock\t0.8\t4.8\n"
    "mine\tm_slab_rock\t0.8\t4.8\n"
    "dungeon\tm_oak_barrel\t0.93\t5.58\n"
    "mine\tm_oak_barrel\t0.93\t5.58\n"
    "coast\tm_oak_barrel\t0.93\t5.58\n"
    "dungeon\tm_treasure_chest\t0.9\t5.4\n"
    "coast\tm_treasure_chest\t0.9\t5.4\n"
    "dungeon\tm_plank_crate\t0.8\t4.8\n"
    "mine\tm_plank_crate\t0.8\t4.8\n"
    "frontier\tm_plank_crate\t0.8\t4.8\n"
    "dungeon\tm_open_crate\t0.8\t4.8\n"
    "mine\tm_open_crate\t0.8\t4.8\n"
    "dungeon\tm_broken_flags_01\t1.2\t7.2\n"
    "temple\tm_broken_flags_01\t1.2\t7.2\n"
    "sundered\tm_broken_flags_01\t1.2\t7.2\n"
    "dungeon\tm_broken_flags_02\t1.2\t7.2\n"
    "temple\tm_broken_flags_02\t1.2\t7.2\n"
    "sundered\tm_broken_flags_02\t1.2\t7.2\n"
    "dungeon\tm_broken_flags_03\t1.2\t7.2\n"
    "temple\tm_broken_flags_03\t1.2\t7.2\n"
    "sundered\tm_broken_flags_03\t1.2\t7.2\n"
    "dungeon\tm_broken_flags_04\t1.2\t7.2\n"
    "temple\tm_broken_flags_04\t1.2\t7.2\n"
    "sundered\tm_broken_flags_04\t1.2\t7.2\n"
    "dungeon\tm_broken_flags_05\t1.2\t7.2\n"
    "temple\tm_broken_flags_05\t1.2\t7.2\n"
    "sundered\tm_broken_flags_05\t1.2\t7.2\n"
    "dungeon\tm_broken_flags_06\t0.6\t3.6\n"
    "temple\tm_broken_flags_06\t0.6\t3.6\n"
    "sundered\tm_broken_flags_06\t0.6\t3.6\n"
    "dungeon\tm_broken_flags_07\t1.2\t7.2\n"
    "temple\tm_broken_flags_07\t1.2\t7.2\n"
    "sundered\tm_broken_flags_07\t1.2\t7.2\n"
    "dungeon\tm_broken_flags_08\t1.2\t7.2\n"
    "temple\tm_broken_flags_08\t1.2\t7.2\n"
    "sundered\tm_broken_flags_08\t1.2\t7.2\n"
    "dungeon\tm_broken_flags_09\t1.2\t7.2\n"
    "temple\tm_broken_flags_09\t1.2\t7.2\n"
    "sundered\tm_broken_flags_09\t1.2\t7.2\n"
    "dungeon\tm_broken_flags_10\t1.2\t7.2\n"
    "temple\tm_broken_flags_10\t1.2\t7.2\n"
    "sundered\tm_broken_flags_10\t1.2\t7.2\n"
    "dungeon\tm_broken_flags_11\t1.2\t7.2\n"
    "temple\tm_broken_flags_11\t1.2\t7.2\n"
    "sundered\tm_broken_flags_11\t1.2\t7.2\n"
    "dungeon\tm_broken_flags_12\t1.2\t7.2\n"
    "temple\tm_broken_flags_12\t1.2\t7.2\n"
    "sundered\tm_broken_flags_12\t1.2\t7.2\n"
    "dungeon\tm_broken_flags_13\t1.2\t7.2\n"
    "temple\tm_broken_flags_13\t1.2\t7.2\n"
    "sundered\tm_broken_flags_13\t1.2\t7.2\n"
    "dungeon\tm_loose_planks_01\t2.48\t14.88\n"
    "urban\tm_loose_planks_01\t2.48\t14.88\n"
    "mine\tm_loose_planks_01\t2.48\t14.88\n"
    "dungeon\tm_loose_planks_02\t1.75\t10.5\n"
    "urban\tm_loose_planks_02\t1.75\t10.5\n"
    "mine\tm_loose_planks_02\t1.75\t10.5\n"
    "dungeon\tm_loose_planks_03\t1.19\t7.14\n"
    "urban\tm_loose_planks_03\t1.19\t7.14\n"
    "mine\tm_loose_planks_03\t1.19\t7.14\n"
    "dungeon\tm_loose_planks_04\t1.0\t6.0\n"
    "urban\tm_loose_planks_04\t1.0\t6.0\n"
    "mine\tm_loose_planks_04\t1.0\t6.0\n"
    "dungeon\tm_loose_planks_05\t1.55\t9.3\n"
    "urban\tm_loose_planks_05\t1.55\t9.3\n"
    "mine\tm_loose_planks_05\t1.55\t9.3\n"
    "temple\tm_candle_dish\t0.2\t2.1\n"
    "dungeon\tm_candle_dish\t0.2\t2.1\n"
    "graveyard\tm_candle_dish\t0.2\t2.1\n"
    "temple\tm_stone_pillar\t2.4\t14.4\n"
    "dungeon\tm_stone_pillar\t2.4\t14.4\n"
    "badlands\tm_rock_arch\t3.0\t18.0\n"
    "maahl\tm_rock_arch\t3.0\t18.0\n"
    "coast\tm_rock_arch\t3.0\t18.0\n"
    "badlands\tm_rock_column\t2.5\t15.0\n"
    "maahl\tm_rock_column\t2.5\t15.0\n"
    "highland\tm_rock_column\t2.5\t15.0\n"
    "badlands\tm_rock_wall\t2.5\t15.0\n"
    "highland\tm_rock_wall\t2.5\t15.0\n"
    "coast\tm_rock_wall\t2.5\t15.0\n"
    "mine\tm_rock_wall\t2.5\t15.0\n"
    "badlands\tm_rock_wall_b\t2.5\t15.0\n"
    "highland\tm_rock_wall_b\t2.5\t15.0\n"
    "coast\tm_rock_wall_b\t2.5\t15.0\n"
    "mine\tm_rock_wall_b\t2.5\t15.0\n"
    "badlands\tm_rock_corner\t3.0\t18.0\n"
    "highland\tm_rock_corner\t3.0\t18.0\n"
    "mine\tm_rock_corner\t3.0\t18.0\n"
    "badlands\tm_rock_01\t1.6\t9.6\n"
    "highland\tm_rock_01\t1.6\t9.6\n"
    "coast\tm_rock_01\t1.6\t9.6\n"
    "mine\tm_rock_01\t1.6\t9.6\n"
    "forest\tm_rock_01\t1.6\t9.6\n"
    "cynder\tm_rock_01\t1.6\t9.6\n"
    "badlands\tm_rock_02\t1.6\t9.6\n"
    "highland\tm_rock_02\t1.6\t9.6\n"
    "coast\tm_rock_02\t1.6\t9.6\n"
    "mine\tm_rock_02\t1.6\t9.6\n"
    "forest\tm_rock_02\t1.6\t9.6\n"
    "cynder\tm_rock_02\t1.6\t9.6\n"
    "badlands\tm_rock_03\t2.0\t12.0\n"
    "highland\tm_rock_03\t2.0\t12.0\n"
    "maahl\tm_rock_03\t2.0\t12.0\n"
    "coast\tm_rock_03\t2.0\t12.0\n"
    "badlands\tm_rock_04\t1.8\t10.8\n"
    "highland\tm_rock_04\t1.8\t10.8\n"
    "coast\tm_rock_04\t1.8\t10.8\n"
    "mine\tm_rock_04\t1.8\t10.8\n"
    "forest\tm_rock_04\t1.8\t10.8\n"
    "cynder\tm_rock_04\t1.8\t10.8\n"
    "badlands\tm_rock_05\t1.7\t10.2\n"
    "highland\tm_rock_05\t1.7\t10.2\n"
    "coast\tm_rock_05\t1.7\t10.2\n"
    "mine\tm_rock_05\t1.7\t10.2\n"
    "forest\tm_rock_05\t1.7\t10.2\n"
    "cynder\tm_rock_05\t1.7\t10.2\n"
    "badlands\tm_rock_08\t1.5\t9.0\n"
    "highland\tm_rock_08\t1.5\t9.0\n"
    "coast\tm_rock_08\t1.5\t9.0\n"
    "mine\tm_rock_08\t1.5\t9.0\n"
    "forest\tm_rock_08\t1.5\t9.0\n"
    "cynder\tm_rock_08\t1.5\t9.0\n"
    "badlands\tm_rock_09\t1.4\t8.4\n"
    "highland\tm_rock_09\t1.4\t8.4\n"
    "coast\tm_rock_09\t1.4\t8.4\n"
    "mine\tm_rock_09\t1.4\t8.4\n"
    "forest\tm_rock_09\t1.4\t8.4\n"
    "cynder\tm_rock_09\t1.4\t8.4\n"
    "badlands\tm_scree\t2.5\t15.0\n"
    "highland\tm_scree\t2.5\t15.0\n"
    "mine\tm_scree\t2.5\t15.0\n"
    "cynder\tm_scree\t2.5\t15.0\n"
    "badlands\tm_scree_b\t2.5\t15.0\n"
    "highland\tm_scree_b\t2.5\t15.0\n"
    "mine\tm_scree_b\t2.5\t15.0\n"
    "cynder\tm_scree_b\t2.5\t15.0\n"
    "badlands\tm_pebble_group\t0.5\t9.0\n"
    "highland\tm_pebble_group\t0.5\t9.0\n"
    "coast\tm_pebble_group\t0.5\t9.0\n"
    "mine\tm_pebble_group\t0.5\t9.0\n"
    "maahl\tm_pebble_group\t0.5\t9.0\n"
    "badlands\tm_pebble_group_b\t0.5\t9.0\n"
    "highland\tm_pebble_group_b\t0.5\t9.0\n"
    "coast\tm_pebble_group_b\t0.5\t9.0\n"
    "mine\tm_pebble_group_b\t0.5\t9.0\n"
    "maahl\tm_pebble_group_b\t0.5\t9.0\n"
    "ashveil\tm_basalt_columns\t3.0\t18.0\n"
    "highland\tm_basalt_columns\t3.0\t18.0\n"
    "coast\tm_basalt_columns\t3.0\t18.0\n"
    "cynder\tm_basalt_columns\t3.0\t18.0\n"
    "highland\tm_cairn\t1.4\t8.4\n"
    "maahl\tm_cairn\t1.4\t8.4\n"
    "frontier\tm_cairn\t1.4\t8.4\n"
    "highland\tm_cairn_b\t1.3\t7.8\n"
    "maahl\tm_cairn_b\t1.3\t7.8\n"
    "frontier\tm_cairn_b\t1.3\t7.8\n"
    "highland\tm_cairn_c\t1.1\t6.6\n"
    "maahl\tm_cairn_c\t1.1\t6.6\n"
    "frontier\tm_cairn_c\t1.1\t6.6\n"
    "badlands\tm_angular_boulder\t1.6\t9.6\n"
    "highland\tm_angular_boulder\t1.6\t9.6\n"
    "forest\tm_angular_boulder\t1.6\t9.6\n"
    "mine\tm_angular_boulder\t1.6\t9.6\n"
    "badlands\tm_angular_boulder_b\t1.8\t10.8\n"
    "highland\tm_angular_boulder_b\t1.8\t10.8\n"
    "forest\tm_angular_boulder_b\t1.8\t10.8\n"
    "mine\tm_angular_boulder_b\t1.8\t10.8\n"
    "badlands\tm_bone_heap\t0.9\t4.2\n"
    "highland\tm_skull_human\t0.22\t1.32\n"
    "highland\tm_femur\t0.45\t2.7\n"
    "highland\tm_bone_heap\t0.9\t4.2\n"
    "maahl\tm_skull_human\t0.22\t1.32\n"
    "maahl\tm_femur\t0.45\t2.7\n"
    "maahl\tm_bone_heap\t0.9\t4.2\n"
    "corridor\tm_skull_human\t0.22\t1.32\n"
    "corridor\tm_femur\t0.45\t2.7\n"
    "corridor\tm_bone_heap\t0.9\t4.2\n"
    "infestation\tm_skull_human\t0.22\t1.32\n"
    "infestation\tm_femur\t0.45\t2.7\n"
    "urban\tm_femur\t0.45\t2.7\n"
    "urban\tm_bone_heap\t0.9\t4.2\n"
    "mine\tm_skull_human\t0.22\t1.32\n"
    "mine\tm_femur\t0.45\t2.7\n"
    "mine\tm_bone_heap\t0.9\t4.2\n"
    "swamp\tm_bone_heap\t0.9\t4.2\n"
    "cynder\tm_skull_human\t0.22\t1.32\n"
    "cynder\tm_bone_heap\t0.9\t4.2\n"
    "targa\tm_skull_human\t0.22\t1.32\n"
    "targa\tm_femur\t0.45\t2.7\n"
    "targa\tm_bone_heap\t0.9\t4.2\n"
    "trederra\tm_bone_heap\t0.9\t4.2\n"
    "fortress\tm_skull_human\t0.22\t1.32\n"
    "fortress\tm_femur\t0.45\t2.7\n"
    "fortress\tm_bone_heap\t0.9\t4.2\n"
    "beast\tm_skull_human\t0.22\t1.32\n"
    "frontier\tm_skull_human\t0.22\t1.32\n"
    "frontier\tm_femur\t0.45\t2.7\n"
    "frontier\tm_bone_heap\t0.9\t4.2\n"
    "graveyard\tm_bone_heap\t0.9\t4.2\n"
    "forest\tm_skull_human\t0.22\t1.32\n"
    "forest\tm_femur\t0.45\t2.7\n"
    "forest\tm_bone_heap\t0.9\t4.2\n"
    "temple\tm_skull_human\t0.22\t1.32\n"
    "temple\tm_femur\t0.45\t2.7\n"
    "temple\tm_bone_heap\t0.9\t4.2\n"
    "coast\tm_skull_human\t0.22\t1.32\n"
    "coast\tm_femur\t0.45\t2.7\n"
    "coast\tm_bone_heap\t0.9\t4.2\n"
    "sundered\tm_bone_heap\t0.9\t4.2\n"
    "ferrum\tm_skull_human\t0.22\t1.32\n"
    "ferrum\tm_femur\t0.45\t2.7\n"
    "ferrum\tm_bone_heap\t0.9\t4.2\n"
    "thornwild\tm_bone_heap\t0.9\t4.2\n"
    "obelis\tm_skull_human\t0.22\t1.32\n"
    "obelis\tm_femur\t0.45\t2.7\n"
    "obelis\tm_bone_heap\t0.9\t4.2\n"
/* IMPORT 2026-11-20 END */
;
/* @@SCALE_END@@ */
static int TABLES_SET;
int sdr_set_tables(const char *text) {
    const char *p = text ? text : SDR_BUILTIN; int n = 0;
    kv_clear(&T_REAL); kv_clear(&T_DRAWN); TABLES_SET = 1;
    while (p && *p) {
        char st[256], nm[256]; double r, d; const char *e = strchr(p, '\n');
        if (sscanf(p, "%255[^\t]\t%255[^\t]\t%lf\t%lf", st, nm, &r, &d) != 4) return SC_ERR;
        kv_put(&T_REAL, st, nm, r); kv_put(&T_DRAWN, st, nm, d); n++;
        p = e ? e + 1 : NULL;
    }
    return n;
}
void sdr_cache_clear(void) { kv_clear(&CACHE); }
void sdr_cache_put(const char *style, const char *name, double k) { kv_put(&CACHE, style, name, k); }
int sdr_cache_get(const char *style, const char *name, double *k) { return kv_get(&CACHE, style, name, k); }
void sdr_scale_clear(void) { kv_clear(&SCALE); HAS_STAR = 0; }
void sdr_scale_put(const char *style, const char *name, double k) { kv_put(&SCALE, style, name, k); }
void sdr_scale_star(int has, double k) { HAS_STAR = has; STAR = k; }

static double pmax(double a, double b) { return b > a ? b : a; }          /* max(a, b): the first of equal (and NaN) */
static double pmin(double a, double b) { return b < a ? b : a; }
/* real_factor(style, name, cls): with a scenery scale, the factor that brings the detail to its real size */
static int real_k(const char *style, const char *name, double *kout) {
    double sc, r = 0, dr = 0, D, k = 1.0;
    if (env_float(HAS_SCALE, ENV_SCALE, 1.0, &sc) < 0) return SC_ERR;
    if (sc == 1.0 && ft_obj_scale() > 0 && !(HAS_NOREAL && ENV_NOREAL[0])) {          /* object size: real size on the base itself */
        const char *bm = getenv("BASE_MM"); double o = ft_obj_scale(), D0 = (bm && *bm) ? strtod(bm, NULL) : 30.0;
        if (!TABLES_SET) sdr_set_tables(NULL);
        kv_get(&T_REAL, style, name, &r); kv_get(&T_DRAWN, style, name, &dr);
        if (r > 0 && dr > 0) {
            double want = pmax(r * 6.0 * o, 3.0);          /* 6 mm a metre at the classic scale: 18 at 3 (1:56); nothing prints under 3 mm */
            k = pmax(0.25, pmin(15.0, want / dr));          /* up to 15 times (was 6, 2026-11-21): a detail drawn small (a container, a cart) still reaches its real size */
            if (dr * k < 3.0) k = 0.0;          /* nothing under 3 mm: it does not print (by mm since 2026-11-21, was 3.5 % of the base: a 4 mm skull now stays on any size) */
        } else if (dr > 0) k = pmax(0.8, pmin(2.2, pow(D0 / 40.0, 0.6)));          /* ground (rocks, drifts): grows with the base */
        *kout = k; return 0;
    }
    if (sc != 1.0 && !(HAS_NOREAL && ENV_NOREAL[0])) {
        if (!TABLES_SET) sdr_set_tables(NULL);          /* _REAL["tables"]: loaded the first time they are needed (built in) */
        kv_get(&T_REAL, style, name, &r); kv_get(&T_DRAWN, style, name, &dr);
        if (env_float(HAS_FINAL, ENV_FINAL, 0.0, &D) < 0) return SC_ERR;
        if (r > 0 && dr > 0) {
            double want = r * 6.0;
            if (sc == 0.0) return SC_ERR;          /* 3.0 / sc: ZeroDivisionError */
            want = pmax(want, 3.0 / sc);
            k = pmax(0.25, pmin(4.0, want / dr));
            if (D != 0.0 && dr * k * sc < 0.08 * D) k = 0.0;          /* SHOW_SHARE */
        } else if (dr > 0 && D != 0.0) {
            double re, im; int t = px_pow(D / 40.0, 0.6, &re, &im);
            if (t != 0) return SC_ERR;          /* a complex (D < 0) cannot be compared: TypeError; or OverflowError */
            k = pmax(0.8, pmin(2.2, re));
        }
    }
    *kout = k;
    return 0;
}
int sdr_real_of(const char *style, const char *name, double *real_m, double *drawn_mm) {          /* the scale tables' entry (the Models preview); 0: none */
    double r = 0, d = 0; if (!TABLES_SET) sdr_set_tables(NULL);
    if (!kv_get(&T_REAL, style, name, &r)) return 0; kv_get(&T_DRAWN, style, name, &d); *real_m = r; *drawn_mm = d; return 1;
}
double sdr_real_factor(const char *style, const char *name, const char *cls) {
    double k;
    if (kv_get(&CACHE, style, name, &k)) return k;
    if (real_k(style, name, &k) < 0) k = 1.0;
    kv_put(&CACHE, style, name, k);
    return k;
}

/* scaled(): S(g) = g if g is None or g.is_empty else affinity.scale(g, k, k, origin=(cx, cy)) */
static GEOSGeometry *S_g(const GEOSGeometry *g, double k, double cx, double cy) { if (shp_is_empty(g)) return shp_clone(g); return shp_scale(g, k, k, SHP_ORIGIN_POINT, cx, cy); }
static SoVal S_val(const SoVal *v, double k, double cx, double cy, int *err) {
    SoVal o = *v;
    if (v->t == LV_GEOM) { o.g = v->g ? S_g(v->g, k, cx, cy) : NULL; if (v->g && !o.g) *err = 1; }
    else if (v->t == LV_LATER) so_later_ref(v->later);
    return o;
}
static SoVal copy_val(const SoVal *v) { SoVal o = *v; if (v->t == LV_GEOM && v->g) o.g = shp_clone(v->g); else if (v->t == LV_LATER) so_later_ref(v->later); return o; }
static int num_times(const SoVal *v, double k, SoVal *o) {          /* L[i] * k (a number times a float: a float) */
    if (v->t == LV_INT) { *o = lv_float((double)v->i * k); return 0; }
    if (v->t == LV_FLOAT) { *o = lv_float(v->f * k); return 0; }
    return SC_ERR;
}
/* pieces(src): [(S(q[0]), q[1] * k, q[2], q[3] * k) + tuple(q[4:]) for q in src] */
static int scale_pieces(const SoPieces *src, double k, double cx, double cy, SoPieces *out) {
    int i;
    for (i = 0; i < src->n; i++) {
        const SoPiece *q = &src->v[i]; GEOSGeometry *g = q->g ? S_g(q->g, k, cx, cy) : NULL;
        if (q->g && !g) return SO_ERR;
        pieces_push(out, g, q->ztop * k, q->c, q->z * k, q->ghost);
    }
    return SO_OK;
}
typedef struct { SoVal src; double k, cx, cy; } ScCtx;
static int scl_fill(void *c, SoPieces *out) { ScCtx *x = (ScCtx *)c; const SoPieces *p = ft_pieces(&x->src); if (!p) return SO_ERR; return scale_pieces(p, x->k, x->cx, x->cy, out); }
static void scl_free(void *c) { ScCtx *x = (ScCtx *)c; if (x->src.t == LV_LATER) so_later_unref(x->src.later); free(x); }

/* sd_reg.scaled(out, cx, cy, k) */
int sdr_scaled(Geo *foot, const SoLayers *layers, double cx, double cy, double k, Geo **foot_out, SoLayers *out) {
    int i, j, err = 0;
    for (i = 0; i < layers->n; i++) {
        const SoLayer *L = layers->v[i]; SoLayer *N; const char *t;
        if (!L) { lyl_push_own(out, NULL); continue; }          /* None stays */
        if (L->n < 1) return SC_ERR;
        t = vstr(&L->v[0]);
        if (!t || !(!strcmp(t, "up") || !strcmp(t, "mound") || !strcmp(t, "steps") || !strcmp(t, "on") || !strcmp(t, "tcut") || !strcmp(t, "gcut") || !strcmp(t, "pad"))) { lyl_push(out, (SoLayer *)L); continue; }          /* L2 = L */
        if (L->n < 2) return SC_ERR;
        N = ly_new(L->n);
        for (j = 0; j < L->n; j++) N->v[j] = copy_val(&L->v[j]);
        if (N->v[1].t == LV_GEOM) { shp_free(N->v[1].g); N->v[1] = S_val(&L->v[1], k, cx, cy, &err); }
        if (!strcmp(t, "up") || !strcmp(t, "mound")) { if (L->n < 3 || num_times(&L->v[2], k, &N->v[2]) < 0) err = 1; }
        else if (!strcmp(t, "on") || !strcmp(t, "tcut")) { if (L->n < 4 || num_times(&L->v[3], k, &N->v[3]) < 0) err = 1; }
        else if (!strcmp(t, "gcut")) { if (L->n < 3 || num_times(&L->v[2], pmin(k, 1.5), &N->v[2]) < 0) err = 1; }
        else if (!strcmp(t, "steps")) {
            if (L->n < 3) err = 1;
            else if (ft_is_later(&L->v[2])) {          /* a _Later stays one: type(L[2])(pieces) */
                ScCtx *x = (ScCtx *)calloc(1, sizeof *x); x->src = L->v[2]; if (x->src.t == LV_LATER) so_later_ref(x->src.later); x->k = k; x->cx = cx; x->cy = cy;
                if (N->v[2].t == LV_LATER) so_later_unref(N->v[2].later);
                N->v[2].t = LV_PTR; N->v[2].p = ft_steps_later(scl_fill, x, scl_free); N->v[2].later = NULL;
            } else {
                const SoPieces *p = ft_pieces(&L->v[2]); SoPieces q = { 0 };
                if (!p || scale_pieces(p, k, cx, cy, &q) != SO_OK) err = 1;
                else { N->v[2].t = LV_PTR; N->v[2].p = ft_steps_list(&q); }
                sop_free(&q);
            }
        }
        if (err) { ly_unref(N); return SC_ERR; }
        lyl_push_own(out, N);
    }
    if (!foot || shp_is_empty(foot->g)) *foot_out = geo_ref(foot);
    else { GEOSGeometry *g = shp_scale(foot->g, k, k, SHP_ORIGIN_POINT, cx, cy); if (!g) return SC_ERR; *foot_out = geo_new(g); }
    return 0;
}

typedef struct { char *style; SdrEntry *v; int n, cap; } RegL;
static RegL *REG; static int nreg;
static RegL *reg_of(const char *style, int make) {
    int i; for (i = 0; i < nreg; i++) if (!strcmp(REG[i].style, style)) return &REG[i];
    if (!make) return NULL;
    REG = (RegL *)realloc(REG, sizeof(RegL) * (size_t)(nreg + 1)); memset(&REG[nreg], 0, sizeof(RegL)); REG[nreg].style = _strdup(style);
    return &REG[nreg++];
}
/* d(style, cls, kind, name, r, ground)(f0): lst = REG.setdefault(style, []); the same name again takes its place */
void sdr_register(const SdrEntry *e) {
    RegL *l = reg_of(e->style, 1); int i;
    for (i = 0; i < l->n; i++) if (!strcmp(l->v[i].name, e->name)) { l->v[i] = *e; return; }
    if (l->n == l->cap) { l->cap = l->cap ? 2 * l->cap : 32; l->v = (SdrEntry *)realloc(l->v, sizeof(SdrEntry) * (size_t)l->cap); }
    l->v[l->n++] = *e;
}
int sdr_reg_count(const char *style) { RegL *l = reg_of(style, 0); return l ? l->n : 0; }
const SdrEntry *sdr_reg_get(const char *style, int i) { RegL *l = reg_of(style, 0); return l && i >= 0 && i < l->n ? &l->v[i] : NULL; }
void sdr_reg_clear(void) { int i; for (i = 0; i < nreg; i++) { free(REG[i].style); free(REG[i].v); } free(REG); REG = NULL; nreg = 0; }

/* d(...).w.f(cx, cy, a, rng) */
int sdr_f(void *entry, double cx, double cy, double a, PyRandom *rng, int has_scale, double scale, Geo **foot, SoLayers *layers) {
    const SdrEntry *e = (const SdrEntry *)entry; double k, ks; Geo *f0 = NULL; SoLayers l0 = { 0 }; int r;
    *foot = NULL;
    if (has_scale) return SC_ERR;          /* f takes four arguments: TypeError */
    if (!kv_get(&SCALE, e->style, e->name, &ks)) ks = HAS_STAR ? STAR : 1.0;
    k = ks * sdr_real_factor(e->style, e->name, e->cls);
    if (k <= 0) return 1;          /* too small to be worth drawing on a base this size */
    r = e->f0(e->ctx, cx, cy, a, rng, 0, 1.0, &f0, &l0);
    if (r != 0) { geo_unref(f0); lyl_free(&l0); return r; }
    if (fabs(k - 1.0) < 1e-6 || !f0) { *foot = f0; for (int i = 0; i < l0.n; i++) lyl_push(layers, l0.v[i]); lyl_free(&l0); return 0; }
    r = sdr_scaled(f0, &l0, cx, cy, k, foot, layers);
    geo_unref(f0); lyl_free(&l0);
    return r;
}
