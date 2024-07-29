#include <Library/DebugLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/IoLib.h>

VOID
SendSDCmd (
  UINT32 command,
  UINT32 recieveResponse,
  UINT32 responseType,
  UINT32 checkResponseCRC,
  UINT32 isDataTransfer,
  UINT32 transferDirection,
  UINT32 transferMode,
  UINT32 sendStopCMD,
  UINT32 waitForDataTransferEnd,
  UINT32 stopDataTransfer,
  UINT32 sendInitSeq,
  UINT32 programClock,
  UINT32 bootMode,
  UINT32 expectBootAcknowledgement,
  UINT32 abortBoot,
  UINT32 isVoltageSwitchCMD
)
{
  UINT32 cmd = 0;
  UINT32 SMHC_BASE = 0x04020000;
  UINT32 SMHC_CMD = 0x0018;

  cmd |= command & 0x1F;
  cmd |= recieveResponse << 6;
  cmd |= responseType << 7;
  cmd |= checkResponseCRC << 8;
  cmd |= isDataTransfer << 9;
  cmd |= transferDirection << 10;
  cmd |= transferMode << 11;
  cmd |= sendStopCMD << 12;
  cmd |= waitForDataTransferEnd << 13;
  cmd |= stopDataTransfer << 14;
  cmd |= sendInitSeq << 15;
  // Bits 16-20 are empty
  cmd |= programClock << 21;
  // Bits 22-23 are empty
  cmd |= (bootMode & 3) << 24;
  cmd |= expectBootAcknowledgement << 26;
  cmd |= abortBoot << 27;
  cmd |= isVoltageSwitchCMD << 28;
  // Bits 29-30 are empty
  cmd |= 1 << 31;

  MmioWrite32(SMHC_BASE + SMHC_CMD, cmd);
}

VOID
InitSD ()
{
  UINT32 CCU_BASE = 0x03001000;
  UINT32 SMHC_BGR = 0x084C;
  UINT32 SMHC_CLK_REG = 0x0830;
  UINT32 SMHC_BASE = 0x04020000;
  UINT32 SMHC_INTMASK_BASE = 0x0030;
  UINT32 SMHC_CLKDIV = 0x0004;
  UINT32 SMHC_CMD = 0x0018;

  UINT32 MMC_CLOCK_CONFIG;
  UINT32 MMC_CONFIG;

  MMC_CLOCK_CONFIG = MmioRead32(CCU_BASE + SMHC_BGR);
  MMC_CLOCK_CONFIG |= (0x1 << 16);
  MmioWrite32(CCU_BASE + SMHC_BGR, MMC_CLOCK_CONFIG); // Reset MMC Clock

  MMC_CLOCK_CONFIG = MmioRead32(CCU_BASE + SMHC_BGR);
  MMC_CLOCK_CONFIG |= 0x1;
  MmioWrite32(CCU_BASE + SMHC_BGR, MMC_CLOCK_CONFIG); // Open MMC Clock Gate

  MMC_CLOCK_CONFIG = MmioRead32(CCU_BASE + SMHC_CLK_REG);
  MMC_CLOCK_CONFIG &= ~(0x3 << 24);
  MMC_CLOCK_CONFIG |= (0x1 << 24); // Set the CLK_SRC_SEL register

  MMC_CLOCK_CONFIG &= ~(0x3 << 8);
  MMC_CLOCK_CONFIG |= (0x1 << 8); // Set the FACTOR_N register

  MMC_CLOCK_CONFIG &= ~(0x7);
  MMC_CLOCK_CONFIG |= 0x3; // Set the FACTOR_M register

  MMC_CLOCK_CONFIG |= (0x1 << 31); // Enable clock
  MmioWrite32(CCU_BASE + SMHC_CLK_REG, MMC_CLOCK_CONFIG); // Turn on the clock with the new settings

  MMC_CONFIG = MmioRead32(SMHC_BASE); // SMHC_CTRL lives on the SMHC Base
  MMC_CONFIG |= (1 << 1); // FIFO Reset

  MmioWrite32(SMHC_BASE, MMC_CONFIG);
  while((MmioRead32(SMHC_BASE) >> 1) & 0x1) { DEBUG((EFI_D_WARN, "Waiting for FIFO Reset...\n")); } // Wait for the reset operation to complete

  MmioWrite32(SMHC_BASE + SMHC_INTMASK_BASE, 0xFFCE); // According to the H616 documentation this "enables normal interrupt and error abnormal interrupt, and register interrupt function."

  MMC_CONFIG = MmioRead32(SMHC_BASE + SMHC_CLKDIV);
  MMC_CONFIG |= (0x1 << 16); // Enable card clock

  MmioWrite32(SMHC_BASE + SMHC_CLKDIV, MMC_CONFIG);

  DEBUG((EFI_D_WARN, "Card Ready to take commands\n"));

  DEBUG((EFI_D_WARN, "Sending over command to change clock...\n"));

  MmioWrite32(SMHC_BASE + SMHC_CMD, 0x80202000);

  while(MmioRead32(SMHC_BASE + SMHC_CMD) >> 31 & 1) { DEBUG((EFI_D_WARN, "Waiting for command to be sent over...\n")); }

  DEBUG((EFI_D_WARN, "Command has been sent!!!!!\n"));

  DEBUG((EFI_D_WARN, "Sending CMD0\n"));

  SendSDCmd(0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0);

  while(MmioRead32(SMHC_BASE + SMHC_CMD) >> 31 & 1) { DEBUG((EFI_D_WARN, "Waiting for command to be sent over...\n")); }

  DEBUG((EFI_D_WARN, "Command has been sent!!!!!\n"));

  while(TRUE){}
}

EFI_STATUS
EFIAPI
InitSDDriver (
  IN EFI_HANDLE        ImageHandle,
  IN EFI_SYSTEM_TABLE *SystemTable
  )
{
  InitSD();

  return EFI_SUCCESS;
}
