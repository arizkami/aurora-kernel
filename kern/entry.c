// Kernel Entry Point
#include "../aurora.h"
#include "../include/io.h"
#include "../include/fb.h"

void KiSystemStartup(void) {
    IoInitialize();
    if(FbInitialize() == STATUS_SUCCESS){
        FbWriteString(0,0,"Aurora Framebuffer Online\n", FB_RGB(255,255,255), FB_RGB(0,0,0));
    }
    while(1) { __asm__("hlt"); }
}
