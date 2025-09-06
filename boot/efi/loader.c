// Aurora EFI Loader (x86_64) using gnu-efi
// Enhanced bootloader with full kernel loading capabilities:
//  - Load kernel from EFI system partition
//  - Apply PE relocations if needed
//  - Retrieve memory map and exit boot services
//  - Transfer control to kernel with boot information

#include <efi.h>
#include <efilib.h>
#include "../../include/boot_protocol.h"

#define KERNEL_FILENAME L"aurkern.exe"
#define MAX_MEMORY_MAP_SIZE 4096
#define STACK_SIZE 0x10000

// PE relocation types
#define IMAGE_REL_BASED_ABSOLUTE    0
#define IMAGE_REL_BASED_HIGH        1
#define IMAGE_REL_BASED_LOW         2
#define IMAGE_REL_BASED_HIGHLOW     3
#define IMAGE_REL_BASED_HIGHADJ     4
#define IMAGE_REL_BASED_DIR64       10

typedef struct {
    UINT32 VirtualAddress;
    UINT32 SizeOfBlock;
} IMAGE_BASE_RELOCATION;

static aurora_boot_info_t g_BootInfo;
static UINT8 g_MemoryMapBuffer[MAX_MEMORY_MAP_SIZE];

// Apply PE relocations if kernel is not loaded at preferred base
static EFI_STATUS ApplyRelocations(VOID* ImageBase, UINT64 PreferredBase, UINT64 ActualBase)
{
    if (PreferredBase == ActualBase) {
        Print(L"[EFI] No relocations needed (loaded at preferred base 0x%lx)\r\n", ActualBase);
        return EFI_SUCCESS;
    }

    Print(L"[EFI] Applying relocations: preferred=0x%lx actual=0x%lx delta=0x%lx\r\n", 
          PreferredBase, ActualBase, ActualBase - PreferredBase);

    UINT8* Base = (UINT8*)ImageBase;
    UINT32 e_lfanew = *(UINT32*)(Base + 0x3C);
    UINT8* NtHeaders = Base + e_lfanew;
    
    // Get relocation table RVA and size from data directory
    UINT32 RelocRva = *(UINT32*)(NtHeaders + 0x98);  // DataDirectory[5].VirtualAddress
    UINT32 RelocSize = *(UINT32*)(NtHeaders + 0x9C); // DataDirectory[5].Size
    
    if (RelocRva == 0 || RelocSize == 0) {
        Print(L"[EFI] No relocation table found\r\n");
        return EFI_LOAD_ERROR;
    }

    IMAGE_BASE_RELOCATION* RelocBlock = (IMAGE_BASE_RELOCATION*)(Base + RelocRva);
    UINT64 Delta = ActualBase - PreferredBase;
    
    while (RelocBlock->SizeOfBlock > 0) {
        UINT16* RelocEntry = (UINT16*)((UINT8*)RelocBlock + sizeof(IMAGE_BASE_RELOCATION));
        UINT32 NumEntries = (RelocBlock->SizeOfBlock - sizeof(IMAGE_BASE_RELOCATION)) / sizeof(UINT16);
        
        for (UINT32 i = 0; i < NumEntries; i++) {
            UINT16 Type = RelocEntry[i] >> 12;
            UINT16 Offset = RelocEntry[i] & 0xFFF;
            UINT64* FixupAddr = (UINT64*)(Base + RelocBlock->VirtualAddress + Offset);
            
            switch (Type) {
                case IMAGE_REL_BASED_ABSOLUTE:
                    break; // No fixup needed
                case IMAGE_REL_BASED_DIR64:
                    *FixupAddr += Delta;
                    break;
                default:
                    Print(L"[EFI] Unsupported relocation type: %d\r\n", Type);
                    return EFI_UNSUPPORTED;
            }
        }
        
        RelocBlock = (IMAGE_BASE_RELOCATION*)((UINT8*)RelocBlock + RelocBlock->SizeOfBlock);
    }
    
    Print(L"[EFI] Relocations applied successfully\r\n");
    return EFI_SUCCESS;
}

// Get memory map and convert to Aurora format
static EFI_STATUS GetMemoryMap(EFI_SYSTEM_TABLE* SystemTable)
{
    UINTN MapSize = MAX_MEMORY_MAP_SIZE;
    UINTN MapKey;
    UINTN DescriptorSize;
    UINT32 DescriptorVersion;
    
    EFI_STATUS Status = uefi_call_wrapper(SystemTable->BootServices->GetMemoryMap, 5,
                                          &MapSize, (EFI_MEMORY_DESCRIPTOR*)g_MemoryMapBuffer,
                                          &MapKey, &DescriptorSize, &DescriptorVersion);
    
    if (EFI_ERROR(Status)) {
        Print(L"[EFI] GetMemoryMap failed: %r\r\n", Status);
        return Status;
    }
    
    // Convert EFI memory map to Aurora format
    EFI_MEMORY_DESCRIPTOR* EfiDesc = (EFI_MEMORY_DESCRIPTOR*)g_MemoryMapBuffer;
    aurora_memory_descriptor_t* AuroraDesc = (aurora_memory_descriptor_t*)g_MemoryMapBuffer;
    UINTN NumDescriptors = MapSize / DescriptorSize;
    
    // Convert in reverse order to avoid overwriting
    for (INTN i = NumDescriptors - 1; i >= 0; i--) {
        EFI_MEMORY_DESCRIPTOR* Src = (EFI_MEMORY_DESCRIPTOR*)((UINT8*)EfiDesc + i * DescriptorSize);
        aurora_memory_descriptor_t* Dst = &AuroraDesc[i];
        
        Dst->base_address = Src->PhysicalStart;
        Dst->length = Src->NumberOfPages * 4096;
        Dst->attributes = Src->Attribute;
        
        // Convert EFI memory types to Aurora types
        switch (Src->Type) {
            case EfiConventionalMemory:
                Dst->type = AURORA_MEMORY_AVAILABLE;
                break;
            case EfiACPIReclaimMemory:
                Dst->type = AURORA_MEMORY_ACPI_RECLAIM;
                break;
            case EfiACPIMemoryNVS:
                Dst->type = AURORA_MEMORY_ACPI_NVS;
                break;
            case EfiUnusableMemory:
                Dst->type = AURORA_MEMORY_BAD;
                break;
            case EfiLoaderCode:
            case EfiLoaderData:
                Dst->type = AURORA_MEMORY_BOOTLOADER;
                break;
            default:
                Dst->type = AURORA_MEMORY_RESERVED;
                break;
        }
    }
    
    g_BootInfo.memory_map_address = (UINT64)(UINTN)g_MemoryMapBuffer;
    g_BootInfo.memory_map_size = NumDescriptors * sizeof(aurora_memory_descriptor_t);
    g_BootInfo.memory_descriptor_size = sizeof(aurora_memory_descriptor_t);
    
    Print(L"[EFI] Memory map retrieved: %lu descriptors\r\n", NumDescriptors);
    return EFI_SUCCESS;
}

static EFI_STATUS LoadKernel(EFI_HANDLE ImageHandle, EFI_SYSTEM_TABLE* SystemTable,
                             VOID** KernelBuffer, UINTN* KernelSize,
                             UINT64* ImageBasePref, UINT32* EntryRva)
{
    EFI_STATUS Status;
    EFI_LOADED_IMAGE* LoadedImage = NULL;
    EFI_SIMPLE_FILE_SYSTEM_PROTOCOL* Fs = NULL;
    EFI_FILE* Root = NULL;
    EFI_FILE* KernelFile = NULL;
    VOID* Buffer = NULL;
    UINTN Size = 0;
    *KernelBuffer = NULL; *KernelSize = 0; *ImageBasePref = 0; *EntryRva = 0;

    // Get Loaded Image Protocol
    Status = uefi_call_wrapper(SystemTable->BootServices->HandleProtocol, 3,
                               ImageHandle, &LoadedImageProtocol, (VOID**)&LoadedImage);
    if(EFI_ERROR(Status) || !LoadedImage){
        Print(L"[EFI] Failed to get LoadedImage protocol: %r\r\n", Status);
        return Status;
    }

    // Get FileSystem protocol from the device handle
    Status = uefi_call_wrapper(SystemTable->BootServices->HandleProtocol, 3,
                               LoadedImage->DeviceHandle, &gEfiSimpleFileSystemProtocolGuid, (VOID**)&Fs);
    if(EFI_ERROR(Status) || !Fs){
        Print(L"[EFI] Failed to get SimpleFileSystem protocol: %r\r\n", Status);
        return Status;
    }

    Status = uefi_call_wrapper(Fs->OpenVolume, 2, Fs, &Root);
    if(EFI_ERROR(Status) || !Root){
        Print(L"[EFI] OpenVolume failed: %r\r\n", Status);
        return Status;
    }

    Status = uefi_call_wrapper(Root->Open, 5, Root, &KernelFile, KERNEL_FILENAME, EFI_FILE_MODE_READ, 0);
    if(EFI_ERROR(Status) || !KernelFile){
        Print(L"[EFI] Kernel file '%s' not found: %r\r\n", KERNEL_FILENAME, Status);
        goto cleanup;
    }

    // Query file size first
    EFI_FILE_INFO* FileInfo = NULL;
    UINTN InfoSize = SIZE_OF_EFI_FILE_INFO + 256;
    Status = uefi_call_wrapper(SystemTable->BootServices->AllocatePool, 3, EfiLoaderData, InfoSize, (VOID**)&FileInfo);
    if(EFI_ERROR(Status)){
        Print(L"[EFI] AllocatePool for FileInfo failed: %r\r\n", Status);
        goto cleanup;
    }
    Status = uefi_call_wrapper(KernelFile->GetInfo, 4, KernelFile, &gEfiFileInfoGuid, &InfoSize, FileInfo);
    if(EFI_ERROR(Status)){
        Print(L"[EFI] GetInfo(FileInfo) failed: %r\r\n", Status);
        goto cleanup;
    }
    Size = (UINTN)FileInfo->FileSize;
    Print(L"[EFI] Kernel file size: %lu bytes\r\n", Size);

    // Get PE information first for proper allocation
    if(Size < 0x200){
        Print(L"[EFI] Kernel image too small to be valid.\r\n");
        Status = EFI_LOAD_ERROR; goto cleanup;
    }
    // Read file into temporary buffer for PE analysis
    UINTN ReadSize = Size;
    Status = uefi_call_wrapper(KernelFile->Read, 3, KernelFile, &ReadSize, FileInfo);
    if(EFI_ERROR(Status) || ReadSize != Size){
        Print(L"[EFI] Initial kernel read failed (%r) read=%lu expected=%lu\r\n", Status, ReadSize, Size);
        goto cleanup;
    }
    
    UINT8* Base = (UINT8*)FileInfo;
    UINT16 MZ = *(UINT16*)Base;
    if(MZ != 0x5A4D){ // 'MZ'
        Print(L"[EFI] Missing MZ signature.\r\n"); Status = EFI_LOAD_ERROR; goto cleanup;
    }
    UINT32 e_lfanew = *(UINT32*)(Base + 0x3C);
    if(e_lfanew + 0x100 > Size){ Print(L"[EFI] e_lfanew outside image.\r\n"); Status = EFI_LOAD_ERROR; goto cleanup; }
    UINT32 pesig = *(UINT32*)(Base + e_lfanew);
    if(pesig != 0x00004550){ // 'PE\0\0'
        Print(L"[EFI] Missing PE signature.\r\n"); Status = EFI_LOAD_ERROR; goto cleanup; }
    
    // Get preferred image base and size
    UINT32 EntryRVA = *(UINT32*)(Base + e_lfanew + 0x28);
    UINT64 ImageBase = *(UINT64*)(Base + e_lfanew + 0x30);
    UINT32 ImageSize = *(UINT32*)(Base + e_lfanew + 0x50); // SizeOfImage
    
    // Try to allocate at preferred base first
    UINTN Pages = (ImageSize + 4095) / 4096;
    EFI_PHYSICAL_ADDRESS Phys = ImageBase;
    Status = uefi_call_wrapper(SystemTable->BootServices->AllocatePages, 4, AllocateAddress, EfiLoaderData, Pages, &Phys);
    if(EFI_ERROR(Status)){
        // Fallback to any available address
        Phys = 0;
        Status = uefi_call_wrapper(SystemTable->BootServices->AllocatePages, 4, AllocateAnyPages, EfiLoaderData, Pages, &Phys);
        if(EFI_ERROR(Status)){
            Print(L"[EFI] AllocatePages failed: %r\r\n", Status);
            goto cleanup;
        }
        Print(L"[EFI] Allocated at 0x%lx (preferred was 0x%lx)\r\n", Phys, ImageBase);
    } else {
        Print(L"[EFI] Allocated at preferred base 0x%lx\r\n", Phys);
    }
    Buffer = (VOID*)(UINTN)Phys;

    // Read file into allocated buffer
    UINTN FinalReadSize = Size;
    Status = uefi_call_wrapper(KernelFile->Read, 3, KernelFile, &FinalReadSize, Buffer);
    if(EFI_ERROR(Status) || FinalReadSize != Size){
        Print(L"[EFI] Kernel read failed (%r) read=%lu expected=%lu\r\n", Status, FinalReadSize, Size);
        goto cleanup;
    }

    Print(L"[EFI] PE validated: ImageBasePref=0x%lx EntryRVA=0x%x (EntryVA=0x%lx)\r\n", ImageBase, EntryRVA, ImageBase + EntryRVA);
    
    // Apply relocations if needed
    Status = ApplyRelocations(Buffer, ImageBase, (UINT64)(UINTN)Buffer);
    if(EFI_ERROR(Status)){
        Print(L"[EFI] Relocation failed: %r\r\n", Status);
        goto cleanup;
    }
    
    *KernelBuffer = Buffer; *KernelSize = ImageSize; *ImageBasePref = ImageBase; *EntryRva = EntryRVA;
    Status = EFI_SUCCESS;

cleanup:
    if(KernelFile) uefi_call_wrapper(KernelFile->Close,1,KernelFile);
    if(Root) uefi_call_wrapper(Root->Close,1,Root);
    // Note: We intentionally do not FreePool(FileInfo) on early success path to keep code small; in production free it.
    // If failure after allocating pages, they stay allocated until exit; acceptable for early prototype.
    return Status;
}

// Setup graphics information
static EFI_STATUS SetupGraphics(EFI_SYSTEM_TABLE* SystemTable)
{
    EFI_STATUS Status;
    EFI_GRAPHICS_OUTPUT_PROTOCOL* Gop = NULL;
    
    Status = LibLocateProtocol(&gEfiGraphicsOutputProtocolGuid, (VOID**)&Gop);
    if(EFI_ERROR(Status) || !Gop) {
        Print(L"[EFI] Graphics Output Protocol not found: %r\r\n", Status);
        return Status;
    }
    
    g_BootInfo.graphics.horizontal_resolution = Gop->Mode->Info->HorizontalResolution;
    g_BootInfo.graphics.vertical_resolution = Gop->Mode->Info->VerticalResolution;
    g_BootInfo.graphics.pixels_per_scan_line = Gop->Mode->Info->PixelsPerScanLine;
    g_BootInfo.graphics.framebuffer_base = Gop->Mode->FrameBufferBase;
    g_BootInfo.graphics.framebuffer_size = Gop->Mode->FrameBufferSize;
    
    switch(Gop->Mode->Info->PixelFormat) {
        case PixelRedGreenBlueReserved8BitPerColor:
            g_BootInfo.graphics.pixel_format = 0; // RGB
            break;
        case PixelBlueGreenRedReserved8BitPerColor:
            g_BootInfo.graphics.pixel_format = 1; // BGR
            break;
        case PixelBitMask:
            g_BootInfo.graphics.pixel_format = 2; // Bitmask
            break;
        case PixelBltOnly:
            g_BootInfo.graphics.pixel_format = 3; // BltOnly
            break;
        default:
            g_BootInfo.graphics.pixel_format = 0;
            break;
    }
    
    Print(L"[EFI] Graphics: %ux%u, FB=0x%lx, Size=%lu\r\n", 
          g_BootInfo.graphics.horizontal_resolution,
          g_BootInfo.graphics.vertical_resolution,
          g_BootInfo.graphics.framebuffer_base,
          g_BootInfo.graphics.framebuffer_size);
    
    return EFI_SUCCESS;
}

// Transfer control to kernel
static void __attribute__((noreturn)) TransferControl(VOID* KernelEntry, aurora_boot_info_t* BootInfo)
{
    Print(L"[EFI] Transferring control to kernel at 0x%lx\r\n", (UINTN)KernelEntry);
    
    // Function pointer for kernel entry
    void (*KernelMain)(aurora_boot_info_t*) = (void(*)(aurora_boot_info_t*))KernelEntry;
    
    // Jump to kernel
    KernelMain(BootInfo);
    
    // Should never reach here
    for(;;) {
        __asm__("hlt");
    }
}

EFI_STATUS EFIAPI efi_main(EFI_HANDLE ImageHandle, EFI_SYSTEM_TABLE* SystemTable)
{
    InitializeLib(ImageHandle, SystemTable);
    Print(L"Aurora EFI Loader starting...\r\n");
    
    // Initialize boot info structure
    SetMem(&g_BootInfo, sizeof(g_BootInfo), 0);
    g_BootInfo.magic = AURORA_BOOT_MAGIC;
    g_BootInfo.version = AURORA_BOOT_VERSION;
    g_BootInfo.flags = AURORA_BOOT_FLAG_EFI;
    g_BootInfo.efi_system_table = (UINT64)(UINTN)SystemTable;
    
    // Load kernel
    VOID* KernelBuffer; UINTN KernelSize; UINT64 ImageBasePref; UINT32 EntryRva;
    EFI_STATUS Status = LoadKernel(ImageHandle, SystemTable, &KernelBuffer, &KernelSize, &ImageBasePref, &EntryRva);
    if(EFI_ERROR(Status)) {
        Print(L"[EFI] LoadKernel failed: %r\r\n", Status);
        goto error;
    }
    
    // Update boot info with kernel information
    g_BootInfo.kernel_physical_base = (UINT64)(UINTN)KernelBuffer;
    g_BootInfo.kernel_virtual_base = ImageBasePref;
    g_BootInfo.kernel_size = KernelSize;
    
    Print(L"[EFI] Kernel loaded at 0x%lx (%lu bytes)\r\n", (UINTN)KernelBuffer, KernelSize);
    
    // Setup graphics
    Status = SetupGraphics(SystemTable);
    if(EFI_ERROR(Status)) {
        Print(L"[EFI] Graphics setup failed: %r\r\n", Status);
        // Continue without graphics
    } else {
        g_BootInfo.flags |= AURORA_BOOT_FLAG_GRAPHICS;
    }
    
    // Get memory map
    Status = GetMemoryMap(SystemTable);
    if(EFI_ERROR(Status)) {
        Print(L"[EFI] Memory map retrieval failed: %r\r\n", Status);
        goto error;
    }
    
    // Exit boot services
    UINTN MapSize = MAX_MEMORY_MAP_SIZE;
    UINTN MapKey;
    UINTN DescriptorSize;
    UINT32 DescriptorVersion;
    
    Status = uefi_call_wrapper(SystemTable->BootServices->GetMemoryMap, 5,
                              &MapSize, (EFI_MEMORY_DESCRIPTOR*)g_MemoryMapBuffer,
                              &MapKey, &DescriptorSize, &DescriptorVersion);
    if(EFI_ERROR(Status)) {
        Print(L"[EFI] Final GetMemoryMap failed: %r\r\n", Status);
        goto error;
    }
    
    Status = uefi_call_wrapper(SystemTable->BootServices->ExitBootServices, 2, ImageHandle, MapKey);
    if(EFI_ERROR(Status)) {
        Print(L"[EFI] ExitBootServices failed: %r\r\n", Status);
        goto error;
    }
    
    // Calculate kernel entry point
    VOID* KernelEntry = (VOID*)((UINT8*)KernelBuffer + EntryRva);
    
    // Transfer control to kernel (this never returns)
    TransferControl(KernelEntry, &g_BootInfo);
    
error:
    Print(L"[EFI] Boot failed. Halting.\r\n");
    uefi_call_wrapper(SystemTable->BootServices->Stall, 1, 5000000);
    return EFI_LOAD_ERROR;
}
