#include "duplex.h"

int main()
{
    int err;

    output_t* output;
    err = snd_output_stdio_attach(&output, stderr, 0);
    SndFatal(err, "Failed snd_output_stdio_attach: %s");

    sndx_duplex_t* d;
    err = sndx_duplex_open(              //
        &d,                              //
        "hw:FC1_1,0", "hw:FC1_1,0",      // play, capt device
        SND_PCM_FORMAT_S16_LE,           // format
        48000,                           // rate
        256, 128,                        // buffer_size, period_size
        SND_PCM_ACCESS_MMAP_INTERLEAVED, // access
        output);
    SndFatal(err, "Failed sndx_duplex_open: %s");

    char*     play_buf;
    char*     capt_buf;
    uframes_t loop_limit = d->rate * 120;

    err = sndx_duplex_start(d, &play_buf, &capt_buf, loop_limit);
    SndCheck_(err, "Failed sndx_duplex_start: %s");

    err = sndx_duplex_stop(d, play_buf, capt_buf);
    SndCheck_(err, "Failed sndx_duplex_stop: %s");

    err = sndx_duplex_close(d);
    SndCheck_(err, "Failed sndx_duplex_close: %s");

    snd_output_close(output);

    return 0;
}
