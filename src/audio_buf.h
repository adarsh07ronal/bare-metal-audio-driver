#ifndef AUDIO_BUF_H
#define AUDIO_BUF_H

#include <stdint.h>
#include <stddef.h>

/* Buffer size must be power of 2 for efficient wrapping */
#define AUDIO_BUF_SIZE   512   /* samples per half-buffer */
#define AUDIO_SAMPLE_RATE 44100
#define AUDIO_CHANNELS    2    /* stereo: left + right interleaved */

/*
 * Ping-pong (double) buffer layout:
 *
 *   [ PING: 512 samples ] [ PONG: 512 samples ]
 *     DMA plays this  -->    CPU fills this
 *   then they swap on each DMA half/complete interrupt
 */
typedef struct {
    int16_t  ping[AUDIO_BUF_SIZE * AUDIO_CHANNELS];
    int16_t  pong[AUDIO_BUF_SIZE * AUDIO_CHANNELS];
    uint8_t  active;      /* 0 = DMA on ping, 1 = DMA on pong */
    uint32_t underruns;   /* count of buffer underrun events   */
} AudioBuf;

/* Initialise buffer to silence */
void     audio_buf_init(AudioBuf *buf);

/* Get pointer to the half the CPU should fill right now */
int16_t *audio_buf_get_write_ptr(AudioBuf *buf);

/* Get pointer to the half currently being played by DMA */
int16_t *audio_buf_get_read_ptr(AudioBuf *buf);

/* Called from DMA interrupt — swaps active halves */
void     audio_buf_swap(AudioBuf *buf);

/* Fill a buffer half with silence */
void     audio_buf_silence(int16_t *buf, size_t samples);

#endif /* AUDIO_BUF_H */
