/*
 * Aurora Kernel - NTCore Configuration API
 * Copyright (c) 2024 NTCore Project
 */

#include "../hive/hive.h"

/* NTCore API constants */
#define NTCORE_API_VERSION 1
#define NTCORE_MAX_KEY_NAME 256
#define NTCORE_MAX_VALUE_NAME 256
#define NTCORE_MAX_VALUE_DATA 4096

/* Configuration key paths */
#define NTCORE_ROOT_KEY "NTCore"
#define NTCORE_SYSTEM_KEY "NTCore\\System"
#define NTCORE_KERNEL_KEY "NTCore\\System\\Kernel"
#define NTCORE_DRIVERS_KEY "NTCore\\System\\Drivers"
#define NTCORE_SERVICES_KEY "NTCore\\System\\Services"
#define NTCORE_HARDWARE_KEY "NTCore\\System\\Hardware"

/* Configuration value types */
typedef enum _NTCORE_VALUE_TYPE {
    NTCoreValueString = 1,
    NTCoreValueDword = 2,
    NTCoreValueQword = 3,
    NTCoreValueBinary = 4,
    NTCoreValueMultiString = 5
} NTCORE_VALUE_TYPE, *PNTCORE_VALUE_TYPE;

/* Configuration context */
typedef struct _NTCORE_CONFIG_CONTEXT {
    PHIVE SystemHive;
    UINT32 RootKeyOffset;
    BOOLEAN Initialized;
    AURORA_SPINLOCK Lock;
} NTCORE_CONFIG_CONTEXT, *PNTCORE_CONFIG_CONTEXT;

/* Global configuration context */
static NTCORE_CONFIG_CONTEXT g_ConfigContext = { NULL, 0, FALSE, 0 };

/* Forward declarations (internal) */
NTSTATUS NTCoreCreateRootKeys(void);

/*
 * Initialize NTCore configuration system
 */
NTSTATUS NTCoreInitializeConfig(void)
{
    if (g_ConfigContext.Initialized) {
        return STATUS_SUCCESS;
    }

    /* Initialize hive system first */
    NTSTATUS Status = HiveInitializeSystem();
    if (!NT_SUCCESS(Status)) {
        return Status;
    }
    
    /* Find or create system hive */
    PHIVE SystemHive = HiveFindByName("SYSTEM");
    if (!SystemHive) {
        /* Create new system hive */
        Status = HiveInitialize(&SystemHive, "SYSTEM", HiveTypeSystem);
        if (!NT_SUCCESS(Status)) {
            return Status;
        }
        
        Status = HiveCreate(SystemHive, 256 * 1024); /* 256KB initial size */
        if (!NT_SUCCESS(Status)) {
            HiveClose(SystemHive);
            return Status;
        }
        
        Status = HiveAddToList(SystemHive);
        if (!NT_SUCCESS(Status)) {
            HiveClose(SystemHive);
            return Status;
        }
    }
    
    g_ConfigContext.SystemHive = SystemHive;
    g_ConfigContext.Lock = 0;
    
    /* Create root NTCore key */
    Status = NTCoreCreateRootKeys();
    if (!NT_SUCCESS(Status)) {
        return Status;
    }
    
    g_ConfigContext.Initialized = TRUE;
    return STATUS_SUCCESS;
}

/*
 * Shutdown NTCore configuration system
 */
VOID NTCoreShutdownConfig(void)
{
    if (!g_ConfigContext.Initialized) {
        return;
    }

    /* Flush configuration changes */
    if (g_ConfigContext.SystemHive) {
        HiveFlush(g_ConfigContext.SystemHive);
    }
    
    /* Shutdown hive system */
    HiveShutdownSystem();
    
    g_ConfigContext.Initialized = FALSE;
    g_ConfigContext.SystemHive = NULL;
    g_ConfigContext.RootKeyOffset = 0;
}

/*
 * Create root configuration keys
 */
NTSTATUS NTCoreCreateRootKeys(void)
{
    if (!g_ConfigContext.SystemHive) {
        return STATUS_INVALID_STATE;
    }

    NTSTATUS Status;
    UINT32 KeyOffset;
    
    /* Create NTCore root key (full path) */
    Status = HiveCreateKey(g_ConfigContext.SystemHive, NTCORE_ROOT_KEY, &KeyOffset);
    if (!NT_SUCCESS(Status) && Status != STATUS_KEY_ALREADY_EXISTS) {
        return Status;
    }
    
    g_ConfigContext.RootKeyOffset = KeyOffset;
    
    /* Create system subkeys */
    const CHAR* SubKeys[] = {
        "System",
        "System\\Kernel",
        "System\\Drivers",
        "System\\Services",
        "System\\Hardware"
    };
    
    for (UINT32 i = 0; i < sizeof(SubKeys) / sizeof(SubKeys[0]); i++) {
        CHAR FullPath[NTCORE_MAX_KEY_NAME];
        /* Build path: "NTCore\\" + SubKeys[i] */
        /* Simple concat without relying on libc */
        UINT32 pos = 0;
        const CHAR prefix[] = NTCORE_ROOT_KEY "\\";
        for (UINT32 j = 0; j < sizeof(prefix) - 1 && pos < sizeof(FullPath); j++) {
            FullPath[pos++] = prefix[j];
        }
        const CHAR* sk = SubKeys[i];
        while (*sk && pos < sizeof(FullPath)) {
            FullPath[pos++] = *sk++;
        }
        if (pos >= sizeof(FullPath)) {
            FullPath[NTCORE_MAX_KEY_NAME - 1] = '\0';
        } else {
            FullPath[pos] = '\0';
        }

        Status = HiveCreateKey(g_ConfigContext.SystemHive, FullPath, &KeyOffset);
        if (!NT_SUCCESS(Status) && Status != STATUS_KEY_ALREADY_EXISTS) {
            return Status;
        }
    }
    
    return STATUS_SUCCESS;
}

/*
 * Open configuration key
 */
NTSTATUS NTCoreOpenKey(IN PCSTR KeyPath, OUT PUINT32 KeyHandle)
{
    if (!KeyPath || !KeyHandle) {
        return STATUS_INVALID_PARAMETER;
    }

    if (!g_ConfigContext.Initialized) {
        return STATUS_NOT_INITIALIZED;
    }
    
    /* Find key in hive */
    UINT32 KeyOffset;
    NTSTATUS Status = HiveFindKey(g_ConfigContext.SystemHive, KeyPath, &KeyOffset);
    if (!NT_SUCCESS(Status)) {
        return Status;
    }
    
    *KeyHandle = KeyOffset;
    return STATUS_SUCCESS;
}

/*
 * Create configuration key
 */
NTSTATUS NTCoreCreateKey(IN PCSTR KeyPath, OUT PUINT32 KeyHandle)
{
    if (!KeyPath || !KeyHandle) {
        return STATUS_INVALID_PARAMETER;
    }

    if (!g_ConfigContext.Initialized) {
        return STATUS_NOT_INITIALIZED;
    }
    
    /* Create key in hive */
    UINT32 KeyOffset;
    NTSTATUS Status = HiveCreateKey(g_ConfigContext.SystemHive, KeyPath, &KeyOffset);
    if (!NT_SUCCESS(Status)) {
        return Status;
    }
    
    *KeyHandle = KeyOffset;
    return STATUS_SUCCESS;
}

/*
 * Delete configuration key
 */
NTSTATUS NTCoreDeleteKey(IN PCSTR KeyPath)
{
    if (!KeyPath) {
        return STATUS_INVALID_PARAMETER;
    }

    if (!g_ConfigContext.Initialized) {
        return STATUS_NOT_INITIALIZED;
    }
    
    /* Delete key from hive */
    return HiveDeleteKey(g_ConfigContext.SystemHive, KeyPath);
}

/*
 * Set string value
 */
NTSTATUS NTCoreSetStringValue(IN UINT32 KeyHandle, IN PCSTR ValueName, IN PCSTR Value)
{
    if (!ValueName || !Value) {
        return STATUS_INVALID_PARAMETER;
    }

    if (!g_ConfigContext.Initialized) {
        return STATUS_NOT_INITIALIZED;
    }
    
    UINT32 ValueLength = strlen(Value) + 1; /* Include null terminator */
    return HiveSetValue(g_ConfigContext.SystemHive, KeyHandle, ValueName, 
                       ValueTypeString, (PVOID)Value, ValueLength);
}

/*
 * Get string value
 */
NTSTATUS NTCoreGetStringValue(IN UINT32 KeyHandle, IN PCSTR ValueName, OUT PCHAR Buffer, IN UINT32 BufferSize)
{
    if (!ValueName || !Buffer || BufferSize == 0) {
        return STATUS_INVALID_PARAMETER;
    }

    if (!g_ConfigContext.Initialized) {
        return STATUS_NOT_INITIALIZED;
    }
    
    UINT32 ValueType;
    UINT32 DataSize = BufferSize;
    
    NTSTATUS Status = HiveGetValue(g_ConfigContext.SystemHive, KeyHandle, ValueName, 
                                  &ValueType, Buffer, &DataSize);
    if (!NT_SUCCESS(Status)) {
        return Status;
    }
    
    if (ValueType != ValueTypeString) {
        return STATUS_INVALID_VALUE_TYPE;
    }
    
    /* Ensure null termination */
    if (DataSize > 0 && DataSize <= BufferSize) {
        Buffer[DataSize - 1] = '\0';
    }
    
    return STATUS_SUCCESS;
}

/*
 * Set DWORD value
 */
NTSTATUS NTCoreSetDwordValue(IN UINT32 KeyHandle, IN PCSTR ValueName, IN UINT32 Value)
{
    if (!ValueName) {
        return STATUS_INVALID_PARAMETER;
    }

    if (!g_ConfigContext.Initialized) {
        return STATUS_NOT_INITIALIZED;
    }
    
    return HiveSetValue(g_ConfigContext.SystemHive, KeyHandle, ValueName, 
                       ValueTypeDword, &Value, sizeof(UINT32));
}

/*
 * Get DWORD value
 */
NTSTATUS NTCoreGetDwordValue(IN UINT32 KeyHandle, IN PCSTR ValueName, OUT PUINT32 Value)
{
    if (!ValueName || !Value) {
        return STATUS_INVALID_PARAMETER;
    }

    if (!g_ConfigContext.Initialized) {
        return STATUS_NOT_INITIALIZED;
    }
    
    UINT32 ValueType;
    UINT32 DataSize = sizeof(UINT32);
    
    NTSTATUS Status = HiveGetValue(g_ConfigContext.SystemHive, KeyHandle, ValueName, 
                                  &ValueType, Value, &DataSize);
    if (!NT_SUCCESS(Status)) {
        return Status;
    }
    
    if (ValueType != ValueTypeDword || DataSize != sizeof(UINT32)) {
        return STATUS_INVALID_VALUE_TYPE;
    }
    
    return STATUS_SUCCESS;
}

/*
 * Set binary value
 */
NTSTATUS NTCoreSetBinaryValue(IN UINT32 KeyHandle, IN PCSTR ValueName, IN PVOID Data, IN UINT32 DataSize)
{
    if (!ValueName || !Data || DataSize == 0) {
        return STATUS_INVALID_PARAMETER;
    }

    if (!g_ConfigContext.Initialized) {
        return STATUS_NOT_INITIALIZED;
    }
    
    return HiveSetValue(g_ConfigContext.SystemHive, KeyHandle, ValueName, 
                       ValueTypeBinary, Data, DataSize);
}

/*
 * Get binary value
 */
NTSTATUS NTCoreGetBinaryValue(IN UINT32 KeyHandle, IN PCSTR ValueName, OUT PVOID Buffer, IN OUT PUINT32 BufferSize)
{
    if (!ValueName || !Buffer || !BufferSize || *BufferSize == 0) {
        return STATUS_INVALID_PARAMETER;
    }

    if (!g_ConfigContext.Initialized) {
        return STATUS_NOT_INITIALIZED;
    }
    
    UINT32 ValueType;
    
    NTSTATUS Status = HiveGetValue(g_ConfigContext.SystemHive, KeyHandle, ValueName, 
                                  &ValueType, Buffer, BufferSize);
    if (!NT_SUCCESS(Status)) {
        return Status;
    }
    
    if (ValueType != ValueTypeBinary) {
        return STATUS_INVALID_VALUE_TYPE;
    }
    
    return STATUS_SUCCESS;
}

/*
 * Delete value
 */
NTSTATUS NTCoreDeleteValue(IN UINT32 KeyHandle, IN PCSTR ValueName)
{
    if (!ValueName) {
        return STATUS_INVALID_PARAMETER;
    }

    if (!g_ConfigContext.Initialized) {
        return STATUS_NOT_INITIALIZED;
    }
    
    return HiveDeleteValue(g_ConfigContext.SystemHive, KeyHandle, ValueName);
}

/*
 * Enumerate subkeys
 */
NTSTATUS NTCoreEnumerateKeys(IN UINT32 KeyHandle, IN UINT32 Index, OUT PCHAR KeyName, IN UINT32 KeyNameSize)
{
    if (!KeyName || KeyNameSize == 0) {
        return STATUS_INVALID_PARAMETER;
    }

    if (!g_ConfigContext.Initialized) {
        return STATUS_NOT_INITIALIZED;
    }
    
    return HiveEnumerateKeys(g_ConfigContext.SystemHive, KeyHandle, Index, KeyName, &KeyNameSize);
}

/*
 * Enumerate values
 */
NTSTATUS NTCoreEnumerateValues(IN UINT32 KeyHandle, IN UINT32 Index, OUT PCHAR ValueName, IN UINT32 ValueNameSize,
                              OUT PUINT32 ValueType, OUT PUINT32 DataSize)
{
    if (!ValueName || ValueNameSize == 0) {
        return STATUS_INVALID_PARAMETER;
    }

    if (!g_ConfigContext.Initialized) {
        return STATUS_NOT_INITIALIZED;
    }
    
    return HiveEnumerateValues(g_ConfigContext.SystemHive, KeyHandle, Index, 
                              ValueName, &ValueNameSize);
}

/*
 * Flush configuration changes
 */
NTSTATUS NTCoreFlushConfig(void)
{
    if (!g_ConfigContext.Initialized || !g_ConfigContext.SystemHive) {
        return STATUS_NOT_INITIALIZED;
    }
    
    return HiveFlush(g_ConfigContext.SystemHive);
}

/*
 * Load configuration from file
 */
NTSTATUS NTCoreLoadConfig(IN PCSTR FileName)
{
    if (!FileName) {
        return STATUS_INVALID_PARAMETER;
    }

    /* This would load configuration from a file */
    /* For now, return not implemented */
    return STATUS_NOT_IMPLEMENTED;
}

/*
 * Save configuration to file
 */
NTSTATUS NTCoreSaveConfig(IN PCSTR FileName)
{
    if (!FileName) {
        return STATUS_INVALID_PARAMETER;
    }

    if (!g_ConfigContext.Initialized || !g_ConfigContext.SystemHive) {
        return STATUS_NOT_INITIALIZED;
    }
    
    /* Save hive to file */
    return HiveSaveToFile(g_ConfigContext.SystemHive, FileName);
}

/*
 * Get configuration statistics
 */
NTSTATUS NTCoreGetConfigStatistics(OUT PUINT32 KeyCount, OUT PUINT32 ValueCount, OUT PUINT32 TotalSize)
{
    if (!KeyCount || !ValueCount || !TotalSize) {
        return STATUS_INVALID_PARAMETER;
    }

    if (!g_ConfigContext.Initialized || !g_ConfigContext.SystemHive) {
        return STATUS_NOT_INITIALIZED;
    }
    
    HIVE_STATISTICS Statistics;
    NTSTATUS Status = HiveGetStatistics(g_ConfigContext.SystemHive, &Statistics);
    if (!NT_SUCCESS(Status)) {
        return Status;
    }
    
    *KeyCount = Statistics.KeyCells;
    *ValueCount = Statistics.ValueCells;
    *TotalSize = Statistics.TotalSize;
    
    return STATUS_SUCCESS;
}

/*
 * Backup configuration
 */
NTSTATUS NTCoreBackupConfig(IN PCSTR BackupPath)
{
    if (!BackupPath) {
        return STATUS_INVALID_PARAMETER;
    }

    if (!g_ConfigContext.Initialized || !g_ConfigContext.SystemHive) {
        return STATUS_NOT_INITIALIZED;
    }
    
    return HiveCreateBackup(g_ConfigContext.SystemHive, BackupPath);
}

/*
 * Restore configuration from backup
 */
NTSTATUS NTCoreRestoreConfig(IN PCSTR BackupPath)
{
    if (!BackupPath) {
        return STATUS_INVALID_PARAMETER;
    }

    /* This would restore configuration from backup */
    /* For now, return not implemented */
    return STATUS_NOT_IMPLEMENTED;
}

/*
 * Validate configuration integrity
 */
NTSTATUS NTCoreValidateConfig(OUT PBOOLEAN IsValid)
{
    if (!IsValid) {
        return STATUS_INVALID_PARAMETER;
    }

    if (!g_ConfigContext.Initialized || !g_ConfigContext.SystemHive) {
        return STATUS_NOT_INITIALIZED;
    }
    
    return HiveCheckIntegrity(g_ConfigContext.SystemHive, 
                             g_ConfigContext.SystemHive->BaseAddress, 
                             g_ConfigContext.SystemHive->Size);
}

/*
 * Compact configuration database
 */
NTSTATUS NTCoreCompactConfig(void)
{
    if (!g_ConfigContext.Initialized || !g_ConfigContext.SystemHive) {
        return STATUS_NOT_INITIALIZED;
    }
    
    return HiveCompact(g_ConfigContext.SystemHive);
}

/*
 * Get API version
 */
UINT32 NTCoreGetApiVersion(void)
{
    return NTCORE_API_VERSION;
}

/*
 * Check if configuration system is initialized
 */
BOOLEAN NTCoreIsConfigInitialized(void)
{
    return g_ConfigContext.Initialized;
}

/*
 * Set configuration value with type checking
 */
NTSTATUS NTCoreSetValue(IN UINT32 KeyHandle, IN PCSTR ValueName, IN NTCORE_VALUE_TYPE Type, 
                       IN PVOID Data, IN UINT32 DataSize)
{
    if (!ValueName || !Data || DataSize == 0) {
        return STATUS_INVALID_PARAMETER;
    }

    if (!g_ConfigContext.Initialized) {
        return STATUS_NOT_INITIALIZED;
    }
    
    /* Convert NTCore type to hive type */
    VALUE_TYPE HiveType;
    switch (Type) {
        case NTCoreValueString:
            HiveType = ValueTypeString;
            break;
        case NTCoreValueDword:
            HiveType = ValueTypeDword;
            break;
        case NTCoreValueQword:
            HiveType = ValueTypeQword;
            break;
        case NTCoreValueBinary:
            HiveType = ValueTypeBinary;
            break;
        case NTCoreValueMultiString:
            HiveType = ValueTypeMultiString;
            break;
        default:
            return STATUS_INVALID_PARAMETER;
    }
    
    return HiveSetValue(g_ConfigContext.SystemHive, KeyHandle, ValueName, HiveType, Data, DataSize);
}

/*
 * Get configuration value with type information
 */
NTSTATUS NTCoreGetValue(IN UINT32 KeyHandle, IN PCSTR ValueName, OUT PNTCORE_VALUE_TYPE Type, 
                       OUT PVOID Buffer, IN OUT PUINT32 BufferSize)
{
    if (!ValueName || !Type || !Buffer || !BufferSize || *BufferSize == 0) {
        return STATUS_INVALID_PARAMETER;
    }

    if (!g_ConfigContext.Initialized) {
        return STATUS_NOT_INITIALIZED;
    }
    
    UINT32 HiveType;
    NTSTATUS Status = HiveGetValue(g_ConfigContext.SystemHive, KeyHandle, ValueName, 
                                  &HiveType, Buffer, BufferSize);
    if (!NT_SUCCESS(Status)) {
        return Status;
    }
    
    /* Convert hive type to NTCore type */
    switch (HiveType) {
        case ValueTypeString:
            *Type = NTCoreValueString;
            break;
        case ValueTypeDword:
            *Type = NTCoreValueDword;
            break;
        case ValueTypeQword:
            *Type = NTCoreValueQword;
            break;
        case ValueTypeBinary:
            *Type = NTCoreValueBinary;
            break;
        case ValueTypeMultiString:
            *Type = NTCoreValueMultiString;
            break;
        default:
            return STATUS_INVALID_VALUE_TYPE;
    }
    
    return STATUS_SUCCESS;
}