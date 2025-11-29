#include "sndx/params.h"

int main()
{
    int err;

    output_t* output;
    err = snd_output_stdio_attach(&output, stdout, 0);
    SndFatal(err, "Failed snd_output_stdio_attach: %s");

    print_(sizeof(sndx_params_t));

    sndx_params_t p = {
        .channels    = 2,
        .format      = SND_PCM_FORMAT_S16,
        .access      = SND_PCM_ACCESS_MMAP_INTERLEAVED,
        .rate        = 48000,
        .periods     = 2,
        .period_size = 128,
    };

    sndx_dump_params(&p, output);

    snd_output_close(output);

    return 0;
}
