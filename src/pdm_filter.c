#include "pdm_filter.h"

/*
 * 16-tap symmetric FIR coefficients — Q15 fixed-point format.
 *
 * Hamming-windowed sinc, low-pass at ~0.8 x Nyquist.
 * Keeps 0–19 kHz, attenuates above. Symmetric = linear phase
 * (no phase distortion in audio band).
 *
 * Q15: real value = coeff / 32768
 * Sum ≈ 32768 → unity DC gain.
 */
static const int16_t fir_coeffs[PDM_FIR_TAPS] = {
    -150, -250,    0,  800,
    1900, 3300, 4800, 5900,
    5900, 4800, 3300, 1900,
     800,    0, -250, -150,
};

/* ------------------------------------------------------------------ */

void pdm_filter_init(PDMFilter *f)
{
    uint32_t i;
    for (i = 0; i < PDM_CIC_STAGES; i++) {
        f->integrator[i] = 0;
        f->comb_prev[i]  = 0;
    }
    for (i = 0; i < PDM_FIR_TAPS; i++) {
        f->fir_delay[i] = 0;
    }
    f->bit_count = 0;
    f->cic_out   = 0;
    f->fir_idx   = 0;
}

/* ------------------------------------------------------------------ */
/*  Stage 1 — CIC filter (Cascaded Integrator-Comb)                  */
/*                                                                    */
/*  Integrators: y[n] = y[n-1] + x[n]   run at every bit            */
/*  Combs:       y[n] = x[n] - x[n-1]   run every 64 bits           */
/*                                                                    */
/*  Returns 1 when a new output sample is stored in f->cic_out.      */
/*  Returns 0 otherwise.                                              */
/*                                                                    */
/*  CIC gain = R^N = 64^3 = 2^18                                     */
/*  We shift right by 3 → maps to int16 range (2^15)                 */
/* ------------------------------------------------------------------ */
static uint32_t cic_process_bit(PDMFilter *f, uint8_t bit)
{
    /*
     * Convert PDM bit to bipolar signal.
     * 1 → +1  (sound pushing up)
     * 0 → -1  (sound pushing down)
     */
    int32_t x = bit ? 1 : -1;

    /* Integrator stages — run every single bit */
    f->integrator[0] += x;
    f->integrator[1] += f->integrator[0];
    f->integrator[2] += f->integrator[1];

    f->bit_count++;

    /* Only produce output every DECIMATION_FACTOR bits */
    if (f->bit_count < PDM_DECIMATION_FACTOR) {
        return 0;
    }
    f->bit_count = 0;

    /*
     * Comb stages — run at decimated rate.
     * Each stage: output = current - previous
     * This cancels the integrator droop, giving flat frequency response.
     */
    int32_t tmp = f->integrator[2];
    uint32_t i;
    for (i = 0; i < PDM_CIC_STAGES; i++) {
        int32_t diff    = tmp - f->comb_prev[i];
        f->comb_prev[i] = tmp;
        tmp             = diff;
    }

    /*
     * Scale to int16 range.
     * CIC gain = 2^18, target = 2^15, so shift right by 3.
     */
    f->cic_out = tmp >> 3;
    return 1;
}

/* ------------------------------------------------------------------ */
/*  Stage 2 — FIR low-pass filter                                     */
/*                                                                    */
/*  y[n] = sum( h[k] * x[n-k] )  for k = 0 .. TAPS-1               */
/*                                                                    */
/*  Delay line is a ring buffer — new samples written at fir_idx,    */
/*  past samples read backwards around the ring.                     */
/* ------------------------------------------------------------------ */
static int16_t fir_process(PDMFilter *f, int32_t sample)
{
    /* Store new CIC sample in ring buffer */
    f->fir_delay[f->fir_idx] = sample;

    /* Multiply-accumulate over all taps */
    int32_t acc = 0;
    uint32_t k;
    for (k = 0; k < PDM_FIR_TAPS; k++) {
        /*
         * Index into ring buffer going backwards from current position.
         * + PDM_FIR_TAPS prevents negative modulo on small k.
         */
        uint32_t idx = (f->fir_idx + PDM_FIR_TAPS - k) % PDM_FIR_TAPS;

        /*
         * Q15 multiply-accumulate:
         *   coeff is scaled by 2^15, so divide result by 2^15
         *   >> 15 removes the scaling factor
         */
        acc += (f->fir_delay[idx] * (int32_t)fir_coeffs[k]) >> 15;
    }

    /* Advance write position */
    f->fir_idx = (f->fir_idx + 1) % PDM_FIR_TAPS;

    /* Clamp to int16 to prevent overflow */
    if (acc >  32767) acc =  32767;
    if (acc < -32768) acc = -32768;

    return (int16_t)acc;
}

/* ------------------------------------------------------------------ */
/*  Public API                                                        */
/* ------------------------------------------------------------------ */

uint32_t pdm_filter_process_byte(PDMFilter *f,
                                  uint8_t    pdm_byte,
                                  int16_t   *pcm_out)
{
    uint32_t ready = 0;
    int bit;

    /*
     * Unpack 8 PDM bits, MSB first.
     * On real hardware PDM data is clocked out MSB-first.
     */
    for (bit = 7; bit >= 0; bit--) {
        uint8_t pdm_bit = (pdm_byte >> bit) & 0x01;

        if (cic_process_bit(f, pdm_bit)) {
            /*
             * CIC produced a new output sample.
             * Pass it through FIR and tell the caller.
             */
            *pcm_out = fir_process(f, f->cic_out);
            ready = 1;
        }
    }

    return ready;
}
