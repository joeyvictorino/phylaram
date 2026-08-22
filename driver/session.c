#include "driver.h"
#include <intrin.h>

#pragma alloc_text(PAGE, PhylaSessionBegin)
#pragma alloc_text(PAGE, PhylaSessionRelease)
#pragma alloc_text(PAGE, PhylaSessionEnd)
#pragma alloc_text(PAGE, PhylaQueryKernelHints)

static ULONG PhylaCountRanges(_In_ PPHYSICAL_MEMORY_RANGE Ranges)
{
    ULONG count = 0;
    if (Ranges == NULL) {
        return 0;
    }

    while (Ranges[count].BaseAddress.QuadPart != 0 ||
           Ranges[count].NumberOfBytes.QuadPart != 0) {
        ++count;
    }
    return count;
}

static VOID PhylaSummarizeRanges(_In_ PPHYSICAL_MEMORY_RANGE Ranges,
                                 _In_ ULONG Count,
                                 _Out_ PULONGLONG TotalBytes,
                                 _Out_ PULONGLONG HighestPhysicalEnd)
{
    ULONGLONG total = 0;
    ULONGLONG highest = 0;

    for (ULONG i = 0; i < Count; ++i) {
        ULONGLONG base = (ULONGLONG)Ranges[i].BaseAddress.QuadPart;
        ULONGLONG length = (ULONGLONG)Ranges[i].NumberOfBytes.QuadPart;
        ULONGLONG end;

        if (length > MAXULONGLONG - base) {
            continue;
        }
        end = base + length;
        if (MAXULONGLONG - total >= length) {
            total += length;
        }
        if (end > highest) {
            highest = end;
        }
    }

    *TotalBytes = total;
    *HighestPhysicalEnd = highest;
}

static BOOLEAN PhylaRangesEqual(_In_ PPHYSICAL_MEMORY_RANGE A,
                                _In_ ULONG CountA,
                                _In_ PPHYSICAL_MEMORY_RANGE B,
                                _In_ ULONG CountB)
{
    if (CountA != CountB) {
        return FALSE;
    }

    for (ULONG i = 0; i < CountA; ++i) {
        if (A[i].BaseAddress.QuadPart != B[i].BaseAddress.QuadPart ||
            A[i].NumberOfBytes.QuadPart != B[i].NumberOfBytes.QuadPart) {
            return FALSE;
        }
    }
    return TRUE;
}

NTSTATUS PhylaSessionBegin(_Inout_ PPHYLA_FILE_CONTEXT Context)
{
    PPHYSICAL_MEMORY_RANGE ranges;
    ULONG count;

    PAGED_CODE();

    if (Context == NULL) {
        return STATUS_INVALID_PARAMETER;
    }

    ranges = MmGetPhysicalMemoryRangesEx2(NULL, 0);
    if (ranges == NULL) {
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    count = PhylaCountRanges(ranges);
    if (count == 0) {
        ExFreePool(ranges);
        return STATUS_NOT_FOUND;
    }

    Context->Ranges = ranges;
    Context->RunCount = count;
    Context->SessionEnded = FALSE;
    PhylaSummarizeRanges(ranges, count, &Context->TotalBytes, &Context->HighestPhysicalEnd);

    return STATUS_SUCCESS;
}

VOID PhylaSessionRelease(_Inout_ PPHYLA_FILE_CONTEXT Context)
{
    PPHYSICAL_MEMORY_RANGE rangesToFree;

    PAGED_CODE();

    if (Context == NULL) {
        return;
    }

    rangesToFree = (PPHYSICAL_MEMORY_RANGE)InterlockedExchangePointer(
        (PVOID*)&Context->Ranges,
        NULL
    );

    if (rangesToFree != NULL) {
        ExFreePool(rangesToFree);
    }

    Context->RunCount = 0;
    Context->TotalBytes = 0;
    Context->HighestPhysicalEnd = 0;
    Context->SessionEnded = TRUE;
}

NTSTATUS PhylaSessionEnd(_Inout_ PPHYLA_FILE_CONTEXT Context,
                         _Out_ PBOOLEAN TopologyChanged,
                         _Out_ PULONG CurrentRunCount)
{
    PPHYSICAL_MEMORY_RANGE current;
    ULONG currentCount;

    PAGED_CODE();

    if (Context == NULL || TopologyChanged == NULL || CurrentRunCount == NULL) {
        return STATUS_INVALID_PARAMETER;
    }

    if (Context->Ranges == NULL || Context->SessionEnded) {
        return STATUS_INVALID_DEVICE_STATE;
    }

    current = MmGetPhysicalMemoryRangesEx2(NULL, 0);
    if (current == NULL) {
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    currentCount = PhylaCountRanges(current);
    *CurrentRunCount = currentCount;
    *TopologyChanged = !PhylaRangesEqual(Context->Ranges, Context->RunCount, current, currentCount);

    ExFreePool(current);
    Context->SessionEnded = TRUE;
    return STATUS_SUCCESS;
}

NTSTATUS PhylaQueryKernelHints(_Out_ PPHYLA_KERNEL_HINTS Hints)
{
    ULONG major = 0, minor = 0, build = 0;
    int cpuInfo[4] = { 0 };
    PVOID kernelBase = NULL;
    PVOID sectionHandle = NULL;
    KAPC_STATE apcState;

    PAGED_CODE();

    if (Hints == NULL) {
        return STATUS_INVALID_PARAMETER;
    }

    RtlZeroMemory(Hints, sizeof(*Hints));
    Hints->Version = PHYLA_PROTOCOL_VERSION;

    PsGetVersion(&major, &minor, &build, NULL);
    Hints->MajorVersion = major;
    Hints->MinorVersion = minor;
    Hints->BuildNumber = build;
    Hints->NumberOfProcessors = (ULONG)KeQueryActiveProcessorCountEx(ALL_PROCESSOR_GROUPS);

#if defined(_M_AMD64) || defined(_M_X64)
    // SAFETY: Read the System process DirectoryTableBase (CR3) by briefly
    // attaching to PsInitialSystemProcess. This gives Volatility 3 and
    // MemProcFS the exact DTB they need to begin analysis without scanning.
    // KeStackAttachProcess/KeUnstackDetachProcess is safe at PASSIVE_LEVEL.
    KeStackAttachProcess(PsInitialSystemProcess, &apcState);
    Hints->DirectoryTableBase = (ULONGLONG)__readcr3();
    KeUnstackDetachProcess(&apcState);

    // KPCR: This is the KPCR of whichever CPU we happen to be running on.
    // It is NOT guaranteed to be CPU 0. Downstream tools should treat this
    // as a starting hint, not a definitive Core 0 KPCR address.
    Hints->KpcrAddress = (ULONGLONG)__readgsqword(0x18);
#endif

    kernelBase = RtlPcToFileHeader((PVOID)&ZwYieldExecution, &sectionHandle);
    if (kernelBase != NULL) {
        PIMAGE_DOS_HEADER dos = (PIMAGE_DOS_HEADER)kernelBase;
        Hints->KernelBase = (ULONGLONG)kernelBase;
        if (dos->e_magic == IMAGE_DOS_SIGNATURE) {
            PIMAGE_NT_HEADERS nt = (PIMAGE_NT_HEADERS)((PUCHAR)kernelBase + dos->e_lfanew);
            if (nt->Signature == IMAGE_NT_SIGNATURE) {
                Hints->KernelSize = (ULONGLONG)nt->OptionalHeader.SizeOfImage;
            }
        }
    }

    __cpuid(cpuInfo, 1);
    if ((cpuInfo[2] & (1 << 31)) != 0) {
        Hints->HypervisorPresent = 1;
    }

    // NOTE: VBS/HVCI detection removed. Hypervisor-present (CPUID.01H:ECX[31])
    // does NOT imply VBS is active — it could be VMware, VirtualBox, or bare
    // Hyper-V without VBS. Proper detection requires ZwQuerySystemInformation
    // with SystemCodeIntegrityInformation which is undocumented and fragile.
    // A forensic tool must not guess.

    return STATUS_SUCCESS;
}
