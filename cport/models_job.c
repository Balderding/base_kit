/* models_job.c: bbb_kit.exe --job models [--pack <file>] (2026-11-20), the window's Models preview. Every model file the kit
   uses (the model table, bld_sd_models.c), rendered once by the kit's own picture renderer (shot.c, the same shot as a built
   base's), at its real size at true scale (18 mm a metre: 1:56 beside a 32 mm man), into <out>\model_previews\<file>.png.
   index.tsv beside them, a line a model: picture, name, styles, length mm, height mm, file. A picture already made is kept
   (made again only when its model's size changes: the size is in its name).
   2026-11-21: drawn on the video card from every core at once (each thread its own Direct3D device, shot_render straight,
   no child exe a picture), and --pack <file> writes them all into one file (gui\previews.bin, the exe's PREVIEWS_BIN
   resource: gui\build.cmd makes it, so the Models window shows them at once from the exe, models_popup.c).
   Pack: "BBPV1\n", u32 count, then a model each: u32 n + n bytes "name\tstyles\tlength\theight\tfile", u32 n + n bytes PNG.
   Included by runner.c (it uses the runner's helpers). */
int bsdde_model_row(int i, const char **style, const char **name, const char **rel, double *mm);          /* bld_sd_models.c (bld_sdde.h clashes with the runner) */
#include "features.h"
#include "safestr.h"          /* bounded text copies (2026-11-23) */
#include "models.h"
#include "shot.h"
typedef struct { const char *rel, *name; char styles[1200]; double L, real, Lmm, Hmm; char png[MAX_PATH * 3]; Mesh m; int ok; } MvEnt;
typedef struct { MvEnt *E; int *todo; int mode, n; CRITICAL_SECTION cs; volatile LONG done, next; } MvJob;
static void mv_draw(MvJob *J, int k) {          /* one picture */
    MvEnt *e = &J->E[J->todo[k]]; char dev[160] = "", err[200] = ""; unsigned char *rgb = (unsigned char *)malloc(320 * 320 * 3);
    if (rgb && shot_render(e->m.v, e->m.nv, e->m.f, e->m.nt, 320, 320, 30, -60, J->mode, rgb, dev, sizeof dev, err, sizeof err)) {
        EnterCriticalSection(&J->cs); e->ok = d2_png_write(e->png, rgb, 320, 320, 3); LeaveCriticalSection(&J->cs);          /* the PNG writer fills its checksum table on first use: one at a time */
    }
    free(rgb); free(e->m.v); free(e->m.f); e->m.v = NULL; e->m.f = NULL;
    { LONG d = InterlockedIncrement(&J->done); if (d % 25 == 0) { EnterCriticalSection(&J->cs); ev("progress", "ii", "done", (int)d, "of", J->n); LeaveCriticalSection(&J->cs); } }
}
static DWORD WINAPI mv_worker(LPVOID p) {          /* a thread: the next picture not yet taken, until none are left */
    MvJob *J = (MvJob *)p; LONG k;
    shot_keep(1);          /* one device and one set of shaders a thread, not one a picture */
    while ((k = InterlockedIncrement(&J->next) - 1) < J->n) mv_draw(J, (int)k);
    shot_keep(0);
    return 0;
}
static int mv_pack(const char *path, MvEnt *E, int ne) {          /* every picture and its line into one file */
    char tmp[MAX_PATH * 3]; FILE *f; int i, n = 0; uint32_t c;
    snprintf(tmp, sizeof tmp, "%s.tmp", path); f = fopen(tmp, "wb"); if (!f) return 0;
    for (i = 0; i < ne; i++) n += E[i].ok;
    fwrite("BBPV1\n", 1, 6, f); c = (uint32_t)n; fwrite(&c, 4, 1, f);
    for (i = 0; i < ne; i++) {
        char line[2000]; FILE *p; long sz; unsigned char *b;
        if (!E[i].ok) continue;
        snprintf(line, sizeof line, "%s\t%s\t%.0f\t%.0f\t%s", E[i].name, E[i].styles, E[i].Lmm, E[i].Hmm, E[i].rel);
        p = fopen(E[i].png, "rb"); if (!p) { fclose(f); return 0; }
        fseek(p, 0, SEEK_END); sz = ftell(p); fseek(p, 0, SEEK_SET); b = (unsigned char *)malloc((size_t)sz + 1);
        if (!b || fread(b, 1, (size_t)sz, p) != (size_t)sz) { fclose(p); free(b); fclose(f); return 0; }
        fclose(p);
        c = (uint32_t)strlen(line); fwrite(&c, 4, 1, f); fwrite(line, 1, c, f);
        c = (uint32_t)sz; fwrite(&c, 4, 1, f); fwrite(b, 1, (size_t)sz, f); free(b);
    }
    if (fclose(f)) return 0;
    { WCHAR a[MAX_PATH * 3], b2[MAX_PATH * 3]; wpath(tmp, a, MAX_PATH * 3); wpath(path, b2, MAX_PATH * 3); return MoveFileExW(a, b2, MOVEFILE_REPLACE_EXISTING) != 0; }
}
static int job_models(const char *pack) {
    char dir[MAX_PATH * 3], idx[MAX_PATH * 3], tmp[MAX_PATH * 3]; MvEnt *E = NULL; int ne = 0, cap = 0, i, j, made = 0, kept = 0, nt = 0, *todo; FILE *fi; MvJob J;
    snprintf(dir, sizeof dir, "%s\\model_previews", out_root()); make_dirs(dir);
    snprintf(idx, sizeof idx, "%s\\index.tsv", dir); snprintf(tmp, sizeof tmp, "%s\\index.tmp", dir);
    mdl_blob_from_resource(NULL, "MODELS_BIN");          /* the models packed in the exe */
    for (i = 0;; i++) {          /* one entry a file: its styles together, its name and real size from the first row */
        const char *st, *nm, *rel; double mm, r = 0, d = 0;
        if (!bsdde_model_row(i, &st, &nm, &rel, &mm)) break;
        for (j = 0; j < ne && strcmp(E[j].rel, rel); j++) {}
        if (j == ne) {
            if (ne == cap) { cap = cap ? 2 * cap : 512; E = (MvEnt *)realloc(E, sizeof *E * (size_t)cap); }
            memset(&E[ne], 0, sizeof *E); E[ne].rel = rel; E[ne].name = nm[0] == 'm' && nm[1] == '_' ? nm + 2 : nm;
            E[ne].real = sdr_real_of(st, nm, &r, &d) && r > 0 ? r : 0; E[ne].L = mm;          /* mm: classic mm a model unit, for one with no real size */
            ne++;
        }
        if (!strstr(E[j].styles, st) && strlen(E[j].styles) + strlen(st) + 3 < sizeof E[j].styles) { if (E[j].styles[0]) SCAT(E[j].styles, ", "); SCAT(E[j].styles, st); }
    }
    ev("log", "s", "text", "Models preview: rendering the models the kit uses");
    todo = (int *)malloc(sizeof(int) * (size_t)(ne ? ne : 1));
    for (i = 0; i < ne; i++) {          /* 1. here: each model loaded and sized, its mesh made ready when its picture is missing */
        int err = 0; const MdlMesh *VF = mdl_load("models", E[i].rel, &err); double lo[3] = { 1e300, 1e300, 1e300 }, hi[3] = { -1e300, -1e300, -1e300 }, longest, s; char safe[300]; int k, n;
        if (!VF || !VF->nv || !VF->nf) continue;
        for (k = 0; k < VF->nv; k++) for (n = 0; n < 3; n++) { double v = VF->v[3 * k + n]; if (v < lo[n]) lo[n] = v; if (v > hi[n]) hi[n] = v; }
        longest = fmax(hi[0] - lo[0], fmax(hi[1] - lo[1], hi[2] - lo[2])); if (longest <= 0) continue;
        E[i].Lmm = E[i].real > 0 ? E[i].real * 18.0 : E[i].L * longest * 3.0;          /* true size; the old models without a real size: their classic size, made true */
        s = E[i].Lmm / longest; E[i].Hmm = (hi[2] - lo[2]) * s;
        for (k = 0, n = 0; E[i].rel[k] && n < (int)sizeof safe - 1; k++) safe[n++] = (E[i].rel[k] == '/' || E[i].rel[k] == '\\' || E[i].rel[k] == ' ' || E[i].rel[k] == ':') ? '_' : E[i].rel[k];
        safe[n] = 0; if (n > 4 && !strcmp(safe + n - 4, ".obj")) safe[n - 4] = 0; else if (n > 4 && !strcmp(safe + n - 4, ".glb")) safe[n - 4] = 0;
        snprintf(E[i].png, sizeof E[i].png, "%s\\%s_%.0f.png", dir, safe, E[i].Lmm * 10);          /* the size in the name: a size change makes a new picture */
        if (file_exists(E[i].png)) { E[i].ok = 1; kept++; continue; }
        {   Mesh *m = &E[i].m; double g = 0.75 * fmax(fmax(hi[0] - lo[0], hi[1] - lo[1]), (hi[2] - lo[2]) * 1.3) * s + 1.0;          /* a square of ground under it, so the picture takes in its whole height */
            int b0 = VF->nv, t0 = (int)VF->nf; static const int BF[12][3] = { { 0, 2, 1 }, { 0, 3, 2 }, { 4, 5, 6 }, { 4, 6, 7 }, { 0, 1, 5 }, { 0, 5, 4 }, { 1, 2, 6 }, { 1, 6, 5 }, { 2, 3, 7 }, { 2, 7, 6 }, { 3, 0, 4 }, { 3, 4, 7 } };
            m->nv = VF->nv + 8; m->nt = (int)VF->nf + 12; m->v = (double *)malloc(sizeof(double) * 3 * (size_t)m->nv); m->f = (int *)malloc(sizeof(int) * 3 * (size_t)m->nt);
            for (k = 0; k < VF->nv; k++) { m->v[3 * k] = (VF->v[3 * k] - (lo[0] + hi[0]) / 2) * s; m->v[3 * k + 1] = (VF->v[3 * k + 1] - (lo[1] + hi[1]) / 2) * s; m->v[3 * k + 2] = (VF->v[3 * k + 2] - lo[2]) * s; }
            for (k = 0; k < 3 * (int)VF->nf; k++) m->f[k] = (int)VF->f[k];
            for (k = 0; k < 8; k++) { m->v[3 * (b0 + k)] = (k % 4 == 1 || k % 4 == 2) ? g : -g; m->v[3 * (b0 + k) + 1] = (k % 4 >= 2) ? g : -g; m->v[3 * (b0 + k) + 2] = k < 4 ? -1.0 : -0.02; }
            for (k = 0; k < 12; k++) for (n = 0; n < 3; n++) m->f[3 * (t0 + k) + n] = b0 + BF[k][n];
            todo[nt++] = i;
        }
    }
    if (nt) {          /* 2. the missing pictures: every core at once, each on the video card */
        SYSTEM_INFO si; GetSystemInfo(&si);
        memset(&J, 0, sizeof J); J.E = E; J.todo = todo; J.n = nt; J.mode = gpu_mode() ? 1 : 2; InitializeCriticalSection(&J.cs);
        {   HANDLE th[256]; int nth = (int)si.dwNumberOfProcessors, h = 0, q; char tn[16];
            if (J.mode == 1 && nth > 8) nth = 8;          /* the video card: 8 threads draw fastest (measured 11-21: 4 5.2 s, 8 4.8 s, 16 5.6 s, 36 7.9 s); software: every core */
            if (GetEnvironmentVariableA("BBB_MV_THREADS", tn, sizeof tn) && atoi(tn) > 0) nth = atoi(tn);          /* a test: how many threads */
            if (nth > nt) nth = nt; if (nth > 256) nth = 256; if (nth < 1) nth = 1;
            for (q = 0; q < nth; q++) if ((th[h] = CreateThread(NULL, 0, mv_worker, &J, 0, NULL))) h++;
            if (!h) mv_worker(&J);
            for (q = 0; q < h; q += 64) WaitForMultipleObjects((DWORD)(h - q < 64 ? h - q : 64), th + q, TRUE, INFINITE);
            for (q = 0; q < h; q++) CloseHandle(th[q]); }
        DeleteCriticalSection(&J.cs);
        for (i = 0; i < nt; i++) made += E[todo[i]].ok;
    }
    free(todo);
    fi = fopen(tmp, "wb"); if (!fi) { free(E); ev("error", "s", "text", "Models preview: cannot write its index"); return 1; }
    for (i = 0; i < ne; i++) if (E[i].ok) fprintf(fi, "%s\t%s\t%s\t%.0f\t%.0f\t%s\n", E[i].png, E[i].name, E[i].styles, E[i].Lmm, E[i].Hmm, E[i].rel);
    fclose(fi);
    { WCHAR a[MAX_PATH * 3], b[MAX_PATH * 3]; wpath(tmp, a, MAX_PATH * 3); wpath(idx, b, MAX_PATH * 3); MoveFileExW(a, b, MOVEFILE_REPLACE_EXISTING); }
    if (pack && *pack && !mv_pack(pack, E, ne)) { free(E); ev("error", "ss", "text", "Models preview: cannot write the pack", "file", pack); return 1; }
    ev("done", "ii", "made", made, "kept", kept);
    free(E);
    return 0;
}
