/* live.h: what the window shows while bases build (docs\superpowers\specs\2026-11-19-live-build-view-design.md).
   The PC's own counters (cores, video card) and the build's progress notes (out\.live\<seed>_<n>.txt). */
#ifndef LIVE_H
#define LIVE_H
#include <windows.h>
#define LIVE_MAXCORE 256
#define LIVE_MAXTILE 1024
int live_cpu(float *per_core, int cap);          /* busy share 0..1 of each logical core since the last call; returns how many cores (0 = could not read) */
int live_job(HANDLE job, float *busy, int cap, float *cores);          /* our own threads (every process in the window's job, the kit's runs) busy since the last call, busiest first, 0..1 each; *cores = the cores they used in all */
float live_gpu(void);                           /* the video card's 3D engine in use 0..1 (all programs); -1 = not available on this PC */

typedef struct {
    int ok;                                      /* a note was read */
    char stage[16];                              /* layout, field, mesh, checks, picture */
    double t0, stage_t0, now;                    /* seconds (the build's clock, time.time()) */
    int pieces, steps_done, steps_total, threads;
    double ovl, ovs;                             /* oval long x short mm; 0 = round */
    double lay_r;                                /* the layout's radius, mm: what the piece outlines (.shapes) are drawn in */
    double pb[4];                                /* what the plan drawing shows, in the built base's mm: x0, y0, x1, y1 */
    WCHAR plan[MAX_PATH];                        /* the build's drawing from above, made as soon as the layout is done */
    int ntile; float tile[LIVE_MAXTILE][4]; unsigned char tdone[LIVE_MAXTILE];
} LiveNote;
int live_read(const WCHAR *dir, const char *seed, int n, LiveNote *out);          /* 1 = read */
double live_expect(const WCHAR *file, const char *style, const char *size, int tier);          /* seconds a base like this took (median of the last 10); 0 = no history yet */
void live_remember(const WCHAR *file, const char *style, const char *size, int tier, double secs);
#endif
