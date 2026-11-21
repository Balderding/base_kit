/* nk_ui.h: the Nuklear face of the window (gui\nk\PLAN.md). nk_ui.c holds Nuklear itself and its GDI+ back end in their
   own translation unit (the back end declares GDI+ types of its own, which would clash with <gdiplus.h> in bbb_kit.c);
   bbb_kit.c sees only Nuklear's declarations and these calls. Nuklear draws into the GDI+ graphics of the window's own
   back buffer, so the window's pictures and self-tests work as before, and old and Nuklear parts share one frame. */
#ifndef NK_UI_H
#define NK_UI_H
#include <windows.h>
#define NK_INCLUDE_FIXED_TYPES
#define NK_INCLUDE_STANDARD_IO
#define NK_INCLUDE_STANDARD_VARARGS
#define NK_INCLUDE_DEFAULT_ALLOCATOR
#include "nuklear.h"
struct nk_context *nkui_init(const char *font_name, int font_px);          /* after GdiplusStartup; NULL on failure */
void nkui_font(const char *font_name, int font_px);                        /* a new size (the monitor's scaling changed) */
const struct nk_user_font *nkui_font_of(int which);                        /* 0 the UI font, 1 small, 2 bold, 3 big */
void nkui_fonts(const char *name, int ui_px, int small_px, int bold_px, int big_px);
int nkui_event(HWND h, UINT msg, WPARAM w, LPARAM l);                     /* feed one window message in; 1 if Nuklear used it */
void nkui_frame(void);                                                     /* close this frame's input, before the widgets */
void nkui_render(void *gp_graphics);                                       /* draw the widgets built since the last call into that GpGraphics, then clear */
struct nk_image nkui_image(void *gp_image);                                /* a GDI+ image (GpImage *) for nk_image / nk_draw_image */
int nkui_wants_mouse(void);                                                /* the mouse is over a Nuklear window */
#endif
