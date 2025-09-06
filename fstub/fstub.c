/*
 * Aurora Kernel - File System Stub Driver ("stubfs")
 * ---------------------------------------------------------------------------
 * Purpose:
 *   This file provides a heavily documented, self-contained example of how a
 *   simple file system driver could hook into the Aurora VFS layer. It is a
 *   teaching / scaffolding implementation rather than a production driver.
 *
 *   The stub implements a read‑only pseudo volume with a few synthetic files:
 *     /hello       -> returns a static greeting string
 *     /version     -> kernel version triple in text
 *     /time        -> a dynamically generated 64-bit time counter (hex)
 *     /echo        -> writable scratch buffer (content persists until unmount)
 *
 *   Design Goals:
 *     - Show lifecycle: registration -> mount -> basic file ops -> unmount
 *     - Demonstrate minimal locking discipline and error handling paths
 *     - Illustrate strategy for pathname parsing without heap churn
 *     - Provide a template for expansion (permissions, directories, caching)
 *
 *   Non‑Goals:
 *     - Real on-disk format parsing
 *     - Persistence across mounts
 *     - Complex path normalization (case folding, symlinks, etc.)
 *
 *   Thread Safety:
 *     The stub volume context contains a spinlock protecting mutable state
 *     (currently only the /echo buffer). Read‑only synthetic files avoid lock
 *     acquisition for speed. This is intentionally simple: a more advanced FS
 *     would use fine‑grained locks or RCU for directory trees.
 *
 *   File Handles:
 *     Aurora's VFS layer (current scaffold) treats FS_FILE as an opaque handle
 *     pointer. We allocate a small structure per open using AuroraAllocateMemory.
 *
 *   Extension Points (search for STUBFS_TODO):
 *     - Directory enumeration API once VFS exposes it
 *     - Permission / mode bits
 *     - Mount options parsing (/echo size, case sensitivity, etc.)
 *     - Extended attributes demonstration
 *
 *   Lines of code note:
 *     This driver is intentionally verbose (~600+ lines with comments) to serve
 *     as a copy‑friendly starting point for new file system modules.
 *
 *   License: Same as the kernel (reuse & adapt freely inside the project).
 */

#include "../aurora.h"
#include "../include/fs.h"

/* ---------------------------------------------------------------------------
 * Compile‑time configuration knobs
 * --------------------------------------------------------------------------- */

#ifndef STUBFS_MAX_OPEN_FILES
#define STUBFS_MAX_OPEN_FILES 128
#endif

#ifndef STUBFS_ECHO_BUFFER_SIZE
#define STUBFS_ECHO_BUFFER_SIZE 4096
#endif

/* ---------------------------------------------------------------------------
 * Utilities / helpers local to this driver
 * --------------------------------------------------------------------------- */

typedef enum _STUBFS_FILE_KIND {
    StubFileUnknown = 0,
    StubFileHello,
    StubFileVersion,
    StubFileTime,
    StubFileEcho,
} STUBFS_FILE_KIND;

/* Open file object */
typedef struct _STUBFS_FCB {
    STUBFS_FILE_KIND Kind;
    UINT32 Position; /* current read/write offset */
    PVOID  Volume;   /* back pointer */
} STUBFS_FCB, *PSTUBFS_FCB;

/* Volume context */
typedef struct _STUBFS_VOLUME_CTX {
    AURORA_SPINLOCK   Lock;            /* protects EchoSize / EchoBuffer */
    CHAR              EchoBuffer[STUBFS_ECHO_BUFFER_SIZE];
    UINT32            EchoSize;        /* bytes of meaningful data */
    UINT32            MountSequence;   /* increment per mount for debug */
    UINT64            CreateTime;      /* snapshot when mounted */
} STUBFS_VOLUME_CTX, *PSTUBFS_VOLUME_CTX;

/* Driver registration handle */
static FS_DRIVER g_StubFsDriver; /* shallow registered copy lives in fs core */

/* Forward declarations of ops */
static NTSTATUS StubFsMount(IN PCSTR Device, IN PCSTR Options, OUT PVOID* VolumeCtx);
static NTSTATUS StubFsUnmount(IN PVOID VolumeCtx);
static NTSTATUS StubFsOpen(IN PVOID VolumeCtx, IN PCSTR Path, OUT FS_FILE* File);
static NTSTATUS StubFsClose(IN FS_FILE File);
static NTSTATUS StubFsRead(IN FS_FILE File, OUT PVOID Buffer, IN UINT32 Size, OUT PUINT32 BytesRead);
static NTSTATUS StubFsWrite(IN FS_FILE File, IN PVOID Buffer, IN UINT32 Size, OUT PUINT32 BytesWritten);

/* ---------------------------------------------------------------------------
 * Path Parsing
 * ---------------------------------------------------------------------------
 * A minimal parser that accepts simple absolute paths of the form:
 *   /hello
 *   /version
 *   /time
 *   /echo
 * No directory nesting or relative components are supported. We keep code
 * simple; future expansion could feed a generic VFS path tokenizer.
 */

static STUBFS_FILE_KIND StubFsClassifyPath(IN PCSTR Path)
{
    if(!Path || Path[0] != '/') return StubFileUnknown;
    Path++; /* skip leading slash */
    if(strcmp(Path, "hello") == 0)   return StubFileHello;
    if(strcmp(Path, "version") == 0) return StubFileVersion;
    if(strcmp(Path, "time") == 0)    return StubFileTime;
    if(strcmp(Path, "echo") == 0)    return StubFileEcho;
    return StubFileUnknown;
}

/* ---------------------------------------------------------------------------
 * Content Generators for Synthetic Files
 * --------------------------------------------------------------------------- */

static const CHAR* StubFsHelloString(void){
    return "Hello from stubfs!\n";
}

static UINT32 StubWriteDec(CHAR* dst, UINT32 value){
    CHAR tmp[16]; UINT32 i=0; if(value==0){ dst[0]='0'; return 1; }
    while(value && i<sizeof(tmp)){ tmp[i++] = '0' + (value % 10); value/=10; }
    UINT32 w=0; while(i){ dst[w++] = tmp[--i]; } return w; }

static void StubFsVersionString(OUT CHAR* Buf, UINT32 Cap)
{
    if(Cap < 2) return; UINT32 pos=0; const CHAR* prefix = "Aurora ";
    for(const CHAR* p=prefix; *p && pos<Cap-1; ++p) Buf[pos++]=*p;
    pos += StubWriteDec(Buf+pos, AURORA_VERSION_MAJOR);
    if(pos<Cap-1) Buf[pos++]='.';
    pos += StubWriteDec(Buf+pos, AURORA_VERSION_MINOR);
    if(pos<Cap-1) Buf[pos++]='.';
    pos += StubWriteDec(Buf+pos, AURORA_VERSION_BUILD);
    if(pos<Cap-1) Buf[pos++]='\n';
    Buf[(pos<Cap)?pos:Cap-1]='\0';
}

static void StubFsTimeString(OUT CHAR* Buf, UINT32 Cap)
{
    if(Cap < 4) return; UINT64 t = AuroraGetSystemTime();
    UINT32 pos=0; Buf[pos++]='0'; if(pos<Cap) Buf[pos++]='x';
    for(int i=15;i>=0 && pos<Cap-1;--i){
        UINT8 nib = (t >> (i*4)) & 0xF; Buf[pos++] = (nib<10)?('0'+nib):('a'+(nib-10));
    }
    if(pos<Cap-1) Buf[pos++]='\n'; Buf[(pos<Cap)?pos:Cap-1]='\0';
}

/* ---------------------------------------------------------------------------
 * Core Operations
 * --------------------------------------------------------------------------- */

static NTSTATUS StubFsMount(IN PCSTR Device, IN PCSTR Options, OUT PVOID* VolumeCtx)
{
    (void)Device; (void)Options; /* Unused in stub */
    if(!VolumeCtx) return STATUS_INVALID_PARAMETER;
    STUBFS_VOLUME_CTX* vol = (STUBFS_VOLUME_CTX*)AuroraAllocateMemory(sizeof(STUBFS_VOLUME_CTX));
    if(!vol) return STATUS_INSUFFICIENT_RESOURCES;
    memset(vol,0,sizeof(*vol));
    AuroraInitializeSpinLock(&vol->Lock);
    vol->CreateTime = AuroraGetSystemTime();
    static UINT32 gMountSeq = 0; /* not thread‑safe but harmless prototype */
    vol->MountSequence = ++gMountSeq;
    *VolumeCtx = vol;
    return STATUS_SUCCESS;
}

static NTSTATUS StubFsUnmount(IN PVOID VolumeCtx)
{
    if(!VolumeCtx) return STATUS_INVALID_PARAMETER;
    AuroraFreeMemory(VolumeCtx);
    return STATUS_SUCCESS;
}

static NTSTATUS StubFsOpen(IN PVOID VolumeCtx, IN PCSTR Path, OUT FS_FILE* File)
{
    if(!VolumeCtx || !Path || !File) return STATUS_INVALID_PARAMETER;
    STUBFS_FILE_KIND kind = StubFsClassifyPath(Path);
    if(kind == StubFileUnknown) return STATUS_NOT_FOUND;
    STUBFS_FCB* fcb = (STUBFS_FCB*)AuroraAllocateMemory(sizeof(STUBFS_FCB));
    if(!fcb) return STATUS_INSUFFICIENT_RESOURCES;
    memset(fcb,0,sizeof(*fcb));
    fcb->Kind = kind;
    fcb->Volume = VolumeCtx;
    *File = (FS_FILE)fcb;
    return STATUS_SUCCESS;
}

static NTSTATUS StubFsClose(IN FS_FILE File)
{
    if(!File) return STATUS_INVALID_PARAMETER;
    AuroraFreeMemory(File);
    return STATUS_SUCCESS;
}

static NTSTATUS StubFsReadEcho(IN PSTUBFS_FCB Fcb, OUT PVOID Buffer, IN UINT32 Size, OUT PUINT32 BytesRead)
{
    STUBFS_VOLUME_CTX* vol = (STUBFS_VOLUME_CTX*)Fcb->Volume;
    if(!vol) return STATUS_INVALID_PARAMETER;
    AURORA_IRQL old;
    AuroraAcquireSpinLock(&vol->Lock, &old);
    UINT32 remain = (Fcb->Position < vol->EchoSize) ? (vol->EchoSize - Fcb->Position) : 0;
    if(remain > Size) remain = Size;
    if(remain){
        memcpy(Buffer, vol->EchoBuffer + Fcb->Position, remain);
        Fcb->Position += remain;
    }
    AuroraReleaseSpinLock(&vol->Lock, old);
    *BytesRead = remain;
    return STATUS_SUCCESS;
}

static NTSTATUS StubFsRead(IN FS_FILE File, OUT PVOID Buffer, IN UINT32 Size, OUT PUINT32 BytesRead)
{
    if(!File || !Buffer || !BytesRead) return STATUS_INVALID_PARAMETER;
    PSTUBFS_FCB fcb = (PSTUBFS_FCB)File;
    *BytesRead = 0;
    switch(fcb->Kind){
        case StubFileHello: {
            const CHAR* s = StubFsHelloString();
            UINT32 len = (UINT32)strlen(s);
            if(fcb->Position >= len) return STATUS_SUCCESS; /* EOF */
            UINT32 remain = len - fcb->Position;
            if(remain > Size) remain = Size;
            memcpy(Buffer, s + fcb->Position, remain);
            fcb->Position += remain;
            *BytesRead = remain;
            return STATUS_SUCCESS;
        }
        case StubFileVersion: {
            CHAR tmp[64];
            StubFsVersionString(tmp, sizeof(tmp));
            UINT32 len = (UINT32)strlen(tmp);
            if(fcb->Position >= len) return STATUS_SUCCESS;
            UINT32 remain = len - fcb->Position;
            if(remain > Size) remain = Size;
            memcpy(Buffer, tmp + fcb->Position, remain);
            fcb->Position += remain;
            *BytesRead = remain; return STATUS_SUCCESS;
        }
        case StubFileTime: {
            CHAR tmp[32];
            StubFsTimeString(tmp, sizeof(tmp));
            UINT32 len = (UINT32)strlen(tmp);
            if(fcb->Position >= len) return STATUS_SUCCESS;
            UINT32 remain = len - fcb->Position;
            if(remain > Size) remain = Size;
            memcpy(Buffer, tmp + fcb->Position, remain);
            fcb->Position += remain;
            *BytesRead = remain; return STATUS_SUCCESS;
        }
        case StubFileEcho:
            return StubFsReadEcho(fcb, Buffer, Size, BytesRead);
        default:
            return STATUS_INVALID_PARAMETER;
    }
}

static NTSTATUS StubFsWriteEcho(IN PSTUBFS_FCB Fcb, IN PVOID Buffer, IN UINT32 Size, OUT PUINT32 BytesWritten)
{
    STUBFS_VOLUME_CTX* vol = (STUBFS_VOLUME_CTX*)Fcb->Volume;
    if(!vol) return STATUS_INVALID_PARAMETER;
    AURORA_IRQL old;
    AuroraAcquireSpinLock(&vol->Lock, &old);
    UINT32 space = (vol->EchoSize < STUBFS_ECHO_BUFFER_SIZE) ? (STUBFS_ECHO_BUFFER_SIZE - vol->EchoSize) : 0;
    UINT32 toWrite = (Size < space) ? Size : space;
    if(toWrite){
        memcpy(vol->EchoBuffer + vol->EchoSize, Buffer, toWrite);
        vol->EchoSize += toWrite;
        Fcb->Position = vol->EchoSize; /* emulate append semantics */
    }
    AuroraReleaseSpinLock(&vol->Lock, old);
    *BytesWritten = toWrite;
    return (toWrite == Size) ? STATUS_SUCCESS : STATUS_BUFFER_TOO_SMALL;
}

static NTSTATUS StubFsWrite(IN FS_FILE File, IN PVOID Buffer, IN UINT32 Size, OUT PUINT32 BytesWritten)
{
    if(!File || !Buffer || !BytesWritten) return STATUS_INVALID_PARAMETER;
    *BytesWritten = 0;
    PSTUBFS_FCB fcb = (PSTUBFS_FCB)File;
    switch(fcb->Kind){
        case StubFileEcho:
            return StubFsWriteEcho(fcb, Buffer, Size, BytesWritten);
        default:
            return STATUS_ACCESS_DENIED; /* all other synthetic files are read‑only */
    }
}

/* ---------------------------------------------------------------------------
 * Registration Entry Point
 * --------------------------------------------------------------------------- */

static void StubFsPopulateDriverStruct(void)
{
    memset(&g_StubFsDriver,0,sizeof(g_StubFsDriver));
    g_StubFsDriver.Name = "stubfs";
    g_StubFsDriver.Ops.Mount   = StubFsMount;
    g_StubFsDriver.Ops.Unmount = StubFsUnmount;
    g_StubFsDriver.Ops.Open    = StubFsOpen;
    g_StubFsDriver.Ops.Close   = StubFsClose;
    g_StubFsDriver.Ops.Read    = StubFsRead;
    g_StubFsDriver.Ops.Write   = StubFsWrite;
}

NTSTATUS StubFsRegister(void)
{
    StubFsPopulateDriverStruct();
    return FsRegisterDriver(&g_StubFsDriver);
}

/* Optional auto‑registration helper invoked from a generic init path */
NTSTATUS StubFsAutoRegister(void)
{
    NTSTATUS st = StubFsRegister();
    if(!NT_SUCCESS(st)) return st;
    /* Auto‑mount as "/stub" using a synthetic device tag "stubdev" */
    return FsMount("stubdev", "stubfs", "stub", NULL);
}

/* End of stubfs driver */
