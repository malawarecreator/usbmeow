#include <wdm.h>
#include <ntddk.h>

typedef struct _DEVICE_EXTENSION {
    PDEVICE_OBJECT LowerDeviceObject;
} DEVICE_EXTENSION, *PDEVICE_EXTENSION;

#define GET_EXTENSION(dev) ((PDEVICE_EXTENSION)(dev->DeviceExtension))

DRIVER_INITIALIZE DriverEntry;
DRIVER_UNLOAD DriverUnload;
DRIVER_ADD_DEVICE AddDevice;
DRIVER_DISPATCH DispatchPnp;
DRIVER_DISPATCH DispatchPower;

NTSTATUS DriverEntry(PDRIVER_OBJECT DriverObject, PUNICODE_STRING RegistryPath) {
    UNREFERENCED_PARAMETER(RegistryPath);

    DbgPrint("usbmeow: Driver loading...\n");

    DriverObject->DriverUnload = DriverUnload;
    DriverObject->DriverExtension->AddDevice = AddDevice;
    DriverObject->MajorFunction[IRP_MJ_PNP] = DispatchPnp;
    DriverObject->MajorFunction[IRP_MJ_POWER] = DispatchPower;

    DbgPrint("usbmeow: Driver loaded successfully.\n");
    return STATUS_SUCCESS;
}

VOID DriverUnload(PDRIVER_OBJECT DriverObject) {
    UNREFERENCED_PARAMETER(DriverObject);
    DbgPrint("usbmeow: Driver unloading.\n");
}

NTSTATUS AddDevice(PDRIVER_OBJECT DriverObject, PDEVICE_OBJECT PhysicalDeviceObject) {
    NTSTATUS status;
    PDEVICE_OBJECT FilterDeviceObject;
    PDEVICE_EXTENSION ext;

    DbgPrint("usbmeow: AddDevice called for PDO 0x%p\n", PhysicalDeviceObject);

    status = IoCreateDevice(DriverObject,
                            sizeof(DEVICE_EXTENSION),
                            NULL,
                            FILE_DEVICE_UNKNOWN,
                            0,
                            FALSE,
                            &FilterDeviceObject);

    if (!NT_SUCCESS(status)) {
        DbgPrint("usbmeow: IoCreateDevice failed: 0x%X\n", status);
        return status;
    }

    ext = GET_EXTENSION(FilterDeviceObject);

    ext->LowerDeviceObject = IoAttachDeviceToDeviceStack(FilterDeviceObject, PhysicalDeviceObject);

    if (ext->LowerDeviceObject == NULL) {
        DbgPrint("usbmeow: IoAttachDeviceToDeviceStack failed.\n");
        IoDeleteDevice(FilterDeviceObject);
        return STATUS_DEVICE_REMOVED;
    }

    FilterDeviceObject->Flags |= (ext->LowerDeviceObject->Flags & (DO_BUFFERED_IO | DO_DIRECT_IO | DO_POWER_PAGABLE));
    FilterDeviceObject->DeviceType = ext->LowerDeviceObject->DeviceType;
    FilterDeviceObject->Characteristics = ext->LowerDeviceObject->Characteristics;
    FilterDeviceObject->Flags &= ~DO_DEVICE_INITIALIZING;

    DbgPrint("usbmeow: Filter device 0x%p attached successfully to stack (LowerDevice=0x%p).\n", FilterDeviceObject, ext->LowerDeviceObject);
    return STATUS_SUCCESS;
}

NTSTATUS DispatchPnp(PDEVICE_OBJECT DeviceObject, PIRP Irp) {
    PIO_STACK_LOCATION irpStack = IoGetCurrentIrpStackLocation(Irp);
    PDEVICE_EXTENSION ext = GET_EXTENSION(DeviceObject);
    NTSTATUS status;

    switch (irpStack->MinorFunction) {
        case IRP_MN_START_DEVICE:
            DbgPrint("usbmeow: meow meow meow (IRP_MN_START_DEVICE received for 0x%p)", DeviceObject);
            IoSkipCurrentIrpStackLocation(Irp);
            status = IoCallDriver(ext->LowerDeviceObject, Irp);
            return status;

        case IRP_MN_REMOVE_DEVICE:
            DbgPrint("usbmeow: meow meow meow (IRP_MN_REMOVE_DEVICE received for 0x%p)\n", DeviceObject);
            IoSkipCurrentIrpStackLocation(Irp);
            status = IoCallDriver(ext->LowerDeviceObject, Irp);
            IoDetachDevice(ext->LowerDeviceObject);
            IoDeleteDevice(DeviceObject);
            return status;

        case IRP_MN_QUERY_REMOVE_DEVICE:
        case IRP_MN_CANCEL_REMOVE_DEVICE:
        case IRP_MN_STOP_DEVICE:
        case IRP_MN_QUERY_STOP_DEVICE:
        case IRP_MN_CANCEL_STOP_DEVICE:
        case IRP_MN_SURPRISE_REMOVAL:
        default:
            IoSkipCurrentIrpStackLocation(Irp);
            status = IoCallDriver(ext->LowerDeviceObject, Irp);
            return status;
    }
}

NTSTATUS DispatchPower(PDEVICE_OBJECT DeviceObject, PIRP Irp) {
    PDEVICE_EXTENSION ext = GET_EXTENSION(DeviceObject);
    IoSkipCurrentIrpStackLocation(Irp);
    return PoCallDriver(ext->LowerDeviceObject, Irp);
}
