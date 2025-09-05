// Aurora Legacy Loader (multiboot-less minimal placeholder)
__attribute__((noreturn)) void _start(void) {
    // In real mode/bootloader this would switch to long mode, load kernel, etc.
    for(;;) { __asm__("hlt"); }
}
