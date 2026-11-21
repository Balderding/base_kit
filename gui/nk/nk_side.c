/* nk_side.c: the side panel in Nuklear (gui\nk\PLAN.md step 2). Included by bbb_kit.c when built with BBB_NK, so it
   sees the window's own state (selStyle, selSize ...) and calls the same act(kind, idx) the GDI+ panel calls: every
   choice does exactly what it did. Themes: nk_theme(t) sets Nuklear's colour table and shapes (no red text in any). */
static struct nk_context *NK;
static int nkActed, nkTheme;          /* nkTheme: 0 Navy (the window's own colours), 1 Poster (the badge: cream, red, gold), 2 Slate (light) */
static const char *NK_THEMES[3] = { "Navy", "Poster", "Slate" };
static struct nk_color nkc(ARGB c) { return nk_rgba((int)((c >> 16) & 255), (int)((c >> 8) & 255), (int)(c & 255), (int)((c >> 24) & 255)); }
static void nk_act(int kind, int idx) { act(kind, idx); nkActed = 1; }
typedef struct { ARGB bg, panel, card, cardh, line, text, dim, acc, acch, onacc, gold; } NkPal;
static const NkPal NKP[3] = {
    { C_BG, C_SIDE, C_CARD, C_CARDH, C_LINE, C_TEXT, C_DIM, C_ACC, C_ACCH, C_ONACC, C_GOLD },
    { 0xFF231F1Cu, 0xFF2C2622u, 0xFF3A322Bu, 0xFF4A4036u, 0xFF5C4E40u, 0xFFF4E6C8u, 0xFFC9B48Eu, 0xFFFACE3Au, 0xFFFFDE70u, 0xFF231F1Cu, 0xFFD6402Eu },
    { 0xFFE9EDF2u, 0xFFDDE3EAu, 0xFFFFFFFFu, 0xFFF1F5FAu, 0xFFB8C2CEu, 0xFF1B2330u, 0xFF56657Au, 0xFF1E5AA8u, 0xFF2D6FC4u, 0xFFFFFFFFu, 0xFF9A7420u },
};
static const NkPal *nkp(void) { return &NKP[nkTheme >= 0 && nkTheme < 3 ? nkTheme : 0]; }
static void nk_theme(int t) {
    struct nk_color T[NK_COLOR_COUNT]; const NkPal *p; struct nk_style *st;
    nkTheme = t; p = nkp(); if (!NK) return;
    T[NK_COLOR_TEXT] = nkc(p->text); T[NK_COLOR_WINDOW] = nkc(p->panel); T[NK_COLOR_HEADER] = nkc(p->card); T[NK_COLOR_BORDER] = nkc(p->line);
    T[NK_COLOR_BUTTON] = nkc(p->card); T[NK_COLOR_BUTTON_HOVER] = nkc(p->cardh); T[NK_COLOR_BUTTON_ACTIVE] = nkc(p->acc);
    T[NK_COLOR_TOGGLE] = nkc(p->card); T[NK_COLOR_TOGGLE_HOVER] = nkc(p->cardh); T[NK_COLOR_TOGGLE_CURSOR] = nkc(p->acc);
    T[NK_COLOR_SELECT] = nkc(p->card); T[NK_COLOR_SELECT_ACTIVE] = nkc(p->acc);
    T[NK_COLOR_SLIDER] = nkc(p->line); T[NK_COLOR_SLIDER_CURSOR] = nkc(p->acc); T[NK_COLOR_SLIDER_CURSOR_HOVER] = nkc(p->acch); T[NK_COLOR_SLIDER_CURSOR_ACTIVE] = nkc(p->acch);
    T[NK_COLOR_PROPERTY] = nkc(p->card); T[NK_COLOR_EDIT] = nkc(p->bg); T[NK_COLOR_EDIT_CURSOR] = nkc(p->text);
    T[NK_COLOR_COMBO] = nkc(p->card); T[NK_COLOR_CHART] = nkc(p->card); T[NK_COLOR_CHART_COLOR] = nkc(p->acc); T[NK_COLOR_CHART_COLOR_HIGHLIGHT] = nkc(p->gold);
    T[NK_COLOR_SCROLLBAR] = nkc(p->panel); T[NK_COLOR_SCROLLBAR_CURSOR] = nkc(p->line); T[NK_COLOR_SCROLLBAR_CURSOR_HOVER] = nkc(p->cardh); T[NK_COLOR_SCROLLBAR_CURSOR_ACTIVE] = nkc(p->acc);
    T[NK_COLOR_TAB_HEADER] = nkc(p->card); T[NK_COLOR_KNOB] = nkc(p->card); T[NK_COLOR_KNOB_CURSOR] = nkc(p->acc); T[NK_COLOR_KNOB_CURSOR_HOVER] = nkc(p->acch); T[NK_COLOR_KNOB_CURSOR_ACTIVE] = nkc(p->acch);
    nk_style_from_table(NK, T);
    st = &NK->style;
    st->window.padding = nk_vec2((float)S(14), (float)S(8)); st->window.spacing = nk_vec2((float)S(4), (float)S(4)); st->window.border = 0;
    st->window.scrollbar_size = nk_vec2((float)S(8), (float)S(8));
    st->button.rounding = (float)S(5); st->button.border = 1.0f; st->button.border_color = nkc(p->line); st->button.text_active = nkc(p->onacc);
    st->selectable.rounding = (float)S(5); st->selectable.text_normal_active = nkc(p->onacc); st->selectable.text_hover_active = nkc(p->onacc);
    st->selectable.normal_active = nk_style_item_color(nkc(p->acc)); st->selectable.hover_active = nk_style_item_color(nkc(p->acch)); st->selectable.pressed_active = nk_style_item_color(nkc(p->acch));
    st->selectable.normal = nk_style_item_color(nkc(p->card)); st->selectable.hover = nk_style_item_color(nkc(p->cardh)); st->selectable.pressed = nk_style_item_color(nkc(p->cardh));
    st->checkbox.border = 1.0f; st->checkbox.border_color = nkc(p->line);
    st->slider.bar_height = (float)S(4); st->slider.cursor_size = nk_vec2((float)S(16), (float)S(16));
}
static void nk_section(const char *label) {
    nk_layout_row_dynamic(NK, (float)S(20), 1);
    nk_label_colored(NK, label, NK_TEXT_LEFT, nkc(nkp()->dim));
}
static void nk_chips(const char *label, const char **items, int n, int cols, int sel, int kind, int first) {
    int i;
    if (label) nk_section(label);
    nk_layout_row_dynamic(NK, (float)S(24), cols);
    for (i = 0; i < n; i++) { if (nk_select_label(NK, items[i], NK_TEXT_CENTERED, i == sel) && i != sel) nk_act(kind, i + first); }
}
static void nk_dash_line(struct nk_command_buffer *cv, float x0, float y0, float x1, float y1, float th, struct nk_color c) {
    float L = sqrtf((x1 - x0) * (x1 - x0) + (y1 - y0) * (y1 - y0)), d = (float)S(5), t;
    for (t = 0; t < L; t += 2 * d) { float a = t / L, b = (t + d < L ? t + d : L) / L; nk_stroke_line(cv, x0 + (x1 - x0) * a, y0 + (y1 - y0) * a, x0 + (x1 - x0) * b, y0 + (y1 - y0) * b, th, c); }
}
static void nk_preview(void) {          /* the picture under the slider, as obj_slider draws it: base, model's space, things, man */
    struct nk_rect r; struct nk_command_buffer *cv; const NkPal *p = nkp(); double A = atof(SIZES[selSize]), B = A, man = GAMEMM[selScale], mpm = man / 1.8 * OBJM[selObj], pk, bx, by, bw, bh, cx, cy, rad, hx, hy; int box; const char *mini = selMini ? MINIS[selMini] : "default"; char b[64];
    { const char *xx = strchr(SIZES[selSize], 'x'); if (xx) B = atof(xx + 1); }
    nk_layout_row_dynamic(NK, (float)S(128), 1);
    if (!nk_widget(&r, NK)) return;
    cv = nk_window_get_canvas(NK);
    nk_fill_rect(cv, r, (float)S(6), nkc(p->bg)); nk_stroke_rect(cv, r, (float)S(6), 1.0f, nkc(p->line));
    {   double g = 2.0, cm = 0.8 * mpm, bm = 0.6 * mpm, rm = 0.5 * mpm * 1.3, mwm = man * 0.3, tot = A + g * 4 + cm + bm + rm + mwm, gl = r.y + r.h - S(10), px; struct nk_color dc = nk_rgb(0x6F, 0xE3, 0xD4);
        pk = fmin((r.w - S(16)) / tot, (r.h - S(30)) / fmax(B, man));          /* one row on one ground line, as the GDI+ panel draws it */
        bw = A * pk; bh = B * pk; bx = r.x + S(8); by = gl - bh; cx = bx + bw / 2; cy = by + bh / 2;
        nk_fill_circle(cv, nk_rect((float)bx, (float)by, (float)bw, (float)bh), nk_rgb(0x5A, 0x52, 0x46));
        nk_fill_circle(cv, nk_rect((float)(bx + S(2)), (float)(by + S(2)), (float)(bw - S(4)), (float)(bh - S(4))), nk_rgb(0x6E, 0x66, 0x56));
        box = ms_space(A, fmin(A, B) / 2 * 0.97, mini, &rad, &hx, &hy);
        if (A >= 39.9 && !obj_classic()) {
            if (box) { double ex = fmin(hx, bw / 2 / pk * 0.97); float x0 = (float)(cx - ex * pk), x1 = (float)(cx + ex * pk), y0 = (float)(cy - hy * pk), y1 = (float)(cy + hy * pk), th = (float)S(1.5);
                       nk_dash_line(cv, x0, y0, x1, y0, th, dc); nk_dash_line(cv, x1, y0, x1, y1, th, dc); nk_dash_line(cv, x1, y1, x0, y1, th, dc); nk_dash_line(cv, x0, y1, x0, y0, th, dc); }
            else { int k; float R = (float)(rad * pk); for (k = 0; k < 48; k += 2) { float a0 = (float)(k * 3.14159265 / 24), a1 = (float)((k + 1) * 3.14159265 / 24); nk_stroke_line(cv, (float)cx + R * cosf(a0), (float)cy + R * sinf(a0), (float)cx + R * cosf(a1), (float)cy + R * sinf(a1), (float)S(1.5), dc); } }
            nk_draw_text(cv, nk_rect(r.x + S(8), r.y + S(2), r.w - S(16), (float)S(14)), "dashed: the model's space", 25, nkui_font_of(1), nkc(p->bg), dc);
        }
        px = bx + bw + g * pk;
        nk_fill_rect(cv, nk_rect((float)px, (float)(gl - cm * pk), (float)(cm * pk), (float)(cm * pk)), 0, nk_rgb(0x9C, 0x6B, 0x3B)); px += (cm + g) * pk;
        nk_fill_circle(cv, nk_rect((float)px, (float)(gl - bm * pk), (float)(bm * pk), (float)(bm * pk)), nk_rgb(0x5D, 0x7A, 0x8C)); px += (bm + g) * pk;
        nk_fill_circle(cv, nk_rect((float)px, (float)(gl - rm * pk / 1.3), (float)(rm * pk), (float)(rm * pk / 1.3)), nk_rgb(0x8A, 0x7F, 0x74)); px += (rm + g) * pk;
        {   double mh = man * pk, mw = fmax(S(3), mwm * pk), mx = px + mw / 2;
            nk_fill_rect(cv, nk_rect((float)(mx - mw / 2), (float)(gl - mh * 0.82), (float)mw, (float)(mh * 0.82)), (float)(mw / 3), nk_rgb(0xD8, 0xD4, 0xCA));
            nk_fill_circle(cv, nk_rect((float)(mx - mw * 0.45), (float)(gl - mh), (float)(mw * 0.9), (float)(mw * 0.9)), nk_rgb(0xD8, 0xD4, 0xCA));
            snprintf(b, sizeof b, "%.0f mm", man); nk_draw_text(cv, nk_rect((float)(mx - S(22)), (float)(gl - mh - S(15)), (float)S(44), (float)S(14)), b, (int)strlen(b), nkui_font_of(1), nkc(p->bg), nkc(p->dim)); }
        nk_stroke_line(cv, r.x + S(4), (float)gl, r.x + r.w - S(4), (float)gl, 1.0f, nkc(p->line));
    }
}
static void nk_side(int H) {
    const NkPal *p = nkp(); char b[96]; int k;
    if (!NK) return;
    if (nk_begin(NK, "side", nk_rect(0, 0, (float)S(SIDE_W), (float)H), 0)) {
        nk_style_set_font(NK, nkui_font_of(3));
        nk_layout_row_dynamic(NK, (float)S(28), 1); nk_label(NK, "Balderding's Ballin Base Kit", NK_TEXT_LEFT);
        nk_style_set_font(NK, nkui_font_of(0));
        nk_layout_row_dynamic(NK, (float)S(16), 1); nk_label_colored(NK, "Save time and money.", NK_TEXT_LEFT, nkc(p->acc));
        nk_chips("STYLE", STYLES, NSTYLES, 3, selStyle, H_STYLE, 0);
        nk_chips("BASE SIZE, MM  (ROUND, THEN OVAL)", SIZES, NROUND, 8, selSize, H_SIZE, 0);
        nk_chips(NULL, SIZES + NROUND, NSIZES - NROUND, 4, selSize - NROUND, H_OVAL, 0);
        nk_chips("DETAIL  (1 SIMPLE ... 4 MOST)", DETAILL, 5, 5, selDetail, H_DETAIL, 0);
        nk_chips("THE MINIATURE IT IS FOR", MINIS, 13, 3, selMini, H_MINI, 0);
        nk_chips("GAME SCALE  (HOW TALL A MAN IS)", SCALES, 3, 3, selScale, H_SCALE, 0);
        nk_section("SIZE OF THINGS ON TOP");
        { int v = selObj; nk_layout_row_dynamic(NK, (float)S(22), 1); nk_slider_int(NK, 0, &v, NOBJ - 1, 1); if (v != selObj) nk_act(H_OBJ, v); }
        nk_layout_row_dynamic(NK, (float)S(14), 3);
        nk_label_colored(NK, "Map", NK_TEXT_LEFT, nkc(p->dim)); nk_label_colored(NK, "True", NK_TEXT_CENTERED, nkc(p->gold)); nk_label_colored(NK, "Chunky", NK_TEXT_RIGHT, nkc(p->dim));
        {   double man = GAMEMM[selScale], mpm = man / 1.8 * OBJM[selObj];
            if (obj_classic()) snprintf(b, sizeof b, "Map look: things a third of true size, a small scene");
            else snprintf(b, sizeof b, "%s crate %.0f mm, barrel %.0f mm, man %.0f mm", selObj == OBJ_TRUE ? "True:" : selObj < OBJ_TRUE ? "Smaller:" : "Chunkier:", 0.8 * mpm, 0.6 * mpm, man);
            nk_layout_row_dynamic(NK, (float)S(18), 1); nk_label(NK, b, NK_TEXT_LEFT); }
        nk_preview();
        nk_section("HOW MANY BASES  (1 TO 20)");
        nk_layout_row_begin(NK, NK_STATIC, (float)S(30), 3);
        nk_layout_row_push(NK, (float)S(40)); if (nk_button_label(NK, "-")) nk_act(H_COUNT, -1);
        nk_layout_row_push(NK, (float)(S(SIDE_W) - S(28) - S(80) - S(16)));
        nk_style_set_font(NK, nkui_font_of(3)); snprintf(b, sizeof b, "%d", selCount); nk_label(NK, b, NK_TEXT_CENTERED); nk_style_set_font(NK, nkui_font_of(0));
        nk_layout_row_push(NK, (float)S(40)); if (nk_button_label(NK, "+")) nk_act(H_COUNT, 1);
        nk_layout_row_end(NK);
        nk_layout_row_dynamic(NK, (float)S(24), 1);
        { nk_bool v = selSup; if (nk_checkbox_label(NK, "Allow overhangs (needs supports)", &v)) nk_act(H_SUP, 0); }
        if (selSup) { nk_bool v = selJoin; nk_layout_row_dynamic(NK, (float)S(24), 1); if (nk_checkbox_label(NK, "Join into one solid (Manifold)", &v)) nk_act(H_JOIN, 0); }
        nk_section("MANIFOLD"); nk_layout_row_dynamic(NK, (float)S(24), 2);
        for (k = 0; k < MF_N; k++) { nk_bool v = mfOn[k]; if (nk_checkbox_label(NK, MFN[k], &v)) nk_act(H_MF, k); }
        nk_section("SPEED"); nk_layout_row_dynamic(NK, (float)S(24), 2);
        { nk_bool v = spThr; if (nk_checkbox_label(NK, "CPU threads", &v)) nk_act(H_SPEED, 0); }
        { nk_bool v = spGpu; if (nk_checkbox_label(NK, "Video card", &v)) nk_act(H_SPEED, 1); }
        nk_section("THEME"); nk_layout_row_dynamic(NK, (float)S(24), 3);
        for (k = 0; k < 3; k++) if (nk_select_label(NK, NK_THEMES[k], NK_TEXT_CENTERED, k == nkTheme) && k != nkTheme) { nk_theme(k); nkActed = 1; }
        nk_layout_row_dynamic(NK, (float)S(8), 1); nk_spacing(NK, 1);
        nk_layout_row_dynamic(NK, (float)S(40), 1);
        snprintf(b, sizeof b, ncell ? "Show %d new choices" : "Show %d choices", selCount);
        if (busy) nk_widget_disable_begin(NK);
        { struct nk_style_button keep = NK->style.button; if (!npicked()) { NK->style.button.normal = nk_style_item_color(nkc(p->acc)); NK->style.button.hover = nk_style_item_color(nkc(p->acch)); NK->style.button.text_normal = nkc(p->onacc); NK->style.button.text_hover = nkc(p->onacc); }
          if (nk_button_label(NK, b)) nk_act(H_SHOW, 0); NK->style.button = keep; }
        nk_layout_row_dynamic(NK, (float)S(30), 1);
        snprintf(b, sizeof b, selCount == 1 ? "Or build 1 straight away" : "Or build %d straight away", selCount);
        if (nk_button_label(NK, b)) nk_act(H_QUICK, 0);
        if (busy) nk_widget_disable_end(NK);
    }
    nk_end(NK);
}

/* the top row in Nuklear (PLAN step 3a): what is happening (head, sub), Models, the look switches, Help; the same act() calls */
static void nk_top(int mx, int mw) {
    const NkPal *p = nkp(); char hb[400], sb[700]; static const char *LK[3] = { "Pixel", "Textures", "Pixel font" }; int on[3], k; float bw = (float)S(96);
    if (!NK) return;
    WideCharToMultiByte(CP_UTF8, 0, head, -1, hb, sizeof hb, NULL, NULL); WideCharToMultiByte(CP_UTF8, 0, sub, -1, sb, sizeof sb, NULL, NULL);
    on[0] = lookPix; on[1] = lookTex; on[2] = lookFont;
    if (nk_begin(NK, "top", nk_rect((float)mx, 0, (float)mw, (float)(S(HEAD_H) - S(12))), NK_WINDOW_NO_SCROLLBAR)) {
        nk_layout_row_begin(NK, NK_STATIC, (float)S(28), 6);
        nk_layout_row_push(NK, (float)mw - S(30) - 5 * bw - S(24));
        nk_style_set_font(NK, nkui_font_of(3)); nk_label(NK, hb, NK_TEXT_LEFT); nk_style_set_font(NK, nkui_font_of(0));
        nk_layout_row_push(NK, bw); if (nk_button_label(NK, "Models")) nk_act(H_MODELS, 0);
        for (k = 0; k < 3; k++) { nk_layout_row_push(NK, bw); if (nk_select_label(NK, LK[k], NK_TEXT_CENTERED, on[k]) != on[k]) nk_act(H_LOOK, k); }
        nk_layout_row_push(NK, bw); if (nk_button_label(NK, "Help  ?")) nk_act(H_HELP, 0);
        nk_layout_row_end(NK);
        nk_layout_row_dynamic(NK, (float)S(18), 1); nk_style_set_font(NK, nkui_font_of(1)); nk_label_colored(NK, sb, NK_TEXT_LEFT, nkc(p->dim)); nk_style_set_font(NK, nkui_font_of(0));
    }
    nk_end(NK);
}
