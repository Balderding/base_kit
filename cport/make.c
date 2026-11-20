/* make.c: gg3.make(style, n) in C (see make.h). */
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "make.h"
#include "audit.h"
#include "pyexact.h"

MkCfg MKC;

static Scene *layout_default(const char *style, int n, const char *salt, const char *reseed, PyRandom *mixr, int *err) {
    return ly_ground(style, n, salt, reseed, mixr, NULL, err);
}
void mk_config(void) {
    memset(&MKC, 0, sizeof MKC);
    MKC.salt = ""; MKC.reseed = ""; MKC.random_mix = 1; MKC.scenes = 0; MKC.landmarks = 1; MKC.set_pieces = 1; MKC.extra_details = 1;
    MKC.layout = layout_default;
}
void mk_style_fns_free(MkStyleFns *f) {
    int i; for (i = 0; i < 3; i++) cp_fnl_free(&f->tiers[i]);
    cp_fnl_free(&f->late); cp_fnl_free(&f->sparse); cp_fnl_free(&f->details); cp_fnl_free(&f->new20);
    free(f->landmarks); memset(f, 0, sizeof *f);
}
static int has_fn(const ScFnList *l, const ScFn *f) { int i; for (i = 0; i < l->n; i++) if (l->v[i] == f) return 1; return 0; }

/* scenes._tag_all over this style's tier functions and details (idempotent: TAGS values are fixed) */
static void tag_all(MkStyleFns *F) {
    ScFn *all[4096]; int n = 0, i, j, k; SmTag tg[4096]; const char *nm[4096]; int same[4096];
    for (k = 0; k < 3; k++) for (i = 0; i < F->tiers[k].n && n < 4096; i++) all[n++] = F->tiers[k].v[i];
    for (i = 0; i < F->details.n && n < 4096; i++) all[n++] = F->details.v[i];
    for (i = 0; i < n; i++) {
        tg[i] = all[i]->tag; nm[i] = all[i]->qualname ? all[i]->qualname : ""; same[i] = i;
        for (j = 0; j < i; j++) if (all[j] == all[i]) { same[i] = j; break; }
    }
    sm_tag_all(tg, nm, same, n);
    for (i = 0; i < n; i++) if (same[i] == i) all[i]->tag = tg[i];
}

static char SALT_BUF[1024];
Scene *mk_make(const char *style, int n, CpOut *out, int *err) {
    Scene *s; PyRandom mixr, rng; int t, RND, i, lvl; char seed[1400], salt[1200]; ScFnList fns = { 0 }, f2 = { 0 }; MkStyleFns F; MkStyleFn sf; int r;
    *err = 0; memset(out, 0, sizeof *out); memset(&F, 0, sizeof F);
    if (n > 20) {          /* 21+: same 4 detail tiers in turn, each with its own seed so every one is new */
        int k = n; char tt[512], *h;
        n = (n - 1) % 20 + 1;
        { char tmp[1024]; snprintf(tmp, sizeof tmp, "%sx%d", MKC.salt ? MKC.salt : "", k); snprintf(SALT_BUF, sizeof SALT_BUF, "%s", tmp); } MKC.salt = SALT_BUF; CPC.salt = SALT_BUF;          /* os.environ["SALT"] += f"x{k}" */
        s = mk_make(style, n, out, err);
        if (!s) return NULL;
        free(s->name); snprintf(tt, sizeof tt, "%s_%02d", style, k); s->name = _strdup(tt);
        h = strrchr(s->title, '#'); if (h) *h = 0;
        snprintf(tt, sizeof tt, "%s#%d", s->title, k); free(s->title); s->title = _strdup(tt);
        return s;
    }
    t = (n - 1) / 5;
    snprintf(salt, sizeof salt, "%s%s", MKC.salt ? MKC.salt : "", MKC.reseed ? MKC.reseed : "");
    RND = salt[0] && MKC.random_mix;
    kinds_layout_hook(style);          /* style_blight / style_obelis: _register() is their layout's first line */
    s = (MKC.layout ? MKC.layout : layout_default)(style, n, MKC.salt ? MKC.salt : "", MKC.reseed ? MKC.reseed : "", &mixr, err);
    if (!s) { if (!*err) *err = SC_ERR; return NULL; }
    if (pl_terr(s)) T_init(s);
    else { s->has_objs = 1; gl_clear(&s->objs); gl_clear(&s->keepout); }
    s->has_base_ops = 1; ol_free(&s->base_ops); for (i = 0; i < s->ops.n; i++) ol_push(&s->base_ops, s->ops.v[i]);
    snprintf(seed, sizeof seed, "%s%dtier%s%s", style, n, MKC.salt ? MKC.salt : "", MKC.reseed ? MKC.reseed : ""); py_seed_str(&rng, seed);
    sf = MKC.style_fns ? MKC.style_fns : (MkStyleFn)bld_any("gg3", "style_fns", "MkStyleFn");
    if (!sf || sf(style, &F) < 0) { *err = SC_ERR; scene_free(s); return NULL; }          /* TIERS[style]: KeyError */
    for (lvl = t; lvl > 0; lvl--) for (i = 0; i < F.tiers[lvl - 1].n; i++) cp_fnl_push(&fns, F.tiers[lvl - 1].v[i]);          /* newest tier first */
    if (RND && fns.n) {          /* a random pick of the extras up to this tier */
        int64_t k = (int64_t)py_rint((double)fns.n * py_uniform(&mixr, 0.55, 1.0)), *ix; ScFnList keep = { 0 };
        if (k > fns.n) k = fns.n;
        if (k < t) k = t;
        ix = (int64_t *)malloc(sizeof(int64_t) * (size_t)(k + 1));
        if (py_sample_e(&mixr, fns.n, k, ix) != 0) { free(ix); cp_fnl_free(&fns); mk_style_fns_free(&F); scene_free(s); *err = SC_ERR; return NULL; }
        for (i = 0; i < k; i++) cp_fnl_push(&keep, fns.v[ix[i]]);
        for (i = 0; i < fns.n; i++) if (has_fn(&keep, fns.v[i])) cp_fnl_push(&f2, fns.v[i]);
        free(ix); cp_fnl_free(&keep); cp_fnl_free(&fns); fns = f2; memset(&f2, 0, sizeof f2);
    }
    for (i = 0; i < fns.n; i++) if (!has_fn(&F.late, fns.v[i])) cp_fnl_push(&f2, fns.v[i]);
    for (i = 0; i < fns.n; i++) if (has_fn(&F.late, fns.v[i])) cp_fnl_push(&f2, fns.v[i]);
    cp_fnl_free(&fns); fns = f2;
    /* features.install(globals()); details.install(...); scenes.install(...); style_details.install(...) */
    kinds_install_features(); kinds_install_details(); kinds_install_scenes(); tag_all(&F); kinds_install_style_details();
    lf_focus = NULL; lf_aimfn = NULL;
    if (MKC.scenes) {
        CpPools pools; pools.details = &F.details; pools.new20 = &F.new20;
        CPC.salt = MKC.salt; CPC.reseed = MKC.reseed;
        r = cp_compose(s, style, &mixr, &rng, t, &fns, &pools, out);
        lf_focus = NULL; lf_aimfn = NULL; PL.hboost = 1.0; cp_release(s);          /* finally: */
        cp_fnl_free(&fns); mk_style_fns_free(&F);
        if (r < 0) { *err = r; scene_free(s); return NULL; }
        {   const char *af = getenv("BBB_AUDIT");          /* the check: one JSON line per laid-out base (audit.h) */
            if (af && *af) { FILE *f = fopen(af, "a"); if (f) { au_scene(s, style, n, SC.base_mm, out ? out->theme : NULL, f); fclose(f); } }
        }
        return s;
    }
    /* scenes off: the old way */
    PL.hboost = SC.base_mm >= 59.9 ? CPC.big_base_height_factor : 1.0;
    if (SC.base_mm >= 59.9 && MKC.landmarks) {
        if (MKC.landmarks_fn) MKC.landmarks_fn(s, style, &mixr, &F, &out->s3d);
        else ft_landmarks(s, F.landmarks, F.nlandmarks, &mixr, SC.base_mm, &out->s3d);
    }
    if (SC.K > 1.15 && MKC.set_pieces) {
        MkDetailsFn an = (MkDetailsFn)bld_any("details", "anchors", "MkDetailsFn");
        if (!an || an(s, style, &mixr, t) < 0) { PL.hboost = 1.0; *err = SC_ERR; cp_fnl_free(&fns); mk_style_fns_free(&F); scene_free(s); return NULL; }
    }
    PL.hboost = 1.0;
    {
        double kk = SC.K; int reps, rep; double re = 0, im = 0;
        px_pow(kk, 1.4, &re, &im);
        reps = (int)py_rint(re); if (reps > 10) reps = 10; if (reps < 1) reps = 1;
        for (rep = 0; rep < reps; rep++) for (i = 0; i < fns.n; i++) {
            ScFn *fn = fns.v[i];
            if (rep && py_random(&mixr) < (has_fn(&F.sparse, fn) ? 0.75 : 0.35)) continue;
            cp_call_fn(fn, s, &rng, t);          /* an error is caught (kitlog.caught("tier detail ...")) and the loop goes on */
        }
    }
    if (MKC.extra_details) {
        MkDetailsFn ap = (MkDetailsFn)bld_any("details", "apply", "MkDetailsFn");
        if (!ap || ap(s, style, &mixr, t) < 0) { *err = SC_ERR; cp_fnl_free(&fns); mk_style_fns_free(&F); scene_free(s); return NULL; }
    }
    cp_fnl_free(&fns); mk_style_fns_free(&F);
    return s;
}
