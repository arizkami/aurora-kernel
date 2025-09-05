// Aurora EFI Loader (x86_64) using gnu-efi
#include <efi.h>
#include <efilib.h>

EFI_STATUS EFIAPI efi_main(EFI_HANDLE ImageHandle, EFI_SYSTEM_TABLE* SystemTable)
{
    InitializeLib(ImageHandle, SystemTable);
    Print(L"Aurora EFI Loader starting...\r\n");
    // TODO: Load aurkern.exe or kernel image from disk
    Print(L"Aurora EFI Loader done. Halting.\r\n");
    // Stall so we can see output
    uefi_call_wrapper(SystemTable->BootServices->Stall, 1, 1000000);
    return EFI_SUCCESS;
}
