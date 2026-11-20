/* bld_features.h: the builders of features.py in C (bld_features.c, agent "bld-features", group G10).
   Every b_* builder and every L_* landmark of features.py, copied routine by routine; each is an FtBuildFn
   (features.h) made by a BldFactory (builders.h) from an FtCall: the factory reads the call's arguments (Python's
   defaults where one is left out), the builder then runs as the Python closure f(cx, cy, a, rng[, k]) does.

   Registered (bld_register_features) under the module "features" and the Python name WITH its prefix, because
   features.py has b_crater and L_crater, b_track and L_track ... (the same FtCall.name in FT_NEW and FT_LMS):
       bld_get("features", "b_wheel") ... bld_get("features", "L_wall") ...
   An FtCall of FT_NEW / FT_LMS (mod NULL, name without the prefix) is made with bld_ft_make below.

   Python semantics kept:
   - a builder that takes no k (b_wheel, b_ladder ...) called with a scale (has_scale) raises TypeError: SC_ERR,
     before any random number is drawn. An L_* builder called without k raises the same way.
   - b_skull, b_ribs and L_skeleton return scenes.py's rounded builders (scenes.b_skull(kind, big) ...): their
     factories hand the call on to the "scenes" entries of the table ("scenes", "b_skull" / "b_ribs" / "L_skeleton",
     the call's mod set to "scenes" and its name to that), so they work once scenes.py's builders are registered;
     until then bld_make gives -2 (not ported yet).
   - random numbers are drawn in exactly Python's order (left to right inside an expression, conditions first).
   - mound layers carry their tiers as LV_PTR to an FtTiers: features.PEB_T / SPIKE_T are FT_PEB_T / FT_SPIKE_T; a
     tiers= argument (b_fairy_ring) lives in the builder's ctx (kept until bld_free_all).
   - geometries in the returned footprint and layers are new objects each (no two layers share one object).
   Proof: work\cscene\bld_features (bfprove.py: real calls inside real bases + random cases, byte for byte). */
#ifndef BLD_FEATURES_H
#define BLD_FEATURES_H
#include "builders.h"

/* an FtCall as FT_NEW (landmark 0) / FT_LMS (landmark 1) write it: mod NULL = features.py, else the style module
   ("style_blight" ...); the name without "b_" / "L_". Looked up as (mod or "features", "b_" / "L_" + name), the Python
   name: blight has b_rune_ring and L_rune_ring. -2 = not registered. */
int bld_ft_make(const FtCall *call, int landmark, FtBuildFn *fn, void **ctx);
void bld_register_features(void);
#endif
