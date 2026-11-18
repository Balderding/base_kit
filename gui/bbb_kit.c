/* bbb_kit.exe: the window in front of the kit. Plain C, Win32 + GDI+ (both part of Windows), nothing to install.

   What it does: you choose style, size, detail and miniature on the left and press "Show choices". The kit draws that
   many candidate bases as small pictures. You click the ones you want and press Build: the kit builds exactly those,
   puts each through the print check, and the card shows the finished base. "More choices" adds another batch and keeps
   what you picked.

   How it talks to Python: it starts  python tools\gui_job.py choices|build ...  hidden, and reads its output. Every
   line is one JSON record with an "ev" field (choice, start, retry, built, done, log ...), so nothing is guessed from
   printed text. Python and everything it starts sit in a job object: Stop, or closing the window, ends them all.

   Drawing: the whole window is painted by this file into one off-screen picture (no flicker), shapes and pictures
   with GDI+ (smooth edges), text with GDI (ClearType). No standard controls, so it looks the same everywhere and
   follows the monitor's scaling (per-monitor DPI). Colours: dark ground, teal for "yours / done", gold for "look here".

   Where things are: the exe sits in the kit folder beside run_set.py; or bbb_kit_folder.txt beside the exe (one line)
   or the BBB_KIT variable names the kit folder. Python: BBB_PYTHON if set, else C:\Python315\python.exe, else PATH.
   Keys: Enter = the main button, Esc = stop, Ctrl+A = pick all, D = details, ? or F1 = the help page.
   Build: gui\build.cmd       Tests (no clicks, window off screen, each writes pictures and a .txt of what happened):
   --selftest <p.bmp> one pick then two;  --quicktest <p.bmp> build 2 straight away;  --resizetest <p.bmp> sizes, number
   box, help page;  --demotest <p> the README pictures (8 graveyard designs, 5 picked and built). */
#define UNICODE
#define _UNICODE
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <windowsx.h>
#include <shellapi.h>
#include <objbase.h>
#include <gdiplus.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>
#include <math.h>

#define WM_LINE (WM_APP + 1)
#define WM_ENDED (WM_APP + 2)
#define C_BG 0xFF15171Cu
#define C_SIDE 0xFF1B1E25u
#define C_CARD 0xFF252932u
#define C_CARDH 0xFF2D323Du
#define C_LINE 0xFF373C49u
#define C_TEXT 0xFFE9EBEFu
#define C_DIM 0xFF98A1B0u
#define C_ACC 0xFF1FB8A6u
#define C_ACCH 0xFF3ACFBDu
#define C_ONACC 0xFF06211Du
#define C_GOLD 0xFFE2B33Cu
#define C_ONGOLD 0xFF14213Du
enum { SIDE_W = 300, HEAD_H = 74, FOOT_H = 66, MAXC = 80, MAXLOG = 300 };
enum { ST_PENDING, ST_NONE, ST_BUILDING, ST_OK, ST_VARIANT, ST_FAIL };
enum { H_NONE, H_STYLE, H_SIZE, H_DETAIL, H_MINI, H_COUNT, H_SUP, H_SHOW, H_MORE, H_ALL, H_CLEAR, H_GO, H_STOP, H_OPEN, H_DETAILS, H_CARD, H_HELP, H_QUICK, H_SCALE };

typedef struct { char seed[16]; int n, tier, picked, state, retry; GpImage *img, *img3; double secs; WCHAR theme[64], why[220], stl[MAX_PATH]; } Cell;
typedef struct { RECT r; int kind, idx; } Hit;
static Cell cells[MAXC]; static int ncell;
static Hit hits[400]; static int nhit, hovK, hovI = -1;
static WCHAR *logv[MAXLOG]; static int nlog;
static HWND hMain; static double SC = 1.0; static HFONT fTitle, fLabel, fUI, fSmall, fBig, fBtn, fMono;
static WCHAR kit[MAX_PATH], py[MAX_PATH], outFolder[MAX_PATH], sub[300], head[200];
static char curStyle[32], curSize[16], curMini[16], curDetail[8], curScale[8] = "3"; static int curSup, selScale;
static int selStyle = 6, selSize = 8, selDetail = 4, selMini = 0, selCount = 8, selSup = 0, showDetails = 0;          /* selCount: how many bases, 1 to 20 */
static int busy, wanted, doneN, nOK, nBad, scrollY, scrollMax, anim, selftest, stStep; static WCHAR stShot[MAX_PATH];
static HANDLE hJob;
static int stSize = 8, demoShot; static RECT gridR, savedR; static int savedMax, showHelp, autoBuild; static DWORD lastDigit;

static const char *STYLES[] = { "badlands", "highland", "maahl", "corridor", "infestation", "urban", "mine", "swamp", "cynder", "targa",
                                "trederra", "fortress", "beast", "frontier", "graveyard", "forest", "dungeon", "temple", "coast" };
static const char *SIZES[] = { "20", "25", "28", "28.5", "30", "32", "40", "50", "60", "65", "70", "80", "90", "100", "130", "160" };          /* the common round bases; the kit itself takes any size from 20 to 200 */
static const char *DETAILL[] = { "Mix", "1", "2", "3", "4" }, *DETAILV[] = { "mix", "1", "2", "3", "4" };
static const char *SCALES[] = { "32 mm", "28 mm", "15 mm", "tiny" }, *SCALEV[] = { "3", "2.6", "1.4", "1" };          /* the scenery scale: how many times larger than laid out, for figures of that height */
static const char *MINIS[] = { "default", "biped", "wide", "walker", "tripod", "quad", "many", "scorpion", "crawler", "tracked", "hover", "none", "mix" };


static int S(double x) { return (int)(x * SC + 0.5); }
static void redraw(void) { InvalidateRect(hMain, NULL, FALSE); }
static int npicked(void) { int i, k = 0; for (i = 0; i < ncell; i++) k += cells[i].picked && cells[i].state == ST_NONE; return k; }

/* ---------- the log (Details) ---------- */
static void logw(const WCHAR *t) {
    if (nlog == MAXLOG) { free(logv[0]); memmove(logv, logv + 1, (MAXLOG - 1) * sizeof *logv); nlog--; }
    logv[nlog++] = _wcsdup(t);
}

/* ---------- JSON: one flat record per line ---------- */
static const WCHAR *jfind(const WCHAR *s, const WCHAR *key) {
    WCHAR pat[48]; const WCHAR *p; swprintf(pat, 48, L"\"%ls\":", key); p = wcsstr(s, pat);
    if (!p) return NULL;
    p += wcslen(pat); while (*p == L' ') p++;
    return p;
}
static int jstr(const WCHAR *s, const WCHAR *key, WCHAR *out, int cap) {
    const WCHAR *p = jfind(s, key); int n = 0; out[0] = 0;
    if (!p || *p != L'"') return 0;
    for (p++; *p && *p != L'"' && n < cap - 1; p++) {
        if (*p == L'\\' && p[1]) {
            p++;
            if (*p == L'u' && p[1] && p[2] && p[3] && p[4]) { WCHAR h[5] = { p[1], p[2], p[3], p[4], 0 }; out[n++] = (WCHAR)wcstol(h, NULL, 16); p += 4; }
            else out[n++] = *p == L'n' ? L' ' : *p == L't' ? L' ' : *p;
        } else out[n++] = *p;
    }
    out[n] = 0; return 1;
}
static double jnum(const WCHAR *s, const WCHAR *key) { const WCHAR *p = jfind(s, key); return p ? wcstod(p, NULL) : 0; }
static int jtrue(const WCHAR *s, const WCHAR *key) { const WCHAR *p = jfind(s, key); return p && !wcsncmp(p, L"true", 4); }
static void narrow(const WCHAR *w, char *out, int cap) { int i; for (i = 0; w[i] && i < cap - 1; i++) out[i] = (char)w[i]; out[i] = 0; }

/* ---------- running Python hidden ---------- */
typedef struct { HANDLE rd, proc; int mode; } Run;
static DWORD WINAPI reader(LPVOID p) {
    Run *r = (Run *)p; char *buf = (char *)malloc(8192), *line = (char *)malloc(16384); int ll = 0; DWORD got, code = 0, i;
    while (ReadFile(r->rd, buf, 8192, &got, NULL) && got) {
        for (i = 0; i < got; i++) {
            char c = buf[i];
            if (c == '\n') {
                WCHAR *w; int n;
                if (ll && line[ll - 1] == '\r') ll--;
                line[ll] = 0; n = MultiByteToWideChar(CP_UTF8, 0, line, -1, NULL, 0); w = (WCHAR *)malloc((n + 1) * sizeof(WCHAR));
                MultiByteToWideChar(CP_UTF8, 0, line, -1, w, n); PostMessageW(hMain, WM_LINE, r->mode, (LPARAM)w); ll = 0;
            } else if (ll < 16382) line[ll++] = c;
        }
    }
    WaitForSingleObject(r->proc, 5000); GetExitCodeProcess(r->proc, &code);
    CloseHandle(r->rd); CloseHandle(r->proc); PostMessageW(hMain, WM_ENDED, r->mode, code); free(buf); free(line); free(r); return 0;
}
static int run(const WCHAR *cmd, int mode) {
    SECURITY_ATTRIBUTES sa = { sizeof sa, NULL, TRUE }; HANDLE rd, wr, nul; STARTUPINFOW si; PROCESS_INFORMATION pi; static WCHAR c[2700], b[2800]; Run *r;
    if (!CreatePipe(&rd, &wr, &sa, 1 << 16)) return 0;
    SetHandleInformation(rd, HANDLE_FLAG_INHERIT, 0);
    nul = CreateFileW(L"NUL", GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, &sa, OPEN_EXISTING, 0, NULL);
    memset(&si, 0, sizeof si); si.cb = sizeof si; si.dwFlags = STARTF_USESTDHANDLES; si.hStdOutput = wr; si.hStdError = wr; si.hStdInput = nul;
    wcsncpy(c, cmd, 2699); c[2699] = 0; swprintf(b, 2800, L"> %ls", c); logw(b);
    if (!CreateProcessW(NULL, c, NULL, NULL, TRUE, CREATE_NO_WINDOW | CREATE_SUSPENDED, NULL, kit, &si, &pi)) {
        swprintf(head, 200, L"Python could not be started"); swprintf(sub, 300, L"Windows error %lu. Looked for %ls", GetLastError(), py); logw(sub);
        CloseHandle(rd); CloseHandle(wr); CloseHandle(nul); return 0;
    }
    if (hJob) AssignProcessToJobObject(hJob, pi.hProcess);
    ResumeThread(pi.hThread); CloseHandle(pi.hThread); CloseHandle(wr); CloseHandle(nul);
    r = (Run *)malloc(sizeof *r); r->rd = rd; r->proc = pi.hProcess; r->mode = mode;
    CloseHandle(CreateThread(NULL, 0, reader, r, 0, NULL));
    busy = mode; anim = 0; SetTimer(hMain, 1, 60, NULL); return 1;
}

static void free_cell(Cell *c) { if (c->img) GdipDisposeImage(c->img); if (c->img3) GdipDisposeImage(c->img3); memset(c, 0, sizeof *c); }
static void drop_pending(void) {
    int i, k = 0;
    for (i = 0; i < ncell; i++) { if (cells[i].state == ST_PENDING) continue; if (k != i) cells[k] = cells[i]; k++; }
    for (i = k; i < ncell; i++) memset(&cells[i], 0, sizeof(Cell));
    ncell = k;
}

static void ask_choices(int fresh) {
    WCHAR cmd[1200], extra[80] = L""; int i, cnt = selCount;
    if (busy) return;
    if (fresh || !ncell) {
        for (i = 0; i < ncell; i++) free_cell(&cells[i]);
        ncell = 0; scrollY = 0; outFolder[0] = 0;
        strcpy(curStyle, STYLES[selStyle]); strcpy(curSize, SIZES[selSize]); strcpy(curDetail, DETAILV[selDetail]); strcpy(curMini, selMini ? MINIS[selMini] : ""); curSup = selSup; strcpy(curScale, SCALEV[selScale]);
    }
    if (ncell + cnt > MAXC) cnt = MAXC - ncell;
    if (cnt <= 0) { wcscpy(sub, L"That is as many choices as fit. Build some, or press Show new choices to start over."); redraw(); return; }
    if (curMini[0]) swprintf(extra, 80, L" --mini %hs", curMini);
    if (curSup) wcscat(extra, L" --supports on");
    swprintf(extra + wcslen(extra), 20, L" --scale %hs", curScale);
    swprintf(cmd, 1200, L"\"%ls\" -u tools\\gui_job.py choices %hs --size %hs --detail %hs --count %d%ls", py, curStyle, curSize, curDetail, cnt, extra);
    if (run(cmd, 1)) {
        for (i = 0; i < cnt; i++) { memset(&cells[ncell], 0, sizeof(Cell)); cells[ncell++].state = ST_PENDING; }
        wanted = cnt; doneN = 0; swprintf(head, 200, L"Drawing %d %hs bases to choose from", cnt, curStyle); swprintf(sub, 300, L"%hs mm. They appear as they are ready; this takes about half a minute.", curSize);
    }
    redraw();
}
static void build(void) {
    static WCHAR cmd[2600], picks[1800]; WCHAR extra[80] = L""; int i, k = 0;
    if (busy) return;
    picks[0] = 0;
    for (i = 0; i < ncell; i++) if (cells[i].picked && cells[i].state == ST_NONE) { swprintf(picks + wcslen(picks), 40, L"%ls%hs:%d", k++ ? L"," : L"", cells[i].seed, cells[i].n); }
    if (!k) return;
    if (curMini[0]) swprintf(extra, 80, L" --mini %hs", curMini);
    if (curSup) wcscat(extra, L" --supports on");
    swprintf(extra + wcslen(extra), 20, L" --scale %hs", curScale);
    swprintf(cmd, 2600, L"\"%ls\" -u tools\\gui_job.py build %hs --size %hs --picks %ls%ls", py, curStyle, curSize, picks, extra);
    if (run(cmd, 2)) {
        for (i = 0; i < ncell; i++) if (cells[i].picked && cells[i].state == ST_NONE) { cells[i].state = ST_BUILDING; cells[i].retry = 0; }
        wanted = k; doneN = nOK = nBad = 0; swprintf(head, 200, L"Building %d base%ls", k, k == 1 ? L"" : L"s"); wcscpy(sub, L"Each one is built as an STL and put through the print check.");
    }
    redraw();
}
static Cell *cell_of(const WCHAR *line) {
    WCHAR ws[32]; char s[16]; int n = (int)jnum(line, L"n"), i; jstr(line, L"seed", ws, 32); narrow(ws, s, 16);
    for (i = 0; i < ncell; i++) if (cells[i].n == n && cells[i].state != ST_PENDING && !strcmp(cells[i].seed, s)) return &cells[i];
    return NULL;
}
static void capture(const WCHAR *path);
static void on_line(int mode, const WCHAR *t) {
    WCHAR ev[24], path[MAX_PATH]; Cell *c; int i;
    (void)mode;
    if (t[0] != L'{' || !jstr(t, L"ev", ev, 24)) { if (t[0]) logw(t); return; }          /* not a record: something Python itself said (an error); keep it for Details */
    if (!wcscmp(ev, L"log")) { static WCHAR b[4000]; jstr(t, L"text", b, 4000); logw(b); if (showDetails) redraw(); return; }
    if (!wcscmp(ev, L"error")) { jstr(t, L"text", sub, 300); wcscpy(head, L"That did not work"); logw(sub); redraw(); return; }
    if (!wcscmp(ev, L"folder")) { jstr(t, L"folder", outFolder, MAX_PATH); return; }
    if (!wcscmp(ev, L"fetch")) {          /* the first run: the free model packs are being fetched, say so plainly */
        WCHAR ti[80]; jstr(t, L"title", ti, 80);
        swprintf(head, 200, L"First run: fetching the free model packs (%d of %d)", (int)jnum(t, L"i"), (int)jnum(t, L"n"));
        swprintf(sub, 300, L"%ls. Public-domain (CC0) models the details are made from, about %d MB in all, fetched once. Details shows from where.", ti, (int)jnum(t, L"mb"));
        redraw(); return;
    }
    if (!wcscmp(ev, L"fetched")) { swprintf(head, 200, L"Drawing %d %hs bases to choose from", wanted, curStyle); wcscpy(sub, L"The model packs are here. The choices appear as they are ready."); redraw(); return; }
    if (!wcscmp(ev, L"choice")) {
        for (i = 0; i < ncell && cells[i].state != ST_PENDING; i++) ;
        if (i == ncell) { if (ncell >= MAXC) return; ncell++; }
        c = &cells[i]; memset(c, 0, sizeof *c); c->state = ST_NONE; c->n = (int)jnum(t, L"n"); c->tier = (int)jnum(t, L"tier");
        { WCHAR ws[32]; jstr(t, L"seed", ws, 32); narrow(ws, c->seed, 16); }
        jstr(t, L"theme", c->theme, 64);
        if (jstr(t, L"img", path, MAX_PATH)) GdipLoadImageFromFile(path, &c->img);
        doneN++; swprintf(head, 200, L"Drawing the choices: %d of %d", doneN, wanted); redraw(); return;
    }
    if (!wcscmp(ev, L"skip")) {
        for (i = ncell - 1; i >= 0; i--) if (cells[i].state == ST_PENDING) { memmove(&cells[i], &cells[i + 1], (ncell - i - 1) * sizeof(Cell)); ncell--; break; }
        logw(t); redraw(); return;
    }
    if (!wcscmp(ev, L"retry")) { if ((c = cell_of(t))) c->retry = 1; redraw(); return; }
    if (!wcscmp(ev, L"built") && (c = cell_of(t))) {
        if (jtrue(t, L"ok")) {
            c->state = jtrue(t, L"variant") ? ST_VARIANT : ST_OK; c->secs = jnum(t, L"secs"); jstr(t, L"stl", c->stl, MAX_PATH); nOK++;
            if (jstr(t, L"img", path, MAX_PATH) && path[0]) GdipLoadImageFromFile(path, &c->img3);
            if (jstr(t, L"img2", path, MAX_PATH) && path[0]) { GpImage *d2 = NULL; GdipLoadImageFromFile(path, &d2); if (d2) { if (c->img) GdipDisposeImage(c->img); c->img = d2; } }          /* the drawing of what was really built */
            if (c->state == ST_VARIANT) { WCHAR th[64]; if (jstr(t, L"theme", th, 64) && th[0]) wcscpy(c->theme, th); wcscpy(c->why, L"The picked design failed the print check, so the kit built a fresh variant in its place. Click to show its STL file."); }
        } else { c->state = ST_FAIL; jstr(t, L"why", c->why, 220); nBad++; }
        c->picked = 0; doneN++; swprintf(head, 200, L"Building: %d of %d done", doneN, wanted); redraw();
        if (selftest == 3 && !demoShot && doneN == 2) { WCHAR q[MAX_PATH]; demoShot = 1; anim = 9; swprintf(q, MAX_PATH, L"%ls.2_building.bmp", stShot); capture(q); }
        return;
    }
}
static void capture(const WCHAR *path);
static void on_ended(int mode, DWORD code) {
    int i;
    busy = 0; KillTimer(hMain, 1);
    if (mode == 1) {
        drop_pending();
        if (ncell) { wcscpy(head, L"Click the bases you want"); wcscpy(sub, L"A teal tick marks a picked base. Then press Build. \"More choices\" adds another batch and keeps your picks."); }
        else { wcscpy(head, L"No choices came back"); swprintf(sub, 300, L"Python ended with code %lu. Press Details to see what it said.", code); }
    } else {
        for (i = 0; i < ncell; i++) if (cells[i].state == ST_BUILDING) { cells[i].state = ST_NONE; cells[i].retry = 0; }
        if (nBad) swprintf(head, 200, L"Done: %d built and checked, %d not built", nOK, nBad); else swprintf(head, 200, L"Done: %d built and checked", nOK);
        wcscpy(sub, nOK ? L"Click a built base to see its STL file, or press Open folder. Pick more and build again if you like." : L"Nothing was built. Press Details to see why.");
    }
    redraw();
    if (selftest == 3) {          /* --demotest: the pictures for the README. 8 designs, 5 picked, built; a picture at each stage */
        WCHAR q[MAX_PATH]; static const int want[5] = { 0, 1, 3, 4, 6 };
        if (mode == 1) {
            for (i = 0; i < 5; i++) if (want[i] < ncell) cells[want[i]].picked = 1;
            swprintf(head, 200, L"%d bases picked", npicked()); wcscpy(sub, L"Press Build when you have the ones you want. \"More choices\" adds another batch and keeps your picks.");
            swprintf(q, MAX_PATH, L"%ls.1_picked.bmp", stShot); capture(q); build();
        } else {
            FILE *f; swprintf(q, MAX_PATH, L"%ls.3_built.bmp", stShot); capture(q); showHelp = 1; swprintf(q, MAX_PATH, L"%ls.4_help.bmp", stShot); capture(q);
            swprintf(q, MAX_PATH, L"%ls.txt", stShot); f = _wfopen(q, L"w");
            if (f) { fprintf(f, "choices %d\nbuilt_ok %d\nfailed %d\nfolder %ls\n", ncell, nOK, nBad, outFolder); fclose(f); }
            PostQuitMessage(0);
        }
        return;
    }
    if (mode == 1 && autoBuild) { autoBuild = 0; if (ncell) { for (i = 0; i < ncell; i++) if (cells[i].state == ST_NONE) cells[i].picked = 1; build(); return; } }
    if (selftest) {
        if (mode == 1 && stStep == 0 && ncell >= 3) { stStep = 1; cells[0].picked = 1; build(); }                                   /* ONE pick: must build exactly one */
        else if (mode == 2 && stStep == 1) { FILE *f; WCHAR p[MAX_PATH]; swprintf(p, MAX_PATH, L"%ls.txt", stShot); f = _wfopen(p, L"w");
               if (f) { fprintf(f, "one_pick_ok %d\none_pick_failed %d\n", nOK, nBad); fclose(f); }
               stStep = 2; cells[1].picked = cells[2].picked = 1; build(); }                                                        /* then two more */
        else { FILE *f; WCHAR p[MAX_PATH]; int built = 0;
               for (i = 0; i < ncell; i++) built += cells[i].state == ST_OK || cells[i].state == ST_VARIANT;
               hovK = H_NONE; capture(stShot); swprintf(p, MAX_PATH, L"%ls.txt", stShot); f = _wfopen(p, L"a");
               if (f) { fprintf(f, "choices %d\nbuilt_cells %d\ntwo_pick_ok %d\ntwo_pick_failed %d\nfolder %ls\n", ncell, built, nOK, nBad, outFolder); fclose(f); }
               PostQuitMessage(0); }
    }
}

/* ---------- drawing ---------- */
static GpGraphics *G; static HDC DC;
static void rpath(GpPath **p, double x, double y, double w, double h, double r) {
    double d = r * 2;
    if (d > w) d = w;
    if (d > h) d = h;
    GdipCreatePath(FillModeAlternate, p);
    GdipAddPathArc(*p, (REAL)x, (REAL)y, (REAL)d, (REAL)d, 180, 90); GdipAddPathArc(*p, (REAL)(x + w - d), (REAL)y, (REAL)d, (REAL)d, 270, 90);
    GdipAddPathArc(*p, (REAL)(x + w - d), (REAL)(y + h - d), (REAL)d, (REAL)d, 0, 90); GdipAddPathArc(*p, (REAL)x, (REAL)(y + h - d), (REAL)d, (REAL)d, 90, 90); GdipClosePathFigure(*p);
}
static void rr(double x, double y, double w, double h, double r, ARGB fill, ARGB line, double lw) {
    GpPath *p; rpath(&p, x, y, w, h, r);
    if (fill) { GpSolidFill *b; GdipCreateSolidFill(fill, &b); GdipFillPath(G, (GpBrush *)b, p); GdipDeleteBrush((GpBrush *)b); }
    if (line) { GpPen *pn; GdipCreatePen1(line, (REAL)lw, UnitPixel, &pn); GdipDrawPath(G, pn, p); GdipDeletePen(pn); }
    GdipDeletePath(p);
}
static void fillrect(int x, int y, int w, int h, ARGB c) { GpSolidFill *b; GdipCreateSolidFill(c, &b); GdipFillRectangleI(G, (GpBrush *)b, x, y, w, h); GdipDeleteBrush((GpBrush *)b); }
static void txt(const WCHAR *s, int x, int y, int w, int h, HFONT f, ARGB c, UINT fl) {
    RECT r = { x, y, x + w, y + h }; GdipFlush(G, FlushIntentionSync);
    SelectObject(DC, f); SetTextColor(DC, RGB((c >> 16) & 255, (c >> 8) & 255, c & 255)); DrawTextW(DC, s, -1, &r, fl | DT_NOPREFIX);
}
static void txta(const char *s, int x, int y, int w, int h, HFONT f, ARGB c, UINT fl) { WCHAR b[200]; swprintf(b, 200, L"%hs", s); txt(b, x, y, w, h, f, c, fl); }
static void hit(int x, int y, int w, int h, int kind, int idx) { if (nhit < 400) { SetRect(&hits[nhit].r, x, y, x + w, y + h); hits[nhit].kind = kind; hits[nhit].idx = idx; nhit++; } }
static int hov(int kind, int idx) { return hovK == kind && hovI == idx; }

static void chip(const char *label, int x, int y, int w, int h, int on, int kind, int idx) {
    rr(x, y, w, h, S(6), on ? C_ACC : hov(kind, idx) ? C_CARDH : C_CARD, on ? 0 : C_LINE, 1);
    txta(label, x, y, w, h, fUI, on ? C_ONACC : C_TEXT, DT_CENTER | DT_VCENTER | DT_SINGLELINE); hit(x, y, w, h, kind, idx);
}
static void chips(const WCHAR *label, const char **items, int n, int cols, int sel, int kind, int *y) {
    int x0 = S(16), wAll = S(SIDE_W - 32), gap = S(4), ch = S(24), cw = (wAll - (cols - 1) * gap) / cols, i;
    txt(label, x0, *y, wAll, S(18), fLabel, C_DIM, DT_LEFT | DT_SINGLELINE); *y += S(20);
    for (i = 0; i < n; i++) chip(items[i], x0 + (i % cols) * (cw + gap), *y + (i / cols) * (ch + gap), cw, ch, i == sel, kind, i);
    *y += ((n + cols - 1) / cols) * (ch + gap) + S(6);
}
/* style: 0 quiet, 1 the main button (teal), 2 gold outline, 3 off */
static void button(const WCHAR *label, int x, int y, int w, int h, int style, int kind) {
    int hv = hov(kind, 0) && style != 3;
    if (style == 1) rr(x, y, w, h, S(8), hv ? C_ACCH : C_ACC, 0, 0);
    else if (style == 2) rr(x, y, w, h, S(8), hv ? C_CARDH : C_CARD, C_GOLD, S(2));
    else rr(x, y, w, h, S(8), hv ? C_CARDH : C_CARD, C_LINE, 1);
    txt(label, x, y, w, h, style == 1 ? fBtn : fUI, style == 1 ? C_ONACC : style == 2 ? C_GOLD : style == 3 ? 0xFF5F6775u : C_TEXT, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    if (style != 3) hit(x, y, w, h, kind, 0);
}
static void pill(const WCHAR *s, int x, int y, int w, int h, ARGB bg, ARGB ink) { rr(x, y, w, h, h / 2.0, bg, 0, 0); txt(s, x, y, w, h, fSmall, ink, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS); }
static void image_fit(GpImage *im, double x, double y, double w, double h) {
    UINT iw = 1, ih = 1; double s, dw, dh; GdipGetImageWidth(im, &iw); GdipGetImageHeight(im, &ih);
    s = w / iw < h / ih ? w / iw : h / ih; dw = iw * s; dh = ih * s; GdipDrawImageRect(G, im, (REAL)(x + (w - dw) / 2), (REAL)(y + (h - dh) / 2), (REAL)dw, (REAL)dh);
}
static void card(int i, int x, int y, int w, int h) {
    Cell *c = &cells[i]; int pad = S(10), iw = w - 2 * pad, hv = hov(H_CARD, i) && !busy, capY = y + pad + iw + S(6); WCHAR b[160];
    if (c->state == ST_PENDING) {          /* a grey card that breathes while its picture is being drawn */
        ARGB a = (ARGB)(150 + (int)(70 * (0.5 + 0.5 * sin((anim + i * 3) * 0.18))));
        rr(x, y, w, h, S(12), (a << 24) | (C_CARD & 0xFFFFFF), 0, 0); rr(x + pad, y + pad, iw, iw, iw / 2.0, ((a / 6) << 24) | 0xFFFFFF, 0, 0);
        rr(x + pad, capY + S(4), iw * 0.6, S(10), S(5), 0x18FFFFFF, 0, 0); return;
    }
    rr(x, y, w, h, S(12), hv ? C_CARDH : C_CARD, c->picked ? C_ACC : hv ? 0xFF4A5060u : C_LINE, c->picked ? S(3) : 1);
    if ((c->state == ST_OK || c->state == ST_VARIANT) && c->img3) {          /* built: the 3D picture, with its 2D drawing small in the corner to check one against the other */
        image_fit(c->img3, x + pad, y + pad + iw * 0.12, iw, iw * 0.88);
        if (c->img) { double d = iw * 0.36; rr(x + pad - 1, y + pad - 1, d + 2, d + 2, d / 2 + 1, C_CARD, C_LINE, 1); image_fit(c->img, x + pad, y + pad, d, d); }
    }
    else if (c->img) image_fit(c->img, x + pad, y + pad, iw, iw);
    if (c->state == ST_BUILDING) {          /* dimmed, with a turning arc */
        GpPen *pn; double cx = x + w / 2.0, cy = y + pad + iw / 2.0, r = S(18);
        rr(x + pad, y + pad, iw, iw, iw / 2.0, 0xA0101216, 0, 0);
        GdipCreatePen1(C_ACC, (REAL)S(4), UnitPixel, &pn); GdipSetPenStartCap(pn, LineCapRound); GdipSetPenEndCap(pn, LineCapRound);
        GdipDrawArc(G, pn, (REAL)(cx - r), (REAL)(cy - r), (REAL)(2 * r), (REAL)(2 * r), (REAL)((anim * 14) % 360), 260); GdipDeletePen(pn);
    }
    if (c->picked) {          /* the tick */
        GpSolidFill *bf; GpPen *pn; double cx = x + w - S(22), cy = y + S(22), r = S(12); GpPointF pt[3];
        GdipCreateSolidFill(C_ACC, &bf); GdipFillEllipse(G, (GpBrush *)bf, (REAL)(cx - r), (REAL)(cy - r), (REAL)(2 * r), (REAL)(2 * r)); GdipDeleteBrush((GpBrush *)bf);
        GdipCreatePen1(C_ONACC, (REAL)S(2.6), UnitPixel, &pn); GdipSetPenStartCap(pn, LineCapRound); GdipSetPenEndCap(pn, LineCapRound); GdipSetPenLineJoin(pn, LineJoinRound);
        pt[0].X = (REAL)(cx - r * 0.45); pt[0].Y = (REAL)cy; pt[1].X = (REAL)(cx - r * 0.1); pt[1].Y = (REAL)(cy + r * 0.38); pt[2].X = (REAL)(cx + r * 0.5); pt[2].Y = (REAL)(cy - r * 0.35);
        GdipDrawLines(G, pn, pt, 3); GdipDeletePen(pn);
    }
    txt(c->theme, x + pad, capY, iw, S(18), fUI, C_TEXT, DT_LEFT | DT_SINGLELINE | DT_END_ELLIPSIS);
    switch (c->state) {
    case ST_BUILDING: txt(c->retry ? L"first try failed the check; new variant ..." : L"building ...", x + pad, capY + S(20), iw, S(18), fSmall, C_ACC, DT_LEFT | DT_SINGLELINE | DT_END_ELLIPSIS); break;
    case ST_OK: swprintf(b, 160, L"BUILT  %.0f s", c->secs); pill(b, x + pad, capY + S(20), S(96), S(18), C_ACC, C_ONACC); break;
    case ST_VARIANT: pill(L"built as a new variant", x + pad, capY + S(20), iw < S(160) ? iw : S(160), S(18), C_GOLD, C_ONGOLD); break;
    case ST_FAIL: pill(L"not built: failed the check", x + pad, capY + S(20), iw < S(180) ? iw : S(180), S(18), C_GOLD, C_ONGOLD); break;
    default: swprintf(b, 160, L"detail %d of 4", c->tier); txt(b, x + pad, capY + S(20), iw, S(18), fSmall, C_DIM, DT_LEFT | DT_SINGLELINE);
    }
    hit(x, y, w, h, H_CARD, i);
}
static void help(int W, int H);
static void paint(HDC out, int W, int H) {
    HBITMAP bm; HGDIOBJ obm; int y, i, mx = S(SIDE_W), mw = W - mx, np = npicked(), fy = H - S(FOOT_H); WCHAR b[200];
    DC = CreateCompatibleDC(out); bm = CreateCompatibleBitmap(out, W, H); obm = SelectObject(DC, bm); SetBkMode(DC, TRANSPARENT);
    GdipCreateFromHDC(DC, &G); GdipSetSmoothingMode(G, SmoothingModeAntiAlias); GdipSetInterpolationMode(G, InterpolationModeHighQualityBicubic); GdipSetPixelOffsetMode(G, PixelOffsetModeHalf);
    nhit = 0; fillrect(0, 0, W, H, C_BG); fillrect(0, 0, mx, H, C_SIDE); fillrect(mx, 0, 1, H, C_LINE);

    /* the left side: what to make */
    txt(L"Balderding's Ballin Base Kit", S(16), S(14), S(SIDE_W - 24), S(26), fTitle, C_TEXT, DT_LEFT | DT_SINGLELINE);
    txt(L"Save time and money.", S(16), S(40), S(SIDE_W - 32), S(18), fSmall, C_ACC, DT_LEFT | DT_SINGLELINE);
    y = S(68);
    chips(L"STYLE", STYLES, 19, 3, selStyle, H_STYLE, &y); chips(L"BASE SIZE, MM", SIZES, 16, 6, selSize, H_SIZE, &y);
    chips(L"DETAIL  (1 SIMPLE ... 4 MOST)", DETAILL, 5, 5, selDetail, H_DETAIL, &y); chips(L"THE MINIATURE IT IS FOR", MINIS, 13, 3, selMini, H_MINI, &y);
    chips(L"OBJECTS SIZED FOR FIGURES OF", SCALES, 4, 4, selScale, H_SCALE, &y);
    {          /* how many: a number from 1 to 20. Type it, or use the minus and plus */
        int x0 = S(16), wAll = S(SIDE_W - 32), bh = S(30), bw = S(40), fresh = GetTickCount() - lastDigit < 1500; WCHAR nb[8];
        txt(L"HOW MANY BASES  (1 TO 20)", x0, y, wAll, S(18), fLabel, C_DIM, DT_LEFT | DT_SINGLELINE); y += S(20);
        rr(x0, y, bw, bh, S(6), hov(H_COUNT, -1) ? C_CARDH : C_CARD, C_LINE, 1); txt(L"\x2212", x0, y, bw, bh, fBtn, selCount > 1 ? C_TEXT : 0xFF5F6775u, DT_CENTER | DT_VCENTER | DT_SINGLELINE); hit(x0, y, bw, bh, H_COUNT, -1);
        rr(x0 + wAll - bw, y, bw, bh, S(6), hov(H_COUNT, 1) ? C_CARDH : C_CARD, C_LINE, 1); txt(L"+", x0 + wAll - bw, y, bw, bh, fBtn, selCount < 20 ? C_TEXT : 0xFF5F6775u, DT_CENTER | DT_VCENTER | DT_SINGLELINE); hit(x0 + wAll - bw, y, bw, bh, H_COUNT, 1);
        rr(x0 + bw + S(4), y, wAll - 2 * bw - S(8), bh, S(6), C_BG, fresh ? C_ACCH : C_ACC, fresh ? S(2) : 1);
        swprintf(nb, 8, L"%d", selCount); txt(nb, x0 + bw + S(4), y, wAll - 2 * bw - S(8), bh, fBig, C_TEXT, DT_CENTER | DT_VCENTER | DT_SINGLELINE); hit(x0 + bw + S(4), y, wAll - 2 * bw - S(8), bh, H_COUNT, 0);
        y += bh + S(8);
    }
    chip("Allow overhangs (the print needs supports)", S(16), y, S(SIDE_W - 32), S(24), selSup, H_SUP, 0); y += S(34);
    swprintf(b, 200, ncell ? L"Show %d new choices" : L"Show %d choices", selCount);
    button(b, S(16), y, S(SIDE_W - 32), S(40), busy ? 3 : (!np ? 1 : 0), H_SHOW); y += S(46);
    swprintf(b, 200, selCount == 1 ? L"Or build 1 straight away" : L"Or build %d straight away", selCount);
    button(b, S(16), y, S(SIDE_W - 32), S(30), busy ? 3 : 0, H_QUICK);

    /* the top: what is happening */
    txt(head, mx + S(22), S(14), mw - S(44) - S(110), S(28), fBig, C_TEXT, DT_LEFT | DT_SINGLELINE | DT_END_ELLIPSIS);
    txt(sub, mx + S(22), S(42), mw - S(44) - S(110), S(20), fSmall, C_DIM, DT_LEFT | DT_SINGLELINE | DT_END_ELLIPSIS);
    button(L"Help   ?", W - S(22) - S(96), S(18), S(96), S(34), 0, H_HELP);
    fillrect(mx + 1, S(HEAD_H) - S(3), mw, S(3), C_LINE);
    if (busy && wanted) { double f = (doneN + 0.15) / (double)wanted; if (f > 1) f = 1; fillrect(mx + 1, S(HEAD_H) - S(3), (int)(mw * f), S(3), C_ACC); }

    /* the middle: the choices */
    SetRect(&gridR, mx + 1, S(HEAD_H), W, fy - (showDetails ? S(170) : 0));
    {
        int pad = S(18), gap = S(14), aw = gridR.right - gridR.left - 2 * pad - S(8), cols = (aw + gap) / (S(196) + gap), cw, ch, rows, total, vh = gridR.bottom - gridR.top;
        if (cols < 1) cols = 1;
        cw = (aw - (cols - 1) * gap) / cols;
        if (cw > S(280)) cw = S(280);
        ch = cw + S(40); rows = (ncell + cols - 1) / cols; total = rows * (ch + gap) + 2 * pad - gap; scrollMax = total > vh ? total - vh : 0;
        if (scrollY > scrollMax) scrollY = scrollMax;
        if (scrollY < 0) scrollY = 0;
        GdipSetClipRectI(G, gridR.left, gridR.top, gridR.right - gridR.left, vh, CombineModeReplace); GdipFlush(G, FlushIntentionSync); IntersectClipRect(DC, gridR.left, gridR.top, gridR.right, gridR.bottom);
        if (!ncell) {
            static const WCHAR *st[3] = { L"Choose a style and a base size on the left, then press Show choices.", L"Click the bases you like. A teal tick marks a picked one.",
                                          L"Press Build. The kit makes the picked bases, checks that each one will print, and saves them as STL files." };
            int cy0 = gridR.top + vh / 2 - S(90);
            for (i = 0; i < 3; i++) {
                WCHAR num[4]; swprintf(num, 4, L"%d", i + 1); rr(gridR.left + S(60), cy0 + i * S(60), S(34), S(34), S(17), C_CARD, C_ACC, S(2));
                txt(num, gridR.left + S(60), cy0 + i * S(60), S(34), S(34), fBtn, C_ACC, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
                txt(st[i], gridR.left + S(110), cy0 + i * S(60) - S(6), gridR.right - gridR.left - S(170), S(46), fBig, C_TEXT, DT_LEFT | DT_WORDBREAK);
            }
        }
        for (i = 0; i < ncell; i++) {
            int cx = gridR.left + pad + (i % cols) * (cw + gap), cy = gridR.top + pad + (i / cols) * (ch + gap) - scrollY;
            if (cy + ch > gridR.top && cy < gridR.bottom) card(i, cx, cy, cw, ch);
        }
        if (scrollMax) { double th = (double)vh * vh / total, ty = gridR.top + (double)scrollY / scrollMax * (vh - th); rr(gridR.right - S(9), ty + S(3), S(5), th - S(6), S(3), 0xFF4A5060u, 0, 0); }
        GdipResetClip(G); GdipFlush(G, FlushIntentionSync); SelectClipRgn(DC, NULL);
    }
    if (showDetails) {          /* what the kit itself says, for when something goes wrong */
        int dy = gridR.bottom, dh = S(170), lh = S(16), k = (dh - S(34)) / lh, j, from = nlog > k ? nlog - k : 0;
        fillrect(mx + 1, dy, mw, dh, C_SIDE); fillrect(mx + 1, dy, mw, 1, C_LINE);
        txt(L"DETAILS: WHAT THE KIT SAYS", mx + S(22), dy + S(8), mw, S(18), fLabel, C_DIM, DT_LEFT | DT_SINGLELINE);
        for (j = from; j < nlog; j++) txt(logv[j], mx + S(22), dy + S(28) + (j - from) * lh, mw - S(44), lh, fMono, 0xFFC4CAD4u, DT_LEFT | DT_SINGLELINE | DT_END_ELLIPSIS);
    }

    /* the bottom: what to do with the picks */
    fillrect(mx + 1, fy, mw, S(FOOT_H), C_SIDE); fillrect(mx + 1, fy, mw, 1, C_LINE);
    {
        int by = fy + S(15), bh = S(36), x = mx + S(22), rx = W - S(22), btnw = S(220);
        if (busy == 2) swprintf(b, 200, L"%d to go", wanted - doneN); else if (np == 1) wcscpy(b, L"1 picked"); else swprintf(b, 200, L"%d picked", np);
        txt(b, x, by, S(84), bh, fBtn, np || busy == 2 ? C_ACC : C_DIM, DT_LEFT | DT_VCENTER | DT_SINGLELINE); x += S(88);
        button(L"Pick all", x, by, S(84), bh, busy || !ncell ? 3 : 0, H_ALL); x += S(92);
        button(L"Clear", x, by, S(70), bh, busy || !np ? 3 : 0, H_CLEAR); x += S(78);
        button(L"More choices", x, by, S(118), bh, busy || !ncell ? 3 : 0, H_MORE);
        if (busy) button(L"Stop", rx - btnw, by - S(3), btnw, bh + S(6), 2, H_STOP);
        else {
            if (np == 1) wcscpy(b, L"Build 1 base"); else if (np) swprintf(b, 200, L"Build %d bases", np); else wcscpy(b, L"Build the picked bases");
            button(b, rx - btnw, by - S(3), btnw, bh + S(6), np ? 1 : 3, H_GO);
        }
        rx -= btnw + S(10); button(L"Open folder", rx - S(110), by, S(110), bh, 0, H_OPEN); rx -= S(118);
        button(showDetails ? L"Hide details" : L"Details", rx - S(104), by, S(104), bh, 0, H_DETAILS);
    }
    if (showHelp) help(W, H);
    GdipFlush(G, FlushIntentionSync); GdipDeleteGraphics(G); BitBlt(out, 0, 0, W, H, DC, 0, 0, SRCCOPY); SelectObject(DC, obm); DeleteObject(bm); DeleteDC(DC);
}
/* the help page: a panel over the window, opened with ? (Shift+/), F1 or the Help button, closed with Esc or a click */
static int para(const WCHAR *title, const WCHAR *body, int x, int y, int w) {
    RECT r = { x, y, x + w, y + 2000 };
    if (title) { txt(title, x, y, w, S(18), fLabel, C_ACC, DT_LEFT | DT_SINGLELINE); y += S(20); r.top = y; }
    GdipFlush(G, FlushIntentionSync); SelectObject(DC, fUI); DrawTextW(DC, body, -1, &r, DT_LEFT | DT_WORDBREAK | DT_NOPREFIX | DT_CALCRECT);
    txt(body, x, y, w, r.bottom - r.top, fUI, C_TEXT, DT_LEFT | DT_WORDBREAK);
    return y + (r.bottom - r.top) + S(12);
}
static void help(int W, int H) {
    int pw = W - S(80) > S(1040) ? S(1040) : W - S(80), ph = H - S(50) > S(700) ? S(700) : H - S(50), px = (W - pw) / 2, py = (H - ph) / 2, cwid = (pw - S(84)) / 2, x1 = px + S(28), x2 = x1 + cwid + S(28), y;
    fillrect(0, 0, W, H, 0xB0080A0Du); rr(px, py, pw, ph, S(14), C_SIDE, C_LINE, 1);
    txt(L"Help", x1, py + S(18), S(300), S(28), fBig, C_TEXT, DT_LEFT | DT_SINGLELINE);
    txt(L"Esc, ? or a click closes this", px, py + S(22), pw - S(28), S(20), fSmall, C_DIM, DT_RIGHT | DT_SINGLELINE);
    GdipFlush(G, FlushIntentionSync); IntersectClipRect(DC, px, py, px + pw, py + ph - S(10));
    y = py + S(58);
    y = para(L"HOW IT WORKS", L"1.  Choose a style and a base size on the left and press Show choices. The kit draws that many designs; nothing is built yet.\n"
                              L"2.  Click the designs you like. A teal tick marks a picked one; click again to un-pick.\n"
                              L"3.  Press Build. Each picked base is made as an STL file and checked that it will print.", x1, y, cwid);
    y = para(L"THE OPTIONS ON THE LEFT", L"Style:  the kind of ground and the things on it (mine, swamp, temple ...).\n"
                              L"Base size:  the width of the round base in millimetres, 20 to 160.\n"
                              L"Detail:  1 is the simplest, 4 has the most on it. Mix gives some of each.\n"
                              L"The miniature it is for:  the kit keeps the spot flat where that kind of model stands. Default takes it from settings.json; none keeps no spot.\n"
                              L"Objects sized for figures of:  how tall your miniatures are. A barrel, a rail track or a campfire is then the right size beside them, the same on every base: a small base holds a few things or a part of one, a large base holds a whole scene. \"tiny\" is the old look, a small landscape about a third of true size. The spot where the miniature stands is always kept flat and on the base.\n"
                              L"How many bases:  1 to 20. Type the number or use the minus and plus. That many designs are drawn each time.\n"
                              L"Allow overhangs:  things may hang over (rounder barrels, arching ribs). The print then needs supports. Off: every base prints without supports.", x1, y, cwid);
    y = para(L"WHERE THE FILES GO", L"The first time, the kit fetches the free model packs its details are made from (public domain, about 90 MB, once). The top line says so while it does, and Details shows from where. \"models_download\": false in settings.json stops it.\n"
                              L"Every base you build is copied into one folder, out\\picked_<date>_<time> in the kit folder, named by style, size and theme: the STL, its 3D picture (_3d.png), its drawing from above (_2d.png) and its plan. Open folder takes you there.", x1, y, cwid);
    y = py + S(58);
    y = para(L"THE CARDS", L"A flat picture from above is a design that is not built yet.\n"
                              L"A grey 3D picture is the finished base, with its drawing small in the corner so you can check one against the other.\n"
                              L"Teal BUILT:  built, and it passed the print check. Click the card to show its STL file.\n"
                              L"Gold \"built as a new variant\":  the design you picked failed the print check, so the kit built a different one in its place.\n"
                              L"Gold \"not built\":  no good base after four tries. Details says why.\n"
                              L"Rest the mouse on a card and the line at the top says what it is.", x2, y, cwid);
    y = para(L"THE BUTTONS", L"Show choices:  throws the designs away and draws new ones with the options on the left.\n"
                              L"Or build straight away:  draws that many designs and builds them all, without stopping for you to pick.\n"
                              L"More choices:  adds another batch of the same style and size and keeps your picks.\n"
                              L"Pick all, Clear:  tick every design, or none.\n"
                              L"Build:  builds the picked ones. While it runs the button is Stop.\n"
                              L"Details:  what the kit itself says, for when something goes wrong.", x2, y, cwid);
    y = para(L"KEYS", L"Enter:  the teal button.      Esc:  stop, or close this page.\n"
                              L"Ctrl+A:  pick all.      D:  details.      ? or F1:  this page.\n"
                              L"0 to 9:  how many bases (1 then 5 is 15).      + and -:  one more, one fewer.\n"
                              L"Mouse wheel:  scroll the designs. Drag the window's edge to make it any size; it opens the same way next time.", x2, y, cwid);
    (void)y; SelectClipRgn(DC, NULL); nhit = 0; hit(0, 0, W, H, H_HELP, 0);
}
static void capture(const WCHAR *path) {
    RECT r; HDC sdc, dc; HBITMAP bm; BITMAPINFO bi; BITMAPFILEHEADER fh; void *bits; int w, h; DWORD n, wr; HANDLE f; HGDIOBJ o;
    GetClientRect(hMain, &r); w = r.right; h = r.bottom; sdc = GetDC(NULL); dc = CreateCompatibleDC(sdc);
    memset(&bi, 0, sizeof bi); bi.bmiHeader.biSize = sizeof bi.bmiHeader; bi.bmiHeader.biWidth = w; bi.bmiHeader.biHeight = h; bi.bmiHeader.biPlanes = 1; bi.bmiHeader.biBitCount = 24;
    bm = CreateDIBSection(sdc, &bi, DIB_RGB_COLORS, &bits, NULL, 0); o = SelectObject(dc, bm); paint(dc, w, h); GdiFlush();
    n = ((w * 3 + 3) & ~3) * h; memset(&fh, 0, sizeof fh); fh.bfType = 0x4D42; fh.bfOffBits = sizeof fh + sizeof bi.bmiHeader; fh.bfSize = fh.bfOffBits + n;
    f = CreateFileW(path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL);
    if (f != INVALID_HANDLE_VALUE) { WriteFile(f, &fh, sizeof fh, &wr, NULL); WriteFile(f, &bi.bmiHeader, sizeof bi.bmiHeader, &wr, NULL); WriteFile(f, bits, n, &wr, NULL); CloseHandle(f); }
    SelectObject(dc, o); DeleteObject(bm); DeleteDC(dc); ReleaseDC(NULL, sdc);
}

/* ---------- acting on clicks and keys ---------- */
static void open_folder(void) {
    WCHAR p[MAX_PATH];
    if (outFolder[0] && GetFileAttributesW(outFolder) != INVALID_FILE_ATTRIBUTES) wcscpy(p, outFolder); else swprintf(p, MAX_PATH, L"%ls\\out", kit);
    if (GetFileAttributesW(p) == INVALID_FILE_ATTRIBUTES) wcscpy(p, kit);
    ShellExecuteW(hMain, L"open", p, NULL, NULL, SW_SHOWNORMAL);
}
static void act(int kind, int idx) {
    int i;
    switch (kind) {
    case H_STYLE: selStyle = idx; break;
    case H_SIZE: selSize = idx; break;
    case H_DETAIL: selDetail = idx; break;
    case H_MINI: selMini = idx; break;
    case H_SCALE: selScale = idx; break;
    case H_COUNT: selCount += idx; if (selCount < 1) selCount = 1; if (selCount > 20) selCount = 20; break;
    case H_QUICK: if (!busy) { autoBuild = 1; ask_choices(1); if (!busy) autoBuild = 0; } return;
    case H_SUP: selSup = !selSup; break;
    case H_SHOW: ask_choices(1); return;
    case H_MORE: ask_choices(0); return;
    case H_GO: build(); return;
    case H_STOP: autoBuild = 0; if (busy && hJob) { TerminateJobObject(hJob, 1); logw(L"Stopped."); wcscpy(sub, L"Stopped."); } break;
    case H_ALL: if (!busy) for (i = 0; i < ncell; i++) if (cells[i].state == ST_NONE) cells[i].picked = 1; break;
    case H_CLEAR: if (!busy) for (i = 0; i < ncell; i++) cells[i].picked = 0; break;
    case H_OPEN: open_folder(); return;
    case H_DETAILS: showDetails = !showDetails; break;
    case H_HELP: showHelp = !showHelp; break;
    case H_CARD: {
        Cell *c = &cells[idx]; int np;
        if (busy) break;
        if (c->state == ST_NONE) c->picked = !c->picked;
        else if ((c->state == ST_OK || c->state == ST_VARIANT) && c->stl[0]) { WCHAR a[MAX_PATH + 20]; swprintf(a, MAX_PATH + 20, L"/select,\"%ls\"", c->stl); ShellExecuteW(hMain, L"open", L"explorer.exe", a, NULL, SW_SHOWNORMAL); }
        np = npicked();
        if (np) { if (np == 1) wcscpy(head, L"1 base picked"); else swprintf(head, 200, L"%d bases picked", np); wcscpy(sub, L"Press Build when you have the ones you want. \"More choices\" adds another batch and keeps your picks."); }
        break; }
    }
    redraw();
}
static void hover_at(int x, int y) {
    int i, k = H_NONE, id = -1; POINT p = { x, y };
    for (i = nhit - 1; i >= 0; i--) if (PtInRect(&hits[i].r, p)) { k = hits[i].kind; id = hits[i].idx; break; }
    if (k == H_CARD && !PtInRect(&gridR, p)) { k = H_NONE; id = -1; }
    if (k != hovK || id != hovI) {
        hovK = k; hovI = id;
        if (k == H_CARD && !busy && cells[id].state != ST_PENDING) {          /* say what the card under the mouse is */
            Cell *c = &cells[id];
            if (c->why[0]) swprintf(sub, 300, L"%ls", c->why);
            else if (c->state == ST_OK) swprintf(sub, 300, L"Built and checked: %ls. Click to show the STL file.", c->theme);
            else swprintf(sub, 300, L"%ls, detail %d of 4 (design %hs-%d). Click to %ls.", c->theme, c->tier, c->seed, c->n, c->picked ? L"un-pick" : L"pick");
        }
        redraw();
    }
    SetCursor(LoadCursor(NULL, k != H_NONE ? IDC_HAND : IDC_ARROW));
}
static void make_fonts(void) {
    HFONT *fs[] = { &fTitle, &fLabel, &fUI, &fSmall, &fBig, &fBtn, &fMono }; int px[] = { 19, 11, 13, 12, 18, 15, 12 }, wt[] = { 700, 600, 400, 400, 600, 600, 400 }, i;
    for (i = 0; i < 7; i++) { if (*fs[i]) DeleteObject(*fs[i]); *fs[i] = CreateFontW(-S(px[i]), 0, 0, 0, wt[i], 0, 0, 0, DEFAULT_CHARSET, 0, 0, CLEARTYPE_QUALITY, 0, i == 6 ? L"Consolas" : L"Segoe UI"); }
}
static void ini(int save) {
    WCHAR p[MAX_PATH], v[16]; int *vals[] = { &selStyle, &selSize, &selDetail, &selMini, &selCount, &selSup, &showDetails, &selScale }; int lim[] = { 19, 16, 5, 13, 21, 2, 2, 4 }, i;
    static const WCHAR *keys[] = { L"style", L"basesize", L"detail", L"mini", L"howmany", L"overhangs", L"details", L"figure" };
    swprintf(p, MAX_PATH, L"%ls\\out", kit);
    if (save) CreateDirectoryW(p, NULL);
    wcscat(p, L"\\bbb_kit.ini");
    if (save) {          /* the window's size and place, so it opens next time the way it was left */
        WINDOWPLACEMENT wp; static const WCHAR *k[] = { L"left", L"top", L"right", L"bottom" }; LONG *e[4];
        memset(&wp, 0, sizeof wp); wp.length = sizeof wp; GetWindowPlacement(hMain, &wp); e[0] = &wp.rcNormalPosition.left; e[1] = &wp.rcNormalPosition.top; e[2] = &wp.rcNormalPosition.right; e[3] = &wp.rcNormalPosition.bottom;
        for (i = 0; i < 4; i++) { swprintf(v, 16, L"%ld", *e[i]); WritePrivateProfileStringW(L"window", k[i], v, p); }
        WritePrivateProfileStringW(L"window", L"maximized", wp.showCmd == SW_SHOWMAXIMIZED ? L"1" : L"0", p);
    } else {
        savedR.left = (int)GetPrivateProfileIntW(L"window", L"left", 0, p); savedR.top = (int)GetPrivateProfileIntW(L"window", L"top", 0, p);
        savedR.right = (int)GetPrivateProfileIntW(L"window", L"right", 0, p); savedR.bottom = (int)GetPrivateProfileIntW(L"window", L"bottom", 0, p);
        savedMax = (int)GetPrivateProfileIntW(L"window", L"maximized", 0, p);
    }
    for (i = 0; i < 8; i++) {
        if (save) { swprintf(v, 16, L"%d", *vals[i]); WritePrivateProfileStringW(L"window", keys[i], v, p); }
        else { int x = (int)GetPrivateProfileIntW(L"window", keys[i], *vals[i], p); if (x >= (i == 4) && x < lim[i]) *vals[i] = x; }
    }
}
static LRESULT CALLBACK proc(HWND h, UINT m, WPARAM w, LPARAM l) {
    switch (m) {
    case WM_LINE: on_line((int)w, (WCHAR *)l); free((void *)l); return 0;
    case WM_ENDED: on_ended((int)w, (DWORD)l); return 0;
    case WM_TIMER: if (w == 2) KillTimer(h, 2); else anim++; redraw(); return 0;
    case WM_MOUSEMOVE: { TRACKMOUSEEVENT t = { sizeof t, TME_LEAVE, h, 0 }; TrackMouseEvent(&t); hover_at(GET_X_LPARAM(l), GET_Y_LPARAM(l)); return 0; }
    case WM_MOUSELEAVE: hovK = H_NONE; hovI = -1; redraw(); return 0;
    case WM_SETCURSOR: if (LOWORD(l) == HTCLIENT) return TRUE; break;
    case WM_LBUTTONDOWN: hover_at(GET_X_LPARAM(l), GET_Y_LPARAM(l)); if (hovK != H_NONE) act(hovK, hovI); return 0;
    case WM_MOUSEWHEEL:
        scrollY -= GET_WHEEL_DELTA_WPARAM(w) * S(90) / 120;
        if (scrollY < 0) scrollY = 0;
        if (scrollY > scrollMax) scrollY = scrollMax;
        redraw(); return 0;
    case WM_KEYDOWN:
        if (showHelp && w != VK_ESCAPE && w != VK_F1 && w != VK_OEM_2 && w != VK_SHIFT) { showHelp = 0; redraw(); }
        else if (w == VK_SHIFT) ;
        else if (w == VK_RETURN) { if (npicked()) build(); else ask_choices(1); }
        else if (w == VK_F1 || (w == VK_OEM_2 && GetKeyState(VK_SHIFT) < 0)) act(H_HELP, 0);          /* F1, or ? (Shift+/) */
        else if (w == VK_ESCAPE) { if (showHelp) act(H_HELP, 0); else act(H_STOP, 0); }
        else if (w == 'A' && GetKeyState(VK_CONTROL) < 0) act(H_ALL, 0);
        else if (w == 'D') act(H_DETAILS, 0);
        return 0;
    case WM_CHAR:          /* digits set "how many": 1 then 5 within a moment is 15 */
        if (showHelp) return 0;
        if (w >= '0' && w <= '9') {
            int d = (int)w - '0', two = selCount * 10 + d;
            if (GetTickCount() - lastDigit < 1500 && two >= 1 && two <= 20) selCount = two; else if (d >= 1) selCount = d; else return 0;
            lastDigit = GetTickCount(); SetTimer(h, 2, 1600, NULL); redraw();
        } else if (w == '+' || w == '=') act(H_COUNT, 1);
        else if (w == '-') act(H_COUNT, -1);
        return 0;
    case WM_ERASEBKGND: return 1;
    case WM_PAINT: { PAINTSTRUCT ps; RECT r; HDC dc = BeginPaint(h, &ps); GetClientRect(h, &r); if (r.right > 0 && r.bottom > 0) paint(dc, r.right, r.bottom); EndPaint(h, &ps); return 0; }
    case WM_SIZE: redraw(); return 0;
    case WM_GETMINMAXINFO: { MINMAXINFO *mi = (MINMAXINFO *)l; RECT r = { 0, 0, S(1190), S(860) }; AdjustWindowRect(&r, WS_OVERLAPPEDWINDOW, FALSE); mi->ptMinTrackSize.x = r.right - r.left; mi->ptMinTrackSize.y = r.bottom - r.top; return 0; }
    case 0x02E0: { RECT *r = (RECT *)l; SC = HIWORD(w) / 96.0; make_fonts(); SetWindowPos(h, NULL, r->left, r->top, r->right - r->left, r->bottom - r->top, SWP_NOZORDER | SWP_NOACTIVATE); redraw(); return 0; }          /* WM_DPICHANGED: moved to a monitor with another scaling */
    case WM_DESTROY: if (hJob) TerminateJobObject(hJob, 1); if (!selftest) ini(1); PostQuitMessage(0); return 0;
    }
    return DefWindowProcW(h, m, w, l);
}
static int find_python(void) {
    WCHAR e[MAX_PATH];
    if (GetEnvironmentVariableW(L"BBB_PYTHON", e, MAX_PATH) && GetFileAttributesW(e) != INVALID_FILE_ATTRIBUTES) { wcscpy(py, e); return 1; }
    if (GetFileAttributesW(L"C:\\Python315\\python.exe") != INVALID_FILE_ATTRIBUTES) { wcscpy(py, L"C:\\Python315\\python.exe"); return 1; }
    return SearchPathW(NULL, L"python.exe", NULL, MAX_PATH, py, NULL) > 0;
}
int WINAPI WinMain(HINSTANCE hi, HINSTANCE hp, LPSTR cmdA, int show) {
    WNDCLASSW wc; MSG msg; RECT wa, r; WCHAR t[MAX_PATH], e[MAX_PATH], *sl; FILE *f; int cw = 1290, chh = 896; JOBOBJECT_EXTENDED_LIMIT_INFORMATION ji; DWORD ex = 0; BOOL dark = TRUE;
    GdiplusStartupInput gin = { 1, NULL, FALSE, FALSE }; ULONG_PTR gtok; HMODULE u32 = GetModuleHandleW(L"user32.dll"), dwm; HDC sdc;
    (void)hp;
    if (!strncmp(cmdA, "--demotest", 10)) { selftest = 3; swprintf(stShot, MAX_PATH, L"%hs", cmdA + 10 + (cmdA[10] == ' ')); }
    else if (!strncmp(cmdA, "--quicktest", 11)) { selftest = 1; stStep = 2; autoBuild = 1; stSize = 3; swprintf(stShot, MAX_PATH, L"%hs", cmdA + 11 + (cmdA[11] == ' ')); }
    else if (!strncmp(cmdA, "--resizetest", 12)) { selftest = 2; swprintf(stShot, MAX_PATH, L"%hs", cmdA + 12 + (cmdA[12] == ' ')); }
    else if (!strncmp(cmdA, "--selftest", 10)) { selftest = 1; swprintf(stShot, MAX_PATH, L"%hs", cmdA + 10 + (cmdA[10] == ' ')); if (!stShot[0]) wcscpy(stShot, L"bbb_selftest.bmp"); }
    GetModuleFileNameW(NULL, kit, MAX_PATH); sl = wcsrchr(kit, L'\\');
    if (sl) *sl = 0;
    swprintf(e, MAX_PATH, L"%ls\\bbb_kit_folder.txt", kit); f = _wfopen(e, L"r");
    if (f) { char a[MAX_PATH]; if (fgets(a, sizeof a, f)) { a[strcspn(a, "\r\n")] = 0; if (a[0]) swprintf(kit, MAX_PATH, L"%hs", a); } fclose(f); }
    if (GetEnvironmentVariableW(L"BBB_KIT", e, MAX_PATH)) wcscpy(kit, e);
    swprintf(t, MAX_PATH, L"%ls\\run_set.py", kit);
    if (GetFileAttributesW(t) == INVALID_FILE_ATTRIBUTES) { sl = wcsrchr(kit, L'\\'); if (sl) *sl = 0; swprintf(t, MAX_PATH, L"%ls\\run_set.py", kit); }      /* started from gui\ : the kit is one folder up */
    if (GetFileAttributesW(t) == INVALID_FILE_ATTRIBUTES) { if (selftest) return 2; MessageBoxW(NULL, L"bbb_kit.exe must sit in the kit folder, beside run_set.py.", L"Balderding's Ballin Base Kit", MB_OK | MB_ICONINFORMATION); return 1; }
    if (!find_python()) { if (selftest) return 3; MessageBoxW(NULL, L"Python was not found.\n\nInstall Python 3.10 or newer, or set the BBB_PYTHON variable to the full path of python.exe.", L"Balderding's Ballin Base Kit", MB_OK | MB_ICONINFORMATION); return 1; }
    SetEnvironmentVariableW(L"PYTHONWARNINGS", L"ignore"); SetEnvironmentVariableW(L"PYTHONUNBUFFERED", L"1"); SetEnvironmentVariableW(L"PYTHONIOENCODING", L"utf-8");
    hJob = CreateJobObjectW(NULL, NULL);
    if (hJob) { memset(&ji, 0, sizeof ji); ji.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE; SetInformationJobObject(hJob, JobObjectExtendedLimitInformation, &ji, sizeof ji); }
    { typedef BOOL(WINAPI *F)(HANDLE); F fn = (F)(void *)GetProcAddress(u32, "SetProcessDpiAwarenessContext"); if (!fn || !fn((HANDLE)-4)) SetProcessDPIAware(); }          /* sharp on every monitor */
    GdiplusStartup(&gtok, &gin, NULL);
    sdc = GetDC(NULL); SC = GetDeviceCaps(sdc, LOGPIXELSY) / 96.0; ReleaseDC(NULL, sdc);
    SystemParametersInfoW(SPI_GETWORKAREA, 0, &wa, 0);
    if ((chh + 44) * SC > wa.bottom - wa.top) { chh = (int)((wa.bottom - wa.top) / SC) - 44; if (chh < 860) { chh = 860; SC = (wa.bottom - wa.top) / (double)(chh + 44); } }          /* a small screen: a lower window, and only then everything a little smaller */
    if ((cw + 20) * SC > wa.right - wa.left) cw = (int)((wa.right - wa.left) / SC) - 20;
    if (!selftest) ini(0);
    make_fonts(); wcscpy(head, L"Pick a style and a size"); wcscpy(sub, L"Then press Show choices. Nothing is built until you press Build.");
    memset(&wc, 0, sizeof wc); wc.lpfnWndProc = proc; wc.hInstance = hi; wc.lpszClassName = L"bbb_kit"; wc.hCursor = LoadCursor(NULL, IDC_ARROW); wc.hIcon = LoadIcon(NULL, IDI_APPLICATION); RegisterClassW(&wc);
    SetRect(&r, 0, 0, S(cw), S(chh)); AdjustWindowRect(&r, WS_OVERLAPPEDWINDOW, FALSE);
    if (selftest) ex = WS_EX_NOACTIVATE | WS_EX_TOOLWINDOW;
    hMain = CreateWindowExW(ex, L"bbb_kit", L"Balderding's Ballin Base Kit", WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN,
                            selftest ? -8000 : wa.left + ((wa.right - wa.left) - (r.right - r.left)) / 2, selftest ? -8000 : wa.top + ((wa.bottom - wa.top) - (r.bottom - r.top)) / 2,
                            r.right - r.left, r.bottom - r.top, NULL, NULL, hi, NULL);
    dwm = LoadLibraryW(L"dwmapi.dll");          /* a dark title bar to match */
    if (dwm) { typedef HRESULT(WINAPI *D)(HWND, DWORD, LPCVOID, DWORD); D fn = (D)(void *)GetProcAddress(dwm, "DwmSetWindowAttribute"); if (fn && fn(hMain, 20, &dark, sizeof dark) != S_OK) fn(hMain, 19, &dark, sizeof dark); }
    if (!selftest && savedR.right - savedR.left > 200 && savedR.bottom - savedR.top > 200 && MonitorFromRect(&savedR, MONITOR_DEFAULTTONULL)) {
        WINDOWPLACEMENT wp; memset(&wp, 0, sizeof wp); wp.length = sizeof wp; wp.rcNormalPosition = savedR; wp.showCmd = SW_HIDE; SetWindowPlacement(hMain, &wp);
        if (savedMax) show = SW_SHOWMAXIMIZED;
    }
    ShowWindow(hMain, selftest ? SW_SHOWNOACTIVATE : show); UpdateWindow(hMain);
    if (selftest == 2) {          /* --resizetest: make the window larger, then as small as it will go, and write down what it became */
        FILE *o; WCHAR q[MAX_PATH]; RECT a, b2, c2; LONG st = GetWindowLongW(hMain, GWL_STYLE); int i;
        GetClientRect(hMain, &a); SetWindowPos(hMain, NULL, 0, 0, a.right + S(520), a.bottom + S(300), SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE); GetClientRect(hMain, &b2);
        for (i = 0; i < 26; i++) { memset(&cells[i], 0, sizeof(Cell)); cells[i].state = ST_NONE; cells[i].n = i + 1; cells[i].tier = 4; cells[i].picked = i % 5 == 0; strcpy(cells[i].seed, "test"); swprintf(cells[i].theme, 64, L"sample %d", i + 1); }
        ncell = 26; wcscpy(head, L"Resize test: 26 sample cards"); PostMessageW(hMain, WM_CHAR, '1', 0); PostMessageW(hMain, WM_CHAR, '5', 0);
        { MSG m2; while (PeekMessageW(&m2, NULL, 0, 0, PM_REMOVE)) { TranslateMessage(&m2); DispatchMessageW(&m2); } } capture(stShot);
        SetWindowPos(hMain, NULL, 0, 0, 200, 200, SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE); GetClientRect(hMain, &c2);
        swprintf(q, MAX_PATH, L"%ls.small.bmp", stShot); capture(q);
        SetWindowPos(hMain, NULL, 0, 0, S(1310), S(850), SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE); showHelp = 1; swprintf(q, MAX_PATH, L"%ls.help.bmp", stShot); capture(q); showHelp = 0; swprintf(q, MAX_PATH, L"%ls.txt", stShot); o = _wfopen(q, L"w");
        if (o) { fprintf(o, "how_many_after_typing_1_5 %d\n", selCount); fprintf(o, "thick_frame %d\nmaximize_box %d\nstart %ldx%ld\nlarger %ldx%ld\nsmallest %ldx%ld\nscroll_max_small %d\n", !!(st & WS_THICKFRAME), !!(st & WS_MAXIMIZEBOX), a.right, a.bottom, b2.right, b2.bottom, c2.right, c2.bottom, scrollMax); fclose(o); }
        return 0;
    }
    if (selftest) { char e[8]; if (GetEnvironmentVariableA("BBB_TEST_SCALE", e, sizeof e)) selScale = atoi(e) & 3; }          /* a test may ask for another figure size: 0 = 32 mm ... 3 = tiny */
    if (selftest == 3) { selStyle = 14; selSize = 8; selDetail = 4; selMini = 0; selCount = 8; selSup = 0; ask_choices(1); }
    if (selftest == 1) { selStyle = 6; selSize = stSize; selDetail = 4; selMini = 0; selCount = autoBuild ? 2 : 8; selSup = 0; ask_choices(1); }
    while (GetMessageW(&msg, NULL, 0, 0) > 0) { TranslateMessage(&msg); DispatchMessageW(&msg); }
    GdiplusShutdown(gtok); return 0;
}
