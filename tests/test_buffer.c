#include "buffer.h"

int main()
{
    int err;

    output_t* output;
    err = snd_output_stdio_attach(&output, stdout, 0);
    SndFatal(err, "Failed snd_output_stdio_attach: %s");

    const isize    num_formats = 4;
    const format_t formats[]   = {
        SND_PCM_FORMAT_FLOAT_LE,
        SND_PCM_FORMAT_S16_LE,
        SND_PCM_FORMAT_S24_3LE,
        SND_PCM_FORMAT_S32_LE,
    };

    const isize channels = 2;
    const isize frames   = 10;

    RANGE(i, num_formats)
    {
        sndx_buffer_t* b;
        err = sndx_buffer_open(&b, formats[i], channels, frames, output);

        char* samples = calloc(b->frames * b->channels * b->bytes, sizeof(char));

        sndx_buffer_map_dev_to_samples(b, samples);
        sndx_dump_buffer(b, output);

        free(samples);

        sndx_buffer_close(b, output);
    }

    snd_output_close(output);

    return 0;
}
