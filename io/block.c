/* Minimal Block Layer Abstraction */
#include "../aurora.h"
#include "../include/io.h"

typedef struct _BLOCK_DEVICE_EXTENSION {
    UINT32 BlockSize;
    UINT64 BlockCount;
    UINT32 BlockType; /* IO_BLOCK_TYPE_* */
} BLOCK_DEVICE_EXTENSION, *PBLOCK_DEVICE_EXTENSION;

/* Submit routine prototype for block devices (sync, PIO for now) */
typedef NTSTATUS (*PBLOCK_RW)(PAIO_DEVICE_OBJECT Dev, UINT64 Lba, UINT32 Count, PVOID Buffer, BOOL Write);

static PBLOCK_RW g_BlockRwHandlers[IO_BLOCK_TYPE_VIRTIO_BLK+1];

NTSTATUS BlockRegisterRwHandler(UINT32 BlockType, PBLOCK_RW Fn){
    if(BlockType==0 || BlockType>IO_BLOCK_TYPE_VIRTIO_BLK) return STATUS_INVALID_PARAMETER;
    g_BlockRwHandlers[BlockType] = Fn; return STATUS_SUCCESS;
}

NTSTATUS BlockRead(PAIO_DEVICE_OBJECT Dev, UINT64 Lba, UINT32 Count, PVOID Buffer){
    PBLOCK_DEVICE_EXTENSION ext;
    if(!Dev || !Buffer) return STATUS_INVALID_PARAMETER;
    ext = (PBLOCK_DEVICE_EXTENSION)Dev->DeviceExtension;
    if(!ext || ext->BlockType==0) return STATUS_INVALID_PARAMETER;
    if(!g_BlockRwHandlers[ext->BlockType]) return STATUS_NOT_IMPLEMENTED;
    return g_BlockRwHandlers[ext->BlockType](Dev,Lba,Count,Buffer,FALSE);
}

NTSTATUS BlockWrite(PAIO_DEVICE_OBJECT Dev, UINT64 Lba, UINT32 Count, PVOID Buffer){
    PBLOCK_DEVICE_EXTENSION ext;
    if(!Dev || !Buffer) return STATUS_INVALID_PARAMETER;
    ext = (PBLOCK_DEVICE_EXTENSION)Dev->DeviceExtension;
    if(!ext || ext->BlockType==0) return STATUS_INVALID_PARAMETER;
    if(!g_BlockRwHandlers[ext->BlockType]) return STATUS_NOT_IMPLEMENTED;
    return g_BlockRwHandlers[ext->BlockType](Dev,Lba,Count,Buffer,TRUE);
}

/* ATA PIO skeleton */
/* PORT IO helpers (to be supplied by arch HAL) */
extern UINT8 HalInByte(UINT16 Port);
extern void HalOutByte(UINT16 Port, UINT8 Value);
static inline void HalIoDelay(void){ for(volatile int i=0;i<1000;i++); }

#define ATA_PRIMARY_IO  0x1F0
#define ATA_PRIMARY_CTRL 0x3F6

/* Fallback status codes if not defined */
#ifndef STATUS_DEVICE_NOT_CONNECTED
#define STATUS_DEVICE_NOT_CONNECTED STATUS_NOT_FOUND
#endif
#ifndef STATUS_IO_DEVICE_ERROR
#define STATUS_IO_DEVICE_ERROR STATUS_UNSUCCESSFUL
#endif

static NTSTATUS AtaIdentify(UINT16 ioBase, UINT16 ctrlBase, UINT8 drive, UINT16* identifyBuf){
    /* Very primitive: select drive and issue IDENTIFY */
    HalOutByte(ioBase + 6, (drive?0xB0:0xA0));
    HalIoDelay();
    HalOutByte(ioBase + 2, 0); /* sector count */
    HalOutByte(ioBase + 3, 0); /* LBA low */
    HalOutByte(ioBase + 4, 0); /* LBA mid */
    HalOutByte(ioBase + 5, 0); /* LBA hi */
    HalOutByte(ioBase + 7, 0xEC); /* IDENTIFY */
    if(HalInByte(ioBase + 7)==0){ return STATUS_DEVICE_NOT_CONNECTED; }
    /* poll DRQ */
    UINT8 status; int timeout=100000;
    while(((status=HalInByte(ioBase+7)) & 0x08)==0){ if(status & 0x01) return STATUS_IO_DEVICE_ERROR; if(--timeout==0) return STATUS_TIMEOUT; }
    for(int i=0;i<256;i++){ ((UINT16*)identifyBuf)[i] = *((volatile UINT16*)(uintptr_t)(ioBase)); }
    return STATUS_SUCCESS;
}

static NTSTATUS AtaRwHandler(PAIO_DEVICE_OBJECT Dev, UINT64 Lba, UINT32 Count, PVOID Buffer, BOOL Write){
    (void)Dev;(void)Lba;(void)Count;(void)Buffer;(void)Write; return STATUS_NOT_IMPLEMENTED; }

static NTSTATUS AtaProbe(void){
    /* allocate identify buffer (on stack for now) */
    UINT16 identify[256];
    if(!NT_SUCCESS(AtaIdentify(ATA_PRIMARY_IO,ATA_PRIMARY_CTRL,0,identify))) return STATUS_NOT_FOUND;
    /* Create device */
    extern NTSTATUS IoCreateDevice(IN PAIO_DRIVER_OBJECT Driver, IN PCHAR Name, IN UINT32 Type, OUT PAIO_DEVICE_OBJECT* DeviceOut);
    extern NTSTATUS IoRegisterStorageDriver(void);
    static AIO_DRIVER_OBJECT storageRef; static int registered=0;
    if(!registered){ IoRegisterStorageDriver(); /* storageRef unused placeholder */ registered=1; }
    PAIO_DEVICE_OBJECT dev; IoCreateDevice(&storageRef,"ata0",IO_DEVICE_CLASS_BLOCK,&dev);
    PBLOCK_DEVICE_EXTENSION ext = (PBLOCK_DEVICE_EXTENSION)AuroraAllocateMemory(sizeof(BLOCK_DEVICE_EXTENSION));
    if(ext){ memset(ext,0,sizeof(*ext)); ext->BlockSize=512; ext->BlockCount=0; ext->BlockType=IO_BLOCK_TYPE_ATA; dev->DeviceExtension=ext; }
    BlockRegisterRwHandler(IO_BLOCK_TYPE_ATA,AtaRwHandler);
    AuroraDebugPrint("[ata] primary master present (stub)");
    return STATUS_SUCCESS;
}

/* NVMe skeleton (PCI scan placeholder) */
static NTSTATUS NvmeScan(void){
    /* Future: iterate PCI config space for class 01 08 02 */
    return STATUS_NOT_IMPLEMENTED;
}

/* Entry called after IO initialized */
NTSTATUS BlockSubsystemInitialize(void){
    AtaProbe();
    NvmeScan();
    return STATUS_SUCCESS;
}
