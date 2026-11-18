"""More details made from ready-made public-domain models (see sd_r_models.py and sd_mesh.py): forest, swamp, coast,
frontier, highland, beast and badlands. One line of a table is one detail. Tall thin things only lie down (the surface
has no overhangs), and every tall model has a cap so that it stays a detail and not a tower."""
from sd_r_models import _model

N = "nature-kit/Models/OBJ format/"
S = "survival-kit/Models/OBJ format/"
P = "pirate-kit/Models/OBJ format/"
T = "fantasy-town-kit/Models/OBJ format/"
M = "mini-forest/Models/OBJ format/"
K = "kaykit-halloween/obj/"

def _table(style, rows):
    for name, f, cls, kind, mm, poses, kw in rows:
        _model(style, name, f + ".obj", kind, mm, poses, cls, **kw)

_table("forest", [
    ("m_log", N + "log", "debris", "log", (12.0, 15.0), "s", {}),
    ("m_tree_log", S + "tree-log", "debris", "log", (9.5, 11.5), "s", dict(cap=3.4)),
    ("m_fallen_tree", K + "tree_dead_small", "debris", "log", (3.2, 3.9), "bf", {}),
    ("m_campfire", N + "campfire_stones", "made", "rock", (13.0, 16.0), "s", dict(tilt=0.0)),
    ("m_fire_pit", S + "campfire-pit", "made", "rock", (22.0, 27.0), "s", dict(tilt=0.0)),
    ("m_rocks", M + "stones", "debris", "rock", (8.0, 10.0), "s", dict(cap=4.5, tilt=0.0)),
    ("m_axe", S + "tool-axe", "made", "wood", (28.0, 34.0), "bf", {}),
])

_table("swamp", [
    ("m_canoe", N + "canoe", "made", "log", (8.5, 10.2), "s", dict(sink=(0.0, 0.05))),
    ("m_paddle", P + "tool-paddle", "made", "log", (3.6, 4.4), "bf", {}),
    ("m_rowboat", P + "boat-row-small", "made", "log", (3.4, 3.9), "s", dict(sink=(0.0, 0.25), tilt=0.12)),
    ("m_log", N + "log", "made", "log", (12.0, 15.0), "s", dict(sink=(0.0, 0.25))),
    ("m_tree_log", S + "tree-log", "made", "log", (9.5, 11.5), "s", dict(sink=(0.0, 0.25), cap=3.4)),
    ("m_dead_tree", K + "tree_dead_small", "made", "log", (3.2, 3.9), "bf", dict(sink=(0.0, 0.2))),
    ("m_skull", K + "skull", "made", "bone", (4.6, 5.6), "b", dict(sink=(0.0, 0.2))),
    ("m_bone", K + "bone_C", "made", "bone", (6.0, 7.5), "s", dict(sink=(0.0, 0.2))),
    ("m_bone_knob", K + "bone_B", "made", "bone", (8.0, 10.0), "s", dict(sink=(0.0, 0.2))),
    ("m_fish", S + "fish-large", "made", "hide", (22.0, 27.0), "bf", dict(tilt=0.1)),
])

_table("coast", [
    ("m_rowboat", P + "boat-row-small", "made", "drift", (3.4, 3.9), "s", dict(sink=(0.0, 0.25), tilt=0.12)),
    ("m_barrel", P + "barrel", "made", "drift", (3.4, 4.2), "s", dict(sink=(0.0, 0.15), cap=5.2)),
    ("m_keg", S + "barrel", "made", "drift", (17.0, 21.0), "lr", dict(sink=(0.1, 0.35))),
    ("m_crate_bottles", P + "crate-bottles", "made", "glass", (4.2, 5.0), "s", dict(tilt=0.1)),
    ("m_chest", P + "chest", "made", "brass", (3.8, 4.6), "s", dict(sink=(0.0, 0.2), cap=5.2, tilt=0.12)),
    ("m_bottle", P + "bottle", "made", "glass", (6.5, 8.0), "blr", dict(sink=(0.0, 0.2))),
    ("m_paddle", P + "tool-paddle", "made", "drift", (3.6, 4.4), "bf", {}),
    ("m_shovel", P + "tool-shovel", "made", "iron", (3.6, 4.4), "bf", {}),
    ("m_fish", S + "fish-large", "made", "shell", (22.0, 27.0), "bf", dict(tilt=0.1)),
    ("m_rocks", P + "rocks-sand-c", "made", "sandstone", (2.3, 2.9), "s", dict(cap=5.5, tilt=0.0)),
])

_table("frontier", [
    ("m_barrel", S + "barrel", "made", "wood", (17.0, 21.0), "lr", {}),
    ("m_barrel_upright", P + "barrel", "made", "wood", (3.4, 4.2), "s", dict(cap=5.2)),
    ("m_bottle_crate", P + "crate-bottles", "made", "board", (4.2, 5.0), "s", dict(tilt=0.1)),
    ("m_cart", T + "cart", "made", "wood", (7.0, 8.2), "s", dict(cap=5.5, tilt=0.08)),
    ("m_wagon_wheel", T + "wheel", "made", "wood", (15.0, 19.0), "lr", dict(tilt=0.1)),
    ("m_chest", S + "chest", "made", "wood", (17.0, 21.0), "s", dict(cap=5.0, tilt=0.12)),
    ("m_pickaxe", S + "tool-pickaxe", "made", "iron", (30.0, 36.0), "bf", {}),
    ("m_shovel", S + "tool-shovel", "made", "iron", (28.0, 34.0), "bf", {}),
    ("m_axe", S + "tool-axe", "made", "iron", (28.0, 34.0), "bf", {}),
    ("m_signpost", S + "signpost", "made", "board", (20.0, 24.0), "b", {}),
])

_table("highland", [
    ("m_standing_stone", N + "stone_tallC", "made", "slate", (8.5, 10.5), "sssb", dict(sink=(0.0, 0.15), cap=7.0)),
    ("m_standing_stone_b", N + "stone_tallD", "made", "slate", (8.5, 10.5), "sssb", dict(sink=(0.0, 0.15), cap=7.0)),
    ("m_standing_stone_c", N + "stone_tallG", "made", "slate", (8.5, 10.5), "sssf", dict(sink=(0.0, 0.15), cap=7.0)),
    ("m_standing_stone_d", N + "stone_tallI", "made", "slate", (8.5, 10.5), "sssf", dict(sink=(0.0, 0.15), cap=7.0)),
    ("m_crag", N + "stone_tallA", "debris", "slate", (7.5, 9.0), "s", dict(sink=(0.0, 0.15), cap=7.0, tilt=0.0)),
    ("m_crag_b", N + "stone_tallB", "debris", "slate", (8.0, 10.0), "s", dict(sink=(0.0, 0.15), cap=7.0, tilt=0.0)),
    ("m_leaning_stone", N + "stone_tallH", "made", "slate", (9.0, 11.0), "s", dict(sink=(0.0, 0.15), cap=6.5)),
    ("m_boulder", N + "stone_largeD", "debris", "slate", (7.0, 9.0), "s", dict(cap=5.0, tilt=0.1)),
    ("m_outcrop", M + "stones", "debris", "slate", (9.0, 11.0), "s", dict(cap=4.5, tilt=0.0)),
    ("m_campfire", N + "campfire_stones", "made", "slate", (13.0, 16.0), "s", dict(tilt=0.0)),
])

_table("beast", [
    ("m_bone", K + "bone_A", "debris", "bone", (7.0, 9.0), "s", dict(sink=(0.0, 0.25))),
    ("m_bone_knob", K + "bone_B", "debris", "bone", (8.0, 10.0), "s", dict(sink=(0.0, 0.25))),
    ("m_long_bone", K + "bone_C", "debris", "bone", (6.0, 7.5), "s", dict(sink=(0.0, 0.25))),
    ("m_skull", K + "skull", "made", "bone", (4.6, 5.6), "b", dict(sink=(0.0, 0.2))),
    ("m_barrel", S + "barrel", "made", "wood", (17.0, 21.0), "lr", dict(sink=(0.15, 0.4))),
    ("m_crate", S + "box-open", "made", "wood", (17.0, 21.0), "s", dict(sink=(0.15, 0.4), cap=5.0)),
    ("m_chest", S + "chest", "made", "wood", (17.0, 21.0), "s", dict(sink=(0.15, 0.4), cap=5.0)),
    ("m_rowboat", P + "boat-row-small", "made", "wood", (3.4, 3.9), "s", dict(sink=(0.2, 0.4))),
    ("m_fish", S + "fish-large", "made", "flesh", (22.0, 27.0), "bf", dict(tilt=0.1)),
])

_table("badlands", [
    ("m_skull", K + "skull", "made", "bone", (4.6, 5.6), "b", dict(sink=(0.0, 0.2))),
    ("m_bone", K + "bone_C", "made", "bone", (6.0, 7.5), "s", dict(sink=(0.0, 0.2))),
    ("m_bone_knob", K + "bone_B", "made", "bone", (8.0, 10.0), "s", dict(sink=(0.0, 0.2))),
    ("m_wagon_wheel", T + "wheel", "made", "wood", (15.0, 19.0), "lr", dict(sink=(0.0, 0.2), tilt=0.1)),
    ("m_cart", T + "cart", "made", "wood", (7.0, 8.2), "s", dict(cap=5.5, tilt=0.08)),
    ("m_barrel", S + "barrel", "made", "wood", (17.0, 21.0), "lr", dict(sink=(0.0, 0.25))),
    ("m_pickaxe", S + "tool-pickaxe", "made", "iron", (30.0, 36.0), "bf", {}),
    ("m_shovel", S + "tool-shovel", "made", "iron", (28.0, 34.0), "bf", {}),
    ("m_campfire", N + "campfire_stones", "made", "rock", (13.0, 16.0), "s", dict(tilt=0.0)),
    ("m_sand_rock", S + "rock-sand-a", "debris", "sandstone", (10.0, 13.0), "s", dict(sink=(0.0, 0.2), cap=5.0)),
    ("m_sand_rocks", S + "rock-sand-b", "debris", "sandstone", (8.0, 10.0), "s", dict(sink=(0.0, 0.2), cap=5.0)),
])
