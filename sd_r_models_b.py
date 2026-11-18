"""More details made from ready-made public-domain (CC0) models, for the modern and science-fiction styles (see
sd_r_models.py for how a model becomes a detail and sd_mesh.py for the adapter). One table per style; each line is
one detail: (name, model file, kind, millimetres per model unit, poses, extras). Tall thin things (lamp posts, signs,
tools, guns) are only offered lying down, where they come out as a clean relief."""
import gg
from sd_r_models import _model

KEN = "/Models/OBJ format/"
KC, KS = "kaykit-city/obj/", "kaykit-space-base/obj/"
RO, RU, CA, IN = "city-kit-roads" + KEN, "retro-urban-kit" + KEN, "car-kit" + KEN, "city-kit-industrial" + KEN
SK, SS, FK, BK, MS = "space-kit" + KEN, "space-station-kit" + KEN, "factory-kit" + KEN, "blaster-kit" + KEN, "modular-space-kit" + KEN
SV, TD = "survival-kit" + KEN, "tower-defense-kit" + KEN

def _table(style, rows):
    for name, rel, kind, mm, poses, kw in rows:
        kw = dict(kw)
        if kw.get("cap"): kw["cap"] = min(kw["cap"], gg.max_height() * 0.92)          # a cap of our own never lifts a height limit the user set
        _model(style, name, rel + ".obj", kind, mm, poses, **kw)

_table("urban", [
    ("m_hydrant", KC + "firehydrant", "iron", (19.0, 22.0), "sssl", dict(cap=4.6)),
    ("m_dumpster", KC + "dumpster", "tin", (14.0, 16.0), "s", dict(cap=4.8)),
    ("m_dumpster_ro", RO + "dumpster", "tin", (22.0, 24.0), "s", dict(cap=4.8)),
    ("m_bench", RU + "detail-bench", "wood", (11.0, 12.5), "s", dict(cap=4.2)),
    ("m_street_light", KC + "streetlight", "iron", (10.5, 12.0), "b", {}),
    ("m_tyre", CA + "debris-tire", "leather", (7.0, 8.0), "llr", {}),
    ("m_cone", CA + "cone", "tin", (6.5, 7.5), "s", dict(cap=4.4)),
    ("m_concrete_barrier", RU + "detail-barrier-strong-damaged", "concrete", (12.5, 14.0), "s", dict(cap=4.4)),
    ("m_stop_sign", RO + "road-sign-stop", "iron", (19.0, 22.0), "l", {}),
    ("m_axle", CA + "debris-drivetrain-axle", "iron", (7.0, 8.0), "s", {}),
    ("m_tank", IN + "detail-tank", "tin", (10.0, 11.5), "s", dict(cap=4.8)),
])

_table("corridor", [
    ("m_container_flat", SS + "container-flat", "metal", (7.5, 8.5), "s", dict(cap=4.6)),
    ("m_container_tall", SS + "container-tall", "metal", (8.0, 9.0), "lbs", dict(cap=5.4)),
    ("m_pipe_bend", SS + "pipe-bend", "pipe", (13.0, 15.0), "bf", {}),
    ("m_valve", FK + "pipe-large-valve", "pipe", (5.5, 6.5), "s", dict(cap=5.2)),
    ("m_cog", FK + "cog-a", "metal", (6.5, 7.5), "s", dict(tilt=0.05)),
    ("m_lever", FK + "lever-single", "metal", (11.0, 12.5), "s", dict(cap=5.0)),
    ("m_crate", BK + "crate-small", "metal", (9.5, 10.5), "s", {}),
    ("m_canisters", KS + "containers_A", "tin", (12.5, 14.0), "s", {}),
    ("m_holo_table", SS + "table-display-planet", "glass", (11.5, 13.0), "s", dict(cap=5.0, tilt=0.06)),
    ("m_cables", MS + "cables", "cable", (4.6, 5.2), "s", {}),
    ("m_beacon", FK + "warning-orange", "glass", (5.5, 6.5), "lr", {}),
])

_table("targa", [
    ("m_ice_crystals", TD + "snow-detail-crystal-large", "ice", (8.5, 9.5), "s", dict(cls="debris", cap=5.0)),
    ("m_ice_shard", TD + "snow-detail-crystal", "ice", (13.0, 15.0), "s", dict(cls="debris", cap=5.2)),
    ("m_snow_rocks", TD + "snow-detail-rocks", "ice", (11.0, 13.0), "s", dict(cls="debris")),
    ("m_cargo", KS + "cargo_B_packed", "metal", (10.0, 11.5), "s", dict(cap=5.0, sink=(0.0, 0.25))),
    ("m_canisters", KS + "containers_B", "metal", (12.5, 14.0), "s", dict(sink=(0.0, 0.2))),
    ("m_lights", KS + "lights", "metal", (8.0, 9.0), "b", {}),
    ("m_skip", SS + "skip", "metal", (6.5, 7.5), "s", dict(cap=4.6, sink=(0.0, 0.2))),
    ("m_tank", SS + "container-tall", "metal", (8.0, 9.0), "lb", dict(sink=(0.0, 0.2))),
    ("m_crate", BK + "crate-small", "metal", (9.5, 10.5), "s", dict(sink=(0.0, 0.2))),
])

_table("maahl", [
    ("m_sand_rock", SV + "rock-sand-a", "sandstone", (10.5, 12.0), "s", dict(cls="debris", cap=5.0)),
    ("m_sand_rock_b", SV + "rock-sand-b", "sandstone", (8.5, 10.0), "s", dict(cls="debris", cap=5.0)),
    ("m_sand_rock_c", SV + "rock-sand-c", "sandstone", (8.5, 10.0), "s", dict(cls="debris", cap=5.4)),
    ("m_crystals", TD + "detail-crystal-large", "crystal", (8.5, 9.5), "s", dict(cls="debris", cap=5.0)),
    ("m_crystal_rock", SK + "rock_crystals", "crystal", (9.0, 10.5), "s", dict(cls="debris")),
    ("m_crater", SK + "crater", "sandstone", (10.5, 12.0), "s", dict(cls="debris")),
    ("m_meteor", SK + "meteor_half", "sandstone", (8.5, 10.0), "s", dict(cls="debris")),
    ("m_rocks", KS + "rocks_A", "sandstone", (5.5, 6.5), "s", dict(cls="debris")),
    ("m_stones", TD + "detail-rocks", "sandstone", (11.0, 13.0), "s", dict(cls="debris")),
])

_table("trederra", [
    ("m_barrel", SV + "barrel", "wood", (14.0, 16.0), "lr", {}),
    ("m_shovel", SV + "tool-shovel", "iron", (27.0, 30.0), "bf", {}),
    ("m_pickaxe", SV + "tool-pickaxe", "iron", (28.0, 32.0), "bf", {}),
    ("m_bedroll_packed", SV + "bedroll-packed", "cloth", (21.0, 24.0), "s", {}),
    ("m_duckboard", SV + "floor-old", "wood", (14.0, 15.5), "s", {}),
    ("m_ammo_crate", BK + "crate-medium", "wood", (8.5, 9.5), "s", {}),
    ("m_campfire", SV + "campfire-pit", "wood", (19.0, 22.0), "s", {}),
    ("m_grenade", BK + "grenade-a", "iron", (17.0, 19.0), "lrs", {}),
    ("m_tin_sheet", SV + "metal-panel", "tin", (13.0, 15.0), "bf", {}),
    ("m_chest", SV + "chest", "wood", (16.0, 18.0), "s", dict(cap=4.6)),
])

_table("cynder", [
    ("m_meteor", SK + "meteor_detailed", "basalt", (7.5, 8.5), "s", dict(cls="debris", cap=5.2, sink=(0.1, 0.35))),
    ("m_crater", SK + "craterLarge", "ash", (10.0, 11.5), "s", dict(cls="debris")),
    ("m_shards", SK + "rock_crystalsLargeA", "obsidian", (8.5, 9.5), "s", dict(cls="debris", cap=5.2)),
    ("m_boulders", TD + "detail-rocks-large", "basalt", (8.5, 9.5), "s", dict(cls="debris")),
])

# infestation: nothing in these packs reads as hive growth at this size, so it gets none
