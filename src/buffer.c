#include "buffer.h"
#include "memops.h"
#include "types.h"

void sndx_dump_buffer(sndx_buffer_t* buffer, output_t* output)
{
    a_info("  frames      = %ld", buffer->frames);
    a_info("  channels    = %d", buffer->channels);
    a_info("  bytes       = %d", buffer->bytes);
    a_info("  interleaved = %d", buffer->interleaved);

    RANGE(chn, buffer->channels)
    {
        a_info("    chn %3ld : [ %p | %3d | %3d || %p | %3d ]",                                 //
               chn, buffer->areas[chn].addr, buffer->areas[chn].first, buffer->areas[chn].step, //
               buffer->addrs[chn], buffer->skips[chn]);

        a_info("    buf %3ld : [ %p | %3d | %3d || %p | %3d ]",                                             //
               chn, buffer->buf.areas[chn].addr, buffer->buf.areas[chn].first, buffer->buf.areas[chn].step, //
               buffer->buf.addrs[chn], buffer->buf.skips[chn]);
    }
}

int sndx_buffer_open(            //
    sndx_buffer_t** bufferp,     //
    u32             channels,    //
    format_t        format,      //
    uframes_t       maxframes,   //
    bool            interleaved, //
    output_t*       output)
{
    sndx_buffer_t* a;

    a = (sndx_buffer_t*)calloc(1, sizeof(*a));
    if (!a) return -ENOMEM;

    a->channels    = channels;
    a->bytes       = snd_pcm_format_width(format) / 8;
    a->interleaved = interleaved;
    a->frames      = maxframes;

    // Map to areas of hardware buffer

    a->areas = calloc(channels, sizeof(snd_pcm_channel_area_t));
    RetVal_(!a->areas, -ENOMEM, "Failed calloc: snd_pcm_channel_area_t* areas");

    a->addrs = calloc(channels, sizeof(char*));
    RetVal_(!a->addrs, -ENOMEM, "Failed calloc: char** addrs");

    a->skips = calloc(channels, sizeof(u32));
    RetVal_(!a->skips, -ENOMEM, "Failed calloc: u32* skips");

    // Map to areas of soft float buffer (sndx_buffer_format_t = SND_PCM_FORMAT_FLOAT_LE)

    a->buf.bytes = snd_pcm_format_width(sndx_buffer_format_t) / 8;

    a->buf.areas = calloc(channels, sizeof(snd_pcm_channel_area_t));
    RetVal_(!a->buf.areas, -ENOMEM, "Failed calloc: snd_pcm_channel_area_t* areas");

    a->buf.addrs = calloc(channels, sizeof(char*));
    RetVal_(!a->buf.addrs, -ENOMEM, "Failed calloc: char** addrs");

    a->buf.skips = calloc(channels, sizeof(u32));
    RetVal_(!a->buf.skips, -ENOMEM, "Failed calloc: u32* skips");

    a->buf.data = calloc(a->channels * a->frames, sizeof(float));
    RetVal_(!a->buf.data, -ENOMEM, "Failed calloc: float* data");

    RANGE(chn, a->channels)
    {
        snd_pcm_channel_area_t* area = &a->buf.areas[chn];

        area->addr = a->buf.data;
        if (interleaved)
        {
            area->first = (4 * 8) * chn;
            area->step  = 4 * 8 * a->channels;
        }
        else {
            area->first = (a->frames * 4 * 8) * chn;
            area->step  = 4 * 8;
        }

        a->buf.addrs[chn] = snd_pcm_channel_area_addr(area, 0);
        a->buf.skips[chn] = snd_pcm_channel_area_step(area);
    }

    *bufferp = a;
    return 0;
}

int sndx_buffer_samples_alloc(sndx_buffer_t* buf, char** samplesp, output_t* output)
{
    char* samples = calloc(buf->channels * buf->frames * buf->bytes, sizeof(char));
    RetVal_(!samples, -ENOMEM, "Failed calloc: char* samples");

    RANGE(chn, buf->channels)
    {
        snd_pcm_channel_area_t* area = &buf->areas[chn];

        area->addr = samples;
        if (buf->interleaved)
        {
            area->first = (buf->buf.bytes * 8) * chn;
            area->step  = buf->buf.bytes * 8 * buf->channels;
        }
        else {
            area->first = (buf->frames * buf->buf.bytes * 8) * chn;
            area->step  = buf->buf.bytes * 8;
        }

        buf->addrs[chn] = snd_pcm_channel_area_addr(area, 0);
        buf->skips[chn] = snd_pcm_channel_area_step(area);
    }

    *samplesp = samples;
    return 0;
}

int sndx_buffer_samples_free(char* samples)
{
    free(samples);
    return 0;
}

int sndx_buffer_to_area_from_buf(sndx_buffer_t* b, uframes_t offset, uframes_t frames, format_t format)
{
    RANGE(chn, b->channels)
    {
        const snd_pcm_channel_area_t* src_area = &b->buf.areas[chn];
        const snd_pcm_channel_area_t* dst_area = &b->areas[chn];

        float* src      = snd_pcm_channel_area_addr(src_area, offset);
        char*  dst      = snd_pcm_channel_area_addr(dst_area, offset);
        u32    src_skip = snd_pcm_channel_area_step(src_area) / 4;
        u32    dst_skip = snd_pcm_channel_area_step(dst_area);
        p_info("src: %p, %d | dst: %p, %d ", (void*)src, src_skip, dst, dst_skip);
        switch (format)
        {
        case SND_PCM_FORMAT_S16_LE: sample_move_d16_sS(dst, src, dst_skip, src_skip, frames); break;
        case SND_PCM_FORMAT_S24_3LE: sample_move_d24_sS(dst, src, dst_skip, src_skip, frames); break;
        case SND_PCM_FORMAT_S32_LE: sample_move_d32_sS(dst, src, dst_skip, src_skip, frames); break;
        default: AssertMsg(true, "Format not supported: %s", snd_pcm_format_name(format)); break;
        }
    }
    return 0;
}

int sndx_buffer_to_buf_from_area(sndx_buffer_t* b, uframes_t offset, uframes_t frames, format_t format)
{
    RANGE(chn, b->channels)
    {
        const snd_pcm_channel_area_t* src_area = &b->areas[chn];
        const snd_pcm_channel_area_t* dst_area = &b->buf.areas[chn];

        char*  src      = snd_pcm_channel_area_addr(src_area, offset);
        float* dst      = snd_pcm_channel_area_addr(dst_area, offset);
        u32    src_skip = snd_pcm_channel_area_step(src_area);
        u32    dst_skip = snd_pcm_channel_area_step(dst_area) / 4;
        p_info("src: %p, %d | dst: %p, %d ", src, src_skip, (void*)dst, dst_skip);
        switch (format)
        {
        case SND_PCM_FORMAT_S16_LE: sample_move_dS_s16(dst, src, dst_skip, src_skip, frames); break;
        case SND_PCM_FORMAT_S24_3LE: sample_move_dS_s24(dst, src, dst_skip, src_skip, frames); break;
        case SND_PCM_FORMAT_S32_LE: sample_move_dS_s32(dst, src, dst_skip, src_skip, frames); break;
        default: AssertMsg(true, "Format not supported: %s", snd_pcm_format_name(format)); break;
        }
    }
    return 0;
}

int sndx_buffer_close(sndx_buffer_t* a)
{
    free(a->areas);
    free(a->addrs);
    free(a->skips);
    free(a->buf.areas);
    free(a->buf.addrs);
    free(a->buf.skips);
    free(a->buf.data);
    free(a);
    return 0;
}
