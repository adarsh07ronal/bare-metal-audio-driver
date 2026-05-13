#ifndef SINE_GEN_H
#define SINE_GEN_H

#include <stdint.h>
#include "audio_buf.h"

typedef struct {
    float    phase;        /* current phase accumulator 0..2π  */
    float    phase_inc;    /* phase increment per sample        */
    float    amplitude;    /* 0.0 .. 1.0                        */
    uint32_t frequency_hz;
} SineGen;

/* Initialise generator for a given frequency */
void sine_gen_init(SineGen *gen, uint32_t freq_hz, float amplitude,
                   uint32_t sample_rate);

/* Fill one buffer half with stereo sine samples */
void sine_gen_fill(SineGen *gen, int16_t *buf, uint32_t frames);

#endif /* SINE_GEN_H */
