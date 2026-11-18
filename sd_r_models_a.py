"""Details made from ready-made public-domain models for the dungeon, the fortress, the temple and the mine (see
sd_r_models.py for the helper and sd_mesh.py for how a model becomes a solid). One line of a table is one detail:
(name, model file, kind, mm per model unit, poses, more). Tall thin things (a spear, a ladder, a pick) are offered
lying down only: the surface cannot overhang, and lying they come out as a clean relief."""
from sd_r_models import _model

K = "kaykit-dungeon/obj/"
def P(pack): return pack + "/Models/OBJ format/"
MD, AR, SV, PI, RF, FT, TD, CV, GY = P("mini-dungeon"), P("mini-arena"), P("survival-kit"), P("pirate-kit"), P("retro-fantasy-kit"), P("fantasy-town-kit"), P("tower-defense-kit"), P("modular-cave-kit"), P("graveyard-kit")

def _table(style, rows):
    for name, rel, kind, mm, poses, kw in rows:
        kw = dict(kw)
        if "with_" in kw: kw["with_"] = [(m + ".obj", k) for m, k in kw["with_"]]
        _model(style, name, rel + ".obj", kind, mm, poses, **kw)

_table("dungeon", [
    ("m_chest_gold", K + "chest_gold", "wood", (2.6, 3.1), "s", {}),
    ("m_chest", MD + "chest", "wood", (6.5, 8.0), "s", {}),
    ("m_trunk", K + "trunk_large_A", "wood", (3.0, 3.8), "s", {}),
    ("m_barrel", K + "barrel_small", "wood", (3.0, 3.6), "ssl", {}),
    ("m_coins", K + "coin_stack_medium", "gold", (4.0, 5.0), "s", dict(with_=[(K + "coin", 1.0)])),
    ("m_key", K + "key", "iron", (5.0, 6.5), "bf", {}),
    ("m_keyring", K + "keyring", "iron", (4.5, 5.5), "s", {}),
    ("m_bottles", K + "bottle_A_brown", "glass", (3.4, 4.0), "ssl", dict(with_=[(K + "bottle_B_green", 1.0), (K + "bottle_C_brown", 1.0)])),
    ("m_trap", MD + "trap", "iron", (6.0, 7.5), "s", {}),
    ("m_pot", MD + "pot", "brass", (7.0, 8.5), "s", {}),
    ("m_shield", MD + "shield-round", "iron", (9.0, 11.0), "bf", {}),
    ("m_sword", MD + "weapon-sword", "iron", (12.0, 14.0), "bf", {}),
    ("m_potion", MD + "potion", "glass", (7.5, 9.0), "s", {}),
])

_table("fortress", [
    ("m_spear", AR + "weapon-spear", "iron", (9.0, 10.5), "bf", {}),
    ("m_bolt", TD + "weapon-ammo-arrow", "iron", (8.0, 10.0), "s", {}),
    ("m_barrel", PI + "barrel", "wood", (2.6, 3.1), "ssl", {}),
    ("m_barrels", RF + "barrels", "wood", (6.0, 7.0), "s", {}),
    ("m_bedroll", SV + "bedroll", "sack", (8.0, 10.0), "s", {}),
    ("m_chest", SV + "chest", "wood", (11.0, 13.0), "s", {}),
    ("m_pot", MD + "pot", "clay", (7.0, 8.5), "s", {}),
])

_table("temple", [
    ("m_column_stub", AR + "column-damaged", "tcolumn", (5.0, 6.0), "s", {}),
    ("m_pillar_fallen", K + "pillar", "tcolumn", (1.2, 1.45), "bl", {}),
    ("m_statue", AR + "statue", "tcolumn", (3.4, 4.0), "s", {}),
    ("m_gold_chest", K + "chest_gold", "gold", (2.6, 3.1), "s", {}),
    ("m_pot", MD + "pot", "tcolumn", (7.0, 8.5), "s", {}),
    ("m_steps", AR + "stairs", "tflag", (3.5, 4.2), "s", {}),
    ("m_blocks", AR + "bricks", "tcolumn", (5.0, 6.0), "s", {}),
    ("m_brazier", GY + "fire-basket", "gold", (7.0, 8.5), "s", {}),
    ("m_altar", GY + "altar-stone", "tcolumn", (5.0, 6.0), "s", {}),
    ("m_urn", GY + "urn-round", "tcolumn", (7.5, 9.0), "s", {}),
    ("m_obelisk", GY + "pillar-obelisk", "tcolumn", (4.0, 5.0), "bl", {}),
])

_table("mine", [
    ("m_pickaxe", SV + "tool-pickaxe", "iron", (22.0, 26.0), "bf", {}),
    ("m_shovel", PI + "tool-shovel", "iron", (3.0, 3.6), "bf", {}),
    ("m_hammer", SV + "tool-hammer-upgraded", "iron", (22.0, 26.0), "bf", {}),
    ("m_barrel", PI + "barrel", "wood", (2.6, 3.1), "ssl", {}),
    ("m_rocks", MD + "stones", "rock", (5.5, 6.5), "s", dict(cls="debris")),
    ("m_ore", SV + "resource-stone-large", "rock", (12.0, 15.0), "s", dict(cls="debris", with_=[(SV + "resource-stone", 1.0)])),
    ("m_crystals", TD + "detail-crystal-large", "rock", (6.0, 7.0), "s", dict(cls="debris")),
    ("m_ladder", CV + "ladder", "wood", (2.0, 2.4), "bf", {}),
    ("m_wheel", FT + "wheel", "wood", (8.0, 9.5), "lr", {}),
])
