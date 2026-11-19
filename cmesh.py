"""The C mesher, inside the kit's own exe (bbb_kit.exe --core-mesh, built by gui\\build.cmd; the older
bbbcore.dll from cport\\build.cmd is used only if the exe cannot be): the same mesh as mesher.mesh_from_field, bit for bit
(proved by cport\\bin\\meshtest.exe on the proof set), about 3.5 times faster.

mesher.build() calls mesh(F, R0, H, rim_tan, mag) first; it returns None (and the Python mesher runs instead) when
the dll is not there, when settings.json has "c_mesher": false or KIT_CMESH=0 is set, or when the C side reports an
error. The input goes over as the proof format (pack()), so what the dll gets is exactly what the proof files hold."""
import os, struct, ctypes
import numpy as np, shapely

HERE = os.path.dirname(os.path.abspath(__file__))
_dll = None; _tried = False; LAST = {"used": False, "error": ""}


def _geom(g):
    b = shapely.to_wkb(g, hex=False, output_dimension=2, byte_order=1, include_srid=False)
    return struct.pack("<I", len(b)) + b + struct.pack("<d", float(shapely.get_precision(g)))          # WKB loses the precision grid: it is kept beside it


def pack(F, R0, H, rim_tan, mag):
    """the mesher's input as bytes: the .min format (cport/dump_mesh.py describes it)"""
    out = [b"BBM1", struct.pack("<3d", R0, H, rim_tan)]
    if mag:
        rm, dm, cens = mag; out.append(struct.pack("<B2dI", 1, rm, dm, len(cens))); out += [struct.pack("<2d", cx, cy) for cx, cy in cens]
    else: out.append(struct.pack("<B", 0))
    out.append(_geom(F.disc))
    ov = getattr(F, "oval", None)
    if ov:
        TR = np.asarray(ov[2], dtype=float); OU = np.asarray(ov[3], dtype=float)
        out.append(struct.pack("<BI", 1, len(TR))); out.append(TR.astype("<f8").tobytes()); out.append(struct.pack("<I", len(OU))); out.append(OU.astype("<f8").tobytes())
    else: out.append(struct.pack("<B", 0))
    out.append(struct.pack("<I", len(F.cells))); ids = {}; idx = []
    for cp, r in F.cells:
        out.append(_geom(cp)); idx.append(ids.setdefault(id(r), len(ids)))
        cls = r.__class__.__name__
        if cls == "Flat": out.append(struct.pack("<Bd", 0, r.z))
        elif cls == "Step":
            out.append(struct.pack("<Bd", 1, r.z))
            p = getattr(r, "poly", None); zb = getattr(r, "zb", None)
            out.append(struct.pack("<B", 1 if p is not None else 0))
            if p is not None: out.append(_geom(p))
            out.append(struct.pack("<Bd", 1 if zb is not None else 0, zb if zb is not None else 0.0))
        elif cls == "Slope": out.append(struct.pack("<B", 2)); out.append(_geom(r.poly)); out.append(struct.pack("<3d", r.z0, r.tan, r.zmax))
        elif cls == "Band": out.append(struct.pack("<B", 3)); out.append(_geom(r.poly)); out.append(_geom(r.inner)); out.append(struct.pack("<3d", r.lo, r.hi, r.w))
        else: raise ValueError(f"unknown rule {cls}")
    out.append(struct.pack("<I", len(idx))); out.append(struct.pack(f"<{len(idx)}I", *idx))
    return b"".join(out)


def pack_ops(base, steps, stand=None):
    """the field builder's input: the "BBF1" stream bbb_kit.exe --core-build reads (cport/field.c). stand: the
    miniature's standing spots (already widened), with the base height and the scenery scale: (pads, H, sc)"""
    out = [b"BBF1", struct.pack("<3d", base["R0"], base["H"], base["rim_tan"])]
    mag = base.get("mag")
    if mag:
        rm, dm, cens = mag; out.append(struct.pack("<B2dI", 1, rm, dm, len(cens))); out += [struct.pack("<2d", cx, cy) for cx, cy in cens]
    else: out.append(struct.pack("<B", 0))
    out.append(_geom(base["disc"]))
    ov = base.get("oval")
    if ov:
        TR = np.asarray(ov[2], dtype=float); OU = np.asarray(ov[3], dtype=float)
        out.append(struct.pack("<BI", 1, len(TR))); out.append(TR.astype("<f8").tobytes()); out.append(struct.pack("<I", len(OU))); out.append(OU.astype("<f8").tobytes())
    else: out.append(struct.pack("<B", 0))
    import mesher
    out.append(_geom(base["guard"])); out.append(struct.pack("<d", mesher.TILE))
    allst = list(steps) + ([(5, "Stand", stand[0], stand[1], stand[2], 0.0, -1)] if stand is not None else [])
    out.append(struct.pack("<I", len(allst)))
    for kind, name, g, a, b, c, ch in allst:
        nm = name.encode("utf-8")[:60]
        out.append(struct.pack("<2B", kind, len(nm))); out.append(nm); out.append(_geom(g)); out.append(struct.pack("<3di", float(a), float(b), float(c), int(ch)))
    return b"".join(out)


def build_c(base, steps, stand=None):
    """the height field cut and meshed in bbb_kit.exe (--core-build), on the CPU threads switch's threads.
    (vertices, triangles, number of cells, skipped steps), or None (then Python does it)"""
    import subprocess
    LAST["used"] = False; LAST["error"] = ""; LAST["by"] = ""
    exe = os.path.join(HERE, "bbb_kit.exe")
    if not enabled() or not os.path.exists(exe): return None
    try: blob = pack_ops(base, steps, stand)
    except Exception as e: LAST["error"] = f"pack: {type(e).__name__} {e}"; return None
    if os.environ.get("KIT_DUMP_BBF"):          # speed work: keep the field builder's input
        with open(os.environ["KIT_DUMP_BBF"], "wb") as fh: fh.write(blob)
    env = dict(os.environ, KIT_MESH_THREADS=str(threads()))
    try: p = subprocess.run([exe, "--core-build"], input=blob, capture_output=True, timeout=900, env=env, creationflags=getattr(subprocess, "CREATE_NO_WINDOW", 0))
    except Exception as e: LAST["error"] = f"bbb_kit.exe: {type(e).__name__} {e}"; return None
    o = p.stdout
    if p.returncode != 0 or o[:4] != b"BBR1": LAST["error"] = "bbb_kit.exe: " + (p.stderr.decode("utf-8", "replace").strip() or f"exit {p.returncode}"); return None
    nv, nt = struct.unpack_from("<II", o, 4); e = 12 + 24 * nv + 12 * nt
    vv = np.frombuffer(o, "<f8", 3 * nv, 12).reshape(-1, 3).copy(); tt = np.frombuffer(o, "<i4", 3 * nt, 12 + 24 * nv).reshape(-1, 3).astype(np.int64)
    ncell, nsk = struct.unpack_from("<II", o, e + 4) if o[e:e + 4] == b"BBX1" else (0, 0)
    sk = [l for l in o[e + 12:e + 12 + nsk].decode("utf-8", "replace").split("\n") if l]
    LAST["used"] = True; LAST["by"] = f"bbb_kit.exe, field in C, {threads() or 1} thread{'s' if threads() > 1 else ''}"
    return vv, tt, ncell, sk


def _load():
    global _dll, _tried
    if _tried: return _dll
    _tried = True
    p = os.path.join(HERE, "bbbcore.dll")
    if not os.path.exists(p): return None
    try:
        d = ctypes.CDLL(p)
        d.bbb_mesh.argtypes = [ctypes.c_char_p, ctypes.c_size_t, ctypes.POINTER(ctypes.POINTER(ctypes.c_double)), ctypes.POINTER(ctypes.c_int),
                               ctypes.POINTER(ctypes.POINTER(ctypes.c_int)), ctypes.POINTER(ctypes.c_int), ctypes.c_char_p, ctypes.c_int]
        d.bbb_mesh.restype = ctypes.c_int; d.bbb_free.argtypes = [ctypes.c_void_p]; d.bbb_free.restype = None
        _dll = d
    except OSError as e: LAST["error"] = f"bbbcore.dll: {e}"
    return _dll


def enabled():
    if os.environ.get("KIT_CMESH") == "0": return False
    try:
        import kit_settings
        return bool(kit_settings.CFG.get("c_mesher", True))
    except Exception: return True


def mesh(F, R0, H, rim_tan, mag):
    """(vertices, triangles) as numpy arrays, exactly as mesher.mesh_from_field returns them; None = use Python.
    KIT_CMESH=require (the proof runs): no falling back, an error instead"""
    got = _mesh(F, R0, H, rim_tan, mag)
    if got is None and os.environ.get("KIT_CMESH") == "require": raise RuntimeError("C mesher required but not used: " + (LAST["error"] or "bbbcore.dll missing"))
    return got


def _exe(blob):
    """bbb_kit.exe --core-mesh: the mesher inside the kit's own exe (no window); None = not there or it failed"""
    import subprocess
    exe = os.path.join(HERE, "bbb_kit.exe")
    if not os.path.exists(exe): return None
    env = dict(os.environ, KIT_MESH_THREADS=str(threads()))          # the CPU threads switch (0 = the proven one-core mesh)
    try: p = subprocess.run([exe, "--core-mesh"], input=blob, capture_output=True, timeout=600, env=env, creationflags=getattr(subprocess, "CREATE_NO_WINDOW", 0))
    except Exception as e: LAST["error"] = f"bbb_kit.exe: {type(e).__name__} {e}"; return None
    o = p.stdout
    if p.returncode != 0 or o[:4] != b"BBR1": LAST["error"] = "bbb_kit.exe: " + (p.stderr.decode("utf-8", "replace").strip() or f"exit {p.returncode}"); return None
    nv, nt = struct.unpack_from("<II", o, 4)
    vv = np.frombuffer(o, "<f8", 3 * nv, 12).reshape(-1, 3).copy(); tt = np.frombuffer(o, "<i4", 3 * nt, 12 + 24 * nv).reshape(-1, 3).astype(np.int64)
    LAST["used"] = True; LAST["by"] = "bbb_kit.exe"
    return vv, tt


def threads():
    """the CPU threads switch: how many threads one base may use for its height field and its mesh (0 = off: one
    core, exactly the proven path). KIT_THREADS=0/n (the window sets it) wins over settings.json "cpu_threads"
    (true = up to every core the PC has, each step taking only as many as its work needs; false = off; or a number)"""
    e = os.environ.get("KIT_THREADS", "")
    if e.isdigit(): v = int(e)
    else:
        try:
            import kit_settings
            v = kit_settings.CFG.get("cpu_threads", True)
        except Exception: v = True
        v = ((os.cpu_count() or 4) if v else 0) if isinstance(v, bool) else int(v)          # true: every core the PC has
    return 0 if v <= 1 else min(v, 256)


def gpu_mode():
    """the 3D pictures: 1 = on the video card (bbb_kit.exe --core-shot, Direct3D 11; Windows' software renderer if
    the card cannot), 0 = the Python drawing. KIT_GPU=0/1 (the window's Video card switch) wins over settings.json
    "video_card" (default on)"""
    e = os.environ.get("KIT_GPU")
    if e in ("0", "1"): return int(e)
    try:
        import kit_settings
        return 1 if kit_settings.CFG.get("video_card", True) else 0
    except Exception: return 1


SHOT_LAST = {"by": ""}


def shot(V, Fc, w, h, elev, azim):
    """mesher.shot's picture drawn by bbb_kit.exe: the same camera, light and shading. A PIL image, or None (then the
    Python drawing is used)"""
    import subprocess
    SHOT_LAST["by"] = ""
    if not gpu_mode(): return None
    exe = os.path.join(HERE, "bbb_kit.exe")
    if not os.path.exists(exe): return None
    V = np.ascontiguousarray(V, dtype="<f8"); Fc = np.ascontiguousarray(Fc, dtype="<i4")
    blob = b"BBS1" + struct.pack("<3I2d2I", int(w), int(h), 1, float(elev), float(azim), len(V), len(Fc)) + V.tobytes() + Fc.tobytes()
    try: p = subprocess.run([exe, "--core-shot"], input=blob, capture_output=True, timeout=300, creationflags=getattr(subprocess, "CREATE_NO_WINDOW", 0))
    except Exception: return None
    o = p.stdout
    if p.returncode != 0 or o[:4] != b"BBI1": SHOT_LAST["by"] = "failed: " + p.stderr.decode("utf-8", "replace").strip(); return None
    W, Hh = struct.unpack_from("<2I", o, 4); dl = o[12]; SHOT_LAST["by"] = o[13:13 + dl].decode("utf-8", "replace")
    from PIL import Image
    return Image.frombytes("RGB", (W, Hh), o[13 + dl:13 + dl + W * Hh * 3])


def _mesh(F, R0, H, rim_tan, mag):
    LAST["used"] = False; LAST["error"] = ""; LAST["by"] = ""
    if not enabled(): return None
    try: blob = pack(F, R0, H, rim_tan, mag)
    except Exception as e: LAST["error"] = f"pack: {type(e).__name__} {e}"; return None
    if os.environ.get("KIT_DUMP_MIN"):          # speed work: keep the mesher's input (cport\bin\meshtest.exe reads it)
        with open(os.environ["KIT_DUMP_MIN"], "wb") as fh: fh.write(blob)
    got = _exe(blob)
    if got is not None: return got
    d = _load()          # the older bbbcore.dll, if it is there
    if d is None: return None
    v = ctypes.POINTER(ctypes.c_double)(); t = ctypes.POINTER(ctypes.c_int)(); nv = ctypes.c_int(); nt = ctypes.c_int(); err = ctypes.create_string_buffer(256)
    ok = d.bbb_mesh(blob, len(blob), ctypes.byref(v), ctypes.byref(nv), ctypes.byref(t), ctypes.byref(nt), err, 256)
    if not ok: LAST["error"] = err.value.decode("utf-8", "replace"); return None
    try:
        vv = np.ctypeslib.as_array(v, shape=(nv.value * 3,)).copy().reshape(-1, 3)
        tt = np.ctypeslib.as_array(t, shape=(nt.value * 3,)).copy().reshape(-1, 3).astype(np.int64)
    finally: d.bbb_free(v); d.bbb_free(t)
    LAST["used"] = True; LAST["by"] = "bbbcore.dll"
    return vv, tt
