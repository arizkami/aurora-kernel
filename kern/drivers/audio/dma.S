.intel_syntax noprefix
.global aur_audio_dma_barrier
aur_audio_dma_barrier:
    mfence
    ret
