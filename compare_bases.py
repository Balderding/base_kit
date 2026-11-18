"""How different are the bases from each other?

usage:  python compare_bases.py <folder with STLs> [--last 10]
        python compare_bases.py out            (walks all sub-folders, compares bases of the same style)

Method: each base's top is turned into a height map (a grid of heights over the 30 mm disc). Two bases are compared
by the share of the DETAILED area (anything that is not plain flat ground on either base) whose height differs by more
than 0.3 mm, taking the BEST of 72 rotations, because a round base turned by hand is still the same base, and
forgiving shifts of half a millimetre. 0 % = same base, 100 % = no detail in common. Each base is compared with the previous N (default 10) of its style and with all others.
Writes compare_report.csv next to the STLs' folder and prints a summary per style."""
import sys, os, glob, csv, argparse, collections
import numpy as np

N = 56            # grid cells across the base
R = 13.6          # mm: compare the top inside the rim
THR = 0.3         # mm height difference that counts as "different here"


def set_size(mm):
    """set the grid for a base of this diameter (cells stay about 0.5 mm)"""
    global R, N
    R = mm / 2.0 - 1.4; N = int(max(40, min(120, round(2 * R / 0.4945))))      # at most 120 across: big bases use slightly bigger cells


def size_of(path):
    import trimesh
    b = trimesh.load(path).bounds
    return round(float(b[1][0] - b[0][0]) * 2) / 2


def heightmap(path):
    import trimesh
    m = trimesh.load(path); T = m.triangles
    T = T[T[:, :, 2].max(axis=1) > 3.0]                       # only the top side matters
    T = T[(np.abs(T[:, :, 2] - 4.0) > 1e-6).any(axis=1)]      # plain flat ground changes nothing: skip it
    dd = (T[:, 1, 1] - T[:, 2, 1]) * (T[:, 0, 0] - T[:, 2, 0]) + (T[:, 2, 0] - T[:, 1, 0]) * (T[:, 0, 1] - T[:, 2, 1])
    T = T[np.abs(dd) > 1e-12]                                 # upright walls cover no area seen from above
    xs = np.linspace(-R, R, N); H = np.full((N, N), 4.0, dtype=np.float32)
    I0 = np.searchsorted(xs, T[:, :, 0].min(axis=1), "left"); I1 = np.searchsorted(xs, T[:, :, 0].max(axis=1), "right")
    J0 = np.searchsorted(xs, T[:, :, 1].min(axis=1), "left"); J1 = np.searchsorted(xs, T[:, :, 1].max(axis=1), "right")
    ok = (I0 < I1) & (J0 < J1)                                  # triangles with no cell centre under them are skipped unseen
    for (a, b, c), i0, i1, j0, j1 in zip(T[ok], I0[ok], I1[ok], J0[ok], J1[ok]):      # z-buffer: highest surface over each cell centre
        X, Y = np.meshgrid(xs[i0:i1], xs[j0:j1], indexing="ij")
        d = (b[1] - c[1]) * (a[0] - c[0]) + (c[0] - b[0]) * (a[1] - c[1])
        if abs(d) < 1e-12: continue
        w0 = ((b[1] - c[1]) * (X - c[0]) + (c[0] - b[0]) * (Y - c[1])) / d
        w1 = ((c[1] - a[1]) * (X - c[0]) + (a[0] - c[0]) * (Y - c[1])) / d
        w2 = 1 - w0 - w1; inside = (w0 >= -1e-9) & (w1 >= -1e-9) & (w2 >= -1e-9)
        if not inside.any(): continue
        Z = w0 * a[2] + w1 * b[2] + w2 * c[2]
        sub = H[i0:i1, j0:j1]; sub[inside] = np.maximum(sub[inside], Z[inside])
    return H


def rotations(H, steps=36):
    """the height map at `steps` rotations (nearest-neighbour resampling, no extra libraries)"""
    xs = np.linspace(-R, R, N); X, Y = np.meshgrid(xs, xs, indexing="ij"); out = []
    for k in range(steps):
        t = 2 * np.pi * k / steps; c, s = np.cos(t), np.sin(t)
        U = c * X + s * Y; V = -s * X + c * Y
        iu = np.clip(np.rint((U + R) / (2 * R) * (N - 1)).astype(int), 0, N - 1); iv = np.clip(np.rint((V + R) / (2 * R) * (N - 1)).astype(int), 0, N - 1)
        out.append(H[iu, iv])
    return np.array(out, dtype=np.float32)


def score(Ri, Hj, disc):
    """% of the detailed area that differs, best rotation. A cell counts as the same if the other base has that height
    within one cell (0.5 mm) of it, so a hair's shift is not a difference. 'Detailed area' = where either base is not
    plain flat ground, so two sparse bases with different rocks score high, not low."""
    Hj = Hj.astype(np.float32, copy=False); Ri = Ri.astype(np.float32, copy=False)
    d1 = d2 = None
    for dx in (-1, 0, 1):
        for dy in (-1, 0, 1):
            a = np.abs(Ri - np.roll(np.roll(Hj, dx, 0), dy, 1)[None])           # Ri cell vs Hj neighbourhood
            d1 = a if d1 is None else np.minimum(d1, a, out=d1)
            b = np.abs(np.roll(np.roll(Ri, dx, 1), dy, 2) - Hj[None])
            d2 = b if d2 is None else np.minimum(d2, b, out=d2)
    d = np.maximum(d1, d2)[:, disc]
    detail = ((np.abs(Ri - 4.0) > 0.15) | (np.abs(Hj - 4.0) > 0.15)[None])[:, disc]
    frac = ((d > THR) & detail).sum(axis=1) / np.maximum(detail.sum(axis=1), 1)
    k = int(frac.argmin())
    return 100 * frac[k], float(d[k].mean())


def main():
    ap = argparse.ArgumentParser(); ap.add_argument("folder"); ap.add_argument("--last", type=int, default=10)
    a = ap.parse_args()
    files = sorted(f for f in glob.glob(os.path.join(a.folder, "**", "*.stl"), recursive=True) if "failed" not in f.replace("\\", "/").split("/"))
    by = collections.defaultdict(list)
    for f in files: by[os.path.basename(f).rsplit("_", 1)[0]].append(f)
    rows = []; print(f"{len(files)} bases in {len(by)} styles. Difference = % of the detailed area that differs by more than {THR} mm (best of 72 rotations).\n")
    print(f"{'style':12} {'bases':>5} {'vs previous ' + str(a.last) + ' (closest)':>28} {'vs all others (closest)':>26} {'typical pair':>13}  closest pair")
    for st, fs in sorted(by.items()):
        fs.sort(key=lambda f: (os.path.getmtime(f), f))
        set_size(size_of(fs[0])); xs = np.linspace(-R, R, N); X, Y = np.meshgrid(xs, xs, indexing="ij"); disc = X * X + Y * Y <= (R - 0.3) ** 2
        Hs = [heightmap(f) for f in fs]; n = len(fs); D = np.zeros((n, n)); MAD = np.zeros((n, n))
        for i in range(n):
            Ri = rotations(Hs[i], 72)
            for j in range(i + 1, n):
                v, md = score(Ri, Hs[j], disc); D[i, j] = D[j, i] = v; MAD[i, j] = MAD[j, i] = md
        nn_all = [np.delete(D[i], i).min() for i in range(n)] if n > 1 else [0]
        nn_prev = [D[i, max(0, i - a.last):i].min() for i in range(1, n)] or [0]
        iu = np.triu_indices(n, 1); pair = D[iu]; kmin = int(pair.argmin()) if len(pair) else 0
        ci, cj = (iu[0][kmin], iu[1][kmin]) if len(pair) else (0, 0)
        print(f"{st:12} {n:5d} {'min %4.0f%%  median %4.0f%%' % (min(nn_prev), np.median(nn_prev)):>28} {'min %4.0f%%  median %4.0f%%' % (min(nn_all), np.median(nn_all)):>26} {'%4.0f%%' % np.median(pair) if len(pair) else '':>13}"
              f"  {os.path.basename(fs[ci])[:-4]} / {os.path.basename(fs[cj])[:-4]}")
        for i in range(n):
            j = int(np.argsort(D[i])[1]) if n > 1 else i
            rows.append([st, os.path.basename(fs[i]), round(nn_prev[i - 1], 1) if i else "", round(nn_all[i], 1), os.path.basename(fs[j]), round(MAD[i, j], 3)])
    out = os.path.join(a.folder, "compare_report.csv")
    with open(out, "w", newline="") as f:
        w = csv.writer(f); w.writerow(["style", "base", f"closest of previous {a.last} (% different)", "closest of all (% different)", "its closest match", "mean height difference to it (mm)"]); w.writerows(rows)
    print(f"\nPer-base numbers: {out}")


if __name__ == "__main__":
    main()
