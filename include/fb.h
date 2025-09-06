/* Simple linear framebuffer interface (EFI GOP handoff placeholder) */
#ifndef _AURORA_FB_H_
#define _AURORA_FB_H_
#include "../aurora.h"
#include "font.h"
#include "io.h"

typedef struct _AURORA_FB_MODE {
    UINT8* Base;      /* Linear framebuffer base */
    UINT32 Width;
    UINT32 Height;
    UINT32 Pitch;     /* Bytes per scanline */
    UINT32 Bpp;       /* Bits per pixel (expect 32) */
} AURORA_FB_MODE, *PAURORA_FB_MODE;

extern AURORA_FB_MODE g_FramebufferMode; /* Zero if not present */

/* Color helper (assumes X8R8G8B8 little-endian) */
#define FB_RGB(r,g,b) ( ((UINT32)(r) << 16) | ((UINT32)(g) << 8) | (UINT32)(b) )

NTSTATUS FbSetMode(UINT8* Base, UINT32 Width, UINT32 Height, UINT32 Pitch, UINT32 Bpp);
NTSTATUS FbInitialize(void); /* Creates display device if mode already set */
void FbPutPixel(UINT32 x, UINT32 y, UINT32 color);
void FbClear(UINT32 color);
void FbDrawChar(UINT32 x, UINT32 y, CHAR ch, UINT32 fg, UINT32 bg);
UINT32 FbWriteString(UINT32 x, UINT32 y, const char* s, UINT32 fg, UINT32 bg);

#endif /* _AURORA_FB_H_ */
