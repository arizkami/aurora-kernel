.intel_syntax noprefix
.global aur_hid_barrier
aur_hid_barrier:
    lfence
    ret
