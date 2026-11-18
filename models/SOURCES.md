# Models used for base details

Only public-domain (Creative Commons Zero, CC0) models go in this folder. CC0 means anyone may use them for any
purpose. Each pack keeps its own `License.txt`.

The packs come from three makers. `packs.json` lists every pack with its page; the ones marked `"used": true` are
the ones the kit has details made from (25 packs, about 92 MB; `tools\used_packs.py` sets the marks from the
details themselves, and the README lists them).

| Maker | What | Licence, as stated | How the kit fetches it |
|---|---|---|---|
| Kenney, www.kenney.nl | 50 packs listed, 20 in use | `License.txt` in every pack: Creative Commons Zero, CC0 | the pack's own page gives a download link; the zip is unpacked to `models\<pack>` |
| Kay Lousberg (KayKit), kaylousberg.com | 4 packs in use: dungeon, Halloween, space base, city | `LICENSE.txt` in every repository: "License: (Creative Commons Zero, CC0)" | the repository's zip from github.com/KayKit-Game-Assets; only its OBJ folder is unpacked, to `models\<pack>\obj` |
| The Base Mesh, thebasemesh.com | 67 single models in use, out of about 900 | the site: "100% Free. CC0 License."; the GLB copies' repository carries a CC0 1.0 LICENSE | single files from github.com/M3-org/base-meshes, to `models\base-meshes\glb`; the repository's licence page is read first and nothing is fetched if it no longer says CC0 |

The kit fetches the used packs on its first run and says so before it does (`tools\get_models.py`, README section
"The model packs"). `tools\get_models.py --all` fetches the rest for trying more.

`_dl\` holds the zip files as downloaded. `_more\` (made by `tools\get_more_models.py`) holds further packs pulled
down to look at; the kit does not use them. Credit is not required by CC0; the makers ask for it as a courtesy.
The kit reads `.obj` and `.glb` files (see `sd_mesh.py`, `sd_r_models*.py`).

## Other public-domain sources (not fetched yet)

Checked on 2026-11-17; check the licence file inside each download again before a pack goes in.

| Source | What it has | Licence, as the author states it |
|---|---|---|
| Quaternius, https://quaternius.com | 70+ packs: fantasy props, medieval village, sci-fi, nature, animals, ships, modular dungeon | "All models are under the CC0 License" (quaternius.com/faq.html) |
| KayKit by Kay Lousberg, https://kaylousberg.itch.io and https://github.com/KayKit-Game-Assets | dungeon (200+ props), adventurers, skeletons, resource and prototype bits | CC0 1.0 on each pack page and in the LICENSE.txt of the GitHub copies; the paid "Extra" tiers were not checked |
| OpenGameArt, https://opengameart.org | everything, by many authors | per item: take only items marked CC0 |
| Poly Pizza, https://poly.pizza | a catalogue of single models | per model: mostly CC-BY (credit required), some CC0; take only CC0 |
| Poly Haven, https://polyhaven.com | scanned props and rocks, high detail (not low-poly) | CC0 |
