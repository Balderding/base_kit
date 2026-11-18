"""the drawing beside the finished base: python tools/make_compare.py <folder with built bases> <out.png> [how many, default 4]
For each STL in the folder that has its _2d.png: one row with the 2D drawing the kit laid out, the finished STL seen
from straight above (same way up as the drawing, so the two can be checked against each other), and the STL seen from
the side. Used for the picture in the README; works on any out\\picked_... or out\\run_...\\<style> folder."""
import sys, os, glob
KIT = os.path.dirname(os.path.dirname(os.path.abspath(__file__))); sys.path[0] = KIT
src, out = sys.argv[1], sys.argv[2]; many = int(sys.argv[3]) if len(sys.argv) > 3 else 4
import mesher, trimesh
import matplotlib; matplotlib.use("Agg"); import matplotlib.pyplot as plt, matplotlib.image as mpimg
def above(m, png):
    """the STL from straight above, lit from the upper left at a low angle and a little lighter the higher it is, so
    raised things read at a glance (straight-down light would show every flat top the same grey)"""
    import numpy as np
    from matplotlib.collections import PolyCollection
    T = np.asarray(m.triangles); N = np.cross(T[:, 1] - T[:, 0], T[:, 2] - T[:, 0]); ln = np.linalg.norm(N, axis=1); k = (ln > 1e-12) & (N[:, 2] > 1e-9); T = T[k]; N = N[k] / ln[k][:, None]
    L = np.array([-0.62, 0.55, 0.56]); L /= np.linalg.norm(L); z = T[:, :, 2].max(axis=1); zr = max(float(z.max()) - 4.0, 0.5)
    g = np.clip(0.18 + 0.62 * np.clip(N @ L, 0, 1) + 0.30 * np.clip((z - 4.0) / zr, 0, 1), 0, 1); o = np.argsort(z); col = np.repeat(g[o][:, None], 3, axis=1)
    f, ax = plt.subplots(figsize=(6, 6), dpi=150); ax.add_collection(PolyCollection(T[o][:, :, :2], facecolors=col, edgecolors=col, linewidths=0.15))
    r = float(np.abs(T[:, :, :2]).max()) * 1.03; ax.set_xlim(-r, r); ax.set_ylim(-r, r); ax.set_aspect("equal"); ax.axis("off"); f.subplots_adjust(0, 0, 1, 1); f.savefig(png, dpi=150, facecolor="white"); plt.close(f)

stls = [f for f in sorted(glob.glob(os.path.join(src, "*.stl"))) if os.path.exists(f[:-4] + "_2d.png")][:many]
if not stls: sys.exit("no STL with a _2d.png beside it in " + src)
tmp = os.path.join(KIT, "out", "compare_tmp"); os.makedirs(tmp, exist_ok=True)
fig, axs = plt.subplots(len(stls), 3, figsize=(13.5, 4.6 * len(stls)), squeeze=False)
for r, stl in enumerate(stls):
    m = trimesh.load(stl, process=False); name = os.path.basename(stl)[:-4]
    top = os.path.join(tmp, name + "_top.png"); side = os.path.join(tmp, name + "_side.png")
    above(m, top); mesher.shot(m, side, 1100, 900, 38, -62)
    for c, (png, title) in enumerate(((stl[:-4] + "_2d.png", "the 2D drawing"), (top, "the finished STL from above"), (side, "the finished STL from the side"))):
        ax = axs[r, c]; ax.imshow(mpimg.imread(png), cmap="gray"); ax.axis("off")
        ax.set_title(title if r == 0 else "", fontsize=15, color="#14213d", pad=10)
    axs[r, 0].text(0.0, -0.04, name, transform=axs[r, 0].transAxes, fontsize=10, color="#444444")
plt.tight_layout(); fig.savefig(out, dpi=70, facecolor="white"); print("saved", os.path.abspath(out))
