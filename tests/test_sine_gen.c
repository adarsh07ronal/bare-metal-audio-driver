#include "../tests/unity/unity.h"
#include "sine_gen.h"
#include <math.h>
#include <string.h>

void setUp(void) {}
void tearDown(void) {}

/* ------------------------------------------------------------------ */
/*  Happy path                                                         */
/* ------------------------------------------------------------------ */

static void test_init_sets_frequency(void)
{
    SineGen gen;
    sine_gen_init(&gen, 440, 1.0f, 44100);
    TEST_ASSERT_EQUAL_INT(440, gen.frequency_hz);
}

static void test_init_phase_starts_at_zero(void)
{
    SineGen gen;
    sine_gen_init(&gen, 440, 1.0f, 44100);
    TEST_ASSERT(gen.phase == 0.0f);
}

static void test_fill_produces_nonzero_samples(void)
{
    SineGen gen;
    sine_gen_init(&gen, 440, 1.0f, 44100);

    int16_t buf[64 * 2];  /* 64 stereo frames */
    memset(buf, 0, sizeof(buf));
    sine_gen_fill(&gen, buf, 64);

    /* At least some samples must be non-zero */
    int nonzero = 0;
    for (int i = 0; i < 64 * 2; i++)
        if (buf[i] != 0) nonzero++;
    TEST_ASSERT(nonzero > 0);
}

static void test_fill_stereo_channels_match(void)
{
    SineGen gen;
    sine_gen_init(&gen, 440, 1.0f, 44100);

    int16_t buf[32 * 2];
    sine_gen_fill(&gen, buf, 32);

    /* Left and right channels must be identical (mono source) */
    for (int i = 0; i < 32; i++)
        TEST_ASSERT_EQUAL_INT(buf[i * 2], buf[i * 2 + 1]);
}

static void test_amplitude_zero_produces_silence(void)
{
    SineGen gen;
    sine_gen_init(&gen, 440, 0.0f, 44100);

    int16_t buf[32 * 2];
    sine_gen_fill(&gen, buf, 32);

    for (int i = 0; i < 32 * 2; i++)
        TEST_ASSERT_EQUAL_INT(0, buf[i]);
}

/* ------------------------------------------------------------------ */
/*  Edge cases                                                         */
/* ------------------------------------------------------------------ */

static void test_phase_advances_across_fills(void)
{
    SineGen gen;
    sine_gen_init(&gen, 440, 1.0f, 44100);

    float phase_before = gen.phase;
    int16_t buf[16 * 2];
    sine_gen_fill(&gen, buf, 16);

    /* Phase must have advanced */
    TEST_ASSERT(gen.phase != phase_before);
}

static void test_phase_wraps_and_does_not_grow_unbounded(void)
{
    SineGen gen;
    sine_gen_init(&gen, 440, 1.0f, 44100);

    int16_t buf[512 * 2];
    /* Fill many times — phase must stay < LUT_SIZE (256) */
    for (int i = 0; i < 100; i++) {
        sine_gen_fill(&gen, buf, 512);
        TEST_ASSERT(gen.phase < 256.0f);
    }
}

static void test_high_frequency_fills_without_crash(void)
{
    SineGen gen;
    /* Near-Nyquist: 20 kHz at 44100 Hz sample rate */
    sine_gen_init(&gen, 20000, 0.5f, 44100);

    int16_t buf[512 * 2];
    sine_gen_fill(&gen, buf, 512);
    /* No crash = pass; phase still valid */
    TEST_ASSERT(gen.phase < 256.0f);
}

static void test_consecutive_fills_are_continuous(void)
{
    SineGen gen;
    sine_gen_init(&gen, 440, 1.0f, 44100);

    int16_t buf_a[4 * 2], buf_b[4 * 2], buf_c[8 * 2];

    /* Two small fills */
    sine_gen_fill(&gen, buf_a, 4);
    sine_gen_fill(&gen, buf_b, 4);

    /* One big fill from fresh init */
    sine_gen_init(&gen, 440, 1.0f, 44100);
    sine_gen_fill(&gen, buf_c, 8);

    /* First half of big fill must match buf_a */
    for (int i = 0; i < 4 * 2; i++)
        TEST_ASSERT_EQUAL_INT(buf_a[i], buf_c[i]);

    /* Second half of big fill must match buf_b */
    for (int i = 0; i < 4 * 2; i++)
        TEST_ASSERT_EQUAL_INT(buf_b[i], buf_c[4 * 2 + i]);
}

/* ------------------------------------------------------------------ */

int main(void)
{
    printf("=== Sine Generator Tests ===\n\n");
    UNITY_BEGIN();

    RUN_TEST(test_init_sets_frequency);
    RUN_TEST(test_init_phase_starts_at_zero);
    RUN_TEST(test_fill_produces_nonzero_samples);
    RUN_TEST(test_fill_stereo_channels_match);
    RUN_TEST(test_amplitude_zero_produces_silence);
    RUN_TEST(test_phase_advances_across_fills);
    RUN_TEST(test_phase_wraps_and_does_not_grow_unbounded);
    RUN_TEST(test_high_frequency_fills_without_crash);
    RUN_TEST(test_consecutive_fills_are_continuous);

    return UNITY_END();
}
