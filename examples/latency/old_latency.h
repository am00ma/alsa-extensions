/** @file old_latency.c
 *  @brief Direct implementation without `duplex_ops` interface
 *
 *  Start:
 *
 *
 *
 *  Stop:
 */
#include "duplex.h"

// Form of start and stop in this program
int sndx_duplex_start(sndx_duplex_t* d, char** play_bufp, char** capt_bufp, uframes_t loop_limit);
int sndx_duplex_stop(sndx_duplex_t* d, char* play_buf, char* capt_buf);

// Implementation
int sndx_duplex_start(sndx_duplex_t* d, char** play_bufp, char** capt_bufp, uframes_t loop_limit)
{
    int err;

    output_t* output = d->out;

    // Allocate samples temporarily
    isize capt_size = d->buf_capt->frames * d->buf_capt->channels * d->buf_capt->bytes;
    char* capt_buf  = calloc(capt_size, sizeof(char));
    *capt_bufp      = capt_buf;

    isize play_size = d->buf_play->frames * d->buf_play->channels * d->buf_play->bytes;
    char* play_buf  = calloc(play_size, sizeof(char));
    *play_bufp      = play_buf;

    sndx_buffer_map_dev_to_samples(d->buf_play, play_buf);
    sndx_buffer_map_dev_to_samples(d->buf_capt, capt_buf);

    // Set silence
    uframes_t frames_silence = 0;

    err = sndx_duplex_write_initial_silence(d, play_buf, &frames_silence);
    SndFatal_(err, "Failed sndx_duplex_write_initial_silence: %s");

    // After silence, stream is already started (start threshold is 0x7fffffff in latency, 2*period_size for duplex)
    // sndx_duplex_timer_start(&d->timer, d->play, d->capt);

    // Debugging
    sndx_dump_duplex(d, output);

    // Start prep for loop -> on error goto __error
    uframes_t frames_out = frames_silence;
    uframes_t frames_in  = 0;
    uframes_t in_max     = 0;
    uframes_t out_max    = 0;

    // Read write loop
    sframes_t r, cap_avail;
    float     gain = 0.5;
    while (frames_in < loop_limit)
    {
        cap_avail = d->period_size;
        snd_pcm_wait(d->capt, 1000);

        // BUG: How do you know avail >= cap_avail?
        // BUG: Error handling for wait (@see pcm.c)

        r = sndx_duplex_readbuf(d, capt_buf, cap_avail, 0, &frames_in, &in_max);
        SndReturn_(r, "Failed readbuf: %s");

        // Copy capture channels to playback
        sndx_duplex_copy_capt_to_play(d->buf_capt, d->buf_play, r, &gain);

        err = sndx_duplex_writebuf(d, play_buf, r, 0, &frames_out, &out_max);
        SndReturn_(err, "Failed writebuf: %s");
    }

    // Report final timings
    // sndx_duplex_timer_stop(&d->timer, frames_in, d->rate, d->out);

    return 0;
}

int sndx_duplex_stop(sndx_duplex_t* d, char* play_buf, char* capt_buf)
{
    snd_pcm_drop(d->capt);
    snd_pcm_drain(d->play);
    snd_pcm_nonblock(d->play, SND_PCM_NONBLOCK);

    snd_pcm_unlink(d->capt);
    snd_pcm_hw_free(d->play);
    snd_pcm_hw_free(d->capt);

    free(capt_buf);
    free(play_buf);

    return 0;
}

// Application
int main()
{
    int err; ///< Brief description after the member

    output_t* output;
    err = snd_output_stdio_attach(&output, stderr, 0);
    SndFatal(err, "Failed snd_output_stdio_attach: %s");

    sndx_duplex_t* d;
    err = sndx_duplex_open(              //
        &d,                              //
        "hw:FC1,0", "hw:FC1,0",          // play, capt device
        SND_PCM_FORMAT_S16_LE,           // format
        48000,                           // rate
        256, 128,                        // buffer_size, period_size
        SND_PCM_ACCESS_MMAP_INTERLEAVED, // access
        output);
    SndFatal(err, "Failed sndx_duplex_open: %s");

    char*     play_buf;
    char*     capt_buf;
    uframes_t loop_limit = d->rate / 100; // 0.01 secs

    err = sndx_duplex_start(d, &play_buf, &capt_buf, loop_limit);
    SndCheck_(err, "Failed sndx_duplex_start: %s");

    err = sndx_duplex_stop(d, play_buf, capt_buf);
    SndCheck_(err, "Failed sndx_duplex_stop: %s");

    err = sndx_duplex_close(d);
    SndCheck_(err, "Failed sndx_duplex_close: %s");

    snd_output_close(output);

    return 0;
}
