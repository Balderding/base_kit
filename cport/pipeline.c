/* pipeline.c: one base end to end in C (see pipeline.h). Each part names the Python it copies (mostly
   work\cscene\runner\layout_bridge.py, which held the last Python of a job). */
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <ctype.h>
#include <io.h>
#include <fcntl.h>
#include "pipeline.h"
#include "runner.h"
#include "make.h"
#include "stylefns.h"
#include "meshglue.h"
#include "models.h"
#include "solids3d.h"
#include "smooth3d.h"
#include "rocks3d.h"
/* from the builder parts (their own headers are for their files only) */
void bsdde_config(const char *models_root, double max_height);          /* bld_sdde.h */
extern FtS3dList *ns_s3d;          /* bld_ns.h */
extern FtS3dList *bs_s3d;          /* bld_scenes.h */
#include "pyx.h"

/* numpy's cos / sin of arange(40) * (2 pi / 40): gg._CSX / gg._CSY (numpy's own vectorised cos, not the UCRT's) */
static const double CSX[40] = { 0x1.0000000000000p+0, 0x1.f9b24942fe45cp-1, 0x1.e6f0e134454ffp-1, 0x1.c83201d3d2c6dp-1, 0x1.9e3779b97f4a8p-1, 0x1.6a09e667f3bcdp-1, 0x1.2cf2304755a5ep-1, 0x1.d0e2e2b44de01p-2, 0x1.3c6ef372fe950p-2, 0x1.4060b67a85377p-3, 0x1.1a62633145c07p-54, -0x1.4060b67a85373p-3, -0x1.3c6ef372fe94ep-2, -0x1.d0e2e2b44de00p-2, -0x1.2cf2304755a5dp-1, -0x1.6a09e667f3bccp-1, -0x1.9e3779b97f4a7p-1, -0x1.c83201d3d2c6cp-1, -0x1.e6f0e134454ffp-1, -0x1.f9b24942fe45bp-1, -0x1.0000000000000p+0, -0x1.f9b24942fe45cp-1, -0x1.e6f0e13445500p-1, -0x1.c83201d3d2c6dp-1, -0x1.9e3779b97f4a8p-1, -0x1.6a09e667f3bcep-1, -0x1.2cf2304755a5fp-1, -0x1.d0e2e2b44de03p-2, -0x1.3c6ef372fe952p-2, -0x1.4060b67a8537bp-3, -0x1.a79394c9e8a0ap-53, 0x1.4060b67a8536ep-3, 0x1.3c6ef372fe94cp-2, 0x1.d0e2e2b44ddfep-2, 0x1.2cf2304755a5cp-1, 0x1.6a09e667f3bcbp-1, 0x1.9e3779b97f4a7p-1, 0x1.c83201d3d2c6cp-1, 0x1.e6f0e134454ffp-1, 0x1.f9b24942fe45bp-1 };
static const double CSY[40] = { 0x0.0p+0, 0x1.4060b67a85375p-3, 0x1.3c6ef372fe94fp-2, 0x1.d0e2e2b44de00p-2, 0x1.2cf2304755a5ep-1, 0x1.6a09e667f3bcdp-1, 0x1.9e3779b97f4a8p-1, 0x1.c83201d3d2c6cp-1, 0x1.e6f0e134454ffp-1, 0x1.f9b24942fe45cp-1, 0x1.0000000000000p+0, 0x1.f9b24942fe45cp-1, 0x1.e6f0e13445500p-1, 0x1.c83201d3d2c6dp-1, 0x1.9e3779b97f4a8p-1, 0x1.6a09e667f3bcdp-1, 0x1.2cf2304755a5fp-1, 0x1.d0e2e2b44de02p-2, 0x1.3c6ef372fe951p-2, 0x1.4060b67a85379p-3, 0x1.1a62633145c07p-53, -0x1.4060b67a85370p-3, -0x1.3c6ef372fe94dp-2, -0x1.d0e2e2b44ddfep-2, -0x1.2cf2304755a5dp-1, -0x1.6a09e667f3bccp-1, -0x1.9e3779b97f4a7p-1, -0x1.c83201d3d2c6cp-1, -0x1.e6f0e134454ffp-1, -0x1.f9b24942fe45bp-1, -0x1.0000000000000p+0, -0x1.f9b24942fe45cp-1, -0x1.e6f0e13445500p-1, -0x1.c83201d3d2c6ep-1, -0x1.9e3779b97f4a9p-1, -0x1.6a09e667f3bcep-1, -0x1.2cf2304755a60p-1, -0x1.d0e2e2b44de04p-2, -0x1.3c6ef372fe954p-2, -0x1.4060b67a8537dp-3 };

/* ================================================================ the process's set-up (import gg3) */
static int READY; static const char *MODE;
static char *E_(const char *k) { const char *v = getenv(k); return v ? _strdup(v) : NULL; }          /* os.environ.get(k) (kept for the process) */
static double env_float(const char *k, double d) { const char *v = getenv(k); double x; if (!v || !*v) return d; if (ft_pyfloat(v, &x) != 0) return d; return x; }
static int cfg_supports(void) { const char *e = getenv("KIT_SUPPORTS"); if (e && (!strcmp(e, "0") || !strcmp(e, "1"))) return e[0] == '1'; return runner_cfg_bool("allow_supports", 0); }
static int cfg_scenes(void) { const char *e = getenv("KIT_SCENES"); if (e && (!strcmp(e, "0") || !strcmp(e, "1"))) return e[0] == '1'; return runner_cfg_bool("scenes", 0); }
static double max_height(double mm) {          /* gg.max_height(mm) */
    double m = runner_cfg_num("max_detail_height_mm", 3.3);
    if (m <= 0) return 1000.0;
    return m * (mm >= 59.9 ? runner_cfg_num("big_base_height_factor", 1.5) : 1.0);
}
static MgEnv ENV;                             /* what meshglue reads */
static char *SALT0, *RESEED0, *OBJ, *MINI, *THEME, *FINAL, *KSCALE, *NOREAL, *WORKER, *FAULT, *MINI_CFG;
static char OBJBUF[64];
static const char *caught_line(void *u, const char *where) { (void)u; return mg_log_caught(where, "Exception", "raised in the C kit", ""); }

/* ground_big's colours into gg.COL (groundbig.c hooks) */
extern const char *(*ly_col_get)(const char *k);
extern void (*ly_col_put)(const char *k, const char *v, int overwrite);
static const char *col_get(const char *k) { return kmap_get(&KD.col, k); }
static void col_put(const char *k, const char *v, int ow) { if (ow) kmap_set(&KD.col, k, v); else kmap_setdefault(&KD.col, k, v); }
extern void (*ft_col_setdefault)(const char *k2, const char *kind);          /* features.c */
static void col_pad(const char *k2, const char *kind) { const char *c = kmap_get(&KD.col, kind); kmap_setdefault(&KD.col, k2, c ? c : "#999999"); }

static int setup(const char *mode) {
    double base_mm, ts; const char *ov;
    if (READY) return READY > 0;
    READY = -1; MODE = mode;
    runner_cfg_init();
    if (!shp_init() || !pyx_load() || !px_init()) return 0;
    /* gg.py at import */
    base_mm = env_float("BASE_MM", 30.0);
    ts = env_float("KIT_SCALE", 1.0); if (ts < 1.0) ts = 1.0;          /* _TS = max(1.0, float(KIT_SCALE or 1)) */
    if (!scene_config(base_mm, !strcmp(mode, "build"), ts, max_height(base_mm), runner_cfg_bool("chamfer_rocks", 1), runner_cfg_bool("base_first", 1), 1)) return 0;
    SC.layout = 0; lf_set_csx(CSX, CSY);
    sm_config_default();
    FINAL = E_("KIT_FINAL_MM"); KSCALE = E_("KIT_SCALE"); NOREAL = E_("KIT_NO_REAL");
    ft_set_env(KSCALE, FINAL, NOREAL);
    /* gg3: HBOOST, DISC, NOTCH, cstone.enabled() (the C rocks: c_rocks and KIT_CROCKS) */
    { const char *cr = getenv("KIT_CROCKS"); int crocks = runner_cfg_bool("c_rocks", 1) && !(cr && !strcmp(cr, "0"));
      if (!pl_config(1.0, getenv("NOTCH") && *getenv("NOTCH"), crocks, CSX, CSY)) return 0;
      PL.cover = runner_cfg_num("plate_cover", 0.97); PL.step = runner_cfg_num("max_step_mm", 0.3); PL.cover_big = runner_cfg_num("plate_cover_big", 0.9);
      sc_min_part = runner_cfg_num("min_part_mm", 0.5); sc_min_cut = runner_cfg_num("min_cut_mm", 0.3); kd_wet_hook_fn = pl_is_wet; pl_terrk_fn = pl_is_terrk;
      { extern int cp_cart_on_track, cp_outcrops; extern double cp_model_room; cp_cart_on_track = runner_cfg_bool("cart_on_track", 1); cp_model_room = runner_cfg_num("model_room", 1.0); cp_outcrops = runner_cfg_bool("outcrops", 1); } }          /* readable details */          /* one level under each detail */
    if (!solids_init(cfg_supports(), ft_true_scale(), SC.ts, SC.MINW, SC.R0, SC.H, 12.0)) return 0;
    { const char *e = getenv("KIT_DETAIL_STEP"); SO.step = e && *e ? strtod(e, NULL) : runner_cfg_num("detail_step_mm", 0.1); }          /* finer steps for rounded objects (0 = the Python kit's) */
    { const char *e = getenv("KIT_SMOOTH"); SO.smooth = e && (!strcmp(e, "0") || !strcmp(e, "1")) ? e[0] == '1' : runner_cfg_bool("smooth_details", 0); }          /* smooth details (smooth3d.c) */
    sm_shell_grid_mm = runner_cfg_num("smooth_grid_mm", 0.08); if (!(sm_shell_grid_mm >= 0.02)) sm_shell_grid_mm = 0.08;
    { const char *e = getenv("KIT_ROCKS"); ork_on = e && (!strcmp(e, "0") || !strcmp(e, "1")) ? e[0] == '1' : runner_cfg_bool("organic_rocks", 0); }          /* organic rocks (rocks3d.c) */
    if (ork_on) { bc_organic_kind = ork_kind; bc_organic_take = ork_take; }
    ly_config(runner_cfg_bool("random_mix", 1), cfg_scenes(), runner_cfg_bool("outcrop_cracks", 0));
    kinds_init();
    ly_col_get = col_get; ly_col_put = col_put; ft_col_setdefault = col_pad;
    /* the model packs from the exe itself; sd_mesh's height limit */
    mdl_blob_from_resource(NULL, "MODELS_BIN");
    bsdde_config(NULL, max_height(base_mm));
    sf_new_details = runner_cfg_bool("new_details", 1); sf_style_details = runner_cfg_bool("style_details", 1);
    bld_register_all(); sf_register();
    /* scenes / compose */
    cp_config_default();
    SALT0 = E_("SALT"); RESEED0 = E_("RESEED"); MINI = E_("KIT_MINI"); THEME = E_("THEME"); WORKER = E_("KIT_WORKER"); FAULT = E_("KIT_FAULT");
    { const char *ko = getenv("KIT_OBJECTS");          /* kit_settings: KIT_OBJECTS -> CFG objects_amount = float(...) */
      if (ko && *ko) { double x; if (ft_pyfloat(ko, &x) == 0) { runner_float_repr(x, OBJBUF, sizeof OBJBUF); OBJ = OBJBUF; } }
      else if (runner_cfg_has("objects_amount")) OBJ = _strdup(runner_cfg_text("objects_amount")); }
    if (runner_cfg_has("miniature")) MINI_CFG = _strdup(runner_cfg_text("miniature"));
    CPC.objects_amount = OBJ; CPC.miniature = MINI_CFG;
    CPC.max_slenderness = runner_cfg_num("max_slenderness", 3.0); CPC.big_base_height_factor = runner_cfg_num("big_base_height_factor", 1.5);
    CPC.no_bare_ground = runner_cfg_bool("no_bare_ground", 1); CPC.style_details = runner_cfg_bool("style_details", 1);
    CPC.style_details_amount = runner_cfg_num("style_details_amount", 0.5); CPC.clear_middle = runner_cfg_num("clear_middle", 0.45);
    CPC.kit_mini = MINI; CPC.kit_final_mm = FINAL; CPC.theme = THEME; CPC.kit_worker = WORKER; CPC.kit_fault = FAULT; CPC.plan_json = NULL;
    CPC.salt = SALT0 ? SALT0 : ""; CPC.reseed = RESEED0 ? RESEED0 : "";
    cp_config();
    sf_subject_base = CPH.subject; CPH.subject = sf_subject;          /* + the subjects the style files add */
    CPH.caught = caught_line; CPH.u = NULL;
    mk_config();
    MKC.salt = CPC.salt; MKC.reseed = CPC.reseed;
    MKC.random_mix = runner_cfg_bool("random_mix", 1); MKC.scenes = cfg_scenes(); MKC.landmarks = runner_cfg_bool("landmarks", 1);
    MKC.set_pieces = runner_cfg_bool("set_pieces", 1); MKC.extra_details = runner_cfg_bool("extra_details", 1);
    /* what meshglue reads */
    memset(&ENV, 0, sizeof ENV);
    ENV.base_mm = SC.base_mm; ENV.final_mm = env_float("KIT_FINAL_MM", 0.0); ENV.scale = env_float("KIT_SCALE", 1.0);
    ENV.tile = env_float("MESH_TILE", 12.0); if (!(ENV.tile > 0)) ENV.tile = 12.0;
    ov = getenv("KIT_OVAL");          /* gg.oval() */
    if (ov && *ov) {
        char t[128]; int j = 0; double a, b; char *x;
        for (const char *p = ov; *p && j < 120; p++) t[j++] = (char)tolower((unsigned char)*p);
        t[j] = 0; { char *mm; while ((mm = strstr(t, "mm"))) memmove(mm, mm + 2, strlen(mm + 2) + 1); }
        x = strchr(t, 'x');
        if (x) {
            *x = 0; char *s0 = t, *s1 = x + 1; while (*s0 == ' ') s0++; while (*s1 == ' ') s1++;
            if (ft_pyfloat(s0, &a) == 0 && ft_pyfloat(s1, &b) == 0 && a > 0 && b > 0) { ENV.has_oval = 1; ENV.oval[0] = a > b ? a : b; ENV.oval[1] = a > b ? b : a; }
        }
    }
    { char d[MAX_PATH * 3]; const char *e = getenv("KIT_ERRORS_DIR"); if (e && *e) snprintf(d, sizeof d, "%s", e); else snprintf(d, sizeof d, "%s\\out", runner_kit()); mg_log_dir(d); }
    READY = 1;
    return 1;
}

/* ================================================================ the scene as buildcmds.h holds it */
typedef struct { const void *k; void *v; } Pair;
typedef struct { Pair *v; int n, cap; } PMap;
static void *pm_get(const PMap *m, const void *k) { for (int i = 0; i < m->n; i++) if (m->v[i].k == k) return m->v[i].v; return NULL; }
static void pm_put(PMap *m, const void *k, void *v) { if (m->n == m->cap) { m->cap = m->cap ? 2 * m->cap : 64; m->v = (Pair *)realloc(m->v, sizeof(Pair) * (size_t)m->cap); } m->v[m->n].k = k; m->v[m->n].v = v; m->n++; }
static BcNum bn(Num n) { BcNum b; b.v = n.v; b.t = n.t == NUM_NONE ? BC_FLOAT : n.t; return b; }
typedef struct { PMap kinds, ops; BcScene *b; } Conv;
static BcKind *ck(Conv *c, const Kind *k) {
    BcKind *b = (BcKind *)pm_get(&c->kinds, k);
    if (b) return b;
    b = (BcKind *)calloc(1, sizeof *b);
    b->idx = c->b->nkinds; b->lk = k->lk; b->s = _strdup(k->s);
    b->has_loft = k->has_loft; b->loft_chain = bn(k->loft_chain); b->loft_z = bn(k->loft_z);
    b->has_ghost = k->has_ghost && k->ghost.t != NUM_NONE;
    c->b->kinds = (BcKind **)realloc(c->b->kinds, sizeof(BcKind *) * (size_t)(c->b->nkinds + 1)); c->b->kinds[c->b->nkinds++] = b;
    pm_put(&c->kinds, k, b);
    return b;
}
static GEOSGeometry **cpolys(const GeoList *l) {
    GEOSGeometry **p = (GEOSGeometry **)calloc((size_t)l->n + 1, sizeof(void *));
    for (int i = 0; i < l->n; i++) p[i] = l->v[i] && l->v[i]->g ? shp_clone(l->v[i]->g) : NULL;
    return p;
}
static BcOp *cop(Conv *c, Op *o) {
    BcOp *b = (BcOp *)pm_get(&c->ops, o); BcNum n[3];
    if (b) { b->rc++; return b; }
    for (int i = 0; i < 3; i++) n[i] = bn(o->n[i]);
    b = bc_op_new(o->type == OP_PRISM ? BC_PRISM : BC_TAPER, ck(c, o->kind), n, cpolys(&o->polys), o->polys.n);
    pm_put(&c->ops, o, b);
    return b;
}
static void to_bc(const Scene *s, BcScene *b) {
    Conv c; memset(&c, 0, sizeof c); memset(b, 0, sizeof *b); c.b = b;
    b->cfg.R0 = SC.R0; b->cfg.H = SC.H; b->cfg.RIM = 12.0; b->cfg.base_mm = SC.base_mm;
    b->cfg.sketch_tol = runner_cfg_num("sketch_tolerance_mm", 0.03); b->cfg.rim_gap = runner_cfg_num("rim_gap", 0.02);
    b->cfg.base_first = runner_cfg_bool("base_first", 1); b->cfg.group_pieces = runner_cfg_bool("group_pieces", 1); b->cfg.split_cuts = runner_cfg_bool("split_cuts", 1);
    b->cfg.chamfer_rocks = runner_cfg_bool("chamfer_rocks", 1); b->cfg.chamfer_grates = runner_cfg_bool("chamfer_grates", 1);
    b->cfg.noarc = getenv("NOARC") != NULL; b->cfg.gg2 = s->gg2; b->cfg.has_cutgap = s->has_cutgap; b->cfg.cutgap = bn(s->cutgap);
    b->cfg.true_circle_rim = runner_cfg_bool("true_circle_rim", 1);
    kinds_bc(&b->cfg);
    b->ops = (BcOp **)calloc((size_t)s->ops.n + 1, sizeof(BcOp *)); b->nops = s->ops.n;
    for (int i = 0; i < s->ops.n; i++) b->ops[i] = cop(&c, s->ops.v[i]);
    b->gcuts = (BcGcut *)calloc((size_t)s->gcuts.n + 1, sizeof(BcGcut)); b->ngcuts = s->gcuts.n;
    for (int i = 0; i < s->gcuts.n; i++) { const Gcut *g = &s->gcuts.v[i]; b->gcuts[i].polys = cpolys(&g->polys); b->gcuts[i].np = g->polys.n; b->gcuts[i].depth = bn(g->depth); b->gcuts[i].kind = ck(&c, g->kind); }
    b->tcuts = (BcTcut *)calloc((size_t)s->tcuts.n + 1, sizeof(BcTcut)); b->ntcuts = s->tcuts.n;
    for (int i = 0; i < s->tcuts.n; i++) { const Tcut *t = &s->tcuts.v[i]; b->tcuts[i].polys = cpolys(&t->polys); b->tcuts[i].np = t->polys.n; b->tcuts[i].z0 = bn(t->z0); b->tcuts[i].z1 = bn(t->z1); b->tcuts[i].kind = ck(&c, t->kind); }
    b->feet = cpolys(&s->feet); b->nfeet = s->feet.n;
    b->chops = (BcChop **)calloc((size_t)s->chops.n + 1, sizeof(BcChop *)); b->nchops = s->chops.n;
    for (int i = 0; i < s->chops.n; i++) {
        const Chop *h = s->chops.v[i]; BcChop *x = (BcChop *)calloc(1, sizeof *x);
        x->kind = ck(&c, h->kind); x->poly = h->poly && h->poly->g ? shp_clone(h->poly->g) : NULL;
        x->z0 = bn(h->z0); x->top = bn(h->top); x->run = bn(h->run); x->drop = bn(h->drop); x->has_iso = h->has_iso; x->iso = bn(h->iso);
        x->ops = (BcOp **)calloc((size_t)h->ops.n + 1, sizeof(BcOp *)); x->nops = h->ops.n;
        for (int j = 0; j < h->ops.n; j++) x->ops[j] = cop(&c, h->ops.v[j]);
        b->chops[i] = x;
    }
    free(c.kinds.v); free(c.ops.v);
}

/* ================================================================ the drawing's scene ("BBD1": layout_bridge.scene_blob) */
static void b_add(MgBuf *o, const void *p, size_t n) { mg_buf_add(o, p, n); }
static void b_u8(MgBuf *o, int v) { unsigned char c = (unsigned char)v; b_add(o, &c, 1); }
static void b_u32(MgBuf *o, uint32_t v) { b_add(o, &v, 4); }
static void b_f64(MgBuf *o, double v) { b_add(o, &v, 8); }
static void b_str(MgBuf *o, const char *s) { uint32_t n = (uint32_t)strlen(s ? s : ""); b_u32(o, n); b_add(o, s ? s : "", n); }
static int ring_out(MgBuf *o, const GEOSGeometry *r) {
    static double *xy; static int cap; int n;
    if (!r) { b_u32(o, 0); return 1; }
    for (;;) { if (!cap) { cap = 4096; xy = (double *)malloc(sizeof(double) * 2 * (size_t)cap); } n = shp_coords(r, xy, cap); if (n >= 0 || cap > (1 << 24)) break; cap *= 4; xy = (double *)realloc(xy, sizeof(double) * 2 * (size_t)cap); }
    if (n < 0) n = 0;
    b_u32(o, (uint32_t)n); b_add(o, xy, sizeof(double) * 2 * (size_t)n);
    return 1;
}
static void geom(MgBuf *o, const GEOSGeometry *p, int *bad) {          /* scene_blob.geom */
    if (!p) { *bad = 1; b_u8(o, 0); return; }
    if (shp_type(p) != GEOS_POLYGON) { *bad = 1; b_u8(o, 2); return; }
    int ni = shp_is_empty(p) ? 0 : shp_num_interiors(p);
    b_u8(o, 1); b_u32(o, (uint32_t)(1 + ni));
    ring_out(o, shp_is_empty(p) ? NULL : shp_exterior(p));
    for (int i = 0; i < ni; i++) ring_out(o, shp_interior(p, i));
}
static void geom_op(MgBuf *o, const GEOSGeometry *p, int *bad) { if (!p || shp_is_empty(p)) { b_u8(o, 0); return; } geom(o, p, bad); }
static const char *colour(const KMap *m, const char *k, const char *d) { const char *v = kmap_get(m, k); return v ? v : d; }
/* two polygons with the very same coordinates (the C copies polygons where Python keeps the objects) */
static int same_poly(const GEOSGeometry *a, const GEOSGeometry *b) {
    static double *x, *y; static int cap; int na, nb, ni;
    if (!a || !b || shp_type(a) != GEOS_POLYGON || shp_type(b) != GEOS_POLYGON || shp_is_empty(a) || shp_is_empty(b)) return 0;
    ni = shp_num_interiors(a); if (ni != shp_num_interiors(b)) return 0;
    for (int r = -1; r < ni; r++) {
        const GEOSGeometry *ra = r < 0 ? shp_exterior(a) : shp_interior(a, r), *rb = r < 0 ? shp_exterior(b) : shp_interior(b, r);
        for (;;) {
            if (!cap) { cap = 4096; x = (double *)malloc(16 * (size_t)cap); y = (double *)malloc(16 * (size_t)cap); }
            na = shp_coords(ra, x, cap); nb = shp_coords(rb, y, cap);
            if ((na >= 0 && nb >= 0) || cap > (1 << 24)) break;
            cap *= 4; x = (double *)realloc(x, 16 * (size_t)cap); y = (double *)realloc(y, 16 * (size_t)cap);
        }
        if (na != nb || na < 0 || memcmp(x, y, 16 * (size_t)na)) return 0;
    }
    return 1;
}
/* mesher._draw2d_pil: a chamfered solid's placeholder ("rockc") is drawn in its chop's own kind: _chopk maps id(q) of
   every chop op polygon to the chop's kind (later chops win), and the first of the op's polygons found decides. The C
   compares coordinates; Python's identity holds only while nothing rebuilt the polygon (build: smooth_isolated
   rebuilds every one, so there match = 0). */
static const char *chop_kind(const BcScene *b, const BcOp *op) {
    for (int q = 0; q < op->np; q++) for (int k = b->nchops - 1; k >= 0; k--) {
        const BcChop *c = b->chops[k];
        for (int j = 0; j < c->nops; j++) for (int t = 0; t < c->ops[j]->np; t++) if (same_poly(op->polys[q], c->ops[j]->polys[t])) return c->kind->s;
    }
    return NULL;
}
/* the drawing of the scene as build_cmds left it; bad = Python's PIL drawing would fail (a piece that is not a Polygon) */
static void scene_blob(const BcScene *b, const char *ground, int has_sp, const MgPlan *sp, int match, MgBuf *o, int *bad) {
    BcGcut *fg = NULL; int nfg = 0;
    *bad = 0;
    b_add(o, "BBD1", 4); b_f64(o, SC.R0); b_f64(o, SC.H); b_f64(o, SC.SINK); b_f64(o, SC.base_mm);
    b_u8(o, has_sp); b_f64(o, has_sp ? sp->sc : 0); b_f64(o, has_sp ? sp->fin : 0); b_f64(o, has_sp ? sp->tx : 0); b_f64(o, has_sp ? sp->ty : 0);
    b_u8(o, ENV.has_oval); b_f64(o, ENV.has_oval ? ENV.oval[0] : 0.0); b_f64(o, ENV.has_oval ? ENV.oval[1] : 0.0);
    b_str(o, ground);
    if (bc_final_gcuts(b, &fg, &nfg) < 0) { nfg = 0; fg = NULL; *bad = 1; }
    b_u32(o, (uint32_t)nfg);
    for (int i = 0; i < nfg; i++) {
        b_str(o, fg[i].kind->s); b_u8(o, fg[i].kind->lk); b_str(o, colour(&KD.cutc, fg[i].kind->s, "#222222"));
        b_u32(o, (uint32_t)fg[i].np); for (int j = 0; j < fg[i].np; j++) geom(o, fg[i].polys[j], bad);
    }
    bc_gcuts_free(fg, nfg);
    b_u32(o, (uint32_t)b->nops);
    for (int i = 0; i < b->nops; i++) {
        const BcOp *op = b->ops[i]; const char *kc = op->kind->s;
        if (op->type == BC_PRISM) { b_u8(o, 0); b_f64(o, op->n[0].v); b_f64(o, op->n[1].v); }
        else { b_u8(o, 1); b_f64(o, op->n[0].v); b_f64(o, op->n[1].v); }
        if (match && !strcmp(kc, "rockc")) { const char *ck2 = chop_kind(b, op); if (ck2) kc = ck2; }
        b_str(o, op->kind->s); b_u8(o, op->kind->lk); b_str(o, colour(&KD.col, kc, "#8a8a8a"));
        b_u32(o, (uint32_t)op->np); for (int j = 0; j < op->np; j++) geom_op(o, op->polys[j], bad);
    }
    b_u32(o, (uint32_t)b->ntcuts);
    for (int i = 0; i < b->ntcuts; i++) {
        b_str(o, b->tcuts[i].kind->s); b_u8(o, b->tcuts[i].kind->lk); b_str(o, colour(&KD.cutc, b->tcuts[i].kind->s, "#111111"));
        b_u32(o, (uint32_t)b->tcuts[i].np); for (int j = 0; j < b->tcuts[i].np; j++) geom(o, b->tcuts[i].polys[j], bad);
    }
}

/* ================================================================ JSON answers */
static MgJ kv2j(const KVal *v) {
    MgJ o;
    if (!v) return mgj_null();
    switch (v->t) {
    case KV_BOOL: return mgj_bool((int)v->i);
    case KV_INT: return mgj_int(v->i);
    case KV_FLOAT: return mgj_float(v->f);
    case KV_STR: return mgj_str(v->s);
    case KV_LIST: o = mgj_list(); for (int i = 0; i < v->n; i++) mgj_push(&o, kv2j(v->v[i])); return o;
    case KV_DICT: o = mgj_dict(); for (int i = 0; i < v->n; i++) mgj_set(&o, v->k[i], kv2j(v->v[i])); return o;
    default: return mgj_null();
    }
}
static MgJ strs_j(const MgStrs *l) { MgJ o = mgj_list(); for (int i = 0; i < l->n; i++) mgj_push(&o, mgj_str(l->s[i])); return o; }
static void answer1(MgBuf *o, const MgJ *head, const MgBuf *blob) {          /* "BBL1": the choices answer */
    MgBuf h = { 0 }; mgj_dump(head, 0, &h);
    b_add(o, "BBL1", 4); b_u32(o, (uint32_t)h.n); b_add(o, h.b, h.n); b_u32(o, blob ? (uint32_t)blob->n : 0); if (blob && blob->n) b_add(o, blob->b, blob->n);
    free(h.b);
}
static void answer2(MgBuf *o, const MgJ *head, const MgBuf *const *blobs, int nb) {          /* "BBL2": the build answer */
    MgBuf h = { 0 }; mgj_dump(head, 0, &h);
    b_add(o, "BBL2", 4); b_u32(o, (uint32_t)h.n); b_add(o, h.b, h.n); b_u32(o, (uint32_t)nb);
    for (int i = 0; i < nb; i++) { b_u32(o, (uint32_t)blobs[i]->n); if (blobs[i]->n) b_add(o, blobs[i]->b, blobs[i]->n); }
    free(h.b);
}
static void why(MgBuf *o, int build, const char *text) {
    MgJ h = mgj_dict(); mgj_set(&h, "ok", mgj_int(0)); mgj_set(&h, "why", mgj_str(text));
    if (build) answer2(o, &h, NULL, 0); else answer1(o, &h, NULL);
    mgj_free(&h);
}
/* the pads of s.stand (None entries kept as NULL); n = 0 without a stand */
static int stand_pads(const CpOut *out, const GEOSGeometry ***pads) {
    if (!out->stand.has_stand) { *pads = NULL; return 0; }
    const GEOSGeometry **p = (const GEOSGeometry **)calloc((size_t)out->stand.pads.n + 1, sizeof(void *));
    for (int i = 0; i < out->stand.pads.n; i++) p[i] = out->stand.pads.v[i] ? out->stand.pads.v[i]->g : NULL;
    *pads = p; return out->stand.pads.n;
}
static int threads_kit(void) { return runner_threads(); }          /* cmesh.threads() */

/* ================================================================ live.start (the window's progress note) */
/* live.piece / _flush / _write: while the layout places pieces (scene.h sc_piece_hook), their outlines go to
   <note>.shapes ("r" raised / "c" cut, then x y pairs, %.2f, at most 48 points a ring) and the note's pieces count
   is rewritten at most every 0.1 s. What is built is the same either way. */
static char LV_P[MAX_PATH * 3], LV_T0[64], LV_OV[80], LV_MM[40], LV_LR[40]; static char *LV_SH; static size_t LV_N, LV_CAP; static int LV_PIECES; static ULONGLONG LV_LAST;
static void lv_now(char *t, size_t cap) { FILETIME ft; GetSystemTimePreciseAsFileTime(&ft); uint64_t v = ((uint64_t)ft.dwHighDateTime << 32) | ft.dwLowDateTime; runner_float_repr((double)(v - 116444736000000000ULL) / 1e7, t, cap); }
static void lv_put(const char *b, size_t n) {
    if (LV_N + n + 1 > LV_CAP) { size_t c = LV_CAP ? LV_CAP * 2 : 65536; while (c < LV_N + n + 1) c *= 2; char *q = (char *)realloc(LV_SH, c); if (!q) return; LV_SH = q; LV_CAP = c; }
    memcpy(LV_SH + LV_N, b, n); LV_N += n;
}
static void lv_flush(void) {          /* live._flush: the outlines so far, added to <note>.shapes */
    if (!LV_N || !LV_P[0]) return;
    char q[MAX_PATH * 3 + 16]; WCHAR w[MAX_PATH * 3]; snprintf(q, sizeof q, "%s.shapes", LV_P); MultiByteToWideChar(CP_UTF8, 0, q, -1, w, MAX_PATH * 3);
    FILE *f = _wfopen(w, L"ab"); if (f) { fwrite(LV_SH, 1, LV_N, f); fclose(f); }
    LV_N = 0;
}
static void lv_write(void) {          /* live._write: the note, whole (stage layout, the pieces so far) */
    if (!LV_P[0]) return;
    char q[MAX_PATH * 3 + 16], now[64]; WCHAR w[MAX_PATH * 3], w2[MAX_PATH * 3]; lv_now(now, sizeof now); LV_LAST = GetTickCount64();
    snprintf(q, sizeof q, "%s.tmp", LV_P); MultiByteToWideChar(CP_UTF8, 0, q, -1, w, MAX_PATH * 3); MultiByteToWideChar(CP_UTF8, 0, LV_P, -1, w2, MAX_PATH * 3);
    FILE *f = _wfopen(w, L"wb"); if (!f) return;
    fprintf(f, "stage=layout\nt0=%s\nstage_t0=%s\npieces=%d\noval=%s\nmm=%s\nlay_r=%s\nnow=%s\n", LV_T0, LV_T0, LV_PIECES, LV_OV, LV_MM, LV_LR, now);
    fclose(f); MoveFileExW(w, w2, MOVEFILE_REPLACE_EXISTING);
}
static void lv_piece(char kind, const GeoList *polys) {          /* live.piece(kind, polys) */
    if (!LV_P[0] || !polys) return;
    LV_PIECES++;
    static double *xy; static int cap;
    for (int i = 0; i < polys->n; i++) {
        const GEOSGeometry *g = polys->v[i] ? polys->v[i]->g : NULL;
        if (!g || shp_type(g) != GEOS_POLYGON) continue;          /* getattr(p, "exterior", None) */
        const GEOSGeometry *ext = shp_exterior(g); int n = ext ? shp_coords(ext, xy, cap / 2) : 0;
        while (ext && n < 0 && cap < (1 << 24)) { cap = cap ? cap * 2 : 1024; xy = (double *)realloc(xy, sizeof(double) * (size_t)cap); n = shp_coords(ext, xy, cap / 2); }
        if (n < 0) continue;
        int step = n > 48 ? n / 48 + 1 : 1; char t[64];          /* cs[::len(cs) // 48 + 1] */
        lv_put(&kind, 1); lv_put(" ", 1);
        for (int k = 0, first = 1; k < n; k += step, first = 0) { int m = snprintf(t, sizeof t, "%s%.2f %.2f", first ? "" : " ", xy[2 * k], xy[2 * k + 1]); lv_put(t, (size_t)m); }
        lv_put("\n", 1);
    }
    if (GetTickCount64() - LV_LAST >= 100) { lv_flush(); lv_write(); }
}
static void lv_end(void) { lv_flush(); if (LV_P[0]) lv_write(); sc_piece_hook = NULL; LV_P[0] = 0; LV_N = 0; }          /* the layout is done (live.plan flushes) */
static void live_start(int n) {
    const char *d = getenv("KIT_LIVE"); char p[MAX_PATH * 3], q[MAX_PATH * 3 + 16], t[64], ovs[80] = "", mm[40], lr[40]; FILE *f; WCHAR w[MAX_PATH * 3], w2[MAX_PATH * 3];
    if (!d || !*d) return;
    snprintf(p, sizeof p, "%s\\%s_%d.txt", d, MKC.salt ? MKC.salt : "", n);
    CreateDirectoryA(d, NULL);
    for (int i = 0; i < 2; i++) { snprintf(q, sizeof q, "%s%s", p, i ? ".tiles" : ".shapes"); MultiByteToWideChar(CP_UTF8, 0, q, -1, w, MAX_PATH * 3); DeleteFileW(w); }
    { FILETIME ft; GetSystemTimePreciseAsFileTime(&ft); uint64_t v = ((uint64_t)ft.dwHighDateTime << 32) | ft.dwLowDateTime; runner_float_repr((double)(v - 116444736000000000ULL) / 1e7, t, sizeof t); }
    if (ENV.has_oval) { char a[32], b[32]; snprintf(a, sizeof a, "%g", ENV.oval[0]); snprintf(b, sizeof b, "%g", ENV.oval[1]); snprintf(ovs, sizeof ovs, "%sx%s", a, b); }
    snprintf(mm, sizeof mm, "%g", SC.base_mm); snprintf(lr, sizeof lr, "%.3f", SC.R0);
    snprintf(q, sizeof q, "%s.tmp", p); MultiByteToWideChar(CP_UTF8, 0, q, -1, w, MAX_PATH * 3); MultiByteToWideChar(CP_UTF8, 0, p, -1, w2, MAX_PATH * 3);
    f = _wfopen(w, L"wb");
    if (!f) return;
    fprintf(f, "stage=layout\nt0=%s\nstage_t0=%s\npieces=0\noval=%s\nmm=%s\nlay_r=%s\nnow=%s\n", t, t, ovs, mm, lr, t);
    fclose(f); MoveFileExW(w, w2, MOVEFILE_REPLACE_EXISTING);
    snprintf(LV_P, sizeof LV_P, "%s", p); snprintf(LV_T0, sizeof LV_T0, "%s", t); snprintf(LV_OV, sizeof LV_OV, "%s", ovs); snprintf(LV_MM, sizeof LV_MM, "%s", mm); snprintf(LV_LR, sizeof LV_LR, "%s", lr);
    LV_PIECES = 0; LV_N = 0; LV_LAST = GetTickCount64(); sc_piece_hook = lv_piece;          /* live.hook_layout */
}

/* ================================================================ debugging: scene dumps (PIPE_DUMPS set) */
/* the scene right after the layout and as gg3.make leaves it, in scene.h's dump format (the proofs' sstate.py reads
   the same bytes): pipe_debug_dump(0 = layout, 1 = made) gives the last base's. Only with the PIPE_DUMPS variable. */
static Buf DUMP[2];
static Scene *(*LAYOUT0)(const char *, int, const char *, const char *, PyRandom *, int *);
static void dump_one(int which, const Scene *s) {
    DumpCtx *d; DUMP[which].n = 0; d = dump_begin(&DUMP[which]); dump_raw(d, 'l', 1); dump_scene(d, s); dump_end(d);
}
static Scene *layout_dumped(const char *style, int n, const char *salt, const char *reseed, PyRandom *mixr, int *err) {
    Scene *s = LAYOUT0 ? LAYOUT0(style, n, salt, reseed, mixr, err) : ly_ground(style, n, salt, reseed, mixr, NULL, err);
    if (s) dump_one(0, s);
    return s;
}
/* PIPE_TRACE: one line per detail call (qualname, rng digest before / after, scene counts after) */
extern void (*sf_trace)(const ScFn *f, Scene *s, PyRandom *rng, int after);
static Buf TRACE;
static void trace_put(const char *t) { size_t n = strlen(t); if (TRACE.n + n + 1 > TRACE.cap) { TRACE.cap = (TRACE.n + n + 1) * 2 + 4096; TRACE.b = (unsigned char *)realloc(TRACE.b, TRACE.cap); } memcpy(TRACE.b + TRACE.n, t, n); TRACE.n += n; }
static void trace_fn(const ScFn *f, Scene *s, PyRandom *rng, int after) {
    char t[512]; uint32_t h = 0; for (int i = 0; i < 624; i++) h += rng->mt[i];
    if (!after) { Buf b = { 0 }; DumpCtx *d = dump_begin(&b); char md[33]; dump_raw(d, 'l', 1); dump_scene(d, s); dump_end(d); mg_md5_hex(b.b, b.n, md); free(b.b); snprintf(t, sizeof t, "%s:%s before %.12s rng %u/%d", f->qualname, f->tag.shape ? f->tag.shape : "-", md, h, rng->index); }
    else {
        Buf b = { 0 }; DumpCtx *d = dump_begin(&b); char md[33]; dump_raw(d, 'l', 1); dump_scene(d, s); dump_end(d); mg_md5_hex(b.b, b.n, md); free(b.b);
        snprintf(t, sizeof t, " -> rng %u/%d ops %d gcuts %d tcuts %d feet %d kc %lld scene %.12s\n", h, rng->index, s->ops.n, s->gcuts.n, s->tcuts.n, s->feet.n, (long long)(s->has_kc ? s->kc : -1), md);
    }
    trace_put(t);
}
size_t pipe_debug_trace(const unsigned char **p) { *p = TRACE.b; return TRACE.n; }
size_t pipe_debug_dump(int which, const unsigned char **p) { *p = DUMP[which & 1].b; return DUMP[which & 1].n; }
static void debug_hooks(void) { if (getenv("PIPE_DUMPS") && MKC.layout != layout_dumped) { LAYOUT0 = MKC.layout; MKC.layout = layout_dumped; } if (getenv("PIPE_TRACE")) { sf_trace = trace_fn; TRACE.n = 0; } }

/* ================================================================ one request */
static const char *req_s(const KVal *r, const char *k, const char *d) { const char *s = kv_s(r, k); return s ? s : d; }
static int req_i(const KVal *r, const char *k, int d) { KVal *v = kv_get(r, k); return v && kv_is_num(v) ? (int)kv_num(v) : d; }
static void finish_base(Scene *s, CpOut *out) {
    if (s) scene_free(s);
    cp_out_free(out); ft_steps_release(); sf_s3d = NULL; bs_s3d = NULL; ns_s3d = NULL;
    so_true3d_clear();          /* the shells of this base are built: TRUE3D / TOP3 / the smooth chains start empty for the next */
    ork_clear();                /* and the rock pieces build_cmds handed over */
    if (getenv("PIPE_SETS")) {          /* debugging: the kind sets as this base leaves them */
        const SmStrSet *S[4] = { KD.terrk, KD.chamfer, SM.terrk, SM.chamfer }; const char *N[4] = { "KD.terrk", "KD.chamfer", "SM.terrk", "SM.chamfer" };
        for (int k = 0; k < 4; k++) { fprintf(stderr, "%s (%d):", N[k], S[k] ? S[k]->n : -1); for (int i = 0; S[k] && i < S[k]->n; i++) fprintf(stderr, " %s", S[k]->v[i]); fprintf(stderr, "\n"); }
    }
}
/* layout_bridge.layout_choice */
static void layout_choice(const char *style, const KVal *req, MgBuf *o) {
    int n = req_i(req, "n", 0), err = 0, km, bad = 0; CpOut out; Scene *s; char t[256]; BcScene b; BcGroups gs = { 0 }; MgPlan sp; int has_sp;
    const GEOSGeometry **pads; int np; MgBuf blob = { 0 }; MgStrs errs = { 0 }; MgJ h;
    snprintf(t, sizeof t, "choice %s #%d", MKC.salt ? MKC.salt : "", n); mg_log_base(t); km = mg_log_mark();
    memset(&out, 0, sizeof out); sf_s3d = bs_s3d = ns_s3d = &out.s3d;
    debug_hooks();
    s = mk_make(style, n, &out, &err);
    if (s && getenv("PIPE_DUMPS")) dump_one(1, s);
    if (!s) {
        snprintf(t, sizeof t, "choice %d", n); mg_log_caught(t, "Exception", "gg3.make raised in the C kit", "");
        why(o, 0, err == CP_EXIT ? "SystemExit: " : "Exception: gg3.make raised in the C kit"); finish_base(NULL, &out); return;
    }
    to_bc(s, &b);
    bc_build_cmds(&b, &gs); bc_groups_free(&gs);          /* try: gg.build_cmds(s) except Exception: pass */
    np = stand_pads(&out, &pads);
    has_sp = mg_scale_plan(&ENV, pads, np, &sp) == 1;
    scene_blob(&b, s->ground ? s->ground : "", has_sp, &sp, 1, &blob, &bad);
    if (has_sp) mg_plan_free(&sp);
    mg_log_since(km, &errs);
    h = mgj_dict(); mgj_set(&h, "ok", mgj_int(1)); mgj_set(&h, "theme", mgj_str(out.theme && out.theme[0] ? out.theme : style));
    mgj_set(&h, "py_drawn", mgj_int(0)); mgj_set(&h, "errors", strs_j(&errs));
    answer1(o, &h, &blob);
    mgj_free(&h); mg_strs_free(&errs); free(blob.b); free(pads); bc_scene_free(&b);
    finish_base(s, &out);
}
/* layout_bridge.layout_build */
static char RESEED_B[256], NAME_B[256]; static char *PLAN_B;
static void layout_build(const char *style, const KVal *req, MgBuf *o) {
    int n = req_i(req, "n", 0), err = 0, km, bad = 0, np, r; CpOut out; Scene *s; BcScene b; MgBuilt bt; MgBuf bbd = { 0 }; MgStrs errs = { 0 };
    const GEOSGeometry **pads; MgJ h, rep; char t[256]; const char *salt_keep = MKC.salt; KVal *plan = kv_get(req, "plan");
    snprintf(RESEED_B, sizeof RESEED_B, "%s", req_s(req, "variant", "")); snprintf(NAME_B, sizeof NAME_B, "%s", req_s(req, "name", ""));
    MKC.reseed = CPC.reseed = RESEED_B;
    free(PLAN_B); PLAN_B = NULL;
    if (plan && !(plan->t == KV_NONE) && !(plan->t == KV_DICT && !plan->n) && !(plan->t == KV_LIST && !plan->n) && !(plan->t == KV_STR && !plan->s[0])) { MgJ pj = kv2j(plan); MgBuf pb = { 0 }; mgj_dump(&pj, 0, &pb); mg_buf_add(&pb, "", 1); PLAN_B = (char *)pb.b; mgj_free(&pj); }
    CPC.plan_json = PLAN_B;
    mg_log_base(NAME_B[0] ? NAME_B : style); km = mg_log_mark();
    live_start(n);
    memset(&out, 0, sizeof out); sf_s3d = bs_s3d = ns_s3d = &out.s3d;
    debug_hooks();
    s = mk_make(style, n, &out, &err);
    lv_end();
    if (s && getenv("PIPE_DUMPS")) dump_one(1, s);
    if (!s) {
        why(o, 1, err == CP_EXIT ? "crash: SystemExit: " : "crash: Exception: gg3.make raised in the C kit"); finish_base(NULL, &out);
        MKC.salt = CPC.salt = salt_keep; MKC.reseed = CPC.reseed = RESEED0 ? RESEED0 : ""; CPC.plan_json = NULL; return;
    }
    if (NAME_B[0]) { free(s->name); s->name = _strdup(NAME_B); }
    to_bc(s, &b);
    np = stand_pads(&out, &pads);
    ork_clear();
    r = mg_build_input(&b, style, &ENV, pads, np, runner_cfg_num("sketch_tolerance_mm", 0.03), runner_cfg_num("touching_tolerance_mm", 0.005), &bt);
    if (r != 0) {
        snprintf(t, sizeof t, "crash: %.100s", mg_err[0] ? mg_err : "RuntimeError: no base in the command groups"); why(o, 1, t);
    } else {
        double fin = ENV.final_mm, sc = ENV.scale; S3Shells sh = { 0 }; MgStrs sk3 = { 0 }; MgBuf bbs = { 0 };
        if (out.s3d.n && !(fin && fabs(sc - 1) > 1e-6)) {          /* mesher.build: add_solids(s, m, skipped, R0 - H * rim_tan - 0.12) unless a scaled final base */
            int64_t *alive = (int64_t *)malloc(sizeof(int64_t) * (size_t)(s->ops.n + 1)); int na = 0;
            for (int i = 0; i < s->ops.n; i++) { const Kind *k = s->ops.v[i]->kind; if (k && k->has_ghost && k->ghost.t != NUM_NONE) alive[na++] = (int64_t)k->ghost.v; }
            s3_add_solids(&out.s3d, alive, na, ENV.has_oval, ENV.oval[0], ENV.oval[1], bt.R0 - bt.H * bt.rim_tan - 0.12, &sh, &sk3);
            free(alive);
        }
        SmShellXf xf; memset(&xf, 0, sizeof xf);          /* smooth details / organic rocks: the scaled base's map (mesher.rescale) */
        if (bt.has_plan && !(ENV.has_oval && fabs(bt.plan.sc - 1) < 1e-6 && !bt.plan.tx && !bt.plan.ty)) { xf.on = 1; xf.f = bt.plan.sc; xf.tx = bt.plan.tx; xf.ty = bt.plan.ty; xf.H = bt.H; xf.cap = 25.0; xf.floor_ = 2.9; }
        for (int i = 0; SO.smooth && i < out.s3d.n; i++) if (out.s3d.v[i].zlo > bt.H) out.s3d.v[i].zlo = bt.H;          /* every shell goes down below the deck (inside the base's solid), also one standing on something: nothing under it can be air */
        if (out.s3d.n && SO.smooth) {          /* smooth details: the smooth shells (smooth3d.c), scaled bases too */
            int64_t *alive = (int64_t *)malloc(sizeof(int64_t) * (size_t)(s->ops.n + 1)); GEOSGeometry **reg = (GEOSGeometry **)calloc((size_t)(s->ops.n + 1), sizeof(GEOSGeometry *)); int na = 0;
            for (int i = 0; i < s->ops.n; i++) {          /* each smooth chain once, with the union of its ghost ops' outlines */
                const Op *op = s->ops.v[i]; const Kind *k = op->kind; int64_t c; int j;
                if (!k || !k->has_ghost || k->ghost.t == NUM_NONE) continue;
                c = (int64_t)k->ghost.v;
                for (j = 0; j < na && alive[j] != c; j++) {}
                if (j == na) { alive[na] = c; reg[na] = NULL; na++; }
                for (int p = 0; p < op->polys.n; p++) {
                    const GEOSGeometry *g = op->polys.v[p]->g; GEOSGeometry *u;
                    if (!g) continue;
                    u = reg[j] ? shp_union(reg[j], g) : shp_clone(g);
                    if (u) { shp_free(reg[j]); reg[j] = u; }
                }
            }
            sm_shell_add_all(&out.s3d, alive, (const GEOSGeometry *const *)reg, na, &xf, ENV.has_oval, ENV.oval[0], ENV.oval[1], bt.R0 - bt.H * bt.rim_tan - 0.12, &sh, &sk3);
            for (int j = 0; j < na; j++) shp_free(reg[j]);
            free(alive); free(reg);
        }
        if (ork_on) { int nr = 0; ork_build(&xf, bt.H, ENV.has_oval, ENV.oval[0], ENV.oval[1], bt.R0 - bt.H * bt.rim_tan - 0.12, &sh, &sk3, &nr); ork_clear(); }          /* organic rocks (rocks3d.c) */
        s3_blob(&sh, &bbs);
        scene_blob(&b, s->ground ? s->ground : "", bt.has_plan, &bt.plan, 0, &bbd, &bad);
        h = mgj_dict();
        mgj_set(&h, "ok", mgj_int(1)); mgj_set(&h, "name", mgj_str(s->name)); mgj_set(&h, "sig", mgj_str(bt.sig)); mgj_set(&h, "skipped", strs_j(&bt.skipped));
        mgj_set(&h, "steps", mgj_int(bt.steps)); mgj_set(&h, "variant", mgj_str(RESEED_B)); mgj_set(&h, "want_mm", mgj_float(bt.want_mm)); mgj_set(&h, "H", mgj_float(bt.H));
        mgj_set(&h, "threads", mgj_int(threads_kit())); mgj_set(&h, "nsteps", mgj_int(bt.nsteps));
        { const char *cf = getenv("KIT_CFIELD"); mgj_set(&h, "cfield", mgj_bool(cf && !strcmp(cf, "1"))); }
        mgj_set(&h, "solids3d", mgj_int(sh.n)); mgj_set(&h, "skipped3d", strs_j(&sk3)); mgj_set(&h, "py_draw", mgj_int(0));
        {   /* head.update(gg3.scene_report(s)) */
            const char *lg[8]; int nl = 0; MgJ pj = kv2j(out.plan);
            for (int i = 0; out.scene_log && i < out.scene_log->n && nl < 8; i++) if (out.scene_log->v[i]->t == KV_STR) lg[nl++] = out.scene_log->v[i]->s;
            mg_scene_report(out.theme ? out.theme : "", out.plan ? &pj : NULL, out.plan_line ? out.plan_line : "", lg, nl, &rep);
            for (int i = 0; i < rep.n; i++) { MgJ v = rep.v[i]; mgj_set(&h, rep.k[i], v); rep.v[i] = mgj_null(); }
            mgj_free(&rep); mgj_free(&pj);
        }
        mg_log_since(km, &errs); mgj_set(&h, "errors", strs_j(&errs));
        { char ef[MAX_PATH * 3]; const char *e = getenv("KIT_ERRORS_DIR"); if (e && *e) snprintf(ef, sizeof ef, "%s\\kit_errors.log", e); else snprintf(ef, sizeof ef, "%s\\out\\kit_errors.log", runner_kit()); mgj_set(&h, "errors_file", mgj_str(ef)); }
        { const MgBuf *bl[3] = { &bt.bbf, &bbd, &bbs }; answer2(o, &h, bl, 3); }
        mgj_free(&h); mg_strs_free(&errs); mg_strs_free(&sk3); s3_shells_free(&sh); free(bbs.b);
        mg_built_free(&bt);
    }
    free(bbd.b); free(pads); bc_scene_free(&b);
    finish_base(s, &out);
    /* finally: SALT, RESEED, BASENAME, PLAN_JSON as they were */
    MKC.salt = CPC.salt = salt_keep; MKC.reseed = CPC.reseed = RESEED0 ? RESEED0 : ""; CPC.plan_json = NULL;
}

int pipe_request(const char *mode, const char *style, const char *line, unsigned char **outp, size_t *nout) {
    MgBuf o = { 0 }; KVal *req; int build = !strcmp(mode, "build");
    *outp = NULL; *nout = 0;
    if (!setup(mode)) { why(&o, build, "crash: RuntimeError: the C kit could not start (GEOS)"); *outp = o.b; *nout = o.n; return 1; }
    req = kv_parse_json(line);
    if (!req || req->t != KV_DICT) why(&o, build, build ? "crash: ValueError: a bad request" : "ValueError: a bad request");
    else if (build) layout_build(style, req, &o);
    else layout_choice(style, req, &o);
    kv_free(req);
    *outp = o.b; *nout = o.n;
    return 0;
}

int pipe_serve(const char *mode, const char *style) {
    HANDLE in = GetStdHandle(STD_INPUT_HANDLE), out = GetStdHandle(STD_OUTPUT_HANDLE);
    char *line = NULL; size_t cap = 0, len = 0; char buf[4096]; DWORD got;
    for (;;) {
        if (!ReadFile(in, buf, sizeof buf, &got, NULL) || !got) break;
        for (DWORD i = 0; i < got; i++) {
            if (buf[i] == '\n') {
                if (len + 1 > cap) { cap = (len + 1) * 2; line = (char *)realloc(line, cap); }
                line[len] = 0;
                while (len && (line[len - 1] == '\r' || line[len - 1] == ' ')) line[--len] = 0;
                if (len) {
                    unsigned char *a; size_t na; DWORD put; size_t off = 0;
                    pipe_request(mode, style, line, &a, &na);
                    while (off < na) { if (!WriteFile(out, a + off, (DWORD)(na - off > (1u << 20) ? (1u << 20) : na - off), &put, NULL) || !put) { free(a); free(line); return 1; } off += put; }
                    free(a);
                }
                len = 0;
            } else { if (len + 2 > cap) { cap = (len + 2) * 2 + 256; line = (char *)realloc(line, cap); } line[len++] = buf[i]; }
        }
    }
    free(line);
    return 0;
}
