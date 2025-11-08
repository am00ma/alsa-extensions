/*! \file latency.c
    \brief A Documented file.

    Details.
*/

#include "duplex.h"

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
    SndFatal(err, "Failed sndx_duplex_write_initial_silence: %s");

    // After silence, stream is already started (start threshold is 0x7fffffff in latency, 2*period_size for duplex)
    sndx_duplex_timer_start(d);

    // Debugging
    sndx_dump_duplex(d, output);

    // Start prep for loop -> on error goto __error
    uframes_t frames_out = frames_silence;
    uframes_t frames_in  = 0;
    uframes_t in_max     = 0;

    // Read write loop
    sframes_t r, cap_avail;
    float     gain = 0.5;
    while (frames_in < loop_limit)
    {
        cap_avail = d->period_size;
        snd_pcm_wait(d->capt, 1000);

        r = sndx_duplex_readbuf(d, capt_buf, cap_avail, &frames_in, &in_max);
        SndReturn_(r, "Failed readbuf: %s");

        // Copy capture channels to playback
        sndx_duplex_copy_capt_to_play(d->buf_capt, d->buf_play, r, &gain);

        err = sndx_duplex_writebuf(d, play_buf, r, &frames_out);
        SndReturn_(r, "Failed writebuf: %s");
    }

    // Report final timings
    sndx_duplex_timer_stop(d, frames_in, output);

    return 0;
}

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
