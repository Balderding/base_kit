/* applog.c (2026-11-23, the user: "we need the logging to be right"): the window's own log on disk, included by bbb_kit.c.
   One file a session, out\logs\bbb_kit_<date>_<time>.log, written and flushed line by line (it is all there even after a
   crash), from every thread under one lock. What goes in:
   - the start: version, command line, screen, the video cards Windows has, the settings file;
   - every click or key that does something (act), every job the window starts, every line the job sends back, how it ended;
   - a crash, on any thread: the exception, where (module + offset, for addr2sym.py), the stack, and a minidump beside the log;
   - a hang: a watchdog thread asks the window every second; when it has not answered for 5 s it writes "NOT RESPONDING",
     the message it is stuck in, the last click, the window thread's stack and one minidump; "responding again" when it is back.
   Nothing here changes what the kit builds. */
#include <dbghelp.h>
#include "safestr.h"          /* bounded text copies (2026-11-23) */
#include <psapi.h>
typedef BOOL (WINAPI *PGPMI)(HANDLE, PPROCESS_MEMORY_COUNTERS, DWORD); static PGPMI pGPMI;          /* GetProcessMemoryInfo, loaded at run time */
#include <signal.h>
static volatile LONG alQuit;          /* set at WM_DESTROY: the watchdog ends */
static volatile DWORD alLastLog;          /* the tick of the last line (the heartbeat writes "alive" when 10 s passed without one) */
static CRITICAL_SECTION alLock; static FILE *alF; static WCHAR alDir[MAX_PATH], alPath[MAX_PATH]; static DWORD alT0, alUiTid, alWdTid; static HANDLE alUiThread;
static volatile LONG alMsg, alMsgDepth; static volatile DWORD alMsgTick; static char alLastAct[160];
static void alog(const char *fmt, ...) {
    va_list a; SYSTEMTIME t; if (!alF) return;
    GetLocalTime(&t); EnterCriticalSection(&alLock);
    fprintf(alF, "%02d:%02d:%02d.%03d %s ", t.wHour, t.wMinute, t.wSecond, t.wMilliseconds, GetCurrentThreadId() == alUiTid ? "ui " : GetCurrentThreadId() == alWdTid ? "wd " : "job");
    va_start(a, fmt); vfprintf(alF, fmt, a); va_end(a); fputc('\n', alF); fflush(alF); alLastLog = GetTickCount();
    LeaveCriticalSection(&alLock);
}
static void alogw(const char *what, const WCHAR *w) {          /* a wide line (a job record) as UTF-8 */
    int n; char *s; if (!alF || !w) return;
    n = WideCharToMultiByte(CP_UTF8, 0, w, -1, NULL, 0, NULL, NULL); s = (char *)malloc(n + 1); if (!s) return;
    WideCharToMultiByte(CP_UTF8, 0, w, -1, s, n, NULL, NULL); alog("%s %s", what, s); free(s);
}
/* the exe's own function names, read once at the start from its COFF symbol table (the build keeps it), so a stack in
   the log reads "paint+0x412" and not only an offset */
typedef struct { DWORD rva; char name[48]; } AlSym;
static AlSym *alSym; static int alNSym;
static int al_symcmp(const void *a, const void *b) { DWORD x = ((const AlSym *)a)->rva, y = ((const AlSym *)b)->rva; return x < y ? -1 : x > y; }
static void al_syms(void) {
    WCHAR p[MAX_PATH]; HANDLE f, mp; unsigned char *v; IMAGE_DOS_HEADER *d; IMAGE_NT_HEADERS64 *nt; IMAGE_SECTION_HEADER *sec; DWORD i, ns, sz;
    GetModuleFileNameW(NULL, p, MAX_PATH); f = CreateFileW(p, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_DELETE, NULL, OPEN_EXISTING, 0, NULL); if (f == INVALID_HANDLE_VALUE) return;
    sz = GetFileSize(f, NULL); mp = CreateFileMappingW(f, NULL, PAGE_READONLY, 0, 0, NULL); v = mp ? (unsigned char *)MapViewOfFile(mp, FILE_MAP_READ, 0, 0, 0) : NULL;
    if (v) {
        d = (IMAGE_DOS_HEADER *)v; nt = (IMAGE_NT_HEADERS64 *)(v + d->e_lfanew); sec = IMAGE_FIRST_SECTION(nt); ns = nt->FileHeader.NumberOfSymbols;
        if (nt->FileHeader.PointerToSymbolTable && (unsigned long long)nt->FileHeader.PointerToSymbolTable + (unsigned long long)ns * 18 < sz) {
            unsigned char *st = v + nt->FileHeader.PointerToSymbolTable; const char *strs = (const char *)(st + ns * 18);
            alSym = (AlSym *)malloc(sizeof(AlSym) * (ns + 1));
            for (i = 0; alSym && i < ns; i++) {
                IMAGE_SYMBOL *s = (IMAGE_SYMBOL *)(st + i * 18); short sn = s->SectionNumber;
                if (sn > 0 && sn <= nt->FileHeader.NumberOfSections && ISFCN(s->Type) && (s->StorageClass == IMAGE_SYM_CLASS_EXTERNAL || s->StorageClass == IMAGE_SYM_CLASS_STATIC)) {
                    AlSym *o = &alSym[alNSym++]; o->rva = sec[sn - 1].VirtualAddress + s->Value;
                    if (s->N.Name.Short) { memcpy(o->name, s->N.ShortName, 8); o->name[8] = 0; } else { snprintf(o->name, sizeof o->name, "%s", strs + s->N.Name.Long); }
                }
                i += s->NumberOfAuxSymbols;
            }
            if (alNSym) qsort(alSym, alNSym, sizeof *alSym, al_symcmp);
        }
        UnmapViewOfFile(v);
    }
    if (mp) CloseHandle(mp);
    CloseHandle(f);
}
/* where an address is: the module's name and the offset in it, and in our own exe the function */
static void al_where(DWORD64 a, char *out, int cap) {
    HMODULE m = NULL; char nm[MAX_PATH] = "?", *b;
    if (GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, (LPCSTR)(ULONG_PTR)a, &m) && m) {
        DWORD rva = (DWORD)(a - (DWORD64)(ULONG_PTR)m); GetModuleFileNameA(m, nm, MAX_PATH); b = strrchr(nm, '\\');
        if (m == GetModuleHandleW(NULL) && alNSym) {
            int lo = 0, hi = alNSym - 1, k = -1;
            while (lo <= hi) { int md = (lo + hi) / 2; if (alSym[md].rva <= rva) { k = md; lo = md + 1; } else hi = md - 1; }
            if (k >= 0) { snprintf(out, cap, "%s+0x%lx (%s+0x%lx)", b ? b + 1 : nm, (unsigned long)rva, alSym[k].name, (unsigned long)(rva - alSym[k].rva)); return; }
        }
        snprintf(out, cap, "%s+0x%lx", b ? b + 1 : nm, (unsigned long)rva);
    } else snprintf(out, cap, "0x%llx", (unsigned long long)a);
}
typedef BOOL (WINAPI *PStackWalk64)(DWORD, HANDLE, HANDLE, LPSTACKFRAME64, PVOID, PREAD_PROCESS_MEMORY_ROUTINE64, PFUNCTION_TABLE_ACCESS_ROUTINE64, PGET_MODULE_BASE_ROUTINE64, PTRANSLATE_ADDRESS_ROUTINE64);
typedef BOOL (WINAPI *PMiniDump)(HANDLE, DWORD, HANDLE, MINIDUMP_TYPE, PMINIDUMP_EXCEPTION_INFORMATION, PMINIDUMP_USER_STREAM_INFORMATION, PMINIDUMP_CALLBACK_INFORMATION);
typedef BOOL (WINAPI *PSymInit)(HANDLE, PCSTR, BOOL);
static HMODULE alDbg; static PStackWalk64 pSW; static PMiniDump pMD; static PFUNCTION_TABLE_ACCESS_ROUTINE64 pFTA; static PGET_MODULE_BASE_ROUTINE64 pGMB;
static void al_dbghelp(void) {
    if (alDbg) return;
    alDbg = LoadLibraryW(L"dbghelp.dll"); if (!alDbg) return;
    pSW = (PStackWalk64)(void *)GetProcAddress(alDbg, "StackWalk64"); pMD = (PMiniDump)(void *)GetProcAddress(alDbg, "MiniDumpWriteDump");
    pFTA = (PFUNCTION_TABLE_ACCESS_ROUTINE64)(void *)GetProcAddress(alDbg, "SymFunctionTableAccess64"); pGMB = (PGET_MODULE_BASE_ROUTINE64)(void *)GetProcAddress(alDbg, "SymGetModuleBase64");
    { PSymInit si = (PSymInit)(void *)GetProcAddress(alDbg, "SymInitialize"); if (si) si(GetCurrentProcess(), NULL, TRUE); }
}
static void al_stack(HANDLE thread, CONTEXT *c) {          /* up to 32 frames, one line each */
    STACKFRAME64 f; int i; char w[MAX_PATH + 32];
    if (!pSW || !pFTA || !pGMB) { al_where(c->Rip, w, sizeof w); alog("  at %s (no stack: dbghelp missing)", w); return; }
    memset(&f, 0, sizeof f); f.AddrPC.Offset = c->Rip; f.AddrStack.Offset = c->Rsp; f.AddrFrame.Offset = c->Rbp; f.AddrPC.Mode = f.AddrStack.Mode = f.AddrFrame.Mode = AddrModeFlat;
    for (i = 0; i < 32 && pSW(IMAGE_FILE_MACHINE_AMD64, GetCurrentProcess(), thread, &f, c, NULL, pFTA, pGMB, NULL) && f.AddrPC.Offset; i++) { al_where(f.AddrPC.Offset, w, sizeof w); alog("  #%02d %s", i, w); }
}
static void al_dump(const char *why, EXCEPTION_POINTERS *ep) {          /* a minidump beside the log: every thread's stack and the memory they use */
    WCHAR p[MAX_PATH]; HANDLE f; SYSTEMTIME t; MINIDUMP_EXCEPTION_INFORMATION x;
    if (!pMD || !alDir[0]) return;
    GetLocalTime(&t); swprintf(p, MAX_PATH, L"%ls\\%hs_%04d%02d%02d_%02d%02d%02d.dmp", alDir, why, t.wYear, t.wMonth, t.wDay, t.wHour, t.wMinute, t.wSecond);
    f = CreateFileW(p, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL); if (f == INVALID_HANDLE_VALUE) { alog("  (the dump could not be written: error %lu)", GetLastError()); return; }
    x.ThreadId = GetCurrentThreadId(); x.ExceptionPointers = ep; x.ClientPointers = FALSE;
    if (pMD(GetCurrentProcess(), GetCurrentProcessId(), f, (MINIDUMP_TYPE)(MiniDumpWithThreadInfo | MiniDumpWithIndirectlyReferencedMemory), ep ? &x : NULL, NULL, NULL)) alogw("  dump written:", p);
    else alog("  (the dump failed: error %lu)", GetLastError());
    CloseHandle(f);
}
static LONG WINAPI al_crash(EXCEPTION_POINTERS *ep) {
    static volatile LONG once; char w[MAX_PATH + 32];
    if (InterlockedExchange(&once, 1)) return EXCEPTION_CONTINUE_SEARCH;
    al_dbghelp(); al_where((DWORD64)(ULONG_PTR)ep->ExceptionRecord->ExceptionAddress, w, sizeof w);
    alog("CRASH exception 0x%08lx at %s on the %s thread; last click: %s", ep->ExceptionRecord->ExceptionCode, w, GetCurrentThreadId() == alUiTid ? "window" : "job", alLastAct[0] ? alLastAct : "none");
    if (ep->ExceptionRecord->ExceptionCode == EXCEPTION_ACCESS_VIOLATION && ep->ExceptionRecord->NumberParameters >= 2)
        alog("  it %s address 0x%llx", ep->ExceptionRecord->ExceptionInformation[0] ? "wrote to" : "read from", (unsigned long long)ep->ExceptionRecord->ExceptionInformation[1]);
    { CONTEXT c = *ep->ContextRecord; al_stack(GetCurrentThread(), &c); }
    al_dump("crash", ep); alog("the window closes now; this log and the dump are in %ls", L"out\\logs");
    return EXCEPTION_CONTINUE_SEARCH;
}
/* the paints: how many and how long, summed up every 10 s (in al_watch) */
static volatile LONG alPaints, alPaintMs, alPaintMax;
static void al_paint(DWORD ms) { InterlockedIncrement(&alPaints); InterlockedExchangeAdd(&alPaintMs, (LONG)ms); if ((LONG)ms > alPaintMax) alPaintMax = (LONG)ms; }
static volatile LONG alTimers[12], alInval;          /* WM_TIMER by id (0..10, 11 = other), and redraw requests, for the beat */
static void al_beat(int every30) {          /* every 10 s: the paints if any; every 30 s: the process's resources */
    {   int i; char b[200] = ""; LONG any = 0; for (i = 0; i < 12; i++) { LONG c = InterlockedExchange(&alTimers[i], 0); if (c) { any += c; snprintf(b + strlen(b), sizeof b - strlen(b), " %d:%ld", i, c); } }
        if (any) alog("timers in the last 10 s (id:count):%s", b); }
    LONG n = InterlockedExchange(&alPaints, 0), ms = InterlockedExchange(&alPaintMs, 0), mx = InterlockedExchange(&alPaintMax, 0);
    if (n) alog("paints in the last 10 s: %ld, %ld ms on average, the longest %ld ms", n, ms / n, mx);
    if (every30) { PROCESS_MEMORY_COUNTERS_EX pm; DWORD hc = 0; memset(&pm, 0, sizeof pm); pm.cb = sizeof pm; GetProcessHandleCount(GetCurrentProcess(), &hc);
        if (pGPMI) pGPMI(GetCurrentProcess(), (PROCESS_MEMORY_COUNTERS *)&pm, sizeof pm);
        alog("resources: GDI %lu, USER %lu, handles %lu, memory %.0f MB", GetGuiResources(GetCurrentProcess(), GR_GDIOBJECTS), GetGuiResources(GetCurrentProcess(), GR_USEROBJECTS), hc, pm.PrivateUsage / 1048576.0); }
}
/* a fatal exception seen first-chance: the watchdog writes it (its own stack: a stack overflow leaves the faulting thread
   none), then the crash goes on as before */
static HANDLE alFatalEv, alFatalDone; static volatile LONG alFatalOnce; static EXCEPTION_RECORD alFatalRec; static CONTEXT alFatalCtx; static DWORD alFatalTid; static HANDLE alFatalThread;
static void al_fatal_write(void) {
    char w[MAX_PATH + 32]; EXCEPTION_POINTERS ep = { &alFatalRec, &alFatalCtx }; CONTEXT c = alFatalCtx;
    al_dbghelp(); al_where((DWORD64)(ULONG_PTR)alFatalRec.ExceptionAddress, w, sizeof w);
    alog("CRASH (first chance) exception 0x%08lx%s at %s on the %s thread; last click: %s", alFatalRec.ExceptionCode,
         alFatalRec.ExceptionCode == EXCEPTION_STACK_OVERFLOW ? " (stack overflow)" : alFatalRec.ExceptionCode == 0xC0000374 ? " (heap corruption)" : "",
         w, alFatalTid == alUiTid ? "window" : "job", alLastAct[0] ? alLastAct : "none");
    if (alFatalRec.ExceptionCode == EXCEPTION_ACCESS_VIOLATION && alFatalRec.NumberParameters >= 2)
        alog("  it %s address 0x%llx", alFatalRec.ExceptionInformation[0] ? "wrote to" : "read from", (unsigned long long)alFatalRec.ExceptionInformation[1]);
    al_stack(alFatalThread, &c);
    {   WCHAR pth[MAX_PATH]; HANDLE f; SYSTEMTIME t; MINIDUMP_EXCEPTION_INFORMATION x;
        GetLocalTime(&t); swprintf(pth, MAX_PATH, L"%ls\\crash_%04d%02d%02d_%02d%02d%02d.dmp", alDir, t.wYear, t.wMonth, t.wDay, t.wHour, t.wMinute, t.wSecond);
        if (pMD && (f = CreateFileW(pth, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL)) != INVALID_HANDLE_VALUE) {
            x.ThreadId = alFatalTid; x.ExceptionPointers = &ep; x.ClientPointers = FALSE;
            if (pMD(GetCurrentProcess(), GetCurrentProcessId(), f, (MINIDUMP_TYPE)(MiniDumpWithThreadInfo | MiniDumpWithIndirectlyReferencedMemory), &x, NULL, NULL)) alogw("  dump written:", pth);
            CloseHandle(f);
        }
    }
}
static LONG CALLBACK al_veh(EXCEPTION_POINTERS *ep) {
    DWORD c = ep->ExceptionRecord->ExceptionCode;
    if (c != EXCEPTION_ACCESS_VIOLATION && c != EXCEPTION_STACK_OVERFLOW && c != 0xC0000374 && c != EXCEPTION_ILLEGAL_INSTRUCTION && c != EXCEPTION_INT_DIVIDE_BY_ZERO) return EXCEPTION_CONTINUE_SEARCH;
    if (InterlockedExchange(&alFatalOnce, 1)) return EXCEPTION_CONTINUE_SEARCH;
    alFatalRec = *ep->ExceptionRecord; alFatalCtx = *ep->ContextRecord; alFatalTid = GetCurrentThreadId();
    DuplicateHandle(GetCurrentProcess(), GetCurrentThread(), GetCurrentProcess(), &alFatalThread, THREAD_GET_CONTEXT | THREAD_QUERY_INFORMATION, FALSE, 0);
    if (alFatalEv && alFatalDone && GetCurrentThreadId() != alWdTid) { SetEvent(alFatalEv); WaitForSingleObject(alFatalDone, 8000); }
    return EXCEPTION_CONTINUE_SEARCH;          /* the crash goes on: the filter, Windows */
}
static void al_abort(int sig) {          /* abort(): a C++ terminate or a failed check in a library */
    (void)sig; RtlCaptureContext(&alFatalCtx); memset(&alFatalRec, 0, sizeof alFatalRec); alFatalRec.ExceptionCode = 0x40000015; alFatalRec.ExceptionAddress = (void *)(ULONG_PTR)alFatalCtx.Rip;
    alog("CRASH: abort() was called (a library gave up); last click: %s", alLastAct[0] ? alLastAct : "none");
    alFatalTid = GetCurrentThreadId(); DuplicateHandle(GetCurrentProcess(), GetCurrentThread(), GetCurrentProcess(), &alFatalThread, THREAD_GET_CONTEXT | THREAD_QUERY_INFORMATION, FALSE, 0);
    if (!InterlockedExchange(&alFatalOnce, 1) && alFatalEv && alFatalDone) { SetEvent(alFatalEv); WaitForSingleObject(alFatalDone, 8000); }
    _exit(3);
}
static void al_paintfail(const char *what, int w, int h, DWORD err) {          /* a paint that could not make its back buffer: at most once a second */
    static DWORD last; DWORD now = GetTickCount(); if (now - last < 1000) return; last = now;
    alog("PAINT FAILED: %s for %dx%d, error %lu; GDI objects %lu, USER objects %lu", what, w, h, err, GetGuiResources(GetCurrentProcess(), GR_GDIOBJECTS), GetGuiResources(GetCurrentProcess(), GR_USEROBJECTS));
}
static volatile LONG *alJobPend; static volatile int *alJobMode; static volatile DWORD *alJobCode, *alJobTick;          /* bbb_kit.c's job end (set by al_job_watch) */
static void al_job_watch(volatile LONG *pend, volatile int *mode, volatile DWORD *code, volatile DWORD *tick) { alJobPend = pend; alJobMode = mode; alJobCode = code; alJobTick = tick; }
static VOID WINAPI al_wd_exit(PVOID v) {          /* runs when the watchdog thread ends (it never should): who ended it */
    void *fr[32]; USHORT n, i; char w[MAX_PATH + 32]; (void)v;
    alog("WATCHDOG THREAD ENDING (it should run until the window closes); its stack:");
    n = RtlCaptureStackBackTrace(0, 32, fr, NULL);
    for (i = 0; i < n; i++) { al_where((DWORD64)(ULONG_PTR)fr[i], w, sizeof w); alog("  #%02u %s", (unsigned)i, w); }
}
static DWORD WINAPI al_watch(LPVOID p) {          /* the hang watchdog */
    DWORD stuck = 0, dumped = 0, ticks = 0; (void)p; alWdTid = GetCurrentThreadId();
    alog("watchdog: started");
    { DWORD fi = FlsAlloc(al_wd_exit); if (fi != FLS_OUT_OF_INDEXES) FlsSetValue(fi, (PVOID)1); }          /* its end is logged, whatever ends it */
    for (;;) {
        DWORD_PTR r;
        if (alFatalEv && WaitForSingleObject(alFatalEv, 1000) == WAIT_OBJECT_0) { al_fatal_write(); SetEvent(alFatalDone); continue; }          /* a fatal exception on another thread */
        if (!alFatalEv) Sleep(1000);
        if (++ticks % 10 == 0) { al_beat(ticks % 30 == 0); if (GetTickCount() - alLastLog >= 9500) alog("alive"); }
        if (alJobPend && *alJobPend && hMain && GetTickCount() - *alJobTick > 2000) {          /* the job ended but the window has not handled it */
            static DWORD said; if (*alJobTick != said) { said = *alJobTick; alog("the window has not handled the job's end after %lu ms: posting it again (queue full or lost?)", GetTickCount() - *alJobTick);
                if (!PostMessageW(hMain, WM_ENDED, (WPARAM)*alJobMode, (LPARAM)*alJobCode)) alog("  PostMessage(WM_ENDED) failed again: error %lu", GetLastError()); } }
        if (alQuit) { alog("watchdog: the window closed, the watchdog stops"); return 0; }          /* the only way out: WM_DESTROY sets alQuit */
        {   HWND hw = *(volatile HWND *)&hMain; DWORD e1, pid = 0, tid;          /* read fresh every time */
            if (!hw) continue;
            SetLastError(0);
            if (SendMessageTimeoutW(hw, WM_NULL, 0, 0, SMTO_NORMAL, 1000, &r)) { if (stuck >= 5) alog("responding again after %lu s", stuck); stuck = 0; dumped = 0; continue; }
            e1 = GetLastError(); tid = GetWindowThreadProcessId(hw, &pid);
            if (!IsWindow(hw)) {          /* never a reason to stop: say what Windows says, and keep watching */
                static DWORD said; if (GetTickCount() - said > 10000) { said = GetTickCount();
                    alog("watchdog: SendMessageTimeout failed (error %lu) and IsWindow(%p) says no window; GetWindowThreadProcessId -> thread %lu pid %lu (ours: window thread %lu, pid %lu); going on", e1, (void *)hw, tid, pid, alUiTid, GetCurrentProcessId()); }
                continue;
            }
            if (e1 != ERROR_TIMEOUT && e1 != 0) { static DWORD said2; if (GetTickCount() - said2 > 10000) { said2 = GetTickCount(); alog("watchdog: SendMessageTimeout failed with error %lu (not a timeout)", e1); } }
        }
        stuck += 2;
        if (stuck >= 5 && !dumped) {
            CONTEXT c; dumped = 1; al_dbghelp();
            if (alMsgDepth > 0) alog("NOT RESPONDING for %lu s: the window thread is in message 0x%04lx for %lu ms; last click: %s", stuck, (unsigned long)alMsg, GetTickCount() - alMsgTick, alLastAct[0] ? alLastAct : "none");
            else alog("NOT RESPONDING for %lu s: the window thread is busy outside any message; last click: %s", stuck, alLastAct[0] ? alLastAct : "none");
            if (alUiThread && SuspendThread(alUiThread) != (DWORD)-1) {          /* only the registers while it is stopped; resumed at once */
                int got; memset(&c, 0, sizeof c); c.ContextFlags = CONTEXT_FULL;
                got = GetThreadContext(alUiThread, &c);
                ResumeThread(alUiThread);          /* before the stack walk: dbghelp takes locks the stopped thread may hold (never leave it stopped) */
                if (got) { alog("  the window thread's stack (walked after it was resumed):"); al_stack(alUiThread, &c); }
            }
            al_dump("hang", NULL);
        } else if (stuck >= 5 && stuck % 30 == 0) alog("still not responding (%lu s)", stuck);
    }
}
static void al_settings(void) {          /* the settings file, as it is at the start */
    WCHAR p[MAX_PATH]; FILE *f; char l[300];
    swprintf(p, MAX_PATH, L"%ls\\..\\bbb_kit.ini", alDir); f = _wfopen(p, L"r"); if (!f) { alog("settings: no bbb_kit.ini yet"); return; }
    { char all[4000] = ""; while (fgets(l, sizeof l, f)) { size_t n = strlen(l); while (n && (l[n - 1] == '\n' || l[n - 1] == '\r')) l[--n] = 0; if (l[0] == '[') continue; if (strlen(all) + n + 2 < sizeof all) { SCAT(all, l); SCAT(all, " "); } } alog("settings: %s", all); }
    fclose(f);
}
static void al_open(const WCHAR *outDir, const char *cmdline) {          /* out\logs\bbb_kit_<stamp>.log; called once, early in WinMain */
    SYSTEMTIME t; int i; DISPLAY_DEVICEW dd; char ver[32] = "?"; WCHAR vp[MAX_PATH]; FILE *vf;
    InitializeCriticalSection(&alLock); alUiTid = GetCurrentThreadId(); alT0 = GetTickCount();
    DuplicateHandle(GetCurrentProcess(), GetCurrentThread(), GetCurrentProcess(), &alUiThread, THREAD_SUSPEND_RESUME | THREAD_GET_CONTEXT | THREAD_QUERY_INFORMATION, FALSE, 0);
    swprintf(alDir, MAX_PATH, L"%ls\\logs", outDir); CreateDirectoryW(outDir, NULL); CreateDirectoryW(alDir, NULL);
    GetLocalTime(&t); swprintf(alPath, MAX_PATH, L"%ls\\bbb_kit_%04d%02d%02d_%02d%02d%02d.log", alDir, t.wYear, t.wMonth, t.wDay, t.wHour, t.wMinute, t.wSecond);
    alF = _wfopen(alPath, L"w");
    SetUnhandledExceptionFilter(al_crash);
    alFatalEv = CreateEventW(NULL, FALSE, FALSE, NULL); alFatalDone = CreateEventW(NULL, FALSE, FALSE, NULL);
    AddVectoredExceptionHandler(1, al_veh);          /* first, before any other handler: crashes the filter would miss */
    signal(SIGABRT, al_abort);
    { HMODULE k = GetModuleHandleW(L"kernel32.dll"); pGPMI = k ? (PGPMI)(void *)GetProcAddress(k, "K32GetProcessMemoryInfo") : NULL; }
#ifdef KIT_VERSION
    snprintf(ver, sizeof ver, "%s", KIT_VERSION); (void)vp; (void)vf;
#else
    swprintf(vp, MAX_PATH, L"%ls\\..\\VERSION", outDir); vf = _wfopen(vp, L"r"); if (vf) { if (fgets(ver, sizeof ver, vf)) ver[strcspn(ver, "\r\n")] = 0; fclose(vf); }
#endif
    al_syms();
#ifndef KIT_COMMIT
#define KIT_COMMIT "?"
#endif
    alog("bbb_kit %s (commit %s) started, pid %lu, command line: %s", ver, KIT_COMMIT, GetCurrentProcessId(), cmdline && *cmdline ? cmdline : "(none)");
    alog("screen %dx%d, %lu CPU threads", GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN), (unsigned long)GetActiveProcessorCount(ALL_PROCESSOR_GROUPS));
    for (i = 0; ; i++) { memset(&dd, 0, sizeof dd); dd.cb = sizeof dd; if (!EnumDisplayDevicesW(NULL, i, &dd, 0)) break; if (dd.StateFlags & DISPLAY_DEVICE_ATTACHED_TO_DESKTOP) { char n[128]; WideCharToMultiByte(CP_UTF8, 0, dd.DeviceString, -1, n, sizeof n, NULL, NULL); alog("video card: %s%s", n, dd.StateFlags & DISPLAY_DEVICE_PRIMARY_DEVICE ? " (main screen)" : ""); } }
    al_settings();
    CreateThread(NULL, 0, al_watch, NULL, 0, NULL);
}
