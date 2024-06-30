#include <Library/DebugLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/IoLib.h>

VOID
InitSD ()
{
  UINT32 CCU_BASE = 0x03001000;
  UINT32 SMHC_BGR = 0x084C;
  UINT32 SMHC_CLK_REG = 0x0830;
  UINT32 SMHC_BASE = 0x04020000;
  UINT32 SMHC_INTMASK_BASE = 0x0030;
  UINT32 SMHC_CLKDIV = 0x0004;

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
