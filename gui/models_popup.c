/* models_popup.c: the Models preview (2026-11-20), included by bbb_kit.c. The Models button opens a window of every model
   the kit uses, each as the kit's own renderer draws it at its real size (bbb_kit.exe --job models, cport\models_job.c),
   with its name, printed length and height, and the styles that use it. It scrolls (wheel, the bar), and shows all styles or
   only the one chosen on the left. The pictures are made once into out\model_previews and kept; while the job runs they
   appear as they are ready. */
typedef struct { WCHAR png[MAX_PATH]; char name[80], styles[420], rel[300]; int L, Hh; GpImage *img; } MvTile;
static MvTile *MV; static int nMV, capMV, mvScroll, mvMax, mvFilter = 0, mvShown; static HWND hMv; static HANDLE mvProc; static long mvSize;
enum { MV_ALL = 1000, MV_STYLE = 1001 }; static RECT mvR[8]; static int mvK[8];
static void mv_paths(WCHAR *dir, WCHAR *idx, WCHAR *tmp) {
    WCHAR r[MAX_PATH]; DWORD n = GetEnvironmentVariableW(L"BBB_OUT_ROOT", r, MAX_PATH);
    if (n && n < MAX_PATH) swprintf(dir, MAX_PATH, L"%ls\\model_previews", r); else swprintf(dir, MAX_PATH, L"%ls\\out\\model_previews", kit);
    swprintf(idx, MAX_PATH, L"%ls\\index.tsv", dir); swprintf(tmp, MAX_PATH, L"%ls\\index.tmp", dir);
}
static int mv_running(void) { DWORD c = 0; return mvProc && GetExitCodeProcess(mvProc, &c) && c == STILL_ACTIVE; }
static void mv_load(void) {          /* new lines of the index: the one being written while the job runs, else the finished one */
    WCHAR dir[MAX_PATH], idx[MAX_PATH], tmp[MAX_PATH]; FILE *f; char line[2000]; int add = 0;
    mv_paths(dir, idx, tmp); f = _wfopen(mv_running() ? tmp : idx, L"rb"); if (!f && !mv_running()) f = _wfopen(tmp, L"rb");
    if (!f) return;
    while (fgets(line, sizeof line, f)) {
        char *c[6]; int k = 0, i; char *p = line; MvTile *t;
        for (c[k++] = p; *p && k < 6; p++) if (*p == '\t') { *p = 0; c[k++] = p + 1; }
        if (k < 6) continue;
        { char *e = c[5] + strlen(c[5]); while (e > c[5] && (e[-1] == '\n' || e[-1] == '\r')) *--e = 0; }
        WCHAR w[MAX_PATH]; MultiByteToWideChar(CP_UTF8, 0, c[0], -1, w, MAX_PATH);
        for (i = 0; i < nMV && wcscmp(MV[i].png, w); i++) {}
        if (i < nMV) continue;
        if (nMV == capMV) { capMV = capMV ? 2 * capMV : 512; MV = (MvTile *)realloc(MV, sizeof *MV * (size_t)capMV); }
        t = &MV[nMV++]; memset(t, 0, sizeof *t); wcscpy(t->png, w);
        snprintf(t->name, sizeof t->name, "%s", c[1]); snprintf(t->styles, sizeof t->styles, "%s", c[2]); t->L = atoi(c[3]); t->Hh = atoi(c[4]); snprintf(t->rel, sizeof t->rel, "%s", c[5]);
        for (i = 0; t->name[i]; i++) if (t->name[i] == '_') t->name[i] = ' ';
        GdipLoadImageFromFile(t->png, &t->img); add++;
    }
    fclose(f);
    if (add && hMv) InvalidateRect(hMv, NULL, FALSE);
}
static int mv_pass(const MvTile *t) { return mvFilter == 0 || strstr(t->styles, STYLES[selStyle]) != NULL; }
static void mv_paint(HDC out, int W, int H) {
    HDC keepDC = DC; GpGraphics *keepG = G; HBITMAP bm; HGDIOBJ obm; int i, n = 0, x0 = S(16), top = S(84), tw = S(190), th = S(236), gap = S(10), cols, vis = 0; WCHAR b[300];
    int nh0 = nhit;          /* the popup's clicks are its own; the main window's list is left as it was */
    DC = CreateCompatibleDC(out); bm = CreateCompatibleBitmap(out, W, H); obm = SelectObject(DC, bm); SetBkMode(DC, TRANSPARENT);
    GdipCreateFromHDC(DC, &G); GdipSetSmoothingMode(G, SmoothingModeAntiAlias); GdipSetInterpolationMode(G, InterpolationModeHighQualityBicubic);
    fillrect(0, 0, W, H, C_BG); fillrect(0, 0, W, top - S(8), C_SIDE);
    for (i = 0; i < nMV; i++) if (mv_pass(&MV[i])) vis++;
    swprintf(b, 300, L"Models in the kit: %d", vis); ltxt(b, x0, S(10), W / 2, S(26), fTitle, C_TEXT, DT_LEFT | DT_SINGLELINE, 2, -1);
    if (mv_running()) swprintf(b, 300, L"Drawing the models at their true size beside a 32 mm man (1:56)... %d so far. Made once, kept after.", nMV);
    else swprintf(b, 300, L"Every model at its true size beside a 32 mm man (1:56), as the kit prints it. Wheel or bar to scroll, Esc to close.");
    txt(b, x0, S(40), W - 2 * x0, S(18), fSmall, C_DIM, DT_LEFT | DT_SINGLELINE | DT_END_ELLIPSIS);
    {   int cw = need_w(L"All styles", S(110)); WCHAR s2[80]; swprintf(s2, 80, L"%hs only", STYLES[selStyle]);
        chip("All styles", W - x0 - cw - S(8) - need_w(s2, S(130)), S(12), cw, S(24), mvFilter == 0, MV_ALL, 0);
        { char s3[80]; snprintf(s3, sizeof s3, "%s only", STYLES[selStyle]); chip(s3, W - x0 - need_w(s2, S(130)), S(12), need_w(s2, S(130)), S(24), mvFilter == 1, MV_STYLE, 0); } }
    cols = (W - 2 * x0 - S(10) + gap) / (tw + gap); if (cols < 1) cols = 1;
    { int rows = (vis + cols - 1) / cols, total = rows * (th + gap); mvMax = total > H - top ? total - (H - top) + S(8) : 0; if (mvScroll > mvMax) mvScroll = mvMax; if (mvScroll < 0) mvScroll = 0; }
    { GpRegion *rg = NULL; GdipSetClipRectI(G, 0, top - S(4), W, H - top + S(4), CombineModeReplace); (void)rg; }
    for (i = 0; i < nMV; i++) {
        MvTile *t = &MV[i]; int x, y;
        if (!mv_pass(t)) continue;
        x = x0 + (n % cols) * (tw + gap); y = top + (n / cols) * (th + gap) - mvScroll; n++;
        if (y + th < top || y > H) continue;
        rr(x, y, tw, th, S(6), C_CARD, C_LINE, 1);
        if (t->img) image_fit(t->img, x + S(6), y + S(6), tw - S(12), tw - S(30));
        { WCHAR nm[100]; swprintf(nm, 100, L"%hs", t->name); txt(nm, x + S(8), y + tw - S(22), tw - S(16), S(18), fUI, C_TEXT, DT_LEFT | DT_SINGLELINE | DT_END_ELLIPSIS); }
        swprintf(b, 300, L"longest side %d mm, %d mm high", t->L, t->Hh); txt(b, x + S(8), y + tw - S(2), tw - S(16), S(16), fSmall, C_DIM, DT_LEFT | DT_SINGLELINE);
        swprintf(b, 300, L"%hs", t->styles); txt(b, x + S(8), y + tw + S(14), tw - S(16), S(32), fSmall, C_ACC, DT_LEFT | DT_WORDBREAK | DT_END_ELLIPSIS);
    }
    GdipResetClip(G);
    if (mvMax > 0) { int vh = H - top, tht = (int)((double)vh * vh / (vh + mvMax)), ty = top + (int)((double)mvScroll / mvMax * (vh - tht)); rr(W - S(10), ty, S(6), tht, S(3), C_LINE, 0, 0); }
    GdipDeleteGraphics(G); BitBlt(out, 0, 0, W, H, DC, 0, 0, SRCCOPY); SelectObject(DC, obm); DeleteObject(bm); DeleteDC(DC);
    G = keepG; DC = keepDC;
    {   /* this window's clicks: what was hit() while drawing it, moved to its own list */
        static RECT mr[8]; static int mk[8]; int k, m = 0;
        for (k = nh0; k < nhit && m < 8; k++) { mr[m] = hits[k].r; mk[m] = hits[k].kind; m++; }
        nhit = nh0; mvShown = m; memcpy(mvR, mr, sizeof mr); memcpy(mvK, mk, sizeof mk);
    }
}
static void mv_capture(const WCHAR *path) {
    RECT r; HDC sdc, dc; HBITMAP bm; BITMAPINFO bi; BITMAPFILEHEADER fh; void *bits; int w, h; DWORD n, wr; HANDLE f; HGDIOBJ o;
    GetClientRect(hMv, &r); w = r.right; h = r.bottom; sdc = GetDC(NULL); dc = CreateCompatibleDC(sdc);
    memset(&bi, 0, sizeof bi); bi.bmiHeader.biSize = sizeof bi.bmiHeader; bi.bmiHeader.biWidth = w; bi.bmiHeader.biHeight = h; bi.bmiHeader.biPlanes = 1; bi.bmiHeader.biBitCount = 24;
    bm = CreateDIBSection(sdc, &bi, DIB_RGB_COLORS, &bits, NULL, 0); o = SelectObject(dc, bm); mv_paint(dc, w, h); GdiFlush();
    n = ((w * 3 + 3) & ~3) * h; memset(&fh, 0, sizeof fh); fh.bfType = 0x4D42; fh.bfOffBits = sizeof fh + sizeof bi.bmiHeader; fh.bfSize = fh.bfOffBits + n;
    f = CreateFileW(path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL);
    if (f != INVALID_HANDLE_VALUE) { WriteFile(f, &fh, sizeof fh, &wr, NULL); WriteFile(f, &bi.bmiHeader, sizeof bi.bmiHeader, &wr, NULL); WriteFile(f, bits, n, &wr, NULL); CloseHandle(f); }
    SelectObject(dc, o); DeleteObject(bm); DeleteDC(dc); ReleaseDC(NULL, sdc);
}
static LRESULT CALLBACK mv_proc(HWND h, UINT m, WPARAM w, LPARAM l) {
    switch (m) {
    case WM_TIMER:
        mv_load();
        if (selftest == 4 && !mv_running()) {          /* --modelstest: the popup's picture once every model is drawn */
            WCHAR q[MAX_PATH]; mv_load(); swprintf(q, MAX_PATH, L"%ls", stShot); mv_capture(q);
            mvScroll = 1 << 30; swprintf(q, MAX_PATH, L"%ls.end.bmp", stShot); mv_capture(q);          /* and scrolled to the end */
            mvFilter = 1; mvScroll = 0; swprintf(q, MAX_PATH, L"%ls.style.bmp", stShot); mv_capture(q);
            { FILE *tf; swprintf(q, MAX_PATH, L"%ls.txt", stShot); tf = _wfopen(q, L"w"); if (tf) { int i, v = 0; for (i = 0; i < nMV; i++) v += mv_pass(&MV[i]); fprintf(tf, "models %d style_only %d scroll_max %d\n", nMV, v, mvMax); fclose(tf); } }
            PostQuitMessage(0);
        }
        return 0;
    case WM_MOUSEWHEEL: mvScroll -= GET_WHEEL_DELTA_WPARAM(w) * S(120) / 120; InvalidateRect(h, NULL, FALSE); return 0;
    case WM_LBUTTONDOWN: { int x = GET_X_LPARAM(l), y = GET_Y_LPARAM(l), k; RECT cr; POINT pt = { x, y };
        for (k = 0; k < mvShown; k++) if (PtInRect(&mvR[k], pt)) { mvFilter = mvK[k] == MV_STYLE; mvScroll = 0; InvalidateRect(h, NULL, FALSE); return 0; }
        GetClientRect(h, &cr); if (x > cr.right - S(16) && mvMax > 0) { mvScroll = (int)((double)(y - S(84)) / (cr.bottom - S(84)) * mvMax); InvalidateRect(h, NULL, FALSE); }
        return 0; }
    case WM_MOUSEMOVE: if ((w & MK_LBUTTON)) { RECT cr; GetClientRect(h, &cr); if (GET_X_LPARAM(l) > cr.right - S(16) && mvMax > 0) { mvScroll = (int)((double)(GET_Y_LPARAM(l) - S(84)) / (cr.bottom - S(84)) * mvMax); InvalidateRect(h, NULL, FALSE); } } return 0;
    case WM_KEYDOWN: if (w == VK_ESCAPE) DestroyWindow(h); else if (w == VK_NEXT || w == VK_DOWN) { mvScroll += w == VK_NEXT ? S(400) : S(60); InvalidateRect(h, NULL, FALSE); } else if (w == VK_PRIOR || w == VK_UP) { mvScroll -= w == VK_PRIOR ? S(400) : S(60); InvalidateRect(h, NULL, FALSE); } return 0;
    case WM_SIZE: InvalidateRect(h, NULL, FALSE); return 0;
    case WM_ERASEBKGND: return 1;
    case WM_PAINT: { PAINTSTRUCT ps; RECT r; HDC dc = BeginPaint(h, &ps); GetClientRect(h, &r); if (r.right > 0 && r.bottom > 0) mv_paint(dc, r.right, r.bottom); EndPaint(h, &ps); return 0; }
    case WM_DESTROY: { int i; KillTimer(h, 1); for (i = 0; i < nMV; i++) if (MV[i].img) GdipDisposeImage(MV[i].img); nMV = 0; hMv = NULL; if (selftest == 4) PostQuitMessage(0); return 0; }
    }
    return DefWindowProcW(h, m, w, l);
}
static void mv_open(void) {          /* the Models button: the window, and the job that makes any pictures still missing */
    static int reg; WNDCLASSW wc; RECT r; WCHAR exe[MAX_PATH], cmd[MAX_PATH + 40]; STARTUPINFOW si; PROCESS_INFORMATION pi;
    if (hMv) { SetForegroundWindow(hMv); return; }
    if (!reg) { memset(&wc, 0, sizeof wc); wc.lpfnWndProc = mv_proc; wc.hInstance = GetModuleHandleW(NULL); wc.lpszClassName = L"bbb_models"; wc.hCursor = LoadCursor(NULL, IDC_ARROW); wc.hIcon = LoadIconW(wc.hInstance, MAKEINTRESOURCEW(1)); RegisterClassW(&wc); reg = 1; }
    GetWindowRect(hMain, &r);
    hMv = CreateWindowExW(selftest ? WS_EX_NOACTIVATE | WS_EX_TOOLWINDOW : 0, L"bbb_models", L"Models in the kit", WS_OVERLAPPEDWINDOW,
                          selftest ? -8000 : r.left + (r.right - r.left) / 10, selftest ? -8000 : r.top + (r.bottom - r.top) / 12, (r.right - r.left) * 8 / 10, (r.bottom - r.top) * 5 / 6, NULL, NULL, GetModuleHandleW(NULL), NULL);
    if (!mv_running()) {
        GetModuleFileNameW(NULL, exe, MAX_PATH); swprintf(cmd, MAX_PATH + 40, L"\"%ls\" --job models", exe);
        memset(&si, 0, sizeof si); si.cb = sizeof si; si.dwFlags = STARTF_USESHOWWINDOW; si.wShowWindow = SW_HIDE;
        if (mvProc) { CloseHandle(mvProc); mvProc = NULL; }
        if (CreateProcessW(NULL, cmd, NULL, NULL, FALSE, CREATE_NO_WINDOW, NULL, NULL, &si, &pi)) { mvProc = pi.hProcess; CloseHandle(pi.hThread); }
    }
    mv_load(); SetTimer(hMv, 1, 400, NULL);
    ShowWindow(hMv, selftest ? SW_SHOWNOACTIVATE : SW_SHOW); UpdateWindow(hMv);
}
