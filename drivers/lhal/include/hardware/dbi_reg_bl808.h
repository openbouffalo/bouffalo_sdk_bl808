/**
  ******************************************************************************
  * @file    dbi_reg_bl808.h
  * @version V1.0
  * @date    2023-02-08
  * @brief   This file is the description of the BL808 DBI register map
  *
  * The BL808 DBI peripheral uses the legacy register layout, which differs
  * from the newer BL616-style layout described in dbi_reg.h:
  *
  *   - command phase: command value lives in CONFIG bits [15:8],
  *     enabled by CONFIG bit 2 (there is no separate command register)
  *   - normal data:   up to 4 bytes, written to WDATA, count in CONFIG [7:6]
  *   - interrupt status at 0x04, bus busy at 0x08, pixel count at 0x0C,
  *     clock period at 0x10, WDATA at 0x18, RDATA at 0x1C
  *   - no QSPI mode, no YUV-to-RGB converter
  *
  * The definitions can be compared with the old BL808 StdDriver register
  * header (bl808_std BL808_BSP_Driver/mm_reg/dbi_reg.h).
  ******************************************************************************
  */
#ifndef __DBI_REG_BL808_H__
#define __DBI_REG_BL808_H__

/* Register offsets **********************************************************/

#define DBI_CONFIG_OFFSET           (0x0)  /* dbi_config */
#define DBI_INT_STS_OFFSET          (0x4)  /* dbi_int_sts */
#define DBI_BUS_BUSY_OFFSET         (0x8)  /* dbi_bus_busy */
#define DBI_PIX_CNT_OFFSET          (0xC)  /* dbi_pix_cnt */
#define DBI_PRD_OFFSET              (0x10) /* dbi_prd */
#define DBI_WDATA_OFFSET            (0x18) /* dbi_wdata */
#define DBI_RDATA_OFFSET            (0x1C) /* dbi_rdata */
#define DBI_FIFO_CONFIG_0_OFFSET    (0x80) /* dbi_fifo_config_0 */
#define DBI_FIFO_CONFIG_1_OFFSET    (0x84) /* dbi_fifo_config_1 */
#define DBI_FIFO_WDATA_OFFSET       (0x88) /* dbi_fifo_wdata */

/* 0x0 : dbi_config */
#define DBI_CR_DBI_EN               (1 << 0U)
#define DBI_CR_DBI_SEL              (1 << 1U)
#define DBI_CR_DBI_CMD_EN           (1 << 2U)
#define DBI_CR_DBI_DAT_EN           (1 << 3U)
#define DBI_CR_DBI_DAT_WR           (1 << 4U)
#define DBI_CR_DBI_DAT_TP           (1 << 5U)
#define DBI_CR_DBI_DAT_BC_SHIFT     (6U)
#define DBI_CR_DBI_DAT_BC_MASK      (0x3 << DBI_CR_DBI_DAT_BC_SHIFT)
#define DBI_CR_DBI_CMD_SHIFT        (8U)
#define DBI_CR_DBI_CMD_MASK         (0xff << DBI_CR_DBI_CMD_SHIFT)
#define DBI_CR_DBI_SCL_POL          (1 << 16U)
#define DBI_CR_DBI_SCL_PH           (1 << 17U)
#define DBI_CR_DBI_CONT_EN          (1 << 18U)
#define DBI_CR_DBI_DMY_EN           (1 << 19U)
#define DBI_CR_DBI_DMY_CNT_SHIFT    (20U)
#define DBI_CR_DBI_DMY_CNT_MASK     (0xf << DBI_CR_DBI_DMY_CNT_SHIFT)
#define DBI_CR_DBI_TC_3W_MODE       (1 << 27U)

/* 0x4 : dbi_int_sts */
#define DBI_END_INT                 (1 << 0U)
#define DBI_TXF_INT                 (1 << 1U)
#define DBI_FER_INT                 (1 << 2U)
#define DBI_CR_DBI_END_MASK         (1 << 8U)
#define DBI_CR_DBI_TXF_MASK         (1 << 9U)
#define DBI_CR_DBI_FER_MASK         (1 << 10U)
#define DBI_CR_DBI_END_CLR          (1 << 16U)
#define DBI_CR_DBI_END_EN           (1 << 24U)
#define DBI_CR_DBI_TXF_EN           (1 << 25U)
#define DBI_CR_DBI_FER_EN           (1 << 26U)

/* 0x8 : dbi_bus_busy */
#define DBI_STS_DBI_BUS_BUSY        (1 << 0U)

/* 0xC : dbi_pix_cnt */
#define DBI_CR_DBI_PIX_CNT_SHIFT    (0U)
#define DBI_CR_DBI_PIX_CNT_MASK     (0xffffff << DBI_CR_DBI_PIX_CNT_SHIFT)
#define DBI_CR_DBI_PIX_FORMAT       (1 << 31U)

/* 0x10 : dbi_prd */
#define DBI_CR_DBI_PRD_S_SHIFT      (0U)
#define DBI_CR_DBI_PRD_S_MASK       (0xff << DBI_CR_DBI_PRD_S_SHIFT)
#define DBI_CR_DBI_PRD_I_SHIFT      (8U)
#define DBI_CR_DBI_PRD_I_MASK       (0xff << DBI_CR_DBI_PRD_I_SHIFT)
#define DBI_CR_DBI_PRD_D_PH_0_SHIFT (16U)
#define DBI_CR_DBI_PRD_D_PH_0_MASK  (0xff << DBI_CR_DBI_PRD_D_PH_0_SHIFT)
#define DBI_CR_DBI_PRD_D_PH_1_SHIFT (24U)
#define DBI_CR_DBI_PRD_D_PH_1_MASK  (0xff << DBI_CR_DBI_PRD_D_PH_1_SHIFT)

/* 0x18 : dbi_wdata */
#define DBI_CR_DBI_WDATA            (1 << 0U)

/* 0x1C : dbi_rdata */
#define DBI_STS_DBI_RDATA           (1 << 0U)

/* 0x80 : dbi_fifo_config_0 */
#define DBI_DMA_TX_EN               (1 << 0U)
#define DBI_TX_FIFO_CLR             (1 << 2U)
#define DBI_TX_FIFO_OVERFLOW        (1 << 4U)
#define DBI_TX_FIFO_UNDERFLOW       (1 << 5U)
#define DBI_FIFO_FORMAT_SHIFT       (29U)
#define DBI_FIFO_FORMAT_MASK        (0x7 << DBI_FIFO_FORMAT_SHIFT)

/* 0x84 : dbi_fifo_config_1 */
#define DBI_TX_FIFO_CNT_SHIFT       (0U)
#define DBI_TX_FIFO_CNT_MASK        (0xf << DBI_TX_FIFO_CNT_SHIFT)
#define DBI_TX_FIFO_TH_SHIFT        (16U)
#define DBI_TX_FIFO_TH_MASK         (0x7 << DBI_TX_FIFO_TH_SHIFT)

/* 0x88 : dbi_fifo_wdata */
#define DBI_FIFO_WDATA              (1 << 0U)

#endif /* __DBI_REG_BL808_H__ */
