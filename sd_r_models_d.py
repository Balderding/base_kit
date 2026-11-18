"""Details made from the Base Mesh collection (thebasemesh.com, public domain, CC0; its GLB copies come from
github.com/M3-org/base-meshes, see models/packs.json and tools/get_models.py): single plain objects other packs lack,
a horseshoe, a sheriff's star, shells and cartridges, spanners, a skull in profile. See sd_r_models.py for how a model
becomes a detail and sd_mesh.py for the adapter.

These models are in metres at their real size, from a 4 cm cartridge to a 2 m cannon, so each line gives the size the
thing is to have on the base instead: (name, model, kind, millimetres along its longest side, poses, extras), and SIDE
holds that longest side in metres. Nearly all lie down (a height map cannot show a standing bell or bottle as more
than a peg). A thing thinner than a printer can lay (a spanner, a grating) is lifted a little, which a height map
fills in underneath: that is the negative "sink"."""
import gg
from sd_r_models import _model

B = "base-meshes/glb/"
SIDE = {          # the longest side of each model, metres
    "762x51mm_bullet": 0.045, "allen_key_screw": 0.035, "axle_nut": 0.027, "barbed_arrowhead": 0.040, "baseball_bat": 0.820,
    "binoculars": 0.109, "breastplate_armour_02": 0.387, "bronze_age_sword": 0.600, "bull_head_rail": 1.000,
    "candle_holder_01": 0.278, "cannon_with_base": 1.966, "cartoon_fish_bone": 0.300, "concrete_block": 0.440,
    "dirt_pile_01": 2.446, "drainage_grate_01": 0.400, "dynamite_stick_bundle": 0.293,
    "firewood_01": 0.233, "gas_canister": 0.591, "gem": 0.250, "gladius_01": 0.739, "goblet_02": 0.175,
    "greek_spear_head_01": 0.173, "greek_spear_head_02": 0.173, "greek_vase_01": 0.400, "hacksaw_01": 0.255, "half_round_ridge_tile": 0.457,
    "hammer": 0.250, "hand_bell": 0.200, "horn": 0.476, "horse_shoe": 0.120, "ingot": 0.371, "insect_leg": 0.130,
    "japanese_stone_lantern_01": 0.847, "mace": 0.580, "mushroom_01": 0.100, "mushroom_03": 0.100,
    "oil_barrel": 0.860, "padlock": 0.095, "plastic_bottle_01": 0.200, "propeller_01": 0.277, "rhino_large_horn": 0.853,
    "roman_interlocking_tile": 0.420, "roman_pottery_10": 0.218, "rope_twist_01": 0.100, "round_shield": 0.750,
    "sai": 0.490, "salloon_door_01": 1.010, "scroll_02": 0.356, "shell_01": 0.040, "shell_02": 0.053, "ship_wheel": 1.002,
    "skull_no_teeth": 0.250, "sledgehammer": 0.712, "spanner": 0.251, "spring": 0.085, "star_badge": 0.100, "stick_03": 0.265,
    "stone_tomb_01": 2.206, "teapot": 0.257, "throwing_knife": 0.300, "trowel": 0.280, "valve_wheel_01": 0.500, "valve_wheel_02": 0.070,
    "vent_cover_01": 0.450, "walkie_talkie": 0.187, "wellington_boot": 0.407,
    "whiskey_bottle": 0.313, "wine_bottle": 0.299, "wine_jug_01": 0.337, "wooden_bowl_01": 0.175, "wooden_mallet": 0.300, "4_point_shuriken": 0.090,
}

def _table(style, rows):
    for name, model, kind, size, poses, kw in rows:
     kw = dict(kw); k = 1.0 / SIDE[model]
     if kw.get("cap"): kw["cap"] = min(kw["cap"], gg.max_height() * 0.92)          # a cap of our own never lifts a height limit the user set
     _model(style, name, B + model + ".glb", kind, (size[0] * k, size[1] * k), poses, **kw)

FLAT = dict(tilt=0.0)          # a flat thing lies flat
def lift(a, b, **kw): return dict(sink=(-a, -b), tilt=0.0, **kw)          # raised by a to b times its own thickness

_table("badlands", [
    ("m_horseshoe", "horse_shoe", "iron", (5.0, 6.0), "b", lift(0.5, 0.2)),
    ("m_sheriff_star", "star_badge", "tin", (4.5, 5.5), "b", dict(tilt=0.06)),
    ("m_horn", "horn", "bone", (7.5, 9.0), "bf", dict(sink=(0.0, 0.2), tilt=0.1)),
    ("m_cartridge", "762x51mm_bullet", "tin", (5.0, 6.0), "lr", dict(tilt=0.08)),
    ("m_boot", "wellington_boot", "leather", (5.5, 6.5), "bf", dict(sink=(0.0, 0.2), tilt=0.1)),
    ("m_whiskey_bottle", "whiskey_bottle", "glass", (6.5, 7.5), "lr", dict(sink=(0.0, 0.2), tilt=0.1)),
])

_table("frontier", [
    ("m_horseshoe", "horse_shoe", "iron", (5.0, 6.0), "b", lift(0.5, 0.2)),
    ("m_sheriff_star", "star_badge", "tin", (4.5, 5.5), "b", dict(tilt=0.06)),
    ("m_whiskey_bottle", "whiskey_bottle", "glass", (6.5, 7.5), "lr", dict(tilt=0.1)),
    ("m_claw_hammer", "hammer", "iron", (7.5, 8.5), "bf", dict(tilt=0.08)),
    ("m_padlock", "padlock", "iron", (4.5, 5.5), "b", dict(tilt=0.06)),
])

_table("mine", [
    ("m_ingot", "ingot", "brass", (6.0, 7.0), "s", dict(tilt=0.08)),
    ("m_rail", "bull_head_rail", "iron", (11.0, 13.0), "lr", FLAT),
    ("m_gear", "valve_wheel_02", "iron", (5.5, 6.5), "s", dict(tilt=0.06)),
    ("m_gem", "gem", "rock", (4.5, 5.5), "bf", dict(tilt=0.1)),
    ("m_spoil_heap", "dirt_pile_01", "rock", (8.0, 10.0), "s", dict(cls="debris", tilt=0.0)),
])

_table("urban", [
    ("m_cinder_block", "concrete_block", "concrete", (5.5, 6.0), "s", dict(tilt=0.08)),
    ("m_drain_grate", "drainage_grate_01", "iron", (6.5, 8.0), "s", lift(1.0, 0.5)),
    ("m_gas_bottle", "gas_canister", "tin", (5.2, 5.6), "lr", dict(tilt=0.06)),
    ("m_bat", "baseball_bat", "wood", (10.0, 12.0), "lr", dict(tilt=0.05)),
    ("m_spanner", "spanner", "iron", (8.0, 10.0), "s", lift(1.2, 0.6)),
    ("m_bottle", "wine_bottle", "glass", (6.5, 7.5), "lr", dict(tilt=0.1)),
    ("m_jerrycan", "plastic_bottle_01", "tin", (5.0, 6.0), "bf", dict(tilt=0.08)),
    ("m_hacksaw", "hacksaw_01", "iron", (8.0, 9.5), "bf", lift(0.8, 0.3)),
])

_table("corridor", [
    ("m_vent_cover", "vent_cover_01", "plate", (7.0, 8.5), "b", lift(1.5, 0.8)),
    ("m_spanner", "spanner", "metal", (8.0, 10.0), "s", lift(1.2, 0.6)),
    ("m_nut", "axle_nut", "metal", (4.5, 5.5), "s", dict(tilt=0.05)),
    ("m_bolt", "allen_key_screw", "metal", (5.0, 5.8), "lr", dict(tilt=0.05)),
    ("m_spring", "spring", "metal", (6.5, 7.5), "lr", dict(tilt=0.05)),
    ("m_gas_bottle", "gas_canister", "tin", (5.2, 5.6), "lr", dict(tilt=0.06)),
    ("m_fan", "propeller_01", "metal", (6.0, 7.0), "b", dict(tilt=0.06)),
    ("m_radio", "walkie_talkie", "metal", (7.0, 8.0), "b", dict(tilt=0.08)),
])

_table("targa", [
    ("m_valve_wheel", "valve_wheel_01", "metal", (6.0, 7.0), "s", dict(sink=(0.0, 0.2), tilt=0.08)),
    ("m_gear", "valve_wheel_02", "metal", (5.5, 6.5), "s", dict(sink=(0.0, 0.2), tilt=0.08)),
    ("m_vent", "vent_cover_01", "metal", (7.0, 8.5), "b", lift(1.5, 0.8)),
    ("m_spanner", "spanner", "metal", (8.0, 10.0), "s", lift(1.2, 0.6)),
    ("m_gas_bottle", "gas_canister", "metal", (5.4, 6.2), "lr", dict(sink=(0.1, 0.25), tilt=0.08)),
    ("m_fan", "propeller_01", "metal", (6.0, 7.0), "b", dict(sink=(0.0, 0.2), tilt=0.06)),
    ("m_bolt", "allen_key_screw", "metal", (5.0, 5.8), "lr", dict(sink=(0.0, 0.2), tilt=0.05)),
])

_table("trederra", [
    ("m_shell", "762x51mm_bullet", "brass", (6.5, 7.5), "lr", dict(sink=(0.0, 0.2), tilt=0.1)),
    ("m_boot", "wellington_boot", "leather", (5.5, 6.5), "bf", dict(sink=(0.0, 0.25), tilt=0.1)),
    ("m_jerrycan", "plastic_bottle_01", "tin", (5.0, 6.0), "bf", dict(sink=(0.0, 0.2), tilt=0.08)),
    ("m_gas_cylinder", "gas_canister", "tin", (5.4, 6.2), "lr", dict(sink=(0.1, 0.25), tilt=0.08)),
    ("m_oil_drum", "oil_barrel", "tin", (4.8, 5.4), "lr", dict(sink=(0.15, 0.3), tilt=0.06)),
    ("m_sledgehammer", "sledgehammer", "iron", (10.0, 12.0), "bf", dict(tilt=0.06)),
])

_table("fortress", [
    ("m_stone_lantern", "japanese_stone_lantern_01", "garden", (8.5, 9.5), "b", dict(tilt=0.05)),
    ("m_shuriken", "4_point_shuriken", "iron", (4.5, 5.5), "b", lift(2.0, 1.2)),
    ("m_kunai", "throwing_knife", "iron", (8.0, 9.5), "s", lift(0.8, 0.3)),
    ("m_sai", "sai", "iron", (9.0, 11.0), "bf", lift(0.4, 0.0)),
    ("m_teapot", "teapot", "iron", (4.8, 5.2), "b", dict(tilt=0.05)),
    ("m_rice_bowl", "wooden_bowl_01", "lacquer", (4.0, 5.0), "s", dict(tilt=0.06)),
    ("m_roof_tile", "half_round_ridge_tile", "clay", (6.0, 7.5), "s", dict(tilt=0.08)),
    ("m_roof_tiles", "roman_interlocking_tile", "clay", (6.5, 8.0), "s", dict(tilt=0.08)),
    ("m_scroll", "scroll_02", "paper", (5.5, 6.5), "b", dict(tilt=0.05)),
    ("m_cuirass", "breastplate_armour_02", "lacquer", (6.0, 6.6), "b", dict(tilt=0.05)),
])

_table("dungeon", [
    ("m_skull", "skull_no_teeth", "bone", (4.4, 5.2), "lr", dict(sink=(0.05, 0.25), tilt=0.1)),
    ("m_mace", "mace", "iron", (10.0, 12.0), "lr", dict(tilt=0.05)),
    ("m_breastplate", "breastplate_armour_02", "iron", (6.0, 6.6), "b", dict(tilt=0.05)),
    ("m_padlock", "padlock", "iron", (4.5, 5.5), "b", dict(tilt=0.06)),
    ("m_goblet", "goblet_02", "gold", (4.8, 5.4), "lr", dict(tilt=0.06)),
    ("m_gem", "gem", "glass", (4.5, 5.5), "bf", dict(tilt=0.1)),
    ("m_candlestick", "candle_holder_01", "brass", (7.0, 8.2), "lr", dict(tilt=0.05)),
    ("m_scroll", "scroll_02", "paper", (5.5, 6.5), "b", dict(tilt=0.05)),
])

_table("temple", [
    ("m_amphora", "greek_vase_01", "tcolumn", (5.5, 6.5), "lr", dict(sink=(0.25, 0.4), tilt=0.1)),
    ("m_dish", "roman_pottery_10", "tcolumn", (5.0, 6.0), "s", dict(tilt=0.08)),
    ("m_spearhead", "greek_spear_head_01", "obsidian", (7.0, 8.5), "bf", dict(tilt=0.06)),
    ("m_shield", "round_shield", "gold", (7.0, 8.5), "b", dict(tilt=0.1)),
    ("m_goblet", "goblet_02", "gold", (4.8, 5.4), "lr", dict(tilt=0.06)),
    ("m_jade_gem", "gem", "jade", (4.5, 5.5), "bf", dict(tilt=0.1)),
    ("m_gold_bar", "ingot", "gold", (6.0, 7.0), "s", dict(tilt=0.08)),
])

_table("highland", [
    ("m_round_shield", "round_shield", "wood", (7.0, 8.5), "b", dict(tilt=0.1)),
    ("m_bronze_sword", "bronze_age_sword", "brass", (10.0, 12.0), "bf", dict(tilt=0.04)),
    ("m_spearhead", "greek_spear_head_02", "brass", (6.0, 7.5), "bf", dict(tilt=0.06)),
    ("m_horn", "horn", "bone", (7.5, 9.0), "bf", dict(sink=(0.0, 0.2), tilt=0.1)),
    ("m_skull", "skull_no_teeth", "bone", (4.4, 5.2), "lr", dict(sink=(0.05, 0.25), tilt=0.1)),
    ("m_arrowhead", "barbed_arrowhead", "slate", (4.0, 4.8), "b", dict(tilt=0.06)),
    ("m_bowl", "wooden_bowl_01", "wood", (4.0, 5.0), "s", dict(tilt=0.08)),
])

_table("graveyard", [
    ("m_skull", "skull_no_teeth", "bone", (4.4, 5.2), "lr", dict(sink=(0.05, 0.25), tilt=0.1)),
    ("m_tomb", "stone_tomb_01", "headstone", (6.4, 7.2), "s", dict(sink=(0.0, 0.2), tilt=0.05)),
    ("m_mallet", "wooden_mallet", "wood", (6.5, 7.5), "bf", dict(tilt=0.08)),
    ("m_trowel", "trowel", "iron", (6.5, 7.5), "bf", dict(tilt=0.08)),
])

_table("forest", [
    ("m_mushroom", "mushroom_01", "shroom", (4.6, 5.0), "lr", dict(cls="debris", tilt=0.06)),
    ("m_toadstool", "mushroom_03", "shroom", (4.8, 5.4), "lr", dict(cls="debris", tilt=0.06)),
    ("m_firewood", "firewood_01", "log", (5.5, 6.5), "bf", dict(tilt=0.1)),
    ("m_mallet", "wooden_mallet", "wood", (6.5, 7.5), "bf", dict(tilt=0.08)),
])

_table("swamp", [
    ("m_boot", "wellington_boot", "hide", (5.5, 6.5), "bf", dict(sink=(0.0, 0.3), tilt=0.1)),
    ("m_mushroom", "mushroom_01", "shroom", (4.6, 5.0), "lr", dict(cls="debris", tilt=0.06)),
    ("m_toadstool", "mushroom_03", "shroom", (4.8, 5.4), "lr", dict(cls="debris", tilt=0.06)),
    ("m_fish_bone", "cartoon_fish_bone", "bone", (8.0, 10.0), "bf", dict(tilt=0.05)),
    ("m_jug", "wine_jug_01", "clay", (5.0, 6.0), "lr", dict(sink=(0.25, 0.4), tilt=0.1)),
])

_table("coast", [
    ("m_ship_wheel", "ship_wheel", "drift", (9.0, 10.5), "b", dict(tilt=0.06)),
    ("m_scallop", "shell_02", "shell", (4.5, 5.5), "s", dict(cls="debris", tilt=0.1)),
    ("m_seashell", "shell_01", "shell", (5.0, 6.0), "b", dict(cls="debris", tilt=0.1)),
    ("m_propeller", "propeller_01", "brass", (6.0, 7.0), "b", dict(sink=(0.0, 0.2), tilt=0.06)),
    ("m_cannon", "cannon_with_base", "iron", (7.5, 8.8), "s", dict(sink=(0.0, 0.2), tilt=0.06)),
    ("m_fish_bone", "cartoon_fish_bone", "bone", (8.0, 10.0), "bf", dict(tilt=0.05)),
    ("m_rope", "rope_twist_01", "drift", (6.5, 7.5), "lr", dict(tilt=0.05)),
    ("m_amphora", "greek_vase_01", "sandstone", (5.5, 6.5), "lr", dict(sink=(0.25, 0.4), tilt=0.1)),
])

_table("beast", [
    ("m_horn", "horn", "tooth", (7.5, 9.0), "bf", dict(cls="debris", sink=(0.0, 0.25), tilt=0.1)),
    ("m_tusk", "rhino_large_horn", "tooth", (9.0, 11.0), "lr", dict(cls="debris", tilt=0.05)),
    ("m_fish_bone", "cartoon_fish_bone", "bone", (8.0, 10.0), "bf", dict(cls="debris", tilt=0.05)),
    ("m_insect_leg", "insect_leg", "bone", (9.0, 11.0), "b", dict(cls="debris", tilt=0.05)),
    ("m_ship_wheel", "ship_wheel", "wood", (9.0, 10.5), "b", dict(tilt=0.06)),
    ("m_cannon", "cannon_with_base", "tin", (7.8, 9.4), "s", dict(sink=(0.15, 0.35), tilt=0.1)),
])

_table("infestation", [
    ("m_insect_leg", "insect_leg", "carapace", (9.0, 11.0), "b", dict(cls="debris", tilt=0.05)),
    ("m_spine", "rhino_large_horn", "spine", (9.0, 11.0), "lr", dict(cls="debris", tilt=0.05)),
])

_table("cynder", [
    ("m_skull", "skull_no_teeth", "tooth", (4.4, 5.2), "lr", dict(sink=(0.05, 0.3), tilt=0.1)),
    ("m_horn", "horn", "tooth", (7.5, 9.0), "bf", dict(sink=(0.0, 0.25), tilt=0.1)),
    ("m_tusk", "rhino_large_horn", "tooth", (9.0, 11.0), "lr", dict(tilt=0.05)),
    ("m_mace", "mace", "iron", (10.0, 12.0), "lr", dict(tilt=0.05)),
    ("m_sword", "gladius_01", "iron", (12.0, 14.0), "bf", dict(tilt=0.03)),
    ("m_breastplate", "breastplate_armour_02", "iron", (6.0, 6.6), "b", dict(tilt=0.05)),
])

_table("maahl", [
    ("m_amphora", "greek_vase_01", "clay", (5.5, 6.5), "lr", dict(sink=(0.25, 0.4), tilt=0.1)),
    ("m_dish", "roman_pottery_10", "clay", (5.0, 6.0), "s", dict(sink=(0.0, 0.2), tilt=0.08)),
    ("m_spearhead", "greek_spear_head_01", "brass", (7.0, 8.5), "bf", dict(tilt=0.06)),
    ("m_shield", "round_shield", "brass", (7.0, 8.5), "b", dict(sink=(0.0, 0.2), tilt=0.1)),
    ("m_insect_leg", "insect_leg", "chitin", (9.0, 11.0), "b", dict(cls="debris", tilt=0.05)),
    ("m_horn", "horn", "chitin", (7.5, 9.0), "bf", dict(cls="debris", sink=(0.0, 0.2), tilt=0.1)),
])
