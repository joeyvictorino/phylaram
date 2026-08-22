#include "driver.h"

DRIVER_INITIALIZE DriverEntry;

#pragma alloc_text(INIT, DriverEntry)
#pragma alloc_text(PAGE, PhylaEvtDriverUnload)
#pragma alloc_text(PAGE, PhylaEvtFileCreate)
#pragma alloc_text(PAGE, PhylaEvtFileCleanup)
#pragma alloc_text(PAGE, PhylaEvtFileClose)

static NTSTATUS PhylaCreateControlDevice(_In_ WDFDRIVER Driver)
{
    NTSTATUS status;
    PWDFDEVICE_INIT init = NULL;
    WDFDEVICE device = NULL;
    WDF_OBJECT_ATTRIBUTES fileAttributes;
    WDF_OBJECT_ATTRIBUTES deviceAttributes;
    WDF_FILEOBJECT_CONFIG fileConfig;
    WDF_IO_QUEUE_CONFIG queueConfig;
    DECLARE_CONST_UNICODE_STRING(deviceName, PHYLA_NT_DEVICE_NAME);
    DECLARE_CONST_UNICODE_STRING(symbolicLink, PHYLA_DOS_DEVICE_NAME);
    DECLARE_CONST_UNICODE_STRING(sddl, L"D:P(A;;GA;;;SY)(A;;GA;;;BA)");

    init = WdfControlDeviceInitAllocate(Driver, &sddl);
    if (init == NULL) {
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    WdfDeviceInitSetExclusive(init, TRUE);
    WdfDeviceInitSetDeviceType(init, FILE_DEVICE_UNKNOWN);
    WdfDeviceInitSetIoType(init, WdfDeviceIoDirect);

    status = WdfDeviceInitAssignName(init, &deviceName);
    if (!NT_SUCCESS(status)) {
        WdfDeviceInitFree(init);
        return status;
    }

    WDF_FILEOBJECT_CONFIG_INIT(&fileConfig,
                               PhylaEvtFileCreate,
                               PhylaEvtFileClose,
                               PhylaEvtFileCleanup);
    WDF_OBJECT_ATTRIBUTES_INIT_CONTEXT_TYPE(&fileAttributes, PHYLA_FILE_CONTEXT);
    fileAttributes.ExecutionLevel = WdfExecutionLevelPassive;
    WdfDeviceInitSetFileObjectConfig(init, &fileConfig, &fileAttributes);

    WDF_OBJECT_ATTRIBUTES_INIT(&deviceAttributes);
    deviceAttributes.ExecutionLevel = WdfExecutionLevelPassive;

    status = WdfDeviceCreate(&init, &deviceAttributes, &device);
    if (!NT_SUCCESS(status)) {
        if (init != NULL) {
            WdfDeviceInitFree(init);
        }
        return status;
    }

    status = WdfDeviceCreateSymbolicLink(device, &symbolicLink);
    if (!NT_SUCCESS(status)) {
        WdfObjectDelete(device);
        return status;
    }

    WDF_IO_QUEUE_CONFIG_INIT_DEFAULT_QUEUE(&queueConfig, WdfIoQueueDispatchSequential);
    queueConfig.EvtIoDeviceControl = PhylaEvtIoDeviceControl;
    queueConfig.PowerManaged = WdfFalse;

    status = WdfIoQueueCreate(device, &queueConfig, WDF_NO_OBJECT_ATTRIBUTES, WDF_NO_HANDLE);
    if (!NT_SUCCESS(status)) {
        WdfObjectDelete(device);
        return status;
    }

    WdfControlFinishInitializing(device);
    return STATUS_SUCCESS;
}

NTSTATUS DriverEntry(_In_ PDRIVER_OBJECT DriverObject, _In_ PUNICODE_STRING RegistryPath)
{
    WDF_DRIVER_CONFIG config;
    WDF_OBJECT_ATTRIBUTES attributes;
    WDFDRIVER driver;
    NTSTATUS status;

    WDF_DRIVER_CONFIG_INIT(&config, WDF_NO_EVENT_CALLBACK);
    config.DriverInitFlags |= WdfDriverInitNonPnpDriver;
    config.EvtDriverUnload = PhylaEvtDriverUnload;
    config.DriverPoolTag = PHYLA_POOL_TAG;

    WDF_OBJECT_ATTRIBUTES_INIT(&attributes);

    status = WdfDriverCreate(DriverObject, RegistryPath, &attributes, &config, &driver);
    if (!NT_SUCCESS(status)) {
        return status;
    }

    return PhylaCreateControlDevice(driver);
}

VOID PhylaEvtDriverUnload(_In_ WDFDRIVER Driver)
{
    UNREFERENCED_PARAMETER(Driver);
    PAGED_CODE();
}

VOID PhylaEvtFileCreate(_In_ WDFDEVICE Device, _In_ WDFREQUEST Request, _In_ WDFFILEOBJECT FileObject)
{
    PPHYLA_FILE_CONTEXT ctx;
    NTSTATUS status;

    UNREFERENCED_PARAMETER(Device);
    PAGED_CODE();

    ctx = PhylaGetFileContext(FileObject);
    RtlZeroMemory(ctx, sizeof(*ctx));

    status = PhylaSessionBegin(ctx);
    WdfRequestComplete(Request, status);
}

VOID PhylaEvtFileCleanup(_In_ WDFFILEOBJECT FileObject)
{
    PPHYLA_FILE_CONTEXT ctx = PhylaGetFileContext(FileObject);
    PAGED_CODE();
    PhylaSessionRelease(ctx);
}

VOID PhylaEvtFileClose(_In_ WDFFILEOBJECT FileObject)
{
    // Cleanup already called PhylaSessionRelease via EvtFileCleanup.
    // EvtFileClose is a no-op — it exists only as a safety net and
    // PhylaSessionRelease is idempotent (InterlockedExchangePointer).
    UNREFERENCED_PARAMETER(FileObject);
}
