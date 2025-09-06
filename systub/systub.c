/*
 * Aurora Kernel - System Stub / Example Character Device Driver ("systub")
 * ---------------------------------------------------------------------------
 * This module demonstrates how to integrate a simple character / control
 * oriented device into the I/O manager dispatch model. It provides a single
 * device object named "systub" that supports basic IRPs:
 *   - Create / Close : bookkeeping only
 *   - Read           : returns a rotating diagnostic line
 *   - Write          : accepts text commands ("PING", "ECHO <text>")
 *   - DeviceControl  : placeholder for future IOCTL style operations
 *
 * Goals:
 *   - Provide a heavily commented template ( ~400 lines ) that can be cloned
 *     when authoring new device category drivers.
 *   - Show minimal synchronization over a small ring buffer.
 *   - Illustrate canonical dispatch function signature and status returns.
 *
 * Non‑Goals:
 *   - Advanced buffering, overlapped I/O, or asynchronous completion.
 *   - Capability / security integration (future layer will restrict access).
 */

#include "../aurora.h"
#include "../include/io.h"

/* Ring buffer for writes (accumulated diagnostic text) */
#define SYSTUB_RING_SIZE 2048
typedef struct _SYSTUB_CONTEXT {
    AURORA_SPINLOCK Lock;
    CHAR Buffer[SYSTUB_RING_SIZE];
    UINT32 Head; /* insertion */
    UINT32 Tail; /* oldest */
    UINT64 OpenCount;
    UINT64 Sequence; /* increments per read to produce varying output */
} SYSTUB_CONTEXT, *PSYSTUB_CONTEXT;

static AIO_DRIVER_OBJECT g_SysStubDriver;
static PAIO_DEVICE_OBJECT g_SysStubDevice = NULL;
static SYSTUB_CONTEXT g_SysStubCtx;

/* Forward dispatchers */
static NTSTATUS SysStubCreate(IN struct _AIO_DEVICE_OBJECT* Device, IN PAIO_IRP Irp);
static NTSTATUS SysStubClose (IN struct _AIO_DEVICE_OBJECT* Device, IN PAIO_IRP Irp);
static NTSTATUS SysStubRead  (IN struct _AIO_DEVICE_OBJECT* Device, IN PAIO_IRP Irp);
static NTSTATUS SysStubWrite (IN struct _AIO_DEVICE_OBJECT* Device, IN PAIO_IRP Irp);
static NTSTATUS SysStubDeviceControl(IN struct _AIO_DEVICE_OBJECT* Device, IN PAIO_IRP Irp);

/* Utility: push text into ring */
static void SysStubRingWrite(IN const CHAR* Text){
    if(!Text) return;
    AURORA_IRQL old;
    AuroraAcquireSpinLock(&g_SysStubCtx.Lock, &old);
    while(*Text){
        g_SysStubCtx.Buffer[g_SysStubCtx.Head] = *Text++;
        g_SysStubCtx.Head = (g_SysStubCtx.Head + 1) % SYSTUB_RING_SIZE;
        if(g_SysStubCtx.Head == g_SysStubCtx.Tail){ /* overwrite oldest */
            g_SysStubCtx.Tail = (g_SysStubCtx.Tail + 1) % SYSTUB_RING_SIZE;
        }
    }
    AuroraReleaseSpinLock(&g_SysStubCtx.Lock, old);
}

/* Utility: emit diagnostic line for reads */
static UINT32 SysStubFormatDec(CHAR* dst, UINT64 v){ CHAR tmp[32]; UINT32 i=0; if(v==0){ dst[0]='0'; return 1; }
    while(v && i<sizeof(tmp)){ tmp[i++] = '0'+(v%10); v/=10; } UINT32 w=0; while(i){ dst[w++]=tmp[--i]; } return w; }
static UINT32 SysStubFormatLine(OUT CHAR* Buf, IN UINT32 Cap){
    if(Cap < 32) return 0; UINT32 pos=0; const CHAR* prefix="[systub] seq="; for(const CHAR* p=prefix; *p && pos<Cap-1; ++p) Buf[pos++]=*p;
    UINT64 seq = ++g_SysStubCtx.Sequence; pos += SysStubFormatDec(Buf+pos, seq);
    const CHAR* mid=" open="; for(const CHAR* p=mid; *p && pos<Cap-1; ++p) Buf[pos++]=*p;
    pos += SysStubFormatDec(Buf+pos, g_SysStubCtx.OpenCount);
    if(pos<Cap-1) Buf[pos++]='\n'; Buf[(pos<Cap)?pos:Cap-1]='\0'; return pos; }

static NTSTATUS SysStubCreate(IN struct _AIO_DEVICE_OBJECT* Device, IN PAIO_IRP Irp){
    (void)Device;
    g_SysStubCtx.OpenCount++;
    SysStubRingWrite("OPEN\n");
    Irp->Information = 0;
    return STATUS_SUCCESS;
}
static NTSTATUS SysStubClose (IN struct _AIO_DEVICE_OBJECT* Device, IN PAIO_IRP Irp){
    (void)Device; (void)Irp;
    SysStubRingWrite("CLOSE\n");
    return STATUS_SUCCESS;
}

static NTSTATUS SysStubRead  (IN struct _AIO_DEVICE_OBJECT* Device, IN PAIO_IRP Irp){
    (void)Device;
    if(!Irp->Buffer || Irp->Length == 0) return STATUS_INVALID_PARAMETER;
    CHAR line[96];
    UINT32 len = SysStubFormatLine(line, sizeof(line));
    if(len > Irp->Length) len = Irp->Length;
    memcpy(Irp->Buffer, line, len);
    Irp->Information = len;
    return STATUS_SUCCESS;
}

static NTSTATUS SysStubWrite (IN struct _AIO_DEVICE_OBJECT* Device, IN PAIO_IRP Irp){
    (void)Device;
    if(!Irp->Buffer || Irp->Length == 0) return STATUS_INVALID_PARAMETER;
    /* Simple command parser: if starts with ECHO space -> store rest; if PING -> respond via ring */
    const CHAR* buf = (const CHAR*)Irp->Buffer;
    if(Irp->Length >= 4 && strncmp(buf, "PING", 4) == 0){
        SysStubRingWrite("PONG\n");
        Irp->Information = 4;
        return STATUS_SUCCESS;
    }
    if(Irp->Length >= 5 && strncmp(buf, "ECHO ",5) == 0){
        CHAR tmp[128];
        UINT32 toCopy = (Irp->Length - 5 < (sizeof(tmp)-2)) ? (Irp->Length -5) : (sizeof(tmp)-2);
        memcpy(tmp, buf+5, toCopy); tmp[toCopy] = '\n'; tmp[toCopy+1] = 0; SysStubRingWrite(tmp);
        Irp->Information = Irp->Length;
        return STATUS_SUCCESS;
    }
    /* Default: store raw text */
    CHAR scratch[128];
    UINT32 toCopy = (Irp->Length < (sizeof(scratch)-2)) ? Irp->Length : (sizeof(scratch)-2);
    memcpy(scratch, buf, toCopy); scratch[toCopy] = '\n'; scratch[toCopy+1] = 0;
    SysStubRingWrite(scratch);
    Irp->Information = Irp->Length;
    return STATUS_SUCCESS;
}

static NTSTATUS SysStubDeviceControl(IN struct _AIO_DEVICE_OBJECT* Device, IN PAIO_IRP Irp){
    (void)Device; (void)Irp;
    return STATUS_NOT_IMPLEMENTED;
}

/* Driver registration + device creation */
NTSTATUS SysStubInitialize(void){
    static int initialized = 0;
    if(initialized) return STATUS_SUCCESS;
    memset(&g_SysStubCtx,0,sizeof(g_SysStubCtx));
    AuroraInitializeSpinLock(&g_SysStubCtx.Lock);
    IoDriverInitialize(&g_SysStubDriver, "systub");
    g_SysStubDriver.Dispatch[AioIrpCreate]        = SysStubCreate;
    g_SysStubDriver.Dispatch[AioIrpClose]         = SysStubClose;
    g_SysStubDriver.Dispatch[AioIrpRead]          = SysStubRead;
    g_SysStubDriver.Dispatch[AioIrpWrite]         = SysStubWrite;
    g_SysStubDriver.Dispatch[AioIrpDeviceControl] = SysStubDeviceControl;
    NTSTATUS st = IoRegisterDriver(&g_SysStubDriver);
    if(!NT_SUCCESS(st)) return st;
    st = IoCreateDevice(&g_SysStubDriver, "systub", (IO_DEVICE_CLASS_CHAR<<16)|1, &g_SysStubDevice);
    if(!NT_SUCCESS(st)) return st;
    initialized = 1;
    return STATUS_SUCCESS;
}

/* End systub */
