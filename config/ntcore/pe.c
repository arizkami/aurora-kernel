/* Minimal in-kernel PE64 image loader (no reloc/imports) */
#include "../../aurora.h"
#include "../../include/ntpe.h"

#define PE_DOS_MAGIC 0x5A4D
#define PE_NT_MAGIC  0x00004550
#define PE_OPT_MAGIC_PE32PLUS 0x20B

static int NtPeValidateHeaders(PE_DOS_HEADER* dos, PE_NT_HEADERS64* nt, UINT32 size){
    if(!dos || !nt) return 0;
    if(dos->e_magic != PE_DOS_MAGIC) return 0;
    if(nt->Signature != PE_NT_MAGIC) return 0;
    if(nt->OptionalHeader.Magic != PE_OPT_MAGIC_PE32PLUS) return 0;
    if(nt->OptionalHeader.SizeOfImage == 0 || nt->OptionalHeader.SectionAlignment == 0) return 0;
    if(nt->OptionalHeader.SizeOfHeaders > size) return 0;
    return 1;
}

static NTSTATUS NtPeApplyRelocations(UINT8* imageBase, PE_NT_HEADERS64* nt, UINT64 loadedBase){
    PE_DATA_DIRECTORY* relocDir = &nt->OptionalHeader.DataDirectory[NTPE_DIR_BASERELOC];
    if(relocDir->VirtualAddress == 0 || relocDir->Size == 0) return STATUS_SUCCESS; /* no relocs */
    UINT64 imageBasePref = nt->OptionalHeader.ImageBase;
    INT64 delta = (INT64)(loadedBase - imageBasePref);
    if(delta == 0) return STATUS_SUCCESS;
    UINT32 parsed = 0;
    while(parsed < relocDir->Size){
        NTPE_BASE_RELOCATION_BLOCK* block = (NTPE_BASE_RELOCATION_BLOCK*)(imageBase + relocDir->VirtualAddress + parsed);
        if(block->SizeOfBlock == 0) break;
        UINT32 entryCount = (block->SizeOfBlock - sizeof(NTPE_BASE_RELOCATION_BLOCK)) / sizeof(UINT16);
        UINT16* entries = (UINT16*)((UINT8*)block + sizeof(NTPE_BASE_RELOCATION_BLOCK));
        for(UINT32 i=0;i<entryCount;i++){
            UINT16 e = entries[i];
            UINT16 type = (UINT16)(e >> 12);
            UINT16 off = (UINT16)(e & 0x0FFF);
            if(type == NTPE_REL_DIR64){
                UINT64* target = (UINT64*)(imageBase + block->VirtualAddress + off);
                *target = (UINT64)((INT64)(*target) + delta);
            }
        }
        parsed += block->SizeOfBlock;
    }
    return STATUS_SUCCESS;
}

static NTSTATUS NtPeProcessImports(UINT8* imageBase, PE_NT_HEADERS64* nt){
    PE_DATA_DIRECTORY* impDir = &nt->OptionalHeader.DataDirectory[NTPE_DIR_IMPORT];
    if(impDir->VirtualAddress == 0 || impDir->Size == 0) return STATUS_SUCCESS; /* no imports */
    /* Stub: mark imported IAT entries as zero (unresolved) */
    if(impDir->VirtualAddress + impDir->Size > nt->OptionalHeader.SizeOfImage) return STATUS_INVALID_IMAGE_FORMAT;
    NTPE_IMPORT_DESCRIPTOR* desc = (NTPE_IMPORT_DESCRIPTOR*)(imageBase + impDir->VirtualAddress);
    while(desc->Name){
        /* For real system: locate module, resolve thunks */
        if(desc->FirstThunk){
            UINT64* thunk = (UINT64*)(imageBase + desc->FirstThunk);
            while(*thunk){
                /* Clear high bit / ordinal (not parsed here) */
                *thunk = 0; /* unresolved placeholder */
                ++thunk;
            }
        }
        ++desc;
    }
    return STATUS_SUCCESS;
}

NTSTATUS NtPeLoadImage(IN PVOID Buffer, IN UINT32 Size, OUT PNTPE_IMAGE ImageOut){
    if(!Buffer || !ImageOut || Size < sizeof(PE_DOS_HEADER)) return STATUS_INVALID_PARAMETER;
    PE_DOS_HEADER* dos = (PE_DOS_HEADER*)Buffer;
    if(dos->e_lfanew + sizeof(PE_NT_HEADERS64) > Size) return STATUS_INVALID_IMAGE_FORMAT;
    PE_NT_HEADERS64* nt = (PE_NT_HEADERS64*)((UINT8*)Buffer + dos->e_lfanew);
    if(!NtPeValidateHeaders(dos, nt, Size)) return STATUS_INVALID_IMAGE_FORMAT;

    UINT64 imageSize = nt->OptionalHeader.SizeOfImage;
    UINT8* imageBase = (UINT8*)AuroraAllocateMemory(imageSize);
    if(!imageBase) return STATUS_INSUFFICIENT_RESOURCES;
    AuroraMemoryZero(imageBase, imageSize);

    /* Copy headers */
    UINT32 headersSize = nt->OptionalHeader.SizeOfHeaders;
    if(headersSize > Size) return STATUS_INVALID_IMAGE_FORMAT;
    AuroraMemoryCopy(imageBase, Buffer, headersSize);

    /* Iterate sections */
    PE_SECTION_HEADER* sec = (PE_SECTION_HEADER*)( (UINT8*)&nt->OptionalHeader + nt->FileHeader.SizeOfOptionalHeader );
    for(UINT16 i=0;i<nt->FileHeader.NumberOfSections;i++){
        UINT64 vaddr = sec[i].VirtualAddress;
        UINT64 rawSize = sec[i].SizeOfRawData;
        UINT64 rawPtr = sec[i].PointerToRawData;
        if(rawPtr + rawSize > Size){ return STATUS_INVALID_IMAGE_FORMAT; }
        if(vaddr + rawSize > imageSize){ return STATUS_INVALID_IMAGE_FORMAT; }
        AuroraMemoryCopy(imageBase + vaddr, (UINT8*)Buffer + rawPtr, rawSize);
    }

    NTSTATUS st = NtPeApplyRelocations(imageBase, nt, (UINT64)imageBase);
    if(!NT_SUCCESS(st)) return st;
    st = NtPeProcessImports(imageBase, nt);
    if(!NT_SUCCESS(st)) return st;

    ImageOut->ImageBase = imageBase;
    ImageOut->ImageSize = imageSize;
    ImageOut->EntryPoint = (UINT64)(imageBase + nt->OptionalHeader.AddressOfEntryPoint);
    return STATUS_SUCCESS;
}
