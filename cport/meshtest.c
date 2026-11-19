/* meshtest: the proof run for the C mesher. For every <name>.min in a folder (cport/dump_mesh.py), mesh it in C and
   compare with <name>.ref (what Python's mesh_from_field returned): every vertex and every triangle, bit for bit.
       meshtest <folder> [name ...]
   Prints one line per base: SAME / DIFF (first difference) / FAIL (C raised what Python did not), and the time. */
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "mesh.h"
#include "pyx.h"

static double now(void) { LARGE_INTEGER f, c; QueryPerformanceFrequency(&f); QueryPerformanceCounter(&c); return (double)c.QuadPart / (double)f.QuadPart; }

static int one(const char *dir, const char *name) {
    char p[MAX_PATH]; MeshIn in; MeshOut out; memset(&out, 0, sizeof out);
    snprintf(p, sizeof p, "%s\\%s.min", dir, name);
    if (!mesh_read(p, &in)) { printf("FAIL %-24s cannot read %s\n", name, p); return 0; }
    double t0 = now(); int ok = mesh_from_field(&in, &out); double dt = now() - t0;
    if (!ok) { printf("FAIL %-24s %s (%.2f s)\n", name, out.err, dt); return 0; }
    snprintf(p, sizeof p, "%s\\%s.ref", dir, name);
    FILE *f = fopen(p, "rb"); if (!f) { printf("FAIL %-24s no %s\n", name, p); return 0; }
    char mg[4]; unsigned nv = 0, nt = 0; fread(mg, 1, 4, f); fread(&nv, 4, 1, f); fread(&nt, 4, 1, f);
    double *v = malloc(sizeof(double) * 3 * (nv ? nv : 1)); int *t = malloc(sizeof(int) * 3 * (nt ? nt : 1));
    fread(v, sizeof(double), 3 * nv, f); fread(t, sizeof(int), 3 * nt, f); fclose(f);
    int same = (int)nv == out.nv && (int)nt == out.nt;
    if (!same) { printf("DIFF %-24s counts: Python %u verts %u tris, C %d verts %d tris (%.2f s)\n", name, nv, nt, out.nv, out.nt, dt); return 0; }
    for (unsigned i = 0; i < 3 * nv; i++) if (memcmp(&v[i], &out.v[i], 8)) { printf("DIFF %-24s vertex %u.%u: Python %.17g, C %.17g (%.2f s)\n", name, i / 3, i % 3, v[i], out.v[i], dt); return 0; }
    for (unsigned i = 0; i < 3 * nt; i++) if (t[i] != out.t[i]) { printf("DIFF %-24s triangle %u: Python %d %d %d, C %d %d %d (%.2f s)\n", name, i / 3, t[i / 3 * 3], t[i / 3 * 3 + 1], t[i / 3 * 3 + 2], out.t[i / 3 * 3], out.t[i / 3 * 3 + 1], out.t[i / 3 * 3 + 2], dt); return 0; }
    printf("SAME %-24s %u verts %u tris, %.2f s\n", name, nv, nt, dt);
    free(v); free(t); return 1;
}

int main(int argc, char **argv) {
    if (argc < 2) { fprintf(stderr, "meshtest <folder> [name ...]\n"); return 2; }
    if (!geos_load() || !pyx_load()) return 2;
    printf("GEOS %s\n", G.GEOSversion());
    int n = 0, good = 0;
    if (argc > 2) for (int i = 2; i < argc; i++) { n++; good += one(argv[1], argv[i]); }
    else {
        char pat[MAX_PATH]; WIN32_FIND_DATAA fd; snprintf(pat, sizeof pat, "%s\\*.min", argv[1]);
        HANDLE h = FindFirstFileA(pat, &fd);
        if (h != INVALID_HANDLE_VALUE) { do { char nm[MAX_PATH]; snprintf(nm, sizeof nm, "%s", fd.cFileName); nm[strlen(nm) - 4] = 0; n++; good += one(argv[1], nm); } while (FindNextFileA(h, &fd)); FindClose(h); }
    }
    printf("%d of %d the same\n", good, n);
    return good == n ? 0 : 1;
}
