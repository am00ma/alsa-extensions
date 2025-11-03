#include "duplex.h"

int main()
{
    int err;

    output_t* output;
    err = snd_output_stdio_attach(&output, stdout, 0);
    SndFatal(err, "Failed snd_output_stdio_attach: %s");

    sndx_duplex_t* d;
    err = sndx_duplex_open(     //
        &d,                     //
        "hw:USB,0", "hw:USB,0", //
        SND_PCM_FORMAT_S24_3LE, //
        48000, 256, 128,        //
        SND_PCM_ACCESS_MMAP_INTERLEAVED);
    SndFatal(err, "Failed sndx_duplex_open: %s");

    sndx_dump_duplex(d, output);

    err = sndx_duplex_close(d);
    SndFatal(err, "Failed sndx_duplex_close: %s");

    return 0;
}
