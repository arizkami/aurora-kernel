/* Aurora minimal PE/COFF loader interfaces */
#ifndef _AURORA_NTPE_H_
#define _AURORA_NTPE_H_
#include "../aurora.h"

/* Basic PE headers (trimmed) */
#pragma pack(push,1)
typedef struct _PE_DOS_HEADER { UINT16 e_magic; UINT16 e_cblp; UINT16 e_cp; UINT16 e_crlc; UINT16 e_cparhdr; UINT16 e_minalloc; UINT16 e_maxalloc; UINT16 e_ss; UINT16 e_sp; UINT16 e_csum; UINT16 e_ip; UINT16 e_cs; UINT16 e_lfarlc; UINT16 e_ovno; UINT16 e_res[4]; UINT16 e_oemid; UINT16 e_oeminfo; UINT16 e_res2[10]; UINT32 e_lfanew; } PE_DOS_HEADER;

typedef struct _PE_FILE_HEADER { UINT16 Machine; UINT16 NumberOfSections; UINT32 TimeDateStamp; UINT32 PointerToSymbolTable; UINT32 NumberOfSymbols; UINT16 SizeOfOptionalHeader; UINT16 Characteristics; } PE_FILE_HEADER;

typedef struct _PE_DATA_DIRECTORY { UINT32 VirtualAddress; UINT32 Size; } PE_DATA_DIRECTORY;

#define PE_NUMBER_OF_RVAS 16

typedef struct _PE_OPTIONAL_HEADER64 { UINT16 Magic; UINT8 MajorLinkerVersion; UINT8 MinorLinkerVersion; UINT32 SizeOfCode; UINT32 SizeOfInitializedData; UINT32 SizeOfUninitializedData; UINT32 AddressOfEntryPoint; UINT32 BaseOfCode; UINT64 ImageBase; UINT32 SectionAlignment; UINT32 FileAlignment; UINT16 MajorOSVersion; UINT16 MinorOSVersion; UINT16 MajorImageVersion; UINT16 MinorImageVersion; UINT16 MajorSubsystemVersion; UINT16 MinorSubsystemVersion; UINT32 Win32VersionValue; UINT32 SizeOfImage; UINT32 SizeOfHeaders; UINT32 CheckSum; UINT16 Subsystem; UINT16 DllCharacteristics; UINT64 SizeOfStackReserve; UINT64 SizeOfStackCommit; UINT64 SizeOfHeapReserve; UINT64 SizeOfHeapCommit; UINT32 LoaderFlags; UINT32 NumberOfRvaAndSizes; PE_DATA_DIRECTORY DataDirectory[PE_NUMBER_OF_RVAS]; } PE_OPTIONAL_HEADER64;

typedef struct _PE_NT_HEADERS64 { UINT32 Signature; PE_FILE_HEADER FileHeader; PE_OPTIONAL_HEADER64 OptionalHeader; } PE_NT_HEADERS64;

typedef struct _PE_SECTION_HEADER { UINT8 Name[8]; UINT32 VirtualSize; UINT32 VirtualAddress; UINT32 SizeOfRawData; UINT32 PointerToRawData; UINT32 PointerToRelocations; UINT32 PointerToLinenumbers; UINT16 NumberOfRelocations; UINT16 NumberOfLinenumbers; UINT32 Characteristics; } PE_SECTION_HEADER;
#pragma pack(pop)

/* Loader result */
typedef struct _NTPE_IMAGE {
    PVOID ImageBase; /* allocated and populated */
    UINT64 ImageSize;
    UINT64 EntryPoint; /* RVA entry -> absolute */
} NTPE_IMAGE, *PNTPE_IMAGE;

/* Directory indices */
#define NTPE_DIR_EXPORT          0
#define NTPE_DIR_IMPORT          1
#define NTPE_DIR_BASERELOC       5

/* Relocation types (subset) */
#define NTPE_REL_ABSOLUTE        0
#define NTPE_REL_DIR64           10

/* Minimal structures for reloc / import */
typedef struct _NTPE_BASE_RELOCATION_BLOCK { UINT32 VirtualAddress; UINT32 SizeOfBlock; } NTPE_BASE_RELOCATION_BLOCK; 
typedef struct _NTPE_IMPORT_DESCRIPTOR { UINT32 OriginalFirstThunk; UINT32 TimeDateStamp; UINT32 ForwarderChain; UINT32 Name; UINT32 FirstThunk; } NTPE_IMPORT_DESCRIPTOR;

NTSTATUS NtPeLoadImage(IN PVOID Buffer, IN UINT32 Size, OUT PNTPE_IMAGE ImageOut);

#endif /* _AURORA_NTPE_H_ */
