"""review sheet: python tools/look.py <style,style|all> <out.png> [--size MM] [--seed X] [--n 6] [--theme "name"] [--plan file.json] [--dpi 70]
-> 2D colour layouts of freshly generated bases, one row per style, the theme in each title"""
import sys, os, time
sys.path[0] = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))      # the kit folder, not tools/ (tools/grp.py would hide a standard module)
a = sys.argv[1:]; seed = "look"; cnt = 6
if "--seed" in a: i = a.index("--seed"); seed = a[i + 1]; del a[i:i + 2]
if "--n" in a: i = a.index("--n"); cnt = int(a[i + 1]); del a[i:i + 2]
dpi = 70
if "--dpi" in a: i = a.index("--dpi"); dpi = int(a[i + 1]); del a[i:i + 2]
if "--theme" in a: i = a.index("--theme"); os.environ["THEME"] = a[i + 1]; del a[i:i + 2]
if "--mini" in a: i = a.index("--mini"); os.environ["KIT_MINI"] = a[i + 1]; del a[i:i + 2]
if "--plan" in a: i = a.index("--plan"); os.environ["PLAN_JSON"] = os.path.abspath(a[i + 1]); del a[i:i + 2]
os.environ["SALT"] = seed
import gg3, gg
if "--size" in a: i = a.index("--size"); del a[i:i + 2]
import matplotlib; matplotlib.use("Agg"); import matplotlib.pyplot as plt
styles = list(gg3.TIERS) if a[0] == "all" else a[0].split(",")
fig, axs = plt.subplots(len(styles), cnt, figsize=(5 * cnt, 5.2 * len(styles)), squeeze=False)
for r, st in enumerate(styles):
    for c in range(cnt):
        n = 20 - c % 5 if c < 5 else 15; t = time.time()
        os.environ["RESEED"] = f"c{c}"
        s = gg3.make(st, n)
        try: gg.build_cmds(s)
        except Exception as e: print("  ! clean-up", st, n, e, flush=True)
        gg3.render(s, axs[r, c]); sd = getattr(s, "stand", None)
        if sd:                                           # where the miniature stands: its contact spots (solid line) and body outline (dashed)
            for p in sd["pads"]: axs[r, c].plot(*p.exterior.xy, color="white", lw=1.6, zorder=90); axs[r, c].plot(*p.exterior.xy, color="black", lw=0.5, zorder=91)
            if not sd["body"].is_empty: axs[r, c].plot(*sd["body"].exterior.xy, color="white", lw=0.8, ls="--", zorder=90)
        axs[r, c].set_title(f"{st} {gg.BASE_MM:g} #{c} {getattr(s, 'theme', '')}" + (f"  [{sd['type']}]" if sd else ""), fontsize=11)
        print(st, c, f"{time.time() - t:.1f}s", getattr(s, "theme", ""), flush=True)
plt.tight_layout(); fig.savefig(a[1], dpi=dpi); print("saved", os.path.abspath(a[1]))
