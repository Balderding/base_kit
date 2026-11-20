/* bld_sd_d.c: the style-only details of sd_d.py (trederra, fortress, beast, frontier, graveyard) in C, and the
   registration of sd_d.py + sd_e.py (bld_register_sd_de; sd_e's builders are in bld_sd_e.c). Agent "bld-sdde",
   2026-11-19. Each builder copies the Python `_` of the same style and name line by line (rules: bld_sdde.h); a
   builder that a later file draws again (sd_r_trederra.py, sd_r_x_*.py) keeps only its place (bsdde_replaced).
   Proof: work\cscene\bld_sdde\prove.py. */
#include "bld_sdde.h"

/* ====================================================================================================== trederra */
/* trederra boot_puddles: boot prints full of water */
BLD(b_trederra_boot_puddles) {
    SoFrame ff = frame(cx, cy, a); int n = ri(3, 6); GL *gs = gl(); GG *f;
    for (int i = 0; i < n; i++) {
        P2 c = F(&ff, (i - (n - 1) / 2.0) * 1.9, 0.75 * (i % 2 ? 1 : -1)); SoFrame f2 = frame(c.x, c.y, a + u(-0.25, 0.25)); P2 q1 = F(&f2, 0.35, 0), q2 = F(&f2, -0.75, 0);
        gl_add(gs, U(glv(2, ell(q1.x, q1.y, 0.75, 0.42, a, 10), ell(q2.x, q2.y, 0.38, 0.34, a, 8))));
    }
    f = rect(cx, cy, n * 1.9 + 1.2, 3.2, a); { GG *g = U(gs); push(lay, L_gcut(g, u(0.3, 0.45), "water")); }
    RET(f);
}

/* ====================================================================================================== fortress */
/* fortress paper_lantern */
typedef struct { double r, L; } PlC;
static int pl_body(void *c, double t, double *out) { PlC *k = (PlC *)c; *out = fabs(t) < k->L / 2 - 0.3 ? k->r * (0.72 + 0.28 * COS(t / k->L * 2.4)) + 0.07 * fpw(SIN(t * 9), 2) : k->r * 0.62; return SO_OK; }
BLD(b_fortress_paper_lantern) {
    SoFrame ff = frame(cx, cy, a); double L = u(2.8, 3.6); double r = u(0.95, 1.2); PlC pc = { r, L }; SoSolids *ps = SL(); P2 q = F(&ff, L / 2 + 0.4, 0), m = F(&ff, 0.3, 0); GG *f;
    spin(ps, cx, cy, a, L, pl_body, &pc, 1.1, 0.0, "round"); ext(ps, ann(q.x, q.y, 0.6, 0.3, 10), 0.4, 0.0);
    f = rect(m.x, m.y, L + 1.2, 2 * r + 0.3, a); push(lay, ST(ps, "paper", NOP, NULL)); RET(f);
}

/* fortress kabuto */
BLD(b_fortress_kabuto) {
    SoFrame ff = frame(cx, cy, a); double r = u(1.1, 1.35); double h = r * 1.0; GG *neck; SoSolids *ps = SL(); P2 cr, q; CL *cu = cl(); double ts[16]; int nt; GG *f;
    neck = Poly(pl_cat(plv(1, pol(cx, cy, r * 0.5, a + 3.14)), arc(cx, cy, r + 0.95, a + 1.3, a + 5.0, 14)));
    ext(ps, neck, 0.3, 0.0); ext(ps, isect(neck, disc(cx, cy, r + 0.5, 20)), 0.5, 0.0); dome(ps, cx, cy, r, h, 0.0, 1.0, 0.0, 20); cyl(ps, cx, cy, 0.3, h + 0.25, 0.0, 6);
    cr = F(&ff, r * 0.85, 0); q = F(&ff, r * 0.85 + 0.75, 0); ext(ps, isect(dif(disc(cr.x, cr.y, 1.25, 14), disc(q.x, q.y, 1.2, 14)), disc(cx, cy, r + 2, 20)), h * 0.55, 0.0);
    nt = arange(0.5, 6.0, 0.7, ts, 16);
    for (int i = 0; i < nt; i++) cl_add(cu, barl(pol(cx, cy, 0.4, a + ts[i]), pol(cx, cy, r, a + ts[i]), 0.2), h * 0.72);
    f = disc(cx, cy, r + 1.2, 20); push(lay, ST(ps, "lacquer", NOP, cu)); RET(f);
}

/* fortress sake_set */
BLD(b_fortress_sake_set) {
    SoFrame ff = frame(cx, cy, a); double L = u(3.6, 4.4); double W = u(2.2, 2.6); GG *R = buf(buf(rect(cx, cy, L, W, a), -0.3, 16), 0.3, 16), *f; SoSolids *ps = SL(); CL *cu = cl(); P2 q = F(&ff, -L * 0.25, 0); int n;
    ext(ps, R, 0.3, 0.0); ext(ps, dif(R, buf(R, -0.3, 16)), 0.48, 0.0);
    turn(ps, q.x, q.y, pl_cat(pl_cat(plv(2, p2(0.5, 0.3), p2(0.72, 0.9)), crv(0.72, 0.28, 0.9, 1.6, 1, 6)), plv(1, p2(0.4, 1.9))), 14, 1.0, 0.0, 0.0);
    n = ri(2, 3);
    for (int i = 0; i < n; i++) { double v = u(-0.5, 0.5); P2 c = F(&ff, L * 0.05 + i * 0.9, v); turn(ps, c.x, c.y, plv(2, p2(0.3, 0.3), p2(0.42, 0.8)), 10, 1.0, 0.0, 0.0); cl_add(cu, disc(c.x, c.y, 0.22, 6), 0.5); }
    f = buf(R, 0.2, 16); push(lay, ST(ps, "clay", NOP, cu)); RET(f);
}

/* fortress bonsai */
static int bo_trunk(void *c, double q, double *out) { (void)c; *out = 0.5 - 0.2 * q; return SO_OK; }
BLD(b_fortress_bonsai) {
    SoFrame ff = frame(cx, cy, a); double L = u(2.6, 3.2); double W = u(1.6, 2.0); double v1 = u(0.6, 1.4); double v2 = u(-0.8, 0.8); P2 e = F(&ff, v1, v2); SoSolids *ps = SL(); int n; GG *f;
    blk(ps, cx, cy, L, W, a, 0.7, 0.0); { double v = u(-0.5, 0.5); rodf(ps, plv(3, F(&ff, -L * 0.2, 0), F(&ff, 0, v), e), bo_trunk, NULL, 3.6, 0.0, 0.0); }
    n = ri(2, 4);
    for (int i = 0; i < n; i++) { double w1 = u(0, 1.0); double w2 = u(0, 6.28); P2 p = pol(e.x, e.y, w1, w2); double r = u(0.8, 1.2); double lf = u(1.5, 2.3); dome(ps, p.x, p.y, r, 0.7, lf, 1.0, 0.0, 9); }
    f = hull(glv(2, rect(cx, cy, L, W, a), disc(e.x, e.y, 2.0, 20))); push(lay, ST(ps, "shrub", NOP, NULL)); RET(f);
}

/* fortress tsukubai */
BLD(b_fortress_tsukubai) {
    double r = u(1.5, 1.9); double h = u(1.0, 1.4); P2 c = pol(cx, cy, r * 0.2, a); double v1 = u(1.6, 2.4); double v2 = u(-0.4, 0.4); P2 e = pol(cx, cy, r + v1, a + v2); SoSolids *ps = SL(); CL *cu = cl(); GG *f;
    { double sq = u(0.85, 1.0); turn(ps, cx, cy, plv(3, p2(r, 0.0), p2(r * 1.02, h * 0.5), p2(r * 0.9, h)), 11, sq, a, 0.0); }
    rod(ps, plv(2, c, e), 0.26, 1.0, h, 0.0); cyl(ps, c.x, c.y, 0.45, 0.5, h, 8);
    f = hull(glv(2, disc(cx, cy, r + 0.3, 20), disc(e.x, e.y, 0.4, 20)));
    cl_add(cu, dif(disc(cx, cy, r * 0.55, 12), buf(Line(plv(2, c, e)), 0.5, 16)), h - 0.4); push(lay, ST(ps, "garden", NOP, cu)); RET(f);
}

/* fortress deer_scarer */
static int ds_tube(void *c, double t, double *out) { double L = *(double *)c; *out = 0.42 + 0.08 * (fabs(fmodp(t + L, 1.3) - 0.65) < 0.1); return E_ ? SO_ERR : SO_OK; }
BLD(b_fortress_deer_scarer) {
    SoFrame ff = frame(cx, cy, a); double L = u(4.4, 5.6); double h = u(1.0, 1.5); SoSolids *ps = SL(); P2 rk = F(&ff, -L / 2 - 0.3, 0), m = F(&ff, -0.3, 0); GG *f;
    for (int sv = -1; sv <= 1; sv += 2) { P2 q = F(&ff, 0, sv * 0.75); cyl(ps, q.x, q.y, 0.36, h + 0.5, 0.0, 8); }
    spin(ps, cx, cy, a, L, ds_tube, &L, 1.0, h * 0.5, "round"); rock(ps, rk.x, rk.y, 0.8, 0.7, 0.0, 0.0, 9);
    f = rect(m.x, m.y, L + 1.6, 2.6, a); push(lay, ST(ps, "bamboo", NOP, NULL)); RET(f);
}

/* fortress taiko */
typedef struct { double r, L; } TkC;
static int tk_drum(void *c, double t, double *out) { TkC *k = (TkC *)c; *out = k->r * (0.86 + 0.14 * COS(t / k->L * 3.0)) + (fabs(fabs(t) - k->L / 2 + 0.25) < 0.2 ? 0.12 : 0.0); return SO_OK; }
BLD(b_fortress_taiko) {
    SoFrame ff = frame(cx, cy, a); double L = u(2.6, 3.2); double r = u(1.4, 1.7); TkC tc = { r, L }; SoSolids *ps = SL(), *st = SL(); P2 c; double b; GG *f; SoLayer *y1, *y2;
    spin(ps, cx, cy, a, L, tk_drum, &tc, 1.1, 0.0, "round");
    for (int sv = -1; sv <= 1; sv += 2) { P2 q = F(&ff, sv * L * 0.25, 0); blk(ps, q.x, q.y, 0.6, 2 * r + 0.9, a, 0.6, 0.0); }
    { double v = u(-1, 1); c = F(&ff, v, r + 1.4); } b = a + u(-0.4, 0.4); rod(st, plv(2, pol(c.x, c.y, 1.6, b), pol(c.x, c.y, 1.6, b + PI_)), 0.28, 1.1, 0.0, 0.0);
    f = hull(glv(2, rect(cx, cy, L + 0.3, 2 * r + 1.0, a), disc(c.x, c.y, 1.7, 20))); y1 = ST(ps, "wood", NOP, NULL); y2 = ST(st, "wood", NOP, NULL); push(lay, y1); push(lay, y2); RET(f);
}

/* fortress geta */
BLD(b_fortress_geta) {
    SoSolids *ps = SL(); CL *cu = cl(); GL *foot = gl(); GG *f;
    for (int sv = -1; sv <= 1; sv += 2) {
        double v1 = u(0.8, 1.4); double v2 = u(-0.3, 0.3); P2 c = pol(cx, cy, v1, a + sv * 1.57 + v2); double b = a + u(-0.6, 0.6); SoFrame f2 = frame(c.x, c.y, b); GG *R = buf(buf(rect(c.x, c.y, 2.4, 1.1, b), -0.3, 16), 0.3, 16);
        ext(ps, R, 0.75, 0.0); for (int s2 = -1; s2 <= 1; s2 += 2) cl_add(cu, barl(F(&f2, 0.9, 0), F(&f2, 0.2, s2 * 0.4), 0.22), 0.55); gl_add(foot, R);
    }
    f = buf(hull(foot), 0.2, 16); push(lay, ST(ps, "wood", NOP, cu)); RET(f);
}

/* fortress sword_stand */
BLD(b_fortress_sword_stand) {
    SoFrame ff = frame(cx, cy, a); double L = u(4.0, 5.0); double h = u(1.4, 1.9); SoSolids *ps = SL(); int n; GG *f;
    blk(ps, cx, cy, L * 0.8, 1.2, a, 0.4, 0.0);
    for (int sv = -1; sv <= 1; sv += 2) { P2 q = F(&ff, sv * L * 0.3, 0); blk(ps, q.x, q.y, 0.6, 1.3, a, h, 0.0); }
    n = ri(1, 2);
    for (int i = 0; i < n; i++) { double v = (i - 0.5) * 0.7; rod(ps, plv(3, F(&ff, -L / 2 - 0.9, v), F(&ff, 0, v + 0.25), F(&ff, L / 2 + 0.9, v)), 0.26, 1.0, h * (0.6 + 0.3 * i), 0.0); }
    f = rect(cx, cy, L + 2.0, 1.8, a); push(lay, ST(ps, "lacquer", NOP, NULL)); RET(f);
}

/* fortress go_board */
BLD(b_fortress_go_board) {
    SoFrame ff = frame(cx, cy, a); double S = u(3.0, 3.6); double h = u(0.8, 1.1); int n = 5; double ls[5]; CL *cu = cl(); SoSolids *ps = SL(); SoLayer *bw[2], *y; GG *f;
    linspace(-S / 2 + 0.3, S / 2 - 0.3, n, ls);
    for (int i = 0; i < n; i++) cl_add(cu, barl(F(&ff, ls[i], -S / 2 + 0.3), F(&ff, ls[i], S / 2 - 0.3), 0.18), h - 0.12);
    for (int i = 0; i < n; i++) cl_add(cu, barl(F(&ff, -S / 2 + 0.3, ls[i]), F(&ff, S / 2 - 0.3, ls[i]), 0.18), h - 0.12);
    blk(ps, cx, cy, S, S, a, h, 0.0);
    for (int k = 0; k < 2; k++) { int sv = k ? 1 : -1; double v = u(-0.6, 0.6); P2 c = F(&ff, sv * (S / 2 + 1.1), v); SoSolids *bs = SL(); CL *bc = cl(); turn(bs, c.x, c.y, plv(2, p2(0.55, 0.0), p2(0.8, 0.7)), 12, 1.0, 0.0, 0.0); cl_add(bc, disc(c.x, c.y, 0.5, 8), 0.4); bw[k] = ST(bs, "wood", NOP, bc); }
    f = rect(cx, cy, S + 4.0, S + 0.2, a); y = ST(ps, "wood", NOP, cu); push(lay, y); push(lay, bw[0]); push(lay, bw[1]); RET(f);
}

/* fortress fan */
BLD(b_fortress_fan) {
    double R = u(2.6, 3.4); double span = u(1.6, 2.6); int n = ri(7, 10); GG *g = dif(Poly(pl_cat(plv(1, p2(cx, cy)), arc(cx, cy, R, a - span / 2, a + span / 2, 18))), disc(cx, cy, 0.5, 20)), *f; CL *cu = cl(); SoSolids *ps = SL();
    for (int k = 1; k < n; k++) cl_add(cu, barl(pol(cx, cy, R * 0.3, a - span / 2 + span * k / n), pol(cx, cy, R, a - span / 2 + span * k / n), 0.2), 0.2);
    f = buf(g, 0.2, 16); ext(ps, g, 0.34, 0.0); cyl(ps, cx, cy, 0.36, 0.5, 0.0, 8); push(lay, ST(ps, "paper", NOP, cu)); RET(f);
}

/* fortress shuriken */
BLD(b_fortress_shuriken) {
    GL *gs = gl(); int n = ri(2, 4); GG *g, *f;
    for (int i = 0; i < n; i++) {
        double v = u(0.4, 2.4); P2 p = pol(cx, cy, v, a + i * 1.7);
        if (rnd() < 0.6) { double ro = u(1.05, 1.3); int k = rchi(4, 4, 4, 5, 6); double t = u(0, 1); gl_add(gs, dif(buf(star(p.x, p.y, ro, 0.55, k, t), 0.06, 16), disc(p.x, p.y, 0.2, 8))); }
        else { double b = u(0, 6.28); P2 q = pol(p.x, p.y, 1.45, b + PI_); gl_add(gs, U(glv(3, tri(pol(p.x, p.y, 0.2, b), pol(p.x, p.y, 1.9, b), 0.7), barl(pol(p.x, p.y, 0.3, b), pol(p.x, p.y, 1.1, b + PI_), 0.42), ann(q.x, q.y, 0.46, 0.22, 8)))); }
    }
    g = U(gs); f = buf(chull(g), 0.2, 16); { SoSolids *ps = SL(); ext(ps, g, 0.42, 0.0); push(lay, ST(ps, "iron", NOP, NULL)); }
    RET(f);
}

/* fortress shamisen */
BLD(b_fortress_shamisen) {
    SoFrame ff = frame(cx, cy, a); double L = u(6.5, 8.0); double S = u(1.8, 2.2); P2 c = F(&ff, -L / 2 + S / 2, 0); SoSolids *ps = SL(); CL *cu = cl(); static const int KS[3][2] = { { 0, 1 }, { 1, -1 }, { 2, 1 } }; double vs[3] = { -0.25, 0.0, 0.25 }; GG *f;
    ext(ps, buf(buf(rect(c.x, c.y, S, S, a), -0.4, 16), 0.4, 16), 0.7, 0.0); rod(ps, plv(2, F(&ff, -L / 2 + S, 0), F(&ff, L / 2, 0)), 0.3, 1.2, 0.0, 0.0);
    for (int i = 0; i < 3; i++) { int k = KS[i][0], sv = KS[i][1]; rod(ps, plv(2, F(&ff, L / 2 - 0.4 - k * 0.5, 0), F(&ff, L / 2 - 0.4 - k * 0.5, sv * 0.9)), 0.22, 1.0, 0.0, 0.0); }
    f = rect(cx, cy, L + 0.2, S + 0.3, a);
    for (int i = 0; i < 3; i++) cl_add(cu, barl(F(&ff, -L / 2 + 0.4, vs[i]), F(&ff, -L / 2 + S - 0.3, vs[i]), 0.16), 0.56);
    push(lay, ST(ps, "wood", NOP, cu)); RET(f);
}

/* fortress raked_gravel */
BLD(b_fortress_raked_gravel) {
    double r = u(0.8, 1.1); int n = ri(3, 5); double gap = u(0.6, 1.4); GL *gs = gl(); GG *f, *g; SoLayer *y1;
    for (int k = 0; k < n; k++) gl_add(gs, buf(Line(arc(cx, cy, r + 0.7 + k * 0.75, a + gap / 2, a + 6.2832 - gap / 2, 30)), 0.2, 4));
    f = disc(cx, cy, r + 0.9 + n * 0.75, 20); g = U(gs); y1 = L_gcut(g, 0.25, "pan");
    { double h = u(0.8, 1.3); SoSolids *ps = SL(); rock(ps, cx, cy, r, h, 0.0, 0.0, 9); push(lay, y1); push(lay, ST(ps, "garden", NOP, NULL)); }
    RET(f);
}

/* fortress koro */
BLD(b_fortress_koro) {
    double r = u(1.0, 1.25); double h = u(0.9, 1.2); SoSolids *ps = SL(); CL *cu = cl(); PT *rp; GG *f;
    turn(ps, cx, cy, plv(6, p2(r * 0.75, 0.0), p2(r, h * 0.5), p2(r * 0.92, h), p2(r * 0.6, h + 0.3), p2(0.3, h + 0.4), p2(0.3, h + 0.7)), 16, 1.0, 0.0, 0.0);
    rp = ringpts(cx, cy, r * 0.95, 3, a, 0.0, 0); for (int i = 0; i < rp->n; i++) { P2 p = pl_at(rp, i); dome(ps, p.x, p.y, 0.36, 0.5, 0.0, 1.0, 0.0, 8); }
    rp = ringpts(cx, cy, r * 0.5, 5, a + 0.3, 0.0, 0); for (int i = 0; i < rp->n; i++) { P2 p = pl_at(rp, i); cl_add(cu, disc(p.x, p.y, 0.18, 6), h * 0.8); }
    f = disc(cx, cy, r + 0.5, 20); push(lay, ST(ps, "brass", NOP, cu)); RET(f);
}

/* fortress suzu_bell */
static int sb_cord(void *c, double q, double *out) { (void)c; *out = 0.36 + 0.08 * SIN(q * 50); return SO_OK; }
BLD(b_fortress_suzu_bell) {
    double r = u(0.9, 1.15); double v1 = u(3.0, 4.5); double v2 = u(-0.3, 0.3); P2 e = pol(cx, cy, r + v1, a + v2); PT *pts = wiggle(pol(cx, cy, r * 0.6, a), e, 0.8, 6); SoSolids *ps = SL(); CL *cu = cl(); GG *f;
    dome(ps, cx, cy, r, r * 1.3, 0.0, 1.0, 0.0, 20); ext(ps, ann(cx, cy, r + 0.12, r - 0.3, 16), r * 0.55, 0.0); rodf(ps, pts, sb_cord, NULL, 1.0, 0.0, 0.0);
    f = hull(glv(2, disc(cx, cy, r + 0.3, 20), disc(e.x, e.y, 0.5, 20)));
    cl_add(cu, barl(pol(cx, cy, r * 0.7, a + 1.57), pol(cx, cy, r * 0.7, a - 1.57), 0.22), r * 0.6); push(lay, ST(ps, "brass", NOP, cu)); RET(f);
}

/* fortress menpo */
BLD(b_fortress_menpo) {
    SoFrame ff = frame(cx, cy, a); double r = u(1.2, 1.5); double h = r * 0.75; SoSolids *ps = SL(); CL *cu = cl(); P2 q = F(&ff, r * 0.1, 0); double vs[3] = { -0.25, 0, 0.25 }; GG *f;
    dome(ps, cx, cy, r, h, 0.0, 0.82, a, 20); ridge(ps, q.x, q.y, a, r * 0.8, 0.5, h + 0.3, 0.0);
    for (int sv = -1; sv <= 1; sv += 2) { double v = u(0.6, 1.4); rod(ps, plv(2, F(&ff, -r * 0.3, sv * r * 0.6), F(&ff, -r * 0.8, sv * (r + v))), 0.22, 1.0, 0.0, 0.0); }
    for (int sv = -1; sv <= 1; sv += 2) { P2 e = F(&ff, -r * 0.3, sv * r * 0.36); cl_add(cu, ell(e.x, e.y, r * 0.22, r * 0.13, a + sv * 0.3, 8), h * 0.4); }
    { P2 m = F(&ff, r * 0.6, 0); cl_add(cu, rect(m.x, m.y, 0.26, r * 0.7, a), h * 0.3); }
    for (int i = 0; i < 3; i++) { P2 m = F(&ff, r * 0.6, vs[i]); cl_add(cu, rect(m.x, m.y, 0.5, 0.16, a), h * 0.2); }
    f = disc(cx, cy, r + 1.4, 20); push(lay, ST(ps, "lacquer", NOP, cu)); RET(f);
}

/* ====================================================================================================== beast */
/* beast molar */
static int mo_crown(void *c, double t, double *out) { (void)t; *out = *(double *)c; return SO_OK; }
typedef struct { double r; } MoC;
static int mo_root(void *c, double q, double *out) { *out = ((MoC *)c)->r * 0.5 * (1 - 0.6 * q); return SO_OK; }
BLD(b_beast_molar) {
    SoFrame ff = frame(cx, cy, a); double L = u(2.8, 3.6); double r = u(0.9, 1.1); SoSolids *ps = SL(); MoC mc = { r }; P2 m = F(&ff, L * 0.2, 0); GG *f;
    spin(ps, cx, cy, a, L * 0.55, mo_crown, &r, 1.0, 0.0, "round");
    for (int sx = -1; sx <= 1; sx += 2) for (int sy = -1; sy <= 1; sy += 2) { P2 c = F(&ff, sx * L * 0.14 - L * 0.0, sy * r * 0.45); dome(ps, c.x, c.y, r * 0.5, r * 1.25, 0.0, 1.0, 0.0, 8); }
    for (int sv = -1; sv <= 1; sv += 2) { double v = u(0.1, 0.9); rodf(ps, plv(3, F(&ff, L * 0.25, sv * r * 0.4), F(&ff, L * 0.5, sv * r * 0.6), F(&ff, L * 0.75, sv * r * v)), mo_root, &mc, 1.2, 0.0, 0.0); }
    f = rect(m.x, m.y, L + 0.6, 2 * r + 0.4, a); push(lay, ST(ps, "tooth", NOP, NULL)); RET(f);
}

/* beast polyps */
BLD(b_beast_polyps) {
    SoSolids *ps = SL(); GL *foot = gl(); int n = ri(3, 7); GG *f;
    for (int i = 0; i < n; i++) {
        double v1 = u(0, 2.0); double v2 = u(0, 6.28); P2 p = pol(cx, cy, v1, v2); double r = u(0.4, 0.65); double h = u(1.0, 2.4);
        turn(ps, p.x, p.y, pl_cat(plv(4, p2(r * 0.9, 0.0), p2(r * 0.6, h * 0.3), p2(r * 0.6, h * 0.6), p2(r, h * 0.75)), crv(r, 0.25, h * 0.75, h, 1, 6)), 10, 1.0, 0.0, 0.0); gl_add(foot, disc(p.x, p.y, r, 20));
    }
    f = buf(hull(foot), 0.2, 16); push(lay, ST(ps, "flesh", NOP, NULL)); RET(f);
}

/* beast gut_folds */
static int gf_gut(void *c, double q, double *out) { (void)c; *out = 0.62 * fpw(SIN(MX(0.06, MN(0.94, q)) * PI_), 0.5); return SO_OK; }
BLD(b_beast_gut_folds) {
    SoFrame ff = frame(cx, cy, a); int n = ri(3, 5); double L = u(5.0, 7.5); SoSolids *ps = SL(); double ph = u(0, 6); GG *f;
    for (int i = 0; i < n; i++) {
        double v1 = u(0.7, 1.0); double v2 = u(0.7, 1.0); double xs[14]; PT *pts = pl();
        linspace(-L / 2 * v1, L / 2 * v2, 14, xs);
        for (int j = 0; j < 14; j++) pl_add(pts, F(&ff, xs[j], (i - (n - 1) / 2.0) * 1.5 + 0.45 * SIN(xs[j] * 1.2 + ph + i)));
        rodf(ps, pts, gf_gut, NULL, 1.0, 0.0, 0.0);
    }
    f = rect(cx, cy, L, n * 1.5 + 1.0, a); push(lay, ST(ps, "flesh", NOP, NULL)); RET(f);
}

/* beast eaten_cuirass */
BLD(b_beast_eaten_cuirass) {
    SoFrame ff = frame(cx, cy, a); double r = u(1.5, 1.9); double h = r * 0.6; SoSolids *ps = SL(); CL *cu = cl(); P2 q = F(&ff, r * 0.75, 0); int n; GG *f;
    dome(ps, cx, cy, r, h, 0.0, 0.82, a, 20); ridge(ps, cx, cy, a, r * 1.5, 0.5, h + 0.1, 0.0);
    cl_add(cu, disc(q.x, q.y, r * 0.4, 10), 0.0);
    for (int sv = -1; sv <= 1; sv += 2) { P2 e = F(&ff, -r * 0.1, sv * r * 0.75); cl_add(cu, disc(e.x, e.y, r * 0.3, 8), 0.0); }
    n = ri(1, 4);
    for (int i = 0; i < n; i++) { double v1 = u(0, r * 0.5); double v2 = u(0, 6.28); P2 p = pol(cx, cy, v1, v2); double rb = u(0.3, 0.5); GG *b = blob(p.x, p.y, rb, 3); cl_add(cu, b, h * u(0.2, 0.6)); }
    f = disc(cx, cy, r + 0.2, 20); push(lay, ST(ps, "tin", NOP, cu)); RET(f);
}

/* beast parasite_worm */
typedef struct { double r; } PwC;
static int pw_body(void *c, double q, double *out) { *out = ((PwC *)c)->r * (1 - 0.3 * q) * (1 + 0.12 * SIN(q * 38)); return SO_OK; }
BLD(b_beast_parasite_worm) {
    double L = u(3.5, 5.5); P2 e = pol(cx, cy, L, a); PT *w = wiggle(pol(cx, cy, 0.9, a), e, 0.9, 5), *pts = pl_cat(plv(1, p2(cx, cy)), pl_slice(w, 1, w->n)); double r = u(0.5, 0.65); PwC pc = { r }; SoSolids *ps = SL(); GG *f; SoLayer *y1;
    rodf(ps, pts, pw_body, &pc, 1.5, 0.0, 0.0); dome(ps, e.x, e.y, r * 0.85, r * 1.4, 0.0, 1.0, 0.0, 20);
    f = hull(glv(2, disc(cx, cy, 1.3, 20), disc(e.x, e.y, 0.9, 20))); y1 = L_gcut(ann(cx, cy, 1.25, 0.55, 12), 0.35, "pit"); push(lay, y1); push(lay, ST(ps, "tendril", NOP, NULL)); RET(f);
}

/* beast cilia */
static int ci_hair(void *c, double q, double *out) { (void)c; *out = 0.32 - 0.1 * q; return SO_OK; }
BLD(b_beast_cilia) {
    SoSolids *ps = SL(); GL *foot = gl(); P2 lean = pol(0, 0, 0.3, a); int n = ri(9, 16); GG *f;
    for (int i = 0; i < n; i++) {
        double v1 = u(0, 2.4); double v2 = u(0, 6.28); P2 p = pol(cx, cy, v1, v2); double h = u(1.0, 2.0);
        rodf(ps, plv(2, p, p2(p.x + lean.x * h, p.y + lean.y * h)), ci_hair, NULL, 1.0, 0.0, 0.0); cone(ps, p.x, p.y, 0.32, h, 0.21, 0.0, 6, 1.0, 0.0); gl_add(foot, disc(p.x, p.y, 0.5, 20));
    }
    f = buf(hull(foot), 0.2, 16); push(lay, ST(ps, "flesh", NOP, NULL)); RET(f);
}

/* beast bile_sump */
BLD(b_beast_bile_sump) {
    double v = u(1.8, 2.4); GG *p = blob(cx, cy, v, 4), *ch, *f; P2 e; GL *pads = gl(); int n;
    v = u(4.0, 5.5); e = pol(cx, cy, v, a); { double w = u(0.5, 0.7); ch = wavy(pol(cx, cy, 1.0, a), e, 0.6, 5, w); }
    n = ri(1, 3);
    for (int i = 0; i < n; i++) {
        double v1 = u(0, 1.0); double v2 = u(0, 6.28); P2 q0 = pol(cx, cy, v1, v2); double rr = u(0.5, 0.62); GG *q = disc(q0.x, q0.y, rr, 10); int ok;
        ok = contains(inset(p, 0.3), q);
        if (ok) for (int j = 0; j < pads->n; j++) if (!(gdist(q, pads->v[j]) > 0.3)) { ok = 0; break; }
        if (ok) gl_add(pads, q);
    }
    f = hull(glv(2, p, ch)); push(lay, L_gcut(U(glv(2, p, ch)), 0.45, "bile"));
    if (pads->n) push(lay, L_pad3(U(pads), "pustule"));
    RET(f);
}

/* beast gallstones */
BLD(b_beast_gallstones) {
    SoSolids *ps = SL(); GL *foot = gl(); int n = ri(5, 9); GG *f;
    for (int i = 0; i < n; i++) {
        double v1 = u(0, 1.6); double v2 = u(0, 6.28); P2 p = pol(cx, cy, v1, v2); double r = u(0.45, 0.8); double lift = MX(0.0, 0.8 - fdist(p, p2(cx, cy)) * 0.6);
        rock(ps, p.x, p.y, r, r * 1.1, 0.0, lift, 6); gl_add(foot, disc(p.x, p.y, r, 20));
    }
    f = buf(hull(foot), 0.2, 16); push(lay, ST(ps, "tooth", NOP, NULL)); RET(f);
}

/* beast vertebrae */
static int vt_rib(void *c, double q, double *out) { *out = 0.3 * *(double *)c * (1 - 0.3 * q); return SO_OK; }
BLD(b_beast_vertebrae) {
    int n = ri(4, 7); double R0 = u(4.0, 9.0); double R = R0 * rchi(2, -1, 1); SoSolids *ps = SL(); GL *foot = gl(); double k = u(0.85, 1.1); double x = cx, y = cy, t = a; GG *f;
    for (int i = 0; i < n; i++) {
        SoFrame f2 = frame(x, y, t); P2 m = F(&f2, 0, 0), nx;
        dome(ps, x, y, 0.6 * k, 0.7 * k, 0.0, 1.0, 0.0, 10);
        for (int sv = -1; sv <= 1; sv += 2) { double v = u(-0.2, 0.2); rodf(ps, plv(2, F(&f2, 0, sv * 0.3), F(&f2, v, sv * 1.3 * k)), vt_rib, &k, 1.0, 0.0, 0.0); }
        cone(ps, m.x, m.y, 0.34 * k, 1.2 * k, 0.21, 0.0, 6, 1.0, 0.0);
        gl_add(foot, disc(x, y, 1.4 * k, 20)); nx = F(&f2, 1.25 * k + (rnd() < 0.15 ? 0.6 : 0), 0); x = nx.x; y = nx.y; t += 1.25 / R;
    }
    f = hull(foot); push(lay, ST(ps, "bone", NOP, NULL)); RET(f);
}

/* beast fish_bones */
static int fb_head(void *c, double t, double *out) { (void)c; *out = 0.62 - 0.4 * (t + 0.7) / 1.4; return SO_OK; }
BLD(b_beast_fish_bones) {
    SoFrame ff = frame(cx, cy, a); double L = u(4.6, 6.2); SoSolids *ps = SL(); P2 hd = F(&ff, L / 2 - 0.5, 0), e = F(&ff, L / 2 - 0.6, 0.0); double xs[64]; int nx; GG *tail, *f; CL *cu = cl();
    rod(ps, plv(2, F(&ff, -L / 2, 0), F(&ff, L / 2 - 1.0, 0)), 0.27, 1.0, 0.0, 0.0); spin(ps, hd.x, hd.y, a, 1.4, fb_head, NULL, 0.9, 0.0, "round");
    nx = arange(-L / 2 + 1.1, L / 2 - 1.3, 0.6, xs, 64);
    for (int i = 0; i < nx; i++) { double w = 1.25 * SIN((xs[i] + L / 2) / L * 3.0) + 0.3; for (int sv = -1; sv <= 1; sv += 2) rod(ps, plv(2, F(&ff, xs[i], 0), F(&ff, xs[i] - 0.35, sv * w)), 0.21, 1.0, 0.0, 0.0); }
    tail = Poly(plv(4, F(&ff, -L / 2 + 0.3, 0), F(&ff, -L / 2 - 0.9, 0.9), F(&ff, -L / 2 - 0.5, 0), F(&ff, -L / 2 - 0.9, -0.9)));
    f = rect(cx, cy, L + 1.2, 3.4, a);
    { SoSolids *all = SL(); cat(all, ps); ext(all, tail, 0.28, 0.0); cl_add(cu, disc(e.x, e.y, 0.2, 6), 0.2); push(lay, ST(all, "bone", NOP, cu)); }
    RET(f);
}

/* beast taste_buds */
BLD(b_beast_taste_buds) {
    SoSolids *ps = SL(); CL *cu = cl(); GL *foot = gl(); int n = ri(3, 6); GG *f;
    for (int i = 0; i < n; i++) {
        double v = i == 0 ? 0 : u(1.7, 2.5); P2 p = pol(cx, cy, v, a + i * 1.3); double r = u(0.7, 1.05); double h = u(0.6, 1.1); int ok = 1;
        for (int j = 0; j < foot->n; j++) if (!(gdist(disc(p.x, p.y, r, 20), foot->v[j]) > 0.2)) { ok = 0; break; }
        if (ok && !E_) { turn(ps, p.x, p.y, plv(3, p2(r * 0.75, 0.0), p2(r, h * 0.8), p2(r * 0.85, h)), 12, 1.0, 0.0, 0.0); cl_add(cu, disc(p.x, p.y, r * 0.4, 8), h - 0.2); gl_add(foot, disc(p.x, p.y, r, 20)); }
    }
    f = buf(hull(foot), 0.2, 16); push(lay, ST(ps, "flesh", NOP, cu)); RET(f);
}

/* beast tendon_cords */
static int tc_cord(void *c, double q, double *out) { (void)c; *out = 0.3 - 0.06 * SIN(q * PI_); return SO_OK; }
BLD(b_beast_tendon_cords) {
    SoFrame ff = frame(cx, cy, a); double L = u(5.0, 7.5); int n = ri(2, 4); SoSolids *ps = SL(); GG *f;
    for (int sv = -1; sv <= 1; sv += 2) { P2 q = F(&ff, sv * L / 2, 0); double r = u(0.9, 1.2); double h = u(0.8, 1.1); dome(ps, q.x, q.y, r, h, 0.0, 0.8, a + 1.57, 20); }
    for (int i = 0; i < n; i++) { double v = (i - (n - 1) / 2.0) * 0.62; rodf(ps, plv(3, F(&ff, -L / 2, v * 0.6), F(&ff, 0, v), F(&ff, L / 2, v * 0.6)), tc_cord, NULL, 1.3, 0.0, 0.0); }
    f = rect(cx, cy, L + 2.4, 2.6, a); push(lay, ST(ps, "vein", NOP, NULL)); RET(f);
}

/* beast embedded_eye */
static int ee_lid(void *c, double q, double *out) { (void)c; *out = 0.42 * SIN(MX(0.08, MN(0.92, q)) * PI_); return SO_OK; }
BLD(b_beast_embedded_eye) {
    SoFrame ff = frame(cx, cy, a); double r = u(1.1, 1.5); double v1 = u(0.0, 0.3); double v2 = u(0, 6.28); P2 look = pol(cx, cy, r * v1, v2); SoSolids *ps = SL(); CL *cu = cl(); GG *f;
    dome(ps, cx, cy, r, r * 0.8, 0.0, 1.0, 0.0, 20);
    for (int sv = -1; sv <= 1; sv += 2) rodf(ps, plv(3, F(&ff, -r * 1.3, 0), F(&ff, 0, sv * r * 1.05), F(&ff, r * 1.3, 0)), ee_lid, NULL, 1.3, 0.0, 0.0);
    cl_add(cu, ann(look.x, look.y, r * 0.5, r * 0.5 - 0.2, 12), r * 0.62); cl_add(cu, disc(look.x, look.y, r * 0.2, 8), r * 0.5);
    f = rect(cx, cy, r * 2.9, r * 2.6, a); push(lay, ST(ps, "flesh", NOP, cu)); RET(f);
}

/* beast ulcer */
BLD(b_beast_ulcer) {
    double v = u(1.3, 1.8); GG *p = blob(cx, cy, v, 4), *ex, *f; SoSolids *ps = SL(); int n;
    ex = exter(buf(p, 0.95, 16)); n = ri(6, 9);
    for (int i = 0; i < n; i++) { double w = u(-0.2, 0.2); GG *q = interp(ex, fmodp((i + w) / n, 1.0), 1); double r = u(0.45, 0.62); double h = u(0.4, 0.7); dome(ps, gx(q), gy(q), r, h, 0.0, 1.0, 0.0, 8); }
    f = buf(p, 1.8, 16); push(lay, L_gcut(p, u(0.5, 0.8), "pit")); push(lay, ST(ps, "flesh", NOP, NULL)); RET(f);
}

/* beast dissolving_barrel */
static int db_stave(void *c, double q, double *out) { (void)c; *out = 0.36 + 0.1 * COS(q * 1.4); return SO_OK; }
BLD(b_beast_dissolving_barrel) {
    int n = ri(6, 9); double R = u(2.2, 2.9); SoSolids *ps = SL(); GG *f;
    ext(ps, ann(cx, cy, 1.0, 0.6, 14), 0.5, 0.0);
    for (int k = 0; k < n; k++) {
        double t; P2 c;
        if (rnd() < 0.2) continue;
        t = a + 6.2832 * k / n + u(-0.12, 0.12); c = pol(cx, cy, R * 0.6, t); { double L = R * u(0.75, 1.0); spin(ps, c.x, c.y, t, L, db_stave, NULL, 0.9, 0.0, "ridge"); }
    }
    f = disc(cx, cy, R + 0.4, 20); push(lay, ST(ps, "wood", NOP, NULL)); RET(f);
}

/* beast nerve_cluster */
static int nc_nerve(void *c, double q, double *out) { *out = *(double *)c * (1 - 0.3 * q); return SO_OK; }
static void nc_br(SoSolids *ps, GL *foot, P2 p, double t, double L, double r, int dep) {
    P2 e = pol(p.x, p.y, L, t); PT *w = wiggle(p, e, 0.3, 3);
    rodf(ps, w, nc_nerve, &r, 1.1, 0.0, 0.0); gl_add(foot, disc(e.x, e.y, 0.5, 20));
    if (dep < 2) { int m = ri(1, 2); for (int j = 0; j < m && !E_; j++) { int sv = j == 0 ? -1 : 1; double t2 = t + sv * u(0.4, 0.9); nc_br(ps, foot, e, t2, L * 0.7, r * 0.75, dep + 1); } }
    else dome(ps, e.x, e.y, 0.36, 0.5, 0.0, 1.0, 0.0, 8);
}
BLD(b_beast_nerve_cluster) {
    SoSolids *ps = SL(); GL *foot = gl(); int n; GG *f;
    dome(ps, cx, cy, 0.8, 0.9, 0.0, 1.0, 0.0, 20); gl_add(foot, disc(cx, cy, 0.9, 20));
    n = ri(3, 4);
    for (int k = 0; k < n; k++) { double t = a + k * 1.9 + u(-0.3, 0.3); double L = u(1.6, 2.2); nc_br(ps, foot, pol(cx, cy, 0.4, a + k * 1.9), t, L, 0.38, 0); }
    f = hull(foot); push(lay, ST(ps, "vein", NOP, NULL)); RET(f);
}

/* beast bone_spurs */
static int bs_spur(void *c, double q, double *out) { *out = *(double *)c * (1 - 0.68 * q); return SO_OK; }
BLD(b_beast_bone_spurs) {
    SoFrame ff = frame(cx, cy, a); double gap = u(1.2, 2.2); SoSolids *ps = SL(); GG *f; double r0s[2];
    for (int k = 0; k < 2; k++) {
        int sv = k ? 1 : -1; double v1, v2, v3; P2 q;
        r0s[k] = u(0.6, 0.8); v1 = u(-0.5, 0.5); v2 = u(-0.3, 0.3); v3 = u(-0.3, 0.3);
        rodf(ps, plv(3, F(&ff, sv * (gap + 2.2), v1), F(&ff, sv * (gap + 1.0), v2), F(&ff, sv * gap * 0.3, v3)), bs_spur, &r0s[k], 1.0 + 0.0, 0.0, 0.0);
        q = F(&ff, sv * (gap + 2.0), 0); { double h = u(1.6, 2.6); cone(ps, q.x, q.y, r0s[k], h, 0.22, 0.0, 8, 1.0, 0.0); }
    }
    f = rect(cx, cy, 2 * gap + 5.4, 2.4, a); push(lay, ST(ps, "bone", NOP, NULL)); RET(f);
}

/* beast lamprey_mouth */
BLD(b_beast_lamprey_mouth) {
    double R = u(1.9, 2.5); SoSolids *ps = SL(); double qs[2] = { 0.86, 0.58 }; int ns[2]; GG *f;
    ext(ps, ann(cx, cy, R, R - 0.6, 18), 0.5, 0.0);
    ns[0] = ri(9, 12); ns[1] = ri(6, 8);
    for (int j = 0; j < 2; j++) { PT *rp = ringpts(cx, cy, R * qs[j], ns[j], a + qs[j], 0.0, 0); for (int i = 0; i < rp->n; i++) { P2 p = pl_at(rp, i); double h = u(0.7, 1.1); cone(ps, p.x, p.y, 0.3, h, 0.21, 0.0, 6, 1.0, 0.0); } }
    f = disc(cx, cy, R + 0.2, 20); push(lay, L_gcut(disc(cx, cy, R * 0.34, 10), 0.8, "pit")); push(lay, ST(ps, "tooth", NOP, NULL)); RET(f);
}

/* beast split_cyst */
typedef struct { double r, L; } SpC;
static int sp_cyst(void *c, double t, double *out) { SpC *k = (SpC *)c; *out = k->r * fsq(MX(0.0, 1 - fpw(2 * t / k->L, 2))) + 0.05; return E_ ? SO_ERR : SO_OK; }
BLD(b_beast_split_cyst) {
    SoFrame ff = frame(cx, cy, a); double L = u(2.8, 3.6); double r = u(1.1, 1.4); GG *p, *zz, *f; PT *zp = pl(); SpC sc = { r, L }; SoSolids *ps = SL(); CL *cu = cl();
    { double v = u(-0.5, 0.5); P2 q = F(&ff, v, r + 1.2); double rb = u(0.9, 1.3); p = blob(q.x, q.y, rb, 3); }
    for (int i = 0; i < 7; i++) pl_add(zp, F(&ff, -L * 0.4 + i * L * 0.8 / 6, 0.3 * (i % 2 ? 1 : -1)));
    zz = line(zp, 0.34);
    f = hull(glv(2, rect(cx, cy, L, 2 * r, a), p));
    spin(ps, cx, cy, a, L, sp_cyst, &sc, 0.9, 0.0, "round"); cl_add(cu, zz, r * 0.4); push(lay, ST(ps, "pustule", NOP, cu)); push(lay, L_gcut(p, 0.25, "bile"));
    RET(f);
}

/* beast barnacles */
BLD(b_beast_barnacles) {
    SoSolids *ps = SL(); CL *cu = cl(); GL *foot = gl(); int n = ri(4, 8); GG *f;
    for (int i = 0; i < n; i++) {
        double v1 = u(0, 1.9); double v2 = u(0, 6.28); P2 p = pol(cx, cy, v1, v2); double r = u(0.5, 0.9); double h = r * u(0.9, 1.3); int ok = 1;
        for (int j = 0; j < foot->n; j++) { GG *c = cent(foot->v[j]); if (!(fdist(p, p2(gx(c), gy(c))) > 0.75)) { ok = 0; break; } }
        if (ok && !E_) { double t = u(0, 1); turn(ps, p.x, p.y, plv(2, p2(r, 0.0), p2(r * 0.5, h)), 7, 1.0, t, 0.0); cl_add(cu, disc(p.x, p.y, r * 0.26, 6), h - 0.35); gl_add(foot, disc(p.x, p.y, r, 20)); }
    }
    f = buf(hull(foot), 0.2, 16); push(lay, ST(ps, "tooth", NOP, cu)); RET(f);
}

/* ====================================================================================================== frontier */
/* frontier spittoon */
BLD(b_frontier_spittoon) {
    double r = u(1.0, 1.25); double h = u(1.1, 1.5); GG *p, *f; SoSolids *ps = SL(); CL *cu = cl();
    { P2 q = pol(cx, cy, r + 1.0, a); double rb = u(0.6, 0.9); p = blob(q.x, q.y, rb, 3); }
    turn(ps, cx, cy, plv(4, p2(r * 0.8, 0.0), p2(r, h * 0.35), p2(r * 0.55, h * 0.7), p2(r * 1.05, h)), 16, 1.0, 0.0, 0.0);
    f = hull(glv(2, disc(cx, cy, r + 0.3, 20), p)); cl_add(cu, disc(cx, cy, r * 0.5, 10), h - 0.5); push(lay, ST(ps, "brass", NOP, cu));
    if (rnd() < 0.6) push(lay, L_gcut(p, 0.2, "tar"));
    RET(f);
}

/* frontier poker_hand */
BLD(b_frontier_poker_hand) {
    SoSolids *ps = SL(); GL *foot = gl(); int n = ri(3, 6), m; GG *f;
    for (int i = 0; i < n; i++) { double v = u(0.2, 1.6); P2 c = pol(cx, cy, v, a + i * 0.5); GG *q = rect(c.x, c.y, 1.5, 1.0, a + i * 0.35 + u(-0.2, 0.2)); ext(ps, q, 0.22 + 0.05 * i, 0.0); gl_add(foot, q); }
    m = ri(2, 4);
    for (int i = 0; i < m; i++) { double v = u(2.2, 3.0); P2 c = pol(cx, cy, v, a + 2.5 + i * 0.8); int k = ri(1, 4); cyl(ps, c.x, c.y, 0.5, 0.25 * k, 0.0, 10); gl_add(foot, disc(c.x, c.y, 0.6, 20)); }
    f = buf(hull(foot), 0.2, 16); push(lay, ST(ps, "paper", NOP, NULL)); RET(f);
}

/* frontier wanted_board */
BLD(b_frontier_wanted_board) {
    SoFrame ff = frame(cx, cy, a); double L = u(3.6, 4.6); double W = u(2.8, 3.4); GG *R = rect(cx, cy, L, W, a), *P, *f, *pc; SoSolids *ps = SL(); CL *cu = cl(); double vs[2];
    { double v1 = u(-0.3, 0.3); double v2 = u(-0.2, 0.2); P2 q = F(&ff, v1, v2); P = rect(q.x, q.y, L * 0.55, W * 0.72, a + u(-0.15, 0.15)); }
    ext(ps, R, 0.45, 0.0); ext(ps, P, 0.62, 0.0);
    vs[0] = -W / 6; vs[1] = W / 6;
    for (int i = 0; i < 2; i++) { P2 q = F(&ff, 0, vs[i]); GG *c = dif(rect(q.x, q.y, L + 0.2, 0.18, a), buf(P, 0.1, 16)); if (!empty(c)) cl_add(cu, c, 0.3); }
    pc = cent(P); { GG *e = ell(gx(pc), gy(pc), L * 0.12, W * 0.16, a, 10); if (!empty(e)) cl_add(cu, e, 0.5); }
    f = buf(R, 0.2, 16); push(lay, ST(ps, "board", NOP, cu)); RET(f);
}

/* frontier washtub */
BLD(b_frontier_washtub) {
    double r = u(1.5, 1.9); double h = u(1.0, 1.3); SoFrame ff = frame(cx, cy, a); double v = u(-0.6, 0.6); P2 c = F(&ff, r + 1.3, v); double b = a + u(-0.4, 0.4); SoFrame f2 = frame(c.x, c.y, b); SoSolids *tub = SL(), *wb = SL(); GG *f; CL *c1 = cl(), *c2 = cl(); double vs[16]; int nv;
    turn(tub, cx, cy, plv(4, p2(r * 0.85, 0.0), p2(r, h), p2(r + 0.12, h + 0.02), p2(r + 0.12, h + 0.14)), 20, 1.0, 0.0, 0.0);
    for (int sv = -1; sv <= 1; sv += 2) { P2 q = F(&ff, 0, sv * (r + 0.2)); blk(tub, q.x, q.y, 0.9, 0.5, a, h * 0.8, 0.0); }
    ext(wb, rect(c.x, c.y, 1.5, 2.8, b), 0.42, 0.0);
    f = hull(glv(2, disc(cx, cy, r + 0.6, 20), rect(c.x, c.y, 1.6, 2.9, b)));
    { GG *d = disc(cx, cy, r - 0.35, 16); cl_add(c1, d, h - u(0.3, 0.6)); } push(lay, ST(tub, "tin", NOP, c1));
    nv = arange(-0.9, 0.95, 0.42, vs, 16); for (int i = 0; i < nv; i++) { P2 q = F(&f2, 0, vs[i]); cl_add(c2, rect(q.x, q.y, 1.0, 0.2, b), 0.2); } push(lay, ST(wb, "board", NOP, c2));
    RET(f);
}

/* frontier branding_irons */
BLD(b_frontier_branding_irons) {
    SoSolids *ps = SL(); GL *foot = gl(); int n = ri(2, 3); GG *f;
    for (int i = 0; i < n; i++) {
        double v1 = u(-0.6, 0.6); double v2 = u(-0.3, 0.3); SoFrame f2 = frame(cx + v1, cy + (i - 1) * 1.6, a + v2); double L = u(5.0, 6.5); P2 e = F(&f2, L / 2 + 0.6, 0); int k = ri(0, 3); GG *mark; P2 an = F(&f2, -L / 2 - 0.4, 0), m = F(&f2, 0.2, 0);
        if (k == 0) mark = ann(e.x, e.y, 0.72, 0.36, 10);
        else if (k == 1) mark = buf(exter(ngon(e.x, e.y, 0.8, 3, a)), 0.2, 16);
        else if (k == 2) mark = U(glv(3, barl(F(&f2, L / 2, -0.5), F(&f2, L / 2 + 1.2, -0.5), 0.36), barl(F(&f2, L / 2, 0.5), F(&f2, L / 2 + 1.2, 0.5), 0.36), barl(F(&f2, L / 2 + 0.6, -0.5), F(&f2, L / 2 + 0.6, 0.5), 0.36)));
        else mark = star(e.x, e.y, 0.85, 0.36, 5, a);
        rod(ps, plv(2, F(&f2, -L / 2, 0), F(&f2, L / 2, 0)), 0.25, 1.1, 0.0, 0.0); ext(ps, mark, 0.5, 0.0); ext(ps, ann(an.x, an.y, 0.5, 0.22, 8), 0.4, 0.0); gl_add(foot, rect(m.x, m.y, L + 2.6, 1.8, a));
    }
    f = hull(foot); push(lay, ST(ps, "iron", NOP, NULL)); RET(f);
}

/* frontier butter_churn */
typedef struct { double r, L; } BcC;
static int bc_churn(void *c, double t, double *out) { BcC *k = (BcC *)c; *out = k->r * (1 - 0.25 * (t + k->L / 2) / k->L) + (fabs(fabs(t) - k->L * 0.25) < 0.15 ? 0.1 : 0.0); return SO_OK; }
BLD(b_frontier_butter_churn) {
    double r = u(0.95, 1.15); double h = u(2.2, 2.8); SoSolids *ps = SL(); GG *f;
    if (rnd() < 0.35) {
        SoFrame ff = frame(cx, cy, a); double L = h; BcC bc = { r, L }; P2 m = F(&ff, 1.0, 0); double v;
        spin(ps, cx, cy, a, L, bc_churn, &bc, 1.1, 0.0, "round"); v = u(-0.6, 0.6); rod(ps, plv(2, F(&ff, L / 2, 0), F(&ff, L / 2 + 2.2, v)), 0.25, 1.1, 0.0, 0.0);
        f = rect(m.x, m.y, L + 2.6, 2 * r + 0.4, a); push(lay, ST(ps, "wood", NOP, NULL)); RET(f);
    }
    turn(ps, cx, cy, plv(7, p2(r, 0.0), p2(r + 0.1, 0.3), p2(r, 0.5), p2(r * 0.78, h * 0.6), p2(r * 0.78 + 0.1, h * 0.7), p2(r * 0.72, h), p2(r * 0.5, h + 0.15)), 14, 1.0, 0.0, 0.0);
    { double v = u(0.8, 1.4); cyl(ps, cx, cy, 0.28, h + v, 0.0, 6); }
    f = disc(cx, cy, r + 0.3, 20); push(lay, ST(ps, "wood", NOP, NULL)); RET(f);
}

/* frontier boots_and_spurs */
BLD(b_frontier_boots_and_spurs) {
    SoLayer *ly[4] = { NULL, NULL, NULL, NULL }; GL *foot = gl(); GG *f;
    for (int k = 0; k < 2; k++) {
        int sv = k ? 1 : -1; double v = u(1.0, 1.5); P2 c = pol(cx, cy, v, a + sv * 1.57); double b = a + u(-0.6, 0.6); SoFrame f2 = frame(c.x, c.y, b); int up_ = rnd() < 0.5; SoSolids *ps = SL(), *sp_ = SL(); P2 q1 = F(&f2, 0.5, 0), q2 = F(&f2, -0.5, 0), sp, st, m;
        loaf(ps, q1.x, q1.y, b, 1.9, 0.9, 0.55, 0.0); blk(ps, q2.x, q2.y, 0.6, 0.8, b, 0.75, 0.0);
        if (up_) { double h = u(1.8, 2.4); cyl(ps, q2.x, q2.y, 0.52, h, 0.0, 10); } else { P2 q3 = F(&f2, -1.7, 0); lie(ps, q3.x, q3.y, b, 2.2, 0.5, 0.0); }
        ly[2 * k] = ST(ps, "leather", NOP, NULL); sp = F(&f2, up_ ? -0.9 : -0.6, sv * 1.0); st = pol(sp.x, sp.y, 0.95, b + 3.14);
        ext(sp_, U(glv(2, line(arc(sp.x, sp.y, 0.5, b + 1.0, b + 5.2, 8), 0.3), star(st.x, st.y, 0.45, 0.22, 6, 0))), 0.34, 0.0); ly[2 * k + 1] = ST(sp_, "iron", NOP, NULL);
        m = F(&f2, -0.6, 0); gl_add(foot, rect(m.x, m.y, 4.6, 2.6, b));
    }
    f = hull(foot);
    if (E_) return SC_ERR;
    push(lay, ly[0]); push(lay, ly[2]);
    for (int j = 1; j <= 3; j += 2) { int ok = 1; for (int o = 0; o <= 2; o += 2) if (!(gdist(ly1(ly[j]), ly1(ly[o])) > 0.25)) { ok = 0; break; } if (ok) push(lay, ly[j]); }
    RET(f);
}

/* frontier lasso */
BLD(b_frontier_lasso) {
    double R = u(1.7, 2.3); double v1 = u(2.5, 4.5); double v2 = u(-0.5, 0.5); P2 e = pol(cx, cy, R + v1, a + v2); SoSolids *ps = SL(); double ts[28]; PT *lp = pl(); P2 q = pol(cx, cy, R, a); GG *f;
    linspace(a, a + 6.2832, 28, ts); for (int i = 0; i < 28; i++) pl_add(lp, pol(cx, cy, R * (1 + 0.08 * SIN(3 * ts[i])), ts[i]));
    rod(ps, lp, 0.3, 1.1, 0.0, 0.0); dome(ps, q.x, q.y, 0.5, 0.6, 0.0, 1.0, 0.0, 20); rod(ps, wiggle(pol(cx, cy, R, a), e, 0.8, 6), 0.28, 1.1, 0.0, 0.0);
    if (rnd() < 0.5) { double t2[22]; PT *l2 = pl(); linspace(a + 0.4, a + 6.0, 22, t2); for (int i = 0; i < 22; i++) pl_add(l2, pol(cx, cy, R * 0.72, t2[i])); rod(ps, l2, 0.3, 1.1, 0.3, 0.0); }
    f = hull(glv(2, disc(cx, cy, R + 0.4, 20), disc(e.x, e.y, 0.4, 20))); push(lay, ST(ps, "rope", NOP, NULL)); RET(f);
}

/* frontier whiskey_jug */
BLD(b_frontier_whiskey_jug) {
    double r = u(1.0, 1.25); double h = u(1.9, 2.5); SoSolids *ps = SL(); CL *cu = cl(); P2 q = pol(cx, cy, 0.75, a); int n; GG *f;
    turn(ps, cx, cy, pl_cat(pl_cat(plv(3, p2(r * 0.85, 0.0), p2(r, h * 0.3), p2(r, h * 0.55)), crv(r, 0.36, h * 0.55, h * 0.85, 1, 6)), plv(2, p2(0.36, h), p2(0.45, h + 0.1))), 16, 1.0, 0.0, 0.0);
    ext(ps, ann(q.x, q.y, 0.5, 0.22, 8), h * 0.92, 0.0);
    for (int sv = -1; sv <= 1; sv++) cl_add(cu, barl(pol(cx, cy, r * 0.3, a + 3.14 + sv * 0.6 - 0.3), pol(cx, cy, r * 0.75, a + 3.14 + sv * 0.6 + 0.3), 0.18), h * 0.5);
    push(lay, ST(ps, "clay", NOP, cu));
    n = ri(1, 3);
    for (int i = 0; i < n; i++) { double v = u(0.9, 1.6); P2 c = pol(cx, cy, r + v, a + 2.4 + i * 0.9); SoSolids *gs = SL(); CL *gc = cl(); turn(gs, c.x, c.y, plv(2, p2(0.36, 0.0), p2(0.45, 0.7)), 8, 1.0, 0.0, 0.0); cl_add(gc, disc(c.x, c.y, 0.24, 6), 0.3); push(lay, ST(gs, "glass", NOP, gc)); }
    f = disc(cx, cy, r + 2.0, 20); RET(f);
}

/* frontier gunbelt */
static int gb_belt(void *c, double q, double *out) { (void)c; *out = 0.42 + 0.06 * (SIN(q * 70) > 0.5); return SO_OK; }
static int gb_holster(void *c, double t, double *out) { (void)c; *out = 0.62 - 0.3 * (t + 1.2) / 2.4; return SO_OK; }
BLD(b_frontier_gunbelt) {
    double R = u(1.7, 2.2); double ts[24]; PT *pts = pl(); P2 hp, hq, c, p0; double b; SoSolids *ps = SL(), *ss = SL(); SoLayer *st; GG *f;
    linspace(a + 0.3, a + 5.9, 24, ts); for (int i = 0; i < 24; i++) pl_add(pts, pol(cx, cy, R * (1 + 0.18 * SIN(2 * ts[i] + 1)), ts[i]));
    hp = pl_at(pts, 12); b = a + u(0, 6.28); hq = pol(hp.x, hp.y, 0.9, b); p0 = pl_at(pts, 0);
    rodf(ps, pts, gb_belt, NULL, 0.9, 0.0, 0.0); spin(ps, hq.x, hq.y, b, 2.4, gb_holster, NULL, 0.9, 0.0, "round"); ext(ps, disc(p0.x, p0.y, 0.5, 8), 0.5, 0.0);
    c = pol(cx, cy, 0.2, a); ext(ss, uni(star(c.x, c.y, 0.85, 0.4, 5, a), disc(c.x, c.y, 0.35, 8)), 0.34, 0.0); st = ST(ss, "tin", NOP, NULL);
    f = disc(cx, cy, R + 2.0, 20); push(lay, ST(ps, "leather", NOP, NULL)); push(lay, st); RET(f);
}

/* frontier singletree */
static int stt_bar(void *c, double t, double *out) { double L = *(double *)c; *out = 0.42 + 0.1 * (t < -L / 2 + 1.0); return SO_OK; }
static int stt_cross(void *c, double t, double *out) { double W = *(double *)c; *out = 0.4 - 0.12 * fabs(t) / (W / 2); return SO_OK; }
BLD(b_frontier_singletree) {
    SoFrame ff = frame(cx, cy, a); double L = u(6.0, 8.0); double W = u(3.6, 4.4); SoSolids *ps = SL(); P2 e = F(&ff, L / 2, 0), q = F(&ff, -L / 2 - 0.4, 0), m = F(&ff, 0.1, 0); GG *f;
    spin(ps, cx, cy, a, L, stt_bar, &L, 1.1, 0.0, "round"); spin(ps, e.x, e.y, a + 1.5708, W, stt_cross, &W, 1.1, 0.0, "round");
    for (int sv = -1; sv <= 1; sv += 2) { P2 r = F(&ff, L / 2 + 0.45, sv * (W / 2 - 0.2)); ext(ps, ann(r.x, r.y, 0.5, 0.24, 8), 0.42, 0.0); }
    ext(ps, ann(q.x, q.y, 0.5, 0.24, 8), 0.42, 0.0);
    f = rect(m.x, m.y, L + 1.8, W + 0.3, a); push(lay, ST(ps, "wood", NOP, NULL)); RET(f);
}

/* frontier nail_keg */
BLD(b_frontier_nail_keg) {
    double r = u(0.9, 1.1); double h = u(1.2, 1.6); P2 c = pol(cx, cy, r + 1.6, a); double b = a + u(0.6, 2.2); SoFrame f2 = frame(c.x, c.y, b); SoSolids *ps = SL(), *ham = SL(); int n; CL *cu = cl(); GG *f; SoLayer *y1;
    turn(ps, cx, cy, plv(3, p2(r * 0.85, 0.0), p2(r, h * 0.5), p2(r * 0.88, h)), 14, 1.0, 0.0, 0.0);
    n = ri(4, 8);
    for (int i = 0; i < n; i++) { double v1 = u(0, r * 0.45); double v2 = u(0, 6.28); P2 p = pol(cx, cy, v1, v2); double hh = h + u(0.0, 0.3); cone(ps, p.x, p.y, 0.22, hh, 0.2, 0.0, 5, 1.0, 0.0); }
    { P2 m = F(&f2, 1.2, 0); rod(ham, plv(2, F(&f2, -1.6, 0), F(&f2, 1.0, 0)), 0.26, 1.1, 0.0, 0.0); blk(ham, m.x, m.y, 0.7, 1.7, b, 0.7, 0.0); }
    f = hull(glv(2, disc(cx, cy, r + 0.3, 20), disc(c.x, c.y, 1.9, 20))); cl_add(cu, ann(cx, cy, r * 0.88 - 0.2, r * 0.5, 12), h - 0.25); y1 = ST(ps, "wood", NOP, cu); push(lay, y1); push(lay, ST(ham, "iron", NOP, NULL)); RET(f);
}

/* frontier horse_collar */
static int hc_pad(void *c, double q, double *out) { (void)c; *out = 0.5 + 0.12 * COS(q * 6.2832); return SO_OK; }
BLD(b_frontier_horse_collar) {
    SoFrame ff = frame(cx, cy, a); double A = u(2.0, 2.5); double B = A * 0.66; double ts[30]; PT *pts = pl(); SoSolids *ps = SL(); GG *f;
    linspace(0, 6.2832, 30, ts); for (int i = 0; i < 30; i++) pl_add(pts, F(&ff, A * COS(ts[i]) * (1.0 + 0.12 * COS(ts[i])), B * SIN(ts[i])));
    rodf(ps, pts, hc_pad, NULL, 1.1, 0.0, 0.0);
    for (int sv = -1; sv <= 1; sv += 2) rod(ps, plv(3, F(&ff, A * 1.2, sv * 0.25), F(&ff, 0, sv * (B + 0.42)), F(&ff, -A * 1.1, sv * 0.5)), 0.22, 1.6, 0.0, 0.0);
    f = rect(cx, cy, 2 * A + 1.4, 2 * B + 1.6, a); push(lay, ST(ps, "leather", NOP, NULL)); RET(f);
}

/* frontier rut_puddles */
BLD(b_frontier_rut_puddles) {
    SoFrame ff = frame(cx, cy, a); double L = u(7.0, 10.0); GL *gs = gl(); double ph = u(0, 6); GG *f;
    for (int sv = -1; sv <= 1; sv += 2) {
        double xs[14]; PT *pts = pl(); linspace(-L / 2, L / 2, 14, xs);
        for (int i = 0; i < 14; i++) pl_add(pts, F(&ff, xs[i], sv * 1.4 + 0.3 * SIN(xs[i] * 0.7 + ph)));
        gl_add(gs, buf(Line(pts), 0.32, 4));
        for (int i = 2; i < 12; i++) if (rnd() < 0.22) { P2 p = pl_at(pts, i); double rx = u(0.8, 1.3); double ry = u(0.5, 0.7); gl_add(gs, ell(p.x, p.y, rx, ry, a, 10)); }
    }
    f = rect(cx, cy, L, 4.4, a); push(lay, L_gcut(U(gs), 0.32, "water")); RET(f);
}

/* ====================================================================================================== graveyard */
/* graveyard wreath */
static int wr_ring(void *c, double q, double *out) { int k = *(int *)c; *out = 0.42 + 0.12 * fpw(SIN(q * 6.2832 * k), 2); return SO_OK; }
static int wr_ribbon(void *c, double q, double *out) { (void)c; *out = 0.34 - 0.08 * q; return SO_OK; }
BLD(b_graveyard_wreath) {
    double R = u(1.5, 1.9); int k = ri(9, 14); P2 bow = pol(cx, cy, R, a); double ts[36]; PT *pts = pl(); SoSolids *ps = SL(); GG *f;
    linspace(a, a + 6.2832, 36, ts); for (int i = 0; i < 36; i++) pl_add(pts, pol(cx, cy, R, ts[i]));
    rodf(ps, pts, wr_ring, &k, 1.0, 0.0, 0.0);
    for (int sv = -1; sv <= 1; sv += 2) { double v1 = u(1.2, 1.8); double v2 = u(0.3, 0.8); rodf(ps, plv(2, bow, pol(bow.x, bow.y, v1, a + sv * v2)), wr_ribbon, NULL, 0.7, 0.0, 0.0); }
    dome(ps, bow.x, bow.y, 0.42, 0.6, 0.0, 1.0, 0.0, 8);
    { P2 q = pol(cx, cy, R + 1.4, a); f = chull(uni(disc(cx, cy, R + 0.5, 20), disc(q.x, q.y, 1.0, 20))); }
    push(lay, ST(ps, "leaf", NOP, NULL)); RET(f);
}

/* graveyard angel_wing */
static int aw_feather(void *c, double q, double *out) { double L = *(double *)c; *out = 0.46 * fsq(MX(0.0, 1 - fpw(2 * q / L, 2))) + 0.06; return E_ ? SO_ERR : SO_OK; }
BLD(b_graveyard_angel_wing) {
    int n = ri(5, 7); SoSolids *ps = SL(); GL *foot = gl(); double L0 = u(2.6, 3.4); GG *f;
    for (int i = 0; i < n; i++) {
        double t = a + (i - n / 2.0) * 0.24, L = L0 * (0.6 + 0.4 * SIN((i + 0.5) / n * 2.4)); P2 c = pol(cx, cy, L / 2 + 0.3, t), q = pol(cx, cy, L + 0.3, t);
        spin(ps, c.x, c.y, t, L, aw_feather, &L, 1.0, 0.07 * i, "round"); gl_add(foot, disc(q.x, q.y, 0.4, 20));
    }
    dome(ps, cx, cy, 0.8, 0.8, 0.0, 1.0, 0.0, 20); gl_add(foot, disc(cx, cy, 0.9, 20));
    f = hull(foot); push(lay, ST(ps, "headstone", NOP, NULL)); RET(f);
}

/* graveyard candles */
BLD(b_graveyard_candles) {
    SoSolids *ps = SL(); GL *foot = gl(); int n = ri(3, 6); GG *f;
    for (int i = 0; i < n; i++) {
        double v1 = u(0, 1.6); double v2 = u(0, 6.28); P2 p = pol(cx, cy, v1, v2); double r = u(0.34, 0.5); double h = u(0.6, 2.4);
        turn(ps, p.x, p.y, plv(6, p2(r * 1.9, 0.0), p2(r * 1.2, 0.2), p2(r, 0.3), p2(r, h), p2(0.2, h + 0.02), p2(0.2, h + 0.3)), 8, 1.0, 0.0, 0.0); gl_add(foot, disc(p.x, p.y, r * 2, 20));
    }
    f = buf(hull(foot), 0.2, 16); push(lay, ST(ps, "wax", NOP, NULL)); RET(f);
}

/* graveyard skull_pile */
BLD(b_graveyard_skull_pile) {
    SoSolids *ps = SL(); CL *cu = cl(); GL *foot = gl(); int n = ri(3, 6); GG *f;
    for (int i = 0; i < n; i++) {
        int top = i == n - 1 && n > 3; P2 p; double b, r, z; SoFrame f2; P2 q;
        if (top) p = p2(cx, cy); else { double v = u(0.9, 1.3); p = pol(cx, cy, v, a + 6.2832 * i / (n - (n > 3))); }
        b = u(0, 6.28); f2 = frame(p.x, p.y, b); r = u(0.62, 0.75); z = top ? 0.8 : 0.0; q = F(&f2, r * 0.8, 0);
        dome(ps, p.x, p.y, r, r * 1.25, z, 0.85, b, 20); blk(ps, q.x, q.y, 0.5, r * 0.9, b, r * 0.5, z);
        for (int sv = -1; sv <= 1; sv += 2) { P2 e = F(&f2, r * 0.45, sv * r * 0.36); cl_add(cu, disc(e.x, e.y, 0.2, 6), z + r * 0.55); }
        gl_add(foot, disc(p.x, p.y, r + 0.3, 20));
    }
    f = buf(hull(foot), 0.2, 16); push(lay, ST(ps, "bone", NOP, cu)); RET(f);
}

/* graveyard raven */
static int rv_body(void *c, double t, double *out) { double L = *(double *)c; *out = 0.62 * fsq(MX(0.0, 1 - fpw(2 * t / L, 2))) * (1 + 0.25 * t / L) + 0.06; return E_ ? SO_ERR : SO_OK; }
BLD(b_graveyard_raven) {
    SoFrame ff = frame(cx, cy, a); double L = u(2.4, 3.0); int dead = rnd() < 0.4; SoSolids *ps = SL(); P2 hd = F(&ff, L / 2 + 0.2, 0), m = F(&ff, -0.2, 0); GG *f;
    spin(ps, cx, cy, a, L, rv_body, &L, 1.2, 0.0, "round"); dome(ps, hd.x, hd.y, 0.45, !dead ? 1.2 : 0.6, 0.0, 1.0, 0.0, 20);
    { double v = u(-0.2, 0.2); ext(ps, tri(F(&ff, L / 2 + 0.5, 0), F(&ff, L / 2 + 1.4, v), 0.5), dead ? 0.5 : 0.95, 0.0); }
    ext(ps, Poly(plv(5, F(&ff, -L / 2 + 0.3, -0.3), F(&ff, -L / 2 - 1.2, -0.6), F(&ff, -L / 2 - 1.4, 0), F(&ff, -L / 2 - 1.2, 0.6), F(&ff, -L / 2 + 0.3, 0.3))), 0.34, 0.0);
    if (dead) for (int sv = -1; sv <= 1; sv += 2) ext(ps, Poly(plv(5, F(&ff, 0.5, sv * 0.3), F(&ff, 0.2, sv * 2.4), F(&ff, -0.6, sv * 2.2), F(&ff, -1.2, sv * 1.2), F(&ff, -0.6, sv * 0.3))), 0.3, 0.0);
    f = rect(m.x, m.y, L + 3.2, dead ? 5.2 : 1.8, a); push(lay, ST(ps, "hide", NOP, NULL)); RET(f);
}

/* graveyard toppled_vase */
typedef struct { double r, L; } TvC;
static int tv_vase(void *c, double t, double *out) {
    TvC *k = (TvC *)c;
    *out = t < k->L * 0.25 ? k->r * (0.5 + 0.5 * fpw(SIN(MX(0.0, MN(1.0, (t + k->L / 2) / (k->L * 0.75))) * PI_), 0.7)) : k->r * 0.45 + 0.4 * (t - k->L * 0.25); return E_ ? SO_ERR : SO_OK;
}
BLD(b_graveyard_toppled_vase) {
    SoFrame ff = frame(cx, cy, a); double L = u(2.6, 3.2); double r = u(0.75, 0.95); TvC tc = { r, L }; SoSolids *ps = SL(); SoLayer *fl[8]; int n, nf = 0; P2 m = F(&ff, 1.0, 0); GG *f;
    spin(ps, cx, cy, a, L, tv_vase, &tc, 1.15, 0.0, "round"); push(lay, ST(ps, "clay", NOP, NULL));
    n = ri(2, 5);
    for (int i = 0; i < n; i++) { double v1 = u(0.7, 2.2); double v2 = u(-1.3, 1.3); P2 c = F(&ff, L / 2 + v1, v2); double fr = u(0.45, 0.62); double fa = u(0, 1); SoSolids *fs = SL(); fluted(fs, c.x, c.y, fr, 0.36, 5, fa, 0.5, 2, 0.0, 0.0); fl[nf++] = ST(fs, "flower", NOP, NULL); }
    f = rect(m.x, m.y, L + 2.8, 3.2, a);
    if (E_) return SC_ERR;
    for (int i = 0; i < nf; i++) { int ok = 1; for (int j = 0; j < i; j++) if (!(gdist(ly1(fl[i]), ly1(fl[j])) > 0.3)) { ok = 0; break; } if (ok) push(lay, fl[i]); }
    RET(f);
}

/* graveyard ivy */
BLD(b_graveyard_ivy) {
    double L = u(5.0, 8.0); PT *pts = wiggle(pol(cx, cy, L / 2, a + 3.14), pol(cx, cy, L / 2, a), 1.2, 7); SoSolids *ps = SL(); GG *ln, *f; double qs[16]; int nq;
    rod(ps, pts, 0.25, 1.0, 0.0, 0.0); ln = Line(pts);
    nq = arange(0.06, 0.97, 0.09, qs, 16);
    for (int i = 0; i < nq; i++) {
        GG *p = interp(ln, qs[i], 1); int sv = rchi(2, -1, 1); double v = u(-0.4, 0.4); P2 c = pol(gx(p), gy(p), 0.6, a + sv * 1.57 + v); double ro = u(0.48, 0.62); double rt = u(0, 2);
        ext(ps, buf(star(c.x, c.y, ro, 0.3, 3, rt), 0.1, 16), 0.3, 0.0);
    }
    f = buf(ln, 1.3, 16); push(lay, ST(ps, "leaf", NOP, NULL)); RET(f);
}

/* graveyard dead_bat */
static int bt_body(void *c, double t, double *out) { (void)c; *out = 0.4 - 0.1 * fabs(t); return SO_OK; }
BLD(b_graveyard_dead_bat) {
    SoFrame ff = frame(cx, cy, a); double S = u(2.2, 2.9); SoSolids *ps = SL(); GG *f;
    spin(ps, cx, cy, a, 1.5, bt_body, NULL, 1.2, 0.0, "round");
    for (int sv = -1; sv <= 1; sv += 2) { P2 q = F(&ff, 0.75, sv * 0.25); cone(ps, q.x, q.y, 0.22, 0.8, 0.2, 0.0, 5, 1.0, 0.0); }
    for (int sv = -1; sv <= 1; sv += 2) {
        PT *wp = plv(3, F(&ff, 0.4, sv * 0.2), F(&ff, 0.9, sv * S * 0.5), F(&ff, 0.5, sv * S));
        for (int k = 1; k < 3; k++) pl_add(wp, F(&ff, -0.2 - 0.45 * fpw(SIN(k * 3.14 / 1.0), 2), sv * S * (1 - k / 3.0)));
        pl_add(wp, F(&ff, -0.5, sv * 0.2));
        ext(ps, buf(Poly(wp), 0, 16), 0.28, 0.0); rod(ps, plv(3, F(&ff, 0.4, sv * 0.2), F(&ff, 0.9, sv * S * 0.5), F(&ff, 0.5, sv * S)), 0.21, 1.0, 0.0, 0.0);
    }
    f = rect(cx, cy, 2.4, 2 * S + 0.4, a); push(lay, ST(ps, "hide", NOP, NULL)); RET(f);
}

/* graveyard chalice_rosary */
static int cr_cup(void *c, double t, double *out) { double L = *(double *)c; *out = t < -L * 0.2 ? 0.75 - 0.5 * (t + L / 2) / (L * 0.3) : t < 0 ? 0.25 : 0.25 + 0.6 * fpw(t / (L / 2), 0.6); return E_ ? SO_ERR : SO_OK; }
static int cr_beads(void *c, double q, double *out) { (void)c; *out = 0.24 + 0.12 * (SIN(q * 90) > 0); return SO_OK; }
BLD(b_graveyard_chalice_rosary) {
    SoFrame ff = frame(cx, cy, a); double L = u(2.4, 3.0); double v1 = u(-0.5, 0.5); double v2 = u(2.2, 2.8); P2 c = F(&ff, v1, v2); double R = u(1.1, 1.5); SoSolids *ps = SL(), *bd = SL(); double ts[30]; PT *bp = pl(); GG *f; SoLayer *y1;
    spin(ps, cx, cy, a, L, cr_cup, &L, 1.1, 0.0, "round");
    linspace(0, 6.2832, 30, ts); for (int i = 0; i < 30; i++) pl_add(bp, pol(c.x, c.y, R * (1 + 0.1 * SIN(3 * ts[i])), ts[i]));
    rodf(bd, bp, cr_beads, NULL, 1.0, 0.0, 0.0);
    ext(bd, U(glv(2, barl(pol(c.x, c.y, R, a), pol(c.x, c.y, R + 1.5, a), 0.36), barl(pol(c.x, c.y, R + 0.9, a + 0.3), pol(c.x, c.y, R + 0.95, a - 0.3), 0.36))), 0.34, 0.0);
    f = hull(glv(2, rect(cx, cy, L, 1.8, a), disc(c.x, c.y, R + 1.6, 20))); y1 = ST(ps, "gold", NOP, NULL); push(lay, y1); push(lay, ST(bd, "wood", NOP, NULL)); RET(f);
}

/* graveyard gargoyle_head */
BLD(b_graveyard_gargoyle_head) {
    SoFrame ff = frame(cx, cy, a); double r = u(1.3, 1.7); double h = r * 0.9; SoSolids *ps = SL(); CL *cu = cl(); P2 q = F(&ff, r * 0.75, 0); GG *f;
    dome(ps, cx, cy, r, h, 0.0, 0.85, a, 20); loaf(ps, q.x, q.y, a, r * 1.0, r * 0.9, h * 0.6, 0.0);
    for (int sv = -1; sv <= 1; sv += 2) { P2 e = F(&ff, -r * 0.3, sv * r * 0.6); double hh = h + u(0.4, 1.0); cone(ps, e.x, e.y, 0.42, hh, 0.22, 0.0, 6, 1.0, 0.0); }
    for (int sv = -1; sv <= 1; sv += 2) ext(ps, tri(F(&ff, -r * 0.1, sv * r * 0.8), F(&ff, -r * 0.5, sv * (r + 0.9)), 0.8), h * 0.5, 0.0);
    for (int sv = -1; sv <= 1; sv += 2) { P2 e = F(&ff, r * 0.2, sv * r * 0.36); cl_add(cu, disc(e.x, e.y, 0.24, 6), h * 0.5); }
    { P2 m = F(&ff, r * 0.95, 0); cl_add(cu, rect(m.x, m.y, 0.5, r * 0.55, a), h * 0.2); }
    f = disc(cx, cy, r + 1.0, 20); push(lay, ST(ps, "headstone", NOP, cu)); RET(f);
}

/* @@NEXT@@ */

/* ====================================================================================================== registration */
void bsdde_e_register(void); void bsdde_d_register(void);          /* bld_sd_e.c: sd_e.py, registered right after sd_d.py */
#define R(st, cls, kind, name, r, ground, fn) bsdde_reg(st, cls, kind, name, r, ground, fn, NULL)
void bld_register_sd_de(void) {
    bsdde_d_register();
    bsdde_e_register();
}
void bsdde_d_register(void) {
/* @@REG@@ */
    /* sd_d.py */
    R("trederra", "made", "wire", "wire_coil", 10.5, 0, bsdde_replaced);          /* drawn again in sd_r_trederra.py */
    R("trederra", "made", "tin", "tin_hat", 10.5, 0, bsdde_replaced);          /* drawn again in sd_r_trederra.py */
    R("trederra", "made", "cloth", "gas_mask", 10.5, 0, bsdde_replaced);          /* drawn again in sd_r_trederra.py */
    R("trederra", "made", "brass", "shell_cases", 10.5, 0, bsdde_replaced);          /* drawn again in sd_r_trederra.py */
    R("trederra", "made", "tin", "ammo_box", 10.5, 0, bsdde_replaced);          /* drawn again in sd_r_trederra.py */
    R("trederra", "made", "tin", "periscope", 10.5, 0, bsdde_replaced);          /* drawn again in sd_r_trederra.py */
    R("trederra", "made", "wood", "field_telephone", 10.5, 0, bsdde_replaced);          /* drawn again in sd_r_trederra.py */
    R("trederra", "made", "iron", "mortar", 10.5, 0, bsdde_replaced);          /* drawn again in sd_r_x_frontier.py */
    R("trederra", "made", "cloth", "stretcher", 10.5, 0, bsdde_replaced);          /* drawn again in sd_r_x_frontier.py */
    R("trederra", "made", "tin", "ration_tins", 10.5, 0, bsdde_replaced);          /* drawn again in sd_r_trederra.py */
    R("trederra", "made", "iron", "screw_pickets", 10.5, 0, bsdde_replaced);          /* drawn again in sd_r_trederra.py */
    R("trederra", "made", "iron", "sniper_plate", 10.5, 0, bsdde_replaced);          /* drawn again in sd_r_trederra.py */
    R("trederra", "made", "iron", "dud_shell", 10.5, 1, bsdde_replaced);          /* drawn again in sd_r_trederra.py */
    R("trederra", "made", "leather", "binoculars_map", 10.5, 0, bsdde_replaced);          /* drawn again in sd_r_trederra.py */
    R("trederra", "growth", "flower", "poppies", 10.5, 0, bsdde_replaced);          /* drawn again in sd_r_trederra.py */
    R("trederra", "made", "hide", "rats", 10.5, 0, bsdde_replaced);          /* drawn again in sd_r_trederra.py */
    R("trederra", "marks", "water", "boot_puddles", 10.5, 0, b_trederra_boot_puddles);
    R("trederra", "made", "wood", "stick_grenades", 10.5, 0, bsdde_replaced);          /* drawn again in sd_r_trederra.py */
    R("trederra", "made", "brass", "bugle", 10.5, 0, bsdde_replaced);          /* drawn again in sd_r_trederra.py */
    R("trederra", "made", "iron", "machine_gun", 10.5, 0, bsdde_replaced);          /* drawn again in sd_r_trederra.py */
    R("fortress", "made", "paper", "paper_lantern", 10.5, 0, b_fortress_paper_lantern);
    R("fortress", "made", "lacquer", "kabuto", 10.5, 0, b_fortress_kabuto);
    R("fortress", "made", "clay", "sake_set", 10.5, 0, b_fortress_sake_set);
    R("fortress", "growth", "shrub", "bonsai", 10.5, 0, b_fortress_bonsai);
    R("fortress", "made", "garden", "tsukubai", 10.5, 1, b_fortress_tsukubai);
    R("fortress", "made", "bamboo", "deer_scarer", 10.5, 0, b_fortress_deer_scarer);
    R("fortress", "made", "wood", "taiko", 10.5, 0, b_fortress_taiko);
    R("fortress", "made", "wood", "geta", 10.5, 0, b_fortress_geta);
    R("fortress", "made", "lacquer", "sword_stand", 10.5, 0, b_fortress_sword_stand);
    R("fortress", "made", "wood", "go_board", 10.5, 0, b_fortress_go_board);
    R("fortress", "made", "paper", "fan", 10.5, 0, b_fortress_fan);
    R("fortress", "made", "iron", "shuriken", 10.5, 0, b_fortress_shuriken);
    R("fortress", "made", "wood", "shamisen", 10.5, 0, b_fortress_shamisen);
    R("fortress", "made", "bamboo", "bamboo_fence", 10.5, 0, bsdde_replaced);          /* drawn again in sd_r_x_frontier.py */
    R("fortress", "made", "sack", "rice_bales", 10.5, 0, bsdde_replaced);          /* drawn again in sd_r_x_frontier.py */
    R("fortress", "made", "lacquer", "torii_beam", 10.5, 0, bsdde_replaced);          /* drawn again in sd_r_x_frontier.py */
    R("fortress", "marks", "pan", "raked_gravel", 10.5, 1, b_fortress_raked_gravel);
    R("fortress", "made", "brass", "koro", 10.5, 0, b_fortress_koro);
    R("fortress", "made", "brass", "suzu_bell", 10.5, 0, b_fortress_suzu_bell);
    R("fortress", "made", "lacquer", "menpo", 10.5, 0, b_fortress_menpo);
    R("beast", "debris", "tooth", "molar", 10.5, 0, b_beast_molar);
    R("beast", "growth", "flesh", "polyps", 10.5, 0, b_beast_polyps);
    R("beast", "growth", "flesh", "gut_folds", 10.5, 0, b_beast_gut_folds);
    R("beast", "made", "tin", "eaten_cuirass", 10.5, 0, b_beast_eaten_cuirass);
    R("beast", "growth", "tendril", "parasite_worm", 10.5, 1, b_beast_parasite_worm);
    R("beast", "growth", "flesh", "cilia", 10.5, 0, b_beast_cilia);
    R("beast", "marks", "bile", "bile_sump", 10.5, 1, b_beast_bile_sump);
    R("beast", "debris", "tooth", "gallstones", 10.5, 0, b_beast_gallstones);
    R("beast", "made", "bone", "vertebrae", 10.5, 0, b_beast_vertebrae);
    R("beast", "made", "bone", "fish_bones", 10.5, 0, b_beast_fish_bones);
    R("beast", "growth", "flesh", "taste_buds", 10.5, 0, b_beast_taste_buds);
    R("beast", "growth", "vein", "tendon_cords", 10.5, 0, b_beast_tendon_cords);
    R("beast", "growth", "flesh", "embedded_eye", 10.5, 0, b_beast_embedded_eye);
    R("beast", "marks", "pit", "ulcer", 10.5, 1, b_beast_ulcer);
    R("beast", "made", "wood", "dissolving_barrel", 10.5, 0, b_beast_dissolving_barrel);
    R("beast", "growth", "vein", "nerve_cluster", 10.5, 0, b_beast_nerve_cluster);
    R("beast", "debris", "bone", "bone_spurs", 10.5, 0, b_beast_bone_spurs);
    R("beast", "growth", "tooth", "lamprey_mouth", 10.5, 1, b_beast_lamprey_mouth);
    R("beast", "growth", "pustule", "split_cyst", 10.5, 0, b_beast_split_cyst);
    R("beast", "growth", "tooth", "barnacles", 10.5, 0, b_beast_barnacles);
    R("frontier", "made", "brass", "spittoon", 10.5, 0, b_frontier_spittoon);
    R("frontier", "made", "paper", "poker_hand", 10.5, 0, b_frontier_poker_hand);
    R("frontier", "made", "board", "wanted_board", 10.5, 0, b_frontier_wanted_board);
    R("frontier", "made", "board", "batwing_door", 10.5, 0, bsdde_replaced);          /* drawn again in sd_r_x_frontier.py */
    R("frontier", "made", "tin", "washtub", 10.5, 0, b_frontier_washtub);
    R("frontier", "made", "iron", "branding_irons", 10.5, 0, b_frontier_branding_irons);
    R("frontier", "made", "wood", "butter_churn", 10.5, 0, b_frontier_butter_churn);
    R("frontier", "made", "sack", "hay_bale", 10.5, 0, bsdde_replaced);          /* drawn again in sd_r_x_frontier.py */
    R("frontier", "made", "leather", "boots_and_spurs", 10.5, 0, b_frontier_boots_and_spurs);
    R("frontier", "made", "rope", "lasso", 10.5, 0, b_frontier_lasso);
    R("frontier", "made", "clay", "whiskey_jug", 10.5, 0, b_frontier_whiskey_jug);
    R("frontier", "made", "iron", "busted_safe", 10.5, 0, bsdde_replaced);          /* drawn again in sd_r_x_frontier.py */
    R("frontier", "made", "leather", "gunbelt", 10.5, 0, b_frontier_gunbelt);
    R("frontier", "made", "wood", "singletree", 10.5, 0, b_frontier_singletree);
    R("frontier", "made", "iron", "potbelly_stove", 10.5, 0, bsdde_replaced);          /* drawn again in sd_r_x_frontier.py */
    R("frontier", "made", "brass", "church_bell", 10.5, 0, bsdde_replaced);          /* drawn again in sd_r_x_frontier.py */
    R("frontier", "made", "board", "shop_sign", 10.5, 0, bsdde_replaced);          /* drawn again in sd_r_x_frontier.py */
    R("frontier", "made", "wood", "nail_keg", 10.5, 0, b_frontier_nail_keg);
    R("frontier", "made", "leather", "horse_collar", 10.5, 0, b_frontier_horse_collar);
    R("frontier", "marks", "water", "rut_puddles", 10.5, 0, b_frontier_rut_puddles);
    R("graveyard", "made", "wood", "wooden_cross", 10.5, 0, bsdde_replaced);          /* drawn again in sd_r_x_graveyard.py */
    R("graveyard", "growth", "leaf", "wreath", 10.5, 0, b_graveyard_wreath);
    R("graveyard", "made", "headstone", "broken_obelisk", 10.5, 0, bsdde_replaced);          /* drawn again in sd_r_x_graveyard.py */
    R("graveyard", "made", "headstone", "angel_wing", 10.5, 0, b_graveyard_angel_wing);
    R("graveyard", "made", "coffin", "open_coffin", 10.5, 0, bsdde_replaced);          /* drawn again in sd_r_x_graveyard.py */
    R("graveyard", "made", "wax", "candles", 10.5, 0, b_graveyard_candles);
    R("graveyard", "made", "fence", "fallen_railing", 10.5, 0, bsdde_replaced);          /* drawn again in sd_r_x_graveyard.py */
    R("graveyard", "made", "bone", "skull_pile", 10.5, 0, b_graveyard_skull_pile);
    R("graveyard", "made", "iron", "robbers_hole", 10.5, 1, bsdde_replaced);          /* drawn again in sd_r_x_graveyard.py */
    R("graveyard", "made", "hide", "raven", 10.5, 0, b_graveyard_raven);
    R("graveyard", "made", "brass", "grave_bell", 10.5, 1, bsdde_replaced);          /* drawn again in sd_r_x_graveyard.py */
    R("graveyard", "made", "headstone", "effigy_slab", 10.5, 0, bsdde_replaced);          /* drawn again in sd_r_x_graveyard.py */
    R("graveyard", "made", "clay", "toppled_vase", 10.5, 0, b_graveyard_toppled_vase);
    R("graveyard", "marks", "pit", "sunken_grave", 10.5, 1, bsdde_replaced);          /* drawn again in sd_r_x_graveyard.py */
    R("graveyard", "made", "headstone", "footstones", 10.5, 0, bsdde_replaced);          /* drawn again in sd_r_x_graveyard.py */
    R("graveyard", "growth", "leaf", "ivy", 10.5, 0, b_graveyard_ivy);
    R("graveyard", "made", "hide", "dead_bat", 10.5, 0, b_graveyard_dead_bat);
    R("graveyard", "made", "gold", "chalice_rosary", 10.5, 0, b_graveyard_chalice_rosary);
    R("graveyard", "made", "headstone", "gargoyle_head", 10.5, 0, b_graveyard_gargoyle_head);
    R("graveyard", "made", "fence", "mortsafe", 10.5, 0, bsdde_replaced);          /* drawn again in sd_r_x_graveyard.py */
}

