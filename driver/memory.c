#include "driver.h"

NTSTATUS PhylaCopyRun(_In_ PPHYLA_FILE_CONTEXT Context,
                      _In_ const PHYLA_READ_REQUEST* Request,
                      _Out_writes_bytes_(PayloadCapacity) PUCHAR Payload,
                      _In_ ULONG PayloadCapacity,
                      _Out_ PPHYLA_READ_RESULT Result)
{
    ULONGLONG base;
    ULONGLONG runLength;
    ULONGLONG physicalAddress;
    MM_COPY_ADDRESS source;
    SIZE_T copied = 0;
    NTSTATUS copyStatus;

    if (Context == NULL || Request == NULL || Payload == NULL || Result == NULL) {
        return STATUS_INVALID_PARAMETER;
    }

    if (Context->Ranges == NULL || Context->SessionEnded) {
        return STATUS_INVALID_DEVICE_STATE;
    }

    if (Request->RunIndex >= Context->RunCount ||
        Request->Length == 0 ||
        Request->Length > PHYLA_MAX_TRANSFER ||
        Request->Length > PayloadCapacity) {
        return STATUS_INVALID_PARAMETER;
    }

    base = (ULONGLONG)Context->Ranges[Request->RunIndex].BaseAddress.QuadPart;
    runLength = (ULONGLONG)Context->Ranges[Request->RunIndex].NumberOfBytes.QuadPart;

    if (Request->OffsetWithinRun > runLength ||
        Request->Length > runLength - Request->OffsetWithinRun ||
        Request->OffsetWithinRun > MAXULONGLONG - base) {
        return STATUS_INVALID_PARAMETER;
    }

    physicalAddress = base + Request->OffsetWithinRun;

    RtlZeroMemory(Result, sizeof(*Result));
    Result->Version = PHYLA_PROTOCOL_VERSION;
    Result->HeaderSize = sizeof(*Result);
    Result->PhysicalAddress = physicalAddress;
    Result->RequestedLength = Request->Length;

    RtlZeroMemory(&source, sizeof(source));
    source.PhysicalAddress.QuadPart = physicalAddress;

    copyStatus = MmCopyMemory(Payload,
                              source,
                              Request->Length,
                              MM_COPY_MEMORY_PHYSICAL,
                              &copied);

    if (copied > MAXULONG) {
        copied = MAXULONG;
    }

    Result->BytesCopied = (ULONG)copied;
    Result->CopyStatus = copyStatus;

    // Protocol success is distinct from acquisition copy success.
    // The caller gets both CopyStatus and the exact number of bytes copied.
    return STATUS_SUCCESS;
}
