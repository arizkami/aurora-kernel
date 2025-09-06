/*
 * Aurora Kernel - AMD64 Memory Management
 * Copyright (c) 2024 Aurora Project
 */

#include "../../include/mem.h"
#include "../../include/kern.h"

/* AMD64 Page Table Entry Flags */
#define AMD64_PTE_PRESENT     0x001
#define AMD64_PTE_WRITE       0x002
#define AMD64_PTE_USER        0x004
#define AMD64_PTE_WRITETHROUGH 0x008
#define AMD64_PTE_CACHEDISABLE 0x010
#define AMD64_PTE_ACCESSED    0x020
#define AMD64_PTE_DIRTY       0x040
#define AMD64_PTE_LARGE       0x080
#define AMD64_PTE_GLOBAL      0x100
#define AMD64_PTE_NX          0x8000000000000000ULL

/* AMD64 Page Table Structure */
typedef struct _AMD64_PAGE_TABLE_ENTRY {
    union {
        struct {
            UINT64 Present : 1;
            UINT64 Write : 1;
            UINT64 User : 1;
            UINT64 WriteThrough : 1;
            UINT64 CacheDisable : 1;
            UINT64 Accessed : 1;
            UINT64 Dirty : 1;
            UINT64 Large : 1;
            UINT64 Global : 1;
            UINT64 Available : 3;
            UINT64 PhysicalAddress : 40;
            UINT64 Available2 : 11;
            UINT64 NoExecute : 1;
        };
        UINT64 Value;
    };
} AMD64_PAGE_TABLE_ENTRY, *PAMD64_PAGE_TABLE_ENTRY;

/* Current page directory physical address */
static UINT64 g_CurrentPageDirectory = 0;

/* Kernel page directory (identity mapped for now) */
static AMD64_PAGE_TABLE_ENTRY g_KernelPML4[512] __attribute__((aligned(4096)));
static AMD64_PAGE_TABLE_ENTRY g_KernelPDPT[512] __attribute__((aligned(4096)));
static AMD64_PAGE_TABLE_ENTRY g_KernelPD[512] __attribute__((aligned(4096)));

/* External assembly functions */
extern UINT64 Amd64GetCR3(void);
extern VOID Amd64SetCR3(UINT64 Value);
extern VOID Amd64FlushTlb(void);

/*
 * Initialize AMD64-specific memory management
 */
NTSTATUS Amd64InitializeMemoryManagement(void)
{
    /* Clear page tables */
    RtlZeroMemory(g_KernelPML4, sizeof(g_KernelPML4));
    RtlZeroMemory(g_KernelPDPT, sizeof(g_KernelPDPT));
    RtlZeroMemory(g_KernelPD, sizeof(g_KernelPD));
    
    /* Set up identity mapping for first 2GB */
    /* PML4[0] -> PDPT */
    g_KernelPML4[0].Present = 1;
    g_KernelPML4[0].Write = 1;
    g_KernelPML4[0].PhysicalAddress = (UINT64)g_KernelPDPT >> 12;
    
    /* PDPT[0] -> PD */
    g_KernelPDPT[0].Present = 1;
    g_KernelPDPT[0].Write = 1;
    g_KernelPDPT[0].PhysicalAddress = (UINT64)g_KernelPD >> 12;
    
    /* Identity map first 1GB using 2MB pages */
    for (int i = 0; i < 512; i++) {
        g_KernelPD[i].Present = 1;
        g_KernelPD[i].Write = 1;
        g_KernelPD[i].Large = 1; /* 2MB pages */
        g_KernelPD[i].PhysicalAddress = (i * 0x200000) >> 12; /* 2MB * i */
    }
    
    /* Set up kernel space mapping (higher half) */
    /* Map kernel space to same physical addresses */
    g_KernelPML4[256].Present = 1; /* 0xFFFF800000000000 */
    g_KernelPML4[256].Write = 1;
    g_KernelPML4[256].PhysicalAddress = (UINT64)g_KernelPDPT >> 12;
    
    /* Store current page directory */
    g_CurrentPageDirectory = (UINT64)g_KernelPML4;
    
    /* Load new page directory */
    Amd64SetCR3(g_CurrentPageDirectory);
    
    return STATUS_SUCCESS;
}

/*
 * Switch address space (change CR3)
 */
VOID Amd64SwitchAddressSpace(IN UINT64 PageDirectoryPhysical)
{
    if (PageDirectoryPhysical != g_CurrentPageDirectory) {
        g_CurrentPageDirectory = PageDirectoryPhysical;
        Amd64SetCR3(PageDirectoryPhysical);
        Amd64FlushTlb();
    }
}

/*
 * Map virtual address to physical address
 */
NTSTATUS Amd64MapVirtualToPhysical(IN UINT64 VirtualAddress, IN UINT64 PhysicalAddress, IN UINT32 Flags)
{
    /* Extract page table indices */
    UINT64 pml4Index = (VirtualAddress >> 39) & 0x1FF;
    UINT64 pdptIndex = (VirtualAddress >> 30) & 0x1FF;
    UINT64 pdIndex = (VirtualAddress >> 21) & 0x1FF;
    UINT64 ptIndex = (VirtualAddress >> 12) & 0x1FF;
    
    /* For now, only support 2MB pages in kernel space */
    if (pml4Index == 0 || pml4Index == 256) {
        if (pdIndex < 512) {
            g_KernelPD[pdIndex].Present = (Flags & MEM_PROTECT_READ) ? 1 : 0;
            g_KernelPD[pdIndex].Write = (Flags & MEM_PROTECT_WRITE) ? 1 : 0;
            g_KernelPD[pdIndex].User = (Flags & MEM_PROTECT_USER) ? 1 : 0;
            g_KernelPD[pdIndex].Large = 1;
            g_KernelPD[pdIndex].PhysicalAddress = PhysicalAddress >> 12;
            g_KernelPD[pdIndex].NoExecute = (Flags & MEM_PROTECT_EXECUTE) ? 0 : 1;
            
            /* Flush TLB for this page */
            Amd64FlushTlb();
            return STATUS_SUCCESS;
        }
    }
    
    return STATUS_NOT_IMPLEMENTED;
}

/*
 * Unmap virtual address
 */
NTSTATUS Amd64UnmapVirtual(IN UINT64 VirtualAddress)
{
    /* Extract page table indices */
    UINT64 pml4Index = (VirtualAddress >> 39) & 0x1FF;
    UINT64 pdIndex = (VirtualAddress >> 21) & 0x1FF;
    
    /* For now, only support 2MB pages in kernel space */
    if (pml4Index == 0 || pml4Index == 256) {
        if (pdIndex < 512) {
            g_KernelPD[pdIndex].Value = 0; /* Clear entire entry */
            
            /* Flush TLB for this page */
            Amd64FlushTlb();
            return STATUS_SUCCESS;
        }
    }
    
    return STATUS_NOT_IMPLEMENTED;
}

/*
 * Get physical address from virtual address
 */
UINT64 Amd64GetPhysicalAddress(IN UINT64 VirtualAddress)
{
    /* Extract page table indices */
    UINT64 pml4Index = (VirtualAddress >> 39) & 0x1FF;
    UINT64 pdIndex = (VirtualAddress >> 21) & 0x1FF;
    UINT64 offset = VirtualAddress & 0x1FFFFF; /* 2MB page offset */
    
    /* For now, only support 2MB pages in kernel space */
    if (pml4Index == 0 || pml4Index == 256) {
        if (pdIndex < 512 && g_KernelPD[pdIndex].Present) {
            return (g_KernelPD[pdIndex].PhysicalAddress << 12) + offset;
        }
    }
    
    return 0; /* Invalid address */
}

/*
 * Check if virtual address is mapped
 */
BOOL Amd64IsVirtualAddressMapped(IN UINT64 VirtualAddress)
{
    /* Extract page table indices */
    UINT64 pml4Index = (VirtualAddress >> 39) & 0x1FF;
    UINT64 pdIndex = (VirtualAddress >> 21) & 0x1FF;
    
    /* For now, only support 2MB pages in kernel space */
    if (pml4Index == 0 || pml4Index == 256) {
        if (pdIndex < 512) {
            return g_KernelPD[pdIndex].Present ? TRUE : FALSE;
        }
    }
    
    return FALSE;
}

/*
 * Get current page directory
 */
UINT64 Amd64GetCurrentPageDirectory(void)
{
    return Amd64GetCR3();
}
