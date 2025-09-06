// Aurora EFI Loader (x86_64) using gnu-efi
// Responsibilities (current stage):
//  - Locate the EFI system partition of this image
//  - Open root directory and read \"aurkern.exe\" into memory
//  - Perform minimal PE (MZ + PE) signature validation
//  - Report image size, preferred image base, and entry point RVA
//  - (Future) Apply PE relocations if not loaded at preferred base and jump to kernel
//  - (Future) Provide memory map to kernel and exit boot services

#include <efi.h>
#include <efilib.h>

#define KERNEL_FILENAME L"aurkern.exe"

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

    // Allocate pages for kernel image (page-aligned load buffer)
    UINTN Pages = (Size + 4095) / 4096;
    EFI_PHYSICAL_ADDRESS Phys = 0;
    Status = uefi_call_wrapper(SystemTable->BootServices->AllocatePages, 4, AllocateAnyPages, EfiLoaderData, Pages, &Phys);
    if(EFI_ERROR(Status)){
        Print(L"[EFI] AllocatePages failed: %r\r\n", Status);
        goto cleanup;
    }
    Buffer = (VOID*)(UINTN)Phys;

    // Read file
    UINTN ReadSize = Size;
    Status = uefi_call_wrapper(KernelFile->Read, 3, KernelFile, &ReadSize, Buffer);
    if(EFI_ERROR(Status) || ReadSize != Size){
        Print(L"[EFI] Kernel read failed (%r) read=%lu expected=%lu\r\n", Status, ReadSize, Size);
        goto cleanup;
    }

    // Minimal PE validation (assumes PE32+)
    if(Size < 0x200){
        Print(L"[EFI] Kernel image too small to be valid.\r\n");
        Status = EFI_LOAD_ERROR; goto cleanup;
    }
    UINT8* Base = (UINT8*)Buffer;
    UINT16 MZ = *(UINT16*)Base;
    if(MZ != 0x5A4D){ // 'MZ'
        Print(L"[EFI] Missing MZ signature.\r\n"); Status = EFI_LOAD_ERROR; goto cleanup;
    }
    UINT32 e_lfanew = *(UINT32*)(Base + 0x3C);
    if(e_lfanew + 0x100 > Size){ Print(L"[EFI] e_lfanew outside image.\r\n"); Status = EFI_LOAD_ERROR; goto cleanup; }
    UINT32 pesig = *(UINT32*)(Base + e_lfanew);
    if(pesig != 0x00004550){ // 'PE\0\0'
        Print(L"[EFI] Missing PE signature.\r\n"); Status = EFI_LOAD_ERROR; goto cleanup; }
    // Entry RVA at e_lfanew + 0x28, ImageBase at e_lfanew + 0x30 + 8 (for PE32+) => offset 0x30? (OptionalHeader start is +24)
    UINT32 EntryRVA = *(UINT32*)(Base + e_lfanew + 0x28);
    UINT64 ImageBase = *(UINT64*)(Base + e_lfanew + 0x30); // For PE32+ OptionalHeader.ImageBase offset
    Print(L"[EFI] PE validated: ImageBasePref=0x%lx EntryRVA=0x%x (EntryVA=0x%lx)\r\n", ImageBase, EntryRVA, ImageBase + EntryRVA);
    *KernelBuffer = Buffer; *KernelSize = Size; *ImageBasePref = ImageBase; *EntryRva = EntryRVA;
    Status = EFI_SUCCESS;

cleanup:
    if(KernelFile) uefi_call_wrapper(KernelFile->Close,1,KernelFile);
    if(Root) uefi_call_wrapper(Root->Close,1,Root);
    // Note: We intentionally do not FreePool(FileInfo) on early success path to keep code small; in production free it.
    // If failure after allocating pages, they stay allocated until exit; acceptable for early prototype.
    return Status;
}

EFI_STATUS EFIAPI efi_main(EFI_HANDLE ImageHandle, EFI_SYSTEM_TABLE* SystemTable)
{
    InitializeLib(ImageHandle, SystemTable);
    Print(L"Aurora EFI Loader starting...\r\n");
    VOID* KernelBuffer; UINTN KernelSize; UINT64 ImageBasePref; UINT32 EntryRva;
    EFI_STATUS st = LoadKernel(ImageHandle,SystemTable,&KernelBuffer,&KernelSize,&ImageBasePref,&EntryRva);
    if(EFI_ERROR(st)){
        Print(L"[EFI] LoadKernel failed: %r\r\n", st);
    } else {
        Print(L"[EFI] Kernel loaded at 0x%lx (%lu bytes).\r\n", (UINTN)KernelBuffer, KernelSize);
        Print(L"[EFI] (Not jumping yet) TODO: Relocate if needed & ExitBootServices -> transfer control.\r\n");
    }
    Print(L"Aurora EFI Loader done. Halting.\r\n");
    uefi_call_wrapper(SystemTable->BootServices->Stall, 1, 2000000);
    return EFI_SUCCESS;
}
