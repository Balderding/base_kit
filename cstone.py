"""the rock clusters in C: gg3.cluster() of gg.stone() rocks made by bbbcore.dll (cport/scatter.c), bit for bit the
same rocks and the same random numbers as the Python. gg3.cluster calls cluster() when its make_one carries a cspec
(details.sh_rock / sh_round, gg3.stone_one); None means "do it in Python" (no dll, KIT_CROCKS=0, "c_rocks": false in
settings.json, or something the C side does not do), and the rng is then untouched.
KIT_CORE_DLL=<path> loads another build of the dll (the proof runs)."""
import os, ctypes, random
HERE = os.path.dirname(os.path.abspath(__file__))
_dll = None; _tried = False; _on = None
_CS = None
CALLS = [0, 0]          # [made in C, handed back to Python]


def _load():
    global _dll, _tried, _CS
    if _tried: return _dll
    _tried = True
    p = os.environ.get("KIT_CORE_DLL") or os.path.join(HERE, "bbbcore.dll")
    if not os.path.exists(p): return None
    try:
        d = ctypes.CDLL(p); f = d.bbb_cluster
    except (OSError, AttributeError): return None
    D, I, PD = ctypes.c_double, ctypes.c_int, ctypes.POINTER(ctypes.c_double)
    f.argtypes = [ctypes.POINTER(ctypes.c_uint32), I, D, D, I, I, I, D, PD, PD, I, D, D, D, D, ctypes.POINTER(ctypes.c_int), PD, I]
    f.restype = I
    import gg          # numpy's own cos / sin of the 40 angles, exactly the ones gg.stone uses
    _CS = ((D * 40)(*[float(v) for v in gg._CSX]), (D * 40)(*[float(v) for v in gg._CSY]))
    _dll = d
    return d


def enabled():
    global _on
    if _on is None:
        try:
            import kit_settings; cfg = bool(kit_settings.CFG.get("c_rocks", True))
        except Exception: cfg = True
        _on = cfg and os.environ.get("KIT_CROCKS") != "0" and _load() is not None
    return _on


_ST = (ctypes.c_uint32 * 625)(); _LEN = (ctypes.c_int * 64)(); _CAP = 8192; _XY = (ctypes.c_double * (2 * _CAP))()


def cluster(rng, spec, n, cx, cy, spread, gap):
    """the list gg3.cluster would return, or None (then nothing was drawn from rng)"""
    if type(rng) is not random.Random or not enabled() or n > 64: CALLS[1] += 1; return None
    v, st, g = rng.getstate()
    _ST[:] = st
    rnd, r0, r1, lo, hi, nvc, asp = spec
    k = _dll.bbb_cluster(_ST, rnd, r0, r1, lo, hi, nvc, asp, _CS[0], _CS[1], n, cx, cy, spread, gap, _LEN, _XY, _CAP)
    if k < 0: CALLS[1] += 1; return None
    rng.setstate((v, tuple(_ST), g))
    from shapely.geometry import Polygon
    out = []; j = 0; xy = _XY
    for i in range(k):
        m = _LEN[i]; out.append(Polygon([(xy[2 * t], xy[2 * t + 1]) for t in range(j, j + m)])); j += m
    CALLS[0] += 1
    return out
