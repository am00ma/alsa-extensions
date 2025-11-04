#include "buffer.h"
#include "types.h"

void sndx_dump_buffer(sndx_buffer_t* b, output_t* output)
{
    fmt_t f = b->format;
    a_info("  channels: %d", b->channels);
    a_info("  frames  : %ld", b->frames);
    a_info("  bytes   : %d", b->bytes);
    a_info("  format  : %s [ %s, %d, %s ]",               //
           snd_pcm_format_name(f),                        //
           snd_pcm_format_linear(f) ? "linear" : "float", //
           snd_pcm_format_width(f),                       //
           snd_pcm_format_little_endian(f) ? "le" : "be");

    RANGE(chn, b->channels)
    a_info("    dev[%ld]: %p, %3u, %3u", chn, b->dev[chn].addr, b->dev[chn].first, b->dev[chn].step);

    RANGE(chn, b->channels)
    a_info("    buf[%ld]: %p, %3u, %3u", chn, b->buf[chn].addr, b->buf[chn].first, b->buf[chn].step);
}

int sndx_buffer_open(sndx_buffer_t** bufp, fmt_t format, u32 channels, uframes_t frames, snd_output_t* output)
{
    sndx_buffer_t* b;
    b = calloc(1, sizeof(*b));
    RetVal_(!b, -ENOMEM, "Failed calloc buffer_t* b");

    b->format   = format;
    b->bytes    = snd_pcm_format_width(format) / 8;
    b->channels = channels;
    b->frames   = frames;

    b->dev = calloc(channels, sizeof(area_t));
    RetVal_(!b, -ENOMEM, "Failed calloc area_t b->dev");

    b->buf = calloc(channels, sizeof(area_t));
    RetVal_(!b, -ENOMEM, "Failed calloc area_t b->buf");

    b->data = calloc(channels * frames, sizeof(float));
    RetVal_(!b, -ENOMEM, "Failed calloc float b->data");

    RANGE(chn, b->channels)
    {
        b->buf[chn].addr  = b->data;
        b->buf[chn].first = (b->frames * b->channels * sizeof(float) * 8) * chn;
        b->buf[chn].step  = sizeof(float) * 8;
    }

    *bufp = b;

    return 0;
}

void sndx_buffer_close(sndx_buffer_t* b, output_t* output)
{
    free(b->dev);
    free(b->buf);
    free(b->data);
    free(b);

    a_info("Closed buffer");
}

void sndx_buffer_map_dev_to_samples(sndx_buffer_t* b, char* samples)
{
    RANGE(chn, b->channels)
    {
        b->dev[chn].addr  = samples;
        b->dev[chn].first = b->bytes * 8 * chn;
        b->dev[chn].step  = b->bytes * 8 * b->channels;
    }
}
