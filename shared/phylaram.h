#pragma once

#ifdef _KERNEL_MODE
#include <ntddk.h>
#else
#include <windows.h>
#include <winioctl.h>
#endif

#define PHYLA_PROTOCOL_VERSION  2u
#define PHYLA_MAX_TRANSFER      (16u * 1024u * 1024u)
#define PHYLA_PAGE_SIZE         4096u

#define PHYLA_NT_DEVICE_NAME    L"\\Device\\PhylaRAM"
#define PHYLA_DOS_DEVICE_NAME   L"\\DosDevices\\PhylaRAM"
#define PHYLA_USER_DEVICE_NAME  L"\\\\.\\PhylaRAM"
#define PHYLA_SERVICE_NAME      L"PhylaRAM"

#define IOCTL_PHYLA_QUERY_INFO \
    CTL_CODE(FILE_DEVICE_UNKNOWN, 0x800, METHOD_BUFFERED, FILE_READ_ACCESS)
#define IOCTL_PHYLA_GET_RANGES \
    CTL_CODE(FILE_DEVICE_UNKNOWN, 0x801, METHOD_BUFFERED, FILE_READ_ACCESS)
#define IOCTL_PHYLA_READ_RUN \
    CTL_CODE(FILE_DEVICE_UNKNOWN, 0x802, METHOD_OUT_DIRECT, FILE_READ_ACCESS)
#define IOCTL_PHYLA_END_SESSION \
    CTL_CODE(FILE_DEVICE_UNKNOWN, 0x803, METHOD_BUFFERED, FILE_READ_ACCESS)
#define IOCTL_PHYLA_QUERY_HINTS \
    CTL_CODE(FILE_DEVICE_UNKNOWN, 0x804, METHOD_BUFFERED, FILE_READ_ACCESS)

#define PHYLA_VERSION_STRING    "0.1.0-alpha"
#define PHYLA_VERSION_WSTRING   L"0.1.0-alpha"

typedef struct _PHYLA_MEMORY_RUN {
    ULONGLONG BaseAddress;
    ULONGLONG NumberOfBytes;
} PHYLA_MEMORY_RUN, *PPHYLA_MEMORY_RUN;

typedef struct _PHYLA_QUERY_INFO {
    ULONG Version;
    ULONG RunCount;
    ULONGLONG TotalBytes;
    ULONGLONG HighestPhysicalEnd;
} PHYLA_QUERY_INFO, *PPHYLA_QUERY_INFO;

typedef struct _PHYLA_READ_REQUEST {
    ULONG RunIndex;
    ULONG Length;
    ULONGLONG OffsetWithinRun;
} PHYLA_READ_REQUEST, *PPHYLA_READ_REQUEST;

typedef struct _PHYLA_READ_RESULT {
    ULONG Version;
    ULONG HeaderSize;
    ULONGLONG PhysicalAddress;
    ULONG RequestedLength;
    ULONG BytesCopied;
    LONG CopyStatus;
    ULONG Reserved;
} PHYLA_READ_RESULT, *PPHYLA_READ_RESULT;

typedef struct _PHYLA_END_RESULT {
    ULONG Version;
    ULONG TopologyChanged;
    ULONG CurrentRunCount;
    ULONG Reserved;
} PHYLA_END_RESULT, *PPHYLA_END_RESULT;

typedef struct _PHYLA_KERNEL_HINTS {
    ULONG Version;
    ULONG HypervisorPresent;
    ULONG MajorVersion;
    ULONG MinorVersion;
    ULONG BuildNumber;
    ULONG NumberOfProcessors;
    ULONG Reserved;
    ULONG Reserved2;
    ULONGLONG DirectoryTableBase;  // System process CR3 / DTB for instant Volatility/WinDbg resolution
    ULONGLONG KpcrAddress;         // Current CPU KPCR (hint only)
    ULONGLONG KernelBase;          // NTOSKRNL base address
    ULONGLONG KernelSize;          // NTOSKRNL image size
} PHYLA_KERNEL_HINTS, *PPHYLA_KERNEL_HINTS;

#if defined(_MSC_VER)
#define PHYLA_STATIC_ASSERT(expr, msg) static_assert((expr), msg)
#elif defined(__cplusplus)
#define PHYLA_STATIC_ASSERT(expr, msg) static_assert((expr), msg)
#else
#define PHYLA_STATIC_ASSERT(expr, msg) _Static_assert((expr), msg)
#endif

#if defined(_MSC_VER) || defined(__cplusplus) || defined(__STDC_VERSION__)
PHYLA_STATIC_ASSERT(sizeof(PHYLA_MEMORY_RUN) == 16, "PHYLA_MEMORY_RUN must be 16 bytes");
PHYLA_STATIC_ASSERT(sizeof(PHYLA_QUERY_INFO) == 24, "PHYLA_QUERY_INFO must be 24 bytes");
PHYLA_STATIC_ASSERT(sizeof(PHYLA_READ_REQUEST) == 16, "PHYLA_READ_REQUEST must be 16 bytes");
PHYLA_STATIC_ASSERT(sizeof(PHYLA_READ_RESULT) == 32, "PHYLA_READ_RESULT must be 32 bytes");
PHYLA_STATIC_ASSERT(sizeof(PHYLA_END_RESULT) == 16, "PHYLA_END_RESULT must be 16 bytes");
PHYLA_STATIC_ASSERT(sizeof(PHYLA_KERNEL_HINTS) == 64, "PHYLA_KERNEL_HINTS must be 64 bytes");
#endif
