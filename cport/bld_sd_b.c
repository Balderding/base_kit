/* bld_sd_b.c: sd_b.py in C (agent "bld-sdabc", 2026-11-19): the style-only details of highland, maahl, corridor and
   infestation, twenty each, in the file's own order. See bld_sd_abc.h for how a builder is written; each one names
   the Python builder it copies (style / name). Proof: work\cscene\bld_sdabc\prove.py. */
#include "bld_sd_abc.h"

/* small callback contexts (the variables a lambda reads) */
typedef struct { double k; } C1;
typedef struct { double L, r; } C2;
typedef struct { double L, r, e; } C3;
typedef struct { double L, W, w, ph; } C4;

/* ====================================================================================================== highland */
/* highland / fallen_cross */
static int b_fallen_cross(SD_ARGS) {
    SD_BEGIN
    SoFrame ff = so_frame(cx, cy, a); double L = u(5.0, 6.4), w = u(0.95, 1.2); P2 c = FF(&ff, L * 0.2, 0); GV *l = GVn(); SoSolids *ps = NS();
    gv_add(l, RECT(cx, cy, L, w, a)); gv_add(l, RECT(c.x, c.y, w, L * 0.55, a)); gv_add(l, ANN(c.x, c.y, L * 0.2, L * 0.2 - 0.5, 20));
    GEOSGeometry *g = UL(l);
    if (RND() < 0.4) { double k = -L * u(0.1, 0.25); P2 p = FF(&ff, k, 0); double b = a + u(-0.5, 0.5); g = DIFF(g, RECT(p.x, p.y, 0.35, 3, b)); }
    GEOSGeometry *f = CHULL(g);
    EXT(ps, g, u(0.6, 0.85), 0.0); EXT(ps, DISC(c.x, c.y, 0.5, 12), 1.0, 0.0);
    ST(ps, "slate", NULL);
    SD_RET(f);
}

/* highland / peat_stack */
static int b_peat_stack(SD_ARGS) {
    SD_BEGIN
    SoFrame ff = so_frame(cx, cy, a); int64_t n = RI(rng, 3, 5); SoSolids *ps = NS();
    for (int row = 0; row < 2; row++)
        for (int64_t i = 0; i < n - row; i++) {
            double x = ((double)i - (double)(n - row - 1) / 2) * 1.25 + u(-0.1, 0.1), v = u(-0.1, 0.1); P2 c = FF(&ff, x, v);
            double t = a + SD_PI / 2 + u(-0.1, 0.1); LOAF(ps, c.x, c.y, t, 2.4, 1.1, 0.75, row * 0.7);
        }
    GEOSGeometry *f = RECT(cx, cy, (double)n * 1.25 + 0.4, 2.8, a);
    ST(ps, "earth", NULL);
    SD_RET(f);
}

/* highland / targe */
static int b_targe(SD_ARGS) {
    SD_BEGIN
    double R = u(1.7, 2.2); int64_t n = RI(rng, 6, 9); SoSolids *ps = NS(); PV *pf = PVn(4);
    pv_add(pf, R, 0.0); pv_add(pf, R, 0.35); pv_add(pf, R * 0.9, 0.5); pv_add(pf, 0.7, 0.62); TURN(ps, cx, cy, pf, 24, 1.0, 0.0, 0.0);
    DOME(ps, cx, cy, 0.6, 1.0, 0.0, 1.0, 0.0, 20);
    PV *rp = RINGPTS(cx, cy, R * 0.62, (int)n, a, 0.0, NULL);
    for (int i = 0; i < rp->n; i++) CYL(ps, rp->xy[2 * i], rp->xy[2 * i + 1], 0.26, 0.8, 0.0, 8);
    GEOSGeometry *f = DISC(cx, cy, R + 0.2, 20);
    ST(ps, "wood", NULL);
    SD_RET(f);
}

/* highland / bagpipes */
static int rf_bag(void *c, double t, double *o) {          /* 0.95 * math.sqrt(max(0.0, 1 - (2 * t / L) ** 2)) + 0.05 */
    double L = ((C1 *)c)->k, q; if (RF_PW(2 * t / L, 2.0, &q) != SO_OK) return SO_ERR;
    *o = 0.95 * sqrt(pmax(0.0, 1 - q)) + 0.05; return SO_OK;
}
static int b_bagpipes(SD_ARGS) {
    SD_BEGIN
    SoFrame ff = so_frame(cx, cy, a); double L = u(2.6, 3.2); C1 cl = { L }; SoSolids *ps = NS(); GV *tips = GVn();
    SPIN(ps, cx, cy, a, L, rf_bag, &cl, 1.0, 0.0, "round");
    for (int k = 0; k < 3; k++) {
        double x = L * 0.2 + u(2.8, 3.8), v = (k - 1) * u(0.7, 1.0); P2 e = FF(&ff, x, v); PV *rp = PVn(2);
        pv_p(rp, FF(&ff, L * 0.25, (k - 1) * 0.35)); pv_p(rp, e); ROD(ps, rp, 0.3, NULL, NULL, 1.0, 0.0, 0.0); gv_add(tips, DISC(e.x, e.y, 0.3, 20));
    }
    P2 e = FF(&ff, -L * 0.4, -2.2); PV *rp = PVn(2); pv_p(rp, FF(&ff, -L * 0.3, -0.4)); pv_p(rp, e); ROD(ps, rp, 0.28, NULL, NULL, 1.0, 0.0, 0.0); gv_add(tips, DISC(e.x, e.y, 0.3, 20));
    GV *hl = GVn(); gv_add(hl, RECT(cx, cy, L, 2.0, a)); for (int i = 0; i < tips->n; i++) gv_add(hl, tips->v[i]);
    GEOSGeometry *f = HL(hl);
    ST(ps, "cloth", NULL);
    SD_RET(f);
}

/* highland / ram_skull */
static int rf_ram_body(void *c, double t, double *o) { double k = ((C1 *)c)->k; *o = k * (0.6 - 0.2 * t / k); return SO_OK; }
static int rf_ram_horn(void *c, double q, double *o) { double k = ((C1 *)c)->k; *o = 0.42 * k * (1 - 0.5 * q); return SO_OK; }
static int b_ram_skull(SD_ARGS) {
    SD_BEGIN
    SoFrame ff = so_frame(cx, cy, a); double k = u(0.9, 1.2); C1 ck = { k }; SoSolids *ps = NS(); CV *cuts = CVn();
    SPIN(ps, cx, cy, a, 2.6 * k, rf_ram_body, &ck, 0.95, 0.0, "round");
    for (int s = -1; s <= 1; s += 2) { double sv = s; P2 c = FF(&ff, -0.7 * k, sv * 1.15 * k); ROD(ps, SPIRAL(c.x, c.y, 0.85 * k, 0.3 * k, 1.1 * sv, a - sv * 1.57, 20), 0, rf_ram_horn, &ck, 1.0, 0.0, 0.0); }
    GEOSGeometry *f = RECT(cx, cy, 2.8 * k, 4.4 * k, a);
    for (int s = -1; s <= 1; s += 2) { double sv = s; P2 p = FF(&ff, -0.2 * k, sv * 0.36 * k); cv_add(cuts, DISC(p.x, p.y, 0.24 * k, 8), 0.3); }
    ST(ps, "bone", cuts);
    SD_RET(f);
}

/* highland / drystone_run */
static int b_drystone_run(SD_ARGS) {
    SD_BEGIN
    SoFrame ff = so_frame(cx, cy, a); int64_t n = RI(rng, 4, 6); SoSolids *ps = NS();
    for (int64_t i = 0; i < n; i++) {
        double x = ((double)i - (double)(n - 1) / 2) * 1.5 + u(-0.15, 0.15), v = u(-0.2, 0.2); P2 c = FF(&ff, x, v);
        double r = u(0.8, 1.0), h = u(0.8, 1.1); ROCK(ps, c.x, c.y, r, h, rng, 0.0, 0.0, 9);
        if (RND() < 0.6 && 0 < i && i < n - 1) { double r2 = u(0.6, 0.8), h2 = u(0.6, 0.8); ROCK(ps, c.x + 0.5, c.y, r2, h2, rng, 0.0, 0.75, 9); }
    }
    GEOSGeometry *f = RECT(cx, cy, (double)n * 1.5 + 0.6, 2.4, a);
    ST(ps, "slate", NULL);
    SD_RET(f);
}

/* highland / thistles */
static int b_thistles(SD_ARGS) {
    SD_BEGIN
    SoSolids *ps = NS(); GV *foot_ = GVn(); int64_t n = RI(rng, 2, 4);
    for (int64_t i = 0; i < n; i++) {
        double r0 = u(0, 1.8), t0 = u(0, 6.28); P2 p = POL(cx, cy, r0, t0); double h = u(1.4, 2.6);
        double fr = u(0.9, 1.2); int64_t nn = RI(rng, 5, 7); double aa = u(0, 1);
        FLUTED(ps, p.x, p.y, fr, 0.5, (int)nn, aa, 0.4, 3, 0.0, 0.0); CYL(ps, p.x, p.y, 0.3, h, 0.0, 8); FLUTED(ps, p.x, p.y, 0.55, 0.7, 9, 0, 0.7, 4, 0.0, h - 0.1);
        gv_add(foot_, DISC(p.x, p.y, 1.2, 20));
    }
    GEOSGeometry *f = HL(foot_);
    ST(ps, "heather", NULL);
    SD_RET(f);
}

/* highland / heather_hummock */
static int b_heather_hummock(SD_ARGS) {
    SD_BEGIN
    SoSolids *ps = NS(); GV *foot_ = GVn(); int64_t n = RI(rng, 4, 7);
    for (int64_t i = 0; i < n; i++) {
        double r0 = u(0, 1.7), t0 = u(0, 6.28); P2 p = POL(cx, cy, r0, t0); double r = u(0.8, 1.3), h = r * u(0.6, 0.9);
        DOME(ps, p.x, p.y, r, h, 0.0, 1.0, 0.0, 10); gv_add(foot_, DISC(p.x, p.y, r, 20));
    }
    GEOSGeometry *f = BUF(HL(foot_), 0.2, 16);
    ST(ps, "heather", NULL);
    SD_RET(f);
}

/* highland / quern */
static int b_quern(SD_ARGS) {
    SD_BEGIN
    double R = u(1.5, 1.9), h = u(0.7, 0.95), ro = RND() < 0.5 ? u(0.0, 0.9) : 0.0; P2 off = POL(cx, cy, ro, a); SoSolids *ps = NS(); PV *p1 = PVn(2), *p2_ = PVn(3); CV *cuts = CVn();
    pv_add(p1, R, 0.0); pv_add(p1, R, h * 0.5); TURN(ps, cx, cy, p1, 24, 1.0, 0.0, 0.0);
    pv_add(p2_, R * 0.92, 0.0); pv_add(p2_, R * 0.92, h); pv_add(p2_, R * 0.8, h + 0.12); TURN(ps, off.x, off.y, p2_, 24, 1.0, 0.0, 0.0);
    P2 q = POL(off.x, off.y, R * 0.62, a + 2); CYL(ps, q.x, q.y, 0.3, h + 0.7, 0.0, 8);
    GEOSGeometry *f = DISC(cx, cy, R + 1.0, 20);
    cv_add(cuts, DISC(off.x, off.y, 0.4, 10), h - 0.4);
    ST(ps, "slate", cuts);
    SD_RET(f);
}

/* highland / pot_still */
static int rf_still(void *c, double q, double *o) { (void)c; *o = 0.4 - 0.1 * q; return SO_OK; }
static int b_pot_still(SD_ARGS) {
    SD_BEGIN
    double r = u(1.3, 1.6), h = u(1.9, 2.5); P2 e = POL(cx, cy, r + u(1.8, 2.6), a), m = POL(cx, cy, r + 0.7, a + 0.25); SoSolids *ps = NS(); PV *pf = PVn(12), *rp = PVn(3);
    pv_add(pf, r * 0.9, 0.0); pv_add(pf, r, h * 0.35); pv_crv(pf, r, 0.42, h * 0.35, h * 0.85); pv_add(pf, 0.42, h); TURN(ps, cx, cy, pf, 24, 1.0, 0.0, 0.0);
    pv_p(rp, POL(cx, cy, 0.2, a)); pv_p(rp, m); pv_p(rp, e); ROD(ps, rp, 0, rf_still, NULL, 1.0, 0.4, 0.0);
    POT(ps, e.x, e.y, 0.75, 1.1, 0.5, 0.66, 0.0, 24);
    GV *hl = GVn(); gv_add(hl, DISC(cx, cy, r + 0.2, 20)); gv_add(hl, DISC(e.x, e.y, 1.0, 20));
    GEOSGeometry *f = HL(hl);
    ST(ps, "brass", NULL);
    SD_RET(f);
}

/* highland / peat_cutting (ground) */
static int b_peat_cutting(SD_ARGS) {
    SD_BEGIN
    SoFrame ff = so_frame(cx, cy, a); double L = u(4.0, 5.5), W = u(2.0, 2.6); P2 p;
    LGCUT(RECT(cx, cy, L, W, a), 0.3, "pit");
    p = FF(&ff, -L * 0.15, 0); LGCUT(RECT(p.x, p.y, L * 0.6, W - 0.5, a), 0.6, "pit");
    int64_t n = RI(rng, 2, 4);
    for (int64_t i = 0; i < n; i++) { p = FF(&ff, -L / 2 + 0.8 + (double)i * 1.3, W / 2 + 0.85); double b = a + u(-0.2, 0.2); LUP(RECT(p.x, p.y, 1.1, 1.0, b), 0.45, "earth"); }
    p = FF(&ff, 0, 0.5);
    SD_RET(RECT(p.x, p.y, L + 0.4, W + 2.0, a));
}

/* highland / cup_and_ring */
static int b_cup_and_ring(SD_ARGS) {
    SD_BEGIN
    GV *gs = GVn(), *foot_ = GVn(); int64_t n = RI(rng, 1, 3);
    for (int64_t i = 0; i < n; i++) {
        double rr = i == 0 ? 0.0 : u(2.6, 3.4); P2 p = POL(cx, cy, rr, a + (double)i * 2.2); double R = u(1.0, 1.5);
        gv_add(gs, DISC(p.x, p.y, 0.32, 10));
        DV rs = ARANGE(0.85, R + 0.01, 0.6); for (int j = 0; j < rs.n; j++) gv_add(gs, ANN(p.x, p.y, rs.v[j], rs.v[j] - 0.3, 20));
        double t = u(0, 6.28); gv_add(gs, BARL(p, POL(p.x, p.y, R + 0.5, t), 0.3)); gv_add(foot_, DISC(p.x, p.y, R + 0.6, 20));
    }
    GEOSGeometry *f = HL(foot_);
    LGCUT(UL(gs), 0.3, "glyph");
    SD_RET(f);
}

/* highland / creel */
static int rf_creel(void *c, double t, double *o) { (void)c; *o = 0.36 * (1 - fabs(t) / 0.9) + 0.14; return SO_OK; }
static int b_creel(SD_ARGS) {
    SD_BEGIN
    double r = u(1.1, 1.4), h = u(1.5, 2.0); SoSolids *ps = NS(); PV *pf = PVn(4); CV *cuts = CVn();
    pv_add(pf, r * 0.75, 0.0); pv_add(pf, r, h * 0.9); pv_add(pf, r + 0.12, h * 0.92); pv_add(pf, r + 0.12, h); TURN(ps, cx, cy, pf, 12, 1.0, 0.0, 0.0);
    GEOSGeometry *q = DISC(cx, cy, r - 0.35, 12); cv_add(cuts, q, h - u(0.4, 0.8));
    ST(ps, "twig", cuts);
    int64_t n = RI(rng, 0, 3);
    for (int64_t i = 0; i < n; i++) {
        P2 p = POL(cx, cy, r + u(0.9, 1.6), a + (double)i * 1.1); double b = u(0, 3.14); SoSolids *s2 = NS();
        SPIN(s2, p.x, p.y, b, 1.7, rf_creel, NULL, 0.9, 0.0, "round"); ST(s2, "hide", NULL);
    }
    SD_RET(DISC(cx, cy, r + 2.0, 20));
}

/* highland / caber_and_stone */
static int rf_caber(void *c, double t, double *o) { double L = ((C1 *)c)->k; *o = 0.62 - 0.18 * t / L; return SO_OK; }
static int b_caber_and_stone(SD_ARGS) {
    SD_BEGIN
    SoFrame ff = so_frame(cx, cy, a); double L = u(7.5, 10.0), cu = u(-2, 2); P2 c = FF(&ff, cu, 1.9); C1 cl = { L }; SoSolids *s1 = NS(), *s2 = NS();
    GV *hl = GVn(); gv_add(hl, RECT(cx, cy, L, 1.5, a)); gv_add(hl, DISC(c.x, c.y, 1.1, 20)); GEOSGeometry *f = HL(hl);
    SPIN(s1, cx, cy, a, L, rf_caber, &cl, 1.1, 0.0, "round"); ST(s1, "wood", NULL);
    double r = u(0.75, 0.95), aa = u(0, 3); DOME(s2, c.x, c.y, r, 1.0, 0.0, 0.85, aa, 20); ST(s2, "slate", NULL);
    SD_RET(f);
}

/* highland / tussocks */
static int b_tussocks(SD_ARGS) {
    SD_BEGIN
    SoSolids *ps = NS(); GV *foot_ = GVn(); int64_t n = RI(rng, 3, 6);
    for (int64_t i = 0; i < n; i++) {
        double r0 = u(0, 2.2), t0 = u(0, 6.28); P2 p = POL(cx, cy, r0, t0); double r = u(0.7, 1.1);
        double h = r * u(0.9, 1.5); int64_t nn = RI(rng, 7, 10); double aa = u(0, 1), tw = u(-0.2, 0.2);
        FLUTED(ps, p.x, p.y, r, h, (int)nn, aa, 0.55, 5, tw, 0.0); gv_add(foot_, DISC(p.x, p.y, r, 20));
    }
    GEOSGeometry *f = BUF(HL(foot_), 0.2, 16);
    ST(ps, "grass", NULL);
    SD_RET(f);
}

/* highland / burial_kist (ground) */
static int b_burial_kist(SD_ARGS) {
    SD_BEGIN
    SoFrame ff = so_frame(cx, cy, a); double L = u(3.4, 4.2), W = u(2.0, 2.5), t = 0.9; GV *l = GVn(); P2 p;
    for (int sv = -1; sv <= 1; sv += 2) { p = FF(&ff, 0, sv * (W / 2 + t / 2 - 0.1)); gv_add(l, RECT(p.x, p.y, L + 2 * t, t, a)); }
    for (int sv = -1; sv <= 1; sv += 2) { p = FF(&ff, sv * (L / 2 + t / 2 - 0.1), 0); gv_add(l, RECT(p.x, p.y, t, W, a)); }
    GEOSGeometry *slabs = UL(l);
    double x = u(1.2, L), v = u(-0.6, 0.6); p = FF(&ff, x, v); double b = a + u(-0.4, 0.4);
    GEOSGeometry *cap = DIFF(RECT(p.x, p.y, L * 0.8, W + 1.2, b), RECT(cx, cy, L + 2 * t + 0.6, W + 2 * t + 0.6, a));
    LGCUT(RECT(cx, cy, L, W, a), 0.7, "pit"); LUP(slabs, u(0.45, 0.7), "slate");
    if (!EMPTY(cap)) {
        ShpList pl = { 0 }; int best = -1; double ba = 0;
        shp_polys_of(cap, &pl);
        for (int i = 0; i < pl.n; i++) Gk(pl.g[i]);
        if (!pl.n) { free(pl.g); sd_fail(); }          /* max() of an empty sequence */
        for (int i = 0; i < pl.n; i++) { double ar = AREA(pl.g[i]); if (best < 0 || ar > ba) { best = i; ba = ar; } }
        LUP(pl.g[best], 0.6, "slate"); free(pl.g);
    }
    GV *hl = GVn(); gv_add(hl, slabs); gv_add(hl, cap);
    SD_RET(HL(hl));
}

/* highland / hurdle */
static int b_hurdle(SD_ARGS) {
    SD_BEGIN
    SoFrame ff = so_frame(cx, cy, a); double L = u(5.0, 6.5), W = u(3.0, 3.6); int64_t n = RI(rng, 3, 4); GV *l = GVn();
    DV vs = LINSPACE(-W / 2 + 0.46, W / 2 - 0.46, (int)n);
    for (int i = 0; i < vs.n; i++) gv_add(l, BARL(FF(&ff, -L / 2, vs.v[i]), FF(&ff, L / 2, vs.v[i]), 0.92));
    for (int sv = -1; sv <= 1; sv += 2) gv_add(l, BARL(FF(&ff, sv * (L / 2 - 0.5), -W / 2 - 0.5), FF(&ff, sv * (L / 2 - 0.5), W / 2 + 0.5), 0.92));
    gv_add(l, BARL(FF(&ff, -L / 2 + 0.6, -W / 2 + 0.5), FF(&ff, L / 2 - 0.6, W / 2 - 0.5), 0.92));
    GEOSGeometry *g = UL(l), *f = RECT(cx, cy, L, W + 1.0, a);
    LUP(g, 0.45, "wood");
    SD_RET(f);
}

/* highland / split_boulder */
static int b_split_boulder(SD_ARGS) {
    SD_BEGIN
    double r = u(1.6, 2.2), h = u(1.3, 1.9); GEOSGeometry *gap = BARL(POL(cx, cy, 3 * r, a), POL(cx, cy, 3 * r, a + SD_PI), u(0.45, 0.8)); SoSolids *ps = NS(); CV *cuts = CVn();
    GEOSGeometry *f = DISC(cx, cy, r + 0.2, 20);
    ROCK(ps, cx, cy, r, h, rng, 0.9, 0.0, 10); cv_add(cuts, gap, 0.0);
    ST(ps, "slate", cuts);
    SD_RET(f);
}

/* highland / crook_and_plaid */
static int b_crook_and_plaid(SD_ARGS) {
    SD_BEGIN
    SoFrame ff = so_frame(cx, cy, a); double L = u(6.0, 7.5); P2 h0 = FF(&ff, L / 2, 0.6); SoSolids *ps = NS(); PV *rp = PVn(12);
    pv_p(rp, FF(&ff, -L / 2, 0)); pv_p(rp, FF(&ff, L / 2, 0)); PV *ar = ARC(h0.x, h0.y, 0.6, a - 1.57, a + 2.2, 8); for (int i = 0; i < ar->n; i++) pv_add(rp, ar->xy[2 * i], ar->xy[2 * i + 1]);
    ROD(ps, rp, 0.3, NULL, NULL, 1.1, 0.0, 0.0);
    double bu = u(-1.5, 1.5); P2 bp = FF(&ff, bu, -1.9); double br = u(1.3, 1.7); GEOSGeometry *b = BLOB(bp.x, bp.y, br, rng, 4);
    GV *hl = GVn(); gv_add(hl, RECT(cx, cy, L + 1.4, 1.4, a)); gv_add(hl, b); GEOSGeometry *f = HL(hl);
    ST(ps, "wood", NULL); LMOUND(b, u(0.5, 0.75), "cloth", SD_SOFT_T);
    SD_RET(f);
}

/* highland / beacon_pyre */
static int b_beacon_pyre(SD_ARGS) {
    SD_BEGIN
    double r = u(1.8, 2.4), h = u(2.4, 3.3); int64_t n = RI(rng, 7, 10); SoSolids *ps = NS();
    for (int k = 0; k < 6; k++) EXT(ps, STAR(cx, cy, r * (1 - k / 6.5), r * (1 - k / 6.5) * 0.6, (int)n, a + 0.05 * k), h * (k + 1) / 6, 0.0);
    ST(ps, "wood", NULL);
    int64_t m = RI(rng, 6, 9); PV *rp = RINGPTS(cx, cy, r + 0.8, (int)m, a, 0.1, rng);
    for (int i = 0; i < rp->n; i++) { GEOSGeometry *st = STONE(rp->xy[2 * i], rp->xy[2 * i + 1], u(0.5, 0.65), rng, 0.85, 6); LMOUND(st, 0.45, "slate", SD_PEB_T); }
    SD_RET(DISC(cx, cy, r + 1.5, 20));
}

/* ====================================================================================================== maahl */
/* maahl / buried_head */
static int b_buried_head(SD_ARGS) {
    SD_BEGIN
    SoFrame ff = so_frame(cx, cy, a); double r = u(1.7, 2.3), h = r * u(0.7, 0.95); SoSolids *ps = NS(); CV *cuts = CVn(); P2 p;
    DOME(ps, cx, cy, r, h, 0.0, 0.8, a, 20);
    p = FF(&ff, r * 0.25, 0); RIDGE(ps, p.x, p.y, a, r * 0.9, 0.7, h + 0.25, 0.0);
    p = FF(&ff, -r * 0.75, 0); BLK(ps, p.x, p.y, 0.6, r * 1.3, a, h * 0.7, 0.0);
    for (int sv = -1; sv <= 1; sv += 2) { p = FF(&ff, r * 0.1, sv * r * 0.36); cv_add(cuts, ELL(p.x, p.y, r * 0.2, r * 0.13, a, 10), h * 0.55); }
    p = FF(&ff, r * 0.72, 0); cv_add(cuts, RECT(p.x, p.y, 0.3, r * 0.5, a), h * 0.3);
    GEOSGeometry *f = DISC(cx, cy, r + 0.3, 20);
    ST(ps, "ruin", cuts);
    SD_RET(f);
}

/* maahl / obelisk_tip */
static int rf_obelisk(void *c, double t, double *o) {
    double L = ((C2 *)c)->L, w = ((C2 *)c)->r;
    *o = t < L / 2 - 1.2 ? w * (1 - 0.3 * (t + L / 2) / L) : w * 0.7 * pmax(0.0, (L / 2 - t) / 1.2); return SO_OK;
}
static int b_obelisk_tip(SD_ARGS) {
    SD_BEGIN
    double L = u(5.0, 7.0), w = u(0.8, 1.05); C2 cl = { L, w }; SoSolids *ps = NS();
    SPIN(ps, cx, cy, a, L, rf_obelisk, &cl, 1.0, 0.0, "ridge");
    GEOSGeometry *f = RECT(cx, cy, L, 2 * w + 0.3, a);
    ST(ps, "obelisk", NULL);
    SD_RET(f);
}

/* maahl / sundial */
static int b_sundial(SD_ARGS) {
    SD_BEGIN
    double R = u(1.9, 2.5), h = u(0.5, 0.7); int n = 12; SoSolids *ps = NS(); PV *pf = PVn(2); CV *cuts = CVn();
    pv_add(pf, R, 0.0); pv_add(pf, R, h); TURN(ps, cx, cy, pf, 28, 1.0, 0.0, 0.0);
    P2 p = POL(cx, cy, R * 0.25, a); double hh = h + u(0.8, 1.2); RIDGE(ps, p.x, p.y, a, R * 0.9, 0.6, hh, 0.0);
    for (int k = 0; k < n; k++) if (2 < k && k < 10) cv_add(cuts, BARL(POL(cx, cy, R * 0.72, a + 6.2832 * k / n), POL(cx, cy, R * 0.94, a + 6.2832 * k / n), 0.28), h - 0.22);
    GEOSGeometry *f = DISC(cx, cy, R + 0.2, 20);
    ST(ps, "ruin", cuts);
    SD_RET(f);
}

/* maahl / amphorae */
static int rf_amphora(void *c, double t, double *o) {
    double L = ((C2 *)c)->L, r = ((C2 *)c)->r;
    if (t < L * 0.12) {          /* r * math.sin(min(1.0, (t + L / 2) / (L * 0.62)) * math.pi) ** 0.7 + 0.05: a complex keeps its real part (_real) */
        double re, im; int k = px_pow(SIN(pmin(1.0, (t + L / 2) / (L * 0.62)) * SD_PI), 0.7, &re, &im);
        if (k < 0) return SO_ERR;
        *o = r * re + 0.05;
    } else *o = t < L / 2 - 0.3 ? 0.3 : 0.45;
    return SO_OK;
}
static int b_amphorae(SD_ARGS) {
    SD_BEGIN
    SoSolids *ps = NS(); GV *foot_ = GVn(); int64_t n = RI(rng, 1, 3);
    for (int64_t i = 0; i < n; i++) {
        double rr = i == 0 ? 0.0 : u(1.6, 2.2); P2 c = POL(cx, cy, rr, a + 1.57 + (double)i * 3.14); double b = a + u(-0.5, 0.5), L = u(3.4, 4.2), r = u(0.75, 0.95);
        C2 cl = { L, r }; SPIN(ps, c.x, c.y, b, L, rf_amphora, &cl, 1.15, 0.0, "round");
        SoFrame f2 = so_frame(c.x, c.y, b);
        for (int sv = -1; sv <= 1; sv += 2) { PV *rp = PVn(3); pv_p(rp, FF(&f2, L * 0.1, sv * 0.45)); pv_p(rp, FF(&f2, L * 0.3, sv * 0.75)); pv_p(rp, FF(&f2, L * 0.42, sv * 0.3)); ROD(ps, rp, 0.24, NULL, NULL, 1.0, 0.0, 0.0); }
        gv_add(foot_, RECT(c.x, c.y, L, 2 * r + 0.4, b));
    }
    GEOSGeometry *f = HL(foot_);
    ST(ps, "clay", NULL);
    SD_RET(f);
}

/* maahl / wind_ripples */
static int b_wind_ripples(SD_ARGS) {
    SD_BEGIN
    SoFrame ff = so_frame(cx, cy, a); int64_t n = RI(rng, 4, 7); double L = u(5.0, 7.5), ph = u(0, 6); GV *gs = GVn();
    for (int64_t i = 0; i < n; i++) {
        double k = 1 - 0.1 * fabs((double)i - (double)n / 2); DV xs = LINSPACE(-L / 2 * k, L / 2 * k, 14); PV *pl = PVn(14);
        for (int j = 0; j < xs.n; j++) { double x = xs.v[j]; pv_p(pl, FF(&ff, x, ((double)i - (double)n / 2) * 0.95 + 0.3 * SIN(x * 1.1 + ph + (double)i * 0.4))); }
        gv_add(gs, LINE(pl, 0.42));
    }
    GEOSGeometry *f = RECT(cx, cy, L, (double)n * 0.95 + 0.8, a);
    LGCUT(UL(gs), 0.28, "pan");
    SD_RET(f);
}

/* maahl / scarab */
static int rf_scarab(void *c, double t, double *o) {          /* W / 2 * math.sqrt(max(0.0, 1 - (2 * t / L) ** 2)) ** 0.8 + 0.05 */
    double L = ((C2 *)c)->L, W = ((C2 *)c)->r, q, p;
    if (RF_PW(2 * t / L, 2.0, &q) != SO_OK || RF_PW(sqrt(pmax(0.0, 1 - q)), 0.8, &p) != SO_OK) return SO_ERR;
    *o = W / 2 * p + 0.05; return SO_OK;
}
static int b_scarab(SD_ARGS) {
    SD_BEGIN
    SoFrame ff = so_frame(cx, cy, a); double L = u(2.8, 3.6), W = L * 0.68; C2 cl = { L, W }; SoSolids *ps = NS(); CV *cuts = CVn(); P2 p;
    SPIN(ps, cx, cy, a, L, rf_scarab, &cl, 0.9, 0.0, "round");
    p = FF(&ff, L / 2, 0); DOME(ps, p.x, p.y, W * 0.3, 0.6, 0.0, 1.0, 0.0, 20);
    for (int k = -1; k <= 1; k++)
        for (int sv = -1; sv <= 1; sv += 2) {
            PV *rp = PVn(3); pv_p(rp, FF(&ff, k * L * 0.25, sv * W * 0.4)); pv_p(rp, FF(&ff, k * L * 0.32 + 0.3 * k, sv * (W / 2 + 0.7))); pv_p(rp, FF(&ff, k * L * 0.45, sv * (W / 2 + 1.1)));
            ROD(ps, rp, 0.25, NULL, NULL, 1.0, 0.0, 0.0);
        }
    GEOSGeometry *f = RECT(cx, cy, L + 0.8, W + 2.6, a);
    cv_add(cuts, BARL(FF(&ff, -L / 2, 0), FF(&ff, L * 0.2, 0), 0.24), 0.75 * W / 2);
    ST(ps, "chitin", cuts);
    SD_RET(f);
}

/* maahl / arch_fragment */
static int b_arch_fragment(SD_ARGS) {
    SD_BEGIN
    double R = u(3.0, 4.2), w = u(1.1, 1.4), span = u(1.2, 2.2); int64_t ni; if (px_int(span * R / 1.3, &ni) != PX_OK) sd_fail(); int n = (int)(ni > 3 ? ni : 3); GV *gs = GVn(); SoSolids *ps = NS();
    for (int k = 0; k < n; k++) {
        double t0 = a + span * k / n + 0.04, t1 = a + span * (k + 1) / n - 0.04; PV *pg = PVn(4);
        pv_p(pg, POL(cx, cy, R, t0)); pv_p(pg, POL(cx, cy, R, t1)); pv_p(pg, POL(cx, cy, R - w, t1)); pv_p(pg, POL(cx, cy, R - w, t0));
        gv_add(gs, POLY(pg));
    }
    GEOSGeometry *f = CHULL(UL(gs));
    for (int i = 0; i < gs->n; i++) EXT(ps, gs->v[i], u(0.7, 1.0), 0.0);
    ST(ps, "ruin", NULL);
    SD_RET(f);
}

/* maahl / geode */
static int b_geode(SD_ARGS) {
    SD_BEGIN
    double r = u(1.5, 2.0), h = u(1.0, 1.4); SoSolids *ps = NS(), *cs = NS(); CV *cuts = CVn();
    ROCK(ps, cx, cy, r, h, rng, 0.9, 0.0, 9); cv_add(cuts, DISC(cx, cy, r * 0.62, 12), h * 0.3); ST(ps, "rock", cuts);
    int64_t n = RI(rng, 3, 5);
    for (int64_t i = 0; i < n; i++) { double r0 = u(0, r * 0.3), t0 = u(0, 6.28); P2 p = POL(cx, cy, r0, t0); double cr = u(0.35, 0.5), chh = u(0.9, 1.6); CONE(cs, p.x, p.y, cr, chh, 0.22, 0.0, 6, 1.0, 0.0); }
    ST(cs, "crystal", NULL);
    SD_RET(DISC(cx, cy, r + 0.3, 20));
}

/* maahl / saddlebags */
static int b_saddlebags(SD_ARGS) {
    SD_BEGIN
    SoFrame ff = so_frame(cx, cy, a); double s = u(1.5, 1.9); SoSolids *ps = NS();
    BLK(ps, cx, cy, 2 * s, 0.9, a, 0.45, 0.0);
    for (int sv = -1; sv <= 1; sv += 2) { P2 c = FF(&ff, sv * s, 0); double r = u(1.0, 1.25), h = u(0.8, 1.1); DOME(ps, c.x, c.y, r, h, 0.0, 0.85, a + 1.57, 20); BLK(ps, c.x, c.y, 0.5, 1.6, a, 0.6, 0.0); }
    GEOSGeometry *f = RECT(cx, cy, 2 * s + 2.8, 2.8, a);
    ST(ps, "cloth", NULL);
    SD_RET(f);
}

/* maahl / oil_lamp */
static int rf_lamp(void *c, double t, double *o) {
    double L = ((C1 *)c)->k, q; if (RF_PW(2 * t / L, 2.0, &q) != SO_OK) return SO_ERR;
    *o = 0.85 * sqrt(pmax(0.0, 1 - q)) + 0.05; return SO_OK;
}
static int rf_lamp_spout(void *c, double q, double *o) { (void)c; *o = 0.42 - 0.14 * q; return SO_OK; }
static int b_oil_lamp(SD_ARGS) {
    SD_BEGIN
    SoFrame ff = so_frame(cx, cy, a); double L = u(2.4, 3.0); C1 cl = { L }; SoSolids *ps = NS(); PV *rp = PVn(2); P2 p;
    SPIN(ps, cx, cy, a, L, rf_lamp, &cl, 1.0, 0.0, "round");
    pv_p(rp, FF(&ff, L * 0.3, 0)); pv_p(rp, FF(&ff, L / 2 + 1.1, 0)); ROD(ps, rp, 0, rf_lamp_spout, NULL, 1.2, 0.0, 0.0);
    p = FF(&ff, -L / 2 - 0.35, 0); EXT(ps, ANN(p.x, p.y, 0.62, 0.28, 12), 0.5, 0.0);
    CYL(ps, cx, cy, 0.34, 1.15, 0.0, 8);
    p = FF(&ff, 0.2, 0); GEOSGeometry *f = RECT(p.x, p.y, L + 2.6, 2.0, a);
    ST(ps, "brass", NULL);
    SD_RET(f);
}

/* maahl / mosaic */
static int b_mosaic(SD_ARGS) {
    SD_BEGIN
    SoFrame ff = so_frame(cx, cy, a); int64_t n = RI(rng, 3, 5), m = RI(rng, 2, 4); GV *gs = GVn(); SoSolids *ps = NS();
    for (int64_t i = 0; i < n; i++)
        for (int64_t j = 0; j < m; j++) {
            if (RND() < 0.22) continue;
            P2 p = FF(&ff, ((double)i - (double)(n - 1) / 2) * 1.25, ((double)j - (double)(m - 1) / 2) * 1.25); double b = a + u(-0.08, 0.08);
            gv_add(gs, RECT(p.x, p.y, 1.0, 1.0, b));
        }
    if (!gs->n) SD_NONE;
    GEOSGeometry *f = RECT(cx, cy, (double)n * 1.25 + 0.3, (double)m * 1.25 + 0.3, a);
    for (int i = 0; i < gs->n; i++) EXT(ps, gs->v[i], u(0.26, 0.36), 0.0);
    ST(ps, "pavers", NULL);
    SD_RET(f);
}

/* maahl / beast_tracks */
static int b_beast_tracks(SD_ARGS) {
    SD_BEGIN
    SoFrame ff = so_frame(cx, cy, a); int64_t n = RI(rng, 3, 5); GV *gs = GVn(); static const double T3[3] = { -0.6, 0, 0.6 };
    for (int64_t i = 0; i < n; i++) {
        P2 c = FF(&ff, ((double)i - (double)(n - 1) / 2) * 2.3, 0.8 * (IMOD(i, 2) ? 1 : -1)); gv_add(gs, DISC(c.x, c.y, 0.5, 10));
        for (int k = 0; k < 3; k++) gv_add(gs, TRI(POL(c.x, c.y, 0.3, a + T3[k]), POL(c.x, c.y, 1.25, a + T3[k]), 0.5));
    }
    GEOSGeometry *f = RECT(cx, cy, (double)n * 2.3 + 1.0, 3.6, a);
    LGCUT(UL(gs), 0.35, "pit");
    SD_RET(f);
}

/* maahl / canopic_jars */
static int b_canopic_jars(SD_ARGS) {
    SD_BEGIN
    SoFrame ff = so_frame(cx, cy, a); int64_t n = RI(rng, 2, 4); SoSolids *ps = NS();
    for (int64_t i = 0; i < n; i++) {
        double v = u(-0.2, 0.2); P2 c = FF(&ff, ((double)i - (double)(n - 1) / 2) * 1.75, v); double r = u(0.6, 0.75), h = u(1.3, 1.8); int64_t lid = RI(rng, 0, 2);
        PV *pf = PVn(3); pv_add(pf, r * 0.8, 0.0); pv_add(pf, r, h * 0.6); pv_add(pf, r * 0.8, h); TURN(ps, c.x, c.y, pf, 14, 1.0, 0.0, 0.0);
        if (lid == 0) DOME(ps, c.x, c.y, r * 0.8, 0.6, h, 1.0, 0.0, 20);
        else if (lid == 1) CONE(ps, c.x, c.y, r * 0.8, 0.8, 0.25, h, 16, 1.0, 0.0);
        else CYL(ps, c.x, c.y, 0.3, 0.4, h, 8);
    }
    GEOSGeometry *f = RECT(cx, cy, (double)n * 1.75 + 0.4, 2.0, a);
    ST(ps, "clay", NULL);
    SD_RET(f);
}

/* maahl / sphinx_paw */
static int b_sphinx_paw(SD_ARGS) {
    SD_BEGIN
    SoFrame ff = so_frame(cx, cy, a); double L = u(3.6, 4.6), W = u(2.6, 3.2), h = u(1.1, 1.5); int n = 4; SoSolids *ps = NS(); CV *cuts = CVn(); P2 p;
    LOAF(ps, cx, cy, a, L, W, h, 0.0);
    for (int i = 0; i < n; i++) { p = FF(&ff, L / 2 + 0.2, (i - 1.5) * W / n); DOME(ps, p.x, p.y, W / n * 0.56, h * 0.8, 0.0, 0.7, a + 1.57, 20); }
    for (int i = 0; i < 3; i++) cv_add(cuts, BARL(FF(&ff, L * 0.1, (i - 1) * W / n), FF(&ff, L / 2 + 0.6, (i - 1) * W / n), 0.24), h * 0.55);
    p = FF(&ff, 0.3, 0); GEOSGeometry *f = RECT(p.x, p.y, L + 1.2, W + 0.3, a);
    ST(ps, "ruin", cuts);
    SD_RET(f);
}

/* maahl / salt_plates */
static int b_salt_plates(SD_ARGS) {
    SD_BEGIN
    GV *gs = GVn(), *foot_ = GVn(); int64_t n = RI(rng, 3, 6); SoSolids *ps = NS();
    for (int64_t i = 0; i < n; i++) {
        double rr = i == 0 ? 0.0 : u(2.0, 2.6); P2 p = POL(cx, cy, rr, a + (double)i * 1.15); double sr = u(0.95, 1.25); int64_t nv = RI(rng, 5, 7);
        GEOSGeometry *q = STONE(p.x, p.y, sr, rng, 0.9, (int)nv); int ok = 1;
        for (int j = 0; j < foot_->n; j++) if (!(GDIST(q, foot_->v[j]) > 0.25)) { ok = 0; break; }
        if (ok) { gv_add(foot_, q); gv_add(gs, DIFF(q, INSET(q, 0.5))); }
    }
    GEOSGeometry *f = BUF(HL(foot_), 0.2, 16);
    for (int i = 0; i < gs->n; i++) EXT(ps, gs->v[i], u(0.3, 0.45), 0.0);
    ST(ps, "salt", NULL);
    SD_RET(f);
}

/* maahl / stele */
static int b_stele(SD_ARGS) {
    SD_BEGIN
    SoFrame ff = so_frame(cx, cy, a); double L = u(4.0, 5.2), W = u(2.2, 2.8), h = u(0.6, 0.85); GV *l = GVn(); CV *cuts = CVn(); SoSolids *ps = NS(); P2 p;
    static const double CHO[3] = { 0.0, 0.6, -0.6 };
    p = FF(&ff, -W / 4, 0); gv_add(l, RECT(p.x, p.y, L - W / 2, W, a)); p = FF(&ff, L / 2 - W / 2, 0); gv_add(l, DISC(p.x, p.y, W / 2, 20));
    GEOSGeometry *g = UL(l);
    DV xs = ARANGE(-L / 2 + 0.7, L / 2 - W / 2, 0.75);
    for (int i = 0; i < xs.n; i++) {
        DV vs = ARANGE(-W / 2 + 0.6, W / 2 - 0.4, 0.8);
        for (int j = 0; j < vs.n; j++) {
            if (!(RND() < 0.8)) continue;
            double v = vs.v[j] + u(-0.15, 0.15); p = FF(&ff, xs.v[i], v); double b = a + CHO[CH(rng, 3)];
            cv_add(cuts, RECT(p.x, p.y, 0.26, 0.6, b), h - 0.2);
        }
    }
    if (RND() < 0.35) { double u1 = u(-1, 1); P2 p1 = FF(&ff, u1, -W); double u2 = u(-1, 1); P2 p2_ = FF(&ff, u2, W); cv_add(cuts, WAVY(p1, p2_, rng, 0.4, 4, 0.4), 0.0); }
    GEOSGeometry *f = CHULL(g);
    EXT(ps, g, h, 0.0); ST(ps, "ruin", cuts);
    SD_RET(f);
}

/* maahl / well_bucket */
static int b_well_bucket(SD_ARGS) {
    SD_BEGIN
    double r = u(0.95, 1.2), h = u(1.3, 1.7); P2 c = POL(cx, cy, r + u(1.9, 2.4), a); SoSolids *ps = NS(), *rope = NS(); PV *p1 = PVn(2), *p2_ = PVn(3); CV *cuts = CVn();
    pv_add(p1, r * 0.8, 0.0); pv_add(p1, r, h); TURN(ps, cx, cy, p1, 14, 1.0, 0.0, 0.0);
    pv_add(p2_, r * 0.86 + 0.1, 0.0); pv_add(p2_, r * 0.86 + 0.1, h * 0.3); pv_add(p2_, r * 0.8, h * 0.32); TURN(ps, cx, cy, p2_, 14, 1.0, 0.0, 0.0);
    GV *hl = GVn(); gv_add(hl, DISC(cx, cy, r + 0.2, 20)); gv_add(hl, DISC(c.x, c.y, 1.5, 20)); GEOSGeometry *f = HL(hl);
    GEOSGeometry *q = DISC(cx, cy, r - 0.35, 12); cv_add(cuts, q, h - u(0.4, 0.9)); ST(ps, "wood", cuts);
    double tu = u(1.6, 2.2); ROD(rope, SPIRAL(c.x, c.y, 1.25, 0.4, tu, a, 30), 0.3, NULL, NULL, 1.1, 0.0, 0.0); ST(rope, "rope", NULL);
    SD_RET(f);
}

/* maahl / desert_rose */
static int b_desert_rose(SD_ARGS) {
    SD_BEGIN
    SoSolids *ps = NS(); double r = u(1.2, 1.7); int64_t n = RI(rng, 5, 8);
    for (int64_t i = 0; i < n; i++) {
        double t = u(0, 3.14), r0 = u(0, r * 0.5), t0 = u(0, 6.28); P2 c = POL(cx, cy, r0, t0);
        double L = r * u(1.0, 1.6), h = u(0.6, 1.2); LOAF(ps, c.x, c.y, t, L, 0.55, h, 0.0);
    }
    GEOSGeometry *f = DISC(cx, cy, r + 0.4, 20);
    ST(ps, "sandstone", NULL);
    SD_RET(f);
}

/* maahl / fallen_capital */
static int rf_capital(void *c, double t, double *o) {          /* r * (1 + 0.55 * max(0.0, (t + L * 0.1) / (L * 0.6)) ** 2) */
    double L = ((C2 *)c)->L, r = ((C2 *)c)->r, q; if (RF_PW(pmax(0.0, (t + L * 0.1) / (L * 0.6)), 2.0, &q) != SO_OK) return SO_ERR;
    *o = r * (1 + 0.55 * q); return SO_OK;
}
static int b_fallen_capital(SD_ARGS) {
    SD_BEGIN
    SoFrame ff = so_frame(cx, cy, a); double L = u(2.6, 3.4), r = u(0.9, 1.15); C2 cl = { L, r }; SoSolids *ps = NS(); P2 p;
    SPIN(ps, cx, cy, a, L, rf_capital, &cl, 1.1, 0.0, "round");
    p = FF(&ff, L / 2 + 0.3, 0); BLK(ps, p.x, p.y, 0.7, r * 3.6, a, r * 1.9, 0.0);
    p = FF(&ff, 0.3, 0); GEOSGeometry *f = RECT(p.x, p.y, L + 1.0, r * 3.8, a);
    ST(ps, "column", NULL);
    SD_RET(f);
}

/* maahl / fulgurite: br() recursing */
static int rf_fulg(void *c, double q, double *o) { double r = ((C1 *)c)->k; *o = r * (1 - 0.35 * q); return SO_OK; }
static void fulg_br(PyRandom *rng, SoSolids *ps, GV *foot_, P2 p, double t, double L, double r, int dep) {
    P2 e = POL(p.x, p.y, L, t); P2 m = POL((p.x + e.x) / 2, (p.y + e.y) / 2, u(-0.4, 0.4), t + 1.57); gv_add(foot_, DISC(e.x, e.y, 0.4, 20));
    PV *rp = PVn(3); pv_p(rp, p); pv_p(rp, m); pv_p(rp, e); C1 cr = { r }; ROD(ps, rp, 0, rf_fulg, &cr, 1.3, 0.0, 0.0);
    if (dep < 2) {
        int64_t n = RI(rng, 1, 2);
        for (int64_t k = 0; k < n; k++) { double t2 = t + u(-0.9, 0.9), L2 = L * u(0.55, 0.8); fulg_br(rng, ps, foot_, k == 0 ? e : m, t2, L2, r * 0.7, dep + 1); }
    }
}
static int b_fulgurite(SD_ARGS) {
    SD_BEGIN
    SoSolids *ps = NS(); GV *foot_ = GVn();
    double L = u(2.2, 3.0), r = u(0.45, 0.55); fulg_br(rng, ps, foot_, p2(cx, cy), a, L, r, 0); gv_add(foot_, DISC(cx, cy, 0.6, 20));
    GEOSGeometry *f = HL(foot_);
    ST(ps, "glass", NULL);
    SD_RET(f);
}

/* ====================================================================================================== corridor */
/* corridor / gas_cylinder */
static int rf_gas(void *c, double t, double *o) {
    double L = ((C3 *)c)->L, r = ((C3 *)c)->r, e = ((C3 *)c)->e, q;
    if (t < L / 2 - e) { *o = r; return SO_OK; }
    if (RF_PW((t - L / 2 + e) / e, 2.0, &q) != SO_OK) return SO_ERR;
    *o = r * sqrt(pmax(0.0, 1 - q)) + 0.02; return SO_OK;
}
static int b_gas_cylinder(SD_ARGS) {
    SD_BEGIN
    SoFrame ff = so_frame(cx, cy, a); double L = u(4.0, 5.2), r = u(0.75, 0.95), e = r * 0.9; C3 cl = { L, r, e }; SoSolids *ps = NS(), *cab = NS(); P2 p;
    SPIN(ps, cx, cy, a, L, rf_gas, &cl, 1.15, 0.0, "round");
    p = FF(&ff, L / 2 + 0.25, 0); LIE(ps, p.x, p.y, a, 0.7, 0.4, 0.0);
    PV *h = PVn(3); pv_p(h, FF(&ff, L / 2 + 0.5, 0)); { double v = u(-1, 1); pv_p(h, FF(&ff, L / 2 + 1.4, v)); }
    { double x = L / 2 + u(0.5, 2.0), v = u(1.5, 2.4); v = v * CHPM(rng); pv_p(h, FF(&ff, x, v)); }
    GV *hl = GVn(); gv_add(hl, RECT(cx, cy, L, 2 * r + 0.3, a)); gv_add(hl, LBUF(h, 0.4, 16)); GEOSGeometry *f = HL(hl);
    ST(ps, "metal", NULL); ROD(cab, h, 0.26, NULL, NULL, 1.1, 0.0, 0.0); ST(cab, "cable", NULL);
    SD_RET(f);
}

/* corridor / toolbox */
static int b_toolbox(SD_ARGS) {
    SD_BEGIN
    SoFrame ff = so_frame(cx, cy, a); double L = u(3.2, 4.0), W = u(1.7, 2.1), h = u(0.9, 1.2); SoSolids *ps = NS(); CV *cuts = CVn(); P2 p;
    BLK(ps, cx, cy, L, W, a, h, 0.0); p = FF(&ff, 0, W / 2 + 0.75); BLK(ps, p.x, p.y, L, 1.2, a, 0.35, 0.0);
    cv_add(cuts, RECT(cx, cy, L - 0.8, W - 0.8, a), 0.4); ST(ps, "metal", cuts);
    int64_t n = RI(rng, 1, 3);
    for (int64_t i = 0; i < n; i++) {
        double x = u(-L / 2, L / 2), v = -W / 2 - u(0.9, 1.8); P2 c = FF(&ff, x, v); double b = u(0, 3.14); GV *l = GVn(); SoSolids *s2 = NS();
        gv_add(l, CAPL(POL(c.x, c.y, 1.0, b), POL(c.x, c.y, 1.0, b + SD_PI), 0.5)); P2 q = POL(c.x, c.y, 1.0, b); gv_add(l, DISC(q.x, q.y, 0.45, 8));
        EXT(s2, UL(l), 0.38, 0.0); ST(s2, "metal", NULL);
    }
    p = FF(&ff, 0, -0.4);
    SD_RET(RECT(p.x, p.y, L + 0.4, W + 5.0, a));
}

/* corridor / helmet */
static int b_helmet(SD_ARGS) {
    SD_BEGIN
    SoFrame ff = so_frame(cx, cy, a); double r = u(1.1, 1.4); SoSolids *ps = NS(); PV *pf = PVn(12), *rp = PVn(2), *vp = PVn(4); CV *cuts = CVn();
    pv_add(pf, r + 0.15, 0.0); pv_add(pf, r + 0.15, 0.3); pv_add(pf, r, 0.32); pv_crv(pf, r, 0.3, 0.32, r * 1.25); TURN(ps, cx, cy, pf, 20, 1.0, 0.0, 0.0);
    pv_p(rp, FF(&ff, -r * 0.6, 0)); { double x = -r - u(1.0, 2.0), v = u(-1, 1); pv_p(rp, FF(&ff, x, v)); } ROD(ps, rp, 0.27, NULL, NULL, 1.0, 0.0, 0.0);
    pv_p(vp, FF(&ff, r * 0.2, -r * 0.6)); pv_p(vp, FF(&ff, r * 1.2, -r * 0.5)); pv_p(vp, FF(&ff, r * 1.2, r * 0.5)); pv_p(vp, FF(&ff, r * 0.2, r * 0.6));
    GEOSGeometry *vis = POLY(vp); P2 p = FF(&ff, -r - 1.0, 0);
    GEOSGeometry *f = CHULL(UNI(DISC(cx, cy, r + 0.4, 20), DISC(p.x, p.y, 1.2, 20)));
    cv_add(cuts, vis, r * u(0.45, 0.65)); ST(ps, "metal", cuts);
    SD_RET(f);
}

/* corridor / terminal */
static int b_terminal(SD_ARGS) {
    SD_BEGIN
    SoFrame ff = so_frame(cx, cy, a); double L = u(2.8, 3.6), W = u(2.2, 2.8), h = u(1.2, 1.7); SoSolids *ps = NS(); CV *keys = CVn(); P2 p;
    BLK(ps, cx, cy, L, W, a, h * 0.45, 0.0); p = FF(&ff, -L * 0.18, 0); RIDGE(ps, p.x, p.y, a + SD_PI / 2, W, L * 0.64, h, 0.0);
    DV vs = ARANGE(-W / 2 + 0.5, W / 2 - 0.3, 0.62);
    for (int i = 0; i < vs.n; i++) { p = FF(&ff, L * 0.3, vs.v[i]); cv_add(keys, RECT(p.x, p.y, 0.5, 0.4, a), h * 0.45 - 0.16); }
    GEOSGeometry *f = RECT(cx, cy, L + 0.2, W + 0.2, a);
    ST(ps, "metal", keys);
    SD_RET(f);
}

/* corridor / deck_hatch */
static int b_deck_hatch(SD_ARGS) {
    SD_BEGIN
    SoFrame ff = so_frame(cx, cy, a); double S = u(3.4, 4.2), R = S * 0.3; int op = RND() < 0.35; SoSolids *ps = NS(); P2 p;
    EXT(ps, BUF(BUF(RECT(cx, cy, S, S, a), -0.3, 16), 0.3, 16), 0.42, 0.0); EXT(ps, ANN(cx, cy, R, R - 0.45, 20), 0.85, 0.0); CYL(ps, cx, cy, 0.42, 0.95, 0.0, 10);
    for (int k = 0; k < 3; k++) EXT(ps, BARL(POL(cx, cy, 0.2, a + k * 2.094), POL(cx, cy, R - 0.2, a + k * 2.094), 0.36), 0.75, 0.0);
    { double vv[2] = { -S * 0.25, S * 0.25 }; for (int i = 0; i < 2; i++) { p = FF(&ff, -S / 2, vv[i]); BLK(ps, p.x, p.y, 0.6, 0.8, a, 0.7, 0.0); } }
    p = FF(&ff, 0.3, 0); GEOSGeometry *f = RECT(p.x, p.y, S + 1.6, S + 0.2, a);
    ST(ps, "plate", NULL);
    if (op) { p = FF(&ff, S / 2 + 0.6, 0); LGCUT(RECT(p.x, p.y, 0.6, S * 0.8, a), 0.6, "slot"); }
    SD_RET(f);
}

/* corridor / cell_rack */
static int b_cell_rack(SD_ARGS) {
    SD_BEGIN
    SoFrame ff = so_frame(cx, cy, a); int64_t n = RI(rng, 3, 5); SoSolids *ps = NS();
    BLK(ps, cx, cy, (double)n * 1.3 + 0.4, 1.7, a, 0.5, 0.0); int64_t miss = RI(rng, 0, n);
    for (int64_t i = 0; i < n; i++) {
        if (i == miss) continue;
        P2 c = FF(&ff, ((double)i - (double)(n - 1) / 2) * 1.3, 0); double h = u(1.4, 1.9); PV *pf = PVn(4);
        pv_add(pf, 0.5, 0.0); pv_add(pf, 0.5, h); pv_add(pf, 0.3, h + 0.05); pv_add(pf, 0.3, h + 0.3); TURN(ps, c.x, c.y, pf, 12, 1.0, 0.0, 0.0);
    }
    GEOSGeometry *f = RECT(cx, cy, (double)n * 1.3 + 0.6, 1.9, a);
    ST(ps, "metal", NULL);
    SD_RET(f);
}

/* corridor / hose_reel */
static int b_hose_reel(SD_ARGS) {
    SD_BEGIN
    double R = u(1.6, 2.1), er = R + u(2.0, 3.5), et = a + u(-0.4, 0.4); P2 e = POL(cx, cy, er, et); SoSolids *ps = NS(), *cab = NS(); PV *pf = PVn(2);
    pv_add(pf, R, 0.0); pv_add(pf, R, 0.35); TURN(ps, cx, cy, pf, 24, 1.0, 0.0, 0.0);
    double tu = u(1.8, 2.6); ROD(ps, SPIRAL(cx, cy, 0.7, R - 0.4, tu, a, 36), 0.33, NULL, NULL, 1.0, 0.3, 0.0);
    CYL(ps, cx, cy, 0.5, 1.0, 0.0, 10);
    GV *hl = GVn(); gv_add(hl, DISC(cx, cy, R + 0.2, 20)); gv_add(hl, DISC(e.x, e.y, 0.5, 20)); GEOSGeometry *f = HL(hl);
    ST(ps, "metal", NULL);
    ROD(cab, WIG(POL(cx, cy, R + 0.2, a), e, rng, 0.6, 4), 0.3, NULL, NULL, 1.1, 0.0, 0.0); ST(cab, "cable", NULL);
    SD_RET(f);
}

/* corridor / junction_box */
static int b_junction_box(SD_ARGS) {
    SD_BEGIN
    double S = u(1.8, 2.3), h = u(0.9, 1.3); SoSolids *ps = NS(); GV *foot_ = GVn(); CV *cuts = CVn(); int64_t ks[4];
    BLK(ps, cx, cy, S, S, a, h, 0.0); gv_add(foot_, RECT(cx, cy, S + 0.3, S + 0.3, a));
    int64_t k = RI(rng, 2, 3); K(py_sample_e(rng, 4, k, ks));
    for (int64_t i = 0; i < k; i++) {
        double t = a + (double)ks[i] * 1.5708; P2 p0 = POL(cx, cy, S / 2 - 0.3, t); double er = S / 2 + u(2.0, 3.2), et = t + u(-0.3, 0.3); P2 e = POL(cx, cy, er, et);
        ROD(ps, WIG(p0, e, rng, 0.4, 3), 0.3, NULL, NULL, 1.1, 0.0, 0.0); gv_add(foot_, DISC(e.x, e.y, 0.4, 20));
    }
    GEOSGeometry *f = HL(foot_);
    cv_add(cuts, RECT(cx, cy, S - 0.8, S - 0.8, a), h - 0.3); ST(ps, "metal", cuts);
    SD_RET(f);
}

/* corridor / magboots */
static int b_magboots(SD_ARGS) {
    SD_BEGIN
    SoSolids *ps = NS(); GV *foot_ = GVn();
    for (int sv = -1; sv <= 1; sv += 2) {
        double rr = u(0.9, 1.4); P2 c = POL(cx, cy, rr, a + sv * 1.57); double b = a + u(-0.7, 0.7); SoFrame f2 = so_frame(c.x, c.y, b); P2 p;
        LOAF(ps, c.x, c.y, b, 2.6, 1.2, 0.6, 0.0);
        p = FF(&f2, -0.7, 0); CYL(ps, p.x, p.y, 0.55, u(1.3, 1.8), 0.0, 12);
        p = FF(&f2, 0.3, 0); BLK(ps, p.x, p.y, 0.4, 1.3, b, 0.75, 0.0);
        gv_add(foot_, RECT(c.x, c.y, 2.8, 1.4, b));
    }
    GEOSGeometry *f = HL(foot_);
    ST(ps, "metal", NULL);
    SD_RET(f);
}

/* corridor / vent_fan */
static int b_vent_fan(SD_ARGS) {
    SD_BEGIN
    double R = u(1.8, 2.3); int64_t n = RI(rng, 4, 6); SoSolids *ps = NS();
    EXT(ps, ANN(cx, cy, R, R - 0.5, 24), 0.7, 0.0); EXT(ps, DISC(cx, cy, R - 0.4, 24), 0.2, 0.0); CYL(ps, cx, cy, 0.5, 0.75, 0.0, 10);
    for (int64_t k = 0; k < n; k++) {
        if (RND() < 0.15) continue;
        double t = a + 6.2832 * (double)k / (double)n; PV *pg = PVn(4);
        pv_p(pg, POL(cx, cy, 0.4, t - 0.3)); pv_p(pg, POL(cx, cy, R - 0.6, t - 0.12)); pv_p(pg, POL(cx, cy, R - 0.6, t + 0.42)); pv_p(pg, POL(cx, cy, 0.4, t + 0.3));
        EXT(ps, POLY(pg), 0.5, 0.0);
    }
    GEOSGeometry *f = DISC(cx, cy, R + 0.2, 20);
    ST(ps, "metal", NULL);
    SD_RET(f);
}

/* corridor / hover_pallet */
static int b_hover_pallet(SD_ARGS) {
    SD_BEGIN
    SoFrame ff = so_frame(cx, cy, a); double L = u(4.4, 5.6), W = u(3.0, 3.6); SoSolids *ps = NS(); CV *cuts = CVn(); P2 p;
    EXT(ps, BUF(BUF(RECT(cx, cy, L, W, a), -0.4, 16), 0.4, 16), 0.5, 0.0);
    for (int sx = -1; sx <= 1; sx += 2) for (int sy = -1; sy <= 1; sy += 2) { p = FF(&ff, sx * (L / 2 - 0.7), sy * (W / 2 - 0.7)); CYL(ps, p.x, p.y, 0.5, 0.75, 0.0, 10); }
    DV xs = ARANGE(-L / 2 + 1.6, L / 2 - 1.2, 0.8);
    for (int i = 0; i < xs.n; i++) { double x = xs.v[i]; PV *pl = PVn(3); pv_p(pl, FF(&ff, x - 0.5, -W * 0.25)); pv_p(pl, FF(&ff, x, 0)); pv_p(pl, FF(&ff, x - 0.5, W * 0.25)); cv_add(cuts, LINE(pl, 0.3), 0.3); }
    GEOSGeometry *f = RECT(cx, cy, L + 0.2, W + 0.2, a);
    ST(ps, "metal", cuts);
    SD_RET(f);
}

/* corridor / dead_drone */
static int b_dead_drone(SD_ARGS) {
    SD_BEGIN
    double r = u(0.9, 1.15), arm = u(1.9, 2.4); SoSolids *ps = NS(); GV *foot_ = GVn();
    DOME(ps, cx, cy, r, 0.8, 0.0, 1.0, 0.0, 20); CYL(ps, cx, cy, r + 0.12, 0.3, 0.0, 20); gv_add(foot_, DISC(cx, cy, r, 20));
    for (int k = 0; k < 4; k++) {
        double t = a + 0.785 + k * 1.5708; double er = arm * (RND() < 0.2 ? 0.55 : 1.0); P2 e = POL(cx, cy, er, t);
        EXT(ps, BARL(POL(cx, cy, r * 0.5, t), e, 0.5), 0.4, 0.0); EXT(ps, ANN(e.x, e.y, 0.75, 0.3, 12), 0.5, 0.0); gv_add(foot_, DISC(e.x, e.y, 0.8, 20));
    }
    GEOSGeometry *f = HL(foot_);
    ST(ps, "metal", NULL);
    SD_RET(f);
}

/* corridor / welding_set */
static int b_welding_set(SD_ARGS) {
    SD_BEGIN
    SoFrame ff = so_frame(cx, cy, a); double L = u(3.6, 4.4), r = 0.6; SoSolids *ps = NS(), *cab = NS(); P2 p;
    p = FF(&ff, 0, -r); LIE(ps, p.x, p.y, a, L, r, 0.0); p = FF(&ff, 0, r); LIE(ps, p.x, p.y, a, L * 0.9, r, 0.0);
    BLK(ps, cx, cy, 0.5, 4 * r + 0.3, a, 0.8, 0.0);
    p = FF(&ff, L / 2 + 0.2, -r); LIE(ps, p.x, p.y, a, 0.5, 0.3, 0.0); p = FF(&ff, L / 2 + 0.2, r); LIE(ps, p.x, p.y, a, 0.5, 0.3, 0.0);
    double x = L / 2 + u(1.5, 2.6), v = u(-2.2, 2.2); P2 e = FF(&ff, x, v); PV *h = WIG(FF(&ff, L / 2 + 0.7, 0), e, rng, 0.7, 4);
    GV *hl = GVn(); gv_add(hl, RECT(cx, cy, L + 0.6, 4 * r + 0.4, a)); gv_add(hl, DISC(e.x, e.y, 0.8, 20)); GEOSGeometry *f = HL(hl);
    ST(ps, "metal", NULL);
    ROD(cab, h, 0.26, NULL, NULL, 1.1, 0.0, 0.0); CONE(cab, e.x, e.y, 0.42, 0.7, 0.24, 0.0, 16, 1.0, 0.0); ST(cab, "cable", NULL);
    SD_RET(f);
}

/* corridor / valve_stub */
static int b_valve_stub(SD_ARGS) {
    SD_BEGIN
    SoFrame ff = so_frame(cx, cy, a); double L = u(3.6, 4.8), r = u(0.6, 0.75), R = r + 0.75; SoSolids *ps = NS(); P2 p;
    LIE(ps, cx, cy, a, L, r, 0.0);
    p = FF(&ff, -L / 2 + 0.2, 0); LIE(ps, p.x, p.y, a, 0.45, r + 0.3, 0.0); p = FF(&ff, L / 2 - 0.2, 0); LIE(ps, p.x, p.y, a, 0.45, r + 0.3, 0.0);
    CYL(ps, cx, cy, 0.35, r * 1.15 + 0.9, 0.0, 8);
    HANG(ps, UNI(ANN(cx, cy, R, R - 0.42, 16), BARL(POL(cx, cy, R, a + 0.8), POL(cx, cy, R, a + 0.8 + SD_PI), 0.36)), r * 1.15 + 0.5, r * 1.15 + 0.9);
    GEOSGeometry *f = RECT(cx, cy, L + 0.2, 2 * R + 0.3, a);
    ST(ps, "pipe", NULL);
    SD_RET(f);
}

/* corridor / deck_stencil */
static int b_deck_stencil(SD_ARGS) {
    SD_BEGIN
    SoFrame ff = so_frame(cx, cy, a); GV *gs = GVn(); double x = -2.6;
    static const char *const SEGS[9] = { "abcdef", "bc", "abged", "abgcd", "fgbc", "afgcd", "abc", "abcdefg", "abfgcd" };          /* "0" "1" "2" "3" "4" "5" "7" "8" "9" */
    static const double S[7][4] = { { 0, 1.4, 0.9, 1.4 }, { 0.9, 1.4, 0.9, 0 }, { 0.9, 0, 0.9, -1.4 }, { 0, -1.4, 0.9, -1.4 }, { 0, 0, 0, -1.4 }, { 0, 1.4, 0, 0 }, { 0, 0, 0.9, 0 } };   /* a .. g */
    int64_t n = RI(rng, 2, 3), chs[3];
    for (int64_t i = 0; i < n; i++) chs[i] = CH(rng, 9);
    for (int64_t i = 0; i < n; i++) {
        for (const char *k = SEGS[chs[i]]; *k; k++) { const double *s = S[*k - 'a']; gv_add(gs, BARL(FF(&ff, x + s[0], s[1]), FF(&ff, x + s[2], s[3]), 0.34)); }
        x += 1.7;
    }
    PV *pl = PVn(3); pv_p(pl, FF(&ff, x + 0.2, -1.2)); pv_p(pl, FF(&ff, x + 1.3, 0)); pv_p(pl, FF(&ff, x + 0.2, 1.2)); gv_add(gs, LINE(pl, 0.36));
    P2 p = FF(&ff, (x - 1.2) / 2, 0); GEOSGeometry *f = RECT(p.x, p.y, x + 4.6, 3.6, a);
    LGCUT(UL(gs), 0.25, "slot");
    SD_RET(f);
}

/* corridor / drip_tray */
static int b_drip_tray(SD_ARGS) {
    SD_BEGIN
    GEOSGeometry *p = BLOB(cx, cy, u(1.6, 2.2), rng, 4); double S = u(1.4, 1.8), x = cx + u(-0.4, 0.4), y = cy + u(-0.4, 0.4); SoFrame f2 = so_frame(x, y, a); GV *l = GVn();
    DV vs = ARANGE(-S / 2 + 0.3, S / 2, 0.55);
    for (int i = 0; i < vs.n; i++) gv_add(l, BARL(FF(&f2, -S / 2, vs.v[i]), FF(&f2, S / 2, vs.v[i]), 0.3));
    GEOSGeometry *drain = INTER(UL(l), INSET(p, 0.3)), *f = BUF(p, 0.3, 16);
    LGCUT(p, 0.3, "coolant");
    if (!EMPTY(drain)) LGCUT(drain, 0.7, "slot");
    SD_RET(f);
}

/* corridor / cryo_canister */
static int rf_cryo(void *c, double t, double *o) {
    double L = ((C3 *)c)->L, r = ((C3 *)c)->r, e = ((C3 *)c)->e, k, q;
    if (fabs(t) < L / 2 - e) k = 1.0;
    else { if (RF_PW((fabs(t) - L / 2 + e) / e, 2.0, &q) != SO_OK) return SO_ERR; k = sqrt(pmax(0.0, 1 - q)); }
    *o = r * k + (fabs(fabs(t) - L * 0.3) < 0.2 ? 0.12 : 0.0) + 0.02; return SO_OK;
}
static int b_cryo_canister(SD_ARGS) {
    SD_BEGIN
    SoFrame ff = so_frame(cx, cy, a); double L = u(3.4, 4.4), r = u(0.85, 1.05), e = r; C3 cl = { L, r, e }; SoSolids *ps = NS(); CV *cuts = CVn();
    SPIN(ps, cx, cy, a, L, rf_cryo, &cl, 1.15, 0.0, "round");
    for (int sv = -1; sv <= 1; sv += 2) { P2 p = FF(&ff, sv * L * 0.3, 0); BLK(ps, p.x, p.y, 0.6, 2 * r + 0.9, a, 0.45, 0.0); }
    GEOSGeometry *f = RECT(cx, cy, L + 0.3, 2 * r + 1.0, a);
    cv_add(cuts, BUF(RECT(cx, cy, L * 0.32, r * 0.9, a), 0.15, 16), r * 1.15 * 0.75); ST(ps, "glass", cuts);
    SD_RET(f);
}

/* corridor / cable_spool */
static int b_cable_spool(SD_ARGS) {
    SD_BEGIN
    SoFrame ff = so_frame(cx, cy, a); double R = u(1.3, 1.7), W = u(1.6, 2.2); SoSolids *ps = NS(), *cab = NS();
    LIE(ps, cx, cy, a, W, R * 0.6, 0.0);
    for (int sv = -1; sv <= 1; sv += 2) { P2 p = FF(&ff, sv * W / 2, 0); LIE(ps, p.x, p.y, a, 0.42, R, 0.0); }
    double x = u(-0.5, 0.5), v = -R - u(1.5, 3.0); P2 e = FF(&ff, x, v);
    GV *hl = GVn(); gv_add(hl, RECT(cx, cy, W + 0.6, 2 * R + 0.2, a)); gv_add(hl, DISC(e.x, e.y, 0.4, 20)); GEOSGeometry *f = HL(hl);
    ST(ps, "metal", NULL);
    ROD(cab, WIG(FF(&ff, 0, -R - 0.3), e, rng, 0.5, 3), 0.28, NULL, NULL, 1.1, 0.0, 0.0); ST(cab, "cable", NULL);
    SD_RET(f);
}

/* corridor / plate_stack */
static int b_plate_stack(SD_ARGS) {
    SD_BEGIN
    double r = u(1.5, 1.9); SoSolids *ps = NS(); int64_t n = RI(rng, 2, 4);
    for (int64_t i = 0; i < n; i++) {
        double on = i > 0 ? 1.0 : 0.0, x = cx + u(-0.35, 0.35) * on, y = cy + u(-0.35, 0.35) * on, t = a + u(-0.3, 0.3) * on;
        EXT(ps, NGON(x, y, r, 6, t), 0.36, (double)i * 0.34);
    }
    GEOSGeometry *f = DISC(cx, cy, r + 0.5, 20);
    ST(ps, "plate", NULL);
    SD_RET(f);
}

/* corridor / mess_tray */
static int b_mess_tray(SD_ARGS) {
    SD_BEGIN
    SoFrame ff = so_frame(cx, cy, a); double L = u(2.8, 3.4), W = u(2.0, 2.4), x = L / 2 + u(0.9, 1.5), v = u(-1, 1); P2 c = FF(&ff, x, v), p; CV *cuts = CVn(), *c2 = CVn(); SoSolids *s1 = NS(), *s2 = NS();
    p = FF(&ff, -L * 0.22, 0); cv_add(cuts, RECT(p.x, p.y, L * 0.38, W - 0.7, a), 0.2);
    p = FF(&ff, L * 0.24, W * 0.2); cv_add(cuts, RECT(p.x, p.y, L * 0.34, W * 0.36, a), 0.2);
    p = FF(&ff, L * 0.24, -W * 0.22); cv_add(cuts, RECT(p.x, p.y, L * 0.34, W * 0.32, a), 0.2);
    GV *hl = GVn(); gv_add(hl, RECT(cx, cy, L, W, a)); gv_add(hl, DISC(c.x, c.y, 0.8, 20)); GEOSGeometry *f = HL(hl);
    EXT(s1, BUF(BUF(RECT(cx, cy, L, W, a), -0.3, 16), 0.3, 16), 0.42, 0.0); ST(s1, "tin", cuts);
    PV *pf = PVn(2); pv_add(pf, 0.5, 0.0); pv_add(pf, 0.6, 0.9); TURN(s2, c.x, c.y, pf, 12, 1.0, 0.0, 0.0); cv_add(c2, DISC(c.x, c.y, 0.32, 8), 0.4); ST(s2, "tin", c2);
    SD_RET(f);
}

/* ====================================================================================================== infestation */
/* infestation / cocoon */
static int rf_cocoon(void *c, double t, double *o) {          /* r * math.sqrt(max(0.0, 1 - (2 * t / L) ** 2)) ** 0.7 * (1 + 0.1 * math.sin(t * 5 + ph)) + 0.05 */
    C4 *k = (C4 *)c; double q, p; if (RF_PW(2 * t / k->L, 2.0, &q) != SO_OK || RF_PW(sqrt(pmax(0.0, 1 - q)), 0.7, &p) != SO_OK) return SO_ERR;
    *o = k->W * p * (1 + 0.1 * SIN(t * 5 + k->ph)) + 0.05; return SO_OK;
}
static int rf_strand(void *c, double q, double *o) { (void)c; *o = 0.34 - 0.1 * q; return SO_OK; }
static int b_cocoon(SD_ARGS) {
    SD_BEGIN
    SoFrame ff = so_frame(cx, cy, a); double L = u(5.0, 6.5), r = u(1.0, 1.3), ph = u(0, 6); C4 cl = { L, r, 0, ph }; SoSolids *ps = NS();
    SPIN(ps, cx, cy, a, L, rf_cocoon, &cl, 1.0, 0.0, "round");
    int64_t n = RI(rng, 2, 4);
    for (int64_t i = 0; i < n; i++) { double t = u(0, 6.28); PV *rp = PVn(2); pv_p(rp, POL(cx, cy, r * 0.5, t)); pv_p(rp, POL(cx, cy, r + u(1.6, 2.6), t)); ROD(ps, rp, 0, rf_strand, NULL, 0.9, 0.0, 0.0); }
    ST(ps, "web", NULL);
    if (RND() < 0.6) { double v = u(-0.3, 0.3); P2 p = FF(&ff, L / 2 + 0.9, v); SoSolids *s2 = NS(); LOAF(s2, p.x, p.y, a, 1.5, 0.75, 0.6, 0.0); ST(s2, "metal", NULL); }
    SD_RET(RECT(cx, cy, L + 3.4, 2 * r + 3.0, a));
}

/* infestation / burst_egg */
static int b_burst_egg(SD_ARGS) {
    SD_BEGIN
    double r = u(1.0, 1.35), h = r * u(1.3, 1.7); int n = 4; SoSolids *ps = NS(); PV *pf = PVn(4); CV *cuts = CVn();
    pv_add(pf, r * 0.8, 0.0); pv_add(pf, r, h * 0.4); pv_add(pf, r * 0.75, h * 0.85); pv_add(pf, r * 0.6, h); TURN(ps, cx, cy, pf, 16, 1.0, 0.0, 0.0);
    cv_add(cuts, DISC(cx, cy, r * 0.42, 10), h * 0.35);
    for (int k = 0; k < n; k++) cv_add(cuts, BARL(p2(cx, cy), POL(cx, cy, r, a + k * 6.2832 / n), 0.26), h * 0.6);
    GEOSGeometry *f = DISC(cx, cy, r + 0.3, 20);
    ST(ps, "egg", cuts);
    SD_RET(f);
}

/* infestation / dead_crawler */
static int rf_crawler(void *c, double t, double *o) {
    double L = ((C2 *)c)->L, W = ((C2 *)c)->r, q; if (RF_PW(2 * t / L, 2.0, &q) != SO_OK) return SO_ERR;
    *o = W / 2 * sqrt(pmax(0.0, 1 - q)) + 0.05; return SO_OK;
}
static int rf_crawler_tail(void *c, double q, double *o) { (void)c; *o = 0.36 - 0.14 * q; return SO_OK; }
static int rf_crawler_leg(void *c, double q, double *o) { (void)c; *o = 0.3 - 0.07 * q; return SO_OK; }
static int b_dead_crawler(SD_ARGS) {
    SD_BEGIN
    SoFrame ff = so_frame(cx, cy, a); double L = u(2.6, 3.4), W = L * 0.6; int64_t n = RI(rng, 3, 4); C2 cl = { L, W }; SoSolids *ps = NS(); PV *rp = PVn(3);
    SPIN(ps, cx, cy, a, L, rf_crawler, &cl, 0.85, 0.0, "round");
    pv_p(rp, FF(&ff, -L / 2, 0)); { double v = u(-0.6, 0.6); pv_p(rp, FF(&ff, -L / 2 - 1.2, v)); } { double v = u(-1.2, 1.2); pv_p(rp, FF(&ff, -L / 2 - 2.2, v)); }
    ROD(ps, rp, 0, rf_crawler_tail, NULL, 1.0, 0.0, 0.0);
    for (int64_t k = 0; k < n; k++)
        for (int sv = -1; sv <= 1; sv += 2) {
            double x = ((double)k - (double)(n - 1) / 2) * L * 0.24, curl = u(0.2, 0.9); PV *lp = PVn(3);
            pv_p(lp, FF(&ff, x, sv * W * 0.3)); pv_p(lp, FF(&ff, x + 0.2, sv * (W / 2 + 0.8))); pv_p(lp, FF(&ff, x + curl, sv * (W / 2 + 0.3 + curl)));
            ROD(ps, lp, 0, rf_crawler_leg, NULL, 1.0, 0.0, 0.0);
        }
    P2 p = FF(&ff, -1.0, 0); GEOSGeometry *f = RECT(p.x, p.y, L + 2.6, W + 2.6, a);
    ST(ps, "carapace", NULL);
    SD_RET(f);
}

/* infestation / resin_stump */
static int b_resin_stump(SD_ARGS) {
    SD_BEGIN
    double r = u(1.2, 1.7), h = u(1.6, 2.6); int64_t n = RI(rng, 5, 8); double tw = u(0.12, 0.3); tw = tw * CHPM(rng); SoSolids *ps = NS(); CV *cuts = CVn();
    for (int k = 0; k < 7; k++) EXT(ps, STAR(cx, cy, r * (1 - 0.07 * k), r * (1 - 0.07 * k) * 0.7, (int)n, a + tw * k), h * (k + 1) / 7, 0.0);
    GEOSGeometry *f = DISC(cx, cy, r + 0.2, 20);
    cv_add(cuts, DISC(cx, cy, r * 0.25, 8), h - 0.4); ST(ps, "resin", cuts);
    SD_RET(f);
}

/* infestation / spore_chimneys */
static int b_spore_chimneys(SD_ARGS) {
    SD_BEGIN
    SoSolids *ps = NS(); CV *cuts = CVn(); GV *foot_ = GVn(); int64_t n = RI(rng, 2, 4);
    for (int64_t i = 0; i < n; i++) {
        double rr = i == 0 ? 0.0 : u(1.6, 2.2); P2 p = POL(cx, cy, rr, a + (double)i * 2.1); double r = u(0.8, 1.2) * (i == 0 ? 1.0 : 0.75), h = r * u(1.6, 2.4);
        PV *pf = PVn(3); pv_add(pf, r, 0.0); pv_add(pf, r * 0.6, h * 0.5); pv_add(pf, r * 0.5, h); TURN(ps, p.x, p.y, pf, 12, 1.0, 0.0, 0.0);
        cv_add(cuts, DISC(p.x, p.y, r * 0.28, 8), h - 0.6); gv_add(foot_, DISC(p.x, p.y, r, 20));
    }
    GEOSGeometry *f = BUF(HL(foot_), 0.2, 16);
    ST(ps, "spore", cuts);
    SD_RET(f);
}

/* infestation / acid_holes */
static int b_acid_holes(SD_ARGS) {
    SD_BEGIN
    GV *l = GVn(); int64_t n = RI(rng, 3, 6);
    for (int64_t i = 0; i < n; i++) { double r0 = u(0, 2.2), t0 = u(0, 6.28); P2 p = POL(cx, cy, r0, t0); double br = u(0.5, 0.95); gv_add(l, BLOB(p.x, p.y, br, rng, 3)); }
    GEOSGeometry *g = UL(l), *f = BUF(CHULL(g), 0.3, 16);
    LGCUT(g, u(0.4, 0.7), "acid");
    SD_RET(f);
}

/* infestation / hive_vent (ground) */
static int b_hive_vent(SD_ARGS) {
    SD_BEGIN
    double R = u(1.5, 2.0); int64_t n = RI(rng, 6, 9); SoSolids *ps = NS();
    for (int64_t k = 0; k < n; k++) {
        double t = a + 6.2832 * (double)k / (double)n; P2 p = POL(cx, cy, R, t); double h = u(0.7, 1.0);
        DOME(ps, p.x, p.y, R * 3.4 / (double)n, h, 0.0, 0.7, a + 6.2832 * (double)k / (double)n, 20);
    }
    GEOSGeometry *f = DISC(cx, cy, R + 1.0, 20);
    GEOSGeometry *g = DISC(cx, cy, R - 0.9, 14); LGCUT(g, u(0.6, 0.85), "pit"); ST(ps, "flesh", NULL);
    SD_RET(f);
}

/* infestation / molt */
static int b_molt(SD_ARGS) {
    SD_BEGIN
    SoFrame ff = so_frame(cx, cy, a); int64_t n = RI(rng, 4, 6); SoSolids *ps = NS(); double x = (double)(-n) * 0.5; CV *cuts = CVn();
    for (int64_t i = 0; i < n; i++) {
        double w = u(2.0, 2.4) * (1 - 0.12 * (double)i), v = u(-0.1, 0.1); P2 c = FF(&ff, x, v);
        LOAF(ps, c.x, c.y, a + SD_PI / 2, w, 1.1, 0.9 * (1 - 0.1 * (double)i), 0.0); x += 0.9;
    }
    GEOSGeometry *f = RECT(cx, cy, (double)n * 0.9 + 1.4, 2.6, a);
    cv_add(cuts, BARL(FF(&ff, (double)(-n) * 0.5 - 0.6, 0), FF(&ff, (double)n * 0.2, 0), 0.3), 0.3); ST(ps, "carapace", cuts);
    SD_RET(f);
}

/* infestation / tendril_knot */
static int rf_tendril(void *c, double q, double *o) { (void)c; *o = 0.5 - 0.27 * q; return SO_OK; }
static int b_tendril_knot(SD_ARGS) {
    SD_BEGIN
    SoSolids *ps = NS(); GV *foot_ = GVn(); double r = u(0.9, 1.2), h = u(0.9, 1.2);
    DOME(ps, cx, cy, r, h, 0.0, 1.0, 0.0, 20);
    int64_t n = RI(rng, 4, 6);
    for (int64_t i = 0; i < n; i++) {
        double t = a + (double)i * 1.2 + u(-0.3, 0.3), er = u(2.4, 3.6), et = t + u(-0.5, 0.5); P2 e = POL(cx, cy, er, et); double mt = t + u(-0.5, 0.5); P2 m = POL(cx, cy, 1.5, mt);
        PV *rp = PVn(3); pv_p(rp, POL(cx, cy, 0.3, t)); pv_p(rp, m); pv_p(rp, e); ROD(ps, rp, 0, rf_tendril, NULL, 1.0, 0.0, 0.0); gv_add(foot_, DISC(e.x, e.y, 0.3, 20));
    }
    GEOSGeometry *f = HL(foot_);
    ST(ps, "tendril", NULL);
    SD_RET(f);
}

/* infestation / claw_marks */
static int b_claw_marks(SD_ARGS) {
    SD_BEGIN
    SoFrame ff = so_frame(cx, cy, a); int64_t n = RI(rng, 3, 4); double L = u(4.0, 6.0), bend = u(-0.8, 0.8); GV *gs = GVn();
    for (int64_t i = 0; i < n; i++) {
        double d = (double)i - (double)(n - 1) / 2; GV *l = GVn();
        gv_add(l, TRI(FF(&ff, 0, d * 0.85), FF(&ff, L / 2, d * 0.7 + bend), 0.5));
        P2 p0 = FF(&ff, 0, d * 0.85); double x = -L / 2 * u(0.7, 1.0); gv_add(l, TRI(p0, FF(&ff, x, d * 0.95), 0.5));
        gv_add(gs, UL(l));
    }
    GEOSGeometry *f = RECT(cx, cy, L + 0.4, (double)n * 0.9 + 1.8, a);
    LGCUT(UL(gs), 0.4, "slot");
    SD_RET(f);
}

/* infestation / sac_cluster */
static int rf_sac(void *c, double q, double *o) { (void)c; *o = 0.36 - 0.12 * q; return SO_OK; }
static int b_sac_cluster(SD_ARGS) {
    SD_BEGIN
    SoSolids *ps = NS(); GV *foot_ = GVn(); int64_t n = RI(rng, 3, 6);
    for (int64_t i = 0; i < n; i++) {
        double r0 = u(0, 1.6), t0 = u(0, 6.28); P2 p = POL(cx, cy, r0, t0); double r = u(0.6, 1.1), h = r * u(0.9, 1.3);
        DOME(ps, p.x, p.y, r, h, 0.0, 1.0, 0.0, 12); gv_add(foot_, DISC(p.x, p.y, r, 20));
    }
    int64_t m = RI(rng, 2, 4);
    for (int64_t i = 0; i < m; i++) {
        double t = u(0, 6.28), er = u(2.6, 3.4); P2 e = POL(cx, cy, er, t);
        ROD(ps, WIG(POL(cx, cy, 0.8, t), e, rng, 0.3, 3), 0, rf_sac, NULL, 0.9, 0.0, 0.0); gv_add(foot_, DISC(e.x, e.y, 0.3, 20));
    }
    GEOSGeometry *f = HL(foot_);
    ST(ps, "pustule", NULL);
    SD_RET(f);
}

/* infestation / flamethrower */
static int b_flamethrower(SD_ARGS) {
    SD_BEGIN
    SoFrame ff = so_frame(cx, cy, a); double L = u(2.6, 3.2); SoSolids *ps = NS(), *gun = NS(), *all = NS(); P2 p;
    p = FF(&ff, 0, -0.5); LIE(ps, p.x, p.y, a, L, 0.5, 0.0); p = FF(&ff, 0, 0.5); LIE(ps, p.x, p.y, a, L, 0.5, 0.0); BLK(ps, cx, cy, 0.5, 2.3, a, 0.7, 0.0);
    double x = L / 2 + u(2.6, 3.2), v = u(1.6, 2.6); v = v * CHPM(rng); P2 g0 = FF(&ff, x, v); double b = a + u(-0.4, 0.4); SoFrame f2 = so_frame(g0.x, g0.y, b);
    LIE(gun, g0.x, g0.y, b, 3.4, 0.36, 0.0); p = FF(&f2, -0.9, -0.5); BLK(gun, p.x, p.y, 0.5, 1.0, b, 0.5, 0.0); p = FF(&f2, 1.9, 0); CONE(gun, p.x, p.y, 0.5, 0.6, 0.3, 0.0, 16, 1.0, 0.0);
    GV *hl = GVn(); gv_add(hl, RECT(cx, cy, L, 2.4, a)); gv_add(hl, RECT(g0.x, g0.y, 4.2, 1.8, b)); GEOSGeometry *f = HL(hl);
    CAT(all, ps); CAT(all, gun); ROD(all, WIG(FF(&ff, L / 2 - 0.2, 0), FF(&f2, -1.6, 0), rng, 0.4, 3), 0.26, NULL, NULL, 1.1, 0.0, 0.0);
    ST(all, "metal", NULL);
    SD_RET(f);
}

/* infestation / scanner_and_mags */
static int b_scanner_and_mags(SD_ARGS) {
    SD_BEGIN
    SoFrame ff = so_frame(cx, cy, a); double L = u(2.0, 2.5), W = u(1.4, 1.7); SoSolids *ps = NS(); CV *cuts = CVn(); P2 p;
    BLK(ps, cx, cy, L, W, a, 0.6, 0.0); p = FF(&ff, -L / 2 - 0.4, 0); BLK(ps, p.x, p.y, 0.9, 0.6, a, 0.45, 0.0);
    p = FF(&ff, 0.15, 0); cv_add(cuts, RECT(p.x, p.y, L * 0.55, W - 0.6, a), 0.38); ST(ps, "metal", cuts);
    int64_t n = RI(rng, 1, 3);
    for (int64_t i = 0; i < n; i++) { double v = W / 2 + u(1.3, 2.0); P2 c = FF(&ff, -1.5 + (double)i * 1.9, v); double b = a + u(-0.5, 0.5); SoSolids *s2 = NS(); BLK(s2, c.x, c.y, 1.5, 0.6, b, 0.42, 0.0); ST(s2, "metal", NULL); }
    p = FF(&ff, 0.3, 1.0);
    SD_RET(RECT(p.x, p.y, L + 3.6, W + 3.6, a));
}

/* infestation / slime_trail */
static int b_slime_trail(SD_ARGS) {
    SD_BEGIN
    SoFrame ff = so_frame(cx, cy, a); double L = u(6.0, 9.0); PV *pts = WIG(FF(&ff, -L / 2, 0), FF(&ff, L / 2, 0), rng, 1.0, 6); GV *l = GVn();
    gv_add(l, LBUF(pts, u(0.45, 0.6), 6));
    for (int i = 1; i < pts->n - 1; i += 2) { double x = pts->xy[2 * i] + u(-0.5, 0.5), y = pts->xy[2 * i + 1] + u(-0.5, 0.5), r = u(0.5, 0.8); gv_add(l, DISC(x, y, r, 10)); }
    GEOSGeometry *g = UL(l), *f = CHULL(g);
    LGCUT(g, 0.28, "acid");
    SD_RET(f);
}

/* infestation / spine_row */
static int b_spine_row(SD_ARGS) {
    SD_BEGIN
    double R = u(3.0, 5.0); int64_t n = RI(rng, 4, 7); SoSolids *ps = NS(); double half = 0.75 * (double)(n - 1) / R;
    PV *pts = ARC(cx - R * COS(a), cy - R * SIN(a), R, a - half, a + half, (int)(n - 1));
    for (int i = 0; i < pts->n; i++) {
        double k = 1 - fabs((double)i - (double)(n - 1) / 2) / (double)n; double h = u(1.6, 2.8) * (0.5 + 0.7 * k);
        CONE(ps, pts->xy[2 * i], pts->xy[2 * i + 1], 0.6 * (0.6 + 0.6 * k), h, 0.23, 0.0, 7, 0.8, a);
    }
    GEOSGeometry *f = LBUF(pts, 1.0, 16);
    ST(ps, "spine", NULL);
    SD_RET(f);
}

/* infestation / larvae */
static int rf_larva(void *c, double t, double *o) {          /* 0.42 * math.sqrt(max(0.0, 1 - (2 * t / L) ** 2)) * (1 + 0.18 * math.sin(t * 9 + ph)) + 0.05 */
    C4 *k = (C4 *)c; double q; if (RF_PW(2 * t / k->L, 2.0, &q) != SO_OK) return SO_ERR;
    *o = 0.42 * sqrt(pmax(0.0, 1 - q)) * (1 + 0.18 * SIN(t * 9 + k->ph)) + 0.05; return SO_OK;
}
static int b_larvae(SD_ARGS) {
    SD_BEGIN
    SoSolids *ps = NS(); GV *foot_ = GVn(); int64_t n = RI(rng, 4, 8);
    for (int64_t i = 0; i < n; i++) {
        double r0 = u(0, 2.6), t0 = u(0, 6.28); P2 p = POL(cx, cy, r0, t0); double b = u(0, 6.28), L = u(1.4, 2.0), ph = u(0, 3); C4 cl = { L, 0, 0, ph };
        SPIN(ps, p.x, p.y, b, L, rf_larva, &cl, 1.0, 0.0, "round"); gv_add(foot_, DISC(p.x, p.y, L / 2, 20));
    }
    GEOSGeometry *f = BUF(HL(foot_), 0.2, 16);
    ST(ps, "flesh", NULL);
    SD_RET(f);
}

/* infestation / web_anchor */
static int b_web_anchor(SD_ARGS) {
    SD_BEGIN
    SoSolids *ps = NS(); GV *foot_ = GVn();
    DOME(ps, cx, cy, 0.7, 0.8, 0.0, 1.0, 0.0, 20); int64_t n = RI(rng, 5, 8);
    for (int64_t k = 0; k < n; k++) {
        double t = a + 6.2832 * (double)k / (double)n + u(-0.25, 0.25), er = u(2.6, 4.2); P2 e = POL(cx, cy, er, t); PV *rp = PVn(2);
        pv_p(rp, POL(cx, cy, 0.3, t)); pv_p(rp, e); ROD(ps, rp, 0, rf_strand, NULL, 0.9, 0.0, 0.0); gv_add(foot_, DISC(e.x, e.y, 0.3, 20));
        if (RND() < 0.5) { double t2 = a + 6.2832 * (double)(k + 1) / (double)n; PV *r2 = PVn(2); pv_p(r2, POL(cx, cy, 1.8, t)); pv_p(r2, POL(cx, cy, 1.8, t2)); ROD(ps, r2, 0.25, NULL, NULL, 0.9, 0.0, 0.0); }
    }
    GEOSGeometry *f = HL(foot_);
    ST(ps, "web", NULL);
    SD_RET(f);
}

/* infestation / mandibles */
static int rf_mandible(void *c, double q, double *o) { (void)c; *o = 0.6 - 0.36 * q; return SO_OK; }
static int b_mandibles(SD_ARGS) {
    SD_BEGIN
    SoFrame ff = so_frame(cx, cy, a); double L = u(3.4, 4.6); SoSolids *ps = NS(); int m = RND() < 0.75 ? 2 : 1;
    for (int s = 0; s < m; s++) {
        double sv = s == 0 ? -1.0 : 1.0; PV *rp = PVn(4);
        pv_p(rp, FF(&ff, -L / 2, sv * 0.5)); pv_p(rp, FF(&ff, -L * 0.1, sv * 1.5)); pv_p(rp, FF(&ff, L * 0.3, sv * 1.3)); pv_p(rp, FF(&ff, L / 2, sv * 0.3));
        ROD(ps, rp, 0, rf_mandible, NULL, 1.0, 0.0, 0.0);
        double xs[3] = { -L * 0.05, L * 0.15, L * 0.32 };
        for (int i = 0; i < 3; i++) { P2 c = FF(&ff, xs[i], sv * 0.75); CONE(ps, c.x, c.y, 0.32, 0.6, 0.22, 0.0, 6, 1.0, 0.0); }
    }
    GEOSGeometry *f = RECT(cx, cy, L + 0.4, 3.8, a);
    ST(ps, "carapace", NULL);
    SD_RET(f);
}

/* infestation / egg_ledge */
static int b_egg_ledge(SD_ARGS) {
    SD_BEGIN
    double R = u(2.6, 3.6), span = u(1.2, 2.0); PV *pts = ARC(cx, cy, R, a, a + span, 12); int64_t n = RI(rng, 3, 5); SoSolids *ps = NS();
    ROD(ps, pts, 0.75, NULL, NULL, 0.7, 0.0, 0.0);
    for (int64_t k = 0; k < n; k++) {
        P2 p = POL(cx, cy, R, a + span * ((double)k + 0.5) / (double)n); double r = u(0.5, 0.65), h = u(0.9, 1.3), aa = u(0, 3);
        DOME(ps, p.x, p.y, r, h, 0.4, 0.85, aa, 20);
    }
    GEOSGeometry *f = LBUF(pts, 1.0, 16);
    ST(ps, "egg", NULL);
    SD_RET(f);
}

/* infestation / hive_pores */
static int b_hive_pores(SD_ARGS) {
    SD_BEGIN
    double r = u(0.6, 0.8), s = r * 2 * 0.866 + 0.38; GV *gs = GVn();
    for (int i = -2; i < 3; i++)
        for (int j = -2; j < 3; j++) {
            double x = cx + (i + 0.5 * (double)IMOD(j, 2)) * s, y = cy + j * s * 0.866;
            if (DIST(p2(x, y), p2(cx, cy)) < u(2.0, 3.2)) gv_add(gs, NGON(x, y, r, 6, 0.5236));
        }
    if (!gs->n) SD_NONE;
    GEOSGeometry *f = BUF(HL(gs), 0.3, 16);
    LGCUT(UL(gs), u(0.35, 0.6), "pit");
    SD_RET(f);
}

void bld_register_sd_b(void) {
    sd_reg(b_fallen_cross, "highland", "made", "slate", "fallen_cross", 0);
    sd_reg(b_peat_stack, "highland", "made", "earth", "peat_stack", 0);
    sd_reg(b_targe, "highland", "made", "wood", "targe", 0);
    sd_reg(b_bagpipes, "highland", "made", "cloth", "bagpipes", 0);
    sd_reg(b_ram_skull, "highland", "made", "bone", "ram_skull", 0);
    sd_reg(b_drystone_run, "highland", "debris", "slate", "drystone_run", 0);
    sd_reg(b_thistles, "highland", "growth", "heather", "thistles", 0);
    sd_reg(b_heather_hummock, "highland", "growth", "heather", "heather_hummock", 0);
    sd_reg(b_quern, "highland", "made", "slate", "quern", 0);
    sd_reg(b_pot_still, "highland", "made", "brass", "pot_still", 0);
    sd_reg(b_peat_cutting, "highland", "marks", "pit", "peat_cutting", 1);
    sd_reg(b_cup_and_ring, "highland", "marks", "glyph", "cup_and_ring", 0);
    sd_reg(b_creel, "highland", "made", "twig", "creel", 0);
    sd_reg(b_caber_and_stone, "highland", "made", "wood", "caber_and_stone", 0);
    sd_reg(b_tussocks, "highland", "growth", "grass", "tussocks", 0);
    sd_reg(b_burial_kist, "highland", "made", "slate", "burial_kist", 1);
    sd_reg(b_hurdle, "highland", "made", "wood", "hurdle", 0);
    sd_reg(b_split_boulder, "highland", "debris", "slate", "split_boulder", 0);
    sd_reg(b_crook_and_plaid, "highland", "made", "wood", "crook_and_plaid", 0);
    sd_reg(b_beacon_pyre, "highland", "made", "wood", "beacon_pyre", 0);
    sd_reg(b_buried_head, "maahl", "made", "ruin", "buried_head", 0);
    sd_reg(b_obelisk_tip, "maahl", "made", "obelisk", "obelisk_tip", 0);
    sd_reg(b_sundial, "maahl", "made", "ruin", "sundial", 0);
    sd_reg(b_amphorae, "maahl", "made", "clay", "amphorae", 0);
    sd_reg(b_wind_ripples, "maahl", "marks", "pan", "wind_ripples", 0);
    sd_reg(b_scarab, "maahl", "made", "chitin", "scarab", 0);
    sd_reg(b_arch_fragment, "maahl", "made", "ruin", "arch_fragment", 0);
    sd_reg(b_geode, "maahl", "debris", "crystal", "geode", 0);
    sd_reg(b_saddlebags, "maahl", "made", "cloth", "saddlebags", 0);
    sd_reg(b_oil_lamp, "maahl", "made", "brass", "oil_lamp", 0);
    sd_reg(b_mosaic, "maahl", "made", "pavers", "mosaic", 0);
    sd_reg(b_beast_tracks, "maahl", "marks", "pit", "beast_tracks", 0);
    sd_reg(b_canopic_jars, "maahl", "made", "clay", "canopic_jars", 0);
    sd_reg(b_sphinx_paw, "maahl", "made", "ruin", "sphinx_paw", 0);
    sd_reg(b_salt_plates, "maahl", "debris", "salt", "salt_plates", 0);
    sd_reg(b_stele, "maahl", "made", "ruin", "stele", 0);
    sd_reg(b_well_bucket, "maahl", "made", "wood", "well_bucket", 0);
    sd_reg(b_desert_rose, "maahl", "debris", "sandstone", "desert_rose", 0);
    sd_reg(b_fallen_capital, "maahl", "made", "column", "fallen_capital", 0);
    sd_reg(b_fulgurite, "maahl", "debris", "glass", "fulgurite", 0);
    sd_reg(b_gas_cylinder, "corridor", "made", "metal", "gas_cylinder", 0);
    sd_reg(b_toolbox, "corridor", "made", "metal", "toolbox", 0);
    sd_reg(b_helmet, "corridor", "made", "metal", "helmet", 0);
    sd_reg(b_terminal, "corridor", "made", "metal", "terminal", 0);
    sd_reg(b_deck_hatch, "corridor", "made", "plate", "deck_hatch", 0);
    sd_reg(b_cell_rack, "corridor", "made", "metal", "cell_rack", 0);
    sd_reg(b_hose_reel, "corridor", "made", "cable", "hose_reel", 0);
    sd_reg(b_junction_box, "corridor", "made", "metal", "junction_box", 0);
    sd_reg(b_magboots, "corridor", "made", "metal", "magboots", 0);
    sd_reg(b_vent_fan, "corridor", "made", "metal", "vent_fan", 0);
    sd_reg(b_hover_pallet, "corridor", "made", "metal", "hover_pallet", 0);
    sd_reg(b_dead_drone, "corridor", "made", "metal", "dead_drone", 0);
    sd_reg(b_welding_set, "corridor", "made", "metal", "welding_set", 0);
    sd_reg(b_valve_stub, "corridor", "made", "pipe", "valve_stub", 0);
    sd_reg(b_deck_stencil, "corridor", "marks", "slot", "deck_stencil", 0);
    sd_reg(b_drip_tray, "corridor", "marks", "coolant", "drip_tray", 0);
    sd_reg(b_cryo_canister, "corridor", "made", "glass", "cryo_canister", 0);
    sd_reg(b_cable_spool, "corridor", "made", "cable", "cable_spool", 0);
    sd_reg(b_plate_stack, "corridor", "made", "plate", "plate_stack", 0);
    sd_reg(b_mess_tray, "corridor", "made", "tin", "mess_tray", 0);
    sd_reg(b_cocoon, "infestation", "growth", "web", "cocoon", 0);
    sd_reg(b_burst_egg, "infestation", "growth", "egg", "burst_egg", 0);
    sd_reg(b_dead_crawler, "infestation", "made", "carapace", "dead_crawler", 0);
    sd_reg(b_resin_stump, "infestation", "growth", "resin", "resin_stump", 0);
    sd_reg(b_spore_chimneys, "infestation", "growth", "spore", "spore_chimneys", 0);
    sd_reg(b_acid_holes, "infestation", "marks", "acid", "acid_holes", 0);
    sd_reg(b_hive_vent, "infestation", "growth", "flesh", "hive_vent", 1);
    sd_reg(b_molt, "infestation", "made", "carapace", "molt", 0);
    sd_reg(b_tendril_knot, "infestation", "growth", "tendril", "tendril_knot", 0);
    sd_reg(b_claw_marks, "infestation", "marks", "slot", "claw_marks", 0);
    sd_reg(b_sac_cluster, "infestation", "growth", "pustule", "sac_cluster", 0);
    sd_reg(b_flamethrower, "infestation", "made", "metal", "flamethrower", 0);
    sd_reg(b_scanner_and_mags, "infestation", "made", "metal", "scanner_and_mags", 0);
    sd_reg(b_slime_trail, "infestation", "marks", "acid", "slime_trail", 0);
    sd_reg(b_spine_row, "infestation", "debris", "spine", "spine_row", 0);
    sd_reg(b_larvae, "infestation", "growth", "flesh", "larvae", 0);
    sd_reg(b_web_anchor, "infestation", "growth", "web", "web_anchor", 0);
    sd_reg(b_mandibles, "infestation", "made", "carapace", "mandibles", 0);
    sd_reg(b_egg_ledge, "infestation", "growth", "egg", "egg_ledge", 0);
    sd_reg(b_hive_pores, "infestation", "marks", "pit", "hive_pores", 0);
}
