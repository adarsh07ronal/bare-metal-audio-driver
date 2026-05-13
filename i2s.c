#include "i2s.h"
#include <stddef.h>

/* ------------------------------------------------------------------ */
/*  Simple busy-wait timeout helper (counts down, not time-based)     */
/* ------------------------------------------------------------------ */
static int wait_flag_clear(volatile uint32_t *reg, uint32_t mask, uint32_t timeout)
{
    while ((*reg & mask) && timeout--);
    return (timeout == 0) ? -1 : 0;
}

/* ------------------------------------------------------------------ */
/*  i2s_init                                                           */
/*  Sets up I2S2 for 16-bit stereo master TX at the given sample rate */
/* ------------------------------------------------------------------ */
I2SStatus i2s_init(I2SDriver *drv, uint32_t sample_rate, AudioBuf *buf)
{
    if (drv == NULL || buf == NULL) return I2S_ERR_BUSY;

    drv->sample_rate  = sample_rate;
    drv->buf          = buf;
    drv->initialised  = 0;

    /*
     * Configure I2SCFGR:
     *   - I2S mode (not SPI)
     *   - Master transmit
     *   - 16-bit data, 16-bit channel frame
     *   - Philips I2S standard (CKPOL=0, I2SSTD=00)
     */
    I2S_I2SCFGR = I2SCFGR_I2SMOD
                | I2SCFGR_I2SCFG_TX
                | I2SCFGR_DATLEN_16
                | I2SCFGR_CHLEN_16;

    /*
     * I2SPR: prescaler for target sample rate.
     * For 44100 Hz on a 168 MHz system clock the formula is:
     *   I2SDIV = PCLK1 / (256 * fs)  ≈ 12, ODD = 1
     * We hardcode a sane value here; a real driver
     * would compute this from the actual clock tree.
     */
    I2S_I2SPR = (1 << 8) | 12;   /* ODD=1, I2SDIV=12 */

    /* Enable DMA request on TX buffer empty */
    I2S_CR2 = CR2_TXDMAEN;

    drv->initialised = 1;
    return I2S_OK;
}

/* ------------------------------------------------------------------ */
/*  i2s_start                                                          */
/* ------------------------------------------------------------------ */
I2SStatus i2s_start(I2SDriver *drv)
{
    if (!drv->initialised) return I2S_ERR_BUSY;

    /* Enable the peripheral — DMA will now clock data out */
    I2S_I2SCFGR |= I2SCFGR_I2SE;
    return I2S_OK;
}

/* ------------------------------------------------------------------ */
/*  i2s_stop                                                           */
/* ------------------------------------------------------------------ */
void i2s_stop(I2SDriver *drv)
{
    /* Wait for last frame to finish, then disable */
    wait_flag_clear((volatile uint32_t *)(I2S2_BASE + 0x08), SR_BSY, 10000);
    I2S_I2SCFGR &= ~I2SCFGR_I2SE;
    drv->initialised = 0;
}

/* ------------------------------------------------------------------ */
/*  i2s_write_sample  (polling mode — for tests / no-DMA bringup)     */
/* ------------------------------------------------------------------ */
I2SStatus i2s_write_sample(I2SDriver *drv, int16_t left, int16_t right)
{
    if (!drv->initialised) return I2S_ERR_BUSY;

    /* Wait for TX buffer empty — left channel */
    if (wait_flag_clear((volatile uint32_t *)(I2S2_BASE + 0x08),
                        SR_TXE, 10000) < 0)
        return I2S_ERR_TIMEOUT;
    I2S_DR = (uint32_t)(uint16_t)left;

    /* Wait for TX buffer empty — right channel */
    if (wait_flag_clear((volatile uint32_t *)(I2S2_BASE + 0x08),
                        SR_TXE, 10000) < 0)
        return I2S_ERR_TIMEOUT;
    I2S_DR = (uint32_t)(uint16_t)right;

    return I2S_OK;
}

/* ------------------------------------------------------------------ */
/*  i2s_dma_callback  — called from DMA ISR on half/complete          */
/* ------------------------------------------------------------------ */
void i2s_dma_callback(I2SDriver *drv)
{
    if (drv == NULL || drv->buf == NULL) return;
    /* Swap ping/pong so CPU can refill the idle half */
    audio_buf_swap(drv->buf);
}
