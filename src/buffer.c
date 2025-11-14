#include "buffer.h"
#include "memops.h"

void sndx_dump_buffer(sndx_buffer_t* b, output_t* output)
{
    a_info("Buffer status:");

    format_t f = b->format;
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

void sndx_dump_buffer_areas(sndx_buffer_t* b, uframes_t offset, uframes_t frames, output_t* output)
{
    a_info("  offset, frames, channels  : %ld, %ld, %d", offset, frames, b->channels);

    RANGE(chn, b->channels)
    {
        const area_t* a_dev = &b->dev[chn];
        const area_t* a_buf = &b->buf[chn];

        char*  dev  = snd_pcm_channel_area_addr(a_dev, offset);
        float* buf  = snd_pcm_channel_area_addr(a_buf, offset);
        int    step = snd_pcm_channel_area_step(a_dev);

        a_info("  chn %ld: dev: %p, buf: %p, step: %d", chn, dev, (void*)buf, step);
    }

    // Data
    a_log("  b->bufdata: ");
    RANGE(i, (isize)frames) { a_log("%2.2f | ", b->bufdata[i]); }
    a_log("\n");
}

int sndx_buffer_open(sndx_buffer_t** bufp, format_t format, u32 channels, uframes_t frames, snd_output_t* output)
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

    b->bufdata = calloc(channels * frames, sizeof(float));
    RetVal_(!b, -ENOMEM, "Failed calloc float b->bufdata");

    b->devdata = calloc(channels * frames, b->bytes);
    RetVal_(!b, -ENOMEM, "Failed calloc float b->devdata");

    RANGE(chn, b->channels)
    {
        b->buf[chn].addr  = b->bufdata;
        b->buf[chn].first = (b->frames * chn * sizeof(float) * 8);
        b->buf[chn].step  = sizeof(float) * 8;
    }

    RANGE(chn, b->channels)
    {
        b->dev[chn].addr  = b->devdata;
        b->dev[chn].first = b->bytes * 8 * chn;
        b->dev[chn].step  = b->bytes * 8 * b->channels;
    }

    *bufp = b;

    return 0;
}

void sndx_buffer_close(sndx_buffer_t* b)
{
    if (!b) return;

    if (b->dev)
    {
        free(b->dev);
        b->dev = nullptr;
    }
    if (b->buf)
    {
        free(b->buf);
        b->buf = nullptr;
    }
    if (b->bufdata)
    {
        free(b->bufdata);
        b->bufdata = nullptr;
    }
    if (b->devdata)
    {
        free(b->devdata);
        b->devdata = nullptr;
    }

    free(b);
    b = nullptr;
}

void sndx_buffer_buf_to_dev(sndx_buffer_t* b, uframes_t offset, uframes_t frames)
{
    RANGE(chn, b->channels)
    {
        const area_t* a_dev = &b->dev[chn];
        const area_t* a_buf = &b->buf[chn];

        float* buf  = snd_pcm_channel_area_addr(a_buf, offset);
        char*  dev  = snd_pcm_channel_area_addr(a_dev, offset);
        int    step = snd_pcm_channel_area_step(a_dev);

        sample_move_d16_sS(dev, buf, step, 4, frames);
    }
}

void sndx_buffer_dev_to_buf(sndx_buffer_t* b, uframes_t offset, uframes_t frames)
{
    RANGE(chn, b->channels)
    {
        const area_t* a_dev = &b->dev[chn];
        const area_t* a_buf = &b->buf[chn];

        char*  dev  = snd_pcm_channel_area_addr(a_dev, offset);
        float* buf  = snd_pcm_channel_area_addr(a_buf, offset);
        int    step = snd_pcm_channel_area_step(a_dev);

        sample_move_dS_s16(buf, dev, 4, step, frames);
    }
}
