/* live.c: see live.h. Everything here is read only and cheap: the window calls it a few times a second. */
#include <windows.h>
#include <pdh.h>
#include <pdhmsg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>
#include "live.h"

/* ---------- the cores: NtQuerySystemInformation(SystemProcessorPerformanceInformation), what Task Manager reads ---------- */
typedef LONG(WINAPI *NtQSI)(ULONG, PVOID, ULONG, PULONG);
typedef struct { LARGE_INTEGER idle, kernel, user, dpc, intr; ULONG count; } SPPI;          /* kernel time includes idle time */
static SPPI prevC[LIVE_MAXCORE]; static int haveC;
int live_cpu(float *pc, int cap) {
    static NtQSI q; static int tried; static SPPI cur[LIVE_MAXCORE]; ULONG got = 0; int n, i;
    if (!tried) { HMODULE h = GetModuleHandleW(L"ntdll.dll"); tried = 1; q = h ? (NtQSI)(void *)GetProcAddress(h, "NtQuerySystemInformation") : NULL; }
    if (!q || q(8, cur, sizeof cur, &got) != 0) return 0;
    n = (int)(got / sizeof(SPPI)); if (n > cap) n = cap;
    for (i = 0; i < n; i++) {
        double idle = (double)(cur[i].idle.QuadPart - prevC[i].idle.QuadPart), tot = (double)((cur[i].kernel.QuadPart + cur[i].user.QuadPart) - (prevC[i].kernel.QuadPart + prevC[i].user.QuadPart));
        float b = haveC && tot > 0 ? (float)(1.0 - idle / tot) : 0.0f;
        pc[i] = b < 0 ? 0 : b > 1 ? 1 : b;
    }
    memcpy(prevC, cur, sizeof(SPPI) * n); haveC = 1; return n;
}

/* ---------- our own work: every thread of every process in the window's job (the kit's runs), how busy each is ----------
   NtQuerySystemInformation(SystemProcessInformation) lists each process with its threads' CPU times (x64 layout:
   the process record is 0x100 bytes, its pid at 0x50, then one 0x50-byte record per thread: kernel 0, user 8, tid 48). */
#define LJ_MAXT 4096
static struct { ULONG_PTR tid; LONGLONG t; } ljPrev[LJ_MAXT], ljCur[LJ_MAXT]; static int ljNp; static LONGLONG ljWall;
static int cmpf(const void *a, const void *b) { float x = *(const float *)a, y = *(const float *)b; return x < y ? 1 : x > y ? -1 : 0; }
int live_job(HANDLE job, float *v, int cap, float *cores) {
    static NtQSI q; static int tried; static unsigned char *buf; static ULONG bcap;
    static union { JOBOBJECT_BASIC_PROCESS_ID_LIST l; unsigned char b[sizeof(JOBOBJECT_BASIC_PROCESS_ID_LIST) + 1023 * sizeof(ULONG_PTR)]; } ids;
    ULONG got = 0; FILETIME ft; LONGLONG wall, dw; int nc = 0, n = 0, i, j; ULONG off = 0; double sum = 0;
    *cores = 0;
    if (!tried) { HMODULE h = GetModuleHandleW(L"ntdll.dll"); tried = 1; q = h ? (NtQSI)(void *)GetProcAddress(h, "NtQuerySystemInformation") : NULL; }
    if (!q || !job) return 0;
    memset(&ids, 0, sizeof ids); ids.l.NumberOfAssignedProcesses = 1024;
    if (!QueryInformationJobObject(job, JobObjectBasicProcessIdList, &ids, sizeof ids, NULL) || !ids.l.NumberOfProcessIdsInList) { ljNp = 0; return 0; }
    for (;;) {
        LONG st = buf ? q(5, buf, bcap, &got) : (LONG)0xC0000004L;
        if (st == 0) break;
        if (st != (LONG)0xC0000004L) return 0;          /* anything but "buffer too small": give up this time */
        free(buf); bcap = (got > bcap * 2 ? got : bcap * 2) + 65536; buf = malloc(bcap); if (!buf) { bcap = 0; return 0; }
    }
    GetSystemTimeAsFileTime(&ft); wall = ((LONGLONG)ft.dwHighDateTime << 32) | ft.dwLowDateTime; dw = wall - ljWall;
    for (;;) {
        unsigned char *p = buf + off; ULONG next = *(ULONG *)p, nt = *(ULONG *)(p + 4); ULONG_PTR pid = *(ULONG_PTR *)(p + 0x50); int mine = 0;
        for (i = 0; i < (int)ids.l.NumberOfProcessIdsInList; i++) if (ids.l.ProcessIdList[i] == pid) { mine = 1; break; }
        for (j = 0; mine && j < (int)nt && nc < LJ_MAXT; j++) {
            unsigned char *t = p + 0x100 + (size_t)j * 0x50; LONGLONG tt = *(LONGLONG *)t + *(LONGLONG *)(t + 8); ULONG_PTR tid = *(ULONG_PTR *)(t + 48);
            ljCur[nc].tid = tid; ljCur[nc].t = tt; nc++;
            for (i = 0; i < ljNp; i++) if (ljPrev[i].tid == tid) {          /* seen last time: its share of one core since then */
                float b = dw > 0 ? (float)((double)(tt - ljPrev[i].t) / (double)dw) : 0;
                if (b > 1) b = 1;
                if (b >= 0.03f && n < cap) v[n++] = b;          /* a thread that only waits is not shown */
                if (b > 0) sum += b;
                break;
            }
        }
        if (!next) break;
        off += next;
    }
    memcpy(ljPrev, ljCur, sizeof(ljCur[0]) * nc); ljNp = nc; ljWall = wall;
    qsort(v, n, sizeof(float), cmpf); *cores = (float)sum; return n;
}

/* ---------- the video card: PDH "\GPU Engine(*engtype_3D)\Utilization Percentage", summed over every program ---------- */
typedef PDH_STATUS(WINAPI *POpen)(LPCWSTR, DWORD_PTR, PDH_HQUERY *);
typedef PDH_STATUS(WINAPI *PAdd)(PDH_HQUERY, LPCWSTR, DWORD_PTR, PDH_HCOUNTER *);
typedef PDH_STATUS(WINAPI *PCollect)(PDH_HQUERY);
typedef PDH_STATUS(WINAPI *PArray)(PDH_HCOUNTER, DWORD, LPDWORD, LPDWORD, PPDH_FMT_COUNTERVALUE_ITEM_W);
float live_gpu(void) {
    static int state; static PDH_HQUERY qh; static PDH_HCOUNTER ch; static PCollect collect; static PArray arr; static void *buf; static DWORD cap;
    DWORD size = 0, cnt = 0, i; double sum = 0;
    if (state == 0) {          /* first call: set up; the first sample has nothing to compare with yet */
        HMODULE h = LoadLibraryW(L"pdh.dll"); POpen op; PAdd add;
        state = -1;
        if (!h) return -1;
        op = (POpen)(void *)GetProcAddress(h, "PdhOpenQueryW"); add = (PAdd)(void *)GetProcAddress(h, "PdhAddEnglishCounterW");
        collect = (PCollect)(void *)GetProcAddress(h, "PdhCollectQueryData"); arr = (PArray)(void *)GetProcAddress(h, "PdhGetFormattedCounterArrayW");
        if (!op || !add || !collect || !arr || op(NULL, 0, &qh) != ERROR_SUCCESS) return -1;
        if (add(qh, L"\\GPU Engine(*engtype_3D)\\Utilization Percentage", 0, &ch) != ERROR_SUCCESS) return -1;
        collect(qh); state = 1; return -1;
    }
    if (state < 0) return -1;
    if (collect(qh) != ERROR_SUCCESS) return -1;
    if (arr(ch, PDH_FMT_DOUBLE | PDH_FMT_NOCAP100, &size, &cnt, NULL) != (PDH_STATUS)PDH_MORE_DATA) return cnt ? -1 : 0;          /* no 3D work anywhere: no instances */
    if (size > cap) { free(buf); buf = malloc(size); cap = buf ? size : 0; if (!buf) return -1; }
    if (arr(ch, PDH_FMT_DOUBLE | PDH_FMT_NOCAP100, &size, &cnt, (PPDH_FMT_COUNTERVALUE_ITEM_W)buf) != ERROR_SUCCESS) return -1;
    for (i = 0; i < cnt; i++) { PDH_FMT_COUNTERVALUE_ITEM_W *it = (PDH_FMT_COUNTERVALUE_ITEM_W *)buf + i; if (it->FmtValue.CStatus == PDH_CSTATUS_VALID_DATA || it->FmtValue.CStatus == PDH_CSTATUS_NEW_DATA) sum += it->FmtValue.doubleValue; }
    if (sum > 100) sum = 100;
    return (float)(sum / 100.0);
}

/* ---------- time left: how long earlier builds of the same kind took (out\live_times.txt, one line per base built) ---------- */
static int cmpd(const void *a, const void *b) { double x = *(const double *)a, y = *(const double *)b; return x < y ? -1 : x > y; }
double live_expect(const WCHAR *file, const char *style, const char *size, int tier) {
    FILE *f = _wfopen(file, L"r"); char st[40], sz[24]; int t; double s, same[10], near_[10]; int ns = 0, nn = 0; char line[160];
    if (!f) return 0;
    while (fgets(line, sizeof line, f)) {
        if (sscanf(line, "%39s %23s %d %lf", st, sz, &t, &s) != 4 || strcmp(st, style) || strcmp(sz, size) || s <= 0) continue;
        if (t == tier) { if (ns == 10) memmove(same, same + 1, 9 * sizeof(double)), ns = 9; same[ns++] = s; }          /* the last 10 */
        if (nn == 10) memmove(near_, near_ + 1, 9 * sizeof(double)), nn = 9;
        near_[nn++] = s;
    }
    fclose(f);
    if (ns) { qsort(same, ns, sizeof(double), cmpd); return same[ns / 2]; }          /* the same style, size and detail */
    if (nn >= 2) { qsort(near_, nn, sizeof(double), cmpd); return near_[nn / 2]; }          /* else the same style and size, any detail */
    return 0;
}
void live_remember(const WCHAR *file, const char *style, const char *size, int tier, double secs) {
    FILE *f = _wfopen(file, L"a");
    if (f) { fprintf(f, "%s %s %d %.1f\n", style, size, tier, secs); fclose(f); }
}

/* ---------- the build's progress notes: key=value lines, the note and the exe's .tiles beside it ---------- */
static int readall(const WCHAR *p, char *b, int cap) {
    HANDLE f = CreateFileW(p, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, NULL, OPEN_EXISTING, 0, NULL); DWORD got = 0;
    if (f == INVALID_HANDLE_VALUE) return -1;
    ReadFile(f, b, (DWORD)cap - 1, &got, NULL); CloseHandle(f); b[got] = 0; return (int)got;
}
static const char *val(const char *b, const char *key) {          /* the text after "key=" at the start of a line */
    size_t k = strlen(key); const char *p = b;
    while (p && *p) { if (!strncmp(p, key, k) && p[k] == '=') return p + k + 1; p = strchr(p, '\n'); if (p) p++; }
    return NULL;
}
static void tiles(const char *b, LiveNote *o) {
    const char *t = val(b, "tiles"), *d = val(b, "tdone"); int k = 0;
    if (t) { o->ntile = 0;
        while (*t && *t != '\n' && *t != '\r' && o->ntile < LIVE_MAXTILE) { float *q = o->tile[o->ntile]; if (sscanf(t, "%f,%f,%f,%f", q, q + 1, q + 2, q + 3) == 4) o->ntile++; t = strpbrk(t, ";\n"); if (!t || *t != ';') break; t++; } }
    if (d) for (k = 0; k < o->ntile && (d[k] == '0' || d[k] == '1'); k++) o->tdone[k] = d[k] == '1';
}
int live_read(const WCHAR *dir, const char *seed, int n, LiveNote *o) {
    static char b[96 * 1024]; WCHAR p[MAX_PATH]; const char *v;
    swprintf(p, MAX_PATH, L"%ls\\%hs_%d.txt", dir, seed, n);
    if (readall(p, b, sizeof b) <= 0) return 0;
    memset(o, 0, sizeof *o); o->ok = 1;
    if ((v = val(b, "stage"))) { int i; for (i = 0; i < 15 && v[i] > ' '; i++) o->stage[i] = v[i]; }
    if ((v = val(b, "t0"))) o->t0 = atof(v);
    if ((v = val(b, "stage_t0"))) o->stage_t0 = atof(v);
    if ((v = val(b, "now"))) o->now = atof(v);
    if ((v = val(b, "pieces"))) o->pieces = atoi(v);
    if ((v = val(b, "steps_done"))) o->steps_done = atoi(v);
    if ((v = val(b, "steps_total"))) o->steps_total = atoi(v);
    if ((v = val(b, "threads"))) o->threads = atoi(v);
    if ((v = val(b, "plan_box"))) sscanf(v, "%lf,%lf,%lf,%lf", &o->pb[0], &o->pb[1], &o->pb[2], &o->pb[3]);
    if ((v = val(b, "lay_r"))) o->lay_r = atof(v);
    if ((v = val(b, "oval"))) sscanf(v, "%lfx%lf", &o->ovl, &o->ovs);
    if ((v = val(b, "plan"))) { int i; for (i = 0; i < MAX_PATH - 1 && v[i] && v[i] != '\n' && v[i] != '\r'; i++) o->plan[i] = (unsigned char)v[i]; o->plan[i] = 0; }
    tiles(b, o);
    swprintf(p, MAX_PATH, L"%ls\\%hs_%d.txt.tiles", dir, seed, n);
    if (readall(p, b, sizeof b) > 0) {
        const char *s = val(b, "steps_done"); tiles(b, o);
        if ((s = val(b, "exe_stage")) && !strncmp(s, "mesh", 4) && !strcmp(o->stage, "field")) strcpy(o->stage, "mesh");          /* the exe has the field done and is meshing */
        s = val(b, "steps_done");
        if (s && atoi(s) > o->steps_done) o->steps_done = atoi(s);
        if ((s = val(b, "steps_total")) && atoi(s)) o->steps_total = atoi(s);
        if ((s = val(b, "threads")) && atoi(s)) o->threads = atoi(s);
    }
    return 1;
}
