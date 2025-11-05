#include "playback.h"

int main()
{
    int err;

    output_t* output;
    err = snd_output_stdio_attach(&output, stdout, 0);
    SndFatal(err, "Failed snd_output_stdio_attach: %s");

    sndx_playback_t* d;
    err = sndx_playback_open(            //
        &d,                              //
        "hw:USB,0",                      //
        SND_PCM_FORMAT_S24_3LE,          //
        48000, 256, 128,                 //
        SND_PCM_ACCESS_MMAP_INTERLEAVED, //
        output);
    SndFatal(err, "Failed sndx_playback_open: %s");

    sndx_dump_playback(d, output);

    err = sndx_playback_close(d);
    SndFatal(err, "Failed sndx_playback_close: %s");

    snd_output_close(output);

    return 0;
}
