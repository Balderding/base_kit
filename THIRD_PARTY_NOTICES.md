# Third-party notices

`bbb_kit.exe` is one self-contained program. Besides the kit's own code it holds, or was written to work exactly like,
the parts below. Their licence texts are in the `licenses` folder beside this file and in the release zip.

| Part | What it is in the kit | Licence | Text |
|---|---|---|---|
| GEOS 3.14.1 (libgeos.org) | the geometry library, linked into the exe | GNU LGPL 2.1 | `licenses/GEOS-LGPL-2.1.txt` |
| Manifold 3.5.4 (github.com/elalish/manifold) | joins the objects into the base as one solid (supports on, "Join into one solid"), linked into the exe | Apache 2.0 | `licenses/Manifold-Apache-2.0.txt` |
| Clipper2 (github.com/AngusJohnson/Clipper2, commit 46f6391) | part of Manifold, linked into the exe | Boost Software License 1.0 | `licenses/Clipper2-BSL-1.0.txt` |
| oneTBB 2022.3.0 (github.com/uxlfoundation/oneTBB) | lets Manifold join a base on several cores, linked into the exe | Apache 2.0 | `licenses/oneTBB-Apache-2.0.txt` |
| DejaVu Sans glyphs | the small font of the report sheets (pixels made from the font) | Bitstream Vera / DejaVu licence | `licenses/DejaVu-fonts.txt` |
| Silkscreen (The Silkscreen Project Authors) | the window's pixel font (pixels made from the font) | SIL Open Font License 1.1 | `docs/fonts/OFL.txt` |
| mingw-w64 winpthreads | thread support, linked into the exe | MIT | `licenses/winpthreads.txt` |
| Mersenne Twister (as in CPython) | the random numbers, so the same seed gives the same base as the Python kit | BSD 3-clause | `licenses/mt19937.txt` |
| CPython | parts of its maths, number formatting and set order, written again in C to give the same numbers | PSF licence | `licenses/CPython-PSF.txt` |
| Pillow 13.0 | the 2D drawing's polygon, line and ellipse filling and its resizing, written again in C from libImaging | HPND (Pillow) | `licenses/Pillow.txt` |
| trimesh 5.2 | the mesh clean-up (merging, splitting, filling holes) written again in C | MIT | `licenses/trimesh.txt` |
| networkx 3.7 | the cycle and part finding the clean-up uses, written again in C | BSD 3-clause | `licenses/networkx.txt` |
| shapely 2.2 | how the kit calls GEOS, written again in C | BSD 3-clause | `licenses/shapely.txt` |
| numpy 2.5 | the number steps (spacing, sums) written again in C to give the same numbers | BSD 3-clause | `licenses/numpy.txt` |
| 3D models | the models inside the exe | public domain (CC0) | none needed |

## GEOS and the LGPL

GEOS is linked into `bbb_kit.exe` as a static library. The LGPL lets you change GEOS and use your changed version
with the kit. Everything you need to do that is public:
- the kit's whole source, in this repository (`cport\`, `gui\`);
- the GEOS 3.14.1 source from libgeos.org, and the small change the kit makes before building it
  (`cport\geos_static\patch_geos.py`, `cport\geos_static\msvc_stl.h`);
- the build steps: `cport\geos_static\build_geos.cmd` builds GEOS, then `gui\build.cmd` links the exe with it.
