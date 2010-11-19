#ifndef FONT_H_INCLUDED
#define FONT_H_INCLUDED

#include "pixmap.h"

typedef struct glyph_s{
	int top, left;
	int width, height;
	int advance;
	pixmap_t pixmap;
}glyph_t;

typedef struct font_s{
	char name[256];
	int style, size;
	int ascent, descent, height;
	glyph_t *glyph[128];
}font_t;

font_t *font_load(const char* name, int index, int size);
void font_free(font_t *font);
int font_textwidth(font_t *font, const char *text);
void font_drawtext(font_t *font, const char *text, int x, int y, int color, int invert);

#endif // FONT_H_INCLUDED
