/* theme_sample.c (PLAN-better U4, 2026-11-22): a picture of a theme beside its name. Resting the mouse on the chosen
   theme in the side panel shows a small drawing of one base of that theme. Each picture is made once, by this exe
   itself in a hidden job (--job choices <style> --theme <name> --count 1, seed "tsample", 40 mm, detail 3), and kept in
   out\theme_samples\<style>_<theme>.png; after that it shows at once. Included by bbb_kit.c (it uses its globals). */
static GpImage *thImg; static char thKey[160]; static HANDLE thProc; static WCHAR thDir[MAX_PATH]; static int thShow, thX, thY;
static void th_file(WCHAR *p, size_t cap, const char *key) { swprintf(p, cap, L"%ls\\out\\theme_samples\\%hs.png", kit, key); }
static void th_key(char *key, size_t cap) {
    const char *tn = theme_name(); size_t i;
    snprintf(key, cap, "%s_%s", style_name(), tn ? tn : "");
    for (i = 0; key[i]; i++) if (!((key[i] >= 'a' && key[i] <= 'z') || (key[i] >= 'A' && key[i] <= 'Z') || (key[i] >= '0' && key[i] <= '9') || key[i] == '_')) key[i] = '_';
}
static void th_start(const char *key) {          /* the hidden job that draws the picture */
    WCHAR exe[MAX_PATH], cmd[MAX_PATH * 2], extra[200] = L"", old[MAX_PATH] = L"", d[MAX_PATH]; DWORD had; STARTUPINFOW si; PROCESS_INFORMATION pi;
    swprintf(d, MAX_PATH, L"%ls\\out", kit); CreateDirectoryW(d, NULL); swprintf(d, MAX_PATH, L"%ls\\out\\theme_samples", kit); CreateDirectoryW(d, NULL);
    swprintf(thDir, MAX_PATH, L"%ls\\out\\theme_samples\\job_%hs", kit, key);
    GetModuleFileNameW(NULL, exe, MAX_PATH); theme_opt(extra, 200);
    swprintf(cmd, MAX_PATH * 2, L"\"%ls\" --job choices %hs --size 40 --detail 3 --count 1 --seed tsample --obj 3 --figure 32%ls", exe, style_name(), extra);
    had = GetEnvironmentVariableW(L"BBB_OUT_ROOT", old, MAX_PATH);
    SetEnvironmentVariableW(L"BBB_OUT_ROOT", thDir);          /* the job's out folder (the child takes this environment) */
    memset(&si, 0, sizeof si); si.cb = sizeof si; si.dwFlags = STARTF_USESHOWWINDOW; si.wShowWindow = SW_HIDE;
    if (CreateProcessW(NULL, cmd, NULL, NULL, FALSE, CREATE_NO_WINDOW | BELOW_NORMAL_PRIORITY_CLASS, NULL, NULL, &si, &pi)) { thProc = pi.hProcess; CloseHandle(pi.hThread); SetTimer(hMain, 8, 400, NULL); }
    SetEnvironmentVariableW(L"BBB_OUT_ROOT", had ? old : NULL);
}
static int th_newest(const WCHAR *dir, WCHAR *out) {          /* the newest choices\<time>\c_tsample_*.png under dir */
    WCHAR pat[MAX_PATH], sub2[MAX_PATH]; WIN32_FIND_DATAW f, g; HANDLE h, h2; FILETIME best = { 0, 0 }; int found = 0;
    swprintf(pat, MAX_PATH, L"%ls\\choices\\*", dir);
    if ((h = FindFirstFileW(pat, &f)) == INVALID_HANDLE_VALUE) return 0;
    do {
        if (!(f.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) || f.cFileName[0] == L'.') continue;
        swprintf(sub2, MAX_PATH, L"%ls\\choices\\%ls\\c_tsample_*.png", dir, f.cFileName);
        if ((h2 = FindFirstFileW(sub2, &g)) != INVALID_HANDLE_VALUE) {
            do { if (CompareFileTime(&g.ftLastWriteTime, &best) > 0) { best = g.ftLastWriteTime; swprintf(out, MAX_PATH, L"%ls\\choices\\%ls\\%ls", dir, f.cFileName, g.cFileName); found = 1; } } while (FindNextFileW(h2, &g));
            FindClose(h2);
        }
    } while (FindNextFileW(h, &f));
    FindClose(h); return found;
}
static void th_poll(void) {          /* timer 8: the job ended? keep its picture */
    WCHAR src[MAX_PATH], dst[MAX_PATH];
    if (!thProc || WaitForSingleObject(thProc, 0) != WAIT_OBJECT_0) return;
    CloseHandle(thProc); thProc = NULL; KillTimer(hMain, 8);
    if (th_newest(thDir, src)) { th_file(dst, MAX_PATH, thKey); CopyFileW(src, dst, FALSE); if (!thImg) GdipLoadImageFromFile(dst, &thImg); }
    if (thShow) redraw();
}
static void th_hover(int on, int x, int y) {          /* from hover_at: the mouse is (on 1) or is not on the chosen theme's name */
    char key[160]; WCHAR p[MAX_PATH];
    if (!on || !theme_name()) { if (thShow) { thShow = 0; redraw(); } return; }
    th_key(key, sizeof key);
    if (strcmp(key, thKey)) { if (thImg) GdipDisposeImage(thImg); thImg = NULL; snprintf(thKey, sizeof thKey, "%s", key); }
    if (!thImg) { th_file(p, MAX_PATH, key); if (GetFileAttributesW(p) != INVALID_FILE_ATTRIBUTES) GdipLoadImageFromFile(p, &thImg); else if (!thProc) th_start(key); }
    if (!thShow) { thShow = 1; thX = x; thY = y; redraw(); }
}
static void th_draw(int W, int H) {          /* at the end of paint: the picture, or a note while it is made */
    int s = S(200), bx, by;
    if (!thShow) return;
    bx = thX + S(18); by = thY + S(18);
    if (bx + s + S(8) > W) bx = W - s - S(8);
    if (by + s + S(28) > H) by = thY - s - S(36);
    if (bx < S(4)) bx = S(4);
    if (by < S(4)) by = S(4);
    rr(bx - S(4), by - S(4), s + S(8), s + S(30), S(6), C_CARD, C_ACC, 1);
    if (thImg) GdipDrawImageRectI(G, thImg, bx, by, s, s);
    else txt(L"drawing a sample of this theme ...", bx, by, s, s, fSmall, C_DIM, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    { WCHAR t[120]; const char *tn = theme_name(); swprintf(t, 120, L"%hs: %hs", style_name(), tn ? tn : ""); tipNo = 1; txt(t, bx, by + s + S(2), s, S(20), fSmall, C_TEXT, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS); tipNo = 0; }
}
