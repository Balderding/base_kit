/* bld_sd_c.c: sd_c.py in C (agent "bld-sdabc", 2026-11-19): the style-only details of urban, mine, swamp, cynder and
   targa, twenty each, in the file's own order. See bld_sd_abc.h for how a builder is written; each one names the
   Python builder it copies (style / name). Proof: work\cscene\bld_sdabc\prove.py. */
#include "bld_sd_abc.h"

typedef struct { double k; } C1;
typedef struct { double L, r; } C2;
typedef struct { double L, r, w; } C3;

/* the layer made last goes to place i of the list (Python built it later but lists it earlier) */
static void move_last_to(int i) {
    SoLayer *l = SD.out.v[SD.out.n - 1];
    memmove(&SD.out.v[i + 1], &SD.out.v[i], sizeof(SoLayer *) * (size_t)(SD.out.n - 1 - i));
    SD.out.v[i] = l;
}
static GEOSGeometry *RRB(double x, double y, double L, double W, double a_, double d) { return BUF(BUF(RECT(x, y, L, W, a_), -d, 16), d, 16); }   /* rect(...).buffer(-d).buffer(d) */
static PV *P2V(int n, const P2 *p) { PV *v = PVn(n); for (int i = 0; i < n; i++) pv_p(v, p[i]); return v; }

/* ====================================================================================================== urban */
/* urban / hydrant */
static int rf_hydrant(void *c, double t, double *o) {
    double L = ((C2 *)c)->L, r = ((C2 *)c)->r, k, q;
    if (t < -L / 2 + 0.4) k = 1.25;
    else if (t < L / 2 - r) k = 1.0;
    else { if (RF_PW((t - L / 2 + r) / r, 2.0, &q) != SO_OK) return SO_ERR; k = sqrt(pmax(0.0, 1 - q)); }
    *o = r * k + 0.02; return SO_OK;
}
static int b_hydrant(SD_ARGS) {
    SD_BEGIN
    SoFrame ff = so_frame(cx, cy, a); double r = u(0.75, 0.9), h = u(2.0, 2.6); SoSolids *ps = NS(); P2 p;
    if (RND() < 0.4) {
        double L = h + 0.6; C2 cl = { L, r };
        SPIN(ps, cx, cy, a, L, rf_hydrant, &cl, 1.1, 0.0, "round"); p = FF(&ff, L * 0.1, 0); CYL(ps, p.x, p.y, 0.4, r * 1.1 + 0.45, 0.0, 8);
        p = FF(&ff, -L / 2 - 1.2, 0); double br = u(1.1, 1.6); GEOSGeometry *b = BLOB(p.x, p.y, br, rng, 3);
        GV *hl = GVn(); gv_add(hl, RECT(cx, cy, L, 2 * r + 0.6, a)); gv_add(hl, b); GEOSGeometry *f = HL(hl);
        ST(ps, "iron", NULL); LGCUT(b, 0.28, "water");
        SD_RET(f);
    }
    PV *pf = PVn(12); pv_add(pf, r * 1.3, 0.0); pv_add(pf, r * 1.3, 0.3); pv_add(pf, r, 0.32); pv_add(pf, r, h * 0.75); pv_crv(pf, r, 0.3, h * 0.75, h); TURN(ps, cx, cy, pf, 16, 1.0, 0.0, 0.0);
    p = FF(&ff, r, 0); LIE(ps, p.x, p.y, a, 1.0, 0.42, h * 0.35);
    p = FF(&ff, 0, r); LIE(ps, p.x, p.y, a + 1.57, 0.9, 0.36, h * 0.4);
    p = FF(&ff, 0, -r); LIE(ps, p.x, p.y, a + 1.57, 0.9, 0.36, h * 0.4);
    GEOSGeometry *f = DISC(cx, cy, r + 0.9, 20);
    ST(ps, "iron", NULL);
    SD_RET(f);
}

/* urban / street_lamp */
static int rf_lamp_post(void *c, double t, double *o) { double L = ((C1 *)c)->k; *o = 0.5 - 0.16 * (t + L / 2) / L + (t < -L / 2 + 0.7 ? 0.3 : 0.0); return SO_OK; }
static int b_street_lamp(SD_ARGS) {
    SD_BEGIN
    SoFrame ff = so_frame(cx, cy, a); double L = u(8.0, 11.0); P2 e = FF(&ff, L / 2, 0), p; C1 cl = { L }; SoSolids *ps = NS(); PV *pf = PVn(12);
    SPIN(ps, cx, cy, a, L, rf_lamp_post, &cl, 1.1, 0.0, "round");
    pv_add(pf, 0.95, 0.0); pv_add(pf, 0.95, 0.5); pv_crv(pf, 0.95, 0.3, 0.5, 1.2); TURN(ps, e.x, e.y, pf, 8, 1.0, 0.0, 0.0);
    ST(ps, "iron", NULL);
    int64_t n = RI(rng, 1, 3);
    for (int64_t i = 0; i < n; i++) { double x = L / 2 + u(1.2, 2.0), v = u(-1.2, 1.2); p = FF(&ff, x, v); double sr = u(0.5, 0.65); LUP(STONE(p.x, p.y, sr, rng, 0.6, 3), 0.3, "glass"); }
    p = FF(&ff, 0.8, 0);
    SD_RET(RECT(p.x, p.y, L + 3.2, 3.0, a));
}

/* urban / tyres */
static int b_tyres(SD_ARGS) {
    SD_BEGIN
    double R = u(1.3, 1.6), x = cx, y = cy; SoSolids *ps = NS(); int64_t n = RI(rng, 1, 3);
    for (int64_t i = 0; i < n; i++) { ROD(ps, ARC(x, y, R, 0, 6.2832, 20), 0.55, NULL, NULL, 1.1, (double)i * 0.95, 0.0); x += u(-0.35, 0.35); y += u(-0.35, 0.35); }
    GEOSGeometry *f = DISC(cx, cy, R + 1.0, 20);
    ST(ps, "tar", NULL);
    SD_RET(f);
}

/* urban / post_box */
static int b_post_box(SD_ARGS) {
    SD_BEGIN
    SoFrame ff = so_frame(cx, cy, a); double L = u(3.4, 4.2), W = u(1.8, 2.2); SoSolids *ps = NS(); CV *cuts = CVn(); P2 p;
    LOAF(ps, cx, cy, a, L, W, W * 0.55, 0.0); p = FF(&ff, -L / 2 - 0.25, 0); BLK(ps, p.x, p.y, 0.5, W + 0.3, a, W * 0.45, 0.0);
    p = FF(&ff, L * 0.22, 0); cv_add(cuts, RECT(p.x, p.y, 0.35, W * 0.6, a), W * 0.55 - 0.22);
    p = FF(&ff, -L * 0.1, 0); cv_add(cuts, RECT(p.x, p.y, L * 0.3, W * 0.5, a), W * 0.55 - 0.14);
    ST(ps, "iron", cuts);
    int64_t n = RI(rng, 0, 3);
    for (int64_t i = 0; i < n; i++) { double x = L / 2 + u(0.9, 1.8), v = u(-1.2, 1.2); p = FF(&ff, x, v); double b = u(0, 3); LUP(RECT(p.x, p.y, 1.3, 1.0, b), 0.25, "paper"); }
    p = FF(&ff, 0.8, 0);
    SD_RET(RECT(p.x, p.y, L + 3.6, W + 1.6, a));
}

/* urban / dustbin */
static int rf_dustbin(void *c, double t, double *o) { double L = ((C2 *)c)->L, r = ((C2 *)c)->r; *o = r * (0.88 + 0.12 * (t + L / 2) / L) + 0.07 * (SIN(t * 7) > 0.3 ? 1.0 : 0.0); return SO_OK; }
static int b_dustbin(SD_ARGS) {
    SD_BEGIN
    SoFrame ff = so_frame(cx, cy, a); double L = u(3.0, 3.6), r = u(1.0, 1.2), x = L / 2 + u(1.4, 2.2), v = u(-1.5, 1.5); P2 c = FF(&ff, x, v); C2 cl = { L, r }; SoSolids *ps = NS(), *lid = NS(); PV *pf = PVn(3);
    SPIN(ps, cx, cy, a, L, rf_dustbin, &cl, 1.12, 0.0, "round");
    pv_add(pf, r + 0.15, 0.0); pv_add(pf, r + 0.15, 0.25); pv_add(pf, 0.5, 0.5); TURN(lid, c.x, c.y, pf, 16, 1.0, 0.0, 0.0); BLK(lid, c.x, c.y, 0.9, 0.4, a, 0.8, 0.0);
    GV *hl = GVn(); gv_add(hl, RECT(cx, cy, L, 2 * r + 0.3, a)); gv_add(hl, DISC(c.x, c.y, r + 0.3, 20)); GEOSGeometry *f = HL(hl);
    ST(ps, "tin", NULL); ST(lid, "tin", NULL);
    SD_RET(f);
}

/* urban / brick_pallet */
static int b_brick_pallet(SD_ARGS) {
    SD_BEGIN
    SoFrame ff = so_frame(cx, cy, a); int64_t n = RI(rng, 3, 4), m = RI(rng, 2, 3); SoSolids *ps = NS();
    BLK(ps, cx, cy, (double)n * 1.25 + 0.5, (double)m * 0.75 + 0.5, a, 0.3, 0.0);
    int64_t nl = RI(rng, 1, 3);
    for (int64_t lv = 0; lv < nl; lv++)
        for (int64_t i = 0; i < n; i++)
            for (int64_t j = 0; j < m; j++) {
                if (RND() < 0.15 * (double)(lv + 1)) continue;
                P2 c = FF(&ff, ((double)i - (double)(n - 1) / 2) * 1.25, ((double)j - (double)(m - 1) / 2) * 0.75); BLK(ps, c.x, c.y, 1.1, 0.62, a, 0.5, 0.28 + (double)lv * 0.48);
            }
    GEOSGeometry *f = RECT(cx, cy, (double)n * 1.25 + 0.8, (double)m * 0.75 + 0.8, a);
    ST(ps, "brick", NULL);
    SD_RET(f);
}

/* urban / storm_drain (ground) */
static int b_storm_drain(SD_ARGS) {
    SD_BEGIN
    SoFrame ff = so_frame(cx, cy, a); double L = u(4.0, 5.2); GV *l = GVn(); P2 p;
    DV xs = ARANGE(-L / 2 + 0.7, L / 2 - 0.4, 0.8);
    for (int i = 0; i < xs.n; i++) { p = FF(&ff, xs.v[i], -1.0); gv_add(l, RECT(p.x, p.y, 0.4, 1.3, a)); }
    GEOSGeometry *slots = UL(l);
    p = FF(&ff, 0, -0.4); GEOSGeometry *f = RECT(p.x, p.y, L, 2.9, a);
    p = FF(&ff, 0, 0.45); GEOSGeometry *g = RECT(p.x, p.y, L, 1.1, a); LUP(g, u(0.5, 0.7), "concrete"); LGCUT(slots, 0.7, "slot");
    SD_RET(f);
}

/* urban / bottles */
static int rf_bottle(void *c, double t, double *o) { double L = ((C1 *)c)->k; *o = t < L * 0.1 ? 0.46 : 0.46 - 0.2 * pmin(1.0, (t - L * 0.1) / (L * 0.15)); return SO_OK; }
static int b_bottles(SD_ARGS) {
    SD_BEGIN
    GV *foot_ = GVn(); int64_t n = RI(rng, 2, 5);
    for (int64_t i = 0; i < n; i++) {
        double rr = u(0.3, 2.6), t = a + (double)i * 1.4 + u(-0.4, 0.4); P2 p = POL(cx, cy, rr, t); double b = u(0, 6.28), L = u(2.0, 2.6); int broke = RND() < 0.3;
        C1 cl = { L }; SoSolids *s = NS(); SPIN(s, p.x, p.y, b, L, rf_bottle, &cl, 1.15, 0.0, "round"); ST(s, "glass", NULL); gv_add(foot_, DISC(p.x, p.y, L / 2 + 0.2, 20));
        if (broke) { P2 q = POL(p.x, p.y, L / 2 + 0.7, b + SD_PI); LUP(STONE(q.x, q.y, 0.6, rng, 0.6, 3), 0.25, "glass"); }
    }
    SD_RET(BUF(HL(foot_), 0.5, 16));
}

/* urban / road_sign */
static int b_road_sign(SD_ARGS) {
    SD_BEGIN
    SoFrame ff = so_frame(cx, cy, a); double L = u(5.0, 7.0); int64_t k = RI(rng, 0, 2); P2 c = FF(&ff, L / 2, 0); double R = u(1.4, 1.8); GEOSGeometry *plate; SoSolids *ps = NS();
    if (k == 0) plate = NGON(c.x, c.y, R, 8, a + 0.39);
    else if (k == 1) { double t = a + u(0, 1); plate = NGON(c.x, c.y, R * 1.15, 3, t); }
    else { double t = a + u(-0.4, 0.4); plate = RECT(c.x, c.y, R * 2.2, R * 1.2, t); }
    double v = u(-0.8, 0.8); P2 bend = FF(&ff, -L * 0.1, v); PV *rp = PVn(3); pv_p(rp, FF(&ff, -L / 2, 0)); pv_p(rp, bend); pv_p(rp, FF(&ff, L / 2 - 0.5, 0));
    ROD(ps, rp, 0.3, NULL, NULL, 1.1, 0.0, 0.0); EXT(ps, plate, 0.4, 0.0); EXT(ps, DIFF(plate, INSET(plate, 0.4)), 0.55, 0.0);
    GV *hl = GVn(); gv_add(hl, plate); gv_add(hl, RECT(cx, cy, L, 1.8, a)); GEOSGeometry *f = HL(hl);
    ST(ps, "iron", NULL);
    SD_RET(f);
}

/* urban / crossarm */
static int b_crossarm(SD_ARGS) {
    SD_BEGIN
    SoFrame ff = so_frame(cx, cy, a); double L = u(5.0, 6.5); SoSolids *ps = NS(); GV *foot_ = GVn(); double xs[3] = { -L * 0.38, 0, L * 0.38 };
    BLK(ps, cx, cy, L, 0.75, a, 0.6, 0.0); gv_add(foot_, RECT(cx, cy, L, 1.2, a));
    for (int i = 0; i < 3; i++) {
        double x = xs[i]; P2 c = FF(&ff, x, 0); PV *pf = PVn(5);
        pv_add(pf, 0.42, 0.0); pv_add(pf, 0.42, 0.9); pv_add(pf, 0.3, 0.95); pv_add(pf, 0.42, 1.1); pv_add(pf, 0.3, 1.3); TURN(ps, c.x, c.y, pf, 10, 1.0, 0.0, 0.0);
        if (RND() < 0.6) {
            double sv = CHPM(rng), ex = x + u(-1.5, 1.5), ev = sv * u(2.2, 3.4); P2 e = FF(&ff, ex, ev); SoSolids *w = NS();
            ROD(w, WIG(FF(&ff, x, sv * 0.85), e, rng, 0.5, 4), 0.25, NULL, NULL, 1.0, 0.0, 0.0); ST(w, "wire", NULL); gv_add(foot_, DISC(e.x, e.y, 0.3, 20));
        }
    }
    GEOSGeometry *f = HL(foot_);
    ST(ps, "wood", NULL); move_last_to(0);          /* [ST(ps, "wood")] + lay */
    SD_RET(f);
}

/* urban / hubcap_and_bumper */
static int b_hubcap_and_bumper(SD_ARGS) {
    SD_BEGIN
    SoFrame ff = so_frame(cx, cy, a); double R = u(1.1, 1.4), cu = u(-1.5, 1.5); P2 c = FF(&ff, cu, 2.4); double L = u(5.0, 6.5); CV *cuts = CVn(); SoSolids *bump = NS(), *hub = NS();
    DV ts = ARANGE(0, 6.2, 6.2832 / 8);
    for (int i = 0; i < ts.n; i++) cv_add(cuts, BARL(POL(c.x, c.y, R * 0.45, ts.v[i]), POL(c.x, c.y, R * 0.85, ts.v[i]), 0.3), 0.25);
    PV *rp = PVn(4); pv_p(rp, FF(&ff, -L / 2, -0.7)); pv_p(rp, FF(&ff, -L * 0.3, 0)); pv_p(rp, FF(&ff, L * 0.3, 0)); { double v = -0.7 * u(0.2, 2.0); pv_p(rp, FF(&ff, L / 2, v)); }
    ROD(bump, rp, 0.42, NULL, NULL, 1.1, 0.0, 0.0);
    GV *hl = GVn(); gv_add(hl, RECT(cx, cy, L, 2.0, a)); gv_add(hl, DISC(c.x, c.y, R, 20)); GEOSGeometry *f = HL(hl);
    ST(bump, "tin", NULL);
    PV *pf = PVn(3); pv_add(pf, R, 0.0); pv_add(pf, R, 0.2); pv_add(pf, R * 0.4, 0.5); TURN(hub, c.x, c.y, pf, 20, 1.0, 0.0, 0.0); ST(hub, "tin", cuts);
    SD_RET(f);
}

/* urban / kerbstones */
static int b_kerbstones(SD_ARGS) {
    SD_BEGIN
    SoFrame ff = so_frame(cx, cy, a); int64_t n = RI(rng, 3, 5); SoSolids *ps = NS();
    for (int64_t i = 0; i < n; i++) { double v = u(-0.5, 0.5); P2 c = FF(&ff, ((double)i - (double)(n - 1) / 2) * 2.1, v); double t = a + u(-0.3, 0.3), h = u(0.6, 0.8); LOAF(ps, c.x, c.y, t, 1.9, 1.0, h, 0.0); }
    GEOSGeometry *f = RECT(cx, cy, (double)n * 2.1 + 0.4, 2.6, a);
    ST(ps, "concrete", NULL);
    SD_RET(f);
}

/* urban / window_frame */
static int b_window_frame(SD_ARGS) {
    SD_BEGIN
    SoFrame ff = so_frame(cx, cy, a); double L = u(3.6, 4.6), W = u(2.8, 3.4); GEOSGeometry *R = RECT(cx, cy, L, W, a); GV *l = GVn(); SoSolids *ps = NS();
    gv_add(l, DIFF(R, INSET(R, 0.5))); gv_add(l, RECT(cx, cy, 0.45, W, a)); gv_add(l, RECT(cx, cy, L, 0.45, a));
    EXT(ps, UL(l), 0.45, 0.0); ST(ps, "wood", NULL);
    for (int sv = -1; sv <= 1; sv += 2) {
        int64_t m = RI(rng, 0, 2);
        for (int64_t i = 0; i < m; i++) { double x = u(-L, L) * 0.7, v = sv * (W / 2 + u(0.8, 1.4)); P2 p = FF(&ff, x, v); double sr = u(0.55, 0.75); LUP(STONE(p.x, p.y, sr, rng, 0.55, 3), 0.25, "glass"); }
    }
    SD_RET(RECT(cx, cy, L + 1.8, W + 3.6, a));
}

/* urban / park_bench */
static int b_park_bench(SD_ARGS) {
    SD_BEGIN
    SoFrame ff = so_frame(cx, cy, a); double L = u(5.0, 6.2); int n = 4; int64_t miss = RI(rng, 0, n + 1); GV *sl = GVn(), *en = GVn();
    for (int i = 0; i < n; i++) { P2 p1 = FF(&ff, -L / 2, (i - 1.5) * 1.05); double k = i == miss ? u(0.2, 0.6) : 1.0; P2 p2_ = FF(&ff, L / 2 * k, (i - 1.5) * 1.05); gv_add(sl, BARL(p1, p2_, 0.92)); }
    GEOSGeometry *slats = UL(sl);
    for (int sv = -1; sv <= 1; sv += 2) { P2 p = FF(&ff, sv * (L / 2 - 0.6), 0); gv_add(en, RECT(p.x, p.y, 0.95, n * 1.05 + 0.9, a)); }
    GEOSGeometry *ends = UL(en), *f = RECT(cx, cy, L, n * 1.05 + 1.0, a);
    LUP(ends, 0.75, "iron"); LUP(DIFF(slats, BUF(ends, 0.02, 16)), 0.45, "wood");
    SD_RET(f);
}

/* urban / rebar_chunk */
static int b_rebar_chunk(SD_ARGS) {
    SD_BEGIN
    double r = u(1.3, 1.8); SoSolids *ps = NS(); GV *foot_ = GVn();
    double h = u(0.9, 1.3); ROCK(ps, cx, cy, r, h, rng, 0.85, 0.0, 6); gv_add(foot_, DISC(cx, cy, r, 20));
    int64_t n = RI(rng, 2, 4);
    for (int64_t i = 0; i < n; i++) {
        double t = a + u(-0.9, 0.9); P2 e = POL(cx, cy, r + u(1.4, 2.6), t); double mt = t + u(-0.5, 0.5); P2 m = POL(cx, cy, r + 0.6, mt);
        PV *rp = PVn(3); pv_p(rp, POL(cx, cy, r * 0.4, t)); pv_p(rp, m); pv_p(rp, e); ROD(ps, rp, 0.24, NULL, NULL, 1.3, 0.0, 0.0); gv_add(foot_, DISC(e.x, e.y, 0.3, 20));
    }
    GEOSGeometry *f = HL(foot_);
    ST(ps, "concrete", NULL);
    SD_RET(f);
}

/* urban / suitcase */
static int b_suitcase(SD_ARGS) {
    SD_BEGIN
    SoFrame ff = so_frame(cx, cy, a); double L = u(2.8, 3.4), W = u(1.9, 2.3), h = u(0.75, 0.95), x = u(-1, 1); P2 c = FF(&ff, x, W / 2 + 1.3), p; double b = u(0, 6.28); SoSolids *ps = NS(), *shoe = NS();
    EXT(ps, RRB(cx, cy, L, W, a, 0.3), h, 0.0); p = FF(&ff, 0, -W / 2 - 0.2); EXT(ps, ANN(p.x, p.y, 0.55, 0.26, 10), 0.4, 0.0);
    for (int sv = -1; sv <= 1; sv += 2) { p = FF(&ff, sv * L * 0.3, 0); BLK(ps, p.x, p.y, 0.4, W + 0.1, a, h + 0.12, 0.0); }
    LOAF(shoe, c.x, c.y, b, 1.7, 0.7, 0.5, 0.0); p = POL(c.x, c.y, -0.45, b); CYL(shoe, p.x, p.y, 0.4, 0.75, 0.0, 8);
    GV *hl = GVn(); gv_add(hl, RECT(cx, cy, L, W + 0.8, a)); gv_add(hl, DISC(c.x, c.y, 1.0, 20)); GEOSGeometry *f = HL(hl);
    ST(ps, "leather", NULL); ST(shoe, "leather", NULL);
    SD_RET(f);
}

/* urban / radiator */
static int b_radiator(SD_ARGS) {
    SD_BEGIN
    SoFrame ff = so_frame(cx, cy, a); int64_t n = RI(rng, 5, 9); double W = u(2.4, 3.0); SoSolids *ps = NS(); P2 p;
    for (int64_t i = 0; i < n; i++) { P2 c = FF(&ff, ((double)i - (double)(n - 1) / 2) * 0.62, 0); LOAF(ps, c.x, c.y, a + 1.5708, W, 0.5, 0.7, 0.0); }
    p = FF(&ff, 0, W / 2 - 0.35); LIE(ps, p.x, p.y, a, (double)n * 0.62 + 0.9, 0.3, 0.0); p = FF(&ff, 0, -W / 2 + 0.35); LIE(ps, p.x, p.y, a, (double)n * 0.62 + 0.9, 0.3, 0.0);
    GEOSGeometry *f = RECT(cx, cy, (double)n * 0.62 + 1.2, W + 0.2, a);
    ST(ps, "iron", NULL);
    SD_RET(f);
}

/* urban / bathtub */
static int b_bathtub(SD_ARGS) {
    SD_BEGIN
    SoFrame ff = so_frame(cx, cy, a); double L = u(4.6, 5.6), W = u(2.2, 2.6), h = u(1.2, 1.5); GV *l = GVn(); SoSolids *ps = NS(); CV *cuts = CVn(); P2 p;
    gv_add(l, RECT(cx, cy, L - W, W, a)); p = FF(&ff, -(L - W) / 2, 0); gv_add(l, DISC(p.x, p.y, W / 2, 20)); p = FF(&ff, (L - W) / 2, 0); gv_add(l, DISC(p.x, p.y, W / 2, 20));
    GEOSGeometry *o = UL(l);
    EXT(ps, BUF(o, -0.25, 16), h - 0.15, 0.0); EXT(ps, DIFF(o, BUF(o, -0.3, 16)), h, 0.0);
    for (int sx = -1; sx <= 1; sx += 2) for (int sy = -1; sy <= 1; sy += 2) { p = FF(&ff, sx * (L / 2 - 0.9), sy * (W / 2 - 0.1)); DOME(ps, p.x, p.y, 0.42, 0.5, 0.0, 1.0, 0.0, 20); }
    GEOSGeometry *f = BUF(o, 0.4, 16);
    GEOSGeometry *q = BUF(o, -0.5, 16); cv_add(cuts, q, u(0.35, 0.6)); ST(ps, "iron", cuts);
    SD_RET(f);
}

/* urban / chalk_outline */
static int b_chalk_outline(SD_ARGS) {
    SD_BEGIN
    SoFrame ff = so_frame(cx, cy, a); double k = u(0.85, 1.1); GV *l = GVn();
    gv_add(l, CAPL(FF(&ff, -1.2 * k, 0), FF(&ff, 1.2 * k, 0), 1.5 * k));
    { double v = u(-0.3, 0.3); P2 p = FF(&ff, 2.2 * k, v); gv_add(l, DISC(p.x, p.y, 0.62 * k, 12)); }
    for (int sv = -1; sv <= 1; sv += 2) { P2 p1 = FF(&ff, 0.9 * k, sv * 0.5); double x = u(0.4, 2.2) * k, v = sv * u(1.6, 2.2) * k; gv_add(l, CAPL(p1, FF(&ff, x, v), 0.55 * k)); }
    for (int sv = -1; sv <= 1; sv += 2) { P2 p1 = FF(&ff, -1.2 * k, sv * 0.4); double x = -u(2.6, 3.4) * k, v = sv * u(0.4, 1.4) * k; gv_add(l, CAPL(p1, FF(&ff, x, v), 0.62 * k)); }
    GEOSGeometry *body = UL(l), *g = DIFF(BUF(body, 0.18, 16), BUF(body, -0.14, 16)), *f = CHULL(body);
    LGCUT(g, 0.22, "seam");
    SD_RET(f);
}

/* urban / setts */
static int b_setts(SD_ARGS) {
    SD_BEGIN
    SoFrame ff = so_frame(cx, cy, a); int64_t n = RI(rng, 3, 5), m = RI(rng, 2, 4); SoSolids *ps = NS();
    for (int64_t i = 0; i < n; i++)
        for (int64_t j = 0; j < m; j++) {
            if (RND() < 0.2) continue;
            P2 c = FF(&ff, ((double)i - (double)(n - 1) / 2) * 1.2 + 0.6 * (double)IMOD(j, 2), ((double)j - (double)(m - 1) / 2) * 1.0); double h = u(0.35, 0.55);
            LOAF(ps, c.x, c.y, a, 1.05, 0.85, h, 0.0);
        }
    if (!ps->n) SD_NONE;
    P2 p = FF(&ff, 0.3, 0); GEOSGeometry *f = RECT(p.x, p.y, (double)n * 1.2 + 1.0, (double)m * 1.0 + 0.4, a);
    ST(ps, "concrete", NULL);
    SD_RET(f);
}

/* ====================================================================================================== mine */
/* mine / dynamite */
static int b_dynamite(SD_ARGS) {
    SD_BEGIN
    SoFrame ff = so_frame(cx, cy, a); int64_t n = RI(rng, 3, 5); double L = u(2.6, 3.2); SoSolids *ps = NS(), *rope = NS();
    for (int64_t i = 0; i < n; i++) { double x = u(-0.1, 0.1); P2 p = FF(&ff, x, ((double)i - (double)(n - 1) / 2) * 0.78); LIE(ps, p.x, p.y, a, L, 0.4, 0.0); }
    BLK(ps, cx, cy, 0.45, (double)n * 0.78 + 0.2, a, 0.6, 0.0); double x = L / 2 + u(1.5, 3.0), v = u(-2, 2); P2 e = FF(&ff, x, v);
    GV *hl = GVn(); gv_add(hl, RECT(cx, cy, L, (double)n * 0.8 + 0.3, a)); gv_add(hl, DISC(e.x, e.y, 0.4, 20)); GEOSGeometry *f = HL(hl);
    ST(ps, "paper", NULL); ROD(rope, WIG(FF(&ff, L / 2 + 0.3, 0), e, rng, 0.6, 5), 0.24, NULL, NULL, 1.0, 0.0, 0.0); ST(rope, "rope", NULL);
    SD_RET(f);
}

/* mine / plunger */
static int b_plunger(SD_ARGS) {
    SD_BEGIN
    SoFrame ff = so_frame(cx, cy, a); double L = u(2.2, 2.7), W = u(1.6, 2.0), h = u(1.3, 1.7), up_ = u(0.5, 1.3); SoSolids *ps = NS(), *w = NS(); P2 p;
    BLK(ps, cx, cy, L, W, a, h, 0.0); CYL(ps, cx, cy, 0.34, h + up_, 0.0, 8); HANG(ps, BARL(FF(&ff, 0, -W * 0.6), FF(&ff, 0, W * 0.6), 0.6), h + up_ - 0.5, h + up_);
    for (int sv = -1; sv <= 1; sv += 2) { p = FF(&ff, sv * L * 0.3, W * 0.25); CYL(ps, p.x, p.y, 0.28, h + 0.3, 0.0, 6); }
    double x = L / 2 + u(2.0, 3.5), v = u(-2, 2); P2 e = FF(&ff, x, v);
    GV *hl = GVn(); gv_add(hl, RECT(cx, cy, L + 0.3, W + 0.6, a)); gv_add(hl, DISC(e.x, e.y, 0.4, 20)); GEOSGeometry *f = HL(hl);
    ST(ps, "wood", NULL); ROD(w, WIG(FF(&ff, L / 2 + 0.3, W * 0.25), e, rng, 0.6, 4), 0.24, NULL, NULL, 1.0, 0.0, 0.0); ST(w, "wire", NULL);
    SD_RET(f);
}

/* mine / miner_helmet */
static int b_miner_helmet(SD_ARGS) {
    SD_BEGIN
    SoFrame ff = so_frame(cx, cy, a); double r = u(1.05, 1.3); SoSolids *ps = NS(); PV *pf = PVn(12); P2 p;
    pv_add(pf, r + 0.5, 0.0); pv_add(pf, r + 0.5, 0.22); pv_add(pf, r, 0.25); pv_crv(pf, r, 0.3, 0.25, r * 0.95); TURN(ps, cx, cy, pf, 20, 0.88, a, 0.0);
    p = FF(&ff, r + 0.1, 0); LIE(ps, p.x, p.y, a, 0.7, 0.45, 0.2); BLK(ps, cx, cy, 2 * r, 0.4, a, r * 0.95 + 0.1, 0.0);
    GEOSGeometry *f = DISC(cx, cy, r + 1.0, 20);
    ST(ps, "tin", NULL);
    SD_RET(f);
}

/* mine / canary_cage */
static int b_canary_cage(SD_ARGS) {
    SD_BEGIN
    double r = u(1.0, 1.25), h = u(1.7, 2.3); int64_t n = RI(rng, 7, 10); SoSolids *ps = NS(); PV *pf = PVn(12); CV *cuts = CVn();
    pv_add(pf, r + 0.15, 0.0); pv_add(pf, r + 0.15, 0.3); pv_add(pf, r, 0.32); pv_add(pf, r, h * 0.7); pv_crv(pf, r, 0.3, h * 0.7, h); TURN(ps, cx, cy, pf, 20, 1.0, 0.0, 0.0);
    EXT(ps, ANN(cx, cy, 0.5, 0.24, 10), h + 0.25, 0.0);
    for (int64_t k = 0; k < n; k++) {
        double t0 = a + 6.2832 * ((double)k + 0.2) / (double)n, t1 = a + 6.2832 * ((double)k + 0.8) / (double)n;
        P2 q[4] = { POL(cx, cy, 0.35, t0), POL(cx, cy, r - 0.2, t0), POL(cx, cy, r - 0.2, t1), POL(cx, cy, 0.35, t1) };
        cv_add(cuts, POLY(P2V(4, q)), h * 0.45);
    }
    GEOSGeometry *f = DISC(cx, cy, r + 0.3, 20);
    ST(ps, "brass", cuts);
    SD_RET(f);
}

/* mine / kibble */
static int b_kibble(SD_ARGS) {
    SD_BEGIN
    double r = u(1.2, 1.5), h = u(1.5, 2.0); int full = RND() < 0.5; SoSolids *ps = NS(), *hd = NS(); PV *pf = PVn(5); CV *cuts = CVn();
    pv_add(pf, r * 0.8, 0.0); pv_add(pf, r, h * 0.5); pv_add(pf, r * 0.9, h); pv_add(pf, r * 0.9 + 0.12, h + 0.02); pv_add(pf, r * 0.9 + 0.12, h + 0.15); TURN(ps, cx, cy, pf, 16, 1.0, 0.0, 0.0);
    GEOSGeometry *q = DISC(cx, cy, r * 0.9 - 0.3, 14); double fl = h - (full ? 0.3 : u(0.6, 1.0)); cv_add(cuts, q, fl); ST(ps, "iron", cuts);
    P2 c = POL(cx, cy, r + 0.8, a); ROD(hd, ARC(c.x, c.y, 1.0, a + 1.2, a + 5.0, 10), 0.26, NULL, NULL, 1.0, 0.0, 0.0); ST(hd, "iron", NULL);
    int64_t n = RI(rng, 1, 4);
    for (int64_t i = 0; i < n; i++) { double rr = r + u(0.7, 1.5), t = a + 3.14 + u(-1, 1); P2 p = POL(cx, cy, rr, t); GEOSGeometry *st = STONE(p.x, p.y, u(0.45, 0.65), rng, 0.8, 5); LMOUND(st, u(0.35, 0.5), "quartz", SD_PEB_T); }
    SD_RET(DISC(cx, cy, r + 2.0, 20));
}

/* mine / sluice_box */
static int b_sluice_box(SD_ARGS) {
    SD_BEGIN
    SoFrame ff = so_frame(cx, cy, a); double L = u(6.0, 8.0), W = u(1.9, 2.3), h = u(0.8, 1.0); DV xs = ARANGE(-L / 2 + 0.5, L / 2 - 0.9, 1.1); CV *cuts = CVn(); SoSolids *ps = NS(); P2 p;
    for (int i = 0; i < xs.n; i++) { p = FF(&ff, xs.v[i] + 0.42, 0); cv_add(cuts, RECT(p.x, p.y, 0.8, W - 0.7, a), 0.3); }
    BLK(ps, cx, cy, L, W, a, h, 0.0);
    for (int sv = -1; sv <= 1; sv += 2) { p = FF(&ff, sv * L * 0.3, 0); BLK(ps, p.x, p.y, 0.6, W + 0.9, a, 0.45, 0.0); }
    GEOSGeometry *f = RECT(cx, cy, L + 0.2, W + 1.0, a);
    ST(ps, "wood", cuts);
    SD_RET(f);
}

/* mine / assay_scale */
static int b_assay_scale(SD_ARGS) {
    SD_BEGIN
    SoFrame ff = so_frame(cx, cy, a); double L = u(3.6, 4.4), h = u(1.5, 2.0), tilt = u(-0.35, 0.35); SoSolids *ps = NS(); CV *cuts = CVn(); P2 p;
    BLK(ps, cx, cy, 1.8, 1.2, a, 0.4, 0.0); CYL(ps, cx, cy, 0.34, h, 0.0, 8); HANG(ps, BARL(FF(&ff, -L / 2, 0), FF(&ff, L / 2, 0), 0.5), h - 0.4, h);
    for (int sv = -1; sv <= 1; sv += 2) { P2 c = FF(&ff, sv * L / 2, 0); PV *pf = PVn(2); pv_add(pf, 0.55, 0.0); pv_add(pf, 0.85, 0.45 + sv * tilt); TURN(ps, c.x, c.y, pf, 14, 1.0, 0.0, 0.0); }
    for (int sv = -1; sv <= 1; sv += 2) { p = FF(&ff, sv * L / 2, 0); cv_add(cuts, DISC(p.x, p.y, 0.55, 10), 0.22); }
    GEOSGeometry *f = RECT(cx, cy, L + 2.0, 2.0, a);
    ST(ps, "brass", cuts);
    SD_RET(f);
}

/* mine / lunch_pail */
static int b_lunch_pail(SD_ARGS) {
    SD_BEGIN
    SoFrame ff = so_frame(cx, cy, a); double L = u(2.2, 2.7), W = u(1.4, 1.7), h = u(0.9, 1.2), x = L / 2 + u(1.0, 1.6), v = u(-1, 1); P2 c = FF(&ff, x, v), p; SoSolids *ps = NS(), *mug = NS(); PV *rp = PVn(2), *pf = PVn(2); CV *cuts = CVn();
    EXT(ps, RRB(cx, cy, L, W, a, 0.35), h, 0.0); LOAF(ps, cx, cy, a, L, W, 0.45, h);
    pv_p(rp, FF(&ff, -L * 0.3, 0)); pv_p(rp, FF(&ff, L * 0.3, 0)); ROD(ps, rp, 0.27, NULL, NULL, 1.0, h + 0.4, 0.0);
    pv_add(pf, 0.52, 0.0); pv_add(pf, 0.58, 0.85); TURN(mug, c.x, c.y, pf, 12, 1.0, 0.0, 0.0); p = POL(c.x, c.y, 0.75, a + 1); EXT(mug, ANN(p.x, p.y, 0.4, 0.18, 8), 0.6, 0.0);
    GV *hl = GVn(); gv_add(hl, RECT(cx, cy, L, W, a)); gv_add(hl, DISC(c.x, c.y, 1.2, 20)); GEOSGeometry *f = HL(hl);
    ST(ps, "tin", NULL); cv_add(cuts, DISC(c.x, c.y, 0.3, 8), 0.4); ST(mug, "tin", cuts);
    SD_RET(f);
}

/* mine / drill_steels */
static int b_drill_steels(SD_ARGS) {
    SD_BEGIN
    SoFrame ff = so_frame(cx, cy, a); SoSolids *ps = NS(), *ps2 = NS(); int64_t n = RI(rng, 2, 4); P2 p;
    for (int64_t i = 0; i < n; i++) {
        double x = u(-0.5, 0.5); p = FF(&ff, x, ((double)i - (double)(n - 1) / 2) * 0.95); double b = a + u(-0.12, 0.12); SoFrame f2 = so_frame(p.x, p.y, b);
        double L = u(5.0, 7.5); PV *rp = PVn(2); pv_p(rp, FF(&f2, -L / 2, 0)); pv_p(rp, FF(&f2, L / 2, 0)); ROD(ps, rp, 0.28, NULL, NULL, 1.1, 0.0, 0.0);
        P2 q = FF(&f2, L / 2, 0); EXT(ps, STAR(q.x, q.y, 0.5, 0.25, 4, a), 0.5, 0.0);
    }
    double x = u(-1, 1); p = FF(&ff, x, (double)n * 0.5 + 1.2); double b = a + u(-0.5, 0.5); SoFrame h = so_frame(p.x, p.y, b);
    PV *rp = PVn(2); pv_p(rp, FF(&h, -2.2, 0)); pv_p(rp, FF(&h, 1.6, 0)); ROD(ps2, rp, 0.3, NULL, NULL, 1.1, 0.0, 0.0);
    P2 q = FF(&h, 1.9, 0); double t = a + 1.57 + u(-0.5, 0.5); LIE(ps2, q.x, q.y, t, 1.8, 0.55, 0.0);
    p = FF(&ff, 0, 0.6); GEOSGeometry *f = RECT(p.x, p.y, 8.0, (double)n * 0.95 + 3.4, a);
    ST(ps, "iron", NULL); ST(ps2, "wood", NULL);
    SD_RET(f);
}

/* mine / oil_lantern */
static int rf_lantern(void *c, double t, double *o) {
    double L = ((C2 *)c)->L, r = ((C2 *)c)->r;
    *o = t < -L * 0.22 ? r : t < L * 0.23 ? r * (0.62 + 0.3 * SIN((t + L * 0.22) / (L * 0.45) * SD_PI)) : t < L * 0.4 ? r * 0.75 : 0.35; return SO_OK;
}
static int b_oil_lantern(SD_ARGS) {
    SD_BEGIN
    SoFrame ff = so_frame(cx, cy, a); double L = u(2.6, 3.2), r = u(0.7, 0.85); C2 cl = { L, r }; SoSolids *ps = NS(); P2 p;
    SPIN(ps, cx, cy, a, L, rf_lantern, &cl, 1.12, 0.0, "round"); p = FF(&ff, L / 2 + 0.5, 0); EXT(ps, ANN(p.x, p.y, 0.75, 0.42, 12), 0.4, 0.0);
    ST(ps, "tin", NULL);
    if (RND() < 0.5) { double v = u(1.2, 1.8); v = v * CHPM(rng); p = FF(&ff, -L * 0.2, v); double br = u(0.7, 1.0); LGCUT(BLOB(p.x, p.y, br, rng, 3), 0.22, "scorch"); }
    p = FF(&ff, 0.3, 0);
    SD_RET(RECT(p.x, p.y, L + 1.8, 2 * r + 3.0, a));
}

/* mine / wheelset */
static int b_wheelset(SD_ARGS) {
    SD_BEGIN
    SoFrame ff = so_frame(cx, cy, a); double L = u(3.4, 4.2), R = u(1.0, 1.25); int one = RND() < 0.25; SoSolids *ps = NS(); P2 p;
    LIE(ps, cx, cy, a, L + 0.8, 0.32, 0.0); p = FF(&ff, -L / 2, 0); LIE(ps, p.x, p.y, a, 0.55, R, 0.0); p = FF(&ff, -L / 2 - 0.3, 0); LIE(ps, p.x, p.y, a, 0.3, R + 0.25, 0.0);
    if (!one) { p = FF(&ff, L / 2, 0); LIE(ps, p.x, p.y, a, 0.55, R, 0.0); p = FF(&ff, L / 2 + 0.3, 0); LIE(ps, p.x, p.y, a, 0.3, R + 0.25, 0.0); }
    GEOSGeometry *f = RECT(cx, cy, L + 1.4, 2 * R + 0.8, a);
    ST(ps, "iron", NULL);
    SD_RET(f);
}

/* mine / stalagmites */
static int b_stalagmites(SD_ARGS) {
    SD_BEGIN
    SoSolids *ps = NS(); GV *foot_ = GVn(); int64_t n = RI(rng, 3, 6);
    for (int64_t i = 0; i < n; i++) {
        double r0 = u(0, 2.0), t0 = u(0, 6.28); P2 p = POL(cx, cy, r0, t0); double r = u(0.5, 1.0), h = r * u(2.0, 3.4); PV *pf = PVn(4);
        pv_add(pf, r, 0.0); pv_add(pf, r * 0.65, h * 0.3); pv_add(pf, r * 0.5, h * 0.6); pv_add(pf, 0.24, h); double sq = u(0.8, 1.0), aa = u(0, 3); TURN(ps, p.x, p.y, pf, 10, sq, aa, 0.0);
        gv_add(foot_, DISC(p.x, p.y, r, 20));
    }
    GEOSGeometry *f = BUF(HL(foot_), 0.2, 16);
    ST(ps, "rock", NULL);
    SD_RET(f);
}

/* mine / sorting_table */
static int b_sorting_table(SD_ARGS) {
    SD_BEGIN
    SoFrame ff = so_frame(cx, cy, a); double L = u(4.0, 5.0), W = u(2.4, 2.9), h = u(0.9, 1.2); GEOSGeometry *R = RECT(cx, cy, L, W, a); SoSolids *ps = NS();
    EXT(ps, R, h, 0.0); EXT(ps, DIFF(R, INSET(R, 0.4)), h + 0.3, 0.0);
    int64_t n = RI(rng, 3, 7);
    for (int64_t i = 0; i < n; i++) { double x = u(-L / 2 + 0.9, L / 2 - 0.9), v = u(-W / 2 + 0.9, W / 2 - 0.9); P2 c = FF(&ff, x, v); double r = u(0.4, 0.6), hh = u(0.4, 0.6); ROCK(ps, c.x, c.y, r, hh, rng, 0.0, h, 9); }
    GEOSGeometry *f = RECT(cx, cy, L + 0.2, W + 0.2, a);
    ST(ps, "wood", NULL);
    SD_RET(f);
}

/* mine / pulley_block */
static int b_pulley_block(SD_ARGS) {
    SD_BEGIN
    SoFrame ff = so_frame(cx, cy, a); double R = u(1.0, 1.3); P2 hk = FF(&ff, -R - 0.9, 0); SoSolids *ps = NS(), *rope = NS(); GV *l = GVn();
    LIE(ps, cx, cy, a + 1.5708, 0.6, R, 0.0);
    for (int sv = -1; sv <= 1; sv += 2) gv_add(l, CAPL(FF(&ff, -R - 0.3, sv * 0.52), FF(&ff, R + 0.3, sv * 0.52), 0.42));
    EXT(ps, UL(l), R * 0.75, 0.0); ROD(ps, ARC(hk.x, hk.y, 0.6, a + 0.6, a + 5.2, 10), 0.26, NULL, NULL, 1.1, 0.0, 0.0);
    double x = R + u(2.0, 4.0), v = u(-2, 2); P2 e = FF(&ff, x, v), p = FF(&ff, -0.6, 0);
    GV *hl = GVn(); gv_add(hl, RECT(p.x, p.y, 2 * R + 2.6, 2 * R, a)); gv_add(hl, DISC(e.x, e.y, 0.4, 20)); GEOSGeometry *f = HL(hl);
    ST(ps, "iron", NULL); ROD(rope, WIG(FF(&ff, R + 0.6, 0), e, rng, 0.6, 4), 0.28, NULL, NULL, 1.0, 0.0, 0.0); ST(rope, "rope", NULL);
    SD_RET(f);
}

/* mine / claim_stake */
static int b_claim_stake(SD_ARGS) {
    SD_BEGIN
    SoFrame ff = so_frame(cx, cy, a); double h = u(2.2, 3.0); SoSolids *ps = NS();
    CYL(ps, cx, cy, 0.42, h, 0.0, 6); P2 p = FF(&ff, 0, 0); double b = a + u(-0.15, 0.15); HANG(ps, RECT(p.x, p.y, 2.6, 0.55, b), h - 1.2, h - 0.3);
    ST(ps, "wood", NULL);
    int64_t n = RI(rng, 4, 6); PV *rp = RINGPTS(cx, cy, 1.05, (int)n, a, 0.2, rng);
    for (int i = 0; i < rp->n; i++) { GEOSGeometry *st = STONE(rp->xy[2 * i], rp->xy[2 * i + 1], u(0.5, 0.75), rng, 0.85, 6); LMOUND(st, u(0.4, 0.6), "rock", SD_PEB_T); }
    SD_RET(DISC(cx, cy, 2.0, 20));
}

/* mine / hand_pump */
static int b_hand_pump(SD_ARGS) {
    SD_BEGIN
    SoFrame ff = so_frame(cx, cy, a); double r = u(0.6, 0.75), h = u(2.2, 2.8); SoSolids *ps = NS(); PV *pf = PVn(5), *rp = PVn(2); P2 p;
    pv_add(pf, r * 1.5, 0.0); pv_add(pf, r * 1.5, 0.3); pv_add(pf, r, 0.32); pv_add(pf, r, h); pv_add(pf, r * 0.7, h + 0.2); TURN(ps, cx, cy, pf, 12, 1.0, 0.0, 0.0);
    pv_p(rp, FF(&ff, r * 0.5, 0)); pv_p(rp, FF(&ff, r + 1.3, 0)); ROD(ps, rp, 0.34, NULL, NULL, 1.0, h * 0.55, 0.0);
    { P2 p1 = FF(&ff, 0, 0); double x = -r - u(1.4, 2.0); HANG(ps, BARL(p1, FF(&ff, x, 0), 0.5), h - 0.3, h + 0.15); }
    p = FF(&ff, r + 1.6, 0); double br = u(0.8, 1.1); GEOSGeometry *b = BLOB(p.x, p.y, br, rng, 3);
    GV *hl = GVn(); gv_add(hl, DISC(cx, cy, r + 2.2, 20)); gv_add(hl, b); GEOSGeometry *f = HL(hl);
    ST(ps, "iron", NULL); LGCUT(b, 0.25, "water");
    SD_RET(f);
}

/* mine / gold_seam */
static int b_gold_seam(SD_ARGS) {
    SD_BEGIN
    SoFrame ff = so_frame(cx, cy, a); double L = u(5.0, 8.0); PV *pts = WIG(FF(&ff, -L / 2, 0), FF(&ff, L / 2, 0), rng, 0.9, 6); GEOSGeometry *g = LBUF(pts, 0.26, 4);
    LGCUT(g, 0.4, "crack");
    for (int i = 1; i < pts->n - 1; i++) {
        if (!(RND() < 0.6)) continue;
        double x = pts->xy[2 * i] + u(-0.2, 0.2), sv = CHPM(rng), y = pts->xy[2 * i + 1] + 0.85 * sv; GEOSGeometry *st = STONE(x, y, u(0.42, 0.55), rng, 0.8, 5);
        LMOUND(st, u(0.3, 0.45), "gold", SD_PEB_T);
    }
    SD_RET(CHULL(BUF(g, 1.3, 16)));
}

/* mine / switch_lever */
static int b_switch_lever(SD_ARGS) {
    SD_BEGIN
    SoFrame ff = so_frame(cx, cy, a); double L = u(2.4, 3.0), x = u(1.8, 2.6); P2 e = FF(&ff, x, 0); double h = u(1.2, 1.8); SoSolids *ps = NS(); PV *rp = PVn(2);
    BLK(ps, cx, cy, L, 1.5, a, 0.4, 0.0); LIE(ps, cx, cy, a + 1.57, 1.1, 0.5, 0.3);
    pv_p(rp, FF(&ff, 0, 0)); pv_p(rp, e); ROD(ps, rp, 0.3, NULL, NULL, 1.0, h * 0.6, 0.0); DOME(ps, e.x, e.y, 0.55, 0.9, h * 0.55, 1.0, 0.0, 20);
    P2 p = FF(&ff, 0.6, 0); GEOSGeometry *f = RECT(p.x, p.y, L + 2.2, 1.9, a);
    ST(ps, "iron", NULL);
    SD_RET(f);
}

/* mine / spilled_ore */
static int rf_sack(void *c, double t, double *o) {          /* W / 2 * max(0.0, 1 - abs(2 * t / L) ** 3) ** 0.5 * (1.0 if t < L * 0.25 else 0.75) + 0.05 */
    double L = ((C2 *)c)->L, W = ((C2 *)c)->r, q, p; if (RF_PW(fabs(2 * t / L), 3.0, &q) != SO_OK || RF_PW(pmax(0.0, 1 - q), 0.5, &p) != SO_OK) return SO_ERR;
    *o = W / 2 * p * (t < L * 0.25 ? 1.0 : 0.75) + 0.05; return SO_OK;
}
static int b_spilled_ore(SD_ARGS) {
    SD_BEGIN
    SoFrame ff = so_frame(cx, cy, a); double L = u(3.0, 3.6), W = u(1.9, 2.3); C2 cl = { L, W }; SoSolids *ps = NS();
    SPIN(ps, cx, cy, a, L, rf_sack, &cl, 0.75, 0.0, "round"); ST(ps, "sack", NULL);
    int64_t n = RI(rng, 4, 8);
    for (int64_t i = 0; i < n; i++) { double x = L / 2 + u(0.5, 2.6), v = u(-1.4, 1.4); P2 c = FF(&ff, x, v); GEOSGeometry *st = STONE(c.x, c.y, u(0.42, 0.65), rng, 0.8, 5); LMOUND(st, u(0.3, 0.5), "quartz", SD_PEB_T); }
    P2 p = FF(&ff, 1.4, 0);
    SD_RET(RECT(p.x, p.y, L + 3.2, W + 1.4, a));
}

/* mine / fallen_stull */
static int b_fallen_stull(SD_ARGS) {
    SD_BEGIN
    SoFrame ff = so_frame(cx, cy, a); double L = u(3.0, 4.2), b = u(0.3, 0.8); b = b * CHPM(rng); double r = u(0.6, 0.75); P2 p = FF(&ff, 0.3, 0), c2 = POL(p.x, p.y, L / 2, a + b); SoSolids *ps = NS();
    p = FF(&ff, -L / 2, 0); LIE(ps, p.x, p.y, a, L, r, 0.0); { double LL = L * u(0.7, 1.0); LIE(ps, c2.x, c2.y, a + b, LL, r, 0.0); }
    for (int sv = -1; sv <= 1; sv += 2) {
        if (!(RND() < 0.7)) continue;
        double x1 = u(-0.3, 0.3); P2 p1 = FF(&ff, x1, sv * 0.3); double x2 = u(-0.5, 0.9), v2 = sv * u(1.5, 2.2); P2 p2_ = FF(&ff, x2, v2);
        EXT(ps, TRI(p1, p2_, 0.95), 0.35, 0.0);
    }
    p = FF(&ff, -L / 2, 0); GV *hl = GVn(); gv_add(hl, RECT(p.x, p.y, L, 2 * r, a)); gv_add(hl, RECT(c2.x, c2.y, L, 2 * r, a + b)); GEOSGeometry *f = BUF(HL(hl), 1.0, 16);
    ST(ps, "wood", NULL);
    SD_RET(f);
}

/* ====================================================================================================== swamp */
/* swamp / lily_pool (ground) */
static int b_lily_pool(SD_ARGS) {
    SD_BEGIN
    GEOSGeometry *p = BLOB(cx, cy, u(2.6, 3.4), rng, 5); GV *pads = GVn(); int64_t n = RI(rng, 2, 5);
    for (int64_t i = 0; i < n; i++) {
        double r0 = u(0, 1.8), t0 = u(0, 6.28); P2 c = POL(cx, cy, r0, t0); double r = u(0.75, 1.0), t = u(0, 6.28);
        P2 tr[3] = { c, POL(c.x, c.y, 2 * r, t), POL(c.x, c.y, 2 * r, t + 0.5) }; GEOSGeometry *q = DIFF(DISC(c.x, c.y, r, 14), POLY(P2V(3, tr)));
        if (CONTAINS(INSET(p, 0.3), q)) { int ok = 1; for (int j = 0; j < pads->n; j++) if (!(GDIST(q, pads->v[j]) > 0.3)) { ok = 0; break; } if (ok) gv_add(pads, q); }
    }
    LGCUT(p, 0.5, "water");
    if (pads->n) LPAD(UL(pads), "lily");
    SD_RET(p);
}

/* swamp / cypress_knees */
static int b_cypress_knees(SD_ARGS) {
    SD_BEGIN
    SoSolids *ps = NS(); GV *foot_ = GVn(); int64_t n = RI(rng, 4, 8);
    for (int64_t i = 0; i < n; i++) {
        double r0 = u(0, 2.4), t0 = u(0, 6.28); P2 p = POL(cx, cy, r0, t0); double r = u(0.45, 0.8), h = r * u(1.8, 3.2); PV *pf = PVn(12);
        pv_add(pf, r, 0.0); pv_add(pf, r * 0.7, h * 0.5); pv_crv(pf, r * 0.7, 0.24, h * 0.5, h); double sq = u(0.75, 1.0), aa = u(0, 3); TURN(ps, p.x, p.y, pf, 10, sq, aa, 0.0);
        gv_add(foot_, DISC(p.x, p.y, r, 20));
    }
    GEOSGeometry *f = BUF(HL(foot_), 0.2, 16);
    ST(ps, "root", NULL);
    SD_RET(f);
}

/* swamp / dugout */
static int dug_f(double L, double W, double t, double *o) {          /* f_(t) = W / 2 * max(0.0, 1 - abs(2 * t / L) ** 2.6) ** 0.6 + 0.04 */
    double q, p; if (RF_PW(fabs(2 * t / L), 2.6, &q) != SO_OK || RF_PW(pmax(0.0, 1 - q), 0.6, &p) != SO_OK) return SO_ERR;
    *o = W / 2 * p + 0.04; return SO_OK;
}
static int rf_dug(void *c, double t, double *o) { return dug_f(((C2 *)c)->L, ((C2 *)c)->r, t, o); }
static int rf_dug99(void *c, double t, double *o) { double v; if (dug_f(((C2 *)c)->L, ((C2 *)c)->r, t, &v) != SO_OK) return SO_ERR; *o = v * 0.99; return SO_OK; }
static int b_dugout(SD_ARGS) {
    SD_BEGIN
    SoFrame ff = so_frame(cx, cy, a); double L = u(7.0, 9.5), W = u(1.9, 2.3); C2 cl = { L, W }; PV *hp = PVn(24); SoSolids *ps = NS(), *pad = NS(); CV *cuts = CVn(); double fv;
    DV t1 = LINSPACE(-L * 0.38, L * 0.38, 12), t2 = LINSPACE(L * 0.38, -L * 0.38, 12);
    for (int i = 0; i < t1.n; i++) { K(dug_f(L, W, t1.v[i], &fv)); pv_p(hp, FF(&ff, t1.v[i], fv - 0.42)); }
    for (int i = 0; i < t2.n; i++) { K(dug_f(L, W, t2.v[i], &fv)); pv_p(hp, FF(&ff, t2.v[i], -fv + 0.42)); }
    GEOSGeometry *hollow = POLY(hp);
    double x = u(-1, 1); P2 q = FF(&ff, x, W / 2 + 0.9); double b = a + u(-0.2, 0.2); SoFrame pd = so_frame(q.x, q.y, b);
    PV *rp = PVn(2); pv_p(rp, FF(&pd, -2.4, 0)); pv_p(rp, FF(&pd, 1.0, 0)); ROD(pad, rp, 0.26, NULL, NULL, 1.0, 0.0, 0.0); P2 e = FF(&pd, 1.9, 0); EXT(pad, ELL(e.x, e.y, 1.0, 0.5, a, 12), 0.35, 0.0);
    P2 p = FF(&ff, 0, 0.5); GEOSGeometry *f = RECT(p.x, p.y, L, W + 2.2, a);
    SPIN(ps, cx, cy, a, L, rf_dug, &cl, 0.9, 0.0, "ridge"); SPIN(ps, cx, cy, a, L, rf_dug99, &cl, 0.55, 0.0, "round");
    cv_add(cuts, hollow, 0.3); ST(ps, "log", cuts); ST(pad, "log", NULL);
    SD_RET(f);
}

/* swamp / fish_trap */
static int rf_trap(void *c, double t, double *o) { double L = ((C2 *)c)->L, r = ((C2 *)c)->r; *o = r * (1 - 0.7 * (t + L / 2) / L) + 0.1 * (SIN(t * 6.5) > 0.2 ? 1.0 : 0.0) + 0.12; return SO_OK; }
static int b_fish_trap(SD_ARGS) {
    SD_BEGIN
    SoFrame ff = so_frame(cx, cy, a); double L = u(3.6, 4.6), r = u(0.9, 1.1); C2 cl = { L, r }; SoSolids *ps = NS(); CV *cuts = CVn();
    SPIN(ps, cx, cy, a, L, rf_trap, &cl, 1.1, 0.0, "round");
    GEOSGeometry *f = RECT(cx, cy, L, 2 * r + 0.4, a);
    P2 p = FF(&ff, -L / 2 + 0.3, 0); cv_add(cuts, DISC(p.x, p.y, r * 0.55, 8), r * 0.35); ST(ps, "twig", cuts);
    SD_RET(f);
}

/* swamp / totem_stake */
static int rf_totem(void *c, double t, double *o) { C3 *k = (C3 *)c; *o = k->r * (1 + 0.16 * SIN(t * 2.6 + k->w)) * (t < k->L / 2 - 1.0 ? 1.0 : pmax(0.3, (k->L / 2 - t))); return SO_OK; }
static int b_totem_stake(SD_ARGS) {
    SD_BEGIN
    SoFrame ff = so_frame(cx, cy, a); double L = u(6.0, 8.0), r = u(0.6, 0.75), ph = u(0, 6); C3 cl = { L, r, ph }; SoSolids *ps = NS();
    SPIN(ps, cx, cy, a, L, rf_totem, &cl, 1.1, 0.0, "round");
    int64_t n = RI(rng, 1, 3);
    for (int64_t i = 0; i < n; i++) { P2 c = FF(&ff, -L / 2 + 1.2 + (double)i * 1.7, 0); DOME(ps, c.x, c.y, r + 0.25, r * 1.1 + 0.5, 0.0, 0.85, a, 20); }
    GEOSGeometry *f = RECT(cx, cy, L, 2 * r + 0.8, a);
    ST(ps, "log", NULL);
    SD_RET(f);
}

/* swamp / shelf_stump */
static int b_shelf_stump(SD_ARGS) {
    SD_BEGIN
    double r = u(1.1, 1.5), h = u(1.8, 2.6); SoSolids *ps = NS(); PV *pf = PVn(3); CV *cuts = CVn();
    pv_add(pf, r * 1.25, 0.0); pv_add(pf, r, h * 0.3); pv_add(pf, r, h); TURN(ps, cx, cy, pf, 10, 1.0, 0.0, 0.0);
    int64_t n = RI(rng, 2, 4);
    for (int64_t i = 0; i < n; i++) {
        double t = a + (double)i * 1.9 + u(-0.4, 0.4); P2 c = POL(cx, cy, r * 0.9, t); double R = u(0.8, 1.2), z = h * u(0.3, 0.8); PV *p2_ = PVn(3);
        pv_add(p2_, R, 0.0); pv_add(p2_, R, z); pv_add(p2_, R * 0.5, z + 0.3); TURN(ps, c.x, c.y, p2_, 12, 1.0, 0.0, 0.0);
    }
    GEOSGeometry *f = DISC(cx, cy, r + 1.4, 20);
    cv_add(cuts, DISC(cx, cy, r * 0.5, 8), h - 0.3); ST(ps, "shroom", cuts);
    SD_RET(f);
}

/* swamp / frog_idol */
static int b_frog_idol(SD_ARGS) {
    SD_BEGIN
    SoFrame ff = so_frame(cx, cy, a); double r = u(1.3, 1.7), h = r * u(0.85, 1.05); SoSolids *ps = NS(); CV *cuts = CVn(); P2 p;
    DOME(ps, cx, cy, r, h, 0.0, 0.85, a, 20);
    for (int sv = -1; sv <= 1; sv += 2) { p = FF(&ff, r * 0.45, sv * r * 0.42); DOME(ps, p.x, p.y, r * 0.3, h * 0.55, h * 0.62, 1.0, 0.0, 20); }
    for (int sv = -1; sv <= 1; sv += 2) { p = FF(&ff, -r * 0.2, sv * r * 0.85); LOAF(ps, p.x, p.y, a + sv * 0.4, r * 1.3, r * 0.5, h * 0.45, 0.0); }
    GEOSGeometry *f = DISC(cx, cy, r * 1.4, 20);
    P2 lp[3] = { FF(&ff, r * 0.7, -r * 0.45), FF(&ff, r * 0.9, 0), FF(&ff, r * 0.7, r * 0.45) }; cv_add(cuts, LINE(P2V(3, lp), 0.26), h * 0.3);
    ST(ps, "tflag", cuts);
    SD_RET(f);
}

/* swamp / reed_bundle */
static int rf_reed(void *c, double t, double *o) { (void)c; (void)t; *o = 0.36; return SO_OK; }
static int b_reed_bundle(SD_ARGS) {
    SD_BEGIN
    SoFrame ff = so_frame(cx, cy, a); double L = u(5.0, 7.0); int64_t n = RI(rng, 5, 8); SoSolids *ps = NS(); GV *tips = GVn(); P2 p;
    for (int64_t i = 0; i < n; i++) {
        double v = (double)i - (double)(n - 1) / 2, x = L / 2 + u(-0.4, 0.4), w = v * u(0.45, 0.8); P2 e = FF(&ff, x, w); PV *rp = PVn(3);
        pv_p(rp, FF(&ff, -L / 2, v * 0.3)); pv_p(rp, FF(&ff, 0, v * 0.3)); pv_p(rp, e); ROD(ps, rp, 0.26, NULL, NULL, 1.1, 0.0, 0.0);
        SPIN(ps, e.x, e.y, a + v * 0.08, 1.3, rf_reed, NULL, 1.1, 0.0, "round"); gv_add(tips, DISC(e.x, e.y, 0.5, 20));
    }
    p = FF(&ff, -L * 0.1, 0); BLK(ps, p.x, p.y, 0.5, (double)n * 0.3 + 0.7, a, 0.5, 0.0);
    GV *hl = GVn(); for (int i = 0; i < tips->n; i++) gv_add(hl, tips->v[i]); gv_add(hl, RECT(cx, cy, L, (double)n * 0.4, a)); GEOSGeometry *f = HL(hl);
    ST(ps, "reed", NULL);
    SD_RET(f);
}

/* swamp / root_tangle */
static int rf_root(void *c, double q, double *o) { (void)c; *o = 0.55 - 0.3 * q + 0.12 * SIN(q * 9); return SO_OK; }
static int b_root_tangle(SD_ARGS) {
    SD_BEGIN
    SoSolids *ps = NS(), *all = NS(); GV *foot_ = GVn(); int64_t n = RI(rng, 4, 6);
    for (int64_t i = 0; i < n; i++) {
        double t = a + 6.2832 * (double)i / (double)n + u(-0.3, 0.3), L = u(3.0, 4.6), t2 = t + u(-0.4, 0.4); PV *pts = WIG(POL(cx, cy, 0.3, t), POL(cx, cy, L, t2), rng, 0.6, 5);
        ROD(ps, pts, 0, rf_root, NULL, 1.4, 0.0, 0.0); gv_add(foot_, DISC(pts->xy[2 * (pts->n - 1)], pts->xy[2 * (pts->n - 1) + 1], 0.3, 20));
    }
    GEOSGeometry *f = HL(foot_);
    CAT(all, ps); DOME(all, cx, cy, 0.9, 1.3, 0.0, 1.0, 0.0, 20); ST(all, "root", NULL);
    SD_RET(f);
}

/* swamp / shed_skin */
static int rf_skin(void *c, double q, double *o) { (void)c; double p; if (RF_PW(SIN(pmax(0.05, q) * SD_PI), 0.5, &p) != SO_OK) return SO_ERR; *o = 0.5 * p + 0.22; return SO_OK; }
static int b_shed_skin(SD_ARGS) {
    SD_BEGIN
    SoFrame ff = so_frame(cx, cy, a); double L = u(6.0, 9.0); DV xs = LINSPACE(-L / 2, L / 2, 18); PV *pts = PVn(18); SoSolids *ps = NS();
    for (int i = 0; i < xs.n; i++) pv_p(pts, FF(&ff, xs.v[i], 0.9 * SIN(xs.v[i] * 1.3 + 1)));
    GEOSGeometry *f = LBUF(pts, 0.8, 16);
    ROD(ps, pts, 0, rf_skin, NULL, 0.45, 0.0, 0.0); ST(ps, "hide", NULL);
    SD_RET(f);
}

/* swamp / mud_boil */
static int b_mud_boil(SD_ARGS) {
    SD_BEGIN
    double R = u(1.8, 2.5); SoSolids *ps = NS(); PV *pf = PVn(3); CV *cuts = CVn();
    pv_add(pf, R, 0.0); pv_add(pf, R * 0.75, 0.3); pv_add(pf, R * 0.5, 0.36); double sq = u(0.8, 1.0); TURN(ps, cx, cy, pf, 18, sq, a, 0.0);
    int64_t n = RI(rng, 2, 5);
    for (int64_t i = 0; i < n; i++) { double r0 = u(0, R * 0.5), t0 = u(0, 6.28); P2 p = POL(cx, cy, r0, t0); double r = u(0.35, 0.6); DOME(ps, p.x, p.y, r, r * 1.0, 0.3, 1.0, 0.0, 10); }
    GEOSGeometry *f = DISC(cx, cy, R + 0.2, 20);
    cv_add(cuts, ANN(cx, cy, R * 0.72, R * 0.72 - 0.26, 16), 0.16); ST(ps, "mud", cuts);
    SD_RET(f);
}

/* swamp / mossy_log */
static int rf_log(void *c, double t, double *o) { double L = ((C2 *)c)->L, r = ((C2 *)c)->r; *o = r * (1 - 0.15 * t / L); return SO_OK; }
static int b_mossy_log(SD_ARGS) {
    SD_BEGIN
    SoFrame ff = so_frame(cx, cy, a); double L = u(5.0, 7.0), r = u(0.9, 1.15); C2 cl = { L, r }; SoSolids *ps = NS();
    SPIN(ps, cx, cy, a, L, rf_log, &cl, 0.75, 0.0, "round");
    int64_t n = RI(rng, 3, 6);
    for (int64_t i = 0; i < n; i++) { double x = u(-L / 2 + 0.6, L / 2 - 0.6), v = u(-r * 0.5, r * 0.5); P2 c = FF(&ff, x, v); double q = u(0.5, 0.8); DOME(ps, c.x, c.y, q, q * 0.8, r * 0.55, 1.0, 0.0, 10); }
    GEOSGeometry *f = RECT(cx, cy, L, 2 * r + 0.3, a);
    ST(ps, "moss", NULL);
    SD_RET(f);
}

/* swamp / shaman_drum */
static int b_shaman_drum(SD_ARGS) {
    SD_BEGIN
    double r = u(1.2, 1.5), h = u(1.0, 1.5); int64_t n = RI(rng, 6, 9); P2 c = POL(cx, cy, r + 1.3, a); SoSolids *ps = NS(), *beat = NS(); PV *pf = PVn(5), *rp = PVn(2);
    pv_add(pf, r * 0.9, 0.0); pv_add(pf, r, h * 0.5); pv_add(pf, r * 0.94, h); pv_add(pf, r * 0.94 + 0.12, h + 0.02); pv_add(pf, r * 0.94 + 0.12, h + 0.14); TURN(ps, cx, cy, pf, 20, 1.0, 0.0, 0.0);
    for (int64_t k = 0; k < n; k++) { P2 p = POL(cx, cy, r * 0.94 + 0.05, a + 6.2832 * (double)k / (double)n); CYL(ps, p.x, p.y, 0.26, h + 0.3, 0.0, 6); }
    pv_p(rp, POL(c.x, c.y, 1.3, a + 1.2)); pv_p(rp, POL(c.x, c.y, 1.3, a + 1.2 + SD_PI)); ROD(beat, rp, 0.25, NULL, NULL, 1.0, 0.0, 0.0);
    P2 p = POL(c.x, c.y, 1.3, a + 1.2); DOME(beat, p.x, p.y, 0.42, 0.7, 0.0, 1.0, 0.0, 20);
    GV *hl = GVn(); gv_add(hl, DISC(cx, cy, r + 0.4, 20)); gv_add(hl, DISC(c.x, c.y, 1.6, 20)); GEOSGeometry *f = HL(hl);
    ST(ps, "hide", NULL); ST(beat, "log", NULL);
    SD_RET(f);
}

/* swamp / croc_skull */
static int rf_croc(void *c, double t, double *o) {
    double L = ((C2 *)c)->L, W = ((C2 *)c)->r, k, q;
    if (t < -L * 0.15) k = 1.0;
    else if (t < L * 0.15) { if (RF_PW(COS(pmin(1.0, (t + L * 0.15) / (L * 0.3)) * 1.5708), 2.0, &q) != SO_OK) return SO_ERR; k = 0.5 + 0.5 * q * 1.0; }
    else k = 0.5;
    *o = W / 2 * k * (t > L / 2 - 0.4 ? 0.9 : 1.0) + 0.03; return SO_OK;
}
static int b_croc_skull(SD_ARGS) {
    SD_BEGIN
    SoFrame ff = so_frame(cx, cy, a); double L = u(4.6, 6.0), W = u(1.9, 2.3); C2 cl = { L, W }; SoSolids *ps = NS(); CV *cuts = CVn(); P2 p;
    SPIN(ps, cx, cy, a, L, rf_croc, &cl, 0.7, 0.0, "round");
    DV xs = ARANGE(-L * 0.05, L / 2 - 0.3, 0.7);
    for (int i = 0; i < xs.n; i++) for (int sv = -1; sv <= 1; sv += 2) { P2 c = FF(&ff, xs.v[i], sv * (W * 0.27 + 0.1)); CONE(ps, c.x, c.y, 0.3, 0.75, 0.21, 0.0, 6, 1.0, 0.0); }
    for (int sv = -1; sv <= 1; sv += 2) { p = FF(&ff, -L * 0.25, sv * W * 0.25); cv_add(cuts, DISC(p.x, p.y, 0.34, 8), 0.28); }
    for (int sv = -1; sv <= 1; sv += 2) { p = FF(&ff, L / 2 - 0.5, sv * 0.22); cv_add(cuts, DISC(p.x, p.y, 0.2, 6), 0.3); }
    GEOSGeometry *f = RECT(cx, cy, L + 0.2, W + 0.3, a);
    ST(ps, "bone", cuts);
    SD_RET(f);
}

/* swamp / turtle_shell */
static int b_turtle_shell(SD_ARGS) {
    SD_BEGIN
    double r = u(1.5, 2.0), h = r * u(0.5, 0.65), sq = u(0.78, 0.9); SoSolids *ps = NS(); PV *pf = PVn(12); CV *cuts = CVn();
    pv_add(pf, r + 0.2, 0.0); pv_add(pf, r + 0.2, 0.18); pv_add(pf, r, 0.2); pv_crv(pf, r, 0.3, 0.2, h); TURN(ps, cx, cy, pf, 20, sq, a, 0.0);
    cv_add(cuts, BUF(EXTR(NGON(cx, cy, r * 0.36, 6, a)), 0.13, 16), h * 0.72);
    for (int k = 0; k < 6; k++) cv_add(cuts, BARL(POL(cx, cy, r * 0.36, a + k * 1.0472), POL(cx, cy, r * 0.82, a + k * 1.0472), 0.25), h * 0.45);
    GEOSGeometry *f = DISC(cx, cy, r + 0.3, 20);
    ST(ps, "carapace", cuts);
    SD_RET(f);
}

/* swamp / dart_gourds */
static int b_dart_gourds(SD_ARGS) {
    SD_BEGIN
    SoFrame ff = so_frame(cx, cy, a); SoSolids *ps = NS(), *pipe = NS(); int64_t n = RI(rng, 2, 3);
    for (int64_t i = 0; i < n; i++) {
        double v = u(-0.3, 0.3); P2 c = FF(&ff, ((double)i - (double)(n - 1) / 2) * 1.8, v); double r = u(0.65, 0.85), h = u(1.4, 1.9); PV *pf = PVn(12);
        pv_add(pf, r * 0.6, 0.0); pv_add(pf, r, h * 0.3); pv_crv(pf, r, 0.36, h * 0.3, h * 0.7); pv_add(pf, 0.3, h * 0.85); pv_add(pf, 0.4, h); TURN(ps, c.x, c.y, pf, 14, 1.0, 0.0, 0.0);
    }
    double L = u(5.5, 7.5); P2 q = FF(&ff, 0, 1.7); double b = a + u(-0.2, 0.2); SoFrame pp = so_frame(q.x, q.y, b);
    P2 p = FF(&ff, 0, 0.6); GEOSGeometry *f = RECT(p.x, p.y, pmax(L, (double)n * 1.8) + 0.4, 3.6, a);
    ST(ps, "clay", NULL);
    PV *rp = PVn(2); pv_p(rp, FF(&pp, -L / 2, 0)); pv_p(rp, FF(&pp, L / 2, 0)); ROD(pipe, rp, 0.27, NULL, NULL, 1.1, 0.0, 0.0); P2 e = FF(&pp, -L / 2 + 0.4, 0); LIE(pipe, e.x, e.y, a, 0.6, 0.4, 0.0);
    ST(pipe, "reed", NULL);
    SD_RET(f);
}

/* swamp / mat_and_bowl */
static int b_mat_and_bowl(SD_ARGS) {
    SD_BEGIN
    SoFrame ff = so_frame(cx, cy, a); double L = u(4.0, 5.0), W = u(2.8, 3.4), x0 = u(-1, 1), v0 = u(-0.6, 0.6); P2 c = FF(&ff, x0, v0); double r = u(0.8, 1.0); CV *cuts = CVn(); SoSolids *ps = NS(); P2 p;
    DV xs = ARANGE(-L / 2 + 0.6, L / 2 - 0.3, 0.6);
    for (int i = 0; i < xs.n; i++) if (fabs(xs.v[i] - (c.x - cx)) > 9) { p = FF(&ff, xs.v[i], 0); cv_add(cuts, RECT(p.x, p.y, 0.24, W - 0.5, a), 0.14); }
    GEOSGeometry *bowl = NULL;
    for (int i = 0; i < xs.n; i++) {
        double vv[2] = { -W / 2 + 0.7, W / 2 - 0.7 };
        for (int j = 0; j < 2; j++) {
            p = FF(&ff, xs.v[i], vv[j]); bowl = DISC(c.x, c.y, r, 20);
            if (GDIST(RECT(p.x, p.y, 0.3, 1.0, a), bowl) > 0.3) cv_add(cuts, RECT(p.x, p.y, 0.24, 0.9, a), 0.14);
        }
    }
    EXT(ps, RECT(cx, cy, L, W, a), 0.3, 0.0); PV *pf = PVn(2); pv_add(pf, r * 0.6, 0.0); pv_add(pf, r, 0.85); TURN(ps, c.x, c.y, pf, 14, 1.0, 0.0, 0.0);
    GEOSGeometry *f = RECT(cx, cy, L + 0.2, W + 0.2, a);
    cv_add(cuts, DISC(c.x, c.y, r - 0.3, 10), 0.5); ST(ps, "reed", cuts);
    SD_RET(f);
}

/* swamp / pilings */
static int b_pilings(SD_ARGS) {
    SD_BEGIN
    SoFrame ff = so_frame(cx, cy, a); int64_t n = RI(rng, 3, 5); SoSolids *ps = NS(); CV *cuts = CVn();
    for (int64_t i = 0; i < n; i++) {
        double x = ((double)i - (double)(n - 1) / 2) * u(1.5, 1.9), v = u(-0.4, 0.4); P2 c = FF(&ff, x, v); double r = u(0.5, 0.7), h = u(0.9, 2.8); PV *pf = PVn(3);
        pv_add(pf, r * 1.15, 0.0); pv_add(pf, r, 0.4); pv_add(pf, r, h); TURN(ps, c.x, c.y, pf, 9, 1.0, 0.0, 0.0);
        double t = u(0, 6.28); GEOSGeometry *tr = TRI(POL(c.x, c.y, r, t), c, r * 1.2); cv_add(cuts, tr, h - u(0.3, 0.6));
    }
    GEOSGeometry *f = RECT(cx, cy, (double)n * 1.9 + 0.6, 2.4, a);
    ST(ps, "log", cuts);
    SD_RET(f);
}

/* swamp / bone_fetish */
static int rf_knuckle(void *c, double q, double *o) { (void)c; *o = 0.3 + 0.16 * ((q < 0.12 || q > 0.88) ? 1.0 : 0.0); return SO_OK; }
static int rf_feather(void *c, double q, double *o) { (void)c; *o = 0.42 - 0.1 * fabs(q); return SO_OK; }
static int b_bone_fetish(SD_ARGS) {
    SD_BEGIN
    double L = u(3.6, 4.6); SoSolids *ps = NS();
    DOME(ps, cx, cy, 0.55, 0.75, 0.0, 1.0, 0.0, 20);
    double bs[2]; bs[0] = a + u(0.5, 0.9); bs[1] = a - u(0.5, 0.9);
    for (int i = 0; i < 2; i++) { PV *rp = PVn(2); pv_p(rp, POL(cx, cy, L / 2, bs[i])); pv_p(rp, POL(cx, cy, L / 2, bs[i] + SD_PI)); ROD(ps, rp, 0, rf_knuckle, NULL, 1.0, 0.0, 0.0); }
    ST(ps, "bone", NULL);
    int64_t n = RI(rng, 2, 3);
    for (int64_t i = 0; i < n; i++) {
        double t = a + 1.57 + (double)(i - 1) * 0.5; P2 p0 = POL(cx, cy, 0.9, t); P2 e = POL(cx, cy, u(2.4, 3.2), t); SoSolids *s2 = NS();
        SPIN(s2, (p0.x + e.x) / 2, (p0.y + e.y) / 2, t, DIST(p0, e), rf_feather, NULL, 0.5, 0.0, "ridge"); ST(s2, "reed", NULL);
    }
    SD_RET(DISC(cx, cy, L / 2 + 0.9, 20));
}

/* swamp / gator_slide */
static int b_gator_slide(SD_ARGS) {
    SD_BEGIN
    SoFrame ff = so_frame(cx, cy, a); double L = u(6.0, 9.0); DV xs = LINSPACE(-L / 2, L / 2, 14); PV *pts = PVn(14); GV *gs = GVn(); static const double T3[3] = { -0.5, 0, 0.5 };
    for (int i = 0; i < xs.n; i++) pv_p(pts, FF(&ff, xs.v[i], 0.5 * SIN(xs.v[i] * 0.9)));
    gv_add(gs, LBUF(pts, 0.55, 6));
    DV ys = ARANGE(-L / 2 + 1, L / 2 - 0.8, 1.9);
    for (int i = 0; i < ys.n; i++)
        for (int sv = -1; sv <= 1; sv += 2) {
            double x = ys.v[i]; GV *l = GVn();
            for (int k = 0; k < 3; k++) {
                P2 c1 = FF(&ff, x, sv * 1.5 + 0.5 * SIN(x * 0.9)); P2 q1 = POL(c1.x, c1.y, 0.1, a);
                P2 c2 = FF(&ff, x, sv * 1.5 + 0.5 * SIN(x * 0.9)); P2 q2 = POL(c2.x, c2.y, 0.8, a + sv * 0.6 + T3[k]);
                gv_add(l, TRI(q1, q2, 0.4));
            }
            gv_add(gs, UL(l));
        }
    GEOSGeometry *f = RECT(cx, cy, L + 0.6, 4.6, a);
    LGCUT(UL(gs), 0.3, "rut");
    SD_RET(f);
}

/* ====================================================================================================== cynder */
/* cynder / skylight (ground) */
static int b_skylight(SD_ARGS) {
    SD_BEGIN
    GEOSGeometry *p = BLOB(cx, cy, u(1.5, 2.1), rng, 4); SoSolids *ps = NS();
    int64_t n = RI(rng, 5, 8); GEOSGeometry *ex = EXTR(BUF(p, 1.15, 16));
    for (int64_t i = 0; i < n; i++) {
        double d = FMOD(((double)i + u(-0.2, 0.2)) / (double)n, 1.0); GEOSGeometry *q = INTERP(ex, d, 1); double r = u(0.45, 0.6), h = u(0.4, 0.7);
        ROCK(ps, shp_x(q), shp_y(q), r, h, rng, 0.0, 0.0, 9);
    }
    GEOSGeometry *f = BUF(p, 2.0, 16);
    LGCUT(p, u(0.6, 0.85), "lava"); ST(ps, "basalt", NULL);
    SD_RET(f);
}

/* cynder / basalt_columns */
static int b_basalt_columns(SD_ARGS) {
    SD_BEGIN
    double r = u(0.6, 0.8), s = r * 1.74; SoSolids *ps = NS(); GV *foot_ = GVn();
    for (int i = -2; i < 3; i++)
        for (int j = -2; j < 3; j++) {
            double x = cx + (i + 0.5 * (double)IMOD(j, 2)) * s, y = cy + j * s * 0.866, dd = DIST(p2(x, y), p2(cx, cy));
            if (dd < u(1.6, 2.6)) { GEOSGeometry *g = NGON(x, y, r - 0.04, 6, 0.5236); double h = pmax(0.4, u(1.4, 3.2) * (1 - dd / 4.2)); EXT(ps, g, h, 0.0); gv_add(foot_, DISC(x, y, r, 20)); }
        }
    if (!ps->n) SD_NONE;
    GEOSGeometry *f = BUF(HL(foot_), 0.2, 16);
    ST(ps, "basalt", NULL);
    SD_RET(f);
}

/* cynder / ropy_lava */
static int rf_ropy(void *c, double q, double *o) { (void)c; *o = 0.5 - 0.15 * fabs(2 * q - 1); return SO_OK; }
static int b_ropy_lava(SD_ARGS) {
    SD_BEGIN
    int64_t n = RI(rng, 4, 6); double span = u(0.9, 1.4), R0_ = u(1.6, 2.2); SoSolids *ps = NS(); GV *hl = GVn();
    for (int64_t k = 0; k < n; k++) {
        double R = R0_ + (double)k * 0.85;
        ROD(ps, ARC(cx - R0_ * COS(a), cy - R0_ * SIN(a), R, a - span / 2 * (1 - 0.06 * (double)k), a + span / 2 * (1 - 0.06 * (double)k), 10), 0, rf_ropy, NULL, 0.9, 0.0, 0.0);
    }
    int64_t ks[2] = { 0, n - 1 };
    for (int i = 0; i < 2; i++) for (int sv = -1; sv <= 1; sv += 2) { P2 p = POL(cx - R0_ * COS(a), cy - R0_ * SIN(a), R0_ + (double)ks[i] * 0.85, a + sv * span / 2); gv_add(hl, DISC(p.x, p.y, 0.5, 20)); }
    GEOSGeometry *f = HL(hl);
    ST(ps, "basalt", NULL);
    SD_RET(f);
}

/* cynder / fumarole */
static int b_fumarole(SD_ARGS) {
    SD_BEGIN
    double r = u(1.3, 1.8), h = u(1.0, 1.6); SoSolids *ps = NS(); PV *pf = PVn(3); CV *cuts = CVn();
    pv_add(pf, r, 0.0); pv_add(pf, r * 0.6, h * 0.6); pv_add(pf, r * 0.42, h); double sq = u(0.8, 1.0); TURN(ps, cx, cy, pf, 12, sq, a, 0.0);
    cv_add(cuts, DISC(cx, cy, r * 0.25, 8), 0.2); ST(ps, "basalt", cuts);
    int64_t n = RI(rng, 3, 7);
    for (int64_t i = 0; i < n; i++) { double rr = r + u(0.4, 1.2), t = u(0, 6.28); P2 p = POL(cx, cy, rr, t); GEOSGeometry *st = STONE(p.x, p.y, u(0.4, 0.55), rng, 0.8, 5); LMOUND(st, u(0.25, 0.4), "ember", SD_PEB_T); }
    SD_RET(DISC(cx, cy, r + 1.5, 20));
}

/* cynder / lava_bomb (ground) */
static int rf_bomb(void *c, double t, double *o) {          /* r * math.sqrt(max(0.0, 1 - (2 * t / L) ** 2)) ** 1.4 + 0.08 */
    double L = ((C2 *)c)->L, r = ((C2 *)c)->r, q, p; if (RF_PW(2 * t / L, 2.0, &q) != SO_OK || RF_PW(sqrt(pmax(0.0, 1 - q)), 1.4, &p) != SO_OK) return SO_ERR;
    *o = r * p + 0.08; return SO_OK;
}
static int b_lava_bomb(SD_ARGS) {
    SD_BEGIN
    SoFrame ff = so_frame(cx, cy, a); double L = u(2.6, 3.6), r = u(0.9, 1.2); C2 cl = { L, r }; SoSolids *ps = NS(); CV *cuts = CVn();
    SPIN(ps, cx, cy, a, L, rf_bomb, &cl, 1.0, 0.0, "round");
    GEOSGeometry *f = DISC(cx, cy, L / 2 + 1.0, 20);
    LGCUT(ELL(cx, cy, L / 2 + 0.8, r + 0.8, a, 16), 0.3, "scorch");
    cv_add(cuts, WAVY(FF(&ff, -L * 0.3, -r), FF(&ff, L * 0.2, r), rng, 0.3, 3, 0.26), r * 0.45); ST(ps, "basalt", cuts);
    SD_RET(f);
}

/* cynder / anvil */
static int b_anvil(SD_ARGS) {
    SD_BEGIN
    SoFrame ff = so_frame(cx, cy, a); double L = u(2.6, 3.2), W = u(1.2, 1.5), h = u(1.6, 2.1); SoSolids *ps = NS(); CV *cuts = CVn(); P2 p;
    BLK(ps, cx, cy, L * 0.8, W + 0.5, a, h * 0.25, 0.0); BLK(ps, cx, cy, L * 0.5, W * 0.8, a, h * 0.7, 0.0);
    p = FF(&ff, 0.1, 0); HANG(ps, RECT(p.x, p.y, L, W, a), h * 0.66, h); SLAB(ps, TRI(FF(&ff, L / 2, 0), FF(&ff, L / 2 + 1.4, 0), W), h * 0.72, h);
    p = FF(&ff, 0.6, 0); GEOSGeometry *f = RECT(p.x, p.y, L + 1.6, W + 0.8, a);
    p = FF(&ff, -L * 0.35, 0); cv_add(cuts, RECT(p.x, p.y, 0.34, 0.34, a), h - 0.3); ST(ps, "iron", cuts);
    SD_RET(f);
}

/* cynder / crucible */
static int b_crucible(SD_ARGS) {
    SD_BEGIN
    double r = u(1.0, 1.3), h = u(1.3, 1.7); P2 c = POL(cx, cy, r + 1.6, a); double b = a + u(0.6, 2.4); SoSolids *tongs = NS(), *ps = NS(); PV *pf = PVn(3); CV *cuts = CVn();
    for (int sv = -1; sv <= 1; sv += 2) { PV *rp = PVn(3); pv_p(rp, POL(c.x, c.y, 2.2, b + SD_PI + sv * 0.12)); pv_p(rp, POL(c.x, c.y, 0.2, b)); pv_p(rp, POL(c.x, c.y, 1.5, b - sv * 0.3)); ROD(tongs, rp, 0.25, NULL, NULL, 1.1, 0.0, 0.0); }
    GV *hl = GVn(); gv_add(hl, DISC(cx, cy, r + 0.3, 20)); gv_add(hl, DISC(c.x, c.y, 2.3, 20)); GEOSGeometry *f = HL(hl);
    pv_add(pf, r * 0.7, 0.0); pv_add(pf, r, h * 0.8); pv_add(pf, r + 0.12, h); TURN(ps, cx, cy, pf, 14, 1.0, 0.0, 0.0); P2 p = POL(cx, cy, r + 0.2, a + 3.14); BLK(ps, p.x, p.y, 0.7, 0.6, a, h, 0.0);
    GEOSGeometry *q = DISC(cx, cy, r - 0.32, 12); cv_add(cuts, q, h - u(0.3, 0.7)); ST(ps, "iron", cuts); ST(tongs, "iron", NULL);
    SD_RET(f);
}

/* cynder / ingots */
static int b_ingots(SD_ARGS) {
    SD_BEGIN
    SoFrame ff = so_frame(cx, cy, a); int64_t n = RI(rng, 2, 4); SoSolids *ps = NS();
    for (int64_t i = 0; i < n; i++) { double x = u(-0.1, 0.1); P2 c = FF(&ff, x, ((double)i - (double)(n - 1) / 2) * 1.05); double t = a + u(-0.05, 0.05); LOAF(ps, c.x, c.y, t, 2.4, 0.95, 0.5, 0.0); }
    int64_t m = RI(rng, 0, n - 1);
    for (int64_t i = 0; i < m; i++) { double x = u(-0.2, 0.2); P2 c = FF(&ff, x, ((double)i - (double)(n - 2) / 2) * 1.05); double t = a + u(-0.3, 0.3); LOAF(ps, c.x, c.y, t, 2.4, 0.95, 0.5, 0.46); }
    GEOSGeometry *f = RECT(cx, cy, 2.9, (double)n * 1.05 + 0.4, a);
    ST(ps, "iron", NULL);
    SD_RET(f);
}

/* cynder / obsidian_blades */
static int rf_blade(void *c, double q, double *o) { double L = ((C1 *)c)->k; *o = 0.36 * (1 - fabs(2 * q / L)) + 0.12; return SO_OK; }
static int b_obsidian_blades(SD_ARGS) {
    SD_BEGIN
    SoSolids *ps = NS(); GV *foot_ = GVn(); int64_t n = RI(rng, 4, 8);
    for (int64_t i = 0; i < n; i++) {
        double r0 = u(0, 2.0), t0 = u(0, 6.28); P2 p = POL(cx, cy, r0, t0); double L = u(1.2, 2.4), t = u(0, 3.14); C1 cl = { L }; double hk = u(2.5, 5.0);
        SPIN(ps, p.x, p.y, t, L, rf_blade, &cl, hk, 0.0, "ridge"); gv_add(foot_, DISC(p.x, p.y, L / 2, 20));
    }
    GEOSGeometry *f = BUF(HL(foot_), 0.2, 16);
    ST(ps, "obsidian", NULL);
    SD_RET(f);
}

/* cynder / charred_hand */
static int rf_palm(void *c, double t, double *o) { (void)t; *o = 0.8 * ((C1 *)c)->k; return SO_OK; }
static int rf_finger(void *c, double q, double *o) { (void)c; double p; if (RF_PW(SIN(q * 9.4), 2.0, &p) != SO_OK) return SO_ERR; *o = 0.25 + 0.06 * p; return SO_OK; }
static int b_charred_hand(SD_ARGS) {
    SD_BEGIN
    SoFrame ff = so_frame(cx, cy, a); double k = u(0.9, 1.2); C1 ck = { k }; SoSolids *ps = NS(); GV *tips = GVn(); PV *rp = PVn(2);
    SPIN(ps, cx, cy, a, 1.9 * k, rf_palm, &ck, 0.6, 0.0, "round");
    pv_p(rp, FF(&ff, -0.9 * k, 0)); { double v = u(-0.6, 0.6); pv_p(rp, FF(&ff, -3.0 * k, v)); } ROD(ps, rp, 0.42 * k, NULL, NULL, 1.0, 0.0, 0.0);
    for (int i = 0; i < 5; i++) {
        double v = (i - 2) * 0.42 * k, L = (0 < i && i < 4 ? u(1.5, 2.0) : 1.1) * k, b = (i - 2) * 0.22 + u(-0.15, 0.15) + (i == 0 ? 0.9 : 0);
        P2 p0 = FF(&ff, 0.8 * k, v), p1 = POL(p0.x, p0.y, L * 0.55, a + b); double t2 = a + b + u(-0.5, 0.5); P2 p2_ = POL(p1.x, p1.y, L * 0.45, t2);
        PV *fp = PVn(3); pv_p(fp, p0); pv_p(fp, p1); pv_p(fp, p2_); ROD(ps, fp, 0, rf_finger, NULL, 1.1, 0.0, 0.0); gv_add(tips, DISC(p2_.x, p2_.y, 0.3, 20));
    }
    P2 p = FF(&ff, -1.2 * k, 0); GV *hl = GVn(); for (int i = 0; i < tips->n; i++) gv_add(hl, tips->v[i]); gv_add(hl, RECT(p.x, p.y, 4.0 * k, 1.8 * k, a)); GEOSGeometry *f = HL(hl);
    ST(ps, "ash", NULL);
    SD_RET(f);
}

/* cynder / horn_spur */
static int rf_horn(void *c, double q, double *o) { *o = ((C1 *)c)->k * (1 - 0.72 * q); return SO_OK; }
static int b_horn_spur(SD_ARGS) {
    SD_BEGIN
    double R = u(2.2, 3.2), a0 = a - u(0.5, 0.8), a1 = a + u(0.5, 0.8); PV *pts = ARC(cx - R * COS(a), cy - R * SIN(a), R, a0, a1, 12); double r0 = u(0.75, 1.0); C1 cl = { r0 }; SoSolids *ps = NS();
    GEOSGeometry *f = LBUF(pts, r0 + 0.2, 16);
    ROD(ps, pts, 0, rf_horn, &cl, u(1.6, 2.4), 0.0, 0.0); ST(ps, "tooth", NULL);
    SD_RET(f);
}

/* cynder / slag_heap */
static int b_slag_heap(SD_ARGS) {
    SD_BEGIN
    double r = u(1.8, 2.5), h = u(0.8, 1.2); static const double TT[3] = { 0.3, 2.2, 4.3 }; CV *cuts = CVn(); SoSolids *ps = NS(); PV *pf = PVn(3);
    int64_t m = RI(rng, 2, 3);
    for (int64_t i = 0; i < m; i++) { double t = TT[i]; P2 p1 = POL(cx, cy, r * 0.9, a + t); double t2 = a + t + u(-0.6, 0.6); P2 p2_ = POL(cx, cy, r * 0.15, t2); cv_add(cuts, WAVY(p1, p2_, rng, 0.3, 3, 0.26), h * 0.3); }
    GEOSGeometry *f = DISC(cx, cy, r + 0.2, 20);
    pv_add(pf, r, 0.0); pv_add(pf, r * 0.8, h * 0.6); pv_add(pf, r * 0.4, h); double sq = u(0.75, 0.95); TURN(ps, cx, cy, pf, 11, sq, a, 0.0);
    ST(ps, "ash", cuts);
    SD_RET(f);
}

/* cynder / forge_bellows */
static int b_forge_bellows(SD_ARGS) {
    SD_BEGIN
    SoFrame ff = so_frame(cx, cy, a); double L = u(3.6, 4.6), W = u(2.2, 2.8); SoSolids *ps = NS(); CV *cuts = CVn(); P2 p;
    P2 q[6] = { FF(&ff, -L / 2, -W / 2), FF(&ff, L * 0.2, -W * 0.3), FF(&ff, L / 2, -0.3), FF(&ff, L / 2, 0.3), FF(&ff, L * 0.2, W * 0.3), FF(&ff, -L / 2, W / 2) };
    GEOSGeometry *g = BUF(BUF(POLY(P2V(6, q)), -0.2, 16), 0.2, 16);
    EXT(ps, g, 0.9, 0.0); EXT(ps, BUF(g, -0.35, 16), 1.15, 0.0);
    { PV *rp = PVn(2); pv_p(rp, FF(&ff, L / 2 - 0.2, 0)); pv_p(rp, FF(&ff, L / 2 + 1.3, 0)); ROD(ps, rp, 0.3, NULL, NULL, 1.1, 0.0, 0.0); }
    for (int sv = -1; sv <= 1; sv += 2) { PV *rp = PVn(2); pv_p(rp, FF(&ff, -L / 2 + 0.2, sv * 0.4)); pv_p(rp, FF(&ff, -L / 2 - 1.2, sv * 0.5)); ROD(ps, rp, 0.28, NULL, NULL, 1.0, 0.0, 0.0); }
    DV xs = ARANGE(-L / 2 + 0.7, L * 0.2, 0.6);
    for (int i = 0; i < xs.n; i++) { p = FF(&ff, xs.v[i], 0); cv_add(cuts, RECT(p.x, p.y, 0.24, W, a), 0.95); }
    GEOSGeometry *f = RECT(cx, cy, L + 2.8, W + 0.2, a);
    ST(ps, "leather", cuts);
    SD_RET(f);
}

/* cynder / ember_moat (ground) */
static int b_ember_moat(SD_ARGS) {
    SD_BEGIN
    double R = u(2.0, 2.7), w = u(0.6, 0.9); GEOSGeometry *g = ANN(cx, cy, R, R - w, 22); P2 p1 = POL(cx, cy, 2 * R, a); double t = a + u(0.4, 0.8); P2 tr[3] = { p2(cx, cy), p1, POL(cx, cy, 2 * R, t) };
    GEOSGeometry *gap = POLY(P2V(3, tr)); SoSolids *ps = NS();
    GEOSGeometry *f = DISC(cx, cy, R + 0.2, 20);
    LGCUT(DIFF(g, gap), 0.5, "lava");
    double r = u(0.7, 1.0), h = u(0.8, 1.3); ROCK(ps, cx, cy, r, h, rng, 0.0, 0.0, 9); ST(ps, "basalt", NULL);
    SD_RET(f);
}

/* cynder / sulfur_crust */
static int b_sulfur_crust(SD_ARGS) {
    SD_BEGIN
    GEOSGeometry *p = BLOB(cx, cy, u(1.4, 1.9), rng, 4); SoSolids *ps = NS();
    EXT(ps, p, 0.25, 0.0);
    int64_t n = RI(rng, 5, 9);
    for (int64_t i = 0; i < n; i++) { double r0 = u(0, 1.0), t0 = u(0, 6.28); P2 q = POL(cx, cy, r0, t0); double r = u(0.3, 0.45), h = u(0.6, 1.2); CONE(ps, q.x, q.y, r, h, 0.21, 0.0, 5, 1.0, 0.0); }
    GEOSGeometry *f = BUF(p, 0.3, 16);
    ST(ps, "ember", NULL);
    SD_RET(f);
}

/* cynder / shackles */
static int rf_chain(void *c, double q, double *o) { (void)c; *o = 0.24 + 0.08 * (SIN(q * 40) > 0 ? 1.0 : 0.0); return SO_OK; }
static int b_shackles(SD_ARGS) {
    SD_BEGIN
    double L = u(3.0, 4.5); PV *pts = WIG(p2(cx, cy), POL(cx, cy, L, a), rng, 0.7, 5); P2 e = p2(pts->xy[2 * (pts->n - 1)], pts->xy[2 * (pts->n - 1) + 1]); SoSolids *ps = NS(); PV *pf = PVn(3);
    pv_add(pf, 0.6, 0.0); pv_add(pf, 0.6, 0.6); pv_add(pf, 0.3, 0.9); TURN(ps, cx, cy, pf, 8, 1.0, 0.0, 0.0); EXT(ps, ANN(cx, cy, 0.9, 0.5, 12), 0.4, 0.0);
    ROD(ps, pts, 0, rf_chain, NULL, 1.0, 0.0, 0.0);
    for (int sv = -1; sv <= 1; sv += 2) {
        P2 c = POL(e.x, e.y, 0.75, a + sv * 0.9); P2 tr[3] = { c, POL(c.x, c.y, 2, a + sv * 0.9 - 0.3), POL(c.x, c.y, 2, a + sv * 0.9 + 0.3) };
        EXT(ps, DIFF(ANN(c.x, c.y, 0.7, 0.36, 12), POLY(P2V(3, tr))), 0.45, 0.0);
    }
    GV *hl = GVn(); gv_add(hl, DISC(cx, cy, 1.0, 20)); gv_add(hl, DISC(e.x, e.y, 1.6, 20)); GEOSGeometry *f = HL(hl);
    ST(ps, "iron", NULL);
    SD_RET(f);
}

/* cynder / ash_drift */
static int rf_drift(void *c, double q, double *o) { double W = ((C1 *)c)->k, p; if (RF_PW(SIN(pmax(0.04, pmin(0.96, q)) * SD_PI), 0.6, &p) != SO_OK) return SO_ERR; *o = W * p; return SO_OK; }
static int b_ash_drift(SD_ARGS) {
    SD_BEGIN
    SoFrame ff = so_frame(cx, cy, a); double L = u(5.0, 7.0), W = u(1.3, 1.8), bow = u(0.6, 1.4); DV xs = LINSPACE(-L / 2, L / 2, 14); PV *pts = PVn(14); CV *cuts = CVn(); C1 cl = { W }; SoSolids *ps = NS();
    for (int i = 0; i < xs.n; i++) pv_p(pts, FF(&ff, xs.v[i], bow * (1 - PW(2 * xs.v[i] / L, 2.0))));
    if (RND() < 0.5) for (int i = 0; i < 3; i++) { P2 p = FF(&ff, (i - 1) * 1.2, bow * 0.8 + 0.25 * (IMOD(i, 2) ? 1 : -1)); cv_add(cuts, ELL(p.x, p.y, 0.42, 0.26, a, 8), 0.2); }
    GEOSGeometry *f = LBUF(pts, W, 16);
    ROD(ps, pts, 0, rf_drift, &cl, 0.5, 0.0, 0.0); ST(ps, "ash", cuts);
    SD_RET(f);
}

/* cynder / imp_idol */
static int b_imp_idol(SD_ARGS) {
    SD_BEGIN
    SoFrame ff = so_frame(cx, cy, a); double r = u(0.9, 1.15), h = u(1.9, 2.5); SoSolids *ps = NS(); PV *pf = PVn(12); CV *cuts = CVn(); P2 p;
    EXT(ps, NGON(cx, cy, r * 1.5, 4, a + 0.785), 0.4, 0.0);
    pv_add(pf, r, 0.0); pv_add(pf, r, h * 0.45); pv_add(pf, r * 0.6, h * 0.55); pv_add(pf, r * 0.8, h * 0.7); pv_crv(pf, r * 0.8, 0.3, h * 0.7, h); TURN(ps, cx, cy, pf, 12, 1.0, 0.0, 0.0);
    for (int sv = -1; sv <= 1; sv += 2) { p = FF(&ff, 0, sv * r * 0.6); CONE(ps, p.x, p.y, 0.36, h + 0.6, 0.21, 0.0, 6, 1.0, 0.0); }
    for (int sv = -1; sv <= 1; sv += 2) { p = FF(&ff, r * 0.5, sv * r * 0.3); cv_add(cuts, DISC(p.x, p.y, 0.2, 6), h * 0.72); }
    GEOSGeometry *f = DISC(cx, cy, r * 1.6, 20);
    ST(ps, "basalt", cuts);
    SD_RET(f);
}

/* cynder / crust_cracks */
static int b_crust_cracks(SD_ARGS) {
    SD_BEGIN
    GV *gs = GVn(), *ends = GVn(); int64_t n = RI(rng, 3, 5); P2 c0 = p2(cx, cy);
    for (int64_t i = 0; i < n; i++) {
        double t = a + 6.2832 * (double)i / 4 + u(-0.4, 0.4), er = u(2.4, 4.0); P2 e = POL(cx, cy, er, t); double w = u(0.4, 0.6);
        gv_add(gs, WAVY(c0, e, rng, 0.5, 4, w)); gv_add(ends, DISC(e.x, e.y, 0.3, 20));
        if (RND() < 0.6) {
            P2 m = POL(cx, cy, 1.6, t); double rr = u(1.2, 2.0), tt = u(0.6, 1.1); tt = tt * CHPM(rng); P2 e2 = POL(m.x, m.y, rr, t + tt);
            gv_add(gs, WAVY(m, e2, rng, 0.3, 3, 0.36)); gv_add(ends, DISC(e2.x, e2.y, 0.3, 20));
        }
    }
    GEOSGeometry *f = HL(ends);
    LGCUT(UL(gs), u(0.35, 0.5), "lava");
    SD_RET(f);
}

/* cynder / quench_trough */
static int b_quench_trough(SD_ARGS) {
    SD_BEGIN
    double L = u(4.0, 5.2), W = u(1.9, 2.3), h = u(1.1, 1.4), b = a + u(0.5, 1.2); GV *l = GVn(); SoSolids *ps = NS(), *all = NS(); CV *cuts = CVn();
    gv_add(l, BARL(POL(cx, cy, 2.6, b + SD_PI), POL(cx, cy, 2.4, b), 0.62)); gv_add(l, TRI(POL(cx, cy, 2.35, b), POL(cx, cy, 3.1, b), 0.62));
    GEOSGeometry *blade = UL(l);
    BLK(ps, cx, cy, L, W, a, h, 0.0);
    GV *hl = GVn(); gv_add(hl, RECT(cx, cy, L, W, a)); gv_add(hl, blade); GEOSGeometry *f = HL(hl);
    CAT(all, ps); EXT(all, blade, h + 0.3, 0.0);
    GEOSGeometry *q = DIFF(RECT(cx, cy, L - 1.0, W - 1.0, a), BUF(blade, 0.15, 16)); cv_add(cuts, q, h - u(0.3, 0.5)); ST(all, "basalt", cuts);
    SD_RET(f);
}

/* ====================================================================================================== targa */
/* targa / fallen_icicles */
static int rf_icicle(void *c, double t, double *o) { double L = ((C2 *)c)->L, r = ((C2 *)c)->r; *o = r * (1 - 0.75 * (t + L / 2) / L) + 0.06; return SO_OK; }
static int b_fallen_icicles(SD_ARGS) {
    SD_BEGIN
    SoSolids *ps = NS(); GV *foot_ = GVn(); int64_t n = RI(rng, 3, 6);
    for (int64_t i = 0; i < n; i++) {
        double r0 = u(0, 2.2), t0 = u(0, 6.28); P2 p = POL(cx, cy, r0, t0); double b = a + u(-0.6, 0.6), L = u(1.6, 4.0), r = u(0.4, 0.6); C2 cl = { L, r };
        SPIN(ps, p.x, p.y, b, L, rf_icicle, &cl, 1.1, 0.0, "round"); gv_add(foot_, RECT(p.x, p.y, L, 2 * r, b));
    }
    GEOSGeometry *f = BUF(HL(foot_), 0.2, 16);
    ST(ps, "ice", NULL);
    SD_RET(f);
}

/* targa / snow_crescent */
static int b_snow_crescent(SD_ARGS) {
    SD_BEGIN
    SoFrame ff = so_frame(cx, cy, a); double L = u(5.0, 8.0), W = u(1.2, 1.9), bow = u(0.8, 2.0); DV xs = LINSPACE(-L / 2, L / 2, 16); PV *pts = PVn(16); C1 cl = { W }; SoSolids *ps = NS();
    for (int i = 0; i < xs.n; i++) pv_p(pts, FF(&ff, xs.v[i], bow * (1 - PW(2 * xs.v[i] / L, 2.0))));
    GEOSGeometry *f = LBUF(pts, W, 16);
    ROD(ps, pts, 0, rf_drift, &cl, u(0.5, 0.8), 0.0, 0.0); ST(ps, "snow", NULL);
    SD_RET(f);
}

/* targa / pack_and_snowshoes */
static int b_pack_and_snowshoes(SD_ARGS) {
    SD_BEGIN
    SoFrame ff = so_frame(cx, cy, a); double L = u(2.6, 3.2), W = u(1.8, 2.2); SoSolids *ps = NS(); P2 p;
    LOAF(ps, cx, cy, a, L, W, u(1.0, 1.3), 0.0); p = FF(&ff, L / 2 + 0.3, 0); LIE(ps, p.x, p.y, a + 1.57, W * 0.9, 0.42, 0.0);
    for (int sv = -1; sv <= 1; sv += 2) { p = FF(&ff, 0, sv * W * 0.28); BLK(ps, p.x, p.y, L + 0.1, 0.36, a, 1.38, 0.0); }
    ST(ps, "cloth", NULL);
    int64_t m = RI(rng, 1, 2);
    for (int s = 0; s < m; s++) {
        double sv = s == 0 ? -1.0 : 1.0, x = u(-1, 1); P2 c = FF(&ff, x, sv * (W / 2 + 1.6)); double b = a + u(-0.5, 0.5); GEOSGeometry *o = ELL(c.x, c.y, 2.0, 0.85, b, 16); SoFrame f2 = so_frame(c.x, c.y, b);
        GV *l = GVn(); double xs[3] = { -0.9, 0, 0.9 }; SoSolids *s2 = NS();
        gv_add(l, DIFF(o, BUF(o, -0.4, 16)));
        for (int i = 0; i < 3; i++) gv_add(l, INTER(BARL(FF(&f2, xs[i], -0.8), FF(&f2, xs[i], 0.8), 0.3), o));
        gv_add(l, BARL(FF(&f2, -1.6, 0), FF(&f2, 1.6, 0), 0.3));
        EXT(s2, UL(l), 0.4, 0.0); ST(s2, "wood", NULL);
    }
    SD_RET(RECT(cx, cy, 4.6, W + 5.4, a));
}

/* targa / pylon_stub */
static int b_pylon_stub(SD_ARGS) {
    SD_BEGIN
    double r = u(1.1, 1.5), h = u(1.8, 3.0); SoSolids *ps = NS(); CV *cuts = CVn();
    EXT(ps, NGON(cx, cy, r * 1.25, 6, a), 0.4, 0.0);
    for (int k = 0; k < 5; k++) EXT(ps, NGON(cx, cy, r * (1 - 0.07 * k), 6, a), 0.4 + (h - 0.4) * (k + 1) / 5, 0.0);
    for (int k = 0; k < 6; k++) {
        if (!(RND() < 0.7)) continue;
        GEOSGeometry *g = BARL(POL(cx, cy, r * 0.25, a + 0.5236 + k * 1.0472), POL(cx, cy, r * 1.3, a + 0.5236 + k * 1.0472), 0.26); cv_add(cuts, g, u(0.5, h * 0.7));
    }
    cv_add(cuts, NGON(cx, cy, r * 0.3, 6, a), h - 0.4);
    GEOSGeometry *f = DISC(cx, cy, r * 1.35, 20);
    ST(ps, "alien", cuts);
    SD_RET(f);
}

/* targa / ice_blocks */
static int b_ice_blocks(SD_ARGS) {
    SD_BEGIN
    SoFrame ff = so_frame(cx, cy, a); int64_t n = RI(rng, 2, 4); SoSolids *ps = NS(); int64_t nl = RI(rng, 1, 2);
    for (int64_t lv = 0; lv < nl; lv++)
        for (int64_t i = 0; i < n - lv; i++) {
            double x = ((double)i - (double)(n - lv - 1) / 2) * 1.75 + u(-0.1, 0.1), v = u(-0.15, 0.15); P2 c = FF(&ff, x, v); double t = a + u(-0.08, 0.08);
            EXT(ps, RRB(c.x, c.y, 1.6, 1.2, t, 0.15), 0.9, (double)lv * 0.86);
        }
    GEOSGeometry *f = RECT(cx, cy, (double)n * 1.75 + 0.4, 1.9, a);
    ST(ps, "ice", NULL);
    SD_RET(f);
}

/* targa / pod_lid */
static int rf_pod(void *c, double t, double *o) {          /* W / 2 * max(0.0, 1 - abs(2 * t / L) ** 3) ** 0.5 + 0.05 */
    double L = ((C2 *)c)->L, W = ((C2 *)c)->r, q, p; if (RF_PW(fabs(2 * t / L), 3.0, &q) != SO_OK || RF_PW(pmax(0.0, 1 - q), 0.5, &p) != SO_OK) return SO_ERR;
    *o = W / 2 * p + 0.05; return SO_OK;
}
static int b_pod_lid(SD_ARGS) {
    SD_BEGIN
    SoFrame ff = so_frame(cx, cy, a); double L = u(4.6, 5.8), W = u(2.4, 3.0), h = u(0.8, 1.1); C2 cl = { L, W }; SoSolids *ps = NS(); CV *cuts = CVn(); P2 p;
    SPIN(ps, cx, cy, a, L, rf_pod, &cl, 2 * h / W, 0.0, "round");
    { double vv[2] = { -L * 0.25, L * 0.25 }; for (int i = 0; i < 2; i++) { p = FF(&ff, vv[i], W / 2); BLK(ps, p.x, p.y, 0.7, 0.6, a, 0.5, 0.0); } }
    p = FF(&ff, L * 0.12, 0); GEOSGeometry *win = ELL(p.x, p.y, L * 0.22, W * 0.24, a, 16);
    GEOSGeometry *f = RECT(cx, cy, L + 0.2, W + 0.8, a);
    cv_add(cuts, win, h * 0.72); cv_add(cuts, DIFF(BUF(win, 0.45, 16), BUF(win, 0.22, 16)), h * 0.5); ST(ps, "alien", cuts);
    SD_RET(f);
}

/* targa / conduit_node (ground) */
static int b_conduit_node(SD_ARGS) {
    SD_BEGIN
    double R = u(1.3, 1.7); int64_t n = RI(rng, 2, 4); GV *gs = GVn(), *ends = GVn(); SoSolids *ps = NS(); static const double CH3[3] = { -0.5236, 0, 0.5236 };
    for (int64_t k = 0; k < n; k++) {
        double t = a + 6.2832 * (double)k / (double)n + u(-0.2, 0.2); P2 e = POL(cx, cy, R + u(2.0, 3.5), t); P2 m = POL(cx, cy, R + 1.2, t);
        P2 q0 = POL(cx, cy, R + 0.3, t); double d = DIST(m, e), tt = t + CH3[CH(rng, 3)]; P2 q2 = POL(m.x, m.y, d, tt);
        P2 lp[3] = { q0, m, q2 }; gv_add(gs, LINE(P2V(3, lp), 0.5)); gv_add(ends, DISC(e.x, e.y, 0.8, 20));
    }
    EXT(ps, ANN(cx, cy, R, R - 0.5, 6), 0.6, 0.0); DOME(ps, cx, cy, R * 0.5, R * 0.7, 0.0, 1.0, 0.0, 6);
    GV *hl = GVn(); for (int i = 0; i < ends->n; i++) gv_add(hl, ends->v[i]); gv_add(hl, DISC(cx, cy, R + 0.3, 20)); GEOSGeometry *f = HL(hl);
    LGCUT(UL(gs), 0.35, "slot"); ST(ps, "alien", NULL);
    SD_RET(f);
}

/* targa / disc_wreck */
static int b_disc_wreck(SD_ARGS) {
    SD_BEGIN
    double R = u(2.2, 3.0); P2 q0 = POL(cx, cy, R * 0.35, a); double t1 = a - u(0.3, 0.6); P2 q1 = POL(cx, cy, 2 * R, t1); double t2 = a + u(0.3, 0.6); P2 q2 = POL(cx, cy, 2 * R, t2);
    P2 tr[3] = { q0, q1, q2 }; GEOSGeometry *bite = POLY(P2V(3, tr)); SoSolids *ps = NS(); PV *pf = PVn(12); CV *cuts = CVn();
    pv_add(pf, R, 0.0); pv_add(pf, R, 0.2); pv_add(pf, R * 0.5, 0.75); pv_crv(pf, R * 0.42, 0.3, 0.75, 1.4); TURN(ps, cx, cy, pf, 24, 1.0, 0.0, 0.0);
    cv_add(cuts, bite, 0.0);
    for (int k = 0; k < 7; k++) { P2 p = POL(cx, cy, R * 0.72, a + 0.8 + k * 0.7); cv_add(cuts, DISC(p.x, p.y, 0.26, 8), 0.3); }
    GEOSGeometry *f = DISC(cx, cy, R + 0.2, 20);
    ST(ps, "alien", cuts);
    SD_RET(f);
}

/* targa / pressure_ridge */
static int b_pressure_ridge(SD_ARGS) {
    SD_BEGIN
    SoFrame ff = so_frame(cx, cy, a); int64_t n = RI(rng, 4, 7); SoSolids *ps = NS();
    for (int64_t i = 0; i < n; i++) {
        double v = u(-0.4, 0.4); P2 c = FF(&ff, ((double)i - (double)(n - 1) / 2) * 1.2, v); double t = a + 1.57 + u(-0.6, 0.6), L = u(1.6, 2.6), W = u(0.9, 1.3), h = u(0.8, 1.6);
        RIDGE(ps, c.x, c.y, t, L, W, h, 0.0);
    }
    GEOSGeometry *f = RECT(cx, cy, (double)n * 1.2 + 1.2, 3.2, a);
    ST(ps, "ice", NULL);
    SD_RET(f);
}

/* targa / glyph_tile */
static int b_glyph_tile(SD_ARGS) {
    SD_BEGIN
    double R = u(1.9, 2.5); int64_t k = RI(rng, 3, 6); CV *cuts = CVn(); SoSolids *ps = NS();
    cv_add(cuts, BUF(EXTR(NGON(cx, cy, R * 0.62, 6, a)), 0.14, 16), 0.2); cv_add(cuts, BUF(EXTR(NGON(cx, cy, R * 0.3, (int)k, a)), 0.14, 16), 0.2);
    for (int64_t j = 0; j < k; j++) cv_add(cuts, BARL(POL(cx, cy, R * 0.3, a + 6.2832 * (double)j / (double)k), POL(cx, cy, R * 0.6, a + 6.2832 * (double)j / (double)k), 0.26), 0.2);
    GEOSGeometry *g = NGON(cx, cy, R, 6, a);
    if (RND() < 0.3) cv_add(cuts, WAVY(POL(cx, cy, R * 1.1, a + 1), POL(cx, cy, R * 1.1, a + 4), rng, 0.4, 4, 0.3), 0.0);
    GEOSGeometry *f = BUF(g, 0.2, 16);
    EXT(ps, g, 0.45, 0.0); ST(ps, "alien", cuts);
    SD_RET(f);
}

/* targa / frost_flowers */
static int b_frost_flowers(SD_ARGS) {
    SD_BEGIN
    SoSolids *ps = NS(); GV *foot_ = GVn(); int64_t n = RI(rng, 3, 6);
    for (int64_t i = 0; i < n; i++) {
        double r0 = u(0, 2.2), t0 = u(0, 6.28); P2 p = POL(cx, cy, r0, t0); double r = u(0.7, 1.1), h = r * u(0.5, 0.8); int64_t nn = RI(rng, 6, 9); double aa = u(0, 1);
        FLUTED(ps, p.x, p.y, r, h, (int)nn, aa, 0.45, 4, 0.0, 0.0); gv_add(foot_, DISC(p.x, p.y, r, 20));
    }
    GEOSGeometry *f = BUF(HL(foot_), 0.2, 16);
    ST(ps, "frost", NULL);
    SD_RET(f);
}

/* targa / fishing_hole (ground) */
static int b_fishing_hole(SD_ARGS) {
    SD_BEGIN
    double R = u(1.3, 1.7); SoSolids *ps = NS(), *pole = NS(); P2 e = POL(cx, cy, R + u(2.5, 3.5), a);
    int64_t n = RI(rng, 4, 7); PV *rp = RINGPTS(cx, cy, R + 0.8, (int)n, a + 0.6, 0.2, rng);
    for (int i = 0; i < rp->n; i++) { double r = u(0.5, 0.7), h = u(0.4, 0.7); ROCK(ps, rp->xy[2 * i], rp->xy[2 * i + 1], r, h, rng, 0.0, 0.0, 9); }
    PV *pp = PVn(2); pv_p(pp, POL(cx, cy, R + 1.7, a + 0.15)); pv_p(pp, e); ROD(pole, pp, 0.26, NULL, NULL, 1.0, 0.0, 0.0);
    GV *hl = GVn(); gv_add(hl, DISC(cx, cy, R + 1.6, 20)); gv_add(hl, DISC(e.x, e.y, 0.4, 20)); GEOSGeometry *f = HL(hl);
    LGCUT(DISC(cx, cy, R, 14), 0.6, "water"); ST(ps, "ice", NULL); ST(pole, "wood", NULL);
    SD_RET(f);
}

/* targa / thermal_grille (ground) */
static int b_thermal_grille(SD_ARGS) {
    SD_BEGIN
    SoFrame ff = so_frame(cx, cy, a); double L = u(3.4, 4.4), W = u(2.0, 2.6); GEOSGeometry *o = ELL(cx, cy, L / 2, W / 2, a, 20); CV *cuts = CVn(); SoSolids *ps = NS();
    DV xs = ARANGE(-L / 2 + 0.7, L / 2 - 0.5, 0.62); GEOSGeometry **sl = (GEOSGeometry **)Mk(sizeof(GEOSGeometry *) * (size_t)(xs.n + 1));
    for (int i = 0; i < xs.n; i++) sl[i] = INTER(BARL(FF(&ff, xs.v[i], -W), FF(&ff, xs.v[i], W), 0.3), BUF(o, -0.45, 16));
    GEOSGeometry *melt = DIFF(BUF(o, u(0.9, 1.3), 16), BUF(o, 0.25, 16)), *f = BUF(o, 1.5, 16);
    LGCUT(melt, 0.22, "water");
    for (int i = 0; i < xs.n; i++) if (!EMPTY(sl[i])) cv_add(cuts, sl[i], 0.12);
    EXT(ps, o, 0.42, 0.0); ST(ps, "alien", cuts);
    SD_RET(f);
}

/* targa / sled */
static int b_sled(SD_ARGS) {
    SD_BEGIN
    SoFrame ff = so_frame(cx, cy, a); double L = u(5.5, 7.0), W = u(2.2, 2.7); SoSolids *ps = NS();
    for (int sv = -1; sv <= 1; sv += 2) { PV *rp = PVn(3); pv_p(rp, FF(&ff, -L / 2, sv * W / 2)); pv_p(rp, FF(&ff, L / 2 - 0.9, sv * W / 2)); pv_p(rp, FF(&ff, L / 2, sv * (W / 2 - 0.5))); ROD(ps, rp, 0.3, NULL, NULL, 1.3, 0.0, 0.0); }
    DV xs = ARANGE(-L / 2 + 0.7, L / 2 - 1.2, 1.25);
    for (int i = 0; i < xs.n; i++) EXT(ps, BARL(FF(&ff, xs.v[i], -W / 2), FF(&ff, xs.v[i], W / 2), 0.5), 0.5, 0.0);
    if (RND() < 0.6) { P2 p = FF(&ff, -L * 0.12, 0); LOAF(ps, p.x, p.y, a, L * 0.4, W - 0.9, u(0.9, 1.3), 0.0); }
    GEOSGeometry *f = RECT(cx, cy, L + 0.4, W + 0.8, a);
    ST(ps, "wood", NULL);
    SD_RET(f);
}

/* targa / tusk */
static int rf_tusk(void *c, double q, double *o) { *o = ((C1 *)c)->k * (1 - 0.68 * q); return SO_OK; }
static int b_tusk(SD_ARGS) {
    SD_BEGIN
    double R = u(3.0, 4.5), span = u(1.3, 2.0); PV *pts = ARC(cx - R * COS(a), cy - R * SIN(a), R, a - span / 2, a + span / 2, 14); double r0 = u(0.65, 0.85); C1 cl = { r0 }; SoSolids *ps = NS();
    GEOSGeometry *f = LBUF(pts, r0 + 0.2, 16);
    ROD(ps, pts, 0, rf_tusk, &cl, 1.05, 0.0, 0.0); ST(ps, "bone", NULL);
    SD_RET(f);
}

/* targa / survey_beacon */
static int b_survey_beacon(SD_ARGS) {
    SD_BEGIN
    double r = u(0.8, 1.0), h = u(2.2, 3.2); SoSolids *ps = NS(); GV *tips = GVn();
    DOME(ps, cx, cy, r, r * 1.1, 0.0, 1.0, 0.0, 20); CYL(ps, cx, cy, 0.3, h, 0.0, 6); DOME(ps, cx, cy, 0.42, 0.5, h - 0.1, 1.0, 0.0, 8);
    for (int k = 0; k < 3; k++) {
        P2 e = POL(cx, cy, r + u(1.3, 1.8), a + k * 2.094); PV *rp = PVn(2); pv_p(rp, POL(cx, cy, r * 0.5, a + k * 2.094)); pv_p(rp, e);
        ROD(ps, rp, 0.28, NULL, NULL, 1.0, 0.0, 0.0); EXT(ps, DISC(e.x, e.y, 0.5, 8), 0.3, 0.0); gv_add(tips, DISC(e.x, e.y, 0.6, 20));
    }
    GEOSGeometry *f = HL(tips);
    ST(ps, "metal", NULL);
    SD_RET(f);
}

/* targa / lattice_shard */
static int b_lattice_shard(SD_ARGS) {
    SD_BEGIN
    double R = u(1.8, 2.5); GV *gs = GVn(); SoSolids *ps = NS(); P2 c0 = p2(cx, cy);
    gv_add(gs, NGON(cx, cy, R * 0.28, 6, a));
    for (int k = 0; k < 6; k++) {
        if (RND() < 0.15) continue;
        double t = a + k * 1.0472; gv_add(gs, BARL(c0, POL(cx, cy, R, t), 0.5));
        double qs[2] = { 0.5, 0.75 };
        for (int i = 0; i < 2; i++) { P2 m = POL(cx, cy, R * qs[i], t); for (int sv = -1; sv <= 1; sv += 2) gv_add(gs, BARL(m, POL(m.x, m.y, R * 0.28, t + sv * 1.0472), 0.42)); }
    }
    GEOSGeometry *f = DISC(cx, cy, R + 0.3, 20);
    EXT(ps, UL(gs), u(0.4, 0.6), 0.0); ST(ps, "crystal", NULL);
    SD_RET(f);
}

/* targa / buried_steps */
static int b_buried_steps(SD_ARGS) {
    SD_BEGIN
    SoFrame ff = so_frame(cx, cy, a); double W = u(3.0, 3.8); int n = 3; SoSolids *ps = NS(), *all = NS();
    for (int i = 0; i < n; i++) { P2 c = FF(&ff, i * 0.5, 0); BLK(ps, c.x, c.y, (n - i) * 1.0, W, a, 0.5 * (i + 1), 0.0); }
    double x = u(-0.6, 0.6), v = W / 2 * CHPM(rng); P2 c = FF(&ff, x, v); double r = u(1.5, 2.0);
    GV *hl = GVn(); gv_add(hl, RECT(cx, cy, n + 0.2, W, a)); gv_add(hl, DISC(c.x, c.y, r, 20)); GEOSGeometry *f = HL(hl);
    CAT(all, ps); double h = u(0.9, 1.4), aa = u(0, 3); DOME(all, c.x, c.y, r, h, 0.0, 0.8, aa, 20); ST(all, "alien", NULL);
    SD_RET(f);
}

/* targa / ice_spikes */
static int b_ice_spikes(SD_ARGS) {
    SD_BEGIN
    SoSolids *ps = NS(); GV *foot_ = GVn(); int64_t n = RI(rng, 3, 6);
    for (int64_t i = 0; i < n; i++) {
        double r0 = u(0, 1.9), t0 = u(0, 6.28); P2 p = POL(cx, cy, r0, t0); double r = u(0.45, 0.85), h = r * u(2.2, 3.6); PV *pf = PVn(6);
        pv_add(pf, r, 0.0); pv_add(pf, r * 0.8, h * 0.2); pv_add(pf, r * 0.62, h * 0.25); pv_add(pf, r * 0.5, h * 0.55); pv_add(pf, r * 0.36, h * 0.6); pv_add(pf, 0.22, h);
        TURN(ps, p.x, p.y, pf, 7, 1.0, 0, 0.0); gv_add(foot_, DISC(p.x, p.y, r, 20));
    }
    GEOSGeometry *f = BUF(HL(foot_), 0.2, 16);
    ST(ps, "ice", NULL);
    SD_RET(f);
}

/* targa / power_cell */
static int rf_cell(void *c, double t, double *o) { double L = ((C2 *)c)->L, r = ((C2 *)c)->r; *o = r * (fabs(t) > L / 2 - 0.5 ? 0.75 : 1.0) + (fabs(fabs(t) - L * 0.2) < 0.22 ? 0.14 : 0.0); return SO_OK; }
static int b_power_cell(SD_ARGS) {
    SD_BEGIN
    SoFrame ff = so_frame(cx, cy, a); double L = u(3.2, 4.2), r = u(0.75, 0.95); C2 cl = { L, r }; SoSolids *ps = NS(); CV *cuts = CVn();
    SPIN(ps, cx, cy, a, L, rf_cell, &cl, 1.0, 0.0, "ridge");
    for (int sv = -1; sv <= 1; sv += 2) { P2 p = FF(&ff, sv * L * 0.3, 0); BLK(ps, p.x, p.y, 0.6, 2 * r + 0.8, a, 0.42, 0.0); }
    GEOSGeometry *f = RECT(cx, cy, L + 0.2, 2 * r + 1.0, a);
    cv_add(cuts, RECT(cx, cy, L * 0.24, 0.3, a), r * 0.6); ST(ps, "alien", cuts);
    SD_RET(f);
}

void bld_register_sd_c(void) {
    sd_reg(b_hydrant, "urban", "made", "iron", "hydrant", 0);
    sd_reg(b_street_lamp, "urban", "made", "iron", "street_lamp", 0);
    sd_reg(b_tyres, "urban", "made", "tar", "tyres", 0);
    sd_reg(b_post_box, "urban", "made", "iron", "post_box", 0);
    sd_reg(b_dustbin, "urban", "made", "tin", "dustbin", 0);
    sd_reg(b_brick_pallet, "urban", "made", "brick", "brick_pallet", 0);
    sd_reg(b_storm_drain, "urban", "made", "concrete", "storm_drain", 1);
    sd_reg(b_bottles, "urban", "made", "glass", "bottles", 0);
    sd_reg(b_road_sign, "urban", "made", "iron", "road_sign", 0);
    sd_reg(b_crossarm, "urban", "made", "wood", "crossarm", 0);
    sd_reg(b_hubcap_and_bumper, "urban", "made", "tin", "hubcap_and_bumper", 0);
    sd_reg(b_kerbstones, "urban", "debris", "concrete", "kerbstones", 0);
    sd_reg(b_window_frame, "urban", "made", "wood", "window_frame", 0);
    sd_reg(b_park_bench, "urban", "made", "iron", "park_bench", 0);
    sd_reg(b_rebar_chunk, "urban", "debris", "concrete", "rebar_chunk", 0);
    sd_reg(b_suitcase, "urban", "made", "leather", "suitcase", 0);
    sd_reg(b_radiator, "urban", "made", "iron", "radiator", 0);
    sd_reg(b_bathtub, "urban", "made", "iron", "bathtub", 0);
    sd_reg(b_chalk_outline, "urban", "marks", "seam", "chalk_outline", 0);
    sd_reg(b_setts, "urban", "debris", "concrete", "setts", 0);
    sd_reg(b_dynamite, "mine", "made", "paper", "dynamite", 0);
    sd_reg(b_plunger, "mine", "made", "wood", "plunger", 0);
    sd_reg(b_miner_helmet, "mine", "made", "tin", "miner_helmet", 0);
    sd_reg(b_canary_cage, "mine", "made", "brass", "canary_cage", 0);
    sd_reg(b_kibble, "mine", "made", "iron", "kibble", 0);
    sd_reg(b_sluice_box, "mine", "made", "wood", "sluice_box", 0);
    sd_reg(b_assay_scale, "mine", "made", "brass", "assay_scale", 0);
    sd_reg(b_lunch_pail, "mine", "made", "tin", "lunch_pail", 0);
    sd_reg(b_drill_steels, "mine", "made", "iron", "drill_steels", 0);
    sd_reg(b_oil_lantern, "mine", "made", "tin", "oil_lantern", 0);
    sd_reg(b_wheelset, "mine", "made", "iron", "wheelset", 0);
    sd_reg(b_stalagmites, "mine", "debris", "rock", "stalagmites", 0);
    sd_reg(b_sorting_table, "mine", "made", "wood", "sorting_table", 0);
    sd_reg(b_pulley_block, "mine", "made", "iron", "pulley_block", 0);
    sd_reg(b_claim_stake, "mine", "made", "wood", "claim_stake", 0);
    sd_reg(b_hand_pump, "mine", "made", "iron", "hand_pump", 0);
    sd_reg(b_gold_seam, "mine", "marks", "crack", "gold_seam", 0);
    sd_reg(b_switch_lever, "mine", "made", "iron", "switch_lever", 0);
    sd_reg(b_spilled_ore, "mine", "made", "sack", "spilled_ore", 0);
    sd_reg(b_fallen_stull, "mine", "made", "wood", "fallen_stull", 0);
    sd_reg(b_lily_pool, "swamp", "growth", "lily", "lily_pool", 1);
    sd_reg(b_cypress_knees, "swamp", "growth", "root", "cypress_knees", 0);
    sd_reg(b_dugout, "swamp", "made", "log", "dugout", 0);
    sd_reg(b_fish_trap, "swamp", "made", "twig", "fish_trap", 0);
    sd_reg(b_totem_stake, "swamp", "made", "log", "totem_stake", 0);
    sd_reg(b_shelf_stump, "swamp", "growth", "shroom", "shelf_stump", 0);
    sd_reg(b_frog_idol, "swamp", "made", "tflag", "frog_idol", 0);
    sd_reg(b_reed_bundle, "swamp", "made", "reed", "reed_bundle", 0);
    sd_reg(b_root_tangle, "swamp", "growth", "root", "root_tangle", 0);
    sd_reg(b_shed_skin, "swamp", "made", "hide", "shed_skin", 0);
    sd_reg(b_mud_boil, "swamp", "growth", "mud", "mud_boil", 0);
    sd_reg(b_mossy_log, "swamp", "growth", "moss", "mossy_log", 0);
    sd_reg(b_shaman_drum, "swamp", "made", "hide", "shaman_drum", 0);
    sd_reg(b_croc_skull, "swamp", "made", "bone", "croc_skull", 0);
    sd_reg(b_turtle_shell, "swamp", "made", "carapace", "turtle_shell", 0);
    sd_reg(b_dart_gourds, "swamp", "made", "clay", "dart_gourds", 0);
    sd_reg(b_mat_and_bowl, "swamp", "made", "reed", "mat_and_bowl", 0);
    sd_reg(b_pilings, "swamp", "made", "log", "pilings", 0);
    sd_reg(b_bone_fetish, "swamp", "made", "bone", "bone_fetish", 0);
    sd_reg(b_gator_slide, "swamp", "marks", "rut", "gator_slide", 0);
    sd_reg(b_skylight, "cynder", "marks", "lava", "skylight", 1);
    sd_reg(b_basalt_columns, "cynder", "debris", "basalt", "basalt_columns", 0);
    sd_reg(b_ropy_lava, "cynder", "debris", "basalt", "ropy_lava", 0);
    sd_reg(b_fumarole, "cynder", "debris", "ember", "fumarole", 0);
    sd_reg(b_lava_bomb, "cynder", "debris", "basalt", "lava_bomb", 1);
    sd_reg(b_anvil, "cynder", "made", "iron", "anvil", 0);
    sd_reg(b_crucible, "cynder", "made", "iron", "crucible", 0);
    sd_reg(b_ingots, "cynder", "made", "iron", "ingots", 0);
    sd_reg(b_obsidian_blades, "cynder", "debris", "obsidian", "obsidian_blades", 0);
    sd_reg(b_charred_hand, "cynder", "made", "ash", "charred_hand", 0);
    sd_reg(b_horn_spur, "cynder", "growth", "tooth", "horn_spur", 0);
    sd_reg(b_slag_heap, "cynder", "debris", "ash", "slag_heap", 0);
    sd_reg(b_forge_bellows, "cynder", "made", "leather", "forge_bellows", 0);
    sd_reg(b_ember_moat, "cynder", "marks", "lava", "ember_moat", 1);
    sd_reg(b_sulfur_crust, "cynder", "debris", "ember", "sulfur_crust", 0);
    sd_reg(b_shackles, "cynder", "made", "iron", "shackles", 0);
    sd_reg(b_ash_drift, "cynder", "debris", "ash", "ash_drift", 0);
    sd_reg(b_imp_idol, "cynder", "made", "basalt", "imp_idol", 0);
    sd_reg(b_crust_cracks, "cynder", "marks", "lava", "crust_cracks", 0);
    sd_reg(b_quench_trough, "cynder", "made", "basalt", "quench_trough", 0);
    sd_reg(b_fallen_icicles, "targa", "debris", "ice", "fallen_icicles", 0);
    sd_reg(b_snow_crescent, "targa", "growth", "snow", "snow_crescent", 0);
    sd_reg(b_pack_and_snowshoes, "targa", "made", "cloth", "pack_and_snowshoes", 0);
    sd_reg(b_pylon_stub, "targa", "made", "alien", "pylon_stub", 0);
    sd_reg(b_ice_blocks, "targa", "made", "ice", "ice_blocks", 0);
    sd_reg(b_pod_lid, "targa", "made", "alien", "pod_lid", 0);
    sd_reg(b_conduit_node, "targa", "made", "alien", "conduit_node", 1);
    sd_reg(b_disc_wreck, "targa", "made", "alien", "disc_wreck", 0);
    sd_reg(b_pressure_ridge, "targa", "debris", "ice", "pressure_ridge", 0);
    sd_reg(b_glyph_tile, "targa", "made", "alien", "glyph_tile", 0);
    sd_reg(b_frost_flowers, "targa", "growth", "frost", "frost_flowers", 0);
    sd_reg(b_fishing_hole, "targa", "made", "ice", "fishing_hole", 1);
    sd_reg(b_thermal_grille, "targa", "made", "alien", "thermal_grille", 1);
    sd_reg(b_sled, "targa", "made", "wood", "sled", 0);
    sd_reg(b_tusk, "targa", "made", "bone", "tusk", 0);
    sd_reg(b_survey_beacon, "targa", "made", "metal", "survey_beacon", 0);
    sd_reg(b_lattice_shard, "targa", "debris", "crystal", "lattice_shard", 0);
    sd_reg(b_buried_steps, "targa", "made", "alien", "buried_steps", 0);
    sd_reg(b_ice_spikes, "targa", "debris", "ice", "ice_spikes", 0);
    sd_reg(b_power_cell, "targa", "made", "alien", "power_cell", 0);
}
