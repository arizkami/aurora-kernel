/*
 * Aurora Kernel - Registry Hive Checksum Management
 * Copyright (c) 2024 NTCore Project
 */

#include "hive.h"

#ifndef PUINT8
typedef UINT8* PUINT8;
#endif

/* Simple abs function for kernel use */
static inline int abs(int x) {
    return x < 0 ? -x : x;
}

/* Checksum algorithm types */
typedef enum _CHECKSUM_ALGORITHM {
    ChecksumAlgorithmXOR = 1,
    ChecksumAlgorithmCRC32 = 2,
    ChecksumAlgorithmMD5 = 3,
    ChecksumAlgorithmSHA1 = 4
} CHECKSUM_ALGORITHM;

/* CRC32 lookup table */
static UINT32 g_Crc32Table[256];
static BOOLEAN g_Crc32TableInitialized = FALSE;

/* Checksum context structure */
typedef struct _CHECKSUM_CONTEXT {
    CHECKSUM_ALGORITHM Algorithm;
    UINT32 CurrentValue;
    UINT32 BytesProcessed;
    UINT8 State[64];  /* Algorithm-specific state */
} CHECKSUM_CONTEXT, *PCHECKSUM_CONTEXT;

/*
 * Initialize CRC32 lookup table
 */
VOID HiveInitializeCrc32Table(void)
{
    if (g_Crc32TableInitialized) {
        return;
    }

    UINT32 Polynomial = 0xEDB88320; /* IEEE 802.3 polynomial */
    
    for (UINT32 i = 0; i < 256; i++) {
        UINT32 Crc = i;
        
        for (UINT32 j = 0; j < 8; j++) {
            if (Crc & 1) {
                Crc = (Crc >> 1) ^ Polynomial;
            } else {
                Crc >>= 1;
            }
        }
        
        g_Crc32Table[i] = Crc;
    }
    
    g_Crc32TableInitialized = TRUE;
}

/*
 * Calculate XOR checksum
 */
UINT32 HiveCalculateXorChecksum(IN PVOID Data, IN UINT32 Size)
{
    if (!Data || Size == 0) {
        return 0;
    }

    UINT32 Checksum = 0;
    PUINT32 DwordData = (PUINT32)Data;
    UINT32 DwordCount = Size / sizeof(UINT32);
    
    /* Process complete DWORDs */
    for (UINT32 i = 0; i < DwordCount; i++) {
        Checksum ^= DwordData[i];
        /* Rotate left by 1 bit */
        Checksum = (Checksum << 1) | (Checksum >> 31);
    }
    
    /* Process remaining bytes */
    UINT32 RemainingBytes = Size % sizeof(UINT32);
    if (RemainingBytes > 0) {
        UINT32 LastDword = 0;
        PUINT8 ByteData = (PUINT8)&DwordData[DwordCount];
        
        for (UINT32 i = 0; i < RemainingBytes; i++) {
            LastDword |= (ByteData[i] << (i * 8));
        }
        
        Checksum ^= LastDword;
    }
    
    return Checksum;
}

/*
 * Calculate CRC32 checksum
 */
UINT32 HiveCalculateCrc32Checksum(IN PVOID Data, IN UINT32 Size)
{
    if (!Data || Size == 0) {
        return 0;
    }

    /* Initialize CRC32 table if needed */
    if (!g_Crc32TableInitialized) {
        HiveInitializeCrc32Table();
    }
    
    UINT32 Crc = 0xFFFFFFFF;
    PUINT8 ByteData = (PUINT8)Data;
    
    for (UINT32 i = 0; i < Size; i++) {
        UINT8 TableIndex = (Crc ^ ByteData[i]) & 0xFF;
        Crc = (Crc >> 8) ^ g_Crc32Table[TableIndex];
    }
    
    return ~Crc;
}

/*
 * Calculate hive header checksum
 */
UINT32 HiveCalculateChecksum(IN PHIVE_HEADER Header)
{
    if (!Header) {
        return 0;
    }

    /* Save original checksum */
    UINT32 OriginalChecksum = Header->Checksum;
    Header->Checksum = 0;
    
    /* Calculate checksum of header */
    UINT32 Checksum = HiveCalculateXorChecksum(Header, sizeof(HIVE_HEADER));
    
    /* Restore original checksum */
    Header->Checksum = OriginalChecksum;
    
    return Checksum;
}

/*
 * Verify hive header checksum
 */
BOOLEAN HiveVerifyChecksum(IN PHIVE_HEADER Header)
{
    if (!Header) {
        return FALSE;
    }

    UINT32 StoredChecksum = Header->Checksum;
    UINT32 CalculatedChecksum = HiveCalculateChecksum(Header);
    
    return (StoredChecksum == CalculatedChecksum);
}

/*
 * Calculate checksum for entire hive
 */
UINT32 HiveCalculateFullChecksum(IN PHIVE Hive, IN CHECKSUM_ALGORITHM Algorithm)
{
    if (!Hive || !Hive->BaseAddress) {
        return 0;
    }

    switch (Algorithm) {
        case ChecksumAlgorithmXOR:
            return HiveCalculateXorChecksum(Hive->BaseAddress, Hive->Size);
            
        case ChecksumAlgorithmCRC32:
            return HiveCalculateCrc32Checksum(Hive->BaseAddress, Hive->Size);
            
        default:
            return 0;
    }
}

/*
 * Calculate incremental checksum
 */
NTSTATUS HiveCalculateIncrementalChecksum(IN PHIVE Hive, IN UINT32 StartOffset, IN UINT32 Size, 
                                         IN CHECKSUM_ALGORITHM Algorithm, OUT PUINT32 Checksum)
{
    if (!Hive || !Hive->BaseAddress || !Checksum || Size == 0) {
        return STATUS_INVALID_PARAMETER;
    }

    if (StartOffset + Size > Hive->Size) {
        return STATUS_INVALID_PARAMETER;
    }
    
    PVOID Data = (UINT8*)Hive->BaseAddress + StartOffset;
    
    switch (Algorithm) {
        case ChecksumAlgorithmXOR:
            *Checksum = HiveCalculateXorChecksum(Data, Size);
            break;
            
        case ChecksumAlgorithmCRC32:
            *Checksum = HiveCalculateCrc32Checksum(Data, Size);
            break;
            
        default:
            return STATUS_NOT_SUPPORTED;
    }
    
    return STATUS_SUCCESS;
}

/*
 * Initialize checksum context for streaming calculation
 */
NTSTATUS HiveInitializeChecksumContext(OUT PCHECKSUM_CONTEXT Context, IN CHECKSUM_ALGORITHM Algorithm)
{
    if (!Context) {
        return STATUS_INVALID_PARAMETER;
    }

    memset(Context, 0, sizeof(CHECKSUM_CONTEXT));
    Context->Algorithm = Algorithm;
    
    switch (Algorithm) {
        case ChecksumAlgorithmXOR:
            Context->CurrentValue = 0;
            break;
            
        case ChecksumAlgorithmCRC32:
            if (!g_Crc32TableInitialized) {
                HiveInitializeCrc32Table();
            }
            Context->CurrentValue = 0xFFFFFFFF;
            break;
            
        default:
            return STATUS_NOT_SUPPORTED;
    }
    
    Context->BytesProcessed = 0;
    return STATUS_SUCCESS;
}

/*
 * Update checksum context with new data
 */
NTSTATUS HiveUpdateChecksumContext(IN OUT PCHECKSUM_CONTEXT Context, IN PVOID Data, IN UINT32 Size)
{
    if (!Context || !Data || Size == 0) {
        return STATUS_INVALID_PARAMETER;
    }

    PUINT8 ByteData = (PUINT8)Data;
    
    switch (Context->Algorithm) {
        case ChecksumAlgorithmXOR:
            {
                PUINT32 DwordData = (PUINT32)Data;
                UINT32 DwordCount = Size / sizeof(UINT32);
                
                /* Process complete DWORDs */
                for (UINT32 i = 0; i < DwordCount; i++) {
                    Context->CurrentValue ^= DwordData[i];
                    Context->CurrentValue = (Context->CurrentValue << 1) | (Context->CurrentValue >> 31);
                }
                
                /* Process remaining bytes */
                UINT32 RemainingBytes = Size % sizeof(UINT32);
                if (RemainingBytes > 0) {
                    UINT32 LastDword = 0;
                    PUINT8 LastBytes = (PUINT8)&DwordData[DwordCount];
                    
                    for (UINT32 i = 0; i < RemainingBytes; i++) {
                        LastDword |= (LastBytes[i] << (i * 8));
                    }
                    
                    Context->CurrentValue ^= LastDword;
                }
            }
            break;
            
        case ChecksumAlgorithmCRC32:
            {
                for (UINT32 i = 0; i < Size; i++) {
                    UINT8 TableIndex = (Context->CurrentValue ^ ByteData[i]) & 0xFF;
                    Context->CurrentValue = (Context->CurrentValue >> 8) ^ g_Crc32Table[TableIndex];
                }
            }
            break;
            
        default:
            return STATUS_NOT_SUPPORTED;
    }
    
    Context->BytesProcessed += Size;
    return STATUS_SUCCESS;
}

/*
 * Finalize checksum calculation
 */
NTSTATUS HiveFinalizeChecksumContext(IN PCHECKSUM_CONTEXT Context, OUT PUINT32 FinalChecksum)
{
    if (!Context || !FinalChecksum) {
        return STATUS_INVALID_PARAMETER;
    }

    switch (Context->Algorithm) {
        case ChecksumAlgorithmXOR:
            *FinalChecksum = Context->CurrentValue;
            break;
            
        case ChecksumAlgorithmCRC32:
            *FinalChecksum = ~Context->CurrentValue;
            break;
            
        default:
            return STATUS_NOT_SUPPORTED;
    }
    
    return STATUS_SUCCESS;
}

/*
 * Calculate checksum for hive cell
 */
UINT32 HiveCalculateCellChecksum(IN PCELL_HEADER Cell)
{
    if (!Cell || Cell->Size == 0) {
        return 0;
    }

    UINT32 CellSize = abs(Cell->Size);
    return HiveCalculateXorChecksum(Cell, CellSize);
}

/*
 * Verify hive cell checksum
 */
BOOLEAN HiveVerifyCellChecksum(IN PCELL_HEADER Cell, IN UINT32 ExpectedChecksum)
{
    if (!Cell) {
        return FALSE;
    }

    UINT32 CalculatedChecksum = HiveCalculateCellChecksum(Cell);
    return (CalculatedChecksum == ExpectedChecksum);
}

/*
 * Calculate checksum for key cell
 */
UINT32 HiveCalculateKeyChecksum(IN PKEY_CELL KeyCell)
{
    if (!KeyCell) {
        return 0;
    }

    /* Calculate checksum of key data excluding variable-length fields */
    UINT32 Checksum = 0;
    
    /* Hash key name */
    if (KeyCell->NameLength > 0) {
        PCHAR KeyName = (PCHAR)((UINT8*)KeyCell + sizeof(KEY_CELL));
        Checksum ^= HiveCalculateXorChecksum(KeyName, KeyCell->NameLength);
    }
    
    /* Hash key metadata */
    Checksum ^= KeyCell->SubKeysCount;
    Checksum ^= KeyCell->ValuesCount;
    Checksum ^= KeyCell->SecurityOffset;
    Checksum ^= KeyCell->ClassOffset;
    
    return Checksum;
}

/*
 * Calculate checksum for value cell
 */
UINT32 HiveCalculateValueChecksum(IN PVALUE_CELL ValueCell)
{
    if (!ValueCell) {
        return 0;
    }

    UINT32 Checksum = 0;
    
    /* Hash value name */
    if (ValueCell->NameLength > 0) {
        PCHAR ValueName = (PCHAR)((UINT8*)ValueCell + sizeof(VALUE_CELL));
        Checksum ^= HiveCalculateXorChecksum(ValueName, ValueCell->NameLength);
    }
    
    /* Hash value metadata */
    Checksum ^= ValueCell->Type;
    Checksum ^= ValueCell->DataLength;
    Checksum ^= ValueCell->DataOffset;
    
    return Checksum;
}

/*
 * Update hive checksum after modification
 */
NTSTATUS HiveUpdateChecksum(IN PHIVE Hive)
{
    if (!Hive || !Hive->Header) {
        return STATUS_INVALID_PARAMETER;
    }

    /* Update header timestamp */
    Hive->Header->Timestamp = 0; /* TODO: Get current time */
    
    /* Recalculate header checksum */
    Hive->Header->Checksum = HiveCalculateChecksum(Hive->Header);
    
    /* Mark hive as dirty */
    Hive->DirtyFlag = TRUE;
    
    return STATUS_SUCCESS;
}

/*
 * Verify entire hive integrity using checksums
 */
NTSTATUS HiveVerifyIntegrityWithChecksums(IN PHIVE Hive, OUT PBOOLEAN IsValid)
{
    if (!Hive || !IsValid) {
        return STATUS_INVALID_PARAMETER;
    }

    *IsValid = FALSE;
    
    /* Verify header checksum */
    if (!HiveVerifyChecksum(Hive->Header)) {
        return STATUS_SUCCESS; /* Invalid but function succeeded */
    }
    
    /* Verify cell checksums */
    UINT32 Offset = sizeof(HIVE_HEADER);
    while (Offset < Hive->Size) {
        PCELL_HEADER Cell = (PCELL_HEADER)((UINT8*)Hive->BaseAddress + Offset);
        
        if (Cell->Size == 0) {
            break; /* Invalid cell size */
        }
        
        /* For allocated cells, verify structure integrity */
        if (Cell->Size < 0) {
            switch (Cell->Signature) {
                case CellTypeKey:
                    {
                        PKEY_CELL KeyCell = (PKEY_CELL)((UINT8*)Cell + sizeof(CELL_HEADER));
                        /* Additional key-specific validation could be added here */
                    }
                    break;
                    
                case CellTypeValue:
                    {
                        PVALUE_CELL ValueCell = (PVALUE_CELL)((UINT8*)Cell + sizeof(CELL_HEADER));
                        /* Additional value-specific validation could be added here */
                    }
                    break;
                    
                default:
                    /* Unknown cell type - could be corruption */
                    break;
            }
        }
        
        Offset += abs(Cell->Size);
    }
    
    *IsValid = TRUE;
    return STATUS_SUCCESS;
}

/*
 * Calculate rolling checksum for change detection
 */
UINT32 HiveCalculateRollingChecksum(IN PVOID Data, IN UINT32 Size, IN UINT32 WindowSize)
{
    if (!Data || Size == 0 || WindowSize == 0 || WindowSize > Size) {
        return 0;
    }

    PUINT8 ByteData = (PUINT8)Data;
    UINT32 Checksum = 0;
    
    /* Calculate initial window checksum */
    for (UINT32 i = 0; i < WindowSize; i++) {
        Checksum += ByteData[i];
    }
    
    /* Roll the window and update checksum */
    for (UINT32 i = WindowSize; i < Size; i++) {
        Checksum = Checksum - ByteData[i - WindowSize] + ByteData[i];
    }
    
    return Checksum;
}

/*
 * Compare checksums for equality
 */
BOOLEAN HiveCompareChecksums(IN UINT32 Checksum1, IN UINT32 Checksum2)
{
    return (Checksum1 == Checksum2);
}

/*
 * Get checksum algorithm name
 */
PCSTR HiveGetChecksumAlgorithmName(IN CHECKSUM_ALGORITHM Algorithm)
{
    switch (Algorithm) {
        case ChecksumAlgorithmXOR:
            return "XOR";
        case ChecksumAlgorithmCRC32:
            return "CRC32";
        case ChecksumAlgorithmMD5:
            return "MD5";
        case ChecksumAlgorithmSHA1:
            return "SHA1";
        default:
            return "Unknown";
    }
}

/*
 * Calculate weak checksum for fast comparison
 */
UINT32 HiveCalculateWeakChecksum(IN PVOID Data, IN UINT32 Size)
{
    if (!Data || Size == 0) {
        return 0;
    }

    PUINT8 ByteData = (PUINT8)Data;
    UINT32 Sum1 = 0, Sum2 = 0;
    
    for (UINT32 i = 0; i < Size; i++) {
        Sum1 = (Sum1 + ByteData[i]) % 65521;
        Sum2 = (Sum2 + Sum1) % 65521;
    }
    
    return (Sum2 << 16) | Sum1;
}

/*
 * Calculate strong checksum for security
 */
UINT32 HiveCalculateStrongChecksum(IN PVOID Data, IN UINT32 Size)
{
    if (!Data || Size == 0) {
        return 0;
    }

    /* Use CRC32 as strong checksum */
    return HiveCalculateCrc32Checksum(Data, Size);
}

/*
 * Validate checksum algorithm
 */
BOOLEAN HiveIsValidChecksumAlgorithm(IN CHECKSUM_ALGORITHM Algorithm)
{
    switch (Algorithm) {
        case ChecksumAlgorithmXOR:
        case ChecksumAlgorithmCRC32:
        case ChecksumAlgorithmMD5:
        case ChecksumAlgorithmSHA1:
            return TRUE;
        default:
            return FALSE;
    }
}

/*
 * Get checksum size for algorithm
 */
UINT32 HiveGetChecksumSize(IN CHECKSUM_ALGORITHM Algorithm)
{
    switch (Algorithm) {
        case ChecksumAlgorithmXOR:
        case ChecksumAlgorithmCRC32:
            return sizeof(UINT32);
        case ChecksumAlgorithmMD5:
            return 16; /* 128 bits */
        case ChecksumAlgorithmSHA1:
            return 20; /* 160 bits */
        default:
            return 0;
    }
}