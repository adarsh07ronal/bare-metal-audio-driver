#ifndef PDM_FILTER_H
#define PDM_FILTER_H

#include <stdint.h>
#include <stddef.h>

/*
 * PDM-to-PCM decimation filter — two stage pipeline:
 *
 *  Stage 1 — CIC filter (Cascaded Integrator-Comb)
 *    Runs at PDM bit rate. Downsamples by 64x using only
 *    addition and subtraction. No multiplications — fast enough
 *    to run at 3 MHz on bare metal.
 *
 *  Stage 2 — FIR filter (Finite Impulse Response)
 *    Runs at PCM rate (after decimation). Cleans up the
 *    frequency response left rough by the CIC stage.
 *    Uses Q15 fixed-point multiply-accumulate.
 *
 *  Pipeline:
 *    PDM bits @ 3.072 MHz
 *        → [CIC, 3 stages, R=64]
 *    Intermediate @ 48 kHz (one sample per 64 bits)
 *        → [FIR, 16 taps, Q15]
 *    PCM samples @ 48 kHz, 16-bit
 */

#define PDM_DECIMATION_FACTOR   64      /* CIC downsampling ratio        */
#define PDM_CIC_STAGES          3       /* integrator/comb stage count   */
#define PDM_FIR_TAPS            16      /* FIR filter length             */
#define PDM_BITS_PER_BYTE       8       /* PDM arrives as packed bytes   */

/*
 * PDMFilter — holds all state for one channel.
 * Declare one per audio channel (mono = 1, stereo = 2).
 */
typedef struct {
    /* --- CIC state --- */
    int32_t  integrator[PDM_CIC_STAGES];   /* running sums (high rate)  */
    int32_t  comb_prev[PDM_CIC_STAGES];    /* previous values for diff  */
    uint32_t bit_count;                     /* bits seen since last out  */
    int32_t  cic_out;                       /* latest CIC output sample  */

    /* --- FIR delay line --- */
    int32_t  fir_delay[PDM_FIR_TAPS];      /* ring buffer of past samples */
    uint32_t fir_idx;                       /* current write position      */
} PDMFilter;

/*
 * Initialise filter to silence.
 * Must be called before pdm_filter_process_byte().
 */
void pdm_filter_init(PDMFilter *f);

/*
 * Feed one byte (8 PDM bits) into the filter.
 *
 * Returns 1 and writes a PCM sample to *pcm_out when a new
 * output sample is ready (every PDM_DECIMATION_FACTOR bits = 8 bytes).
 * Returns 0 and leaves *pcm_out unchanged otherwise.
 */
uint32_t pdm_filter_process_byte(PDMFilter *f,
                                  uint8_t    pdm_byte,
                                  int16_t   *pcm_out);

#endif /* PDM_FILTER_H */
