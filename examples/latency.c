#include "duplex.h"

int main()
{
    int err;

    output_t* output;
    err = snd_output_stdio_attach(&output, stdout, 0);
    SndFatal(err, "Failed snd_output_stdio_attach: %s");

    sndx_duplex_t* d;
    err = sndx_duplex_open(              //
        &d,                              //
        "hw:FC1,0", "hw:FC1_1,0",        //
        SND_PCM_FORMAT_S16_LE,           //
        48000, 256, 128,                 //
        SND_PCM_ACCESS_MMAP_INTERLEAVED, //
        output);
    SndFatal(err, "Failed sndx_duplex_open: %s");

    isize capt_size = d->buf_capt->frames * d->buf_capt->channels * d->buf_capt->bytes;
    char* capt_buf  = calloc(capt_size, sizeof(char));

    isize play_size = d->buf_play->frames * d->buf_play->channels * d->buf_play->bytes;
    char* play_buf  = calloc(play_size, sizeof(char));

    sndx_buffer_map_dev_to_samples(d->buf_play, play_buf);
    sndx_buffer_map_dev_to_samples(d->buf_capt, capt_buf);

    sndx_dump_duplex(d, output);

    err = sndx_duplex_close(d);
    SndFatal(err, "Failed sndx_duplex_close: %s");

    free(capt_buf);
    free(play_buf);

    snd_output_close(output);

    return 0;
}
