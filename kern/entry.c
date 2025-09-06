// Kernel Entry Point
#include "../aurora.h"
#include "../include/io.h"
#include "../include/fb.h"

void KiSystemStartup(void) {
    IoInitialize();
    /* Initialize example system stub & file system stub (best effort) */
    extern NTSTATUS SysStubInitialize(void); SysStubInitialize();
    extern NTSTATUS StubFsAutoRegister(void); StubFsAutoRegister();
    if(FbInitialize() == STATUS_SUCCESS){
        FbWriteString(0,0,"Aurora Framebuffer Online\n", FB_RGB(255,255,255), FB_RGB(0,0,0));
    }
    extern void StorageDriverInitialize(void); StorageDriverInitialize();
    extern void DisplayDriverInitialize(void); DisplayDriverInitialize();
    extern void AudioDriverInitialize(void); AudioDriverInitialize();
    // extern void HidDriverInitialize(void); HidDriverInitialize();
    while(1) { __asm__("hlt"); }
}
