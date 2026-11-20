/* bld_sd_e.c: the style-only details of sd_e.py in C (registered by bld_sd_d.c's bld_register_sd_de, right after
   sd_d.py, through bsdde_e_register). Agent "bld-sdde", 2026-11-19. Each builder copies the Python `_` of the same
   style and name line by line (rules: bld_sdde.h); a builder that a later file draws again keeps only its place
   (bsdde_replaced). Proof: work\cscene\bld_sdde\prove.py. */
#include "bld_sdde.h"

/* ====================================================================================================== forest */
/* forest fern_fronds */
static int ff_frond(void *c, double q, double *out) { double L = *(double *)c; *out = (0.62 * (1 - (q + L / 2) / L) + 0.1) * (0.72 + 0.28 * (SIN(q * 8) > 0)); return SO_OK; }
BLD(b_forest_fern_fronds) {
    SoSolids *ps = SL(); GL *foot = gl(); int n; GG *f;
    dome(ps, cx, cy, 0.6, 0.6, 0.0, 1.0, 0.0, 8); n = ri(4, 7);
    for (int k = 0; k < n; k++) {
        double t = a + 6.2832 * k / n + u(-0.3, 0.3); double L = u(2.4, 3.6); P2 c = pol(cx, cy, L / 2 + 0.2, t), q = pol(cx, cy, L + 0.2, t);
        spin(ps, c.x, c.y, t, L, ff_frond, &L, 0.7, 0.0, "ridge"); gl_add(foot, disc(q.x, q.y, 0.3, 20));
    }
    f = hull(foot); push(lay, ST(ps, "fern", NOP, NULL)); RET(f);
}

/* forest pine_cones */
static int pc_cone(void *c, double q, double *out) { double L = *(double *)c; *out = 0.5 * SIN(fpw(MX(0.03, MN(0.97, (q + L / 2) / L)), 0.7) * PI_) * (0.8 + 0.2 * (SIN(q * 14) > 0)) + 0.06; return SO_OK; }
BLD(b_forest_pine_cones) {
    SoSolids *ps = SL(); GL *foot = gl(); int n = ri(3, 6); GG *f;
    for (int i = 0; i < n; i++) { double v1 = u(0, 2.2); double v2 = u(0, 6.28); P2 p = pol(cx, cy, v1, v2); double b = u(0, 6.28); double L = u(1.5, 2.2); spin(ps, p.x, p.y, b, L, pc_cone, &L, 1.1, 0.0, "round"); gl_add(foot, disc(p.x, p.y, L / 2 + 0.1, 20)); }
    f = buf(hull(foot), 0.2, 16); push(lay, ST(ps, "twig", NOP, NULL)); RET(f);
}

/* forest acorns_and_leaves */
BLD(b_forest_acorns_and_leaves) {
    SoSolids *ps = SL(); GL *foot = gl(); int n = ri(2, 4), m; static const double QW[4][2] = { { -0.9, 0.55 }, { -0.3, 0.75 }, { 0.35, 0.7 }, { 0.95, 0.45 } }; GG *f;
    for (int i = 0; i < n; i++) {
        double v = u(0.5, 2.4); P2 p = pol(cx, cy, v, a + i * 1.9); double b = u(0, 6.28); SoFrame f2 = frame(p.x, p.y, b); GL *ls = gl(); GG *leaf;
        for (int k = 0; k < 4; k++) { P2 q = F(&f2, QW[k][0], 0); gl_add(ls, ell(q.x, q.y, 0.5, QW[k][1], b, 10)); }
        leaf = U(ls); ext(ps, leaf, 0.28, 0.0); rod(ps, plv(2, F(&f2, -1.6, 0), F(&f2, 1.2, 0)), 0.2, 1.6, 0.0, 0.0); gl_add(foot, leaf);
    }
    m = ri(2, 5);
    for (int i = 0; i < m; i++) {
        double v1 = u(0, 2.0); double v2 = u(0, 6.28); P2 p = pol(cx, cy, v1, v2); GG *pt = Pt(p.x, p.y); int ok = 1;
        for (int j = 0; j < foot->n; j++) if (!(gdist(pt, foot->v[j]) > 0.6)) { ok = 0; break; }
        if (ok && !E_) { dome(ps, p.x, p.y, 0.38, 0.75, 0.0, 1.0, 0.0, 8); cyl(ps, p.x, p.y, 0.44, 0.3, 0.0, 8); gl_add(foot, disc(p.x, p.y, 0.5, 20)); }
    }
    f = buf(hull(foot), 0.2, 16); push(lay, ST(ps, "leaf", NOP, NULL)); RET(f);
}

/* forest wild_hive */
typedef struct { double r, L, ph; } WhC;
static int wh_hive(void *c, double t, double *out) { WhC *k = (WhC *)c; *out = k->r * SIN(fpw(MX(0.04, MN(0.96, (t + k->L / 2) / k->L)), 0.75) * PI_) * (1 + 0.07 * SIN(t * 7 + k->ph)) + 0.06; return SO_OK; }
BLD(b_forest_wild_hive) {
    SoFrame ff = frame(cx, cy, a); double L = u(3.0, 3.8); double r = u(1.1, 1.4); double ph = u(0, 6); WhC wc = { r, L, ph }; SoSolids *ps = SL(); CL *cu = cl(); GG *f;
    spin(ps, cx, cy, a, L, wh_hive, &wc, 1.0, 0.0, "round");
    for (int i = -1; i < 2; i++) for (int j = -1; j < 2; j++) { P2 q = F(&ff, L * 0.05 + i * 0.5 + 0.25 * pmod(j, 2), j * 0.44); cl_add(cu, ngon(q.x, q.y, 0.24, 6, a), r * 0.55); }
    f = rect(cx, cy, L + 0.2, 2 * r + 0.3, a); push(lay, ST(ps, "wax", NOP, cu)); RET(f);
}

/* forest fox_den */
static int fd_root(void *c, double q, double *out) { (void)c; *out = 0.5 - 0.25 * q; return SO_OK; }
BLD(b_forest_fox_den) {
    SoFrame ff = frame(cx, cy, a); double A = u(1.3, 1.7); double B = A * 0.7; GG *m, *f; SoSolids *roots = SL(); SoLayer *y1, *y2;
    { double v = u(-0.5, 0.5); P2 q = F(&ff, A + 1.6, v); double rb = u(1.2, 1.6); m = blob(q.x, q.y, rb, 4); }
    for (int sv = -1; sv <= 1; sv += 2) rodf(roots, plv(3, F(&ff, -A - 0.9, sv * 0.4), F(&ff, -A * 0.3, sv * (B + 0.55)), F(&ff, A * 0.8, sv * (B + 0.75))), fd_root, NULL, 1.3, 0.0, 0.0);
    f = hull(glv(2, ell(cx, cy, A + 1.0, B + 1.2, a, 20), m));
    y1 = L_gcut(ell(cx, cy, A, B, a, 14), u(0.6, 0.85), "pit"); y2 = ST(roots, "root", NOP, NULL);
    push(lay, y1); push(lay, y2); push(lay, L_mound(m, u(0.5, 0.8), "earth", &BSDDE_SOFT_T)); RET(f);
}

/* forest puffballs */
BLD(b_forest_puffballs) {
    SoSolids *ps = SL(); CL *cu = cl(); GL *foot = gl(); int n = ri(3, 6); GG *f;
    for (int i = 0; i < n; i++) {
        double v = i == 0 ? 0 : u(1.5, 2.3); P2 p = pol(cx, cy, v, a + i * 1.25); double r = u(0.55, 1.0); int ok = 1;
        for (int j = 0; j < foot->n; j++) if (!(gdist(disc(p.x, p.y, r, 20), foot->v[j]) > 0.15)) { ok = 0; break; }
        if (ok && !E_) {
            turn(ps, p.x, p.y, pl_cat(plv(2, p2(r * 0.7, 0.0), p2(r, r * 0.6)), crv(r, 0.25, r * 0.6, r * 1.5, 1, 6)), 12, 1.0, 0.0, 0.0); gl_add(foot, disc(p.x, p.y, r, 20));
            if (rnd() < 0.3) { double t = u(0, 1); cl_add(cu, star(p.x, p.y, r * 0.5, r * 0.2, 5, t), r * 0.8); }
        }
    }
    f = buf(hull(foot), 0.2, 16); push(lay, ST(ps, "shroom", NOP, cu)); RET(f);
}

/* forest moss_stones */
BLD(b_forest_moss_stones) {
    SoSolids *ps = SL(); GL *foot = gl(); int n = ri(2, 4); GG *f;
    for (int i = 0; i < n; i++) {
        double v = i == 0 ? 0 : u(1.8, 2.5); P2 p = pol(cx, cy, v, a + i * 2.0); double r = u(0.9, 1.4); double h = r * u(0.7, 1.0); double x1, y1;
        rock(ps, p.x, p.y, r, h, 0.0, 0.0, 9); x1 = p.x + u(-0.2, 0.2); y1 = p.y + u(-0.2, 0.2); dome(ps, x1, y1, r * 0.7, 0.4, h * 0.8, 1.0, 0.0, 9); gl_add(foot, disc(p.x, p.y, r, 20));
    }
    f = buf(hull(foot), 0.2, 16); push(lay, ST(ps, "rock", NOP, NULL)); RET(f);
}

/* forest brambles */
BLD(b_forest_brambles) {
    SoSolids *ps = SL(); GL *foot = gl(); int n = ri(2, 4); GG *f;
    for (int i = 0; i < n; i++) {
        double t = a + i * 1.1 + u(-0.3, 0.3); double R = u(2.0, 3.2); P2 c0 = pol(cx, cy, 0.3 * i, t + 1.57); double v = u(1.6, 2.6); PT *pts = arc(c0.x, c0.y, R, t, t + v, 12);
        rod(ps, pts, 0.28, 1.4, 0.0, 0.0); gl_add(foot, buf(Line(pts), 0.6, 16));
        for (int j = 1; j < pts->n - 1; j += 2) { P2 p = pl_at(pts, j); double w = u(0, 6.28); P2 q = pol(p.x, p.y, 0.36, w); double h = u(0.5, 0.8); cone(ps, q.x, q.y, 0.25, h, 0.2, 0.0, 5, 1.0, 0.0); }
    }
    f = hull(foot); push(lay, ST(ps, "twig", NOP, NULL)); RET(f);
}

/* forest snare */
BLD(b_forest_snare) {
    double R = u(1.1, 1.5); double v = u(2.5, 4.0); P2 e = pol(cx, cy, R + v, a), pg = pol(cx, cy, R + 1.2, a + 3.14); SoSolids *ps = SL(); double ts[24]; PT *rp = pl(); GG *f;
    linspace(a, a + 6.2832, 24, ts); for (int i = 0; i < 24; i++) pl_add(rp, pol(cx, cy, R, ts[i]));
    rod(ps, rp, 0.24, 1.0, 0.0, 0.0); rod(ps, wiggle(pol(cx, cy, R, a), e, 0.4, 4), 0.24, 1.0, 0.0, 0.0);
    { double h = u(1.4, 2.0); cone(ps, e.x, e.y, 0.42, h, 0.24, 0.0, 6, 1.0, 0.0); } cone(ps, pg.x, pg.y, 0.36, 1.0, 0.24, 0.0, 6, 1.0, 0.0);
    f = hull(glv(3, disc(cx, cy, R + 0.3, 20), disc(e.x, e.y, 0.5, 20), disc(pg.x, pg.y, 0.5, 20))); push(lay, ST(ps, "rope", NOP, NULL)); RET(f);
}

/* forest sap_bucket */
BLD(b_forest_sap_bucket) {
    double r = u(1.1, 1.4); double h = u(1.8, 2.4); P2 c = pol(cx, cy, r + 0.9, a); double rb = u(0.7, 0.85); SoSolids *ps = SL(); CL *cu = cl(); GG *f;
    turn(ps, cx, cy, plv(3, p2(r * 1.35, 0.0), p2(r, h * 0.3), p2(r, h)), 12, 1.0, 0.0, 0.0); rod(ps, plv(2, pol(cx, cy, r * 0.6, a), pol(cx, cy, r + 0.6, a)), 0.24, 1.0, h * 0.55, 0.0);
    turn(ps, c.x, c.y, plv(2, p2(rb * 0.8, 0.0), p2(rb, h * 0.5)), 12, 1.0, 0.0, 0.0);
    f = hull(glv(2, disc(cx, cy, r * 1.4, 20), disc(c.x, c.y, rb + 0.2, 20)));
    cl_add(cu, disc(c.x, c.y, rb - 0.3, 8), h * 0.5 - 0.5); cl_add(cu, ann(cx, cy, r * 0.7, r * 0.7 - 0.18, 10), h - 0.14); push(lay, ST(ps, "wood", NOP, cu)); RET(f);
}

/* forest feathers_pellets */
static int fp_feather(void *c, double q, double *out) { double L = *(double *)c; *out = 0.5 * fpw(SIN(MX(0.05, MN(0.95, (q + L / 2) / L)) * PI_), 0.6) + 0.05; return SO_OK; }
BLD(b_forest_feathers_pellets) {
    SoSolids *ps = SL(); GL *foot = gl(); int n = ri(2, 4), m; GG *f;
    for (int i = 0; i < n; i++) {
        double v = u(0.4, 2.2); P2 p = pol(cx, cy, v, a + i * 1.7); double b = u(0, 6.28); double L = u(2.2, 3.2); SoFrame f2 = frame(p.x, p.y, b);
        spin(ps, p.x, p.y, b, L, fp_feather, &L, 0.6, 0.0, "ridge"); rod(ps, plv(2, F(&f2, -L / 2 - 0.7, 0), F(&f2, -L / 2 + 0.3, 0)), 0.2, 1.0, 0.0, 0.0); gl_add(foot, disc(p.x, p.y, L / 2 + 0.5, 20));
    }
    m = ri(1, 3);
    for (int i = 0; i < m; i++) { double v = u(2.6, 3.2); P2 p = pol(cx, cy, v, a + 0.9 + i * 2.0); double t = u(0, 3); dome(ps, p.x, p.y, 0.45, 0.5, 0.0, 0.65, t, 8); gl_add(foot, disc(p.x, p.y, 0.5, 20)); }
    f = hull(foot); push(lay, ST(ps, "hide", NOP, NULL)); RET(f);
}

/* forest boar_wallow */
BLD(b_forest_boar_wallow) {
    SoFrame ff = frame(cx, cy, a); double v = u(1.9, 2.6); GG *p = blob(cx, cy, v, 5), *f; GL *gs = gl(), *all = gl(); int n = ri(3, 5);
    for (int i = 0; i < n; i++) { P2 c = F(&ff, 3.4 + i * 1.3, 0.5 * (i % 2 ? 1 : -1)); SoFrame f2 = frame(c.x, c.y, a); for (int sv = -1; sv <= 1; sv += 2) { P2 q = F(&f2, 0, sv * 0.2); gl_add(gs, ell(q.x, q.y, 0.36, 0.17, a, 6)); } }
    gl_add(all, p); for (int i = 0; i < gs->n; i++) gl_add(all, gs->v[i]);
    f = buf(hull(all), 0.3, 16); push(lay, L_gcut(p, u(0.3, 0.45), "water")); push(lay, L_gcut(U(gs), 0.3, "pit")); RET(f);
}

/* forest leaf_litter */
BLD(b_forest_leaf_litter) {
    SoSolids *ps = SL(); GL *foot = gl(); int n = ri(7, 13); GG *f;
    for (int i = 0; i < n; i++) {
        double v1 = u(0, 2.6); double v2 = u(0, 6.28); P2 p = pol(cx, cy, v1, v2); double b = u(0, 3.14); double rx = u(0.6, 0.85); double ry = u(0.3, 0.42); GG *g = U(glv(2, ell(p.x, p.y, rx, ry, b, 8), tri(pol(p.x, p.y, 0.5, b), pol(p.x, p.y, 1.15, b), 0.42)));
        ext(ps, g, u(0.2, 0.45), 0.0); gl_add(foot, g);
    }
    f = buf(hull(foot), 0.2, 16); push(lay, ST(ps, "leaf", NOP, NULL)); RET(f);
}

/* forest amanita */
BLD(b_forest_amanita) {
    SoSolids *ps = SL(); GL *foot = gl(); int n = ri(1, 3); GG *f;
    for (int i = 0; i < n; i++) {
        double v = i == 0 ? 0 : u(1.8, 2.6); P2 p = pol(cx, cy, v, a + i * 2.6); double R = u(1.0, 1.5) * (i == 0 ? 1 : 0.7); double h = u(1.6, 2.6) * (i == 0 ? 1 : 0.7); int m;
        turn(ps, p.x, p.y, pl_cat(plv(2, p2(R, 0.0), p2(R, h * 0.62)), crv(R, 0.28, h * 0.62, h, 1, 6)), 16, 1.0, 0.0, 0.0);
        m = ri(4, 7);
        for (int k = 0; k < m; k++) { double w1 = u(0.2, R * 0.72); double w2 = u(0, 6.28); P2 q = pol(p.x, p.y, w1, w2); dome(ps, q.x, q.y, 0.26, 0.3, h * 0.82, 1.0, 0.0, 6); }
        gl_add(foot, disc(p.x, p.y, R, 20));
    }
    f = buf(hull(foot), 0.2, 16); push(lay, ST(ps, "shroom", NOP, NULL)); RET(f);
}

/* forest waymarker */
BLD(b_forest_waymarker) {
    SoFrame ff = frame(cx, cy, a); double L = u(3.4, 4.4); double W = u(1.9, 2.4); double h = u(0.7, 0.95); GG *g, *arrow, *f; CL *cu = cl(); SoSolids *ps = SL();
    { P2 q1 = F(&ff, -W / 4, 0), q2 = F(&ff, L / 2 - W / 2, 0); g = U(glv(2, rect(q1.x, q1.y, L - W / 2, W, a), disc(q2.x, q2.y, W / 2, 16))); }
    arrow = U(glv(2, barl(F(&ff, -L * 0.3, 0), F(&ff, L * 0.15, 0), 0.26), line(plv(3, F(&ff, L * 0.0, -0.45), F(&ff, L * 0.2, 0), F(&ff, L * 0.0, 0.45)), 0.26)));
    cl_add(cu, arrow, h - 0.2);
    for (int sv = -1; sv <= 1; sv += 2) if (rnd() < 0.6) { P2 q = F(&ff, -L * 0.36, sv * W * 0.26); cl_add(cu, buf(exter(ngon(q.x, q.y, 0.3, 3, a + sv)), 0.1, 16), h - 0.2); }
    f = buf(g, 0.2, 16);
    ext(ps, g, h, 0.0); { double v1 = u(-1, 0.5); double v2 = u(-0.5, 0.5); int ch = rchi(2, -1, 1); P2 q = F(&ff, v1, v2 * 0 + W * 0.3 * ch); dome(ps, q.x, q.y, 0.5, 0.3, h * 0.9, 1.0, 0.0, 8); }
    push(lay, ST(ps, "rock", NOP, cu)); RET(f);
}

/* forest squirrel_cache */
BLD(b_forest_squirrel_cache) {
    double R = u(1.2, 1.6); SoSolids *ps = SL(); int n = ri(4, 8); GG *f;
    for (int i = 0; i < n; i++) { double v1 = u(0.5, 1.3); double v2 = u(-1.3, 1.3); P2 p = pol(cx, cy, R + v1, a + v2); double r = u(0.3, 0.42); double h = u(0.5, 0.7); dome(ps, p.x, p.y, r, h, 0.0, 1.0, 0.0, 7); }
    f = disc(cx, cy, R + 1.5, 20); push(lay, L_gcut(disc(cx, cy, R, 12), u(0.5, 0.7), "pit")); push(lay, ST(ps, "twig", NOP, NULL)); RET(f);
}

/* forest wolf_tracks */
BLD(b_forest_wolf_tracks) {
    SoFrame ff = frame(cx, cy, a); int n = ri(3, 6); GL *gs = gl(); static const double VS[4] = { -0.42, -0.14, 0.14, 0.42 }; GG *f;
    for (int i = 0; i < n; i++) {
        P2 c = F(&ff, (i - (n - 1) / 2.0) * 2.0, 0.6 * (i % 2 ? 1 : -1)); SoFrame f2 = frame(c.x, c.y, a + u(-0.15, 0.15)); P2 q = F(&f2, -0.2, 0);
        gl_add(gs, ell(q.x, q.y, 0.42, 0.36, a, 8));
        for (int j = 0; j < 4; j++) { P2 t = F(&f2, 0.42, VS[j]); gl_add(gs, ell(t.x, t.y, 0.22, 0.16, a, 6)); }
    }
    f = rect(cx, cy, n * 2.0 + 1.0, 2.8, a); push(lay, L_gcut(U(gs), 0.3, "pit")); RET(f);
}

/* ====================================================================================================== dungeon */
/* dungeon bear_trap */
static int bt_chain(void *c, double q, double *out) { (void)c; *out = 0.24 + 0.08 * (SIN(q * 40) > 0); return SO_OK; }
BLD(b_dungeon_bear_trap) {
    SoFrame ff = frame(cx, cy, a); double R = u(1.5, 1.9); double v1 = u(2.5, 4.0); double v2 = u(-0.5, 0.5); P2 e = pol(cx, cy, R + v1, a + 3.14 + v2); SoSolids *ps = SL(); GG *f;
    ext(ps, disc(cx, cy, R * 0.42, 10), 0.3, 0.0); blk(ps, cx, cy, 2 * R + 1.6, 0.6, a, 0.36, 0.0);
    for (int sv = -1; sv <= 1; sv += 2) {
        PT *pts = arc(cx, cy, R, a + (sv > 0 ? 0.12 : 3.26), a + (sv > 0 ? 3.02 : 6.16), 12); rod(ps, pts, 0.25, 1.1, 0.0, 0.0);
        for (int j = 1; j < pts->n - 1; j += 2) { P2 p = pl_at(pts, j), q = pol(p.x, p.y, 0.3, ATAN2(cy - p.y, cx - p.x)); cone(ps, q.x, q.y, 0.25, 0.65, 0.2, 0.0, 5, 1.0, 0.0); }
    }
    rodf(ps, wiggle(F(&ff, -R - 0.8, 0), e, 0.5, 4), bt_chain, NULL, 1.0, 0.0, 0.0);
    f = hull(glv(2, disc(cx, cy, R + 0.9, 20), disc(e.x, e.y, 0.4, 20))); push(lay, ST(ps, "iron", NOP, NULL)); RET(f);
}

/* dungeon torch */
static int to_torch(void *c, double t, double *out) { double L = *(double *)c; *out = 0.26 + 0.1 * (t + L / 2) / L + (t > L * 0.22 ? 0.3 * fpw(SIN(MX(0.0, (t - L * 0.22) / (L * 0.28)) * 3.0), 2) : 0.0); return SO_OK; }
BLD(b_dungeon_torch) {
    SoFrame ff = frame(cx, cy, a); double L = u(4.4, 5.6); GG *p, *f; SoSolids *ps = SL(); P2 q = F(&ff, -L * 0.1, 0.55);
    { double v = u(-0.5, 0.5); P2 c = F(&ff, L / 2 + 0.6, v); double rb = u(0.8, 1.2); p = blob(c.x, c.y, rb, 3); }
    spin(ps, cx, cy, a, L, to_torch, &L, 1.1, 0.0, "round"); ext(ps, ann(q.x, q.y, 0.55, 0.26, 8), 0.4, 0.0);
    f = hull(glv(2, rect(cx, cy, L, 1.6, a), p)); push(lay, L_gcut(p, 0.2, "scorch")); push(lay, ST(ps, "wood", NOP, NULL)); RET(f);
}

/* dungeon potions */
static int po_lying(void *c, double t, double *out) { *out = t < 0.2 ? *(double *)c : 0.28; return SO_OK; }
BLD(b_dungeon_potions) {
    SoFrame ff = frame(cx, cy, a); SoSolids *ps = SL(); int n = ri(2, 4); SoLayer *ly[4]; int nl = 0; GG *f; double rs[4];
    for (int i = 0; i < n; i++) {
        double v = u(-0.3, 0.3); P2 c = F(&ff, (i - (n - 1) / 2.0) * 1.7, v); int k = ri(0, 2); double r = u(0.55, 0.72); rs[i] = r;
        if (rnd() < 0.25) { double b = u(0, 6.28); P2 q = pol(c.x, c.y, 1.6, b); spin(ps, c.x, c.y, b, 1.9, po_lying, &rs[i], 1.1, 0.0, "round"); ly[nl++] = L_gcut(blob(q.x, q.y, 0.7, 3), 0.2, "acid"); }
        else if (k == 0) turn(ps, c.x, c.y, pl_cat(pl_cat(plv(1, p2(r * 0.7, 0.0)), crv(r, 0.26, r * 0.5, r * 1.5, 1, 6)), plv(2, p2(0.26, r * 2.0), p2(0.34, r * 2.1))), 12, 1.0, 0.0, 0.0);
        else if (k == 1) turn(ps, c.x, c.y, plv(3, p2(r, 0.0), p2(0.26, r * 1.9), p2(0.26, r * 2.4)), 12, 1.0, 0.0, 0.0);
        else turn(ps, c.x, c.y, plv(3, p2(0.36, 0.0), p2(0.36, r * 2.6), p2(0.42, r * 2.7)), 8, 1.0, 0.0, 0.0);
    }
    f = rect(cx, cy, n * 1.7 + 2.6, 4.6, a); push(lay, ST(ps, "glass", NOP, NULL));
    if (E_) return SC_ERR;
    for (int j = 0; j < nl; j++) {
        int ok = 1;
        for (int i = 0; i < n; i++) { P2 q = F(&ff, (i - (n - 1) / 2.0) * 1.7, 0); if (!(gdist(ly1(ly[j]), disc(q.x, q.y, 0.9, 20)) > 0.3)) { ok = 0; break; } }
        if (ok) { push(lay, ly[j]); break; }
    }
    RET(f);
}

/* dungeon skull_candle */
BLD(b_dungeon_skull_candle) {
    SoFrame ff = frame(cx, cy, a); double r = u(0.95, 1.2); double h = r * 1.2; SoSolids *ps = SL(); CL *cu = cl(); P2 q = F(&ff, r * 0.8, 0), cd = F(&ff, -r * 0.15, 0); int n; GG *f;
    dome(ps, cx, cy, r, h, 0.0, 0.86, a, 20); blk(ps, q.x, q.y, 0.6, r * 1.0, a, h * 0.45, 0.0);
    { double v = u(0.4, 1.1); turn(ps, cd.x, cd.y, plv(5, p2(0.5, 0.0), p2(0.5, h * 0.9), p2(0.36, h + 0.1), p2(0.36, h + v), p2(0.2, h + 1.2)), 8, 1.0, 0.0, 0.0); }
    n = ri(2, 4);
    for (int i = 0; i < n; i++) { double v1 = u(0.6, 0.95); double v2 = u(0, 6.28); rod(ps, plv(2, cd, pol(cd.x, cd.y, r * v1, v2)), 0.22, 1.0, h * 0.55, 0.0); }
    for (int sv = -1; sv <= 1; sv += 2) { P2 e = F(&ff, r * 0.5, sv * r * 0.38); cl_add(cu, disc(e.x, e.y, 0.25, 6), h * 0.45); }
    f = disc(cx, cy, r + 0.5, 20); push(lay, ST(ps, "bone", NOP, cu)); RET(f);
}

/* dungeon spellbook */
BLD(b_dungeon_spellbook) {
    SoFrame ff = frame(cx, cy, a); double L = u(2.6, 3.2); double W = u(1.9, 2.3); SoSolids *ps = SL(); CL *cu = cl(); double vs[32]; int nv; GG *f;
    blk(ps, cx, cy, 2 * W + 0.5, L + 0.3, a, 0.3, 0.0);
    for (int sv = -1; sv <= 1; sv += 2) { P2 q = F(&ff, sv * (W / 2 + 0.06), 0); double h = u(0.5, 0.75); loaf(ps, q.x, q.y, a + 1.5708, L, W, h, 0.0); }
    nv = arange(-L / 2 + 0.6, L / 2 - 0.4, 0.5, vs, 32);
    for (int sv = -1; sv <= 1; sv += 2) for (int i = 0; i < nv; i++) if (rnd() < 0.85) { P2 q = F(&ff, sv * (W / 2 + 0.1), vs[i]); cl_add(cu, rect(q.x, q.y, W * 0.62, 0.16, a), 0.36); }
    if (rnd() < 0.5) {
        P2 c = F(&ff, W / 2, 0); int k = rchi(3, 3, 5, 6); GG *pt; CL *c2 = cl();
        cl_add(cu, buf(exter(ngon(c.x, c.y, 0.5, k, a)), 0.09, 16), 0.36); pt = Pt(c.x, c.y);
        for (int i = 0; i < cu->n; i++) if (gdist(cu->v[i].q, pt) > 0.75 || i == cu->n - 1) cl_add(c2, cu->v[i].q, cu->v[i].fl);
        cu = c2;
    }
    f = rect(cx, cy, 2 * W + 0.7, L + 0.5, a); push(lay, ST(ps, "paper", NOP, cu)); RET(f);
}

/* dungeon key_ring */
BLD(b_dungeon_key_ring) {
    double R = u(0.95, 1.2); GL *gs = gl(); int n; GG *g, *f;
    gl_add(gs, ann(cx, cy, R, R - 0.34, 14)); n = ri(3, 5);
    for (int k = 0; k < n; k++) {
        double t = a + k * 0.75 + u(-0.15, 0.15); P2 p0 = pol(cx, cy, R - 0.1, t); double L = u(2.0, 2.8); P2 e = pol(p0.x, p0.y, L, t), q = pol(p0.x, p0.y, 0.5, t); SoFrame f2 = frame(e.x, e.y, t); P2 r1 = F(&f2, -0.3, 0.4), r2 = F(&f2, -1.0, 0.36);
        gl_add(gs, barl(p0, e, 0.34)); gl_add(gs, ann(q.x, q.y, 0.5, 0.2, 8)); gl_add(gs, rect(r1.x, r1.y, 0.5, 0.6, t)); gl_add(gs, rect(r2.x, r2.y, 0.36, 0.5, t));
    }
    g = U(gs); f = buf(chull(g), 0.2, 16); { SoSolids *ps = SL(); ext(ps, g, 0.36, 0.0); push(lay, ST(ps, "iron", NOP, NULL)); }
    RET(f);
}

/* dungeon crown_sceptre */
BLD(b_dungeon_crown_sceptre) {
    double r = u(1.0, 1.25); double h = u(0.8, 1.1); int n = ri(5, 8); P2 c0 = pol(cx, cy, r + 1.4, a + 1.57); SoFrame f2 = frame(c0.x, c0.y, a + u(-0.4, 0.4)); double L = u(5.0, 6.5); SoSolids *ps = SL(), *sc_ = SL(); PT *rp; GG *f; SoLayer *y1;
    ext(ps, ann(cx, cy, r, r - 0.42, 18), h, 0.0);
    rp = ringpts(cx, cy, r - 0.21, n, a, 0.0, 0); for (int i = 0; i < rp->n; i++) { P2 p = pl_at(rp, i); double hh = h + u(0.5, 0.8); cone(ps, p.x, p.y, 0.34, hh, 0.2, 0.0, 6, 1.0, 0.0); }
    { P2 d1 = F(&f2, L / 2, 0), l1 = F(&f2, L / 2 - 0.8, 0), d2 = F(&f2, -L / 2, 0); rod(sc_, plv(2, F(&f2, -L / 2, 0), F(&f2, L / 2, 0)), 0.25, 1.1, 0.0, 0.0); dome(sc_, d1.x, d1.y, 0.55, 0.9, 0.0, 1.0, 0.0, 10); lie(sc_, l1.x, l1.y, a, 0.36, 0.42, 0.0); dome(sc_, d2.x, d2.y, 0.36, 0.5, 0.0, 1.0, 0.0, 8); }
    { P2 m = F(&f2, 0, 0); f = hull(glv(2, disc(cx, cy, r + 0.3, 20), rect(m.x, m.y, L + 1.2, 1.3, a))); }
    y1 = ST(ps, "gold", NOP, NULL); push(lay, y1); push(lay, ST(sc_, "gold", NOP, NULL)); RET(f);
}

/* dungeon block_and_axe */
BLD(b_dungeon_block_and_axe) {
    SoFrame ff = frame(cx, cy, a); double r = u(1.3, 1.6); double h = u(1.2, 1.6); double L = u(4.6, 5.6); double b = a + u(-0.4, 0.4); P2 q = F(&ff, 0, r + 1.5); SoFrame f2 = frame(q.x, q.y, b); GG *blade, *f; SoSolids *p1 = SL(), *p2s = SL(); CL *cu = cl(); SoLayer *y1;
    blade = Poly(plv(5, F(&f2, L / 2 - 1.0, -0.2), F(&f2, L / 2 - 0.2, -1.5), F(&f2, L / 2 + 0.5, -1.2), F(&f2, L / 2 + 0.6, 0.3), F(&f2, L / 2 - 0.2, 0.3)));
    { P2 m = F(&f2, 0, -0.5); f = hull(glv(2, disc(cx, cy, r + 0.2, 20), rect(m.x, m.y, L + 1.4, 2.2, b))); }
    turn(p1, cx, cy, plv(3, p2(r * 1.08, 0.0), p2(r, h * 0.2), p2(r, h)), 9, 1.0, 0.0, 0.0);
    { P2 e = F(&ff, 0, -r * 0.5); cl_add(cu, ell(e.x, e.y, r * 0.5, r * 0.32, a, 10), h - 0.35); cl_add(cu, barl(F(&ff, -r * 0.7, r * 0.3), F(&ff, r * 0.6, r * 0.5), 0.2), h - 0.2); }
    y1 = ST(p1, "wood", NOP, cu); rod(p2s, plv(2, F(&f2, -L / 2, 0), F(&f2, L / 2, 0)), 0.27, 1.1, 0.0, 0.0); ext(p2s, blade, 0.42, 0.0); push(lay, y1); push(lay, ST(p2s, "iron", NOP, NULL)); RET(f);
}

/* dungeon dice_and_cup */
typedef struct { double L; } DcupC;
static int dcup_cup(void *c, double q, double *out) { double L = ((DcupC *)c)->L; *out = 0.62 + 0.14 * (q + L / 2) / L; return SO_OK; }
BLD(b_dungeon_dice_and_cup) {
    SoSolids *ps = SL(), *cup = SL(); CL *cu = cl(); GL *foot = gl(); P2 c = pol(cx, cy, 2.2, a); double b = a + u(1.5, 3.5); double L = 2.0; int n = ri(2, 3); DcupC dc = { L }; GG *f, *cr; int ok = 1;
    static const double PIP[4][4][2] = { { { 0, 0 } }, { { -0.22, -0.22 }, { 0.22, 0.22 } }, { { -0.24, -0.24 }, { 0, 0 }, { 0.24, 0.24 } }, { { -0.22, -0.22 }, { 0.22, 0.22 }, { -0.22, 0.22 }, { 0.22, -0.22 } } };
    for (int i = 0; i < n; i++) {
        double v = u(0.0, 1.2); P2 p = pol(cx, cy, v, a + 3.14 + i * 1.6); double t = u(0, 1.5); SoFrame f2; int k;
        ext(ps, buf(buf(rect(p.x, p.y, 0.95, 0.95, t), -0.15, 16), 0.15, 16), 0.9, 0.0); f2 = frame(p.x, p.y, t);
        k = rch(4); for (int j = 0; j <= k; j++) { P2 q = F(&f2, PIP[k][j][0], PIP[k][j][1]); cl_add(cu, disc(q.x, q.y, 0.13, 5), 0.72); }
        gl_add(foot, disc(p.x, p.y, 0.8, 20));
    }
    spin(cup, c.x, c.y, b, L, dcup_cup, &dc, 1.1, 0.0, "round");
    { GL *all = gl(); for (int i = 0; i < foot->n; i++) gl_add(all, foot->v[i]); gl_add(all, rect(c.x, c.y, L, 1.7, b)); f = buf(hull(all), 0.2, 16); }
    cr = rect(c.x, c.y, L, 1.7, b);
    for (int i = 0; i < foot->n; i++) if (!(gdist(foot->v[i], cr) > 0.2)) { ok = 0; break; }
    { SoLayer *y1 = ST(ps, "bone", NOP, cu); push(lay, y1); if (ok) push(lay, ST(cup, "leather", NOP, NULL)); }
    RET(f);
}

/* dungeon ooze */
static int oz_knob(void *c, double q, double *out) { (void)c; *out = 0.26 + 0.14 * (q < 0.14 || q > 0.86); return SO_OK; }
BLD(b_dungeon_ooze) {
    double R = u(1.8, 2.5); SoSolids *ps = SL(); int n = ri(4, 6); GG *f;
    for (int i = 0; i < n; i++) { double v1 = u(0, R * 0.55); double v2 = u(0, 6.28); P2 p = pol(cx, cy, v1, v2); double r = u(0.9, 1.4); turn(ps, p.x, p.y, plv(3, p2(r, 0.0), p2(r * 0.85, 0.35), p2(r * 0.4, 0.55)), 12, 1.0, 0.0, 0.0); }
    n = ri(1, 3);
    for (int i = 0; i < n; i++) { double t = u(0, 6.28); double v1 = u(0, R * 0.3); double v2 = u(0, 6.28); P2 p = pol(cx, cy, v1, v2); double L = u(1.6, 2.4); rodf(ps, plv(2, p, pol(p.x, p.y, L, t)), oz_knob, NULL, 1.0, 0.3, 0.0); }
    f = disc(cx, cy, R + 1.2, 20); push(lay, ST(ps, "slime", NOP, NULL)); RET(f);
}

/* dungeon mace_and_shield */
typedef struct { double W, L; } MsC;
static int ms_shield(void *c, double t, double *out) { MsC *k = (MsC *)c; *out = k->W / 2 * (t < -k->L * 0.1 ? 1.0 : MX(0.0, 1 - fpw((t + k->L * 0.1) / (k->L * 0.6), 1.6))) + 0.04; return SO_OK; }
BLD(b_dungeon_mace_and_shield) {
    SoFrame ff = frame(cx, cy, a); double L = u(3.6, 4.4); double W = u(2.6, 3.1); SoFrame f2; double Lm; MsC mc; SoSolids *sh = SL(), *mace = SL(); CL *cu = cl(); GG *f; SoLayer *y1;
    { double v = u(-0.5, 0.5); P2 q = F(&ff, v, W / 2 + 1.3); f2 = frame(q.x, q.y, a + u(-0.4, 0.4)); } Lm = u(4.0, 5.0); mc.W = W; mc.L = L;
    spin(sh, cx, cy, a, L, ms_shield, &mc, 0.5, 0.0, "round"); dome(sh, cx - 0.0, cy, 0.6, 0.95, 0.0, 1.0, 0.0, 10);
    { P2 q = F(&f2, Lm / 2 - 0.4, 0); rod(mace, plv(2, F(&f2, -Lm / 2, 0), F(&f2, Lm / 2 - 0.8, 0)), 0.28, 1.1, 0.0, 0.0); fluted(mace, q.x, q.y, 0.85, 1.1, 6, a, 0.55, 4, 0.0, 0.0); }
    { P2 m = F(&f2, 0, 0); f = hull(glv(2, rect(cx, cy, L, W, a), rect(m.x, m.y, Lm + 0.6, 1.8, a))); }
    cl_add(cu, dif(barl(F(&ff, -L * 0.42, 0), F(&ff, L * 0.4, 0), 0.22), disc(cx, cy, 0.8, 20)), 0.3); y1 = ST(sh, "iron", NOP, cu); push(lay, y1); push(lay, ST(mace, "iron", NOP, NULL)); RET(f);
}

/* dungeon scrolls */
BLD(b_dungeon_scrolls) {
    SoFrame ff = frame(cx, cy, a); SoSolids *ps = SL(), *op = SL(); int n = ri(2, 4); SoFrame f2; CL *cu = cl(); static const double VS[3] = { -0.6, -0.1, 0.4 }; GG *f; SoLayer *y1;
    for (int i = 0; i < n; i++) {
        double v = u(-0.4, 0.4); P2 c = F(&ff, v, (i - (n - 1) / 2.0) * 1.0); double b = a + u(-0.25, 0.25); double L = u(2.6, 3.4);
        lie(ps, c.x, c.y, b, L, 0.38, 0.0);
        for (int sv = -1; sv <= 1; sv += 2) { P2 q = pol(c.x, c.y, L / 2 + 0.15, b + (sv > 0 ? 0 : PI_)); lie(ps, q.x, q.y, b, 0.3, 0.5, 0.0); }
    }
    { P2 q = F(&ff, 0, n * 0.5 + 1.9); f2 = frame(q.x, q.y, a + u(-0.2, 0.2)); }
    { P2 m = F(&f2, 0, 0), l1 = F(&f2, 0, 1.4), l2 = F(&f2, 0, -1.4); ext(op, rect(m.x, m.y, 2.6, 2.6, a), 0.24, 0.0); lie(op, l1.x, l1.y, a, 3.0, 0.4, 0.0); lie(op, l2.x, l2.y, a, 3.0, 0.4, 0.0); }
    { P2 m = F(&ff, 0, 1.2); f = rect(m.x, m.y, 4.4, n + 5.0, a); }
    y1 = ST(ps, "paper", NOP, NULL);
    for (int i = 0; i < 3; i++) { P2 q = F(&f2, 0, VS[i]); cl_add(cu, rect(q.x, q.y, 1.9, 0.16, a), 0.12); }
    push(lay, y1); push(lay, ST(op, "paper", NOP, cu)); RET(f);
}

/* dungeon blood_drain */
BLD(b_dungeon_blood_drain) {
    double R = u(1.1, 1.5); double v1 = u(3.0, 5.0); double v2 = u(-0.3, 0.3); P2 e = pol(cx, cy, R + v1, a + v2); GL *gs = gl(); GG *ch, *f;
    gl_add(gs, ann(cx, cy, R, R - 0.3, 14)); gl_add(gs, disc(cx, cy, 0.34, 8));
    for (int k = 0; k < 8; k++) gl_add(gs, barl(pol(cx, cy, 0.5, a + k * 0.7854), pol(cx, cy, R - 0.4, a + k * 0.7854), 0.26));
    ch = wavy(pol(cx, cy, R + 0.3, a), e, 0.3, 3, 0.42);
    f = hull(glv(2, disc(cx, cy, R + 0.2, 20), disc(e.x, e.y, 0.4, 20))); push(lay, L_gcut(U(gs), 0.6, "slot")); push(lay, L_gcut(ch, 0.28, "seam")); RET(f);
}

/* dungeon candelabra */
static int cd_stem(void *c, double t, double *out) { (void)c; *out = 0.3 + 0.12 * fpw(SIN(t * 2.6), 2); return SO_OK; }
BLD(b_dungeon_candelabra) {
    SoFrame ff = frame(cx, cy, a); double L = u(5.0, 6.2); int n = rchi(2, 3, 5); double W = u(1.2, 1.7); SoSolids *ps = SL(); P2 q = F(&ff, -L / 2, 0), m = F(&ff, 0.2, 0); GG *f;
    turn(ps, q.x, q.y, plv(3, p2(1.0, 0.0), p2(1.0, 0.3), p2(0.42, 0.5)), 12, 1.0, 0.0, 0.0); spin(ps, cx, cy, a, L, cd_stem, NULL, 1.1, 0.0, "round");
    for (int k = 0; k < n; k++) { double v = (k - (n - 1) / 2.0) * W; P2 e = F(&ff, L / 2 + 0.6 - fabs(v) * 0.2, v); rod(ps, plv(3, F(&ff, L * 0.2, 0), F(&ff, L * 0.36, v * 0.9), e), 0.25, 1.1, 0.0, 0.0); lie(ps, e.x, e.y, a, 0.7, 0.42, 0.0); }
    f = rect(m.x, m.y, L + 2.2, n * W + 1.2, a); push(lay, ST(ps, "brass", NOP, NULL)); RET(f);
}

/* ====================================================================================================== temple */
/* temple offering_bowl */
BLD(b_temple_offering_bowl) {
    double R = u(1.6, 2.0); double h = u(0.8, 1.1); SoSolids *ps = SL(), *fr = SL(); int n; CL *cu = cl(); GG *f;
    turn(ps, cx, cy, plv(3, p2(R * 0.5, 0.0), p2(R * 0.5, 0.2), p2(R, h)), 20, 1.0, 0.0, 0.0);
    n = ri(3, 7);
    for (int i = 0; i < n; i++) { double v1 = u(0, R * 0.4); double v2 = u(0, 6.28); P2 p = pol(cx, cy, v1, v2); double r = u(0.36, 0.5); double hh = u(0.4, 0.6); dome(fr, p.x, p.y, r, hh, h - 0.25, 1.0, 0.0, 8); }
    f = disc(cx, cy, R + 0.2, 20); cl_add(cu, ann(cx, cy, R - 0.3, R * 0.62, 16), h - 0.3); push(lay, ST(SLc(ps, fr), "gold", NOP, cu)); RET(f);
}

/* temple censer */
static int ce_chain(void *c, double q, double *out) { (void)c; *out = 0.24 + 0.08 * (SIN(q * 50) > 0); return SO_OK; }
BLD(b_temple_censer) {
    double r = u(0.9, 1.1); double v = u(3.0, 4.5); P2 e = pol(cx, cy, r + v, a); PT *pts = wiggle(pol(cx, cy, r * 0.5, a), e, 0.8, 6); SoSolids *ps = SL(); CL *cu = cl(); PT *rp; GG *f;
    turn(ps, cx, cy, pl_cat(pl_cat(plv(3, p2(r * 0.6, 0.0), p2(r * 0.6, 0.2), p2(r, r * 0.7)), crv(r, 0.3, r * 0.7, r * 1.6, 1, 6)), plv(1, p2(0.3, r * 1.9))), 14, 1.0, 0.0, 0.0);
    rodf(ps, pts, ce_chain, NULL, 1.0, 0.0, 0.0); ext(ps, ann(e.x, e.y, 0.55, 0.26, 8), 0.4, 0.0);
    rp = ringpts(cx, cy, r * 0.55, 6, a + 0.5, 0.0, 0); for (int i = 0; i < rp->n; i++) { P2 p = pl_at(rp, i); cl_add(cu, disc(p.x, p.y, 0.17, 5), r * 0.9); }
    f = hull(glv(2, disc(cx, cy, r + 0.2, 20), disc(e.x, e.y, 0.7, 20))); push(lay, ST(ps, "gold", NOP, cu)); RET(f);
}

/* temple idol */
BLD(b_temple_idol) {
    SoFrame ff = frame(cx, cy, a); double S = u(2.0, 2.5); double r = S * 0.36; double h = u(1.6, 2.2); SoSolids *ps = SL(); CL *cu = cl(); GG *f;
    blk(ps, cx, cy, S, S, a, 0.5, 0.0); blk(ps, cx, cy, S * 0.8, S * 0.8, a, 0.75, 0.0);
    turn(ps, cx, cy, pl_cat(plv(4, p2(r * 1.2, 0.75), p2(r, 0.75 + h * 0.5), p2(r * 0.5, 0.75 + h * 0.6), p2(r * 0.72, 0.75 + h * 0.72)), crv(r * 0.72, 0.26, 0.75 + h * 0.72, 0.75 + h, 1, 6)), 14, 1.0, 0.0, 0.0);
    for (int sv = -1; sv <= 1; sv += 2) { P2 q = F(&ff, r * 0.6, sv * r * 0.8); dome(ps, q.x, q.y, r * 0.5, 0.5, 0.75, 1.0, 0.0, 8); }
    for (int sv = -1; sv <= 1; sv += 2) { P2 q = F(&ff, r * 0.4, sv * r * 0.28); cl_add(cu, disc(q.x, q.y, 0.17, 5), 0.75 + h * 0.75); }
    f = rect(cx, cy, S + 0.2, S + 0.2, a); push(lay, ST(ps, "gold", NOP, cu)); RET(f);
}

/* temple prayer_wheel */
typedef struct { double r, L; } PwhC;
static int pwh_drum(void *c, double t, double *out) { PwhC *k = (PwhC *)c; *out = fabs(t) < k->L / 2 - 0.3 ? k->r + 0.1 * (SIN(t * 9) > 0.2) : k->r + 0.16; return SO_OK; }
BLD(b_temple_prayer_wheel) {
    SoFrame ff = frame(cx, cy, a); double L = u(2.4, 3.0); double r = u(0.9, 1.15); double Lh = u(2.2, 3.0); PwhC pc = { r, L }; SoSolids *ps = SL(); P2 q = F(&ff, L / 2 + Lh * 0.6, 0), m = F(&ff, Lh / 2 - 0.2, 0); GG *f;
    spin(ps, cx, cy, a, L, pwh_drum, &pc, 1.1, 0.0, "round"); rod(ps, plv(2, F(&ff, -L / 2 - 0.5, 0), F(&ff, L / 2 + Lh, 0)), 0.27, 1.1, 0.0, 0.0); lie(ps, q.x, q.y, a, Lh * 0.5, 0.4, 0.0);
    f = rect(m.x, m.y, L + Lh + 1.2, 2 * r + 0.4, a); push(lay, ST(ps, "gold", NOP, NULL)); RET(f);
}

/* temple gong */
BLD(b_temple_gong) {
    double R = u(2.0, 2.6); P2 c0 = pol(cx, cy, R + 1.1, a); SoFrame f2 = frame(c0.x, c0.y, a + 1.57 + u(-0.4, 0.4)); double L = u(3.4, 4.2); SoSolids *ps = SL(), *mal = SL(); CL *cu = cl(); GG *f; SoLayer *y1;
    turn(ps, cx, cy, pl_cat(plv(5, p2(R, 0.0), p2(R, 0.5), p2(R - 0.3, 0.5), p2(R - 0.4, 0.32), p2(R * 0.36, 0.32)), crv(R * 0.36, 0.25, 0.32, 0.8, 1, 6)), 28, 1.0, 0.0, 0.0);
    for (int sv = -1; sv <= 1; sv += 2) { P2 q = pol(cx, cy, R - 0.16, a + 3.14 + sv * 0.35); cl_add(cu, disc(q.x, q.y, 0.12, 5), 0.2); }
    { P2 d = F(&f2, L / 2 - 0.2, 0); rod(mal, plv(2, F(&f2, -L / 2, 0), F(&f2, L / 2 - 0.5, 0)), 0.25, 1.1, 0.0, 0.0); dome(mal, d.x, d.y, 0.6, 0.9, 0.0, 1.0, 0.0, 10); }
    { P2 m = F(&f2, 0, 0); f = hull(glv(2, disc(cx, cy, R + 0.2, 20), rect(m.x, m.y, L + 0.6, 1.4, a + 1.57))); }
    y1 = ST(ps, "gold", NOP, cu); push(lay, y1); push(lay, ST(mal, "wood", NOP, NULL)); RET(f);
}

/* temple singing_bowl */
static int sbw_stick(void *c, double t, double *out) { (void)c; *out = 0.26 + 0.14 * (t > 0.5); return SO_OK; }
BLD(b_temple_singing_bowl) {
    double R = u(1.2, 1.5); double r = R * 0.85; double h = u(1.0, 1.3); P2 c0 = pol(cx, cy, R + 1.3, a); SoFrame f2 = frame(c0.x, c0.y, a + 1.57 + u(-0.5, 0.5)); SoSolids *ps = SL(), *st = SL(); double ts[24]; PT *cu_ = pl(); CL *cu = cl(); GG *f; SoLayer *y1;
    linspace(0, 6.2832, 24, ts); for (int i = 0; i < 24; i++) pl_add(cu_, pol(cx, cy, R, ts[i]));
    rod(ps, cu_, 0.5, 0.9, 0.0, 0.0); turn(ps, cx, cy, plv(4, p2(r * 0.7, 0.0), p2(r * 0.7, 0.4), p2(r, 0.4 + h * 0.5), p2(r * 0.95, 0.4 + h)), 18, 1.0, 0.0, 0.0);
    { P2 m = F(&f2, 0, 0); f = hull(glv(2, disc(cx, cy, R + 0.6, 20), rect(m.x, m.y, 3.4, 1.0, a + 1.57))); }
    cl_add(cu, disc(cx, cy, r * 0.95 - 0.28, 14), 0.4 + h - 0.6); y1 = ST(ps, "gold", NOP, cu);
    { P2 m = F(&f2, 0, 0), q = F(&f2, 1, 0); spin(st, m.x, m.y, q.x * 0 + a + 1.57, 3.0, sbw_stick, NULL, 1.1, 0.0, "round"); }
    push(lay, y1); push(lay, ST(st, "wood", NOP, NULL)); RET(f);
}

/* temple sun_inlay */
BLD(b_temple_sun_inlay) {
    double R = u(1.3, 1.7); int n = ri(8, 14); int k = rnd() < 0.5; GL *gs = gl(); GG *f;
    gl_add(gs, ann(cx, cy, R, R - 0.3, 20));
    for (int i = 0; i < n; i++) gl_add(gs, tri(pol(cx, cy, R + 0.25, a + 6.2832 * i / n), pol(cx, cy, R + (i % 2 || !k ? 1.6 : 1.0), a + 6.2832 * i / n), 0.6));
    for (int sv = -1; sv <= 1; sv += 2) { P2 q = pol(cx, cy, R * 0.36, a + sv * 0.6); gl_add(gs, disc(q.x, q.y, 0.2, 6)); }
    gl_add(gs, buf(Line(arc(cx, cy, R * 0.5, a + 2.5, a + 3.8, 6)), 0.13, 16));
    f = disc(cx, cy, R + 1.8, 20); push(lay, L_gcut(U(gs), 0.3, "glyph")); RET(f);
}

/* temple votive_tablets */
BLD(b_temple_votive_tablets) {
    SoFrame ff = frame(cx, cy, a); int n = ri(3, 6); SoSolids *ps = SL(); CL *cu = cl(); static const double VS[3] = { -0.5, 0.0, 0.5 }; GG *f;
    for (int i = 0; i < n; i++) {
        double v = u(-0.3, 0.3); P2 c = F(&ff, (i - (n - 1) / 2.0) * 1.5, v); double b = a + u(-0.25, 0.25); double h = u(0.36, 0.5); GG *R = buf(buf(rect(c.x, c.y, 1.25, 2.0, b), -0.2, 16), 0.2, 16); SoFrame f2 = frame(c.x, c.y, b);
        ext(ps, R, h, 0.0);
        for (int j = 0; j < 3; j++) if (rnd() < 0.8) { P2 q = F(&f2, 0, VS[j]); cl_add(cu, rect(q.x, q.y, 0.75, 0.16, b), h - 0.16); }
    }
    f = rect(cx, cy, n * 1.5 + 0.4, 2.8, a); push(lay, ST(ps, "tcolumn", NOP, cu)); RET(f);
}

/* temple sun_mask */
BLD(b_temple_sun_mask) {
    SoFrame ff = frame(cx, cy, a); double r = u(1.2, 1.5); double h = r * 0.55; int n = ri(7, 11); SoSolids *ps = SL(); CL *cu = cl(); GG *f;
    dome(ps, cx, cy, r, h, 0.0, 0.85, a, 20);
    for (int k = 0; k < n; k++) { P2 p = pol(cx, cy, r * 0.7, a + 1.5708 + (k - (n - 1) / 2.0) * 0.3); double v = u(0.9, 1.6); P2 q = pol(cx, cy, r + v, a + (k - (n - 1) / 2.0) * 0.32 + 3.14); ext(ps, tri(p, q, 0.62), 0.32, 0.0); }
    for (int sv = -1; sv <= 1; sv += 2) { P2 e = F(&ff, -r * 0.15, sv * r * 0.36); cl_add(cu, ell(e.x, e.y, r * 0.2, r * 0.12, a, 8), 0.1); }
    { P2 m = F(&ff, r * 0.5, 0); cl_add(cu, rect(m.x, m.y, 0.26, r * 0.6, a), h * 0.3); }
    f = disc(cx, cy, r + 1.7, 20); push(lay, ST(ps, "gold", NOP, cu)); RET(f);
}

/* temple serpent_drum */
typedef struct { double r, k, ph; } SdC;
static int sd_drum(void *c, double t, double *out) { SdC *q = (SdC *)c; *out = q->r + 0.16 * fpw(SIN(t * q->k + q->ph), 2); return SO_OK; }
BLD(b_temple_serpent_drum) {
    SoFrame ff = frame(cx, cy, a); double L = u(3.6, 4.6); double r = u(1.1, 1.35); double k = u(3.0, 4.5); double ph = u(0, 6); double v = u(-0.4, 0.4); P2 hd = F(&ff, L / 2 + 0.7, v); SdC sc = { r, k, ph }; SoSolids *ps = SL(); CL *cu = cl(); P2 m = F(&ff, 0.5, 0); GG *f;
    spin(ps, cx, cy, a, L, sd_drum, &sc, 1.1, 0.0, "round"); dome(ps, hd.x, hd.y, 0.62, 0.7, 0.0, 0.7, a, 20); rod(ps, plv(2, F(&ff, L / 2 - 0.3, 0), hd), 0.42, 1.0, 0.0, 0.0);
    f = rect(m.x, m.y, L + 1.8, 2 * r + 0.5, a);
    for (int sv = -1; sv <= 1; sv += 2) { P2 q = pol(hd.x, hd.y, 0.25, a + sv * 0.9); cl_add(cu, disc(q.x, q.y, 0.14, 5), 0.5); }
    push(lay, ST(ps, "tcolumn", NOP, cu)); RET(f);
}

/* temple black_mirror */
static int bm_handle(void *c, double t, double *out) { (void)c; *out = 0.36 + 0.1 * fpw(SIN(t * 4), 2); return SO_OK; }
BLD(b_temple_black_mirror) {
    SoFrame ff = frame(cx, cy, a); double R = u(1.3, 1.7); int n = ri(8, 12); SoSolids *ps = SL(); CL *cu = cl(); PT *rp; P2 hq = F(&ff, -R - 1.2, 0), m = F(&ff, -1.2, 0); GG *f;
    turn(ps, cx, cy, plv(5, p2(R, 0.0), p2(R, 0.5), p2(R - 0.4, 0.5), p2(R - 0.42, 0.3), p2(0.3, 0.34)), 24, 1.0, 0.0, 0.0);
    rp = ringpts(cx, cy, R - 0.2, n, a, 0.0, 0); for (int i = 0; i < rp->n; i++) { P2 p = pl_at(rp, i); dome(ps, p.x, p.y, 0.26, 0.7, 0.0, 1.0, 0.0, 6); }
    spin(ps, hq.x, hq.y, a, 2.6, bm_handle, NULL, 1.1, 0.0, "round");
    if (rnd() < 0.4) cl_add(cu, wavy(pol(cx, cy, R * 0.6, a + 1), pol(cx, cy, R * 0.6, a + 3.6), 0.3, 3, 0.2), 0.12);
    f = rect(m.x, m.y, 2 * R + 2.8, 2 * R + 0.3, a); push(lay, ST(ps, "obsidian", NOP, cu)); RET(f);
}

/* temple ritual_dagger */
BLD(b_temple_ritual_dagger) {
    SoFrame ff = frame(cx, cy, a); double L = u(3.6, 4.6); GG *cloth, *blade, *f; double xs[10]; PT *bp = pl(); SoSolids *ps = SL();
    { double v1 = u(-0.4, 0.4); double v2 = u(-0.3, 0.3); P2 q = F(&ff, v1, v2); cloth = rect(q.x, q.y, L + 1.6, 2.6, a + u(-0.3, 0.3)); }
    pl_add(bp, F(&ff, 0, -0.42));
    linspace(0, L * 0.72, 10, xs); for (int i = 0; i < 10; i++) pl_add(bp, F(&ff, xs[i], -0.45 * (1 - xs[i] / (L * 0.75)) + 0.14 * SIN(xs[i] * 4.2)));
    pl_add(bp, F(&ff, L * 0.75, 0));
    linspace(L * 0.72, 0, 10, xs); for (int i = 0; i < 10; i++) pl_add(bp, F(&ff, xs[i], 0.45 * (1 - xs[i] / (L * 0.75)) + 0.14 * SIN(xs[i] * 4.2)));
    blade = buf(Poly(bp), 0, 16);
    { P2 g = F(&ff, 0, 0), d = F(&ff, -L * 0.25 - 0.2, 0); ext(ps, cloth, 0.22, 0.0); ext(ps, blade, 0.52, 0.0); rod(ps, plv(2, F(&ff, -L * 0.25, 0), F(&ff, 0, 0)), 0.3, 1.5, 0.0, 0.0); blk(ps, g.x, g.y, 0.4, 1.7, a, 0.62, 0.0); dome(ps, d.x, d.y, 0.42, 0.8, 0.0, 1.0, 0.0, 8); }
    f = buf(cloth, 0.2, 16); push(lay, ST(ps, "gold", NOP, NULL)); RET(f);
}

/* temple figurines */
static int fg_lying(void *c, double t, double *out) { double r = *(double *)c; *out = r * (0.7 + 0.3 * fpw(SIN(t * 5), 2)); return SO_OK; }
BLD(b_temple_figurines) {
    SoFrame ff = frame(cx, cy, a); int n = ri(3, 5); SoSolids *ps = SL(); double rs[8]; GG *f;
    for (int i = 0; i < n; i++) {
        double v = u(-0.25, 0.25); P2 c = F(&ff, (i - (n - 1) / 2.0) * 1.5, v); double r = u(0.48, 0.62); double h = u(1.3, 2.2); int k = ri(0, 2); rs[i] = r;
        if (rnd() < 0.2) { double t = a + 1.57 + u(-0.5, 0.5); spin(ps, c.x, c.y, t, h, fg_lying, &rs[i], 1.1, 0.0, "round"); continue; }
        ext(ps, ngon(c.x, c.y, r * 1.3, 4, a + 0.785), 0.25, 0.0);
        turn(ps, c.x, c.y, plv(5, p2(r, 0.0), p2(r * (k ? 0.8 : 1.1), h * 0.5), p2(r * 0.5, h * 0.6), p2(r * (k < 2 ? 0.75 : 1.0), h * 0.75), p2(0.25, h)), 10, 1.0, 0.0, 0.0);
    }
    f = rect(cx, cy, n * 1.5 + 0.8, 2.6, a); push(lay, ST(ps, "jade", NOP, NULL)); RET(f);
}

/* temple sun_staff */
BLD(b_temple_sun_staff) {
    SoFrame ff = frame(cx, cy, a); double L = u(8.0, 10.0); P2 e = F(&ff, L / 2, 0); double R = u(0.95, 1.2); int n = ri(7, 10); SoSolids *ps = SL(); CL *cu = cl(); P2 q = F(&ff, L * 0.2, 0), m = F(&ff, 0.6, 0); GG *f;
    rod(ps, plv(2, F(&ff, -L / 2, 0), e), 0.27, 1.1, 0.0, 0.0); ext(ps, disc(e.x, e.y, R, 16), 0.5, 0.0);
    for (int k = 0; k < n; k++) { P2 p = pol(e.x, e.y, R - 0.1, a + (k - (n - 1) / 2.0) * 0.4); double v = u(0.8, 1.3); ext(ps, tri(p, pol(e.x, e.y, R + v, a + (k - (n - 1) / 2.0) * 0.4), 0.55), 0.34, 0.0); }
    lie(ps, q.x, q.y, a, 0.42, 0.42, 0.0);
    f = rect(m.x, m.y, L + 2.6, 2 * R + 2.4, a); cl_add(cu, ann(e.x, e.y, R * 0.62, R * 0.62 - 0.18, 12), 0.34); push(lay, ST(ps, "gold", NOP, cu)); RET(f);
}

/* temple prayer_beads */
static int pb_beads(void *c, double q, double *out) { (void)c; *out = 0.25 + 0.14 * (SIN(q * 110) > 0); return SO_OK; }
BLD(b_temple_prayer_beads) {
    double R = u(1.5, 2.0); double tw = u(0.0, 0.5); double ts[34]; PT *pts = pl(); SoSolids *ps = SL(); P2 p0; static const double SV[3] = { -0.3, 0.0, 0.3 }; GG *f;
    (void)pol(cx, cy, R, a);
    linspace(0, 6.2832, 34, ts); for (int i = 0; i < 34; i++) pl_add(pts, pol(cx, cy, R * (1 + tw * SIN(2 * ts[i])) * (0.8 + 0.2 * COS(ts[i])), a + ts[i]));
    p0 = pl_at(pts, 0);
    rodf(ps, pts, pb_beads, NULL, 1.0, 0.0, 0.0); dome(ps, p0.x, p0.y, 0.5, 0.7, 0.0, 1.0, 0.0, 8);
    for (int i = 0; i < 3; i++) { double v = u(1.2, 1.7); rod(ps, plv(2, p0, pol(p0.x, p0.y, v, a + SV[i])), 0.21, 0.9, 0.0, 0.0); }
    { P2 q = pol(p0.x, p0.y, 1.2, a); f = chull(uni(chull(buf(Line(pts), 0.6, 16)), disc(q.x, q.y, 0.9, 20))); }
    push(lay, ST(ps, "wood", NOP, NULL)); RET(f);
}

/* temple meander_relief */
BLD(b_temple_meander_relief) {
    SoFrame ff = frame(cx, cy, a); int n = ri(2, 4); double s = 2.0; GG *R = rect(cx, cy, n * s + 0.9, 2.9, a), *key, *f; PT *pts = pl(); SoSolids *ps = SL();
    for (int i = 0; i < n; i++) { double x = (i - n / 2.0) * s; pl_add(pts, F(&ff, x, -0.95)); pl_add(pts, F(&ff, x, 0.95)); pl_add(pts, F(&ff, x + 1.45, 0.95)); pl_add(pts, F(&ff, x + 1.45, -0.95)); }
    pl_add(pts, F(&ff, n * s / 2, -0.95));
    key = bufx(Line(pts), 0.2, 16, SHP_CAP_FLAT, SHP_JOIN_MITRE, 5.0);
    f = buf(R, 0.2, 16); ext(ps, R, 0.34, 0.0); ext(ps, key, 0.62, 0.0); ext(ps, dif(R, buf(R, -0.3, 16)), 0.5, 0.0); push(lay, ST(ps, "tflag", NOP, NULL)); RET(f);
}

/* temple water_clock */
BLD(b_temple_water_clock) {
    SoFrame ff = frame(cx, cy, a); SoSolids *ps = SL(); CL *cu = cl(); double x = -2.6; P2 m = F(&ff, 0.05, 0); GG *f;
    for (int i = 0; i < 3; i++) {
        double r = 1.15 - 0.2 * i, h = 1.9 - 0.5 * i; P2 c = F(&ff, x + r, 0);
        turn(ps, c.x, c.y, plv(2, p2(r * 0.8, 0.0), p2(r, h)), 14, 1.0, 0.0, 0.0); { GG *d = disc(c.x, c.y, r - 0.3, 10); cl_add(cu, d, h - u(0.3, 0.6)); }
        if (i < 2) { P2 q = F(&ff, x + 2 * r + 0.1, 0); blk(ps, q.x, q.y, 0.6, 0.42, a, h - 0.45, 0.0); }
        x += 2 * r + 0.2;
    }
    f = rect(m.x, m.y, 6.2, 2.6, a); push(lay, ST(ps, "tcolumn", NOP, cu)); RET(f);
}

/* ====================================================================================================== coast */
/* coast starfish */
static int sf_arm(void *c, double q, double *out) { (void)c; *out = 0.42 - 0.2 * q; return SO_OK; }
BLD(b_coast_starfish) {
    SoSolids *ps = SL(); GL *foot = gl(); int n = ri(1, 3); GG *f;
    for (int i = 0; i < n; i++) {
        double v = i == 0 ? 0 : u(2.4, 3.2); P2 p = pol(cx, cy, v, a + i * 2.4); double R = u(1.2, 1.7); double b = u(0, 1.2);
        dome(ps, p.x, p.y, R * 0.42, 0.55, 0.0, 1.0, 0.0, 10);
        for (int k = 0; k < 5; k++) { double t = b + k * 1.2566 + u(-0.12, 0.12); P2 c = pol(p.x, p.y, R * 0.55, t); double w = u(-0.3, 0.3); P2 e = pol(p.x, p.y, R, t + w); rodf(ps, plv(3, pol(p.x, p.y, 0.1, t), c, e), sf_arm, NULL, 1.0, 0.0, 0.0); }
        gl_add(foot, disc(p.x, p.y, R + 0.2, 20));
    }
    f = hull(foot); push(lay, ST(ps, "shell", NOP, NULL)); RET(f);
}

/* coast conch */
typedef struct { double r, L; } CoC;
static int co_shell(void *c, double t, double *out) { CoC *k = (CoC *)c; *out = (t < k->L * 0.12 ? k->r * (0.25 + 0.75 * (t + k->L / 2) / (k->L * 0.62)) : k->r * (1 - 0.6 * (t - k->L * 0.12) / (k->L * 0.38))) * (0.82 + 0.18 * fabs(SIN(t * 5.5))) + 0.05; return SO_OK; }
BLD(b_coast_conch) {
    SoFrame ff = frame(cx, cy, a); double L = u(3.0, 4.0); double r = u(0.95, 1.2); CoC cc = { r, L }; SoSolids *ps = SL(); CL *cu = cl(); P2 q1 = F(&ff, L * 0.08, r * 0.85), q2 = F(&ff, L * 0.08, r * 0.8), m = F(&ff, 0, 0.3); GG *f;
    spin(ps, cx, cy, a, L, co_shell, &cc, 1.05, 0.0, "round"); ext(ps, ell(q1.x, q1.y, L * 0.3, r * 0.5, a, 12), r * 0.45, 0.0);
    f = rect(m.x, m.y, L + 0.2, 2 * r + 1.0, a); cl_add(cu, ell(q2.x, q2.y, L * 0.2, r * 0.22, a, 8), 0.2); push(lay, ST(ps, "shell", NOP, cu)); RET(f);
}

/* coast crab */
BLD(b_coast_crab) {
    SoFrame ff = frame(cx, cy, a); double W = u(2.0, 2.6); double D = W * 0.65; SoSolids *ps = SL(); CL *cu = cl(); P2 m = F(&ff, 0.2, 0); GG *f;
    dome(ps, cx, cy, W / 2, W * 0.36, 0.0, 0.68, a + 1.5708, 20);
    for (int sv = -1; sv <= 1; sv += 2) {
        P2 el = F(&ff, D * 0.55, sv * W * 0.62); double v = u(0.2, 0.5); P2 c = F(&ff, D * 0.95, sv * W * v);
        rod(ps, plv(3, F(&ff, D * 0.2, sv * W * 0.36), el, c), 0.3, 1.0, 0.0, 0.0); dome(ps, c.x, c.y, 0.5, 0.6, 0.0, 0.7, a + sv * 0.6, 8);
        for (int k = 0; k < 3; k++) { double x = -D * 0.1 - k * 0.42; rod(ps, plv(3, F(&ff, x, sv * W * 0.4), F(&ff, x - 0.1, sv * (W / 2 + 0.6)), F(&ff, x - 0.5 - 0.2 * k, sv * (W / 2 + 1.1))), 0.22, 1.0, 0.0, 0.0); }
    }
    f = rect(m.x, m.y, D + 2.4, W + 2.6, a);
    for (int sv = -1; sv <= 1; sv += 2) { P2 q = F(&ff, D * 0.42, sv * 0.3); cl_add(cu, disc(q.x, q.y, 0.14, 5), 0.3); }
    push(lay, ST(ps, "shell", NOP, cu)); RET(f);
}

/* coast scallops */
BLD(b_coast_scallops) {
    SoSolids *ps = SL(); CL *cu = cl(); GL *foot = gl(); int n = ri(3, 5); static const double QS[3] = { 0.85, 0.65, 0.42 }, TS[5] = { -0.8, -0.4, 0.0, 0.4, 0.8 }; GG *f;
    for (int i = 0; i < n; i++) {
        double v = i == 0 ? 0 : u(1.9, 2.7); P2 p = pol(cx, cy, v, a + i * 1.5); double R = u(0.9, 1.3); double b = u(0, 6.28); double h = R * 0.36; int near = 0; GG *fan;
        for (int j = 0; j < foot->n; j++) if (gdist(disc(p.x, p.y, R, 20), foot->v[j]) < 0.2) { near = 1; break; }
        if (near) continue;
        fan = buf(Poly(pl_cat(plv(1, pol(p.x, p.y, 0.2, b + 3.14)), arc(p.x, p.y, R, b - 1.25, b + 1.25, 12))), 0.1, 16); ext(ps, fan, 0.2, 0.0);
        for (int j = 0; j < 3; j++) { double q = QS[j]; P2 d = pol(p.x, p.y, R * (1 - q) * 0.4, b); ext(ps, isect(fan, disc(d.x, d.y, R * q, 20)), h * (1.1 - q) / 0.68 + 0.2, 0.0); }
        for (int j = 0; j < 5; j++) cl_add(cu, barl(pol(p.x, p.y, R * 0.3, b + TS[j]), pol(p.x, p.y, R, b + TS[j]), 0.17), 0.24);
        gl_add(foot, disc(p.x, p.y, R, 20));
    }
    f = buf(hull(foot), 0.2, 16); push(lay, ST(ps, "shell", NOP, cu)); RET(f);
}

/* coast urchins */
BLD(b_coast_urchins) {
    SoSolids *ps = SL(); CL *cu = cl(); GL *foot = gl(); int n = ri(2, 5); GG *f;
    for (int i = 0; i < n; i++) {
        double v = i == 0 ? 0 : u(1.8, 2.6); P2 p = pol(cx, cy, v, a + i * 1.4); double r = u(0.7, 1.1); int near = 0, k; double t;
        for (int j = 0; j < foot->n; j++) if (gdist(disc(p.x, p.y, r, 20), foot->v[j]) < 0.2) { near = 1; break; }
        if (near) continue;
        k = ri(11, 15); t = u(0, 1); fluted(ps, p.x, p.y, r, r * 0.9, k, t, 0.55, 5, 0.0, 0.0); cl_add(cu, disc(p.x, p.y, 0.2, 6), r * 0.62); gl_add(foot, disc(p.x, p.y, r, 20));
    }
    f = buf(hull(foot), 0.2, 16); push(lay, ST(ps, "shell", NOP, cu)); RET(f);
}

/* coast bollard */
BLD(b_coast_bollard) {
    double r = u(0.9, 1.15); double h = u(1.9, 2.5); double v = u(3.0, 5.0); P2 e = pol(cx, cy, r + v, a); SoSolids *ps = SL(), *rp = SL(); int n; GG *f;
    turn(ps, cx, cy, pl_cat(plv(4, p2(r * 1.4, 0.0), p2(r * 1.4, 0.3), p2(r * 1.3, 0.3), p2(r * 1.3, h * 0.75)), crv(r * 1.3, 0.3, h * 0.75, h, 1, 6)), 16, 1.0, 0.0, 0.0);
    n = ri(1, 3);
    for (int k = 0; k < n; k++) rod(rp, arc(cx, cy, r * 1.3 + 0.36, a + 0.5 + k, a + 6.6 + k, 20), 0.34, 1.0, 0.3 + k * 0.55, 0.0);
    rod(rp, wiggle(pol(cx, cy, r * 1.3 + 0.36, a), e, 0.8, 5), 0.34, 1.0, 0.0, 0.0);
    f = hull(glv(2, disc(cx, cy, r * 1.4 + 0.8, 20), disc(e.x, e.y, 0.5, 20))); push(lay, ST(SLc(ps, rp), "iron", NOP, NULL)); RET(f);
}

/* coast ship_wheel */
BLD(b_coast_ship_wheel) {
    double R = u(1.9, 2.4); int n = rchi(2, 6, 8); GL *gs = gl(); GG *g, *f; SoSolids *ps = SL();
    gl_add(gs, ann(cx, cy, R, R - 0.6, 24)); gl_add(gs, disc(cx, cy, 0.7, 12));
    for (int k = 0; k < n; k++) { double t; P2 q; if (rnd() < 0.12) continue; t = a + 6.2832 * k / n; q = pol(cx, cy, R + 0.8, t); gl_add(gs, barl(pol(cx, cy, 0.4, t), pol(cx, cy, R + 0.75, t), 0.42)); gl_add(gs, disc(q.x, q.y, 0.3, 8)); }
    g = U(gs);
    if (rnd() < 0.3) g = dif(g, dif(Poly(plv(3, p2(cx, cy), pol(cx, cy, 2 * R, a + 1), pol(cx, cy, 2 * R, a + 1.6))), disc(cx, cy, 0.8, 20)));
    f = disc(cx, cy, R + 1.2, 20); ext(ps, g, 0.5, 0.0); cyl(ps, cx, cy, 0.42, 0.8, 0.0, 8); push(lay, ST(ps, "drift", NOP, NULL)); RET(f);
}

/* coast kelp */
static int kp_frond(void *c, double q, double *out) { (void)c; *out = 0.5 * fpw(SIN(MX(0.08, MN(0.95, q)) * PI_), 0.4) + 0.1; return SO_OK; }
BLD(b_coast_kelp) {
    SoSolids *ps = SL(); GL *foot = gl(); int n = ri(2, 4); GG *f;
    for (int i = 0; i < n; i++) {
        double t = a + (i - 1) * u(0.25, 0.6); double L = u(5.0, 8.0); PT *pts = wiggle(p2(cx, cy), pol(cx, cy, L, t), 0.9, 7);
        rodf(ps, pts, kp_frond, NULL, 0.5, 0.0, 0.0); gl_add(foot, buf(Line(pts), 0.7, 16));
        for (int j = 2; j < pts->n - 1; j += 2) { P2 p = pl_at(pts, j); double x1 = p.x + u(-0.4, 0.4); double y1 = p.y + u(-0.4, 0.4); dome(ps, x1, y1, 0.34, 0.55, 0.0, 1.0, 0.0, 7); }
    }
    f = hull(foot); dome(ps, cx, cy, 0.6, 0.6, 0.0, 1.0, 0.0, 8); push(lay, ST(ps, "kelp", NOP, NULL)); RET(f);
}

/* coast crab_burrows */
BLD(b_coast_crab_burrows) {
    GL *gs = gl(), *foot = gl(); SoLayer *ups[16]; int nu = 0, n = ri(2, 4); GG *f, *ug;
    for (int i = 0; i < n; i++) {
        double v = i == 0 ? 0 : u(2.0, 3.0); P2 p = pol(cx, cy, v, a + i * 1.9); double t; int m;
        { double r = u(0.36, 0.5); gl_add(gs, disc(p.x, p.y, r, 8)); } gl_add(foot, disc(p.x, p.y, 1.6, 20)); t = u(0, 6.28);
        m = ri(4, 8); for (int k = 0; k < m; k++) { P2 q = pol(p.x, p.y, 0.8 + 0.3 * k, t + (k % 2 - 0.5) * 0.5); gl_add(gs, disc(q.x, q.y, 0.17, 5)); }
        m = ri(1, 3); for (int k = 0; k < m && nu < 16; k++) { double v1 = u(0.9, 1.3); double v2 = u(-1, 1); P2 q = pol(p.x, p.y, v1, t + 3.14 + v2); ups[nu++] = L_mound(disc(q.x, q.y, 0.45, 8), 0.3, "pebble", &BSDDE_PEB_T); }
    }
    f = hull(foot); ug = U(gs); push(lay, L_gcut(ug, 0.4, "pit"));
    if (E_) return SC_ERR;
    for (int i = 0; i < nu; i++) { int ok = 1; for (int j = 0; j < i; j++) if (!(gdist(ly1(ups[i]), ly1(ups[j])) > 0.3)) { ok = 0; break; } if (ok && gdist(ly1(ups[i]), U(gs)) > 0.3) push(lay, ups[i]); }
    RET(f);
}

/* coast jellyfish */
static int jf_tent(void *c, double q, double *out) { (void)c; *out = 0.26 - 0.05 * q; return SO_OK; }
BLD(b_coast_jellyfish) {
    double R = u(1.5, 2.1); SoSolids *ps = SL(); GL *foot = gl(); CL *cu = cl(); int n; GG *f;
    { double sq = u(0.85, 1.0); turn(ps, cx, cy, plv(3, p2(R, 0.0), p2(R * 0.9, 0.3), p2(R * 0.4, 0.55)), 20, sq, a, 0.0); } gl_add(foot, disc(cx, cy, R, 20));
    n = ri(3, 6);
    for (int k = 0; k < n; k++) { double t = a + u(-0.7, 0.7); double v = u(1.2, 2.6); P2 e = pol(cx, cy, R + v, t); rodf(ps, wiggle(pol(cx, cy, R * 0.7, t), e, 0.4, 4), jf_tent, NULL, 0.8, 0.0, 0.0); gl_add(foot, disc(e.x, e.y, 0.3, 20)); }
    for (int k = 0; k < 4; k++) { P2 q = pol(cx, cy, R * 0.3, k * 1.5708 + a); cl_add(cu, ann(q.x, q.y, R * 0.2, R * 0.2 - 0.13, 8), 0.42); }
    f = hull(foot); push(lay, ST(ps, "glass", NOP, cu)); RET(f);
}

/* coast fish_crate */
static int fc_fish(void *c, double t, double *out) { double Lf = *(double *)c; *out = 0.4 * fpw(SIN(MX(0.05, MN(1.0, (t + Lf / 2) / (Lf * 0.8))) * PI_), 0.7) + 0.12 * (t > Lf * 0.36) + 0.06; return SO_OK; }
static void fc_spin(SoSolids *o, P2 c, double b, double Lf) { spin(o, c.x, c.y, b, Lf, fc_fish, &Lf, 0.9, 0.0, "round"); }
/* (lambda z, f=q[0]: f(max(0.0, z - 0.3)), q[1] + 0.3): a solid's section raised 0.3 */
static int fc_raised(void *c, double z, GEOSGeometry **out) { return sol_sect((Solid *)c, z - 0.3 > 0.0 ? z - 0.3 : 0.0, out); }
static void fc_unref(void *c) { sol_unref((Solid *)c); }
BLD(b_coast_fish_crate) {
    SoFrame ff = frame(cx, cy, a); double L = u(3.6, 4.4); double W = u(2.4, 2.9); double h = u(0.9, 1.2); GG *R = rect(cx, cy, L, W, a), *f; SoSolids *ps = SL(); int n; SoLayer *ly[2]; int nl = 0; P2 m = F(&ff, 0, 0.6);
    ext(ps, dif(R, buf(R, -0.42, 16)), h, 0.0); ext(ps, R, 0.35, 0.0);
    n = ri(2, 3);
    for (int i = 0; i < n; i++) {
        double v = u(-0.3, 0.3); P2 c = F(&ff, v, (i - 1) * 0.62); double b = a + u(-0.15, 0.15); SoSolids *fs = SL();
        fc_spin(fs, c, b, L - 1.3);
        if (!E_) for (int j = 0; j < fs->n; j++) { Solid *s = sol_custom(fc_raised, sol_ref(fs->v[j]), fc_unref, sol_top(fs->v[j]) + 0.3); sol_push(ps, s); sol_unref(s); }
    }
    ly[nl++] = ST(ps, "drift", NOP, NULL);
    if (rnd() < 0.6) { double v1 = u(-1, 1); P2 c = F(&ff, v1, W / 2 + 1.0); double b = a + u(-0.5, 0.5); SoSolids *fs = SL(); fc_spin(fs, c, b, 2.6); ly[nl++] = ST(fs, "hide", NOP, NULL); }
    f = rect(m.x, m.y, L + 0.3, W + 2.2, a); for (int i = 0; i < nl; i++) push(lay, ly[i]); RET(f);
}

/* coast spyglass_compass */
typedef struct { double L; } SgC;
static int sg_glass(void *c, double t, double *out) { double L = ((SgC *)c)->L; *out = t < -L * 0.17 ? 0.3 : t < L * 0.17 ? 0.4 : 0.5; return SO_OK; }
BLD(b_coast_spyglass_compass) {
    SoFrame ff = frame(cx, cy, a); double L = u(4.0, 5.0); double v = u(-1, 1); P2 c = F(&ff, v, 2.0); double r = u(0.85, 1.05); double nd = u(0, 3.14); SgC sc = { L }; SoSolids *ps = SL(), *cp = SL(); P2 q = F(&ff, L / 2 - 0.15, 0); GG *f; SoLayer *y1;
    spin(ps, cx, cy, a, L, sg_glass, &sc, 1.1, 0.0, "round"); lie(ps, q.x, q.y, a, 0.3, 0.58, 0.0);
    turn(cp, c.x, c.y, plv(5, p2(r, 0.0), p2(r, 0.5), p2(r - 0.24, 0.5), p2(r - 0.26, 0.34), p2(0.25, 0.36)), 16, 1.0, 0.0, 0.0);
    f = hull(glv(2, rect(cx, cy, L, 1.3, a), disc(c.x, c.y, r + 0.2, 20)));
    y1 = ST(ps, "brass", NOP, NULL);
    ext(cp, uni(tri(pol(c.x, c.y, 0.1, nd + 3.14), pol(c.x, c.y, r - 0.36, nd), 0.3), tri(pol(c.x, c.y, 0.1, nd), pol(c.x, c.y, r - 0.36, nd + 3.14), 0.3)), 0.48, 0.0);
    push(lay, y1); push(lay, ST(cp, "brass", NOP, NULL)); RET(f);
}

/* coast whale_vertebra */
static int wv_wing(void *c, double t, double *out) { (void)c; *out = 0.5 - 0.22 * fabs(t); return SO_OK; }
static int wv_spine(void *c, double t, double *out) { (void)c; *out = 0.36 + 0.14 * t; return SO_OK; }
BLD(b_coast_whale_vertebra) {
    SoFrame ff = frame(cx, cy, a); double r = u(1.3, 1.7); double h = u(1.0, 1.4); SoSolids *ps = SL(); CL *cu = cl(); P2 q = F(&ff, -r - 0.7, 0), m = F(&ff, -0.5, 0); GG *f;
    turn(ps, cx, cy, plv(3, p2(r, 0.0), p2(r * 0.9, h * 0.5), p2(r, h)), 14, 1.0, 0.0, 0.0);
    for (int sv = -1; sv <= 1; sv += 2) { P2 w = F(&ff, 0, sv * (r + 0.9)); spin(ps, w.x, w.y, a + 1.5708, 2.2, wv_wing, NULL, 1.0, 0.0, "ridge"); }
    spin(ps, q.x, q.y, a, 1.9, wv_spine, NULL, 1.6, 0.0, "ridge");
    f = rect(m.x, m.y, 2 * r + 2.2, 2 * r + 4.0, a); cl_add(cu, disc(cx, cy, r * 0.55, 10), h - 0.25); push(lay, ST(ps, "bone", NOP, cu)); RET(f);
}

/* coast sand_castle */
BLD(b_coast_sand_castle) {
    SoFrame ff = frame(cx, cy, a); double S = u(2.6, 3.4); SoSolids *ps = SL(), *bs = SL(); CL *cu = cl(); P2 bk; GG *f; SoLayer *y1, *y2;
    blk(ps, cx, cy, S, S, a, 0.75, 0.0); { P2 q = F(&ff, S / 2, 0); cl_add(cu, rect(q.x, q.y, 0.6, 0.6, a), 0.0); }
    for (int sx = -1; sx <= 1; sx += 2) for (int sy = -1; sy <= 1; sy += 2) {
        P2 c = F(&ff, sx * S / 2, sy * S / 2); double r, h;
        if (rnd() < 0.15) { dome(ps, c.x, c.y, 0.8, 0.5, 0.0, 1.0, 0.0, 9); continue; }          /* a tower already slumped */
        r = u(0.62, 0.8); h = u(1.3, 2.0); turn(ps, c.x, c.y, plv(3, p2(r * 1.1, 0.0), p2(r, h * 0.8), p2(r, h)), 12, 1.0, 0.0, 0.0);
        { double ts[2] = { a, a + 1.5708 }; for (int j = 0; j < 2; j++) cl_add(cu, barl(pol(c.x, c.y, r, ts[j]), pol(c.x, c.y, r, ts[j] + 3.14), 0.3), h - 0.3); }
    }
    { double v = u(-1, 1); bk = F(&ff, v, S / 2 + 2.0); }
    y1 = ST(ps, "sandstone", NOP, cu); turn(bs, bk.x, bk.y, plv(2, p2(0.75, 0.0), p2(0.55, 1.0)), 12, 1.0, 0.0, 0.0); y2 = ST(bs, "tin", NOP, NULL);
    f = hull(glv(2, rect(cx, cy, S + 1.8, S + 1.8, a), disc(bk.x, bk.y, 0.9, 20))); push(lay, y1); push(lay, y2); RET(f);
}

/* @@NEXT@@ */

/* ====================================================================================================== registration */
#define R(st, cls, kind, name, r, ground, fn) bsdde_reg(st, cls, kind, name, r, ground, fn, NULL)
void bsdde_e_register(void);
void bsdde_e_register(void) {
/* @@REG@@ */
    /* sd_e.py */
    R("forest", "growth", "fern", "fern_fronds", 10.5, 0, b_forest_fern_fronds);
    R("forest", "growth", "twig", "pine_cones", 10.5, 0, b_forest_pine_cones);
    R("forest", "growth", "leaf", "acorns_and_leaves", 10.5, 0, b_forest_acorns_and_leaves);
    R("forest", "growth", "stump", "snag", 10.5, 0, bsdde_replaced);          /* drawn again in sd_r_x_frontier.py */
    R("forest", "growth", "wax", "wild_hive", 10.5, 0, b_forest_wild_hive);
    R("forest", "marks", "pit", "fox_den", 10.5, 1, b_forest_fox_den);
    R("forest", "growth", "shroom", "puffballs", 10.5, 0, b_forest_puffballs);
    R("forest", "debris", "rock", "moss_stones", 10.5, 0, b_forest_moss_stones);
    R("forest", "growth", "twig", "brambles", 10.5, 0, b_forest_brambles);
    R("forest", "made", "log", "hollow_log", 10.5, 0, bsdde_replaced);          /* drawn again in sd_r_x_frontier.py */
    R("forest", "made", "rope", "snare", 10.5, 0, b_forest_snare);
    R("forest", "made", "wood", "sap_bucket", 10.5, 0, b_forest_sap_bucket);
    R("forest", "debris", "hide", "feathers_pellets", 10.5, 0, b_forest_feathers_pellets);
    R("forest", "marks", "water", "boar_wallow", 10.5, 1, b_forest_boar_wallow);
    R("forest", "growth", "leaf", "leaf_litter", 10.5, 0, b_forest_leaf_litter);
    R("forest", "growth", "shroom", "amanita", 10.5, 0, b_forest_amanita);
    R("forest", "made", "rock", "waymarker", 10.5, 0, b_forest_waymarker);
    R("forest", "debris", "twig", "fallen_branch", 10.5, 0, bsdde_replaced);          /* drawn again in sd_r_x_frontier.py */
    R("forest", "made", "twig", "squirrel_cache", 10.5, 1, b_forest_squirrel_cache);
    R("forest", "marks", "pit", "wolf_tracks", 10.5, 0, b_forest_wolf_tracks);
    R("dungeon", "made", "wood", "stocks", 10.5, 0, bsdde_replaced);          /* drawn again in sd_r_x_frontier.py */
    R("dungeon", "made", "iron", "bear_trap", 10.5, 0, b_dungeon_bear_trap);
    R("dungeon", "made", "wood", "torch", 10.5, 0, b_dungeon_torch);
    R("dungeon", "made", "glass", "potions", 10.5, 0, b_dungeon_potions);
    R("dungeon", "made", "bone", "skull_candle", 10.5, 0, b_dungeon_skull_candle);
    R("dungeon", "made", "paper", "spellbook", 10.5, 0, b_dungeon_spellbook);
    R("dungeon", "made", "iron", "key_ring", 10.5, 0, b_dungeon_key_ring);
    R("dungeon", "made", "gold", "crown_sceptre", 10.5, 0, b_dungeon_crown_sceptre);
    R("dungeon", "made", "iron", "portcullis_piece", 10.5, 0, bsdde_replaced);          /* drawn again in sd_r_x_frontier.py */
    R("dungeon", "made", "iron", "gibbet_cage", 10.5, 0, bsdde_replaced);          /* drawn again in sd_r_x_frontier.py */
    R("dungeon", "made", "wood", "mimic", 10.5, 0, bsdde_replaced);          /* drawn again in sd_r_x_frontier.py */
    R("dungeon", "made", "flag", "pressure_plate", 10.5, 0, bsdde_replaced);          /* drawn again in sd_r_x_frontier.py */
    R("dungeon", "made", "wood", "block_and_axe", 10.5, 0, b_dungeon_block_and_axe);
    R("dungeon", "made", "flag", "keystone", 10.5, 0, bsdde_replaced);          /* drawn again in sd_r_x_frontier.py */
    R("dungeon", "made", "bone", "dice_and_cup", 10.5, 0, b_dungeon_dice_and_cup);
    R("dungeon", "debris", "slime", "ooze", 10.5, 0, b_dungeon_ooze);
    R("dungeon", "made", "iron", "mace_and_shield", 10.5, 0, b_dungeon_mace_and_shield);
    R("dungeon", "made", "paper", "scrolls", 10.5, 0, b_dungeon_scrolls);
    R("dungeon", "marks", "slot", "blood_drain", 10.5, 1, b_dungeon_blood_drain);
    R("dungeon", "made", "brass", "candelabra", 10.5, 0, b_dungeon_candelabra);
    R("temple", "made", "gold", "offering_bowl", 10.5, 0, b_temple_offering_bowl);
    R("temple", "made", "gold", "censer", 10.5, 0, b_temple_censer);
    R("temple", "made", "gold", "idol", 10.5, 0, b_temple_idol);
    R("temple", "made", "gold", "prayer_wheel", 10.5, 0, b_temple_prayer_wheel);
    R("temple", "made", "gold", "gong", 10.5, 0, b_temple_gong);
    R("temple", "made", "gold", "singing_bowl", 10.5, 0, b_temple_singing_bowl);
    R("temple", "marks", "glyph", "sun_inlay", 10.5, 0, b_temple_sun_inlay);
    R("temple", "made", "tcolumn", "votive_tablets", 10.5, 0, b_temple_votive_tablets);
    R("temple", "made", "gold", "sun_mask", 10.5, 0, b_temple_sun_mask);
    R("temple", "made", "tcolumn", "serpent_drum", 10.5, 0, b_temple_serpent_drum);
    R("temple", "made", "tcolumn", "lotus_basin", 10.5, 0, bsdde_replaced);          /* drawn again in sd_r_x_frontier.py */
    R("temple", "made", "obsidian", "black_mirror", 10.5, 0, b_temple_black_mirror);
    R("temple", "made", "gold", "ritual_dagger", 10.5, 0, b_temple_ritual_dagger);
    R("temple", "made", "tcolumn", "glyph_lintel", 10.5, 0, bsdde_replaced);          /* drawn again in sd_r_x_frontier.py */
    R("temple", "made", "jade", "figurines", 10.5, 0, b_temple_figurines);
    R("temple", "made", "tcolumn", "calendar_stone", 10.5, 0, bsdde_replaced);          /* drawn again in sd_r_x_frontier.py */
    R("temple", "made", "gold", "sun_staff", 10.5, 0, b_temple_sun_staff);
    R("temple", "made", "wood", "prayer_beads", 10.5, 0, b_temple_prayer_beads);
    R("temple", "made", "tflag", "meander_relief", 10.5, 0, b_temple_meander_relief);
    R("temple", "made", "tcolumn", "water_clock", 10.5, 0, b_temple_water_clock);
    R("coast", "growth", "shell", "starfish", 10.5, 0, b_coast_starfish);
    R("coast", "made", "shell", "conch", 10.5, 0, b_coast_conch);
    R("coast", "made", "shell", "crab", 10.5, 0, b_coast_crab);
    R("coast", "made", "twig", "lobster_pot", 10.5, 0, bsdde_replaced);          /* drawn again in sd_r_x_frontier.py */
    R("coast", "made", "glass", "net_floats", 10.5, 0, bsdde_replaced);          /* drawn again in sd_r_x_frontier.py */
    R("coast", "debris", "drift", "driftwood", 10.5, 0, bsdde_replaced);          /* drawn again in sd_r_x_frontier.py */
    R("coast", "debris", "shell", "scallops", 10.5, 0, b_coast_scallops);
    R("coast", "growth", "shell", "urchins", 10.5, 0, b_coast_urchins);
    R("coast", "made", "iron", "bollard", 10.5, 0, b_coast_bollard);
    R("coast", "made", "drift", "oars", 10.5, 0, bsdde_replaced);          /* drawn again in sd_r_x_frontier.py */
    R("coast", "made", "drift", "ship_wheel", 10.5, 0, b_coast_ship_wheel);
    R("coast", "growth", "kelp", "kelp", 10.5, 0, b_coast_kelp);
    R("coast", "marks", "pit", "crab_burrows", 10.5, 0, b_coast_crab_burrows);
    R("coast", "growth", "glass", "jellyfish", 10.5, 0, b_coast_jellyfish);
    R("coast", "made", "drift", "fish_crate", 10.5, 0, b_coast_fish_crate);
    R("coast", "made", "drift", "capstan", 10.5, 0, bsdde_replaced);          /* drawn again in sd_r_x_frontier.py */
    R("coast", "made", "iron", "cannon", 10.5, 0, bsdde_replaced);          /* drawn again in sd_r_x_frontier.py */
    R("coast", "made", "brass", "spyglass_compass", 10.5, 0, b_coast_spyglass_compass);
    R("coast", "made", "bone", "whale_vertebra", 10.5, 0, b_coast_whale_vertebra);
    R("coast", "made", "sandstone", "sand_castle", 10.5, 0, b_coast_sand_castle);
}
