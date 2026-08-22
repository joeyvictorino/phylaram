#pragma once

#include <ntifs.h>
#include <ntimage.h>
#include <wdf.h>
#include "../shared/phylaram.h"

#define PHYLA_POOL_TAG 'MLYP'

NTSYSAPI NTSTATUS NTAPI ZwYieldExecution(VOID);

typedef struct _PHYLA_FILE_CONTEXT {
    PPHYSICAL_MEMORY_RANGE Ranges;
    ULONG RunCount;
    ULONGLONG TotalBytes;
    ULONGLONG HighestPhysicalEnd;
    BOOLEAN SessionEnded;
} PHYLA_FILE_CONTEXT, *PPHYLA_FILE_CONTEXT;

WDF_DECLARE_CONTEXT_TYPE_WITH_NAME(PHYLA_FILE_CONTEXT, PhylaGetFileContext)

EVT_WDF_DRIVER_UNLOAD PhylaEvtDriverUnload;
EVT_WDF_DEVICE_FILE_CREATE PhylaEvtFileCreate;
EVT_WDF_FILE_CLEANUP PhylaEvtFileCleanup;
EVT_WDF_FILE_CLOSE PhylaEvtFileClose;
EVT_WDF_IO_QUEUE_IO_DEVICE_CONTROL PhylaEvtIoDeviceControl;

NTSTATUS PhylaSessionBegin(_Inout_ PPHYLA_FILE_CONTEXT Context);
VOID PhylaSessionRelease(_Inout_ PPHYLA_FILE_CONTEXT Context);
NTSTATUS PhylaSessionEnd(_Inout_ PPHYLA_FILE_CONTEXT Context,
                         _Out_ PBOOLEAN TopologyChanged,
                         _Out_ PULONG CurrentRunCount);

NTSTATUS PhylaCopyRun(_In_ PPHYLA_FILE_CONTEXT Context,
                      _In_ const PHYLA_READ_REQUEST* Request,
                      _Out_writes_bytes_(PayloadCapacity) PUCHAR Payload,
                      _In_ ULONG PayloadCapacity,
                      _Out_ PPHYLA_READ_RESULT Result);

NTSTATUS PhylaQueryKernelHints(_Out_ PPHYLA_KERNEL_HINTS Hints);
