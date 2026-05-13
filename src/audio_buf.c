#include "audio_buf.h"

static void zero_buf(int16_t *buf, size_t count)
{
    for (size_t i = 0; i < count; i++) buf[i] = 0;
}

void audio_buf_init(AudioBuf *buf)
{
    zero_buf(buf->ping, AUDIO_BUF_SIZE * AUDIO_CHANNELS);
    zero_buf(buf->pong, AUDIO_BUF_SIZE * AUDIO_CHANNELS);
    buf->active   = 0;
    buf->underruns = 0;
}

/*
 * The CPU always fills the buffer that DMA is NOT playing.
 * active=0 → DMA plays ping → CPU fills pong
 * active=1 → DMA plays pong → CPU fills ping
 */
int16_t *audio_buf_get_write_ptr(AudioBuf *buf)
{
    return (buf->active == 0) ? buf->pong : buf->ping;
}

int16_t *audio_buf_get_read_ptr(AudioBuf *buf)
{
    return (buf->active == 0) ? buf->ping : buf->pong;
}

/*
 * Called from DMA half/complete interrupt.
 * Atomically swaps which half DMA is reading from.
 * In bare-metal this runs inside an ISR — keep it short.
 */
void audio_buf_swap(AudioBuf *buf)
{
    buf->active ^= 1;
}

void audio_buf_silence(int16_t *buf, size_t samples)
{
    zero_buf(buf, samples);
}
