/* geos_dyn.c: finds shapely's geos_c.dll and fills the GeosApi table (see geos_dyn.h) */
#include <windows.h>
#include <stdio.h>
#include <string.h>
#include "geos_dyn.h"

GeosApi G;
GEOSContextHandle_t gctx;
char geos_err[512];

static void on_err(const char *msg, void *u) { (void)u; snprintf(geos_err, sizeof geos_err, "%s", msg); }
static void on_note(const char *msg, void *u) { (void)msg; (void)u; }

static HMODULE try_dir(const char *dir) {
    char pat[MAX_PATH]; WIN32_FIND_DATAA fd; HANDLE h; HMODULE m = NULL;
    snprintf(pat, sizeof pat, "%s\\geos_c*.dll", dir);
    h = FindFirstFileA(pat, &fd);
    if (h == INVALID_HANDLE_VALUE) return NULL;
    FindClose(h);
    snprintf(pat, sizeof pat, "%s\\%s", dir, fd.cFileName);
    SetDllDirectoryA(dir);          /* geos_c needs its geos-*.dll and msvcp140 from the same folder */
    m = LoadLibraryA(pat);
    SetDllDirectoryA(NULL);
    return m;
}

int geos_load(void) {
    HMODULE m = NULL; char p[MAX_PATH], *s;
    if (GetEnvironmentVariableA("BBB_GEOS", p, sizeof p)) {
        s = strrchr(p, '\\'); if (s) { *s = 0; SetDllDirectoryA(p); *s = '\\'; }
        m = LoadLibraryA(p); SetDllDirectoryA(NULL);
    }
    if (!m && GetModuleFileNameA(NULL, p, sizeof p)) { s = strrchr(p, '\\'); if (s) { strcpy(s, "\\geos"); m = try_dir(p); } }
    if (!m && GetEnvironmentVariableA("APPDATA", p, sizeof p)) {
        char d[MAX_PATH]; snprintf(d, sizeof d, "%s\\Python\\Python315\\site-packages\\shapely.libs", p); m = try_dir(d);
    }
    if (!m) { fprintf(stderr, "geos_c.dll not found (set BBB_GEOS)\n"); return 0; }
#define GEOS_GET(ret, name, args) G.name = (ret (*) args)(void *)GetProcAddress(m, #name); if (!G.name) { fprintf(stderr, "geos_c.dll has no %s\n", #name); return 0; }
    GEOS_FNS(GEOS_GET)
#undef GEOS_GET
    gctx = G.GEOS_init_r();
    G.GEOSContext_setErrorMessageHandler_r(gctx, on_err, NULL);
    (void)on_note;
    return 1;
}
