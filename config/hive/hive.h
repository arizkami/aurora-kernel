/*
 * Aurora Kernel - Registry Hive Management Header
 * Copyright (c) 2024 NTCore Project
 */

#ifndef _HIVE_H_
#define _HIVE_H_

#include "../../aurora.h"
#include "../../include/kern.h"

/* Type definitions */
#ifndef BOOLEAN
typedef BOOL BOOLEAN;
#endif

typedef BOOLEAN* PBOOLEAN;

/* Hive Constants */
#define HIVE_SIGNATURE          0x66676572  /* 'regf' */
#define HIVE_FLAG_LOADED_FROM_BACKUP 0x00000001
#define HIVE_HEADER_SIGNATURE   0x66676572  /* 'regf' */
#define HIVE_VERSION            0x00000001
#define HIVE_BLOCK_SIZE         4096
#define HIVE_MAX_NAME_LENGTH    255
#define HIVE_MAX_VALUE_SIZE     (1024 * 1024)  /* 1MB */

/* Hive Types */
typedef enum _HIVE_TYPE {
    HiveTypeVolatile = 0,
    HiveTypeNonVolatile = 1,
    HiveTypeSystem = 2,
    HiveTypeUser = 3
} HIVE_TYPE, *PHIVE_TYPE;

/* Cell Types */
typedef enum _CELL_TYPE {
    CellTypeKey = 0x6B6E,      /* 'nk' */
    CellTypeValue = 0x6B76,    /* 'vk' */
    CellTypeSubkeys = 0x666C,  /* 'lf' */
    CellTypeData = 0x6264,     /* 'db' */
    CellTypeSecurity = 0x6B73, /* 'sk' */
    CellTypeSecurityDescriptor = 5,
    CellTypeFree = 0x0000      /* Free cell */
} CELL_TYPE;

/* Hive Header Structure */
typedef struct _HIVE_HEADER {
    UINT32 Signature;
    UINT32 PrimarySequence;
    UINT32 SecondarySequence;
    UINT64 LastWriteTime;
    UINT64 Timestamp;
    UINT32 MajorVersion;
    UINT32 MinorVersion;
    UINT32 Version;
    UINT32 Type;
    UINT32 Format;
    UINT32 Flags;
    UINT32 RootCell;
    UINT32 RootKeyOffset;
    UINT32 Length;
    UINT32 Size;
    UINT32 Cluster;
    CHAR FileName[64];
    UINT32 Checksum;
    UINT8 Reserved[3584];
} HIVE_HEADER, *PHIVE_HEADER;

/* Cell Header */
typedef struct _CELL_HEADER {
    INT32 Size;                /* Negative if allocated, positive if free */
    UINT16 Signature;
    UINT16 Flags;
} CELL_HEADER, *PCELL_HEADER;

/* Key Cell */
typedef struct _KEY_CELL {
    CELL_HEADER Header;
    UINT16 Flags;
    UINT64 LastWriteTime;
    UINT32 Spare;
    UINT32 Parent;
    UINT32 SubKeysCount;
    UINT32 VolatileSubKeysCount;
    UINT32 SubKeysList;
    UINT32 VolatileSubKeysList;
    UINT32 ValuesCount;
    UINT32 ValuesList;
    UINT32 Security;
    UINT32 SecurityOffset;
    UINT32 Class;
    UINT32 ClassOffset;
    UINT32 MaxNameLen;
    UINT32 MaxClassLen;
    UINT32 MaxValueNameLen;
    UINT32 MaxValueDataLen;
    UINT32 WorkVar;
    UINT16 NameLength;
    UINT16 ClassLength;
    CHAR Name[1];
} KEY_CELL, *PKEY_CELL;

/* Value Cell */
typedef struct _VALUE_CELL {
    CELL_HEADER Header;
    UINT16 NameLength;
    UINT32 DataLength;
    UINT32 DataOffset;
    UINT32 Type;
    UINT16 Flags;
    UINT16 Spare;
    CHAR Name[1];
} VALUE_CELL, *PVALUE_CELL;

/* Hive Structure */
typedef struct _HIVE {
    PHIVE_HEADER Header;
    UINT32 Signature;
    UINT32 Version;
    UINT32 Flags;
    PVOID BaseAddress;
    SIZE_T Size;
    HIVE_TYPE Type;
    BOOL Dirty;
    BOOL DirtyFlag;
    BOOL ReadOnly;
    UINT32 RefCount;
    UINT32 RootKeyOffset;
    CHAR Name[256];
    AURORA_SPINLOCK Lock;
    struct _HIVE* Next;
} HIVE, *PHIVE;

/* Forward declarations */
typedef struct _HIVE_MAPPING HIVE_MAPPING, *PHIVE_MAPPING;
typedef struct _HIVE_VIEW HIVE_VIEW, *PHIVE_VIEW;
typedef struct _HIVE_TRANSACTION HIVE_TRANSACTION, *PHIVE_TRANSACTION;
typedef struct _HIVE_RWLOCK HIVE_RWLOCK, *PHIVE_RWLOCK;

/* Lock Types */
typedef enum _HIVE_LOCK_TYPE {
    HiveLockShared = 0,
    HiveLockExclusive = 1
} HIVE_LOCK_TYPE;

/* Function Declarations */

/* Initialization */
NTSTATUS HiveInitialize(OUT PHIVE* Hive, IN PCSTR Name, IN HIVE_TYPE Type);
VOID HiveShutdown(void);

/* Hive Management */
NTSTATUS HiveCreate(IN PHIVE Hive, IN UINT32 InitialSize);
NTSTATUS HiveLoad(IN PHIVE Hive, IN PVOID HiveData, IN UINT32 HiveSize);
NTSTATUS HiveLoadFromData(IN PHIVE Hive, IN PVOID HiveData, IN UINT32 HiveSize);
NTSTATUS HiveUnload(IN PHIVE Hive);
NTSTATUS HiveFlush(IN PHIVE Hive);
NTSTATUS HiveInitializeSystem(void);
VOID HiveShutdownSystem(void);
PHIVE HiveFindByName(IN PCSTR Name);
NTSTATUS HiveAddToList(IN PHIVE Hive);
NTSTATUS HiveRemoveFromList(IN PHIVE Hive);
NTSTATUS HiveClose(IN PHIVE Hive);
NTSTATUS HiveSaveToFile(IN PHIVE Hive, IN PCSTR FileName);
NTSTATUS HiveCreateBackup(IN PHIVE Hive, IN PCSTR BackupPath);
NTSTATUS HiveCompact(IN PHIVE Hive);
NTSTATUS HiveCheckIntegrity(IN PHIVE Hive, IN PVOID BaseAddress, IN SIZE_T Size);

/* Cell Management */
UINT32 HiveAllocateCell(IN PHIVE Hive, IN SIZE_T Size);
VOID HiveFreeCell(IN PHIVE Hive, IN UINT32 CellOffset);
PVOID HiveGetCell(IN PHIVE Hive, IN UINT32 CellOffset);
PKEY_CELL HiveGetKeyCell(IN PHIVE Hive, IN UINT32 CellOffset);
PVALUE_CELL HiveGetValueCell(IN PHIVE Hive, IN UINT32 CellOffset);
BOOL HiveValidateCell(IN PHIVE Hive, IN UINT32 CellOffset);
BOOL HiveBinValidateBlock(IN PHIVE Hive, IN UINT32 Offset);
NTSTATUS HiveCellResize(IN PHIVE Hive, IN UINT32 CellOffset, IN SIZE_T NewSize);
BOOL HiveCheckKeyCell(IN PHIVE Hive, IN UINT32 CellOffset);
BOOL HiveCheckValueCell(IN PHIVE Hive, IN UINT32 CellOffset);
NTSTATUS HiveBinCompact(IN PHIVE Hive);

/* Key Operations */
NTSTATUS HiveCreateKey(IN PHIVE Hive, IN PCSTR KeyPath, OUT PUINT32 KeyCell);
NTSTATUS HiveFindKey(IN PHIVE Hive, IN PCSTR KeyPath, OUT PUINT32 KeyCell);
NTSTATUS HiveDeleteKey(IN PHIVE Hive, IN PCSTR KeyPath);
NTSTATUS HiveEnumerateKeys(IN PHIVE Hive, IN UINT32 ParentKey, IN UINT32 Index, OUT PCHAR KeyName, IN OUT PUINT32 NameSize);

/* Value Operations */
NTSTATUS HiveSetValue(IN PHIVE Hive, IN UINT32 KeyCell, IN PCSTR ValueName, IN UINT32 Type, IN PVOID Data, IN UINT32 DataSize);
NTSTATUS HiveGetValue(IN PHIVE Hive, IN UINT32 KeyCell, IN PCSTR ValueName, OUT PUINT32 Type, OUT PVOID Data, IN OUT PUINT32 DataSize);
NTSTATUS HiveDeleteValue(IN PHIVE Hive, IN UINT32 KeyCell, IN PCSTR ValueName);
NTSTATUS HiveEnumerateValues(IN PHIVE Hive, IN UINT32 KeyCell, IN UINT32 Index, OUT PCHAR ValueName, IN OUT PUINT32 NameSize);

/* Checksum and Integrity */
UINT32 HiveCalculateChecksum(IN PHIVE_HEADER Header);
BOOLEAN HiveVerifyChecksum(IN PHIVE_HEADER Header);
NTSTATUS HiveMap(IN PHIVE Hive, IN PVOID BaseAddress, IN SIZE_T Size);
VOID HiveUnmap(IN PHIVE Hive);

/* Statistics */
typedef struct _HIVE_STATISTICS {
    UINT32 KeyCells;
    UINT32 ValueCells;
    UINT32 AllocatedCells;
    UINT32 FreeCells;
    UINT32 SecurityCells;
    UINT32 OtherCells;
    UINT32 HeaderSize;
    UINT32 TotalSize;
    UINT32 FreeSpace;
    UINT32 UsedSpace;
    UINT32 FragmentationPercent;
} HIVE_STATISTICS, *PHIVE_STATISTICS;

NTSTATUS HiveGetStatistics(IN PHIVE Hive, OUT PHIVE_STATISTICS Statistics);
NTSTATUS HiveRebuildFreeSpaceMap(IN PHIVE Hive);
VOID HiveFreeSpaceClear(IN PHIVE Hive);
VOID HiveFreeSpaceAdd(IN PHIVE Hive, IN UINT32 Offset, IN UINT32 Size);
UINT32 HiveCalculateCompactedSize(IN PHIVE Hive);
NTSTATUS HiveCopyCompacted(IN PHIVE SourceHive, IN PHIVE DestHive);
NTSTATUS HiveApplyTransactionLog(IN PHIVE Hive, IN PCSTR LogFile);
NTSTATUS HiveCreateCompactedCopy(IN PHIVE SourceHive, OUT PHIVE* CompactedHive);
UINT32 HiveCalculateFileChecksum(IN PVOID FileData, IN UINT32 FileSize);
VOID HiveExtractNameFromPath(IN PCSTR FilePath, OUT PCHAR Name, IN UINT32 NameSize);

/* Mapping Functions */
PHIVE_MAPPING HiveFindMapping(IN PHIVE Hive);
PHIVE_VIEW HiveFindView(IN PHIVE_MAPPING Mapping, IN UINT32 Offset, IN UINT32 Size);
VOID HiveRemoveMapping(IN PHIVE_MAPPING Mapping);
NTSTATUS HiveFlushMappedViews(IN PHIVE_MAPPING Mapping);
NTSTATUS HiveFlushView(IN PHIVE_MAPPING Mapping, IN PHIVE_VIEW View);
NTSTATUS HiveUnmapHive(IN PHIVE Hive);
PHIVE_TRANSACTION HiveFindTransaction(IN UINT32 TransactionId);
VOID HiveRemoveTransaction(IN PHIVE_TRANSACTION Transaction);
VOID HiveWakeupWaiters(IN PHIVE_RWLOCK RWLock, IN HIVE_LOCK_TYPE LockType);
NTSTATUS HiveAbortTransaction(IN UINT32 TransactionId);

/* Value Types */
typedef enum _VALUE_TYPE {
    ValueTypeString = 1,
    ValueTypeDword = 2,
    ValueTypeQword = 3,
    ValueTypeBinary = 4,
    ValueTypeMultiString = 5
} VALUE_TYPE;

/* Hint Types */
typedef enum _HINT_TYPE {
    HintTypeKeyAccess = 1,
    HintTypeValueAccess = 2,
    HintTypeFrequentPath = 3,
    HintTypeCacheWarm = 4
} HINT_TYPE;

/* Hint Entry Structure */
typedef struct _HINT_ENTRY {
    HINT_TYPE Type;
    UINT32 CellOffset;
    CHAR Path[256];
    UINT32 AccessCount;
    UINT64 LastAccess;
    struct _HINT_ENTRY* Next;
} HINT_ENTRY, *PHINT_ENTRY;

/* Hint Cache Structure */
typedef struct _HINT_CACHE {
    PHINT_ENTRY Entries;
    UINT32 Count;
    UINT32 MaxEntries;
    AURORA_SPINLOCK Lock;
} HINT_CACHE, *PHINT_CACHE;

/* Status Codes */
#ifndef STATUS_SUCCESS
#define STATUS_SUCCESS                  0x00000000L
#endif
#ifndef STATUS_INVALID_PARAMETER
#define STATUS_INVALID_PARAMETER        0xC000000DL
#endif
#ifndef STATUS_NOT_INITIALIZED
#define STATUS_NOT_INITIALIZED          0xC0000142L
#endif
#ifndef STATUS_INVALID_STATE
#define STATUS_INVALID_STATE            0xC0000203L
#endif
#ifndef STATUS_KEY_ALREADY_EXISTS
#define STATUS_KEY_ALREADY_EXISTS       0xC0000035L
#endif
#ifndef STATUS_NOT_IMPLEMENTED
#define STATUS_NOT_IMPLEMENTED          0xC0000002L
#endif
#ifndef STATUS_INVALID_VALUE_TYPE
#define STATUS_INVALID_VALUE_TYPE       0xC0000004L
#endif
#ifndef STATUS_NOT_FOUND
#define STATUS_NOT_FOUND                0xC0000225L
#endif
#ifndef STATUS_INVALID_HIVE_SIGNATURE
#define STATUS_INVALID_HIVE_SIGNATURE   0xC0000226L
#endif
#ifndef STATUS_HIVE_SIZE_MISMATCH
#define STATUS_HIVE_SIZE_MISMATCH       0xC0000227L
#endif
#ifndef STATUS_HIVE_CHECKSUM_MISMATCH
#define STATUS_HIVE_CHECKSUM_MISMATCH 0xC0000243
#endif

#ifndef STATUS_INVALID_FILE_FORMAT
#define STATUS_INVALID_FILE_FORMAT 0xC0000244
#endif

#ifndef STATUS_INVALID_FILE_SIGNATURE
#define STATUS_INVALID_FILE_SIGNATURE 0xC0000245
#endif

#ifndef STATUS_FILE_CHECKSUM_MISMATCH
#define STATUS_FILE_CHECKSUM_MISMATCH 0xC0000246
#endif

#ifndef STATUS_FILE_NOT_FOUND
#define STATUS_FILE_NOT_FOUND 0xC0000247
#endif

#ifndef STATUS_INTERNAL_ERROR
#define STATUS_INTERNAL_ERROR 0xC0000248
#endif

#ifndef STATUS_TOO_MANY_VIEWS
#define STATUS_TOO_MANY_VIEWS 0xC0000249
#endif

#ifndef STATUS_INVALID_TRANSACTION_ID
#define STATUS_INVALID_TRANSACTION_ID 0xC000024A
#endif

#ifndef STATUS_INVALID_LOCK_STATE
#define STATUS_INVALID_LOCK_STATE 0xC000024B
#endif

/* Synchronization */
VOID HiveAcquireLock(IN PHIVE Hive);
VOID HiveReleaseLock(IN PHIVE Hive);

/* Hint Management */
NTSTATUS HiveUpdateHints(IN PHIVE Hive);
VOID HiveClearHints(IN PHIVE Hive);

/* Global Variables */
extern PHIVE g_SystemHive;
extern PHIVE g_SoftwareHive;
extern PHIVE g_SecurityHive;
extern PHIVE g_DefaultHive;

#endif /* _HIVE_H_ */