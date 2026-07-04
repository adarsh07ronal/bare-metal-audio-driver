#include <stdint.h>
#include "i2s.h"
#include "audio_buf.h"
#include "sine_gen.h"
#include "pdm_filter.h"

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
/*  Simulated PDM microphone front-end                                 */
/*                                                                      */
/*  QEMU has no PDM/ADC peripheral model for this chip, so there is no */
/*  real microphone signal to decimate. This first-order sigma-delta   */
/*  modulator stands in for the physical MEMS mic: it re-encodes a PCM */
/*  test tone into the same kind of 1-bit-dense bitstream a real PDM    */
/*  mic produces, so pdm_filter.c has real bits to run its CIC+FIR      */
/*  decimation on instead of sitting unused. (Same technique already   */
/*  proven in tests/test_pdm_filter.c's sine roundtrip test.)          */
/* ------------------------------------------------------------------ */
static uint8_t pcm_to_pdm_byte(int32_t *error, int16_t pcm)
{
    uint8_t pdm = 0;
    int bit;
    for (bit = 7; bit >= 0; bit--) {
        *error += pcm;
        if (*error >= 0) {
            pdm |= (1 << bit);
            *error -= 32767;
        } else {
            *error += 32767;
        }
    }
    return pdm;
}

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

    /* -- Mic capture setup -------------------------------------------
     * 1 kHz test tone standing in for "what the microphone hears",
     * plus the PDM decimation filter that would normally sit between
     * a real MEMS mic and this PCM audio path.
     */
    static SineGen   mic_source;
    static PDMFilter mic_filter;
    int32_t sd_error = 0;

    sine_gen_init(&mic_source, 1000, 0.5f, AUDIO_SAMPLE_RATE);
    pdm_filter_init(&mic_filter);

    /* -- Main loop -------------------------------------------------- */
    while (1) {
        /*
         * In a real driver the DMA ISR calls i2s_dma_callback(),
         * which swaps buffers. Here in polling mode we just keep
         * refilling the write-side buffer.
         */
        int16_t *write_buf = audio_buf_get_write_ptr(&audio_buf);
        sine_gen_fill(&sine, write_buf, AUDIO_BUF_SIZE);

        /*
         * Exercise the mic capture path: one simulated PCM sample is
         * sigma-delta-encoded into a PDM byte (as a real mic's ADC
         * would output it), then decoded back through the same
         * CIC+FIR pipeline a real driver would use. Every 8th byte
         * (PDM_DECIMATION_FACTOR / 8 bits-per-byte) yields one
         * recovered PCM sample, which gets mixed under the playback
         * tone so the capture path's output is actually audible
         * instead of computed and discarded.
         */
        uint32_t frame;
        for (frame = 0; frame < AUDIO_BUF_SIZE; frame++) {
            int16_t mic_pcm[2];
            sine_gen_fill(&mic_source, mic_pcm, 1);

            uint8_t pdm_byte = pcm_to_pdm_byte(&sd_error, mic_pcm[0]);

            int16_t recovered;
            if (pdm_filter_process_byte(&mic_filter, pdm_byte, &recovered)) {
                int32_t mixed = write_buf[frame * 2] + (recovered >> 2);
                if (mixed >  32767) mixed =  32767;
                if (mixed < -32768) mixed = -32768;
                write_buf[frame * 2]     = (int16_t)mixed;
                write_buf[frame * 2 + 1] = (int16_t)mixed;
            }
        }
    }
}
