#include "buffer.h"
#include "pcm_lfloat.h"

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
    a_info("  from_dev_int32_idx   : %d", b->from_dev_idx_int32);
    a_info("  from_dev_float32_idx : %d", b->from_dev_idx_float32);
    a_info("  to_dev_float32_idx   : %d", b->to_dev_idx_float32);
    a_info("  to_dev_int32_idx     : %d", b->to_dev_idx_int32);

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

    a_log("  b->bufdata: ");
    RANGE(i, (isize)frames) { a_log("%2.2f | ", b->bufdata[i]); }
    a_log("\n");

    // NOTE: Not working
    if (b->bytes == 4)
    {
        a_log("  b->devdata: ");
        RANGE(i, (isize)frames) { a_log("%d | ", (i32) * (&b->devdata[i * b->bytes])); }
        a_log("\n");
    }
    else if (b->bytes == 2)
    {
        a_log("  b->devdata: ");
        RANGE(i, (isize)frames) { a_log("%d | ", (i16) * (&b->devdata[i * b->bytes])); }
        a_log("\n");
    }
}

int sndx_buffer_open(sndx_buffer_t** bufp, format_t format, u32 channels, uframes_t frames, output_t* output)
{
    int err;

    sndx_buffer_t* b;
    b = calloc(1, sizeof(*b));
    RetVal_(!b, -ENOMEM, "Failed calloc buffer_t* b");

    b->format   = format;
    b->bytes    = snd_pcm_format_width(format) / 8;
    b->channels = channels;
    b->frames   = frames;

    b->from_dev_idx_int32   = snd_pcm_linear_get_index(b->format, SND_PCM_FORMAT_S32);
    b->from_dev_idx_float32 = snd_pcm_lfloat_put_s32_index(SND_PCM_FORMAT_FLOAT);

    b->to_dev_idx_int32   = snd_pcm_linear_put_index(SND_PCM_FORMAT_S32, b->format);
    b->to_dev_idx_float32 = snd_pcm_lfloat_get_s32_index(SND_PCM_FORMAT_FLOAT);

    b->dev = calloc(channels, sizeof(area_t));
    err    = -(!b->dev);
    Goto_(err, __close, "Failed calloc area_t* b->dev");

    b->buf = calloc(channels, sizeof(area_t));
    err    = -(!b->buf);
    Goto_(err, __close, "Failed calloc area_t* b->buf");

    b->bufdata = calloc(channels * frames, sizeof(float));
    err        = -(!b->bufdata);
    Goto_(err, __close, "Failed calloc float* b->bufdata");

    b->devdata = calloc(channels * frames, b->bytes);
    err        = -(!b->devdata);
    Goto_(err, __close, "Failed calloc char* b->devdata");

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

__close:
    sndx_buffer_close(b);
    *bufp = nullptr;

    return err;
}

void sndx_buffer_mmap_dev_areas(sndx_buffer_t* b, const area_t* areas)
{
    RANGE(chn, b->channels)
    {
        b->dev[chn].addr  = areas[chn].addr;
        b->dev[chn].first = b->bytes * 8 * chn;
        b->dev[chn].step  = b->bytes * 8 * b->channels;
    }
}

void sndx_buffer_close(sndx_buffer_t* b)
{
    if (!b) return;

    Free(b->dev);
    Free(b->buf);
    Free(b->bufdata);
    Free(b->devdata);

    free(b);
    b = nullptr;
}

void sndx_buffer_dev_to_buf(sndx_buffer_t* b, uframes_t offset, uframes_t frames)
{
    snd_pcm_lfloat_convert_integer_float( //
        b->buf, offset,                   //
        b->dev, offset,                   //
        b->channels, frames,              //
        b->from_dev_idx_int32, b->from_dev_idx_float32);
}

void sndx_buffer_buf_to_dev(sndx_buffer_t* b, uframes_t offset, uframes_t frames)
{
    snd_pcm_lfloat_convert_float_integer( //
        b->dev, offset,                   //
        b->buf, offset,                   //
        b->channels, frames,              //
        b->to_dev_idx_int32, b->to_dev_idx_float32);
}
