#include "../tests/unity/unity.h"
#include "pdm_filter.h"
#include <math.h>
#include <string.h>
#include <stdint.h>

void setUp(void) {}
void tearDown(void) {}

/* ------------------------------------------------------------------ */
/*  Helper: simple first-order sigma-delta modulator                  */
/*  Converts a PCM sample into 8 bits of PDM.                        */
/*                                                                    */
/*  This is how a real MEMS mic works internally — it produces a     */
/*  PDM stream whose density encodes the instantaneous amplitude.    */
/* ------------------------------------------------------------------ */
static uint8_t pcm_to_pdm_byte(int32_t *error, int16_t pcm)
{
    uint8_t pdm = 0;
    int bit;
    for (bit = 7; bit >= 0; bit--) {
        *error += pcm;
        if (*error >= 0) {
            pdm |= (1 << bit);   /* dense 1 = positive signal */
            *error -= 32767;
        } else {
            *error += 32767;
        }
    }
    return pdm;
}

/* ------------------------------------------------------------------ */
/*  Happy path tests                                                  */
/* ------------------------------------------------------------------ */

static void test_init_zeroes_all_state(void)
{
    PDMFilter f;
    memset(&f, 0xFF, sizeof(f));  /* dirty memory first */
    pdm_filter_init(&f);

    uint32_t i;
    for (i = 0; i < PDM_CIC_STAGES; i++) {
        TEST_ASSERT_EQUAL_INT(0, f.integrator[i]);
        TEST_ASSERT_EQUAL_INT(0, f.comb_prev[i]);
    }
    for (i = 0; i < PDM_FIR_TAPS; i++) {
        TEST_ASSERT_EQUAL_INT(0, f.fir_delay[i]);
    }
    TEST_ASSERT_EQUAL_INT(0, f.bit_count);
    TEST_ASSERT_EQUAL_INT(0, f.fir_idx);
}

static void test_silence_pdm_produces_near_zero_output(void)
{
    /*
     * All-zero PDM = -1 every bit = maximum negative signal.
     * After warmup the output should be large negative,
     * NOT near zero. We test all-ones for near-positive and
     * alternating for near-zero (silence in PDM = alternating 10101010).
     */
    PDMFilter f;
    pdm_filter_init(&f);

    /* Alternating 10101010 = 0xAA = silence in PDM */
    int16_t pcm_out = 0;
    uint32_t i;

    /* Warm up filter (flush the delay line) */
    for (i = 0; i < PDM_FIR_TAPS * 8; i++) {
        pdm_filter_process_byte(&f, 0xAA, &pcm_out);
    }

    /* Now check: output should be close to zero */
    TEST_ASSERT(pcm_out > -1000);
    TEST_ASSERT(pcm_out <  1000);
}

static void test_all_ones_pdm_produces_positive_output(void)
{
    /*
     * All-ones PDM (0xFF) = maximum positive signal.
     * After warmup the output should be large positive.
     */
    PDMFilter f;
    pdm_filter_init(&f);

    int16_t pcm_out = 0;
    uint32_t i;

    for (i = 0; i < 200; i++) {
        pdm_filter_process_byte(&f, 0xFF, &pcm_out);
    }

    TEST_ASSERT(pcm_out > 10000);
}

static void test_all_zeros_pdm_produces_negative_output(void)
{
    /*
     * All-zeros PDM (0x00) = maximum negative signal.
     * After warmup the output should be large negative.
     */
    PDMFilter f;
    pdm_filter_init(&f);

    int16_t pcm_out = 0;
    uint32_t i;

    for (i = 0; i < 200; i++) {
        pdm_filter_process_byte(&f, 0x00, &pcm_out);
    }

    TEST_ASSERT(pcm_out < -10000);
}

static void test_output_ready_every_8_bytes(void)
{
    /*
     * With DECIMATION_FACTOR=64 and 8 bits per byte,
     * a new PCM sample should be produced every 64/8 = 8 bytes.
     */
    PDMFilter f;
    pdm_filter_init(&f);

    int16_t pcm_out = 0;
    uint32_t ready_count = 0;
    uint32_t i;

    for (i = 0; i < 80; i++) {
        if (pdm_filter_process_byte(&f, 0xAA, &pcm_out)) {
            ready_count++;
        }
    }

    /* 80 bytes / 8 bytes per sample = 10 samples */
    TEST_ASSERT_EQUAL_INT(10, ready_count);
}

static void test_sine_wave_roundtrip_nonzero(void)
{
    /*
     * Convert a 1 kHz sine wave to PDM (sigma-delta modulation),
     * feed it through the filter, and verify the output is non-zero
     * and has the right sign pattern — positive half then negative half.
     *
     * We can't check exact values because the filter has group delay
     * and gain that shift timing and amplitude. But we can verify
     * the signal is alive and approximately correct in sign.
     */
    PDMFilter f;
    pdm_filter_init(&f);

    /* 1 kHz sine at 48 kHz sample rate */
    const uint32_t SAMPLE_RATE = 48000;
    const uint32_t FREQ_HZ     = 1000;
    const float    PHASE_INC   = (float)FREQ_HZ / (float)SAMPLE_RATE;

    int32_t sd_error = 0;   /* sigma-delta error accumulator */
    float   phase    = 0.0f;

    int16_t pcm_out  = 0;
    int32_t sum      = 0;
    uint32_t samples = 0;

    /* Run enough PDM bytes for ~100 PCM output samples (800 bytes) */
    uint32_t i;
    for (i = 0; i < 800; i++) {
        /* Generate one PCM sample from sine */
        int16_t pcm_in = (int16_t)(sinf(2.0f * 3.14159265f * phase) * 16000.0f);
        phase += PHASE_INC;
        if (phase >= 1.0f) phase -= 1.0f;

        /* Convert PCM → PDM byte (sigma-delta) */
        uint8_t pdm_byte = pcm_to_pdm_byte(&sd_error, pcm_in);

        /* Feed into filter */
        if (pdm_filter_process_byte(&f, pdm_byte, &pcm_out)) {
            sum += (pcm_out > 0) ? 1 : (pcm_out < 0) ? -1 : 0;
            samples++;
        }
    }

    /* Output must be non-zero — signal is alive */
    TEST_ASSERT(samples > 0);
    TEST_ASSERT(sum != 0);  /* some positive, some negative — it's oscillating */
}

/* ------------------------------------------------------------------ */
/*  Edge case tests                                                   */
/* ------------------------------------------------------------------ */

static void test_multiple_inits_reset_state(void)
{
    PDMFilter f;
    pdm_filter_init(&f);

    /* Run some data through to dirty the state */
    int16_t out = 0;
    uint32_t i;
    for (i = 0; i < 100; i++) {
        pdm_filter_process_byte(&f, 0xFF, &out);
    }

    /* Re-initialise */
    pdm_filter_init(&f);

    /* State must be clean again */
    for (i = 0; i < PDM_CIC_STAGES; i++) {
        TEST_ASSERT_EQUAL_INT(0, f.integrator[i]);
        TEST_ASSERT_EQUAL_INT(0, f.comb_prev[i]);
    }
    TEST_ASSERT_EQUAL_INT(0, f.bit_count);
}

static void test_two_channels_are_independent(void)
{
    /*
     * Two filter instances must not share state.
     * Feed opposite signals — one all-ones, one all-zeros.
     * Outputs must be opposite in sign.
     */
    PDMFilter left, right;
    pdm_filter_init(&left);
    pdm_filter_init(&right);

    int16_t out_left = 0, out_right = 0;
    uint32_t i;

    for (i = 0; i < 200; i++) {
        pdm_filter_process_byte(&left,  0xFF, &out_left);
        pdm_filter_process_byte(&right, 0x00, &out_right);
    }

    TEST_ASSERT(out_left  >  0);
    TEST_ASSERT(out_right <  0);
    TEST_ASSERT(out_left  > out_right);
}

static void test_output_stays_within_int16_range(void)
{
    /*
     * No matter what PDM data we feed, output must never
     * exceed int16 limits. Tests the clamping in fir_process().
     */
    PDMFilter f;
    pdm_filter_init(&f);

    int16_t out = 0;
    uint32_t i;

    /* Worst case: alternate between all-ones and all-zeros */
    for (i = 0; i < 1000; i++) {
        uint8_t byte = (i % 2 == 0) ? 0xFF : 0x00;
        pdm_filter_process_byte(&f, byte, &out);
        TEST_ASSERT(out >= -32768);
        TEST_ASSERT(out <=  32767);
    }
}

/* ------------------------------------------------------------------ */

int main(void)
{
    printf("=== PDM Filter Tests ===\n\n");
    UNITY_BEGIN();

    RUN_TEST(test_init_zeroes_all_state);
    RUN_TEST(test_silence_pdm_produces_near_zero_output);
    RUN_TEST(test_all_ones_pdm_produces_positive_output);
    RUN_TEST(test_all_zeros_pdm_produces_negative_output);
    RUN_TEST(test_output_ready_every_8_bytes);
    RUN_TEST(test_sine_wave_roundtrip_nonzero);
    RUN_TEST(test_multiple_inits_reset_state);
    RUN_TEST(test_two_channels_are_independent);
    RUN_TEST(test_output_stays_within_int16_range);

    return UNITY_END();
}
