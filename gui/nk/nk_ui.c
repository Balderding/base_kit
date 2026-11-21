/* nk_ui.c: Nuklear (public domain / MIT, gui\nk\nuklear.h) and its GDI+ back end (gui\nk\nuklear_gdip.h), used by
   bbb_kit.c through nk_ui.h. The back end's own window buffer is not used: nkui_render draws into the GpGraphics the
   window's paint() gives it (its back buffer), and the window blits that as it always did. */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <string.h>
#include <limits.h>
#include <math.h>
#include "nk_ui.h"          /* Nuklear's declarations, with the same NK_INCLUDE_ options as bbb_kit.c */
#define NK_IMPLEMENTATION
#define NK_GDIP_IMPLEMENTATION
#include "nuklear.h"
#include "nuklear_gdip.h"
GpStatus WINGDIPAPI GdipResetClip(GpGraphics *graphics);
GpStatus WINGDIPAPI GdipCreateFromHDC(HDC hdc, GpGraphics **graphics);          /* not among the back end's own declarations */

static int in_input, ready;
static GdipFont *F[4];

static void set_format(void) {
    GdipCreatePen1(0, 1.0f, UnitPixel, &gdip.pen);
    GdipCreateSolidFill(0, &gdip.brush);
    GdipStringFormatGetGenericTypographic(&gdip.format);
    GdipSetStringFormatFlags(gdip.format, StringFormatFlagsNoFitBlackBox | StringFormatFlagsMeasureTrailingSpaces | StringFormatFlagsNoWrap | StringFormatFlagsNoClip);
}
void nkui_fonts(const char *name, int ui_px, int small_px, int bold_px, int big_px) {
    int px[4] = { ui_px, small_px, bold_px, big_px }, i;
    for (i = 0; i < 4; i++) { if (F[i]) nk_gdipfont_del(F[i]); F[i] = nk_gdipfont_create(name, px[i]); }
    if (F[0]) { nk_gdip_set_font(F[0]); }
    for (i = 1; i < 4; i++) if (F[i]) { struct nk_user_font *u = &F[i]->nk; u->userdata = nk_handle_ptr(F[i]); GdipGetFontSize(F[i]->handle, &u->height); u->width = nk_gdipfont_get_text_width; }
}
void nkui_font(const char *font_name, int font_px) { nkui_fonts(font_name, font_px, font_px - 1, font_px, font_px + 5); }
const struct nk_user_font *nkui_font_of(int which) { return which >= 0 && which < 4 && F[which] ? &F[which]->nk : (F[0] ? &F[0]->nk : NULL); }
struct nk_context *nkui_init(const char *font_name, int font_px) {
    int i;
    if (ready) return &gdip.ctx;
    set_format();
    {   /* a small surface of its own for measuring text while the widgets are laid out (drawing goes into the window's buffer) */
        HDC mdc = CreateCompatibleDC(NULL); if (mdc) GdipCreateFromHDC(mdc, &gdip.memory);
    }
    for (i = 0; i < (int)(sizeof(gdip.fontCollection) / sizeof(gdip.fontCollection[0])); i++) gdip.fontCollection[i] = NULL;
    gdip.curFontCollection = 0;
    nk_init_default(&gdip.ctx, NULL);
    gdip.ctx.clip.copy = nk_gdip_clipboard_copy; gdip.ctx.clip.paste = nk_gdip_clipboard_paste;
    nkui_font(font_name, font_px);
    ready = 1;
    return &gdip.ctx;
}
int nkui_event(HWND h, UINT msg, WPARAM w, LPARAM l) {
    if (!ready || msg == WM_SIZE || msg == WM_PAINT) return 0;          /* the back end's own buffer is not used */
    if (!in_input) { nk_input_begin(&gdip.ctx); in_input = 1; }
    return nk_gdip_handle_event(h, msg, w, l);
}
void nkui_frame(void) {
    if (!ready) return;
    if (!in_input) nk_input_begin(&gdip.ctx);
    nk_input_end(&gdip.ctx); in_input = 0;
}
void nkui_render(void *g) {
    GpGraphics *keep;
    if (!ready || !g) return;
    keep = gdip.memory; gdip.memory = (GpGraphics *)g;
    nk_gdip_prerender_gui(NK_ANTI_ALIASING_ON);
    GdipResetClip(gdip.memory);
    gdip.memory = keep;
    nk_clear(&gdip.ctx);
}
struct nk_image nkui_image(void *gp_image) { return nk_image_ptr(gp_image); }
int nkui_wants_mouse(void) { return ready && nk_window_is_any_hovered(&gdip.ctx); }
