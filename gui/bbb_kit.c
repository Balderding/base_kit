/* bbb_kit.exe: the window in front of the kit. Plain C, Win32 + GDI+ (both part of Windows), nothing to install.

   What it does: you choose style, size, detail and miniature on the left and press "Show choices". The kit draws that
   many candidate bases as small pictures. You click the ones you want and press Build: the kit builds exactly those,
   puts each through the print check, and the card shows the finished base. "More choices" adds another batch and keeps
   what you picked.

   How it runs a job: the kit is C, inside this exe (cport\runner.c, cport\pipeline.c and the rest). A job
   (choices|build ...) runs in this process on a worker thread (runner_main), which starts helper copies of this exe
   for the layouts, the height fields and the pictures (bbb_kit.exe --core-...). Every line it gives out is one JSON
   record with an "ev" field (choice, start, retry, built, done, log ...), so nothing is guessed from printed text.
   The helpers sit in a job object: Stop, or closing the window, ends them all. No python.exe is ever started.

   Drawing: the whole window is painted by this file into one off-screen picture (no flicker), shapes and pictures
   with GDI+ (smooth edges), text with GDI (ClearType). No standard controls, so it looks the same everywhere and
   follows the monitor's scaling (per-monitor DPI). Colours: dark ground, teal for "yours / done", gold for "look here".

   Where things are: anywhere. Everything the kit needs is inside the exe (GEOS, the model packs, the settings
   defaults, the fonts); a settings.json beside it may override settings, and the output goes to an "out" folder
   beside it (bbb_kit_folder.txt beside the exe, one line, or the BBB_KIT variable may name another folder).
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
#include "live.h"
#include "runner.h"          /* cport\runner.c: the job runner (gui_job.py in C) */
#include "pipeline.h"        /* cport\pipeline.c: bbb_kit.exe --core-layout */
#ifdef BBB_NK
#include "nk_ui.h"          /* the Nuklear face (gui
k): built with BBB_NK=1, still one exe */
#endif
int bbb_core_main(const char *cmd);          /* cport\core.c: bbb_kit.exe --core-mesh and the other C parts of the kit */

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
/* the pixel look's ramp (work\ui_research\REPORT.md), made from the colours above. No red anywhere: navy, teal, green, gold */
#define C_EDGE 0xFF0D0F13u          /* the dark outline round every pixel box */
#define C_OFF 0xFF8C95A3u           /* ink of a button that is off (F1 2026-11-22: was 5F6775, 2.55:1 on the card colour; now about 4.7:1, still dimmer than C_DIM) */
#define C_BEVH 0xFF343946u          /* bevel: the lit top and left edge of a box */
#define C_BEVHH 0xFF3E4452u
#define C_BEVD 0xFF1C1F26u          /* bevel: the shaded bottom and right edge */
#define C_NAVY 0xFF1E2A4Au
#define C_NAVYH 0xFF2E4373u
#define C_NAVYD 0xFF172038u
#define C_TEALD 0xFF0E6B62u
#define C_TEALM 0xFF149384u
#define C_TEALL 0xFF6FE3D4u
#define C_GREEN 0xFF5DBB63u
#define C_GOLDD 0xFF9A7420u
#define C_BAR 0xFFC9D3E6u           /* label ink on a navy bar */
#define C_STONEL 0xFF20242Bu        /* the stone texture's light and dark grain on the side panel */
#define C_STONED 0xFF16181Eu
/* resize plan R1 (2026-11-22): the side panel's width in design px (x SC on screen), dragged by the line between it and the
   cards; double-click puts it back. Saved as sidew in bbb_kit.ini. SIDE_W stays the name every row uses. */
enum { SIDE_W0 = 420, SIDE_MIN = 320, SIDE_FOOT = 92 };          /* SIDE_FOOT: R4, the side panel's footer (Show / build straight away), design px */
static int sideW = SIDE_W0, sideWant = SIDE_W0;          /* sideWant: what the user set (kept, saved); sideW: that, kept in bounds for this window */
enum { DET_H0 = 170, DET_MIN = 110 };
static int detailsH = DET_H0, detWant = DET_H0;          /* R2: the Details pane's height in design px, dragged by the line above it; detWant saved as detailsh */
static int divDrag, divHover;          /* R1 / R2: which line is being dragged / is under the mouse: 1 the side panel's, 2 the Details pane's */
#define SIDE_W sideW
enum { HEAD_H = 74, STRIP_H = 30, FOOT_H = 66, MAXC = 80, MAXLOG = 300 };
enum { ST_PENDING, ST_NONE, ST_BUILDING, ST_OK, ST_VARIANT, ST_FAIL };
enum { H_NONE, H_STYLE, H_SIZE, H_DETAIL, H_MINI, H_COUNT, H_SUP, H_SHOW, H_MORE, H_ALL, H_CLEAR, H_GO, H_STOP, H_OPEN, H_DETAILS, H_CARD, H_HELP, H_QUICK, H_SCALE, H_OVAL, H_LOOK, H_SPEED, H_VIEW, H_PARTS, H_COL, H_JOIN, H_MF, H_OBJ, H_MODELS,
       H_THEME, H_SEED, H_STR, H_MULTI, H_SET, H_PRINTER, H_SQ };

struct LiveC;          /* a building card's live view (below, "the live view") */
typedef struct { char seed[16], size[16]; int n, tier, picked, state, retry, col; GpImage *img, *img3, *img3c; double secs; WCHAR theme[64], why[220], stl[MAX_PATH]; struct LiveC *lv; } Cell;
typedef struct { RECT r; int kind, idx; } Hit;
static Cell cells[MAXC]; static int ncell;
static Hit hits[400]; static int nhit, hovK, hovI = -1;
static WCHAR *logv[MAXLOG]; static int nlog;
/* the scale follows the window (2026-11-20): the layout is drawn for DESIGN_W x DESIGN_H, and at any window size everything is
   scaled so that size fits (larger on a big or 4K screen, smaller on a small one), in 5 % steps from 0.6 to 3; under that the side
   panel scrolls (sideOff, sideMax) and the cards scroll as they always did */
enum { DESIGN_W = 1650, DESIGN_H = 1240 };          /* 1240 since 2026-11-21: the theme, strength, seed and saved-set rows (was 1030) */          /* DESIGN_H: the side panel's full height (measured with the UI check, compact mode) */
/* 2026-11-22: the scale follows the SCREEN size, not the side panel's full height (the panel scrolls): FIT_W x FIT_H is what
   a scale of 1 fills, so a maximized 1080p window is 1.0, 1440p about 1.35, 4K about 2; text stays a comfortable size */
enum { FIT_W = 1650, FIT_H = 1000 };
static double fit_sc(int w, int h) { double s = fmin(w / (double)FIT_W, h / (double)FIT_H);          /* R6: the screen alone sets the scale; the dividers only share the room out (a wider panel shrinking everything was worse) */ s = floor(s * 20 + 0.5) / 20; return s < 0.6 ? 0.6 : s > 3.0 ? 3.0 : s; }
static int sideOff, sideMax, hitDy, txtDy, titleH;          /* the side panel's scroll (px), how far it can go, and the shift its click areas get */
static HWND hMain; static double SC = 1.0; static HFONT fTitle, fLabel, fUI, fSmall, fBig, fBtn, fMono;
static WCHAR kit[MAX_PATH], outFolder[MAX_PATH], sub[300], head[200], batchFolder[64];          /* batchFolder: several sizes, the batch's picked_ folder name */
static char curStyle[32], curSize[16], curMini[16], curDetail[8], curScale[8] = "1", curObj[16] = "3"; static int curSup, curJoin = 1, selScale = 0, selObj = 5, curFig = 32;          /* selScale: the game scale (GAMEMM), selObj: the slider (OBJM) */
static int selStyle = 6, selSize = 8, selDetail = 4, selMini = 0, selCount = 8, selSup = 0, selJoin = 1, showDetails = 0;
/* plan B step 6 (2026-11-21): the theme picker (selTheme 0 = any, k = the style's k-th theme), the design seed box
   (seedBuf, typed while seedEdit), style strength, several sizes at once (sizeMask, built one size after another), and
   three saved sets (out\bbb_kit.ini [set1]..[set3]) */
static const char *STRENGTHS[] = { "0.5", "0.75", "1", "1.5", "2" }; enum { NSTR = 5 };
static const char *PRINTERS[] = { "FDM", "Resin" }; static int selPrinter;          /* review N1: the printer the bases are for (grain size, smallest detail) */
static int selStr = 2, selTheme, seedEdit, sizeMulti, setSave, sizeQ[32], nSizeQ; static unsigned sizeMask; static char seedBuf[16];
static const char *style_name(void);
static void sets_ini(WCHAR *p);          /* where the saved sets live (a test: beside its picture, never the real ini) */
extern int kd_theme_names(const char *style, const char **out, int max);
static const char *theme_name(void) { static const char *nm[64]; int n = kd_theme_names(style_name(), nm, 64); return selTheme >= 1 && selTheme <= n ? nm[selTheme - 1] : NULL; }
static int theme_count(void) { const char *nm[64]; return kd_theme_names(style_name(), nm, 64); }
static void theme_opt(WCHAR *extra, size_t cap) {          /* " --theme words+joined" (the job splits its line on spaces) */
    const char *t = theme_name(); WCHAR b[160]; int i;
    if (!t) return;
    swprintf(b, 160, L" --theme %hs", t); for (i = 9; b[i]; i++) if (b[i] == L' ') b[i] = L'+';
    wcsncat(extra, b, cap - wcslen(extra) - 1);
}
static int mfOn[4] = { 1, 1, 0, 0 };          /* the MANIFOLD switches: Smaller files, Strict check, Smooth details, Organic rocks */
static const char *MFN[4] = { "Smaller files", "Strict check", "Smooth details", "Organic rocks" }, *MFA[4] = { "smaller", "strict", "smooth", "rocks" }; static int MF_N = 2;          /* 2.1.0: Smooth details and Organic rocks not shown yet (still being checked; --smooth / --rocks work) */          /* selCount: how many bases, 1 to 20 */
static int busy, wanted, doneN, nOK, nBad, scrollY, scrollMax, anim, selftest, stStep; static WCHAR stShot[MAX_PATH];
static HANDLE hJob;
static int stSize = 8, demoShot; static RECT gridR, savedR; static int savedMax, showHelp, autoBuild; static DWORD lastDigit;
/* the look: three switches, saved in out\bbb_kit.ini. lookPix: pixel frames (off = the smooth look with round corners);
   lookTex: textures (dither, scanlines, segmented progress, marching ants; with the pixel look only); lookFont: the pixel
   font for labels, chips, buttons and the title. PX: one art pixel in screen pixels, a whole number from the monitor's
   scaling (2 at 100 %, 3 at 150 %, 4 at 200 %); FS: the pixel font's scale, also whole */
static int lookPix = 1, lookTex = 1, lookFont = 1, PX = 2, FS = 1;
static int colLast;          /* the colour view was last switched on: new built cards start in it too */
/* speed: two switches, saved in out\bbb_kit.ini and handed to the job as KIT_THREADS / KIT_GPU.
   spThr: the height field and the mesh on as many CPU threads as the base needs (off = one core, the proven path). spGpu: the 3D pictures on
   the video card (off = drawn by Windows' software renderer, slower). */
static int spThr = 1, spGpu = 1;
/* the live view (docs\superpowers\specs\2026-11-19-live-build-view-design.md), saved in out\bbb_kit.ini. lvView: 0 Small (words, a bar, two
   meters), 1 Live card (the base taking shape in its card), 2 Big view (the base being built large, rows over time below). The parts,
   each its own switch: lvCores one bar per core, lvGpu the video card meter, lvShape the base taking shape, lvTime time left,
   lvRows rows over time. lvParts: the Parts panel is open. Read twice a second (timer 3): the core bars are OUR threads only (every process
   the window started, in hJob), the video card meter is the whole PC's. */
static int lvView = 1, lvCores = 1, lvGpu = 1, lvShape = 1, lvTime = 1, lvRows = 1, lvParts;
static float cpuV[LIVE_MAXCORE], gpuV = -1, jobCores; static int ncpu, nthr;
static void meters_tick(void) { float pc[LIVE_MAXCORE]; ncpu = live_cpu(pc, LIVE_MAXCORE); nthr = live_job(hJob, cpuV, LIVE_MAXCORE, &jobCores); gpuV = live_gpu(); }
/* idle CPU (2026-11-22, GUICy's baseline: 31.5 % of a core idle, a full redraw twice a second for the meters):
   the window redraws for the meters only when what they show changed, and not at all while it is behind another
   window (a game in front) or minimized and nothing is building. in_front: the window or one of its own is active */
static int idleSecs, nPaint;          /* --idletest: how long, and how many full redraws happened */
static int in_front(void) { if (selftest == 6) return GetEnvironmentVariableA("BBB_TEST_FRONT", NULL, 0) > 0; HWND f = GetForegroundWindow(); return f && (f == hMain || GetAncestor(f, GA_ROOTOWNER) == hMain) && !IsIconic(hMain); }
static long long meters_shown(void) {          /* what the strip shows now, as one number: busy, our threads, our cores, the video card's percent */
    long long g = gpuV < 0 ? -1 : (long long)(gpuV * 100 + 0.5);
    if (!busy) g = g < 0 ? -1 : g / 5;          /* idle: the video card's percent in 5 % steps (the rest of the PC moves it all the time) */
    return ((long long)busy << 40) ^ ((long long)nthr << 28) ^ ((long long)(jobCores * 10 + 0.5) << 12) ^ (g & 0xFFF);
}
static void speed_env(void) { WCHAR n[16]; DWORD c = GetActiveProcessorCount(ALL_PROCESSOR_GROUPS); swprintf(n, 16, L"%lu", (unsigned long)(c > 1 ? c : 2)); SetEnvironmentVariableW(L"KIT_THREADS", spThr ? n : L"0"); SetEnvironmentVariableW(L"KIT_GPU", spGpu ? L"1" : L"0"); }

static const char *STYLES[] = { "badlands", "highland", "maahl", "corridor", "infestation", "urban", "mine", "swamp", "cynder", "targa",
                                "trederra", "fortress", "beast", "frontier", "graveyard", "forest", "dungeon", "temple", "coast",
                                "sundered", "ashveil", "ferrum", "thornwild", "obelis", "blight" };          /* the last six: the styles kept one per file (style_<name>.py) */
#define NSTYLES ((int)(sizeof STYLES / sizeof STYLES[0]))
static const char *style_name(void) { return STYLES[selStyle]; }
static const char *SIZES[] = { "20", "25", "28", "28.5", "30", "32", "40", "50", "60", "65", "70", "80", "90", "100", "120", "130", "160",          /* the common round bases; the kit itself takes any size from 20 to 200 */
                                "60x35.5", "75x42", "90x52", "105x70", "120x92", "150x95", "170x109",          /* then the common oval bases, long x short */
                                "20sq", "25sq", "40sq", "50sq", "25x50r", "50x100r" };          /* then square and rectangle bases (2026-11-22) */
enum { NROUND = 17, NOVAL = 7, NSQ = 6, NSIZES = 30 };
static const char *DETAILL[] = { "Mix", "1", "2", "3", "4" }, *DETAILV[] = { "mix", "1", "2", "3", "4" };
static const char *SCALES[] = { "32 mm", "28 mm", "15 mm" }; static const double GAMEMM[] = { 32, 28, 15 };          /* the game scale: how tall a man is */
static const double OBJM[] = { 0.33, 0.45, 0.6, 0.75, 0.9, 1.0, 1.15, 1.3, 1.5 }; enum { NOBJ = 9, OBJ_TRUE = 5 };          /* the slider: times the true size (0.33 of 32 mm: the classic look) */
static const char *MINIS[] = { "default", "biped", "wide", "walker", "tripod", "quad", "many", "scorpion", "crawler", "tracked", "hover", "none", "mix" };


static int S(double x) { return (int)(x * SC + 0.5); }
static int snp(double v) { return (int)floor(v / PX + 0.5) * PX; }          /* to the nearest whole art pixel */
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

/* ---------- running a job: runner_main on a worker thread ---------- */
typedef struct { int mode, argc; char *argv[64]; char buf[4096]; } Run;
static void job_line(void *ctx, const char *line) {          /* one JSON record: to the window as a WM_LINE (the window frees it) */
    Run *r = (Run *)ctx; int n = MultiByteToWideChar(CP_UTF8, 0, line, -1, NULL, 0); WCHAR *w = (WCHAR *)malloc((n + 1) * sizeof(WCHAR));
    MultiByteToWideChar(CP_UTF8, 0, line, -1, w, n); PostMessageW(hMain, WM_LINE, r->mode, (LPARAM)w);
}
static DWORD WINAPI job_thread(LPVOID p) {
    Run *r = (Run *)p; int code = runner_main(r->argc, r->argv, job_line, r);
    PostMessageW(hMain, WM_ENDED, r->mode, (DWORD)code); free(r); return 0;
}
static int run(const WCHAR *cmd, int mode) {          /* cmd: gui_job.py's arguments (choices|build <style> ...) */
    static WCHAR b[2800]; Run *r = (Run *)calloc(1, sizeof *r); char *q; HANDLE th;
    swprintf(b, 2800, L"> job %ls", cmd); logw(b);
    WideCharToMultiByte(CP_UTF8, 0, cmd, -1, r->buf, sizeof r->buf, NULL, NULL);
    for (q = strtok(r->buf, " "); q && r->argc < 63; q = strtok(NULL, " ")) r->argv[r->argc++] = q;
    r->mode = mode; runner_stop = 0; runner_job = hJob;
    th = CreateThread(NULL, 1 << 24, job_thread, r, 0, NULL);
    if (!th) { swprintf(head, 200, L"The job could not be started"); swprintf(sub, 300, L"Windows error %lu.", GetLastError()); logw(sub); free(r); return 0; }
    CloseHandle(th);
    busy = mode; anim = 0; SetTimer(hMain, 1, 60, NULL); return 1;
}

/* ---------- the live view: what each building card shows, from the build's progress notes (live.c, live.py) ----------
   Polled every 0.1 s while building (timer 4): the stage, the pieces laid out so far (their outlines, .shapes), the
   build's drawing from above once the layout is done, the height field's tiles and which are done (.tiles), and the
   time left from earlier builds of the same kind (out\live_times.txt). Kept after the base is done, for the rows over time. */
typedef struct LiveC {
    LiveNote nt; int have; GpImage *plan; WCHAR planPath[MAX_PATH];
    float *pts; int npts, cappts, *pstart, npoly, cappoly; char *pkind; long long shOff;
    double expect, shownLeft, tEnd; int segS[16]; double segT[16]; int nseg;
} LiveC;
static WCHAR liveDir[MAX_PATH], liveTimes[MAX_PATH]; static double jobT0; static int bigPick = -1, bigNow = -1, shrinkI = -1; static DWORD shrinkT; static RECT shrinkFrom, cardR[MAXC], bigR;
static const char *STAGES[5] = { "layout", "field", "mesh", "picture", "checks" };
static double now_unix(void) { FILETIME ft; ULARGE_INTEGER u; GetSystemTimeAsFileTime(&ft); u.LowPart = ft.dwLowDateTime; u.HighPart = ft.dwHighDateTime; return (double)(u.QuadPart - 116444736000000000ULL) / 1e7; }
static int stage_ix(const char *s) { int i; for (i = 0; i < 5; i++) if (!strcmp(s, STAGES[i])) return i; return 0; }
static void live_free(LiveC *L) { if (!L) return; if (L->plan) GdipDisposeImage(L->plan); free(L->pts); free(L->pstart); free(L->pkind); free(L); }
static void live_restart(LiveC *L) {          /* a retry of the same base: its picture starts again */
    if (L->plan) GdipDisposeImage(L->plan);
    L->plan = NULL; L->planPath[0] = 0; L->npts = L->npoly = 0; L->shOff = 0; L->shownLeft = 1e9; L->nseg = 0; L->have = 0;
}
static GpImage *load_copy(const WCHAR *p, UINT maxw) {          /* a picture copied into memory (the file is not held open), its white surround made see-through */
    GpImage *im = NULL; GpBitmap *out = NULL; GpGraphics *g = NULL; GpImageAttributes *ia = NULL; UINT w = 0, h = 0, W, H;
    if (GdipLoadImageFromFile(p, &im) != Ok || !im) return NULL;
    GdipGetImageWidth(im, &w); GdipGetImageHeight(im, &h); W = w; H = h;
    if (!w || !h) { GdipDisposeImage(im); return NULL; }
    if (W > maxw) { H = H * maxw / W; W = maxw; }
    if (GdipCreateBitmapFromScan0((INT)W, (INT)H, 0, PixelFormat32bppARGB, NULL, &out) == Ok && GdipGetImageGraphicsContext((GpImage *)out, &g) == Ok) {
        GdipSetInterpolationMode(g, InterpolationModeHighQualityBicubic); GdipCreateImageAttributes(&ia);
        if (ia) GdipSetImageAttributesColorKeys(ia, ColorAdjustTypeDefault, TRUE, 0xFFF2F2F2u, 0xFFFFFFFFu);
        GdipDrawImageRectRectI(g, im, 0, 0, (INT)W, (INT)H, 0, 0, (INT)w, (INT)h, UnitPixel, ia, NULL, NULL);
        if (ia) GdipDisposeImageAttributes(ia);
        GdipDeleteGraphics(g);
    }
    GdipDisposeImage(im); return (GpImage *)out;
}
static void shapes_read(LiveC *L, const WCHAR *path) {          /* the piece outlines added since the last look: "r|c x y x y ..." per line */
    HANDLE f = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, NULL, OPEN_EXISTING, 0, NULL); LARGE_INTEGER sz, off; DWORD got = 0; char *b, *e, *ln;
    if (f == INVALID_HANDLE_VALUE) return;
    GetFileSizeEx(f, &sz);
    if (sz.QuadPart < L->shOff) { L->shOff = 0; L->npts = L->npoly = 0; }          /* started again */
    if (sz.QuadPart > L->shOff && sz.QuadPart - L->shOff < (4 << 20) && (b = (char *)malloc((size_t)(sz.QuadPart - L->shOff) + 1))) {
        off.QuadPart = L->shOff; SetFilePointerEx(f, off, NULL, FILE_BEGIN); ReadFile(f, b, (DWORD)(sz.QuadPart - L->shOff), &got, NULL); b[got] = 0;
        if ((e = strrchr(b, '\n'))) {
            *e = 0; L->shOff += (e - b) + 1;
            for (ln = b; ln && *ln; ) {
                char *nx = strchr(ln, '\n'), *q = ln + 1; int k = 0;
                if (nx) *nx = 0;
                if (L->npoly + 1 >= L->cappoly) { L->cappoly = L->cappoly ? L->cappoly * 2 : 256; L->pstart = (int *)realloc(L->pstart, sizeof(int) * (L->cappoly + 1)); L->pkind = (char *)realloc(L->pkind, L->cappoly); }
                L->pstart[L->npoly] = L->npts; L->pkind[L->npoly] = ln[0];
                for (;;) {
                    char *z; double v = strtod(q, &z);
                    if (z == q) break;
                    if (L->npts >= L->cappts) { L->cappts = L->cappts ? L->cappts * 2 : 4096; L->pts = (float *)realloc(L->pts, sizeof(float) * L->cappts); }
                    L->pts[L->npts++] = (float)v; q = z; k++;
                }
                if (k >= 6 && !(k & 1)) { L->npoly++; L->pstart[L->npoly] = L->npts; } else L->npts = L->pstart[L->npoly];
                ln = nx ? nx + 1 : NULL;
            }
        }
        free(b);
    }
    CloseHandle(f);
}
static void live_poll(void) {
    int i; double t = now_unix();
    for (i = 0; i < ncell; i++) {
        Cell *c = &cells[i]; LiveC *L; LiveNote nt; WCHAR p[MAX_PATH]; int si;
        if (c->state != ST_BUILDING) continue;
        if (!c->lv) { if (!(c->lv = (LiveC *)calloc(1, sizeof(LiveC)))) continue; c->lv->expect = live_expect(liveTimes, curStyle, curSize, c->tier); c->lv->shownLeft = 1e9; }
        L = c->lv;
        if (!live_read(liveDir, c->seed, c->n, &nt) || nt.t0 < jobT0 - 5) continue;          /* not started yet, or a note from an earlier build */
        if (L->have && nt.t0 > L->nt.t0 + 0.5) live_restart(L);
        L->nt = nt; L->have = 1; si = stage_ix(nt.stage);
        if ((!L->nseg || L->segS[L->nseg - 1] != si) && L->nseg < 16) { L->segS[L->nseg] = si; L->segT[L->nseg] = L->nseg ? t : nt.t0; L->nseg++; }
        if (nt.plan[0] && wcscmp(nt.plan, L->planPath)) { GpImage *im = load_copy(nt.plan, 520); if (im) { if (L->plan) GdipDisposeImage(L->plan); L->plan = im; wcscpy(L->planPath, nt.plan); } }
        if (!L->plan) { swprintf(p, MAX_PATH, L"%ls\\%hs_%d.txt.shapes", liveDir, c->seed, c->n); shapes_read(L, p); }
        if (L->expect > 0) { double left = L->expect - (t - nt.t0); if (left < 0) left = 0; if (left < L->shownLeft) L->shownLeft = left; }          /* never goes back up */
    }
}
static void live_text(const LiveC *L, WCHAR *b, int cap) {          /* what is being done, in words */
    const LiveNote *n = &L->nt; int done = 0, k;
    if (!L->have) { wcscpy(b, L"starting ..."); return; }
    switch (stage_ix(n->stage)) {
    case 0: swprintf(b, cap, L"laying out the scene: %d pieces", n->pieces); break;
    case 1: if (n->ntile) { for (k = 0; k < n->ntile; k++) done += n->tdone[k]; swprintf(b, cap, L"height field: %d of %d tiles, %d thread%ls", done, n->ntile, n->threads, n->threads == 1 ? L"" : L"s"); }
            else if (n->steps_total) swprintf(b, cap, L"height field: step %d of %d (one core)", n->steps_done, n->steps_total);
            else wcscpy(b, L"height field"); break;
    case 2: wcscpy(b, L"making the mesh"); break;
    case 3: wcscpy(b, spGpu ? L"drawing the 3D picture (video card)" : L"drawing the 3D picture"); break;
    default: wcscpy(b, L"print check");
    }
}
static double live_frac(const LiveC *L, double t) {          /* how far this base is, 0..1; -1 = not known yet */
    if (!L || !L->have) return -1;
    if (L->expect > 0) { double f = (t - L->nt.t0) / L->expect; return f > 0.97 ? 0.97 : f; }
    if (!strcmp(L->nt.stage, "field") && L->nt.ntile) { int k, d = 0; for (k = 0; k < L->nt.ntile; k++) d += L->nt.tdone[k]; return 0.5 * d / L->nt.ntile + 0.4; }
    return -1;
}
static void live_left(const LiveC *L, double t, WCHAR *b, int cap) {          /* the time left, in words */
    double el = L && L->have ? t - L->nt.t0 : 0;
    if (!L || !L->have || L->expect <= 0) wcscpy(b, L"estimating ...");
    else if (el > L->expect) swprintf(b, cap, L"longer than usual: %d s so far", (int)el);
    else swprintf(b, cap, L"%d s left", (int)(L->shownLeft + 0.99));
}

static void free_cell(Cell *c) { if (c->img) GdipDisposeImage(c->img); if (c->img3) GdipDisposeImage(c->img3); if (c->img3c) GdipDisposeImage(c->img3c); live_free(c->lv); memset(c, 0, sizeof *c); }
static void drop_pending(void) {
    int i, k = 0;
    for (i = 0; i < ncell; i++) { if (cells[i].state == ST_PENDING) continue; if (k != i) cells[k] = cells[i]; k++; }
    for (i = k; i < ncell; i++) memset(&cells[i], 0, sizeof(Cell));
    ncell = k;
}

static void ask_choices(int fresh) {
    WCHAR cmd[1200], extra[400] = L""; int i, cnt = selCount;
    if (busy) return;
    if (fresh || !ncell) {
        if (fresh != 2) { for (i = 0; i < ncell; i++) free_cell(&cells[i]); ncell = 0; scrollY = 0; outFolder[0] = 0; }          /* 2: several sizes, the next size: the cards so far stay (review P2-6) */
        strcpy(curStyle, STYLES[selStyle]); strcpy(curSize, SIZES[selSize]); strcpy(curDetail, DETAILV[selDetail]); strcpy(curMini, selMini ? MINIS[selMini] : ""); curSup = selSup; curJoin = selJoin; { double o = GAMEMM[selScale] / 32.0 * 3.0 * OBJM[selObj]; snprintf(curObj, sizeof curObj, "%.3g", o > 1.0 ? o : 1.0); curFig = (int)GAMEMM[selScale]; }
    }
    if (ncell + cnt > MAXC) cnt = MAXC - ncell;
    if (cnt <= 0) { wcscpy(sub, L"That is as many choices as fit. Build some, or press Show new choices to start over."); redraw(); return; }
    if (curMini[0]) swprintf(extra, 80, L" --mini %hs", curMini);
    if (curSup) wcscat(extra, L" --supports on");
    swprintf(extra + wcslen(extra), 40, L" --obj %hs --figure %d", curObj, curFig);
    theme_opt(extra, 400); if (seedBuf[0]) swprintf(extra + wcslen(extra), 24, L" --seed %hs", seedBuf);
    for (i = 2; i < MF_N; i++) swprintf(extra + wcslen(extra), 24, L" --%hs %ls", MFA[i], mfOn[i] ? L"on" : L"off");          /* smooth details / organic rocks change the layout: the choices are drawn the same way */
    swprintf(cmd, 1200, L"choices %hs --size %hs --detail %hs --count %d%ls", curStyle, curSize, curDetail, cnt, extra);
    if (run(cmd, 1)) {
        for (i = 0; i < cnt; i++) { memset(&cells[ncell], 0, sizeof(Cell)); strcpy(cells[ncell].size, curSize); cells[ncell++].state = ST_PENDING; }
        wanted = cnt; doneN = 0; swprintf(head, 200, L"Drawing %d %hs bases to choose from", cnt, curStyle); swprintf(sub, 300, L"%hs mm. They appear as they are ready; this takes about half a minute.", curSize);
    }
    redraw();
}
static void build(void) {
    static WCHAR cmd[2600], picks[1800]; WCHAR extra[400] = L""; int i, k = 0;
    if (busy) return;
    picks[0] = 0;
    for (i = 0; i < ncell; i++) if (cells[i].picked && cells[i].state == ST_NONE) { swprintf(picks + wcslen(picks), 40, L"%ls%hs:%d", k++ ? L"," : L"", cells[i].seed, cells[i].n); }
    if (!k) return;
    if (curMini[0]) swprintf(extra, 80, L" --mini %hs", curMini);
    if (curSup) wcscat(extra, curJoin ? L" --supports on --join on" : L" --supports on --join off");
    swprintf(extra + wcslen(extra), 40, L" --obj %hs --figure %d", curObj, curFig);
    theme_opt(extra, 400); swprintf(extra + wcslen(extra), 48, L" --strength %hs --printer %hs", STRENGTHS[selStr], selPrinter ? "resin" : "fdm");
    for (i = 0; i < MF_N; i++) swprintf(extra + wcslen(extra), 24, L" --%hs %ls", MFA[i], mfOn[i] ? L"on" : L"off");          /* the MANIFOLD switches */
    if (batchFolder[0]) swprintf(extra + wcslen(extra), 80, L" --folder %ls", batchFolder);          /* several sizes: one folder for the batch (review P2-6 / N5) */
    swprintf(cmd, 2600, L"build %hs --size %hs --picks %ls%ls", curStyle, curSize, picks, extra);
    jobT0 = now_unix(); bigPick = -1; shrinkI = -1;          /* the live view: notes older than this build are not looked at */
    for (i = 0; i < ncell; i++) { live_free(cells[i].lv); cells[i].lv = NULL; }
    if (run(cmd, 2)) {
        SetTimer(hMain, 4, 100, NULL);          /* the build's progress notes, ten times a second */
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
    if (t[0] != L'{' || !jstr(t, L"ev", ev, 24)) { if (t[0]) logw(t); return; }          /* not a record: something the job said that is not a record (an error); keep it for Details */
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
        c = &cells[i]; memset(c, 0, sizeof *c); strcpy(c->size, curSize); c->state = ST_NONE; c->n = (int)jnum(t, L"n"); c->tier = (int)jnum(t, L"tier");
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
        if (c->lv) c->lv->tEnd = now_unix();          /* its row over time ends here */
        if (lvView == 2 && c - cells == bigNow) { shrinkI = (int)(c - cells); shrinkT = GetTickCount(); shrinkFrom = bigR; SetTimer(hMain, 5, 30, NULL); }          /* the Big view's base goes back down into its card */
        if (jtrue(t, L"ok") && jnum(t, L"secs") > 0) live_remember(liveTimes, curStyle, curSize, c->tier, jnum(t, L"secs"));          /* for the next time left */
        if (jtrue(t, L"ok")) {
            c->state = jtrue(t, L"variant") ? ST_VARIANT : ST_OK; c->secs = jnum(t, L"secs"); jstr(t, L"stl", c->stl, MAX_PATH); nOK++;
            if (jstr(t, L"img", path, MAX_PATH) && path[0]) GdipLoadImageFromFile(path, &c->img3);
            if (jstr(t, L"img3c", path, MAX_PATH) && path[0]) GdipLoadImageFromFile(path, &c->img3c);          /* the colour view (the COLOUR toggle) */
            c->col = colLast && c->img3c;
            if (jstr(t, L"img2", path, MAX_PATH) && path[0]) { GpImage *d2 = NULL; GdipLoadImageFromFile(path, &d2); if (d2) { if (c->img) GdipDisposeImage(c->img); c->img = d2; } }          /* the drawing of what was really built */
            if (c->state == ST_VARIANT) { WCHAR th[64]; if (jstr(t, L"theme", th, 64) && th[0]) wcscpy(c->theme, th); wcscpy(c->why, L"The picked design failed the print check, so the kit built a fresh variant in its place. Click to show its STL file."); }
        } else { c->state = ST_FAIL; jstr(t, L"why", c->why, 220); nBad++; }
        c->picked = 0; doneN++; swprintf(head, 200, L"Building: %d of %d done", doneN, wanted); redraw();
        if (selftest == 3 && !demoShot && doneN == 2) { WCHAR q[MAX_PATH]; demoShot = 1; anim = 9; swprintf(q, MAX_PATH, L"%ls.2_building.bmp", stShot); capture(q); }
        return;
    }
}
static void capture(const WCHAR *path);
/* the animation timer (1) runs while a job works; with the pixel look and textures it also runs, slower, while picked
   cards are on show, to move their marching ants */
static void anim_tick(void) {
    if (busy) return;
    if (lookPix && lookTex && npicked()) SetTimer(hMain, 1, 120, NULL); else KillTimer(hMain, 1);
}
static void on_ended(int mode, DWORD code) {
    int i;
    busy = 0; KillTimer(hMain, 1); KillTimer(hMain, 4); anim_tick();
    if (mode == 1) {
        drop_pending();
        if (ncell) { wcscpy(head, L"Click the bases you want"); wcscpy(sub, L"A teal tick marks a picked base. Then press Build. \"More choices\" adds another batch and keeps your picks."); }
        else { wcscpy(head, L"No choices came back"); swprintf(sub, 300, L"The job ended with code %lu. Press Details to see what it said.", code); }
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
    if (mode == 2 && nSizeQ > 0 && !runner_stop) {          /* several sizes: the next one, its cards added to these, into the same folder */
        if (!batchFolder[0] && outFolder[0]) { const WCHAR *bs = wcsrchr(outFolder, L'\\'); wcsncpy(batchFolder, bs ? bs + 1 : outFolder, 63); batchFolder[63] = 0; }
        selSize = sizeQ[0]; memmove(sizeQ, sizeQ + 1, sizeof(int) * (size_t)--nSizeQ); autoBuild = 1; ask_choices(2); if (!busy) { autoBuild = 0; nSizeQ = 0; batchFolder[0] = 0; } return; }
    if (mode == 2) batchFolder[0] = 0;          /* the batch is over */          /* several sizes: the next one */
    if (selftest == 5 && mode == 2) {          /* BBB_TEST_MULTI: several sizes built straight away; what came out */
        FILE *f; WCHAR q[MAX_PATH], pat[MAX_PATH]; int nstl = 0, nok = 0; WIN32_FIND_DATAW fd; HANDLE fh;
        for (i = 0; i < ncell; i++) nok += cells[i].state == ST_OK || cells[i].state == ST_VARIANT;
        swprintf(pat, MAX_PATH, L"%ls\\*.stl", outFolder); fh = FindFirstFileW(pat, &fd);
        if (fh != INVALID_HANDLE_VALUE) { do nstl++; while (FindNextFileW(fh, &fd)); FindClose(fh); }
        swprintf(q, MAX_PATH, L"%ls.multi.txt", stShot); f = _wfopen(q, L"w");
        if (f) { fprintf(f, "cards %d built %d stls_in_folder %d folder %ls\n", ncell, nok, nstl, outFolder); for (i = 0; i < ncell; i++) fprintf(f, "card %d size %s state %d\n", i, cells[i].size, cells[i].state); fclose(f); }
        PostQuitMessage(0); return;
    }
    if (mode == 1 && autoBuild) { autoBuild = 0; if (ncell) { for (i = 0; i < ncell; i++) if (cells[i].state == ST_NONE) cells[i].picked = 1; build(); return; } }
    if (selftest) {
        if (mode == 1 && stStep == 0 && ncell >= 3) { stStep = 1; cells[0].picked = 1; build(); }                                   /* ONE pick: must build exactly one */
        else if (mode == 2 && stStep == 1) { FILE *f; WCHAR p[MAX_PATH]; swprintf(p, MAX_PATH, L"%ls.txt", stShot); f = _wfopen(p, L"w");
               if (f) { fprintf(f, "one_pick_ok %d\none_pick_failed %d\n", nOK, nBad); fclose(f); }
               stStep = 2; cells[1].picked = cells[2].picked = 1; build(); }                                                        /* then two more */
        else { FILE *f; WCHAR p[MAX_PATH]; int built = 0;
               for (i = 0; i < ncell; i++) built += cells[i].state == ST_OK || cells[i].state == ST_VARIANT;
               int ncol = 0; hovK = H_NONE; capture(stShot);
               for (i = 0; i < ncell; i++) if (cells[i].img3c) { cells[i].col = 1; ncol++; }          /* the colour view on every built card: a second picture */
               if (ncol) { WCHAR q[MAX_PATH]; swprintf(q, MAX_PATH, L"%ls.colour.bmp", stShot); capture(q); }
               swprintf(p, MAX_PATH, L"%ls.txt", stShot); f = _wfopen(p, L"a");
               if (f) { fprintf(f, "choices %d\nbuilt_cells %d\ncolour_cells %d\ntwo_pick_ok %d\ntwo_pick_failed %d\nfolder %ls\n", ncell, built, ncol, nOK, nBad, outFolder); fclose(f); }
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
static GpSolidFill *fillB;          /* one brush for every plain fill: the pixel look fills many small rectangles */
static void fillrect(int x, int y, int w, int h, ARGB c) {
    if (w <= 0 || h <= 0) return;
    if (!fillB) GdipCreateSolidFill(c, &fillB); else GdipSetSolidFillColor(fillB, c);
    GdipFillRectangleI(G, (GpBrush *)fillB, x, y, w, h);
}
/* R3 (2026-11-22): a one-line text too long for its box is cut with "..." and kept here with its whole text; resting the
   mouse on it for half a second shows the whole text (drawn by the window itself, timer 7) */
typedef struct { RECT r; unsigned c; WCHAR s[80]; } TextRec;          /* F1 (2026-11-22): every text of the last paint, for the contrast test */
static TextRec texts[700]; static int ntext; static int hitClip;          /* hitClip: R4, see hit() */
static void text_rec(int x, int y, int w, int h, unsigned c, const WCHAR *s) { if (hitClip && x < hitClip >> 16 && y >= (hitClip & 0xFFFF)) return;          /* under the side panel's footer: not seen */
    if (ntext < 700) { SetRect(&texts[ntext].r, x, y, x + w, y + h); texts[ntext].c = c; wcsncpy(texts[ntext].s, s, 79); texts[ntext].s[79] = 0; ntext++; } }
typedef struct { RECT r; WCHAR s[200]; } Tip;
static Tip tips[96]; static int ntip, tipNo, tipCur = -1, tipShown, tipX, tipY; static WCHAR tipText[200];
static void txt(const WCHAR *s, int x, int y, int w, int h, HFONT f, ARGB c, UINT fl) {
    RECT r = { x, y + txtDy, x + w, y + h + txtDy }; GdipFlush(G, FlushIntentionSync);
    SelectObject(DC, f); SetTextColor(DC, RGB((c >> 16) & 255, (c >> 8) & 255, c & 255));
    if ((fl & DT_SINGLELINE) && !tipNo && w > 0) {
        SIZE sz; int n = (int)wcslen(s);
        if (GetTextExtentPoint32W(DC, s, n, &sz) && sz.cx > w) {
            fl |= DT_END_ELLIPSIS;
            if (ntip < 96) { tips[ntip].r = r; wcsncpy(tips[ntip].s, s, 199); tips[ntip].s[199] = 0; ntip++; }
        }
    }
    DrawTextW(DC, s, -1, &r, fl | DT_NOPREFIX);
    if (!tipNo) text_rec(r.left, r.top, r.right - r.left, r.bottom - r.top, c, s);
}
/* hitClip (declared above): R4, while the side panel's rows are drawn: the footer's top; a click area under it is cut there (the footer covers it) */
static void hit(int x, int y, int w, int h, int kind, int idx) { y += hitDy; if (hitClip && x < hitClip >> 16) { int cy = hitClip & 0xFFFF; if (y >= cy) return; if (y + h > cy) h = cy - y; } if (nhit < 400) { SetRect(&hits[nhit].r, x, y, x + w, y + h); hits[nhit].kind = kind; hits[nhit].idx = idx; nhit++; } }
static int hov(int kind, int idx) { return hovK == kind && hovI == idx; }

/* ---------- the pixel drawing kit: boxes with notched corners and a bevel, discs, dither textures made in code ----------
   Everything is whole art pixels (PX), filled rectangles only, no smoothing; the base pictures on the cards stay smooth. */
static void px_shape(int x, int y, int w, int h, int k, ARGB c) {          /* a filled box whose corners step in k art pixels */
    int j;
    if (w <= 0 || h <= 0 || !c) return;
    while (k > 0 && (2 * k * PX > w || 2 * k * PX > h)) k--;
    for (j = 0; j < k; j++) { int in = (k - j) * PX; fillrect(x + in, y + j * PX, w - 2 * in, PX, c); fillrect(x + in, y + h - (j + 1) * PX, w - 2 * in, PX, c); }
    fillrect(x, y + k * PX, w, h - 2 * k * PX, c);
}
/* a pixel box: an outline (edge, 0 = none) round a fill, lit top-left (hi) and shaded bottom-right (lo); 0 leaves one out */
static void px_box(double xd, double yd, double wd, double hd, int k, ARGB fill, ARGB edge, ARGB hi, ARGB lo) {
    int x = snp(xd), y = snp(yd), w = snp(xd + wd) - x, h = snp(yd + hd) - y, p = PX;
    if (w < 2 * p || h < 2 * p) { px_shape(x, y, w, h, 0, fill); return; }
    if (edge) { px_shape(x, y, w, h, k, edge); x += p; y += p; w -= 2 * p; h -= 2 * p; if (k) k--; }
    px_shape(x, y, w, h, k, fill);
    if (hi) { fillrect(x + k * p, y, w - 2 * k * p, p, hi); fillrect(x, y + k * p, p, h - 2 * k * p, hi); }
    if (lo) { fillrect(x + k * p, y + h - p, w - 2 * k * p, p, lo); fillrect(x + w - p, y + k * p, p, h - 2 * k * p, lo); }
}
static void px_disc(double cx, double cy, double r, ARGB c) {          /* a round disc, stepped in art pixels */
    int R = (int)(r / PX + 0.5), X = snp(cx), Y = snp(cy), j;
    for (j = -R; j < R; j++) { double yy = j + 0.5; int hw = (int)(sqrt(R * R - yy * yy) + 0.5); fillrect(X - hw * PX, Y + j * PX, 2 * hw * PX, PX, c); }
}
static void px_tick(double cx, double cy, ARGB c) {          /* the tick of a picked card, two art pixels thick */
    static const signed char pt[7][2] = { { -3, 0 }, { -2, 1 }, { -1, 2 }, { 0, 1 }, { 1, 0 }, { 2, -1 }, { 3, -2 } }; int i, X = snp(cx), Y = snp(cy);
    for (i = 0; i < 7; i++) fillrect(X + (pt[i][0] - 1) * PX, Y + (pt[i][1] - 1) * PX, PX, 2 * PX, c);
}
/* the 8 x 8 Bayer matrix: the order the dots of a dither come on in, so any level from 0 to 64 looks even */
static const unsigned char BAYER[64] = { 0, 32, 8, 40, 2, 34, 10, 42, 48, 16, 56, 24, 50, 18, 58, 26, 12, 44, 4, 36, 14, 46, 6, 38, 60, 28, 52, 20, 62, 30, 54, 22,
                                         3, 35, 11, 43, 1, 33, 9, 41, 51, 19, 59, 27, 49, 17, 57, 25, 15, 47, 7, 39, 13, 45, 5, 37, 63, 31, 55, 23, 61, 29, 53, 21 };
static unsigned hash2(int x, int y) { unsigned h = (unsigned)x * 374761393u + (unsigned)y * 668265263u; h = (h ^ (h >> 13)) * 1274126177u; return h ^ (h >> 16); }
typedef struct { int kind, lvl, px; ARGB c; GpBitmap *bm; GpTexture *t; } TexC;
static TexC texc[32]; static int ntexc;
/* a tiling texture brush, made once and kept: kind 0 Bayer dither of colour c at level lvl (of 64), kind 1 scanlines (every
   second art row c), kind 2 stone (low-contrast grain from value noise, dithered). Tiles start at the window's corner, so
   neighbouring fills join without a seam */
static GpTexture *tex_of(int kind, ARGB c, int lvl) {
    int i, n = kind == 2 ? 64 : 8, X, Y, x, y; GpBitmap *bm = NULL; GpTexture *t = NULL;
    for (i = 0; i < ntexc; i++) if (texc[i].kind == kind && texc[i].c == c && texc[i].lvl == lvl && texc[i].px == PX) return texc[i].t;
    if (ntexc == 32) { for (i = 0; i < ntexc; i++) { GdipDeleteBrush((GpBrush *)texc[i].t); GdipDisposeImage((GpImage *)texc[i].bm); } ntexc = 0; }
    GdipCreateBitmapFromScan0(n * PX, n * PX, 0, PixelFormat32bppARGB, NULL, &bm);
    if (!bm) return NULL;
    for (Y = 0; Y < n; Y++) for (X = 0; X < n; X++) {
        ARGB v = 0; int th = BAYER[(Y & 7) * 8 + (X & 7)];
        if (kind == 0) v = th < lvl ? c : 0;
        else if (kind == 1) v = (Y & 1) ? c : 0;
        else {          /* stone: smooth noise on an 8 x 8 lattice that wraps (64 art pixels a tile), plus grain, cut into light / none / dark */
            int gx = X / 8, gy = Y / 8; double fx = (X % 8) / 8.0, fy = (Y % 8) / 8.0, a, b, cc, d, nv, tt;
            fx = fx * fx * (3 - 2 * fx); fy = fy * fy * (3 - 2 * fy);
            a = (hash2(gx & 7, gy & 7) & 255) / 255.0; b = (hash2((gx + 1) & 7, gy & 7) & 255) / 255.0;
            cc = (hash2(gx & 7, (gy + 1) & 7) & 255) / 255.0; d = (hash2((gx + 1) & 7, (gy + 1) & 7) & 255) / 255.0;
            nv = (a + (b - a) * fx) * (1 - fy) + (cc + (d - cc) * fx) * fy; tt = nv * 0.6 + (hash2(X + 101, Y + 37) & 255) / 255.0 * 0.4;
            v = tt > 0.66 + th / 64.0 * 0.22 ? c : tt < 0.12 + th / 64.0 * 0.22 ? C_STONED : 0;
        }
        for (y = 0; y < PX; y++) for (x = 0; x < PX; x++) GdipBitmapSetPixel(bm, X * PX + x, Y * PX + y, v);
    }
    GdipCreateTexture((GpImage *)bm, WrapModeTile, &t);
    if (!t) { GdipDisposeImage((GpImage *)bm); return NULL; }
    texc[ntexc].kind = kind; texc[ntexc].c = c; texc[ntexc].lvl = lvl; texc[ntexc].px = PX; texc[ntexc].bm = bm; texc[ntexc].t = t; ntexc++;
    return t;
}
static void texfill(int x, int y, int w, int h, int kind, ARGB c, int lvl) { GpTexture *t; if (w > 0 && h > 0 && (t = tex_of(kind, c, lvl))) GdipFillRectangleI(G, (GpBrush *)t, x, y, w, h); }
/* the kit's boxes in the pixel look. st: 0 quiet, 1 teal (picked / the main button), 2 gold edge (Stop), 3 off, 4 sunken */
static void pbox(double x, double y, double w, double h, int k, int st, int hv) {
    switch (st) {
    case 1: px_box(x, y, w, h, k, hv ? C_ACCH : C_ACC, C_EDGE, C_TEALL, C_TEALM); break;
    case 2: px_box(x, y, w, h, k, hv ? C_CARDH : C_CARD, C_GOLD, hv ? C_BEVHH : C_BEVH, C_GOLDD); break;
    case 3: px_box(x, y, w, h, k, C_CARD, C_EDGE, 0, 0);
        if (lookTex) { int X = snp(x) + PX, Y = snp(y) + PX; texfill(X, Y, snp(x + w) - PX - X, snp(y + h) - PX - Y, 0, 0xFF1B1E25u, 24); }          /* an off button: dithered back, like a greyed-out old one */
        break;
    case 4: px_box(x, y, w, h, k, C_BG, hv ? C_ACCH : C_ACC, 0xFF0F1115u, 0xFF2A2F39u); break;
    default: px_box(x, y, w, h, k, hv ? C_CARDH : C_CARD, C_EDGE, hv ? C_BEVHH : C_BEVH, C_BEVD);
    }
}
static int mod8(int v) { return ((v % 8) + 8) % 8; }
/* one side of the marching ants: len art pixels from (x0, y0), dashes 4 on 4 off, at perimeter place p0 stepping by dir */
static void ant_side(int horiz, int x0, int y0, int len, int p0, int dir, ARGB c) {
    int i, run = -1;
    for (i = 0; i <= len; i++) {
        int on = i < len && mod8(p0 + dir * i - anim) < 4;
        if (on && run < 0) run = i;
        if (!on && run >= 0) { if (horiz) fillrect(x0 + run * PX, y0, (i - run) * PX, 2 * PX, c); else fillrect(x0, y0 + run * PX, 2 * PX, (i - run) * PX, c); run = -1; }
    }
}
static void ants(int xd, int yd, int wd, int hd) {          /* dark dashes walking clockwise round a picked card's teal outline (the animation timer moves them) */
    int x = snp(xd), y = snp(yd), W = (snp(xd + wd) - x) / PX, H = (snp(yd + hd) - y) / PX, lw = W - 4, lh = H - 4;
    if (lw < 1 || lh < 1) return;
    ant_side(1, x + 2 * PX, y, lw, 0, 1, C_TEALD);
    ant_side(0, x + (W - 2) * PX, y + 2 * PX, lh, lw, 1, C_TEALD);
    ant_side(1, x + 2 * PX, y + (H - 2) * PX, lw, 2 * lw + lh - 1, -1, C_TEALD);
    ant_side(0, x, y + 2 * PX, lh, 2 * lw + 2 * lh - 1, -1, C_TEALD);
}
static void ribbon(int xd, int yd, int wd, ARGB c) {          /* a built card: a stepped corner flag, top right */
    int xr = snp(xd + wd) - 2 * PX, yt = snp(yd) + 2 * PX, n = 7, j;
    for (j = 0; j < n; j++) { fillrect(xr - (n - j) * PX, yt + j * PX, (n - j) * PX, PX, j == n - 1 ? C_EDGE : c); }
}
static void px_spinner(double cx, double cy, double r) {          /* 8 blocks round a ring; the lit one walks clockwise with a fading trail */
    static const ARGB ramp[8] = { C_ACCH, C_ACC, C_TEALM, C_TEALD, C_NAVYH, C_NAVYH, C_NAVY, C_NAVY }; int i, b = 3 * PX, head = (anim / 2) & 7;
    for (i = 0; i < 8; i++) { double a = i * 3.14159265358979 / 4 - 3.14159265358979 / 2; fillrect(snp(cx + r * cos(a) - b / 2.0), snp(cy + r * sin(a) - b / 2.0), b, b, ramp[(head - i) & 7]); }
}

/* ---------- the pixel font: Silkscreen (SIL Open Font License 1.1, docs\fonts\OFL.txt), regular and bold ----------
   tools\bake_font.py bakes each face into a 1-bit atlas with its widths; bbb_kit.rc embeds both (RCDATA 201, 202), so
   the exe stays one file. A string is built letter by letter into a small picture at one font pixel per pixel and drawn
   scaled by a whole number with nearest-neighbour, so it stays crisp. Short text only: labels, chips, buttons, pills
   and the title; long text (help, Details, the line under the title, captions) stays Segoe UI. A text that does not
   fit its box at any pixel size is written in the plain font instead. */
typedef struct { int rows, base, cap, first, count, aw; const unsigned char *gl, *at; } PFont;
static PFont pf[2];          /* 0 regular, 1 bold */
static void pf_load(void) {
    int i;
    for (i = 0; i < 2; i++) {
        HRSRC r = FindResourceW(NULL, MAKEINTRESOURCEW(201 + i), (LPCWSTR)RT_RCDATA); HGLOBAL g; const unsigned char *p;
        if (!r || !(g = LoadResource(NULL, r)) || !(p = (const unsigned char *)LockResource(g)) || SizeofResource(NULL, r) < 14 || memcmp(p, "BPF1", 4)) continue;
        pf[i].rows = p[4]; pf[i].base = p[5]; pf[i].cap = p[6]; pf[i].first = p[7]; pf[i].count = p[8]; pf[i].aw = p[12] | p[13] << 8;
        pf[i].gl = p + 14; pf[i].at = p + 14 + 5 * pf[i].count;
    }
}
static const unsigned char *pf_glyph(const PFont *f, WCHAR ch) {          /* each: u16 atlas x, u8 ink width, s8 x offset, u8 advance */
    int k;
    if (ch == 0x2212) ch = L'-';          /* the minus sign of the number box */
    k = ch - f->first;
    if (k < 0 || k >= f->count) k = '?' - f->first;
    return f->gl + 5 * k;
}
static int pf_width(const PFont *f, const WCHAR *s) {          /* from the pen to the right edge of the last letter's ink */
    int pen = 0, right = 0;
    for (; *s; s++) { const unsigned char *g = pf_glyph(f, *s); if (g[2]) right = pen + (signed char)g[3] + g[2]; pen += g[4]; }
    return right;
}
/* which face and size a text gets: 1 regular x FS, 2 bold x FS, 3 regular x 2FS, 4 bold x 2FS; 0 the plain font.
   role 0: labels, chips, buttons (bold, else regular); 1: the main buttons (big if it fits); 2: numbers and titles, big bold first */
static const int ROLES[3][5] = { { 2, 1, 0 }, { 3, 2, 1, 0 }, { 4, 3, 2, 1, 0 } };
static int spec_fits(int sp, const WCHAR *s, int w) { const PFont *f = &pf[(sp - 1) & 1]; int sc = sp > 2 ? 2 * FS : FS; return f->gl && pf_width(f, s) * sc <= w - 2 * sc; }
static int need_w(const WCHAR *s, int minw) { int w; if (!lookFont || !pf[1].gl) return minw; w = pf_width(&pf[1], s) * FS + 2 * FS + S(18); return w > minw ? w : minw; }          /* a button wide enough for its text in the bold pixel font */
static int need_wa(const char *s, int minw) { WCHAR b[64]; swprintf(b, 64, L"%hs", s); return need_w(b, minw); }
static int footFit = 1;          /* the bottom bar's buttons sized to the pixel font (0: the window is too narrow for that, old sizes and the plain font) */
#define NW(s, m) (footFit ? need_w(s, m) : (m))
static int foot_need(void) { int f = footFit, n; footFit = 1; n = NW(L"20 picked", S(84)) + S(4) + NW(L"Pick all", S(84)) + S(8) + NW(L"Clear", S(70)) + S(8) + NW(L"More choices", S(118)) + S(24)
    + NW(L"Hide details", S(104)) + S(8) + NW(L"Open folder", S(110)) + S(10) + NW(L"Build the picked bases", S(220)) + S(44); footFit = f; return n; }          /* how wide the main area must be for the bottom bar */
static int pick_spec(int role, const WCHAR *s, int w) { const int *r; if (!lookFont) return 0; for (r = ROLES[role]; *r; r++) if (spec_fits(*r, s, w)) return *r; return 0; }
static void pf_draw(int sp, const WCHAR *s, int x, int y, int w, int h, ARGB c, UINT fl) {
    const PFont *f = &pf[(sp - 1) & 1]; int sc = sp > 2 ? 2 * FS : FS, tw = pf_width(f, s), bw = tw + 4, bh = f->rows, pen = 2, dx, dy, r, i; UINT32 *buf; GpBitmap *bm = NULL; const WCHAR *q;
    if (tw <= 0 || !(buf = (UINT32 *)calloc((size_t)bw * bh, 4))) return;
    for (q = s; *q; q++) {
        const unsigned char *g = pf_glyph(f, *q); int ax = g[0] | g[1] << 8, iw = g[2], xo = (signed char)g[3];
        for (r = 0; r < bh; r++) for (i = 0; i < iw; i++) if (f->at[r * f->aw + ax + i]) { int X = pen + xo + i; if (X >= 0 && X < bw) buf[r * bw + X] = c; }
        pen += g[4];
    }
    dx = fl & DT_CENTER ? x + (w - tw * sc) / 2 : fl & DT_RIGHT ? x + w - tw * sc : x;
    text_rec(x, y + txtDy, w, h, c, s);          /* F1: the pixel font's text too (its y on screen: the side panel's shift) */
    dy = y + (h - f->cap * sc + 1) / 2 - (f->base - f->cap) * sc;          /* the capitals centred in the box */
    GdipCreateBitmapFromScan0(bw, bh, bw * 4, PixelFormat32bppARGB, (BYTE *)buf, &bm);
    if (bm) {
        GdipSetInterpolationMode(G, InterpolationModeNearestNeighbor); GdipDrawImageRectI(G, (GpImage *)bm, dx - 2 * sc, dy, bw * sc, bh * sc);
        GdipSetInterpolationMode(G, InterpolationModeHighQualityBicubic); GdipDisposeImage((GpImage *)bm);
    }
    free(buf);
}
/* short text: the pixel font when it is on and the text fits (sp -1: choose for this text by role), else the plain font fo */
static void ltxt(const WCHAR *s, int x, int y, int w, int h, HFONT fo, ARGB c, UINT fl, int role, int sp) {
    if (sp < 0) sp = pick_spec(role, s, w);
    if (sp && lookFont) pf_draw(sp, s, x, y, w, h, c, fl); else txt(s, x, y, w, h, fo, c, fl);
    if (!sp && lookFont && role >= 0 && GetEnvironmentVariableA("BBB_FONT_MISS", NULL, 0)) {          /* a test: the texts that did not fit the pixel font */
        char e[MAX_PATH]; FILE *f; GetEnvironmentVariableA("BBB_FONT_MISS", e, MAX_PATH); f = fopen(e, "a"); if (f) { fprintf(f, "%d %d %ls\n", w, role, s); fclose(f); } }
}
static int chipSpec = -1; static int chipsMask; static unsigned chipsBits;          /* chipsMask: chips() shows every chip whose bit is set (several sizes) */          /* chips of one block share one font size, chosen so that every label of the block fits */
static void chip(const char *label, int x, int y, int w, int h, int on, int kind, int idx) {
    WCHAR b[200]; swprintf(b, 200, L"%hs", label);
    if (lookPix) pbox(x, y, w, h, 1, on ? 1 : 0, !on && hov(kind, idx));
    else rr(x, y, w, h, S(6), on ? C_ACC : hov(kind, idx) ? C_CARDH : C_CARD, on ? 0 : C_LINE, 1);
    ltxt(b, x, y, w, h, fUI, on ? C_ONACC : C_TEXT, DT_CENTER | DT_VCENTER | DT_SINGLELINE, 0, chipSpec); hit(x, y, w, h, kind, idx);
    if (on) {          /* F2 (2026-11-22): "on" is a shape too, not only a colour: a small dark square in the chip's top right corner */
        int d = lookPix ? 2 * PX : (S(4) > 2 ? S(4) : 2), in = lookPix ? 2 * PX : S(4);
        fillrect(x + w - in - d, y + in, d, d, C_ONACC);
    }
}
static void section(const WCHAR *label, int x, int y, int w, int h) {          /* the label over a block of the side panel */
    if (lookPix && lookTex) {          /* on a navy scanline bar with a teal grip */
        int X = snp(x), Y = snp(y), Wd = snp(x + w) - X, Hd = snp(y + h) - PX - Y, my = snp(Y + Hd / 2.0);
        fillrect(X, Y, Wd, Hd, C_NAVY); texfill(X, Y, Wd, Hd, 1, C_NAVYD, 0); fillrect(X, Y, Wd, PX, C_NAVYH);
        fillrect(X + 2 * PX, my - 3 * PX, PX, PX, C_ACC); fillrect(X + 2 * PX, my - PX, PX, PX, C_ACC); fillrect(X + 2 * PX, my + PX, PX, PX, C_ACC);
        ltxt(label, X + 5 * PX, Y, Wd - 6 * PX, Hd, fLabel, C_BAR, DT_LEFT | DT_VCENTER | DT_SINGLELINE, 0, -1);
    } else ltxt(label, x, y, w, h, fLabel, C_DIM, DT_LEFT | DT_SINGLELINE, 0, -1);
}
static int sideTight, sideNeed;          /* the side panel does not fit the window at full size: its chips are drawn a little shorter (sideNeed: its full height) */
static void chips(const WCHAR *label, const char **items, int n, int cols, int sel, int kind, int *y) {
    int x0 = S(16), wAll = S(SIDE_W - 32), gap = sideTight ? S(3) : S(4), ch = sideTight ? S(20) : S(24), cw = (wAll - (cols - 1) * gap) / cols, i; const int *r;
    while (cols > 1 && cw < S(34)) { cols--; cw = (wAll - (cols - 1) * gap) / cols; }          /* R3: a narrow panel: fewer chips a row, more rows */
    if (label) { section(label, x0, *y, wAll, S(18)); *y += sideTight ? S(19) : S(20); }          /* no label: a second block of the same choice */
    chipSpec = 0;
    if (lookFont) for (r = ROLES[0]; *r && !chipSpec; r++) { for (i = 0; i < n; i++) { WCHAR b[64]; swprintf(b, 64, L"%hs", items[i]); if (!spec_fits(*r, b, cw)) break; } if (i == n) chipSpec = *r; }
    for (i = 0; i < n; i++) chip(items[i], x0 + (i % cols) * (cw + gap), *y + (i / cols) * (ch + gap), cw, ch, chipsMask ? (int)((chipsBits >> (sel + i)) & 1u) : i == sel, kind, i);
    chipSpec = -1; *y += ((n + cols - 1) / cols) * (ch + gap) + (sideTight ? S(4) : S(6));
}
/* style: 0 quiet, 1 the main button (teal), 2 gold outline, 3 off */
static void button(const WCHAR *label, int x, int y, int w, int h, int style, int kind) {
    int hv = hov(kind, 0) && style != 3;
    if (lookPix) pbox(x, y, w, h, 1, style, hv);
    else if (style == 1) rr(x, y, w, h, S(8), hv ? C_ACCH : C_ACC, 0, 0);
    else if (style == 2) rr(x, y, w, h, S(8), hv ? C_CARDH : C_CARD, C_GOLD, S(2));
    else rr(x, y, w, h, S(8), hv ? C_CARDH : C_CARD, C_LINE, 1);
    ltxt(label, x, y, w, h, style == 1 ? fBtn : fUI, style == 1 ? C_ONACC : style == 2 ? C_GOLD : style == 3 ? C_OFF : C_TEXT, DT_CENTER | DT_VCENTER | DT_SINGLELINE, style == 1 || style == 2 ? 1 : 0, -1);
    if (style != 3) hit(x, y, w, h, kind, 0);
}
static void pill(const WCHAR *s, int x, int y, int w, int h, ARGB bg, ARGB ink) {
    int sp = pick_spec(0, s, w);
    if (lookPix) px_box(x, y, w, h, 1, bg, 0, 0, 0); else rr(x, y, w, h, h / 2.0, bg, 0, 0);
    if (sp) pf_draw(sp, s, x, y, w, h, ink, DT_CENTER); else txt(s, x, y, w, h, fSmall, ink, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
}
static void image_fit(GpImage *im, double x, double y, double w, double h) {
    UINT iw = 1, ih = 1; double s, dw, dh; GdipGetImageWidth(im, &iw); GdipGetImageHeight(im, &ih);
    s = w / iw < h / ih ? w / iw : h / ih; dw = iw * s; dh = ih * s; GdipDrawImageRect(G, im, (REAL)(x + (w - dw) / 2), (REAL)(y + (h - dh) / 2), (REAL)dw, (REAL)dh);
}
/* the oval of the session's base size: long / short (1 = round). Oval cards are wide boxes, their pictures cropped to the oval */
static double oval_ratio(void) { double a = 0, b = 0; if (sscanf(curSize[0] ? curSize : SIZES[selSize], "%lfx%lf", &a, &b) == 2 && a > 0 && b > 0) return a > b ? a / b : b / a; return 1.0; }          /* long over short (a rectangle is written short x long) */
static void image_box(GpImage *im, double x, double y, double w, double h, double ovr) {          /* a picture in a box; an oval's square drawing cropped to the oval's band */
    UINT iw = 1, ih = 1; double sh, s, dw, dh, sy;
    if (ovr <= 1.01) { image_fit(im, x, y, w, h); return; }
    GdipGetImageWidth(im, &iw); GdipGetImageHeight(im, &ih);
    sh = ih; sy = 0;
    if (iw && ih && fabs((double)iw / ih - 1) < 0.05) { sh = ih / ovr * 1.08; if (sh > ih) sh = ih; sy = (ih - sh) / 2; }          /* a square drawing from above: just the oval's band */
    s = w / iw < h / sh ? w / iw : h / sh; dw = iw * s; dh = sh * s;
    GdipDrawImageRectRect(G, im, (REAL)(x + (w - dw) / 2), (REAL)(y + (h - dh) / 2), (REAL)dw, (REAL)dh, 0, (REAL)sy, (REAL)iw, (REAL)sh, UnitPixel, NULL, NULL, NULL);
}
static void meter_bar(int x, int y, int w, int h, double f, ARGB c);
static const ARGB STAGEC[5] = { C_NAVYH, C_ACC, C_GREEN, C_GOLD, C_TEALL };          /* layout, height field, mesh, picture, check */
/* the base taking shape, in a box: while laying out, the pieces' outlines as they are placed; then the build's own drawing from
   above, with the height field's tiles over it, dimmed until a thread has finished them */
static void live_pic(const LiveC *L, int x, int y, int w, int h) {
    GpRegion *old = NULL; const LiveNote *n = &L->nt; double ovr = n->ovl > 0 && n->ovs > 0 ? n->ovl / n->ovs : 1.0;
    GdipCreateRegion(&old); GdipGetClip(G, old); GdipSetClipRectI(G, x, y, w, h, CombineModeIntersect);
    fillrect(x, y, w, h, 0xFF101216u);
    if (L->plan) {
        double D = w * 1.03 < h * 1.03 * ovr ? w * 1.03 : h * 1.03 * ovr, dx = x + (w - D) / 2, dy = y + (h - D) / 2; int k, st = stage_ix(n->stage);
        GdipDrawImageRect(G, L->plan, (REAL)dx, (REAL)dy, (REAL)D, (REAL)D);
        if (st == 1 && n->ntile && n->pb[2] > n->pb[0]) {          /* the tiles still to do, dimmed, with a teal edge */
            double sx = D / (n->pb[2] - n->pb[0]), sy = D / (n->pb[3] - n->pb[1]); GpPen *pn = NULL;
            GdipCreatePen1(C_TEALD, (REAL)(lookPix ? PX : 1), UnitPixel, &pn);
            for (k = 0; k < n->ntile; k++) {
                const float *q = n->tile[k]; double ax = dx + (q[0] - n->pb[0]) * sx, ay = dy + (n->pb[3] - q[3]) * sy, aw = (q[2] - q[0]) * sx, ah = (q[3] - q[1]) * sy;
                if (n->tdone[k]) continue;
                fillrect((int)ax, (int)ay, (int)(aw + 0.5), (int)(ah + 0.5), 0xB0101216u);
                if (pn) GdipDrawRectangle(G, pn, (REAL)ax, (REAL)ay, (REAL)aw, (REAL)ah);
            }
            if (pn) GdipDeletePen(pn);
        } else if (st == 1 && n->steps_total) {          /* the one-core field: no tiles, step after step; dim what is still to come */
            double f = (double)n->steps_done / n->steps_total; int yy = (int)(y + h * f);
            fillrect(x, yy, w, y + h - yy, 0xA0101216u); fillrect(x, yy, w, lookPix ? PX : S(2), C_ACC);
        }
    } else if (L->npoly) {          /* the layout so far: the ground, raised pieces light, cuts dark */
        double R = (n->lay_r > 0 ? n->lay_r : 15) + 1, side = w < h ? w : h, sc = side / (2 * R), cx = x + w / 2.0, cy = y + h / 2.0; int k, j; GpSolidFill *br[2] = { NULL, NULL }; GpPen *pn = NULL;
        static GpPointF pt[64];
        if (lookPix) px_disc(cx, cy, R * sc, 0xFF3B342Cu); else rr(cx - R * sc, cy - R * sc, 2 * R * sc, 2 * R * sc, R * sc, 0xFF3B342Cu, 0, 0);
        GdipCreateSolidFill(0xFFA8977Au, &br[0]); GdipCreateSolidFill(0xFF1B1714u, &br[1]); GdipCreatePen1(0xFF6B5D49u, 1, UnitPixel, &pn);
        for (k = 0; k < L->npoly; k++) {
            int a = L->pstart[k], m = (L->pstart[k + 1] - a) / 2; if (m > 64) m = 64;
            for (j = 0; j < m; j++) { pt[j].X = (REAL)(cx + L->pts[a + 2 * j] * sc); pt[j].Y = (REAL)(cy - L->pts[a + 2 * j + 1] * sc); }
            GdipFillPolygon(G, (GpBrush *)br[L->pkind[k] == 'c'], pt, m, FillModeAlternate);
            if (L->pkind[k] != 'c' && pn) GdipDrawPolygon(G, pn, pt, m);
        }
        GdipDeleteBrush((GpBrush *)br[0]); GdipDeleteBrush((GpBrush *)br[1]); if (pn) GdipDeletePen(pn);
    }
    GdipSetClipRegion(G, old, CombineModeReplace); GdipDeleteRegion(old);
}
/* rows over time: one row per base of this build, coloured by what it was doing when (layout, height field, mesh, picture, check) */
static int rows_count(void) { int i, k = 0; for (i = 0; i < ncell; i++) k += cells[i].lv && cells[i].lv->nseg; return k > 12 ? 12 : k; }
static void rows(int x, int y, int w, int h) {
    static const WCHAR *nm[5] = { L"laying out", L"height field", L"mesh", L"picture", L"check" };
    int i, k = 0, lw = S(150), j, lx = x, most = (h - S(20)) / S(14); double t = now_unix(), t0 = jobT0, t1 = t, rh = S(14);
    if (t1 < t0 + 10) t1 = t0 + 10;
    if (most > 12) most = 12;
    for (j = 0; j < 5; j++) {          /* the key */
        fillrect(lx, y + S(5), S(10), S(8), STAGEC[j]); txt(nm[j], lx + S(14), y, S(90), S(18), fSmall, C_DIM, DT_LEFT | DT_SINGLELINE); lx += S(100);
    }
    y += S(20);
    for (i = 0; i < ncell && k < most; i++) {
        const LiveC *L = cells[i].lv; double ry = y + k * rh;
        if (!L || !L->nseg) continue;
        txt(cells[i].theme, x, (int)ry - S(2), lw - S(8), (int)rh + S(2), fSmall, C_TEXT, DT_LEFT | DT_SINGLELINE | DT_END_ELLIPSIS);
        fillrect(x + lw, (int)ry + S(2), w - lw, (int)rh - S(4), 0xFF1E2128u);
        for (j = 0; j < L->nseg; j++) {
            double a = L->segT[j], b = j + 1 < L->nseg ? L->segT[j + 1] : (L->tEnd > 0 ? L->tEnd : t);
            int xa = x + lw + (int)((a - t0) / (t1 - t0) * (w - lw)), xb = x + lw + (int)((b - t0) / (t1 - t0) * (w - lw));
            fillrect(xa, (int)ry + S(2), xb - xa > 1 ? xb - xa : 1, (int)rh - S(4), STAGEC[L->segS[j]]);
        }
        k++;
    }
}
static void card(int i, int x, int y, int w, int h) {
    Cell *c = &cells[i]; int pad = S(10), iw = w - 2 * pad, ih = h - 2 * pad - S(40), hv = hov(H_CARD, i) && !busy, capY = y + pad + ih + S(6); WCHAR b[160]; double ovr = oval_ratio();
    SetRect(&cardR[i], x, y, x + w, y + h);
    if (c->state == ST_PENDING) {          /* a grey card that breathes while its picture is being drawn */
        ARGB a = (ARGB)(150 + (int)(70 * (0.5 + 0.5 * sin((anim + i * 3) * 0.18)))); double rd = (iw < ih ? iw : ih) / 2.0;
        if (lookPix) {
            px_box(x, y, w, h, 2, (a << 24) | (C_CARD & 0xFFFFFF), 0, 0, 0); px_disc(x + pad + iw / 2.0, y + pad + ih / 2.0, rd, ((a / 6) << 24) | 0xFFFFFF);
            px_box(x + pad, capY + S(4), iw * 0.6, S(10), 0, 0x18FFFFFF, 0, 0, 0); return;
        }
        rr(x, y, w, h, S(12), (a << 24) | (C_CARD & 0xFFFFFF), 0, 0); rr(x + pad, y + pad, iw, ih, rd, ((a / 6) << 24) | 0xFFFFFF, 0, 0);
        rr(x + pad, capY + S(4), iw * 0.6, S(10), S(5), 0x18FFFFFF, 0, 0); return;
    }
    if (!lookPix) rr(x, y, w, h, S(12), hv ? C_CARDH : C_CARD, c->picked ? C_ACC : hv ? 0xFF4A5060u : C_LINE, c->picked ? S(3) : 1);
    else if (c->picked) {          /* picked: a teal outline two art pixels thick (with marching ants when textures are on) */
        px_box(x, y, w, h, 2, C_ACC, 0, 0, 0); px_box(x + PX, y + PX, w - 2 * PX, h - 2 * PX, 2, hv ? C_CARDH : C_CARD, C_ACC, C_BEVH, C_BEVD);
        if (lookTex) ants(x, y, w, h);
    } else px_box(x, y, w, h, 2, hv ? C_CARDH : C_CARD, hv ? 0xFF4A5060u : C_EDGE, hv ? C_BEVHH : C_BEVH, C_BEVD);
    if ((c->state == ST_OK || c->state == ST_VARIANT) && c->img3) {          /* built: the 3D picture, with its 2D drawing small in the corner to check one against the other */
        GpImage *p3 = c->col && c->img3c ? c->img3c : c->img3;          /* the COLOUR toggle: the same picture coloured as the 2D drawing */
        if (ovr > 1.01) image_fit(p3, x + pad, y + pad, iw, ih); else image_fit(p3, x + pad, y + pad + ih * 0.12, iw, ih * 0.88);
        if (c->img) { double d = (iw < ih ? iw : ih) * 0.36, dw = ovr > 1.01 ? d * ovr : d;
            if (lookPix) px_box(x + pad - PX, y + pad - PX, dw + 2 * PX, d + 2 * PX, 1, C_CARD, C_LINE, 0, 0); else rr(x + pad - 1, y + pad - 1, dw + 2, d + 2, d / 2 + 1, C_CARD, C_LINE, 1);
            image_box(c->img, x + pad, y + pad, dw, d, ovr); }
    }
    else if (c->img) image_box(c->img, x + pad, y + pad, iw, ih, ovr);
    if (lookPix && lookTex && (c->state == ST_OK || c->state == ST_VARIANT)) ribbon(x, y, w, c->state == ST_OK ? C_ACC : C_GOLD);
    if (c->state == ST_BUILDING) {
        const LiveC *L = c->lv; double t = now_unix(), f = live_frac(L, t);
        if (lvView >= 1 && lvShape && L && L->have && (L->plan || L->npoly)) live_pic(L, x + pad, y + pad, iw, ih);          /* Live card: the base taking shape */
        else if (lookPix) {          /* dimmed, with blocks walking round */
            px_disc(x + pad + iw / 2.0, y + pad + ih / 2.0, (iw < ih ? iw : ih) / 2.0, 0xA0101216); px_spinner(x + w / 2.0, y + pad + ih / 2.0, S(24));
        } else {          /* dimmed, with a turning arc */
            GpPen *pn; double cx = x + w / 2.0, cy = y + pad + ih / 2.0, r = S(18);
            rr(x + pad, y + pad, iw, ih, (iw < ih ? iw : ih) / 2.0, 0xA0101216, 0, 0);
            GdipCreatePen1(C_ACC, (REAL)S(4), UnitPixel, &pn); GdipSetPenStartCap(pn, LineCapRound); GdipSetPenEndCap(pn, LineCapRound);
            GdipDrawArc(G, pn, (REAL)(cx - r), (REAL)(cy - r), (REAL)(2 * r), (REAL)(2 * r), (REAL)((anim * 14) % 360), 260); GdipDeletePen(pn);
        }
        if (lvTime && L && L->have) {          /* the time left: a bar along the bottom of the picture, the seconds over it */
            int by = y + pad + ih - S(10); WCHAR tl[64];
            live_left(L, t, tl, 64);
            fillrect(x + pad, by - S(18), iw, S(28), 0xC0101216u);
            if (f >= 0) meter_bar(x + pad + S(6), by, iw - S(12), S(6), f, C_ACC);
            else if (lookPix) { int k; for (k = 0; k < 6; k++) fillrect(x + pad + S(6) + ((anim * 3 + k * 9) % (iw - S(20))), by, S(6), S(6), C_TEALM); }          /* no estimate yet: blocks walking */
            else { double p = (anim % 40) / 40.0; meter_bar(x + pad + S(6) + (int)(p * (iw - S(60))), by, S(48), S(6), 1, C_TEALM); }
            txt(tl, x + pad + S(6), by - S(18), iw - S(12), S(16), fSmall, C_TEXT, DT_RIGHT | DT_SINGLELINE);
        }
    }
    if (c->picked && lookPix) { px_disc(x + w - S(22), y + S(22), S(12), C_ACC); px_tick(x + w - S(22), y + S(22), C_ONACC); }
    else if (c->picked) {          /* the tick */
        GpSolidFill *bf; GpPen *pn; double cx = x + w - S(22), cy = y + S(22), r = S(12); GpPointF pt[3];
        GdipCreateSolidFill(C_ACC, &bf); GdipFillEllipse(G, (GpBrush *)bf, (REAL)(cx - r), (REAL)(cy - r), (REAL)(2 * r), (REAL)(2 * r)); GdipDeleteBrush((GpBrush *)bf);
        GdipCreatePen1(C_ONACC, (REAL)S(2.6), UnitPixel, &pn); GdipSetPenStartCap(pn, LineCapRound); GdipSetPenEndCap(pn, LineCapRound); GdipSetPenLineJoin(pn, LineJoinRound);
        pt[0].X = (REAL)(cx - r * 0.45); pt[0].Y = (REAL)cy; pt[1].X = (REAL)(cx - r * 0.1); pt[1].Y = (REAL)(cy + r * 0.38); pt[2].X = (REAL)(cx + r * 0.5); pt[2].Y = (REAL)(cy - r * 0.35);
        GdipDrawLines(G, pn, pt, 3); GdipDeletePen(pn);
    }
    if (sizeMulti && c->size[0]) { WCHAR cap[100]; swprintf(cap, 100, L"%hs mm  %ls", c->size, c->theme); txt(cap, x + pad, capY, iw, S(18), fUI, C_TEXT, DT_LEFT | DT_SINGLELINE | DT_END_ELLIPSIS); }          /* several sizes: each card says its size */
    else txt(c->theme, x + pad, capY, iw, S(18), fUI, C_TEXT, DT_LEFT | DT_SINGLELINE | DT_END_ELLIPSIS);
    switch (c->state) {
    case ST_BUILDING: {          /* what is being done, in words (every view) */
        WCHAR st[120]; if (c->lv && c->lv->have) live_text(c->lv, st, 120); else wcscpy(st, L"building ...");
        if (c->retry) { WCHAR r2[160]; swprintf(r2, 160, L"new variant: %ls", st); wcscpy(st, r2); }
        txt(st, x + pad, capY + S(20), iw, S(18), fSmall, C_ACC, DT_LEFT | DT_SINGLELINE | DT_END_ELLIPSIS); break; }
    case ST_OK: swprintf(b, 160, L"BUILT  %.0f s", c->secs); pill(b, x + pad, capY + S(20), S(96), S(18), C_ACC, C_ONACC); break;
    case ST_VARIANT: { int pw = c->img3c ? iw - S(86) : iw; pill(L"built as a new variant", x + pad, capY + S(20), pw < S(160) ? pw : S(160), S(18), C_GOLD, C_ONGOLD); break; }          /* room for the COLOUR toggle */
    case ST_FAIL: pill(L"not built: failed the check", x + pad, capY + S(20), iw < S(180) ? iw : S(180), S(18), C_GOLD, C_ONGOLD); break;
    default: swprintf(b, 160, L"detail %d of 4", c->tier); txt(b, x + pad, capY + S(20), iw, S(18), fSmall, C_DIM, DT_LEFT | DT_SINGLELINE);
    }
    { RECT cr = { x, y, x + w, y + h }, vis; if (IntersectRect(&vis, &cr, &gridR)) hit(vis.left, vis.top, vis.right - vis.left, vis.bottom - vis.top, H_CARD, i); }          /* only the part of the card that shows (not under the bottom bar)  */
    if ((c->state == ST_OK || c->state == ST_VARIANT) && c->img3c) {          /* the colour view toggle, beside the base's BUILT label (its hit is after the card's, so it wins) */
        int tw = S(78), th = S(18); chip("COLOUR", x + w - pad - tw, capY + S(20), tw, th, c->col, H_COL, i);
    }
}
static void help(int W, int H);
/* the strip under the top bar: the PC's meters on the left (Small: one CPU and one video card bar; Live card and Big view:
   one thin bar per core), the three views and the Parts button on the right */
static const char *VIEWS[3] = { "Small", "Live card", "Big view" };
static RECT partsR; static int lookW = 238, lookWd = 238;          /* lookWd: the same in design px (a resize does not make it stale) */          /* how wide the three look switches came out (the title stops short of them) */          /* where the Parts button is, for its panel */
static void meter_bar(int x, int y, int w, int h, double f, ARGB c) {
    if (lookPix) { px_box(x, y, w, h, 0, 0xFF262A33u, 0, 0, 0); if (f > 0) px_box(x, y, w * (f > 1 ? 1 : f), h, 0, c, 0, 0, 0); }
    else { rr(x, y, w, h, h / 2.0, 0xFF262A33u, 0, 0); if (f > 0.02) rr(x, y, w * (f > 1 ? 1 : f), h, h / 2.0, c, 0, 0); }
}
static void strip(int x, int y, int w, int h) {
    int cw = S(78), gap = S(4), pw = need_w(L"Parts", S(64)), rx = x + w - pw, i, gx, vl = need_w(L"VIDEO CARD", S(80)) - S(10); WCHAR b[64];
    for (i = 0; i < 3; i++) cw = need_wa(VIEWS[i], cw);
    chipSpec = -1;
    button(L"Parts", rx, y, pw, h, lvParts ? 1 : 0, H_PARTS); SetRect(&partsR, rx, y, rx + pw, y + h);
    for (i = 2; i >= 0; i--) { rx -= cw + gap; chip(VIEWS[i], rx, y, cw, h, lvView == i, H_VIEW, i); }
    rx -= S(14); gx = rx;
    if (lvGpu && rx - x > vl + S(116) + need_w(L"OUR CORES", S(64)) + S(240)) {          /* the video card: its 3D engine, every program on the PC (only with room left for our cores) */
        gx = rx - S(116) - vl;
        ltxt(L"VIDEO CARD", gx, y, vl, h, fLabel, C_DIM, DT_LEFT | DT_VCENTER | DT_SINGLELINE, 0, -1);
        if (gpuV < 0) txt(L"not available", gx + vl + S(2), y, S(110), h, fSmall, C_DIM, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
        else { meter_bar(gx + vl + S(2), y + h / 2 - S(5), S(70), S(10), gpuV, C_GOLD); swprintf(b, 64, L"%d%%", (int)(gpuV * 100 + 0.5)); txt(b, gx + vl + S(76), y, S(40), h, fSmall, C_TEXT, DT_LEFT | DT_VCENTER | DT_SINGLELINE); }
        hit(gx, y, vl + S(116), h, H_PARTS, 12);
    }
    if (lvCores && ncpu) {          /* our own threads only: one bar each that is working, busiest first; idle ones are not shown */
        int lw = need_w(L"OUR CORES", S(64)) - S(8), ax = x + lw, aw = gx - S(120) - ax, n = nthr;          /* room for "12 of 36 cores" after the bars */
        ltxt(L"OUR CORES", x, y, lw, h, fLabel, C_DIM, DT_LEFT | DT_VCENTER | DT_SINGLELINE, 0, -1);
        if (!busy && !n) txt(L"idle", ax, y, S(80), h, fSmall, C_DIM, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
        else if (lvView == 0 || aw < n * 3) {          /* Small (or too narrow for a bar each): one bar, how many of the PC's cores we use */
            int mw2 = aw < S(160) ? aw : S(160);
            meter_bar(ax, y + h / 2 - S(5), mw2, S(10), jobCores / ncpu, C_ACC);
            swprintf(b, 64, L"%.1f of %d cores", jobCores, ncpu); txt(b, ax + mw2 + S(6), y, S(110), h, fSmall, C_TEXT, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
        } else {          /* one thin bar per working thread, filled from the bottom by how busy it is */
            int step = n ? aw / n : aw, bw = step - (step > 4 ? 2 : 1), bh = h - S(4), by = y + S(2);
            if (bw > S(8)) { bw = S(8); step = bw + S(3); }
            if (lookPix) { bw = bw / PX * PX; if (bw < PX) bw = PX; }
            for (i = 0; i < n; i++) {
                int bx = ax + i * step, fh = (int)(bh * cpuV[i] + 0.5);
                if (lookPix) fh = (fh + PX / 2) / PX * PX;
                fillrect(bx, by, bw, bh, 0xFF262A33u);
                if (fh > 0) fillrect(bx, by + bh - fh, bw, fh, cpuV[i] > 0.85 ? C_ACCH : C_ACC);
            }
            swprintf(b, 64, L"%d of %d cores", n, ncpu); txt(b, ax + n * step + S(4), y, S(110), h, fSmall, C_TEXT, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
        }
        hit(x, y, gx - x, h, H_PARTS, 11);
    }
}
static const char *PARTS[5] = { "Core bars", "Video card meter", "Base taking shape", "Time left", "Rows over time" };
static int *part_v(int i) { int *v[5] = { &lvCores, &lvGpu, &lvShape, &lvTime, &lvRows }; return v[i]; }
static void parts_panel(int W, int H) {          /* the five parts, each its own switch, under the Parts button */
    int pw = S(220), ph = S(20) + 5 * S(30) + S(10), px = partsR.right - pw, py = partsR.bottom + S(6), i;
    hit(0, 0, W, H, H_PARTS, 0);          /* a click anywhere else closes it */
    if (lookPix) px_box(px, py, pw, ph, 2, C_SIDE, C_EDGE, C_BEVH, C_BEVD); else rr(px, py, pw, ph, S(10), C_SIDE, C_LINE, 1);
    ltxt(L"SHOW WHILE BUILDING", px + S(12), py + S(6), pw - S(24), S(16), fLabel, C_DIM, DT_LEFT | DT_SINGLELINE, 0, -1);
    for (i = 0; i < 5; i++) chip(PARTS[i], px + S(12), py + S(26) + i * S(30), pw - S(24), S(24), *part_v(i), H_PARTS, i + 1);
}
/* the Big view: the base being built (the oldest one, or the one clicked) fills the middle, what is being done and the time
   left beside it, the rows over time under it, and every card of this build small in one row at the bottom. When the base is
   done it shrinks back into its card (paint's overlay) and the next one building takes its place */
static void big_view(int x0, int y0, int w0, int h0, int nrw) {
    int pad = S(18), gap = S(12), aw = w0 - 2 * pad, n = ncell, cwm, ihm, chm, rh = nrw && lvRows ? S(28) + nrw * S(14) : 0, bx = x0 + pad, by = y0 + pad, bw = aw, bh, pw, i, mx, off = 0, total;
    double ovr = oval_ratio(), t = now_unix(); Cell *c = &cells[bigNow]; const LiveC *L = c->lv; WCHAR b[160];
    cwm = n ? (aw - (n - 1) * gap) / n : aw;
    if (cwm > S(170)) cwm = S(170);
    if (cwm < S(104)) cwm = S(104);
    ihm = ovr > 1.01 ? (int)((cwm - 2 * S(10)) / ovr * 1.08) : cwm - 2 * S(10); chm = ihm + 2 * S(10) + S(40);
    bh = h0 - 2 * pad - chm - gap - (rh ? rh + gap : 0);
    if (bh < S(120)) bh = S(120);
    pw = (int)(bh * (ovr > 1.01 ? ovr : 1.0)); if (pw > bw * 0.66) pw = (int)(bw * 0.66);          /* the picture: as large as fits, the words beside it */
    GdipSetClipRectI(G, x0, y0, w0, h0, CombineModeReplace);
    if (lookPix) px_box(bx, by, bw, bh, 2, C_CARD, C_ACC, C_BEVH, C_BEVD); else rr(bx, by, bw, bh, S(14), C_CARD, C_ACC, S(2));
    SetRect(&bigR, bx, by, bx + pw, by + bh);
    if (L && L->have && lvShape && (L->plan || L->npoly)) live_pic(L, bx + S(12), by + S(12), pw - S(24), bh - S(24));
    else {
        if (c->img) image_box(c->img, bx + S(12), by + S(12), pw - S(24), bh - S(24), ovr);
        if (lookPix) px_spinner(bx + pw / 2.0, by + bh / 2.0, S(30)); else { GpPen *pn; double r = S(24); GdipCreatePen1(C_ACC, (REAL)S(5), UnitPixel, &pn); GdipDrawArc(G, pn, (REAL)(bx + pw / 2.0 - r), (REAL)(by + bh / 2.0 - r), (REAL)(2 * r), (REAL)(2 * r), (REAL)((anim * 14) % 360), 260); GdipDeletePen(pn); }
    }
    {          /* the words beside it */
        int tx = bx + pw + S(10), tw = bw - pw - S(26), ty = by + S(18);
        txt(c->theme, tx, ty, tw, S(28), fBig, C_TEXT, DT_LEFT | DT_SINGLELINE | DT_END_ELLIPSIS); ty += S(30);
        swprintf(b, 160, L"design %hs-%d, detail %d of 4", c->seed, c->n, c->tier); txt(b, tx, ty, tw, S(18), fSmall, C_DIM, DT_LEFT | DT_SINGLELINE); ty += S(30);
        if (L && L->have) live_text(L, b, 160); else wcscpy(b, L"starting ...");
        section(L"NOW", tx, ty, tw, S(18)); ty += S(22); txt(b, tx, ty, tw, S(40), fUI, C_ACC, DT_LEFT | DT_WORDBREAK); ty += S(44);
        if (lvTime) {
            double f = live_frac(L, t);
            section(L"TIME LEFT", tx, ty, tw, S(18)); ty += S(22);
            live_left(L, t, b, 160); txt(b, tx, ty, tw, S(26), fBig, C_TEXT, DT_LEFT | DT_SINGLELINE); ty += S(30);
            if (f >= 0) meter_bar(tx, ty, tw, S(10), f, C_ACC); ty += S(24);
        }
        if (L && L->have && L->nt.threads) { swprintf(b, 160, L"this base: %d thread%ls for the height field and the mesh", L->nt.threads, L->nt.threads == 1 ? L"" : L"s"); txt(b, tx, ty, tw, S(36), fSmall, C_DIM, DT_LEFT | DT_WORDBREAK); ty += S(36); }
        swprintf(b, 160, L"%d of %d bases done. Click another building card to watch it.", doneN, wanted); txt(b, tx, by + bh - S(46), tw, S(36), fSmall, C_DIM, DT_LEFT | DT_WORDBREAK);
    }
    if (rh) rows(bx, by + bh + gap, bw, rh);
    {          /* every card of this build, small, in one row (slid so the one shown stays in view) */
        int ry = y0 + h0 - pad - chm; total = n * (cwm + gap) - gap; mx = bx;
        if (total > aw) { off = bigNow * (cwm + gap) + cwm / 2 - aw / 2; if (off < 0) off = 0; if (off > total - aw) off = total - aw; }
        for (i = 0; i < n; i++) { int cx = mx + i * (cwm + gap) - off; if (cx + cwm > x0 && cx < x0 + w0) card(i, cx, ry, cwm, chm); else SetRect(&cardR[i], cx, ry, cx + cwm, ry + chm); }
    }
    GdipResetClip(G);
}
/* SIZE OF THINGS ON TOP (2026-11-20): one slider for how big the objects on a base are. True = their real size beside a man
   of the game scale (1:56 for 28 and 32 mm). Left of True shrinks them to the old "classic" map look, right makes them chunkier.
   Under it a small picture from above: the base, the space the model takes (dashed: a long box for a tank, else a disc, the
   same rule as the kit's features.c ft_model_space), a crate, a barrel and a rock at that size, and a man from the side. */
static int ms_space(double mm, double R, const char *mini, double *rad, double *hx, double *hy) {
    double a = mm < 39.9 ? 0.2 : mm < 59.9 ? 0.4 : mm < 89.9 ? 0.5 : 0.6; int dflt = !mini || !*mini || !strcmp(mini, "default") || !strcmp(mini, "mix");
    int wide = mm >= 59.9 && ((mini && (!strcmp(mini, "tracked") || !strcmp(mini, "wide") || !strcmp(mini, "hover") || !strcmp(mini, "quad") || !strcmp(mini, "many"))) || (dflt && mm >= 89.9));
    if (wide) { *hx = 0.9 * R; *hy = a * 3.14159265 * R / 3.6; if (*hy > 0.8 * R) *hy = 0.8 * R; *rad = 0; return 1; }
    *rad = sqrt(a) * R; *hx = *hy = 0; return 0;
}
static void ell(double x, double y, double w, double h, ARGB fill, ARGB line, double lw, int dash) {
    if (fill) { GpSolidFill *b; GdipCreateSolidFill(fill, &b); GdipFillEllipse(G, (GpBrush *)b, (REAL)x, (REAL)y, (REAL)w, (REAL)h); GdipDeleteBrush((GpBrush *)b); }
    if (line) { GpPen *pn; GdipCreatePen1(line, (REAL)lw, UnitPixel, &pn); if (dash) GdipSetPenDashStyle(pn, DashStyleDash); GdipDrawEllipse(G, pn, (REAL)x, (REAL)y, (REAL)w, (REAL)h); GdipDeletePen(pn); }
}
static void boxl(double x, double y, double w, double h, ARGB fill, ARGB line, double lw, int dash) {
    if (fill) { GpSolidFill *b; GdipCreateSolidFill(fill, &b); GdipFillRectangle(G, (GpBrush *)b, (REAL)x, (REAL)y, (REAL)w, (REAL)h); GdipDeleteBrush((GpBrush *)b); }
    if (line) { GpPen *pn; GdipCreatePen1(line, (REAL)lw, UnitPixel, &pn); if (dash) GdipSetPenDashStyle(pn, DashStyleDash); GdipDrawRectangle(G, pn, (REAL)x, (REAL)y, (REAL)w, (REAL)h); GdipDeletePen(pn); }
}
static int obj_classic(void) { return OBJM[selObj] * GAMEMM[selScale] / 32.0 * 3.0 <= 1.0; }          /* the slider's left end: the classic map look */
static void obj_slider(int *y) {
    int x0 = S(16), wAll = S(SIDE_W - 32), th = S(24), cw = wAll / NOBJ, i, ph = S(128); WCHAR b[160];
    double A = atof(SIZES[selSize]), B = A, man = GAMEMM[selScale], mpm = man / 1.8 * OBJM[selObj];          /* mpm: mm a metre on the base */
    { const char *xx = strchr(SIZES[selSize], 'x'); if (xx) B = atof(xx + 1); }
    section(L"SIZE OF THINGS ON TOP", x0, *y, wAll, S(18)); *y += sideTight ? S(19) : S(20);
    if (lookPix) pbox(x0, *y, wAll, th, 1, 4, 0); else rr(x0, *y, wAll, th, S(6), C_BG, C_LINE, 1);
    fillrect(x0 + cw / 2, *y + th / 2 - S(1), wAll - cw, S(2), C_LINE);
    for (i = 0; i < NOBJ; i++) {
        int cx = x0 + i * cw + cw / 2;
        if (i == OBJ_TRUE) fillrect(cx - S(1), *y + S(4), S(2), th - S(8), C_GOLD);
        if (i == selObj) ell(cx - S(8), *y + th / 2 - S(8), S(16), S(16), hov(H_OBJ, i) ? C_ACCH : C_ACC, C_EDGE, 1, 0);
        else ell(cx - S(3), *y + th / 2 - S(3), S(6), S(6), hov(H_OBJ, i) ? C_ACCH : C_DIM, 0, 0, 0);
        hit(x0 + i * cw, *y, cw, th, H_OBJ, i);
    }
    *y += th + S(2);
    txt(L"Map", x0, *y, wAll / 3, S(14), fSmall, C_DIM, DT_LEFT | DT_SINGLELINE);
    txt(L"True", x0 + wAll / 3, *y, wAll / 3, S(14), fSmall, C_GOLD, DT_CENTER | DT_SINGLELINE);
    txt(L"Chunky", x0 + 2 * wAll / 3, *y, wAll / 3, S(14), fSmall, C_DIM, DT_RIGHT | DT_SINGLELINE);
    *y += S(16);
    if (obj_classic()) swprintf(b, 160, L"Map look: things a third of true size, a small scene");
    else swprintf(b, 160, L"%ls crate %.0f mm, barrel %.0f mm, man %.0f mm", selObj == OBJ_TRUE ? L"True:" : selObj < OBJ_TRUE ? L"Smaller:" : L"Chunkier:", 0.8 * mpm, 0.6 * mpm, man);
    txt(b, x0, *y, wAll, S(28), fSmall, C_TEXT, DT_LEFT | DT_WORDBREAK);
    *y += S(30);
    if (!sideTight) {          /* the picture (left out when the panel is short of room: the line above still gives the sizes) */
        double pw = wAll - S(8), pk, bx, by, bw, bh, cx, cy, rad, hx, hy; int box; const char *mini = selMini ? MINIS[selMini] : "default";
        rr(x0, *y, wAll, ph, S(6), C_BG, C_LINE, 1);
        {   /* one row on one ground line, all to one scale: the base (from above), a crate, a barrel and a rock, then the man (from the side) */
            double g = 2.0, cm = 0.8 * mpm, bm = 0.6 * mpm, rm = 0.5 * mpm * 1.3, mwm = man * 0.3, tot = A + g * 4 + cm + bm + rm + mwm, gl = *y + ph - S(10), px;
            pk = fmin((pw - S(8)) / tot, (ph - S(30)) / fmax(B, man));          /* px a mm: the whole row fits the box */
            bw = A * pk; bh = B * pk; bx = x0 + S(8); by = gl - bh; cx = bx + bw / 2; cy = by + bh / 2;
            ell(bx, by, bw, bh, 0xFF5A5246u, C_EDGE, 1, 0);
            ell(bx + S(2), by + S(2), bw - S(4), bh - S(4), 0xFF6E6656u, 0, 0, 0);
            box = ms_space(A, fmin(A, B) / 2 * 0.97, mini, &rad, &hx, &hy);
            if (A >= 39.9 && !obj_classic()) {
                double ex = fmin(hx, bw / 2 / pk * 0.97);
                if (box) boxl(cx - ex * pk, cy - hy * pk, 2 * ex * pk, 2 * hy * pk, 0, C_TEALL, S(1.5), 1);
                else ell(cx - rad * pk, cy - rad * pk, 2 * rad * pk, 2 * rad * pk, 0, C_TEALL, S(1.5), 1);
                txt(L"dashed: the model's space", x0 + S(8), *y + S(2), wAll - S(16), S(14), fSmall, C_TEALL, DT_LEFT | DT_SINGLELINE | DT_END_ELLIPSIS);
            }
            px = bx + bw + g * pk;
            boxl(px, gl - cm * pk, cm * pk, cm * pk, 0xFF9C6B3Bu, C_EDGE, 1, 0); px += (cm + g) * pk;          /* the crate */
            ell(px, gl - bm * pk, bm * pk, bm * pk, 0xFF5D7A8Cu, C_EDGE, 1, 0); px += (bm + g) * pk;          /* the barrel, from above */
            ell(px, gl - rm * pk / 1.3, rm * pk, rm * pk / 1.3, 0xFF8A7F74u, C_EDGE, 1, 0); px += (rm + g) * pk;          /* a rock */
            {   double mh = man * pk, mw = fmax(S(3), mwm * pk), mx = px + mw / 2;          /* the man, standing on the same line */
                rr(mx - mw / 2, gl - mh * 0.82, mw, mh * 0.82, mw / 3, 0xFFD8D4CAu, C_EDGE, 1);
                ell(mx - mw * 0.45, gl - mh, mw * 0.9, mw * 0.9, 0xFFD8D4CAu, C_EDGE, 1, 0);
                swprintf(b, 160, L"%.0f mm", man); txt(b, (int)(mx - S(22)), (int)(gl - mh - S(15)), S(44), S(14), fSmall, C_DIM, DT_CENTER | DT_SINGLELINE); }
            fillrect(x0 + S(4), (int)gl, wAll - S(8), 1, C_LINE);          /* the ground line */
        }
        *y += ph + S(8);
    }
}
#include "models_popup.c"          /* the Models preview window */
#ifdef BBB_NK
static void act(int kind, int idx);
#include "nk_side.c"
#endif
static void paint(HDC out, int W, int H) {
    HBITMAP bm; HGDIOBJ obm; int y, i, mx = S(SIDE_W), mw = W - mx, np = npicked(), fy = H - S(FOOT_H), HH = S(HEAD_H + STRIP_H), rowsH = 0, rowsN = 0; WCHAR b[200];
    DC = CreateCompatibleDC(out); bm = CreateCompatibleBitmap(out, W, H); obm = SelectObject(DC, bm); SetBkMode(DC, TRANSPARENT);
    GdipCreateFromHDC(DC, &G); GdipSetSmoothingMode(G, lookPix ? SmoothingModeNone : SmoothingModeAntiAlias); GdipSetInterpolationMode(G, InterpolationModeHighQualityBicubic); GdipSetPixelOffsetMode(G, PixelOffsetModeHalf);
    nhit = 0; ntip = 0; ntext = 0; fillrect(0, 0, W, H, C_BG); fillrect(0, 0, mx, H, C_SIDE); fillrect(mx, 0, 1, H, C_LINE);
    if (lookPix && lookTex) {          /* textures: stone on the side panel, a navy scanline title bar over it, faint scanlines on the top */
        int tb = snp(S(66));
        texfill(0, 0, mx, H, 2, C_STONEL, 0);
        fillrect(0, 0, mx, tb, C_NAVY); texfill(0, 0, mx, tb, 1, C_NAVYD, 0); fillrect(0, tb - PX, mx, PX, C_TEALD);
        texfill(mx + 1, 0, mw, HH - 2 * PX, 1, 0xFF181A20u, 0);
    }

#ifndef BBB_NK
    if (sideOff > sideMax) sideOff = sideMax; if (sideOff < 0) sideOff = 0;          /* never past the end (sideMax from the last paint) */
    hitClip = (mx << 16) | ((H - S(SIDE_FOOT)) & 0xFFFF);          /* R4: the rows' click areas stop at the footer */
    GdipTranslateWorldTransform(G, 0, (REAL)-sideOff, MatrixOrderAppend); txtDy = hitDy = -sideOff;          /* GDI+ shapes by the transform, GDI text and click areas by the same shift */          /* the side panel, scrolled */
    /* the left side: what to make */
    if (lookFont && pick_spec(2, L"Balderding's Ballin Base Kit", S(SIDE_W - 24)) < 3 && pick_spec(2, L"Balderding's Ballin", S(SIDE_W - 24)) >= 3) {          /* the pixel title: on two lines, so it can be big */
        int sp = pick_spec(2, L"Balderding's Ballin", S(SIDE_W - 24));
        int lh = pf[(sp - 1) & 1].cap * (sp > 2 ? 2 * FS : FS) + S(6);          /* a line: the capitals' height at this size, and a gap */
        pf_draw(sp, L"Balderding's Ballin", S(16), S(6), S(SIDE_W - 24), lh, C_TEXT, DT_LEFT); pf_draw(sp, L"Base Kit", S(16), S(6) + lh, S(SIDE_W - 24), lh, C_TEXT, DT_LEFT);
        txt(L"Save time and money.", S(16), S(12) + 2 * lh, S(SIDE_W - 32), S(16), fSmall, C_ACC, DT_LEFT | DT_SINGLELINE); titleH = S(12) + 2 * lh + S(18);          /* R3: clear of the second line */
    } else {
        ltxt(L"Balderding's Ballin Base Kit", S(16), S(14), S(SIDE_W - 24), S(26), fTitle, C_TEXT, DT_LEFT | DT_SINGLELINE, 2, -1);
        txt(L"Save time and money.", S(16), S(40), S(SIDE_W - 32), S(18), fSmall, C_ACC, DT_LEFT | DT_SINGLELINE);
    }
    y = titleH > S(68) ? titleH : S(68); titleH = 0;
    chips(L"STYLE", STYLES, NSTYLES, 3, selStyle, H_STYLE, &y);
    {          /* the theme: any, or one of the style's own (step through them with < and >; the middle goes back to any) */
        int x0 = S(16), wAll = S(SIDE_W - 32), bh = sideTight ? S(24) : S(26), bw = S(34) < wAll / 8 ? S(34) : wAll / 8, nt = theme_count(); const char *tn = theme_name(); WCHAR tb[120];
        section(L"THEME  (ANY, OR PICK ONE)", x0, y, wAll, S(18)); y += S(20);
        if (lookPix) { pbox(x0, y, bw, bh, 1, 0, hov(H_THEME, -1)); pbox(x0 + wAll - bw, y, bw, bh, 1, 0, hov(H_THEME, 1)); pbox(x0 + bw + S(4), y, wAll - 2 * bw - S(8), bh, 1, tn ? 1 : 0, hov(H_THEME, 0)); }
        else { rr(x0, y, bw, bh, S(6), hov(H_THEME, -1) ? C_CARDH : C_CARD, C_LINE, 1); rr(x0 + wAll - bw, y, bw, bh, S(6), hov(H_THEME, 1) ? C_CARDH : C_CARD, C_LINE, 1); rr(x0 + bw + S(4), y, wAll - 2 * bw - S(8), bh, S(6), tn ? C_ACC : C_CARD, C_LINE, 1); }
        ltxt(L"<", x0, y, bw, bh, fBtn, C_TEXT, DT_CENTER | DT_VCENTER | DT_SINGLELINE, 2, -1); hit(x0, y, bw, bh, H_THEME, -1);
        ltxt(L">", x0 + wAll - bw, y, bw, bh, fBtn, C_TEXT, DT_CENTER | DT_VCENTER | DT_SINGLELINE, 2, -1); hit(x0 + wAll - bw, y, bw, bh, H_THEME, 1);
        if (tn) swprintf(tb, 120, L"%hs  (%d of %d)", tn, selTheme, nt); else swprintf(tb, 120, L"Any of its %d themes", nt);
        txt(tb, x0 + bw + S(4), y, wAll - 2 * bw - S(8), bh, fUI, tn ? C_ONACC : C_TEXT, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS); hit(x0 + bw + S(4), y, wAll - 2 * bw - S(8), bh, H_THEME, 0);
        y += bh + S(8);
    }
    chipsMask = sizeMulti; chipsBits = sizeMulti ? sizeMask : 0;
    chips(sizeMulti ? L"BASE SIZES, MM  (EACH ONE, FOR BUILD STRAIGHT AWAY)" : L"BASE SIZE, MM  (ROUND, OVAL, THEN SQUARE)", SIZES, NROUND, 9, sizeMulti ? 0 : selSize, H_SIZE, &y); y -= S(4);
    {   int y0 = y, x0 = S(16), wAll = S(SIDE_W - 32), gap = sideTight ? S(3) : S(4), ch = sideTight ? S(20) : S(24), cw = (wAll - 3 * gap) / 4;
        chips(NULL, SIZES + NROUND, NOVAL, 4, sizeMulti ? NROUND : selSize - NROUND, H_OVAL, &y);
        chips(NULL, SIZES + NROUND + NOVAL, NSQ, 3, sizeMulti ? NROUND + NOVAL : selSize - NROUND - NOVAL, H_SQ, &y);          /* square and rectangle bases */
        chipsMask = 0;
        chip("Several", x0 + 3 * (cw + gap), y0 + ch + gap, cw, ch, sizeMulti, H_MULTI, 0); }          /* several sizes at once: in the oval row's free place */
    chips(L"DETAIL  (1 SIMPLE ... 4 MOST)", DETAILL, 5, 5, selDetail, H_DETAIL, &y); chips(L"THE MINIATURE IT IS FOR", MINIS, 13, 3, selMini, H_MINI, &y);
    chips(L"GAME SCALE  (HOW TALL A MAN IS)", SCALES, 3, 3, selScale, H_SCALE, &y); obj_slider(&y);
    {          /* how many: a number from 1 to 20. Type it, or use the minus and plus */
        int x0 = S(16), wAll = S(SIDE_W - 32), bh = S(30), bw = S(40) < wAll / 7 ? S(40) : wAll / 7, fresh = GetTickCount() - lastDigit < 1500; WCHAR nb[8];
        section(L"HOW MANY BASES  (1 TO 20)", x0, y, wAll, S(18)); y += S(20);
        if (lookPix) { pbox(x0, y, bw, bh, 1, 0, hov(H_COUNT, -1)); pbox(x0 + wAll - bw, y, bw, bh, 1, 0, hov(H_COUNT, 1)); pbox(x0 + bw + S(4), y, wAll - 2 * bw - S(8), bh, 1, 4, fresh); }
        else { rr(x0, y, bw, bh, S(6), hov(H_COUNT, -1) ? C_CARDH : C_CARD, C_LINE, 1); rr(x0 + wAll - bw, y, bw, bh, S(6), hov(H_COUNT, 1) ? C_CARDH : C_CARD, C_LINE, 1); rr(x0 + bw + S(4), y, wAll - 2 * bw - S(8), bh, S(6), C_BG, fresh ? C_ACCH : C_ACC, fresh ? S(2) : 1); }
        ltxt(L"\x2212", x0, y, bw, bh, fBtn, selCount > 1 ? C_TEXT : C_OFF, DT_CENTER | DT_VCENTER | DT_SINGLELINE, 2, -1); hit(x0, y, bw, bh, H_COUNT, -1);
        ltxt(L"+", x0 + wAll - bw, y, bw, bh, fBtn, selCount < 20 ? C_TEXT : C_OFF, DT_CENTER | DT_VCENTER | DT_SINGLELINE, 2, -1); hit(x0 + wAll - bw, y, bw, bh, H_COUNT, 1);
        swprintf(nb, 8, L"%d", selCount); ltxt(nb, x0 + bw + S(4), y, wAll - 2 * bw - S(8), bh, fBig, C_TEXT, DT_CENTER | DT_VCENTER | DT_SINGLELINE, 2, -1); hit(x0 + bw + S(4), y, wAll - 2 * bw - S(8), bh, H_COUNT, 0);
        y += bh + S(8);
    }
    chips(L"STYLE STRENGTH  (2 = BOLDER FOR PAINT)", STRENGTHS, NSTR, 5, selStr, H_STR, &y);
    chips(L"PRINTER  (GRAIN AND SMALLEST DETAIL)", PRINTERS, 2, 2, selPrinter, H_PRINTER, &y);
    {          /* the design seed: empty = new designs each time; type a word to get the same designs again */
        int x0 = S(16), wAll = S(SIDE_W - 32), bh = sideTight ? S(24) : S(26), cw = S(70) < wAll / 4 ? S(70) : wAll / 4; WCHAR sb[64];
        section(L"DESIGN SEED  (EMPTY = NEW EACH TIME)", x0, y, wAll, S(18)); y += S(20);
        if (lookPix) { pbox(x0, y, wAll - cw - S(4), bh, 1, seedEdit ? 4 : 0, hov(H_SEED, 0)); pbox(x0 + wAll - cw, y, cw, bh, 1, 0, hov(H_SEED, 1)); }
        else { rr(x0, y, wAll - cw - S(4), bh, S(6), C_BG, seedEdit ? C_ACCH : C_LINE, seedEdit ? S(2) : 1); rr(x0 + wAll - cw, y, cw, bh, S(6), hov(H_SEED, 1) ? C_CARDH : C_CARD, C_LINE, 1); }
        if (seedEdit) swprintf(sb, 64, L"%hs_", seedBuf); else if (seedBuf[0]) swprintf(sb, 64, L"%hs", seedBuf); else wcscpy(sb, L"new each time (click to type one)");
        txt(sb, x0 + S(8), y, wAll - cw - S(20), bh, seedBuf[0] || seedEdit ? fMono : fSmall, seedBuf[0] || seedEdit ? C_TEXT : C_DIM, DT_LEFT | DT_VCENTER | DT_SINGLELINE); hit(x0, y, wAll - cw - S(4), bh, H_SEED, 0);
        ltxt(L"Clear", x0 + wAll - cw, y, cw, bh, fUI, C_TEXT, DT_CENTER | DT_VCENTER | DT_SINGLELINE, 0, -1); hit(x0 + wAll - cw, y, cw, bh, H_SEED, 1);
        y += bh + S(8);
    }
    {          /* saved sets: click one to load it; "Save to a set", then a set, keeps what is chosen now */
        int x0 = S(16), wAll = S(SIDE_W - 32), g = S(4), cw = (wAll - 3 * g) / 4, k; WCHAR p[MAX_PATH]; sets_ini(p);
        section(setSave ? L"SAVED SETS  (NOW CLICK A SET TO SAVE INTO)" : L"SAVED SETS  (CLICK TO LOAD)", x0, y, wAll, S(18)); y += S(20);
        for (k = 0; k < 3; k++) { WCHAR sec[8]; char lb[16]; swprintf(sec, 8, L"set%d", k + 1); snprintf(lb, sizeof lb, GetPrivateProfileIntW(sec, L"saved", 0, p) ? "Set %d" : "Set %d -", k + 1); chip(lb, x0 + k * (cw + g), y, cw, sideTight ? S(22) : S(24), 0, H_SET, k); }
        chip(setSave ? "Saving..." : "Save to a set", x0 + 3 * (cw + g), y, wAll - 3 * (cw + g), sideTight ? S(22) : S(24), setSave, H_SET, 3); y += sideTight ? S(27) : S(30);
    }
    chip("Allow overhangs (needs supports)", S(16), y, S(SIDE_W - 32), S(24), selSup, H_SUP, 0); y += S(30);
    if (selSup) { chip("Join into one solid (Manifold)", S(16), y, S(SIDE_W - 32), S(24), selJoin, H_JOIN, 0); y += S(30); }          /* only with overhangs allowed */
    {          /* MANIFOLD: what the Manifold library does to each base, one switch each */
        int x0 = S(16), wAll = S(SIDE_W - 32), half = (wAll - S(6)) / 2, k;
        section(L"MANIFOLD", x0, y, wAll, S(18)); y += S(20);
        for (k = 0; k < MF_N; k++) { int cx = k % 2 ? x0 + half + S(6) : x0, cw = k % 2 ? wAll - half - S(6) : half; chip(MFN[k], cx, y, cw, S(24), mfOn[k], H_MF, k); if (k % 2 || k == MF_N - 1) y += S(30); }
        y += S(4);
    }
    {          /* speed: CPU threads, video card */
        int x0 = S(16), wAll = S(SIDE_W - 32), half = (wAll - S(6)) / 2;
        section(L"SPEED", x0, y, wAll, S(18)); y += S(20);
        chip("CPU threads", x0, y, half, S(24), spThr, H_SPEED, 0); chip("Video card", x0 + half + S(6), y, wAll - half - S(6), S(24), spGpu, H_SPEED, 1); y += S(34);
    }
    {          /* the panel must fit: if the full size runs past the bottom, draw it tight; back to full size when it fits again */
        int need = y + S(4) + S(SIDE_FOOT);          /* R4: the rows, then the footer with Show / build straight away under them */
        if (!sideTight) { sideNeed = need; if (need > H) { sideTight = 1; InvalidateRect(hMain, NULL, FALSE); } }
        else if (sideNeed <= H) { sideTight = 0; InvalidateRect(hMain, NULL, FALSE); }
        sideMax = sideTight && need > H ? need - H + S(8) : 0; if (sideOff > sideMax) sideOff = sideMax;          /* still too tall when tight: it scrolls */
    }
    GdipResetWorldTransform(G); txtDy = hitDy = 0; hitClip = 0;
    {          /* R4 (2026-11-22): Show and Build straight away in a footer of the side panel that does not scroll: always in sight */
        int fh = S(SIDE_FOOT), f0 = H - fh, by = f0 + S(10);
        fillrect(0, f0, mx, fh, C_SIDE); if (lookPix && lookTex) texfill(0, f0, mx, fh, 2, C_STONEL, 0);
        fillrect(S(16), f0, S(SIDE_W - 32), lookPix ? PX : 1, C_LINE);
        swprintf(b, 200, ncell ? L"Show %d new choices" : L"Show %d choices", selCount);
        button(b, S(16), by, S(SIDE_W - 32), S(40), busy ? 3 : (!np ? 1 : 0), H_SHOW); by += S(46);
        { int ns = 0, q; for (q = 0; q < NSIZES; q++) ns += (sizeMask >> q) & 1u;
          if (sizeMulti && ns > 1) swprintf(b, 200, L"Or build %d of each of %d sizes straight away", selCount, ns);
          else swprintf(b, 200, selCount == 1 ? L"Or build 1 straight away" : L"Or build %d straight away", selCount); }
        button(b, S(16), by, S(SIDE_W - 32), S(30), busy ? 3 : 0, H_QUICK);
    }
    if (sideMax > 0) { int Hs = H - S(SIDE_FOOT), th = (int)((double)Hs * Hs / (Hs + sideMax)), ty = (int)((double)sideOff / sideMax * (Hs - th)); fillrect(mx - S(7), 0, S(6), Hs, C_SIDE); rr(mx - S(6), ty + S(2), S(4), th - S(4), S(2), C_LINE, 0, 0); }          /* its scroll bar, above the footer */
    if (divHover == 1 || divDrag == 1) fillrect(mx - S(1), 0, S(3), H, C_ACC);          /* R1: the line lit while it can be dragged */

#else
    nkui_frame(); nk_side(H); nk_top(mx, mw); nkui_render(G); (void)y;          /* the side panel and the top row in Nuklear */
#endif
    /* the top: what is happening */
#ifndef BBB_NK
    {          /* the look switches, left of Help: Pixel (L), Textures (T), Pixel font (F) */
        static const char *LK[3] = { "Pixel", "Textures", "Pixel font" }; static const int LW[3] = { 58, 78, 86 }; int on[3], lx = W - S(22) - S(96) - S(10), k, lw;
        on[0] = lookPix; on[1] = lookTex; on[2] = lookFont;
        for (k = 2; k >= 0; k--) { lw = need_wa(LK[k], S(LW[k])); lx -= lw; chip(LK[k], lx, S(14), lw, S(22), on[k], H_LOOK, k); lx -= S(4); }
        { int mw = need_w(L"Models", S(84)); lx -= S(6) + mw; button(L"Models", lx, S(12), mw, S(26), 0, H_MODELS); }          /* the Models preview */
        lookW = W - S(22) - S(96) - S(10) - lx; lookWd = (int)(lookW / SC + 0.5);
    }
    txt(head, mx + S(22), S(14), mw - S(44) - S(110) - lookW - S(10), S(28), fBig, C_TEXT, DT_LEFT | DT_SINGLELINE | DT_END_ELLIPSIS);
    txt(sub, mx + S(22), S(42), mw - S(44) - S(110), S(20), fSmall, C_DIM, DT_LEFT | DT_SINGLELINE | DT_END_ELLIPSIS);
    button(L"Help   ?", W - S(22) - S(96), S(18), S(96), S(34), 0, H_HELP);
#endif
    strip(mx + S(22), S(HEAD_H) - S(8), mw - S(44), S(26));
    if (!lookPix) {
        fillrect(mx + 1, HH - S(3), mw, S(3), C_LINE);
        if (busy && wanted) { double f = (doneN + 0.15) / (double)wanted; if (f > 1) f = 1; fillrect(mx + 1, HH - S(3), (int)(mw * f), S(3), C_ACC); }
    } else {          /* the line under the top, a whole number of art pixels high: a lit row over a dark one */
        int ly = HH - 2 * PX; double f = busy && wanted ? (doneN + 0.15) / (double)wanted : 0;
        if (f > 1) f = 1;
        fillrect(mx + 1, ly, mw, PX, C_LINE); fillrect(mx + 1, ly + PX, mw, PX, C_EDGE);
        if (f > 0 && lookTex) {          /* segmented: blocks of 6 art pixels; the one being worked on blinks green */
            int seg = 6 * PX, step = seg + PX, n = mw / step, lit = (int)(f * n), k, by = ly - PX;
            fillrect(mx + 1, by, mw, 3 * PX, C_EDGE);
            for (k = 0; k < n; k++) {
                ARGB c = k < lit ? C_ACC : k == lit ? ((anim & 4) ? C_GREEN : C_TEALM) : 0xFF262A33u;
                fillrect(mx + 1 + k * step, by, seg, 3 * PX, c); if (k < lit) fillrect(mx + 1 + k * step, by, seg, PX, C_ACCH);
            }
        } else if (f > 0) { fillrect(mx + 1, ly, snp(mw * f), PX, C_ACCH); fillrect(mx + 1, ly + PX, snp(mw * f), PX, C_TEALD); }
    }

    /* the middle: the choices */
    {          /* the live view while building: which base the Big view shows, and the rows over time under the cards */
        int building = 0, nrw = busy == 2 && lvRows ? rows_count() : 0; double best = 1e300;
        for (i = 0; i < ncell; i++) building += cells[i].state == ST_BUILDING;
        bigNow = -1;
        if (busy == 2 && lvView == 2 && building) {
            if (bigPick >= 0 && bigPick < ncell && cells[bigPick].state == ST_BUILDING) bigNow = bigPick;
            else for (i = 0; i < ncell; i++) if (cells[i].state == ST_BUILDING) { double t0 = cells[i].lv && cells[i].lv->have ? cells[i].lv->nt.t0 : 1e299; if (bigNow < 0 || t0 < best) { bigNow = i; best = t0; } }          /* the oldest one building */
        }
        rowsH = nrw && bigNow < 0 ? S(28) + nrw * S(14) + S(10) : 0;
        rowsN = nrw;
    }
    SetRect(&gridR, mx + 1, HH, W, fy - (showDetails ? S(detailsH) : 0) - rowsH);
    if (bigNow >= 0) big_view(gridR.left, gridR.top, gridR.right - gridR.left, gridR.bottom - gridR.top, rowsN);
    else {
        int pad = S(18), gap = S(14), aw = gridR.right - gridR.left - 2 * pad - S(8), cols, cw, ch, nrows, total, vh = gridR.bottom - gridR.top; double ovr = oval_ratio();
        cols = (aw + gap) / ((ovr > 1.01 ? S(300) : S(196)) + gap);          /* an oval base: wider cards */
        if (cols < 1) cols = 1;
        cw = (aw - (cols - 1) * gap) / cols;
        if (cw > (ovr > 1.01 ? S(440) : S(280))) cw = ovr > 1.01 ? S(440) : S(280);
        ch = ovr > 1.01 ? (int)((cw - 2 * S(10)) / ovr * 1.08) + 2 * S(10) + S(40) : cw + S(40); nrows = (ncell + cols - 1) / cols; total = nrows * (ch + gap) + 2 * pad - gap; scrollMax = total > vh ? total - vh : 0;
        if (scrollY > scrollMax) scrollY = scrollMax;
        if (scrollY < 0) scrollY = 0;
        GdipSetClipRectI(G, gridR.left, gridR.top, gridR.right - gridR.left, vh, CombineModeReplace); GdipFlush(G, FlushIntentionSync); IntersectClipRect(DC, gridR.left, gridR.top, gridR.right, gridR.bottom);
        if (!ncell) {
            static const WCHAR *st[3] = { L"Choose a style and a base size on the left, then press Show choices.", L"Click the bases you like. A teal tick marks a picked one.",
                                          L"Press Build. The kit makes the picked bases, checks that each one will print, and saves them as STL files." };
            int cy0 = gridR.top + vh / 2 - S(90);
            for (i = 0; i < 3; i++) {
                WCHAR num[4]; swprintf(num, 4, L"%d", i + 1);
                if (lookPix) px_box(gridR.left + S(60), cy0 + i * S(60), S(34), S(34), 2, C_CARD, C_ACC, C_BEVH, C_BEVD); else rr(gridR.left + S(60), cy0 + i * S(60), S(34), S(34), S(17), C_CARD, C_ACC, S(2));
                ltxt(num, gridR.left + S(60), cy0 + i * S(60), S(34), S(34), fBtn, C_ACC, DT_CENTER | DT_VCENTER | DT_SINGLELINE, 2, -1);
                txt(st[i], gridR.left + S(110), cy0 + i * S(60) - S(6), gridR.right - gridR.left - S(170), S(46), fBig, C_TEXT, DT_LEFT | DT_WORDBREAK);
            }
        }
        for (i = 0; i < ncell; i++) {
            int cx = gridR.left + pad + (i % cols) * (cw + gap), cy = gridR.top + pad + (i / cols) * (ch + gap) - scrollY;
            if (cy + ch > gridR.top && cy < gridR.bottom) card(i, cx, cy, cw, ch);
        }
        if (scrollMax) { double th = (double)vh * vh / total, ty = gridR.top + (double)scrollY / scrollMax * (vh - th);
            if (!lookPix) rr(gridR.right - S(9), ty + S(3), S(5), th - S(6), S(3), 0xFF4A5060u, 0, 0);
            else {          /* a pixel thumb three art pixels wide, on a dithered track when textures are on */
                int tx = snp(gridR.right - S(10));
                if (lookTex) texfill(tx, gridR.top, 3 * PX, vh, 0, C_LINE, 20);
                px_box(tx, ty + S(3), 3 * PX, th - S(6), 1, 0xFF4A5060u, 0, C_OFF, 0xFF353A46u);
            } }
        GdipResetClip(G); GdipFlush(G, FlushIntentionSync); SelectClipRgn(DC, NULL);
    }
    if (rowsH) {          /* rows over time, under the cards */
        fillrect(mx + 1, gridR.bottom, mw, rowsH, C_SIDE); fillrect(mx + 1, gridR.bottom, mw, lookPix ? PX : 1, C_LINE);
        rows(mx + S(22), gridR.bottom + S(8), mw - S(44), rowsH - S(10));
    }
    if (shrinkI >= 0 && shrinkI < ncell) {          /* the Big view's base going back down into its card */
        DWORD el = GetTickCount() - shrinkT; double u = el / 300.0, e; Cell *c = &cells[shrinkI]; RECT *to = &cardR[shrinkI];
        if (u >= 1) shrinkI = -1;
        else {
            double rx, ry, rw, rh2;
            e = 1 - (1 - u) * (1 - u);
            rx = shrinkFrom.left + (to->left - shrinkFrom.left) * e; ry = shrinkFrom.top + (to->top - shrinkFrom.top) * e;
            rw = (shrinkFrom.right - shrinkFrom.left) + ((to->right - to->left) - (shrinkFrom.right - shrinkFrom.left)) * e;
            rh2 = (shrinkFrom.bottom - shrinkFrom.top) + ((to->bottom - to->top) - (shrinkFrom.bottom - shrinkFrom.top)) * e;
            if (lookPix) px_box(rx, ry, rw, rh2, 2, C_CARD, C_ACC, C_BEVH, C_BEVD); else rr(rx, ry, rw, rh2, S(12), C_CARD, C_ACC, S(2));
            if (c->img3) image_fit(c->col && c->img3c ? c->img3c : c->img3, rx + S(8), ry + S(8), rw - S(16), rh2 - S(16));
            else if (c->lv && c->lv->plan) image_fit(c->lv->plan, rx + S(8), ry + S(8), rw - S(16), rh2 - S(16));
        }
    }
    if (showDetails) {          /* what the kit itself says, for when something goes wrong */
        if (divHover == 2 || divDrag == 2) fillrect(mx, gridR.bottom + rowsH - S(1), W - mx, S(3), C_ACC);          /* R2: its line lit while it can be dragged */
        int dy = gridR.bottom + rowsH, dh = S(detailsH), lh = S(16), k = (dh - S(34)) / lh, j, from = nlog > k ? nlog - k : 0;
        fillrect(mx + 1, dy, mw, dh, C_SIDE); if (lookPix && lookTex) texfill(mx + 1, dy, mw, dh, 2, C_STONEL, 0);
        fillrect(mx + 1, dy, mw, lookPix ? PX : 1, C_LINE);
        section(L"DETAILS: WHAT THE KIT SAYS", mx + S(22), dy + S(8), S(320), S(18));
        for (j = from; j < nlog; j++) txt(logv[j], mx + S(22), dy + S(28) + (j - from) * lh, mw - S(44), lh, fMono, 0xFFC4CAD4u, DT_LEFT | DT_SINGLELINE | DT_END_ELLIPSIS);
    }

    /* the bottom: what to do with the picks */
    fillrect(mx + 1, fy, mw, S(FOOT_H), C_SIDE); if (lookPix && lookTex) texfill(mx + 1, fy, mw, S(FOOT_H), 2, C_STONEL, 0);
    fillrect(mx + 1, fy, mw, lookPix ? PX : 1, C_LINE);
    {
        int by, bh, x, rx, btnw; footFit = foot_need() <= mw; by = fy + S(15); bh = S(36); x = mx + S(22); rx = W - S(22); btnw = NW(L"Build the picked bases", S(220));
        if (busy == 2) swprintf(b, 200, L"%d to go", wanted - doneN); else if (np == 1) wcscpy(b, L"1 picked"); else swprintf(b, 200, L"%d picked", np);
        { int lw0 = NW(L"20 picked", S(84)), w1 = NW(L"Pick all", S(84)), w2 = NW(L"Clear", S(70)), w3 = NW(L"More choices", S(118));
        ltxt(b, x, by, lw0, bh, fBtn, np || busy == 2 ? C_ACC : C_DIM, DT_LEFT | DT_VCENTER | DT_SINGLELINE, 1, -1); x += lw0 + S(4);
        button(L"Pick all", x, by, w1, bh, busy || !ncell ? 3 : 0, H_ALL); x += w1 + S(8);
        button(L"Clear", x, by, w2, bh, busy || !np ? 3 : 0, H_CLEAR); x += w2 + S(8);
        button(L"More choices", x, by, w3, bh, busy || !ncell ? 3 : 0, H_MORE); }
        if (busy) button(L"Stop", rx - btnw, by - S(3), btnw, bh + S(6), 2, H_STOP);
        else {
            if (np == 1) wcscpy(b, L"Build 1 base"); else if (np) swprintf(b, 200, L"Build %d bases", np); else wcscpy(b, L"Build the picked bases");
            button(b, rx - btnw, by - S(3), btnw, bh + S(6), np ? 1 : 3, H_GO);
        }
        { int wo = NW(L"Open folder", S(110)), wd = NW(L"Hide details", S(104));
        rx -= btnw + S(10); button(L"Open folder", rx - wo, by, wo, bh, 0, H_OPEN); rx -= wo + S(8);
        button(showDetails ? L"Hide details" : L"Details", rx - wd, by, wd, bh, 0, H_DETAILS); }
    }
    if (lvParts && !showHelp) parts_panel(W, H);
    if (showHelp) help(W, H);
#ifdef BBB_NK
    if (nkActed) { nkActed = 0; InvalidateRect(hMain, NULL, FALSE); }          /* a widget changed something: draw again with it */
#endif
    if (tipShown && tipText[0]) {          /* R3: the whole text of a label cut short, by the mouse */
        SIZE sz; int bw, bh, bx, by; tipNo = 1; SelectObject(DC, fSmall); GetTextExtentPoint32W(DC, tipText, (int)wcslen(tipText), &sz);
        bw = sz.cx + S(16); bh = sz.cy + S(10); if (bw > W - S(8)) bw = W - S(8);
        bx = tipX + S(12); by = tipY + S(20);
        if (bx + bw > W - S(4)) bx = W - S(4) - bw;
        if (bx < S(4)) bx = S(4);
        if (by + bh > H - S(4)) by = tipY - S(8) - bh;
        rr(bx, by, bw, bh, S(4), C_CARD, C_ACC, 1); txt(tipText, bx + S(8), by, bw - S(16), bh, fSmall, C_TEXT, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS); tipNo = 0;
    }
    GdipFlush(G, FlushIntentionSync); GdipDeleteGraphics(G); BitBlt(out, 0, 0, W, H, DC, 0, 0, SRCCOPY); SelectObject(DC, obm); DeleteObject(bm); DeleteDC(DC);
}
/* the help page: a panel over the window, opened with ? (Shift+/), F1 or the Help button, closed with Esc or a click */
static int para(const WCHAR *title, const WCHAR *body, int x, int y, int w) {
    RECT r = { x, y, x + w, y + 2000 };
    if (title) { ltxt(title, x, y, w, S(18), fLabel, C_ACC, DT_LEFT | DT_SINGLELINE, 1, -1); y += S(20); r.top = y; }
    GdipFlush(G, FlushIntentionSync); SelectObject(DC, fUI); DrawTextW(DC, body, -1, &r, DT_LEFT | DT_WORDBREAK | DT_NOPREFIX | DT_CALCRECT);
    txt(body, x, y, w, r.bottom - r.top, fUI, C_TEXT, DT_LEFT | DT_WORDBREAK);
    return y + (r.bottom - r.top) + S(12);
}
static void help(int W, int H) {
    int pw = W - S(80) > S(1040) ? S(1040) : W - S(80), ph = H - S(40) > S(880) ? S(880) : H - S(40), px = (W - pw) / 2, py = (H - ph) / 2, cwid = (pw - S(84)) / 2, x1 = px + S(28), x2 = x1 + cwid + S(28), y;
    fillrect(0, 0, W, H, 0xB0080A0Du);
    if (lookPix) px_box(px, py, pw, ph, 2, C_SIDE, C_EDGE, C_BEVH, C_BEVD); else rr(px, py, pw, ph, S(14), C_SIDE, C_LINE, 1);
    if (lookPix && lookTex) { int bx = snp(px) + 2 * PX, by = snp(py) + 2 * PX, bw = snp(px + pw) - 2 * PX - bx, bh = snp(S(48)); fillrect(bx, by, bw, bh, C_NAVY); texfill(bx, by, bw, bh, 1, C_NAVYD, 0); fillrect(bx, by + bh - PX, bw, PX, C_TEALD); }          /* its title bar */
    ltxt(L"Help", x1, py + S(18), S(300), S(28), fBig, C_TEXT, DT_LEFT | DT_SINGLELINE, 2, -1);
    txt(L"Esc, ? or a click closes this", px, py + S(22), pw - S(28), S(20), fSmall, C_DIM, DT_RIGHT | DT_SINGLELINE);
    GdipFlush(G, FlushIntentionSync); IntersectClipRect(DC, px, py, px + pw, py + ph - S(10));
    y = py + S(58);
    y = para(L"HOW IT WORKS", L"1.  Choose a style and a base size on the left and press Show choices. The kit draws that many designs; nothing is built yet.\n"
                              L"2.  Click the designs you like. A teal tick marks a picked one; click again to un-pick.\n"
                              L"3.  Press Build. Each picked base is made as an STL file and checked that it will print.", x1, y, cwid);
    y = para(L"THE OPTIONS ON THE LEFT", L"Style:  the kind of ground and the things on it (mine, swamp, temple ...).\n"
                              L"Base size:  the width of the round base in millimetres, 20 to 160; or an oval, long x short (60x35.5 to 170x109).\n"
                              L"Detail:  1 is the simplest, 4 has the most on it. Mix gives some of each.\n"
                              L"The miniature it is for:  the kit keeps the spot flat where that kind of model stands. Default takes it from settings.json; none keeps no spot.\n"
                              L"Game scale:  how tall a man is in your game, 32, 28 or 15 mm. Size of things on top:  True (the default) makes every crate, barrel and skull its real size beside that man (a crate 14 mm at 32 mm), on every base. A bigger base is for a bigger model, so it gets the model's space kept clear and level in the middle (a long box for a tank, a disc for a walker or a monster), the big pieces round it and no grit. Left of True makes things smaller, down to Map (the old classic look: a whole small scene, things a third of true size); right of True makes them chunkier. The picture under the slider shows it from above: the base, the model's space dashed, a crate, a barrel and a rock at that size, and the man.\n"
                              L"How many bases:  1 to 20. Type the number or use the minus and plus. That many designs are drawn each time.\n"
                              L"Allow overhangs:  things may hang over (rounder barrels, arching ribs). The print then needs supports. Off: every base prints without supports.  Join into one solid (shown with overhangs on): the objects are joined into the base as one clean solid (Manifold); a piece that would hang in the air is left out.", x1, y, cwid);
    y = para(L"WHERE THE FILES GO", L"Everything the kit needs is inside bbb_kit.exe, the free public-domain models its details are made from too: nothing is downloaded and nothing needs installing. It runs from any folder.\n"
                              L"Every base you build is copied into one folder, out\\picked_<date>_<time> beside bbb_kit.exe, named by style, size and theme: the STL, its 3D picture (_3d.png, and in colour _3d_color.png), its drawing from above (_2d.png) and its plan. Open folder takes you there.", x1, y, cwid);
    y = para(L"WATCHING IT BUILD", L"The strip at the top: a bar for each of the kit's own working threads (only ours, idle ones hidden) and the video card. Small:  words and a time-left bar.  "
                              L"Live card:  the card shows the base taking shape, tile by tile.  Big view:  the base being built fills the middle; done, it goes back into its card.  "
                              L"Parts:  a switch each for Core bars, Video card meter, Base taking shape, Time left (from your earlier builds) and Rows over time.", x1, y, cwid);
    y = py + S(58);
    y = para(L"THE CARDS", L"A flat picture from above is a design that is not built yet.\n"
                              L"A grey 3D picture is the finished base, with its drawing small in the corner so you can check one against the other. The post standing beside it is a man of the game scale you chose, to judge the size by; on a base of 40 mm and up a dashed line floating over it marks the model's space.\n"
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
    y = para(L"THE LOOK", L"L:  pixel or smooth look.     T:  textures (pixel look only).     F:  pixel or plain font for the short words.\n"
                              L"The switches at the top right do the same; the kit remembers them.\n"
                              L"Pixel font: Silkscreen (The Silkscreen Project Authors), SIL Open Font License 1.1, docs\\fonts\\OFL.txt.", x2, y, cwid);
    (void)y; SelectClipRgn(DC, NULL); nhit = 0; hit(0, 0, W, H, H_HELP, 0);
}
static void capture(const WCHAR *path) {
    RECT r; HDC sdc, dc; HBITMAP bm; BITMAPINFO bi; BITMAPFILEHEADER fh; void *bits; int w, h; DWORD n, wr; HANDLE f; HGDIOBJ o;
    GetClientRect(hMain, &r); w = r.right; h = r.bottom; sdc = GetDC(NULL); dc = CreateCompatibleDC(sdc);
    memset(&bi, 0, sizeof bi); bi.bmiHeader.biSize = sizeof bi.bmiHeader; bi.bmiHeader.biWidth = w; bi.bmiHeader.biHeight = h; bi.bmiHeader.biPlanes = 1; bi.bmiHeader.biBitCount = 24;
    bm = CreateDIBSection(sdc, &bi, DIB_RGB_COLORS, &bits, NULL, 0); o = SelectObject(dc, bm); { int t0 = sideTight; paint(dc, w, h); if (sideTight != t0) paint(dc, w, h); } GdiFlush();          /* the side panel may switch to tight on the first paint: paint again then */
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
static void sets_ini(WCHAR *p) {
    if (selftest) { swprintf(p, MAX_PATH, L"%ls.sets.ini", stShot); return; }          /* review P2-5: a test never touches the real out\bbb_kit.ini */
    swprintf(p, MAX_PATH, L"%ls\\out", kit); CreateDirectoryW(p, NULL); wcscat(p, L"\\bbb_kit.ini");
}
static void set_io(int k, int save) {          /* out\bbb_kit.ini [setN]: style, sizes, how many, detail, miniature, game scale, object size, strength, theme, seed */
    /* review P3-1 (2026-11-22): style and sizes by NAME (a list position shifts when a size is added); a set saved
       by position before (saved=1) still loads */
    WCHAR p[MAX_PATH], sec[8], v[400]; int i; int *iv[] = { &selCount, &selDetail, &selMini, &selScale, &selObj, &selStr, &sizeMulti };
    static const WCHAR *ik[] = { L"howmany", L"detail", L"mini", L"game", L"objsize", L"strength", L"several" }; int lim[] = { 21, 5, 13, 3, NOBJ, NSTR, 2 };
    sets_ini(p); swprintf(sec, 8, L"set%d", k + 1);
    if (save) {
        const char *tn = theme_name(); WCHAR *q = v;
        for (i = 0; i < 7; i++) { swprintf(v, 64, L"%d", *iv[i]); WritePrivateProfileStringW(sec, ik[i], v, p); }
        swprintf(v, 64, L"%hs", STYLES[selStyle]); WritePrivateProfileStringW(sec, L"style", v, p);
        swprintf(v, 64, L"%hs", SIZES[selSize]); WritePrivateProfileStringW(sec, L"size", v, p);
        v[0] = 0; for (i = 0; i < NSIZES; i++) if (sizeMask >> i & 1u) q += swprintf(q, 400 - (size_t)(q - v), L"%ls%hs", q > v ? L"," : L"", SIZES[i]);
        WritePrivateProfileStringW(sec, L"sizes", v, p);
        swprintf(v, 64, L"%hs", tn ? tn : ""); WritePrivateProfileStringW(sec, L"theme", v, p);
        swprintf(v, 64, L"%hs", seedBuf); WritePrivateProfileStringW(sec, L"seed", v, p);
        WritePrivateProfileStringW(sec, L"saved", L"2", p);
        swprintf(sub, 300, L"Saved as set %d: %hs, %d base%ls. Click Set %d any time to choose all of it again.", k + 1, STYLES[selStyle], selCount, selCount == 1 ? L"" : L"s", k + 1);
        return;
    }
    if (!GetPrivateProfileIntW(sec, L"saved", 0, p)) { swprintf(sub, 300, L"Set %d is empty. Press \"Save to a set\", then Set %d, to keep what is chosen now.", k + 1, k + 1); return; }
    {   int old = GetPrivateProfileIntW(sec, L"saved", 0, p) == 1; char a[400];
        for (i = 0; i < 7; i++) { int x = (int)GetPrivateProfileIntW(sec, ik[i], *iv[i], p); if (x >= (i == 0) && x < lim[i]) *iv[i] = x; }
        GetPrivateProfileStringW(sec, L"style", L"", v, 64, p); WideCharToMultiByte(CP_UTF8, 0, v, -1, a, 64, NULL, NULL);
        if (old) { int x = atoi(a); if (x >= 0 && x < NSTYLES) selStyle = x; } else for (i = 0; i < NSTYLES; i++) if (!strcmp(STYLES[i], a)) selStyle = i;
        GetPrivateProfileStringW(sec, L"size", L"", v, 64, p); WideCharToMultiByte(CP_UTF8, 0, v, -1, a, 64, NULL, NULL);
        if (old) { int x = atoi(a); if (x >= 0 && x < NSIZES) selSize = x; } else for (i = 0; i < NSIZES; i++) if (!strcmp(SIZES[i], a)) selSize = i;
        GetPrivateProfileStringW(sec, L"sizes", L"", v, 400, p); WideCharToMultiByte(CP_UTF8, 0, v, -1, a, 400, NULL, NULL);
        if (old) sizeMask = (unsigned)strtoul(a, NULL, 10) & ((1u << NSIZES) - 1u);
        else { char *t = strtok(a, ","); sizeMask = 0; for (; t; t = strtok(NULL, ",")) for (i = 0; i < NSIZES; i++) if (!strcmp(SIZES[i], t)) sizeMask |= 1u << i; }
    }
    GetPrivateProfileStringW(sec, L"seed", L"", v, 64, p); { char a[16]; WideCharToMultiByte(CP_UTF8, 0, v, -1, a, 16, NULL, NULL); a[15] = 0; strcpy(seedBuf, a); }
    GetPrivateProfileStringW(sec, L"theme", L"", v, 64, p); selTheme = 0;
    if (v[0]) { const char *nm[64]; char a[64]; int n = kd_theme_names(STYLES[selStyle], nm, 64); WideCharToMultiByte(CP_UTF8, 0, v, -1, a, 64, NULL, NULL); for (i = 0; i < n; i++) if (!strcmp(nm[i], a)) selTheme = i + 1; }
    swprintf(sub, 300, L"Set %d loaded: %hs, %d base%ls.", k + 1, STYLES[selStyle], selCount, selCount == 1 ? L"" : L"s");
}
static void act2(int kind, int idx);
static void act(int kind, int idx) {          /* every click and key: do it, then start or stop the animation timer */
    act2(kind, idx); anim_tick();
}
static void act2(int kind, int idx) {
    int i;
    switch (kind) {
    case H_STYLE: if (selStyle != idx) selTheme = 0; selStyle = idx; break;
    case H_SIZE: if (sizeMulti) { sizeMask ^= 1u << idx; if (sizeMask & (1u << idx)) selSize = idx; } else selSize = idx; break;
    case H_OVAL: if (sizeMulti) { sizeMask ^= 1u << (NROUND + idx); if (sizeMask & (1u << (NROUND + idx))) selSize = NROUND + idx; } else selSize = NROUND + idx; break;
    case H_SQ: { int q = NROUND + NOVAL + idx; if (sizeMulti) { sizeMask ^= 1u << q; if (sizeMask & (1u << q)) selSize = q; } else selSize = q; } break;
    case H_MULTI: sizeMulti = !sizeMulti; if (sizeMulti && !sizeMask) sizeMask = 1u << selSize; break;
    case H_THEME: { int nt = theme_count(); if (idx == 0) selTheme = 0; else if (nt) selTheme = (selTheme + idx + nt + 1) % (nt + 1); break; }
    case H_STR: selStr = idx; break;
    case H_PRINTER: selPrinter = idx; break;
    case H_SEED: if (idx == 1) { seedBuf[0] = 0; seedEdit = 0; } else seedEdit = !seedEdit; break;
    case H_SET: if (idx == 3) setSave = !setSave; else { set_io(idx, setSave); setSave = 0; } break;
    case H_DETAIL: selDetail = idx; break;
    case H_MINI: selMini = idx; break;
    case H_SCALE: selScale = idx; break;
    case H_OBJ: selObj = idx; break;
    case H_COUNT: selCount += idx; if (selCount < 1) selCount = 1; if (selCount > 20) selCount = 20; break;
    case H_QUICK: if (!busy) {
        nSizeQ = 0; batchFolder[0] = 0;
        if (sizeMulti && sizeMask) { int q, first = -1; for (q = 0; q < NSIZES; q++) if ((sizeMask >> q) & 1u) { if (first < 0) first = q; else if (nSizeQ < 32) sizeQ[nSizeQ++] = q; } if (first >= 0) selSize = first; }
        autoBuild = 1; ask_choices(1); if (!busy) { autoBuild = 0; nSizeQ = 0; } } return;
    case H_SUP: selSup = !selSup; break;
    case H_JOIN: selJoin = !selJoin; break;
    case H_MF: if (idx >= 0 && idx < MF_N) mfOn[idx] = !mfOn[idx]; break;
    case H_SPEED: if (idx == 0) spThr = !spThr; else spGpu = !spGpu; speed_env(); break;
    case H_VIEW: lvView = idx; lvParts = 0; break;
    case H_PARTS: if (idx >= 1 && idx <= 5) *part_v(idx - 1) = !*part_v(idx - 1); else lvParts = !lvParts; break;
    case H_SHOW: ask_choices(1); return;
    case H_MORE: ask_choices(0); return;
    case H_GO: build(); return;
    case H_STOP: autoBuild = 0; nSizeQ = 0; if (busy) { runner_stop = 1; if (hJob) TerminateJobObject(hJob, 1); logw(L"Stopped."); wcscpy(sub, L"Stopped."); } break;
    case H_ALL: if (!busy) for (i = 0; i < ncell; i++) if (cells[i].state == ST_NONE) cells[i].picked = 1; break;
    case H_CLEAR: if (!busy) for (i = 0; i < ncell; i++) cells[i].picked = 0; break;
    case H_OPEN: open_folder(); return;
    case H_DETAILS: showDetails = !showDetails; break;
    case H_HELP: showHelp = !showHelp; break;
    case H_MODELS: mv_open(); return;
    case H_LOOK: if (idx == 0) lookPix = !lookPix; else if (idx == 1) lookTex = !lookTex; else lookFont = !lookFont; break;
    case H_COL: if (idx >= 0 && idx < ncell && cells[idx].img3c) { cells[idx].col = !cells[idx].col; colLast = cells[idx].col; } break;
    case H_CARD: {
        Cell *c = &cells[idx]; int np;
        if (busy) { if (busy == 2 && c->state == ST_BUILDING) { bigPick = idx; if (lvView != 2) lvView = 2; } break; }          /* while building: watch that one in the Big view */
        if (c->state == ST_NONE) c->picked = !c->picked;
        else if ((c->state == ST_OK || c->state == ST_VARIANT) && c->stl[0]) { WCHAR a[MAX_PATH + 20]; swprintf(a, MAX_PATH + 20, L"/select,\"%ls\"", c->stl); ShellExecuteW(hMain, L"open", L"explorer.exe", a, NULL, SW_SHOWNORMAL); }
        np = npicked();
        if (np) { if (np == 1) wcscpy(head, L"1 base picked"); else swprintf(head, 200, L"%d bases picked", np); wcscpy(sub, L"Press Build when you have the ones you want. \"More choices\" adds another batch and keeps your picks."); }
        break; }
    }
    redraw();
}
/* R1: the line between the side panel and the cards (a strip 4 px either side of it), and its width kept in bounds:
   at least SIDE_MIN, at most what leaves the cards one column (196 + pads) */
static int det_top(void) { RECT rc; GetClientRect(hMain, &rc); return rc.bottom - S(FOOT_H) - S(detailsH); }          /* R2: the Details pane's top (its line) */
static int on_divider(int x, int y) {          /* 1 the side panel's line, 2 the Details pane's, 0 none */
    int mx = S(sideW);
    if (showHelp) return 0;
    if (x >= mx - S(4) && x <= mx + S(4)) return 1;
    if (showDetails && x > mx + S(4)) { int t = det_top(); if (y >= t - S(4) && y <= t + S(4)) return 2; }
    return 0;
}
static void det_fit(void);
static void det_set(int h) { detWant = h; det_fit(); }          /* R2: what the user asked, then what fits */
static void det_fit(void) {          /* R2: at least DET_MIN, at most half of the card area */
    RECT rc; int mxh, h = detWant; GetClientRect(hMain, &rc); mxh = (int)((rc.bottom / SC - FOOT_H - HEAD_H - STRIP_H) / 2);
    if (h > mxh) h = mxh;
    if (h < DET_MIN) h = DET_MIN;
    detailsH = h;
}
static int main_need(void) {          /* R3: design px the card side needs: one card column, the top row (look switches, Help, a title), the strip, the bottom bar compact */
    int n = 260, a, b, c, i, vw = S(78);
    for (i = 0; i < 3; i++) vw = need_wa(VIEWS[i], vw);
    a = lookWd + 22 + 96 + 10 + 22 + 200;
    b = 84 + 4 + 84 + 8 + 70 + 8 + 118 + 24 + 104 + 8 + 110 + 10 + 220 + 44;          /* the bottom bar at its compact sizes (foot_need's smallest widths; footFit off) */
    c = (int)((need_w(L"Parts", S(64)) + 3 * (vw + S(4)) + S(14) + need_w(L"OUR CORES", S(64)) + S(170) + S(44)) / SC);
    if (a > n) n = a;
    if (b > n) n = b;
    if (c > n) n = c;
    return n;
}
static void side_fit(void);
static void side_set(int w) { sideWant = w; side_fit(); }          /* R1: what the user asked, then what fits */
static void side_fit(void) {
    RECT rc; int mxw, w = sideWant; GetClientRect(hMain, &rc); mxw = (int)(rc.right / SC) - main_need();
    if (w > mxw) w = mxw;
    if (w < SIDE_MIN) w = SIDE_MIN;
    sideW = w;
}
static void hover_at(int x, int y) {
    int i, k = H_NONE, id = -1; POINT p = { x, y };
    {   int t = -1, j; for (j = ntip - 1; j >= 0; j--) if (PtInRect(&tips[j].r, p)) { t = j; break; }          /* R3: a cut label under the mouse: its tooltip in half a second */
        if (t < 0) { if (tipCur >= 0 || tipShown) { tipCur = -1; tipShown = 0; KillTimer(hMain, 7); redraw(); } }
        else if (tipCur < 0 || wcscmp(tipText, tips[t].s)) { int was = tipShown; tipCur = t; tipShown = 0; wcscpy(tipText, tips[t].s); tipX = x; tipY = y; SetTimer(hMain, 7, 500, NULL); if (was) redraw(); } }
    for (i = nhit - 1; i >= 0; i--) if (PtInRect(&hits[i].r, p)) { k = hits[i].kind; id = hits[i].idx; break; }
    if (k == H_CARD && !PtInRect(&gridR, p)) { k = H_NONE; id = -1; }
    if (k != hovK || id != hovI) {
        hovK = k; hovI = id;
        if (k == H_LOOK) {          /* say what the switch under the mouse does */
            static const WCHAR *lt[3] = { L"Pixel look (key L): blocky pixel frames in whole art pixels. Off: the smooth look with round corners.",
                                          L"Textures (key T): stone on the side panel, scanline bars, a segmented progress bar, marching ants round picked bases. Pixel look only.",
                                          L"Pixel font (key F): a pixel font for labels, chips, buttons and the title; long text stays plain. Off: the plain font everywhere." };
            wcscpy(sub, lt[id]);
        }
        if (k == H_SPEED) {          /* say what the speed switch under the mouse does */
            static const WCHAR *st[2] = { L"CPU threads: the height field and the mesh are made on as many CPU threads as the base needs (up to all of them), about 3 times faster for those steps. Off: one core, the proven path.",
                                          L"Video card: the 3D pictures are drawn by the video card (Direct3D 11), in a fraction of a second. Off: drawn by Windows' software renderer (all CPU cores), slower." };
            wcscpy(sub, st[id]);
        }
        if (k == H_VIEW) {          /* say what the view under the mouse shows */
            static const WCHAR *vt[3] = { L"Small: each building card says in words what is being done, with a bar for the time left; one CPU and one video card meter at the top.",
                                          L"Live card: the card being built shows the base taking shape (the plan, then the height field filling in tile by tile), what is being done and the time left.",
                                          L"Big view: the base being built fills the middle, large, with rows over time below. When it is done it goes back into its card and the next one takes its place." };
            wcscpy(sub, vt[id]);
        }
        if (k == H_PARTS) {          /* say what the part under the mouse is */
            static const WCHAR *pt[6] = { L"Parts: choose what the live view shows. Each part has its own switch.",
                                          L"Core bars: one thin bar for each of the kit's own threads that is working, filled by how busy it is (other programs are not counted), and how many of the PC's cores that is.",
                                          L"Video card meter: how much of the video card's 3D engine is in use (every program on the PC). It draws the 3D pictures.",
                                          L"Base taking shape: the building card (or the big view) shows the plan, then the height field tiles lighting up as the threads finish them.",
                                          L"Time left: a bar and seconds left, worked out from your earlier builds of the same style, size and detail. It never goes back up.",
                                          L"Rows over time: one row per base being built, coloured by what it is doing (laying out, height field, mesh, checks, picture)." };
            if (id >= 0 && id <= 5) wcscpy(sub, pt[id]);
            else if (id == 11) wcscpy(sub, L"The kit's own working threads, one bar each, busiest first (other programs are not counted). Idle threads are not shown.");
            else if (id == 12) wcscpy(sub, gpuV < 0 ? L"The video card meter: Windows does not report the video card's use on this PC." : L"How much of the video card's 3D engine is in use (all programs). The kit draws its 3D pictures there.");
        }
        if (k == H_MF) {          /* say what the Manifold switch under the mouse does */
            static const WCHAR *mt[4] = { L"Smaller files: Manifold draws the flat parts of each base with fewer, bigger triangles (the shape moves no more than 0.005 mm), so the STL is smaller and loads faster. Off: the mesh as it is made.",
                                          L"Strict check: the print check also asks Manifold whether the base is a valid closed solid, and builds a fresh variant if not. Off: the kit's own checks only.",
                                          L"Smooth details: objects keep their true rounded shape instead of stacked slices.",
                                          L"Organic rocks: rocks and stones grown as smooth lumpy shapes." };
            if (id >= 0 && id < 4) wcscpy(sub, mt[id]);
        }
        if (k == H_MULTI) wcscpy(sub, L"Several sizes at once: click each size you want (round or oval). \"Build straight away\" then builds that many bases for every size, one size after another.");
        if (k == H_THEME) wcscpy(sub, L"Theme: Any lets the kit choose among the style's themes. Step with < and > to make every base one theme; click the middle for Any again. A themes.json beside bbb_kit.exe adds your own.");
        if (k == H_PRINTER) wcscpy(sub, L"Printer: FDM (filament) gets a coarser ground grain, deep enough to print in layers, and keeps no detail under 3 mm. Resin gets the finer grain and keeps details down to 2 mm.");
        if (k == H_STR) wcscpy(sub, L"Style strength: 1 is the standard look. Higher gives deeper cracks, bolder rock shapes and chips and a coarser ground grain, which catch washes and drybrushing better; lower is softer.");
        if (k == H_SEED) wcscpy(sub, id == 1 ? L"Clear the seed: new designs each time." : L"Design seed: type a word (letters and digits) to get the same designs again, on any PC. Empty: new designs each time. Enter or Esc when done.");
        if (k == H_SET) wcscpy(sub, id == 3 ? L"Save to a set: press this, then Set 1, 2 or 3, to keep the style, sizes, number, detail, miniature, scale, object size, strength, theme and seed chosen now." : L"Click to choose everything kept in this set again (\"-\" = nothing saved in it yet).");
        if (k == H_JOIN) wcscpy(sub, L"Join into one solid: the objects that may overhang are joined into the base as one clean solid (Manifold), and a piece that would hang in the air is left out. Off: each object is its own shell sunk into the base, for the slicer to join.");
        if (k == H_COL) wcscpy(sub, L"Colour view: the finished 3D base coloured as its 2D drawing (the same colours), to check each part is where it should be. Click again for the plain grey picture.");
        if (k == H_CARD && !busy && cells[id].state != ST_PENDING) {          /* say what the card under the mouse is */
            Cell *c = &cells[id];
            if (c->why[0]) swprintf(sub, 300, L"%ls", c->why);
            else if (c->state == ST_OK) swprintf(sub, 300, L"Built and checked: %ls. Click to show the STL file.", c->theme);
            else swprintf(sub, 300, L"%ls, detail %d of 4 (design %hs-%d). Click to %ls.", c->theme, c->tier, c->seed, c->n, c->picked ? L"un-pick" : L"pick");
        }
        redraw();
    }
    { int dv = on_divider(x, y); if (dv != divHover) { divHover = dv; redraw(); } }
    { int d = divDrag ? divDrag : divHover; SetCursor(LoadCursor(NULL, d == 1 ? IDC_SIZEWE : d == 2 ? IDC_SIZENS : k != H_NONE ? IDC_HAND : IDC_ARROW)); }
}
static void make_fonts(void) {
    HFONT *fs[] = { &fTitle, &fLabel, &fUI, &fSmall, &fBig, &fBtn, &fMono }; int px[] = { 19, 11, 13, 12, 18, 15, 12 }, wt[] = { 700, 600, 400, 400, 600, 600, 400 }, i;
    for (i = 0; i < 7; i++) { if (*fs[i]) DeleteObject(*fs[i]); *fs[i] = CreateFontW(-S(px[i]), 0, 0, 0, wt[i], 0, 0, 0, DEFAULT_CHARSET, 0, 0, CLEARTYPE_QUALITY, 0, i == 6 ? L"Consolas" : L"Segoe UI"); }
    PX = (int)(SC * 2 + 0.25); if (PX < 1) PX = 1;          /* the art pixel and the pixel font's scale follow the monitor's scaling */
    FS = (int)(SC * 2 + 0.05); if (FS < 1) FS = 1;          /* rounded down: never larger than the layout around it */          /* the pixel font follows the scale: 2 screen pixels a font pixel at 100 %, 3 at 150 %, 1 at 60 % */          /* the pixel font: two screen pixels a font pixel at 100 %, on the art grid (2026-11-20: was one) */
}
static void ini(int save) {
    WCHAR p[MAX_PATH], v[16]; int *vals[] = { &selStyle, &selSize, &selDetail, &selMini, &selCount, &selSup, &showDetails, &selScale, &lookPix, &lookTex, &lookFont, &spThr, &spGpu,
                                              &lvView, &lvCores, &lvGpu, &lvShape, &lvTime, &lvRows, &selJoin, &mfOn[0], &mfOn[1], &mfOn[2], &mfOn[3], &selObj }; int lim[] = { NSTYLES, NSIZES, 5, 13, 21, 2, 2, 3, 2, 2, 2, 2, 2, 3, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, NOBJ }, i;
    static const WCHAR *keys[] = { L"style", L"basesize", L"detail", L"mini", L"howmany", L"overhangs", L"details", L"game", L"pixel", L"textures", L"pixelfont", L"threads", L"videocard",
                                   L"liveview", L"livecores", L"livegpu", L"liveshape", L"livetime", L"liverows", L"joinsolid", L"mfsmaller", L"mfstrict", L"mfsmooth", L"mfrocks", L"objsize" };          /* "game": the game scale (2026-11-20; the old "look" key, the figure size with classic, is not read now), "objsize": the slider */
    swprintf(p, MAX_PATH, L"%ls\\out", kit);
    if (save) CreateDirectoryW(p, NULL);
    wcscat(p, L"\\bbb_kit.ini");
    if (selftest) { WCHAR te[MAX_PATH]; if (!GetEnvironmentVariableW(L"BBB_TEST_INI", te, MAX_PATH)) return; wcscpy(p, te); }          /* a test: its own ini, never out\\bbb_kit.ini */
    if (save) { swprintf(v, 16, L"%d", sideWant); WritePrivateProfileStringW(L"window", L"sidew", v, p); swprintf(v, 16, L"%d", detWant); WritePrivateProfileStringW(L"window", L"detailsh", v, p); }          /* R1, R2 */
    else { sideW = sideWant = (int)GetPrivateProfileIntW(L"window", L"sidew", SIDE_W0, p); detailsH = detWant = (int)GetPrivateProfileIntW(L"window", L"detailsh", DET_H0, p); }
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
    if (save) {          /* the extras of plan B step 6 and the size by its name (the list grew: 120 mm, 2026-11-21) */
        swprintf(v, 16, L"%hs", SIZES[selSize]); WritePrivateProfileStringW(L"window", L"basesizemm", v, p);
        swprintf(v, 16, L"%d", selStr); WritePrivateProfileStringW(L"window", L"strength", v, p); WritePrivateProfileStringW(L"window", L"printer", selPrinter ? L"resin" : L"fdm", p);
        swprintf(v, 16, L"%d", sizeMulti); WritePrivateProfileStringW(L"window", L"several", v, p);
        swprintf(v, 16, L"%u", sizeMask); WritePrivateProfileStringW(L"window", L"sizes", v, p);
        { WCHAR sd[16]; swprintf(sd, 16, L"%hs", seedBuf); WritePrivateProfileStringW(L"window", L"seed", sd, p); }
    }
    for (i = 0; i < 25; i++) {
        if (save) { swprintf(v, 16, L"%d", *vals[i]); WritePrivateProfileStringW(L"window", keys[i], v, p); }
        else { int x = (int)GetPrivateProfileIntW(L"window", keys[i], *vals[i], p); if (x >= (i == 4) && x < lim[i]) *vals[i] = x; }
    }
    if (!save) {          /* the size by its name when saved so (the list grew), and the extras of plan B step 6 */
        WCHAR t[64]; int x;
        GetPrivateProfileStringW(L"window", L"basesizemm", L"", t, 64, p);
        if (t[0]) for (i = 0; i < NSIZES; i++) { WCHAR sz[16]; swprintf(sz, 16, L"%hs", SIZES[i]); if (!wcscmp(sz, t)) selSize = i; }
        x = (int)GetPrivateProfileIntW(L"window", L"strength", selStr, p); if (x >= 0 && x < NSTR) selStr = x;
        { WCHAR pv[16]; GetPrivateProfileStringW(L"window", L"printer", L"fdm", pv, 16, p); selPrinter = !_wcsicmp(pv, L"resin"); }
        sizeMulti = GetPrivateProfileIntW(L"window", L"several", 0, p) ? 1 : 0;
        GetPrivateProfileStringW(L"window", L"sizes", L"0", t, 64, p); sizeMask = (unsigned)wcstoul(t, NULL, 10) & ((1u << NSIZES) - 1u);
        GetPrivateProfileStringW(L"window", L"seed", L"", t, 64, p); { char a[16]; WideCharToMultiByte(CP_UTF8, 0, t, -1, a, 16, NULL, NULL); a[15] = 0; strcpy(seedBuf, a); }
    }
}
static LRESULT CALLBACK proc(HWND h, UINT m, WPARAM w, LPARAM l) {
#ifdef BBB_NK
    if (NK && (m == WM_MOUSEMOVE || m == WM_LBUTTONDOWN || m == WM_LBUTTONUP || m == WM_LBUTTONDBLCLK || m == WM_RBUTTONDOWN || m == WM_RBUTTONUP || m == WM_MOUSEWHEEL || m == WM_KEYDOWN || m == WM_KEYUP || m == WM_CHAR)) {          /* Nuklear sees every input; the side panel is its */
        POINT pt; int side; nkui_event(h, m, w, l);
        if (m == WM_MOUSEWHEEL) { pt.x = GET_X_LPARAM(l); pt.y = GET_Y_LPARAM(l); ScreenToClient(h, &pt); } else { pt.x = GET_X_LPARAM(l); pt.y = GET_Y_LPARAM(l); }
        side = (m == WM_KEYDOWN || m == WM_KEYUP || m == WM_CHAR) ? 0 : (pt.x < S(SIDE_W) || pt.y < S(HEAD_H) - S(12));          /* the side panel and the top row are Nuklear's */
        if (side) { if (m == WM_LBUTTONDOWN) SetCapture(h); if (m == WM_LBUTTONUP) ReleaseCapture(); redraw(); if (m != WM_MOUSEMOVE) return 0; hovK = H_NONE; hovI = -1; return 0; }
        if (m == WM_LBUTTONUP && GetCapture() == h) { ReleaseCapture(); redraw(); }
    }
#endif
    switch (m) {
    case WM_LINE: on_line((int)w, (WCHAR *)l); free((void *)l); return 0;
    case WM_ENDED: on_ended((int)w, (DWORD)l); return 0;
    case WM_TIMER:
        if (w == 7) { KillTimer(h, 7); if (tipCur >= 0) { tipShown = 1; redraw(); } return 0; }          /* R3: the tooltip's half second is up */
        if (w == 9 && selftest == 6) {          /* --idletest: the CPU time this process used while it sat idle, and the redraws */
            static ULONGLONG c0 = 0, t0 = 0; FILETIME a, b2, k, u; GetProcessTimes(GetCurrentProcess(), &a, &b2, &k, &u);
            ULONGLONG c = (((ULONGLONG)k.dwHighDateTime << 32 | k.dwLowDateTime) + ((ULONGLONG)u.dwHighDateTime << 32 | u.dwLowDateTime)) / 10000, t = GetTickCount64();
            if (!t0) { c0 = c; t0 = t; nPaint = 0; KillTimer(h, 9); SetTimer(h, 9, idleSecs * 1000, NULL); return 0; }          /* the first tick: start counting after start-up */
            { FILE *f = _wfopen(stShot, L"w"); double secs = (t - t0) / 1000.0, pct = secs > 0 ? 100.0 * (c - c0) / 1000.0 / secs : 0;
              if (f) { fprintf(f, "%s idle %.1f s: cpu %.0f ms = %.2f %% of a core, %d redraws, in_front %d" "\n", pct < 1.0 ? "OK" : "HIGH", secs, (double)(c - c0), pct, nPaint, in_front()); fclose(f); } }
            PostQuitMessage(0); return 0;
        }
        if (w == 2) KillTimer(h, 2);
        else if (w == 3) {          /* the meters: a redraw only when what they show changed, and only in front (or building) */
            static long long last = -2; long long now;
            if (!busy && !in_front() && (!selftest || selftest == 6)) return 0;
            meters_tick(); now = meters_shown();
            if (now == last) return 0;
            last = now;
        }
        else if (w == 1 && !busy && !in_front() && (!selftest || selftest == 6)) return 0;          /* the picked cards' animated outline: not behind another window */
        else if (w == 4) {          /* the animation timer redraws while building */
            live_poll();
            if (selftest && GetEnvironmentVariableA("BBB_LIVE_SHOTS", NULL, 0)) {          /* a test: pictures of the three views while laying out, then while the height field is cut */
                static int got[2]; int k, ph = -1, keep = lvView;
                for (k = 0; k < ncell; k++) { LiveC *L = cells[k].lv; if (!L || !L->have) continue; if (!got[0] && L->npoly > 20 && !L->plan) ph = 0; else if (!got[1] && L->plan && !strcmp(L->nt.stage, "field") && L->nt.ntile) ph = 1; }
                if (ph >= 0) { WCHAR q[MAX_PATH]; got[ph] = 1; for (k = 0; k < 3; k++) { lvView = k; swprintf(q, MAX_PATH, L"%ls.live_%ls_%hs.bmp", stShot, ph ? L"field" : L"layout", VIEWS[k]); capture(q); } lvView = keep; }
            }
            return 0;
        }
        else if (w == 5) { if (shrinkI < 0) KillTimer(h, 5); }
        else anim++;
        redraw(); return 0;
    case WM_MOUSEMOVE: if (divDrag) {          /* R1 / R2: the line follows the mouse */
            if (divDrag == 1) side_set((int)(GET_X_LPARAM(l) / SC + 0.5));
            else { RECT rc; GetClientRect(h, &rc); det_set((int)((rc.bottom - S(FOOT_H) - GET_Y_LPARAM(l)) / SC + 0.5)); }
            SetCursor(LoadCursor(NULL, divDrag == 1 ? IDC_SIZEWE : IDC_SIZENS)); redraw(); return 0; }          /* R1: the line follows the mouse */
        { TRACKMOUSEEVENT t = { sizeof t, TME_LEAVE, h, 0 }; TrackMouseEvent(&t); hover_at(GET_X_LPARAM(l), GET_Y_LPARAM(l)); if ((w & MK_LBUTTON) && hovK == H_OBJ && hovI != selObj) act(H_OBJ, hovI); return 0; }          /* the slider follows a drag */
    case WM_MOUSELEAVE: hovK = H_NONE; hovI = -1; tipCur = -1; tipShown = 0; KillTimer(h, 7); redraw(); return 0;
    case WM_SETCURSOR: if (LOWORD(l) == HTCLIENT) return TRUE; break;
    case WM_LBUTTONDBLCLK: { int d = on_divider(GET_X_LPARAM(l), GET_Y_LPARAM(l)); if (d) { if (d == 1) side_set(SIDE_W0); else det_set(DET_H0); SendMessageW(h, WM_SIZE, 0, 0); return 0; } }          /* R1: back to the default */
        /* any other double-click is a click, as it was before the window took double-clicks */
    case WM_LBUTTONDOWN: if (tipShown || tipCur >= 0) { tipShown = 0; tipCur = -1; KillTimer(h, 7); }          /* R3: a click hides the tooltip */
        { int d = on_divider(GET_X_LPARAM(l), GET_Y_LPARAM(l)); if (d) { divDrag = d; SetCapture(h); redraw(); return 0; } }          /* R1: drag the line */
        hover_at(GET_X_LPARAM(l), GET_Y_LPARAM(l)); if (hovK != H_NONE) act(hovK, hovI); return 0;
    case WM_LBUTTONUP: if (divDrag) { divDrag = 0; ReleaseCapture(); SendMessageW(h, WM_SIZE, 0, 0); return 0; }          /* R1: let go: the scale fits the new split */
        break;
    case WM_MOUSEWHEEL:
        { POINT pt = { GET_X_LPARAM(l), GET_Y_LPARAM(l) }; ScreenToClient(h, &pt); if (pt.x < S(SIDE_W) && sideMax > 0) { sideOff -= GET_WHEEL_DELTA_WPARAM(w) * S(90) / 120; if (sideOff < 0) sideOff = 0; if (sideOff > sideMax) sideOff = sideMax; redraw(); return 0; } }
        scrollY -= GET_WHEEL_DELTA_WPARAM(w) * S(90) / 120;
        if (scrollY < 0) scrollY = 0;
        if (scrollY > scrollMax) scrollY = scrollMax;
        redraw(); return 0;
    case WM_KEYDOWN:
        if (seedEdit && w != VK_F1) return 0;          /* typing a design seed: its keys are letters (WM_CHAR below), not shortcuts; Enter/Esc finish it there */
        if (showHelp && w != VK_ESCAPE && w != VK_F1 && w != VK_OEM_2 && w != VK_SHIFT) { showHelp = 0; redraw(); }
        else if (w == VK_SHIFT) ;
        else if (w == VK_RETURN) { if (npicked()) build(); else ask_choices(1); }
        else if (w == VK_F1 || (w == VK_OEM_2 && GetKeyState(VK_SHIFT) < 0)) act(H_HELP, 0);          /* F1, or ? (Shift+/) */
        else if (w == VK_ESCAPE) { if (showHelp) act(H_HELP, 0); else act(H_STOP, 0); }
        else if (w == 'A' && GetKeyState(VK_CONTROL) < 0) act(H_ALL, 0);
        else if (w == 'D') act(H_DETAILS, 0);
        else if (w == 'L' && GetKeyState(VK_CONTROL) >= 0) act(H_LOOK, 0);          /* the look: L pixel / smooth, T textures, F pixel font */
        else if (w == 'T' && GetKeyState(VK_CONTROL) >= 0) act(H_LOOK, 1);
        else if (w == 'F' && GetKeyState(VK_CONTROL) >= 0) act(H_LOOK, 2);
        return 0;
    case WM_CHAR:          /* digits set "how many": 1 then 5 within a moment is 15 */
        if (seedEdit) {          /* the design seed box: letters and digits, Backspace, Enter or Esc to finish */
            size_t n = strlen(seedBuf);
            if (w == 8) { if (n) seedBuf[n - 1] = 0; }
            else if (w == 13 || w == 27) seedEdit = 0;
            else if (((w >= 'a' && w <= 'z') || (w >= 'A' && w <= 'Z') || (w >= '0' && w <= '9')) && n < 12) { seedBuf[n] = (char)(w >= 'A' && w <= 'Z' ? w + 32 : w); seedBuf[n + 1] = 0; }
            redraw(); return 0;
        }
        if (showHelp) return 0;
        if (w >= '0' && w <= '9') {
            int d = (int)w - '0', two = selCount * 10 + d;
            if (GetTickCount() - lastDigit < 1500 && two >= 1 && two <= 20) selCount = two; else if (d >= 1) selCount = d; else return 0;
            lastDigit = GetTickCount(); SetTimer(h, 2, 1600, NULL); redraw();
        } else if (w == '+' || w == '=') act(H_COUNT, 1);
        else if (w == '-') act(H_COUNT, -1);
        return 0;
    case WM_ERASEBKGND: return 1;
    case WM_PAINT: { PAINTSTRUCT ps; RECT r; HDC dc = BeginPaint(h, &ps); nPaint++; GetClientRect(h, &r); if (r.right > 0 && r.bottom > 0) paint(dc, r.right, r.bottom); EndPaint(h, &ps); return 0; }
    case WM_SIZE: { RECT rc; GetClientRect(h, &rc); if (rc.right > 0 && rc.bottom > 0) { double s2 = fit_sc(rc.right, rc.bottom); if (fabs(s2 - SC) > 1e-6) { SC = s2; make_fonts();          /* the scale follows the window */
#ifdef BBB_NK
        if (NK) { nkui_fonts("Segoe UI", S(13), S(12), S(13), S(19)); nk_theme(nkTheme); }
#endif
        } side_fit(); det_fit(); } redraw(); return 0; }          /* R1/R2: what was asked, fitted to this size */
    case WM_GETMINMAXINFO: { MINMAXINFO *mi = (MINMAXINFO *)l; RECT r = { 0, 0, 900, 560 };          /* as small as 900 x 560 px: the scale follows the window, the side panel scrolls */
        {   /* R5: at the smallest scale (0.6): the side panel at its narrowest + what the card side needs; the top, one card row,
               the bottom bar and the side panel's footer with a little of its rows above; never under 900 x 560 */
            double s0 = 0.6, k = SC; int w, hh;
            SC = s0; w = (int)((SIDE_MIN + main_need()) * s0 + 0.5); SC = k;
            hh = (int)((HEAD_H + STRIP_H + 196 + 40 + 36 + FOOT_H) * s0 + 0.5); if (hh < (int)((SIDE_FOOT + 240) * s0)) hh = (int)((SIDE_FOOT + 240) * s0);
            if (w > r.right) r.right = w;
            if (hh > r.bottom) r.bottom = hh;
        }
        AdjustWindowRect(&r, WS_OVERLAPPEDWINDOW, FALSE); mi->ptMinTrackSize.x = r.right - r.left; mi->ptMinTrackSize.y = r.bottom - r.top; return 0; }
    case 0x02E0: { RECT *r = (RECT *)l; SetWindowPos(h, NULL, r->left, r->top, r->right - r->left, r->bottom - r->top, SWP_NOZORDER | SWP_NOACTIVATE); redraw(); return 0; }          /* WM_DPICHANGED: moved to a monitor with another scaling */
    case WM_DESTROY: if (hJob) TerminateJobObject(hJob, 1); if (!selftest) ini(1); PostQuitMessage(0); return 0;
    }
    return DefWindowProcW(h, m, w, l);
}
#include "uicheck.c"          /* the UI check of --resizetest (BBB_TEST_UICHECK) */
static LONG WINAPI crash_note(EXCEPTION_POINTERS *ep) {          /* BBB_CRASH_LOG: where a helper crashed (code, address, offset in this exe), then the usual end */
    char p[MAX_PATH]; FILE *f; HMODULE me = GetModuleHandleW(NULL);
    if (GetEnvironmentVariableA("BBB_CRASH_LOG", p, MAX_PATH) && (f = fopen(p, "a"))) {
        void *a = ep->ExceptionRecord->ExceptionAddress; void *st[24]; USHORT n = CaptureStackBackTrace(0, 24, st, NULL);
        fprintf(f, "crash %08lx at %p (exe+0x%llx)\n", ep->ExceptionRecord->ExceptionCode, a, (unsigned long long)((char *)a - (char *)me));
        for (USHORT i = 0; i < n; i++) fprintf(f, "  exe+0x%llx\n", (unsigned long long)((char *)st[i] - (char *)me));
        { CONTEXT *c = ep->ContextRecord; DWORD64 *sp = (DWORD64 *)c->Rsp; for (int i = 0; i < 64; i++) { DWORD64 v = sp[i]; if (v > (DWORD64)me && v < (DWORD64)me + 0x4000000) fprintf(f, "  stack exe+0x%llx\n", (unsigned long long)(v - (DWORD64)me)); } }
        fclose(f);
    }
    return EXCEPTION_CONTINUE_SEARCH;
}
int WINAPI WinMain(HINSTANCE hi, HINSTANCE hp, LPSTR cmdA, int show) {
    WNDCLASSW wc; MSG msg; RECT wa, r; WCHAR t[MAX_PATH], e[MAX_PATH], *sl; FILE *f; int cw = 1290, chh = 896; JOBOBJECT_EXTENDED_LIMIT_INFORMATION ji; DWORD ex = 0; BOOL dark = TRUE;
    GdiplusStartupInput gin = { 1, NULL, FALSE, FALSE }; ULONG_PTR gtok; HMODULE u32 = GetModuleHandleW(L"user32.dll"), dwm; HDC sdc;
    (void)hp;
    if (!strncmp(cmdA, "--core-layout", 13)) {          /* a layout helper of a job: --core-layout <choices|build> <style> (cport\pipeline.c) */
        char md[16] = "", st[64] = ""; sscanf(cmdA + 13, " %15s %63s", md, st); if (GetEnvironmentVariableA("BBB_CRASH_LOG", NULL, 0)) SetUnhandledExceptionFilter(crash_note); return pipe_serve(md, st);
    }
    if (!strncmp(cmdA, "--core-", 7)) return bbb_core_main(cmdA);          /* the kit's other C parts: height field + mesh, pictures (cport\core.c); no window */
    if (!strncmp(cmdA, "--job ", 6)) {          /* a job without the window: bbb_kit.exe --job choices|build <style> ... (gui_job.py's arguments); its JSON lines on stdout */
        int n = 0, i; LPWSTR *wa = CommandLineToArgvW(GetCommandLineW(), &n); char **av = (char **)calloc((size_t)n + 1, sizeof(char *)); int code;
        for (i = 2; i < n; i++) { int k = WideCharToMultiByte(CP_UTF8, 0, wa[i], -1, NULL, 0, NULL, NULL); av[i - 2] = (char *)malloc((size_t)k); WideCharToMultiByte(CP_UTF8, 0, wa[i], -1, av[i - 2], k, NULL, NULL); }
        code = runner_main(n - 2 > 0 ? n - 2 : 0, av, NULL, NULL);
        return code;
    }
    if (!strncmp(cmdA, "--demotest", 10)) { selftest = 3; swprintf(stShot, MAX_PATH, L"%hs", cmdA + 10 + (cmdA[10] == ' ')); }
    else if (!strncmp(cmdA, "--quicktest", 11)) { selftest = 1; stStep = 2; autoBuild = 1; stSize = 3; swprintf(stShot, MAX_PATH, L"%hs", cmdA + 11 + (cmdA[11] == ' ')); }
    else if (!strncmp(cmdA, "--resizetest", 12)) { selftest = 2; swprintf(stShot, MAX_PATH, L"%hs", cmdA + 12 + (cmdA[12] == ' ')); }
    else if (!strncmp(cmdA, "--idletest", 10)) { selftest = 6; idleSecs = 60; { const char *q = cmdA + 10 + (cmdA[10] == ' '); idleSecs = atoi(q); if (idleSecs < 1) idleSecs = 60; while (*q && *q != ' ') q++; if (*q) q++; swprintf(stShot, MAX_PATH, L"%hs", q); } }          /* --idletest <secs> <out.txt>: sit idle, hidden, and say what it cost */
    else if (!strncmp(cmdA, "--modelstest", 12)) { selftest = 4; swprintf(stShot, MAX_PATH, L"%hs", cmdA + 12 + (cmdA[12] == ' ')); }
    else if (!strncmp(cmdA, "--selftest", 10)) { selftest = 1; swprintf(stShot, MAX_PATH, L"%hs", cmdA + 10 + (cmdA[10] == ' ')); if (!stShot[0]) wcscpy(stShot, L"bbb_selftest.bmp"); }
    GetModuleFileNameW(NULL, kit, MAX_PATH); sl = wcsrchr(kit, L'\\');
    if (sl) *sl = 0;
    swprintf(e, MAX_PATH, L"%ls\\bbb_kit_folder.txt", kit); f = _wfopen(e, L"r");
    if (f) { char a[MAX_PATH]; if (fgets(a, sizeof a, f)) { a[strcspn(a, "\r\n")] = 0; if (a[0]) swprintf(kit, MAX_PATH, L"%hs", a); } fclose(f); }
    if (GetEnvironmentVariableW(L"BBB_KIT", e, MAX_PATH)) wcscpy(kit, e);
    (void)t; SetEnvironmentVariableW(L"BBB_KIT", kit);          /* the runner's kit folder: this one (its out folder is beside the exe) */
    hJob = CreateJobObjectW(NULL, NULL);
    if (hJob) { memset(&ji, 0, sizeof ji); ji.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE; SetInformationJobObject(hJob, JobObjectExtendedLimitInformation, &ji, sizeof ji); }
    { typedef BOOL(WINAPI *F)(HANDLE); F fn = (F)(void *)GetProcAddress(u32, "SetProcessDpiAwarenessContext"); if (!fn || !fn((HANDLE)-4)) SetProcessDPIAware(); }          /* sharp on every monitor */
    GdiplusStartup(&gtok, &gin, NULL); pf_load();
    sdc = GetDC(NULL); SC = GetDeviceCaps(sdc, LOGPIXELSY) / 96.0; ReleaseDC(NULL, sdc);
    SystemParametersInfoW(SPI_GETWORKAREA, 0, &wa, 0);
    {   /* the first size: 85 % of the screen (a self-test: the design size at the screen's DPI), and the scale that fits it */
        int tw = selftest ? (int)(DESIGN_W * SC) : (int)((wa.right - wa.left) * 0.85), th = selftest ? (int)(DESIGN_H * SC) : (int)((wa.bottom - wa.top) * 0.85);
        SC = fit_sc(tw, th); cw = (int)(tw / SC + 0.5); chh = (int)(th / SC + 0.5);
    }
    if (!selftest) ini(0);
    swprintf(liveDir, MAX_PATH, L"%ls\\out\\.live", kit); swprintf(liveTimes, MAX_PATH, L"%ls\\out\\live_times.txt", kit);          /* the live view's notes and time history */
    speed_env();          /* the speed switches, for every job started from here on */
    make_fonts();
#ifdef BBB_NK
    NK = nkui_init("Segoe UI", S(13)); nkui_fonts("Segoe UI", S(13), S(12), S(13), S(19)); nk_theme(nkTheme);
#endif
    wcscpy(head, L"Pick a style and a size"); wcscpy(sub, L"Then press Show choices. Nothing is built until you press Build.");
    { extern void kinds_init(void); extern char kd_themes_note[600]; kinds_init(); if (kd_themes_note[0]) swprintf(sub, 300, L"%.290hs", kd_themes_note); }          /* review P2-3: what happened with themes.json, at start */
    memset(&wc, 0, sizeof wc); wc.style = CS_DBLCLKS; wc.lpfnWndProc = proc; wc.hInstance = hi; wc.lpszClassName = L"bbb_kit"; wc.hCursor = LoadCursor(NULL, IDC_ARROW); wc.hIcon = LoadIconW(hi, MAKEINTRESOURCEW(1)); if (!wc.hIcon) wc.hIcon = LoadIcon(NULL, IDI_APPLICATION);          /* our icon (bbb_kit.rc) */ RegisterClassW(&wc);
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
    meters_tick(); SetTimer(hMain, 3, 500, NULL);          /* the PC's meters, twice a second */
    ShowWindow(hMain, selftest ? SW_SHOWNOACTIVATE : show); UpdateWindow(hMain);
    if (selftest == 4) mv_open();          /* --modelstest: the Models window (its timer writes the pictures and quits) */
    if (selftest) { char e[8]; if (GetEnvironmentVariableA("BBB_TEST_LOOK", e, sizeof e) >= 3) { lookPix = e[0] == '1'; lookTex = e[1] == '1'; lookFont = e[2] == '1'; } }          /* a test may ask for another look: 3 digits, pixel textures font */
    if (selftest == 2) {          /* --resizetest: make the window larger, then as small as it will go, and write down what it became */
        FILE *o; WCHAR q[MAX_PATH]; RECT a, b2, c2; LONG st = GetWindowLongW(hMain, GWL_STYLE); int i;
        GetClientRect(hMain, &a); SetWindowPos(hMain, NULL, 0, 0, a.right + S(520), a.bottom + S(300), SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE); GetClientRect(hMain, &b2);
        for (i = 0; i < 26; i++) { memset(&cells[i], 0, sizeof(Cell)); cells[i].state = ST_NONE; cells[i].n = i + 1; cells[i].tier = 4; cells[i].picked = i % 5 == 0; strcpy(cells[i].seed, "test"); swprintf(cells[i].theme, 64, L"sample %d", i + 1); }
        Sleep(500); meters_tick();          /* the meters need two readings to show anything */
        ncell = 26; wcscpy(head, L"Resize test: 26 sample cards"); PostMessageW(hMain, WM_CHAR, '1', 0); PostMessageW(hMain, WM_CHAR, '5', 0);
        { MSG m2; while (PeekMessageW(&m2, NULL, 0, 0, PM_REMOVE)) { TranslateMessage(&m2); DispatchMessageW(&m2); } }
        { char e[32]; int tw2, th2; if (GetEnvironmentVariableA("BBB_TEST_CLIENT", e, sizeof e) && sscanf(e, "%dx%d", &tw2, &th2) == 2) {          /* a test: the window's inside exactly this big (a screen size), one picture + the UI check, then quit */
            RECT wr = { 0, 0, tw2, th2 }; AdjustWindowRect(&wr, st, FALSE); SetWindowPos(hMain, NULL, 0, 0, wr.right - wr.left, wr.bottom - wr.top, SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
            { char e2[16]; if (GetEnvironmentVariableA("BBB_TEST_SIDEW", e2, sizeof e2)) { side_set(atoi(e2)); SendMessageW(hMain, WM_SIZE, 0, 0); } }          /* R1: the line here */
            { char e2[16]; if (GetEnvironmentVariableA("BBB_TEST_DETAILSH", e2, sizeof e2)) { showDetails = 1; det_set(atoi(e2)); SendMessageW(hMain, WM_SIZE, 0, 0); } }          /* R2: the Details pane open, this tall */
            if (GetEnvironmentVariableA("BBB_TEST_DRAG", e, sizeof e)) {          /* R1: a drag sent to the window itself (no real mouse): press on the line, move, let go */
                int x1, y1, x2, y2, want, which; FILE *tf; WCHAR p2[MAX_PATH];
                if (sscanf(e, "%d,%d,%d,%d", &x1, &y1, &x2, &y2) == 4) {
                    if (x1 < 0) x1 = S(sideW);          /* x1 -1: on the side panel's line, wherever the scale put it */
                    if (y1 < 0) { showDetails = 1; SendMessageW(hMain, WM_SIZE, 0, 0); y1 = det_top(); }          /* y1 -1: on the Details pane's line (R2) */
                    which = on_divider(x1, y1);
                    int *val = which == 2 ? &detailsH : &sideW, before = *val, keep, def;
                    SendMessageW(hMain, WM_MOUSEMOVE, 0, MAKELPARAM(x1, y1)); SendMessageW(hMain, WM_LBUTTONDOWN, MK_LBUTTON, MAKELPARAM(x1, y1));
                    double sc0 = SC; RECT rc0; GetClientRect(hMain, &rc0);
                    SendMessageW(hMain, WM_MOUSEMOVE, MK_LBUTTON, MAKELPARAM(x2, y2)); SendMessageW(hMain, WM_LBUTTONUP, 0, MAKELPARAM(x2, y2));
                    int after = *val, saved = -1, back = -1;
                    keep = which == 2 ? detWant : sideWant;
                    if (which == 2) { det_set((int)((rc0.bottom - FOOT_H * sc0 - y2) / sc0 + 0.5)); want = detailsH; det_set(DET_H0); def = detailsH; det_set(keep); }
                    else { side_set((int)(x2 / sc0 + 0.5)); want = sideW; side_set(SIDE_W0); def = sideW; side_set(keep); }
                    if (GetEnvironmentVariableA("BBB_TEST_INI", NULL, 0)) { ini(1); sideWant = SIDE_W0; detWant = DET_H0; side_fit(); det_fit(); ini(0); side_fit(); det_fit(); saved = *val; }          /* written, then read back */
                    if (GetEnvironmentVariableA("BBB_TEST_DBLCLK", NULL, 0)) { int bx = which == 2 ? x1 : S(sideW), by = which == 2 ? det_top() : y1; SendMessageW(hMain, WM_LBUTTONDBLCLK, MK_LBUTTON, MAKELPARAM(bx, by)); back = *val; }
                    int ok = which && after == want && after != before && (saved < 0 || saved == after) && (back < 0 || back == def);
                    swprintf(p2, MAX_PATH, L"%ls.drag.txt", stShot); tf = _wfopen(p2, L"w");
                    if (tf) { fprintf(tf, "%s %s %d -> %d (want %d) scale %.2f -> %.2f saved+read %d dblclk %d (default %d)" "\n", ok ? "OK" : "FAIL", which == 2 ? "detailsh" : which == 1 ? "sidew" : "no line at the press:", before, after, want, sc0, SC, saved, back, def); fclose(tf); }
                }
            }
            if (GetEnvironmentVariableA("BBB_TEST_FOOT", NULL, 0)) {          /* R4: Show and Build straight away drawn fully inside the window */
                FILE *tf; WCHAR p2[MAX_PATH]; RECT cr; int k, show = 0, quick = 0; GetClientRect(hMain, &cr); capture(NULL);
                for (k = 0; k < nhit; k++) { RECT q = hits[k].r; int in = q.left >= 0 && q.top >= 0 && q.right <= cr.right && q.bottom <= cr.bottom;
                    if (hits[k].kind == H_SHOW) show = in ? 1 : -1;
                    if (hits[k].kind == H_QUICK) quick = in ? 1 : -1; }
                swprintf(p2, MAX_PATH, L"%ls.foot.txt", stShot); tf = _wfopen(p2, L"w");
                if (tf) { fprintf(tf, "%s show %d quick %d side_max %d window %ldx%ld" "\n", show == 1 && quick == 1 ? "OK" : "FAIL", show, quick, sideMax, cr.right, cr.bottom); fclose(tf); }
            }
            if (GetEnvironmentVariableA("BBB_TEST_TIP", NULL, 0)) {          /* R3: the cut labels, and the first one's tooltip after a (sent) rest of the mouse */
                FILE *tf; WCHAR p2[MAX_PATH]; int n0;
                capture(NULL); n0 = ntip;
                if (n0) { RECT tr = tips[0].r; WCHAR first[200]; wcscpy(first, tips[0].s);
                    SendMessageW(hMain, WM_MOUSEMOVE, 0, MAKELPARAM((tr.left + tr.right) / 2, (tr.top + tr.bottom) / 2)); SendMessageW(hMain, WM_TIMER, 7, 0);
                    swprintf(p2, MAX_PATH, L"%ls.tip.bmp", stShot); capture(p2);
                    swprintf(p2, MAX_PATH, L"%ls.tip.txt", stShot); tf = _wfopen(p2, L"w");
                    if (tf) { fprintf(tf, "%s cut %d first \"%ls\" shown %d" "\n", tipShown && !wcscmp(tipText, first) ? "OK" : "FAIL", n0, first, tipShown); fprintf(tf, "sidew %d main_need %d lookW %d scale %.2f" "\n", sideW, main_need(), lookW, SC); fclose(tf); }
                    tipShown = 0; tipCur = -1; }
                else { swprintf(p2, MAX_PATH, L"%ls.tip.txt", stShot); tf = _wfopen(p2, L"w"); if (tf) { fprintf(tf, "OK cut 0" "\n"); fprintf(tf, "sidew %d main_need %d lookW %d scale %.2f" "\n", sideW, main_need(), lookW, SC); fclose(tf); } }
            }
            if (GetEnvironmentVariableA("BBB_LOOK_SAMPLES", NULL, 0)) {          /* F1: cards building, built, failed, waiting, for the contrast test */
                cells[1].state = ST_BUILDING; cells[2].state = ST_OK; cells[2].secs = 38; cells[3].state = ST_VARIANT; cells[4].state = ST_FAIL; cells[7].state = ST_PENDING; busy = 2; wanted = 8; doneN = 3; anim = 5; }
            capture(stShot);
            if (GetEnvironmentVariableA("BBB_TEST_TEXTS", NULL, 0)) {          /* F1: every text's box and colour, for work\\rest\\contrast.py */
                WCHAR p2[MAX_PATH]; FILE *tf; int k; swprintf(p2, MAX_PATH, L"%ls.texts.txt", stShot); tf = _wfopen(p2, L"w, ccs=UTF-8");
                if (tf) { for (k = 0; k < ntext; k++) fwprintf(tf, L"%ld %ld %ld %ld %08X %ls" L"\n", texts[k].r.left, texts[k].r.top, texts[k].r.right, texts[k].r.bottom, texts[k].c, texts[k].s); fclose(tf); }
            }
            ui_check(L"screen"); return 0; } }
        if (GetEnvironmentVariableA("BBB_TEST_SEEDKEYS", NULL, 0)) {          /* a test: typing f, d, Enter in the seed box must not fire the shortcuts */
            int lf = lookFont, sd = showDetails, bz = busy, nc = ncell; WPARAM ks[3] = { 'F', 'D', VK_RETURN }, cs[3] = { 'f', 'd', 13 }; FILE *tf; WCHAR p2[MAX_PATH];
            seedEdit = 1; seedBuf[0] = 0;
            for (i = 0; i < 3; i++) { SendMessageW(hMain, WM_KEYDOWN, ks[i], 0); SendMessageW(hMain, WM_CHAR, cs[i], 0); }
            swprintf(p2, MAX_PATH, L"%ls.seedkeys.txt", stShot); tf = _wfopen(p2, L"w");
            if (tf) { fprintf(tf, "%s font %d->%d details %d->%d busy %d->%d cards %d->%d seed \"%s\" editing %d\n", lookFont == lf && showDetails == sd && busy == bz && ncell == nc && !strcmp(seedBuf, "fd") && !seedEdit ? "OK" : "FAIL",
                               lf, lookFont, sd, showDetails, bz, busy, nc, ncell, seedBuf, seedEdit); fclose(tf); }
            return 0;
        }
        if (GetEnvironmentVariableA("BBB_LOOK_SAMPLES", NULL, 0)) {          /* only to look at the drawing: a few cards building, built, failed, waiting, and a half-full progress bar, in the first picture */
            cells[1].state = ST_BUILDING; cells[2].state = ST_OK; cells[2].secs = 38; cells[3].state = ST_VARIANT; cells[4].state = ST_FAIL; cells[7].state = ST_PENDING; busy = 2; wanted = 8; doneN = 3; anim = 5;
            capture(stShot); busy = 0; for (i = 1; i < 8; i++) if (i != 5) cells[i].state = ST_NONE;
        } else capture(stShot);
        if (GetEnvironmentVariableA("BBB_TEST_UICHECK", NULL, 0)) ui_check(L"large");
        SetWindowPos(hMain, NULL, 0, 0, 200, 200, SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE); GetClientRect(hMain, &c2);
        { char e[8]; if (GetEnvironmentVariableA("BBB_TEST_SIDEOFF", e, sizeof e)) { capture(NULL); sideOff = atoi(e); } }          /* a test: the side panel scrolled (one paint first, so sideMax is known) */
        swprintf(q, MAX_PATH, L"%ls.small.bmp", stShot); capture(q);
        if (GetEnvironmentVariableA("BBB_TEST_UICHECK", NULL, 0)) ui_check(L"small");
        if (GetEnvironmentVariableA("BBB_TEST_SIDEOFF", NULL, 0)) {          /* a test: click Allow overhangs where it is drawn on the scrolled panel; did it switch? */
            int k, was = selSup, hk = -1; FILE *tf; WCHAR p2[MAX_PATH];
            for (k = 0; k < nhit; k++) if (hits[k].kind == H_SUP) { int cx = (hits[k].r.left + hits[k].r.right) / 2, cy = (hits[k].r.top + hits[k].r.bottom) / 2; hk = k;
                SendMessageW(hMain, WM_MOUSEMOVE, 0, MAKELPARAM(cx, cy)); SendMessageW(hMain, WM_LBUTTONDOWN, MK_LBUTTON, MAKELPARAM(cx, cy)); SendMessageW(hMain, WM_LBUTTONUP, 0, MAKELPARAM(cx, cy)); break; }
            swprintf(p2, MAX_PATH, L"%ls.click.bmp", stShot); capture(p2);
            swprintf(p2, MAX_PATH, L"%ls.click.txt", stShot); tf = _wfopen(p2, L"w");
            if (tf) { fprintf(tf, "side_off %d side_max %d found %d overhangs_before %d after %d\n", sideOff, sideMax, hk >= 0, was, selSup); fclose(tf); }
        }
        SetWindowPos(hMain, NULL, 0, 0, S(1310), S(850), SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE); showHelp = 1; swprintf(q, MAX_PATH, L"%ls.help.bmp", stShot); capture(q); showHelp = 0; swprintf(q, MAX_PATH, L"%ls.txt", stShot); o = _wfopen(q, L"w");
        if (o) { fprintf(o, "how_many_after_typing_1_5 %d\n", selCount); fprintf(o, "thick_frame %d\nmaximize_box %d\nstart %ldx%ld\nlarger %ldx%ld\nsmallest %ldx%ld\nscroll_max_small %d\n", !!(st & WS_THICKFRAME), !!(st & WS_MAXIMIZEBOX), a.right, a.bottom, b2.right, b2.bottom, c2.right, c2.bottom, scrollMax); fclose(o); }
        return 0;
    }
    if (selftest) { char e[8]; if (GetEnvironmentVariableA("BBB_TEST_SIDEOFF", e, sizeof e)) sideOff = atoi(e); }          /* a test: the side panel scrolled this far (clamped when drawn) */
    if (selftest) { char e[8]; if (GetEnvironmentVariableA("BBB_TEST_SCALE", e, sizeof e)) selScale = atoi(e) % 3; }          /* a test may ask for another game scale: 0 = 32 mm, 1 = 28 mm, 2 = 15 mm */
    if (selftest) { char e[16]; int i; if (GetEnvironmentVariableA("BBB_TEST_SIZE", e, sizeof e)) for (i = 0; i < NSIZES; i++) if (!strcmp(e, SIZES[i])) stSize = i; }          /* and another base size, as written on its chip: 60, 90x52 ... */
    if (selftest == 3) { selStyle = 14; selSize = 8; selDetail = 4; selMini = 0; selCount = 8; selSup = 0; ask_choices(1); }
    if (selftest == 1 && GetEnvironmentVariableA("BBB_TEST_MULTI", NULL, 0)) {          /* a test: sizes 25, 40, 60, two of each, build straight away (review P2-6) */
        char e[64], *t; int q; selftest = 5; selStyle = 6; selDetail = 4; selMini = 0; selCount = 2; sizeMulti = 1; sizeMask = 0; GetEnvironmentVariableA("BBB_TEST_MULTI", e, sizeof e);
        for (t = strtok(e, ","); t; t = strtok(NULL, ",")) for (q = 0; q < NSIZES; q++) if (!strcmp(SIZES[q], t)) sizeMask |= 1u << q;
        act(H_QUICK, 0);
    }
    else if (selftest == 1) { selStyle = 6; selSize = stSize; selDetail = 4; selMini = 0; selCount = autoBuild ? 2 : 8; selSup = GetEnvironmentVariableA("BBB_TEST_OVERHANGS", NULL, 0) ? 1 : 0; ask_choices(1); }          /* a test of overhangs + join: BBB_TEST_OVERHANGS set */
    if (selftest == 6) SetTimer(hMain, 9, 2000, NULL);          /* --idletest: two seconds to settle, then the counting */
    while (GetMessageW(&msg, NULL, 0, 0) > 0) { TranslateMessage(&msg); DispatchMessageW(&msg); }
    GdiplusShutdown(gtok); return 0;
}
