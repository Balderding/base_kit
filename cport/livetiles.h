/* livetiles.h: the window's live view, from bbb_kit.exe --core-build. With KIT_LIVE_FILE set (the window is watching),
   the height field's tiles and which are done go to <KIT_LIVE_FILE>.tiles, at most every 0.1 s, from a thread of its
   own; the tile workers only set a byte. Nothing at all happens without KIT_LIVE_FILE. */
#ifndef LIVETILES_H
#define LIVETILES_H
void lt_begin(int ntiles, const double (*boxes)[4], int steps_total, int threads);          /* boxes: x0,y0,x1,y1 mm, one per tile */
void lt_tile_done(int k);
void lt_steps(int done);
void lt_stage(const char *stage);                                                            /* "field", "mesh" */
void lt_end(void);
#endif
