#include <Library/DebugLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/IoLib.h>
#include <Library/TimerLib.h>

VOID
DumpResponseRegisters ()
{
  UINT32 SMHC_BASE = 0x04020000;
  UINT32 SMHC_RSP0 = 0x0020;
  UINT32 SMHC_RSP1 = 0x0024;
  UINT32 SMHC_RSP2 = 0x0028;
  UINT32 SMHC_RSP3 = 0x002C;

  DEBUG((EFI_D_ERROR, "Response 0: 0x%x\n", MmioRead32(SMHC_BASE + SMHC_RSP0)));
  DEBUG((EFI_D_ERROR, "Response 1: 0x%x\n", MmioRead32(SMHC_BASE + SMHC_RSP1)));
  DEBUG((EFI_D_ERROR, "Response 2: 0x%x\n", MmioRead32(SMHC_BASE + SMHC_RSP2)));
  DEBUG((EFI_D_ERROR, "Response 3: 0x%x\n", MmioRead32(SMHC_BASE + SMHC_RSP3)));
}

#define CCU_H6_PLL6_CFG			0x020

#define CCM_PLL6_CTRL_N_SHIFT		8
#define CCM_PLL6_CTRL_N_MASK		(0xff << CCM_PLL6_CTRL_N_SHIFT)

#define CCM_PLL6_CTRL_DIV1_SHIFT	0
#define CCM_PLL6_CTRL_DIV1_MASK		(0x1 << CCM_PLL6_CTRL_DIV1_SHIFT)

#define CCM_PLL6_CTRL_DIV2_SHIFT	1
#define CCM_PLL6_CTRL_DIV2_MASK		(0x1 << CCM_PLL6_CTRL_DIV2_SHIFT)

// Hacky.
UINT32 clock_get_pll6(void)
{
	UINT32 ccm = 0x03001000;
	UINT32 rval = MmioRead32(ccm + CCU_H6_PLL6_CFG);
	INT32 n = ((rval & CCM_PLL6_CTRL_N_MASK) >> CCM_PLL6_CTRL_N_SHIFT) + 1;
	INT32 div2 = ((rval & CCM_PLL6_CTRL_DIV2_MASK) >>
		    CCM_PLL6_CTRL_DIV2_SHIFT) + 1;
	int div1, m;

	div1 = ((rval & CCM_PLL6_CTRL_DIV1_MASK) >>
		CCM_PLL6_CTRL_DIV1_SHIFT) + 1;

	/*
	 * The factors encoded in the register describe the doubled clock
	 * frequency, expect for the H6, where it's the quadrupled frequency.
	 * Compensate for that here.
	 */
	m = 4;

	return 24000000U * n / m / div1 / div2;
}

/* MMC clock bit field */
#define CCM_MMC_CTRL_M(x)		((x) - 1)
#define CCM_MMC_CTRL_N(x)		((x) << 8)
#define CCM_MMC_CTRL_OSCM24		(0x0 << 24)
#define CCM_MMC_CTRL_PLL6		(0x1 << 24)
#define CCM_MMC_CTRL_PLL_PERIPH2X2	(0x2 << 24)
#define CCM_MMC_CTRL_ENABLE		(0x1 << 31)

INT32
MMCSetModClk (UINT32 hz)
{
  UINT32 pll, pll_hz, div, n, oclk_dly, sclk_dly;
  UINT32 val = 0;

  if (hz <= 24000000)
  {
    pll = (0 << 24); // CCM_MMC_CTRL_OSCM24
    pll_hz = 24000000;
  }
  else
  {
    pll = (1 << 24); // CCM_MMC_CTRL_PLL6
    pll_hz = clock_get_pll6(); // Honestly, really hacky, I should really make a clock driver
                               // And put it under it's own protocol.

  }

  div = pll_hz / hz;
  if (pll_hz % hz)
    div++;

  n = 0;
  while (div > 16)
  {
    n++;
    div = (div + 1) / 2;
  }

  if (n > 3)
  {
    DEBUG((EFI_D_ERROR, "Cannot set clock!\n"));
    while(1){}
    return -1;
  }

  // Determine delays
  if (hz <= 400000)
  {
    oclk_dly = 0;
    sclk_dly = 0;
  }
  else if (hz <= 25000000)
  {
    oclk_dly = 0;
    sclk_dly = 5;
  }
  else
  {
    if (hz <= 52000000)
      oclk_dly = 3;
    else
      oclk_dly = 1;

    sclk_dly = 4;
  }

  val |= 0; // CCM_MMC_CTRL_MODE_SEL_NEW
  MmioWrite32(0x04020000 + 0x005C, (MmioRead32(0x04020000 + 0x005C) | (1 << 31))); // SUNXI_MMC_NTSR_MODE_SEL_NEW

  MmioWrite32(0x03001000 + 0x0830, CCM_MMC_CTRL_ENABLE | pll | CCM_MMC_CTRL_N(n) | CCM_MMC_CTRL_M(div) | val);
  return 0;
}

#define SUNXI_MMC_CMD_RESP_EXPIRE	(0x1 << 6)
#define SUNXI_MMC_CMD_LONG_RESPONSE	(0x1 << 7)
#define SUNXI_MMC_CMD_CHK_RESPONSE_CRC	(0x1 << 8)
#define SUNXI_MMC_CMD_DATA_EXPIRE	(0x1 << 9)
#define SUNXI_MMC_CMD_WRITE		(0x1 << 10)
#define SUNXI_MMC_CMD_AUTO_STOP		(0x1 << 12)
#define SUNXI_MMC_CMD_WAIT_PRE_OVER	(0x1 << 13)
#define SUNXI_MMC_CMD_SEND_INIT_SEQ	(0x1 << 15)
#define SUNXI_MMC_CMD_UPCLK_ONLY	(0x1 << 21)
#define SUNXI_MMC_CMD_START		(0x1 << 31)

STATIC
INT32
MMCUpdateClk()
{
  UINT32 cmd;
  UINT32 TimeoutMsecs = 2000;

  cmd = SUNXI_MMC_CMD_START |
	SUNXI_MMC_CMD_UPCLK_ONLY |
	SUNXI_MMC_CMD_WAIT_PRE_OVER;

  MmioWrite32(0x04020000 + 0x0018, cmd);

  while(MmioRead32(0x04020000 + 0x0018) & SUNXI_MMC_CMD_START) {
    MicroSecondDelay(1000);
    TimeoutMsecs--;

    if(TimeoutMsecs <= 0)
    {
      DEBUG((EFI_D_ERROR, "CLK UPDATE FAILED\n"));
      while(1);
      return -1;
    }
  }

  // Clock update sets various irq status bits, clear these
  MmioWrite32(0x04020000 + 0x0038, MmioRead32(0x04020000 + 0x0038));
  return 0;
}

STATIC
INT32
MMCConfigClock()
{
  UINT32 rval = MmioRead32(0x04020000 + 0x0004);

  // Disable Clock
  rval &= ~(1 << 16); // SUNXI_MMC_CLK_ENABLE
  MmioWrite32(0x04020000 + 0x0004, rval);

  if(MMCUpdateClk())
  {
    DEBUG((EFI_D_ERROR, "Welp\n"));
    return -1;
  }

  if(MMCSetModClk(24000000)) // Dunno the actual freq rn, lets hope
  {
    DEBUG((EFI_D_ERROR, "Welp 1\n"));
    return -1;
  }

  MmioWrite32(0x04020000 + 0x0140, 1 << 7); // SUNXI_MMC_CAL_DL_SW_EN

  rval |= (1 << 16); // SUNXI_MMC_CLK_ENABLE
  MmioWrite32(0x04020000 + 0x0004, rval);

  if (MMCUpdateClk())
  {
    DEBUG((EFI_D_ERROR, "Welp 2\n"));
    return -1;
  }

  return 0;
}

STATIC
INT32
SunxiMMCSetIOsCommon ()
{
  DEBUG((EFI_D_ERROR, "Set IOs: BW: 8, CLK: 24000000\n"));

  if(MMCConfigClock())
  {
    DEBUG((EFI_D_ERROR, "uhoh\n"));
    while(1);
    return -1;
  }

  MmioWrite32(0x04020000 + 0x000C, 0x2); // Set bus width to 8
  return 0;
}

//STATIC
//INT
//MMCTransDataByCPU

#define SUNXI_MMC_RINT_RESP_ERROR		(0x1 << 1)
#define SUNXI_MMC_RINT_COMMAND_DONE		(0x1 << 2)
#define SUNXI_MMC_RINT_DATA_OVER		(0x1 << 3)
#define SUNXI_MMC_RINT_TX_DATA_REQUEST		(0x1 << 4)
#define SUNXI_MMC_RINT_RX_DATA_REQUEST		(0x1 << 5)
#define SUNXI_MMC_RINT_RESP_CRC_ERROR		(0x1 << 6)
#define SUNXI_MMC_RINT_DATA_CRC_ERROR		(0x1 << 7)
#define SUNXI_MMC_RINT_RESP_TIMEOUT		(0x1 << 8)
#define SUNXI_MMC_RINT_DATA_TIMEOUT		(0x1 << 9)
#define SUNXI_MMC_RINT_VOLTAGE_CHANGE_DONE	(0x1 << 10)
#define SUNXI_MMC_RINT_FIFO_RUN_ERROR		(0x1 << 11)
#define SUNXI_MMC_RINT_HARD_WARE_LOCKED		(0x1 << 12)
#define SUNXI_MMC_RINT_START_BIT_ERROR		(0x1 << 13)
#define SUNXI_MMC_RINT_AUTO_COMMAND_DONE	(0x1 << 14)
#define SUNXI_MMC_RINT_END_BIT_ERROR		(0x1 << 15)
#define SUNXI_MMC_RINT_SDIO_INTERRUPT		(0x1 << 16)
#define SUNXI_MMC_RINT_CARD_INSERT		(0x1 << 30)
#define SUNXI_MMC_RINT_CARD_REMOVE		(0x1 << 31)
#define SUNXI_MMC_RINT_INTERRUPT_ERROR_BIT      \
	(SUNXI_MMC_RINT_RESP_ERROR |		\
	 SUNXI_MMC_RINT_RESP_CRC_ERROR |	\
	 SUNXI_MMC_RINT_DATA_CRC_ERROR |	\
	 SUNXI_MMC_RINT_RESP_TIMEOUT |		\
	 SUNXI_MMC_RINT_DATA_TIMEOUT |		\
	 SUNXI_MMC_RINT_VOLTAGE_CHANGE_DONE |	\
	 SUNXI_MMC_RINT_FIFO_RUN_ERROR |	\
	 SUNXI_MMC_RINT_HARD_WARE_LOCKED |	\
	 SUNXI_MMC_RINT_START_BIT_ERROR |	\
	 SUNXI_MMC_RINT_END_BIT_ERROR) /* 0xbfc2 */
#define SUNXI_MMC_RINT_INTERRUPT_DONE_BIT	\
	(SUNXI_MMC_RINT_AUTO_COMMAND_DONE |	\
	 SUNXI_MMC_RINT_DATA_OVER |		\
	 SUNXI_MMC_RINT_COMMAND_DONE |		\
	 SUNXI_MMC_RINT_VOLTAGE_CHANGE_DONE)

STATIC
INT32
MMCRintWait (UINT32 TimeoutMsecs, UINT32 DoneBit)
{
  UINT32 status;
  UINT32 Timeout = TimeoutMsecs;


  do
  {
    status = MmioRead32(0x04020000 + 0x0038);

    if(Timeout <= 0 || (status & SUNXI_MMC_RINT_INTERRUPT_ERROR_BIT))
    {
      DEBUG((EFI_D_ERROR, "CMD TIMEOUT!\n"));
      return -1;
    }

    MicroSecondDelay(1000);
    Timeout--;
  } while(!(status & DoneBit));

  return 0;
}

#define SUNXI_MMC_GCTRL_FIFO_RESET	(0x1 << 1)

STATIC
INT32
SunxiMMCSendCMDCommon (UINT32 cmdID)
{
  UINT32 CMDVal = SUNXI_MMC_CMD_START;
  UINT32 error = 0; // need to impl TimeoutMsecs, status, bytecnt

  if(!cmdID)
    CMDVal |= SUNXI_MMC_CMD_SEND_INIT_SEQ;

  DEBUG((EFI_D_ERROR, "MMC 0, CMD %d", cmdID));

  // Gotta do args later

  MmioWrite32(0x04020000 + 0x0018, CMDVal | cmdID);
  error = MMCRintWait(100, SUNXI_MMC_RINT_COMMAND_DONE);

//  out:
    if(error < 0)
    {
      // reset mmc stuff idk
    }

    MmioWrite32(0x04020000 + 0x0038, 0xFFFFFFFF);
    MmioWrite32(0x04020000, (MmioRead32(0x04020000) | SUNXI_MMC_GCTRL_FIFO_RESET));\

    return error;
}

#define SUNXI_MMC_GCTRL_SOFT_RESET	(0x1 << 0)
#define SUNXI_MMC_GCTRL_FIFO_RESET	(0x1 << 1)
#define SUNXI_MMC_GCTRL_DMA_RESET	(0x1 << 2)
#define SUNXI_MMC_GCTRL_RESET		(SUNXI_MMC_GCTRL_SOFT_RESET|\
					 SUNXI_MMC_GCTRL_FIFO_RESET|\
					 SUNXI_MMC_GCTRL_DMA_RESET)

#define SUNXI_MMC_HWRST		0x078
#define SUNXI_MMC_HWRST_ASSERT		(0x0 << 0)
#define SUNXI_MMC_HWRST_DEASSERT	(0x1 << 0)


#define SUNXI_MMC_THLDC		0x100
#define SUNXI_MMC_THLDC_READ_EN		(0x1 << 0)
#define SUNXI_MMC_THLDC_BSY_CLR_INT_EN	(0x1 << 1)
#define SUNXI_MMC_THLDC_WRITE_EN	(0x1 << 2)
#define SUNXI_MMC_THLDC_READ_THLD(x)	(((x) & 0xfff) << 16)

VOID
SunxiMMCReset()
{
  MmioWrite32(0x04020000, SUNXI_MMC_GCTRL_RESET);
  MicroSecondDelay(1000);

  // Reset Card
  MmioWrite32(0x04020000 + 0x0070, SUNXI_MMC_HWRST_ASSERT);
  MicroSecondDelay(10);
  MmioWrite32(0x04020000 + 0x0070, SUNXI_MMC_HWRST_DEASSERT);
  MicroSecondDelay(300);

  // Setup FIFO R/W threshold. Needed on H616.
  MmioWrite32(0x04020000 + 0x0100, SUNXI_MMC_THLDC_READ_THLD(512) |
				   SUNXI_MMC_THLDC_WRITE_EN |
				   SUNXI_MMC_THLDC_READ_EN);
}

VOID
SunxiMMCCoreInit()
{
  SunxiMMCReset();
}

#define CCU_H6_MMC_GATE_RESET		0x84c
#define RESET_SHIFT			(16)

VOID
InitSD ()
{

  DEBUG((EFI_D_ERROR, "Init MMC 0 Clock and IO\n"));
  MmioWrite32(0x03001000 + CCU_H6_MMC_GATE_RESET, (MmioRead32(0x03001000 + CCU_H6_MMC_GATE_RESET) | 1));
  MmioWrite32(0x03001000 + CCU_H6_MMC_GATE_RESET, (MmioRead32(0x03001000 + CCU_H6_MMC_GATE_RESET) | (1 << 16)));

  SunxiMMCCoreInit();

  MMCConfigClock();

  SunxiMMCSendCMDCommon(0);

  SunxiMMCSetIOsCommon();

  DumpResponseRegisters();

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
