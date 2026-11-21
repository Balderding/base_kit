/* the UI check (2026-11-20), part of bbb_kit.c's --resizetest when BBB_TEST_UICHECK is set: at the size the window has now,
   every click area is checked (two that overlap, one cut off by the window's edge), then every safe one is clicked where it
   is drawn and the window's state is compared before and after (each must change something), and put back. Clicks that
   start a job or open a folder are not made (Show, Build, Quick, More, Stop, Open folder). Writes <shot>.<tag>.uicheck.txt. */
static void ui_state(char *s, size_t n) {
    int i, pk = 0; for (i = 0; i < ncell; i++) pk += cells[i].picked;
    snprintf(s, n, "st%d sz%d de%d mi%d n%d sup%d join%d sc%d obj%d mf%d%d%d%d thr%d gpu%d pix%d tex%d font%d view%d parts%d p%d%d%d%d%d det%d help%d picks%d col%d",
             selStyle, selSize, selDetail, selMini, selCount, selSup, selJoin, selScale, selObj, mfOn[0], mfOn[1], mfOn[2], mfOn[3], spThr, spGpu, lookPix, lookTex, lookFont,
             lvView, lvParts, lvCores, lvGpu, lvShape, lvTime, lvRows, showDetails, showHelp, pk, ncell > 0 ? cells[0].col : 0);
}
static const char *KN[] = { "none", "style", "size", "detail", "mini", "count", "overhangs", "show", "more", "all", "clear", "go", "stop", "open", "details", "card", "help", "quick", "scale", "oval", "look", "speed", "view", "parts", "colour", "join", "manifold", "objsize", "models" };
static void ui_check(const WCHAR *tag) {
    WCHAR p[MAX_PATH]; FILE *f; RECT c; int i, j, nh, bad = 0, nclick = 0, nnochange = 0; struct { RECT r; int kind, idx; } H[400]; char a[400], b[400];
    swprintf(p, MAX_PATH, L"%ls.%ls.uicheck.txt", stShot, tag); f = _wfopen(p, L"w"); if (!f) return;
    capture(NULL); GetClientRect(hMain, &c);
    nh = nhit; for (i = 0; i < nh; i++) { H[i].r = hits[i].r; H[i].kind = hits[i].kind; H[i].idx = hits[i].idx; }
    fprintf(f, "window %dx%d scale %.2f font %d side_max %d click_areas %d side_need_at_1 %.0f tight %d\n", c.right, c.bottom, SC, FS, sideMax, nh, sideNeed / SC, sideTight);
    for (i = 0; i < nh; i++) {          /* cut off, or overlapping another */
        RECT x;
        if (H[i].kind == H_CARD) continue;
        if (H[i].r.left < 0 || H[i].r.right > c.right + 1 || H[i].r.bottom > c.bottom + 1 || H[i].r.top < -1) {
            if (!(H[i].r.left < S(SIDE_W) && sideMax > 0)) { fprintf(f, "CUT %s %d at %ld,%ld-%ld,%ld\n", KN[H[i].kind], H[i].idx, H[i].r.left, H[i].r.top, H[i].r.right, H[i].r.bottom); bad++; }
        }
        for (j = i + 1; j < nh; j++) if (H[j].kind != H_CARD && IntersectRect(&x, &H[i].r, &H[j].r) && (x.right - x.left) * (x.bottom - x.top) > 4 && !(H[i].kind == H[j].kind && H[i].idx == H[j].idx)) {
            fprintf(f, "OVERLAP %s %d and %s %d\n", KN[H[i].kind], H[i].idx, KN[H[j].kind], H[j].idx); bad++;
        }
    }
    for (i = 0; i < nh; i++) {          /* click each safe one where it is drawn: it must change the state; then put it back */
        int k = H[i].kind, cx = (H[i].r.left + H[i].r.right) / 2, cy = (H[i].r.top + H[i].r.bottom) / 2, keep[40], m = 0, *V[] = { &selStyle, &selSize, &selDetail, &selMini, &selCount, &selSup, &selJoin, &selScale, &selObj, &mfOn[0], &mfOn[1], &mfOn[2], &mfOn[3], &spThr, &spGpu, &lookPix, &lookTex, &lookFont, &lvView, &lvParts, &lvCores, &lvGpu, &lvShape, &lvTime, &lvRows, &showDetails, &showHelp };
        int nv = (int)(sizeof V / sizeof V[0]), pk[MAXC], q;
        if (k == H_SHOW || k == H_GO || k == H_QUICK || k == H_MORE || k == H_STOP || k == H_OPEN || k == H_NONE || k == H_MODELS) continue;
        if (cy < 0 || cy >= c.bottom || cx < 0 || cx >= c.right) continue;
        for (m = 0; m < nv; m++) keep[m] = *V[m]; for (q = 0; q < ncell; q++) pk[q] = cells[q].picked;
        ui_state(a, sizeof a);
        SendMessageW(hMain, WM_MOUSEMOVE, 0, MAKELPARAM(cx, cy)); SendMessageW(hMain, WM_LBUTTONDOWN, MK_LBUTTON, MAKELPARAM(cx, cy)); SendMessageW(hMain, WM_LBUTTONUP, 0, MAKELPARAM(cx, cy));
        ui_state(b, sizeof b); nclick++;
        if (!strcmp(a, b)) {          /* nothing changed: fine only for a choice already chosen, or count at its end */
            int chosen = (k == H_STYLE && H[i].idx == selStyle) || (k == H_SIZE && H[i].idx == selSize) || (k == H_OVAL && H[i].idx + NROUND == selSize) || (k == H_DETAIL && H[i].idx == selDetail) || (k == H_MINI && H[i].idx == selMini) ||
                         (k == H_SCALE && H[i].idx == selScale) || (k == H_OBJ && H[i].idx == selObj) || (k == H_VIEW && H[i].idx == lvView) || (k == H_COUNT && (H[i].idx == 0 || (H[i].idx < 0 && selCount == 1) || (H[i].idx > 0 && selCount == 20)));
            if (!chosen) { fprintf(f, "NO CHANGE %s %d (clicked at %d,%d)\n", KN[k], H[i].idx, cx, cy); nnochange++; }
        }
        for (m = 0; m < nv; m++) *V[m] = keep[m]; for (q = 0; q < ncell; q++) cells[q].picked = pk[q];
        speed_env(); capture(NULL);
    }
    fprintf(f, "clicked %d, no change %d, layout problems %d\n", nclick, nnochange, bad);
    fclose(f);
}
