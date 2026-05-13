#include <stdint.h>
#include "i2s.h"
#include "audio_buf.h"
#include "sine_gen.h"

/* ------------------------------------------------------------------ */
/*  Minimal vector table — QEMU needs at least Reset_Handler           */
/* ------------------------------------------------------------------ */
extern uint32_t _estack;        /* defined in linker script */

void Reset_Handler(void);
void Default_Handler(void) { while (1); }

/* First 16 entries: ARM Cortex-M core exceptions */
__attribute__((section(".isr_vector")))
void (*const vector_table[])(void) = {
    (void (*)(void))&_estack,   /* Initial stack pointer */
    Reset_Handler,              /* Reset                 */
    Default_Handler,            /* NMI                   */
    Default_Handler,            /* HardFault             */
    Default_Handler,            /* MemManage             */
    Default_Handler,            /* BusFault              */
    Default_Handler,            /* UsageFault            */
};

/* ------------------------------------------------------------------ */
/*  Basic clock init — on QEMU we skip real RCC config                 */
/* ------------------------------------------------------------------ */
static void clock_init(void)
{
    /*
     * On real hardware you would:
     *   1. Enable HSE oscillator
     *   2. Configure PLL for 168 MHz
     *   3. Set AHB/APB prescalers
     *   4. Enable GPIO and peripheral clocks
     *
     * QEMU boots with a default 168 MHz system clock,
     * so we just proceed.
     */
}

/* ------------------------------------------------------------------ */
/*  Reset handler — C runtime startup                                  */
/* ------------------------------------------------------------------ */
extern uint32_t _sidata, _sdata, _edata, _sbss, _ebss;

void Reset_Handler(void)
{
    /* Copy .data section from FLASH to SRAM */
    uint32_t *src = &_sidata;
    uint32_t *dst = &_sdata;
    while (dst < &_edata) *dst++ = *src++;

    /* Zero-fill .bss section */
    dst = &_sbss;
    while (dst < &_ebss) *dst++ = 0;

    /* Run application */
    clock_init();

    /* -- Audio setup ------------------------------------------------ */
    static AudioBuf audio_buf;
    static I2SDriver i2s_drv;
    static SineGen   sine;

    audio_buf_init(&audio_buf);
    i2s_init(&i2s_drv, AUDIO_SAMPLE_RATE, &audio_buf);

    /* 440 Hz A4 tone at 80% volume */
    sine_gen_init(&sine, 440, 0.8f, AUDIO_SAMPLE_RATE);

    /* Pre-fill both halves before starting DMA */
    sine_gen_fill(&sine, audio_buf.ping, AUDIO_BUF_SIZE);
    sine_gen_fill(&sine, audio_buf.pong, AUDIO_BUF_SIZE);

    i2s_start(&i2s_drv);

    /* -- Main loop -------------------------------------------------- */
    while (1) {
        /*
         * In a real driver the DMA ISR calls i2s_dma_callback(),
         * which swaps buffers. Here in polling mode we just keep
         * refilling the write-side buffer.
         */
        int16_t *write_buf = audio_buf_get_write_ptr(&audio_buf);
        sine_gen_fill(&sine, write_buf, AUDIO_BUF_SIZE);
    }
}
