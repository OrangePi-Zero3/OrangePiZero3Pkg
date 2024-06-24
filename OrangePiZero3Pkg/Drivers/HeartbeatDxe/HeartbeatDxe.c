#include <Library/DebugLib.h>
#include <Library/UefiBootServicesTableLib.h>

#include <Protocol/SunxiGpio.h>

// Cached copy of the Hardware Gpio protocol instance
SUNXI_GPIO_PROTOCOL *gSunxiGpio = NULL;

EFI_STATUS
EFIAPI
HeartbeatDxeInitialize (
  IN EFI_HANDLE        ImageHandle,
  IN EFI_SYSTEM_TABLE *SystemTable
  )
{
  EFI_STATUS Status;

  // Find the sunxi gpio controller protocol.  ASSERT if not found.
  Status = gBS->LocateProtocol (&gSunxiGpioProtocolGuid, NULL, (VOID **)&gSunxiGpio);
  ASSERT_EFI_ERROR (Status);

  // Turn off red LED and enable green
  gSunxiGpio->SetOutput(SUNXI_GPC(12), SUNXI_GPIO_IO_DISABLE);
  gSunxiGpio->ConfigurePin(SUNXI_GPC(12), SUNXI_GPIO_IO_DISABLE);

  gSunxiGpio->ConfigurePin(SUNXI_GPC(13), SUNXI_GPIO_OUTPUT);
  gSunxiGpio->SetDrive(SUNXI_GPC(13), SUNXI_DRIVE_L0);
  gSunxiGpio->SetOutput(SUNXI_GPC(13), SUNXI_GPIO_OUTPUT);

  return EFI_SUCCESS;
}
