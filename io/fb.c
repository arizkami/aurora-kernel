/* Framebuffer driver (linear X8R8G8B8) */
#include "../aurora.h"
#include "../include/fb.h"

AURORA_FB_MODE g_FramebufferMode; /* zero-initialized */
static PAIO_DEVICE_OBJECT g_FbDevice = NULL;

NTSTATUS FbSetMode(UINT8* Base, UINT32 Width, UINT32 Height, UINT32 Pitch, UINT32 Bpp){
    g_FramebufferMode.Base = Base;
    g_FramebufferMode.Width = Width;
    g_FramebufferMode.Height = Height;
    g_FramebufferMode.Pitch = Pitch;
    g_FramebufferMode.Bpp = Bpp;
    return STATUS_SUCCESS;
}

static NTSTATUS FbEnsureDevice(void){
    static int registered = 0;
    if(!registered){
        IoRegisterDisplayDriver();
        registered = 1;
    }
    if(!g_FbDevice){
        AIO_DRIVER_OBJECT dummyDriver; /* Device needs a driver reference; use display driver head later */
        IoDriverInitialize(&dummyDriver, "display");
        IoCreateDevice(&dummyDriver, "fb0", (IO_DEVICE_CLASS_DISPLAY << 16) | IO_DISPLAY_TYPE_FB, &g_FbDevice);
    }
    return STATUS_SUCCESS;
}

NTSTATUS FbInitialize(void){
    if(!g_FramebufferMode.Base) return STATUS_NOT_INITIALIZED; /* Mode must be set by loader */
    FontInitialize();
    FbEnsureDevice();
    FbClear(FB_RGB(0,0,0));
    return STATUS_SUCCESS;
}

void FbPutPixel(UINT32 x, UINT32 y, UINT32 color){
    if(!g_FramebufferMode.Base) return;
    if(x >= g_FramebufferMode.Width || y >= g_FramebufferMode.Height) return;
    if(g_FramebufferMode.Bpp != 32) return; /* only support 32bpp for now */
    UINT32* row = (UINT32*)(g_FramebufferMode.Base + y * g_FramebufferMode.Pitch);
    row[x] = color;
}

void FbClear(UINT32 color){
    if(!g_FramebufferMode.Base) return;
    if(g_FramebufferMode.Bpp != 32) return;
    for(UINT32 y=0; y<g_FramebufferMode.Height; ++y){
        UINT32* row = (UINT32*)(g_FramebufferMode.Base + y * g_FramebufferMode.Pitch);
        for(UINT32 x=0; x<g_FramebufferMode.Width; ++x){ row[x] = color; }
    }
}

void FbDrawChar(UINT32 x, UINT32 y, CHAR ch, UINT32 fg, UINT32 bg){
    if(!g_FramebufferMode.Base) return;
    const unsigned char* glyph = FontGetGlyph((UINT8)ch);
    for(UINT32 row=0; row<AURORA_FONT_HEIGHT; ++row){
        unsigned char bits = glyph[row];
        for(UINT32 col=0; col<AURORA_FONT_WIDTH; ++col){
            UINT32 color = (bits & (0x80 >> col)) ? fg : bg;
            FbPutPixel(x+col, y+row, color);
        }
    }
}

UINT32 FbWriteString(UINT32 x, UINT32 y, const char* s, UINT32 fg, UINT32 bg){
    while(*s){
        if(*s == '\n'){ y += AURORA_FONT_HEIGHT; x = 0; ++s; continue; }
        FbDrawChar(x,y,*s,fg,bg);
        x += AURORA_FONT_WIDTH;
        if(x + AURORA_FONT_WIDTH >= g_FramebufferMode.Width){ x = 0; y += AURORA_FONT_HEIGHT; }
        ++s;
    }
    return x;
}
