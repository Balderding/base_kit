"""Proof data for the C mesher: builds bases with the normal Python builder, and for each one saves what
mesh_from_field was given and what it returned.

    python cport/dump_mesh.py <out dir> <style> <n> [<style> <n> ...]      (run with PYTHONHASHSEED=0)

The runner's own environment is needed (KIT_SCALE, KIT_FINAL_MM, BASE_MM, SALT ...): set it before calling.
Each base gives <name>.min (the input, a little binary file the C side reads) and <name>.ref (Python's answer:
the vertices and triangles, binary). Format, all little-endian:
  .min : "BBM1", f64 R0, H, rim_tan; u8 has_mag [f64 rm, dm; u32 n; n x (f64 cx, cy)];
         geom disc; u8 has_oval [u32 n; n x f64 2 (TR); u32 m; m x f64 2 (outer)];
         u32 ncells; per cell: geom poly; u8 kind (0 Flat, 1 Step, 2 Slope, 3 Band); f64 params; extra geoms
            Flat/Step: f64 z  (Step: also u8 has_poly [geom poly], f64 zb if present)
            Slope: geom poly, f64 z0, tan, zmax        Band: geom poly, geom inner boundary, f64 lo, hi, w
         u32 nrules_shared: index of the rule each cell uses (cells sharing one rule object share its index)
  geom = u32 len + WKB bytes (shapely.to_wkb, exact doubles) + f64 precision grid (shapely.get_precision; 0 = none)
  .ref : "BBR1", u32 nv, nt; nv x 3 f64; nt x 3 i32
"""
import os, sys, struct
KIT = os.path.dirname(os.path.dirname(os.path.abspath(__file__))); sys.path.insert(0, KIT); os.chdir(KIT)
import numpy as np, shapely
import mesher

OUT = sys.argv[1]; os.makedirs(OUT, exist_ok=True)
jobs = list(zip(sys.argv[2::2], sys.argv[3::2]))
_orig = mesher.mesh_from_field
CUR = [""]

def geom(g):
    b = shapely.to_wkb(g, hex=False, output_dimension=2, byte_order=1, include_srid=False)
    return struct.pack("<I", len(b)) + b + struct.pack("<d", float(shapely.get_precision(g)))          # WKB loses the precision grid: it is kept beside it

def dump_in(F, R0, H, rim_tan, mag):
    out = [b"BBM1", struct.pack("<3d", R0, H, rim_tan)]
    if mag:
        rm, dm, cens = mag; out.append(struct.pack("<B2dI", 1, rm, dm, len(cens))); out += [struct.pack("<2d", cx, cy) for cx, cy in cens]
    else: out.append(struct.pack("<B", 0))
    out.append(geom(F.disc))
    ov = getattr(F, "oval", None)
    if ov:
        TR = np.asarray(ov[2], dtype=float); OU = np.asarray(ov[3], dtype=float)
        out.append(struct.pack("<BI", 1, len(TR))); out.append(TR.astype("<f8").tobytes()); out.append(struct.pack("<I", len(OU))); out.append(OU.astype("<f8").tobytes())
    else: out.append(struct.pack("<B", 0))
    out.append(struct.pack("<I", len(F.cells))); ids = {}; idx = []
    for cp, r in F.cells:
        out.append(geom(cp)); idx.append(ids.setdefault(id(r), len(ids)))
        cls = r.__class__.__name__
        if cls == "Flat": out.append(struct.pack("<Bd", 0, r.z))
        elif cls == "Step":
            out.append(struct.pack("<Bd", 1, r.z))
            p = getattr(r, "poly", None); zb = getattr(r, "zb", None)
            out.append(struct.pack("<B", 1 if p is not None else 0));
            if p is not None: out.append(geom(p))
            out.append(struct.pack("<Bd", 1 if zb is not None else 0, zb if zb is not None else 0.0))
        elif cls == "Slope": out.append(struct.pack("<B", 2)); out.append(geom(r.poly)); out.append(struct.pack("<3d", r.z0, r.tan, r.zmax))
        elif cls == "Band":
            out.append(struct.pack("<B", 3)); out.append(geom(r.poly))
            out.append(geom(r.inner)); out.append(struct.pack("<3d", r.lo, r.hi, r.w))          # the inner piece's boundary, as Band.at uses it
        else: raise SystemExit(f"unknown rule {cls}")
    out.append(struct.pack("<I", len(idx))); out.append(struct.pack(f"<{len(idx)}I", *idx))
    return b"".join(out)

def hooked(F, R0, H, rim_tan, mag, fix=None):
    blob = dump_in(F, R0, H, rim_tan, mag)
    v, f = _orig(F, R0, H, rim_tan, mag, fix)
    with open(os.path.join(OUT, CUR[0] + ".min"), "wb") as fh: fh.write(blob)
    with open(os.path.join(OUT, CUR[0] + ".ref"), "wb") as fh:
        fh.write(b"BBR1" + struct.pack("<II", len(v), len(f))); fh.write(np.asarray(v, dtype="<f8").tobytes()); fh.write(np.asarray(f, dtype="<i4").tobytes())
    return v, f
mesher.mesh_from_field = hooked

for st, n in jobs:
    CUR[0] = f"{st}_{os.environ.get('BASE_MM', '')}_{n}"
    os.environ["BASENAME"] = CUR[0]
    r = mesher.build(st, int(n), os.path.join(OUT, CUR[0] + ".stl"))
    print(CUR[0], r["cells"], "cells", r["tris"], "tris", flush=True)
