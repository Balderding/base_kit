/* mt.c: see mt.h. GEOS is safe with threads when each thread uses its own context (GEOS RFC 3). */
#include <windows.h>
#include <stdlib.h>
#include "mt.h"

typedef struct { volatile LONG next; int n; MtFn fn; void *user; } Pool;
static void quiet(const char *m, void *u) { (void)m; (void)u; }

static DWORD WINAPI worker(LPVOID p) {
    Pool *P = p; GEOSContextHandle_t c = G.GEOS_init_r(); G.GEOSContext_setErrorMessageHandler_r(c, quiet, NULL);
    for (;;) { LONG i = InterlockedIncrement(&P->next) - 1; if (i >= P->n) break; P->fn((int)i, c, P->user); }
    G.GEOS_finish_r(c); return 0;
}

int mt_threads(void) {
    char b[16]; if (!GetEnvironmentVariableA("KIT_MESH_THREADS", b, sizeof b)) return 0;
    int n = atoi(b); if (n <= 1) return 0; if (n > 256) n = 256; return n;
}

void mt_run(int nitems, int nthreads, int grain, MtFn fn, void *user) {
    Pool P = { 0, nitems, fn, user }; HANDLE h[256]; int k = 0;
    if (grain > 1 && nthreads > (nitems + grain - 1) / grain) nthreads = (nitems + grain - 1) / grain;          /* only as many threads as the work needs */
    if (nthreads > nitems) nthreads = nitems;
    if (nthreads < 2) { worker(&P); return; }          /* too little work for a second thread: here */
    if (nthreads > 256) nthreads = 256;
    for (int i = 0; i < nthreads; i++) { h[k] = CreateThread(NULL, 0, worker, &P, 0, NULL); if (h[k]) k++; }
    if (!k) { worker(&P); return; }          /* no threads could be made: do it here */
    for (int i = 0; i < k; i += 64) WaitForMultipleObjects((DWORD)(k - i < 64 ? k - i : 64), h + i, TRUE, INFINITE);          /* Windows waits on 64 at most at once */
    for (int i = 0; i < k; i++) CloseHandle(h[i]);
}
