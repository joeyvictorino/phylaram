#include "driver.h"

static PPHYLA_FILE_CONTEXT PhylaRequestFileContext(_In_ WDFREQUEST Request)
{
    WDFFILEOBJECT fileObject = WdfRequestGetFileObject(Request);
    return fileObject ? PhylaGetFileContext(fileObject) : NULL;
}

static NTSTATUS PhylaHandleQueryInfo(_In_ WDFREQUEST Request,
                                     _In_ size_t OutputBufferLength,
                                     _Out_ size_t* BytesReturned)
{
    PPHYLA_FILE_CONTEXT ctx = PhylaRequestFileContext(Request);
    PPHYLA_QUERY_INFO info;
    NTSTATUS status;

    *BytesReturned = 0;

    if (ctx == NULL || ctx->Ranges == NULL || ctx->SessionEnded) {
        return STATUS_INVALID_DEVICE_STATE;
    }

    if (OutputBufferLength < sizeof(PHYLA_QUERY_INFO)) {
        return STATUS_BUFFER_TOO_SMALL;
    }

    status = WdfRequestRetrieveOutputBuffer(Request, sizeof(PHYLA_QUERY_INFO), (PVOID*)&info, NULL);
    if (!NT_SUCCESS(status)) {
        return status;
    }

    info->Version = PHYLA_PROTOCOL_VERSION;
    info->RunCount = ctx->RunCount;
    info->TotalBytes = ctx->TotalBytes;
    info->HighestPhysicalEnd = ctx->HighestPhysicalEnd;
    *BytesReturned = sizeof(*info);
    return STATUS_SUCCESS;
}

static NTSTATUS PhylaHandleGetRanges(_In_ WDFREQUEST Request,
                                     _In_ size_t OutputBufferLength,
                                     _Out_ size_t* BytesReturned)
{
    PPHYLA_FILE_CONTEXT ctx = PhylaRequestFileContext(Request);
    PPHYLA_MEMORY_RUN out;
    size_t required;
    NTSTATUS status;

    *BytesReturned = 0;

    if (ctx == NULL || ctx->Ranges == NULL || ctx->SessionEnded) {
        return STATUS_INVALID_DEVICE_STATE;
    }

    if (ctx->RunCount > (((size_t)-1) / sizeof(PHYLA_MEMORY_RUN))) {
        return STATUS_INTEGER_OVERFLOW;
    }

    required = (size_t)ctx->RunCount * sizeof(PHYLA_MEMORY_RUN);
    if (OutputBufferLength < required) {
        return STATUS_BUFFER_TOO_SMALL;
    }

    status = WdfRequestRetrieveOutputBuffer(Request, required, (PVOID*)&out, NULL);
    if (!NT_SUCCESS(status)) {
        return status;
    }

    for (ULONG i = 0; i < ctx->RunCount; ++i) {
        out[i].BaseAddress = (ULONGLONG)ctx->Ranges[i].BaseAddress.QuadPart;
        out[i].NumberOfBytes = (ULONGLONG)ctx->Ranges[i].NumberOfBytes.QuadPart;
    }

    *BytesReturned = required;
    return STATUS_SUCCESS;
}

static NTSTATUS PhylaHandleReadRun(_In_ WDFREQUEST Request,
                                   _In_ size_t OutputBufferLength,
                                   _Out_ size_t* BytesReturned)
{
    PPHYLA_FILE_CONTEXT ctx = PhylaRequestFileContext(Request);
    PPHYLA_READ_REQUEST input;
    PMDL mdl = NULL;
    PUCHAR output;
    ULONG outputLength;
    ULONG payloadCapacity;
    PPHYLA_READ_RESULT result;
    PUCHAR payload;
    NTSTATUS status;

    *BytesReturned = 0;

    if (ctx == NULL || ctx->Ranges == NULL || ctx->SessionEnded) {
        return STATUS_INVALID_DEVICE_STATE;
    }

    status = WdfRequestRetrieveInputBuffer(Request, sizeof(PHYLA_READ_REQUEST), (PVOID*)&input, NULL);
    if (!NT_SUCCESS(status)) {
        return status;
    }

    status = WdfRequestRetrieveOutputWdmMdl(Request, &mdl);
    if (!NT_SUCCESS(status) || mdl == NULL) {
        return NT_SUCCESS(status) ? STATUS_INVALID_USER_BUFFER : status;
    }

    output = (PUCHAR)MmGetSystemAddressForMdlSafe(mdl, NormalPagePriority | MdlMappingNoExecute);
    if (output == NULL) {
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    outputLength = MmGetMdlByteCount(mdl);
    if (OutputBufferLength < outputLength) {
        outputLength = (ULONG)OutputBufferLength;
    }

    if (outputLength < sizeof(PHYLA_READ_RESULT)) {
        return STATUS_BUFFER_TOO_SMALL;
    }

    result = (PPHYLA_READ_RESULT)output;
    payload = output + sizeof(PHYLA_READ_RESULT);
    payloadCapacity = outputLength - sizeof(PHYLA_READ_RESULT);

    status = PhylaCopyRun(ctx, input, payload, payloadCapacity, result);
    if (!NT_SUCCESS(status)) {
        return status;
    }

    *BytesReturned = sizeof(PHYLA_READ_RESULT) + result->BytesCopied;
    return STATUS_SUCCESS;
}

static NTSTATUS PhylaHandleEndSession(_In_ WDFREQUEST Request,
                                     _In_ size_t OutputBufferLength,
                                     _Out_ size_t* BytesReturned)
{
    PPHYLA_FILE_CONTEXT ctx = PhylaRequestFileContext(Request);
    PPHYLA_END_RESULT out;
    BOOLEAN changed = FALSE;
    ULONG currentCount = 0;
    NTSTATUS status;

    *BytesReturned = 0;

    if (ctx == NULL) {
        return STATUS_INVALID_DEVICE_STATE;
    }

    if (OutputBufferLength < sizeof(PHYLA_END_RESULT)) {
        return STATUS_BUFFER_TOO_SMALL;
    }

    status = WdfRequestRetrieveOutputBuffer(Request, sizeof(PHYLA_END_RESULT), (PVOID*)&out, NULL);
    if (!NT_SUCCESS(status)) {
        return status;
    }

    status = PhylaSessionEnd(ctx, &changed, &currentCount);
    if (!NT_SUCCESS(status)) {
        return status;
    }

    out->Version = PHYLA_PROTOCOL_VERSION;
    out->TopologyChanged = changed ? 1u : 0u;
    out->CurrentRunCount = currentCount;
    out->Reserved = 0;
    *BytesReturned = sizeof(*out);
    return STATUS_SUCCESS;
}

static NTSTATUS PhylaHandleQueryHints(_In_ WDFREQUEST Request,
                                      _In_ size_t OutputBufferLength,
                                      _Out_ size_t* BytesReturned)
{
    PPHYLA_KERNEL_HINTS hints;
    NTSTATUS status;

    *BytesReturned = 0;

    if (OutputBufferLength < sizeof(PHYLA_KERNEL_HINTS)) {
        return STATUS_BUFFER_TOO_SMALL;
    }

    status = WdfRequestRetrieveOutputBuffer(Request, sizeof(PHYLA_KERNEL_HINTS), (PVOID*)&hints, NULL);
    if (!NT_SUCCESS(status)) {
        return status;
    }

    status = PhylaQueryKernelHints(hints);
    if (!NT_SUCCESS(status)) {
        return status;
    }

    *BytesReturned = sizeof(*hints);
    return STATUS_SUCCESS;
}

VOID PhylaEvtIoDeviceControl(_In_ WDFQUEUE Queue,
                             _In_ WDFREQUEST Request,
                             _In_ size_t OutputBufferLength,
                             _In_ size_t InputBufferLength,
                             _In_ ULONG IoControlCode)
{
    NTSTATUS status;
    size_t bytesReturned = 0;

    UNREFERENCED_PARAMETER(Queue);
    UNREFERENCED_PARAMETER(InputBufferLength);

    switch (IoControlCode) {
    case IOCTL_PHYLA_QUERY_INFO:
        status = PhylaHandleQueryInfo(Request, OutputBufferLength, &bytesReturned);
        break;
    case IOCTL_PHYLA_GET_RANGES:
        status = PhylaHandleGetRanges(Request, OutputBufferLength, &bytesReturned);
        break;
    case IOCTL_PHYLA_READ_RUN:
        status = PhylaHandleReadRun(Request, OutputBufferLength, &bytesReturned);
        break;
    case IOCTL_PHYLA_END_SESSION:
        status = PhylaHandleEndSession(Request, OutputBufferLength, &bytesReturned);
        break;
    case IOCTL_PHYLA_QUERY_HINTS:
        status = PhylaHandleQueryHints(Request, OutputBufferLength, &bytesReturned);
        break;
    default:
        status = STATUS_INVALID_DEVICE_REQUEST;
        break;
    }

    if (!NT_SUCCESS(status)) {
        bytesReturned = 0;
    }

    WdfRequestCompleteWithInformation(Request, status, bytesReturned);
}
