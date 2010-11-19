#include "ft2build.h"
#include "freetype.h"

#include "font.h"
#include "pixmap.h"

font_t *font_load(const char* name, int index, int size){
	FT_Library ftlib;
	FT_Face face;
	font_t *font;
	glyph_t *glyph;

	void *src, *dst;
	int i, j, w,h;

	if(FT_Init_FreeType( &ftlib )) return NULL;


	if(FT_New_Face(ftlib,name,index,&face))	return NULL;

	if(index >= face->num_faces) return NULL;

	if(FT_Set_Char_Size(face,0,size*64,72,72)){
		FT_Done_Face(face);
		return NULL;
	}

	font = malloc(sizeof(font_t));

	if(!font){
		FT_Done_Face(face);
		return NULL;
	}
	strncpy(font->name,face->family_name,256);
	font->style = face->style_flags;
	font->size = size;

	font->ascent = face->size->metrics.ascender>>6;
	font->descent = face->size->metrics.descender>>6;
	font->height = face->size->metrics.height>>6;

	for(i=0; i<128; i++){
		index = FT_Get_Char_Index( face, i );

		if(!index && i>0){
			font->glyph[i] = font->glyph[0];
			continue;
		}

		if(FT_Load_Glyph(face, index, FT_LOAD_RENDER)) continue;
		w = face->glyph->bitmap.width;
		h = face->glyph->bitmap.rows;
		glyph = malloc(sizeof(glyph_t)+w*h);
		glyph->top = face->glyph->bitmap_top;
		glyph->left = face->glyph->bitmap_left;
		glyph->advance = face->glyph->advance.x >> 6;
		glyph->pixmap.width = w;
		glyph->pixmap.height = h;
		glyph->pixmap.pelsize = 1;
		glyph->pixmap.pitch = w;
		glyph->pixmap.ptr = (void*)glyph + sizeof(glyph_t);

		src = face->glyph->bitmap.buffer;
		dst = glyph->pixmap.ptr;

		for(j=0; j<h; j++){
			memcpy(dst,src,w);
			src+=face->glyph->bitmap.pitch;
			dst+=w;
		}
		font->glyph[i] = glyph;
	}

	FT_Done_Face(face);
	FT_Done_FreeType(ftlib);
	return font;
};

void font_free(font_t *font){
	int i;
	if(!font) return;
	for(i=0; i<128; i++)
		free(font->glyph[i]);
	free(font);
}

int font_textwidth(font_t *font, const char *text){
	int w = 0;
	if(!font || !text) return 0;
	while(*text){
		w += font->glyph[(int)(*text)]->advance;
		text++;
	}
	return w;
}
