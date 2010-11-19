#include "ubytegui/gui.h"

int gui_maprgb(int r, int g, int b);

int gui_pollevent(guievent_t *ev);

unsigned int darken(int color);

unsigned int osd_darken(int color);

void gui_drawpixmap(pixmap_t *pix, int x, int y, int color, int invert);

void osd_drawpixmap(pixmap_t *pix, int x, int y, int color);
void gui_begin();
int gui_update();
void gui_end();
void osd_cls(int x, int y, int w, int h);

void gui_cls();

void gui_drawrect(int x, int y, int w, int h, int color, int rounded);

void osd_drawrect(int x, int y, int w, int h, int color, int rounded);

void osd_drawtext(font_t *font, const char *text, int x, int y, int color);

void gui_sleep(int s);
