.intel_syntax noprefix
.globl ProcEnterUserMode
ProcEnterUserMode:
    /* TODO: set up segment selectors, MSRs, and iret to user */
    ret
