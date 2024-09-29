#include <Library/ArmLib.h>
#include <Library/DebugLib.h>
#include <Library/TimerLib.h>

#include "H616-Reg.h"
#include "UBootEnv.h"
#include "SunxiMmc.h"

struct sunxi_mmc_plat {
	struct mmc_config cfg;
	struct mmc mmc;
};

struct sunxi_mmc_priv {
	unsigned mmc_no;
	uint32_t *mclkreg;
	unsigned fatal_err;
	//struct gpio_desc cd_gpio; Broken on zero3, always assume card inserted
	struct sunxi_mmc *reg;
	struct mmc_config cfg;
};

unsigned int clock_get_pll6(void)
{
	struct sunxi_ccm_reg *const ccm = (struct sunxi_ccm_reg *)SUNXI_CCM_BASE;
	uint32_t rval = readl((UINTN)&ccm->pll6_cfg);
	int n = ((rval & CCM_PLL6_CTRL_N_MASK) >> CCM_PLL6_CTRL_N_SHIFT) + 1;
	int div2 = ((rval & CCM_PLL6_CTRL_DIV2_MASK) >>
		    CCM_PLL6_CTRL_DIV2_SHIFT) + 1;
	int div1, m;

	div1 = ((rval & CCM_PLL6_CTRL_DIV1_MASK) >>
		CCM_PLL6_CTRL_DIV1_SHIFT) + 1;

	m = 4;

	return 24000000U * n / m / div1 / div2;
}

static BOOLEAN sunxi_mmc_can_calibrate(void)
{
	return TRUE;
}

static int mmc_set_mod_clk(struct sunxi_mmc_priv *priv, unsigned int hz)
{
	unsigned int pll, pll_hz, div, n, oclk_dly, sclk_dly;
	BOOLEAN new_mode = TRUE;
	u32 val = 0;

	if (hz <= 24000000) {
		pll = CCM_MMC_CTRL_OSCM24;
		pll_hz = 24000000;
	} else {
		/*
		 * SoCs since the A64 (H5, H6, H616) actually use the doubled
		 * rate of PLL6/PERIPH0 as an input clock, but compensate for
		 * that with a fixed post-divider of 2 in the mod clock.
		 * This cancels each other out, so for simplicity we just
		 * pretend it's always PLL6 without a post divider here.
		 */
		pll = CCM_MMC_CTRL_PLL6;
		pll_hz = clock_get_pll6();
	}

	div = pll_hz / hz;
	if (pll_hz % hz)
		div++;

	n = 0;
	while (div > 16) {
		n++;
		div = (div + 1) / 2;
	}

	if (n > 3) {
		printf("mmc %u error cannot set clock to %u\n", priv->mmc_no,
		       hz);
		return -1;
	}

	/* determine delays */
	if (hz <= 400000) {
		oclk_dly = 0;
		sclk_dly = 0;
	} else if (hz <= 25000000) {
		oclk_dly = 0;
		sclk_dly = 5;
	} else {
		if (FALSE) {
			if (hz <= 52000000)
				oclk_dly = 5;
			else
				oclk_dly = 2;
		} else {
			if (hz <= 52000000)
				oclk_dly = 3;
			else
				oclk_dly = 1;
		}
		sclk_dly = 4;
	}

	if (new_mode) {
		val |= CCM_MMC_CTRL_MODE_SEL_NEW;
		setbits_le32((UINTN)&priv->reg->ntsr, SUNXI_MMC_NTSR_MODE_SEL_NEW);
	}

	if (!sunxi_mmc_can_calibrate()) {
		/*
		 * Use hardcoded delay values if controller doesn't support
		 * calibration
		 */
		val = CCM_MMC_CTRL_OCLK_DLY(oclk_dly) |
			CCM_MMC_CTRL_SCLK_DLY(sclk_dly);
	}

	writel(CCM_MMC_CTRL_ENABLE| pll | CCM_MMC_CTRL_N(n) |
	       CCM_MMC_CTRL_M(div) | val, (UINTN)priv->mclkreg);

	debug("mmc %u set mod-clk req %u parent %u n %u m %u rate %u\n",
	      priv->mmc_no, hz, pll_hz, 1u << n, div, pll_hz / (1u << n) / div);

	return 0;
}

static int mmc_update_clk(struct sunxi_mmc_priv *priv)
{
	unsigned int cmd;
	unsigned timeout_microsecs = 2000000;

	cmd = SUNXI_MMC_CMD_START |
	      SUNXI_MMC_CMD_UPCLK_ONLY |
	      SUNXI_MMC_CMD_WAIT_PRE_OVER;

	writel(cmd, (UINTN)&priv->reg->cmd);
	while (readl((UINTN)&priv->reg->cmd) & SUNXI_MMC_CMD_START) {
		if (timeout_microsecs == 0)
			return -1;
		timeout_microsecs--;
		MicroSecondDelay(1);
	}

	/* clock update sets various irq status bits, clear these */
	writel(readl((UINTN)&priv->reg->rint), (UINTN)&priv->reg->rint);

	return 0;
}

static int mmc_config_clock(struct sunxi_mmc_priv *priv, struct mmc *mmc)
{
	unsigned rval = readl((UINTN)&priv->reg->clkcr);

	/* Disable Clock */
	rval &= ~SUNXI_MMC_CLK_ENABLE;
	writel(rval, (UINTN)&priv->reg->clkcr);
	if (mmc_update_clk(priv))
		return -1;

	/* Set mod_clk to new rate */
	if (mmc_set_mod_clk(priv, mmc->clock))
		return -1;

	/* Clear internal divider */
	rval &= ~SUNXI_MMC_CLK_DIVIDER_MASK;
	writel(rval, (UINTN)&priv->reg->clkcr);

	/* A64 supports calibration of delays on MMC controller and we
	 * have to set delay of zero before starting calibration.
	 * Allwinner BSP driver sets a delay only in the case of
	 * using HS400 which is not supported by mainline U-Boot or
	 * Linux at the moment
	 */
	if (sunxi_mmc_can_calibrate())
		writel(SUNXI_MMC_CAL_DL_SW_EN, (UINTN)&priv->reg->samp_dl);

	/* Re-enable Clock */
	rval |= SUNXI_MMC_CLK_ENABLE;
	writel(rval, (UINTN)&priv->reg->clkcr);
	if (mmc_update_clk(priv))
		return -1;

	return 0;
}

static int sunxi_mmc_set_ios_common(struct sunxi_mmc_priv *priv,
				    struct mmc *mmc)
{
	debug("set ios: bus_width: %x, clock: %d\n",
	      mmc->bus_width, mmc->clock);

	/* Change clock first */
	if (mmc->clock && mmc_config_clock(priv, mmc) != 0) {
		priv->fatal_err = 1;
		return -EINVAL;
	}

	/* Change bus width */
	switch(mmc->bus_width) {
	  case 8:
		writel(0x2, (UINTN)&priv->reg->width);
		break;
	  case 4:
		writel(0x1, (UINTN)&priv->reg->width);
		break;
	  default:
		writel(0x0, (UINTN)&priv->reg->width);
		break;
          }

	return 0;
}

static int mmc_trans_data_by_cpu(struct sunxi_mmc_priv *priv, struct mmc *mmc,
				 struct mmc_data *data)
{
	const int reading = !!(data->flags & MMC_DATA_READ);
	const uint32_t status_bit = reading ? SUNXI_MMC_STATUS_FIFO_EMPTY :
					      SUNXI_MMC_STATUS_FIFO_FULL;
	unsigned i;
	unsigned *buff = (unsigned int *)(reading ? data->dest : data->src);
	unsigned word_cnt = (data->blocksize * data->blocks) >> 2;
	unsigned timeout_microsecs = (word_cnt >> 6) * 1000;
	uint32_t status;

	if (timeout_microsecs < 2000000)
		timeout_microsecs = 2000000;

	/* Always read / write data through the CPU */
	setbits_le32((UINTN)&priv->reg->gctrl, SUNXI_MMC_GCTRL_ACCESS_BY_AHB);

	for (i = 0; i < word_cnt;) {
		unsigned int in_fifo;

		while ((status = readl((UINTN)&priv->reg->status)) & status_bit) {
			if (timeout_microsecs == 0)
				return -1;
			timeout_microsecs--;
			MicroSecondDelay(1);
		}

		/*
		 * For writing we do not easily know the FIFO size, so have
		 * to check the FIFO status after every word written.
		 * TODO: For optimisation we could work out a minimum FIFO
		 * size across all SoCs, and use that together with the current
		 * fill level to write chunks of words.
		 */
		if (!reading) {
			writel(buff[i++], (UINTN)&priv->reg->fifo);
			continue;
		}

		/*
		 * The status register holds the current FIFO level, so we
		 * can be sure to collect as many words from the FIFO
		 * register without checking the status register after every
		 * read. That saves half of the costly MMIO reads, effectively
		 * doubling the read performance.
		 * Some SoCs (A20) report a level of 0 if the FIFO is
		 * completely full (value masked out?). Use a safe minimal
		 * FIFO size in this case.
		 */
		in_fifo = SUNXI_MMC_STATUS_FIFO_LEVEL(status);
		if (in_fifo == 0 && (status & SUNXI_MMC_STATUS_FIFO_FULL))
			in_fifo = 32;
		for (; in_fifo > 0; in_fifo--)
			buff[i++] = readl((UINTN)&priv->reg->fifo);
		ArmDataMemoryBarrier();
	}

	return 0;
}

static int mmc_rint_wait(struct sunxi_mmc_priv *priv, struct mmc *mmc,
			 uint timeout_microsecs, uint done_bit, const char *what)
{
	unsigned int status;

	do {
		status = readl((UINTN)&priv->reg->rint);
		if ((timeout_microsecs == 0) ||
		    (status & SUNXI_MMC_RINT_INTERRUPT_ERROR_BIT)) {
			debug("%s timeout %x\n", what,
			      status & SUNXI_MMC_RINT_INTERRUPT_ERROR_BIT);
			return -ETIMEDOUT;
		}
		timeout_microsecs--;
		MicroSecondDelay(1);
	} while (!(status & done_bit));

	return 0;
}

static int sunxi_mmc_send_cmd_common(struct sunxi_mmc_priv *priv,
				     struct mmc *mmc, struct mmc_cmd *cmd,
				     struct mmc_data *data)
{
	unsigned int cmdval = SUNXI_MMC_CMD_START;
	unsigned int timeout_microsecs;
	int error = 0;
	unsigned int status = 0;
	unsigned int bytecnt = 0;

	if (priv->fatal_err)
		return -1;
	if (cmd->resp_type & MMC_RSP_BUSY)
		debug("mmc cmd %d check rsp busy\n", cmd->cmdidx);
	if (cmd->cmdidx == 12)
		return 0;

	if (!cmd->cmdidx)
		cmdval |= SUNXI_MMC_CMD_SEND_INIT_SEQ;
	if (cmd->resp_type & MMC_RSP_PRESENT)
		cmdval |= SUNXI_MMC_CMD_RESP_EXPIRE;
	if (cmd->resp_type & MMC_RSP_136)
		cmdval |= SUNXI_MMC_CMD_LONG_RESPONSE;
	if (cmd->resp_type & MMC_RSP_CRC)
		cmdval |= SUNXI_MMC_CMD_CHK_RESPONSE_CRC;

	if (data) {
		if ((u32)(long)data->dest & 0x3) {
			error = -1;
			goto out;
		}

		cmdval |= SUNXI_MMC_CMD_DATA_EXPIRE|SUNXI_MMC_CMD_WAIT_PRE_OVER;
		if (data->flags & MMC_DATA_WRITE)
			cmdval |= SUNXI_MMC_CMD_WRITE;
		if (data->blocks > 1)
			cmdval |= SUNXI_MMC_CMD_AUTO_STOP;
		writel(data->blocksize, (UINTN)&priv->reg->blksz);
		writel(data->blocks * data->blocksize, (UINTN)&priv->reg->bytecnt);
	}

	debug("mmc %d, cmd %d(0x%08x), arg 0x%08x\n", priv->mmc_no,
	      cmd->cmdidx, cmdval | cmd->cmdidx, cmd->cmdarg);
	writel(cmd->cmdarg, (UINTN)&priv->reg->arg);

	if (!data)
		writel(cmdval | cmd->cmdidx, (UINTN)&priv->reg->cmd);

	/*
	 * transfer data and check status
	 * STATREG[2] : FIFO empty
	 * STATREG[3] : FIFO full
	 */
	if (data) {
		int ret = 0;

		bytecnt = data->blocksize * data->blocks;
		debug("trans data %d bytes\n", bytecnt);
		writel(cmdval | cmd->cmdidx, (UINTN)&priv->reg->cmd);
		ret = mmc_trans_data_by_cpu(priv, mmc, data);
		if (ret) {
			error = readl((UINTN)&priv->reg->rint) &
				SUNXI_MMC_RINT_INTERRUPT_ERROR_BIT;
			error = -ETIMEDOUT;
			goto out;
		}
	}

	error = mmc_rint_wait(priv, mmc, 1000000, SUNXI_MMC_RINT_COMMAND_DONE,
			      "cmd");
	if (error)
		goto out;

	if (data) {
		timeout_microsecs = 120000;
		debug("cacl timeout %x msec\n", timeout_microsecs);
		error = mmc_rint_wait(priv, mmc, timeout_microsecs,
				      data->blocks > 1 ?
				      SUNXI_MMC_RINT_AUTO_COMMAND_DONE :
				      SUNXI_MMC_RINT_DATA_OVER,
				      "data");
		if (error)
			goto out;
	}

	if (cmd->resp_type & MMC_RSP_BUSY) {
		timeout_microsecs = 2000000;

		do {
			status = readl((UINTN)&priv->reg->status);
			if (timeout_microsecs == 0) {
				debug("busy timeout\n");
				error = -ETIMEDOUT;
				goto out;
			}
			timeout_microsecs--;
			MicroSecondDelay(1);
		} while (status & SUNXI_MMC_STATUS_CARD_DATA_BUSY);
	}

	if (cmd->resp_type & MMC_RSP_136) {
		cmd->response[0] = readl((UINTN)&priv->reg->resp3);
		cmd->response[1] = readl((UINTN)&priv->reg->resp2);
		cmd->response[2] = readl((UINTN)&priv->reg->resp1);
		cmd->response[3] = readl((UINTN)&priv->reg->resp0);
		debug("mmc resp 0x%08x 0x%08x 0x%08x 0x%08x\n",
		      cmd->response[3], cmd->response[2],
		      cmd->response[1], cmd->response[0]);
	} else {
		cmd->response[0] = readl((UINTN)&priv->reg->resp0);
		debug("mmc resp 0x%08x\n", cmd->response[0]);
	}
out:
	if (error < 0) {
		writel(SUNXI_MMC_GCTRL_RESET, (UINTN)&priv->reg->gctrl);
		mmc_update_clk(priv);
	}
	writel(0xffffffff, (UINTN)&priv->reg->rint);
	writel(readl((UINTN)&priv->reg->gctrl) | SUNXI_MMC_GCTRL_FIFO_RESET,
	       (UINTN)&priv->reg->gctrl);

	return error;
}

EFI_STATUS
EFIAPI
SdDxeInitialize (
  IN EFI_HANDLE        ImageHandle,
  IN EFI_SYSTEM_TABLE *SystemTable
  )
{

unsigned int h = clock_get_pll6();

DEBUG((EFI_D_ERROR, "heres ur clock, greedy cunt, %u\n", h));

while(1){}
mmc_set_mod_clk(NULL, 1);
mmc_config_clock(NULL, NULL);
sunxi_mmc_set_ios_common(NULL, NULL);
mmc_trans_data_by_cpu(NULL, NULL, NULL);
sunxi_mmc_send_cmd_common(NULL, NULL, NULL, NULL);
return EFI_SUCCESS;
}
