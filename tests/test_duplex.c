#include "duplex.h"
#include <alsa/asoundlib.h>

int main()
{
    int err;

    output_t* output;
    err = snd_output_stdio_attach(&output, stdout, 0);
    SndFatal(err, "Failed snd_output_stdio_attach: %s");

    sndx_duplex_t* d;
    err = sndx_duplex_open(              //
        &d,                              //
        "hw:PCH,0", "hw:PCH,0",          //
        SND_PCM_FORMAT_S16_LE,           //
        48000, 128, 2,                   //
        SND_PCM_ACCESS_MMAP_INTERLEAVED, //
        output);
    SndFatal(err, "Failed sndx_duplex_open: %s");

    // Should be prepared
    sndx_dump_duplex_status(d, output);

    err = sndx_duplex_write_initial_silence(d);
    SndCheck(err, "Failed sndx_duplex_write_initial_silence: %s");

    err = snd_pcm_start(d->play);
    SndCheck(err, "Failed snd_pcm_start: %s");

    // Should be running?
    sndx_dump_duplex_status(d, output);

    // // Start prep for loop -> on error goto __error
    // uframes_t frames_out = 0;
    // uframes_t frames_in  = 0;
    // uframes_t in_max     = 0;
    // uframes_t out_max    = 0;
    //
    // // Read write loop
    // sframes_t r, cap_avail;
    // while (frames_in < 10)
    // {
    //     cap_avail = d->period_size;
    //     snd_pcm_wait(d->capt, 1000);
    // }

    err = sndx_duplex_close(d);
    SndFatal(err, "Failed sndx_duplex_close: %s");

    snd_output_close(output);

    return 0;
}
