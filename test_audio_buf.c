#include "../tests/unity/unity.h"
#include "audio_buf.h"
#include <string.h>

/* ------------------------------------------------------------------ */
/*  Happy path tests                                                   */
/* ------------------------------------------------------------------ */

static void test_init_clears_buffers(void)
{
    AudioBuf buf;
    /* Dirty the memory first */
    memset(&buf, 0xFF, sizeof(buf));

    audio_buf_init(&buf);

    /* All samples should be silence (0) */
    for (int i = 0; i < AUDIO_BUF_SIZE * AUDIO_CHANNELS; i++) {
        TEST_ASSERT_EQUAL_INT(0, buf.ping[i]);
        TEST_ASSERT_EQUAL_INT(0, buf.pong[i]);
    }
    TEST_ASSERT_EQUAL_INT(0, buf.active);
    TEST_ASSERT_EQUAL_INT(0, buf.underruns);
}

static void test_write_ptr_is_pong_when_active_zero(void)
{
    AudioBuf buf;
    audio_buf_init(&buf);
    buf.active = 0;

    int16_t *wp = audio_buf_get_write_ptr(&buf);
    TEST_ASSERT(wp == buf.pong);
}

static void test_write_ptr_is_ping_when_active_one(void)
{
    AudioBuf buf;
    audio_buf_init(&buf);
    buf.active = 1;

    int16_t *wp = audio_buf_get_write_ptr(&buf);
    TEST_ASSERT(wp == buf.ping);
}

static void test_read_ptr_is_ping_when_active_zero(void)
{
    AudioBuf buf;
    audio_buf_init(&buf);
    buf.active = 0;

    int16_t *rp = audio_buf_get_read_ptr(&buf);
    TEST_ASSERT(rp == buf.ping);
}

static void test_swap_toggles_active(void)
{
    AudioBuf buf;
    audio_buf_init(&buf);

    TEST_ASSERT_EQUAL_INT(0, buf.active);
    audio_buf_swap(&buf);
    TEST_ASSERT_EQUAL_INT(1, buf.active);
    audio_buf_swap(&buf);
    TEST_ASSERT_EQUAL_INT(0, buf.active);
}

static void test_read_write_ptrs_never_alias(void)
{
    AudioBuf buf;
    audio_buf_init(&buf);

    /* At active=0 */
    TEST_ASSERT(audio_buf_get_read_ptr(&buf) !=
                audio_buf_get_write_ptr(&buf));

    /* At active=1 */
    audio_buf_swap(&buf);
    TEST_ASSERT(audio_buf_get_read_ptr(&buf) !=
                audio_buf_get_write_ptr(&buf));
}

/* ------------------------------------------------------------------ */
/*  Edge case tests                                                    */
/* ------------------------------------------------------------------ */

static void test_silence_fills_zeros(void)
{
    int16_t buf[16];
    memset(buf, 0xAB, sizeof(buf));

    audio_buf_silence(buf, 16);

    for (int i = 0; i < 16; i++)
        TEST_ASSERT_EQUAL_INT(0, buf[i]);
}

static void test_multiple_swaps_return_to_origin(void)
{
    AudioBuf buf;
    audio_buf_init(&buf);

    /* Even number of swaps must restore state */
    for (int i = 0; i < 100; i++) audio_buf_swap(&buf);
    TEST_ASSERT_EQUAL_INT(0, buf.active);

    /* Odd number of swaps must flip state */
    for (int i = 0; i < 99; i++) audio_buf_swap(&buf);
    TEST_ASSERT_EQUAL_INT(1, buf.active);
}

static void test_write_does_not_corrupt_read_side(void)
{
    AudioBuf buf;
    audio_buf_init(&buf);

    /* Fill the DMA (read) side with a known pattern */
    int16_t *rp = audio_buf_get_read_ptr(&buf);
    for (int i = 0; i < AUDIO_BUF_SIZE * AUDIO_CHANNELS; i++)
        rp[i] = (int16_t)(i & 0x7FFF);

    /* Write something completely different to write side */
    int16_t *wp = audio_buf_get_write_ptr(&buf);
    for (int i = 0; i < AUDIO_BUF_SIZE * AUDIO_CHANNELS; i++)
        wp[i] = -1;

    /* Read side must be untouched */
    for (int i = 0; i < AUDIO_BUF_SIZE * AUDIO_CHANNELS; i++)
        TEST_ASSERT_EQUAL_INT((int16_t)(i & 0x7FFF), rp[i]);
}

/* ------------------------------------------------------------------ */

int main(void)
{
    printf("=== Audio Buffer Tests ===\n\n");

    RUN_TEST(test_init_clears_buffers);
    RUN_TEST(test_write_ptr_is_pong_when_active_zero);
    RUN_TEST(test_write_ptr_is_ping_when_active_one);
    RUN_TEST(test_read_ptr_is_ping_when_active_zero);
    RUN_TEST(test_swap_toggles_active);
    RUN_TEST(test_read_write_ptrs_never_alias);
    RUN_TEST(test_silence_fills_zeros);
    RUN_TEST(test_multiple_swaps_return_to_origin);
    RUN_TEST(test_write_does_not_corrupt_read_side);

    UNITY_SUMMARY();
}
