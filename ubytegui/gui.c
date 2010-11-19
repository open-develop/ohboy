#include "gui.h"

gui_t gui = {.open = 0};

void gui_setclip(int x, int y, int w, int h){
	gui.clip.x = x;
	gui.clip.y = y;
	gui.clip.w = w;
	gui.clip.h = h;
}

void gui_clip(int x, int y, int w, int h){
	int right, bottom;

	if(x>gui.clip.x){
		gui.clip.w -= x - gui.clip.x;
		gui.clip.x = x;
	}

	if(y>gui.clip.y){
		gui.clip.h -= y - gui.clip.y;
		gui.clip.y = y;
	}

	if((right = x+w) < gui.clip.x + gui.clip.w)
		gui.clip.w = right - gui.clip.x;

	if((bottom = y+h) < gui.clip.y + gui.clip.h)
		gui.clip.h = bottom - gui.clip.y;

}

void gui_drawtext(font_t *font, const char *text, int x, int y, int color, int invert){
	glyph_t *glyph;

	if(!font || !text) return;

	while(*text){
		glyph = font->glyph[(int)(*text)];
		gui_drawpixmap(&(glyph->pixmap), x+glyph->left, y-glyph->top, color,invert);
		x+= glyph->advance;
		text++;
	}
}
