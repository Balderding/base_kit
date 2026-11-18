"""the theme list as a table: python tools/theme_table.py            prints it
                                python tools/theme_table.py --readme   rewrites the table between the markers in README.md
Made from themes.py, so the README never lists a theme the kit does not have."""
import sys, os
KIT = os.path.dirname(os.path.dirname(os.path.abspath(__file__))); sys.path[0] = KIT
import themes
SUBJ = {"wall": "ruined wall", "dais_brazier": "platform with a brazier", "dais_column": "platform with a column", "dais_altar": "platform with an altar", "skeleton": "giant skeleton",
        "crater": "crater", "colonnade": "colonnade", "hull": "wreck", "tree": "great stump", "circle": "ritual circle", "ruin": "ruined building", "pool": "pool", "nest": "great nest",
        "spires": "spires", "tentacles": "tentacle pit", "portal": "doorway on the rim", "shaft": "shaft with windlass", "camp": "camp fire and bedrolls", "depot": "stacked crates and barrels",
        "graves": "row of graves", "wagon": "broken wagon", "corral": "fenced ring", "machine": "machine", "emplacement": "sandbagged gun pit", "tent": "tent", "log": "fallen giant tree",
        "well": "well", "statue": "plinth and fallen statue", "organ": "great organ with veins", "bridge": "stream with a crossing"}
PATH = {"ruts": "wagon ruts", "trail": "footpath", "track": "rail track", "boards": "boardwalk", "stream": "stream", "stones": "stepping stones", "way": "paved way", "pipe": "pipe run",
        "cable": "cables", "veins": "vein or root", "none": "none"}
rows = ["| Style | Theme | Subject | Path | Belongs to it |", "|---|---|---|---|---|"]
for st, ths in themes.THEMES.items():
    for th in ths:
        k, _, r = th["path"].partition(":"); props = []
        for w in th["props"].split():
            what = w.split(":")[1]; name, _, c = what.partition("*")
            if name not in props: props.append(name)
        rows.append(f"| `{st}` | {th['theme']} | {SUBJ.get(th['subject'], th['subject'])} | {PATH.get(k, k)}" + (f", {'to it' if r == 'to' else 'past it'}" if k != "none" and th["subject"] != "bridge" else "") + f" | {', '.join(props)} |")
table = "\n".join(rows)
if "--readme" in sys.argv:
    p = os.path.join(KIT, "README.md"); s = open(p, encoding="utf-8").read(); a, b = "<!-- themes:start -->", "<!-- themes:end -->"
    if a not in s or b not in s: raise SystemExit("README.md has no theme table markers")
    s = s[:s.index(a) + len(a)] + "\n" + table + "\n" + s[s.index(b):]
    open(p, "w", encoding="utf-8", newline="\n").write(s); print(f"README.md: {len(rows) - 2} themes written")
else: print(table)
