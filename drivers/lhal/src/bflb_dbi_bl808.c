/**
  ******************************************************************************
  * @file    bflb_dbi_bl808.c
  * @brief   LHAL DBI driver for BL808.
  *
  * The BL808 DBI peripheral uses the legacy register layout (see
  * hardware/dbi_reg_bl808.h).  This driver is the BL808 equivalent of
  * bflb_dbi.c, based on the old BL808 StdDriver:
  *   - the command phase is configured in CONFIG bits [15:8] with CMD_EN
  *     (bit 2), there is no separate command register
  *   - normal data is limited to 4 bytes and is written to WDATA,
  *     longer parameter blocks are split into 4-byte transfers with the
  *     CS line kept asserted (continuous-CS mode)
  *   - pixel data is written through the tx fifo
  ******************************************************************************
  */
#include "bflb_dbi.h"
#include "bflb_clock.h"
#include "hardware/dbi_reg_bl808.h"

static void bflb_dbi_format_set(struct bflb_device_s *dev, uint8_t format)
{
    uint32_t reg_base;
    uint32_t regval;

    reg_base = dev->reg_base;

    regval = getreg32(reg_base + DBI_FIFO_CONFIG_0_OFFSET);
    regval &= ~DBI_FIFO_FORMAT_MASK;
    regval |= (format << DBI_FIFO_FORMAT_SHIFT) & DBI_FIFO_FORMAT_MASK;
    putreg32(regval, reg_base + DBI_FIFO_CONFIG_0_OFFSET);
}

void bflb_dbi_init(struct bflb_device_s *dev, const struct bflb_dbi_config_s *config)
{
    LHAL_PARAM_ASSERT(dev);
    LHAL_PARAM_ASSERT(IS_DBI_MODE(config->dbi_mode));
    LHAL_PARAM_ASSERT(IS_DBI_PIXEL_INPUT_FORMAT(config->pixel_input_format));
    LHAL_PARAM_ASSERT(IS_DBI_PIXEL_OUTPUT_FORMAT(config->pixel_output_format));
    LHAL_PARAM_ASSERT(IS_DBI_CLOCK_MODE(config->clk_mode));
    LHAL_PARAM_ASSERT(IS_DBI_THRESHLOD(config->tx_fifo_threshold));

    uint32_t reg_base;
    uint32_t regval;
    uint32_t div, div_0, div_1;

    reg_base = dev->reg_base;

    regval = getreg32(reg_base + DBI_CONFIG_OFFSET);

    /* disable DBI transaction */
    regval &= ~DBI_CR_DBI_EN;
    putreg32(regval, reg_base + DBI_CONFIG_OFFSET);

    /* dbi work mode select */
    if (config->dbi_mode == DBI_MODE_TYPE_B) {
        regval &= ~DBI_CR_DBI_SEL;
        regval &= ~DBI_CR_DBI_TC_3W_MODE;
    } else if (config->dbi_mode == DBI_MODE_TYPE_C_3_WIRE) {
        regval |= DBI_CR_DBI_SEL;
        regval |= DBI_CR_DBI_TC_3W_MODE;
    } else { /* DBI_MODE_TYPE_C_4_WIRE */
        regval |= DBI_CR_DBI_SEL;
        regval &= ~DBI_CR_DBI_TC_3W_MODE;
    }

    /* clock phase and polarity cfg */
    switch (config->clk_mode) {
        case DBI_CLOCK_MODE_0:
            /* CPOL=0 CHPHA=0 */
            regval &= ~DBI_CR_DBI_SCL_POL;
            regval |= DBI_CR_DBI_SCL_PH;
            break;
        case DBI_CLOCK_MODE_1:
            /* CPOL=0 CHPHA=1 */
            regval &= ~DBI_CR_DBI_SCL_POL;
            regval &= ~DBI_CR_DBI_SCL_PH;
            break;
        case DBI_CLOCK_MODE_2:
            /* CPOL=1 CHPHA=0 */
            regval |= DBI_CR_DBI_SCL_POL;
            regval |= DBI_CR_DBI_SCL_PH;
            break;
        case DBI_CLOCK_MODE_3:
            /* CPOL=1 CHPHA=1 */
            regval |= DBI_CR_DBI_SCL_POL;
            regval &= ~DBI_CR_DBI_SCL_PH;
            break;
        default:
            break;
    }

    /* disable pixel data continuous transfer mode (CS) */
    regval &= ~DBI_CR_DBI_CONT_EN;

    /* disable dummy between command phase and data phase */
    regval &= ~DBI_CR_DBI_DMY_EN;

    /* enable command phase */
    regval |= DBI_CR_DBI_CMD_EN;
    putreg32(regval, reg_base + DBI_CONFIG_OFFSET);

    /* clock cfg */
    /* integer frequency segmentation by rounding */
    div = (bflb_clk_get_peripheral_clock(BFLB_DEVICE_TYPE_DBI, dev->idx) * 10 / config->clk_freq_hz + 5) / 10;
    div_0 = (div + 1) / 2;
    div_0 = (div_0 > 0xff) ? (0xff) : ((div_0 > 0) ? (div_0 - 1) : 0);
    div_1 = (div) / 2;
    div_1 = (div_1 > 0xff) ? (0xff) : ((div_1 > 0) ? (div_1 - 1) : 0);

    div = (div > 0xff) ? 0xff : div;
    regval = 0;
    regval |= div_0 << DBI_CR_DBI_PRD_S_SHIFT;
    regval |= div_1 << DBI_CR_DBI_PRD_I_SHIFT;
    regval |= div_0 << DBI_CR_DBI_PRD_D_PH_0_SHIFT;
    regval |= div_1 << DBI_CR_DBI_PRD_D_PH_1_SHIFT;
    putreg32(regval, reg_base + DBI_PRD_OFFSET);

    /* dbi output pixel format cfg */
    regval = getreg32(reg_base + DBI_PIX_CNT_OFFSET);
    if (config->pixel_output_format == DBI_PIXEL_OUTPUT_FORMAT_RGB_565) {
        regval &= ~DBI_CR_DBI_PIX_FORMAT;
    } else {
        regval |= DBI_CR_DBI_PIX_FORMAT;
    }
    putreg32(regval, reg_base + DBI_PIX_CNT_OFFSET);

    /* dbi input pixel format */
    bflb_dbi_format_set(dev, config->pixel_input_format);

    /* clear fifo */
    regval = getreg32(reg_base + DBI_FIFO_CONFIG_0_OFFSET);
    regval |= DBI_TX_FIFO_CLR;

    /* disable dma mode */
    regval &= ~DBI_DMA_TX_EN;
    putreg32(regval, reg_base + DBI_FIFO_CONFIG_0_OFFSET);

    /* tx fifo threshold cfg */
    regval = getreg32(reg_base + DBI_FIFO_CONFIG_1_OFFSET);
    regval &= ~DBI_TX_FIFO_TH_MASK;
    regval |= (config->tx_fifo_threshold << DBI_TX_FIFO_TH_SHIFT) & DBI_TX_FIFO_TH_MASK;
    putreg32(regval, reg_base + DBI_FIFO_CONFIG_1_OFFSET);
}

void bflb_dbi_deinit(struct bflb_device_s *dev)
{
    uint32_t reg_base;
    uint32_t regval;

    reg_base = dev->reg_base;

    /* disable DBI transaction */
    regval = getreg32(reg_base + DBI_CONFIG_OFFSET);
    regval &= ~DBI_CR_DBI_EN;
    putreg32(regval, reg_base + DBI_CONFIG_OFFSET);

    /* clear fifo */
    regval = getreg32(reg_base + DBI_FIFO_CONFIG_0_OFFSET);
    regval |= DBI_TX_FIFO_CLR;
    putreg32(regval, reg_base + DBI_FIFO_CONFIG_0_OFFSET);
}

static void bflb_dbi808_int_clear(uint32_t reg_base)
{
    uint32_t regval;

    regval = getreg32(reg_base + DBI_INT_STS_OFFSET);
    regval |= DBI_CR_DBI_END_CLR;
    putreg32(regval, reg_base + DBI_INT_STS_OFFSET);
}

static void bflb_dbi_transfer_end_wait(uint32_t reg_base)
{
    uint32_t regval;

    /* Wait transfer complete */
    do {
        regval = getreg32(reg_base + DBI_INT_STS_OFFSET);
    } while ((regval & DBI_END_INT) == 0);

    /* disable DBI transaction */
    regval = getreg32(reg_base + DBI_CONFIG_OFFSET);
    regval &= ~DBI_CR_DBI_EN;
    putreg32(regval, reg_base + DBI_CONFIG_OFFSET);

    /* clear complete interrupt */
    bflb_dbi808_int_clear(reg_base);
}

static void bflb_dbi_fill_fifo(struct bflb_device_s *dev, uint32_t words_cnt, uint32_t *data_buff)
{
    uint32_t reg_base;
    uint32_t regval;
    uint32_t fifo_cnt;

    reg_base = dev->reg_base;

    for (; words_cnt > 0;) {
        /* get fifo available count */
        regval = getreg32(reg_base + DBI_FIFO_CONFIG_1_OFFSET);
        fifo_cnt = (regval & DBI_TX_FIFO_CNT_MASK) >> DBI_TX_FIFO_CNT_SHIFT;

        if (fifo_cnt) {
            fifo_cnt = (fifo_cnt > words_cnt) ? words_cnt : fifo_cnt;
            words_cnt -= fifo_cnt;
        } else {
            continue;
        }

        /* fill fifo */
        for (; fifo_cnt > 0; fifo_cnt--, data_buff++) {
            putreg32(*data_buff, reg_base + DBI_FIFO_WDATA_OFFSET);
        }
    }
}

static uint32_t bflb_dbi_get_words_cnt_form_pixel(struct bflb_device_s *dev, uint32_t pixle_cnt)
{
    uint32_t reg_base;
    uint32_t regval;
    uint32_t words_cnt;
    uint8_t pixel_input_format;

    reg_base = dev->reg_base;

    /* get fifo input pixel_format */
    regval = getreg32(reg_base + DBI_FIFO_CONFIG_0_OFFSET);
    pixel_input_format = (regval & DBI_FIFO_FORMAT_MASK) >> DBI_FIFO_FORMAT_SHIFT;

    switch (pixel_input_format) {
        /* 32-bit/pixel format list */
        case DBI_PIXEL_INPUT_FORMAT_NBGR_8888:
        case DBI_PIXEL_INPUT_FORMAT_NRGB_8888:
        case DBI_PIXEL_INPUT_FORMAT_BGRN_8888:
        case DBI_PIXEL_INPUT_FORMAT_RGBN_8888:
            words_cnt = pixle_cnt;
            break;

        /* 24-bit/pixel format list */
        case DBI_PIXEL_INPUT_FORMAT_RGB_888:
        case DBI_PIXEL_INPUT_FORMAT_BGR_888:
            words_cnt = (pixle_cnt * 3 + 3) / 4;
            break;

        /* 16-bit/pixel format list */
        case DBI_PIXEL_INPUT_FORMAT_BGR_565:
        case DBI_PIXEL_INPUT_FORMAT_RGB_565:
            words_cnt = (pixle_cnt + 1) / 2;
            break;

        default:
            words_cnt = 0;
            break;
    }

    return words_cnt;
}

int bflb_dbi_send_cmd_data(struct bflb_device_s *dev, uint8_t cmd, uint8_t data_len, uint8_t *data_buff)
{
    uint32_t reg_base;
    uint32_t regval;
    uint32_t wdata;
    uint8_t remain;
    uint8_t chunk;
    uint8_t idx;
    uint8_t first;
    bool cont_en;
    bool cmd_en;

    reg_base = dev->reg_base;

    /* null */
    if (data_len && data_buff == NULL) {
        return 0;
    }

    /* disable DBI transaction */
    regval = getreg32(reg_base + DBI_CONFIG_OFFSET);
    regval &= ~DBI_CR_DBI_EN;
    putreg32(regval, reg_base + DBI_CONFIG_OFFSET);

    if (((regval & DBI_CR_DBI_CMD_EN) == 0) && (data_len == 0)) {
        /* There is no data or command phase, nothing to do */
        return 0;
    }

    cmd_en = ((regval & DBI_CR_DBI_CMD_EN) != 0);

    /* The BL808 can only carry 4 normal data bytes in a single transfer.
       Longer parameter blocks (e.g. 15-byte gamma tables) are sent as a
       series of 4-byte transfers while the CS line stays asserted. */
    cont_en = ((regval & DBI_CR_DBI_CONT_EN) != 0) || (data_len > DBI_WRITE_DATA_BYTE_MAX);
    if (cont_en) {
        regval |= DBI_CR_DBI_CONT_EN;
        putreg32(regval, reg_base + DBI_CONFIG_OFFSET);
    }

    /* normal data mode, write */
    regval &= ~DBI_CR_DBI_DAT_TP;
    regval |= DBI_CR_DBI_DAT_WR;
    putreg32(regval, reg_base + DBI_CONFIG_OFFSET);

    /* send the data in chunks of at most DBI_WRITE_DATA_BYTE_MAX bytes */
    remain = data_len;
    first = 1;
    do {
        chunk = (remain > DBI_WRITE_DATA_BYTE_MAX) ? DBI_WRITE_DATA_BYTE_MAX : remain;
        remain -= chunk;

        /* configure command/data phase */
        regval = getreg32(reg_base + DBI_CONFIG_OFFSET);
        if (chunk) {
            regval |= DBI_CR_DBI_DAT_EN;
            regval &= ~DBI_CR_DBI_DAT_BC_MASK;
            regval |= ((uint32_t)(chunk - 1) << DBI_CR_DBI_DAT_BC_SHIFT) & DBI_CR_DBI_DAT_BC_MASK;
        } else {
            /* command-only transfer */
            regval &= ~DBI_CR_DBI_DAT_EN;
        }
        if (first && (regval & DBI_CR_DBI_CMD_EN) != 0) {
            /* the first transfer carries the command, so the panel sees
               one command followed by the whole data block */
            regval &= ~DBI_CR_DBI_CMD_MASK;
            regval |= (uint32_t)cmd << DBI_CR_DBI_CMD_SHIFT;
        } else {
            /* no command phase on data-only transfers */
            regval &= ~DBI_CR_DBI_CMD_EN;
        }
        putreg32(regval, reg_base + DBI_CONFIG_OFFSET);

        /* pack the chunk into WDATA, little endian */
        if (chunk) {
            wdata = 0;
            for (idx = 0; idx < chunk; idx++) {
                uint8_t data = *data_buff++;
                // wdata |= ((uint32_t)data) << (8 * idx);
                wdata <<= 8;
                wdata |= data;
            }
            putreg32(wdata, reg_base + DBI_WDATA_OFFSET);
        }

        /* clear complete interrupt */
        bflb_dbi808_int_clear(reg_base);

        /* trigger the transaction */
        regval = getreg32(reg_base + DBI_CONFIG_OFFSET);
        regval |= DBI_CR_DBI_EN;
        putreg32(regval, reg_base + DBI_CONFIG_OFFSET);

        bflb_dbi_transfer_end_wait(reg_base);

        first = 0;
    } while (remain);

    /* restore command phase enable and continuous-CS state */
    regval = getreg32(reg_base + DBI_CONFIG_OFFSET);
    if (cmd_en) {
        regval |= DBI_CR_DBI_CMD_EN;
    } else {
        regval &= ~DBI_CR_DBI_CMD_EN;
    }
    if (!cont_en) {
        regval &= ~DBI_CR_DBI_CONT_EN;
    }
    putreg32(regval, reg_base + DBI_CONFIG_OFFSET);

    return 0;
}

int bflb_dbi_send_cmd_read_data(struct bflb_device_s *dev, uint8_t cmd, uint8_t data_len, uint8_t *data_buff)
{
    uint32_t reg_base;
    uint32_t regval;

    reg_base = dev->reg_base;

    /* disable DBI transaction */
    regval = getreg32(reg_base + DBI_CONFIG_OFFSET);
    regval &= ~DBI_CR_DBI_EN;
    putreg32(regval, reg_base + DBI_CONFIG_OFFSET);

    if (((regval & DBI_CR_DBI_CMD_EN) == 0) && (data_len == 0)) {
        /* There is no data or command phase, nothing to do */
        return 0;
    }

    /* normal data mode, read */
    regval &= ~DBI_CR_DBI_DAT_TP;
    regval &= ~DBI_CR_DBI_DAT_WR;
    if (data_len) {
        /* BL808 read is limited to 4 bytes */
        if (data_len > DBI_READ_DATA_BYTE_MAX) {
            data_len = DBI_READ_DATA_BYTE_MAX;
        }
        regval |= DBI_CR_DBI_DAT_EN;
        regval &= ~DBI_CR_DBI_DAT_BC_MASK;
        regval |= ((uint32_t)(data_len - 1) << DBI_CR_DBI_DAT_BC_SHIFT) & DBI_CR_DBI_DAT_BC_MASK;
    } else {
        regval &= ~DBI_CR_DBI_DAT_EN;
    }

    /* set command */
    if (regval & DBI_CR_DBI_CMD_EN) {
        regval &= ~DBI_CR_DBI_CMD_MASK;
        regval |= (uint32_t)cmd << DBI_CR_DBI_CMD_SHIFT;
    }
    putreg32(regval, reg_base + DBI_CONFIG_OFFSET);

    /* clear complete interrupt */
    bflb_dbi808_int_clear(reg_base);

    /* trigger the transaction */
    regval = getreg32(reg_base + DBI_CONFIG_OFFSET);
    regval |= DBI_CR_DBI_EN;
    putreg32(regval, reg_base + DBI_CONFIG_OFFSET);

    bflb_dbi_transfer_end_wait(reg_base);

    /* copy data to buff, discard data if null */
    if (data_buff != NULL) {
        regval = getreg32(reg_base + DBI_RDATA_OFFSET);
        for (uint8_t i = 0; (data_len > 0) && (i < DBI_READ_DATA_BYTE_MAX); i++, data_len--, data_buff++) {
            *data_buff = (uint8_t)(regval >> (8 * i));
        }
    }

    return 0;
}

/**
 * Configure the command/pixel-count/pixel-mode registers and arm the TX
 * FIFO for a pixel transfer, without asserting DBI_CR_DBI_EN. Splitting
 * this out from the actual trigger lets DMA-driven transfers get the FIFO
 * pre-loaded (or at least the DMA request already latched) before the
 * DBI shift engine is told to start clocking data out. Triggering EN
 * before any data is available makes the engine clock out whatever
 * garbage is sitting in the just-cleared FIFO.
 *
 * Returns false if there is nothing to send (no command phase and no
 * pixel data), in which case the caller must not trigger the transaction.
 */
bool bflb_dbi_pixel_transfer_prepare(struct bflb_device_s *dev, uint8_t cmd, uint32_t pixel_cnt)
{
    uint32_t reg_base;
    uint32_t regval;

    reg_base = dev->reg_base;

    /* disable DBI transaction */
    regval = getreg32(reg_base + DBI_CONFIG_OFFSET);
    regval &= ~DBI_CR_DBI_EN;
    putreg32(regval, reg_base + DBI_CONFIG_OFFSET);

    if (((regval & DBI_CR_DBI_CMD_EN) == 0) && (pixel_cnt == 0)) {
        /* There is no data or command phase, nothing to do */
        return false;
    }

    /* pixel mode, write */
    regval |= DBI_CR_DBI_DAT_TP;
    regval |= DBI_CR_DBI_DAT_WR;

    /* pixel data phase enable */
    if (pixel_cnt) {
        regval |= DBI_CR_DBI_DAT_EN;
    } else {
        regval &= ~DBI_CR_DBI_DAT_EN;
    }

    /* set command */
    if (regval & DBI_CR_DBI_CMD_EN) {
        regval &= ~DBI_CR_DBI_CMD_MASK;
        regval |= (uint32_t)cmd << DBI_CR_DBI_CMD_SHIFT;
    }
    putreg32(regval, reg_base + DBI_CONFIG_OFFSET);

    /* pixel cnt */
    if (pixel_cnt) {
        regval = getreg32(reg_base + DBI_PIX_CNT_OFFSET);
        regval &= ~DBI_CR_DBI_PIX_CNT_MASK;
        regval |= ((uint32_t)(pixel_cnt - 1) << DBI_CR_DBI_PIX_CNT_SHIFT) & DBI_CR_DBI_PIX_CNT_MASK;
        putreg32(regval, reg_base + DBI_PIX_CNT_OFFSET);
    }

    /* clear fifo */
    regval = getreg32(reg_base + DBI_FIFO_CONFIG_0_OFFSET);
    regval |= DBI_TX_FIFO_CLR;
    putreg32(regval, reg_base + DBI_FIFO_CONFIG_0_OFFSET);

    /* clear complete interrupt */
    bflb_dbi808_int_clear(reg_base);

    return true;
}

/**
 * Assert DBI_CR_DBI_EN to actually start clocking out the configured
 * command/pixel transaction. Must be called after
 * bflb_dbi_pixel_transfer_prepare(), and for DMA-driven transfers, only
 * after the DMA channel has been started so the FIFO already has (or is
 * actively receiving) valid data.
 */
void bflb_dbi_pixel_transfer_start(struct bflb_device_s *dev)
{
    uint32_t reg_base;
    uint32_t regval;

    reg_base = dev->reg_base;

    regval = getreg32(reg_base + DBI_CONFIG_OFFSET);
    regval |= DBI_CR_DBI_EN;
    putreg32(regval, reg_base + DBI_CONFIG_OFFSET);
}

int bflb_dbi_send_cmd_pixel(struct bflb_device_s *dev, uint8_t cmd, uint32_t pixel_cnt, void *pixel_buff)
{
    uint32_t reg_base;

    reg_base = dev->reg_base;

    if (!bflb_dbi_pixel_transfer_prepare(dev, cmd, pixel_cnt)) {
        return 0;
    }

    /* trigger the transaction */
    bflb_dbi_pixel_transfer_start(dev);

    /* No need to fill in fifo, for DMA mode */
    if (pixel_buff == NULL) {
        return 0;
    }

    /* fill the data into the fifo, can only be used in non-DMA mode */
    bflb_dbi_fill_fifo(dev, bflb_dbi_get_words_cnt_form_pixel(dev, pixel_cnt), (uint32_t *)pixel_buff);

    bflb_dbi_transfer_end_wait(reg_base);

    return 0;
}

void bflb_dbi_link_txdma(struct bflb_device_s *dev, bool enable)
{
    printf("bflb_dbi_link_txdma - en %d\n", enable ? 1 : 0);
    uint32_t reg_base;
    uint32_t regval;

    reg_base = dev->reg_base;

    regval = getreg32(reg_base + DBI_FIFO_CONFIG_0_OFFSET);
    if (enable) {
        regval |= DBI_DMA_TX_EN;
    } else {
        regval &= ~DBI_DMA_TX_EN;
    }
    putreg32(regval, reg_base + DBI_FIFO_CONFIG_0_OFFSET);
}

void bflb_dbi_txint_mask(struct bflb_device_s *dev, bool mask)
{
    uint32_t reg_base;
    uint32_t regval;

    reg_base = dev->reg_base;

    regval = getreg32(reg_base + DBI_INT_STS_OFFSET);
    if (mask) {
        regval |= DBI_CR_DBI_TXF_MASK;
    } else {
        regval &= ~DBI_CR_DBI_TXF_MASK;
    }
    putreg32(regval, reg_base + DBI_INT_STS_OFFSET);
}

void bflb_dbi_tcint_mask(struct bflb_device_s *dev, bool mask)
{
    uint32_t reg_base;
    uint32_t regval;

    reg_base = dev->reg_base;

    regval = getreg32(reg_base + DBI_INT_STS_OFFSET);
    if (mask) {
        regval |= DBI_CR_DBI_END_MASK;
    } else {
        regval &= ~DBI_CR_DBI_END_MASK;
    }
    putreg32(regval, reg_base + DBI_INT_STS_OFFSET);
}

void bflb_dbi_errint_mask(struct bflb_device_s *dev, bool mask)
{
    uint32_t reg_base;
    uint32_t regval;

    reg_base = dev->reg_base;

    regval = getreg32(reg_base + DBI_INT_STS_OFFSET);
    if (mask) {
        regval |= DBI_CR_DBI_FER_MASK;
    } else {
        regval &= ~DBI_CR_DBI_FER_MASK;
    }
    putreg32(regval, reg_base + DBI_INT_STS_OFFSET);
}

uint32_t bflb_dbi_get_intstatus(struct bflb_device_s *dev)
{
    uint32_t reg_base;
    uint32_t regval;
    uint32_t int_sts;

    reg_base = dev->reg_base;
    int_sts = 0;

    regval = getreg32(reg_base + DBI_INT_STS_OFFSET);

    /* transfer completion interrupt */
    if (regval & DBI_END_INT) {
        int_sts |= DBI_INTSTS_TC;
    }

    /* fifo threshold interrupt */
    if (regval & DBI_TXF_INT) {
        int_sts |= DBI_INTSTS_TX_FIFO;
    }

    /* fifo error (underflow or overflow) interrupt */
    if (regval & DBI_FER_INT) {
        int_sts |= DBI_INTSTS_FIFO_ERR;
    }

    return int_sts;
}

void bflb_dbi_int_clear(struct bflb_device_s *dev, uint32_t int_clear)
{
    uint32_t reg_base;
    uint32_t regval;

    reg_base = dev->reg_base;

    regval = getreg32(reg_base + DBI_INT_STS_OFFSET);

    /* transfer completion interrupt */
    if (int_clear & DBI_INTCLR_TC) {
        regval |= DBI_CR_DBI_END_CLR;
    }

    putreg32(regval, reg_base + DBI_INT_STS_OFFSET);
}

int bflb_dbi_feature_control(struct bflb_device_s *dev, int cmd, size_t arg)
{
    int ret = 0;
    uint32_t reg_base;
    uint32_t regval;

    reg_base = dev->reg_base;

    switch (cmd) {
        case DBI_CMD_CLEAR_TX_FIFO:
            /* clear fifo */
            regval = getreg32(reg_base + DBI_FIFO_CONFIG_0_OFFSET);
            regval |= DBI_TX_FIFO_CLR;
            putreg32(regval, reg_base + DBI_FIFO_CONFIG_0_OFFSET);
            break;

        case DBI_CMD_GET_TX_FIFO_CNT:
            /* get fifo available count */
            regval = getreg32(reg_base + DBI_FIFO_CONFIG_1_OFFSET);
            ret = (regval & DBI_TX_FIFO_CNT_MASK) >> DBI_TX_FIFO_CNT_SHIFT;
            break;

        case DBI_CMD_MASK_CMD_PHASE:
            /* mask command phase, arg use true or false,
               true: no command phase, false: command will be sent */
            regval = getreg32(reg_base + DBI_CONFIG_OFFSET);
            if (arg) {
                regval &= ~DBI_CR_DBI_CMD_EN;
            } else {
                regval |= DBI_CR_DBI_CMD_EN;
            }
            putreg32(regval, reg_base + DBI_CONFIG_OFFSET);
            break;

        case DBI_CMD_CS_CONTINUE:
            /* set CS continue mode, arg use true or false */
            regval = getreg32(reg_base + DBI_CONFIG_OFFSET);
            if (arg) {
                regval |= DBI_CR_DBI_CONT_EN;
            } else {
                regval &= ~DBI_CR_DBI_CONT_EN;
            }
            putreg32(regval, reg_base + DBI_CONFIG_OFFSET);
            break;

        case DBI_CMD_SET_DUMMY_CNT:
            /* set dummy cycle(s) between command phase and data phase,
               arg range: 0 ~ 16 */
            regval = getreg32(reg_base + DBI_CONFIG_OFFSET);
            if (arg) {
                regval |= DBI_CR_DBI_DMY_EN;
                regval &= ~DBI_CR_DBI_DMY_CNT_MASK;
                regval |= (((arg - 1) & 0xf) << DBI_CR_DBI_DMY_CNT_SHIFT) & DBI_CR_DBI_DMY_CNT_MASK;
            } else {
                regval &= ~DBI_CR_DBI_DMY_EN;
            }
            putreg32(regval, reg_base + DBI_CONFIG_OFFSET);
            break;

        case DBI_CMD_GET_SIZE_OF_PIXEL_CNT:
            /* gets the pixel_data size(byte), arg: pixel number */
            ret = bflb_dbi_get_words_cnt_form_pixel(dev, arg) * 4;
            break;

        case DBI_CMD_INPUT_PIXEL_FORMAT:
            /* dbi input pixel format, arg use @ref DBI_PIXEL_INPUT_FORMAT */
            bflb_dbi_format_set(dev, arg);
            break;

        case DBI_CMD_OUTPUT_PIXEL_FORMAT:
            /* dbi output pixel format, arg use @ref DBI_PIXEL_OUTPUT_FORMAT */
            regval = getreg32(reg_base + DBI_PIX_CNT_OFFSET);
            if (arg == DBI_PIXEL_OUTPUT_FORMAT_RGB_565) {
                regval &= ~DBI_CR_DBI_PIX_FORMAT;
            } else if (arg == DBI_PIXEL_OUTPUT_FORMAT_RGB_888) {
                regval |= DBI_CR_DBI_PIX_FORMAT;
            }
            putreg32(regval, reg_base + DBI_PIX_CNT_OFFSET);
            break;

        default:
            ret = -EPERM;
            break;
    }

    return ret;
}
