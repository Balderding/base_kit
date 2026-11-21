/* models_job.c: bbb_kit.exe --job models (2026-11-20), the window's Models preview. Every model file the kit uses (the model
   table, bld_sd_models.c), rendered once by the kit's own picture renderer (the same shot as a built base's), at its real size
   at true scale (18 mm a metre: 1:56 beside a 32 mm man), into <out>\model_previews\<file>.png. index.tsv beside them, a line a
   model as each is ready: picture, name, styles, length mm, height mm, file. A picture already made is kept (made again only
   when its model's size line changes). Included by runner.c (it uses shot_png and the runner's helpers). */
int bsdde_model_row(int i, const char **style, const char **name, const char **rel, double *mm);          /* bld_sd_models.c (bld_sdde.h clashes with the runner) */
#include "features.h"
#include "models.h"
typedef struct { const char *rel, *name; char styles[400]; double L, H, real; } MvEnt;
static int job_models(void) {
    char dir[MAX_PATH * 3], idx[MAX_PATH * 3], tmp[MAX_PATH * 3]; MvEnt *E = NULL; int ne = 0, cap = 0, i, j, made = 0, kept = 0; FILE *fi;
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
        if (!strstr(E[j].styles, st) && strlen(E[j].styles) + strlen(st) + 2 < sizeof E[j].styles) { if (E[j].styles[0]) strcat(E[j].styles, ", "); strcat(E[j].styles, st); }
    }
    fi = fopen(tmp, "wb"); if (!fi) { free(E); ev("error", "s", "text", "Models preview: cannot write its index"); return 1; }
    ev("log", "s", "text", "Models preview: rendering the models the kit uses");
    for (i = 0; i < ne; i++) {
        int err = 0; const MdlMesh *VF = mdl_load("models", E[i].rel, &err); double lo[3] = { 1e300, 1e300, 1e300 }, hi[3] = { -1e300, -1e300, -1e300 }, longest, s, Lmm; char png[MAX_PATH * 3], safe[300]; int k, n;
        if (!VF || !VF->nv || !VF->nf) continue;
        for (k = 0; k < VF->nv; k++) for (n = 0; n < 3; n++) { double v = VF->v[3 * k + n]; if (v < lo[n]) lo[n] = v; if (v > hi[n]) hi[n] = v; }
        longest = fmax(hi[0] - lo[0], fmax(hi[1] - lo[1], hi[2] - lo[2])); if (longest <= 0) continue;
        Lmm = E[i].real > 0 ? E[i].real * 18.0 : E[i].L * longest * 3.0;          /* true size; the old models without a real size: their classic size, made true */
        s = Lmm / longest;
        for (k = 0, n = 0; E[i].rel[k] && n < (int)sizeof safe - 1; k++) safe[n++] = (E[i].rel[k] == '/' || E[i].rel[k] == '\\' || E[i].rel[k] == ' ' || E[i].rel[k] == ':') ? '_' : E[i].rel[k];
        safe[n] = 0; if (n > 4 && !strcmp(safe + n - 4, ".obj")) safe[n - 4] = 0; else if (n > 4 && !strcmp(safe + n - 4, ".glb")) safe[n - 4] = 0;
        snprintf(png, sizeof png, "%s\\%s_%.0f.png", dir, safe, Lmm * 10);          /* the size in the name: a size change makes a new picture */
        if (!file_exists(png)) {
            Mesh m; char by[200];
            double g = 0.75 * fmax(fmax(hi[0] - lo[0], hi[1] - lo[1]), (hi[2] - lo[2]) * 1.3) * s + 1.0;          /* a square of ground under it, so the picture takes in its whole height */
            m.nv = VF->nv + 8; m.nt = (int)VF->nf + 12; m.v = (double *)malloc(sizeof(double) * 3 * (size_t)m.nv); m.f = (int *)malloc(sizeof(int) * 3 * (size_t)m.nt);
            for (k = 0; k < VF->nv; k++) { m.v[3 * k] = (VF->v[3 * k] - (lo[0] + hi[0]) / 2) * s; m.v[3 * k + 1] = (VF->v[3 * k + 1] - (lo[1] + hi[1]) / 2) * s; m.v[3 * k + 2] = (VF->v[3 * k + 2] - lo[2]) * s; }
            for (k = 0; k < 3 * (int)VF->nf; k++) m.f[k] = (int)VF->f[k];
            {   int b0 = VF->nv, t0 = (int)VF->nf; static const int BF[12][3] = { { 0, 2, 1 }, { 0, 3, 2 }, { 4, 5, 6 }, { 4, 6, 7 }, { 0, 1, 5 }, { 0, 5, 4 }, { 1, 2, 6 }, { 1, 6, 5 }, { 2, 3, 7 }, { 2, 7, 6 }, { 3, 0, 4 }, { 3, 4, 7 } };
                for (k = 0; k < 8; k++) { m.v[3 * (b0 + k)] = (k % 4 == 1 || k % 4 == 2) ? g : -g; m.v[3 * (b0 + k) + 1] = (k % 4 >= 2) ? g : -g; m.v[3 * (b0 + k) + 2] = k < 4 ? -1.0 : -0.02; }
                for (k = 0; k < 12; k++) for (n = 0; n < 3; n++) m.f[3 * (t0 + k) + n] = b0 + BF[k][n]; }
            if (shot_png(&m, png, 320, 320, 30, -60, by, sizeof by)) made++;
            free(m.v); free(m.f);
        } else kept++;
        if (file_exists(png)) { fprintf(fi, "%s\t%s\t%s\t%.0f\t%.0f\t%s\n", png, E[i].name, E[i].styles, Lmm, (hi[2] - lo[2]) * s, E[i].rel); fflush(fi); }
        if ((i + 1) % 25 == 0) ev("progress", "ii", "done", i + 1, "of", ne);
    }
    fclose(fi);
    { WCHAR a[MAX_PATH * 3], b[MAX_PATH * 3]; wpath(tmp, a, MAX_PATH * 3); wpath(idx, b, MAX_PATH * 3); MoveFileExW(a, b, MOVEFILE_REPLACE_EXISTING); }
    ev("done", "ii", "made", made, "kept", kept);
    free(E);
    return 0;
}
