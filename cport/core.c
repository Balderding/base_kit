/* core.c: the kit's C parts inside bbb_kit.exe, for the Python builder while the port is under way.
     bbb_kit.exe --core-mesh      stdin: the mesher's input (.min format, cport/dump_mesh.py)
                                  stdout: "BBR1", u32 nv, nt, nv x 3 f64, nt x 3 i32 (as mesh_from_field returns)
                                  exit 0 = done; 1 = an error (stderr says what; Python then meshes it itself)
   No window is made. GEOS is shapely's own geos_c.dll (geos_dyn.c). */
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <io.h>
#include <fcntl.h>
#include "mesh.h"
#include "pyx.h"
#include "shot.h"
#include "field.h"
#include "mt.h"

static unsigned char *read_all(HANDLE h, size_t *n) {
    size_t cap = 1 << 20, len = 0; unsigned char *b = malloc(cap); DWORD got;
    for (;;) {
        if (len == cap) { cap *= 2; b = realloc(b, cap); }
        if (!ReadFile(h, b + len, (DWORD)(cap - len > 0x40000000 ? 0x40000000 : cap - len), &got, NULL) || !got) break;
        len += got;
    }
    *n = len; return b;
}
static int write_all(HANDLE h, const void *p, size_t n) {
    const unsigned char *c = p; DWORD put;
    while (n) { if (!WriteFile(h, c, (DWORD)(n > 0x40000000 ? 0x40000000 : n), &put, NULL) || !put) return 0; c += put; n -= put; }
    return 1;
}
static void say(const char *m) { DWORD put; HANDLE e = GetStdHandle(STD_ERROR_HANDLE); if (e && e != INVALID_HANDLE_VALUE) WriteFile(e, m, (DWORD)strlen(m), &put, NULL); }

static int core_mesh(void) {
    HANDLE in = GetStdHandle(STD_INPUT_HANDLE), out = GetStdHandle(STD_OUTPUT_HANDLE);
    if (!in || in == INVALID_HANDLE_VALUE || !out || out == INVALID_HANDLE_VALUE) { say("no stdin/stdout\n"); return 1; }
    if (!geos_load() || !pyx_load()) { say("GEOS or the UCRT math could not be loaded\n"); return 1; }
    size_t n; unsigned char *blob = read_all(in, &n);
    MeshIn mi; MeshOut mo; memset(&mo, 0, sizeof mo);
    if (!mesh_read_buf(blob, n, &mi)) { say("input not readable\n"); return 1; }
    free(blob);
    int ok = mesh_from_field(&mi, &mo); mesh_in_free(&mi);
    if (!ok) { say(mo.err); say("\n"); return 1; }
    uint32_t hd[2] = { (uint32_t)mo.nv, (uint32_t)mo.nt };
    if (!write_all(out, "BBR1", 4) || !write_all(out, hd, 8) || !write_all(out, mo.v, sizeof(double) * 3 * (size_t)mo.nv) || !write_all(out, mo.t, sizeof(int) * 3 * (size_t)mo.nt)) return 1;
    return 0;
}

/* bbb_kit.exe --core-build: stdin "BBF1" (the height field's steps, cmesh.pack_ops); the field is cut here (on
   KIT_MESH_THREADS threads) and meshed. stdout: as --core-mesh, then "BBX1", u32 cells, u32 n + n bytes skipped text. */
static int core_build(void) {
    HANDLE in = GetStdHandle(STD_INPUT_HANDLE), out = GetStdHandle(STD_OUTPUT_HANDLE);
    if (!geos_load() || !pyx_load()) { say("GEOS or the UCRT math could not be loaded\n"); return 1; }
    size_t n; unsigned char *blob = read_all(in, &n);
    MeshIn mi; MeshOut mo; memset(&mo, 0, sizeof mo); static char sk[8192]; char err[256] = "";
    if (!field_build(blob, n, mt_threads(), &mi, sk, sizeof sk, err, sizeof err)) { say(err); say("\n"); return 1; }
    free(blob);
    uint32_t ncell = (uint32_t)mi.ncells;
    int ok = mesh_from_field(&mi, &mo); mesh_in_free(&mi);
    if (!ok) { say(mo.err); say("\n"); return 1; }
    uint32_t hd[2] = { (uint32_t)mo.nv, (uint32_t)mo.nt }, x[2] = { ncell, (uint32_t)strlen(sk) };
    if (!write_all(out, "BBR1", 4) || !write_all(out, hd, 8) || !write_all(out, mo.v, sizeof(double) * 3 * (size_t)mo.nv) || !write_all(out, mo.t, sizeof(int) * 3 * (size_t)mo.nt)) return 1;
    if (!write_all(out, "BBX1", 4) || !write_all(out, x, 8) || !write_all(out, sk, x[1])) return 1;
    return 0;
}

/* bbb_kit.exe --core-shot: stdin "BBS1", u32 w, h, mode (1 video card, 2 software), f64 elev, azim, u32 nv, nt,
   nv x 3 f64, nt x 3 i32. stdout "BBI1", u32 w, h, u8 n + n bytes (what drew it), w*h*3 bytes RGB, rows top down. */
static int core_shot(void) {
    HANDLE in = GetStdHandle(STD_INPUT_HANDLE), out = GetStdHandle(STD_OUTPUT_HANDLE);
    size_t n; unsigned char *b = read_all(in, &n);
    if (n < 40 || memcmp(b, "BBS1", 4)) { say("input not readable\n"); return 1; }
    uint32_t w, h, mode, nv, nt; double el, az; const unsigned char *p = b + 4;
    memcpy(&w, p, 4); memcpy(&h, p + 4, 4); memcpy(&mode, p + 8, 4); memcpy(&el, p + 12, 8); memcpy(&az, p + 20, 8); memcpy(&nv, p + 28, 4); memcpy(&nt, p + 32, 4); p += 36;
    if ((size_t)(p - b) + 24 * (size_t)nv + 12 * (size_t)nt > n) { say("input too short\n"); return 1; }
    double *V = malloc(24 * (size_t)(nv ? nv : 1)); int *F = malloc(12 * (size_t)(nt ? nt : 1)); memcpy(V, p, 24 * (size_t)nv); memcpy(F, p + 24 * (size_t)nv, 12 * (size_t)nt); free(b);
    for (uint32_t i = 0; i < 3 * nt; i++) if (F[i] < 0 || (uint32_t)F[i] >= nv) { say("triangle corner out of range\n"); return 1; }
    unsigned char *rgb = malloc((size_t)w * h * 3); char dev[160] = "", err[200] = "";
    if (!rgb || !shot_render(V, (int)nv, F, (int)nt, (int)w, (int)h, el, az, (int)mode, rgb, dev, sizeof dev, err, sizeof err)) { say(err); say("\n"); return 1; }
    uint32_t hd[2] = { w, h }; unsigned char dl = (unsigned char)strlen(dev);
    if (!write_all(out, "BBI1", 4) || !write_all(out, hd, 8) || !write_all(out, &dl, 1) || !write_all(out, dev, dl) || !write_all(out, rgb, (size_t)w * h * 3)) return 1;
    return 0;
}

int bbb_core_main(const char *cmd) {
    if (!strncmp(cmd, "--core-mesh", 11)) return core_mesh();
    if (!strncmp(cmd, "--core-shot", 11)) return core_shot();
    if (!strncmp(cmd, "--core-build", 12)) return core_build();
    if (!strncmp(cmd, "--core-version", 14)) { say("bbb_kit core 3: mesher, shot, field\n"); return 0; }
    say("unknown --core- command\n"); return 2;
}
