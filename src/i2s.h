#ifndef I2S_H
#define I2S_H

#include <stdint.h>
#include "audio_buf.h"

/*
 * STM32F407 I2S2 register base address.
 * In QEMU these map to the SPI2/I2S2 peripheral block.
 */
#define I2S2_BASE       0x40003800UL

/* Register offsets from base */
#define I2S_CR1         (*(volatile uint32_t *)(I2S2_BASE + 0x00))
#define I2S_CR2         (*(volatile uint32_t *)(I2S2_BASE + 0x04))
#define I2S_SR          (*(volatile uint32_t *)(I2S2_BASE + 0x08))
#define I2S_DR          (*(volatile uint32_t *)(I2S2_BASE + 0x0C))
#define I2S_I2SCFGR     (*(volatile uint32_t *)(I2S2_BASE + 0x1C))
#define I2S_I2SPR       (*(volatile uint32_t *)(I2S2_BASE + 0x20))

/* I2SCFGR bit positions */
#define I2SCFGR_I2SMOD      (1 << 11)   /* I2S mode enable        */
#define I2SCFGR_I2SE        (1 << 10)   /* I2S peripheral enable  */
#define I2SCFGR_I2SCFG_TX   (2 << 8)    /* Master transmit        */
#define I2SCFGR_DATLEN_16   (0 << 1)    /* 16-bit data            */
#define I2SCFGR_CHLEN_16    (0 << 0)    /* 16-bit channel frame   */

/* CR2 bit positions */
#define CR2_TXDMAEN         (1 << 1)    /* TX DMA enable          */

/* SR bit positions */
#define SR_TXE              (1 << 1)    /* TX buffer empty        */
#define SR_BSY              (1 << 7)    /* Busy flag              */

/* Return codes */
typedef enum {
    I2S_OK      =  0,
    I2S_ERR_BUSY = -1,
    I2S_ERR_TIMEOUT = -2,
} I2SStatus;

/* Driver state */
typedef struct {
    uint8_t   initialised;
    uint32_t  sample_rate;
    AudioBuf *buf;
} I2SDriver;

/* Initialise I2S peripheral and DMA, bind audio buffer */
I2SStatus i2s_init(I2SDriver *drv, uint32_t sample_rate, AudioBuf *buf);

/* Start streaming — DMA takes over from here */
I2SStatus i2s_start(I2SDriver *drv);

/* Stop streaming */
void      i2s_stop(I2SDriver *drv);

/* Write one stereo sample directly (used in polling/test mode) */
I2SStatus i2s_write_sample(I2SDriver *drv, int16_t left, int16_t right);

/* DMA complete callback — call this from your DMA ISR */
void      i2s_dma_callback(I2SDriver *drv);

#endif /* I2S_H */
