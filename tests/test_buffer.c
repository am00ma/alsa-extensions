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
    const isize offset   = 0;

    RANGE(i, num_formats)
    {
        sndx_buffer_t* b;
        err = sndx_buffer_open(&b, formats[i], channels, frames, output);

        char* samples = calloc(b->frames * b->channels * b->bytes, sizeof(char));

        sndx_buffer_map_dev_to_samples(b, samples);

        // Confirm dev areas are mapped
        sndx_dump_buffer(b, output);

        // Set some data
        RANGE(i, frames) { b->data[i] = (float)i / frames; }

        // Check areas
        sndx_dump_buffer_areas(b, offset, frames, output);

        // Move to device
        sndx_buffer_buf_to_dev(b, offset, frames);

        // Reset buf
        RANGE(i, frames) { b->data[i] = 0.0; }

        // Check areas
        sndx_dump_buffer_areas(b, offset, frames, output);

        // Move from device to buffer
        sndx_buffer_dev_to_buf(b, offset, frames);

        // Check areas
        sndx_dump_buffer_areas(b, offset, frames, output);

        free(samples);

        sndx_buffer_close(b);
    }

    snd_output_close(output);

    return 0;
}
