"""where each relation puts things: python tools/rel.py <style> <out.png> [--size MM] [--theme "name"] [--seed X]
Builds one scene, then draws 200 sample spots per relation over it (one panel per relation)."""
import sys, os
sys.path[0] = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))      # first: tools/grp.py would otherwise be loaded in place of a standard module
import random
a = sys.argv[1:]; seed = "rel"
if "--seed" in a: i = a.index("--seed"); seed = a[i + 1]; del a[i:i + 2]
if "--theme" in a: i = a.index("--theme"); os.environ["THEME"] = a[i + 1]; del a[i:i + 2]
os.environ["SALT"] = seed; os.environ["KIT_SCENES"] = "1"
import gg3, gg, scenes
if "--size" in a: i = a.index("--size"); del a[i:i + 2]
import matplotlib; matplotlib.use("Agg"); import matplotlib.pyplot as plt
keep = {}
orig = scenes.release
def spy(s):          # sample while the yard is still held, exactly as the props see it
    sc = keep.get("sc")
    if sc is not None and "pts" not in keep:
        rng = random.Random(1); keep["pts"] = {}
        for rel in scenes.RELATIONS:
            at, aim = sc.relation(rel)
            keep["pts"][rel] = [at(rng) for _ in range(200)] if at else []
    orig(s)
scenes.release = spy
_init = scenes.Sc.__init__
def init(self, *x): _init(self, *x); keep["sc"] = self
scenes.Sc.__init__ = init
s = gg3.make(a[0], 16)
fig, axs = plt.subplots(3, 4, figsize=(20, 15.6)); axs = axs.flat
gg3.render(s, axs[0]); axs[0].set_title(f"{a[0]} {gg.BASE_MM:g} mm: {getattr(s, 'theme', '')}", fontsize=12)
for ax, rel in zip(axs[1:], scenes.RELATIONS):
    gg3.render(s, ax); pts = keep.get("pts", {}).get(rel, [])
    if pts: ax.scatter([p[0] for p in pts], [p[1] for p in pts], s=9, c="#00e5ff", zorder=50, edgecolors="black", linewidths=0.3)
    ax.set_title(f"{rel}: {len(pts)} spots", fontsize=12)
plt.tight_layout(); fig.savefig(a[1], dpi=60); print("saved", os.path.abspath(a[1]))
