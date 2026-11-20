"""Themes: 5 named places per style, one level below the style (a mine base is a "tunnel mouth", not just "mine").
Each line is a plan in short; scenes.pick_plan() expands it to the full plan dict (see PLAN_SCENES.md, A1).

  T(name, subject, path, props, also="", pal={...}, clutter={...}, open=1.0, ban=(...))
  subject  a key of scenes.SUBJECTS ("portal" always sits on the rim; "bridge" is a stream with a crossing)
  path     "<kind>:to" (rim to the subject), "<kind>:past" (rim to rim in front of it) or "none"
  props    "<where>:<what>[*count]" ...   where = a relation in scenes.RELATIONS, what = a key of scenes.PROPS
  also     "<subject>:<where>" ...        smaller second features on bases of 100 mm and up
  pal      material roles changed for this theme only (pit="lava": the crater is a lava vent)
  clutter  debris / growth / marks density against the default 1.0;  ban = detail kinds that would contradict it
"""

def T(name, subject, path, props, also="", **kw):
    return dict(theme=name, subject=subject, path=path, props=props, also=also, **kw)

THEMES = {
"mine": [
    T("tunnel mouth", "portal", "track:to", "path:cart wall:timbers*2 path:pick path:shovel yard:orepile front:lantern back:rubble*2", also="depot:path"),
    T("shaft head", "shaft", "trail:to", "front:ladder at:keg back:crates at:rope wall:timbers yard:orepile", also="camp:far"),
    T("blast site", "crater", "cable:to", "start:crate at:holes*2 at:rubble*3 path:pick far:keg", clutter=dict(debris=2.0), also="depot:far"),
    T("ore siding", "depot", "track:past", "path:cart path:orepile*2 path:shovel wall:barrels start:beam", also="shaft:far"),
    T("miners' camp", "camp", "trail:to", "at:crates at:keg at:pick at:shovel at:lantern back:timbers", also="shaft:far"),
],
"swamp": [
    T("sunken shrine", "ruin", "stones:to", "at:drum*2 in:runes back:shrubs*2 front:skull at:stub", ban=("crate", "iron")),
    T("boat landing", "hull", "boards:to", "path:posts at:rope path:crate at:spear back:shrubs*2", also="camp:far"),
    T("hanging tree", "tree", "veins:past", "at:fairy wall:bones at:stump at:skull", pal=dict(vein="root"), ban=("crate", "iron")),
    T("serpent pool", "pool", "stream:to", "at:eggs*2 at:nest at:tentacle back:shrubs*2", ban=("crate", "iron")),
    T("hunters' hide", "camp", "boards:to", "at:spear back:carcass path:posts at:sacks", also="hull:far"),
],
"cynder": [
    T("lava vent", "crater", "stream:to", "at:crystals*2 at:rubble*2 far:crack", pal=dict(pit="lava"), clutter=dict(debris=1.5, growth=0.0), ban=("crate", "wood")),
    T("forge altar", "dais_brazier", "way:to", "front:slab at:chain wall:pick at:rubble*2 path:brazier"),
    T("obsidian field", "spires", "trail:to", "path:pick path:crate path:crystals*2 at:crystals"),
    T("ash burial", "skeleton", "none", "at:ribs front:runes at:brazier*2", ban=("crate", "wood", "iron")),
    T("rail to the deep", "portal", "track:to", "path:cart wall:timbers front:brazier path:pick"),
],
"targa": [
    T("frozen gate", "portal", "way:to", "front:stub*2 wall:crystals*2 back:boulder", ban=("crate",)),
    T("power core", "machine", "pipe:to", "at:vent*2 at:cables at:grate back:crystals"),
    T("crashed pod", "hull", "trail:to", "at:tiles*2 wall:crystals*2 path:slab"),
    T("ice garden", "spires", "stones:to", "front:runes at:boulder*2 at:crystals", ban=("crate",)),
    T("hall of pillars", "colonnade", "way:past", "at:drum at:slab*2 back:crystals", ban=("crate",)),
],
"trederra": [
    T("gun pit", "emplacement", "boards:to", "at:casings*2 back:crates at:sandbags back:posts*2"),
    T("shell hole", "crater", "trail:past", "path:rifle at:lid back:posts front:ladder", pal=dict(pit="water"), clutter=dict(debris=1.5)),
    T("dugout entrance", "portal", "boards:to", "wall:sandbags*2 path:sign wall:crates front:lantern"),
    T("supply dump", "depot", "ruts:to", "path:cart wall:barrels wall:oildrum*2 at:shovel at:crates at:sacks", also="tent:far"),
    T("field graves", "graves", "trail:to", "at:rifle*2 back:posts path:shovel", ban=("crate",)),
],
"fortress": [
    T("garden shrine", "dais_altar", "stones:to", "path:lantern*2 back:boulder*2 far:puddle", ban=("crate",)),
    T("gatehouse", "portal", "way:to", "wall:spear*2 front:brazier path:posts"),
    T("koi pond", "pool", "stones:past", "at:lantern at:boulder*2 at:planks", ban=("crate", "iron")),
    T("armoury", "depot", "way:to", "at:posts wall:chest at:lid wall:sword wall:spear"),
    T("dojo floor", "ruin", "way:to", "wall:sword wall:spear in:brazier in:urn*2", pal=dict(floor="tatami", stone="wood")),
],
"beast": [
    T("feeding pit", "tentacles", "veins:past", "at:bones*2 at:carcass at:teeth", pal=dict(pit="bile")),
    T("the heart", "organ", "veins:to", "at:pustules*2 at:eggs at:puddle*2"),
    T("swallowed wreck", "hull", "stream:past", "at:chest at:anchor at:crate back:pustules", pal=dict(wood="wood")),
    T("brood nest", "nest", "veins:to", "at:eggs*3 at:pustules*2", pal=dict(twig="flesh")),
    T("jaw line", "wall", "none", "wall:teeth*3 back:pustules*2 front:puddle", pal=dict(stone="tooth")),
],
"badlands": [
    T("dry camp", "camp", "trail:to", "at:sacks at:rifle at:wheel at:keg"),
    T("boot hill", "graves", "trail:to", "at:shovel back:posts start:sign", ban=("crate",)),
    T("broken wagon", "wagon", "ruts:past", "at:wheel path:crate*2 path:barrel far:bones"),
    T("old diggings", "shaft", "track:to", "path:pick yard:orepile path:cart wall:timbers"),
    T("carcass in the wash", "skeleton", "stream:past", "at:skull in:nest path:shrubs*2", pal=dict(water="wash"), ban=("crate", "wood")),
],
"highland": [
    T("standing stones", "circle", "trail:to", "at:drum at:fairy far:shrubs*2", pal=dict(stone="standing"), ban=("crate",)),
    T("sheepfold", "corral", "trail:to", "in:keg start:posts path:cart", pal=dict(wood="slate")),
    T("ruined tower", "ruin", "stones:to", "at:rubble*3 front:sword in:campfire"),
    T("burn crossing", "bridge", "stones:past", "path:shrubs*2 at:boulder*2 start:boulder", ban=("crate",)),
    T("hill camp", "camp", "trail:to", "at:axe at:stump at:logpile at:lid"),
],
"maahl": [
    T("crystal well", "spires", "stones:to", "front:runes path:crystals*2 path:spear", ban=("crate",)),
    T("buried temple", "colonnade", "way:past", "at:drum*2 back:slab at:stub at:pot", ban=("crate",)),
    T("altar of sand", "dais_altar", "way:to", "path:brazier*2 front:bones at:urn*2 at:tiles*2", ban=("crate",)),
    T("bones of the giant", "skeleton", "none", "in:crystals*2 at:pothole at:shovel"),
    T("ruin wall", "wall", "trail:past", "wall:rubble*3 back:shrubs*2 front:steps", ban=("crate",)),
],
"corridor": [
    T("airlock", "portal", "way:to", "path:crates wall:crate front:stub at:cables"),
    T("reactor bay", "machine", "pipe:to", "at:grate*2 at:vent at:wrench at:puddle back:tank"),
    T("cargo hold", "depot", "track:past", "path:cart at:crates*2 at:ladder wall:oildrum*2 wall:tank"),
    T("breach", "crater", "cable:to", "at:tiles*2 at:crack*2 at:slab*2", pal=dict(pit="slot"), clutter=dict(debris=1.5)),
    T("maintenance pit", "shaft", "pipe:past", "front:ladder at:wrench at:gear at:grate"),
],
"infestation": [
    T("hive mouth", "tentacles", "veins:to", "at:eggs*2 at:pustules*2 path:rifle", pal=dict(pit="acid")),
    T("egg chamber", "nest", "veins:past", "at:eggs*2 path:eggs*2 far:puddle*2", pal=dict(twig="flesh")),
    T("overrun cargo", "depot", "veins:to", "at:crates at:tentacle*2 front:skull"),
    T("acid pool", "pool", "stream:to", "at:bones*2 at:crystals at:pustules"),
    T("cocooned wreck", "hull", "cable:to", "wall:pustules*2 at:ribs in:eggs*2", pal=dict(wood="carapace")),
],
"urban": [
    T("bomb site", "crater", "cable:past", "at:slab*2 at:rubble*3 path:sign", pal=dict(pit="pothole"), clutter=dict(debris=2.0)),
    T("barricade", "wall", "none", "wall:sandbags*2 back:oildrum*2 back:casings*2 wall:crates"),
    T("tram stop", "depot", "track:past", "path:sign path:planks at:grate at:wheel"),
    T("collapsed shopfront", "ruin", "way:past", "front:rubble*3 path:beam front:tiles", pal=dict(stone="brick")),
    T("open manhole", "shaft", "pipe:to", "at:lid front:ladder at:posts at:wrench", pal=dict(wood="iron")),
],
"frontier": [
    T("water stop", "well", "ruts:past", "at:keg at:posts path:prints*2 at:barrel"),
    T("broken wagon", "wagon", "ruts:past", "at:wheel path:crate*2 path:barrel at:rifle"),
    T("corral", "corral", "trail:to", "in:keg at:sacks start:posts at:rope"),
    T("storefront", "portal", "boards:past", "wall:barrels wall:sacks wall:crates path:sign wall:planks", pal=dict(stone="board")),
    T("prospector's camp", "camp", "trail:to", "at:pick at:lid at:sacks at:shovel", also="tent:at"),
],
"graveyard": [
    T("fresh grave", "graves", "trail:to", "at:shovel at:coffin at:lantern", ban=("crate",)),
    T("crypt door", "portal", "way:to", "front:urn*2 path:posts*2 wall:stub"),
    T("fenced plot", "corral", "stones:to", "in:headstone*2 start:posts far:stump", pal=dict(wood="fence")),
    T("hanging tree", "tree", "trail:to", "at:coffin at:rope wall:fairy", pal=dict(wood="wood")),
    T("robbed tomb", "dais_altar", "trail:to", "front:slab path:bones*2 at:pot at:spear"),
],
"forest": [
    T("fallen giant", "log", "trail:past", "wall:fairy*2 wall:shrubs*2 back:bush", ban=("crate", "iron")),
    T("woodcutters' clearing", "camp", "trail:to", "at:axe at:logpile*2 at:log at:stump*2 far:stump"),
    T("stream ford", "bridge", "stones:past", "path:shrubs*2 at:boulder*2 far:prints", ban=("crate", "iron")),
    T("old shrine", "circle", "stones:to", "in:lid at:fairy far:fairy wall:shrubs", ban=("crate",)),
    T("bear den", "portal", "trail:to", "front:bones*2 path:crack*2 front:carcass", pal=dict(stone="rock"), ban=("crate", "iron")),
],
"dungeon": [
    T("cell door", "portal", "way:to", "at:chain*2 front:keg wall:bones*2 path:lid"),
    T("pit trap", "shaft", "none", "at:trapdoor at:spear wall:bones far:puddle", pal=dict(wood="flag")),
    T("torture chamber", "dais_altar", "way:to", "at:brazier at:chain*2 at:sword path:grate"),
    T("treasure hoard", "depot", "way:to", "at:coins*3 at:sword at:lid front:skull wall:chest at:urn"),
    T("summoning circle", "circle", "way:to", "at:brazier*3 front:stub in:runes", ban=("crate",)),
],
"temple": [
    T("high altar", "dais_altar", "way:to", "path:brazier*2 front:steps at:urn*2", clutter=dict(growth=0.2), ban=("crate",)),
    T("fallen idol", "statue", "way:to", "path:slab*2 at:coins at:drum", ban=("crate",)),
    T("processional way", "colonnade", "way:past", "path:drum path:brazier*2", ban=("crate",)),
    T("sacred pool", "pool", "stones:to", "at:steps at:stub*4 at:shrubs", ban=("crate",)),
    T("sealed vault", "portal", "way:to", "front:slab path:pick front:carcass wall:runes"),
],
"coast": [
    T("shipwreck", "hull", "trail:to", "at:chest at:anchor at:barrel*2 at:rope"),
    T("jetty", "portal", "boards:to", "path:posts*2 path:rope path:crates at:anchor", pal=dict(stone="drift")),
    T("tide pool", "pool", "stream:to", "at:pebbles*2 at:shrubs*2 at:nest", ban=("crate", "iron")),
    T("beach camp", "camp", "trail:to", "at:timbers at:posts at:anchor at:barrel at:sacks"),
    T("whale bones", "skeleton", "none", "wall:shrubs*2 back:pebbles*2 at:posts", ban=("crate", "iron")),
],
}
__import__("newstyles").hook_themes(THEMES)          # the styles kept one per file (style_<name>.py)
