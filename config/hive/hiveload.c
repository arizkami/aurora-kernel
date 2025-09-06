/*
 * Aurora Kernel - Registry Hive Loading and Persistence
 * Copyright (c) 2024 NTCore Project
 */

#include "hive.h"

#ifndef abs
#define abs(x) ((x) < 0 ? -(x) : (x))
#endif

#ifndef PUINT8
typedef UINT8* PUINT8;
#endif

/* Simple string formatting for kernel environment */
static int simple_sprintf(char* buffer, const char* format, const char* dir, const char* name) {
    int i = 0, j = 0;
    while (format[i]) {
        if (format[i] == '%' && format[i+1] == 's') {
            if (j == 0) {
                /* First %s - directory */
                const char* src = dir;
                while (*src) buffer[j++] = *src++;
            } else {
                /* Second %s - name */
                const char* src = name;
                while (*src) buffer[j++] = *src++;
            }
            i += 2;
        } else {
            buffer[j++] = format[i++];
        }
    }
    buffer[j] = '\0';
    return j;
}

/* Simple strrchr implementation for kernel environment */
static const char* simple_strrchr(const char* str, int c) {
    const char* last = NULL;
    while (*str) {
        if (*str == c) {
            last = str;
        }
        str++;
    }
    return last;
}

/* File format constants */
#define HIVE_FILE_SIGNATURE 0x66676572  /* 'regf' */
#define HIVE_BACKUP_SIGNATURE 0x6B636162 /* 'back' */
#define HIVE_LOG_SIGNATURE 0x676F6C72   /* 'rlog' */

/* File header structure */
typedef struct _HIVE_FILE_HEADER {
    UINT32 Signature;           /* File signature */
    UINT32 Version;             /* File format version */
    UINT32 HiveSize;            /* Size of hive data */
    UINT32 Checksum;            /* File checksum */
    UINT64 Timestamp;           /* Last modification time */
    UINT32 Flags;               /* File flags */
    UINT32 Reserved[3];         /* Reserved for future use */
} HIVE_FILE_HEADER, *PHIVE_FILE_HEADER;

/* Load operation context */
typedef struct _HIVE_LOAD_CONTEXT {
    PHIVE Hive;
    PVOID FileData;
    UINT32 FileSize;
    BOOLEAN UseBackup;
    BOOLEAN VerifyIntegrity;
} HIVE_LOAD_CONTEXT, *PHIVE_LOAD_CONTEXT;

/*
 * Load hive from file data
 */
NTSTATUS HiveLoadFromFile(IN PCSTR FileName, OUT PHIVE* Hive)
{
    if (!FileName || !Hive) {
        return STATUS_INVALID_PARAMETER;
    }

    *Hive = NULL;

    /* In real implementation, would read from file system */
    PVOID FileData = NULL;
    UINT32 FileSize = 0;
    
    /* Simulate file reading */
    /* NTSTATUS Status = FileSystemReadFile(FileName, &FileData, &FileSize); */
    /* if (!NT_SUCCESS(Status)) { */
    /*     return Status; */
    /* } */
    
    /* For now, return error since we can't actually read files */
    return STATUS_FILE_NOT_FOUND;
    
    /* The rest would be implemented as follows: */
    
    /* Validate file header */
    if (FileSize < sizeof(HIVE_FILE_HEADER)) {
        return STATUS_INVALID_FILE_FORMAT;
    }
    
    PHIVE_FILE_HEADER FileHeader = (PHIVE_FILE_HEADER)FileData;
    if (FileHeader->Signature != HIVE_FILE_SIGNATURE) {
        return STATUS_INVALID_FILE_SIGNATURE;
    }
    
    /* Verify file checksum */
    UINT32 StoredChecksum = FileHeader->Checksum;
    FileHeader->Checksum = 0;
    UINT32 CalculatedChecksum = HiveCalculateFileChecksum(FileData, FileSize);
    FileHeader->Checksum = StoredChecksum;
    
    if (StoredChecksum != CalculatedChecksum) {
        return STATUS_FILE_CHECKSUM_MISMATCH;
    }
    
    /* Extract hive name from filename */
    CHAR HiveName[64];
    HiveExtractNameFromPath(FileName, HiveName, sizeof(HiveName));
    
    /* Create hive structure */
    PHIVE NewHive;
    NTSTATUS Status = HiveInitialize(&NewHive, HiveName, HiveTypeUser);
    if (!NT_SUCCESS(Status)) {
        return Status;
    }
    
    /* Load hive data */
    PVOID HiveData = (UINT8*)FileData + sizeof(HIVE_FILE_HEADER);
    UINT32 HiveSize = FileHeader->HiveSize;
    
    Status = HiveLoad(NewHive, HiveData, HiveSize);
    if (!NT_SUCCESS(Status)) {
        HiveClose(NewHive);
        return Status;
    }
    
    *Hive = NewHive;
    return STATUS_SUCCESS;
}

/*
 * Save hive to file data
 */
NTSTATUS HiveSaveToFile(IN PHIVE Hive, IN PCSTR FileName)
{
    if (!Hive || !FileName) {
        return STATUS_INVALID_PARAMETER;
    }

    /* Calculate total file size */
    UINT32 FileSize = sizeof(HIVE_FILE_HEADER) + Hive->Size;
    
    /* Allocate file buffer */
    PVOID FileData = NULL; /* KernAllocateMemory(FileSize); */
    if (!FileData) {
        return STATUS_INSUFFICIENT_RESOURCES;
    }
    
    /* Prepare file header */
    PHIVE_FILE_HEADER FileHeader = (PHIVE_FILE_HEADER)FileData;
    FileHeader->Signature = HIVE_FILE_SIGNATURE;
    FileHeader->Version = HIVE_VERSION;
    FileHeader->HiveSize = Hive->Size;
    FileHeader->Timestamp = 0; /* TODO: Get current time */
    FileHeader->Flags = 0;
    memset(FileHeader->Reserved, 0, sizeof(FileHeader->Reserved));
    
    /* Copy hive data */
    PVOID HiveDataDest = (UINT8*)FileData + sizeof(HIVE_FILE_HEADER);
    memcpy(HiveDataDest, Hive->BaseAddress, Hive->Size);
    
    /* Calculate and set file checksum */
    FileHeader->Checksum = 0;
    FileHeader->Checksum = HiveCalculateFileChecksum(FileData, FileSize);
    
    /* Write to file system */
    /* NTSTATUS Status = FileSystemWriteFile(FileName, FileData, FileSize); */
    
    /* Free file buffer */
    /* KernFreeMemory(FileData); */
    
    /* For now, return success since we can't actually write files */
    return STATUS_SUCCESS;
}

/*
 * Load hive with backup support
 */
NTSTATUS HiveLoadWithBackup(IN PCSTR PrimaryFile, IN PCSTR BackupFile, OUT PHIVE* Hive)
{
    if (!PrimaryFile || !Hive) {
        return STATUS_INVALID_PARAMETER;
    }

    *Hive = NULL;
    
    /* Try to load primary file first */
    NTSTATUS Status = HiveLoadFromFile(PrimaryFile, Hive);
    if (NT_SUCCESS(Status)) {
        return Status;
    }
    
    /* If primary failed and backup exists, try backup */
    if (BackupFile) {
        Status = HiveLoadFromFile(BackupFile, Hive);
        if (NT_SUCCESS(Status)) {
            /* Mark hive as loaded from backup */
            (*Hive)->Flags |= HIVE_FLAG_LOADED_FROM_BACKUP;
            return Status;
        }
    }
    
    return Status;
}

/*
 * Create backup of hive
 */
NTSTATUS HiveCreateBackup(IN PHIVE Hive, IN PCSTR BackupFileName)
{
    if (!Hive || !BackupFileName) {
        return STATUS_INVALID_PARAMETER;
    }

    /* Flush any pending changes first */
    NTSTATUS Status = HiveFlush(Hive);
    if (!NT_SUCCESS(Status)) {
        return Status;
    }
    
    /* Save to backup file */
    return HiveSaveToFile(Hive, BackupFileName);
}

/*
 * Load hive incrementally (for large hives)
 */
NTSTATUS HiveLoadIncremental(IN PCSTR FileName, OUT PHIVE* Hive, IN UINT32 ChunkSize)
{
    if (!FileName || !Hive || ChunkSize == 0) {
        return STATUS_INVALID_PARAMETER;
    }

    *Hive = NULL;
    
    /* This would implement incremental loading for very large hives */
    /* For now, fall back to regular loading */
    return HiveLoadFromFile(FileName, Hive);
}

/*
 * Validate hive file before loading
 */
NTSTATUS HiveValidateFile(IN PCSTR FileName, OUT PBOOLEAN IsValid)
{
    if (!FileName || !IsValid) {
        return STATUS_INVALID_PARAMETER;
    }

    *IsValid = FALSE;
    
    /* Read file header */
    HIVE_FILE_HEADER FileHeader;
    /* NTSTATUS Status = FileSystemReadFilePartial(FileName, 0, &FileHeader, sizeof(FileHeader)); */
    /* if (!NT_SUCCESS(Status)) { */
    /*     return Status; */
    /* } */
    
    /* For simulation, assume file doesn't exist */
    return STATUS_FILE_NOT_FOUND;
    
    /* Validate signature */
    if (FileHeader.Signature != HIVE_FILE_SIGNATURE) {
        return STATUS_SUCCESS; /* File is invalid but function succeeded */
    }
    
    /* Validate version */
    if (FileHeader.Version > HIVE_VERSION) {
        return STATUS_SUCCESS; /* Unsupported version */
    }
    
    /* Additional validation could be performed here */
    
    *IsValid = TRUE;
    return STATUS_SUCCESS;
}

/*
 * Extract hive name from file path
 */
VOID HiveExtractNameFromPath(IN PCSTR FilePath, OUT PCHAR Name, IN UINT32 NameSize)
{
    if (!FilePath || !Name || NameSize == 0) {
        return;
    }

    /* Find last path separator */
    const CHAR* LastSeparator = simple_strrchr(FilePath, '\\');
    if (!LastSeparator) {
        LastSeparator = simple_strrchr(FilePath, '/');
    }
    
    const CHAR* FileName = LastSeparator ? (LastSeparator + 1) : FilePath;
    
    /* Copy name without extension */
    UINT32 i = 0;
    while (i < NameSize - 1 && FileName[i] && FileName[i] != '.') {
        Name[i] = FileName[i];
        i++;
    }
    Name[i] = '\0';
}

/*
 * Calculate file checksum
 */
UINT32 HiveCalculateFileChecksum(IN PVOID FileData, IN UINT32 FileSize)
{
    if (!FileData || FileSize == 0) {
        return 0;
    }

    UINT32 Checksum = 0;
    PUINT32 Data = (PUINT32)FileData;
    UINT32 DwordCount = FileSize / sizeof(UINT32);
    
    /* Calculate checksum for complete DWORDs */
    for (UINT32 i = 0; i < DwordCount; i++) {
        Checksum ^= Data[i];
        Checksum = (Checksum << 1) | (Checksum >> 31); /* Rotate left */
    }
    
    /* Handle remaining bytes */
    UINT32 RemainingBytes = FileSize % sizeof(UINT32);
    if (RemainingBytes > 0) {
        UINT32 LastDword = 0;
        PUINT8 LastBytes = (PUINT8)&Data[DwordCount];
        
        for (UINT32 i = 0; i < RemainingBytes; i++) {
            LastDword |= (LastBytes[i] << (i * 8));
        }
        
        Checksum ^= LastDword;
    }
    
    return Checksum;
}

/*
 * Load multiple hives from directory
 */
NTSTATUS HiveLoadFromDirectory(IN PCSTR DirectoryPath, OUT PHIVE** HiveArray, OUT PUINT32 HiveCount)
{
    if (!DirectoryPath || !HiveArray || !HiveCount) {
        return STATUS_INVALID_PARAMETER;
    }

    *HiveArray = NULL;
    *HiveCount = 0;
    
    /* This would enumerate files in directory and load each hive */
    /* For now, return not implemented */
    return STATUS_NOT_IMPLEMENTED;
}

/*
 * Save multiple hives to directory
 */
NTSTATUS HiveSaveToDirectory(IN PHIVE* HiveArray, IN UINT32 HiveCount, IN PCSTR DirectoryPath)
{
    if (!HiveArray || HiveCount == 0 || !DirectoryPath) {
        return STATUS_INVALID_PARAMETER;
    }

    NTSTATUS Status = STATUS_SUCCESS;
    
    for (UINT32 i = 0; i < HiveCount; i++) {
        if (!HiveArray[i]) {
            continue;
        }
        
        /* Construct file path */
        CHAR FilePath[256];
        simple_sprintf(FilePath, "%s\\%s.hiv", DirectoryPath, HiveArray[i]->Name);
        
        /* Save hive */
        NTSTATUS SaveStatus = HiveSaveToFile(HiveArray[i], FilePath);
        if (!NT_SUCCESS(SaveStatus) && NT_SUCCESS(Status)) {
            Status = SaveStatus; /* Remember first error */
        }
    }
    
    return Status;
}

/*
 * Load hive with transaction log support
 */
NTSTATUS HiveLoadWithTransactionLog(IN PCSTR HiveFile, IN PCSTR LogFile, OUT PHIVE* Hive)
{
    if (!HiveFile || !Hive) {
        return STATUS_INVALID_PARAMETER;
    }

    *Hive = NULL;
    
    /* Load primary hive */
    NTSTATUS Status = HiveLoadFromFile(HiveFile, Hive);
    if (!NT_SUCCESS(Status)) {
        return Status;
    }
    
    /* Apply transaction log if present */
    if (LogFile) {
        Status = HiveApplyTransactionLog(*Hive, LogFile);
        if (!NT_SUCCESS(Status)) {
            HiveClose(*Hive);
            *Hive = NULL;
            return Status;
        }
    }
    
    return STATUS_SUCCESS;
}

/*
 * Apply transaction log to hive
 */
NTSTATUS HiveApplyTransactionLog(IN PHIVE Hive, IN PCSTR LogFile)
{
    if (!Hive || !LogFile) {
        return STATUS_INVALID_PARAMETER;
    }

    /* This would read and apply transaction log entries */
    /* For now, return success (no-op) */
    return STATUS_SUCCESS;
}

/*
 * Create transaction log entry
 */
NTSTATUS HiveCreateLogEntry(IN PHIVE Hive, IN UINT32 Operation, IN UINT32 Offset, IN PVOID Data, IN UINT32 Size)
{
    if (!Hive) {
        return STATUS_INVALID_PARAMETER;
    }

    /* This would create a transaction log entry for the operation */
    /* For now, return success (no-op) */
    UNREFERENCED_PARAMETER(Operation);
    UNREFERENCED_PARAMETER(Offset);
    UNREFERENCED_PARAMETER(Data);
    UNREFERENCED_PARAMETER(Size);
    
    return STATUS_SUCCESS;
}

/*
 * Compact hive during save
 */
NTSTATUS HiveSaveCompacted(IN PHIVE Hive, IN PCSTR FileName)
{
    if (!Hive || !FileName) {
        return STATUS_INVALID_PARAMETER;
    }

    /* Create a compacted copy of the hive */
    PHIVE CompactedHive;
    NTSTATUS Status = HiveCreateCompactedCopy(Hive, &CompactedHive);
    if (!NT_SUCCESS(Status)) {
        return Status;
    }
    
    /* Save compacted hive */
    Status = HiveSaveToFile(CompactedHive, FileName);
    
    /* Clean up compacted hive */
    HiveClose(CompactedHive);
    
    return Status;
}

/*
 * Create compacted copy of hive
 */
NTSTATUS HiveCreateCompactedCopy(IN PHIVE SourceHive, OUT PHIVE* CompactedHive)
{
    if (!SourceHive || !CompactedHive) {
        return STATUS_INVALID_PARAMETER;
    }

    *CompactedHive = NULL;
    
    /* Calculate compacted size */
    UINT32 CompactedSize = HiveCalculateCompactedSize(SourceHive);
    
    /* Create new hive */
    PHIVE NewHive;
    NTSTATUS Status = HiveInitialize(&NewHive, SourceHive->Name, SourceHive->Type);
    if (!NT_SUCCESS(Status)) {
        return Status;
    }
    
    Status = HiveCreate(NewHive, CompactedSize);
    if (!NT_SUCCESS(Status)) {
        HiveClose(NewHive);
        return Status;
    }
    
    /* Copy and compact data */
    Status = HiveCopyCompacted(SourceHive, NewHive);
    if (!NT_SUCCESS(Status)) {
        HiveClose(NewHive);
        return Status;
    }
    
    *CompactedHive = NewHive;
    return STATUS_SUCCESS;
}

/*
 * Calculate compacted hive size
 */
UINT32 HiveCalculateCompactedSize(IN PHIVE Hive)
{
    if (!Hive) {
        return 0;
    }

    UINT32 CompactedSize = sizeof(HIVE_HEADER);
    
    /* Scan hive and count allocated cells */
    UINT32 Offset = sizeof(HIVE_HEADER);
    while (Offset < Hive->Size) {
        PCELL_HEADER Cell = (PCELL_HEADER)((UINT8*)Hive->BaseAddress + Offset);
        
        if (Cell->Size < 0) {
            /* Allocated cell - include in compacted size */
            CompactedSize += abs(Cell->Size);
        }
        
        Offset += abs(Cell->Size);
    }
    
    /* Add some padding for future allocations */
    CompactedSize += 4096;
    
    return CompactedSize;
}

/*
 * Copy hive data in compacted form
 */
NTSTATUS HiveCopyCompacted(IN PHIVE SourceHive, IN PHIVE DestHive)
{
    if (!SourceHive || !DestHive) {
        return STATUS_INVALID_PARAMETER;
    }

    /* Copy header */
    memcpy(DestHive->Header, SourceHive->Header, sizeof(HIVE_HEADER));
    
    /* Copy allocated cells sequentially */
    UINT32 SourceOffset = sizeof(HIVE_HEADER);
    UINT32 DestOffset = sizeof(HIVE_HEADER);
    
    while (SourceOffset < SourceHive->Size && DestOffset < DestHive->Size) {
        PCELL_HEADER SourceCell = (PCELL_HEADER)((UINT8*)SourceHive->BaseAddress + SourceOffset);
        
        if (SourceCell->Size < 0) {
            /* Allocated cell - copy to destination */
            UINT32 CellSize = abs(SourceCell->Size);
            
            if (DestOffset + CellSize > DestHive->Size) {
                return STATUS_BUFFER_TOO_SMALL;
            }
            
            memcpy((UINT8*)DestHive->BaseAddress + DestOffset, 
                   (UINT8*)SourceHive->BaseAddress + SourceOffset, 
                   CellSize);
            
            DestOffset += CellSize;
        }
        
        SourceOffset += abs(SourceCell->Size);
    }
    
    /* Update destination hive size */
    DestHive->Size = DestOffset;
    DestHive->Header->Size = DestOffset;
    
    /* Create free space for remaining area */
    if (DestOffset < DestHive->Size) {
        UINT32 FreeSize = DestHive->Size - DestOffset;
        PCELL_HEADER FreeCell = (PCELL_HEADER)((UINT8*)DestHive->BaseAddress + DestOffset);
        FreeCell->Size = (INT32)FreeSize;
        FreeCell->Signature = CellTypeFree;
        
        HiveFreeSpaceAdd(DestHive, DestOffset, FreeSize);
    }
    
    return STATUS_SUCCESS;
}