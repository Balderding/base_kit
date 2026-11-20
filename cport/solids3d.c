/* solids3d.c: --supports on, the true 3D solids (see solids3d.h). Each function names the Python it copies. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "pyx.h"
#include "pyexact.h"
#include "shp.h"
#include "solids3d.h"

#define S3_PI 3.141592653589793          /* math.pi */

static int s3_fail(char *exc, int exccap, char *text, int textcap, const char *e, const char *t) {
    snprintf(exc, (size_t)exccap, "%s", e); snprintf(text, (size_t)textcap, "%s", t); return MG_ERR;
}
/* a meshglue error "Name: text" split into the exception's name and str(e) */
static int s3_fail_mg(char *exc, int exccap, char *text, int textcap) {
    const char *c = strstr(mg_err, ": ");
    if (c) { snprintf(exc, (size_t)exccap, "%.*s", (int)(c - mg_err), mg_err); snprintf(text, (size_t)textcap, "%s", c + 2); }
    else { snprintf(exc, (size_t)exccap, "%s", mg_err[0] ? mg_err : "Exception"); text[0] = 0; }
    return MG_ERR;
}

/* the x[2] lambdas:
   slab:  lambda zb, zs=1.0: _extrude(poly.intersection(Point(0, 0).buffer(gg.R0 - gg.H * math.tan(math.radians(RIM)) - 0.15, 96)), zb + z0 * zs, zb + z1 * zs)
   vault: r = W / 2; c = lift * 1.5 + 0.8 * r; ca, sa = math.cos(a), math.sin(a)
          lambda zb, zs=1.0: _sweep([(cx + u * ca, cy + u * sa, zb + (c if axle is None else axle)) for u in (-L / 2, 0.0, L / 2)], [r] * 3, 20 if axle is not None else 14)
   lathe: lambda zb, zs=1.0: _revolve(cx, cy, [(r_, z_ * zs) for r_, z_ in real], min(seg, 24), squash, a, zb + lift * zs)
   loft:  k3 = [i for i in range(0, m + 1, 2) if rs[i] >= 0.15]; c = lift * 1.5 + 0.8 * max(rs)
          lambda zb, zs=1.0: _sweep([ff(us[i], 0) + (zb + c,) for i in k3], [rs[i] for i in k3], 14)
   tube:  k3 = [i for i in range(m + 1) if R[i] >= 0.12]
          lambda zb, zs=1.0: _sweep([(P[i].x, P[i].y, zb + lift + 0.75 * R[i] + arch * math.sin(math.pi * i / m) ** 0.7) for i in k3], [R[i] for i in k3], 10) */
int s3_build(const Solid *s, double zb, double zs, MgMesh *out, char *exc, int exccap, char *text, int textcap) {
    So3dView v; int rc;
    memset(out, 0, sizeof *out); exc[0] = 0; text[0] = 0;
    if (so_view3d(s, &v) != SO_OK) return s3_fail(exc, exccap, text, textcap, "TypeError", "not a solid with a 3D build");
    switch (v.type) {
    case SO3_SLAB:
        rc = mg_slab3d(v.poly, v.z0, v.z1, zb, zs, SO.R0, SO.H, SO.rim, out);
        return rc < 0 ? s3_fail_mg(exc, exccap, text, textcap) : 0;
    case SO3_VAULT: {
        double r = v.W / 2, c = v.lift * 1.5 + 0.8 * r, ca = py_cos(v.a), sa = py_sin(v.a), z = zb + (v.has_axle ? v.axle : c);
        double us[3] = { -v.L / 2, 0.0, v.L / 2 }, pts[9], radii[3] = { r, r, r };
        for (int k = 0; k < 3; k++) { pts[3 * k] = v.cx + us[k] * ca; pts[3 * k + 1] = v.cy + us[k] * sa; pts[3 * k + 2] = z; }
        rc = mg_sweep3d(pts, radii, 3, v.has_axle ? 20 : 14, 1.0, out);
        return rc < 0 ? s3_fail_mg(exc, exccap, text, textcap) : 0;
    }
    case SO3_LATHE: {
        double *prof = (double *)malloc(sizeof(double) * 2 * (size_t)(v.nreal ? v.nreal : 1));
        for (int i = 0; i < v.nreal; i++) { prof[2 * i] = v.real[2 * i]; prof[2 * i + 1] = v.real[2 * i + 1] * zs; }
        rc = mg_revolve3d(v.cx, v.cy, prof, v.nreal, v.seg < 24 ? v.seg : 24, v.squash, v.a, zb + v.lift * zs, 0.3, out);
        free(prof);
        return rc < 0 ? s3_fail_mg(exc, exccap, text, textcap) : 0;
    }
    case SO3_LOFT: {
        double mr = v.rs[0], *pts = (double *)malloc(sizeof(double) * 3 * (size_t)(v.m + 1)), *radii = (double *)malloc(sizeof(double) * (size_t)(v.m + 1)), c;
        int n = 0;
        for (int i = 1; i <= v.m; i++) if (v.rs[i] > mr) mr = v.rs[i];          /* max(rs): the first of equal values */
        c = v.lift * 1.5 + 0.8 * mr;
        for (int i = 0; i <= v.m; i += 2) if (v.rs[i] >= 0.15) {
            so_ff(&v.ff, v.us[i], 0.0, &pts[3 * n], &pts[3 * n + 1]); pts[3 * n + 2] = zb + c; radii[n] = v.rs[i]; n++;
        }
        rc = mg_sweep3d(pts, radii, n, 14, 1.0, out);
        free(pts); free(radii);
        return rc < 0 ? s3_fail_mg(exc, exccap, text, textcap) : 0;
    }
    case SO3_TUBE: {
        double *pts = (double *)malloc(sizeof(double) * 3 * (size_t)(v.m + 1)), *radii = (double *)malloc(sizeof(double) * (size_t)(v.m + 1));
        int n = 0;
        for (int i = 0; i <= v.m; i++) if (v.R[i] >= 0.12) {
            double re, im; int pr = px_pow(py_sin(S3_PI * i / v.m), 0.7, &re, &im);
            if (pr != 0) {          /* a hair below zero to the 0.7: a complex height, and _sweep's math.sqrt of it raises */
                free(pts); free(radii);
                return pr == 1 ? s3_fail(exc, exccap, text, textcap, "TypeError", "must be real number, not complex") : s3_fail(exc, exccap, text, textcap, "ValueError", "math domain error");
            }
            pts[3 * n] = shp_x(v.P[i]); pts[3 * n + 1] = shp_y(v.P[i]); pts[3 * n + 2] = zb + v.lift + 0.75 * v.R[i] + v.arch * re; radii[n] = v.R[i]; n++;
        }
        rc = mg_sweep3d(pts, radii, n, 10, 1.0, out);
        free(pts); free(radii);
        return rc < 0 ? s3_fail_mg(exc, exccap, text, textcap) : 0;
    }
    }
    return s3_fail(exc, exccap, text, textcap, "TypeError", "not a solid with a 3D build");
}

void s3_shells_free(S3Shells *s) { for (int i = 0; i < s->n; i++) mg_mesh_free(&s->v[i]); free(s->v); memset(s, 0, sizeof *s); }
static void s3_push(S3Shells *s, MgMesh m) {
    if (s->n == s->cap) { s->cap = s->cap ? 2 * s->cap : 8; s->v = (MgMesh *)realloc(s->v, sizeof(MgMesh) * (size_t)s->cap); }
    s->v[s->n++] = m;
}

/* mesher.add_solids:
       alive = {getattr(op[-1], "ghost", None) for op in s.ops}
       for cid, zb, zs in s.solids3d:
           if cid not in alive: continue
           for mk in scenes.TRUE3D.get(cid, []):
               try: V3, F3 = mk(zb, zs); t3 = trimesh.Trimesh(V3, F3, process=False); (invert, press inside: mg_solid_fit); extra.append(t3)
               except Exception as e: skipped.append(f"solid {cid}: {type(e).__name__}"); kitlog.caught(f"solid {cid} dropped", e) */
void s3_add_solids(const FtS3dList *l, const int64_t *alive, int nalive, int has_oval, double A, double B, double rlim,
                   S3Shells *out, MgStrs *skipped) {
    for (int i = 0; i < l->n; i++) {
        int64_t cid = l->v[i].c; int live = 0; const SoSolids *mk;
        for (int k = 0; k < nalive && !live; k++) live = alive[k] == cid;
        if (!live || !(mk = so_true3d(cid))) continue;
        for (int j = 0; j < mk->n; j++) {
            MgMesh t; char exc[64], text[400], line[160];
            int rc = s3_build(mk->v[j], l->v[i].zb, l->v[i].zs, &t, exc, sizeof exc, text, sizeof text);
            if (rc == 0 && mg_solid_fit(&t, has_oval, A, B, rlim) < 0) rc = s3_fail_mg(exc, sizeof exc, text, sizeof text);
            if (rc == 0) { s3_push(out, t); continue; }
            mg_mesh_free(&t);
            snprintf(line, sizeof line, "solid %lld: %s", (long long)cid, exc); mg_strs_push(skipped, line);
            snprintf(line, sizeof line, "solid %lld dropped", (long long)cid); mg_log_caught(line, exc, text, "");
        }
    }
}

static void s3_u32(MgBuf *o, uint32_t x) { mg_buf_add(o, &x, 4); }
void s3_blob(const S3Shells *s, MgBuf *out) {
    mg_buf_add(out, "BBS1", 4); s3_u32(out, (uint32_t)s->n);
    for (int i = 0; i < s->n; i++) {
        s3_u32(out, (uint32_t)s->v[i].nv); s3_u32(out, (uint32_t)s->v[i].nt);
        if (s->v[i].nv) mg_buf_add(out, s->v[i].v, sizeof(double) * 3 * (size_t)s->v[i].nv);
        if (s->v[i].nt) mg_buf_add(out, s->v[i].f, sizeof(int) * 3 * (size_t)s->v[i].nt);
    }
}
