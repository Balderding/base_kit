/* bld_sd_models.c: the model details (sd_r_models.py _model / _fine with the tables of sd_r_models.py and
   sd_r_models_a/b/c/d.py) in C, and the shared machinery of the bld_sd_* files (bld_sdde.h): the per-call arena
   (bsdde_run), registration, the settings, the stub for builders a later file replaces. Agent "bld-sdde", 2026-11-19.
   Proof: work\cscene\bld_sdde (prove.py: real calls recorded inside real bases + random cases, footprint, layers with
   every steps layer filled, the rng state and the chain counter compared byte for byte). */
#include <stdio.h>
#include "bld_sdde.h"
#include "models.h"

_Thread_local BA *bsdde_ba;

/* ================================================================== the arena */
int bsdde_run(int (*body)(double, double, double, SoLayers *, GG **), double cx, double cy, double a, PyRandom *rng, Geo **foot, SoLayers *lay) {
    BA ba; BA *prev = BA_; GG *f = NULL; int r, n0 = lay->n, i;
    memset(&ba, 0, sizeof ba); ba.rng = rng; BA_ = &ba;
    *foot = NULL;
    r = body(cx, cy, a, lay, &f);
    if (ba.err) r = SC_ERR;
    if (r == 0) *foot = f ? geo_new(shp_clone(f)) : NULL;
    else { for (i = n0; i < lay->n; i++) ly_unref(lay->v[i]); lay->n = n0; }          /* raised / None: nothing handed out */
    for (i = 0; i < ba.ny; i++) ly_unref(ba.y[i]);
    for (i = 0; i < ba.no; i++) { so_obj_free(ba.o[i]); free(ba.o[i]); }
    for (i = 0; i < ba.ns; i++) { sol_free(ba.s[i]); free(ba.s[i]); }
    for (i = 0; i < ba.ng; i++) shp_free(ba.g[i]);
    for (i = 0; i < ba.nm; i++) { free(*(void **)ba.m[i]); free(ba.m[i]); }          /* PT / GL / CL: the array, then the list */
    free(ba.y); free(ba.o); free(ba.s); free(ba.g); free(ba.m);
    BA_ = prev;
    return r;
}

/* style_details.ROCK_T, SOFT_T, PEB_T, SPIKE_T, ROUND_T */
#define TI(f, n) { { (f), NUM_FLOAT, 0 }, { (n), NUM_INT, 0 } }          /* (float, int) as written in style_details */
const FtTiers BSDDE_ROCK_T = { 2, { TI(0.7, 6), TI(0.3, 40) } };
const FtTiers BSDDE_SOFT_T = { 2, { TI(0.55, 25), TI(0.45, 50) } };
const FtTiers BSDDE_PEB_T = { 2, { TI(0.7, 10), TI(0.3, 45) } };
const FtTiers BSDDE_SPIKE_T = { 2, { TI(0.75, 4), TI(0.25, 30) } };
const FtTiers BSDDE_ROUND_T = { 3, { TI(0.45, 12), TI(0.3, 38), TI(0.25, 62) } };

/* d(style, cls, kind, name, r, ground)(f0) */
void bsdde_reg(const char *style, const char *cls, const char *kind, const char *name, double r, int ground, FtBuildFn f0, void *ctx) {
    SdrEntry e; e.f0 = f0; e.ctx = ctx; e.style = style; e.cls = cls; e.kind = kind; e.name = name; e.r = r; e.ground = ground;
    sdr_register(&e);
}
/* A builder that a later file of the kit draws again (the same style and name: sd_reg.d keeps the first one's place
   in REG[style] and the later one's function). Only its place is needed, so the first draft is not copied; with
   every part registered it is never called. Called anyway (a later part left out of the build), it raises. */
int bsdde_replaced(void *ctx, double cx, double cy, double a, PyRandom *rng, int hs, double sc, Geo **foot, SoLayers *lay) {
    (void)ctx; (void)cx; (void)cy; (void)a; (void)rng; (void)hs; (void)sc; (void)lay; *foot = NULL; return SC_ERR;
}

/* ================================================================== settings */
static char ROOT[1024] = "models"; static int HAVE_MH; static double MH;
/* models_root = sd_mesh.ROOT (<kit>\models; not read when a model blob is in use: mdl_blob_use); max_height =
   gg.max_height() for this base (gg.max_height reads gg.BASE_MM and CFG). Without a call: CFG's defaults (3.3 mm,
   x1.5 from 59.9 mm) on SC.base_mm. Set it before bld_register_all (the files' presence is read there). */
void bsdde_config(const char *models_root, double max_height) {
    if (models_root) { strncpy(ROOT, models_root, sizeof ROOT - 1); ROOT[sizeof ROOT - 1] = 0; }
    HAVE_MH = 1; MH = max_height;
}
static double max_height(void) { return HAVE_MH ? MH : 3.3 * (SC.base_mm >= 59.9 ? 1.5 : 1.0); }

/* ================================================================== sd_r_models._model */
typedef struct { const char *rel; double k; } MWith;
typedef struct {
    const char *style, *name, *rel, *kind; double mm0, mm1; const char *poses, *cls; double sink0, sink1, tilt, cap;
    int capmin;          /* models_b / models_d: cap = min(cap, gg.max_height() * 0.92) (done per call, see below) */
    int nwith; MWith with_[3];
} MRow;
/* the rows, in the order Python makes them (written by work\cscene\bld_sdde\gen_models.py from the Python tables) */
static const MRow ROWS[] = {
/* BEGIN MODEL TABLE */
    {"graveyard", "m_gravestone_cross", "graveyard-kit/Models/OBJ format/gravestone-cross.obj", "headstone", 4.2, 5.2, "sbbf", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* sd_r_models */
    {"graveyard", "m_gravestone_round", "graveyard-kit/Models/OBJ format/gravestone-round.obj", "headstone", 4.6, 5.6, "ssbf", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* sd_r_models */
    {"graveyard", "m_gravestone_bevel", "graveyard-kit/Models/OBJ format/gravestone-bevel.obj", "headstone", 4.6, 5.6, "ssbf", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* sd_r_models */
    {"graveyard", "m_gravestone_roof", "graveyard-kit/Models/OBJ format/gravestone-roof.obj", "headstone", 4.6, 5.6, "ssb", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* sd_r_models */
    {"graveyard", "m_gravestone_wide", "graveyard-kit/Models/OBJ format/gravestone-wide.obj", "headstone", 4.6, 5.6, "ssb", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* sd_r_models */
    {"graveyard", "m_gravestone_fancy", "graveyard-kit/Models/OBJ format/gravestone-decorative.obj", "headstone", 5.0, 6.0, "ssbf", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* sd_r_models */
    {"graveyard", "m_gravestone_broken", "graveyard-kit/Models/OBJ format/gravestone-broken.obj", "headstone", 5.0, 6.0, "s", "made", 0.0, 0.0, 0.22, 0.0, 0, 1, {{"graveyard-kit/Models/OBJ format/gravestone-debris.obj", 1.0}}},          /* sd_r_models */
    {"graveyard", "m_wood_cross", "graveyard-kit/Models/OBJ format/cross-wood.obj", "wood", 4.2, 5.0, "bbfs", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* sd_r_models */
    {"graveyard", "m_stone_cross", "graveyard-kit/Models/OBJ format/cross.obj", "headstone", 4.2, 5.0, "bbfs", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* sd_r_models */
    {"graveyard", "m_coffin", "graveyard-kit/Models/OBJ format/coffin.obj", "wood", 7.0, 8.5, "s", "made", 0.0, 0.3, 0.22, 0.0, 0, 0, {}},          /* sd_r_models */
    {"graveyard", "m_coffin_old", "graveyard-kit/Models/OBJ format/coffin-old.obj", "wood", 7.0, 8.5, "s", "made", 0.0, 0.3, 0.22, 0.0, 0, 0, {}},          /* sd_r_models */
    {"graveyard", "m_lantern", "graveyard-kit/Models/OBJ format/lantern-candle.obj", "iron", 6.0, 7.5, "sslr", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* sd_r_models */
    {"graveyard", "m_lantern_glass", "graveyard-kit/Models/OBJ format/lantern-glass.obj", "iron", 6.0, 7.5, "sslr", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* sd_r_models */
    {"graveyard", "m_urn", "graveyard-kit/Models/OBJ format/urn-round.obj", "headstone", 6.0, 7.5, "sslr", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* sd_r_models */
    {"graveyard", "m_urn_square", "graveyard-kit/Models/OBJ format/urn-square.obj", "headstone", 6.0, 7.5, "sslr", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* sd_r_models */
    {"graveyard", "m_pumpkin", "graveyard-kit/Models/OBJ format/pumpkin-carved.obj", "shrub", 6.0, 8.0, "s", "made", 0.0, 0.0, 0.22, 0.0, 0, 1, {{"graveyard-kit/Models/OBJ format/pumpkin.obj", 0.8}}},          /* sd_r_models */
    {"graveyard", "m_candles", "graveyard-kit/Models/OBJ format/candle-multiple.obj", "wax", 8.0, 10.0, "s", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* sd_r_models */
    {"graveyard", "m_shovel", "graveyard-kit/Models/OBJ format/shovel-dirt.obj", "iron", 5.5, 6.5, "s", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* sd_r_models */
    {"graveyard", "m_fire_basket", "graveyard-kit/Models/OBJ format/fire-basket.obj", "iron", 7.0, 8.5, "s", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* sd_r_models */
    {"graveyard", "m_obelisk", "graveyard-kit/Models/OBJ format/pillar-obelisk.obj", "headstone", 4.0, 5.0, "sbl", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* sd_r_models */
    {"graveyard", "m_grave", "graveyard-kit/Models/OBJ format/grave.obj", "earth", 5.0, 6.0, "s", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* sd_r_models */
    {"graveyard", "m_bench", "graveyard-kit/Models/OBJ format/bench-damaged.obj", "wood", 5.5, 6.5, "s", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* sd_r_models */
    {"graveyard", "m_chalice", "graveyard-kit/Models/OBJ format/detail-chalice.obj", "gold", 9.0, 11.0, "slr", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* sd_r_models */
    {"graveyard", "m_altar", "graveyard-kit/Models/OBJ format/altar-stone.obj", "headstone", 5.0, 6.0, "s", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* sd_r_models */
    {"dungeon", "m_chest_gold", "kaykit-dungeon/obj/chest_gold.obj", "wood", 2.6, 3.1, "s", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* sd_r_models_a */
    {"dungeon", "m_chest", "mini-dungeon/Models/OBJ format/chest.obj", "wood", 6.5, 8.0, "s", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* sd_r_models_a */
    {"dungeon", "m_trunk", "kaykit-dungeon/obj/trunk_large_A.obj", "wood", 3.0, 3.8, "s", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* sd_r_models_a */
    {"dungeon", "m_barrel", "kaykit-dungeon/obj/barrel_small.obj", "wood", 3.0, 3.6, "ssl", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* sd_r_models_a */
    {"dungeon", "m_coins", "kaykit-dungeon/obj/coin_stack_medium.obj", "gold", 4.0, 5.0, "s", "made", 0.0, 0.0, 0.22, 0.0, 0, 1, {{"kaykit-dungeon/obj/coin.obj", 1.0}}},          /* sd_r_models_a */
    {"dungeon", "m_key", "kaykit-dungeon/obj/key.obj", "iron", 5.0, 6.5, "bf", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* sd_r_models_a */
    {"dungeon", "m_keyring", "kaykit-dungeon/obj/keyring.obj", "iron", 4.5, 5.5, "s", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* sd_r_models_a */
    {"dungeon", "m_bottles", "kaykit-dungeon/obj/bottle_A_brown.obj", "glass", 3.4, 4.0, "ssl", "made", 0.0, 0.0, 0.22, 0.0, 0, 2, {{"kaykit-dungeon/obj/bottle_B_green.obj", 1.0}, {"kaykit-dungeon/obj/bottle_C_brown.obj", 1.0}}},          /* sd_r_models_a */
    {"dungeon", "m_trap", "mini-dungeon/Models/OBJ format/trap.obj", "iron", 6.0, 7.5, "s", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* sd_r_models_a */
    {"dungeon", "m_pot", "mini-dungeon/Models/OBJ format/pot.obj", "brass", 7.0, 8.5, "s", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* sd_r_models_a */
    {"dungeon", "m_shield", "mini-dungeon/Models/OBJ format/shield-round.obj", "iron", 9.0, 11.0, "bf", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* sd_r_models_a */
    {"dungeon", "m_sword", "mini-dungeon/Models/OBJ format/weapon-sword.obj", "iron", 12.0, 14.0, "bf", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* sd_r_models_a */
    {"dungeon", "m_potion", "mini-dungeon/Models/OBJ format/potion.obj", "glass", 7.5, 9.0, "s", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* sd_r_models_a */
    {"fortress", "m_spear", "mini-arena/Models/OBJ format/weapon-spear.obj", "iron", 9.0, 10.5, "bf", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* sd_r_models_a */
    {"fortress", "m_bolt", "tower-defense-kit/Models/OBJ format/weapon-ammo-arrow.obj", "iron", 8.0, 10.0, "s", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* sd_r_models_a */
    {"fortress", "m_barrel", "pirate-kit/Models/OBJ format/barrel.obj", "wood", 2.6, 3.1, "ssl", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* sd_r_models_a */
    {"fortress", "m_barrels", "retro-fantasy-kit/Models/OBJ format/barrels.obj", "wood", 6.0, 7.0, "s", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* sd_r_models_a */
    {"fortress", "m_bedroll", "survival-kit/Models/OBJ format/bedroll.obj", "sack", 8.0, 10.0, "s", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* sd_r_models_a */
    {"fortress", "m_chest", "survival-kit/Models/OBJ format/chest.obj", "wood", 11.0, 13.0, "s", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* sd_r_models_a */
    {"fortress", "m_pot", "mini-dungeon/Models/OBJ format/pot.obj", "clay", 7.0, 8.5, "s", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* sd_r_models_a */
    {"temple", "m_column_stub", "mini-arena/Models/OBJ format/column-damaged.obj", "tcolumn", 5.0, 6.0, "s", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* sd_r_models_a */
    {"temple", "m_pillar_fallen", "kaykit-dungeon/obj/pillar.obj", "tcolumn", 1.2, 1.45, "bl", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* sd_r_models_a */
    {"temple", "m_statue", "mini-arena/Models/OBJ format/statue.obj", "tcolumn", 3.4, 4.0, "s", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* sd_r_models_a */
    {"temple", "m_gold_chest", "kaykit-dungeon/obj/chest_gold.obj", "gold", 2.6, 3.1, "s", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* sd_r_models_a */
    {"temple", "m_pot", "mini-dungeon/Models/OBJ format/pot.obj", "tcolumn", 7.0, 8.5, "s", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* sd_r_models_a */
    {"temple", "m_steps", "mini-arena/Models/OBJ format/stairs.obj", "tflag", 3.5, 4.2, "s", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* sd_r_models_a */
    {"temple", "m_blocks", "mini-arena/Models/OBJ format/bricks.obj", "tcolumn", 5.0, 6.0, "s", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* sd_r_models_a */
    {"temple", "m_brazier", "graveyard-kit/Models/OBJ format/fire-basket.obj", "gold", 7.0, 8.5, "s", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* sd_r_models_a */
    {"temple", "m_altar", "graveyard-kit/Models/OBJ format/altar-stone.obj", "tcolumn", 5.0, 6.0, "s", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* sd_r_models_a */
    {"temple", "m_urn", "graveyard-kit/Models/OBJ format/urn-round.obj", "tcolumn", 7.5, 9.0, "s", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* sd_r_models_a */
    {"temple", "m_obelisk", "graveyard-kit/Models/OBJ format/pillar-obelisk.obj", "tcolumn", 4.0, 5.0, "bl", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* sd_r_models_a */
    {"mine", "m_pickaxe", "survival-kit/Models/OBJ format/tool-pickaxe.obj", "iron", 22.0, 26.0, "bf", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* sd_r_models_a */
    {"mine", "m_shovel", "pirate-kit/Models/OBJ format/tool-shovel.obj", "iron", 3.0, 3.6, "bf", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* sd_r_models_a */
    {"mine", "m_hammer", "survival-kit/Models/OBJ format/tool-hammer-upgraded.obj", "iron", 22.0, 26.0, "bf", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* sd_r_models_a */
    {"mine", "m_barrel", "pirate-kit/Models/OBJ format/barrel.obj", "wood", 2.6, 3.1, "ssl", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* sd_r_models_a */
    {"mine", "m_rocks", "mini-dungeon/Models/OBJ format/stones.obj", "rock", 5.5, 6.5, "s", "debris", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* sd_r_models_a */
    {"mine", "m_ore", "survival-kit/Models/OBJ format/resource-stone-large.obj", "rock", 12.0, 15.0, "s", "debris", 0.0, 0.0, 0.22, 0.0, 0, 1, {{"survival-kit/Models/OBJ format/resource-stone.obj", 1.0}}},          /* sd_r_models_a */
    {"mine", "m_crystals", "tower-defense-kit/Models/OBJ format/detail-crystal-large.obj", "rock", 6.0, 7.0, "s", "debris", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* sd_r_models_a */
    {"mine", "m_ladder", "modular-cave-kit/Models/OBJ format/ladder.obj", "wood", 2.0, 2.4, "bf", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* sd_r_models_a */
    {"mine", "m_wheel", "fantasy-town-kit/Models/OBJ format/wheel.obj", "wood", 8.0, 9.5, "lr", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* sd_r_models_a */
    {"urban", "m_hydrant", "kaykit-city/obj/firehydrant.obj", "iron", 19.0, 22.0, "sssl", "made", 0.0, 0.0, 0.22, 4.6, 1, 0, {}},          /* sd_r_models_b */
    {"urban", "m_dumpster", "kaykit-city/obj/dumpster.obj", "tin", 14.0, 16.0, "s", "made", 0.0, 0.0, 0.22, 4.8, 1, 0, {}},          /* sd_r_models_b */
    {"urban", "m_dumpster_ro", "city-kit-roads/Models/OBJ format/dumpster.obj", "tin", 22.0, 24.0, "s", "made", 0.0, 0.0, 0.22, 4.8, 1, 0, {}},          /* sd_r_models_b */
    {"urban", "m_bench", "retro-urban-kit/Models/OBJ format/detail-bench.obj", "wood", 11.0, 12.5, "s", "made", 0.0, 0.0, 0.22, 4.2, 1, 0, {}},          /* sd_r_models_b */
    {"urban", "m_street_light", "kaykit-city/obj/streetlight.obj", "iron", 10.5, 12.0, "b", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* sd_r_models_b */
    {"urban", "m_tyre", "car-kit/Models/OBJ format/debris-tire.obj", "leather", 7.0, 8.0, "llr", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* sd_r_models_b */
    {"urban", "m_cone", "car-kit/Models/OBJ format/cone.obj", "tin", 6.5, 7.5, "s", "made", 0.0, 0.0, 0.22, 4.4, 1, 0, {}},          /* sd_r_models_b */
    {"urban", "m_concrete_barrier", "retro-urban-kit/Models/OBJ format/detail-barrier-strong-damaged.obj", "concrete", 12.5, 14.0, "s", "made", 0.0, 0.0, 0.22, 4.4, 1, 0, {}},          /* sd_r_models_b */
    {"urban", "m_stop_sign", "city-kit-roads/Models/OBJ format/road-sign-stop.obj", "iron", 19.0, 22.0, "l", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* sd_r_models_b */
    {"urban", "m_axle", "car-kit/Models/OBJ format/debris-drivetrain-axle.obj", "iron", 7.0, 8.0, "s", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* sd_r_models_b */
    {"urban", "m_tank", "city-kit-industrial/Models/OBJ format/detail-tank.obj", "tin", 10.0, 11.5, "s", "made", 0.0, 0.0, 0.22, 4.8, 1, 0, {}},          /* sd_r_models_b */
    {"corridor", "m_container_flat", "space-station-kit/Models/OBJ format/container-flat.obj", "metal", 7.5, 8.5, "s", "made", 0.0, 0.0, 0.22, 4.6, 1, 0, {}},          /* sd_r_models_b */
    {"corridor", "m_container_tall", "space-station-kit/Models/OBJ format/container-tall.obj", "metal", 8.0, 9.0, "lbs", "made", 0.0, 0.0, 0.22, 5.4, 1, 0, {}},          /* sd_r_models_b */
    {"corridor", "m_pipe_bend", "space-station-kit/Models/OBJ format/pipe-bend.obj", "pipe", 13.0, 15.0, "bf", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* sd_r_models_b */
    {"corridor", "m_valve", "factory-kit/Models/OBJ format/pipe-large-valve.obj", "pipe", 5.5, 6.5, "s", "made", 0.0, 0.0, 0.22, 5.2, 1, 0, {}},          /* sd_r_models_b */
    {"corridor", "m_cog", "factory-kit/Models/OBJ format/cog-a.obj", "metal", 6.5, 7.5, "s", "made", 0.0, 0.0, 0.05, 0.0, 0, 0, {}},          /* sd_r_models_b */
    {"corridor", "m_lever", "factory-kit/Models/OBJ format/lever-single.obj", "metal", 11.0, 12.5, "s", "made", 0.0, 0.0, 0.22, 5.0, 1, 0, {}},          /* sd_r_models_b */
    {"corridor", "m_crate", "blaster-kit/Models/OBJ format/crate-small.obj", "metal", 9.5, 10.5, "s", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* sd_r_models_b */
    {"corridor", "m_canisters", "kaykit-space-base/obj/containers_A.obj", "tin", 12.5, 14.0, "s", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* sd_r_models_b */
    {"corridor", "m_holo_table", "space-station-kit/Models/OBJ format/table-display-planet.obj", "glass", 11.5, 13.0, "s", "made", 0.0, 0.0, 0.06, 5.0, 1, 0, {}},          /* sd_r_models_b */
    {"corridor", "m_cables", "modular-space-kit/Models/OBJ format/cables.obj", "cable", 4.6, 5.2, "s", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* sd_r_models_b */
    {"corridor", "m_beacon", "factory-kit/Models/OBJ format/warning-orange.obj", "glass", 5.5, 6.5, "lr", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* sd_r_models_b */
    {"targa", "m_ice_crystals", "tower-defense-kit/Models/OBJ format/snow-detail-crystal-large.obj", "ice", 8.5, 9.5, "s", "debris", 0.0, 0.0, 0.22, 5.0, 1, 0, {}},          /* sd_r_models_b */
    {"targa", "m_ice_shard", "tower-defense-kit/Models/OBJ format/snow-detail-crystal.obj", "ice", 13.0, 15.0, "s", "debris", 0.0, 0.0, 0.22, 5.2, 1, 0, {}},          /* sd_r_models_b */
    {"targa", "m_snow_rocks", "tower-defense-kit/Models/OBJ format/snow-detail-rocks.obj", "ice", 11.0, 13.0, "s", "debris", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* sd_r_models_b */
    {"targa", "m_cargo", "kaykit-space-base/obj/cargo_B_packed.obj", "metal", 10.0, 11.5, "s", "made", 0.0, 0.25, 0.22, 5.0, 1, 0, {}},          /* sd_r_models_b */
    {"targa", "m_canisters", "kaykit-space-base/obj/containers_B.obj", "metal", 12.5, 14.0, "s", "made", 0.0, 0.2, 0.22, 0.0, 0, 0, {}},          /* sd_r_models_b */
    {"targa", "m_lights", "kaykit-space-base/obj/lights.obj", "metal", 8.0, 9.0, "b", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* sd_r_models_b */
    {"targa", "m_skip", "space-station-kit/Models/OBJ format/skip.obj", "metal", 6.5, 7.5, "s", "made", 0.0, 0.2, 0.22, 4.6, 1, 0, {}},          /* sd_r_models_b */
    {"targa", "m_tank", "space-station-kit/Models/OBJ format/container-tall.obj", "metal", 8.0, 9.0, "lb", "made", 0.0, 0.2, 0.22, 0.0, 0, 0, {}},          /* sd_r_models_b */
    {"targa", "m_crate", "blaster-kit/Models/OBJ format/crate-small.obj", "metal", 9.5, 10.5, "s", "made", 0.0, 0.2, 0.22, 0.0, 0, 0, {}},          /* sd_r_models_b */
    {"maahl", "m_sand_rock", "survival-kit/Models/OBJ format/rock-sand-a.obj", "sandstone", 10.5, 12.0, "s", "debris", 0.0, 0.0, 0.22, 5.0, 1, 0, {}},          /* sd_r_models_b */
    {"maahl", "m_sand_rock_b", "survival-kit/Models/OBJ format/rock-sand-b.obj", "sandstone", 8.5, 10.0, "s", "debris", 0.0, 0.0, 0.22, 5.0, 1, 0, {}},          /* sd_r_models_b */
    {"maahl", "m_sand_rock_c", "survival-kit/Models/OBJ format/rock-sand-c.obj", "sandstone", 8.5, 10.0, "s", "debris", 0.0, 0.0, 0.22, 5.4, 1, 0, {}},          /* sd_r_models_b */
    {"maahl", "m_crystals", "tower-defense-kit/Models/OBJ format/detail-crystal-large.obj", "crystal", 8.5, 9.5, "s", "debris", 0.0, 0.0, 0.22, 5.0, 1, 0, {}},          /* sd_r_models_b */
    {"maahl", "m_crystal_rock", "space-kit/Models/OBJ format/rock_crystals.obj", "crystal", 9.0, 10.5, "s", "debris", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* sd_r_models_b */
    {"maahl", "m_crater", "space-kit/Models/OBJ format/crater.obj", "sandstone", 10.5, 12.0, "s", "debris", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* sd_r_models_b */
    {"maahl", "m_meteor", "space-kit/Models/OBJ format/meteor_half.obj", "sandstone", 8.5, 10.0, "s", "debris", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* sd_r_models_b */
    {"maahl", "m_rocks", "kaykit-space-base/obj/rocks_A.obj", "sandstone", 5.5, 6.5, "s", "debris", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* sd_r_models_b */
    {"maahl", "m_stones", "tower-defense-kit/Models/OBJ format/detail-rocks.obj", "sandstone", 11.0, 13.0, "s", "debris", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* sd_r_models_b */
    {"trederra", "m_barrel", "survival-kit/Models/OBJ format/barrel.obj", "wood", 14.0, 16.0, "lr", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* sd_r_models_b */
    {"trederra", "m_shovel", "survival-kit/Models/OBJ format/tool-shovel.obj", "iron", 27.0, 30.0, "bf", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* sd_r_models_b */
    {"trederra", "m_pickaxe", "survival-kit/Models/OBJ format/tool-pickaxe.obj", "iron", 28.0, 32.0, "bf", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* sd_r_models_b */
    {"trederra", "m_bedroll_packed", "survival-kit/Models/OBJ format/bedroll-packed.obj", "cloth", 21.0, 24.0, "s", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* sd_r_models_b */
    {"trederra", "m_duckboard", "survival-kit/Models/OBJ format/floor-old.obj", "wood", 14.0, 15.5, "s", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* sd_r_models_b */
    {"trederra", "m_ammo_crate", "blaster-kit/Models/OBJ format/crate-medium.obj", "wood", 8.5, 9.5, "s", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* sd_r_models_b */
    {"trederra", "m_campfire", "survival-kit/Models/OBJ format/campfire-pit.obj", "wood", 19.0, 22.0, "s", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* sd_r_models_b */
    {"trederra", "m_grenade", "blaster-kit/Models/OBJ format/grenade-a.obj", "iron", 17.0, 19.0, "lrs", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* sd_r_models_b */
    {"trederra", "m_tin_sheet", "survival-kit/Models/OBJ format/metal-panel.obj", "tin", 13.0, 15.0, "bf", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* sd_r_models_b */
    {"trederra", "m_chest", "survival-kit/Models/OBJ format/chest.obj", "wood", 16.0, 18.0, "s", "made", 0.0, 0.0, 0.22, 4.6, 1, 0, {}},          /* sd_r_models_b */
    {"cynder", "m_meteor", "space-kit/Models/OBJ format/meteor_detailed.obj", "basalt", 7.5, 8.5, "s", "debris", 0.1, 0.35, 0.22, 5.2, 1, 0, {}},          /* sd_r_models_b */
    {"cynder", "m_crater", "space-kit/Models/OBJ format/craterLarge.obj", "ash", 10.0, 11.5, "s", "debris", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* sd_r_models_b */
    {"cynder", "m_shards", "space-kit/Models/OBJ format/rock_crystalsLargeA.obj", "obsidian", 8.5, 9.5, "s", "debris", 0.0, 0.0, 0.22, 5.2, 1, 0, {}},          /* sd_r_models_b */
    {"cynder", "m_boulders", "tower-defense-kit/Models/OBJ format/detail-rocks-large.obj", "basalt", 8.5, 9.5, "s", "debris", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* sd_r_models_b */
    {"forest", "m_log", "nature-kit/Models/OBJ format/log.obj", "log", 12.0, 15.0, "s", "debris", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* sd_r_models_c */
    {"forest", "m_tree_log", "survival-kit/Models/OBJ format/tree-log.obj", "log", 9.5, 11.5, "s", "debris", 0.0, 0.0, 0.22, 3.4, 0, 0, {}},          /* sd_r_models_c */
    {"forest", "m_fallen_tree", "kaykit-halloween/obj/tree_dead_small.obj", "log", 3.2, 3.9, "bf", "debris", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* sd_r_models_c */
    {"forest", "m_campfire", "nature-kit/Models/OBJ format/campfire_stones.obj", "rock", 13.0, 16.0, "s", "made", 0.0, 0.0, 0.0, 0.0, 0, 0, {}},          /* sd_r_models_c */
    {"forest", "m_fire_pit", "survival-kit/Models/OBJ format/campfire-pit.obj", "rock", 22.0, 27.0, "s", "made", 0.0, 0.0, 0.0, 0.0, 0, 0, {}},          /* sd_r_models_c */
    {"forest", "m_rocks", "mini-forest/Models/OBJ format/stones.obj", "rock", 8.0, 10.0, "s", "debris", 0.0, 0.0, 0.0, 4.5, 0, 0, {}},          /* sd_r_models_c */
    {"forest", "m_axe", "survival-kit/Models/OBJ format/tool-axe.obj", "wood", 28.0, 34.0, "bf", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* sd_r_models_c */
    {"swamp", "m_canoe", "nature-kit/Models/OBJ format/canoe.obj", "log", 8.5, 10.2, "s", "made", 0.0, 0.05, 0.22, 0.0, 0, 0, {}},          /* sd_r_models_c */
    {"swamp", "m_paddle", "pirate-kit/Models/OBJ format/tool-paddle.obj", "log", 3.6, 4.4, "bf", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* sd_r_models_c */
    {"swamp", "m_rowboat", "pirate-kit/Models/OBJ format/boat-row-small.obj", "log", 3.4, 3.9, "s", "made", 0.0, 0.25, 0.12, 0.0, 0, 0, {}},          /* sd_r_models_c */
    {"swamp", "m_log", "nature-kit/Models/OBJ format/log.obj", "log", 12.0, 15.0, "s", "made", 0.0, 0.25, 0.22, 0.0, 0, 0, {}},          /* sd_r_models_c */
    {"swamp", "m_tree_log", "survival-kit/Models/OBJ format/tree-log.obj", "log", 9.5, 11.5, "s", "made", 0.0, 0.25, 0.22, 3.4, 0, 0, {}},          /* sd_r_models_c */
    {"swamp", "m_dead_tree", "kaykit-halloween/obj/tree_dead_small.obj", "log", 3.2, 3.9, "bf", "made", 0.0, 0.2, 0.22, 0.0, 0, 0, {}},          /* sd_r_models_c */
    {"swamp", "m_skull", "kaykit-halloween/obj/skull.obj", "bone", 4.6, 5.6, "b", "made", 0.0, 0.2, 0.22, 0.0, 0, 0, {}},          /* sd_r_models_c */
    {"swamp", "m_bone", "kaykit-halloween/obj/bone_C.obj", "bone", 6.0, 7.5, "s", "made", 0.0, 0.2, 0.22, 0.0, 0, 0, {}},          /* sd_r_models_c */
    {"swamp", "m_bone_knob", "kaykit-halloween/obj/bone_B.obj", "bone", 8.0, 10.0, "s", "made", 0.0, 0.2, 0.22, 0.0, 0, 0, {}},          /* sd_r_models_c */
    {"swamp", "m_fish", "survival-kit/Models/OBJ format/fish-large.obj", "hide", 22.0, 27.0, "bf", "made", 0.0, 0.0, 0.1, 0.0, 0, 0, {}},          /* sd_r_models_c */
    {"coast", "m_rowboat", "pirate-kit/Models/OBJ format/boat-row-small.obj", "drift", 3.4, 3.9, "s", "made", 0.0, 0.25, 0.12, 0.0, 0, 0, {}},          /* sd_r_models_c */
    {"coast", "m_barrel", "pirate-kit/Models/OBJ format/barrel.obj", "drift", 3.4, 4.2, "s", "made", 0.0, 0.15, 0.22, 5.2, 0, 0, {}},          /* sd_r_models_c */
    {"coast", "m_keg", "survival-kit/Models/OBJ format/barrel.obj", "drift", 17.0, 21.0, "lr", "made", 0.1, 0.35, 0.22, 0.0, 0, 0, {}},          /* sd_r_models_c */
    {"coast", "m_crate_bottles", "pirate-kit/Models/OBJ format/crate-bottles.obj", "glass", 4.2, 5.0, "s", "made", 0.0, 0.0, 0.1, 0.0, 0, 0, {}},          /* sd_r_models_c */
    {"coast", "m_chest", "pirate-kit/Models/OBJ format/chest.obj", "brass", 3.8, 4.6, "s", "made", 0.0, 0.2, 0.12, 5.2, 0, 0, {}},          /* sd_r_models_c */
    {"coast", "m_bottle", "pirate-kit/Models/OBJ format/bottle.obj", "glass", 6.5, 8.0, "blr", "made", 0.0, 0.2, 0.22, 0.0, 0, 0, {}},          /* sd_r_models_c */
    {"coast", "m_paddle", "pirate-kit/Models/OBJ format/tool-paddle.obj", "drift", 3.6, 4.4, "bf", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* sd_r_models_c */
    {"coast", "m_shovel", "pirate-kit/Models/OBJ format/tool-shovel.obj", "iron", 3.6, 4.4, "bf", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* sd_r_models_c */
    {"coast", "m_fish", "survival-kit/Models/OBJ format/fish-large.obj", "shell", 22.0, 27.0, "bf", "made", 0.0, 0.0, 0.1, 0.0, 0, 0, {}},          /* sd_r_models_c */
    {"coast", "m_rocks", "pirate-kit/Models/OBJ format/rocks-sand-c.obj", "sandstone", 2.3, 2.9, "s", "made", 0.0, 0.0, 0.0, 5.5, 0, 0, {}},          /* sd_r_models_c */
    {"frontier", "m_barrel", "survival-kit/Models/OBJ format/barrel.obj", "wood", 17.0, 21.0, "lr", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* sd_r_models_c */
    {"frontier", "m_barrel_upright", "pirate-kit/Models/OBJ format/barrel.obj", "wood", 3.4, 4.2, "s", "made", 0.0, 0.0, 0.22, 5.2, 0, 0, {}},          /* sd_r_models_c */
    {"frontier", "m_bottle_crate", "pirate-kit/Models/OBJ format/crate-bottles.obj", "board", 4.2, 5.0, "s", "made", 0.0, 0.0, 0.1, 0.0, 0, 0, {}},          /* sd_r_models_c */
    {"frontier", "m_cart", "fantasy-town-kit/Models/OBJ format/cart.obj", "wood", 7.0, 8.2, "s", "made", 0.0, 0.0, 0.08, 5.5, 0, 0, {}},          /* sd_r_models_c */
    {"frontier", "m_wagon_wheel", "fantasy-town-kit/Models/OBJ format/wheel.obj", "wood", 15.0, 19.0, "lr", "made", 0.0, 0.0, 0.1, 0.0, 0, 0, {}},          /* sd_r_models_c */
    {"frontier", "m_chest", "survival-kit/Models/OBJ format/chest.obj", "wood", 17.0, 21.0, "s", "made", 0.0, 0.0, 0.12, 5.0, 0, 0, {}},          /* sd_r_models_c */
    {"frontier", "m_pickaxe", "survival-kit/Models/OBJ format/tool-pickaxe.obj", "iron", 30.0, 36.0, "bf", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* sd_r_models_c */
    {"frontier", "m_shovel", "survival-kit/Models/OBJ format/tool-shovel.obj", "iron", 28.0, 34.0, "bf", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* sd_r_models_c */
    {"frontier", "m_axe", "survival-kit/Models/OBJ format/tool-axe.obj", "iron", 28.0, 34.0, "bf", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* sd_r_models_c */
    {"frontier", "m_signpost", "survival-kit/Models/OBJ format/signpost.obj", "board", 20.0, 24.0, "b", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* sd_r_models_c */
    {"highland", "m_standing_stone", "nature-kit/Models/OBJ format/stone_tallC.obj", "slate", 8.5, 10.5, "sssb", "made", 0.0, 0.15, 0.22, 7.0, 0, 0, {}},          /* sd_r_models_c */
    {"highland", "m_standing_stone_b", "nature-kit/Models/OBJ format/stone_tallD.obj", "slate", 8.5, 10.5, "sssb", "made", 0.0, 0.15, 0.22, 7.0, 0, 0, {}},          /* sd_r_models_c */
    {"highland", "m_standing_stone_c", "nature-kit/Models/OBJ format/stone_tallG.obj", "slate", 8.5, 10.5, "sssf", "made", 0.0, 0.15, 0.22, 7.0, 0, 0, {}},          /* sd_r_models_c */
    {"highland", "m_standing_stone_d", "nature-kit/Models/OBJ format/stone_tallI.obj", "slate", 8.5, 10.5, "sssf", "made", 0.0, 0.15, 0.22, 7.0, 0, 0, {}},          /* sd_r_models_c */
    {"highland", "m_crag", "nature-kit/Models/OBJ format/stone_tallA.obj", "slate", 7.5, 9.0, "s", "debris", 0.0, 0.15, 0.0, 7.0, 0, 0, {}},          /* sd_r_models_c */
    {"highland", "m_crag_b", "nature-kit/Models/OBJ format/stone_tallB.obj", "slate", 8.0, 10.0, "s", "debris", 0.0, 0.15, 0.0, 7.0, 0, 0, {}},          /* sd_r_models_c */
    {"highland", "m_leaning_stone", "nature-kit/Models/OBJ format/stone_tallH.obj", "slate", 9.0, 11.0, "s", "made", 0.0, 0.15, 0.22, 6.5, 0, 0, {}},          /* sd_r_models_c */
    {"highland", "m_boulder", "nature-kit/Models/OBJ format/stone_largeD.obj", "slate", 7.0, 9.0, "s", "debris", 0.0, 0.0, 0.1, 5.0, 0, 0, {}},          /* sd_r_models_c */
    {"highland", "m_outcrop", "mini-forest/Models/OBJ format/stones.obj", "slate", 9.0, 11.0, "s", "debris", 0.0, 0.0, 0.0, 4.5, 0, 0, {}},          /* sd_r_models_c */
    {"highland", "m_campfire", "nature-kit/Models/OBJ format/campfire_stones.obj", "slate", 13.0, 16.0, "s", "made", 0.0, 0.0, 0.0, 0.0, 0, 0, {}},          /* sd_r_models_c */
    {"beast", "m_bone", "kaykit-halloween/obj/bone_A.obj", "bone", 7.0, 9.0, "s", "debris", 0.0, 0.25, 0.22, 0.0, 0, 0, {}},          /* sd_r_models_c */
    {"beast", "m_bone_knob", "kaykit-halloween/obj/bone_B.obj", "bone", 8.0, 10.0, "s", "debris", 0.0, 0.25, 0.22, 0.0, 0, 0, {}},          /* sd_r_models_c */
    {"beast", "m_long_bone", "kaykit-halloween/obj/bone_C.obj", "bone", 6.0, 7.5, "s", "debris", 0.0, 0.25, 0.22, 0.0, 0, 0, {}},          /* sd_r_models_c */
    {"beast", "m_skull", "kaykit-halloween/obj/skull.obj", "bone", 4.6, 5.6, "b", "made", 0.0, 0.2, 0.22, 0.0, 0, 0, {}},          /* sd_r_models_c */
    {"beast", "m_barrel", "survival-kit/Models/OBJ format/barrel.obj", "wood", 17.0, 21.0, "lr", "made", 0.15, 0.4, 0.22, 0.0, 0, 0, {}},          /* sd_r_models_c */
    {"beast", "m_crate", "survival-kit/Models/OBJ format/box-open.obj", "wood", 17.0, 21.0, "s", "made", 0.15, 0.4, 0.22, 5.0, 0, 0, {}},          /* sd_r_models_c */
    {"beast", "m_chest", "survival-kit/Models/OBJ format/chest.obj", "wood", 17.0, 21.0, "s", "made", 0.15, 0.4, 0.22, 5.0, 0, 0, {}},          /* sd_r_models_c */
    {"beast", "m_rowboat", "pirate-kit/Models/OBJ format/boat-row-small.obj", "wood", 3.4, 3.9, "s", "made", 0.2, 0.4, 0.22, 0.0, 0, 0, {}},          /* sd_r_models_c */
    {"beast", "m_fish", "survival-kit/Models/OBJ format/fish-large.obj", "flesh", 22.0, 27.0, "bf", "made", 0.0, 0.0, 0.1, 0.0, 0, 0, {}},          /* sd_r_models_c */
    {"badlands", "m_skull", "kaykit-halloween/obj/skull.obj", "bone", 4.6, 5.6, "b", "made", 0.0, 0.2, 0.22, 0.0, 0, 0, {}},          /* sd_r_models_c */
    {"badlands", "m_bone", "kaykit-halloween/obj/bone_C.obj", "bone", 6.0, 7.5, "s", "made", 0.0, 0.2, 0.22, 0.0, 0, 0, {}},          /* sd_r_models_c */
    {"badlands", "m_bone_knob", "kaykit-halloween/obj/bone_B.obj", "bone", 8.0, 10.0, "s", "made", 0.0, 0.2, 0.22, 0.0, 0, 0, {}},          /* sd_r_models_c */
    {"badlands", "m_wagon_wheel", "fantasy-town-kit/Models/OBJ format/wheel.obj", "wood", 15.0, 19.0, "lr", "made", 0.0, 0.2, 0.1, 0.0, 0, 0, {}},          /* sd_r_models_c */
    {"badlands", "m_cart", "fantasy-town-kit/Models/OBJ format/cart.obj", "wood", 7.0, 8.2, "s", "made", 0.0, 0.0, 0.08, 5.5, 0, 0, {}},          /* sd_r_models_c */
    {"badlands", "m_barrel", "survival-kit/Models/OBJ format/barrel.obj", "wood", 17.0, 21.0, "lr", "made", 0.0, 0.25, 0.22, 0.0, 0, 0, {}},          /* sd_r_models_c */
    {"badlands", "m_pickaxe", "survival-kit/Models/OBJ format/tool-pickaxe.obj", "iron", 30.0, 36.0, "bf", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* sd_r_models_c */
    {"badlands", "m_shovel", "survival-kit/Models/OBJ format/tool-shovel.obj", "iron", 28.0, 34.0, "bf", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* sd_r_models_c */
    {"badlands", "m_campfire", "nature-kit/Models/OBJ format/campfire_stones.obj", "rock", 13.0, 16.0, "s", "made", 0.0, 0.0, 0.0, 0.0, 0, 0, {}},          /* sd_r_models_c */
    {"badlands", "m_sand_rock", "survival-kit/Models/OBJ format/rock-sand-a.obj", "sandstone", 10.0, 13.0, "s", "debris", 0.0, 0.2, 0.22, 5.0, 0, 0, {}},          /* sd_r_models_c */
    {"badlands", "m_sand_rocks", "survival-kit/Models/OBJ format/rock-sand-b.obj", "sandstone", 8.0, 10.0, "s", "debris", 0.0, 0.2, 0.22, 5.0, 0, 0, {}},          /* sd_r_models_c */
    {"badlands", "m_horseshoe", "base-meshes/glb/horse_shoe.glb", "iron", 41.66666666666667, 50.0, "b", "made", -0.5, -0.2, 0.0, 0.0, 0, 0, {}},          /* sd_r_models_d */
    {"badlands", "m_sheriff_star", "base-meshes/glb/star_badge.glb", "tin", 45.0, 55.0, "b", "made", 0.0, 0.0, 0.06, 0.0, 0, 0, {}},          /* sd_r_models_d */
    {"badlands", "m_horn", "base-meshes/glb/horn.glb", "bone", 15.756302521008404, 18.907563025210084, "bf", "made", 0.0, 0.2, 0.1, 0.0, 0, 0, {}},          /* sd_r_models_d */
    {"badlands", "m_cartridge", "base-meshes/glb/762x51mm_bullet.glb", "tin", 111.11111111111111, 133.33333333333331, "lr", "made", 0.0, 0.0, 0.08, 0.0, 0, 0, {}},          /* sd_r_models_d */
    {"badlands", "m_boot", "base-meshes/glb/wellington_boot.glb", "leather", 13.513513513513516, 15.970515970515972, "bf", "made", 0.0, 0.2, 0.1, 0.0, 0, 0, {}},          /* sd_r_models_d */
    {"badlands", "m_whiskey_bottle", "base-meshes/glb/whiskey_bottle.glb", "glass", 20.766773162939298, 23.961661341853038, "lr", "made", 0.0, 0.2, 0.1, 0.0, 0, 0, {}},          /* sd_r_models_d */
    {"frontier", "m_horseshoe", "base-meshes/glb/horse_shoe.glb", "iron", 41.66666666666667, 50.0, "b", "made", -0.5, -0.2, 0.0, 0.0, 0, 0, {}},          /* sd_r_models_d */
    {"frontier", "m_sheriff_star", "base-meshes/glb/star_badge.glb", "tin", 45.0, 55.0, "b", "made", 0.0, 0.0, 0.06, 0.0, 0, 0, {}},          /* sd_r_models_d */
    {"frontier", "m_whiskey_bottle", "base-meshes/glb/whiskey_bottle.glb", "glass", 20.766773162939298, 23.961661341853038, "lr", "made", 0.0, 0.0, 0.1, 0.0, 0, 0, {}},          /* sd_r_models_d */
    {"frontier", "m_claw_hammer", "base-meshes/glb/hammer.glb", "iron", 30.0, 34.0, "bf", "made", 0.0, 0.0, 0.08, 0.0, 0, 0, {}},          /* sd_r_models_d */
    {"frontier", "m_padlock", "base-meshes/glb/padlock.glb", "iron", 47.36842105263158, 57.89473684210527, "b", "made", 0.0, 0.0, 0.06, 0.0, 0, 0, {}},          /* sd_r_models_d */
    {"mine", "m_ingot", "base-meshes/glb/ingot.glb", "brass", 16.172506738544474, 18.867924528301884, "s", "made", 0.0, 0.0, 0.08, 0.0, 0, 0, {}},          /* sd_r_models_d */
    {"mine", "m_rail", "base-meshes/glb/bull_head_rail.glb", "iron", 11.0, 13.0, "lr", "made", 0.0, 0.0, 0.0, 0.0, 0, 0, {}},          /* sd_r_models_d */
    {"mine", "m_gear", "base-meshes/glb/valve_wheel_02.glb", "iron", 78.57142857142857, 92.85714285714285, "s", "made", 0.0, 0.0, 0.06, 0.0, 0, 0, {}},          /* sd_r_models_d */
    {"mine", "m_gem", "base-meshes/glb/gem.glb", "rock", 18.0, 22.0, "bf", "made", 0.0, 0.0, 0.1, 0.0, 0, 0, {}},          /* sd_r_models_d */
    {"mine", "m_spoil_heap", "base-meshes/glb/dirt_pile_01.glb", "rock", 3.2706459525756335, 4.088307440719542, "s", "debris", 0.0, 0.0, 0.0, 0.0, 0, 0, {}},          /* sd_r_models_d */
    {"urban", "m_cinder_block", "base-meshes/glb/concrete_block.glb", "concrete", 12.500000000000002, 13.636363636363637, "s", "made", 0.0, 0.0, 0.08, 0.0, 0, 0, {}},          /* sd_r_models_d */
    {"urban", "m_drain_grate", "base-meshes/glb/drainage_grate_01.glb", "iron", 16.25, 20.0, "s", "made", -1.0, -0.5, 0.0, 0.0, 0, 0, {}},          /* sd_r_models_d */
    {"urban", "m_gas_bottle", "base-meshes/glb/gas_canister.glb", "tin", 8.79864636209814, 9.475465313028764, "lr", "made", 0.0, 0.0, 0.06, 0.0, 0, 0, {}},          /* sd_r_models_d */
    {"urban", "m_bat", "base-meshes/glb/baseball_bat.glb", "wood", 12.195121951219512, 14.634146341463413, "lr", "made", 0.0, 0.0, 0.05, 0.0, 0, 0, {}},          /* sd_r_models_d */
    {"urban", "m_spanner", "base-meshes/glb/spanner.glb", "iron", 31.872509960159363, 39.8406374501992, "s", "made", -1.2, -0.6, 0.0, 0.0, 0, 0, {}},          /* sd_r_models_d */
    {"urban", "m_bottle", "base-meshes/glb/wine_bottle.glb", "glass", 21.73913043478261, 25.08361204013378, "lr", "made", 0.0, 0.0, 0.1, 0.0, 0, 0, {}},          /* sd_r_models_d */
    {"urban", "m_jerrycan", "base-meshes/glb/plastic_bottle_01.glb", "tin", 25.0, 30.0, "bf", "made", 0.0, 0.0, 0.08, 0.0, 0, 0, {}},          /* sd_r_models_d */
    {"urban", "m_hacksaw", "base-meshes/glb/hacksaw_01.glb", "iron", 31.372549019607842, 37.25490196078431, "bf", "made", -0.8, -0.3, 0.0, 0.0, 0, 0, {}},          /* sd_r_models_d */
    {"corridor", "m_vent_cover", "base-meshes/glb/vent_cover_01.glb", "plate", 15.555555555555557, 18.88888888888889, "b", "made", -1.5, -0.8, 0.0, 0.0, 0, 0, {}},          /* sd_r_models_d */
    {"corridor", "m_spanner", "base-meshes/glb/spanner.glb", "metal", 31.872509960159363, 39.8406374501992, "s", "made", -1.2, -0.6, 0.0, 0.0, 0, 0, {}},          /* sd_r_models_d */
    {"corridor", "m_nut", "base-meshes/glb/axle_nut.glb", "metal", 166.66666666666669, 203.7037037037037, "s", "made", 0.0, 0.0, 0.05, 0.0, 0, 0, {}},          /* sd_r_models_d */
    {"corridor", "m_bolt", "base-meshes/glb/allen_key_screw.glb", "metal", 142.85714285714283, 165.7142857142857, "lr", "made", 0.0, 0.0, 0.05, 0.0, 0, 0, {}},          /* sd_r_models_d */
    {"corridor", "m_spring", "base-meshes/glb/spring.glb", "metal", 76.47058823529412, 88.23529411764706, "lr", "made", 0.0, 0.0, 0.05, 0.0, 0, 0, {}},          /* sd_r_models_d */
    {"corridor", "m_gas_bottle", "base-meshes/glb/gas_canister.glb", "tin", 8.79864636209814, 9.475465313028764, "lr", "made", 0.0, 0.0, 0.06, 0.0, 0, 0, {}},          /* sd_r_models_d */
    {"corridor", "m_fan", "base-meshes/glb/propeller_01.glb", "metal", 21.660649819494584, 25.27075812274368, "b", "made", 0.0, 0.0, 0.06, 0.0, 0, 0, {}},          /* sd_r_models_d */
    {"corridor", "m_radio", "base-meshes/glb/walkie_talkie.glb", "metal", 37.43315508021391, 42.780748663101605, "b", "made", 0.0, 0.0, 0.08, 0.0, 0, 0, {}},          /* sd_r_models_d */
    {"targa", "m_valve_wheel", "base-meshes/glb/valve_wheel_01.glb", "metal", 12.0, 14.0, "s", "made", 0.0, 0.2, 0.08, 0.0, 0, 0, {}},          /* sd_r_models_d */
    {"targa", "m_gear", "base-meshes/glb/valve_wheel_02.glb", "metal", 78.57142857142857, 92.85714285714285, "s", "made", 0.0, 0.2, 0.08, 0.0, 0, 0, {}},          /* sd_r_models_d */
    {"targa", "m_vent", "base-meshes/glb/vent_cover_01.glb", "metal", 15.555555555555557, 18.88888888888889, "b", "made", -1.5, -0.8, 0.0, 0.0, 0, 0, {}},          /* sd_r_models_d */
    {"targa", "m_spanner", "base-meshes/glb/spanner.glb", "metal", 31.872509960159363, 39.8406374501992, "s", "made", -1.2, -0.6, 0.0, 0.0, 0, 0, {}},          /* sd_r_models_d */
    {"targa", "m_gas_bottle", "base-meshes/glb/gas_canister.glb", "metal", 9.137055837563453, 10.490693739424705, "lr", "made", 0.1, 0.25, 0.08, 0.0, 0, 0, {}},          /* sd_r_models_d */
    {"targa", "m_fan", "base-meshes/glb/propeller_01.glb", "metal", 21.660649819494584, 25.27075812274368, "b", "made", 0.0, 0.2, 0.06, 0.0, 0, 0, {}},          /* sd_r_models_d */
    {"targa", "m_bolt", "base-meshes/glb/allen_key_screw.glb", "metal", 142.85714285714283, 165.7142857142857, "lr", "made", 0.0, 0.2, 0.05, 0.0, 0, 0, {}},          /* sd_r_models_d */
    {"trederra", "m_shell", "base-meshes/glb/762x51mm_bullet.glb", "brass", 144.44444444444443, 166.66666666666666, "lr", "made", 0.0, 0.2, 0.1, 0.0, 0, 0, {}},          /* sd_r_models_d */
    {"trederra", "m_boot", "base-meshes/glb/wellington_boot.glb", "leather", 13.513513513513516, 15.970515970515972, "bf", "made", 0.0, 0.25, 0.1, 0.0, 0, 0, {}},          /* sd_r_models_d */
    {"trederra", "m_jerrycan", "base-meshes/glb/plastic_bottle_01.glb", "tin", 25.0, 30.0, "bf", "made", 0.0, 0.2, 0.08, 0.0, 0, 0, {}},          /* sd_r_models_d */
    {"trederra", "m_gas_cylinder", "base-meshes/glb/gas_canister.glb", "tin", 9.137055837563453, 10.490693739424705, "lr", "made", 0.1, 0.25, 0.08, 0.0, 0, 0, {}},          /* sd_r_models_d */
    {"trederra", "m_oil_drum", "base-meshes/glb/oil_barrel.glb", "tin", 5.5813953488372094, 6.279069767441861, "lr", "made", 0.15, 0.3, 0.06, 0.0, 0, 0, {}},          /* sd_r_models_d */
    {"trederra", "m_sledgehammer", "base-meshes/glb/sledgehammer.glb", "iron", 14.04494382022472, 16.853932584269664, "bf", "made", 0.0, 0.0, 0.06, 0.0, 0, 0, {}},          /* sd_r_models_d */
    {"fortress", "m_stone_lantern", "base-meshes/glb/japanese_stone_lantern_01.glb", "garden", 10.035419126328216, 11.216056670602125, "b", "made", 0.0, 0.0, 0.05, 0.0, 0, 0, {}},          /* sd_r_models_d */
    {"fortress", "m_shuriken", "base-meshes/glb/4_point_shuriken.glb", "iron", 50.0, 61.11111111111111, "b", "made", -2.0, -1.2, 0.0, 0.0, 0, 0, {}},          /* sd_r_models_d */
    {"fortress", "m_kunai", "base-meshes/glb/throwing_knife.glb", "iron", 26.666666666666668, 31.666666666666668, "s", "made", -0.8, -0.3, 0.0, 0.0, 0, 0, {}},          /* sd_r_models_d */
    {"fortress", "m_sai", "base-meshes/glb/sai.glb", "iron", 18.367346938775512, 22.448979591836736, "bf", "made", -0.4, -0.0, 0.0, 0.0, 0, 0, {}},          /* sd_r_models_d */
    {"fortress", "m_teapot", "base-meshes/glb/teapot.glb", "iron", 18.677042801556418, 20.233463035019454, "b", "made", 0.0, 0.0, 0.05, 0.0, 0, 0, {}},          /* sd_r_models_d */
    {"fortress", "m_rice_bowl", "base-meshes/glb/wooden_bowl_01.glb", "lacquer", 22.857142857142858, 28.571428571428573, "s", "made", 0.0, 0.0, 0.06, 0.0, 0, 0, {}},          /* sd_r_models_d */
    {"fortress", "m_roof_tile", "base-meshes/glb/half_round_ridge_tile.glb", "clay", 13.129102844638949, 16.411378555798684, "s", "made", 0.0, 0.0, 0.08, 0.0, 0, 0, {}},          /* sd_r_models_d */
    {"fortress", "m_roof_tiles", "base-meshes/glb/roman_interlocking_tile.glb", "clay", 15.476190476190476, 19.047619047619047, "s", "made", 0.0, 0.0, 0.08, 0.0, 0, 0, {}},          /* sd_r_models_d */
    {"fortress", "m_scroll", "base-meshes/glb/scroll_02.glb", "paper", 15.449438202247192, 18.258426966292134, "b", "made", 0.0, 0.0, 0.05, 0.0, 0, 0, {}},          /* sd_r_models_d */
    {"fortress", "m_cuirass", "base-meshes/glb/breastplate_armour_02.glb", "lacquer", 15.503875968992247, 17.05426356589147, "b", "made", 0.0, 0.0, 0.05, 0.0, 0, 0, {}},          /* sd_r_models_d */
    {"dungeon", "m_skull", "base-meshes/glb/skull_no_teeth.glb", "bone", 17.6, 20.8, "lr", "made", 0.05, 0.25, 0.1, 0.0, 0, 0, {}},          /* sd_r_models_d */
    {"dungeon", "m_mace", "base-meshes/glb/mace.glb", "iron", 17.24137931034483, 20.689655172413794, "lr", "made", 0.0, 0.0, 0.05, 0.0, 0, 0, {}},          /* sd_r_models_d */
    {"dungeon", "m_breastplate", "base-meshes/glb/breastplate_armour_02.glb", "iron", 15.503875968992247, 17.05426356589147, "b", "made", 0.0, 0.0, 0.05, 0.0, 0, 0, {}},          /* sd_r_models_d */
    {"dungeon", "m_padlock", "base-meshes/glb/padlock.glb", "iron", 47.36842105263158, 57.89473684210527, "b", "made", 0.0, 0.0, 0.06, 0.0, 0, 0, {}},          /* sd_r_models_d */
    {"dungeon", "m_goblet", "base-meshes/glb/goblet_02.glb", "gold", 27.428571428571427, 30.85714285714286, "lr", "made", 0.0, 0.0, 0.06, 0.0, 0, 0, {}},          /* sd_r_models_d */
    {"dungeon", "m_gem", "base-meshes/glb/gem.glb", "glass", 18.0, 22.0, "bf", "made", 0.0, 0.0, 0.1, 0.0, 0, 0, {}},          /* sd_r_models_d */
    {"dungeon", "m_candlestick", "base-meshes/glb/candle_holder_01.glb", "brass", 25.179856115107913, 29.49640287769784, "lr", "made", 0.0, 0.0, 0.05, 0.0, 0, 0, {}},          /* sd_r_models_d */
    {"dungeon", "m_scroll", "base-meshes/glb/scroll_02.glb", "paper", 15.449438202247192, 18.258426966292134, "b", "made", 0.0, 0.0, 0.05, 0.0, 0, 0, {}},          /* sd_r_models_d */
    {"temple", "m_amphora", "base-meshes/glb/greek_vase_01.glb", "tcolumn", 13.75, 16.25, "lr", "made", 0.25, 0.4, 0.1, 0.0, 0, 0, {}},          /* sd_r_models_d */
    {"temple", "m_dish", "base-meshes/glb/roman_pottery_10.glb", "tcolumn", 22.93577981651376, 27.522935779816514, "s", "made", 0.0, 0.0, 0.08, 0.0, 0, 0, {}},          /* sd_r_models_d */
    {"temple", "m_spearhead", "base-meshes/glb/greek_spear_head_01.glb", "obsidian", 40.46242774566474, 49.13294797687861, "bf", "made", 0.0, 0.0, 0.06, 0.0, 0, 0, {}},          /* sd_r_models_d */
    {"temple", "m_shield", "base-meshes/glb/round_shield.glb", "gold", 9.333333333333332, 11.333333333333332, "b", "made", 0.0, 0.0, 0.1, 0.0, 0, 0, {}},          /* sd_r_models_d */
    {"temple", "m_goblet", "base-meshes/glb/goblet_02.glb", "gold", 27.428571428571427, 30.85714285714286, "lr", "made", 0.0, 0.0, 0.06, 0.0, 0, 0, {}},          /* sd_r_models_d */
    {"temple", "m_jade_gem", "base-meshes/glb/gem.glb", "jade", 18.0, 22.0, "bf", "made", 0.0, 0.0, 0.1, 0.0, 0, 0, {}},          /* sd_r_models_d */
    {"temple", "m_gold_bar", "base-meshes/glb/ingot.glb", "gold", 16.172506738544474, 18.867924528301884, "s", "made", 0.0, 0.0, 0.08, 0.0, 0, 0, {}},          /* sd_r_models_d */
    {"highland", "m_round_shield", "base-meshes/glb/round_shield.glb", "wood", 9.333333333333332, 11.333333333333332, "b", "made", 0.0, 0.0, 0.1, 0.0, 0, 0, {}},          /* sd_r_models_d */
    {"highland", "m_bronze_sword", "base-meshes/glb/bronze_age_sword.glb", "brass", 16.666666666666668, 20.0, "bf", "made", 0.0, 0.0, 0.04, 0.0, 0, 0, {}},          /* sd_r_models_d */
    {"highland", "m_spearhead", "base-meshes/glb/greek_spear_head_02.glb", "brass", 34.68208092485549, 43.35260115606937, "bf", "made", 0.0, 0.0, 0.06, 0.0, 0, 0, {}},          /* sd_r_models_d */
    {"highland", "m_horn", "base-meshes/glb/horn.glb", "bone", 15.756302521008404, 18.907563025210084, "bf", "made", 0.0, 0.2, 0.1, 0.0, 0, 0, {}},          /* sd_r_models_d */
    {"highland", "m_skull", "base-meshes/glb/skull_no_teeth.glb", "bone", 17.6, 20.8, "lr", "made", 0.05, 0.25, 0.1, 0.0, 0, 0, {}},          /* sd_r_models_d */
    {"highland", "m_arrowhead", "base-meshes/glb/barbed_arrowhead.glb", "slate", 100.0, 120.0, "b", "made", 0.0, 0.0, 0.06, 0.0, 0, 0, {}},          /* sd_r_models_d */
    {"highland", "m_bowl", "base-meshes/glb/wooden_bowl_01.glb", "wood", 22.857142857142858, 28.571428571428573, "s", "made", 0.0, 0.0, 0.08, 0.0, 0, 0, {}},          /* sd_r_models_d */
    {"graveyard", "m_skull", "base-meshes/glb/skull_no_teeth.glb", "bone", 17.6, 20.8, "lr", "made", 0.05, 0.25, 0.1, 0.0, 0, 0, {}},          /* sd_r_models_d */
    {"graveyard", "m_tomb", "base-meshes/glb/stone_tomb_01.glb", "headstone", 2.901178603807797, 3.2638259292837715, "s", "made", 0.0, 0.2, 0.05, 0.0, 0, 0, {}},          /* sd_r_models_d */
    {"graveyard", "m_mallet", "base-meshes/glb/wooden_mallet.glb", "wood", 21.666666666666668, 25.0, "bf", "made", 0.0, 0.0, 0.08, 0.0, 0, 0, {}},          /* sd_r_models_d */
    {"graveyard", "m_trowel", "base-meshes/glb/trowel.glb", "iron", 23.21428571428571, 26.785714285714285, "bf", "made", 0.0, 0.0, 0.08, 0.0, 0, 0, {}},          /* sd_r_models_d */
    {"forest", "m_mushroom", "base-meshes/glb/mushroom_01.glb", "shroom", 46.0, 50.0, "lr", "debris", 0.0, 0.0, 0.06, 0.0, 0, 0, {}},          /* sd_r_models_d */
    {"forest", "m_toadstool", "base-meshes/glb/mushroom_03.glb", "shroom", 48.0, 54.0, "lr", "debris", 0.0, 0.0, 0.06, 0.0, 0, 0, {}},          /* sd_r_models_d */
    {"forest", "m_firewood", "base-meshes/glb/firewood_01.glb", "log", 23.60515021459227, 27.896995708154506, "bf", "made", 0.0, 0.0, 0.1, 0.0, 0, 0, {}},          /* sd_r_models_d */
    {"forest", "m_mallet", "base-meshes/glb/wooden_mallet.glb", "wood", 21.666666666666668, 25.0, "bf", "made", 0.0, 0.0, 0.08, 0.0, 0, 0, {}},          /* sd_r_models_d */
    {"swamp", "m_boot", "base-meshes/glb/wellington_boot.glb", "hide", 13.513513513513516, 15.970515970515972, "bf", "made", 0.0, 0.3, 0.1, 0.0, 0, 0, {}},          /* sd_r_models_d */
    {"swamp", "m_mushroom", "base-meshes/glb/mushroom_01.glb", "shroom", 46.0, 50.0, "lr", "debris", 0.0, 0.0, 0.06, 0.0, 0, 0, {}},          /* sd_r_models_d */
    {"swamp", "m_toadstool", "base-meshes/glb/mushroom_03.glb", "shroom", 48.0, 54.0, "lr", "debris", 0.0, 0.0, 0.06, 0.0, 0, 0, {}},          /* sd_r_models_d */
    {"swamp", "m_fish_bone", "base-meshes/glb/cartoon_fish_bone.glb", "bone", 26.666666666666668, 33.333333333333336, "bf", "made", 0.0, 0.0, 0.05, 0.0, 0, 0, {}},          /* sd_r_models_d */
    {"swamp", "m_jug", "base-meshes/glb/wine_jug_01.glb", "clay", 14.836795252225519, 17.80415430267062, "lr", "made", 0.25, 0.4, 0.1, 0.0, 0, 0, {}},          /* sd_r_models_d */
    {"coast", "m_ship_wheel", "base-meshes/glb/ship_wheel.glb", "drift", 8.982035928143713, 10.479041916167665, "b", "made", 0.0, 0.0, 0.06, 0.0, 0, 0, {}},          /* sd_r_models_d */
    {"coast", "m_scallop", "base-meshes/glb/shell_02.glb", "shell", 84.9056603773585, 103.77358490566039, "s", "debris", 0.0, 0.0, 0.1, 0.0, 0, 0, {}},          /* sd_r_models_d */
    {"coast", "m_seashell", "base-meshes/glb/shell_01.glb", "shell", 125.0, 150.0, "b", "debris", 0.0, 0.0, 0.1, 0.0, 0, 0, {}},          /* sd_r_models_d */
    {"coast", "m_propeller", "base-meshes/glb/propeller_01.glb", "brass", 21.660649819494584, 25.27075812274368, "b", "made", 0.0, 0.2, 0.06, 0.0, 0, 0, {}},          /* sd_r_models_d */
    {"coast", "m_cannon", "base-meshes/glb/cannon_with_base.glb", "iron", 3.8148524923702953, 4.476093591047814, "s", "made", 0.0, 0.2, 0.06, 0.0, 0, 0, {}},          /* sd_r_models_d */
    {"coast", "m_fish_bone", "base-meshes/glb/cartoon_fish_bone.glb", "bone", 26.666666666666668, 33.333333333333336, "bf", "made", 0.0, 0.0, 0.05, 0.0, 0, 0, {}},          /* sd_r_models_d */
    {"coast", "m_rope", "base-meshes/glb/rope_twist_01.glb", "drift", 65.0, 75.0, "lr", "made", 0.0, 0.0, 0.05, 0.0, 0, 0, {}},          /* sd_r_models_d */
    {"coast", "m_amphora", "base-meshes/glb/greek_vase_01.glb", "sandstone", 13.75, 16.25, "lr", "made", 0.25, 0.4, 0.1, 0.0, 0, 0, {}},          /* sd_r_models_d */
    {"beast", "m_horn", "base-meshes/glb/horn.glb", "tooth", 15.756302521008404, 18.907563025210084, "bf", "debris", 0.0, 0.25, 0.1, 0.0, 0, 0, {}},          /* sd_r_models_d */
    {"beast", "m_tusk", "base-meshes/glb/rhino_large_horn.glb", "tooth", 10.550996483001173, 12.895662368112545, "lr", "debris", 0.0, 0.0, 0.05, 0.0, 0, 0, {}},          /* sd_r_models_d */
    {"beast", "m_fish_bone", "base-meshes/glb/cartoon_fish_bone.glb", "bone", 26.666666666666668, 33.333333333333336, "bf", "debris", 0.0, 0.0, 0.05, 0.0, 0, 0, {}},          /* sd_r_models_d */
    {"beast", "m_insect_leg", "base-meshes/glb/insect_leg.glb", "bone", 69.23076923076923, 84.61538461538461, "b", "debris", 0.0, 0.0, 0.05, 0.0, 0, 0, {}},          /* sd_r_models_d */
    {"beast", "m_ship_wheel", "base-meshes/glb/ship_wheel.glb", "wood", 8.982035928143713, 10.479041916167665, "b", "made", 0.0, 0.0, 0.06, 0.0, 0, 0, {}},          /* sd_r_models_d */
    {"beast", "m_cannon", "base-meshes/glb/cannon_with_base.glb", "tin", 3.967446592065107, 4.781281790437437, "s", "made", 0.15, 0.35, 0.1, 0.0, 0, 0, {}},          /* sd_r_models_d */
    {"infestation", "m_insect_leg", "base-meshes/glb/insect_leg.glb", "carapace", 69.23076923076923, 84.61538461538461, "b", "debris", 0.0, 0.0, 0.05, 0.0, 0, 0, {}},          /* sd_r_models_d */
    {"infestation", "m_spine", "base-meshes/glb/rhino_large_horn.glb", "spine", 10.550996483001173, 12.895662368112545, "lr", "debris", 0.0, 0.0, 0.05, 0.0, 0, 0, {}},          /* sd_r_models_d */
    {"cynder", "m_skull", "base-meshes/glb/skull_no_teeth.glb", "tooth", 17.6, 20.8, "lr", "made", 0.05, 0.3, 0.1, 0.0, 0, 0, {}},          /* sd_r_models_d */
    {"cynder", "m_horn", "base-meshes/glb/horn.glb", "tooth", 15.756302521008404, 18.907563025210084, "bf", "made", 0.0, 0.25, 0.1, 0.0, 0, 0, {}},          /* sd_r_models_d */
    {"cynder", "m_tusk", "base-meshes/glb/rhino_large_horn.glb", "tooth", 10.550996483001173, 12.895662368112545, "lr", "made", 0.0, 0.0, 0.05, 0.0, 0, 0, {}},          /* sd_r_models_d */
    {"cynder", "m_mace", "base-meshes/glb/mace.glb", "iron", 17.24137931034483, 20.689655172413794, "lr", "made", 0.0, 0.0, 0.05, 0.0, 0, 0, {}},          /* sd_r_models_d */
    {"cynder", "m_sword", "base-meshes/glb/gladius_01.glb", "iron", 16.238159675236805, 18.944519621109606, "bf", "made", 0.0, 0.0, 0.03, 0.0, 0, 0, {}},          /* sd_r_models_d */
    {"cynder", "m_breastplate", "base-meshes/glb/breastplate_armour_02.glb", "iron", 15.503875968992247, 17.05426356589147, "b", "made", 0.0, 0.0, 0.05, 0.0, 0, 0, {}},          /* sd_r_models_d */
    {"maahl", "m_amphora", "base-meshes/glb/greek_vase_01.glb", "clay", 13.75, 16.25, "lr", "made", 0.25, 0.4, 0.1, 0.0, 0, 0, {}},          /* sd_r_models_d */
    {"maahl", "m_dish", "base-meshes/glb/roman_pottery_10.glb", "clay", 22.93577981651376, 27.522935779816514, "s", "made", 0.0, 0.2, 0.08, 0.0, 0, 0, {}},          /* sd_r_models_d */
    {"maahl", "m_spearhead", "base-meshes/glb/greek_spear_head_01.glb", "brass", 40.46242774566474, 49.13294797687861, "bf", "made", 0.0, 0.0, 0.06, 0.0, 0, 0, {}},          /* sd_r_models_d */
    {"maahl", "m_shield", "base-meshes/glb/round_shield.glb", "brass", 9.333333333333332, 11.333333333333332, "b", "made", 0.0, 0.2, 0.1, 0.0, 0, 0, {}},          /* sd_r_models_d */
    {"maahl", "m_insect_leg", "base-meshes/glb/insect_leg.glb", "chitin", 69.23076923076923, 84.61538461538461, "b", "debris", 0.0, 0.0, 0.05, 0.0, 0, 0, {}},          /* sd_r_models_d */
    {"maahl", "m_horn", "base-meshes/glb/horn.glb", "chitin", 15.756302521008404, 18.907563025210084, "bf", "debris", 0.0, 0.2, 0.1, 0.0, 0, 0, {}},          /* sd_r_models_d */
/* END MODEL TABLE */
};

/* sd_mesh.solid(...)[0] as a solid: sect(z) = the model's outline above z */
static int msect(void *ctx, double z, GEOSGeometry **out) { int e = MDL_OK; *out = mdl_sect((MdlSolid *)ctx, z, &e); return e == MDL_OK ? SO_OK : SO_ERR; }
static void mfree(void *ctx) { mdl_solid_free((MdlSolid *)ctx); }
/* sol, foot, top = sd_mesh.solid(rel, mm, cx, cy, yaw, pose, sink, mirror, tilt, tilt_dir, cap): sol into *sol */
static int msolid(const char *rel, double mm, double cx, double cy, double yaw, char pose, double sink, int mirror, double tilt, double tdir, double cap, SoSolids *sol, GG **foot, double *top) {
    int err = 0; const MdlMesh *VF; MdlSolid *ms = NULL; GEOSGeometry *f = NULL;
    if (E_) return SO_ERR;
    if (!(VF = mdl_load(ROOT, rel, &err))) { fail(); return SO_ERR; }
    if (mdl_solid(VF, mm, cx, cy, yaw, pose, sink, mirror, tilt, tdir, cap, &ms, &f, top) != MDL_OK) { if (ms) mdl_solid_free(ms); shp_free(f); fail(); return SO_ERR; }
    *foot = gk(f);
    sol_push(sol, sol_custom(msect, ms, mfree, *top)); sol_unref(sol->v[sol->n - 1]);          /* the list holds the one reference */
    return SO_OK;
}
static _Thread_local const MRow *CUR;
static int m_body(double cx, double cy, double a, SoLayers *lay, GG **foot_) {
    const MRow *R = CUR; double lim, mm, sink, tilt, tdir, top, ar, r, ang, yaw, mm2, tilt2, tdir2, t2; char pose, pose2; int mirror, mirror2, i;
    SoSolids *sol = SL(); GG *foot = NULL; GL *foots = gl(); SoLayer *y = NULL;
    pose = R->poses[rch((int)strlen(R->poses))];
    if (R->cap) { lim = R->cap; if (R->capmin) lim = MN(R->cap, max_height() * 0.92); }          /* cap or (gg.max_height() * 0.92) */
    else lim = max_height() * 0.92;
    mm = u(R->mm0, R->mm1); sink = u(R->sink0, R->sink1); mirror = rnd() < 0.5;
    if (pose != 's' || rnd() < 0.5) tilt = u(0, R->tilt); else tilt = 0.0;
    tdir = u(0, 6.28);
    if (msolid(R->rel, mm, cx, cy, a, pose, sink, mirror, tilt, tdir, lim, sol, &foot, &top) != SO_OK) return SC_ERR;
    if (ck(so_fine(sol, top, R->kind, &y)) < 0) return SC_ERR;
    yk(y); push(lay, y); gl_add(foots, foot);
    for (i = 0; i < R->nwith; i++) {
        if (!mdl_has(ROOT, R->with_[i].rel)) continue;          /* with_ = [w for w in with_ if os.path.exists(...)] */
        if (rnd() < 0.6) {
            SoSolids *s2 = SL(); GG *f2 = NULL; SoLayer *y2 = NULL;
            ar = area(foot); r = sqrt(ar / 3.14) + u(0.8, 1.6);
            ang = u(0, 6.28); { P2 p = pol(cx, cy, r, ang);
            mm2 = u(R->mm0, R->mm1) * R->with_[i].k; yaw = u(0, 6.28); pose2 = "sbl"[rch(3)]; mirror2 = rnd() < 0.5; tilt2 = u(0, 0.3); tdir2 = u(0, 6.28);
            if (msolid(R->with_[i].rel, mm2, p.x, p.y, yaw, pose2, 0.0, mirror2, tilt2, tdir2, lim, s2, &f2, &t2) != SO_OK) return SC_ERR; }
            if (!intersects(f2, foot) && !E_) {
                if (ck(so_fine(s2, t2, R->kind, &y2)) < 0) return SC_ERR;
                yk(y2); push(lay, y2); gl_add(foots, f2);
            }
        }
    }
    RET(hull(foots));
}
static int m_build(void *ctx, double cx, double cy, double a, PyRandom *rng, int hs, double sc, Geo **foot, SoLayers *lay) {
    const MRow *prev = CUR; int r; (void)hs; (void)sc;
    CUR = (const MRow *)ctx; r = bsdde_run(m_body, cx, cy, a, rng, foot, lay); CUR = prev;
    return r;
}
/* the rows of one Python file, registered as _model does it: a missing model file makes no detail */
void bsdde_models_register(void) {
    int n = (int)(sizeof ROWS / sizeof ROWS[0]), i;
    for (i = 0; i < n; i++) {
        if (!mdl_has(ROOT, ROWS[i].rel)) continue;
        bsdde_reg(ROWS[i].style, ROWS[i].cls, ROWS[i].kind, ROWS[i].name, 10.5, 0, m_build, (void *)&ROWS[i]);
    }
}
