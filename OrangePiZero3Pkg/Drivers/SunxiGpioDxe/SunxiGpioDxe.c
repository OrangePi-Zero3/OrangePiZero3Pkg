#include <Library/DebugLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/IoLib.h>

#include <Protocol/SunxiGpio.h>

VOID
ConfigurePin (
  INT32          Pin,
  INT32          Config
  )
{
  UINT32 Configuration;
  UINT32 Bank = GPIO_BANK(Pin);
  UINT32 Number = GPIO_NUM(Pin);
  UINT32 Bit = GPIO_CFG_BIT(Pin);
  UINT32 Address;

  switch (Bank) {
      case SUNXI_GPIO_C:
          switch (Number) {
              case 0 ... 7:
                  Address = GPIO_CFG0_BASE(Bank);
                  break;
              case 8 ... 15:
                  Address = GPIO_CFG1_BASE(Bank);
                  break;
              case 16:
                  Address = GPIO_CFG2_BASE(Bank);
                  break;
              default:
                  ASSERT(FALSE); // Invalid GPIO Number
                  break;
          }
          break;
      case SUNXI_GPIO_F:
          switch (Number) {
              case 0 ... 6:
                  Address = GPIO_CFG0_BASE(Bank);
                  break;
              default:
                  ASSERT(FALSE); // Invalid GPIO Number
                  break;
          }
          break;
      case SUNXI_GPIO_G:
          switch (Number) {
              case 0 ... 7:
                  Address = GPIO_CFG0_BASE(Bank);
                  break;
              case 8 ... 15:
                  Address = GPIO_CFG1_BASE(Bank);
                  break;
              case 16 ... 19:
                  Address = GPIO_CFG2_BASE(Bank);
                  break;
              default:
                  ASSERT(FALSE); // Invalid GPIO Number
                  break;
          }
          break;
      case SUNXI_GPIO_H:
          switch (Number) {
              case 0 ... 7:
                  Address = GPIO_CFG0_BASE(Bank);
                  break;
              case 8 ... 10:
                  Address = GPIO_CFG1_BASE(Bank);
                  break;
              default:
                  ASSERT(FALSE); // Invalid GPIO Number
                  break;
          }
          break;
      case SUNXI_GPIO_I:
          switch (Number) {
              case 0 ... 7:
                  Address = GPIO_CFG0_BASE(Bank);
                  break;
              case 8 ... 15:
                  Address = GPIO_CFG1_BASE(Bank);
                  break;
              case 16 ... 19:
                  Address = GPIO_CFG2_BASE(Bank);
                  break;
              default:
                  ASSERT(FALSE); // Invalid GPIO Number
                  break;
          }
          break;
      default:
          ASSERT(FALSE); // Invalid Bank
          break;
  }

  Configuration = MmioRead32(Address);
  Configuration &= ~(0x7 << Bit);
  Configuration |= (Config << Bit);

  MmioWrite32(Address, Configuration);
}

/*UINT32
GetGpio (UINT32 Pin)
{
  TODO
}*/

/*VOID
SetPull (
  INT32          Pin,
  INT32          Mode)
{
  TODO
}*/

VOID
SetDrive (
  INT32          Pin,
  INT32          Level
  )
{
  UINT32 Configuration;
  UINT32 Bank = GPIO_BANK(Pin);
  UINT32 Number = GPIO_NUM(Pin);
  UINT32 Bit = GPIO_MDR_BIT(Pin);
  UINT32 Address;

  switch (Bank) {
      case SUNXI_GPIO_C:
          switch (Number) {
              case 0 ... 15:
                  Address = GPIO_MDR0_BASE(Bank);
                  break;
              case 16:
                  Address = GPIO_MDR1_BASE(Bank);
                  break;
              default:
                  ASSERT(FALSE); // Invalid GPIO Number
                  break;
          }
          break;
      case SUNXI_GPIO_F:
          switch (Number) {
              case 0 ... 6:
                  Address = GPIO_MDR0_BASE(Bank);
                  break;
              default:
                  ASSERT(FALSE); // Invalid GPIO Number
                  break;
          }
          break;
      case SUNXI_GPIO_G:
          switch (Number) {
              case 0 ... 15:
                  Address = GPIO_MDR0_BASE(Bank);
                  break;
              case 16 ... 19:
                  Address = GPIO_MDR1_BASE(Bank);
                  break;
              default:
                  ASSERT(FALSE); // Invalid GPIO Number
                  break;
          }
          break;
      case SUNXI_GPIO_H:
          switch (Number) {
              case 0 ... 10:
                  Address = GPIO_MDR0_BASE(Bank);
                  break;
              default:
                  ASSERT(FALSE); // Invalid GPIO Number
                  break;
          }
          break;
      case SUNXI_GPIO_I:
          switch (Number) {
              case 0 ... 15:
                  Address = GPIO_MDR0_BASE(Bank);
                  break;
              case 16:
                  Address = GPIO_MDR1_BASE(Bank);
                  break;
              default:
                  ASSERT(FALSE); // Invalid GPIO Number
                  break;
          }
          break;
      default:
          ASSERT(FALSE); // Invalid Bank
          break;
  }

  Configuration = MmioRead32(Address);
  Configuration &= ~(0x3 << Bit);
  Configuration |= (Level << Bit);

  MmioWrite32(Address, Configuration);
}

VOID
SetOutput (
  INT32          Pin,
  INT32          Enable
  )
{
  UINT32 Configuration;
  UINT32 Bank = GPIO_BANK(Pin);
  UINT32 Number = GPIO_NUM(Pin);
  UINT32 Address = GPIO_DAT_BASE(Bank);

  Configuration = MmioRead32(Address);
  Configuration |= (Enable << Number);

  MmioWrite32(Address, Configuration);
}

STATIC SUNXI_GPIO_PROTOCOL mSunxiGpio = {
  ConfigurePin,
  SetDrive,
  SetOutput
};

EFI_STATUS
EFIAPI
InitGpioDriver (
  IN EFI_HANDLE        ImageHandle,
  IN EFI_SYSTEM_TABLE *SystemTable
  )
{
  EFI_STATUS Status;

  // Install Sunxi Gpio Protocol in a new Handle
  Status = gBS->InstallMultipleProtocolInterfaces (&ImageHandle, &gSunxiGpioProtocolGuid, &mSunxiGpio, NULL);
  if (EFI_ERROR (Status)) {
    DEBUG ((EFI_D_ERROR, "Failed to Register Sunxi GPIO Protocol!\n"));
    ASSERT_EFI_ERROR (Status);
  }

  return EFI_SUCCESS;
}
