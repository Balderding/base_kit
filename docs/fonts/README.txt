The pixel font in bbb_kit.exe

bbb_kit.exe draws its labels, chips, buttons and title in Silkscreen (regular and bold), a free pixel font by
The Silkscreen Project Authors, licensed under the SIL Open Font License 1.1 (OFL.txt beside this file).
Source: the Google Fonts repository, github.com/google/fonts, folder ofl/silkscreen.

The font is built into the exe as two small bitmaps (gui\pixelfont.bin, gui\pixelfont_bold.bin, made by
tools\bake_font.py at the font's own pixel size). Two small changes to the drawing at that size, so numbers read right:
- the full stop sits on the line (in the bold it is a 2 x 2 square), so 28.5 does not read as 28-5;
- the bold 4 gets a gap between its strokes, so 40 does not read as 90.
The font declares no Reserved Font Name, so the OFL allows these changes under the same name.
Long text in the window (the help page, Details, the line under the title) stays Segoe UI, which comes with Windows.
