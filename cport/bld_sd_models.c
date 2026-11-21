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
/* IMPORT 2026-11-20 BEGIN: work\import\apply.py */
    {"graveyard", "m_round_headstone", "polyy-gravestones/obj/grave_01_classic_rounded.obj", "headstone", 5.52, 6.48, "s", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* round-top headstone; reduced 7270 -> 3012 tris; stands on it */
    {"frontier", "m_round_headstone", "polyy-gravestones/obj/grave_01_classic_rounded.obj", "rock", 5.52, 6.48, "s", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* round-top headstone; reduced 7270 -> 3012 tris; stands on it */
    {"graveyard", "m_ring_cross", "polyy-gravestones/obj/grave_02_ring_cross.obj", "headstone", 7.728, 9.072, "s", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* ring cross; reduced 6749 -> 2827 tris; stands on its own gro */
    {"highland", "m_ring_cross", "polyy-gravestones/obj/grave_02_ring_cross.obj", "slate", 7.728, 9.072, "s", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* ring cross; reduced 6749 -> 2827 tris; stands on its own gro */
    {"graveyard", "m_tapered_obelisk", "polyy-gravestones/obj/grave_03_tapering_obelisk.obj", "headstone", 12.144, 14.256, "s", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* obelisk on a plinth; reduced 7725 -> 2940 tris; stands on it */
    {"obelis", "m_tapered_obelisk", "polyy-gravestones/obj/grave_03_tapering_obelisk.obj", "headstone", 12.144, 14.256, "s", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* obelisk on a plinth; reduced 7725 -> 2940 tris; stands on it */
    {"temple", "m_tapered_obelisk", "polyy-gravestones/obj/grave_03_tapering_obelisk.obj", "tcolumn", 12.144, 14.256, "s", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* obelisk on a plinth; reduced 7725 -> 2940 tris; stands on it */
    {"graveyard", "m_broken_column", "polyy-gravestones/obj/grave_04_broken_column.obj", "headstone", 8.28, 9.72, "s", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* broken column on a plinth; reduced 6459 -> 3101 tris; stands */
    {"temple", "m_broken_column", "polyy-gravestones/obj/grave_04_broken_column.obj", "tcolumn", 8.28, 9.72, "s", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* broken column on a plinth; reduced 6459 -> 3101 tris; stands */
    {"sundered", "m_broken_column", "polyy-gravestones/obj/grave_04_broken_column.obj", "headstone", 8.28, 9.72, "s", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* broken column on a plinth; reduced 6459 -> 3101 tris; stands */
    {"graveyard", "m_gothic_headstone", "polyy-gravestones/obj/grave_05_gothic_arch.obj", "headstone", 6.624, 7.776, "s", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* gothic arch headstone; reduced 5692 -> 3286 tris; stands on  */
    {"sundered", "m_gothic_headstone", "polyy-gravestones/obj/grave_05_gothic_arch.obj", "headstone", 6.624, 7.776, "s", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* gothic arch headstone; reduced 5692 -> 3286 tris; stands on  */
    {"graveyard", "m_ledger_slab", "polyy-gravestones/obj/grave_06_ledger_slab.obj", "headstone", 11.04, 12.96, "s", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* sunken ledger slab; reduced 7413 -> 3585 tris; stands on its */
    {"dungeon", "m_ledger_slab", "polyy-gravestones/obj/grave_06_ledger_slab.obj", "flag", 11.04, 12.96, "s", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* sunken ledger slab; reduced 7413 -> 3585 tris; stands on its */
    {"graveyard", "m_pillow_marker", "polyy-gravestones/obj/grave_07_pillow_marker.obj", "headstone", 4.968, 5.832, "s", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* low pillow marker; reduced 7942 -> 3278 tris; stands on its  */
    {"graveyard", "m_weeping_angel", "polyy-gravestones/obj/grave_08_weeping_angel.obj", "headstone", 9.991, 11.729, "s", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* weeping angel on a pedestal; reduced 6421 -> 3163 tris; stan */
    {"sundered", "m_weeping_angel", "polyy-gravestones/obj/grave_08_weeping_angel.obj", "headstone", 9.991, 11.729, "s", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* weeping angel on a pedestal; reduced 6421 -> 3163 tris; stan */
    {"graveyard", "m_draped_urn", "polyy-gravestones/obj/grave_09_draped_urn.obj", "headstone", 8.28, 9.72, "s", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* draped urn on a pedestal; reduced 7392 -> 2715 tris; stands  */
    {"temple", "m_draped_urn", "polyy-gravestones/obj/grave_09_draped_urn.obj", "tcolumn", 8.28, 9.72, "s", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* draped urn on a pedestal; reduced 7392 -> 2715 tris; stands  */
    {"graveyard", "m_chest_tomb", "polyy-gravestones/obj/grave_10_chest_tomb.obj", "headstone", 11.04, 12.96, "s", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* chest tomb; reduced 7552 -> 3215 tris; stands on its own gro */
    {"temple", "m_chest_tomb", "polyy-gravestones/obj/grave_10_chest_tomb.obj", "tcolumn", 11.04, 12.96, "s", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* chest tomb; reduced 7552 -> 3215 tris; stands on its own gro */
    {"dungeon", "m_chest_tomb", "polyy-gravestones/obj/grave_10_chest_tomb.obj", "flag", 11.04, 12.96, "s", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* chest tomb; reduced 7552 -> 3215 tris; stands on its own gro */
    {"graveyard", "m_leaning_slab", "polyy-gravestones/obj/grave_11_leaning_slab.obj", "headstone", 6.182, 7.258, "s", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* leaning cracked slab; reduced 7658 -> 3522 tris; stands on i */
    {"swamp", "m_leaning_slab", "polyy-gravestones/obj/grave_11_leaning_slab.obj", "tflag", 6.182, 7.258, "s", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* leaning cracked slab; reduced 7658 -> 3522 tris; stands on i */
    {"graveyard", "m_boulder_memorial", "polyy-gravestones/obj/grave_12_boulder_memorial.obj", "headstone", 5.52, 6.48, "s", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* boulder with a polished face; reduced 7532 -> 3172 tris; sta */
    {"highland", "m_boulder_memorial", "polyy-gravestones/obj/grave_12_boulder_memorial.obj", "slate", 5.52, 6.48, "s", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* boulder with a polished face; reduced 7532 -> 3172 tris; sta */
    {"graveyard", "m_plank_cross", "polyy-gravestones/obj/grave_13_plank_cross.obj", "wood", 6.624, 7.776, "s", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* wooden plank cross; reduced 7420 -> 3145 tris; stands on its */
    {"frontier", "m_plank_cross", "polyy-gravestones/obj/grave_13_plank_cross.obj", "wood", 6.624, 7.776, "s", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* wooden plank cross; reduced 7420 -> 3145 tris; stands on its */
    {"badlands", "m_plank_cross", "polyy-gravestones/obj/grave_13_plank_cross.obj", "wood", 6.624, 7.776, "s", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* wooden plank cross; reduced 7420 -> 3145 tris; stands on its */
    {"graveyard", "m_iron_cross", "polyy-gravestones/obj/grave_14_iron_cross.obj", "iron", 6.624, 7.776, "s", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* iron grave cross; reduced 7332 -> 3212 tris; stands on its o */
    {"frontier", "m_iron_cross", "polyy-gravestones/obj/grave_14_iron_cross.obj", "iron", 6.624, 7.776, "s", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* iron grave cross; reduced 7332 -> 3212 tris; stands on its o */
    {"trederra", "m_iron_cross", "polyy-gravestones/obj/grave_14_iron_cross.obj", "iron", 6.624, 7.776, "s", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* iron grave cross; reduced 7332 -> 3212 tris; stands on its o */
    {"graveyard", "m_stump_marker", "polyy-gravestones/obj/grave_15_stump_marker.obj", "wood", 5.52, 6.48, "s", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* carved tree-stump marker; reduced 7630 -> 3475 tris; stands  */
    {"forest", "m_stump_marker", "polyy-gravestones/obj/grave_15_stump_marker.obj", "wood", 5.52, 6.48, "s", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* carved tree-stump marker; reduced 7630 -> 3475 tris; stands  */
    {"graveyard", "m_scroll_headstone", "polyy-gravestones/obj/grave_16_scroll_top.obj", "headstone", 5.52, 6.48, "s", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* scroll-top stone; reduced 7776 -> 3442 tris; stands on its o */
    {"graveyard", "m_stepped_cross", "polyy-gravestones/obj/grave_17_stepped_cross.obj", "headstone", 8.832, 10.368, "s", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* stepped calvary cross; reduced 5902 -> 3648 tris; stands on  */
    {"sundered", "m_stepped_cross", "polyy-gravestones/obj/grave_17_stepped_cross.obj", "headstone", 8.832, 10.368, "s", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* stepped calvary cross; reduced 5902 -> 3648 tris; stands on  */
    {"graveyard", "m_double_headstone", "polyy-gravestones/obj/grave_19_double_arch.obj", "headstone", 7.728, 9.072, "s", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* double headstone; reduced 7182 -> 2867 tris; stands on its o */
    {"graveyard", "m_bordered_slate", "polyy-gravestones/obj/grave_20_bordered_slate.obj", "headstone", 6.072, 7.128, "s", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* bordered slate stone; reduced 7558 -> 3676 tris; stands on i */
    {"graveyard", "m_deaths_head_stone", "polyy-gravestones/obj/grave_21_winged_deaths_head.obj", "headstone", 6.624, 7.776, "s", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* winged death's-head stone; reduced 8057 -> 3103 tris; stands */
    {"sundered", "m_deaths_head_stone", "polyy-gravestones/obj/grave_21_winged_deaths_head.obj", "headstone", 6.624, 7.776, "s", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* winged death's-head stone; reduced 8057 -> 3103 tris; stands */
    {"graveyard", "m_sarcophagus", "polyy-gravestones/obj/grave_22_sarcophagus.obj", "headstone", 12.144, 14.256, "s", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* lidded sarcophagus; reduced 7790 -> 2997 tris; stands on its */
    {"temple", "m_sarcophagus", "polyy-gravestones/obj/grave_22_sarcophagus.obj", "tcolumn", 12.144, 14.256, "s", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* lidded sarcophagus; reduced 7790 -> 2997 tris; stands on its */
    {"dungeon", "m_sarcophagus", "polyy-gravestones/obj/grave_22_sarcophagus.obj", "flag", 12.144, 14.256, "s", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* lidded sarcophagus; reduced 7790 -> 2997 tris; stands on its */
    {"obelis", "m_sarcophagus", "polyy-gravestones/obj/grave_22_sarcophagus.obj", "headstone", 12.144, 14.256, "s", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* lidded sarcophagus; reduced 7790 -> 2997 tris; stands on its */
    {"graveyard", "m_toppled_stone", "polyy-gravestones/obj/grave_25_toppled_stone.obj", "headstone", 9.936, 11.664, "s", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* toppled broken stone on a ledger; reduced 7623 -> 3356 tris; */
    {"sundered", "m_toppled_stone", "polyy-gravestones/obj/grave_25_toppled_stone.obj", "headstone", 9.936, 11.664, "s", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* toppled broken stone on a ledger; reduced 7623 -> 3356 tris; */
    {"graveyard", "m_human_skull", "polyy-bones/obj/skull_01_human.obj", "bone", 1.214, 1.426, "ssl", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* human skull */
    {"dungeon", "m_human_skull", "polyy-bones/obj/skull_01_human.obj", "bone", 1.214, 1.426, "ssl", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* human skull */
    {"swamp", "m_human_skull", "polyy-bones/obj/skull_01_human.obj", "bone", 1.214, 1.426, "ssl", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* human skull */
    {"badlands", "m_human_skull", "polyy-bones/obj/skull_01_human.obj", "bone", 1.214, 1.426, "ssl", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* human skull */
    {"blight", "m_human_skull", "polyy-bones/obj/skull_01_human.obj", "bone", 1.214, 1.426, "ssl", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* human skull */
    {"ashveil", "m_human_skull", "polyy-bones/obj/skull_01_human.obj", "bone", 1.214, 1.426, "ssl", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* human skull */
    {"trederra", "m_human_skull", "polyy-bones/obj/skull_01_human.obj", "bone", 1.214, 1.426, "ssl", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* human skull */
    {"sundered", "m_human_skull", "polyy-bones/obj/skull_01_human.obj", "bone", 1.214, 1.426, "ssl", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* human skull */
    {"thornwild", "m_human_skull", "polyy-bones/obj/skull_01_human.obj", "bone", 1.214, 1.426, "ssl", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* human skull */
    {"urban", "m_human_skull", "polyy-bones/obj/skull_01_human.obj", "bone", 1.214, 1.426, "ssl", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* human skull */
    {"graveyard", "m_cranium", "polyy-bones/obj/skull_02_cranium.obj", "bone", 1.104, 1.296, "ssl", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* cranium without jaw; reduced 6269 -> 3676 tris */
    {"dungeon", "m_cranium", "polyy-bones/obj/skull_02_cranium.obj", "bone", 1.104, 1.296, "ssl", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* cranium without jaw; reduced 6269 -> 3676 tris */
    {"blight", "m_cranium", "polyy-bones/obj/skull_02_cranium.obj", "bone", 1.104, 1.296, "ssl", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* cranium without jaw; reduced 6269 -> 3676 tris */
    {"dungeon", "m_cracked_skull", "polyy-bones/obj/skull_03_cracked.obj", "bone", 1.214, 1.426, "ssl", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* cracked skull; reduced 4761 -> 3405 tris */
    {"badlands", "m_cracked_skull", "polyy-bones/obj/skull_03_cracked.obj", "bone", 1.214, 1.426, "ssl", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* cracked skull; reduced 4761 -> 3405 tris */
    {"urban", "m_cracked_skull", "polyy-bones/obj/skull_03_cracked.obj", "bone", 1.214, 1.426, "ssl", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* cracked skull; reduced 4761 -> 3405 tris */
    {"ashveil", "m_cracked_skull", "polyy-bones/obj/skull_03_cracked.obj", "bone", 1.214, 1.426, "ssl", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* cracked skull; reduced 4761 -> 3405 tris */
    {"dungeon", "m_giant_skull", "polyy-bones/obj/skull_05_giant.obj", "bone", 2.76, 3.24, "ssl", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* giant humanoid skull */
    {"badlands", "m_giant_skull", "polyy-bones/obj/skull_05_giant.obj", "bone", 2.76, 3.24, "ssl", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* giant humanoid skull */
    {"maahl", "m_giant_skull", "polyy-bones/obj/skull_05_giant.obj", "bone", 2.76, 3.24, "ssl", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* giant humanoid skull */
    {"temple", "m_demon_skull", "polyy-bones/obj/skull_06_horned.obj", "bone", 3.312, 3.888, "s", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* horned skull; reduced 4816 -> 3682 tris */
    {"dungeon", "m_demon_skull", "polyy-bones/obj/skull_06_horned.obj", "bone", 3.312, 3.888, "s", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* horned skull; reduced 4816 -> 3682 tris */
    {"blight", "m_demon_skull", "polyy-bones/obj/skull_06_horned.obj", "bone", 3.312, 3.888, "s", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* horned skull; reduced 4816 -> 3682 tris */
    {"cynder", "m_demon_skull", "polyy-bones/obj/skull_06_horned.obj", "tooth", 3.312, 3.888, "s", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* horned skull; reduced 4816 -> 3682 tris */
    {"dungeon", "m_brute_skull", "polyy-bones/obj/skull_08_tusked.obj", "bone", 1.932, 2.268, "ssl", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* tusked brute skull */
    {"badlands", "m_brute_skull", "polyy-bones/obj/skull_08_tusked.obj", "bone", 1.932, 2.268, "ssl", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* tusked brute skull */
    {"blight", "m_brute_skull", "polyy-bones/obj/skull_08_tusked.obj", "bone", 1.932, 2.268, "ssl", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* tusked brute skull */
    {"temple", "m_cyclops_skull", "polyy-bones/obj/skull_09_cyclops.obj", "bone", 2.76, 3.24, "ssl", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* one-eyed giant skull */
    {"coast", "m_cyclops_skull", "polyy-bones/obj/skull_09_cyclops.obj", "bone", 2.76, 3.24, "ssl", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* one-eyed giant skull */
    {"badlands", "m_cyclops_skull", "polyy-bones/obj/skull_09_cyclops.obj", "bone", 2.76, 3.24, "ssl", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* one-eyed giant skull */
    {"temple", "m_carved_skull", "polyy-bones/obj/skull_10_carved.obj", "bone", 1.214, 1.426, "s", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* carved ritual skull; reduced 4405 -> 2954 tris */
    {"swamp", "m_carved_skull", "polyy-bones/obj/skull_10_carved.obj", "bone", 1.214, 1.426, "s", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* carved ritual skull; reduced 4405 -> 2954 tris */
    {"thornwild", "m_carved_skull", "polyy-bones/obj/skull_10_carved.obj", "bone", 1.214, 1.426, "s", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* carved ritual skull; reduced 4405 -> 2954 tris */
    {"forest", "m_wolf_skull", "polyy-bones/obj/skull_11_wolf.obj", "bone", 1.38, 1.62, "ssl", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* wolf skull; reduced 4677 -> 3028 tris */
    {"highland", "m_wolf_skull", "polyy-bones/obj/skull_11_wolf.obj", "bone", 1.38, 1.62, "ssl", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* wolf skull; reduced 4677 -> 3028 tris */
    {"forest", "m_bear_skull", "polyy-bones/obj/skull_12_bear.obj", "bone", 2.208, 2.592, "ssl", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* bear skull; reduced 5067 -> 3009 tris */
    {"highland", "m_bear_skull", "polyy-bones/obj/skull_12_bear.obj", "bone", 2.208, 2.592, "ssl", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* bear skull; reduced 5067 -> 3009 tris */
    {"mine", "m_bear_skull", "polyy-bones/obj/skull_12_bear.obj", "bone", 2.208, 2.592, "ssl", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* bear skull; reduced 5067 -> 3009 tris */
    {"frontier", "m_horse_skull", "polyy-bones/obj/skull_14_horse.obj", "bone", 3.036, 3.564, "ssl", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* horse skull */
    {"badlands", "m_horse_skull", "polyy-bones/obj/skull_14_horse.obj", "bone", 3.036, 3.564, "ssl", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* horse skull */
    {"maahl", "m_horse_skull", "polyy-bones/obj/skull_14_horse.obj", "bone", 3.036, 3.564, "ssl", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* horse skull */
    {"frontier", "m_longhorn_skull", "polyy-bones/obj/skull_15_longhorn.obj", "bone", 6.624, 7.776, "s", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* longhorn steer skull, horns 1.2 m; reduced 4238 -> 3475 tris */
    {"badlands", "m_longhorn_skull", "polyy-bones/obj/skull_15_longhorn.obj", "bone", 6.624, 7.776, "s", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* longhorn steer skull, horns 1.2 m; reduced 4238 -> 3475 tris */
    {"highland", "m_ram_skull", "polyy-bones/obj/skull_16_ram.obj", "bone", 2.208, 2.592, "s", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* ram skull; reduced 4463 -> 3517 tris */
    {"maahl", "m_ram_skull", "polyy-bones/obj/skull_16_ram.obj", "bone", 2.208, 2.592, "s", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* ram skull; reduced 4463 -> 3517 tris */
    {"forest", "m_stag_skull", "polyy-bones/obj/skull_17_stag.obj", "bone", 4.968, 5.832, "s", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* stag skull with antlers; reduced 4844 -> 3192 tris */
    {"highland", "m_stag_skull", "polyy-bones/obj/skull_17_stag.obj", "bone", 4.968, 5.832, "s", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* stag skull with antlers; reduced 4844 -> 3192 tris */
    {"forest", "m_boar_skull", "polyy-bones/obj/skull_18_boar.obj", "bone", 1.932, 2.268, "ssl", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* boar skull; reduced 5456 -> 3272 tris */
    {"swamp", "m_boar_skull", "polyy-bones/obj/skull_18_boar.obj", "bone", 1.932, 2.268, "ssl", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* boar skull; reduced 5456 -> 3272 tris */
    {"badlands", "m_mammoth_skull", "polyy-bones/obj/skull_20_mammoth.obj", "bone", 6.624, 7.776, "s", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* mammoth skull; reduced 7049 -> 3646 tris */
    {"highland", "m_mammoth_skull", "polyy-bones/obj/skull_20_mammoth.obj", "bone", 6.624, 7.776, "s", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* mammoth skull; reduced 7049 -> 3646 tris */
    {"maahl", "m_mammoth_skull", "polyy-bones/obj/skull_20_mammoth.obj", "bone", 6.624, 7.776, "s", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* mammoth skull; reduced 7049 -> 3646 tris */
    {"swamp", "m_serpent_skull", "polyy-bones/obj/skull_22_serpent.obj", "bone", 5.52, 6.48, "s", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* giant serpent skull; reduced 6057 -> 3291 tris */
    {"thornwild", "m_serpent_skull", "polyy-bones/obj/skull_22_serpent.obj", "bone", 5.52, 6.48, "s", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* giant serpent skull; reduced 6057 -> 3291 tris */
    {"temple", "m_serpent_skull", "polyy-bones/obj/skull_22_serpent.obj", "bone", 5.52, 6.48, "s", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* giant serpent skull; reduced 6057 -> 3291 tris */
    {"badlands", "m_predator_skull", "polyy-bones/obj/skull_24_great_predator.obj", "bone", 8.28, 9.72, "s", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* great predator skull; reduced 4741 -> 3098 tris */
    {"thornwild", "m_predator_skull", "polyy-bones/obj/skull_24_great_predator.obj", "bone", 8.28, 9.72, "s", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* great predator skull; reduced 4741 -> 3098 tris */
    {"maahl", "m_predator_skull", "polyy-bones/obj/skull_24_great_predator.obj", "bone", 8.28, 9.72, "s", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* great predator skull; reduced 4741 -> 3098 tris */
    {"badlands", "m_frilled_skull", "polyy-bones/obj/skull_25_frilled.obj", "bone", 11.04, 12.96, "s", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* frilled horned beast skull */
    {"thornwild", "m_frilled_skull", "polyy-bones/obj/skull_25_frilled.obj", "bone", 11.04, 12.96, "s", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* frilled horned beast skull */
    {"dungeon", "m_dragon_skull", "polyy-bones/obj/skull_28_dragon.obj", "bone", 9.936, 11.664, "s", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* dragon skull centrepiece; reduced 4983 -> 3311 tris */
    {"highland", "m_dragon_skull", "polyy-bones/obj/skull_28_dragon.obj", "bone", 9.936, 11.664, "s", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* dragon skull centrepiece; reduced 4983 -> 3311 tris */
    {"cynder", "m_dragon_skull", "polyy-bones/obj/skull_28_dragon.obj", "tooth", 9.936, 11.664, "s", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* dragon skull centrepiece; reduced 4983 -> 3311 tris */
    {"maahl", "m_dragon_skull", "polyy-bones/obj/skull_28_dragon.obj", "bone", 9.936, 11.664, "s", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* dragon skull centrepiece; reduced 4983 -> 3311 tris */
    {"ashveil", "m_dragon_skull", "polyy-bones/obj/skull_28_dragon.obj", "bone", 9.936, 11.664, "s", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* dragon skull centrepiece; reduced 4983 -> 3311 tris */
    {"badlands", "m_dragon_skull", "polyy-bones/obj/skull_28_dragon.obj", "bone", 9.936, 11.664, "s", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* dragon skull centrepiece; reduced 4983 -> 3311 tris */
    {"graveyard", "m_femur", "polyy-bones/obj/bone_29_femur.obj", "bone", 2.484, 2.916, "s", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* femur */
    {"dungeon", "m_femur", "polyy-bones/obj/bone_29_femur.obj", "bone", 2.484, 2.916, "s", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* femur */
    {"swamp", "m_femur", "polyy-bones/obj/bone_29_femur.obj", "bone", 2.484, 2.916, "s", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* femur */
    {"badlands", "m_femur", "polyy-bones/obj/bone_29_femur.obj", "bone", 2.484, 2.916, "s", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* femur */
    {"blight", "m_femur", "polyy-bones/obj/bone_29_femur.obj", "bone", 2.484, 2.916, "s", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* femur */
    {"ashveil", "m_femur", "polyy-bones/obj/bone_29_femur.obj", "bone", 2.484, 2.916, "s", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* femur */
    {"trederra", "m_femur", "polyy-bones/obj/bone_29_femur.obj", "bone", 2.484, 2.916, "s", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* femur */
    {"sundered", "m_femur", "polyy-bones/obj/bone_29_femur.obj", "bone", 2.484, 2.916, "s", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* femur */
    {"thornwild", "m_femur", "polyy-bones/obj/bone_29_femur.obj", "bone", 2.484, 2.916, "s", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* femur */
    {"beast", "m_femur", "polyy-bones/obj/bone_29_femur.obj", "bone", 2.484, 2.916, "s", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* femur */
    {"cynder", "m_femur", "polyy-bones/obj/bone_29_femur.obj", "tooth", 2.484, 2.916, "s", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* femur */
    {"badlands", "m_beast_ribcage", "polyy-bones/obj/bone_31_ribcage.obj", "bone", 16.56, 19.44, "bf", "made", 0.0, 0.2, 0.22, 0.0, 0, 0, {}},          /* ribcage with spine, as a 3 m beast carcass; reduced 4275 ->  */
    {"ashveil", "m_beast_ribcage", "polyy-bones/obj/bone_31_ribcage.obj", "bone", 16.56, 19.44, "bf", "made", 0.0, 0.2, 0.22, 0.0, 0, 0, {}},          /* ribcage with spine, as a 3 m beast carcass; reduced 4275 ->  */
    {"blight", "m_beast_ribcage", "polyy-bones/obj/bone_31_ribcage.obj", "bone", 16.56, 19.44, "bf", "made", 0.0, 0.2, 0.22, 0.0, 0, 0, {}},          /* ribcage with spine, as a 3 m beast carcass; reduced 4275 ->  */
    {"infestation", "m_beast_ribcage", "polyy-bones/obj/bone_31_ribcage.obj", "spine", 16.56, 19.44, "bf", "made", 0.0, 0.2, 0.22, 0.0, 0, 0, {}},          /* ribcage with spine, as a 3 m beast carcass; reduced 4275 ->  */
    {"beast", "m_beast_ribcage", "polyy-bones/obj/bone_31_ribcage.obj", "bone", 16.56, 19.44, "bf", "made", 0.0, 0.2, 0.22, 0.0, 0, 0, {}},          /* ribcage with spine, as a 3 m beast carcass; reduced 4275 ->  */
    {"badlands", "m_vertebrae", "polyy-bones/obj/bone_32_spine.obj", "bone", 2.76, 3.24, "bf", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* spine segment; reduced 4257 -> 3539 tris */
    {"blight", "m_vertebrae", "polyy-bones/obj/bone_32_spine.obj", "bone", 2.76, 3.24, "bf", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* spine segment; reduced 4257 -> 3539 tris */
    {"infestation", "m_vertebrae", "polyy-bones/obj/bone_32_spine.obj", "spine", 2.76, 3.24, "bf", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* spine segment; reduced 4257 -> 3539 tris */
    {"badlands", "m_pelvis", "polyy-bones/obj/bone_33_pelvis.obj", "bone", 1.656, 1.944, "ssl", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* pelvis */
    {"dungeon", "m_pelvis", "polyy-bones/obj/bone_33_pelvis.obj", "bone", 1.656, 1.944, "ssl", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* pelvis */
    {"badlands", "m_jawbone", "polyy-bones/obj/bone_35_jawbone.obj", "bone", 1.656, 1.944, "s", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* jawbone with teeth */
    {"dungeon", "m_jawbone", "polyy-bones/obj/bone_35_jawbone.obj", "bone", 1.656, 1.944, "s", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* jawbone with teeth */
    {"coast", "m_jawbone", "polyy-bones/obj/bone_35_jawbone.obj", "bone", 1.656, 1.944, "s", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* jawbone with teeth */
    {"graveyard", "m_crossed_bones", "polyy-bones/obj/bone_36_crossed.obj", "bone", 2.76, 3.24, "s", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* crossed bones; reduced 4663 -> 3102 tris */
    {"dungeon", "m_crossed_bones", "polyy-bones/obj/bone_36_crossed.obj", "bone", 2.76, 3.24, "s", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* crossed bones; reduced 4663 -> 3102 tris */
    {"coast", "m_crossed_bones", "polyy-bones/obj/bone_36_crossed.obj", "bone", 2.76, 3.24, "s", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* crossed bones; reduced 4663 -> 3102 tris */
    {"dungeon", "m_bone_heap", "polyy-bones/obj/bone_37_heap.obj", "bone", 5.52, 6.48, "s", "debris", 0.0, 0.3, 0.22, 0.0, 0, 0, {}},          /* bone heap; reduced 7544 -> 2764 tris */
    {"blight", "m_bone_heap", "polyy-bones/obj/bone_37_heap.obj", "bone", 5.52, 6.48, "s", "debris", 0.0, 0.3, 0.22, 0.0, 0, 0, {}},          /* bone heap; reduced 7544 -> 2764 tris */
    {"infestation", "m_bone_heap", "polyy-bones/obj/bone_37_heap.obj", "spine", 5.52, 6.48, "s", "debris", 0.0, 0.3, 0.22, 0.0, 0, 0, {}},          /* bone heap; reduced 7544 -> 2764 tris */
    {"ashveil", "m_bone_heap", "polyy-bones/obj/bone_37_heap.obj", "bone", 5.52, 6.48, "s", "debris", 0.0, 0.3, 0.22, 0.0, 0, 0, {}},          /* bone heap; reduced 7544 -> 2764 tris */
    {"beast", "m_bone_heap", "polyy-bones/obj/bone_37_heap.obj", "bone", 5.52, 6.48, "s", "debris", 0.0, 0.3, 0.22, 0.0, 0, 0, {}},          /* bone heap; reduced 7544 -> 2764 tris */
    {"dungeon", "m_skull_bone_heap", "polyy-bones/obj/bone_38_skull_heap.obj", "bone", 5.52, 6.48, "s", "debris", 0.0, 0.3, 0.22, 0.0, 0, 0, {}},          /* skull on a bone heap; reduced 10764 -> 3385 tris */
    {"graveyard", "m_skull_bone_heap", "polyy-bones/obj/bone_38_skull_heap.obj", "bone", 5.52, 6.48, "s", "debris", 0.0, 0.3, 0.22, 0.0, 0, 0, {}},          /* skull on a bone heap; reduced 10764 -> 3385 tris */
    {"blight", "m_skull_bone_heap", "polyy-bones/obj/bone_38_skull_heap.obj", "bone", 5.52, 6.48, "s", "debris", 0.0, 0.3, 0.22, 0.0, 0, 0, {}},          /* skull on a bone heap; reduced 10764 -> 3385 tris */
    {"badlands", "m_mammoth_tusk", "polyy-bones/obj/bone_39_tusk.obj", "bone", 13.8, 16.2, "bf", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* curved tusk */
    {"highland", "m_mammoth_tusk", "polyy-bones/obj/bone_39_tusk.obj", "bone", 13.8, 16.2, "bf", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* curved tusk */
    {"maahl", "m_mammoth_tusk", "polyy-bones/obj/bone_39_tusk.obj", "bone", 13.8, 16.2, "bf", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* curved tusk */
    {"forest", "m_shed_antler", "polyy-bones/obj/bone_40_antler.obj", "bone", 3.864, 4.536, "bf", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* shed antler; reduced 4402 -> 2866 tris */
    {"highland", "m_shed_antler", "polyy-bones/obj/bone_40_antler.obj", "bone", 3.864, 4.536, "bf", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* shed antler; reduced 4402 -> 2866 tris */
    {"coast", "m_ammonite", "polyy-bones/obj/fossil_46_ammonite.obj", "shell", 2.76, 3.24, "ssbf", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* ammonite */
    {"badlands", "m_ammonite", "polyy-bones/obj/fossil_46_ammonite.obj", "bone", 2.76, 3.24, "ssbf", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* ammonite */
    {"mine", "m_ammonite", "polyy-bones/obj/fossil_46_ammonite.obj", "bone", 2.76, 3.24, "ssbf", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* ammonite */
    {"coast", "m_cut_ammonite", "polyy-bones/obj/fossil_47_ammonite_half.obj", "shell", 2.76, 3.24, "ssbf", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* cut ammonite; reduced 4657 -> 2850 tris */
    {"badlands", "m_cut_ammonite", "polyy-bones/obj/fossil_47_ammonite_half.obj", "bone", 2.76, 3.24, "ssbf", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* cut ammonite; reduced 4657 -> 2850 tris */
    {"mine", "m_cut_ammonite", "polyy-bones/obj/fossil_47_ammonite_half.obj", "bone", 2.76, 3.24, "ssbf", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* cut ammonite; reduced 4657 -> 2850 tris */
    {"coast", "m_ammonite_cluster", "polyy-bones/obj/fossil_48_ammonite_cluster.obj", "shell", 2.76, 3.24, "s", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* ammonite cluster; reduced 4256 -> 3015 tris */
    {"badlands", "m_ammonite_cluster", "polyy-bones/obj/fossil_48_ammonite_cluster.obj", "bone", 2.76, 3.24, "s", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* ammonite cluster; reduced 4256 -> 3015 tris */
    {"coast", "m_trilobite", "polyy-bones/obj/fossil_49_trilobite.obj", "shell", 2.208, 2.592, "s", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* trilobite; also an alien crawler husk; reduced 4024 -> 3668  */
    {"badlands", "m_trilobite", "polyy-bones/obj/fossil_49_trilobite.obj", "bone", 2.208, 2.592, "s", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* trilobite; also an alien crawler husk; reduced 4024 -> 3668  */
    {"obelis", "m_trilobite", "polyy-bones/obj/fossil_49_trilobite.obj", "bone", 2.208, 2.592, "s", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* trilobite; also an alien crawler husk; reduced 4024 -> 3668  */
    {"infestation", "m_trilobite", "polyy-bones/obj/fossil_49_trilobite.obj", "carapace", 2.208, 2.592, "s", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* trilobite; also an alien crawler husk; reduced 4024 -> 3668  */
    {"badlands", "m_petrified_log", "polyy-bones/obj/fossil_50_petrified_wood.obj", "rock", 4.416, 5.184, "sslr", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* petrified log section; reduced 4483 -> 3653 tris */
    {"maahl", "m_petrified_log", "polyy-bones/obj/fossil_50_petrified_wood.obj", "sandstone", 4.416, 5.184, "sslr", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* petrified log section; reduced 4483 -> 3653 tris */
    {"badlands", "m_fossil_fish", "polyy-bones/obj/fossil_51_fish_plate.obj", "rock", 3.312, 3.888, "s", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* fossil fish slab; reduced 4949 -> 2982 tris */
    {"coast", "m_fossil_fish", "polyy-bones/obj/fossil_51_fish_plate.obj", "sandstone", 3.312, 3.888, "s", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* fossil fish slab; reduced 4949 -> 2982 tris */
    {"badlands", "m_fossil_fern", "polyy-bones/obj/fossil_52_fern_plate.obj", "rock", 3.312, 3.888, "s", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* fossil fern slab; reduced 4459 -> 3187 tris */
    {"maahl", "m_fossil_fern", "polyy-bones/obj/fossil_52_fern_plate.obj", "sandstone", 3.312, 3.888, "s", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* fossil fern slab; reduced 4459 -> 3187 tris */
    {"coast", "m_sea_beast_jaw", "polyy-bones/obj/bone_53_sea_beast_jaw.obj", "bone", 5.52, 6.48, "s", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* sea-beast jaw section; reduced 4448 -> 3653 tris */
    {"badlands", "m_sea_beast_jaw", "polyy-bones/obj/bone_53_sea_beast_jaw.obj", "bone", 5.52, 6.48, "s", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* sea-beast jaw section; reduced 4448 -> 3653 tris */
    {"coast", "m_turtle_shell", "polyy-bones/obj/fossil_54_turtle_shell.obj", "shell", 6.624, 7.776, "s", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* giant turtle shell */
    {"swamp", "m_turtle_shell", "polyy-bones/obj/fossil_54_turtle_shell.obj", "bone", 6.624, 7.776, "s", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* giant turtle shell */
    {"highland", "m_great_maul", "polyy-weapons/obj/giants_maul.obj", "brass", 7.728, 9.072, "lr", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* great maul, lying; reduced 7840 -> 3398 tris */
    {"badlands", "m_great_maul", "polyy-weapons/obj/giants_maul.obj", "iron", 7.728, 9.072, "lr", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* great maul, lying; reduced 7840 -> 3398 tris */
    {"highland", "m_war_axe", "polyy-weapons/obj/war_axe.obj", "brass", 5.52, 6.48, "bf", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* heavy war axe, lying; reduced 8432 -> 3167 tris */
    {"dungeon", "m_war_axe", "polyy-weapons/obj/war_axe.obj", "iron", 5.52, 6.48, "bf", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* heavy war axe, lying; reduced 8432 -> 3167 tris */
    {"dungeon", "m_great_hammer", "polyy-weapons/obj/great_hammer.obj", "iron", 6.072, 7.128, "bf", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* great_hammer, lying; reduced 6885 -> 2845 tris */
    {"highland", "m_great_hammer", "polyy-weapons/obj/great_hammer.obj", "brass", 6.072, 7.128, "bf", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* great_hammer, lying; reduced 6885 -> 2845 tris */
    {"badlands", "m_bone_club", "polyy-weapons/obj/bone_club.obj", "bone", 5.52, 6.48, "lr", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* bone club, lying; reduced 7128 -> 2960 tris */
    {"swamp", "m_bone_club", "polyy-weapons/obj/bone_club.obj", "bone", 5.52, 6.48, "lr", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* bone club, lying; reduced 7128 -> 2960 tris */
    {"dungeon", "m_bone_club", "polyy-weapons/obj/bone_club.obj", "bone", 5.52, 6.48, "lr", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* bone club, lying; reduced 7128 -> 2960 tris */
    {"badlands", "m_brute_cleaver", "polyy-weapons/obj/brute_cleaver.obj", "iron", 6.624, 7.776, "bf", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* wide brute cleaver, lying; reduced 11884 -> 3381 tris */
    {"blight", "m_brute_cleaver", "polyy-weapons/obj/brute_cleaver.obj", "iron", 6.624, 7.776, "bf", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* wide brute cleaver, lying; reduced 11884 -> 3381 tris */
    {"dungeon", "m_brute_cleaver", "polyy-weapons/obj/brute_cleaver.obj", "iron", 6.624, 7.776, "bf", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* wide brute cleaver, lying; reduced 11884 -> 3381 tris */
    {"highland", "m_double_bit_axe", "polyy-weapons/obj/double_axe.obj", "brass", 7.176, 8.424, "bf", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* double-bit axe, lying; reduced 7883 -> 3161 tris */
    {"dungeon", "m_double_bit_axe", "polyy-weapons/obj/double_axe.obj", "iron", 7.176, 8.424, "bf", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* double-bit axe, lying; reduced 7883 -> 3161 tris */
    {"highland", "m_claymore", "polyy-weapons/obj/claymore.obj", "brass", 7.728, 9.072, "lr", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* broad two-hander, lying; reduced 7504 -> 3356 tris */
    {"graveyard", "m_claymore", "polyy-weapons/obj/claymore.obj", "iron", 7.728, 9.072, "lr", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* broad two-hander, lying; reduced 7504 -> 3356 tris */
    {"dungeon", "m_iron_bound_chest", "avramania/obj/dungeon_chest.obj", "wood", 4.416, 5.184, "s", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* iron-bound chest */
    {"coast", "m_iron_bound_chest", "avramania/obj/dungeon_chest.obj", "drift", 4.416, 5.184, "s", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* iron-bound chest */
    {"dungeon", "m_block_rubble", "avramania/obj/dungeon_rubble.obj", "flag", 5.52, 6.48, "s", "debris", 0.0, 0.3, 0.22, 0.0, 0, 0, {}},          /* block rubble pile */
    {"temple", "m_block_rubble", "avramania/obj/dungeon_rubble.obj", "tcolumn", 5.52, 6.48, "s", "debris", 0.0, 0.3, 0.22, 0.0, 0, 0, {}},          /* block rubble pile */
    {"sundered", "m_block_rubble", "avramania/obj/dungeon_rubble.obj", "rock", 5.52, 6.48, "s", "debris", 0.0, 0.3, 0.22, 0.0, 0, 0, {}},          /* block rubble pile */
    {"urban", "m_block_rubble", "avramania/obj/dungeon_rubble.obj", "concrete", 5.52, 6.48, "s", "debris", 0.0, 0.3, 0.22, 0.0, 0, 0, {}},          /* block rubble pile */
    {"fortress", "m_block_rubble", "avramania/obj/dungeon_rubble.obj", "garden", 5.52, 6.48, "s", "debris", 0.0, 0.3, 0.22, 0.0, 0, 0, {}},          /* block rubble pile */
    {"dungeon", "m_plain_skull", "avramania/obj/dungeon_skull.obj", "bone", 0.994, 1.166, "ssl", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* low-poly skull */
    {"graveyard", "m_plain_skull", "avramania/obj/dungeon_skull.obj", "bone", 0.994, 1.166, "ssl", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* low-poly skull */
    {"temple", "m_square_pillar", "avramania/obj/dungeon_pillar.obj", "tcolumn", 13.248, 15.552, "s", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* square stone pillar */
    {"dungeon", "m_square_pillar", "avramania/obj/dungeon_pillar.obj", "flag", 13.248, 15.552, "s", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* square stone pillar */
    {"temple", "m_tripod_brazier", "avramania/obj/brazier.obj", "gold", 6.624, 7.776, "s", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* tripod brazier; legs about 1 mm at true size */
    {"dungeon", "m_tripod_brazier", "avramania/obj/brazier.obj", "iron", 6.624, 7.776, "s", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* tripod brazier; legs about 1 mm at true size */
    {"frontier", "m_feed_trough", "avramania/obj/feed_trough.obj", "wood", 6.624, 7.776, "s", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* feed trough */
    {"frontier", "m_hay_bale_b", "avramania/obj/hay_bale.obj", "sack", 4.471, 5.249, "s", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* hay bale */
    {"highland", "m_hay_bale", "avramania/obj/hay_bale.obj", "sack", 4.471, 5.249, "s", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* hay bale */
    {"frontier", "m_haystack", "avramania/obj/haystack.obj", "sack", 7.176, 8.424, "s", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* haystack */
    {"highland", "m_haystack", "avramania/obj/haystack.obj", "sack", 7.176, 8.424, "s", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* haystack */
    {"frontier", "m_scarecrow", "avramania/obj/scarecrow.obj", "wood", 10.488, 12.312, "s", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* scarecrow */
    {"graveyard", "m_scarecrow", "avramania/obj/scarecrow.obj", "wood", 10.488, 12.312, "s", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* scarecrow */
    {"coast", "m_small_cask", "avramania/obj/cask.obj", "drift", 2.374, 2.786, "ssl", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* small cask */
    {"dungeon", "m_small_cask", "avramania/obj/cask.obj", "wood", 2.374, 2.786, "ssl", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* small cask */
    {"frontier", "m_small_cask", "avramania/obj/cask.obj", "wood", 2.374, 2.786, "ssl", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* small cask */
    {"frontier", "m_grain_sack", "avramania/obj/grain_sack.obj", "sack", 2.87, 3.37, "s", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* grain sack */
    {"fortress", "m_grain_sack", "avramania/obj/grain_sack.obj", "sack", 2.87, 3.37, "s", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* grain sack */
    {"temple", "m_stone_altar", "avramania/obj/altar.obj", "tcolumn", 7.728, 9.072, "s", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* stone altar */
    {"dungeon", "m_stone_altar", "avramania/obj/altar.obj", "flag", 7.728, 9.072, "s", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* stone altar */
    {"mine", "m_anvil", "avramania/obj/anvil_block.obj", "iron", 3.974, 4.666, "s", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* anvil on a block */
    {"swamp", "m_cauldron", "avramania/obj/cauldron.obj", "iron", 3.754, 4.406, "s", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* cauldron */
    {"dungeon", "m_cauldron", "avramania/obj/cauldron.obj", "iron", 3.754, 4.406, "s", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* cauldron */
    {"dungeon", "m_plank_chest", "avramania/obj/plank_chest.obj", "wood", 4.637, 5.443, "s", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* plank chest */
    {"coast", "m_plank_chest", "avramania/obj/plank_chest.obj", "drift", 4.637, 5.443, "s", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* plank chest */
    {"forest", "m_woodpile", "avramania/obj/woodpile.obj", "log", 5.52, 6.48, "s", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* woodpile */
    {"frontier", "m_woodpile", "avramania/obj/woodpile.obj", "wood", 5.52, 6.48, "s", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* woodpile */
    {"highland", "m_woodpile", "avramania/obj/woodpile.obj", "wood", 5.52, 6.48, "s", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* woodpile */
    {"highland", "m_round_boulder", "avramania/obj/boulder.obj", "slate", 8.28, 9.72, "s", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* boulder */
    {"forest", "m_round_boulder", "avramania/obj/boulder.obj", "rock", 8.28, 9.72, "s", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* boulder */
    {"swamp", "m_round_boulder", "avramania/obj/boulder.obj", "rock", 8.28, 9.72, "s", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* boulder */
    {"coast", "m_round_boulder", "avramania/obj/boulder.obj", "sandstone", 8.28, 9.72, "s", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* boulder */
    {"badlands", "m_round_boulder", "avramania/obj/boulder.obj", "rock", 8.28, 9.72, "s", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* boulder */
    {"mine", "m_round_boulder", "avramania/obj/boulder.obj", "rock", 8.28, 9.72, "s", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* boulder */
    {"frontier", "m_round_boulder", "avramania/obj/boulder.obj", "rock", 8.28, 9.72, "s", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* boulder */
    {"cynder", "m_round_boulder", "avramania/obj/boulder.obj", "basalt", 8.28, 9.72, "s", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* boulder */
    {"forest", "m_round_bush", "avramania/obj/bush.obj", "shrub", 6.348, 7.452, "s", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* round bush (solid blob) */
    {"highland", "m_round_bush", "avramania/obj/bush.obj", "shrub", 6.348, 7.452, "s", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* round bush (solid blob) */
    {"swamp", "m_round_bush", "avramania/obj/bush.obj", "shrub", 6.348, 7.452, "s", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* round bush (solid blob) */
    {"forest", "m_branchy_log", "avramania/obj/fallen_log.obj", "log", 14.352, 16.848, "s", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* fallen log with branch stubs */
    {"swamp", "m_branchy_log", "avramania/obj/fallen_log.obj", "log", 14.352, 16.848, "s", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* fallen log with branch stubs */
    {"highland", "m_branchy_log", "avramania/obj/fallen_log.obj", "wood", 14.352, 16.848, "s", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* fallen log with branch stubs */
    {"forest", "m_mushroom_cluster", "avramania/obj/mushrooms.obj", "shroom", 2.65, 3.11, "s", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* mushroom cluster */
    {"swamp", "m_mushroom_cluster", "avramania/obj/mushrooms.obj", "shroom", 2.65, 3.11, "s", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* mushroom cluster */
    {"thornwild", "m_mushroom_cluster", "avramania/obj/mushrooms.obj", "wood", 2.65, 3.11, "s", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* mushroom cluster */
    {"forest", "m_pebble_scatter", "avramania/obj/pebbles.obj", "rock", 6.072, 7.128, "s", "debris", 0.0, 0.3, 0.22, 0.0, 0, 0, {}},          /* pebble scatter */
    {"highland", "m_pebble_scatter", "avramania/obj/pebbles.obj", "slate", 6.072, 7.128, "s", "debris", 0.0, 0.3, 0.22, 0.0, 0, 0, {}},          /* pebble scatter */
    {"coast", "m_pebble_scatter", "avramania/obj/pebbles.obj", "sandstone", 6.072, 7.128, "s", "debris", 0.0, 0.3, 0.22, 0.0, 0, 0, {}},          /* pebble scatter */
    {"badlands", "m_pebble_scatter", "avramania/obj/pebbles.obj", "rock", 6.072, 7.128, "s", "debris", 0.0, 0.3, 0.22, 0.0, 0, 0, {}},          /* pebble scatter */
    {"mine", "m_pebble_scatter", "avramania/obj/pebbles.obj", "rock", 6.072, 7.128, "s", "debris", 0.0, 0.3, 0.22, 0.0, 0, 0, {}},          /* pebble scatter */
    {"swamp", "m_pebble_scatter", "avramania/obj/pebbles.obj", "rock", 6.072, 7.128, "s", "debris", 0.0, 0.3, 0.22, 0.0, 0, 0, {}},          /* pebble scatter */
    {"frontier", "m_pebble_scatter", "avramania/obj/pebbles.obj", "rock", 6.072, 7.128, "s", "debris", 0.0, 0.3, 0.22, 0.0, 0, 0, {}},          /* pebble scatter */
    {"maahl", "m_pebble_scatter", "avramania/obj/pebbles.obj", "sandstone", 6.072, 7.128, "s", "debris", 0.0, 0.3, 0.22, 0.0, 0, 0, {}},          /* pebble scatter */
    {"forest", "m_mossy_rock", "avramania/obj/rock_mossy.obj", "rock", 7.176, 8.424, "s", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* mossy rock */
    {"swamp", "m_mossy_rock", "avramania/obj/rock_mossy.obj", "rock", 7.176, 8.424, "s", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* mossy rock */
    {"highland", "m_mossy_rock", "avramania/obj/rock_mossy.obj", "slate", 7.176, 8.424, "s", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* mossy rock */
    {"highland", "m_stacked_rocks", "avramania/obj/rocks_stacked.obj", "slate", 6.072, 7.128, "s", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* stacked rocks */
    {"forest", "m_stacked_rocks", "avramania/obj/rocks_stacked.obj", "rock", 6.072, 7.128, "s", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* stacked rocks */
    {"coast", "m_stacked_rocks", "avramania/obj/rocks_stacked.obj", "sandstone", 6.072, 7.128, "s", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* stacked rocks */
    {"badlands", "m_stacked_rocks", "avramania/obj/rocks_stacked.obj", "rock", 6.072, 7.128, "s", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* stacked rocks */
    {"mine", "m_stacked_rocks", "avramania/obj/rocks_stacked.obj", "rock", 6.072, 7.128, "s", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* stacked rocks */
    {"frontier", "m_stacked_rocks", "avramania/obj/rocks_stacked.obj", "rock", 6.072, 7.128, "s", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* stacked rocks */
    {"highland", "m_flat_slab", "avramania/obj/slab.obj", "slate", 8.832, 10.368, "s", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* flat rock slab */
    {"coast", "m_flat_slab", "avramania/obj/slab.obj", "sandstone", 8.832, 10.368, "s", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* flat rock slab */
    {"badlands", "m_flat_slab", "avramania/obj/slab.obj", "rock", 8.832, 10.368, "s", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* flat rock slab */
    {"swamp", "m_stepping_stones", "avramania/obj/stepping_stones.obj", "rock", 17.112, 20.088, "s", "made", 0.0, 0.2, 0.22, 0.0, 0, 0, {}},          /* row of stepping stones */
    {"coast", "m_stepping_stones", "avramania/obj/stepping_stones.obj", "sandstone", 17.112, 20.088, "s", "made", 0.0, 0.2, 0.22, 0.0, 0, 0, {}},          /* row of stepping stones */
    {"swamp", "m_stream_rock", "avramania/obj/stream_rock.obj", "rock", 8.28, 9.72, "s", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* low water-worn rock */
    {"coast", "m_stream_rock", "avramania/obj/stream_rock.obj", "sandstone", 8.28, 9.72, "s", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* low water-worn rock */
    {"forest", "m_root_stump", "avramania/obj/stump.obj", "log", 8.832, 10.368, "s", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* tree stump with roots */
    {"swamp", "m_root_stump", "avramania/obj/stump.obj", "log", 8.832, 10.368, "s", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* tree stump with roots */
    {"graveyard", "m_root_stump", "avramania/obj/stump.obj", "wood", 8.832, 10.368, "s", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* tree stump with roots */
    {"forest", "m_pine_sapling", "avramania/obj/pine_sapling.obj", "shrub", 7.728, 9.072, "s", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* young pine */
    {"highland", "m_pine_sapling", "avramania/obj/pine_sapling.obj", "shrub", 7.728, 9.072, "s", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* young pine */
    {"frontier", "m_stave_barrel", "avramania/obj/barrel.obj", "wood", 4.306, 5.054, "ssl", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* barrel */
    {"dungeon", "m_stave_barrel", "avramania/obj/barrel.obj", "wood", 4.306, 5.054, "ssl", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* barrel */
    {"coast", "m_stave_barrel", "avramania/obj/barrel.obj", "drift", 4.306, 5.054, "ssl", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* barrel */
    {"mine", "m_stave_barrel", "avramania/obj/barrel.obj", "wood", 4.306, 5.054, "ssl", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* barrel */
    {"frontier", "m_cart_wheel", "avramania/obj/cart_wheel.obj", "wood", 4.637, 5.443, "lr", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* cart wheel, lying */
    {"mine", "m_cart_wheel", "avramania/obj/cart_wheel.obj", "wood", 4.637, 5.443, "lr", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* cart wheel, lying */
    {"frontier", "m_small_crate", "avramania/obj/crate.obj", "wood", 2.926, 3.434, "s", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* small crate */
    {"coast", "m_small_crate", "avramania/obj/crate.obj", "drift", 2.926, 3.434, "s", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* small crate */
    {"dungeon", "m_small_crate", "avramania/obj/crate.obj", "wood", 2.926, 3.434, "s", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* small crate */
    {"frontier", "m_rail_fence", "avramania/obj/fence_rail.obj", "wood", 7.176, 8.424, "s", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* wooden rail fence section */
    {"highland", "m_rail_fence", "avramania/obj/fence_rail.obj", "wood", 7.176, 8.424, "s", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* wooden rail fence section */
    {"graveyard", "m_wooden_marker", "avramania/obj/grave_marker.obj", "wood", 4.416, 5.184, "s", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* wooden grave marker */
    {"frontier", "m_wooden_marker", "avramania/obj/grave_marker.obj", "wood", 4.416, 5.184, "s", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* wooden grave marker */
    {"coast", "m_rope_coil", "avramania/obj/rope_coil.obj", "rope", 3.643, 4.277, "s", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* rope coil */
    {"mine", "m_rope_coil", "avramania/obj/rope_coil.obj", "rope", 3.643, 4.277, "s", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* rope coil */
    {"frontier", "m_sack_b", "avramania/obj/sack.obj", "sack", 3.533, 4.147, "s", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* sack */
    {"mine", "m_sack_b", "avramania/obj/sack.obj", "sack", 3.533, 4.147, "s", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* sack */
    {"frontier", "m_fingerpost", "avramania/obj/signpost.obj", "wood", 11.592, 13.608, "s", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* signpost */
    {"highland", "m_fingerpost", "avramania/obj/signpost.obj", "wood", 11.592, 13.608, "s", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* signpost */
    {"graveyard", "m_plain_tombstone", "avramania/obj/tombstone.obj", "headstone", 5.52, 6.48, "s", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* simple tombstone */
    {"highland", "m_dry_stone_wall", "avramania/obj/wall_stone.obj", "slate", 5.52, 6.48, "s", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* dry-stone wall, 1 m */
    {"forest", "m_dry_stone_wall", "avramania/obj/wall_stone.obj", "rock", 5.52, 6.48, "s", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* dry-stone wall, 1 m */
    {"frontier", "m_plank_trough", "avramania/obj/water_trough.obj", "wood", 7.176, 8.424, "s", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* water trough */
    {"mine", "m_wheelbarrow", "avramania/obj/wheelbarrow.obj", "wood", 8.28, 9.72, "s", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* wheelbarrow */
    {"frontier", "m_wheelbarrow", "avramania/obj/wheelbarrow.obj", "wood", 8.28, 9.72, "s", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* wheelbarrow */
    {"urban", "m_timber_beam", "avramania/obj/beam.obj", "wood", 11.04, 12.96, "s", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* fallen squared timber beam */
    {"sundered", "m_timber_beam", "avramania/obj/beam.obj", "wood", 11.04, 12.96, "s", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* fallen squared timber beam */
    {"frontier", "m_timber_beam", "avramania/obj/beam.obj", "wood", 11.04, 12.96, "s", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* fallen squared timber beam */
    {"frontier", "m_chimney_stack", "avramania/obj/chimney.obj", "rock", 11.592, 13.608, "s", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* stone chimney stack of a burnt-out house */
    {"highland", "m_chimney_stack", "avramania/obj/chimney.obj", "slate", 11.592, 13.608, "s", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* stone chimney stack of a burnt-out house */
    {"forest", "m_log_fire", "avramania/obj/campfire_logs.obj", "log", 3.036, 3.564, "s", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* campfire logs, unlit */
    {"frontier", "m_log_fire", "avramania/obj/campfire_logs.obj", "wood", 3.036, 3.564, "s", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* campfire logs, unlit */
    {"highland", "m_log_fire", "avramania/obj/campfire_logs.obj", "wood", 3.036, 3.564, "s", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* campfire logs, unlit */
    {"frontier", "m_roofed_well", "avramania/obj/well.obj", "rock", 12.144, 14.256, "s", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* roofed well */
    {"highland", "m_roofed_well", "avramania/obj/well.obj", "slate", 12.144, 14.256, "s", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* roofed well */
    {"highland", "m_targe", "avramania/obj/shield_round.obj", "wood", 3.588, 4.212, "bf", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* round shield, lying */
    {"highland", "m_plank_shield", "avramania/obj/shield_wooden.obj", "wood", 3.036, 3.564, "bf", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* wooden shield, lying */
    {"dungeon", "m_plank_shield", "avramania/obj/shield_wooden.obj", "wood", 3.036, 3.564, "bf", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* wooden shield, lying */
    {"dungeon", "m_buckler", "avramania/obj/buckler.obj", "iron", 1.932, 2.268, "bf", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* buckler, lying */
    {"frontier", "m_hooped_barrel", "frontier-town/obj/barrel.obj", "wood", 5.078, 5.962, "ssl", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* barrel */
    {"mine", "m_hooped_barrel", "frontier-town/obj/barrel.obj", "wood", 5.078, 5.962, "ssl", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* barrel */
    {"badlands", "m_hooped_barrel", "frontier-town/obj/barrel.obj", "wood", 5.078, 5.962, "ssl", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* barrel */
    {"frontier", "m_crate", "frontier-town/obj/crate.obj", "wood", 5.52, 6.48, "s", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* crate */
    {"mine", "m_crate", "frontier-town/obj/crate.obj", "wood", 5.52, 6.48, "s", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* crate */
    {"mine", "m_powder_keg", "frontier-town/obj/powder_keg.obj", "wood", 3.698, 4.342, "ssl", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* powder keg */
    {"frontier", "m_powder_keg", "frontier-town/obj/powder_keg.obj", "wood", 3.698, 4.342, "ssl", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* powder keg */
    {"frontier", "m_hay_block", "frontier-town/obj/hay_bale.obj", "sack", 5.52, 6.48, "s", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* hay bale */
    {"frontier", "m_water_trough", "frontier-town/obj/water_trough.obj", "wood", 8.832, 10.368, "s", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* water trough */
    {"frontier", "m_anvil_stump", "frontier-town/obj/anvil.obj", "iron", 4.858, 5.702, "s", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* anvil on a stump */
    {"mine", "m_anvil_stump", "frontier-town/obj/anvil.obj", "iron", 4.858, 5.702, "s", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* anvil on a stump */
    {"frontier", "m_spare_wheel", "frontier-town/obj/wagon_wheel.obj", "wood", 5.52, 6.48, "lr", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* spare wagon wheel, lying */
    {"badlands", "m_spare_wheel", "frontier-town/obj/wagon_wheel.obj", "wood", 5.52, 6.48, "lr", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* spare wagon wheel, lying */
    {"badlands", "m_desert_rocks", "frontier-town/obj/desert_rocks.obj", "rock", 9.384, 11.016, "s", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* desert rock group */
    {"maahl", "m_desert_rocks", "frontier-town/obj/desert_rocks.obj", "sandstone", 9.384, 11.016, "s", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* desert rock group */
    {"frontier", "m_desert_rocks", "frontier-town/obj/desert_rocks.obj", "rock", 9.384, 11.016, "s", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* desert rock group */
    {"badlands", "m_saguaro_b", "frontier-town/obj/saguaro_small.obj", "shrub", 10.488, 12.312, "s", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* small saguaro */
    {"frontier", "m_saguaro", "frontier-town/obj/saguaro_small.obj", "shrub", 10.488, 12.312, "s", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* small saguaro */
    {"badlands", "m_tall_saguaro", "frontier-town/obj/saguaro_tall.obj", "shrub", 17.112, 20.088, "s", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* tall saguaro, large bases */
    {"frontier", "m_tall_saguaro", "frontier-town/obj/saguaro_tall.obj", "shrub", 17.112, 20.088, "s", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* tall saguaro, large bases */
    {"badlands", "m_prickly_pear_b", "frontier-town/obj/prickly_pear.obj", "shrub", 4.858, 5.702, "s", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* prickly pear */
    {"frontier", "m_prickly_pear", "frontier-town/obj/prickly_pear.obj", "shrub", 4.858, 5.702, "s", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* prickly pear */
    {"mine", "m_ore_cart", "frontier-town/obj/mine_cart.obj", "iron", 7.728, 9.072, "s", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* ore cart */
    {"mine", "m_rail_track", "frontier-town/obj/rail_track.obj", "iron", 11.04, 12.96, "s", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* 2 m mine rail section */
    {"frontier", "m_hitching_post", "frontier-town/obj/hitching_post.obj", "wood", 11.04, 12.96, "s", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* hitching post */
    {"frontier", "m_fence_2m", "frontier-town/obj/fence.obj", "wood", 11.04, 12.96, "s", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* 2 m rail fence */
    {"highland", "m_fence_2m", "frontier-town/obj/fence.obj", "wood", 11.04, 12.96, "s", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* 2 m rail fence */
    {"frontier", "m_direction_sign", "frontier-town/obj/signpost.obj", "wood", 13.248, 15.552, "s", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* signpost */
    {"badlands", "m_direction_sign", "frontier-town/obj/signpost.obj", "wood", 13.248, 15.552, "s", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* signpost */
    {"mine", "m_barrow", "frontier-town/obj/wheelbarrow.obj", "wood", 9.384, 11.016, "s", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* wheelbarrow */
    {"frontier", "m_barrow", "frontier-town/obj/wheelbarrow.obj", "wood", 9.384, 11.016, "s", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* wheelbarrow */
    {"badlands", "m_cow_skull", "frontier-town/obj/cow_skull.obj", "bone", 4.637, 5.443, "s", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* cow skull */
    {"frontier", "m_cow_skull", "frontier-town/obj/cow_skull.obj", "bone", 4.637, 5.443, "s", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* cow skull */
    {"frontier", "m_town_well", "frontier-town/obj/well.obj", "wood", 12.144, 14.256, "s", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* roofed well */
    {"mine", "m_mine_entrance", "frontier-town/obj/mine_entrance.obj", "wood", 17.333, 20.347, "s", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* timbered mine entrance, large bases */
    {"badlands", "m_mine_entrance", "frontier-town/obj/mine_entrance.obj", "wood", 17.333, 20.347, "s", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* timbered mine entrance, large bases */
    {"urban", "m_fuel_drum", "kaykit-resource-bits/obj/oil_drum.obj", "iron", 4.968, 5.832, "ssl", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* oil drum */
    {"ferrum", "m_fuel_drum", "kaykit-resource-bits/obj/oil_drum.obj", "iron", 4.968, 5.832, "ssl", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* oil drum */
    {"trederra", "m_fuel_drum", "kaykit-resource-bits/obj/oil_drum.obj", "iron", 4.968, 5.832, "ssl", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* oil drum */
    {"targa", "m_fuel_drum", "kaykit-resource-bits/obj/oil_drum.obj", "metal", 4.968, 5.832, "ssl", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* oil drum */
    {"ashveil", "m_fuel_drum", "kaykit-resource-bits/obj/oil_drum.obj", "iron", 4.968, 5.832, "ssl", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* oil drum */
    {"urban", "m_dented_drum", "kaykit-resource-bits/obj/oil_drum_dented.obj", "iron", 4.968, 5.832, "ssl", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* dented oil drum */
    {"ashveil", "m_dented_drum", "kaykit-resource-bits/obj/oil_drum_dented.obj", "iron", 4.968, 5.832, "ssl", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* dented oil drum */
    {"ferrum", "m_dented_drum", "kaykit-resource-bits/obj/oil_drum_dented.obj", "iron", 4.968, 5.832, "ssl", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* dented oil drum */
    {"trederra", "m_dented_drum", "kaykit-resource-bits/obj/oil_drum_dented.obj", "iron", 4.968, 5.832, "ssl", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* dented oil drum */
    {"ferrum", "m_drum_cluster", "kaykit-resource-bits/obj/oil_drums_four.obj", "iron", 8.28, 9.72, "s", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* four drums strapped together; reduced 4400 -> 3176 tris */
    {"urban", "m_drum_cluster", "kaykit-resource-bits/obj/oil_drums_four.obj", "iron", 8.28, 9.72, "s", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* four drums strapped together; reduced 4400 -> 3176 tris */
    {"corridor", "m_drum_cluster", "kaykit-resource-bits/obj/oil_drums_four.obj", "metal", 8.28, 9.72, "s", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* four drums strapped together; reduced 4400 -> 3176 tris */
    {"urban", "m_fuel_can", "kaykit-resource-bits/obj/jerrycan.obj", "iron", 2.484, 2.916, "ssl", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* jerrycan */
    {"trederra", "m_fuel_can", "kaykit-resource-bits/obj/jerrycan.obj", "iron", 2.484, 2.916, "ssl", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* jerrycan */
    {"targa", "m_fuel_can", "kaykit-resource-bits/obj/jerrycan.obj", "metal", 2.484, 2.916, "ssl", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* jerrycan */
    {"ashveil", "m_fuel_can", "kaykit-resource-bits/obj/jerrycan.obj", "iron", 2.484, 2.916, "ssl", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* jerrycan */
    {"mine", "m_ore_lump", "kaykit-resource-bits/obj/ore_lump.obj", "rock", 2.594, 3.046, "s", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* ore lump */
    {"mine", "m_ore_scatter", "kaykit-resource-bits/obj/ore_scatter.obj", "rock", 5.134, 6.026, "s", "debris", 0.0, 0.3, 0.22, 0.0, 0, 0, {}},          /* ore scatter */
    {"mine", "m_ingot_stack", "kaykit-resource-bits/obj/ingot_stack.obj", "brass", 4.416, 5.184, "s", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* ingot stack */
    {"dungeon", "m_ingot_stack", "kaykit-resource-bits/obj/ingot_stack.obj", "gold", 4.416, 5.184, "s", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* ingot stack */
    {"mine", "m_ingot_heap", "kaykit-resource-bits/obj/ingot_heap.obj", "brass", 7.176, 8.424, "s", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* ingot heap */
    {"dungeon", "m_ingot_heap", "kaykit-resource-bits/obj/ingot_heap.obj", "gold", 7.176, 8.424, "s", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* ingot heap */
    {"fortress", "m_cut_stone_stack", "kaykit-resource-bits/obj/stone_stack_large.obj", "garden", 8.832, 10.368, "s", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* stacked cut stone */
    {"urban", "m_cut_stone_stack", "kaykit-resource-bits/obj/stone_stack_large.obj", "concrete", 8.832, 10.368, "s", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* stacked cut stone */
    {"sundered", "m_cut_stone_stack", "kaykit-resource-bits/obj/stone_stack_large.obj", "rock", 8.832, 10.368, "s", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* stacked cut stone */
    {"fortress", "m_small_stone_stack", "kaykit-resource-bits/obj/stone_stack_small.obj", "garden", 5.52, 6.48, "s", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* small stone stack */
    {"sundered", "m_small_stone_stack", "kaykit-resource-bits/obj/stone_stack_small.obj", "rock", 5.52, 6.48, "s", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* small stone stack */
    {"fortress", "m_cut_block", "kaykit-resource-bits/obj/stone_block.obj", "garden", 4.14, 4.86, "s", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* single cut block */
    {"sundered", "m_cut_block", "kaykit-resource-bits/obj/stone_block.obj", "rock", 4.14, 4.86, "s", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* single cut block */
    {"dungeon", "m_cut_block", "kaykit-resource-bits/obj/stone_block.obj", "flag", 4.14, 4.86, "s", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* single cut block */
    {"urban", "m_rubble_pile", "kaykit-resource-bits/obj/rubble_large.obj", "concrete", 8.28, 9.72, "s", "debris", 0.0, 0.3, 0.22, 0.0, 0, 0, {}},          /* rubble pile */
    {"sundered", "m_rubble_pile", "kaykit-resource-bits/obj/rubble_large.obj", "rock", 8.28, 9.72, "s", "debris", 0.0, 0.3, 0.22, 0.0, 0, 0, {}},          /* rubble pile */
    {"dungeon", "m_rubble_pile", "kaykit-resource-bits/obj/rubble_large.obj", "flag", 8.28, 9.72, "s", "debris", 0.0, 0.3, 0.22, 0.0, 0, 0, {}},          /* rubble pile */
    {"temple", "m_rubble_pile", "kaykit-resource-bits/obj/rubble_large.obj", "tcolumn", 8.28, 9.72, "s", "debris", 0.0, 0.3, 0.22, 0.0, 0, 0, {}},          /* rubble pile */
    {"mine", "m_rubble_pile", "kaykit-resource-bits/obj/rubble_large.obj", "rock", 8.28, 9.72, "s", "debris", 0.0, 0.3, 0.22, 0.0, 0, 0, {}},          /* rubble pile */
    {"urban", "m_small_rubble", "kaykit-resource-bits/obj/rubble_small.obj", "concrete", 6.072, 7.128, "s", "debris", 0.0, 0.3, 0.22, 0.0, 0, 0, {}},          /* small rubble */
    {"sundered", "m_small_rubble", "kaykit-resource-bits/obj/rubble_small.obj", "rock", 6.072, 7.128, "s", "debris", 0.0, 0.3, 0.22, 0.0, 0, 0, {}},          /* small rubble */
    {"dungeon", "m_small_rubble", "kaykit-resource-bits/obj/rubble_small.obj", "flag", 6.072, 7.128, "s", "debris", 0.0, 0.3, 0.22, 0.0, 0, 0, {}},          /* small rubble */
    {"temple", "m_small_rubble", "kaykit-resource-bits/obj/rubble_small.obj", "tcolumn", 6.072, 7.128, "s", "debris", 0.0, 0.3, 0.22, 0.0, 0, 0, {}},          /* small rubble */
    {"forest", "m_cut_log", "kaykit-resource-bits/obj/log.obj", "log", 7.728, 9.072, "s", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* log */
    {"frontier", "m_cut_log", "kaykit-resource-bits/obj/log.obj", "wood", 7.728, 9.072, "s", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* log */
    {"highland", "m_cut_log", "kaykit-resource-bits/obj/log.obj", "wood", 7.728, 9.072, "s", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* log */
    {"forest", "m_log_pile", "kaykit-resource-bits/obj/log_stack.obj", "log", 9.384, 11.016, "s", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* log pile */
    {"frontier", "m_log_pile", "kaykit-resource-bits/obj/log_stack.obj", "wood", 9.384, 11.016, "s", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* log pile */
    {"highland", "m_log_pile", "kaykit-resource-bits/obj/log_stack.obj", "wood", 9.384, 11.016, "s", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* log pile */
    {"frontier", "m_plank_stack", "kaykit-resource-bits/obj/planks_medium.obj", "wood", 9.384, 11.016, "s", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* plank stack */
    {"mine", "m_plank_stack", "kaykit-resource-bits/obj/planks_medium.obj", "wood", 9.384, 11.016, "s", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* plank stack */
    {"frontier", "m_small_plank_stack", "kaykit-resource-bits/obj/planks_small.obj", "wood", 8.832, 10.368, "s", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* small plank stack */
    {"mine", "m_small_plank_stack", "kaykit-resource-bits/obj/planks_small.obj", "wood", 8.832, 10.368, "s", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* small plank stack */
    {"urban", "m_pallet", "kaykit-resource-bits/obj/pallet.obj", "wood", 6.624, 7.776, "s", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* pallet */
    {"ferrum", "m_pallet", "kaykit-resource-bits/obj/pallet.obj", "wood", 6.624, 7.776, "s", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* pallet */
    {"ferrum", "m_parts_heap", "kaykit-resource-bits/obj/parts_heap.obj", "iron", 9.384, 11.016, "s", "debris", 0.0, 0.3, 0.22, 0.0, 0, 0, {}},          /* scrap and machine-parts heap */
    {"targa", "m_parts_heap", "kaykit-resource-bits/obj/parts_heap.obj", "metal", 9.384, 11.016, "s", "debris", 0.0, 0.3, 0.22, 0.0, 0, 0, {}},          /* scrap and machine-parts heap */
    {"ashveil", "m_parts_heap", "kaykit-resource-bits/obj/parts_heap.obj", "iron", 9.384, 11.016, "s", "debris", 0.0, 0.3, 0.22, 0.0, 0, 0, {}},          /* scrap and machine-parts heap */
    {"urban", "m_parts_heap", "kaykit-resource-bits/obj/parts_heap.obj", "iron", 9.384, 11.016, "s", "debris", 0.0, 0.3, 0.22, 0.0, 0, 0, {}},          /* scrap and machine-parts heap */
    {"ferrum", "m_parts_crate", "kaykit-resource-bits/obj/parts_crate.obj", "iron", 8.28, 9.72, "s", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* crate of machine parts */
    {"corridor", "m_parts_crate", "kaykit-resource-bits/obj/parts_crate.obj", "metal", 8.28, 9.72, "s", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* crate of machine parts */
    {"ferrum", "m_gear_scatter", "kaykit-resource-bits/obj/parts_small.obj", "iron", 6.624, 7.776, "s", "debris", 0.0, 0.3, 0.22, 0.0, 0, 0, {}},          /* gears on the ground */
    {"targa", "m_gear_scatter", "kaykit-resource-bits/obj/parts_small.obj", "metal", 6.624, 7.776, "s", "debris", 0.0, 0.3, 0.22, 0.0, 0, 0, {}},          /* gears on the ground */
    {"corridor", "m_gear_scatter", "kaykit-resource-bits/obj/parts_small.obj", "metal", 6.624, 7.776, "s", "debris", 0.0, 0.3, 0.22, 0.0, 0, 0, {}},          /* gears on the ground */
    {"ferrum", "m_cog_wheel", "kaykit-resource-bits/obj/cog.obj", "iron", 2.208, 2.592, "bf", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* single cog, lying */
    {"corridor", "m_cog_wheel", "kaykit-resource-bits/obj/cog.obj", "metal", 2.208, 2.592, "bf", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* single cog, lying */
    {"mine", "m_smith_anvil", "kaykit-rpg-tools/obj/anvil.obj", "iron", 4.416, 5.184, "s", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* anvil */
    {"frontier", "m_smith_anvil", "kaykit-rpg-tools/obj/anvil.obj", "iron", 4.416, 5.184, "s", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* anvil */
    {"mine", "m_bucket", "kaykit-rpg-tools/obj/bucket.obj", "iron", 1.932, 2.268, "ssl", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* bucket with handle */
    {"frontier", "m_bucket", "kaykit-rpg-tools/obj/bucket.obj", "iron", 1.932, 2.268, "ssl", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* bucket with handle */
    {"coast", "m_bucket", "kaykit-rpg-tools/obj/bucket.obj", "iron", 1.932, 2.268, "ssl", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* bucket with handle */
    {"mine", "m_grindstone", "kaykit-rpg-tools/obj/grindstone.obj", "rock", 6.624, 7.776, "s", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* grindstone on a frame */
    {"frontier", "m_grindstone", "kaykit-rpg-tools/obj/grindstone.obj", "rock", 6.624, 7.776, "s", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* grindstone on a frame */
    {"mine", "m_lantern", "kaykit-rpg-tools/obj/lantern.obj", "iron", 1.932, 2.268, "s", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* lantern */
    {"dungeon", "m_lantern", "kaykit-rpg-tools/obj/lantern.obj", "iron", 1.932, 2.268, "s", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* lantern */
    {"frontier", "m_lantern", "kaykit-rpg-tools/obj/lantern.obj", "iron", 1.932, 2.268, "s", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* lantern */
    {"mine", "m_pick", "kaykit-rpg-tools/obj/pickaxe.obj", "iron", 5.52, 6.48, "bf", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* pickaxe, lying */
    {"mine", "m_rope_bundle", "kaykit-rpg-tools/obj/rope_bundle.obj", "rope", 5.52, 6.48, "s", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* rope bundle */
    {"coast", "m_rope_bundle", "kaykit-rpg-tools/obj/rope_bundle.obj", "rope", 5.52, 6.48, "s", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* rope bundle */
    {"mine", "m_coiled_rope", "kaykit-rpg-tools/obj/rope_coil.obj", "rope", 4.416, 5.184, "s", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* rope coil */
    {"coast", "m_coiled_rope", "kaykit-rpg-tools/obj/rope_coil.obj", "rope", 4.416, 5.184, "s", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* rope coil */
    {"mine", "m_spade", "kaykit-rpg-tools/obj/shovel.obj", "iron", 6.624, 7.776, "bf", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* shovel, lying */
    {"graveyard", "m_spade", "kaykit-rpg-tools/obj/shovel.obj", "iron", 6.624, 7.776, "bf", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* shovel, lying */
    {"trederra", "m_spade", "kaykit-rpg-tools/obj/shovel.obj", "iron", 6.624, 7.776, "bf", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* shovel, lying */
    {"dungeon", "m_closed_book", "kaykit-rpg-tools/obj/book.obj", "wood", 1.38, 1.62, "lr", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* closed book */
    {"dungeon", "m_rolled_map", "kaykit-rpg-tools/obj/map_rolled.obj", "wood", 2.76, 3.24, "s", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* rolled map */
    {"highland", "m_battle_axe", "kaykit-fantasy-weapons/obj/axe_double.obj", "brass", 5.52, 6.48, "bf", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* double axe, lying */
    {"dungeon", "m_battle_axe", "kaykit-fantasy-weapons/obj/axe_double.obj", "iron", 5.52, 6.48, "bf", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* double axe, lying */
    {"highland", "m_hand_axe", "kaykit-fantasy-weapons/obj/axe.obj", "brass", 5.52, 6.48, "bf", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* axe, lying */
    {"frontier", "m_hand_axe", "kaykit-fantasy-weapons/obj/axe.obj", "iron", 5.52, 6.48, "bf", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* axe, lying */
    {"dungeon", "m_spiked_mace", "kaykit-fantasy-weapons/obj/mace_spiked.obj", "iron", 4.968, 5.832, "lr", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* spiked mace, lying */
    {"badlands", "m_spiked_mace", "kaykit-fantasy-weapons/obj/mace_spiked.obj", "iron", 4.968, 5.832, "lr", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* spiked mace, lying */
    {"badlands", "m_spiked_club", "kaykit-fantasy-weapons/obj/club_spiked.obj", "wood", 4.968, 5.832, "bf", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* spiked club, lying */
    {"swamp", "m_spiked_club", "kaykit-fantasy-weapons/obj/club_spiked.obj", "log", 4.968, 5.832, "bf", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* spiked club, lying */
    {"blight", "m_spiked_club", "kaykit-fantasy-weapons/obj/club_spiked.obj", "wood", 4.968, 5.832, "bf", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* spiked club, lying */
    {"dungeon", "m_kite_shield", "kaykit-fantasy-weapons/obj/shield_kite.obj", "iron", 4.968, 5.832, "bf", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* kite shield, lying */
    {"highland", "m_kite_shield", "kaykit-fantasy-weapons/obj/shield_kite.obj", "brass", 4.968, 5.832, "bf", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* kite shield, lying */
    {"dungeon", "m_heater_shield", "kaykit-fantasy-weapons/obj/shield_heater.obj", "iron", 4.968, 5.832, "bf", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* heater shield, lying */
    {"graveyard", "m_heater_shield", "kaykit-fantasy-weapons/obj/shield_heater.obj", "iron", 4.968, 5.832, "bf", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* heater shield, lying */
    {"badlands", "m_spiked_shield", "kaykit-fantasy-weapons/obj/shield_tower_spiked.obj", "iron", 5.52, 6.48, "bf", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* spiked tower shield, lying */
    {"blight", "m_spiked_shield", "kaykit-fantasy-weapons/obj/shield_tower_spiked.obj", "iron", 5.52, 6.48, "bf", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* spiked tower shield, lying */
    {"dungeon", "m_spiked_shield", "kaykit-fantasy-weapons/obj/shield_tower_spiked.obj", "iron", 5.52, 6.48, "bf", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* spiked tower shield, lying */
    {"graveyard", "m_broadsword", "kaykit-fantasy-weapons/obj/sword_broad.obj", "iron", 5.52, 6.48, "bf", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* broad sword, lying */
    {"highland", "m_broadsword", "kaykit-fantasy-weapons/obj/sword_broad.obj", "brass", 5.52, 6.48, "bf", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* broad sword, lying */
    {"graveyard", "m_longsword", "kaykit-fantasy-weapons/obj/sword.obj", "iron", 5.52, 6.48, "bf", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* sword, lying */
    {"dungeon", "m_longsword", "kaykit-fantasy-weapons/obj/sword.obj", "iron", 5.52, 6.48, "bf", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* sword, lying */
    {"urban", "m_steel_drum", "industrial-cc0/obj/steel_drum.obj", "iron", 4.968, 5.832, "ssl", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* steel drum */
    {"ferrum", "m_steel_drum", "industrial-cc0/obj/steel_drum.obj", "iron", 4.968, 5.832, "ssl", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* steel drum */
    {"trederra", "m_steel_drum", "industrial-cc0/obj/steel_drum.obj", "iron", 4.968, 5.832, "ssl", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* steel drum */
    {"targa", "m_steel_drum", "industrial-cc0/obj/steel_drum.obj", "metal", 4.968, 5.832, "ssl", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* steel drum */
    {"urban", "m_plastic_drum", "industrial-cc0/obj/plastic_drum.obj", "iron", 4.968, 5.832, "ssl", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* plastic drum */
    {"ashveil", "m_plastic_drum", "industrial-cc0/obj/plastic_drum.obj", "iron", 4.968, 5.832, "ssl", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* plastic drum */
    {"ferrum", "m_gas_cylinder", "industrial-cc0/obj/gas_cylinder.obj", "iron", 3.864, 4.536, "ssl", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* gas bottle */
    {"corridor", "m_gas_cylinder_b", "industrial-cc0/obj/gas_cylinder.obj", "metal", 3.864, 4.536, "ssl", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* gas bottle */
    {"urban", "m_gas_cylinder", "industrial-cc0/obj/gas_cylinder.obj", "iron", 3.864, 4.536, "ssl", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* gas bottle */
    {"urban", "m_petrol_can", "industrial-cc0/obj/gas_canister.obj", "iron", 2.484, 2.916, "ssl", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* jerrycan */
    {"trederra", "m_petrol_can", "industrial-cc0/obj/gas_canister.obj", "iron", 2.484, 2.916, "ssl", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* jerrycan */
    {"ferrum", "m_power_cabinet", "industrial-cc0/obj/power_cabinet.obj", "iron", 6.624, 7.776, "s", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* power cabinet with conduits */
    {"corridor", "m_power_cabinet", "industrial-cc0/obj/power_cabinet.obj", "metal", 6.624, 7.776, "s", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* power cabinet with conduits */
    {"urban", "m_power_cabinet", "industrial-cc0/obj/power_cabinet.obj", "iron", 6.624, 7.776, "s", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* power cabinet with conduits */
    {"ferrum", "m_fuse_cabinet", "industrial-cc0/obj/electrical_box.obj", "iron", 5.52, 6.48, "s", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* electrical cabinet */
    {"corridor", "m_fuse_cabinet", "industrial-cc0/obj/electrical_box.obj", "metal", 5.52, 6.48, "s", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* electrical cabinet */
    {"ferrum", "m_cable_reel", "industrial-cc0/obj/cable_drum.obj", "wood", 5.52, 6.48, "s", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* cable drum */
    {"urban", "m_cable_reel", "industrial-cc0/obj/cable_drum.obj", "wood", 5.52, 6.48, "s", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* cable drum */
    {"ferrum", "m_generator", "industrial-cc0/obj/generator.obj", "iron", 4.968, 5.832, "s", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* framed generator; reduced 4627 -> 3398 tris */
    {"urban", "m_generator", "industrial-cc0/obj/generator.obj", "iron", 4.968, 5.832, "s", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* framed generator; reduced 4627 -> 3398 tris */
    {"targa", "m_generator", "industrial-cc0/obj/generator.obj", "metal", 4.968, 5.832, "s", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* framed generator; reduced 4627 -> 3398 tris */
    {"corridor", "m_locker", "industrial-cc0/obj/locker.obj", "metal", 9.936, 11.664, "s", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* locker, door ajar */
    {"urban", "m_locker", "industrial-cc0/obj/locker.obj", "iron", 9.936, 11.664, "s", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* locker, door ajar */
    {"corridor", "m_extinguisher", "industrial-cc0/obj/fire_extinguisher.obj", "metal", 3.312, 3.888, "ssl", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* fire extinguisher */
    {"graveyard", "m_dead_tree_small", "dead-forest/obj/dead_tree_small.obj", "wood", 13.8, 16.2, "s", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* small dead tree */
    {"swamp", "m_dead_tree_small", "dead-forest/obj/dead_tree_small.obj", "log", 13.8, 16.2, "s", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* small dead tree */
    {"blight", "m_dead_tree_small", "dead-forest/obj/dead_tree_small.obj", "wood", 13.8, 16.2, "s", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* small dead tree */
    {"ashveil", "m_dead_tree_small", "dead-forest/obj/dead_tree_small.obj", "wood", 13.8, 16.2, "s", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* small dead tree */
    {"badlands", "m_dead_tree_small", "dead-forest/obj/dead_tree_small.obj", "wood", 13.8, 16.2, "s", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* small dead tree */
    {"graveyard", "m_dead_tree_forked", "dead-forest/obj/dead_tree_forked.obj", "wood", 16.56, 19.44, "s", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* forked dead tree */
    {"swamp", "m_dead_tree_forked", "dead-forest/obj/dead_tree_forked.obj", "log", 16.56, 19.44, "s", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* forked dead tree */
    {"blight", "m_dead_tree_forked", "dead-forest/obj/dead_tree_forked.obj", "wood", 16.56, 19.44, "s", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* forked dead tree */
    {"forest", "m_dead_tree_forked", "dead-forest/obj/dead_tree_forked.obj", "log", 16.56, 19.44, "s", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* forked dead tree */
    {"graveyard", "m_dead_tree_wide", "dead-forest/obj/dead_tree_wide.obj", "wood", 16.56, 19.44, "s", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* dead tree with a wide crown */
    {"swamp", "m_dead_tree_wide", "dead-forest/obj/dead_tree_wide.obj", "log", 16.56, 19.44, "s", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* dead tree with a wide crown */
    {"ashveil", "m_dead_tree_wide", "dead-forest/obj/dead_tree_wide.obj", "wood", 16.56, 19.44, "s", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* dead tree with a wide crown */
    {"highland", "m_boulder_group", "dead-forest/obj/boulder_group.obj", "slate", 9.936, 11.664, "s", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* cracked boulder group; reduced 5642 -> 3293 tris */
    {"forest", "m_boulder_group", "dead-forest/obj/boulder_group.obj", "rock", 9.936, 11.664, "s", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* cracked boulder group; reduced 5642 -> 3293 tris */
    {"coast", "m_boulder_group", "dead-forest/obj/boulder_group.obj", "sandstone", 9.936, 11.664, "s", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* cracked boulder group; reduced 5642 -> 3293 tris */
    {"badlands", "m_boulder_group", "dead-forest/obj/boulder_group.obj", "rock", 9.936, 11.664, "s", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* cracked boulder group; reduced 5642 -> 3293 tris */
    {"mine", "m_boulder_group", "dead-forest/obj/boulder_group.obj", "rock", 9.936, 11.664, "s", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* cracked boulder group; reduced 5642 -> 3293 tris */
    {"swamp", "m_boulder_group", "dead-forest/obj/boulder_group.obj", "rock", 9.936, 11.664, "s", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* cracked boulder group; reduced 5642 -> 3293 tris */
    {"cynder", "m_boulder_group", "dead-forest/obj/boulder_group.obj", "basalt", 9.936, 11.664, "s", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* cracked boulder group; reduced 5642 -> 3293 tris */
    {"blight", "m_boulder_group", "dead-forest/obj/boulder_group.obj", "rock", 9.936, 11.664, "s", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* cracked boulder group; reduced 5642 -> 3293 tris */
    {"highland", "m_long_flat_rock", "dead-forest/obj/rock_long_flat.obj", "slate", 5.52, 6.48, "s", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* long flat rock */
    {"coast", "m_long_flat_rock", "dead-forest/obj/rock_long_flat.obj", "sandstone", 5.52, 6.48, "s", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* long flat rock */
    {"badlands", "m_long_flat_rock", "dead-forest/obj/rock_long_flat.obj", "rock", 5.52, 6.48, "s", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* long flat rock */
    {"forest", "m_boulder_b", "dead-forest/obj/rock_boulder.obj", "rock", 4.416, 5.184, "s", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* boulder */
    {"highland", "m_boulder_b", "dead-forest/obj/rock_boulder.obj", "slate", 4.416, 5.184, "s", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* boulder */
    {"mine", "m_boulder_b", "dead-forest/obj/rock_boulder.obj", "rock", 4.416, 5.184, "s", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* boulder */
    {"swamp", "m_boulder_b", "dead-forest/obj/rock_boulder.obj", "rock", 4.416, 5.184, "s", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* boulder */
    {"coast", "m_boulder_b", "dead-forest/obj/rock_boulder.obj", "sandstone", 4.416, 5.184, "s", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* boulder */
    {"forest", "m_round_rock", "dead-forest/obj/rock_round.obj", "rock", 2.208, 2.592, "s", "debris", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* round rock */
    {"highland", "m_round_rock", "dead-forest/obj/rock_round.obj", "slate", 2.208, 2.592, "s", "debris", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* round rock */
    {"mine", "m_round_rock", "dead-forest/obj/rock_round.obj", "rock", 2.208, 2.592, "s", "debris", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* round rock */
    {"swamp", "m_round_rock", "dead-forest/obj/rock_round.obj", "rock", 2.208, 2.592, "s", "debris", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* round rock */
    {"coast", "m_round_rock", "dead-forest/obj/rock_round.obj", "sandstone", 2.208, 2.592, "s", "debris", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* round rock */
    {"badlands", "m_round_rock", "dead-forest/obj/rock_round.obj", "rock", 2.208, 2.592, "s", "debris", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* round rock */
    {"forest", "m_low_rock", "dead-forest/obj/rock_low_long.obj", "rock", 4.968, 5.832, "s", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* low long rock */
    {"highland", "m_low_rock", "dead-forest/obj/rock_low_long.obj", "slate", 4.968, 5.832, "s", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* low long rock */
    {"coast", "m_low_rock", "dead-forest/obj/rock_low_long.obj", "sandstone", 4.968, 5.832, "s", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* low long rock */
    {"badlands", "m_low_rock", "dead-forest/obj/rock_low_long.obj", "rock", 4.968, 5.832, "s", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* low long rock */
    {"mine", "m_low_rock", "dead-forest/obj/rock_low_long.obj", "rock", 4.968, 5.832, "s", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* low long rock */
    {"forest", "m_slab_rock", "dead-forest/obj/rock_slab.obj", "rock", 4.416, 5.184, "s", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* slab rock */
    {"highland", "m_slab_rock", "dead-forest/obj/rock_slab.obj", "slate", 4.416, 5.184, "s", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* slab rock */
    {"coast", "m_slab_rock", "dead-forest/obj/rock_slab.obj", "sandstone", 4.416, 5.184, "s", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* slab rock */
    {"badlands", "m_slab_rock", "dead-forest/obj/rock_slab.obj", "rock", 4.416, 5.184, "s", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* slab rock */
    {"mine", "m_slab_rock", "dead-forest/obj/rock_slab.obj", "rock", 4.416, 5.184, "s", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* slab rock */
    {"dungeon", "m_oak_barrel", "lowpoly-dungeon/obj/barrel.obj", "wood", 5.134, 6.026, "ssl", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* barrel */
    {"mine", "m_oak_barrel", "lowpoly-dungeon/obj/barrel.obj", "wood", 5.134, 6.026, "ssl", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* barrel */
    {"coast", "m_oak_barrel", "lowpoly-dungeon/obj/barrel.obj", "drift", 5.134, 6.026, "ssl", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* barrel */
    {"dungeon", "m_treasure_chest", "lowpoly-dungeon/obj/chest.obj", "wood", 4.968, 5.832, "s", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* treasure chest */
    {"coast", "m_treasure_chest", "lowpoly-dungeon/obj/chest.obj", "drift", 4.968, 5.832, "s", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* treasure chest */
    {"dungeon", "m_plank_crate", "lowpoly-dungeon/obj/crate_closed.obj", "wood", 4.416, 5.184, "s", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* crate */
    {"mine", "m_plank_crate", "lowpoly-dungeon/obj/crate_closed.obj", "wood", 4.416, 5.184, "s", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* crate */
    {"frontier", "m_plank_crate", "lowpoly-dungeon/obj/crate_closed.obj", "wood", 4.416, 5.184, "s", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* crate */
    {"dungeon", "m_open_crate", "lowpoly-dungeon/obj/crate_open.obj", "wood", 4.416, 5.184, "s", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* open crate */
    {"mine", "m_open_crate", "lowpoly-dungeon/obj/crate_open.obj", "wood", 4.416, 5.184, "s", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* open crate */
    {"dungeon", "m_broken_flags_01", "lowpoly-dungeon/obj/flag_debris_01.obj", "flag", 6.624, 7.776, "s", "debris", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* broken flagstone scatter */
    {"temple", "m_broken_flags_01", "lowpoly-dungeon/obj/flag_debris_01.obj", "tcolumn", 6.624, 7.776, "s", "debris", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* broken flagstone scatter */
    {"sundered", "m_broken_flags_01", "lowpoly-dungeon/obj/flag_debris_01.obj", "rock", 6.624, 7.776, "s", "debris", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* broken flagstone scatter */
    {"dungeon", "m_broken_flags_02", "lowpoly-dungeon/obj/flag_debris_02.obj", "flag", 6.624, 7.776, "s", "debris", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* broken flagstone scatter */
    {"temple", "m_broken_flags_02", "lowpoly-dungeon/obj/flag_debris_02.obj", "tcolumn", 6.624, 7.776, "s", "debris", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* broken flagstone scatter */
    {"sundered", "m_broken_flags_02", "lowpoly-dungeon/obj/flag_debris_02.obj", "rock", 6.624, 7.776, "s", "debris", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* broken flagstone scatter */
    {"dungeon", "m_broken_flags_03", "lowpoly-dungeon/obj/flag_debris_03.obj", "flag", 6.624, 7.776, "s", "debris", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* broken flagstone scatter */
    {"temple", "m_broken_flags_03", "lowpoly-dungeon/obj/flag_debris_03.obj", "tcolumn", 6.624, 7.776, "s", "debris", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* broken flagstone scatter */
    {"sundered", "m_broken_flags_03", "lowpoly-dungeon/obj/flag_debris_03.obj", "rock", 6.624, 7.776, "s", "debris", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* broken flagstone scatter */
    {"dungeon", "m_broken_flags_04", "lowpoly-dungeon/obj/flag_debris_04.obj", "flag", 6.624, 7.776, "s", "debris", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* broken flagstone scatter */
    {"temple", "m_broken_flags_04", "lowpoly-dungeon/obj/flag_debris_04.obj", "tcolumn", 6.624, 7.776, "s", "debris", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* broken flagstone scatter */
    {"sundered", "m_broken_flags_04", "lowpoly-dungeon/obj/flag_debris_04.obj", "rock", 6.624, 7.776, "s", "debris", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* broken flagstone scatter */
    {"dungeon", "m_broken_flags_05", "lowpoly-dungeon/obj/flag_debris_05.obj", "flag", 6.624, 7.776, "s", "debris", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* broken flagstone scatter */
    {"temple", "m_broken_flags_05", "lowpoly-dungeon/obj/flag_debris_05.obj", "tcolumn", 6.624, 7.776, "s", "debris", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* broken flagstone scatter */
    {"sundered", "m_broken_flags_05", "lowpoly-dungeon/obj/flag_debris_05.obj", "rock", 6.624, 7.776, "s", "debris", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* broken flagstone scatter */
    {"dungeon", "m_broken_flags_06", "lowpoly-dungeon/obj/flag_debris_06.obj", "flag", 3.312, 3.888, "s", "debris", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* broken flagstone scatter */
    {"temple", "m_broken_flags_06", "lowpoly-dungeon/obj/flag_debris_06.obj", "tcolumn", 3.312, 3.888, "s", "debris", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* broken flagstone scatter */
    {"sundered", "m_broken_flags_06", "lowpoly-dungeon/obj/flag_debris_06.obj", "rock", 3.312, 3.888, "s", "debris", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* broken flagstone scatter */
    {"dungeon", "m_broken_flags_07", "lowpoly-dungeon/obj/flag_debris_07.obj", "flag", 6.624, 7.776, "s", "debris", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* broken flagstone scatter */
    {"temple", "m_broken_flags_07", "lowpoly-dungeon/obj/flag_debris_07.obj", "tcolumn", 6.624, 7.776, "s", "debris", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* broken flagstone scatter */
    {"sundered", "m_broken_flags_07", "lowpoly-dungeon/obj/flag_debris_07.obj", "rock", 6.624, 7.776, "s", "debris", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* broken flagstone scatter */
    {"dungeon", "m_broken_flags_08", "lowpoly-dungeon/obj/flag_debris_08.obj", "flag", 6.624, 7.776, "s", "debris", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* broken flagstone scatter */
    {"temple", "m_broken_flags_08", "lowpoly-dungeon/obj/flag_debris_08.obj", "tcolumn", 6.624, 7.776, "s", "debris", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* broken flagstone scatter */
    {"sundered", "m_broken_flags_08", "lowpoly-dungeon/obj/flag_debris_08.obj", "rock", 6.624, 7.776, "s", "debris", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* broken flagstone scatter */
    {"dungeon", "m_broken_flags_09", "lowpoly-dungeon/obj/flag_debris_09.obj", "flag", 6.624, 7.776, "s", "debris", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* broken flagstone scatter */
    {"temple", "m_broken_flags_09", "lowpoly-dungeon/obj/flag_debris_09.obj", "tcolumn", 6.624, 7.776, "s", "debris", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* broken flagstone scatter */
    {"sundered", "m_broken_flags_09", "lowpoly-dungeon/obj/flag_debris_09.obj", "rock", 6.624, 7.776, "s", "debris", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* broken flagstone scatter */
    {"dungeon", "m_broken_flags_10", "lowpoly-dungeon/obj/flag_debris_10.obj", "flag", 6.624, 7.776, "s", "debris", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* broken flagstone scatter */
    {"temple", "m_broken_flags_10", "lowpoly-dungeon/obj/flag_debris_10.obj", "tcolumn", 6.624, 7.776, "s", "debris", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* broken flagstone scatter */
    {"sundered", "m_broken_flags_10", "lowpoly-dungeon/obj/flag_debris_10.obj", "rock", 6.624, 7.776, "s", "debris", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* broken flagstone scatter */
    {"dungeon", "m_broken_flags_11", "lowpoly-dungeon/obj/flag_debris_11.obj", "flag", 6.624, 7.776, "s", "debris", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* broken flagstone scatter */
    {"temple", "m_broken_flags_11", "lowpoly-dungeon/obj/flag_debris_11.obj", "tcolumn", 6.624, 7.776, "s", "debris", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* broken flagstone scatter */
    {"sundered", "m_broken_flags_11", "lowpoly-dungeon/obj/flag_debris_11.obj", "rock", 6.624, 7.776, "s", "debris", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* broken flagstone scatter */
    {"dungeon", "m_broken_flags_12", "lowpoly-dungeon/obj/flag_debris_12.obj", "flag", 6.624, 7.776, "s", "debris", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* broken flagstone scatter */
    {"temple", "m_broken_flags_12", "lowpoly-dungeon/obj/flag_debris_12.obj", "tcolumn", 6.624, 7.776, "s", "debris", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* broken flagstone scatter */
    {"sundered", "m_broken_flags_12", "lowpoly-dungeon/obj/flag_debris_12.obj", "rock", 6.624, 7.776, "s", "debris", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* broken flagstone scatter */
    {"dungeon", "m_broken_flags_13", "lowpoly-dungeon/obj/flag_debris_13.obj", "flag", 6.624, 7.776, "s", "debris", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* broken flagstone scatter */
    {"temple", "m_broken_flags_13", "lowpoly-dungeon/obj/flag_debris_13.obj", "tcolumn", 6.624, 7.776, "s", "debris", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* broken flagstone scatter */
    {"sundered", "m_broken_flags_13", "lowpoly-dungeon/obj/flag_debris_13.obj", "rock", 6.624, 7.776, "s", "debris", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* broken flagstone scatter */
    {"dungeon", "m_loose_planks_01", "lowpoly-dungeon/obj/planks_01.obj", "wood", 13.69, 16.07, "s", "debris", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* loose planks */
    {"urban", "m_loose_planks_01", "lowpoly-dungeon/obj/planks_01.obj", "wood", 13.69, 16.07, "s", "debris", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* loose planks */
    {"mine", "m_loose_planks_01", "lowpoly-dungeon/obj/planks_01.obj", "wood", 13.69, 16.07, "s", "debris", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* loose planks */
    {"dungeon", "m_loose_planks_02", "lowpoly-dungeon/obj/planks_02.obj", "wood", 9.66, 11.34, "s", "debris", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* loose planks */
    {"urban", "m_loose_planks_02", "lowpoly-dungeon/obj/planks_02.obj", "wood", 9.66, 11.34, "s", "debris", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* loose planks */
    {"mine", "m_loose_planks_02", "lowpoly-dungeon/obj/planks_02.obj", "wood", 9.66, 11.34, "s", "debris", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* loose planks */
    {"dungeon", "m_loose_planks_03", "lowpoly-dungeon/obj/planks_03.obj", "wood", 6.569, 7.711, "s", "debris", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* loose planks */
    {"urban", "m_loose_planks_03", "lowpoly-dungeon/obj/planks_03.obj", "wood", 6.569, 7.711, "s", "debris", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* loose planks */
    {"mine", "m_loose_planks_03", "lowpoly-dungeon/obj/planks_03.obj", "wood", 6.569, 7.711, "s", "debris", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* loose planks */
    {"dungeon", "m_loose_planks_04", "lowpoly-dungeon/obj/planks_04.obj", "wood", 5.52, 6.48, "s", "debris", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* loose planks */
    {"urban", "m_loose_planks_04", "lowpoly-dungeon/obj/planks_04.obj", "wood", 5.52, 6.48, "s", "debris", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* loose planks */
    {"mine", "m_loose_planks_04", "lowpoly-dungeon/obj/planks_04.obj", "wood", 5.52, 6.48, "s", "debris", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* loose planks */
    {"dungeon", "m_loose_planks_05", "lowpoly-dungeon/obj/planks_05.obj", "wood", 8.556, 10.044, "s", "debris", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* loose planks */
    {"urban", "m_loose_planks_05", "lowpoly-dungeon/obj/planks_05.obj", "wood", 8.556, 10.044, "s", "debris", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* loose planks */
    {"mine", "m_loose_planks_05", "lowpoly-dungeon/obj/planks_05.obj", "wood", 8.556, 10.044, "s", "debris", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* loose planks */
    {"temple", "m_candle_dish", "lowpoly-dungeon/obj/candles.obj", "gold", 1.932, 2.268, "s", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* candle cluster on a dish */
    {"dungeon", "m_candle_dish", "lowpoly-dungeon/obj/candles.obj", "gold", 1.932, 2.268, "s", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* candle cluster on a dish */
    {"graveyard", "m_candle_dish", "lowpoly-dungeon/obj/candles.obj", "wax", 1.932, 2.268, "s", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* candle cluster on a dish */
    {"temple", "m_stone_pillar", "lowpoly-dungeon/obj/pillar.obj", "tcolumn", 13.248, 15.552, "s", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* stone pillar */
    {"dungeon", "m_stone_pillar", "lowpoly-dungeon/obj/pillar.obj", "flag", 13.248, 15.552, "s", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* stone pillar */
    {"badlands", "m_rock_arch", "rocks-misc/obj/rockform_arch.obj", "rock", 16.56, 19.44, "s", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* natural rock arch, large bases */
    {"maahl", "m_rock_arch", "rocks-misc/obj/rockform_arch.obj", "sandstone", 16.56, 19.44, "s", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* natural rock arch, large bases */
    {"coast", "m_rock_arch", "rocks-misc/obj/rockform_arch.obj", "sandstone", 16.56, 19.44, "s", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* natural rock arch, large bases */
    {"badlands", "m_rock_column", "rocks-misc/obj/rockform_column.obj", "rock", 13.8, 16.2, "s", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* stacked rock column */
    {"maahl", "m_rock_column", "rocks-misc/obj/rockform_column.obj", "sandstone", 13.8, 16.2, "s", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* stacked rock column */
    {"highland", "m_rock_column", "rocks-misc/obj/rockform_column.obj", "slate", 13.8, 16.2, "s", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* stacked rock column */
    {"badlands", "m_rock_wall", "rocks-misc/obj/rockform_wall_a.obj", "rock", 13.8, 16.2, "s", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* short rock wall */
    {"highland", "m_rock_wall", "rocks-misc/obj/rockform_wall_a.obj", "slate", 13.8, 16.2, "s", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* short rock wall */
    {"coast", "m_rock_wall", "rocks-misc/obj/rockform_wall_a.obj", "sandstone", 13.8, 16.2, "s", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* short rock wall */
    {"mine", "m_rock_wall", "rocks-misc/obj/rockform_wall_a.obj", "rock", 13.8, 16.2, "s", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* short rock wall */
    {"badlands", "m_rock_wall_b", "rocks-misc/obj/rockform_wall_b.obj", "rock", 13.8, 16.2, "s", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* short rock wall */
    {"highland", "m_rock_wall_b", "rocks-misc/obj/rockform_wall_b.obj", "slate", 13.8, 16.2, "s", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* short rock wall */
    {"coast", "m_rock_wall_b", "rocks-misc/obj/rockform_wall_b.obj", "sandstone", 13.8, 16.2, "s", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* short rock wall */
    {"mine", "m_rock_wall_b", "rocks-misc/obj/rockform_wall_b.obj", "rock", 13.8, 16.2, "s", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* short rock wall */
    {"badlands", "m_rock_corner", "rocks-misc/obj/rockform_corner.obj", "rock", 16.56, 19.44, "s", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* rock wall corner, large bases */
    {"highland", "m_rock_corner", "rocks-misc/obj/rockform_corner.obj", "slate", 16.56, 19.44, "s", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* rock wall corner, large bases */
    {"mine", "m_rock_corner", "rocks-misc/obj/rockform_corner.obj", "rock", 16.56, 19.44, "s", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* rock wall corner, large bases */
    {"badlands", "m_rock_01", "rocks-misc/obj/rock_01.obj", "rock", 8.832, 10.368, "s", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* solid boulder */
    {"highland", "m_rock_01", "rocks-misc/obj/rock_01.obj", "slate", 8.832, 10.368, "s", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* solid boulder */
    {"coast", "m_rock_01", "rocks-misc/obj/rock_01.obj", "sandstone", 8.832, 10.368, "s", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* solid boulder */
    {"mine", "m_rock_01", "rocks-misc/obj/rock_01.obj", "rock", 8.832, 10.368, "s", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* solid boulder */
    {"forest", "m_rock_01", "rocks-misc/obj/rock_01.obj", "rock", 8.832, 10.368, "s", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* solid boulder */
    {"cynder", "m_rock_01", "rocks-misc/obj/rock_01.obj", "basalt", 8.832, 10.368, "s", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* solid boulder */
    {"badlands", "m_rock_02", "rocks-misc/obj/rock_02.obj", "rock", 8.832, 10.368, "s", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* solid boulder */
    {"highland", "m_rock_02", "rocks-misc/obj/rock_02.obj", "slate", 8.832, 10.368, "s", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* solid boulder */
    {"coast", "m_rock_02", "rocks-misc/obj/rock_02.obj", "sandstone", 8.832, 10.368, "s", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* solid boulder */
    {"mine", "m_rock_02", "rocks-misc/obj/rock_02.obj", "rock", 8.832, 10.368, "s", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* solid boulder */
    {"forest", "m_rock_02", "rocks-misc/obj/rock_02.obj", "rock", 8.832, 10.368, "s", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* solid boulder */
    {"cynder", "m_rock_02", "rocks-misc/obj/rock_02.obj", "basalt", 8.832, 10.368, "s", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* solid boulder */
    {"badlands", "m_rock_03", "rocks-misc/obj/rock_03.obj", "rock", 11.04, 12.96, "s", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* tall rock */
    {"highland", "m_rock_03", "rocks-misc/obj/rock_03.obj", "slate", 11.04, 12.96, "s", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* tall rock */
    {"maahl", "m_rock_03", "rocks-misc/obj/rock_03.obj", "sandstone", 11.04, 12.96, "s", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* tall rock */
    {"coast", "m_rock_03", "rocks-misc/obj/rock_03.obj", "sandstone", 11.04, 12.96, "s", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* tall rock */
    {"badlands", "m_rock_04", "rocks-misc/obj/rock_04.obj", "rock", 9.936, 11.664, "s", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* tall rock */
    {"highland", "m_rock_04", "rocks-misc/obj/rock_04.obj", "slate", 9.936, 11.664, "s", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* tall rock */
    {"coast", "m_rock_04", "rocks-misc/obj/rock_04.obj", "sandstone", 9.936, 11.664, "s", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* tall rock */
    {"mine", "m_rock_04", "rocks-misc/obj/rock_04.obj", "rock", 9.936, 11.664, "s", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* tall rock */
    {"forest", "m_rock_04", "rocks-misc/obj/rock_04.obj", "rock", 9.936, 11.664, "s", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* tall rock */
    {"cynder", "m_rock_04", "rocks-misc/obj/rock_04.obj", "basalt", 9.936, 11.664, "s", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* tall rock */
    {"badlands", "m_rock_05", "rocks-misc/obj/rock_05.obj", "rock", 9.384, 11.016, "s", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* solid boulder */
    {"highland", "m_rock_05", "rocks-misc/obj/rock_05.obj", "slate", 9.384, 11.016, "s", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* solid boulder */
    {"coast", "m_rock_05", "rocks-misc/obj/rock_05.obj", "sandstone", 9.384, 11.016, "s", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* solid boulder */
    {"mine", "m_rock_05", "rocks-misc/obj/rock_05.obj", "rock", 9.384, 11.016, "s", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* solid boulder */
    {"forest", "m_rock_05", "rocks-misc/obj/rock_05.obj", "rock", 9.384, 11.016, "s", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* solid boulder */
    {"cynder", "m_rock_05", "rocks-misc/obj/rock_05.obj", "basalt", 9.384, 11.016, "s", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* solid boulder */
    {"badlands", "m_rock_08", "rocks-misc/obj/rock_08.obj", "rock", 8.28, 9.72, "s", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* solid boulder */
    {"highland", "m_rock_08", "rocks-misc/obj/rock_08.obj", "slate", 8.28, 9.72, "s", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* solid boulder */
    {"coast", "m_rock_08", "rocks-misc/obj/rock_08.obj", "sandstone", 8.28, 9.72, "s", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* solid boulder */
    {"mine", "m_rock_08", "rocks-misc/obj/rock_08.obj", "rock", 8.28, 9.72, "s", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* solid boulder */
    {"forest", "m_rock_08", "rocks-misc/obj/rock_08.obj", "rock", 8.28, 9.72, "s", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* solid boulder */
    {"cynder", "m_rock_08", "rocks-misc/obj/rock_08.obj", "basalt", 8.28, 9.72, "s", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* solid boulder */
    {"badlands", "m_rock_09", "rocks-misc/obj/rock_09.obj", "rock", 7.728, 9.072, "s", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* solid boulder */
    {"highland", "m_rock_09", "rocks-misc/obj/rock_09.obj", "slate", 7.728, 9.072, "s", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* solid boulder */
    {"coast", "m_rock_09", "rocks-misc/obj/rock_09.obj", "sandstone", 7.728, 9.072, "s", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* solid boulder */
    {"mine", "m_rock_09", "rocks-misc/obj/rock_09.obj", "rock", 7.728, 9.072, "s", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* solid boulder */
    {"forest", "m_rock_09", "rocks-misc/obj/rock_09.obj", "rock", 7.728, 9.072, "s", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* solid boulder */
    {"cynder", "m_rock_09", "rocks-misc/obj/rock_09.obj", "basalt", 7.728, 9.072, "s", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* solid boulder */
    {"badlands", "m_scree", "rocks-misc/obj/scree_a.obj", "rock", 13.8, 16.2, "s", "debris", 0.0, 0.3, 0.22, 0.0, 0, 0, {}},          /* scree scatter */
    {"highland", "m_scree", "rocks-misc/obj/scree_a.obj", "slate", 13.8, 16.2, "s", "debris", 0.0, 0.3, 0.22, 0.0, 0, 0, {}},          /* scree scatter */
    {"mine", "m_scree", "rocks-misc/obj/scree_a.obj", "rock", 13.8, 16.2, "s", "debris", 0.0, 0.3, 0.22, 0.0, 0, 0, {}},          /* scree scatter */
    {"cynder", "m_scree", "rocks-misc/obj/scree_a.obj", "basalt", 13.8, 16.2, "s", "debris", 0.0, 0.3, 0.22, 0.0, 0, 0, {}},          /* scree scatter */
    {"badlands", "m_scree_b", "rocks-misc/obj/scree_b.obj", "rock", 13.8, 16.2, "s", "debris", 0.0, 0.3, 0.22, 0.0, 0, 0, {}},          /* scree scatter */
    {"highland", "m_scree_b", "rocks-misc/obj/scree_b.obj", "slate", 13.8, 16.2, "s", "debris", 0.0, 0.3, 0.22, 0.0, 0, 0, {}},          /* scree scatter */
    {"mine", "m_scree_b", "rocks-misc/obj/scree_b.obj", "rock", 13.8, 16.2, "s", "debris", 0.0, 0.3, 0.22, 0.0, 0, 0, {}},          /* scree scatter */
    {"cynder", "m_scree_b", "rocks-misc/obj/scree_b.obj", "basalt", 13.8, 16.2, "s", "debris", 0.0, 0.3, 0.22, 0.0, 0, 0, {}},          /* scree scatter */
    {"badlands", "m_pebble_group", "rocks-misc/obj/pebbles_a.obj", "rock", 8.28, 9.72, "s", "debris", 0.0, 0.3, 0.22, 0.0, 0, 0, {}},          /* pebble group */
    {"highland", "m_pebble_group", "rocks-misc/obj/pebbles_a.obj", "slate", 8.28, 9.72, "s", "debris", 0.0, 0.3, 0.22, 0.0, 0, 0, {}},          /* pebble group */
    {"coast", "m_pebble_group", "rocks-misc/obj/pebbles_a.obj", "sandstone", 8.28, 9.72, "s", "debris", 0.0, 0.3, 0.22, 0.0, 0, 0, {}},          /* pebble group */
    {"mine", "m_pebble_group", "rocks-misc/obj/pebbles_a.obj", "rock", 8.28, 9.72, "s", "debris", 0.0, 0.3, 0.22, 0.0, 0, 0, {}},          /* pebble group */
    {"maahl", "m_pebble_group", "rocks-misc/obj/pebbles_a.obj", "sandstone", 8.28, 9.72, "s", "debris", 0.0, 0.3, 0.22, 0.0, 0, 0, {}},          /* pebble group */
    {"badlands", "m_pebble_group_b", "rocks-misc/obj/pebbles_b.obj", "rock", 8.28, 9.72, "s", "debris", 0.0, 0.3, 0.22, 0.0, 0, 0, {}},          /* pebble group */
    {"highland", "m_pebble_group_b", "rocks-misc/obj/pebbles_b.obj", "slate", 8.28, 9.72, "s", "debris", 0.0, 0.3, 0.22, 0.0, 0, 0, {}},          /* pebble group */
    {"coast", "m_pebble_group_b", "rocks-misc/obj/pebbles_b.obj", "sandstone", 8.28, 9.72, "s", "debris", 0.0, 0.3, 0.22, 0.0, 0, 0, {}},          /* pebble group */
    {"mine", "m_pebble_group_b", "rocks-misc/obj/pebbles_b.obj", "rock", 8.28, 9.72, "s", "debris", 0.0, 0.3, 0.22, 0.0, 0, 0, {}},          /* pebble group */
    {"maahl", "m_pebble_group_b", "rocks-misc/obj/pebbles_b.obj", "sandstone", 8.28, 9.72, "s", "debris", 0.0, 0.3, 0.22, 0.0, 0, 0, {}},          /* pebble group */
    {"ashveil", "m_basalt_columns", "rocks-misc/obj/basalt_columns.obj", "rock", 16.56, 19.44, "s", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* hexagonal basalt columns */
    {"highland", "m_basalt_columns", "rocks-misc/obj/basalt_columns.obj", "slate", 16.56, 19.44, "s", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* hexagonal basalt columns */
    {"coast", "m_basalt_columns", "rocks-misc/obj/basalt_columns.obj", "sandstone", 16.56, 19.44, "s", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* hexagonal basalt columns */
    {"cynder", "m_basalt_columns", "rocks-misc/obj/basalt_columns.obj", "basalt", 16.56, 19.44, "s", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* hexagonal basalt columns */
    {"highland", "m_cairn", "rocks-misc/obj/cairn_a.obj", "slate", 7.728, 9.072, "s", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* stacked-stone cairn */
    {"maahl", "m_cairn", "rocks-misc/obj/cairn_a.obj", "sandstone", 7.728, 9.072, "s", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* stacked-stone cairn */
    {"frontier", "m_cairn", "rocks-misc/obj/cairn_a.obj", "rock", 7.728, 9.072, "s", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* stacked-stone cairn */
    {"highland", "m_cairn_b", "rocks-misc/obj/cairn_b.obj", "slate", 7.176, 8.424, "s", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* stacked-stone cairn with side stones */
    {"maahl", "m_cairn_b", "rocks-misc/obj/cairn_b.obj", "sandstone", 7.176, 8.424, "s", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* stacked-stone cairn with side stones */
    {"frontier", "m_cairn_b", "rocks-misc/obj/cairn_b.obj", "rock", 7.176, 8.424, "s", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* stacked-stone cairn with side stones */
    {"highland", "m_cairn_c", "rocks-misc/obj/cairn_c.obj", "slate", 6.072, 7.128, "s", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* stacked-stone cairn */
    {"maahl", "m_cairn_c", "rocks-misc/obj/cairn_c.obj", "sandstone", 6.072, 7.128, "s", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* stacked-stone cairn */
    {"frontier", "m_cairn_c", "rocks-misc/obj/cairn_c.obj", "rock", 6.072, 7.128, "s", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* stacked-stone cairn */
    {"badlands", "m_angular_boulder", "rocks-misc/obj/rock_large_a.obj", "rock", 8.832, 10.368, "s", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* angular boulder */
    {"highland", "m_angular_boulder", "rocks-misc/obj/rock_large_a.obj", "slate", 8.832, 10.368, "s", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* angular boulder */
    {"forest", "m_angular_boulder", "rocks-misc/obj/rock_large_a.obj", "rock", 8.832, 10.368, "s", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* angular boulder */
    {"mine", "m_angular_boulder", "rocks-misc/obj/rock_large_a.obj", "rock", 8.832, 10.368, "s", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* angular boulder */
    {"badlands", "m_angular_boulder_b", "rocks-misc/obj/rock_large_b.obj", "rock", 9.936, 11.664, "s", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* angular boulder */
    {"highland", "m_angular_boulder_b", "rocks-misc/obj/rock_large_b.obj", "slate", 9.936, 11.664, "s", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* angular boulder */
    {"forest", "m_angular_boulder_b", "rocks-misc/obj/rock_large_b.obj", "rock", 9.936, 11.664, "s", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* angular boulder */
    {"mine", "m_angular_boulder_b", "rocks-misc/obj/rock_large_b.obj", "rock", 9.936, 11.664, "s", "made", 0.0, 0.0, 0.22, 0.0, 0, 0, {}},          /* angular boulder */
    {"badlands", "m_bone_heap", "polyy-bones/obj/bone_37_heap.obj", "bone", 3.864, 4.536, "s", "debris", 0.0, 0.3, 0.22, 0.0, 0, 0, {}},          /* skulls and bones in every style (user, 2026-11-20) */
    {"highland", "m_skull_human", "polyy-bones/obj/skull_01_human.obj", "bone", 1.214, 1.426, "s", "made", 0.0, 0.2, 0.22, 0.0, 0, 0, {}},          /* skulls and bones in every style (user, 2026-11-20) */
    {"highland", "m_femur", "polyy-bones/obj/bone_29_femur.obj", "bone", 2.484, 2.916, "l", "debris", 0.0, 0.1, 0.22, 0.0, 0, 0, {}},          /* skulls and bones in every style (user, 2026-11-20) */
    {"highland", "m_bone_heap", "polyy-bones/obj/bone_37_heap.obj", "bone", 3.864, 4.536, "s", "debris", 0.0, 0.3, 0.22, 0.0, 0, 0, {}},          /* skulls and bones in every style (user, 2026-11-20) */
    {"maahl", "m_skull_human", "polyy-bones/obj/skull_01_human.obj", "bone", 1.214, 1.426, "s", "made", 0.0, 0.2, 0.22, 0.0, 0, 0, {}},          /* skulls and bones in every style (user, 2026-11-20) */
    {"maahl", "m_femur", "polyy-bones/obj/bone_29_femur.obj", "bone", 2.484, 2.916, "l", "debris", 0.0, 0.1, 0.22, 0.0, 0, 0, {}},          /* skulls and bones in every style (user, 2026-11-20) */
    {"maahl", "m_bone_heap", "polyy-bones/obj/bone_37_heap.obj", "bone", 3.864, 4.536, "s", "debris", 0.0, 0.3, 0.22, 0.0, 0, 0, {}},          /* skulls and bones in every style (user, 2026-11-20) */
    {"corridor", "m_skull_human", "polyy-bones/obj/skull_01_human.obj", "bone", 1.214, 1.426, "s", "made", 0.0, 0.2, 0.22, 0.0, 0, 0, {}},          /* skulls and bones in every style (user, 2026-11-20) */
    {"corridor", "m_femur", "polyy-bones/obj/bone_29_femur.obj", "bone", 2.484, 2.916, "l", "debris", 0.0, 0.1, 0.22, 0.0, 0, 0, {}},          /* skulls and bones in every style (user, 2026-11-20) */
    {"corridor", "m_bone_heap", "polyy-bones/obj/bone_37_heap.obj", "bone", 3.864, 4.536, "s", "debris", 0.0, 0.3, 0.22, 0.0, 0, 0, {}},          /* skulls and bones in every style (user, 2026-11-20) */
    {"infestation", "m_skull_human", "polyy-bones/obj/skull_01_human.obj", "bone", 1.214, 1.426, "s", "made", 0.0, 0.2, 0.22, 0.0, 0, 0, {}},          /* skulls and bones in every style (user, 2026-11-20) */
    {"infestation", "m_femur", "polyy-bones/obj/bone_29_femur.obj", "bone", 2.484, 2.916, "l", "debris", 0.0, 0.1, 0.22, 0.0, 0, 0, {}},          /* skulls and bones in every style (user, 2026-11-20) */
    {"urban", "m_femur", "polyy-bones/obj/bone_29_femur.obj", "bone", 2.484, 2.916, "l", "debris", 0.0, 0.1, 0.22, 0.0, 0, 0, {}},          /* skulls and bones in every style (user, 2026-11-20) */
    {"urban", "m_bone_heap", "polyy-bones/obj/bone_37_heap.obj", "bone", 3.864, 4.536, "s", "debris", 0.0, 0.3, 0.22, 0.0, 0, 0, {}},          /* skulls and bones in every style (user, 2026-11-20) */
    {"mine", "m_skull_human", "polyy-bones/obj/skull_01_human.obj", "bone", 1.214, 1.426, "s", "made", 0.0, 0.2, 0.22, 0.0, 0, 0, {}},          /* skulls and bones in every style (user, 2026-11-20) */
    {"mine", "m_femur", "polyy-bones/obj/bone_29_femur.obj", "bone", 2.484, 2.916, "l", "debris", 0.0, 0.1, 0.22, 0.0, 0, 0, {}},          /* skulls and bones in every style (user, 2026-11-20) */
    {"mine", "m_bone_heap", "polyy-bones/obj/bone_37_heap.obj", "bone", 3.864, 4.536, "s", "debris", 0.0, 0.3, 0.22, 0.0, 0, 0, {}},          /* skulls and bones in every style (user, 2026-11-20) */
    {"swamp", "m_bone_heap", "polyy-bones/obj/bone_37_heap.obj", "bone", 3.864, 4.536, "s", "debris", 0.0, 0.3, 0.22, 0.0, 0, 0, {}},          /* skulls and bones in every style (user, 2026-11-20) */
    {"cynder", "m_skull_human", "polyy-bones/obj/skull_01_human.obj", "bone", 1.214, 1.426, "s", "made", 0.0, 0.2, 0.22, 0.0, 0, 0, {}},          /* skulls and bones in every style (user, 2026-11-20) */
    {"cynder", "m_bone_heap", "polyy-bones/obj/bone_37_heap.obj", "bone", 3.864, 4.536, "s", "debris", 0.0, 0.3, 0.22, 0.0, 0, 0, {}},          /* skulls and bones in every style (user, 2026-11-20) */
    {"targa", "m_skull_human", "polyy-bones/obj/skull_01_human.obj", "bone", 1.214, 1.426, "s", "made", 0.0, 0.2, 0.22, 0.0, 0, 0, {}},          /* skulls and bones in every style (user, 2026-11-20) */
    {"targa", "m_femur", "polyy-bones/obj/bone_29_femur.obj", "bone", 2.484, 2.916, "l", "debris", 0.0, 0.1, 0.22, 0.0, 0, 0, {}},          /* skulls and bones in every style (user, 2026-11-20) */
    {"targa", "m_bone_heap", "polyy-bones/obj/bone_37_heap.obj", "bone", 3.864, 4.536, "s", "debris", 0.0, 0.3, 0.22, 0.0, 0, 0, {}},          /* skulls and bones in every style (user, 2026-11-20) */
    {"trederra", "m_bone_heap", "polyy-bones/obj/bone_37_heap.obj", "bone", 3.864, 4.536, "s", "debris", 0.0, 0.3, 0.22, 0.0, 0, 0, {}},          /* skulls and bones in every style (user, 2026-11-20) */
    {"fortress", "m_skull_human", "polyy-bones/obj/skull_01_human.obj", "bone", 1.214, 1.426, "s", "made", 0.0, 0.2, 0.22, 0.0, 0, 0, {}},          /* skulls and bones in every style (user, 2026-11-20) */
    {"fortress", "m_femur", "polyy-bones/obj/bone_29_femur.obj", "bone", 2.484, 2.916, "l", "debris", 0.0, 0.1, 0.22, 0.0, 0, 0, {}},          /* skulls and bones in every style (user, 2026-11-20) */
    {"fortress", "m_bone_heap", "polyy-bones/obj/bone_37_heap.obj", "bone", 3.864, 4.536, "s", "debris", 0.0, 0.3, 0.22, 0.0, 0, 0, {}},          /* skulls and bones in every style (user, 2026-11-20) */
    {"beast", "m_skull_human", "polyy-bones/obj/skull_01_human.obj", "bone", 1.214, 1.426, "s", "made", 0.0, 0.2, 0.22, 0.0, 0, 0, {}},          /* skulls and bones in every style (user, 2026-11-20) */
    {"frontier", "m_skull_human", "polyy-bones/obj/skull_01_human.obj", "bone", 1.214, 1.426, "s", "made", 0.0, 0.2, 0.22, 0.0, 0, 0, {}},          /* skulls and bones in every style (user, 2026-11-20) */
    {"frontier", "m_femur", "polyy-bones/obj/bone_29_femur.obj", "bone", 2.484, 2.916, "l", "debris", 0.0, 0.1, 0.22, 0.0, 0, 0, {}},          /* skulls and bones in every style (user, 2026-11-20) */
    {"frontier", "m_bone_heap", "polyy-bones/obj/bone_37_heap.obj", "bone", 3.864, 4.536, "s", "debris", 0.0, 0.3, 0.22, 0.0, 0, 0, {}},          /* skulls and bones in every style (user, 2026-11-20) */
    {"graveyard", "m_bone_heap", "polyy-bones/obj/bone_37_heap.obj", "bone", 3.864, 4.536, "s", "debris", 0.0, 0.3, 0.22, 0.0, 0, 0, {}},          /* skulls and bones in every style (user, 2026-11-20) */
    {"forest", "m_skull_human", "polyy-bones/obj/skull_01_human.obj", "bone", 1.214, 1.426, "s", "made", 0.0, 0.2, 0.22, 0.0, 0, 0, {}},          /* skulls and bones in every style (user, 2026-11-20) */
    {"forest", "m_femur", "polyy-bones/obj/bone_29_femur.obj", "bone", 2.484, 2.916, "l", "debris", 0.0, 0.1, 0.22, 0.0, 0, 0, {}},          /* skulls and bones in every style (user, 2026-11-20) */
    {"forest", "m_bone_heap", "polyy-bones/obj/bone_37_heap.obj", "bone", 3.864, 4.536, "s", "debris", 0.0, 0.3, 0.22, 0.0, 0, 0, {}},          /* skulls and bones in every style (user, 2026-11-20) */
    {"temple", "m_skull_human", "polyy-bones/obj/skull_01_human.obj", "bone", 1.214, 1.426, "s", "made", 0.0, 0.2, 0.22, 0.0, 0, 0, {}},          /* skulls and bones in every style (user, 2026-11-20) */
    {"temple", "m_femur", "polyy-bones/obj/bone_29_femur.obj", "bone", 2.484, 2.916, "l", "debris", 0.0, 0.1, 0.22, 0.0, 0, 0, {}},          /* skulls and bones in every style (user, 2026-11-20) */
    {"temple", "m_bone_heap", "polyy-bones/obj/bone_37_heap.obj", "bone", 3.864, 4.536, "s", "debris", 0.0, 0.3, 0.22, 0.0, 0, 0, {}},          /* skulls and bones in every style (user, 2026-11-20) */
    {"coast", "m_skull_human", "polyy-bones/obj/skull_01_human.obj", "bone", 1.214, 1.426, "s", "made", 0.0, 0.2, 0.22, 0.0, 0, 0, {}},          /* skulls and bones in every style (user, 2026-11-20) */
    {"coast", "m_femur", "polyy-bones/obj/bone_29_femur.obj", "bone", 2.484, 2.916, "l", "debris", 0.0, 0.1, 0.22, 0.0, 0, 0, {}},          /* skulls and bones in every style (user, 2026-11-20) */
    {"coast", "m_bone_heap", "polyy-bones/obj/bone_37_heap.obj", "bone", 3.864, 4.536, "s", "debris", 0.0, 0.3, 0.22, 0.0, 0, 0, {}},          /* skulls and bones in every style (user, 2026-11-20) */
    {"sundered", "m_bone_heap", "polyy-bones/obj/bone_37_heap.obj", "bone", 3.864, 4.536, "s", "debris", 0.0, 0.3, 0.22, 0.0, 0, 0, {}},          /* skulls and bones in every style (user, 2026-11-20) */
    {"ferrum", "m_skull_human", "polyy-bones/obj/skull_01_human.obj", "bone", 1.214, 1.426, "s", "made", 0.0, 0.2, 0.22, 0.0, 0, 0, {}},          /* skulls and bones in every style (user, 2026-11-20) */
    {"ferrum", "m_femur", "polyy-bones/obj/bone_29_femur.obj", "bone", 2.484, 2.916, "l", "debris", 0.0, 0.1, 0.22, 0.0, 0, 0, {}},          /* skulls and bones in every style (user, 2026-11-20) */
    {"ferrum", "m_bone_heap", "polyy-bones/obj/bone_37_heap.obj", "bone", 3.864, 4.536, "s", "debris", 0.0, 0.3, 0.22, 0.0, 0, 0, {}},          /* skulls and bones in every style (user, 2026-11-20) */
    {"thornwild", "m_bone_heap", "polyy-bones/obj/bone_37_heap.obj", "bone", 3.864, 4.536, "s", "debris", 0.0, 0.3, 0.22, 0.0, 0, 0, {}},          /* skulls and bones in every style (user, 2026-11-20) */
    {"obelis", "m_skull_human", "polyy-bones/obj/skull_01_human.obj", "bone", 1.214, 1.426, "s", "made", 0.0, 0.2, 0.22, 0.0, 0, 0, {}},          /* skulls and bones in every style (user, 2026-11-20) */
    {"obelis", "m_femur", "polyy-bones/obj/bone_29_femur.obj", "bone", 2.484, 2.916, "l", "debris", 0.0, 0.1, 0.22, 0.0, 0, 0, {}},          /* skulls and bones in every style (user, 2026-11-20) */
    {"obelis", "m_bone_heap", "polyy-bones/obj/bone_37_heap.obj", "bone", 3.864, 4.536, "s", "debris", 0.0, 0.3, 0.22, 0.0, 0, 0, {}},          /* skulls and bones in every style (user, 2026-11-20) */
/* IMPORT 2026-11-20 END */
/* END MODEL TABLE */
};

/* sd_mesh.solid(...)[0] as a solid: sect(z) = the model's outline above z */
static int msect(void *ctx, double z, GEOSGeometry **out) { int e = MDL_OK; *out = mdl_sect((MdlSolid *)ctx, z, &e); return e == MDL_OK ? SO_OK : SO_ERR; }
static void mfree(void *ctx) { mdl_solid_free((MdlSolid *)ctx); }
static int mhr(void *ctx, double x0, double y0, double g, int nx, int ny, double *H) { return mdl_hraster((MdlSolid *)ctx, x0, y0, g, nx, ny, H) == 0 ? SO_OK : SO_ERR; }   /* smooth details */
/* sol, foot, top = sd_mesh.solid(rel, mm, cx, cy, yaw, pose, sink, mirror, tilt, tilt_dir, cap): sol into *sol */
static int msolid(const char *rel, double mm, double cx, double cy, double yaw, char pose, double sink, int mirror, double tilt, double tdir, double cap, SoSolids *sol, GG **foot, double *top) {
    int err = 0; const MdlMesh *VF; MdlSolid *ms = NULL; GEOSGeometry *f = NULL;
    if (E_) return SO_ERR;
    if (!(VF = mdl_load(ROOT, rel, &err))) { fail(); return SO_ERR; }
    if (mdl_solid(VF, mm, cx, cy, yaw, pose, sink, mirror, tilt, tdir, cap, &ms, &f, top) != MDL_OK) { if (ms) mdl_solid_free(ms); shp_free(f); fail(); return SO_ERR; }
    *foot = gk(f);
    sol_push(sol, sol_custom(msect, ms, mfree, *top)); sol_unref(sol->v[sol->n - 1]);          /* the list holds the one reference */
    sol_custom_hraster(sol->v[sol->n - 1], mhr);
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
/* the model table for the window's Models preview (2026-11-20): row i's style, name, file and classic mm a unit; 0 past the end */
int bsdde_model_row(int i, const char **style, const char **name, const char **rel, double *mm) {
    if (i < 0 || i >= (int)(sizeof ROWS / sizeof ROWS[0])) return 0;
    *style = ROWS[i].style; *name = ROWS[i].name; *rel = ROWS[i].rel; *mm = (ROWS[i].mm0 + ROWS[i].mm1) / 2; return 1;
}
/* the rows of one Python file, registered as _model does it: a missing model file makes no detail */
void bsdde_models_register(void) {
    int n = (int)(sizeof ROWS / sizeof ROWS[0]), i;
    for (i = 0; i < n; i++) {
        if (!mdl_has(ROOT, ROWS[i].rel)) continue;
        bsdde_reg(ROWS[i].style, ROWS[i].cls, ROWS[i].kind, ROWS[i].name, 10.5, 0, m_build, (void *)&ROWS[i]);
    }
}
