// Kernel Entry Point
void KiSystemStartup(void) {
    // Kernel initialization code goes here
    while(1) { __asm__("hlt"); }
}
