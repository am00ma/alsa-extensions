#include "buffer.h"

int main()
{
    int err;

    output_t* output;
    err = snd_output_stdio_attach(&output, stdout, 0);
    SndFatal(err, "Failed snd_output_stdio_attach: %s");

    sndx_buffer_t* b_float_non;
    err = sndx_buffer_open(   //
        &b_float_non,         //
        2,                    // channels
        SND_PCM_FORMAT_FLOAT, // format
        10,                   // frames
        false,                // noninterleaved
        output);
    SndFatal(err, "Failed sndx_buffer_open: %s");

    sndx_dump_buffer(b_float_non, output);

    err = sndx_buffer_close(b_float_non);
    SndFatal(err, "Failed sndx_buffer_close: %s");

    sndx_buffer_t* b_float_int;
    err = sndx_buffer_open(   //
        &b_float_int,         //
        2,                    // channels
        SND_PCM_FORMAT_FLOAT, // format
        10,                   // frames
        true,                 // interleaved
        output);
    SndFatal(err, "Failed sndx_buffer_open: %s");

    sndx_dump_buffer(b_float_int, output);

    err = sndx_buffer_close(b_float_int);
    SndFatal(err, "Failed sndx_buffer_close: %s");

    return 0;
}
