"""Run Python's mesher.mesh_from_field on a saved proof input (.min), as the C port does, and compare with the .ref.
Proves the .min holds everything the mesher needs, and with MESH_STAGES=1 prints the same stage lines as the C side.
    python cport/replay_mesh.py <file.min>"""
import os, sys, struct
KIT = os.path.dirname(os.path.dirname(os.path.abspath(__file__))); sys.path.insert(0, KIT)
import numpy as np, shapely
import mesher

def read(path):
    b = open(path, "rb").read(); p = [4]
    def d(): v = struct.unpack_from("<d", b, p[0])[0]; p[0] += 8; return v
    def u(): v = struct.unpack_from("<I", b, p[0])[0]; p[0] += 4; return v
    def by(): v = b[p[0]]; p[0] += 1; return v
    def g():
        n = u(); x = shapely.from_wkb(b[p[0]:p[0] + n]); p[0] += n; pr = d()
        return shapely.set_precision(x, pr, mode="pointwise") if pr > 0 else x          # the grid back, the geometry unchanged
    assert b[:4] == b"BBM1"
    R0, H, rim_tan = d(), d(), d(); mag = None
    if by(): rm, dm, n = d(), d(), u(); mag = (rm, dm, [(d(), d()) for _ in range(n)])
    disc = g(); ov = None
    if by():
        n = u(); TR = [(d(), d()) for _ in range(n)]; m = u(); OU = [(d(), d()) for _ in range(m)]; ov = (None, None, TR, OU)
    nc = u(); cells = []; rules = []
    for _ in range(nc):
        cp = g(); k = by()
        if k == 0: r = mesher.Flat(d())
        elif k == 1:
            r = mesher.Step(d())
            if by(): r.poly = g()
            hz = by(); zb = d()
            if hz: r.zb = zb
        elif k == 2: poly = g(); z0, tan, zmax = d(), d(), d(); r = mesher.Slope(poly, z0, tan, zmax)
        else:
            poly = g(); inner = g(); lo, hi, w = d(), d(), d(); r = object.__new__(mesher.Band)
            r.poly = poly; r.outer = poly.boundary; r.inner = inner; r.lo = lo; r.hi = hi; r.w = w
        cells.append(cp); rules.append(r)
    ni = u(); idx = list(struct.unpack_from(f"<{ni}I", b, p[0]))
    first = {}
    for i, j in enumerate(idx): first.setdefault(j, rules[i])
    F = object.__new__(mesher.Field); F.disc = disc; F.cells = [(cells[i], first[idx[i]]) for i in range(nc)]; F.chains = {}
    if ov: F.oval = ov
    return F, R0, H, rim_tan, mag

if __name__ == "__main__":
    path = sys.argv[1]; F, R0, H, rim_tan, mag = read(path)
    v, f = mesher.mesh_from_field(F, R0, H, rim_tan, mag)
    b = open(path[:-4] + ".ref", "rb").read(); nv, nt = struct.unpack_from("<II", b, 4)
    rv = np.frombuffer(b, "<f8", 3 * nv, 12).reshape(-1, 3); rt = np.frombuffer(b, "<i4", 3 * nt, 12 + 24 * nv).reshape(-1, 3)
    same = v.shape == rv.shape and f.shape == rt.shape and np.array_equal(v, rv) and np.array_equal(f, rt)
    print(("SAME" if same else "DIFF"), os.path.basename(path), len(v), "verts", len(f), "tris")
