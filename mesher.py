"""Build a base as a mesh directly, without SolidCraft.

Reads the same command list the SolidCraft build uses (gg.build_cmds), so the shapes, rim clipping and order are the
same. Every shape in the kit is an outline pushed up (straight or sloped) or cut down, so the top of a base is a
height map: for every spot there is one top height. This file keeps that height map as exact 2D cells
(polygon + height rule), then writes the mesh: top faces, walls where neighbouring cells differ, the 12 degree rim,
the flat bottom and the magnet pocket.  Sloped faces are real slopes, not steps.

    python mesher.py <style> <n> [out.stl]
"""
import math, os, sys, time
os.environ["KIT_ENGINE"] = "mesh"       # tells gg.py that sloped bands may meet exactly (no ledges)
import numpy as np
import shapely
from shapely.geometry import Polygon, Point, LineString, MultiPolygon
from shapely.ops import unary_union, polygonize
from shapely.strtree import STRtree

GRID = 1e-4          # every coordinate snaps to this, so shared edges really are shared
NSEG = 360           # facets round the rim
NSEG_OVAL = 480      # facets round an oval's rim
TILE = float(os.environ.get('MESH_TILE', 12.0))     # mm: size of the ground squares the top surface starts as


def P(g): return shapely.set_precision(g, GRID) if g is not None and not g.is_empty else g

def polys(g):
    if g is None or g.is_empty: return []
    if g.geom_type == "Polygon": return [g] if g.area > 1e-6 else []
    return [q for q in getattr(g, "geoms", []) for q in polys(q)]


class _BadCorner(Exception): pass


class Flat:
    __slots__ = ("z",)
    def __init__(s, z): s.z = round(z, 4)
    def at(s, x, y): return s.z
    lo = hi = property(lambda s: s.z)

class Slope:
    """height rises from z0 at the outline of `poly`, by `cot` mm per mm inwards, up to zmax"""
    __slots__ = ("poly", "ring", "z0", "cot", "zmax", "tan", "_ab")
    def __init__(s, poly, z0, tan, zmax):
        s.poly = poly; s.ring = poly.boundary; s.z0 = z0; s.tan = tan; s.cot = 1.0 / tan; s.zmax = zmax; s._ab = {}
    def at(s, x, y): return round(min(s.zmax, s.z0 + s.ring.distance(Point(x, y)) * s.cot), 4)
    def above(s, z):
        """the part of the outline where this slope is higher than z (worked out once per height: most cells it is
        asked about are ground at the same height)"""
        if z <= s.z0: return s.poly
        if z >= s.zmax: return None
        g = s._ab.get(z)
        if g is None: g = s._ab[z] = s.poly.buffer(-(z - s.z0) * s.tan, join_style=2)
        return g
    lo = property(lambda s: s.z0); hi = property(lambda s: s.zmax)


class Step(Flat):
    """the flat top of one piece of a rounded shape, until the next piece of its chain arrives"""
    __slots__ = ("poly", "zb")

class Band:
    """the sloped strip between two pieces of a rounded shape: `lo` at the outline of the lower piece, `hi` at the
    outline of the next one up, straight in between"""
    __slots__ = ("poly", "outer", "inner", "lo", "hi", "w")
    def __init__(s, poly, inner, lo, hi):
        s.poly = poly; s.outer = poly.boundary; s.inner = inner.boundary; s.lo = lo; s.hi = hi
        ds = [s.outer.distance(Point(c)) for g in polys(inner) for c in g.exterior.coords[::2]]
        s.w = max(max(ds, default=0.0), 0.05)             # the widest the strip gets between the two outlines
    def at(s, x, y):
        """between the two outlines: by how far across the strip the spot is. A part of the lower piece that the upper
        outline never reaches (the thin end of a rib) rises at the same slope and levels off at `hi`."""
        p = Point(x, y); d0 = s.outer.distance(p); d1 = s.inner.distance(p)
        t = max(d0 / (d0 + d1) if d0 + d1 > 1e-9 else 0.0, min(1.0, d0 / s.w))
        return round(s.lo + (s.hi - s.lo) * t, 4)
    def above(s, z):
        """a band is thin: it counts as wholly above or wholly below"""
        return s.poly if z < (s.lo + s.hi) / 2 else None


class Field:
    """the top surface as cells that never overlap: [(polygon, rule)]. The cells are kept in buckets, one per ground
    tile (a cell never leaves its tile: every step only splits cells), so a step only looks at the tiles it reaches.
    `cells` is all buckets one after the other: the same list, in the same order, as one flat list would be."""
    def __init__(s, disc, z):
        s.disc = disc; s.cells = [(disc, Flat(z))]; s.chains = {}

    @property
    def cells(s): return [c for b in s.buckets for c in b]
    @cells.setter
    def cells(s, v):
        s.buckets = [list(v)]; s.bbox = [(-1e18, -1e18, 1e18, 1e18)]

    def tile(s, guard, z, size=None):
        """start with the ground cut into squares instead of one big cell: each piece then only reshapes the few
        squares it stands on, not one huge outline with every earlier piece already cut into it"""
        from shapely.geometry import box
        size = size or TILE
        x0, y0, x1, y1 = guard.bounds; ring = P(s.disc.difference(guard))
        s.buckets = [[(ring, Flat(z))] if ring is not None and not ring.is_empty else []]; s.bbox = [ring.bounds if s.buckets[0] else (0, 0, 0, 0)]
        nx = max(1, math.ceil((x1 - x0) / size)); ny = max(1, math.ceil((y1 - y0) / size))
        for i in range(nx):
            for j in range(ny):
                b = (x0 + i * size, y0 + j * size, x0 + (i + 1) * size, y0 + (j + 1) * size)
                q = guard.intersection(box(*b))
                got = [(g, Flat(z)) for g in polys(P(q)) if g is not None and not g.is_empty]
                if got: s.buckets.append(got); s.bbox.append(b)

    def _region(s, region):
        region = P(region.intersection(s.disc))
        return None if region is None or region.is_empty else region

    def _touched(s, region):
        """the buckets whose box the region's box reaches"""
        x0, y0, x1, y1 = region.bounds
        return [k for k, (a, b, c, d) in enumerate(s.bbox) if a <= x1 and x0 <= c and b <= y1 and y0 <= d]

    @staticmethod
    def _apply_bucket(cells, region, new_for):
        """one bucket's cells after the step (exactly what the one flat list did to them)"""
        out = []
        for cp, rule in cells:
            if not cp.intersects(region): out.append((cp, rule)); continue
            inter = cp.intersection(region)
            taken = []
            if not inter.is_empty and inter.area > 1e-7:
                for sub, nr in new_for(inter, rule):
                    for q in polys(P(sub)): taken.append((q, nr))
            if not taken: out.append((cp, rule)); continue
            rest = cp.difference(unary_union([q for q, _ in taken]))
            for q in polys(P(rest)): out.append((q, rule))
            out += taken
        return out

    def _apply(s, region, new_for):
        """new_for(cell_poly, rule) -> list of (sub_polygon, new_rule) to take over inside region"""
        region = s._region(region)
        if region is None: return
        for k in s._touched(region): s.buckets[k] = s._apply_bucket(s.buckets[k], region, new_for)

    def join_prism(s, poly, z1, f=None):
        f = f or Flat(z1)
        def new_for(inter, rule):
            if rule.hi <= z1: return [(inter, f)]
            if rule.lo >= z1: return []
            ab = rule.above(z1)                            # sloped cell: keep the part of it that is higher
            return [(inter.difference(ab) if ab is not None and not ab.is_empty else inter, f)]
        s._apply(poly, new_for)

    def join_step(s, poly, z1, chain, zb):
        """one piece of a rounded shape. The piece before it in the same chain stops being a step: what is left of
        its top becomes a slope up to this piece's outline."""
        prev = s.chains.get(chain)
        if prev is not None and prev.z > prev.zb + 1e-4 and not poly.is_empty:
            band = Band(prev.poly, poly, prev.zb, prev.z)
            s.buckets = [[(cp, band if r is prev else r) for cp, r in b] for b in s.buckets]
        f = Step(z1); f.poly = poly; f.zb = min(zb, z1); s.chains[chain] = f
        s.join_prism(poly, z1, f)

    def join_taper(s, poly, z0, h, tan):
        ztop = z0 + h
        top = poly.buffer(-h * tan, join_style=2)
        sl = Slope(poly, z0, tan, ztop)
        def new_for(inter, rule):
            zc = rule.z if isinstance(rule, Flat) else rule.at(*inter.representative_point().coords[0])
            if zc >= ztop: return []
            vis = sl.above(zc)                             # where the new slope stands above what is there
            if vis is None or vis.is_empty: return []
            vis = inter.intersection(vis)
            res = []
            if not top.is_empty:
                t = vis.intersection(top)
                if not t.is_empty: res.append((t, Flat(ztop)))
                vis = vis.difference(top)
            if not vis.is_empty: res.append((vis, sl))
            return res
        s._apply(poly, new_for)

    def cut(s, poly, zb, zt=None):
        """lower what is inside `poly` to zb. zt = the top of the cutting tool: what stands higher than that (a tank
        standing over a groove in the ground) stays whole, as the solid tool would leave it; None = no top"""
        f = Flat(zb)
        def new_for(inter, rule):
            if zt is not None and rule.lo > zt + 1e-6: return []          # wholly above the tool
            full = zt is None or rule.hi <= zt + 1e-6
            if rule.lo >= zb and full: return [(inter, f)]
            if rule.hi <= zb: return []
            g = inter
            if rule.lo < zb:
                ab = rule.above(zb)
                if ab is None or ab.is_empty: return []
                g = g.intersection(ab)
            if not full:                                   # a slope reaching over the tool's top: keep that part
                at = rule.above(zt)
                if at is not None and not at.is_empty: g = g.difference(at)
            return [(g, f)] if not g.is_empty else []
        s._apply(poly, new_for)


def _arc_pts(a, m, b, step=0.12):
    """points along the circle arc from a through m to b"""
    ax, ay = a; bx, by = b; mx, my = m
    d = 2 * (ax * (by - my) + bx * (my - ay) + mx * (ay - by))
    if abs(d) < 1e-12: return [a, m, b]
    ux = ((ax * ax + ay * ay) * (by - my) + (bx * bx + by * by) * (my - ay) + (mx * mx + my * my) * (ay - by)) / d
    uy = ((ax * ax + ay * ay) * (mx - bx) + (bx * bx + by * by) * (ax - mx) + (mx * mx + my * my) * (bx - ax)) / d
    r = math.hypot(ax - ux, ay - uy)
    t0 = math.atan2(ay - uy, ax - ux); t1 = math.atan2(my - uy, mx - ux); t2 = math.atan2(by - uy, bx - ux)
    def unwrap(t, ref, sign):
        while sign * (t - ref) < 0: t += sign * 2 * math.pi
        return t
    sign = 1
    if not (unwrap(t1, t0, 1) <= unwrap(t2, t0, 1)): sign = -1
    t2u = unwrap(t2, t0, sign)
    # three points almost in a line give a huge circle, and rounding can then send the arc the long way round it: an
    # outline hundreds of millimetres across, which as a cut planes the whole base flat. An arc is never much longer
    # than the two straight hops through its middle point; one that is, is that mistake: take the straight hops.
    if abs(t2u - t0) * r > 2.5 * (math.hypot(mx - ax, my - ay) + math.hypot(bx - mx, by - my)) + 1e-9: return [a, m, b]
    n = max(2, int(abs(t2u - t0) * r / step))
    return [(ux + r * math.cos(t0 + (t2u - t0) * i / n), uy + r * math.sin(t0 + (t2u - t0) * i / n)) for i in range(n + 1)]


def sketch_region(cmds):
    """the filled region a sketch describes (closed lines, line+arc loops, circles); nested outlines are holes"""
    loops = []; run = []; circ = None
    for c in cmds:
        p = c["params"]; k = c["command"]
        if k == "sketch.circle.center": loops.append(Point(p["center"]).buffer(p["radius"], NSEG // 4))
        elif k == "sketch.line" and p.get("closed"):
            if len(p["points"]) >= 3: loops.append(Polygon(p["points"]))
        elif k == "sketch.line": run += list(p["points"])
        elif k == "sketch.arc.three_point": run += _arc_pts(p["start"], p["through"], p["end"])
        if run and len(run) >= 4 and math.hypot(run[-1][0] - run[0][0], run[-1][1] - run[0][1]) < 2e-3:
            loops.append(Polygon(run)); run = []
    if len(run) >= 3: loops.append(Polygon(run))
    loops = [l.buffer(0) for l in loops if l.is_valid or True]
    loops = [l for l in loops if not l.is_empty and l.area > 1e-5]
    reg = None
    for l in sorted(loops, key=lambda g: -g.area):      # even-odd: an outline inside another is a hole
        if reg is None: reg = l
        elif reg.contains(l): reg = reg.difference(l)
        else: reg = reg.union(l)
    return reg


def scale_plan(s):
    """with a scenery scale: (scale, base mm, shift x, shift y, the miniature's standing spots on the finished base).
    The miniature must always be able to stand on the base, at its own real size: so the enlarged scene is slid (as
    far as the layout reaches past the edge) to bring the standing place onto the base, and build() then cuts the
    real-size standing spots flat there, whatever was laid on them. None when no scale is set."""
    import gg
    from shapely import affinity
    fin = float(os.environ.get("KIT_FINAL_MM", 0) or 0); sc = float(os.environ.get("KIT_SCALE", 1) or 1)
    if gg.oval(): return _oval_plan(s, gg.oval(), sc)
    if not fin or (abs(sc - 1) < 1e-6 and abs(fin - gg.BASE_MM) < 1e-6): return None
    tx = ty = 0.0; pads = None; sd = getattr(s, "stand", None)
    try:
        if sd and sd.get("pads"):
            U = unary_union([q for q in sd["pads"] if q is not None and not q.is_empty]); c = U.centroid
            rp = max(math.hypot(x - c.x, y - c.y) for g in polys(U) for x, y in g.exterior.coords) * fin / gg.BASE_MM
            px, py = c.x * sc, c.y * sc; lim = max(0.0, fin / 2 - rp - 2.0); h = math.hypot(px, py); qx, qy = (px * lim / h, py * lim / h) if h > lim else (px, py)
            slack = max(0.0, (gg.BASE_MM * sc - fin) / 2); dx, dy = qx - px, qy - py; dh = math.hypot(dx, dy)
            if dh > slack and dh > 0: dx, dy = dx * slack / dh, dy * slack / dh          # slide no further than the layout has to give: the top may not go bare at the far side
            tx, ty = dx, dy; pads = affinity.translate(affinity.scale(U, fin / gg.BASE_MM, fin / gg.BASE_MM, origin=(c.x, c.y)), qx - c.x, qy - c.y)          # the spots are a share of the base: as large as the finished base needs
    except Exception: pads = None
    return sc, fin, tx, ty, pads


def _oval_plan(s, ov, sc):
    """scale_plan() for an oval base (even with no scenery scale: the round layout is wider than the oval). The
    standing spots are sized as on a round base of the oval's length, made smaller only if they would not fit, and
    moved (no further than needed) so that they lie wholly on the oval's flat top, 2.4 mm inside its outline, which
    keeps them on the flat even with the 1.2 mm margin they are cut with. The scene is slid along as far as the round
    layout still covers the whole oval."""
    import gg
    from shapely import affinity
    A, B = ov; a, b = A / 2.0, B / 2.0; fin = A
    ring, T = gg.ellipse_ring(a, b, 240)
    inner = Polygon(gg.ellipse_offset(a, b, T, 2.4))
    tx = ty = 0.0; pads = None; sd = getattr(s, "stand", None)
    try:
        if sd and sd.get("pads"):
            U = unary_union([q for q in sd["pads"] if q is not None and not q.is_empty]); c = U.centroid
            k = fin / gg.BASE_MM
            for _ in range(30):                             # too big for the oval: smaller until they fit in its middle
                Q = affinity.scale(U, k, k, origin=(c.x, c.y))
                if inner.contains(affinity.translate(Q, -c.x, -c.y)): break
                k *= 0.9
            def fits(x, y): return inner.contains(affinity.translate(Q, x - c.x, y - c.y))
            px, py = c.x * sc, c.y * sc
            if fits(px, py): qx, qy = px, py
            else:
                lo, hi = 0.0, 1.0
                for _ in range(24):
                    mid = (lo + hi) / 2
                    if fits(px * mid, py * mid): lo = mid
                    else: hi = mid
                qx, qy = px * lo, py * lo
            dx, dy = qx - px, qy - py; RL = gg.BASE_MM * sc / 2.0 + 0.1
            def covers(t): return all(math.hypot(x - t * dx, y - t * dy) <= RL for x, y in ring)          # the layout still reaches every part of the oval: its top never goes bare
            t = 0.0
            if sc > 1.0 + 1e-6 and (dx or dy) and covers(0.0):
                if covers(1.0): t = 1.0
                else:
                    lo, hi = 0.0, 1.0
                    for _ in range(24):
                        mid = (lo + hi) / 2
                        if covers(mid): lo = mid
                        else: hi = mid
                    t = lo
            tx, ty = dx * t, dy * t; pads = affinity.translate(Q, qx - c.x, qy - c.y)
    except Exception: pads = None
    return sc, fin, tx, ty, pads


def stand_flat(F, pads, H, sc):
    """the miniature's standing spots made flat, level with the ground they lie on: whatever stands higher there is
    cut down to it and whatever is lower (a joint, a crack) is filled up to it. The ground's level is the height that
    most of the spots' area already has, among heights a ground can have."""
    pads = P(pads.intersection(F.guard))
    if pads is None or pads.is_empty: return
    area = {}
    for cp, rule in F.cells:
        if H - 1e-6 <= rule.lo and rule.hi <= H + 0.9 * sc + 1e-6 and cp.intersects(pads):
            z = round(float(rule.hi), 2); area[z] = area.get(z, 0.0) + cp.intersection(pads).area
    level = max(area, key=area.get) if area else H
    F.join_prism(pads, level); F.cut(pads, level)


def rescale(groups, f, mm, cap=25.0, floor=2.9, tx=0.0, ty=0.0, pads=None):
    """the scenery scale. The kit lays out a small base and this makes everything on it f times larger, on a base of
    mm millimetres: every outline f times wider about the middle, every height above the deck and every depth below
    it f times as much. The base itself (its height, its sloped edge, its magnet pockets) is that of the mm base, not
    scaled. What then reaches past the edge is cut off there by the builder, so a small base shows a fragment of the
    scene. Nothing stands more than `cap` mm above the deck and no cut goes below `floor` mm from the bottom (the
    magnet pockets are 2.1 mm deep)."""
    import gg
    H = next((c["params"]["distance"] for g in groups for c in g if c["command"] == "solid.extrude" and c["params"].get("name") == "Base"), 4.0)
    def Z(z): return min(H + (z - H) * f, H + cap) if z >= H else max(H - (H - z) * f, min(floor, z))
    def P(q): return [q[0] * f + tx, q[1] * f + ty]
    out = []
    for g in groups:
        names = [str(c["params"].get("name", "")) for c in g if c["command"] == "solid.extrude"]; ng = []; off = 0.0; mag_done = False
        for c in g:
            k = c["command"]; p = dict(c["params"])
            if "Base" in names:
                if k == "sketch.circle.center": p["radius"] = mm / 2.0
            elif "MagCut" in names:
                if k == "sketch.circle.center":
                    if mag_done: continue
                    mag_done = True; ng += [{"command": k, "params": dict(p, center=[x, y])} for x, y in gg.magnet_centres(mm)]; continue
            elif k == "sketch.create": off = p.get("offset", 0.0); p["offset"] = Z(off)
            elif k == "sketch.circle.center": p["center"] = P(p["center"]); p["radius"] = p["radius"] * f
            elif k == "sketch.line": p["points"] = [P(q) for q in p["points"]]
            elif k == "sketch.arc.three_point": p.update(start=P(p["start"]), through=P(p["through"]), end=P(p["end"]))
            elif k == "solid.extrude":
                d = p["distance"]
                if p.get("operation") == "cut" and p.get("direction") == "negative": p["distance"] = max(Z(off) - Z(off - d), 0.01)
                elif p.get("operation") != "cut": p["distance"] = max(Z(off + d) - Z(off), 0.01)
                if p.get("loft"): p["loft"] = [p["loft"][0], Z(p["loft"][1])]
            ng.append({"command": k, "params": p})
        out.append(ng)
    if pads is not None and not pads.is_empty:          # where the miniature stands: flat at deck height, at the miniature's real size
        g = [{"command": "sketch.create", "params": {"plane": "XY", "offset": H + cap + 1.0, "name": "Stand"}}]
        g += [{"command": "sketch.line", "params": {"points": [[round(x, 4), round(y, 4)] for x, y in list(q.exterior.coords)[:-1]], "closed": True}} for q in polys(pads.buffer(1.2, 6))]
        g += [{"command": "sketch.finish", "params": {}}, {"command": "solid.extrude", "params": {"sketch": "Stand", "distance": cap + 1.0, "direction": "negative", "operation": "cut", "targets": ["Base"], "name": "FStand"}}]
        out.append(g)
    return out


def field_from_cmds(groups):
    """run the kit's command groups on a height field. Returns (field, base radius, top z, rim tan, magnet (r, depth))"""
    import gg, live
    F = None; R0 = H = None; rim_tan = 0.0; mag = None; skipped = []
    for gi, g in enumerate(groups):
        live.note(steps_done=gi)                            # the window's live view (nothing when it is not watching)
        ext = [c for c in g if c["command"] == "solid.extrude"]
        if not ext: continue                                # chamfer polish etc.: nothing to do here
        # a group may hold several sketch+extrude pairs (straight part + sloped cap)
        cur = []; off = 0.0
        for c in g:
            k = c["command"]; p = c["params"]
            if k == "sketch.create": cur = []; off = p.get("offset", 0.0)
            elif k.startswith("sketch.") and k != "sketch.finish": cur.append(c)
            elif k == "solid.extrude":
                name = str(p.get("name", "")); op = p.get("operation"); dist = p["distance"]; tap = abs(p.get("taper", 0.0))
                reg = sketch_region(cur)
                if reg is None or reg.is_empty: continue
                if name == "Base":
                    R0 = cur[0]["params"]["radius"]; H = off + dist; rim_tan = math.tan(math.radians(tap))
                    rt = R0 - H * rim_tan
                    ov = gg.oval()
                    if ov:                                      # an oval base: its top is the outline H*tan(rim) inside the oval all round (an offset curve)
                        oa, ob = ov[0] / 2.0, ov[1] / 2.0; ring, T = gg.ellipse_ring(oa, ob, NSEG_OVAL); topr = gg.ellipse_offset(oa, ob, T, H * rim_tan)
                        disc = P(Polygon(topr))
                    else:
                        ang = [2 * math.pi * i / NSEG for i in range(NSEG)]
                        disc = P(Polygon([(rt * math.cos(a), rt * math.sin(a)) for a in ang]))
                    F = Field(disc, H); F.guard = P(disc.buffer(-0.02, join_style=2)); F.tile(F.guard, H)
                    if ov: F.oval = (oa, ob, topr, ring); F.ref_area = math.pi * (rt - 0.02) ** 2          # "a cut far larger than the base" is measured against the round layout, as on a round base
                    continue
                if name == "MagCut":
                    mag = (cur[0]["params"]["radius"], off, [tuple(c_["params"]["center"]) for c_ in cur])
                    if gg.oval(): mag = (mag[0], mag[1], gg.oval_magnet_centres(*gg.oval()))          # an oval's pockets lie along its long axis
                    continue
                if F is None: continue
                if op == "cut" and reg.area > 1.5 * getattr(F, "ref_area", F.guard.area): skipped.append(f"{name}: a cut far larger than the base, left out"); continue          # never let one bad outline plane the base
                reg = reg.intersection(F.guard)
                if reg.is_empty: continue
                try:
                    if op == "cut":
                        zb, zt = (off - dist, off) if p.get("direction") == "negative" else (off, off + dist)
                        F.cut(reg, zb, zt)
                    elif tap > 0.01:
                        for q in polys(reg): F.join_taper(q, off, dist, math.tan(math.radians(tap)))
                    elif p.get("loft"): F.join_step(P(reg), off + dist, p["loft"][0], p["loft"][1])
                    else: F.join_prism(reg, off + dist)
                except Exception as e:
                    skipped.append(f"{name}: {type(e).__name__}")
    return F, R0, H, rim_tan, mag, skipped


def field_ops(groups):
    """field_from_cmds() for the C field builder (the CPU threads switch): the same reading of the command groups,
    but the steps are only written down, for bbb_kit.exe --core-build to cut on all threads. Returns
    (base: dict of what the mesher needs, steps: [(kind, name, outline, a, b, c, chain)], skipped). Kinds: 1 prism
    (a = top z), 2 step (a = top z, b = zb, chain), 3 taper (a = z0, b = height, c = tan), 4 cut (a = bottom z, b = the tool's top z)."""
    import gg
    base = None; steps = []; skipped = []; chains = {}; mag = None
    for g in groups:
        ext = [c for c in g if c["command"] == "solid.extrude"]
        if not ext: continue
        cur = []; off = 0.0
        for c in g:
            k = c["command"]; p = c["params"]
            if k == "sketch.create": cur = []; off = p.get("offset", 0.0)
            elif k.startswith("sketch.") and k != "sketch.finish": cur.append(c)
            elif k == "solid.extrude":
                name = str(p.get("name", "")); op = p.get("operation"); dist = p["distance"]; tap = abs(p.get("taper", 0.0))
                reg = sketch_region(cur)
                if reg is None or reg.is_empty: continue
                if name == "Base":
                    R0 = cur[0]["params"]["radius"]; H = off + dist; rim_tan = math.tan(math.radians(tap)); rt = R0 - H * rim_tan
                    ov = gg.oval(); oval = None; ref_area = None
                    if ov:
                        oa, ob = ov[0] / 2.0, ov[1] / 2.0; ring, T = gg.ellipse_ring(oa, ob, NSEG_OVAL); topr = gg.ellipse_offset(oa, ob, T, H * rim_tan)
                        disc = P(Polygon(topr)); oval = (oa, ob, topr, ring); ref_area = math.pi * (rt - 0.02) ** 2
                    else:
                        ang = [2 * math.pi * i / NSEG for i in range(NSEG)]
                        disc = P(Polygon([(rt * math.cos(a), rt * math.sin(a)) for a in ang]))
                    guard = P(disc.buffer(-0.02, join_style=2))
                    base = {"R0": R0, "H": H, "rim_tan": rim_tan, "disc": disc, "guard": guard, "oval": oval, "ref_area": ref_area or guard.area}
                    continue
                if name == "MagCut":
                    mag = (cur[0]["params"]["radius"], off, [tuple(c_["params"]["center"]) for c_ in cur])
                    if gg.oval(): mag = (mag[0], mag[1], gg.oval_magnet_centres(*gg.oval()))
                    continue
                if base is None: continue
                if op == "cut" and reg.area > 1.5 * base["ref_area"]: skipped.append(f"{name}: a cut far larger than the base, left out"); continue
                reg = reg.intersection(base["guard"])
                if reg.is_empty: continue
                try:
                    if op == "cut": steps.append((4, name, reg, *((off - dist, off) if p.get("direction") == "negative" else (off, off + dist)), 0.0, -1))
                    elif tap > 0.01:
                        for q in polys(reg): steps.append((3, name, q, off, dist, math.tan(math.radians(tap)), -1))
                    elif p.get("loft"): steps.append((2, name, P(reg), off + dist, p["loft"][1], 0.0, chains.setdefault(p["loft"][0], len(chains))))
                    else: steps.append((1, name, reg, off + dist, 0.0, 0.0, -1))
                except Exception as e:
                    skipped.append(f"{name}: {type(e).__name__}")
    if base is not None: base["mag"] = mag
    return base, steps, skipped


def _earclip(pts):
    """triangles of a simple outline with no holes, by cutting off one corner at a time (slow, only a fallback)"""
    pts = list(pts)
    if sum(pts[i][0] * pts[(i + 1) % len(pts)][1] - pts[(i + 1) % len(pts)][0] * pts[i][1] for i in range(len(pts))) < 0: pts.reverse()
    idx = list(range(len(pts))); out = []
    def cross(o, a, b): return (a[0] - o[0]) * (b[1] - o[1]) - (a[1] - o[1]) * (b[0] - o[0])
    guard = 0
    while len(idx) > 3 and guard < 20000:
        guard += 1; cut = False
        for k in range(len(idx)):
            i0, i1, i2 = idx[k - 1], idx[k], idx[(k + 1) % len(idx)]; A, B, C = pts[i0], pts[i1], pts[i2]
            if cross(A, B, C) <= 0: continue
            if any(j not in (i0, i1, i2) and cross(A, B, pts[j]) >= 0 and cross(B, C, pts[j]) >= 0 and cross(C, A, pts[j]) >= 0 for j in idx): continue
            out.append((A, B, C)); idx.pop(k); cut = True; break
        if not cut:                                         # only flat corners left: fan what remains
            out += [(pts[idx[0]], pts[idx[i]], pts[idx[i + 1]]) for i in range(1, len(idx) - 1)]; idx = []
    if len(idx) == 3: out.append((pts[idx[0]], pts[idx[1]], pts[idx[2]]))
    return out


def _tri2d(poly):
    """triangles of a polygon (with holes) using only its own vertices"""
    try:
        return [list(t.exterior.coords)[:3] for t in shapely.constrained_delaunay_triangles(poly).geoms]
    except Exception:
        if poly.interiors: raise
        return _earclip(list(poly.exterior.coords)[:-1])


def mesh_from_field(F, R0, H, rim_tan, mag, fix=None):
    V = {}; verts = []; tris = []
    def vid(x, y, z):
        k = (round(x / 1e-6), round(y / 1e-6), round(z / GRID))
        i = V.get(k)
        if i is None: i = V[k] = len(verts); verts.append((k[0] * 1e-6, k[1] * 1e-6, k[2] * GRID))
        return i
    def tri(a, b, c):
        if a != b and b != c and a != c: tris.append((a, b, c))
    # 1. one shared network of edges for all cells
    cells = [(P(cp), r) for cp, r in F.cells]; cells = [(cp, r) for cp, r in cells if cp is not None and not cp.is_empty and cp.area > 1e-7]
    lines = unary_union([cp.boundary for cp, _ in cells])
    faces = [f for f in polygonize(lines) if f.area > 0]
    tree = STRtree([cp for cp, _ in cells])
    frule = []
    for f in faces:
        pt = f.representative_point(); r = None
        for i in tree.query(pt):
            if cells[int(i)][0].contains(pt): r = cells[int(i)][1]; break
        if r is None: r = cells[int(tree.nearest(pt))][1]          # a hairline gap between two cells: belongs to the nearest
        frule.append(r)
    def _real(f):                                           # a face that is only a line walked there and back is no face
        ks = [(round(x / 1e-6), round(y / 1e-6)) for x, y in f.exterior.coords]
        return len(set(ks)) >= 3
    keep = [(f, r) for f, r in zip(faces, frule) if r is not None and _real(f)]
    _ST = os.environ.get("MESH_STAGES")          # the C port's proof: the same stage lines as cport/mesh.c
    if _ST: print("S faces", len(faces), "none", sum(1 for r in frule if r is None), "keep", len(keep), file=sys.stderr)
    # 2. heights of every face at every corner; all heights met at each corner
    hts = {}          # (x, y) -> set of z
    edges = {}        # undirected edge -> list of (z at p, z at q)
    KG = 1e-6
    _kc = {}
    def key(x, y):
        k = _kc.get((x, y))
        if k is None: k = _kc[(x, y)] = (round(x / KG), round(y / KG))
        return k
    _hc = {}
    def rat(r, x, y, k):
        """height of a rule at a corner, worked out once"""
        if r.__class__ is Flat: return r.z
        h = _hc.get((id(r), k))
        if h is None: h = _hc[(id(r), k)] = r.at(x, y)
        return h
    forced = {}
    def heights(rings_of):
        """canonical height of every face at every corner: heights a hair apart at one corner are the same height"""
        raw = {}; flats = {}
        for (f, r), rings in zip(keep, rings_of):
            for cs in rings:
                for x, y in cs:
                    k = key(x, y)
                    if k in forced: continue
                    z = rat(r, x, y, k); raw.setdefault(k, set()).add(z)
                    if isinstance(r, Flat): flats.setdefault(k, set()).add(z)
        canon = {}; hts.clear()
        for k, zs in raw.items():
            m = {}; groups = []
            for z in sorted(zs):
                if groups and z - groups[-1][-1] < 2.5e-3: groups[-1].append(z)
                else: groups.append([z])
            for g in groups:
                fl = [z for z in g if z in flats.get(k, ())]; c = fl[0] if fl else g[0]
                for z in g: m[z] = c
            canon[k] = m; hts[k] = set(m.values())
        for k, z in forced.items(): hts[k] = {z}
        def zat(r, x, y):
            k = key(x, y)
            return forced[k] if k in forced else canon[k][rat(r, x, y, k)]
        return zat
    side = {}
    def edge_table(rings_of, zat):
        edges.clear(); side.clear()
        for (f, r), rings in zip(keep, rings_of):
            for cs in rings:
                zs = [zat(r, x, y) for x, y in cs]
                for i in range(len(cs) - 1):
                    a, b = key(*cs[i]), key(*cs[i + 1])
                    if a == b: continue
                    if a < b: edges.setdefault((a, b), []).append((zs[i], zs[i + 1])); side.setdefault((a, b), []).append(1)
                    else: edges.setdefault((b, a), []).append((zs[i + 1], zs[i])); side.setdefault((b, a), []).append(-1)
    from shapely.geometry.polygon import orient
    keep = [(orient(f, 1.0), r) for f, r in keep]           # outline counter-clockwise, holes clockwise: the face is always on the left
    rings_of = [[list(f.exterior.coords)] + [list(i.coords) for i in f.interiors] for f, r in keep]
    def cut_corners(fix):
        nonlocal rings_of
        # corners where four walls would meet on one line (two high cells touching only at a point): cut the corner off
        # every face there and put a tiny flat tile in the gap, so each new corner has three faces and one clean wall
        fix = set(fix); nb = {}
        for rings in rings_of:
            for cs in rings:
                for j in range(len(cs) - 1):
                    a, b = key(*cs[j]), key(*cs[j + 1])
                    if a in fix: nb.setdefault(a, set()).add(b)
                    if b in fix: nb.setdefault(b, set()).add(a)
        eps = {v: min(0.02, 0.3 * min(math.hypot(o[0] - v[0], o[1] - v[1]) * KG for o in os_)) for v, os_ in nb.items()}
        def cutpt(v, o):
            d = math.hypot(o[0] - v[0], o[1] - v[1]) * KG; e = eps[v]
            return (round((v[0] * KG + (o[0] - v[0]) * KG * e / d) / KG) * KG, round((v[1] * KG + (o[1] - v[1]) * KG * e / d) / KG) * KG)
        zmid = {}
        for (f, r), rings in zip(keep, rings_of):
            for cs in rings:
                for x, y in cs[:-1]:
                    k = key(x, y)
                    if k in eps: zmid.setdefault(k, []).append(rat(r, x, y, k))
        new_rings = []
        for rings in rings_of:
            nr = []
            for cs in rings:
                body = cs[:-1]; n = len(body); out = []
                for j in range(n):
                    k = key(*body[j])
                    if k in eps and eps[k] > 5e-5:
                        out.append(cutpt(k, key(*body[j - 1]))); out.append(cutpt(k, key(*body[(j + 1) % n])))
                    else: out.append(body[j])
                nr.append(out + [out[0]])
            if len(nr[0]) < 4 or not Polygon(nr[0], nr[1:]).is_valid:          # cutting here would spoil this face: leave these corners alone
                raise _BadCorner({key(x, y) for cs in rings for x, y in cs} & set(eps))
            new_rings.append(nr)
        rings_of = new_rings
        for v, os_ in nb.items():
            if eps[v] <= 5e-5: continue
            pts = sorted({cutpt(v, o) for o in os_}, key=lambda q: math.atan2(q[1] - v[1] * KG, q[0] - v[0] * KG))
            if len(pts) < 3: continue
            zs = sorted(zmid[v]); tile = Polygon(pts)
            keep.append((tile, Flat(zs[len(zs) // 2]))); rings_of.append([pts + [pts[0]]])
    def settle():
        nonlocal rings_of
        zat = None
        for _pass in range(3):
            zat = heights(rings_of); edge_table(rings_of, zat)
            # where two neighbours swap which is higher along a shared edge, put a corner at the crossing, at one height
            split = {}
            for (a, b), lst in edges.items():
                if len(lst) != 2: continue
                (ap, aq), (bp, bq) = lst; dp = ap - bp; dq = aq - bq
                if dp * dq < 0:
                    u = dp / (dp - dq)
                    x = (a[0] + (b[0] - a[0]) * u) * KG; y = (a[1] + (b[1] - a[1]) * u) * KG
                    k = key(x, y)
                    if k in (a, b): continue
                    split[(a, b)] = (k[0] * KG, k[1] * KG); forced[k] = round(ap + (aq - ap) * u, 4)
            if not split: break
            new_rings = []
            for rings in rings_of:
                nr = []
                for cs in rings:
                    out = [cs[0]]
                    for i in range(len(cs) - 1):
                        a, b = key(*cs[i]), key(*cs[i + 1]); sp = split.get((a, b) if a < b else (b, a))
                        if sp is not None and a != b: out.append(sp)
                        out.append(cs[i + 1])
                    nr.append(out)
                new_rings.append(nr)
            rings_of = new_rings
        return zat
    def pinches():
        """corners where more than two walls would share one upright line"""
        at = {}
        for (a, b), lst in edges.items():
            if len(lst) != 2: continue
            (ap, aq), (bp, bq) = lst
            if abs(ap - bp) > GRID / 2: at.setdefault(a, []).append((min(ap, bp), max(ap, bp)))
            if abs(aq - bq) > GRID / 2: at.setdefault(b, []).append((min(aq, bq), max(aq, bq)))
        out = set()
        for v, iv in at.items():
            if len(iv) < 4: continue
            ev = sorted([(lo, 1) for lo, hi in iv] + [(hi, -1) for lo, hi in iv], key=lambda e: (e[0], e[1])); n = 0
            for z, d in ev:
                n += d
                if n > 2: out.add(v); break
        return out
    zat = settle()
    if _ST: print("S settle forced", len(forced), "edges", len(edges), file=sys.stderr)
    pins = pinches() if fix is None else set(fix)
    if _ST: print("S pins", len(pins), file=sys.stderr)
    for _try in range(6):
        if not pins: break
        saved = ([list(map(list, r)) for r in rings_of], len(keep), dict(forced))
        try:
            cut_corners(pins); zat = settle(); break
        except _BadCorner as e:                             # try again without the corners that cannot be cut
            rings_of = saved[0]; del keep[saved[1]:]; forced.clear(); forced.update(saved[2]); pins = pins - e.args[0] if e.args[0] else set()
            zat = settle()
        except Exception:                                   # leave the corners as they were: a pinched edge still prints
            rings_of = saved[0]; del keep[saved[1]:]; forced.clear(); forced.update(saved[2]); zat = settle(); break
    if _ST: print("S cut keep", len(keep), "forced", len(forced), "edges", len(edges), file=sys.stderr)
    for (f, r), rings in zip(keep, rings_of):
        poly = Polygon(rings[0], rings[1:])
        if not poly.is_valid: poly = f
        tl = _tri2d(poly) if poly.area > 0 else []
        if not tl and len(rings) == 1:                      # a face squeezed flat: still needed to tie its neighbours together
            cs = rings[0][:-1]; tl = [(cs[0], cs[i], cs[i + 1]) for i in range(1, len(cs) - 1)]
        for t in tl:
            (x0, y0), (x1, y1), (x2, y2) = t
            if (x1 - x0) * (y2 - y0) - (x2 - x0) * (y1 - y0) < 0: t = (t[0], t[2], t[1])      # counter-clockwise from above: faces up
            tri(*[vid(x, y, zat(r, x, y)) for x, y in t])
    n_top = len(tris)
    if _ST: print("S top tris", len(tris), "verts", len(verts), file=sys.stderr)
    if os.environ.get("DBG_MESH"):
        for (a, b), lst in edges.items():
            if len(lst) > 2:
                who = [(i, round(keep[i][0].area, 6), type(keep[i][1]).__name__) for i, rings in enumerate(rings_of) for cs in rings for j in range(len(cs) - 1) if {key(*cs[j]), key(*cs[j + 1])} == {a, b}]
                print("EDGE4", a, b, lst, who, file=sys.stderr)
        import collections
        print("faces", len(faces), "no rule", sum(1 for r in frule if r is None), "area none", round(sum(f.area for f, r in zip(faces, frule) if r is None), 6), "kept", len(keep), "edge counts", collections.Counter(len(l) for l in edges.values()), file=sys.stderr)
    # 3. walls between neighbours of different height
    rt = R0 - H * rim_tan
    for (a, b), lst in edges.items():
        if len(lst) != 2: continue                          # rim edge (one face): handled with the rim
        (ap, aq), (bp, bq) = lst
        if abs(ap - bp) < GRID / 2 and abs(aq - bq) < GRID / 2: continue
        pa = (a[0] * KG, a[1] * KG); pb = (b[0] * KG, b[1] * KG); L = math.hypot(pb[0] - pa[0], pb[1] - pa[1])
        lo_p, hi_p = sorted((ap, bp)); lo_q, hi_q = sorted((aq, bq))
        mid_p = sorted(z for z in hts[a] if lo_p < z < hi_p); mid_q = sorted(z for z in hts[b] if lo_q < z < hi_q)
        ring = [(0.0, lo_p), (L, lo_q)] + [(L, z) for z in mid_q] + [(L, hi_q)] + [(0.0, hi_p)] + [(0.0, z) for z in reversed(mid_p)]
        ded = [ring[0]]
        for pt in ring[1:]:
            if pt != ded[-1]: ded.append(pt)
        if ded[0] == ded[-1]: ded.pop()
        if len(ded) < 3: continue
        wp = Polygon(ded)
        if os.environ.get("DBG_MESH") and (not wp.is_valid or wp.area < 1e-12): print("BADWALL", pa, pb, (ap, aq), (bp, bq), mid_p, mid_q, wp.is_valid, wp.area, file=sys.stderr)
        if not wp.is_valid or wp.area < 1e-12: continue
        # the first face lies on the left of a->b when its side is +1; the wall faces the lower neighbour
        first_higher = (ap + aq) > (bp + bq); left_higher = first_higher if side[(a, b)][0] == 1 else not first_higher
        rx, ry = (pb[1] - pa[1]), -(pb[0] - pa[0])            # points to the right of a->b
        for t in _tri2d(wp):
            pts = [((pa[0] if u < L / 2 else pb[0]), (pa[1] if u < L / 2 else pb[1]), z) for u, z in t]
            A, B, C = (np.array(q) for q in pts); n = np.cross(B - A, C - A)
            if (n[0] * rx + n[1] * ry > 0) != left_higher: pts = [pts[0], pts[2], pts[1]]
            tri(*[vid(*q) for q in pts])
    # 4. rim (12 degree cone), bottom, magnet pocket
    n_wall = len(tris)
    if _ST: print("S walls tris", len(tris), "verts", len(verts), file=sys.stderr)
    rimc = list(F.disc.exterior.coords)[:-1]                 # the very corners the top surface ends on
    ang = [math.atan2(y, x) for x, y in rimc]; NS = len(rimc)
    top = [vid(x, y, H) for x, y in rimc]
    ov = getattr(F, "oval", None)
    if ov:                                                  # an oval: each top corner goes straight down the slope to the oval point it was drawn from
        TR = np.array(ov[2]); outer = [ov[3][int(np.argmin(((TR - (x, y)) ** 2).sum(axis=1)))] for x, y in rimc]
        bot = [vid(x, y, 0.0) for x, y in outer]
    else: bot = [vid(R0 * math.cos(a), R0 * math.sin(a), 0.0) for a in ang]
    for i in range(NS):
        j = (i + 1) % NS
        tri(top[i], bot[i], bot[j]); tri(top[i], bot[j], top[j])
    if mag:
        rm, dm, cens = mag; MS = 72
        ma = [2 * math.pi * i / MS for i in range(MS)]
        if not ov: outer = [(R0 * math.cos(a), R0 * math.sin(a)) for a in ang]
        holes = [[(cx + rm * math.cos(a), cy + rm * math.sin(a)) for a in ma] for cx, cy in cens]
        for t in _tri2d(Polygon(outer, holes)):                       # flat bottom round the pockets
            tri(*[vid(x, y, 0.0) for x, y in t])
        for (cx, cy), ring in zip(cens, holes):
            m0 = [vid(x, y, 0.0) for x, y in ring]; m1 = [vid(x, y, dm) for x, y in ring]; c = vid(cx, cy, dm)
            for i in range(MS):
                j = (i + 1) % MS
                tri(m0[i], m1[i], m1[j]); tri(m0[i], m1[j], m0[j])    # pocket wall
                tri(m1[i], c, m1[j])                                  # pocket roof
    else:
        c = vid(0.0, 0.0, 0.0)
        for i in range(NS): tri(bot[i], c, bot[(i + 1) % NS])
    vv = np.array(verts)
    for i in range(n_wall, len(tris)):                      # rim faces out, bottom faces down, pocket wall faces the middle, pocket roof faces down
        a, b, c = tris[i]; A, B, C = vv[a], vv[b], vv[c]; n = np.cross(B - A, C - A); g = (A + B + C) / 3
        rr = math.hypot(g[0], g[1])
        if abs(n[2]) > 5 * math.hypot(n[0], n[1]): want = n[2] < 0                     # flat: bottom or pocket roof
        elif mag and any(math.hypot(g[0] - cx, g[1] - cy) < mag[0] + 1e-6 for cx, cy in mag[2]):      # pocket wall: faces its own middle
            cx, cy = min(mag[2], key=lambda q: math.hypot(g[0] - q[0], g[1] - q[1])); want = n[0] * (g[0] - cx) + n[1] * (g[1] - cy) < 0
        else: want = n[0] * g[0] + n[1] * g[1] > 0                                    # rim
        if not want: tris[i] = (a, c, b)
    return vv, np.array(tris)


def add_solids(s, m, skipped=None, rlim=None):
    """--supports on: the true 3D solids of the objects still on the base, each a closed shell sunk a little into the
    surface (a slicer joins them). Returns (mesh, the shells added)."""
    import trimesh
    extra = []
    if getattr(s, "solids3d", None):
        import scenes, gg
        if rlim is None: rlim = gg.R0 - gg.H * math.tan(math.radians(12)) - 0.12
        alive = {getattr(op[-1], "ghost", None) for op in s.ops}
        for cid, zb, zs in s.solids3d:
            if cid not in alive: continue
            for mk in scenes.TRUE3D.get(cid, []):
                try:
                    V3, F3 = mk(zb, zs); t3 = trimesh.Trimesh(V3, F3, process=False)
                    if t3.volume < 0: t3.invert()          # faces outwards
                    if gg.oval():                          # an oval: pressed back inside the oval's top
                        oa, ob = gg.oval()[0] / 2.0 - 0.97, gg.oval()[1] / 2.0 - 0.97
                        d = np.hypot(t3.vertices[:, 0] / oa, t3.vertices[:, 1] / ob); out = d > 1.0
                        if out.any(): t3.vertices[out, :2] /= d[out][:, None]
                    else:
                        d = np.hypot(t3.vertices[:, 0], t3.vertices[:, 1]); out = d > rlim          # nothing reaches past the edge of the base's top: what would is pressed back to it
                        if out.any(): t3.vertices[out, :2] *= (rlim / d[out])[:, None]
                    extra.append(t3)
                except Exception as e: (skipped if skipped is not None else []).append(f"solid {cid}: {type(e).__name__}"); __import__("kitlog").caught(f"solid {cid} dropped", e)
        if extra: m = trimesh.util.concatenate([m] + extra)
    return m, extra

def build(style, n, path=None):
    import gg3, gg, trimesh, live
    t = time.time()
    ov0 = gg.oval()
    live.start(n, oval=f"{ov0[0]:g}x{ov0[1]:g}" if ov0 else "", mm=f"{gg.BASE_MM:g}", lay_r=f"{gg.R0:.3f}")          # the window's live view (only when it asks: KIT_LIVE)
    if live.on(): live.hook_layout()
    _kl = __import__("kitlog"); _kl.base(os.environ.get("BASENAME") or f"{style} #{n}"); _km = _kl.mark()          # every error this base catches (out/kit_errors.log)
    s = gg3.make(style, n)
    if os.environ.get("BASENAME"): s.name = os.environ["BASENAME"]
    import hashlib
    sig = hashlib.md5(repr([(op[0], op[-1], round(op[2], 2), [[round(v, 1) for c in p.exterior.coords for v in c] for p in op[1]]) for op in s.ops]).encode()).hexdigest()[:16]
    gg3.smooth_isolated(s)
    groups = list(gg.build_cmds(s))
    drawn = None
    if live.on():          # the window is watching: the 2D drawing now (not at the end, so it costs nothing more), to show while the rest is built
        if path is None:
            D = os.environ.get("SETDIR", gg3.CFG["out_folder"]); os.makedirs(f"{D}/{style}", exist_ok=True); path = f"{D}/{style}/{s.name}.stl"
        if gg3.CFG.get("drawing_2d", True):
            try: draw2d(s, path[:-4] + "_2d.png"); drawn = path[:-4] + "_2d.png"; live.plan(drawn, PLAN_WIN)
            except Exception as e: __import__("kitlog").caught("drawing from above (early)", e)
        live.note(stage="field", steps_total=len(groups), steps_done=0)
    fin = float(os.environ.get("KIT_FINAL_MM", 0) or 0); sc = float(os.environ.get("KIT_SCALE", 1) or 1)          # the scenery scale: laid out small, built sc times larger on the base that was asked for
    sp = scale_plan(s); ov = gg.oval()
    if sp and not (ov and abs(sp[0] - 1) < 1e-6 and not sp[2] and not sp[3]): groups = rescale(groups, sp[0], sp[1], tx=sp[2], ty=sp[3])          # an oval with no scenery scale is built as laid out, only cut to the oval
    import cmesh
    got = None
    if cmesh.threads() > 1 or os.environ.get("KIT_CFIELD") == "1":          # the CPU threads switch: the height field cut in C on all threads, and meshed there
        base, steps, skipped = field_ops(groups)
        if base is not None:
            stand = None
            if sp and sp[4] is not None:
                try: stand = (sp[4].buffer(1.2, 6), base["H"], sp[0])
                except Exception as e: skipped.append(f"standing place: {type(e).__name__}"); __import__("kitlog").caught("standing place", e)
            if live.on(): os.environ["KIT_LIVE_FILE"] = live._P[0]; live.note(threads=cmesh.threads(), steps_total=len(steps))          # the exe writes its tiles beside the note
            try: res = cmesh.build_c(base, steps, stand)
            finally: os.environ.pop("KIT_LIVE_FILE", None)
            if res is not None:
                v, f, ncells, sk = res; skipped += sk; got = (v, f)
                R0, H, rim_tan = base["R0"], base["H"], base["rim_tan"]
    if got is None:          # one core: the proven path (Python field, the C mesher)
        F, R0, H, rim_tan, mag, skipped = field_from_cmds(groups)
        if sp and sp[4] is not None:
            try: stand_flat(F, sp[4].buffer(1.2, 6), H, sp[0])
            except Exception as e: skipped.append(f"standing place: {type(e).__name__}"); __import__("kitlog").caught("standing place", e)
        live.note(stage="mesh", threads=1)
        got = cmesh.mesh(F, R0, H, rim_tan, mag)          # the C mesher (bbb_kit.exe): the same mesh, faster; None = use Python
        v, f = got if got is not None else mesh_from_field(F, R0, H, rim_tan, mag)
        ncells = len(F.cells)
    m = trimesh.Trimesh(v, f, process=False)
    m.merge_vertices()
    m, extra = add_solids(s, m, skipped, R0 - H * rim_tan - 0.12) if not (fin and abs(sc - 1) > 1e-6) else (m, [])
    if path is None:
        D = os.environ.get("SETDIR", gg3.CFG["out_folder"]); os.makedirs(f"{D}/{style}", exist_ok=True); path = f"{D}/{style}/{s.name}.stl"
    m.export(path)
    if gg3.CFG.get("drawing_2d", True) and not drawn:          # the 2D drawing of this very base beside its STL, to check the 3D against
        try: draw2d(s, path[:-4] + "_2d.png")
        except Exception as e: __import__("kitlog").caught("drawing from above", e)
    r = {"name": s.name, "secs": round(time.time() - t, 1), "skipped": skipped, "restarts": 0, "rock_fallbacks": 0, "sig": f"{style}{gg.BASE_MM:g}" + (f"o{ov[1]:g}" if ov else "") + sig,
         "variant": os.environ.get("RESEED", ""), "steps": len(groups), "slowest": "", "path": path, "cells": ncells, "tris": len(f), "watertight": bool(m.is_watertight),
         "top_mm": round(float(m.bounds[1][2]) - H, 1), "solids": len(extra), "mesher": ("C, " + cmesh.LAST.get("by", "")) if cmesh.LAST["used"] else ("Python" + (f" ({cmesh.LAST['error']})" if cmesh.LAST["error"] else ""))}
    try: r["want_mm"] = round(min(25.0, (max((float(op[3]) for op in s.ops if op[0] == "prism"), default=H) - H) * (sc if fin else 1.0)), 1)          # how high the tallest thing was designed to stand: the runner checks the mesh got there
    except Exception: r["want_mm"] = 0.0
    if sp and (ov or sp[1] / sp[0] < gg.BASE_MM - 0.05): r["want_mm"] = 0.0          # (an oval shows only the part of the round layout that lies on it)          # only a part of the layout is on the base: its tallest thing may be off it, so the height says nothing here
    r.update(gg3.scene_report(s)); r["errors"] = _kl.since(_km); return r




def draw2d(s, png, px=900, transparent=False):
    """the base seen from straight above as a colour drawing (what the kit laid out, before it became a mesh). Drawn
    with PIL (fast); draw2d_mpl is the older matplotlib drawing (settings.json "drawing_2d_mpl": true).
    transparent: no white round the base (the window's choice pictures)."""
    import gg3
    if gg3.CFG.get("drawing_2d_mpl", False): return draw2d_mpl(s, png, px, transparent)
    try: return _draw2d_pil(s, png, px, transparent)
    except Exception: return draw2d_mpl(s, png, px, transparent)


PLAN_WIN = (0.0, 0.0, 0.0, 0.0)


def _to_field(s, box):
    """a box in the layout's millimetres -> the built base's (build() rescales the layout when there is a scale plan)"""
    import gg
    sp = scale_plan(s); ov = gg.oval()
    if not sp or (ov and abs(sp[0] - 1) < 1e-6 and not sp[2] and not sp[3]): return box
    f, tx, ty = sp[0], sp[2], sp[3]
    return (box[0] * f + tx, box[1] * f + ty, box[2] * f + tx, box[3] * f + ty)


def _draw2d_pil(s, png, px=900, transparent=False):
    from PIL import Image, ImageDraw
    import gg
    SS = 2; W = px * SS; R0 = gg.R0
    cx, cy, half, clip = 0.0, 0.0, R0 + 1.0, ("circle", 0.0, 0.0, R0, R0); ring = None
    sp = scale_plan(s)
    if sp and gg.oval():                                    # an oval: just the oval part of the round layout
        sc, fin, tx, ty = sp[:4]; A, B = gg.oval(); ocx, ocy = -tx / sc, -ty / sc; w, h = A / sc, B / sc
        clip = ("ellipse", ocx, ocy, w / 2, h / 2); ring = clip; cx, cy, half = ocx, ocy, w / 2 * 1.03
    elif sp and sp[1] / sp[0] < gg.BASE_MM - 0.05:          # a figure scale: just the part that is on the base
        sc, fin, tx, ty = sp[:4]; r = fin / sc / 2.0; ocx, ocy = -tx / sc, -ty / sc
        clip = ("circle", ocx, ocy, r, r); ring = clip; cx, cy, half = ocx, ocy, r * 1.03
    k = W / (2 * half)
    global PLAN_WIN; PLAN_WIN = _to_field(s, (cx - half, cy - half, cx + half, cy + half))          # what this drawing shows, for the live view's tiles
    def X(x): return (x - (cx - half)) * k
    def Y(y): return ((cy + half) - y) * k
    def ell(c): return [X(c[1] - c[3]), Y(c[2] + c[4]), X(c[1] + c[3]), Y(c[2] - c[4])]
    img = Image.new("RGB", (W, W), "white"); d = ImageDraw.Draw(img)
    d.ellipse(ell(("c", 0.0, 0.0, R0 + 0.6, R0 + 0.6)), fill="#1d1b1a")
    lay = Image.new("RGB", (W, W), s.ground); dl = ImageDraw.Draw(lay)          # everything inside the base, clipped to it at the end
    def rings(p): return [[(X(x), Y(y)) for x, y in r.coords] for r in [p.exterior] + list(p.interiors)]
    def fill(p, colour):
        rs = rings(p)
        if len(rs[0]) < 3: return
        if len(rs) == 1: dl.polygon(rs[0], fill=colour); return
        xs = [x for x, _ in rs[0]]; ys = [y for _, y in rs[0]]; x0, y0 = int(max(0, min(xs))), int(max(0, min(ys))); x1, y1 = int(min(W, max(xs) + 2)), int(min(W, max(ys) + 2))
        if x1 <= x0 or y1 <= y0: return
        m = Image.new("L", (x1 - x0, y1 - y0), 0); dm = ImageDraw.Draw(m)
        dm.polygon([(x - x0, y - y0) for x, y in rs[0]], fill=255)
        for h in rs[1:]:
            if len(h) >= 3: dm.polygon([(x - x0, y - y0) for x, y in h], fill=0)
        lay.paste(colour, (x0, y0, x1, y1), m)
    def shadow(p, dx, dy):          # black at 35 %: the ground under it darkened
        rs = rings(p); ox, oy = dx * k, -dy * k
        xs = [x + ox for x, _ in rs[0]]; ys = [y + oy for _, y in rs[0]]
        if len(xs) < 3: return
        x0, y0 = int(max(0, min(xs))), int(max(0, min(ys))); x1, y1 = int(min(W, max(xs) + 2)), int(min(W, max(ys) + 2))
        if x1 <= x0 or y1 <= y0: return
        m = Image.new("L", (x1 - x0, y1 - y0), 0); dm = ImageDraw.Draw(m)
        dm.polygon([(x + ox - x0, y + oy - y0) for x, y in rs[0]], fill=89)
        for h in rs[1:]:
            if len(h) >= 3: dm.polygon([(x + ox - x0, y + oy - y0) for x, y in h], fill=0)
        box = (x0, y0, x1, y1); lay.paste(Image.composite(Image.new("RGB", m.size, "black"), lay.crop(box), m), box)
    for ps, dep, kind in s.final_gcuts():
        for p in ps: fill(p, gg.CUTC.get(kind, "#222222"))
    _chopk = {id(q): c["kind"] for c in getattr(s, "chops", []) for op in c.get("ops", []) for q in op[1]}          # a chamfered solid's outlines -> its own kind
    lw = max(1, int(round(0.3 / 72 * (px / 3) * SS)))
    for op in s.ops:
        if op[0] == "prism": _, ps, z0, top, kind = op; tier = z0 > gg.H
        else: _, ps, z0, hh, a, kind = op; top = z0 + hh; tier = z0 > gg.H - gg.SINK + 0.01
        if kind == "rockc": kind = next((_chopk[id(q)] for q in ps if id(q) in _chopk), kind)          # a chamfered solid's placeholder: drawn in the solid's own material, not the placeholder's orange
        kk = 0.78 + 0.13 * (top - gg.H); col = gg.COL.get(kind, "#8a8a8a")
        for p in ps:
            if p is None or p.is_empty: continue
            if not tier: shadow(p, .2 + .12 * (top - gg.H), -.2 - .12 * (top - gg.H))
            fill(p, gg.shade(col, kk))
            for r in rings(p): dl.line(r, fill=gg.shade(col, .55), width=lw)
    for ps, z0, z1, kind in s.tcuts:
        for p in ps: fill(p, gg.CUTC.get(kind, "#111111"))
    m = Image.new("L", (W, W), 0); ImageDraw.Draw(m).ellipse(ell(clip), fill=255)
    if clip[0] == "circle" and ring is None: m2 = m
    else: m2 = m
    img.paste(lay, (0, 0), m2)
    if ring is not None: d.ellipse(ell(ring), outline="#1a1a1a", width=max(2, int(3 / 72 * (px / 3) * SS)))
    if transparent:          # only the base itself is drawn
        a = Image.new("L", (W, W), 0); ImageDraw.Draw(a).ellipse(ell(("c", 0.0, 0.0, R0 + 0.6, R0 + 0.6)), fill=255)
        if ring is not None: a = Image.new("L", (W, W), 0); ImageDraw.Draw(a).ellipse(ell((ring[0], ring[1], ring[2], ring[3] * 1.01, ring[4] * 1.01)), fill=255)
        img = img.convert("RGBA"); img.putalpha(a)
    img.resize((px, px), Image.LANCZOS).save(png)


def draw2d_mpl(s, png, px=900, transparent=False):
    """the older 2D drawing, with matplotlib"""
    import matplotlib; matplotlib.use("Agg"); import matplotlib.pyplot as plt, gg3
    fig, ax = plt.subplots(figsize=(3, 3), dpi=px / 3); gg3.render(s, ax); ax.set_title(""); ax.set_axis_off(); fig.subplots_adjust(0, 0, 1, 1)
    crop_to_scale(ax, s)
    fig.savefig(png, dpi=px / 3, **({"transparent": True} if transparent else {"facecolor": "white"})); plt.close(fig)


def crop_to_scale(ax, s=None):
    """with a scenery scale the finished base may show only a part of what was laid out: draw just that part"""
    import gg
    sp = scale_plan(s) if s is not None else None
    if sp and gg.oval():                                    # an oval: just the oval part of the round layout
        import matplotlib.patches as mp
        sc, fin, tx, ty = sp[:4]; A, B = gg.oval(); cx, cy = -tx / sc, -ty / sc; w, h = A / sc, B / sc
        ring = mp.Ellipse((cx, cy), w, h, fill=False, ec="#1a1a1a", lw=3, zorder=200); ax.add_patch(ring)
        for art in list(ax.patches) + list(ax.collections) + list(ax.lines) + list(ax.images):
            if art is not ring: art.set_clip_path(mp.Ellipse((cx, cy), w, h, transform=ax.transData))
        r = w / 2.0; ax.set_xlim(cx - r * 1.03, cx + r * 1.03); ax.set_ylim(cy - r * 1.03, cy + r * 1.03); return
    if not sp or sp[1] / sp[0] >= gg.BASE_MM - 0.05: return
    import matplotlib.patches as mp
    sc, fin, tx, ty = sp[:4]; r = fin / sc / 2.0; cx, cy = -tx / sc, -ty / sc; ring = mp.Circle((cx, cy), r, fill=False, ec="#1a1a1a", lw=3, zorder=200); ax.add_patch(ring)
    for art in list(ax.patches) + list(ax.collections) + list(ax.lines) + list(ax.images):
        if art is not ring: art.set_clip_path(mp.Circle((cx, cy), r, transform=ax.transData))
    ax.set_xlim(cx - r * 1.03, cx + r * 1.03); ax.set_ylim(cy - r * 1.03, cy + r * 1.03)


def pic_by():
    """what drew the last 3D picture: the video card's name, or Python"""
    import cmesh
    by = cmesh.SHOT_LAST.get("by", "")
    return by if by and not by.startswith("failed") else "Python" + (f" ({by})" if by else "")


def shot(mesh_or_path, png, w=900, h=560, elev=30, azim=-60):
    """a shaded picture of the base seen from the side at `elev` degrees above the table (30 by default, so heights
    show). Drawn with its own depth buffer: what is in front always hides what is behind."""
    import trimesh
    from PIL import Image
    m = trimesh.load(mesh_or_path, process=False) if isinstance(mesh_or_path, str) else mesh_or_path
    V = np.asarray(m.vertices, dtype=float); Fc = np.asarray(m.faces)
    import cmesh
    im = cmesh.shot(V, Fc, w, h, elev, azim)          # the video card (the window's Video card switch); None = draw it here
    if im is not None: im.save(png); return
    e_, a_ = math.radians(elev), math.radians(azim)
    cam = np.array([math.cos(e_) * math.cos(a_), math.cos(e_) * math.sin(a_), math.sin(e_)])        # towards the viewer
    right = np.array([-math.sin(a_), math.cos(a_), 0.0]); upv = np.cross(cam, right)
    T = V[Fc]; N = np.cross(T[:, 1] - T[:, 0], T[:, 2] - T[:, 0]); ln = np.linalg.norm(N, axis=1); keep = ln > 1e-12
    T = T[keep]; N = N[keep] / ln[keep][:, None]
    vis = N @ cam > 1e-6; T = T[vis]; N = N[vis]
    L = np.array([-0.45, -0.35, 0.82]); L /= np.linalg.norm(L)
    shade = 0.30 + 0.70 * np.clip(N @ L, 0, 1)
    X = T @ right; Y = T @ upv; Z = T @ cam
    r = max(float(np.abs(V[:, :2]).max()), 1.0); sc = min((w - 20) / (2 * r), (h - 20) / (2 * r * math.sin(e_) + 10 * math.cos(e_)))
    cy = (Y.max() + Y.min()) / 2
    PX = w / 2 + X * sc; PY = h / 2 - (Y - cy) * sc
    img = np.full((h, w), 1.0); zb = np.full((h, w), -1e9)
    x0 = np.clip(np.floor(PX.min(axis=1)).astype(int), 0, w - 1); x1 = np.clip(np.ceil(PX.max(axis=1)).astype(int) + 1, 0, w)
    y0 = np.clip(np.floor(PY.min(axis=1)).astype(int), 0, h - 1); y1 = np.clip(np.ceil(PY.max(axis=1)).astype(int) + 1, 0, h)
    for i in range(len(T)):
        if x0[i] >= x1[i] or y0[i] >= y1[i]: continue
        ax, ay, bx, by, cx_, cy_ = PX[i, 0], PY[i, 0], PX[i, 1], PY[i, 1], PX[i, 2], PY[i, 2]
        d = (by - cy_) * (ax - cx_) + (cx_ - bx) * (ay - cy_)
        if abs(d) < 1e-12: continue
        xs = np.arange(x0[i], x1[i]) + 0.5; ys = np.arange(y0[i], y1[i]) + 0.5; GX, GY = np.meshgrid(xs, ys)
        w0 = ((by - cy_) * (GX - cx_) + (cx_ - bx) * (GY - cy_)) / d; w1 = ((cy_ - ay) * (GX - cx_) + (ax - cx_) * (GY - cy_)) / d; w2 = 1 - w0 - w1
        ins = (w0 >= -1e-6) & (w1 >= -1e-6) & (w2 >= -1e-6)
        if not ins.any(): continue
        zz = w0 * Z[i, 0] + w1 * Z[i, 1] + w2 * Z[i, 2]
        sub_z = zb[y0[i]:y1[i], x0[i]:x1[i]]; sub_i = img[y0[i]:y1[i], x0[i]:x1[i]]
        hit = ins & (zz > sub_z); sub_z[hit] = zz[hit]; sub_i[hit] = shade[i]
    rgb = np.dstack([img * 0.86, img * 0.84, img * 0.80]); rgb[zb < -1e8] = 1.0
    Image.fromarray((np.clip(rgb, 0, 1) * 255).astype(np.uint8)).save(png)


if __name__ == "__main__":
    import json
    if sys.argv[1] == "serve":                              # python mesher.py serve: stays running and builds one base per line
        sys.argv = sys.argv[:1]                             # it is sent, so Python and the libraries start once, not once per base
        import gg3                                          # noqa: load everything before the first request
        for line in sys.stdin:
            line = line.strip()
            if not line: continue
            req = json.loads(line); keep_env = {k: os.environ.get(k) for k in ("SALT", "RESEED", "BASENAME", "PLAN_JSON")}
            try:
                os.environ["RESEED"] = req.get("variant", ""); os.environ["BASENAME"] = req["name"]
                if req.get("plan"): os.environ["PLAN_JSON"] = json.dumps(req["plan"])          # this base's scene plan, from the designer
                r = build(req["style"], int(req["n"]))
                import live; live.note(stage="picture")
                try: shot(r["path"], r["path"][:-4] + "_iso.png"); r["pictures"] = pic_by()
                except Exception as e: r["note"] = f"no picture: {e}"
            except Exception as e:
                r = {"error": f"crash: {type(e).__name__}: {str(e)[:100]}"}
            finally:
                for k, v in keep_env.items():
                    if v is None: os.environ.pop(k, None)
                    else: os.environ[k] = v
            sys.stdout.write(json.dumps(r) + "\n"); sys.stdout.flush()
    elif sys.argv[1] == "build":                            # python mesher.py build <style> <n>   (one base, then exit)
        st, n = sys.argv[2], int(sys.argv[3]); sys.argv = sys.argv[:1]
        r = build(st, n)
        try: shot(r["path"], r["path"][:-4] + "_iso.png"); r["pictures"] = pic_by()
        except Exception as e: r["note"] = f"no picture: {e}"
        print(json.dumps(r))
    else:
        st, n, out = sys.argv[1], int(sys.argv[2]), (sys.argv[3] if len(sys.argv) > 3 else None); sys.argv = sys.argv[:1]
        print(build(st, n, out))
