#include "sine_gen.h"
#include <math.h>

/*
 * 256-entry sine lookup table.
 * Values are Q15 fixed-point: range -32767 .. +32767
 * Generated at startup via sine_gen_init — avoids a linker dependency
 * on libm in the final firmware if desired.
 */
#define LUT_SIZE 256
static int16_t sine_lut[LUT_SIZE];
static uint8_t lut_ready = 0;

static void build_lut(void)
{
    for (int i = 0; i < LUT_SIZE; i++) {
        sine_lut[i] = (int16_t)(sinf(2.0f * 3.14159265f * i / LUT_SIZE)
                                 * 32767.0f);
    }
    lut_ready = 1;
}

/* ------------------------------------------------------------------ */

void sine_gen_init(SineGen *gen, uint32_t freq_hz, float amplitude,
                   uint32_t sample_rate)
{
    if (!lut_ready) build_lut();

    gen->frequency_hz = freq_hz;
    gen->amplitude    = amplitude;
    gen->phase        = 0.0f;
    /*
     * Each sample advances the phase by freq_hz / sample_rate cycles.
     * We scale to LUT_SIZE so the index wraps naturally.
     */
    gen->phase_inc = ((float)freq_hz * LUT_SIZE) / (float)sample_rate;
}

/* ------------------------------------------------------------------ */

void sine_gen_fill(SineGen *gen, int16_t *buf, uint32_t frames)
{
    for (uint32_t i = 0; i < frames; i++) {
        /* Integer index into LUT */
        uint32_t idx = (uint32_t)gen->phase & (LUT_SIZE - 1);
        int16_t  sample = (int16_t)(sine_lut[idx] * gen->amplitude);

        /* Interleaved stereo: same value on both channels */
        buf[i * 2]     = sample;   /* left  */
        buf[i * 2 + 1] = sample;   /* right */

        gen->phase += gen->phase_inc;
        if (gen->phase >= LUT_SIZE)
            gen->phase -= LUT_SIZE;
    }
}
