/* livetiles.c: see livetiles.h */
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "livetiles.h"

static char path[MAX_PATH + 16];
static double (*box)[4];
static volatile unsigned char *done;
static int nt, total, thr, on;
static volatile LONG steps, stop;
static const char *volatile stage = "field";
static HANDLE th;

static void write_now(void) {
    size_t cap = 256 + (size_t)nt * 64, n = 0; char *b = malloc(cap), tmp[MAX_PATH + 24]; FILE *f;
    if (!b) return;
    n += snprintf(b + n, cap - n, "exe_stage=%s\nsteps_done=%ld\nsteps_total=%d\nthreads=%d\ntiles=", stage, (long)steps, total, thr);
    for (int k = 0; k < nt && n + 64 < cap; k++) n += snprintf(b + n, cap - n, "%s%.3f,%.3f,%.3f,%.3f", k ? ";" : "", box[k][0], box[k][1], box[k][2], box[k][3]);
    n += snprintf(b + n, cap - n, "\ntdone=");
    for (int k = 0; k < nt && n + 4 < cap; k++) b[n++] = done[k] ? '1' : '0';
    b[n++] = '\n';
    snprintf(tmp, sizeof tmp, "%s.tmp", path);
    if ((f = fopen(tmp, "wb"))) { fwrite(b, 1, n, f); fclose(f); MoveFileExA(tmp, path, MOVEFILE_REPLACE_EXISTING); }
    free(b);
}
static DWORD WINAPI writer(LPVOID p) { (void)p; while (!stop) { Sleep(100); if (!stop) write_now(); } return 0; }

void lt_begin(int ntiles, const double (*boxes)[4], int steps_total, int threads) {
    char e[MAX_PATH];
    if (!GetEnvironmentVariableA("KIT_LIVE_FILE", e, sizeof e) || !e[0] || ntiles <= 0) return;
    snprintf(path, sizeof path, "%s.tiles", e);
    box = malloc(sizeof(double) * 4 * ntiles); done = calloc(ntiles, 1);
    if (!box || !done) return;
    memcpy(box, boxes, sizeof(double) * 4 * ntiles); nt = ntiles; total = steps_total; thr = threads < 1 ? 1 : threads; on = 1;
    write_now(); th = CreateThread(NULL, 0, writer, NULL, 0, NULL);
}
void lt_tile_done(int k) { if (on && k >= 0 && k < nt) done[k] = 1; }
void lt_steps(int d) { if (on) InterlockedExchange(&steps, d); }
void lt_stage(const char *s) { if (on) { stage = s; write_now(); } }
void lt_end(void) {
    if (!on) return;
    stop = 1; if (th) { WaitForSingleObject(th, 1000); CloseHandle(th); }
    write_now(); on = 0;
}
