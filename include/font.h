/* Spleen VGA font integration (generated at build) */
#ifndef _AURORA_FONT_H_
#define _AURORA_FONT_H_
#include "../aurora.h"

#define AURORA_FONT_GLYPHS 256
#define AURORA_FONT_HEIGHT 16
#define AURORA_FONT_WIDTH   8

extern const unsigned char g_Spleen8x16[AURORA_FONT_GLYPHS][AURORA_FONT_HEIGHT];

static inline const unsigned char* FontGetGlyph(UINT8 code){
    return g_Spleen8x16[code];
}

static inline unsigned char FontGetGlyphRow(UINT8 code, UINT8 row){
    return g_Spleen8x16[code][row];
}

BOOL FontInitialize(void); /* currently stub */

#endif