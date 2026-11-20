/* bld_sd_r.c: the redrawn style-only details in C: sd_r.py, sd_r_badlands.py, sd_r_badlands2.py, sd_r_cynder.py,
   sd_r_infestation.py, sd_r_maahl.py, sd_r_targa.py, sd_r_trederra.py (one section each, in that order), and their
   registration in bld_register_sd_r (with sd_r_models*: bld_sd_models.c). Agent "bld-sdde", 2026-11-19.
   Each builder copies the Python `_` of the same style and name line by line (see bld_sdde.h for the rules);
   a builder that a later file draws again keeps only its place (bsdde_replaced).
   Proof: work\cscene\bld_sdde\prove.py. */
#include "bld_sdde.h"

/* ====================================================================================================== sd_r.py */
/* corridor toolbox: a cantilever toolbox (two lid flaps, a carrying bar, latches, rivets) or open with its trays;
   spanners beside it */
BLD(b_corridor_toolbox) {
    SoFrame ff = frame(cx, cy, a); double L = u(3.6, 4.4); double W = u(2.0, 2.4); double h = u(1.0, 1.3); SoObj *o = OBJ("metal"); GG *g = rrect(cx, cy, L, W, a, 0.25);
    int64_t b = o_up(o, g, h, NULL); int n, i;
    if (rnd() < 0.3) {
        P2 p1 = F(&ff, -L * 0.24, 0), p2_ = F(&ff, L * 0.24, 0);
        o_cut(o, U(glv(2, rect(p1.x, p1.y, L * 0.42, W - 0.6, a), rect(p2_.x, p2_.y, L * 0.42, W - 0.6, a))), b, 0.55);
    } else {
        double xs[2] = { -L / 2 + 0.42, L / 2 - 0.42 }, ar[64]; int na; PT *st;
        int64_t lid = o_lid(o, g, b, 0.18, 0.14, NULL); o_cut(o, rect(cx, cy, L - 0.5, 0.2, a), lid, 0.16);
        o_handle(o, F(&ff, -L * 0.24, 0), F(&ff, L * 0.24, 0), lid, 0.5, 0.45, NULL);
        o_tabs(o, &ff, 2, xs, 0, lid, a, 0.46, 0.7, 0.16, NULL);
        na = arange(-L / 2 + 0.9, L / 2 - 0.6, 0.9, ar, 64); st = pl();
        for (i = 0; i < na; i++) for (int sv = -1; sv <= 1; sv += 2) pl_add(st, F(&ff, ar[i], sv * (W / 2 - 0.45)));
        o_studs(o, st, lid, 0.25, 0.12, NULL, 8);
    }
    n = ri(1, 3);
    for (i = 0; i < n; i++) {          /* spanners: a shaft with a ring at one end and an open jaw at the other */
        double v1 = u(-L / 2, L / 2); double v2 = u(1.0, 1.9); P2 c = F(&ff, v1, -W / 2 - v2); double t = u(0, 3.14);
        P2 p = pol(c.x, c.y, 1.0, t), q = pol(c.x, c.y, 1.0, t + PI_);
        SoSolids *ps = SL(); CL *cu = cl();
        ext(ps, U(glv(3, capl(p, q, 0.5), disc(p.x, p.y, 0.48, 8), disc(q.x, q.y, 0.5, 8))), 0.36, 0.0);
        cl_add(cu, disc(p.x, p.y, 0.2, 6), 0.1); cl_add(cu, barl(q, pol(q.x, q.y, 0.6, t + PI_), 0.3), 0.1);
        o_st(o, ps, NULL, NOP, cu, NULL);
    }
    RET(o_out(o, lay, NULL));
}

/* corridor cell_rack: a charging rack of power cells, one socket empty, one cell perhaps lying beside it */
BLD(b_corridor_cell_rack) {
    SoFrame ff = frame(cx, cy, a); int n = ri(3, 5); double L = n * 1.35 + 0.9, W = 2.0; SoObj *o = OBJ("metal"); int miss = ri(0, n);
    int64_t b = o_up(o, rrect(cx, cy, L, W, a, 0.15), 0.5, NULL); P2 q1 = F(&ff, -1 * (L / 2 - 0.25), 0), q2 = F(&ff, 1 * (L / 2 - 0.25), 0), q3 = F(&ff, 0, W / 2 - 0.3);
    o_on(o, U(glv(2, rect(q1.x, q1.y, 0.5, W, a), rect(q2.x, q2.y, 0.5, W, a))), b, 0.5, NULL); o_on(o, rect(q3.x, q3.y, L - 1.2, 0.46, a), b, 0.22, NULL);
    for (int i = 0; i < n; i++) {
        P2 c = F(&ff, (i - (n - 1) / 2.0) * 1.35, -0.15); double h; SoSolids *ps;
        if (i == miss) { o_cut(o, disc(c.x, c.y, 0.45, 12), b, 0.3); continue; }
        h = u(1.3, 1.8); ps = SL();
        turn(ps, c.x, c.y, plv(10, p2(0.56, 0.0), p2(0.56, 0.2), p2(0.5, 0.22), p2(0.5, h * 0.5), p2(0.56, h * 0.52), p2(0.56, h * 0.62), p2(0.5, h * 0.64), p2(0.5, h), p2(0.3, h + 0.04), p2(0.3, h + 0.3)), 14, 1.0, 0.0, 0.0);
        o_st(o, ps, NULL, b, NULL, NULL);
    }
    if (rnd() < 0.5) {
        double v1 = u(-L / 3, L / 3); double v2 = u(1.0, 1.6); P2 c = F(&ff, v1, -W / 2 - v2); double t = a + u(-0.6, 0.6); SoFrame f2 = frame(c.x, c.y, t);
        SoSolids *ps = SL(); P2 e1 = F(&f2, 0.95, 0), e2 = F(&f2, -0.2, 0);
        lie(ps, c.x, c.y, t, 1.7, 0.5, 0.0); lie(ps, e1.x, e1.y, t, 0.3, 0.3, 0.0); lie(ps, e2.x, e2.y, t, 0.2, 0.56, 0.0);
        o_st(o, ps, NULL, NOP, NULL, NULL);
    }
    RET(o_out(o, lay, NULL));
}

/* corridor junction_box: a screwed cover plate with a warning triangle, cable glands and their cables; or the
   cover lying beside it */
BLD(b_corridor_junction_box) {
    SoFrame ff = frame(cx, cy, a); double S = u(2.2, 2.7); double h = u(0.9, 1.2); SoObj *o = OBJ("metal"); int nk = ri(2, 3); int64_t ks[3]; GL *g0 = gl(); int64_t b, bx;
    rsample(4, nk, ks);
    gl_add(g0, rect(cx, cy, S, S, a));
    for (int i = 0; i < nk; i++) { P2 p = pol(cx, cy, S / 2 + 0.2, a + ks[i] * 1.5708); gl_add(g0, rect(p.x, p.y, 0.7, 0.9, a + ks[i] * 1.5708)); }
    b = o_up(o, U(g0), h * 0.7, NULL); bx = o_on(o, rect(cx, cy, S, S, a), b, h * 0.3, NULL);
    if (rnd() < 0.3) {
        o_cut(o, rect(cx, cy, S - 0.7, S - 0.7, a), bx, 0.4);
        double t2 = a + u(-0.6, 0.6); double v1 = u(-0.5, 0.5); double v2 = u(1.7, 2.3); P2 c = F(&ff, v1, -S / 2 - v2); GG *cg = rect(c.x, c.y, S - 0.3, S - 0.3, t2);
        int64_t c2 = o_up(o, cg, 0.3, NULL); SoFrame f2 = frame(c.x, c.y, t2); double q = S / 2 - 0.55; PT *st = pl();
        for (int sx = -1; sx <= 1; sx += 2) for (int sy = -1; sy <= 1; sy += 2) pl_add(st, F(&f2, sx * q, sy * q));
        o_studs(o, st, c2, 0.25, 0.12, NULL, 8);
    } else {
        int64_t lid = o_lid(o, rect(cx, cy, S, S, a), bx, 0.15, 0.18, NULL); PT *st = pl();
        for (int sx = -1; sx <= 1; sx += 2) for (int sy = -1; sy <= 1; sy += 2) pl_add(st, F(&ff, sx * (S / 2 - 0.5), sy * (S / 2 - 0.5)));
        o_studs(o, st, lid, 0.25, 0.12, NULL, 8);
        o_cut(o, U(glv(3, barl(F(&ff, -0.45, -0.35), F(&ff, 0, 0.5), 0.17), barl(F(&ff, 0.45, -0.35), F(&ff, 0, 0.5), 0.17), barl(F(&ff, -0.45, -0.35), F(&ff, 0.45, -0.35), 0.17))), lid, 0.12);
    }
    for (int i = 0; i < nk; i++) {          /* each cable leaves its gland with a small gap */
        double t = a + ks[i] * 1.5708; P2 p0 = pol(cx, cy, S / 2 + 0.95, t); double v1 = u(2.4, 3.6); double v2 = u(-0.35, 0.35); P2 e = pol(cx, cy, S / 2 + v1, t + v2);
        SoSolids *ps = SL(); rod(ps, wiggle(p0, e, 0.4, 3), 0.3, 1.1, 0.0, 0.0); o_st(o, ps, "cable", NOP, NULL, NULL);
    }
    RET(o_out(o, lay, NULL));
}

/* corridor vent_fan: an extractor fan lying loose: plate, shroud ring, swept blades (some snapped), hub, guard bar */
BLD(b_corridor_vent_fan) {
    double R = u(1.8, 2.2); int n = ri(4, 6); double S = 2 * R + 0.9; SoObj *o = OBJ("metal"); SoFrame ff = frame(cx, cy, a);
    int64_t b = o_up(o, rrect(cx, cy, S, S, a, 0.4), 0.3, NULL), hub; GL *hs = gl(), *bl = gl(); double t0;
    for (int sx = -1; sx <= 1; sx += 2) for (int sy = -1; sy <= 1; sy += 2) { P2 p = F(&ff, sx * (S / 2 - 0.4), sy * (S / 2 - 0.4)); gl_add(hs, disc(p.x, p.y, 0.2, 6)); }
    o_cut(o, U(hs), b, 0.25);
    o_on(o, ann(cx, cy, R, R - 0.5, 28), b, 0.6, NULL); t0 = u(0, 6.28);
    for (int k = 0; k < n; k++) {
        double t;
        if (rnd() < 0.18) continue;
        t = t0 + 6.2832 * k / n;
        gl_add(bl, Poly(plv(5, pol(cx, cy, 0.35, t - 0.5), pol(cx, cy, R - 0.75, t - 0.1), pol(cx, cy, R - 0.7, t + 0.36), pol(cx, cy, R - 1.0, t + 0.5), pol(cx, cy, 0.35, t + 0.4))));
    }
    if (bl->n) o_on(o, U(bl), b, 0.3, NULL);
    hub = o_on(o, disc(cx, cy, 0.62, 12), b, 0.5, NULL); o_cut(o, disc(cx, cy, 0.22, 6), hub, 0.2);
    if (rnd() < 0.4) { double t = u(0, 3.14); o_on(o, barl(pol(cx, cy, R - 0.3, t), pol(cx, cy, R - 0.3, t + PI_), 0.46), b, 0.62, NULL); }
    RET(o_out(o, lay, NULL));
}

/* corridor mess_tray: a pressed ration tray with its wells, food in some, a beaker and a spoon */
BLD(b_corridor_mess_tray) {
    SoFrame ff = frame(cx, cy, a); double L = u(3.2, 3.8); double W = u(2.3, 2.7); SoObj *o = OBJ("tin"); GG *g = rrect(cx, cy, L, W, a, 0.35);
    int64_t b = o_up(o, g, 0.3, NULL), cup; P2 w1 = F(&ff, -L * 0.23, 0), w2 = F(&ff, L * 0.22, W * 0.21), w3 = F(&ff, L * 0.22, -W * 0.22);
    GG *wells[3]; wells[0] = rrect(w1.x, w1.y, L * 0.4, W - 0.7, a, 0.2); wells[1] = rrect(w2.x, w2.y, L * 0.38, W * 0.36, a, 0.15); wells[2] = rrect(w3.x, w3.y, L * 0.38, W * 0.34, a, 0.15);
    o_on(o, dif(g, U(glv(3, wells[0], wells[1], wells[2]))), b, 0.2, NULL);
    for (int i = 0; i < 3; i++) {
        if (rnd() < 0.6) {
            GG *c = cent(wells[i]); double v1 = gx(c) + u(-0.15, 0.15); double v2 = gy(c) + u(-0.15, 0.15); double v3 = u(0.34, 0.42);
            GG *bb = isect(blob(v1, v2, v3, 3), inset(wells[i], 0.08));
            o_on(o, bb, b, u(0.16, 0.3), "moss");
        }
    }
    { double v1 = u(1.0, 1.5); double v2 = u(-0.8, 0.8); P2 c = F(&ff, L / 2 + v1, v2); GG *dc = disc(c.x, c.y, 0.6, 14); cup = o_up(o, dc, u(0.8, 1.0), NULL); o_cut(o, disc(c.x, c.y, 0.36, 10), cup, 0.45); }
    { double v1 = u(-0.6, 0.6); double v2 = u(0.8, 1.2); P2 p = F(&ff, v1, -W / 2 - v2); double t = a + u(-0.5, 0.5); P2 e = pol(p.x, p.y, 1.7, t);
      SoSolids *ps = SL(); CL *cu = cl();
      ext(ps, U(glv(2, capl(p, pol(p.x, p.y, 1.5, t), 0.46), ell(e.x, e.y, 0.5, 0.34, t, 8))), 0.3, 0.0);
      cl_add(cu, ell(e.x, e.y, 0.3, 0.18, t, 6), 0.14);
      o_st(o, ps, NULL, NOP, cu, NULL); }
    RET(o_out(o, lay, NULL));
}

/* corridor magboots: a pair of magnetic boots: sole plates, the upper, two straps, the open shaft */
BLD(b_corridor_magboots) {
    SoObj *o = OBJ("metal");
    for (int sv = -1; sv <= 1; sv += 2) {
        double v1 = u(0.95, 1.4); P2 c = pol(cx, cy, v1, a + sv * 1.57); double t = a + u(-0.7, 0.7); SoFrame f2 = frame(c.x, c.y, t); GG *sole = rrect(c.x, c.y, 2.8, 1.3, t, 0.45);
        int64_t b = o_up(o, sole, 0.4, NULL), up_, sh; P2 q = F(&f2, 0.05, 0), r1 = F(&f2, 0.3, 0), r2 = F(&f2, 0.95, 0), s = F(&f2, -0.7, 0); GG *dsh;
        up_ = o_on(o, rrect(q.x, q.y, 2.5, 1.06, t, 0.4), b, 0.4, NULL);
        o_on(o, U(glv(2, rect(r1.x, r1.y, 0.46, 1.06, t), rect(r2.x, r2.y, 0.46, 1.06, t))), up_, 0.14, NULL);
        dsh = disc(s.x, s.y, 0.54, 14); sh = o_on(o, dsh, up_, u(0.7, 1.2), NULL); o_cut(o, disc(s.x, s.y, 0.32, 10), sh, 0.5);
    }
    RET(o_out(o, lay, NULL));
}

/* corridor hose_reel: a hose reel lying flat: flange, hose rings round the hub, a crank, the free end to a nozzle */
BLD(b_corridor_hose_reel) {
    double R = u(1.9, 2.3); SoObj *o = OBJ("metal"); double t0 = u(0, 6.28); int64_t b = o_up(o, disc(cx, cy, R, 28), 0.34, NULL), hub; int k = 0; double r = R - 0.3;
    while (r > 1.1 && !E_) { o_on(o, ann(cx, cy, r, r - 0.46, 24), b, k % 2 == 0 ? 0.42 : 0.3, "cable"); r -= 0.48; k += 1; }
    hub = o_on(o, disc(cx, cy, 0.62, 12), b, 0.8, NULL); o_on(o, capl(p2(cx, cy), pol(cx, cy, 0.5, t0), 0.46), hub, 0.16, NULL);
    { double v1 = u(2.2, 3.6); double v2 = u(-0.4, 0.4); P2 e = pol(cx, cy, R + v1, a + v2); PT *pts = wiggle(pol(cx, cy, R + 0.3, a), e, 0.6, 4); SoSolids *ps = SL();
      rod(ps, pts, 0.3, 1.1, 0.0, 0.0); cone(ps, e.x, e.y, 0.42, 0.6, 0.26, 0.0, 16, 1.0, 0.0); o_st(o, ps, "cable", NOP, NULL, NULL); }
    RET(o_out(o, lay, NULL));
}

/* ====================================================================================================== sd_r_badlands.py */
/* (all nine are drawn again: sd_r_badlands2.py and sd_r_x_badlands.py) */

/* ====================================================================================================== sd_r_badlands2.py */
/* badlands longhorn_skull: brow to muzzle, eye sockets, nostrils, two long horns thinning to a point */
BLD(b_badlands_longhorn_skull) {
    SoFrame ff = frame(cx, cy, a); double L = u(3.0, 3.6); SoObj *o = OBJ("bone");
    GG *g = buf(buf(Poly(plv(6, F(&ff, -L / 2, -0.85), F(&ff, -L * 0.1, -0.6), F(&ff, L / 2, -0.36), F(&ff, L / 2, 0.36), F(&ff, -L * 0.1, 0.6), F(&ff, -L / 2, 0.85))), -0.2, 16), 0.2, 6);
    SoSolids *ps = SL(); CL *cu = cl(); P2 dc = F(&ff, -L * 0.28, 0);
    ext(ps, g, 0.5, 0.0); ext(ps, inset(g, 0.2), 0.75, 0.0); dome(ps, dc.x, dc.y, 0.6, 0.95, 0.0, 0.9, a, 20);
    for (int sv = -1; sv <= 1; sv += 2) { P2 p = F(&ff, -L * 0.12, sv * 0.42); cl_add(cu, disc(p.x, p.y, 0.22, 8), 0.3); }
    for (int sv = -1; sv <= 1; sv += 2) { P2 p = F(&ff, L * 0.4, sv * 0.15); cl_add(cu, ell(p.x, p.y, 0.18, 0.1, a, 6), 0.4); }
    cl_add(cu, barl(F(&ff, -L * 0.05, 0), F(&ff, L * 0.3, 0), 0.14), 0.62);
    o_st(o, ps, NULL, NOP, cu, NULL);
    for (int sv = -1; sv <= 1; sv += 2) {
        double k = rnd() < 0.2 ? 0.5 : 1.0; double sw = u(-0.3, 0.6);
        PT *pts = plv(4, F(&ff, -L * 0.4, sv * 0.6), F(&ff, -L * 0.45, sv * (0.6 + 1.1 * k)), F(&ff, -L * 0.36 + sw * 0.4, sv * (0.6 + 2.0 * k)), F(&ff, -L * 0.16 + sw, sv * (0.6 + 2.7 * k)));
        SoSolids *hs = SL(); P2 tip = pl_at(pts, 3);
        rod(hs, pl_slice(pts, 0, 2), 0.4, 1.0, 0.0, 0.0); rod(hs, pl_slice(pts, 1, 3), 0.33, 1.0, 0.0, 0.0); rod(hs, pl_slice(pts, 2, 4), 0.26, 1.0, 0.0, 0.0);
        if (k == 1.0) cone(hs, tip.x, tip.y, 0.26, 0.26, 0.2, 0.0, 16, 1.0, 0.0);
        o_st(o, hs, NULL, NOP, NULL, buf(Line(pts), 0.5, 16));
    }
    RET(o_out(o, lay, NULL));
}

/* badlands rattlesnake: the thick body in open rings, the wedge of a head on the inner coil, the rattle */
static int rs_head(void *c, double t, double *out) { (void)c; *out = 0.2 + 0.3 * MX(0.0, 1 - fabs(t + 0.15) / 0.75); return SO_OK; }
BLD(b_badlands_rattlesnake) {
    double t0 = u(0, 6.28); double turns = u(1.5, 1.9); double r0 = 1.0; PT *sp = spiral(cx, cy, r0, r0 + turns * 1.15, turns, t0, 40); SoObj *o = OBJ("hide");
    SoSolids *p1 = SL(), *p2s = SL(), *p3 = SL(); CL *cu = cl(); P2 h, e; double th, d2; SoFrame f2; double rr[3] = { 0.3, 0.26, 0.22 };
    rod(p1, sp, 0.42, 1.0, 0.0, 0.0); o_st(o, p1, NULL, NOP, NULL, disc(cx, cy, r0 + turns * 1.15 + 0.5, 20));
    h = pol(cx, cy, 0.15, t0 + 2.6); th = t0 + 2.6 + PI_; f2 = frame(h.x, h.y, th);
    rod(p2s, plv(3, pl_at(sp, 0), pl_at(sp, 1), h), 0.36, 1.1, 0.0, 0.0); spin(p2s, h.x, h.y, th, 1.2, rs_head, NULL, 1.3, 0.15, "round");
    for (int sv = -1; sv <= 1; sv += 2) { P2 q = F(&f2, -0.2, sv * 0.22); cl_add(cu, disc(q.x, q.y, 0.1, 5), 0.5); }
    o_st(o, p2s, NULL, NOP, cu, disc(h.x, h.y, 0.8, 20));
    e = pl_at(sp, -1); d2 = ATAN2(pl_at(sp, -1).y - pl_at(sp, -3).y, pl_at(sp, -1).x - pl_at(sp, -3).x);
    for (int k = 0; k < 3; k++) { P2 q = pol(e.x, e.y, 0.45 + k * 0.4, d2); dome(p3, q.x, q.y, rr[k], rr[k] * 1.3, 0.0, 1.0, 0.0, 20); }
    { P2 q = pol(e.x, e.y, 0.8, d2); o_st(o, p3, "bone", NOP, NULL, disc(q.x, q.y, 0.8, 20)); }
    RET(o_out(o, lay, NULL));
}

/* badlands gold_pan: a prospector's pan, rolled rim, riffle ring, gravel and a nugget or two; a poke of dust */
BLD(b_badlands_gold_pan) {
    double R = u(1.6, 2.0); SoObj *o = OBJ("tin"); int64_t b = o_up(o, disc(cx, cy, R, 28), 0.3, NULL); int n;
    o_on(o, ann(cx, cy, R, R - 0.46, 28), b, 0.34, NULL); o_on(o, ann(cx, cy, R - 0.58, R - 1.04, 24), b, 0.16, NULL);
    n = ri(3, 6);
    for (int i = 0; i < n; i++) {
        double v1 = u(0, R - 1.4); double v2 = u(0, 6.28); P2 p = pol(cx, cy, v1, v2); double r = u(0.25, 0.32); double hh = u(0.2, 0.32); SoSolids *ps = SL();
        rock(ps, p.x, p.y, r, hh, 0.0, 0.0, 9); o_st(o, ps, i < 2 ? "gold" : "rock", b, NULL, NULL);
    }
    { double v1 = u(1.0, 1.5); double v2 = u(0, 6.28); P2 c = pol(cx, cy, R + v1, v2); double t = u(0, 6.28); SoSolids *ps = SL(); P2 q = pol(c.x, c.y, 0.8, t);
      dome(ps, c.x, c.y, 0.62, 0.7, 0.0, 0.85, t, 20); rod(ps, plv(2, c, pol(c.x, c.y, 0.62, t)), 0.24, 1.0, 0.2, 0.0); dome(ps, q.x, q.y, 0.3, 0.42, 0.0, 1.0, 0.0, 20);
      o_st(o, ps, "leather", NOP, NULL, disc(c.x, c.y, 1.1, 20)); }
    RET(o_out(o, lay, NULL));
}

/* ====================================================================================================== sd_r_cynder.py */
/* _facet(poly, ax, ay, h, n=10, top=0.22): a faceted spire, the outline drawn in towards (ax, ay) as it rises */
static void cy_facet(SoSolids *o, const GG *poly, double ax, double ay, double h, int n, double top) {
    for (int k = 0; k < n; k++) { double f = 1 - (1 - top) * k / (n - 1); ext(o, scl_at(poly, f, f, ax, ay), h * (k + 1) / n, 0.0); }
}
/* _curl(p, t, L, bend, n=10): a line from p setting off towards t, bending by `bend` over its length L */
static PT *cy_curl(P2 p, double t, double L, double bend, int n) {
    PT *pts = pl(); pl_add(pts, p);
    for (int k = 0; k < n; k++) { double t2 = t + bend * (k + 0.5) / n; P2 q = pl_at(pts, -1); pl_add(pts, p2(q.x + L / n * COS(t2), q.y + L / n * SIN(t2))); }
    return pts;
}

/* cynder skylight: a lava tube's roof fallen in, the crust round the hole broken into lifted plates */
BLD(b_cynder_skylight) {
    double v = u(1.5, 2.1); GG *p = blob(cx, cy, v, 4); SoObj *o = OBJ("basalt"); int n = ri(6, 9), m; GG *c = cent(p); double ts[16]; GG *ring, *slits; GL *sl = gl(), *gs;
    for (int k = 0; k < n; k++) ts[k] = a + 6.2832 * k / n + u(-0.25, 0.25);
    o_gcut(o, p, u(0.6, 0.85), "lava"); v = u(1.3, 1.6); ring = dif(buf(p, v, 3), buf(p, 0.06, 16));
    for (int k = 0; k < n; k++) gl_add(sl, barl(p2(gx(c), gy(c)), pol(gx(c), gy(c), 9, ts[k]), 0.34));
    slits = U(sl); gs = polys(dif(ring, slits));
    for (int i = 0; i < gs->n; i++) {
        const GG *g = gs->v[i]; double h; SoSolids *ps;
        if (area(g) < 0.5 || rnd() < 0.12) continue;
        h = u(0.4, 0.8); ps = SL(); ext(ps, g, h * 0.55, 0.0); v = u(0.6, 0.9); ext(ps, isect(g, buf(p, v, 3)), h, 0.0);
        o_st(o, ps, NULL, NOP, NULL, g);
    }
    m = ri(0, 2);
    for (int i = 0; i < m; i++) { double v1 = u(0.0, 0.7); double v2 = u(0, 6.28); P2 q = pol(gx(c), gy(c), v1, v2); SoSolids *ps = SL(); double r = u(0.5, 0.7); ext(ps, stone(q.x, q.y, r, 0, 0, 0), 0.3, 0.0); o_st(o, ps, NULL, NOP, NULL, disc(q.x, q.y, 0.6, 20)); }
    RET(o_out(o, lay, NULL));
}

/* cynder basalt_columns: six-sided pillars packed side by side, tall in the middle, dished tops, broken stubs */
BLD(b_cynder_basalt_columns) {
    double r = u(0.68, 0.85); double s = r * 1.74; SoObj *o = OBJ("basalt"); int n = 0; double R = u(2.0, 2.8);
    for (int i = -3; i < 4; i++) for (int j = -3; j < 4; j++) {
        double X = (i + 0.5 * pmod(j, 2)) * s, Y = j * s * 0.866; P2 p = pol(cx, cy, py_hypot(X, Y), a + ATAN2(Y, X)); double dd = fdist(p, p2(cx, cy)), h, v1, v2; int64_t b;
        if (dd > R + u(-0.5, 0.3)) continue;
        v1 = u(1.3, 3.2); v2 = u(-0.05, 0.05); h = MX(0.35, py_rint(v1 * (1 - dd / (R * 1.5)) / 0.35) * 0.35 + v2);
        b = o_up(o, ngon(p.x, p.y, r - 0.13, 6, a + 0.5236), h, NULL); n += 1;
        if (rnd() < 0.35) o_cut(o, ngon(p.x, p.y, r * 0.5, 6, a + 0.5236), b, 0.16);
        else if (rnd() < 0.3) { double t = u(0, 6.28); o_cut(o, isect(Poly(plv(3, pol(p.x, p.y, 2 * r, t - 1.2), pol(p.x, p.y, 0.1, t + 3.14), pol(p.x, p.y, 2 * r, t + 1.2))), ngon(p.x, p.y, r - 0.13, 6, a + 0.5236)), b, MN(0.3, h * 0.5)); }
    }
    if (!n) RETNONE;
    RET(o_out(o, lay, NULL));
}

/* cynder ropy_lava: a tongue of flow frozen with its skin pushed up into bowed ropes */
typedef struct { double W, L; } RopyC;
static int ropy_body(void *c, double t, double *out) { RopyC *k = (RopyC *)c; double m = MN(1.0, MX(0.0, (t + k->L / 2 + 0.3) / (k->L + 0.6))); *out = k->W * fpw(SIN(PI_ * fpw(m, 0.8)), 0.5) + 0.02; return SO_OK; }
static int ropy_rope(void *c, double q, double *out) { (void)c; *out = 0.4 - 0.1 * fpw(fabs(2 * q - 1), 2); return SO_OK; }
BLD(b_cynder_ropy_lava) {
    SoFrame ff = frame(cx, cy, a); int n = ri(5, 7); double st = u(0.9, 1.0); double W = u(1.9, 2.6); double L = n * st; double R0_ = u(1.8, 2.6); SoSolids *ps = SL(); GL *ends = gl(); double sk = u(-0.3, 0.3);
    RopyC rc = { W, L };
    spin(ps, cx, cy, a, L + 0.6, ropy_body, &rc, 0.2, 0.0, "round");
    for (int k = 0; k < n; k++) {
        double x = -L / 2 + (k + 0.5) * st, wl = W * fpw(SIN(PI_ * fpw((k + 0.5) / n, 0.8)), 0.5) * 0.92, Rk = MX(R0_, wl * 1.05), half = py_asin(MN(1.0, wl / Rk));
        P2 c = F(&ff, x - Rk, sk * (k - n / 2.0) * 0.2); PT *pts = arc(c.x, c.y, Rk, a - half, a + half, 12); P2 e0 = pl_at(pts, 0), e1 = pl_at(pts, -1);
        rodf(ps, pts, ropy_rope, NULL, 1.1, 0.12, 0.0); gl_add(ends, disc(e0.x, e0.y, 0.5, 20)); gl_add(ends, disc(e1.x, e1.y, 0.5, 20));
    }
    { GG *f = hull(ends); push(lay, ST(ps, "basalt", NOP, NULL)); RET(f); }
}

/* cynder fumarole: a vent built up in terraces, the open throat, sulphur crystals on the rim, blow holes beside */
BLD(b_cynder_fumarole) {
    double r = u(1.7, 2.3); SoObj *o = OBJ("basalt"); double asp = u(0.8, 0.95); GG *g = stone(cx, cy, r, asp, 9, 0); double v1 = u(0.0, 0.2); double v2 = u(0, 6.28); P2 c = pol(cx, cy, r * v1, v2); int m = ri(3, 4), nn;
    int64_t b = o_up(o, g, u(0.3, 0.4), NULL); double k = 1.0, rt;
    for (int i = 0; i < m; i++) { GG *gi; k *= u(0.7, 0.8); gi = scl_at(g, k, k, c.x, c.y); gi = rot_at(gi, u(-0.3, 0.3), c.x, c.y); b = o_on(o, gi, b, u(0.26, 0.4), NULL); }
    rt = MX(0.42, r * k * 0.5); o_cut(o, disc(c.x, c.y, rt, 10), b, 0.5);
    nn = ri(4, 7);
    for (int i = 0; i < nn; i++) {
        double w1 = u(0.5, 0.95); double w2 = u(0, 6.28); P2 q = pol(c.x, c.y, r * w1, w2); double rr = u(0.36, 0.5); double t = u(0, 1.5); GG *ng = ngon(q.x, q.y, rr, 4, t); double hh = u(0.6, 1.1); SoSolids *ps = SL();
        cy_facet(ps, ng, q.x, q.y, hh, 6, 0.5); o_st(o, ps, "ember", NOP, NULL, disc(q.x, q.y, 0.4, 20));
    }
    nn = ri(1, 2);
    for (int i = 0; i < nn; i++) {
        double w1 = u(0.9, 1.4); double w2 = u(0, 6.28); P2 q = pol(cx, cy, r + w1, w2); double rr = u(0.36, 0.48); SoSolids *ps = SL(); GG *an = ann(q.x, q.y, rr + 0.42, rr, 10);
        ext(ps, an, u(0.3, 0.4), 0.0); o_st(o, ps, NULL, NOP, NULL, disc(q.x, q.y, rr + 0.4, 20)); o_gcut(o, disc(q.x, q.y, rr - 0.03, 10), 0.5, "pit");
    }
    RET(o_out(o, lay, NULL));
}

/* cynder lava_bomb: a spindle of rock with twisted ends, its crust cracked, in its dent, a thrown-up lip, a splash */
static int lb_lip(void *c, double q, double *out) { (void)c; *out = 0.26 + 0.24 * SIN(q * PI_); return SO_OK; }
typedef struct { double r, L; } LbC;
static int lb_body(void *c, double t, double *out) { LbC *k = (LbC *)c; *out = k->r * fpw(MX(0.0, 1 - fpw(2 * t / k->L, 2)), 0.75) + 0.2 * MX(0.0, 1 - fabs(fabs(t) - k->L * 0.42) / 0.25) + 0.06; return SO_OK; }
BLD(b_cynder_lava_bomb) {
    SoFrame ff = frame(cx, cy, a); double L = u(3.2, 4.2); double r = u(1.0, 1.3); SoObj *o = OBJ("basalt"); double tw = u(-0.5, 0.5); double ls[12], xs[64], v1, v2, st; PT *lip = pl(); CL *cu = cl(); SoSolids *ps; int nx, nr; LbC lc = { r, L };
    o_gcut(o, ell(cx, cy, L / 2 + 0.5, r + 0.6, a, 18), 0.3, "scorch");
    v1 = u(-0.4, 0.2); v2 = u(1.9, 2.6); linspace(v1, v2, 12, ls);
    for (int i = 0; i < 12; i++) pl_add(lip, F(&ff, (L / 2 + 0.75) * COS(ls[i]), (r + 0.85) * SIN(ls[i])));
    ps = SL(); rodf(ps, lip, lb_lip, NULL, 0.9, 0.0, 0.0); o_st(o, ps, NULL, NOP, NULL, buf(Line(lip), 0.5, 16));
    st = u(0.75, 1.0); nx = arange(-L * 0.28, L * 0.3, st, xs, 64);
    for (int i = 0; i < nx; i++) cl_add(cu, wavy(F(&ff, xs[i] - 0.5, -r), F(&ff, xs[i] + 0.5 + tw, r), 0.25, 3, 0.32), r * 0.5);
    v1 = u(-0.2, 0.2); v2 = u(-0.2, 0.2); cl_add(cu, wavy(F(&ff, -L * 0.3, v1), F(&ff, L * 0.3, v2), 0.2, 4, 0.3), r * 0.5);
    ps = SL(); spin(ps, cx, cy, a, L, lb_body, &lc, 1.0, 0.0, "round"); o_st(o, ps, NULL, NOP, cu, rect(cx, cy, L, 2 * r, a));
    nr = ri(2, 5);
    for (int i = 0; i < nr; i++) { double w1 = u(-L / 2, L / 2); double w2 = u(1.5, 2.2); P2 q = F(&ff, w1, r + w2); double rr = u(0.32, 0.45); double hh = u(0.3, 0.45); SoSolids *p3 = SL(); rock(p3, q.x, q.y, rr, hh, 0.0, 0.0, 9); o_st(o, p3, NULL, NOP, NULL, disc(q.x, q.y, 0.4, 20)); }
    RET(o_out(o, lay, NULL));
}

/* cynder anvil: the foot, the body, the face with its hardy and punch holes, the horn; a hammer by it */
typedef struct { double W, Lh; } AnC;
static int an_horn(void *c, double t, double *out) { AnC *k = (AnC *)c; *out = k->W * 0.47 * fpw(MX(0.0, 1 - (t + k->Lh / 2 + 0.1) / (k->Lh + 0.25)), 0.75) + 0.14; return SO_OK; }
BLD(b_cynder_anvil) {
    SoFrame ff = frame(cx, cy, a); double L = u(2.5, 3.0); double W = u(1.25, 1.5); double h = u(1.5, 2.0); SoObj *o = OBJ("iron"); double Lh = u(1.5, 1.9); int64_t ft, fc; P2 hc, q1, q2; SoSolids *ps; AnC ac = { W, Lh }; double hk;
    ft = o_up(o, rect(cx, cy, L * 0.8, W + 0.9, a), h * 0.3, NULL); o_cut(o, rect(cx, cy, L * 0.36, W + 2, a), ft, h * 0.12);
    hc = F(&ff, L / 2 + Lh / 2 - 0.1, 0); ps = SL(); hk = (h * 0.5 - u(0.22, 0.3)) / (W * 0.47 + 0.14);
    spin(ps, hc.x, hc.y, a, Lh + 0.2, an_horn, &ac, hk, h * 0.5, "round"); o_st(o, ps, NULL, NOP, NULL, rect(hc.x, hc.y, Lh, W, a));
    fc = o_up(o, rect(cx, cy, L, W, a), h, NULL); q1 = F(&ff, -L * 0.34, 0); q2 = F(&ff, -L * 0.34 + 0.55, W * 0.2);
    o_cut(o, U(glv(2, rect(q1.x, q1.y, 0.42, 0.42, a), disc(q2.x, q2.y, 0.2, 6))), fc, 0.35);
    if (rnd() < 0.8) {
        double v1 = u(-0.8, 1.2); int ch = rchi(2, -1, 1); double v2 = u(1.5, 2.0); P2 c = F(&ff, v1, ch * (W / 2 + v2)); double t = a + u(-0.7, 0.7); SoFrame f2 = frame(c.x, c.y, t); P2 m = F(&f2, 1.0, 0);
        ps = SL(); ext(ps, capl(F(&f2, -0.2, 0), F(&f2, 2.3, 0), 0.56), 0.4, 0.0); o_st(o, ps, "wood", NOP, NULL, rect(m.x, m.y, 2.6, 0.5, t)); o_up(o, rect(c.x, c.y, 0.95, 1.5, t), 0.75, NULL);
    }
    RET(o_out(o, lay, NULL));
}

/* cynder crucible: a thick pot with its lip and lugs, the melt under dross (or spilt), the tongs beside it */
BLD(b_cynder_crucible) {
    double r = u(1.15, 1.45); double h = u(1.3, 1.7); SoObj *o = OBJ("iron"); SoFrame ff = frame(cx, cy, a); int64_t b; int n; double tc, t, op; P2 c; SoFrame f2; SoSolids *ps;
    o_up(o, Poly(plv(4, F(&ff, r * 0.5, -r * 0.62), F(&ff, r + 0.62, -0.2), F(&ff, r + 0.62, 0.2), F(&ff, r * 0.5, r * 0.62))), h, NULL);
    for (int sv = -1; sv <= 1; sv += 2) { P2 q = F(&ff, -r * 0.1, sv * (r + 0.1)); o_up(o, rect(q.x, q.y, 0.95, 0.9, a), h * 0.7, NULL); }
    b = o_up(o, disc(cx, cy, r, 24), h, NULL);
    { GG *cu = U(glv(2, disc(cx, cy, r - 0.38, 18), barl(F(&ff, r * 0.6, 0), F(&ff, r + 0.45, 0), 0.34))); o_cut(o, cu, b, u(0.3, 0.5)); }
    n = ri(1, 3);
    for (int i = 0; i < n; i++) { double v1 = u(0, r * 0.3); double v2 = u(0, 6.28); P2 q = pol(cx, cy, v1, v2); double rr = u(0.3, 0.42); ps = SL(); ext(ps, stone(q.x, q.y, rr, 0, 0, 0), 0.2, 0.0); o_st(o, ps, "ember", b, NULL, NULL); }
    if (rnd() < 0.45) { double v = u(-0.3, 0.3); P2 q = F(&ff, r + 1.3, v); double rr = u(0.8, 1.1); double sq = u(0.6, 0.9); ps = SL(); dome(ps, q.x, q.y, rr, 0.36, 0.0, sq, a, 20); o_st(o, ps, "ember", NOP, NULL, disc(q.x, q.y, 1.0, 20)); }
    { int ch = rchi(2, -1, 1); tc = a + ch * u(1.5, 2.6); }
    { double v = u(1.7, 2.1); c = pol(cx, cy, r + v, tc); }
    { double v = u(-0.3, 0.3); t = tc + 1.5708 + v + (rnd() < 0.5 ? PI_ : 0); }
    f2 = frame(c.x, c.y, t); c = F(&f2, 0.5, 0); f2 = frame(c.x, c.y, t); op = u(0.2, 0.4); ps = SL();
    for (int sv = -1; sv <= 1; sv += 2) ext(ps, line(plv(5, F(&f2, -2.6, sv * 0.42), F(&f2, 0, -sv * 0.05), F(&f2, 0.5, -sv * (0.4 + op)), F(&f2, 1.1, -sv * (0.55 + op)), F(&f2, 1.6, -sv * (0.2 + op))), 0.56), 0.36, 0.0);
    { P2 m = F(&f2, -0.5, 0); o_st(o, ps, NULL, NOP, NULL, rect(m.x, m.y, 4.4, 1.8, t)); }
    ps = SL(); ext(ps, disc(c.x, c.y, 0.42, 8), 0.56, 0.0); o_st(o, ps, NULL, NOP, NULL, disc(c.x, c.y, 0.4, 20));
    RET(o_out(o, lay, NULL));
}

/* cynder ingots: cast ingots stacked, each with its stamp; a row, a row across, one thrown beside */
static void cy_ingot(SoObj *o, double Li, double Wi, P2 c, double t, double z) {
    GG *g = rect(c.x, c.y, Li, Wi, t); int64_t b = o_up(o, g, z + 0.42, NULL), tp; SoFrame f2 = frame(c.x, c.y, t); double v; P2 q;
    tp = o_on(o, rect(c.x, c.y, Li - 0.4, Wi - 0.4, t), b, 0.14, NULL);
    v = u(-0.5, 0.5); q = F(&f2, v, 0); o_cut(o, rect(q.x, q.y, 0.6, 0.27, t), tp, 0.12);
}
BLD(b_cynder_ingots) {
    SoFrame ff = frame(cx, cy, a); int n = ri(2, 4), m; SoObj *o = OBJ("iron"); double Li = u(2.5, 2.9); double Wi = 1.0, st = Wi + 0.27;
    for (int i = 0; i < n; i++) { double v = u(-0.08, 0.08); P2 c = F(&ff, v, (i - (n - 1) / 2.0) * st); double t = a + u(-0.03, 0.03); cy_ingot(o, Li, Wi, c, t, 0.0); }
    m = n > 2 ? ri(0, n - 1 < 2 ? n - 1 : 2) : ri(0, 1);
    for (int i = 0; i < m; i++) { double v1 = u(-0.1, 0.1); double v2 = u(-0.2, 0.2); P2 c = F(&ff, (i - (m - 1) / 2.0) * st + v1, v2); double t = a + 1.5708 + u(-0.12, 0.12); cy_ingot(o, Li, Wi, c, t, 0.56); }
    if (rnd() < 0.6) { double v1 = u(1.0, 1.5); double v2 = u(-1.0, 1.0); P2 c = F(&ff, Li / 2 + v1, v2); double t = a + u(0.3, 2.8); cy_ingot(o, Li, Wi, c, t, 0.0); }
    RET(o_out(o, lay, NULL));
}

/* cynder obsidian_blades: glassy blades standing out of the ground, each a spire of flat faces; flakes round them */
BLD(b_cynder_obsidian_blades) {
    SoObj *o = OBJ("obsidian"); int n = ri(3, 5), m; double t0 = u(0, 6.28);
    for (int i = 0; i < n; i++) {
        double t = t0 + 6.2832 * i / n + u(-0.4, 0.4); double dd = i == 0 ? 0.0 : u(1.3, 2.1); P2 c = pol(cx, cy, dd, t); double L = u(2.0, 3.0) * (i == 0 ? 1.15 : 0.85); double w = L * u(0.4, 0.52);
        SoFrame f2 = frame(c.x, c.y, t + u(-0.6, 0.6)); double h = L * u(0.85, 1.15); double v1 = u(-0.2, 0.2); P2 a1 = F(&f2, -L / 2, v1), a2 = F(&f2, -L * 0.15, w / 2); double v2 = u(-0.1, 0.3); P2 a3 = F(&f2, L / 2, v2), a4 = F(&f2, L * 0.1, -w / 2);
        GG *g = Poly(plv(4, a1, a2, a3, a4)); double w1 = u(-0.05, 0.3); double w2 = u(-0.1, 0.1); P2 ap = F(&f2, L * w1, w2); SoSolids *ps = SL();
        cy_facet(ps, g, ap.x, ap.y, MN(3.3, h), 12, 0.2); o_st(o, ps, NULL, NOP, NULL, g);
    }
    m = ri(2, 5);
    for (int i = 0; i < m; i++) {
        double v1 = u(2.4, 3.6); double v2 = u(0, 6.28); P2 c = pol(cx, cy, v1, v2); double t = u(0, 6.28); double r1 = u(0.5, 0.8); double r2 = u(0.35, 0.5); double r3 = u(0.35, 0.5);
        GG *g = Poly(plv(3, pol(c.x, c.y, r1, t), pol(c.x, c.y, r2, t + 2.2), pol(c.x, c.y, r3, t + 4.1))); SoSolids *ps = SL(); ext(ps, g, u(0.26, 0.36), 0.0); o_st(o, ps, NULL, NOP, NULL, g);
    }
    RET(o_out(o, lay, NULL));
}

/* cynder charred_hand: a burnt arm reaching out of the ash: forearm, wrist, hand, four curled fingers, the thumb */
static int ch_arm(void *c, double q, double *out) { *out = (0.62 - 0.14 * q) * *(double *)c; return SO_OK; }
static int ch_finger(void *c, double q, double *out) { *out = (0.33 - 0.05 * q + 0.07 * fpw(MX(0.0, COS(q * 6.2832 * 1.5)), 4)) * *(double *)c; return SO_OK; }
static int ch_thumb(void *c, double q, double *out) { *out = (0.38 - 0.06 * q) * *(double *)c; return SO_OK; }
BLD(b_cynder_charred_hand) {
    SoFrame ff = frame(cx, cy, a); double k = u(1.0, 1.25); SoObj *o = OBJ("ash"); double v = u(-0.6, 0.6); P2 e = F(&ff, -3.4 * k, v); SoSolids *ps = SL(); double sq = u(0.7, 0.95); double aa = u(0, 3); P2 d1, d2, m; int sv; PT *pts;
    dome(ps, e.x, e.y, 1.5 * k, 0.5, 0.0, sq, aa, 20); o_st(o, ps, NULL, NOP, NULL, disc(e.x, e.y, 1.4 * k, 20));
    ps = SL(); d1 = F(&ff, -0.95 * k, 0); d2 = F(&ff, 0.0, 0); m = F(&ff, -1.6 * k, 0);
    rodf(ps, plv(2, e, F(&ff, -1.0 * k, 0)), ch_arm, &k, 1.0, 0.0, 0.0); dome(ps, d1.x, d1.y, 0.56 * k, 0.6 * k, 0.0, 1.0, 0.0, 20); dome(ps, d2.x, d2.y, 0.9 * k, 0.62 * k, 0.0, 0.85, a, 20);
    o_st(o, ps, NULL, NOP, NULL, rect(m.x, m.y, 4.0 * k, 1.8 * k, a));
    for (int i = 0; i < 4; i++) {
        P2 p0 = F(&ff, 0.6 * k, (i - 1.5) * 0.5 * k); double L = (0 < i && i < 3 ? u(1.7, 2.1) : u(1.3, 1.6)) * k; double t = a + (i - 1.5) * 0.36 + u(-0.08, 0.08); double b1 = u(0.2, 0.7); double b2 = u(-0.3, 0.3);
        pts = cy_curl(p0, t, L, (i - 1.5) * b1 + b2, 9); ps = SL(); rodf(ps, pts, ch_finger, &k, 1.1, 0.0, 0.0); o_st(o, ps, NULL, NOP, NULL, buf(Line(pts), 0.4, 16));
    }
    sv = rchi(2, -1, 1);
    { double t = a + sv * u(0.9, 1.3); double b1 = u(0.0, 0.7); pts = cy_curl(F(&ff, -0.3 * k, sv * 0.7 * k), t, 1.4 * k, -sv * b1, 6); }
    ps = SL(); rodf(ps, pts, ch_thumb, &k, 1.1, 0.0, 0.0); o_st(o, ps, NULL, NOP, NULL, buf(Line(pts), 0.4, 16));
    RET(o_out(o, lay, NULL));
}

/* cynder horn_spur: a horn out of the rock, ringed at the root in heaved ground, curving over to its point */
typedef struct { double r0, nr; } HsC;
static int hs_horn(void *c, double q, double *out) { HsC *k = (HsC *)c; *out = k->r0 * (1 - 0.74 * q) * (0.87 + 0.13 * fabs(SIN(q * PI_ * k->nr))); return SO_OK; }
BLD(b_cynder_horn_spur) {
    SoObj *o = OBJ("tooth"); int n = rnd() < 0.45 ? 2 : 1;
    for (int i = 0; i < n; i++) {
        double k = i == 0 ? 1.0 : u(0.55, 0.7); P2 p; double r0, L, t, bend, nr, hk; PT *pts; SoSolids *ps; HsC hc; GG *s1, *s2;
        if (i == 0) p = p2(cx, cy); else { double v1 = u(1.8, 2.4); int ch = rchi(2, -1, 1); double v2 = u(1.2, 1.9); p = pol(cx, cy, v1, a + ch * v2); }
        r0 = u(0.85, 1.05) * k; L = u(4.0, 5.2) * k;
        { double v1 = u(-0.3, 0.3); double v2 = u(-0.8, 0.8); t = a + v1 + i * v2; }
        { int ch = rchi(2, -1, 1); bend = ch * u(1.0, 2.0); }
        pts = cy_curl(p, t, L, bend, 14); nr = L / 0.5;
        ps = SL(); s1 = stone(p.x, p.y, r0 + 0.7, 0.9, 8, 0); ext(ps, s1, 0.3, 0.0); s2 = stone(p.x, p.y, r0 + 0.35, 0.9, 7, 0); ext(ps, s2, 0.5, 0.0);
        o_st(o, ps, "basalt", NOP, NULL, disc(p.x, p.y, r0 + 0.7, 20));
        hc.r0 = r0; hc.nr = nr; hk = u(1.5, 2.1); ps = SL(); rodf(ps, pts, hs_horn, &hc, hk, 0.0, 0.0); o_st(o, ps, NULL, NOP, NULL, buf(Line(pts), r0, 16));
    }
    RET(o_out(o, lay, NULL));
}

/* cynder slag_heap: clinker heaped on a spread of ash, small lumps rolled out, a tongue of glassy slag */
BLD(b_cynder_slag_heap) {
    double r = u(2.0, 2.7); SoObj *o = OBJ("ash"); GG *g = blob(cx, cy, r, 5); GG *c = cent(g); SoSolids *ps = SL(); int n; double X = gx(c), Y = gy(c);
    ext(ps, g, 0.3, 0.0); ext(ps, scl_at(g, 0.7, 0.7, X, Y), 0.55, 0.0); o_st(o, ps, NULL, NOP, NULL, g);
    ps = SL(); n = ri(4, 6);
    for (int i = 0; i < n; i++) { double v1 = u(0, r * 0.35); double v2 = u(0, 6.28); P2 q = pol(X, Y, v1, v2); double rr = u(0.65, 0.95); double hh = u(0.8, 1.3); rock(ps, q.x, q.y, rr, hh, 0.0, 0.4, 9); }
    o_st(o, ps, "basalt", NOP, NULL, disc(X, Y, r * 0.6, 20));
    n = ri(6, 10);
    for (int i = 0; i < n; i++) { double t = u(0, 6.28); double v = u(0.55, 1.1); P2 q = pol(X, Y, r * v, t); double rr = u(0.4, 0.62); double hh = u(0.45, 0.75); SoSolids *p3 = SL(); rock(p3, q.x, q.y, rr, hh, 0.0, 0.0, 9); o_st(o, p3, "basalt", NOP, NULL, disc(q.x, q.y, 0.5, 20)); }
    n = ri(1, 2);
    for (int i = 0; i < n; i++) { double v1 = u(0.5, 0.9); double v2 = u(0, 6.28); P2 q = pol(X, Y, r * v1, v2); double rr = u(0.6, 0.85); SoSolids *p3 = SL(); ext(p3, stone(q.x, q.y, rr, 0.6, 0, 0), 0.62, 0.0); o_st(o, p3, "obsidian", NOP, NULL, disc(q.x, q.y, 0.7, 20)); }
    RET(o_out(o, lay, NULL));
}

/* cynder forge_bellows: the bottom board, the leather fold on fold, the nailed top board, handles and nozzle */
static int fb_nozzle(void *c, double t, double *out) { (void)c; *out = 0.52 - 0.12 * (t + 1.0) + (fabs(t + 0.2) < 0.16 ? 0.12 : 0.0); return SO_OK; }
BLD(b_cynder_forge_bellows) {
    SoFrame ff = frame(cx, cy, a); double L = u(3.8, 4.8); double R = u(1.4, 1.75); SoObj *o = OBJ("leather"); int nf = ri(2, 4); P2 c0 = F(&ff, -L / 2 + R, 0), c1 = F(&ff, L / 2 - 0.5, 0), nz;
    GG *g = hull(glv(2, disc(c0.x, c0.y, R, 24), disc(c1.x, c1.y, 0.55, 10))), *gt, *ex; int64_t b, tp; double ins; SoSolids *ps; PT *st;
    for (int sv = -1; sv <= 1; sv += 2) {
        double v1 = u(1.2, 1.6); double v2 = u(0.45, 0.8); P2 h0 = F(&ff, -L / 2 + 0.5, sv * 0.5), h1 = F(&ff, -L / 2 - v1, sv * v2);
        ps = SL(); ext(ps, capl(h0, h1, 0.6), sv < 0 ? 0.3 : 0.3 + 0.2 * (nf + 1), 0.0); o_st(o, ps, "wood", NOP, NULL, buf(Line(plv(2, h0, h1)), 0.4, 16));
    }
    nz = F(&ff, L / 2 + 0.6, 0); ps = SL(); spin(ps, nz.x, nz.y, a, 2.0, fb_nozzle, NULL, 1.05, 0.0, "round"); o_st(o, ps, "iron", NOP, NULL, rect(nz.x, nz.y, 2.0, 1.0, a));
    b = o_up(o, g, 0.3, "wood"); ins = 0.16;
    for (int i = 0; i < nf; i++) { b = o_on(o, buf(g, -ins, 8), b, 0.2, NULL); ins += u(0.15, 0.2); }
    gt = buf(g, -ins - 0.05, 8); tp = o_on(o, gt, b, 0.24, "wood"); ex = exter(buf(gt, -0.32, 8)); st = pl();
    for (int i = 0; i < 10; i++) { GG *q = interp(ex, i / 10.0, 1); pl_add(st, p2(gx(q), gy(q))); }
    o_studs(o, st, tp, 0.25, 0.13, "iron", 8);
    { double v = u(-0.2, 0.3); P2 q = F(&ff, -L / 2 + R + v, 0); o_cut(o, disc(q.x, q.y, 0.34, 8), tp, 0.2); }
    RET(o_out(o, lay, NULL));
}

/* cynder ember_moat: a standing stone of flat faces ringed by a lava channel, a crust lip, one causeway */
static int em_lip(void *c, double q, double *out) { (void)c; *out = 0.3 + 0.16 * SIN(q * PI_); return SO_OK; }
BLD(b_cynder_ember_moat) {
    double R = u(2.3, 3.0); double w = u(0.7, 1.0); SoObj *o = OBJ("basalt"); double gapw = u(0.5, 0.9); double t0 = a + gapw; GG *g, *st; double v1, v2, v3, v4, v5; int nv;
    g = dif(ann(cx, cy, R, R - w, 24), Poly(plv(4, p2(cx, cy), pol(cx, cy, 2 * R, a), pol(cx, cy, 2 * R, a + gapw / 2), pol(cx, cy, 2 * R, a + gapw))));
    o_gcut(o, g, 0.5, "lava");
    for (int k = 0; k < 3; k++) {
        double s0; PT *pts; SoSolids *ps;
        if (rnd() < 0.25) continue;
        s0 = t0 + 0.25 + k * (6.2832 - gapw - 0.5) / 3; pts = arc(cx, cy, R + 0.45, s0, s0 + (6.2832 - gapw - 0.5) / 3 - 0.3, 10);
        ps = SL(); rodf(ps, pts, em_lip, NULL, 0.9, 0.0, 0.0); o_st(o, ps, NULL, NOP, NULL, buf(Line(pts), 0.4, 16));
    }
    v1 = u(0.95, 1.25); v2 = u(0.6, 0.85); nv = ri(5, 6); st = stone(cx, cy, v1, v2, nv, 0);
    v3 = u(-0.3, 0.3); v4 = u(-0.3, 0.3); v5 = u(1.6, 2.5);
    { SoSolids *ps = SL(); cy_facet(ps, st, cx + v3, cy + v4, v5, 12, 0.3); o_st(o, ps, NULL, NOP, NULL, st); }
    RET(o_out(o, lay, NULL));
}

/* cynder sulfur_crust: a cracked plate of yellow scale round a blow hole, clumps of four-sided crystals on it */
BLD(b_cynder_sulfur_crust) {
    double r = u(1.8, 2.4); SoObj *o = OBJ("ember"); double asp = u(0.75, 0.95); GG *g = stone(cx, cy, r, asp, 9, 0); int64_t b = o_up(o, g, 0.3, NULL); double vt = u(0, 6.28); P2 v = pol(cx, cy, r * 0.45, vt); double ts[3]; GL *cr = gl(); int nj;
    ts[0] = a; ts[1] = a + u(1.6, 2.4); ts[2] = a + u(3.6, 4.6);
    gl_add(cr, disc(v.x, v.y, 0.42, 8));
    for (int k = 0; k < 3; k++) gl_add(cr, wavy(v, pol(cx, cy, r * 1.2, ts[k]), 0.3, 3, 0.26));
    o_cut(o, isect(U(cr), g), b, 0.28);
    nj = ri(2, 3);
    for (int j = 0; j < nj; j++) {
        double v1 = u(0.2, 0.6); double t = ATAN2(v.y - cy, v.x - cx) + 2.0 + j * 1.9 + u(-0.3, 0.3); P2 c = pol(cx, cy, r * v1, t); int ni = ri(2, 4);
        for (int i = 0; i < ni; i++) {
            double w1 = i == 0 ? 0 : u(0.5, 0.8); double w2 = u(0, 6.28); P2 q = pol(c.x, c.y, w1, w2); double rr = i == 0 ? u(0.65, 0.85) : u(0.45, 0.58); double ta = u(0, 1.5); GG *ng = ngon(q.x, q.y, rr, 4, ta);
            double x1 = u(-0.15, 0.15); double y1 = u(-0.15, 0.15); double hh = rr * u(2.2, 2.8); SoSolids *ps = SL();
            cy_facet(ps, ng, q.x + x1, q.y + y1, hh, 8, 0.3); o_st(o, ps, NULL, NOP, NULL, disc(q.x, q.y, rr, 20));
        }
    }
    RET(o_out(o, lay, NULL));
}

/* cynder shackles: a plate bolted to the rock, staple and ring, chains of alternating links ending in open cuffs */
BLD(b_cynder_shackles) {
    SoFrame ff = frame(cx, cy, a); SoObj *o = OBJ("iron"); GG *g = rrect(cx, cy, 2.0, 1.7, a, 0.3); int64_t b = o_up(o, g, 0.36, NULL); PT *st = pl(); P2 rc; SoSolids *ps; int nj;
    for (int sx = -1; sx <= 1; sx += 2) for (int sy = -1; sy <= 1; sy += 2) pl_add(st, F(&ff, sx * 0.68, sy * 0.53));
    o_studs(o, st, b, 0.25, 0.14, NULL, 8); o_on(o, rect(cx, cy, 0.5, 1.0, a), b, 0.4, NULL);
    rc = F(&ff, 0.85, 0); ps = SL(); ext(ps, ann(rc.x, rc.y, 0.9, 0.38, 14), 0.32, 0.0); o_st(o, ps, NULL, NOP, NULL, disc(rc.x, rc.y, 0.9, 20));
    nj = ri(1, 2);
    for (int j = 0; j < nj; j++) {
        double t, t2 = 0, bend; int n; PT *pts; P2 e, kq;
        if (j == 0) t = a + u(-0.5, 0.5); else { int ch = rchi(2, -1, 1); t = a + ch * u(0.9, 1.4); }
        n = ri(3, 5); bend = u(-1.4, 1.4); pts = cy_curl(pol(rc.x, rc.y, 0.95, t), t, n * 1.0, bend, n);
        for (int i = 0; i < n; i++) {
            P2 p = pl_at(pts, i), q = pl_at(pts, i + 1), m = p2((p.x + q.x) / 2, (p.y + q.y) / 2); t2 = ATAN2(q.y - p.y, q.x - p.x); ps = SL();
            if (i % 2 == 0) { ext(ps, capl(pol(m.x, m.y, 0.36, t2), pol(m.x, m.y, 0.36, t2 + PI_), 0.52), 0.56, 0.0); o_st(o, ps, NULL, NOP, NULL, disc(m.x, m.y, 0.6, 20)); }
            else { CL *cu = cl(); ext(ps, ell(m.x, m.y, 0.78, 0.5, t2, 12), 0.32, 0.0); cl_add(cu, ell(m.x, m.y, 0.36, 0.15, t2, 8), 0.12); o_st(o, ps, NULL, NOP, cu, disc(m.x, m.y, 0.7, 20)); }
        }
        e = pol(pl_at(pts, -1).x, pl_at(pts, -1).y, 0.95, t2);
        { double v = u(-0.4, 0.4); GG *gp = Poly(plv(3, e, pol(e.x, e.y, 2, t2 + v - 0.3), pol(e.x, e.y, 2, t2 + 0.5)));
          ps = SL(); ext(ps, dif(ann(e.x, e.y, 1.1, 0.56, 16), gp), 0.5, 0.0); o_st(o, ps, NULL, NOP, NULL, disc(e.x, e.y, 1.1, 20)); }
        kq = pol(e.x, e.y, 1.0, t2 + PI_); ps = SL(); ext(ps, disc(kq.x, kq.y, 0.42, 8), 0.68, 0.0); o_st(o, ps, NULL, NOP, NULL, disc(kq.x, kq.y, 0.4, 20));
    }
    RET(o_out(o, lay, NULL));
}

/* cynder imp_idol: a squat imp on its plinth, horns (one broken on some), eyes and mouth, an offering bowl */
BLD(b_cynder_imp_idol) {
    SoFrame ff = frame(cx, cy, a); double r = u(1.15, 1.4); double h = u(1.3, 1.6); SoObj *o = OBJ("basalt"); double S = r * 2 + u(1.5, 1.9); P2 pc = F(&ff, 0.35, 0), hd; int64_t b, bw; double rh, hh; int gone;
    SoSolids *body = SL(), *lw = SL(); CL *cu = cl();
    b = o_up(o, rect(pc.x, pc.y, S, S * 0.9, a), 0.45, NULL); o_groove(o, inset(rect(pc.x, pc.y, S, S * 0.9, a), 0.26), b, 0.26, 0.16);
    hd = F(&ff, 0.15, 0); rh = r * 0.66; hh = rh * 1.2;
    turn(body, cx, cy, plv(4, p2(r, 0.0), p2(r, h * 0.45), p2(r * 0.8, h * 0.75), p2(r * 0.5, h)), 18, 0.9, a, 0.0); dome(body, hd.x, hd.y, rh, hh, h * 0.85, 0.92, a, 20);
    for (int sv = -1; sv <= 1; sv += 2) { P2 q = F(&ff, 0.15 + rh * 0.5, sv * rh * 0.42); cl_add(cu, disc(q.x, q.y, 0.21, 6), h * 0.85 + hh * 0.5); }
    cl_add(cu, barl(F(&ff, 0.15 + rh * 0.82, -rh * 0.5), F(&ff, 0.15 + rh * 0.82, rh * 0.5), 0.27), h * 0.85 + hh * 0.2);
    gone = rnd() < 0.3 ? rchi(2, -1, 1) : 0;
    for (int sv = -1; sv <= 1; sv += 2) { P2 q = F(&ff, 0.0, sv * rh * 0.72); double hc = sv == gone ? 0.35 : u(0.8, 1.1); cone(body, q.x, q.y, 0.42, hc, 0.3, h * 0.85 + hh * 0.55, 8, 1.0, 0.0); }
    o_st(o, body, NULL, b, cu, NULL);
    for (int sv = -1; sv <= 1; sv += 2) {
        P2 q = F(&ff, r * 0.75, sv * r * 0.8);
        dome(lw, q.x, q.y, 0.52, 0.6, 0.0, 1.0, 0.0, 20); rod(lw, plv(2, F(&ff, r * 0.75, sv * r * 0.8), F(&ff, r * 1.25, sv * r * 0.35)), 0.36, 1.0, 0.0, 0.0);
        rod(lw, plv(3, F(&ff, -0.1, sv * r * 0.95), F(&ff, r * 0.55, sv * r * 0.8), F(&ff, r * 0.92, sv * r * 0.3)), 0.33, 1.0, h * 0.3, 0.0);
    }
    o_st(o, lw, NULL, b, NULL, NULL);
    { double v = u(-0.3, 0.3); P2 c = F(&ff, r + S * 0.25 + 0.1, v); bw = o_on(o, disc(c.x, c.y, 0.56, 12), b, 0.34, NULL); o_cut(o, disc(c.x, c.y, 0.32, 8), bw, 0.2); }
    RET(o_out(o, lay, NULL));
}

/* cynder cinder_cone: a steep heap of spatter with its crater, the rim breached, the lava channel, bombs round it */
BLD(b_cynder_cinder_cone) {
    double r = u(2.3, 3.0); double h = u(1.7, 2.4); SoObj *o = OBJ("basalt"); double sq = u(0.85, 1.0); double tb = a + u(-0.5, 0.5); double rc = r * u(0.3, 0.36); CL *cu = cl(); SoSolids *ps = SL(); int n;
    { GG *d = disc(cx, cy, rc, 12); cl_add(cu, d, h * u(0.5, 0.62)); }
    cl_add(cu, Poly(plv(3, p2(cx, cy), pol(cx, cy, r * 1.2, tb - 0.2), pol(cx, cy, r * 1.2, tb + 0.2))), h * 0.72);
    cl_add(cu, wavy(pol(cx, cy, rc * 0.8, tb), pol(cx, cy, r * 1.1, tb), 0.2, 3, 0.42), h * 0.2);
    turn(ps, cx, cy, plv(4, p2(r, 0.0), p2(r * 0.82, h * 0.3), p2(r * 0.56, h * 0.75), p2(r * 0.44, h)), 14, sq, a, 0.0); o_st(o, ps, NULL, NOP, cu, disc(cx, cy, r, 20));
    { double v1 = u(1.6, 2.6); double v2 = u(-0.4, 0.4); P2 e = pol(cx, cy, r + v1, tb + v2); GG *ch = wavy(pol(cx, cy, r * 0.96, tb), e, 0.4, 4, 0.6); double rb = u(0.6, 0.9); GG *bb = blob(e.x, e.y, rb, 3);
      o_gcut(o, U(glv(2, ch, bb)), 0.36, "lava"); }
    n = ri(3, 6);
    for (int i = 0; i < n; i++) { double v1 = u(0.5, 1.4); double v2 = u(0.9, 5.4); P2 q = pol(cx, cy, r + v1, tb + v2); double rr = u(0.36, 0.55); double hh = u(0.35, 0.6); SoSolids *p3 = SL(); rock(p3, q.x, q.y, rr, hh, 0.0, 0.0, 9); o_st(o, p3, NULL, NOP, NULL, disc(q.x, q.y, 0.5, 20)); }
    RET(o_out(o, lay, NULL));
}

/* ====================================================================================================== sd_r_infestation.py */
/* (_curl is the same as sd_r_cynder's: cy_curl; _pl is only used by builders drawn again later) */
/* _lean(x, y, r, h, dx, dy, n=10, tip=0.26, sq=1.0, a=0.0, pw=1.0): a spike whose point stands (dx, dy) off its foot */
static void in_lean(SoSolids *o, double x, double y, double r, double h, double dx, double dy, int n, double tip, double sq, double a, double pw) {
    for (int k = 0; k < n; k++) {
        double f = fpw(1 - (double)k / (n - 1), pw);
        ext(o, ell(x + dx * k / (n - 1), y + dy * k / (n - 1), MX(tip, r * f), MX(tip, r * f * sq), a, 12), h * (k + 1) / n, 0.0);
    }
}
/* _fin(x, y, t, Lf, h, w, rake=1.0, n=10): a back spine as a raked fin */
static void in_fin(SoSolids *o, double x, double y, double t, double Lf, double h, double w, double rake, int n) {
    for (int k = 0; k < n; k++) {
        double q = (double)k / (n - 1), l = Lf + (0.5 - Lf) * q; P2 c = pol(x, y, rake * (Lf - l) / 2, t);
        ext(o, rect(c.x, c.y, l, w + (0.5 - w) * q, t), h * (k + 1) / n, 0.0);
    }
}

/* infestation dead_crawler: a dead crawler on its back: plated body, belly seam, curled jointed legs, coiled tail */
typedef struct { double L, W; int ns; } DcC;
static int dc_body(void *c, double t, double *out) {
    DcC *k = (DcC *)c; double s = (t + k->L / 2) / k->L;
    *out = k->W / 2 * fpw(SIN(PI_ * fpw(MN(1.0, MX(0.0, s)), 0.8)), 0.6) * (0.84 + 0.16 * fpw(1 - fmodp(s * k->ns, 1.0), 0.6)) + 0.04; return E_ ? SO_ERR : SO_OK;
}
static int dc_leg(void *c, double q, double *out) { (void)c; *out = 0.4 - 0.14 * q + 0.1 * py_exp(-fpw((q - 0.5) / 0.1, 2)); return SO_OK; }
static int dc_tail(void *c, double q, double *out) { (void)c; *out = 0.44 - 0.2 * q; return SO_OK; }
BLD(b_infestation_dead_crawler) {
    SoFrame ff = frame(cx, cy, a); double L = u(3.4, 4.1); double W = L * u(0.5, 0.58); SoObj *o = OBJ("carapace"); int ns = ri(5, 6); DcC dc = { L, W, ns }; SoSolids *ps = SL(); CL *cu = cl(); PT *tl; P2 hd;
    spin(ps, cx, cy, a, L, dc_body, &dc, 1.25, 0.0, "round"); cl_add(cu, barl(F(&ff, -L * 0.3, 0), F(&ff, L * 0.34, 0), 0.34), W * 0.36); o_st(o, ps, NULL, NOP, cu, rect(cx, cy, L, W, a));
    for (int k = 0; k < 3; k++) for (int sv = -1; sv <= 1; sv += 2) {
        double x = (k - 1) * L * 0.27; double t = a + sv * (1.5708 - (k - 1) * 0.5) + u(-0.15, 0.15); P2 p0 = F(&ff, x, sv * W * 0.3); double out = W * 0.2 + u(1.1, 1.6); P2 kn = pol(p0.x, p0.y, out, t);
        int ch = rchi(2, -1, 1); double t2 = t + sv * ch * u(1.6, 2.2); double Lc = u(1.0, 1.5); double bend = sv * u(-0.8, 0.8); PT *cu2 = cy_curl(kn, t2, Lc, bend, 4); PT *pts = pl_cat(plv(2, p0, kn), pl_slice(cu2, 1, cu2->n));
        ps = SL(); rodf(ps, pts, dc_leg, NULL, 1.0, 0.0, 0.0); o_st(o, ps, NULL, NOP, NULL, buf(Line(pts), 0.5, 16));
    }
    { double t0 = a + PI_ + u(-0.4, 0.4); double Lt = u(4.5, 6.5); int ch = rchi(2, -1, 1); double bend = ch * u(2.6, 4.2); tl = cy_curl(F(&ff, -L / 2 + 0.2, 0), t0, Lt, bend, 16); }
    ps = SL(); rodf(ps, tl, dc_tail, NULL, 1.0, 0.0, 0.0); o_st(o, ps, NULL, NOP, NULL, buf(Line(tl), 0.5, 16));
    hd = F(&ff, L / 2 + 0.1, 0); ps = SL();
    for (int sv = -1; sv <= 1; sv += 2) rod(ps, plv(3, F(&ff, L / 2 - 0.3, sv * 0.3), F(&ff, L / 2 + 0.7, sv * 0.45), F(&ff, L / 2 + 1.0, sv * 0.15)), 0.26, 1.0, 0.0, 0.0);
    o_st(o, ps, NULL, NOP, NULL, disc(hd.x, hd.y, 1.0, 20));
    RET(o_out(o, lay, NULL));
}

/* infestation resin_stump: a column of twisted ribs flaring into buttress roots, snapped off slantwise */
static int rs_root(void *c, double q, double *out) { (void)c; *out = 0.6 - 0.32 * q; return SO_OK; }
BLD(b_infestation_resin_stump) {
    double r = u(1.5, 2.0); double h = u(1.9, 2.8); int n = ri(5, 7); double tw; int m = 9; SoObj *o = OBJ("resin"); SoSolids *ps = SL(), *rs = SL(); double tb; GG *brk; CL *cu = cl(); int ni;
    { double v = u(0.05, 0.11); tw = v * rchi(2, -1, 1); }
    for (int k = 0; k < m; k++) { double f = r * (0.6 + 0.4 * fpw(1 - (double)k / (m - 1), 2)); ext(ps, star(cx, cy, f, f * 0.66, n, a + tw * k), h * (k + 1) / m, 0.0); }
    tb = u(0, 6.28); brk = Poly(plv(4, pol(cx, cy, 3 * r, tb - 1.5), pol(cx, cy, 0.1 * r, tb + PI_), pol(cx, cy, 3 * r, tb + 1.5), pol(cx, cy, 4 * r, tb)));
    cl_add(cu, brk, h * u(0.55, 0.7)); { double v = u(0, 3); cl_add(cu, ngon(cx, cy, r * 0.3, 5, v), h * 0.3); }
    for (int k = 0; k < n; k++) { double t = a + 6.2832 * k / n; double v1 = u(0.5, 1.1); double v2 = u(-0.7, 0.7); rodf(rs, cy_curl(pol(cx, cy, r * 0.5, t), t, r * 0.5 + v1, v2, 6), rs_root, NULL, 1.0, 0.0, 0.0); }
    o_st(o, rs, NULL, NOP, NULL, disc(cx, cy, r + 1.0, 20)); o_st(o, ps, NULL, NOP, cu, disc(cx, cy, r, 20));
    ni = ri(1, 3);
    for (int i = 0; i < ni; i++) {
        double v1 = u(1.0, 1.8); double v2 = u(-0.8, 0.8); P2 c = pol(cx, cy, r + v1, tb + PI_ + v2); double lr = u(0.4, 0.55); double lh = u(0.4, 0.7); double dx = u(-0.2, 0.2); double dy = u(-0.2, 0.2); double la = u(0, 3); SoSolids *p3 = SL();
        in_lean(p3, c.x, c.y, lr, lh, dx, dy, 5, 0.26, 0.7, la, 1.0); o_st(o, p3, NULL, NOP, NULL, disc(c.x, c.y, 0.5, 20));
    }
    RET(o_out(o, lay, NULL));
}

/* infestation spore_chimneys: swollen bulbs, collars and flues with thick lips, crowded; pods round their feet */
BLD(b_infestation_spore_chimneys) {
    SoObj *o = OBJ("spore"); int n = ri(2, 4), m; SoSolids *ps;
    for (int i = 0; i < n; i++) {
        double v1 = i == 0 ? 0 : u(1.9, 2.5); double v2 = u(-0.3, 0.3); P2 p = pol(cx, cy, v1, a + i * 2.1 + v2); double r0 = u(1.0, 1.3); double r = r0 * (i == 0 ? 1 : u(0.65, 0.85)); double h = r * u(1.9, 2.5); double rf = MX(0.72, r * 0.62);
        double sq = u(0.85, 1.0); double aa = u(0, 3); CL *cu = cl(); double w = MX(rf + 0.1, r * 0.8);
        ps = SL(); dome(ps, p.x, p.y, r, h * 0.42, 0.0, sq, aa, 20); turn(ps, p.x, p.y, plv(4, p2(w, 0.0), p2(w, h * 0.5), p2(rf, h * 0.58), p2(rf, h)), 16, 1.0, 0.0, 0.0);
        cl_add(cu, disc(p.x, p.y, rf - 0.3, 10), h * 0.45); o_st(o, ps, NULL, NOP, cu, disc(p.x, p.y, r, 20));
    }
    ps = SL(); m = ri(3, 6);
    for (int i = 0; i < m; i++) { double v1 = u(2.6, 3.6); double v2 = u(0, 6.28); P2 p = pol(cx, cy, v1, v2); double dr = u(0.32, 0.42); double dh = u(0.36, 0.5); dome(ps, p.x, p.y, dr, dh, 0.0, 1.0, 0.0, 20); o_F(o, disc(p.x, p.y, 0.4, 20)); }
    o_st(o, ps, NULL, NOP, NULL, disc(cx, cy, 1.0, 20));
    RET(o_out(o, lay, NULL));
}

/* infestation acid_holes: pits eaten by acid with melted lips, a big one and smaller ones, pin holes of single drops */
BLD(b_infestation_acid_holes) {
    GL *foot = gl(), *hs = gl(), *dl = gl(); int n = ri(3, 5), nd; GG *dr;
    for (int i = 0; i < n; i++) {
        P2 p; double rb; GG *g; int near = 0; SoSolids *ps;
        if (i == 0) p = p2(cx, cy); else { double v1 = u(1.9, 3.0); double v2 = u(-0.4, 0.4); p = pol(cx, cy, v1, a + i * 6.2832 / (n - 1) + v2); }
        rb = i == 0 ? u(0.9, 1.2) : u(0.5, 0.8); g = blob(p.x, p.y, rb, 3);
        for (int j = 0; j < hs->n && !near; j++) if (gdist(g, hs->v[j]) < 1.0) near = 1;
        if (near) continue;
        gl_add(hs, g); ps = SL(); { GG *lip = dif(buf(g, 0.5, 6), buf(g, 0.03, 16)); ext(ps, lip, u(0.3, 0.42), 0.0); }
        push(lay, ST(ps, "acid", NOP, NULL)); push(lay, L_gcut(g, u(0.5, 0.8), "acid")); gl_add(foot, buf(g, 0.6, 16));
    }
    nd = ri(3, 7);
    for (int i = 0; i < nd; i++) { double v1 = u(1.2, 3.4); double v2 = u(0, 6.28); P2 q = pol(cx, cy, v1, v2); double rr = u(0.26, 0.36); gl_add(dl, disc(q.x, q.y, rr, 8)); }
    dr = dif(U(dl), buf(U(foot), 0.3, 16));
    if (!empty(dr)) { push(lay, L_gcut(dr, 0.4, "acid")); gl_add(foot, dr); }
    RET(buf(hull(foot), 0.2, 16));
}

/* infestation hive_vent: a puckered ring of flesh, folded all round, closing on a deep throat; veins out from it */
static int hv_vein(void *c, double q, double *out) { (void)c; *out = 0.5 - 0.24 * q; return SO_OK; }
BLD(b_infestation_hive_vent) {
    double R = u(1.9, 2.5); double h = u(1.0, 1.4); int n = ri(8, 11), nv; SoObj *o = OBJ("flesh"); double sq = u(0.8, 1.0); double rI = R * 0.3; SoSolids *vs = SL(), *ps = SL(); CL *cu = cl();
    nv = ri(2, 4);
    for (int k = 0; k < nv; k++) { double t = u(0, 6.28); double v1 = u(1.1, 1.6); double v2 = u(-1.0, 1.0); rodf(vs, cy_curl(pol(cx, cy, R * 0.7, t), t, v1, v2, 6), hv_vein, NULL, 0.8, 0.0, 0.0); }
    o_st(o, vs, NULL, NOP, NULL, disc(cx, cy, R + 1.0, 20));
    cl_add(cu, disc(cx, cy, rI, 14), 0.0);
    for (int k = 0; k < n; k++) cl_add(cu, tri(pol(cx, cy, R * 0.98, a + 6.2832 * k / n), pol(cx, cy, rI * 0.8, a + 6.2832 * k / n), 0.4), h * 0.55);
    turn(ps, cx, cy, plv(4, p2(R, 0.0), p2(R * 0.86, h * 0.5), p2(R * 0.62, h * 0.9), p2(R * 0.46, h)), 24, sq, a, 0.0); o_st(o, ps, NULL, NOP, cu, disc(cx, cy, R, 20));
    { GG *d = disc(cx, cy, rI - 0.1, 12); o_gcut(o, d, u(0.6, 0.85), "pit"); }
    RET(o_out(o, lay, NULL));
}

/* infestation tendril_knot: fat ringed arms winding out of one heap, two looped over the others */
static int tk_arm(void *c, double q, double *out) { double nr = *(double *)c; *out = (0.68 - 0.44 * q) * (0.86 + 0.14 * fabs(SIN(q * PI_ * nr))); return SO_OK; }
BLD(b_infestation_tendril_knot) {
    SoObj *o = OBJ("tendril"); int n = ri(4, 6); SoSolids *ps = SL(); double t;
    for (int i = 0; i < n; i++) {
        double ti = a + 6.2832 * i / n + u(-0.3, 0.3); double Lt = u(3.0, 4.6); int ch = rchi(2, -1, 1); double bend = ch * u(1.0, 2.4); PT *pts = cy_curl(pol(cx, cy, 0.3, ti), ti, Lt, bend, 14); double nr = Lt / 0.55;
        rodf(ps, pts, tk_arm, &nr, 1.0, 0.0, 0.0); o_F(o, buf(Line(pts), 0.5, 16));
    }
    o_st(o, ps, NULL, NOP, NULL, disc(cx, cy, 1.0, 20));
    t = u(0, 6.28); ps = SL();
    { P2 c1 = pol(cx, cy, 0.5, t), c2 = pol(cx, cy, 0.5, t + 3.0);
      rod(ps, arc(c1.x, c1.y, 0.85, t, t + 4.4, 14), 0.5, 1.0, 0.45, 0.0); rod(ps, arc(c2.x, c2.y, 0.75, t + 2.0, t + 5.6, 14), 0.46, 1.0, 0.75, 0.0); }
    o_st(o, ps, NULL, NOP, NULL, disc(cx, cy, 1.6, 20));
    RET(o_out(o, lay, NULL));
}

/* infestation sac_cluster: taut bladders on a skin of membrane, each with its teat, one burst; veins feeding them */
static int sc_vein(void *c, double q, double *out) { (void)c; *out = 0.42 - 0.16 * q; return SO_OK; }
BLD(b_infestation_sac_cluster) {
    SoObj *o = OBJ("pustule"); int n = ri(4, 6), nc = 0, nv, burst; P2 cp[8]; double cr[8]; GL *md = gl(); GG *mem; SoSolids *vs = SL(); int64_t b;
    for (int i = 0; i < n; i++) for (int tr = 0; tr < 8; tr++) {
        P2 p; double r; int ok = 1;
        if (i == 0) p = p2(cx, cy); else { double v1 = u(1.0, 2.4); double v2 = u(0, 6.28); p = pol(cx, cy, v1, v2); }
        r = i == 0 ? u(0.95, 1.25) : u(0.6, 0.95);
        for (int j = 0; j < nc; j++) if (!(fdist(p, cp[j]) > (r + cr[j]) * 0.8)) { ok = 0; break; }
        if (ok && !E_) { cp[nc] = p; cr[nc] = r; nc++; break; }
    }
    for (int j = 0; j < nc; j++) gl_add(md, disc(cp[j].x, cp[j].y, cr[j] + 0.45, 14));
    mem = buf(buf(U(md), 0.4, 16), -0.4, 16);
    nv = ri(2, 4);
    for (int k = 0; k < nv; k++) { double t = u(0, 6.28); double v = u(3.2, 4.2); P2 e = pol(cx, cy, v, t); rodf(vs, wiggle(pol(cx, cy, 1.0, t), e, 0.4, 3), sc_vein, NULL, 0.9, 0.0, 0.0); o_F(o, disc(e.x, e.y, 0.3, 20)); }
    o_st(o, vs, NULL, NOP, NULL, mem); b = o_up(o, mem, 0.3, "resin"); burst = rnd() < 0.6 ? ri(1, nc) : -1;
    for (int i = 0; i < nc; i++) {
        double h = cr[i] * u(1.0, 1.3); double sq = u(0.82, 1.0); double t = u(0, 3); SoSolids *ps = SL();
        if (i == burst) { CL *cu = cl(); dome(ps, cp[i].x, cp[i].y, cr[i], h * 0.6, 0.0, sq, t, 20); cl_add(cu, star(cp[i].x, cp[i].y, cr[i] * 0.8, cr[i] * 0.3, 5, t), h * 0.2); o_st(o, ps, NULL, b, cu, NULL); }
        else { dome(ps, cp[i].x, cp[i].y, cr[i], h, 0.0, sq, t, 20); dome(ps, cp[i].x, cp[i].y, 0.34, 0.3, h * 0.93, 1.0, 0.0, 20); o_st(o, ps, NULL, b, NULL, NULL); }
    }
    RET(o_out(o, lay, NULL));
}

/* infestation flamethrower: fuel bottles on a back frame, straps, the hose to the gun in its slotted heat shield */
typedef struct { double L, r; } FtC;
static int ft_tank(void *c, double t, double *out) {
    FtC *k = (FtC *)c; double e = fabs(t) - (k->L / 2 - k->r);
    *out = k->r * (e > 0 ? fsq(MX(0.0, 1 - fpw(e / k->r, 2))) : (fabs(fabs(t) - k->L * 0.2) < 0.16 ? 1.12 : 1.0)) + 0.02; return E_ ? SO_ERR : SO_OK;
}
static int ft_brl(void *c, double t, double *out) { double Lg = *(double *)c; *out = t > Lg * 0.42 - Lg * 0.2 ? 0.5 : (fabs(t + Lg * 0.2 - Lg * 0.14) < Lg * 0.14 ? 0.42 : 0.3); return SO_OK; }
BLD(b_infestation_flamethrower) {
    SoFrame ff = frame(cx, cy, a); double L = u(3.0, 3.6); double r = u(0.58, 0.68); int n = rchi(3, 2, 2, 3); SoObj *o = OBJ("metal"); double Wt = n * 2 * r + 0.1; FtC fc = { L, r };
    P2 q = F(&ff, -0.1, 0); SoSolids *ps = SL(); int64_t rc; int sv; P2 g0; double b_, Lg, hs; SoFrame f2; GG *gp; CL *cu;
    o_up(o, rrect(q.x, q.y, L + 0.5, Wt + 0.5, a, 0.3), 0.3, NULL);
    for (int i = 0; i < n; i++) { P2 c = F(&ff, 0, (i - (n - 1) / 2.0) * (2 * r + 0.05)); P2 e = pol(c.x, c.y, L / 2 + 0.1, a); spin(ps, c.x, c.y, a, L, ft_tank, &fc, 1.05, 0.2, "round"); lie(ps, e.x, e.y, a, 0.5, 0.3, 0.3); }
    { P2 m = F(&ff, L / 2 + 0.45, 0); blk(ps, m.x, m.y, 0.5, Wt - 0.3, a, 0.75, 0.0); o_st(o, ps, NULL, NOP, NULL, rect(cx, cy, L + 1.2, Wt, a)); }
    for (sv = -1; sv <= 1; sv += 2) {          /* the shoulder straps, each with its buckle */
        double v1 = u(-0.6, 0.6); P2 p = F(&ff, v1, sv * (Wt / 2 + 0.1)); double t = a + sv * 1.57 + u(-0.4, 0.4); double Lc = u(1.6, 2.4); double bend = u(-1.2, 1.2); PT *pts = cy_curl(p, t, Lc, bend, 8);
        ps = SL(); ext(ps, line(pts, 0.6), 0.3, 0.0); o_st(o, ps, "leather", NOP, NULL, buf(Line(pts), 0.5, 16));
    }
    sv = rchi(2, -1, 1); { double v1 = u(0.6, 1.6); double v2 = u(2.0, 2.6); g0 = F(&ff, v1, sv * (Wt / 2 + v2)); } b_ = a + u(-0.3, 0.3); f2 = frame(g0.x, g0.y, b_); Lg = u(4.2, 5.0);
    { P2 m = F(&f2, -Lg * 0.28, 0); GL *bs = gl(); double xs[3] = { -0.35, 0.0, 0.35 };
      rc = o_up(o, rrect(m.x, m.y, Lg * 0.36, 0.95, b_, 0.2), 0.62, NULL);
      for (int i = 0; i < 3; i++) gl_add(bs, barl(F(&f2, -Lg * 0.28 + xs[i], -0.3), F(&f2, -Lg * 0.28 + xs[i], 0.3), 0.26));
      o_cut(o, U(bs), rc, 0.18); }
    gp = Poly(plv(4, F(&f2, -Lg * 0.36, -sv * 0.3), F(&f2, -Lg * 0.22, -sv * 0.3), F(&f2, -Lg * 0.3, -sv * 1.4), F(&f2, -Lg * 0.46, -sv * 1.3))); ps = SL(); ext(ps, gp, 0.5, 0.0); o_st(o, ps, NULL, NOP, NULL, gp);
    hs = Lg * 0.6; cu = cl();
    { int nsl = (int)(Lg * 0.28 / 0.5); P2 m = F(&f2, Lg * 0.2, 0), e = F(&f2, Lg * 0.2 + hs / 2, 0);
      for (int i = 0; i < nsl; i++) cl_add(cu, barl(F(&f2, Lg * 0.2 - hs / 2 + 0.3 + i * 0.5, -0.5), F(&f2, Lg * 0.2 - hs / 2 + 0.3 + i * 0.5, 0.5), 0.26), 0.32);
      cl_add(cu, disc(e.x, e.y, 0.3, 8), 0.2);
      ps = SL(); spin(ps, m.x, m.y, b_, hs, ft_brl, &Lg, 1.05, 0.0, "round"); o_st(o, ps, NULL, NOP, cu, rect(m.x, m.y, hs, 1.0, b_)); }
    { P2 m = F(&f2, -Lg * 0.02, sv * 0.66); ps = SL(); lie(ps, m.x, m.y, b_, 1.3, 0.32, 0.0); o_st(o, ps, NULL, NOP, NULL, rect(m.x, m.y, 1.3, 0.6, b_)); }
    ps = SL(); rod(ps, wiggle(F(&ff, L / 2 + 0.6, 0), F(&f2, -Lg * 0.46, 0), 0.6, 4), 0.3, 1.1, 0.0, 0.0); o_st(o, ps, "cable", NOP, NULL, NULL);
    RET(o_out(o, lay, NULL));
}

/* infestation scanner_and_mags: a motion tracker face up (screen, rings, sweep, keys, dial, grip, horn); magazines */
BLD(b_infestation_scanner_and_mags) {
    SoFrame ff = frame(cx, cy, a); double L = u(2.8, 3.3); double W = u(1.9, 2.2); SoObj *o = OBJ("metal"); GG *g = rrect(cx, cy, L, W, a, 0.3), *half, *rings; int64_t gr, b, hd; P2 sc; double R, sw; int nm; PT *st;
    { double v = u(-0.3, 0.3); GG *gp = buf(buf(Poly(plv(4, F(&ff, -L / 2 + 0.2, -0.5), F(&ff, -L / 2 - 1.8, -0.5 + v), F(&ff, -L / 2 - 1.8, 0.5), F(&ff, -L / 2 + 0.2, 0.5))), -0.12, 16), 0.12, 4); gr = o_up(o, gp, 0.5, NULL); }
    { GL *bs = gl(); double xs[3] = { 0.5, 0.9, 1.3 }; for (int i = 0; i < 3; i++) gl_add(bs, barl(F(&ff, -L / 2 - xs[i], -1), F(&ff, -L / 2 - xs[i], 1), 0.26)); o_cut(o, U(bs), gr, 0.16); }
    b = o_up(o, g, 0.6, NULL); o_up(o, Poly(plv(4, F(&ff, L / 2 - 0.1, -W * 0.3), F(&ff, L / 2 + 0.7, -W * 0.18), F(&ff, L / 2 + 0.7, W * 0.18), F(&ff, L / 2 - 0.1, W * 0.3))), 0.45, NULL);
    sc = F(&ff, L * 0.12, 0); hd = o_on(o, rrect(sc.x, sc.y, L * 0.58, W - 0.3, a, 0.2), b, 0.3, NULL); R = MN(L * 0.58, W - 0.3) / 2 - 0.22;
    o_cut(o, disc(sc.x, sc.y, R, 20), hd, 0.2);
    half = Poly(pl_cat(plv(1, sc), arc(sc.x, sc.y, 3, a - 1.2, a + 1.2, 8)));
    rings = isect(U(glv(2, ann(sc.x, sc.y, R * 0.75, R * 0.75 - 0.26, 16), disc(sc.x, sc.y, 0.26, 8))), half);
    sw = u(-0.9, 0.9); o_cut(o, U(glv(2, rings, barl(sc, pol(sc.x, sc.y, R, a + sw), 0.26))), hd, 0.4);
    st = pl(); for (int i = 0; i < 3; i++) pl_add(st, F(&ff, -L / 2 + 0.5, (i - 1) * 0.55)); o_studs(o, st, b, 0.3, 0.16, NULL, 8);
    { double v = u(-0.3, 0.6); P2 kn = F(&ff, v, -W / 2 - 0.05); o_up(o, disc(kn.x, kn.y, 0.52, 10), 0.4, NULL); }
    nm = ri(1, 3);
    for (int i = 0; i < nm; i++) {          /* magazines: a slightly bent box, ribs, the feed lips and the top round */
        double v1 = u(-0.2, 0.2); double v2 = u(1.5, 2.2); P2 c = F(&ff, -1.4 + i * 1.7 + v1, W / 2 + v2); double t = a + 1.57 + u(-0.7, 0.7); SoFrame f2 = frame(c.x, c.y, t); double Lm = u(2.2, 2.6);
        GG *gm = Poly(plv(6, F(&f2, -Lm / 2, -0.5), F(&f2, 0, -0.6), F(&f2, Lm / 2, -0.5), F(&f2, Lm / 2, 0.5), F(&f2, 0, 0.4), F(&f2, -Lm / 2, 0.5))); SoSolids *ps = SL(); GL *rb = gl(); double xs[3] = { -0.75, 0.0, 0.75 };
        ext(ps, gm, 0.42, 0.0); o_st(o, ps, NULL, NOP, NULL, gm);
        for (int j = 0; j < 3; j++) { P2 m = F(&f2, xs[j], -0.05); gl_add(rb, rect(m.x, m.y, 0.5, 0.96, t)); }
        ps = SL(); ext(ps, U(rb), 0.58, 0.0); o_st(o, ps, NULL, NOP, NULL, gm);
        if (rnd() < 0.7) { P2 e1 = F(&f2, Lm / 2 + 0.3, 0), e2 = F(&f2, Lm / 2 + 0.72, 0), e3 = F(&f2, Lm / 2 + 0.4, 0); ps = SL(); lie(ps, e1.x, e1.y, t, 0.7, 0.24, 0.0); cone(ps, e2.x, e2.y, 0.24, 0.26, 0.2, 0.0, 8, 1.0, 0.0); o_st(o, ps, "brass", NOP, NULL, disc(e3.x, e3.y, 0.5, 20)); }
    }
    RET(o_out(o, lay, NULL));
}

/* infestation spine_row: a ridge of knuckled bone along a curve, a raked spine from each knuckle, one snapped */
typedef struct { int n; double half, R; } SrC;
static int sr_ridge(void *c, double q, double *out) { SrC *k = (SrC *)c; *out = 0.7 + 0.25 * fabs(COS(q * PI_ * (k->n - 1) * (1 + 1.4 / (2 * k->half * k->R)))); return SO_OK; }
BLD(b_infestation_spine_row) {
    double R = u(3.2, 5.2); int n = ri(3, 5), rk, brk; SoObj *o = OBJ("spine"); double half = 1.15 * (n - 1) / R, ox = cx - R * COS(a), oy = cy - R * SIN(a); PT *pts, *ex; SoSolids *ps = SL(); SrC sc = { n, half, R };
    pts = arc(ox, oy, R, a - half, a + half, n - 1); rk = rchi(2, -1, 1); brk = rnd() < 0.4 ? ri(0, n - 1) : -1;
    ex = arc(ox, oy, R, a - half - 0.7 / R, a + half + 0.7 / R, 3 * n);
    rodf(ps, ex, sr_ridge, &sc, 0.7, 0.0, 0.0); o_st(o, ps, NULL, NOP, NULL, buf(Line(ex), 0.8, 16));
    for (int i = 0; i < pts->n; i++) {
        P2 p = pl_at(pts, i); double k = 1 - fabs(i - (n - 1) / 2.0) / n, t = a - half + 2 * half * i / (n - 1) + rk * 1.5708; double h = u(2.0, 2.9) * (0.5 + 0.65 * k); double Lf = u(1.5, 2.0) * (0.7 + 0.4 * k);
        double w = u(0.7, 0.85); double rake = u(0.5, 1.0); CL *cu = NULL; SoSolids *fs = SL();
        in_fin(fs, p.x, p.y, t, Lf, h, w, rake, 10);
        if (i == brk) { P2 q = pol(p.x, p.y, Lf * 0.4, t); cu = cl(); { GG *d = disc(q.x, q.y, 2.0, 20); cl_add(cu, d, h * u(0.3, 0.45)); } }
        o_st(o, fs, NULL, NOP, cu, disc(p.x, p.y, Lf / 2, 20));
    }
    RET(o_out(o, lay, NULL));
}

/* infestation larvae: fat ringed grubs, each curled its own way, blunt at the head with two jaw points */
typedef struct { double R; int ns; } LvC;
static int lv_body(void *c, double q, double *out) {
    LvC *k = (LvC *)c;
    *out = k->R * (0.55 + 0.45 * SIN(MN(1.0, q * 1.6 + 0.12) * PI_ / 2)) * fsq(MX(0.05, 1 - fpw(MX(0.0, (q - 0.85) / 0.15), 2))) * (0.8 + 0.2 * fabs(SIN(q * PI_ * k->ns))); return E_ ? SO_ERR : SO_OK;
}
BLD(b_infestation_larvae) {
    SoObj *o = OBJ("flesh"); P2 cs[8]; int nc = 0, n = ri(3, 5);
    for (int i = 0; i < n; i++) for (int tr = 0; tr < 10; tr++) {
        double v1 = u(0, 3.0); double v2 = u(0, 6.28); P2 p = pol(cx, cy, v1, v2); int ok = 1;
        for (int j = 0; j < nc; j++) if (!(fdist(p, cs[j]) > 2.0)) { ok = 0; break; }
        if (ok && !E_) { cs[nc++] = p; break; }
    }
    for (int j = 0; j < nc; j++) {
        P2 p = cs[j], mid, e; double L = u(2.4, 3.3); double R = u(0.52, 0.64); double t = u(0, 6.28); int ch = rchi(2, -1, 1); double bend = ch * u(0.8, 2.6); int ns = ri(6, 8); PT *p0, *pts; SoSolids *ps; LvC lc = { R, ns }; double t2;
        p0 = cy_curl(p, t, L, bend, 12); mid = pl_at(p0, p0->n / 2); pts = pl();
        for (int i = 0; i < p0->n; i++) pl_add(pts, p2(p0->xy[2 * i] - mid.x + p.x, p0->xy[2 * i + 1] - mid.y + p.y));
        ps = SL(); rodf(ps, pts, lv_body, &lc, 1.05, 0.0, 0.0); o_st(o, ps, NULL, NOP, NULL, buf(Line(pts), R, 16));
        e = pl_at(pts, -1); t2 = ATAN2(pl_at(pts, -1).y - pl_at(pts, -2).y, pl_at(pts, -1).x - pl_at(pts, -2).x); ps = SL();
        for (int sv = -1; sv <= 1; sv += 2) { P2 q = pol(e.x, e.y, 0.3, t2 + sv * 0.7); cone(ps, q.x, q.y, 0.26, 0.3, 0.2, 0.0, 6, 1.0, 0.0); }
        o_st(o, ps, "carapace", NOP, NULL, disc(e.x, e.y, 0.5, 20));
    }
    RET(o_out(o, lay, NULL));
}

/* infestation web_anchor: spokes from a knot of resin to their glue points, rings of sagging threads, torn in places */
BLD(b_infestation_web_anchor) {
    SoObj *o = OBJ("web"); int n = ri(6, 8); double ts[8], Ls[8]; GL *gs = gl(), *ends = gl(); static const double FS[3] = { 0.42, 0.68, 0.92 }; SoSolids *ps;
    for (int k = 0; k < n; k++) ts[k] = a + 6.2832 * k / n + u(-0.2, 0.2);
    for (int k = 0; k < n; k++) Ls[k] = u(3.0, 4.6);
    for (int k = 0; k < n; k++) { P2 e = pol(cx, cy, Ls[k], ts[k]); gl_add(gs, capl(p2(cx, cy), e, 0.56)); gl_add(ends, disc(e.x, e.y, 0.5, 8)); }
    for (int fi = 0; fi < 3; fi++) for (int k = 0; k < n; k++) {
        double f = FS[fi], tm; int k2 = (k + 1) % n; P2 p, q, m;
        if (rnd() < 0.2) continue;
        p = pol(cx, cy, Ls[k] * f, ts[k]); q = pol(cx, cy, Ls[k2] * f, ts[k2]); tm = ts[k] + fmodp(ts[k2] - ts[k], 6.2832) / 2; m = pol(cx, cy, (Ls[k] + Ls[k2]) / 2 * f * 0.8, tm);
        gl_add(gs, line(plv(4, p, p2((p.x + m.x * 2) / 3, (p.y + m.y * 2) / 3), p2((q.x + m.x * 2) / 3, (q.y + m.y * 2) / 3), q), 0.52));
    }
    { GL *all = gl(); for (int i = 0; i < gs->n; i++) gl_add(all, gs->v[i]); for (int i = 0; i < ends->n; i++) gl_add(all, ends->v[i]);
      ps = SL(); ext(ps, buf(buf(U(all), 0.08, 16), -0.08, 16), 0.38, 0.0); o_st(o, ps, NULL, NOP, NULL, hull(ends)); }
    { double r = u(0.75, 0.95); double h = u(0.75, 1.0); double aa = u(0, 3); ps = SL(); dome(ps, cx, cy, r, h, 0.0, 0.85, aa, 20); o_st(o, ps, "resin", NOP, NULL, disc(cx, cy, 0.8, 20)); }
    RET(o_out(o, lay, NULL));
}

/* infestation mandibles: a pair of jaws torn from something big, saw teeth inside, socket knobs; some lie alone */
static int md_blade(void *c, double q, double *out) { (void)c; *out = 0.78 - 0.5 * q; return SO_OK; }
static P2 md_cl(const SoFrame *ff, double L, int sv, double sp, double s) { return F(ff, -L / 2 + L * s, sv * (0.62 + sp * SIN(PI_ * fpw(s, 0.7)))); }
BLD(b_infestation_mandibles) {
    SoFrame ff = frame(cx, cy, a); double L = u(4.4, 5.6); SoObj *o = OBJ("carapace"); int both = rnd() < 0.75;
    for (int sv = -1; sv <= (both ? 1 : -1); sv += 2) {
        double sp = u(1.0, 1.8), st, ss[64]; PT *pts = pl(); GL *teeth = gl(); int ns; SoSolids *ps; P2 k, d; CL *cu;
        L = L * u(0.85, 1.0);
        for (int j = 0; j < 15; j++) pl_add(pts, md_cl(&ff, L, sv, sp, j / 14.0));
        st = u(0.11, 0.15); ns = arange(0.36, 0.86, st, ss, 64);
        for (int j = 0; j < ns; j++) {
            double s = ss[j], w = 0.75 * (1 - 0.4 * s), sn = SIN(PI_ * fpw(s, 0.7)); P2 p2_ = F(&ff, -L / 2 + L * s, sv * (0.62 + sp * sn - 0.3)), q = F(&ff, -L / 2 + L * s + 0.25, sv * (0.62 + sp * sn - 0.3 - 1.25 * (1 - 0.5 * s)));
            gl_add(teeth, tri(p2_, q, w));
        }
        ps = SL(); ext(ps, U(teeth), 0.42, 0.0); o_st(o, ps, NULL, NOP, NULL, buf(Line(pts), 0.9, 16));
        ps = SL(); rodf(ps, pts, md_blade, NULL, 0.95, 0.0, 0.0); o_st(o, ps, NULL, NOP, NULL, buf(Line(pts), 0.8, 16));
        k = pl_at(pts, 0); d = F(&ff, -L / 2 - 0.2, sv * 0.62); ps = SL(); cu = cl(); dome(ps, k.x, k.y, 0.92, 1.0, 0.0, 0.85, a, 20); cl_add(cu, disc(d.x, d.y, 0.3, 8), 0.6); o_st(o, ps, NULL, NOP, cu, disc(k.x, k.y, 0.9, 20));
    }
    RET(o_out(o, lay, NULL));
}

/* infestation victim_bones: a skull with eye holes, the backbone with its ribs, a thigh bone or two */
static int vb_spine(void *c, double q, double *out) { (void)c; *out = 0.34 + 0.1 * fabs(SIN(q * PI_ * 9)); return SO_OK; }
BLD(b_infestation_victim_bones) {
    SoFrame ff = frame(cx, cy, a); SoObj *o = OBJ("spine"); double L = u(4.4, 5.2); int nr = ri(3, 4), nb; PT *sp = pl(); SoSolids *ps = SL(); CL *cu;
    for (int k = 0; k < 9; k++) pl_add(sp, F(&ff, -L / 2 + L * k / 8, 0.25 * SIN(k * 0.8 + a)));
    rodf(ps, sp, vb_spine, NULL, 1.1, 0.0, 0.0);
    for (int k = 0; k < nr; k++) {
        double x = -L * 0.15 + k * 0.9, w = 1.6 * (0.75 + 0.25 * SIN(PI_ * (k + 0.5) / nr));
        for (int sv = -1; sv <= 1; sv += 2) {
            if (rnd() < 0.15) continue;
            rod(ps, plv(4, F(&ff, x, sv * 0.15), F(&ff, x + 0.25, sv * w * 0.6), F(&ff, x + 0.1, sv * w), F(&ff, x - 0.35, sv * w * 1.25)), 0.31, 1.1, 0.0, 0.0);
        }
    }
    o_st(o, ps, NULL, NOP, NULL, rect(cx, cy, L, 4.4, a));
    { double v1 = u(0.7, 1.1); double v2 = u(-0.5, 0.5); P2 c = F(&ff, L / 2 + v1, v2); double t = a + u(-0.8, 0.8); SoFrame f2 = frame(c.x, c.y, t); P2 d = F(&f2, 0.6, 0);
      ps = SL(); cu = cl(); dome(ps, c.x, c.y, 0.85, 1.0, 0.0, 0.85, t, 20); dome(ps, d.x, d.y, 0.52, 0.6, 0.0, 1.0, 0.0, 20);
      for (int sv = -1; sv <= 1; sv += 2) { P2 q = F(&f2, 0.42, sv * 0.34); cl_add(cu, disc(q.x, q.y, 0.25, 8), 0.4); }
      cl_add(cu, tri(F(&f2, 0.98, 0), F(&f2, 0.6, 0), 0.34), 0.25); o_st(o, ps, NULL, NOP, cu, disc(c.x, c.y, 1.2, 20)); }
    nb = ri(1, 2);
    for (int i = 0; i < nb; i++) {
        double v1 = u(-L * 0.4, L * 0.2); int ch = rchi(2, -1, 1); double v2 = u(2.6, 3.2); P2 c = F(&ff, v1, ch * v2); double t = a + u(-0.6, 0.6); double Lb = u(2.2, 2.8); P2 p = pol(c.x, c.y, Lb / 2, t), q = pol(c.x, c.y, Lb / 2, t + PI_), es[2];
        ps = SL(); rod(ps, plv(2, p, q), 0.29, 1.1, 0.0, 0.0); es[0] = p; es[1] = q;
        for (int j = 0; j < 2; j++) for (int sv = -1; sv <= 1; sv += 2) { P2 m = pol(es[j].x, es[j].y, 0.24, t + sv * 1.57); dome(ps, m.x, m.y, 0.33, 0.42, 0.0, 1.0, 0.0, 20); }
        o_st(o, ps, NULL, NOP, NULL, buf(Line(plv(2, p, q)), 0.6, 16));
    }
    RET(o_out(o, lay, NULL));
}

/* ====================================================================================================== sd_r_maahl.py */
/* ma_bar(p, q, rf, k=0.4, z0=None, z1=None) (= sd_r_targa.tg_bar = sd_r_trederra.t_bar, the same code): a round bar
   from p to q with flat ends, solid underneath; rf a number (rfn NULL) or a function of 0..1 along it. Its section is
   worked out when it is asked for (sculpt), so it keeps its own copy of the profile. */
typedef struct { double px, py, ux, uy, L; int m; double *S, *R, *C; } RBar;
static void rbar_free(void *c) { RBar *b = (RBar *)c; free(b->S); free(b->R); free(b->C); free(b); }
static GEOSGeometry *rbar_poly(const RBar *b, const double *rs, const double *rw, int n) {          /* Polygon([pt(s, w) ...] + [pt(s, -w) reversed]) */
    double *xy = (double *)malloc(sizeof(double) * 4 * (size_t)n); GEOSGeometry *g; int k = 0;
    for (int i = 0; i < n; i++) { xy[k++] = b->px + b->ux * rs[i] * b->L - b->uy * rw[i]; xy[k++] = b->py + b->uy * rs[i] * b->L + b->ux * rw[i]; }
    for (int i = n - 1; i >= 0; i--) { xy[k++] = b->px + b->ux * rs[i] * b->L - b->uy * -rw[i]; xy[k++] = b->py + b->uy * rs[i] * b->L + b->ux * -rw[i]; }
    g = shp_polygon(xy, 2 * n); free(xy); return g;
}
static int rbar_sect(void *c, double z, GEOSGeometry **out) {
    const RBar *b = (const RBar *)c; double *rs = (double *)malloc(sizeof(double) * (size_t)(b->m + 2)), *rw = (double *)malloc(sizeof(double) * (size_t)(b->m + 2));
    GEOSGeometry **os = (GEOSGeometry **)malloc(sizeof(GEOSGeometry *) * (size_t)(b->m + 2)); int nrun = 0, no = 0, bad = 0, e;
    *out = NULL;
    for (int i = 0; i <= b->m + 1 && !bad; i++) {
        double s = i <= b->m ? b->S[i] : 0.0, r = i <= b->m ? b->R[i] : 0.0, cc = i <= b->m ? b->C[i] : 0.0, w;
        if (z <= cc) w = r;
        else if (z < cc + r) { int pe = 0; double d = lf_fpow(z - cc, 2, &pe), v = r * r - d; w = sqrt(v > 0.0 ? v : 0.0); }
        else w = 0.0;
        if (w >= 0.2) { rs[nrun] = s; rw[nrun] = w; nrun++; continue; }
        if (nrun >= 2 && (rs[nrun - 1] - rs[0]) * b->L >= 0.3) {
            GEOSGeometry *g = rbar_poly(b, rs, rw, nrun); int v;
            if (!g) { bad = 1; break; }
            v = shp_is_valid(g);
            if (v < 0) { shp_free(g); bad = 1; break; }
            if (v && shp_area(g) > 0.05) os[no++] = g; else shp_free(g);
        }
        nrun = 0;
    }
    if (!bad && no) { *out = sd_U((const GEOSGeometry *const *)os, no); if (!*out) bad = 1; }
    for (int i = 0; i < no; i++) shp_free(os[i]);
    free(rs); free(rw); free(os); (void)e;
    return bad ? SO_ERR : SO_OK;
}
/* rf: rfn(ctx, s) or the number r; hz: z0 / z1 given */
static void rbar(SoSolids *o, P2 p, P2 q, SoNumFn rfn, void *ctx, double r, double k, int hz, double z0, double z1) {
    RBar *b; double L, top = 0;
    if (E_) return;
    L = fdist(p, q); if (L == 0.0) L = 1.0;
    b = (RBar *)calloc(1, sizeof(RBar)); b->L = L; { double t = L / 0.11; int m = (int)t; b->m = m > 8 ? m : 8; }
    b->ux = (q.x - p.x) / L; b->uy = (q.y - p.y) / L; b->px = p.x; b->py = p.y;
    b->S = (double *)malloc(sizeof(double) * (size_t)(b->m + 1)); b->R = (double *)malloc(sizeof(double) * (size_t)(b->m + 1)); b->C = (double *)malloc(sizeof(double) * (size_t)(b->m + 1));
    for (int i = 0; i <= b->m; i++) {
        double s = (double)i / b->m, v = r;
        b->S[i] = s;
        if (rfn && rfn(ctx, s, &v) != SO_OK) { rbar_free(b); fail(); return; }
        b->R[i] = MX(0.0, v); b->C[i] = hz ? z0 + (z1 - z0) * s : k * b->R[i];
    }
    for (int i = 0; i <= b->m; i++) { double v = b->C[i] + b->R[i]; if (i == 0 || v > top) top = v; }
    sol_push(o, sol_custom(rbar_sect, b, rbar_free, top)); sol_unref(o->v[o->n - 1]);
}

/* ma_glyphs is only used by builders drawn again later */

/* maahl scarab: a great dune beetle dead on its belly: wing cases, the shield, the horned head, six splayed legs */
BLD(b_maahl_scarab) {
    SoFrame ff = frame(cx, cy, a); double L = u(3.4, 4.2); double W = L * 0.66; double h = u(0.9, 1.15); SoObj *o = OBJ("chitin"); GL *legs = gl(); SoSolids *ps;
    double T[3][3] = { { -L * 0.22, -0.7, 1.3 }, { L * 0.05, -0.1, 1.4 }, { L * 0.28, 0.6, 1.2 } };
    for (int sv = -1; sv <= 1; sv += 2) for (int i = 0; i < 3; i++) {
        double x = T[i][0], dx = T[i][1], rc = T[i][2], j;
        if (rnd() < 0.12) continue;
        j = u(-0.2, 0.2); gl_add(legs, line(plv(3, F(&ff, x, sv * W * 0.3), F(&ff, x + dx * 0.4 + j, sv * (W / 2 + rc * 0.5)), F(&ff, x + dx + j * 2, sv * (W / 2 + rc))), 0.5));
    }
    if (legs->n) { ps = SL(); ext(ps, U(legs), 0.36, 0.0); o_st(o, ps, NULL, NOP, NULL, rect(cx, cy, L, W + 3.0, a)); }
    for (int sv = -1; sv <= 1; sv += 2) { P2 d = F(&ff, -L * 0.14, sv * W * 0.235), m = F(&ff, -L * 0.14, sv * W * 0.25); ps = SL(); dome(ps, d.x, d.y, L * 0.37, h, 0.0, (W * 0.26) / (L * 0.37), a, 20); o_st(o, ps, NULL, NOP, NULL, rect(m.x, m.y, L * 0.7, W / 2, a)); }
    { P2 d = F(&ff, L * 0.27, 0); ps = SL(); dome(ps, d.x, d.y, W * 0.46, h * 0.85, 0.0, 0.6, a + 1.5708, 20); o_st(o, ps, NULL, NOP, NULL, rect(d.x, d.y, W * 0.5, W * 0.9, a)); }
    { P2 d = F(&ff, L * 0.5, 0), m = F(&ff, L * 0.55, 0); double v1, v2; ps = SL(); dome(ps, d.x, d.y, W * 0.26, h * 0.5, 0.0, 0.8, a, 20);
      v1 = u(0.7, 1.1); v2 = u(-0.2, 0.2); rod(ps, plv(2, F(&ff, L * 0.5, 0), F(&ff, L * 0.5 + v1, v2)), 0.27, 1.3, 0.0, 0.0); o_st(o, ps, NULL, NOP, NULL, disc(m.x, m.y, 0.9, 20)); }
    RET(o_out(o, lay, NULL));
}

/* maahl geode: a split geode, the rind, the hollow, the ring of six-sided crystals; the smaller half beside it */
static void ge_half(SoLayers *lay, GL *foot, double x, double y, double r, double h) {
    SoSolids *ps = SL(), *cs = SL(); CL *cu = cl(); int m; PT *pts;
    rock(ps, x, y, r, h, 0.9, 0.0, 10); cl_add(cu, disc(x, y, r * 0.64, 12), h * 0.3); push(lay, ST(ps, "rock", NOP, cu));
    { double t = r * 0.42 * 6.28 / 0.85; int mi = (int)t; m = mi > 3 ? mi : 3; }
    { double st = u(0, 6.28); pts = ringpts(x, y, r * 0.4, m, st, 0.15, 1); }
    if (r > 1.5) pl_add(pts, p2(x, y));
    for (int i = 0; i < pts->n; i++) { P2 p = pl_at(pts, i); double rr = u(0.3, 0.4); double hh = u(0.75, 1.35) * (h / 1.2); double ta = u(0, 1); turn(cs, p.x, p.y, plv(3, p2(rr, 0.0), p2(rr, hh * 0.7), p2(0.2, hh)), 6, 1.0, ta, 0.0); }
    push(lay, ST(cs, "crystal", NOP, NULL)); gl_add(foot, disc(x, y, r + 0.2, 20));
}
BLD(b_maahl_geode) {
    GL *foot = gl(); double r = u(1.7, 2.2);
    { double h = u(1.0, 1.4); ge_half(lay, foot, cx, cy, r, h); }
    if (rnd() < 0.45) { double r2 = r * u(0.6, 0.75); double v = u(0.3, 0.8); P2 p = pol(cx, cy, r + r2 + v, a); double h = u(0.8, 1.0); ge_half(lay, foot, p.x, p.y, r2, h); }
    RET(hull(foot));
}

/* maahl saddlebags: two full pouches joined by the yoke strap, flaps, straps and buckles, tassels */
BLD(b_maahl_saddlebags) {
    SoFrame ff = frame(cx, cy, a); double s = u(1.9, 2.3); SoObj *o = OBJ("cloth"); int64_t y;
    { double w = u(1.5, 1.8); y = o_up(o, rect(cx, cy, 2 * s - 1.0, w, a), 0.3, "leather"); }
    o_cut(o, U(glv(2, barl(F(&ff, -s, -0.45), F(&ff, s, -0.45), 0.2), barl(F(&ff, -s, 0.45), F(&ff, s, 0.45), 0.2))), y, 0.15);
    for (int sv = -1; sv <= 1; sv += 2) {
        double v = u(-0.2, 0.2); P2 c = F(&ff, sv * s, v); double t = a + u(-0.15, 0.15); SoFrame f2 = frame(c.x, c.y, t); double gate_w = u(2.4, 2.8); GG *g = rrect(c.x, c.y, 2.1, gate_w, t, 0.55); int64_t b, fl; P2 q1 = F(&f2, -sv * 0.4, 0), q2 = F(&f2, -sv * 0.05, 0);
        b = o_up(o, g, u(0.8, 1.1), NULL);
        fl = o_on(o, rrect(q1.x, q1.y, 1.25, 2.3, t, 0.45), b, 0.17, NULL); (void)fl; o_on(o, rect(q2.x, q2.y, 1.7, 0.5, t), b, 0.3, "leather"); o_studs(o, plv(1, F(&f2, sv * 0.55, 0)), b, 0.3, 0.42, "brass", 8);
        if (rnd() < 0.7) {
            GL *ts = gl(); double vs[3] = { -0.75, 0, 0.75 }; SoSolids *ps = SL(); P2 m = F(&f2, sv * 1.5, 0);
            for (int i = 0; i < 3; i++) { double w1 = u(1.6, 1.9); double w2 = u(-0.3, 0.3); gl_add(ts, barl(F(&f2, sv * 1.1, vs[i]), F(&f2, sv * w1, vs[i] + w2), 0.46)); }
            ext(ps, U(ts), 0.25, 0.0); o_st(o, ps, NULL, NOP, NULL, disc(m.x, m.y, 0.9, 20));
        }
    }
    RET(o_out(o, lay, NULL));
}

/* maahl oil_lamp: the squat body on its foot, the filling hole, the long spout, the ring handle; the stopper on a chain */
static int ol_spout(void *c, double s, double *out) { (void)c; *out = 0.62 - 0.22 * s; return SO_OK; }
BLD(b_maahl_oil_lamp) {
    SoObj *o = OBJ("brass"); double r = u(1.05, 1.3); double h = u(0.8, 1.0); double Ls = u(1.5, 2.0); P2 q = pol(cx, cy, r + 0.5, a + PI_), e, w; SoSolids *ps = SL(); CL *cu;
    ext(ps, ann(q.x, q.y, 0.74, 0.27, 12), 0.5, 0.0); o_st(o, ps, NULL, NOP, NULL, disc(q.x, q.y, 0.7, 20));
    e = pol(cx, cy, r + Ls, a); w = pol(cx, cy, r + Ls - 0.42, a); (void)w; ps = SL(); rbar(ps, pol(cx, cy, r * 0.5, a), e, ol_spout, NULL, 0, 0.5, 0, 0, 0); o_st(o, ps, NULL, NOP, NULL, disc(e.x, e.y, 0.6, 20));
    ps = SL(); cu = cl();
    turn(ps, cx, cy, pl_cat(pl_cat(plv(4, p2(r * 0.62, 0.0), p2(r * 0.62, 0.12), p2(r, h * 0.45), p2(r, h * 0.55)), crv(r, 0.52, h * 0.55, h, 1, 6)), plv(1, p2(0.52, h + 0.14))), 24, 1.0, 0.0, 0.0);
    cl_add(cu, disc(cx, cy, 0.28, 8), h - 0.25); o_st(o, ps, NULL, NOP, cu, disc(cx, cy, r, 20));
    if (rnd() < 0.6) {
        double v1 = u(0.9, 1.4); int ch = rchi(2, -1, 1); double v2 = u(1.2, 1.9); P2 c = pol(cx, cy, r + v1, a + ch * v2); PT *pts = wiggle(pol(cx, cy, r + 0.1, ATAN2(c.y - cy, c.x - cx)), c, 0.25, 3);
        ps = SL(); rod(ps, pts, 0.25, 0.9, 0.0, 0.0); dome(ps, c.x, c.y, 0.4, 0.5, 0.0, 1.0, 0.0, 20); o_st(o, ps, NULL, NOP, NULL, disc(c.x, c.y, 0.5, 20));
    }
    RET(o_out(o, lay, NULL));
}

/* maahl salt_plates: a dried salt pan of six-sided plates, edges curled into a rim, a crack or two across each */
BLD(b_maahl_salt_plates) {
    SoObj *o = OBJ("salt"); double sp = u(2.0, 2.4); SoFrame ff = frame(cx, cy, a);
    for (int i = -1; i < 2; i++) for (int j = -1; j < 2; j++) {
        P2 c = F(&ff, i * sp + pmod(j, 2) * sp / 2 - sp * 0.25, j * sp * 0.87); PT *pg = pl(); GG *g; int64_t b;
        if ((i || j) && (fdist(c, p2(cx, cy)) > sp * 1.45 || rnd() < 0.15)) continue;
        for (int k = 0; k < 6; k++) { double v1 = u(0.9, 1.02); double v2 = u(-0.07, 0.07); pl_add(pg, pol(c.x, c.y, sp * 0.555 * v1, a + 0.5236 + 1.0472 * k + v2)); }
        g = Poly(pg); b = o_up(o, g, u(0.25, 0.32), NULL); { GG *rim = dif(g, inset(g, 0.5)); o_on(o, rim, b, u(0.2, 0.3), NULL); }
        if (rnd() < 0.5) { double t = u(0, 3.14); double v = u(-0.5, 0.5); GG *w = wavy(pol(c.x, c.y, sp * 0.4, t), pol(c.x, c.y, sp * 0.4, t + PI_ + v), 0.2, 3, 0.22); o_cut(o, isect(w, inset(g, 0.6)), b, 0.16); }
    }
    RET(o_out(o, lay, NULL));
}

/* maahl well_bucket: the staved pail with its ears and fallen bail (or tipped over, hoops showing), the rope to its coil */
typedef struct { double r; } WbC;
static int wb_hoop(double s) { return fabs(s - 0.2) < 0.07 || fabs(s - 0.78) < 0.07; }
static int wb_pail(void *c, double s, double *out) { double r = ((WbC *)c)->r; *out = r * (0.78 + 0.22 * s) + (wb_hoop(s) ? 0.12 : 0.0); return SO_OK; }
BLD(b_maahl_well_bucket) {
    SoObj *o = OBJ("wood"); double r = u(1.05, 1.3); SoFrame ff = frame(cx, cy, a); P2 p0; SoSolids *ps;
    if (rnd() < 0.55) {
        GL *gs = gl(); GG *g; int64_t b; double ls[12]; PT *pts = pl();
        gl_add(gs, disc(cx, cy, r, 20));
        for (int sv = -1; sv <= 1; sv += 2) { P2 q = pol(cx, cy, r + 0.1, a + sv * 1.5708); gl_add(gs, rect(q.x, q.y, 0.7, 0.6, a + sv * 1.5708)); }
        g = U(gs); b = o_up(o, g, u(1.3, 1.7), NULL); { GG *d = disc(cx, cy, r - 0.36, 16); o_cut(o, d, b, u(0.5, 0.9)); }
        { GL *st = gl(); for (int k = 0; k < 10; k++) gl_add(st, barl(pol(cx, cy, r - 0.4, a + 0.3 + 0.6283 * k), pol(cx, cy, r + 0.1, a + 0.3 + 0.6283 * k), 0.24)); o_cut(o, U(st), b, 0.16); }
        linspace(-1.35, 1.35, 12, ls);
        for (int i = 0; i < 12; i++) { double v = u(0.9, 1.2); pl_add(pts, pol(cx, cy, r + 0.35 + v * COS(ls[i]), a + ls[i])); }
        ps = SL(); ext(ps, line(pts, 0.48), 0.3, 0.0); o_st(o, ps, "iron", NOP, NULL, buf(Line(pts), 0.4, 16)); p0 = pol(cx, cy, r + 0.35, a + 2.4);
    } else {
        double L = u(2.3, 2.8), ls[10]; WbC wc = { r }; PT *pts = pl(); P2 e = F(&ff, L / 2, 0);
        ps = SL(); rbar(ps, F(&ff, -L / 2, 0), F(&ff, L / 2, 0), wb_pail, &wc, 0, 0.45, 0, 0, 0); o_st(o, ps, NULL, NOP, NULL, rect(cx, cy, L, 2 * r + 0.3, a));
        linspace(-1.3, 1.3, 10, ls);
        for (int i = 0; i < 10; i++) pl_add(pts, pol(e.x, e.y, 0.3 + 1.0 * COS(ls[i]), a + ls[i]));
        ps = SL(); ext(ps, line(pts, 0.48), 0.3, 0.0); o_st(o, ps, "iron", NOP, NULL, buf(Line(pts), 0.4, 16)); { double v = u(-0.4, 0.4); p0 = F(&ff, -L / 2 - 0.3, v); }
    }
    { double t = ATAN2(p0.y - cy, p0.x - cx) + u(-0.4, 0.4); double v = u(2.0, 2.6); P2 c = pol(p0.x, p0.y, v, t); double Rc = u(1.15, 1.4); double t1 = ATAN2(p0.y - c.y, p0.x - c.x); PT *w = wiggle(p0, pol(c.x, c.y, Rc, t1), 0.3, 3), *sp;
      double tu = u(2.0, 2.6); sp = spiral(c.x, c.y, Rc, 0.42, tu, t1, 34);
      ps = SL(); rod(ps, pl_cat(w, pl_slice(sp, 1, sp->n)), 0.3, 1.1, 0.0, 0.0); o_st(o, ps, "rope", NOP, NULL, disc(c.x, c.y, Rc + 0.3, 20)); }
    RET(o_out(o, lay, NULL));
}

/* maahl desert_rose: thin curved blades of crystal on edge, folded round one another, rising to the bud */
BLD(b_maahl_desert_rose) {
    SoObj *o = OBJ("sandstone"); double r = u(1.6, 2.1); double t0 = u(0, 6.28); int64_t b; int m;
    { double v = u(0.88, 0.96); GG *e = ell(cx, cy, r, r * v, t0, 18); b = o_up(o, e, u(0.3, 0.4), NULL); }
    m = ri(5, 6);
    for (int k = 0; k < m; k++) { double t = t0 + 6.2832 * k / m + u(-0.15, 0.15); P2 c = pol(cx, cy, r * 0.45, t); double v1 = u(0.9, 1.3); double v2 = u(0.9, 1.3); GG *g = line(arc(c.x, c.y, r * 0.44, t - v1, t + v2, 8), 0.5); o_on(o, g, b, u(0.3, 0.6), NULL); }
    for (int k = 0; k < 3; k++) { double t = t0 + 0.5 + 6.2832 * k / 3 + u(-0.2, 0.2); P2 c = pol(cx, cy, r * 0.2, t); double v1 = u(1.0, 1.4); double v2 = u(1.0, 1.4); GG *g = line(arc(c.x, c.y, r * 0.27, t - v1, t + v2, 8), 0.5); o_on(o, g, b, u(0.7, 0.95), NULL); }
    { GG *d = disc(cx, cy, 0.3, 8); o_on(o, d, b, u(1.0, 1.2), NULL); }
    RET(o_out(o, lay, NULL));
}

/* maahl fulgurite: the knobbly glass tube lightning leaves in sand, forking, thinner at every fork, ends hollow */
typedef struct { double r, L, ph; } FgC;
static int fg_tube(void *c, double q, double *out) { FgC *k = (FgC *)c; *out = k->r * (1 - 0.25 * q) * (1 + 0.1 * SIN(q * k->L * 6.0 + k->ph)); return SO_OK; }
static void fg_br(SoSolids *ps, GL *foot, CL *cuts, P2 p, double t, double L, double r, int dep) {
    P2 e = pol(p.x, p.y, L, t), m; double v = u(-0.4, 0.4), ph; int kids; FgC fc;
    m = pol((p.x + e.x) / 2, (p.y + e.y) / 2, v, t + 1.57); gl_add(foot, disc(e.x, e.y, 0.5, 20)); ph = u(0, 6);
    fc.r = r; fc.L = L; fc.ph = ph; rodf(ps, plv(3, p, m, e), fg_tube, &fc, 1.25, 0.0, 0.0);
    kids = dep < 2 ? ri(1, 2) : 0;
    if (kids) { for (int k = 0; k < kids && !E_; k++) { double t2 = t + u(-0.9, 0.9); double L2 = L * u(0.6, 0.8); fg_br(ps, foot, cuts, k == 0 ? e : m, t2, L2, r * 0.78, dep + 1); } }
    else if (r * 0.75 > 0.38) cl_add(cuts, disc(e.x, e.y, 0.2, 6), 0.25);
}
BLD(b_maahl_fulgurite) {
    SoSolids *ps = SL(); GL *foot = gl(); CL *cuts = cl(); GG *f;
    gl_add(foot, disc(cx, cy, 0.7, 20)); cl_add(cuts, disc(cx, cy, 0.24, 8), 0.3);
    { double L = u(2.4, 3.2); double r = u(0.58, 0.7); fg_br(ps, foot, cuts, p2(cx, cy), a, L, r, 0); }
    f = hull(foot); push(lay, ST(ps, "glass", NOP, cuts)); RET(f);
}

/* maahl spore_pods: leathery eggs half sunk in the sand, ridged, most split open at the top, one or two shut */
BLD(b_maahl_spore_pods) {
    GL *foot = gl(); double gx_[8], gy_[8], gr[8]; int ng = 0, n = ri(3, 6);
    for (int i = 0; i < n; i++) {
        double r = u(0.75, 1.1); double v1 = i == 0 ? 0 : u(1.6, 3.0); double v2 = u(-0.3, 0.3); P2 p = pol(cx, cy, v1, a + i * 1.25 + v2); int near = 0; double h, t, sq; CL *cu; SoSolids *ps;
        for (int j = 0; j < ng; j++) if (fdist(p, p2(gx_[j], gy_[j])) < r + gr[j] + 0.3) { near = 1; break; }
        if (near) continue;
        gx_[ng] = p.x; gy_[ng] = p.y; gr[ng] = r; ng++; h = r * u(1.1, 1.5); t = u(0, 6.28); sq = u(0.8, 0.95); cu = cl();
        for (int j = 0; j < 6; j++) cl_add(cu, barl(pol(p.x, p.y, r * 0.75, t + 6.2832 * j / 6), pol(p.x, p.y, r * 1.1, t + 6.2832 * j / 6), 0.22), h * 0.3);
        if (rnd() < 0.6) { double tg = t + 0.52; cl_add(cu, Poly(plv(3, pol(p.x, p.y, r * 0.1, tg + PI_), pol(p.x, p.y, r * 1.3, tg - 0.3), pol(p.x, p.y, r * 1.3, tg + 0.3))), h * 0.35); }
        ps = SL(); turn(ps, p.x, p.y, pl_cat(plv(2, p2(r * 0.85, 0.0), p2(r, h * 0.3)), crv(r, 0.32, h * 0.3, h, 1, 7)), 16, sq, t, 0.0);
        push(lay, ST(ps, "chitin", NOP, cu)); gl_add(foot, disc(p.x, p.y, r + 0.2, 20));
    }
    RET(hull(foot));
}

/* maahl water_skins: full bags swelling from their tied necks, the stopper and knot, the carrying cord loose */
typedef struct { double w, L; } WsC;
static int ws_bag(void *c, double t, double *out) {
    WsC *k = (WsC *)c; double s = (t + k->L / 2) / k->L, v;
    if (s < 0.88) v = k->w * fpw(SIN(MN(1.0, s / 0.72) * 1.5708), 0.6) * (s < 0.6 ? 1.0 : MX(0.3, 1 - fpw((s - 0.6) / 0.3, 1.6)));
    else v = s < 0.95 ? 0.3 : 0.42;
    *out = v + 0.02; return E_ ? SO_ERR : SO_OK;
}
BLD(b_maahl_water_skins) {
    SoObj *o = OBJ("leather"); int n = ri(2, 3);
    for (int i = 0; i < n; i++) {
        double v = i == 0 ? 0 : u(1.5, 2.0); P2 c = pol(cx, cy, v, a + 1.57 + i * 2.6); double b = a + u(-0.6, 0.6); double L = u(3.0, 3.8); double w = u(0.95, 1.2); SoFrame f2 = frame(c.x, c.y, b); WsC wc = { w, L }; SoSolids *ps = SL();
        spin(ps, c.x, c.y, b, L, ws_bag, &wc, 0.85, 0.0, "round"); o_st(o, ps, NULL, NOP, NULL, rect(c.x, c.y, L, 2 * w, b));
        if (rnd() < 0.7) { double v1 = u(-0.5, 0.8); int ch = rchi(2, -1, 1); double v2 = u(0.8, 1.3); P2 e = F(&f2, v1, ch * (w + v2)); PT *pts = plv(3, F(&f2, L * 0.4, 0), e, F(&f2, -L * 0.25, 0)); ps = SL(); rod(ps, pts, 0.26, 0.9, 0.0, 0.0); o_st(o, ps, "rope", NOP, NULL, buf(Line(pts), 0.4, 16)); }
    }
    RET(o_out(o, lay, NULL));
}

/* ====================================================================================================== sd_r_targa.py */
/* tg_bar: rbar (sd_r_maahl section). tg_drift(o, x, y, r, h, rng): snow blown up against a thing */
static void tg_drift(SoObj *o, double x, double y, double r, double h) {
    double sq = u(0.55, 0.8); double t = u(0, 3.14); SoSolids *ps = SL();
    dome(ps, x, y, r, h, 0.0, sq, t, 20); o_st(o, ps, "snow", NOP, NULL, disc(x, y, r * 0.8, 20));
}

/* targa fallen_icicles: long cones of ice lying where they broke, snapped flat at the root; chips of ice */
BLD(b_targa_fallen_icicles) {
    SoObj *o = OBJ("ice"); GL *foot = gl(); int n = ri(3, 5), m; static const double FH[3][2] = { { 1.0, 0.4 }, { 0.62, 0.8 }, { 0.3, 1.15 } };
    for (int i = 0; i < n; i++) {
        double v1 = i == 0 ? 0 : u(1.2, 2.6); double v2 = u(-0.3, 0.3); P2 p = pol(cx, cy, v1, a + i * 1.45 + v2); double b = a + u(-0.9, 0.9); double L, r, ph; SoFrame f2; SoSolids *ps = SL();
        b = b + (rnd() < 0.3 ? PI_ : 0); L = u(3.0, 5.0); r = u(0.55, 0.8); ph = u(0, 6); (void)ph; f2 = frame(p.x, p.y, b);
        for (int k = 0; k < 3; k++) { double f = FH[k][0], hh = FH[k][1]; ext(ps, Poly(plv(3, F(&f2, -L / 2, -r * f), F(&f2, -L / 2 + L * (0.35 + 0.65 * f), 0), F(&f2, -L / 2, r * f))), r * hh, 0.0); }
        o_st(o, ps, NULL, NOP, NULL, rect(p.x, p.y, L, 2 * r, b)); gl_add(foot, rect(p.x, p.y, L, 2 * r, b));
    }
    m = ri(2, 5);
    for (int i = 0; i < m; i++) { double v1 = u(1.0, 3.2); double v2 = u(0, 6.28); P2 p = pol(cx, cy, v1, v2); double rr = u(0.4, 0.6); GG *s = stone(p.x, p.y, rr, 0.6, 4, 0); SoSolids *ps = SL(); ext(ps, s, u(0.28, 0.4), 0.0); o_st(o, ps, NULL, NOP, NULL, disc(p.x, p.y, 0.5, 20)); gl_add(foot, disc(p.x, p.y, 0.5, 20)); }
    { GG *f = hull(foot); o_layers(o, lay); RET(f); }
}

/* targa pylon_stub: a six-sided foot plate with bolts, three buttress fins, the sheared column with its seams and socket */
BLD(b_targa_pylon_stub) {
    double r = u(1.25, 1.6); double h = u(1.3, 2.2); SoObj *o = OBJ("alien"); int64_t b, col; PT *st = pl(); GL *sm = gl();
    b = o_up(o, ngon(cx, cy, r * 1.75, 6, a), 0.4, NULL);
    for (int k = 0; k < 6; k++) pl_add(st, pol(cx, cy, r * 1.75 - 0.5, a + k * 1.0472));
    o_studs(o, st, b, 0.27, 0.16, NULL, 8);
    for (int k = 0; k < 3; k++) { double t = a + 0.5236 + k * 2.0944; P2 q = pol(cx, cy, r * 1.15, t); GG *g = rect(q.x, q.y, r * 0.9, 0.6, t); o_on(o, g, b, h * u(0.3, 0.5), NULL); }
    col = o_on(o, ngon(cx, cy, r, 6, a), b, h, NULL);
    for (int k = 0; k < 6; k++) gl_add(sm, barl(pol(cx, cy, r * 0.55, a + k * 1.0472), pol(cx, cy, r * 1.1, a + k * 1.0472), 0.22));
    o_cut(o, U(sm), col, 0.18);
    { GG *g = dif(ngon(cx, cy, r * 0.5, 6, a), disc(cx, cy, 0.3, 8)); o_cut(o, g, col, u(0.3, 0.5)); }
    if (rnd() < 0.5) { double t = u(0, 6.28); GG *g = dif(Poly(plv(3, pol(cx, cy, r * 0.3, t), pol(cx, cy, r * 1.3, t - 0.5), pol(cx, cy, r * 1.3, t + 0.5))), disc(cx, cy, r * 0.55, 20)); o_cut(o, g, col, u(0.3, 0.6)); }
    if (rnd() < 0.6) { double t = u(0, 6.28); P2 c = pol(cx, cy, r * 1.9, t); double dr = u(1.2, 1.7); double dh = u(0.5, 0.8); tg_drift(o, c.x, c.y, dr, dh); }
    RET(o_out(o, lay, NULL));
}

/* targa pressure_ridge: the floe broken along a line, its slabs driven up on edge and tilted; broken bits at their feet */
BLD(b_targa_pressure_ridge) {
    SoFrame ff = frame(cx, cy, a); int n = ri(4, 6), m; GL *foot = gl();
    for (int i = 0; i < n; i++) {
        double v1 = u(-0.2, 0.2); double v2 = u(-0.4, 0.4); P2 c = F(&ff, (i - (n - 1) / 2.0) * 1.5 + v1, v2); double t = a + 1.5708 + u(-0.5, 0.5) + (i % 2 ? PI_ : 0.0); SoFrame f2 = frame(c.x, c.y, t);
        double L = u(2.2, 3.2); double W = u(1.5, 2.1); double h = u(1.0, 1.9); double lx[4], lv[4]; SoSolids *ps = SL(); PT *fp = pl();
        lx[0] = -L / 2; lv[0] = -W / 2;
        lx[1] = L / 2 + u(-0.4, 0.2); lv[1] = -W / 2 + u(-0.2, 0.3);
        lx[2] = L / 2 + u(-0.3, 0.3); lv[2] = W / 2;
        lx[3] = -L / 2 + u(-0.2, 0.4); lv[3] = W / 2 + u(-0.3, 0.2);
        for (int k = 0; k < 5; k++) { PT *q = pl(); for (int j = 0; j < 4; j++) pl_add(q, F(&f2, lx[j] * (1 - 0.03 * k), W / 2 - (W / 2 - lv[j]) * (1 - 0.17 * k))); ext(ps, Poly(q), h * (k + 1) / 5, 0.0); }
        push(lay, ST(ps, "ice", NOP, NULL)); for (int j = 0; j < 4; j++) pl_add(fp, F(&f2, lx[j], lv[j])); gl_add(foot, Poly(fp));
    }
    m = ri(2, 4);
    for (int i = 0; i < m; i++) {
        double v1 = u(-n * 0.7, n * 0.7); int ch = rchi(2, -1, 1); double v2 = u(1.5, 2.1); P2 p = F(&ff, v1, ch * v2); double rr = u(0.45, 0.7); GG *s = stone(p.x, p.y, rr, 0.7, 5, 0); SoSolids *ps = SL();
        ext(ps, s, u(0.3, 0.5), 0.0); push(lay, ST(ps, "ice", NOP, NULL)); gl_add(foot, disc(p.x, p.y, 0.6, 20));
    }
    RET(hull(foot));
}

/* targa glyph_tile: a six-sided floor tile, a border line, the raised sign, bolts, a corner under snow, cracked on some */
BLD(b_targa_glyph_tile) {
    double R = u(2.2, 2.8); int k = ri(3, 6); SoObj *o = OBJ("alien"); GG *g = ngon(cx, cy, R, 6, a); double t0 = a + u(0, 1.0); int64_t b; GL *sg = gl(); PT *st = pl();
    b = o_up(o, g, 0.45, NULL); o_groove(o, ngon(cx, cy, R * 0.82, 6, a), b, 0.2, 0.16);
    gl_add(sg, ann(cx, cy, R * 0.3, R * 0.3 - 0.46, 20));
    for (int j = 0; j < k; j++) gl_add(sg, barl(pol(cx, cy, R * 0.28, t0 + 6.2832 * j / k), pol(cx, cy, R * 0.6, t0 + 6.2832 * j / k), 0.46));
    for (int j = 0; j < k; j++) { P2 q = pol(cx, cy, R * 0.6, t0 + 6.2832 * j / k); gl_add(sg, disc(q.x, q.y, 0.34, 8)); }
    o_on(o, U(sg), b, 0.18, NULL);
    for (int j = 0; j < 6; j++) pl_add(st, pol(cx, cy, R * 0.9, a + j * 1.0472));
    o_studs(o, st, b, 0.25, 0.13, NULL, 8);
    if (rnd() < 0.35) o_cut(o, wavy(pol(cx, cy, R * 1.1, a + 1), pol(cx, cy, R * 1.1, a + 4), 0.4, 4, 0.26), b, 0.25);
    if (rnd() < 0.6) { double v = u(0, 6.28); P2 c = pol(cx, cy, R * 0.95, a + v); double dr = u(1.1, 1.5); double dh = u(0.5, 0.75); tg_drift(o, c.x, c.y, dr, dh); }
    RET(o_out(o, lay, NULL));
}

/* targa fishing_hole: the round hole cut through the ice, the chipped ice round it, the sawn plug, a rod on its rest, the catch */
BLD(b_targa_fishing_hole) {
    double R = u(1.3, 1.6); SoObj *o = OBJ("ice"); PT *rp; int nr, m; int64_t plg; P2 c, e, mm;
    o_gcut(o, disc(cx, cy, R, 16), 0.6, "water");
    nr = ri(5, 8); rp = ringpts(cx, cy, R + 0.75, nr, a + 0.9, 0.25, 1);
    for (int i = 0; i < rp->n; i++) {
        P2 p = pl_at(rp, i);
        if (fabs(fmodp(ATAN2(p.y - cy, p.x - cx) - a + 3.1416, 6.2832) - 3.1416) > 0.5) {
            double rr = u(0.5, 0.7); GG *s = stone(p.x, p.y, rr, 0.8, 5, 0); SoSolids *ps = SL(); ext(ps, s, u(0.35, 0.6), 0.0); o_st(o, ps, NULL, NOP, NULL, disc(p.x, p.y, 0.6, 20));
        }
    }
    { double v1 = u(2.3, 2.8); double v2 = u(1.8, 2.6); c = pol(cx, cy, R + v1, a + v2); }
    { GG *d = disc(c.x, c.y, R * 0.92, 16); plg = o_up(o, d, u(0.5, 0.7), NULL); }
    o_cut(o, barl(pol(c.x, c.y, R, a), pol(c.x, c.y, R, a + PI_), 0.2), plg, 0.15);
    { double v = u(3.2, 4.0); e = pol(cx, cy, R + v, a); } mm = pol(cx, cy, R + 1.5, a);
    { SoSolids *ps = SL(); ext(ps, U(glv(3, barl(pol(cx, cy, R + 0.4, a), e, 0.5), rect(mm.x, mm.y, 0.6, 1.3, a), disc(e.x, e.y, 0.42, 8))), 0.45, 0.0); o_st(o, ps, "wood", NOP, NULL, buf(Line(plv(2, mm, e)), 0.6, 16)); }
    m = ri(1, 3);
    for (int i = 0; i < m; i++) {
        double v = u(2.0, 2.6); P2 q = pol(cx, cy, R + v, a - 1.3 - i * 0.75); double t = u(0, 6.28); SoFrame f2 = frame(q.x, q.y, t); double l = u(0.85, 1.1); SoSolids *ps = SL(); CL *cu = cl(); P2 ey = F(&f2, l * 0.55, 0.05);
        ext(ps, U(glv(2, ell(q.x, q.y, l, l * 0.42, t, 14), Poly(plv(3, F(&f2, -l + 0.15, 0), F(&f2, -l - 0.6, 0.5), F(&f2, -l - 0.6, -0.5))))), 0.34, 0.0);
        cl_add(cu, disc(ey.x, ey.y, 0.13, 5), 0.2); o_st(o, ps, "bone", NOP, cu, disc(q.x, q.y, l, 20));
    }
    RET(o_out(o, lay, NULL));
}

/* targa thermal_grille: an oval plate bolted down, slots across it, the snow melted back to a wet edge */
BLD(b_targa_thermal_grille) {
    SoFrame ff = frame(cx, cy, a); double L = u(3.8, 4.8); double W = u(2.3, 2.9); SoObj *o = OBJ("alien"); GG *g = ell(cx, cy, L / 2, W / 2, a, 28), *inner; int64_t b; double xs[64]; int nx; GL *sl = gl();
    { double v = u(0.9, 1.3); o_gcut(o, dif(buf(g, v, 16), buf(g, 0.3, 16)), 0.22, "water"); }
    b = o_up(o, g, 0.42, NULL); inner = dif(ell(cx, cy, L / 2 - 0.55, W / 2 - 0.5, a, 24), rect(cx, cy, L, 0.46, a));
    nx = arange(-L / 2 + 0.8, L / 2 - 0.5, 0.62, xs, 64);
    for (int i = 0; i < nx; i++) gl_add(sl, barl(F(&ff, xs[i], -W), F(&ff, xs[i], W), 0.3));
    o_cut(o, isect(U(sl), inner), b, 0.26);
    o_studs(o, plv(2, F(&ff, -1 * (L / 2 - 0.32), 0), F(&ff, 1 * (L / 2 - 0.32), 0)), b, 0.25, 0.14, NULL, 8);
    RET(o_out(o, lay, NULL));
}

/* targa ice_spikes: sharp five- and six-sided blades of ice in a clump, leaning the way the wind carved them */
BLD(b_targa_ice_spikes) {
    GL *foot = gl(); double lean = a + u(-0.3, 0.3); int n = ri(4, 7);
    for (int i = 0; i < n; i++) {
        double v1 = i == 0 ? 0 : u(0.9, 2.1); double v2 = u(-0.3, 0.3); P2 p = pol(cx, cy, v1, a + i * 1.1 + v2); double r = u(0.75, 1.05) * (i < 2 ? 1.0 : 0.75); double h = MN(3.2, r * u(2.4, 3.2)); int k = rchi(2, 5, 6); double sq = u(0.9, 1.0); SoSolids *ps = SL();
        for (int j = 0; j < 10; j++) { double f = j / 9.0, w = MX(0.3, r * fpw(1 - f, 0.8)); P2 q = pol(p.x, p.y, r * 0.5 * f, lean); turn(ps, q.x, q.y, plv(2, p2(w, 0.0), p2(w, MX(0.12, h * f))), k, sq, lean, 0.0); }
        push(lay, ST(ps, "ice", NOP, NULL)); gl_add(foot, disc(p.x, p.y, r + 0.2, 20));
    }
    RET(hull(foot));
}

/* targa power_cell: a canister with collars, bands and a contact stud, in its cradle or stood on end; a dead one split beside it */
static int pc_prof(void *c, double s, double *out) { double r = *(double *)c; *out = r * (s < 0.14 || s > 0.86 ? 1.16 : fabs(s - 0.5) < 0.05 || fabs(s - 0.34) < 0.03 || fabs(s - 0.66) < 0.03 ? 1.08 : 1.0); return SO_OK; }
static int pc_dead(void *c, double s, double *out) { double r = *(double *)c; *out = r * (s < 0.28 ? 1.16 : 1.0); return SO_OK; }
BLD(b_targa_power_cell) {
    SoFrame ff = frame(cx, cy, a); double L = u(3.6, 4.4); double r = u(0.8, 1.0); SoObj *o = OBJ("alien"); SoSolids *ps;
    if (rnd() < 0.65) {
        P2 q1 = F(&ff, -1 * L * 0.3, 0), q2 = F(&ff, 1 * L * 0.3, 0), e = F(&ff, L / 2 + 0.3, 0);
        o_up(o, U(glv(2, rect(q1.x, q1.y, 0.8, 2 * r + 1.1, a), rect(q2.x, q2.y, 0.8, 2 * r + 1.1, a))), 0.45, NULL);
        ps = SL(); rbar(ps, F(&ff, -L / 2, 0), F(&ff, L / 2, 0), pc_prof, &r, 0, 0.5, 0, 0, 0); o_st(o, ps, NULL, NOP, NULL, rect(cx, cy, L, 2 * r, a));
        ps = SL(); rbar(ps, F(&ff, L / 2, 0), F(&ff, L / 2 + 0.5, 0), NULL, NULL, 0.4, 0.5, 1, 0.5 * r, 0.5 * r); o_st(o, ps, NULL, NOP, NULL, disc(e.x, e.y, 0.5, 20));
    } else {
        double h = L * 0.62;
        ps = SL(); turn(ps, cx, cy, plv(12, p2(r * 1.16, 0.0), p2(r * 1.16, h * 0.16), p2(r, h * 0.18), p2(r, h * 0.46), p2(r * 1.08, h * 0.48), p2(r * 1.08, h * 0.56), p2(r, h * 0.58), p2(r, h * 0.84), p2(r * 1.16, h * 0.86), p2(r * 1.16, h), p2(0.4, h + 0.02), p2(0.4, h + 0.35)), 20, 1.0, 0.0, 0.0);
        o_st(o, ps, NULL, NOP, NULL, disc(cx, cy, r * 1.2, 20));
    }
    if (rnd() < 0.45) {
        double v1 = u(-1, 1); double v2 = u(1.6, 2.2); P2 c = F(&ff, v1, -r - v2); double t = a + u(-0.5, 0.5); SoFrame f2 = frame(c.x, c.y, t); double l2 = L * 0.5; P2 bq = F(&f2, l2 / 2 + 0.8, 0);
        ps = SL(); rbar(ps, F(&f2, -l2 / 2, 0), F(&f2, l2 / 2, 0), pc_dead, &r, 0, 0.5, 0, 0, 0); o_st(o, ps, NULL, NOP, NULL, rect(c.x, c.y, l2, 2 * r, t));
        { double rb = u(0.7, 1.0); o_gcut(o, blob(bq.x, bq.y, rb, 3), 0.22, "water"); }
    }
    RET(o_out(o, lay, NULL));
}

/* ====================================================================================================== sd_r_trederra.py */
/* t_along(pts, step, first=None): points along a bent line, one every `step`, each with the line's direction there */
typedef struct { P2 p; double t; } TAl;
static int t_along(const PT *pts, double step, TAl *out, int cap) {
    GG *ln = Line(pts); double L, s = step * 0.5; int n = 0;
    if (E_) return 0;
    L = shp_length(ln);
    while (s < L - step * 0.3 && n < cap) {
        GG *p = interp(ln, s, 0), *q = interp(ln, MN(L, s + 0.1), 0);
        out[n].p = p2(gx(p), gy(p)); out[n].t = ATAN2(gy(q) - gy(p), gx(q) - gx(p)); n++; s += step;
    }
    return n;
}
/* t_bar: rbar (sd_r_maahl section). t_belt(o, pts, kind="cloth"): a cartridge belt, a round across it every little way */
static int64_t t_belt(SoObj *o, const PT *pts, const char *kind) {
    int64_t b = o_up(o, line(pts, 1.3), 0.28, kind); TAl al[256]; int n = t_along(pts, 0.95, al, 256); GL *rs = gl();
    for (int i = 0; i < n; i++) gl_add(rs, barl(pol(al[i].p.x, al[i].p.y, 0.42, al[i].t + 1.5708), pol(al[i].p.x, al[i].p.y, 0.42, al[i].t - 1.5708), 0.5));
    o_on(o, U(rs), b, 0.3, "brass");
    return b;
}
/* t_tin_box(o, cx, cy, a, L, W, h, rng, open_=False): an ammunition tin, shut (lid, handle, latch, hinges) or open */
static int64_t t_tin_box(SoObj *o, double cx, double cy, double a, double L, double W, double h, int open_) {
    SoFrame ff = frame(cx, cy, a); GG *g = rrect(cx, cy, L, W, a, 0.15); int64_t b = o_up(o, g, h, "tin");
    if (open_) {
        double xs[64]; int nx = arange(-L / 2 + 0.55, L / 2 - 0.4, 0.6, xs, 64); GL *rs = gl(); GG *lg; int64_t l2;
        for (int i = 0; i < nx; i++) { P2 q = F(&ff, xs[i], 0); gl_add(rs, rect(q.x, q.y, 0.32, W, a)); }
        o_cut(o, dif(rect(cx, cy, L - 0.5, W - 0.5, a), U(rs)), b, 0.4);
        { double v1 = u(-0.3, 0.3); P2 q = F(&ff, v1, W + 0.25); double v2 = u(-0.2, 0.2); lg = rrect(q.x, q.y, L, W, a + v2, 0.15); }
        l2 = o_up(o, lg, 0.26, "tin"); o_groove(o, inset(lg, 0.25), l2, 0.2, 0.15);
    } else {
        int64_t lid = o_lid(o, g, b, 0.17, 0.13, "tin"); double x1[1] = { L / 2 - 0.62 }, x2[1] = { -L / 2 + 0.6 };
        o_handle(o, F(&ff, -L * 0.2, 0), F(&ff, L * 0.2, 0), lid, 0.46, 0.36, "tin");
        o_tabs(o, &ff, 1, x1, 0, lid, a, 0.5, 0.7, 0.2, "tin"); o_tabs(o, &ff, 1, x2, W * 0.24, lid, a, 0.5, 0.5, 0.18, "tin"); o_tabs(o, &ff, 1, x2, -W * 0.24, lid, a, 0.5, 0.5, 0.18, "tin");
    }
    return b;
}

/* trederra wire_coil: a coil of barbed wire as it comes off the wagon, barbs all round, ties, the loose end run out */
static GL *wc_barbs(GL *gs, double cx, double cy, double r, int m, double ph) {
    for (int j = 0; j < m; j++) { P2 p = pol(cx, cy, r, ph + 6.2832 * j / m); double tb = ph + 6.2832 * j / m + rchd(2, 0.6, -0.6); gl_add(gs, barl(pol(p.x, p.y, 0.55, tb), pol(p.x, p.y, 0.55, tb + PI_), 0.36)); }
    return gs;
}
BLD(b_trederra_wire_coil) {
    SoObj *o = OBJ("wire"); int n = ri(3, 4); double R = u(2.0, 2.3) + 0.3 * (n - 3); double t0 = u(0, 6.28); double Ri = R - (n - 1) * 0.62; GL *bs = gl(); SoSolids *ps;
    { int m1 = (int)(R * 6.28 / 1.7), m2 = (int)(Ri * 6.28 / 1.7); if (m2 < 3) m2 = 3; wc_barbs(bs, cx, cy, R + 0.1, m1, t0); wc_barbs(bs, cx, cy, Ri - 0.1, m2, t0 + 0.5); }
    ps = SL(); ext(ps, U(bs), 0.3, 0.0); o_st(o, ps, NULL, NOP, NULL, disc(cx, cy, R + 0.6, 20));
    ps = SL();
    for (int k = 0; k < n; k++) { double v1 = u(-0.06, 0.06); double v2 = u(-0.06, 0.06); rod(ps, arc(cx + v1, cy + v2, R - k * 0.62, t0, t0 + 6.2, 30), 0.33, k % 2 == 0 ? 1.5 : 1.9, 0.0, 0.0); }
    o_st(o, ps, NULL, NOP, NULL, disc(cx, cy, R + 0.4, 20));
    { GL *ts = gl(); for (int k = 0; k < 2; k++) { double t = t0 + 2.6 * k + u(-0.4, 0.4); P2 q = pol(cx, cy, (R + Ri) / 2, t); gl_add(ts, rect(q.x, q.y, R - Ri + 0.9, 0.5, t)); }
      ps = SL(); ext(ps, U(ts), 0.74, 0.0); o_st(o, ps, "rope", NOP, NULL, disc(cx, cy, R, 20)); }
    { double t = a + u(-0.5, 0.5); double v = u(2.2, 3.6); P2 e = pol(cx, cy, R + v, t); PT *pts = wiggle(pol(cx, cy, R + 0.75, t), e, 0.6, 4); TAl al[64]; int na = t_along(pts, 1.1, al, 64); GL *gs = gl();
      gl_add(gs, line(pts, 0.5));
      for (int i = 0; i < na; i++) gl_add(gs, barl(pol(al[i].p.x, al[i].p.y, 0.5, al[i].t + 0.9), pol(al[i].p.x, al[i].p.y, 0.5, al[i].t + 0.9 + PI_), 0.36));
      ps = SL(); ext(ps, U(gs), 0.4, 0.0); o_st(o, ps, NULL, NOP, NULL, buf(Line(pts), 0.6, 16)); }
    RET(o_out(o, lay, NULL));
}

/* trederra tin_hat: a soup-plate steel helmet, rolled brim, round crown, chin strap; shot through or upside down on some */
BLD(b_trederra_tin_hat) {
    double R = u(1.7, 2.0); double sq = u(0.86, 0.94); double rc = R * 0.66; SoObj *o = OBJ("tin"); double t = a + u(0, 6.28); PT *pts; SoSolids *ps; double h, tb; CL *cu = cl();
    { P2 c = pol(cx, cy, R + 0.5, t + 0.3); double rr = u(0.8, 1.1); pts = pl_cat(pl_cat(plv(1, pol(cx, cy, R - 0.3, t)), arc(c.x, c.y, rr, t - 1.2, t + 1.9, 10)), plv(1, pol(cx, cy, R - 0.3, t + 0.9))); }
    ps = SL(); ext(ps, line(pts, 0.5), 0.26, 0.0); o_st(o, ps, "leather", NOP, NULL, buf(Line(pts), 0.5, 16));
    if (rnd() < 0.3) {
        int64_t b = o_up(o, ell(cx, cy, R, R * sq, a, 28), 0.5, NULL), bw = o_on(o, ell(cx, cy, rc + 0.3, (rc + 0.3) * sq, a, 28), b, 0.5, NULL); GL *gs = gl();
        for (int k = 0; k < 2; k++) gl_add(gs, barl(pol(cx, cy, rc, a + k * 1.5708), pol(cx, cy, rc, a + k * 1.5708 + PI_), 0.46));
        gl_add(gs, disc(cx, cy, 0.5, 10));
        o_cut(o, dif(ell(cx, cy, rc, rc * sq, a, 28), U(gs)), bw, 0.5);
        RET(o_out(o, lay, NULL));
    }
    h = u(1.1, 1.35);
    tb = a + u(0, 6.28); if (rnd() < 0.3) cl_add(cu, Poly(plv(3, pol(cx, cy, R * 0.72, tb), pol(cx, cy, R * 1.2, tb - 0.22), pol(cx, cy, R * 1.2, tb + 0.22))), 0.0);          /* a bite out of the brim */
    ps = SL(); turn(ps, cx, cy, pl_cat(plv(4, p2(R, 0.0), p2(R, 0.2), p2(R - 0.22, 0.22), p2(rc + 0.1, 0.42)), crv(rc, 0.3, 0.42, h, 1, 8)), 28, sq, a, 0.0); o_st(o, ps, NULL, NOP, cu, NULL);
    RET(o_out(o, lay, NULL));
}

/* trederra gas_mask: the face piece with its eye pieces and valve, head straps, the ribbed hose to the filter tin in its satchel */
static int gm_hose(void *c, double q, double *out) { (void)c; *out = 0.36 + 0.1 * (SIN(q * 45) > 0); return SO_OK; }
BLD(b_trederra_gas_mask) {
    SoFrame ff = frame(cx, cy, a); SoObj *o = OBJ("cloth"); GG *g; int64_t b, r1, s, fl; P2 ey[2], c; double t; SoFrame f2; PT *pts; SoSolids *ps;
    { P2 d1 = F(&ff, -0.35, 0.6), d2 = F(&ff, -0.35, -0.6), d3 = F(&ff, 0.85, 0); g = hull(glv(3, disc(d1.x, d1.y, 0.8, 20), disc(d2.x, d2.y, 0.8, 20), disc(d3.x, d3.y, 0.62, 20))); }
    for (int sv = -1; sv <= 1; sv++) { double v1 = u(1.9, 2.4); double v2 = u(0.8, 1.2); P2 e = F(&ff, -v1, sv * v2); pts = plv(2, F(&ff, -0.8, sv * 0.6), e); ps = SL(); ext(ps, line(pts, 0.5), 0.24, 0.0); o_st(o, ps, "leather", NOP, NULL, buf(Line(pts), 0.4, 16)); }
    b = o_up(o, g, u(0.6, 0.75), NULL); ey[0] = F(&ff, -0.35, -0.62); ey[1] = F(&ff, -0.35, 0.62);
    r1 = o_on(o, U(glv(2, disc(ey[0].x, ey[0].y, 0.48, 14), disc(ey[1].x, ey[1].y, 0.48, 14))), b, 0.2, "tin"); o_cut(o, U(glv(2, disc(ey[0].x, ey[0].y, 0.27, 10), disc(ey[1].x, ey[1].y, 0.27, 10))), r1, 0.2);
    { P2 v = F(&ff, 0.75, 0); o_on(o, disc(v.x, v.y, 0.46, 12), b, 0.36, "tin"); }
    { double v1 = u(3.2, 4.0); double v2 = u(-1.6, 1.6); c = F(&ff, v1, v2); } t = a + u(-0.7, 0.7); f2 = frame(c.x, c.y, t); pts = wiggle(F(&ff, 1.5, 0), F(&f2, -1.2, 0), 0.7, 4);
    ps = SL(); rodf(ps, pts, gm_hose, NULL, 1.0, 0.0, 0.0); o_st(o, ps, "cable", NOP, NULL, buf(Line(pts), 0.5, 16));
    { GG *sg = rrect(c.x, c.y, 2.1, 1.7, t, 0.3); P2 q = F(&f2, -0.35, 0); s = o_up(o, sg, u(0.7, 0.9), NULL); fl = o_on(o, rrect(q.x, q.y, 1.3, 1.6, t, 0.25), s, 0.15, NULL); }
    o_studs(o, plv(2, F(&f2, 0.1, -0.45), F(&f2, 0.1, 0.45)), fl, 0.25, 0.13, "brass", 8);
    { double v1 = u(0.5, 1.2); int ch = rchi(2, -1, 1); double v2 = u(1.8, 2.4); P2 e = F(&f2, v1, ch * v2); pts = rnd() < 0.5 ? plv(3, F(&f2, 0.7, 0.6), e, F(&f2, 0.7, -0.6)) : plv(2, F(&f2, 0.7, 0), e); }
    ps = SL(); ext(ps, line(pts, 0.5), 0.24, 0.0); o_st(o, ps, "leather", NOP, NULL, buf(Line(pts), 0.4, 16));
    RET(o_out(o, lay, NULL));
}

/* trederra shell_cases: spent shell cases, rim at the base, shoulder to the mouth; a heap of them, or thrown down, some standing */
typedef struct { double r; } ScC;
static int sc_case(void *c, double s, double *out) { double r = ((ScC *)c)->r; *out = r * (s < 0.1 ? 1.17 : s < 0.66 ? 1.0 : 1.0 - 0.2 * MN(1.0, (s - 0.66) / 0.08)); return SO_OK; }
static void sc_one(SoObj *o, double r, double L, double x, double y, double b, double lift) {
    SoFrame f2 = frame(x, y, b); int sv = rchi(2, -1, 1); ScC sc = { r }; SoSolids *ps = SL();
    rbar(ps, F(&f2, -sv * L / 2, 0), F(&f2, sv * L / 2, 0), sc_case, &sc, 0, 0.4, 1, lift + 0.4 * r, lift + 0.4 * r); o_st(o, ps, NULL, NOP, NULL, rect(x, y, L, 2 * r, b));
}
BLD(b_trederra_shell_cases) {
    SoObj *o = OBJ("brass"); SoFrame ff = frame(cx, cy, a); double r = u(0.62, 0.74); double L = u(3.4, 4.2); int m;
    if (rnd() < 0.5) {
        int n = ri(2, 4), n2;
        for (int i = 0; i < n; i++) { double v = u(-0.3, 0.3); P2 c = F(&ff, v, (i - (n - 1) / 2.0) * (2 * r + 0.1)); double b = a + u(-0.06, 0.06); sc_one(o, r, L, c.x, c.y, b, 0.0); }
        n2 = ri(1, n - 1);
        for (int i = 0; i < n2; i++) { double v = u(-0.4, 0.4); P2 c = F(&ff, v, (i - (n - 2) / 2.0) * (2 * r + 0.1)); double b = a + u(-0.12, 0.12); sc_one(o, r, L, c.x, c.y, b, r * 1.25); }
    } else {
        int n = ri(2, 4);
        for (int i = 0; i < n; i++) { double v = i == 0 ? 0 : u(1.6, 2.8); P2 c = pol(cx, cy, v, a + i * 1.7); double b = a + u(-1.2, 1.2); sc_one(o, r, L, c.x, c.y, b, 0.0); }
    }
    m = ri(1, 2);
    for (int i = 0; i < m; i++) {
        double v1 = u(-1.5, 1.5); double v2 = u(3.2, 3.9); P2 p = F(&ff, v1, (i % 2 ? -1 : 1) * v2); double h = L * 0.75; SoSolids *ps = SL(); CL *cu = cl();
        turn(ps, p.x, p.y, plv(6, p2(r * 1.18, 0.0), p2(r * 1.18, 0.22), p2(r, 0.26), p2(r, h * 0.72), p2(r * 0.82, h * 0.82), p2(r * 0.82, h)), 16, 1.0, 0.0, 0.0);
        cl_add(cu, disc(p.x, p.y, r * 0.82 - 0.2, 10), h - 0.7); o_st(o, ps, NULL, NOP, cu, disc(p.x, p.y, r * 1.2, 20));
    }
    RET(o_out(o, lay, NULL));
}

/* trederra ammo_box: an ammunition tin and a belt of rounds run out of it along the ground */
BLD(b_trederra_ammo_box) {
    SoFrame ff = frame(cx, cy, a); double L = u(3.0, 3.6); double W = u(1.6, 1.9); double h = u(1.0, 1.3); SoObj *o = OBJ("tin");
    { int op = rnd() < 0.4; t_tin_box(o, cx, cy, a, L, W, h, op); }
    { double v1 = u(2.2, 3.4); double v2 = u(0.8, 2.0); P2 e = F(&ff, L / 2 + v1, -v2); PT *w = wiggle(F(&ff, L / 2 + 0.7, -W * 0.3), e, 0.5, 4); t_belt(o, pl_cat(plv(1, F(&ff, L / 2 + 0.1, -W * 0.2)), w), "cloth"); }
    RET(o_out(o, lay, NULL));
}

/* trederra periscope: the long square tube with its clamp bands, the head box with its window, the eyepiece, the grip */
BLD(b_trederra_periscope) {
    SoFrame ff = frame(cx, cy, a); double L = u(6.0, 7.5); SoObj *o = OBJ("tin"); int64_t b, hd, ey; GG *wn; double xs[2]; int sv;
    b = o_up(o, rect(cx, cy, L, 1.0, a), 0.7, NULL); xs[0] = -L * 0.18; xs[1] = L * 0.14; o_straps(o, &ff, 2, xs, 0.2, b, a, 0.5, 0.14, NULL);
    { P2 q = F(&ff, L / 2 - 0.5, 0), w = F(&ff, L / 2 - 0.45, 0); hd = o_up(o, rrect(q.x, q.y, 1.7, 1.5, a, 0.15), 1.0, NULL); wn = rect(w.x, w.y, 1.0, 0.9, a); }
    o_cut(o, wn, hd, 0.35); o_groove(o, bufx(wn, 0.2, 16, SHP_CAP_ROUND, SHP_JOIN_MITRE, 5.0), hd, 0.17, 0.12);
    sv = rchi(2, -1, 1); { P2 q = F(&ff, -L / 2 + 0.6, sv * 0.8), d = F(&ff, -L / 2 + 0.6, sv * 0.85); ey = o_up(o, rect(q.x, q.y, 1.0, 1.0, a), 0.7, NULL); o_on(o, disc(d.x, d.y, 0.42, 12), ey, 0.22, NULL); }
    { double v = u(1.4, 1.9); SoSolids *ps = SL(); P2 m = F(&ff, -L / 2 - 1.0, 0); rod(ps, plv(2, F(&ff, -L / 2 - 0.1, 0), F(&ff, -L / 2 - v, 0)), 0.4, 1.1, 0.0, 0.0); o_st(o, ps, "wood", NOP, NULL, disc(m.x, m.y, 1.0, 20)); }
    RET(o_out(o, lay, NULL));
}

/* trederra field_telephone: the wooden case and lid, the handset in its rest, terminals, crank, strap, the line to its drum */
BLD(b_trederra_field_telephone) {
    SoFrame ff = frame(cx, cy, a); double L = u(2.9, 3.4); double W = u(2.0, 2.3); double h = u(1.0, 1.3); SoObj *o = OBJ("wood"); GG *g = rrect(cx, cy, L, W, a, 0.2); PT *p0, *pts; SoSolids *ps; int64_t b, lid, dr; double v; P2 k, k2, c; double R;
    { P2 ac = F(&ff, 0, W / 2 + 0.4); PT *ar = arc(ac.x, ac.y, L * 0.3, a + PI_, a, 10); p0 = pl_cat(pl_cat(plv(1, F(&ff, -L * 0.3, W / 2 - 0.2)), pl_slice(ar, 1, ar->n - 1)), plv(1, F(&ff, L * 0.3, W / 2 - 0.2))); }
    pts = pl(); for (int i = 0; i < p0->n; i++) { double v1 = u(-0.1, 0.1); double v2 = u(-0.1, 0.1); pl_add(pts, p2(p0->xy[2 * i] + v1, p0->xy[2 * i + 1] + v2)); }
    ps = SL(); ext(ps, line(pts, 0.5), 0.24, 0.0); o_st(o, ps, "leather", NOP, NULL, buf(Line(pts), 0.4, 16));
    b = o_up(o, g, h, NULL); lid = o_lid(o, g, b, 0.16, 0.15, NULL); v = W * 0.14;
    { P2 d1 = F(&ff, -1 * L * 0.3, v), d2 = F(&ff, 1 * L * 0.3, v); o_on(o, U(glv(2, disc(d1.x, d1.y, 0.5, 12), disc(d2.x, d2.y, 0.5, 12))), lid, 0.4, "tar"); }
    o_on(o, capl(F(&ff, -L * 0.3, v), F(&ff, L * 0.3, v), 0.46), lid, 0.28, "tar");
    o_studs(o, plv(2, F(&ff, -1 * 0.45, -W * 0.3), F(&ff, 1 * 0.45, -W * 0.3)), lid, 0.27, 0.2, "brass", 8);
    k = F(&ff, L / 2 + 0.25, -W * 0.1); k2 = F(&ff, L / 2 + 0.3, -W * 0.1 + 0.8);
    ps = SL(); ext(ps, U(glv(3, rect(k.x, k.y, 0.6, 0.5, a), capl(k, k2, 0.46), disc(k2.x, k2.y, 0.32, 8))), h * 0.6, 0.0); cyl(ps, k2.x, k2.y, 0.32, h * 0.6 + 0.3, 0.0, 8); o_st(o, ps, "iron", NOP, NULL, disc(k2.x, k2.y, 0.5, 20));
    R = u(0.95, 1.15); { double v1 = u(-1.0, 1.0); double v2 = u(2.2, 3.0); c = F(&ff, v1, -W / 2 - v2); } pts = wiggle(F(&ff, 0, -W / 2 - 0.25), pol(c.x, c.y, R + 0.2, a + 1.57), 0.5, 3);
    ps = SL(); rod(ps, pts, 0.28, 1.0, 0.0, 0.0); o_st(o, ps, "wire", NOP, NULL, buf(Line(pts), 0.4, 16));
    dr = o_up(o, disc(c.x, c.y, R, 20), 0.3, "wood"); o_on(o, ann(c.x, c.y, R - 0.12, R - 0.6, 20), dr, 0.4, "wire"); o_on(o, disc(c.x, c.y, 0.36, 10), dr, 0.62, "wood");
    RET(o_out(o, lay, NULL));
}

/* trederra ration_tins: round tins, one opened with its lid bent back, a bully-beef tin and its key, a mess tin, a spoon */
BLD(b_trederra_ration_tins) {
    SoObj *o = OBJ("tin"); int n = ri(3, 5); int64_t kinds[4] = { 0, 1, 2, 3 };          /* "round", "open", "bully", "mess" */
    if (!E_) py_shuffle_i64(BA_->rng, kinds, 4);
    for (int i = 0; i < n; i++) {
        double v1 = i == 0 ? 0 : u(1.9, 2.6); double v2 = u(-0.3, 0.3); P2 p = pol(cx, cy, v1, a + i * 6.2832 / (n - 1 > 1 ? n - 1 : 1) + v2); double t = u(0, 6.28); SoFrame f2 = frame(p.x, p.y, t); int64_t k = kinds[i % 4], b;
        if (k == 0) {
            double r = u(0.6, 0.75); GG *d = disc(p.x, p.y, r, 16); b = o_up(o, d, u(0.7, 1.1), NULL); o_on(o, ann(p.x, p.y, r, r - 0.3, 16), b, 0.14, NULL); o_cut(o, ann(p.x, p.y, r * 0.4, r * 0.4 - 0.18, 10), b, 0.12);
        } else if (k == 1) {
            double r = u(0.65, 0.8); GG *d = disc(p.x, p.y, r, 16); P2 c; b = o_up(o, d, u(0.8, 1.1), NULL); o_cut(o, disc(p.x, p.y, r - 0.26, 12), b, 0.5);
            c = pol(p.x, p.y, r + 0.35, t); o_up(o, star(c.x, c.y, r - 0.1, r - 0.3, 9, t), 0.3, NULL);
        } else if (k == 2) {
            GG *g = buf(buf(Poly(plv(4, F(&f2, -0.9, -0.6), F(&f2, 0.9, -0.48), F(&f2, 0.9, 0.48), F(&f2, -0.9, 0.6))), -0.12, 16), 0.12, 6); double v; SoSolids *ps = SL(); P2 an = F(&f2, 1.75, 0), m = F(&f2, 1.5, 0);
            b = o_up(o, g, u(0.7, 0.9), NULL); o_lid(o, g, b, 0.13, 0.16, NULL);
            v = u(-0.3, 0.3); ext(ps, U(glv(2, capl(F(&f2, 1.0, 0), F(&f2, 1.6, v), 0.46), ann(an.x, an.y, 0.4, 0.17, 8))), 0.26, 0.0); o_st(o, ps, "iron", NOP, NULL, disc(m.x, m.y, 0.6, 20));
        } else {
            P2 d1 = F(&f2, 0, -0.45), d2 = F(&f2, 0, 0.45), d3 = F(&f2, 0.25, 0), m = F(&f2, -1.2, 0); GG *g = hull(glv(3, disc(d1.x, d1.y, 0.62, 12), disc(d2.x, d2.y, 0.62, 12), disc(d3.x, d3.y, 0.6, 12))); SoSolids *ps = SL();
            b = o_up(o, g, u(0.8, 1.0), NULL); o_cut(o, inset(g, 0.26), b, 0.45);
            ext(ps, line(plv(4, F(&f2, -0.5, 0.3), F(&f2, -1.6, 0.22), F(&f2, -1.6, -0.22), F(&f2, -0.5, -0.3)), 0.46), 0.3, 0.0); o_st(o, ps, "iron", NOP, NULL, disc(m.x, m.y, 0.6, 20));
        }
    }
    { double v1 = u(1.0, 1.4); double v2 = u(0, 6.28); P2 p = pol(cx, cy, v1, a + v2); double t = u(0, 6.28); P2 q = pol(p.x, p.y, 1.5, t), e = pol(p.x, p.y, 1.75, t); SoSolids *ps = SL(); CL *cu = cl();
      ext(ps, U(glv(2, capl(p, q, 0.46), ell(e.x, e.y, 0.52, 0.36, t, 10))), 0.26, 0.0); cl_add(cu, ell(e.x, e.y, 0.32, 0.18, t, 8), 0.1); o_st(o, ps, NULL, NOP, cu, buf(Line(plv(2, p, e)), 0.5, 16)); }
    RET(o_out(o, lay, NULL));
}

/* trederra screw_pickets: screw pickets thrown down in a bundle: the eye, the loops, the corkscrew at the foot */
BLD(b_trederra_screw_pickets) {
    SoObj *o = OBJ("iron"); int n = ri(2, 3);
    for (int i = 0; i < n; i++) {
        double v1 = u(-0.8, 0.8); double v2 = u(1.5, 1.9); double v3 = u(-0.3, 0.3); SoFrame f2 = frame(cx + v1, cy + (i - (n - 1) / 2.0) * v2, a + v3); double L = u(6.0, 7.5); int sv = rchi(2, -1, 1); double x1 = L / 2 - 1.9;
        GL *gs = gl(); P2 e = F(&f2, -L / 2 - 0.4, 0); double ls[24]; PT *cs = pl(); SoSolids *ps = SL(); P2 m = F(&f2, 0, 0);
        gl_add(gs, barl(F(&f2, -L / 2, 0), F(&f2, x1, 0), 0.52)); gl_add(gs, ann(e.x, e.y, 0.62, 0.26, 10));
        if (rnd() < 0.5) { double xs[2] = { -L * 0.22, L * 0.08 }; for (int j = 0; j < 2; j++) { P2 q = F(&f2, xs[j], sv * 0.6); gl_add(gs, ann(q.x, q.y, 0.6, 0.26, 10)); } }
        else { double xs[3] = { -L * 0.28, -L * 0.06, L * 0.14 }; for (int j = 0; j < 3; j++) { P2 q = F(&f2, xs[j], sv * 0.6); gl_add(gs, ann(q.x, q.y, 0.6, 0.26, 10)); } }
        linspace(0, 1.9, 24, ls);
        for (int j = 0; j < 24; j++) pl_add(cs, F(&f2, x1 + ls[j], 0.5 * SIN(ls[j] * 6.2832 / 0.95)));
        pl_add(cs, F(&f2, L / 2 + 0.3, 0)); gl_add(gs, line(cs, 0.5));
        { GG *g = U(gs); ext(ps, g, u(0.45, 0.55), 0.0); } o_st(o, ps, NULL, NOP, NULL, rect(m.x, m.y, L + 1.6, 2.0, a));
    }
    RET(o_out(o, lay, NULL));
}

/* trederra sniper_plate: a loophole plate laid flat, rivets, the keyhole and its shutter, the carrying slot, splashes, prop legs */
BLD(b_trederra_sniper_plate) {
    SoFrame ff = frame(cx, cy, a); double L = u(3.8, 4.6); double W = u(2.8, 3.4); SoObj *o = OBJ("iron"); GG *g = rrect(cx, cy, L, W, a, 0.25); int64_t b; PT *st = pl(); double l4[4], l5[5]; P2 c, pv; int nh;
    for (int sx = -1; sx <= 1; sx += 2) { P2 p = F(&ff, sx * L * 0.3, W / 2 + 0.35); double v1 = u(0.3, 0.45); double v2 = u(1.6, 2.2); P2 e = F(&ff, sx * L * v1, W / 2 + v2); SoSolids *ps = SL(); ext(ps, capl(p, e, 0.5), 0.36, 0.0); o_st(o, ps, NULL, NOP, NULL, buf(Line(plv(2, p, e)), 0.4, 16)); }
    b = o_up(o, g, 0.55, NULL);
    linspace(-W / 2 + 0.4, W / 2 - 0.4, 4, l4); linspace(-L / 2 + 0.4, L / 2 - 0.4, 5, l5);
    for (int sx = -1; sx <= 1; sx += 2) for (int j = 0; j < 4; j++) pl_add(st, F(&ff, sx * (L / 2 - 0.4), l4[j]));
    for (int sy = -1; sy <= 1; sy += 2) for (int j = 1; j < 4; j++) pl_add(st, F(&ff, l5[j], sy * (W / 2 - 0.4)));
    o_studs(o, st, b, 0.26, 0.14, NULL, 8);
    c = F(&ff, -L * 0.08, -W * 0.05); o_cut(o, U(glv(2, rect(c.x, c.y, 1.5, 0.42, a), disc(c.x, c.y, 0.42, 10))), b, 0.5);
    pv = F(&ff, -L * 0.08 + 1.2, -W * 0.05 + 0.7);
    { double t = a + u(0.3, 1.2); P2 q = pol(pv.x, pv.y, 0.55, t + PI_); o_on(o, rrect(q.x, q.y, 1.7, 0.75, t, 0.2), b, 0.17, NULL); o_on(o, disc(pv.x, pv.y, 0.3, 8), b, 0.3, NULL); }
    o_cut(o, capl(F(&ff, -0.5, -W / 2 + 0.95), F(&ff, 0.5, -W / 2 + 0.95), 0.34), b, 0.5);
    nh = ri(0, 3);
    { P2 hs[3]; GL *hg = gl();
      for (int i = 0; i < nh; i++) { double v1 = u(-L * 0.4, -L * 0.2); double v2 = u(0.2, W * 0.3); hs[i] = F(&ff, v1, v2); }
      if (nh) { for (int i = 0; i < nh; i++) { double rr = u(0.24, 0.32); gl_add(hg, disc(hs[i].x, hs[i].y, rr, 7)); } o_cut(o, U(hg), b, 0.22); } }
    RET(o_out(o, lay, NULL));
}

/* trederra dud_shell: a heavy shell where it ploughed in, the driving band, the nose and fuze, the furrow and thrown-up earth */
typedef struct { double L, r; } DsC;
static int ds_shell(void *c, double t, double *out) {
    DsC *k = (DsC *)c; double x = (t + k->L / 2) / k->L, r = k->r;
    if (x > 0.93) *out = r * 0.3;
    else if (x > 0.5) *out = r * MX(0.3, fsq(MX(0.0, 1 - fpw((x - 0.5) / 0.5, 2))));
    else *out = r * (x < 0.05 ? 0.9 : 0.09 < x && x < 0.19 ? 1.1 : 1.0);
    return E_ ? SO_ERR : SO_OK;
}
BLD(b_trederra_dud_shell) {
    SoFrame ff = frame(cx, cy, a); double L = u(4.4, 5.6); double r = u(0.85, 1.05); SoObj *o = OBJ("iron"); DsC dc = { L, r }; GG *fur; SoSolids *ps;
    { double v1 = u(1.2, 2.2); double v2 = u(-0.3, 0.3); P2 q1 = F(&ff, -L / 2 - v1, v2), q2 = F(&ff, -L * 0.2, 0), q3 = F(&ff, -L / 2, 0), q4 = F(&ff, L / 2, 0);
      fur = dif(hull(glv(2, disc(q1.x, q1.y, r * 0.7, 20), disc(q2.x, q2.y, r + 0.5, 20))), hull(glv(2, disc(q3.x, q3.y, r + 0.25, 20), disc(q4.x, q4.y, r + 0.25, 20)))); }
    o_gcut(o, fur, u(0.3, 0.4), "pit");
    ps = SL(); spin(ps, cx, cy, a, L, ds_shell, &dc, 1.0, 0.0, "round"); o_st(o, ps, NULL, NOP, NULL, NULL);
    for (int sv = -1; sv <= 1; sv += 2) {
        if (rnd() < 0.75) { double v1 = u(-L * 0.1, L * 0.3); double v2 = u(0.8, 1.1); P2 c = F(&ff, v1, sv * (r + v2)); double dr = u(0.7, 1.0); double dh = u(0.35, 0.5); double sq = u(0.5, 0.7); double da = a + u(-0.3, 0.3);
            ps = SL(); dome(ps, c.x, c.y, dr, dh, 0.0, sq, da, 20); o_st(o, ps, "earth", NOP, NULL, disc(c.x, c.y, 1.0, 20)); }
    }
    RET(o_out(o, lay, NULL));
}

/* trederra binoculars_map: a trench map spread out with its folds and trench lines, field glasses on it, a pencil beside */
static int bm_barrel(void *c, double s, double *out) { (void)c; *out = s < 0.12 ? 0.45 : s < 0.42 ? 0.36 : 0.36 + 0.18 * MN(1.0, (s - 0.42) / 0.12); return SO_OK; }
BLD(b_trederra_binoculars_map) {
    SoFrame ff = frame(cx, cy, a); double L = u(4.2, 5.2); double W = u(3.2, 3.8); SoObj *o = OBJ("leather"); GG *g = rect(cx, cy, L, W, a), *keep; int64_t m; P2 c; double t; SoFrame f2; GL *fl = gl(), *zz = gl(); SoSolids *ps = SL();
    m = o_up(o, g, 0.26, "paper"); { double v1 = u(-0.7, 0.7); double v2 = u(-0.3, 0.3); c = F(&ff, v1, v2); } t = a + u(0.3, 1.3); f2 = frame(c.x, c.y, t); keep = rect(c.x, c.y, 3.0, 2.6, t);
    gl_add(fl, barl(F(&ff, -L / 6, -W), F(&ff, -L / 6, W), 0.2)); gl_add(fl, barl(F(&ff, L / 6, -W), F(&ff, L / 6, W), 0.2)); gl_add(fl, barl(F(&ff, -L, 0), F(&ff, L, 0), 0.2));
    o_cut(o, dif(U(fl), keep), m, 0.15);
    { double vs[2] = { -W * 0.3, W * 0.28 }, xs[64]; int nx = arange(-L / 2 + 0.3, L / 2 - 0.2, 0.5, xs, 64);
      for (int j = 0; j < 2; j++) { PT *q = pl(); for (int i = 0; i < nx; i++) pl_add(q, F(&ff, xs[i], vs[j] + 0.3 * ((int)((xs[i] + L) / 0.5) % 2 ? 1 : -1))); gl_add(zz, line(q, 0.2)); } }
    o_cut(o, dif(U(zz), keep), m, 0.15);
    for (int sv = -1; sv <= 1; sv += 2) rbar(ps, F(&f2, -1.2, sv * 0.56), F(&f2, 1.0, sv * 0.64), bm_barrel, NULL, 0, 0.5, 0, 0, 0);
    { P2 q = F(&f2, -0.1, 0); ext(ps, rect(q.x, q.y, 0.6, 0.9, t), 0.42, 0.0); cyl(ps, q.x, q.y, 0.3, 0.62, 0.0, 8); }
    o_st(o, ps, NULL, m, NULL, NULL);
    { double v1 = u(-L * 0.3, L * 0.3); double v2 = u(0.6, 1.0); P2 p = F(&ff, v1, -W / 2 - v2); double tp = a + u(-0.4, 0.4); P2 q = pol(p.x, p.y, 2.0, tp);
      ps = SL(); ext(ps, U(glv(2, barl(p, q, 0.5), tri(q, pol(q.x, q.y, 0.6, tp), 0.5))), 0.34, 0.0); o_st(o, ps, "wood", NOP, NULL, buf(Line(plv(2, p, q)), 0.5, 16)); }
    RET(o_out(o, lay, NULL));
}

/* trederra poppies: blooms of four cupped petals about the seed head, over rosettes of jagged leaves; a bud or two */
BLD(b_trederra_poppies) {
    SoObj *o = OBJ("flower"); GL *foot = gl(); int n = ri(3, 6);
    for (int i = 0; i < n; i++) {
        double v1 = i == 0 ? 0 : u(1.5, 2.8); double v2 = u(-0.3, 0.3); P2 p = pol(cx, cy, v1, a + i * 1.3 + v2); double r = u(0.8, 1.05); double t = u(0, 1.57); double h = u(0.6, 0.9); SoSolids *ps; GL *pt = gl(); CL *cu;
        gl_add(foot, disc(p.x, p.y, r + 0.3, 20));
        { int k = ri(5, 6); double sa = u(0, 1); ps = SL(); ext(ps, star(p.x, p.y, r * 1.45, r * 0.55, k, sa), 0.26, 0.0); o_st(o, ps, "grass", NOP, NULL, disc(p.x, p.y, r, 20)); }
        if (rnd() < 0.2) { ps = SL(); dome(ps, p.x, p.y, 0.42, h, 0.0, 0.75, t, 20); o_st(o, ps, NULL, NOP, NULL, disc(p.x, p.y, 0.4, 20)); continue; }
        for (int k = 0; k < 4; k++) { P2 q = pol(p.x, p.y, r * 0.5, t + k * 1.5708); gl_add(pt, disc(q.x, q.y, r * 0.54, 12)); }
        cu = cl(); cl_add(cu, ann(p.x, p.y, r * 0.5, 0.3, 10), h - 0.28);
        for (int k = 0; k < 4; k++) cl_add(cu, barl(pol(p.x, p.y, r * 0.45, t + 0.785 + k * 1.5708), pol(p.x, p.y, r * 1.2, t + 0.785 + k * 1.5708), 0.22), h - 0.3);
        ps = SL(); ext(ps, U(pt), h, 0.0); o_st(o, ps, NULL, NOP, cu, disc(p.x, p.y, r, 20));
    }
    { GG *f = hull(foot); o_layers(o, lay); RET(f); }
}

/* trederra rats: a fat pear of a body, the pointed head and ears, the feet tucked in, the long bare tail curling away */
typedef struct { double w, L; } RtC;
static int rt_body(void *c, double t, double *out) { RtC *k = (RtC *)c; *out = k->w * fsq(MX(0.02, 1 - fpw(2 * t / k->L, 2))) * (1.0 - 0.5 * t / k->L); return E_ ? SO_ERR : SO_OK; }
static int rt_head(void *c, double q, double *out) { (void)c; *out = 0.5 - 0.24 * q; return SO_OK; }
BLD(b_trederra_rats) {
    SoObj *o = OBJ("hide"); GL *foot = gl(); int n = ri(1, 3);
    for (int i = 0; i < n; i++) {
        double v = i == 0 ? 0 : u(2.6, 3.6); P2 p = pol(cx, cy, v, a + i * 2.2); double b = u(0, 6.28); SoFrame f2 = frame(p.x, p.y, b); double L = u(2.6, 3.2); double w = L * 0.24; RtC rc = { w, L }; SoSolids *ps = SL(); int sv; PT *tl; GG *tf;
        spin(ps, p.x, p.y, b, L, rt_body, &rc, 1.05, 0.0, "round");
        rodf(ps, plv(2, F(&f2, L * 0.32, 0), F(&f2, L / 2 + 0.75, 0)), rt_head, NULL, 1.2, 0.0, 0.0);
        for (int s2 = -1; s2 <= 1; s2 += 2) { P2 q = F(&f2, L * 0.36, s2 * 0.42); cyl(ps, q.x, q.y, 0.27, 0.78, 0.0, 8); }
        sv = rchi(2, -1, 1);
        { double w1 = u(0.1, 0.5); double w2 = u(0.5, 1.1); double w3 = u(1.2, 1.9); tl = plv(4, F(&f2, -L / 2 + 0.2, 0), F(&f2, -L / 2 - 0.8, sv * w1), F(&f2, -L / 2 - 1.6, sv * w2), F(&f2, -L / 2 - 2.1, sv * w3)); }
        o_st(o, ps, NULL, NOP, NULL, disc(p.x, p.y, L / 2 + 0.8, 20)); ps = SL(); rod(ps, tl, 0.26, 0.9, 0.0, 0.0); tf = buf(Line(tl), 0.4, 16); o_st(o, ps, NULL, NOP, NULL, tf);
        gl_add(foot, disc(p.x, p.y, L / 2 + 0.9, 20)); gl_add(foot, buf(Line(tl), 0.4, 16));
    }
    { GG *f = hull(foot); o_layers(o, lay); RET(f); }
}

/* trederra stick_grenades: the can of a head, the wooden handle, the cap; thrown down together or in their open crate */
static int sg_gren(void *c, double s, double *out) { (void)c; *out = s < 0.09 ? 0.38 : s < 0.62 ? 0.29 : 0.7 < s && s < 0.78 ? 0.6 : 0.54; return SO_OK; }
static void sg_one(SoObj *o, double Lg, double x, double y, double b, int64_t par) {
    SoFrame f2 = frame(x, y, b); int sv = rchi(2, -1, 1); SoSolids *ps = SL();
    rbar(ps, F(&f2, -sv * Lg / 2, 0), F(&f2, sv * Lg / 2, 0), sg_gren, NULL, 0, 0.5, 0, 0, 0); o_st(o, ps, NULL, par, NULL, rect(x, y, Lg, 1.2, b));
}
BLD(b_trederra_stick_grenades) {
    SoObj *o = OBJ("wood"); SoFrame ff = frame(cx, cy, a); double Lg = u(3.2, 3.8);
    if (rnd() < 0.5) {
        int n = ri(3, 4); double W = n * 1.2 + 1.3, L = Lg + 1.7; GG *g = rect(cx, cy, L, W, a); int64_t b = o_up(o, g, 0.3, NULL);
        o_on(o, dif(g, rect(cx, cy, L - 1.2, W - 1.2, a)), b, 0.75, NULL); o_seams(o, &ff, L - 1.3, W - 1.3, b, 1.1, 0, 0.2, 0.15, NULL);
        for (int i = 0; i < n; i++) if (rnd() < 0.85) { double v = u(-0.1, 0.1); P2 c = F(&ff, v, (i - (n - 1) / 2.0) * 1.2); double t = a + u(-0.05, 0.05); sg_one(o, Lg, c.x, c.y, t, b); }
        for (int sx = -1; sx <= 1; sx += 2) { P2 c = F(&ff, sx * (L / 2 + 0.25), 0); PT *pts = arc(c.x, c.y, 0.7, sx > 0 ? a - 1.5708 : a + 1.5708, sx > 0 ? a + 1.5708 : a + 4.712, 8); SoSolids *ps = SL(); rod(ps, pts, 0.27, 1.0, 0.0, 0.0); o_st(o, ps, "rope", NOP, NULL, disc(c.x, c.y, 1.0, 20)); }
        if (rnd() < 0.6) { double v1 = u(-1, 1); double v2 = u(0.9, 1.4); P2 c = F(&ff, v1, -W / 2 - v2); double t = a + u(-0.5, 0.5); sg_one(o, Lg, c.x, c.y, t, NOP); }
    } else {
        int n = ri(2, 4);
        for (int i = 0; i < n; i++) { double v = i == 0 ? 0 : u(1.3, 2.0); P2 p = pol(cx, cy, v, a + i * 1.6); double t = a + u(-0.9, 0.9); sg_one(o, Lg, p.x, p.y, t, NOP); }
    }
    RET(o_out(o, lay, NULL));
}

/* trederra bugle: the tube doubled once into a long loop, the flaring bell, the mouthpiece, its cord with a tassel */
static int bg_bell(void *c, double s, double *out) { (void)c; *out = 0.3 + 0.95 * fpw(s, 2.2); return SO_OK; }
BLD(b_trederra_bugle) {
    SoFrame ff = frame(cx, cy, a); double L = u(4.6, 5.4); SoObj *o = OBJ("brass"); int sv = rchi(2, -1, 1); GG *lp; SoSolids *ps;
    { P2 q = F(&ff, -L * 0.12, sv * 0.52); lp = ell(q.x, q.y, L * 0.3, 0.62, a, 24); }
    { P2 d = F(&ff, -L / 2 - 0.5, 0), m = F(&ff, -L * 0.15, 0); ps = SL(); ext(ps, U(glv(3, buf(exter(lp), 0.27, 16), barl(F(&ff, -L / 2 - 0.3, 0), F(&ff, 0.1, 0), 0.6), disc(d.x, d.y, 0.42, 10))), 0.56, 0.0); o_st(o, ps, NULL, NOP, NULL, rect(m.x, m.y, L * 0.8, 2.4, a)); }
    { P2 m = F(&ff, L * 0.25, 0); ps = SL(); rbar(ps, F(&ff, 0, 0), F(&ff, L / 2, 0), bg_bell, NULL, 0, 0.3, 0, 0, 0); o_st(o, ps, NULL, NOP, NULL, rect(m.x, m.y, L / 2, 2.6, a)); }
    { P2 p = F(&ff, -L * 0.2, -sv * 0.4); double v1 = u(-1.0, 1.0); double v2 = u(1.8, 2.6); P2 e = F(&ff, -L * 0.2 + v1, -sv * v2); PT *pts = wiggle(p, e, 0.5, 3);
      ps = SL(); rod(ps, pts, 0.26, 0.9, 0.0, 0.0); dome(ps, e.x, e.y, 0.42, 0.5, 0.0, 1.0, 0.0, 20); o_st(o, ps, "cloth", NOP, NULL, buf(Line(pts), 0.5, 16)); }
    RET(o_out(o, lay, NULL));
}

/* trederra machine_gun: the ribbed water jacket, breech, grips, the tripod, and the belt in from an ammunition tin */
typedef struct { double rj, Lj; } MgC;
static int mg_barrel(void *c, double q, double *out) { MgC *k = (MgC *)c; *out = q < k->Lj / (k->Lj + 0.8) ? k->rj + 0.07 * (SIN(q * (k->Lj + 0.8) * 9.5) > 0) : q < 0.94 ? 0.36 : 0.46; return SO_OK; }
BLD(b_trederra_machine_gun) {
    SoFrame ff = frame(cx, cy, a); SoObj *o = OBJ("iron"); int sv = rchi(2, -1, 1); P2 feet[3]; SoSolids *ps = SL(); double z, Lj, rj = 0.62; MgC mc; GG *bg; int64_t bx, cv; P2 c; double t;
    { double v = u(1.7, 2.2); feet[0] = F(&ff, v, 1.7); } { double v = u(1.7, 2.2); feet[1] = F(&ff, v, -1.7); } { double v1 = u(2.8, 3.4); double v2 = u(-0.3, 0.3); feet[2] = F(&ff, -v1, v2); }
    for (int i = 0; i < 3; i++) { rbar(ps, feet[i], p2(cx, cy), NULL, NULL, 0.32, 0.4, 1, 0.05, 0.7); cyl(ps, feet[i].x, feet[i].y, 0.46, 0.26, 0.0, 8); }
    cyl(ps, cx, cy, 0.62, 1.0, 0.0, 12); o_st(o, ps, NULL, NOP, NULL, hull(glv(3, disc(feet[0].x, feet[0].y, 0.5, 20), disc(feet[1].x, feet[1].y, 0.5, 20), disc(feet[2].x, feet[2].y, 0.5, 20))));
    z = u(1.0, 1.3); Lj = u(3.0, 3.6); mc.rj = rj; mc.Lj = Lj;
    { P2 m = F(&ff, 0.6 + Lj / 2, 0); ps = SL(); rbar(ps, F(&ff, 0.2, 0), F(&ff, 1.0 + Lj, 0), mg_barrel, &mc, 0, 0.4, 1, z, z); o_st(o, ps, NULL, NOP, NULL, rect(m.x, m.y, Lj + 1.2, 1.4, a)); }
    { P2 q = F(&ff, -0.95, 0), r1 = F(&ff, -0.6, 0), r2 = F(&ff, -0.2, 0); bg = rect(q.x, q.y, 2.3, 1.1, a); bx = o_up(o, bg, z + rj, NULL); cv = o_lid(o, bg, bx, 0.14, 0.15, NULL); o_on(o, rect(r1.x, r1.y, 0.46, 0.6, a), cv, 0.3, NULL); o_on(o, rect(r2.x, r2.y, 0.5, 1.1, a), bx, 0.14, NULL); }
    { P2 g1 = F(&ff, -2.45, -0.36), g2 = F(&ff, -2.45, 0.36), r1 = F(&ff, -2.25, 0), m = F(&ff, -2.4, 0); ps = SL(); cyl(ps, g1.x, g1.y, 0.26, z + 0.45, 0.0, 8); cyl(ps, g2.x, g2.y, 0.26, z + 0.45, 0.0, 8); ext(ps, rect(r1.x, r1.y, 0.5, 0.5, a), z + 0.3, 0.0); o_st(o, ps, "wood", NOP, NULL, rect(m.x, m.y, 0.8, 1.5, a)); }
    { double v1 = u(-0.6, 0.4); double v2 = u(3.0, 3.6); c = F(&ff, v1, sv * v2); } t = a + u(-0.3, 0.3); { double h = u(0.8, 1.0); t_tin_box(o, c.x, c.y, t, 2.0, 1.3, h, 1); }
    { double v = u(-0.3, 0.3); t_belt(o, plv(3, F(&ff, -0.4, sv * 0.6), F(&ff, -0.4 + v, sv * 1.5), pol(c.x, c.y, 0.3, a - sv * 1.57)), "cloth"); }
    RET(o_out(o, lay, NULL));
}

/* trederra rifle: butt, wrist, fore-end with its bands, breech and bolt, magazine, muzzle, the bayonet on some, the sling */
static P2 rf_F(const SoFrame *ff, int sv, double x, double v) { return F(ff, x, sv * v); }
BLD(b_trederra_rifle) {
    SoFrame ff = frame(cx, cy, a); double L = u(8.0, 9.2); SoObj *o = OBJ("wood"); int sv = rchi(2, -1, 1); double x0 = -L / 2; GG *stock; PT *pts; SoSolids *ps; int64_t b;
#define FR(x, v) rf_F(&ff, sv, (x), (v))
    stock = buf(Poly(plv(9, FR(x0, -0.2), FR(x0, 0.95), FR(x0 + 1.9, 0.5), FR(x0 + 2.6, 0.38), FR(x0 + 2.9, 0.62), FR(x0 + L - 1.0, 0.56), FR(x0 + L - 1.0, 0.0), FR(x0 + 2.6, -0.06), FR(x0 + 1.8, -0.22))), 0.06, 4);
    { double v1 = u(1.3, 1.9); double v2 = u(1.2, 1.8); pts = plv(4, FR(x0 + 0.9, 0.7), FR(x0 + 2.2, v1), FR(x0 + 4.0, v2), FR(x0 + L * 0.62, 0.5)); }
    ps = SL(); ext(ps, line(pts, 0.5), 0.24, 0.0); o_st(o, ps, "leather", NOP, NULL, buf(Line(pts), 0.4, 16));
    b = o_up(o, stock, 0.56, NULL);
    { P2 q = FR(x0 + 3.5, 0.3), b1 = FR(x0 + L * 0.62, 0.28), b2 = FR(x0 + L - 1.5, 0.28); o_on(o, rect(q.x, q.y, 1.9, 0.5, a), b, 0.16, "iron"); o_on(o, U(glv(2, rect(b1.x, b1.y, 0.46, 0.7, a), rect(b2.x, b2.y, 0.46, 0.7, a))), b, 0.13, "iron"); }
    { P2 q1 = FR(x0 + 3.9, 0.85), q2 = FR(x0 + 3.0, 0.82), q3 = FR(x0 + 3.2, -0.3), m = FR(x0 + 3.5, 0.3); ps = SL(); ext(ps, rect(q1.x, q1.y, 0.75, 0.7, a), 0.4, 0.0); ext(ps, ann(q2.x, q2.y, 0.42, 0.18, 8), 0.3, 0.0); cyl(ps, q3.x, q3.y, 0.3, 0.5, 0.0, 8); o_st(o, ps, "iron", NOP, NULL, rect(m.x, m.y, 1.6, 2.0, a)); }
    { P2 m = FR(x0 + L - 0.6, 0.2); ps = SL(); rod(ps, plv(2, FR(x0 + L - 1.2, 0.2), FR(x0 + L - 0.1, 0.2)), 0.3, 1.1, 0.0, 0.0); o_st(o, ps, "iron", NOP, NULL, disc(m.x, m.y, 0.6, 20)); }
    if (rnd() < 0.6) { P2 p = FR(x0 + L - 0.3, 0.52); double v = u(1.6, 2.2); P2 q = FR(x0 + L + v, 0.52), r1 = FR(x0 + L - 0.55, 0.45); ps = SL(); ext(ps, U(glv(2, tri(p, q, 0.56), rect(r1.x, r1.y, 0.6, 0.6, a))), 0.3, 0.0); o_st(o, ps, "iron", NOP, NULL, buf(Line(plv(2, p, q)), 0.4, 16)); }
#undef FR
    RET(o_out(o, lay, NULL));
}

/* trederra rum_jar: a stoneware gallon jar with shoulder, neck, lip and loop handle (standing or lying), tin mugs by it */
typedef struct { double L, r; } RjC;
static int rj_jar(void *c, double t, double *out) {
    RjC *k = (RjC *)c; double x = (t + k->L / 2) / k->L, r = k->r;
    *out = x < 0.55 ? r * (0.9 + 0.1 * MN(1.0, x / 0.06)) : x < 0.82 ? r * MX(0.42, COS((x - 0.55) / 0.27 * 1.5708)) : (x < 0.92 ? 0.44 : 0.58); return SO_OK;
}
BLD(b_trederra_rum_jar) {
    SoObj *o = OBJ("clay"); double r = u(1.1, 1.3); SoFrame ff = frame(cx, cy, a); int n; SoSolids *ps;
    if (rnd() < 0.6) {
        double h = u(1.7, 2.1); P2 hp = pol(cx, cy, 0.75, a + 2.0); CL *cu = cl(); (void)hp;
        ps = SL(); turn(ps, cx, cy, pl_cat(pl_cat(plv(3, p2(r * 0.94, 0.0), p2(r, 0.2), p2(r, h * 0.6)), crv(r, 0.48, h * 0.6, h * 0.85, 1, 6)), plv(3, p2(0.48, h * 0.95), p2(0.6, h * 0.96), p2(0.6, h + 0.12))), 24, 1.0, 0.0, 0.0);
        ext(ps, capl(pol(cx, cy, 0.4, a + 2.0), pol(cx, cy, r * 0.9, a + 2.0), 0.56), h * 0.9, 0.0);
        cl_add(cu, disc(cx, cy, 0.3, 8), h - 0.25); o_st(o, ps, NULL, NOP, cu, disc(cx, cy, r, 20));
    } else {
        double L = u(3.0, 3.6); RjC rc = { L, r }; P2 hp = F(&ff, L * 0.3, r * 0.5);
        ps = SL(); spin(ps, cx, cy, a, L, rj_jar, &rc, 1.0, 0.0, "round"); ext(ps, disc(hp.x, hp.y, 0.6, 10), 0.6, 0.0); o_st(o, ps, NULL, NOP, NULL, rect(cx, cy, L, 2 * r, a));
    }
    n = ri(1, 3);
    for (int i = 0; i < n; i++) {
        double v1 = u(1.3, 2.0); double v2 = u(0.9, 1.2); P2 c = pol(cx, cy, r + v1, a + 3.4 + i * v2); double t = u(0, 6.28); P2 q = pol(c.x, c.y, 0.75, t); GG *g = U(glv(2, disc(c.x, c.y, 0.64, 14), rect(q.x, q.y, 0.7, 0.5, t))); int64_t k;
        k = o_up(o, g, u(0.75, 0.95), "tin"); o_cut(o, disc(c.x, c.y, 0.4, 10), k, 0.5);
    }
    RET(o_out(o, lay, NULL));
}

/* @@NEXT@@ */

/* ====================================================================================================== registration */
#define R(st, cls, kind, name, r, ground, fn) bsdde_reg(st, cls, kind, name, r, ground, fn, NULL)
void bld_register_sd_r(void) {
/* @@REG@@ */
    /* sd_r.py */
    R("corridor", "made", "metal", "toolbox", 10.5, 0, b_corridor_toolbox);
    R("corridor", "made", "metal", "terminal", 10.5, 0, bsdde_replaced);          /* drawn again in sd_r_x_corridor.py */
    R("corridor", "made", "plate", "deck_hatch", 10.5, 0, bsdde_replaced);          /* drawn again in sd_r_x_corridor.py */
    R("corridor", "made", "metal", "cell_rack", 10.5, 0, b_corridor_cell_rack);
    R("corridor", "made", "metal", "junction_box", 10.5, 0, b_corridor_junction_box);
    R("corridor", "made", "metal", "vent_fan", 10.5, 0, b_corridor_vent_fan);
    R("corridor", "made", "metal", "hover_pallet", 10.5, 0, bsdde_replaced);          /* drawn again in sd_r_x_corridor.py */
    R("corridor", "made", "plate", "plate_stack", 10.5, 0, bsdde_replaced);          /* drawn again in sd_r_x_corridor.py */
    R("corridor", "made", "tin", "mess_tray", 10.5, 0, b_corridor_mess_tray);
    R("corridor", "made", "metal", "magboots", 10.5, 0, b_corridor_magboots);
    R("corridor", "made", "cable", "hose_reel", 10.5, 0, b_corridor_hose_reel);
    /* sd_r_badlands.py */
    R("badlands", "growth", "cactus", "saguaro", 10.5, 0, bsdde_replaced);          /* drawn again in sd_r_x_badlands.py */
    R("badlands", "growth", "cactus", "prickly_pear", 10.5, 0, bsdde_replaced);          /* drawn again in sd_r_x_badlands.py */
    R("badlands", "made", "bone", "longhorn_skull", 10.5, 0, bsdde_replaced);          /* drawn again in sd_r_badlands2.py */
    R("badlands", "made", "wood", "hitching_rail", 10.5, 0, bsdde_replaced);          /* drawn again in sd_r_x_badlands.py */
    R("badlands", "made", "wood", "water_trough", 10.5, 0, bsdde_replaced);          /* drawn again in sd_r_x_badlands.py */
    R("badlands", "made", "wood", "ox_yoke", 10.5, 0, bsdde_replaced);          /* drawn again in sd_r_x_badlands.py */
    R("badlands", "made", "hide", "rattlesnake", 10.5, 0, bsdde_replaced);          /* drawn again in sd_r_badlands2.py */
    R("badlands", "made", "leather", "saddle", 10.5, 0, bsdde_replaced);          /* drawn again in sd_r_x_badlands.py */
    R("badlands", "made", "tin", "gold_pan", 10.5, 0, bsdde_replaced);          /* drawn again in sd_r_badlands2.py */
    /* sd_r_badlands2.py */
    R("badlands", "growth", "cactus", "prickly_pear", 10.5, 0, bsdde_replaced);          /* drawn again in sd_r_x_badlands.py */
    R("badlands", "made", "bone", "longhorn_skull", 10.5, 0, b_badlands_longhorn_skull);
    R("badlands", "made", "wood", "ox_yoke", 10.5, 0, bsdde_replaced);          /* drawn again in sd_r_x_badlands.py */
    R("badlands", "made", "hide", "rattlesnake", 10.5, 0, b_badlands_rattlesnake);
    R("badlands", "made", "leather", "saddle", 10.5, 0, bsdde_replaced);          /* drawn again in sd_r_x_badlands.py */
    R("badlands", "made", "tin", "gold_pan", 10.5, 0, b_badlands_gold_pan);
    /* sd_r_cynder.py */
    R("cynder", "marks", "lava", "skylight", 10.5, 1, b_cynder_skylight);
    R("cynder", "debris", "basalt", "basalt_columns", 10.5, 0, b_cynder_basalt_columns);
    R("cynder", "debris", "basalt", "ropy_lava", 10.5, 0, b_cynder_ropy_lava);
    R("cynder", "debris", "ember", "fumarole", 10.5, 0, b_cynder_fumarole);
    R("cynder", "debris", "basalt", "lava_bomb", 10.5, 1, b_cynder_lava_bomb);
    R("cynder", "made", "iron", "anvil", 10.5, 0, b_cynder_anvil);
    R("cynder", "made", "iron", "crucible", 10.5, 0, b_cynder_crucible);
    R("cynder", "made", "iron", "ingots", 10.5, 0, b_cynder_ingots);
    R("cynder", "debris", "obsidian", "obsidian_blades", 10.5, 0, b_cynder_obsidian_blades);
    R("cynder", "made", "ash", "charred_hand", 10.5, 0, b_cynder_charred_hand);
    R("cynder", "growth", "tooth", "horn_spur", 10.5, 0, b_cynder_horn_spur);
    R("cynder", "debris", "ash", "slag_heap", 10.5, 0, b_cynder_slag_heap);
    R("cynder", "made", "leather", "forge_bellows", 10.5, 0, b_cynder_forge_bellows);
    R("cynder", "marks", "lava", "ember_moat", 10.5, 1, b_cynder_ember_moat);
    R("cynder", "debris", "ember", "sulfur_crust", 10.5, 0, b_cynder_sulfur_crust);
    R("cynder", "made", "iron", "shackles", 10.5, 0, b_cynder_shackles);
    R("cynder", "made", "basalt", "imp_idol", 10.5, 0, b_cynder_imp_idol);
    R("cynder", "made", "basalt", "quench_trough", 10.5, 0, bsdde_replaced);          /* drawn again in sd_r_x_frontier.py */
    R("cynder", "debris", "basalt", "cinder_cone", 10.5, 0, b_cynder_cinder_cone);
    /* sd_r_infestation.py */
    R("infestation", "growth", "web", "cocoon", 10.5, 0, bsdde_replaced);          /* drawn again in sd_r_x_frontier.py */
    R("infestation", "growth", "egg", "burst_egg", 10.5, 0, bsdde_replaced);          /* drawn again in sd_r_x_frontier.py */
    R("infestation", "made", "carapace", "dead_crawler", 10.5, 0, b_infestation_dead_crawler);
    R("infestation", "growth", "resin", "resin_stump", 10.5, 0, b_infestation_resin_stump);
    R("infestation", "growth", "spore", "spore_chimneys", 10.5, 0, b_infestation_spore_chimneys);
    R("infestation", "marks", "acid", "acid_holes", 10.5, 0, b_infestation_acid_holes);
    R("infestation", "growth", "flesh", "hive_vent", 10.5, 1, b_infestation_hive_vent);
    R("infestation", "made", "carapace", "molt", 10.5, 0, bsdde_replaced);          /* drawn again in sd_r_x_frontier.py */
    R("infestation", "growth", "tendril", "tendril_knot", 10.5, 0, b_infestation_tendril_knot);
    R("infestation", "growth", "pustule", "sac_cluster", 10.5, 0, b_infestation_sac_cluster);
    R("infestation", "made", "metal", "flamethrower", 10.5, 0, b_infestation_flamethrower);
    R("infestation", "made", "metal", "scanner_and_mags", 10.5, 0, b_infestation_scanner_and_mags);
    R("infestation", "debris", "spine", "spine_row", 10.5, 0, b_infestation_spine_row);
    R("infestation", "growth", "flesh", "larvae", 10.5, 0, b_infestation_larvae);
    R("infestation", "growth", "web", "web_anchor", 10.5, 0, b_infestation_web_anchor);
    R("infestation", "made", "carapace", "mandibles", 10.5, 0, b_infestation_mandibles);
    R("infestation", "growth", "egg", "egg_ledge", 10.5, 0, bsdde_replaced);          /* drawn again in sd_r_x_frontier.py */
    R("infestation", "debris", "spine", "victim_bones", 10.5, 0, b_infestation_victim_bones);
    /* sd_r_maahl.py */
    R("maahl", "made", "ruin", "buried_head", 10.5, 0, bsdde_replaced);          /* drawn again in sd_r_x_maahl.py */
    R("maahl", "made", "obelisk", "obelisk_tip", 10.5, 0, bsdde_replaced);          /* drawn again in sd_r_x_maahl.py */
    R("maahl", "made", "ruin", "sundial", 10.5, 0, bsdde_replaced);          /* drawn again in sd_r_x_maahl.py */
    R("maahl", "made", "clay", "amphorae", 10.5, 0, bsdde_replaced);          /* drawn again in sd_r_x_maahl.py */
    R("maahl", "made", "chitin", "scarab", 10.5, 0, b_maahl_scarab);
    R("maahl", "made", "ruin", "arch_fragment", 10.5, 0, bsdde_replaced);          /* drawn again in sd_r_x_maahl.py */
    R("maahl", "debris", "crystal", "geode", 10.5, 0, b_maahl_geode);
    R("maahl", "made", "cloth", "saddlebags", 10.5, 0, b_maahl_saddlebags);
    R("maahl", "made", "brass", "oil_lamp", 10.5, 0, b_maahl_oil_lamp);
    R("maahl", "made", "pavers", "mosaic", 10.5, 0, bsdde_replaced);          /* drawn again in sd_r_x_maahl.py */
    R("maahl", "made", "ruin", "sphinx_paw", 10.5, 0, bsdde_replaced);          /* drawn again in sd_r_x_maahl.py */
    R("maahl", "debris", "salt", "salt_plates", 10.5, 0, b_maahl_salt_plates);
    R("maahl", "made", "ruin", "stele", 10.5, 0, bsdde_replaced);          /* drawn again in sd_r_x_maahl.py */
    R("maahl", "made", "wood", "well_bucket", 10.5, 0, b_maahl_well_bucket);
    R("maahl", "debris", "sandstone", "desert_rose", 10.5, 0, b_maahl_desert_rose);
    R("maahl", "made", "column", "fallen_capital", 10.5, 0, bsdde_replaced);          /* drawn again in sd_r_x_maahl.py */
    R("maahl", "debris", "glass", "fulgurite", 10.5, 0, b_maahl_fulgurite);
    R("maahl", "debris", "chitin", "worm_maw", 10.5, 1, bsdde_replaced);          /* drawn again in sd_r_x_maahl.py */
    R("maahl", "growth", "chitin", "spore_pods", 10.5, 0, b_maahl_spore_pods);
    R("maahl", "made", "cloth", "water_skins", 10.5, 0, b_maahl_water_skins);
    /* sd_r_models.py */
    bsdde_models_register();          /* sd_r_models.py and sd_r_models_a..d.py */
    /* sd_r_targa.py */
    R("targa", "debris", "ice", "fallen_icicles", 10.5, 0, b_targa_fallen_icicles);
    R("targa", "made", "cloth", "pack_and_snowshoes", 10.5, 0, bsdde_replaced);          /* drawn again in sd_r_x_targa.py */
    R("targa", "made", "alien", "pylon_stub", 10.5, 0, b_targa_pylon_stub);
    R("targa", "made", "ice", "ice_blocks", 10.5, 0, bsdde_replaced);          /* drawn again in sd_r_x_targa.py */
    R("targa", "made", "alien", "pod_lid", 10.5, 0, bsdde_replaced);          /* drawn again in sd_r_x_targa.py */
    R("targa", "made", "alien", "conduit_node", 10.5, 1, bsdde_replaced);          /* drawn again in sd_r_x_targa.py */
    R("targa", "debris", "ice", "pressure_ridge", 10.5, 0, b_targa_pressure_ridge);
    R("targa", "made", "alien", "glyph_tile", 10.5, 0, b_targa_glyph_tile);
    R("targa", "made", "ice", "fishing_hole", 10.5, 1, b_targa_fishing_hole);
    R("targa", "made", "alien", "thermal_grille", 10.5, 1, b_targa_thermal_grille);
    R("targa", "made", "wood", "sled", 10.5, 0, bsdde_replaced);          /* drawn again in sd_r_x_targa.py */
    R("targa", "made", "alien", "buried_steps", 10.5, 0, bsdde_replaced);          /* drawn again in sd_r_x_targa.py */
    R("targa", "debris", "ice", "ice_spikes", 10.5, 0, b_targa_ice_spikes);
    R("targa", "made", "alien", "power_cell", 10.5, 0, b_targa_power_cell);
    R("targa", "made", "wood", "skis", 10.5, 0, bsdde_replaced);          /* drawn again in sd_r_x_targa.py */
    R("targa", "made", "metal", "supply_drop", 10.5, 0, bsdde_replaced);          /* drawn again in sd_r_x_targa.py */
    /* sd_r_trederra.py */
    R("trederra", "made", "wire", "wire_coil", 10.5, 0, b_trederra_wire_coil);
    R("trederra", "made", "tin", "tin_hat", 10.5, 0, b_trederra_tin_hat);
    R("trederra", "made", "cloth", "gas_mask", 10.5, 0, b_trederra_gas_mask);
    R("trederra", "made", "brass", "shell_cases", 10.5, 0, b_trederra_shell_cases);
    R("trederra", "made", "tin", "ammo_box", 10.5, 0, b_trederra_ammo_box);
    R("trederra", "made", "tin", "periscope", 10.5, 0, b_trederra_periscope);
    R("trederra", "made", "wood", "field_telephone", 10.5, 0, b_trederra_field_telephone);
    R("trederra", "made", "iron", "mortar", 10.5, 0, bsdde_replaced);          /* drawn again in sd_r_x_frontier.py */
    R("trederra", "made", "cloth", "stretcher", 10.5, 0, bsdde_replaced);          /* drawn again in sd_r_x_frontier.py */
    R("trederra", "made", "tin", "ration_tins", 10.5, 0, b_trederra_ration_tins);
    R("trederra", "made", "iron", "screw_pickets", 10.5, 0, b_trederra_screw_pickets);
    R("trederra", "made", "iron", "sniper_plate", 10.5, 0, b_trederra_sniper_plate);
    R("trederra", "made", "iron", "dud_shell", 10.5, 1, b_trederra_dud_shell);
    R("trederra", "made", "leather", "binoculars_map", 10.5, 0, b_trederra_binoculars_map);
    R("trederra", "growth", "flower", "poppies", 10.5, 0, b_trederra_poppies);
    R("trederra", "made", "hide", "rats", 10.5, 0, b_trederra_rats);
    R("trederra", "made", "wood", "stick_grenades", 10.5, 0, b_trederra_stick_grenades);
    R("trederra", "made", "brass", "bugle", 10.5, 0, b_trederra_bugle);
    R("trederra", "made", "iron", "machine_gun", 10.5, 0, b_trederra_machine_gun);
    R("trederra", "made", "sack", "sandbags", 10.5, 0, bsdde_replaced);          /* drawn again in sd_r_x_frontier.py */
    R("trederra", "made", "wood", "duckboard", 10.5, 0, bsdde_replaced);          /* drawn again in sd_r_x_frontier.py */
    R("trederra", "made", "wood", "rifle", 10.5, 0, b_trederra_rifle);
    R("trederra", "made", "clay", "rum_jar", 10.5, 0, b_trederra_rum_jar);
    R("trederra", "made", "wood", "trench_sign", 10.5, 0, bsdde_replaced);          /* drawn again in sd_r_x_frontier.py */
}
