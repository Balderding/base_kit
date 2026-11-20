/* bld_sd_rx_trederra.c: sd_r_x_trederra.py in C (see bld_sd_rx.h). */
#include "bld_sd_rx.h"

static double sgn(int i) { return i ? 1 : -1; }

/* stretcher */
typedef struct { GEOSGeometry *cv, *band; double zp; } Canvas;
static void canvas_free(void *c) { Canvas *v = (Canvas *)c; shp_free(v->cv); shp_free(v->band); free(v); }
static int canvas_sect(void *c, double z, GEOSGeometry **out) {
    Canvas *v = (Canvas *)c;
    if (z < v->zp - 0.12) *out = shp_clone(v->cv);
    else if (z < v->zp) *out = shp_clone(v->band);
    else { *out = NULL; return SO_OK; }
    return *out ? SO_OK : SO_ERR;
}
static double pole_rf(void *c, double s) { (void)c; return (s < 0.1 || s > 0.9) ? 0.22 : 0.26; }
static double load_rf(void *c, double s) {
    double W = *(double *)c;
    return (W * 0.36) * (s < 0.12 ? 0.62 : (s < 0.3 ? (0.62 + 0.38 * MN(1.0, (s - 0.12) / 0.08)) : 1.0 - 0.3 * (s - 0.3) / 0.7));
}
static void stretcher(double cx, double cy, double a, PyRandom *rng, SoObj *o, GG **foot) {
    F ff = FR(cx, cy, a); double L = u(8.0, 8.5), W = u(2.2, 2.4); so_obj_init(o, "cloth"); double zp = 0.62, Lc = L - 2.4;
    double *Wp = (double *)xr_mem(sizeof(double)); *Wp = W;
    GG *cv = RECT(cx, cy, Lc, W - 0.2, a);
    { GL l = { 0 }; for (int i = 0; i < 2; i++) GA(&l, RECTp(FF(&ff, sgn(i) * Lc * 0.38, 0), Lc * 0.26, W, a)); GG *band = INT(cv, UL(&l));
      Canvas *v = (Canvas *)malloc(sizeof *v); v->cv = shp_clone(cv); v->band = shp_clone(band); v->zp = zp;
      S *s = SN(); Solid *so = sol_custom(canvas_sect, v, canvas_free, zp); sol_push(s, so); sol_unref(so);
      C cu[1] = { { BARL(FF(&ff, -Lc / 2, 0), FF(&ff, Lc / 2, 0), 0.12), zp - 0.24 } }; ST(o, s, NULL, -1, cu, 1, cv); }
    for (int i = 0; i < 2; i++) { double sv = sgn(i); S *s = SN(); TBAR(s, FF(&ff, -L / 2, sv * W / 2), FF(&ff, L / 2, sv * W / 2), 0, pole_rf, NULL, 0.4, 1, zp - 0.02, zp - 0.02); ST(o, s, "wood", -1, NULL, 0, RECTp(FF(&ff, 0, sv * W / 2), L, 0.6, a)); }
    for (int i = 0; i < 2; i++) {
        double x = sgn(i) * (Lc / 2 + 0.25); S *s = SN(); EXT(s, RECTp(FF(&ff, x, 0), 0.3, W, a), zp + 0.05, 0);
        for (int j = 0; j < 2; j++) EXT(s, RECTp(FF(&ff, x, sgn(j) * W / 2), 0.62, 0.62, a), zp + 0.26, 0);
        ST(o, s, "iron", -1, NULL, 0, RECTp(FF(&ff, x, 0), 0.6, W + 0.6, a));
    }
    double load = rnd();
    if (load < 0.55) {
        double Lb = u(5.0, 5.4), x = u(-0.2, 0.2); P c = FF(&ff, x, 0); double t = a + u(-0.04, 0.04); F f2 = FR(c.x, c.y, t);
        S *s = SN(); TBAR(s, FF(&f2, -Lb / 2, 0), FF(&f2, Lb / 2, 0), 0, load_rf, Wp, 0.4, 1, zp + 0.15, zp + 0.1);
        GL l = { 0 }; double xs[3] = { -Lb * 0.18, Lb * 0.12, Lb * 0.38 }; for (int k = 0; k < 3; k++) GA(&l, RECTp(FF(&f2, xs[k], 0), 0.18, W, t));
        C cu[1] = { { UL(&l), zp + 0.75 } }; ST(o, s, "sack", -1, cu, 1, RECT(c.x, c.y, Lb, W * 0.75, t));
        s = SN(); TBARz(s, FF(&f2, -Lb / 2 + 0.2, 0), FF(&f2, Lb / 2 - 0.3, 0), 0.22, zp + 0.98, zp + 0.75); ST(o, s, "sack", -1, NULL, 0, RECT(c.x, c.y, Lb, 0.5, t));
    } else {
        double x = u(-2.0, -0.8); P c = FF(&ff, x, 0); double t = a + u(-0.15, 0.15), bw = u(1.7, 2.1); GG *bl = RRECT(c.x, c.y, bw, W - 0.7, t, 0.25); int k = UP(o, bl, zp + 0.5, "sack"); F f2 = FR(c.x, c.y, t);
        { GL l = { 0 }; double vs[3] = { -0.35, 0.0, 0.35 }; for (int i = 0; i < 3; i++) GA(&l, BARL(FF(&f2, -2, vs[i]), FF(&f2, 2, vs[i]), 0.14)); CUT(o, UL(&l), k, 0.12); }
        double hx = u(0.8, 2.0), hy = u(-0.2, 0.2); P h = FF(&ff, hx, hy); double R = 0.95;
        double pr[32] = { R, zp, R, zp + 0.12, R - 0.18, zp + 0.14, 0.66, zp + 0.3 }; int np = 8; np += CRV(pr + np, 0.62, 0.22, zp + 0.3, zp + 0.95, 1, 6);
        S *s = SN(); TURN(s, h.x, h.y, pr, np / 2, 20, 0.9, a, 0.0); ST(o, s, "tin", -1, NULL, 0, DISCp(h, R, 20));
    }
    *foot = OUT(o);
}

/* sandbags: returns hull(foot), lay (not an Obj: the layers go straight into o->L) */
typedef struct { S *ps; C cuts[4]; int nc; GG *fp; } Course;
static void course(PyRandom *rng, const F *ff, double L, double W, double h, int k, int m, double dx, double b0, double v0, GL *foot, Course *out) {
    S *ps = SN(); GL fp = { 0 }; double lift = k * h * 0.82; out->nc = 0;
    for (int i = 0; i < m; i++) {
        double d1 = u(-0.08, 0.08); double x = (i - (m - 1) / 2.0) * (L - 0.05) + dx + d1; double v = v0 + u(-0.1, 0.1); P c = FF(ff, x, v);
        double b = b0 + u(-0.06, 0.06); b = b + PI * (double)RI(rng, 0, 1);
        double Ls = L * u(0.94, 1.0); XSACK(ps, c.x, c.y, b, Ls, W, h, lift);
        GA(foot, RECTp(c, L + 0.8, W, b)); GA(&fp, RECTp(c, L, W, b));
        F f2 = FR(c.x, c.y, b); out->cuts[out->nc].q = BARL(FF(&f2, -L * 0.36, 0), FF(&f2, L * 0.28, 0), 0.12); out->cuts[out->nc].fl = lift + h - 0.1; out->nc++;
    }
    out->ps = ps; out->fp = UL(&fp);
}
static void sandbags(double cx, double cy, double a, PyRandom *rng, SoObj *o, GG **foot) {
    F ff = FR(cx, cy, a); double L = u(2.15, 2.3), W = u(1.25, 1.35), h = u(0.64, 0.7); int n = 3; GL fo = { 0 };
    so_obj_init(o, "sack");
    Course got[4]; int ng = 0;
    if (rnd() < 0.3) { double v0s[2] = { -W / 2 + 0.12, W / 2 - 0.12 }; for (int i = 0; i < 2; i++) course(rng, &ff, L, W, h, 0, n, 0.0, a, v0s[i], &fo, &got[ng++]); }
    else course(rng, &ff, L, W, h, 0, n, 0.0, a, 0.0, &fo, &got[ng++]);
    { double dx = u(-0.1, 0.1); course(rng, &ff, L, W, h, 1, n - 1, dx, a, 0.0, &fo, &got[ng++]); }
    if (rnd() < 0.7) { double dx = u(-0.3, 0.3); course(rng, &ff, L, W, h, 2, n - 2, dx, a, 0.0, &fo, &got[ng++]); }
    C keep[16]; int nk = 0;
    for (int i = 0; i < ng; i++) for (int j = 0; j < got[i].nc; j++) {
        int hit = 0;
        for (int g = i + 1; g < ng && !hit; g++) { if (!got[g].fp || EMPTY(got[g].fp)) continue; if (INTERSECTS(got[i].cuts[j].q, got[g].fp)) hit = 1; }
        if (!hit) keep[nk++] = got[i].cuts[j];
    }
    S *all = SN(); for (int i = 0; i < ng; i++) SCAT(all, got[i].ps);
    { SoLayer *y; xr_ok(sd_ST(all, "sack", 0, 0, keep, nk, &y)); lyl_push_own(&o->L, y); }
    if (rnd() < 0.6) {
        double x = u(-L, L) * 0.4, sv = CH2(rng); double y = sv * (W * 1.6 + u(0.2, 0.5)); P c = FF(&ff, x, y); double b = a + u(-0.8, 0.8);
        S *s = SN(); XSACK(s, c.x, c.y, b, L, W, h, 0.0); SoLayer *ly; xr_ok(sd_ST(s, "sack", 0, 0, NULL, 0, &ly)); lyl_push_own(&o->L, ly);
        GA(&fo, DISCp(c, L / 2, 20));
    }
    *foot = HULL(&fo);
}

/* mortar */
static double barrel_rf(void *c, double s) { (void)c; return s < 0.06 ? 0.36 : (s < 0.95 ? 0.3 : 0.36); }
static double bomb_rf(void *c, double s) { (void)c; return s > 0.15 ? 0.36 : 0.2; }
static void mortar(double cx, double cy, double a, PyRandom *rng, SoObj *o, GG **foot) {
    F ff = FR(cx, cy, a); so_obj_init(o, "iron");
    double y = u(-0.8, -0.5); P pc = FF(&ff, -1.4, y); double t = a + u(-0.4, 0.4); F f2 = FR(pc.x, pc.y, t);
    GG *pg = RRECTp(pc, 2.0, 2.0, t, 0.25); int bp = UP(o, pg, 0.3, NULL);
    { GL l = { 0 }; double vs[2] = { -0.5, 0.5 }; for (int i = 0; i < 2; i++) GA(&l, RECTp(FF(&f2, 0, vs[i]), 1.8, 0.3, t)); for (int i = 0; i < 2; i++) GA(&l, RECTp(FF(&f2, vs[i], 0), 0.3, 1.8, t)); ON(o, UL(&l), bp, 0.14, NULL); }
    int so = ON(o, DISCp(pc, 0.55, 14), bp, 0.4, NULL); CUT(o, DISCp(pc, 0.3, 10), so, 0.3);
    double bt = a + u(-0.15, 0.15); y = u(1.0, 1.3); P p = FF(&ff, -2.6, y); double d = u(4.0, 4.4); P q = POLp(p, d, bt);
    { P pq[2] = { p, q }; S *s = SN(); TBAR(s, p, q, 0, barrel_rf, NULL, 1.0, 0, 0, 0); ST(o, s, NULL, -1, NULL, 0, BUF(LSTR(pq, 2), 0.4, 16)); }
    y = u(-2.0, -1.7); P p2 = FF(&ff, -0.2, y); double bt2 = a + u(-0.3, 0.3); F f3 = FR(p2.x, p2.y, bt2);
    { GL l = { 0 }; GA(&l, BARL(FF(&f3, -1.4, -0.22), FF(&f3, 1.2, -0.05), 0.24)); GA(&l, BARL(FF(&f3, -1.4, 0.22), FF(&f3, 1.2, 0.05), 0.24)); GA(&l, RECTp(FF(&f3, 1.3, 0), 0.5, 0.6, bt2));
      GA(&l, DISCp(FF(&f3, -1.5, -0.22), 0.25, 6)); GA(&l, DISCp(FF(&f3, -1.5, 0.22), 0.25, 6));
      S *s = SN(); EXT(s, UL(&l), 0.32, 0); TBARc(s, FF(&f3, 1.3, 0), FF(&f3, 2.2, 0), 0.22, 1.0); ST(o, s, NULL, -1, NULL, 0, RECTp(FF(&f3, 0.4, 0), 3.8, 0.8, bt2)); }
    int nb = (int)RI(rng, 1, 3); y = u(-0.3, 0.3); P bc = FF(&ff, 1.7, y); double bl = 2.1, bw = 1.3, bh = u(1.05, 1.15); int par = -1;
    for (int k = 0; k < nb; k++) {
        double tb = a + 1.5708 + u(-0.08, 0.08); double gx = bc.x + u(-0.06, 0.06), gy = bc.y + u(-0.06, 0.06); GG *g = RECT(gx, gy, bl - 0.06 * k, bw - 0.06 * k, tb); F f4 = FR(bc.x, bc.y, tb);
        int bx = par < 0 ? UP(o, g, bh, NULL) : ON(o, g, par, bh, NULL);
        int lid = LID(o, g, bx, 0.1, 0.12, "wood");
        { GL l = { 0 }; double xs[2] = { -bl * 0.2, bl * 0.2 }; for (int i = 0; i < 2; i++) GA(&l, BARL(FF(&f4, xs[i], -bw), FF(&f4, xs[i], bw), 0.1)); CUT(o, UL(&l), lid, 0.07); }
        { P sp[4]; int n = 0; for (int i = 0; i < 2; i++) for (int j = 0; j < 2; j++) sp[n++] = FF(&f4, sgn(i) * (bl / 2 - 0.25), sgn(j) * (bw / 2 - 0.22)); STUDS(o, sp, 4, lid, 0.2, 0.08, "iron", 8); }
        par = bx;
    }
    { S *s = SN(); for (int i = 0; i < 2; i++) { double sx = sgn(i), ang = a + 1.5708 + (sx > 0 ? 0 : PI); TBARz(s, POLp(bc, bl / 2 - 0.2, ang), POLp(bc, bl / 2 + 0.25, ang), 0.22, bh * 0.6, bh * 0.6); }
      ST(o, s, "rope", -1, NULL, 0, DISCp(bc, 1.4, 20)); }
    if (par >= 0) { static const char *W4[4] = { "STOKES", "3IN", "HE", "AMMO" }; const char *w = W4[CHI(rng, 4)]; CUT(o, XWORD(w, bc.x, bc.y, a + 1.5708, 0.5, 0.12, 1, bl - 0.6), par, 0.08); }
    int n = (int)RI(rng, 1, 2);
    for (int i = 0; i < n; i++) {
        double x = u(0.8, 2.6), sv = CH2(rng); sv = sv * u(1.8, 2.1); P c = FF(&ff, x, sv); double tt = a + u(-0.6, 0.6); F f5 = FR(c.x, c.y, tt);
        S *s = SN(); TBAR(s, FF(&f5, -0.6, 0), FF(&f5, 0.6, 0), 0, bomb_rf, NULL, 0.9, 0, 0, 0);
        EXT(s, U2(BARL(FF(&f5, -0.95, -0.35), FF(&f5, -0.95, 0.35), 0.22), BARL(FF(&f5, -1.0, 0), FF(&f5, -0.6, 0), 0.24)), 0.42, 0);
        ST(o, s, NULL, -1, NULL, 0, RECT(c.x, c.y, 2.2, 0.8, tt));
    }
    *foot = OUT(o);
}

/* duckboard */
static void duckboard(double cx, double cy, double a, PyRandom *rng, SoObj *o, GG **foot) {
    F ff = FR(cx, cy, a); int n = (int)RI(rng, 6, 7); double st = u(1.12, 1.2), L = n * st + 0.9, W = u(2.5, 2.8); so_obj_init(o, "wood");
    { double x = u(-1.0, 1.0), y = u(-0.4, 0.4); P c = FF(&ff, x, y); double rx = L * u(0.3, 0.4); double t = a + u(-0.2, 0.2); GCUT(o, ELL(c.x, c.y, rx, W * 0.7, t, 20), 0.3, "water"); }
    double rv = W * 0.3; int rn;
    { GL l = { 0 }; for (int i = 0; i < 2; i++) GA(&l, RECTp(FF(&ff, 0, sgn(i) * rv), L, 0.6, a)); rn = UP(o, UL(&l), 0.36, NULL); }
    { GL l = { 0 }; for (int i = 0; i < 2; i++) { double sv = sgn(i); GA(&l, BARL(FF(&ff, -L, sv * rv + 0.05), FF(&ff, L, sv * rv - 0.05), 0.1)); } CUT(o, UL(&l), rn, 0.07); }
    int gone = -1; if (rnd() < 0.5) gone = (int)RI(rng, 0, n - 1);
    for (int i = 0; i < n; i++) {
        if (i == gone) continue;
        double x = (i - (n - 1) / 2.0) * st, t = a + u(-0.05, 0.05), w = W, v = 0.0;
        if (rnd() < 0.15) { w = W * u(0.55, 0.7); v = CH2(rng) * (W - w) / 2; }
        double d1 = u(-0.04, 0.04), d2 = u(-0.08, 0.08); P c = FF(&ff, x + d1, v + d2); F f2 = FR(c.x, c.y, t); GG *g = RECTp(c, 0.82, w, t); double hh = u(0.58, 0.64); int s = UP(o, g, hh, NULL);
        { GL l = { 0 }; double vx[2] = { -0.2, 0.18 }; for (int k = 0; k < 2; k++) { P p = FF(&f2, vx[k], -w / 2 + 0.15); double e = u(-0.05, 0.05); GA(&l, BARL(p, FF(&f2, vx[k] + e, w / 2 - 0.15), 0.1)); } CUT(o, UL(&l), s, 0.07); }
        { GL l = { 0 }; double dx[2] = { -0.2, 0.2 }; for (int k = 0; k < 2; k++) for (int j = 0; j < 2; j++) { double sv = sgn(j); if (fabs(sv * rv - v) < w / 2 - 0.2) GA(&l, DISCp(FF(&f2, dx[k], sv * rv - v), 0.15, 8)); } CUT(o, UL(&l), s, 0.06); }
        if (rnd() < 0.25) { double e1 = u(-0.1, 0.1), e2 = u(-w * 0.3, w * 0.3); P q = FF(&f2, e1, e2); S *sl = SN(); DOME(sl, q.x, q.y, 0.5, 0.3, 0, 0.7, t, 20); ST(o, sl, "earth", s, NULL, 0, DISCp(c, 0.4, 20)); }
    }
    *foot = OUT(o);
}

/* trench_sign */
static void trench_sign(double cx, double cy, double a, PyRandom *rng, SoObj *o, GG **foot) {
    static const char *WORDS[11] = { "PICCADILLY", "HELLFIRE", "TO-FRONT", "AID-POST", "GAS", "SNIPER", "HQ", "BOND-ST", "WIPERS", "DUMP", "KEEP-LOW" };
    F ff = FR(cx, cy, a); so_obj_init(o, "wood"); double Ls = u(5.8, 6.3), L = u(3.8, 4.3), W = u(1.4, 1.6);
    GL spl = { 0 }; GA(&spl, RECTp(FF(&ff, -0.4, 0), Ls - 0.8, 0.62, a));
    { double vs[3] = { -0.2, 0.0, 0.2 }; for (int i = 0; i < 3; i++) { P p = FF(&ff, Ls / 2 - 0.85, vs[i]); double d = u(0.3, 0.7); GA(&spl, TRI(p, FF(&ff, Ls / 2 - 0.85 + d, vs[i] * 0.6), 0.22)); } }
    int sk = UP(o, UL(&spl), 0.4, NULL); CUT(o, BARL(FF(&ff, -Ls / 2, 0.08), FF(&ff, Ls / 2 - 1, -0.05), 0.1), sk, 0.07);
    P c = FF(&ff, -Ls / 2 + 1.1, 0); double t = a + 1.5708 + u(-0.25, 0.25); F f2 = FR(c.x, c.y, t); double sv = CH2(rng); int arrow = rnd() < 0.55;
    GG *g;
    if (arrow) { P q[5] = { FF(&f2, -L / 2, -W / 2), FF(&f2, L / 2 - 0.7, -W / 2), FF(&f2, L / 2, 0), FF(&f2, L / 2 - 0.7, W / 2), FF(&f2, -L / 2, W / 2) }; g = POLY(q, 5); }
    else g = RECTp(c, L, W, t);
    if (sv < 0) g = ROT(g, PI, SHP_ORIGIN_POINT, c.x, c.y, 1);
    int b = UP(o, g, 0.56, NULL);
    { double v1 = u(-0.45, -0.35); P p1 = FF(&f2, -L, v1); double v2 = u(-0.45, -0.35); CUT(o, INT(BARL(p1, FF(&f2, L, v2), 0.12), g), b, 0.1); }
    const char *word = WORDS[CHI(rng, 11)];
    P wc = FF(&f2, arrow ? -0.25 * sv : 0.0, 0.12);
    GG *wd = XWORD(word, wc.x, wc.y, t + (sv < 0 ? PI : 0.0), 0.8, 0.15, 1, L - (arrow ? 1.2 : 0.5));
    if (wd) CUT(o, INT(wd, INS(g, 0.18)), b, 0.12);
    { P sp[2] = { FF(&f2, 0, -W * 0.32), FF(&f2, 0, W * 0.34) }; STUDS(o, sp, 2, b, 0.2, 0.08, "iron", 8); }
    if (rnd() < 0.5) { GL l = { 0 }; int n = (int)RI(rng, 1, 2); for (int i = 0; i < n; i++) { double x = u(-L * 0.3, L * 0.2), y = u(-0.4, 0.4); GA(&l, DISCp(FF(&f2, x, y), 0.16, 8)); } CUT(o, UL(&l), b, 0.3); }
    if (rnd() < 0.6) {
        P pts[4]; pts[0] = FF(&ff, -Ls / 2 + 2.2, 0.3); pts[1] = FF(&ff, -Ls / 2 + 2.5, 0.7); double d = u(0.9, 1.3); pts[2] = FF(&ff, -Ls / 2 + 3.2, d); d = u(0.6, 1.1); pts[3] = FF(&ff, -Ls / 2 + 3.9, d);
        S *s = SN(); ROD(s, pts, 4, 0.17, NULL, NULL, 1.4, 0, 0); ST(o, s, "wire", -1, NULL, 0, BUF(LSTR(pts, 4), 0.25, 16));
    }
    *foot = OUT(o);
}

static const XrDef DEFS[] = {
    { "trederra", "made", "cloth", "stretcher", 10.5, 0, 1, stretcher },
    { "trederra", "made", "sack", "sandbags", 10.5, 0, 1, sandbags },
    { "trederra", "made", "iron", "mortar", 10.5, 0, 1, mortar },
    { "trederra", "made", "wood", "duckboard", 10.5, 0, 1, duckboard },
    { "trederra", "made", "wood", "trench_sign", 10.5, 0, 1, trench_sign },
};
void bld_sdrx_trederra(void) { xr_register(DEFS, (int)(sizeof DEFS / sizeof DEFS[0])); }
