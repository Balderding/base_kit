"""The ground of a large base (90 mm and up, built at the miniature's true scale).

At true scale a big base carries a big model, and small things are left out there on purpose, so the base is mostly
ground. Laid flat, that ground reads as a wide plain with a pattern on it. This file gives it real terrain at the
scale of the base: low rises and ridges, gullies with banks, rock shelves and outcrops, pools and mud, rubble fields,
roots, drifts... each style in its own character (RECIPES below).

scenes.compose() calls apply(sc) once, after the miniature's standing place and the clear middle are held and before
anything else is set down. Everything here is laid out in layout millimetres (the scene is built 3 times larger): a
rise 1 mm high here is 3 mm high on the print. The rules:
  - the clear middle (s.keepout) and the miniature's standing spots stay free and flat;
  - the surface stays a height map (nothing overhangs): rises are stacked sloped bands, shelves are stacked slabs;
  - heights follow the distance from the middle: low near the miniature (it may be a war machine whose hull hangs
    over the ground), up to about 4 mm (12 mm printed) only out towards the edge;
  - cuts are shallow (the base is only so thick): a gully reads deep because it has banks.
Only scenes.compose() calls this, and only for finished bases of 90 mm and up with a scenery scale."""
import math, os, random

from shapely.geometry import Point, Polygon, LineString, MultiPoint
from shapely.ops import unary_union, voronoi_diagram

ROCK_T = ((0.7, 6), (0.3, 40))
PEB_T = ((0.7, 10), (0.3, 45))
SOFT_KINDS = {}          # this file's soft ground kinds: name -> colour (registered as terrain, see _kinds)


def _u(gs):
    gs = [g for g in gs if g is not None and not g.is_empty]
    return unary_union(gs) if gs else Polygon()


def _one_line(g):
    """an offset curve as one line: when the offset splits into pieces (a tight bend), its longest piece; a single
    line comes back as the same line it always was"""
    if g.geom_type == "MultiLineString": g = max(g.geoms, key=lambda q: q.length)
    return LineString(g.coords)


def _biggest(g, polys_of):
    ps = polys_of(g)
    return max(ps, key=lambda q: q.area) if ps else None


def _shade(hexcol, f):
    """a colour a little lighter (f > 1) or darker (f < 1)"""
    h = hexcol.lstrip("#")
    try: r, g, b = (int(h[i:i + 2], 16) for i in (0, 2, 4))
    except ValueError: return "#888888"
    c = [max(0, min(255, int(v * f))) for v in (r, g, b)]
    return "#%02x%02x%02x" % tuple(c)


class Ground:
    def __init__(self, sc):
        import gg3, scenes
        self.sc, self.s, self.G, self.S = sc, sc.s, gg3, scenes
        self.gg = gg3.gg
        s = self.s
        import features
        fin = float(os.environ.get("KIT_FINAL_MM") or 0); ts = features.true_scale()
        self.R = float(self.gg.R0)
        if fin > 0 and ts > 0: self.R = min(self.R, fin / (2 * ts))          # under 90 mm the layout is larger than the base: work in the part that is built
        self.HS = 1.0 if fin >= 89.9 or fin <= 0 else max(0.4, (fin / 90.0) ** 1.3)          # and lower: 12 mm at the edge of a 90 mm base, about 7 mm on 60 mm, 5.5 mm on 50 mm
        self.H = float(gg3.H); self.SINK = float(gg3.SINK); self.LAP = float(getattr(self.gg, "LAP", 0.0))
        self.terr = gg3.terr(s)
        self.rng = random.Random(f"ground_big{sc.style}{s.name}{os.environ.get('SALT', '')}{os.environ.get('RESEED', '')}")
        self.A = max(0.3, (self.R / 26.67) ** 2)          # the share of a 160 mm base's room this base has
        stand = getattr(s, "stand", None) or {}
        pads = [p for p in (stand.get("pads") or []) if p is not None and not p.is_empty]
        body = stand.get("body")
        keep = list(getattr(s, "keepout", []))
        self.free = _u(keep + [p.buffer(1.6) for p in pads] + ([body.buffer(0.4)] if body is not None and not body.is_empty else []))
        self.body = body.buffer(2.5) if body is not None and not body.is_empty else Polygon()
        self.rk = max([math.sqrt(k.area / math.pi) for k in keep] + [0.0])          # the clear middle's radius
        wet = [p for ps, d, k in s.gcuts if (k in scenes.WETK or k in ("pit", "hollow", "pothole")) for p in ps if p.area > 2.0]
        self.wet_all = _u(wet); big = 0.2 * math.pi * self.R * self.R
        self.wet = _u([p for p in wet if p.area < big])          # a pool or a pit blocks raised ground; a lake or a marsh may have islands
        try: self.plates = [(float(z), u) for z, u in scenes._plates(s)]          # floors: (top, outline), highest first
        except Exception: self.plates = []
        self.mine = []           # footprints laid here (they keep apart)
        self.rises = []          # (polygon, height, run, profile exponent, foot z): the ground's own height there
        self.disc = Point(0, 0).buffer(self.R - 0.6, 96)
        self.made = 0
        self.floors = _u([u.buffer(0.5, 8) for z, u in self.plates])          # the style's floors: no terrain goes over them (it wiped their joints and buried what stood there)
        self.on_floors = False          # True for a floored style's own recipe (slabs and rubble meant for a floor)

    def floor_z(self, g):
        """the top of the floor (deck plates, flagstones, planks) that covers most of g, else the bare ground"""
        for z, pl in self.plates:
            try:
                if pl.intersection(g).area > 0.3 * g.area: return max(self.H, z)
            except Exception as e: __import__("kitlog").caught("ground_big.floor_z", e)
        return self.H

    # ------------------------------------------------------------------------------------------- where things may go
    def hmax(self, g):
        """how tall a thing with footprint g may stand: low beside the miniature, tall only out at the edge"""
        u = g.distance(Point(0, 0)) / self.R
        if not self.body.is_empty and g.intersects(self.body): return 0.7 * self.HS
        return self.HS * self._hmax(u)

    def _hmax(self, u):
        pts = ((0.0, 0.45), (0.5, 0.5), (0.62, 0.9), (0.72, 1.5), (0.82, 2.5), (0.92, 4.0))
        for (u0, h0), (u1, h1) in zip(pts, pts[1:]):
            if u <= u1: return h0 + (h1 - h0) * (u - u0) / (u1 - u0) if u >= u0 else h0
        return 4.0

    def ok(self, g, gap=0.8, wet=True, inside=0.55):
        if g is None or g.is_empty or not g.is_valid: return False
        if g.intersects(self.free): return False
        w = self.wet_all if wet == "all" else self.wet
        if wet and not w.is_empty and g.intersects(w.buffer(0.5)): return False
        a = g.area
        if a <= 0 or g.intersection(self.disc).area < inside * a: return False
        if not self.on_floors and not self.floors.is_empty and g.intersection(self.floors).area > 0.05 * a: return False          # open ground only
        return not any(g.distance(m) < gap for m in self.mine)

    def spot(self, u0, u1, t=None):
        """a point between u0 and u1 of the radius (outside the clear middle)"""
        lo = max(u0 * self.R, self.rk + 1.0)
        hi = max(lo + 0.5, u1 * self.R)
        t = self.rng.uniform(0, 2 * math.pi) if t is None else t
        d = math.sqrt(self.rng.uniform(lo * lo, hi * hi))
        return d * math.cos(t), d * math.sin(t), t

    def try_place(self, make, u0, u1, tries=40, **kw):
        for _ in range(tries):
            x, y, t = self.spot(u0, u1)
            got = make(x, y, t)
            if got is None: continue
            foot = got[0]
            if self.ok(foot, **kw): return got
        return None

    def clear_under(self, g):
        """small loose pieces and grit under a new piece of ground go, and so do the top cuts there (floor joints,
        seams): a top cut takes everything above its floor, so one left under a rise would groove it right through"""
        try: self.S.clear_small(self.s, g.buffer(0.3)); self.G.evict(self.s, g)
        except Exception as e: __import__("kitlog").caught("ground_big.clear_under", e)
        gb = g.buffer(0.3); out = []
        for ps, z0, z1, k in self.s.tcuts:
            qs = [q for p in ps for q in (self.G.polys_of(p.difference(gb)) if p.intersects(gb) else [p]) if q.area > 0.02]
            if qs: out.append((qs, z0, z1, k))
        self.s.tcuts = out

    def take(self, g): self.mine.append(g); self.made += 1

    def K(self, kind):
        """this file's own name for a material: the same colour (and bevel), but counted as terrain, so the scene
        neither prunes it as a loose piece nor takes it away with a small thing it happens to lie over"""
        if kind.startswith("gb_"): return kind
        base = kind[:-2] if kind.endswith(("_p", "_s")) else kind
        name = "gb_" + base; gg = self.gg
        gg.COL.setdefault(name, gg.COL.get(base, gg.COL.get(kind, "#8a8a8a")))
        if base in gg.CHAMFER_PRISMS: gg.CHAMFER_PRISMS.add(name)
        self.S.TERRK.add(name)
        return name

    # ------------------------------------------------------------------------------------------- shapes
    def blob(self, cx, cy, rx, ry, a, lumps=6, rough=0.25):
        """an organic outline about an ellipse rx x ry turned by a"""
        rng = self.rng; c, s_ = math.cos(a), math.sin(a); gs = []
        for _ in range(lumps):
            u, v = rng.uniform(-0.55, 0.55) * rx, rng.uniform(-0.5, 0.5) * ry
            r = min(rx, ry) * rng.uniform(0.55, 0.85)
            e = Point(0, 0).buffer(1.0, 20)
            from shapely import affinity
            e = affinity.scale(e, r * rx / min(rx, ry) * rng.uniform(0.6, 0.9), r * rng.uniform(0.9, 1.1))
            e = affinity.rotate(e, math.degrees(a) + rng.uniform(-15, 15), origin=(0, 0))
            gs.append(affinity.translate(e, cx + u * c - v * s_, cy + u * s_ + v * c))
        g = _u(gs).buffer(rough * min(rx, ry), 8).buffer(-rough * min(rx, ry), 8).simplify(0.1)
        return _biggest(g, self.G.polys_of)

    def path(self, p0, p1, amp, n=7):
        return LineString(self.G.wiggle(p0, p1, self.rng, amp, n))

    def ribbon(self, line, w0, w1, step=0.6, jit=0.15):
        """a band along a line, w0 wide at its start and w1 at its end"""
        L = line.length; gs = []; n = max(2, int(L / step))
        for i in range(n + 1):
            f = i / n; p = line.interpolate(f * L)
            gs.append(Point(p.x, p.y).buffer(max(0.2, (w0 + (w1 - w0) * f) / 2 * (1 + self.rng.uniform(-jit, jit))), 12))
        return _u(gs).buffer(0.25, 8).buffer(-0.25, 8).simplify(0.04)

    # ------------------------------------------------------------------------------------------- the ground's height
    @staticmethod
    def _prof(u, e): return 1 - (1 - u) ** e

    def z_at(self, g):
        """the height of the ground laid here under footprint g (its lowest point under g), H on flat ground"""
        z = self.H
        pts = [Point(c) for c in (g.exterior.coords if g.geom_type == "Polygon" else [g.centroid.coords[0]])]
        for poly, h, run, e, zb in self.rises:
            if not poly.intersects(g): continue
            lo = min((h * self._prof(min(1.0, poly.exterior.distance(p) / run), e) if poly.contains(p) else 0.0) for p in pts)
            z = max(z, zb + lo)
        return z

    # ------------------------------------------------------------------------------------------- builders
    def rise(self, poly, h, kind, run=None, e=1.7, m=None):
        """a smooth rise of ground: sloped bands one inside the other, steep at the foot and easing to a crown. Real
        terrain on the stepped-bedrock styles, so things laid later can stand on it."""
        G = self.G; s = self.s; H = self.H
        if poly is None or poly.is_empty: return False
        poly = Polygon(poly.exterior).simplify(0.04)
        if not poly.is_valid or poly.area < 1.0: return False
        kind = self.K(kind)
        inr = 2 * poly.area / poly.length; run0 = run
        run = min(run0 if run0 is not None else 2.6 * h + 0.8, 0.92 * inr)
        crown = G.inset(poly, run * 0.7); zb = self.floor_z(poly); lift = zb - H
        h = min(h + lift * 0.6, max(0.3, self.hmax(crown if not crown.is_empty else poly) - lift))          # the limit is read where the rise is tall, counted from the ground
        run = min(run0 if run0 is not None else 2.6 * h + 0.8, 0.92 * inr)
        if h < 0.15 + lift * 0.3 or run < 0.3: return False
        m = m or max(4, min(6, int(h / 0.3) + 3))          # few bands: every band is a sloped piece the mesh builder must cut into all it crosses
        cur = [poly.simplify(0.12)]; first = True; built = False
        for k in range(m):
            if not cur: break
            z0, z1 = h * self._prof(k / m, e), h * self._prof((k + 1) / m, e); hh = z1 - z0; dd = run / m
            if hh < 0.015: break
            ok = [q for q in (self.gg.fit_for_taper(p, dd + 0.03) for p in cur) if q is not None]
            if not ok: break
            sink = self.SINK if first else self.LAP
            s.ops.append(("taper", ok, round(zb + z0 - sink, 4), round(hh + sink, 4), round(math.degrees(math.atan(dd / hh)), 3), kind))
            tops = [t.simplify(0.1) for q in ok for t in G.polys_of(G.inset(q, dd)) if t.area > 0.25]
            if self.terr: G.gg2.T_add(s, _u(ok), zb + z0, zb + z1, _u(tops))
            if first: s.feet += ok; first = False
            built = True; cur = tops
        if built: self.rises.append((poly, h, run, e, zb))
        return built

    def shelf(self, poly, h, kind, steps=2, drift=(0.0, 0.0), cracks=True):
        """a rock shelf or mesa: flat slabs stacked one on another, each smaller and higher, steep sides"""
        G = self.G; s = self.s; H = self.H; rng = self.rng
        if h < 0.3: return False
        kd = self.K(kind); cur = poly; z = H; built = False; inr = 2 * poly.area / poly.length; made = []
        from shapely import affinity
        for k in range(steps):
            zt = H + h * (k + 1) / steps
            if k:
                d = inr * rng.uniform(0.22, 0.34)
                cur = _biggest(G.inset(affinity.translate(cur, drift[0] * d, drift[1] * d), d), G.polys_of)
                if cur is None or cur.area < 1.5: break
                cur = cur.buffer(0.3, 6).buffer(-0.3, 6).simplify(0.05)
            zt = min(zt, H + self.hmax(cur))          # each step as tall as its own place allows: the shelf climbs towards the edge
            if zt <= z + 0.25: break
            ps = s.prism([cur], zt, kd, z0=(H - self.SINK) if k == 0 else z - 0.15, foot=(k == 0))
            if not ps: break
            if self.terr: G.gg2.T_add(s, _u(ps), H, zt, G.inset(_u(ps), 0.4))
            built = True; made.append((cur, zt)); z = zt
        for i, (cur, zt) in enumerate(made if cracks else []):          # cracks in each step's top, only where no step stands on it
            if cur.area < 6: continue
            cs = []
            for _ in range(rng.randint(1, 3)):
                c = cur.representative_point(); a = rng.uniform(0, math.pi); L = math.sqrt(cur.area) * rng.uniform(0.6, 1.1)
                cs.append(LineString(G.wiggle((c.x - L / 2 * math.cos(a), c.y - L / 2 * math.sin(a)), (c.x + L / 2 * math.cos(a), c.y + L / 2 * math.sin(a)), rng, 0.4, 5)).buffer(0.17, 2))
            g = _u(cs).intersection(G.inset(cur, 0.45))
            if i + 1 < len(made): g = g.difference(made[i + 1][0].buffer(0.4))
            if not g.is_empty: s.tcut(G.polys_of(g), zt - 0.3, zt + 0.3, "crack")
        if built: self.rises.append((poly, h * 0.5, 0.3, 1.0, H))
        return built

    def rocks(self, region, n, r0, r1, h0, h1, kind, gap=0.25, tiers=ROCK_T, tries=None):
        """loose rocks in a region, each set down on whatever ground has been laid there"""
        G = self.G; s = self.s; rng = self.rng; got = []
        if region is None or region.is_empty: return 0
        x0, y0, x1, y1 = region.bounds; kd = self.K(kind)
        for _ in range(tries or n * 25):
            if len(got) >= n: break
            r = rng.uniform(r0, r1); p = self.gg.stone(rng.uniform(x0, x1), rng.uniform(y0, y1), r, rng, rng.uniform(0.6, 0.95), rng.randint(5, 8))
            if not region.contains(p) or p.intersects(self.free) or any(p.distance(q) < gap for q in got): continue
            if not self.wet_all.is_empty and p.intersects(self.wet_all): continue
            got.append(p)
        for p in got:
            z = self.z_at(p); h = min(rng.uniform(h0, h1), max(0.25, self.hmax(p) + 0.4 - (z - self.H)))
            if self.terr: G.tstack(s, [p], h, tiers, kd); continue
            if z <= self.H + 1e-6: s.stack([p], h, tiers, kd)
            else: s.stack([p], h, tiers, kd, z0=z)
        return len(got)

    def cut(self, g, depth, kind):
        if g is None or g.is_empty: return
        g = g.difference(self.free.buffer(0.3))
        if g.is_empty: return
        try: self.S.lay_cut(self.sc, g, depth, kind, meet=True)
        except Exception: self.s.gcut(self.G.polys_of(g), depth, kind)

    def hills(self): return _u([r[0].buffer(0.45) for r in self.rises])

    def _strips(self, g, step):
        from shapely.geometry import box as _box
        x0, y0, x1, y1 = g.bounds; out = []; x = x0
        while x < x1:
            y = y0
            while y < y1: out += self.G.polys_of(g.intersection(_box(x, y, x + step, y + step))); y += step
            x += step
        return out

    def carve(self, g, depth, kind):
        """a cut that follows the ground laid here: on flat ground an ordinary ground cut; where it crosses a rise, cut
        down from the rise's own surface in short steps, so a gully runs through a hill and a pool lies in a dip.
        Returns the part that was cut into flat ground."""
        if g is None or g.is_empty: return Polygon()
        g = g.difference(self.free.buffer(0.3))
        if g.is_empty: return Polygon()
        hill = self.hills()
        on = g.intersection(hill) if not hill.is_empty else Polygon()
        flat = g.difference(hill) if not hill.is_empty else g
        if not flat.is_empty: self.cut(flat, depth, kind)
        if not on.is_empty:
            groups = {}
            for q in self._strips(on, 2.0):
                if q.area < 0.05: continue
                z0 = max(self.H - 0.37, self.z_at(q) - depth)
                groups.setdefault(max(self.H - 0.37, round(z0 / 0.12) * 0.12), []).append(q)          # steps of 0.12 mm: neighbouring strips at one level merge into one cut
            for z0, qs in groups.items():
                qs = [x.simplify(0.05) for x in self.G.polys_of(_u([x.buffer(0.01) for x in qs]))]
                self.s.tcut(qs, z0, z0 + depth + 6.0, kind)
        return flat

    def ripples(self, pool, kind, n=3):
        """low rings standing in a pool (ripples, scum lines), only on bare ground"""
        if self.terr or self.H > 0 and any(pool.intersects(pl) for z, pl in self.S._plates(self.s)): return
        G = self.G; rings = []
        for k in range(1, n + 1):
            a = G.inset(pool, 0.55 * k); b = G.inset(pool, 0.55 * k + 0.32)
            if a.is_empty or b.is_empty: break
            rings += [q for q in G.polys_of(a.difference(b)) if q.area > 0.3]
        if rings:
            self.gg.COL.setdefault(kind + "_pad", self.gg.COL.get(kind, "#406070"))
            self.s.prism(rings, self.H - 0.2, kind + "_pad", z0=self.H - 0.8, foot=False)

    def slabs(self, region, h0, h1, kind, n=6, tilt=None, gap=0.3, joints=True):
        """a region broken into slabs, each raised its own height (heaved paving, buckled deck plates, ice floes).
        With tilt=(x, y, a) the slabs climb towards a crest line through (x, y) at angle a."""
        G = self.G; s = self.s; rng = self.rng; H = self.H
        x0, y0, x1, y1 = region.bounds
        pts = MultiPoint([(rng.uniform(x0, x1), rng.uniform(y0, y1)) for _ in range(n)])
        kd = self.K(kind); built = 0; zf = self.floor_z(region); lift = zf - H
        from shapely.geometry import box
        cells = [c for c in voronoi_diagram(pts, envelope=box(x0 - 2, y0 - 2, x1 + 2, y1 + 2)).geoms]
        span = max(x1 - x0, y1 - y0) / 2
        for c in cells:
            q = _biggest(G.inset(c.intersection(region), gap / 2), G.polys_of)
            if q is None or q.area < 1.2 or not G.wide_enough(q, 1.2): continue
            if tilt is not None:
                tx, ty, ta = tilt; d = abs(-(q.centroid.x - tx) * math.sin(ta) + (q.centroid.y - ty) * math.cos(ta))
                h = h0 + (h1 - h0) * max(0.0, 1 - d / max(0.5, span * 0.8)) + rng.uniform(-0.08, 0.08)
            else: h = rng.uniform(h0, h1)
            h = min(h + lift, max(lift + 0.3, self.hmax(q)))
            if h < 0.2 + lift: continue
            ps = s.prism([q], H + h, kd)
            if not ps: continue
            built += 1
            if self.terr: G.gg2.T_add(s, _u(ps), H, H + h, G.inset(_u(ps), 0.4))
            if joints and q.area > 5 and rng.random() < 0.6:           # a crack across the slab
                cc = q.representative_point(); a = rng.uniform(0, math.pi); L = math.sqrt(q.area)
                g = LineString(G.wiggle((cc.x - L * math.cos(a), cc.y - L * math.sin(a)), (cc.x + L * math.cos(a), cc.y + L * math.sin(a)), rng, 0.3, 4)).buffer(0.16, 2).intersection(G.inset(q, 0.35))
                if not g.is_empty: s.tcut(G.polys_of(g), H + h - 0.3, H + h + 0.3, "crack")
        if built: self.rises.append((region, min(h0, h1), 0.3, 1.0, zf))
        return built

    def crack_patch(self, region, n, depth=0.3, kind="crack", w=0.2):
        """a cracked flat: a web of fine cracks over a patch of ground"""
        G = self.G; rng = self.rng; x0, y0, x1, y1 = region.bounds
        pts = MultiPoint([(rng.uniform(x0, x1), rng.uniform(y0, y1)) for _ in range(n)]); segs = []
        from shapely.geometry import box
        for cell in voronoi_diagram(pts, envelope=box(x0 - 1, y0 - 1, x1 + 1, y1 + 1)).geoms:
            c = list(cell.exterior.coords)
            for a, b in zip(c, c[1:]):
                if rng.random() < 0.8: segs.append(LineString(G.wiggle(a, b, rng, 0.25, 3)).buffer(w, 2))
        g = _u(segs).intersection(G.inset(region, 0.3))
        if not g.is_empty: self.carve(g, depth, kind)

    # ------------------------------------------------------------------------------------------- compound features
    def f_rise(self, kind, r=(0.12, 0.22), h=(0.4, 1.0), u=(0.5, 0.95), elong=(1.0, 2.2), rocks=None, inside=0.6):
        R = self.R
        def mk(x, y, t):
            rx = R * self.rng.uniform(*r); a = t + math.pi / 2 + self.rng.uniform(-0.6, 0.6)
            g = self.blob(x, y, rx * self.rng.uniform(*elong) ** 0.5, rx / self.rng.uniform(*elong) ** 0.5, a)
            return (g,) if g is not None else None
        got = self.try_place(mk, *u, inside=inside)
        if not got: return False
        g = got[0]; self.clear_under(g)
        if not self.rise(g, self.rng.uniform(*h), kind): return False
        self.take(g)
        if rocks: self.rocks(g.buffer(-0.4), *rocks)
        return True

    def f_ridge(self, kind, L=(0.35, 0.6), w=(2.0, 3.5), h=(0.6, 1.4), u=(0.6, 0.95), rocks=None, tang=True, run=None, e=1.7):
        """a long ridge (dune, drift, bank, fold, berm) lying round the miniature, not at it"""
        R = self.R; rng = self.rng
        def mk(x, y, t):
            a = t + (math.pi / 2 if tang else 0) + rng.uniform(-0.5, 0.5); Ln = R * rng.uniform(*L)
            p0 = (x - Ln / 2 * math.cos(a), y - Ln / 2 * math.sin(a)); p1 = (x + Ln / 2 * math.cos(a), y + Ln / 2 * math.sin(a))
            ww = rng.uniform(*w); line = self.path(p0, p1, Ln * 0.08, 6)
            g = self.ribbon(line, ww * rng.uniform(0.5, 0.8), ww * rng.uniform(0.6, 1.0))
            return (_biggest(g, self.G.polys_of),) if g is not None and not g.is_empty else None
        got = self.try_place(mk, *u)
        if not got or got[0] is None: return False
        g = got[0]; self.clear_under(g)
        if not self.rise(g, rng.uniform(*h), kind, run=run, e=e): return False
        self.take(g)
        if rocks: self.rocks(g.buffer(-0.3), *rocks)
        return True

    def f_mesa(self, kind, r=(0.13, 0.2), h=(2.0, 3.8), steps=(2, 3), u=(0.8, 1.05), scree=None):
        """a tall rock shelf out at the edge, partly cut by the rim: a mesa, a cliff, a ledge"""
        R = self.R; rng = self.rng
        def mk(x, y, t):
            rx = R * rng.uniform(*r); g = self.blob(x, y, rx * rng.uniform(1.0, 1.6), rx * rng.uniform(0.7, 1.0), t + math.pi / 2, lumps=7, rough=0.12)
            if g is None: return None
            g = g.simplify(0.25)          # cliffs: straight-ish faces
            return (g, t)
        got = self.try_place(mk, *u, inside=0.4)
        if not got: return False
        g, t = got; self.clear_under(g)
        dr = (math.cos(t) * 0.5, math.sin(t) * 0.5) if rng.random() < 0.75 else (-math.sin(t) * 0.5, math.cos(t) * 0.5)          # mostly climbing towards the edge
        if not self.shelf(g, rng.uniform(*h), kind, rng.randint(*steps), drift=dr): return False
        self.take(g)
        if scree:          # broken rock fallen at the foot, on the side facing the middle
            c = g.centroid; foot = g.buffer(2.2).difference(g.buffer(0.3)).intersection(Point(c.x * 0.8, c.y * 0.8).buffer(math.sqrt(g.area) * 0.9))
            self.rocks(foot.difference(self.free), *scree)
        return True

    def f_outcrop(self, kind, r=(0.07, 0.12), h=(1.2, 3.2), n=(3, 6), u=(0.75, 1.0), soft=None):
        """a group of big rocks breaking through the ground, the tallest at its back"""
        R = self.R; rng = self.rng
        def mk(x, y, t):
            rr = R * rng.uniform(*r); return (Point(x, y).buffer(rr * 1.9, 16), x, y, rr, t)
        got = self.try_place(mk, *u, inside=0.5)
        if not got: return False
        g, x, y, rr, t = got; self.clear_under(g)
        if soft: self.rise(self.blob(x, y, rr * 1.9, rr * 1.4, t + math.pi / 2), 0.5, soft)
        kd = self.K(kind); k = rng.randint(*n); made = 0
        for i in range(k):
            b = rng.uniform(0, 2 * math.pi); d = rr * rng.uniform(0.0, 1.1) * (i > 0)
            cx, cy = x + d * math.cos(b), y + d * math.sin(b)
            p = self.gg.stone(cx, cy, rr * rng.uniform(0.45, 0.8) * (1.0 if i == 0 else 0.8), rng, rng.uniform(0.6, 0.9), rng.randint(5, 8))
            if p.intersects(self.free): continue
            back = math.hypot(cx, cy) / self.R          # further out, taller
            z = self.z_at(p)
            hh = min(rng.uniform(*h) * (0.6 + 0.5 * back), self.hmax(p) - (z - self.H))
            if hh < 0.4: continue
            if self.terr: self.G.tstack(self.s, [p], hh, ROCK_T, kd)
            elif z > self.H + 1e-6: self.s.stack([p], hh, ROCK_T, kd, z0=z)
            else: self.s.stack([p], hh, ROCK_T, kd)
            made += 1
        if made: self.take(g)
        return made > 0

    def f_gully(self, bed, bank, L=(0.35, 0.55), w=(1.6, 2.6), bank_h=(0.5, 1.1), u=(0.55, 1.05), pebbles=None, depth=0.36):
        """an eroded gully: a channel worn into the ground with a bank either side, running in from the edge"""
        R = self.R; rng = self.rng; G = self.G
        def mk(x, y, t):
            t0 = t; r0 = R * 1.04; p0 = (r0 * math.cos(t0), r0 * math.sin(t0))
            a = t0 + math.pi + rng.uniform(-0.9, 0.9); Ln = R * rng.uniform(*L)
            p1 = (p0[0] + Ln * math.cos(a), p0[1] + Ln * math.sin(a))
            line = self.path(p0, p1, Ln * 0.12, 8); ww = rng.uniform(*w)
            ch = self.ribbon(line, ww, ww * 0.35)
            ban = [self.ribbon(_one_line(line.offset_curve(sg * (ww * 0.5 + 1.3))), 2.0, 1.0) for sg in (-1, 1)]
            ban = [b.difference(ch.buffer(0.45)) for b in ban if b is not None and not b.is_empty]
            foot = _u([ch] + ban)
            return (foot, ch, ban)
        got = self.try_place(mk, *u, inside=0.5)
        if not got: return False
        foot, ch, ban = got; self.clear_under(foot)
        for b in ban:
            for q in G.polys_of(b):
                if q.area > 2.0: self.rise(q, rng.uniform(*bank_h), bank, e=1.4)
        flat = self.carve(ch, depth, bed)
        if pebbles and not flat.is_empty: self.rocks(G.inset(flat, 0.25), *pebbles, tiers=((1.0, 30),))
        self.take(foot)
        return True

    def f_pool(self, kind, r=(0.08, 0.15), u=(0.52, 0.9), depth=0.36, ripples=2, rim=None, rim_h=(0.3, 0.6)):
        """a pool, puddle or mud hole: a shallow cut with ripples standing in it and, if given, a lumpy rim"""
        R = self.R; rng = self.rng; G = self.G
        def mk(x, y, t):
            rx = R * rng.uniform(*r); g = self.blob(x, y, rx * rng.uniform(1.0, 1.5), rx, rng.uniform(0, math.pi))
            return (g.buffer(1.6) if g is not None and rim else g, g) if g is not None else None
        got = self.try_place(mk, *u, wet="all")
        if not got: return False
        foot, pool = got; self.clear_under(foot)
        if rim:
            lumps = []
            ring_ = pool.buffer(1.7).difference(pool.buffer(0.45))
            for _ in range(rng.randint(4, 7)):
                p = pool.exterior.interpolate(rng.uniform(0, pool.exterior.length))
                q = self.blob(p.x, p.y, rng.uniform(1.0, 1.8), rng.uniform(0.7, 1.1), rng.uniform(0, math.pi), lumps=3)
                if q is None: continue
                q = _biggest(q.intersection(ring_), G.polys_of)
                if q is not None and q.area > 1.0 and all(q.distance(o) > 0.2 for o in lumps): lumps.append(q)
            for q in lumps: self.rise(q, rng.uniform(*rim_h), rim, e=1.5)
        flat = self.carve(pool, depth, kind)
        if ripples and not flat.is_empty and flat.area > 0.9 * pool.area: self.ripples(pool, kind, ripples)
        self.wet = _u([self.wet, pool]); self.wet_all = _u([self.wet_all, pool])
        self.take(foot)
        return True

    def f_rubble(self, soft, stone, r=(0.1, 0.16), h=(0.6, 1.6), u=(0.62, 1.0), n=(10, 18), big=(0.8, 1.6)):
        """a rubble heap: a mound of broken stuff with chunks lying on and round it"""
        R = self.R; rng = self.rng
        def mk(x, y, t):
            rx = R * rng.uniform(*r); g = self.blob(x, y, rx * rng.uniform(1.0, 1.5), rx * rng.uniform(0.7, 1.0), t + math.pi / 2)
            return (g.buffer(1.0), g) if g is not None else None
        got = self.try_place(mk, *u)
        if not got: return False
        foot, g = got; self.clear_under(foot)
        self.rise(g, rng.uniform(*h), soft, e=1.4)
        self.rocks(g.buffer(0.8), rng.randint(*n), big[0], big[1], 0.4, 1.0, stone, tiers=ROCK_T)
        self.rocks(g.buffer(1.0), rng.randint(*n), 0.4, 0.7, 0.25, 0.45, stone, tiers=PEB_T)
        self.take(foot)
        return True

    def f_strands(self, kind, n=(3, 6), L=(0.35, 0.6), w=(1.6, 2.6), h=(0.7, 1.2), u=(0.85, 1.05), hub=None, hub_h=2.4):
        """roots, veins, cables: thick strands spreading from a point at the edge, thinning as they go"""
        R = self.R; rng = self.rng; G = self.G
        def mk(x, y, t):
            hx, hy = R * rng.uniform(*u) * math.cos(t), R * rng.uniform(*u) * math.sin(t); parts = []
            for i in range(rng.randint(*n)):
                a = t + math.pi + rng.uniform(-1.1, 1.1); Ln = R * rng.uniform(*L)
                line = self.path((hx, hy), (hx + Ln * math.cos(a), hy + Ln * math.sin(a)), Ln * 0.1, 7)
                parts.append(self.ribbon(line, rng.uniform(*w), 0.5))
            g = _u(parts).difference(self.free.buffer(0.8))
            return (g.buffer(0.5), g, hx, hy, t)
        got = self.try_place(mk, 0.9, 1.0, inside=0.4)
        if not got: return False
        foot, g, hx, hy, t = got; self.clear_under(foot)
        kd = self.K(kind)
        ps = [q for q in G.polys_of(g) if q.area > 0.6]
        if not ps: return False
        hh = rng.uniform(*h)
        for q in ps: self.rise(q, min(hh, self.hmax(q) + 0.3), kd, e=1.3)
        if hub:
            hb = self.blob(hx, hy, R * 0.09, R * 0.07, t, lumps=4)
            if hb is not None: self.rise(hb, hub_h, hub, e=1.2)
        self.take(foot)
        return True

    def f_slabs(self, kind, r=(0.12, 0.2), h=(0.3, 1.4), u=(0.6, 1.0), n=(5, 9), tilt=True):
        R = self.R; rng = self.rng
        def mk(x, y, t):
            rx = R * rng.uniform(*r); g = self.blob(x, y, rx * rng.uniform(1.1, 1.6), rx, t + math.pi / 2, rough=0.1)
            return (g, x, y, t) if g is not None else None
        got = self.try_place(mk, *u)
        if not got: return False
        g, x, y, t = got; self.clear_under(g)
        if not self.slabs(g, h[0], h[1], kind, rng.randint(*n), tilt=(x, y, t + math.pi / 2 + rng.uniform(-0.4, 0.4)) if tilt else None): return False
        self.take(g)
        return True

    def f_crater(self, rim, pit, r=(0.09, 0.14), u=(0.6, 0.95), rim_h=(0.6, 1.2), ejecta=None, depth=0.37, water=None):
        """a blast crater: a raised broken rim round a sunken middle, thrown chunks round it"""
        R = self.R; rng = self.rng; G = self.G
        def mk(x, y, t):
            rr = R * rng.uniform(*r); return (Point(x, y).buffer(rr + 2.6, 24), x, y, rr)
        got = self.try_place(mk, *u)
        if not got: return False
        foot, x, y, rr = got; self.clear_under(foot)
        hole = self.blob(x, y, rr, rr * 0.85, rng.uniform(0, math.pi), lumps=5, rough=0.2)
        if hole is None: return False
        ring_ = hole.buffer(2.2).difference(hole.buffer(0.35))
        a0 = rng.uniform(0, 2 * math.pi); n = rng.randint(3, 4)
        for k in range(n):          # the rim in arcs (each a rise of its own), so no rise has a hole
            a, b = a0 + k * 2 * math.pi / n + 0.12, a0 + (k + 1) * 2 * math.pi / n - 0.12
            wedge = Polygon([(x, y)] + [(x + (rr + 5) * math.cos(a + (b - a) * i / 8), y + (rr + 5) * math.sin(a + (b - a) * i / 8)) for i in range(9)])
            arc = _biggest(ring_.intersection(wedge), G.polys_of)
            if arc is not None and arc.area > 1.0: self.rise(arc, rng.uniform(*rim_h), rim, e=1.3)
        self.carve(hole, depth, pit)
        if water: self.carve(G.inset(hole, 0.8), depth, water)
        if ejecta: self.rocks(foot.difference(hole.buffer(0.3)), *ejecta)
        self.wet = _u([self.wet, hole]); self.wet_all = _u([self.wet_all, hole]); self.take(foot)
        return True

    def f_trench(self, bed, wall=None, w=(1.6, 2.2), L=(0.5, 0.8), u=(0.6, 0.8), pipe=None, depth=0.36):
        """a straight trench across the outer ground (a conduit run, a drain), with a raised lip and a pipe in it"""
        R = self.R; rng = self.rng; G = self.G
        def mk(x, y, t):
            a = t + math.pi / 2 + rng.uniform(-0.25, 0.25); Ln = R * rng.uniform(*L); ww = rng.uniform(*w)
            p0 = (x - Ln / 2 * math.cos(a), y - Ln / 2 * math.sin(a)); p1 = (x + Ln / 2 * math.cos(a), y + Ln / 2 * math.sin(a))
            line = LineString([p0, p1]); ch = line.buffer(ww / 2, cap_style=2)
            return (ch.buffer(1.4, cap_style=2), line, ch, ww)
        got = self.try_place(mk, *u, inside=0.5)
        if not got: return False
        foot, line, ch, ww = got; self.clear_under(foot)
        self.carve(ch, depth, bed)
        if wall:
            kd = self.K(wall)
            for sg in (-1, 1):
                lip = _one_line(line.offset_curve(sg * (ww / 2 + 0.55))).buffer(0.45, cap_style=2)
                self.s.prism([lip.difference(self.free.buffer(0.3))], self.H + 0.45, kd)
        if pipe and not self.terr and not any(ch.intersects(pl) for z, pl in self.S._plates(self.s)):
            pp = _one_line(line.offset_curve(rng.uniform(-0.2, 0.2) * ww)).buffer(min(0.5, ww * 0.25), cap_style=2).intersection(G.inset(ch, 0.25))
            if not pp.is_empty:
                self.gg.COL.setdefault(pipe + "_pad", self.gg.COL.get(pipe, "#6f747c"))
                self.s.prism(G.polys_of(pp), self.H - 0.05, pipe + "_pad", z0=self.H - 0.8, foot=False)
        self.take(foot)
        return True

    def f_cracked_flat(self, u=(0.5, 0.9), r=(0.14, 0.22), kind="crack", n=(10, 16)):
        R = self.R; rng = self.rng
        def mk(x, y, t):
            rx = R * rng.uniform(*r); return (self.blob(x, y, rx * 1.3, rx, t + math.pi / 2, rough=0.2),)
        got = self.try_place(mk, *u, gap=0.3, wet=False)
        if not got or got[0] is None: return False
        self.crack_patch(got[0], rng.randint(*n), 0.3, kind, 0.2); self.take(got[0])
        return True

    def field(self, kind, h=(0.3, 1.4), r=(0.19, 0.3), cover=0.7, n_max=22, rocks=None, elong=(1.2, 2.4)):
        """the lie of the land: broad low rises side by side over the ground round the clear middle, low near the
        miniature and higher towards the edge, so the base reads as rolling ground rather than a flat plain. They
        may run into one another (the surface takes the higher)."""
        R = self.R; rng = self.rng; G = self.G
        ring = self.disc.difference(self.free.buffer(0.6))
        if ring.is_empty: return 0
        want = cover * ring.area; done = Polygon(); centres = []; made = 0
        block = _u([m.buffer(0.3) for m in self.mine] + ([self.wet.buffer(0.7)] if not self.wet.is_empty else []) + ([self.floors] if not self.floors.is_empty else []))          # not over a floor
        for _ in range(n_max):
            best = None
            for _ in range(14):
                t = rng.uniform(0, 2 * math.pi); d = max(self.rk + 1.5, self.R * rng.uniform(0.5, 0.88)); x, y = d * math.cos(t), d * math.sin(t)
                if done.contains(Point(x, y)) and rng.random() < 0.8: continue
                d = min([math.hypot(x - a, y - b) for a, b in centres] + [1e9])
                if best is None or d > best[0]: best = (d, x, y, t)
            if best is None: continue
            _, x, y, t = best
            rx = R * rng.uniform(*r); el = rng.uniform(*elong) ** 0.5
            g = self.blob(x, y, rx * el, rx / el, t + math.pi / 2 + rng.uniform(-0.5, 0.5), lumps=7, rough=0.3)
            if g is None: continue
            g = g.difference(self.free.buffer(0.7))
            if not block.is_empty: g = g.difference(block)
            g = _biggest(g.buffer(-0.4, 8).buffer(0.4, 8), G.polys_of) if not g.is_empty else None
            if g is None or g.area < 6 or not G.wide_enough(g, 2.0): continue
            centres.append((x, y))
            u = min(1.0, max(0.0, (math.hypot(x, y) / R - 0.45) / 0.55))
            hh = (h[0] + (h[1] - h[0]) * u) * rng.uniform(0.75, 1.15)
            self.clear_under(g)
            if self.rise(g, hh, kind, e=1.6):
                made += 1; done = _u([done, g])
                if rocks and rng.random() < 0.4: self.rocks(G.inset(g, 0.6), *rocks)
            if done.area >= want: break
        self.made += made
        return made


# ================================================================================================== the styles
# A style: the lie of its land (field: kind, low and high height, how much of the ground it covers; None = none) and its
# features: (feature, how many on a 160 mm base, arguments). Counts shrink with the base's room. "floored" replaces
# the field and the features when most of the ground is a floor (deck plates, flagstones, planks).
def _kinds(gg, scenes, ground):
    """this file's soft ground kinds, coloured from the style's own ground and counted as terrain"""
    out = {}
    for name, f in (("gb_rise", 1.12), ("gb_bank", 0.92), ("gb_heap", 1.05)):
        gg.COL[name] = _shade(ground, f); out[name] = name
        try: scenes.TERRK.add(name)
        except Exception: pass
    return out


def STYLE(style):
    E, B, P = "gb_rise", "gb_bank", "gb_heap"
    pebbles = (6, 0.35, 0.6, 0.2, 0.35, "pebble")
    def scree(k): return (5, 0.5, 1.0, 0.3, 0.7, k)
    T = {
    "badlands":   dict(field=(E, 0.3, 1.9, 0.65), rocks=(2, 0.6, 1.1, 0.4, 0.8, "sandstone"),
                       feats=[("mesa", 2, dict(kind="sandstone", scree=scree("sandstone"))), ("gully", 2, dict(bed="wash", bank=B, pebbles=pebbles)),
                              ("cracked_flat", 2, dict(kind="mudcrack")), ("outcrop", 1, dict(kind="sandstone", soft=B))]),
    "highland":   dict(field=("moss", 0.4, 2.0, 0.7), rocks=(3, 0.5, 0.9, 0.3, 0.6, "slate"),
                       feats=[("mesa", 1, dict(kind="slate", steps=(3, 4), h=(1.6, 3.0), scree=scree("slate"))), ("outcrop", 2, dict(kind="slate", soft="moss")),
                              ("pool", 1, dict(kind="water", rim="moss")), ("gully", 1, dict(bed="water", bank=B, pebbles=pebbles))]),
    "maahl":      dict(field=(E, 0.3, 2.0, 0.6),
                       feats=[("ridge", 3, dict(kind=E, h=(0.8, 2.2), w=(3.0, 5.0), L=(0.4, 0.7))), ("mesa", 1, dict(kind="ruin", steps=(1, 2), h=(1.2, 2.2))),
                              ("cracked_flat", 1, dict(kind="mudcrack"))]),
    "corridor":   dict(field=None,
                       feats=[("slabs", 4, dict(kind="metal", h=(0.4, 1.6), r=(0.15, 0.25), n=(6, 10))), ("trench", 2, dict(bed="slot", wall="metal", pipe="pipe")),
                              ("rubble", 2, dict(soft=P, stone="metal", h=(0.5, 1.2), big=(0.6, 1.2))), ("ridge", 3, dict(kind="metal", h=(1.0, 1.8), w=(2.6, 3.6), e=1.0)),
                              ("crater", 1, dict(rim="metal", pit="slot", rim_h=(0.5, 1.0), ejecta=(8, 0.5, 0.9, 0.3, 0.6, "metal")))]),
    "infestation":dict(field=("flesh", 0.4, 1.8, 0.6), rocks=(5, 0.45, 0.8, 0.4, 0.8, "pustule"),
                       feats=[("strands", 2, dict(kind="tendril", hub="flesh")), ("pool", 2, dict(kind="acid", rim="flesh")), ("slabs", 1, dict(kind="metal", h=(0.3, 1.1)))]),
    "urban":      dict(field=(P, 0.2, 0.9, 0.45),
                       feats=[("rubble", 3, dict(soft=P, stone="concrete")), ("slabs", 2, dict(kind="concrete", h=(0.3, 1.2))),
                              ("crater", 1, dict(rim=B, pit="pothole", ejecta=(8, 0.5, 1.0, 0.3, 0.7, "rubble"))), ("pool", 2, dict(kind="water", r=(0.05, 0.09)))]),
    "mine":       dict(field=(E, 0.3, 1.6, 0.55), rocks=(3, 0.5, 1.0, 0.4, 0.8, "rock"),
                       feats=[("rubble", 2, dict(soft=P, stone="rock")), ("mesa", 2, dict(kind="rock", steps=(2, 3), h=(1.6, 3.4), scree=scree("rock"))),
                              ("pool", 2, dict(kind="water", r=(0.05, 0.1)))]),
    "swamp":      dict(field=("mud", 0.25, 1.0, 0.6), rocks=(2, 0.5, 0.9, 0.3, 0.6, "rock"),
                       feats=[("pool", 3, dict(kind="water", rim="mud", ripples=3)), ("strands", 2, dict(kind="root", hub="stump")),
                              ("rise", 4, dict(kind="moss", h=(0.5, 1.2), r=(0.07, 0.13)))]),
    "cynder":     dict(field=("ash", 0.3, 1.8, 0.55),
                       feats=[("mesa", 2, dict(kind="basalt", steps=(2, 3), h=(1.6, 3.6), scree=scree("basalt"))),
                              ("gully", 2, dict(bed="lava", bank="ash", bank_h=(0.4, 0.9))), ("outcrop", 1, dict(kind="obsidian", h=(1.4, 3.0)))]),
    "targa":      dict(field=("snow", 0.3, 1.9, 0.6),
                       feats=[("ridge", 2, dict(kind="snow", h=(0.6, 1.8), w=(2.6, 4.2))), ("outcrop", 2, dict(kind="ice", h=(1.6, 3.6), soft="snow")),
                              ("slabs", 2, dict(kind="alien", h=(0.3, 1.3))), ("gully", 1, dict(bed="slot", bank="snow", w=(0.9, 1.3)))]),
    "trederra":   dict(field=("mud", 0.3, 1.3, 0.6),
                       feats=[("crater", 3, dict(rim="mud", pit="pit", water="water", ejecta=(6, 0.4, 0.8, 0.3, 0.6, "rock"))), ("pool", 2, dict(kind="water", rim="mud", ripples=2))]),
    "fortress":   dict(field=("moss", 0.25, 1.0, 0.5),
                       feats=[("pool", 1, dict(kind="water", r=(0.1, 0.15), ripples=3, rim="moss")), ("outcrop", 2, dict(kind="garden", h=(1.0, 2.6), n=(2, 4), soft="moss")),
                              ("ridge", 1, dict(kind=B, h=(0.4, 0.9)))],
                       floored=dict(field=None, feats=[("slabs", 3, dict(kind="plank", h=(0.2, 0.9), tilt=False)), ("rubble", 2, dict(soft=P, stone="garden")),
                                                       ("outcrop", 1, dict(kind="garden", h=(1.0, 2.4), n=(2, 4)))])),
    "beast":      dict(field=("flesh", 0.4, 2.0, 0.7),
                       feats=[("ridge", 3, dict(kind="flesh", h=(0.6, 1.6), w=(2.0, 3.5))), ("pool", 2, dict(kind="bile", rim="flesh")),
                              ("strands", 2, dict(kind="vein", hub="flesh")), ("outcrop", 1, dict(kind="tooth", h=(1.4, 3.0), n=(3, 5)))]),
    "frontier":   dict(field=(E, 0.25, 1.3, 0.55), rocks=(2, 0.5, 0.9, 0.3, 0.6, "rock"),
                       feats=[("gully", 1, dict(bed="rut", bank=B, pebbles=pebbles)), ("pool", 2, dict(kind="water", r=(0.05, 0.09))), ("outcrop", 1, dict(kind="rock", soft=B))],
                       floored=dict(field=None, feats=[("slabs", 2, dict(kind="board", h=(0.2, 0.8), tilt=False)), ("rubble", 1, dict(soft=P, stone="wood")),
                                                       ("rise", 3, dict(kind=E, h=(0.3, 0.8)))])),
    "graveyard":  dict(field=("earth", 0.25, 1.0, 0.5),
                       feats=[("ridge", 4, dict(kind="earth", h=(0.4, 0.8), w=(2.2, 3.0), L=(0.14, 0.2), tang=False)), ("strands", 1, dict(kind="root", hub="stump")),
                              ("pool", 1, dict(kind="water", r=(0.05, 0.08)))]),
    "forest":     dict(field=("moss", 0.35, 1.8, 0.65), rocks=(2, 0.6, 1.0, 0.4, 0.7, "rock"),
                       feats=[("strands", 2, dict(kind="root", hub="stump", n=(4, 7))), ("ridge", 2, dict(kind="leaf", h=(0.3, 0.7))),
                              ("pool", 1, dict(kind="water")), ("outcrop", 1, dict(kind="rock", soft="moss"))]),
    "dungeon":    dict(field=None,
                       feats=[("slabs", 4, dict(kind="flag", h=(0.3, 1.2), r=(0.15, 0.24), n=(6, 10), tilt=False)), ("rubble", 2, dict(soft=P, stone="flag")),
                              ("pool", 2, dict(kind="water", r=(0.05, 0.1), ripples=1)), ("cracked_flat", 1, {})]),
    "temple":     dict(field=(E, 0.3, 1.5, 0.5),
                       feats=[("ridge", 2, dict(kind=E, h=(0.5, 1.4), w=(2.6, 4.2))), ("rubble", 2, dict(soft=P, stone="tcolumn", big=(1.0, 1.8))),
                              ("slabs", 2, dict(kind="tflag", h=(0.3, 1.1))), ("pool", 1, dict(kind="water", r=(0.06, 0.1)))]),
    "coast":      dict(field=(E, 0.2, 1.2, 0.5),
                       feats=[("mesa", 2, dict(kind="shelf", steps=(2, 4), h=(1.4, 3.0))), ("pool", 3, dict(kind="water", ripples=2, r=(0.07, 0.12))),
                              ("ridge", 2, dict(kind=E, h=(0.3, 0.8), w=(2.0, 3.0))), ("outcrop", 1, dict(kind="shelf"))]),
    }
    T.update(__import__("newstyles").ground_big(E, B, P, scree, pebbles))          # the styles kept one per file
    return T.get(style, dict(field=(E, 0.3, 1.2, 0.5), feats=[("outcrop", 1, dict(kind="rock"))]))


def _floored(g):
    """most of the ground round the middle is a floor (deck plates, flagstones, planks)"""
    try:
        pl = _u([u for z, u in g.S._plates(g.s)])
        ring = g.disc.difference(g.free)
        return not pl.is_empty and pl.intersection(ring).area > 0.5 * ring.area
    except Exception: return False


def apply(sc):
    """lay this style's terrain on a large base. Returns how many features went down."""
    fin = float(os.environ.get("KIT_FINAL_MM") or 0)
    import features
    if fin < 49.9 or features.true_scale() == 1.0: return 0
    import scenes
    g = Ground(sc)
    _kinds(g.gg, scenes, getattr(sc.s, "ground", "#777777") or "#777777")
    for k, v in {"ash": "#5c5552", "pipe": "#6f747c", "slot": "#0d1014"}.items(): g.gg.COL.setdefault(k, v)
    st = STYLE(sc.style); n0 = len(sc.s.ops)
    if st.get("floored") and _floored(g): st = dict(st, **st["floored"]); g.on_floors = True          # the floor's own recipe: meant to lie on the floor
    todo = []
    for name, n, kw in st["feats"]:
        m = n * g.A; m = int(m) + (1 if g.rng.random() < m - int(m) else 0)
        todo += [(name, kw)] * max(1 if n >= 2 else 0, m)
    # tall things first (they need the outer ring), then the lie of the land round them, then the rest
    tall = [t for t in todo if t[0] in ("mesa", "outcrop", "strands")]; rest = [t for t in todo if t not in tall]
    g.rng.shuffle(rest)
    def run(name, kw):
        try: ok = getattr(g, "f_" + name)(**kw)
        except Exception as e: ok = None; sc.log.append(__import__("kitlog").caught(f"ground_big {name}", e))
        if os.environ.get("GB_DEBUG"): print("  ground_big", name, ok, flush=True)
    for name, kw in tall: run(name, kw)
    if st.get("field"):
        kind, h0, h1, cover = st["field"]
        try: g.field(kind, (h0, h1), cover=cover * min(1.0, 0.75 + 0.25 * g.A), rocks=st.get("rocks"))
        except Exception as e: sc.log.append(__import__("kitlog").caught(f"ground_big field", e))
    for name, kw in rest: run(name, kw)
    if os.environ.get("GB_DEBUG"):
        new = [op for op in sc.s.ops[n0:]]
        top = max([op[3] for op in new if op[0] == "prism"] + [op[2] + op[3] for op in new if op[0] == "taper"] + [g.H]) - g.H
        print(f"  ground_big: tallest laid here {top:.2f} mm (layout)", flush=True)
    if os.environ.get("SCENE_DEBUG"): print(f"  ground_big: {g.made} ground features", flush=True)
    return g.made
